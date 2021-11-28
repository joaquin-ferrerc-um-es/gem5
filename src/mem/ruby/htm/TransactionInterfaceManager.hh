/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#ifndef __MEM_RUBY_HTM_TRANSACTIONINTERFACEMANAGER_HH__
#define __MEM_RUBY_HTM_TRANSACTIONINTERFACEMANAGER_HH__

#include <vector>

#include "mem/htm.hh"
#include "mem/packet.hh"
#include "mem/request.hh"
#include "mem/ruby/common/Address.hh"
#include "mem/ruby/profiler/annotated_regions.h"
#include "mem/ruby/structures/CacheMemory.hh"
#include "mem/ruby/system/RubySystem.hh"
#include "mem/ruby/system/TransactionalSequencer.hh"
#include "params/TransactionInterfaceManager.hh"
#include "sim/sim_object.hh"
#include "sim/system.hh"

namespace gem5
{
namespace ruby
{

class EagerTransactionVersionManager;
class LazyTransactionCommitArbiter;
class LazyTransactionVersionManager;
class TransactionInterfaceManager;
class TransactionConflictManager;
class TransactionIsolationManager;

#define _unused(x) ((void)(x))


class TransactionInterfaceManager : public ClockedObject
{
public:
  PARAMS(TransactionInterfaceManager);
  TransactionInterfaceManager(const Params &p);
  ~TransactionInterfaceManager();

  void regStats() override;
  TransactionIsolationManager* getXactIsolationManager();
  TransactionConflictManager*  getXactConflictManager();
  EagerTransactionVersionManager*   getXactEagerVersionManager();
  LazyTransactionVersionManager*   getXactLazyVersionManager();
  LazyTransactionCommitArbiter* getXactLazyCommitArbiter();
  TransactionalSequencer *getSequencer();

  bool shouldNackLoad(Addr addr,
                      MachineID requestor,
                      Cycles remote_timestamp,
                      bool remote_trans);
  bool shouldNackStore(Addr addr,
                       MachineID requestor,
                       Cycles remote_timestamp,
                       bool remote_trans,
                       bool local_is_exclusive);
  void notifyReceiveNack(Addr addr, Cycles remote_timestamp,
                         MachineID remote_id);
  Cycles getOldestTimestamp();

  void setAbortFlag(int thread, Addr addr,
                    MachineID abortSource,
                    bool remote_trans = false,
                    bool capacity = false, bool wset = false);
  void cancelTransaction(int thread, PacketPtr pkt);
  bool isCancelledTransaction(int thread);

  void setAbortCause(HTMStats::AbortCause cause);
  void profileHtmFailureFaultCause(int thread,
                                   HtmFailureFaultCause cause);
  HtmCacheFailure getHtmTransactionalReqResponseCode(int thread);

  AnnotatedRegion_t getWaitForRetryRegionFromPreviousAbortCause(int thread);
  void setController(AbstractController *_ctrl)
  { m_controller = _ctrl; }

  void beginTransaction(int thread, int xid, PacketPtr pkt);
  bool canCommitTransaction(int thread, int xid, PacketPtr pkt) const;
  void initiateCommitTransaction(int thread, int xid, PacketPtr pkt);
  bool atCommit(int thread);
  void commitTransaction(int thread, int xid, PacketPtr pkt);
  void abortTransaction(int thread, PacketPtr pkt);
  Addr getAbortAddress(int thread);

  int getTransactionLevel(int thread);
  int getXID(int thread);
  void setXID(int thread, int xid); // Only for software transacitons
  bool isValidXID(int thread);

  bool inTransaction(int thread);
  void isolateTransactionLoad(int thread, Addr physicalAddr);
  void addToRetiredReadSet(int thread, Addr physicalAddr);
  bool inRetiredReadSet(int thread, Addr physicalAddr);
  void isolateTransactionStore(int thread, Addr physicalAddr);

  void profileCommitCycle();

  void profileTransactionAccess(bool miss, bool isWrite,
                                MachineType respondingMach,
                                Addr addr, Addr pc, int bytes);

  bool isAborting(int thread);
  bool isDoomed(int thread); // Aborting or bound to abort

  void setVersion(int version);
  int getVersion() const;

  void xactReplacement(Addr addr, MachineID source, bool capacity = false);

  bool checkReadSignature(Addr addr);
  bool checkWriteSignature(Addr addr);

  void notifyMissCompleted(bool isStore, bool remoteConflict) {}
  bool hasConflictWith(TransactionInterfaceManager *another);

