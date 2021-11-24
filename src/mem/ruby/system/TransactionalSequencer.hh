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

// LogTM
struct LogRequestInfo
{
    PacketPtr mainPkt = NULL; // Program store
    PacketPtr logAddrPkt = NULL;
    PacketPtr logDataPkt = NULL;
    RequestStatus logAddrPktStatus = RequestStatus_NULL;
    RequestStatus logDataPktStatus = RequestStatus_NULL;
    //WriteCallbackArgs callbackArgs;
    /** Number of outstanding associated access to complete (LogTM) */
    int outstanding = 2; // log addr + log data + program store
    LogRequestInfo(PacketPtr _mainPkt,
                   PacketPtr _logAddrPkt,
                   PacketPtr _logDataPkt)
        : mainPkt(_mainPkt), logAddrPkt(_logAddrPkt),
          logDataPkt(_logDataPkt)
          //callbackArgs()
    {}
};

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
    void writeCallback(Addr address,
                       DataBlock& data,
                       const bool externalHit = false,
                       const MachineType mach = MachineType_NUM,
                       const Cycles initialRequestTime = Cycles(0),
                       const Cycles forwardRequestTime = Cycles(0),
                       const Cycles firstResponseTime = Cycles(0),
                       const bool noCoales = false) override;

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
    LogRequestInfo buildLogPackets(PacketPtr mainPkt);
    void handleStoresToLog(Addr address, PacketPtr pkt,
                        DataBlock& data);
    void handleLoggedStore(Addr address,
                           SequencerRequest& request,
                           DataBlock& data);
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

    // LogTM (eager VM) RequestTable contains outstanding log requests
    // for pending program stores (per line address)
    std::unordered_map<Addr, std::list<LogRequestInfo>> m_logRequestTable;

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
