// =================================================================
// PanClarityRegistry — NereusSDR-original GUI ownership, GPLv3.
// no-port-check: NereusSDR-original GUI controller ownership.
// The retained Thetis filename comment is historical context moved from MainWindow.
// This file wires per-pan controllers to NereusSDR interfaces; the Clarity estimator
// remains in the existing core/ClarityController implementation.
// Modification history (NereusSDR):
// 2026-10-12 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex:
//   Requested Clarity is each pan's choice; setEnabled is a master broadcast.
// 2026-10-05 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-10 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code:
//   Display pill plan Task 3, the noise floor goes to the pan with Clarity's
//   thresholds.
// 2026-10-10 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code:
//   Display pill plan Task 12. A key takes Clarity off no pan; the un-key
//   starts each pan's hold (and, after a band change while keyed, snaps once
//   to the new band's stored floor); a same-band re-centre keeps the floor
//   and holds; the per-band priming window holds the 30 readings its check
//   needs; a local pan's floor takes the pan's calibration; later levels go
//   to the pan as a target it glides to.
// 2026-10-10 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code:
//   Display pill plan Task 17. The pan's strip is told whether Clarity is
//   on as well as active and paused (setClarityState).
// =================================================================
#include "PanClarityRegistry.h"
#include "core/ClarityController.h"
#include "core/FFTEngine.h"
#include "core/MoxController.h"
#include "core/settings/PanDisplayKeys.h"
#include "core/spectrum/MoxDisplayHold.h"
#include "gui/SpectrumWidget.h"
#include "gui/SpectrumOverlayPanel.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/PanadapterModel.h"
#include "models/SliceModel.h"
#include <QDateTime>
#include <QPointer>
#include <cmath>
#include <optional>
#include <utility>

