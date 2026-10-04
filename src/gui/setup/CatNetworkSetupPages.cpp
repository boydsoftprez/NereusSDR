// no-port-check: NereusSDR-original UI file. The "// From Thetis" inline
// comments below are design cross-references documenting where AppSettings
// key names and default values were verified against the Thetis control
// inventory (setup.designer.cs / TCIServer.cs). No Thetis code is
// translated here; all AppSettings keys are attributed in TciProtocol.h.
// 2026-09-27 - Parity Task 23 Core TCI options and read-only bind,
//              J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-09-29 - R-R3-49 / R-IOS-18: Setup description version 15 ids on the
//              Peripherals rows. J.J. Boyd (KG4VCF), AI-assisted via
//              Anthropic Claude Code.
// 2026-09-29 - The three RX2 VFO options work: captions and tooltips say
//              what Thetis's options do, their defaults come from
//              TciProtocol.h, and Forget follows Duplicate as in Thetis.
//              J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

#include "CatNetworkSetupPages.h"
#include "gui/StyleConstants.h"
#include "gui/LanScanDialog.h"
#include "gui/OperatorReasonText.h"
#include "core/AppSettings.h"
#include "core/session/IStationLink.h"
#include "models/AmplifierModel.h"
#include "models/StationTciModel.h"
#include "core/TciProtocol.h"
#include "core/TciSwitch.h"
#include "core/TciUpdateGap.h"
#include "models/RadioModel.h"

#include <QHideEvent>
#include <QSignalBlocker>
#include <QTimer>
#include <QNetworkInterface>
#ifdef HAVE_WEBSOCKETS
#include "core/TciServer.h"
#include <QWebSocket>
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
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcPeripherals, "nereus.peripherals")

