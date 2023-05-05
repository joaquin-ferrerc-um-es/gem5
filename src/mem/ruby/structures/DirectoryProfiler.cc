#include "mem/ruby/structures/DirectoryProfiler.hh"
#include "mem/ruby/system/RubySystem.hh"
#include "base/trace.hh"
#include "debug/DirectoryProfiler.hh"

namespace gem5
{

namespace ruby
{

DirectoryProfiler::DirectoryProfiler(RubySystem* rs)
  : Named("DirectoryProfilerStats"), directoryProfilerStats(rs)
{
    caches.resize(MachineType_base_count(MachineType_L2Cache));
    numCaches = 0;
    DPRINTF(DirectoryProfiler, "DirectoryProfiler created\n");
}

DirectoryProfiler::
DirectoryProfilerStats::DirectoryProfilerStats(statistics::Group *parent)
  : statistics::Group(parent), Named("DirectoryProfilerStats"),
      ADD_STAT(jfcSharersPerLine, "Number of sharers per cache line"),
      ADD_STAT(jfcDirectoryUsage, "Percentage of directory usage"),
      ADD_STAT(jfcNumIterations, "Number of iterations if JFC stats")
{
    DPRINTF(DirectoryProfiler, "DirectoryProfilerStats created\n");

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
    DPRINTF(DirectoryProfiler, "DirectoryProfiler added cache %s\n", cacheMemory->name());
    assert(MachineType_base_count(MachineType_L2Cache) > 0);
    assert(numCaches < MachineType_base_count(MachineType_L2Cache));
    caches.resize(MachineType_base_count(MachineType_L2Cache));
    caches[numCaches] = cacheMemory;
    numCaches++;
}

void
DirectoryProfiler::startup()
{
    DPRINTF(DirectoryProfiler, "DirectoryProfiler startup called\n");
    assert(numCaches == MachineType_base_count(MachineType_L2Cache));
}

void
DirectoryProfiler::profilePrecision()
{
    DPRINTF(DirectoryProfiler, "DirectoryProfiler profilePrecision called\n");
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
}

} // namespace ruby
} // namespace gem5
