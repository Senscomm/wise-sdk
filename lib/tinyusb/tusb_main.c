/*
 * Copyright 2021-2024 Senscomm Semiconductor Co., Ltd.	All rights reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <mem.h>
#include <hal/kmem.h>
#include <hal/kernel.h>
#include <hal/timer.h>

#include <FreeRTOS/FreeRTOS.h>
#include <cli.h>

#include "cmsis_os.h"
#include "tusb.h"
#include "tusb_main.h"
#include "scdc.h"

#include "device/usbd.h"
#include "device/usbd_pvt.h"

typedef struct
{
  struct
  {
    bool         detached;
    void		 *ctx;
    dfu_cb_prog  prog_cb;
    dfu_cb_done  done_cb;
  } dfu;
  struct
  {
    bool         loopback;
  } vendor;
  osThreadId_t tid;
} tusb_ctx;

static tusb_ctx _tusb_ctx;

/*
 * TinyUSB device task
 * This top level thread process all usb events and invoke callbacks.
 */

#ifdef CONFIG_CMDLINE
extern int os_system(const char * command);
#endif
static void tusb_main(void *param)
{
  (void) param;

  memset(&_tusb_ctx.vendor, 0, sizeof(_tusb_ctx.vendor));
  _tusb_ctx.dfu.detached = false;
  /* XXX: ctx, prog_cb, and done_cb should have already been set. */

  /* This should be called after scheduler/kernel is started.
   * Otherwise it could cause kernel issue since USB IRQ handler does use RTOS queue API.
   */
#ifdef CONFIG_TEST_PERIPHERAL

extern void i2c_init(void);
extern void i2c_write(unsigned int addr, unsigned int data);
extern unsigned int i2c_read(unsigned int addr);

  i2c_init();
  do
  {
    udelay(100);
    i2c_write(0x13, 0x82);
  }
  while (i2c_read(0x13) != 0x82);

#endif

#if defined(CONFIG_TUSB_DEBUG_STDOUT)
  os_system("dmesg start");
#endif

#if defined(CONFIG_SUPPORT_SCDC)
  scdc_init();
#endif
  tusb_init();

  /* RTOS forever loop */
  while (1)
  {
    /* tinyusb device task */
    tud_task();
  }
}

void start_tusb(void)
{
  osThreadAttr_t attr =
  {
    .name 		= "tusb",
    .stack_size = CONFIG_DEFAULT_STACK_SIZE,
    .priority 	= osPriorityNormal,
  };

  _tusb_ctx.tid = osThreadNew(tusb_main, NULL, &attr);
  if (_tusb_ctx.tid == NULL)
	printf("%s: failed to create tusb thread\n", __func__);
}

/* APIs */

#if CFG_TUD_DFU

bool tusb_dfu_detached(void)
{
  return _tusb_ctx.dfu.detached;
}

void tusb_dfu_start(void *ctx, dfu_cb_prog pcb, dfu_cb_done dcb)
{
  _tusb_ctx.dfu.ctx       = ctx;
  _tusb_ctx.dfu.prog_cb   = pcb;
  _tusb_ctx.dfu.done_cb   = dcb;
}

#endif

/* Callbacks */

#if CFG_TUD_DFU_RUNTIME

/* DFU runtime */

void tud_dfu_runtime_reboot_to_dfu_cb(void)
{
  _tusb_ctx.dfu.detached = true;
}

#endif

#if CFG_TUD_DFU

/*
 * Note: alt is used as the partition number, in order to support multiple partitions like FLASH, EEPROM, etc. */

/* Invoked right before tud_dfu_download_cb() (state=DFU_DNBUSY) or tud_dfu_manifest_cb() (state=DFU_MANIFEST)
 * Application return timeout in milliseconds (bwPollTimeout) for the next download/manifest operation.
 * During this period, USB host won't try to communicate with us.
 */

uint32_t tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state)
{
  if (state == DFU_DNBUSY)
  {
    return 10;
  }
  return 0;
}

