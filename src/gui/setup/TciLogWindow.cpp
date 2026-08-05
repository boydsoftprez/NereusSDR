// =================================================================
// src/gui/setup/TciLogWindow.cpp  (NereusSDR)
// =================================================================
//
// Phase 3J-1 closeout Item 2 (2026-05-12): see header for design notes.
// =================================================================

#include "TciLogWindow.h"

#include "core/AppSettings.h"
#include "gui/StyleConstants.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QVBoxLayout>

namespace NereusSDR {

namespace {
// Cap the QPlainTextEdit at this many lines so the window stays cheap even
// during a long WSJT-X session.  setMaximumBlockCount evicts oldest lines
// silently.  Chosen so a busy session (~10 lines/sec) gives ~16 minutes of
// rolling history before eviction.
constexpr int kMaxLogBlocks = 10000;

// Default window size in pixels.  Restored from AppSettings on every open
// after the first; this is the cold-start fallback.
constexpr int kDefaultWidth  = 720;
constexpr int kDefaultHeight = 480;
} // namespace

TciLogWindow::TciLogWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("TCI Server Log"));
    setObjectName(QStringLiteral("TciLogWindow"));
    // Non-modal: operator should be able to watch the log while interacting
    // with the rest of the app.  Modeless == default for QDialog when
    // shown via show() rather than exec(), but be explicit.
    setModal(false);
    // Avoid the WA_DeleteOnClose default for child dialogs -- MainWindow
    // owns this and reuses the same instance across open/close cycles.
    setAttribute(Qt::WA_DeleteOnClose, false);

    buildUI();
    restoreGeometryFromSettings();
}

TciLogWindow::~TciLogWindow() = default;

void TciLogWindow::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // ── Toolbar row ───────────────────────────────────────────────────────
    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(8);

    m_autoScrollCheck = new QCheckBox(tr("Auto-scroll"), this);
    m_autoScrollCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_autoScrollCheck->setToolTip(tr("Keep the view pinned to the newest log entry."));
    auto& s = AppSettings::instance();
    const bool autoScroll = s.value(QStringLiteral("TciLogWindowAutoScroll"),
                                    QStringLiteral("True")).toString()
                            == QStringLiteral("True");
    m_autoScrollCheck->setChecked(autoScroll);
    connect(m_autoScrollCheck, &QCheckBox::toggled,
            this, &TciLogWindow::onAutoScrollToggled);
    toolbar->addWidget(m_autoScrollCheck);

    m_pauseButton = new QPushButton(tr("Pause"), this);
    m_pauseButton->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    m_pauseButton->setCheckable(true);
    m_pauseButton->setToolTip(tr("Stop appending new entries to the log view. "
                                  "Already-buffered TCI traffic is still recorded "
                                  "in the server -- toggling Pause off resumes the "
                                  "display from current activity."));
    connect(m_pauseButton, &QPushButton::toggled,
            this, &TciLogWindow::onPauseToggled);
    toolbar->addWidget(m_pauseButton);

    m_clearButton = new QPushButton(tr("Clear"), this);
    m_clearButton->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    m_clearButton->setToolTip(tr("Empty the log view.  Does not affect the "
                                  "TCI server itself."));
    connect(m_clearButton, &QPushButton::clicked,
            this, &TciLogWindow::onClearClicked);
    toolbar->addWidget(m_clearButton);

    toolbar->addStretch();

    toolbar->addWidget(new QLabel(tr("Filter:"), this));
    m_filterCombo = new QComboBox(this);
    m_filterCombo->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
    m_filterCombo->addItem(tr("All"),      0);
    m_filterCombo->addItem(tr("In only"),  1);
    m_filterCombo->addItem(tr("Out only"), 2);
    m_filterCombo->setToolTip(tr("Show only inbound (client -> server) or "
                                  "outbound (server -> client) messages."));
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TciLogWindow::onFilterChanged);
    toolbar->addWidget(m_filterCombo);

    root->addLayout(toolbar);

    // ── Log view ──────────────────────────────────────────────────────────
    m_logView = new QPlainTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(kMaxLogBlocks);
    // Monospaced font so the timestamp + direction columns line up.
    QFont mono(QStringLiteral("Menlo"));
    mono.setStyleHint(QFont::TypeWriter);
    mono.setPointSize(10);
    m_logView->setFont(mono);
    // Dark-theme styling to match the rest of NereusSDR's setup dialogs.
    m_logView->setStyleSheet(QStringLiteral(
        "QPlainTextEdit {"
        "  background-color: #1a1a1a;"
        "  color: #d0d0d0;"
        "  border: 1px solid #3a3a3a;"
        "  selection-background-color: #4a6f8f;"
        "}"));
    root->addWidget(m_logView, /*stretch=*/1);
}

