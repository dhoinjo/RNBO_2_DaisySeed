#!/usr/bin/env python3
"""
make_effect.py v12 — RNBO-on-kshep effect generator (pedal lists managed for you)

EVERYDAY USE — two commands:

    # 1. export (or re-export) an effect and put it on a pedal:
    python3 tools/make_effect.py RNBO_exports/tribonacci . --apply --pedal purple

    # 2. build / flash that pedal:
    make PEDAL=purple
    make PEDAL=purple program-dfu

MORE:
    --pedal purple,blue     put the effect on several pedals at once
    --off purple            take the effect off a pedal
    --fast-pool 128         give the pedal(s) in --pedal a fast RNBO pool of
                            128 KB in internal SRAM (0 = none). Effects that
                            come first in the pedal's list get it first.
    (without --apply)       dry run: shows what would happen, writes nothing

    python3 tools/make_effect.py --pedals .
                            show every pedal and its effects

Pedals: purple, yellow, green, orange, blue (edit PEDALS below to change).
The tool keeps one small list per pedal in pedals/<colour>.mk; you never
need to open or create those files yourself.

What a run does:
  1. PARSE      description.json -> effect name + classified params
  2. MODULE     write Effect-Modules/<snake>_module.cpp/.h   (only if changed)
  3. EXPORT     copy <snake>.cpp.h into dependencies/RNBO/   (only if changed)
                (+ runtime version check)
  4. WIRE       loaded_effects.h: include + effectList entry, each wrapped in
                #ifdef FX_<snake> ... #endif
  5. PEDALS     add/remove the effect in pedals/<colour>.mk

Files are only rewritten when their content changed, so make only recompiles
what you actually changed. No `make clean` needed. The Makefile is never
edited by this tool (editing it would rebuild everything).

Stepped params (v12): an RNBO param with @steps N (N >= 3) becomes a stepped
kshep param: knob, MIDI and menu move through exactly N values from min to max
(e.g. 1..16 @steps 16 -> 1,2,...,16), and the menu shows the real values.
@steps 2 on a 0..1 param stays a checkbox. No @steps = continuous, as before.

Overrides (normally auto-derived from description.json meta):
    --name FreezeVerb --snake freezeverb

Safety:
  - Dry-run by default; --apply required to write.
  - Re-running is safe.
  - Git is the undo:
        git restore loaded_effects.h Effect-Modules/ dependencies/RNBO/ pedals/
  - rnbo_allocator.cpp is NEVER touched (your hand-written SDRAM allocator).
  - A runtime version bump is gated behind an explicit y/N prompt.
"""

import sys
import os
import re
import glob
import json
import math
import shutil
import filecmp

# ===========================================================================
# constants
# ===========================================================================
RESERVED = {"fsw1", "fsw2", "led1", "led2"}
MAX_KNOBS = 6
MAX_MENU = 18   # 6 knobs + 18 menu = 24 assignable params (+4 reserved)

# MIDI CC per param slot (slot = knob order: knobs 0-5 first, then menu params).
# Only the first 16 slots get a CC; slots 17-24 have no CC (menu only).
# Slots 0-11 -> CC 20-31 (kshep's own convention, see EffectChain examples).
# Slots 12-15 -> CC 102-105. We skip 32-35 on purpose: CC 32 is Bank Select LSB
# and 33-35 are LSBs of mod wheel etc., so a controller could hit them by accident.
# All of these are "undefined" in the MIDI spec. Edit this list to remap.
MIDI_CC_BY_SLOT = list(range(20, 32)) + [102, 103, 104, 105]
LABEL_WARN_LEN = 12
MAX_STEPS = 128   # stepped params with more steps stay continuous (Float)
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
        warnings.append(f"{len(overflow)} param(s) beyond {MAX_KNOBS + MAX_MENU} non-reserved "
                        f"capacity -> DROPPED: {', '.join(p['name'] for p in overflow)}")
    # actually drop them (previous versions warned but still emitted them)
    non_reserved = knobs + menu

    # reserved presence check
    present = {p["name"] for p in params}
    for r in sorted(RESERVED):
        if r not in present:
            warnings.append(f"reserved param {r!r} not in export "
                            f"-> that hardware path will be inactive.")

    # assign knobMapping (slot 0..5 or -1) to each non-reserved param
    # and a MIDI CC (MIDI_CC_BY_SLOT, or -1 beyond the table)
    for slot, p in enumerate(non_reserved):
        p["knobMapping"] = slot if slot < MAX_KNOBS else -1
        p["midiCC"] = MIDI_CC_BY_SLOT[slot] if slot < len(MIDI_CC_BY_SLOT) else -1
        p["isBool"] = is_checkbox(p)
        p["stepValues"] = None if p["isBool"] else step_values(p)
        try:
            n_steps = int(p.get("steps", 0) or 0)
        except (TypeError, ValueError):
            n_steps = 0
        if not p["isBool"] and n_steps > MAX_STEPS:
            warnings.append(f"'{p['name']}' has @steps {n_steps} (> {MAX_STEPS}) -> kept continuous")
        if p["isBool"] and p["knobMapping"] >= 0:
            warnings.append(f"checkbox '{p['name']}' landed on knob {slot} "
                            f"(knob past halfway = on). Give it a higher @order "
                            f"to put it in the menu instead.")

    # long-label warnings
    for p in non_reserved:
        label = clean_label(p["name"])
        if len(label) > LABEL_WARN_LEN:
            warnings.append(f"label '{label}' ({len(label)} chars) may truncate on screen")

    return non_reserved, knobs, menu, overflow, warnings


