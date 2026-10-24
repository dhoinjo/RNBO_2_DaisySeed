// heap_guard_v1.cpp   (install as: Util/heap_guard.cpp)
// ---------------------------------------------------------------------------
// See heap_guard.h. Util/*.cpp is compiled automatically by the Makefile, so
// no Makefile change is needed.
//
// This _sbrk replaces the one from libnosys (newlib), which has no limit.
// malloc/new call it whenever they need more heap.
// ---------------------------------------------------------------------------
#include "heap_guard.h"
#include <errno.h>
#include <stdint.h>

// Start of the heap, provided by libDaisy's linker script (bottom of RAM_D2).
extern "C" char end;

// End of RAM_D2 = end of the D2-domain SRAM (SRAM1+2+3).
// libDaisy STM32H750IB_sram.lds: RAM_D2 ORIGIN 0x30008000, LENGTH 256K.
static const uintptr_t kHeapLimit = 0x30008000u + 256u * 1024u;

static char *s_heapTop = nullptr;
static bool s_overflowed = false;
static void (*s_onFull)(size_t) = nullptr;

extern "C" void *_sbrk(ptrdiff_t incr) {
    if (s_heapTop == nullptr) {
        s_heapTop = &end;
    }
    char *prev = s_heapTop;
    if ((uintptr_t)(s_heapTop + incr) > kHeapLimit) {
        if (!s_overflowed) {
            s_overflowed = true;
            if (s_onFull != nullptr) {
                s_onFull((size_t)incr);
            }
        }
        errno = ENOMEM;
        return (void *)-1; // malloc returns NULL
    }
    s_heapTop += incr;
    return prev;
}

size_t HeapGuardUsedBytes() {
    char *top = (s_heapTop != nullptr) ? s_heapTop : &end;
    return (size_t)(top - &end);
}

size_t HeapGuardCapacityBytes() {
    return (size_t)(kHeapLimit - (uintptr_t)&end);
}

bool HeapGuardOverflowed() {
    return s_overflowed;
}

void HeapGuardSetOnFull(void (*handler)(size_t requested)) {
    s_onFull = handler;
}
