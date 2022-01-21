/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#include "mem/ruby/htm/TransactionConflictManager.hh"

#include <cassert>
#include <cstdlib>

#include "debug/RubyHTM.hh"
#include "mem/ruby/htm/LazyTransactionCommitArbiter.hh"
#include "mem/ruby/htm/TransactionInterfaceManager.hh"
#include "mem/ruby/htm/TransactionIsolationManager.hh"
#include "mem/ruby/system/RubySystem.hh"
#include "mem/ruby/system/Sequencer.hh"

namespace gem5
{
namespace ruby
{


#define XACT_CONFLICT_RES m_policy

TransactionConflictManager::
TransactionConflictManager(TransactionInterfaceManager *xact_mgr,
                           int version) {
  int smt_threads = m_xact_mgr->numberofSMTThreads();

  m_version = version;
  m_xact_mgr = xact_mgr;

  // The cycle at which Ruby simulation starts is substracted from
  // current time to generate timestamps in order to avoid overflowing
  // the Timestamp field (int) in SLICC protocol messages (SLICC
  // doesn't support long types). Must set it only once at the
  // beginning, as RubySystem::resetStats updates the start cycle

  m_timestamp       = new Cycles[smt_threads];
  m_lock_timestamp  = new bool[smt_threads];
  m_possible_cycle  = new bool[smt_threads];
  m_numRetries      = new int[smt_threads];
  m_receivedNack          = new bool[smt_threads];
  m_sentNack      = new bool[smt_threads];
  m_sentNackAddr      = new Addr[smt_threads];
  m_doomed          = new bool[smt_threads];

  for (int i = 0; i < smt_threads; i++){
    m_timestamp[i]      = Cycles(0);
    m_possible_cycle[i] = false;
    m_lock_timestamp[i] = false;
    m_numRetries[i]     = 0;
    m_sentNack[i]         = false;
    m_receivedNack[i]         = false;
    m_doomed[i]         = false;
  }

  m_policy = xact_mgr->config_conflictResPolicy();
  if (m_policy == HtmPolicyStrings::requester_stalls_cda_base ||
      m_policy == HtmPolicyStrings::requester_stalls_cda_hybrid ||
      m_policy == HtmPolicyStrings::requester_stalls_cda_hybrid_ntx ||
      m_policy == HtmPolicyStrings::requester_stalls_cda_base_ntx) {
      m_policy_is_req_stalls_cda = true;
  } else {
      m_policy_is_req_stalls_cda = false;
  }
  if (m_policy == HtmPolicyStrings::requester_stalls_cda_base_ntx ||
      m_policy == HtmPolicyStrings::requester_stalls_cda_hybrid_ntx) {
      m_policy_nack_non_transactional = true;
  } else {
      m_policy_nack_non_transactional = false;
  }
}

TransactionConflictManager::~TransactionConflictManager() {
}

void
TransactionConflictManager::setVersion(int version) {
  m_version = version;
}

int
TransactionConflictManager::getVersion() const {
  return m_version;
}

int
TransactionConflictManager::getProcID() const{
  return m_xact_mgr->getProcID();
}

int
TransactionConflictManager::getLogicalProcID(int thread) const{
  return getProcID() * m_xact_mgr->numberofSMTThreads() + thread;
}

void
TransactionConflictManager::beginTransaction(int thread){
  int transactionLevel = m_xact_mgr->getTransactionLevel(thread);
  assert(transactionLevel >= 1);

  if ((transactionLevel == 1) && !(m_lock_timestamp[thread])){
    string conflict_res_policy(XACT_CONFLICT_RES);
    m_timestamp[thread] = m_xact_mgr->curCycle();
    m_lock_timestamp[thread] = true;
  }
}

void
TransactionConflictManager::commitTransaction(int thread){
  int transactionLevel = m_xact_mgr->getTransactionLevel(thread);
  assert(transactionLevel >= 1);
  assert(!m_doomed[thread]);

  if (transactionLevel == 1){
    m_lock_timestamp[thread] = false;
    m_numRetries[thread]     = 0;
    m_receivedNack[thread]         = false;
    clearPossibleCycle(thread);

  }
}

void
TransactionConflictManager::restartTransaction(int thread){
  m_numRetries[thread]++;
  clearPossibleCycle(thread);
  m_sentNack[thread]         = false;
  m_receivedNack[thread] = false;
  m_doomed[thread] = false;
}

int
TransactionConflictManager::getNumRetries(int thread){
  return m_numRetries[thread];
}

bool
TransactionConflictManager::possibleCycle(int thread){
  return m_possible_cycle[thread];
}

void
TransactionConflictManager::setPossibleCycle(int thread){
  m_possible_cycle[thread] = true;
}

void
TransactionConflictManager::clearPossibleCycle(int thread){
  m_possible_cycle[thread] = false;
}

bool
TransactionConflictManager::nackReceived(int thread){
  return m_receivedNack[thread];
}

bool
TransactionConflictManager::doomed(int thread){
  return m_doomed[thread];
}

void
TransactionConflictManager::setDoomed(int thread){
    m_doomed[thread] = true;
}

Cycles
TransactionConflictManager::getTimestamp(int thread){
  if (m_xact_mgr->getTransactionLevel(thread) > 0)
    return m_timestamp[thread];
  else {
      Cycles ts = m_xact_mgr->curCycle();
      assert(ts > 0); // Detect overflows
      return ts;
  }
}

bool
TransactionConflictManager::isRequesterStallsPolicy(){
    return m_policy_is_req_stalls_cda;
}

Cycles
TransactionConflictManager::getOldestTimestamp(){
  Cycles currentTime = m_xact_mgr->curCycle();
  Cycles oldestTime = currentTime;

  for (int i = 0; i < m_xact_mgr->numberofSMTThreads(); i++){
    if ((m_xact_mgr->getTransactionLevel(i) > 0) &&
        (m_timestamp[i] < oldestTime))
        oldestTime = m_timestamp[i];
  }
  assert(oldestTime > 0);
  return oldestTime;
}

bool
TransactionConflictManager::isRemoteOlder(int thread,
                                          int remote_thread,
                                          Cycles local_timestamp,
                                          Cycles remote_timestamp,
                                          MachineID remote_id){

  bool older = false;

  if (local_timestamp == remote_timestamp){
    if (getProcID() == (int) machineIDToNodeID(remote_id)){
      older = (remote_thread < thread);
    } else {
      older = (int) machineIDToNodeID(remote_id) < getProcID();
    }
  } else {
    older = (remote_timestamp < local_timestamp);
  }
  return older;
}

bool
TransactionConflictManager::shouldNackLoad(Addr addr,
                                           MachineID remote_id,
                                           Cycles remote_timestamp,
                                           bool remote_trans)
{
  int thread=0;
  string conflict_res_policy(XACT_CONFLICT_RES);

  bool shouldNack; // Leave uninitialize so that compiler warns us if
                   // we ever miss a case
  bool remoteNonTransWins = false;
  bool existConflict = m_xact_mgr->
    getXactIsolationManager()->isInWriteSetFilterSummary(addr);
  if (existConflict) {
      assert(machineIDToMachineType(remote_id) == MachineType_L1Cache);
      assert(remote_timestamp > 0);
      DPRINTF(RubyHTM, "Conflict detected by shouldNackLoad,"
              " requestor=%d addr %#lx (%s)\n",
              machineIDToNodeID(remote_id), addr,
              remote_trans ? "trans" : "non-trans");
      if (m_xact_mgr->isUnrollingLog(thread)) {
          assert(!m_xact_mgr->config_lazyVM()); // LogTM
          shouldNack = true;
#if 0 // TODO: Check
      } else if (m_xact_mgr->isDoomed(thread)) {
          shouldNack = false;
#endif
      } else if (m_policy_is_req_stalls_cda) {
          if (!remote_trans &&
              !m_policy_nack_non_transactional) {
              if (!m_xact_mgr->config_lazyVM()) { // LogTM
                  shouldNack = true; // Nack until old value restored
                  // Abort but keep nacking until old value restored
                  // from log (or read signature cleared before log unroll)
                  m_xact_mgr->setAbortFlag(thread, addr, remote_id,
                                           remote_trans);
                  DPRINTF(RubyHTM,"Abort due to non-transactional conflicting"
                          " access to address %#x from remote reader %d\n",
                          addr, machineIDToNodeID(remote_id));
              } else {
                  shouldNack = false;
                  remoteNonTransWins = true;

                  DPRINTF(RubyHTM,"Cannot nack non-transactional conflicting"
                          " access to address %#x from remote reader %d\n",
                          addr, machineIDToNodeID(remote_id));
              }
          } else { // trans-trans conflict
              shouldNack = true;
          }
      } else if (!m_xact_mgr->config_eagerCD() && // Lazy conflict detection
                 m_xact_mgr->getXactLazyCommitArbiter()->validated() &&
                 conflict_res_policy == HtmPolicyStrings::committer_wins) {
          shouldNack = true;
      }
      else {
          assert(conflict_res_policy ==
                 HtmPolicyStrings::requester_wins ||
                 (!m_xact_mgr->config_eagerCD() &&
                  !m_xact_mgr->getXactLazyCommitArbiter()->validated()));
          DPRINTF(RubyHTM, "Conflict (%s):  Local writer"
                  " %d vs remote reader %d for addr %#lx\n",
                  conflict_res_policy, getProcID(),
                  machineIDToNodeID(remote_id), addr);
          shouldNack = false;
          if (!m_xact_mgr->config_lazyVM()) { // LogTM+reqwins
              panic("Eager VM + requester wins not tested!\n");
          }
      }
      DPRINTF(RubyHTM,"HTM: PROC %d shouldNackLoad detected conflict "
              "with PROC %d for address %#x, %s %d\n", getProcID(),
              machineIDToNodeID(remote_id), addr,
              shouldNack ? "nacks" : "aborted by",
              machineIDToNodeID(remote_id));

      // Finally, if req not nacked, resolve by aborting local tx
      if (!shouldNack) {
          assert(!m_policy_is_req_stalls_cda ||
                 (!hasHighestPriority() ||
                  remoteNonTransWins ||
                  (machineIDToMachineType(remote_id) == MachineType_L2Cache)));
          m_xact_mgr->setAbortFlag(thread, addr,
                                   remote_id, remote_trans);
      } else{
          notifySendNack(addr, remote_timestamp, remote_id);
      }
  }
  else { // No conflict
      shouldNack = false;
  }

  return shouldNack;
}

bool
TransactionConflictManager::shouldNackStore(Addr addr,
                                            MachineID remote_id,
                                            Cycles remote_timestamp,
                                            bool remote_trans,
                                            bool local_is_exclusive)
{
  int thread=0;
  string conflict_res_policy(XACT_CONFLICT_RES);
  bool shouldNack;
  bool local_is_writer = m_xact_mgr->getXactIsolationManager()->
      isInWriteSetFilterSummary(addr);
  bool existConflict = local_is_writer ||
    m_xact_mgr->getXactIsolationManager()->
    isInReadSetFilterSummary(addr);
  bool localIsYoungerReader = false;
  bool remoteNonTransWins = false;

  if (existConflict) {
      if (machineIDToMachineType(remote_id) == MachineType_L2Cache) {
          // LLC replacement
          assert(remote_timestamp == 0);
          DPRINTF(RubyHTM, "L2 cache eviction of transactional block"
                  " addr %#lx (local is writer: %d)\n", addr,
                  local_is_writer);
          if ((!local_is_writer &&
               !m_xact_mgr->config_allowReadSetL2CacheEvictions()) ||
              (local_is_writer &&
               !m_xact_mgr->config_allowWriteSetL2CacheEvictions())) {
              m_xact_mgr->setAbortFlag(thread, addr, remote_id,
                                       remote_trans);
          }
          return false;
      } else {
          assert(machineIDToMachineType(remote_id) == MachineType_L1Cache);
          assert(remote_timestamp > 0);
      }
      DPRINTF(RubyHTM, "Conflict detected by shouldNackStore,"
              " requestor=%d addr %#lx (%s)\n",
              machineIDToNodeID(remote_id), addr,
              remote_trans ? "trans" : "non-trans");

      if (m_xact_mgr->isUnrollingLog(thread)) {
          assert(!m_xact_mgr->config_lazyVM()); // LogTM
          shouldNack = true;
#if 0 // TODO: Check
      } else if (m_xact_mgr->isDoomed(thread)) {
          shouldNack = false;
#endif
      } else if (m_policy_is_req_stalls_cda) {
          if (!remote_trans &&
              !m_policy_nack_non_transactional) {
              if (!m_xact_mgr->config_lazyVM() && // LogTM
                  local_is_writer) {
                  shouldNack = true;

                  // Abort but keep nacking until old value restored
                  // from log (or read signature cleared before log unroll)
                  m_xact_mgr->setAbortFlag(thread, addr, remote_id,
                                           remote_trans);
                  DPRINTF(RubyHTM,"Abort due to non-transactional conflicting"
                          " access to address %#x from remote writer %d\n",
                          addr, machineIDToNodeID(remote_id));
              } else {
                  shouldNack = false;
                  remoteNonTransWins = true;

                  DPRINTF(RubyHTM,"Cannot nack non-transactional conflicting"
                          " access to address %#x from remote writer %d\n",
                          addr, machineIDToNodeID(remote_id));
              }
          } else { // trans-trans conflict
              shouldNack = true;
              int remote_thread = 0;
              if (m_policy == HtmPolicyStrings::requester_stalls_cda_hybrid &&
                  !local_is_writer &&
                  isRemoteOlder(thread, remote_thread, getTimestamp(thread),
                                remote_timestamp, remote_id)) {
                  // See Bobba ISCA 2007: CDA hybrid allows an elder
                  // writer to simultanously abort a number of younger
                  // readers
                  localIsYoungerReader = true;
                  shouldNack = false;
              }
          }
      } else if (!m_xact_mgr->config_eagerCD() && // Lazy conflict detection
                 m_xact_mgr->getXactLazyCommitArbiter()->validated() &&
                 conflict_res_policy == HtmPolicyStrings::committer_wins) {
          shouldNack = true;
      } else {
          assert(conflict_res_policy ==
                 HtmPolicyStrings::requester_wins ||
                 (!m_xact_mgr->config_eagerCD() &&
                  !m_xact_mgr->getXactLazyCommitArbiter()->validated()));

          DPRINTF(RubyHTM, "Conflict (%s):  Local %d %d "
                  "vs remote writer %d for addr %#lx\n",
                  conflict_res_policy,
                  local_is_writer ? "writer" : "reader", getProcID(),
                  machineIDToNodeID(remote_id), addr);
          shouldNack = false;
          if (!m_xact_mgr->config_lazyVM()) { // LogTM+reqwins
              panic("Eager VM + requester wins not tested!\n");
          }
      }
      DPRINTF(RubyHTM,"HTM: PROC %d shouldNackStore detected conflict "
              "with PROC %d for address %#x, %s %d\n", getProcID(),
              machineIDToNodeID(remote_id), addr,
              shouldNack ? "nacks" : "aborted by",
              machineIDToNodeID(remote_id));

      // Finally, if req not nacked, resolve by aborting local tx
      if (!shouldNack) {
          assert(!m_policy_is_req_stalls_cda ||
                 (!hasHighestPriority() ||
                  localIsYoungerReader ||
                  remoteNonTransWins ||
                  (machineIDToMachineType(remote_id) == MachineType_L2Cache)));
          m_xact_mgr->setAbortFlag(thread, addr, remote_id,
                                   remote_trans);
      } else{
          notifySendNack(addr, remote_timestamp, remote_id);
      }
 /* Atomiciy may be violated if the load is handled by the L1 cache
    controller in the same fashion as non-transactional loads. Let us
    suppose that the GETS from the reader arrives first at the
    directory and gets data from L2, and while this data message
    arrives to the reader's L1 cache, a GETX arrives at dir and the
    resulting invalidation sent to the new reader overtake the L2 data
    message. For non-transactional loads, this is not a race, as it
    does not matter whether the data obtained by the load comes from
    L2 (the happens before the store) or from the writer's cache (the
    load happens after the store), both values (pre and post update)
    are valid. The L1 protocol sends an ACK for the pending load miss,
    goes to IS_I and when the data eventually arrives, the miss is
    resolved, the data is used and a shared copy is kept in cache only
    if it the data came from another L1 (the reader obtained the data
    from the writer whose inv it acked earlier), otherwise the line is
    invalidated. However, in the case of a transactional load, only
    the post-update value is correct: if the reader acks the
    invalidation and does not signal an abort (the SR bit is not yet
    set since the load has not completed), and then uses the data when
    it arrives regardless of its source, atomiciy will be violated if
    the data came from the L2 and the writer commits immediately
    after. The reader transaction will continue execution and may
    commit after the writer despite having observed a value that was
    modified by an earlier transaction.

    How we finally solved this. The timing CPU isolates loads early
    and thus will detect conflicts with pending loads, since they are
    not speculative. The O3 CPU isolates loads when they retire, and
    detects conflicts with pending loads by forwarding evictions also
    when invalidations are received for pending misses that result in
    the cache not keeping a copy of the block.
 */
  }
  else {
       shouldNack = false;
  }

  return shouldNack;
}


void
TransactionConflictManager::notifySendNack(Addr addr,
                                           Cycles remote_timestamp,
                                           MachineID remote_id){
  // This method is used to update the deadlock avoidance logic, if used
  string conflict_res_policy(XACT_CONFLICT_RES);

  if (m_policy_is_req_stalls_cda) {
    assert(m_xact_mgr->numberofSMTThreads() == 1);
    int remote_thread = 0;
    for (int i = 0; i < m_xact_mgr->numberofSMTThreads(); i++){
      if (m_xact_mgr->getXactIsolationManager()->
          isInReadSetFilter(i, addr) ||
          m_xact_mgr->getXactIsolationManager()->
          isInWriteSetFilter(i, addr)) {
        if (isRemoteOlder(i, remote_thread, getTimestamp(i),
                          remote_timestamp, remote_id)){
          m_sentNack[i] = true;
          m_sentNackAddr[i] = addr;
          setPossibleCycle(i);
          DPRINTF(RubyHTM,"HTM: PROC %d notifySendNack "
                  "sets possible cycle after conflict "
                  "with PROC %d for address %#x\n", getProcID(),
                  machineIDToNodeID(remote_id), addr);
        }
      }
    }
  }
}

void
TransactionConflictManager::notifyReceiveNack(Addr addr,
                                              Cycles remote_timestamp,
                                              MachineID remote_id){
    int thread = 0;
    int transactionLevel = m_xact_mgr->getTransactionLevel(thread);
    if (transactionLevel == 0) return;

    assert(m_xact_mgr->numberofSMTThreads() == 1);

    Cycles local_timestamp = getTimestamp(thread);
    int remote_thread = 0;
    string conflict_res_policy(XACT_CONFLICT_RES);

    if (m_policy_is_req_stalls_cda) {
        if (possibleCycle(thread) &&
            isRemoteOlder(thread, remote_thread, local_timestamp,
                          remote_timestamp, remote_id)){
            if (m_xact_mgr->isUnrollingLog(thread)) {
                assert(!m_xact_mgr->config_lazyVM()); // LogTM
                // Already aborted
                return;
            }
            m_xact_mgr->setAbortFlag(thread,
                                     m_sentNackAddr[thread],
                                     remote_id);
            DPRINTF(RubyHTM,"HTM: PROC %d notifyReceiveNack "
                    "found possible cycle set after conflict "
                    "with PROC %d for address %#x, aborting "
                    "local (%d) as it not older  than remote (%d)"
                    "%s\n",
                    getProcID(), machineIDToNodeID(remote_id),
                    addr, local_timestamp, remote_timestamp,
                    (local_timestamp == remote_timestamp) ?
                    " and remote has lower proc ID" : "");
        }
        else if (possibleCycle(thread)) {
            DPRINTF(RubyHTM,"HTM: PROC %d notifyReceiveNack "
                    "found possible cycle set after conflict "
                    "with PROC %d for address %#x, "
                    "but local (%d) is older than remote (%d) \n",
                    getProcID(), machineIDToNodeID(remote_id),
                    addr, local_timestamp, remote_timestamp);
        }
    }
}

bool
TransactionConflictManager::hasHighestPriority()
{
    // Magic conflict detection at commit time
    std::vector<TransactionInterfaceManager*> mgrs =
        m_xact_mgr->getRemoteTransactionManagers();

    Cycles local_ts = getOldestTimestamp();
    for (int i=0; i < mgrs.size(); i++) {
        TransactionInterfaceManager* remote_mgr=mgrs[i];
        Cycles remote_ts = remote_mgr->
            getXactConflictManager()->getOldestTimestamp();
        if (remote_ts < local_ts)
            return false;
    }
    return true;
}

} // namespace ruby
} // namespace gem5
