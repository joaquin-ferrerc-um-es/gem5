#include "env_globals.h"
#include "m5iface.h"

#if ! defined(ENABLE_M5OPS)
void m5_init() {}
void simBeginRegionOfInterest() {}
void simEndRegionOfInterest() {}
void simWorkBegin() {}
void simWorkEnd() {}
void simBarrierBegin() {}
void simBarrierEnd() {}
void simCodeRegionBegin(unsigned long int codeRegionId) {}
void simCodeRegionEnd(unsigned long int codeRegionId) {}

#ifdef ANNOTATE_FALLBACKLOCK_ADDR 
#error "ANNOTATE_FALLBACKLOCK_ADDR requires ENABLE_M5OPS"
#endif

#ifdef ANNOTATE_PROC_MAPS
#error "ANNOTATE_PROC_MAPS requires ENABLE_M5OPS"
#endif

#ifdef ANNOTATE_CODE_REGIONS
#error "ANNOTATE_CODE_REGIONS requires ENABLE_M5OPS"
#endif

#else // # defined(ENABLE_M5OPS)

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "include/gem5/m5ops.h"
#include "util/m5/src/m5_mmap.h"

void dumpValueToHostFileSystem(long value, const char *out_filename);
void catProcMaps(const char* out_filename);


#ifdef ANNOTATE_FALLBACKLOCK_ADDR 
#include "spinlock.h"
#define FALLBACKLOCKADDR_FILENAME ("fallback_lock")
#endif

void
m5_init()
{
    if (inSimulator()) {
        map_m5_mem();
    } else {
        // When running in real hardware, point m5_mem to a region of
        // memory filled with zeros, no need to mmap /dev/mem
        m5_mem = calloc( 0x10000, sizeof(char));
    }

#ifdef ANNOTATE_FALLBACKLOCK_ADDR
    if (inSimulator()) {
        long fallBackLockAddr = (long)spinlock_getAddress();
        // Max fallback lock filename length is 63 characters
        char fallbackLockAddrFilename[CACHE_LINE_SIZE_BYTES]
            __attribute__ ((aligned (CACHE_LINE_SIZE_BYTES))) =
            FALLBACKLOCKADDR_FILENAME;
        dumpValueToHostFileSystem(fallBackLockAddr,
                                  fallbackLockAddrFilename);
    }
#endif

#ifdef ANNOTATE_PROC_MAPS
    if (inSimulator())
        catProcMaps("proc_maps");
#endif
}


// NOTE: Must keep HEXSPEAK arg values passed to m5_sum in sync with
// those in $(GEM5_ROOT)/src/sim/pseudo_inst.cc (sanity checks)
#define M5_SUM_HACK_TYPE_KVM_CKPT_SYNC 0xC0DE
#define M5_SUM_HACK_TYPE_REGION_BEGIN 0xBAAD
#define M5_SUM_HACK_TYPE_REGION_END   0xF00D

#define M5_SUM_HACK(type,code) m5_sum_addr(0xCAFE,0xBEEF, 0xDEAD, 0xBABE, type, code)

void simBeginRegionOfInterest() {
    // IMPORTANT NOTICE (support for fast-forward using KVM): See
    // $(GEM5_ROOT)/src/sim/System.py for documentation. Basically, we
    // need to ensure that we checkpoint the state of the guest
    // immediately after the execution of m5_work_begin (GOTO_SIM),
    // which is not always the case when running with KVM. To ensure
    // proper checkpointing when running in the simulator (we can tell
    // by looking at the return value of m5_sum_addr), we enter a
    // "dummy loop" that may cause the program to "spin" indefinitely,
    // depending on the value returned by the m5_sum for a specific
    // set of arguments (HEXSPEAK below). When running in real
    // hardware, we point m5_mem to a zero-filled memory region so
    // that all loads to that area are safe and simply return 0.  (see
    // init_m5_mem() in abort_handlers.c)

    // When running in gem5, the command-line option
    // 'checkpoint-m5sum-kvm-hack' will determine whether the return value
    // of m5_sum is tweaked (0) instead of the expected sum of arguments

    m5_work_begin_addr(0,0);
    if (m5_sum_addr(1,2,3,4,5,6)) { // i.e., inSimulator()
        while (M5_SUM_HACK(M5_SUM_HACK_TYPE_KVM_CKPT_SYNC, 0) == 0);
    }
    m5_reset_stats_addr(0,0);
}

void simEndRegionOfInterest() {
    m5_work_end_addr(0,0);
    m5_dump_stats_addr(0,0);
}

void simWorkBegin() {
    m5_work_begin_addr(0,0);
}
void simWorkEnd() {
    m5_work_end_addr(0,0);
}

#if defined(ANNOTATE_CODE_REGIONS)

#include "annotated_regions.h"

// The m5_sum op is reused for communication between benchmark and
// simulator, in this case to let the simulator now in which region of
// the abort the benchmark is at a given moment.
void simCodeRegionBegin(unsigned long int codeRegionId)
{
    M5_SUM_HACK(M5_SUM_HACK_TYPE_REGION_BEGIN, codeRegionId);
}
void simCodeRegionEnd(unsigned long int codeRegionId)
{
    M5_SUM_HACK(M5_SUM_HACK_TYPE_REGION_END, codeRegionId);
}

void simBarrierBegin()
{
    M5_SUM_HACK(M5_SUM_HACK_TYPE_REGION_BEGIN, AnnotatedRegion_BARRIER);
}
void simBarrierEnd()
{
    M5_SUM_HACK(M5_SUM_HACK_TYPE_REGION_END, AnnotatedRegion_BARRIER);
}

#else // # defined(ANNOTATE_CODE_REGIONS)


void annotateCodeRegionBegin(unsigned long int codeRegionId) {}
void annotateCodeRegionEnd(unsigned long int codeRegionId) {}

void annotateBarrierRegionBegin() {}
void annotateBarrierRegionEnd() {}

#endif // # defined(ANNOTATE_CODE_REGIONS)


#define BUFF_SIZE 16386

void catProcMaps(const char* out_filename) {
    char filename[32];
    sprintf(filename,"/proc/%d/maps",getpid());

    int fd = open(filename, 0);
    assert(fd >= 0);

    char buf[BUFF_SIZE];
    size_t bytesRead = 0;
    size_t size = 0;
    do {
        bytesRead = read(fd, &buf[size], BUFF_SIZE);
        size += bytesRead;
        assert(size < BUFF_SIZE);
    } while (bytesRead != 0);
    close(fd);
    assert(size > 0);
    int bytesWritten =  m5_write_file_addr(buf, size, 0, out_filename);
    assert(bytesWritten == size);
}

void dumpValueToHostFileSystem(long value, const char *out_filename) {
    char buf[BUFF_SIZE];
    sprintf(buf,"%#lx\n", value);
    assert(strlen(out_filename) > 0);
    int size = strlen(buf);
    int bytesWritten =  m5_write_file_addr(buf, size, 0, out_filename);
    assert(bytesWritten == size);
}

#endif // #if ! defined(ENABLE_M5OPS)
