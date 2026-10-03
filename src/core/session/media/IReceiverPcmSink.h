// =================================================================
// src/core/session/media/IReceiverPcmSink.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  R-R3-43: what an app-facing consumer
// on this computer (TCI, VAX) implements to receive one receiver's audio
// from the Core without a speaker. RemoteMediaController hands it blocks;
// it owns no stream, network or device.
// =================================================================

#pragma once

#include <QString>

namespace NereusSDR {

/// A consumer of one receiver's audio in a remote window, registered with
/// RemoteMediaController::requestReceiverAudio() and removed with
/// releaseReceiverAudio(). One sink may listen to several receivers; each
/// call names the receiver (the Core's slice id).
class IReceiverPcmSink {
public:
    virtual ~IReceiverPcmSink() = default;

    /// `frames` frames of interleaved stereo 48 kHz float, in stream order,
    /// paced by the Core's clock. A lost packet arrives as concealed audio
    /// (Opus) or silence (lossless). Called on a receive worker thread,
    /// never the GUI thread: it must return quickly, must not block and
    /// must not call back into RemoteMediaController. Once
    /// releaseReceiverAudio() has returned it is not called again for that
    /// receiver.
    virtual void receiverAudioBlock(int sliceId, const float* interleavedStereo, int frames) = 0;

    /// The receiver's audio stopped, with why: one of the Core's wire
    /// reasons (client-disabled, media-not-ready, radio-offline,
    /// encoder-unavailable, slice-removed, receiver-limit) or a sentence
    /// this computer wrote. Show it through OperatorReasonText::forDisplay().
    /// Called on the controller's (GUI) thread. A stop is not final: while
    /// the sink stays registered, blocks resume if the Core sends the
    /// receiver's audio again. Until then the consumer plays silence; no
    /// request is repeated on its behalf, so a removed receiver never loops.
    virtual void receiverAudioStopped(int sliceId, const QString& reason) = 0;
};

} // namespace NereusSDR
