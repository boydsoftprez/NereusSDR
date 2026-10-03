// =================================================================
// src/core/daemon/DaemonAgcSource.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Headless ownership and bounded
// transport plumbing around the existing FFTEngine and NoiseFloorTracker.
// =================================================================

#pragma once

#include "core/session/media/DaemonSpectrumSource.h"

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QVector>

#include <map>
#include <memory>

namespace NereusSDR {

class DaemonSpectrumSource;
class NoiseFloorTracker;
class RadioModel;
class SliceModel;
enum class ConnectionState;

/// Station-lifetime, per-stream Auto AGC noise-floor production.
///
/// This deliberately owns a separate DaemonSpectrumSource.  Display media
/// sources are configured by an authenticated client's endpoint request and
/// can be retired when that endpoint disappears; Auto AGC is station state
/// and must continue without a client or display subscription.
class DaemonAgcSource final : public QObject {
    Q_OBJECT
public:
    explicit DaemonAgcSource(RadioModel* radioModel, QObject* parent = nullptr);
    ~DaemonAgcSource() override;

    int activeStreamCount() const;

private slots:
    void onFrameAvailable(NereusSDR::MediaSourceKey key);
    void onStreamBindingsChanged(int streamIndex, const QVector<int>& sliceIndices);
    void onStreamCentreChanged(int streamIndex, double centreHz, int sampleRateHz);
    void onStreamsSuspended(const QVector<int>& streamIndices, const QString& reason);
    void onStreamAdcRoutingChanged();
    void onConnectionStateChanged(NereusSDR::ConnectionState state);
    void onMoxChanged(bool mox);

private:
    struct StreamState;

    void reconcileAllStreams();
    void reconcileStream(int streamIndex);
    void reconfigureStream(int streamIndex);
    void retireStream(int streamIndex);
    void observeSlice(SliceModel* slice);
    void invalidate(StreamState& state);
    void publishInvalid(StreamState& state);
    void publishIfGood(StreamState& state);

    bool canReceiveFromStream(int streamIndex) const;
    bool hasBoundSlices(int streamIndex) const;
    static MediaSourceKey sourceKey(int streamIndex);

    QPointer<RadioModel> m_radioModel;
    std::unique_ptr<DaemonSpectrumSource> m_source;
    std::map<int, std::unique_ptr<StreamState>> m_streams;
    QSet<int> m_suspendedStreams;
    QHash<SliceModel*, int> m_observedSliceStreams;
};

} // namespace NereusSDR