void TciLogWindow::appendEntry(const QString& direction,
                                const QString& peer,
                                const QString& text,
                                qint64 epochMs)
{
    if (m_paused) {
        return;
    }
    if (m_filter == 1 && direction != QStringLiteral("in"))  { return; }
    if (m_filter == 2 && direction != QStringLiteral("out")) { return; }

    // Format: "HH:mm:ss.zzz  in   host:port  text"
    // The peer column is left-aligned to 21 chars (IPv4 max 21: 15+colon+5).
    // Longer IPv6 peers will spill but stay readable.
    const QDateTime when = QDateTime::fromMSecsSinceEpoch(epochMs);
    const QString ts = when.toString(QStringLiteral("HH:mm:ss.zzz"));
    const QString dirCol = direction.leftJustified(3, QLatin1Char(' '));
    const QString peerCol = peer.leftJustified(21, QLatin1Char(' '));
    const QString line = QStringLiteral("%1  %2  %3  %4")
        .arg(ts, dirCol, peerCol, text);

    m_logView->appendPlainText(line);

    if (m_autoScrollCheck && m_autoScrollCheck->isChecked()) {
        auto* sb = m_logView->verticalScrollBar();
        if (sb) {
            sb->setValue(sb->maximum());
        }
    }
}

void TciLogWindow::onClearClicked()
{
    if (m_logView) {
        m_logView->clear();
    }
}

void TciLogWindow::onPauseToggled(bool paused)
{
    m_paused = paused;
    if (m_pauseButton) {
        m_pauseButton->setText(paused ? tr("Resume") : tr("Pause"));
    }
}

void TciLogWindow::onFilterChanged(int index)
{
    if (!m_filterCombo) { return; }
    m_filter = m_filterCombo->itemData(index).toInt();
}

void TciLogWindow::onAutoScrollToggled(bool on)
{
    AppSettings::instance().setValue(
        QStringLiteral("TciLogWindowAutoScroll"),
        on ? QStringLiteral("True") : QStringLiteral("False"));
}

void TciLogWindow::closeEvent(QCloseEvent* event)
{
    saveGeometryToSettings();
    QDialog::closeEvent(event);
}

void TciLogWindow::restoreGeometryFromSettings()
{
    auto& s = AppSettings::instance();
    // Settings store the geometry as a base64-encoded latin1 string (XML-
    // safe).  Decode back to raw bytes before handing to restoreGeometry().
    const QString b64 = s.value(
        QStringLiteral("TciLogWindowGeometry"), QString{}).toString();
    if (!b64.isEmpty()) {
        const QByteArray geom = QByteArray::fromBase64(b64.toLatin1());
        if (!geom.isEmpty() && restoreGeometry(geom)) {
            return;
        }
    }
    resize(kDefaultWidth, kDefaultHeight);
}

void TciLogWindow::saveGeometryToSettings()
{
    AppSettings::instance().setValue(
        QStringLiteral("TciLogWindowGeometry"),
        QString::fromLatin1(saveGeometry().toBase64()));
}

} // namespace NereusSDR
