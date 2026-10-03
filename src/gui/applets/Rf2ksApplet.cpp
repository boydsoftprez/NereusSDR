// =================================================================
// src/gui/applets/Rf2ksApplet.cpp  (NereusSDR-native)
// =================================================================
//   2026-05-24  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
//   Layout patterns from src/gui/applets/AmpApplet.{h,cpp} (which is
//   an AetherSDR port). The RF-Kit-specific content is original.
//   2026-09-23  R-R3-21: on a remote-station model OPERATE, the antenna
//   buttons and Disconnect/Reconnect are disabled with the amplifier
//   reason AmpApplet gives; they drive this computer's own RF2K-S
//   connection. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-23  R-R3-47 / R-R3-22: the status dot, name and version,
//   OPERATE state, gauges and telemetry strip read the RadioModel's
//   RfKitModel in local and remote windows; a remote window shows a stale
//   line when it loses the Core. J.J. Boyd (KG4VCF), AI-assisted via
//   Anthropic Claude Code.
//   2026-09-24  R-R3-47 / R-R3-48: the tuner and antenna rows read the
//   RfKitModel too; the band-follow line; a remote window's
//   Disconnect/Reconnect asks the Core. J.J. Boyd (KG4VCF), AI-assisted
//   via Anthropic Claude Code.
//   2026-09-24  R-R3-47 / R-R3-22: an empty antenna name shows "ANT N" (a
//   remote window takes the Core's names). J.J. Boyd (KG4VCF), AI-assisted
//   via Anthropic Claude Code.
//   2026-09-24  R-R3-22 / R-R3-47: a remote window's Disconnect and
//   Reconnect are sent to the Core by the applet (disconnectRfKit,
//   configureRfKit, remoteRfKitControlVersion 2), and a status line shows
//   the Core's connection as it changes and the plain reason for a
//   refusal. OPERATE and the antennas stay with remote transmit. J.J. Boyd
//   (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25  R-R3-49 (parity Task 10): a remote window's OPERATE and
//   ANT 1 to 4 ask the Core (setRfKitOperate, setRfKitAntenna,
//   remoteRfKitControlVersion 4) while the Core is connected to the amp and
//   the radio is off the air, disabled with the reason otherwise; they
//   follow the amp's report, never the click, and never reach this
//   computer's own connection. coreDiagnosticsText for Copy diagnostics.
//   J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "Rf2ksApplet.h"
#include "AmpApplet.h"
#include "gui/HGauge.h"
#include "core/session/IStationLink.h"
#include "gui/OperatorReasonText.h"
#include "gui/StyleConstants.h"
#include "gui/UnbuiltFeatures.h"
#include "models/AccessoryDataModel.h"
#include "models/RadioModel.h"
#include "models/RfKitModel.h"

#include <QContextMenuEvent>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QStyle>
#include <QVBoxLayout>

namespace NereusSDR {

Rf2ksApplet::Rf2ksApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Section A: header row.
    // Device label + nickname/version on the left;
    // status dot + OPERATE/STANDBY button on the right.
    auto* headerWrap = new QWidget(this);
    auto* header = new QHBoxLayout(headerWrap);
    header->setContentsMargins(8, 6, 8, 6);

    auto* leftCol = new QVBoxLayout();
    m_deviceLabel   = new QLabel(QStringLiteral("RF-Kit RF2K-S"), headerWrap);
    m_nicknameLabel = new QLabel(QString(), headerWrap);
    m_deviceLabel->setStyleSheet(QStringLiteral("font-weight:600;"));
    m_nicknameLabel->setStyleSheet(QStringLiteral("color:#7d8893; font-size:10px;"));
    leftCol->addWidget(m_deviceLabel);
    leftCol->addWidget(m_nicknameLabel);
    header->addLayout(leftCol);
    header->addStretch();

    m_statusDot = new QLabel(headerWrap);
    m_statusDot->setFixedSize(10, 10);
    setConnectedState(false);
    header->addWidget(m_statusDot);

    m_operateBtn = new QPushButton(QStringLiteral("STANDBY"), headerWrap);
    m_operateMode = QStringLiteral("STANDBY");
    connect(m_operateBtn, &QPushButton::clicked, this, [this]() {
        const bool wantOperate = m_operateMode != QStringLiteral("OPERATE");
        // R-R3-49 (parity Task 10): a remote window asks the Core, whose amp
        // switches; the button follows the amp's report, not the click.
        if (isRemoteModel()) {
            if (remoteControlReason().isEmpty()) {
                m_model->stationLink()->requestRfKitOperate(wantOperate);
            }
            return;
        }
        // Group B fix wave (M5, the operator's ruling 2026-09-25): this
        // computer's amp waits on the air too, by the Core's own rule.
        if (refuseLocalSwitchOnAir()) {
            updateRemoteControls();
            return;
        }
        emit operateToggled(wantOperate);
    });
    header->addWidget(m_operateBtn);