namespace NereusSDR {

// ---------------------------------------------------------------------------
// CatSerialPortsPage
// ---------------------------------------------------------------------------

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
    buildCoreGroup();
    buildCompatibilityGroup();
    buildIqStreamGroup();
    buildAudioStreamGroup();
    buildSensorsGroup();
    buildVfoQuirksGroup();

    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// Group 1: Server
// Controls: Enable / Listen on (address dropdown) / Port + Default button /
//           Send initial state / Rate limit / Show Log button / Status line.
// AppSettings: TciServerEnabled, TciServerPort, TciSendInitialFrequencyStateOnConnect,
//              TciRateLimitMs.
// ---------------------------------------------------------------------------
void CatTciServerPage::buildServerGroup()
{
    auto* group = new QGroupBox(tr("This window's server"), this);
    m_serverGroup = group;  // saved so refreshTciStatusDisplay() can update title
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
    auto* form = new QFormLayout(group);
    form->setSpacing(6);

    auto& s = AppSettings::instance();

    // Enable checkbox
    // From Thetis setup.designer.cs:57979-57983 [v2.10.3.13] — chkTCIEnable
    m_enableCheck = new QCheckBox(tr("Enable TCI Server"), group);
    m_enableCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    // R-R3-21 / R-R3-48 (operator wording, 2026-09-24): in a window on a
    // Core the switch and port are the Core's; the station line below says
    // so in that window.
    m_enableCheck->setToolTip(tr("Turn on the TCI server so programs like WSJT-X or JTDX "
                                 "can control this radio."));
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

    // ── "Listen on:" address dropdown ───────────────────────────────────────
    //
    // Phase 3J-1 closeout Item 1 (2026-05-12): replaces the read-only
    // "127.0.0.1" label with an interface-aware dropdown.  Operator can
    // pick (labels reworded 2026-09-24, R-R3-21):
    //   - "This computer only (127.0.0.1)": default; safest
    //   - "Any IPv4 address (0.0.0.0), open to your network"
    //   - A specific detected NIC (e.g. "en0 (192.168.1.50)")
    //   - IPv6 equivalents
    //
    // Functional parity with Thetis Setup.cs:22410-22473 [v2.10.3.13]
    // `txtTCIServerBindIPPort` (which uses a free-text "IP:port" field
    // accepting any valid IPv4/IPv6 via `IPAddress.TryParse`).  UX
    // diverges per CLAUDE.md feedback_source_first_ui_vs_dsp.md — Qt
    // widgets are NereusSDR-native; a dropdown with validated, NIC-aware
    // choices is the better UX for our platform.
    m_bindAddressCombo = new QComboBox(group);
    m_bindAddressCombo->setObjectName(QStringLiteral("tciListenOnCombo"));
    m_bindAddressCombo->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
    m_bindAddressCombo->setToolTip(tr(
        "The IP address the TCI server listens on. "
        "127.0.0.1 accepts programs on this computer only. "
        "0.0.0.0 accepts them from anywhere on your network. "
        "TCI has no password, so choose a network address or 0.0.0.0 "
        "only on a network you trust."));
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
    form->addRow(tr("Listen on:"), m_bindAddressCombo);

    // Port spinbox + Default button
    // From Thetis setup.designer.cs:57991-57998 [v2.10.3.13] — udTCIPort (default 50001)
    m_portSpin = new QSpinBox(group);
    m_portSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
    m_portSpin->setRange(1024, 65535);
    // Rework follow-up 5 (R-R3-48): the port is sent (to this window's
    // server and the Core's) when editing finishes (Enter, focus leaving,
    // the arrows), not for every keystroke.
    m_portSpin->setKeyboardTracking(false);
    m_portSpin->setToolTip(tr("The TCP port the TCI server listens on (1024–65535). "
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

    // Rate limit: the gap between frequency updates sent to each app.
    // Receiver and transmit gaps plan, Task 10 (R-R3-49). Thetis's
    // udTCIRateLimit is not a limit on incoming messages: it is the shortest
    // gap in ms between outgoing vfo, dds and tx_frequency updates to each
    // app (TCIServer.cs:6421-6480 [v2.10.3.15], ported in TciUpdateGap).
    // From Thetis setup.designer.cs:58629-58664 [v2.10.3.15]: label
    // "Rate Limit (ms)", Minimum 0, Maximum 1000, Value 100, tooltip
    // "The maximum rate VFO/IF/DDS messages can be sent to clients"
    // (reworded in plain words below). Thetis applies a change when the
    // server is next started (setup.cs:22563-22566 [v2.10.3.15] shows a
    // "toggle to use" note); here it reaches the running server at once.
    m_rateLimitSpin = new QSpinBox(group);
    m_rateLimitSpin->setObjectName(QStringLiteral("tciRateLimitSpin"));
    m_rateLimitSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
    m_rateLimitSpin->setRange(NereusSDR::TciUpdateGap::kMinGapMs,
                              NereusSDR::TciUpdateGap::kMaxGapMs);
    m_rateLimitSpin->setSuffix(tr(" ms"));
    m_rateLimitSpin->setSpecialValueText(tr("Off"));
    m_rateLimitSpin->setToolTip(tr("How long to wait between frequency updates sent to each TCI app. "
                                    "Changes made faster than this reach the app as the latest "
                                    "frequency once the time has passed. Off sends every change."));
    m_rateLimitSpin->setValue(
        s.value(QString::fromLatin1(NereusSDR::TciUpdateGap::kSettingKey),
                NereusSDR::TciUpdateGap::kDefaultGapMs).toInt());
    connect(m_rateLimitSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        AppSettings::instance().setValue(QString::fromLatin1(NereusSDR::TciUpdateGap::kSettingKey), v);
#ifdef HAVE_WEBSOCKETS
        if (m_tciServerRef) {
            m_tciServerRef->setUpdateGapMs(v);
        }
#endif
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

    // R-R3-48: in a remote window on a Core that runs its own TCI server,
    // where devices at the station (the RF-Kit amplifier) reach it. The
    // switch and port above drive both servers.
    m_stationLine = new QLabel(group);
    m_stationLine->setObjectName(QStringLiteral("tciStationLine"));
    m_stationLine->setTextFormat(Qt::PlainText);
    m_stationLine->setWordWrap(true);
    m_stationLine->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    m_stationLine->setVisible(false);
    form->addRow(QString(), m_stationLine);

    contentLayout()->addWidget(group);
}

void CatTciServerPage::setRadioModel(NereusSDR::RadioModel* model)
{
    if (m_radioModelRef) {
        disconnect(m_radioModelRef, nullptr, this, nullptr);
        if (auto* station = m_radioModelRef->stationTciModel()) {
            disconnect(station, nullptr, this, nullptr);
        }
    }
    m_radioModelRef = model;
    if (model) {
        connect(model, &NereusSDR::RadioModel::stationLinkStateChanged,
                this, &CatTciServerPage::refreshStationLine);
        connect(model, &NereusSDR::RadioModel::coreOnAirChanged,
                this, &CatTciServerPage::refreshCoreGroup);
        connect(model, &NereusSDR::RadioModel::infoChanged,
                this, &CatTciServerPage::refreshIqStreamGroup);
        if (auto* station = model->stationTciModel()) {
            connect(station, &NereusSDR::StationTciModel::stateChanged,
                    this, &CatTciServerPage::refreshStationLine);
        }
    }
    refreshStationLine();
    refreshCoreGroup();
    refreshIqStreamGroup();
}

void CatTciServerPage::refreshIqStreamGroup()
{
    if (!m_iqSwapCheck || !m_alwaysStreamIqCheck) { return; }
    const bool unavailable = m_radioModelRef
        && m_radioModelRef->role() == NereusSDR::RadioModel::Role::Remote
        && m_radioModelRef->stationRemoteIqVersion() < 1;
    const QString reason = tr("The connected Core does not support remote TCI IQ streaming.");
    m_iqSwapCheck->setEnabled(!unavailable);
    m_alwaysStreamIqCheck->setEnabled(!unavailable);
    if (unavailable) {
        m_iqSwapCheck->setToolTip(reason);
        m_alwaysStreamIqCheck->setToolTip(reason);
    } else {
        m_iqSwapCheck->setToolTip(
            tr("Swap the I and Q samples in the TCI IQ data stream. "
               "Enabled by default for compatibility with most TCI IQ consumers."));
        m_alwaysStreamIqCheck->setToolTip(
            tr("Stream IQ data to all connected TCI clients continuously, even if no client "
               "has explicitly subscribed to the IQ stream. Increases CPU and network load."));
    }
}

void CatTciServerPage::refreshStationLine()
{
    // Rework part 1 (R-R3-48, one switch and one port): the switch and port
    // show the Core's, which TciSwitch writes to this computer's settings
    // once the Core's whole change has arrived; read them after it.
    QTimer::singleShot(0, this, &CatTciServerPage::reloadSwitchFromSettings);
    if (!m_stationLine) {
        return;
    }
    const QString line = NereusSDR::TciSwitch::stationLine(m_radioModelRef.data());
    m_stationLine->setText(line);
    m_stationLine->setVisible(!line.isEmpty());
    refreshCoreGroup();
}

void CatTciServerPage::buildCoreGroup()
{
    m_coreGroup = new QGroupBox(tr("The Core's TCI server"), this);
    m_coreGroup->setObjectName(QStringLiteral("coreTciOptions"));
    m_coreGroup->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
    auto* form = new QFormLayout(m_coreGroup);
    form->setSpacing(6);
    m_coreBind = new QLabel(m_coreGroup);
    m_coreBind->setObjectName(QStringLiteral("coreTciBind"));
    m_coreBind->setTextFormat(Qt::PlainText);
    form->addRow(tr("Listens on:"), m_coreBind);
    m_coreExpert = new QCheckBox(tr("Emulate ExpertSDR3 protocol"), m_coreGroup);
    m_coreSunSdr = new QCheckBox(tr("Emulate SunSDR2 PRO device"), m_coreGroup);
    m_coreCwlu = new QCheckBox(tr("CWL/CWU becomes CW"), m_coreGroup);
    m_coreInitial = new QCheckBox(tr("Send initial state on connect"), m_coreGroup);
    m_coreExpert->setProperty("nereusSetupId", "catNetwork.tciServer.coreExpert");
    m_coreSunSdr->setProperty("nereusSetupId", "catNetwork.tciServer.coreSunSdr");
    m_coreCwlu->setProperty("nereusSetupId", "catNetwork.tciServer.coreCwlu");
    m_coreInitial->setProperty("nereusSetupId", "catNetwork.tciServer.coreInitial");
    for (QCheckBox* option : {m_coreExpert, m_coreSunSdr, m_coreCwlu, m_coreInitial}) {
        option->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
        connect(option, &QCheckBox::toggled, this, &CatTciServerPage::sendCoreOptions);
        form->addRow(QString(), option);
    }
    // JJ's ruling of 2026-09-28 (stationTciSettingsVersion 1): the rest of
    // this page's settings, for the Core's server. The captions, ranges and
    // tooltips are this page's own (the groups below).
    const auto addCheck = [this, form](const char* name, const QString& text,
                                       const QString& tip) {
        auto* box = new QCheckBox(text, m_coreGroup);
        box->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
        box->setProperty("nereusSetupId",
                         QStringLiteral("catNetwork.tciServer.core.%1").arg(QLatin1String(name)));
        box->setToolTip(tip);
        m_coreSettingTips.insert(QByteArray(name), tip);
        const QByteArray key(name);
        connect(box, &QCheckBox::toggled, this,
                [this, key](bool on) { sendCoreSetting(key, on); });
        form->addRow(QString(), box);
        m_coreSettings.insert(key, box);
    };
    const auto addSpin = [this, form](const char* name, const QString& label, int min, int max,
                                      const QString& suffix, const QString& tip) {
        auto* spin = new QSpinBox(m_coreGroup);
        spin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
        spin->setRange(min, max);
        spin->setSuffix(suffix);
        spin->setKeyboardTracking(false);
        spin->setProperty("nereusSetupId",
                          QStringLiteral("catNetwork.tciServer.core.%1").arg(QLatin1String(name)));
        spin->setToolTip(tip);
        m_coreSettingTips.insert(QByteArray(name), tip);
        const QByteArray key(name);
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this, key](int value) { sendCoreSetting(key, value); });
        form->addRow(label, spin);
        m_coreSettings.insert(key, spin);
        return spin;
    };
    addSpin("rateLimitMs", tr("Rate limit:"), NereusSDR::TciUpdateGap::kMinGapMs,
            NereusSDR::TciUpdateGap::kMaxGapMs, tr(" ms"),
            tr("How long to wait between frequency updates sent to each TCI app. Changes made "
               "faster than this reach the app as the latest frequency once the time has "
               "passed. Off sends every change."))->setSpecialValueText(tr("Off"));
    addCheck("cwBecomesCwuAbove10mhz", tr("CW becomes CWU above 10 MHz"),
             tr("On bands above 10 MHz, report mode as \"CWU\" instead of \"CW\" or \"CWL\". "
                "Required by certain logging apps that follow the ARRL sideband convention."));
    addCheck("iqSwap", tr("Swap I/Q channels"),
             tr("Swap the I and Q samples in the TCI IQ data stream. "
                "Enabled by default for compatibility with most TCI IQ consumers."));
    addCheck("alwaysStreamIq", tr("Always stream IQ"),
             tr("Stream IQ data to all connected TCI clients continuously, even if no client "
                "has explicitly subscribed to the IQ stream. Increases CPU and network load."));
    addSpin("audioBlockSamples", tr("Block size:"), 100, 2048, tr(" samples"),
            tr("Number of audio samples per TCI audio stream block (100 to 2048). "
               "Larger blocks reduce overhead but increase latency."));
    {
        auto* combo = new QComboBox(m_coreGroup);
        combo->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
        combo->addItems({QStringLiteral("Left"), QStringLiteral("Right"), QStringLiteral("Both")});
        combo->setProperty("nereusSetupId", QStringLiteral("catNetwork.tciServer.core.txChannel"));
        const QString channelTip =
            tr("Which audio channel carries the TX audio in the TCI audio stream. "
               "\"Both\" sends the same mono signal to both left and right channels.");
        combo->setToolTip(channelTip);
        m_coreSettingTips.insert(QByteArrayLiteral("txChannel"), channelTip);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int index) { sendCoreSetting(QByteArrayLiteral("txChannel"), index); });
        form->addRow(tr("TX channel:"), combo);
        m_coreSettings.insert(QByteArrayLiteral("txChannel"), combo);
    }
    addSpin("rxSensorIntervalMs", tr("RX interval:"), 30, 1000, tr(" ms"),
            tr("How often RX sensor data (signal level, AGC gain, etc.) is pushed to TCI clients "
               "that subscribe to sensors (30 to 1000 ms)."));
    addSpin("txSensorIntervalMs", tr("TX interval:"), 30, 1000, tr(" ms"),
            tr("How often TX sensor data (forward power, SWR, ALC, etc.) is pushed to TCI "
               "clients that subscribe to sensors (30 to 1000 ms)."));
    // The three RX2 VFO options (TciProtocol.cpp applies them).
    addCheck("forgetRx2VfoBOnDisconnect", rx2VfoForgetLabel(), rx2VfoForgetTip());
    addCheck("useRx1VfoaForRx2Vfoa", rx2VfoUseRx1Label(), rx2VfoUseRx1Tip());
    addCheck("copyRx2VfobToVfoa", rx2VfoCopyLabel(), rx2VfoCopyTip());
    // Forget works only with Duplicate, so it is enabled only while
    // Duplicate is on, as on Thetis's page.
    // From Thetis setup.cs:22568-22572 [v2.10.3.15] (chkCopyRX2VFObToVFOa_CheckedChanged)
    auto* coreCopy = qobject_cast<QCheckBox*>(m_coreSettings.value("copyRx2VfobToVfoa"));
    QWidget* coreForget = m_coreSettings.value("forgetRx2VfoBOnDisconnect");
    connect(coreCopy, &QCheckBox::toggled, coreForget,
            [coreCopy, coreForget](bool on) { coreForget->setEnabled(on && coreCopy->isEnabled()); });
    m_coreReason = new QLabel(m_coreGroup);
    m_coreReason->setObjectName(QStringLiteral("coreTciReason"));
    m_coreReason->setWordWrap(true);
    m_coreReason->setTextFormat(Qt::PlainText);
    form->addRow(QString(), m_coreReason);
    m_coreGroup->hide();
    contentLayout()->addWidget(m_coreGroup);
}

