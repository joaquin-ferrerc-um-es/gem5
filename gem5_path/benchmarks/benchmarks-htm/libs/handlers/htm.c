#include <assert.h>

#include "abort_codes.h"
#include "htm.h"

#if defined (AARCH64)
#include <arm_acle.h>

#if 0
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
    case 0: __tcancel(CANCEL_TRANSACTION_DEFAULT_CODE);
    default:
    __builtin_unreachable();
    }
}

void htm_cancel_lock_acquired() {
    __tcancel(TME_CODE_FALLBACK_LOCK_LOCKED);
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
    return abort_code == TME_CODE_FALLBACK_LOCK_LOCKED;
}

bool htm_may_succeed_on_retry(uint64_t status) {
    return status & _TMFAILURE_RTRY;
}

bool htm_abort_cause_disabled(uint64_t abort_status) {
    return (abort_status & _TMFAILURE_DISABLED);
}
#endif

#elif defined (X86)



#endif
