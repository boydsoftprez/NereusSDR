#include "SupportDialog.h"
#include "StyleConstants.h"
#include "core/LogCategories.h"
#include "core/SupportBundle.h"
#include "models/RadioModel.h"
#include "core/session/IStationLink.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFile>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QFont>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QHideEvent>
#include <QShowEvent>

namespace NereusSDR {

SupportDialog::SupportDialog(RadioModel* model, QWidget* parent)
    : QDialog(parent)
    , m_radioModel(model)
{
    setWindowTitle(QStringLiteral("Support & Diagnostics"));
    setMinimumSize(650, 550);
    resize(700, 650);

    buildUI();
    refreshLogViewer();

    m_coreBundleTimeout.setSingleShot(true);
    m_coreBundleTimeout.setInterval(kCoreBundleWaitMs);
    connect(&m_coreBundleTimeout, &QTimer::timeout, this, [this]() {
        if (!m_waitingForCore) {
            return;
        }
        m_waitingForCore = false;
        writeBundle(true, {}, QStringLiteral("The Core did not send its support bundle in time."));
    });
    if (m_radioModel != nullptr && isRemote()) {
        connect(m_radioModel, &RadioModel::logCategoriesChanged, this,
                [this](const QString&) { syncCoreCategories(); });
        connect(m_radioModel, &RadioModel::stationSupportAvailabilityChanged, this,
                [this]() { syncCoreCategories(); refreshCoreLogViewer(); });
        connect(m_radioModel, &RadioModel::stationCoreLogChanged, this,
                &SupportDialog::refreshCoreLogViewer);
        connect(m_radioModel, &RadioModel::stationLogCategoriesRefused, this,
                [this](const QString& reason) {
            m_statusLabel->setText(reason);
            syncCoreCategories();
        });
        connect(m_radioModel, &RadioModel::stationSupportBundleFinished, this,
                [this](quint32 commandId, bool accepted, const QString& reason,
                       const QByteArray& bundle) {
            if (!m_waitingForCore || commandId != m_coreBundleCommand) {
                return;
            }
            m_waitingForCore = false;
            m_coreBundleTimeout.stop();
            writeBundle(true, accepted ? bundle : QByteArray(),
                        accepted ? QString()
                                 : (reason.isEmpty()
                                        ? QStringLiteral("The Core did not send its support "
                                                         "bundle.")
                                        : reason));
        });
        syncCoreCategories();
    }
}

bool SupportDialog::isRemote() const
{
    return m_radioModel != nullptr && m_radioModel->role() == RadioModel::Role::Remote;
}

void SupportDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (isRemote() && !m_holdingCoreLog) {
        m_holdingCoreLog = true;
        m_radioModel->addStationCoreLogViewer();
        refreshCoreLogViewer();
    }
}

void SupportDialog::hideEvent(QHideEvent* event)
{
    QDialog::hideEvent(event);
    if (m_holdingCoreLog) {
        m_holdingCoreLog = false;
        m_radioModel->removeStationCoreLogViewer();
    }
}

void SupportDialog::syncCoreCategories()
{
    if (!isRemote()) {
        return;
    }
    const QString reason = m_radioModel->stationSupportUnavailableReason();
    const QStringList on = m_radioModel->logCategories().split(QLatin1Char(','),
                                                              Qt::SkipEmptyParts);
    for (auto it = m_categoryChecks.begin(); it != m_categoryChecks.end(); ++it) {
        it.value()->blockSignals(true);
        it.value()->setChecked(on.contains(it.key()));
        it.value()->blockSignals(false);
        it.value()->setEnabled(reason.isEmpty());
    }
    m_enableAllBtn->setEnabled(reason.isEmpty());
    m_disableAllBtn->setEnabled(reason.isEmpty());
    m_categoryReason->setText(reason);
    m_categoryReason->setVisible(!reason.isEmpty());
}

void SupportDialog::sendCoreCategories()
{
    QStringList on;
    for (auto it = m_categoryChecks.cbegin(); it != m_categoryChecks.cend(); ++it) {
        if (it.value()->isChecked()) {
            on.append(it.key());
        }
    }
    IStationLink* link = m_radioModel->stationLink();
    const IStationLink::CommandOutcome outcome = link != nullptr
        ? link->requestLogCategories(on.join(QLatin1Char(',')))
        : IStationLink::CommandOutcome{false, QStringLiteral("Connect to the Core to change its "
                                                             "logging.")};
    if (!outcome.sent) {
        m_statusLabel->setText(outcome.reason);
        syncCoreCategories();
    }
}