/* Invoked when received DFU_DNLOAD (wLength>0) following by DFU_GETSTATUS (state=DFU_DNBUSY) requests
 * This callback could be returned before flashing op is complete (async).
 * Once finished flashing, application must call tud_dfu_finish_flashing()
 */
void tud_dfu_download_cb(uint8_t alt, uint16_t block_num, uint8_t const* data, uint16_t length)
{
  int ret;
  (void) alt;
  (void) block_num;

  TU_LOG2("\r\nReceived Alt %u BlockNum %u of length %u\r\n", alt, block_num, length);
  if (_tusb_ctx.dfu.prog_cb)
  {
    if ((ret = _tusb_ctx.dfu.prog_cb(_tusb_ctx.dfu.ctx, data, length)) != (int)length)
	{
	  tud_dfu_finish_flashing(DFU_STATUS_ERR_WRITE);
	  TU_LOG1("Error occurred from prog_cb: %d.", ret);
	  return;
    }
  }

  /* flashing op for download complete without error */
  tud_dfu_finish_flashing(DFU_STATUS_OK);
}

/* Invoked when download process is complete, received DFU_DNLOAD (wLength=0) following by DFU_GETSTATUS (state=Manifest)
 * Application can do checksum, or actual flashing if buffered entire image previously.
 * Once finished flashing, application must call tud_dfu_finish_flashing()
 */
void tud_dfu_manifest_cb(uint8_t alt)
{
  (void) alt;
  TU_LOG2("Download completed, enter manifestation\r\n");

  /* flashing op for manifest is complete without error
   * Application can perform checksum, should it fail, use appropriate status such as errVERIFY.
   */

  if (_tusb_ctx.dfu.done_cb)
  {
    _tusb_ctx.dfu.done_cb(_tusb_ctx.dfu.ctx);
  }

  tud_dfu_finish_flashing(DFU_STATUS_OK);
}

/* Invoked when the Host has terminated a download or upload transfer */
void tud_dfu_abort_cb(uint8_t alt)
{
  (void) alt;
  TU_LOG2("Host aborted transfer\r\n");

  _tusb_ctx.dfu.detached = false;
}

/* Invoked when a DFU_DETACH request is received */
void tud_dfu_detach_cb(void)
{
  TU_LOG2("Host detach, we should probably reboot\r\n");
}

#endif

#if CFG_TUD_VENDOR_TEST

#ifdef CONFIG_SUPPORT_SCDC
#error "CONFIG_SUPPORT_SCDC cannot be defined."
#endif

#if CONFIG_TUSB_BULK_OUT_DMA_CP_FIFO
#error "Disable TUSB_BULK_OUT_DMA_CP_FIFO"
#endif

#if CONFIG_TUSB_BULK_IN_DMA_CP_EPINBUF
#error "Disable TUSB_BULK_IN_DMA_CP_EPINBUF"
#endif

void tud_vendor_rx_cb(uint8_t itf, uint8_t ep_addr, uint16_t n)
{
  uint32_t num;
  uint8_t *buf;

  buf = kmalloc(n);
  if (!buf)
  {
    TU_LOG1("[%s, %d] Cannot allocate %d bytes.\n",
			__func__, __LINE__, n);
    return;
  }
  num = tud_vendor_n_read(itf, ep_addr, buf, n);
  TU_ASSERT(num == n, );
  if (_tusb_ctx.vendor.loopback)
  {
    tud_vendor_n_write(itf, buf, num);
  }
  else
  {
#if CONFIG_CMD_MEM
    hexdump(buf, num);
#endif
  }

  kfree(buf);
}

#define TUD_VENDOR_CTRL_TEST_SET	0x50
#define TUD_VENDOR_CTRL_TEST_GET	0x51

