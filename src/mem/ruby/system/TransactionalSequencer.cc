#include "mem/ruby/system/TransactionalSequencer.hh"

#include "arch/x86/ldstflags.hh"
#include "debug/ProtocolTrace.hh"
#include "debug/RubyHTM.hh"
#include "debug/RubyHTMlog.hh"
#include "debug/RubyHTMverbose.hh"
#include "debug/RubyPort.hh"
#include "mem/ruby/htm/EagerTransactionVersionManager.hh"
#include "mem/ruby/htm/TransactionInterfaceManager.hh"
#include "mem/ruby/htm/XactIsolationChecker.hh"
#include "mem/ruby/htm/XactValueChecker.hh"
#include "mem/ruby/htm/logtm.h"
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
      m_lastStateBeforeStall(AnnotatedRegion_INVALID),
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
TransactionalSequencer::print(std::ostream& out) const
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
    assert(m_xact_mgr->isAborting(thread) ||
           pkt->req->isHTMAbort());
    if (!m_htm->params().lazy_vm) {
        cancelLogRequests();
    }
    m_stalled = false;
    m_lastStateBeforeStall = AnnotatedRegion_INVALID;
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
      if (m_xact_mgr->config_enableIsolationChecker()) {
          m_ruby_system->getXactIsolationChecker()->
              addToReadSet(m_version,
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
        if (pkt->req->hasVaddr()){
            if (pkt->req->getVaddr() == m_htm->getFallbackLockVAddr()) {
                // Intercept access to fallback lock and obtain physical addr
                m_htm->setFallbackLockPAddr(pkt->req->getPaddr());
            } else if (!m_htm->params().lazy_vm) {
                assert(m_xact_mgr);
                // LogTM: Intercept access to undo log and setup
                // log TLB translations
                if (m_xact_mgr->isAccessToLog(pkt->req->getVaddr())) {
                    if (!m_xact_mgr->isLogReady()) {
                        m_xact_mgr->setupLogTranslation(pkt->req->getVaddr(),
                                                        pkt->req->getPaddr());
                    } else if (m_xact_mgr->isUnrollingLog(thread)) {
                        // We can have lingering transactional
                        // loads immediately abort signal from CPU
                        DPRINTF(RubyHTMlog, "Log access during unroll "
                                "vaddr %#x paddr %#x\n",
                                pkt->req->getVaddr(),
                                pkt->req->getPaddr());
                        if (m_xact_mgr->isEndLogUnrollSignal(pkt)) {
                            assert(pkt->isWrite());
                           // "Magic value" written to logbase to
                           // signal log unroll completed without
                           // writing to the stack (call m5xxx)
                            m_xact_mgr->endLogUnroll(thread);
                            DPRINTF(RubyHTMlog, "Log unroll completed\n");
                            // No need to perform memory access in cache
                            ruby_hit_callback(pkt);
                            testDrainComplete();
                            return RequestStatus_Issued;
                        } else {
                            assert(!pkt->isWrite());
                        }
                    } else {
                        if (pkt->isWrite()) {
                            panic("Unexpected write to undo log!\n");
                        } else {
                            // Speculative from mispredicted paths may
                            // read from log locations immediately
                            // after unroll has completed
                            warn("Unexpected load to undo log!"
                                 " - PC %#x vaddr %#x\n",
                                 pkt->req->getPC(),
                                 pkt->req->getVaddr());
                        }
                    }
                }
            }
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
                        "vaddr %#x paddr %#x\n",
                        pkt->req->getVaddr(),
                        pkt->req->getPaddr());
            }
        } else if (!m_htm->params().lazy_vm) { // LogTM
            Addr line_addr = makeLineAddress(pkt->getAddr());
            if (pkt->isHtmTransactional() &&
                pkt->isWrite()) {
                assert(m_htm->params().eager_cd);
                if (m_xact_mgr->checkWriteSignature(pkt->getAddr())) {
                    // Store to block already in wset
                    DPRINTF(RubyHTMlog, "Store to vaddr %#x"
                            " block paddr %#x already logged\n",
                            pkt->req->getVaddr(),
                            makeLineAddress(pkt->req->getPaddr()));
                    assert(m_logRequestTable.find(pkt->getAddr()) ==
                           m_logRequestTable.end());
                } else {
                    /* Generate log requests and use
                       Sequencer::makeRequest(pkt) to handle them. Once both
                       log requests issued, then let it thru to make request
                       for program store. Need to keep track of each
                       "oustanding" store and its associated log requests.
                    */
                    // Check if there is any outstanding log request for the
                    // cache line targeted by this write.
                    if (m_logRequestTable.find(line_addr) !=
                        m_logRequestTable.end()) {
                        auto &log_req_list = m_logRequestTable[line_addr];
                        assert(!log_req_list.empty());
                        // Outstanding log request
                        LogRequestInfo &log_req = log_req_list.back();
                        if (log_req.htmTransactionUid !=
                            pkt->getHtmTransactionUid()) {
                            // Wait for lingering log store from
                            // already aborted transaction
                            assert(log_req.suppressed);
                            DPRINTF(RubyHTMlog, "Cannot make request for"
                                    " store to vaddr - Outstanding"
                                    " lingering stores from htmUid %ld\n",
                                    log_req.htmTransactionUid);
                            return RequestStatus_WaitUntilLogged;
                        }
                        if (log_req.completed + log_req.outstanding < 2) {
                            makeLogRequests(log_req);
                        }
                        if (log_req.completed < 2) {
                            DPRINTF(RubyHTMlog, "Cannot make request for"
                                    " store to vaddr %#x paddr %#x (%#x)"
                                    " - %d/%d outstanding/completed\n",
                                    pkt->req->getVaddr(),
                                    pkt->req->getPaddr(),
                                    line_addr,
                                    log_req.outstanding,
                                    log_req.completed);
                            return RequestStatus_WaitUntilLogged;
                        }
                        // Complete
                        DPRINTF(RubyHTMlog, "Done adding log entry for"
                                " store to vaddr %#x paddr %#x\n",
                                pkt->req->getVaddr(),
                                pkt->req->getPaddr());
                        // Will call makeRequest for program store next
                    } else {
                        // Create log requests/packets
                        LogRequestInfo log_req = buildLogPackets(pkt);
                        // Try to issue them to cache
                        makeLogRequests(log_req);
                        // Record their status via logReqTable
                        auto &log_req_list = m_logRequestTable[line_addr];
                        assert(log_req_list.empty());
                        log_req_list.emplace_back(log_req);
                        DPRINTF(RubyHTMlog, "Store to vaddr %#x paddr %#x"
                                "(%#x) must wait until log requests done\n",
                                pkt->req->getVaddr(),
                                pkt->req->getPaddr(),
                                line_addr);


                        return RequestStatus_WaitUntilLogged;
                    }
                }
            } else { // Not a transactional write
                if (m_logRequestAddr.find(line_addr) !=
                    m_logRequestAddr.end()) {
                    // Prevent accesses the log while logging is ongoing

                    DPRINTF(RubyHTMlog, "%strans %s to paddr %#x"
                            "(%#x) must wait until log requests done\n",
                            pkt->isHtmTransactional() ? "" : "non-",
                            pkt->isWrite() ? "write" : "read",
                            pkt->getAddr(),
                            line_addr);
                    // Prevent processor from accessing the log while logging
                    return RequestStatus_WaitUntilLogged;
                }
            }
        }// Logtm
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
        if (pkt->req->hasVaddr() &&
            pkt->req->getVaddr() == m_htm->getFallbackLockVAddr()) {
            DPRINTF(RubyHTM,
                    "Failed access to fallback lock!"
                    " - PC %#x vaddr %#x\n",
                    pkt->req->getPC(),
                    pkt->req->getVaddr());
            // Requester-stalls policies that prevent the lock from
            // being acquired/released are subject to deadlocks
            // without adequate management of conflicts with a
            // non-transactional requester
            warn("Failed access to fallback lock!"
                  " - PC %#x vaddr %#x\n",
                  pkt->req->getPC(),
                  pkt->req->getVaddr());
        }
        // Skip all the following actions and do not call
        // Sequencer::hitCallback
        pkt->setHtmAccessFailedInCache(true);
        if (pkt->isAtLSQHead() &&
            !m_xact_mgr->isAborting(thread) &&
            !m_stalled &&
            (!pkt->isHtmTransactional() ||
             m_lastAbortHtmUid != pkt->getHtmTransactionUid())) {
            m_stalled = true;
            assert(m_lastStateBeforeStall == AnnotatedRegion_INVALID);
            m_lastStateBeforeStall = m_ruby_system->getProfiler()->
                getXactProfiler()->getCurrentRegion(m_version);
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

        if (pkt->isHtmTransactional()) {
            assert(m_xact_mgr->inTransaction(thread));
            assert(m_lastStateBeforeStall == AnnotatedRegion_TRANSACTIONAL);
        }
        m_ruby_system->getProfiler()->
            getXactProfiler()->moveTo(m_version,
                                      m_lastStateBeforeStall);
        // Reset
        m_lastStateBeforeStall = AnnotatedRegion_INVALID;
        m_stalled = false;
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
                        "Load to %#x (%#x)"
                        " has failed value check!\n",
                        pkt->getAddr(),
                        makeLineAddress(pkt->getAddr()));
                panic("Value check failed!\n");
            }
        }
    }
    if (m_xact_mgr->config_enableIsolationChecker() &&
        !m_xact_mgr->isAborting(thread)) {
        bool passed = m_ruby_system->getXactIsolationChecker()->
            checkXACTIsolation(m_version, pkt->getAddr(),
                               pkt->isHtmTransactional(),
                               srequest->m_type);
        if (!passed) {
            panic("Transaction isolation check failed!\n");
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
    }
}

