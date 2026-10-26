// tempo_sync_v2.cpp   (install as: Util/tempo_sync.cpp)
// See tempo_sync.h. Util/*.cpp is compiled automatically.
#include "tempo_sync.h"
#include "daisy_seed.h"   // daisy::System::GetUs
#include <math.h>

using daisy::System;

namespace {

enum State { kIdle = 0, kLeading = 1, kFollowing = 2, kHolding = 3 };

const int kMidiChannel = 15;   // channel 16 (0-based 15)
const int kCcHi = 116, kCcLo = 117, kCcLead = 119;
const uint32_t kClockLostUs = 1000000;  // 1 s without ticks = clock gone
const int kWindow = 96;                 // ticks used to measure the tempo (4 beats)
const float kCorrection = 0.25f;        // drift correction strength (per beat of error)
const float kMaxNudge = 0.02f;          // never nudge the tempo by more than 2 %
const float kKnobMove = 0.5f;           // knob must move this many bpm to count (pot noise)

void (*s_send)(uint8_t *, size_t) = nullptr;

volatile int s_state = kIdle;

// --- leading (requested from the audio callback, acted on in the main loop)
volatile float s_leadBpm = 120.0f;
volatile bool s_announcePending = false;
bool s_tickClockRunning = false;
uint32_t s_leadSinceUs = 0;
// Leader ticks are counted on the AUDIO sample clock (same clock the leader's
// own patch runs on, so leader and followers can't drift apart) and sent from
// the main loop.
volatile uint32_t s_ticksDue = 0;
uint32_t s_ticksSent = 0;
double s_tickAcc = 0.0;

// --- following (written in the MIDI interrupt)
volatile uint32_t s_tickCount = 0;
volatile uint32_t s_lastTickUs = 0;
uint32_t s_tickTimes[kWindow + 1];
volatile int s_tickHead = 0;
volatile int s_tickFill = 0;
volatile float s_announcedBpm = 0.0f;   // from the leader's CCs, until ticks take over
int s_ccHi = -1;

// --- audio side
float s_currentBpm = 120.0f;    // what the patch gets this block
double s_modelBeats = 0.0;      // beats the patch has run through (integral of bpm)
uint32_t s_followTickBase = 0;  // tick count when following started
bool s_followInit = false;
float s_heldBpm = 120.0f;

// --- last one wins: shared tempo vs knob
volatile bool s_syncWins = false;   // true: patch gets the shared tempo
volatile bool s_knobRefValid = false;
float s_knobRef = 0.0f;             // knob value when the shared tempo last won

void SyncWins() {
    s_knobRefValid = false;
    s_syncWins = true;
}

void Send3(uint8_t a, uint8_t b, uint8_t c) {
    uint8_t m[3] = {a, b, c};
    if (s_send) s_send(m, 3);
}

float MeasuredBpm() {
    // Tempo from the last kWindow ticks (needs at least 24 = one beat).
    const int fill = s_tickFill;
    if (fill < 24) {
        return s_announcedBpm > 0.0f ? s_announcedBpm : 0.0f;
    }
    const int head = s_tickHead;
    const int n = fill - 1;
    const int newest = (head + kWindow) % (kWindow + 1);
    const int oldest = (head + kWindow + 1 - fill) % (kWindow + 1);
    const uint32_t span = s_tickTimes[newest] - s_tickTimes[oldest];
    if (span == 0) return s_announcedBpm;
    return 60.0e6f * (float)n / (24.0f * (float)span);
}

void ResetFollowWindow() {
    s_tickFill = 0;
    s_tickHead = 0;
    s_followInit = false;
}

} // namespace

void TempoSyncInit(void (*send)(uint8_t *bytes, size_t size)) { s_send = send; }

void TempoSyncOnRealtimeByte(uint8_t status) {
    if (status != 0xF8 || s_state == kLeading) {
        return;  // only clock ticks matter; a leader ignores incoming ticks
    }
    const uint32_t now = System::GetUs();
    if (s_tickFill > 0 && now - s_lastTickUs > 250000) {
        ResetFollowWindow();  // long gap: start measuring afresh
    }
    s_tickTimes[s_tickHead] = now;
    s_tickHead = (s_tickHead + 1) % (kWindow + 1);
    if (s_tickFill < kWindow + 1) s_tickFill = s_tickFill + 1;
    s_lastTickUs = now;
    s_tickCount = s_tickCount + 1;
    if (s_state != kFollowing) {
        s_state = kFollowing;   // a clock (re)starts: counts as a new tempo
        SyncWins();
    }
}

bool TempoSyncHandleCC(int channel, int cc, int value) {
    if (channel != kMidiChannel || (cc != kCcHi && cc != kCcLo && cc != kCcLead)) {
        return false;
    }
    if (cc == kCcHi) {
        s_ccHi = value;
    } else if (cc == kCcLo && s_ccHi >= 0) {
        s_announcedBpm = (float)((s_ccHi << 7) | value) / 10.0f;
    } else if (cc == kCcLead && value != PEDAL_ID) {
        // Another pedal takes the lead. Tie-break: if we claimed it in the last
        // 50 ms and have the higher id, we keep it.
        const bool justClaimed = s_state == kLeading && (System::GetUs() - s_leadSinceUs) < 50000;
        if (!(justClaimed && PEDAL_ID > value)) {
            s_state = kFollowing;
            s_tickClockRunning = false;
            ResetFollowWindow();
            SyncWins();
        }
    }
    return true;
}

