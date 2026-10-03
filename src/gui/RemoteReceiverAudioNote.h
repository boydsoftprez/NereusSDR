// =================================================================
// src/gui/RemoteReceiverAudioNote.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. R-R3-43 / R-R3-44: which note
// Setup > Audio > VAX shows about a remote window's receiver streams.
// Its own header so SetupDialog.h and AudioVaxPage.h need not pull in
// RemoteAudioStatus.h; remoteReceiverAudioNote() there computes it.
// =================================================================

#pragma once

#include <QtGlobal>

namespace NereusSDR {

/// None: no receiver streams from the Core, or they are lossless (and
/// always in a local window). OpusChosen: Opus runs because it is the
/// choice. LosslessUnavailable: Lossless is the choice but Opus runs (the
/// link could not carry it, or the Core refused).
enum class RemoteReceiverAudioNote : quint8 {
    None,
    OpusChosen,
    LosslessUnavailable,
};

} // namespace NereusSDR
