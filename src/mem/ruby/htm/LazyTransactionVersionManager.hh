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


  void addToWriteBuffer(int thread, Addr addr, int size, uint8_t *data);
  vector<uint8_t> forwardData(int thread, Addr addr, int size,
                              DataBlock& data, bool& forwarding);
  void flushWriteBuffer(int thread);
  void discardWriteBuffer(int thread);

  void beginTransaction(int thread, PacketPtr pkt);
  void restartTransaction(int thread);
  void commitTransaction(int thread);
  bool committed() { return m_committed; };
  bool committing() { return m_committing; };
  void notifyCommittedTransaction(int thread);
  void mergeDataFromWriteBuffer(int thread, Addr address, DataBlock& data);
  void cancelWriteBufferFlush(int thread);
  bool shouldResumeFlush() { return m_shouldResumeFlush; };

private:
  enum WriteBufferBlockStatus {
      Pending,
      Issued,
      Cancelled
  };
  int getProcID() const;
  int getLogicalProcID(int thread) const;

  void takeCheckpoint(int thread);
  bool existInWriteBuffer(int thread, Addr addr);
  uint8_t getDataFromWriteBuffer(int thread, Addr addr);

  /** Contents of write buffer (byte-addressable) */
  vector < map<Addr, uint8_t> > m_writeBuffer;
    /** Block addresses in write buffer, set to true when write
        request issued during lazy commit */
  vector < map<Addr, WriteBufferBlockStatus> > m_writeBufferBlocks;
  bool m_committed;
  bool m_committing;
  bool m_flushPending;
  bool m_shouldResumeFlush;
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

