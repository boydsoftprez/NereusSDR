// no-port-check: NereusSDR-original WDSP scheduling glue. Not a port of
// Thetis or WDSP logic; it schedules the existing per-channel csDSP lock so
// control calls are not starved by a busy DSP worker, and lets channel
// teardown wait for the worker to leave its loop, and times each worker block
// so the application can see how loaded each channel is.

/*  dsplock.h

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2026 J.J. Boyd, KG4VCF (NereusSDR-original scheduling glue)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

boydsoftprez@gmail.com

*/

// =================================================================
// third_party/wdsp/src/dsplock.h (NereusSDR)
// =================================================================
//
// Fair turn-taking for each channel's csDSP lock. comm.h redirects every
// WDSP EnterCriticalSection call to WdspEnterCS. A thread entering a
// channel's csDSP announces itself while it waits; before the channel's
// worker retakes csDSP for its next block it holds off, for a bounded time,
// while announced waiters exist. Every other lock goes straight to the
// platform call.
//
// Channel teardown (pre_main_destroy) waits for the channel's worker to
// signal that it has left its loop before any buffer is freed.
//
// Each worker block (csDSP acquired to csDSP released) is timed with a
// monotonic clock; per-channel cumulative counters are written only by the
// worker and read, without csDSP, through GetChannelDspLoad.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23 - Created by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code (R-R3-39).
//   2026-09-23 - Worker-exit signal and WdspWaitWorkerExit added by
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code (R-R3-39).
//   2026-09-23 - Per-channel block timing and GetChannelDspLoad added by
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code (R-R3-40).
//   2026-09-23 - Test-only process delay (WdspWorkerTestProcessDelay,
//                 WDSPSetTestProcessDelayUs) added by J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code
//                 (R-R3-39).
//   2026-09-23 - Block in progress (currentBlockNs), block period published
//                 at block start, and the per-interval longest block
//                 (TakeChannelDspIntervalMaxBlockUs) added by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code (R-R3-40).
//   2026-09-23 - Worker start result (WdspWorkerStarted): teardown of a
//                 channel whose worker never started logs once and does not
//                 wait, by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code (R-R3-39).
//   2026-09-23 - Test-only WDSPGetTestLastWorkerExitWaitUs added by J.J.
//                 Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code (R-R3-39).
//   2026-09-23 - WdspChannelLoad::readNs, busyNs and currentBlockNs read as
//                 one instant's pair, and the test-only periodic delay
//                 (WDSPSetTestPeriodicDelayUs) added by J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code
//                 (R-R3-40, R-R3-37).
//   2026-09-23 - GetChannelDspLoad's return 1 (a read that may be torn)
//                 and the test-only WDSPSetTestHoldLoadPair documented by
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code (R-R3-40).
//   2026-09-24 - Caller check (WDSPSetCallerCheckHook, WdspCallerCheck and
//                 the WDSP_CALLER_* kinds) added by J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code
//                 (R-R3-39).
//   2026-09-25 - WDSP_CALLER_RESAMPLE_FV: resample.c's float resampler calls
//                 (create_resampleFV, xresampleFV, destroy_resampleFV) report
//                 to the caller check, by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code
//                 (R-R3-39).
//   2026-09-25 - Test-only WDSPSetTestBlockHook declared by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code (R-R3-40).
//   2026-09-29 - Test-only WDSPGetTestWorkerPauseNs declared by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code (R-R3-39).
//   2026-10-01 - Test-only exchange hook (WDSPSetTestExchangeHook,
//                 WdspTestExchangeHook): iobuffs.c's dexchange calls it after
//                 releasing Sem_OutReady, so a test holds the worker there
//                 while the caller runs ahead. One pointer test when none is
//                 installed. By J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#ifndef _dsplock_h
#define _dsplock_h

#include "comm.h"

// Target of comm.h's EnterCriticalSection redirect.
void WdspEnterCS (LPCRITICAL_SECTION cs);

