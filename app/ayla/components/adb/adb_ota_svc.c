/*
 * Copyright 2023-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <cmsis_os.h>

#include <scm_mcuboot/mcuboot_agent.h>
#include <wise_system.h>

#include <ada/libada.h>
#include <ada/client.h>
#include <ayla/log.h>
#include <adb/adb.h>
#include <adb/adb_ota_svc.h>
#include <al/al_bt.h>
#include <ayla/utypes.h>

#ifdef AYLA_BLUETOOTH_SUPPORT
#ifdef CONFIG_SCM_MCUBOOT_UPDATE_AGENT

#define ADB_OTA_INVALID_CONN 0xffff
#define ADB_OTA_STACK_SIZE 8192
#define ADB_OTA_REBOOT_DELAY_MS 1000

static uint16_t app_version;

enum adb_ota_cmd
{
    ADB_OTA_CMD_START = 1,
    ADB_OTA_CMD_GET_VER = 2,
    ADB_OTA_CMD_GET_NET = 3,
};

enum adb_ota_state
{
    ADB_OTA_STATE_IDLE    = 0,
    ADB_OTA_STATE_RUNNING = 1,
    ADB_OTA_STATE_SUCCESS = 2,
    ADB_OTA_STATE_FAILED  = 3,
};

enum adb_ota_result
{
    ADB_OTA_RESULT_NONE         = 0,
    ADB_OTA_RESULT_OK           = 1,
    ADB_OTA_RESULT_BUSY         = 2,
    ADB_OTA_RESULT_INVALID_URL  = 3,
    ADB_OTA_RESULT_START_FAILED = 4,
    ADB_OTA_RESULT_OTA_FAILED   = 5,
};

struct adb_ota_cmd_msg
{
    u8 cmd;
    u8 reserved;
    u16 url_len;
} PACKED;

struct adb_ota_status_msg
{
    u8 state;
    u8 result;
    u8 progress;
    u8 reserved;
} PACKED;

#define ADB_OTA_URL_MAX_LEN (ADB_GATT_ATT_MAX_LEN - sizeof(struct adb_ota_cmd_msg) - 1)

struct adb_ota_ctx
{
    osMutexId_t lock;
    osThreadId_t worker;
    u16 conn;
    u8 busy;
    u8 next_progress;
    struct adb_ota_status_msg status;
    char url[ADB_OTA_URL_MAX_LEN + 1];
};

static enum adb_att_err adb_ota_write_cb(u16 conn, const struct adb_attr * attr, u8 * buf, u16 length);
static enum adb_att_err adb_ota_read_cb(u16 conn, const struct adb_attr * attr, u8 * buf, u16 * length);
static void adb_ota_thread(void * arg);
static void adb_ota_progress_cb(u8 progress, void * arg);
static void adb_ota_conn_event_cb(enum adb_conn_event event, u16 conn, void * arg);

/* 8C09A001-3570-40DE-9C25-31C66F9161A2 */
static const AL_BT_UUID128(ota_cmd_uuid, 0xa2, 0x61, 0x91, 0x6f, 0xc6, 0x31, 0x25, 0x9c, 0xde, 0x40, 0x70, 0x35, 0x01, 0xa0, 0x09,
                           0x8c);
/* 8C09A002-3570-40DE-9C25-31C66F9161A2 */
static const AL_BT_UUID128(ota_status_uuid, 0xa2, 0x61, 0x91, 0x6f, 0xc6, 0x31, 0x25, 0x9c, 0xde, 0x40, 0x70, 0x35, 0x02, 0xa0,
                           0x09, 0x8c);

/* 8C09A000-3570-40DE-9C25-31C66F9161A2 */
static const AL_BT_UUID128(ota_svc_uuid, 0xa2, 0x61, 0x91, 0x6f, 0xc6, 0x31, 0x25, 0x9c, 0xde, 0x40, 0x70, 0x35, 0x00, 0xa0, 0x09,
                           0x8c);

static struct adb_chr_info ota_cmd_chr;
static struct adb_chr_info ota_status_chr;
static struct adb_service_info ota_svc = {
    .is_primary = 1,
};