    root->addWidget(headerWrap);

    // Section B: gauges + telemetry strip.
    auto* gaugesWrap = new QWidget(this);
    auto* gaugesLay  = new QVBoxLayout(gaugesWrap);
    gaugesLay->setContentsMargins(8, 6, 8, 6);

    // Gauge thresholds and tick labels mirror src/gui/applets/AmpApplet.cpp
    // exactly so RF-Kit operators have the same visual language PGXL
    // operators already know.  Bench feedback 2026-05-25 KG4VCF: without
    // tick labels the bar reads as "about 1/4 scale" with no power context.
    m_fwdGauge  = new HGauge(gaugesWrap);
    m_fwdGauge->setRange(0.0, 2000.0);
    m_fwdGauge->setYellowStart(1000.0);
    m_fwdGauge->setRedStart(1500.0);
    m_fwdGauge->setTitle(QStringLiteral("Fwd Pwr"));
    m_fwdGauge->setUnit(QStringLiteral("W"));
    m_fwdGauge->setTickLabels({
        QStringLiteral("0"),
        QStringLiteral("500"),
        QStringLiteral("1000"),
        QStringLiteral("1.5k"),
        QStringLiteral("2k")
    });
    gaugesLay->addWidget(m_fwdGauge);

    // SWR stored as x100 fixed-point (1.0 -> 100, 3.0 -> 300) so we can
    // use the integer-friendly gauge range while still resolving to 2 d.p.
    m_swrGauge  = new HGauge(gaugesWrap);
    m_swrGauge->setRange(100.0, 300.0);
    m_swrGauge->setYellowStart(200.0);
    m_swrGauge->setRedStart(250.0);
    m_swrGauge->setTitle(QStringLiteral("SWR"));
    m_swrGauge->setValue(100.0);   // start at 1.0:1
    m_swrGauge->setTickLabels({
        QStringLiteral("1"),
        QStringLiteral("1.5"),
        QStringLiteral("2"),
        QStringLiteral("2.5"),
        QStringLiteral("3")
    });
    gaugesLay->addWidget(m_swrGauge);

    m_tempGauge = new HGauge(gaugesWrap);
    m_tempGauge->setRange(0.0, 100.0);
    m_tempGauge->setYellowStart(55.0);
    m_tempGauge->setRedStart(80.0);
    m_tempGauge->setTitle(QStringLiteral("Temp"));
    m_tempGauge->setUnit(QStringLiteral("°C"));
    m_tempGauge->setTickLabels({
        QStringLiteral("0"),
        QStringLiteral("30"),
        QStringLiteral("55"),
        QStringLiteral("80"),
        QStringLiteral("100")
    });
    gaugesLay->addWidget(m_tempGauge);

    m_telemetryLabel = new QLabel(gaugesWrap);
    m_telemetryLabel->setStyleSheet(QStringLiteral("color:#9aa5b1; font-size:10px;"));
    gaugesLay->addWidget(m_telemetryLabel);
    root->addWidget(gaugesWrap);

    // Section C: antennas + tuner status + greyed TUNE/BYPASS.
    auto* tunerWrap = new QWidget(this);
    auto* tunerLay  = new QVBoxLayout(tunerWrap);
    tunerLay->setContentsMargins(8, 6, 8, 6);

    // 2026-05-25 KG4VCF bench fix: green-on-active styling.  Each antenna
    // button carries a dynamic `active` property (set in setAntennas /
    // setActiveAntenna based on what the amp reports); a QSS attribute
    // selector on the button itself paints active=true buttons green --
    // same palette as the band / mode / filter buttons elsewhere in the
    // app (kGreenBg / kGreenText / kGreenBorder).  We use a property
    // selector instead of QPushButton:checked + setCheckable() because
    // we don't want the user's click to flip the visual state until the
    // amp confirms via its /antennas/active response; the property is
    // written exclusively from amp-driven setters.
    const QString activeStyle = QStringLiteral(
        "QPushButton[active=\"true\"] {"
        "  background: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 3px; font-weight: bold;"
        "}"
    ).arg(Style::kGreenBg, Style::kGreenText, Style::kGreenBorder);

