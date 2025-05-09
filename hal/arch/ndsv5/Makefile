CROSS_COMPILE := $(subst $\",,$(CONFIG_TOOLCHAIN_PATH))riscv32-elf-

# arch-y definitions

arch-y += -mcmodel=medany --specs=nosys.specs
arch-y += -Wall -fno-strict-aliasing -fno-builtin \
	-funroll-loops -ffunction-sections -fdata-sections

arch-$(CONFIG_SUPPORT_GCOV_ALL) += -fprofile-arcs -ftest-coverage

ifeq ($(CONFIG_BOOTROM),y)
arch-y += -fno-common
endif
arch-y += -march=$(subst $\",,$(CONFIG_MARCH_OPTION))
arch-y += -mabi=$(subst $\",,$(CONFIG_MABI_OPTION))
arch-y += -matomic
arch-y += -I$(srctree)/include/hal/ndsv5

ifeq ($(CONFIG_FRAME_POINTER),y)
  arch-y += -fno-omit-frame-pointer -fno-optimize-sibling-calls
else
  arch-y += -fomit-frame-pointer
endif

#arch-y += -malways-align -mstrict-align

PLATFORM_CPPFLAGS += $(arch-y)
PLATFORM_CPPFLAGS += $(subst $\",,$(CONFIG_OPTIM_OPTION))

ifneq ($(CONFIG_WATCHER), y)
PLATFORM_LDFLAGS += --no-relax
endif

CC = $(CROSS_COMPILE)gcc
ifeq ($(shell echo | $(CC) -E -dM - | grep __nds_execit > /dev/null && echo execit),execit)
ifeq ($(CONFIG_SUPPORT_EXECIT),y)
PLATFORM_CPPFLAGS += -mexecit
PLATFORM_LDFLAGS += --mexecit
else
PLATFORM_LDFLAGS += --mno-execit
endif
else
ifeq ($(CONFIG_SUPPORT_EXECIT),y)
$(error this toolchain do not support EXEC.IT optimization)
endif
endif

#ifneq ($(CONFIG_BOOTROM),y)
#PLATFORM_LIBS += -nostdlib
#else
PLATFORM_LIBS += -L $(shell dirname `$(CC) $(c_flags) -print-file-name=libnosys.a`) -lnosys
PLATFORM_LIBS += -L $(shell dirname `$(CC) $(c_flags) -print-file-name=libc.a`) -lc
PLATFORM_LIBS += -L $(shell dirname `$(CC) $(c_flags) -print-file-name=libm.a`) -lm
PLATFORM_LIBGCC := -L $(shell dirname `$(CC) $(c_flags) -print-libgcc-file-name`) -lgcc
PLATFORM_LIBS += $(PLATFORM_LIBGCC)
#endif
