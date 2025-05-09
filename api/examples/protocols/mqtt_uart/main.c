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
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <cmsis_os.h>
#include <hal/kernel.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "scm_uart.h"
#include <arpa/inet.h>
#include <wise_wifi.h>
#include <wise_event_loop.h>
#include "protocol_common.h"
#include <scm_wifi.h>
#include <scm_log.h>
#include "errno.h"
#include "sys/ioctl.h"
#include "transport_interface.h"
#include "core_mqtt.h"

#define MQTT_UART_MAX_LEN			64
#define MQTT_UART_HEADER_LEN			4
#define MQTT_APP_TAG 				"MQTT_UART_DEMO"
#define DEMO_TEST_TOPIC				"TestTopic"

#ifdef CONFIG_MQTT_MBEDTLS_SUPPORT
/* Transport interface implementation include header for TLS. */
#include "transport_mbedtls.h"
#define MQTT_TLS_DEMO_SUPPORT
#endif

static osTimerId_t mqtt_timer = NULL;

/* Timeout for MQTT_ProcessLoop and Sub in milliseconds.*/
#define PROCESS_LOOP_TIMEOUT_MS                ( 5000U )

static uint8_t uart_rx_buf[MQTT_UART_MAX_LEN];

struct mqtt_uart_ctx {
	enum scm_uart_idx idx;
};

static struct mqtt_uart_ctx g_uart_ctx[SCM_UART_IDX_MAX];

struct mqtt_uart_data {
	int fd;
	char *dev_name;
	osSemaphoreId_t sem;
	uint8_t sync;
	scm_uart_notify notify;
	void *ctx;
};

static struct scm_uart_cfg mqtt_uart_cfg = {
	.baudrate = UART_BDR_115200,
	.data_bits = UART_DATA_BITS_8,
	.parity = UART_NO_PARITY,
	.stop_bits = UART_STOP_BIT_1,
	.dma_en = 0,
};

struct mqtt_cmd_header {
	uint16_t head;
	uint16_t len;
};

struct mqtt_cmd {
	struct mqtt_cmd_header cmd_header;
	uint16_t payload_len;
	uint8_t *buf;
	char cmd_buf[MQTT_UART_MAX_LEN];
};

struct mqtt_cmd mqtt_uart_cmd;

static bool isEventCallbackInvoked = false;

struct mqtt_sub_pub_desc {
	const char *topic;
	int qos; /* 0 - 2 */
	const char *payload;
	int payload_len;
};
#ifdef MQTT_TLS_DEMO_SUPPORT

/* Case of using Pub/Sub with Unit Func, But recommend using Agent */
/* #define TLS_USE_MQTT_UNIT_FUNC */

#else
/*
 * coreMQTT: TransportInterface
 */
struct tcp_socket_context
{
	int sockfd;
};

struct NetworkContext
{
	struct tcp_socket_context tcp_ctx;
};
#endif
/*
 * Global context
 */
struct mqtt_demo_ctx
{
	NetworkContext_t network_ctx;
	MQTTContext_t *pMqttContext;
	/* Added for mbedtls */
#ifdef MQTT_TLS_DEMO_SUPPORT
	NetworkCredentials_t credentials_ctx;	/* for tls handshakeing */
	TlsTransportParams_t TlsTransport_ctx;	/* for tls tcp connection */
#endif
	uint8_t message_buffer[CONFIG_MQTT_DEMO_NETWORK_BUFFER_SIZE];

	/* Configuration */
	const char *broker_url;
	const char *broker_port;
	const char *client_id;
	uint16_t keep_alive_interval;
	uint32_t connack_timeout;

	/* Inter-thread comm. */
	osThreadId_t mqtt_tid;
	uint32_t wifi_conn_flag;
	uint32_t ip_got_flag;
	uint32_t mqtt_conn_flag;
	osEventFlagsId_t mqtt_conn_event;
	bool reconnect_flag;
} demo_mqtt_ctx;

#ifdef MQTT_TLS_DEMO_SUPPORT

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

/* Transport timeout in milliseconds for transport send and receive as forever */
#define TRANSPORT_SEND_RECV_TIMEOUT_MS         0	//( CONFIG_MQTT_SEND_TIMEOUT_MS )

#endif

static void mqtt_ping_callback(void *arg)
{
	MQTT_Ping(demo_mqtt_ctx.pMqttContext);
}

