// rnbo_allocator.cpp
// ---------------------------------------------------------------------------
// Custom memory allocator for RNBO, backed by a large static pool in the
// Daisy's 64MB SDRAM.
//
// WHY THIS EXISTS:
// RNBO allocates its buffers (delay lines, allpass buffers, signal buffers)
// via Platform::malloc / calloc / realloc during initialize()/prepareToProcess.
// By default these route to newlib's heap, which on this build lives in the
// small (128KB, mostly-used) DTCMRAM region. A reverb's delay lines exhaust it
// -> allocation fails -> crash at boot.
//
// Defining RNBO_USECUSTOMALLOCATOR (in the Makefile C_DEFS) makes RNBO_Platform.h
// call the four functions below instead of the system heap. We hand out memory
// from an 8MB static pool placed in SDRAM (DSY_SDRAM_BSS), which has tens of MB
// free. This fixes the crash for THIS effect and every future RNBO effect.
//
// The allocator is a simple bump allocator with a minimal free-list-free design:
// RNBO allocates its buffers ONCE at init and keeps them for the lifetime of the
// program (audio DSP doesn't allocate per-block), so we never actually need to
// reclaim memory. free() is a near-no-op; realloc() copies to a fresh block.
// This is simple, fast, and correct for RNBO's allocate-once usage pattern.
// ---------------------------------------------------------------------------

#include <stddef.h>
#include <string.h>
#include "daisy_seed.h"   // for DSY_SDRAM_BSS

// 8 MB pool in SDRAM. You have ~44MB free, so this is comfortable and leaves
// room for multiple RNBO effects. Increase if you ever add very large effects.
#define RNBO_POOL_BYTES (8 * 1024 * 1024)

static char DSY_SDRAM_BSS s_rnboPool[RNBO_POOL_BYTES];
static size_t s_rnboOffset = 0;

// 8-byte alignment for safety with doubles/pointers.
static inline size_t alignUp(size_t n) {
    return (n + 7u) & ~((size_t)7u);
}

extern "C" {
// (internal pool helpers not strictly needed to be extern C, but harmless)
}

// ---------------------------------------------------------------------------
// When RNBO_USECUSTOMALLOCATOR is defined, RNBO_Platform.h DECLARES (but does
// not define) these four functions in namespace RNBO::Platform and calls them
// for all its allocations. We DEFINE them here, backed by the SDRAM pool.
// ---------------------------------------------------------------------------
namespace RNBO {
namespace Platform {

void *malloc(size_t size) {
    size = alignUp(size);
    if (s_rnboOffset + size > RNBO_POOL_BYTES) {
        return nullptr; // pool exhausted
    }
    void *p = &s_rnboPool[s_rnboOffset];
    s_rnboOffset += size;
    return p;
}

void *calloc(size_t count, size_t size) {
    size_t total = count * size;
    void *p = RNBO::Platform::malloc(total);
    if (p) {
        memset(p, 0, total);
    }
    return p;
}

void free(void * /*ptr*/) {
    // No-op: RNBO allocates once at init and keeps buffers for the program's
    // lifetime, so reclaiming is unnecessary. (Bump allocator.)
}

void *realloc(void *ptr, size_t size) {
    // Grow-only realloc at init: allocate fresh, copy, abandon old block.
    void *newp = RNBO::Platform::malloc(size);
    if (newp && ptr) {
        memcpy(newp, ptr, size);
    }
    return newp;
}

} // namespace Platform
} // namespace RNBO
