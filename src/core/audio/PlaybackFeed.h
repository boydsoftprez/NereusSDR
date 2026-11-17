// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original interface.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; what AudioEngine's drain pulls a
//             recording's playback from, for one mixer playback slot
//             (spec 6.1 Player, 6.3). AI tooling: Anthropic Claude Code.
#pragma once

namespace NereusSDR {

// AudioEngine calls pullSpeaker on its DSP thread, inside each drain, for
// every playback slot that drain sums (the local one and each owner mix's
// own), asking for what tops the slot up to the drain. It must not lock,
// wait or allocate. Up to `frames` mono 48 kHz samples into `mono48k`;
// returns how many were the playback (0 when nothing plays there).
class RecordingPlaybackFeed {
public:
    virtual ~RecordingPlaybackFeed() = default;
    virtual int pullSpeaker(int playbackSlotId, float* mono48k, int frames) noexcept = 0;
};

}  // namespace NereusSDR