void CatTciServerPage::refreshCoreGroup()
{
    if (!m_coreGroup) {
        return;
    }
    const bool remote = m_radioModelRef && m_radioModelRef->role() == RadioModel::Role::Remote;
    m_coreGroup->setVisible(remote);
    if (!remote) {
        return;
    }
    IStationLink* link = m_radioModelRef->stationLink();
    const StationTciModel* station = m_radioModelRef->stationTciModel();
    const bool available = link && link->stationTciServerAvailable() && station;
    const bool onAir = m_radioModelRef->isCoreOnAir();
    const QString reason = !link || !link->stationLinkReady()
        ? tr("Connect to the Core to change its TCI server settings.")
        : !available ? IStationLink::stationTciServerUnavailableReason()
                     : onAir ? tr("The radio is on the air. Try again when it stops.") : QString();
    m_coreReason->setText(reason);
    m_coreBind->setText(station && link && link->stationTciAvailable()
        ? QStringLiteral("%1, port %2 (set on the Core)")
              .arg(station->stationAddress().isEmpty() ? QStringLiteral("the Core's computer")
                                                       : station->stationAddress())
              .arg(station->port())
        : QStringLiteral("--"));
    if (station && available) {
        const QSignalBlocker b1(m_coreExpert);
        const QSignalBlocker b2(m_coreSunSdr);
        const QSignalBlocker b3(m_coreCwlu);
        const QSignalBlocker b4(m_coreInitial);
        m_coreExpert->setChecked(station->emulateExpertSdr3());
        m_coreSunSdr->setChecked(station->emulateSunSdr2Pro());
        m_coreCwlu->setChecked(station->cwluBecomesCw());
        m_coreInitial->setChecked(station->sendInitialState());
    }
    for (QCheckBox* option : {m_coreExpert, m_coreSunSdr, m_coreCwlu, m_coreInitial}) {
        option->setEnabled(available && !onAir);
        option->setToolTip(reason);
    }
    // JJ's ruling of 2026-09-28: the rest of the server's settings, from a
    // Core that shares them (stationTciSettingsVersion 1); otherwise shown
    // disabled with the reason.
    const bool settingsAvailable = available && link->stationTciSettingsAvailable();
    const QString settingsReason = !reason.isEmpty() ? reason
        : !settingsAvailable ? IStationLink::stationTciServerUnavailableReason() : QString();
    for (auto it = m_coreSettings.cbegin(); it != m_coreSettings.cend(); ++it) {
        QWidget* control = it.value();
        if (station && settingsAvailable) {
            const StationTciModel::Setting* setting = StationTciModel::setting(it.key());
            const QVariant value = setting ? StationTciModel::valueIn(station->state(), *setting)
                                           : QVariant();
            const QSignalBlocker block(control);
            if (auto* box = qobject_cast<QCheckBox*>(control)) {
                box->setChecked(value.toBool());
            } else if (auto* spin = qobject_cast<QSpinBox*>(control)) {
                spin->setValue(value.toInt());
            } else if (auto* combo = qobject_cast<QComboBox*>(control)) {
                combo->setCurrentIndex(value.toInt());
            }
        }
        control->setEnabled(settingsAvailable && !onAir);
        control->setToolTip(settingsReason.isEmpty() ? m_coreSettingTips.value(it.key())
                                                     : settingsReason);
    }
    // Forget RX2 VFO B works only with Duplicate: enabled only while
    // Duplicate is on (Thetis setup.cs chkCopyRX2VFObToVFOa_CheckedChanged).
    if (auto* copy = qobject_cast<QCheckBox*>(m_coreSettings.value("copyRx2VfobToVfoa"))) {
        QWidget* forget = m_coreSettings.value("forgetRx2VfoBOnDisconnect");
        forget->setEnabled(copy->isEnabled() && copy->isChecked());
    }
}

void CatTciServerPage::sendCoreSetting(const QByteArray& name, const QVariant& value)
{
    IStationLink* link = m_radioModelRef ? m_radioModelRef->stationLink() : nullptr;
    if (!link || !link->stationTciSettingsAvailable()) {
        refreshCoreGroup();
        return;
    }
    const auto outcome = link->requestStationTciSetting(name, value);
    if (!outcome.sent) {
        m_coreReason->setText(outcome.reason);
    }
}

void CatTciServerPage::sendCoreOptions()
{
    IStationLink* link = m_radioModelRef ? m_radioModelRef->stationLink() : nullptr;
    if (!link || !link->stationTciServerAvailable()) {
        refreshCoreGroup();
        return;
    }
    const auto outcome = link->requestStationTciOptions(
        m_coreExpert->isChecked(), m_coreSunSdr->isChecked(), m_coreCwlu->isChecked(),
        m_coreInitial->isChecked());
    if (!outcome.sent) {
        m_coreReason->setText(outcome.reason);
    }
}

void CatTciServerPage::reloadSwitchFromSettings()
{
    auto& s = AppSettings::instance();
    if (m_enableCheck) {
        const QSignalBlocker block(m_enableCheck);
        m_enableCheck->setChecked(
            s.value(QStringLiteral("TciServerEnabled"), QStringLiteral("False")).toString()
            == QStringLiteral("True"));
    }
    if (m_portSpin && !m_portSpin->hasFocus()) {
        const QSignalBlocker block(m_portSpin);
        m_portSpin->setValue(s.value(QStringLiteral("TciServerPort"), 50001).toInt());
    }
}

bool CatTciServerPage::switchOnForTesting() const
{
    return m_enableCheck && m_enableCheck->isChecked();
}

int CatTciServerPage::portForTesting() const
{
    return m_portSpin ? m_portSpin->value() : 0;
}

