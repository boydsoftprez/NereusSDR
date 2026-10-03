#pragma once

#include <QDialog>
#include <QPlainTextEdit>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QMap>
#include <QTimer>

namespace NereusSDR {

class RadioModel;

// Support dialog with diagnostic logging controls, log viewer,
// and support bundle creation for bug reports.
//
// Access via Help → Support... or Tools → Network Diagnostics...
//
// Remote-window parity Task 22 (R-R3-49): in a remote window the logging
// checkboxes turn the Core's categories on and off (radio's logCategories,
// support.setLogCategories), the Core's recent log shows beside this
// computer's, and Create Support Bundle carries the Core's bundle
// (support.collect) under core/ beside this computer's. Every bundle is
// written on a worker thread. J.J. Boyd (KG4VCF), 2026-09-27, AI-assisted
// via Anthropic Claude Code.
class SupportDialog : public QDialog {
    Q_OBJECT

public:
    explicit SupportDialog(RadioModel* model, QWidget* parent = nullptr);
    ~SupportDialog() override;

    /// R-R3-21: the last 200 KB of the log file, or a one-line reason why
    /// there is none. The log viewer here and Setup > Diagnostics > Logs
    /// show the same text.
    static QString logTailText();

    /// How long a remote window waits for the Core's bundle before it
    /// writes its own with the reason instead.
    static constexpr int kCoreBundleWaitMs = 60000;

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void onRefresh();
    void onClearLog();
    void onOpenLogFolder();
    void onEnableAll();
    void onDisableAll();
    void onCreateBundle();
    void onCategoryToggled(const QString& id, bool on);

private:
    void buildUI();
    void refreshLogViewer();
    void updateLogInfo();
    bool isRemote() const;
    // Remote window: the checkboxes show the Core's categories, enabled
    // while the Core takes a change.
    void syncCoreCategories();
    void sendCoreCategories();
    void refreshCoreLogViewer();
    void writeBundle(bool withCore, const QByteArray& coreBundle, const QString& coreReason);
    void bundleWritten(const QString& path);

    RadioModel* m_radioModel;

    // Category checkboxes keyed by category id
    QMap<QString, QCheckBox*> m_categoryChecks;
    QPushButton* m_enableAllBtn{nullptr};
    QPushButton* m_disableAllBtn{nullptr};
    QLabel* m_categoryReason{nullptr};

    // Log viewer
    QPlainTextEdit* m_logViewer{nullptr};
    QLabel* m_logPathLabel{nullptr};
    QLabel* m_logSizeLabel{nullptr};
    // Remote window: the Core's recent log.
    QLabel* m_coreLogLabel{nullptr};
    QPlainTextEdit* m_coreLogViewer{nullptr};
    bool m_holdingCoreLog{false};

    QPushButton* m_bundleBtn{nullptr};
    quint32 m_coreBundleCommand{0};
    bool m_waitingForCore{false};
    QTimer m_coreBundleTimeout;

    // Status
    QLabel* m_statusLabel{nullptr};

    static constexpr int kMaxLogViewBytes = 200 * 1024;  // 200 KB tail
    static constexpr int kMaxLogViewLines = 2000;
};

} // namespace NereusSDR