void
TransactionalSequencer::writeBufferEvent(PacketPtr pkt)
{
    writeBufferHitEvent.clearPacket();
    ruby_hit_callback(pkt);
    testDrainComplete();
}

LogRequestInfo
TransactionalSequencer::buildLogPackets(PacketPtr mainPkt) {
  assert(mainPkt->isHtmTransactional());
  assert(mainPkt->req->hasVaddr());

#if 0
  /* TODO: Let CPU notify Ruby about which stores need to be logged,
     and provide v2p translations for required log accesses.
   */
  Addr logDataPtr =  mainPkt->getLogAddr();
#else
  int numEntry = m_xact_mgr->addLogEntry(mainPkt->getAddr());
  Addr logDataVPtr = m_xact_mgr->getXactEagerVersionManager()->
      computeLogDataPointer(numEntry);
  Addr logDataPtr = m_xact_mgr->getXactEagerVersionManager()->
      translateLogAddress(logDataVPtr);
  DPRINTF(RubyHTMlog,
          "Logging store to paddr %#x - log %d vaddr %#x log paddr %#x \n",
          mainPkt->getAddr(), numEntry, logDataVPtr, logDataPtr);
#endif
  assert(logDataPtr == makeLineAddress(logDataPtr));
  Addr logAddressPtr = (Addr)logtm_compute_addr_ptr_from_data_ptr(logDataPtr);

  if (makeLineAddress(logDataPtr) == makeLineAddress(logAddressPtr)) {
      panic("Undo log has overflowed,"
            " address log should never overlap with data log\n");
  }

  // Log data pointer must be always aligned to cache line size
  assert(makeLineAddress(logDataPtr) == logDataPtr);

  RequestPtr logDataReq =
      std::make_shared<Request>(logDataPtr,
                                RubySystem::getBlockSizeBytes(),
                                Request::PHYSICAL,
                                mainPkt->req->requestorId());

  RequestPtr logAddrReq =
      std::make_shared<Request>(logAddressPtr,
                                sizeof(Addr),
                                Request::PHYSICAL,
                                mainPkt->req->requestorId());

  // Create request and packets
  PacketPtr logDataPkt = Packet::createWrite(logDataReq);
  PacketPtr logAddrPkt = Packet::createWrite(logAddrReq);
  // Mark these packets as stores to the log
  // Keep pointer to original packet, required to locate
  // LogRequestInfo upon callback
  logAddrPkt->setHtmStoreToLog(true, mainPkt);
  logDataPkt->setHtmStoreToLog(true, mainPkt);

  // Allocate packet data, will copy values to be logged upon program
  // store writeCallback
  logDataPkt->allocate();
  // Allocate packet data and copy program store's target virtual line address
  logAddrPkt->allocate();
  Addr vaddr = makeLineAddress(mainPkt->req->getVaddr());
  uint8_t *p = (uint8_t *)&vaddr;
  logAddrPkt->setData(p);
  DPRINTF(RubyHTMlog, "Generating accesses to log:"
          " - paddr %#x (addr) %#x (data)\n",
          logAddrPkt->getAddr(),
          logDataPkt->getAddr());

  return LogRequestInfo(logAddrPkt, logDataPkt,
                        mainPkt->getHtmTransactionUid(),
                        mainPkt->req->getVaddr(),
                        makeLineAddress(mainPkt->getAddr()));
}

