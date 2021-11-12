#include "mem/ruby/system/TransactionalSequencer.hh"

#include "arch/x86/ldstflags.hh"
#include "debug/ProtocolTrace.hh"
#include "debug/RubyHTM.hh"
#include "debug/RubyHTMverbose.hh"
#include "debug/RubyPort.hh"
#include "mem/ruby/htm/TransactionInterfaceManager.hh"
#include "mem/ruby/htm/XactValueChecker.hh"
#include "mem/ruby/profiler/Profiler.hh"
#include "mem/ruby/profiler/XactProfiler.hh"
#include "mem/ruby/protocol/HtmFailedInCacheReason.hh"
#include "mem/ruby/slicc_interface/RubySlicc_Util.hh"
#include "sim/system.hh"


namespace gem5
{

namespace ruby
{

TransactionalSequencer::TransactionalSequencer(const Params &p)
    : Sequencer(p),
      m_commitPending(false),
      m_failedCallback(false),
      m_stalled(false),
      writeBufferHitEvent(this)

{
    // TransactionalSequencer is only used by UMU protocols
    assert(m_ruby_system->getProtocol() == "MESI_Two_Level_HTM_umu" ||
           m_ruby_system->getProtocol() == "MESI_Three_Level_HTM_umu");;
    m_ruby_system->getXactValueChecker()->registerSequencer(getId());
    m_htm = system->getHTM();
    assert(m_htm);
    assert(m_ruby_system->getProfiler()->hasXactProfiler());
}

TransactionalSequencer::~TransactionalSequencer()
{
}

void
TransactionalSequencer::print(ostream& out) const
{
    Sequencer::print(out);

    out << "+ [TransactionalSequencer: " << m_version
        << "]";
}

void
TransactionalSequencer::setController(AbstractController* _cntrl) 
{
    m_controller = _cntrl;
    assert(m_xact_mgr);
    m_xact_mgr->setController(m_controller);
}


void
TransactionalSequencer::
setTransactionManager(TransactionInterfaceManager* xact_mgr)
{
  m_xact_mgr = xact_mgr;
}

/* HTM extensions */
void
TransactionalSequencer::abortTransaction(PacketPtr pkt)
{
    int thread = 0;
    m_stalled = false;
    m_xact_mgr->abortTransaction(thread, pkt);
    m_lastAbortHtmUid = pkt->getHtmTransactionUid();
}

bool
TransactionalSequencer::notifyXactionEvent(PacketPtr pkt)
{

  uint64_t xid = pkt->getAddr(), thread = 0;

  if (m_xact_mgr->isAborting(thread)) {
      // Only abort command accepted
      if (pkt->req->isHTMAbort()) {
          DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s \n",
                   curTick(), m_version, "Seq",
                   "HTM_ABORT" , "", "");
          DPRINTF(RubyHTM, "HTM_ABORT\n");
          abortTransaction(pkt);
      }
      else {
          // Other commands (e.g. HTM_COMMIT) get ignored if the abort
          // flag is found set. rubyHtmCallback next turns around
          // packet and notifies CPU that transaction has failed via
          // response code (see getHtmTransactionalReqResponseCode)
          // NOTE: The tcommit instruction appears as committed since
          // the HTM fault is triggered after the instruction retires,
          DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s %s \n",
                   curTick(), m_version, "Seq",
                   "HTM_CMD" , "", "",  "(abort flag set)");
          if (m_commitPending) {
              assert(pkt->req->isHTMCommit());
              if (m_xact_mgr->canCommitTransaction(thread, xid, pkt)) {
                  // All commit actions completed (e.g. pending write
                  // misses), now rubyHtmCallback will signal abort
                  // via getHtmTransactionalReqResponseCode
                  m_commitPending = false;
              }
          }
      }
      return true;
  }
  if (pkt->req->isHTMStart()) {
      DPRINTF(RubyHTM, "HTM_BEGIN\n");
      m_xact_mgr->beginTransaction(thread, xid, pkt);
      DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s \n",
               curTick(), m_version, "Seq",
               "HTM_START" , "", "");
  } else if (pkt->req->isHTMCommit()) {
      DPRINTF(RubyHTM, "HTM_COMMIT\n");
      // Store value returned by canCommit, used to signal CPU whether
      // xend must fault. Prevent calling canCommit again after
      // initiateCommitTransaction since it changes the returned value
      if (m_xact_mgr->canCommitTransaction(thread, xid, pkt)) {
          m_xact_mgr->commitTransaction(thread, xid, pkt);
          DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s \n",
                   curTick(), m_version, "Seq",
                   "HTM_COMMIT", "", "");
          m_commitPending = false;
      } else {
          m_xact_mgr->initiateCommitTransaction(thread, xid, pkt);
          m_commitPending = true;
          DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s \n",
                   curTick(), m_version, "Seq",
                   "HTM_COMMIT_PENDING" , "", "");
      }
  } else if (pkt->req->isHTMCancel()) {
      // Explicit abort originated from a user instruction
      // (xabort/tcancel)
      DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s \n",
               curTick(), m_version, "Seq",
               "HTM_CANCEL" , "", "");
      assert(m_xact_mgr->getProcID() == m_version);
      m_xact_mgr->cancelTransaction(thread, pkt);
  } else if (pkt->req->isHTMAbort()) {
      // CPU may only trigger HTM_ABORT before Ruby has set abort flag
      // for aborts whose cause is LSQ conflict or exception/interrupt
      if ((pkt->req->getHtmAbortCause() ==
           HtmFailureFaultCause::LSQ) ||
          (pkt->req->getHtmAbortCause() ==
           HtmFailureFaultCause::EXCEPTION) ||
          (pkt->req->getHtmAbortCause() ==
           HtmFailureFaultCause::INTERRUPT) ||
          (pkt->req->getHtmAbortCause() ==
           HtmFailureFaultCause::DISABLED)) {
          DPRINTF(RubyHTM, "HTM_ABORT due to %s\n",
                  htmFailureToStr(pkt->req->getHtmAbortCause()));
      } else {
          panic("HTM_ABORT must find abort flag set!\n");
      }
      DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s \n",
               curTick(), m_version, "Seq",
               "HTM_ABORT" , "", "");
      abortTransaction(pkt);
  } else if (pkt->req->isHTMIsolate()) {
      Addr addr = makeLineAddress(pkt->getAddr());
      int thread = 0;
      if (!m_htm->params().precise_read_set_tracking) {
          /* With imprecise read sets, HTM_ISOLATE signal must always
             find the block already in the read set. This signal will
             simply add to the "retired read set", for profiling.

             NOTE: in O3CPU, store-to-load forwarding may lead to
             loads never accessing cache, so in this case the
             preceding store in program order adds the block to the
             write set, but the store may not have performed in cache
             yet..
          */
          if (!m_xact_mgr->checkReadSignature(addr)) {
              DPRINTF(RubyHTMverbose,
                      "Committed load to %#x (%#x) but block "
                      "does not belong to read set (store-to-load"
                      "forwarding?)\n",
                      pkt->getAddr(),
                      makeLineAddress(pkt->getAddr()));
              panic("Unexpected HTM_ISOLATE! (ST2LD forwarding?) \n");
          }
      }
      if (m_xact_mgr->checkReadSignature(addr)) {
          DPRINTF(RubyHTMverbose,
                  "Load to %#x (%#x) already in read set\n",
                  pkt->getAddr(),
                  makeLineAddress(pkt->getAddr()));
      } else {
          assert(m_htm->params().precise_read_set_tracking);
          /* With precise_read_set_tracking, transactional loads are
           * isolated (added to the read-set) when the load retires
           * from the processor via HTM_ISOLATE signal. If disabled,
           * the SR bit is set when the load executes, which may
           * "imprecisely" set SR bits for blocks targeted by loads
           * coming from mispredicted paths.
           */
          m_xact_mgr->isolateTransactionLoad(thread, addr);
          // With precise read sets, if block not in the read set this
          // far, then it cannot be part of retired read set
          assert(!m_xact_mgr->inRetiredReadSet(thread, addr));
          DPRINTF(RubyHTM,
                  "Committed load to %#x (%#x) adding block"
                  " address to read set\n",
                  pkt->getAddr(),
                  makeLineAddress(pkt->getAddr()));
      }
      if (!m_xact_mgr->inRetiredReadSet(thread, addr)) {
          // Regardless of whether loads are isolated on issue or
          // retirement, always keep track of blocks referenced by
          // retired loads ("retired read set")
          m_xact_mgr->addToRetiredReadSet(thread, addr);
          DPRINTF(RubyHTM,
                  "Committed load to %#x (%#x) adding block"
                  " address to retired read set\n",
                  pkt->getAddr(),
                  makeLineAddress(pkt->getAddr()));
      }
  } else {
    panic("Unsupported transactional MemCmd\n");
  }

  return true;
}