static uint8_t *loopback_buf;

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage,
		tusb_control_request_t const * request)
{
  // Handle class request only
  TU_VERIFY(request->bmRequestType_bit.type == TUSB_REQ_TYPE_VENDOR);

  TU_LOG2("%s\n", request->bRequest == TUD_VENDOR_CTRL_TEST_SET ?
		  "CTRL_TEST_SET" : "CTRL_TEST_GET");
  TU_LOG2("%s\n", stage == CONTROL_STAGE_SETUP ?
		  "CONTROL_STAGE_SETUP" : "CONTROL_STAGE_ACK");

  switch (request->bRequest)
  {
	case TUD_VENDOR_CTRL_TEST_SET:
	  if (stage == CONTROL_STAGE_SETUP)
	  {
		if (!request->wLength)
		{
		  break;
		}
        loopback_buf = kmalloc(request->wLength);
        if (!loopback_buf)
        {
          TU_LOG2("[%s, %d] Cannot allocate %d bytes.\n",
				  __func__, __LINE__, request->wLength);
          return false;
        }
		return tud_control_xfer(rhport, request, loopback_buf, request->wLength);
	  }
	  break;
	case TUD_VENDOR_CTRL_TEST_GET:
	  if (stage == CONTROL_STAGE_SETUP)
	  {
		return tud_control_xfer(rhport, request, loopback_buf, request->wLength);
	  }
	  else if (stage == CONTROL_STAGE_ACK)
	  {
	    kfree(loopback_buf);
		break;
	  }
	  break;
	default:
	  return false;
  }

  return true;
}

#endif

#ifdef CONFIG_CMD_USB
/**
 * USB CLI commands
 */
#include <cli.h>

#include <stdio.h>
#include <stdlib.h>

static int do_usb_connect(int argc, char *argv[])
{
  tud_connect();

  return CMD_RET_SUCCESS;
}

static int do_usb_disconnect(int argc, char *argv[])
{
#ifdef CONFIG_WLAN
  /* Let wlan interface(s) down first. */
  extern void wlan_down(void);
  wlan_down();
#endif
  tud_disconnect();

  return CMD_RET_SUCCESS;
}

#if CFG_TUD_VENDOR_TEST

static int do_usb_loopback(int argc, char *argv[])
{
  uint32_t val;

  if (argc < 2)
  {
	return CMD_RET_USAGE;
  }

  val = strtoul(argv[1], NULL, 0);

  if (val != 0 && val != 1)
  {
    return CMD_RET_USAGE;
  }

  _tusb_ctx.vendor.loopback = val == 1 ? true : false;

  return CMD_RET_SUCCESS;
}

#endif

#if CFG_TUD_VENDOR_DUMP_BULK_STATUS
static int do_usb_bulkstatus(int argc, char *argv[])
{
  vendor_dump_bulk_out_pkts();
  vendor_dump_fifo_usage();
  return 0;
}
#endif

static const struct cli_cmd usb_cmd[] =
{
  CMDENTRY(connect, do_usb_connect, "", ""),
  CMDENTRY(disconnect, do_usb_disconnect, "", ""),
#if CFG_TUD_VENDOR_TEST
  CMDENTRY(loopback, do_usb_loopback, "", ""),
#endif
#if CFG_TUD_VENDOR_DUMP_BULK_STATUS
  CMDENTRY(bulkstatus, do_usb_bulkstatus, "", ""),
#endif
};

static int do_usb(int argc, char *argv[])
{
  const struct cli_cmd *cmd;

  argc--;
  argv++;

  cmd = cli_find_cmd(argv[0], usb_cmd, ARRAY_SIZE(usb_cmd));
  if (cmd == NULL)
  {
    return CMD_RET_USAGE;
  }

  return cmd->handler(argc, argv);
}

CMD(usb, do_usb,
  "test routines for USB (MUSBHSFC)",
  "usb connect" OR
  "usb disconnect"
#if CFG_TUD_VENDOR_TEST
  OR
  "usb loopback <1|0>"
#endif
#if CFG_TUD_VENDOR_DUMP_BULK_STATUS
  OR
  "usb bulkstatus"
#endif
);

#endif
