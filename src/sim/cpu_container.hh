#ifndef _CPU_CONTAINER_HH
#define _CPU_CONTAINER_HH

/*
   This class is necessary to provide a wrapper to SLICC/Ruby to access the CPU pointer.
   Since SLICC does not support access to pointers we cannot pass a pointer to the CPU
   and call the appropiate function calls, instead we pass this object to the
   cache controllers and use it as an interface to increase the statistics
 */

#include "sim/sim_object.hh"
#include "params/SimObject.hh"
#include "params/CPUContainer.hh"
#include "cpu/base.hh"

namespace gem5
{

class CPUContainer : public SimObject
{

public :

  typedef CPUContainerParams Params;
  CPUContainer(const Params &p);

  BaseCPU *m_cpu;
  bool has_l3;

  inline void l1MissesPending() { m_cpu->l1MissesPending(); };
  inline void l1NoMissesPending() { m_cpu->l1NoMissesPending(); };
  inline void l2MissesPending() { m_cpu->l2MissesPending(); };
  inline void l2NoMissesPending() { m_cpu->l2NoMissesPending(); };
  inline void anyMissesPending() { m_cpu->anyMissesPending(); };
  inline void anyNoMissesPending() { m_cpu->anyNoMissesPending(); };
  inline bool hasL3() { return has_l3; }

};

} // namespace gem5

#endif // _CPU_CONTAINER_HH
