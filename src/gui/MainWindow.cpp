#include "MainWindow.h"
#include "ConnectionPanel.h"
#include "SupportDialog.h"
#include "models/RadioModel.h"
#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "core/LogCategories.h"

#include <QApplication>
#include <QCloseEvent>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QVBoxLayout>

#include <cstdlib>

namespace NereusSDR {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_radioModel(new RadioModel(this))
{
    buildUI();
    buildMenuBar();
    buildStatusBar();
    applyDarkTheme();

    // Wire connection state changes to status bar
    connect(m_radioModel, &RadioModel::connectionStateChanged,
            this, &MainWindow::onConnectionStateChanged);

    // Start discovery in background so radios are found before the user opens the panel
    m_radioModel->discovery()->startDiscovery();

    // Auto-reconnect to last radio if it appears
    tryAutoReconnect();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUI()
{
    setWindowTitle(QStringLiteral("NereusSDR %1").arg(NEREUSSDR_VERSION));
    setMinimumSize(800, 600);
    resize(1280, 800);

    // Central widget with placeholder
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* placeholder = new QLabel(QStringLiteral("NereusSDR"), central);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(QStringLiteral(
        "font-size: 24px; color: #00b4d8; font-weight: bold;"));
    layout->addWidget(placeholder);

    setCentralWidget(central);
}

void MainWindow::buildMenuBar()
{
    // --- File ---
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    fileMenu->addAction(QStringLiteral("&Quit"), QKeySequence::Quit,
                        qApp, &QApplication::quit);

    // --- Radio ---
    auto* radioMenu = menuBar()->addMenu(QStringLiteral("&Radio"));

    radioMenu->addAction(QStringLiteral("&Connect..."), QKeySequence(Qt::CTRL | Qt::Key_K),
                         this, &MainWindow::showConnectionPanel);

    radioMenu->addAction(QStringLiteral("&Disconnect"), this, [this]() {
        m_radioModel->disconnectFromRadio();
    });

    radioMenu->addSeparator();

    radioMenu->addAction(QStringLiteral("&Protocol Info"), this, [this]() {
        if (m_radioModel->isConnected()) {
            RadioInfo info = m_radioModel->connection()->radioInfo();
            QString msg = QStringLiteral("Radio: %1\nProtocol: P%2\nFirmware: %3\nMAC: %4\nIP: %5")
                .arg(info.displayName())
                .arg(static_cast<int>(info.protocol))
                .arg(info.firmwareVersion)
                .arg(info.macAddress, info.address.toString());
            qCDebug(lcConnection) << msg;
        }
    });

    // --- Help ---
    auto* helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));
    helpMenu->addAction(QStringLiteral("&Support..."), this,
                        &MainWindow::showSupportDialog);
    helpMenu->addSeparator();
    helpMenu->addAction(QStringLiteral("&About NereusSDR"), this, [this]() {
        Q_UNUSED(this);
    });
}

void MainWindow::buildStatusBar()
{
    // Connection status indicator (left side)
    m_connStatusLabel = new QLabel(QStringLiteral(" Disconnected "), this);
    m_connStatusLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  color: #8090a0;"
        "  background: #1a2a3a;"
        "  border: 1px solid #203040;"
        "  border-radius: 3px;"
        "  padding: 2px 8px;"
        "  font-size: 12px;"
        "}"));
    statusBar()->addWidget(m_connStatusLabel);

    // Radio info (center)
    m_radioInfoLabel = new QLabel(this);
    m_radioInfoLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-size: 12px; }"));
    statusBar()->addWidget(m_radioInfoLabel, 1);  // stretch factor 1

    // Click status label to open connection panel
    m_connStatusLabel->setCursor(Qt::PointingHandCursor);
    m_connStatusLabel->installEventFilter(this);
}