    auto* antennaRow = new QHBoxLayout();
    for (int i = 1; i <= 4; ++i) {
        auto* btn = new QPushButton(QStringLiteral("ANT %1").arg(i), tunerWrap);
        btn->setStyleSheet(activeStyle);
        // Seed the property so the QSS selector has something to match
        // before the first amp poll lands.  Defaults to inactive (no
        // green) until setActiveAntenna() flips one on.
        btn->setProperty("active", false);
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            // R-R3-49 (parity Task 10): a remote window's ANT asks the Core.
            if (isRemoteModel()) {
                if (remoteControlReason().isEmpty()) {
                    m_model->stationLink()->requestRfKitAntenna(i);
                }
                return;
            }
            if (refuseLocalSwitchOnAir()) {   // group B fix wave (M5)
                updateRemoteControls();
                return;
            }
            emit antennaRequested(RfKitAntenna::Type::Internal, i);
        });
        m_antennaButtons[i] = btn;
        antennaRow->addWidget(btn);
    }
    tunerLay->addLayout(antennaRow);

    m_tunerStatusLabel = new QLabel(QStringLiteral("Tuner: -"), tunerWrap);
    tunerLay->addWidget(m_tunerStatusLabel);

    auto* actionRow = new QHBoxLayout();
    m_tuneBtn   = new QPushButton(QStringLiteral("TUNE"),   tunerWrap);
    m_bypassBtn = new QPushButton(QStringLiteral("BYPASS"), tunerWrap);
    // R-R3-17: user words. Firmware G200C267 has no tuner write verb; a
    // feature request to RF-Power is filed.
    const QString tip = QStringLiteral(
        "The amplifier's firmware does not let NereusSDR tune or bypass it.\n"
        "Press TUNE or BYPASS on the amplifier's front panel.\n"
        "These buttons turn on when the amplifier's firmware allows it.");
    m_tuneBtn->setEnabled(false);
    m_bypassBtn->setEnabled(false);
    m_tuneBtn->setToolTip(tip);
    m_bypassBtn->setToolTip(tip);
    actionRow->addWidget(m_tuneBtn,   2);
    actionRow->addWidget(m_bypassBtn, 1);
    tunerLay->addLayout(actionRow);
    // R-R3-49: hidden until tuning and bypass from NereusSDR are built.
    UnbuiltFeatures::hideUnlessBuilt(m_tuneBtn, UnbuiltFeature::RfkitTune);
    UnbuiltFeatures::hideUnlessBuilt(m_bypassBtn, UnbuiltFeature::RfkitTune);

    root->addWidget(tunerWrap);

    // R-R3-48: whether the amp follows the radio's band, and if not, the
    // TCI server address to enter on it.
    m_bandFollowLabel = new QLabel(this);
    m_bandFollowLabel->setObjectName(QStringLiteral("rfKitBandFollowLabel"));
    m_bandFollowLabel->setTextFormat(Qt::PlainText);
    m_bandFollowLabel->setWordWrap(true);
    m_bandFollowLabel->setContentsMargins(8, 0, 8, 4);
    m_bandFollowLabel->setStyleSheet(QStringLiteral("color: #9aa5b1; font-size: 10px;"));
    root->addWidget(m_bandFollowLabel);

    // R-R3-47: a remote window's readings come from the Core; this line
    // says when they are not live.
    m_staleLabel = new QLabel(this);
    m_staleLabel->setTextFormat(Qt::PlainText);
    m_staleLabel->setWordWrap(true);
    m_staleLabel->setContentsMargins(8, 0, 8, 6);
    m_staleLabel->setStyleSheet(QStringLiteral("color: #d9a441; font-size: 10px;"));
    m_staleLabel->setVisible(false);
    root->addWidget(m_staleLabel);

    // R-R3-22: a remote window's line for the Core's connection to the amp
    // and the reason a Disconnect or Connect was not taken.
    m_connectionLabel = new QLabel(this);
    m_connectionLabel->setObjectName(QStringLiteral("rfKitConnectionLabel"));
    m_connectionLabel->setTextFormat(Qt::PlainText);
    m_connectionLabel->setWordWrap(true);
    m_connectionLabel->setContentsMargins(8, 0, 8, 6);
    m_connectionLabel->setStyleSheet(QStringLiteral("color: #9aa5b1; font-size: 10px;"));
    m_connectionLabel->setVisible(false);
    root->addWidget(m_connectionLabel);

    // R-R3-47: the header, gauges and strip follow the RadioModel's
    // RfKitModel: the Core's `rfkit` object in a remote window, the same
    // object fed by this computer's own Rf2ksConnection in a local one.
    if (m_model) {
        m_rfKit = m_model->rfKitModel();
        if (m_rfKit) {
            connect(m_rfKit, &RfKitModel::statusChanged, this, &Rf2ksApplet::syncFromRfKit);
            connect(m_rfKit, &RfKitModel::stationConnectionChanged,
                    this, &Rf2ksApplet::syncFromRfKit);
            connect(m_rfKit, &RfKitModel::bandFollowChanged,
                    this, &Rf2ksApplet::syncFromRfKit);
            connect(m_rfKit, &RfKitModel::stationConnectionChanged,
                    this, &Rf2ksApplet::updateConnectionLine);
            m_lastPhase = m_rfKit->connectionPhase();
        }
        connect(m_model, &RadioModel::stationLinkStateChanged,
                this, &Rf2ksApplet::updateStationState);
        connect(m_model, &RadioModel::stationCommandFinished,
                this, &Rf2ksApplet::onStationCommandFinished);
        if (m_model->role() == RadioModel::Role::Remote) {
            connect(m_model, &RadioModel::stationLinkStateChanged,
                    this, &Rf2ksApplet::updateRemoteControls);
        }
        // Group B fix wave (M5): both windows wait on the air.
        connect(m_model, &RadioModel::coreOnAirChanged,
                this, &Rf2ksApplet::updateRemoteControls);
        syncFromRfKit();
        updateStationState();
    }

    // R-R3-21: rfKitEnabled is mirrored from the Core, so a Core with RF-Kit
    // enabled shows this applet in a remote window. OPERATE and the antenna
    // buttons drive this computer's own Rf2ksConnection in a local window
    // (MainWindow's handlers), which a remote window never opens: the
    // amplifier sits at the station. R-R3-49 (parity Task 10): a Core at
    // remoteRfKitControlVersion 4 switches its own amp instead.
    updateRemoteControls();
}

