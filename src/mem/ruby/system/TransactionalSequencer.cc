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
      writeBufferHitEvent(this),
      lazyCommitCheckEvent(this)

{
    // TransactionalSequencer is only used by UMU protocols
    assert(m_ruby_system->getProtocol() == "MESI_Two_Level_HTM_umu" ||
           m_ruby_system->getProtocol() == "MESI_Three_Level_HTM_umu");;
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
    m_stalled = false;
    m_lastStateBeforeStall = AnnotatedRegion_INVALID;
    m_xact_mgr->abortTransaction(thread, pkt);
    m_lastAbortHtmUid = pkt->getHtmTransactionUid();
    m_failedStores.clear();
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
              } else {
                  // If pending commit actions that prevent abort,
                  // schedule event to complete the abort the such
                  // outstanding actions are done
                  if (!lazyCommitCheckEvent.scheduled()) {
                      lazyCommitCheckEvent.setPacket(pkt);
                      schedule(lazyCommitCheckEvent,
                               clockEdge(Cycles(1)));
                      DPRINTF(RubyHTM, "Scheduled lazy commit check"
                              " event (abort)\n");
                  }
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
          m_failedStores.clear();
      } else {
          m_xact_mgr->initiateCommitTransaction(thread, xid, pkt);
          m_commitPending = true;
          DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s \n",
                   curTick(), m_version, "Seq",
                   "HTM_COMMIT_PENDING" , "", "");
          // Schedule event to call makeRequest again on the next
          // cycle with this commit packet. rubyHtmCallback next will
          // observe commitPending active and thus will not delete the
          // packet nor send a response back.
          if (!lazyCommitCheckEvent.scheduled()) {
              lazyCommitCheckEvent.setPacket(pkt);
              schedule(lazyCommitCheckEvent,
                       clockEdge(Cycles(1)));
              DPRINTF(RubyHTM, "Scheduled lazy commit check event\n");
          }
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
        m_failedStores[address] = true;
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
        bool skip_response = false;
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
                skip_response = true;
            } else {
                response_code =
                    m_xact_mgr->getHtmTransactionalReqResponseCode(thread);
            }
        }
        *dataptr = (uint8_t) response_code;

        if (!skip_response) {

            // First retrieve the request port from the sender State
            RubyPort::SenderState *senderState =
                safe_cast<RubyPort::SenderState *>(pkt->popSenderState());

            MemResponsePort *port =
                safe_cast<MemResponsePort*>(senderState->port);
            assert(port != nullptr);
            delete senderState;
            pkt->makeHtmTransactionalReqResponse(response_code);
            port->schedTimingResp(pkt, curTick());
        }
    } else {
        // First retrieve the request port from the sender State
        RubyPort::SenderState *senderState =
            safe_cast<RubyPort::SenderState *>(pkt->popSenderState());
        delete senderState;
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
    Addr address = makeLineAddress(pkt->getAddr());
    if (m_failedStores.find(address) != m_failedStores.end()) {
        if (secondary_type != RubyRequestType_ST) {
            // Prevent reordering of loads w.r.t. earlier stores that
            // have been nacked and not yet retried
            DPRINTF(RubyHTM,
                    "Cannot issue load %#x while pending failed store\n",
                    address);
            panic("Cannot issue load while pending failed store\n");

        }
    }

    RequestStatus status = Sequencer::insertRequest(pkt,
                                                    primary_type,
                                                    secondary_type);
    if (!m_htm->params().lazy_vm) { // LogTM
        if (pkt->isWrite() &&
            pkt->isHtmTransactional() &&
            !m_xact_mgr->checkWriteSignature(address)) {
            // Transactional store to block not in Wset
            if (status == RequestStatus_Ready) {
                assert(m_pendingLogging.find(address) ==
                       m_pendingLogging.end());
                m_pendingLogging[address]=true;
            } else {
                assert(status == RequestStatus_Aliased);
                // No logging needed unless this is the first store to
                // the block (aliased with load miss)
                auto &seq_req_list = m_RequestTable[address];
                assert(seq_req_list.size() > 1);
                int numStoresFound = 0;
                for (auto it=seq_req_list.begin();
                     it != seq_req_list.end(); ++it) {
                    SequencerRequest seq_req = *it;
                    if ((seq_req.m_type != RubyRequestType_LD) &&
                        (seq_req.m_type != RubyRequestType_Load_Linked) &&
                        (seq_req.m_type != RubyRequestType_IFETCH)) {
                        ++numStoresFound;
                    }
                }
                // At least the current store
                assert(numStoresFound > 0);
                if (numStoresFound == 1) {
                    assert(m_pendingLogging.find(address) ==
                           m_pendingLogging.end());
                    m_pendingLogging[address]=true;
                } else { // Coalesced into a previous store that
                         // already booked the MSHRs for logging
                    assert(m_pendingLogging.find(address) !=
                           m_pendingLogging.end() ||
                           m_logRequestTable.find(address) !=
                           m_logRequestTable.end());
                }
            }
        }
    }

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
bool
TransactionalSequencer::canMakeRequest(PacketPtr pkt)
{
    // HTM abort signals must be allowed to reach the Sequencer
    // the same cycle they are issued. They cannot be retried.
    int num_reserved_mshrs = 0;
    if (!m_htm->params().lazy_vm) { // LogTM
        // Need two extra MSHRs for each store with log actions
        // pending.
        num_reserved_mshrs = 2*m_pendingLogging.size();
        if (pkt->isWrite() &&
            pkt->isHtmTransactional()) {
            Addr address = makeLineAddress(pkt->getAddr());
            if (!m_xact_mgr->checkWriteSignature(address) &&
                m_pendingLogging.find(address) == m_pendingLogging.end()) {
                // This trans store can issue if it can reserve 2 extra
                // MSHRs for its logging actions
                num_reserved_mshrs += 2;
            }
        } else if (pkt->isHtmStoreToLog()) {
            assert(!pkt->isHtmTransactional());
            Addr store_addr = makeLineAddress(pkt->getHtmLoggedStoreAddr());
            // These are the two logging requests: a pair of the
            // reserved mshrs was allocated for them, so exclude them
            assert(m_pendingLogging[store_addr]);
            num_reserved_mshrs -= 2;
        }
    }
    if ((m_outstanding_count +
         num_reserved_mshrs >= m_max_outstanding_requests) &&
        !pkt->req->isHTMAbort()) {
        return false;
    } else {
        return true;
    }
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
        if (m_commitPending) {
            panic("Unexpected abort while commit pending!\n");
        }
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
                            // Wait for lingering stores to complete
                            if (!m_logRequestTable.empty()) {
                                DPRINTF(RubyHTMlog, "Log unroll must"
                                        " wait for %d lingering log stores\n",
                                        m_logRequestTable.size());
                                return RequestStatus_BufferFull;
                            }
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
            } else { // Not a transactional write
                for (auto it = m_logRequestTable.begin();
                    it != m_logRequestTable.end(); ++it) {
                    auto &log_req_list =(*it).second;
                    assert(log_req_list.size() == 1);
                    LogRequestInfo &log_req = log_req_list.back();
                    if ((line_addr == makeLineAddress(log_req.logAddr)) ||
                        (line_addr == makeLineAddress(log_req.logData))) {
                        // Access to log while logging
                        panic("Unexpected access to the"
                              " undo log while logging!\n");
                    }
                }
            }

        }// Logtm
        return Sequencer::makeRequest(pkt);
    }
}