void
TransactionalSequencer::makeLogRequests(LogRequestInfo &logreqinfo)
{
    if (m_logRequestTable.size() > m_dataCache_ptr->getCacheAssoc()/2) {
        /* Since logging requires locking the destination (log data
           block) until the store is ready to perform in cache, we
           must always leave room in the set for other cache lines
           apart from the locked log lines.
         */
        DPRINTF(RubyHTMlog, "Too many outstanding log requests (%d)"
                " - cannot issue log requests for store paddr %#x\n",
                m_logRequestTable.size(),
                logreqinfo.paddr);
        return;
    }

    assert(!logreqinfo.suppressed);

    if (logreqinfo.logAddrPktStatus != RequestStatus_Issued) {
        logreqinfo.logAddrPktStatus =
            Sequencer::makeRequest(logreqinfo.logAddrPkt);
        if (logreqinfo.logAddrPktStatus == RequestStatus_Issued) {
            DPRINTF(RubyHTMlog, "Issued log address request:"
                    " - paddr %#x (addr)\n",
                    logreqinfo.logAddrPkt->getAddr());
            ++logreqinfo.outstanding;
            Addr line_address =
                makeLineAddress(logreqinfo.logAddrPkt->getAddr());
            m_logRequestAddr[line_address]='y';
        }
    }
    if (logreqinfo.logDataPktStatus != RequestStatus_Issued) {
        logreqinfo.logDataPktStatus =
            Sequencer::makeRequest(logreqinfo.logDataPkt);
        if (logreqinfo.logDataPktStatus == RequestStatus_Issued) {
            DPRINTF(RubyHTMlog, "Issued log data request:"
                    " - paddr %#x (addr)\n",
                    logreqinfo.logDataPkt->getAddr());
            ++logreqinfo.outstanding;
            Addr line_address =
                makeLineAddress(logreqinfo.logDataPkt->getAddr());
            m_logRequestAddr[line_address]='y';
        }
    }

}

