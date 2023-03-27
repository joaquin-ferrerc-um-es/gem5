#ifndef __MEM_RUBY_PROFILER_DIRECTORYPROFILER_HH__
#define __MEM_RUBY_PROFILER_DIRECTORYPROFILER_HH__


#include "base/statistics.hh"
// #include "mem/ruby/structures/DirectoryCacheMemory.hh"
#include "mem/ruby/protocol/MachineType.hh"
#include "sim/clocked_object.hh"

namespace gem5
{

namespace ruby
{

class DirectoryProfiler : public ClockedObject
{
    public:
      DirectoryProfiler(const Params &p);
      ~DirectoryProfiler();

      void wakeup();

      void addCacheMemory(SimObject* cacheMemory);

    private:
     int numCaches;
     std::vector<SimObject*> caches;

     struct DirectoryProfilerStats : public statistics::Group
     {
        DirectoryProfilerStats(statistics::Group *parent);

        statistics::Histogram jfcSharersPerLine;
        statistics::Histogram jfcDirectoryUsage;
     };

     DirectoryProfilerStats directoryProfilerStats;

};

} // namespace ruby
} // namespace gem5



#endif // __MEM_RUBY_PROFILER_DIRECTORYPROFILER_HH__