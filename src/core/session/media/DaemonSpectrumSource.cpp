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
// 2026-10-10: resetInputHistoryAll, the FFT input purge on a MOX edge.
// J.J. Boyd (KG4VCF), AI-assisted implementation via Anthropic Claude Code.
// 2026-10-11: R-DISP-RETUNE: a caller can ask that a change of the center
// alone relabels the running source and keeps its input history. Without
// that the source restarts as before.
// J.J. Boyd (KG4VCF), AI-assisted implementation via Anthropic Claude Code.
// 2026-10-15: Display send thread: setFrameWake(), a published frame wakes
// the display thread instead of queueing frameAvailable to the main thread;
// currentGeneration(), so a frame held for the main thread is dropped after
// a retune.
// J.J. Boyd (KG4VCF), AI-assisted implementation via Anthropic Claude Code.
// 2026-10-21: Frames are stamped with the time of their samples
// (SpectrumSampleClock) in place of the FFT's publish time. Input queue
// overflows are counted and logged, the engine's 5 ms skips and the clock's
// re-anchors read out.
// J.J. Boyd (KG4VCF), AI-assisted implementation via Anthropic Claude Code.
// 2026-10-22: RadioModel::iqSequenceGapForStream marks the stream's sources
// through the dropped-packet path (inputDiscontinuity), so a lost I/Q packet
// does not paint a line across the spectrum (after Longpath PR 62).
// J.J. Boyd (KG4VCF), with AI-assisted transformation via Claude Code.
// 2026-10-23: a gap is marked at its place in the queued I/Q (gapAtFloat)
// instead, and the drain asks the engine for the same bounded window reset
// the desktop app's engines take (FFTEngine::applyPendingWindowReset), so
// the Core and the app behave alike; engineWindowResets and
// engineWindowResetsSkipped read the engine's counts. J.J. Boyd (KG4VCF),
// with AI-assisted implementation via Claude Code.
// 2026-10-24: every gap queued before a drain keeps its own place
// (gapsAtFloat, in order), and the drain applies each at its sample
// position under the engine's rule, which now keeps letting gaps through
// on a lossy link until a whole window passes with none (JJ's ruling,
// option i). J.J. Boyd (KG4VCF), with AI-assisted implementation via
// Claude Code.
// 2026-10-24: the engine's rule is now option c (JJ's ruling): a gap
// within four windows of the last one is let through once a frame has
// come since the last reset; the drain's comment says so. J.J. Boyd
// (KG4VCF), with AI-assisted implementation via Claude Code.
// 2026-10-26: setEngineFrameGapClockForTest. J.J. Boyd (KG4VCF), with
// AI-assisted implementation via Claude Code.
// 2026-10-26: setEngineFrameGapClockForTest refuses once a drain has
// started (inputDrainStarted, under the mutex) and sets the clock under
// it. J.J. Boyd (KG4VCF), with AI-assisted implementation via Claude Code.

#include "core/ControlRanges.h"
#include "core/session/media/DaemonSpectrumSource.h"

#include "core/FFTEngine.h"
#include "core/LogCategories.h"
#include "core/session/media/SpectrumSampleClock.h"
#include "models/RadioModel.h"

#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>