void
TransactionalSequencer::handleStoresToLog(Addr address,
                                       PacketPtr pkt,
                                       DataBlock& data)
{
    assert(pkt->isWrite());
    Addr store_addr = makeLineAddress(pkt->getHtmLoggedStoreAddr());
    assert(m_logRequestTable.find(store_addr) !=
           m_logRequestTable.end());
    auto &log_req_list = m_logRequestTable[store_addr];
    assert(log_req_list.size() == 1);
    LogRequestInfo &log = log_req_list.back();
    assert(pkt->isHtmStoreToLog());

    --log.outstanding;
    ++log.completed;
    bool found = m_logRequestAddr.erase(address);
    assert(found);

    if (pkt == log.logAddrPkt) {
        // Copy virtual address to address log
        const uint64_t *vaddrPtr = pkt->getConstPtr<uint64_t>();
        data.setData(pkt->getConstPtr<uint8_t>(),
                     getOffset(pkt->getAddr()), pkt->getSize());
        DPRINTF(RubyHTMlog, "Log address block at paddr %#x"
                " written with vaddr %#x\n",
                *vaddrPtr, pkt->getAddr());
    }
    else {
        // We cannot copy to the data log, we may not yet have the
        // data block from the program store
        assert(pkt == log.logDataPkt);
        // However, we need to set pending log store bit to
        // prevent replacements on this line: need both program
        // (src) and data log (dest) blocks cached
        m_dataCache_ptr->setHtmLogPending(address, true);
        DPRINTF(RubyHTMlog, "Log data block paddr %#x pinned in cache"
                " until data from vaddr %#x (paddr %#x) copied\n",
                address, log.vaddr, log.paddr);
    }
    if (log.suppressed) {
        // Transaction aborted while outstanding log requests: ignore
        DPRINTF(RubyHTMlog, "Ignored log requests for "
                " cancelled store to paddr %#x due to abort "
                " - %d/%d outstanding/completed\n", store_addr,
                log.outstanding, log.completed);
        // Erase entry if no more outstanding stores to log
        if (log.outstanding == 0) {
            // Release lock on log data block
            m_dataCache_ptr->
                setHtmLogPending(log.logDataPkt->getAddr(), false);
            // Erase entry from log request table
            bool found = m_logRequestTable.erase(store_addr);
            assert(found);
            // Delete packets
#if 0
            delete log.logAddrPkt;
            delete log.logDataPkt;
#endif
        }
    }
}

