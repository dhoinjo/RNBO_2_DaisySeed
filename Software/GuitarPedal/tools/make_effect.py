#!/usr/bin/env python3
"""
make_effect.py — RNBO-on-kshep effect generator (consolidated)

ONE command turns an RNBO export into a flashable effect. Runs four stages:
  1. PARSE      description.json -> effect name + classified params
  2. MODULE     write Effect-Modules/<snake>_module.cpp/.h
  3. EXPORT     copy <snake>.cpp.h into dependencies/RNBO/ (+ runtime version check)
  4. WIRE       edit Makefile (CPP_SOURCES) and loaded_effects.h (include + effectList)

Usage:
    # show the full plan, change nothing (default):
    python make_effect.py <export_folder> <project_root>

    # do everything:
    python make_effect.py <export_folder> <project_root> --apply

    # overrides (normally auto-derived from description.json meta):
    python make_effect.py <export_folder> <project_root> --name FreezeVerb --snake freezeverb --apply

    <export_folder>  holds description.json, <snake>.cpp.h, common/
    <project_root>   the GuitarPedal folder (Makefile, loaded_effects.h,
                     Effect-Modules/, dependencies/RNBO/)

Safety:
  - Dry-run by default; --apply required to write.
  - Idempotent: re-running is safe (present lines skipped, files overwritten
    with identical content).
  - Git is the undo. If anything looks wrong:
        git restore Makefile loaded_effects.h Effect-Modules/ dependencies/RNBO/
  - rnbo_allocator.cpp is NEVER touched (your hand-written SDRAM allocator).
  - A runtime version bump is gated behind an explicit y/N prompt.

After a successful --apply:
    make clean && make        (base header changes need a clean build)
    tap RESET, then: make program-dfu
"""

import sys
import os
import json
import math
import shutil

# ===========================================================================
# constants
# ===========================================================================
RESERVED = {"fsw1", "fsw2", "led1", "led2"}
MAX_KNOBS = 6
MAX_MENU = 10
LABEL_WARN_LEN = 12
ALLOCATOR = "rnbo_allocator.cpp"
STAMP = ".rnbo_version"


# ===========================================================================
# STAGE 1 — parse + classify
# ===========================================================================
def load_description(export_folder):
    path = os.path.join(export_folder, "description.json")
    if not os.path.isfile(path):
        sys.exit(f"ERROR: no description.json in {export_folder!r}")
    with open(path, "r") as f:
        return json.load(f)


def classify(params):
    """Return (non_reserved_sorted, knobs, menu, overflow, warnings).

    Non-reserved sorted by (order, index). First MAX_KNOBS -> knobs, next
    MAX_MENU -> menu, rest -> overflow (dropped, warned). Reserved names are
    handled at runtime by the wrapper, not listed here.
    """
    warnings = []
    non_reserved = [p for p in params if p["name"] not in RESERVED]

    # duplicate-order tie warning
    seen = {}
    for p in non_reserved:
        seen.setdefault(p["order"], []).append(p)
    for order_val, group in seen.items():
        if len(group) > 1:
            names = ", ".join(f"{g['name']}(index {g['index']})" for g in group)
            warnings.append(f"order={order_val} shared by: {names} "
                            f"-> tie broken by index (set distinct @order to fix).")

    non_reserved.sort(key=lambda p: (p["order"], p["index"]))

    knobs = non_reserved[:MAX_KNOBS]
    menu = non_reserved[MAX_KNOBS:MAX_KNOBS + MAX_MENU]
    overflow = non_reserved[MAX_KNOBS + MAX_MENU:]

    if overflow:
        warnings.append(f"{len(overflow)} param(s) beyond 16 non-reserved capacity "
                        f"-> DROPPED: {', '.join(p['name'] for p in overflow)}")

    # reserved presence check
    present = {p["name"] for p in params}
    for r in sorted(RESERVED):
        if r not in present:
            warnings.append(f"reserved param {r!r} not in export "
                            f"-> that hardware path will be inactive.")

    # assign knobMapping (slot 0..5 or -1) to each non-reserved param
    for slot, p in enumerate(non_reserved):
        p["knobMapping"] = slot if slot < MAX_KNOBS else -1

    # long-label warnings
    for p in non_reserved:
        label = clean_label(p["name"])
        if len(label) > LABEL_WARN_LEN:
            warnings.append(f"label '{label}' ({len(label)} chars) may truncate on screen")

    return non_reserved, knobs, menu, overflow, warnings