QString CatTciServerPage::stationLineForTesting() const
{
    return m_stationLine && !m_stationLine->isHidden() ? m_stationLine->text() : QString();
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
    connect(m_alwaysStreamIqCheck, &QCheckBox::toggled, this, [this](bool on) {
        AppSettings::instance().setValue(QStringLiteral("TciAlwaysStreamIq"),
                                          on ? QStringLiteral("True") : QStringLiteral("False"));
        if (m_tciServerRef) { m_tciServerRef->refreshRemoteIqDemand(); }
    });
    form->addRow(QString(), m_alwaysStreamIqCheck);

    contentLayout()->addWidget(group);
    refreshIqStreamGroup();
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
    connect(m_txChannelCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        AppSettings::instance().setValue(QStringLiteral("TciTxChannel"), text);
        // Thetis setup.cs:37386-37394 [v2.10.3.15]: the running server
        // takes the new TX channel at once.
#ifdef HAVE_WEBSOCKETS
        if (m_tciServerRef) {
            m_tciServerRef->setTxStereoInputMode(TciServer::txStereoInputModeFromText(text));
        }
#endif
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
    // R-R3-49: the sensor intervals are not applied yet; hidden until they are.
}

// ---------------------------------------------------------------------------
// Group 6: VFO Quirks
// Controls: Forget RX2 VFO B / Use RX1 VFO A for RX2 VFO A /
//           Duplicate RX2 VFO B to RX2 VFO A.
// AppSettings: TciForgetRx2VfoBOnDisconnect, TciUseRx1VfoaForRx2Vfoa,
//              TciCopyRx2VfobToVfoa (the key names predate the port; kept).
// Defaults: TciProtocol.h kTci...Default. TciProtocol.cpp applies them.
// ---------------------------------------------------------------------------
QString CatTciServerPage::rx2VfoForgetLabel()
{
    return tr("Forget RX2 VFO B");
}

QString CatTciServerPage::rx2VfoForgetTip()
{
    return tr("While Duplicate RX2 VFO B to RX2 VFO A is on, send RX2's frequency to TCI "
              "apps only as RX2 VFO A, without its VFO B messages.");
}

QString CatTciServerPage::rx2VfoUseRx1Label()
{
    return tr("Use RX1 VFO A for RX2 VFO A");
}

QString CatTciServerPage::rx2VfoUseRx1Tip()
{
    return tr("While RX2 is on, TCI apps see RX1's frequency as RX2 VFO A, and an app "
              "that sets RX2 VFO A tunes RX1.");
}

QString CatTciServerPage::rx2VfoCopyLabel()
{
    return tr("Duplicate RX2 VFO B to RX2 VFO A");
}

QString CatTciServerPage::rx2VfoCopyTip()
{
    return tr("RX2 has one frequency, which TCI apps get as RX2 VFO B. This also sends "
              "it as RX2 VFO A, for apps that follow VFO A.");
}

void CatTciServerPage::buildVfoQuirksGroup()
{
    auto* group = new QGroupBox(tr("VFO Quirks"), this);
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
    auto* form = new QFormLayout(group);
    form->setSpacing(6);

    auto& s = AppSettings::instance();
    const auto boolText = [](bool on) {
        return on ? QStringLiteral("True") : QStringLiteral("False");
    };

    // Forget RX2 VFO B
    // From Thetis setup.designer.cs [v2.10.3.15] (chkForgetRX2VfoBVFOinfo)
    m_forgetRx2VfoBCheck = new QCheckBox(rx2VfoForgetLabel(), group);
    m_forgetRx2VfoBCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_forgetRx2VfoBCheck->setObjectName(QStringLiteral("tciForgetRx2VfoBCheck"));
    m_forgetRx2VfoBCheck->setToolTip(rx2VfoForgetTip());
    m_forgetRx2VfoBCheck->setChecked(
        s.value(QStringLiteral("TciForgetRx2VfoBOnDisconnect"),
                boolText(kTciForgetRx2VfobDefault)).toString()
        == QStringLiteral("True"));
    connect(m_forgetRx2VfoBCheck, &QCheckBox::toggled, this, [boolText](bool on) {
        AppSettings::instance().setValue(QStringLiteral("TciForgetRx2VfoBOnDisconnect"),
                                          boolText(on));
    });
    form->addRow(QString(), m_forgetRx2VfoBCheck);

    // Use RX1 VFO A for RX2 VFO A
    // From Thetis setup.designer.cs [v2.10.3.15] (chkUseRX1vfoaForRX2vfoa)
    m_useRx1VfoaForRx2Check = new QCheckBox(rx2VfoUseRx1Label(), group);
    m_useRx1VfoaForRx2Check->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_useRx1VfoaForRx2Check->setObjectName(QStringLiteral("tciUseRx1VfoaForRx2VfoaCheck"));
    m_useRx1VfoaForRx2Check->setToolTip(rx2VfoUseRx1Tip());
    m_useRx1VfoaForRx2Check->setChecked(
        s.value(QStringLiteral("TciUseRx1VfoaForRx2Vfoa"),
                boolText(kTciUseRx1VfoaForRx2VfoaDefault)).toString()
        == QStringLiteral("True"));
    connect(m_useRx1VfoaForRx2Check, &QCheckBox::toggled, this, [boolText](bool on) {
        AppSettings::instance().setValue(QStringLiteral("TciUseRx1VfoaForRx2Vfoa"),
                                          boolText(on));
    });
    form->addRow(QString(), m_useRx1VfoaForRx2Check);

    // Duplicate RX2 VFO B to RX2 VFO A
    // From Thetis setup.designer.cs [v2.10.3.15] (chkCopyRX2VFObToVFOa)
    m_copyRx2VfobToVfoaCheck = new QCheckBox(rx2VfoCopyLabel(), group);
    m_copyRx2VfobToVfoaCheck->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_copyRx2VfobToVfoaCheck->setObjectName(QStringLiteral("tciCopyRx2VfobToVfoaCheck"));
    m_copyRx2VfobToVfoaCheck->setToolTip(rx2VfoCopyTip());
    m_copyRx2VfobToVfoaCheck->setChecked(
        s.value(QStringLiteral("TciCopyRx2VfobToVfoa"),
                boolText(kTciCopyRx2VfobToVfoaDefault)).toString()
        == QStringLiteral("True"));
    // Forget works only with Duplicate: enabled only while Duplicate is on.
    // From Thetis setup.cs:22568-22572 [v2.10.3.15] (chkCopyRX2VFObToVFOa_CheckedChanged)
    m_forgetRx2VfoBCheck->setEnabled(m_copyRx2VfobToVfoaCheck->isChecked());
    connect(m_copyRx2VfobToVfoaCheck, &QCheckBox::toggled, this, [this, boolText](bool on) {
        AppSettings::instance().setValue(QStringLiteral("TciCopyRx2VfobToVfoa"), boolText(on));
        m_forgetRx2VfoBCheck->setEnabled(on);
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
//   - This computer only (127.0.0.1)                     ← default
//   - Any IPv4 address (0.0.0.0), open to your network
//   - <detected non-loopback IPv4 NICs>
//   - This computer only, IPv6 (::1)
//   - Any IPv6 address (::), open to your network
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
        tr("This computer only (127.0.0.1)"),
        QStringLiteral("127.0.0.1"));
    m_bindAddressCombo->addItem(
        tr("Any IPv4 address (0.0.0.0), open to your network"),
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
                const QString label = QStringLiteral("%1 (%2)")
                    .arg(iface.name(), ip.toString());
                m_bindAddressCombo->addItem(label, ip.toString());
            }
        }
    }

    // IPv6 well-known options.
    m_bindAddressCombo->addItem(
        tr("This computer only, IPv6 (::1)"),
        QStringLiteral("::1"));
    m_bindAddressCombo->addItem(
        tr("Any IPv6 address (::), open to your network"),
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
                const QString label = QStringLiteral("%1 (%2)")
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
                tr("This window's server (%1 %2)")
                    .arg(m_tciClientCount)
                    .arg(m_tciClientCount == 1 ? tr("client") : tr("clients")));
        } else {
            m_serverGroup->setTitle(tr("This window's server"));
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
    m_enableCheck->setToolTip(QStringLiteral("Turn on MIDI control"));
    grid->addWidget(m_enableCheck, 0, 0, 1, 2);

    // Device combo
    auto* devLabel = new QLabel(QStringLiteral("Device:"), group);
    devLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    grid->addWidget(devLabel, 1, 0);

    m_deviceCombo = new QComboBox(group);
    m_deviceCombo->setStyleSheet(QString::fromLatin1(Style::kComboStyle));
    m_deviceCombo->addItem(QStringLiteral("(no MIDI devices found)"));
    m_deviceCombo->setDisabled(true);
    m_deviceCombo->setToolTip(QStringLiteral("The MIDI device to use"));
    grid->addWidget(m_deviceCombo, 1, 1);

    // Mapping table placeholder label
    m_mappingLabel = new QLabel(
        QStringLiteral("The MIDI mapping table is not available in this version"), group);
    m_mappingLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    m_mappingLabel->setAlignment(Qt::AlignCenter);
    m_mappingLabel->setMinimumHeight(80);
    grid->addWidget(m_mappingLabel, 2, 0, 1, 2);

    // Learn button
    m_learnButton = new QPushButton(QStringLiteral("Learn..."), group);
    m_learnButton->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    m_learnButton->setDisabled(true);
    m_learnButton->setToolTip(QStringLiteral("Learn a control: move it on the MIDI device to assign it"));
    grid->addWidget(m_learnButton, 3, 0, 1, 2);

    contentLayout()->addWidget(group);
    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// PeripheralsPage
// Phase 3P-II Task 17: Setup > Network > Peripherals.
// Two device rows (TGXL, PGXL) with six columns each:
//   Name | Host IP | Port | Scan LAN | Connect | Status
//
// Per-radio peripherals refactor (2026-05-26): the four connection keys
// (TGXL_ManualIp / TGXL_ManualPort / PGXL_ManualIp / PGXL_ManualPort) now
// scope under hardware/<mac>/peripherals/ via RadioModel::peripheralValue /
// setPeripheralValue.  Reads return defaults when no radio is connected;
// writes log a qCWarning and are a no-op.  The page grays itself out
// in that state so the operator can't reach a dead write path.
// ---------------------------------------------------------------------------

PeripheralsPage::PeripheralsPage(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    setStyleSheet(QString::fromLatin1(Style::kPageStyle));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(10);

    // ── Group box: Peripherals ────────────────────────────────────────────────
    auto* group = new QGroupBox(tr("Peripherals"), this);
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));

    m_grid = new QGridLayout(group);
    m_grid->setSpacing(8);
    m_grid->setContentsMargins(10, 16, 10, 10);

    // Column header labels (row 0).
    static const char* kHeaders[] = {
        "Device", "Host IP", "Port", "Scan", "Action", "Status"
    };
    for (int col = 0; col < 6; ++col) {
        auto* hdr = new QLabel(tr(kHeaders[col]), group);
        hdr->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
        m_grid->addWidget(hdr, 0, col);
    }

    // Column width hints: generous so long "Connected to 192.168.1.42:9010" fits.
    m_grid->setColumnMinimumWidth(0, 160);  // Device name
    m_grid->setColumnMinimumWidth(1, 180);  // Host IP
    m_grid->setColumnMinimumWidth(2,  80);  // Port
    m_grid->setColumnMinimumWidth(3,  80);  // Scan LAN
    m_grid->setColumnMinimumWidth(4, 100);  // Connect/Disconnect
    m_grid->setColumnMinimumWidth(5, 280);  // Status
    m_grid->setColumnStretch(5, 1);         // Status expands with window width

    // Reserve space for 2 device rows (added by buildRow below).
    m_statusLabels.resize(2);
    m_connectBtns.resize(2);

    // Row 1: TGXL (Tuner Genius XL, port 9010)
    buildRow(1,
             QStringLiteral("Tuner Genius XL"),
             QStringLiteral("TGXL_ManualIp"),
             QStringLiteral("TGXL_ManualPort"),
             9010);

    // Row 2: PGXL (Power Genius XL, port 9008)
    buildRow(2,
             QStringLiteral("Power Genius XL"),
             QStringLiteral("PGXL_ManualIp"),
             QStringLiteral("PGXL_ManualPort"),
             9008);

    outer->addWidget(group);

    // Footnote text (from design spec §5.3).
    auto* note = new QLabel(
        tr("Configure peripherals on your local network. Devices auto-connect on app"
           " launch if a Host is configured. The Scan LAN button passively listens for"
           " 4O3A device announcements on UDP 9008 / 9010 for 3 seconds."),
        this);
    note->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    note->setWordWrap(true);
    outer->addWidget(note);

    outer->addStretch();

    // Phase 3P-II Task 63: connect live status signals from PGXL and TGXL.
    wireStatusSignals();
}

