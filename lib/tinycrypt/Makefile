ifeq ($(CONFIG_TINYCRYPT),y)

ccflags-y += -I$(srctree)/lib/tinycrypt/lib/include

obj-y += \
    lib/source/aes_encrypt.o \
    lib/source/aes_decrypt.o \
    lib/source/cbc_mode.o \
    lib/source/ccm_mode.o \
    lib/source/cmac_mode.o \
    lib/source/ctr_mode.o \
    lib/source/ctr_prng.o \
    lib/source/ecc.o \
    lib/source/ecc_dh.o \
    lib/source/ecc_dsa.o \
    lib/source/ecc_platform_specific.o \
    lib/source/hmac.o \
    lib/source/hmac_prng.o \
    lib/source/sha256.o \
    lib/source/utils.o

endif