def clean_label(rnbo_name):
    """reverb_time -> 'reverb time'. Underscores to spaces, case untouched."""
    return rnbo_name.replace("_", " ")


def enum_name(rnbo_name):
    return rnbo_name.upper()


# ===========================================================================
# STAGE 2 — module file generation
# ===========================================================================
def gen_header(class_name, snake, module_class, non_reserved):
    guard = f"{snake.upper()}_MODULE_H"
    enum_block = "\n".join(
        f"        {enum_name(p['name'])} = {i}," for i, p in enumerate(non_reserved))

    return f"""#pragma once
#ifndef {guard}
#define {guard}

// ===========================================================================
// {snake}_module.h  — GENERATED by make_effect.py. Do not hand-edit.
// ---------------------------------------------------------------------------
// Self-contained per-effect wrapper, PIMPL shape (RNBO out of the header, only
// forward-declared), so this can sit next to q-using modules in
// loaded_effects.h without the fast-math symbol clash. RNBO access lives in
// {snake}_module.cpp.
// ===========================================================================

#include <stdint.h>
#include "base_effect_module.h"

#ifdef __cplusplus

namespace RNBO {{
    template <class ENGINE> class {class_name};
}}

namespace bkshepherd {{

class {module_class} : public BaseEffectModule {{
  public:
    // kshep-side param SLOTS (not RNBO indices). RNBO index for each is
    // discovered by name at Init(). Order matches s_metaData + s_paramNames.
    enum Param {{
{enum_block}
        PARAM_COUNT
    }};

    {module_class}();
    ~{module_class}();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;
    void SetFootswitch(int fsw_id, float value) override;

    bool UsesRawFootswitch1() const override {{ return true; }}

  private:
    static constexpr int kBlockSize = 48; // must match guitar_pedal.cpp blockSize

    int m_fsw1Index = -1;
    int m_fsw2Index = -1;
    int m_led1Index = -1;
    int m_led2Index = -1;

    int m_paramIndex[PARAM_COUNT];

    volatile float m_fsw1Value = 0.0f;
    volatile float m_fsw2Value = 0.0f;
    volatile float m_led1Value = 0.0f;
    volatile float m_led2Value = 0.0f;

    float m_inL[kBlockSize];
    float m_inR[kBlockSize];
    float m_outL[kBlockSize];
    float m_outR[kBlockSize];
    int m_bufferIndex;

    struct Impl;
    Impl *m_impl;

    void ProcessAudioBlock(int size);
}};

}} // namespace bkshepherd
#endif // __cplusplus
#endif // {guard}
"""