void SupportDialog::refreshCoreLogViewer()
{
    if (m_coreLogViewer == nullptr) {
        return;
    }
    const QString reason = m_radioModel->stationSupportUnavailableReason();
    if (!reason.isEmpty()) {
        m_coreLogViewer->setPlainText(reason);
        return;
    }
    const QStringList lines = m_radioModel->stationCoreLog();
    m_coreLogViewer->setPlainText(lines.isEmpty()
                                      ? QStringLiteral("Reading the Core's log...")
                                      : lines.join(QLatin1Char('\n')));
    m_coreLogViewer->moveCursor(QTextCursor::End);
}

SupportDialog::~SupportDialog()
{
    // A dialog closed with its window still holds the Core's log.
    if (m_holdingCoreLog && m_radioModel != nullptr) {
        m_radioModel->removeStationCoreLogViewer();
    }
}

void SupportDialog::buildUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // --- Diagnostic Logging Categories ---
    auto* catGroup = new QGroupBox(isRemote() ? QStringLiteral("The Core's Diagnostic Logging")
                                              : QStringLiteral("Diagnostic Logging"),
                                   this);
    auto* catGrid = new QGridLayout(catGroup);
    catGrid->setSpacing(6);

    const auto& mgr = LogManager::instance();
    const auto cats = mgr.categories();
    int col = 0;
    int row = 0;
    for (const auto& cat : cats) {
        auto* cb = new QCheckBox(cat.label, catGroup);
        cb->setToolTip(cat.description);
        cb->setChecked(cat.enabled);
        cb->setStyleSheet(QStringLiteral("QCheckBox { color: %1; }").arg(Style::kTextPrimary));

        connect(cb, &QCheckBox::toggled, this, [this, id = cat.id](bool on) {
            onCategoryToggled(id, on);
        });

        m_categoryChecks.insert(cat.id, cb);
        catGrid->addWidget(cb, row, col);

        ++col;
        if (col >= 3) {
            col = 0;
            ++row;
        }
    }

    // Enable All / Disable All buttons
    auto* catBtnLayout = new QHBoxLayout();
    catBtnLayout->addStretch();

    m_enableAllBtn = new QPushButton(QStringLiteral("Enable All"), catGroup);
    m_enableAllBtn->setAutoDefault(false);
    connect(m_enableAllBtn, &QPushButton::clicked, this, &SupportDialog::onEnableAll);
    catBtnLayout->addWidget(m_enableAllBtn);

    m_disableAllBtn = new QPushButton(QStringLiteral("Disable All"), catGroup);
    m_disableAllBtn->setAutoDefault(false);
    connect(m_disableAllBtn, &QPushButton::clicked, this, &SupportDialog::onDisableAll);
    catBtnLayout->addWidget(m_disableAllBtn);

    catBtnLayout->addStretch();
    catGrid->addLayout(catBtnLayout, row + 1, 0, 1, 3);
    // Remote window: why the Core's categories cannot change now.
    m_categoryReason = new QLabel(catGroup);
    m_categoryReason->setObjectName(QStringLiteral("supportCategoryReason"));
    m_categoryReason->setWordWrap(true);
    m_categoryReason->setVisible(false);
    catGrid->addWidget(m_categoryReason, row + 2, 0, 1, 3);
    mainLayout->addWidget(catGroup);

    // --- Log File Info ---
    auto* logInfoLayout = new QHBoxLayout();
    m_logPathLabel = new QLabel(this);
    m_logPathLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-family: monospace; font-size: 11px; }")
        .arg(Style::kTextScale));
    logInfoLayout->addWidget(m_logPathLabel, 1);

    m_logSizeLabel = new QLabel(this);
    m_logSizeLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 11px; }")
        .arg(Style::kTextScale));
    logInfoLayout->addWidget(m_logSizeLabel);
    mainLayout->addLayout(logInfoLayout);

    // --- Log Viewer ---
    m_logViewer = new QPlainTextEdit(this);
    m_logViewer->setReadOnly(true);
    m_logViewer->setMaximumBlockCount(kMaxLogViewLines);
    m_logViewer->setFont(QFont(QStringLiteral("Consolas"), 9));
    // §D: #0a0a14 = Style::kStatusBarBg, #203040 = Style::kBorderSubtle, #00b4d8 = Style::kAccent.
    // §D exception: fg #a0b0c0 (off-palette warm-blue for log text readability).
    m_logViewer->setStyleSheet(
        QStringLiteral(
            "QPlainTextEdit {"
            "  background: %1;"                  // Style::kStatusBarBg
            "  color: #a0b0c0;"                  // §D exception: log text warm-blue
            "  border: 1px solid %2;"            // Style::kBorderSubtle
            "  selection-background-color: %3;"  // Style::kAccent
            "}")
        .arg(Style::kStatusBarBg, Style::kBorderSubtle, Style::kAccent));
    mainLayout->addWidget(m_logViewer, 1);  // stretch factor 1

    // Remote window: the Core's recent log below this computer's.
    if (isRemote()) {
        m_coreLogLabel = new QLabel(QStringLiteral("The Core's recent log"), this);
        m_coreLogLabel->setStyleSheet(
            QStringLiteral("QLabel { color: %1; font-size: 11px; }").arg(Style::kTextScale));
        mainLayout->addWidget(m_coreLogLabel);
        m_coreLogViewer = new QPlainTextEdit(this);
        m_coreLogViewer->setObjectName(QStringLiteral("supportCoreLogViewer"));
        m_coreLogViewer->setReadOnly(true);
        m_coreLogViewer->setMaximumBlockCount(kMaxLogViewLines);
        m_coreLogViewer->setFont(QFont(QStringLiteral("Consolas"), 9));
        m_coreLogViewer->setStyleSheet(m_logViewer->styleSheet());
        mainLayout->addWidget(m_coreLogViewer, 1);
    }

    // --- Action Buttons ---
    auto* btnLayout = new QHBoxLayout();

    auto* refreshBtn = new QPushButton(QStringLiteral("Refresh"), this);
    refreshBtn->setAutoDefault(false);
    connect(refreshBtn, &QPushButton::clicked, this, &SupportDialog::onRefresh);
    btnLayout->addWidget(refreshBtn);

    auto* clearBtn = new QPushButton(QStringLiteral("Clear Log"), this);
    clearBtn->setAutoDefault(false);
    connect(clearBtn, &QPushButton::clicked, this, &SupportDialog::onClearLog);
    btnLayout->addWidget(clearBtn);

    auto* openFolderBtn = new QPushButton(QStringLiteral("Open Log Folder"), this);
    openFolderBtn->setAutoDefault(false);
    connect(openFolderBtn, &QPushButton::clicked, this, &SupportDialog::onOpenLogFolder);
    btnLayout->addWidget(openFolderBtn);

    btnLayout->addStretch();

    auto* bundleBtn = new QPushButton(QStringLiteral("Create Support Bundle"), this);
    m_bundleBtn = bundleBtn;
    bundleBtn->setObjectName(QStringLiteral("supportCreateBundle"));
    bundleBtn->setAutoDefault(false);
    // §D: bg = Style::kAccent. Hover #0096b7 = accent-dark; no canonical match.
    bundleBtn->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            "  background: %1;"          // Style::kAccent
            "  color: #ffffff;"
            "  border: none;"
            "  border-radius: 4px;"
            "  padding: 6px 14px;"
            "  font-weight: bold;"
            "}"
            "QPushButton:hover { background: #0096b7; }")  // §D exception: accent-dark hover
        .arg(Style::kAccent));
    connect(bundleBtn, &QPushButton::clicked, this, &SupportDialog::onCreateBundle);
    btnLayout->addWidget(bundleBtn);

    mainLayout->addLayout(btnLayout);

    // --- Status ---
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 11px; }")
        .arg(Style::kTextSecondary));
    mainLayout->addWidget(m_statusLabel);

    // Dialog theme. §D: #0f0f1a = kAppBg, #8090a0 = kTextSecondary,
    // #203040 = kBorderSubtle, #304050 = kOverlayBorder, #c8d8e8 = kTextPrimary.
    // §D exception: #405060 — button border/hover; off-palette (no canonical match).
    setStyleSheet(
        QStringLiteral(
            "QDialog { background: %1; }"
            "QGroupBox {"
            "  color: %2;"
            "  border: 1px solid %3;"
            "  border-radius: 4px;"
            "  margin-top: 8px;"
            "  padding-top: 14px;"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin;"
            "  left: 10px;"
            "  padding: 0 3px;"
            "}"
            "QPushButton {"
            "  background: %4;"
            "  color: %5;"
            "  border: 1px solid #405060;"    // §D exception: off-palette border
            "  border-radius: 3px;"
            "  padding: 5px 12px;"
            "}"
            "QPushButton:hover { background: #405060; }")  // §D exception
        .arg(Style::kAppBg, Style::kTextSecondary, Style::kBorderSubtle,
             Style::kOverlayBorder, Style::kTextPrimary));
}

