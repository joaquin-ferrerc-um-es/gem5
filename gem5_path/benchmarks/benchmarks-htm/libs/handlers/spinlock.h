#ifndef SPINLOCK_H
#define SPINLOCK_H 1

#define CACHE_LINE_SIZE_BYTES 64
#define NUM_GLOBAL_LOCKS 2
#define PADDED_ARRAY_SIZE_BYTES (CACHE_LINE_SIZE_BYTES * NUM_GLOBAL_LOCKS)

#if defined (AARCH64)

#include <stdatomic.h>

typedef atomic_int lock_t;

typedef struct {
    lock_t lock;
    char padding[CACHE_LINE_SIZE_BYTES - sizeof(lock_t)];
} spinlock_t __attribute__ ((aligned (CACHE_LINE_SIZE_BYTES)));

spinlock_t fallbackLock;

static inline void * spinlock_getAddress()
{
    return (void *) &fallbackLock.lock;
}

static inline void spinlock_init()
{
    atomic_init(&fallbackLock.lock, 0);
}

static inline long spinlock_isLocked()
{
    return atomic_load_explicit(&fallbackLock.lock,
                                memory_order_acquire);
}

static inline void spinlock_whileIsLocked()
{
  while (spinlock_isLocked()) {
      // __yield();
  }
}

static inline void spinlock_lock()
{
    do {
        spinlock_whileIsLocked();
    }
    while (atomic_exchange_explicit(&fallbackLock.lock, 1,
                                    memory_order_acquire));
}


static inline void spinlock_unlock()
{
    atomic_store_explicit(&fallbackLock.lock, 0, memory_order_release);
}

#elif defined (X86)


#include <assert.h>
#include <stdio.h>
#include <xmmintrin.h>

volatile char lock_array[PADDED_ARRAY_SIZE_BYTES]
__attribute__ ((aligned (CACHE_LINE_SIZE_BYTES))) ;

typedef struct {
    long lock;
    char padding[CACHE_LINE_SIZE_BYTES-8];
} spinlock_t __attribute__ ((aligned (CACHE_LINE_SIZE_BYTES)));

typedef struct {
    volatile long * fallbackLock;
    volatile long * preFallbackLock; // see HANDLER_FALLBACKLOCK_2PHASE
} lockPtr_t __attribute__ ((aligned (CACHE_LINE_SIZE_BYTES)));

lockPtr_t locks;

static inline void * spinlock_getAddress()
{
    return (void *)locks.fallbackLock;
}
static inline void spinlock_init()
{
  /* If we ever need to use more than one lock, make sure to make room
     in each one maps to a different cache line in the lock array,
     which should be adequately sized using NUM_GLOBAL_LOCKS */
  int numLock = 0;
  assert(CACHE_LINE_SIZE_BYTES*numLock < sizeof(lock_array));
  (locks.fallbackLock) = (long *)&lock_array[CACHE_LINE_SIZE_BYTES*numLock];
  *(locks.fallbackLock) = 0;

  ++numLock;
  (locks.preFallbackLock) = (long *)&lock_array[CACHE_LINE_SIZE_BYTES*numLock];
  *(locks.preFallbackLock) = 0;

  // Sanity check: remember to increase lock_array size accordingly
  assert(numLock < NUM_GLOBAL_LOCKS);
}

static inline long spinlock_isLocked()
{
  return *(locks.fallbackLock) != 0;
}

static inline void spinlock_whileIsLocked()
{
  while (spinlock_isLocked()) {
    _mm_pause();
  }
}

static inline void spinlock_lock()
{
    do {
        spinlock_whileIsLocked();
    }
    while (!__sync_bool_compare_and_swap((locks.fallbackLock), 0, 1));
}


static inline void spinlock_unlock()
{
    __asm__ volatile (""); // acts as a memory barrier.
    *(locks.fallbackLock) = 0;
}

static inline long spinlock_prefb_isLocked()
{
  return *(locks.preFallbackLock) != 0;
}

static inline void spinlock_prefb_whileIsLocked()
{
  while (spinlock_prefb_isLocked()) {
    _mm_pause();
  }
}

static inline void spinlock_prefb_lock()
{
    do {
        spinlock_prefb_whileIsLocked();
    }
    while (!__sync_bool_compare_and_swap((locks.preFallbackLock), 0, 1));
}


static inline void spinlock_prefb_unlock()
{
    __asm__ volatile (""); // acts as a memory barrier.
    *(locks.preFallbackLock) = 0;
}
#endif // ARCH

#endif /* SPINLOCK_H */