static const struct adb_attr ota_svc_table[] = {
    ADB_SERVICE("ota_svc", &ota_svc_uuid, &ota_svc),
    ADB_CHR("ota_cmd", &ota_cmd_uuid, AL_BT_AF_WRITE | AL_BT_AF_WRITE_ENC, NULL, adb_ota_write_cb, &ota_cmd_chr),
    ADB_CHR("ota_status", &ota_status_uuid, AL_BT_AF_READ | AL_BT_AF_READ_ENC | AL_BT_AF_NOTIFY, adb_ota_read_cb, NULL,
            &ota_status_chr),
    ADB_SERVICE_END()
};

static struct adb_ota_ctx adb_ota_ctx = {
	.conn = ADB_OTA_INVALID_CONN,
	.status = {
		.state = ADB_OTA_STATE_IDLE,
		.result = ADB_OTA_RESULT_NONE,
		.progress = 0,
	},
};

static const osThreadAttr_t adb_ota_thread_attr = {
    .name       = "adb_ota",
    .stack_size = ADB_OTA_STACK_SIZE,
    .priority   = osPriorityNormal,
};

static void adb_ota_notify_conn(u16 conn, const struct adb_ota_status_msg * status)
{
    if (conn == ADB_OTA_INVALID_CONN)
    {
        return;
    }
    al_bt_notify_conn(conn, &ota_svc_table[2], (u8 *) status, sizeof(*status));
}

static void adb_ota_set_status(u16 conn, u8 state, u8 result, u8 progress)
{
    osMutexAcquire(adb_ota_ctx.lock, osWaitForever);
    adb_ota_ctx.conn            = conn;
    adb_ota_ctx.status.state    = state;
    adb_ota_ctx.status.result   = result;
    adb_ota_ctx.status.progress = progress;
    adb_ota_ctx.status.reserved = 0;
    osMutexRelease(adb_ota_ctx.lock);
}

static void adb_ota_status_snapshot(struct adb_ota_status_msg * status, u16 * conn)
{
    osMutexAcquire(adb_ota_ctx.lock, osWaitForever);
    *status = adb_ota_ctx.status;
    *conn   = adb_ota_ctx.conn;
    osMutexRelease(adb_ota_ctx.lock);
}

static void adb_ota_progress_cb(u8 progress, void * arg)
{
    struct adb_ota_status_msg status;
    u16 conn;

    (void) arg;

    osMutexAcquire(adb_ota_ctx.lock, osWaitForever);
    adb_ota_ctx.status.state    = ADB_OTA_STATE_RUNNING;
    adb_ota_ctx.status.result   = ADB_OTA_RESULT_NONE;
    adb_ota_ctx.status.progress = progress;

    while (progress >= adb_ota_ctx.next_progress && adb_ota_ctx.next_progress <= 100)
    {
        status          = adb_ota_ctx.status;
        status.progress = adb_ota_ctx.next_progress;
        conn            = adb_ota_ctx.conn;
        adb_ota_ctx.next_progress += 10;
        osMutexRelease(adb_ota_ctx.lock);
        adb_ota_notify_conn(conn, &status);
        osMutexAcquire(adb_ota_ctx.lock, osWaitForever);
    }
    osMutexRelease(adb_ota_ctx.lock);
}

static void adb_ota_thread(void * arg)
{
    struct mcuboot_agent_params params = {
        .progress_cb = adb_ota_progress_cb,
        .auto_reboot = 0,
    };
    struct adb_ota_status_msg status;
    u16 conn;
    char url[sizeof(adb_ota_ctx.url)];
    int rc;

    (void) arg;

    osMutexAcquire(adb_ota_ctx.lock, osWaitForever);
    memcpy(url, adb_ota_ctx.url, sizeof(url));
    conn = adb_ota_ctx.conn;
    osMutexRelease(adb_ota_ctx.lock);

    rc = mcuboot_agent_run(url, &params);

    osMutexAcquire(adb_ota_ctx.lock, osWaitForever);
    adb_ota_ctx.worker        = NULL;
    adb_ota_ctx.busy          = 0;
    adb_ota_ctx.status.state  = (rc == 0) ? ADB_OTA_STATE_SUCCESS : ADB_OTA_STATE_FAILED;
    adb_ota_ctx.status.result = (rc == 0) ? ADB_OTA_RESULT_OK : ADB_OTA_RESULT_OTA_FAILED;
    if (rc == 0)
    {
        adb_ota_ctx.status.progress = 100;
    }
    status = adb_ota_ctx.status;
    conn   = adb_ota_ctx.conn;
    osMutexRelease(adb_ota_ctx.lock);

    adb_ota_notify_conn(conn, &status);

    if (rc == 0)
    {
        osDelay(ADB_OTA_REBOOT_DELAY_MS);
        wise_restart();
    }

    osThreadExit();
}

