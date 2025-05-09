/*
 * HTTP client example
 */

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <wise_err.h>
#include <wise_wifi.h>
#include <wise_event_loop.h>

#include <scm_log.h>
#include <scm_wifi.h>

#include <cli.h>

#include <hal/kernel.h>

#include "esp_http_client.h"
#include "esp_tls.h"
#include "mem.h"

#include "protocol_common.h"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048

enum httpc_cmd_id {
	HTTPC_CMD_REST_URL,
	HTTPC_CMD_REST_HOSTNAME,
	HTTPC_CMD_AUTH,
	HTTPC_CMD_DIGEST_AUTH_MD5,
	HTTPC_CMD_DIGEST_AUTH_SHA256,
	HTTPC_CMD_NATIVE_REQUEST,
	HTTPC_CMD_STREAM_READ,
	HTTPC_CMD_SECURE_URL,
	HTTPC_CMD_SECURE_HOSTNAME,
	HTTPC_CMD_SECURE_INVALID,
};

struct httpc_cmd_event {
	enum httpc_cmd_id cmd_id;
};

static const char *test_user_agent = "SCM HTTP Client/1.0";

static const char *TAG = "HTTP_CLIENT";

static osMessageQueueId_t cmd_queue;

/* Root cert for howsmyssl.com, taken from howsmyssl_com_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.
*/
#ifdef USE_LOCALDEBUG
extern const char howsmyssl_com_root_cert_pem[];
extern const unsigned int howsmyssl_com_root_cert_pem_size;
#else
static char root_cert_pem_file[PATH_MAX + 1];
static char *howsmyssl_com_root_cert_pem;
static size_t howsmyssl_com_root_cert_pem_size;

static char *
read_file_mem(const char *filename, size_t *length)
{
	FILE *f;
	char *buf;
	struct stat statbuf;

	*length = 0;
	if (!filename || !(f = fopen(filename, "r")))
		return NULL;

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

	buf[statbuf.st_size] = '\0';
	*length = (size_t)(statbuf.st_size + 1);
	fclose(f);
	return buf;
}
#endif

