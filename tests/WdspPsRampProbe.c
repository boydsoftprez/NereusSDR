// no-port-check: NereusSDR-original test observability and RED cleanup.
// No calibration or correction logic is replaced. The IQC lock remains
// independent of CALCC's update lock, even in the production deadlock.
#include "comm.h"

// Public setter is defined in calcc.c; comm.h exposes internal CALCC only.
extern void SetPSControl(int channel, int reset, int mancal, int automode, int turnon);

int nereus_ps_iqc_state(int channel)
{
    int state;
    EnterCriticalSection(&ch[channel].csDSP);
    state = txa[channel].iqc.p->state;
    LeaveCriticalSection(&ch[channel].csDSP);
    return state;
}

void nereus_ps_release_parked_ramp(int channel)
{
    // An independent cleanup path, never the behavior under test: clear
    // IQC busy without taking the CALCC lock, so RED can finish and report.
    StopTXAiqcQuiescent(channel);
}

// Groups the actual native calls under CALCC's recursive lock so the calc
// worker cannot observe the transient suspension flag between Off and rearm.
// The production functions, ownership and epoch changes remain unchanged.
int nereus_ps_stop_and_rearm(int channel)
{
    int stopped;
    EnterCriticalSection(&txa[channel].calcc.cs_update);
    stopped = StopPSCorrectionQuiescent(channel);
    SetPSControl(channel, 1, 0, 1, 0);
    LeaveCriticalSection(&txa[channel].calcc.cs_update);
    return stopped;
}

// Flush completion is stronger than channel state=0: the normal flush worker
// sets exec_bypass. SetChannelState's no-samples timeout does not set that bit.
int nereus_ps_native_quiescent(int channel)
{
    IOB io = ch[channel].iob.pc;
    int quiescent;
    EnterCriticalSection(&ch[channel].csDSP);
    EnterCriticalSection(&ch[channel].csEXCH);
    quiescent = ch[channel].state == 0 &&
        !_InterlockedAnd(&ch[channel].exchange, 1) &&
        !_InterlockedAnd(&ch[channel].flushflag, 1) &&
        !_InterlockedAnd(&io->slew.downflag, 1) &&
        _InterlockedAnd(&io->exec_bypass, 1);
    LeaveCriticalSection(&ch[channel].csEXCH);
    LeaveCriticalSection(&ch[channel].csDSP);
    return quiescent;
}
