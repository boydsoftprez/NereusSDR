// no-port-check: NereusSDR-original test observability; no DSP logic copied.
// Copies the TX EQ's current impulse under the same lock its public setters
// hold while they rebuild it.
#include "comm.h"

int nereus_copy_txa_eq_impulse(int channel, double* impulse, int capacity)
{
    EQP eq = txa[channel].eqp.p;
    if (!eq || !impulse || capacity < 2 * eq->nc) {
        return 0;
    }
    EnterCriticalSection(&eq->csEQ);
    const int count = 2 * eq->nc;
    memcpy(impulse, eq->impulse, (size_t)count * sizeof(double));
    LeaveCriticalSection(&eq->csEQ);
    return count;
}

double nereus_txa_eq_samplerate(int channel)
{
    EQP eq = txa[channel].eqp.p;
    return eq ? eq->samplerate : 0.0;
}
