#ifndef ABORT_CODES_H
#define ABORT_CODES_H

#if defined AARCH64

#define TME_LOCK_IS_ACQUIRED    65535

// Masks to decode ret into fields
#define TME_FAILURE_REASON_DECODE(ret) (ret & _TMFAILURE_REASON)

#elif defined X86

// Bit-shifts to encode value into bit-field code
#define _XABORT_CODE_ENCODE(code)         ((((uint64_t)code) & 0xFF) << 24)

// Masks to decode ret into fields
#define _XABORT_CODE_DECODE(ret)         (uint64_t)(((ret) >> 24) & 0xFF)

// Set by abort handler if abort due to fallback lock locked after xbegin
#define XABORT_CODE_FALLBACK_LOCK_LOCKED 0xff

#endif

#define M5_XBEGIN_TAG_ENCODE(tag) (tag)

#define M5_XBEGIN_TAG_DECODE(ret) (ret & 0xFF)

#endif