static enum adb_att_err adb_ota_read_cb(u16 conn, const struct adb_attr * attr, u8 * buf, u16 * length)
{
    struct adb_ota_status_msg status;

    (void) conn;
    (void) attr;

    if (*length < sizeof(status))
    {
        return ADB_ATT_INSUFFICIENT_RES;
    }

    adb_ota_status_snapshot(&status, &conn);
    memcpy(buf, &status, sizeof(status));
    *length = sizeof(status);
    return ADB_ATT_SUCCESS;
}

#if 0
static uint16_t wise_sdk_version;
static int parse_wise_sdk_version(const char *str) {
    const char *prefix = "WISE-SDK-";
    const char *ver = strstr(str, prefix);
    if (!ver) return -1;
    ver += strlen(prefix);
    int major, minor, patch;
    if (sscanf(ver, "%d.%d.%d", &major, &minor, &patch) != 3) {
        return -1;
    }
    return major * 100 + minor * 10 + patch;
}
#endif
extern uint8_t demo_get_ip_state();
extern int demo_log_external_connectivity();
static enum adb_att_err adb_ota_write_cb(u16 conn, const struct adb_attr * attr, u8 * buf, u16 length)
{
    const struct adb_ota_cmd_msg * msg = (const struct adb_ota_cmd_msg *) buf;
    struct adb_ota_status_msg status;
    osThreadId_t worker;

    printf("Recv ble buf: %u-%u-%u-%u; len:%d\n", *buf, *(buf + 1),*(buf + 2), *(buf + 3), length);
    (void) attr;

    /* Handle Other cmds */
    if (msg->cmd == ADB_OTA_CMD_GET_VER) {
        osMutexAcquire(adb_ota_ctx.lock, osWaitForever);
        status          = adb_ota_ctx.status;
        *(u16 *)(&(status.state))    = app_version;
        status.reserved = ADB_OTA_CMD_GET_VER - 1;
        osMutexRelease(adb_ota_ctx.lock);
        adb_ota_notify_conn(conn, &status);
        return ADB_ATT_SUCCESS;
    }

    if (msg->cmd == ADB_OTA_CMD_GET_NET) {
        osMutexAcquire(adb_ota_ctx.lock, osWaitForever);
        status          = adb_ota_ctx.status;
        status.state    = demo_get_ip_state();
        status.result   = demo_log_external_connectivity();
        status.reserved = ADB_OTA_CMD_GET_NET - 1;
        osMutexRelease(adb_ota_ctx.lock);
        adb_ota_notify_conn(conn, &status);
        return ADB_ATT_SUCCESS;
    }

    if (length < sizeof(*msg))
    {
        return ADB_ATT_INVALID_ATTR_VALUE_LEN;
    }
    if (msg->cmd != ADB_OTA_CMD_START)
    {
        return ADB_ATT_REQ_NOT_SUPPORTED;
    }
    if (msg->url_len == 0 || msg->url_len > ADB_OTA_URL_MAX_LEN)
    {
        return ADB_ATT_INVALID_ATTR_VALUE_LEN;
    }
    if (length != sizeof(*msg) + msg->url_len)
    {
        return ADB_ATT_INVALID_ATTR_VALUE_LEN;
    }

    printf("Begin to process1!!!\n");
    osMutexAcquire(adb_ota_ctx.lock, osWaitForever);
    if (adb_ota_ctx.busy)
    {
        status        = adb_ota_ctx.status;
        status.result = ADB_OTA_RESULT_BUSY;
        osMutexRelease(adb_ota_ctx.lock);
        adb_ota_notify_conn(conn, &status);
        return ADB_ATT_SUCCESS;
    }
    osMutexRelease(adb_ota_ctx.lock);

