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

/*
 * This demo is based on coreMQTT and coreMQTT-Agent libraries.
 * Refer to the following websites for details.
 * https://freertos.github.io/coreMQTT/v2.1.1/index.html
 * https://freertos.github.io/coreMQTT-Agent/v1.2.0/index.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <assert.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <vfs.h>

#include <arpa/inet.h>

#include <wise_err.h>
#include <wise_wifi.h>
#include <wise_event_loop.h>

#include <scm_log.h>
#include <scm_wifi.h>

#include "protocol_common.h"

#include "FreeRTOS.h"
#include "vqueue.h"

#include "transport_interface.h"
#include "core_mqtt_agent.h"
#include "core_mqtt_agent_message_interface.h"

#include "esp_transport.h"
#include "esp_transport_tcp.h"
#include "esp_transport_ssl.h"
#include "esp_transport_internal.h"

#define MQTT_APP_TAG "MQTT_APP"

#define MQTT_USERNAME_MAX 32
#define MQTT_PASSWORD_MAX 32

#define KEEP_ALIVE_MARGIN 5

/*
 * coreMQTT: TransportInterface
 */

struct NetworkContext
{
    esp_transport_handle_t tph;
};

/**
 * Server's root CA certificate.
 *
 * This certificate should be PEM-encoded.
 *
 * Must include the PEM header and footer:
 * "-----BEGIN CERTIFICATE-----\n"\
 * "...base64 data...\n"\
 * "-----END CERTIFICATE-----\n"
 */
#define democonfigROOT_CA_PEM  \
    "-----BEGIN CERTIFICATE-----\n"\
    "MIIEAzCCAuugAwIBAgIUBY1hlCGvdj4NhBXkZ/uLUZNILAwwDQYJKoZIhvcNAQEL\n"\
    "BQAwgZAxCzAJBgNVBAYTAkdCMRcwFQYDVQQIDA5Vbml0ZWQgS2luZ2RvbTEOMAwG\n"\
    "A1UEBwwFRGVyYnkxEjAQBgNVBAoMCU1vc3F1aXR0bzELMAkGA1UECwwCQ0ExFjAU\n"\
    "BgNVBAMMDW1vc3F1aXR0by5vcmcxHzAdBgkqhkiG9w0BCQEWEHJvZ2VyQGF0Y2hv\n"\
    "by5vcmcwHhcNMjAwNjA5MTEwNjM5WhcNMzAwNjA3MTEwNjM5WjCBkDELMAkGA1UE\n"\
    "BhMCR0IxFzAVBgNVBAgMDlVuaXRlZCBLaW5nZG9tMQ4wDAYDVQQHDAVEZXJieTES\n"\
    "MBAGA1UECgwJTW9zcXVpdHRvMQswCQYDVQQLDAJDQTEWMBQGA1UEAwwNbW9zcXVp\n"\
    "dHRvLm9yZzEfMB0GCSqGSIb3DQEJARYQcm9nZXJAYXRjaG9vLm9yZzCCASIwDQYJ\n"\
    "KoZIhvcNAQEBBQADggEPADCCAQoCggEBAME0HKmIzfTOwkKLT3THHe+ObdizamPg\n"\
    "UZmD64Tf3zJdNeYGYn4CEXbyP6fy3tWc8S2boW6dzrH8SdFf9uo320GJA9B7U1FW\n"\
    "Te3xda/Lm3JFfaHjkWw7jBwcauQZjpGINHapHRlpiCZsquAthOgxW9SgDgYlGzEA\n"\
    "s06pkEFiMw+qDfLo/sxFKB6vQlFekMeCymjLCbNwPJyqyhFmPWwio/PDMruBTzPH\n"\
    "3cioBnrJWKXc3OjXdLGFJOfj7pP0j/dr2LH72eSvv3PQQFl90CZPFhrCUcRHSSxo\n"\
    "E6yjGOdnz7f6PveLIB574kQORwt8ePn0yidrTC1ictikED3nHYhMUOUCAwEAAaNT\n"\
    "MFEwHQYDVR0OBBYEFPVV6xBUFPiGKDyo5V3+Hbh4N9YSMB8GA1UdIwQYMBaAFPVV\n"\
    "6xBUFPiGKDyo5V3+Hbh4N9YSMA8GA1UdEwEB/wQFMAMBAf8wDQYJKoZIhvcNAQEL\n"\
    "BQADggEBAGa9kS21N70ThM6/Hj9D7mbVxKLBjVWe2TPsGfbl3rEDfZ+OKRZ2j6AC\n"\
    "6r7jb4TZO3dzF2p6dgbrlU71Y/4K0TdzIjRj3cQ3KSm41JvUQ0hZ/c04iGDg/xWf\n"\
    "+pp58nfPAYwuerruPNWmlStWAXf0UTqRtg4hQDWBuUFDJTuWuuBvEXudz74eh/wK\n"\
    "sMwfu1HFvjy5Z0iMDU8PUDepjVolOCue9ashlS4EB5IECdSR2TItnAIiIwimx839\n"\
    "LdUdRudafMu5T5Xma182OC0/u/xRlEm+tvKGGmfFcN0piqVl8OrSPBgIlb+1IKJE\n"\
    "m/XriWr/Cq4h/JfB7NTsezVslgkBaoU=\n"\
    "-----END CERTIFICATE-----\n"

/*-----------------------------------------------------------*/

/*
 * coreMQTT-Agent: Message Interface
 */
struct MQTTAgentMessageContext
{
    /* vqueue will be used for message delivery instead of
     * raw message queue, i.e., osMessageQueueXXX, because
     * it can facilitate select(). */
    int vq_fd;
};

/*
 * coreMQTT: Time Function (in milliseconds)
 */
static uint32_t current_time(void)
{
    uint32_t tick = osKernelGetTickCount();

    return (1000 * tick) / osKernelGetTickFreq();
}

static inline uint32_t ms_to_tick(uint32_t time_in_ms)
{
    return (time_in_ms * osKernelGetTickFreq()) / 1000;
}

/*
 * coreMQTT-Agent: Command Context
 */
struct MQTTAgentCommandContext
{
    MQTTStatus_t status;
    osThreadId_t tid_to_notify;
    uint32_t flag;
    void *priv;
};

/*
 * Security context
 */