PeripheralsPage::~PeripheralsPage()
{
    // R-R3-49 (parity Task 8): an edit still unsent when the page goes.
    sendRemoteTgxlAddress();
    // R-R3-49 (parity Task 9): and the Power Genius's.
    sendRemotePgxlAddress();
}

void PeripheralsPage::hideEvent(QHideEvent* event)
{
    sendRemoteTgxlAddress();
    sendRemotePgxlAddress();
    QWidget::hideEvent(event);
}

// R-R3-49 (parity Task 9): the Power Genius row's Host and Port typed in a
// remote window reach the Core's saved address without dialling
// (setPgxlAddress). A Core below remotePgxlControlVersion 4 keeps only what
// Connect sends, as before.
void PeripheralsPage::sendRemotePgxlAddress()
{
    if (!m_pgxlAddressEdited || !isRemoteMode() || !m_grid || !m_model) {
        return;
    }
    IStationLink* link = m_model->stationLink();
    auto* amp = m_model->amplifierModel();
    QLayoutItem* hostItem = m_grid->itemAtPosition(2, 1);
    QLayoutItem* portItem = m_grid->itemAtPosition(2, 2);
    auto* ipEdit = hostItem ? qobject_cast<QLineEdit*>(hostItem->widget()) : nullptr;
    auto* portSpin = portItem ? qobject_cast<QSpinBox*>(portItem->widget()) : nullptr;
    if (!link || !link->pgxlFullControlAvailable() || !amp || !ipEdit || !portSpin) {
        return;
    }
    m_pgxlAddressEdited = false;
    const QString host = ipEdit->text().trimmed();
    const int port = portSpin->value();
    if (host == amp->configuredHost() && port == amp->configuredPort()) {
        return;
    }
    // A refusal arrives as the Core's notice (MainWindow's accessory route).
    const IStationLink::CommandOutcome outcome = link->requestPgxlAddress(host, port);
    if (!outcome.sent && m_statusLabels.size() > 1 && m_statusLabels[1]) {
        m_statusLabels[1]->setText(OperatorReasonText::forDisplay(outcome.reason));
    }
}

// R-R3-49 (parity Task 8): the Host and Port typed in a remote window reach
// the Core's saved address without dialling (setTgxlAddress). A Core below
// remoteTgxlControlVersion 4 keeps only what Connect sends, as before.
void PeripheralsPage::sendRemoteTgxlAddress()
{
    if (!m_tgxlAddressEdited || !isRemoteMode() || !m_grid || !m_model) {
        return;
    }
    IStationLink* link = m_model->stationLink();
    auto* tuner = m_model->tunerModel();
    QLayoutItem* hostItem = m_grid->itemAtPosition(1, 1);
    QLayoutItem* portItem = m_grid->itemAtPosition(1, 2);
    auto* ipEdit = hostItem ? qobject_cast<QLineEdit*>(hostItem->widget()) : nullptr;
    auto* portSpin = portItem ? qobject_cast<QSpinBox*>(portItem->widget()) : nullptr;
    if (!link || !link->tgxlFullControlAvailable() || !tuner || !ipEdit || !portSpin) {
        return;
    }
    m_tgxlAddressEdited = false;
    const QString host = ipEdit->text().trimmed();
    const int port = portSpin->value();
    if (host == tuner->configuredHost() && port == tuner->configuredPort()) {
        return;
    }
    // A refusal arrives as the Core's notice (MainWindow's accessory route).
    const IStationLink::CommandOutcome outcome = link->requestTgxlAddress(host, port);
    if (!outcome.sent && m_statusLabels.size() > 0 && m_statusLabels[0]) {
        m_statusLabels[0]->setText(OperatorReasonText::forDisplay(outcome.reason));
    }
}

void PeripheralsPage::wireStatusSignals()
{
    if (isRemoteMode()) {
        // Remote mode only projects Core-owned state. It must not subscribe
        // to, scan for, or dial a Mac-local TGXL/PGXL socket.
        auto* tuner = m_model ? m_model->tunerModel() : nullptr;
        if (tuner) {
            connect(tuner, &TunerModel::stationConnectionChanged,
                    this, &PeripheralsPage::refreshRemoteTgxlRow);
        }
        auto* amp = m_model ? m_model->amplifierModel() : nullptr;
        if (amp) {
            connect(amp, &AmplifierModel::stationConnectionChanged,
                    this, &PeripheralsPage::refreshRemotePgxlRow);
        }
        if (m_model) {
            connect(m_model, &RadioModel::connectionStateChanged,
                    this, &PeripheralsPage::refreshRemoteTgxlRow);
            connect(m_model, &RadioModel::stationLinkStateChanged,
                    this, &PeripheralsPage::refreshRemoteTgxlRow);
            connect(m_model, &RadioModel::connectionStateChanged,
                    this, &PeripheralsPage::refreshRemotePgxlRow);
            connect(m_model, &RadioModel::stationLinkStateChanged,
                    this, &PeripheralsPage::refreshRemotePgxlRow);
        }
        refreshRemoteTgxlRow();
        refreshRemotePgxlRow();
        return;
    }
    // Row index map: 0 = TGXL, 1 = PGXL (matches buildRow call order above).
    // m_statusLabels and m_connectBtns are sized to 2 before this runs.

    // --- PGXL (row 1) ---
    PgxlConnection* pgxl      = m_model->pgxlConnection();
    QLabel*         pgxlLabel = m_statusLabels[1];
    QPushButton*    pgxlBtn   = m_connectBtns[1];

    connect(pgxl, &PgxlConnection::connected, this,
            [pgxlLabel, pgxlBtn]() {
                pgxlLabel->setText(QObject::tr("Connected (pairing...)"));
                pgxlBtn->setText(QObject::tr("Disconnect"));
            });

    connect(pgxl, &PgxlConnection::disconnected, this,
            [pgxlLabel, pgxlBtn]() {
                pgxlLabel->setText(QObject::tr("Disconnected"));
                pgxlBtn->setText(QObject::tr("Connect"));
            });

    connect(pgxl, &PgxlConnection::connectionFailed, this,
            [pgxlLabel, pgxlBtn](const QString& msg) {
                pgxlLabel->setText(QObject::tr("Error: %1").arg(msg));
                pgxlBtn->setText(QObject::tr("Connect"));
            });

    connect(pgxl, &PgxlConnection::pairingResult, this,
            [pgxlLabel](bool succeeded, const QString& detail) {
                if (succeeded) {
                    pgxlLabel->setText(QObject::tr("Connected, paired"));
                } else {
                    pgxlLabel->setText(
                        QObject::tr("Connected (pairing failed: %1)").arg(detail));
                }
            });

    // Restore status label + button label to match current connection
    // state on page open. The connected/disconnected signals only fire
    // on transition; if PGXL already connected before the Setup dialog
    // was opened, the wires above don't fire and the label sits at its
    // initial "Disconnected" text. Bench-confirmed dead-end 2026-05-20.
    if (pgxl->isConnected()) {
        pgxlBtn->setText(QObject::tr("Disconnect"));
        pgxlLabel->setText(QObject::tr("Connected"));
    }

    // --- TGXL (row 0) ---
    TgxlConnection* tgxl      = m_model->tgxlConnection();
    QLabel*         tgxlLabel = m_statusLabels[0];
    QPushButton*    tgxlBtn   = m_connectBtns[0];

    connect(tgxl, &TgxlConnection::connected, this,
            [tgxlLabel, tgxlBtn]() {
                tgxlLabel->setText(QObject::tr("Connected"));
                tgxlBtn->setText(QObject::tr("Disconnect"));
            });

    connect(tgxl, &TgxlConnection::disconnected, this,
            [tgxlLabel, tgxlBtn]() {
                tgxlLabel->setText(QObject::tr("Disconnected"));
                tgxlBtn->setText(QObject::tr("Connect"));
            });

    connect(tgxl, &TgxlConnection::connectionFailed, this,
            [tgxlLabel, tgxlBtn](const QString& msg) {
                tgxlLabel->setText(QObject::tr("Error: %1").arg(msg));
                tgxlBtn->setText(QObject::tr("Connect"));
            });

    // Same dead-end fix as PGXL above: status label needs an explicit
    // refresh on page open since the connected signal only fires on
    // transition, not on subscription.
    if (tgxl->isConnected()) {
        tgxlBtn->setText(QObject::tr("Disconnect"));
        tgxlLabel->setText(QObject::tr("Connected"));
    }
}

