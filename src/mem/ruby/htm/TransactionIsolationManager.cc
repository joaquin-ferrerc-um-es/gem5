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

#define EXECUTED_LOAD ('x')
#define RETIRED_LOAD ('r')
#define OVERTAKING_LOAD ('o')
#define RETIRED_STORE ('w')

TransactionIsolationManager::
TransactionIsolationManager(TransactionInterfaceManager *xact_mgr,
                            int version) {

  m_version = version;
  m_xact_mgr = xact_mgr;

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

void TransactionIsolationManager::beginTransaction(){
  int xact_level = XACT_MGR->getTransactionLevel();
  assert(xact_level == 1);
  if (xact_level > m_readSet.size()){
    m_readSet.resize(xact_level);
  }
  if (!m_readSet[xact_level - 1].empty()) {
      // Do not clear Rset on beginTransaction since in O3CPU trans
      // loads may overtake the xbegin.
      DPRINTF(RubyHTM,"HTM: PROC %d beginTransaction finds"
              " non-empty read set\n", getProcID());
  }
  //  m_readSet[xact_level - 1].clear();
  if (xact_level > m_writeSet.size()){
    m_writeSet.resize(xact_level);
  }
  m_writeSet[xact_level - 1].clear();
}

void TransactionIsolationManager::commitTransaction(){
  int old_xact_level = XACT_MGR->getTransactionLevel();
  assert(old_xact_level >= 1);
  int new_xact_level = old_xact_level - 1;

  vector<Addr> readSet;
  for ( map<Addr, char>::iterator ii =
            m_readSet[old_xact_level-1].begin();
        ii!=m_readSet[old_xact_level-1].end(); ++ii) {
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
            m_writeSet[old_xact_level-1].begin();
        ii!=m_writeSet[old_xact_level-1].end(); ++ii) {
      Addr key=(*ii).first;
      writeSet.push_back(key);
  }

  if (new_xact_level > 0){
      for (int i = 0; i < readSet.size(); i++)
          addToReadSetPerfectFilter(readSet[i], new_xact_level);
      for (int i = 0; i < writeSet.size(); i++)
          addToWriteSetPerfectFilter(writeSet[i], new_xact_level);
  }

  clearReadSetPerfectFilter(old_xact_level);
  clearWriteSetPerfectFilter(old_xact_level);

  setFiltersToXactLevel(new_xact_level, old_xact_level);
}

void TransactionIsolationManager::abortTransaction(int new_xact_level){
  int old_xact_level = XACT_MGR->getTransactionLevel();
  assert(old_xact_level >= 1);
  assert(new_xact_level < old_xact_level);
  assert(new_xact_level >= 0);

  for (int i = new_xact_level; i < old_xact_level; i++){
    clearReadSetPerfectFilter(i + 1);
    clearWriteSetPerfectFilter(i + 1);
  }

  setFiltersToXactLevel(new_xact_level, old_xact_level);
}

void TransactionIsolationManager::releaseIsolation(int xact_level){
  assert(xact_level >= 0);

  clearReadSetPerfectFilter(xact_level);
  clearWriteSetPerfectFilter(xact_level);

  setFiltersToXactLevel(xact_level - 1, xact_level);
}

void TransactionIsolationManager::releaseReadIsolation(){

  int levels = XACT_MGR->getTransactionLevel();
  for (int i = 0; i < levels; i++){
    clearReadSetPerfectFilter(i + 1);
  }

  DPRINTF(RubyHTM,"HTM: PROC %d releaseReadIsolation (abort) \n",
          getProcID());

}

bool
TransactionIsolationManager::isInReadSetPerfectFilter(Addr addr) {
  int transactionLevel = 1;

  map<Addr, char>::iterator it =
      m_readSet[transactionLevel-1].find(makeLineAddress(addr));
  if (it != m_readSet[transactionLevel-1].end()) {
      if (XACT_MGR->getTransactionLevel() == 0) {
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
TransactionIsolationManager::isInWriteSetPerfectFilter(Addr addr){
  int transactionLevel = 1;
  map<Addr, char>::iterator it =
      m_writeSet[transactionLevel-1].find(makeLineAddress(addr));
  if (it != m_writeSet[transactionLevel-1].end()) {
      assert(it->second == RETIRED_STORE);
      return true;
  } else {
      return false;
  }
}

void
TransactionIsolationManager::addToReadSetPerfectFilter(Addr address,
                                                       int transactionLevel){
  assert(transactionLevel > 0);
  assert(transactionLevel <= m_readSet.size());
  Addr addr = makeLineAddress(address);
  char value = EXECUTED_LOAD;
  if (XACT_MGR->getTransactionLevel() == 0) {
      // Sanity checks: distinguish read-set blocks that have been
      // isolated before xact mgr::beginTransaction called
      value = OVERTAKING_LOAD; // (o)vertaking load
  }

  if (m_readSet[transactionLevel - 1].find(addr) ==
      m_readSet[transactionLevel - 1].end())
    m_readSet[transactionLevel-1].
      insert(std::pair<Addr,char>(addr, value));

  assert(m_readSet[transactionLevel-1].find(addr) !=
         m_readSet[transactionLevel-1].end());

}

void
TransactionIsolationManager::addToRetiredReadSet(Addr addr)
{
    int transactionLevel = 1;
    assert(XACT_MGR->getTransactionLevel() == 1);
    map<Addr, char>::iterator it =
        m_readSet[transactionLevel-1].find(makeLineAddress(addr));
    assert(it != m_readSet[transactionLevel-1].end());
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
TransactionIsolationManager::inRetiredReadSet(Addr addr) {
  int transactionLevel = 1;
  map<Addr, char>::iterator it =
      m_readSet[transactionLevel-1].find(makeLineAddress(addr));
  assert(it != m_readSet[transactionLevel-1].end());
  // Trans loads cannot retire before htm_start instruction
  return (it->second == RETIRED_LOAD);
}

bool
TransactionIsolationManager::wasOvertakingRead(Addr addr) {
    int transactionLevel = 1;
    map<Addr, char>::iterator it =
        m_readSet[transactionLevel - 1].find(addr);
    assert(it != m_readSet[transactionLevel - 1].end());
    return it->second == OVERTAKING_LOAD;
}

void
TransactionIsolationManager::removeFromReadSetPerfectFilter(Addr address){
  int transactionLevel = m_xact_mgr->getTransactionLevel();

  assert(transactionLevel <= m_readSet.size());

  Addr addr = makeLineAddress(address);

  map<Addr, char>::iterator it =
    m_readSet[transactionLevel - 1].find(addr);
  if (it != m_readSet[transactionLevel - 1].end()) {
    m_readSet[transactionLevel-1].erase(it);
  }
  else { // release address not in Rset??
    assert(false);
  }
  assert(m_readSet[transactionLevel-1].find(addr) ==
         m_readSet[transactionLevel-1].end());
}

void
TransactionIsolationManager::removeFromWriteSetPerfectFilter(Addr address){
  int transactionLevel = m_xact_mgr->getTransactionLevel();
  assert(transactionLevel <= m_writeSet.size());

  Addr addr = makeLineAddress(address);

  map<Addr, char>::iterator it =
    m_writeSet[transactionLevel - 1].find(addr);
  if (it != m_writeSet[transactionLevel - 1].end()) {
    m_writeSet[transactionLevel-1].erase(it);
  }
  else { // release address not in Wset??
    assert(false);
  }
  assert(m_writeSet[transactionLevel-1].find(addr) ==
         m_writeSet[transactionLevel-1].end());
}


void
TransactionIsolationManager::addToWriteSetPerfectFilter(Addr address,
                                                        int transactionLevel){
  assert(transactionLevel > 0);
  assert(transactionLevel <= m_writeSet.size());
  Addr addr = makeLineAddress(address);

  if (m_writeSet[transactionLevel - 1].find(addr) ==
      m_writeSet[transactionLevel - 1].end())
    m_writeSet[transactionLevel-1].
      insert(std::pair<Addr,char>(addr, RETIRED_STORE));

  assert(m_writeSet[transactionLevel-1].find(addr) !=
         m_writeSet[transactionLevel-1].end());
}

void
TransactionIsolationManager::clearReadSetPerfectFilter(int transactionLevel){
  assert(transactionLevel > 0);
  assert(transactionLevel <= m_readSet.size());

  m_readSet[transactionLevel - 1].clear();
  assert(m_readSet[transactionLevel - 1].size() == 0);
}

void
TransactionIsolationManager::clearWriteSetPerfectFilter(int transactionLevel){
  assert(transactionLevel > 0);
  assert(transactionLevel <= m_writeSet.size());

  m_writeSet[transactionLevel - 1].clear();
  assert(m_writeSet[transactionLevel - 1] .size() == 0);

  m_writeSetInWriteBuffer.clear(); // ideal lazy VM
}

void
TransactionIsolationManager::
setFiltersToXactLevel(int new_xact_level, int old_xact_level){
  assert((new_xact_level >= 0) &&
         (new_xact_level <= m_readSet.size()) &&
         (new_xact_level <= m_writeSet.size()));

  for (int i = 0; i < new_xact_level; i++){

    vector<Addr> readSet;
    for ( map<Addr, char>::iterator ii=m_readSet[i].begin();
          ii!=m_readSet[i].end();
          ++ii) {
      Addr key=(*ii).first;
      readSet.push_back(key);
    }

    vector<Addr> writeSet;
    for ( map<Addr, char>::iterator ii =
            m_writeSet[i].begin();
          ii!=m_writeSet[i].end();
          ++ii) {
      Addr key=(*ii).first;
      writeSet.push_back(key);
    }
  }

  DPRINTF(RubyHTM,"HTM: PROC %d setFiltersToXactLevel"
          " (new level: %d) \n", getProcID(), new_xact_level);
}


int TransactionIsolationManager::getReadSetSize(int xact_level){
  assert(xact_level >= 1);
  assert(xact_level <= m_readSet.size());
  return m_readSet[xact_level - 1].size();
}

int TransactionIsolationManager::getWriteSetSize(int xact_level){
  assert(xact_level >= 1);
  assert(xact_level <= m_writeSet.size());
  return m_writeSet[xact_level - 1].size();
}

vector<Addr> *
TransactionIsolationManager::getWriteSet(int xact_level)
{
  // Allocates a new vector<Addr> with the write set and returns it
  // Caller must delete the object after it is done with it
  assert(xact_level == 1);
  assert(xact_level == m_writeSet.size());

  vector<Addr> *wset = new vector<Addr>();
  for ( map<Addr, char>::iterator ii =
          m_writeSet[xact_level-1].begin();
        ii!=m_writeSet[xact_level-1].end(); ++ii) {
    Addr key=(*ii).first;
    wset->push_back(key);
  }
  return wset;
}

vector<Addr> *
TransactionIsolationManager::getReadSet(int xact_level)
{
  // Allocates a new vector<Addr> with the read set and returns it
  // Caller must delete the object after it is done with it
  assert(xact_level == 1);
  assert(xact_level == m_readSet.size());

  vector<Addr> *rset = new vector<Addr>();
  for ( map<Addr, char>::iterator ii =
          m_readSet[xact_level-1].begin();
        ii!=m_readSet[xact_level-1].end(); ++ii) {
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
        if (new_committer->isInReadSetPerfectFilter(waddr)) {
            return true;
        }
        else if (new_committer->isRedirectedStoreToWriteBuffer(waddr)) {
            return true;
        }
    }
    return false;
}


} // namespace ruby
} // namespace gem5
