/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#include <cassert>
#include <cstdlib>
#include <iostream>

#include "debug/RubyHTM.hh"
#include "mem/ruby/htm/TransactionConflictManager.hh"
#include "mem/ruby/htm/TransactionInterfaceManager.hh"
#include "mem/ruby/htm/TransactionIsolationManager.hh"

//#include "XactIsolationChecker.h"

namespace gem5
{
namespace ruby
{


using namespace std;

#define XACT_MGR m_xact_mgr

#define PERFECT_FILTER true
#define READ_WRITE_FILTER "Perfect_"
#define EXECUTED_LOAD ('x')
#define RETIRED_LOAD ('r')
#define OVERTAKING_LOAD ('o')
#define RETIRED_STORE ('w')

TransactionIsolationManager::
TransactionIsolationManager(TransactionInterfaceManager *xact_mgr,
                            int version) {

  m_version = version;
  m_xact_mgr = xact_mgr;

  int smt_threads = XACT_MGR->numberofSMTThreads();

  m_readSet.resize(smt_threads);
  m_writeSet.resize(smt_threads);
  m_readSetFilter.resize(smt_threads);
  m_writeSetFilter.resize(smt_threads);

  m_xact_readCount.resize(smt_threads);
  m_xact_writeCount.resize(smt_threads);
  m_xact_overflow_readCount.resize(smt_threads);
  m_xact_overflow_writeCount.resize(smt_threads);

  /* For SMT conflicts */
  int p;
  for (p=0; p < smt_threads; ++p){
    m_readSet[p].resize(1);
    m_writeSet[p].resize(1);

    if (!PERFECT_FILTER) {
        panic("Isolation manager: Bloom filters not tested!");

#if 0
      m_readSetFilter[p] = new BulkBloomFilter();
      m_writeSetFilter[p] = new BulkBloomFilter();
#endif
    }
    m_xact_readCount[p].resize(1);
    m_xact_writeCount[p].resize(1);
    m_xact_overflow_readCount[p].resize(1);
    m_xact_overflow_writeCount[p].resize(1);
  }
}

TransactionIsolationManager::~TransactionIsolationManager() {
}

void TransactionIsolationManager::setVersion(int version) {
  m_version = version;
}

int TransactionIsolationManager::getVersion() const {
  return m_version;
}

int TransactionIsolationManager::getProcID() const{
  return m_xact_mgr->getProcID();
}

int TransactionIsolationManager::getLogicalProcID(int thread) const{
  return getProcID() * m_xact_mgr->numberofSMTThreads() + thread;
}

void TransactionIsolationManager::beginTransaction(int thread){
  int xact_level = XACT_MGR->getTransactionLevel(thread);
  assert(xact_level == 1);
  if (xact_level > m_readSet[thread].size()){
    m_readSet[thread].resize(xact_level);
  }
  if (!m_readSet[thread][xact_level - 1].empty()) {
      // Do not clear Rset on beginTransaction since in O3CPU trans
      // loads may overtake the xbegin.
      DPRINTF(RubyHTM,"HTM: PROC %d beginTransaction finds"
              " non-empty read set\n", getProcID());
  }
  //  m_readSet[thread][xact_level - 1].clear();
  if (xact_level > m_writeSet[thread].size()){
    m_writeSet[thread].resize(xact_level);
  }
  m_writeSet[thread][xact_level - 1].clear();
}

void TransactionIsolationManager::commitTransaction(int thread){
  int old_xact_level = XACT_MGR->getTransactionLevel(thread);
  assert(old_xact_level >= 1);
  int new_xact_level = old_xact_level - 1;

  vector<Addr> readSet;
  for ( map<Addr, char>::iterator ii =
            m_readSet[thread][old_xact_level-1].begin();
        ii!=m_readSet[thread][old_xact_level-1].end(); ++ii) {
      Addr key=(*ii).first;
      if (XACT_MGR->config_preciseReadSetTracking()) {
          assert(ii->second == RETIRED_LOAD);
      } else if (ii->second != RETIRED_LOAD) {
          // TODO: Executed load never retired: profile?
          DPRINTF(RubyHTM,
                  "HTM: PROC %d commitTransaction finds "
                  "non-retired read set block addr %x (%c)\n",
                  getProcID(), key, ii->second);
      }
      readSet.push_back(key);
  }

  vector<Addr> writeSet;
  for ( map<Addr, char>::iterator ii =
            m_writeSet[thread][old_xact_level-1].begin();
        ii!=m_writeSet[thread][old_xact_level-1].end(); ++ii) {
      Addr key=(*ii).first;
      writeSet.push_back(key);
  }

  if (new_xact_level > 0){
      for (int i = 0; i < readSet.size(); i++)
          addToReadSetPerfectFilter(thread, readSet[i], new_xact_level);
      for (int i = 0; i < writeSet.size(); i++)
          addToWriteSetPerfectFilter(thread, writeSet[i], new_xact_level);
  }

  clearReadSetPerfectFilter(thread, old_xact_level);
  clearWriteSetPerfectFilter(thread, old_xact_level);

  setFiltersToXactLevel(thread, new_xact_level, old_xact_level);
}

void TransactionIsolationManager::abortTransaction(int thread,
                                                   int new_xact_level){
  int old_xact_level = XACT_MGR->getTransactionLevel(thread);
  assert(old_xact_level >= 1);
  assert(new_xact_level < old_xact_level);
  assert(new_xact_level >= 0);

  if (new_xact_level == 0){
    // profile signatures (for top-level abort)
    int xid = XACT_MGR->getXID(thread);
    profileReadSetFilterActivity(xid, thread, false);
    profileWriteSetFilterActivity(xid, thread, false);
  }

  for (int i = new_xact_level; i < old_xact_level; i++){
    clearReadSetPerfectFilter(thread, i + 1);
    clearWriteSetPerfectFilter(thread, i + 1);
  }

  setFiltersToXactLevel(thread, new_xact_level, old_xact_level);
}

void TransactionIsolationManager::releaseIsolation(int thread, int xact_level){
  assert(xact_level >= 0);

  if (xact_level-1 == 0){
    // profile signatures (for top-level abort)
    //int xid = XACT_MGR->getXID(thread);
    //profileReadSetFilterActivity(xid, thread, false);
    //profileWriteSetFilterActivity(xid, thread, false);
  }

  clearReadSetPerfectFilter(thread, xact_level);
  clearWriteSetPerfectFilter(thread, xact_level);

  setFiltersToXactLevel(thread, xact_level - 1, xact_level);
}

void TransactionIsolationManager::releaseReadIsolation(int thread){

  int levels = XACT_MGR->getTransactionLevel(thread);
  for (int i = 0; i < levels; i++){
    clearReadSetPerfectFilter(thread, i + 1);
  }

  if (!PERFECT_FILTER){
    m_readSetFilter[thread]->clear();
  }

  DPRINTF(RubyHTM,"HTM: PROC %d releaseReadIsolation (abort) \n",
          getProcID());

}

bool
TransactionIsolationManager::isInReadSetPerfectFilter(int thread,
                                                      Addr addr) {
  int transactionLevel = 1;
  assert(thread >= 0);

  map<Addr, char>::iterator it =
      m_readSet[thread][transactionLevel-1].find(makeLineAddress(addr));
  if (it != m_readSet[thread][transactionLevel-1].end()) {
      if (XACT_MGR->getTransactionLevel(thread) == 0) {
          // Trans loads cannot retire before htm_start instruction
          assert(it->second != RETIRED_LOAD);
      }
      if (XACT_MGR->config_preciseReadSetTracking()) {
          assert(it->second == RETIRED_LOAD);
      }
      return true;
  } else {
      return false;
  }
}

bool
TransactionIsolationManager::isInWriteSetPerfectFilter(int thread,
                                                       Addr addr){
  int transactionLevel = 1;
  assert(thread >= 0);
  map<Addr, char>::iterator it =
      m_writeSet[thread][transactionLevel-1].find(makeLineAddress(addr));
  if (it != m_writeSet[thread][transactionLevel-1].end()) {
      assert(it->second == RETIRED_STORE);
      return true;
  } else {
      return false;
  }
}

void
TransactionIsolationManager::addToReadSetPerfectFilter(int thread,
                                                       Addr address,
                                                       int transactionLevel){
  assert(thread >= 0);
  assert(transactionLevel > 0);
  assert(transactionLevel <= m_readSet[thread].size());
  Addr addr = makeLineAddress(address);
  char value = EXECUTED_LOAD;
  if (XACT_MGR->getTransactionLevel(thread) == 0) {
      // Sanity checks: distinguish read-set blocks that have been
      // isolated before xact mgr::beginTransaction called
      value = OVERTAKING_LOAD; // (o)vertaking load
  }

  if (m_readSet[thread][transactionLevel - 1].find(addr) ==
      m_readSet[thread][transactionLevel - 1].end())
    m_readSet[thread][transactionLevel-1].
      insert(std::pair<Addr,char>(addr, value));

  assert(m_readSet[thread][transactionLevel-1].find(addr) !=
         m_readSet[thread][transactionLevel-1].end());

}

void
TransactionIsolationManager::addToRetiredReadSet(int thread, Addr addr)
{
    int transactionLevel = 1;
    assert(XACT_MGR->getTransactionLevel(thread) == 1);
    map<Addr, char>::iterator it =
        m_readSet[thread][transactionLevel-1].find(makeLineAddress(addr));
    assert(it != m_readSet[thread][transactionLevel-1].end());
    if (it->second == OVERTAKING_LOAD) {
          // TODO: Executed load never retired: profile?
        DPRINTF(RubyHTM,
                "HTM: PROC %d addToRetiredReadSet finds "
                "overtaking load to block addr %x\n",
                getProcID(), addr);
    } else {
        assert(it->second == EXECUTED_LOAD);
    }
    it->second = RETIRED_LOAD;
}

bool
TransactionIsolationManager::inRetiredReadSet(int thread, Addr addr) {
  int transactionLevel = 1;
  map<Addr, char>::iterator it =
      m_readSet[thread][transactionLevel-1].find(makeLineAddress(addr));
  assert(it != m_readSet[thread][transactionLevel-1].end());
  // Trans loads cannot retire before htm_start instruction
  return (it->second == RETIRED_LOAD);
}

bool
TransactionIsolationManager::wasOvertakingRead(int thread, Addr addr) {
    int transactionLevel = 1;
    map<Addr, char>::iterator it =
        m_readSet[thread][transactionLevel - 1].find(addr);
    assert(it != m_readSet[thread][transactionLevel - 1].end());
    return it->second == OVERTAKING_LOAD;
}

void
TransactionIsolationManager::removeFromReadSetPerfectFilter(int thread,
                                                            Addr address){
  assert(PERFECT_FILTER);
  int transactionLevel = m_xact_mgr->getTransactionLevel(thread);
  assert(thread >= 0);

  assert(transactionLevel <= m_readSet[thread].size());

  Addr addr = makeLineAddress(address);

  map<Addr, char>::iterator it =
    m_readSet[thread][transactionLevel - 1].find(addr);
  if (it != m_readSet[thread][transactionLevel - 1].end()) {
    m_readSet[thread][transactionLevel-1].erase(it);
  }
  else { // release address not in Rset??
    assert(false);
  }
  assert(m_readSet[thread][transactionLevel-1].find(addr) ==
         m_readSet[thread][transactionLevel-1].end());
}

void
TransactionIsolationManager::removeFromWriteSetPerfectFilter(int thread,
                                                             Addr address){
  assert(PERFECT_FILTER);
  int transactionLevel = m_xact_mgr->getTransactionLevel(thread);
  assert(thread >= 0);
  assert(transactionLevel <= m_writeSet[thread].size());

  Addr addr = makeLineAddress(address);

  map<Addr, char>::iterator it =
    m_writeSet[thread][transactionLevel - 1].find(addr);
  if (it != m_writeSet[thread][transactionLevel - 1].end()) {
    m_writeSet[thread][transactionLevel-1].erase(it);
  }
  else { // release address not in Wset??
    assert(false);
  }
  assert(m_writeSet[thread][transactionLevel-1].find(addr) ==
         m_writeSet[thread][transactionLevel-1].end());
}


void
TransactionIsolationManager::addToWriteSetPerfectFilter(int thread,
                                                        Addr address,
                                                        int transactionLevel){
  assert(thread >= 0);
  assert(transactionLevel > 0);
  assert(transactionLevel <= m_writeSet[thread].size());
  Addr addr = makeLineAddress(address);

  if (m_writeSet[thread][transactionLevel - 1].find(addr) ==
      m_writeSet[thread][transactionLevel - 1].end())
    m_writeSet[thread][transactionLevel-1].
      insert(std::pair<Addr,char>(addr, RETIRED_STORE));

  assert(m_writeSet[thread][transactionLevel-1].find(addr) !=
         m_writeSet[thread][transactionLevel-1].end());
}

void
TransactionIsolationManager::clearReadSetPerfectFilter(int thread,
                                                       int transactionLevel){
  assert(thread >= 0);
  assert(transactionLevel > 0);
  assert(transactionLevel <= m_readSet[thread].size());

  m_readSet[thread][transactionLevel - 1].clear();
  assert(m_readSet[thread][transactionLevel - 1].size() == 0);
}

void
TransactionIsolationManager::clearWriteSetPerfectFilter(int thread,
                                                        int transactionLevel){
  assert(thread >= 0);
  assert(transactionLevel > 0);
  assert(transactionLevel <= m_writeSet[thread].size());

  m_writeSet[thread][transactionLevel - 1].clear();
  assert(m_writeSet[thread][transactionLevel - 1] .size() == 0);

  m_writeSetInWriteBuffer.clear(); // ideal lazy VM
}

void
TransactionIsolationManager::addToReadSetFilter(int thread,
                                                Addr address){
  Addr addr = makeLineAddress(address);
  if (!PERFECT_FILTER){
    m_readSetFilter[thread]->set(addr);
  }
}

void
TransactionIsolationManager::addToWriteSetFilter(int thread,
                                                 Addr address){
  Addr addr = makeLineAddress(address);
  if (!PERFECT_FILTER){
    m_writeSetFilter[thread]->set(addr);
  }
}

// returns a summary result. If any filter has addr in read set, returns true
bool
TransactionIsolationManager::isInReadSetFilterSummary(Addr physicalAddr){
  Addr addr = makeLineAddress(physicalAddr);
  int smt_threads = XACT_MGR->numberofSMTThreads();
  bool result = false;
  for (int p=0; p < smt_threads; ++p){
    result = result || isInReadSetFilter(p, addr);
  }
  return result;
}

bool
TransactionIsolationManager::isInWriteSetFilterSummary(Addr physicalAddr){
  Addr addr = makeLineAddress(physicalAddr);
  int smt_threads = XACT_MGR->numberofSMTThreads();
  bool result = false;
  for (int p=0; p < smt_threads; ++p){
    result = result || isInWriteSetFilter(p, addr);
  }
  return result;
}

// used to query either the Perfect or Bloom read set filter
bool
TransactionIsolationManager::isInReadSetFilter(int thread,
                                               Addr physicalAddr){
  Addr addr = makeLineAddress(physicalAddr);
  if (PERFECT_FILTER){
    // use perfect filters
    bool is_read = isInReadSetPerfectFilter(thread, addr);
    return is_read;
  }
  else{
    // use Bloom filters
    bool result = m_readSetFilter[thread]->isSet(addr);
    bool is_read = isInReadSetPerfectFilter(thread, addr);
    _unused(is_read);
    assert( result || !is_read);  // NO FALSE NEGATIVES
    return result;
  }
}

bool
TransactionIsolationManager::isInWriteSetFilter(int thread,
                                                Addr physicalAddr){
  Addr addr = makeLineAddress(physicalAddr);
  if (PERFECT_FILTER){
    // use perfect filters
    bool is_read = isInWriteSetPerfectFilter(thread, addr);
    return is_read;
  }
  else{
    // use Bloom filters
    bool result = m_writeSetFilter[thread]->isSet(addr);
    bool is_read = isInWriteSetPerfectFilter(thread, addr);
    _unused(is_read);
    assert( result || !is_read);  // NO FALSE NEGATIVES
    return result;
  }
}

void TransactionIsolationManager::clearReadSetFilter(int thread){
  if (!PERFECT_FILTER){
    m_readSetFilter[thread]->clear();
  }
}

void TransactionIsolationManager::clearWriteSetFilter(int thread){
  if (!PERFECT_FILTER){
    m_writeSetFilter[thread]->clear();
  }
}

int TransactionIsolationManager::getTotalReadSetCount(int thread){
  if (!PERFECT_FILTER){
    return m_readSetFilter[thread]->getTotalCount();
  } else
    return m_readSet[thread].size();
}

int TransactionIsolationManager::getTotalWriteSetCount(int thread){
  if (!PERFECT_FILTER){
    return m_writeSetFilter[thread]->getTotalCount();
  } else
    return m_writeSet[thread].size();
}

void
TransactionIsolationManager::
setFiltersToXactLevel(int thread, int new_xact_level, int old_xact_level){
  assert((new_xact_level >= 0) &&
         (new_xact_level <= m_readSet[thread].size()) &&
         (new_xact_level <= m_writeSet[thread].size()));

  clearReadSetFilter(thread);
  clearWriteSetFilter(thread);

  for (int i = 0; i < new_xact_level; i++){

    vector<Addr> readSet;
    for ( map<Addr, char>::iterator ii=m_readSet[thread][i].begin();
          ii!=m_readSet[thread][i].end();
          ++ii) {
      Addr key=(*ii).first;
      readSet.push_back(key);
    }

    vector<Addr> writeSet;
    for ( map<Addr, char>::iterator ii =
            m_writeSet[thread][i].begin();
          ii!=m_writeSet[thread][i].end();
          ++ii) {
      Addr key=(*ii).first;
      writeSet.push_back(key);
    }

    for (int j = 0; j < readSet.size(); j++)
      addToReadSetFilter(thread, readSet[j]);
    for (int j = 0; j < writeSet.size(); j++)
      addToWriteSetFilter(thread, writeSet[j]);
  }

  /*
  if (XACT_EAGER_CD) {
    for (int i = old_xact_level; i > new_xact_level; i--){
      g_system_ptr->getXactIsolationChecker()->
        clearReadSet(getLogicalProcID(thread), i);
      g_system_ptr->getXactIsolationChecker()->
        clearWriteSet(getLogicalProcID(thread), i);
    }
  }
  */
  DPRINTF(RubyHTM,"HTM: PROC %d setFiltersToXactLevel"
          " (new level: %d) \n", getProcID(), new_xact_level);
}


void TransactionIsolationManager::
profileReadSetFilterActivity(int xid, int thread, bool isCommit){
  /*
  if (!PERFECT_FILTER){
    int bits_set = m_readSetFilter[thread]->getTotalCount();
    g_system_ptr->getProfiler()->
        profileReadFilterBitsSet(xid, bits_set, isCommit);
  }
*/
}

void
TransactionIsolationManager::
profileWriteSetFilterActivity(int xid, int thread, bool isCommit){
  /*
  if (!PERFECT_FILTER){
    int bits_set = m_writeSetFilter[thread]->getTotalCount();
    g_system_ptr->getProfiler()->
      profileWriteFilterBitsSet(xid, bits_set, isCommit);
  }
*/
}

int TransactionIsolationManager::getReadSetSize(int thread, int xact_level){
  assert(xact_level >= 1);
  assert(xact_level <= m_readSet[thread].size());
  return m_readSet[thread][xact_level - 1].size();
}

int TransactionIsolationManager::getWriteSetSize(int thread, int xact_level){
  assert(xact_level >= 1);
  assert(xact_level <= m_writeSet[thread].size());
  return m_writeSet[thread][xact_level - 1].size();
}

AbstractBloomFilter *
TransactionIsolationManager::getReadSetFilter(int thread) {
  return m_readSetFilter[thread];
}

AbstractBloomFilter *
TransactionIsolationManager::getWriteSetFilter(int thread) {
  return m_writeSetFilter[thread];
}

vector<Addr> *
TransactionIsolationManager::getWriteSet(int thread, int xact_level)
{
  // Allocates a new vector<Addr> with the write set and returns it
  // Caller must delete the object after it is done with it
  assert(xact_level == 1);
  assert(xact_level == m_writeSet[thread].size());

  vector<Addr> *wset = new vector<Addr>();
  for ( map<Addr, char>::iterator ii =
          m_writeSet[thread][xact_level-1].begin();
        ii!=m_writeSet[thread][xact_level-1].end(); ++ii) {
    Addr key=(*ii).first;
    wset->push_back(key);
  }
  return wset;
}

vector<Addr> *
TransactionIsolationManager::getReadSet(int thread, int xact_level)
{
  // Allocates a new vector<Addr> with the read set and returns it
  // Caller must delete the object after it is done with it
  assert(xact_level == 1);
  assert(xact_level == m_readSet[thread].size());

  vector<Addr> *rset = new vector<Addr>();
  for ( map<Addr, char>::iterator ii =
          m_readSet[thread][xact_level-1].begin();
        ii!=m_readSet[thread][xact_level-1].end(); ++ii) {
    Addr key=(*ii).first;
    rset->push_back(key);
  }
  return rset;
}

void 
TransactionIsolationManager::redirectedStoreToWriteBuffer(Addr addr)
{
    m_writeSetInWriteBuffer.
      insert(std::pair<Addr,char>(addr, RETIRED_STORE));
}

bool
TransactionIsolationManager::isRedirectedStoreToWriteBuffer(Addr addr)
{
    return m_writeSetInWriteBuffer.find(addr) !=
        m_writeSetInWriteBuffer.end();
}

bool
TransactionIsolationManager::hasConflictWith(TransactionIsolationManager* new_committer)
{
    // this: ongoing committer
    // Check write set of ongoing committer against read-write set of new committer
    for ( map<Addr, char>::iterator ii =
              m_writeSetInWriteBuffer.begin();
          ii!=m_writeSetInWriteBuffer.end(); ++ii) {
        Addr waddr=(*ii).first;
        if (new_committer->isInReadSetFilterSummary(waddr)) {
            return true;
        }
        else if (new_committer->isRedirectedStoreToWriteBuffer(waddr)) {
            return true;
        }
    }
    return false;
}


void
TransactionIsolationManager::validateTransaction(int thread, int xact_level)
{
  assert(false);
#if 0
  assert(xact_level == 1);
  uint64_t committer_ts = m_xact_mgr->
    getXactConflictManager()->getOldestTimestamp();
  MachineID committer_id = createMachineID(MachineType_L1Cache, m_version);
  for ( map<Addr, char>::iterator ii =
          m_writeSet[thread][xact_level-1].begin();
        ii != m_writeSet[thread][xact_level-1].end();
        ++ii) {
    Addr addr=(*ii).first;
    int num_cpus = g_system_ptr->getNumOfSequencers();
    for (int i=0; i < num_cpus; i++) {
      if (i == m_version) continue;
      TransactionInterfaceManager *remote_mgr =
        g_system_ptr->getTransactionInterfaceManager(i);

      if (remote_mgr->getXactConflictManager()->
          shouldNackStore(addr, committer_ts, committer_id, true, false)) {
        DPRINTF(RubyHTM,"HTM: PROC %02d validateTransaction "
                "aborting proc %02", i);
      }
    }
  }
#endif
}

} // namespace ruby
} // namespace gem5