def gen_cpp(class_name, snake, module_class, non_reserved):
    name_rows = ",\n".join(f'    "{p["name"]}"' for p in non_reserved)

    meta_rows = []
    for p in non_reserved:
        label = clean_label(p["name"])
        default = float(p.get("initialValue", 0.0))
        # All params are Float. min/max come from the RNBO range: floor the min,
        # ceil the max, so the pedal shows the real range and kshep scales the
        # knob (0..1) into [min, max] before it reaches RNBO. kshep's min/max are
        # int fields, so a fractional bound (e.g. 0.25) is floored/ceiled -- clamp
        # the exact floor inside the patch if it matters (e.g. base_pitch 0.25).
        vmin = int(math.floor(float(p.get("minimum", 0))))
        vmax = int(math.ceil(float(p.get("maximum", 1))))
        meta_rows.append(f"""    {{
        name : "{label}",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {{.float_value = {default}f}},
        knobMapping : {p['knobMapping']},
        midiCCMapping : -1,
        minValue : {vmin},
        maxValue : {vmax}
    }}""")
    meta_block = ",\n".join(meta_rows)

    return f"""// ===========================================================================
// {snake}_module.cpp  — GENERATED by make_effect.py. Do not hand-edit.
// ---------------------------------------------------------------------------
// The ONE translation unit where RNBO is included. `q` must never be included
// here. Modeled on the proven freezeverb v12 wrapper, generalized from
// hardcoded knobs to a loop over the discovered param table.
// ===========================================================================

#include "{snake}_module.h"
#include <string.h> // strcmp for param-name discovery

// RNBO export, native filename ("<classname>.cpp.h"). Sits at
// dependencies/RNBO/{snake}.cpp.h.
#include "{snake}.cpp.h"

using namespace bkshepherd;

struct {module_class}::Impl {{
    RNBO::{class_name}<> rnbo;
    RNBO::SampleValue *in[2];
    RNBO::SampleValue *out[2];
}};

// Exact RNBO param names, in Param-enum slot order.
static const char *s_paramNames[{module_class}::PARAM_COUNT] = {{
{name_rows}
}};

// kshep parameter metadata, same slot order. Labels from RNBO names; defaults
// from description.json initialValue; no MIDI CC yet (-1).
static const ParameterMetaData s_metaData[{module_class}::PARAM_COUNT] = {{
{meta_block}
}};

{module_class}::{module_class}() : BaseEffectModule(), m_bufferIndex(0), m_impl(nullptr) {{
    m_name = "{class_name}";

    m_paramMetaData = s_metaData;
    InitParams({module_class}::PARAM_COUNT);

    for (int i = 0; i < {module_class}::PARAM_COUNT; ++i) {{
        m_paramIndex[i] = -1;
    }}
    // RNBO object deferred to Init() (module ctors run before hardware/heap up).
}}

{module_class}::~{module_class}() {{
    delete m_impl;
}}

void {module_class}::Init(float sample_rate) {{
    BaseEffectModule::Init(sample_rate);

    m_bufferIndex = 0;
    for (int i = 0; i < kBlockSize; ++i) {{
        m_inL[i] = 0.0f;
        m_inR[i] = 0.0f;
        m_outL[i] = 0.0f;
        m_outR[i] = 0.0f;
    }}

    if (m_impl == nullptr) {{
        m_impl = new Impl();
        m_impl->in[0] = m_inL;
        m_impl->in[1] = m_inR;
        m_impl->out[0] = m_outL;
        m_impl->out[1] = m_outR;
    }}

    m_impl->rnbo.initialize();
    m_impl->rnbo.prepareToProcess(sample_rate, kBlockSize, true);

    m_fsw1Index = m_fsw2Index = m_led1Index = m_led2Index = -1;
    for (int i = 0; i < {module_class}::PARAM_COUNT; ++i) {{
        m_paramIndex[i] = -1;
    }}

    const RNBO::ParameterIndex n = m_impl->rnbo.getNumParameters();
    for (RNBO::ParameterIndex i = 0; i < n; ++i) {{
        const char *pname = m_impl->rnbo.getParameterName(i);
        if (pname == nullptr) {{
            continue;
        }}
        if (strcmp(pname, "fsw1") == 0) {{
            m_fsw1Index = (int)i;
        }} else if (strcmp(pname, "fsw2") == 0) {{
            m_fsw2Index = (int)i;
        }} else if (strcmp(pname, "led1") == 0) {{
            m_led1Index = (int)i;
        }} else if (strcmp(pname, "led2") == 0) {{
            m_led2Index = (int)i;
        }} else {{
            for (int k = 0; k < {module_class}::PARAM_COUNT; ++k) {{
                if (strcmp(pname, s_paramNames[k]) == 0) {{
                    m_paramIndex[k] = (int)i;
                    break;
                }}
            }}
        }}
    }}

    for (int k = 0; k < {module_class}::PARAM_COUNT; ++k) {{
        if (m_paramIndex[k] >= 0) {{
            m_impl->rnbo.setParameterValue(m_paramIndex[k], GetParameterAsFloat(k), RNBO::TimeNow);
        }}
    }}
}}

void {module_class}::ProcessAudioBlock(int size) {{
    for (int k = 0; k < {module_class}::PARAM_COUNT; ++k) {{
        if (m_paramIndex[k] >= 0) {{
            m_impl->rnbo.setParameterValue(m_paramIndex[k], GetParameterAsFloat(k), RNBO::TimeNow);
        }}
    }}

    if (m_fsw1Index >= 0) {{
        m_impl->rnbo.setParameterValue(m_fsw1Index, m_fsw1Value, RNBO::TimeNow);
    }}
    if (m_fsw2Index >= 0) {{
        m_impl->rnbo.setParameterValue(m_fsw2Index, m_fsw2Value, RNBO::TimeNow);
    }}

    m_impl->rnbo.process(m_impl->in, 2, m_impl->out, 2, (RNBO::Index)size);

    if (m_led1Index >= 0) {{
        m_led1Value = (float)m_impl->rnbo.getParameterValue(m_led1Index);
    }}
    if (m_led2Index >= 0) {{
        m_led2Value = (float)m_impl->rnbo.getParameterValue(m_led2Index);
    }}
}}

void {module_class}::SetFootswitch(int fsw_id, float value) {{
    if (fsw_id == 0) {{
        m_fsw1Value = value;
    }} else if (fsw_id == 1) {{
        m_fsw2Value = value;
    }}
}}

void {module_class}::ProcessStereo(float inL, float inR) {{
    if (m_impl == nullptr) {{
        m_audioLeft = inL;
        m_audioRight = inR;
        return;
    }}

    m_inL[m_bufferIndex] = inL;
    m_inR[m_bufferIndex] = inR;

    m_audioLeft = m_outL[m_bufferIndex];
    m_audioRight = m_outR[m_bufferIndex];

    ++m_bufferIndex;
    if (m_bufferIndex >= kBlockSize) {{
        ProcessAudioBlock(kBlockSize);
        m_bufferIndex = 0;
    }}
}}

void {module_class}::ProcessMono(float in) {{
    ProcessStereo(in, in);
}}

float {module_class}::GetBrightnessForLED(int led_id) const {{
    if (m_impl != nullptr) {{
        if (led_id == 0 && m_led1Index >= 0) {{
            return m_led1Value;
        }}
        if (led_id == 1 && m_led2Index >= 0) {{
            return m_led2Value;
        }}
    }}
    return BaseEffectModule::GetBrightnessForLED(led_id);
}}
"""


