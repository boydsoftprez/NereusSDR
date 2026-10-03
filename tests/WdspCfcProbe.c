// no-port-check: NereusSDR-original test observability; no DSP logic copied.
// Reads the real CFC profile under the same lock used by its public setters.
#include "comm.h"

int nereus_copy_cfc_profile(int channel, double* compression, double* post_eq,
                            int capacity)
{
    CFCOMP profile = txa[channel].cfcomp.p;
    if (!profile || !compression || !post_eq || capacity < profile->msize) {
        return 0;
    }
    EnterCriticalSection(&ch[channel].csDSP);
    const int count = profile->msize;
    memcpy(compression, profile->comp, (size_t)count * sizeof(double));
    memcpy(post_eq, profile->peq, (size_t)count * sizeof(double));
    LeaveCriticalSection(&ch[channel].csDSP);
    return count;
}