    if (msg->url_len < 7 || memcmp(buf + sizeof(*msg), "http://", 7) != 0)
    {
        adb_ota_set_status(conn, ADB_OTA_STATE_FAILED, ADB_OTA_RESULT_INVALID_URL, 0);
        adb_ota_status_snapshot(&status, &conn);
        adb_ota_notify_conn(conn, &status);
        return ADB_ATT_SUCCESS;
    }

    osMutexAcquire(adb_ota_ctx.lock, osWaitForever);
    memcpy(adb_ota_ctx.url, buf + sizeof(*msg), msg->url_len);
    adb_ota_ctx.url[msg->url_len] = '\0';
    adb_ota_ctx.conn              = conn;
    adb_ota_ctx.busy              = 1;
    adb_ota_ctx.next_progress     = 10;
    adb_ota_ctx.status.state      = ADB_OTA_STATE_RUNNING;
    adb_ota_ctx.status.result     = ADB_OTA_RESULT_NONE;
    adb_ota_ctx.status.progress   = 0;
    adb_ota_ctx.status.reserved   = 0;
    status                        = adb_ota_ctx.status;
    osMutexRelease(adb_ota_ctx.lock);

    adb_ota_notify_conn(conn, &status);

    worker = osThreadNew(adb_ota_thread, NULL, &adb_ota_thread_attr);
    if (!worker)
    {
        osMutexAcquire(adb_ota_ctx.lock, osWaitForever);
        adb_ota_ctx.busy            = 0;
        adb_ota_ctx.conn            = conn;
        adb_ota_ctx.status.state    = ADB_OTA_STATE_FAILED;
        adb_ota_ctx.status.result   = ADB_OTA_RESULT_START_FAILED;
        adb_ota_ctx.status.progress = 0;
        status                      = adb_ota_ctx.status;
        osMutexRelease(adb_ota_ctx.lock);
        adb_ota_notify_conn(conn, &status);
        return ADB_ATT_SUCCESS;
    }

    osMutexAcquire(adb_ota_ctx.lock, osWaitForever);
    adb_ota_ctx.worker = worker;
    osMutexRelease(adb_ota_ctx.lock);

    return ADB_ATT_SUCCESS;
}

static void adb_ota_conn_event_cb(enum adb_conn_event event, u16 conn, void * arg)
{
    struct adb_ota_status_msg status;

    (void) arg;

    if (event == ADB_CONN_UP)
    {
        /* may not be available at this time because you may not have subscribed yet */
        osMutexAcquire(adb_ota_ctx.lock, osWaitForever);
        status = adb_ota_ctx.status;
        *(u16 *)(&(status.state)) = app_version;
        status.reserved = 2;
        osMutexRelease(adb_ota_ctx.lock);
        adb_ota_notify_conn(conn, &status);
    }
    else if (event == ADB_CONN_DOWN)
    {
        osMutexAcquire(adb_ota_ctx.lock, osWaitForever);
        if (adb_ota_ctx.conn == conn)
        {
            adb_ota_ctx.conn = ADB_OTA_INVALID_CONN;
        }
        osMutexRelease(adb_ota_ctx.lock);
    }
}

const void * adb_ota_svc_get_uuid(void)
{
    return &ota_svc_uuid;
}

/* APP developer need to implement this func */
extern int demo_get_app_version();
int adb_ota_svc_register(const struct adb_attr ** service)
{
    if (!adb_ota_ctx.lock)
    {
        adb_ota_ctx.lock = osMutexNew(NULL);
        if (!adb_ota_ctx.lock)
        {
            return -1;
        }
    }

    if (service)
    {
        *service = ota_svc_table;
    }

    /* for BLE_GAP_EVENT_CONNECT & DISCONNECT CB */
    adb_conn_event_register(adb_ota_conn_event_cb, NULL);
    app_version = demo_get_app_version();
    adb_log(LOG_INFO "%s app_version:%u.%u.%u\n", __func__, app_version/100, (app_version % 100)/10, (app_version % 10));
    return al_bt_register_service(ota_svc_table);
}

#endif /* CONFIG_SCM_MCUBOOT_UPDATE_AGENT */
#endif /* AYLA_BLUETOOTH_SUPPORT */