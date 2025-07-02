
VERSION 	= 2018
PATCHLEVEL 	= 02

MAKEFLAGS += -rR --include-dir=$(CURDIR)

# Beautify output
# ---------------------------------------------------------------------------
#
# Normally, we echo the whole command before executing it. By making
# that echo $($(quiet)$(cmd)), we now have the possibility to set
# $(quiet) to choose other forms of output instead, e.g.
#
#         quiet_cmd_cc_o_c = Compiling $(RELDIR)/$@
#         cmd_cc_o_c       = $(CC) $(c_flags) -c -o $@ $<
#
# If $(quiet) is empty, the whole command will be printed.
# If it is set to "quiet_", only the short version will be printed.
# If it is set to "silent_", nothing will be printed at all, since
# the variable $(silent_cmd_cc_o_c) doesn't exist.
#
# A simple variant is to prefix commands with $(Q) - that's useful
# for commands that shall be hidden in non-verbose mode.
#
#   $(Q)ln $@ :<
#
# If KBUILD_VERBOSE equals 0 then the above command will be hidden.
# If KBUILD_VERBOSE equals 1 then the above command is displayed.
#
# To put more focus on warnings, be less verbose as default
# Use 'make V=1' to see the full commands

ifeq ("$(origin V)", "command line")
  KBUILD_VERBOSE = $(V)
endif
ifndef KBUILD_VERBOSE
  KBUILD_VERBOSE = 0
endif

ifeq ($(KBUILD_VERBOSE),1)
  quiet =
  Q =
else
  quiet=quiet_
  Q = @
endif

export quiet Q KBUILD_VERBOSE

# kbuild supports saving output files in a separate directory.
# To locate output files in a separate directory two syntaxes are supported.
# In both cases the working directory must be the root of the kernel src.
# 1) O=
# Use "make O=dir/to/store/output/files/"
#
# 2) Set KBUILD_OUTPUT
# Set the environment variable KBUILD_OUTPUT to point to the directory
# where the output files shall be placed.
# export KBUILD_OUTPUT=dir/to/store/output/files/
# make
#
# The O= assignment takes precedence over the KBUILD_OUTPUT environment
# variable.

# KBUILD_SRC is set on invocation of make in OBJ directory
# KBUILD_SRC is not intended to be used by the regular user (for now)
ifeq ($(KBUILD_SRC),)

# OK, Make called in directory where kernel src resides
# Do we want to locate output files in a separate directory?
ifeq ("$(origin O)", "command line")
  KBUILD_OUTPUT := $(O)
endif

# That's our default target when none is given on the command line
PHONY := _all
_all:

# Cancel implicit rules on top Makefile
$(CURDIR)/Makefile Makefile: ;

ifneq ($(KBUILD_OUTPUT),)
# Invoke a second make in the output directory, passing relevant variables
# check that the output directory actually exists
saved-output := $(KBUILD_OUTPUT)
KBUILD_OUTPUT := $(shell mkdir -p $(KBUILD_OUTPUT) && cd $(KBUILD_OUTPUT) \
								&& /bin/pwd)
$(if $(KBUILD_OUTPUT),, \
     $(error failed to create output directory "$(saved-output)"))

PHONY += $(MAKECMDGOALS) sub-make

$(filter-out _all sub-make $(CURDIR)/Makefile, $(MAKECMDGOALS)) _all: sub-make
	@:

sub-make: FORCE
	$(Q)$(MAKE) -C $(KBUILD_OUTPUT) KBUILD_SRC=$(CURDIR) \
	-f $(CURDIR)/Makefile $(filter-out _all sub-make,$(MAKECMDGOALS))

# Leave processing to above invocation of make
skip-makefile := 1
endif # ifneq ($(KBUILD_OUTPUT),)
endif # ifeq ($(KBUILD_SRC),)

MAKEFLAGS += --no-print-directory

PHONY += all
_all: all

# Variables
srctree := .
objtree := .
src 	:= $(srctree)
obj 	:= $(objtree)

export srctree objtree

KCONFIG_CONFIG ?= .config
export KCONFIG_CONFIG

