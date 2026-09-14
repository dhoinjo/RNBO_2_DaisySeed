#pragma once
#ifndef FREEZEVERB_MODULE_H
#define FREEZEVERB_MODULE_H

#include <stdint.h>
#include "base_effect_module.h"

// NOTE: We deliberately do NOT include the RNBO export here.
// RNBO's RNBO_MathFast.h and the `q` library both define global fast-math
// symbols (fastersinfull, fasterexp, ...). If RNBO and q land in the same
// translation unit they collide. This header is pulled into loaded_effects.h
// (which also pulls in q-using modules), so the RNBO include must stay OUT of
// this header and live only in freezeverb_module.cpp. We forward-declare the
// RNBO type and hold it behind an opaque pointer (PIMPL).

#ifdef __cplusplus

// Forward declaration only - no RNBO headers here.
namespace RNBO {
    template <class ENGINE> class FreezeVerb;
}

namespace bkshepherd {

class FreezeVerbModule : public BaseEffectModule {
  public:
    // [STEP3] These are kshep-side KNOB SLOTS, not RNBO indices. The actual
    // RNBO param index for each is discovered by name at Init() (see
    // m_reverbTimeIndex etc.). The s_metaData table order below must match this
    // enum. (Option-b minimal proof: knobs stay hardcoded, fsw/led discovered.)
    enum Param {
        REVERB_TIME = 0,
        AMOUNT = 1,
        LP = 2,
        PARAM_COUNT
    };

    FreezeVerbModule();
    ~FreezeVerbModule();

    void Init(float sample_rate) override;
    void ProcessMono(float in) override;
    void ProcessStereo(float inL, float inR) override;
    float GetBrightnessForLED(int led_id) const override;
    // [STEP3] Receives raw footswitch state from the framework and forwards it
    // into the discovered fsw1/fsw2 RNBO params. fsw_id 0 -> fsw1, 1 -> fsw2.
    void SetFootswitch(int fsw_id, float value) override;

    // [STEP4.5] RNBO effects use switch 1 as a raw footswitch (into fsw1), not
    // as the framework bypass. So the pedal never bypass-toggles this effect;
    // on/off lives in the patch. Returns true.
    bool UsesRawFootswitch1() const override { return true; }

  private:
    static constexpr int kBlockSize = 48; // must match kshep hardware blockSize
                                          // (guitar_pedal.cpp: blockSize = 48)

    // [STEP3] Discovered RNBO param indices for the reserved-name params.
    // -1 means "not present in this export". Found by NAME at Init() by
    // scanning the RNBO object, so their physical index in the export doesn't
    // matter (in FreezeVerb they happen to be 0/2 for fsw, 1/3 for led).
    // This is the (b) minimal-proof version: the 3 knob params still use the
    // hardcoded enum below. The full generic version (all params discovered)
    // is the generator's job later.
    int m_fsw1Index = -1;
    int m_fsw2Index = -1;
    int m_led1Index = -1;
    int m_led2Index = -1;

    // RNBO indices for the 3 knob params, resolved by name at Init() too, so
    // the hardcoded REVERB_TIME/AMOUNT/LP enum (kshep-side slot order) maps to
    // whatever index RNBO gave them in this export.
    int m_reverbTimeIndex = -1;
    int m_amountIndex = -1;
    int m_lpIndex = -1;

    // [STEP3] Latest raw footswitch values, written by SetFootswitch() from the
    // main loop, read by ProcessAudioBlock() in the audio thread.
    volatile float m_fsw1Value = 0.0f;
    volatile float m_fsw2Value = 0.0f;

    // [STEP3] Cached LED values, read from RNBO in ProcessAudioBlock (audio
    // thread) and returned by the const GetBrightnessForLED (UI thread). We
    // cache because RNBO's getParameterValue is non-const and shouldn't be
    // called from the const LED getter; caching also keeps RNBO calls on one
    // thread.
    volatile float m_led1Value = 0.0f;
    volatile float m_led2Value = 0.0f;

    // --- sample<->block bridge state ---
    float m_inL[kBlockSize];
    float m_inR[kBlockSize];
    float m_outL[kBlockSize];
    float m_outR[kBlockSize];
    int m_bufferIndex;

    // --- RNBO engine, hidden behind an opaque pointer (PIMPL) ---
    // The concrete type lives only in the .cpp so RNBO headers never enter
    // this header (and thus never share a translation unit with q).
    struct Impl;
    Impl *m_impl;

    void ProcessAudioBlock(int size);
};

} // namespace bkshepherd
#endif
#endif