bool Rf2ksApplet::remoteFullControl() const
{
    if (!isRemoteModel()) { return false; }
    const IStationLink* link = m_model->stationLink();
    return link && link->rfKitFullControlAvailable();
}

QString Rf2ksApplet::remoteControlReason() const
{
    if (!remoteFullControl()) {
        return AmpApplet::remoteUnavailableReason();
    }
    // They key nothing, so a receive-only Core takes them; they wait while
    // the radio is on the air and need the Core connected to the amp.
    if (m_model->isCoreOnAir()) {
        return RadioModel::onAirReason();
    }
    if (!m_rfKit || m_rfKit->connectionPhase() != TunerModel::ConnectionPhase::Connected) {
        return tr("The Core is not connected to the RF-Kit amplifier.");
    }
    return QString();
}

bool Rf2ksApplet::refuseLocalSwitchOnAir()
{
    // Parity mini-round (ruling c): a refused click says why, as a remote
    // window's Core does.
    return m_model && !isRemoteModel()
        && m_model->refuseLocalAccessorySwitchOnAir(QStringLiteral("rfkit"));
}

void Rf2ksApplet::updateRemoteControls()
{
    if (!m_operateBtn) {
        return;
    }
    if (!isRemoteModel()) {
        // Group B fix wave (M5, the operator's ruling 2026-09-25): a local
        // window's OPERATE and antennas wait while the radio is on the air,
        // with the remote window's reason; otherwise as the amp lists them.
        const bool onAir = m_model && m_model->isCoreOnAir();
        const QString reason = onAir ? RadioModel::onAirReason() : QString();
        m_operateBtn->setEnabled(!onAir);
        m_operateBtn->setToolTip(reason);
        m_operateBtn->setAccessibleDescription(reason);
        for (auto it = m_antennaButtons.cbegin(); it != m_antennaButtons.cend(); ++it) {
            it.value()->setEnabled(!onAir && !m_localAntennaDisabled.contains(it.key()));
            it.value()->setToolTip(reason);
            it.value()->setAccessibleDescription(reason);
        }
        return;
    }
    const QString reason = remoteControlReason();
    m_operateBtn->setEnabled(reason.isEmpty());
    m_operateBtn->setToolTip(reason);
    m_operateBtn->setAccessibleDescription(reason);
    // An antenna the amp lists as disabled, or does not list once it has
    // listed its antennas, stays off as in a local window.
    const int present = m_rfKit ? m_rfKit->antennaPresentMask() : 0;
    const int disabled = m_rfKit ? m_rfKit->antennaDisabledMask() : 0;
    for (auto it = m_antennaButtons.cbegin(); it != m_antennaButtons.cend(); ++it) {
        const int bit = 1 << (it.key() - 1);
        const bool listed = present == 0 || ((present & bit) != 0 && (disabled & bit) == 0);
        const QString why = !reason.isEmpty() ? reason
            : listed ? QString()
                     : tr("This antenna is not available on the RF-Kit amplifier.");
        it.value()->setEnabled(why.isEmpty());
        it.value()->setToolTip(why);
        it.value()->setAccessibleDescription(why);
    }
}

QString Rf2ksApplet::antennaButtonToolTipForTesting(int number) const
{
    const auto* btn = m_antennaButtons.value(number, nullptr);
    return btn ? btn->toolTip() : QString();
}