  void redirectStoreToWriteBuffer(PacketPtr pkt);
  void bypassLoadFromWriteBuffer(PacketPtr pkt, DataBlock& datablock);
  void mergeDataFromWriteBuffer(PacketPtr pkt, DataBlock& datablock);

  bool isLogReady();
  bool isAccessToLog(Addr addr);
  bool isEndLogUnrollSignal(PacketPtr pkt);
  void setupLogTranslation(Addr vaddr, Addr paddr);
  Addr addLogEntry(Addr addr);
  void commitLogEntry(Addr addr);
  int getLogNumEntries(int thread);
  bool isUnrollingLog(int thread);
  void endLogUnroll(int thread);

  void beginEscapeAction(int thread);
  void endEscapeAction(int thread);
  bool inEscapeAction(int thread);

  void setStartCycle(Cycles startCycle);
  Cycles getStartCycle();

  std::string config_protocol() const {
      return m_ruby_system->getProtocol();
  }

  bool config_eagerCD() const {
      return m_htm->params().eager_cd;
  }

  std::string config_conflictResPolicy() const {
      return m_htm->params().conflict_resolution;
  }
  std::string config_lazyValidatedConflictResPolicy() const {
      return m_htm->params().lazy_validated_conf_res;
  }
  bool config_lazyVM() const {
      return m_htm->params().lazy_vm;
  }
  bool config_allowReadSetLowerLevelCacheEvictions() const {
      if (m_ruby_system->getProtocol() == "MESI_Two_Level_HTM_umu") {
          return m_htm->params().allow_read_set_l1_cache_evictions;
      } else {
          return m_htm->params().allow_read_set_l0_cache_evictions;
      }
  }
  bool config_allowReadSetL0CacheEvictions() const {
      return m_htm->params().allow_read_set_l0_cache_evictions;
  }
  bool config_allowReadSetL1CacheEvictions() const {
      return m_htm->params().allow_read_set_l1_cache_evictions;
  }
  bool config_nackL1LocalEvictions() const {
      return m_htm->params().nack_l1_local_evictions;
  }
  bool config_preciseReadSetTracking() const {
      return m_htm->params().precise_read_set_tracking;
  }
  bool config_replaceNonTransCandidatesPreferred() const {
      return m_htm->params().replace_nontrans_preferred;
  }
  bool config_enableValueChecker() const {
      return m_htm->params().value_checker;
  }
  std::vector<TransactionInterfaceManager*>
     getRemoteTransactionManagers() const;

  /********/
  static int numberofSMTThreads() { return 1; };

  int getProcID() const;


private:
  void discardWriteSetFromL1DataCache(int thread);

  const TransactionInterfaceManagerParams &_params;
  RubySystem *m_ruby_system;
  HTM * m_htm;
  int m_version;
  AbstractController *m_controller;
  TransactionalSequencer *m_sequencer;
  CacheMemory* m_dataCache_ptr;

  TransactionIsolationManager     * m_xactIsolationManager;
  TransactionConflictManager      * m_xactConflictManager;
  EagerTransactionVersionManager   * m_xactEagerVersionManager;
  LazyTransactionVersionManager   * m_xactLazyVersionManager;
  LazyTransactionCommitArbiter    * m_xactLazyCommitArbiter;

  int*      m_transactionLevel; // nesting depth, where outermost has depth 1
  int*      m_escapeLevel; // nesting depth, where outermost has depth 1
  int*      m_xid;
  int*      m_xidValid;
  bool*     m_abortFlag;
  bool*     m_unrollingLogFlag;
  bool*     m_atCommit;
  HTMStats::AbortCause *m_abortCause;
  bool*     m_abortSourceNonTransactional;
  HtmFailureFaultCause  *m_lastFailureCause;  // Cause of preceding abort
  bool*     m_capacityAbortWriteSet; // For capacity aborts, whether Wset/Rset
  Addr*     m_abortAddress;
  // Sanity checks
  std::map<Addr, char> m_writeSetDiscarded;

    Tick m_htmstart_tick;
    Counter m_htmstart_instruction;

    //! Histogram of cycle latencies of HTM transactions
    Stats::Histogram m_htm_transaction_cycles;
    //! Histogram of instruction lengths of HTM transactions
    Stats::Histogram m_htm_transaction_instructions;
    //! Causes for HTM transaction aborts
    Stats::Vector m_htm_transaction_abort_cause;
};

} // namespace ruby
} // namespace gem5

#endif