static void eventCallback( MQTTContext_t * pContext,
                           MQTTPacketInfo_t * pPacketInfo,
                           MQTTDeserializedInfo_t * pDeserializedInfo )
{
	( void ) pContext;
	( void ) pPacketInfo;
	( void ) pDeserializedInfo;

	/* Update the global state to indicate that event callback is invoked. */
	isEventCallbackInvoked = true;
	SCM_DBG_LOG(MQTT_APP_TAG, "MQTT receives message, the message is %s\n", &demo_mqtt_ctx.message_buffer[4]);
}

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

static MQTTStatus_t ProcessLoopWithTimeout( MQTTContext_t * pMqttContext,
                                               uint32_t ulTimeoutMs )
{
	uint32_t ulMqttProcessLoopTimeoutTime;
	uint32_t ulCurrentTime;

	MQTTStatus_t eMqttStatus = MQTTSuccess;

	ulCurrentTime = pMqttContext->getTime();
	ulMqttProcessLoopTimeoutTime = ulCurrentTime + ulTimeoutMs;

	/* Call MQTT_ProcessLoop multiple times a timeout happens, or
	 * MQTT_ProcessLoop fails. */
	while( ( ulCurrentTime < ulMqttProcessLoopTimeoutTime ) &&
			( eMqttStatus == MQTTSuccess || eMqttStatus == MQTTNeedMoreBytes ) )
	{
		eMqttStatus = MQTT_ProcessLoop( pMqttContext );
		ulCurrentTime = pMqttContext->getTime();
	}

	if( eMqttStatus == MQTTNeedMoreBytes )
	{
		eMqttStatus = MQTTSuccess;
	}

	return eMqttStatus;
}

static int sub_to_topic(struct mqtt_demo_ctx *ctx, struct mqtt_sub_pub_desc *sub_desc)
{
	MQTTContext_t *pxMQTTContext;
	uint16_t usSubscribePacketIdentifier;
	MQTTStatus_t xResult = MQTTSuccess;
	MQTTSubscribeInfo_t xMQTTSubscription[1];

	pxMQTTContext = ctx->pMqttContext;

	/* Some fields not used by this demo so start with everything at 0. */
	memset((void *) &xMQTTSubscription, 0x00, sizeof( xMQTTSubscription ));

	/* Get a unique packet id. */
	usSubscribePacketIdentifier = MQTT_GetPacketId(pxMQTTContext);

	/* Populate subscription list. */
	xMQTTSubscription[0].qos = sub_desc->qos;
	xMQTTSubscription[0].pTopicFilter = sub_desc->topic;
	xMQTTSubscription[0].topicFilterLength = ( uint16_t )strlen(sub_desc->topic);

	/* The client is now connected to the broker. Subscribe to the topic
	 * as specified in mqttexampleTOPIC at the top of this file by sending a
	 * subscribe packet then waiting for a subscribe acknowledgment (SUBACK).
	 * This client will then publish to the same topic it subscribed to, so it
	 * will expect all the messages it sends to the broker to be sent back to it
	 * from the broker. */
	xResult = MQTT_Subscribe( pxMQTTContext,
				xMQTTSubscription,
				sizeof( xMQTTSubscription ) / sizeof( MQTTSubscribeInfo_t ),
				usSubscribePacketIdentifier );

	if (xResult != MQTTSuccess ){
		SCM_ERR_LOG(MQTT_APP_TAG, "MQTT Sub failed(%d)\n", xResult);
	}
	return 0;
}

static int pub_to_topic(struct mqtt_demo_ctx *ctx, struct mqtt_sub_pub_desc *pub_desc)
{
	uint16_t usPublishPacketIdentifier;
	MQTTStatus_t xResult = MQTTSuccess;
	MQTTPublishInfo_t xMQTTPublishInfo;

	/* Some fields are not used by this demo so start with everything at 0. */
	( void ) memset( ( void * ) &xMQTTPublishInfo, 0x00, sizeof( xMQTTPublishInfo ) );

	/* If This demo uses QoS2 , need outgoingPublishRecords */
	xMQTTPublishInfo.qos = pub_desc->qos;
	xMQTTPublishInfo.retain = false;
	xMQTTPublishInfo.pTopicName = pub_desc->topic;
	xMQTTPublishInfo.topicNameLength = ( uint16_t ) strlen(pub_desc->topic);
	xMQTTPublishInfo.pPayload = pub_desc->payload;
	xMQTTPublishInfo.payloadLength = pub_desc->payload_len;

	/* Get a unique packet id. */
	usPublishPacketIdentifier = MQTT_GetPacketId( ctx->pMqttContext );

	SCM_DBG_LOG(MQTT_APP_TAG, "Publishing to the MQTT topic : %s\r\n", pub_desc->topic );

	/* Send PUBLISH packet. */
	xResult = MQTT_Publish( ctx->pMqttContext, &xMQTTPublishInfo, usPublishPacketIdentifier );

	return xResult;
}

