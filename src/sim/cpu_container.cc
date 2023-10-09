#include "sim/cpu_container.hh"

namespace gem5
{

/**
 * Instantiate the objects to be manipulable from Python
 */

CPUContainer::CPUContainer(const Params &p) : SimObject(p) {
  m_cpu = p.cpu;
  has_l3 = p.hasl3;
}

} // namespace gem5
