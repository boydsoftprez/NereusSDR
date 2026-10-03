// =================================================================
// src/gui/ReceiveLayoutNotices.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R-R3-34 ("unsupported or failed
// restoration is explicit, with actionable recovery").
//
// The receive-layout restore status (RadioModel::receiveLayoutRestoreState
// and receiveLayoutRestoreMessage) says why receivers were not restored or
// were closed: a saved layout the radio cannot host, or slices a smaller
// board cannot take when it connects. A window connected to a Core shows it
// as a toast pointing at the Core connection panel, where the details stay.
// A local window (no Core) closes slices past the board's limit too, and
// shows the same message through the same toast, without the pointer to a
// panel it does not have. This decides, for each status change, whether a
// toast is news and what it says; MainWindow shows it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//                 Receiver and transmit gaps plan, Task 1 follow-up.
// =================================================================

#pragma once

#include <QCoreApplication>
#include <QString>

namespace NereusSDR {

class ReceiveLayoutNotices {
    Q_DECLARE_TR_FUNCTIONS(ReceiveLayoutNotices)

public:
    /// The toast for a receive-layout status of `state` with `message`, or
    /// empty when there should be none: an accepted restore clears the
    /// memory, and a warning ("invalid", "degraded", "fallback") is toasted
    /// once per distinct message. `viaCore`: this window shows the Core's
    /// status, whose details also stay in the Core connection panel.
    QString toastFor(const QString& state, const QString& message, bool viaCore)
    {
        if (state == QLatin1String("accepted")) {
            m_lastWarning.clear();
            return {};
        }
        const bool warning = state == QLatin1String("invalid")
            || state == QLatin1String("degraded") || state == QLatin1String("fallback");
        if (!warning || message.isEmpty() || message == m_lastWarning) {
            return {};
        }
        m_lastWarning = message;
        return viaCore ? tr("%1 Details remain in Core connection.").arg(message) : message;
    }

    /// Forget the last warning, so the same one is toasted again. A window
    /// with no Core calls this when its radio disconnects: its status never
    /// returns to "accepted", and the same closure on the next connect is a
    /// new event the operator must hear about (fix wave 1, M3).
    void forget() { m_lastWarning.clear(); }

private:
    QString m_lastWarning;
};

} // namespace NereusSDR
