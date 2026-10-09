// no-port-check: NereusSDR-original test observability; no DSP logic copied.
#include "comm.h"

// dexp.c SetAntiVOXRun/SetAntiVOXDetectorTau use cs_update.
double nereus_issue299_antivox_tau(int dexp_id)
{
    DEXP detector = pdexp[dexp_id];
    if (!detector) {
        return -1.0;
    }
    EnterCriticalSection(&detector->cs_update);
    const double value = detector->antivox_tau;
    LeaveCriticalSection(&detector->cs_update);
    return value;
}

int nereus_issue299_antivox_running(int dexp_id)
{
    DEXP detector = pdexp[dexp_id];
    if (!detector) {
        return -1;
    }
    EnterCriticalSection(&detector->cs_update);
    const int value = detector->antivox_run;
    LeaveCriticalSection(&detector->cs_update);
    return value;
}
