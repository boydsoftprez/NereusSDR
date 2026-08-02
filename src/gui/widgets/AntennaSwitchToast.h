// =================================================================
// src/gui/widgets/AntennaSwitchToast.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Non-blocking bottom-right
// toast widget for antenna auto-switch notification (Phase 3F
// multi-pan UI). See Phase 3F design doc section 5 (antenna conflict
// policy) and
// docs/architecture/2026-05-26-phase3f-sub-epic-e-ui-atlas-plan.md
// Task 6.
//
// MainWindow consumer wire-up (subscribe to
// `RadioModel::antennaAutoSwitched` and instantiate this toast) lands
// when the antenna-auto-switch pipeline + signal are added in a later
// sub-epic. Widget is standalone-constructible today: pass a message
// string, position it, show() it, listen for undoRequested().
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27 Created in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//              with AI-assisted transformation via Anthropic Claude Code.
// =================================================================
//
// no-port-check: NereusSDR-original

#pragma once

#include <QTimer>
#include <QWidget>

namespace NereusSDR {

/// Non-blocking bottom-right toast for antenna auto-switch with Undo
/// button. 8s auto-dismiss. Designed to overlay MainWindow without
/// modal blocking. Caller positions and shows; the widget emits
/// `undoRequested()` if the operator clicks UNDO before auto-dismiss
/// fires, and closes itself either way.
class AntennaSwitchToast : public QWidget {
    Q_OBJECT
public:
    explicit AntennaSwitchToast(const QString& message, QWidget* parent = nullptr);
    ~AntennaSwitchToast() override;

signals:
    void undoRequested();

private:
    QTimer m_autoDismissTimer;
};

} // namespace NereusSDR
