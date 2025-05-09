ccflags-y += -I$(srctree)/include/FreeRTOS
ccflags-y += -I$(srctree)/include/freebsd
ccflags-y += -I$(srctree)/lib/net/http_parser
ccflags-y += -I$(srctree)/api/include
ccflags-y += -I$(srctree)/lib/lwip/src/include
ccflags-y += -I$(srctree)/lib/lwip/ports/freertos/include
ccflags-y += -I$(srctree)/lib/mbedtls-2.16.2/include
ccflags-y += -I$(src)
ccflags-y += -I$(src)/esp-tls-crypto
ccflags-y += -I$(src)/private_include

obj-y += esp_tls.o
obj-y += esp-tls-crypto/esp_tls_crypto.o
obj-y += esp_tls_error_capture.o
obj-y += esp_tls_platform_port.o
obj-$(CONFIG_ESP_TLS_USING_MBEDTLS) += esp_tls_mbedtls.o
obj-$(CONFIG_ESP_TLS_USING_WOLFSSL) += esp_tls_wolfssl.o
