// no-port-check: NereusSDR-original mirrored presentation of the Core's Alex
// antenna settings. All antenna logic stays in AlexController.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/accessories/AlexAntennaFacade.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See AlexAntennaFacade.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  Created. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R-R3-46 fix wave: one band's antenna at
//                                    a time. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-46 / R-R3-21: the filter policy
//                                    for a remote window
//                                    (setBpfModeForChain). AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 / R-R3-46 parity Task 12: the
//                                    TX antennas and relays two-way, and
//                                    the window's transmit edit
//                                    availability. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 / R-R3-46 parity mini-round:
//                                    setTxAntForBand and the TX band edit
//                                    sender. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/accessories/AlexAntennaFacade.h"

#include "core/accessories/AlexController.h"

#include <QStringList>

#include <algorithm>

namespace NereusSDR {

namespace {

constexpr int kAntFirst = 1;
constexpr int kAntLast = 3;
constexpr int kRxOnlyFirst = 0;

QString malformedListReason()
{
    return QStringLiteral("The Core keeps one antenna for each of its bands.");
}

QString antennaRangeReason()
{
    return QStringLiteral("Antennas are numbered 1 to 3.");
}

QString rxOnlyRangeReason()
{
    return QStringLiteral("The receive-only input is none or 1 to 3.");
}

QString txBlockedReason()
{
    return QStringLiteral("An antenna blocked for transmit cannot be a band's TX antenna.");
}

QString connectReason()
{
    return QStringLiteral("Connect to the Core to change the radio's hardware settings.");
}

} // namespace

AlexAntennaFacade::AlexAntennaFacade(QObject* parent)
    : QObject(parent)
    , m_windowReason(connectReason())
    , m_txAntennasReason(connectReason())
    , m_rxBypassReason(connectReason())
    , m_values(defaults())
{
}

AlexAntennaFacade::~AlexAntennaFacade() = default;

AlexAntennaFacade::Values AlexAntennaFacade::defaults()
{
    // AlexController's own defaults: every band on Ant 1, no RX-only input.
    Values v;
    v.rxAnt.fill(kAntFirst);
    v.txAnt.fill(kAntFirst);
    v.rxOnlyAnt.fill(kRxOnlyFirst);
    return v;
}

void AlexAntennaFacade::bindController(AlexController* controller)
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
    using C = AlexController;
    const auto follow = [this](auto signal) {
        m_controllerConnections.append(
            connect(m_controller.data(), signal, this, [this]() { refresh(); }));
    };
    follow(&C::antennaChanged);
    follow(&C::rxOnlyAntChanged);
    follow(&C::blockTxChanged);
    follow(&C::rxOutOnTxChanged);
    follow(&C::ext1OutOnTxChanged);
    follow(&C::ext2OutOnTxChanged);
    follow(&C::rxOutOverrideChanged);
    follow(&C::useTxAntForRxChanged);
    refresh();
}

AlexController* AlexAntennaFacade::controller() const
{
    return m_controller.data();
}

bool AlexAntennaFacade::isBound() const
{
    return !m_controller.isNull();
}

void AlexAntennaFacade::setWindowAvailability(bool available, const QString& reason)
{
    const QString kept = available ? QString() : reason;
    if (m_windowAvailable == available && m_windowReason == kept) {
        return;
    }
    m_windowAvailable = available;
    m_windowReason = kept;
    emit windowAvailabilityChanged(available);
}

void AlexAntennaFacade::setTransmitEditAvailability(bool txAntennas,
                                                    const QString& txAntennasReason,
                                                    bool rxBypass,
                                                    const QString& rxBypassReason)
{
    const QString txKept = txAntennas ? QString() : txAntennasReason;
    const QString bypassKept = rxBypass ? QString() : rxBypassReason;
    if (m_txAntennasEditable == txAntennas && m_txAntennasReason == txKept
        && m_rxBypassEditable == rxBypass && m_rxBypassReason == bypassKept) {
        return;
    }
    m_txAntennasEditable = txAntennas;
    m_txAntennasReason = txKept;
    m_rxBypassEditable = rxBypass;
    m_rxBypassReason = bypassKept;
    emit transmitEditAvailabilityChanged();
}

QString AlexAntennaFacade::settleReason(const QByteArray& property) const
{
    return m_settleReasons.value(property);
}

