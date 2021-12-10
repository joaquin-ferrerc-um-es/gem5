/*
 *
 * Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
 * Universidad de Murcia
 *
 * Copyright (c) 1999-2008 Mark D. Hill and David A. Wood
 * All rights reserved.
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


#ifndef __MEM_RUBY_HTM_XACTISOLATIONCHECKER_HH__
#define __MEM_RUBY_HTM_XACTISOLATIONCHECKER_HH__

#include <map>
#include <vector>

#include "mem/ruby/common/Address.hh"
#include "mem/ruby/slicc_interface/RubyRequest.hh"

namespace gem5
{
namespace ruby
{
class RubySystem;

class XactIsolationChecker
{
public:
  XactIsolationChecker(RubySystem *rs);
  ~XactIsolationChecker();

  bool checkXACTIsolation(int proc, Addr addr, RubyRequestType accessType);
  void addToReadSet(int proc, Addr addr, int xact_level);
  void addToReadSet(int proc, Addr addr);
  void addToWriteSet(int proc, Addr addr, int xact_level);
  void addToWriteSet(int proc, Addr addr);
  void clearReadSet(int proc, int xact_level);
  void clearWriteSet(int proc, int xact_level);
  void removeFromReadSet(int proc, Addr addr, int xact_level);
  void removeFromWriteSet(int proc, Addr addr, int xact_level);
  bool existInReadSet(int proc, Addr addr, Tick &since);
  bool existInWriteSet(int proc, Addr addr, Tick &since);
  void setAbortingProcessor(int proc);
  void clearAbortingProcessor(int proc);
  void printReadWriteSets(int proc);

private:
  RubySystem *m_ruby_system;
  HTM *m_htm;

  std::vector< std::vector< std::map<Addr, Tick> > > m_readSet;
  std::vector< std::vector< std::map<Addr, Tick> > > m_writeSet;
  std::vector<bool> m_abortingProcessor;
  int m_num_sequencers;
};


} // namespace ruby
} // namespace gem5

#endif

