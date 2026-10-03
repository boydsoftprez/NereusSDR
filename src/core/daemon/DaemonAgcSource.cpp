// =================================================================
// src/core/daemon/DaemonAgcSource.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  This class only routes the bounded
// station I/Q source into existing FFT/noise-floor components; it introduces
// no noise-floor or AGC algorithm.
// =================================================================
// Modification history (NereusSDR):
// 2026-09-30: The per-bin dBm conversion moved to DaemonSpectrumSource's
// engine threads; the values are unchanged.
// J.J. Boyd (KG4VCF), AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/daemon/DaemonAgcSource.h"

#include "core/NoiseFloorTracker.h"
#include "core/StepAttenuatorController.h"
#include "core/session/media/DaemonSpectrumSource.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace NereusSDR {
namespace {

constexpr float kFftDbmFloor = -200.0f;
constexpr float kTrackerFrameIntervalMs = 33.0f;

DaemonSpectrumSourceConfig configFor(const RadioModel& radioModel, int streamIndex)
{
    DaemonSpectrumSourceConfig config;
    // FftPoolConfig's documented defaults are the station-safe 4096 / 30 /
    // Blackman-Harris-4 settings.  Auto AGC has no client display settings
    // and no FFT-size offset, matching RadioModel's existing headless
    // calibration path (m_fftEngine == nullptr).
    config.centreHz = radioModel.streamCentreHz(streamIndex);
    config.sampleRateHz = radioModel.streamSampleRateHz(streamIndex);

    // Two complete 4096-complex-sample frames, expressed in the source's
    // interleaved-float unit.  This remains explicitly bounded and changes
    // mechanically with the configured FFT size; it is not a packet queue.
    config.maxPendingIqFloats = config.fft.fftSize * 4;
    return config;
}

} // namespace

struct DaemonAgcSource::StreamState {
    int streamIndex{-1};
    quint64 measurementGeneration{0};
    std::unique_ptr<NoiseFloorTracker> tracker;
};

DaemonAgcSource::DaemonAgcSource(RadioModel* radioModel, QObject* parent)
    : QObject(parent)
    , m_radioModel(radioModel)
    , m_source(std::make_unique<DaemonSpectrumSource>(this))
{
    // The per-bin dBm conversion runs on the source's engine threads.
    m_source->setComputesBinsDbm(true);
    if (!m_radioModel) {
        return;
    }

    for (const int streamIndex : m_radioModel->suspendedStreams()) {
        m_suspendedStreams.insert(streamIndex);
    }
    m_source->setRadioModel(m_radioModel);
    connect(m_source.get(), &DaemonSpectrumSource::frameAvailable,
            this, &DaemonAgcSource::onFrameAvailable);
    connect(m_radioModel, &RadioModel::streamBindingsChanged,
            this, &DaemonAgcSource::onStreamBindingsChanged);
    connect(m_radioModel, &RadioModel::streamCentreChanged,
            this, &DaemonAgcSource::onStreamCentreChanged);
    connect(m_radioModel, &RadioModel::streamsSuspended,
            this, &DaemonAgcSource::onStreamsSuspended);
    connect(m_radioModel, &RadioModel::streamAdcRoutingChanged,
            this, &DaemonAgcSource::onStreamAdcRoutingChanged);
    connect(m_radioModel, &RadioModel::connectionStateChanged,
            this, &DaemonAgcSource::onConnectionStateChanged);
    connect(&m_radioModel->transmitModel(), &TransmitModel::moxChanged,
            this, &DaemonAgcSource::onMoxChanged);

    // The local tracker resets for attenuation/preamp changes.  The daemon
    // uses the same controller as the station model, so these are source
    // context transitions even where a DDC's centre/rate stays unchanged.
    if (StepAttenuatorController* const att = m_radioModel->stepAttController()) {
        const auto invalidateActive = [this] {
            for (const auto& [streamIndex, unused] : m_streams) {
                Q_UNUSED(unused);
                reconfigureStream(streamIndex);
            }
        };
        connect(att, &StepAttenuatorController::attenuationChanged, this, invalidateActive);
        connect(att, &StepAttenuatorController::preampModeChanged, this, invalidateActive);
    }

    for (SliceModel* const slice : m_radioModel->slices()) {
        observeSlice(slice);
    }
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int sliceId) {
        if (m_radioModel) {
            observeSlice(m_radioModel->sliceById(sliceId));
        }
    });

    reconcileAllStreams();
}