typedef struct mqtt_credential_ctx
{
    const char *alpn_protos;
    bool disable_sni; /* Disable server name indication */
    char *root_ca;
    size_t root_ca_size;
    char *client_cert;
    size_t client_cert_size;
    char *client_key;
    size_t client_key_size;
} mqtt_credential_ctx_t;

/*
 * Global context
 */
struct mqtt_demo_ctx
{
    NetworkContext_t net_ctx;
    MQTTAgentMessageContext_t msg_q_ctx;
    MQTTAgentContext_t agent_ctx;
    /* 	Added for mbedtls */
    MQTTAgentCommandContext_t agent_cmd_ctx;

    mqtt_credential_ctx_t credentials_ctx;

    uint8_t message_buffer[CONFIG_MQTT_DEMO_AGENT_NETWORK_BUFFER_SIZE];

    /* tcp-transport */
    esp_transport_list_handle_t transport_list;

    /* Configuration */
    char *broker_url;
    int broker_port;
    bool secure;	/* true: TLS, false: plaintext TCP */
    char *ca_file;
    char *client_cert_file;
    char *client_key_file;
    const char *client_id;
    uint16_t keep_alive_interval;
    uint32_t connack_timeout;
    uint32_t msg_q_len;

    /* Inter-thread comm. */
    osThreadId_t agent_tid;
    osThreadId_t cli_tid;
    uint32_t wifi_conn_flag;
    uint32_t ip_got_flag;
    uint32_t ip6_got_flag;
    uint32_t agent_set_flags;
    uint32_t cmd_done_flag;
    bool reconnect_flag;
    char username[MQTT_USERNAME_MAX+1];
    char password[MQTT_PASSWORD_MAX+1];
} demo_ctx;

static void incoming_publish_cb(MQTTAgentContext_t *ctx, uint16_t pid,
        MQTTPublishInfo_t *publish_info)
{
    char *topic, *payload;

    topic = zalloc(publish_info->topicNameLength + 1);
    payload = zalloc(publish_info->payloadLength + 1);

    if (payload) {
        strncpy(payload, publish_info->pPayload,
                publish_info->payloadLength);
        SCM_INFO_LOG(MQTT_APP_TAG, "Got Message:%s published", payload);
        if (topic) {
            strncpy(topic, publish_info->pTopicName,
                    publish_info->topicNameLength);
            SCM_INFO_LOG(MQTT_APP_TAG, " on topic:%s.\n", topic);
        } else {
            SCM_INFO_LOG(MQTT_APP_TAG, ".\n");
        }
    }

    if (payload)
        free(payload);
    if (topic)
        free(topic);
}

static bool agent_message_send(MQTTAgentMessageContext_t *ctx,
        MQTTAgentCommand_t * const *cmd, uint32_t block_ms )
{
    int ret;

    ret = vqueue_put(ctx->vq_fd, cmd, 0, ms_to_tick(block_ms));

    return (ret == 0 ? true : false);
}

static bool agent_message_recv(MQTTAgentMessageContext_t *ctx,
        MQTTAgentCommand_t ** cmd, uint32_t block_ms )
{
    int ret;

    ret = vqueue_get(ctx->vq_fd, cmd, NULL, ms_to_tick(block_ms));

    return (ret == 0 ? true : false);
}

static MQTTAgentCommand_t *agent_cmd_get(uint32_t block_ms)
{
    return (MQTTAgentCommand_t *)malloc(sizeof(MQTTAgentCommand_t));
}

static bool agent_cmd_release(MQTTAgentCommand_t *cmd)
{
    free(cmd);
    return true;
}

static int32_t transport_recv(NetworkContext_t *net_ctx, void *buf, size_t len)
{
    int32_t ret = 0;

    if (net_ctx == NULL || net_ctx->tph <= 0 ) {
        SCM_ERR_LOG(MQTT_APP_TAG, "invalid input, net_ctx=%p\n", net_ctx);
        ret = -1;
    } else if (buf == NULL) {
        SCM_ERR_LOG(MQTT_APP_TAG, "invalid input, buf == NULL\n");
        ret = -1;
    } else if (len == 0) {
        SCM_ERR_LOG(MQTT_APP_TAG, "invalid input, len == 0\n");
        ret = -1;
    } else {
        ret = esp_transport_read(net_ctx->tph, buf, len, 0);
        if (ret < 0) {
            if (errno == EAGAIN)
                ret = 0;
            else
                SCM_ERR_LOG(MQTT_APP_TAG, "recv failed: %d, %s\n", errno, strerror(errno));
        }
    }

    return ret;
}

static int32_t transport_send(NetworkContext_t *net_ctx, const void *buf, size_t len)
{
    int32_t ret = 0;

    if (net_ctx == NULL || net_ctx->tph <= 0 ) {
        SCM_ERR_LOG(MQTT_APP_TAG, "invalid input, net_ctx=%p\n", net_ctx);
        ret = -1;
    } else if (buf == NULL) {
        SCM_ERR_LOG(MQTT_APP_TAG, "invalid input, buf == NULL\n");
        ret = -1;
    } else if (len == 0) {
        SCM_ERR_LOG(MQTT_APP_TAG, "invalid input, len == 0\n");
        ret = -1;
    } else {
        ret = esp_transport_write(net_ctx->tph, buf, len, 0/*-1*/);
        if (ret < 0) {
            SCM_ERR_LOG(MQTT_APP_TAG, "send failed:%s\n", strerror(errno));
        }
    }

    return ret;
}

static int init_mqtt(struct mqtt_demo_ctx *ctx)
{
    MQTTStatus_t status;
    MQTTFixedBuffer_t fixed_buf = {
        .pBuffer = ctx->message_buffer,
        .size = CONFIG_MQTT_DEMO_AGENT_NETWORK_BUFFER_SIZE
    };
    TransportInterface_t trans_if = {
        .pNetworkContext 	= NULL,
        .send 			 	= transport_send,
        .recv				= transport_recv,
        .writev				= NULL
    };
    MQTTAgentMessageInterface_t msg_if = {
        .pMsgCtx        	= NULL,
        .send           	= agent_message_send,
        .recv           	= agent_message_recv,
        .getCommand     	= agent_cmd_get,
        .releaseCommand 	= agent_cmd_release
    };

    ctx->msg_q_ctx.vq_fd = vqueue(ctx->msg_q_len, sizeof(MQTTAgentCommand_t *));
    assert(ctx->msg_q_ctx.vq_fd);
    msg_if.pMsgCtx = &ctx->msg_q_ctx;

    trans_if.pNetworkContext = &ctx->net_ctx;

    /* Initialize MQTT library. */
    status = MQTTAgent_Init(&ctx->agent_ctx, &msg_if, &fixed_buf, &trans_if,
            current_time, incoming_publish_cb, NULL);

    return (status == MQTTSuccess ? 0 : -1);
}