void TempoSyncRequestLead(float bpm) {
    if (bpm <= 0.0f) {
        // Patch wrote 0: stop leading. Keep our tempo; others hold theirs.
        if (s_state == kLeading) {
            s_heldBpm = s_leadBpm;
            s_state = kHolding;
        }
        return;
    }
    if (bpm < 20.0f) bpm = 20.0f;
    if (bpm > 999.0f) bpm = 999.0f;
    s_leadBpm = bpm;
    s_currentBpm = bpm;   // patch gets the new tempo right away (same thread)
    s_heldBpm = bpm;
    s_announcePending = true;
    s_state = kLeading;
    SyncWins();
}

void TempoSyncProcess() {
    const uint32_t now = System::GetUs();

    if (s_state == kFollowing && s_tickFill > 0 && now - s_lastTickUs > kClockLostUs) {
        s_state = kHolding;  // leader gone: keep the last tempo
        ResetFollowWindow();
    }

    if (s_state != kLeading) {
        s_tickClockRunning = false;
        return;
    }

    if (s_announcePending) {
        s_announcePending = false;
        const int t = (int)(s_leadBpm * 10.0f + 0.5f);
        const uint8_t st = (uint8_t)(0xB0 | kMidiChannel);
        Send3(st, kCcHi, (uint8_t)((t >> 7) & 0x7F));
        Send3(st, kCcLo, (uint8_t)(t & 0x7F));
        Send3(st, kCcLead, (uint8_t)PEDAL_ID);
        s_leadSinceUs = now;
        if (!s_tickClockRunning) {
            s_tickClockRunning = true;
            s_ticksSent = s_ticksDue;  // start fresh, no backlog
        }
    }

    // Send the ticks the audio clock says are due. A late loop iteration sends
    // the missed ones right away, so the tick count never falls behind.
    int guard = 0;
    while (s_ticksSent != s_ticksDue && guard++ < 8) {
        uint8_t tick = 0xF8;
        if (s_send) s_send(&tick, 1);
        s_ticksSent++;
    }
}

void TempoSyncAudioBlock(size_t size, float sampleRate) {
    const int state = s_state;
    const double blockBeats = (double)s_currentBpm / 60.0 * (double)size / (double)sampleRate;

    if (state == kLeading) {
        s_tickAcc += (double)s_leadBpm * 24.0 / 60.0 * (double)size / (double)sampleRate;
        while (s_tickAcc >= 1.0) {
            s_tickAcc -= 1.0;
            s_ticksDue = s_ticksDue + 1;
        }
        s_currentBpm = s_leadBpm;
        s_heldBpm = s_currentBpm;
        s_followInit = false;
        return;
    }

    if (state == kFollowing) {
        const float measured = MeasuredBpm();
        if (measured <= 0.0f) {
            return;  // not enough ticks yet: keep the previous tempo
        }
        // Read tick count + time consistently (the MIDI interrupt may update them).
        uint32_t count, last;
        do {
            count = s_tickCount;
            last = s_lastTickUs;
        } while (count != s_tickCount);

        const double tickPeriodUs = 60.0e6 / ((double)measured * 24.0);
        double sinceTick = (double)(System::GetUs() - last) / tickPeriodUs;
        if (sinceTick > 1.0) sinceTick = 1.0;

        if (!s_followInit) {
            s_followInit = true;
            s_followTickBase = count;
            s_modelBeats = sinceTick / 24.0;
        } else {
            s_modelBeats += blockBeats;
        }
        const double masterBeats = ((double)(count - s_followTickBase) + sinceTick) / 24.0;
        float err = (float)(masterBeats - s_modelBeats);  // + = patch is behind
        float nudge = kCorrection * err;
        if (nudge > kMaxNudge) nudge = kMaxNudge;
        if (nudge < -kMaxNudge) nudge = -kMaxNudge;
        s_currentBpm = measured * (1.0f + nudge);
        s_heldBpm = measured;
        return;
    }

    if (state == kHolding) {
        s_currentBpm = s_heldBpm;
    }
}

bool TempoSyncBpmForPatch(float knobBpm, float *bpmOut) {
    if (!s_syncWins || s_state == kIdle) {
        return false;  // knob/menu value goes to the patch
    }
    if (!s_knobRefValid) {
        s_knobRef = knobBpm;
        s_knobRefValid = true;
    } else if (fabsf(knobBpm - s_knobRef) > kKnobMove) {
        s_syncWins = false;  // knob turned: knob wins until the next announcement
        return false;
    }
    *bpmOut = s_currentBpm;
    return true;
}

int TempoSyncState() { return s_state; }