namespace NereusSDR {
namespace {
// Per-band priming (Task 2.10, NereusSDR-original): a band's floor is saved
// once this many readings in a row are steady. Clarity takes a reading
// about every 500 ms (ClarityController's poll interval), so 30 readings
// are about 15 s. The window is those readings by count, because the real
// cadence is the first frame at least 500 ms after the last and so runs a
// little over 500 ms; a reading older than twice the nominal 15 s is dropped
// so a long gap (a key, a pause) is not bridged.
constexpr int kPrimingReadings = 30;
constexpr qint64 kPrimingMaxAgeMs = 30000;
constexpr float kPrimingMaxVarianceDb2 = 1.0f;
// "No band" in Entry::bandBeforeKey.
constexpr int kNoBand = -1;
}
struct PanClarityRegistry::Entry {
    QString panId;
    std::optional<ClarityStatus> announcedStatus;
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
    bool requestedEnabled = false;
    quint32 mediaEpoch = 0;
    quint32 endpointId = 0;
    quint32 contextGeneration = 0;
    QHash<int, float> hints;
    std::optional<int> currentBand;
    // The band the pan was on when the first band change of this key
    // arrived (kNoBand when it had none); empty when the band has not
    // changed while keyed.
    std::optional<int> bandBeforeKey;
    // The pan is showing a Clarity level this registry gave it.
    bool showing = false;
    // The next level goes to the pan whole, not as a target to glide to:
    // the first level, and the first after anything that starts Clarity over.
    bool takeNextWhole = true;
    // When the reading being fed was taken (the priming window's clock).
    qint64 readingMs = 0;
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
std::optional<PanClarityRegistry::SourceAssociation>
PanClarityRegistry::sourceForPanForTest(const QString& id) const
{
    const auto entry = m_entries.value(id);
    return entry ? entry->source : std::nullopt;
}
void PanClarityRegistry::setLocalKeyedQueryForTest(std::function<bool()> query)
{
    m_localKeyedQueryForTest = std::move(query);
}
void PanClarityRegistry::setHoldLengthProvider(std::function<qint64(const QString& panId)> provider)
{
    m_holdLengthProvider = std::move(provider);
}
void PanClarityRegistry::setClockForTest(std::function<qint64()> nowMs)
{
    m_clockForTest = std::move(nowMs);
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
qint64 PanClarityRegistry::nowMs() const
{
    return m_clockForTest ? m_clockForTest() : QDateTime::currentMSecsSinceEpoch();
}
qint64 PanClarityRegistry::holdLengthMs(const QString& id) const
{
    const qint64 length = m_holdLengthProvider ? m_holdLengthProvider(id) : kMoxHoldMinMs;
    return length > 0 ? length : kMoxHoldMinMs;
}
float PanClarityRegistry::storedFloor(const QSharedPointer<Entry>& entry, int band)
{
    return entry->saved ? entry->saved->bandNFEstimate(Band(band))
                        : entry->hints.value(band, qQNaN());
}
void PanClarityRegistry::unkeyed(const QString& id, const QSharedPointer<Entry>& entry,
                                 ClarityController* controller)
{
    // Display pill plan Task 12 (design 5.5). The controller kept its floor
    // and the pan kept its Clarity level through the key, so the first
    // receive row already uses the pre-key level and nothing is emitted
    // here. The one exception is a band change while keyed.
    const QPointer<PanClarityRegistry> self(this);
    const QPointer<ClarityController> guard(controller);
    const bool bandMoved = entry->bandBeforeKey
        && *entry->bandBeforeKey != entry->currentBand.value_or(kNoBand);
    entry->bandBeforeKey.reset();
    if (bandMoved && entry->requestedEnabled) {
        const float stored = storedFloor(entry, entry->currentBand.value_or(kNoBand));
        entry->history.clear();
        entry->takeNextWhole = true;
        if (std::isfinite(stored)) {
            // One snap to the band the pan is on now. A stored floor may
            // reach a pan whose input is not available yet, as a
            // band-change hint does.
            const bool hint = entry->hintOutput;
            entry->hintOutput = true;
            controller->snapToFloor(stored);
            if (!self || !guard || entry->retired) { return; }
            entry->hintOutput = hint;
        } else {
            // No stored floor for the new band, and the old band's floor is
            // no guide to it: the first reading after the hold is taken
            // whole. Nothing is emitted until then.
            controller->setEnabled(false);
            controller->setEnabled(entry->requestedEnabled);
        }
    }
    // The hold: for its length Clarity takes in no frames (they still carry
    // the transmission while the FFT refills) and emits nothing.
    controller->beginHold(holdLengthMs(id), nowMs());
    // A pan whose Clarity level something else took off during the key
    // shows it again: after the un-key Clarity is always active without
    // Re-tune or a band change (#359), and at an unchanged floor no
    // emission would bring it back.
    if (entry->showing && entry->requestedEnabled && entry->widget && !entry->widget->clarityActive()) {
        entry->widget->setClarityActive(true);
    }
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
            entry->takeNextWhole = true;
            controller->setEnabled(false);
            if (!self || entry->retired) { break; }
            controller->setEnabled(entry->requestedEnabled);
            if (!self || entry->retired) { break; }
        }
        // Preserve explicit queued keyed edges, while authoritative local MOX
        // truth continues to fence an older cached display/unkey transition.
        const bool wasKeyed = controller->isTransmitting();
        controller->setTransmitting(operation.keyedTransition
            ? *operation.keyedTransition || self->localKeyedNow() : self->keyedNow());
        if (!self || entry->retired) { break; }
        // Whichever operation finds the key gone starts the hold: an
        // explicit setKeyed(false), or the first one after the radio's own
        // MOX fell.
        if (wasKeyed && !controller->isTransmitting()) {
            self->unkeyed(id, entry, controller);
            if (!self || entry->retired) { break; }
        }
        if (!valid()) { continue; }
        operation.call(controller);
    }
    // A rebind without an immediate replacement sample must also discard the
    // obsolete call's kept floor and cadence tail as soon as that call unwinds.
    if (self && !entry->retired && controller && entry->pendingReset) {
        entry->pendingReset = false;
        entry->takeNextWhole = true;
        controller->setEnabled(false);
        controller->setEnabled(entry->requestedEnabled);
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
    return current(id, entry) && entry->requestedEnabled && !keyedNow() && (entry->available || entry->hintOutput)
        && (!entry->executing || entry->operationToken == entry->token);
}
void PanClarityRegistry::refreshStatus(const QSharedPointer<Entry>& entry)
{
    if (!current(entry->panId, entry)) { return; }
    const QPointer<PanClarityRegistry> self(this);
    const std::optional<ClarityStatus> state = statusForPan(entry->panId);
    if (!state) { return; }
    // A newly bound panel still needs seeding even if the status flags did
    // not change. The GUI notification itself is deduplicated below.
    if (entry->panel) {
        entry->panel->setClarityState(state->requestedEnabled, state->active, state->paused);
    }
    if (!self || !current(entry->panId, entry)) { return; }
    const std::optional<ClarityStatus> currentState = statusForPan(entry->panId);
    if (entry->announcedStatus == currentState) { return; }
    entry->announcedStatus = currentState;
    emit panStatusChanged(entry->panId);
}

void PanClarityRegistry::setPanEnabled(const QString& id, const QSharedPointer<Entry>& entry,
                                      bool enabled)
{
    if (!current(id, entry) || entry->requestedEnabled == enabled) { return; }
    const QPointer<PanClarityRegistry> self(this);
    // Fence output immediately, before an executing controller can finish
    // its old reading. The controller transition retains owner authority
    // across a rebind and is rejected on retirement like other controls.
    entry->requestedEnabled = enabled;
    entry->takeNextWhole = true;
    entry->hasOutput = false;
    if (!enabled) { entry->showing = false; }
    dispatch(id, entry, [enabled](ClarityController* controller) {
        controller->setEnabled(enabled);
    }, OperationAuthority::OwnerControl);
    if (!self || !current(id, entry)) { return; }
    if (!entry->requestedEnabled) { entry->widget->setClarityActive(false); }
    if (self && current(id, entry)) { refreshStatus(entry); }
}
void PanClarityRegistry::registerPan(const QString& id, SpectrumWidget* widget,
                                    SpectrumOverlayPanel* panel, PanadapterModel* saved)
{
    if (id.isEmpty() || !widget) { return; }
    const auto old = m_entries.value(id);
    if (old && old->widget == widget) { old->panel = panel; refreshStatus(old); return; }
    const QPointer<PanClarityRegistry> self(this);
    const QPointer<SpectrumWidget> recipient(widget);
    retirePan(id);
    if (!self || !recipient) { return; }
    auto entry = QSharedPointer<Entry>::create();
    entry->panId = id;
    entry->widget = widget; entry->panel = panel; entry->saved = saved;
    entry->token = {++m_nextInstance, 1};
    entry->controller = new ClarityController(this);
    entry->requestedEnabled = widget->clarityEnabled();
    entry->controller->setEnabled(entry->requestedEnabled);
    entry->controller->setTransmitting(keyedNow());
    m_entries.insert(id, entry);
    widget->setClarityActive(false);
    if (!self || !current(id, entry)) { return; }
    const QWeakPointer<Entry> weak(entry);
    connect(widget, &QObject::destroyed, this, [this, id, weak]() {
        auto live = weak.toStrongRef();
        if (live && m_entries.value(id) == live) { retirePan(id); }
    });
    const auto followChoice = [this, id, weak]() {
        auto live = weak.toStrongRef();
        if (live && current(id, live)) { setPanEnabled(id, live, live->widget->clarityEnabled()); }
    };
    connect(widget, &SpectrumWidget::displaySettingChanged, this,
        [followChoice](const QString& base) {
        if (base == QStringLiteral("ClarityEnabled")) { followChoice(); }
    });
    // Reset reloads the requested choice without per-setting announcements.
    connect(widget, &SpectrumWidget::panDisplayReset, this, followChoice);
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
        // RX noise floor and would otherwise write RX-tuned thresholds
        // during TX.  (When this was written a MOX-rise lambda also called
        // setClarityActive(false), and this gate kept Clarity from undoing
        // it.  Display pill plan Task 12 removed that call; see below.)
        //
        // Kept through the 3M-5 revive merge, re-applied on top of the
        // issue-#230 shape: the threshold write now goes to the runtime
        // mirror via setClarityWaterfallThresholds rather than to the
        // persisted setWfLow/HighThreshold pair. The MOX gate is still
        // needed and is independent of that change -- #230 stopped Clarity
        // clobbering SAVED thresholds, this stops it running at all during
        // transmit.
        //
        // Display pill plan Task 12: the gate stays (nothing is emitted
        // while keyed), but the key no longer takes Clarity off the pan;
        // the pan keeps the level it had.
        // Parity Task 29: a remote window's Core keyed (no local MOX).
        if (!live || !canOutput(id, live)) { return; }
        // Display pill plan Task 3: under Clarity the waterfall colours
        // against the noise floor itself (Black Level and Color Gain place
        // the range, core/spectrum/ClarityLevels.h), so the floor goes with
        // the thresholds, and first, so the first Clarity frame has it.
        // snapToFloor() emits this signal too, after it seeds the floor.
        // Display pill plan Task 12: the first level, a band-change snap and
        // the first level after Clarity starts over go to the pan whole;
        // every other level is a target the pan glides to, row by row
        // (SpectrumWidget::setClarityFloorTargetDbm).
        const float floor = live->controller->smoothedFloor();
        if (live->takeNextWhole || live->hintOutput
            || !std::isfinite(live->widget->clarityFloorDbm())) {
            live->takeNextWhole = false;
            live->widget->setClarityFloorDbm(floor);
        } else {
            live->widget->setClarityFloorTargetDbm(floor);
        }
        if (!self || !canOutput(id, live)) { return; }
        live->showing = true;
        live->widget->setClarityActive(true);
        if (!self || !canOutput(id, live)) { return; }
        live->widget->setClarityWaterfallThresholds(low, high);
        if (!self || !canOutput(id, live)) { return; }
        live->hasOutput = true; refreshStatus(live);
    });
    // Clarity → SpectrumWidget NF-aware grid (Task 2.9).
    // NereusSDR-original — no Thetis equivalent.
    // noiseFloorChanged fires on every reading Clarity takes in, after EWMA
    // smoothing, so the grid tracks the floor at every cadence tick. It does
    // not fire while keyed or in the hold after a key.
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
        // On each noiseFloorChanged tick, keep a sliding window of the last
        // 30 NF samples (≈ 15 s / cadence-0.5s = 30 ticks). When their
        // variance is below 1 dB, save the current floor to the panadapter's
        // per-band NF slot so the next band-switch can snap instantly instead
        // of cold-starting from zero.
        // Display pill plan Task 12: the window used to be 2 seconds, which
        // at the 500 ms cadence never held the 30 samples the check needs,
        // so nothing was ever saved. It is on the time of the reading, which
        // the injected clock supplies in tests.
        const qint64 now = live->readingMs;
        live->history.append({now, nf});
        // Trim to the window (kPrimingReadings, kPrimingMaxAgeMs).
        const qint64 cutoff = now - kPrimingMaxAgeMs;
        while (!live->history.isEmpty()
               && (live->history.size() > kPrimingReadings || live->history.first().t < cutoff)) {
            live->history.removeFirst();
        }
        // Compute variance when we have ≥30 samples (~30 cadence ticks).
        if (live->history.size() >= kPrimingReadings) {
            float sum = 0.0f;
            for (const auto& e : std::as_const(live->history)) { sum += e.value; }
            const float mean = sum / static_cast<float>(live->history.size());
            float sqSum = 0.0f;
            for (const auto& e : std::as_const(live->history)) {
                const float d = e.value - mean; sqSum += d * d;
            }
            const float variance = sqSum / static_cast<float>(live->history.size());
            if (variance < kPrimingMaxVarianceDb2) {
                // NereusSDR-original — no Thetis equivalent.
                // NF settled within 1 dB variance over the window; save for
                // this band, through this pan's own saved model.
                live->saved->setBandNFEstimate(live->slice ? live->slice->band() : live->saved->band(), nf);
            }
        }
    });
    // A key or a manual pause changes the badge only. The pan keeps its
    // Clarity level: AGC, NF-AGC and the stored levels do not take over
    // while keyed or paused (display pill plan Task 12, design 5.4 step 1).
    connect(entry->controller, &ClarityController::pausedChanged, this, [this, id, weak](bool) {
        auto live = weak.toStrongRef();
        if (!live || !current(id, live)) { return; }
        refreshStatus(live);
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
    const QPointer<PanClarityRegistry> self(this);
    auto entry = m_entries.take(id);
    if (!entry) { return; }
    ++entry->token.revision; entry->available = false; entry->retired = true; entry->pending.clear();
    entry->showing = false;
    if (entry->widget) { entry->widget->setClarityActive(false); }
    if (!self) { return; }
    if (entry->controller && !entry->executing) { entry->controller->deleteLater(); }
    // A callback may already have replaced this id; its new registration
    // announced that state, so the old owner must not invalidate it again.
    if (!m_entries.contains(id)) { emit panStatusChanged(id); }
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
std::optional<PanClarityRegistry::ClarityStatus> PanClarityRegistry::statusForPan(const QString& id) const
{
    const QSharedPointer<Entry> entry = m_entries.value(id);
    if (!entry || !entry->controller || !current(id, entry)) { return std::nullopt; }
    // Keep the same active/paused meaning refreshStatus already sends to
    // the panel. Availability is the existing source association's flag.
    return ClarityStatus{entry->requestedEnabled,
        entry->requestedEnabled && entry->hasOutput,
        entry->requestedEnabled && (keyedNow() || entry->controller->isPaused()),
        entry->available};
}

void PanClarityRegistry::invalidate(const QSharedPointer<Entry>& entry)
{
    ++entry->token.revision;
    entry->available = false; entry->source.reset(); entry->hasOutput = false;
    QObject::disconnect(entry->fftConnection); QObject::disconnect(entry->bandConnection);
    entry->engine.clear(); entry->slice.clear(); entry->history.clear();
    entry->pendingReset = true; entry->showing = false; entry->takeNextWhole = true;
    const QPointer<PanClarityRegistry> self(this);
    if (entry->widget) { entry->widget->setClarityActive(false); }
    if (self) { refreshStatus(entry); }
}
void PanClarityRegistry::associate(const QString& id, const QSharedPointer<Entry>& entry,
                                   SliceModel* slice, const SourceAssociation& source)
{
    const QPointer<PanClarityRegistry> self(this);
    const RecipientToken before = entry->token;
    if (entry->source && *entry->source != source) {
        const SourceAssociation& old = *entry->source;
        const bool sameStream = old.streamIndex == source.streamIndex
            && old.streamEpoch == source.streamEpoch
            && old.sampleRateHz == source.sampleRateHz;
        if (sameStream && bandFromFrequency(old.centreHz) == bandFromFrequency(source.centreHz)) {
            // Display pill plan Task 12 (design 5.8): only the centre moved,
            // on the same band. The floor there is the floor here, so the
            // smoothing is kept, and the frames that straddle the move are
            // held off as they are after a key.
            if (entry->controller) { entry->controller->beginHold(holdLengthMs(id), nowMs()); }
            if (!self || !current(id, entry) || entry->token != before) { return; }
        } else {
            // Another stream, epoch, rate or band: start over.
            entry->pendingReset = true;
            entry->history.clear();
        }
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
    const int previous = entry->currentBand.value_or(kNoBand);
    entry->currentBand = band;
    // The priming window is one band's readings.
    entry->history.clear();
    if (keyedNow()) {
        // Nothing is emitted while keyed. The un-key snaps once to the band
        // the pan is on then, if it is not the one it was on at the key
        // (unkeyed()).
        if (!entry->bandBeforeKey) { entry->bandBeforeKey = previous; }
        return;
    }
    if (!entry->requestedEnabled) { return; }
    const float floor = storedFloor(entry, band);
    // Best-effort hints can prime before an input recipient becomes available.
    dispatch(id, entry, [floor](ClarityController* controller) { controller->snapToFloor(floor); }, OperationAuthority::Recipient, true);
}
void PanClarityRegistry::bindLocal(const QString& id, SliceModel* slice, FFTEngine* engine,
                                  const SourceAssociation& source)
{
    const QPointer<PanClarityRegistry> self(this);
    auto entry = m_entries.value(id);
    if (!entry) { return; }
    if (!slice || !engine || source.streamIndex < 0) { invalidate(entry); return; }
    if (!entry->remote && entry->engine == engine && entry->slice == slice
        && entry->source == source) { entry->available = true; refreshStatus(entry); return; }
    const RecipientToken before = entry->token;
    associate(id, entry, slice, source);
    if (!self || !current(id, entry) || entry->token != before || entry->source != source) { return; }
    QObject::disconnect(entry->fftConnection);
    entry->remote = false; entry->engine = engine; entry->available = true;
    const RecipientToken token{entry->token.instance, ++entry->token.revision};
    const QWeakPointer<Entry> weak(entry); const QPointer<FFTEngine> guard(engine);
    // Feed FFT bins to Clarity (auto-queued: spectrum thread → main).
    // Parity Task 18: Clarity reads the stream of the pan it tunes.
    entry->fftConnection = connect(engine, &FFTEngine::fftReady, this,
        [this, id, weak, guard, token](int, const QVector<float>& bins) {
        auto live = weak.toStrongRef();
        if (!live || !current(id, live) || !live->requestedEnabled || keyedNow() || !live->available || !guard || live->engine != guard
            || live->token != token || !live->slice
            || live->slice->streamIndex() != live->source->streamIndex
            || live->slice->streamEpoch() != live->source->streamEpoch) { return; }
        dispatch(id, live, [this, id, live, bins](ClarityController* controller) {
            if (!canOutput(id, live)) { return; }
            // Display pill plan Task 12 (design 8.4): the bins are
            // uncalibrated, so the floor takes the offset this pan's receive
            // rows get and Black Level is an offset from a floor in the same
            // dB as the rows. Read with each frame; no frame is fed while
            // keyed, so the keyed value is never the one read.
            controller->setCalibrationOffsetDb(live->widget ? live->widget->displayCalOffsetDb() : 0.0f);
            live->readingMs = nowMs();
            controller->feedBins(bins, live->readingMs);
        });
    });
    if (self && current(id, entry)) { refreshStatus(entry); }
}
PanClarityRegistry::RecipientToken PanClarityRegistry::bindRemote(const QString& id,
    SpectrumWidget* widget, SliceModel* slice, const SourceAssociation& source,
    quint32 mediaEpoch, quint32 endpointId, quint32 contextGeneration)
{
    const QPointer<PanClarityRegistry> self(this);
    auto entry = m_entries.value(id);
    if (!entry || entry->widget != widget || !slice) { return {}; }
    const RecipientToken before = entry->token;
    associate(id, entry, slice, source);
    if (!self || !current(id, entry) || entry->token != before || entry->source != source) { return {}; }
    QObject::disconnect(entry->fftConnection); entry->engine.clear();
    entry->remote = true; entry->available = true;
    entry->mediaEpoch = mediaEpoch; entry->endpointId = endpointId;
    entry->contextGeneration = contextGeneration;
    ++entry->token.revision;
    const RecipientToken token = entry->token;
    refreshStatus(entry);
    return self && current(id, entry) && entry->token == token ? token : RecipientToken{};
}
void PanClarityRegistry::feedRemoteFloor(const QString& id, SpectrumWidget* widget,
    const RecipientToken& token, float floor, qint64 now)
{
    auto entry = m_entries.value(id);
    if (!entry || !current(id, entry) || !entry->requestedEnabled || keyedNow() || !entry->available || !entry->remote || entry->widget != widget
        || token.instance == 0 || entry->token != token || !entry->slice || !entry->source
        || entry->slice->streamIndex() != entry->source->streamIndex
        || entry->slice->streamEpoch() != entry->source->streamEpoch) { return; }
    // The Core's floor is calibrated already (its message carries the
    // frame's offset and the station's), so nothing is added to it.
    if (now < 0) { now = nowMs(); }
    dispatch(id, entry, [this, id, entry, floor, now](ClarityController* controller) {
        if (!canOutput(id, entry)) { return; }
        entry->readingMs = now;
        controller->feedNoiseFloor(floor, now);
    });
}
void PanClarityRegistry::setRemoteAvailable(const QString& id, const RecipientToken& token, bool available)
{
    const auto entry = m_entries.value(id);
    if (!entry || !current(id, entry) || !entry->remote || entry->token != token
            || token.instance == 0 || entry->available == available) { return; }
    entry->available = available;
    refreshStatus(entry);
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
    m_enabled = enabled;
    const quint64 revision = ++m_masterRevision;
    const QPointer<PanClarityRegistry> self(this);
    const auto entries = m_entries;
    // Preserve pending edits before the explicit master clears overrides,
    // including closed pans. An equal master value is still a broadcast.
    for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
        if (!self || revision != m_masterRevision) { return; }
        if (current(it.key(), it.value())) {
            it.value()->widget->storePendingPerPanSetting(QStringLiteral("ClarityEnabled"));
        }
    }
    PanDisplayKeys::writeDefault(QStringLiteral("ClarityEnabled"),
        enabled ? QStringLiteral("True") : QStringLiteral("False"));
    for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
        if (!self || revision != m_masterRevision) { return; }
        const auto entry = it.value();
        if (!current(it.key(), entry)) { continue; }
        entry->widget->setClarityEnabled(enabled);
        if (!self || revision != m_masterRevision) { return; }
        // A previously silent load may already hold the requested value.
        if (current(it.key(), entry)) {
            setPanEnabled(it.key(), entry, entry->widget->clarityEnabled());
        }
    }
}
void PanClarityRegistry::setKeyed(bool keyed)
{
    // Display pill plan Task 12 (design 5.4, 5.5). The rise takes Clarity
    // off no pan: every pan keeps its level through the key, and in a remote
    // window coreWaterfallLevelsInUse() does not change at either edge. The
    // fall starts each pan's hold (dispatch(), unkeyed()).
    m_keyed = keyed;
    const QPointer<PanClarityRegistry> self(this);
    const auto entries = m_entries;
    for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
        if (!self) { return; }
        const auto entry = it.value();
        dispatch(it.key(), entry, [](ClarityController*) {}, OperationAuthority::OwnerControl, false, keyed);
        if (!self) { return; }
        refreshStatus(entry);
    }
}
void PanClarityRegistry::retunePan(const QString& id)
{
    const auto entry = m_entries.value(id);
    if (!entry) { return; }
    const QPointer<PanClarityRegistry> self(this);
    entry->takeNextWhole = true;
    dispatch(id, entry, [](ClarityController* controller) { controller->retuneNow(); }, OperationAuthority::OwnerControl);
    if (self && current(id, entry)) { refreshStatus(entry); }
}
}