// Caller check (R-R3-39). The application may install one function that
// hears every WDSP entry point listed below, on the calling thread, before
// the call does anything else; NereusSDR's debug and test builds use it to
// count WDSP calls still made from the event loop. The hook must not block,
// take a WDSP lock or call into WDSP. With no hook installed each check is
// one pointer load and one null test. Kinds (the second argument; the
// first is the channel, -1 when the lock is not a channel's csDSP):
// src/core/wdsp_api.h mirrors these values as kWdspCaller*.
enum
{
	WDSP_CALLER_ENTER_CS = 1,           // WdspEnterCS: any WDSP lock entry
	WDSP_CALLER_WAIT_WORKER_EXIT = 2,   // WdspWaitWorkerExit: channel teardown
	WDSP_CALLER_OPEN_CHANNEL = 3,       // OpenChannel (channel.c)
	WDSP_CALLER_SET_CHANNEL_STATE = 4,  // SetChannelState (channel.c)
	WDSP_CALLER_RESAMPLE_FV = 5         // create/x/destroy_resampleFV (resample.c)
};

typedef void (*WdspCallerCheckHook) (int channel, int kind);

// Installs (or, with 0, removes) the caller check. Any thread.
PORT void WDSPSetCallerCheckHook (WdspCallerCheckHook hook);

// The installed hook (0 = none); written only by WDSPSetCallerCheckHook.
extern WdspCallerCheckHook volatile wdsp_caller_check_hook;

static __inline void WdspCallerCheck (int channel, int kind)
{
#ifdef _WIN32
	const WdspCallerCheckHook hook = wdsp_caller_check_hook;
#else
	const WdspCallerCheckHook hook = __atomic_load_n (&wdsp_caller_check_hook, __ATOMIC_ACQUIRE);
#endif
	if (hook != 0)
	{
		hook (channel, kind);
	}
}

// The channel worker's csDSP acquire and release for one block (main.c).
void WdspWorkerEnter (int channel);
void WdspWorkerLeave (int channel);

// The channel worker calls this once, after its loop ends (main.c).
void WdspWorkerExited (int channel);

// start_thread (channel.c) reports whether it started the channel's worker
// (started != 0). A teardown of a channel whose worker never started logs
// one line and returns without waiting, since no exit will ever come.
void WdspWorkerStarted (int channel, int started);

// The channel worker calls this at the start of a block it processes, after
// its exec_bypass check (main.c). It runs the test-only process delay set by
// WDSPSetTestProcessDelayUs and the test-only periodic delay set by
// WDSPSetTestPeriodicDelayUs, and otherwise returns after two relaxed loads.
void WdspWorkerTestProcessDelay (int channel);

// Channel teardown (pre_main_destroy): returns once the channel's worker has
// left its loop. Never gives up while the worker is still inside a block;
// writes a dprintf line for every kWorkerExitLogIntervalMs of waiting.
// Returns at once, after one dprintf line, if the worker never started.
void WdspWaitWorkerExit (int channel);

// One channel's worker load since the process started. Every field only
// grows except blockPeriodUs, which is the block period (dsp_size / dsp_rate)
// of the worker's latest or current block (0 before its first block), and
// currentBlockNs.
//   blocks         - worker blocks completed
//   busyNs         - total time the worker held csDSP for those blocks
//   lateBlocks     - blocks that took longer than their block period
//   maxBlockUs     - longest single block
//   currentBlockNs - how long the block in progress has run so far, 0 when
//                    the worker is not inside a block
//   readNs         - when this read was taken, on the monotonic clock the
//                    worker times its blocks with; the same clock read gives
//                    currentBlockNs
// busyNs + currentBlockNs is the worker's total time inside blocks up to
// readNs, so between two reads the change in that sum over the change in
// readNs is the share of wall time the worker spent inside blocks.
// src/core/wdsp_api.h declares the same struct; the guard lets a file include
// both headers.
#ifndef NEREUS_WDSP_CHANNEL_LOAD_DEFINED
#define NEREUS_WDSP_CHANNEL_LOAD_DEFINED
typedef struct
{
	long long blocks;
	long long busyNs;
	long long lateBlocks;
	long long maxBlockUs;
	int blockPeriodUs;
	long long currentBlockNs;
	long long readNs;
} WdspChannelLoad;
#endif

// Copies the channel's load counters into *out without taking csDSP, so it
// never waits for the worker. Returns 0 on success, -1 for an invalid
// channel or a null out, and 1 when the worker kept busyNs and the block in
// progress changing through every attempt: *out is filled, but busyNs,
// currentBlockNs and readNs may not be one instant's values, so the caller
// skips this read. busyNs, currentBlockNs and readNs are one instant's
// values: a block that completes during the read is counted in exactly one
// of busyNs and currentBlockNs. The other fields are read one by one, so such
// a block may be counted in some of them and not others.
PORT int GetChannelDspLoad (int channel, WdspChannelLoad* out);

