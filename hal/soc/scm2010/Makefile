quiet_cmd_wise_ram  = LD      $@
quiete_cmd_wise_rom = LD      $@
quite_cmd_wise      = LD      $@

#riscv32-elf-bfd doesn't support -gX option properly while generating linker script
cmd_cpp_lds = $(CPP) -Wp,-MD,$(depfile) $(patsubst -g%,,$(cpp_flags)) $(LDPPFLAGS) \
			  -D__ASSEMBLY -x assembler-with-cpp -P -o $@ $<

ifdef CONFIG_LINK_TO_ROM

LDFLAGS_wise += -T $(CONFIG_ROM_LDSCRIPT:"%"=%)

endif

ifdef CONFIG_BUILD_ROM

# Generate relocation table

quiet_cmd_wise__ ?= LD      $@
      cmd_wise__ ?= $(LD) $(LDFLAGS) $(LDFLAGS_wise) -q -o $@	\
		-T wise.lds $(wise-init) 				\
	   	--start-group --whole-archive $(wise-main) --no-whole-archive --end-group 			\
	   	--start-group $(PLATFORM_LIBS) --end-group -Map wise.map;	\
		$(OBJDUMP) -x $@ > $@.reloc;	\
		$(call SYSTEM_MAP,$@) > System.map;	\
		python3 ./scripts/check_rompatch.py;	\
		rm $@ $@.reloc System.map;	\
		$(LD) $(LDFLAGS) $(LDFLAGS_wise) -o $@	\
		-T wise.lds $(wise-init) 				\
	   	--start-group $(wise-main) --end-group 			\
	   	--start-group $(PLATFORM_LIBS) --end-group -Map wise.map

endif

ifdef CONFIG_WATCHER

#
# wise-watcher
# - Watcher
#
WISE_WATCHER_CMD = ./scripts/bin2char.sh < wise.bin > hal/soc/scm2010/scm2020-watcher.inc
quiet_cmd_wise_bin = OBJCOPY $@
      cmd_wise_bin = $(OBJCOPY) -O binary -S wise wise.bin; \
					 $(WISE_WATCHER_CMD)
endif

ifdef CONFIG_BOOTLOADER

#
# wise-boot
# - Legacy Bootloader
#
lma			= 0x$(shell grep "__init_lma" wise.sym | cut -d ' ' -f1,1)
vma_start 	= 0x$(shell grep "__vma_start" wise.sym | cut -d ' ' -f1,1)
ver			= 0x$(shell grep "fw_version" wise.sym | cut -d ' ' -f1,1)

ifdef CONFIG_SCMBOOT_SECURE
WISE_BOOT_BIN = $(CONFIG_BOOT_BIN:"%.bin"=%.sign.bin)
WISE_SIGN_CMD = ./scripts/sign.py $(WISE_BOOT_BIN)
else
WISE_BOOT_BIN = $(CONFIG_BOOT_BIN:"%"=%)
WISE_SIGN_CMD = :
endif

ifdef CONFIG_FLASH_ENCRYPTION
WISE_ENC_CMD = ./scripts/cipher.py $(lma) $(WISE_BOOT_BIN)
else
WISE_ENC_CMD = :
endif

quiet_cmd_wise_bin = MKIMAGE $(WISE_BOOT_BIN)
	  cmd_wise_bin = $(cmd_objcopy); \
			 hal/soc/$(SOC)/mkimage -f $@ \
			 -l $(lma) -v $(vma_start) -r $(ver); \
			 mv $@ $(WISE_BOOT_BIN); \
			 $(WISE_SIGN_CMD); \
			 $(WISE_ENC_CMD)
endif

ifdef CONFIG_MCUBOOT_BL

# wise-boot
# - MCUBoot Bootloader
#
lma			= 0x$(shell grep "__init_lma" wise.sym | cut -d ' ' -f1,1)
vma_start 	= 0x$(shell grep "__vma_start" wise.sym | cut -d ' ' -f1,1)
ver			= 0x$(shell grep "fw_version" wise.sym | cut -d ' ' -f1,1)

