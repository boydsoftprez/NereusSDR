// =================================================================
// src/core/session/media/DaemonSpectrumSource.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. See DaemonSpectrumSource.h.
// =================================================================

// Modification history (NereusSDR):
// 2026-09-27: Use the approved shared decimation bounds.
// J.J. Boyd (KG4VCF), AI-assisted implementation via OpenAI Codex.
// 2026-09-30: Frames can carry their bins in dBm, converted on the engine
// thread, so DaemonAgcSource does no per-bin log10 on the main thread.
// J.J. Boyd (KG4VCF), AI-assisted implementation via Anthropic Claude Code.

#include "core/ControlRanges.h"
#include "core/session/media/DaemonSpectrumSource.h"

#include "core/FFTEngine.h"
#include "models/RadioModel.h"

#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>

#include <chrono>
#include <cmath>
#include <utility>

namespace NereusSDR {

namespace {

// Moved unchanged from DaemonAgcSource.cpp, which had them for the same
// conversion (FFTEngine::fftReady's floor).
constexpr float kFftPowerFloor = 1.0e-20f;
constexpr float kFftDbmFloor = -200.0f;

} // namespace

struct DaemonSpectrumSource::FrameState {
    mutable QMutex mutex;
    DaemonSpectrumSourceConfig config;
    DaemonSpectrumFrame latest;
    quint64 generation{0};
    bool active{false};
    bool retired{false};
    bool configurationPending{true};
    bool hasLatest{false};
    bool notificationQueued{false};
    bool iqDrainQueued{false};
    bool inputDiscontinuity{false};
    int maxPendingIqFloats{0};
    QVector<float> pendingIq;
    quint64 inputFramesDropped{0};
    quint64 completedInputHandoffs{0};
    quint64 publishedFrames{0};
};

struct DaemonSpectrumSource::SourceEntry {
    MediaSourceKey key;
    QPointer<FFTEngine> engine;
    QSharedPointer<FrameState> state;
    QMetaObject::Connection iqConnection;
    QMetaObject::Connection frameConnection;
};

DaemonSpectrumSource::DaemonSpectrumSource(QObject* parent)
    : QObject(parent)
    , m_pool(std::make_unique<FftEnginePool>(nullptr, false))
{
}

DaemonSpectrumSource::~DaemonSpectrumSource()
{
    // Mark every state inert before stopping the pool.  The pool then quits
    // and waits on its workers before source destruction can discard any
    // callback context, so an engine-thread callback cannot dereference a
    // freed engine or enqueue a fresh source notification during teardown.
    for (SourceEntry& entry : m_sources) {
        QMutexLocker lock(&entry.state->mutex);
        entry.state->active = false;
        entry.state->retired = true;
        entry.state->configurationPending = true;
        entry.state->hasLatest = false;
        entry.state->notificationQueued = false;
        entry.state->pendingIq.clear();
    }
    // R-R3-49: take the radio's I/Q route down before any engine goes, as
    // deactivate() does. The radio emits on its connection thread straight
    // into each engine; were an engine destroyed while still connected,
    // Qt's ~QObject would clear the route's slot object under an emission
    // already in flight and that thread would then call into the dying
    // engine (the Core crashed this way at stop while the radio streamed).
    for (SourceEntry& entry : m_sources) {
        QObject::disconnect(entry.iqConnection);
        entry.iqConnection = {};
    }
    m_sources.clear();
    m_pool.reset();
}

void DaemonSpectrumSource::setRadioModel(RadioModel* radioModel)
{
    if (m_radioModel == radioModel) {
        return;
    }
    for (SourceEntry& entry : m_sources) {
        QObject::disconnect(entry.iqConnection);
        entry.iqConnection = {};
    }
    m_radioModel = radioModel;
    for (SourceEntry& entry : m_sources) {
        attachRadioIq(entry);
    }
}

bool DaemonSpectrumSource::activate(const MediaSourceKey& key,
                                    const DaemonSpectrumSourceConfig& config)
{
    if (!isValidConfig(key, config) || m_sources.contains(key)) {
        return false;
    }

    FFTEngine* engine = m_pool->engineForSource(key, config.fft);
    if (!engine) {
        return false;
    }

    SourceEntry entry;
    entry.key = key;
    entry.engine = engine;
    entry.state = QSharedPointer<FrameState>::create();
    entry.state->config = config;
    entry.state->generation = ++m_nextGeneration;
    entry.state->maxPendingIqFloats = config.maxPendingIqFloats;

    entry.frameConnection = connect(
        engine, &FFTEngine::fftReadyLinear, this,
        [this, key, state = entry.state](int receiverId,
                                         const QVector<float>& binsLinear,
                                         double windowEnb,
                                         double dbmOffset) {
            if (receiverId == key.streamIndex) {
                publishFrame(key, state, binsLinear, windowEnb, dbmOffset);
            }
        },
        Qt::DirectConnection);

    auto inserted = m_sources.insert(key, std::move(entry));
    attachRadioIq(inserted.value());
    queueConfiguredActivation(inserted.value(), config, inserted->state->generation,
                              true);
    return true;
}

bool DaemonSpectrumSource::update(const MediaSourceKey& key,
                                  const DaemonSpectrumSourceConfig& config)
{
    if (!isValidConfig(key, config)) {
        return false;
    }
    auto it = m_sources.find(key);
    if (it == m_sources.end() || !it->engine) {
        return false;
    }

    quint64 generation = 0;
    bool resetInputHistory = false;
    {
        QMutexLocker lock(&it->state->mutex);
        resetInputHistory = !inputHistoryIsCompatible(it->state->config, config);
        it->state->active = false;
        it->state->configurationPending = true;
        it->state->hasLatest = false;
        it->state->notificationQueued = false;
        it->state->config = config;
        it->state->maxPendingIqFloats = config.maxPendingIqFloats;
        if (resetInputHistory && !it->state->pendingIq.isEmpty()) {
            it->state->pendingIq.clear();
            ++it->state->inputFramesDropped;
        }
        generation = ++m_nextGeneration;
        it->state->generation = generation;
    }
    queueConfiguredActivation(it.value(), config, generation, resetInputHistory);
    return true;
}

bool DaemonSpectrumSource::updateFrameRate(const MediaSourceKey& key, int fps,
                                           bool transformsFollowFrameRate)
{
    auto it = m_sources.find(key);
    if (it == m_sources.end() || !it->engine) {
        return false;
    }
    DaemonSpectrumSourceConfig config;
    {
        QMutexLocker lock(&it->state->mutex);
        config = it->state->config;
    }
    config.fft.fps = fps;
    config.transformsFollowFrameRate = transformsFollowFrameRate;
    if (!isValidConfig(key, config)) {
        return false;
    }
    {
        QMutexLocker lock(&it->state->mutex);
        it->state->config.fft.fps = fps;
        it->state->config.transformsFollowFrameRate = transformsFollowFrameRate;
    }
    // Queued behind any pending full configuration, which carries the rate
    // it was built with, so this newer rate is the one left applied.
    QMetaObject::invokeMethod(it->engine, [engine = it->engine, fps,
                                           transformsFollowFrameRate]() {
        if (engine) {
            engine->setTransformsFollowFrameRate(transformsFollowFrameRate);
            engine->setOutputFps(fps);
        }
    }, Qt::QueuedConnection);
    return true;
}

void DaemonSpectrumSource::deactivate(const MediaSourceKey& key)
{
    auto it = m_sources.find(key);
    if (it == m_sources.end()) {
        return;
    }

    {
        QMutexLocker lock(&it->state->mutex);
        it->state->active = false;
        it->state->retired = true;
        it->state->configurationPending = true;
        it->state->hasLatest = false;
        it->state->notificationQueued = false;
        it->state->pendingIq.clear();
    }
    QObject::disconnect(it->iqConnection);
    QObject::disconnect(it->frameConnection);
    m_pool->removeSource(key);
    m_sources.erase(it);
}

void DaemonSpectrumSource::deactivateStream(int streamIndex)
{
    const QList<MediaSourceKey> keys = m_sources.keys();
    for (const MediaSourceKey& key : keys) {
        if (key.streamIndex == streamIndex) {
            deactivate(key);
        }
    }
}

bool DaemonSpectrumSource::isActive(const MediaSourceKey& key) const
{
    const auto it = m_sources.constFind(key);
    if (it == m_sources.cend()) {
        return false;
    }
    QMutexLocker lock(&it->state->mutex);
    return it->state->active && !it->state->configurationPending;
}

QList<MediaSourceKey> DaemonSpectrumSource::activeSources() const
{
    return m_sources.keys();
}

quint64 DaemonSpectrumSource::droppedInputFrames(const MediaSourceKey& key) const
{
    const auto it = m_sources.constFind(key);
    if (it == m_sources.cend()) {
        return 0;
    }
    QMutexLocker lock(&it->state->mutex);
    return it->state->inputFramesDropped;
}

quint64 DaemonSpectrumSource::publishedFrames(const MediaSourceKey& key) const
{
    const auto it = m_sources.constFind(key);
    if (it == m_sources.cend()) {
        return 0;
    }
    QMutexLocker lock(&it->state->mutex);
    return it->state->publishedFrames;
}

DaemonSpectrumSource::InputQueueDiagnostics
DaemonSpectrumSource::inputQueueDiagnostics(const MediaSourceKey& key) const
{
    const auto it = m_sources.constFind(key);
    if (it == m_sources.cend()) { return {}; }
    QMutexLocker lock(&it->state->mutex);
    return {it->state->generation, it->state->active,
            it->state->configurationPending, it->state->iqDrainQueued,
            static_cast<int>(it->state->pendingIq.size()), it->state->maxPendingIqFloats};
}

int DaemonSpectrumSource::engineDecimation(const MediaSourceKey& key) const
{
    const auto it = m_sources.constFind(key);
    if (it == m_sources.cend() || !it->engine) {
        return 0;
    }
    // FFTEngine keeps it in an atomic: safe to read off its thread.
    return it->engine->decimation();
}

quint64 DaemonSpectrumSource::completedInputHandoffs(const MediaSourceKey& key) const
{
    const auto it = m_sources.constFind(key);
    if (it == m_sources.cend()) {
        return 0;
    }
    QMutexLocker lock(&it->state->mutex);
    return it->state->completedInputHandoffs;
}

std::optional<DaemonSpectrumFrame>
DaemonSpectrumSource::takeLatest(const MediaSourceKey& key)
{
    const auto it = m_sources.constFind(key);
    if (it == m_sources.cend()) {
        return std::nullopt;
    }
    QMutexLocker lock(&it->state->mutex);
    if (!it->state->hasLatest) {
        return std::nullopt;
    }
    it->state->hasLatest = false;
    it->state->notificationQueued = false;
    return it->state->latest;
}

void DaemonSpectrumSource::submitIq(int streamIndex,
                                    const QVector<float>& interleavedIq)
{
    if (streamIndex < 0 || interleavedIq.isEmpty()) {
        return;
    }
    for (auto it = m_sources.cbegin(); it != m_sources.cend(); ++it) {
        const SourceEntry& entry = it.value();
        if (!entry.engine || it.key().streamIndex != streamIndex) {
            continue;
        }
        enqueueIq(entry.engine, entry.state, interleavedIq);
    }
}

bool DaemonSpectrumSource::isValidConfig(
    const MediaSourceKey& key, const DaemonSpectrumSourceConfig& config)
{
    if (key.streamIndex < 0
        || (key.tier != FftTier::Wide && key.tier != FftTier::Fine)
        || !std::isfinite(config.centreHz)
        || !(config.sampleRateHz > 0.0) || !std::isfinite(config.sampleRateHz)) {
        return false;
    }
    const int fftSize = config.fft.fftSize;
    if (fftSize < 1024 || (fftSize & (fftSize - 1)) != 0) {
        return false;
    }
    // FFTEngine owns the maximum supported size.  Ask its public setter
    // rather than duplicating a private DSP bound in the daemon layer.
    FFTEngine validator(-1);
    validator.setFftSize(fftSize);
    if (validator.fftSize() != fftSize) {
        return false;
    }
    if (config.fft.fps < 1 || config.fft.fps > 60
        || config.fft.windowType < static_cast<int>(WindowFunction::Rectangular)
        || config.fft.windowType >= static_cast<int>(WindowFunction::Count)
        || !std::isfinite(config.fft.hzPerBinTarget)
        || config.fft.hzPerBinTarget < 0.0
        || config.decimation < ControlRanges::kDisplayDecimationMin
        || config.decimation > ControlRanges::kDisplayDecimationMax) {
        return false;
    }
    if (config.maxPendingIqFloats <= 0 || (config.maxPendingIqFloats % 2) != 0) {
        return false;
    }
    return true;
}

bool DaemonSpectrumSource::inputHistoryIsCompatible(
    const DaemonSpectrumSourceConfig& before,
    const DaemonSpectrumSourceConfig& after)
{
    // Output cadence and the optional auto-zoom target do not change what
    // RF sample a stored I/Q pair represents. The RF tuning, sample rate,
    // FFT size and window do; retaining overlap across any of those would
    // make a new generation describe old-context samples.
    return before.centreHz == after.centreHz
        && before.sampleRateHz == after.sampleRateHz
        && before.fft.fftSize == after.fft.fftSize
        && before.fft.windowType == after.fft.windowType
        && before.decimation == after.decimation;
}

qint64 DaemonSpectrumSource::monotonicNowNs()
{
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}

void DaemonSpectrumSource::applyEngineConfig(
    FFTEngine* engine, const DaemonSpectrumSourceConfig& config)
{
    engine->setOutputFps(config.fft.fps);
    engine->setTransformsFollowFrameRate(config.transformsFollowFrameRate);
    engine->setFftSizeBaseline(config.fft.fftSize);
    engine->setFftSize(config.fft.fftSize);
    engine->setWindowFunction(static_cast<WindowFunction>(config.fft.windowType));
    engine->setHzPerBinTarget(config.fft.hzPerBinTarget);
    engine->setDecimation(config.decimation);
    engine->setSampleRate(config.sampleRateHz);
}

void DaemonSpectrumSource::enqueueIq(
    FFTEngine* engine, const QSharedPointer<FrameState>& state,
    const QVector<float>& interleavedIq)
{
    if (!engine || interleavedIq.isEmpty() || (interleavedIq.size() % 2) != 0) {
        return;
    }
    bool queueDrain = false;
    {
        QMutexLocker lock(&state->mutex);
        if (state->retired || !state->active || state->configurationPending
            || interleavedIq.size() > state->maxPendingIqFloats) {
            // The discarded whole packet may separate already queued I/Q
            // from the next accepted packet. Never join samples across that
            // unknown gap in the FFT overlap ring.
            state->pendingIq.clear();
            state->inputDiscontinuity = true;
            ++state->inputFramesDropped;
            return;
        }
        if (state->pendingIq.size() > state->maxPendingIqFloats - interleavedIq.size()) {
            // A slow worker must still make forward progress. Retire the
            // older pending samples, mark the gap for FFTEngine's overlap
            // ring, and keep this individually valid whole packet. The
            // already queued drain (if any) will take the replacement.
            state->pendingIq.clear();
            state->inputDiscontinuity = true;
            ++state->inputFramesDropped;
        }
        state->pendingIq += interleavedIq;
        if (!state->iqDrainQueued) {
            state->iqDrainQueued = true;
            queueDrain = true;
        }
        if (queueDrain) {
            // Posting while the state lock is held closes the DirectConnection
            // retirement race: deactivate() first takes this same lock and
            // marks the state retired before it disconnects/deletes engine.
            // Thus a raw-I/Q callback can either post to a live engine or see
            // retired, never post through a deleted raw pointer.
            QMetaObject::invokeMethod(engine, [engine, state]() {
            QVector<float> iq;
            bool resetInputHistory = false;
            {
                QMutexLocker lock(&state->mutex);
                if (state->retired || !state->active
                    || state->configurationPending) {
                    state->pendingIq.clear();
                    state->iqDrainQueued = false;
                    return;
                }
                resetInputHistory = state->inputDiscontinuity;
                state->inputDiscontinuity = false;
                iq.swap(state->pendingIq);
                state->iqDrainQueued = false;
            }
            if (resetInputHistory) {
                engine->resetInputHistory();
            }
            if (!iq.isEmpty()) {
                engine->feedIQ(iq);
                QMutexLocker lock(&state->mutex);
                ++state->completedInputHandoffs;
            }
            }, Qt::QueuedConnection);
        }
    }
}

void DaemonSpectrumSource::attachRadioIq(SourceEntry& entry)
{
    if (!m_radioModel || !entry.engine || entry.iqConnection) {
        return;
    }
    const int streamIndex = entry.key.streamIndex;
    entry.iqConnection = connect(
        m_radioModel, &RadioModel::rawIqDataForStream, entry.engine,
        [engine = entry.engine, state = entry.state,
         streamIndex](int receivedStream, const QVector<float>& samples) {
            if (receivedStream == streamIndex) {
                enqueueIq(engine, state, samples);
            }
        }, Qt::DirectConnection);
}

void DaemonSpectrumSource::queueConfiguredActivation(
    SourceEntry& entry, const DaemonSpectrumSourceConfig& config,
    quint64 generation, bool resetInputHistory)
{
    const QSharedPointer<FrameState> state = entry.state;
    QMetaObject::invokeMethod(entry.engine,
                              [engine = entry.engine, state, config, generation,
                               resetInputHistory]() {
        if (!engine) {
            return;
        }
        applyEngineConfig(engine, config);
        if (resetInputHistory) {
            engine->resetInputHistory();
        }
        QMutexLocker lock(&state->mutex);
        if (!state->retired && state->generation == generation) {
            state->configurationPending = false;
            state->active = true;
        }
    }, Qt::QueuedConnection);
}

bool DaemonSpectrumSource::publishFrameForTest(const MediaSourceKey& key,
                                               qint64 producedAtNs, float binLinear)
{
    const auto it = m_sources.constFind(key);
    if (it == m_sources.cend()) {
        return false;
    }
    const QSharedPointer<FrameState> state = it->state;
    {
        QMutexLocker lock(&state->mutex);
        if (!state->active || state->configurationPending) {
            return false;
        }
        state->latest.source = key;
        state->latest.generation = state->generation;
        state->latest.centreHz = state->config.centreHz;
        state->latest.sampleRateHz = state->config.sampleRateHz;
        state->latest.producedAtNs = producedAtNs;
        state->latest.binsLinear = QVector<float>(state->config.fft.fftSize, binLinear);
        state->latest.windowEnb = 1.0;
        state->latest.dbmOffset = 0.0;
        state->latest.binsDbm.clear();
        state->latest.binsDbmValid = computesBinsDbm()
            && binsLinearToDbm(state->latest.binsLinear, 0.0, state->latest.binsDbm);
        state->hasLatest = true;
        state->notificationQueued = true;
        ++state->publishedFrames;
    }
    emit frameAvailable(key);
    return true;
}

bool DaemonSpectrumSource::publishFrameForTest(const MediaSourceKey& key,
                                               qint64 producedAtNs,
                                               const QVector<float>& binsLinear)
{
    const auto it = m_sources.constFind(key);
    if (it == m_sources.cend()) {
        return false;
    }
    const QSharedPointer<FrameState> state = it->state;
    {
        QMutexLocker lock(&state->mutex);
        if (!state->active || state->configurationPending
            || binsLinear.size() != state->config.fft.fftSize) {
            return false;
        }
        state->latest.source = key;
        state->latest.generation = state->generation;
        state->latest.centreHz = state->config.centreHz;
        state->latest.sampleRateHz = state->config.sampleRateHz;
        state->latest.producedAtNs = producedAtNs;
        state->latest.binsLinear = binsLinear;
        state->latest.windowEnb = 1.0;
        state->latest.dbmOffset = 0.0;
        state->latest.binsDbm.clear();
        state->latest.binsDbmValid = computesBinsDbm()
            && binsLinearToDbm(binsLinear, 0.0, state->latest.binsDbm);
        state->hasLatest = true;
        state->notificationQueued = true;
        ++state->publishedFrames;
    }
    emit frameAvailable(key);
    return true;
}

void DaemonSpectrumSource::publishFrame(
    const MediaSourceKey& key, const QSharedPointer<FrameState>& state,
    const QVector<float>& binsLinear, double windowEnb, double dbmOffset)
{
    // On the engine thread, outside the frame lock.
    QVector<float> binsDbm;
    bool binsDbmValid = false;
    if (computesBinsDbm()) {
        binsDbmValid = binsLinearToDbm(binsLinear, dbmOffset, binsDbm);
    }
    bool queueNotification = false;
    {
        QMutexLocker lock(&state->mutex);
        if (!state->active || state->configurationPending) {
            return;
        }
        state->latest.source = key;
        state->latest.generation = state->generation;
        state->latest.centreHz = state->config.centreHz;
        state->latest.sampleRateHz = state->config.sampleRateHz;
        state->latest.producedAtNs = monotonicNowNs();
        state->latest.binsLinear = binsLinear;
        state->latest.windowEnb = windowEnb;
        state->latest.dbmOffset = dbmOffset;
        state->latest.binsDbm = std::move(binsDbm);
        state->latest.binsDbmValid = binsDbmValid;
        state->hasLatest = true;
        ++state->publishedFrames;
        if (!state->notificationQueued) {
            state->notificationQueued = true;
            queueNotification = true;
        }
    }
    if (queueNotification) {
        QMetaObject::invokeMethod(this, [this, key, state]() {
            QMutexLocker lock(&state->mutex);
            const bool emitNow = state->active && state->hasLatest
                                 && state->notificationQueued;
            lock.unlock();
            if (emitNow) {
                emit frameAvailable(key);
            }
        }, Qt::QueuedConnection);
    }
}

void DaemonSpectrumSource::setComputesBinsDbm(bool computes)
{
    m_computesBinsDbm.store(computes, std::memory_order_release);
}

bool DaemonSpectrumSource::computesBinsDbm() const
{
    return m_computesBinsDbm.load(std::memory_order_acquire);
}

bool DaemonSpectrumSource::binsLinearToDbm(const QVector<float>& binsLinear,
                                           double dbmOffset, QVector<float>& out)
{
    // Moved unchanged from DaemonAgcSource::onFrameAvailable: this is
    // exactly FFTEngine::fftReady's conversion (FFTEngine.cpp).
    out.clear();
    out.reserve(binsLinear.size());
    for (const float power : binsLinear) {
        if (!std::isfinite(power)) {
            out.clear();
            return false;
        }
        const float dbm = power < kFftPowerFloor
            ? kFftDbmFloor
            : static_cast<float>(10.0 * std::log10(static_cast<double>(power))
                                 + dbmOffset);
        if (!std::isfinite(dbm)) {
            out.clear();
            return false;
        }
        out.append(dbm);
    }
    return true;
}

} // namespace NereusSDR
