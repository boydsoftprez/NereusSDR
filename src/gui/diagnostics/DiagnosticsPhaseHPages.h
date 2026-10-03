// =================================================================
// src/gui/diagnostics/DiagnosticsPhaseHPages.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Four sibling Diagnostics sub-tabs added in
// Phase 3P-H per spec §13:
//   - Connection Quality   (60 s history of latency/seq-gap/throttle)
//   - Settings Validation  (full audit list backed by SettingsHygiene)
//   - Export / Import      (per-MAC + global AppSettings XML round-trip)
//   - Logs                 (recent qCWarning/qCDebug viewer)
//
// SettingsValidation and ExportImportConfig are functional in this
// commit; ConnectionQuality and Logs render as placeholders pending
// follow-up wire-up tasks.
//
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-20 — Original implementation for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-23 - R-R3-21 / R-R3-10: Settings Validation's Reset and Forget change
//                 the radio's settings, which a remote window holds on
//                 the Core; they are disabled while it does not have them.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-26 - R-R3-32 (remote-window parity Task 14): Connection
//                 Quality's Live Counters title says "from the Core" in a
//                 remote window. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-27 - R-R3-49 (remote-window parity Task 22): Logs shows the
//                 Core's recent log in a remote window, and this
//                 computer's labelled; Refresh reads both again. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "gui/SetupPage.h"

#include <QByteArray>
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>

namespace NereusSDR {

class AppSettings;
class RadioModel;
class IStationLink;

// Diagnostics → Connection Quality (Phase H placeholder).
class ConnectionQualityPage : public SetupPage {
    Q_OBJECT
public:
    explicit ConnectionQualityPage(RadioModel* model = nullptr, QWidget* parent = nullptr);

private slots:
    void onTick();

private:
    RadioModel* m_model{nullptr};
    QLabel*     m_ep6BytesLabel{nullptr};
    QLabel*     m_ep2BytesLabel{nullptr};
    QLabel*     m_throttleLabel{nullptr};
    QLabel*     m_seqGapLabel{nullptr};
    QLabel*     m_historyPlaceholder{nullptr};
    // R-R3-32 (parity Task 14): says "from the Core" in a remote window.
    QGroupBox*  m_liveGroup{nullptr};

    void buildUI();
};

// Diagnostics → Settings Validation (Phase H, functional).
class SettingsValidationPage : public SetupPage {
    Q_OBJECT
public:
    explicit SettingsValidationPage(RadioModel* model = nullptr, QWidget* parent = nullptr);

    // R-R3-21 / R-R3-10: Reset and Forget change the radio's settings, which are
    // the Core's in a remote window, so they are disabled while the Core's
    // settings are unavailable. Refresh only reads.
    void setStationSettingsAvailable(bool available, const QString& reason) override;

private slots:
    void refresh();
    void onRevalidateClicked();
    void onRepairClicked();
    void onForgetClicked();

private:
    RadioModel*  m_model{nullptr};
    QListWidget* m_issueList{nullptr};
    QPushButton* m_repairBtn{nullptr};
    QPushButton* m_forgetBtn{nullptr};
    QPushButton* m_refreshBtn{nullptr};
    // The last availability pushed by setStationSettingsAvailable(). Read
    // again after a confirmation returns: the link can drop while it is
    // open (R3 Setup fix wave, final review M2).
    bool         m_stationSettingsAvailable{true};

    void buildUI();
};

// Diagnostics → Export / Import Config (Phase H, functional).
class ExportImportConfigPage : public SetupPage {
    Q_OBJECT
public:
    explicit ExportImportConfigPage(RadioModel* model = nullptr, QWidget* parent = nullptr);
    ~ExportImportConfigPage() override;

    void setStationSettingsAvailable(bool available, const QString& reason) override;

    /// The connected radio's own settings (everything kept under
    /// hardware/<mac>/) as a settings XML holding nothing else, or empty
    /// with `error` set. In a remote window `settings` reads the Core's
    /// values through its settings proxy.
    static QByteArray connectedRadioXml(const AppSettings& settings, const QString& mac,
                                        QString* error = nullptr);

protected:
    // The native picker and message boxes live at the page boundary so the
    // export transaction can be exercised without a modal desktop dialog.
    virtual QString chooseExportDestination(bool remote);
    virtual QString chooseRadioExportDestination(const QString& mac);
    virtual void showExportResult(bool success, const QString& text);
    void closeEvent(QCloseEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void onExportAllClicked();
    void onImportAllClicked();
    void onExportRadioClicked();

private:
    struct ExportCompletion {
        quint32 operationId {0};
        bool accepted {false};
        QString reason;
        QByteArray coreXml;
    };

    QPointer<RadioModel> m_model;
    QLabel*      m_settingsPathLabel{nullptr};
    QLabel*      m_exportExplanation{nullptr};
    QLabel*      m_importExplanation{nullptr};
    QLabel*      m_radioSummaryLabel{nullptr};
    QPushButton* m_exportAllBtn{nullptr};
    QPushButton* m_importAllBtn{nullptr};
    QPushButton* m_exportRadioBtn{nullptr};
    bool         m_stationSettingsAvailable{true};
    QString      m_stationUnavailableReason;
    quint64      m_linkGeneration{0};
    quint64      m_visibilityGeneration{0};
    quint64      m_pendingGeneration{0};
    IStationLink* m_pendingLink{nullptr};
    quint32      m_operationId{0};
    bool         m_exportPending{false};
    bool         m_requestStarting{false};
    bool         m_ownsExport{false};
    bool         m_shuttingDown{false};
    QByteArray   m_windowXml;
    QString      m_destination;
    QList<ExportCompletion> m_earlyCompletions;

    void buildUI();
    bool remoteWindow() const;
    bool exportAllowed(QString* reason) const;
    bool radioExportAllowed(QString* reason) const;
    void refreshExportAvailability();
    void onLinkStateChanged();
    void onExportCompleted(quint32 operationId, bool accepted, const QString& reason,
                           const QByteArray& coreXml);
    void finishExport(const ExportCompletion& completion);
    void clearPending(bool cancelOwned);
};

// Diagnostics → Logs. R-R3-21: shows the log file's recent lines, the
// ones Help > Support's log viewer shows (SupportDialog::logTailText),
// read again each time the page is shown or Refresh is pressed.
class LogsPage : public SetupPage {
    Q_OBJECT
public:
    explicit LogsPage(RadioModel* model = nullptr, QWidget* parent = nullptr);
    ~LogsPage() override;

    void refresh();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    // Remote-window parity Task 22: in a remote window the Core's recent
    // log (the `coreLog` stream) above this computer's.
    RadioModel*     m_model{nullptr};
    QPlainTextEdit* m_coreLogView{nullptr};
    bool            m_holdingCoreLog{false};
    QPlainTextEdit* m_logView{nullptr};
    QPushButton*    m_refreshBtn{nullptr};
    QPushButton*    m_clearBtn{nullptr};

    bool isRemote() const;
    void refreshCoreLog();
    void buildUI();
};

} // namespace NereusSDR