def stepped_tag(p):
    sv = p.get("stepValues")
    if not sv:
        return ""
    return f"  [stepped {len(sv)}: {fmt_step(sv[0])}..{fmt_step(sv[-1])}]"


def cc_label(p):
    return f"CC {p['midiCC']:>3}" if p.get("midiCC", -1) >= 0 else "no CC "


def is_checkbox(p):
    """RNBO param with min 0, max 1 and @steps 2 -> kshep Bool (menu checkbox)."""
    try:
        return (float(p.get("minimum", 0)) == 0.0
                and float(p.get("maximum", 1)) == 1.0
                and int(p.get("steps", 0)) == 2)
    except (TypeError, ValueError):
        return False


def step_values(p):
    """Values of a stepped param (@steps N, 3 <= N <= MAX_STEPS), else None."""
    try:
        n = int(p.get("steps", 0) or 0)
        lo = float(p.get("minimum", 0))
        hi = float(p.get("maximum", 1))
    except (TypeError, ValueError):
        return None
    if n < 3 or n > MAX_STEPS or hi <= lo:
        return None
    step = (hi - lo) / (n - 1)
    return [lo + i * step for i in range(n)]


def fmt_step(v):
    """Menu label for a step value: '7', '0.25', '-3'."""
    if abs(v - round(v)) < 1e-6:
        return str(int(round(v)))
    return f"{v:.3f}".rstrip("0").rstrip(".")


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
    // RNBO runs once per kBlockSize samples. Independent of the pedal's own
    // block size; it sets this wrapper's added latency (48 = 1 ms).
    static constexpr int kBlockSize = 48;

    int m_fsw1Index = -1;
    int m_fsw2Index = -1;
    int m_led1Index = -1;
    int m_led2Index = -1;

    int m_paramIndex[PARAM_COUNT];

    // Last value sent to RNBO per param / footswitch. Only changes are sent.
    float m_lastSent[PARAM_COUNT];
    float m_lastFsw1Sent = -1.0f;
    float m_lastFsw2Sent = -1.0f;

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

    // Value to send RNBO for param slot k. Bool (checkbox) and Binned (stepped)
    // params are stored as raw ints, so GetParameterAsFloat would return
    // garbage for them.
    float ParamValueForRnbo(int k) const;
}};

}} // namespace bkshepherd
#endif // __cplusplus
#endif // {guard}
"""


def gen_cpp(class_name, snake, module_class, non_reserved):
    name_rows = ",\n".join(f'    "{p["name"]}"' for p in non_reserved)

    bin_arrays = []
    step_min_rows = []
    step_size_rows = []
    for k, p in enumerate(non_reserved):
        sv = p.get("stepValues")
        if sv:
            names = ", ".join(f'"{fmt_step(v)}"' for v in sv)
            bin_arrays.append(f"static const char *s_bins{k}[] = {{{names}}};")
            step_min_rows.append(f"{sv[0]!r}f")
            step_size_rows.append(f"{(sv[1] - sv[0])!r}f")
        else:
            step_min_rows.append("0.0f")
            step_size_rows.append("0.0f")
    bin_block = "\n".join(bin_arrays) if bin_arrays else "// (no stepped params)"
    step_min_block = ", ".join(step_min_rows)
    step_size_block = ", ".join(step_size_rows)

    meta_rows = []
    for k, p in enumerate(non_reserved):
        label = clean_label(p["name"])
        default = float(p.get("initialValue", 0.0))
        # All params are Float. min/max come from the RNBO range: floor the min,
        # ceil the max, so the pedal shows the real range and kshep scales the
        # knob (0..1) into [min, max] before it reaches RNBO. kshep's min/max are
        # int fields, so a fractional bound (e.g. 0.25) is floored/ceiled -- clamp
        # the exact floor inside the patch if it matters (e.g. base_pitch 0.25).
        vmin = int(math.floor(float(p.get("minimum", 0))))
        vmax = int(math.ceil(float(p.get("maximum", 1))))
        sv = p.get("stepValues")
        if sv:
            # Stepped (@steps N): kshep Binned. Raw value = step index (0-based);
            # the menu shows s_bins<k> (the real values); the wrapper sends RNBO
            # s_stepMin + index * s_stepSize.
            idx = min(range(len(sv)), key=lambda i: abs(sv[i] - default))
            meta_rows.append(f"""    {{
        name : "{label}",
        valueType : ParameterValueType::Binned,
        valueBinCount : {len(sv)},
        valueBinNames : s_bins{k},
        defaultValue : {{.uint_value = {idx}}},
        knobMapping : {p['knobMapping']},
        midiCCMapping : {p['midiCC']},
        minValue : 0,
        maxValue : 1
    }}""")
            continue
        if p.get("isBool"):
            # Checkbox (@steps 2, 0..1). kshep stores Bool defaults as an int.
            bool_default = 1 if default >= 0.5 else 0
            meta_rows.append(f"""    {{
        name : "{label}",
        valueType : ParameterValueType::Bool,
        valueBinCount : 0,
        defaultValue : {{.uint_value = {bool_default}}},
        knobMapping : {p['knobMapping']},
        midiCCMapping : {p['midiCC']},
        minValue : 0,
        maxValue : 1
    }}""")
            continue
        meta_rows.append(f"""    {{
        name : "{label}",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {{.float_value = {default}f}},
        knobMapping : {p['knobMapping']},
        midiCCMapping : {p['midiCC']},
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

// Stepped params (@steps N): menu labels (the real values) per step.
{bin_block}

// Stepped params: value sent to RNBO = s_stepMin[k] + (bin - 1) * s_stepSize[k].
static const float s_stepMin[{module_class}::PARAM_COUNT] = {{{step_min_block}}};
static const float s_stepSize[{module_class}::PARAM_COUNT] = {{{step_size_block}}};

// kshep parameter metadata, same slot order. Labels from RNBO names; defaults
// from description.json initialValue; min/max from the RNBO range; MIDI CC per
// slot (CC 20-31, then 102-105), scaled into min/max like the knobs.
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

    // Initial push of every param; afterwards only changes are sent.
    for (int k = 0; k < {module_class}::PARAM_COUNT; ++k) {{
        m_lastSent[k] = ParamValueForRnbo(k);
        if (m_paramIndex[k] >= 0) {{
            m_impl->rnbo.setParameterValue(m_paramIndex[k], m_lastSent[k], RNBO::TimeNow);
        }}
    }}
    m_lastFsw1Sent = -1.0f; // forces the first footswitch state to be sent
    m_lastFsw2Sent = -1.0f;
}}

