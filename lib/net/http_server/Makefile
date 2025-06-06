ccflags-y += -I$(srctree)/include/FreeRTOS
ccflags-y += -I$(srctree)/include/freebsd
ccflags-y += -I$(srctree)/lib/net/http_parser
ccflags-y += -I$(srctree)/api/include
ccflags-y += -I$(srctree)/lib/lwip/src/include
ccflags-y += -I$(srctree)/lib/lwip/ports/freertos/include
ccflags-y += -I$(srctree)/lib/mbedtls-2.28.10/include
ccflags-y += -I$(src)/src/util
ccflags-y += -I$(src)/src/port/wise
ccflags-y += -I$(src)/include

obj-y += src/httpd_main.o
obj-y += src/httpd_parse.o
obj-y += src/httpd_sess.o
obj-y += src/httpd_txrx.o
obj-y += src/httpd_uri.o
obj-y += src/httpd_ws.o
obj-y += src/util/ctrl_sock.o
