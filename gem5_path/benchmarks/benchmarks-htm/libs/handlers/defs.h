#include <stdbool.h>
#include <stdint.h>

#define CACHE_LINE_SIZE_BYTES 64
#if ! defined(PAGE_SIZE_BYTES)
#define PAGE_SIZE_BYTES 4096
#endif

#if defined(ENABLE_M5_TRIGGER) || (ANNOTATE_CODE_REGIONS)
#include "include/gem5/m5ops.h"
#include "util/m5/src/m5_mmap.h"

#endif
