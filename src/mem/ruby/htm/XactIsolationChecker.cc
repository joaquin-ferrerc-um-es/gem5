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
#include "mem/ruby/htm/XactIsolationChecker.hh"

#include <cassert>
#include <cstdlib>
#include <iostream>

#include "debug/RubyHTM.hh"
#include "mem/ruby/htm/TransactionInterfaceManager.hh"
#include "mem/ruby/system/RubySystem.hh"

namespace gem5
{
namespace ruby
{

XactIsolationChecker::XactIsolationChecker(RubySystem *rs) {
  m_ruby_system = rs;
  m_htm = rs->params().system->getHTM();
  int num_sequencers = rs->params().num_of_sequencers;
  m_num_sequencers = num_sequencers;
  m_readSet.resize(num_sequencers);
  m_writeSet.resize(num_sequencers);
  m_abortingProcessor.resize(num_sequencers);
  for (int i = 0; i < num_sequencers; i++){
    m_readSet[i].resize(1);
    m_writeSet[i].resize(1);
    m_abortingProcessor[i] = false;
  }
}

XactIsolationChecker::~XactIsolationChecker() {
}

bool XactIsolationChecker::existInReadSet(int proc, Addr addr, Tick &since){
  int numLevels = m_readSet[proc].size();
  bool found = false;

  for (int i = 0; i < numLevels; i++){
    if (m_readSet[proc][i].find(addr) != m_readSet[proc][i].end()) {
      found = true;
      since = m_readSet[proc][i][addr];
      break;
    }
  }
  return found;
}

 bool XactIsolationChecker::existInWriteSet(int proc, Addr addr, Tick &since){
  int numLevels = m_writeSet[proc].size();
  bool found = false;

  for (int i = 0; i < numLevels; i++){
    if (m_writeSet[proc][i].find(addr) != m_writeSet[proc][i].end()){
      found = true;
      since = m_writeSet[proc][i][addr];
      break;
    }
  }
  return found;
}

bool XactIsolationChecker::checkXACTIsolation(int proc, Addr addr, bool trans,
                                              RubyRequestType accessType){
   addr = makeLineAddress(addr);
  bool ok = true;

  for (int i = 0; i < m_num_sequencers; i++){
    if (i == proc) continue;
    // Processors that are in the process of aborting their
    // transactions.  It is ok to access the read/write sets belonging
    // to these transactions.
    Tick since;
    if (m_abortingProcessor[i]) continue;
    switch(accessType){
      case RubyRequestType_LD:
          if (existInWriteSet(i, addr, since)){
           if (trans) {
               DPRINTF(RubyHTM, "HTM: Isolation check failed addr %#x"
                      " read from proc %d in"
                      " write set of proc %d since %ld\n",
                      addr, proc, i, since);
               ok = false;
           } else { // It is ok for non-transactional loads to use
               // Data_Stale (inv seen while outstanding load
               // miss)
              DPRINTF(RubyHTM, "HTM: Non-transactional load"
                      " to addr %#x from proc %d can use Data_Stale,"
                      " now in write set of proc %d since %ld\n",
                      addr, proc, i, since);
           }
          }
          break;
      case RubyRequestType_ST:
      case RubyRequestType_ATOMIC:
          if (existInReadSet(i, addr, since)){
              DPRINTF(RubyHTM, "HTM: Isolation check failed addr %#x"
                      " write from proc %d in"
                      " read set of proc %d since %ld\n",
                      addr, proc, i, since);
             ok = false;
          }
          if (existInWriteSet(i, addr, since)){
              DPRINTF(RubyHTM, "HTM: Isolation check failed addr %#x"
                      " write from proc %d in"
                      " write set of proc %d since %ld\n",
                      addr, proc, i, since);
             ok = false;
          }
          break;
       default:
           break;
     }
  }
  return ok;
}

void XactIsolationChecker::addToReadSet(int proc, Addr addr){
  addToReadSet(proc, addr, 1);
}

void XactIsolationChecker::addToReadSet(int proc, Addr addr, int xact_level){
  assert(xact_level == 1);

  if (m_readSet[proc][xact_level - 1].find(addr) ==
      m_readSet[proc][xact_level - 1].end()) {
      m_readSet[proc][xact_level-1].
          insert(std::pair<Addr,Tick>(addr, curTick()));
  }

}

void XactIsolationChecker::addToWriteSet(int proc, Addr addr){
  addToWriteSet(proc, addr, 1);
}

void XactIsolationChecker::addToWriteSet(int proc, Addr addr, int xact_level){
  assert(xact_level == 1);

  if (m_writeSet[proc][xact_level - 1].find(addr) ==
      m_writeSet[proc][xact_level - 1].end()){
      m_writeSet[proc][xact_level-1].
          insert(std::pair<Addr,Tick>(addr, curTick()));
  }
}

void XactIsolationChecker::removeFromReadSet(int proc, Addr addr,
                                             int xact_level){
  assert(xact_level == 1);
  if (m_readSet[proc][xact_level-1].find(addr) !=
      m_readSet[proc][xact_level-1].end()){
    m_readSet[proc][xact_level-1].erase(addr);
  }
}

void XactIsolationChecker::removeFromWriteSet(int proc, Addr addr,
                                              int xact_level){
  assert(xact_level == 1);
  if (m_writeSet[proc][xact_level-1].find(addr) !=
      m_writeSet[proc][xact_level-1].end()){
    m_writeSet[proc][xact_level-1].erase(addr);
  }
}

void XactIsolationChecker::clearWriteSet(int proc, int xact_level){
    assert(xact_level == 1);
    m_writeSet[proc][xact_level-1].clear();
}

void XactIsolationChecker::clearReadSet(int proc, int xact_level){
    assert(xact_level == 1);
    m_readSet[proc][xact_level-1].clear();
}

void XactIsolationChecker::setAbortingProcessor(int proc) {
  m_abortingProcessor[proc] = true;
}

void XactIsolationChecker::clearAbortingProcessor(int proc) {
  m_abortingProcessor[proc] = false;
}

void XactIsolationChecker::printReadWriteSets(int proc) {
  std::cout << " PROCESSOR: " << proc << std::endl;
  std::cout << " READ SET: ";
  for (int i = 0; i < m_readSet[proc].size(); i++){
    std::cout << " LEVEL " << i << ": ";
    for (std::map<Addr,Tick>::iterator it =
           m_readSet[proc][i].begin();
         it!=m_readSet[proc][i-1].end();
         ++it) {
      Addr addr = it->first;
      std::cout << addr << " ";
    }
  }
  std::cout << std::endl;
  std::cout << " WRITE SET: ";
  for (int i = 0; i < m_writeSet[proc].size(); i++){
    std::cout << " LEVEL " << i << ": ";
    for (std::map<Addr,Tick>::iterator it =
           m_writeSet[proc][i].begin();
         it!=m_writeSet[proc][i-1].end();
         ++it) {
      Addr addr = it->first;
      std::cout << addr << " ";
    }
  }
  std::cout << std::endl;
}


} // namespace ruby
} // namespace gem5

