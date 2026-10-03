// no-port-check: NereusSDR-original mirrored presentation of the Core's step
// attenuator and preamp. All attenuator logic stays in StepAttenuatorController.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/StepAttenuatorFacade.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See StepAttenuatorFacade.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  Created. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  A remote window's availability
//                                    (R-R3-46, R-R3-21). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 5): ATT on TX,
//                                    its value and Force ATT.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-11: rx2AttenuationDb
//                                    and rx2SliceMask, the other ADC's own
//                                    attenuator. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-11: RX2's own enable
//                                    and auto-attenuate settings.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Level Cal: rx2PreampMode, RX2's own
//                                    preamp mode. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-30  J.J. Boyd / KG4VCF  Level Cal 2 review: rx2AttenuationDb
//                                    stops at RX2's own top (0-31 dB)
//                                    unless linked. AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include "core/StepAttenuatorFacade.h"

#include "core/BoardCapabilities.h"
#include "core/StepAttenuatorController.h"
#include "models/RadioModel.h"

#include <algorithm>

namespace NereusSDR {

namespace {

constexpr int kPreampModeFirst = static_cast<int>(PreampMode::Off);
constexpr int kPreampModeLast = static_cast<int>(PreampMode::SaMinus30);

int overloadWireLevel(OverloadLevel level)
{
    switch (level) {
    case OverloadLevel::Yellow: return 1;
    case OverloadLevel::Red: return 2;
    case OverloadLevel::None: break;
    }
    return 0;
}

// Round to whole seconds, as the controller keeps and saves them.
int wholeSeconds(int ms)
{
    const int clamped = std::clamp(ms, StepAttenuatorFacade::kMinAutoAttTimeMs,
                                   StepAttenuatorFacade::kMaxAutoAttTimeMs);
    return (clamped + 500) / 1000;
}

QString timeSettleReason()
{
    return QStringLiteral("The Core keeps this time in whole seconds, from 1 to 3600.");
}

} // namespace

StepAttenuatorFacade::StepAttenuatorFacade(RadioModel* radio, QObject* parent)
    : QObject(parent)
    , m_radio(radio)
    , m_windowReason(QStringLiteral("Connect to the Core to change the attenuator and preamp."))
{
}

StepAttenuatorFacade::~StepAttenuatorFacade() = default;

void StepAttenuatorFacade::bindController(StepAttenuatorController* controller)
{
    if (m_controller == controller) {
        return;
    }
    for (const QMetaObject::Connection& c : std::as_const(m_controllerConnections)) {
        disconnect(c);
    }
    m_controllerConnections.clear();
    m_controller = controller;
    if (!controller) {
        return;
    }
    using C = StepAttenuatorController;
    const auto follow = [this](auto signal) {
        m_controllerConnections.append(
            connect(m_controller.data(), signal, this, [this]() { refresh(); }));
    };
    follow(&C::attenuationChanged);
    follow(&C::preampModeChanged);
    follow(&C::stepAttEnabledChanged);
    follow(&C::autoAttEnabledChanged);
    follow(&C::autoAttActiveChanged);
    follow(&C::autoAttModeChanged);
    follow(&C::autoAttUndoChanged);
    follow(&C::autoUndoDelayChanged);
    follow(&C::autoAttHoldChanged);
    follow(&C::attenuationRangeChanged);
    follow(&C::rx1PreampChanged);
    follow(&C::overloadStatusChanged);
    follow(&C::adcLinkedChanged);
    follow(&C::settingsReloaded);
    // R-R3-49 (parity Task 5).
    follow(&C::attOnTxEnabledChanged);
    follow(&C::attOnTxValueChanged);
    follow(&C::forceAttWhenPsOffChanged);
    follow(&C::rx2AttenuationChanged);
    follow(&C::adcRoutingChanged);
    follow(&C::rx2StepAttEnabledChanged);
    follow(&C::rx2AutoAttEnabledChanged);
    follow(&C::rx2AutoAttUndoChanged);
    follow(&C::rx2AutoUndoDelayChanged);
    follow(&C::rx2PreampModeChanged);
    refresh();
}

void StepAttenuatorFacade::setWindowAvailability(bool available, const QString& reason)
{
    const QString kept = available ? QString() : reason;
    if (m_windowAvailable == available && m_windowReason == kept) {
        return;
    }
    m_windowAvailable = available;
    m_windowReason = kept;
    emit windowAvailabilityChanged(available);
}

StepAttenuatorController* StepAttenuatorFacade::controller() const
{
    return m_controller.data();
}

bool StepAttenuatorFacade::isBound() const
{
    return !m_controller.isNull();
}

QString StepAttenuatorFacade::settleReason(const QByteArray& property) const
{
    return m_settleReasons.value(property);
}

bool StepAttenuatorFacade::applyRemoteProperty(const QByteArray& property, const QVariant& value)
{
    if (isBound()) {
        return false;
    }
    Values next = m_values;
    if (property == "minDb") {
        next.minDb = value.toInt();
    } else if (property == "maxDb") {
        next.maxDb = value.toInt();
    } else if (property == "autoAttApplied") {
        next.autoAttApplied = value.toBool();
    } else if (property == "overloadAdc0") {
        next.overloadAdc0 = value.toInt();
    } else if (property == "overloadAdc1") {
        next.overloadAdc1 = value.toInt();
    } else if (property == "adcLinked") {
        next.adcLinked = value.toBool();
    } else if (property == "rx2SliceMask") {
        next.rx2SliceMask = value.toInt();
    } else {
        return false;
    }
    publish(next);
    return true;
}

bool StepAttenuatorFacade::beginEdit(const char* property)
{
    m_settleReasons.remove(QByteArray(property));
    if (!m_editGate) {
        return true;
    }
    QString reason;
    if (m_editGate(&reason)) {
        return true;
    }
    if (reason.isEmpty()) {
        reason = QStringLiteral("The attenuator cannot be changed from here right now.");
    }
    emit editRejected(reason);
    return false;
}

void StepAttenuatorFacade::settle(const char* property, const QString& reason)
{
    m_settleReasons.insert(QByteArray(property), reason);
}

void StepAttenuatorFacade::setEnabled(bool on)
{
    if (!beginEdit("enabled")) {
        return;
    }
    if (StepAttenuatorController* c = m_controller.data()) {
        c->setStepAttEnabled(on);
        refresh();
        return;
    }
    Values next = m_values;
    next.enabled = on;
    publish(next);
}

void StepAttenuatorFacade::setAttenuationDb(int dB)
{
    if (!beginEdit("attenuationDb")) {
        return;
    }
    if (StepAttenuatorController* c = m_controller.data()) {
        const int lo = c->minAttenuation();
        const int hi = c->maxAttenuation();
        const int kept = std::clamp(dB, lo, hi);
        if (kept != dB) {
            settle("attenuationDb",
                   QStringLiteral("This radio's attenuator goes from %1 to %2 dB.").arg(lo).arg(hi));
        }
        c->setAttenuation(kept);
        refresh();
        return;
    }
    Values next = m_values;
    next.attenuationDb = dB;
    publish(next);
}

void StepAttenuatorFacade::setPreampMode(int mode)
{
    if (!beginEdit("preampMode")) {
        return;
    }
    if (StepAttenuatorController* c = m_controller.data()) {
        bool offered = mode >= kPreampModeFirst && mode <= kPreampModeLast;
        if (offered && m_radio) {
            // The same items the RX applet offers for this radio
            // (BoardCapsTable::preampItemsForBoard, from Thetis
            // SetComboPreampForHPSDR). A radio with no items takes any mode.
            const BoardCapabilities& caps = m_radio->boardCapabilities();
            const auto items = BoardCapsTable::preampItemsForBoard(caps.board,
                                                                   caps.hasAlexFilters);
            if (!items.empty()) {
                offered = std::any_of(items.begin(), items.end(),
                                      [mode](const BoardCapsTable::PreampItem& item) {
                    return item.modeInt == mode;
                });
            }
        }
        if (!offered) {
            settle("preampMode", QStringLiteral("This radio does not offer that preamp setting."));
        } else {
            c->setPreampMode(static_cast<PreampMode>(mode));
        }
        refresh();
        return;
    }
    Values next = m_values;
    next.preampMode = mode;
    publish(next);
}

void StepAttenuatorFacade::setRx1Preamp(bool on)
{
    if (!beginEdit("rx1Preamp")) {
        return;
    }
    if (StepAttenuatorController* c = m_controller.data()) {
        // Only dual-ADC P2 boards have it (the RX applet's own test,
        // BoardCapabilities::p2PreampPerAdc).
        const bool offered = m_radio && m_radio->boardCapabilities().p2PreampPerAdc;
        if (on && !offered) {
            settle("rx1Preamp", QStringLiteral("This radio has no second preamp."));
        } else {
            c->setRx1Preamp(on);
        }
        refresh();
        return;
    }
    Values next = m_values;
    next.rx1Preamp = on;
    publish(next);
}

void StepAttenuatorFacade::setAutoAttEnabled(bool on)
{
    if (!beginEdit("autoAttEnabled")) {
        return;
    }
    if (StepAttenuatorController* c = m_controller.data()) {
        c->setAutoAttEnabled(on);
        refresh();
        return;
    }
    Values next = m_values;
    next.autoAttEnabled = on;
    publish(next);
}

void StepAttenuatorFacade::setAutoAttMode(int mode)
{
    if (!beginEdit("autoAttMode")) {
        return;
    }
    if (StepAttenuatorController* c = m_controller.data()) {
        if (mode != static_cast<int>(AutoAttMode::Classic)
            && mode != static_cast<int>(AutoAttMode::Adaptive)) {
            settle("autoAttMode", QStringLiteral("The Core does not know that auto-attenuate mode."));
        } else {
            // The controller settles Adaptive to Classic on a radio without
            // per-step calibration (the Hermes Lite 2 among them).
            if (mode == static_cast<int>(AutoAttMode::Adaptive) && !c->hasStepAttenuatorCal()) {
                settle("autoAttMode", QStringLiteral("This radio offers only Classic auto-attenuate."));
            }
            c->setAutoAttMode(static_cast<AutoAttMode>(mode));
        }
        refresh();
        return;
    }
    Values next = m_values;
    next.autoAttMode = mode;
    publish(next);
}

void StepAttenuatorFacade::setAutoAttUndo(bool on)
{
    if (!beginEdit("autoAttUndo")) {
        return;
    }
    if (StepAttenuatorController* c = m_controller.data()) {
        c->setAutoAttUndo(on);
        refresh();
        return;
    }
    Values next = m_values;
    next.autoAttUndo = on;
    publish(next);
}

void StepAttenuatorFacade::setAutoAttUndoDelayMs(int ms)
{
    if (!beginEdit("autoAttUndoDelayMs")) {
        return;
    }
    if (StepAttenuatorController* c = m_controller.data()) {
        const int seconds = wholeSeconds(ms);
        if (seconds * 1000 != ms) {
            settle("autoAttUndoDelayMs", timeSettleReason());
        }
        c->setAutoUndoDelaySec(seconds);
        refresh();
        return;
    }
    Values next = m_values;
    next.autoAttUndoDelayMs = ms;
    publish(next);
}

void StepAttenuatorFacade::setAutoAttHoldMs(int ms)
{
    if (!beginEdit("autoAttHoldMs")) {
        return;
    }
    if (StepAttenuatorController* c = m_controller.data()) {
        const int seconds = wholeSeconds(ms);
        if (seconds * 1000 != ms) {
            settle("autoAttHoldMs", timeSettleReason());
        }
        c->setAutoAttHoldSeconds(static_cast<double>(seconds));
        refresh();
        return;
    }
    Values next = m_values;
    next.autoAttHoldMs = ms;
    publish(next);
}

// ── R-R3-49 (parity Task 5): ATT on TX, its value and Force ATT ──────────

bool StepAttenuatorFacade::isTransmitSetting(const QByteArray& property)
{
    return property == "attOnTxEnabled" || property == "attOnTxValue"
        || property == "forceAttWhenPsOff";
}

QString StepAttenuatorFacade::transmitSettingRefusal(const QByteArray& property,
                                                     const QVariant& value) const
{
    if (property != "attOnTxValue") {
        return {};
    }
    bool ok = false;
    const qlonglong dB = value.toLongLong(&ok);
    if (ok && dB >= m_values.minDb && dB <= kMaxAttOnTxDb) {
        return {};
    }
    return QStringLiteral("Choose an ATT on TX value from %1 to %2 dB.")
        .arg(m_values.minDb).arg(kMaxAttOnTxDb);
}

void StepAttenuatorFacade::setAttOnTxEnabled(bool on)
{
    if (!beginEdit("attOnTxEnabled")) {
        return;
    }
    if (StepAttenuatorController* c = m_controller.data()) {
        c->setAttOnTxEnabled(on);
        refresh();
        return;
    }
    Values next = m_values;
    next.attOnTxEnabled = on;
    publish(next);
}

void StepAttenuatorFacade::setAttOnTxValue(int dB)
{
    if (!beginEdit("attOnTxValue")) {
        return;
    }
    if (StepAttenuatorController* c = m_controller.data()) {
        // The controller clamps to [minAttenuation, 31] (setup.cs:3999-4008
        // through StepAttenuatorController::setAttOnTxValue).
        const int lo = c->minAttenuation();
        if (dB < lo || dB > kMaxAttOnTxDb) {
            settle("attOnTxValue", QStringLiteral("Choose an ATT on TX value from %1 to %2 dB.")
                                       .arg(lo).arg(kMaxAttOnTxDb));
        }
        c->setAttOnTxValue(dB);
        refresh();
        return;
    }
    Values next = m_values;
    next.attOnTxValue = dB;
    publish(next);
}

void StepAttenuatorFacade::setForceAttWhenPsOff(bool on)
{
    if (!beginEdit("forceAttWhenPsOff")) {
        return;
    }
    if (StepAttenuatorController* c = m_controller.data()) {
        c->setForceAttWhenPsOff(on);
        refresh();
        return;
    }
    Values next = m_values;
    next.forceAttWhenPsOff = on;
    publish(next);
}

void StepAttenuatorFacade::setRx2AttenuationDb(int dB)
{
    if (!beginEdit("rx2AttenuationDb")) {
        return;
    }
    if (StepAttenuatorController* c = m_controller.data()) {
        const int lo = c->minAttenuation();
        // Level Cal 2 review: RX2's own value stops at the second ADC's
        // field (StepAttenuatorController::rx2MaxAttenuation); linked, it
        // is RX1's and takes RX1's range.
        const bool linked = c->adcAttenuatorsLinked();
        const int hi = linked ? c->maxAttenuation() : c->rx2MaxAttenuation();
        const int kept = std::clamp(dB, lo, hi);
        if (kept != dB) {
            settle("rx2AttenuationDb",
                   linked ? QStringLiteral("This radio's attenuator goes from %1 to %2 dB.")
                                .arg(lo).arg(hi)
                          : QStringLiteral("RX2's attenuator goes from %1 to %2 dB.")
                                .arg(lo).arg(hi));
        }
        c->setRx2Attenuation(kept);
        refresh();
        return;
    }
    Values next = m_values;
    next.rx2AttenuationDb = dB;
    publish(next);
}

void StepAttenuatorFacade::setRx2StepAttEnabled(bool on)
{
    if (!beginEdit("rx2StepAttEnabled")) {
        return;
    }
    if (StepAttenuatorController* c = m_controller.data()) {
        c->setRx2StepAttEnabled(on);
        refresh();
        return;
    }
    Values next = m_values;
    next.rx2StepAttEnabled = on;
    publish(next);
}

void StepAttenuatorFacade::setRx2AutoAttEnabled(bool on)
{
    if (!beginEdit("rx2AutoAttEnabled")) {
        return;
    }
    if (StepAttenuatorController* c = m_controller.data()) {
        c->setRx2AutoAttEnabled(on);
        refresh();
        return;
    }
    Values next = m_values;
    next.rx2AutoAttEnabled = on;
    publish(next);
}

void StepAttenuatorFacade::setRx2AutoAttUndo(bool on)
{
    if (!beginEdit("rx2AutoAttUndo")) {
        return;
    }
    if (StepAttenuatorController* c = m_controller.data()) {
        c->setRx2AutoAttUndo(on);
        refresh();
        return;
    }
    Values next = m_values;
    next.rx2AutoAttUndo = on;
    publish(next);
}

void StepAttenuatorFacade::setRx2AutoAttUndoDelayMs(int ms)
{
    if (!beginEdit("rx2AutoAttUndoDelayMs")) {
        return;
    }
    if (StepAttenuatorController* c = m_controller.data()) {
        const int seconds = wholeSeconds(ms);
        if (seconds * 1000 != ms) {
            settle("rx2AutoAttUndoDelayMs", timeSettleReason());
        }
        c->setRx2AutoUndoDelaySec(seconds);
        refresh();
        return;
    }
    Values next = m_values;
    next.rx2AutoAttUndoDelayMs = ms;
    publish(next);
}

void StepAttenuatorFacade::setRx2PreampMode(int mode)
{
    if (!beginEdit("rx2PreampMode")) {
        return;
    }
    if (StepAttenuatorController* c = m_controller.data()) {
        bool offered = mode >= kPreampModeFirst && mode <= kPreampModeLast;
        if (offered && m_radio) {
            // The same items the RX applet offers a slice on the other ADC
            // (BoardCapsTable::rx2PreampItemsForBoard, from Thetis
            // comboRX2Preamp's lists).
            const BoardCapabilities& caps = m_radio->boardCapabilities();
            const auto items = BoardCapsTable::rx2PreampItemsForBoard(caps.board);
            if (!items.empty()) {
                offered = std::any_of(items.begin(), items.end(),
                                      [mode](const BoardCapsTable::PreampItem& item) {
                    return item.modeInt == mode;
                });
            }
        }
        if (!offered) {
            settle("rx2PreampMode",
                   QStringLiteral("This radio does not offer that preamp setting."));
        } else {
            c->setRx2PreampMode(static_cast<PreampMode>(mode));
        }
        refresh();
        return;
    }
    Values next = m_values;
    next.rx2PreampMode = mode;
    publish(next);
}

void StepAttenuatorFacade::setPreampModeForSlice(int sliceId, int mode)
{
    if (sliceUsesRx2(sliceId)) {
        setRx2PreampMode(mode);
    } else {
        setPreampMode(mode);
    }
}

void StepAttenuatorFacade::setAttenuationDbForSlice(int sliceId, int dB)
{
    if (sliceUsesRx2(sliceId)) {
        setRx2AttenuationDb(dB);
    } else {
        setAttenuationDb(dB);
    }
}

void StepAttenuatorFacade::refresh()
{
    const StepAttenuatorController* c = m_controller.data();
    if (!c) {
        return;
    }
    Values next;
    next.enabled = c->stepAttEnabled();
    next.attenuationDb = c->attenuatorDb();
    next.preampMode = static_cast<int>(c->preampMode());
    next.rx1Preamp = c->rx1Preamp();
    next.autoAttEnabled = c->autoAttEnabled();
    next.autoAttMode = static_cast<int>(c->autoAttMode());
    next.autoAttUndo = c->autoAttUndo();
    next.autoAttUndoDelayMs = c->autoUndoDelaySec() * 1000;
    next.autoAttHoldMs = c->adaptiveHoldMs();
    next.minDb = c->minAttenuation();
    next.maxDb = c->maxAttenuation();
    next.autoAttApplied = c->autoAttApplied();
    next.overloadAdc0 = overloadWireLevel(c->overloadLevel(0));
    next.overloadAdc1 = overloadWireLevel(c->overloadLevel(1));
    next.adcLinked = c->adcLinked();
    next.attOnTxEnabled = c->attOnTxEnabled();
    next.attOnTxValue = c->attOnTxValue();
    next.forceAttWhenPsOff = c->forceAttWhenPsOff();
    next.rx2AttenuationDb = c->rx2AttenuatorDb();
    next.rx2SliceMask = static_cast<int>(c->rx2SliceMask());
    next.rx2StepAttEnabled = c->rx2StepAttEnabled();
    next.rx2AutoAttEnabled = c->rx2AutoAttEnabled();
    next.rx2AutoAttUndo = c->rx2AutoAttUndo();
    next.rx2AutoAttUndoDelayMs = c->rx2AutoUndoDelaySec() * 1000;
    next.rx2PreampMode = static_cast<int>(c->rx2PreampMode());
    publish(next);
}

void StepAttenuatorFacade::publish(const Values& next)
{
    const Values before = m_values;
    m_values = next;
    if (before.enabled != next.enabled) { emit enabledChanged(next.enabled); }
    if (before.attenuationDb != next.attenuationDb) { emit attenuationDbChanged(next.attenuationDb); }
    if (before.preampMode != next.preampMode) { emit preampModeChanged(next.preampMode); }
    if (before.rx1Preamp != next.rx1Preamp) { emit rx1PreampChanged(next.rx1Preamp); }
    if (before.autoAttEnabled != next.autoAttEnabled) {
        emit autoAttEnabledChanged(next.autoAttEnabled);
    }
    if (before.autoAttMode != next.autoAttMode) { emit autoAttModeChanged(next.autoAttMode); }
    if (before.autoAttUndo != next.autoAttUndo) { emit autoAttUndoChanged(next.autoAttUndo); }
    if (before.autoAttUndoDelayMs != next.autoAttUndoDelayMs) {
        emit autoAttUndoDelayMsChanged(next.autoAttUndoDelayMs);
    }
    if (before.autoAttHoldMs != next.autoAttHoldMs) {
        emit autoAttHoldMsChanged(next.autoAttHoldMs);
    }
    if (before.attOnTxEnabled != next.attOnTxEnabled) {
        emit attOnTxEnabledChanged(next.attOnTxEnabled);
    }
    if (before.attOnTxValue != next.attOnTxValue) { emit attOnTxValueChanged(next.attOnTxValue); }
    if (before.forceAttWhenPsOff != next.forceAttWhenPsOff) {
        emit forceAttWhenPsOffChanged(next.forceAttWhenPsOff);
    }
    if (before.minDb != next.minDb) { emit minDbChanged(next.minDb); }
    if (before.maxDb != next.maxDb) { emit maxDbChanged(next.maxDb); }
    if (before.autoAttApplied != next.autoAttApplied) {
        emit autoAttAppliedChanged(next.autoAttApplied);
    }
    if (before.overloadAdc0 != next.overloadAdc0) { emit overloadAdc0Changed(next.overloadAdc0); }
    if (before.overloadAdc1 != next.overloadAdc1) { emit overloadAdc1Changed(next.overloadAdc1); }
    if (before.adcLinked != next.adcLinked) { emit adcLinkedChanged(next.adcLinked); }
    if (before.rx2AttenuationDb != next.rx2AttenuationDb) {
        emit rx2AttenuationDbChanged(next.rx2AttenuationDb);
    }
    if (before.rx2SliceMask != next.rx2SliceMask) { emit rx2SliceMaskChanged(next.rx2SliceMask); }
    if (before.rx2StepAttEnabled != next.rx2StepAttEnabled) {
        emit rx2StepAttEnabledChanged(next.rx2StepAttEnabled);
    }
    if (before.rx2AutoAttEnabled != next.rx2AutoAttEnabled) {
        emit rx2AutoAttEnabledChanged(next.rx2AutoAttEnabled);
    }
    if (before.rx2AutoAttUndo != next.rx2AutoAttUndo) {
        emit rx2AutoAttUndoChanged(next.rx2AutoAttUndo);
    }
    if (before.rx2AutoAttUndoDelayMs != next.rx2AutoAttUndoDelayMs) {
        emit rx2AutoAttUndoDelayMsChanged(next.rx2AutoAttUndoDelayMs);
    }
    if (before.rx2PreampMode != next.rx2PreampMode) {
        emit rx2PreampModeChanged(next.rx2PreampMode);
    }
}

} // namespace NereusSDR
