from m5.params import *
from m5.SimObject import SimObject

class DirectoryProfiler(SimObject):
    type = 'DirectoryProfiler'
    cxx_class = 'gem5::ruby::DirectoryProfiler'
    cxx_header = 'mem/ruby/structures/DirectoryProfiler.hh'

    time_to_wait = Param.Latency("Time before firing the event")
    