void PeripheralsPage::buildRow(int row, const QString& name,
                               const QString& ipKey, const QString& portKey,
                               quint16 defaultPort)
{
    // Map grid row to m_statusLabels / m_connectBtns index (row 1 -> 0, row 2 -> 1).
    const int idx = row - 1;

    // Column 0: device name label.
    auto* nameLabel = new QLabel(name, this);
    nameLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    m_grid->addWidget(nameLabel, row, 0);

    // Per-radio peripherals refactor (2026-05-26): host/port keys are
    // scoped under hardware/<mac>/peripherals/ via the RadioModel
    // helpers.  When no radio is connected the reads return defaults
    // and the writes are a no-op + qCWarning.
    RadioModel* model = m_model;

    // Column 1: host IP line edit.
    auto* ipEdit = new QLineEdit(this);
    ipEdit->setStyleSheet(QString::fromLatin1(Style::kLineEditStyle));
    ipEdit->setPlaceholderText(QStringLiteral("192.168.1.42"));
    ipEdit->setToolTip(tr("IP address or hostname of the %1 on your LAN. "
                          "Leave blank to disable auto-connect.").arg(name));
    const bool remote = model && model->role() == RadioModel::Role::Remote;
    const bool remoteTgxl = remote && idx == 0;
    const bool remotePgxl = remote && idx == 1;
    const auto* tuner = model ? model->tunerModel() : nullptr;
    const auto* amp = model ? model->amplifierModel() : nullptr;
    const QString savedIp = remoteTgxl && tuner
        ? tuner->configuredHost()
        : remotePgxl && amp
        ? amp->configuredHost()
        : model
        ? model->peripheralValue(ipKey)
        : QString{};
    ipEdit->setText(savedIp);
    ipEdit->setObjectName(idx == 0
                              ? QStringLiteral("tgxlHostEdit")
                              : QStringLiteral("pgxlHostEdit"));
    if (remoteTgxl) {
        m_lastDisplayedCoreTgxlHost = savedIp;
    }
    if (remotePgxl) {
        m_lastDisplayedCorePgxlHost = savedIp;
    }
    connect(ipEdit, &QLineEdit::textChanged, this,
            [model, ipKey](const QString& text) {
                if (model && model->role() != RadioModel::Role::Remote) {
                    model->setPeripheralValue(ipKey, text);
                }
            });
    // R-R3-49 (parity Task 8): in a remote window a typed Host reaches the
    // Core when editing finishes (or Setup closes), without dialling.
    if (remoteTgxl) {
        connect(ipEdit, &QLineEdit::textEdited, this, [this]() {
            m_tgxlAddressEdited = true;
        });
        connect(ipEdit, &QLineEdit::editingFinished, this,
                &PeripheralsPage::sendRemoteTgxlAddress);
    }
    // R-R3-49 (parity Task 9): the same for the Power Genius row.
    if (remotePgxl) {
        connect(ipEdit, &QLineEdit::textEdited, this, [this]() {
            m_pgxlAddressEdited = true;
        });
        connect(ipEdit, &QLineEdit::editingFinished, this,
                &PeripheralsPage::sendRemotePgxlAddress);
    }
    m_grid->addWidget(ipEdit, row, 1);

    // Column 2: port spinbox.
    auto* portSpin = new QSpinBox(this);
    portSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
    portSpin->setRange(1, 65535);
    portSpin->setToolTip(tr("TCP port the %1 listens on (default %2).")
                             .arg(name).arg(defaultPort));
    const int savedPort = remoteTgxl && tuner && tuner->configuredPort() > 0
        ? tuner->configuredPort()
        : remotePgxl && amp && amp->configuredPort() > 0
        ? amp->configuredPort()
        : model
        ? model->peripheralValue(portKey,
                                 QString::number(static_cast<int>(defaultPort))).toInt()
        : static_cast<int>(defaultPort);
    portSpin->setValue(savedPort);
    portSpin->setObjectName(idx == 0
                                ? QStringLiteral("tgxlPortSpin")
                                : QStringLiteral("pgxlPortSpin"));
    if (remoteTgxl) {
        m_lastDisplayedCoreTgxlPort = tuner ? tuner->configuredPort() : 0;
    }
    if (remotePgxl) {
        m_lastDisplayedCorePgxlPort = amp ? static_cast<quint16>(amp->configuredPort()) : 0;
    }
    connect(portSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [model, portKey](int v) {
                if (model && model->role() != RadioModel::Role::Remote) {
                    model->setPeripheralValue(portKey, QString::number(v));
                }
            });
    if (remoteTgxl) {
        connect(portSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() {
            if (!m_fillingTgxlFromCore) {
                m_tgxlAddressEdited = true;
            }
        });
        connect(portSpin, &QSpinBox::editingFinished, this,
                &PeripheralsPage::sendRemoteTgxlAddress);
    }
    if (remotePgxl) {
        connect(portSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this]() {
            if (!m_fillingPgxlFromCore) {
                m_pgxlAddressEdited = true;
            }
        });
        connect(portSpin, &QSpinBox::editingFinished, this,
                &PeripheralsPage::sendRemotePgxlAddress);
    }
    m_grid->addWidget(portSpin, row, 2);

    // Column 3: Scan LAN button.
    auto* scanBtn = new QPushButton(tr("Scan LAN"), this);
    scanBtn->setObjectName(idx == 0
                               ? QStringLiteral("tgxlScanButton")
                               : QStringLiteral("pgxlScanButton"));
    scanBtn->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    scanBtn->setToolTip(tr("Listen for %1 announcements on the LAN for 3 seconds.").arg(name));
    // Capture idx by value for the slot dispatch.
    connect(scanBtn, &QPushButton::clicked, this, [this, idx]() {
        onScanLan(idx);
    });
    m_grid->addWidget(scanBtn, row, 3);

    // Column 4: Connect / Disconnect button.
    auto* connectBtn = new QPushButton(tr("Connect"), this);
    connectBtn->setObjectName(idx == 0
                                  ? QStringLiteral("tgxlConnectButton")
                                  : QStringLiteral("pgxlConnectButton"));
    connectBtn->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    connectBtn->setToolTip(tr("Connect to or disconnect from the %1.").arg(name));
    m_connectBtns[idx] = connectBtn;
    connect(connectBtn, &QPushButton::clicked, this, [this, idx]() {
        onConnect(idx);
    });
    m_grid->addWidget(connectBtn, row, 4);

    // Column 5: status label.
    auto* statusLabel = new QLabel(tr("Disconnected"), this);
    statusLabel->setObjectName(idx == 0
                                   ? QStringLiteral("tgxlStatusLabel")
                                   : QStringLiteral("pgxlStatusLabel"));
    statusLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    m_statusLabels[idx] = statusLabel;
    m_grid->addWidget(statusLabel, row, 5);

    // Setup description version 15: this row's ids. The one button is
    // described as Connect and Disconnect (its caption follows the phase).
    const QString id = QStringLiteral("catNetwork.fourO3A.")
        + (idx == 0 ? QStringLiteral("tgxl") : QStringLiteral("pgxl"));
    ipEdit->setProperty("nereusSetupId", id + QStringLiteral("Host"));
    portSpin->setProperty("nereusSetupId", id + QStringLiteral("Port"));
    connectBtn->setProperty("nereusSetupIds", QStringList{id + QStringLiteral("Connect"),
                                                          id + QStringLiteral("Disconnect")});
    statusLabel->setProperty("nereusSetupId", id + QStringLiteral("Status"));
}