void MainWindow::applyDarkTheme()
{
    setStyleSheet(QStringLiteral(
        "QMainWindow { background: #0f0f1a; }"
        "QMenuBar {"
        "  background: #1a2a3a;"
        "  color: #c8d8e8;"
        "  border-bottom: 1px solid #203040;"
        "}"
        "QMenuBar::item:selected { background: #00b4d8; }"
        "QMenu {"
        "  background: #1a2a3a;"
        "  color: #c8d8e8;"
        "  border: 1px solid #203040;"
        "}"
        "QMenu::item:selected { background: #00b4d8; }"
        "QLabel { color: #c8d8e8; }"
        "QStatusBar {"
        "  background: #1a2a3a;"
        "  color: #8090a0;"
        "  border-top: 1px solid #203040;"
        "}"));
}

void MainWindow::showConnectionPanel()
{
    if (!m_connectionPanel) {
        m_connectionPanel = new ConnectionPanel(m_radioModel, this);
        m_connectionPanel->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_connectionPanel, &QObject::destroyed, this, [this]() {
            m_connectionPanel = nullptr;
        });
    }
    m_connectionPanel->show();
    m_connectionPanel->raise();
    m_connectionPanel->activateWindow();
}

void MainWindow::showSupportDialog()
{
    if (!m_supportDialog) {
        m_supportDialog = new SupportDialog(m_radioModel, this);
        m_supportDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_supportDialog, &QObject::destroyed, this, [this]() {
            m_supportDialog = nullptr;
        });
    }
    m_supportDialog->show();
    m_supportDialog->raise();
    m_supportDialog->activateWindow();
}

void MainWindow::onConnectionStateChanged()
{
    if (m_radioModel->isConnected()) {
        m_connStatusLabel->setText(QStringLiteral(" Connected "));
        m_connStatusLabel->setStyleSheet(QStringLiteral(
            "QLabel {"
            "  color: #ffffff;"
            "  background: #007a3d;"
            "  border: 1px solid #00a050;"
            "  border-radius: 3px;"
            "  padding: 2px 8px;"
            "  font-size: 12px;"
            "}"));
        m_radioInfoLabel->setText(QStringLiteral("%1  —  FW %2")
            .arg(m_radioModel->name(), m_radioModel->version()));
        m_radioInfoLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #c8d8e8; font-size: 12px; }"));
    } else {
        m_connStatusLabel->setText(QStringLiteral(" Disconnected "));
        m_connStatusLabel->setStyleSheet(QStringLiteral(
            "QLabel {"
            "  color: #8090a0;"
            "  background: #1a2a3a;"
            "  border: 1px solid #203040;"
            "  border-radius: 3px;"
            "  padding: 2px 8px;"
            "  font-size: 12px;"
            "}"));
        m_radioInfoLabel->setText(QString());
        m_radioInfoLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #607080; font-size: 12px; }"));
    }
}

void MainWindow::tryAutoReconnect()
{
    QString lastMac = AppSettings::instance()
        .value(QStringLiteral("LastConnectedRadioMac")).toString();
    if (lastMac.isEmpty()) {
        return;
    }

    // When a radio matching the last MAC is discovered, auto-connect
    connect(m_radioModel->discovery(), &RadioDiscovery::radioDiscovered,
            this, [this, lastMac](const RadioInfo& info) {
        if (info.macAddress == lastMac && !m_radioModel->isConnected() && !info.inUse) {
            qCDebug(lcConnection) << "Auto-reconnecting to" << info.displayName();
            m_radioModel->connectToRadio(info);
        }
    });
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Stop discovery to prevent new signals during shutdown
    m_radioModel->discovery()->stopDiscovery();

    // Tear down connection (sends stop command, closes sockets, joins thread)
    m_radioModel->disconnectFromRadio();

    AppSettings::instance().save();
    event->accept();

    // Force process exit. Worker threads and active QObjects can prevent
    // the Qt event loop from returning. Settings are saved, sockets closed,
    // and stop command sent to the radio — safe to exit now.
    std::exit(0);
}

} // namespace NereusSDR
