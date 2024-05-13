from m5.params import *
from m5.SimObject import SimObject
from m5.proxy import *

class CPUContainer(SimObject):
    type = 'CPUContainer'
    cxx_header = "sim/cpu_container.hh"
    cxx_class = 'gem5::CPUContainer'

    cpu = Param.BaseCPU(NULL, "Pointer to Contained BaseCPU")
    hasl3 = Param.Bool(False,"The processor has an L3")
