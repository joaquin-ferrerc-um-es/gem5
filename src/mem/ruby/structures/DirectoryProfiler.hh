#ifndef __MEM_RUBY_PROFILER_DIRECTORYPROFILER_HH__
#define __MEM_RUBY_PROFILER_DIRECTORYPROFILER_HH__


#include "base/statistics.hh"
#include "mem/ruby/protocol/MachineType.hh"
#include "sim/clocked_object.hh"

namespace gem5
{

namespace ruby
{

class RubySystem;

class DirectoryProfiler : Named
{
    public:
      DirectoryProfiler(RubySystem* rs);
      ~DirectoryProfiler();
      void profilePrecision();
      void addCacheMemory(SimObject* cacheMemory);
      void startup();

    private:
      int numCaches;
      std::vector<SimObject*> caches;

      struct DirectoryProfilerStats : public statistics::Group, Named
      {
        DirectoryProfilerStats(statistics::Group *parent);

        statistics::Histogram SharersPerLine;
        statistics::Histogram DirectoryUsage;
      } directoryProfilerStats;

};

} // namespace ruby
} // namespace gem5



#endif // __MEM_RUBY_PROFILER_DIRECTORYPROFILER_HH__
