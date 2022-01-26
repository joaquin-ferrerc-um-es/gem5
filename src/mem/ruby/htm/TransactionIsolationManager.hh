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

namespace gem5
{
namespace ruby
{

class TransactionIsolationManager {
public:
  TransactionIsolationManager(TransactionInterfaceManager *xact_mgr,
                              int version);
  ~TransactionIsolationManager();

  void beginTransaction();
  void commitTransaction();
  void abortTransaction(int new_xact_level);
  void releaseIsolation(int xact_level);
  void releaseReadIsolation();

  /* These functions query the perfect filter */
  bool isInReadSetPerfectFilter(Addr addr);
  bool isInWriteSetPerfectFilter(Addr addr);
  void addToReadSetPerfectFilter(Addr addr,
                                 int transactionLevel = 1);
  void removeFromReadSetPerfectFilter(Addr addr);
  void removeFromWriteSetPerfectFilter(Addr addr);
  void addToWriteSetPerfectFilter(Addr addr, int transactionLevel);
  void clearReadSetPerfectFilter(int transactionLevel = 1);
  void clearWriteSetPerfectFilter(int transactionLevel);
  void setFiltersToXactLevel(int new_xact_level,
                             int old_xact_level);

  bool inRetiredReadSet(Addr addr); // Profiling
  void addToRetiredReadSet(Addr addr);
  bool wasOvertakingRead(Addr addr); // Sanity checks

  bool hasConflictWith(TransactionIsolationManager* another);
  void redirectedStoreToWriteBuffer(Addr addr);
  bool isRedirectedStoreToWriteBuffer(Addr addr);

  int  getReadSetSize(int xact_level);
  int  getWriteSetSize(int xact_level);

  std::vector<Addr> *getReadSet(int xact_level = 1);
  std::vector<Addr> *getWriteSet(int xact_level = 1);

  void setVersion(int version);
  int getVersion() const;

private:
  int getProcID() const;

  TransactionInterfaceManager *m_xact_mgr;
  int m_version;

  std::vector< std::map<Addr, char> > m_readSet;
  std::vector< std::map<Addr, char> > m_writeSet;

  // Lazy VM ideal write buffer
  std::map<Addr, char> m_writeSetInWriteBuffer;

  std::vector<int> m_xact_readCount;
  std::vector<int> m_xact_writeCount;
  std::vector<int> m_xact_overflow_readCount;
  std::vector<int> m_xact_overflow_writeCount;
};

} // namespace ruby
} // namespace gem5

#endif

