// no-port-check: NereusSDR-original WDSP scheduling glue. Not a port of
// Thetis or WDSP logic; it schedules the existing per-channel csDSP lock so
// control calls are not starved by a busy DSP worker, and lets channel
// teardown wait for the worker to leave its loop, and times each worker block
// so the application can see how loaded each channel is.

/*  dsplock.c

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
// third_party/wdsp/src/dsplock.c (NereusSDR)
// =================================================================
//
// Why: the platform locks behind csDSP (a recursive pthread mutex on
// Linux/macOS, a CRITICAL_SECTION on Windows) do not hand ownership to a
// waiter. A channel worker that is slower than real time always has its next
// block ready, releases csDSP at the end of a block and retakes it within
// microseconds, so a control call (a getter, a setter, NNR configuration)
// can wait for many blocks. On an overloaded Core that starved the event loop
// and dropped the station link.
//
// How: every WDSP EnterCriticalSection call arrives here through comm.h's
// redirect. A csDSP lock is recognised by its address inside ch[]; its caller
// counts itself as a waiter until the platform call returns. Before the
// worker takes csDSP for a block it checks that count with one atomic read.
// If waiters exist it pauses briefly between checks while they do, keeps
// waiting for a short grace after the last one so a burst of calls stays
// together, and never waits longer than a per-block budget. The worker holds
// no lock while it waits, so no lock-order edge is added.
//
// A worker that failed to start (start_thread reports it through
// WdspWorkerStarted) is never waited for: teardown logs one line instead.
//
// Teardown: pre_main_destroy used to sleep a fixed 25 ms after telling the
// worker to stop, then free the channel's buffers and locks. A block slower
// than that (a neural noise reduction block on a slow computer) was still
// running when its memory went away. The worker now counts its exit after its
// loop ends, and WdspWaitWorkerExit waits for that count. Each channel build
// starts exactly one worker and each teardown stops exactly one, so the Nth
// teardown of a channel waits for the Nth exit; a worker that has not yet
// been scheduled still sees run cleared and exits at once.
//
// Load: WdspWorkerEnter reads a monotonic clock right after the worker
// acquires csDSP and WdspWorkerLeave reads it again just before the release,
// so the block time is the whole locked section (including the test block
// delay). The worker alone adds that time to per-channel 64-bit atomic
// counters; GetChannelDspLoad reads them without csDSP, so reading never
// makes the worker wait. The cost to the worker is two clock reads and a few
// atomic stores per block. The worker also publishes when its current block
// started (0 between blocks), so a reader can see a block that has not
// finished, and raises a per-interval maximum that one periodic reader takes
// and resets (TakeChannelDspIntervalMaxBlockUs).
//
// A reader turns two reads into a load as busy time over wall time: the
// change in busyNs + currentBlockNs over the change in readNs, the time of
// the clock read that gave currentBlockNs. That sum must be one instant's
// value, so the worker brackets its two changes to it (publishing a block's
// start; adding a finished block to busyNs and clearing the start) with a
// sequence count, and GetChannelDspLoad reads the pair again if the count
// was odd or moved. Without that, a read that fell between the worker adding
// a block and clearing its start would count the block twice, and one that
// fell the other way would count it not at all.
//
// Portability: Windows uses only Interlocked*, QueryPerformanceCounter and
// SwitchToThread, which WDSP already relies on (the load counters use
// InterlockedExchangeAdd64, InterlockedExchange64 and
// InterlockedCompareExchange64, the last also for the interval maximum's
// compare-and-swap); POSIX uses GCC/Clang atomic
// builtins, clock_gettime and nanosleep, as linux_port.c does.
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
//   2026-09-23 - Read time (readNs) from the clock read behind
//                 currentBlockNs, busyNs and the block in progress read as
//                 one consistent pair (load_seq), and the test-only
//                 periodic delay (WDSPSetTestPeriodicDelayUs) added by J.J.
//                 Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code (R-R3-40, R-R3-37).
//   2026-09-23 - GetChannelDspLoad returns 1 when no attempt found the pair
//                 at rest (the read may be torn; the caller skips it), and
//                 the test-only WDSPSetTestHoldLoadPair added by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code (R-R3-40). 2026-09-24: the hold is a flag the
//                 reader checks, steady whatever the worker does.
//   2026-09-24 - Caller check: WDSPSetCallerCheckHook, and WdspEnterCS and
//                 WdspWaitWorkerExit report each call to the installed hook
//                 before doing anything else (one pointer test when none
//                 is installed), by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code (R-R3-39).
//   2026-09-25 - Test-only block hook (WDSPSetTestBlockHook): reports each
//                 block's start and end on the clock the worker times it
//                 with, so a test measures the busy share the way the load
//                 counters do, independently of them. By J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code (R-R3-40).
//   2026-09-29 - Test-only WDSPGetTestWorkerPauseNs: the pause between
//                 waiter checks, so a test times the same pause without a
//                 copy of the constant. By J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code
//                 (R-R3-39).
//   2026-10-01 - Test-only exchange hook (WDSPSetTestExchangeHook,
//                 WdspTestExchangeHook): iobuffs.c's dexchange calls it after
//                 releasing Sem_OutReady, so a test holds the worker there
//                 while the caller runs ahead. One pointer test when none is
//                 installed. By J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include "comm.h"

// This file calls the platform lock directly.
#undef EnterCriticalSection

// Longest the worker holds off for waiters before one block, in microseconds.
static const int64_t kDspWorkerMaxDeferUs = 20000;
// The per-block budget is also at most this fraction (1/N) of the block period.
static const int64_t kDspWorkerDeferPeriodDivisor = 4;
// After the last announced waiter the worker keeps holding off this long, so
// back-to-back control calls are served as one burst.
static const int64_t kDspWorkerBurstGraceUs = 1000;
// Pause between waiter checks while the worker holds off.
static const long kDspWorkerPauseNs = 50000;

// Teardown writes one log line per this much time spent waiting for a worker.
static const int64_t kWorkerExitLogIntervalMs = 2000;
// Teardown polls finely for this long (a worker that is idle or between
// blocks exits well within it), then once per millisecond.
static const int64_t kWorkerExitFastPollUs = 5000;

// The caller check (see dsplock.h); 0 = none.
WdspCallerCheckHook volatile wdsp_caller_check_hook = 0;

// Test-only exchange hook (WDSPSetTestExchangeHook), 0 when none.
WdspTestExchangeHookFn volatile wdsp_test_exchange_hook = 0;

// Threads currently blocked entering each channel's csDSP.
static volatile long dsp_waiters[MAX_CHANNELS];

// Test-only per-block busy-wait, microseconds; 0 = off.
static volatile long test_block_delay_us[MAX_CHANNELS];
// Test-only busy-wait inside a processed block, microseconds; 0 = off.
static volatile long test_process_delay_us[MAX_CHANNELS];
// Test-only busy-wait inside every Nth processed block: microseconds and N
// (0 = off). The count is the worker's own.
static volatile long test_periodic_delay_us[MAX_CHANNELS];
static volatile long test_periodic_every[MAX_CHANNELS];
static long test_periodic_count[MAX_CHANNELS];

// Test-only: 1 while WDSPSetTestHoldLoadPair holds the channel's load pair
// open, so every read of it finds a change in progress.
static volatile long test_load_pair_held[MAX_CHANNELS];

// Times each channel's worker has left its loop.
static volatile long worker_exits[MAX_CHANNELS];
// Times teardown has waited for each channel's worker (control thread only).
static volatile long worker_exit_waits[MAX_CHANNELS];
// 1 while the channel has a started worker that teardown has not yet waited
// for; 0 before the first build, after teardown, or when the start failed.
static volatile long worker_started[MAX_CHANNELS];
// Test-only: how long the channel's latest teardown waited for its worker,
// microseconds (control thread writes, test reads).
static volatile long long last_exit_wait_us[MAX_CHANNELS];

// Per-channel load counters (see WdspChannelLoad). Written only by the
// channel's worker, read by GetChannelDspLoad from any thread.
static volatile long long load_blocks[MAX_CHANNELS];
static volatile long long load_busy_ns[MAX_CHANNELS];
static volatile long long load_late_blocks[MAX_CHANNELS];
static volatile long long load_max_block_us[MAX_CHANNELS];
static volatile long load_block_period_us[MAX_CHANNELS];
// Longest block since the last TakeChannelDspIntervalMaxBlockUs; the worker
// raises it, the one interval reader takes it and resets it to 0.
static volatile long long load_interval_max_us[MAX_CHANNELS];
// When the worker acquired csDSP for its current block, 0 while it is not
// inside a block. Written by the worker, read by GetChannelDspLoad.
static volatile long long block_start_ns[MAX_CHANNELS];
// Odd while the worker is changing load_busy_ns or block_start_ns; each
// change adds 2. GetChannelDspLoad reads the pair again when it moved.
static volatile long long load_seq[MAX_CHANNELS];
// Most times GetChannelDspLoad reads the pair before it takes what it has.
// The worker's changes are a few instructions long, so a second read almost
// always succeeds.
static const int kLoadReadAttempts = 64;

static int64_t dsplock_now_us (void)
{
#ifdef _WIN32
	LARGE_INTEGER frequency, counter;
	QueryPerformanceFrequency (&frequency);
	QueryPerformanceCounter (&counter);
	return (int64_t)(counter.QuadPart / frequency.QuadPart) * 1000000
		+ (int64_t)(counter.QuadPart % frequency.QuadPart) * 1000000 / frequency.QuadPart;
#else
	struct timespec now;
	clock_gettime (CLOCK_MONOTONIC, &now);
	return (int64_t)now.tv_sec * 1000000 + (int64_t)now.tv_nsec / 1000;
#endif
}

static int64_t dsplock_now_ns (void)
{
#ifdef _WIN32
	LARGE_INTEGER frequency, counter;
	QueryPerformanceFrequency (&frequency);
	QueryPerformanceCounter (&counter);
	return (int64_t)(counter.QuadPart / frequency.QuadPart) * 1000000000
		+ (int64_t)(counter.QuadPart % frequency.QuadPart) * 1000000000 / frequency.QuadPart;
#else
	struct timespec now;
	clock_gettime (CLOCK_MONOTONIC, &now);
	return (int64_t)now.tv_sec * 1000000000 + (int64_t)now.tv_nsec;
#endif
}

static void load_add64 (volatile long long* target, long long value)
{
#ifdef _WIN32
	InterlockedExchangeAdd64 (target, value);
#else
	__atomic_fetch_add (target, value, __ATOMIC_RELEASE);
#endif
}

static void load_store64 (volatile long long* target, long long value)
{
#ifdef _WIN32
	InterlockedExchange64 (target, value);
#else
	__atomic_store_n (target, value, __ATOMIC_RELEASE);
#endif
}

// Raises *target to value if value is larger (the worker's only writer
// path; a concurrent take may reset it to 0).
static void load_max64 (volatile long long* target, long long value)
{
#ifdef _WIN32
	long long current = InterlockedCompareExchange64 (target, 0, 0);
	while (value > current)
	{
		const long long seen = InterlockedCompareExchange64 (target, value, current);
		if (seen == current)
		{
			break;
		}
		current = seen;
	}
#else
	long long current = __atomic_load_n (target, __ATOMIC_RELAXED);
	while (value > current
		&& !__atomic_compare_exchange_n (target, &current, value, 0,
			__ATOMIC_RELEASE, __ATOMIC_RELAXED))
	{
		// current now holds the value another thread stored; retry.
	}
#endif
}

static long long load_exchange64 (volatile long long* target, long long value)
{
#ifdef _WIN32
	return InterlockedExchange64 (target, value);
#else
	return __atomic_exchange_n (target, value, __ATOMIC_ACQ_REL);
#endif
}

static long long load_read64 (volatile long long* source)
{
#ifdef _WIN32
	return InterlockedCompareExchange64 (source, 0, 0);
#else
	return __atomic_load_n (source, __ATOMIC_ACQUIRE);
#endif
}

// Worker only: opens (odd) or closes (even) a change to the busy/start pair.
static void load_seq_step (int channel)
{
#ifdef _WIN32
	InterlockedIncrement64 (&load_seq[channel]);
#else
	__atomic_fetch_add (&load_seq[channel], 1, __ATOMIC_ACQ_REL);
#endif
}

static long load_read32 (volatile long* source)
{
#ifdef _WIN32
	return InterlockedCompareExchange (source, 0, 0);
#else
	return __atomic_load_n (source, __ATOMIC_ACQUIRE);
#endif
}

static void dsplock_pause (void)
{
#ifdef _WIN32
	// Sleep(1) is too coarse on Windows; yield the rest of the time slice.
	SwitchToThread ();
#else
	const struct timespec pause = { 0, kDspWorkerPauseNs };
	nanosleep (&pause, 0);
#endif
}

static long load_waiters (int channel)
{
#ifdef _WIN32
	return InterlockedCompareExchange (&dsp_waiters[channel], 0, 0);
#else
	return __atomic_load_n (&dsp_waiters[channel], __ATOMIC_ACQUIRE);
#endif
}

static long load_worker_exits (int channel)
{
#ifdef _WIN32
	return InterlockedCompareExchange (&worker_exits[channel], 0, 0);
#else
	return __atomic_load_n (&worker_exits[channel], __ATOMIC_ACQUIRE);
#endif
}

static long load_test_block_delay (int channel)
{
#ifdef _WIN32
	return test_block_delay_us[channel];
#else
	return __atomic_load_n (&test_block_delay_us[channel], __ATOMIC_RELAXED);
#endif
}

// Returns the channel whose csDSP lives at cs, or -1 for any other lock.
static int dsp_channel_of (const void* cs)
{
	const uintptr_t base = (uintptr_t)&ch[0].csDSP;
	const uintptr_t address = (uintptr_t)cs;
	uintptr_t offset, index;
	if (address < base)
	{
		return -1;
	}
	offset = address - base;
	if (offset % sizeof (ch[0]) != 0)
	{
		return -1;
	}
	index = offset / sizeof (ch[0]);
	return index < MAX_CHANNELS ? (int)index : -1;
}

static int valid_channel (int channel)
{
	return channel >= 0 && channel < MAX_CHANNELS;
}

static int64_t worker_defer_budget_us (int channel)
{
	const int64_t size = ch[channel].dsp_size;
	const int64_t rate = ch[channel].dsp_rate;
	int64_t budget = kDspWorkerMaxDeferUs;
	if (size > 0 && rate > 0)
	{
		const int64_t share = size * 1000000 / rate / kDspWorkerDeferPeriodDivisor;
		if (share < budget)
		{
			budget = share;
		}
	}
	return budget;
}

// Called only when waiters were seen. Holds off while they exist, plus the
// burst grace, within the per-block budget.
static void worker_defer (int channel)
{
	const int64_t budget = worker_defer_budget_us (channel);
	const int64_t start = dsplock_now_us ();
	int64_t last_seen = start;
	for (;;)
	{
		int64_t now;
		dsplock_pause ();
		now = dsplock_now_us ();
		if (now - start >= budget)
		{
			break;
		}
		if (load_waiters (channel) != 0)
		{
			last_seen = now;
		}
		else if (now - last_seen >= kDspWorkerBurstGraceUs)
		{
			break;
		}
	}
}

static long load_test_process_delay (int channel)
{
#ifdef _WIN32
	return test_process_delay_us[channel];
#else
	return __atomic_load_n (&test_process_delay_us[channel], __ATOMIC_RELAXED);
#endif
}

static void test_busy_wait_us (long delay)
{
	if (delay > 0)
	{
		const int64_t until = dsplock_now_us () + delay;
		while (dsplock_now_us () < until)
		{
			// busy-wait: simulates DSP work while holding csDSP
		}
	}
}

// Test-only block hook (WDSPSetTestBlockHook), 0 when none. The worker
// loads it once at each block's start and end.
typedef void (*WdspTestBlockHook) (int channel, long long startNs, long long endNs);
static WdspTestBlockHook volatile test_block_hook = 0;

static WdspTestBlockHook load_test_block_hook (void)
{
#ifdef _WIN32
	return (WdspTestBlockHook)InterlockedCompareExchangePointer ((PVOID volatile*)&test_block_hook, 0, 0);
#else
	return __atomic_load_n (&test_block_hook, __ATOMIC_ACQUIRE);
#endif
}

static void test_block_delay (int channel)
{
	test_busy_wait_us (load_test_block_delay (channel));
}

static long load_relaxed32 (volatile long* source)
{
#ifdef _WIN32
	return *source;
#else
	return __atomic_load_n (source, __ATOMIC_RELAXED);
#endif
}

void WdspWorkerTestProcessDelay (int channel)
{
	if (valid_channel (channel))
	{
		const long every = load_relaxed32 (&test_periodic_every[channel]);
		test_busy_wait_us (load_test_process_delay (channel));
		if (every > 0 && ++test_periodic_count[channel] % every == 0)
		{
			test_busy_wait_us (load_relaxed32 (&test_periodic_delay_us[channel]));
		}
	}
}

void WdspEnterCS (LPCRITICAL_SECTION cs)
{
	const int channel = dsp_channel_of (cs);
	WdspCallerCheck (channel, WDSP_CALLER_ENTER_CS);
	if (channel < 0)
	{
		EnterCriticalSection (cs);
		return;
	}
	InterlockedIncrement (&dsp_waiters[channel]);
	EnterCriticalSection (cs);
	InterlockedDecrement (&dsp_waiters[channel]);
}

void WdspWorkerEnter (int channel)
{
	if (valid_channel (channel) && load_waiters (channel) != 0)
	{
		worker_defer (channel);
	}
	EnterCriticalSection (&ch[channel].csDSP);
	if (valid_channel (channel))
	{
		const int64_t size = ch[channel].dsp_size;
		const int64_t rate = ch[channel].dsp_rate;
		const long period_us = (size > 0 && rate > 0) ? (long)(size * 1000000 / rate) : 0L;
		// Published before the block starts, so a reader can judge a first
		// block that has not finished yet.
		if (period_us != load_block_period_us[channel])
		{
			InterlockedExchange (&load_block_period_us[channel], period_us);
		}
		const int64_t start_ns = dsplock_now_ns ();
		WdspTestBlockHook hook;
		load_seq_step (channel);
		load_store64 (&block_start_ns[channel], (long long)start_ns);
		load_seq_step (channel);
		hook = load_test_block_hook ();
		if (hook != 0)
		{
			hook (channel, (long long)start_ns, 0);
		}
		test_block_delay (channel);
	}
}

// Worker only, csDSP held: adds the block that just ended to the counters.
static void record_block (int channel)
{
	const int64_t start_ns = (int64_t)block_start_ns[channel];
	const int64_t end_ns = dsplock_now_ns ();
	const int64_t elapsed_ns = end_ns - start_ns;
	WdspTestBlockHook hook;
	const long long elapsed_us = (long long)(elapsed_ns / 1000);
	const long period_us = load_block_period_us[channel];
	// The block moves from "in progress" to busyNs as one change.
	load_seq_step (channel);
	load_add64 (&load_busy_ns[channel], (long long)elapsed_ns);
	load_store64 (&block_start_ns[channel], 0);
	load_seq_step (channel);
	if (period_us > 0 && elapsed_us > period_us)
	{
		load_add64 (&load_late_blocks[channel], 1);
	}
	if (elapsed_us > load_max_block_us[channel])
	{
		load_store64 (&load_max_block_us[channel], elapsed_us);
	}
	load_max64 (&load_interval_max_us[channel], elapsed_us);
	load_add64 (&load_blocks[channel], 1);
	hook = load_test_block_hook ();
	if (hook != 0)
	{
		hook (channel, (long long)start_ns, (long long)end_ns);
	}
}

void WdspWorkerLeave (int channel)
{
	if (valid_channel (channel))
	{
		record_block (channel);
	}
	LeaveCriticalSection (&ch[channel].csDSP);
}

void WdspWorkerExited (int channel)
{
	if (valid_channel (channel))
	{
		InterlockedIncrement (&worker_exits[channel]);
	}
}

void WdspWorkerStarted (int channel, int started)
{
	if (valid_channel (channel))
	{
		InterlockedExchange (&worker_started[channel], started ? 1L : 0L);
	}
}

void WdspWaitWorkerExit (int channel)
{
	long expected;
	int64_t start, next_log;
	WdspCallerCheck (channel, WDSP_CALLER_WAIT_WORKER_EXIT);
	if (!valid_channel (channel))
	{
		return;
	}
	if (InterlockedExchange (&worker_started[channel], 0L) == 0)
	{
		// No worker is running for this build, so no exit will come; the
		// exit pairing count is left as it is.
		dprintf ("wdsp: channel %d has no DSP worker (it did not start); "
			"teardown goes ahead without waiting\n", channel);
		load_store64 (&last_exit_wait_us[channel], 0);
		return;
	}
	expected = InterlockedIncrement (&worker_exit_waits[channel]);
	start = dsplock_now_us ();
	next_log = start + kWorkerExitLogIntervalMs * 1000;
	while (load_worker_exits (channel) < expected)
	{
		int64_t now = dsplock_now_us ();
		if (now - start < kWorkerExitFastPollUs)
		{
			dsplock_pause ();
		}
		else
		{
			Sleep (1);
		}
		now = dsplock_now_us ();
		if (now >= next_log)
		{
			dprintf ("wdsp: channel %d teardown still waiting for its DSP worker "
				"to finish a block (%d ms)\n", channel, (int)((now - start) / 1000));
			next_log += kWorkerExitLogIntervalMs * 1000;
		}
	}
	load_store64 (&last_exit_wait_us[channel], (long long)(dsplock_now_us () - start));
}

PORT
void WDSPSetCallerCheckHook (WdspCallerCheckHook hook)
{
#ifdef _WIN32
	InterlockedExchangePointer ((PVOID volatile*)&wdsp_caller_check_hook, (PVOID)hook);
#else
	__atomic_store_n (&wdsp_caller_check_hook, hook, __ATOMIC_RELEASE);
#endif
}

PORT
void WDSPSetTestBlockDelayUs (int channel, int microseconds)
{
	if (!valid_channel (channel))
	{
		return;
	}
	InterlockedExchange (&test_block_delay_us[channel], microseconds > 0 ? (long)microseconds : 0L);
}

PORT
void WDSPSetTestProcessDelayUs (int channel, int microseconds)
{
	if (!valid_channel (channel))
	{
		return;
	}
	InterlockedExchange (&test_process_delay_us[channel], microseconds > 0 ? (long)microseconds : 0L);
}

PORT
void WDSPSetTestPeriodicDelayUs (int channel, int microseconds, int everyBlocks)
{
	if (!valid_channel (channel))
	{
		return;
	}
	InterlockedExchange (&test_periodic_delay_us[channel], microseconds > 0 ? (long)microseconds : 0L);
	InterlockedExchange (&test_periodic_every[channel], everyBlocks > 0 ? (long)everyBlocks : 0L);
}

PORT
int GetChannelDspLoad (int channel, WdspChannelLoad* out)
{
	if (!valid_channel (channel) || out == 0)
	{
		return -1;
	}
	// blocks first: the worker bumps it last, so every block it counts has
	// already been added to the other fields.
	out->blocks = load_read64 (&load_blocks[channel]);
	out->lateBlocks = load_read64 (&load_late_blocks[channel]);
	out->maxBlockUs = load_read64 (&load_max_block_us[channel]);
	out->blockPeriodUs = (int)load_read32 (&load_block_period_us[channel]);
	{
		// busyNs and the block in progress as one instant's pair (load_seq).
		// One clock read, taken while the pair is known to hold, gives both
		// the block's time so far and the read time, so busyNs +
		// currentBlockNs is the busy time up to readNs.
		long long busy = 0, start = 0, now_ns = 0;
		int attempt;
		for (attempt = 0; attempt < kLoadReadAttempts; ++attempt)
		{
			const long long before = load_read64 (&load_seq[channel]);
			busy = load_read64 (&load_busy_ns[channel]);
			start = load_read64 (&block_start_ns[channel]);
			now_ns = (long long)dsplock_now_ns ();
			// A test hold (WDSPSetTestHoldLoadPair) makes every attempt
			// find the pair changing, steadily, whatever the worker does.
			if ((before & 1) == 0 && load_read64 (&load_seq[channel]) == before
				&& load_read32 (&test_load_pair_held[channel]) == 0)
			{
				break;
			}
		}
		out->busyNs = busy;
		{
			const long long elapsed = start != 0 ? now_ns - start : 0;
			out->currentBlockNs = elapsed > 0 ? elapsed : 0;
		}
		out->readNs = now_ns;
		// The worker kept the pair moving for every attempt: the last one
		// may be torn (a block counted twice or not at all), so the caller
		// is told to skip this read rather than measure with it.
		if (attempt == kLoadReadAttempts)
		{
			return 1;
		}
	}
	return 0;
}

PORT
void WDSPSetTestHoldLoadPair (int channel, int hold)
{
	if (!valid_channel (channel))
	{
		return;
	}
	// Read by GetChannelDspLoad only; the worker's sequence is untouched, so
	// the hold is steady (a parity trick on load_seq was not: the worker's
	// own steps briefly made it even again).
	InterlockedExchange (&test_load_pair_held[channel], hold ? 1L : 0L);
}

PORT
void WDSPSetTestBlockHook (void (*hook) (int channel, long long startNs, long long endNs))
{
#ifdef _WIN32
	InterlockedExchangePointer ((PVOID volatile*)&test_block_hook, (PVOID)hook);
#else
	__atomic_store_n (&test_block_hook, hook, __ATOMIC_RELEASE);
#endif
}

PORT
long long TakeChannelDspIntervalMaxBlockUs (int channel)
{
	if (!valid_channel (channel))
	{
		return -1;
	}
	return load_exchange64 (&load_interval_max_us[channel], 0);
}

PORT
void WDSPSetTestExchangeHook (WdspTestExchangeHookFn hook)
{
#ifdef _WIN32
	InterlockedExchangePointer ((PVOID volatile*)&wdsp_test_exchange_hook, (PVOID)hook);
#else
	__atomic_store_n (&wdsp_test_exchange_hook, hook, __ATOMIC_RELEASE);
#endif
}

PORT
long long WDSPGetTestLastWorkerExitWaitUs (int channel)
{
	return valid_channel (channel) ? load_read64 (&last_exit_wait_us[channel]) : -1;
}

PORT
int WDSPGetTestWorkerExitCount (int channel)
{
	return valid_channel (channel) ? (int)load_worker_exits (channel) : 0;
}

PORT
long WDSPGetTestWorkerPauseNs (void)
{
	return kDspWorkerPauseNs;
}
