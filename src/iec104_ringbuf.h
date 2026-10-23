/**
 * @file iec104_ringbuf.h
 * @brief MPSC (Multiple-Producer, Single-Consumer) ring buffer
 *        for marshalling point updates from protocol threads to the
 *        GTK main thread without per-element heap allocation.
 *
 * Fixes VULN-1 (auto-create IOA exhaustion) and VULN-4 (unbounded
 * g_idle_add queue) by replacing per-point g_idle_add with a fixed-size
 * ring buffer that the GTK thread drains in batches.
 *
 * Thread safety contract:
 *   - ANY number of threads may call iec104_ring_push  (protocol threads)
 *   - Exactly ONE thread calls iec104_ring_pop         (GTK main thread)
 *   - Push side is serialised with a lightweight spinlock.
 *   - Pop side is lock-free (single consumer).
 */
#ifndef IEC104_RINGBUF_H
#define IEC104_RINGBUF_H

#include "iec104_types.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Platform-specific includes for spinlock */
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/* --------------------------------------------------------------------
 * Configuration
 * ----------------------------------------------------------------- */
#define IEC104_RING_SIZE  4096   /* must be power of 2 */
#define IEC104_RING_MASK  (IEC104_RING_SIZE - 1)

/* Maximum number of auto-created points across all connections.
 * Beyond this limit, unknown IOAs are silently dropped. */
#define IEC104_MAX_AUTO_POINTS  64

/* --------------------------------------------------------------------
 * Ring entry — mirrors the old PointUpdateData
 * ----------------------------------------------------------------- */
typedef struct {
    int            conn_idx;
    int            ioa;
    IEC104_Value   val;
    IEC104_Quality q;
    uint64_t       ts_ms;
    int            asdu_type;
} IEC104_RingEntry;

/* --------------------------------------------------------------------
 * Ring buffer structure (MPSC — Multi-Producer, Single-Consumer)
 * ----------------------------------------------------------------- */
typedef struct {
    IEC104_RingEntry entries[IEC104_RING_SIZE];
    volatile uint32_t head;   /* next write position (any protocol thread)  */
    volatile uint32_t tail;   /* next read  position (GTK thread only)      */
#ifdef _WIN32
    CRITICAL_SECTION  push_lock;   /* serialises push from multiple threads */
#else
    pthread_mutex_t   push_lock;
#endif
} IEC104_PointRing;

/* --------------------------------------------------------------------
 * Inline operations
 * ----------------------------------------------------------------- */

/** Initialise ring buffer (call once at startup). */
static inline void iec104_ring_init(IEC104_PointRing *ring)
{
    memset(ring->entries, 0, sizeof(ring->entries));
    ring->head = 0;
    ring->tail = 0;
#ifdef _WIN32
    InitializeCriticalSection(&ring->push_lock);
#else
    pthread_mutex_init(&ring->push_lock, NULL);
#endif
}

/** Destroy ring buffer resources. */
static inline void iec104_ring_destroy(IEC104_PointRing *ring)
{
#ifdef _WIN32
    DeleteCriticalSection(&ring->push_lock);
#else
    pthread_mutex_destroy(&ring->push_lock);
#endif
}

/** Is the ring full? (caller must hold push_lock or be single consumer) */
static inline bool iec104_ring_full(const IEC104_PointRing *ring)
{
    return ((ring->head + 1) & IEC104_RING_MASK) == ring->tail;
}

/**
 * Push one entry into the ring (ANY protocol thread — thread-safe).
 *
 * If the ring is full the entry is DROPPED and the function returns
 * false.  This guarantees bounded memory even under sustained ASDU
 * flood.
 */
static inline bool iec104_ring_push(IEC104_PointRing *ring,
                                     const IEC104_RingEntry *entry)
{
    bool ok;

    /* Lock: serialise concurrent pushes from multiple connection threads */
#ifdef _WIN32
    EnterCriticalSection(&ring->push_lock);
#else
    pthread_mutex_lock(&ring->push_lock);
#endif

    if (iec104_ring_full(ring)) {
        ok = false;   /* drop — ring full */
    } else {
        ring->entries[ring->head] = *entry;

        /* Compiler fence: ensure entry data is visible before head advances */
#if defined(__GNUC__) || defined(__clang__)
        __asm__ volatile("" ::: "memory");
#elif defined(_MSC_VER)
        _ReadWriteBarrier();
#endif

        ring->head = (ring->head + 1) & IEC104_RING_MASK;
        ok = true;
    }

#ifdef _WIN32
    LeaveCriticalSection(&ring->push_lock);
#else
    pthread_mutex_unlock(&ring->push_lock);
#endif

    return ok;
}

/**
 * Pop one entry from the ring (GTK main thread ONLY — lock-free).
 * Returns false if the ring is empty.
 */
static inline bool iec104_ring_pop(IEC104_PointRing *ring,
                                    IEC104_RingEntry *out)
{
    if (ring->tail == ring->head)
        return false;   /* empty */

    *out = ring->entries[ring->tail];

#if defined(__GNUC__) || defined(__clang__)
    __asm__ volatile("" ::: "memory");
#elif defined(_MSC_VER)
    _ReadWriteBarrier();
#endif

    ring->tail = (ring->tail + 1) & IEC104_RING_MASK;
    return true;
}

#endif /* IEC104_RINGBUF_H */
