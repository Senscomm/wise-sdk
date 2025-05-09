ifeq ($(CONFIG_SCM_MCUBOOT),y)

ccflags-y += -I$(src)/mcuboot/boot/wise/include
ccflags-y += -I$(src)/mcuboot/boot/bootutil/include
ccflags-y += -I$(src)/mcuboot/ext/mbedtls-asn1/include

ccflags-y += -I$(srctree)/lib/tinycrypt/lib/include

ifeq ($(CONFIG_SCM_MCUBOOT_BOOTLOADER),y)
obj-y += mcuboot/boot/wise/main.o
obj-y += loader/loader.o
endif

obj-y += mcuboot/boot/bootutil/src/boot_record.o
obj-y += mcuboot/boot/bootutil/src/bootutil_misc.o
obj-y += mcuboot/boot/bootutil/src/bootutil_public.o
obj-y += mcuboot/boot/bootutil/src/caps.o
obj-y += mcuboot/boot/bootutil/src/encrypted.o
obj-y += mcuboot/boot/bootutil/src/fault_injection_hardening.o
obj-y += mcuboot/boot/bootutil/src/fault_injection_hardening_delay_rng_mbedtls.o
obj-y += mcuboot/boot/bootutil/src/image_ec.o
obj-y += mcuboot/boot/bootutil/src/image_ec256.o
obj-y += mcuboot/boot/bootutil/src/image_ed25519.o
obj-y += mcuboot/boot/bootutil/src/image_rsa.o
obj-y += mcuboot/boot/bootutil/src/image_validate.o
obj-y += mcuboot/boot/bootutil/src/loader.o
obj-y += mcuboot/boot/bootutil/src/swap_misc.o
obj-y += mcuboot/boot/bootutil/src/swap_move.o
obj-y += mcuboot/boot/bootutil/src/swap_scratch.o
obj-y += mcuboot/boot/bootutil/src/tlv.o
obj-y += mcuboot/boot/wise/src/flash_map_backend/flash_map_backend.o
obj-y += mcuboot/boot/wise/src/flash_map_backend/flash_backend.o
obj-y += mcuboot/boot/wise/src/flash_map_backend/flash_crypto.o

ifeq ($(CONFIG_SCM_MCUBOOT_SIGN_AUTH),y)
obj-y += mcuboot/ext/mbedtls-asn1/src/asn1parse.o
obj-y += mcuboot/ext/mbedtls-asn1/src/platform_util.o
obj-y += mcuboot/boot/wise/keys.o
endif

ifeq ($(CONFIG_SCM_MCUBOOT_UPDATE_AGENT),y)
obj-y += update_agent/mcuboot_agent.o
endif

ifeq ($(CONFIG_SCM_MCUBOOT_SWAP_TEST),y)
obj-y += swap_test/confirm.o
obj-y += swap_test/set_img.o
obj-y += swap_test/version.o
endif

endif