// Returns the longest block (microseconds) the channel's worker completed
// since the previous call, and starts the next interval (0 if no block
// completed). One periodic reader owns this (NereusSDR's RadioModel load
// sampler); a second caller would split its intervals. Returns -1 for an
// invalid channel. Never takes csDSP.
PORT long long TakeChannelDspIntervalMaxBlockUs (int channel);

// Test-only: busy-wait this many microseconds inside the worker's locked
// section on every block, to simulate an overloaded DSP chain. Default 0
// (off); when off the worker pays one relaxed load per block. Not for
// production use.
PORT void WDSPSetTestBlockDelayUs (int channel, int microseconds);

// Test-only: like WDSPSetTestBlockDelayUs, but the busy-wait runs inside a
// block the worker processes, after its exec_bypass check and before
// dexchange, so a teardown that lands during the delay meets real DSP work.
// Default 0 (off); when off the worker pays one relaxed load per processed
// block. Not for production use.
PORT void WDSPSetTestProcessDelayUs (int channel, int microseconds);

// Test-only: like WDSPSetTestProcessDelayUs, but the busy-wait runs only in
// every everyBlocks-th block the worker processes, like a stage that works in
// frames longer than one block (neural noise reduction: a 768-sample hop at
// 48 kHz is one heavy block in 12 at a 64-sample buffer). Runs after the
// process delay, if both are set. 0 for either value turns it off. Not for
// production use.
PORT void WDSPSetTestPeriodicDelayUs (int channel, int microseconds, int everyBlocks);

// Test-only: while hold is nonzero every GetChannelDspLoad read of the
// channel finds its load pair changing (as if the worker were always mid
// update) and returns 1; hold 0 releases it. Never call it in production
// code.
PORT void WDSPSetTestHoldLoadPair (int channel, int hold);

// Test-only: install (or, with 0, remove) a function every channel worker
// calls on itself at each block's start (endNs 0) and end, with the block's
// start and end on the monotonic clock it times blocks with (the clock
// GetChannelDspLoad's readNs uses). A test measures the worker's busy share
// from it without the load counters. Never call it in production code.
PORT void WDSPSetTestBlockHook (void (*hook) (int channel, long long startNs, long long endNs));

// Test-only: install (or, with 0, remove) a function iobuffs.c's dexchange
// calls on the channel worker right after it releases Sem_OutReady (the
// tokens that let fexchange0's caller run on). A test that blocks in it holds
// the worker there while the caller runs ahead, which proves the input chunk
// was already copied out. The hook must not call into WDSP. Never call it in
// production code.
typedef void (*WdspTestExchangeHookFn) (int channel);
PORT void WDSPSetTestExchangeHook (WdspTestExchangeHookFn hook);

// The installed exchange hook (0 = none); written only by
// WDSPSetTestExchangeHook.
extern WdspTestExchangeHookFn volatile wdsp_test_exchange_hook;

// dexchange's call: one pointer load and one null test when none is installed.
static __inline void WdspTestExchangeHook (int channel)
{
#ifdef _WIN32
	const WdspTestExchangeHookFn hook = wdsp_test_exchange_hook;
#else
	const WdspTestExchangeHookFn hook = __atomic_load_n (&wdsp_test_exchange_hook, __ATOMIC_ACQUIRE);
#endif
	if (hook != 0)
	{
		hook (channel);
	}
}

// Test-only: how long, in microseconds, the channel's latest teardown spent
// waiting for its worker to leave its loop (WdspWaitWorkerExit alone, not
// the rest of the teardown); -1 for an invalid channel. Not for production
// use.
PORT long long WDSPGetTestLastWorkerExitWaitUs (int channel);

// Test-only: how many times this channel's worker has left its loop in this
// process. Not for production use.
PORT int WDSPGetTestWorkerExitCount (int channel);

// Test-only: the pause, in nanoseconds, between the worker's waiter checks
// while it holds off (a nanosleep; SwitchToThread on Windows). Not for
// production use.
PORT long WDSPGetTestWorkerPauseNs (void);

#endif
