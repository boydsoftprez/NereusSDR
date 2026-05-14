#pragma once

// =================================================================
// src/gui/setup/TciLogWindow.h  (NereusSDR)
// =================================================================
//
// Phase 3J-1 closeout Item 2 (2026-05-12): TCI server message log
// viewer.  Modeless QDialog opened from the Setup -> CAT/Network/TCI
// "Show Log..." button; lifecycle is owned by MainWindow so the
// window survives the Setup dialog closing.
//
// Source-first reads:
//   - Thetis Setup.cs:22562-22566 [v2.10.3.13] -- btnShowLog_Click
//     opens console.ShowTCILog().  Thetis routes through Console, so
//     the window outlives the Setup form; NereusSDR mirrors that with
//     MainWindow ownership.
//
// NereusSDR-original Qt6 implementation: no direct C# UI port.  The
// widget choices (QPlainTextEdit with maximum-block trimming for fast
// append, QCheckBox/QComboBox/QPushButton from the existing Setup-page
// palette) are NereusSDR-native per CLAUDE.md
// feedback_source_first_ui_vs_dsp.md.
// =================================================================

#include <QDialog>

class QPlainTextEdit;
class QCheckBox;
class QComboBox;
class QPushButton;

namespace NereusSDR {

class TciLogWindow : public QDialog {
    Q_OBJECT

public:
    explicit TciLogWindow(QWidget* parent = nullptr);
    ~TciLogWindow() override;

public slots:
    // Append a new entry to the log view.  Called via Qt::QueuedConnection
    // from TciServer::messageLogged so emit-side never blocks.
    //
    //   direction: "in" (client -> server) or "out" (server -> client)
    //   peer:      "host:port" of the client involved
    //   text:      the TCI command line, trimmed of the trailing ';'
    //   epochMs:   QDateTime::currentMSecsSinceEpoch() at emit time
    //
    // Honors the auto-scroll, pause, and direction-filter UI state.  No-op
    // while paused.  When filter != "All", entries with mismatched
    // direction are still buffered into the underlying QPlainTextEdit so
    // toggling the filter back to All shows the full history.
    void appendEntry(const QString& direction,
                     const QString& peer,
                     const QString& text,
                     qint64 epochMs);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onClearClicked();
    void onPauseToggled(bool paused);
    void onFilterChanged(int index);
    void onAutoScrollToggled(bool on);

private:
    void buildUI();
    void restoreGeometryFromSettings();
    void saveGeometryToSettings();

    QPlainTextEdit* m_logView{nullptr};
    QCheckBox*      m_autoScrollCheck{nullptr};
    QPushButton*    m_pauseButton{nullptr};
    QPushButton*    m_clearButton{nullptr};
    QComboBox*      m_filterCombo{nullptr};

    bool m_paused{false};
    // Current filter: 0=All, 1=In only, 2=Out only.  Matches m_filterCombo
    // user-data indices set in buildUI().
    int  m_filter{0};
};

} // namespace NereusSDR