void
TransactionalSequencer::failedCallback(Addr address,
                                       DataBlock& data,
                                       Cycles remote_timestamp,
                                       MachineID remote_nacker,
                                       bool write)
{
    m_failedCallback = true;

    m_xact_mgr->notifyReceiveNack(address,
                                  remote_timestamp,
                                  remote_nacker);
    if (write) {
        Sequencer::writeCallback(address, data);
    } else {
        Sequencer::readCallback(address, data);
    }
    m_failedCallback = false;
}

void
TransactionalSequencer::rubyHtmCallback(PacketPtr pkt)
{
    int thread = 0;
    assert(pkt->isRequest());

    // First retrieve the request port from the sender State
    RubyPort::SenderState *senderState =
        safe_cast<RubyPort::SenderState *>(pkt->popSenderState());

    MemResponsePort *port = safe_cast<MemResponsePort*>(senderState->port);
    assert(port != nullptr);
    delete senderState;

    // rubyHtmCallback called by:
    //  a) HTM commands after notifyXactionEvent
    //  b) mem accesses that find abort flag set
    // Note: Only loads may signal abort back to CPU
    // Cache access for stores & ifetches simply suppressed
    if (pkt->req->isHTMCmd()) {
        DPRINTF(RubyHTM, "rubyHtmcallback: start=%d, commit=%d, "
                "cancel=%d isolate=%d\n",
                pkt->req->isHTMStart(), pkt->req->isHTMCommit(),
                pkt->req->isHTMCancel(), pkt->req->isHTMIsolate());
    }
    else {
        assert(pkt->isHtmTransactional()); // Check: may fail...
        assert(m_xact_mgr->isAborting(thread));
        if (pkt->isRead() && !pkt->req->isInstFetch()) {
            DPRINTF(RubyHTM, "rubyHtmcallback: load finds abort flag set\n");
        }
        DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s %#x %s %s\n",
                 curTick(), m_version, "Seq", "Begin", "", "",
                 printAddress(pkt->req->getPaddr()),
                 "FAIL", " (abort flag set)");


    }

    // turn packet around to go back to requestor if response expected
    if (pkt->needsResponse()) {
        // ArmISAInst::Tstart64::completeAcc expects that response
        // packets have data (payload is HtmFailedInCacheReason)
        uint8_t* dataptr = pkt->getPtr<uint8_t>();
        memset(dataptr, 0, pkt->getSize());

        // Turn around packet into response: if HtmCacheFailure
        // anything but NO_FAIL, will set flag FAILS_TRANSACTION in
        // packet to notify CPU that transaction has failed in cache.
        HtmCacheFailure response_code = HtmCacheFailure::NO_FAIL;
        if (pkt->isRead() && !pkt->req->isInstFetch()
            && !pkt->req->isHTMStart()) { // HTM begin cannot fault
            if (m_commitPending) {
                assert(pkt->req->isHTMCommit());
                response_code = HtmCacheFailure::NO_FAIL_RETRY;
            } else {
                response_code =
                    m_xact_mgr->getHtmTransactionalReqResponseCode(thread);
            }
        }
        *dataptr = (uint8_t) response_code;

        pkt->makeHtmTransactionalReqResponse(response_code);
        port->schedTimingResp(pkt, curTick());
    } else {
        delete pkt;
    }

    trySendRetries();
}
// Insert the request in the request table. Return
// RequestStatus_Aliased if the entry was already present.
RequestStatus
TransactionalSequencer::insertRequest(PacketPtr pkt,
                                      RubyRequestType primary_type,
                                      RubyRequestType secondary_type)
{
    RequestStatus status = Sequencer::insertRequest(pkt,
                                                    primary_type,
                                                    secondary_type);
    if (!m_htm->params().precise_read_set_tracking) {
        /* If no precise read-set tracking, cache blocks are added to
         * read set as soon as load access begins. This forces the
         * abort of transactions that see conflicting snoops (Invs)
         * for loads that are not yet retired from the processor.
         * Note that with an O3 CPU model, it may lead to SR bits
         * being set despite not running a transaction.
         */
        if (pkt->isHtmTransactional() && pkt->isRead()) {
            // Trans loads added to read set as soon as issued.
            assert(!pkt->isWrite());
            assert(isDataReadRequest(secondary_type) ||
                   (primary_type == RubyRequestType_RMW_Read));

            // It is OK to receive RequestStatus_Aliased, it can be
            // considered Issued
            if ((status == RequestStatus_Ready) ||
                (status == RequestStatus_Aliased)) {

                int thread = 0;
                Addr addr = makeLineAddress(pkt->getAddr());
                if (m_xact_mgr->inTransaction(thread)) {
                    if (m_xact_mgr->checkReadSignature(addr)) {
                        DPRINTF(RubyHTMverbose,
                                "Load to %#x (%#x) already in read set\n",
                                pkt->getAddr(), addr);
                    } else {
                        DPRINTF(RubyHTM,
                                "Issued load to %#x (%#x), adding block"
                                " address to read set\n",
                                pkt->getAddr(), addr);
                        m_xact_mgr->isolateTransactionLoad(thread, addr);
                    }
                } else {
                    DPRINTF(RubyHTM,
                            "Transactional load to %#x (%#x)"
                            " outside transaction boundaries\n",
                            pkt->getAddr(),
                            makeLineAddress(pkt->getAddr()));
                    m_xact_mgr->isolateTransactionLoad(thread, addr);
                }
            }
        }
    }
    return status;
}

