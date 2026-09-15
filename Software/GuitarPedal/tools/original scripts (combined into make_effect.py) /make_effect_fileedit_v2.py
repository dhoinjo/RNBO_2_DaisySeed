#!/usr/bin/env python3
"""
make_effect.py — RNBO-on-kshep effect generator
STAGE 3a (v2): FILE-EDITOR. Dry-run by default; --apply writes.

Wires a new effect into the two project files:
  - Makefile        : adds  CPP_SOURCES += Effect-Modules/<snake>_module.cpp
  - loaded_effects.h: adds  #include "Effect-Modules/<snake>_module.h"
                      and   new <Name>Module(),   in effectList[]

Usage:
    # show the plan, change nothing (default):
    python make_effect_fileedit_v2.py <project_root> <Name> [snake_stem]

    # actually write the changes:
    python make_effect_fileedit_v2.py <project_root> <Name> [snake_stem] --apply

Safety:
  - Idempotent: if a line is already present, it is skipped. Re-running is safe.
  - No backups are written by this script BY DESIGN — the project is under git.
    If a run goes wrong:  git restore Makefile loaded_effects.h
  - --apply refuses to run if it can't find a sane insertion anchor.

Example:
    python make_effect_fileedit_v2.py . PaulStretch paulstretch --apply
"""

import sys
import os
import re


# ---------------------------------------------------------------------------
# name helpers
# ---------------------------------------------------------------------------
def pascal_to_snake(name):
    return re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()


def resolve_names(name, snake_override):
    module_class = f"{name}Module"
    snake = snake_override if snake_override else pascal_to_snake(name)
    return name, module_class, snake


def read_lines(path):
    if not os.path.isfile(path):
        sys.exit(f"ERROR: not found: {path}")
    with open(path, "r") as f:
        return f.readlines()


def write_lines(path, lines):
    with open(path, "w") as f:
        f.writelines(lines)


# ---------------------------------------------------------------------------
# An Insertion describes one line to add after a given 0-based line index.
# ---------------------------------------------------------------------------
class Insertion:
    def __init__(self, after_idx, text, note):
        self.after_idx = after_idx  # insert AFTER this 0-based index
        self.text = text            # full line text (no trailing newline)
        self.note = note            # human description for the plan printout


# ---------------------------------------------------------------------------
# Makefile planning
# ---------------------------------------------------------------------------
def plan_makefile(lines, snake):
    add_line = f"CPP_SOURCES += Effect-Modules/{snake}_module.cpp"
    alloc_line = "CPP_SOURCES += dependencies/RNBO/rnbo_allocator.cpp"

    present_active = any(l.strip() == add_line for l in lines)
    alloc_present = any(l.strip() == alloc_line for l in lines)

    # anchor: after the last active "CPP_SOURCES += Effect-Modules/..._module.cpp"
    anchor_idx = None
    for i, l in enumerate(lines):
        s = l.strip()
        if s.startswith("CPP_SOURCES += Effect-Modules/") and s.endswith("_module.cpp"):
            anchor_idx = i

    insertions = []
    skipped = []

    if present_active:
        skipped.append(f"Makefile: module source already wired ({add_line})")
    elif anchor_idx is None:
        sys.exit("ERROR: Makefile — no CPP_SOURCES Effect-Modules anchor found; "
                 "aborting rather than guess.")
    else:
        insertions.append(Insertion(anchor_idx, add_line,
                                    f"Makefile: add module source (after line {anchor_idx+1})"))

    if alloc_present:
        skipped.append("Makefile: allocator already wired")
    else:
        # place allocator right after the (new or existing) last module line.
        after = anchor_idx if anchor_idx is not None else len(lines) - 1
        insertions.append(Insertion(after, alloc_line,
                                    "Makefile: add RNBO allocator (first RNBO effect)"))

    return insertions, skipped