#ifdef CONFIG_SUPPORT_WC_MQTT_KEEPALIVE
extern struct netconn *sock_get_netconn(int sockfd);
struct tcp_pcb *get_mqtt_pcb(MQTTContext_t *mqttContext)
{
	NetworkContext_t *net_ctx = mqttContext->transportInterface.pNetworkContext;
	int fd = esp_transport_get_socket(net_ctx->tph);
	struct netconn *netconn;

	netconn = sock_get_netconn(fd);
	if (netconn)
		return netconn->pcb.tcp;
	else
		return NULL;
}
#endif

static int connect_mqtt(struct mqtt_demo_ctx *ctx, bool clean_session)
{
    MQTTStatus_t status;
    MQTTConnectInfo_t conn_info;
    bool session_present = false;
#ifdef CONFIG_SUPPORT_WC_MQTT_KEEPALIVE
    struct tcp_pcb *mqtt_pcb = NULL;
#endif

    /* Many fields are not used in this demo so start with everything at 0. */
    memset(&conn_info, 0x00, sizeof(conn_info));

    /* Start with a clean session i.e. direct the MQTT broker to discard any
     * previous session data. Also, establishing a connection with clean session
     * will ensure that the broker does not store any data when this client
     * gets disconnected. */
    conn_info.cleanSession = clean_session;

    if (strlen(ctx->username) != 0 && strlen(ctx->password) != 0) {
        conn_info.pUserName = ctx->username;
        conn_info.userNameLength = strlen(conn_info.pUserName);
        conn_info.pPassword = ctx->password;
        conn_info.passwordLength = strlen(conn_info.pPassword);
    }

    /* The client identifier is used to uniquely identify this MQTT client to
     * the MQTT broker. In a production device the identifier can be something
     * unique, such as a device serial number. */
    conn_info.pClientIdentifier = ctx->client_id;
    conn_info.clientIdentifierLength = (uint16_t)strlen(ctx->client_id);

    /* Set MQTT keep-alive period. It is the responsibility of the application
     * to ensure that the interval between Control Packets being sent does not
     * exceed the Keep Alive value. In the absence of sending any other Control
     * Packets, the Client MUST send a PINGREQ Packet.
     * This responsibility will be moved inside the agent. */
    conn_info.keepAliveSeconds = ctx->keep_alive_interval;

    /* Send MQTT CONNECT packet to broker. MQTT's Last Will and Testament feature
     * is not used in this demo, so it is passed as NULL. */
    status = MQTT_Connect(&ctx->agent_ctx.mqttContext, &conn_info, NULL,
            ctx->connack_timeout, &session_present);
    if (status != MQTTSuccess) {
        SCM_ERR_LOG(MQTT_APP_TAG, "MQTT_Connect error: %d\n", status);
        return -1;
    }
#ifdef CONFIG_SUPPORT_WC_MQTT_KEEPALIVE
    mqtt_pcb = get_mqtt_pcb(&ctx->agent_ctx.mqttContext);
    scm_wifi_set_options(SCM_WIFI_SET_SHARED_MEM_ADDR, mqtt_pcb);
#endif
    SCM_INFO_LOG(MQTT_APP_TAG, "Session present: %d\n", session_present);

    /* Resume a session if desired. */
    if (status == MQTTSuccess && clean_session == false) {
        status = MQTTAgent_ResumeSession(&ctx->agent_ctx, session_present);
        if (status != MQTTSuccess) {
            SCM_ERR_LOG(MQTT_APP_TAG, "MQTTAgent_ResumeSession error: %d\n", status);
            return -1;
        }
    }

    return 0;
}

static int connect_socket(struct mqtt_demo_ctx *ctx)
{
    int ret;

    SCM_INFO_LOG(MQTT_APP_TAG, "Connecting to %s:%d...\n", ctx->broker_url, ctx->broker_port);

    ctx->net_ctx.tph = esp_transport_list_get_transport(ctx->transport_list, ctx->secure ? "mqtts" : "mqtt");
    ret = esp_transport_connect(ctx->net_ctx.tph, ctx->broker_url, ctx->broker_port, 0);
    if (ret < 0) {
        SCM_ERR_LOG(MQTT_APP_TAG, "ERROR! esp_transport_connect() failed: %s\n", strerror(errno));
        return -1;
    }

    SCM_INFO_LOG(MQTT_APP_TAG, "TCP connection established\n");

    return 0;
}

