#!/usr/bin/env python3
"""
make_effect.py — RNBO-on-kshep effect generator
STAGE 1 (v1): PARSE + CLASSIFY + PRINT only. Writes no files.

Usage:
    python make_effect_v1.py <export_folder> [Name]

<export_folder> must contain description.json.
[Name] is optional; if omitted, taken from meta.name in the JSON.

This stage exists to eyeball the param classification against a known-good
export (freezeverb) before any file generation. Confirm the knob/menu
assignment matches what you built by hand.
"""

import sys
import os
import json

RESERVED = {"fsw1", "fsw2", "led1", "led2"}
MAX_KNOBS = 6          # knobs 0..5
MAX_MENU = 10          # params 7..16 -> menu-only (10 slots)


def load_description(export_folder):
    path = os.path.join(export_folder, "description.json")
    if not os.path.isfile(path):
        sys.exit(f"ERROR: no description.json in {export_folder!r}")
    with open(path, "r") as f:
        return json.load(f), path


def classify(params):
    """Split into reserved + non-reserved, sort non-reserved by (order, index).

    Returns (reserved_list, knobs_list, menu_list, warnings).
    reserved_list: params whose name is in RESERVED (order preserved as-is).
    knobs_list:    first MAX_KNOBS non-reserved, sorted.
    menu_list:     next MAX_MENU non-reserved, sorted.
    Anything beyond knobs+menu is flagged as overflow in warnings.
    """
    warnings = []

    reserved = [p for p in params if p["name"] in RESERVED]
    non_reserved = [p for p in params if p["name"] not in RESERVED]

    # Detect duplicate 'order' values among non-reserved -> tie, warn.
    seen_order = {}
    for p in non_reserved:
        seen_order.setdefault(p["order"], []).append(p)
    for order_val, group in seen_order.items():
        if len(group) > 1:
            names = ", ".join(f"{g['name']}(index {g['index']})" for g in group)
            warnings.append(
                f"order={order_val} shared by: {names} "
                f"-> tie broken by index (patch order ambiguous; "
                f"set distinct @order in the patch to fix)."
            )

    # Sort by order, then index as tiebreak.
    non_reserved.sort(key=lambda p: (p["order"], p["index"]))

    knobs = non_reserved[:MAX_KNOBS]
    menu = non_reserved[MAX_KNOBS:MAX_KNOBS + MAX_MENU]
    overflow = non_reserved[MAX_KNOBS + MAX_MENU:]

    if overflow:
        names = ", ".join(p["name"] for p in overflow)
        warnings.append(
            f"{len(overflow)} param(s) beyond knob+menu capacity "
            f"({MAX_KNOBS}+{MAX_MENU}): {names} -> would be DROPPED."
        )

    # Missing reserved names -> the #1 gotcha from the handoff (silent no-op).
    present_reserved = {p["name"] for p in reserved}
    for r in sorted(RESERVED):
        if r not in present_reserved:
            warnings.append(
                f"reserved param {r!r} NOT in export "
                f"-> that hardware path will be inactive."
            )

    return reserved, knobs, menu, warnings


def report(data, name):
    params = data["parameters"]
    meta = data.get("meta", {})

    reserved, knobs, menu, warnings = classify(params)

    print("=" * 60)
    print(f"  make_effect  STAGE 1 (parse only)")
    print("=" * 60)
    print(f"  effect name   : {name}")
    print(f"  classname     : {meta.get('rnboobjname', '?')}")
    print(f"  export base   : {meta.get('name', '?')}  (-> {meta.get('name','?')}.cpp.h)")
    print(f"  rnbo version  : {meta.get('rnboversion', '?')}")
    print(f"  total params  : {data.get('numParameters', len(params))}")
    print(f"  audio I/O     : {data.get('numInputChannels','?')} in / "
          f"{data.get('numOutputChannels','?')} out")
    print(f"  MIDI ports    : {data.get('numMidiInputPorts','?')} in / "
          f"{data.get('numMidiOutputPorts','?')} out")

    print("\n  RESERVED (hardware-bound, not knobs/menu):")
    if reserved:
        for p in reserved:
            role = {
                "fsw1": "footswitch 1 IN",
                "fsw2": "footswitch 2 IN",
                "led1": "LED 1 OUT",
                "led2": "LED 2 OUT",
            }.get(p["name"], "?")
            print(f"    {p['name']:<12} index {p['index']:<2} -> {role}")
    else:
        print("    (none)")

    print("\n  KNOBS (0..5), sorted by order then index:")
    if knobs:
        for slot, p in enumerate(knobs):
            print(f"    knob {slot}  {p['name']:<14} "
                  f"order={p['order']} index={p['index']}  "
                  f"[{p['minimum']}..{p['maximum']}] init={p['initialValue']}")
    else:
        print("    (none)")

    print("\n  MENU-ONLY (knobMapping -1):")
    if menu:
        for p in menu:
            print(f"    {p['name']:<14} order={p['order']} index={p['index']}  "
                  f"[{p['minimum']}..{p['maximum']}] init={p['initialValue']}")
    else:
        print("    (none)")

    print("\n  WARNINGS:")
    if warnings:
        for w in warnings:
            print(f"    ! {w}")
    else:
        print("    (none)")
    print("=" * 60)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    export_folder = sys.argv[1]
    data, _ = load_description(export_folder)
    name = sys.argv[2] if len(sys.argv) > 2 else data.get("meta", {}).get("name", "effect")
    report(data, name)


if __name__ == "__main__":
    main()
