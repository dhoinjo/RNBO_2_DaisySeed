// tempo_sync_v2.h   (install as: Util/tempo_sync.h)
// ---------------------------------------------------------------------------
// Shared tempo between pedals over MIDI, "last one wins" (Link-like).
//
//  - A pedal LEADS when its patch writes a new value > 0 to `tempo_out`.
//    It announces itself (3 CCs on channel 16) and sends MIDI clock
//    (24 ticks per beat) at that tempo. Writing 0 stops leading (no clock).
//  - Every other pedal FOLLOWS: it hears the announcement, stops leading if it
//    was, and locks onto the incoming ticks (drift-free).
//  - The patch's `bpm` param gets the knob/menu value OR the shared tempo,
//    whichever changed last:
//        a new announcement (from any pedal, this one included) -> shared tempo
//        the knob/menu value of `bpm` moved                      -> knob value
//    Small drift corrections while following don't count as a change.
//  - Clock gone (> 1 s no ticks): the last shared tempo is held.
//
// Messages (channel 16): CC 116 = tempo*10 high 7 bits, CC 117 = low 7 bits,
// CC 119 = pedal id (commits "this pedal leads now"). Plus MIDI clock (0xF8).
// Wire every pedal's MIDI out to the other pedals' inputs (directly or via a
// Midihub). Switch "Midi Thru" off on every pedal.
//
// Threads: TempoSyncOnRealtimeByte runs in the MIDI receive interrupt,
// TempoSyncAudioBlock in the audio callback, TempoSyncProcess in the main loop.
// ---------------------------------------------------------------------------
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifndef PEDAL_ID
#define PEDAL_ID 9
#endif

// Setup: send function for outgoing MIDI bytes (called from the main loop only).
void TempoSyncInit(void (*send)(uint8_t *bytes, size_t size));

// MIDI receive interrupt: every realtime byte (0xF8 clock etc.).
void TempoSyncOnRealtimeByte(uint8_t status);

// Main loop, for every incoming channel message. Returns true if it was a
// tempo-sync message (then don't pass it on / don't process it further).
bool TempoSyncHandleCC(int channel, int cc, int value);

// Main loop, every iteration: sends announcements and clock ticks when leading.
void TempoSyncProcess();

// Audio callback, once per callback, before the effect runs.
void TempoSyncAudioBlock(size_t size, float sampleRate);

// Wrapper (audio callback): the patch wrote a new tempo_out value.
// > 0: lead at this tempo.  <= 0: stop leading (if leading).
void TempoSyncRequestLead(float bpm);

// Wrapper (audio callback): value for the patch's `bpm` param this block.
// Returns false when the knob/menu value should be used (knobBpm as is).
bool TempoSyncBpmForPatch(float knobBpm, float *bpmOut);

// For display/debug: 0 idle, 1 leading, 2 following, 3 holding.
int TempoSyncState();
