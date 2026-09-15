#!/usr/bin/env python3
"""
make_effect.py — RNBO-on-kshep effect generator
STAGE 3a (v1): FILE-EDITOR, DRY-RUN ONLY.

Reads the project's Makefile and loaded_effects.h and prints EXACTLY the lines
it would add to wire in a new effect. Writes nothing. This is the risky part
(a bad edit breaks the build), so we prove it in isolation first.

Usage:
    python make_effect_fileedit_v1.py <project_root> <Name>

    <project_root>  the GuitarPedal folder (holds Makefile + loaded_effects.h)
    <Name>          the effect's PascalCase class stem, e.g. FreezeVerb
                    (module class -> <Name>Module; files -> <name>_module.*)

Example:
    python make_effect_fileedit_v1.py . FreezeVerb

It is IDEMPOTENT-AWARE: if the effect already appears wired in (as FreezeVerb
already is in your project), it says so and proposes no duplicate lines.
"""

import sys
import os
import re


def pascal_to_snake(name):
    """FreezeVerb -> freezeverb ; PitchShifter -> pitch_shifter.

    kshep's existing files use snake_case with no separator for single concepts
    (freezeverb) but underscores for multi-word (pitch_shifter, tape_delay).
    We can't perfectly infer which the user wants from PascalCase alone, so we
    insert an underscore at each internal capital. For single-word names this
    gives the right answer (FreezeVerb has an internal capital -> freeze_verb,
    which is WRONG for this project's 'freezeverb').

    -> So we DO NOT guess. The snake stem is taken from meta.name in the JSON in
    the real generator. Here, for the standalone file-editor, we accept the snake
    stem explicitly if the PascalCase split would be ambiguous. See resolve_names.
    """
    s = re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()
    return s


def resolve_names(name, snake_override):
    """Return (className, moduleClass, snakeStem).

    className   : the RNBO/base class stem as given (e.g. FreezeVerb)
    moduleClass : <className>Module (e.g. FreezeVerbModule) — used in effectList
    snakeStem   : file stem (e.g. freezeverb) — used in filenames + includes
    """
    module_class = f"{name}Module"
    if snake_override:
        snake = snake_override
    else:
        snake = pascal_to_snake(name)
    return name, module_class, snake


def read_lines(path):
    if not os.path.isfile(path):
        sys.exit(f"ERROR: not found: {path}")
    with open(path, "r") as f:
        return f.readlines()


# ---------------------------------------------------------------------------
# Makefile
# ---------------------------------------------------------------------------
def plan_makefile(root, snake):
    path = os.path.join(root, "Makefile")
    lines = read_lines(path)

    module_src = f"Effect-Modules/{snake}_module.cpp"
    add_line = f"CPP_SOURCES += {module_src}"

    # Already present? (active OR commented — either way, don't duplicate.)
    present_active = any(l.strip() == add_line for l in lines)
    present_comment = any(l.strip() == f"#{add_line}" or l.strip() == f"//{add_line}"
                          for l in lines)

    # Allocator already wired? (first-time-only line)
    alloc_line = "CPP_SOURCES += dependencies/RNBO/rnbo_allocator.cpp"
    alloc_present = any(l.strip() == alloc_line for l in lines)

    # Find a good insertion anchor: after the last active
    # "CPP_SOURCES += Effect-Modules/..._module.cpp" line.
    anchor_idx = None
    for i, l in enumerate(lines):
        if l.strip().startswith("CPP_SOURCES += Effect-Modules/") and \
           l.strip().endswith("_module.cpp"):
            anchor_idx = i

    print("  --- Makefile ---")
    if present_active:
        print(f"    already wired (active): {add_line}")
    elif present_comment:
        print(f"    present but COMMENTED OUT — would uncomment:")
        print(f"      {add_line}")
    else:
        where = f"after line {anchor_idx + 1}" if anchor_idx is not None else "end of CPP_SOURCES block"
        print(f"    would INSERT ({where}):")
        print(f"      {add_line}")

    if not alloc_present:
        print(f"    would INSERT (first RNBO effect — allocator):")
        print(f"      {alloc_line}")
    else:
        print(f"    allocator already wired — no change")


# ---------------------------------------------------------------------------
# loaded_effects.h
# ---------------------------------------------------------------------------
def plan_loaded_effects(root, snake, module_class):
    path = os.path.join(root, "loaded_effects.h")
    lines = read_lines(path)

    include_line = f'#include "Effect-Modules/{snake}_module.h"'
    list_entry = f"new {module_class}(),"

    include_present = any(l.strip() == include_line for l in lines)
    # effectList entry: match "new <Module>()" allowing trailing comma/spaces.
    entry_re = re.compile(r"^\s*new\s+" + re.escape(module_class) + r"\s*\(\s*\)\s*,?\s*$")
    entry_present = any(entry_re.match(l) for l in lines)

    # Include anchor: after the last active Effect-Modules include.
    inc_anchor = None
    for i, l in enumerate(lines):
        if l.strip().startswith('#include "Effect-Modules/') and \
           l.strip().endswith('_module.h"'):
            inc_anchor = i

    # effectList anchor: the last active "new ...Module()," inside the array.
    list_anchor = None
    for i, l in enumerate(lines):
        if re.match(r"^\s*new\s+\w+Module\s*\(\s*\)\s*,\s*$", l):
            list_anchor = i

    print("  --- loaded_effects.h ---")
    if include_present:
        print(f"    include already present: {include_line}")
    else:
        where = f"after line {inc_anchor + 1}" if inc_anchor is not None else "with the other includes"
        print(f"    would INSERT include ({where}):")
        print(f"      {include_line}")

    if entry_present:
        print(f"    effectList entry already present: {list_entry}")
    else:
        where = f"after line {list_anchor + 1}" if list_anchor is not None else "at end of effectList[]"
        print(f"    would INSERT effectList entry ({where}):")
        print(f"      {list_entry}")


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    root = sys.argv[1]
    name = sys.argv[2]
    snake_override = sys.argv[3] if len(sys.argv) > 3 else None

    class_name, module_class, snake = resolve_names(name, snake_override)

    print("=" * 60)
    print("  make_effect  STAGE 3a — file-editor DRY RUN (no writes)")
    print("=" * 60)
    print(f"  class name    : {class_name}")
    print(f"  module class  : {module_class}")
    print(f"  file stem     : {snake}   (-> {snake}_module.cpp/.h, {snake}.cpp.h)")
    if snake_override is None:
        print(f"  NOTE: file stem auto-derived from class name. If your project uses")
        print(f"        a different stem (e.g. 'freezeverb' not 'freeze_verb'), pass")
        print(f"        it as a 3rd argument. The real generator reads it from")
        print(f"        description.json's meta.name and won't need this.")
    print()

    plan_makefile(root, snake)
    print()
    plan_loaded_effects(root, snake, module_class)
    print("=" * 60)


if __name__ == "__main__":
    main()