QString Rf2ksApplet::coreDiagnosticsText(RadioModel* model)
{
    const RfKitModel* rfKit = model ? model->rfKitModel() : nullptr;
    const AccessoryDataModel* data = model ? model->accessoryDataModel() : nullptr;
    if (!rfKit) { return QString(); }
    const auto time = [](qint64 ms) {
        return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms).toString(Qt::ISODate)
                      : QStringLiteral("--");
    };
    const bool connected = rfKit->connectionPhase() == TunerModel::ConnectionPhase::Connected;
    // The local Copy diagnostics' lines (MainWindow), from the Core.
    QString text = QStringLiteral("RF-Kit RF2K-S diagnostics (the Core's connection)\n");
    text += QStringLiteral("Connected: %1\n").arg(connected ? QStringLiteral("Yes")
                                                              : QStringLiteral("No"));
    text += QStringLiteral("Host: %1:%2\n")
                .arg(rfKit->configuredHost().isEmpty() ? QStringLiteral("--")
                                                       : rfKit->configuredHost())
                .arg(rfKit->configuredPort());
    text += QStringLiteral("Version: %1\n").arg(rfKit->deviceVersion());
    text += QStringLiteral("Operate: %1\nInterface: %2\n")
                .arg(rfKit->operate() ? QStringLiteral("Yes") : QStringLiteral("No"),
                     rfKit->operationalInterface().isEmpty() ? QStringLiteral("--")
                                                             : rfKit->operationalInterface());
    if (!rfKit->connectionError().isEmpty()) {
        text += QStringLiteral("Last error: %1\n").arg(rfKit->connectionError());
    }
    if (data) {
        text += QStringLiteral("Polls OK/failed: %1/%2\n")
                    .arg(data->rfkitPollsOk()).arg(data->rfkitPollsFailed());
        // Group B fix wave (M7): the local copy's RTT line, from a Core at
        // accessoryDataVersion 3.
        const IStationLink* link = model->stationLink();
        text += (link && link->rfKitResponseTimeAvailable())
            ? QStringLiteral("RTT avg: %1 ms\n").arg(data->rfkitRttAvgMs())
            : QStringLiteral("RTT avg: --\n");
        text += QStringLiteral("Reconnects: %1\n").arg(data->rfkitReconnectCount());
        text += QStringLiteral("Connected since: %1\n").arg(time(data->rfkitConnectedSinceMs()));
        text += QStringLiteral("Last poll: %1\n").arg(time(data->rfkitLastPollMs()));
    }
    return text;
}

bool Rf2ksApplet::isRemoteModel() const
{
    return m_model && !m_model->ownsLocalDsp();
}

// R-R3-47: every reading the RfKitModel holds. The status dot follows the
// connection phase; the name and version appear once the amp has sent
// them; OPERATE and the meters once it has sent a reading, and then as
// they are, zeros and STANDBY included.
void Rf2ksApplet::syncFromRfKit()
{
    if (!m_rfKit) {
        return;
    }
    setConnectedState(m_rfKit->connectionPhase() == TunerModel::ConnectionPhase::Connected);
    // R-R3-49 (parity Task 10): the Core's phase and the amp's antenna list
    // decide a remote window's OPERATE and ANT.
    updateRemoteControls();
    if (!m_rfKit->deviceNickname().isEmpty() || !m_rfKit->deviceVersion().isEmpty()) {
        setNicknameAndVersion(m_rfKit->deviceNickname(), m_rfKit->deviceVersion());
    }
    if (m_bandFollowLabel) {
        m_bandFollowLabel->setText(m_rfKit->bandFollowText());
    }
    if (!m_rfKit->present()) {
        return;
    }
    // R-R3-47: the tuner and antenna rows, as the amp last reported them.
    setTuner(m_rfKit->tuner());
    if (m_rfKit->antennaPresentMask() != 0) {
        setAntennas(m_rfKit->antennas());
    }
    if (m_rfKit->activeAntennaNumber() > 0) {
        setActiveAntenna(m_rfKit->activeAntenna());
    }
    setOperateMode(m_rfKit->operate() ? QStringLiteral("OPERATE") : QStringLiteral("STANDBY"));
    RfKitPowerSnapshot snap;
    snap.forwardW = qRound(m_rfKit->forwardPowerW());
    snap.reflectedW = qRound(m_rfKit->reflectedPowerW());
    snap.swr = static_cast<float>(m_rfKit->swr());
    snap.temperatureC = static_cast<float>(m_rfKit->temperatureC());
    snap.voltageV = static_cast<float>(m_rfKit->voltageV());
    snap.currentA = static_cast<float>(m_rfKit->currentA());
    setPower(snap);
}

void Rf2ksApplet::updateStationState()
{
    if (!m_staleLabel) {
        return;
    }
    if (!isRemoteModel()) {
        m_staleLabel->setVisible(false);
        return;
    }
    const IStationLink* link = m_model->stationLink();
    if (!link || !link->stationLinkReady()) {
        m_staleLabel->setText(tr("Core disconnected. RF-Kit readings are stale."));
        m_staleLabel->setVisible(true);
    } else if (!link->remoteRfKitStatusAvailable()) {
        m_staleLabel->setText(tr("This Core does not report its RF-Kit amplifier to this "
                                 "app. Updating the Core may help."));
        m_staleLabel->setVisible(true);
    } else {
        m_staleLabel->setVisible(false);
    }
    updateConnectionLine();
}

