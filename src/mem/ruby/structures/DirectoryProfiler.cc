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
      ADD_STAT(SharersPerLine, "Number of sharers per cache line"),
      ADD_STAT(DirectoryUsage, "Percentage of directory usage")
{
    DPRINTF(DirectoryProfiler, "DirectoryProfilerStats created\n");

    /*SharersPerLine
        .init(MachineType_base_count(MachineType_L1Cache) ? MachineType_base_count(MachineType_L1Cache)+1 : 257)
        .flags(statistics::pdf | statistics::dist | statistics::nonan);*/

    DirectoryUsage
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
    if (numCaches == MachineType_base_count(MachineType_L2Cache)) {
        directoryProfilerStats.SharersPerLine
        .init(MachineType_base_count(MachineType_L1Cache)+1)
        .flags(statistics::pdf | statistics::dist | statistics::nonan);
    }
}

void
DirectoryProfiler::startup()
{
    DPRINTF(DirectoryProfiler, "DirectoryProfiler startup called\n");
}

void
DirectoryProfiler::profilePrecision()
{
    DPRINTF(DirectoryProfiler, "DirectoryProfiler profilePrecision called\n");
    for (int i = 0; i < numCaches; i++) {
        caches[i]->getPrecisionStats(directoryProfilerStats.SharersPerLine, directoryProfilerStats.DirectoryUsage);
    }
}

} // namespace ruby
} // namespace gem5
