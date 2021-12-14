# gem5 hooks library
GEM5_ROOT = ../../../../..

ifndef ARCH
$(error ARCH not set)
endif

ifndef HANDLER
$(error HANDLER not set)
endif

$(info Building libaborthandlers: $$ARCH is [${ARCH}] and $$HANDLER is [${HANDLER}])

# Set ARCH dependent variables
ifeq ($(ARCH),aarch64)
M5_ARCH := arm64
CFLAGS += -DAARCH64
#CFLAGS += -march=armv8-a+tme # TODO: should be set for all files, currently only used by htm.c because old versions of GCC do not support it
GCC_PREFIX := ${AARCH64_CROSS_GCC_PREFIX}

# Hack to support building htm.c with a different version of
# GCC. Currently, we have tested that everything works with GCC 9.2.0,
# except htm.c which needs GCC 10.
# TODO: Remove this once we check that there is no problem building everything with a recent GCC.
$(info BENCHMARKS_AARCH64_TME_CROSS_HACK_GCC is $(BENCHMARKS_AARCH64_TME_CROSS_HACK_GCC))

ifndef BENCHMARKS_AARCH64_TME_CROSS_HACK_GCC
$(info All files will be compiled with $(GCC_PREFIX)gcc)
CFLAGS += -march=armv8-a+tme # TODO: check if this is really necessary with newer versions of GCC
else
$(info htm.c will be compiled with $(BENCHMARKS_AARCH64_TME_CROSS_HACK_GCC), the rest with $(GCC_PREFIX)gcc without TME support)
endif

else ifeq ($(ARCH),x86_64)
M5_ARCH := x86
CFLAGS += -DX86
#CFLAGS += -march=… -mtune=…
GCC_PREFIX := ${X86_CROSS_GCC_PREFIX}
else
$(error ARCH $(ARCH) not supported)
endif

CC := $(GCC_PREFIX)gcc
AR := $(GCC_PREFIX)ar
LD := $(GCC_PREFIX)gcc

# Set HANDLER dependent variables
ifeq ($(HANDLER),empty)
CFLAGS += -DHANDLER_EMPTY
else ifeq ($(HANDLER),fallbacklock)
CFLAGS += -DHANDLER_FALLBACKLOCK
else ifeq ($(HANDLER),fallbacklock2phase)
CFLAGS += -DHANDLER_FALLBACKLOCK -DHANDLER_FALLBACKLOCK_2PHASE
else ifeq ($(HANDLER),sgl)
CFLAGS += -DHANDLER_SGL
else
$(error HANDLER $(HANDLER) not supported)
endif

# Enable m5 ops instrumentation for gem5
CFLAGS += -DENABLE_M5OPS

# Pass memory regions in /proc/<pid>/maps to simulator
CFLAGS += -DANNOTATE_PROC_MAPS

# Pass fallback lock address to simulator (clasify aborts)
CFLAGS += -DANNOTATE_FALLBACKLOCK_ADDR

# Annotate code regions for more precise HTM visualizer
CFLAGS += -DANNOTATE_CODE_REGIONS

C_FILES := abort_handlers.c htm.c thread_context.c util.c env_globals.c m5iface.c spinlock.c logtm.c

M5_INCLUDE := -I$(GEM5_ROOT) -I$(GEM5_ROOT)/include
CFLAGS += $(M5_INCLUDE)

CFLAGS += -O3 -Wall -fgnu-tm -std=c11 -D_POSIX_C_SOURCE=200112L

CFLAGS += -g

libaborthandlers: build/$(ARCH)/libaborthandlers-$(HANDLER).a

libaborthandlers-$(HANDLER)_OBJS=$(foreach f,$(C_FILES:.c=-$(HANDLER).o),build/$(ARCH)/$(f))

build/$(ARCH)/libaborthandlers-$(HANDLER).a: $(libaborthandlers-$(HANDLER)_OBJS) | build/$(ARCH)/
	$(AR) rvs $@ $(libaborthandlers-$(HANDLER)_OBJS)

build/$(ARCH)/%-$(HANDLER).o: %.c *.h | build/$(ARCH)/
	$(CC) $(CFLAGS) -c $< -o $@

ifdef BENCHMARKS_AARCH64_TME_CROSS_HACK_GCC
ifeq ($(ARCH),aarch64)
# Hack to support building htm.c with a different version of
# GCC. Currently, we have tested that everything works with GCC 9.2.0,
# except htm.c which needs GCC 10.
# TODO: Remove this once we check that there is no problem building everything with a recent GCC.
build/$(ARCH)/htm-$(HANDLER).o: htm.c *.h | build/$(ARCH)/
	$(BENCHMARKS_AARCH64_TME_CROSS_HACK_GCC) $(CFLAGS) -march=armv8-a+tme -c $< -o $@
endif
endif

build/$(ARCH)/:
	mkdir -p $@

.PHONY: clean
clean:
	rm -f build/$(ARCH)/*-$(HANDLER).[ao]
	[ -e build/${ARCH} ] && rmdir --ignore-fail-on-non-empty build/${ARCH} || true
	[ -e build ] && rmdir --ignore-fail-on-non-empty build/ || true
