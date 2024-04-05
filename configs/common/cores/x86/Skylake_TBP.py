# Copyright (c) 2012 The Regents of The University of Michigan
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Authors: Ron Dreslinski

# HCP comments
# AF: information was obtained from the Agner Fog's work
# U <Undefined>: it is not documented
# R: obtained from http://www.realworldtech.com/haswell-cpu/
from m5.objects import *

# Simple ALU Instructions have a latency of 1
class Skylake_TBP_Simple_Int(FUDesc):
    opList = [ OpDesc(opClass='IntAlu', opLat=1) ]		        #AF
    count = 1								#AF

# Complex ALU instructions have a variable latencies
class Skylake_TBP_Combined(FUDesc):
    opList = [
               #Generic
               OpDesc(opClass='InstPrefetch', opLat=1),
               #Integer instructions
               OpDesc(opClass='IntAlu', opLat=1),	                #AF
               OpDesc(opClass='IntMult', opLat=4, pipelined=True),	#AF (integer vector multiplication: 5 // integer multiplication: 3)
               OpDesc(opClass='IntDiv', opLat=11, pipelined=True),		# divss
               OpDesc(opClass='IprAccess', opLat=3, pipelined=True), 	# ?
               #Floating point instructions
               #OpDesc(opClass='VectorNop', opLat=1, pipelined=True),
               OpDesc(opClass='FloatAdd', opLat=4, pipelined=True),
               OpDesc(opClass='FloatCmp', opLat=4, pipelined=True),
               OpDesc(opClass='FloatCvt', opLat=4, pipelined=True),
               OpDesc(opClass='FloatDiv', opLat=11, pipelined=True),
               OpDesc(opClass='FloatSqrt', opLat=12, pipelined=True),
               OpDesc(opClass='FloatMult', opLat=4, pipelined=True),
               OpDesc(opClass='FloatMultAcc', opLat=4, pipelined=True),
               OpDesc(opClass='FloatMisc', opLat=4, pipelined=True),
               #Vector instructions (SSE, AVX, AVX512, Generic vector length)
               OpDesc(opClass='SimdAdd', opLat=3, pipelined=True),
               OpDesc(opClass='SimdAddAcc', opLat=3, pipelined=True),
               OpDesc(opClass='SimdAlu', opLat=2, pipelined=True),
               OpDesc(opClass='SimdCmp', opLat=3, pipelined=True),
               OpDesc(opClass='SimdCvt', opLat=4, pipelined=True),
               OpDesc(opClass='SimdMisc', opLat=1, pipelined=True),
               OpDesc(opClass='SimdMult', opLat=4, pipelined=True),
               OpDesc(opClass='SimdMultAcc', opLat=4, pipelined=True),
               OpDesc(opClass='SimdDiv', opLat=18, pipelined=True),
               OpDesc(opClass='SimdShift', opLat=1, pipelined=True),
               OpDesc(opClass='SimdShiftAcc', opLat=1, pipelined=True),
               OpDesc(opClass='SimdSqrt', opLat=20, pipelined=True),
#               OpDesc(opClass='VectorIntReciprocal', opLat=7, pipelined=True),
               OpDesc(opClass='SimdReduceAdd', opLat=5, pipelined=True),
               OpDesc(opClass='SimdReduceAlu', opLat=5, pipelined=True),
               OpDesc(opClass='SimdReduceCmp', opLat=5, pipelined=True),
               OpDesc(opClass='SimdFloatAdd', opLat=4, pipelined=True),
               OpDesc(opClass='SimdFloatAlu', opLat=4, pipelined=True),
               OpDesc(opClass='SimdFloatCmp', opLat=4, pipelined=True),
               OpDesc(opClass='SimdFloatCvt', opLat=4, pipelined=True),
               OpDesc(opClass='SimdFloatMisc', opLat=1, pipelined=True),
               OpDesc(opClass='SimdFloatMult', opLat=5, pipelined=True),
               OpDesc(opClass='SimdFloatMultAcc', opLat=5, pipelined=True),
               OpDesc(opClass='SimdFloatDiv', opLat=18, pipelined=True),
               OpDesc(opClass='SimdFloatSqrt', opLat=20, pipelined=True),
#               OpDesc(opClass='VectorFloatReciprocal', opLat=7, pipelined=True),
               OpDesc(opClass='SimdFloatReduceAdd', opLat=5, pipelined=True),
               OpDesc(opClass='SimdFloatReduceCmp', opLat=5, pipelined=True),
               OpDesc(opClass='SimdPredAlu', opLat=5, pipelined=True)]
    count = 3

# Load/Store Units
# According to R, it is much more complex than this
# although it is not described like that here
class Skylake_TBP_Load(FUDesc):
    opList = [ OpDesc(opClass='MemRead',opLat=2),
               OpDesc(opClass='FloatMemRead',opLat=2)]
    count = 2								#R

