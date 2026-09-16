#include "freezeverb_module.h"
#include <string.h> // [STEP3] strcmp for param-name discovery

// RNBO lives ONLY in this .cpp, never in the header, so it never shares a
// translation unit with the `q` library (which defines clashing global
// fast-math symbols). This is the whole reason for the PIMPL split.
//
// Build defines (Makefile C_DEFS): -DRNBO_USE_FLOAT32 -DRNBO_NOTHROW -DRNBO_NOSTL
//   FLOAT32 -> RNBO::SampleValue == float, matches kshep/DaisySP (no conversion)
//   NOTHROW/NOSTL -> RNBO error path doesn't use C++ exceptions (kshep builds
//                    with exceptions disabled)
#include "freezeverb_cpp.h"

using namespace bkshepherd;

// ---------------------------------------------------------------------------
// PIMPL: the concrete RNBO object and its non-interleaved buffer pointers.
// FreezeVerb<> uses the default INTERNALENGINE, which it constructs itself.
// ---------------------------------------------------------------------------
struct FreezeVerbModule::Impl {
    RNBO::FreezeVerb<> rnbo;
    RNBO::SampleValue *in[2];
    RNBO::SampleValue *out[2];
};

// ---------------------------------------------------------------------------
// Parameter metadata. 3 params, mapped to knobs 0/1/2. No MIDI CC yet (-1).
// ---------------------------------------------------------------------------
static const ParameterMetaData s_metaData[FreezeVerbModule::PARAM_COUNT] = {
    {
        name : "Time",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 0,
        midiCCMapping : -1
    },
    {
        name : "Amount",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.0f},
        knobMapping : 1,
        midiCCMapping : -1
    },
    {
        name : "LP",
        valueType : ParameterValueType::Float,
        valueBinCount : 0,
        defaultValue : {.float_value = 0.5f},
        knobMapping : 2,
        midiCCMapping : -1
    }
};

FreezeVerbModule::FreezeVerbModule() : BaseEffectModule(), m_bufferIndex(0), m_impl(nullptr) {
    m_name = "FreezeVerb";

    m_paramMetaData = s_metaData;
    InitParams(FreezeVerbModule::PARAM_COUNT);

    // NOTE: We do NOT allocate/construct the RNBO object here. Module
    // constructors run very early (in the effectList[], before hardware/heap
    // is fully up), and constructing the large RNBO patch that early can fault
    // on bare metal. All RNBO setup is deferred to Init(), which runs after
    // the hardware is initialized.
}

FreezeVerbModule::~FreezeVerbModule() {
    delete m_impl;
}

void FreezeVerbModule::Init(float sample_rate) {
    BaseEffectModule::Init(sample_rate);

    m_bufferIndex = 0;
    for (int i = 0; i < kBlockSize; ++i) {
        m_inL[i] = 0.0f;
        m_inR[i] = 0.0f;
        m_outL[i] = 0.0f;
        m_outR[i] = 0.0f;
    }

    // Full setup. With the SDRAM-backed custom allocator (rnbo_allocator.cpp +
    // -DRNBO_USECUSTOMALLOCATOR), initialize()'s delay-buffer allocations now
    // come from SDRAM instead of the tiny DTCMRAM heap.
    if (m_impl == nullptr) {
        m_impl = new Impl();
        m_impl->in[0] = m_inL;
        m_impl->in[1] = m_inR;
        m_impl->out[0] = m_outL;
        m_impl->out[1] = m_outR;
    }

    m_impl->rnbo.initialize();
    m_impl->rnbo.prepareToProcess(sample_rate, kBlockSize, true);

    // [STEP3] Self-discovery: scan the RNBO object's parameters BY NAME and
    // cache the indices we care about. This is why the wrapper no longer needs
    // hardcoded param indices — whatever order the export puts them in, we
    // find them by name. Reserved names (fsw1/fsw2/led1/led2) drive footswitches
    // and LEDs; the 3 knob names map to the kshep knob slots.
    m_fsw1Index = m_fsw2Index = m_led1Index = m_led2Index = -1;
    m_reverbTimeIndex = m_amountIndex = m_lpIndex = -1;
    const RNBO::ParameterIndex n = m_impl->rnbo.getNumParameters();
    for (RNBO::ParameterIndex i = 0; i < n; ++i) {
        const char *pname = m_impl->rnbo.getParameterName(i);
        if (pname == nullptr) {
            continue;
        }
        if (strcmp(pname, "fsw1") == 0) {
            m_fsw1Index = (int)i;
        } else if (strcmp(pname, "fsw2") == 0) {
            m_fsw2Index = (int)i;
        } else if (strcmp(pname, "led1") == 0) {
            m_led1Index = (int)i;
        } else if (strcmp(pname, "led2") == 0) {
            m_led2Index = (int)i;
        } else if (strcmp(pname, "reverb_time") == 0) {
            m_reverbTimeIndex = (int)i;
        } else if (strcmp(pname, "amount") == 0) {
            m_amountIndex = (int)i;
        } else if (strcmp(pname, "lp") == 0) {
            m_lpIndex = (int)i;
        }
    }

    // Push initial knob values into the discovered indices (guard -1 in case a
    // name is missing from the export).
    if (m_reverbTimeIndex >= 0) {
        m_impl->rnbo.setParameterValue(m_reverbTimeIndex, GetParameterAsFloat(REVERB_TIME), RNBO::TimeNow);
    }
    if (m_amountIndex >= 0) {
        m_impl->rnbo.setParameterValue(m_amountIndex, GetParameterAsFloat(AMOUNT), RNBO::TimeNow);
    }
    if (m_lpIndex >= 0) {
        m_impl->rnbo.setParameterValue(m_lpIndex, GetParameterAsFloat(LP), RNBO::TimeNow);
    }
}