DaemonAgcSource::~DaemonAgcSource()
{
    // RadioModel only borrows tracker pointers.  Clear every registration
    // before StreamState destroys them and before DaemonSpectrumSource joins
    // its worker threads.
    if (m_radioModel) {
        for (const auto& [streamIndex, state] : m_streams) {
            // Make the station value explicitly stale before withdrawing the
            // borrowed tracker pointer.  A remote mirror must never retain a
            // last-good value through source teardown.
            invalidate(*state);
            m_radioModel->clearStreamNoiseFloorTracker(streamIndex, state->tracker.get());
        }
    }
    m_streams.clear();
    m_source.reset();
}

int DaemonAgcSource::activeStreamCount() const
{
    return static_cast<int>(std::count_if(m_streams.cbegin(), m_streams.cend(),
        [this](const auto& pair) { return m_source && m_source->isActive(sourceKey(pair.first)); }));
}

void DaemonAgcSource::onFrameAvailable(MediaSourceKey key)
{
    if (!m_source || key.tier != FftTier::Wide) {
        return;
    }
    const auto it = m_streams.find(key.streamIndex);
    if (it == m_streams.cend() || !canReceiveFromStream(key.streamIndex)) {
        return;
    }
    const std::optional<DaemonSpectrumFrame> frame = m_source->takeLatest(key);
    if (!frame.has_value() || !(frame->source == key) || frame->binsLinear.isEmpty()
        || !std::isfinite(frame->dbmOffset)) {
        return;
    }

    // This is exactly FFTEngine::fftReady's conversion (FFTEngine.cpp):
    // DaemonSpectrumSource carries the raw |X[k]|^2 side channel plus the
    // matching window coherent-gain offset, so no display reducer, station
    // meter offset, percentile estimator, or client calibration participates.
    // The source converts it on its engine thread
    // (DaemonSpectrumSource::binsLinearToDbm), so no per-bin log10 runs here.
    if (!frame->binsDbmValid || frame->binsDbm.size() != frame->binsLinear.size()) {
        return;
    }
    const QVector<float>& binsDbm = frame->binsDbm;

    StreamState& state = *it->second;
    state.tracker->feed(binsDbm, kTrackerFrameIntervalMs);
    publishIfGood(state);
}

void DaemonAgcSource::onStreamBindingsChanged(int streamIndex, const QVector<int>& sliceIndices)
{
    Q_UNUSED(sliceIndices);
    if (hasBoundSlices(streamIndex)) {
        reconfigureStream(streamIndex);
    } else {
        retireStream(streamIndex);
    }
}

void DaemonAgcSource::onStreamCentreChanged(int streamIndex, double, int)
{
    reconfigureStream(streamIndex);
}

void DaemonAgcSource::onStreamsSuspended(const QVector<int>& streamIndices, const QString& reason)
{
    Q_UNUSED(reason);
    QSet<int> next;
    for (const int streamIndex : streamIndices) {
        next.insert(streamIndex);
    }
    const QSet<int> previouslySuspended = m_suspendedStreams;
    m_suspendedStreams = next;
    for (const int streamIndex : m_suspendedStreams) {
        retireStream(streamIndex);
    }
    for (const int streamIndex : previouslySuspended) {
        if (!m_suspendedStreams.contains(streamIndex)) {
            reconcileStream(streamIndex);
        }
    }
}

