// =================================================================
// src/core/SpeakerAmplifier.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original rule written from the radio speaker
//   spec (R-SPK-09). piHPSDR is cited as the source of a fact (the CW and
//   Tune exception); no upstream code is translated.
//
// When the radio's speaker amplifier is off. On Protocol 2 radios with a
// switchable amplifier (HardwareProfile::hasAudioAmplifier) it drives the
// radio's own speaker, headphone and line out, and high-priority byte 1400
// bit 1 mutes it (P2CodecOrionMkII).
//
// The amplifier is off when:
//   - the operator set it Always off (mode 2),
//   - the radio speaker is muted, or
//   - it is set Off while transmitting (mode 1), the radio is transmitting,
//     and no side tone is expected.
// A side tone is expected in CW and while tuning, so Off while transmitting
// leaves the amplifier on then, as piHPSDR does:
//   From piHPSDR src/new_protocol.c:868-882 [@4aa95c5] (the condition, in
//   substance): mute_spkr_amp, or mute_spkr_xmit while xmit with a mode
//   other than CWL or CWU and the transmitter not tuning.
//
// Used by P2RadioConnection for the wire byte and by RadioModel for the
// state it shows.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-06: Created for the radio speaker control (R-SPK-09). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

namespace NereusSDR {

// mode: 0 Normal, 1 Off while transmitting, 2 Always off.
constexpr bool speakerAmplifierOff(bool hasAmp, int mode, bool muted, bool transmitting,
                                   bool sidetoneExpected)
{
    return hasAmp && (mode == 2 || muted || (mode == 1 && transmitting && !sidetoneExpected));
}

} // namespace NereusSDR
