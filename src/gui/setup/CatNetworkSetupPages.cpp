#include "CatNetworkSetupPages.h"
#include "gui/StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// CatSerialPortsPage
// ---------------------------------------------------------------------------

CatSerialPortsPage::CatSerialPortsPage(QWidget* parent)
    : SetupPage(QStringLiteral("Serial Ports"), parent)
{
    buildUI();
}

void CatSerialPortsPage::buildUI()
{
    setStyleSheet(QString::fromLatin1(Style::kPageStyle));

    static const char* kPortLabels[4] = {
        "CAT Port 1", "CAT Port 2", "CAT Port 3", "CAT Port 4"
    };

    static const char* kBaudRates[] = {
        "9600", "19200", "38400", "57600", "115200", nullptr
    };

    for (int i = 0; i < 4; ++i) {
        auto* group = new QGroupBox(QString::fromLatin1(kPortLabels[i]), this);
        group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));

        auto* grid = new QGridLayout(group);
        grid->setSpacing(6);

        // Column 0: Port label + combo
        auto* portLabel = new QLabel(QStringLiteral("Port:"), group);
        portLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
        grid->addWidget(portLabel, 0, 0);

        m_ports[i].portCombo = new QComboBox(group);
        m_ports[i].portCombo->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
        m_ports[i].portCombo->addItem(QStringLiteral("(none)"));
        m_ports[i].portCombo->setDisabled(true);
        m_ports[i].portCombo->setToolTip(QStringLiteral("NYI — serial port selection"));
        grid->addWidget(m_ports[i].portCombo, 0, 1);

        // Column 2: Baud label + combo
        auto* baudLabel = new QLabel(QStringLiteral("Baud:"), group);
        baudLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
        grid->addWidget(baudLabel, 0, 2);

        m_ports[i].baudCombo = new QComboBox(group);
        m_ports[i].baudCombo->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
        for (int b = 0; kBaudRates[b] != nullptr; ++b) {
            m_ports[i].baudCombo->addItem(QString::fromLatin1(kBaudRates[b]));
        }
        m_ports[i].baudCombo->setDisabled(true);
        m_ports[i].baudCombo->setToolTip(QStringLiteral("NYI — baud rate selection"));
        grid->addWidget(m_ports[i].baudCombo, 0, 3);

        // Row 1: enable + status
        m_ports[i].enableCheck = new QCheckBox(QStringLiteral("Enable"), group);
        m_ports[i].enableCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
        m_ports[i].enableCheck->setDisabled(true);
        m_ports[i].enableCheck->setToolTip(QStringLiteral("NYI — enable CAT port"));
        grid->addWidget(m_ports[i].enableCheck, 1, 0, 1, 2);

        m_ports[i].statusLabel = new QLabel(QStringLiteral("Status: not connected"), group);
        m_ports[i].statusLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
        grid->addWidget(m_ports[i].statusLabel, 1, 2, 1, 2);

        contentLayout()->addWidget(group);
    }

    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// CatTciServerPage
// ---------------------------------------------------------------------------

CatTciServerPage::CatTciServerPage(QWidget* parent)
    : SetupPage(QStringLiteral("TCI Server"), parent)
{
    buildUI();
}

void CatTciServerPage::buildUI()
{
    setStyleSheet(QString::fromLatin1(Style::kPageStyle));

    auto* group = new QGroupBox(QStringLiteral("TCI"), this);
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));

    auto* grid = new QGridLayout(group);
    grid->setSpacing(6);

    // Enable toggle
    m_enableCheck = new QCheckBox(QStringLiteral("Enable TCI Server"), group);
    m_enableCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_enableCheck->setDisabled(true);
    m_enableCheck->setToolTip(QStringLiteral("NYI — TCI server enable"));
    grid->addWidget(m_enableCheck, 0, 0, 1, 2);

    // Bind IP
    auto* ipLabel = new QLabel(QStringLiteral("Bind IP:"), group);
    ipLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    grid->addWidget(ipLabel, 1, 0);

    m_bindIpEdit = new QLineEdit(QStringLiteral("0.0.0.0"), group);
    m_bindIpEdit->setStyleSheet(QString::fromLatin1(Style::kLineEditStyle));
    m_bindIpEdit->setDisabled(true);
    m_bindIpEdit->setToolTip(QStringLiteral("NYI — bind IP address"));
    grid->addWidget(m_bindIpEdit, 1, 1);

    // Port
    auto* portLabel = new QLabel(QStringLiteral("Port:"), group);
    portLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    grid->addWidget(portLabel, 2, 0);

    m_portSpin = new QSpinBox(group);
    m_portSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
    m_portSpin->setRange(1024, 65535);
    m_portSpin->setValue(40001);
    m_portSpin->setDisabled(true);
    m_portSpin->setToolTip(QStringLiteral("NYI — TCI server port (default 40001)"));
    grid->addWidget(m_portSpin, 2, 1);

    // Status
    m_statusLabel = new QLabel(QStringLiteral("Status: stopped"), group);
    m_statusLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    grid->addWidget(m_statusLabel, 3, 0, 1, 2);

    // Connected clients
    m_clientsLabel = new QLabel(QStringLiteral("Connected clients: 0"), group);
    m_clientsLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    grid->addWidget(m_clientsLabel, 4, 0, 1, 2);

    contentLayout()->addWidget(group);
    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// CatTcpIpPage