// R-R3-22: the Core's phase, or the reason the applet's own request was
// not taken, while the Core reports its amp; the stale line covers the rest.
void Rf2ksApplet::updateConnectionLine()
{
    if (!m_connectionLabel) {
        return;
    }
    if (m_rfKit && m_rfKit->connectionPhase() != m_lastPhase) {
        // The Core moved: its phase replaces an earlier refusal. A request
        // still waiting stays tied to its own command's result.
        m_lastPhase = m_rfKit->connectionPhase();
        m_requestReason.clear();
    }
    const IStationLink* link = isRemoteModel() ? m_model->stationLink() : nullptr;
    if (!m_rfKit || !link || !link->stationLinkReady() || !link->remoteRfKitStatusAvailable()) {
        m_connectionLabel->clear();
        m_connectionLabel->setVisible(false);
        return;
    }
    m_connectionLabel->setText(m_requestReason.isEmpty()
        ? AmpApplet::stationConnectionText(m_rfKit->connectionPhase(),
                                           m_rfKit->connectionError())
        : m_requestReason);
    m_connectionLabel->setVisible(true);
}

// R-R3-22: the Core answered the applet's own Disconnect or Connect: a
// refusal shows its reason; either way the request is no longer waiting.
// The RF-Kit page's requests show on the page.
void Rf2ksApplet::onStationCommandFinished(quint32 commandId, bool accepted,
                                           const QString& reason)
{
    if (m_pendingCommandId == 0 || commandId != m_pendingCommandId) {
        return;
    }
    m_pendingCommandId = 0;
    if (!accepted) {
        m_requestReason = OperatorReasonText::forDisplay(reason);
        updateConnectionLine();
    }
}

void Rf2ksApplet::requestRemoteConnectionToggle()
{
    IStationLink* link = m_model ? m_model->stationLink() : nullptr;
    if (!link || !m_rfKit || !link->remoteRfKitControlAvailable()) {
        return;
    }
    const auto outcome = AmpApplet::stationConnectionActive(m_rfKit->connectionPhase())
        ? link->requestDisconnectRfKit()
        : link->requestConfigureRfKit(m_rfKit->configuredHost(),
                                      static_cast<quint16>(m_rfKit->configuredPort()));
    m_pendingCommandId = outcome.sent ? outcome.commandId : 0;
    m_requestReason = outcome.sent ? QString() : OperatorReasonText::forDisplay(outcome.reason);
    updateConnectionLine();
    // R-R3-21 / R-R3-23: a refusal of this request shows on the applet's
    // connection line, so it is not toasted too, as the accessory pages
    // claim theirs. The claim is on the line itself: it holds only while
    // the line is on screen when the refusal arrives.
    if (outcome.sent) {
        m_model->noteAccessoryRequestShownOnPage(outcome.commandId, m_connectionLabel);
    }
}

QString Rf2ksApplet::connectionLineTextForTesting() const
{
    return m_connectionLabel && !m_connectionLabel->isHidden() ? m_connectionLabel->text()
                                                               : QString();
}

bool Rf2ksApplet::staleIndicatorVisibleForTesting() const
{
    return m_staleLabel && !m_staleLabel->isHidden();
}

QString Rf2ksApplet::staleIndicatorTextForTesting() const
{
    return m_staleLabel ? m_staleLabel->text() : QString();
}

QString Rf2ksApplet::bandFollowTextForTesting() const
{
    return m_bandFollowLabel ? m_bandFollowLabel->text() : QString();
}

// ---------- Section A slots ----------

void Rf2ksApplet::setNicknameAndVersion(const QString& nickname, const QString& version)
{
    m_nicknameLabel->setText(QStringLiteral("%1  %2").arg(nickname, version));
}

void Rf2ksApplet::setOperateMode(const QString& mode)
{
    m_operateMode = mode;
    m_operateBtn->setText(mode);

    // 2026-05-25 KG4VCF bench fix: button styling did not change on
    // mode flip ("standby operate button does not change the color of
    // the glowing dot on mode change").  Mirror AmpApplet::setState
    // styling: green pill on OPERATE, neutral blue-grey on STANDBY.
    const bool operating = (mode == QStringLiteral("OPERATE"));
    if (operating) {
        m_operateBtn->setStyleSheet(
            QStringLiteral(
                "QPushButton { background: #006030; border: 1px solid #008040; "
                "border-radius: 3px; color: #ffffff; font-size: 10px; font-weight: bold; }"
                "QPushButton:hover { background: #007040; }"));
    } else {
        m_operateBtn->setStyleSheet(
            QStringLiteral(
                "QPushButton { background: #1a3a5a; border: 1px solid #205070; "
                "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
                "QPushButton:hover { background: #204060; }"));
    }
}

void Rf2ksApplet::setConnectedState(bool connected)
{
    m_connected = connected;
    const QString color = connected
        ? QStringLiteral("#34c759")
        : QStringLiteral("#e64949");
    m_statusDot->setStyleSheet(
        QStringLiteral("background:%1; border-radius:5px;").arg(color));
}