namespace NereusSDR {

namespace {

// Moved unchanged from DaemonAgcSource.cpp, which had them for the same
// conversion (FFTEngine::fftReady's floor).
constexpr float kFftPowerFloor = 1.0e-20f;
constexpr float kFftDbmFloor = -200.0f;

// The input queue overflow warning's interval: one line at most this often,
// with the overflows since the last.
constexpr qint64 kQueueOverflowWarningIntervalNs = 10'000'000'000LL;

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
    /// Where in pendingIq each I/Q sequence gap from the radio falls (the
    /// floats before it came before the gap), in the order they came, one
    /// entry per place. Cleared wherever pendingIq is emptied, for a
    /// discontinuity, which resets the input history whole, or on retire.
    QVector<int> gapsAtFloat;
    quint64 inputFramesDropped{0};
    quint64 completedInputHandoffs{0};
    /// A drain has taken input for the engine (set under the mutex as it
    /// takes it, before feedIQ): setEngineFrameGapClockForTest refuses
    /// from then on.
    bool inputDrainStarted{false};
    quint64 publishedFrames{0};
    quint64 inputHistoryResets{0};
    /// Frames replaced in `latest` since the last one taken, with nobody
    /// having taken them (DaemonSpectrumFrame::replacedBefore).
    quint32 replacedSinceTaken{0};
    /// Each frame's producedAtNs: the time of its samples. Reset wherever
    /// the samples stop being continuous: the engine takes a new
    /// configuration, or its input history is cleared for a discontinuity.
    SpectrumSampleClock sampleClock;
    /// The last published frame's producedAtNs, for the next frame's
    /// sourceFrameGapNs; cleared with the clock.
    bool hasPublishedStamp{false};
    qint64 lastPublishedStampNs{0};
    /// Full queues emptied for a new packet, and the warning's state.
    quint64 inputQueueOverflows{0};
    quint64 inputQueueOverflowsWarned{0};
    qint64 inputQueueOverflowWarnedAtNs{0};
    bool inputQueueOverflowWarned{false};
};

struct DaemonSpectrumSource::SourceEntry {
    MediaSourceKey key;
    QPointer<FFTEngine> engine;
    QSharedPointer<FrameState> state;
    QMetaObject::Connection iqConnection;
    /// RadioModel::iqSequenceGapForStream for this stream (attachRadioIq);
    /// made and taken down with iqConnection.
    QMetaObject::Connection gapConnection;
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
        entry.state->replacedSinceTaken = 0;
        entry.state->notificationQueued = false;
        entry.state->pendingIq.clear();
        entry.state->gapsAtFloat.clear();
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
        QObject::disconnect(entry.gapConnection);
        entry.gapConnection = {};
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
        QObject::disconnect(entry.gapConnection);
        entry.gapConnection = {};
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
        [this, key, engine, state = entry.state](int receiverId,
                                                 const QVector<float>& binsLinear,
                                                 double windowEnb,
                                                 double dbmOffset) {
            if (receiverId == key.streamIndex) {
                // On the engine's thread, inside its emit: the engine is
                // alive and frameInputPosition() is this frame's.
                publishFrame(key, state, binsLinear, windowEnb, dbmOffset,
                             engine->frameInputPosition(), monotonicNowNs());
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
                                  const DaemonSpectrumSourceConfig& config,
                                  DaemonSpectrumCenterMove centerMove,
                                  DaemonSpectrumSourceUpdate* result)
{
    if (result) {
        *result = {};
    }
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
        const quint64 previous = it->state->generation;
        if (centerMove == DaemonSpectrumCenterMove::Relabel
            && it->state->active && !it->state->configurationPending
            && onlyCenterDiffers(it->state->config, config)) {
            // R-DISP-RETUNE: the receiver was retuned, nothing else changed
            // and the caller is the display, which asked for the picture to
            // carry on. A retune changes which RF frequency the stored
            // samples came from, so one transform after it can mix samples
            // from before and after the step. That is JJ's choice for the
            // display (2026-10-11): it carries on through a retune as
            // NereusSDR's own local display does (its FFT path never calls
            // FFTEngine::resetInputHistory) and as Thetis, piHPSDR,
            // deskHPSDR and AetherSDR do. Dropping the history instead
            // would be clean, and would freeze the picture for as long as a
            // tuning wheel turns faster than one transform fills (up to
            // 0.7 s at deep zoom). A caller that measures does not ask for
            // this and takes the restart below.
            //
            // The engine does not use the center: it is a label put
            // on each frame as it is published (publishFrame reads it under
            // this mutex). So the running source takes the new label and a
            // new generation here and carries on: no activation hop, queued
            // I/Q and the engine's overlap history kept, nothing dropped.
            // The one frame it may still hold was labeled before the move;
            // it is discarded, so takeLatest() never hands a consumer the
            // old center after this call. A frame the engine is producing
            // now takes the lock after this and gets the new label.
            it->state->config.centreHz = config.centreHz;
            generation = ++m_nextGeneration;
            it->state->generation = generation;
            it->state->hasLatest = false;
            it->state->replacedSinceTaken = 0;
            it->state->notificationQueued = false;
            if (result) {
                *result = {true, previous, generation};
            }
            return true;
        }
        if (result) {
            *result = {false, previous, m_nextGeneration + 1};
        }
        resetInputHistory = !inputHistoryIsCompatible(it->state->config, config);
        it->state->active = false;
        it->state->configurationPending = true;
        it->state->hasLatest = false;
        it->state->replacedSinceTaken = 0;
        it->state->notificationQueued = false;
        it->state->config = config;
        it->state->maxPendingIqFloats = config.maxPendingIqFloats;
        if (resetInputHistory && !it->state->pendingIq.isEmpty()) {
            it->state->pendingIq.clear();
            ++it->state->inputFramesDropped;
        }
        if (resetInputHistory) {
            it->state->gapsAtFloat.clear();
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
        it->state->replacedSinceTaken = 0;
        it->state->notificationQueued = false;
        it->state->pendingIq.clear();
        it->state->gapsAtFloat.clear();
    }
    QObject::disconnect(it->iqConnection);
    QObject::disconnect(it->gapConnection);
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

quint64 DaemonSpectrumSource::currentGeneration(const MediaSourceKey& key) const
{
    const auto it = m_sources.constFind(key);
    if (it == m_sources.cend()) { return 0; }
    QMutexLocker lock(&it->state->mutex);
    return it->state->generation;
}

bool DaemonSpectrumSource::frameWaitingForTest(const MediaSourceKey& key) const
{
    const auto it = m_sources.constFind(key);
    if (it == m_sources.cend()) { return false; }
    QMutexLocker lock(&it->state->mutex);
    return it->state->hasLatest;
}

quint64 DaemonSpectrumSource::inputQueueOverflows(const MediaSourceKey& key) const
{
    const auto it = m_sources.constFind(key);
    if (it == m_sources.cend()) { return 0; }
    QMutexLocker lock(&it->state->mutex);
    return it->state->inputQueueOverflows;
}

quint64 DaemonSpectrumSource::engineMinimumGapSkips(const MediaSourceKey& key) const
{
    const auto it = m_sources.constFind(key);
    if (it == m_sources.cend() || !it->engine) { return 0; }
    // An atomic in FFTEngine: safe to read off its thread.
    return it->engine->minimumGapSkips();
}

bool DaemonSpectrumSource::setEngineFrameGapClockForTest(const MediaSourceKey& key,
                                                         std::function<qint64()> clockMs)
{
    const auto it = m_sources.constFind(key);
    if (it == m_sources.cend() || !it->engine) { return false; }
    // Before any input, so the worker thread is not reading it yet: a
    // drain marks its start under this mutex before it feeds the engine,
    // and the clock is set under it too, so a drain that starts later sees
    // it.
    QMutexLocker lock(&it->state->mutex);
    if (it->state->inputDrainStarted) {
        return false;
    }
    it->engine->setFrameGapClockForTest(std::move(clockMs));
    return true;
}

quint64 DaemonSpectrumSource::engineWindowResets(const MediaSourceKey& key) const
{
    const auto it = m_sources.constFind(key);
    if (it == m_sources.cend() || !it->engine) { return 0; }
    // An atomic in FFTEngine: safe to read off its thread.
    return it->engine->windowResets();
}

quint64 DaemonSpectrumSource::engineWindowResetsSkipped(const MediaSourceKey& key) const
{
    const auto it = m_sources.constFind(key);
    if (it == m_sources.cend() || !it->engine) { return 0; }
    // An atomic in FFTEngine: safe to read off its thread.
    return it->engine->windowResetsSkipped();
}

quint64 DaemonSpectrumSource::sampleClockReanchors(const MediaSourceKey& key) const
{
    const auto it = m_sources.constFind(key);
    if (it == m_sources.cend()) { return 0; }
    QMutexLocker lock(&it->state->mutex);
    return it->state->sampleClock.largeErrorReanchors();
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
    DaemonSpectrumFrame frame = it->state->latest;
    frame.replacedBefore = std::exchange(it->state->replacedSinceTaken, 0);
    return frame;
}

void DaemonSpectrumSource::setFrameWake(std::function<void()> wake)
{
    const std::lock_guard<std::mutex> lock(m_frameWakeMutex);
    m_frameWake = std::move(wake);
}

bool DaemonSpectrumSource::callFrameWake()
{
    const std::lock_guard<std::mutex> lock(m_frameWakeMutex);
    if (!m_frameWake) {
        return false;
    }
    m_frameWake();
    return true;
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
    //
    // R-DISP-RETUNE: the center is in this list. The display's relabel of a
    // center-only move never reaches this function (see update()).
    return before.centreHz == after.centreHz
        && before.sampleRateHz == after.sampleRateHz
        && before.fft.fftSize == after.fft.fftSize
        && before.fft.windowType == after.fft.windowType
        && before.decimation == after.decimation;
}

bool DaemonSpectrumSource::onlyCenterDiffers(
    const DaemonSpectrumSourceConfig& before,
    const DaemonSpectrumSourceConfig& after)
{
    return before.centreHz != after.centreHz
        && before.sampleRateHz == after.sampleRateHz
        && before.fft.fps == after.fft.fps
        && before.fft.fftSize == after.fft.fftSize
        && before.fft.windowType == after.fft.windowType
        && before.fft.hzPerBinTarget == after.fft.hzPerBinTarget
        && before.fft.threadCount == after.fft.threadCount
        && before.maxPendingIqFloats == after.maxPendingIqFloats
        && before.transformsFollowFrameRate == after.transformsFollowFrameRate
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
    quint64 overflowsToReport = 0;
    quint64 overflowsInAll = 0;
    {
        QMutexLocker lock(&state->mutex);
        if (state->retired || !state->active || state->configurationPending
            || interleavedIq.size() > state->maxPendingIqFloats) {
            // The discarded whole packet may separate already queued I/Q
            // from the next accepted packet. Never join samples across that
            // unknown gap in the FFT overlap ring.
            state->pendingIq.clear();
            state->gapsAtFloat.clear();
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
            state->gapsAtFloat.clear();
            state->inputDiscontinuity = true;
            ++state->inputFramesDropped;
            // 2026-10-21: counted on its own and logged, at most once an
            // interval: it was silent, and each one drops queued samples.
            ++state->inputQueueOverflows;
            const qint64 nowNs = monotonicNowNs();
            if (!state->inputQueueOverflowWarned
                || nowNs - state->inputQueueOverflowWarnedAtNs
                    >= kQueueOverflowWarningIntervalNs) {
                overflowsToReport = state->inputQueueOverflows
                    - state->inputQueueOverflowsWarned;
                overflowsInAll = state->inputQueueOverflows;
                state->inputQueueOverflowsWarned = state->inputQueueOverflows;
                state->inputQueueOverflowWarnedAtNs = nowNs;
                state->inputQueueOverflowWarned = true;
            }
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
                drainInput(engine, state);
            }, Qt::QueuedConnection);
        }
    }
    if (overflowsToReport != 0) {
        qCWarning(lcSpectrum) << "Spectrum source: the FFT input queue was full and its"
                              << "samples were dropped:" << overflowsToReport
                              << "times since the last report (" << overflowsInAll
                              << "in all)";
    }
}

void DaemonSpectrumSource::drainInput(FFTEngine* engine,
                                      const QSharedPointer<FrameState>& state)
{
    // On the engine's worker thread.
    QVector<float> iq;
    bool resetInputHistory = false;
    QVector<int> gapsAtFloat;
    {
        QMutexLocker lock(&state->mutex);
        if (state->retired || !state->active
            || state->configurationPending) {
            state->pendingIq.clear();
            state->gapsAtFloat.clear();
            state->iqDrainQueued = false;
            return;
        }
        resetInputHistory = state->inputDiscontinuity;
        state->inputDiscontinuity = false;
        iq.swap(state->pendingIq);
        gapsAtFloat.swap(state->gapsAtFloat);
        state->iqDrainQueued = false;
        state->inputDrainStarted = true;
        if (resetInputHistory) {
            ++state->inputHistoryResets;
            // The samples before the reset and after it are not continuous.
            state->sampleClock.reset();
            state->hasPublishedStamp = false;
        }
    }
    if (resetInputHistory) {
        engine->resetInputHistory();
    }
    bool fed = false;
    // NereusSDR, 2026-10-23: I/Q sequence gaps from the radio. 2026-10-24:
    // each gap queued before this drain, in order (two or more can wait
    // when the engine thread lags). The samples before a gap go in first,
    // then the engine takes the gap as the desktop app's engines do
    // (FFTEngine::requestWindowReset): it drops its window on a healthy
    // link, and lets the gap through on a lossy one (the last gap, let
    // through or reset, less than FFTEngine::kGapLossyLinkWindows windows
    // before, once a frame has come since the last reset; or no frame for
    // FFTEngine::kGapResetStallFrameIntervals frame intervals). Only a
    // dropped window restarts the sample clock, before the samples after
    // the gap make a frame.
    int fedTo = 0;
    for (const int gapAt : std::as_const(gapsAtFloat)) {
        const int split = std::clamp(gapAt, fedTo, static_cast<int>(iq.size())) & ~1;
        if (split > fedTo) {
            engine->feedIQ(iq.mid(fedTo, split - fedTo));
            fed = true;
            fedTo = split;
        }
        engine->requestWindowReset();
        if (engine->applyPendingWindowReset()) {
            QMutexLocker lock(&state->mutex);
            ++state->inputHistoryResets;
            state->sampleClock.reset();
            state->hasPublishedStamp = false;
        }
    }
    if (fedTo < iq.size()) {
        engine->feedIQ(fedTo == 0 ? iq : iq.mid(fedTo));
        fed = true;
    }
    if (fed) {
        QMutexLocker lock(&state->mutex);
        ++state->completedInputHandoffs;
    }
}

void DaemonSpectrumSource::resetInputHistoryAll()
{
    // NereusSDR, based on Thetis console.cs:24243-24254 and 24132
    // [v2.10.3.15]: Thetis purges the display buffers on a MOX edge only
    // when "Purge WDSP buffers on MOX" is set (off by default). NereusSDR
    // always clears the FFT input, because a transform that spans the edge
    // mixes receive and transmit samples on every display.
    //
    // The engine is not touched from this thread. Each source is marked
    // through the path a dropped packet uses (inputDiscontinuity): the
    // samples queued before the edge are discarded here, and the drain on
    // the engine's own worker thread calls FFTEngine::resetInputHistory
    // before it feeds the first samples after the edge.
    for (SourceEntry& entry : m_sources) {
        if (!entry.engine) {
            continue;
        }
        const QSharedPointer<FrameState> state = entry.state;
        QMutexLocker lock(&state->mutex);
        if (state->retired) {
            continue;
        }
        state->pendingIq.clear();
        state->gapsAtFloat.clear();
        state->inputDiscontinuity = true;
        if (!state->iqDrainQueued) {
            // No drain is waiting: post one, so the reset does not wait for
            // the next I/Q packet. Posted under the state lock, as enqueueIq
            // does, so it never reaches a retired engine.
            state->iqDrainQueued = true;
            QMetaObject::invokeMethod(entry.engine,
                                      [engine = entry.engine.data(), state]() {
                drainInput(engine, state);
            }, Qt::QueuedConnection);
        }
    }
}

quint64 DaemonSpectrumSource::inputHistoryResets(const MediaSourceKey& key) const
{
    const auto it = m_sources.constFind(key);
    if (it == m_sources.cend()) {
        return 0;
    }
    QMutexLocker lock(&it->state->mutex);
    return it->state->inputHistoryResets;
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
    // A lost I/Q packet on this stream (-1: every stream). NereusSDR,
    // 2026-10-22, after Longpath PR 62: the window in progress is dropped
    // so the jump at the seam does not paint a line across the spectrum.
    // Sent on the connection thread between the samples before the gap
    // and after it. 2026-10-23: the gap's place in the queued samples is
    // marked (gapsAtFloat; 2026-10-24, every gap's, in order), and the
    // drain the next packet posts feeds the samples before each, then has
    // the engine take the gap under the same rule as the desktop app
    // (FFTEngine::applyPendingWindowReset), then feeds the samples after
    // it.
    entry.gapConnection = connect(
        m_radioModel, &RadioModel::iqSequenceGapForStream, entry.engine,
        [state = entry.state, streamIndex](int gapStream) {
            if (gapStream >= 0 && gapStream != streamIndex) {
                return;
            }
            QMutexLocker lock(&state->mutex);
            if (state->retired) {
                return;
            }
            // Two gaps at one place are one, as two requestWindowReset()
            // calls before one feedIQ() are in the desktop app.
            const int at = static_cast<int>(state->pendingIq.size());
            if (state->gapsAtFloat.isEmpty() || state->gapsAtFloat.constLast() != at) {
                state->gapsAtFloat.append(at);
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
            // A new configuration: the next frame takes a new anchor.
            state->sampleClock.reset();
            state->hasPublishedStamp = false;
        }
    }, Qt::QueuedConnection);
}

void DaemonSpectrumSource::noteSourceFrameGap(FrameState& state)
{
    // Under the state's lock. A stamp that did not move on is a test's own
    // (the sample clock's always increase); it reads as unknown.
    const qint64 stampNs = state.latest.producedAtNs;
    state.latest.sourceFrameGapNs =
        state.hasPublishedStamp && stampNs > state.lastPublishedStampNs
        ? stampNs - state.lastPublishedStampNs : 0;
    state.hasPublishedStamp = true;
    state.lastPublishedStampNs = stampNs;
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
        noteSourceFrameGap(*state);
        state->latest.binsLinear = QVector<float>(state->config.fft.fftSize, binLinear);
        state->latest.windowEnb = 1.0;
        state->latest.dbmOffset = 0.0;
        state->latest.binsDbm.clear();
        state->latest.binsDbmValid = computesBinsDbm()
            && binsLinearToDbm(state->latest.binsLinear, 0.0, state->latest.binsDbm);
        if (state->hasLatest) { ++state->replacedSinceTaken; }
        state->hasLatest = true;
        state->notificationQueued = true;
        ++state->publishedFrames;
    }
    // Display send thread: the display thread takes it when it is woken.
    if (!callFrameWake()) {
        emit frameAvailable(key);
    }
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
        noteSourceFrameGap(*state);
        state->latest.binsLinear = binsLinear;
        state->latest.windowEnb = 1.0;
        state->latest.dbmOffset = 0.0;
        state->latest.binsDbm.clear();
        state->latest.binsDbmValid = computesBinsDbm()
            && binsLinearToDbm(binsLinear, 0.0, state->latest.binsDbm);
        if (state->hasLatest) { ++state->replacedSinceTaken; }
        state->hasLatest = true;
        state->notificationQueued = true;
        ++state->publishedFrames;
    }
    // Display send thread: the display thread takes it when it is woken.
    if (!callFrameWake()) {
        emit frameAvailable(key);
    }
    return true;
}

