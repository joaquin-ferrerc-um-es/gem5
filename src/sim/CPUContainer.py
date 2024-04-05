from m5.params import *
from m5.SimObject import SimObject
from m5.proxy import *

class CPUContainer(SimObject):
    type = 'CPUContainer'
    cxx_header = "sim/cpu_container.hh"
    cxx_class = 'gem5::CPUContainer'

    cpu = Param.BaseCPU(NULL, "Pointer to Contained BaseCPU")
    hasl3 = Param.Bool(False,"The processor has an L3")

    store_hit_latency = Param.Cycles(1, "Default hit latency for stores")
    store_miss_latency = Param.Cycles(3, "Default miss latency (final latency will be this + hit latency)")

    load_access_latency = Param.Cycles(4, "Default hit latency for load requests")
    icache_access_latency = Param.Cycles(1, "Default hit latency for ifetch requests")
