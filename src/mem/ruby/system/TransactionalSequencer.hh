#ifndef __MEM_RUBY_SYSTEM_TRANSACTIIONALSEQUENCER_HH__
#define __MEM_RUBY_SYSTEM_TRANSACTIIONALSEQUENCER_HH__

#include <cassert>
#include <iostream>

#include "mem/htm.hh"
#include "mem/ruby/profiler/annotated_regions.h"
#include "mem/ruby/system/RubyPort.hh"
#include "mem/ruby/system/Sequencer.hh"
#include "params/RubyTransactionalSequencer.hh"


namespace gem5
{

namespace ruby
{

class TransactionInterfaceManager;


class TransactionalSequencer : public Sequencer
{
  public:
    PARAMS(RubyTransactionalSequencer);
    TransactionalSequencer(const Params &p);
    ~TransactionalSequencer();

    void print(std::ostream& out) const override;

        /* HTM extensions */
    RequestStatus insertRequest(PacketPtr pkt,
                                RubyRequestType primary_type,
                                RubyRequestType secondary_type) override;
    RequestStatus makeRequest(PacketPtr pkt);
    void setTransactionManager(TransactionInterfaceManager* xact_mgr);
    void setController(AbstractController* _cntrl);

    void failedCallback(Addr address, DataBlock& data,
                        Cycles remote_timestamp,
                        MachineID nacker, bool write);

  private:
    // Private copy constructor and assignment operator
    TransactionalSequencer(const TransactionalSequencer& obj);
    TransactionalSequencer& operator=(const TransactionalSequencer& obj);

    void abortTransaction(PacketPtr pkt);
    void rubyHtmCallback(PacketPtr pkt);
    void hitCallback(SequencerRequest* srequest, DataBlock& data,
                     bool llscSuccess,
                     const MachineType mach, const bool externalHit,
                     const Cycles initialRequestTime,
                     const Cycles forwardRequestTime,
                     const Cycles firstResponseTime,
                     const bool was_coalesced) override;

    bool isWriteBufferHitEventScheduled() const
    { return writeBufferHitEvent.scheduled(); }
    bool notifyXactionEvent(PacketPtr pkt);
    void handleTransactionalRead(SequencerRequest *request,
                                   DataBlock& data, bool externalHit,
                                         const MachineType respondingMach);
    void handleTransactionalWrite(SequencerRequest *request,
                                  DataBlock& data, bool externalHit,
                                  const MachineType respondingMach);

    // HTM support
    HTM * m_htm;
    TransactionInterfaceManager* m_xact_mgr;
    // LL (lazy CD) support

    // WriteBufferHitEvent models access latency of the transactional
    // write buffer
    void writeBufferEvent(PacketPtr _pkt);
    bool m_commitPending;
    bool m_failedCallback;
    bool m_stalled;
    uint64_t m_lastAbortHtmUid;

    // Lazy-lazy HTM:
    class WriteBufferHitEvent : public Event
    {
      private:
        TransactionalSequencer *m_sequencer_ptr;
        PacketPtr m_pkt;

      public:
        WriteBufferHitEvent(TransactionalSequencer *_seq) :
            m_sequencer_ptr(_seq), m_pkt(NULL) {}
        void setPacket(PacketPtr _pkt) {
            assert(m_pkt ==  NULL);
            m_pkt = _pkt;
        }
        void clearPacket() {
            assert(m_pkt !=  NULL);
            m_pkt = NULL;
        }
        void process() {
            m_sequencer_ptr->writeBufferEvent(m_pkt);
        }
    };
    WriteBufferHitEvent writeBufferHitEvent;
};


} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_SYSTEM_TRANSACTIIONALSEQUENCER_HH__
