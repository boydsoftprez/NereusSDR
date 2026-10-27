// no-port-check: NereusSDR-original test observability; no DSP logic copied.
// Reads a WDSP channel's run flags (channel.h, iobuffs.h) for
// tst_wdsp_drain_timeout_flush: the exchange bit fexchange0 tests, the
// down-slew request SetChannelState(ch, 0, ...) raises, and the worker's
// exec_bypass.
#include "comm.h"

int nereus_channel_exchange(int channel)
{
    return (int)(_InterlockedAnd(&ch[channel].exchange, 1) != 0);
}

int nereus_channel_downflag(int channel)
{
    return (int)(_InterlockedAnd(&ch[channel].iob.pc->slew.downflag, 1) != 0);
}

int nereus_channel_exec_bypass(int channel)
{
    return (int)(_InterlockedAnd(&ch[channel].iob.pc->exec_bypass, 1) != 0);
}
