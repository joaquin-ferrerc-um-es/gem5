/*
  Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
  Universidad de Murcia

  GPLv2, see file LICENSE.
*/

#include "cpu/checker/htm_checker.hh"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <iomanip>
#include <iostream>

#include "debug/HTMChecker.hh"
#include "enums/LockStepMode.hh"
#include "sim/system.hh"

namespace gem5
{

#define _unused(x) ((void)(x))

using namespace std;

HTMChecker::HTMChecker(const std::string &_my_name,
                       BaseCPU *_cpu) :
    BaseHTMChecker(_my_name, _cpu),
    _name(_my_name),
    cpu(_cpu),
    recorder(_my_name + ".recorder", _cpu, this),
    replayer(_my_name + ".replayer", _cpu, this),
    hasFallbackLock(false),
    lastFallbackLockReadValue(0),
    valuesFifoFileDescriptor(-1),
    valueRecordGlobalSeqNo(0),
    fallbackLockVirtAddr(0),
    faultPC(0)
{
    if (cpu->system->getHTM() != NULL) {
        fallbackLockVirtAddr =
            cpu->system->getHTM()->getFallbackLockVAddr();
    } else {
        panic("HTM system configuration unset!\n");
    }
}


HTMChecker::Recorder::Recorder(const std::string &_my_name,
                               BaseCPU *_cpu,
                               HTMChecker *_checker) :
    _name(_my_name),
    cpu(_cpu),
    checker(_checker)
{

}

HTMChecker::Replayer::Replayer(const std::string &_my_name,
                               BaseCPU *_cpu,
                               HTMChecker *_checker) :
    _name(_my_name),
    cpu(_cpu),
    checker(_checker),
    currentValueRecordLocalIndex(0),
    inSyncWithRecorder(false),
    retiredMemRefIgnoredBegin(0),
    retiredMemRefIgnoredEnd(0)
{
}

void
HTMChecker::begin(uint64_t xid) {
    if (cpu->system->getLockstepMode() == enums::record) {
        recorder.begin(xid);
    } else if (cpu->system->getLockstepMode() == enums::replay) {
        // replayer begins transactions that immediately abort, so
        // ignore this.. value checking begins when lock acquisition
        // detected via retiredMemRef
    } else {
    }
}

void
HTMChecker::commit(uint64_t xid) {
    if (cpu->system->getLockstepMode() == enums::record) {
        // called upon htm_stop instruction
        recorder.commit(xid);
    }
    else { // replayer can never commit a transaction, values are
           // checked when lock released detected by retiredMemRef
        assert(cpu->system->getLockstepMode() == enums::disabled);
    }
}


void
HTMChecker::Recorder::begin(uint64_t xid)
{
    // Called when lock acquisition detected
    assert(checker->values.empty());
}

bool
HTMChecker::canReplay(uint64_t xid) {
    if (cpu->system->getLockstepMode() == enums::replay) {
        return replayer.canReplay(xid);
    } else {
        return true;
    }
}

bool
HTMChecker::Replayer::canReplay(uint64_t xid) {
    if (!cpu->system->getLockstepManager()->
        canReplay(cpu->cpuId(), xid)) {
        // Cannot begin transaction, not next in commit order
        return false;
    }

    DPRINTF(HTMChecker, "replayer allowed replay\n");
    return true;
}

void
HTMChecker::Replayer::begin(uint64_t xid) {
    assert(checker->values.empty());
    assert(checker->hasFallbackLock);
    // Read recorded values from pipe
    cpu->system->getLockstepManager()->
        readReplayValuesFromFile(checker->values,
                                 checker->valuesFifoFileDescriptor,
                                 checker->valueRecordGlobalSeqNo);
    DPRINTF(HTMChecker, "replayer begin\n");
}

void
HTMChecker::Recorder::commit(uint64_t xid)
{
    DPRINTF(HTMChecker, "recorder commit\n");
    // Record this commit in global pipe to release transaction
    // running in lockstep (replayer)
    cpu->system->getLockstepManager()->
        recordCommit(cpu->cpuId(), xid);

    cpu->system->getLockstepManager()->
        dumpRecordedValuesToFile(checker->values,
                                 checker->valuesFifoFileDescriptor);

    checker->values.clear();
}

void
HTMChecker::Replayer::commit(uint64_t xid) {
    assert(checker->hasFallbackLock);
    // Called upon detection of fallback lock release
    cpu->system->getLockstepManager()->
        markReplayed(cpu->cpuId(), xid);

    DPRINTF(HTMChecker, "replayer commit\n");
    // Must have checked all records before commit
    if (currentValueRecordLocalIndex != checker->values.size()) {
        panic("Value check failed: commit reached"
              " before all values were checked\n");
    }
    currentValueRecordLocalIndex = 0; // Reset index
    inSyncWithRecorder = false;
    assert(retiredMemRefIgnoredBegin < 10); // TODO: Automate check
    assert(retiredMemRefIgnoredEnd < 2); // TODO: Automate check
    retiredMemRefIgnoredBegin = 0;
    retiredMemRefIgnoredEnd = 0;
    checker->values.clear();
}

void
HTMChecker::abort() {
    if (cpu->system->getLockstepMode() == enums::record) {
        // Clear all recorded values upon abort
        recorder.abort();
    } else if (cpu->system->getLockstepMode() == enums::replay) {
        // Replayer ignores abort signals while spinning on xbegin
        // before canReplay returns true
    }
}

void
HTMChecker::Recorder::abort()
{
    checker->valueRecordGlobalSeqNo -= checker->values.size();
    checker->values.clear();
    DPRINTF(HTMChecker, "recorder abort\n");
}

void
HTMChecker::retireInst(bool isMemRef, bool isTransactional,
                       Trace::InstRecord *traceData) {
    if (cpu->system->getLockstepMode() == enums::record ||
        cpu->system->getLockstepMode() == enums::replay) {
        if (isTransactional || hasFallbackLock) {
            // Detect entry/exit into/from kernel during
            // transactions (no record/replay)
            if (faultPC != 0){
                if (faultPC == traceData->getPCState().instAddr()) {
                    if (traceData->getPCState().microPC() < 32768) {
                        DPRINTF(HTMChecker, "Resuming value recording after "
                                "handling interrupt/fault at PC %#x\n",
                                faultPC);
                        faultPC = 0;
                    } else {
                        DPRINTF(HTMChecker, "Current PC matches fault PC %#x"
                                " but microPC is not 0 (upc=%#x)\n",
                                faultPC, traceData->getPCState().microPC());
                    }
                }
            }
            // Detect trap to kernel code
            else if (traceData->getPCState().microPC() >= 32768 &&
                     faultPC == 0) {
                // Save int/fault pc
                faultPC = traceData->getPCState().instAddr();
                DPRINTF(HTMChecker, "Skipping value recording while "
                        "handling interrupt/fault at PC %#x\n",
                        faultPC);
            }
        }
    }
    if (!isMemRef)
        return;
    bool isStore =traceData->getStaticInst()->isStore();
    if (traceData->getStaticInst()->isHtmCmd()) {
        // Skip htm commands
    } else if (traceData->getAddr() == fallbackLockVirtAddr) {
        if (!isStore) {
            lastFallbackLockReadValue = traceData->getIntData();
        } else if (isStore) {
            if (traceData->getIntData() == 0) { // Unlock
                assert(hasFallbackLock &&
                       (lastFallbackLockReadValue == 1));
                DPRINTF(HTMChecker, "lock released\n");
                // Check replayed values at end of critical section
                if (cpu->system->getLockstepMode() == enums::replay) {
                    // Notify replayer
                    replayer.commit(0);
                } else if (cpu->system->getLockstepMode() == enums::record) {
                    // Record values of non-spec transaction
                    recorder.commit(0);
                }
                hasFallbackLock = false;
            } else if ((traceData->getIntData() == 1) &&
                       (lastFallbackLockReadValue == 0)) {
                DPRINTF(HTMChecker, "lock acquired\n");
                assert(!hasFallbackLock);
                hasFallbackLock = true;
                if (cpu->system->getLockstepMode() == enums::replay) {
                    replayer.begin(0);
                } else if (cpu->system->getLockstepMode() == enums::record) {
                    // Record values of non-spec transaction
                    recorder.begin(0);
                }
            } else if (traceData->getIntData() == 1) {
                /* Stored value may be 1 if read data was 1
                   (lock already acquired), so need to check last
                   value seen for lock in order to detect if this is a
                   successful "acquire" */
                DPRINTF(HTMChecker, "compare-and-swap found busy lock\n");
            } else {
                panic("Unexpected value for fallback lock");
            }
        }
    } else { // Not an access to the lock
        if (cpu->system->getLockstepMode() == enums::record) {
            if (isTransactional || hasFallbackLock) {
                /* Record transactional values as well as those in irrevocable
                   transactions (acq fallback lock, non speculative sections)
                */
                recorder.recordValue(isStore, traceData);
            }
        }
        else if (cpu->system->getLockstepMode() == enums::replay) {
            if (hasFallbackLock) {
                replayer.checkValue(isStore, traceData);
            }
        }
    }
}

void
HTMChecker::openFifos()
{
        // HTM Checker support
    if (cpu->system->getLockstepMode() == enums::replay) {
        replayer.openFifo();
    }
    else if (cpu->system->getLockstepMode() == enums::record) {
        recorder.openFifo();
    }
}

void
HTMChecker::createFifos()
{
    if (cpu->system->getLockstepMode() == enums::record) {
        recorder.createFifo();
    }
}

void
HTMChecker::Recorder::createFifo()
{
    if (cpu->system->getLockstepMode() == enums::record) {

        std::string fifoPath = cpu->system->getLockstepManager()->
            getValuesFifoPath(cpu->cpuId());

        /* create and open the FIFO (named pipe) */
        if (mkfifo(fifoPath.c_str(), 0666) < 0) {
            perror("cannot make xact values fifo");
            fatal("Lockstep mode (record): "
                  "failed to create named pipe");
        }
    }
}

void
HTMChecker::Recorder::openFifo()
{
    assert(cpu->system->getLockstepMode() == enums::record);
    //  Opening the read or write end of a FIFO blocks until the
    //  other end is also opened
    std::string fifoPath = cpu->system->getLockstepManager()->
        getValuesFifoPath(cpu->cpuId());
    int fd = ::open(fifoPath.c_str(), O_WRONLY);
    if (fd < 0) {
        perror("cannot open xact values fifo");
        fatal("Lockstep mode (record): "
              "failed to open named pipe");
    }
    else {
        assert(fd >= 0);
        checker->valuesFifoFileDescriptor = fd;
    }
}


void
HTMChecker::Replayer::openFifo() {

    if (cpu->system->getLockstepMode() == enums::replay) {
        std::string fifoPath = cpu->system->getLockstepManager()->
            getValuesFifoPath(cpu->cpuId());
        int fd;
        do {
            fd = ::open(fifoPath.c_str(), O_RDONLY | O_NONBLOCK);
        }
        while (fd < 0);
        checker->valuesFifoFileDescriptor = fd;
    }
}


void
HTMChecker::Recorder::recordValue(bool isStore, Trace::InstRecord *traceData) {
    if (!traceData)
        fatal("lockStep mode requires 'Exec' debug flag set\n");;

    uint64_t addr = traceData->getAddr();
    uint64_t value = traceData->getIntData();
    Addr pc = traceData->getPCState().instAddr();
    MicroPC upc = traceData->getPCState().microPC();

    if (checker->hasFallbackLock) {
        if (checker->faultPC != 0) {
            DPRINTF(HTMChecker, "skipping value record during faults\n");
            // Do not record mem refs from interrupts/faults
            return;
        }
    }
    DPRINTF(HTMChecker,
            "value recorded for PC: %#x.%d (global seqno: %d)\n",
            pc, upc, checker->valueRecordGlobalSeqNo);

    // Records each value loaded and stored by the transaction
    checker->values.push_back(ValueRecord(isStore, pc, upc, addr, value,
                                 checker->valueRecordGlobalSeqNo++));
}

void
HTMChecker::Replayer::checkValue(bool isStore, Trace::InstRecord *traceData) {
    if (!traceData)
        fatal("lockStep mode requires 'Exec' debug flag set\n");;

    if (checker->faultPC != 0) {
        // Do not check mem refs from interrupts/faults
        DPRINTF(HTMChecker, "skipping value replay during faults\n");
        return;
    }

    uint64_t value = traceData->getIntData();
    uint64_t address = traceData->getAddr();
    Addr pc = traceData->getPCState().instAddr();
    MicroPC upc = traceData->getPCState().microPC();

    if (!inSyncWithRecorder) {
        assert(currentValueRecordLocalIndex == 0);
        // Search value record for matching return (matching
        // target address in the stack and return address)
        for (int i=0; i < checker->values.size(); ++i) {
            ValueRecord& record = checker->values[i];
            if (pc == record.pc &&
                upc == record.upc &&
                isStore == record.wasStore &&
                address == record.address &&
                value == record.value) {
                inSyncWithRecorder = true;
                currentValueRecordLocalIndex = i+1;
                checker->valueRecordGlobalSeqNo +=
                    currentValueRecordLocalIndex;
                DPRINTF(HTMChecker,
                        "replayer correctly sync'ed with value record\n");
                return;
            }
        }
        retiredMemRefIgnoredBegin++;
        return; // Wait until replayer in sync with recorder
    }
    if (currentValueRecordLocalIndex >= checker->values.size()) {
        // The recorded transaction must be non-speculative
        retiredMemRefIgnoredEnd++;
        return;
    }
    ValueRecord& record = checker->values[currentValueRecordLocalIndex];

    bool pcMismatch = false;
    bool typeMismatch = false;
    bool addrMismatch = false;
    bool valueMismatch = false;
    if (pc != record.pc) {
        pcMismatch = true;
    } else {
        assert(upc == record.upc);
    }
    if (isStore != record.wasStore) {
        typeMismatch = true;
    }
    if (address != record.address) {
        addrMismatch = true;
    }
    if (value != record.value) {
        valueMismatch = true;
    }

    if (pcMismatch || typeMismatch || addrMismatch || valueMismatch) {
        if (!pcMismatch && !typeMismatch && !valueMismatch) {
            // Only address mismatch, this could be due to running a
            // different thread than the recorder cpu
            if ((address & 0xffffff0000000000) == 0x7f0000000000) {
                // mistmatched address in stack access?
                cerr << "It looks like CPU " << cpu->cpuId()
                     << " may be executing different threads in recorder"
                     << " and replayer simulations" << endl;
                cerr << "IMPORTANT NOTICE: Lockstep record/replay"
                     << " requires thread pinning (e.g. via set_affinity)"
                     << endl;
            }
        }
        cerr << "Replayer CPU " << setw(2) << cpu->cpuId()
             << " detected mismatch with recorder CPU ("
             << (valueMismatch ? "value" : (addrMismatch ? "address" : "type"))
             << ") Record/replay pairs:" << endl
             << "PC: " << hex << record.pc << "."
             << record.upc << "/" << pc << "." << upc << endl
             << "Address: " << hex << record.address << "/" << address << endl
             << "Value:   " << hex << record.value << "/" << value << endl
             << "Type: " << (record.wasStore ? "store" : "load")
             << "/" << (isStore ? "store" : "load") << endl
             << "Replayer CPU " << (checker->hasFallbackLock ?
                                    "has lock" : "in tx") << endl << dec
             << "GlobalSeqNo=" << checker->valueRecordGlobalSeqNo
             << " LocalIndex=" << currentValueRecordLocalIndex
             << endl;


        fatal("lockstep replayer detected mistmach with recorder CPU");
    }
    DPRINTF(HTMChecker,
            "value check passed for PC: %#x.%d (global seqno: %d)\n",
            pc, upc, checker->valueRecordGlobalSeqNo);


    _unused(value);
    _unused(address);
    _unused(record);

    checker->valueRecordGlobalSeqNo++;

    currentValueRecordLocalIndex++; // Move on to next record
}

} // namespace gem5
