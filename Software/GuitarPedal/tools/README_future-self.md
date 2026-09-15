# make_effect.py — RNBO → kshep effect generator

**For future-me.** This turns an RNBO export into a flashable kshep effect with
one command, so I never hand-edit five files per effect again.

---

## The one-command workflow

1. In RNBO, author the patch (see "Authoring rules" below), export to
   `RNBO_exports/` inside the GuitarPedal folder.
2. From the GuitarPedal folder:
   ```
   python3 tools/make_effect.py RNBO_exports .            # dry run — shows the plan
   python3 tools/make_effect.py RNBO_exports . --apply    # actually build
   ```
3. Build and flash:
   ```
   make clean && make
   # tap RESET on the pedal, then:
   make program-dfu
   ```

Dry run writes nothing. `--apply` is required to touch any file. Re-running is
safe (idempotent — already-present lines are skipped).

**Undo everything** the tool did:
```
git restore Makefile loaded_effects.h Effect-Modules/ dependencies/RNBO/
```

---

## What the tool does (four stages)

1. **Parse** `description.json` — effect name (from `meta`), param names, order,
   defaults.
2. **Module files** — writes `Effect-Modules/<snake>_module.cpp` and `.h`.
3. **Export copy** — copies `<snake>.cpp.h` into `dependencies/RNBO/`, plus an
   RNBO-runtime version check (see below).
4. **Wiring** — edits `Makefile` (`CPP_SOURCES`) and `loaded_effects.h`
   (`#include` + `effectList[]` entry).

---

## Authoring rules in RNBO (what the patch MUST do)

- **Reserved param names** drive hardware, discovered by name (index doesn't
  matter):
  - `fsw1`, `fsw2` — footswitch state comes IN here (0.0 up / 1.0 pressed).
    Do edge/toggle/tap logic inside the patch; switches are momentary.
  - `led1`, `led2` — LED brightness goes OUT here. Patch decides what they mean.
  - The patch must **gate its own output** (on/off/silence lives in the patch;
    the framework always feeds it samples). Switch 1 is raw into `fsw1`, NOT a
    bypass toggle.
- **Knob order** = the param `@order` attribute (NOT patch index). First 6
  non-reserved params (by `@order`) → knobs 0–5; next 10 → menu-only.
- **Defaults** = each param's `@value` in the patch → exported as `initialValue`
  → becomes the on-screen default. Set `@value` in the patch to control defaults.
- **Screen labels** = the param name with underscores → spaces, case untouched
  (`reverb_time` → "reverb time"). Name params how you want them read.
- Keep labels short (~12 chars) or they truncate on screen — the tool warns.

## RNBO export settings that work

- Minimal Export: ON
- Fixed vector size: **None**
- Fixed sample rate: 48000
- Classname: the effect name (becomes the C++ class)
- Enable UI Messages: OFF (wrapper polls via get/setParameterValue)
- Export writes `<classname>.cpp.h` natively — no hand-renaming.

---

## Limits & gotchas (READ before loading heavy effects)

These are three SEPARATE limits — don't confuse them.

### Storage (SRAM / SDRAM) — checked at build time
`make` prints a memory table. The numbers to watch:
- **SRAM (480 KB)** — code + data for ALL compiled effects, loaded or not.
  An unloaded effect still costs SRAM. Was ~70% with freezeverb. If this nears
  ~95%, stop adding effects.
- **SDRAM (64 MB)** — delay/reverb/sampler buffers. Was ~44% with CloudSeed.
  This is the one samplers eat (see below).
If an effect is too big, the LINK FAILS with "overflowed region" — caught on the
desk, no flash needed. So `make` IS the storage check.

### CPU — runtime only, can't be checked at build
- Only the ACTIVE effect uses CPU. Unloaded effects cost 0 CPU.
- **Desktop CPU % does NOT predict Daisy CPU %.** The Daisy is a 480 MHz M7 with
  no OS; an FFT effect at 8% on the Mac could be far higher on the Daisy. The
  ratio isn't fixed.
- The only reliable test: flash it and read the on-pedal audio-callback load
  (libDaisy `CpuLoadMeter` / DaisySP). Glitches/dropouts = it can't keep up.
- Cheap proxy before flashing: how often/big the FFT runs per 48-sample block.
  Big FFT every block = heavy; less often / overlapped = lighter.
- **TODO (future-me):** wire the CPU-load meter to the screen so heavy effects
  can be measured, not guessed. (scope_module already has diagnostic plumbing —
  start there.)

### Buffers (live samplers, big data objects) — SDRAM + allocator
- A 32-second STEREO buffer @ 48 kHz = **~12.3 MB** each. These go in SDRAM.
- **The allocator pool is the hard limit.** `dependencies/RNBO/rnbo_allocator.cpp`
  is a bump allocator currently capped at **8 MB**. A single 12 MB sampler
  EXCEEDS that → alloc fails → crash at Init.
  - Raising the pool is a one-line constant change (`8 * 1024 * 1024` → bigger).
    It sits over `DSY_SDRAM_BSS`, so there's room to grow toward the 64 MB SDRAM
    total (minus whatever else lives in SDRAM — CloudSeed etc.).
  - **BUT the bump allocator's `free` is a no-op** — memory is never reclaimed
    during a run; the pointer only moves forward. With several big samplers,
    switching between effects could exhaust the pool even though only one is
    active. At that point a bigger pool isn't enough — need a real allocator that
    reclaims on effect-switch. Investigate when samplers become the focus.
