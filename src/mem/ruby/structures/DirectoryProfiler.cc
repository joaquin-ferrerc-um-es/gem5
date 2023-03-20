#include "mem/ruby/structures/DirectoryProfiler.hh"

#include "sim/eventq.hh"

namespace gem5
{

namespace ruby
{

DirectoryProfiler::DirectoryProfiler()
{
    numCaches = 0;
    directoryProfilerStats(this);
}

DirectoryProfiler::
DirectoryProfilerStats::DirectoryProfilerStats(statistics::Group *parent)
    : statistics::Group(parent),
      ADD_STAT(jfcSharersPerLine, "Number of sharers per cache line"),
      ADD_STAT(jfcDirectoryUsage, "Percentage of directory usage")
{
    jfcSharersPerLine
        .init(8)
        .flags(statistics::pdf | statistics::dist | statistics::nozero |
            statistics::nonan);

    jfcDirectoryUsage
        .init(8)
        .flags(statistics::pdf | statistics::dist | statistics::nozero |
            statistics::nonan);
}

DirectoryProfiler::~DirectoryProfiler()
{
}

void
DirectoryProfiler::addCacheMemory(CacheMemory* cacheMemory)
{
    assert(numCaches < MachineType_base_count(MachineType_L1Cache));
    caches[numCaches] = cacheMemory;
    numCaches++;
    if (numCaches == MachineType_base_count(MachineType_L1Cache)) {
        eventq->schedule(, 100000);
    }
}

void
DirectoryProfiler::wakeup()
{
    double* stats = malloc(sizeof(double)*2);
    double nSPL = 0;
    double nC = 0;
    for (int i = 0; i < numCaches; i++) {
        caches[i]->getPrecisionStats(stats);
        nSPL += stats[0];
        nC += stats[1];
    }
    nSPL = nSPL/numCaches;
    nC = nC/numCaches;

    directoryProfilerStats.jfcSharersPerLine.sample(nSPL);
    directoryProfilerStats.jfcDirectoryUsage.sample(nC);
    free(stats);
    eventq->schedule(, 100000);
}

} // namespace ruby
} // namespace gem5