ifdef CONFIG_SCMBOOT_SECURE
WISE_BOOT_BIN = $(CONFIG_BOOT_BIN:"%.bin"=%.sign.bin)
WISE_SIGN_CMD = ./scripts/sign.py $(WISE_BOOT_BIN)
else
WISE_BOOT_BIN = $(CONFIG_BOOT_BIN:"%"=%)
WISE_SIGN_CMD = :
endif

ifdef CONFIG_FLASH_ENCRYPTION
WISE_ENC_CMD = ./scripts/cipher.py $(lma) $(WISE_BOOT_BIN)
else
WISE_ENC_CMD = :
endif

quiet_cmd_wise_bin = MKIMAGE $(WISE_BOOT_BIN)
	  cmd_wise_bin = $(cmd_objcopy); \
			 hal/soc/$(SOC)/mkimage -f $@ \
			 -l $(lma) -v $(vma_start) -r $(ver); \
			 mv $@ $(WISE_BOOT_BIN); \
			 $(WISE_SIGN_CMD); \
			 $(WISE_ENC_CMD)
endif

ifdef CONFIG_UART_BOOT

# wise.bin
# - For UART boot
#
lma			= 0x$(shell grep "__init_lma" wise.sym | cut -d ' ' -f1,1)
vma_start 	= 0x$(shell grep "__vma_start" wise.sym | cut -d ' ' -f1,1)
ver			= 0x$(shell grep "fw_version" wise.sym | cut -d ' ' -f1,1)

quiet_cmd_wise_bin = MKIMAGE $@
	  cmd_wise_bin = $(cmd_objcopy); \
			 hal/soc/$(SOC)/mkimage -f $@ \
			 -l $(lma) -v $(vma_start) -r $(ver)

endif

ifdef CONFIG_MCUBOOT_OTA

#
# wise-ota
#  - MCUBoot OTA format
#
ifdef CONFIG_MCUBOOT_OTA_SECURE
WISE_OTA_BIN = wise.mcuboot.sign.bin
WISE_OTA_SECURE = --key privkey.pem --public-key-format hash
else
WISE_OTA_BIN = wise.mcuboot.bin
WISE_OTA_SECURE =
endif

ifdef CONFIG_FLASH_ENCRYPTION
WISE_ENC_CMD = \
		./scripts/cipher.py $(CONFIG_SCM2010_OTA_PRIMARY_SLOT_OFFSET) $(WISE_OTA_BIN); \
		mv $(WISE_OTA_BIN:.bin=.enc.bin) $(WISE_OTA_BIN:.bin=.flash.enc.bin); \
		./scripts/cipher.py $(CONFIG_SCM2010_OTA_SECONDARY_SLOT_OFFSET) $(WISE_OTA_BIN); \
		mv $(WISE_OTA_BIN:.bin=.enc.bin) $(WISE_OTA_BIN:.bin=.ota.enc.bin)
else
WISE_ENC_CMD = :
endif

ifdef CONFIG_MFG
WISE_MFG_CMD = ./scripts/mfg.py $(WISE_OTA_BIN)
else
WISE_MFG_CMD = :
endif

quiet_cmd_wise_bin = MKIMAGE $(WISE_OTA_BIN)
      cmd_wise_bin = $(cmd_objcopy); \
				mv $@ $(WISE_OTA_BIN); \
				imgtool sign \
				--align 4 \
				--version $(CONFIG_IMAGE_VER_MAJOR).$(CONFIG_IMAGE_VER_MINOR) \
				--header-size 0x20 \
				--slot-size $(CONFIG_SCM2010_OTA_SLOT_SIZE) \
				--pad-header \
				$(WISE_OTA_SECURE) \
				$(WISE_OTA_BIN) $(WISE_OTA_BIN); \
				$(WISE_ENC_CMD); \
				$(WISE_MFG_CMD)
