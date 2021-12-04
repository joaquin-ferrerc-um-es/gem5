/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#ifndef __MEM_RUBY_HTM_TRANSACTIONCONFLICTMANAGER_HH__
#define __MEM_RUBY_HTM_TRANSACTIONCONFLICTMANAGER_HH__

#include <map>

#include "mem/ruby/common/Address.hh"
#include "mem/ruby/common/MachineID.hh"
#include "mem/ruby/htm/TransactionInterfaceManager.hh"

namespace gem5
{
namespace ruby
{

using namespace std;

class TransactionConflictManager {
public:
  TransactionConflictManager(TransactionInterfaceManager *xact_mgr,
                             int version);
  ~TransactionConflictManager();

  void beginTransaction(int thread);
  void commitTransaction(int thread);
  void restartTransaction(int thread);

  bool shouldNackLoad(Addr addr,
                      MachineID remote_id,
                      Cycles remote_timestamp,
                      bool remote_trans);
  bool shouldNackStore(Addr addr,
                       MachineID remote_id,
                       Cycles remote_timestamp,
                       bool remote_trans,
                       bool local_is_exclusive);

  bool possibleCycle(int thread);
  void setPossibleCycle(int thread);
  void clearPossibleCycle(int thread);
  bool nackReceived(int thread);
  bool doomed(int thread);
  void setDoomed(int thread);

  void notifySendNack(Addr physicalAddress, Cycles remote_timestamp,
                      MachineID remote_id);
  void notifyReceiveNack(Addr addr, Cycles remote_timestamp,
                         MachineID remote_id);
  bool hasHighestPriority();

  Cycles getTimestamp(int thread);
  Cycles getOldestTimestamp();
  bool isRequesterStallsPolicy();
  Addr getNackedPossibleCycleAddr(int thread) {
      assert(isRequesterStallsPolicy());
      assert(m_sentNack[thread]);
      return m_sentNackAddr[thread]; };

  int getNumRetries(int thread);

  void setVersion(int version);
  int getVersion() const;
  bool isRemoteOlder(int thread, int remote_thread, Cycles local_timestamp,
                     Cycles remote_timestamp,
                     MachineID remote_id);

private:
  int getProcID() const;
  int getLogicalProcID(int thread) const;

  TransactionInterfaceManager *m_xact_mgr;
  int m_version;

  Cycles    *m_timestamp;
  bool   *m_possible_cycle;
  bool   *m_lock_timestamp;
  int    *m_numRetries;
  bool   *m_receivedNack;
  bool   *m_sentNack;
  Addr   *m_sentNackAddr;
  bool   *m_doomed;
  std::string    m_policy;
  std::string    m_lazy_validated_policy;
  bool m_policy_is_req_stalls_cda;
  bool m_policy_nack_non_transactional;
};

} // namespace ruby
} // namespace gem5

#endif


