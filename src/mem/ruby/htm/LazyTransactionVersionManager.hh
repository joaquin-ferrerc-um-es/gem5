/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#ifndef __MEM_RUBY_HTM_LAZYTRANSACTIONVERSIONMANAGER_HH__
#define __MEM_RUBY_HTM_LAZYTRANSACTIONVERSIONMANAGER_HH__

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

class LazyTransactionVersionManager {
public:
  LazyTransactionVersionManager(TransactionInterfaceManager *xact_mgr,
                                int version,
                                CacheMemory* dataCache_ptr);
  ~LazyTransactionVersionManager();


  void addToWriteBuffer(Addr addr, int size, uint8_t *data);
  vector<uint8_t> forwardData(Addr addr, int size,
                              DataBlock& data, bool& forwarding);
  void flushWriteBuffer();
  void discardWriteBuffer();

  void beginTransaction(PacketPtr pkt);
  void restartTransaction();
  void commitTransaction();
  bool committed() { return m_committed; };
  bool committing() { return m_committing; };
  void notifyCommittedTransaction();
  void mergeDataFromWriteBuffer(Addr address, DataBlock& data);
  void cancelWriteBufferFlush();

private:
  enum WriteBufferBlockStatus {
      Pending,
      Issued,
      Cancelled
  };
  int getProcID() const;

  bool existInWriteBuffer(Addr addr);
  uint8_t getDataFromWriteBuffer(Addr addr);

  map<Addr, uint8_t> m_writeBuffer;
  map<Addr, WriteBufferBlockStatus> m_writeBufferBlocks;
  bool m_committed;
  bool m_committing;
  bool m_flushPending;
  bool m_aborting;

  Addr m_issuedWriteBufferRequest;

  TransactionInterfaceManager *m_xact_mgr;
  int m_version;
  CacheMemory *m_dataCache_ptr;
  RequestorID  m_requestorID;
  uint64_t m_currentHtmUid = 0;
};

} // namespace ruby
} // namespace gem5

#endif

