// rnbo_allocator_v2.cpp
// ---------------------------------------------------------------------------
// Custom memory allocator for RNBO, backed by a large static pool in the
// Daisy's 64MB SDRAM.  (v2)
//
// WHAT CHANGED FROM v1 AND WHY:
// v1's realloc() was broken. It did:  memcpy(newp, ptr, size)  where `size` is
// the NEW (usually larger) size -- but the OLD block was smaller, so the copy
// read PAST the end of the old block and pulled garbage into the resized
// buffer. Any RNBO storage that is grown via realloc (its internal `list`
// objects -- e.g. the FFT twiddle tables built by push() -- and buffer~ /
// DataRef storage) ended up corrupt on-device, while desktop RNBO (real libc
// realloc, which knows the block size) was fine. Symptom: scrambled audio even
// for a bare FFT round-trip or a plain windowed overlap-add through buffer~.
//
// THE FIX: a bump allocator can't know an old block's size for free, so every
// allocation now carries an 8-byte header storing its usable size. realloc()
// reads that header and copies min(oldSize, newSize) -- never over-reading.
//
// Everything else is unchanged: allocate-once usage, free() is a no-op.
// ---------------------------------------------------------------------------

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "daisy_seed.h"   // for DSY_SDRAM_BSS

// 16 MB pool in SDRAM.
#define RNBO_POOL_BYTES (16 * 1024 * 1024)

// Bytes reserved before each returned block to store its usable size.
// 8 (not sizeof(size_t)=4 on this M7) so the returned pointer stays 8-byte
// aligned for doubles/pointers.
#define RNBO_HEADER_BYTES 8

static char DSY_SDRAM_BSS s_rnboPool[RNBO_POOL_BYTES];
static size_t s_rnboOffset = 0;

// 8-byte alignment for safety with doubles/pointers.
static inline size_t alignUp(size_t n) {
    return (n + 7u) & ~((size_t)7u);
}

namespace RNBO {
namespace Platform {

void *malloc(size_t size) {
    size = alignUp(size);                       // keep offset a multiple of 8
    size_t need = RNBO_HEADER_BYTES + size;

    if (s_rnboOffset + need > RNBO_POOL_BYTES) {
        return nullptr;                         // pool exhausted
    }

    char *base = &s_rnboPool[s_rnboOffset];      // 8-aligned (offset & pool are)
    *reinterpret_cast<size_t *>(base) = size;    // header: usable size
    s_rnboOffset += need;
    return base + RNBO_HEADER_BYTES;             // payload, 8-aligned
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
    if (ptr == nullptr) {
        return RNBO::Platform::malloc(size);     // first alloc: nothing to copy
    }

    // Recover the old block's usable size from its header, then copy only the
    // smaller of old/new so we never read past the old block.
    size_t oldSize = *reinterpret_cast<size_t *>(
        reinterpret_cast<char *>(ptr) - RNBO_HEADER_BYTES);

    void *newp = RNBO::Platform::malloc(size);
    if (newp) {
        size_t copyBytes = (oldSize < size) ? oldSize : size;
        memcpy(newp, ptr, copyBytes);
    }
    return newp;                                  // old block abandoned (bump)
}

} // namespace Platform
} // namespace RNBO