void DaemonAgcSource::onStreamAdcRoutingChanged()
{
    for (const auto& [streamIndex, unused] : m_streams) {
        Q_UNUSED(unused);
        reconfigureStream(streamIndex);
    }
}

void DaemonAgcSource::onConnectionStateChanged(ConnectionState state)
{
    if (state == ConnectionState::Connected) {
        reconcileAllStreams();
        return;
    }
    for (const auto& [streamIndex, unused] : m_streams) {
        Q_UNUSED(unused);
        retireStream(streamIndex);
    }
}

void DaemonAgcSource::onMoxChanged(bool mox)
{
    if (mox) {
        for (const auto& [streamIndex, unused] : m_streams) {
            Q_UNUSED(unused);
            retireStream(streamIndex);
        }
    } else {
        reconcileAllStreams();
    }
}

void DaemonAgcSource::reconcileAllStreams()
{
    if (!m_radioModel) {
        return;
    }
    QSet<int> streams;
    for (SliceModel* const slice : m_radioModel->slices()) {
        if (slice && slice->streamIndex() >= 0) {
            streams.insert(slice->streamIndex());
        }
    }
    for (const auto& [streamIndex, unused] : m_streams) {
        Q_UNUSED(unused);
        streams.insert(streamIndex);
    }
    for (const int streamIndex : streams) {
        reconcileStream(streamIndex);
    }
}

void DaemonAgcSource::reconcileStream(int streamIndex)
{
    if (!hasBoundSlices(streamIndex)) {
        retireStream(streamIndex);
        return;
    }
    if (!canReceiveFromStream(streamIndex)) {
        if (m_streams.contains(streamIndex)) {
            retireStream(streamIndex);
        }
        return;
    }

    auto [it, inserted] = m_streams.try_emplace(streamIndex, std::make_unique<StreamState>());
    StreamState& state = *it->second;
    if (inserted) {
        state.streamIndex = streamIndex;
        state.tracker = std::make_unique<NoiseFloorTracker>();
    }
    invalidate(state);
    const DaemonSpectrumSourceConfig config = configFor(*m_radioModel, streamIndex);
    const MediaSourceKey key = sourceKey(streamIndex);
    const bool sourceExists = m_source->activeSources().contains(key);
    const bool accepted = sourceExists
        ? m_source->update(key, config)
        : m_source->activate(key, config);
    if (accepted) {
        m_radioModel->setStreamNoiseFloorTracker(streamIndex, state.tracker.get());
    }
}

void DaemonAgcSource::reconfigureStream(int streamIndex)
{
    if (!m_streams.contains(streamIndex)) {
        reconcileStream(streamIndex);
        return;
    }
    if (!canReceiveFromStream(streamIndex) || !hasBoundSlices(streamIndex)) {
        retireStream(streamIndex);
        return;
    }
    StreamState& state = *m_streams.at(streamIndex);
    invalidate(state);
    const MediaSourceKey key = sourceKey(streamIndex);
    const DaemonSpectrumSourceConfig config = configFor(*m_radioModel, streamIndex);
    const bool sourceExists = m_source->activeSources().contains(key);
    const bool accepted = sourceExists
        ? m_source->update(key, config)
        : m_source->activate(key, config);
    if (!accepted) {
        retireStream(streamIndex);
        return;
    }
    m_radioModel->setStreamNoiseFloorTracker(streamIndex, state.tracker.get());
}

void DaemonAgcSource::retireStream(int streamIndex)
{
    const auto it = m_streams.find(streamIndex);
    if (it == m_streams.cend()) {
        return;
    }
    StreamState& state = *it->second;
    invalidate(state);
    if (m_source) {
        m_source->deactivate(sourceKey(streamIndex));
    }
    if (m_radioModel) {
        m_radioModel->clearStreamNoiseFloorTracker(streamIndex, state.tracker.get());
    }
}

