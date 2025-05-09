/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <wise_wifi.h>
#include <wise_event_loop.h>
#include <wise_log.h>
#include <scm_log.h>
#include <scm_wifi.h>
#include "protocol_common.h"
#include <scm_http_server.h>
#include "esp_tls_crypto.h"
#include "cmsis_os.h"

/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */

static const char *TAG = "HTTPD";

/*
 * Global context
 */
struct httpd_demo_ctx
{
    httpd_handle_t server;
	uint32_t wifi_conn_flag;
	uint32_t ip_got_flag;
	osThreadId_t tid;
} demo_ctx;

#if CONFIG_EXAMPLE_BASIC_AUTH

/* Structure for storing Basic Authentication info */
typedef struct {
    char    *username;
    char    *password;
} basic_auth_info_t;

#define HTTPD_401      "401 UNAUTHORIZED"           /*!< HTTP Response 401 */

/* Function to encode username and password in Base64 for Basic Authentication */
static char *http_auth_basic(const char *username, const char *password)
{
    int out;
    char *user_info = NULL;
    char *digest = NULL;
    size_t n = 0;
    asprintf(&user_info, "%s:%s", username, password);
    if (!user_info) {
        SCM_ERR_LOG(TAG, "No enough memory for user information\n");
        return NULL;
    }
    esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char *)user_info, strlen(user_info));

    /* 6: The length of the "Basic " string
     * n: Number of bytes for a base64 encode format
     * 1: Number of bytes for a reserved which be used to fill zero
    */
    digest = calloc(1, 6 + n + 1);
    if (digest) {
        strcpy(digest, "Basic ");
        esp_crypto_base64_encode((unsigned char *)digest + 6, n, (size_t *)&out, (const unsigned char *)user_info, strlen(user_info));
    }
    free(user_info);
    return digest;
}

/* HTTP GET handler for Basic Authentication */
static wise_err_t basic_auth_get_handler(httpd_req_t *req)
{
    char *buf = NULL;
    size_t buf_len = 0;
    basic_auth_info_t *basic_auth_info = req->user_ctx;

    buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
    if (buf_len > 1) {
        buf = calloc(1, buf_len);
        if (!buf) {
            SCM_ERR_LOG(TAG, "No enough memory for basic authorization\n");
            return WISE_ERR_NO_MEM;
        }

        if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) == WISE_OK) {
            SCM_INFO_LOG(TAG, "Found header => Authorization: %s\n", buf);
        } else {
            SCM_ERR_LOG(TAG, "No auth value received\n");
        }

        char *auth_credentials = http_auth_basic(basic_auth_info->username, basic_auth_info->password);
        if (!auth_credentials) {
            SCM_ERR_LOG(TAG, "No enough memory for basic authorization credentials\n");
            free(buf);
            return WISE_ERR_NO_MEM;
        }

        if (strncmp(auth_credentials, buf, buf_len)) {
            SCM_ERR_LOG(TAG, "Not authenticated\n");
            httpd_resp_set_status(req, HTTPD_401);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
            httpd_resp_send(req, NULL, 0);
        } else {
            SCM_INFO_LOG(TAG, "Authenticated!\n");
            char *basic_auth_resp = NULL;
            httpd_resp_set_status(req, HTTPD_200);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            asprintf(&basic_auth_resp, "{\"authenticated\": true,\"user\": \"%s\"}", basic_auth_info->username);
            if (!basic_auth_resp) {
                SCM_ERR_LOG(TAG, "No enough memory for basic authorization response\n");
                free(auth_credentials);
                free(buf);
                return WISE_ERR_NO_MEM;
            }
            httpd_resp_send(req, basic_auth_resp, strlen(basic_auth_resp));
            free(basic_auth_resp);
        }
        free(auth_credentials);
        free(buf);
    } else {
        SCM_ERR_LOG(TAG, "No auth header received\n");
        httpd_resp_set_status(req, HTTPD_401);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
        httpd_resp_send(req, NULL, 0);
    }

    return WISE_OK;
}

/* Basic Auth URI configuration */
static httpd_uri_t basic_auth = {
    .uri       = "/basic_auth",
    .method    = HTTP_GET,
    .handler   = basic_auth_get_handler,
};

