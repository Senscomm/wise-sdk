ccflags-$(CONFIG_SUPPORT_GCOV_SCDC) += -fprofile-arcs -ftest-coverage
ccflags-y += -I$(srctree)/lib/
ccflags-y += -I$(srctree)/lib/lwip/src/include
ccflags-y += -I$(srctree)/lib/lwip/ports/freertos/include
ccflags-y += -I$(srctree)/include/FreeRTOS
ccflags-y += -I$(srctree)/include/freebsd
ccflags-y += -I$(srctree)/lib/libifconfig
ccflags-y += -I$(src)
ccflags-y += -I$(srctree)/lib/lwip/src/include
ccflags-y += -I$(srctree)/lib/at
ccflags-y += -I$(srctree)/api/include

# Application
#

obj-y += scdc.o
obj-y += fwil.o
obj-y += fweh.o
obj-y += fws.o