void
TransactionalSequencer::handleLoggedStore(Addr address,
                                          PacketPtr pkt,
                                          DataBlock& data)
{
    assert(pkt->isHtmTransactional());
    assert(pkt->isWrite());
    auto &log_req_list = m_logRequestTable[address];
    assert(log_req_list.size() == 1);
    LogRequestInfo &log = log_req_list.back();
    assert(log.outstanding == 0 && log.completed == 2);
    assert(!log.suppressed);
    PacketPtr logPkt = log.logDataPkt;
    assert(logPkt->getSize() == RubySystem::getBlockSizeBytes());
    // get a pointer to the data log cache block, then write values
    // tryCacheAccess must find a write hit, and return pointer
    if (!m_dataCache_ptr->isHtmLogPending(logPkt->getAddr())) {
        // TODO: Prevent replacements of entries with htmLogPending
        // flag set
      panic("Undo log data entry has been evicted "
            "Transactional store cannot be logged!\n");
    }
    // Unblock the data log cache block now..
    m_dataCache_ptr->setHtmLogPending(logPkt->getAddr(), false);
    DataBlock* logDatablockPtr;
    bool hit = m_dataCache_ptr->
        tryCacheAccess(makeLineAddress(logPkt->getAddr()),
                       RubyRequestType_ST,
                       logDatablockPtr, false);
    _unused(hit);
    assert(hit);
    // Should only log blocks that have not yet been written
    assert(!m_xact_mgr->checkWriteSignature(address));
    // Finally, write non-speculative values from current data block
    // (target of transactional store) to data log cache block
    logDatablockPtr->setData(data.getData(0,RubySystem::getBlockSizeBytes()),
                             0 /*offset*/,
                             RubySystem::getBlockSizeBytes() /*len*/);

    DPRINTF(RubyHTMlog, "Successfully logged store to vaddr"
            " %#x paddr %#x old value %s\n",
            makeLineAddress(pkt->req->getVaddr()),
            makeLineAddress(pkt->getAddr()),
            logDatablockPtr->toString());
    // Finally, commit this entry into the log.
    m_xact_mgr->commitLogEntry(log.logDataPkt->getAddr());

    if (m_xact_mgr->config_enableValueChecker()) {
        // Record old value we just logged
        m_ruby_system->getXactValueChecker()->
            notifyLoggedDataBlock(m_version,address, data);
    }

    // Delete packets
#if 0
    delete log.logAddrPkt;
    delete log.logDataPkt;
#endif
    // Erase log request
    bool found = m_logRequestTable.erase(address);
    assert(found);
}

