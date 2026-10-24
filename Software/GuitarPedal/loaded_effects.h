// Edit the contents of this file to populate the available effects that you
// want to use

#ifndef LOADED_EFFECTS_H
#define LOADED_EFFECTS_H
#pragma once

#include "Effect-Modules/base_effect_module.h"

// Include all effect modules
//#include "Effect-Modules/autopan_module.h"
//#include "Effect-Modules/chopper_module.h"
//#include "Effect-Modules/chorus_module.h"
//#include "Effect-Modules/cloudseed_module.h" // Takes up significant SDRAM (about 30%)
//#include "Effect-Modules/compressor_module.h"
//#include "Effect-Modules/crusher_module.h"
//#include "Effect-Modules/delay_module.h"
//#include "Effect-Modules/distortion_module.h"
//#include "Effect-Modules/drum_module.h"
//#include "Effect-Modules/effect_chain.h" // Some caution required - See README and examples below for usage
//#include "Effect-Modules/flanger_module.h"
//#include "Effect-Modules/geq_module.h"
//#include "Effect-Modules/granulardelay_module.h"
//#include "Effect-Modules/harmonic_tremolo_module.h"
//#include "Effect-Modules/ir_module.h"
//#include "Effect-Modules/looper_module.h"
//#include "Effect-Modules/metro_module.h"
//#include "Effect-Modules/modulated_tremolo_module.h"
//#include "Effect-Modules/multi_delay_module.h"
//#include "Effect-Modules/nam_a2_module.h"
//#include "Effect-Modules/noise_gate_module.h"
//#include "Effect-Modules/overdrive_module.h"
//#include "Effect-Modules/peq_module.h"
//#include "Effect-Modules/phaser_module.h"
//#include "Effect-Modules/pitch_shifter_module.h"
//#include "Effect-Modules/polyoctave_module.h"
//#include "Effect-Modules/reverb_module.h"
//#include "Effect-Modules/scifi_module.h"
//#include "Effect-Modules/spectral_delay_module.h"
//#include "Effect-Modules/tape_delay_module.h"
//#include "Effect-Modules/tuner_module.h"

// Keyboard modules
// #include "Effect-Modules/fm_keys_module.h"
// #include "Effect-Modules/midi_keys_module.h"
// #include "Effect-Modules/modal_keys_module.h"
// #include "Effect-Modules/pluckecho_module.h"
// #include "Effect-Modules/string_keys_module.h"

// Dattorro plate reverb - GPL-3.0-or-later, disabled by default. See
// Effect-Modules/Dattorro/README.md and the matching block in the Makefile
// before uncommenting.
// #include "Effect-Modules/dattorro_reverb_module.h"
#ifdef FX_freezeverb
#include "Effect-Modules/freezeverb_module.h"
#endif
#ifdef FX_sympath
#include "Effect-Modules/sympath_module.h"
#endif
//#include "Effect-Modules/scrub_module.h"
//#include "Effect-Modules/spectralscrub_module.h"
#ifdef FX_graincloud
#include "Effect-Modules/graincloud_module.h"
#endif
#ifdef FX_tension_relief
#include "Effect-Modules/tension_relief_module.h"
#endif
#ifdef FX_tens_rel
#include "Effect-Modules/tens_rel_module.h"
#endif
#ifdef FX_chord_shadow
#include "Effect-Modules/chord_shadow_module.h"
#endif
#ifdef FX_chordshadow
#include "Effect-Modules/chordshadow_module.h"
#endif
#ifdef FX_evolver
#include "Effect-Modules/evolver_module.h"
#endif
//#include "Effect-Modules/centroid_res_module.h"
//#include "Effect-Modules/tribonacci_module.h"
//#include "Effect-Modules/phrase_shuffle_module.h"