// ---------------------------------------------------------------------------

CatTcpIpPage::CatTcpIpPage(QWidget* parent)
    : SetupPage(QStringLiteral("TCP/IP CAT"), parent)
{
    buildUI();
}

void CatTcpIpPage::buildUI()
{
    setStyleSheet(QString::fromLatin1(Style::kPageStyle));

    auto* group = new QGroupBox(QStringLiteral("TCP CAT"), this);
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));

    auto* grid = new QGridLayout(group);
    grid->setSpacing(6);

    // Enable toggle
    m_enableCheck = new QCheckBox(QStringLiteral("Enable TCP/IP CAT Server"), group);
    m_enableCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_enableCheck->setDisabled(true);
    m_enableCheck->setToolTip(QStringLiteral("NYI — TCP CAT server enable"));
    grid->addWidget(m_enableCheck, 0, 0, 1, 2);

    // Bind IP
    auto* ipLabel = new QLabel(QStringLiteral("Bind IP:"), group);
    ipLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    grid->addWidget(ipLabel, 1, 0);

    m_bindIpEdit = new QLineEdit(QStringLiteral("0.0.0.0"), group);
    m_bindIpEdit->setStyleSheet(QString::fromLatin1(Style::kLineEditStyle));
    m_bindIpEdit->setDisabled(true);
    m_bindIpEdit->setToolTip(QStringLiteral("NYI — bind IP address"));
    grid->addWidget(m_bindIpEdit, 1, 1);

    // Port
    auto* portLabel = new QLabel(QStringLiteral("Port:"), group);
    portLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    grid->addWidget(portLabel, 2, 0);

    m_portSpin = new QSpinBox(group);
    m_portSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
    m_portSpin->setRange(1024, 65535);
    m_portSpin->setValue(4532);
    m_portSpin->setDisabled(true);
    m_portSpin->setToolTip(QStringLiteral("NYI — TCP CAT port (default 4532 / rigctld)"));
    grid->addWidget(m_portSpin, 2, 1);

    // Status
    m_statusLabel = new QLabel(QStringLiteral("Status: stopped"), group);
    m_statusLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    grid->addWidget(m_statusLabel, 3, 0, 1, 2);

    contentLayout()->addWidget(group);
    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// CatMidiControlPage
// ---------------------------------------------------------------------------

CatMidiControlPage::CatMidiControlPage(QWidget* parent)
    : SetupPage(QStringLiteral("MIDI Control"), parent)
{
    buildUI();
}

void CatMidiControlPage::buildUI()
{
    setStyleSheet(QString::fromLatin1(Style::kPageStyle));

    auto* group = new QGroupBox(QStringLiteral("MIDI"), this);
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));

    auto* grid = new QGridLayout(group);
    grid->setSpacing(6);

    // Enable toggle
    m_enableCheck = new QCheckBox(QStringLiteral("Enable MIDI Control"), group);
    m_enableCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_enableCheck->setDisabled(true);
    m_enableCheck->setToolTip(QStringLiteral("NYI — MIDI control enable"));
    grid->addWidget(m_enableCheck, 0, 0, 1, 2);

    // Device combo
    auto* devLabel = new QLabel(QStringLiteral("Device:"), group);
    devLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    grid->addWidget(devLabel, 1, 0);

    m_deviceCombo = new QComboBox(group);
    m_deviceCombo->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
    m_deviceCombo->addItem(QStringLiteral("(no MIDI devices found)"));
    m_deviceCombo->setDisabled(true);
    m_deviceCombo->setToolTip(QStringLiteral("NYI — MIDI device selection"));
    grid->addWidget(m_deviceCombo, 1, 1);

    // Mapping table placeholder label
    m_mappingLabel = new QLabel(
        QStringLiteral("MIDI mapping table will appear here"), group);
    m_mappingLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    m_mappingLabel->setAlignment(Qt::AlignCenter);
    m_mappingLabel->setMinimumHeight(80);
    grid->addWidget(m_mappingLabel, 2, 0, 1, 2);

    // Learn button
    m_learnButton = new QPushButton(QStringLiteral("Learn..."), group);
    m_learnButton->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    m_learnButton->setDisabled(true);
    m_learnButton->setToolTip(QStringLiteral("NYI — MIDI learn mode"));
    grid->addWidget(m_learnButton, 3, 0, 1, 2);

    contentLayout()->addWidget(group);
    contentLayout()->addStretch();
}

} // namespace NereusSDR