void {module_class}::ProcessAudioBlock(int size) {{
    // Send only what changed since the last block (each send runs RNBO's
    // event machinery, so skipping unchanged values saves CPU).
    for (int k = 0; k < {module_class}::PARAM_COUNT; ++k) {{
        const float v = ParamValueForRnbo(k);
        if (v != m_lastSent[k]) {{
            m_lastSent[k] = v;
            if (m_paramIndex[k] >= 0) {{
                m_impl->rnbo.setParameterValue(m_paramIndex[k], v, RNBO::TimeNow);
            }}
        }}
    }}

    const float fsw1 = m_fsw1Value;
    if (fsw1 != m_lastFsw1Sent) {{
        m_lastFsw1Sent = fsw1;
        if (m_fsw1Index >= 0) {{
            m_impl->rnbo.setParameterValue(m_fsw1Index, fsw1, RNBO::TimeNow);
        }}
    }}
    const float fsw2 = m_fsw2Value;
    if (fsw2 != m_lastFsw2Sent) {{
        m_lastFsw2Sent = fsw2;
        if (m_fsw2Index >= 0) {{
            m_impl->rnbo.setParameterValue(m_fsw2Index, fsw2, RNBO::TimeNow);
        }}
    }}

    m_impl->rnbo.process(m_impl->in, 2, m_impl->out, 2, (RNBO::Index)size);

    if (m_led1Index >= 0) {{
        m_led1Value = (float)m_impl->rnbo.getParameterValue(m_led1Index);
    }}
    if (m_led2Index >= 0) {{
        m_led2Value = (float)m_impl->rnbo.getParameterValue(m_led2Index);
    }}
}}