static int mqtt_uart_notify(struct scm_uart_event *event, void *ctx)
{
	struct mqtt_uart_ctx *uart_ctx = ctx;

	switch (event->type) {
		case SCM_UART_EVENT_TX_CMPL:
			printf("UART%d TX complete\n", uart_ctx->idx);
			break;
		case SCM_UART_EVENT_RX_CMPL:
			printf("UART%d RX complete\n", uart_ctx->idx);
			break;
		default:
			printf("unknown event type : %d\n", event->type);
			break;
	}
	return 0;
}

static int mqtt_cmd_parse(struct mqtt_cmd *uart_cmd)
{
	int ret;
	struct mqtt_sub_pub_desc sub_pub_desc = {0};
	/* mqtt demo debug */
	SCM_DBG_LOG(MQTT_APP_TAG, "The payload of mqtt cmd is %x%x%x%x...%x%x, len = %d\n",
			uart_cmd->buf[0], uart_cmd->buf[1], uart_cmd->buf[2], uart_cmd->buf[3],
			uart_cmd->buf[uart_cmd->payload_len-2], uart_cmd->buf[uart_cmd->payload_len-1],
			uart_cmd->payload_len);

	if(0x00 != uart_cmd->buf[uart_cmd->payload_len - 1]){
		SCM_ERR_LOG(MQTT_APP_TAG, "The last byte of mqtt cmd is error!\n");
		return -1;
	}

	sub_pub_desc.topic = DEMO_TEST_TOPIC;
	sub_pub_desc.qos = MQTTQoS0;
	memcpy(&uart_cmd->cmd_buf[0], uart_cmd->buf, uart_cmd->payload_len);
	sub_pub_desc.payload = &uart_cmd->cmd_buf[0];
	sub_pub_desc.payload_len = uart_cmd->payload_len;

	ret = pub_to_topic(&demo_mqtt_ctx, &sub_pub_desc);
	if( ret != MQTTSuccess ){
		SCM_ERR_LOG(MQTT_APP_TAG, "MQTT PUBLISH failed.ret = %d\n", ret);
	}

	return 0;
}

static int mqtt_header_parse(struct mqtt_cmd *uart_cmd)
{
	uart_cmd->cmd_header.head = 0xfeef;
	uart_cmd->buf = uart_rx_buf;

	/*mqtt uart debug */
	SCM_DBG_LOG(MQTT_APP_TAG, "The first byte of mqtt cmd is %x\n", uart_cmd->buf[0]);
	SCM_DBG_LOG(MQTT_APP_TAG, "The second byte of mqtt cmd is %x\n", uart_cmd->buf[1]);
	SCM_DBG_LOG(MQTT_APP_TAG, "The cmd is %x\n", *((uint16_t*)uart_cmd->buf));

	if (uart_cmd->cmd_header.head == *((uint16_t*)uart_cmd->buf)){
		uart_cmd->cmd_header.len = uart_cmd->buf[2];
		SCM_DBG_LOG(MQTT_APP_TAG, "It's mqtt cmd format!\n");
	}else{
		SCM_ERR_LOG(MQTT_APP_TAG, "It's not mqtt cmd format!\n");
		return -1;
	}
	uart_cmd->payload_len = uart_cmd->cmd_header.len - MQTT_UART_HEADER_LEN;
	if(uart_cmd->payload_len > MQTT_UART_MAX_LEN){
		SCM_ERR_LOG(MQTT_APP_TAG, "Cmd length is error!\n");
		return -1;
	}
	SCM_DBG_LOG(MQTT_APP_TAG, "The payload len of mqtt cmd is %d\n", uart_cmd->payload_len);

	return 0;
}

static void uart_rx_task(void *param)
{
	int ret;
	while(1){
		if (scm_uart_rx(1, uart_rx_buf, MQTT_UART_HEADER_LEN, osWaitForever) == 0) {
			ret = mqtt_header_parse(&mqtt_uart_cmd);
			if(0 != ret){
				SCM_ERR_LOG(MQTT_APP_TAG, "The mqtt header parse fail\n");
				scm_uart_reset(1);
				goto error;
			}else{
				SCM_DBG_LOG(MQTT_APP_TAG, "The mqtt header parse success.\n");
			}
		}else{
			SCM_ERR_LOG(MQTT_APP_TAG, "rx header fail!\n");
			scm_uart_reset(1);
			goto error;
		}
		if (scm_uart_rx(1, uart_rx_buf, mqtt_uart_cmd.payload_len, osWaitForever) == 0) {
			ret = mqtt_cmd_parse(&mqtt_uart_cmd);
			if(0 != ret){
				SCM_ERR_LOG(MQTT_APP_TAG, "The mqtt cmd parse fail.\n");
				scm_uart_reset(1);
			}else{
				SCM_DBG_LOG(MQTT_APP_TAG, "The mqtt cmd parse success, and publish to broker.\n");
				scm_uart_reset(1);
			}
		}else{
			SCM_ERR_LOG(MQTT_APP_TAG, "rx payload fail!\n");
			scm_uart_reset(1);
		}
error:
		continue;
	}
}

