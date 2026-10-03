// no-port-check: NereusSDR-original test observability; no DSP logic copied.
// Inspect the actual state written by WDSP setters, using their locks.
#include "comm.h"
// WDSP exports this in siphon.c; its public headers omit the declaration.
extern void TXASetSipMode(int channel, int mode);

// This model fixture has no spectrum window/analyzer owner. Keep its
// unused display siphon in buffered mode, leaving TX DSP and gen1 active.
void nereus_issue256_disable_display_siphon(int channel)
{
    TXASetSipMode(channel, 0);
}

// gen.c SetTXAPostGenToneMag uses the channel DSP lock.
double nereus_issue256_tone_magnitude(int channel)
{
    double value = -1.0;
    EnterCriticalSection(&ch[channel].csDSP);
    if (txa[channel].gen1.p) {
        value = txa[channel].gen1.p->tone.mag;
    }
    LeaveCriticalSection(&ch[channel].csDSP);
    return value;
}