endif


ifdef CONFIG_MFG

#
# wise-mfg
#  - Manufacturing firmware
#
ifdef CONFIG_XIP
WISE_MFG_BIN = wise.mfg.xip.bin
quiet_cmd_wise_bin = MKIMAGE $(WISE_MFG_BIN)
      cmd_wise_bin = $(cmd_objcopy); \
				mv $@ $(WISE_MFG_BIN) -f; \
				imgtool sign \
				--align 4 \
				--version $(CONFIG_IMAGE_VER_MAJOR).$(CONFIG_IMAGE_VER_MINOR) \
				--header-size 0x20 \
				--slot-size $(CONFIG_SCM2010_OTA_SLOT_SIZE) \
				--pad-header \
				$(WISE_MFG_BIN) $(WISE_MFG_BIN); \
				./scripts/mfg.py $(WISE_MFG_BIN)
else
lma			= 0x$(shell grep "__init_lma" wise.sym | cut -d ' ' -f1,1)
vma_start 	= 0x$(shell grep "__vma_start" wise.sym | cut -d ' ' -f1,1)
WISE_MFG_BIN = wise.mfg.ram.bin
quiet_cmd_wise_bin = MKIMAGE $(WISE_MFG_BIN)
      cmd_wise_bin = $(cmd_objcopy); \
				mv $@ $(WISE_MFG_BIN); \
				hal/soc/$(SOC)/mkimage -f $(WISE_MFG_BIN) \
				-l $(lma) -v $(vma_start) -h
endif

endif


ifdef CONFIG_XIP

#
# wise-bin
#  - default for XIP if not defined elsewhere
#
lma			?= 0x$(shell grep "__init_lma" wise.sym | cut -d ' ' -f1,1)

ifdef CONFIG_FLASH_ENCRYPTION
WISE_ENC_CMD ?= ./scripts/cipher.py $(lma) $@
else
WISE_ENC_CMD ?= :
endif

quiet_cmd_wise_bin ?= MKIMAGE $@
      cmd_wise_bin ?= $(cmd_objcopy); \
				$(WISE_ENC_CMD)
endif

ifdef CONFIG_HOSTBOOT

ifdef CONFIG_MULTI_BINS

#
# wise.bin
#  - Binary image that can be loaded onto ILM of N22. This is bootable.
#
# wise.text
#  - Binary image that can be loaded onto AHB_SRAM.
#
# wise.etext
#  - Binary image that can be loaded onto ILM of D25.
#
# wise.data
#  - Binary image that can be loaded onto DLM of N22.
#

boot_lma	= 0x$(shell grep "__boot_lma" wise.sym | cut -d ' ' -f1,1)
boot_start 	= 0x$(shell grep "__boot_start" wise.sym | cut -d ' ' -f1,1)
boot_end	= 0x$(shell grep "__boot_end" wise.sym | cut -d ' ' -f1,1)
boot_size  	= $(shell echo $$(( $(boot_end) - $(boot_start) )))

text_lma 	= 0x$(shell grep "__text_lma" wise.sym | cut -d ' ' -f1,1)
text_start 	= 0x$(shell grep "__text_start" wise.sym | cut -d ' ' -f1,1)
text_end    = 0x$(shell grep "__text_end" wise.sym | cut -d ' ' -f1,1)
text_size   = 0x$(shell grep "__text_size" wise.sym | cut -d ' ' -f1,1)

etext_lma 	= 0x$(shell grep "__etext_lma" wise.sym | cut -d ' ' -f1,1)
etext_start = 0x$(shell grep "__etext_start" wise.sym | cut -d ' ' -f1,1)
etext_end   = 0x$(shell grep "__etext_end" wise.sym | cut -d ' ' -f1,1)
etext_size  = 0x$(shell grep "__etext_size" wise.sym | cut -d ' ' -f1,1)

