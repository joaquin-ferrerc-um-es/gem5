#ifndef __MEM_RUBY_PROFILER_DIRECTORYPROFILER_HH__
#define __MEM_RUBY_PROFILER_DIRECTORYPROFILER_HH__


#include "base/statistics.hh"
#include "mem/ruby/structures/CacheMemory.hh"

namespace gem5
{

namespace ruby
{

class DirectoryProfiler : public ClockedObject
{
    public:
      DirectoryProfiler();
      ~DirectoryProfiler();

      void wakeup();

      void addCacheMemory(CacheMemory* cacheMemory);

    private:
     int numCaches;
     CacheMemory* caches[MachineType_base_count(MachineType_L1Cache)];

     struct DirectoryProfilerStats : public statistics::Group
     {
        DirectoryProfilerStats(statistics::Group *parent);

        statistics::Histogram jfcSharersPerLine;
        statistics::Histogram jfcDirectoryUsage;
     } directoryProfilerStats;

};

} // namespace ruby
} // namespace gem5



#endif // __MEM_RUBY_PROFILER_DIRECTORYPROFILER_HH__