// ---------- Section B slots ----------

void Rf2ksApplet::setPower(const RfKitPowerSnapshot& snap)
{
    m_fwdGauge->setValue(static_cast<double>(snap.forwardW));
    // SWR stored x100 so the gauge range (100..300) maps to 1.0..3.0.
    m_swrGauge->setValue(static_cast<double>(snap.swr) * 100.0);
    m_tempGauge->setValue(static_cast<double>(snap.temperatureC));
    // Bench feedback 2026-05-25 KG4VCF: include numeric forward power +
    // SWR in the telemetry strip so the operator has a concrete reading
    // alongside the bar graph (the bar alone reads as "about 1/4 scale").
    m_telemetryLabel->setText(QStringLiteral("Fwd %1 W  SWR %2  %3 V  %4 A")
        .arg(snap.forwardW)
        .arg(static_cast<double>(snap.swr), 0, 'f', 2)
        .arg(snap.voltageV, 0, 'f', 0)
        .arg(snap.currentA, 0, 'f', 1));
}

// ---------- Section C slots ----------

void Rf2ksApplet::setTuner(const RfKitTunerSnapshot& snap)
{
    using Mode = RfKitTunerSnapshot::Mode;
    QString text;
    switch (snap.mode) {
        case Mode::AutoTuning:
            text = QStringLiteral("TUNING...");
            break;
        case Mode::Bypass:
            text = QStringLiteral("BYPASS");
            break;
        case Mode::Auto:
        case Mode::Manual:
            if (snap.tunedFrequencyKHz > 0) {
                text = QStringLiteral("TUNED %1 MHz (%2)")
                          .arg(snap.tunedFrequencyKHz / 1000.0, 0, 'f', 3)
                          .arg(snap.setup);
            } else {
                text = QStringLiteral("NOT TUNED");
            }
            break;
        case Mode::Unknown:
            text = QStringLiteral("Tuner: -");
            break;
    }
    m_tunerStatusLabel->setText(text);
}

// QSS attribute selectors only re-evaluate when the widget is
// unpolish'd + polish'd; setProperty alone is not enough to trigger
// a repaint.  This helper bundles both so we never forget.
static void setButtonActive(QPushButton* btn, bool active)
{
    if (!btn) {
        return;
    }
    btn->setProperty("active", active);
    if (auto* s = btn->style()) {
        s->unpolish(btn);
        s->polish(btn);
    }
    btn->update();
}

void Rf2ksApplet::setAntennas(const QList<RfKitAntenna>& list)
{
    for (const auto& a : list) {
        if (a.type != RfKitAntenna::Type::Internal) {
            continue;
        }
        auto* btn = m_antennaButtons.value(a.number, nullptr);
        if (!btn) {
            continue;
        }
        // R-R3-47: an empty name (the Core's default) shows "ANT N" too.
        QString label = m_antennaLabels.value(a.number);
        if (label.isEmpty()) {
            label = QStringLiteral("ANT %1").arg(a.number);
        }
        btn->setText(label);
        // R-R3-21: an amplifier report never re-enables a remote window's
        // antenna buttons by itself: updateRemoteControls decides there.
        // Group B fix wave (M5): a local window keeps the amp's listing and
        // updateRemoteControls adds the on-air rule.
        if (!isRemoteModel()) {
            if (a.state == RfKitAntenna::State::Disabled) {
                m_localAntennaDisabled.insert(a.number);
            } else {
                m_localAntennaDisabled.remove(a.number);
            }
        }
        setButtonActive(btn, a.state == RfKitAntenna::State::Active);
    }
    updateRemoteControls();
}

void Rf2ksApplet::setActiveAntenna(const RfKitAntenna& a)
{
    // m_antennaButtons is keyed by INTERNAL antenna number (1..4).  The amp
    // numbers its external antennas from 1 as well, so matching on number
    // alone lit the internal ANT button whose index happened to collide with
    // the active external antenna, telling the operator RF was routed
    // somewhere it was not.  An external antenna owns no button here, so
    // every internal button goes dark.  Codex review, PR #291.
    const bool internalActive = (a.type == RfKitAntenna::Type::Internal);
    for (auto it = m_antennaButtons.begin(); it != m_antennaButtons.end(); ++it) {
        setButtonActive(it.value(), internalActive && it.key() == a.number);
    }
}

void Rf2ksApplet::setAntennaLabel(int number, const QString& label)
{
    m_antennaLabels[number] = label;
    if (auto* btn = m_antennaButtons.value(number, nullptr)) {
        btn->setText(label.isEmpty()
            ? QStringLiteral("ANT %1").arg(number)
            : label);
    }
}

// ---------- Section A test seams ----------

