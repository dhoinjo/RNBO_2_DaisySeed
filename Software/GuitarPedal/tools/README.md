# RNBO-on-kshep Effect Generator

A Python tool that packages [RNBO](https://rnbo.cycling74.com/) patches as
effects for the [Electrosmith Daisy](https://www.electro-smith.com/daisy)
guitar-pedal firmware based on
[bkshepherd's DaisySeedProjects](https://github.com/bkshepherd/DaisySeedProjects)
("kshep").

Export a patch from RNBO, run one command, flash the pedal. The tool generates
the C++ wrapper, copies the export, and wires it into the build — no hand-editing.

---

## What it does

Given an RNBO export folder, `make_effect.py`:

1. Reads `description.json` for the effect name and parameters.
2. Generates a per-effect C++ wrapper (`<name>_module.cpp/.h`) that bridges
   RNBO's block processing to kshep's per-sample audio path.
3. Copies the RNBO export into the build's dependency folder, tracking the RNBO
   runtime version.
4. Registers the effect in the `Makefile` and `loaded_effects.h`.

Parameters are mapped automatically: reserved names bind to hardware
(footswitches and LEDs), and the rest map to knobs and menu entries by a
declared order.

---

## Requirements

- The kshep firmware project (this tool lives in its `tools/` folder).
- The ARM toolchain and libDaisy/DaisySP set up per the kshep project's own
  build instructions.
- Python 3 (standard library only — no packages to install).
- RNBO (Max) to author and export patches.

---

## Setup

Place `make_effect.py` in a `tools/` folder at the root of the GuitarPedal
project (alongside `Makefile` and `loaded_effects.h`).

Create an export target folder for RNBO, e.g. `RNBO_exports/`, anywhere that is
**not** in the Makefile's compile path (a plain subfolder is fine).

---

## Authoring a patch

The wrapper recognises four **reserved parameter names**:

| Name          | Direction | Role                                   |
|---------------|-----------|----------------------------------------|
| `fsw1`,`fsw2` | in        | raw footswitch state (0 up / 1 pressed)|
| `led1`,`led2` | out       | LED brightness, set by the patch       |

The patch handles its own footswitch logic (momentary switches; do toggle/tap
inside the patch) and **gates its own output** — the framework always feeds it
audio, so on/off lives in the patch.

All other parameters map to hardware controls, ordered by their RNBO `@order`
attribute: the first six become knobs, the rest become menu entries. Each
parameter's `@value` becomes its on-screen default. Parameter names become
on-screen labels (underscores shown as spaces).

### Recommended RNBO export settings

- Minimal Export: on
- Fixed vector size: none
- Fixed sample rate: 48000
- Enable UI Messages: off

---

## Usage

From the project root:

```bash
# Preview all changes without writing anything:
python3 tools/make_effect.py RNBO_exports .

# Apply:
python3 tools/make_effect.py RNBO_exports . --apply
```

Then build and flash per the kshep project:

```bash
make clean && make
# put the pedal in bootloader mode, then:
make program-dfu
```

Options:

- `--apply` — write changes (default is a dry run).
- `--name <ClassName>` — override the C++ class name (default from the export).
- `--snake <stem>` — override the file stem (default from the export).

The tool is idempotent: re-running skips anything already present.

---

## Removing an effect

The tool only adds effects. To take one out, comment it in **both** files (the
same way stock effects are parked, so it's easy to re-enable):

- `Makefile` — comment its `CPP_SOURCES += Effect-Modules/<name>_module.cpp` line.
- `loaded_effects.h` — comment **both** its `#include` and its `new <Name>Module(),`
  entry in `effectList[]`.

Then `make clean && make` and reflash. All three must be commented together: a
leftover list entry with a commented include fails to compile; a commented entry
with the source still built just wastes space.

To remove it completely, also delete the generated `Effect-Modules/<name>_module.*`
and `dependencies/RNBO/<name>.cpp.h` — though commenting is usually cleaner and
reversible.

---

## Safety

- **Dry run by default.** Nothing is written without `--apply`.
- **Version control is the undo.** The tool writes no backup files; it assumes
  the project is under git. To revert:
  ```bash
  git restore Makefile loaded_effects.h Effect-Modules/ dependencies/RNBO/
  ```
- The tool never modifies a hand-written SDRAM allocator
  (`rnbo_allocator.cpp`).
- On an RNBO runtime version change, the tool prompts before refreshing the
  shared runtime headers.

---

## Resource notes

- **Code size** is reported by `make`; a too-large build fails to link (caught
  before flashing).
- **CPU load** depends on the target and can only be measured at runtime on the
  device — desktop CPU figures do not translate to the Daisy.
- **Large sample buffers** live in SDRAM and are bounded by the allocator's pool
  size; very large or many simultaneous buffers may require enlarging it.

---

## Design notes

The generated wrapper keeps the RNBO include confined to a single translation
unit (behind an opaque pointer) to avoid symbol clashes with kshep's DSP
libraries, defers RNBO construction until hardware init, and discovers
parameters by name so the export's internal ordering doesn't matter.

---

## License

Follows the license of the underlying kshep project. Check individual effect
sources — some third-party DSP is under copyleft licenses and is disabled by
default.