CONFIG_SHELL := $(shell if [ -x "$$BASH" ]; then echo $$BASH; \
		else if [ -x /bin/bash ]; then echo /bin/bash; \
		else echo sh; fi; fi)

HOSTCC 		= cc
HOSTCXX 	= c++
HOSTCFLAGS  = -Wall -Wstrict-prototypes -O2 -formit-frame-pointer
HOSTCFLAGS 	= -O2

PHONY := all

# To keep things going
KBUILD_BUILTIN := 1
export KBUILD_BUILTIN

#-include $(srctree)/.config
#-include $(srctree)/out/make.config

export KBUILD_MODULES KBUILD_BUILTIN
export KBUILD_CHECKSRC KBUILD_SRC KBUILD_EXTMOD

include scripts/Kbuild.include

ifdef XXXXX
# Make variables
CC     	 = $(CROSS_COMPILE)gcc
CPP      = $(CC) -E
CXX      = $(CROSS_COMPILE)g++
AS       = $(CROSS_COMPILE)as
LD       = $(CROSS_COMPILE)ld
AR       = $(CROSS_COMPILE)ar
NM       = $(CROSS_COMPILE)nm
LDR      = $(CROSS_COMPILE)ldr
STRIP  	 = $(CROSS_COMPILE)strip
OBJCOPY  = $(CROSS_COMPILE)objcopy
OBJDUMP  = $(CROSS_COMPILE)objdump
endif

KBUILD_CPPFLAGS := -D__WISE__
KBUILD_CFLAGS := $(CFLAGS) -x none -Wall -Werror
KBUILD_CFLAGS += -fno-common -fno-exceptions -ffunction-sections -fdata-sections
#KBUILD_CFLAGS += -fPIC

KBUILD_CXXFLAGS := --std=gnu++1y -Wno-literal-suffix -fpermissive -fno-rtti
KBUILD_CXXFLAGS += -Wno-narrowing

KBUILD_AFLAGS := -D__ASSEMBLY__

WISERELEASE = $(shell cat include/config/wise.release 2> /dev/null)
WISEVERSION = $(VERSION)$(if $(PATCHLEVEL),.$(PATCHLEVEL))$(if $(SUBLEVEL),.$(SUBLEVEL))

export CONFIG_SHELL
export CC CPP CXX AS LD AR NM LDR STRIP OBJCOPY OBJDUMP
export KBUILD_CFLAGS KBUILD_CXXFLAGS
export HOSTCC HOSTCXX HOSTCFLAGS HOSTLDFLAGS CROSS_COMPILER
export HOSTCXXFLAGS
export KBUILD_CPPFLAGS NOSTDINC_FLAGS WISEINCLUDE OBJCOPYFLAGS LDFLAGS
export KBUILD_CFLAGS KBUILD_AFLAGS

export RCS_FIND_IGNORE := \( -name SCCS -o -name BitKeeper -o -name .svn -o    \
			  -name CVS -o -name .pc -o -name .hg -o -name .git -o -name prebuilt \) \
			  -prune -o
export RCS_TAR_IGNORE := --exclude SCCS --exclude BitKeeper --exclude .svn \
			 --exclude CVS --exclude .pc --exclude .hg --exclude .git

# Rules shared between *config targets and build targets

# Basic helpers built in scripts/
PHONY += scripts_basic
scripts_basic:
	$(Q)$(MAKE) $(build)=scripts/basic
	$(Q)rm -f .tmp_quiet_recordmcount

scripts/basic/%: scripts_basic ;

timestamp_h := include/generated/timestamp_autogenerated.h

no-dot-config-targets := clean clobber distclean mrproper

config-targets := 0
dot-config := 1

ifneq ($(filter $(no-dot-config-targets), $(MAKECMDGOALS)),)
	ifeq ($(filter-out $(no-dot-config-targets), $(MAKECMDGOALS)),)
		dot-config := 0
	endif
endif

ifneq ($(filter config %config,$(MAKECMDGOALS)),)
	config-targets := 1
endif

ifeq ($(config-targets),1)
# *config targets only

KBUILD_DEFCONFIG := scm1010_defconfig
export KBUILD_DEFCONFIG KBUILD_KCONFIG

