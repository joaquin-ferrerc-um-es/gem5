#include "sim/cpu_container.hh"

namespace gem5
{

/**
 * Instantiate the objects to be manipulable from Python
 */

CPUContainer::CPUContainer(const Params &p) : SimObject(p) {
  m_cpu = p.cpu;
  has_l3 = p.hasl3;
  _store_hit_latency = p.store_hit_latency;
  _store_miss_latency = p.store_miss_latency;
  _load_access_latency = p.load_access_latency;
  _icache_access_latency = p.icache_access_latency;
}

} // namespace gem5
