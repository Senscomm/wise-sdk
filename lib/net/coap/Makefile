ccflags-y += -I$(srctree)/lib/lwip/src/include
ccflags-y += -I$(srctree)/lib/mbedtls-2.28.10/include
ccflags-y += -I$(srctree)/lib/mbedtls-2.28.10/include/mbedtls
ccflags-y += -I$(srctree)/include/freebsd

ccflags-y += -I$(src)/port/include
ccflags-y += -I$(src)/libcoap/include

obj-y += libcoap/src/coap_address.o
obj-y += libcoap/src/coap_asn1.o
obj-y += libcoap/src/coap_async.o
obj-y += libcoap/src/coap_block.o
obj-y += libcoap/src/coap_cache.o
obj-y += libcoap/src/coap_debug.o
obj-y += libcoap/src/coap_dtls.o
obj-y += libcoap/src/coap_encode.o
obj-y += libcoap/src/coap_event.o
obj-y += libcoap/src/coap_hashkey.o
obj-y += libcoap/src/coap_io.o
obj-y += libcoap/src/coap_layers.o
obj-y += libcoap/src/coap_mbedtls.o
obj-y += libcoap/src/coap_mem.o
obj-y += libcoap/src/coap_net.o
obj-y += libcoap/src/coap_netif.o
obj-y += libcoap/src/coap_notls.o
obj-y += libcoap/src/coap_option.o
obj-y += libcoap/src/coap_oscore.o
obj-y += libcoap/src/coap_pdu.o
obj-y += libcoap/src/coap_prng.o
obj-y += libcoap/src/coap_resource.o
obj-y += libcoap/src/coap_session.o
obj-y += libcoap/src/coap_str.o
obj-y += libcoap/src/coap_subscribe.o
obj-y += libcoap/src/coap_tcp.o
obj-y += libcoap/src/coap_time.o
obj-y += libcoap/src/coap_uri.o
obj-y += libcoap/src/coap_ws.o

ifdef CONFIG_COAP_OSCORE_SUPPORT
obj-y += libcoap/src/oscore/oscore.o
obj-y += libcoap/src/oscore/oscore_cbor.o
obj-y += libcoap/src/oscore/oscore_context.o
obj-y += libcoap/src/oscore/oscore_cose.o
obj-y += libcoap/src/oscore/oscore_crypto.o
endif
