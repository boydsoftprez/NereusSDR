// no-port-check: NereusSDR-original UI file. The "// From Thetis" inline
// comments below are design cross-references documenting where AppSettings
// key names and default values were verified against the Thetis control
// inventory (setup.designer.cs / TCIServer.cs). No Thetis code is
// translated here; all AppSettings keys are attributed in TciProtocol.h.

#include "CatNetworkSetupPages.h"
#include "gui/StyleConstants.h"
#include "core/AppSettings.h"

#include <QNetworkInterface>
#ifdef HAVE_WEBSOCKETS
#include "core/TciServer.h"
#endif

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>

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
// Phase 20 (Phase 3J-1): 6-group-box TCI Server setup page.
// AppSettings keys documented in src/core/TciProtocol.h header block.
// ---------------------------------------------------------------------------

CatTciServerPage::CatTciServerPage(QWidget* parent)
    : SetupPage(QStringLiteral("TCI Server"), parent)
{
    buildUI();
}

void CatTciServerPage::buildUI()
{
    NereusSDR::Style::applyDarkPageStyle(this);

    buildServerGroup();
    buildCompatibilityGroup();
    buildIqStreamGroup();
    buildAudioStreamGroup();
    buildSensorsGroup();
    buildVfoQuirksGroup();

    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// Group 1: Server
// Controls: Enable / Bind IP (read-only 127.0.0.1) / Port + Default button /
//           Send initial state / Rate limit / Show Log button / Status line.
// AppSettings: TciServerEnabled, TciServerPort, TciSendInitialFrequencyStateOnConnect,
//              TciRateLimitMsgsPerSec.
// ---------------------------------------------------------------------------
void CatTciServerPage::buildServerGroup()
{
    auto* group = new QGroupBox(tr("Server"), this);
    m_serverGroup = group;  // saved so refreshTciStatusDisplay() can update title
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
    auto* form = new QFormLayout(group);
    form->setSpacing(6);

    auto& s = AppSettings::instance();

    // Enable checkbox
    // From Thetis setup.designer.cs:57979-57983 [v2.10.3.13] — chkTCIEnable
    m_enableCheck = new QCheckBox(tr("Enable TCI Server"), group);
    m_enableCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_enableCheck->setToolTip(tr("Enable the built-in TCI (Transceiver Control Interface) WebSocket server."));
    m_enableCheck->setChecked(
        s.value(QStringLiteral("TciServerEnabled"), QStringLiteral("False")).toString()
        == QStringLiteral("True"));
    connect(m_enableCheck, &QCheckBox::toggled, this, [this](bool on) {
        AppSettings::instance().setValue(QStringLiteral("TciServerEnabled"),
                                          on ? QStringLiteral("True") : QStringLiteral("False"));
        // Phase 3J-1 review P2.4: emit signal so MainWindow can live-wire
        // start/stop without requiring a disconnect/reconnect cycle.  The port
        // comes from the current spinbox value (already persisted in AppSettings
        // by the spinbox valueChanged handler above).
        const quint16 port = static_cast<quint16>(m_portSpin->value());
        emit tciServerEnableToggled(on, port);
    });
    form->addRow(QString(), m_enableCheck);

    // ── Bind address dropdown ───────────────────────────────────────────────
    //
    // Phase 3J-1 closeout Item 1 (2026-05-12): replaces the read-only
    // "127.0.0.1" label with an interface-aware dropdown.  Operator can
    // pick:
    //   - "Loopback only (127.0.0.1)" — default; safest
    //   - "Any IPv4 interface (0.0.0.0)" — exposes server to LAN
    //   - A specific detected NIC (e.g. "en0 — 192.168.1.50")
    //   - IPv6 equivalents
    //
    // Functional parity with Thetis Setup.cs:22410-22473 [v2.10.3.13]
    // `txtTCIServerBindIPPort` (which uses a free-text "IP:port" field
    // accepting any valid IPv4/IPv6 via `IPAddress.TryParse`).  UX
    // diverges per CLAUDE.md feedback_source_first_ui_vs_dsp.md — Qt
    // widgets are NereusSDR-native; a dropdown with validated, NIC-aware
    // choices is the better UX for our platform.
    m_bindAddressCombo = new QComboBox(group);
    m_bindAddressCombo->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
    m_bindAddressCombo->setToolTip(tr(
        "Network interface the TCI server binds to. "
        "Loopback (127.0.0.1) accepts connections only from this machine. "
        "Any interface (0.0.0.0) accepts from anywhere on your LAN. "
        "TCI has no authentication — choose a specific LAN IP or 0.0.0.0 "
        "only if your network is trusted."));
    populateBindAddressCombo();
    connect(m_bindAddressCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        if (idx < 0) { return; }
        const QString addr = m_bindAddressCombo->itemData(idx).toString();
        AppSettings::instance().setValue(QStringLiteral("TciServerBindAddress"), addr);
        // Emit the combined bind+port change signal so MainWindow can live-
        // restart the server if it's running.
        emit tciServerBindOrPortChanged(addr,
            static_cast<quint16>(m_portSpin ? m_portSpin->value() : 50001));
    });
    form->addRow(tr("Bind interface:"), m_bindAddressCombo);

    // Port spinbox + Default button
    // From Thetis setup.designer.cs:57991-57998 [v2.10.3.13] — udTCIPort (default 50001)
    m_portSpin = new QSpinBox(group);
    m_portSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
    m_portSpin->setRange(1024, 65535);
    m_portSpin->setToolTip(tr("TCP port the TCI WebSocket server listens on (1024–65535). "
                               "Default is 50001. Requires server restart to take effect."));
    m_portSpin->setValue(
        s.value(QStringLiteral("TciServerPort"), 50001).toInt());
    connect(m_portSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        AppSettings::instance().setValue(QStringLiteral("TciServerPort"), v);
        // Phase 3J-1 closeout Item 1: emit combined bind+port so the
        // running server picks up a port change without a manual toggle.
        const QString addr = AppSettings::instance().value(
            QStringLiteral("TciServerBindAddress"), QStringLiteral("127.0.0.1")).toString();
        emit tciServerBindOrPortChanged(addr, static_cast<quint16>(v));
    });

    m_portDefaultBtn = new QPushButton(tr("Default"), group);
    m_portDefaultBtn->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    m_portDefaultBtn->setToolTip(tr("Reset port to 50001."));
    connect(m_portDefaultBtn, &QPushButton::clicked, this, [this] {
        m_portSpin->setValue(50001);
    });

    auto* portRow = new QHBoxLayout;
    portRow->addWidget(m_portSpin);
    portRow->addWidget(m_portDefaultBtn);
    portRow->addStretch();
    form->addRow(tr("Port:"), portRow);

    // Send initial state on connect
    // From Thetis TCIServer.cs [v2.10.3.13] — initial-state burst on handshake
    m_sendInitialStateCheck = new QCheckBox(tr("Send initial state on connect"), group);
    m_sendInitialStateCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_sendInitialStateCheck->setToolTip(tr("When a TCI client connects, immediately send the current VFO, "
                                            "mode, filter, and transceiver state as a burst of TCI commands."));
    m_sendInitialStateCheck->setChecked(
        s.value(QStringLiteral("TciSendInitialFrequencyStateOnConnect"), QStringLiteral("True")).toString()
        == QStringLiteral("True"));
    connect(m_sendInitialStateCheck, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("TciSendInitialFrequencyStateOnConnect"),
                                          on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    form->addRow(QString(), m_sendInitialStateCheck);

    // Rate limit
    // From Thetis TCIServer.cs [v2.10.3.13] — per-client outbound rate cap
    m_rateLimitSpin = new QSpinBox(group);
    m_rateLimitSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
    m_rateLimitSpin->setRange(0, 1000);
    m_rateLimitSpin->setSuffix(tr(" msg/s"));
    m_rateLimitSpin->setSpecialValueText(tr("Unlimited"));
    m_rateLimitSpin->setToolTip(tr("Maximum outbound TCI messages per second per client (0 = unlimited). "
                                    "Lower values reduce CPU load for slow TCI apps."));
    m_rateLimitSpin->setValue(
        s.value(QStringLiteral("TciRateLimitMsgsPerSec"), 60).toInt());
    connect(m_rateLimitSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [](int v) {
        AppSettings::instance().setValue(QStringLiteral("TciRateLimitMsgsPerSec"), v);
    });
    form->addRow(tr("Rate limit:"), m_rateLimitSpin);

    // Show Log button — Phase 3J-1 closeout Item 2 (2026-05-12) wires the
    // click through SetupDialog up to MainWindow, which owns the lazy-
    // constructed TciLogWindow.  Enabled only while the server is running
    // (refreshTciStatusDisplay toggles this from setTciServer's hookups).
    m_showLogBtn = new QPushButton(tr("Show Log..."), group);
    m_showLogBtn->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    m_showLogBtn->setToolTip(tr("Open the TCI server message log window. "
                                 "Available while the server is running."));
    m_showLogBtn->setEnabled(false);
    connect(m_showLogBtn, &QPushButton::clicked, this, [this] {
        emit showLogRequested();
    });
    form->addRow(QString(), m_showLogBtn);

    // Status line — read-only; updated by TciServer in Phase 21+
    m_statusLabel = new QLabel(tr("Stopped"), group);
    m_statusLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    m_statusLabel->setObjectName(QStringLiteral("tciStatusLabel"));
    form->addRow(tr("Status:"), m_statusLabel);

    contentLayout()->addWidget(group);
}

