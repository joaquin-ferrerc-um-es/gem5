#ifndef __MEM_RUBY_PROFILER_DIRECTORYPROFILER_HH__
#define __MEM_RUBY_PROFILER_DIRECTORYPROFILER_HH__


#include "base/statistics.hh"
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
      void profilePrecision();
      void addCacheMemory(SimObject* cacheMemory);
      void startup();

    private:
      int numCaches;
      std::vector<SimObject*> caches;
      EventFunctionWrapper event;
      const Tick delay;

      struct DirectoryProfilerStats : public statistics::Group
      {
        DirectoryProfilerStats(statistics::Group *parent);

        statistics::Histogram jfcSharersPerLine;
        statistics::Histogram jfcDirectoryUsage;
        statistics::Scalar jfcNumIterations;
      } directoryProfilerStats;

};

} // namespace ruby
} // namespace gem5



#endif // __MEM_RUBY_PROFILER_DIRECTORYPROFILER_HH__