RequestStatus
TransactionalSequencer::makeRequest(PacketPtr pkt)
{
    int thread = 0;

    if (pkt->req->isHTMCmd()) {
        // HTM command: Intercept and notify transaction manager
        notifyXactionEvent(pkt);

        // All HTM commands need to callback CPU immediately
        rubyHtmCallback(pkt);
        
        // Pretend this request issued so that RubyPort does not try
        // to send it again later
        return RequestStatus_Issued;
    }
    else if (m_xact_mgr->isAborting(thread) &&
             pkt->isHtmTransactional()) {
        // Transactional access finds abort flag set: Callback CPU
        // immediately. If access is load, abort signal sent back to
        // CPU by setting the htmReturnReason in the response packet
        // (ifetch and stores always return HtmCacheFailure::NO_FAIL)
        rubyHtmCallback(pkt);
        return RequestStatus_Issued;
    } else {
        if (pkt->req->hasVaddr() &&
            pkt->req->getVaddr() == m_htm->getFallbackLockVAddr()) {
            // Intercept access to fallback lock and obtain physical addr
            m_htm->setFallbackLockPAddr(pkt->req->getPaddr());
        }
        uint32_t flags = pkt->req->getFlags();
        bool is_trans_rmw_read = false;
        if (pkt->isHtmTransactional() &&
            system->getArch() == Arch::X86ISA &&
            pkt->isRead() &&
            (flags & (X86ISA::StoreCheck << X86ISA::FlagShift))) {
            // Careful with rmw macroops in x86: ld microop is sent to
            // protocol as ST, but otherwise handle it as a trans load
            is_trans_rmw_read = true;

            DPRINTF(RubyHTM, "Transactional load is RMW_Read, "
                    "vaddr %#x paddr %#x\n",
                    pkt->req->getVaddr(),
                    pkt->req->getPaddr());
        }
        if (m_htm->params().lazy_vm &&
            !m_htm->params().eager_cd && // LL system
            pkt->isHtmTransactional()) {
            if (pkt->isWrite()) {
                assert(m_xact_mgr);
                if (m_xact_mgr->atCommit(thread)) {
                    // LL: write buffer contents being written back to cache,
                    // issue write request
                    DPRINTF(RubyHTM,
                            "Store to %#x while flushing write buffer\n",
                            pkt->getAddr());
                }
                else {
                    // LL: Redirect stores to write buffer
                    // Schedule hit callback for the next cycle
                    if (isWriteBufferHitEventScheduled()) {
                        return RequestStatus_BufferFull;
                    }
                    m_xact_mgr->redirectStoreToWriteBuffer(pkt);

                    writeBufferHitEvent.setPacket(pkt);

                    // For now assume that write buffer has same latency
                    // of Dcache
                    Cycles wb_latency = m_controller->
                        mandatoryQueueLatency(RubyRequestType_ST);
                    schedule(writeBufferHitEvent,
                             clockEdge(wb_latency));
                    return RequestStatus_Issued;
                }
            } else if (pkt->req->isLockedRMW()) {
                panic("Transactional Locked RMW not tested!\n");
            } else if (is_trans_rmw_read) {
                pkt->req->clearFlags(X86ISA::StoreCheck << X86ISA::FlagShift);
                DPRINTF(RubyHTM, "Transactional load is RMW_Read, "
                        "StoreCheck flag was cleared from req "
                        "vaddr %# paddr %#x\n",
                        pkt->req->getVaddr(),
                        pkt->req->getPaddr());
            }
        }
        return Sequencer::makeRequest(pkt);
    }
}