#ifdef MQTT_TLS_DEMO_SUPPORT

#define MAX_CONNECT_ATTEMPS CONFIG_MQTT_MAX_CONNACK_RECEIVE_RETRY_COUNT

/**
 * @brief Connect to MQTT broker.
 *
 * @param[out] pxNetworkContext The output parameter to return the created network context.
 *
 * @return The status of the final connection attempt.
 */

static TlsTransportStatus_t tls_Connect_to_Server( struct mqtt_demo_ctx *ctx, NetworkCredentials_t * pxNetworkCredentials,
								NetworkContext_t * pxNetworkContext )
{
	TlsTransportStatus_t xNetworkStatus;
	int MQTT_TLS_CONN_TRY = 0;

	/* Set the credentials for establishing a TLS connection. */
	pxNetworkCredentials->pRootCa = ( const unsigned char * ) democonfigROOT_CA_PEM;
	pxNetworkCredentials->rootCaSize = sizeof( democonfigROOT_CA_PEM );
	pxNetworkCredentials->disableSni = pdFALSE;

	/* Establish a TLS session with the MQTT broker. */
	SCM_DBG_LOG(MQTT_APP_TAG, "Creating a TLS connection to %s:%u.\r\n", ctx->broker_url, atoi(ctx->broker_port));

	/* Attempt to create a server-authenticated TLS connection. */
	do
	{
		/* Attempt to create a server-authenticated TLS connection. */
		xNetworkStatus = scm_tls_Connect( pxNetworkContext,
						ctx->broker_url,
						atoi(ctx->broker_port),
						pxNetworkCredentials,
						TRANSPORT_SEND_RECV_TIMEOUT_MS,
						TRANSPORT_SEND_RECV_TIMEOUT_MS );

		if( xNetworkStatus != TLS_TRANSPORT_SUCCESS )
		{
			/* delay 100ms */
			vTaskDelay( pdMS_TO_TICKS( PROCESS_LOOP_TIMEOUT_MS ) );
		}
	} while( ( xNetworkStatus != TLS_TRANSPORT_SUCCESS ) && ( MQTT_TLS_CONN_TRY++ < MAX_CONNECT_ATTEMPS) );

	return xNetworkStatus;
}

/**
 * @brief Sends an MQTT Connect packet over the already connected TLS over TCP connection.
 *
 * @param[in, out] pxMQTTContext MQTT context pointer.
 * @param[in] xNetworkContext network context.
 */
static int tls_Mqtt_Handshake_with_Broker( void *params,
						MQTTContext_t * pxMQTTContext,
						NetworkContext_t * pxNetworkContext,
						bool clean_session)
{
	MQTTStatus_t xResult;
	MQTTConnectInfo_t xConnectInfo;
	bool xSessionPresent;
	MQTTPublishInfo_t willInfo;
	TransportInterface_t xTransport;
	struct mqtt_demo_ctx *ctx = params;

	/* Fill in Transport Interface send and receive function */
	xTransport.pNetworkContext = NULL;
	xTransport.send = scm_tls_Send;
	xTransport.recv = scm_tls_Recv;
	xTransport.writev = NULL;

	MQTTFixedBuffer_t fixed_buf = {
		.pBuffer = ctx->message_buffer,
		.size = CONFIG_MQTT_DEMO_NETWORK_BUFFER_SIZE
	};

	if(ctx->pMqttContext == NULL){
		ctx->pMqttContext = malloc(sizeof(MQTTContext_t));
	}
	xTransport.pNetworkContext = &ctx->network_ctx;

	/* Initialize MQTT library. */
	xResult = MQTT_Init(ctx->pMqttContext, &xTransport, current_time, eventCallback, &fixed_buf);
	if( xResult != MQTTSuccess )
		SCM_ERR_LOG(MQTT_APP_TAG, "MQTT_Init failed.");

	/* Some fields are not used in this demo so start with everything at 0. */
	( void ) memset( ( void * ) &xConnectInfo, 0x00, sizeof( xConnectInfo ) );
	memset(&willInfo, 0, sizeof(MQTTPublishInfo_t));
	willInfo.pTopicName = DEMO_TEST_TOPIC;
	willInfo.topicNameLength = strlen(willInfo.pTopicName);
	willInfo.pPayload = "welcome back";
	willInfo.payloadLength = strlen(willInfo.pPayload);
	/* Start with a clean session i.e. direct the MQTT broker to discard any
	 * previous session data. Also, establishing a connection with clean session
	 * will ensure that the broker does not store any data when this client
	 * gets disconnected. */
	xConnectInfo.cleanSession = clean_session;

	/* The client identifier is used to uniquely identify this MQTT client to the MQTT broker. */
	xConnectInfo.pClientIdentifier = ctx->client_id;
	xConnectInfo.clientIdentifierLength = (uint16_t)strlen(ctx->client_id);

	/* Set MQTT keep-alive period. If the application does not send packets at an interval less than
	 * the keep-alive period, the MQTT library will send PINGREQ packets. */
	xConnectInfo.keepAliveSeconds = ctx->keep_alive_interval;

	/* Send MQTT CONNECT packet to broker. LWT is not used in this demo, so it is passed as NULL. */
	xResult = MQTT_Connect( pxMQTTContext, &xConnectInfo, &willInfo, ctx->connack_timeout, &xSessionPresent );
	if (xResult != MQTTSuccess) {
		SCM_ERR_LOG(MQTT_APP_TAG, "MQTT_Connect failed error: %d\n", xResult);
		return xResult;
	}

	SCM_DBG_LOG(MQTT_APP_TAG, "Session present: %d\n", xSessionPresent);

	return MQTTSuccess;
}