QString AlexAntennaFacade::encode(const BandList& list)
{
    QStringList parts;
    parts.reserve(kBandCount);
    for (int value : list) {
        parts.append(QString::number(value));
    }
    return parts.join(QLatin1Char(','));
}

bool AlexAntennaFacade::decode(const QString& text, int lo, int hi, BandList* out, bool* clamped)
{
    // 15 entries, one per per-band state slot (160m .. XVTR, then 2 m), or
    // the 14 without 2 m from a peer built before it (BandLinkFit.h): 2 m
    // then keeps the value `out` holds.
    const QStringList parts = text.split(QLatin1Char(','));
    if (parts.size() != kBandCount && parts.size() != kBandCount - 1) {
        return false;
    }
    BandList parsed = *out;
    bool moved = false;
    for (int b = 0; b < parts.size(); ++b) {
        bool ok = false;
        const int value = parts.at(b).trimmed().toInt(&ok);
        if (!ok) {
            return false;
        }
        const int kept = std::clamp(value, lo, hi);
        moved = moved || kept != value;
        parsed[static_cast<std::size_t>(b)] = kept;
    }
    *out = parsed;
    if (clamped) {
        *clamped = moved;
    }
    return true;
}

QString AlexAntennaFacade::rxAntennas() const { return encode(m_values.rxAnt); }
QString AlexAntennaFacade::rxOnlyAntennas() const { return encode(m_values.rxOnlyAnt); }
QString AlexAntennaFacade::txAntennas() const { return encode(m_values.txAnt); }

int AlexAntennaFacade::rxAnt(Band band) const
{
    const int b = perBandStateSlot(band);
    return (b >= 0 && b < kBandCount) ? m_values.rxAnt[static_cast<std::size_t>(b)] : kAntFirst;
}

int AlexAntennaFacade::rxOnlyAnt(Band band) const
{
    const int b = perBandStateSlot(band);
    return (b >= 0 && b < kBandCount) ? m_values.rxOnlyAnt[static_cast<std::size_t>(b)]
                                      : kRxOnlyFirst;
}

int AlexAntennaFacade::txAnt(Band band) const
{
    const int b = perBandStateSlot(band);
    return (b >= 0 && b < kBandCount) ? m_values.txAnt[static_cast<std::size_t>(b)] : kAntFirst;
}

bool AlexAntennaFacade::applyRemoteProperty(const QByteArray& property, const QVariant& value)
{
    if (isBound()) {
        return false;
    }
    Values next = m_values;
    if (property == "txAntennas") {
        BandList list = m_values.txAnt;
        if (!decode(value.toString(), kAntFirst, kAntLast, &list, nullptr)) {
            return true;  // ours, but not a list we can show: keep the last one
        }
        next.txAnt = list;
    } else if (property == "blockTxAnt2") {
        next.blockTxAnt2 = value.toBool();
    } else if (property == "blockTxAnt3") {
        next.blockTxAnt3 = value.toBool();
    } else if (property == "rxOutOnTx") {
        next.rxOutOnTx = value.toBool();
    } else if (property == "ext1OutOnTx") {
        next.ext1OutOnTx = value.toBool();
    } else if (property == "ext2OutOnTx") {
        next.ext2OutOnTx = value.toBool();
    } else if (property == "rxOutOverride") {
        next.rxOutOverride = value.toBool();
    } else {
        return false;
    }
    publish(next);
    return true;
}

bool AlexAntennaFacade::beginEdit(const char* property)
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
        reason = QStringLiteral("The antennas cannot be changed from here right now.");
    }
    emit editRejected(reason);
    return false;
}

void AlexAntennaFacade::settle(const char* property, const QString& reason)
{
    m_settleReasons.insert(QByteArray(property), reason);
}

void AlexAntennaFacade::setRxAntennas(const QString& list)
{
    if (!beginEdit("rxAntennas")) {
        return;
    }
    BandList parsed = m_values.rxAnt;
    bool clamped = false;
    if (!decode(list, kAntFirst, kAntLast, &parsed, &clamped)) {
        settle("rxAntennas", malformedListReason());
        return;
    }
    if (clamped) {
        settle("rxAntennas", antennaRangeReason());
    }
    if (AlexController* c = m_controller.data()) {
        for (int b = 0; b < kBandCount; ++b) {
            const int want = parsed[static_cast<std::size_t>(b)];
            if (c->rxAnt(bandFromPerBandStateSlot(b)) != want) {
                c->setRxAnt(bandFromPerBandStateSlot(b), want);
            }
        }
        refresh();
        return;
    }
    Values next = m_values;
    next.rxAnt = parsed;
    publish(next);
}

