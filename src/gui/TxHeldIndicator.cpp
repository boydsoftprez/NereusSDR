// =================================================================
// src/gui/TxHeldIndicator.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. See TxHeldIndicator.h.
//
// Modification history (NereusSDR):
//   2026-10-27: Created for remote transmit hold then cut (Task 7). J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/TxHeldIndicator.h"

#include "core/session/TransmitStateFacade.h"
#include "gui/widgets/StatusBadge.h"

#include <algorithm>

namespace NereusSDR {

TxHeldIndicator::TxHeldIndicator(QObject* parent)
    : QObject(parent)
{
    m_afterHold.setSingleShot(true);
    connect(&m_afterHold, &QTimer::timeout, this, &TxHeldIndicator::changed);
}

void TxHeldIndicator::setTransmitState(TransmitState* state)
{
    if (m_state == state) {
        return;
    }
    disconnect(m_holdConnection);
    m_state = state;
    m_afterHold.stop();
    m_evaluatePending = false;
    m_held = state != nullptr && state->rfHeld();
    m_heldReason = state != nullptr ? state->rfHeldReason() : QString();
    m_seenSerial = state != nullptr ? state->rfHoldSerial() : 0;
    m_sawHeld = m_held;
    if (state != nullptr) {
        // holdChanged comes once per property as a txState delta is
        // applied (rfHoldSerial before rfLastHoldMs), so the state is read
        // only once the whole delta is in, on the next event-loop turn.
        m_holdConnection = connect(state, &TransmitState::holdChanged,
                                   this, &TxHeldIndicator::scheduleEvaluate);
    }
    emit changed();
}

void TxHeldIndicator::setKeyed(bool keyed)
{
    if (m_keyed == keyed) {
        return;
    }
    m_keyed = keyed;
    if (!keyed) {
        // A hold shown after it ended is for this key only.
        m_afterHold.stop();
    }
    emit changed();
}

bool TxHeldIndicator::showsHeld() const
{
    return m_keyed && (m_held || m_afterHold.isActive());
}

void TxHeldIndicator::scheduleEvaluate()
{
    if (m_evaluatePending) {
        return;
    }
    m_evaluatePending = true;
    QMetaObject::invokeMethod(this, &TxHeldIndicator::evaluate, Qt::QueuedConnection);
}

void TxHeldIndicator::evaluate()
{
    if (!m_evaluatePending) {
        // Superseded by setTransmitState, which read the state itself.
        return;
    }
    m_evaluatePending = false;
    if (m_state == nullptr) {
        return;
    }
    const bool held = m_state->rfHeld();
    const quint64 serial = m_state->rfHoldSerial();
    if (serial > m_seenSerial) {
        // A hold ended. One this window never saw held (its link was the
        // one that went quiet) is shown for its length, capped, while
        // still keyed.
        const qint64 lastMs = std::min<qint64>(m_state->rfLastHoldMs(), kMaxAfterHoldMs);
        if (!m_sawHeld && !held && m_keyed && lastMs > 0) {
            m_afterHoldMs = static_cast<int>(lastMs);
            m_afterHold.start(m_afterHoldMs);
        }
        m_sawHeld = false;
    } else if (serial < m_seenSerial) {
        // The serial went down: another Core's count (clearStationValues
        // and a new Core's values). No hold ended here.
        m_afterHold.stop();
        m_sawHeld = false;
    }
    m_seenSerial = serial;
    if (held) {
        // A hold this window sees is shown as it is, not as one after.
        m_sawHeld = true;
        m_afterHold.stop();
    }
    m_held = held;
    m_heldReason = m_state->rfHeldReason();
    emit changed();
}

void TxHeldIndicator::applyTo(StatusBadge* badge, bool inhibited) const
{
    if (badge == nullptr) {
        return;
    }
    if (inhibited) {
        badge->setSvgIcon(QStringLiteral(":/icons/badge-prohibited.svg"));
        badge->setLabel(QStringLiteral("TX"));
        badge->setVariant(StatusBadge::Variant::Tx);
        return;
    }
    badge->setSvgIcon(QStringLiteral(":/icons/badge-dot.svg"));
    if (showsHeld()) {
        badge->setLabel(tr("Held"));
        badge->setVariant(StatusBadge::Variant::Warn);
        if (!m_held) {
            badge->setToolTip(tr("Transmitting (MOX engaged). The Core held RF off "
                                 "while this window's link was quiet."));
        } else if (m_heldReason == QLatin1String(TransmitState::kHoldMic)) {
            badge->setToolTip(tr("MOX engaged, RF held off: no microphone audio "
                                 "is arriving."));
        } else {
            badge->setToolTip(tr("MOX engaged, RF held off: the transmitting "
                                 "device's link went quiet."));
        }
        return;
    }
    badge->setLabel(QStringLiteral("TX"));
    badge->setVariant(m_keyed ? StatusBadge::Variant::Tx : StatusBadge::Variant::Off);
    badge->setToolTip(m_keyed ? tr("Transmitting (MOX engaged)") : tr("Receive (MOX off)"));
}

} // namespace NereusSDR