void
TransactionalSequencer::hitCallback(SequencerRequest* srequest, DataBlock& data,
                                    bool llscSuccess,
                                    const MachineType mach, const bool externalHit,
                                    const Cycles initialRequestTime,
                                    const Cycles forwardRequestTime,
                                    const Cycles firstResponseTime,
                                    const bool was_coalesced)
{
    PacketPtr pkt = srequest->pkt;
    int thread = 0;
    if (m_failedCallback) {
        // Handle nacking of Locked_RMW accesses.
        // address variable here is assumed to be a line address, so when
        // blocking buffers, must check line addresses.
        Addr address = makeLineAddress(srequest->pkt->getAddr());
        if (srequest->m_type == RubyRequestType_Locked_RMW_Read) {
            assert(m_controller->isBlocked(address));
            m_controller->unblock(address);
            DPRINTF(RubyHTM,
                    "Failed callback for Locked_RMW_Read to addr %#x"
                    " - unblocking queue\n", address);
        } else {
            assert(!m_controller->isBlocked(address));
        }
        // Skip all the following actions and do not call
        // Sequencer::hitCallback
        pkt->setHtmAccessFailedInCache(true);
        if (pkt->isAtLSQHead() &&
            !m_xact_mgr->isAborting(thread) &&
            (!pkt->isHtmTransactional() ||
             m_lastAbortHtmUid != pkt->getHtmTransactionUid())) {
            m_stalled = true;
            // If htm load at LQ head, should be past htm_start
            assert(m_xact_mgr->inTransaction(thread) ==
                   pkt->isHtmTransactional());
            Addr address = makeLineAddress(pkt->getAddr());
            DPRINTF(RubyHTM,
                    "Stalled (nacked) thread after failing to perform"
                    " access to block addr %#x\n", address);
            m_ruby_system->getProfiler()->
                getXactProfiler()->moveTo(m_version,
                                          pkt->isHtmTransactional() ?
                                          AnnotatedRegion_STALLED :
                                          AnnotatedRegion_STALLED_NONTRANS);
        }
        ruby_hit_callback(pkt);
        testDrainComplete();
        return;
    }
    if (m_stalled && pkt->isAtLSQHead() &&
        !m_xact_mgr->isAborting(thread) &&
        (!pkt->isHtmTransactional() ||
         m_lastAbortHtmUid != pkt->getHtmTransactionUid())) {
        m_stalled = false;
        if (pkt->isHtmTransactional()) {
            assert(m_xact_mgr->inTransaction(thread));
            m_ruby_system->getProfiler()->
                getXactProfiler()->moveTo(m_version,
                                          AnnotatedRegion_TRANSACTIONAL);
        } else {
            m_ruby_system->getProfiler()->
                getXactProfiler()->moveTo(m_version,
                                          AnnotatedRegion_DEFAULT);
        }
        Addr address = makeLineAddress(pkt->getAddr());
        DPRINTF(RubyHTM,
                "Stalled (nacked) thread successfully completed"
                " access to block addr %#x\n", address);
    }

    bool bypassTransLoad = false;
    if (pkt->isHtmTransactional()) {
        bool read = ((srequest->m_type == RubyRequestType_LD) ||
                     (srequest->m_type == RubyRequestType_Load_Linked) ||
                     (srequest->m_type == RubyRequestType_Locked_RMW_Read) ||
                     (srequest->m_type == RubyRequestType_RMW_Read) ||
                     (srequest->m_type == RubyRequestType_IFETCH));
        
        if (read) {
            handleTransactionalRead(srequest,
                                    data,
                                    externalHit,
                                    mach);
        }
        else if (srequest->m_type == RubyRequestType_RMW_Read ||
                 srequest->m_type == RubyRequestType_Locked_RMW_Read) {
            // Handle RMW_Read's with care due to writeback of dirty data
            // before it gets speculatively modified
            panic("Not tested!\n");        
            handleTransactionalRead(srequest,
                                    data,
                                    externalHit,
                                    mach);
        }
        else {
            handleTransactionalWrite(srequest,
                                     data,
                                     externalHit,
                                     mach);
        }
        if (m_htm->params().lazy_vm &&
            !m_htm->params().eager_cd) {
            // LL system
            if (m_xact_mgr->atCommit(thread)) {
                if (read) {
                    // Corner case: in O3CPU, transactional loads from
                    // mispredicted paths can cause misses that
                    // complete in cache after the instruction was
                    // squashed while at commit
                    DPRINTF(RubyHTM,
                            "Unexpected load hit to %#x during"
                            " lazy commit (squashed load?)\n",
                            pkt->getAddr());
                } else {
                    // Write performed in cache during lazy commit:
                    // now copy data from write buffer to datablock
                    m_xact_mgr->mergeDataFromWriteBuffer(pkt, data);
                }
                // No hitCallback needed, but handle retries in cache
                // IFETCH on xend could not be issued
                trySendRetries();
                return;
            } else if (read) {
                // Bypass from write buffer: copy data into packet
                // after hitCallback (overwrite old data from cache)
                bypassTransLoad = true;
            } else { // No callback expected from writes before commit
                assert(false);
            }
        }

    }
    if (pkt->isWrite()) {
        if (m_xact_mgr->config_enableValueChecker()) {
            uint8_t *data_ptr=(pkt->getPtr<uint8_t>());
            m_ruby_system->getXactValueChecker()->
                notifyWrite(m_version, pkt->isHtmTransactional(),
                            pkt->getAddr(), pkt->getSize(), data_ptr);
        }
    }

    Sequencer::hitCallback(srequest, data,
                           llscSuccess,
                           mach, externalHit,
                           initialRequestTime,
                           forwardRequestTime,
                           firstResponseTime,
                           was_coalesced);
    if (bypassTransLoad) {
        m_xact_mgr->bypassLoadFromWriteBuffer(pkt, data);
    }

}

