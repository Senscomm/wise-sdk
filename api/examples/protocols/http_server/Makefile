ccflags-y += -I$(srctree)/include/FreeRTOS
ccflags-y += -I$(srctree)/include/freebsd
ccflags-y += -I$(srctree)/lib/
ccflags-y += -I$(srctree)/lib/ifconfig
ccflags-y += -I$(srctree)/lib/lwip/ports/freertos/include
ccflags-y += -I$(srctree)/lib/libifconfig
ccflags-y += -I$(srctree)/lib/lwip/src/include
ccflags-y += -I$(srctree)/lib/net/http_parser
ccflags-y += -I$(srctree)/api/include
ccflags-y += -I$(srctree)/api/examples/protocols/common
ccflags-$(CONFIG_DEMO_HTTP_SERVER_SIMPLE) += -I$(srctree)/lib/net/esp-tls/esp-tls-crypto

ccflags-y += -I$(srctree)/lib/net/http_server/include

obj-$(CONFIG_DEMO_HTTP_SERVER_SIMPLE) += simple/main.o
