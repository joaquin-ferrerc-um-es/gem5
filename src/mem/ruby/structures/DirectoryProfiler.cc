#include "mem/ruby/structures/DirectoryProfiler.hh"

#include "sim/eventq.hh"

namespace gem5
{

namespace ruby
{

DirectoryProfiler::DirectoryProfiler(const Params &p)
    : ClockedObject(p), directoryProfilerStats(this)
{
    caches.resize(MachineType_base_count(MachineType_L2Cache));
    numCaches = 0;
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
DirectoryProfiler::addCacheMemory(SimObject* cacheMemory)
{
    assert(numCaches < MachineType_base_count(MachineType_L2Cache));
    caches[numCaches] = cacheMemory;
    numCaches++;
    if (numCaches == MachineType_base_count(MachineType_L2Cache)) {
        eventq->schedule(nullptr, 100000);
    }
}

void
DirectoryProfiler::wakeup()
{
    std::vector<double> stats;
    stats.resize(2);
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
    eventq->schedule(nullptr, 100000);
}

} // namespace ruby
} // namespace gem5