config: scripts_basic FORCE
	$(Q)$(MAKE) $(build)=scripts/kconfig $@

%config: scripts_basic FORCE
	$(Q)$(MAKE) $(build)=scripts/kconfig $@

else

#
# build targets (everything else except for *config)
#

PHONY += scripts
scripts: scripts_basic include/config/auto.conf
	$(Q)$(MAKE) $(build)=$(@)

ifeq ($(dot-config),1)

-include include/config/auto.conf
-include include/config/auto.conf.cmd

$(KCONFIG_CONFIG) include/config/auto.conf.cmd: ;

# FIXME: if .config is newer than include/config/auto.conf ...
include/config/%.conf: $(KCONFIG_CONFIG) include/config/auto.conf.cmd
	$(Q)$(MAKE) -f $(srctree)/Makefile silentoldconfig
	$(Q)$(MAKE) -f $(srctree)/scripts/Makefile.autoconf || \
		{ rm -f include/config/auto.conf; false; }
	$(Q)touch include/config/auto.conf

wise.cfg: include/config.h FORCE
	$(Q)$(MAKE) -f $(srctree)/scripts/Makefile.autoconf $(@)

-include include/autoconf.mk
-include include/autoconf.mk.dep

# We want to include arch/$(ARCH)/config.mk only when include/config/auto.conf
# is up-to-date. When we switch to a different board configuration, old CONFIG
# macros are still remaining in include/config/auto.conf. Without the following
# gimmick, wrong config.mk would be included leading nasty warnings/errors.
ifneq ($(wildcard $(KCONFIG_CONFIG)),)
ifneq ($(wildcard include/config/auto.conf),)
autoconf_is_old := $(shell find . -path ./$(KCONFIG_CONFIG) -newer \
						include/config/auto.conf)
ifeq ($(autoconf_is_old),)
include config.mk
include hal/arch/$(ARCH)/Makefile
-include hal/soc/$(SOC)/Makefile
endif
endif
endif

# Make variables
CC     	 = $(CROSS_COMPILE)gcc
CPP      = $(CC) -E
CXX      = $(CROSS_COMPILE)g++
AS       = $(CROSS_COMPILE)as
LD       = $(CROSS_COMPILE)ld
AR       = $(CROSS_COMPILE)ar
NM       = $(CROSS_COMPILE)nm
LDR      = $(CROSS_COMPILE)ldr
STRIP  	 = $(CROSS_COMPILE)strip
OBJCOPY  = $(CROSS_COMPILE)objcopy
OBJDUMP  = $(CROSS_COMPILE)objdump

# Remove double quotations
arch  := $(ARCH)
cpu   := $(CONFIG_SYS_CPU:"%"=%)
soc   := $(CONFIG_SYS_SOC:"%"=%)
board := $(CONFIG_SYS_BOARD:"%"=%)
osver := V$(CONFIG_FREERTOS_VERSION:"%"=%)

export arch cpu soc board osver

ifndef LDSCRIPT
	ifdef CONFIG_SYS_LDSCRIPT
		LDSCRIPT := $(srctree)/$(CONFIG_SYS_LDSCRIPT:"%"=%)
	endif
endif

ifndef LDSCRIPT
	ifeq ($(wildcard $(LDSCRIPT)),)
		LDSCRIPT := $(srctree)/hal/soc/$(soc)/wise.lds
	endif
	ifeq ($(wildcard $(LDSCRIPT)),)
		LDSCRIPT := $(srctree)/hal/board/$(board)/wise.lds
	endif
	ifeq ($(wildcard $(LDSCRIPT)),)
		LDSCRIPT := $(srctree)/hal/arch/$(ARCH)/cpu/$(cpu)/wise.lds
	endif
endif

else
# Dummy target needed
include/config/auto.conf: ;
endif # $(dot-config)

#KBUILD_CFLAGS += -Os
#KBUILD_CFLAGS += -g

# Where are these from? Command line?
KBUILD_CPPFLAGS += $(KCPPFLAGS)
KBUILD_AFLAGS += $(KAFLAGS)
KBUILD_CFLAGS += $(KCFLAGS)