bool PeripheralsPage::isRemoteMode() const
{
    return m_model && m_model->role() == RadioModel::Role::Remote;
}

void PeripheralsPage::refreshRemoteTgxlRow()
{
    if (!isRemoteMode() || !m_grid || m_statusLabels.size() < 2) {
        return;
    }
    auto* tuner = m_model->tunerModel();
    auto* link = m_model->stationLink();
    auto* ipEdit = qobject_cast<QLineEdit*>(m_grid->itemAtPosition(1, 1)->widget());
    auto* portSpin = qobject_cast<QSpinBox*>(m_grid->itemAtPosition(1, 2)->widget());
    auto* scanButton = qobject_cast<QPushButton*>(m_grid->itemAtPosition(1, 3)->widget());
    auto* connectButton = m_connectBtns[0];
    auto* status = m_statusLabels[0];
    if (!tuner || !ipEdit || !portSpin || !scanButton || !connectButton || !status) {
        return;
    }
    const bool available = link && link->remoteTgxlConfigAvailable();
    // R-R3-49 (parity Task 8): on a Core at remoteTgxlControlVersion 4 the
    // Core scans its own network for this window, and keeps a typed
    // address. Parity mini-round (the operator's rulings a and b): both
    // only listen or save, so neither waits on the air, as in a local
    // window.
    const bool full = link && link->tgxlFullControlAvailable();
    scanButton->setEnabled(full);
    scanButton->setToolTip(!full
        ? tr("This Core does not scan for a Tuner Genius for this app. Updating the Core may help.")
        : tr("The Core listens for Tuner Genius announcements on its network for 3 seconds."));
    const QString coreHost = tuner->configuredHost();
    const quint16 corePort = static_cast<quint16>(tuner->configuredPort());
    if (coreHost != m_lastDisplayedCoreTgxlHost
        || corePort != m_lastDisplayedCoreTgxlPort) {
        m_fillingTgxlFromCore = true;
        ipEdit->setText(coreHost);
        if (corePort > 0) {
            portSpin->setValue(corePort);
        }
        m_fillingTgxlFromCore = false;
        m_tgxlAddressEdited = false;
        m_lastDisplayedCoreTgxlHost = coreHost;
        m_lastDisplayedCoreTgxlPort = corePort;
    }
    const auto phase = tuner->connectionPhase();
    const bool active = phase == TunerModel::ConnectionPhase::Discovering
        || phase == TunerModel::ConnectionPhase::Connecting
        || phase == TunerModel::ConnectionPhase::Identifying
        || phase == TunerModel::ConnectionPhase::Retrying;
    const bool connected = phase == TunerModel::ConnectionPhase::Connected;
    connectButton->setText(connected ? tr("Disconnect") : active ? tr("Cancel") : tr("Connect"));
    connectButton->setEnabled(available);
    ipEdit->setEnabled(available && !connected && !active);
    portSpin->setEnabled(available && !connected && !active);
    ipEdit->setToolTip(tr("IP address or hostname of the Tuner Genius XL on your LAN. "
                          "Leave blank to disable auto-connect."));
    portSpin->setToolTip(tr("TCP port the Tuner Genius XL listens on (default 9010)."));
    if (!available) {
        const QString reason = tr("This Core does not offer Tuner Genius XL control to this app.");
        status->setText(reason);
        connectButton->setToolTip(reason);
        return;
    }
    // The Core's own reason, shown in user words; the raw reason is logged.
    const QString error = tuner->connectionError().isEmpty()
        ? QString() : OperatorReasonText::forDisplay(tuner->connectionError());
    QString text;
    switch (phase) {
    case TunerModel::ConnectionPhase::Disabled: text = tr("Disabled at the Core"); break;
    case TunerModel::ConnectionPhase::Disconnected: text = tr("Disconnected"); break;
    case TunerModel::ConnectionPhase::Discovering: text = tr("Discovering at the Core"); break;
    case TunerModel::ConnectionPhase::Connecting: text = tr("Connecting at the Core"); break;
    case TunerModel::ConnectionPhase::Identifying: text = tr("Identifying device"); break;
    case TunerModel::ConnectionPhase::Retrying:
        text = error.isEmpty()
            ? tr("Retrying at the Core")
            : tr("Retrying at the Core: %1").arg(error);
        break;
    case TunerModel::ConnectionPhase::Connected:
        text = tr("Connected: %1 %2").arg(tuner->deviceModel(), tuner->deviceSerial()); break;
    case TunerModel::ConnectionPhase::Error:
        text = tr("Error: %1").arg(OperatorReasonText::forDisplay(tuner->connectionError()));
        break;
    }
    status->setText(text);
    connectButton->setToolTip(QString());
}

void PeripheralsPage::refreshRemotePgxlRow()
{
    if (!isRemoteMode() || !m_grid || m_statusLabels.size() < 2) {
        return;
    }
    auto* amp = m_model->amplifierModel();
    auto* link = m_model->stationLink();
    auto* ipEdit = qobject_cast<QLineEdit*>(m_grid->itemAtPosition(2, 1)->widget());
    auto* portSpin = qobject_cast<QSpinBox*>(m_grid->itemAtPosition(2, 2)->widget());
    auto* scanButton = qobject_cast<QPushButton*>(m_grid->itemAtPosition(2, 3)->widget());
    auto* connectButton = m_connectBtns[1];
    auto* status = m_statusLabels[1];
    if (!amp || !ipEdit || !portSpin || !scanButton || !connectButton || !status) {
        return;
    }
    const bool available = link && link->remotePgxlControlAvailable();
    // R-R3-49 (parity Task 9): on a Core at remotePgxlControlVersion 4 the
    // Core scans its own network for this window, and keeps a typed
    // address. Parity mini-round (rulings a and b): neither waits on the
    // air, as in a local window.
    const bool full = link && link->pgxlFullControlAvailable();
    scanButton->setEnabled(full);
    scanButton->setToolTip(!full
        ? tr("This Core does not scan for a Power Genius for this app. Updating the Core may help.")
        : tr("The Core listens for Power Genius announcements on its network for 3 seconds."));
    // The Core's address fills the fields only when it changes, so an
    // unsent draft survives a phase or error update.
    const QString coreHost = amp->configuredHost();
    const quint16 corePort = static_cast<quint16>(amp->configuredPort());
    if (coreHost != m_lastDisplayedCorePgxlHost || corePort != m_lastDisplayedCorePgxlPort) {
        m_fillingPgxlFromCore = true;
        ipEdit->setText(coreHost);
        if (corePort > 0) {
            portSpin->setValue(corePort);
        }
        m_fillingPgxlFromCore = false;
        m_pgxlAddressEdited = false;
        m_lastDisplayedCorePgxlHost = coreHost;
        m_lastDisplayedCorePgxlPort = corePort;
    }
    using Phase = AmplifierModel::ConnectionPhase;
    const auto phase = amp->connectionPhase();
    const bool active = phase == Phase::Discovering || phase == Phase::Connecting
        || phase == Phase::Identifying || phase == Phase::Retrying;
    const bool connected = phase == Phase::Connected;
    connectButton->setText(connected ? tr("Disconnect") : active ? tr("Cancel") : tr("Connect"));
    connectButton->setEnabled(available);
    ipEdit->setEnabled(available && !connected && !active);
    portSpin->setEnabled(available && !connected && !active);
    ipEdit->setToolTip(tr("IP address or hostname of the Power Genius XL on your LAN. "
                          "Leave blank to disable auto-connect."));
    portSpin->setToolTip(tr("TCP port the Power Genius XL listens on (default 9008)."));
    if (!available) {
        const QString reason = tr("This Core does not offer Power Genius XL control to this app.");
        status->setText(reason);
        connectButton->setToolTip(reason);
        return;
    }
    // The Core's own reason, shown in user words; the raw reason is logged.
    const QString error = amp->connectionError().isEmpty()
        ? QString() : OperatorReasonText::forDisplay(amp->connectionError());
    QString text;
    switch (phase) {
    case Phase::Disabled: text = tr("Disabled at the Core"); break;
    case Phase::Disconnected: text = tr("Disconnected"); break;
    case Phase::Discovering: text = tr("Discovering at the Core"); break;
    case Phase::Connecting: text = tr("Connecting at the Core"); break;
    case Phase::Identifying: text = tr("Identifying device"); break;
    case Phase::Retrying:
        text = error.isEmpty() ? tr("Retrying at the Core")
                               : tr("Retrying at the Core: %1").arg(error);
        break;
    case Phase::Connected:
        text = tr("Connected: %1 %2").arg(amp->deviceModel(), amp->deviceSerial()); break;
    case Phase::Error:
        text = tr("Error: %1").arg(OperatorReasonText::forDisplay(amp->connectionError()));
        break;
    }
    status->setText(text);
    connectButton->setToolTip(QString());
}