QString Rf2ksApplet::deviceLabelTextForTesting()   const { return m_deviceLabel->text(); }
QString Rf2ksApplet::nicknameLabelTextForTesting() const { return m_nicknameLabel->text(); }
QString Rf2ksApplet::operateButtonTextForTesting() const { return m_operateBtn->text(); }
void    Rf2ksApplet::clickOperateButtonForTesting() { m_operateBtn->click(); }

// ---------- Section B+C test seams ----------

int     Rf2ksApplet::fwdGaugeValueForTesting()      const { return static_cast<int>(m_fwdGauge->value()); }
float   Rf2ksApplet::swrGaugeValueForTesting()      const { return static_cast<float>(m_swrGauge->value() / 100.0); }
float   Rf2ksApplet::tempGaugeValueForTesting()     const { return static_cast<float>(m_tempGauge->value()); }
QString Rf2ksApplet::telemetryStripTextForTesting() const { return m_telemetryLabel->text(); }

QString Rf2ksApplet::antennaButtonTextForTesting(int number) const
{
    const auto* btn = m_antennaButtons.value(number, nullptr);
    return btn ? btn->text() : QString();
}

bool Rf2ksApplet::antennaButtonIsActiveForTesting(int number) const
{
    const auto* btn = m_antennaButtons.value(number, nullptr);
    return btn && btn->property("active").toBool();
}

bool Rf2ksApplet::antennaButtonIsEnabledForTesting(int number) const
{
    const auto* btn = m_antennaButtons.value(number, nullptr);
    return btn && btn->isEnabled();
}

void Rf2ksApplet::clickAntennaButtonForTesting(int number)
{
    if (auto* btn = m_antennaButtons.value(number, nullptr)) {
        btn->click();
    }
}

QString Rf2ksApplet::tunerStatusTextForTesting()    const { return m_tunerStatusLabel->text(); }
bool    Rf2ksApplet::tuneButtonIsEnabledForTesting()   const { return m_tuneBtn->isEnabled(); }
bool    Rf2ksApplet::bypassButtonIsEnabledForTesting() const { return m_bypassBtn->isEnabled(); }
QString Rf2ksApplet::tuneButtonTooltipForTesting()  const { return m_tuneBtn->toolTip(); }

// ---------- Section D: right-click context menu ----------

QMenu* Rf2ksApplet::buildContextMenu(QObject* menuParent)
{
    auto* menu = new QMenu(qobject_cast<QWidget*>(menuParent));
    auto* openAdv = menu->addAction(QStringLiteral("Open RF-Kit Advanced..."));
    menu->addSeparator();
    // R-R3-22 / R-R3-47: in a remote window the toggle follows the Core's
    // phase (Cancel while it is still trying) and asks the Core, which
    // owns the amp; a Core that does not offer it keeps the item off.
    const bool remote = isRemoteModel();
    const bool active = remote
        ? (m_rfKit && AmpApplet::stationConnectionActive(m_rfKit->connectionPhase()))
        : m_connected;
    // Remote: Cancel while the Core is still trying (R-R3-22 fix wave).
    auto* disco = menu->addAction(remote
        ? AmpApplet::stationConnectionToggleText(
              m_rfKit ? m_rfKit->connectionPhase() : TunerModel::ConnectionPhase::Disabled)
        : (active ? QStringLiteral("Disconnect") : QStringLiteral("Connect")));
    auto* diag  = menu->addAction(QStringLiteral("Copy diagnostics to clipboard"));

    connect(openAdv, &QAction::triggered, this, [this] {
        emit navigationRequested(QStringLiteral("rfKit"));
    });
    if (!remote) {
        connect(disco, &QAction::triggered, this, &Rf2ksApplet::connectionToggleRequested);
    } else {
        const IStationLink* link = m_model ? m_model->stationLink() : nullptr;
        QString unavailable;
        if (link && !link->stationLinkReady()) {
            unavailable = tr("Waiting for the Core to connect.");
        } else if (!link || !link->remoteRfKitControlAvailable()) {
            unavailable = tr("This Core does not offer RF-Kit amplifier setup to this app.");
        } else if (!active && (!m_rfKit || m_rfKit->configuredHost().isEmpty()
                               || m_rfKit->configuredPort() <= 0)) {
            unavailable = tr("Enter the RF-Kit amplifier's address in Setup first.");
        }
        if (!unavailable.isEmpty()) {
            disco->setEnabled(false);
            disco->setToolTip(unavailable);
            menu->setToolTipsVisible(true);
        }
        connect(disco, &QAction::triggered, this, &Rf2ksApplet::requestRemoteConnectionToggle);
    }
    connect(diag,  &QAction::triggered, this, &Rf2ksApplet::diagnosticsCopyRequested);
    return menu;
}

void Rf2ksApplet::contextMenuEvent(QContextMenuEvent* ev)
{
    auto* menu = buildContextMenu(this);
    menu->exec(ev->globalPos());
    delete menu;
}

} // namespace NereusSDR