namespace bkshepherd {

void load_effects(int &availableEffectsCount, BaseEffectModule **&availableEffects) {
    // clang-format off
    static BaseEffectModule* effectList[] = {
        //new ModulatedTremoloModule(),
        //new OverdriveModule(),
        //new AutoPanModule(),
        //new ChorusModule(),
        //new ChopperModule(),
        //new ReverbModule(), // single-instance only
        //new MultiDelayModule(),  // single-instance-only
        //new MetroModule(),
        //new TunerModule(), // single-instance only
        //new PitchShifterModule(),  // single-instance-only
        //new CompressorModule(),
        //new LooperModule(),  // single-instance-only
        //new GraphicEQModule(),
        //new ParametricEQModule(),
        //new NoiseGateModule(),
        //new CloudSeedModule(), // single-instance only
        //new DelayModule(), // single-instance only
        //new TapeDelayModule(),  // single-instance-only
        //new NamA2Module(),  // single-instance-only
        //new SciFiModule(),  // single-instance-only
        //new PolyOctaveModule(),
        //new SpectralDelayModule(),  // single-instance-only
        //new DistortionModule(),
        //new GranularDelayModule(),  // single-instance-only
        //new IrModule(),
        //new DrumModule(),  // This module can be used with MIDI keyboard as a drum machine
        //new PhaserModule(),
        //new FlangerModule(),
        //new CrusherModule(),
        //new HarmonicTremoloModule(),
#ifdef FX_freezeverb
        new FreezeVerbModule(),
#endif
#ifdef FX_sympath
        new SympathModule(),
#endif
        //new scrubModule(),
        //new SpectralScrubModule(),
#ifdef FX_graincloud
        new graincloudModule(),
#endif
#ifdef FX_tension_relief
        new tension_reliefModule(),
#endif
#ifdef FX_tens_rel
        new tens_relModule(),
#endif
#ifdef FX_chord_shadow
        new chord_shadowModule(),
#endif
#ifdef FX_chordshadow
        new chordshadowModule(),
#endif
#ifdef FX_evolver
        new evolverModule(),
#endif
        //new cent_resoModule(),
        //new tribonacciModule(),
        //new phrase_shufModule(),

        // The following require a MIDI keyboard
        // new MidiKeysModule(),
        // new PluckEchoModule(),  // single-instance-only
        // new StringKeysModule(),
        // new ModalKeysModule(),
        // new FmKeysModule(),

        // GPL-3.0-or-later - see Effect-Modules/Dattorro/README.md
        // new DattorroReverbModule(),

        // Effects chain examples

        // Combines two or more effects into one, processed serially. Note that
        // many modules cannot be used more than once - see README.md.
        // In this case, the ReverbModule is single-instance only, so it cannot
        // be used standalone if it is used here.

        // new EffectChain(
        //     "TremVerb",
        //     // Slots: a short menu tag plus the child effect instance (owned by the chain).
        //     {{"Tr", new ModulatedTremoloModule()}, {"Rv", new ReverbModule()}},
        //     // Knob/MIDI CC mappings: {slot, child param id, knob (-1 = none), midi CC (-1 = none)}.
        //     {{0, ModulatedTremoloModule::DEPTH,    0, 20},
        //      {0, ModulatedTremoloModule::FREQ,     1, 21},
        //      {0, ModulatedTremoloModule::WAVE,     2, 22},

        //      {1, ReverbModule::TIME,               3, 23},
        //      {1, ReverbModule::DAMP,               4, 24},
        //      {1, ReverbModule::MIX,                5, 25},
             
        //      // Shift bank - hold the alternate footswitch for 1s to reach these.
        //      {0, ModulatedTremoloModule::OSC_WAVE, 6, 26},
        //      {0, ModulatedTremoloModule::OSC_FREQ, 7, 27}}
        // ),

        // Example of the alternate footswitch's toggle mode: instead of tap
        // tempo, pressing it toggles the tremolo off/on (and leaves the
        // reverb and delay always-on). Change footswitchTogglesSlots to {0, 1}
        // to toggle tremolo+delay together, or to {} to give the footswitch
        // back to the delay for tap tempo instead.
        // Note that DelayModule is single-instance-only so cannot be used
        // standalone if it is used here.

        // new EffectChain(
        //     "Ambience",
        //     {{"HT", new HarmonicTremoloModule()}, {"Dl", new DelayModule()}, {"Rv", new DattorroReverbModule()}},
        //     // Knob/MIDI CC mappings: {slot, child param id, knob (-1 = none), midi CC (-1 = none)}.
        //     {{2, DattorroReverbModule::MIX,       0, 20},
        //      {0, HarmonicTremoloModule::DEPTH,    1, 21},
        //      {0, HarmonicTremoloModule::SPEED,    2, 22},

        //      {1, DelayModule::DELAY_MIX,          3, 23},
        //      {1, DelayModule::DELAY_TIME,         4, 24},
        //      {1, DelayModule::D_FEEDBACK,         5, 25},

        //      // Shift bank - hold the alternate footswitch for 1s to reach these.
        //      {2, DattorroReverbModule::TONE,      6, 26},
        //      {2, DattorroReverbModule::DECAY,     7, 27},
        //      {2, DattorroReverbModule::SIZE,      8, 28},
             
        //      {1, DelayModule::D_SPREAD,            9, 29},
        //      {1, DelayModule::MOD_AMT,          10, 30},
        //      {1, DelayModule::MOD_RATE,            11, 31}},
        //     /* primarySlot */ 1, // delay owns the LED
        //     /* footswitchTogglesSlots */ {0} // but footswitch toggles tremolo on/off
        // ),
    };
    static_assert(sizeof(effectList) > 0, "This pedal's effect list is empty: no effect in its pedals/<colour>.mk is wired into loaded_effects.h. Run make_effect.py for it.");
    // clang-format on

    availableEffectsCount = sizeof(effectList) / sizeof(effectList[0]);
    availableEffects = effectList;
}

} // namespace bkshepherd

#endif
