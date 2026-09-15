#!/usr/bin/env python3
"""
make_effect.py — RNBO-on-kshep effect generator
STAGE 3b (v1): EXPORT COPY + RUNTIME VERSION CHECK.

Copies the RNBO export's <snake>.cpp.h into dependencies/RNBO/ (the build
location), and manages the shared RNBO RUNTIME headers by version:

  - Reads meta.rnboversion from description.json.
  - Compares it to a stamp file at dependencies/RNBO/.rnbo_version.
  - MATCH (or first run with same version): copies only <snake>.cpp.h.
  - MISMATCH: warns, and with --apply asks before refreshing the runtime
    headers from the export's common/ folder. On refresh it rewrites the stamp.
  - NEVER touches rnbo_allocator.cpp — that file is YOURS (hand-written SDRAM
    allocator), not part of RNBO's export.

Usage:
    # show the plan, change nothing (default):
    python make_effect_exportcopy_v1.py <export_folder> <project_root> [snake]

    # actually copy / refresh:
    python make_effect_exportcopy_v1.py <export_folder> <project_root> [snake] --apply

    <export_folder> holds description.json, <snake>.cpp.h, and common/
    <project_root>  the GuitarPedal folder (holds dependencies/RNBO/)
    [snake]         file stem; default from meta.name

Safety:
  - Dry-run by default. --apply required to write.
  - Git is the undo: if a copy goes wrong, `git restore dependencies/RNBO/`.
  - The runtime refresh is gated behind an explicit y/N prompt even under --apply.
"""

import sys
import os
import json
import shutil

ALLOCATOR = "rnbo_allocator.cpp"  # protected: never overwritten
STAMP = ".rnbo_version"


def load_description(export_folder):
    path = os.path.join(export_folder, "description.json")
    if not os.path.isfile(path):
        sys.exit(f"ERROR: no description.json in {export_folder!r}")
    with open(path, "r") as f:
        return json.load(f)


def read_stamp(rnbo_dir):
    p = os.path.join(rnbo_dir, STAMP)
    if os.path.isfile(p):
        with open(p, "r") as f:
            return f.read().strip()
    return None


def main():
    args = [a for a in sys.argv[1:] if a != "--apply"]
    apply = "--apply" in sys.argv
    if len(args) < 2:
        sys.exit(__doc__)

    export_folder = args[0]
    project_root = args[1]
    data = load_description(export_folder)
    meta = data.get("meta", {})
    snake = args[2] if len(args) > 2 else meta.get("name", "effect")
    version = meta.get("rnboversion", "unknown")

    rnbo_dir = os.path.join(project_root, "dependencies", "RNBO")
    export_cpp_h = os.path.join(export_folder, f"{snake}.cpp.h")
    dest_cpp_h = os.path.join(rnbo_dir, f"{snake}.cpp.h")
    export_common = os.path.join(export_folder, "common")
    dest_common = os.path.join(rnbo_dir, "common")

    if not os.path.isfile(export_cpp_h):
        sys.exit(f"ERROR: export not found: {export_cpp_h}")
    if not os.path.isdir(rnbo_dir):
        sys.exit(f"ERROR: dependencies/RNBO not found under {project_root!r} — "
                 f"is this the right project root?")

    installed_version = read_stamp(rnbo_dir)
    mode = "APPLY (writing)" if apply else "DRY RUN (no writes)"

    print("=" * 60)
    print(f"  make_effect  STAGE 3b — export copy + runtime — {mode}")
    print("=" * 60)
    print(f"  effect stem       : {snake}")
    print(f"  export version    : {version}")
    print(f"  installed version : {installed_version if installed_version else '(no stamp yet)'}")
    print()

    # --- the export .cpp.h always gets copied (it's per-effect) ---
    print("  --- export header ---")
    print(f"    copy {snake}.cpp.h  ->  dependencies/RNBO/")
    if os.path.isfile(dest_cpp_h):
        print(f"    (overwrites existing {snake}.cpp.h — git is your undo)")

    # --- runtime version handling ---
    print("  --- RNBO runtime ---")
    version_mismatch = (installed_version is not None) and (installed_version != version)
    first_runtime = installed_version is None

    refresh_runtime = False
    if first_runtime:
        if os.path.isdir(dest_common):
            print(f"    runtime present but no stamp — will WRITE stamp ({version}),")
            print(f"    leaving existing runtime headers as-is.")
        else:
            print(f"    no runtime installed — will copy common/ and write stamp ({version}).")
            refresh_runtime = True
    elif version_mismatch:
        print(f"    ! VERSION MISMATCH: export {version} vs installed {installed_version}.")
        print(f"    The per-effect header is built against {version}; a stale runtime")
        print(f"    can cause silent breakage. Recommend refreshing the runtime.")
        # refresh decided interactively below (only under --apply)
    else:
        print(f"    runtime up to date ({version}) — copy export header only.")

    print(f"    NOTE: {ALLOCATOR} is never touched (your file).")
    print("=" * 60)

    if not apply:
        print("  DRY RUN — nothing written. Re-run with --apply to copy.")
        if version_mismatch:
            print("  On --apply you'll be asked before the runtime is refreshed.")
        return

    # ---- APPLY ----
    # 1) always copy the per-effect export header
    shutil.copy2(export_cpp_h, dest_cpp_h)
    print(f"  copied {snake}.cpp.h -> dependencies/RNBO/")

    # 2) runtime
    def copy_runtime():
        if not os.path.isdir(export_common):
            print(f"  WARNING: export has no common/ folder — cannot refresh runtime.")
            return False
        # copy headers but never the allocator (allocator isn't in common/ anyway,
        # but guard regardless).
        os.makedirs(dest_common, exist_ok=True)
        for root, _dirs, files in os.walk(export_common):
            rel = os.path.relpath(root, export_common)
            target_root = os.path.join(dest_common, rel) if rel != "." else dest_common
            os.makedirs(target_root, exist_ok=True)
            for fn in files:
                if fn == ALLOCATOR:
                    continue
                shutil.copy2(os.path.join(root, fn), os.path.join(target_root, fn))
        return True

    def write_stamp():
        with open(os.path.join(rnbo_dir, STAMP), "w") as f:
            f.write(version + "\n")

    if first_runtime:
        if refresh_runtime:
            if copy_runtime():
                write_stamp()
                print(f"  installed runtime common/ and wrote stamp ({version}).")
        else:
            write_stamp()
            print(f"  wrote stamp ({version}); left existing runtime as-is.")
    elif version_mismatch:
        resp = input(f"  Refresh runtime {installed_version} -> {version}? [y/N] ").strip().lower()
        if resp == "y":
            if copy_runtime():
                write_stamp()
                print(f"  refreshed runtime and updated stamp to {version}.")
        else:
            print("  left runtime unchanged. WARNING: export header may not match "
                  "the installed runtime.")
    else:
        print("  runtime already current — no runtime change.")

    print("  Undo if needed:  git restore dependencies/RNBO/")


if __name__ == "__main__":
    main()
