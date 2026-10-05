// NereusSDR for iOS: non-variadic wrappers over the Opus encoder settings, which Swift cannot reach through opus_encoder_ctl
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#ifndef NEREUS_COPUS_SHIM_H
#define NEREUS_COPUS_SHIM_H

#include <opus.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Each returns OPUS_OK, or the Opus error code opus_encoder_ctl gave. */
int nereus_opus_set_bitrate(OpusEncoder *encoder, opus_int32 bitsPerSecond);
int nereus_opus_set_bandwidth(OpusEncoder *encoder, opus_int32 bandwidth);
int nereus_opus_set_complexity(OpusEncoder *encoder, opus_int32 complexity);
int nereus_opus_set_vbr(OpusEncoder *encoder, opus_int32 enabled);
int nereus_opus_set_vbr_constraint(OpusEncoder *encoder, opus_int32 constrained);
int nereus_opus_set_inband_fec(OpusEncoder *encoder, opus_int32 enabled);
int nereus_opus_set_packet_loss_perc(OpusEncoder *encoder, opus_int32 percent);
int nereus_opus_set_dtx(OpusEncoder *encoder, opus_int32 enabled);
int nereus_opus_set_signal(OpusEncoder *encoder, opus_int32 signal);
int nereus_opus_get_bitrate(OpusEncoder *encoder, opus_int32 *bitsPerSecond);
/* Core receive profile: 48 kHz stereo, OPUS_APPLICATION_AUDIO. Returns -1 on failure. */
int nereus_opus_receive_profile_lookahead_frames(void);
/* True only for structurally parseable Opus containing exactly 40 ms at 48 kHz. */
int nereus_opus_valid_receive_packet(const unsigned char *payload, int length);

#ifdef __cplusplus
}
#endif

#endif
