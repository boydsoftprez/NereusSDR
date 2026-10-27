#pragma once
// =================================================================
// src/gui/TxHeldIndicator.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. Remote transmit hold then cut
// (decision 7): what the status bar's TX badge shows while the Core holds
// a remote key's RF silent. "Held" in amber (StatusBadge::Variant::Warn)
// while the TransmitState it follows reads rfHeld and this window is
// keyed; a window that never saw rfHeld true for a hold (its own link was
// the one that went quiet) shows "Held" for min(rfLastHoldMs, 3000) ms
// from when it learns the hold ended, only while still keyed. Otherwise
// the badge is what MOX says, and TX Inhibit's icon paints over all of it.
//
// Modification history (NereusSDR):
//   2026-10-27: Created for remote transmit hold then cut (Task 7). J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/TransmitStateFacade.h"
#include "gui/NereusGuiExport.h"

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

namespace NereusSDR {

class StatusBadge;

class NEREUS_GUI_EXPORT TxHeldIndicator : public QObject {
    Q_OBJECT

public:
    /// The longest "Held" a window shows for a hold it learns of only
    /// after it ended (the Core cuts a key at 3000 ms of link silence).
    static constexpr int kMaxAfterHoldMs = 3000;

    explicit TxHeldIndicator(QObject* parent = nullptr);

    /// The TransmitState to follow: the Core's own on the Core window, the
    /// client's mirrored copy on a remote window; nullptr follows none.
    /// Whatever it reads now is taken as already seen, never as a hold
    /// that just ended.
    void setTransmitState(TransmitState* state);
    TransmitState* transmitState() const { return m_state; }

    /// This window's MOX. Dropping it ends any "Held" at once.
    void setKeyed(bool keyed);
    bool keyed() const { return m_keyed; }

    /// The badge reads "Held": keyed, and the Core holds RF now or a hold
    /// this window did not see is still being shown.
    bool showsHeld() const;
    /// The length of the "Held" running for a hold that already ended, in
    /// ms; 0 when none is running.
    int afterHoldMs() const { return m_afterHold.isActive() ? m_afterHoldMs : 0; }

    /// Paints `badge`: TX Inhibit's prohibition icon at Variant::Tx while
    /// `inhibited` (its tooltip is the caller's, which names the reason);
    /// otherwise "Held" at Variant::Warn, "TX" at Variant::Tx while keyed,
    /// or "TX" at Variant::Off.
    void applyTo(StatusBadge* badge, bool inhibited) const;

signals:
    /// What the badge should show may have changed.
    void changed();

private:
    void scheduleEvaluate();
    void evaluate();

    QPointer<TransmitState> m_state;
    QMetaObject::Connection m_holdConnection;
    bool m_keyed{false};
    // The state as of the last whole txState delta applied.
    bool m_held{false};
    QString m_heldReason;
    quint64 m_seenSerial{0};
    // rfHeld was seen true for the hold that ends at the next serial.
    bool m_sawHeld{false};
    bool m_evaluatePending{false};
    int m_afterHoldMs{0};
    QTimer m_afterHold;
};

} // namespace NereusSDR