# ===========================================================================
# STAGE 3 — file wiring (Makefile + loaded_effects.h)
# ===========================================================================
class Insertion:
    def __init__(self, after_idx, text, note):
        self.after_idx = after_idx
        self.text = text
        self.note = note


def read_lines(path):
    if not os.path.isfile(path):
        sys.exit(f"ERROR: not found: {path}")
    with open(path, "r") as f:
        return f.readlines()


def write_lines(path, lines):
    with open(path, "w") as f:
        f.writelines(lines)


def apply_insertions(lines, insertions):
    out = list(lines)
    for ins in sorted(insertions, key=lambda x: x.after_idx, reverse=True):
        text = ins.text if ins.text.endswith("\n") else ins.text + "\n"
        out.insert(ins.after_idx + 1, text)
    return out


def plan_makefile(lines, snake):
    add_line = f"CPP_SOURCES += Effect-Modules/{snake}_module.cpp"
    alloc_line = "CPP_SOURCES += dependencies/RNBO/rnbo_allocator.cpp"
    present_active = any(l.strip() == add_line for l in lines)
    alloc_present = any(l.strip() == alloc_line for l in lines)

    anchor_idx = None
    for i, l in enumerate(lines):
        s = l.strip()
        if s.startswith("CPP_SOURCES += Effect-Modules/") and s.endswith("_module.cpp"):
            anchor_idx = i

    insertions, skipped = [], []
    if present_active:
        skipped.append(f"Makefile: module source already wired")
    elif anchor_idx is None:
        sys.exit("ERROR: Makefile — no CPP_SOURCES Effect-Modules anchor found; aborting.")
    else:
        insertions.append(Insertion(anchor_idx, add_line,
                                    f"Makefile: add module source (after line {anchor_idx+1})"))

    if alloc_present:
        skipped.append("Makefile: allocator already wired")
    else:
        after = anchor_idx if anchor_idx is not None else len(lines) - 1
        insertions.append(Insertion(after, alloc_line,
                                    "Makefile: add RNBO allocator (first RNBO effect)"))
    return insertions, skipped