std::optional<qint64> DaemonSpectrumSource::publishSampledFrameForTest(
    const MediaSourceKey& key, qint64 inputPosition, qint64 publishedAtNs)
{
    const auto it = m_sources.constFind(key);
    if (it == m_sources.cend()) {
        return std::nullopt;
    }
    const QSharedPointer<FrameState> state = it->state;
    int fftSize = 0;
    {
        QMutexLocker lock(&state->mutex);
        if (!state->active || state->configurationPending) {
            return std::nullopt;
        }
        fftSize = state->config.fft.fftSize;
    }
    publishFrame(key, state, QVector<float>(fftSize, 1.0e-6f), 1.0, 0.0,
                 inputPosition, publishedAtNs);
    QMutexLocker lock(&state->mutex);
    if (!state->hasLatest) {
        return std::nullopt;
    }
    return state->latest.producedAtNs;
}

void DaemonSpectrumSource::publishFrame(
    const MediaSourceKey& key, const QSharedPointer<FrameState>& state,
    const QVector<float>& binsLinear, double windowEnb, double dbmOffset,
    qint64 inputPosition, qint64 publishedAtNs)
{
    // On the engine thread, outside the frame lock.
    QVector<float> binsDbm;
    bool binsDbmValid = false;
    if (computesBinsDbm()) {
        binsDbmValid = binsLinearToDbm(binsLinear, dbmOffset, binsDbm);
    }
    // Display send thread: with a wake set the display thread is told, not
    // the main thread, and the frame is not announced on the main thread;
    // whoever clears the wake takes it (setFrameWake). Held from here to
    // the call, so a wake replaced meanwhile is never called.
    std::unique_lock<std::mutex> wakeLock(m_frameWakeMutex);
    const bool waking = static_cast<bool>(m_frameWake);
    if (!waking) {
        wakeLock.unlock();
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
        // 2026-10-21: the time of the frame's samples, not of this publish.
        state->latest.producedAtNs = state->sampleClock.stamp(
            inputPosition, state->config.sampleRateHz, publishedAtNs);
        noteSourceFrameGap(*state);
        state->latest.binsLinear = binsLinear;
        state->latest.windowEnb = windowEnb;
        state->latest.dbmOffset = dbmOffset;
        state->latest.binsDbm = std::move(binsDbm);
        state->latest.binsDbmValid = binsDbmValid;
        if (state->hasLatest) { ++state->replacedSinceTaken; }
        state->hasLatest = true;
        ++state->publishedFrames;
        if (!waking && !state->notificationQueued) {
            state->notificationQueued = true;
            queueNotification = true;
        }
    }
    if (waking) {
        m_frameWake();
        return;
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