void PeripheralsPage::onScanLan(int rowIdx)
{
    if (isRemoteMode()) {
        // R-R3-49 (parity Task 8): the Tuner Genius row asks the Core to
        // listen on its own network; a pick fills Host and Port and is kept
        // on the Core as a typed address is. R-R3-49 (parity Task 9): the
        // Power Genius row does the same (scanPgxlLan, setPgxlAddress).
        IStationLink* link = m_model ? m_model->stationLink() : nullptr;
        if (rowIdx == 1) {
            if (!link || !link->pgxlFullControlAvailable()) {
                return;
            }
            auto* pgxlIp = qobject_cast<QLineEdit*>(m_grid->itemAtPosition(2, 1)->widget());
            auto* pgxlPort = qobject_cast<QSpinBox*>(m_grid->itemAtPosition(2, 2)->widget());
            if (!pgxlIp || !pgxlPort) {
                return;
            }
            auto* pgxlDialog = new LanScanDialog(m_model, this,
                                                 LanScanDialog::CoreDevice::PowerGenius);
            pgxlDialog->setAttribute(Qt::WA_DeleteOnClose);
            pgxlDialog->setObjectName(QStringLiteral("pgxlCoreScanDialog"));
            connect(pgxlDialog, &LanScanDialog::deviceSelected,
                    this, [this, pgxlIp, pgxlPort](const QString& ip, quint16 port) {
                        pgxlIp->setText(ip);
                        pgxlPort->setValue(static_cast<int>(port));
                        m_pgxlAddressEdited = true;
                        sendRemotePgxlAddress();
                    });
            pgxlDialog->show();
            return;
        }
        if (rowIdx != 0 || !link || !link->tgxlFullControlAvailable()) {
            return;
        }
        auto* ipEdit = qobject_cast<QLineEdit*>(m_grid->itemAtPosition(1, 1)->widget());
        auto* portSpin = qobject_cast<QSpinBox*>(m_grid->itemAtPosition(1, 2)->widget());
        if (!ipEdit || !portSpin) {
            return;
        }
        auto* dialog = new LanScanDialog(m_model, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setObjectName(QStringLiteral("tgxlCoreScanDialog"));
        connect(dialog, &LanScanDialog::deviceSelected,
                this, [this, ipEdit, portSpin](const QString& ip, quint16 port) {
                    ipEdit->setText(ip);
                    portSpin->setValue(static_cast<int>(port));
                    m_tgxlAddressEdited = true;
                    sendRemoteTgxlAddress();
                });
        dialog->show();
        return;
    }
    // rowIdx is 0-based (0 = TGXL, 1 = PGXL). Grid row = rowIdx + 1 because
    // row 0 is the header. Column 1 = IP edit, column 2 = port spin.
    const int gridRow = rowIdx + 1;

    auto* ipEdit   = qobject_cast<QLineEdit*>(
                         m_grid->itemAtPosition(gridRow, 1)->widget());
    auto* portSpin = qobject_cast<QSpinBox*>(
                         m_grid->itemAtPosition(gridRow, 2)->widget());

    if (!ipEdit || !portSpin) {
        qCWarning(lcPeripherals) << "onScanLan: could not find row widgets for rowIdx"
                                 << rowIdx;
        return;
    }

    auto* dialog = new LanScanDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    // On double-click: fill the row's IP edit and port spin, then persist.
    connect(dialog, &LanScanDialog::deviceSelected,
            this, [ipEdit, portSpin](const QString& ip, quint16 port) {
                ipEdit->setText(ip);
                portSpin->setValue(static_cast<int>(port));
            });

    dialog->show();
}

void PeripheralsPage::onConnect(int rowIdx)
{
    // rowIdx 0 = TGXL (port 9010), rowIdx 1 = PGXL (port 9008).
    // Grid row = rowIdx + 1 (row 0 is the header). Column 1 = IP edit, column 2 = port spin.
    if (!m_model) {
        qCWarning(lcPeripherals) << "onConnect: m_model is null";
        return;
    }

    const int gridRow = rowIdx + 1;
    auto* ipEdit = qobject_cast<QLineEdit*>(m_grid->itemAtPosition(gridRow, 1)->widget());
    auto* portSpin = qobject_cast<QSpinBox*>(m_grid->itemAtPosition(gridRow, 2)->widget());
    if (!ipEdit || !portSpin) {
        qCWarning(lcPeripherals) << "onConnect: could not find row widgets for rowIdx" << rowIdx;
        return;
    }
    const QString host = ipEdit->text().trimmed();
    const quint16 port = static_cast<quint16>(portSpin->value());

    if (isRemoteMode() && rowIdx == 1) {
        // R-R3-47: the Core connects, identifies and pairs its Power
        // Genius; this window only asks.
        auto* link = m_model->stationLink();
        if (!link || !link->remotePgxlControlAvailable()) {
            refreshRemotePgxlRow();
            return;
        }
        using Phase = AmplifierModel::ConnectionPhase;
        const auto phase = m_model->amplifierModel()->connectionPhase();
        const bool active = phase == Phase::Connected || phase == Phase::Discovering
            || phase == Phase::Connecting || phase == Phase::Identifying
            || phase == Phase::Retrying;
        const auto outcome = active ? link->requestDisconnectPgxl()
                                    : link->requestConfigurePgxl(host, port);
        if (!outcome.sent && m_statusLabels.size() > 1) {
            m_statusLabels[1]->setText(OperatorReasonText::forDisplay(outcome.reason));
        }
        return;
    }
    if (isRemoteMode()) {
        if (rowIdx != 0) { return; }
        auto* link = m_model->stationLink();
        if (!link || !link->remoteTgxlConfigAvailable()) {
            refreshRemoteTgxlRow();
            return;
        }
        auto* tuner = m_model->tunerModel();
        const bool active = tuner && (tuner->connectionPhase() == TunerModel::ConnectionPhase::Connected
            || tuner->connectionPhase() == TunerModel::ConnectionPhase::Discovering
            || tuner->connectionPhase() == TunerModel::ConnectionPhase::Connecting
            || tuner->connectionPhase() == TunerModel::ConnectionPhase::Identifying
            || tuner->connectionPhase() == TunerModel::ConnectionPhase::Retrying);
        const auto outcome = active ? link->requestDisconnectTgxl()
            : link->requestConfigureTgxl(host, port);
        if (!outcome.sent && !m_statusLabels.isEmpty()) {
            // The reason comes from the station link; shown in user words,
            // logged raw (R-R3-21).
            m_statusLabels[0]->setText(OperatorReasonText::forDisplay(outcome.reason));
        }
        return;
    }

    if (rowIdx == 1) {
        // PGXL row.
        PgxlConnection* pgxl = m_model->pgxlConnection();
        if (!pgxl) {
            qCWarning(lcPeripherals) << "onConnect: pgxlConnection() returned null";
            return;
        }
        if (pgxl->isConnected()) {
            pgxl->disconnect();
        } else {
            if (host.isEmpty() || port == 0) {
                qCWarning(lcPeripherals)
                    << "onConnect(PGXL): host or port not set; fill in the fields first";
                return;
            }
            pgxl->connectToPgxl(host, port);
        }
    } else {
        // TGXL row (rowIdx == 0).
        TgxlConnection* tgxl = m_model->tgxlConnection();
        if (!tgxl) {
            qCWarning(lcPeripherals) << "onConnect: tgxlConnection() returned null";
            return;
        }
        if (tgxl->isConnected()) {
            tgxl->disconnect();
        } else {
            if (host.isEmpty() || port == 0) {
                qCWarning(lcPeripherals)
                    << "onConnect(TGXL): host or port not set; fill in the fields first";
                return;
            }
            tgxl->connectToTgxl(host, port);
        }
    }
}

} // namespace NereusSDR
