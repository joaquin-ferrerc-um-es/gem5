#define _POSIX_C_SOURCE 200112L // pthread_barrier

#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "env_globals.h"
#include "util.h"

#define DEFAULT_HTM_MAX_RETRIES 8

bool parseBoolEnv(char *envVarName, bool *envVar) {
    // Sets envVar if "envVarName" defined and set to a valid int value

    char *envString = getenv(envVarName);
    if (envString != NULL) {
        char* end;
        long value = strtol(envString, &end, 10);

        if (!*end) {
            assert((value == 1) || (value == 0)); // boolean
            // Converted successfully
            *envVar = value;
            return true;
        }
        else {
            fprintf(stderr, "Invalid value for %s env var\n",
                    envVarName);
            exit(1);
        }
    }
    return false;
}

bool parseLongIntEnv(char *envVarName, long int *envVar) {
    // Sets envVar if "envVarName" defined and set to a valid int value

    char *envString = getenv(envVarName);
    if (envString != NULL) {
        char* end;
        long value = strtol(envString, &end, 10);

        if (!*end) {
            // Converted successfully
            assert(value > 0);
            *envVar = value;
            return true;
        }
        else {
            fprintf(stderr, "Ignoring invalid value for %s env var\n",
                    envVarName);
            exit(1);
        }
    }
    return false;
}

// This function is called from SimStartup (part of TM_STARTUP macro)
void setEnvGlobals(int numThreads) {
    assert(sizeof(_env_globals_t) == CACHE_LINE_SIZE_BYTES);

    assert((numThreads > 0) && (numThreads < 256)); // Sanity checks

    // Allocate mem and init barrier
    sh_globals.barrier =
        (pthread_barrier_t *)malloc(sizeof(pthread_barrier_t));
    int s = pthread_barrier_init(sh_globals.barrier, NULL, numThreads);
    assert(s == 0);

    // Set default values
    env.config.inSimulator = 0;
    env.config.numCPUs = sysconf(_SC_NPROCESSORS_ONLN);
    env.config.htm_max_retries = DEFAULT_HTM_MAX_RETRIES;

    assert(env.config.numCPUs >= 1);
    bool set = parseBoolEnv(ENV_VAR_IN_SIMULATOR,
                            &env.config.inSimulator);
    if (!set) {
        fprintf(stderr, "WARNING %s env var is unset!\n",
                ENV_VAR_IN_SIMULATOR);
    }
    long result = -1;
    set = parseLongIntEnv(ENV_VAR_HTM_MAX_RETRIES, &result);
    if (set) {
        assert(result >= 0 && result < UINT8_MAX);
        env.config.htm_max_retries = (uint8_t) result;
    }
    set = parseBoolEnv(ENV_VAR_HTM_HEAP_PREFAULT,
                            &env.config.heapPrefault);
    if (!set) {
        fprintf(stderr, "WARNING %s env var is unset!\n",
                ENV_VAR_HTM_HEAP_PREFAULT);
    }
}

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
    uint64_t bytesWritten =  m5_write_file_addr(buf, size, 0, out_filename);
    assert(bytesWritten == size);
}

void dumpValueToHostFileSystem(long value, const char *out_filename) {
    char buf[BUFF_SIZE];
    sprintf(buf,"%#lx\n", value);
    assert(strlen(out_filename) > 0);
    int size = strlen(buf);
    uint64_t bytesWritten =  m5_write_file_addr(buf, size, 0, out_filename);
    assert(bytesWritten == size);
}

#if defined(ANNOTATE_CODE_REGIONS)

#include "annotated_regions.h"

void annotateCodeRegionBegin(uint64_t codeRegionId) {
    uint64_t threadid = 0; // ignored
    uint64_t val =  AnnotatedRegion_regionToWorkId(codeRegionId);
    m5_work_begin_addr(val, threadid);
}

void annotateCodeRegionEnd(uint64_t codeRegionId) {
    uint64_t threadid = 0; // ignored
    uint64_t val =  AnnotatedRegion_regionToWorkId(codeRegionId);
    m5_work_end_addr(val, threadid);
}
#else

void annotateCodeRegionBegin(uint64_t codeRegionId) {}
void annotateCodeRegionEnd(uint64_t codeRegionId) {}

#endif