void
TransactionalSequencer::handleTransactionalWrite(SequencerRequest *request,
                                          DataBlock& data, bool externalHit,
                                          const MachineType respondingMach)
{
    int thread=0; // The thread id within this CPU (SMT compliance)
    PacketPtr pkt = request->pkt;
    Addr request_address(pkt->getAddr()); // Word address
    assert(pkt->isWrite());
    assert(pkt->isHtmTransactional());
    assert(m_xact_mgr->inTransaction(thread));

    if (m_xact_mgr->checkWriteSignature(pkt->getAddr())) {
        DPRINTF(RubyHTMverbose,
                "Store to %#x (%#x) already in write set\n",
                pkt->getAddr(),
                makeLineAddress(pkt->getAddr()));
    } else {
        DPRINTF(RubyHTM,
                "Store to %#x (%#x) adds block"
                " address to write set\n",
                pkt->getAddr(),
                makeLineAddress(pkt->getAddr()));
        m_xact_mgr->isolateTransactionStore(thread, request_address);
    }
    Addr pc = Addr(0);
    if (pkt->req->hasPC()) {
        pc = pkt->req->getPC();
        assert(pkt->req->hasVaddr());
    } else { // LL flushing write buffer
        assert(m_htm->params().lazy_vm &&
               !m_htm->params().eager_cd);
        assert(m_xact_mgr->atCommit(thread));
        // Whole block
        assert(pkt->getSize() == RubySystem::getBlockSizeBytes());
    }
    m_xact_mgr->profileTransactionAccess(externalHit, true,
                                         respondingMach,
                                         pkt->getAddr(), pc,
                                         pkt->getSize());
    
}

