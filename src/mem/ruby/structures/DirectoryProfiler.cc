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
      ADD_STAT(impreciseSharersPerLine, "Number of sharers per cache line"),
      ADD_STAT(impreciseDirectoryUsage, "Percentage of directory usage"),
      ADD_STAT(impreciseNumIterations, "Number of iterations if Imprecise stats")
{
    DPRINTF(DirectoryProfiler, "DirectoryProfilerStats created\n");

    impreciseNumIterations
        .flags(statistics::nozero);

    impreciseSharersPerLine
        .init(MachineType_base_count(MachineType_L1Cache) ? MachineType_base_count(MachineType_L1Cache)+1 : 257)
        //.init(65)
        .flags(statistics::pdf | statistics::dist | statistics::nonan);

    impreciseDirectoryUsage
        .init(10)
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
    // assert(numCaches == MachineType_base_count(MachineType_L2Cache));
}

void
DirectoryProfiler::profilePrecision()
{
    DPRINTF(DirectoryProfiler, "DirectoryProfiler profilePrecision called\n");
    directoryProfilerStats.impreciseNumIterations++;
    for (int i = 0; i < numCaches; i++) {
        caches[i]->getPrecisionStats(directoryProfilerStats.impreciseSharersPerLine, directoryProfilerStats.impreciseDirectoryUsage);
    }
}

} // namespace ruby
} // namespace gem5
