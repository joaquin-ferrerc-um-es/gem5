/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#ifndef __MEM_RUBY_HTM_EAGERTRANSACTIONVERSIONMANAGER_HH__
#define __MEM_RUBY_HTM_EAGERTRANSACTIONVERSIONMANAGER_HH__

#include <map>
#include <vector>

#include "mem/packet.hh"
#include "mem/request.hh"
#include "mem/ruby/common/DataBlock.hh"
#include "mem/ruby/structures/CacheMemory.hh"

namespace gem5
{
namespace ruby
{

using namespace std;

class TransactionInterfaceManager;

class EagerTransactionVersionManager
{
public:
  EagerTransactionVersionManager(TransactionInterfaceManager *xact_mgr,
                                int version,
                                CacheMemory* dataCache_ptr);
  ~EagerTransactionVersionManager();

  void beginTransaction(int thread);
  void restartTransaction(int thread);
  void commitTransaction(int thread);
  bool isReady() const { return m_initStatus == LogInitStatus::Ready; };
  bool isAccessToLog(Addr addr) const;
  void setupLogTranslation(Addr vaddr, Addr paddr);
  void setLogBaseVirtualAddress(Addr addr);
  Addr getLogBaseVirtualAddress() const {
      assert(m_initStatus >= LogInitStatus::BaseAddress);
      return m_logBaseVAddr; };
  Addr addLogEntry(Addr addr);
  void commitLogEntry(Addr addr);
  int getLogNumEntries() const {
      // Number of entries that were actually written to the log
      // ("committed")
      assert(m_initStatus == LogInitStatus::Ready);
      return m_logNumCommittedEntries;
  };
  bool isEndLogUnrollSignal(PacketPtr pkt);

private:
  enum LogInitStatus
  {
      Invalid,
      BaseAddress,
      V2PTranslations,
      Ready
  };
public:
  Addr computeLogDataPointer(int numEntries) const;
  Addr translateLogAddress(Addr vaddr) const;
private:
  TransactionInterfaceManager *m_xact_mgr;
  int m_version;
  CacheMemory *m_dataCache_ptr;
  LogInitStatus m_initStatus = LogInitStatus::Invalid;
  Addr m_logBaseVAddr;
  std::map<Addr, Addr> m_logTLB;
  int m_logNumEntries = 0;
  int m_logNumCommittedEntries = 0;
  std::vector<Addr> m_addedLogDataPAddr;
};

} // namespace ruby
} // namespace gem5

#endif