static wise_err_t wifi_event_handler(void *priv, system_event_t * event)
{
	switch (event->event_id) {
	case SYSTEM_EVENT_STA_GOT_IP:
    {
        char ip[IP4ADDR_STRLEN_MAX] = {0,};
        scm_wifi_get_ip("wlan0", ip, sizeof(ip), NULL, 0, NULL, 0);
		SCM_INFO_LOG(TAG, "WIFI GOT IP: <%s>\n", ip);
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
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		SCM_INFO_LOG(TAG, "WIFI DISCONNECTED\n");
		break;
	default:
		break;
	}

	return WISE_OK;
}

esp_err_t http_client_event_handler(esp_http_client_event_t *evt)
{
	static char *output_buffer;
	static int output_len;
	int mbedtls_err = 0;
	esp_err_t err;

	switch (evt->event_id) {
		case HTTP_EVENT_ERROR:
			SCM_DBG_LOG(TAG, "HTTP_EVENT_ERROR\n");
		break;
		case HTTP_EVENT_ON_CONNECTED:
			SCM_DBG_LOG(TAG, "HTTP_EVENT_ON_CONNECTED\n");
		break;
		case HTTP_EVENT_HEADER_SENT:
			SCM_DBG_LOG(TAG, "HTTP_EVENT_HEADER_SENT\n");
		break;
		case HTTP_EVENT_ON_HEADER:
			SCM_DBG_LOG(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s\n",
					evt->header_key, evt->header_value);
		break;
		case HTTP_EVENT_ON_DATA:
			SCM_DBG_LOG(TAG, "HTTP_EVENT_ON_DATA, len=%d\n", evt->data_len);
			if (output_len == 0 && evt->user_data) {
				memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
			}

			if (!esp_http_client_is_chunked_response(evt->client)) {
				int copy_len = 0;

				if (evt->user_data) {
					copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));

					if (copy_len) {
						memcpy(evt->user_data + output_len, evt->data, copy_len);
					}
				} else {
					int content_len = esp_http_client_get_content_length(evt->client);

					if (output_buffer == NULL) {
						output_buffer = (char *)calloc(content_len + 1, sizeof(char));
						output_len = 0;

						if (output_buffer == NULL) {
							SCM_ERR_LOG(TAG, "Failed to allocation memory for output buffer\n");
							return ESP_FAIL;
						}
					}

					copy_len = MIN(evt->data_len, (content_len - output_len));
					if (copy_len) {
						memcpy(output_buffer + output_len, evt->data, copy_len);
					}
				}

				output_len += copy_len;
			}
		break;
		case HTTP_EVENT_ON_FINISH:
			SCM_DBG_LOG(TAG, "HTTP_EVENT_ON_FINISH\n");
			if (output_buffer != NULL) {
#ifdef CONFIG_EXAMPLE_ENABLE_RESPONSE_BUFFER_DUMP
				SCM_INFO_LOG(TAG, "HTTP OUTPUT DUMP\n");
				hexdump(output_buffer, output_len);
#endif
				free(output_buffer);
				output_buffer = NULL;
			}

			output_len = 0;
		break;
		case HTTP_EVENT_DISCONNECTED:
			SCM_DBG_LOG(TAG, "HTTP_EVENT_DISCONNECTED\n");

			err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data,
					&mbedtls_err, NULL);
			if (err != 0) {
				SCM_ERR_LOG(TAG, "Last esp error code : 0x%x\n", err);
				SCM_ERR_LOG(TAG, "Last mbedtls failure: 0x%x\n", mbedtls_err);
			}

			if (output_buffer != NULL) {
				free(output_buffer);
				output_buffer = NULL;
			}

			output_len = 0;
		break;
		case HTTP_EVENT_REDIRECT:
			SCM_DBG_LOG(TAG, "HTTP_EVENT_REDIRECT\n");
			esp_http_client_set_header(evt->client, "From", "user@example.com");
			esp_http_client_set_header(evt->client, "Accept", "text/html");
			esp_http_client_set_redirection(evt->client);
		break;
		default:
		break;
	}

    return ESP_OK;
}

static void http_rest_with_url(void)
{
	char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
	esp_http_client_config_t config = {
		.host = CONFIG_EXAMPLE_HTTP_ENDPOINT,
		.path = "/get",
		.query = "scm",
		.user_agent = test_user_agent,
		.event_handler = http_client_event_handler,
		.user_data = local_response_buffer,
		.disable_auto_redirect = true,
	};
	esp_err_t err;
	esp_http_client_handle_t client = esp_http_client_init(&config);

	if (client == NULL) {
		SCM_ERR_LOG(TAG, "HTTP client initialize failure\n");
		return;
	}

	/* GET */
	err = esp_http_client_perform(client);
	if (err == ESP_OK) {
		SCM_INFO_LOG(TAG, "HTTP GET Status = %d, content_length = 0x%llx\n",
			esp_http_client_get_status_code(client),
			esp_http_client_get_content_length(client));
	} else {
		SCM_ERR_LOG(TAG, "HTTP GET request failure 0x%x\n", err);
	}

	hexdump(local_response_buffer, strlen(local_response_buffer));

	/* GET */
    const char *post_data = "{\"field1\":\"value1\"}";
    esp_http_client_set_url(client, "http://"CONFIG_EXAMPLE_HTTP_ENDPOINT"/post");
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        SCM_INFO_LOG(TAG, "HTTP POST Status = %d, content_length = 0x%llx\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        SCM_ERR_LOG(TAG, "HTTP POST request failed: 0x%x\n", err);
    }

	/* PUT */
    esp_http_client_set_url(client, "http://"CONFIG_EXAMPLE_HTTP_ENDPOINT"/put");
    esp_http_client_set_method(client, HTTP_METHOD_PUT);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        SCM_INFO_LOG(TAG, "HTTP PUT Status = %d, content_length = 0x%llx\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        SCM_ERR_LOG(TAG, "HTTP PUT request failed: 0x%x\n", err);
    }

	/* PATCH */
    esp_http_client_set_url(client, "http://"CONFIG_EXAMPLE_HTTP_ENDPOINT"/patch");
    esp_http_client_set_method(client, HTTP_METHOD_PATCH);
    esp_http_client_set_post_field(client, NULL, 0);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        SCM_INFO_LOG(TAG, "HTTP PATCH Status = %d, content_length = 0x%llx\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        SCM_ERR_LOG(TAG, "HTTP PATCH request failed: 0x%x\n", err);
    }

    /* DELETE */
    esp_http_client_set_url(client, "http://"CONFIG_EXAMPLE_HTTP_ENDPOINT"/delete");
    esp_http_client_set_method(client, HTTP_METHOD_DELETE);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        SCM_INFO_LOG(TAG, "HTTP DELETE Status = %d, content_length = 0x%llx\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        SCM_ERR_LOG(TAG, "HTTP DELETE request failed: 0x%x\n", err);
    }

    /* HEAD */
    esp_http_client_set_url(client, "http://"CONFIG_EXAMPLE_HTTP_ENDPOINT"/get");
    esp_http_client_set_method(client, HTTP_METHOD_HEAD);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        SCM_INFO_LOG(TAG, "HTTP HEAD Status = %d, content_length = 0x%llx\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        SCM_ERR_LOG(TAG, "HTTP HEAD request failed: 0x%x\n", err);
    }

	esp_http_client_cleanup(client);
}

