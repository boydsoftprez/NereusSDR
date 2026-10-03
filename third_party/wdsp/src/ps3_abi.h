/*
 * Narrow Nereus-facing PureSignal 3 ABI shared with C++ callers.
 *
 * Copyright (C) 2026 J.J. Boyd, KG4VCF
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * 2026-09-22 — Added bounded PS3 geometry, status/readback, and asynchronous
 * file-operation cancellation ABI for NereusSDR.
 */
#ifndef _ps3_abi_h
#define _ps3_abi_h

#include <stdint.h>

#define PS3_MAX_DISPLAY_SAMPLES 4096
#define PS3_DISPLAY_CORRECTION_POINTS 512

typedef struct _PSFileOperationStatus
{
	uint64_t generation;
	int pending;
	int result; /* 0 success, 1 failure, 2 cancelled */
} PSFileOperationStatus;

#ifdef __cplusplus
extern "C" {
#endif

int GetPSRunCal(int channel, int* run);
int GetPSCorrectionState(int channel, int* run, int* busy);
/*
 * Nonblocking active-stream stop. CALCC is retired to LRESET immediately and
 * IQC enters its normal END ramp; GetPSCorrectionState acknowledges completion
 * with run=busy=0. Call while TxChannel sample processing is still running.
 * Returns 0 for invalid, uninitialized, or tearing-down channels.
 */
int RequestPSCorrectionStop(int channel);
/*
 * Immediately retires correction processing only after the host has made TXA
 * quiescent. This call does not perform the normal active-stream END ramp and
 * must not be used while TXA can process samples. On success CALCC reports
 * LRESET and IQC reports run=busy=0 before return; retained curves remain
 * available. Returns 0 for invalid, uninitialized, or tearing-down channels.
 */
int StopPSCorrectionQuiescent(int channel);
/*
 * Applies the retained correction through IQC BEGIN, preserving the normal
 * ramp when TXA next processes samples. Returns 0 without changing state when
 * the channel is invalid, is tearing down, or has no complete retained curve.
 */
int ApplyPSCorrection(int channel);
/* Copies whether a complete retained correction exists under the IQC lock. */
int GetPSCorrectionAvailable(int channel, int* available);
int GetPSFileOperationStatus(int channel, int kind,
	PSFileOperationStatus* status);
/*
 * Requests nonblocking cancellation of a pending save (kind 0) or restore
 * (kind 1). Returns 1 for a valid pending operation, including an already
 * requested cancellation, and 0 for invalid/uninitialized/not-pending input.
 * Status remains pending until the worker is quiescent, then advances once
 * with result 2. A cancelled restore cannot mutate IQC after this returns.
 */
int CancelPSFileOperation(int channel, int kind);

#ifdef __cplusplus
}
#endif

#endif