class Skylake_TBP_Store(FUDesc):
    opList = [OpDesc(opClass='MemWrite',opLat=2),
              OpDesc(opClass='FloatMemWrite',opLat=2)]
    count = 1								#R

# Functional Units for this CPU
class Skylake_TBP_FUP(FUPool):
    FUList = [Skylake_TBP_Simple_Int(),
              Skylake_TBP_Combined(),
              Skylake_TBP_Load(),
              Skylake_TBP_Store()]

class Skylake_TBP_CPU(DerivO3CPU):
    LQEntries = 72							#R
    SQEntries = 56							#R
    LSQDepCheckShift = 0						#R
    LFSTSize = 1024							#R
    SSITSize = 1024							#R
    decodeToFetchDelay = 1						#U
    renameToFetchDelay = 1						#U
    iewToFetchDelay = 1							#U
    commitToFetchDelay = 1						#U
    renameToDecodeDelay = 1						#U
    iewToDecodeDelay = 1						#U
    commitToDecodeDelay = 1						#U
    iewToRenameDelay = 1   						#U
    commitToRenameDelay = 1						#U
    commitToIEWDelay = 1						#U
    fetchWidth = 4							#R
    fetchBufferSize = 16						#R
    fetchToDecodeDelay = 1						#U
    decodeWidth = 4							#R 4 uops
    decodeToRenameDelay = 1						#U
    renameWidth = 4							#R
    renameToIEWDelay = 1						#U
    issueToExecuteDelay = 1						#U
    dispatchWidth = 8							#R
    issueWidth = 8							#U
    wbWidth = 8								#U
    fuPool = Skylake_TBP_FUP()
    iewToCommitDelay = 1						#U
    renameToROBDelay = 1						#U
    commitWidth = 8							#U
    squashWidth = 224							#U
    trapLatency = 13							#U
    backComSize = 5							#U
    forwardComSize = 5							#U
    numPhysIntRegs = 180						#AF
    numPhysFloatRegs = 180						#AF
    numIQEntries = 128							#U
    numROBEntries = 224							#AF

    switched_out = False
    branchPred = TournamentBP() #TAGE_SC_L_64KB()

# TLB Cache
#Use a cache as a L2 TLB
class Skylake_TBP_PageTableWalkerCache(Cache):
    mshrs = 6								#U
    tgts_per_mshr = 8							#U
    size = '1kB'							#U
    assoc = 8								#U
    write_buffers = 16							#U
#    is_read_only = True
    # Writeback clean lines as well
    writeback_clean = True
    tag_latency = 2
    data_latency = 2
    response_latency = 2

# From now on is only for classic memory model. Ruby must be set in config/ruby/PROTOCOL_NAME and in the launch script

# Instruction Cache
class Skylake_TBP_ICache(Cache):
    mshrs = 4								#U
    tgts_per_mshr = 8							#U
#   These two are always overwritten by simulator flag --l2cache and --l3cache
#   So I'm commenting them here to prevent issues later
#   size = '32kB'
#   assoc = 8
    is_read_only = True
    writeback_clean = True
    tag_latency = 0
    data_latency = 1
    response_latency = 1
#   prefetcher = StridePrefetcher(degree=8, latency = 1)		#U

# Data Cache
class Skylake_TBP_DCache(Cache):
    mshrs = 16								#U
    tgts_per_mshr = 16							#U
#   These two are always overwritten by simulator flag --l2cache and --l3cache
#   So I'm commenting them here to prevent issues later
#   size = '32kB'							#AF
#   assoc = 8								#AF
    write_buffers = 16							#U
    writeback_clean = True
    tag_latency = 2
    data_latency = 2
    response_latency = 2
#   prefetcher = StridePrefetcher(degree=8, latency = 1)		#U

# L2 Cache
# Haswell has an L2 cache per core and a shared L3
class Skylake_TBP_L2Cache(Cache):
    mshrs = 32								#U
    tgts_per_mshr = 8							#U
#   These two are always overwritten by simulator flag --l2cache and --l3cache
#   So I'm commenting them here to prevent issues later
#   size = '1MB'							#AF
#   assoc = 16								#AF
    write_buffers = 8							#U
    prefetch_on_access = True						#AF
    tag_latency = 4
    data_latency = 10
    response_latency = 10
#   prefetcher = StridePrefetcher(degree=8, latency = 1)		#U

# L3 Cache
# Haswell has an L2 cache per core and a shared L3
class Skylake_TBP_L3Cache(Cache):
    mshrs = 20								#U
    tgts_per_mshr = 12							#U
#   These two are always overwritten by simulator flag --l2cache and --l3cache
#   So I'm commenting them here to prevent issues later
#   size = '16MB'							#AF
#   assoc = 11								#AF
    write_buffers = 8							#U
    prefetch_on_access = True						#AF
    clusivity = 'mostly_excl'
    tag_latency = 5
    data_latency = 45
    response_latency = 45
#   prefetcher = StridePrefetcher(degree=8, latency = 1)		#U