data_lma 	= 0x$(shell grep "__rodata_lma" wise.sym | cut -d ' ' -f1,1)
data_start  = 0x$(shell grep "__rodata_start" wise.sym | cut -d ' ' -f1,1)
data_end    = 0x$(shell grep "__data_end" wise.sym | cut -d ' ' -f1,1)
data_size   = $(shell echo $$(( $(data_end) - $(data_start) )))

quiet_cmd_wise_bin = MKIMAGE $@
      cmd_wise_bin = $(OBJCOPY) -O binary -j .text.boot $< wise.bin;\
		     hal/soc/$(SOC)/mkimage -f wise.bin \
		     -l $(boot_lma) -v $(boot_start) -s $(boot_size) -h; \
	  		$(OBJCOPY) -O binary -j .text.ram1 $< wise.text;\
		     hal/soc/$(SOC)/mkimage -f wise.text \
		     -l $(text_lma) -v $(text_start) -s $(text_size) -h -t; \
	  		$(OBJCOPY) -O binary -j .text.ram2 $< wise.etext;\
		     hal/soc/$(SOC)/mkimage -f wise.etext \
		     -l $(etext_lma) -v $(etext_start) -s $(etext_size) -h -t; \
			 $(OBJCOPY) -O binary -j .rodata -j .data $< wise.data;\
		     hal/soc/$(SOC)/mkimage -f wise.data \
		     -l $(data_lma) -v $(data_start) -s $(data_size) -h -t; \
			tar cvzf wise.hosted.tar.gz wise.bin wise.text wise.etext wise.data

else

lma			= 0x$(shell grep "__init_lma" wise.sym | cut -d ' ' -f1,1)
vma_start 	= 0x$(shell grep "__vma_start" wise.sym | cut -d ' ' -f1,1)

quiet_cmd_wise_bin = MKIMAGE $@
      cmd_wise_bin = $(cmd_objcopy); \
		     hal/soc/$(SOC)/mkimage -f $@ \
		     -l $(lma) -v $(vma_start) -h

endif

endif

ifdef CONFIG_BOOTROM

#
# wise.rom
#  - Binary image that will be put into the Boot ROM.
#

quiet_cmd_wise_rom = GEN     $@
      cmd_wise_rom = $(cmd_objcopy); \
					 ./scripts/genrom.py $@

endif

ifdef CONFIG_ATE

#
# wise-ate
#  - ATE test binary
#
lma			= 0x$(shell grep "__init_lma" wise.sym | cut -d ' ' -f1,1)
vma_start 	= 0x$(shell grep "__vma_start" wise.sym | cut -d ' ' -f1,1)

quiet_cmd_wise_bin ?= MKIMAGE $(CONFIG_ATE_BIN:"%"=%)
      cmd_wise_bin ?= $(cmd_objcopy); \
		     $(srctree)/app/ate/mkimage -f $@ \
		     -l $(lma) -v $(vma_start); \
			 mv $@ $(CONFIG_ATE_BIN:"%"=%); \
			 $(srctree)/app/ate/jpat $(CONFIG_ATE_BIN:"%"=%)

endif

ifdef CONFIG_IRQ_DISPATCHER

#
# wise-irqd
#  - irq_dispatcher binary
#
quiet_cmd_wise_bin = MKIMAGE $(CONFIG_IRQD_BIN:"%"=%)
      cmd_wise_bin = $(cmd_objcopy); \
					 mv $@ $(CONFIG_IRQD_BIN:"%"=%)

endif

export CROSS_COMPILE
wise-sdk: all
	@if [ -e ./scripts/sdk_release_$(SDK_TARGET).sh ]; then \
		./scripts/sdk_release_$(SDK_TARGET).sh; \
	else \
		./scripts/sdk_release.sh; \
	fi
	@if [ -e ./scripts/sdk_custom.sh ]; then \
	    ./scripts/sdk_custom.sh; \
	fi