def plan_loaded_effects(lines, snake, module_class):
    include_line = f'#include "Effect-Modules/{snake}_module.h"'
    list_entry = f"        new {module_class}(),"
    import re
    include_present = any(l.strip() == include_line for l in lines)
    entry_re = re.compile(r"^\s*new\s+" + re.escape(module_class) + r"\s*\(\s*\)\s*,?\s*$")
    entry_present = any(entry_re.match(l) for l in lines)

    inc_anchor = None
    for i, l in enumerate(lines):
        s = l.strip()
        if s.startswith('#include "Effect-Modules/') and s.endswith('_module.h"'):
            inc_anchor = i

    list_anchor = None
    for i, l in enumerate(lines):
        if re.match(r"^\s*new\s+\w+Module\s*\(\s*\)\s*,\s*$", l):
            list_anchor = i

    insertions, skipped = [], []
    if include_present:
        skipped.append("loaded_effects.h: include already present")
    elif inc_anchor is None:
        sys.exit("ERROR: loaded_effects.h — no include anchor found; aborting.")
    else:
        insertions.append(Insertion(inc_anchor, include_line,
                                    f"loaded_effects.h: add include (after line {inc_anchor+1})"))

    if entry_present:
        skipped.append("loaded_effects.h: effectList entry already present")
    elif list_anchor is None:
        sys.exit("ERROR: loaded_effects.h — no effectList anchor found; aborting.")
    else:
        insertions.append(Insertion(list_anchor, list_entry,
                                    f"loaded_effects.h: add effectList entry (after line {list_anchor+1})"))
    return insertions, skipped


# ===========================================================================
# main orchestration
# ===========================================================================
def get_arg(flag):
    if flag in sys.argv:
        i = sys.argv.index(flag)
        if i + 1 < len(sys.argv):
            return sys.argv[i + 1]
    return None