- `rnbo_allocator.cpp` is MINE (hand-written). The generator NEVER touches it.

### The four original C++ gotchas (still true)
1. **RNBO vs `q` name clash**: RNBO include lives ONLY in the module `.cpp`,
   behind an opaque `Impl*` (PIMPL). Header forward-declares the RNBO type.
   Never include RNBO in a header.
2. **Exceptions off**: kshep builds `-fno-exceptions`; needs
   `-DRNBO_NOTHROW -DRNBO_NOSTL`.
3. **Boot crash from allocation**: custom SDRAM allocator, enabled by
   `-DRNBO_USECUSTOMALLOCATOR`. RNBO setup deferred to `Init()`, NOT the ctor.
4. **`setParameterValue` needs 3 args**: `(index, value, RNBO::TimeNow)`.

Plus: **always confirm the export is in the BUILD folder** —
`grep -c "fsw1" dependencies/RNBO/<name>.cpp.h` should be nonzero. A stale
export in `dependencies/RNBO/` while the real one sits unused = silent no-op
(knobs work, fsw/led don't). This cost a whole debugging session once.

---

## RNBO runtime version handling

- The tool stamps `dependencies/RNBO/.rnbo_version` with the export's
  `meta.rnboversion`.
- Same version on next run → copies only the per-effect `<name>.cpp.h`.
- **Version bump** (e.g. after updating RNBO in Max) → the tool warns and asks
  before refreshing the runtime headers under `dependencies/RNBO/common/`.
  It never overwrites `rnbo_allocator.cpp`.
- Why this matters: a new per-effect header against a stale runtime = silent
  breakage. Keep them in lockstep.

*(Currently on RNBO 1.4.4. Waiting on a fix for an FFT bug reported to Cycling;
that fix will land in a later RNBO version — expect a runtime refresh then.)*

---

## File locations

- `tools/make_effect.py` — the tool (this is the one to run).
- `tools/original scripts/` — the four separate stage-scripts it was built from,
  kept as a fallback / for debugging one stage in isolation.
- `Effect-Modules/<name>_module.cpp/.h` — generated per-effect wrappers.
- `dependencies/RNBO/<name>.cpp.h` — the RNBO export (build location).
- `dependencies/RNBO/common/` — RNBO runtime headers (refreshed on version bump).
- `dependencies/RNBO/rnbo_allocator.cpp` — MY allocator (never auto-touched).
- `RNBO_exports/` — where I export from RNBO (staging; not in the build path).

## L/R switch swap (board quirk, software fix — permanent)
The Terrarium board's silkscreen has fsw1/led1 on the physical RIGHT. The fix
lives in the switch loop in `guitar_pedal.cpp` (physical index reversed, LED
calls swapped) so physical LEFT = fsw1/led1 for all effects. Not rewiring.

## Before publishing (year-end open-source release)

The stack is MIT throughout — publishable for students freely — but three
attribution/license items to settle before release:

1. **Credit Émilie Gillet (Mutable Instruments, MIT)** for the freeze-verb.
   It's derived from her MIT-licensed code ("Copyright 2014 Émilie Gillet").
   MIT's one condition is keeping the copyright notice — put it in the effect's
   source comments / patch / a credits section.
2. **Check the RNBO runtime / export license** (Cycling '74). The exported code
   ships the RNBO runtime (`dependencies/RNBO/common/`) in every binary. C74's
   export terms generally permit distributing exported code, but read them once
   before publishing.
3. **Keep Keith Shepherd's MIT LICENSE** (kshep base) and the Dattorro GPL-3.0
   corner commented-out/excluded, so the published build stays cleanly MIT.

Project license summary: kshep = MIT (Keith Shepherd), freeze-verb = MIT
(É. Gillet), the generator + READMEs = mine. Only non-MIT: Dattorro (GPL,
disabled) and the RNBO runtime (C74's own terms).

## Removing / disabling an effect

The tool ADDS effects; it doesn't remove them. To take one out, comment it in
BOTH files (comment, don't delete — that's how every stock kshep effect is
parked, easy to re-enable). Match the existing `//` style.

To **disable** (keep the files, drop it from the build):

1. `Makefile` — comment its source line:
   ```
   //CPP_SOURCES += Effect-Modules/<name>_module.cpp
   ```
2. `loaded_effects.h` — comment BOTH its include and its effectList entry:
   ```
   //#include "Effect-Modules/<name>_module.h"
   ...
   //new <Name>Module(),
   ```
3. `make clean && make`, reflash.

Miss any one of the three and you get a confusing result: a leftover
`effectList[]` entry with a commented include = compile error (unknown type); a
commented entry with the source still compiled = wasted SRAM for an effect you
can't select. Comment all three.

To **delete entirely**, also remove `Effect-Modules/<name>_module.cpp/.h` and
`dependencies/RNBO/<name>.cpp.h`. Usually not worth it — commenting is cleaner
and reversible. (git restores anything deleted by mistake.)

Disabling an effect frees its **SRAM** (it's no longer compiled in) but has no
effect on CPU (only the active effect ever used CPU anyway).

## Regenerating freezeverb (the known-good test)
freezeverb is the reference effect. To prove the tool still works after any
change: regenerate it, `make`, confirm it builds and the screen shows
"reverb time / amount / lp" on knobs 0/1/2.
