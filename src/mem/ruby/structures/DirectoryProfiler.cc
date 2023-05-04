#include "mem/ruby/structures/DirectoryProfiler.hh"

namespace gem5
{

namespace ruby
{

DirectoryProfiler::DirectoryProfiler(const Params &p)
    : ClockedObject(p), directoryProfilerStats(this),
    event([this]{profilePrecision();}, name()),
    delay(250000000)
{
    caches.resize(MachineType_base_count(MachineType_L2Cache));
    numCaches = 0;
}

DirectoryProfiler::
DirectoryProfilerStats::DirectoryProfilerStats(statistics::Group *parent)
    : statistics::Group(parent),
      ADD_STAT(jfcSharersPerLine, "Number of sharers per cache line"),
      ADD_STAT(jfcDirectoryUsage, "Percentage of directory usage"),
      ADD_STAT(jfcNumIterations, "Number of iterations if JFC stats")
{
    jfcNumIterations
        .flags(statistics::nozero);

    jfcSharersPerLine
        .init(8)
        .flags(statistics::pdf | statistics::dist | statistics::nonan);

    jfcDirectoryUsage
        .init(8)
        .flags(statistics::pdf | statistics::dist | statistics::nonan);
}

DirectoryProfiler::~DirectoryProfiler()
{
}

void
DirectoryProfiler::addCacheMemory(SimObject* cacheMemory)
{
    assert(MachineType_base_count(MachineType_L2Cache) > 0);
    assert(numCaches < MachineType_base_count(MachineType_L2Cache));
    caches.resize(MachineType_base_count(MachineType_L2Cache));
    caches[numCaches] = cacheMemory;
    numCaches++;
}

void
DirectoryProfiler::startup()
{
    assert(numCaches == MachineType_base_count(MachineType_L2Cache));
    schedule(&event, curTick() + delay);
}

void
DirectoryProfiler::profilePrecision()
{
    directoryProfilerStats.jfcNumIterations++;
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
    schedule(&event, curTick() + delay);
}

} // namespace ruby
} // namespace gem5