WISEINCLUDE += -include $(srctree)/include/generated/autoconf.h
ifdef CONFIG_LINK_TO_ROM
WISEINCLUDE += -include $(srctree)/include/rom/fixedconf.h
endif
ifdef CONFIG_HOSTBOOT
WISEINCLUDE += -include $(srctree)/include/hostbootconf.h
endif
WISEINCLUDE += -Iinclude -Iinclude/cli -Iinclude/freebsd
WISEINCLUDE += -Ihal/board/$(board)/ -Ihal/soc/$(soc)/ -Ihal/soc/$(soc)/freertos -Ihal/soc/$(soc)/freertos/$(osver)


# Steve commendted this out
#NOSTDINC_FLAGS += -nostdinc -isystem $(shell $(CC) -print-file-name=include)
CHECKFLAGS += $(NOSTDINC_FLAGS)

cpp_flags := $(KBUILD_CPPFLAGS) $(PLATFORM_CPPFLAGS) $(WISEINCLUDE) $(NOSTDINC_FLAGS)
c_flags := $(KBUILD_CFLAGS) $(cpp_flags)

OBJCOPYFLAGS += -O binary

#
# WISE objects
#
ifneq ($(CONFIG_WATCHER),y)
ifneq ($(CONFIG_IRQ_DISPATCHER),y)
ifneq ($(CONFIG_PMM),y)
ifneq ($(CONFIG_ATE),y)
libs-y += kernel/
libs-y += api/
endif
endif
endif
endif
libs-y += app/

ifneq ($(CONFIG_WATCHER),y)
ifneq ($(CONFIG_IRQ_DISPATCHER),y)
ifneq ($(CONFIG_PMM),y)
ifneq ($(CONFIG_ATE),y)
include kernel/Makefile
include lib/Makefile
include hal/Makefile
#libs-y := $(sort $(libs-y))
endif
endif
endif
endif

wise-dirs := $(patsubst %/,%,$(filter %/, $(libs-y)))

wise-all-dirs := $(sort $(wise-dirs) $(patsubst %/,%,$(filter %/, $(libs-))))

libs-y :=$(foreach var, $(libs-y), $(var)$(patsubst %-,%,$(subst /,-,$(var))).a)

wise-main := $(libs-y)

export PLATFORM_LIBS
export PLATFORM_LIBGCC

#ALL-y += wise
ALL-y += wise.sym System.map wise.bin
ifdef CONFIG_BOOTROM
ALL-y += wise.rom
endif

quiet_cmd_objcopy = OBJCOPY $@
      cmd_objcopy = $(OBJCOPY) $(OBJCOPYFLAGS) $(OBJCOPY_FLAGS_$(@F)) $< $@

quiet_cmd_wise_bin ?= OBJCOPY $@
      cmd_wise_bin ?= $(cmd_objcopy)

wise.bin: wise wise.sym System.map FORCE
	$(call if_changed,wise_bin)

ifdef CONFIG_BOOTROM

quiet_cmd_wise_rom ?= OBJCOPY $@
      cmd_wise_rom ?= $(cmd_objcopy)

wise.rom: wise wise.sym System.map FORCE
	$(call if_changed,wise_rom)

endif

LDFLAGS_wise += $(PLATFORM_LDFLAGS)
LDFLAGS_wise += $(LDFLAGS_FINAL)
LDFLAGS_wise += --gc-sections
LDFLAGS_wise += $(call ld-option, --no-dynamic-linker)

cfg: wise.cfg

quiet_cmd_cfgcheck = CFGCK   $2
	  cmd_cfgcheck = $(srctree)/scripts/check-config.sh $2 \
					 $(srctree)/scripts/config_whitelist.txt $(srctree)

all: $(ALL-y) cfg
	$(call cmd,cfgcheck,wise.cfg)