float {module_class}::ParamValueForRnbo(int k) const {{
    const ParameterValueType t = GetParameterType(k);
    if (t == ParameterValueType::Bool) {{
        return GetParameterAsBool(k) ? 1.0f : 0.0f;
    }}
    if (t == ParameterValueType::Binned) {{
        return s_stepMin[k] + (float)(GetParameterAsBinnedValue(k) - 1) * s_stepSize[k];
    }}
    return GetParameterAsFloat(k);
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
# STAGE 3/4 helpers — write only when changed
# ===========================================================================
def read_lines(path):
    if not os.path.isfile(path):
        sys.exit(f"ERROR: not found: {path}")
    with open(path, "r") as f:
        return f.readlines()


def write_lines(path, lines):
    with open(path, "w") as f:
        f.writelines(lines)


def text_differs(path, text):
    """True if path is missing or its content differs from text."""
    if not os.path.isfile(path):
        return True
    with open(path, "r") as f:
        return f.read() != text


def write_if_changed(path, text):
    """Write only when content differs. Unchanged file keeps its date, so
    make doesn't recompile it. Returns True if written."""
    if not text_differs(path, text):
        return False
    with open(path, "w") as f:
        f.write(text)
    return True


def file_differs(src, dst):
    return (not os.path.isfile(dst)) or (not filecmp.cmp(src, dst, shallow=False))


def copy_if_changed(src, dst):
    """Copy only when content differs. Uses copyfile (date = now), not copy2:
    keeping the export's old date could make make skip a needed recompile."""
    if not file_differs(src, dst):
        return False
    shutil.copyfile(src, dst)
    return True


# ===========================================================================
# STAGE 4 — loaded_effects.h wiring (#ifdef FX_<snake> guards)
# ===========================================================================
def guard_open(snake):
    return f"#ifdef FX_{snake}"


GUARD_CLOSE = "#endif"


def include_text(snake):
    return f'#include "Effect-Modules/{snake}_module.h"'


def entry_regex(module_class):
    return re.compile(r"^\s*new\s+" + re.escape(module_class)
                      + r"\s*\(\s*\)\s*,?\s*(//.*)?$")


ANY_INCLUDE_RE = re.compile(r'^\s*#include\s+"Effect-Modules/\w+_module\.h"')
ANY_ENTRY_RE = re.compile(r"^\s*new\s+\w+\s*\(\s*\)\s*,\s*(//.*)?$")


def find_line(lines, pred):
    for i, l in enumerate(lines):
        if pred(l):
            return i
    return None


def is_wrapped(lines, i, snake):
    """True if the line already sits directly inside ANY FX guard. (v11: v10
    only accepted this effect's own guard, so a line matched by two effects got
    wrapped twice and later effects ended up nested -- switched off.)"""
    return (i > 0 and i + 1 < len(lines)
            and lines[i - 1].strip().startswith("#ifdef FX_")
            and lines[i + 1].strip() == GUARD_CLOSE)


def wrap_at(lines, i, snake):
    lines.insert(i + 1, GUARD_CLOSE + "\n")
    lines.insert(i, guard_open(snake) + "\n")


def anchor_after(lines, regex):
    """Index after which to insert a new guarded block: after the last line
    matching regex, stepping past its #endif if that line is itself guarded."""
    idx = None
    for i, l in enumerate(lines):
        if regex.match(l):
            idx = i
    if idx is not None and idx + 1 < len(lines) and lines[idx + 1].strip() == GUARD_CLOSE:
        idx += 1
    return idx


LIST_OPEN_RE = re.compile(r"effectList\s*\[\s*\]\s*=\s*\{")


def commented_regex(inner):
    """Match a commented-out line like  //new X(),  or  // #include "..." """
    return re.compile(r"^\s*//\s*" + inner)


def wire_loaded_effects(lines, effects, new_snake):
    """effects: list of (snake, module_class) for every generated module.
    New effect gets added (guarded); existing unguarded generated ones get
    wrapped. A commented-out line for the new effect (//new X(), or
    //#include ...) is replaced in place by the guarded version.
    Returns (new_lines, notes)."""
    out = list(lines)
    notes = []
    for snake, cls in effects:
        inc = include_text(snake)
        ent_re = entry_regex(cls)
        inc_commented = commented_regex(re.escape(inc) + r"\s*(//.*)?$")
        ent_commented = commented_regex(r"new\s+" + re.escape(cls)
                                        + r"\s*\(\s*\)\s*,?\s*(//.*)?$")

        # include
        i = find_line(out, lambda l: l.strip() == inc)
        if i is not None:
            if not is_wrapped(out, i, snake):
                wrap_at(out, i, snake)
                notes.append(f"wrap include of {snake} in #ifdef FX_{snake}")
        elif snake == new_snake:
            c = find_line(out, lambda l: bool(inc_commented.match(l)))
            if c is not None:
                out[c:c + 1] = [guard_open(snake) + "\n", inc + "\n", GUARD_CLOSE + "\n"]
                notes.append(f"replace commented-out include of {snake} (guarded)")
            else:
                a = anchor_after(out, ANY_INCLUDE_RE)
                if a is None:
                    sys.exit("ERROR: loaded_effects.h — no Effect-Modules include anchor found; aborting.")
                out[a + 1:a + 1] = [guard_open(snake) + "\n", inc + "\n", GUARD_CLOSE + "\n"]
                notes.append(f"add include of {snake} (guarded)")

        # effectList entry
        i = find_line(out, lambda l: bool(ent_re.match(l)))
        if i is not None:
            if not is_wrapped(out, i, snake):
                wrap_at(out, i, snake)
                notes.append(f"wrap effectList entry {cls} in #ifdef FX_{snake}")
        elif snake == new_snake:
            block = [guard_open(snake) + "\n", f"        new {cls}(),\n", GUARD_CLOSE + "\n"]
            c = find_line(out, lambda l: bool(ent_commented.match(l)))
            if c is not None:
                out[c:c + 1] = block
                notes.append(f"replace commented-out entry {cls} (guarded)")
            else:
                a = anchor_after(out, ANY_ENTRY_RE)
                if a is None:
                    # empty list (everything commented out): put it first
                    a = find_line(out, lambda l: bool(LIST_OPEN_RE.search(l)))
                if a is None:
                    sys.exit("ERROR: loaded_effects.h — no 'effectList[] = {' found; aborting.")
                out[a + 1:a + 1] = block
                notes.append(f"add effectList entry {cls} (guarded)")
    return out, notes


def check_guard_structure(lines):
    """Problems with the FX guards: an '#ifdef FX_' opened inside another
    '#ifdef FX_' (nested -> the inner effect is switched off unless both are
    on), or unbalanced #if/#endif. Returns a list of messages."""
    problems = []
    stack = []  # (line_no, text) of open #if lines
    for n, l in enumerate(lines, 1):
        t = l.strip()
        if t.startswith("#if"):
            if t.startswith("#ifdef FX_") and any(x[1].startswith("#ifdef FX_") for x in stack):
                outer = [x for x in stack if x[1].startswith("#ifdef FX_")][-1]
                problems.append(f"line {n}: '{t}' is nested inside '{outer[1]}' (line {outer[0]})")
            stack.append((n, t))
        elif t.startswith("#endif"):
            if not stack:
                problems.append(f"line {n}: '#endif' without a matching #if")
            else:
                stack.pop()
    for n, t in stack:
        problems.append(f"line {n}: '{t}' is never closed")
    return problems


EMPTY_LIST_ASSERT = ("    static_assert(sizeof(effectList) > 0, \"This pedal's effect list is empty: "
                     "no effect in its pedals/<colour>.mk is wired into loaded_effects.h. "
                     "Run make_effect.py for it.\");\n")


def add_empty_list_assert(lines):
    """Insert a compile-time check after the effectList's closing '};' so an
    empty list stops the build instead of flashing a pedal that freezes."""
    if any("static_assert(sizeof(effectList)" in l for l in lines):
        return lines, False
    start = find_line(lines, lambda l: bool(LIST_OPEN_RE.search(l)))
    if start is None:
        return lines, False
    for i in range(start + 1, len(lines)):
        if lines[i].strip() == "};":
            out = list(lines)
            out.insert(i + 1, EMPTY_LIST_ASSERT)
            return out, True
    return lines, False


def generated_modules(eff_dir):
    """All modules this tool generated: [(snake, module_class)]."""
    found = []
    for h in sorted(glob.glob(os.path.join(eff_dir, "*_module.h"))):
        with open(h, "r") as f:
            txt = f.read()
        if "GENERATED by make_effect.py" not in txt:
            continue
        m = re.search(r"class\s+(\w+)\s*:\s*public\s+BaseEffectModule", txt)
        if m:
            snake = os.path.basename(h)[:-len("_module.h")]
            found.append((snake, m.group(1)))
    return found


# ===========================================================================
# Makefile / pedal profile checks (read-only)
# ===========================================================================
def makefile_report(mk_lines, gen_snakes):
    """Return (has_pedal_block, old_lines_to_delete)."""
    has_block = any("PEDAL_FILE" in l for l in mk_lines)
    old = []
    in_block = False
    for n, l in enumerate(mk_lines, 1):
        s = l.strip()
        if "Pedal profiles" in s and "[start]" in s:
            in_block = True
        elif "Pedal profiles" in s and "[end]" in s:
            in_block = False
        if in_block or s.startswith("#"):
            continue
        if s == "CPP_SOURCES += dependencies/RNBO/rnbo_allocator.cpp":
            old.append((n, s))
        for snake in gen_snakes:
            if s == f"CPP_SOURCES += Effect-Modules/{snake}_module.cpp":
                old.append((n, s))
    return has_block, old


PEDALS = ["purple", "yellow", "green", "orange", "blue"]


def pedal_path(project_root, pedal):
    return os.path.join(project_root, "pedals", f"{pedal}.mk")


def read_pedal(project_root, pedal):
    """Effects listed on a pedal ([] if the pedal file doesn't exist yet)."""
    path = pedal_path(project_root, pedal)
    effects = []
    if os.path.isfile(path):
        with open(path, "r") as f:
            for l in f:
                s = l.split("#", 1)[0]
                if re.match(r"^\s*EFFECTS\s*[:+]?=", s):
                    effects += s.split("=", 1)[1].split()
    return effects


def read_pedal_lines(project_root, pedal):
    path = pedal_path(project_root, pedal)
    if not os.path.isfile(path):
        return []
    with open(path, "r") as f:
        return f.readlines()


def read_fast_pool(project_root, pedal):
    """FAST_POOL_KB set in the pedal file, or 0."""
    for l in read_pedal_lines(project_root, pedal):
        m = re.match(r"^\s*FAST_POOL_KB\s*[:?]?=\s*(\d+)", l.split("#", 1)[0])
        if m:
            return int(m.group(1))
    return 0


def pedal_text(project_root, pedal, effects, fast_pool_kb=None):
    """New pedal file content. Keeps any other lines the file already has;
    replaces the EFFECTS line, and FAST_POOL_KB when fast_pool_kb is given."""
    out = [f"# pedal: {pedal} -- managed by make_effect.py (--pedal / --off / --fast-pool)\n",
           f"EFFECTS = {' '.join(effects)}\n"]
    pool_line = None
    if fast_pool_kb is None:
        cur = read_fast_pool(project_root, pedal)
        if cur:
            pool_line = f"FAST_POOL_KB = {cur}\n"
    elif fast_pool_kb > 0:
        pool_line = f"FAST_POOL_KB = {fast_pool_kb}\n"
    if pool_line:
        out.append(pool_line)
    for l in read_pedal_lines(project_root, pedal):
        code = l.split("#", 1)[0]
        if l.startswith("# pedal:"):
            continue
        if re.match(r"^\s*(EFFECTS|FAST_POOL_KB)\s*[:+?]?=", code):
            continue
        out.append(l if l.endswith("\n") else l + "\n")
    return "".join(out)


def all_pedals(project_root):
    """Known pedals first (in PEDALS order), then any extra files found."""
    found = [os.path.basename(p)[:-3]
             for p in sorted(glob.glob(os.path.join(project_root, "pedals", "*.mk")))]
    return PEDALS + [p for p in found if p not in PEDALS]


def parse_pedal_list(value, flag):
    if value is None:
        return []
    names = [v.strip().lower() for v in value.split(",") if v.strip()]
    bad = [n for n in names if n not in PEDALS]
    if bad:
        sys.exit(f"ERROR: {flag} {', '.join(bad)} -- unknown pedal. "
                 f"Pedals are: {', '.join(PEDALS)}")
    return names


def print_pedals(project_root):
    print("  All pedals:")
    for p in all_pedals(project_root):
        eff = read_pedal(project_root, p)
        pool = read_fast_pool(project_root, p)
        print(f"    {p:<8} {' '.join(eff) if eff else '(empty)'}"
              + (f"   [fast pool {pool} KB]" if pool else ""))


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
    on_pedals = parse_pedal_list(get_arg("--pedal"), "--pedal")
    off_pedals = parse_pedal_list(get_arg("--off"), "--off")
    clash = set(on_pedals) & set(off_pedals)
    if clash:
        sys.exit(f"ERROR: {', '.join(clash)} is in both --pedal and --off")

    fast_pool_arg = get_arg("--fast-pool")
    fast_pool_kb = None
    if fast_pool_arg is not None:
        if not fast_pool_arg.isdigit():
            sys.exit("ERROR: --fast-pool takes a size in KB, e.g. --fast-pool 128")
        fast_pool_kb = int(fast_pool_arg)
        if not on_pedals:
            sys.exit("ERROR: --fast-pool needs --pedal <colour> to say which pedal")

    value_flags = ("--name", "--snake", "--pedal", "--off", "--fast-pool")
    positional = [a for i, a in enumerate(sys.argv[1:], 1)
                  if not a.startswith("--")
                  and sys.argv[i - 1] not in value_flags]

    if "--pedals" in sys.argv:
        print_pedals(positional[0] if positional else ".")
        return

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
    mod_h_path = os.path.join(eff_dir, f"{snake}_module.h")
    mod_cpp_path = os.path.join(eff_dir, f"{snake}_module.cpp")

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

    # generated content + what changed
    h_text = gen_header(class_name, snake, module_class, non_reserved)
    cpp_text = gen_cpp(class_name, snake, module_class, non_reserved)
    h_changed = text_differs(mod_h_path, h_text)
    cpp_changed = text_differs(mod_cpp_path, cpp_text)
    export_changed = file_differs(export_cpp_h, dest_cpp_h)

    # loaded_effects.h: this effect + every other generated module (migration)
    gen = [(s, c) for (s, c) in generated_modules(eff_dir) if s != snake]
    gen.append((snake, module_class))
    le_lines = read_lines(le_path)

    # v11: two effects with the same class name confuse the wiring.
    by_class = {}
    for s_, c_ in gen:
        by_class.setdefault(c_, []).append(s_)
    dupes = {c_: ss for c_, ss in by_class.items() if len(ss) > 1}
    for c_, ss in dupes.items():
        warnings.append(f"effects {', '.join(ss)} all define class {c_} -- delete the stale "
                        f"one's Effect-Modules/<name>_module.* files (or re-export with its own name)")
    if snake in dupes.get(module_class, []):
        sys.exit(f"ERROR: class {module_class} is also used by "
                 f"{', '.join(x for x in dupes[module_class] if x != snake)}. "
                 f"Give this export its own name (or delete the stale module files) first.")
    gen = [(s_, c_) for (s_, c_) in gen if s_ == snake or c_ not in dupes]

    problems = check_guard_structure(le_lines)
    if problems:
        print("ERROR: loaded_effects.h has broken FX guards (nothing written):")
        for pr in problems:
            print(f"    {pr}")
        sys.exit("Fix those lines (each effect needs its own #ifdef FX_name / #endif pair, "
                 "not inside another one) and run again.")

    le_new, le_notes = wire_loaded_effects(le_lines, gen, snake)
    le_new, added_assert = add_empty_list_assert(le_new)
    if added_assert:
        le_notes.append("add empty-list check (static_assert) after effectList")
    post = check_guard_structure(le_new)
    if post:
        sys.exit("ERROR: internal -- wiring would break loaded_effects.h: " + "; ".join(post))

    # Makefile + pedals (read-only)
    mk_lines = read_lines(mk_path)
    has_block, old_mk = makefile_report(mk_lines, [s for s, _ in gen])
    # pedal lists: current -> wanted
    pedal_changes = []   # (pedal, new_effects, note)
    for p in on_pedals:
        cur = read_pedal(project_root, p)
        if snake not in cur:
            pedal_changes.append((p, cur + [snake], f"add {snake} to {p}"))
        if fast_pool_kb is not None and fast_pool_kb != read_fast_pool(project_root, p):
            eff = cur + [snake] if snake not in cur else cur
            pedal_changes.append((p, eff, f"set fast pool of {p} to {fast_pool_kb} KB"
                                  + (" (none)" if fast_pool_kb == 0 else "")))
    for p in off_pedals:
        cur = read_pedal(project_root, p)
        if snake in cur:
            pedal_changes.append((p, [e for e in cur if e != snake], f"take {snake} off {p}"))
    changed = {p: eff for p, eff, _ in pedal_changes}
    on_after = [p for p in all_pedals(project_root)
                if snake in changed.get(p, read_pedal(project_root, p))]

    def state(changed):
        return "changed -> write" if changed else "unchanged -> skip (no recompile)"

    mode = "APPLY (writing)" if apply else "DRY RUN (no writes)"
    print("=" * 64)
    print(f"  make_effect v12 — {mode}")
    print("=" * 64)
    print(f"  effect        : {class_name}  ({module_class})")
    print(f"  file stem     : {snake}   (pedal profile name + define FX_{snake})")
    print(f"  RNBO version  : {version}  (installed: {installed_version or 'no stamp'})")
    print(f"  params        : {len(non_reserved)} non-reserved "
          f"({len(knobs)} knob, {len(menu)} menu)")
    for slot, p in enumerate(knobs):
        print(f"      knob {slot}  {cc_label(p)}  {p['name']}  (\"{clean_label(p['name'])}\")"
              + ("  [checkbox]" if p.get("isBool") else "") + stepped_tag(p))
    for p in menu:
        print(f"      menu    {cc_label(p)}  {p['name']}  (\"{clean_label(p['name'])}\")"
              + ("  [checkbox]" if p.get("isBool") else "") + stepped_tag(p))
    print()
    print("  STAGE 2 — module files:")
    print(f"    {snake}_module.h    {state(h_changed)}")
    print(f"    {snake}_module.cpp  {state(cpp_changed)}")
    print("  STAGE 3 — export copy:")
    print(f"    {snake}.cpp.h       {state(export_changed)}")
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
    print("  STAGE 4 — loaded_effects.h:")
    if le_notes:
        for n in le_notes:
            print(f"    + {n}")
    else:
        print("    already wired (guarded) -> skip")
    print("  Makefile (never edited by this tool):")
    if has_block:
        print("    pedal block present")
    else:
        print("    ! no pedal block yet -> paste the pedal block once (see notes)")
    for n, s in old_mk:
        print(f"    ! line {n}: delete '{s}'  (pedal block handles it now)")
    print("  STAGE 5 — pedals:")
    for _, _, note in pedal_changes:
        print(f"    + {note}")
    if not pedal_changes and (on_pedals or off_pedals):
        print("    no change")
    if on_after:
        print(f"    {snake} is on: {', '.join(on_after)}")
    else:
        print(f"    ! {snake} is on no pedal -- add --pedal <colour>")
    if warnings:
        print("\n  WARNINGS:")
        for w in warnings:
            print(f"    ! {w}")
    print("=" * 64)

    if not apply:
        print("  DRY RUN — nothing written. Re-run with --apply to build.")
        return

    # ---------------- APPLY ----------------
    # Stage 2: module files (only if changed)
    write_if_changed(mod_h_path, h_text)
    write_if_changed(mod_cpp_path, cpp_text)

    # Stage 3: export copy (only if changed)
    copy_if_changed(export_cpp_h, dest_cpp_h)

    def copy_runtime():
        if not os.path.isdir(export_common):
            print("  WARNING: export has no common/ — cannot refresh runtime.")
            return False
        os.makedirs(dest_common, exist_ok=True)
        n = 0
        for root, _dirs, files in os.walk(export_common):
            rel = os.path.relpath(root, export_common)
            tgt = os.path.join(dest_common, rel) if rel != "." else dest_common
            os.makedirs(tgt, exist_ok=True)
            for fn in files:
                if fn == ALLOCATOR:
                    continue
                if copy_if_changed(os.path.join(root, fn), os.path.join(tgt, fn)):
                    n += 1
        print(f"  runtime: {n} file(s) updated")
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

    # Stage 4: loaded_effects.h
    if le_new != le_lines:
        write_lines(le_path, le_new)
        print("  updated loaded_effects.h")

    # Stage 5: pedal lists (only if changed)
    final = {}
    for p, eff, note in pedal_changes:
        final[p] = eff
        print(f"  {note}")
    for p, eff in final.items():
        os.makedirs(os.path.join(project_root, "pedals"), exist_ok=True)
        pool = fast_pool_kb if p in on_pedals else None
        write_if_changed(pedal_path(project_root, p), pedal_text(project_root, p, eff, pool))

    print("\n  DONE. Next:")
    if not has_block or old_mk:
        print("    - Makefile: paste the pedal block / delete the lines flagged above")
    for p in on_after:
        print(f"    make PEDAL={p}            then: make PEDAL={p} program-dfu")
    if not on_after:
        print(f"    put {snake} on a pedal: re-run with --pedal <colour>")


if __name__ == "__main__":
    main()