void
TransactionalSequencer::failedCallbackCleanup(Addr address,
                                              SequencerRequest* srequest)
{
    assert(m_failedCallback);
    if (!m_htm->params().lazy_vm) { // LogTM: free reserved MSHRs via
                                    // "pendingLogging" map.
        // Done after ruby_hit_callback in case a request is retried
        PacketPtr pkt = srequest->pkt;
        bool needsLogging =
            m_pendingLogging.find(address) != m_pendingLogging.end();
        if (needsLogging) {
            auto &seq_req_list = m_RequestTable[address];
            if (!pkt->isWrite()) { // Aliased loads on nacked store miss
                assert(seq_req_list.size() > 1);
            } else { // Nacked write
                assert(pkt->isHtmTransactional() &&
                       !m_xact_mgr->checkWriteSignature(address));
                if (seq_req_list.size() == 1) { // Only write
                    // Clear from pending to free "reserved MSHRs"
                    // see (canMakeRequest)
                    m_pendingLogging.erase(address);
                } else { // Aliased requests: if other stores
                    // coalesced into this miss, do not erase
                    int numStoresFound = 0;
                    for (auto it=seq_req_list.begin();
                         it != seq_req_list.end(); ++it) {
                        SequencerRequest seq_req = *it;
                        if ((seq_req.m_type != RubyRequestType_LD) &&
                            (seq_req.m_type != RubyRequestType_Load_Linked) &&
                            (seq_req.m_type != RubyRequestType_IFETCH)) {
                            ++numStoresFound;
                        }
                    }
                    if (numStoresFound == 1) {
                        // This is the only store, other aliased reqs are
                        // loads: erase from pending logging
                        m_pendingLogging.erase(address);
                    } else { // There are other stores apart from this
                        // one that failed: retain entry
                        assert(numStoresFound > 1);
                    }
                }
            }
        }
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
        pkt->setHtmFailedCacheAccess(true);
        if (pkt->isAtLSQHead() &&
            !m_xact_mgr->isAborting(thread) &&
            !m_stalled &&
            (!pkt->isHtmTransactional() ||
             m_lastAbortHtmUid != pkt->getHtmTransactionUid())) {
            m_stalled = true;
            assert(m_lastStateBeforeStall == AnnotatedRegion_INVALID);
            m_lastStateBeforeStall = m_ruby_system->getProfiler()->
                getXactProfiler()->getCurrentRegion(m_version);
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
        failedCallbackCleanup(address, srequest);
        testDrainComplete();
        return;
    }
    if (pkt->isHtmStoreToLog()) {
        ///////////
        // Intercept stores to the log
        assert(!m_failedCallback);
        // Logging stores (addr + data): one copies old value into
        // the log and the other the virtual address targeted by
        // the program store. When both are complete, will
        // directly callback Sequencer::writeCallback
        Addr address = makeLineAddress(pkt->getAddr());
        handleStoresToLog(address, pkt, data);
        // No callback needed
        delete pkt;
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

        Addr address = makeLineAddress(pkt->getAddr());
        if (read) {
            // No reads while failed store pending to be retried
            assert(m_failedStores.find(address) == m_failedStores.end());
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
            m_failedStores.erase(address);
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

void
TransactionalSequencer::lazyCommitEvent(PacketPtr pkt)
{
    lazyCommitCheckEvent.clearPacket();
    makeRequest(pkt);
    testDrainComplete();
}

LogRequestInfo
TransactionalSequencer::buildLogPackets(PacketPtr mainPkt,
                                        DataBlock& datablock) {
  assert(mainPkt->isHtmTransactional());
  assert(mainPkt->req->hasVaddr());

  int logIndex = m_xact_mgr->addLogEntry(makeLineAddress(mainPkt->getAddr()));
  Addr logDataVPtr = m_xact_mgr->getXactEagerVersionManager()->
      computeLogDataPointer(logIndex);
  Addr logDataPtr = m_xact_mgr->getXactEagerVersionManager()->
      translateLogAddress(logDataVPtr);
  DPRINTF(RubyHTMlog,
          "Logging store to paddr %#x -"
          " log index %d vaddr %#x log paddr %#x \n",
          mainPkt->getAddr(), logIndex, logDataVPtr, logDataPtr);

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

  // Allocate packet data and copy values to be logged
  logDataPkt->allocate();
  logDataPkt->setData(datablock.getData(0, RubySystem::getBlockSizeBytes()));

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
                        logIndex,
                        mainPkt->req->getVaddr(),
                        makeLineAddress(mainPkt->getAddr()));
}

bool
TransactionalSequencer::makeLogRequests(LogRequestInfo &logreqinfo)
{

    if (logreqinfo.logAddrPktStatus != RequestStatus_Issued) {
        logreqinfo.logAddrPktStatus =
            Sequencer::makeRequest(logreqinfo.logAddrPkt);
        if (logreqinfo.logAddrPktStatus != RequestStatus_Issued) {
            return false;
        }
        DPRINTF(RubyHTMlog, "Issued log address request:"
                " - paddr %#x (addr)\n",
                logreqinfo.logAddrPkt->getAddr());
        ++logreqinfo.outstanding;
    }
    if (logreqinfo.logDataPktStatus != RequestStatus_Issued) {
        logreqinfo.logDataPktStatus =
            Sequencer::makeRequest(logreqinfo.logDataPkt);
        if (logreqinfo.logDataPktStatus != RequestStatus_Issued) {
            return false;
        }
        DPRINTF(RubyHTMlog, "Issued log data request:"
                " - paddr %#x (addr)\n",
                logreqinfo.logDataPkt->getAddr());
        ++logreqinfo.outstanding;
    }
    return true;
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
    if (pkt == log.logAddrPkt) { // This is the callback for the log
                                 // address block
        // Will copy virtual address to log when program store
        // completes. For now, set it to zero to indicate that the
        // data log is not valid. This is essential for correctness
        // since the program store may never complete leaving "gaps",
        // and the unroll must skip restoring memory in that case
        const uint64_t *vaddrPtr = pkt->getConstPtr<uint64_t>();
        assert(*vaddrPtr == makeLineAddress(log.vaddr));
        data.setData(pkt->getConstPtr<uint8_t>(),
                     getOffset(pkt->getAddr()), pkt->getSize());
        DPRINTF(RubyHTMlog, "Log address block at paddr %#x"
                " written with vaddr %#x"
                " targeted by program store (%#x)\n",
                pkt->getAddr(), *vaddrPtr, log.paddr);
    }
    else { // This is the callback for the log data block
        assert(pkt == log.logDataPkt);
        assert(pkt->getSize() == RubySystem::getBlockSizeBytes());
        // Write non-speculative values from log buffer
        // to data log cache block
        data.setData(pkt->getConstPtr<uint8_t>(),
                     0 /*offset*/,
                     RubySystem::getBlockSizeBytes() /*len*/);
        DPRINTF(RubyHTMlog, "Successfully logged program store "
                "  vaddr %#x (paddr %#x) into log paddr %#x\n",
                log.vaddr, log.paddr, address);
    }

    if (log.outstanding == 0 && log.completed == 2) {
        m_xact_mgr->commitLogEntry(log.logIndex, store_addr);
        DPRINTF(RubyHTMlog, "Done logging store to paddr %#x\n",
                store_addr);
        // Unlock block targeted by program store
        m_dataCache_ptr->setHtmLogPending(store_addr, false);
        DataBlock* storeDataPtr;
        bool hit = m_dataCache_ptr->
            tryCacheAccess(store_addr, RubyRequestType_ST,
                           storeDataPtr, false);
        assert(hit);
        Sequencer::writeCallback(store_addr, *storeDataPtr);
        if (m_xact_mgr->config_enableValueChecker()) {
            // Record old value we just logged
            m_ruby_system->getXactValueChecker()->
                notifyLoggedDataBlock(m_version,address, data);
        }

        // Erase log request
        bool found = m_logRequestTable.erase(store_addr);
        assert(found);
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
    assert(!m_failedCallback);
    if (!m_htm->params().lazy_vm) { // LogTM
        assert(m_htm->params().eager_cd);
        assert(address == makeLineAddress(address));
        assert(m_RequestTable.find(address) != m_RequestTable.end());
        auto &seq_req_list = m_RequestTable[address];

        assert(!seq_req_list.empty());
        SequencerRequest &seq_req = seq_req_list.front();
        assert(seq_req.pkt->isWrite());

        bool needsLogging =
            m_pendingLogging.find(address) != m_pendingLogging.end();
        if (needsLogging) {
            assert(seq_req.pkt->isHtmTransactional() &&
                   !m_xact_mgr->checkWriteSignature(address));
        }
        if (needsLogging) {
            // Lock this line in cache until logging done, we need it
            // for the callback
            m_dataCache_ptr->setHtmLogPending(address, true);
            // Add to write set in order to start detecting conflicts
            int thread = 0;
            m_xact_mgr->isolateTransactionStore(thread, address);
            /* Generate log requests and use
               Sequencer::makeRequest(pkt) to handle them. Once both
               log requests completed, then callback CPU to complete the
               program store. Need to keep track of each
               "oustanding" store and its associated log requests.
            */
            // Check if there  outstanding log request for the
            // cache line targeted by this write.
            assert(m_logRequestTable.find(address) ==
                   m_logRequestTable.end());

            // Create log requests/packets
            LogRequestInfo log_req = buildLogPackets(seq_req.pkt, data);
            // Try to issue them to cache
            if (makeLogRequests(log_req)) {
                bool found = m_pendingLogging.erase(address);
                assert(found);
                // Record their status via logReqTable
                auto &log_req_list = m_logRequestTable[address];
                assert(log_req_list.empty());
                log_req_list.emplace_back(log_req);
                DPRINTF(RubyHTMlog, "Store to paddr %#x"
                        " must wait until log requests done\n",
                        address);
                // Do not call hitCallback until log requests done. Will
                // be done by handleStoresToLog when logging completes
                return;
            } else {
                panic("Cannot make log requests!\n");
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
