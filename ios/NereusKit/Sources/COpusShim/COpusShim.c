// NereusSDR for iOS: non-variadic wrappers over the Opus encoder settings, which Swift cannot reach through opus_encoder_ctl
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#include "COpusShim.h"

int nereus_opus_set_bitrate(OpusEncoder *encoder, opus_int32 bitsPerSecond)
{
    return opus_encoder_ctl(encoder, OPUS_SET_BITRATE(bitsPerSecond));
}

int nereus_opus_set_bandwidth(OpusEncoder *encoder, opus_int32 bandwidth)
{
    return opus_encoder_ctl(encoder, OPUS_SET_BANDWIDTH(bandwidth));
}

int nereus_opus_set_complexity(OpusEncoder *encoder, opus_int32 complexity)
{
    return opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(complexity));
}

int nereus_opus_set_vbr(OpusEncoder *encoder, opus_int32 enabled)
{
    return opus_encoder_ctl(encoder, OPUS_SET_VBR(enabled));
}

int nereus_opus_set_vbr_constraint(OpusEncoder *encoder, opus_int32 constrained)
{
    return opus_encoder_ctl(encoder, OPUS_SET_VBR_CONSTRAINT(constrained));
}

int nereus_opus_set_inband_fec(OpusEncoder *encoder, opus_int32 enabled)
{
    return opus_encoder_ctl(encoder, OPUS_SET_INBAND_FEC(enabled));
}

int nereus_opus_set_packet_loss_perc(OpusEncoder *encoder, opus_int32 percent)
{
    return opus_encoder_ctl(encoder, OPUS_SET_PACKET_LOSS_PERC(percent));
}

int nereus_opus_set_dtx(OpusEncoder *encoder, opus_int32 enabled)
{
    return opus_encoder_ctl(encoder, OPUS_SET_DTX(enabled));
}

int nereus_opus_set_signal(OpusEncoder *encoder, opus_int32 signal)
{
    return opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(signal));
}

int nereus_opus_get_bitrate(OpusEncoder *encoder, opus_int32 *bitsPerSecond)
{
    return opus_encoder_ctl(encoder, OPUS_GET_BITRATE(bitsPerSecond));
}

int nereus_opus_receive_profile_lookahead_frames(void)
{
    int error = OPUS_OK;
    OpusEncoder *encoder = opus_encoder_create(48000, 2, OPUS_APPLICATION_AUDIO, &error);
    if (encoder == NULL || error != OPUS_OK) {
        if (encoder != NULL) { opus_encoder_destroy(encoder); }
        return -1;
    }
    opus_int32 frames = 0;
    const int status = opus_encoder_ctl(encoder, OPUS_GET_LOOKAHEAD(&frames));
    opus_encoder_destroy(encoder);
    return status == OPUS_OK && frames >= 0 ? (int)frames : -1;
}

int nereus_opus_valid_receive_packet(const unsigned char *payload, int length)
{
    if (payload == NULL || length <= 0) { return 0; }
    opus_int16 sizes[48];
    const unsigned char *frames[48];
    if (opus_packet_parse(payload, length, NULL, frames, sizes, NULL) <= 0) { return 0; }
    return opus_packet_get_nb_samples(payload, length, 48000) == 1920;
}