void
TransactionalSequencer::handleTransactionalRead(SequencerRequest *srequest,
                                         DataBlock& data, bool externalHit,
                                         const MachineType respondingMach)
{
    assert ((srequest->m_type == RubyRequestType_LD) ||
            (srequest->m_type == RubyRequestType_Load_Linked) ||
            (srequest->m_type == RubyRequestType_Locked_RMW_Read) ||
            (srequest->m_type == RubyRequestType_RMW_Read) ||
            (srequest->m_type == RubyRequestType_IFETCH));
    PacketPtr pkt = srequest->pkt;
    int thread=0; // The thread id within this CPU (SMT compliance)
    _unused(thread);
    assert(pkt->isHtmTransactional());
    assert(m_xact_mgr);
    assert(pkt->req->hasPC());
    Addr pc = pkt->req->getPC();
    m_xact_mgr->profileTransactionAccess(externalHit, false, 
                                         respondingMach, 
                                         pkt->getAddr(),
                                         pc,
                                         pkt->getSize());
    // Trans loads isolated (i.e. SR bit set) when when cache access
    // begins or when retired from ROB
    if (m_xact_mgr->isAborting(thread)) {
        HtmCacheFailure reason =
            m_xact_mgr->getHtmTransactionalReqResponseCode(thread);
        assert(reason != HtmCacheFailure::NO_FAIL ||
               m_xact_mgr->isCancelledTransaction(thread));
        pkt->setHtmTransactionFailedInCache(reason);
        DPRINTF(RubyHTM, "Transactional read callback finds abort flag set\n");
    } else {
        if (m_xact_mgr->config_enableValueChecker()) {
            assert(m_xact_mgr);
            Addr request_address(pkt->getAddr()); // Word address
            _unused(request_address);
            bool checkPassed =
                m_ruby_system->getXactValueChecker()->
                xactValueCheck(m_version, request_address, pkt->getSize(),
                               data.getData(getOffset(request_address),
                                            pkt->getSize()));
            if (!checkPassed) {
                DPRINTF(RubyHTM,
                        "Transactional load to %#x (%#x)"
                        " has failed value check!\n",
                        pkt->getAddr(),
                        makeLineAddress(pkt->getAddr()));
                panic("Value check failed!\n");
            }
        }
    }
}

void
TransactionalSequencer::writeBufferEvent(PacketPtr pkt)
{
    writeBufferHitEvent.clearPacket();
    ruby_hit_callback(pkt);
    testDrainComplete();
}

} // namespace ruby
} // namespace gem5