QString SupportDialog::logTailText()
{
    QString path = LogManager::instance().logFilePath();
    if (path.isEmpty()) {
        return QStringLiteral("No log file found.");
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QStringLiteral("Could not open log file.");
    }

    // Read tail if file is large
    qint64 size = f.size();
    if (size > kMaxLogViewBytes) {
        f.seek(size - kMaxLogViewBytes);
        f.readLine();  // Skip partial first line
    }

    return QString::fromUtf8(f.readAll());
}

void SupportDialog::refreshLogViewer()
{
    updateLogInfo();

    m_logViewer->setPlainText(logTailText());

    // Scroll to bottom
    auto cursor = m_logViewer->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logViewer->setTextCursor(cursor);
}

void SupportDialog::updateLogInfo()
{
    const auto& mgr = LogManager::instance();
    QString path = mgr.logFilePath();

    if (path.isEmpty()) {
        m_logPathLabel->setText(QStringLiteral("Log: (none)"));
        m_logSizeLabel->setText(QString());
    } else {
        // Show just the filename, not full path
        m_logPathLabel->setText(
            (isRemote() ? QStringLiteral("This computer's log: %1") : QStringLiteral("Log: %1"))
                .arg(QFileInfo(path).fileName()));

        qint64 size = mgr.logFileSize();
        if (size < 1024) {
            m_logSizeLabel->setText(QStringLiteral("%1 B").arg(size));
        } else if (size < 1024 * 1024) {
            m_logSizeLabel->setText(QStringLiteral("%1 KB").arg(size / 1024));
        } else {
            m_logSizeLabel->setText(QStringLiteral("%1 MB").arg(size / (1024 * 1024)));
        }
    }
}