quiet_cmd_wise__ ?= LD      $@
      cmd_wise__ ?= \
		if [ -s ./prebuilt ]; then cp -a $(wildcard ./prebuilt/*) . ; fi; \
		 $(LD) $(LDFLAGS) $(LDFLAGS_wise) -o $@ 		\
		-T wise.lds $(wise-init) 				\
	   	--start-group --whole-archive $(wise-main) --no-whole-archive --end-group 			\
	   	--start-group $(PLATFORM_LIBS) --end-group -Map wise.map \
		--print-memory-usage;	\
	    touch $(timestamp_h)

wise: $(wise-init) $(wise-main) wise.lds FORCE
	+$(call if_changed,wise__)

quiet_cmd_dis ?= DIS     $@
      cmd_dis ?= $(OBJDUMP) -S -d $< > $@

wise.dis: wise FORCE
	$(call if_changed,dis)


SECTION ?= "*"

quiet_cmd_ssz ?= SIZ     $@
      cmd_ssz ?= \
				 $(OBJCOPY) -j $(SECTION) $< $@.tmp; \
				 $(NM) -S --size-sort -r $@.tmp > $@; \
				 rm $@.tmp

wise.ssz: wise FORCE
	$(call if_changed,ssz)


quiet_cmd_sym ?= SYM     $@
      cmd_sym ?= $(OBJDUMP) -t $< > $@


wise.sym: wise FORCE
	$(call if_changed,sym)

$(sort $(wise-init) $(wise-main)): $(wise-dirs) ;

PHONY += $(wise-dirs)
$(wise-dirs): prepare scripts
	$(Q)$(MAKE) $(build)=$@

tools: prepare
$(filter-out tools, $(wise-dirs)): tools

define filechk_wise.release
	echo "$(WISEVERSION)$$($(CONFIG_SHELL) $(srctree)/scripts/setlocalversion $(srctree))"
endef

include/config/wise.release: include/config/auto.conf FORCE
	$(call filechk,wise.release)

PHONY += prepare archprepare prepare0 prepare1 prepare2 prepare3 freertos

prepare3: include/config/wise.release
ifneq ($(KBUILD_SRC),)
	@$(kecho) '  Using $(srctree) as source for WISE'
	$(Q)if [ -f $(srctree)/.config -o -d $(srctree)/include/config ]; then \
		echo >&2 "  $(srctree) is not clean, please run 'make mrproper'"; \
		echo >&2 "  in the $(srctree) directory."; \
		/bin/false; \
	fi;
endif

#
# Symbolic link FreeRTOS source and header files
#
#
freertos-ver-source-dir := $(srctree)/kernel/FreeRTOS/V$(patsubst "%",%,$(CONFIG_FREERTOS_VERSION))
freertos-src-core-dir := $(srctree)/kernel/FreeRTOS/core

$(freertos-src-core-dir): FORCE
	@[ -e $@ ] || ln -s $(abspath $(freertos-ver-source-dir)) $@

freertos-ver-header-dir := $(srctree)/kernel/FreeRTOS/V$(patsubst "%",%,$(CONFIG_FREERTOS_VERSION))/include
freertos-inc-dir := $(srctree)/include/FreeRTOS

$(freertos-inc-dir): FORCE
	@[ -e $@ ] || ln -s $(abspath $(freertos-ver-header-dir)) $@

freertos: $(freertos-inc-dir) $(freertos-src-core-dir) FORCE

kconfig2freertosopt_h := include/generated/kconfig2freertosopt.h
freertos_config := $(shell grep -w -e config -e menuconfig kernel/FreeRTOS/Kconfig | \
			grep -v '\#' | cut -d ' ' -f 2,2 | xargs)

define filechk_kconfig2freertosopt.h
	for kconf in $(freertos_config); do \
		case $${kconf} in \
		INCLUDE_*) \
			conf=$${kconf}; \
			;; \
		*) \
			conf=config$${kconf}; \
			;; \
		esac; \
	 	echo "#ifndef CONFIG_$${kconf}"; \
		echo "#define $${conf} 0" ; \
	 	echo "#else" ; \
	 	echo "#define $${conf} CONFIG_$${kconf}"; \
	 	echo "#endif"; \
	 	echo ""; \
	done
endef

$(kconfig2freertosopt_h): $(wildcard $(srctree)/kernel/FreeRTOS/Kconfig)
	$(call filechk,kconfig2freertosopt.h)

kconfig2lwipopt_h := include/generated/kconfig2lwipopt.h
lwip_config := $(shell grep -w config lib/lwip/Kconfig.* | grep -v '\#' | cut -d ' ' -f 2,2 | xargs)

define filechk_kconfig2lwipopt.h
	for kconf in $(lwip_config); do \
	 	echo "#ifndef CONFIG_$${kconf}"; \
		echo "#define $${kconf} 0" ; \
	 	echo "#else" ; \
	 	echo "#define $${kconf} CONFIG_$${kconf}"; \
	 	echo "#endif"; \
	 	echo ""; \
	done
endef

$(kconfig2lwipopt_h): $(wildcard $(srctree)/lib/lwip/Kconfig.*)
	$(call filechk,kconfig2lwipopt.h)

prepare2: prepare3 freertos


prepare1: prepare2 $(kconfig2lwipopt_h) \
	$(kconfig2freertosopt_h) include/config/auto.conf
ifeq ($(wildcard $(LDSCRIPT)),)
	@echo >&2 "  Could not find linker script."
	@/bin/false
endif

archprepare: prepare1 scripts_basic

prepare0: archprepare FORCE
	$(Q)$(MAKE) $(build)=.

# All the preparing..
prepare: prepare0

ifdef CONFIG_BOOTLOADER
WISE := WISE-BOOT
else
WISE := WISE
endif

define filechk_version.h
	(echo \#define PLAIN_VERSION \"$(WISERELEASE)\"; \
	echo \#define WISE_VERSION \"$(WISE) \" PLAIN_VERSION; \
	echo \#define CC_VERSION_STRING \"$$(LC_ALL=C $(CC) --version | head -n 1)\"; \
	echo \#define LD_VERSION_STRING \"$$(LC_ALL=C $(LD) --version | head -n 1)\";  \
	echo \#define GIT_VERSION_STRING \"$$(git rev-parse HEAD 2>&1)\"; \
	echo \#define GIT_API_VERSION_STRING \"$$(cd api && git rev-parse HEAD 2>&1)\"; )
endef

# The SOURCE_DATE_EPOCH mechanism requires a date that behaves like GNU date.
# The BSD date on the other hand behaves different and would produce errors
# with the misused '-d' switch.  Respect that and search a working date with
# well known pre- and suffixes for the GNU variant of date.
define filechk_timestamp.h
	(if test -n "$${SOURCE_DATE_EPOCH}"; then \
		SOURCE_DATE="@$${SOURCE_DATE_EPOCH}"; \
		DATE=""; \
		for date in gdate date.gnu date; do \
			$${date} -u -d "$${SOURCE_DATE}" >/dev/null 2>&1 && DATE="$${date}"; \
		done; \
		if test -n "$${DATE}"; then \
			LC_ALL=C $${DATE} -u -d "$${SOURCE_DATE}" +'#define WISE_DATE "%b %d %C%y"'; \
			LC_ALL=C $${DATE} -u -d "$${SOURCE_DATE}" +'#define WISE_TIME "%T"'; \
			LC_ALL=C $${DATE} -u -d "$${SOURCE_DATE}" +'#define WISE_TZ "%z"'; \
			LC_ALL=C $${DATE} -u -d "$${SOURCE_DATE}" +'#define WISE_DMI_DATE "%m/%d/%Y"'; \
			LC_ALL=C $${DATE} -u -d "$${SOURCE_DATE}" +'#define WISE_BUILD_DATE 0x%Y%m%d'; \
		else \
			return 42; \
		fi; \
	else \
		LC_ALL=C date +'#define WISE_DATE "%b %d %C%y"'; \
		LC_ALL=C date +'#define WISE_TIME "%T"'; \
		LC_ALL=C date +'#define WISE_TZ "%z"'; \
		LC_ALL=C date +'#define WISE_DMI_DATE "%m/%d/%Y"'; \
		LC_ALL=C date +'#define WISE_BUILD_DATE 0x%Y%m%d'; \
	fi)
endef

quiet_cmd_cpp_lds ?= LDS     $@
cmd_cpp_lds ?= $(CPP) -Wp,-MD,$(depfile) $(cpp_flags) $(LDPPFLAGS) \
			  -D__ASSEMBLY -x assembler-with-cpp -P -o $@ $<

wise.lds: $(LDSCRIPT) prepare FORCE
	$(call if_changed_dep,cpp_lds)

cscope:
	@rm -rf cscope.*
	@find . -name "*.[chS]" -exec readlink -f {} \; | sort | uniq > cscope.files
	@cscope -b -q -k

SYSTEM_MAP = \
	$(NM) $1 -S| \
	grep -v '\(compiled\)\|\(\.o$$\)\|\( [aUw] \)\|\(\.\.ng$$\)\|\(LASH[RL]DI\)' | \
	sort

System.map: wise
	@$(call SYSTEM_MAP,$<) > $@


PHONY += clean

###
# Cleaning is done on three levels.
# make clean     Delete most generated files
#                Leave enough to build external modules
# make mrproper  Delete the current configuration, and all generated files
# make distclean Remove editor backup files, patch leftover files and the like

# Directories & files removed with 'make clean'
CLEAN_DIRS  +=
CLEAN_FILES += wise* System.map

# Directories & files removed with 'make mrproper'
MRPROPER_DIRS  += include/config .tmp_objdiff
MRPROPER_FILES += .config .config.old include/autoconf.mk* include/config.h \
		  ctags etags tags TAGS cscope* GPATH GTAGS GRTAGS GSYMS
#MRPROPER_FILES += $(wildcard kernel/FreeRTOS/*.c)
#MRPROPER_FILES += $(wildcard include/FreeRTOS/*.h)

# clean - Delete most, but leave enough to build external modules
#
clean: rm-dirs  := $(CLEAN_DIRS)
clean: rm-files := $(CLEAN_FILES)

clean-dirs	:= $(foreach f,$(wise-alldirs),$(if $(wildcard $(srctree)/$f/Makefile),$f))

#clean-dirs      := $(addprefix _clean_, $(clean-dirs) doc/DocBook)

PHONY += $(clean-dirs) clean archclean
$(clean-dirs):
	$(Q)$(MAKE) $(clean)=$(patsubst _clean_%,%,$@)

# TODO: Do not use *.cfgtmp
clean: $(clean-dirs)
	$(call cmd,rmdirs)
	$(call cmd,rmfiles)
	@touch -m -d "1970-01-01T00:00:00Z" $(timestamp_h)
	@find . $(wise-dirs) $(RCS_FIND_IGNORE) \
		\( -name '*.[oas]' -o -name '*.ko' -o -name '.*.cmd' \
		-o -name '*.ko.*' -o -name '*.su' -o -name '*.cfgtmp' \
		-o -name '.*.d' -o -name '.*.tmp' -o -name '*.mod.c' \
		-o -name modules.builtin -o -name '.tmp_*.o.*' \
		-o -name '*.gcno' -o -name '*.gcov' -o -name '*.gcda' -o -name '*.xxd' \) -type f -print | xargs rm -f
	@find . $(wise-dirs) $(RCS_FIND_IGNORE) \
		-name '*.[ch]' -type l -print | xargs rm -f

# mrproper - Delete all generated files, including .config
#
mrproper: rm-dirs  := $(wildcard $(MRPROPER_DIRS))
mrproper: rm-files := $(wildcard $(MRPROPER_FILES))
mrproper-dirs      := $(addprefix _mrproper_,scripts)

PHONY += $(mrproper-dirs) mrproper archmrproper
$(mrproper-dirs):
	$(Q)$(MAKE) $(clean)=$(patsubst _mrproper_%,%,$@)

mrproper: clean $(mrproper-dirs)
	$(call cmd,rmdirs)
	$(call cmd,rmfiles)
	@rm -f arch/*/include/asm/arch
	@rm -rf $(freertos-src-core-dir)
	@rm -rf $(freertos-inc-dir)

# distclean
#
PHONY += distclean

distclean: mrproper
	@find $(srctree) $(RCS_FIND_IGNORE) \
		\( -name '*.orig' -o -name '*.rej' -o -name '*~' \
		-o -name '*.bak' -o -name '#*#' -o -name '.*.orig' \
		-o -name '.*.rej' -o -name '*%' -o -name 'core' \
		-o -name '*.pyc' -o -name '*.gcno' -o -name '*.gcov' -o -name '*.gcda' -o -name '*.xxd' \) \
		-type f -print | xargs rm -f
	@rm -f boards.cfg

backup:
	F=`basename $(srctree)` ; cd .. ; \
	gtar --force-local -zcvf `LC_ALL=C date "+$$F-%Y-%m-%d-%T.tar.gz"` $$F

help:
	@echo  'Cleaning targets:'
	@echo  '  clean		  - Remove most generated files but keep the config'
	@echo  '  mrproper	  - Remove all generated files + config + various backup files'
	@echo  '  distclean	  - mrproper + remove editor backup and patch files'
	@echo  ''
	@echo  'Configuration targets:'
	@$(MAKE) -f $(srctree)/scripts/kconfig/Makefile help
	@echo  ''
	@echo  'Other generic targets:'
	@echo  '  all		  - Build all necessary images depending on configuration'
	@echo  '  tests		  - Build U-Boot for sandbox and run tests'
	@echo  '* u-boot	  - Build the bare u-boot'
	@echo  '  dir/            - Build all files in dir and below'
	@echo  '  dir/file.[oisS] - Build specified target only'
	@echo  '  dir/file.lst    - Build specified mixed source/assembly target only'
	@echo  '                    (requires a recent binutils and recent build (System.map))'
	@echo  '  tags/ctags	  - Generate ctags file for editors'
	@echo  '  etags		  - Generate etags file for editors'
	@echo  '  cscope	  - Generate cscope index'
	@echo  '  ubootrelease	  - Output the release version string (use with make -s)'
	@echo  '  ubootversion	  - Output the version stored in Makefile (use with make -s)'
	@echo  "  cfg		  - Don't build, just create the .cfg files"
	@echo  "  envtools	  - Build only the target-side environment tools"
	@echo  ''
	@echo  'Static analysers'
	@echo  '  checkstack      - Generate a list of stack hogs'
	@echo  '  coccicheck      - Execute static code analysis with Coccinelle'
	@echo  ''
	@echo  'Documentation targets:'
	@$(MAKE) -f $(srctree)/doc/DocBook/Makefile dochelp
	@echo  ''
	@echo  '  make V=0|1 [targets] 0 => quiet build (default), 1 => verbose build'
	@echo  '  make V=2   [targets] 2 => give reason for rebuild of target'
	@echo  '  make O=dir [targets] Locate all output files in "dir", including .config'
	@echo  '  make C=1   [targets] Check all c source with $$CHECK (sparse by default)'
	@echo  '  make C=2   [targets] Force check of all c source with $$CHECK'
	@echo  '  make RECORDMCOUNT_WARN=1 [targets] Warn about ignored mcount sections'
	@echo  '  make W=n   [targets] Enable extra gcc checks, n=1,2,3 where'
	@echo  '		1: warnings which may be relevant and do not occur too often'
	@echo  '		2: warnings which occur quite often but may still be relevant'
	@echo  '		3: more obscure warnings, can most likely be ignored'
	@echo  '		Multiple levels can be combined with W=12 or W=123'
	@echo  ''
	@echo  'Execute "make" or "make all" to build all targets marked with [*] '
	@echo  'For further info see the ./README file'


endif # $(config-targets),1)

# FIXME Should go into a make.lib or something
# ===========================================================================

quiet_cmd_rmdirs = $(if $(wildcard $(rm-dirs)),CLEAN   $(wildcard $(rm-dirs)))
      cmd_rmdirs = rm -rf $(rm-dirs)

quiet_cmd_rmfiles = $(if $(wildcard $(rm-files)),CLEAN   $(wildcard $(rm-files)))
      cmd_rmfiles = rm -f $(rm-files)

# read all saved command lines

targets := $(wildcard $(sort $(targets)))
cmd_files := $(wildcard .*.cmd $(foreach f,$(targets),$(dir $(f)).$(notdir $(f)).cmd))

ifneq ($(cmd_files),)
  $(cmd_files): ;	# Do not try to update included dependency files
  include $(cmd_files)
endif

PHONY += FORCE
FORCE:

.PHONY: $(PHONY)
