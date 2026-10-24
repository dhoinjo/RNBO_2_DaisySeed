// rnbo_allocator_v3.cpp
// ---------------------------------------------------------------------------
// Custom memory allocator for RNBO, backed by a large static pool in the
// Daisy's 64MB SDRAM, plus (v3) an optional small FAST pool in internal SRAM.
//
// WHAT CHANGED FROM v2 AND WHY:
// SDRAM is slow for data that is touched every sample (delay lines, filter
// state). v3 adds a second pool in the free part of the main internal SRAM
// (AXI-SRAM, where the program code lives). Every allocation tries the fast
// pool first and falls back to the SDRAM pool when the fast one is full.
//
// The fast pool size is set PER PEDAL in its profile (pedals/<colour>.mk):
//     FAST_POOL_KB = 128
// The Makefile turns that into -DRNBO_FAST_POOL_KB=128. Not set (or 0) means
// no fast pool: behaviour is exactly v2.
//
// The pool lives in the linker section .sram_axi_bss, which rnbo_fast_pool.lds
// places in SRAM right after the program. It takes room from the same 480 KB
// as the effects' code; if it doesn't fit, the build stops with
// "region `SRAM' overflowed".
//
// First come, first served: effects allocate at boot in effect-list order, so
// the first effect(s) in the list get the fast memory.
//
// Unchanged from v2: 8-byte size header per block (so realloc copies
// min(old,new)), free() is a no-op, realloc abandons the old block.
// ---------------------------------------------------------------------------

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "daisy_seed.h"   // for DSY_SDRAM_BSS

// 48 MB pool in SDRAM.
#define RNBO_POOL_BYTES (48 * 1024 * 1024)

// Fast pool size in KB, set per pedal by the Makefile (FAST_POOL_KB).
#ifndef RNBO_FAST_POOL_KB
#define RNBO_FAST_POOL_KB 0
#endif
#define RNBO_FAST_POOL_BYTES ((size_t)RNBO_FAST_POOL_KB * 1024u)

// Bytes reserved before each returned block to store its usable size.
// 8 (not sizeof(size_t)=4 on this M7) so the returned pointer stays 8-byte
// aligned for doubles/pointers.
#define RNBO_HEADER_BYTES 8

static char DSY_SDRAM_BSS s_rnboPool[RNBO_POOL_BYTES];
static size_t s_rnboOffset = 0;

#if RNBO_FAST_POOL_KB > 0
// NOLOAD section in internal SRAM (see rnbo_fast_pool.lds). Not zeroed at
// startup -- calloc() zeroes, same as for the SDRAM pool.
static char __attribute__((section(".sram_axi_bss"), aligned(32))) s_fastPool[RNBO_FAST_POOL_BYTES];
static size_t s_fastOffset = 0;
#endif

// 8-byte alignment for safety with doubles/pointers.
static inline size_t alignUp(size_t n) {
    return (n + 7u) & ~((size_t)7u);
}

// Carve `need` bytes from a pool; nullptr if it doesn't fit.
static inline char *carve(char *pool, size_t poolBytes, size_t &offset, size_t need) {
    if (offset + need > poolBytes) {
        return nullptr;
    }
    char *base = pool + offset;
    offset += need;
    return base;
}

namespace RNBO {
namespace Platform {

void *malloc(size_t size) {
    size = alignUp(size);                       // keep offsets a multiple of 8
    size_t need = RNBO_HEADER_BYTES + size;

    char *base = nullptr;
#if RNBO_FAST_POOL_KB > 0
    base = carve(s_fastPool, RNBO_FAST_POOL_BYTES, s_fastOffset, need);   // fast first
#endif
    if (base == nullptr) {
        base = carve(s_rnboPool, RNBO_POOL_BYTES, s_rnboOffset, need);   // then SDRAM
    }
    if (base == nullptr) {
        return nullptr;                         // both pools exhausted
    }

    *reinterpret_cast<size_t *>(base) = size;    // header: usable size
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

    // Recover the old block's usable size from its header (works for blocks
    // from either pool), then copy only the smaller of old/new.
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

// Diagnostics: bytes used in each pool (e.g. to show on screen while testing).
size_t RnboFastPoolUsedBytes() {
#if RNBO_FAST_POOL_KB > 0
    return s_fastOffset;
#else
    return 0;
#endif
}

size_t RnboSdramPoolUsedBytes() {
    return s_rnboOffset;
}