static void httpd_register_basic_auth(httpd_handle_t server)
{
    basic_auth_info_t *basic_auth_info = calloc(1, sizeof(basic_auth_info_t));
    if (basic_auth_info) {
        basic_auth_info->username = CONFIG_EXAMPLE_BASIC_AUTH_USERNAME;
        basic_auth_info->password = CONFIG_EXAMPLE_BASIC_AUTH_PASSWORD;

        basic_auth.user_ctx = basic_auth_info;
        httpd_register_uri_handler(server, &basic_auth);
    }
}
#endif

/* HTTP GET handler */
static wise_err_t hello_get_handler(httpd_req_t *req)
{
    /* Handler implementation for responding to GET requests on /hello URI */
    char*  buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == WISE_OK) {
            SCM_INFO_LOG(TAG, "Found header => Host: %s\n", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) == WISE_OK) {
            SCM_INFO_LOG(TAG, "Found header => Test-Header-2: %s\n", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-1") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-1", buf, buf_len) == WISE_OK) {
            SCM_INFO_LOG(TAG, "Found header => Test-Header-1: %s\n", buf);
        }
        free(buf);
    }

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == WISE_OK) {
            SCM_INFO_LOG(TAG, "Found URL query => %s\n", buf);
            char param[32];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == WISE_OK) {
                SCM_INFO_LOG(TAG, "Found URL query parameter => query1=%s\n", param);
            }
            if (httpd_query_key_value(buf, "query3", param, sizeof(param)) == WISE_OK) {
                SCM_INFO_LOG(TAG, "Found URL query parameter => query3=%s\n", param);
            }
            if (httpd_query_key_value(buf, "query2", param, sizeof(param)) == WISE_OK) {
                SCM_INFO_LOG(TAG, "Found URL query parameter => query2=%s\n", param);
            }
        }
        free(buf);
    }

    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        SCM_INFO_LOG(TAG, "Request headers lost\n");
    }
    return WISE_OK;
}

static const httpd_uri_t hello = {
    .uri       = "/hello",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = "Hello World!"
};

/* HTTP POST handler */
static wise_err_t echo_post_handler(httpd_req_t *req)
{
    // Handler implementation for echoing back POST data
    char buf[100];
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                        MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }

        /* Send back the same data */
        httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;

        /* Log data received */
        SCM_INFO_LOG(TAG, "=========== RECEIVED DATA ==========\n");
        SCM_INFO_LOG(TAG, "%.*s\n", ret, buf);
        SCM_INFO_LOG(TAG, "====================================\n");
    }

    /* End response */
    httpd_resp_send_chunk(req, NULL, 0);
    return WISE_OK;
}

static const httpd_uri_t echo = {
    .uri       = "/echo",
    .method    = HTTP_POST,
    .handler   = echo_post_handler,
    .user_ctx  = NULL
};

/* This handler allows the custom error handling functionality to be
 * tested from client side. For that, when a PUT request 0 is sent to
 * URI /ctrl, the /hello and /echo URIs are unregistered and following
 * custom error handler http_404_error_handler() is registered.
 * Afterwards, when /hello or /echo is requested, this custom error
 * handler is invoked which, after sending an error message to client,
 * either closes the underlying socket (when requested URI is /echo)
 * or keeps it open (when requested URI is /hello). This allows the
 * client to infer if the custom error handler is functioning as expected
 * by observing the socket state.
 */
