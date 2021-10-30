#include <stdbool.h>
#include <stdint.h>

uint64_t htm_start(uint64_t arg) ;
void htm_commit(uint64_t arg) ;
void htm_cancel(uint64_t code) ;
bool htm_started(uint64_t status) ;
void htm_cancel_lock_acquired() ;
bool htm_abort_cause_conflict(uint64_t status) ;
bool htm_abort_cause_explicit(uint64_t status) ;
uint16_t htm_abort_cause_explicit_code(uint64_t status) ;
bool htm_abort_code_is_lock_acquired(uint16_t abort_code) ;
bool htm_may_succeed_on_retry(uint64_t status) ;
bool htm_abort_cause_disabled(uint64_t status) ;