void DaemonAgcSource::observeSlice(SliceModel* slice)
{
    if (!slice) {
        return;
    }
    m_observedSliceStreams.insert(slice, slice->streamIndex());

    // Thetis resets its per-receiver noise-floor acquisition after a retune
    // or mode change.  A retune inside an existing DDC window does not emit
    // streamCentreChanged, so these signals are required in addition to the
    // stream-topology hooks above.
    connect(slice, &SliceModel::frequencyChanged, this, [this, slice](double) {
        if (slice) {
            reconfigureStream(slice->streamIndex());
        }
    });
    connect(slice, &SliceModel::dspModeChanged, this, [this, slice](DSPMode) {
        if (slice) {
            reconfigureStream(slice->streamIndex());
        }
    });
    connect(slice, &SliceModel::streamIndexChanged, this, [this, slice](int streamIndex) {
        const int oldStream = m_observedSliceStreams.value(slice, -1);
        m_observedSliceStreams.insert(slice, streamIndex);

        // streamIndexChanged fires after SliceModel has switched its value,
        // so publishInvalid(oldStream) cannot see this slice.  Mark the
        // moved/unbound slice stale directly before a fresh stream can emit
        // a good measurement.  This also prevents an off-stream threshold
        // from surviving a DDC migration.
        quint64 invalidGeneration = 1;
        if (const auto old = m_streams.find(oldStream); old != m_streams.cend()) {
            invalidate(*old->second);
            invalidGeneration = old->second->measurementGeneration;
            if (!hasBoundSlices(oldStream)) {
                retireStream(oldStream);
            }
        }
        slice->setStationAutoAgcNoiseFloor(kFftDbmFloor, false, invalidGeneration);

        if (streamIndex >= 0) {
            reconcileStream(streamIndex);
        }
    });
    connect(slice, &QObject::destroyed, this, [this, slice] {
        const int oldStream = m_observedSliceStreams.take(slice);
        if (oldStream >= 0 && !hasBoundSlices(oldStream)) {
            retireStream(oldStream);
        }
    });
}

void DaemonAgcSource::invalidate(StreamState& state)
{
    ++state.measurementGeneration;
    state.tracker->triggerFastAttack();
    publishInvalid(state);
}

void DaemonAgcSource::publishInvalid(StreamState& state)
{
    if (!m_radioModel) {
        return;
    }
    for (SliceModel* const slice : m_radioModel->slices()) {
        if (slice && slice->streamIndex() == state.streamIndex) {
            slice->setStationAutoAgcNoiseFloor(kFftDbmFloor, false,
                                                state.measurementGeneration);
        }
    }
}

void DaemonAgcSource::publishIfGood(StreamState& state)
{
    if (!m_radioModel || !state.tracker->isGood()) {
        return;
    }
    const float floorDbm = state.tracker->noiseFloor();
    for (SliceModel* const slice : m_radioModel->slices()) {
        if (slice && slice->streamIndex() == state.streamIndex) {
            slice->setStationAutoAgcNoiseFloor(floorDbm, true,
                                                state.measurementGeneration);
        }
    }
}

bool DaemonAgcSource::canReceiveFromStream(int streamIndex) const
{
    return m_radioModel && m_radioModel->isConnected()
        && !m_radioModel->transmitModel().isMox()
        && !m_suspendedStreams.contains(streamIndex)
        && m_radioModel->streamActive(streamIndex);
}

bool DaemonAgcSource::hasBoundSlices(int streamIndex) const
{
    if (!m_radioModel) {
        return false;
    }
    const QList<SliceModel*> slices = m_radioModel->slices();
    return std::any_of(slices.cbegin(), slices.cend(),
                       [streamIndex](const SliceModel* slice) {
        return slice && slice->streamIndex() == streamIndex;
    });
}

MediaSourceKey DaemonAgcSource::sourceKey(int streamIndex)
{
    return {streamIndex, FftTier::Wide};
}

} // namespace NereusSDR