void
TransactionalSequencer::cancelLogRequests()
{
    // Careful when erasing from a map while iterating it. Must avoid
    // using iterator after erasing
    auto it = m_logRequestTable.begin();
    for (auto next_it = it;
         it != m_logRequestTable.end(); it = next_it) {
        ++next_it;
        auto &log_req_list =(*it).second;
        assert(log_req_list.size() == 1);
        LogRequestInfo &log_req = log_req_list.back();
        DPRINTF(RubyHTMlog, "Lingering logging of store"
                " to paddr %#x\n",
                (*it).first);
        if (log_req.outstanding == 0) {
            // Release lock on log data block for transactional stores
            // that have not completed before the abort.
            PacketPtr logPkt = log_req.logDataPkt;
            m_dataCache_ptr->setHtmLogPending(logPkt->getAddr(), false);
            // Erase entry from log request table
#if 0
            delete log_req.logAddrPkt;
            delete log_req.logDataPkt;
#endif
            bool found = m_logRequestTable.erase((*it).first);
            assert(found);
        } else {
            // Signal that the log req entry is to be erased and the
            // lock on the cache released by handleStoresToLog when
            // the outstanding log store completes
            log_req.suppressed = true;
            DPRINTF(RubyHTMlog, "Lingering logging of store"
                    " to paddr %#x has outstanding request\n",
                    (*it).first);
        }
    }
}

void
TransactionalSequencer::writeCallback(Addr address, DataBlock& data,
                         const bool externalHit, const MachineType mach,
                         const Cycles initialRequestTime,
                         const Cycles forwardRequestTime,
                         const Cycles firstResponseTime,
                         const bool noCoales)
{
    if (!m_htm->params().lazy_vm) { // LogTM
        // Intercept stores to the log
        assert(address == makeLineAddress(address));
        assert(m_RequestTable.find(address) != m_RequestTable.end());
        auto &seq_req_list = m_RequestTable[address];

        assert(!seq_req_list.empty());
        SequencerRequest &seq_req = seq_req_list.front();
        if (seq_req.pkt->isHtmStoreToLog()) {
            // Logging stores (addr + data)
            handleStoresToLog(address, seq_req.pkt, data);
            // Remove this request from Sequencer structures
            assert(seq_req_list.size() == 1);
            seq_req_list.pop_front();
            m_RequestTable.erase(address);
            markRemoved();
            // Do not callback CPU
            // Wake up program store or store to log pending to be issued
            trySendRetries();
            return;
        } else if (m_logRequestTable.find(address) !=
                   m_logRequestTable.end()) {
            // Must check all outstanding requests for this address to
            // handle logged store that needs
            bool isWrite = false;
            bool isRMWRead = false;
            PacketPtr writePkt;
            for (auto it=seq_req_list.begin();
                 it != seq_req_list.end(); ++it) {
                if ((*it).pkt->isWrite()) {
                    isWrite = true;
                    writePkt = (*it).pkt;
                    break;
                } else if ((*it).m_type == RubyRequestType_RMW_Read) {
                    isRMWRead = true;
                }
            }
            // Can we have a writeCallback for an address in the
            // logRequestTable if we don't have a write?
            if (isWrite) {
                // Transactional store with outstanding logging actions
                handleLoggedStore(address, writePkt, data);
                // Fall thru to erase and callback CPU...
            } else {
                // This callback is for a prior RMW_Read to the same
                // line that has a later store pending to be logged
                assert(isRMWRead);
            }
        }
    }
    Sequencer::writeCallback(address, data, externalHit, mach,
                             initialRequestTime,
                             forwardRequestTime,
                             firstResponseTime,
                             noCoales);
    if (!m_htm->params().lazy_vm) { // LogTM
        int thread = 0;
        if (m_xact_mgr->config_enableValueChecker() &&
            m_xact_mgr->isUnrollingLog(thread)) {
            if (m_xact_mgr->checkWriteSignature(address)) {
                // Restoring old value from log into wset block: Save
                // datablock and check when unroll completes. Must do
                // it after writeCallback observe written value
                m_ruby_system->getXactValueChecker()->
                    notifyUnrolledDataBlock(m_version, address, data);
            }
        }
    }
}

} // namespace ruby
} // namespace gem5
