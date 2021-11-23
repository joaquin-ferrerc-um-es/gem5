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
ifdef AARCH64_CROSS_GCC_PREFIX
GCC_PREFIX := ${AARCH64_CROSS_GCC_PREFIX}
else
GCC_PREFIX := aarch64-linux-gnu-
endif
ifdef BENCHMARKS_AARCH64_TME_CROSS_HACK_GCC
CC10 := ${BENCHMARKS_AARCH64_TME_CROSS_HACK_GCC}
else
CC10 := /opt/cross/bin/aarch64-linux-gcc # Ruben's gcc-10.2.0
endif

else ifeq ($(ARCH),x86)

CFLAGS   += -DX86
GCC_PREFIX := ${X86_CROSS_GCC_PREFIX}

endif

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

# Enable m5 ops instrumentation for gem5
CFLAGS   += -DENABLE_M5OPS

# Pass memory regions in /proc/<pid>/maps to simulator
CFLAGS    += -DANNOTATE_PROC_MAPS

# Pass fallback lock address to simulator (clasify aborts)
CFLAGS    += -DANNOTATE_FALLBACKLOCK_ADDR

# Annotate code regions for more precise HTM visualizer
CFLAGS    += -DANNOTATE_CODE_REGIONS

OBJS= abort_handlers.o htm.o thread_context.o util.o env_globals.o m5iface.o

M5_INCLUDE := -I$(GEM5_ROOT) -I$(GEM5_ROOT)/include

CFLAGS   += -O3 -Wall -fgnu-tm -std=c11 -D_POSIX_C_SOURCE=200112L
CFLAGS   += $(M5_INCLUDE)
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


ifeq ($(ARCH),riscv)
M5_ARCH=riscv
else
ifeq ($(ARCH),aarch64)
M5_ARCH=arm64
else
ifeq ($(ARCH),x86) # TODO: why not x86_64 here?
M5_ARCH=x86
else
$(error M5_ARCH unknown)
endif
endif
endif

$(M5):
	cd $(GEM5_ROOT)/util/m5 && scons $(M5_ARCH).CROSS_COMPILE=${GCC_PREFIX} build/$(M5_ARCH)/out/m5
