/*
 * Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
 * Universidad de Murcia
 *
 * Copyright (c) 2020 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MEM_HTM_HH__
#define __MEM_HTM_HH__

#include <map>
#include <string>

#include "params/HTM.hh"
#include "sim/clocked_object.hh"

namespace gem5
{

enum class HtmFailureFaultCause : int
{
    INVALID = -1,
    EXPLICIT,
    NEST,
    SIZE,
    EXCEPTION,
    INTERRUPT,
    DISABLED,
    MEMORY,
    /* LSQ: conflicting snoop seen by CPU for trans load not yet in
       Rset, caused either by remote requests or local replacements */
    LSQ,
    OTHER,
    /* Precise abort cause, set by xact mgr based on abortcause */
    SIZE_RSET,
    SIZE_WSET,
    SIZE_L1PRIV,
    SIZE_LLC,
    SIZE_WRONG_CACHE,
    EXPLICIT_FALLBACKLOCK,
    MEMORY_FALLBACKLOCK,
    MEMORY_STALEDATA,
    NUM_CAUSES
};

enum class HtmCacheFailure
{
    NO_FAIL,     // no failure in cache
    NO_FAIL_RETRY, // no failure in cache, must retry
    FAIL_SELF,   // failed due local cache's replacement policy
    FAIL_REMOTE, // failed due remote invalidation
    FAIL_OTHER,  // failed due other circumstances
};

/** Convert enum into string to be used for debug purposes */
std::string htmFailureToStr(HtmFailureFaultCause cause);

/** Convert enum into string to be used for debug purposes */
std::string htmFailureToStr(HtmCacheFailure rc);

class HtmPolicyStrings {
public:
  static const std::string requester_wins;
  static const std::string committer_wins;
  static const std::string requester_stalls;
  static const std::string magic;
  static const std::string requester_stalls_cda_base;
  static const std::string requester_stalls_cda_base_ntx;
  static const std::string requester_stalls_cda_hybrid;
};

class HTM : public ClockedObject
{
  public:
    const HTMParams &_params;
    Addr m_fallbackLockPhysicalAddress;
    Addr m_fallbackLockVirtualAddress;

    PARAMS(HTM);
    HTM(const Params &p);
    Addr getFallbackLockPAddr() { return m_fallbackLockPhysicalAddress; }
    void setFallbackLockPAddr(Addr addr) {
        if (m_fallbackLockPhysicalAddress == Addr(0)) {
            m_fallbackLockPhysicalAddress = addr;
        } else {
            assert(m_fallbackLockPhysicalAddress == addr);
        }
    }
    Addr getFallbackLockVAddr() { return m_fallbackLockVirtualAddress; }
    virtual void notifyPseudoInst() {};
    virtual void notifyPseudoInstWork(bool begin, int cpuId, uint64_t workid) {};
};

} // namespace gem5

#endif // __MEM_HTM_HH__
