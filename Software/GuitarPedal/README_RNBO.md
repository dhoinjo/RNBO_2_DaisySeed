<!-- README_rnbo_v1  (paste into the repo README, or save as README_RNBO.md) -->

# RNBO effects on the Daisy pedal

This fork adds a workflow that turns an **RNBO C++ export** into a pedal effect
with one command, and lets each pedal carry its own selection of effects.

## What you need

- The GuitarPedal toolchain from the main README (arm-none-eabi-gcc 10, libDaisy built).
- The Daisy **SRAM bootloader** (`make program-boot` once); effects run from SRAM.
- Max with RNBO (tested with RNBO 1.4.5).

## Exporting from RNBO

C++ source export, sample rate 48000, **one folder per effect**:
`RNBO_exports/<name>` (e.g. `RNBO_exports/freezeverb`). Each export writes its own
`description.json`, so effects must not share a folder.

Conventions in the patch:

| In RNBO | On the pedal |
|---|---|
| `param` with `@order` | first 6 by order → knobs 1–6, the rest → menu (max 24) |
| real min/max | knob, MIDI CC and menu cover exactly that range |
| `@steps N` (N ≥ 3) | stepped: knob/MIDI/menu click through N values, menu shows them |
| 0–1 with `@steps 2` | checkbox in the menu |
| params named `fsw1`, `fsw2` | footswitch states (0/1) pushed into the patch |
| params named `led1`, `led2` | the patch drives the two LEDs (use 0/1 or blinks) |

MIDI CCs are assigned automatically: 20–31, then 102–105.

## Everyday use

```
# 1. export (or re-export) and put the effect on a pedal
python3 tools/make_effect.py RNBO_exports/freezeverb . --apply --pedal purple

# 2. build and flash that pedal
make PEDAL=purple
make PEDAL=purple program-dfu
```

- Without `--apply` the generator only shows what it would do.
- Pedals have names (`purple, yellow, green, orange, blue`; edit `PEDALS` in the
  script). Each has a list in `pedals/<name>.mk` (`EFFECTS = a b c`); the generator
  maintains it, or edit the line by hand once an effect has been generated.
- `--off purple` removes an effect from a pedal; `--pedals` lists all pedals.
- Each pedal builds in its own folder (`build_<name>`); only what changed is recompiled.
  `make clean PEDAL=purple` cleans one pedal, `make clean-all` all of them.

## Memory and CPU

- **Code** of all effects on a pedal shares the 480 KB internal SRAM
  (framework ≈ 176 KB). The build prints the usage.
- **RNBO buffers** (`buffer~`, `data`, delay lines) go to a 48 MB pool in SDRAM
  (`dependencies/RNBO/rnbo_allocator.cpp`).
- `--fast-pool 128 --pedal <name>` gives a pedal a fast pool in internal SRAM for
  RNBO buffers that are read every sample. It costs code room on that pedal.
- **Heap** (effect objects, `codebox~` arrays): the pedal shows its use at boot
  ("heap 38%") and stops with **HEAP FULL** instead of crashing if it runs out.
  Large arrays belong in `buffer~`/`data`, not in `codebox~` arrays.
- The screen shows the peak CPU load per second; stay well below 1.0.
  `peek`/`poke` on buffers is expensive in exported code; for per-sample state,
  `codebox~` `FixedSampleArray` is much cheaper.

## Safety nets

- `make` stops if a pedal lists an effect that isn't wired in yet.
- A pedal with an empty effect list does not compile (instead of freezing).
- The generator refuses to write a broken `loaded_effects.h`.

## Credits

Built on Keith Shepherd's DaisySeedProjects (MIT). The freezeverb example is a
Dattorro plate reverb; see the freeze-verb credits in the main README.
