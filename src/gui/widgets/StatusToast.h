// =================================================================
// src/gui/widgets/StatusToast.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Non-blocking notification
// toast for transient operator messages.
//
// Replaces QStatusBar::showMessage for every notification in
// MainWindow. The bottom bar is a single widget added with
// QStatusBar::addWidget (MainWindow.cpp, buildStatusBar), and Qt hides
// every non-permanent widget for the whole duration of a showMessage
// call. So a 6-second notice blanked the CH pill, the PureSignal
// indicator, the radio name, CAT/TCI state, PA and TX badges and the
// clock, all at once. Reported on the bench 2026-07-30: hitting TUNE
// with PureSignal active replaced the entire bar with one line of text.
//
// The notification itself is worth keeping, so it moves off the bar
// instead of being dropped.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-07-30 Created in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//              with AI-assisted transformation via Anthropic Claude Code.
// =================================================================
//
// no-port-check: NereusSDR-original

#pragma once

#include <QString>
#include <QTimer>
#include <QWidget>

namespace NereusSDR {

/// Severity of a transient notice. Namespace scope with a fixed
/// underlying type so MainWindow.h can forward-declare it, which keeps
/// that header on Qt includes and forward declarations only, as it is
/// everywhere else.
enum class ToastSeverity : int {
    Info,     ///< Neutral notice: TX handoff, auto-connect chatter.
    Warning,  ///< Something was refused or taken away but is recoverable.
    Error     ///< A protection or interlock actively blocked the operator.
};

/// Non-blocking bottom-right notification toast. Caller positions and
/// shows it; the toast closes itself when its timer expires or when the
/// operator clicks it. Deletes on close, so callers hold it by
/// QPointer if they track it at all.
///
/// Severity drives only the left accent bar, matching the badge palette
/// already used across the bottom bar so a warning reads the same here
/// as it does there.
class StatusToast : public QWidget {
    Q_OBJECT
public:
    StatusToast(const QString& message,
                ToastSeverity severity,
                int timeoutMs,
                QWidget* parent = nullptr);
    ~StatusToast() override;

    /// The message this toast is showing. Used to collapse a repeat of
    /// the same notice into the existing toast rather than stacking a
    /// duplicate underneath it.
    QString message() const { return m_message; }

    /// Restart the dismiss countdown. Called when an identical message
    /// arrives while this toast is still up.
    void refresh(int timeoutMs);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    QString m_message;
    QTimer  m_dismissTimer;
};

} // namespace NereusSDR