wise_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (strcmp("/hello", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/hello URI is not available");
        /* Return WISE_OK to keep underlying socket open */
        return WISE_OK;
    } else if (strcmp("/echo", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/echo URI is not available");
        /* Return ESP_FAIL to close underlying socket */
        return ESP_FAIL;
    }
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

/* An HTTP PUT handler. This demonstrates realtime
 * registration and deregistration of URI handlers
 */
static wise_err_t ctrl_put_handler(httpd_req_t *req)
{
    char buf;
    int ret;

    if ((ret = httpd_req_recv(req, &buf, 1)) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    if (buf == '0') {
        /* URI handlers can be unregistered using the uri string */
        SCM_INFO_LOG(TAG, "Unregistering /hello and /echo URIs\n");
        httpd_unregister_uri(req->handle, "/hello");
        httpd_unregister_uri(req->handle, "/echo");
        /* Register the custom error handler */
        httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    else {
        SCM_INFO_LOG(TAG, "Registering /hello and /echo URIs\n");
        httpd_register_uri_handler(req->handle, &hello);
        httpd_register_uri_handler(req->handle, &echo);
        /* Unregister custom error handler */
        httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, NULL);
    }

    /* Respond with empty body */
    httpd_resp_send(req, NULL, 0);
    return WISE_OK;
}

static const httpd_uri_t ctrl = {
    .uri       = "/ctrl",
    .method    = HTTP_PUT,
    .handler   = ctrl_put_handler,
    .user_ctx  = NULL
};

/* Function to start the web server */
static httpd_handle_t start_webserver(void)
{
    /* Implementation to start the HTTP server and register URI handlers */
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    /* Start the httpd server */
    SCM_INFO_LOG(TAG, "Starting server on port: '%d'\n", config.server_port);
    if (httpd_start(&server, &config) == WISE_OK) {
        /* Set URI handlers */
        SCM_INFO_LOG(TAG, "Registering URI handlers\n");
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &echo);
        httpd_register_uri_handler(server, &ctrl);
        #if CONFIG_EXAMPLE_BASIC_AUTH
        httpd_register_basic_auth(server);
        #endif
        return server;
    }

    SCM_INFO_LOG(TAG, "Error starting server!\n");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    /* Stop the httpd server */
    httpd_stop(server);
}

/* Event handler for system events */
static wise_err_t event_handler(void *priv, system_event_t * event)
{
    /* Implementation for handling system events related to Wi-Fi and IP status */
	struct httpd_demo_ctx *ctx = priv;
    httpd_handle_t *server = &ctx->server;

	switch (event->event_id) {
	case SYSTEM_EVENT_STA_GOT_IP:
    {
        char ip[IP4ADDR_STRLEN_MAX] = {0,};
        scm_wifi_get_ip("wlan0", ip, sizeof(ip), NULL, 0, NULL, 0);
		SCM_INFO_LOG(TAG, "WIFI GOT IP: <%s>\n", ip);
		if (*server == NULL) {
			SCM_INFO_LOG(TAG, "Starting webserver\n");
			*server = start_webserver();
		}
		osThreadFlagsSet(ctx->tid, ctx->ip_got_flag);
		break;
    }
	case SYSTEM_EVENT_STA_LOST_IP:
		SCM_INFO_LOG(TAG, "WIFI LOST IP\n");
		break;
	case SYSTEM_EVENT_STA_CONNECTED:
		SCM_INFO_LOG(TAG, "WIFI CONNECTED\n");
		if (!event->event_info.connected.not_en_dhcp) {
			scm_wifi_status connect_status;
			netifapi_dhcp_start(scm_wifi_get_netif(WISE_IF_WIFI_STA));
			scm_wifi_sta_get_connect_info(&connect_status);
			scm_wifi_sta_dump_ap_info(&connect_status);
		}
		osThreadFlagsSet(ctx->tid, ctx->wifi_conn_flag);
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		SCM_INFO_LOG(TAG, "WIFI DISCONNECTED\n");
		if (*server) {
			SCM_INFO_LOG(TAG, "Stopping webserver\n");
			stop_webserver(*server);
			*server = NULL;
		}
		break;
	default:
		break;
	}

	return WISE_OK;
}

/* Function to initialize the global context */
static void init_ctx(struct httpd_demo_ctx *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->wifi_conn_flag = (0x1 << 0);
	ctx->ip_got_flag = (0x1 << 1);
}

int main(void)
{
    /* Pointer to the global context */
	struct httpd_demo_ctx *ctx = &demo_ctx;

    /* Initialize the global context */
	init_ctx(ctx);

    /* Get the current thread ID */
	ctx->tid = osThreadGetId();

	osThreadFlagsClear(ctx->wifi_conn_flag | ctx->ip_got_flag);

    /* Register event handlers to stop the server when Wi-Fi is disconnected,
     * and re-start it upon connection.
     */

	scm_wifi_register_event_callback(event_handler, ctx);

#ifdef CONFIG_DEMO_WIFI_CONF

    /* This helper function configures Wi-Fi, as selected in menuconfig.
     */
    demo_wifi_connect();

	/* Wait for the interface to be up and running. */
	osThreadFlagsWait(ctx->wifi_conn_flag | ctx->ip_got_flag, osFlagsWaitAll, osWaitForever);

    /* Start the server for the first time */
	if (ctx->server == NULL) {
    	ctx->server = start_webserver();
	}

#endif

	return 0;
}