// ---------------------------------------------------------------------------
// INNER ENGINE SLOT: run RNBO on the collected 48-sample stereo block.
// ---------------------------------------------------------------------------
void FreezeVerbModule::ProcessAudioBlock(int size) {
    // [STEP3] Knob params pushed via discovered indices (was hardcoded enum).
    if (m_reverbTimeIndex >= 0) {
        m_impl->rnbo.setParameterValue(m_reverbTimeIndex, GetParameterAsFloat(REVERB_TIME), RNBO::TimeNow);
    }
    if (m_amountIndex >= 0) {
        m_impl->rnbo.setParameterValue(m_amountIndex, GetParameterAsFloat(AMOUNT), RNBO::TimeNow);
    }
    if (m_lpIndex >= 0) {
        m_impl->rnbo.setParameterValue(m_lpIndex, GetParameterAsFloat(LP), RNBO::TimeNow);
    }

    // [STEP3] Footswitch state (set from the main loop via SetFootswitch) pushed
    // into the discovered fsw1/fsw2 params here, in the audio thread.
    if (m_fsw1Index >= 0) {
        m_impl->rnbo.setParameterValue(m_fsw1Index, m_fsw1Value, RNBO::TimeNow);
    }
    if (m_fsw2Index >= 0) {
        m_impl->rnbo.setParameterValue(m_fsw2Index, m_fsw2Value, RNBO::TimeNow);
    }

    m_impl->rnbo.process(m_impl->in, 2, m_impl->out, 2, (RNBO::Index)size);

    // [STEP3] Cache LED values the patch computed, for the const LED getter.
    if (m_led1Index >= 0) {
        m_led1Value = (float)m_impl->rnbo.getParameterValue(m_led1Index);
    }
    if (m_led2Index >= 0) {
        m_led2Value = (float)m_impl->rnbo.getParameterValue(m_led2Index);
    }
}

// ---------------------------------------------------------------------------
// [STEP3] Footswitch input: called from the main loop (guitar_pedal.cpp). We
// just store the raw value; ProcessAudioBlock pushes it into RNBO next block.
// A float store is atomic on the M7 and one block of staleness (~1ms) is fine.
// ---------------------------------------------------------------------------
void FreezeVerbModule::SetFootswitch(int fsw_id, float value) {
    if (fsw_id == 0) {
        m_fsw1Value = value;
    } else if (fsw_id == 1) {
        m_fsw2Value = value;
    }
}

// ---------------------------------------------------------------------------
// The bridge: per-sample in, block process, per-sample out (one block latency).
// ---------------------------------------------------------------------------
void FreezeVerbModule::ProcessStereo(float inL, float inR) {
    // If RNBO isn't set up yet (Init not called), pass audio through cleanly.
    if (m_impl == nullptr) {
        m_audioLeft = inL;
        m_audioRight = inR;
        return;
    }

    m_inL[m_bufferIndex] = inL;
    m_inR[m_bufferIndex] = inR;

    m_audioLeft = m_outL[m_bufferIndex];
    m_audioRight = m_outR[m_bufferIndex];

    ++m_bufferIndex;
    if (m_bufferIndex >= kBlockSize) {
        ProcessAudioBlock(kBlockSize);
        m_bufferIndex = 0;
    }
}

void FreezeVerbModule::ProcessMono(float in) {
    ProcessStereo(in, in);
}

// ---------------------------------------------------------------------------
// [STEP3] LED: brightness now comes from the RNBO patch itself, via the
// discovered led1/led2 params. The patch decides what the LEDs mean (e.g.
// led1 = freeze indicator). We read the LIVE value with getParameterValue so
// it reflects patch-internal changes. Straight map for now: led1 -> physical
// left (led_id 0), led2 -> physical right (led_id 1). The physical wiring is
// reversed on the user's Terrarium; that gets fixed centrally in step 6.
// ---------------------------------------------------------------------------
float FreezeVerbModule::GetBrightnessForLED(int led_id) const {
    if (m_impl != nullptr) {
        if (led_id == 0 && m_led1Index >= 0) {
            return m_led1Value;
        }
        if (led_id == 1 && m_led2Index >= 0) {
            return m_led2Value;
        }
    }
    return BaseEffectModule::GetBrightnessForLED(led_id);
}
