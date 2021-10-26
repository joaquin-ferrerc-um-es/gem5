/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#include "mem/ruby/common/Global.hh"
#include "mem/ruby/htm/htm.hh"
#include "mem/ruby/profiler/Profiler.hh"
#include "mem/ruby/profiler/XactProfiler.hh"

namespace gem5
{

namespace ruby
{

void
RubyHTM::notifyPseudoInst() {
    /* Per thread "state" cycles (non-txnal, aborted, etc.)  must
       be accumulated before pseudo instruction serviced,
       simulating a state change. This solves the problem of the
       missing cycles due to the stretch between the last state
       change and the m5_exit instruction.
    */
    assert(g_system_ptr);
    g_system_ptr->getProfiler()->
        getXactProfiler()->profileCurrentAnnotatedRegion();
}

void
RubyHTM::notifyPseudoInstWork(bool begin, int cpuId, uint64_t workid) {
    if (!g_system_ptr) return;
    if (AnnotatedRegion_isValidRegion(workid)) {
        AnnotatedRegion_t region = AnnotatedRegion_getRegion(workid);
        
        if (g_system_ptr->getProfiler()->hasXactProfiler()) {
            if (begin) {
                g_system_ptr->getProfiler()->getXactProfiler()->
                    beginRegion(cpuId, region);
            } else{
                g_system_ptr->getProfiler()->getXactProfiler()->
                    endRegion(cpuId, region);
            }
        }
    }
}

} // namespace ruby
} // namespace gem5