void AlexAntennaFacade::setRxOnlyAntennas(const QString& list)
{
    if (!beginEdit("rxOnlyAntennas")) {
        return;
    }
    BandList parsed = m_values.rxOnlyAnt;
    bool clamped = false;
    if (!decode(list, kRxOnlyFirst, kAntLast, &parsed, &clamped)) {
        settle("rxOnlyAntennas", malformedListReason());
        return;
    }
    if (clamped) {
        settle("rxOnlyAntennas", rxOnlyRangeReason());
    }
    if (AlexController* c = m_controller.data()) {
        for (int b = 0; b < kBandCount; ++b) {
            const int want = parsed[static_cast<std::size_t>(b)];
            if (c->rxOnlyAnt(bandFromPerBandStateSlot(b)) != want) {
                c->setRxOnlyAnt(bandFromPerBandStateSlot(b), want);
            }
        }
        refresh();
        return;
    }
    Values next = m_values;
    next.rxOnlyAnt = parsed;
    publish(next);
}

void AlexAntennaFacade::setUseTxAntennaForRx(bool on)
{
    if (!beginEdit("useTxAntennaForRx")) {
        return;
    }
    if (AlexController* c = m_controller.data()) {
        c->setUseTxAntForRx(on);
        refresh();
        return;
    }
    Values next = m_values;
    next.useTxAntForRx = on;
    publish(next);
}

void AlexAntennaFacade::setRxOutOnTx(bool on)
{
    if (!beginEdit("rxOutOnTx")) {
        return;
    }
    if (AlexController* c = m_controller.data()) {
        c->setRxOutOnTx(on);
        refresh();
        return;
    }
    Values next = m_values;
    next.rxOutOnTx = on;
    publish(next);
}

void AlexAntennaFacade::setTxAntennas(const QString& list)
{
    if (!beginEdit("txAntennas")) {
        return;
    }
    BandList parsed = m_values.txAnt;
    bool clamped = false;
    if (!decode(list, kAntFirst, kAntLast, &parsed, &clamped)) {
        settle("txAntennas", malformedListReason());
        return;
    }
    if (clamped) {
        settle("txAntennas", antennaRangeReason());
    }
    if (AlexController* c = m_controller.data()) {
        // The local tab's call, one band at a time: the controller keeps a
        // band on its antenna when the one asked for is blocked for transmit.
        bool kept = false;
        for (int b = 0; b < kBandCount; ++b) {
            const int want = parsed[static_cast<std::size_t>(b)];
            if (c->txAnt(bandFromPerBandStateSlot(b)) != want) {
                c->setTxAnt(bandFromPerBandStateSlot(b), want);
                kept = kept || c->txAnt(bandFromPerBandStateSlot(b)) != want;
            }
        }
        if (kept && !clamped) {
            settle("txAntennas", txBlockedReason());
        }
        refresh();
        return;
    }
    Values next = m_values;
    next.txAnt = parsed;
    publish(next);
}

void AlexAntennaFacade::setBlockTxAnt2(bool on)
{
    if (!beginEdit("blockTxAnt2")) {
        return;
    }
    if (AlexController* c = m_controller.data()) {
        c->setBlockTxAnt2(on);
        refresh();
        return;
    }
    Values next = m_values;
    next.blockTxAnt2 = on;
    publish(next);
}

void AlexAntennaFacade::setBlockTxAnt3(bool on)
{
    if (!beginEdit("blockTxAnt3")) {
        return;
    }
    if (AlexController* c = m_controller.data()) {
        c->setBlockTxAnt3(on);
        refresh();
        return;
    }
    Values next = m_values;
    next.blockTxAnt3 = on;
    publish(next);
}

void AlexAntennaFacade::setExt1OutOnTx(bool on)
{
    if (!beginEdit("ext1OutOnTx")) {
        return;
    }
    if (AlexController* c = m_controller.data()) {
        c->setExt1OutOnTx(on);
        refresh();
        return;
    }
    Values next = m_values;
    next.ext1OutOnTx = on;
    publish(next);
}