static int disconnect_socket(struct mqtt_demo_ctx *ctx)
{
    int ret;

    ret = esp_transport_close(ctx->net_ctx.tph);
    if (ret < 0) {
        SCM_ERR_LOG(MQTT_APP_TAG, "ERROR! esp_transport_close() failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

static int connect_to_mqtt_broker(struct mqtt_demo_ctx *ctx)
{
    int ret;

    /* Initialize the MQTT context with the buffer and transport interface. */
    /* Case of auto wifi setup , fault is happened. so executed before connect_socket() */
    ret = init_mqtt(ctx);
    if (ret < 0)
        goto exit;

    /* Connect a TCP socket to the broker. */
    ret = connect_socket(ctx);
    if (ret < 0)
        goto exit;

    /* Form an MQTT connection without a persistent session. */
    ret = connect_mqtt(ctx, true);

exit:
    return ret;
}

static void mqtt_agent_loop(struct mqtt_demo_ctx *ctx)
{
    int ret;
    MQTTStatus_t status = MQTTSuccess;
    MQTTContext_t *mctx = &ctx->agent_ctx.mqttContext;
    int sockfd;
    int vq_fd;
    fd_set rfdset, efdset;
    struct timeval tv = {0,}, *ptv = NULL;
    int nfds = -1;
    bool endLoop;

    do
    {
        sockfd = esp_transport_get_socket(ctx->net_ctx.tph);
        vq_fd = ctx->msg_q_ctx.vq_fd;
        FD_ZERO(&rfdset);
        FD_ZERO(&efdset);
        FD_SET(vq_fd, &rfdset);
        FD_SET(sockfd, &rfdset);
        FD_SET(sockfd, &efdset);
        nfds = (vq_fd > sockfd) ? vq_fd : sockfd;
        if (ctx->keep_alive_interval) {
            /* Here, we should allow some buffer time to avoid issues
             * with strict keep-alive checks enforced by certain MQTT brokers.
             * We assume this `ctx->keep_alive_interval` is much larger than KEEP_ALIVE_MARGIN.
             */
            tv.tv_sec = (ctx->keep_alive_interval - KEEP_ALIVE_MARGIN) > 0 ?
                        ctx->keep_alive_interval - KEEP_ALIVE_MARGIN : ctx->keep_alive_interval;
            ptv = &tv;
        }
        ret = select(nfds+1, &rfdset, NULL, &efdset, ptv);

        /* MQTTAgent_CommandLoop() is effectively the agent implementation.  It
         * will manage the MQTT protocol until such time that an error occurs,
         * which could be a disconnect.  If an error occurs the MQTT context on
         * which the error happened is returned so there can be an attempt to
         * clean up and reconnect however the application writer prefers. */
        /* XXX:; MQTTAgent_CommandBody is an replacement of MQTTAgent_CommandLoop
         * to accommodate waiting on multiple events without need for polling.
         */
        status = MQTTAgent_CommandBody(&ctx->agent_ctx, &endLoop);
        if (endLoop == false) {
            /* Everything's fine. Go back to wait for events and/or packets.
            */
            continue;
        }

#ifdef CONFIG_SUPPORT_WC_MQTT_KEEPALIVE
        /* Demo needs keep disconnect and reconnect
         * ctx->broker_url is NULL means the new broker has been
         * specified and we should exit from the loop
         */
        if (ctx->broker_url)
	        status = MQTTServerRefused;
#endif
        /* The body determined that there is something wrong and wants to
         * end this loop.
         * Figure out what to do before doing so.
         */
        if (status == MQTTSuccess) {
            /* This corresponds to being terminated by MQTTAgent_Terminate.
             * We will exit the loop as directed, but need to clean up things first.
             */
            if (mctx->connectStatus == MQTTConnected) {
                /* MQTT is yet connected. So, disconnect it now.
                */
                status = MQTT_Disconnect(mctx);
                if (status != MQTTSuccess) {
                    SCM_ERR_LOG(MQTT_APP_TAG, "MQTT_Disconnect failed: %d\n", status);
                }
                mctx->connectStatus = MQTTNotConnected;
            }
            ret = disconnect_socket(ctx);
            if (ret < 0) {
                SCM_ERR_LOG(MQTT_APP_TAG, "disconnect_socket failed.\n");
            }
        } else {
            /* It may be the case that underlying transport, e.g., Wi-Fi, has
             * been disconnected under the hood, or the remote server went down
             * temporarily.
             * We could just exit this agent loop as if it has been 'termincated',
             * or we could stay here trying to reconnect to the Server
             * hoping underlying transport will be recovered soon.
             * This is the second option.
             */
            endLoop = false;
            do {
                /* Reconnect TCP. */
                disconnect_socket(ctx);

                /* Exit the loop if a reconnect request is issued. */
                if (ctx->reconnect_flag) {
                    endLoop = true;
                    break;
                }
                /* retry the socket reconnect every 5s, no need to be too frequently */
                osDelay(pdMS_TO_TICKS(5000));
                mctx->connectStatus = MQTTNotConnected;
                ret = connect_socket(ctx);
                if (ret < 0) {
                    /* Don't bother to try MQTT connection. */
                    continue;
                }
                /* MQTT Connect with a persistent session. */
                ret = connect_mqtt(ctx, false);
                if (ret == 0) {
                    SCM_INFO_LOG(MQTT_APP_TAG, "MQTT reconnected.\n");
                }
            } while (ret < 0);
        }
    } while (endLoop == false);
}

static void mqtt_agent_task(void *param)
{
    struct mqtt_demo_ctx *ctx = param;

#ifdef CONFIG_DEMO_WIFI_CONF
    if (ctx->agent_set_flags ^ (ctx->wifi_conn_flag | ctx->ip_got_flag)) {
        demo_wifi_connect();
    }
#endif

#ifdef CONFIG_LWIP_IPV6
    osThreadFlagsClear(ctx->wifi_conn_flag | ctx->ip_got_flag | ctx->ip6_got_flag);

    if (ctx->agent_set_flags ^ (ctx->wifi_conn_flag | ctx->ip_got_flag | ctx->ip6_got_flag)) {
        /* Wait for the interface to be up and running. */
        osThreadFlagsWait(ctx->wifi_conn_flag | ctx->ip_got_flag | ctx->ip6_got_flag, osFlagsWaitAll, osWaitForever);
    }
#else
    osThreadFlagsClear(ctx->wifi_conn_flag | ctx->ip_got_flag);

    if (ctx->agent_set_flags ^ (ctx->wifi_conn_flag | ctx->ip_got_flag)) {
        /* Wait for the interface to be up and running. */
        osThreadFlagsWait(ctx->wifi_conn_flag | ctx->ip_got_flag, osFlagsWaitAll, osWaitForever);
    }
#endif

    SCM_INFO_LOG(MQTT_APP_TAG, "Network interface is up and running.\n");

    /* Create the TCP connection to the broker, then the MQTT connection to the same. */
    if (!connect_to_mqtt_broker(ctx)) {
        /* This task has nothing left to do, so rather than create the MQTT
         * agent as a separate thread, it simply calls the function that implements
         * the agent - in effect turning itself into the agent. */
        mqtt_agent_loop(ctx);

        /* XXX: this should be done here, i.e., after mqtt_agent_loop has cleaned everything
         *      up for us.
         *      For example, this can't be done in terminateCallback.
         */
    } else {
        SCM_ERR_LOG(MQTT_APP_TAG, "Cannot connect to the server: %s\n", ctx->broker_url);
        free(ctx->broker_url);
        ctx->broker_url = NULL;
    }

    /* It must be safe to clean up vqueue here because we came out of the message loop.
     */
    close(ctx->msg_q_ctx.vq_fd);
    ctx->msg_q_ctx.vq_fd = 0;

    osThreadFlagsSet(ctx->cli_tid, ctx->cmd_done_flag);

    osThreadExit();
}

static wise_err_t event_handler(void *priv, system_event_t * event)
{
    struct mqtt_demo_ctx *ctx = priv;

    switch (event->event_id) {
        case SYSTEM_EVENT_STA_GOT_IP:
        {
            char ip[IP4ADDR_STRLEN_MAX] = {0,};
            scm_wifi_get_ip("wlan0", ip, sizeof(ip), NULL, 0, NULL, 0);
            SCM_INFO_LOG(MQTT_APP_TAG, "WIFI GOT IP: <%s>\n", ip);
            osThreadFlagsSet(ctx->agent_tid, ctx->ip_got_flag);
            ctx->agent_set_flags |= ctx->ip_got_flag;
            break;
        }
#ifdef CONFIG_LWIP_IPV6
        case SYSTEM_EVENT_GOT_IP6:
            ip6_addr_t *addr __maybe_unused = &event->event_info.got_ip6.ip6_info.ip;
		SCM_INFO_LOG(MQTT_APP_TAG, "WIFI GOT IP6 %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
				IP6_ADDR_BLOCK1(addr),
				IP6_ADDR_BLOCK2(addr),
				IP6_ADDR_BLOCK3(addr),
				IP6_ADDR_BLOCK4(addr),
				IP6_ADDR_BLOCK5(addr),
				IP6_ADDR_BLOCK6(addr),
				IP6_ADDR_BLOCK7(addr),
				IP6_ADDR_BLOCK8(addr));
            osThreadFlagsSet(ctx->agent_tid, ctx->ip6_got_flag);
            ctx->agent_set_flags |= ctx->ip6_got_flag;
            break;
#endif
        case SYSTEM_EVENT_STA_LOST_IP:
            SCM_INFO_LOG(MQTT_APP_TAG, "\r\nWIFI LOST IP\r\n");
            ctx->agent_set_flags &= ~(ctx->ip_got_flag | ctx->ip6_got_flag);
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            SCM_INFO_LOG(MQTT_APP_TAG, "\r\nWIFI CONNECTED\r\n");
            osThreadFlagsSet(ctx->agent_tid, ctx->wifi_conn_flag);
            ctx->agent_set_flags |= ctx->wifi_conn_flag;
            if (!event->event_info.connected.not_en_dhcp) {
                scm_wifi_status connect_status;
                netifapi_dhcp_start(scm_wifi_get_netif(WISE_IF_WIFI_STA));
                scm_wifi_sta_get_connect_info(&connect_status);
                scm_wifi_sta_dump_ap_info(&connect_status);
            }
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ctx->agent_set_flags &= ~ctx->wifi_conn_flag;
            SCM_INFO_LOG(MQTT_APP_TAG, "\r\nWIFI DISCONNECT\r\n");
            break;
        default:
            break;
    }

    return WISE_OK;
}

static void init_ctx(struct mqtt_demo_ctx *ctx)
{
    memset(&demo_ctx, 0, sizeof(demo_ctx));
    ctx->client_id = CONFIG_MQTT_DEMO_CLIENT_ID;
    ctx->keep_alive_interval = CONFIG_MQTT_DEMO_KEEP_ALIVE_INTERVAL;
    ctx->connack_timeout = CONFIG_MQTT_DEMO_CONNACK_RECV_TIMEOUT_MS;
    ctx->msg_q_len = CONFIG_MQTT_DEMO_AGENT_COMMAND_QUEUE_LENGTH;
    ctx->wifi_conn_flag = (0x1 << 0); /* wise_event_loop -> mqtt_agent */
    ctx->ip_got_flag = (0x1 << 1); /* wise_event_loop -> mqtt_agent */
#ifdef CONFIG_LWIP_IPV6
    ctx->ip6_got_flag = (0x1 << 2); /* wise_event_loop -> mqtt_agent */
#endif
    ctx->cmd_done_flag = (0x1 << 0); /* mqtt_agent -> init */
    ctx->cli_tid = osThreadGetId();

    ctx->credentials_ctx.root_ca = strdup((const char *)democonfigROOT_CA_PEM);
    ctx->credentials_ctx.root_ca_size = sizeof(democonfigROOT_CA_PEM);
    ctx->credentials_ctx.disable_sni = false;
}

int main(void)
{
    esp_transport_list_handle_t tlist;
    esp_transport_handle_t tinst;
    mqtt_credential_ctx_t *cr = &demo_ctx.credentials_ctx;

    init_ctx(&demo_ctx);

    scm_wifi_register_event_callback(event_handler, &demo_ctx);

    tlist = esp_transport_list_init();

    /* Add a transport for plain TCP. */
    tinst = esp_transport_tcp_init();
    esp_transport_list_add(tlist, tinst, "mqtt");

    /* Add a transport for secure connection, i.e., TLS., and configure it.
    */
    tinst = esp_transport_ssl_init();
    esp_transport_list_add(tlist, tinst, "mqtts");
    if (cr->root_ca_size > 0) {
        esp_transport_ssl_set_cert_data(tinst, cr->root_ca, cr->root_ca_size);
    }
    if (cr->client_cert_size > 0) {
        esp_transport_ssl_set_client_cert_data(tinst, cr->client_cert, cr->client_cert_size);
    }
    if (cr->client_key_size > 0) {
        esp_transport_ssl_set_client_key_data(tinst, cr->client_key, cr->client_key_size);
    }
#if defined(CONFIG_MBEDTLS_SSL_ALPN) || defined(CONFIG_WOLFSSL_HAVE_ALPN)
    if (cr->alpn_protos != '\0') {
        esp_transport_ssl_set_alpn_protocol(tinst, &cr->alpn_protos);
    }
#endif
    if (cr->disable_sni) {
        esp_transport_ssl_skip_common_name_check(tinst);
    }


    demo_ctx.transport_list = tlist;

    return 0;
}

#ifdef CONFIG_CMDLINE

#include <cli.h>

struct mqtt_demo_sub_pub_desc
{
    const char *topic;
    int qos; /* 0 - 2 */
    const char *payload;
};

static void mqttagent_cmd_complete(MQTTAgentCommandContext_t *cmd_cb_ctx,
        MQTTAgentReturnInfo_t *ret_info)
{
    cmd_cb_ctx->status = ret_info->returnCode;
    osThreadFlagsSet(cmd_cb_ctx->tid_to_notify, cmd_cb_ctx->flag);
}

static int subscribe_to_topic(struct mqtt_demo_ctx *ctx,
        struct mqtt_demo_sub_pub_desc *sub_desc)
{
    MQTTStatus_t status;
    MQTTAgentCommandContext_t cmd_ctx = {0};
    MQTTAgentCommandInfo_t cmd_info = {0};
    MQTTSubscribeInfo_t sub_info = {0};
    MQTTAgentSubscribeArgs_t sub_args = {0};

    if (ctx->agent_ctx.mqttContext.connectStatus != MQTTConnected) {
        SCM_ERR_LOG(MQTT_APP_TAG, "MQTTAgent Not Connected\n");
        return -1;
    }

    cmd_ctx.tid_to_notify = ctx->cli_tid;
    cmd_ctx.flag = ctx->cmd_done_flag;

    cmd_info.cmdCompleteCallback = mqttagent_cmd_complete;
    cmd_info.pCmdCompleteCallbackContext = &cmd_ctx;
    cmd_info.blockTimeMs = 500;

    sub_info.qos = sub_desc->qos;
    sub_info.pTopicFilter = sub_desc->topic;
    sub_info.topicFilterLength = strlen(sub_desc->topic);
    sub_args.pSubscribeInfo = &sub_info;
    sub_args.numSubscriptions = 1;

    osThreadFlagsClear(ctx->cmd_done_flag);

    status = MQTTAgent_Subscribe(&ctx->agent_ctx, &sub_args, &cmd_info);
    if (status != MQTTSuccess) {
        SCM_ERR_LOG(MQTT_APP_TAG, "MQTTAgent_Subscribe error: %d\n", status);
        return -1;
    }

    /* Wait for the command to be processed. */
    osThreadFlagsWait(ctx->cmd_done_flag, osFlagsWaitAll, osWaitForever);

    if (cmd_ctx.status != MQTTSuccess) {
        SCM_ERR_LOG(MQTT_APP_TAG, "MQTTAgent_Subscribe return error: %d\n", cmd_ctx.status);
        return -1;
    }

    SCM_INFO_LOG(MQTT_APP_TAG, "Subscription to %s done.\n", sub_desc->topic);
    return 0;
}

static int unsubscribe_from_topic(struct mqtt_demo_ctx *ctx,
        struct mqtt_demo_sub_pub_desc *unsub_desc)
{
    MQTTStatus_t status;
    MQTTAgentCommandContext_t cmd_ctx = {0};
    MQTTAgentCommandInfo_t cmd_info = {0};
    MQTTSubscribeInfo_t unsub_info = {0};
    MQTTAgentSubscribeArgs_t unsub_args = {0};

    if (ctx->agent_ctx.mqttContext.connectStatus != MQTTConnected) {
        SCM_ERR_LOG(MQTT_APP_TAG, "MQTTAgent Not Connected\n");
        return -1;
    }

    cmd_ctx.tid_to_notify = ctx->cli_tid;
    cmd_ctx.flag = ctx->cmd_done_flag;

    cmd_info.cmdCompleteCallback = mqttagent_cmd_complete;
    cmd_info.pCmdCompleteCallbackContext = &cmd_ctx;
    cmd_info.blockTimeMs = 500;

    unsub_info.pTopicFilter = unsub_desc->topic;
    unsub_info.topicFilterLength = strlen(unsub_desc->topic);
    unsub_args.pSubscribeInfo = &unsub_info;
    unsub_args.numSubscriptions = 1;

    osThreadFlagsClear(ctx->cmd_done_flag);

    status = MQTTAgent_Unsubscribe(&ctx->agent_ctx, &unsub_args, &cmd_info);
    if (status != MQTTSuccess) {
        SCM_ERR_LOG(MQTT_APP_TAG, "MQTTAgent_Unsubscribe error: %d\n", status);
        return -1;
    }

    /* Wait for the command to be processed. */
    osThreadFlagsWait(ctx->cmd_done_flag, osFlagsWaitAll, osWaitForever);

    if (cmd_ctx.status != MQTTSuccess) {
        SCM_ERR_LOG(MQTT_APP_TAG, "MQTTAgent_Unsubscribe return error: %d\n", cmd_ctx.status);
        return -1;
    }

    SCM_INFO_LOG(MQTT_APP_TAG, "Unsubscription from %s done.\n", unsub_desc->topic);
    return 0;
}

static int publish_to_topic(struct mqtt_demo_ctx *ctx,
        struct mqtt_demo_sub_pub_desc *pub_desc)
{
    MQTTStatus_t status;
    MQTTAgentCommandContext_t cmd_ctx = {0};
    MQTTAgentCommandInfo_t cmd_info = {0};
    MQTTPublishInfo_t pub_info = {0};

    if (ctx->agent_ctx.mqttContext.connectStatus != MQTTConnected) {
        SCM_ERR_LOG(MQTT_APP_TAG, "MQTTAgent Not Connected\n");
        return -1;
    }

    cmd_ctx.tid_to_notify = ctx->cli_tid;
    cmd_ctx.flag = ctx->cmd_done_flag;

    cmd_info.cmdCompleteCallback = mqttagent_cmd_complete;
    cmd_info.pCmdCompleteCallbackContext = &cmd_ctx;
    cmd_info.blockTimeMs = 500;

    pub_info.qos = pub_desc->qos;
    pub_info.pTopicName = pub_desc->topic;
    pub_info.topicNameLength = strlen(pub_desc->topic);
    pub_info.pPayload = pub_desc->payload;
    pub_info.payloadLength = strlen(pub_desc->payload);

    osThreadFlagsClear(ctx->cmd_done_flag);

    status = MQTTAgent_Publish(&ctx->agent_ctx, &pub_info, &cmd_info);
    if (status != MQTTSuccess) {
        SCM_ERR_LOG(MQTT_APP_TAG, "MQTTAgent_Publish error: %d\n", status);
        return -1;
    }

    /* Wait for the command to be processed. */
    osThreadFlagsWait(ctx->cmd_done_flag, osFlagsWaitAll, osWaitForever);

    if (cmd_ctx.status != MQTTSuccess) {
        SCM_ERR_LOG(MQTT_APP_TAG, "MQTTAgent_Publish return error: %d\n", cmd_ctx.status);
        return -1;
    }

    SCM_INFO_LOG(MQTT_APP_TAG, "Publish to %s done.\n", pub_desc->topic);
    return 0;
}

static int mqtt_ping(void *param)
{
    MQTTStatus_t status;
    MQTTAgentCommandContext_t cmd_ctx = {0};
    MQTTAgentCommandInfo_t cmd_info = {0};
    struct mqtt_demo_ctx *ctx = (struct mqtt_demo_ctx *)param;

    if (ctx->agent_ctx.mqttContext.connectStatus != MQTTConnected) {
        SCM_ERR_LOG(MQTT_APP_TAG, "MQTTAgent Not Connected\n");
        return -1;
    }

    cmd_ctx.tid_to_notify = ctx->cli_tid;
    cmd_ctx.flag = ctx->cmd_done_flag;

    cmd_info.cmdCompleteCallback = mqttagent_cmd_complete;
    cmd_info.pCmdCompleteCallbackContext = &cmd_ctx;
    cmd_info.blockTimeMs = 500;

    osThreadFlagsClear(ctx->cmd_done_flag);

    status = MQTTAgent_Ping(&ctx->agent_ctx, &cmd_info);
    if (status != MQTTSuccess) {
        SCM_ERR_LOG(MQTT_APP_TAG, "MQTTAgent_Ping error: %d\n", status);
        return -1;
    }

    /* Wait for the command to be processed. */
    osThreadFlagsWait(ctx->cmd_done_flag, osFlagsWaitAll, osWaitForever);

    if (cmd_ctx.status != MQTTSuccess) {
        SCM_ERR_LOG(MQTT_APP_TAG, "MQTTAgent_Ping return error: %d\n", cmd_ctx.status);
        return -1;
    }
    SCM_INFO_LOG(MQTT_APP_TAG, "MQTTAgent_Ping ok\n");

    return 0;
}

static void terminateCallback( MQTTAgentCommandContext_t * pCmdCallbackContext,
        MQTTAgentReturnInfo_t * pReturnInfo )
{
    SCM_INFO_LOG(MQTT_APP_TAG, "Agent terminated. returnCode: %d\r\n", pReturnInfo->returnCode );
}

static char * _mmap(const char *filename, size_t *len)
{
    FILE *f;
    char *buf;
    struct stat statbuf;

    *len = 0;
    if (!filename || !(f = fopen(filename, "r"))) {
        return NULL;
    }

    if (stat(filename, &statbuf) == -1) {
        fclose(f);
        return NULL;
    }

    buf = malloc(statbuf.st_size+1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    if (fread(buf, 1, statbuf.st_size, f) != (size_t)statbuf.st_size) {
        fclose(f);
        free(buf);
        return NULL;
    }

    buf[statbuf.st_size] = '\000';
    *len = (size_t)(statbuf.st_size + 1);
    fclose(f);

    return buf;
}

static int mqtt_cli_init(int argc, char *argv[])
{
    struct mqtt_demo_ctx *ctx = &demo_ctx;
    mqtt_credential_ctx_t *cr = &ctx->credentials_ctx;
    osThreadId_t tid;
    osThreadAttr_t attr = {
        .name		= "mqtt-agent",
        .stack_size = 6144, /* 4096 is not enough to accommodate TLS. */
        .priority	= osPriorityNormal,
    };

    if (argc < 4) {
        return CMD_RET_USAGE;
    }

    if (ctx->broker_url) {
        MQTTStatus_t status;
        MQTTAgentCommandInfo_t cmdinfo;

        ctx->reconnect_flag = true;

        /* Cancel all pending commands and stop the current agent first. */
        status = MQTTAgent_CancelAll(&ctx->agent_ctx);
        if (status != MQTTSuccess) {
            SCM_ERR_LOG(MQTT_APP_TAG, "MQTTAgent_CancelAll failed, status: %d\n", status);
            return CMD_RET_FAILURE;
        }

        /* If these two flags are being waited on in the agent task, set them first to avoid blocking. */
        osThreadFlagsSet(ctx->agent_tid, ctx->ip_got_flag | ctx->wifi_conn_flag);

        osThreadFlagsClear(ctx->cmd_done_flag);

        cmdinfo.cmdCompleteCallback = terminateCallback;
        cmdinfo.pCmdCompleteCallbackContext = (MQTTAgentCommandContext_t *)ctx;
        cmdinfo.blockTimeMs = 500;
        status = MQTTAgent_Terminate(&ctx->agent_ctx, &cmdinfo);
        if (status != MQTTSuccess) {
            SCM_ERR_LOG(MQTT_APP_TAG, "MQTTAgent_Terminate failed, status: %d\n", status);
            return CMD_RET_FAILURE;
        }

        /* Wait for the command to be processed. */
        osThreadFlagsWait(ctx->cmd_done_flag, osFlagsWaitAll, osWaitForever);

        if (ctx->broker_url) {
            free(ctx->broker_url);
            ctx->broker_url = NULL;
        }
    }

    ctx->broker_url = strdup((const char *)argv[1]);
    ctx->broker_port = atoi(argv[2]);
    ctx->secure = (atoi(argv[3]) != 0 ? true : false);
    if (ctx->secure) {
        esp_transport_handle_t tinst = esp_transport_list_get_transport(ctx->transport_list,
                "mqtts");
        if (argc > 4) {
            if (ctx->ca_file) {
                free(ctx->ca_file);
                ctx->ca_file = NULL;
            }
            ctx->ca_file = strdup((const char *)argv[4]);
            if (cr->root_ca && cr->root_ca_size > 0) {
                free(cr->root_ca);
                cr->root_ca = NULL;
            }
            cr->root_ca = _mmap(ctx->ca_file, &cr->root_ca_size);
            if (cr->root_ca == NULL) {
                SCM_ERR_LOG(MQTT_APP_TAG, "Can't find CA file at %s.\n", ctx->ca_file);
                goto error;
            }
            esp_transport_ssl_set_cert_data(tinst, cr->root_ca, cr->root_ca_size);
        }
        if (argc > 5) {
            if (argc < 7) {
                /* Both client certificate and client key are needed. */
                goto error;
            }
            if (ctx->client_cert_file) {
                free(ctx->client_cert_file);
                ctx->client_cert_file = NULL;
            }
            ctx->client_cert_file = strdup((const char *)argv[5]);
            if (cr->client_cert && cr->client_cert_size > 0) {
                free(cr->client_cert);
                cr->client_cert = NULL;
            }
            cr->client_cert = _mmap(ctx->client_cert_file, &cr->client_cert_size);
            if (cr->client_cert == NULL) {
                SCM_ERR_LOG(MQTT_APP_TAG, "Can't find client cert file at %s.\n",
                        ctx->client_cert_file);
                goto error;
            }
            esp_transport_ssl_set_client_cert_data(tinst, cr->client_cert, cr->client_cert_size);

            if (ctx->client_key_file) {
                free(ctx->client_key_file);
                ctx->client_key_file = NULL;
            }
            ctx->client_key_file = strdup((const char *)argv[6]);
            if (cr->client_key && cr->client_key_size > 0) {
                free(cr->client_key);
                cr->client_key = NULL;
            }
            cr->client_key = _mmap(ctx->client_key_file, &cr->client_key_size);
            if (cr->client_key == NULL) {
                SCM_ERR_LOG(MQTT_APP_TAG, "Can't find client key file at %s.\n",
                        ctx->client_key_file);
                goto error;
            }
            esp_transport_ssl_set_client_key_data(tinst, cr->client_key, cr->client_key_size);
        }
    }

    /* Start MQTT-Agent thread. */
    tid = osThreadNew(mqtt_agent_task, ctx, &attr);
    if (tid == NULL) {
        SCM_ERR_LOG(MQTT_APP_TAG, "task creation failed!");
        goto error;
    }

    ctx->agent_tid = tid;
    ctx->reconnect_flag = false;

    return CMD_RET_SUCCESS;

error:

    if (ctx->broker_url) {
        free(ctx->broker_url);
        ctx->broker_url = NULL;
    }
    if (ctx->ca_file) {
        free(ctx->ca_file);
        ctx->ca_file = NULL;
    }
    if (cr->root_ca) {
        free(cr->root_ca);
        cr->root_ca = NULL;
    }
    if (ctx->client_cert_file) {
        free(ctx->client_cert_file);
        ctx->client_cert_file = NULL;
    }
    if (cr->client_cert) {
        free(cr->client_cert);
        cr->client_cert = NULL;
    }
    if (ctx->client_key_file) {
        free(ctx->client_key_file);
        ctx->client_key_file = NULL;
    }
    if (cr->client_key) {
        free(cr->client_key);
        cr->client_key = NULL;
    }

    return CMD_RET_FAILURE;
}

static int mqtt_cli_subscribe(int argc, char *argv[])
{
    struct mqtt_demo_sub_pub_desc desc = {0};
    int ret;

    if (argc < 2 || argc > 3) {
        return CMD_RET_USAGE;
    }

    desc.topic = argv[1];
    if (argc == 3) {
        desc.qos = atoi(argv[2]);
    }

    ret = subscribe_to_topic(&demo_ctx, &desc);
    if (ret < 0)
        return CMD_RET_FAILURE;

    return CMD_RET_SUCCESS;
}

static int mqtt_cli_unsubscribe(int argc, char *argv[])
{
    struct mqtt_demo_sub_pub_desc desc = {0};
    int ret;

    if (argc != 2) {
        return CMD_RET_USAGE;
    }

    desc.topic = argv[1];

    ret = unsubscribe_from_topic(&demo_ctx, &desc);
    if (ret < 0)
        return CMD_RET_FAILURE;

    return CMD_RET_SUCCESS;
}

static int mqtt_cli_publish(int argc, char *argv[])
{
    struct mqtt_demo_sub_pub_desc desc = {0};
    int ret;

    if (argc < 3 || argc > 4) {
        return CMD_RET_USAGE;
    }

    desc.topic = argv[1];
    desc.payload = argv[2];
    if (argc == 4) {
        desc.qos = atoi(argv[3]);
    }

    ret = publish_to_topic(&demo_ctx, &desc);
    if (ret < 0)
        return CMD_RET_FAILURE;

    return CMD_RET_SUCCESS;
}


static int mqtt_cli_ping(int argc, char *argv[])
{
    mqtt_ping(&demo_ctx);

    return CMD_RET_SUCCESS;
}

static int mqtt_cli_cfg(int argc, char *argv[])
{
    struct mqtt_demo_ctx *ctx = &demo_ctx;

    if (argc < 3)
        return CMD_RET_FAILURE;

    strncpy(ctx->username, argv[1], MQTT_USERNAME_MAX);
    ctx->username[MQTT_USERNAME_MAX] = '\0';

    strncpy(ctx->password, argv[2], MQTT_PASSWORD_MAX);
    ctx->password[MQTT_PASSWORD_MAX] = '\0';

    return CMD_RET_SUCCESS;
}

#ifdef CONFIG_SUPPORT_WC_MQTT_KEEPALIVE
int mqtt_disconnect()
{
    MQTTStatus_t status;
    MQTTAgentCommandInfo_t cmd_info = {0};
    struct mqtt_demo_ctx *ctx = (struct mqtt_demo_ctx *)&demo_ctx;

    cmd_info.cmdCompleteCallback = NULL;
    cmd_info.pCmdCompleteCallbackContext = NULL;
    cmd_info.blockTimeMs = 500;

    status = MQTTAgent_Disconnect(&ctx->agent_ctx, &cmd_info);
    if (status != MQTTSuccess) {
        SCM_ERR_LOG(MQTT_APP_TAG, "MQTTAgent_Disconnect error: %d\n", status);
        return -1;
    }

    return 0;
}
#endif

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

static const struct cli_cmd mqtt_cli_cmd[] = {
    CMDENTRY(init , mqtt_cli_init, "", ""),
    CMDENTRY(sub , mqtt_cli_subscribe, "", ""),
    CMDENTRY(unsub , mqtt_cli_unsubscribe, "", ""),
    CMDENTRY(pub , mqtt_cli_publish, "", ""),
    CMDENTRY(ping , mqtt_cli_ping, "", ""),
    CMDENTRY(cfg , mqtt_cli_cfg, "", ""),
};

static int do_mqtt_cli(int argc, char *argv[])
{
    const struct cli_cmd *cmd;

    argc--;
    argv++;

    cmd = cli_find_cmd(argv[0], mqtt_cli_cmd, ARRAY_SIZE(mqtt_cli_cmd));
    if (cmd == NULL)
        return CMD_RET_USAGE;

    return cmd->handler(argc, argv);
}

CMD(mqtt, do_mqtt_cli,
        "mqtt for MQTT client operations",
        "mqtt cfg <username> <password>" OR
        "mqtt init <url> <port> <secure(0|1)> <ca_file> <client_cert_file> <client_key_file>" OR
        "mqtt sub <topic> <qos(0|1|2)>" OR
        "mqtt unsub <topic>" OR
        "mqtt pub <topic> <payload> <qos(0|1|2)>" OR
        "mqtt ping"
   );

#endif
