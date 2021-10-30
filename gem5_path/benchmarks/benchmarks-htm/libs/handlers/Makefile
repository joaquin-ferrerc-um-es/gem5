#
# Makefile for the library
#

# gem5 hooks library
GEM5_ROOT = ../../../../..

ifndef ARCH
$(error ARCH not set)
else
$(info $$ARCH is [${ARCH}])
endif
# ARCH set via make ARCH=xxx when building this library as part of the
# build process of STAMP (see common/Defines.common.mk)

ifeq ($(ARCH),aarch64)

CFLAGS   += -DAARCH64
GCC_PREFIX := aarch64-linux-gnu-
CC10 := /opt/cross/bin/aarch64-linux-gcc # gcc-10 entirely built from source

else ifeq ($(ARCH),x86)

CFLAGS   += -DX86
GCC_PREFIX :=

endif

# These objects are built by running scons build/{arch}/out/m5 inside the gem5/util/m5 directory
#GEM5_OBJS := $(GEM5_ROOT)/util/m5/build/$(ARCH)/m5_mmap.o $(GEM5_ROOT)/util/m5/build/$(ARCH)/abi/$(ARCH)/m5op.o
#M5 := $(GEM5_ROOT)/build/$(ARCH)/out/m5


CC        := $(GCC_PREFIX)gcc
AR        := $(GCC_PREFIX)ar
LD        := $(GCC_PREFIX)gcc

AR_FLAGS  := rvs

# Choice of abort handler
CFLAGS    += $(HANDLER)
# Must be set to one of the following:
#-DHANDLER_EMPTY
#-DHANDLER_FALLBACKLOCK
#-DHANDLER_FALLBACKLOCK_2PHASE

# Pass memory regions in /proc/<pid>/maps to simulator
CFLAGS    += -DANNOTATE_PROC_MAPS

# Pass fallback lock address to simulator (clasify aborts)
CFLAGS    += -DANNOTATE_FALLBACKLOCK_ADDR

# Annotate code regions for more precise HTM visualizer
CFLAGS    += -DANNOTATE_CODE_REGIONS

OBJS= abort_handlers.o htm.o thread_context.o util.o env_globals.o

M5_INCLUDE := -I$(GEM5_ROOT) -I$(GEM5_ROOT)/include

CFLAGS   += -O3 -Wall -fgnu-tm -std=c11 -D_POSIX_C_SOURCE=200112L
CFLAGS   += $(M5_INCLUDE)
CFLAGS   += -DENABLE_M5_TRIGGER
CFLAGS   += -g

libaborthandlers.a: $(OBJS) $(M5)
	$(AR) $(AR_FLAGS) $@ $(OBJS) $(GEM5_OBJS)

ifeq ($(ARCH),aarch64)
# ACLE (TME intrinsics) requires gcc-10, but everything else is built
# using an older gcc version to avoid issues when running in  gem5
htm.o: htm.c
	$(CC10) $(CFLAGS) -march=armv8-a+tme -c $< -o $@
endif

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.a
#	cd $(GEM5_ROOT)/util/m5 && scons --clean

$(M5):
	cd $(GEM5_ROOT)/util/m5 && scons build/$(ARCH)/out/m5
