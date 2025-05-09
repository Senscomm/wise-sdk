ccflags-$(CONFIG_TINYUSB) += -I$(srctree)/lib/tinyusb/

obj-y += spi-flash.o

# Controller drivers
obj-$(CONFIG_SPI_FLASH_CONTROLLER_ADC) += spi-flash-adc.o
obj-$(CONFIG_SPI_FLASH_CONTROLLER_SANDBOX) += sandbox.o sandbox-sfdp.o
CFLAGS_sandbox.o = -D"SANDBOX_SPI_IMAGE_FILE=KBUILD_STR($(abspath $(srctree)/bin/spi.img))"
obj-$(CONFIG_ATCSPI200) += atcspi200.o

# SPI flash chip drivers
obj-$(CONFIG_SPI_FLASH_SST) += spi-flash-sst.o
obj-$(CONFIG_SPI_FLASH_XTX) += spi-flash-xtx.o
obj-$(CONFIG_SPI_FLASH_GIGADEVICE) += spi-flash-gd25.o
obj-$(CONFIG_SPI_FLASH_GIANTEC) += spi-flash-gt25.o
obj-$(CONFIG_SPI_FLASH_SPANSION) += spi-flash-s25fl.o
obj-$(CONFIG_SPI_FLASH_PUYA) += spi-flash-p25q.o
obj-$(CONFIG_SPI_FLASH_ZBIT) += spi-flash-zb25.o
obj-$(CONFIG_SPI_FLASH_XMC) += spi-flash-xmc.o