def main():
    apply = "--apply" in sys.argv
    name_override = get_arg("--name")
    snake_override = get_arg("--snake")

    positional = [a for i, a in enumerate(sys.argv[1:], 1)
                  if not a.startswith("--")
                  and sys.argv[i - 1] not in ("--name", "--snake")]
    if len(positional) < 2:
        sys.exit(__doc__)

    export_folder = positional[0]
    project_root = positional[1]

    data = load_description(export_folder)
    meta = data.get("meta", {})
    class_name = name_override or meta.get("rnboobjname", "Effect")
    snake = snake_override or meta.get("name", "effect")
    module_class = f"{class_name}Module"
    version = meta.get("rnboversion", "unknown")

    non_reserved, knobs, menu, overflow, warnings = classify(data["parameters"])

    # paths
    eff_dir = os.path.join(project_root, "Effect-Modules")
    rnbo_dir = os.path.join(project_root, "dependencies", "RNBO")
    mk_path = os.path.join(project_root, "Makefile")
    le_path = os.path.join(project_root, "loaded_effects.h")
    export_cpp_h = os.path.join(export_folder, f"{snake}.cpp.h")
    dest_cpp_h = os.path.join(rnbo_dir, f"{snake}.cpp.h")
    export_common = os.path.join(export_folder, "common")
    dest_common = os.path.join(rnbo_dir, "common")

    for p, what in [(eff_dir, "Effect-Modules"), (rnbo_dir, "dependencies/RNBO")]:
        if not os.path.isdir(p):
            sys.exit(f"ERROR: {what} not found under {project_root!r} — right project root?")
    if not os.path.isfile(export_cpp_h):
        sys.exit(f"ERROR: export header not found: {export_cpp_h}")

    installed_version = None
    stamp_path = os.path.join(rnbo_dir, STAMP)
    if os.path.isfile(stamp_path):
        with open(stamp_path) as f:
            installed_version = f.read().strip()

    mk_lines = read_lines(mk_path)
    le_lines = read_lines(le_path)
    mk_ins, mk_skip = plan_makefile(mk_lines, snake)
    le_ins, le_skip = plan_loaded_effects(le_lines, snake, module_class)

    mode = "APPLY (writing)" if apply else "DRY RUN (no writes)"
    print("=" * 64)
    print(f"  make_effect — {mode}")
    print("=" * 64)
    print(f"  effect        : {class_name}  ({module_class})")
    print(f"  file stem     : {snake}")
    print(f"  RNBO version  : {version}  (installed: {installed_version or 'no stamp'})")
    print(f"  params        : {len(non_reserved)} non-reserved "
          f"({len(knobs)} knob, {len(menu)} menu)")
    for slot, p in enumerate(knobs):
        print(f"      knob {slot}  {p['name']}  (\"{clean_label(p['name'])}\")")
    for p in menu:
        print(f"      menu    {p['name']}  (\"{clean_label(p['name'])}\")")
    print()
    print("  STAGE 2 — module files:")
    print(f"    write {eff_dir}/{snake}_module.h")
    print(f"    write {eff_dir}/{snake}_module.cpp")
    print("  STAGE 3 — export copy:")
    print(f"    copy  {snake}.cpp.h -> dependencies/RNBO/"
          + ("  (overwrites)" if os.path.isfile(dest_cpp_h) else ""))
    version_mismatch = (installed_version is not None) and (installed_version != version)
    if installed_version is None:
        if os.path.isdir(dest_common):
            print(f"    runtime present, no stamp -> write stamp ({version}), leave headers")
        else:
            print(f"    no runtime -> copy common/ + write stamp ({version})")
    elif version_mismatch:
        print(f"    ! RUNTIME MISMATCH {installed_version} -> {version} "
              f"(will ask before refreshing)")
    else:
        print(f"    runtime current ({version}) -> header only")
    print(f"    ({ALLOCATOR} never touched)")
    print("  STAGE 3 — wiring:")
    for ins in mk_ins + le_ins:
        print(f"    + {ins.note}")
    for s in mk_skip + le_skip:
        print(f"    (skip) {s}")
    if warnings:
        print("\n  WARNINGS:")
        for w in warnings:
            print(f"    ! {w}")
    print("=" * 64)

    if not apply:
        print("  DRY RUN — nothing written. Re-run with --apply to build.")
        return

    # ---------------- APPLY ----------------
    # Stage 2: module files
    with open(os.path.join(eff_dir, f"{snake}_module.h"), "w") as f:
        f.write(gen_header(class_name, snake, module_class, non_reserved))
    with open(os.path.join(eff_dir, f"{snake}_module.cpp"), "w") as f:
        f.write(gen_cpp(class_name, snake, module_class, non_reserved))
    print(f"  wrote module files -> Effect-Modules/")

    # Stage 3: export copy
    shutil.copy2(export_cpp_h, dest_cpp_h)
    print(f"  copied {snake}.cpp.h -> dependencies/RNBO/")

    def copy_runtime():
        if not os.path.isdir(export_common):
            print("  WARNING: export has no common/ — cannot refresh runtime.")
            return False
        os.makedirs(dest_common, exist_ok=True)
        for root, _dirs, files in os.walk(export_common):
            rel = os.path.relpath(root, export_common)
            tgt = os.path.join(dest_common, rel) if rel != "." else dest_common
            os.makedirs(tgt, exist_ok=True)
            for fn in files:
                if fn == ALLOCATOR:
                    continue
                shutil.copy2(os.path.join(root, fn), os.path.join(tgt, fn))
        return True

    def write_stamp():
        with open(stamp_path, "w") as f:
            f.write(version + "\n")

    if installed_version is None:
        if os.path.isdir(dest_common):
            write_stamp()
            print(f"  wrote stamp ({version}); left existing runtime as-is.")
        else:
            if copy_runtime():
                write_stamp()
                print(f"  installed runtime + stamp ({version}).")
    elif version_mismatch:
        resp = input(f"  Refresh runtime {installed_version} -> {version}? [y/N] ").strip().lower()
        if resp == "y":
            if copy_runtime():
                write_stamp()
                print(f"  refreshed runtime, stamp now {version}.")
        else:
            print("  runtime left unchanged (export header may not match runtime).")

    # Stage 3: wiring
    if mk_ins:
        write_lines(mk_path, apply_insertions(mk_lines, mk_ins))
    if le_ins:
        write_lines(le_path, apply_insertions(le_lines, le_ins))
    print(f"  wired Makefile + loaded_effects.h")

    print("\n  DONE. Next:")
    print("    make clean && make")
    print("    tap RESET, then: make program-dfu")
    print("  Undo everything:")
    print("    git restore Makefile loaded_effects.h Effect-Modules/ dependencies/RNBO/")


if __name__ == "__main__":
    main()
