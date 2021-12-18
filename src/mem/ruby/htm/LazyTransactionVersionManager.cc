/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#include "mem/ruby/htm/LazyTransactionVersionManager.hh"

#include <iomanip>
#include <iostream>
#include <string>

#include "debug/RubyHTM.hh"
#include "debug/RubyHTMverbose.hh"
#include "mem/packet.hh"
#include "mem/ruby/common/DataBlock.hh"
#include "mem/ruby/htm/TransactionInterfaceManager.hh"
#include "mem/ruby/system/Sequencer.hh"

#define CLASS_NS LazyTransactionVersionManager::

namespace gem5
{
namespace ruby
{


CLASS_NS
LazyTransactionVersionManager(TransactionInterfaceManager *xact_mgr,
                              int version, CacheMemory* dataCache_ptr) {
    m_version = version;
    m_xact_mgr = xact_mgr;
    m_dataCache_ptr = dataCache_ptr;
    m_aborting = false;
    m_committed = false;
    m_committing = false;
    m_flushPending = false;
    m_shouldResumeFlush = false;
    m_requestorID = Request::invldRequestorId;
    int smt_threads = 1; // TODO

    m_issuedWriteBufferRequest = 0;
    m_writeBuffer.resize(smt_threads);
    m_writeBufferBlocks.resize(smt_threads);
}

CLASS_NS ~LazyTransactionVersionManager() {
}

void
CLASS_NS beginTransaction(int thread, PacketPtr pkt)
{
    m_committed = false;
    m_committing = false;
    m_requestorID = pkt->req->requestorId();
    assert(!m_aborting);
}

void
CLASS_NS restartTransaction(int thread){
    int transactionLevel = m_xact_mgr->getTransactionLevel(thread);
    assert(transactionLevel == 1);
    if (m_committing) {
        // Aborted while committing
        assert(m_aborting);
    }
    else {
        assert(!m_aborting);
    }
    m_committing = false;
    m_committed = false;
    m_aborting = false;
    assert(m_issuedWriteBufferRequest == 0);
    discardWriteBuffer(thread);
}

void
CLASS_NS notifyCommittedTransaction(int thread){
    int transactionLevel = m_xact_mgr->getTransactionLevel(thread);
    assert(transactionLevel == 1);
    _unused(transactionLevel);
    assert(m_committed);
    assert(m_committing);
    assert(!m_aborting);
    m_committing = false;
    m_committed = false;
    assert(m_issuedWriteBufferRequest == 0);
    assert(m_writeBufferBlocks[thread].empty());
    assert(m_writeBuffer[thread].empty());
}

void
CLASS_NS commitTransaction(int thread)
{
    int transactionLevel = m_xact_mgr->getTransactionLevel(thread);
    assert(transactionLevel == 1);
    _unused(transactionLevel);
    assert(!m_committed);
    m_committing = true;
    m_flushPending = false;
    m_shouldResumeFlush = false;

    flushWriteBuffer(thread);
}

int CLASS_NS getProcID() const{
    return m_version;
}

bool CLASS_NS existInWriteBuffer(int thread, Addr addr){
    int transactionLevel = m_xact_mgr->getTransactionLevel(thread);
    assert(transactionLevel == 1);

    return (m_writeBuffer[thread].find(addr) !=
            m_writeBuffer[thread].end());
}

uint8_t
CLASS_NS getDataFromWriteBuffer(int thread, Addr addr){
    int transactionLevel = m_xact_mgr->getTransactionLevel(thread);
    assert(transactionLevel == 1);
    assert(m_writeBuffer[thread].find(addr) !=
           m_writeBuffer[thread].end());
    return m_writeBuffer[thread][addr];
}

void
CLASS_NS addToWriteBuffer(int thread,
                          Addr addr, int size, uint8_t *data){

    int transactionLevel = m_xact_mgr->getTransactionLevel(thread);
    assert(transactionLevel == 1);

    for (int i = 0; i < size; i++){
        m_writeBuffer[thread][addr + i] = data[i];
    }
    // Ensure all data falls in the same block
    assert(makeLineAddress(addr) == makeLineAddress(addr + size - 1));

    m_writeBufferBlocks[thread][makeLineAddress(addr)] = Pending;
    uint64_t value = 0;
    _unused(value);
    switch(size) {
    case sizeof(uint8_t):
        value = (uint64_t)*((uint8_t *)data);
        break;
    case sizeof(uint16_t):
        value = (uint64_t)*((uint16_t *)data);
        break;
    case sizeof(uint32_t):
        value = (uint64_t)*((uint32_t *)data);
        break;
    case sizeof(uint64_t):
        value = (uint64_t)*((uint64_t *)data);
        break;
    default:
        panic("Unsupported packet size\n");
    }
    DPRINTF(RubyHTMverbose,
            "Adding to write buffer,"
            " addr %#x size %d value %#x\n",
            addr, size, value);
}

vector<uint8_t>
CLASS_NS forwardData(int thread, Addr addr, int size,
                     DataBlock& cacheBlock, bool& forwarding){

    vector<uint8_t> data;
    forwarding = false;
    int transactionLevel = m_xact_mgr->getTransactionLevel(thread);
    if (transactionLevel == 0) {
        // The O3CPU may issue speculative transactional loads that
        // reach memory before xbegin has retired
        // No forwarding in this case
        return data;
    }

    assert(transactionLevel > 0);

    data.resize(size);
    uint8_t buffer[64];

    for (int i = 0; i < size; i++){
        if (existInWriteBuffer(thread, addr + i)){
            data[i] = getDataFromWriteBuffer(thread, addr + i);
            forwarding = true;
        } else {
            data[i] = *(cacheBlock.getData(getOffset(addr + i), 1));
        }
        buffer[i] = data[i];
    }
    if (forwarding) {
        uint64_t value = 0;
        _unused(value);
        switch(size) {
        case sizeof(uint8_t):
            value = (uint64_t)*((uint8_t *)buffer);
            break;
        case sizeof(uint16_t):
            value = (uint64_t)*((uint16_t *)buffer);
            break;
        case sizeof(uint32_t):
            value = (uint64_t)*((uint32_t *)buffer);
            break;
        case sizeof(uint64_t):
            value = (uint64_t)*((uint64_t *)buffer);
            break;
        default:
            panic("Unsupported packet size\n");
        }
        DPRINTF(RubyHTMverbose,
                "Forwarding from write buffer,"
                " addr %#x size %d value %#x\n",
                addr, size, value);
    }

    return data;
}

void
CLASS_NS flushWriteBuffer(int thread){
    int transactionLevel = m_xact_mgr->getTransactionLevel(thread);
    assert(transactionLevel == 1);
    assert(!m_aborting);

    if (m_writeBufferBlocks[thread].empty()) {
        m_committed = true;
        assert(m_issuedWriteBufferRequest == 0);
        return;
    }
    for (map<Addr,WriteBufferBlockStatus>::iterator it =
             m_writeBufferBlocks[thread].begin();
         it != m_writeBufferBlocks[thread].end();
         ++it) {
        if (m_issuedWriteBufferRequest ==
            m_xact_mgr->config_lazyCommitWidth()) {
            m_flushPending = true;
            break;
        };
        WriteBufferBlockStatus status =(*it).second;
        if (status == Pending) {
            Addr addr =(*it).first;

            // Create request and packet
            Request::Flags flags;
            assert(m_requestorID != Request::invldRequestorId);
            RequestPtr req = std::make_shared<Request>(
                         addr, RubySystem::getBlockSizeBytes(),
                         flags,  m_requestorID);
            PacketPtr pkt =  new Packet(req, MemCmd::WriteReq,
                                        RubySystem::getBlockSizeBytes());
            pkt->allocate();
            uint64_t uid = 0; // TODO getHtmTransactionUid()
            pkt->setHtmTransactional(uid);

            RequestStatus requestStatus =
                m_xact_mgr->getSequencer()->makeRequest(pkt);
            if (requestStatus != RequestStatus_Issued) {
                DPRINTF(RubyHTM, "Write buffer flush pending, makeRequest"
                        " could not issue request paddr %#x\n", addr);
                m_flushPending = true;
                continue; // Try to issue request for another block
            }
            // Mark as issued
            (*it).second = Issued;
            ++m_issuedWriteBufferRequest;
        }
    }
    if (m_flushPending) {
        // Ensure there are issued reqs that will wake up the flush
        // again when calling mergedDataFromWriteBuffer
        for (map<Addr,WriteBufferBlockStatus>::iterator it =
                 m_writeBufferBlocks[thread].begin();
             it != m_writeBufferBlocks[thread].end();
             ++it) {
            WriteBufferBlockStatus status =(*it).second;
            if (status == Issued) {
                // OK: At least one issued request will wake us up
                return;
            }
        }
        // Set this flag, sequencer must check it after hitcallback
        // and resume flush
        m_shouldResumeFlush = true;
        DPRINTF(RubyHTM, "Write buffer flush paused, "
                " Sequencer will try to resume it upon callback\n");
    }

}

void
CLASS_NS mergeDataFromWriteBuffer(int thread, Addr address, DataBlock& data)
{
    int transactionLevel = m_xact_mgr->getTransactionLevel(thread);
    assert(transactionLevel == 1);

    // Block address must exist
    assert(m_writeBufferBlocks[thread].find(address) !=
           m_writeBufferBlocks[thread].end());
    // Request for this block has been issued
    WriteBufferBlockStatus status = m_writeBufferBlocks[thread][address];
    assert(status == Issued);
    _unused(status);
    int mergedBytes = 0;
    DPRINTF(RubyHTMverbose, "Data block before merge, addr %#x:\n%s\n",
            address, data.toString());
    assert(address == makeLineAddress(address));
    map<Addr,uint8_t>::iterator it;
    for (int i=0; i < RubySystem::getBlockSizeBytes(); ++i) {
        Addr byteAddr = Addr(address+i);
        it = m_writeBuffer[thread].find(byteAddr);
        if (it != m_writeBuffer[thread].end()) {
            // Merge byte
            assert(byteAddr =(*it).first);
            uint8_t val =(*it).second;
            int offset = getOffset(byteAddr);
            data.setData(&val, offset, 1);
            DPRINTF(RubyHTMverbose, "Merged byte val %#x at addr"
                    " %#x (offset %#x) from write buffer into"
                    " block paddr %#x\n",
                    val, byteAddr, offset, address);
            mergedBytes++;
        }
    }
    DPRINTF(RubyHTMverbose, "Data block after merge, addr %#x:\n%s\n",
            address, data.toString());
    // Delete block address from write buffer blocks
    m_writeBufferBlocks[thread].erase(address);
    --m_issuedWriteBufferRequest;
    DPRINTF(RubyHTM, "Merged %d bytes from write buffer"
            " into block paddr %#x\n",
            mergedBytes, address);

    // Check if we are done flushing the write buffer
    if (m_writeBufferBlocks[thread].empty()) {
        if (m_aborting) {
            DPRINTF(RubyHTM, "Write buffer flush terminated"
                    " prematurely due to abort\n");
        }
        else {
            DPRINTF(RubyHTM, "Write buffer flush completed\n");
        }
        m_committed = true; // Now canCommitTransaction will be true,
                            // allowing commit/abort to complete
        // Discard write buffer after all contents merged into
        // cache blocks
        m_writeBuffer[thread].clear();
    }
    else {
        // If flush pending due to too many outstanding misses, resume
        if (m_aborting) {
            DPRINTF(RubyHTM, "Write buffer flush will be cancelled once all"
                    " outstanding writes complete, transaction is aborting\n");
        }
        else if (m_flushPending) {
            m_flushPending = false;
            DPRINTF(RubyHTM, "Write buffer flush resumed"
                    " after miss completed\n");
            flushWriteBuffer(thread);
        }
    }
}

void
CLASS_NS cancelWriteBufferFlush(int thread)
{
    assert(m_committing);
    assert(!m_aborting);
    m_aborting = true;
    int transactionLevel = m_xact_mgr->getTransactionLevel(thread);
    assert(transactionLevel == 1);
    map<Addr,WriteBufferBlockStatus>::iterator it =
        m_writeBufferBlocks[thread].begin();
    for (auto next_it = it;
         it != m_writeBufferBlocks[thread].end(); it = next_it) {
        ++next_it;
        WriteBufferBlockStatus status =(*it).second;
        if (status == Pending) {
            Addr addr = (*it).first;
            DPRINTF(RubyHTM, "Cancelled pending "
                    "write buffer flush block paddr %#x\n",
                    addr);
            bool found = m_writeBufferBlocks[thread].erase(addr);
            assert(found);
        }
    }
}

void
CLASS_NS discardWriteBuffer(int thread){
    int transactionLevel = m_xact_mgr->getTransactionLevel(thread);
    assert(transactionLevel == 1);

    m_writeBuffer[thread].clear();
    m_writeBufferBlocks[thread].clear();
    DPRINTF(RubyHTM, "Discarding contents of write buffer upon abort\n");
}

} // namespace ruby
} // namespace gem5