static void mqtt_tls_task(void *param)
{
	int ret;
	MQTTContext_t *pxMQTTContext;
	NetworkCredentials_t *pxNetworkCredentials;
	TlsTransportStatus_t xNetworkStatus;
	NetworkContext_t *xNetworkContext;

	struct mqtt_demo_ctx *ctx = (struct mqtt_demo_ctx *)param;

	if(ctx->pMqttContext == NULL){
		ctx->pMqttContext = malloc(sizeof(MQTTContext_t));
	}

	/* Get the Network context */
	xNetworkContext = &ctx->network_ctx;

	pxMQTTContext = ctx->pMqttContext;

	pxNetworkCredentials = &ctx->credentials_ctx;

	/* Set the pParams member of the network context with desired transport. */
	xNetworkContext->pParams = &ctx->TlsTransport_ctx;

#ifdef CONFIG_DEMO_WIFI_CONF
	demo_wifi_connect();
#endif

	/****************************** Connect & IP ******************************/
	pxMQTTContext->getTime = current_time;

	osThreadFlagsClear(ctx->wifi_conn_flag | ctx->ip_got_flag);

	/* Wait for the interface to be up and running. */
	osThreadFlagsWait(ctx->wifi_conn_flag | ctx->ip_got_flag, osFlagsWaitAll, osWaitForever);

	/* Attempt to establish a TLS connection with the MQTT broker. */

	xNetworkStatus = tls_Connect_to_Server( ctx, pxNetworkCredentials, xNetworkContext );
	if (xNetworkStatus != TLS_TRANSPORT_SUCCESS) {
		SCM_ERR_LOG(MQTT_APP_TAG, "MQTT TLS Connect failed: %s\n", strerror(errno));
		return;
	}

	/* Send an MQTT CONNECT packet over the established TLS connection,
	 * and wait for the connection acknowledgment (CONNACK) packet. */
	ret = tls_Mqtt_Handshake_with_Broker((void *)ctx,  pxMQTTContext, xNetworkContext, true );
	if (ret){
		SCM_ERR_LOG(MQTT_APP_TAG, "MQTT TLS established with broker failed\n");
		return;
	}

	mqtt_timer = osTimerNew(mqtt_ping_callback, osTimerPeriodic, NULL, NULL);
	osTimerStart(mqtt_timer, ms_to_tick(55000));
	osEventFlagsSet(ctx->mqtt_conn_event, ctx->mqtt_conn_flag);
}
#else /* MQTT_TLS_DEMO_SUPPORT */
static int32_t transport_recv(NetworkContext_t *net_ctx, void *buf, size_t len)
{
	int32_t ret = 0;

	if (net_ctx == NULL || net_ctx->tcp_ctx.sockfd <= 0 ) {
		SCM_ERR_LOG(MQTT_APP_TAG, "invalid input, net_ctx=%p\n", net_ctx);
		ret = -1;
	} else if (buf == NULL) {
		SCM_ERR_LOG(MQTT_APP_TAG, "invalid input, buf == NULL\n");
		ret = -1;
	} else if (len == 0) {
		SCM_ERR_LOG(MQTT_APP_TAG, "invalid input, len == 0\n");
		ret = -1;
	} else {
		ret = recv(net_ctx->tcp_ctx.sockfd, buf, len, 0);
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

	if (net_ctx == NULL || net_ctx->tcp_ctx.sockfd <= 0 ) {
		SCM_ERR_LOG(MQTT_APP_TAG, "invalid input, net_ctx=%p\n", net_ctx);
		ret = -1;
	} else if (buf == NULL) {
		SCM_ERR_LOG(MQTT_APP_TAG, "invalid input, buf == NULL\n");
		ret = -1;
	} else if (len == 0) {
		SCM_ERR_LOG(MQTT_APP_TAG, "invalid input, len == 0\n");
		ret = -1;
	} else {
		ret = send(net_ctx->tcp_ctx.sockfd, buf, len, 0);
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
		.size = CONFIG_MQTT_DEMO_NETWORK_BUFFER_SIZE
	};
	TransportInterface_t trans_if = {
		.pNetworkContext 	= NULL,
		.send 			 	= transport_send,
		.recv				= transport_recv,
		.writev				= NULL
	};

	if(ctx->pMqttContext == NULL){
		ctx->pMqttContext = malloc(sizeof(MQTTContext_t));
	}
	trans_if.pNetworkContext = &ctx->network_ctx;

	/* Initialize MQTT library. */
	status = MQTT_Init(ctx->pMqttContext, &trans_if, current_time, eventCallback, &fixed_buf);

	return (status == MQTTSuccess ? 0 : -1);
}

static int connect_mqtt(struct mqtt_demo_ctx *ctx, bool clean_session)
{
	MQTTStatus_t status;
	MQTTConnectInfo_t conn_info;
	MQTTPublishInfo_t willInfo;
	bool session_present = false;

	/* Many fields are not used in this demo so start with everything at 0. */
	memset(&conn_info, 0x00, sizeof(conn_info));
	memset(&willInfo, 0, sizeof(MQTTPublishInfo_t));
	willInfo.pTopicName = DEMO_TEST_TOPIC;
	willInfo.topicNameLength = strlen(willInfo.pTopicName);
	willInfo.pPayload = "welcome back";
	willInfo.payloadLength = strlen(willInfo.pPayload);
	/* Start with a clean session i.e. direct the MQTT broker to discard any
	 * previous session data. Also, establishing a connection with clean session
	 * will ensure that the broker does not store any data when this client
	 * gets disconnected. */
	conn_info.cleanSession = clean_session;

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
	status = MQTT_Connect(ctx->pMqttContext, &conn_info, &willInfo,
							ctx->connack_timeout, &session_present);
	if (status != MQTTSuccess) {
		SCM_ERR_LOG(MQTT_APP_TAG, "MQTT_Connect error: %d\n", status);
		return -1;
	}

	SCM_DBG_LOG(MQTT_APP_TAG, "Session present: %d\n", session_present);
	return 0;
}

static int connect_socket(struct mqtt_demo_ctx *ctx)
{
	struct addrinfo hints;
	struct addrinfo *servinfo;
	struct addrinfo *itr;
	struct sockaddr_in *saddr_in __maybe_unused;
	struct timeval timeout;
	int fd;
	int ret;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family  = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	SCM_DBG_LOG(MQTT_APP_TAG, "Connecting to %s:%s...\n", ctx->broker_url, ctx->broker_port);

	ret = getaddrinfo(ctx->broker_url, ctx->broker_port, &hints, &servinfo);
	if (ret) {
		SCM_ERR_LOG(MQTT_APP_TAG, "ERROR! getaddrinfo() failed: %s\n", gai_strerror(ret));
		return -1;
	}

	itr = servinfo;
	do {
		fd = socket(itr->ai_family, itr->ai_socktype, itr->ai_protocol);
		if (fd < 0)
		{
			continue;
		}

		saddr_in = (struct sockaddr_in *)itr->ai_addr;
		SCM_DBG_LOG(MQTT_APP_TAG, "ip4 addr:%s\n", inet_ntoa(saddr_in->sin_addr));

retry:
		ret = connect(fd, itr->ai_addr, itr->ai_addrlen);
		if (ret == 0) {
			break;
		} else {
			SCM_ERR_LOG(MQTT_APP_TAG, "connect failed: %s\n", strerror(errno));
			osDelay(1000);
			goto retry;
		}

		close(fd);
		fd = -1;
	} while ((itr = itr->ai_next) != NULL);

	freeaddrinfo(servinfo);

	if (fd < 0) {
		SCM_ERR_LOG(MQTT_APP_TAG, "ERROR! Couldn't create socket\n");
		return -1;
	}

	/* Cannot block while receiving packets inside MQTT_ProcessLoop(). */

	/* XXX: cannot specify 0 because it means waiting indefinitely. */
	timeout.tv_sec = 10000;
	timeout.tv_usec = 0;

	ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	if (ret < 0) {
		SCM_ERR_LOG(MQTT_APP_TAG, "ERROR! setsockopt() failed: %s\n", strerror(errno));
		return -1;
	}

	ctx->network_ctx.tcp_ctx.sockfd = fd;

	SCM_DBG_LOG(MQTT_APP_TAG, "TCP connection established\n");

	return 0;
}

static int connect_mqtt_broker(struct mqtt_demo_ctx *ctx)
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

static void mqtt_init_task(void *param)
{
	struct mqtt_demo_ctx *ctx = param;

#ifdef CONFIG_DEMO_WIFI_CONF
	demo_wifi_connect();
#endif

	osThreadFlagsClear(ctx->wifi_conn_flag | ctx->ip_got_flag);

	/* Wait for the interface to be up and running. */
	osThreadFlagsWait(ctx->wifi_conn_flag | ctx->ip_got_flag, osFlagsWaitAll, osWaitForever);

	SCM_DBG_LOG(MQTT_APP_TAG, "Network interface is up and running.\n");

	/* Create the TCP connection to the broker, then the MQTT connection to the same. */
	connect_mqtt_broker(ctx);

	mqtt_timer = osTimerNew(mqtt_ping_callback, osTimerPeriodic, NULL, NULL);
	osTimerStart(mqtt_timer, ms_to_tick(55000));
	osEventFlagsSet(ctx->mqtt_conn_event, ctx->mqtt_conn_flag);
}
#endif /* MQTT_TLS_DEMO_SUPPORT */

static wise_err_t event_handler(void *priv, system_event_t * event)
{
	struct mqtt_demo_ctx *ctx = priv;

	switch (event->event_id) {
	case SYSTEM_EVENT_STA_GOT_IP:
		printf("\r\nWIFI GOT IP\r\n");
		osThreadFlagsSet(ctx->mqtt_tid, ctx->ip_got_flag);
		break;
	case SYSTEM_EVENT_STA_LOST_IP:
		printf("\r\nWIFI LOST IP\r\n");
		break;
	case SYSTEM_EVENT_STA_CONNECTED:
		printf("\r\nWIFI CONNECTED\r\n");
		osThreadFlagsSet(ctx->mqtt_tid, ctx->wifi_conn_flag);
		if (!event->event_info.connected.not_en_dhcp) {
			scm_wifi_status connect_status;
			netifapi_dhcp_start(scm_wifi_get_netif(WISE_IF_WIFI_STA));
			scm_wifi_sta_get_connect_info(&connect_status);
			scm_wifi_sta_dump_ap_info(&connect_status);
		}
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		printf("\r\nWIFI DISCONNECT\r\n");
		break;
	default:
		break;
	}

	return WISE_OK;
}

static void init_ctx(struct mqtt_demo_ctx *ctx)
{
	ctx->broker_url = CONFIG_MQTT_DEMO_BROKER_URL;
	ctx->broker_port = CONFIG_MQTT_DEMO_BROKER_PORT;
	ctx->client_id = CONFIG_MQTT_DEMO_CLIENT_ID;
	ctx->keep_alive_interval = CONFIG_MQTT_DEMO_KEEP_ALIVE_INTERVAL;
	ctx->connack_timeout = CONFIG_MQTT_DEMO_CONNACK_RECV_TIMEOUT_MS;
	ctx->wifi_conn_flag = (0x1 << 0); /* wise_event_loop -> mqtt */
	ctx->ip_got_flag = (0x1 << 1); /* wise_event_loop -> mqtt */
	ctx->mqtt_conn_flag = (0x1 << 2);
}

static void mqtt_rx_task(void *param)
{
	int ret;
	struct mqtt_sub_pub_desc sub_desc = {0};

	sub_desc.topic = DEMO_TEST_TOPIC;
	sub_desc.qos = MQTTQoS0;
	sub_desc.payload_len = ( uint16_t )strlen(DEMO_TEST_TOPIC);
	while(1){
		osEventFlagsWait(demo_mqtt_ctx.mqtt_conn_event, demo_mqtt_ctx.mqtt_conn_flag, osFlagsWaitAll, osWaitForever);
		ret = sub_to_topic(&demo_mqtt_ctx, &sub_desc);
		if( ret != MQTTSuccess ){
			SCM_ERR_LOG(MQTT_APP_TAG, "MQTT subscribe failed.ret = %d\n", ret);
		}else{
			break;
		}
	}
	while(ret == MQTTSuccess){
		ret = ProcessLoopWithTimeout(demo_mqtt_ctx.pMqttContext, PROCESS_LOOP_TIMEOUT_MS);
		if ( ret == MQTTSuccess ){
			SCM_DBG_LOG(MQTT_APP_TAG, "Ok\n");
			SCM_DBG_LOG(MQTT_APP_TAG, "MQTT message is %4x%4x%4x%4x %4x%4x%4x%4x %4x%4x%4x%4x %4x%4x%4x%4x %4x%4x%4x%4x %4x%4x%4x%4x\n",
						demo_mqtt_ctx.message_buffer[0],demo_mqtt_ctx.message_buffer[1],
						demo_mqtt_ctx.message_buffer[2],demo_mqtt_ctx.message_buffer[3],
						demo_mqtt_ctx.message_buffer[4],demo_mqtt_ctx.message_buffer[5],
						demo_mqtt_ctx.message_buffer[6],demo_mqtt_ctx.message_buffer[7],
						demo_mqtt_ctx.message_buffer[8],demo_mqtt_ctx.message_buffer[9],
						demo_mqtt_ctx.message_buffer[10],demo_mqtt_ctx.message_buffer[11],
						demo_mqtt_ctx.message_buffer[12],demo_mqtt_ctx.message_buffer[13],
						demo_mqtt_ctx.message_buffer[14],demo_mqtt_ctx.message_buffer[15],
						demo_mqtt_ctx.message_buffer[16],demo_mqtt_ctx.message_buffer[17],
						demo_mqtt_ctx.message_buffer[18],demo_mqtt_ctx.message_buffer[19],
						demo_mqtt_ctx.message_buffer[20],demo_mqtt_ctx.message_buffer[21],
						demo_mqtt_ctx.message_buffer[22],demo_mqtt_ctx.message_buffer[23]);
		}else{
			SCM_DBG_LOG(MQTT_APP_TAG, "Fail\n");
		}
	}
}

static void create_mqtt_rx_task(void)
{
	osThreadAttr_t attr = {
		.name		= "mqtt rx",
		.stack_size 	= 4096,
		.priority	= osPriorityNormal,
	};
	demo_mqtt_ctx.mqtt_conn_event = osEventFlagsNew(NULL);
	if (osThreadNew(mqtt_rx_task, NULL, &attr) == NULL) {
		SCM_ERR_LOG(MQTT_APP_TAG, "Task creation failed!\n");
	}
}

static void create_mqtt_init_task(void)
{
	osThreadAttr_t attr = {
		.name		= "mqtt init",
#ifdef MQTT_TLS_DEMO_SUPPORT
		.stack_size = 6144,
#else
		.stack_size = 4096,
#endif
		.priority	= osPriorityNormal,
	};
	osThreadId_t tid;

#ifdef MQTT_TLS_DEMO_SUPPORT
	tid = osThreadNew(mqtt_tls_task, &demo_mqtt_ctx, &attr);
#else
	tid = osThreadNew(mqtt_init_task, &demo_mqtt_ctx, &attr);
#endif
	if (tid == NULL) {
		SCM_ERR_LOG(MQTT_APP_TAG,"Task creation failed!\n");
	}
	demo_mqtt_ctx.mqtt_tid = tid;
}

static void create_uart_rx_task(void)
{
	osThreadAttr_t attr = {
		.name		= "mqtt_uart_rx",
		.stack_size 	= 2048,
		.priority	= osPriorityNormal,
	};
	if (osThreadNew(uart_rx_task, NULL, &attr) == NULL) {
		SCM_ERR_LOG(MQTT_APP_TAG,"Task creation failed!\n");
	}
}

int main(void)
{
	int ret;

	printf("MQTT UART TEST!\n");
	g_uart_ctx[1].idx = SCM_UART_IDX_1;
	ret = scm_uart_init(SCM_UART_IDX_1, &mqtt_uart_cfg, mqtt_uart_notify, &g_uart_ctx[1]);
	if (ret != 0) {
		SCM_ERR_LOG(MQTT_APP_TAG, "Init failed!\n");
		return -1;
	}
	create_uart_rx_task();

	memset(&demo_mqtt_ctx, 0, sizeof(demo_mqtt_ctx));
	init_ctx(&demo_mqtt_ctx);
	scm_wifi_register_event_callback(event_handler, &demo_mqtt_ctx);

	create_mqtt_init_task();
	create_mqtt_rx_task();

	demo_mqtt_ctx.reconnect_flag = false;

	return 0;
}
