ccflags-y += -I$(srctree)/include/FreeRTOS
ccflags-y += -I$(srctree)/include/freebsd
ccflags-y += -I$(srctree)/lib/net/esp-tls
ccflags-y += -I$(srctree)/lib/net/esp-tls/esp-tls-crypto
ccflags-y += -I$(srctree)/api/include
ccflags-y += -I$(srctree)/lib/lwip/src/include
ccflags-y += -I$(srctree)/lib/lwip/ports/freertos/include
ccflags-y += -I$(srctree)/lib/mbedtls-2.16.2/include
ccflags-y += -I$(src)/include
ccflags-y += -I$(src)/private_include

obj-y += transport.o
obj-y += transport_ssl.o
obj-y += transport_internal.o
obj-y += transport_socks_proxy.o
obj-$(CONFIG_WS_TRANSPORT) += transport_ws.o
obj-$(CONFIG_ESP_TLS_USING_WOLFSSL) += esp_tls_wolfssl.o
