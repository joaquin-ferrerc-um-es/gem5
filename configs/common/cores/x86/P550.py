# Copyright (c) 2021 CAPS Group University of Murcia
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

from m5.objects import *

# Simple ALU Instructions have a latency of 1
class P550_Simple_Int(FUDesc):
    opList = [ OpDesc(opClass='IntAlu', opLat=1) ]
    count = 1

# Complex ALU instructions have a variable latencies
class P550_Combined(FUDesc):
    opList = [
               #Generic
               OpDesc(opClass='InstPrefetch', opLat=1),
               #Integer instructions
               OpDesc(opClass='IntAlu', opLat=1),
               OpDesc(opClass='IntMult', opLat=4, pipelined=True),	# (integer vector multiplication: 5 // integer multiplication: 3)
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
class P550_Load(FUDesc):
    opList = [ OpDesc(opClass='MemRead',opLat=2),
               OpDesc(opClass='FloatMemRead',opLat=2)]
    count = 1

class P550_Store(FUDesc):
    opList = [OpDesc(opClass='MemWrite',opLat=2),
              OpDesc(opClass='FloatMemWrite',opLat=2)]
    count = 1

# Functional Units for this CPU
class P550_FUP(FUPool):
    FUList = [P550_Simple_Int(),
              P550_Combined(),
              P550_Load(),
              P550_Store()]

class P550_CPU(DerivO3CPU):
    LQEntries = 16
    SQEntries = 16
    LSQDepCheckShift = 0
    LFSTSize = 1024
    SSITSize = 1024
    decodeToFetchDelay = 1
    renameToFetchDelay = 1
    iewToFetchDelay = 1
    commitToFetchDelay = 1
    renameToDecodeDelay = 1
    iewToDecodeDelay = 1
    commitToDecodeDelay = 1
    iewToRenameDelay = 1
    commitToRenameDelay = 1
    commitToIEWDelay = 1
    fetchWidth = 3
    fetchBufferSize = 16
    fetchToDecodeDelay = 1
    decodeWidth = 3
    decodeToRenameDelay = 1
    renameWidth = 3
    renameToIEWDelay = 1
    issueToExecuteDelay = 1
    dispatchWidth = 3
    issueWidth = 3
    wbWidth = 3
    fuPool = P550_FUP()
    iewToCommitDelay = 1
    renameToROBDelay = 1
    commitWidth = 3
    squashWidth = 352
    trapLatency = 13
    backComSize = 5
    forwardComSize = 5
    numPhysIntRegs = 128
    numPhysFloatRegs = 120
    numIQEntries = 160
    numROBEntries = 56

    switched_out = False
    branchPred = TAGE_SC_L_64KB()

# TLB Cache
#Use a cache as a L2 TLB
class P550_PageTableWalkerCache(Cache):
    mshrs = 6
    tgts_per_mshr = 8
    size = '1kB'
    assoc = 8
    write_buffers = 16
#    is_read_only = True
    # Writeback clean lines as well
    writeback_clean = True
    tag_latency = 2
    data_latency = 2
    response_latency = 2

# From now on is only for classic memory model. Ruby must be set in config/ruby/PROTOCOL_NAME and in the launch script

# Instruction Cache
class P550_ICache(Cache):
    mshrs = 4
    tgts_per_mshr = 8
#   These two are always overwritten by simulator flag --l2cache and --l3cache
#   So I'm commenting them here to prevent issues later
#   size = '32kB'
#   assoc = 8
    is_read_only = True
    writeback_clean = True
    tag_latency = 0
    data_latency = 1
    response_latency = 1
#   prefetcher = StridePrefetcher(degree=8, latency = 1)

# Data Cache
class P550_DCache(Cache):
    mshrs = 16
    tgts_per_mshr = 16
#   These two are always overwritten by simulator flag --l2cache and --l3cache
#   So I'm commenting them here to prevent issues later
#   size = '32kB'
#   assoc = 12
    write_buffers = 16
    writeback_clean = True
    tag_latency = 2
    data_latency = 2
    response_latency = 2
#   prefetcher = StridePrefetcher(degree=8, latency = 1)

# L2 Cache
# Haswell has an L2 cache per core and a shared L3
class P550_L2Cache(Cache):
    mshrs = 32
    tgts_per_mshr = 8
#   These two are always overwritten by simulator flag --l2cache and --l3cache
#   So I'm commenting them here to prevent issues later
#   size = '256KB'
#   assoc = 8
    write_buffers = 8
    prefetch_on_access = True
    tag_latency = 4
    data_latency = 10
    response_latency = 10
#   prefetcher = StridePrefetcher(degree=8, latency = 1)

# L3 Cache
# Haswell has an L2 cache per core and a shared L3
class P550_L3Cache(Cache):
    mshrs = 20
    tgts_per_mshr = 12
#   These two are always overwritten by simulator flag --l2cache and --l3cache
#   So I'm commenting them here to prevent issues later
#   size = '1MB' # Per Core
#   assoc = 16
    write_buffers = 8
    prefetch_on_access = True
    clusivity = 'mostly_excl'
    tag_latency = 5
    data_latency = 45
    response_latency = 45
#   prefetcher = StridePrefetcher(degree=8, latency = 1)