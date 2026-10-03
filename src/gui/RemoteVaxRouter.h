// =================================================================
// src/gui/RemoteVaxRouter.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R-R3-44 (R3 receiver audio plan,
// Task 5): VAX in a remote window.
//
// Owns the four VAX channels' feeders (core/audio/RemoteVaxFeeder) and
// decides, on the GUI thread, which of the Core's receivers each one asks
// for:
//
//   - A slice's VAX channel is kept on this computer, per Core and slice
//     (settingsKey()), never in the Core's Slice<N>/VaxChannel. The router
//     installs itself as the remote model's VAX channel store and restores
//     each slice's channel as the slice appears.
//   - VAX N carries every slice assigned to it, mixed as the local VAX
//     tee mixes them (the feeder sums their streams).
//   - Its stream is asked for while the channel has a slice and an open
//     output, and, where the platform reports whether an app is reading
//     the output (macOS, PipeWire), only while one is. Elsewhere it runs
//     while the channel is assigned.
//   - Request and release run here, on the GUI thread, never from the
//     feeders' block callbacks (RemoteMediaController's contract).
//
// A reason the Core's stream for one of the channel's slices stopped for
// is raised once per slice as notice(), in its wire or sentence form
// (OperatorReasonText::forDisplay words it); media-not-ready is not
// raised, being the link's own state.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23: Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-23: R-R3-44 fix wave: every slice on a channel is asked for
//                 and mixed, not only the lowest-numbered one. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-23: R-R3-44 fix wave follow-up: each slice's stop is raised
//                 once, naming the slice. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>

#include <array>
#include <functional>
#include <memory>

class QTimer;

namespace NereusSDR {

class AudioEngine;
class IReceiverPcmSink;
class RadioModel;
class RemoteVaxFeeder;
struct RemoteStationOptions;

class RemoteVaxRouter final : public QObject {
    Q_OBJECT
public:
    static constexpr int kChannels = 4;
    static constexpr int kReaderPollMs = 500;

    /// How the router asks the Core for a receiver's audio (MainWindow
    /// wires RemoteMediaController::requestReceiverAudio / release).
    struct ReceiverAudio {
        std::function<void(int sliceId, IReceiverPcmSink* sink)> request;
        std::function<void(int sliceId, IReceiverPcmSink* sink)> release;
    };

    /// Builds channel N's feeder (1..4). Tests pass their own output and
    /// clock; production uses the engine's VAX outputs.
    using FeederFactory = std::function<std::unique_ptr<RemoteVaxFeeder>(int channel)>;

    /// `coreKey` names the Core the assignments belong to (coreKeyFor()).
    /// With startWorkers false the feeders' workers are never started (a
    /// test pumps them itself).
    RemoteVaxRouter(RadioModel* model, AudioEngine* engine, QString coreKey,
                    QObject* parent = nullptr);
    RemoteVaxRouter(RadioModel* model, AudioEngine* engine, QString coreKey,
                    FeederFactory factory, bool startWorkers, QObject* parent = nullptr);
    ~RemoteVaxRouter() override;

    /// A short, stable name for the Core a remote window connects to: its
    /// pinned certificate when there is one, else its address.
    static QString coreKeyFor(const RemoteStationOptions& station);
    /// This computer's setting for a slice's VAX channel with that Core.
    static QString settingsKey(const QString& coreKey, int sliceId);

    void setReceiverAudio(ReceiverAudio source);

    RemoteVaxFeeder* feeder(int channel) const;
    /// The slices channel N's streams are asked for (ascending), empty for
    /// none; requestedSlice() is the lowest of them, -1 for none.
    QList<int> requestedSlices(int channel) const;
    int requestedSlice(int channel) const;
    /// The channel stored on this computer for a slice (0 when none).
    int storedChannel(int sliceId) const;

    /// Recomputes what each channel asks for. Runs on every assignment
    /// change and on a timer (the reader check).
    void refresh();

signals:
    /// The stream of `sliceId` on a channel stopped for `reason` (show it
    /// through OperatorReasonText::forDisplay).
    void notice(int channel, int sliceId, const QString& reason);

private:
    void storeChannel(int sliceId, int channel);
    void adoptSlice(int sliceId);
    void releaseChannel(int index);

    QPointer<RadioModel> m_model;
    AudioEngine* m_engine{nullptr};
    QString m_coreKey;
    ReceiverAudio m_source;
    bool m_startWorkers{true};
    std::array<std::unique_ptr<RemoteVaxFeeder>, kChannels> m_feeders;
    std::array<QList<int>, kChannels> m_requested;
    // "slice:reason" for each stop already raised on the channel.
    std::array<QSet<QString>, kChannels> m_noticed;
    QTimer* m_readerTimer{nullptr};
};

} // namespace NereusSDR