// --- Slots ---

void SupportDialog::onRefresh()
{
    refreshLogViewer();
    if (isRemote()) {
        m_radioModel->refreshStationCoreLog();
    }
    m_statusLabel->setText(QStringLiteral("Log refreshed."));
}

void SupportDialog::onClearLog()
{
    LogManager::instance().clearLog();
    refreshLogViewer();
    m_statusLabel->setText(QStringLiteral("Log cleared."));
}

void SupportDialog::onOpenLogFolder()
{
    QString dir = LogManager::instance().logDirPath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void SupportDialog::onEnableAll()
{
    if (isRemote()) {
        for (QCheckBox* box : std::as_const(m_categoryChecks)) {
            box->blockSignals(true);
            box->setChecked(true);
            box->blockSignals(false);
        }
        sendCoreCategories();
        m_statusLabel->setText(QStringLiteral("All of the Core's diagnostic logging enabled."));
        return;
    }
    LogManager::instance().setAllEnabled(true);
    for (auto it = m_categoryChecks.begin(); it != m_categoryChecks.end(); ++it) {
        it.value()->blockSignals(true);
        it.value()->setChecked(true);
        it.value()->blockSignals(false);
    }
    m_statusLabel->setText(QStringLiteral("All diagnostic logging enabled."));
}

void SupportDialog::onDisableAll()
{
    if (isRemote()) {
        for (QCheckBox* box : std::as_const(m_categoryChecks)) {
            box->blockSignals(true);
            box->setChecked(false);
            box->blockSignals(false);
        }
        sendCoreCategories();
        m_statusLabel->setText(QStringLiteral("All of the Core's diagnostic logging disabled."));
        return;
    }
    LogManager::instance().setAllEnabled(false);
    for (auto it = m_categoryChecks.begin(); it != m_categoryChecks.end(); ++it) {
        it.value()->blockSignals(true);
        it.value()->setChecked(false);
        it.value()->blockSignals(false);
    }
    m_statusLabel->setText(QStringLiteral("All diagnostic logging disabled."));
}

void SupportDialog::onCreateBundle()
{
    if (m_waitingForCore || !m_bundleBtn->isEnabled()) {
        return;
    }
    m_bundleBtn->setEnabled(false);
    if (!isRemote()) {
        m_statusLabel->setText(QStringLiteral("Creating support bundle..."));
        writeBundle(false, {}, {});
        return;
    }
    // A remote window: the Core's bundle first, then this computer's with
    // it under core/.
    IStationLink* link = m_radioModel->stationLink();
    const IStationLink::CommandOutcome outcome = link != nullptr
        ? link->requestSupportBundle()
        : IStationLink::CommandOutcome{false, QStringLiteral("This window was not connected to "
                                                             "the Core.")};
    if (!outcome.sent) {
        m_statusLabel->setText(QStringLiteral("Creating support bundle..."));
        writeBundle(true, {}, outcome.reason);
        return;
    }
    m_coreBundleCommand = outcome.commandId;
    m_waitingForCore = true;
    m_coreBundleTimeout.start();
    m_statusLabel->setText(QStringLiteral("Asking the Core for its support bundle..."));
}

void SupportDialog::writeBundle(bool withCore, const QByteArray& coreBundle,
                                const QString& coreReason)
{
    m_statusLabel->setText(QStringLiteral("Creating support bundle..."));
    SupportBundle::CoreAttachment core;
    core.wanted = withCore;
    core.bundle = coreBundle;
    core.reason = coreReason;
    SupportBundle::writeBundleAsync(this, SupportBundle::gatherInputs(m_radioModel), core,
                                    [this](const QString& path) { bundleWritten(path); });
}

void SupportDialog::bundleWritten(const QString& path)
{
    m_bundleBtn->setEnabled(true);
    if (path.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("Failed to create support bundle."));
        return;
    }

    m_statusLabel->setText(QStringLiteral("Bundle created: %1").arg(QFileInfo(path).fileName()));

    // Ask user what to do
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(QStringLiteral("Support Bundle Created"));
    msgBox.setText(QStringLiteral("Support bundle saved to:\n%1").arg(path));
    msgBox.setInformativeText(QStringLiteral(
        "Attach this file when filing a bug report at\n"
        "github.com/boydsoftprez/NereusSDR/issues"));
    // §D: #0f0f1a = kAppBg, #c8d8e8 = kTextPrimary, #304050 = kOverlayBorder.
    // §D exception: #405060 — button border; off-palette (matches dialog theme above).
    msgBox.setStyleSheet(
        QStringLiteral(
            "QMessageBox { background: %1; color: %2; }"
            "QLabel { color: %2; }"
            "QPushButton { background: %3; color: %2; border: 1px solid #405060;"  // §D exception
            "  border-radius: 3px; padding: 5px 12px; }")
        .arg(Style::kAppBg, Style::kTextPrimary, Style::kOverlayBorder));

    auto* openBtn = msgBox.addButton(QStringLiteral("Open Folder"), QMessageBox::ActionRole);
    auto* issueBtn = msgBox.addButton(QStringLiteral("File an Issue"), QMessageBox::ActionRole);
    msgBox.addButton(QMessageBox::Close);
    msgBox.exec();

    if (msgBox.clickedButton() == openBtn) {
        SupportBundle::openBundleFolder();
    } else if (msgBox.clickedButton() == issueBtn) {
        QDesktopServices::openUrl(
            QUrl(QStringLiteral("https://github.com/boydsoftprez/NereusSDR/issues/new")));
        SupportBundle::openBundleFolder();
    }
}

void SupportDialog::onCategoryToggled(const QString& id, bool on)
{
    if (isRemote()) {
        sendCoreCategories();
        m_statusLabel->setText(QStringLiteral("The Core's %1 logging %2.")
            .arg(id, on ? QStringLiteral("enabled") : QStringLiteral("disabled")));
        return;
    }
    LogManager::instance().setEnabled(id, on);
    m_statusLabel->setText(QStringLiteral("%1 logging %2.")
        .arg(id, on ? QStringLiteral("enabled") : QStringLiteral("disabled")));
}

} // namespace NereusSDR
