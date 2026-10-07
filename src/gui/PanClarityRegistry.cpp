// =================================================================
// PanClarityRegistry — NereusSDR-original GUI ownership, GPLv3.
// no-port-check: NereusSDR-original GUI controller ownership.
// The retained Thetis filename comment is historical context moved from MainWindow.
// This file wires per-pan controllers to NereusSDR interfaces; the Clarity estimator
// remains in the existing core/ClarityController implementation.
// Modification history (NereusSDR):
// 2026-10-05 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// =================================================================
#include "PanClarityRegistry.h"
#include "core/ClarityController.h"
#include "core/FFTEngine.h"
#include "core/MoxController.h"
#include "gui/SpectrumWidget.h"
#include "gui/SpectrumOverlayPanel.h"
#include "models/RadioModel.h"
#include "models/PanadapterModel.h"
#include "models/SliceModel.h"
#include <QDateTime>
#include <QPointer>
#include <optional>
#include <utility>

namespace NereusSDR {
struct PanClarityRegistry::Entry {
    QPointer<SpectrumWidget> widget;
    QPointer<SpectrumOverlayPanel> panel;
    QPointer<PanadapterModel> saved;
    QPointer<SliceModel> slice;
    QPointer<FFTEngine> engine;
    QPointer<ClarityController> controller;
    RecipientToken token;
    std::optional<SourceAssociation> source;
    QMetaObject::Connection fftConnection;
    QMetaObject::Connection bandConnection;
    bool retired = false;
    bool executing = false;
    bool pendingReset = false;
    bool hintOutput = false;
    RecipientToken operationToken;
    struct Operation {
        RecipientToken token;
        std::function<void(ClarityController*)> call;
        OperationAuthority authority;
        bool hint;
        std::optional<bool> keyedTransition;
    };
    QList<Operation> pending;
    bool remote = false;
    bool available = false;
    bool hasOutput = false;
    quint32 mediaEpoch = 0;
    quint32 endpointId = 0;
    quint32 contextGeneration = 0;
    QHash<int, float> hints;
    std::optional<int> currentBand;
    struct NFHistoryEntry { qint64 t; float value; };
    QList<NFHistoryEntry> history;
    ~Entry() { QObject::disconnect(fftConnection); QObject::disconnect(bandConnection); }
};
PanClarityRegistry::PanClarityRegistry(RadioModel* model, QObject* parent)
    : QObject(parent), m_model(model), m_enabled(model && model->clarityEnabled())
{
    if (model) { connect(model, &RadioModel::clarityEnabledChanged, this, &PanClarityRegistry::setEnabled); }
}
PanClarityRegistry::~PanClarityRegistry() { retireAll(); }
void PanClarityRegistry::setLocalKeyedQueryForTest(std::function<bool()> query)
{
    m_localKeyedQueryForTest = std::move(query);
}
bool PanClarityRegistry::localKeyedNow() const
{
    return (m_localKeyedQueryForTest && m_localKeyedQueryForTest())
        || (m_model && m_model->ownsLocalDsp() && m_model->moxController()
            && m_model->moxController()->isMox());
}
bool PanClarityRegistry::keyedNow() const
{
    return m_keyed || localKeyedNow();
}
void PanClarityRegistry::dispatch(const QString& id, const QSharedPointer<Entry>& entry,
    std::function<void(ClarityController*)> call, OperationAuthority authority, bool hint,
    std::optional<bool> keyedTransition)
{
    if (!current(id, entry)) { return; }
    entry->pending.append({entry->token, std::move(call), authority, hint, keyedTransition});
    if (entry->executing) { return; }
    // Emitting controller calls may synchronously retire their Qt owner. Keep
    // the controller outside automatic child deletion until every call unwinds.
    const QPointer<PanClarityRegistry> self(this);
    const QPointer<ClarityController> controller(entry->controller);
    entry->executing = true;
    controller->setParent(nullptr);
    while (self && !entry->retired && controller && !entry->pending.isEmpty()) {
        const auto operation = entry->pending.takeFirst();
        // Feed/hint work belongs to its recipient revision. Master, keyed and
        // Retune controls belong to this surviving owner, even across a rebind.
        // Retirement still rejects every operation, including owner controls.
        const auto valid = [&]() {
            return self && self->current(id, entry)
                && operation.token.instance == entry->token.instance
                && (operation.authority == OperationAuthority::OwnerControl
                    || operation.token == entry->token);
        };
        if (!valid()) { continue; }
        entry->operationToken = operation.token;
        entry->hintOutput = operation.hint;
        if (entry->pendingReset) {
            entry->pendingReset = false;
            controller->setEnabled(false);
            if (!self || entry->retired) { break; }
            controller->setEnabled(self->m_enabled);
            if (!self || entry->retired) { break; }
        }
        // Preserve explicit queued keyed edges, while authoritative local MOX
        // truth continues to fence an older cached display/unkey transition.
        controller->setTransmitting(operation.keyedTransition
            ? *operation.keyedTransition || self->localKeyedNow() : self->keyedNow());
        if (!self || entry->retired) { break; }
        if (!valid()) { continue; }
        operation.call(controller);
    }
    // A rebind without an immediate replacement sample must also discard the
    // obsolete call's deadband/cadence tail as soon as that call unwinds.
    if (self && !entry->retired && controller && entry->pendingReset) {
        entry->pendingReset = false;
        controller->setEnabled(false);
        controller->setEnabled(self->m_enabled);
    }
    entry->executing = false;
    entry->hintOutput = false;
    entry->pending.clear();
    if (controller) {
        if (self && self->current(id, entry)) { controller->setParent(self); }
        else { controller->deleteLater(); }
    }
}
bool PanClarityRegistry::current(const QString& id, const QSharedPointer<Entry>& entry) const
{
    return !entry->retired && m_entries.value(id) == entry && entry->widget && entry->controller;
}
bool PanClarityRegistry::canOutput(const QString& id, const QSharedPointer<Entry>& entry) const
{
    return current(id, entry) && m_enabled && !keyedNow() && (entry->available || entry->hintOutput)
        && (!entry->executing || entry->operationToken == entry->token);
}
void PanClarityRegistry::refreshStatus(const QSharedPointer<Entry>& entry)
{
    if (entry->panel && entry->controller) {
        entry->panel->setClarityStatus(m_enabled && entry->hasOutput,
            m_enabled && (keyedNow() || entry->controller->isPaused()));
    }
}
void PanClarityRegistry::registerPan(const QString& id, SpectrumWidget* widget,
                                    SpectrumOverlayPanel* panel, PanadapterModel* saved)
{
    if (id.isEmpty() || !widget) { return; }
    const auto old = m_entries.value(id);
    if (old && old->widget == widget) { old->panel = panel; refreshStatus(old); return; }
    retirePan(id);
    auto entry = QSharedPointer<Entry>::create();
    entry->widget = widget; entry->panel = panel; entry->saved = saved;
    entry->token = {++m_nextInstance, 1};
    entry->controller = new ClarityController(this);
    entry->controller->setEnabled(m_enabled);
    entry->controller->setTransmitting(keyedNow());
    m_entries.insert(id, entry);
    widget->setClarityActive(false);
    const QWeakPointer<Entry> weak(entry);
    connect(widget, &QObject::destroyed, this, [this, id, weak]() {
        auto live = weak.toStrongRef();
        if (live && m_entries.value(id) == live) { retirePan(id); }
    });
    // Clarity → SpectrumWidget threshold update + clarityActive flag.
    // Issue #230 fix: write the render-active mirror, not the
    // persistent user fields — Clarity is runtime state per Thetis's
    // AGC pattern (display.cs:6584 [v2.10.3.13] uses
    // _RX1waterfallPreviousMinValue, a runtime field separate from
    // waterfall_low_threshold).  The previous setWfLow/HighThreshold
    // calls were silently overwriting the user's saved thresholds via
    // scheduleSettingsSave() on every Clarity tick.
    connect(entry->controller, &ClarityController::waterfallThresholdsChanged, this,
        [this, id, weak](float low, float high) {
        const QPointer<PanClarityRegistry> self(this);
        auto live = weak.toStrongRef();
        // PR #212 follow-up bench fix (KG4VCF, 2026-05-10): suppress
        // Clarity threshold updates while MOX is active.  Clarity tracks
        // RX noise floor and would otherwise re-enable itself with
        // RX-tuned thresholds during TX, defeating the
        // setClarityActive(false) call in the MOX-rise lambda.
        //
        // Kept through the 3M-5 revive merge, re-applied on top of the
        // issue-#230 shape: the threshold write now goes to the runtime
        // mirror via setClarityWaterfallThresholds rather than to the
        // persisted setWfLow/HighThreshold pair. The MOX gate is still
        // needed and is independent of that change -- #230 stopped Clarity
        // clobbering SAVED thresholds, this stops it running at all during
        // transmit.
        // Parity Task 29: a remote window's Core keyed (no local MOX).
        if (!live || !canOutput(id, live)) { return; }
        live->widget->setClarityActive(true);
        if (!self || !canOutput(id, live)) { return; }
        live->widget->setClarityWaterfallThresholds(low, high);
        if (!self || !canOutput(id, live)) { return; }
        live->hasOutput = true; refreshStatus(live);
    });
    // Clarity → SpectrumWidget NF-aware grid (Task 2.9).
    // NereusSDR-original — no Thetis equivalent.
    // noiseFloorChanged fires after EWMA smoothing but before the deadband
    // gate so the grid tracks the floor at every cadence tick.
    // The window owns both output routes; retiring the initial pane must
    // neither disconnect them nor leave its grid receiving another pane's floor.
    connect(entry->controller, &ClarityController::noiseFloorChanged, this,
        [this, id, weak](float nf) {
        const QPointer<PanClarityRegistry> self(this);
        auto live = weak.toStrongRef();
        if (!live || !canOutput(id, live)) { return; }
        live->widget->onNoiseFloorChanged(nf);
        if (!self || !canOutput(id, live)) { return; }
        if (live->slice) { live->hints.insert(int(live->slice->band()), nf); }
        if (!live->saved) { return; }
        // Task 2.10: per-band NF priming — settle detector.
        // NereusSDR-original — no Thetis equivalent.
        //
        // On each noiseFloorChanged tick, keep a 2-second sliding window of NF
        // samples. When variance drops below 1 dB for a sustained window of ≥30
        // samples (≈ 15 s / cadence-0.5s = 30 ticks), save the current floor to
        // the panadapter's per-band NF slot so the next band-switch can snap
        // instantly instead of cold-starting from zero.
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        live->history.append({now, nf});
        // Trim to 2-second window.
        const qint64 cutoff = now - 2000;
        while (!live->history.isEmpty() && live->history.first().t < cutoff) {
            live->history.removeFirst();
        }
        // Compute variance when we have ≥30 samples (~30 cadence ticks).
        if (live->history.size() >= 30) {
            float sum = 0.0f;
            for (const auto& e : std::as_const(live->history)) { sum += e.value; }
            const float mean = sum / static_cast<float>(live->history.size());
            float sqSum = 0.0f;
            for (const auto& e : std::as_const(live->history)) {
                const float d = e.value - mean; sqSum += d * d;
            }
            const float variance = sqSum / static_cast<float>(live->history.size());
            if (variance < 1.0f) {
                // NereusSDR-original — no Thetis equivalent.
                // NF settled within 1 dB variance over 2s; save for this band.
                live->saved->setBandNFEstimate(live->slice ? live->slice->band() : live->saved->band(), nf);
            }
        }
    });
    // When Clarity pauses or is disabled, let legacy AGC resume.
    connect(entry->controller, &ClarityController::pausedChanged, this, [this, id, weak](bool paused) {
        auto live = weak.toStrongRef();
        if (!live || !current(id, live)) { return; }
        const QPointer<PanClarityRegistry> self(this);
        if (paused) { live->widget->setClarityActive(false); }
        if (self && current(id, live)) { refreshStatus(live); }
    });
    if (panel) {
        connect(panel, &SpectrumOverlayPanel::clarityRetuneRequested, this, [this, id, weak]() {
            auto live = weak.toStrongRef();
            if (live && current(id, live)) { retunePan(id); }
        });
    }
    if (saved) {
        connect(saved, &PanadapterModel::bandChanged, this, [this, id, weak](Band band) {
            auto live = weak.toStrongRef();
            if (live && current(id, live) && (!live->slice || live->slice->band() == band)) { bandChanged(id, live, int(band)); }
        });
    }
    refreshStatus(entry);
}
void PanClarityRegistry::retirePan(const QString& id)
{
    auto entry = m_entries.take(id);
    if (!entry) { return; }
    ++entry->token.revision; entry->available = false; entry->retired = true; entry->pending.clear();
    if (entry->widget) { entry->widget->setClarityActive(false); }
    if (entry->controller && !entry->executing) { entry->controller->deleteLater(); }
}
void PanClarityRegistry::retireAll()
{
    const QPointer<PanClarityRegistry> self(this);
    const auto ids = m_entries.keys();
    for (const QString& id : ids) {
        if (!self) { return; }
        retirePan(id);
    }
}
ClarityController* PanClarityRegistry::controllerForPan(const QString& id) const
{
    auto entry = m_entries.value(id); return entry ? entry->controller.data() : nullptr;
}
void PanClarityRegistry::invalidate(const QSharedPointer<Entry>& entry)
{
    ++entry->token.revision;
    entry->available = false; entry->source.reset(); entry->hasOutput = false;
    QObject::disconnect(entry->fftConnection); QObject::disconnect(entry->bandConnection);
    entry->engine.clear(); entry->slice.clear(); entry->history.clear();
    entry->pendingReset = true;
    const QPointer<PanClarityRegistry> self(this);
    if (entry->widget) { entry->widget->setClarityActive(false); }
    if (self) { refreshStatus(entry); }
}
void PanClarityRegistry::associate(const QString& id, const QSharedPointer<Entry>& entry,
                                   SliceModel* slice, const SourceAssociation& source)
{
    if (entry->source && *entry->source != source) {
        entry->pendingReset = true;
        entry->history.clear();
    }
    entry->source = source;
    if (entry->slice != slice) {
        QObject::disconnect(entry->bandConnection); entry->slice = slice;
        if (slice) {
            entry->currentBand = int(slice->band());
            const QWeakPointer<Entry> weak(entry);
            const QPointer<SliceModel> guard(slice);
            entry->bandConnection = connect(slice, &SliceModel::bandChanged, this,
                [this, id, weak, guard](Band band) {
                auto live = weak.toStrongRef();
                if (live && current(id, live) && guard && live->slice == guard) { bandChanged(id, live, int(band)); }
            });
        }
    }
}
void PanClarityRegistry::bandChanged(const QString& id, const QSharedPointer<Entry>& entry, int band)
{
    // Task 2.10: band-change → prime ClarityController EWMA with stored NF.
    // NereusSDR-original — no Thetis equivalent.
    //
    // PanadapterModel::bandChanged fires when the pan center crosses a band
    // boundary. snapToFloor() seeds the EWMA (m_smoothedFloor) and emits
    // waterfallThresholdsChanged immediately so the waterfall snaps to the
    // remembered state rather than cold-starting from an uninitialized floor.
    // NaN is ignored by snapToFloor (band with no stored data is a no-op).
    // NereusSDR-original — no Thetis equivalent.
    // Prime estimator with last-seen NF for this band to eliminate
    // cold-start visual jump after band change.
    if (!current(id, entry)) { return; }
    if (entry->currentBand && *entry->currentBand == band) { return; }
    entry->currentBand = band;
    if (!m_enabled || keyedNow()) { return; }
    const float floor = entry->saved ? entry->saved->bandNFEstimate(Band(band))
                                    : entry->hints.value(band, qQNaN());
    // Best-effort hints can prime before an input recipient becomes available.
    dispatch(id, entry, [floor](ClarityController* controller) { controller->snapToFloor(floor); }, OperationAuthority::Recipient, true);
}
void PanClarityRegistry::bindLocal(const QString& id, SliceModel* slice, FFTEngine* engine,
                                  const SourceAssociation& source)
{
    auto entry = m_entries.value(id);
    if (!entry) { return; }
    if (!slice || !engine || source.streamIndex < 0) { invalidate(entry); return; }
    if (!entry->remote && entry->engine == engine && entry->slice == slice
        && entry->source == source) { entry->available = true; return; }
    associate(id, entry, slice, source);
    QObject::disconnect(entry->fftConnection);
    entry->remote = false; entry->engine = engine; entry->available = true;
    const RecipientToken token{entry->token.instance, ++entry->token.revision};
    const QWeakPointer<Entry> weak(entry); const QPointer<FFTEngine> guard(engine);
    // Feed FFT bins to Clarity (auto-queued: spectrum thread → main).
    // Parity Task 18: Clarity reads the stream of the pan it tunes.
    entry->fftConnection = connect(engine, &FFTEngine::fftReady, this,
        [this, id, weak, guard, token](int, const QVector<float>& bins) {
        auto live = weak.toStrongRef();
        if (!live || !current(id, live) || !m_enabled || keyedNow() || !live->available || !guard || live->engine != guard
            || live->token != token || !live->slice
            || live->slice->streamIndex() != live->source->streamIndex
            || live->slice->streamEpoch() != live->source->streamEpoch) { return; }
        dispatch(id, live, [this, id, live, bins](ClarityController* controller) {
            if (canOutput(id, live)) { controller->feedBins(bins); }
        });
    });
}
PanClarityRegistry::RecipientToken PanClarityRegistry::bindRemote(const QString& id,
    SpectrumWidget* widget, SliceModel* slice, const SourceAssociation& source,
    quint32 mediaEpoch, quint32 endpointId, quint32 contextGeneration)
{
    auto entry = m_entries.value(id);
    if (!entry || entry->widget != widget || !slice) { return {}; }
    associate(id, entry, slice, source);
    QObject::disconnect(entry->fftConnection); entry->engine.clear();
    entry->remote = true; entry->available = true;
    entry->mediaEpoch = mediaEpoch; entry->endpointId = endpointId;
    entry->contextGeneration = contextGeneration;
    ++entry->token.revision; return entry->token;
}
void PanClarityRegistry::feedRemoteFloor(const QString& id, SpectrumWidget* widget,
    const RecipientToken& token, float floor, qint64 now)
{
    auto entry = m_entries.value(id);
    if (!entry || !current(id, entry) || !m_enabled || keyedNow() || !entry->available || !entry->remote || entry->widget != widget
        || token.instance == 0 || entry->token != token || !entry->slice || !entry->source
        || entry->slice->streamIndex() != entry->source->streamIndex
        || entry->slice->streamEpoch() != entry->source->streamEpoch) { return; }
    dispatch(id, entry, [this, id, entry, floor, now](ClarityController* controller) {
        if (canOutput(id, entry)) { controller->feedNoiseFloor(floor, now); }
    });
}
void PanClarityRegistry::setRemoteAvailable(const QString& id, const RecipientToken& token, bool available)
{
    auto entry = m_entries.value(id);
    if (entry && entry->remote && entry->token == token && token.instance != 0) { entry->available = available; }
}
void PanClarityRegistry::invalidateRemoteSession()
{
    const QPointer<PanClarityRegistry> self(this);
    const auto entries = m_entries;
    for (const auto& entry : entries) {
        if (!self) { return; }
        if (entry->remote) { invalidate(entry); }
    }
}
void PanClarityRegistry::invalidateAllSources()
{
    const QPointer<PanClarityRegistry> self(this);
    const auto entries = m_entries;
    for (const auto& entry : entries) {
        if (!self) { return; }
        invalidate(entry);
    }
}
void PanClarityRegistry::setEnabled(bool enabled)
{
    if (enabled == m_enabled) { return; }
    m_enabled = enabled;
    const QPointer<PanClarityRegistry> self(this);
    const auto entries = m_entries;
    for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
        if (!self) { return; }
        const auto entry = it.value();
        dispatch(it.key(), entry, [enabled](ClarityController* controller) { controller->setEnabled(enabled); }, OperationAuthority::OwnerControl);
        if (!self) { return; }
        if (!enabled && entry->widget) { entry->widget->setClarityActive(false); }
        if (!self) { return; }
        refreshStatus(entry);
    }
}
void PanClarityRegistry::setKeyed(bool keyed)
{
    m_keyed = keyed;
    const QPointer<PanClarityRegistry> self(this);
    const auto entries = m_entries;
    for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
        if (!self) { return; }
        const auto entry = it.value();
        dispatch(it.key(), entry, [](ClarityController*) {}, OperationAuthority::OwnerControl, false, keyed);
        if (!self) { return; }
        if (keyedNow() && entry->widget) { entry->widget->setClarityActive(false); }
        if (!self) { return; }
        refreshStatus(entry);
    }
}
void PanClarityRegistry::retunePan(const QString& id)
{
    const auto entry = m_entries.value(id);
    if (!entry) { return; }
    const QPointer<PanClarityRegistry> self(this);
    dispatch(id, entry, [](ClarityController* controller) { controller->retuneNow(); }, OperationAuthority::OwnerControl);
    if (self && current(id, entry)) { refreshStatus(entry); }
}
}
