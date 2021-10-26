/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#ifndef __MEM_RUBY_HTM_XACTVALUECHECKER_HH__
#define __MEM_RUBY_HTM_XACTVALUECHECKER_HH__

#include <map>
#include <vector>

#include "mem/ruby/common/Address.hh"
#include "mem/ruby/common/DataBlock.hh"
#include "mem/ruby/structures/CacheMemory.hh"

namespace gem5
{
namespace ruby
{

using namespace std;

typedef map<Addr, DataBlock> WriteSetValueMap;

class XactValueChecker {
public:
  XactValueChecker();
  ~XactValueChecker();
  void registerSequencer(int proc);

  void notifyWrite(int proc, bool trans, Addr addr,
                   int size, uint8_t *data_ptr);

  void restartTransaction(int proc);
  void commitTransaction(int proc, TransactionInterfaceManager *xact_mgr,
                         CacheMemory *dataCache_ptr);
  bool xactValueCheck(int thread, Addr addr, int size,
                      const uint8_t *ptr);
private:
  uint8_t readGlobalValue(Addr addr);
  bool existGlobalValue(Addr addr);
  void writeGlobalValue(Addr addr, uint8_t value);

  bool existBlockInWriteBuffer(int proc, Addr addr);
  void discardWriteBuffer(int proc);
  bool existInWriteBuffer(int proc, Addr addr);
  uint8_t getDataFromWriteBuffer(int proc, Addr addr);

  map<Addr, uint8_t> m_xact_data;
  vector< map<Addr, uint8_t> > m_writeBuffer;
  vector< map<Addr, uint8_t> > m_writeBufferBlocks;
  vector< int > m_writeBufferSizes;

};

} // namespace ruby
} // namespace gem5

#endif