void AlexAntennaFacade::setExt2OutOnTx(bool on)
{
    if (!beginEdit("ext2OutOnTx")) {
        return;
    }
    if (AlexController* c = m_controller.data()) {
        c->setExt2OutOnTx(on);
        refresh();
        return;
    }
    Values next = m_values;
    next.ext2OutOnTx = on;
    publish(next);
}

void AlexAntennaFacade::setRxOutOverride(bool on)
{
    if (!beginEdit("rxOutOverride")) {
        return;
    }
    if (AlexController* c = m_controller.data()) {
        c->setRxOutOverride(on);
        refresh();
        return;
    }
    Values next = m_values;
    next.rxOutOverride = on;
    publish(next);
}

QString AlexAntennaFacade::setRxAntForBand(Band band, int antenna)
{
    AlexController* c = m_controller.data();
    const int b = perBandStateSlot(band);
    if (!c) {
        return QStringLiteral("The Core has no antenna settings ready.");
    }
    if (b < 0 || b >= kBandCount) {
        return malformedListReason();
    }
    if (antenna < kAntFirst || antenna > kAntLast) {
        return antennaRangeReason();
    }
    if (c->rxAnt(band) != antenna) {
        c->setRxAnt(band, antenna);
    }
    refresh();
    return c->rxAnt(band) == antenna
        ? QString() : QStringLiteral("The Core kept this band's antenna.");
}

QString AlexAntennaFacade::setRxOnlyAntForBand(Band band, int antenna)
{
    AlexController* c = m_controller.data();
    const int b = perBandStateSlot(band);
    if (!c) {
        return QStringLiteral("The Core has no antenna settings ready.");
    }
    if (b < 0 || b >= kBandCount) {
        return malformedListReason();
    }
    if (antenna < kRxOnlyFirst || antenna > kAntLast) {
        return rxOnlyRangeReason();
    }
    if (c->rxOnlyAnt(band) != antenna) {
        c->setRxOnlyAnt(band, antenna);
    }
    refresh();
    return c->rxOnlyAnt(band) == antenna
        ? QString() : QStringLiteral("The Core kept this band's receive-only input.");
}

QString AlexAntennaFacade::setTxAntForBand(Band band, int antenna)
{
    AlexController* c = m_controller.data();
    const int b = perBandStateSlot(band);
    if (!c) {
        return QStringLiteral("The Core has no antenna settings ready.");
    }
    if (b < 0 || b >= kBandCount) {
        return malformedListReason();
    }
    if (antenna < kAntFirst || antenna > kAntLast) {
        return antennaRangeReason();
    }
    // The local grid's call: the controller keeps a band on its antenna
    // when the one asked for is blocked for transmit.
    if (c->txAnt(band) != antenna) {
        c->setTxAnt(band, antenna);
    }
    refresh();
    return c->txAnt(band) == antenna ? QString() : txBlockedReason();
}

QString AlexAntennaFacade::setBpfModeForChain(int chain, int mode)
{
    AlexController* c = m_controller.data();
    if (!c) {
        return QStringLiteral("The Core has no filter settings ready.");
    }
    if (chain < 0 || chain >= kFilterChainCount) {
        return QStringLiteral("The Core has two receive filter chains, 0 and 1.");
    }
    if (mode < static_cast<int>(AlexController::BpfMode::Auto)
        || mode > static_cast<int>(AlexController::BpfMode::ForceBypass)) {
        return QStringLiteral("The filter policy is Auto, Force filter or Force bypass.");
    }
    const auto wanted = static_cast<AlexController::BpfMode>(mode);
    if (c->bpfMode(chain) == wanted) {
        return {};
    }
    // The local filter policy dialog's Apply makes this same call.
    c->setBpfMode(chain, wanted);
    if (c->bpfMode(chain) != wanted) {
        return QStringLiteral("The Core kept this chain's filter policy.");
    }
    return {};
}

bool AlexAntennaFacade::sendBandEdit(const char* property, Band band, int ant, bool rxOnly)
{
    if (isBound() || !m_bandEditSender) {
        return false;
    }
    if (!beginEdit(property)) {
        emit bandEditRefused();
        return true;
    }
    QString reason;
    if (!m_bandEditSender(band, ant, rxOnly, &reason)) {
        if (reason.isEmpty()) {
            reason = QStringLiteral("The antennas cannot be changed from here right now.");
        }
        emit editRejected(reason);
        emit bandEditRefused();
    }
    return true;
}