static void http_rest_with_hostname_path(void)
{
    esp_http_client_config_t config = {
        .host = CONFIG_EXAMPLE_HTTP_ENDPOINT,
        .path = "/get",
		.user_agent = test_user_agent,
        .transport_type = HTTP_TRANSPORT_OVER_TCP,
        .event_handler = http_client_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

	if (client == NULL) {
		SCM_ERR_LOG(TAG, "HTTP client initialize failure\n");
		return;
	}

    /* GET */
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        SCM_INFO_LOG(TAG, "HTTP GET Status = %d, content_length = 0x%llx\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        SCM_ERR_LOG(TAG, "HTTP GET request failed: 0x%x\n", err);
    }

    /* POST */
    const char *post_data = "field1=value1&field2=value2";
    esp_http_client_set_url(client, "/post");
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        SCM_INFO_LOG(TAG, "HTTP POST Status = %d, content_length = 0x%llx\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        SCM_ERR_LOG(TAG, "HTTP POST request failed: 0x%x\n", err);
    }

    /* PUT */
    esp_http_client_set_url(client, "/put");
    esp_http_client_set_method(client, HTTP_METHOD_PUT);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        SCM_INFO_LOG(TAG, "HTTP PUT Status = %d, content_length = 0x%llx\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        SCM_ERR_LOG(TAG, "HTTP PUT request failed: 0x%x\n", err);
    }

    /* PATCH */
    esp_http_client_set_url(client, "/patch");
    esp_http_client_set_method(client, HTTP_METHOD_PATCH);
    esp_http_client_set_post_field(client, NULL, 0);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        SCM_INFO_LOG(TAG, "HTTP PATCH Status = %d, content_length = 0x%llx\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        SCM_ERR_LOG(TAG, "HTTP PATCH request failed: 0x%x\n", err);
    }

    /* DELETE */
    esp_http_client_set_url(client, "/delete");
    esp_http_client_set_method(client, HTTP_METHOD_DELETE);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        SCM_INFO_LOG(TAG, "HTTP DELETE Status = %d, content_length = 0x%llx\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        SCM_ERR_LOG(TAG, "HTTP DELETE request failed: 0x%x\n", err);
    }

    /* HEAD */
    esp_http_client_set_url(client, "/get");
    esp_http_client_set_method(client, HTTP_METHOD_HEAD);
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        SCM_INFO_LOG(TAG, "HTTP HEAD Status = %d, content_length = %llx\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        SCM_ERR_LOG(TAG, "HTTP HEAD request failed: 0x%x\n", err);
    }

    esp_http_client_cleanup(client);
}

#if CONFIG_ESP_HTTP_CLIENT_ENABLE_BASIC_AUTH
static void http_client_basic_auth(void)
{
    esp_http_client_config_t config = {
        .url = "http://user:passwd@"CONFIG_EXAMPLE_HTTP_ENDPOINT"/basic-auth/user/passwd",
        .event_handler = http_client_event_handler,
        .auth_type = HTTP_AUTH_TYPE_BASIC,
        .max_authorization_retries = -1,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
	if (client == NULL) {
		SCM_ERR_LOG(TAG, "HTTP client initialize failure\n");
		return;
	}

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        SCM_INFO_LOG(TAG, "HTTP Basic Auth Status = %d, content_length = 0x%llx\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        SCM_ERR_LOG(TAG, "Error perform http request 0x%x", err);
    }

	esp_http_client_cleanup(client);
}
#endif

#if CONFIG_ESP_HTTP_CLIENT_ENABLE_DIGEST_AUTH
static void http_auth_digest_md5(void)
{
    esp_http_client_config_t config = {
        .url = "http://user:passwd@"CONFIG_EXAMPLE_HTTP_ENDPOINT"/digest-auth/auth/user/passwd/MD5/never",
        .event_handler = http_client_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
	if (client == NULL) {
		SCM_ERR_LOG(TAG, "HTTP client initialize failure\n");
		return;
	}

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        SCM_INFO_LOG(TAG, "HTTP MD5 Digest Auth Status = %d, content_length = 0x%llx\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        SCM_ERR_LOG(TAG, "Error performing http request 0x%x\n", err);
    }

    esp_http_client_cleanup(client);
}

static void http_auth_digest_sha256(void)
{
    esp_http_client_config_t config = {
        .url = "http://user:passwd@"CONFIG_EXAMPLE_HTTP_ENDPOINT"/digest-auth/auth/user/passwd/SHA-256/never",
        .event_handler = http_client_event_handler,
        .buffer_size_tx = 1024, // Increase buffer size as header size will increase as it contains SHA-256.
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
	if (client == NULL) {
		SCM_ERR_LOG(TAG, "HTTP client initialize failure\n");
		return;
	}

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        SCM_INFO_LOG(TAG, "HTTP SHA256 Digest Auth Status = %d, content_length = 0x%llx\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        SCM_ERR_LOG(TAG, "Error performing http request 0x%x\n", err);
    }

    esp_http_client_cleanup(client);
}
#endif

static void http_client_native_req(void)
{
    char output_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};   // Buffer to store response of http request
    int content_length = 0;
    esp_http_client_config_t config = {
        .url = "http://"CONFIG_EXAMPLE_HTTP_ENDPOINT"/get",
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
	if (client == NULL) {
		SCM_ERR_LOG(TAG, "HTTP client initialize failure\n");
		return;
	}

    /* GET Request */
    esp_http_client_set_method(client, HTTP_METHOD_GET);

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        SCM_ERR_LOG(TAG, "Failed to open HTTP connection: %x\n", err);
    } else {
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0) {
            SCM_ERR_LOG(TAG, "HTTP client fetch headers failed\n");
        } else {
            int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
            if (data_read >= 0) {
                SCM_INFO_LOG(TAG, "HTTP GET Status = %d, content_length = 0x%llx\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
                hexdump(output_buffer, data_read);
            } else {
                SCM_ERR_LOG(TAG, "Failed to read response\n");
            }
        }
    }

    esp_http_client_close(client);

    /* POST Request */
    const char *post_data = "{\"field1\":\"value1\"}";
    esp_http_client_set_url(client, "http://"CONFIG_EXAMPLE_HTTP_ENDPOINT"/post");
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    err = esp_http_client_open(client, strlen(post_data));
    if (err != ESP_OK) {
        SCM_ERR_LOG(TAG, "Failed to open HTTP connection: 0x%x\n", err);
    } else {
        int wlen = esp_http_client_write(client, post_data, strlen(post_data));
        if (wlen < 0) {
            SCM_ERR_LOG(TAG, "Write failed\n");
        }
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0) {
            SCM_ERR_LOG(TAG, "HTTP client fetch headers failed\n");
        } else {
            int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
            if (data_read >= 0) {
                SCM_INFO_LOG(TAG, "HTTP POST Status = %d, content_length = 0x%llx\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
                hexdump(output_buffer, strlen(output_buffer));
            } else {
                SCM_ERR_LOG(TAG, "Failed to read response\n");
            }
        }
    }

    esp_http_client_cleanup(client);
}

static void http_client_stream_read(void)
{
    char *buffer = malloc(MAX_HTTP_RECV_BUFFER + 1);
    if (buffer == NULL) {
        SCM_ERR_LOG(TAG, "Cannot malloc http receive buffer\n");
        return;
    }

    esp_http_client_config_t config = {
        .url = "http://"CONFIG_EXAMPLE_HTTP_ENDPOINT"/get",
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
	if (client == NULL) {
		SCM_ERR_LOG(TAG, "HTTP client initialize failure\n");
        return;
	}

    esp_err_t err;
    if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
        SCM_ERR_LOG(TAG, "Failed to open HTTP connection: 0x%x\n", err);
        free(buffer);
        return;
    }
    int content_length =  esp_http_client_fetch_headers(client);
    int total_read_len = 0, read_len = 0;
    if (total_read_len < content_length && content_length <= MAX_HTTP_RECV_BUFFER) {
        read_len = esp_http_client_read(client, buffer, content_length);
        if (read_len <= 0) {
            SCM_ERR_LOG(TAG, "Error read data\n");
        }
        buffer[read_len] = 0;
        SCM_DBG_LOG(TAG, "read_len = %d\n", read_len);
    }

    SCM_INFO_LOG(TAG, "HTTP Stream reader Status = %d, content_length = 0x%llx\n",
                    esp_http_client_get_status_code(client),
                    esp_http_client_get_content_length(client));

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

	hexdump(buffer, content_length);

    free(buffer);
}

static void http_secure_with_url(void)
{
    esp_http_client_config_t config = {
        .url = "https://www.howsmyssl.com",
        .path = "/",
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = http_client_event_handler,
    };

#ifndef USE_LOCALDEBUG
	howsmyssl_com_root_cert_pem = read_file_mem(root_cert_pem_file, &howsmyssl_com_root_cert_pem_size);
	if (howsmyssl_com_root_cert_pem == NULL) {
		SCM_ERR_LOG(TAG, "Root cert PEM file error: %s\n", root_cert_pem_file);
		return;
	}
#endif
	config.cert_pem = howsmyssl_com_root_cert_pem;
	config.cert_len = howsmyssl_com_root_cert_pem_size;

	esp_http_client_handle_t client = esp_http_client_init(&config);
	if (client == NULL) {
		SCM_ERR_LOG(TAG, "HTTP client initialize failure\n");
		return;
	}

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        SCM_INFO_LOG(TAG, "HTTPS Status = %d, content_length = 0x%llx\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        SCM_ERR_LOG(TAG, "Error perform http request 0x%8x", err);
    }

    esp_http_client_cleanup(client);
#ifndef USE_LOCALDEBUG
	free(howsmyssl_com_root_cert_pem);
	howsmyssl_com_root_cert_pem = NULL;
	howsmyssl_com_root_cert_pem_size = 0;
#endif
}

static void http_secure_with_hostname_path(void)
{
    esp_http_client_config_t config = {
        .host = "www.howsmyssl.com",
        .path = "/",
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .event_handler = http_client_event_handler,
    };

#ifndef USE_LOCALDEBUG
	howsmyssl_com_root_cert_pem = read_file_mem(root_cert_pem_file, &howsmyssl_com_root_cert_pem_size);
	if (howsmyssl_com_root_cert_pem == NULL) {
		SCM_ERR_LOG(TAG, "Root cert PEM file error: %s\n", root_cert_pem_file);
		return;
	}
#endif
	config.cert_pem = howsmyssl_com_root_cert_pem;
	config.cert_len = howsmyssl_com_root_cert_pem_size;

	esp_http_client_handle_t client = esp_http_client_init(&config);
	if (client == NULL) {
		SCM_ERR_LOG(TAG, "HTTP client initialize failure\n");
		return;
	}

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        SCM_INFO_LOG(TAG, "HTTPS Status = %d, content_length = 0x%llx\n",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        SCM_ERR_LOG(TAG, "Error perform http request 0x%x\n", err);
    }

    esp_http_client_cleanup(client);
#ifndef USE_LOCALDEBUG
	free(howsmyssl_com_root_cert_pem);
	howsmyssl_com_root_cert_pem = NULL;
	howsmyssl_com_root_cert_pem_size = 0;
#endif

}

static void http_secure_with_invalid_url(void)
{
    esp_http_client_config_t config = {
            .url = "https://not.existent.url",
            .event_handler = http_client_event_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
	if (client == NULL) {
		SCM_ERR_LOG(TAG, "HTTP client initialize failure\n");
		return;
	}

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        SCM_INFO_LOG(TAG, "HTTPS Status = %d, content_length = 0x%llx\n",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    } else {
        SCM_ERR_LOG(TAG, "Error perform http request 0x%x\n", err);
    }

    esp_http_client_cleanup(client);
}

static void httpc_cmd_thread(void *arg)
{
	struct httpc_cmd_event cmd_evt;

	while (1) {
		if (osMessageQueueGet(cmd_queue, &cmd_evt, NULL, osWaitForever) == osOK) {

			SCM_INFO_LOG(TAG, "HTTP client Start\n");

			switch (cmd_evt.cmd_id) {
				case HTTPC_CMD_REST_URL:
					http_rest_with_url();
				break;
				case HTTPC_CMD_REST_HOSTNAME:
					http_rest_with_hostname_path();
				break;
#if CONFIG_ESP_HTTP_CLIENT_ENABLE_BASIC_AUTH
				case HTTPC_CMD_AUTH:
					http_client_basic_auth();
				break;
#endif
#if CONFIG_ESP_HTTP_CLIENT_ENABLE_DIGEST_AUTH
				case HTTPC_CMD_DIGEST_AUTH_MD5:
					http_auth_digest_md5();
				break;
				case HTTPC_CMD_DIGEST_AUTH_SHA256:
					http_auth_digest_sha256();
				break;
#endif
				case HTTPC_CMD_NATIVE_REQUEST:
					http_client_native_req();
				break;
				case HTTPC_CMD_STREAM_READ:
					http_client_stream_read();
				break;
				case HTTPC_CMD_SECURE_URL:
					http_secure_with_url();
				break;
				case HTTPC_CMD_SECURE_HOSTNAME:
					http_secure_with_hostname_path();
				break;
				case HTTPC_CMD_SECURE_INVALID:
					http_secure_with_invalid_url();
				break;
				default:
					SCM_ERR_LOG(TAG, "Unknown httpc command ID : %d\n", cmd_evt.cmd_id);
				break;
			}

			SCM_INFO_LOG(TAG, "HTTP client done\n");
		}
	}
}

int main(void)
{
	osThreadAttr_t attr = {
		.name 		= "httpc",
		.stack_size = 1024 * 6,
		.priority 	= osPriorityNormal,
	};

	cmd_queue = osMessageQueueNew(4, sizeof(struct httpc_cmd_event), NULL);
	if (!cmd_queue) {
		printf("%s: failed to create httpc message queue\n", __func__);
	}

	if (!osThreadNew(httpc_cmd_thread, NULL, &attr)) {
		printf("%s: failed to create httpc command thread\n", __func__);
	}

	scm_wifi_register_event_callback(wifi_event_handler, NULL);

#ifdef CONFIG_DEMO_WIFI_CONF
	demo_wifi_connect();
#endif

	return 0;
}

static void http_client_put_cmd(enum httpc_cmd_id cmd_id)
{
	struct httpc_cmd_event cmd_evt;

	cmd_evt.cmd_id = cmd_id;

	osMessageQueuePut(cmd_queue, &cmd_evt, 0, 0);
}

static int http_client_cli_rest(int argc, char *argv[])
{
	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	if (!strcmp("url", argv[1])) {
		http_client_put_cmd(HTTPC_CMD_REST_URL);
	} else if (!strcmp("hostname", argv[1])) {
		http_client_put_cmd(HTTPC_CMD_REST_HOSTNAME);
	} else {
		printf("Invalid Type : %s\n", argv[1]);
		return CMD_RET_USAGE;
	}

	return CMD_RET_SUCCESS;
}

#if CONFIG_ESP_HTTP_CLIENT_ENABLE_BASIC_AUTH
static int http_client_cli_auth(int argc, char *argv[])
{
	http_client_put_cmd(HTTPC_CMD_AUTH);

	return CMD_RET_SUCCESS;
}
#endif

#if CONFIG_ESP_HTTP_CLIENT_ENABLE_DIGEST_AUTH
static int http_client_cli_digest_auth(int argc, char *argv[])
{
	if (argc < 2) {
		return CMD_RET_USAGE;
	}

	if (!strcmp("md5", argv[1])) {
		http_client_put_cmd(HTTPC_CMD_DIGEST_AUTH_MD5);
	} else if (!strcmp("sha256", argv[1])) {
		http_client_put_cmd(HTTPC_CMD_DIGEST_AUTH_SHA256);
	} else {
		printf("Invalid Type : %s\n", argv[1]);
		return CMD_RET_USAGE;
	}

	return CMD_RET_SUCCESS;
}
#endif

static int http_client_cli_native_req(int argc, char *argv[])
{
	http_client_put_cmd(HTTPC_CMD_NATIVE_REQUEST);

	return CMD_RET_SUCCESS;
}

static int http_client_cli_stream_read(int argc, char *argv[])
{
	http_client_put_cmd(HTTPC_CMD_STREAM_READ);

	return CMD_RET_SUCCESS;
}

static int http_client_cli_secure(int argc, char *argv[])
{
	if (argc < 2) {
		return CMD_RET_USAGE;
	}

#ifndef USE_LOCALDEBUG
	strncpy(root_cert_pem_file, argv[2], PATH_MAX);
	root_cert_pem_file[PATH_MAX] = '\0';
#endif

	if (!strcmp("url", argv[1])) {
		http_client_put_cmd(HTTPC_CMD_SECURE_URL);
	} else if (!strcmp("hostname", argv[1])) {
		http_client_put_cmd(HTTPC_CMD_SECURE_HOSTNAME);
	} else if (!strcmp("invalid", argv[1])) {
		http_client_put_cmd(HTTPC_CMD_SECURE_INVALID);
	} else {
		printf("Invalid Type : %s\n", argv[1]);
		return CMD_RET_USAGE;
	}

	return CMD_RET_SUCCESS;
}

static const struct cli_cmd http_client_cli_cmd[] = {
	CMDENTRY(rest, http_client_cli_rest, "", ""),
#if CONFIG_ESP_HTTP_CLIENT_ENABLE_BASIC_AUTH
	CMDENTRY(auth, http_client_cli_auth, "", ""),
#endif
#if CONFIG_ESP_HTTP_CLIENT_ENABLE_DIGEST_AUTH
	CMDENTRY(digest_auth, http_client_cli_digest_auth, "", ""),
#endif
	CMDENTRY(stream_read, http_client_cli_stream_read, "", ""),
	CMDENTRY(native_req, http_client_cli_native_req, "", ""),
	CMDENTRY(secure, http_client_cli_secure, "", ""),
};

static int do_http_client_cli(int argc, char *argv[])
{
	const struct cli_cmd *cmd;

	argc--;
	argv++;

	cmd = cli_find_cmd(argv[0], http_client_cli_cmd, ARRAY_SIZE(http_client_cli_cmd));
	if (cmd == NULL) {
		return CMD_RET_USAGE;
	}

	return cmd->handler(argc, argv);
}

CMD(httpc, do_http_client_cli,
		"CLI for HTTP client operations",
		"httpc rest <type : url or hostname>" OR
#if CONFIG_ESP_HTTP_CLIENT_ENABLE_BASIC_AUTH
		"httpc auth" OR
#endif
#if CONFIG_ESP_HTTP_CLIENT_ENABLE_DIGEST_AUTH
		"httpc digest_auth <type : md5 or sha256>" OR
#endif
		"httpc stream_read" OR
		"httpc native_req" OR
		"httpc secure <type : url or hostname or invalid> <PEM file path for the certificate>"
   );
