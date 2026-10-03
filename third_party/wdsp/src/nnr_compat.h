/* NereusSDR NNR control/readback boundary.
 * Copyright (C) 2026 J.J. Boyd, KG4VCF
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
// no-port-check: NereusSDR-original ABI glue. DSP algorithms remain in nnr.c.
// 2026-09-21: J.J. Boyd (KG4VCF), with OpenAI Codex assistance.
// 2026-09-23: runtime NNR limit (R-R3-40) by J.J. Boyd (KG4VCF), with
// Anthropic Claude Code assistance.
#ifndef NEREUS_NNR_COMPAT_H
#define NEREUS_NNR_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NNRConfiguration {
    int model_slot;
    int position;
    double mask_floor_db;
    double alpha;
    double alpha_knee_db;
    double tau_seconds;
    double max_gain_db;
    double attack_ms;
    double release_ms;
} NNRConfiguration;

typedef struct NNRRuntimeStatus {
    NNRConfiguration configuration;
    int ready;
    int running;
    int rate_supported;
    int model_available[2];
    int model_source[2]; /* 0 unavailable, 1 bundled, 2 file */
    int dsp_rate_hz;
    int network_rate_hz;
    int delay_samples;
    int test_mode;
    int output_mode;
    int profiling_available;
    /* NereusSDR (R-R3-40): configuration.model_slot is the caller's accepted
     * model; active_model_slot is the one running, limit the applied runtime
     * limit (0 none, 1 standard model only, 2 off), requested_run the
     * caller's run request. */
    int active_model_slot;
    int limit;
    int requested_run;
} NNRRuntimeStatus;

/* Caller owns the channel lifetime. These never retain an output pointer.
 * Return 1 for a valid copy/accepted operation, 0 for invalid/unavailable.
 * Configuration is applied under one DSP lock; refusal applies no fields. */
int GetRXANNRStatus(int channel, NNRRuntimeStatus* out);
int ConfigureRXANNR(int channel, const NNRConfiguration* requested,
                   NNRRuntimeStatus* accepted);
int SetRXANNRDiagnostics(int channel, int test_mode, int output_mode);
/* R-R3-40: never takes the channel DSP lock. limit: 0 none, 1 standard model
 * only, 2 off. Applied by the channel's worker at its next block. */
void RequestRXANNRLimit(int channel, int limit);

#ifdef __cplusplus
}
#endif
#endif