# ---------------------------------------------------------------------------
# loaded_effects.h planning
# ---------------------------------------------------------------------------
def plan_loaded_effects(lines, snake, module_class):
    include_line = f'#include "Effect-Modules/{snake}_module.h"'
    list_entry_text = f"        new {module_class}(),"  # 8-space indent matches file

    include_present = any(l.strip() == include_line for l in lines)
    entry_re = re.compile(r"^\s*new\s+" + re.escape(module_class) + r"\s*\(\s*\)\s*,?\s*$")
    entry_present = any(entry_re.match(l) for l in lines)

    # include anchor: after the last active Effect-Modules include
    inc_anchor = None
    for i, l in enumerate(lines):
        s = l.strip()
        if s.startswith('#include "Effect-Modules/') and s.endswith('_module.h"'):
            inc_anchor = i

    # effectList anchor: the last active "new ...Module()," entry
    list_anchor = None
    for i, l in enumerate(lines):
        if re.match(r"^\s*new\s+\w+Module\s*\(\s*\)\s*,\s*$", l):
            list_anchor = i

    insertions = []
    skipped = []

    if include_present:
        skipped.append(f"loaded_effects.h: include already present")
    elif inc_anchor is None:
        sys.exit("ERROR: loaded_effects.h — no Effect-Modules include anchor found; aborting.")
    else:
        insertions.append(Insertion(inc_anchor, include_line,
                                    f"loaded_effects.h: add include (after line {inc_anchor+1})"))

    if entry_present:
        skipped.append("loaded_effects.h: effectList entry already present")
    elif list_anchor is None:
        sys.exit("ERROR: loaded_effects.h — no effectList entry anchor found; aborting.")
    else:
        insertions.append(Insertion(list_anchor, list_entry_text,
                                    f"loaded_effects.h: add effectList entry (after line {list_anchor+1})"))

    return insertions, skipped


# ---------------------------------------------------------------------------
# apply: insert lines bottom-up so earlier indices stay valid
# ---------------------------------------------------------------------------
def apply_insertions(lines, insertions):
    out = list(lines)
    for ins in sorted(insertions, key=lambda x: x.after_idx, reverse=True):
        text = ins.text if ins.text.endswith("\n") else ins.text + "\n"
        out.insert(ins.after_idx + 1, text)
    return out


def print_plan(title, insertions, skipped):
    print(f"  --- {title} ---")
    for s in skipped:
        print(f"    (skip) {s}")
    for ins in insertions:
        print(f"    + {ins.note}")
        print(f"        {ins.text.strip()}")
    if not insertions and not skipped:
        print("    (nothing to do)")


def main():
    args = [a for a in sys.argv[1:] if a != "--apply"]
    apply = "--apply" in sys.argv

    if len(args) < 2:
        sys.exit(__doc__)

    root = args[0]
    name = args[1]
    snake_override = args[2] if len(args) > 2 else None

    class_name, module_class, snake = resolve_names(name, snake_override)

    mk_path = os.path.join(root, "Makefile")
    le_path = os.path.join(root, "loaded_effects.h")

    mk_lines = read_lines(mk_path)
    le_lines = read_lines(le_path)

    mk_ins, mk_skip = plan_makefile(mk_lines, snake)
    le_ins, le_skip = plan_loaded_effects(le_lines, snake, module_class)

    mode = "APPLY (writing)" if apply else "DRY RUN (no writes)"
    print("=" * 60)
    print(f"  make_effect  STAGE 3a — file-editor — {mode}")
    print("=" * 60)
    print(f"  class name    : {class_name}")
    print(f"  module class  : {module_class}")
    print(f"  file stem     : {snake}")
    print()
    print_plan("Makefile", mk_ins, mk_skip)
    print()
    print_plan("loaded_effects.h", le_ins, le_skip)
    print("=" * 60)

    if not apply:
        print("  DRY RUN — nothing written. Re-run with --apply to write.")
        return

    total = len(mk_ins) + len(le_ins)
    if total == 0:
        print("  Nothing to write (all already present).")
        return

    if mk_ins:
        write_lines(mk_path, apply_insertions(mk_lines, mk_ins))
    if le_ins:
        write_lines(le_path, apply_insertions(le_lines, le_ins))

    print(f"  WROTE {total} line(s).")
    print("  Undo if needed:  git restore Makefile loaded_effects.h")


if __name__ == "__main__":
    main()
