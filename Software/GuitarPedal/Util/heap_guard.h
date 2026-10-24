// heap_guard_v1.h   (install as: Util/heap_guard.h)
// ---------------------------------------------------------------------------
// Heap ceiling + usage readout for the GuitarPedal firmware.
//
// On this target (BOOT_SRAM) the heap starts at the linker symbol `end`
// (0x30008000, bottom of RAM_D2) and grows upward. Without a guard nothing
// stops it at the end of RAM_D2, so running out corrupts memory or crashes
// silently. heap_guard.cpp replaces the C library's _sbrk with one that
// refuses to grow past the end of RAM_D2 and calls a handler you register,
// so the pedal can show "HEAP FULL" instead.
//
// Everything that uses new/malloc goes through here: the effect objects (and
// the inline arrays of RNBO codebox~ FixedSampleArrays inside them), the menus.
// NOT the RNBO pools (those are static: SDRAM pool / fast pool).
// ---------------------------------------------------------------------------
#pragma once
#include <stddef.h>

// Bytes of heap in use / total heap available.
size_t HeapGuardUsedBytes();
size_t HeapGuardCapacityBytes();

// True once any allocation has been refused.
bool HeapGuardOverflowed();

// Called (once) when an allocation is refused. `requested` = bytes asked for.
void HeapGuardSetOnFull(void (*handler)(size_t requested));
