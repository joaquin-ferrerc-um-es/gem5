/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#ifndef __MEM_RUBY_HTM_TRANSACTIONISOLATIONMANAGER_HH__
#define __MEM_RUBY_HTM_TRANSACTIONISOLATIONMANAGER_HH__

#include <map>
#include <vector>

#include "mem/ruby/common/Address.hh"
#include "mem/ruby/htm/filters/AbstractBloomFilter.hh"

namespace gem5
{
namespace ruby
{

class TransactionIsolationManager {
public:
  TransactionIsolationManager(TransactionInterfaceManager *xact_mgr,
                              int version);
  ~TransactionIsolationManager();

  void beginTransaction(int thread);
  void commitTransaction(int thread);
  void abortTransaction(int thread, int new_xact_level);
  void releaseIsolation(int thread, int xact_level);
  void releaseReadIsolation(int thread);
  void validateTransaction(int thread, int xact_level);

  /* These functions query the perfect filter */
  bool isInReadSetPerfectFilter(int thread, Addr addr);
  bool isInWriteSetPerfectFilter(int thread, Addr addr);
  void addToReadSetPerfectFilter(int thread, Addr addr,
                                 int transactionLevel = 1);
  void removeFromReadSetPerfectFilter(int thread, Addr addr);
  void removeFromWriteSetPerfectFilter(int thread, Addr addr);
  void addToWriteSetPerfectFilter(int thread, Addr addr, int transactionLevel);
  void clearReadSetPerfectFilter(int thread, int transactionLevel = 1);
  void clearWriteSetPerfectFilter(int thread, int transactionLevel);
  void setFiltersToXactLevel(int thread, int new_xact_level,
                             int old_xact_level);

    /* These functions are used to manipulate the Read/Write Bloom filters */
  bool isInReadSetFilter(int thread, Addr addr);
  bool isInWriteSetFilter(int thread, Addr addr);
  bool isInReadSetFilterSummary(Addr addr);
  bool isInWriteSetFilterSummary(Addr addr);
  void addToReadSetFilter(int thread, Addr addr);
  void addToWriteSetFilter(int thread, Addr addr);
  void clearReadSetFilter(int thread);
  void clearWriteSetFilter(int thread);
  int getTotalReadSetCount(int thread);
  int getTotalWriteSetCount(int thread);
  AbstractBloomFilter * getReadSetFilter(int thread);
  AbstractBloomFilter * getWriteSetFilter(int thread);

  bool inRetiredReadSet(int thread, Addr addr); // Profiling
  void addToRetiredReadSet(int thread, Addr addr);
  bool wasOvertakingRead(int thread, Addr addr); // Sanity checks

  bool hasConflictWith(TransactionIsolationManager* another);
  void redirectedStoreToWriteBuffer(Addr addr);
  bool isRedirectedStoreToWriteBuffer(Addr addr);

  void profileReadSetFilterActivity(int xid, int thread, bool isCommit);
  void profileWriteSetFilterActivity(int xid, int thread, bool isCommit);

  int  getReadSetSize(int thread, int xact_level);
  int  getWriteSetSize(int thread, int xact_level);

  std::vector<Addr> *getReadSet(int thread, int xact_level = 1);
  std::vector<Addr> *getWriteSet(int thread, int xact_level = 1);

  void setVersion(int version);
  int getVersion() const;

private:
  int getProcID() const;
  int getLogicalProcID(int thread) const;

  TransactionInterfaceManager *m_xact_mgr;
  int m_version;

        Addr m_summaryConflictAddress;
        unsigned int m_summaryConflictType;

  std::vector< std::vector< std::map<Addr, char> > > m_readSet;
  std::vector< std::vector< std::map<Addr, char> > > m_writeSet;

  std::vector<AbstractBloomFilter*>  m_readSetFilter;
  std::vector<AbstractBloomFilter*>  m_writeSetFilter;

  // Lazy VM ideal write buffer
  std::map<Addr, char> m_writeSetInWriteBuffer;

  std::vector< std::vector<int> > m_xact_readCount;
  std::vector< std::vector<int> > m_xact_writeCount;
  std::vector< std::vector<int> > m_xact_overflow_readCount;
  std::vector< std::vector<int> > m_xact_overflow_writeCount;
};

} // namespace ruby
} // namespace gem5

#endif