// ---------------------------------------------------------------------------
// Group 2: Compatibility
// Controls: Emulate ExpertSDR3 / Emulate SunSDR2 PRO / CWL+CWU→CW /
//           CW→CWU above 10 MHz.
// AppSettings: TciEmulateExpertSDR3Protocol, TciEmulateSunSDR2Pro,
//              TciCwluBecomesCw, TciCwBecomesCwuAbove10mhz.
// ---------------------------------------------------------------------------
void CatTciServerPage::buildCompatibilityGroup()
{
    auto* group = new QGroupBox(tr("Compatibility"), this);
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
    auto* form = new QFormLayout(group);
    form->setSpacing(6);

    auto& s = AppSettings::instance();

    // Emulate ExpertSDR3 protocol
    // From Thetis TCIServer.cs [v2.10.3.13] — ExpertSDR3 compat flag
    m_emulateExpertSdr3Check = new QCheckBox(tr("Emulate ExpertSDR3 protocol"), group);
    m_emulateExpertSdr3Check->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_emulateExpertSdr3Check->setToolTip(
        tr("Enables TCI protocol extensions that ExpertSDR3-compatible apps expect. "
           "Disable if connecting to standard TCI clients."));
    // Phase 3J-1 bench fix (2026-05-11): default True — must agree with
    // runtime default in TciProtocol::buildInitBurst.  WSJT-X / Hamlib gate
    // TCI-audio mode on the ExpertSDR3 protocol identifier; defaulting OFF
    // breaks WSJT-X TX audio out-of-box.
    m_emulateExpertSdr3Check->setChecked(
        s.value(QStringLiteral("TciEmulateExpertSDR3Protocol"), QStringLiteral("True")).toString()
        == QStringLiteral("True"));
    connect(m_emulateExpertSdr3Check, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("TciEmulateExpertSDR3Protocol"),
                                          on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    form->addRow(QString(), m_emulateExpertSdr3Check);

    // Emulate SunSDR2 PRO device
    // From Thetis TCIServer.cs [v2.10.3.13] — SunSDR2 PRO compat flag
    m_emulateSunSdr2Check = new QCheckBox(tr("Emulate SunSDR2 PRO device"), group);
    m_emulateSunSdr2Check->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_emulateSunSdr2Check->setToolTip(
        tr("Reports device identity as SunSDR2 PRO in TCI handshake. "
           "Required by some apps that check the device field."));
    // Phase 3J-1 bench fix (2026-05-11): default True — must agree with
    // runtime default in TciProtocol::buildInitBurst.  See ExpertSDR3
    // comment above for the full compat rationale.
    m_emulateSunSdr2Check->setChecked(
        s.value(QStringLiteral("TciEmulateSunSDR2Pro"), QStringLiteral("True")).toString()
        == QStringLiteral("True"));
    connect(m_emulateSunSdr2Check, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("TciEmulateSunSDR2Pro"),
                                          on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    form->addRow(QString(), m_emulateSunSdr2Check);

    // CWL/CWU becomes CW
    // From Thetis TCIServer.cs [v2.10.3.13] — CWL/CWU→CW mode map
    m_cwluBecomesCwCheck = new QCheckBox(tr("CWL/CWU becomes CW"), group);
    m_cwluBecomesCwCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_cwluBecomesCwCheck->setToolTip(
        tr("Report mode as \"CW\" instead of \"CWL\" or \"CWU\" to TCI clients that "
           "do not understand the L/U sideband distinction."));
    m_cwluBecomesCwCheck->setChecked(
        s.value(QStringLiteral("TciCwluBecomesCw"), QStringLiteral("False")).toString()
        == QStringLiteral("True"));
    connect(m_cwluBecomesCwCheck, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("TciCwluBecomesCw"),
                                          on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    form->addRow(QString(), m_cwluBecomesCwCheck);

    // CW becomes CWU above 10 MHz
    // From Thetis TCIServer.cs [v2.10.3.13] — W2PA #559 CWU-above-10MHz quirk
    m_cwBecomesCwuCheck = new QCheckBox(tr("CW becomes CWU above 10 MHz"), group);
    m_cwBecomesCwuCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_cwBecomesCwuCheck->setToolTip(
        tr("On bands above 10 MHz, report mode as \"CWU\" instead of \"CW\" or \"CWL\". "
           "Required by certain logging apps that follow the ARRL sideband convention."));
    m_cwBecomesCwuCheck->setChecked(
        s.value(QStringLiteral("TciCwBecomesCwuAbove10mhz"), QStringLiteral("False")).toString()
        == QStringLiteral("True"));
    connect(m_cwBecomesCwuCheck, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("TciCwBecomesCwuAbove10mhz"),
                                          on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    form->addRow(QString(), m_cwBecomesCwuCheck);

    contentLayout()->addWidget(group);
}

// ---------------------------------------------------------------------------
// Group 3: IQ Stream
// Controls: Swap I/Q / Always stream IQ.
// AppSettings: TciIqSwap, TciAlwaysStreamIq.
// ---------------------------------------------------------------------------
void CatTciServerPage::buildIqStreamGroup()
{
    auto* group = new QGroupBox(tr("IQ Stream"), this);
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
    auto* form = new QFormLayout(group);
    form->setSpacing(6);

    auto& s = AppSettings::instance();

    // Swap I/Q
    // From Thetis TCIServer.cs [v2.10.3.13] — IQ swap flag (default True)
    m_iqSwapCheck = new QCheckBox(tr("Swap I/Q channels"), group);
    m_iqSwapCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_iqSwapCheck->setToolTip(
        tr("Swap the I and Q samples in the TCI IQ data stream. "
           "Enabled by default for compatibility with most TCI IQ consumers."));
    m_iqSwapCheck->setChecked(
        s.value(QStringLiteral("TciIqSwap"), QStringLiteral("True")).toString()
        == QStringLiteral("True"));
    connect(m_iqSwapCheck, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("TciIqSwap"),
                                          on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    form->addRow(QString(), m_iqSwapCheck);

    // Always stream IQ
    // From Thetis TCIServer.cs [v2.10.3.13] — always-stream flag
    m_alwaysStreamIqCheck = new QCheckBox(tr("Always stream IQ"), group);
    m_alwaysStreamIqCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_alwaysStreamIqCheck->setToolTip(
        tr("Stream IQ data to all connected TCI clients continuously, even if no client "
           "has explicitly subscribed to the IQ stream. Increases CPU and network load."));
    m_alwaysStreamIqCheck->setChecked(
        s.value(QStringLiteral("TciAlwaysStreamIq"), QStringLiteral("False")).toString()
        == QStringLiteral("True"));
    connect(m_alwaysStreamIqCheck, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("TciAlwaysStreamIq"),
                                          on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    form->addRow(QString(), m_alwaysStreamIqCheck);

    contentLayout()->addWidget(group);
}

// ---------------------------------------------------------------------------
// Group 4: Audio Stream
// Controls: Block size spinbox / TX channel combo.
// AppSettings: TciAudioStreamSamples, TciTxChannel.
// ---------------------------------------------------------------------------
void CatTciServerPage::buildAudioStreamGroup()
{
    auto* group = new QGroupBox(tr("Audio Stream"), this);
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
    auto* form = new QFormLayout(group);
    form->setSpacing(6);

    auto& s = AppSettings::instance();

    // Audio block size
    // From Thetis TCIServer.cs [v2.10.3.13] — audio block size (default 2048)
    m_audioBlockSpin = new QSpinBox(group);
    m_audioBlockSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
    m_audioBlockSpin->setRange(100, 2048);
    m_audioBlockSpin->setSuffix(tr(" samples"));
    m_audioBlockSpin->setToolTip(
        tr("Number of audio samples per TCI audio stream block (100–2048). "
           "Larger blocks reduce overhead but increase latency."));
    m_audioBlockSpin->setValue(
        s.value(QStringLiteral("TciAudioStreamSamples"), 2048).toInt());
    connect(m_audioBlockSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [](int v) {
        AppSettings::instance().setValue(QStringLiteral("TciAudioStreamSamples"), v);
    });
    form->addRow(tr("Block size:"), m_audioBlockSpin);

    // TX channel
    // From Thetis TCIServer.cs [v2.10.3.13] — TX audio channel routing
    m_txChannelCombo = new QComboBox(group);
    m_txChannelCombo->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
    m_txChannelCombo->addItems({
        QStringLiteral("Left"),
        QStringLiteral("Right"),
        QStringLiteral("Both"),
    });
    m_txChannelCombo->setToolTip(
        tr("Which audio channel carries the TX audio in the TCI audio stream. "
           "\"Both\" sends the same mono signal to both left and right channels."));
    const QString savedTxCh = s.value(QStringLiteral("TciTxChannel"),
                                       QStringLiteral("Both")).toString();
    const int txChIdx = m_txChannelCombo->findText(savedTxCh);
    m_txChannelCombo->setCurrentIndex(txChIdx >= 0 ? txChIdx
                                                    : m_txChannelCombo->findText(QStringLiteral("Both")));
    connect(m_txChannelCombo, &QComboBox::currentTextChanged, this, [](const QString& text) {
        AppSettings::instance().setValue(QStringLiteral("TciTxChannel"), text);
    });
    form->addRow(tr("TX channel:"), m_txChannelCombo);

    contentLayout()->addWidget(group);
}

// ---------------------------------------------------------------------------
// Group 5: Sensors
// Controls: RX interval / TX interval / Note label.
// AppSettings: TciRxSensorIntervalMs, TciTxSensorIntervalMs.
// ---------------------------------------------------------------------------
void CatTciServerPage::buildSensorsGroup()
{
    auto* group = new QGroupBox(tr("Sensors"), this);
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
    auto* form = new QFormLayout(group);
    form->setSpacing(6);

    auto& s = AppSettings::instance();

    // RX sensor interval
    // From Thetis TCIServer.cs [v2.10.3.13] — RX sensor push interval (default 200 ms)
    m_rxSensorSpin = new QSpinBox(group);
    m_rxSensorSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
    m_rxSensorSpin->setRange(30, 1000);
    m_rxSensorSpin->setSuffix(tr(" ms"));
    m_rxSensorSpin->setToolTip(
        tr("How often RX sensor data (signal level, AGC gain, etc.) is pushed to "
           "TCI clients that subscribe to sensors (30–1000 ms)."));
    m_rxSensorSpin->setValue(
        s.value(QStringLiteral("TciRxSensorIntervalMs"), 200).toInt());
    connect(m_rxSensorSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [](int v) {
        AppSettings::instance().setValue(QStringLiteral("TciRxSensorIntervalMs"), v);
    });
    form->addRow(tr("RX interval:"), m_rxSensorSpin);

    // TX sensor interval
    // From Thetis TCIServer.cs [v2.10.3.13] — TX sensor push interval (default 200 ms)
    m_txSensorSpin = new QSpinBox(group);
    m_txSensorSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
    m_txSensorSpin->setRange(30, 1000);
    m_txSensorSpin->setSuffix(tr(" ms"));
    m_txSensorSpin->setToolTip(
        tr("How often TX sensor data (forward power, SWR, ALC, etc.) is pushed to "
           "TCI clients that subscribe to sensors (30–1000 ms)."));
    m_txSensorSpin->setValue(
        s.value(QStringLiteral("TciTxSensorIntervalMs"), 200).toInt());
    connect(m_txSensorSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [](int v) {
        AppSettings::instance().setValue(QStringLiteral("TciTxSensorIntervalMs"), v);
    });
    form->addRow(tr("TX interval:"), m_txSensorSpin);

    // Informational note — not an AppSettings key
    auto* noteLabel = new QLabel(
        tr("Effective rate is the minimum across all subscribed clients "
           "(TciSensorManager aggregation)."),
        group);
    noteLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    noteLabel->setWordWrap(true);
    form->addRow(noteLabel);

    contentLayout()->addWidget(group);
}

// ---------------------------------------------------------------------------
// Group 6: VFO Quirks
// Controls: Forget RX2 VFOB on disconnect / Use RX1 VFOA for RX2 VFOA /
//           Copy RX2 VFOB to VFOA.
// AppSettings: TciForgetRx2VfoBOnDisconnect, TciUseRx1VfoaForRx2Vfoa,
//              TciCopyRx2VfobToVfoa.
// ---------------------------------------------------------------------------
void CatTciServerPage::buildVfoQuirksGroup()
{
    auto* group = new QGroupBox(tr("VFO Quirks"), this);
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
    auto* form = new QFormLayout(group);
    form->setSpacing(6);

    auto& s = AppSettings::instance();

    // Forget RX2 VFOB on disconnect
    // From Thetis TCIServer.cs [v2.10.3.13] — RX2 VFOB forget-on-disconnect
    m_forgetRx2VfoBCheck = new QCheckBox(tr("Forget RX2 VFOB on disconnect"), group);
    m_forgetRx2VfoBCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_forgetRx2VfoBCheck->setToolTip(
        tr("When a TCI client disconnects, reset RX2 VFOB to its default frequency "
           "instead of keeping the last value set by the client."));
    m_forgetRx2VfoBCheck->setChecked(
        s.value(QStringLiteral("TciForgetRx2VfoBOnDisconnect"), QStringLiteral("False")).toString()
        == QStringLiteral("True"));
    connect(m_forgetRx2VfoBCheck, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("TciForgetRx2VfoBOnDisconnect"),
                                          on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    form->addRow(QString(), m_forgetRx2VfoBCheck);

    // Use RX1 VFOA for RX2 VFOA
    // From Thetis TCIServer.cs [v2.10.3.13] — shared-VFOA quirk
    m_useRx1VfoaForRx2Check = new QCheckBox(tr("Use RX1 VFOA for RX2 VFOA"), group);
    m_useRx1VfoaForRx2Check->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_useRx1VfoaForRx2Check->setToolTip(
        tr("Report the RX1 VFOA frequency when a TCI client queries RX2 VFOA. "
           "Required by clients that do not maintain independent per-receiver VFO state."));
    m_useRx1VfoaForRx2Check->setChecked(
        s.value(QStringLiteral("TciUseRx1VfoaForRx2Vfoa"), QStringLiteral("False")).toString()
        == QStringLiteral("True"));
    connect(m_useRx1VfoaForRx2Check, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("TciUseRx1VfoaForRx2Vfoa"),
                                          on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    form->addRow(QString(), m_useRx1VfoaForRx2Check);

    // Copy RX2 VFOB to VFOA
    // From Thetis TCIServer.cs [v2.10.3.13] — VFOB→VFOA copy quirk
    m_copyRx2VfobToVfoaCheck = new QCheckBox(tr("Copy RX2 VFOB to VFOA"), group);
    m_copyRx2VfobToVfoaCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_copyRx2VfobToVfoaCheck->setToolTip(
        tr("Automatically copy RX2 VFOB into RX2 VFOA whenever VFOB changes. "
           "Required by apps that drive split mode via VFOB but read back VFOA."));
    m_copyRx2VfobToVfoaCheck->setChecked(
        s.value(QStringLiteral("TciCopyRx2VfobToVfoa"), QStringLiteral("False")).toString()
        == QStringLiteral("True"));
    connect(m_copyRx2VfobToVfoaCheck, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("TciCopyRx2VfobToVfoa"),
                                          on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    form->addRow(QString(), m_copyRx2VfobToVfoaCheck);

    contentLayout()->addWidget(group);
}

// ---------------------------------------------------------------------------
// CatTciServerPage live-status hookup
//
// Phase 3J-1 bench fix (2026-05-11): the Server group box title and Status
// label reflect the live TciServer state (running + client count) so the
// operator can see at a glance whether the server is up and how many TCI
// clients are connected.  Modeled on Thetis Setup.cs:9491-9494
// [v2.10.3.13] — TCIClientsConnectedChange setter updates
// `grpTCIServer.Text = "TCI Server (N clients)"`.
//
// Client count is tracked locally via clientConnected/clientDisconnected
// signal increments — TciServer exposes the signals but not a count getter,
// and the local counter is the canonical pattern (MainWindow uses the same
// approach for m_tciClientCount).
// ---------------------------------------------------------------------------

// ── populateBindAddressCombo ─────────────────────────────────────────────────
//
// Phase 3J-1 closeout Item 1 (2026-05-12): enumerate bindable interfaces
// via QNetworkInterface::allInterfaces() and add one combo entry per
// detected non-loopback IPv4 (and IPv6) NIC, plus the well-known options:
//   - Loopback only (127.0.0.1)         ← default
//   - Any IPv4 interface (0.0.0.0)
//   - <detected non-loopback IPv4 NICs>
//   - Loopback IPv6 (::1)
//   - Any IPv6 interface (::)
//   - <detected non-loopback IPv6 NICs>
//
// Each entry's data() carries the bindable address string used by
// QHostAddress::setAddress and persisted as `TciServerBindAddress`.
// Selection is restored from AppSettings if it matches any entry's
// data; otherwise falls back to Loopback (the safe default that
// requires no LAN trust).
//
// Functional behavior matches Thetis Setup.cs:22460 [v2.10.3.13]
// `IPAddress.TryParse` — any valid IPv4 or IPv6 the operator can type
// into Thetis is also pickable here, but only addresses that actually
// exist on a NIC at the moment of page-construct are surfaced.
void CatTciServerPage::populateBindAddressCombo()
{
    if (!m_bindAddressCombo) { return; }
    m_bindAddressCombo->clear();

    // Well-known IPv4 options first.
    m_bindAddressCombo->addItem(
        tr("Loopback only (127.0.0.1)"),
        QStringLiteral("127.0.0.1"));
    m_bindAddressCombo->addItem(
        tr("Any IPv4 interface (0.0.0.0) — exposes to LAN"),
        QStringLiteral("0.0.0.0"));

    // Enumerate detected NICs.  Skip loopback (already in the well-known
    // list) and skip down/disabled interfaces.  For IPv6, also skip link-
    // local addresses (scope-id-dependent, fragile across networks).
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto& iface : interfaces) {
        const auto flags = iface.flags();
        if (!flags.testFlag(QNetworkInterface::IsRunning)) { continue; }
        if (flags.testFlag(QNetworkInterface::IsLoopBack)) { continue; }
        for (const auto& entry : iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (ip.isNull()) { continue; }
            if (ip.protocol() == QAbstractSocket::IPv4Protocol) {
                const QString label = QStringLiteral("%1 — %2")
                    .arg(iface.name(), ip.toString());
                m_bindAddressCombo->addItem(label, ip.toString());
            }
        }
    }

    // IPv6 well-known options.
    m_bindAddressCombo->addItem(
        tr("Loopback IPv6 (::1)"),
        QStringLiteral("::1"));
    m_bindAddressCombo->addItem(
        tr("Any IPv6 interface (::) — exposes to LAN"),
        QStringLiteral("::"));

    // Enumerate non-link-local IPv6 NICs (link-local addresses include a
    // scope-id and don't bind cleanly without the index suffix; skip them
    // for the initial Phase 3J-1 scope).
    for (const auto& iface : interfaces) {
        const auto flags = iface.flags();
        if (!flags.testFlag(QNetworkInterface::IsRunning)) { continue; }
        if (flags.testFlag(QNetworkInterface::IsLoopBack)) { continue; }
        for (const auto& entry : iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (ip.isNull()) { continue; }
            if (ip.protocol() == QAbstractSocket::IPv6Protocol) {
                if (ip.isLinkLocal()) { continue; }
                const QString label = QStringLiteral("%1 — %2")
                    .arg(iface.name(), ip.toString());
                m_bindAddressCombo->addItem(label, ip.toString());
            }
        }
    }

    // Restore selection from AppSettings (default Loopback).
    const QString stored = AppSettings::instance().value(
        QStringLiteral("TciServerBindAddress"),
        QStringLiteral("127.0.0.1")).toString();
    const int restoreIdx = m_bindAddressCombo->findData(stored);
    if (restoreIdx >= 0) {
        m_bindAddressCombo->setCurrentIndex(restoreIdx);
    } else {
        // Stored address not currently available (NIC unplugged?).
        // Fall back to Loopback and re-persist so we don't fight a
        // stale value the next time the page opens.
        m_bindAddressCombo->setCurrentIndex(0);  // Loopback
        AppSettings::instance().setValue(
            QStringLiteral("TciServerBindAddress"),
            QStringLiteral("127.0.0.1"));
    }
}

void CatTciServerPage::setTciServer(NereusSDR::TciServer* server)
{
#ifdef HAVE_WEBSOCKETS
    // Disconnect any previous hookup so re-calls don't accumulate slots.
    if (m_tciServerRef) {
        disconnect(m_tciServerRef.data(), nullptr, this, nullptr);
    }
    m_tciServerRef = server;
    m_tciServerRunning = (server != nullptr) && server->isRunning();
    m_tciClientCount = 0;  // reset; we'll learn the count from signal flow

    if (server) {
        connect(server, &NereusSDR::TciServer::serverStarted,
                this, [this](quint16) {
                    m_tciServerRunning = true;
                    m_tciClientCount = 0;
                    refreshTciStatusDisplay();
                });
        connect(server, &NereusSDR::TciServer::serverStopped,
                this, [this]() {
                    m_tciServerRunning = false;
                    m_tciClientCount = 0;
                    refreshTciStatusDisplay();
                });
        connect(server, &NereusSDR::TciServer::clientConnected,
                this, [this](QWebSocket*) {
                    ++m_tciClientCount;
                    refreshTciStatusDisplay();
                });
        connect(server, &NereusSDR::TciServer::clientDisconnected,
                this, [this](QWebSocket*) {
                    if (m_tciClientCount > 0) { --m_tciClientCount; }
                    refreshTciStatusDisplay();
                });
    }
    refreshTciStatusDisplay();
#else
    Q_UNUSED(server);
#endif
}

void CatTciServerPage::refreshTciStatusDisplay()
{
    // Group box title: "Server"  →  "Server (N clients)"  when running.
    // Stays plain "Server" when stopped so the title doesn't lie about
    // active state.  Matches Thetis Setup.cs:9491-9494 [v2.10.3.13] —
    // `grpTCIServer.Text = "TCI Server (" + value + " clients)"`.
    if (m_serverGroup) {
        if (m_tciServerRunning) {
            m_serverGroup->setTitle(
                tr("Server (%1 %2)")
                    .arg(m_tciClientCount)
                    .arg(m_tciClientCount == 1 ? tr("client") : tr("clients")));
        } else {
            m_serverGroup->setTitle(tr("Server"));
        }
    }

    // Status label.  Use a coloured dot prefix for at-a-glance state:
    //   ● red   = stopped
    //   ● green = running (with client count appended)
    if (m_statusLabel) {
        if (m_tciServerRunning) {
            m_statusLabel->setText(
                tr("<span style='color:#3DD068'>●</span> Running (%1 %2)")
                    .arg(m_tciClientCount)
                    .arg(m_tciClientCount == 1 ? tr("client") : tr("clients")));
        } else {
            m_statusLabel->setText(
                tr("<span style='color:#D04040'>●</span> Stopped"));
        }
        m_statusLabel->setTextFormat(Qt::RichText);
    }

    // Phase 3J-1 closeout Item 2 (2026-05-12): Show Log button is only
    // useful while the server is running -- before then there is no
    // TciServer to subscribe the log window to.  Disabled state is the
    // initial cold-start condition set in buildServerGroup().
    if (m_showLogBtn) {
        m_showLogBtn->setEnabled(m_tciServerRunning);
    }
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
