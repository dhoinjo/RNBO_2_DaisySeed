/*
    rnbo_fast_pool_v1.lds   (install as: rnbo_fast_pool.lds, next to the Makefile)

    Supplemental linker fragment for the RNBO fast pool (rnbo_allocator v3).

    Adds a `.sram_axi_bss` output section in the main internal SRAM (AXI-SRAM,
    0x24000000), placed right after the program image. It is NOLOAD, so it does
    not make the firmware file bigger or the boot slower.

    Why here and not RAM_D2: on this target the heap starts at `end` =
    0x30008000, i.e. at the bottom of RAM_D2, and grows upward with no limit
    check. A static pool in RAM_D2 would shrink that heap. AXI-SRAM's free part
    is otherwise unused at runtime.

    Same mechanism as nam_a2_sections.lds: an INSERT-only script passed with -T
    BEFORE libDaisy's main script. The "memory region `SRAM' not declared" /
    "redeclaration" warnings ld prints for it are harmless (the NAM fragment
    gets the same ones for RAM_D2).

    INSERT AFTER .data: in the SRAM region this lands after .data's load image,
    so nothing loadable comes after the pool (no gap in the .bin).

    Size is decided in C (rnbo_allocator.cpp, FAST_POOL_KB per pedal); with no
    pool the section is simply empty.
*/

SECTIONS
{
    .sram_axi_bss (NOLOAD) :
    {
        . = ALIGN(32);
        _srnbo_fast_pool = .;
        *(.sram_axi_bss)
        *(.sram_axi_bss*)
        . = ALIGN(32);
        _ernbo_fast_pool = .;
    } > SRAM
} INSERT AFTER .data;