bool AlexAntennaFacade::sendTxBandEdit(Band band, int ant)
{
    if (isBound() || !m_txBandEditSender) {
        return false;
    }
    if (!beginEdit("txAntennas")) {
        emit bandEditRefused();
        return true;
    }
    QString reason;
    if (!m_txBandEditSender(band, ant, &reason)) {
        if (reason.isEmpty()) {
            reason = QStringLiteral("The antennas cannot be changed from here right now.");
        }
        emit editRejected(reason);
        emit bandEditRefused();
    }
    return true;
}

void AlexAntennaFacade::setRxAnt(Band band, int ant)
{
    const int b = perBandStateSlot(band);
    if (b < 0 || b >= kBandCount) {
        return;
    }
    if (sendBandEdit("rxAntennas", band, ant, false)) {
        return;
    }
    BandList list = m_values.rxAnt;
    list[static_cast<std::size_t>(b)] = ant;
    setRxAntennas(encode(list));
}

void AlexAntennaFacade::setRxOnlyAnt(Band band, int ant)
{
    const int b = perBandStateSlot(band);
    if (b < 0 || b >= kBandCount) {
        return;
    }
    if (sendBandEdit("rxOnlyAntennas", band, ant, true)) {
        return;
    }
    BandList list = m_values.rxOnlyAnt;
    list[static_cast<std::size_t>(b)] = ant;
    setRxOnlyAntennas(encode(list));
}

void AlexAntennaFacade::setTxAnt(Band band, int ant)
{
    const int b = perBandStateSlot(band);
    if (b < 0 || b >= kBandCount) {
        return;
    }
    // Parity mini-round: the band alone when the Core takes it
    // (setAlexTxAntenna); otherwise the whole list, as before.
    if (sendTxBandEdit(band, ant)) {
        return;
    }
    BandList list = m_values.txAnt;
    list[static_cast<std::size_t>(b)] = ant;
    setTxAntennas(encode(list));
}

void AlexAntennaFacade::refresh()
{
    const AlexController* c = m_controller.data();
    if (!c) {
        return;
    }
    Values next;
    for (int b = 0; b < kBandCount; ++b) {
        const auto i = static_cast<std::size_t>(b);
        next.rxAnt[i] = c->rxAnt(bandFromPerBandStateSlot(b));
        next.rxOnlyAnt[i] = c->rxOnlyAnt(bandFromPerBandStateSlot(b));
        next.txAnt[i] = c->txAnt(bandFromPerBandStateSlot(b));
    }
    next.useTxAntForRx = c->useTxAntForRx();
    next.blockTxAnt2 = c->blockTxAnt2();
    next.blockTxAnt3 = c->blockTxAnt3();
    next.rxOutOnTx = c->rxOutOnTx();
    next.ext1OutOnTx = c->ext1OutOnTx();
    next.ext2OutOnTx = c->ext2OutOnTx();
    next.rxOutOverride = c->rxOutOverride();
    publish(next);
}

void AlexAntennaFacade::publish(const Values& next)
{
    const Values before = m_values;
    m_values = next;
    if (before.rxAnt != next.rxAnt) { emit rxAntennasChanged(encode(next.rxAnt)); }
    if (before.rxOnlyAnt != next.rxOnlyAnt) { emit rxOnlyAntennasChanged(encode(next.rxOnlyAnt)); }
    if (before.useTxAntForRx != next.useTxAntForRx) {
        emit useTxAntennaForRxChanged(next.useTxAntForRx);
    }
    if (before.txAnt != next.txAnt) { emit txAntennasChanged(encode(next.txAnt)); }
    if (before.blockTxAnt2 != next.blockTxAnt2) { emit blockTxAnt2Changed(next.blockTxAnt2); }
    if (before.blockTxAnt3 != next.blockTxAnt3) { emit blockTxAnt3Changed(next.blockTxAnt3); }
    if (before.rxOutOnTx != next.rxOutOnTx) { emit rxOutOnTxChanged(next.rxOutOnTx); }
    if (before.ext1OutOnTx != next.ext1OutOnTx) { emit ext1OutOnTxChanged(next.ext1OutOnTx); }
    if (before.ext2OutOnTx != next.ext2OutOnTx) { emit ext2OutOnTxChanged(next.ext2OutOnTx); }
    if (before.rxOutOverride != next.rxOutOverride) {
        emit rxOutOverrideChanged(next.rxOutOverride);
    }
}

} // namespace NereusSDR
