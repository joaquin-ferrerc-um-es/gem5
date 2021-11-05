#include <assert.h>

#include "abort_codes.h"
#include "htm.h"

#if defined AARCH64

#include "../isa/aarch64/tme.h"

#elif defined X86

#include "../isa/x86/abort_status.h"
#include "../isa/x86/tsx.h"

#endif


#if defined (AARCH64)
#include <arm_acle.h>

uint64_t htm_start(uint64_t arg) {
    return __tstart();
}

void htm_commit(uint64_t arg) {
    __tcommit();
}

bool htm_started(uint64_t status) {
    return status == 0;
}

void htm_cancel(uint64_t code) {
    // __tcancel expects a 16-bit immediate, so need this hack: Make
    // sure the abort codes used by benchmarks are reflected in this
    // switch Currently, only code 0 allowed for application-triggered
    // aborts (STAMP benchmarks do not pass any abort code, so we
    // invariably pass a zero value)
    switch(code) {
    case 0: __tcancel(0);
    default:
    __builtin_unreachable();
    }
}

void htm_cancel_lock_acquired() {
    __tcancel(TME_LOCK_IS_ACQUIRED);
    __builtin_unreachable();
}

bool htm_abort_cause_conflict(uint64_t status) {
    return (status & _TMFAILURE_MEM);
}

bool htm_abort_cause_explicit(uint64_t status) {
    return (status & _TMFAILURE_CNCL);
}

uint16_t htm_abort_cause_explicit_code(uint64_t status) {
    assert(status & _TMFAILURE_CNCL);
    return TME_FAILURE_REASON_DECODE(status);
}

bool htm_abort_code_is_lock_acquired(uint16_t abort_code) {
    return abort_code == TME_LOCK_IS_ACQUIRED;
}

bool htm_may_succeed_on_retry(uint64_t status) {
    return status & _TMFAILURE_RTRY;
}

bool htm_abort_cause_disabled(uint64_t abort_status) {
    //return (abort_status & _XABORT_DISABLED); // TODO: lockstep support
    return false;
}

#elif defined (X86)


uint64_t xbegin(unsigned long flags, unsigned long arg)
{
    // TODO: For now, the abort handler offset is fixed to 0, so that
    // the abort handler invariably begins at the instruction
    // immediately after the xbegin. This doesn't need to be the case,
    // as xbegin takes a rel32 offset as immediate operand, though its
    // the usual value is 0
    // unsigned long abort_handler_offset = 0;
    // Pass flags to simulator in RDI, additional arg in RSI
    return M5_XBEGIN(flags, arg);
    // Do not use intrinsic, as we want to return a 64-bit value (RAX)
    // rather than 32 bits (EAX) in order to return virtual addresses
    // (48 bit)
}

void xend(unsigned long tag) {
    // Pass tag to simulator RDI. Also, this macro ensures the
    // compiles knows that rax gets clobbered by xend,
    M5_XEND(tag);
}

uint64_t htm_start(uint64_t arg) {
    return xbegin(arg, 0);
}
void htm_commit(uint64_t arg) {
    xend(arg);
}

bool htm_started(uint64_t code) {
    return (code == _XBEGIN_STARTED);
}

void htm_cancel(uint64_t code) {
    // Pass abort code to simulator via RDI
    M5_XABORT(code);
    assert(false); // Unreachable
}

void htm_cancel_lock_acquired() {
    // Pass abort code to simulator via RDI.
    // NOTE: 0xff is hard-coded as explicit abort because of lock acquired
    M5_XABORT(XABORT_CODE_FALLBACK_LOCK_LOCKED);
    assert(false); // Unreachable
}

bool htm_abort_cause_conflict(uint64_t abort_status) {
    return (abort_status & _XABORT_CONFLICT);
}

bool htm_abort_cause_explicit(uint64_t abort_status) {
    return (abort_status & _XABORT_EXPLICIT);
}

uint16_t htm_abort_cause_explicit_code(uint64_t abort_status) {
    assert(abort_status & _XABORT_EXPLICIT);
    return (_XABORT_CODE_DECODE(abort_status));
}

bool htm_abort_code_is_lock_acquired(uint16_t abort_code) {
    return (abort_code == XABORT_CODE_FALLBACK_LOCK_LOCKED);
}

bool htm_may_succeed_on_retry(uint64_t abort_status) {
    return (abort_status & _XABORT_RETRY);
}

bool htm_abort_cause_disabled(uint64_t abort_status) {
    return (abort_status & _XABORT_DISABLED);
}

#endif
