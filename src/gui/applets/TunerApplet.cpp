// =================================================================
// src/gui/applets/TunerApplet.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR -- GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       -- per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 ss.5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-18  Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Layout from AetherSDR src/gui/TunerApplet.{h,cpp}
//                 (ATU/tune controls + SWR progress bar). All controls
//                 NYI -- wired in later phase.
//   2026-05-18  Rewired for TGXL by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//                 NyiOverlay removed; QProgressBar relay bars replaced
//                 with RelayBar; 3-button mode group replaced with single
//                 cycle button; TunerModel signal connections added;
//                 antenna container added (hidden by default).
//                 From AetherSDR src/gui/TunerApplet.cpp [@0cd4559].
//   2026-09-24  R-R3-49 / R-R3-47 by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//                 In a remote window, ANT 1/2/3 and OPERATE ask the Core
//                 (remoteTgxlControlVersion 2) and wait while the radio
//                 is on the air; TUNE and the relay bars are unchanged.
//   2026-09-24  R-R3-49 fix wave by J.J. Boyd (KG4VCF), with AI-assisted
//                 transformation via Anthropic Claude Code. "On the air"
//                 reads the Core's real MOX (the radio's `transmitting`).
//                 STANDBY to OPERATE is one request on a Core at
//                 remoteTgxlControlVersion 3.
//   2026-09-24  R-R3-49 (parity Task 1) by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//                 coreOnAir() reads RadioModel::isCoreOnAir().
//   2026-09-25  R-R3-49 (parity Task 8) by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//                 In a remote window on a Core at remoteTgxlControlVersion
//                 4 the relay bars' wheel asks the Core (moveTgxlRelay) and
//                 waits while the radio is on the air; the bars follow the
//                 tuner's report. Recall tune memory and Open TGXL
//                 Advanced work in a remote window; coreDiagnosticsText.
//   2026-09-25  TUNE disabled with the reason while receive only, TX
//                 inhibit or a PA trip blocks transmit (receiver and
//                 transmit gaps plan, Task 16 fix wave M2), by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code. NereusSDR-native; no AetherSDR equivalent.
//   2026-09-25  A TUNE press on the TGXL itself while transmit is
//                 blocked is refused with the reason (tuneRefused), as the
//                 applet's TUNE is (Task 16 fix wave 2), by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code. NereusSDR-native; no AetherSDR equivalent.
//   2026-09-26  iPhone app plan Task 77 fix round 2 (R-IOS-02, R-IOS-03,
//                 R-IOS-13): TUNE waits on the air with the reason, as
//                 OPERATE and ANT do, by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: Fix wave GUI-I4, GUI-M3, GUI-M4: a remote window never
//               falls back to this computer's own Tuner Genius (TUNE,
//               ANT, OPERATE, relays wait with the reason); the context
//               menu shows its tooltips; plain stale label. Fix round 1:
//               a remote TUNE click while the Core is on the air starts
//               nothing and puts the reason back. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "TunerApplet.h"
#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "gui/HGauge.h"
#include "gui/RelayBar.h"
#include "models/RadioModel.h"
#include "models/TunerModel.h"
#include "models/AccessoryDataModel.h"
#include "core/session/IStationLink.h"
#include "core/session/PureSignalSessionFacade.h"
#include "models/TransmitModel.h"

#include <QContextMenuEvent>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace NereusSDR {

TunerApplet::TunerApplet(RadioModel* model, TunerModel* tunerModel, QWidget* parent,
                         TuneMemoryStore* tuneStore)
    : AppletWidget(model, parent)
    , m_tuneStore(tuneStore)
{
    m_transmitPermitted = !model
        || (model->role() == RadioModel::Role::Local
            && !model->receiveOnlyStationPolicy());
    m_stationConnected = !model || model->role() == RadioModel::Role::Local;

    // NOTE: m_tunerModel is deliberately NOT initialised from `tunerModel` in
    // the initializer list. The setTunerModel() call below uses an `if (==)`
    // early-return to detect a no-op rebind; passing the same pointer twice
    // would skip every signal connect in setTunerModel() (state/antA/
    // direct-connection wiring etc.), leaving the antenna buttons gated on
    // a flag that nothing ever updates. Bench-fix 2026-05-19.
    // Post-tune capture timer: after tuning=0 arrives, keep capturing SWR
    // for 400 ms so the final settled value from the TGXL has time to arrive.
    // From AetherSDR src/gui/TunerApplet.cpp:TunerApplet() [@0cd4559]
    m_postTuneTimer = new QTimer(this);
    m_postTuneTimer->setSingleShot(true);
    m_postTuneTimer->setInterval(400);
    connect(m_postTuneTimer, &QTimer::timeout, this, [this]() {
        m_postTuneCapture = false;
        float result = (m_tuneSwr < 900.0f) ? m_tuneSwr : m_swr;
        m_tuneBtn->setText(QString("SWR %1").arg(result, 0, 'f', 2));
        QTimer::singleShot(2500, this, [this]() {
            m_tuneBtn->setText(QStringLiteral("TUNE"));
        });
    });

    buildUI();

    if (tunerModel) {
        setTunerModel(tunerModel);
    }
    // R-R3-49 / R-R3-47: in a remote window ANT and OPERATE follow the
    // Core's offer (the link's capabilities) and its transmit state
    // (RadioModel::isCoreOnAir: the radio's `transmitting`, the Core's real
    // MOX; the mirrored transmit model's TUNE; and PureSignal's two-tone).
    if (model && model->role() == RadioModel::Role::Remote) {
        connect(model, &RadioModel::stationLinkStateChanged,
                this, &TunerApplet::updateActuatingControls);
    }
    // Group B fix wave (M5, the operator's ruling 2026-09-25): a local
    // window's relays, ANT and OPERATE wait on the air too, with the same
    // reason, so both windows follow one rule.
    if (model) {
        connect(model, &RadioModel::coreOnAirChanged,
                this, &TunerApplet::updateActuatingControls);
    }
    // Task 16 fix wave (M2): TUNE starts a transmission (the tune carrier
    // under the TGXL sweep), so it follows the transmit block the way the
    // TX applet's TUNE does, with the reason. Local and remote alike.
    if (model && model->moxController()) {
        connect(model->moxController(), &MoxController::transmitBlockChanged,
                this, [this](const QString&) { updateActuatingControls(); });
    }
    updateActuatingControls();
    updateStationAvailability();
}

void TunerApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    // Do NOT add appletTitleBar() here — AppletPanelWidget::wrapWithTitleBar
    // already prepends a host-side title bar from appletTitle(). Adding our
    // own here results in a double header. Same fix in PureSignalApplet.

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 2, 4, 4);
    vbox->setSpacing(2);

    m_staleLabel = new QLabel(
        QStringLiteral("Core disconnected. The tuner values are stale."), this);
    m_staleLabel->setTextFormat(Qt::PlainText);
    m_staleLabel->setWordWrap(true);
    m_staleLabel->setStyleSheet(QStringLiteral("color: #d9a441; font-size: 10px;"));
    m_staleLabel->setVisible(false);
    vbox->addWidget(m_staleLabel);

    // --- Control 1: Forward power gauge (0-200W, red@125; auto-rescaled via setPowerScale) ---
    // From AetherSDR src/gui/TunerApplet.cpp:buildUI() [@0cd4559]
    m_fwdPowerGauge = new HGauge(this);
    m_fwdPowerGauge->setRange(0.0, 200.0);
    m_fwdPowerGauge->setYellowStart(125.0);
    m_fwdPowerGauge->setRedStart(125.0);
    m_fwdPowerGauge->setTitle(QStringLiteral("Fwd Pwr"));
    m_fwdPowerGauge->setUnit(QStringLiteral("W"));
    vbox->addWidget(m_fwdPowerGauge);

    // --- Control 2: SWR gauge (1.0-3.0, red@2.5) ---
    // From AetherSDR src/gui/TunerApplet.cpp:buildUI() [@0cd4559]
    m_swrGauge = new HGauge(this);
    m_swrGauge->setRange(1.0, 3.0);
    m_swrGauge->setYellowStart(2.5);
    m_swrGauge->setRedStart(2.5);
    m_swrGauge->setTitle(QStringLiteral("SWR"));
    vbox->addWidget(m_swrGauge);

    // --- Bottom row: relay bars (left, 70%) + buttons (right, 30%) ---
    // From AetherSDR src/gui/TunerApplet.cpp:buildUI() [@0cd4559]
    auto* bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(4);

    // Left column: relay bars C1 / L / C2
    auto* relayCol = new QVBoxLayout;
    relayCol->setSpacing(2);

    // From AetherSDR src/gui/TunerApplet.cpp:buildUI() m_c1Bar / m_lBar / m_c2Bar [@0cd4559]
    m_c1Bar = new RelayBar(QStringLiteral("C1"), this);
    m_lBar  = new RelayBar(QStringLiteral("L"),  this);
    m_c2Bar = new RelayBar(QStringLiteral("C2"), this);
    relayCol->addWidget(m_c1Bar);
    relayCol->addWidget(m_lBar);
    relayCol->addWidget(m_c2Bar);
    bottomRow->addLayout(relayCol, 7);  // 70% width

    // Manual relay adjustment via mousewheel scroll.
    // From AetherSDR src/gui/TunerApplet.cpp:buildUI() relayAdjusted connections [@0cd4559]
    // R-R3-49 (parity Task 8): requestRelayMove asks the Core in a remote
    // window.
    connect(m_c1Bar, &RelayBar::relayAdjusted, this,
            [this](int dir) { requestRelayMove(0, dir); });
    connect(m_lBar, &RelayBar::relayAdjusted, this,
            [this](int dir) { requestRelayMove(1, dir); });
    connect(m_c2Bar, &RelayBar::relayAdjusted, this,
            [this](int dir) { requestRelayMove(2, dir); });

    // Right column: TUNE + OPERATE cycle buttons
    auto* btnCol = new QVBoxLayout;
    btnCol->setSpacing(2);

    // TUNE button -- non-toggle; shows "TUNING..." in red while active.
    // From AetherSDR src/gui/TunerApplet.cpp:buildUI() m_tuneBtn [@0cd4559]
    m_tuneBtn = new QPushButton(QStringLiteral("TUNE"), this);
    m_tuneBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_tuneBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a3a5a; border: 1px solid #205070; "
        "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
        "QPushButton:hover { background: #204060; }"));
    btnCol->addWidget(m_tuneBtn);

    // TUNE click orchestrates a full tune cycle:
    //   1. engage local tune-carrier (MoxController::setTune(true)) so RF is
    //      present at the TGXL input -- TGXL aborts with "low RF power" if
    //      it can't sense a carrier when it starts the relay sweep;
    //   2. after a 200 ms settle, send `tune start` to TGXL (TGXL then
    //      sweeps its relays under the carrier);
    //   3. on tuning=0 from TGXL (handled in the tuningChanged lambda
    //      below) drop the carrier we engaged.
    //
    // If the operator already has TUN engaged via the TxApplet TUNE button
    // (isManualMox() true), we skip step 1 and just start the TGXL sweep
    // immediately -- the operator's carrier is already on-air. The latch
    // m_carrierEngagedForTgxlTune tracks whether *we* engaged TUN so the
    // tuningChanged lambda only drops the carrier we put up, never the
    // operator's manual TUN.
    //
    // NereusSDR-native; no AetherSDR equivalent (AetherSDR routes through
    // a real FlexRadio that handles the carrier internally).
    connect(m_tuneBtn, &QPushButton::clicked, this, [this]() {
        if (!m_transmitPermitted || !m_tunerModel || transmitBlocked()) { return; }
        // GUI-I4 (fix wave): a remote window tunes only through the Core.
        if (remoteWindow() && !remoteTuneControl()) { return; }
        // Fix round 1 (minor 1): the Core's radio on the air (its TUN among
        // it) leaves TUNE disabled with the reason; a click that still gets
        // through puts the reason back rather than falling to this
        // computer's own tuner.
        if (remoteWindow() && coreOnAir()) {
            updateActuatingControls();
            return;
        }
        // Engage local CW tune carrier via the G.4 orchestrator
        // RadioModel::setTune(true). That call configures the gen1 PostGen
        // tone (TxChannel::setTuneTone), swaps CW->LSB/USB if needed,
        // loads tune power, and engages MOX. MoxController::setTune by
        // itself only flips MOX state -- it never asks TxChannel to emit a
        // tone, so a TGXL tune cycle preceded by that would see MOX-on but
        // no RF and abort with "low RF power" (bench-confirmed 22:19 on
        // 2026-05-19).
        if (m_model && !m_model->isTune()) {
            m_carrierEngagedForTgxlTune = true;
            // Reset the per-cycle "did TGXL ever enter tuning=1" flag so
            // the short-watchdog below knows whether to escape.
            m_tgxlEnteredTuning = false;
            // Route through RadioModel::startTgxlAutotune which does the
            // PGXL standby + setTune(true) + autotune-after-settle
            // orchestration in one place. The 200 ms settle for the
            // autotune command lives there. fromHardware=false because
            // we (the operator's app click) are initiating the cycle.
            // Bench-discovered 2026-05-20: TGXL refuses the sweep with
            // "no PTT" when PGXL is in OPERATE -- the amp's amplified
            // carrier breaks TGXL's calibration. Auto-standby PGXL fixes.
            m_model->startTgxlAutotune(/*fromHardware=*/false);
            // Short watchdog (3 s): if TGXL never enters its own
            // tuning=1 state within this window, TGXL refused to start
            // the cycle (typical: "no PTT" abort because PGXL is in
            // OPERATE rather than STANDBY, or autotune command rejected).
            // Drop the stuck local carrier so the operator isn't stranded.
            //
            // The watchdog is gated on m_tgxlEnteredTuning -- if TGXL
            // DID enter tuning=1, the tuningChanged(true) handler set
            // the flag and this watchdog is a no-op. The long tune cycle
            // (TGXL relay sweep, up to ~30 s) is then bounded by the
            // existing tuningChanged(false) handler, not this watchdog.
            //
            // Bench-fix 2026-05-20: the prior 30 s watchdog left the
            // operator stranded with an on-air carrier for half a minute
            // when TGXL aborted immediately. 3 s is enough for TGXL to
            // reply to `autotune` with either tuning=1 (sweep started)
            // or no state change (aborted).
            QTimer::singleShot(3 * 1000, this, [this]() {
                if (m_carrierEngagedForTgxlTune
                        && !m_tgxlEnteredTuning
                        && m_model) {
                    qInfo() << "TunerApplet::TUNE watchdog: TGXL never"
                               " entered tuning=1 within 3 s, aborting"
                               " (likely 'no PTT' / amp-in-OPERATE)";
                    m_model->setTune(false);
                    m_carrierEngagedForTgxlTune = false;
                }
            });
        } else {
            // TUN already engaged (operator pressed TxApplet TUNE first
            // and is now extending the cycle to a TGXL sweep) OR no
            // RadioModel available (test seam). Fall through to TGXL
            // command; the existing carrier is already on-air.
            m_tunerModel->autoTune();
        }
    });

    // Single cycle button: OPERATE -> BYPASS -> STANDBY -> OPERATE.
    // From AetherSDR src/gui/TunerApplet.cpp:buildUI() m_operateBtn [@0cd4559]
    m_operateBtn = new QPushButton(QStringLiteral("STANDBY"), this);
    m_operateBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_operateBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1a3a5a; border: 1px solid #205070; "
        "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
        "QPushButton:hover { background: #204060; }"));
    btnCol->addWidget(m_operateBtn);

    connect(m_operateBtn, &QPushButton::clicked, this,
            &TunerApplet::cycleOperateState);

    bottomRow->addLayout(btnCol, 3);  // 30% width
    vbox->addLayout(bottomRow);

    // --- Control 6: Antenna container (3 buttons) -- hidden until direct connection active ---
    // Gated: hasDirectConnection() && hasAntennaSwitch().
    // From AetherSDR src/gui/TunerApplet.cpp:buildUI() m_antContainer [@0cd4559]
    {
        m_antContainer = new QWidget(this);
        m_antContainer->setVisible(false);
        // Bench-fix 2026-05-19: reserve 24 px of layout height when visible.
        // Without an explicit minimum, the TUNE / STANDBY buttons above
        // (QSizePolicy::Expanding vertical) consumed every available pixel
        // in the column, leaving zero space for the antenna row even though
        // setVisible(true) had been called.
        m_antContainer->setMinimumHeight(24);
        m_antContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        auto* antRow = new QHBoxLayout(m_antContainer);
        antRow->setContentsMargins(0, 0, 0, 0);
        antRow->setSpacing(2);

        // From AetherSDR src/gui/TunerApplet.cpp:buildUI() makeAntBtn [@0cd4559]
        auto makeAntBtn = [this](const QString& text) {
            auto* btn = new QPushButton(text, this);
            btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            btn->setFixedHeight(22);
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { background: #1a2a3a; border: 1px solid #205070; "
                "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
                "QPushButton:hover { background: #204060; }"));
            return btn;
        };

        m_ant1Btn = makeAntBtn(QStringLiteral("ANT 1"));
        m_ant2Btn = makeAntBtn(QStringLiteral("ANT 2"));
        m_ant3Btn = makeAntBtn(QStringLiteral("ANT 3"));
        antRow->addWidget(m_ant1Btn);
        antRow->addWidget(m_ant2Btn);
        antRow->addWidget(m_ant3Btn);

        // ANT buttons: 1-indexed to match AetherSDR upstream convention.
        // From AetherSDR src/gui/TunerApplet.cpp:buildUI() ant clicked [@0cd4559]
        // R-R3-49 / R-R3-47: requestAntenna asks the Core in a remote window.
        connect(m_ant1Btn, &QPushButton::clicked, this,
                [this]() { requestAntenna(1); });
        connect(m_ant2Btn, &QPushButton::clicked, this,
                [this]() { requestAntenna(2); });
        connect(m_ant3Btn, &QPushButton::clicked, this,
                [this]() { requestAntenna(3); });

        vbox->addWidget(m_antContainer);
    }

    vbox->addStretch();
    root->addWidget(body);

    // Phase 3P-II Phase 4 Task 95: apply persisted antenna labels from AppSettings.
    refreshAntennaLabels();
}

// Phase 3P-II Phase 4 Task 95: read TGXL_Ant{1,2,3}_Label from AppSettings
// and apply to the three antenna QPushButtons.  Default to "ANT N" when empty.
void TunerApplet::refreshAntennaLabels()
{
    const auto& s = AppSettings::instance();
    struct { QPushButton* btn; const char* key; const char* def; } rows[] = {
        { m_ant1Btn, "TGXL_Ant1_Label", "ANT 1" },
        { m_ant2Btn, "TGXL_Ant2_Label", "ANT 2" },
        { m_ant3Btn, "TGXL_Ant3_Label", "ANT 3" },
    };
    for (auto& row : rows) {
        if (!row.btn) { continue; }
        const QString val = s.value(QString::fromLatin1(row.key), QString{}).toString().trimmed();
        row.btn->setText(val.isEmpty() ? QString::fromLatin1(row.def) : val);
    }
}

// Phase 3P-II Phase 4 Task 95: live-update a single antenna button label.
// index is 1..3; label is the new text (empty string resets to "ANT N").
void TunerApplet::onAntennaLabelChanged(int index, const QString& label)
{
    QPushButton* btn = nullptr;
    QString def;
    switch (index) {
    case 1: btn = m_ant1Btn; def = QStringLiteral("ANT 1"); break;
    case 2: btn = m_ant2Btn; def = QStringLiteral("ANT 2"); break;
    case 3: btn = m_ant3Btn; def = QStringLiteral("ANT 3"); break;
    default: return;
    }
    if (btn) {
        btn->setText(label.trimmed().isEmpty() ? def : label.trimmed());
    }
}

// Phase 3P-II review fix C1: update m_currentBand so Save/Recall/Clear
// context-menu actions operate on the correct (antenna, band) slot.
// Wired by MainWindow::wireSliceToSpectrum to SliceModel::bandChanged.
void TunerApplet::setBand(Band band)
{
    m_currentBand = band;
}

QString TunerApplet::tuneButtonTextForTesting() const
{
    return m_tuneBtn ? m_tuneBtn->text() : QString{};
}

bool TunerApplet::actuatingControlsEnabledForTesting() const
{
    return m_tuneBtn && m_tuneBtn->isEnabled()
        && m_operateBtn && m_operateBtn->isEnabled()
        && m_ant1Btn && m_ant1Btn->isEnabled()
        && m_ant2Btn && m_ant2Btn->isEnabled()
        && m_ant3Btn && m_ant3Btn->isEnabled();
}

QPushButton* TunerApplet::antennaButtonForTesting(int port) const
{
    switch (port) {
    case 1: return m_ant1Btn;
    case 2: return m_ant2Btn;
    case 3: return m_ant3Btn;
    default: return nullptr;
    }
}

bool TunerApplet::remoteTunerControl() const
{
    if (!m_model || m_model->role() != RadioModel::Role::Remote) { return false; }
    const IStationLink* link = m_model->stationLink();
    return link && link->tgxlControlAvailable();
}

bool TunerApplet::remoteRelayControl() const
{
    if (!remoteTunerControl()) { return false; }
    const IStationLink* link = m_model->stationLink();
    return link && link->tgxlFullControlAvailable();
}

bool TunerApplet::remoteWindow() const
{
    return m_model && m_model->role() == RadioModel::Role::Remote;
}

bool TunerApplet::remoteTuneControl() const
{
    if (!remoteWindow()) { return false; }
    const IStationLink* link = m_model->stationLink();
    return link && link->tgxlAutotuneAvailable();
}

QString TunerApplet::noRemoteTuneReason()
{
    return QStringLiteral("The connected Core does not offer Tuner Genius tuning to this app.");
}

QString TunerApplet::noRemoteTunerReason()
{
    return QStringLiteral("The connected Core does not offer Tuner Genius controls to this app.");
}

QString TunerApplet::noRemoteRelayReason()
{
    return QStringLiteral("The connected Core does not offer Tuner Genius relay control to this app.");
}

// R-R3-49 (parity Task 8): a remote window asks the Core, whose tuner moves
// the relay; the bar follows the tuner's report (relayC1/relayL/relayC2),
// never the wheel.
void TunerApplet::requestRelayMove(int relay, int direction)
{
    if (remoteRelayControl()) {
        if (!coreOnAir() && m_tunerModel && m_tunerModel->hasDirectConnection()) {
            m_model->stationLink()->requestTgxlRelayMove(relay, direction > 0 ? 1 : -1);
        }
        return;
    }
    if (remoteWindow()) {
        return;  // GUI-I4: never this computer's own tuner
    }
    if (m_transmitPermitted && m_tunerModel && !refuseLocalSwitchOnAir()) {
        m_tunerModel->adjustRelay(relay, direction);
    }
}

RelayBar* TunerApplet::relayBarForTesting(int relay) const
{
    switch (relay) {
    case 0: return m_c1Bar;
    case 1: return m_lBar;
    case 2: return m_c2Bar;
    default: return nullptr;
    }
}

bool TunerApplet::coreOnAir() const
{
    // R-R3-49 (parity Task 1): the window's one on-the-air state. The
    // mirrored transmit model's mox latch is not read: the Core never
    // writes it while its controller exists.
    return m_model && m_model->isCoreOnAir();
}

QString TunerApplet::onAirReason()
{
    return RadioModel::onAirReason();
}

bool TunerApplet::refuseLocalSwitchOnAir()
{
    // Group B fix wave (M5): this computer's own tuner, refused by the
    // Core's own on-the-air rule (RadioModel::stationOnAirRefusal), which
    // also covers the hand-back to receive after MOX. Parity mini-round
    // (ruling c): a refused click says why, as a remote window's Core does.
    return m_model && m_model->role() != RadioModel::Role::Remote
        && m_model->refuseLocalAccessorySwitchOnAir(QStringLiteral("tgxl"));
}

// R-R3-49 / R-R3-47: a remote window asks the Core, which switches its own
// tuner; the button follows the Core's reported antenna, not the click.
void TunerApplet::requestAntenna(int port)
{
    if (remoteTunerControl()) {
        if (!coreOnAir()) {
            m_model->stationLink()->requestTgxlAntenna(port);
        }
        return;
    }
    if (remoteWindow()) {
        return;  // GUI-I4: never this computer's own tuner
    }
    if (m_transmitPermitted && m_tunerModel && !refuseLocalSwitchOnAir()) {
        m_tunerModel->setAntennaA(port);
    }
}

bool TunerApplet::staleIndicatorVisibleForTesting() const
{
    return m_staleLabel && !m_staleLabel->isHidden();
}

void TunerApplet::setTransmitPermitted(bool permitted, const QString& reason)
{
    if (m_transmitPermitted == permitted && m_transmitPermissionReason == reason) {
        return;
    }

    if (!permitted && m_carrierEngagedForTgxlTune) {
        if (m_model) {
            m_model->setTune(false);
        }
        m_carrierEngagedForTgxlTune = false;
    }

    m_transmitPermitted = permitted;
    m_transmitPermissionReason = reason;
    updateActuatingControls();
}

void TunerApplet::setStationConnected(bool connected)
{
    m_stationConnected = connected;
    updateStationAvailability();
}

void TunerApplet::updateStationAvailability()
{
    if (!m_staleLabel) { return; }
    const bool remote = m_model && m_model->role() == RadioModel::Role::Remote;
    m_staleLabel->setVisible(remote && !m_stationConnected);
}

bool TunerApplet::transmitBlocked() const
{
    return m_model && m_model->moxController()
        && !m_model->moxController()->transmitBlockReason().isEmpty();
}

void TunerApplet::updateActuatingControls()
{
    const QString tooltip = m_transmitPermitted ? QString() : m_transmitPermissionReason;
    const auto updateButton = [this, &tooltip](QPushButton* button) {
        if (!button) { return; }
        button->setEnabled(m_transmitPermitted);
        button->setToolTip(tooltip);
    };
    updateButton(m_tuneBtn);
    // GUI-I4 (fix wave): a remote window's TUNE is the Core's tune; on a
    // Core that does not offer it, TUNE waits with that reason.
    if (m_tuneBtn && m_tuneBtn->isEnabled() && remoteWindow() && !remoteTuneControl()) {
        m_tuneBtn->setEnabled(false);
        m_tuneBtn->setToolTip(noRemoteTuneReason());
    }
    // Task 16 fix wave (M2): TUNE also waits on the transmit block, with its
    // reason, and with both reasons when a remote window's missing transmit
    // applies too (M6).
    if (m_tuneBtn && m_model && transmitBlocked()) {
        m_tuneBtn->setEnabled(false);
        m_tuneBtn->setToolTip(m_model->transmitBlockReasonAlongside(tooltip));
    }
    // iPhone app plan Task 77 fix round 2 (one on-air rule in both windows):
    // TUNE switches the Power Genius to standby first, so on the air it
    // waits with the reason, as OPERATE and ANT do below (the Core refuses
    // it there too: RadioModel::beginTgxlAutotune, tx.tunerTune).
    if (m_tuneBtn && m_tuneBtn->isEnabled() && coreOnAir()) {
        m_tuneBtn->setEnabled(false);
        m_tuneBtn->setToolTip(onAirReason());
    }
    if (m_tuneBtn) {
        m_tuneBtn->setAccessibleDescription(m_tuneBtn->toolTip());
    }

    // R-R3-49 / R-R3-47: in a remote window on a Core that offers them, ANT
    // and OPERATE ask the Core (they key nothing, so the receive-only
    // station does not grey them) and wait while the radio is on the air.
    // Otherwise they keep the transmit permission, with its reason.
    bool switchable = m_transmitPermitted;
    QString switchTip = tooltip;
    if (remoteTunerControl()) {
        const bool onAir = coreOnAir();
        switchable = !onAir;
        switchTip = onAir ? onAirReason() : QString();
    } else if (remoteWindow() && m_transmitPermitted) {
        // GUI-I4 (fix wave): never this computer's own tuner. Without the
        // transmit permission the transmit reason above stands.
        switchable = false;
        switchTip = noRemoteTunerReason();
    } else if (m_transmitPermitted && coreOnAir()) {
        // Group B fix wave (M5): a local window's, the same rule.
        switchable = false;
        switchTip = onAirReason();
    }
    for (QPushButton* button : {m_operateBtn, m_ant1Btn, m_ant2Btn, m_ant3Btn}) {
        if (!button) { continue; }
        button->setEnabled(switchable);
        button->setToolTip(switchTip);
    }

    // R-R3-49 (parity Task 8): on a Core that moves them for this app, the
    // relay bars key nothing and wait only while the radio is on the air.
    bool relayCommandsEnabled = m_transmitPermitted && m_tunerModel
        && m_tunerModel->hasDirectConnection();
    QString relayTip;
    if (remoteRelayControl()) {
        const bool onAir = coreOnAir();
        relayCommandsEnabled = !onAir && m_tunerModel && m_tunerModel->hasDirectConnection();
        relayTip = onAir ? onAirReason() : QString();
    } else if (remoteWindow() && m_transmitPermitted) {
        // GUI-I4 (fix wave): never this computer's own tuner. Without the
        // transmit permission the bars are already off.
        relayCommandsEnabled = false;
        relayTip = remoteTunerControl() ? noRemoteRelayReason() : noRemoteTunerReason();
    } else if (m_transmitPermitted && coreOnAir()) {
        // Group B fix wave (M5): a local window's relays, the same rule.
        relayCommandsEnabled = false;
        relayTip = onAirReason();
    }
    for (RelayBar* bar : {m_c1Bar, m_lBar, m_c2Bar}) {
        if (!bar) { continue; }
        bar->setScrollEnabled(relayCommandsEnabled);
        bar->setToolTip(relayTip);
    }
}

void TunerApplet::setTunerModel(TunerModel* model)
{
    // From AetherSDR src/gui/TunerApplet.cpp:setTunerModel [@0cd4559]
    if (m_tunerModel == model) return;
    m_tunerModel = model;
    if (!m_tunerModel) return;

    // Relay bars: sync on any relay change.
    // Phase 3P-II Phase 4 Task 89: also cache last relay values for Save.
    connect(m_tunerModel, &TunerModel::relayChanged, this, [this]() {
        if (!m_tunerModel) return;
        m_c1Bar->setValue(m_tunerModel->relayC1());
        m_lBar->setValue(m_tunerModel->relayL());
        m_c2Bar->setValue(m_tunerModel->relayC2());
        m_lastC1 = m_tunerModel->relayC1();
        m_lastL  = m_tunerModel->relayL();
        m_lastC2 = m_tunerModel->relayC2();
    });

    // State changes -> refresh OPERATE button label + color.
    connect(m_tunerModel, &TunerModel::stateChanged, this, &TunerApplet::syncFromModel);

    // Tuning state: red button + "TUNING..." + post-tune SWR flash.
    // From AetherSDR src/gui/TunerApplet.cpp:setTunerModel() tuningChanged [@0cd4559]
    //
    // NereusSDR adds carrier orchestration here:
    //   * on tuning=1 from a TGXL hardware TUNE press (operator pushed the
    //     button on the device) we engage the local tune-carrier so TGXL
    //     sees RF and doesn't abort with "low RF power". When the TUNE
    //     click came from our applet the carrier is already engaged via the
    //     click handler above -- the !isManualMox() gate prevents a double
    //     engage.
    //   * on tuning=0 we drop the carrier ONLY if we engaged it for this
    //     cycle (m_carrierEngagedForTgxlTune). If the operator engaged TUN
    //     manually via TxApplet, the cycle finishing must not drop their
    //     carrier.
    connect(m_tunerModel, &TunerModel::tuningChanged, this, [this](bool tuning) {
        if (tuning) {
            m_wasTuning = true;
            // Tell the short-watchdog (in the TUNE click handler above)
            // that TGXL has confirmed it's running the sweep, so the
            // 3 s "did TGXL respond at all" guard becomes a no-op. The
            // long sweep cycle is bounded by the matching
            // tuningChanged(false) below, not by the watchdog.
            m_tgxlEnteredTuning = true;
            m_postTuneCapture = false;
            m_postTuneTimer->stop();
            m_tuneSwr = 999.0f;  // reset high so capture tracking works
            m_tuneBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background: #cc2222; border: 1px solid #ff4444; "
                "border-radius: 3px; color: #ffffff; font-size: 10px; font-weight: bold; }"));
            m_tuneBtn->setText(QStringLiteral("TUNING..."));

            // Engage local CW tune carrier + standby PGXL for a hardware-
            // initiated TGXL tune cycle.
            //
            // 2026-05-20 pcap-driven fix (flex-tgxl-direct-CONTROL.pcapng):
            // The TGXL hardware TUNE button does NOT send `transmit tune on`
            // upfront -- it pushes `tuning=1` via its :9010 status stream,
            // which lands here as TunerModel::tuningChanged(true). The
            // prior code just called m_model->setTune(true) which engaged
            // the CW carrier but DID NOT standby PGXL. With PGXL in OPERATE
            // the amp amplified the carrier into TGXL's directional couplers
            // -- TGXL saw ~450 W and aborted with "high RF power". With PGXL
            // in STANDBY the carrier reached TGXL at tunepower (~10 W) but
            // TGXL still complained "low input power" because no coordinated
            // sequence had happened.
            //
            // Route through startTgxlAutotune(fromHardware=true) instead so
            // the same orchestration the TunerApplet TUNE click uses
            // (standby PGXL -> engage TUN after STANDBY ACK -> restore PGXL
            // on tune complete) covers the hardware-initiated path too.
            // fromHardware=true skips the redundant `autotune` cmd on :9010
            // because TGXL is already running its own sweep.
            //
            // The recursion guard in startTgxlAutotune handles the case
            // where TunerApplet TUNE click already started the cycle
            // (m_tgxlAutotuneInProgress is true) and TGXL then echoes by
            // pushing tuning=1; that re-entry is detected and ignored.
            const bool localOrchestration = m_transmitPermitted && m_model
                && m_model->role() == RadioModel::Role::Local
                && !m_model->receiveOnlyStationPolicy();
            if (localOrchestration && transmitBlocked()) {
                // Task 16 fix wave 2 (Minor 3): a press on the TGXL itself
                // while transmit is blocked keys nothing, and the operator
                // is told why: startTgxlAutotune refuses before it touches
                // the amplifier or the tuner and emits tuneRefused with the
                // reason, as the applet's TUNE does. No carrier is ours to
                // drop when the tuner finishes.
                m_model->startTgxlAutotune(/*fromHardware=*/true);
            } else if (localOrchestration && !m_carrierEngagedForTgxlTune) {
                m_carrierEngagedForTgxlTune = true;
                m_model->startTgxlAutotune(/*fromHardware=*/true);
            }
        } else {
            // Restore normal style.
            m_tuneBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background: #1a3a5a; border: 1px solid #205070; "
                "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
                "QPushButton:hover { background: #204060; }"));

            // Drop our orchestrated carrier (and ONLY ours) when TGXL
            // reports tune complete. An operator's manual TUN (engaged via
            // TxApplet) remains untouched -- m_carrierEngagedForTgxlTune
            // is false in that case. Routes through RadioModel::setTune so
            // the G.4 orchestrator unwinds the gen1 tone, restores power,
            // and releases MOX in the correct order.
            if (m_carrierEngagedForTgxlTune) {
                if (m_model) {
                    m_model->setTune(false);
                }
                m_carrierEngagedForTgxlTune = false;
            }

            // Don't display result immediately -- the final settled SWR from the TGXL
            // often arrives after tuning=0 via TCP.  Start a short capture window.
            // From AetherSDR src/gui/TunerApplet.cpp:setTunerModel() post-tune window [@0cd4559]
            if (m_wasTuning) {
                m_wasTuning = false;
                m_postTuneCapture = true;
                m_tuneSwr = 999.0f;  // reset -- we want the post-tune value, not the sweep
                m_tuneBtn->setText(QStringLiteral("TUNING..."));
                m_postTuneTimer->start();
            } else {
                m_tuneBtn->setText(QStringLiteral("TUNE"));
            }
        }
    });

    // Forward power + SWR from direct TGXL connection.
    connect(m_tunerModel, &TunerModel::metersChanged,
            this, &TunerApplet::updateMeters);

    // Direct connection active -> enable relay bar scrolling.
    // From AetherSDR src/gui/TunerApplet.cpp:setTunerModel() directConnectionChanged [@0cd4559]
    auto updateScrollEnabled = [this]() {
        updateActuatingControls();
    };
    connect(m_tunerModel, &TunerModel::directConnectionChanged, this, updateScrollEnabled);
    updateScrollEnabled();

    // Antenna switch: show buttons only when direct connection active AND
    // the TGXL reports antA (models without a switch never send antA).
    // From AetherSDR src/gui/TunerApplet.cpp:setTunerModel() antVisible [@0cd4559]
    auto updateAntVisible = [this]() {
        if (!m_tunerModel) return;
        m_antContainer->setVisible(m_tunerModel->hasDirectConnection()
                                   && m_tunerModel->hasAntennaSwitch());
    };
    connect(m_tunerModel, &TunerModel::directConnectionChanged, this, updateAntVisible);
    // Also re-evaluate when state keys (including one_by_three) arrive via
    // applyStatus().  hasAntennaSwitch() is NOTIFY stateChanged, so the first
    // status frame that sets one_by_three=1 only fires stateChanged, not
    // directConnectionChanged.  Without this wire the antenna container stays
    // hidden even when the 1x3 TGXL is live.
    connect(m_tunerModel, &TunerModel::stateChanged, this, updateAntVisible);
    connect(m_tunerModel, &TunerModel::antennaAChanged, this,
            [this, updateAntVisible](int antA) {
                updateAntVisible();
                updateAntennaButtons(antA);
                // Phase 3P-II review fix C1: keep m_currentAntenna in sync so
                // Save/Recall/Clear context-menu actions operate on the correct
                // (antenna, band) slot.  TunerModel uses 0-indexed antA;
                // TuneMemoryStore uses 1-indexed -- mirror RadioModel.cpp:8936
                // convention where antennaA() + 1 is the store key.
                m_currentAntenna = antA + 1;
            });
    updateAntVisible();
    updateAntennaButtons(m_tunerModel->antennaA());
    // Seed m_currentAntenna from the model's current value.
    m_currentAntenna = m_tunerModel->antennaA() + 1;

    // Initial full sync.
    syncFromModel();
}

void TunerApplet::setPowerScale(int maxWatts, bool hasAmplifier)
{
    // From AetherSDR src/gui/TunerApplet.cpp:setPowerScale [@0cd4559]
    if (hasAmplifier) {
        // PGXL: 0-2000 W, red > 1500 W
        m_fwdPowerGauge->setRange(0.0, 2000.0);
        m_fwdPowerGauge->setYellowStart(1500.0);
        m_fwdPowerGauge->setRedStart(1500.0);
    } else if (maxWatts > 100) {
        // Aurora (500 W): 0-600 W, red > 500 W
        m_fwdPowerGauge->setRange(0.0, 600.0);
        m_fwdPowerGauge->setYellowStart(500.0);
        m_fwdPowerGauge->setRedStart(500.0);
    } else {
        // Barefoot radio: 0-200 W, red > 125 W
        m_fwdPowerGauge->setRange(0.0, 200.0);
        m_fwdPowerGauge->setYellowStart(125.0);
        m_fwdPowerGauge->setRedStart(125.0);
    }
}

void TunerApplet::syncFromModel()
{
    // From AetherSDR src/gui/TunerApplet.cpp:syncFromModel [@0cd4559]
    if (!m_tunerModel) return;

    // Relay bars
    m_c1Bar->setValue(m_tunerModel->relayC1());
    m_lBar->setValue(m_tunerModel->relayL());
    m_c2Bar->setValue(m_tunerModel->relayC2());

    // Operate/Bypass/Standby cycle button -- 3-state display:
    //   operate=1, bypass=0  -> OPERATE  (green)
    //   operate=1, bypass=1  -> BYPASS   (orange)
    //   operate=0            -> STANDBY  (default blue)
    // From AetherSDR src/gui/TunerApplet.cpp:syncFromModel operate/bypass display [@0cd4559]
    if (m_tunerModel->isOperate() && !m_tunerModel->isBypass()) {
        m_operateBtn->setText(QStringLiteral("OPERATE"));
        m_operateBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: #006030; border: 1px solid #008040; "
            "border-radius: 3px; color: #ffffff; font-size: 10px; font-weight: bold; }"
            "QPushButton:hover { background: #007040; }"));
    } else if (m_tunerModel->isOperate() && m_tunerModel->isBypass()) {
        m_operateBtn->setText(QStringLiteral("BYPASS"));
        m_operateBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: #8a6000; border: 1px solid #a07000; "
            "border-radius: 3px; color: #ffffff; font-size: 10px; font-weight: bold; }"
            "QPushButton:hover { background: #9a7000; }"));
    } else {
        m_operateBtn->setText(QStringLiteral("STANDBY"));
        m_operateBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background: #1a3a5a; border: 1px solid #205070; "
            "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
            "QPushButton:hover { background: #204060; }"));
    }
}

void TunerApplet::cycleOperateState()
{
    // From AetherSDR src/gui/TunerApplet.cpp:cycleOperateState [@0cd4559]
    // Cycle: OPERATE -> BYPASS -> STANDBY -> OPERATE
    // (AetherSDR comment at line 184: "OPERATE -> BYPASS -> STANDBY -> OPERATE")
    // R-R3-49 / R-R3-47: a remote window reads the Core's reported state
    // and asks the Core for the next one; its tuner switches, and the button
    // follows its report.
    if (!m_tunerModel) return;
    const bool remote = remoteTunerControl();
    // GUI-I4 (fix wave): a remote window switches only through the Core.
    if (!remote && remoteWindow()) { return; }
    if (remote ? coreOnAir() : (!m_transmitPermitted || refuseLocalSwitchOnAir())) return;
    IStationLink* link = remote ? m_model->stationLink() : nullptr;
    const auto setBypass = [this, link](bool on) {
        if (link) { link->requestTgxlBypass(on); } else { m_tunerModel->setBypass(on); }
    };
    const auto setOperate = [this, link](bool on) {
        if (link) { link->requestTgxlOperate(on); } else { m_tunerModel->setOperate(on); }
    };

    if (m_tunerModel->isOperate() && !m_tunerModel->isBypass()) {
        // Currently OPERATE -> go to BYPASS
        setBypass(true);
    } else if (m_tunerModel->isOperate() && m_tunerModel->isBypass()) {
        // Currently BYPASS -> go to STANDBY
        setOperate(false);
    } else {
        // Currently STANDBY -> go to OPERATE
        // R-R3-49 fix wave: a Core at remoteTgxlControlVersion 3 applies
        // setTgxlOperate on whole (bypass off, then operate on, one
        // command), so a key between two requests cannot leave the tuner
        // half-changed. An older Core gets the two requests as before.
        if (!link || !link->tgxlOperateAppliesWhole()) {
            setBypass(false);
        }
        setOperate(true);
    }
}

void TunerApplet::updateMeters(float fwdPower, float swr)
{
    // From AetherSDR src/gui/TunerApplet.cpp:updateMeters [@0cd4559]
    m_fwdPower = fwdPower;
    m_swr = swr;
    m_fwdPowerGauge->setValue(fwdPower);
    m_swrGauge->setValue(swr);

    // During the post-tune capture window, record the last non-idle SWR.
    // The TGXL reports the settled SWR shortly after tuning=0 arrives;
    // we take the last value > 1.01 (idle/no-RF reads ~1.00).
    // From AetherSDR src/gui/TunerApplet.cpp:updateMeters post-tune capture [@0cd4559]
    if (m_postTuneCapture && swr > 1.01f) {
        m_tuneSwr = swr;
        m_tuneBtn->setText(QString("SWR %1").arg(swr, 0, 'f', 2));
    }
}

void TunerApplet::updateAntennaButtons(int antA)
{
    // antA is 0-indexed: 0=ANT1, 1=ANT2, 2=ANT3
    // From AetherSDR src/gui/TunerApplet.cpp:updateAntennaButtons [@0cd4559]
    static constexpr const char* kDefault =
        "QPushButton { background: #1a2a3a; border: 1px solid #205070; "
        "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
        "QPushButton:hover { background: #204060; }";
    static constexpr const char* kActive =
        "QPushButton { background: #006030; border: 1px solid #008040; "
        "border-radius: 3px; color: #ffffff; font-size: 10px; font-weight: bold; }";

    m_ant1Btn->setStyleSheet(antA == 0 ? kActive : kDefault);
    m_ant2Btn->setStyleSheet(antA == 1 ? kActive : kDefault);
    m_ant3Btn->setStyleSheet(antA == 2 ? kActive : kDefault);
}


// Phase 3P-II Phase 4 Task 89: update TGXL connected flag for context menu.
void TunerApplet::setTgxlConnected(bool connected)
{
    m_tgxlConnected = connected;
}

// Phase 3P-II Phase 4 Task 89: right-click context menu.
// Menu structure per design doc ss5.9:
//   Open TGXL Advanced...                     -> navigationRequested("tgxlAdvanced")
//   (separator)
//   Save current tune memory                  -> m_tuneStore->store(currentMem())
//   Recall tune memory for current (ant,band) -> apply stored positions (if any)
//   Clear tune memory for current (ant,band)  -> m_tuneStore->clear(ant, band)
//   (separator)
//   Disconnect / Connect                    -> connectionToggleRequested()
//   Copy diagnostics to clipboard             -> diagnosticsCopyRequested()
void TunerApplet::contextMenuEvent(QContextMenuEvent* ev)
{
    QMenu* menu = buildContextMenu(this);
    menu->exec(ev->globalPos());
    menu->deleteLater();
}

QMenu* TunerApplet::buildContextMenu(QObject* menuParent)
{
    auto* menu = new QMenu(qobject_cast<QWidget*>(menuParent));
    // GUI-M3 (fix wave): the disabled items carry their reasons as tooltips.
    menu->setToolTipsVisible(true);

    // Open TGXL Advanced...
    // R-R3-49 (parity Task 8): Setup > CAT & Network > 4O3A > Tuner Genius
    // XL, in local and remote windows (a remote window's tab shows the
    // Core's records and settings).
    auto* openAdvancedAction = menu->addAction(QStringLiteral("Open TGXL Advanced..."));
    connect(openAdvancedAction, &QAction::triggered, this, [this]() {
        emit navigationRequested(QStringLiteral("tgxlAdvanced"));
    });

    menu->addSeparator();

    // Save current tune memory
    auto* saveAction = menu->addAction(QStringLiteral("Save current tune memory"));
    if (!m_tuneStore) { saveAction->setEnabled(false); }
    connect(saveAction, &QAction::triggered, this, [this]() {
        if (m_tuneStore) {
            m_tuneStore->store(currentMem());
        }
    });

    // Recall tune memory for current (ant, band)
    // Note: TunerModel only exposes adjustRelay(relay, dir) for incremental
    // updates; absolute position setting via TGXL requires a protocol-level
    // "relay set" command not yet in TunerModel (Phase 3P-II follow-up).
    // For now, recall loads the stored values into the local cache so a
    // subsequent auto-tune starts from the memorised position rather than
    // the TGXL's default. Absolute apply deferred to the TGXL "set relay"
    // command when that API lands.
    // R-R3-49 (parity Task 8): recall copies the stored values into the
    // bars and sends nothing, so a remote window offers it too (its store
    // holds the Core's tune memory).
    auto* recallAction = menu->addAction(QStringLiteral("Recall tune memory"));
    const bool remoteWindow = m_model && m_model->role() == RadioModel::Role::Remote;
    const bool recallPermitted = m_transmitPermitted || remoteWindow;
    if (!m_tuneStore || !recallPermitted) {
        recallAction->setEnabled(false);
        if (!recallPermitted) {
            recallAction->setToolTip(m_transmitPermissionReason);
        }
    }
    connect(recallAction, &QAction::triggered, this, [this, recallPermitted]() {
        if (!recallPermitted || !m_tuneStore) { return; }
        auto rec = m_tuneStore->recall(m_currentAntenna, m_currentBand);
        if (!rec.has_value()) { return; }
        // Update local relay display so the operator can see the stored values.
        m_lastC1 = rec->c1;
        m_lastL  = rec->l;
        m_lastC2 = rec->c2;
        if (m_c1Bar) { m_c1Bar->setValue(rec->c1); }
        if (m_lBar)  { m_lBar->setValue(rec->l);   }
        if (m_c2Bar) { m_c2Bar->setValue(rec->c2); }
    });

    // Clear tune memory for current (ant, band)
    auto* clearAction = menu->addAction(QStringLiteral("Clear tune memory"));
    if (!m_tuneStore) { clearAction->setEnabled(false); }
    connect(clearAction, &QAction::triggered, this, [this]() {
        if (m_tuneStore) {
            m_tuneStore->clear(m_currentAntenna, m_currentBand);
        }
    });

    menu->addSeparator();

    const bool remote = m_model && m_model->role() == RadioModel::Role::Remote;
    if (remote) {
        const bool connected = m_tunerModel
            && m_tunerModel->connectionPhase() == TunerModel::ConnectionPhase::Connected;
        auto* remoteAction = menu->addAction(connected
            ? QStringLiteral("Disconnect") : QStringLiteral("Configure remote TGXL..."));
        auto* link = m_model->stationLink();
        if (connected && link && link->remoteTgxlConfigAvailable()) {
            connect(remoteAction, &QAction::triggered, this, [this]() {
                auto* currentLink = m_model ? m_model->stationLink() : nullptr;
                if (currentLink && currentLink->remoteTgxlConfigAvailable()) {
                    currentLink->requestDisconnectTgxl();
                }
            });
        } else {
            if (connected) {
                remoteAction->setEnabled(false);
                remoteAction->setToolTip(QStringLiteral("Remote TGXL control is unavailable on this Core."));
            }
            connect(remoteAction, &QAction::triggered, this, [this]() {
                emit navigationRequested(QStringLiteral("peripherals"));
            });
        }
    } else {
        const QString toggleLabel = m_tgxlConnected
            ? QStringLiteral("Disconnect") : QStringLiteral("Connect");
        auto* toggleAction = menu->addAction(toggleLabel);
        connect(toggleAction, &QAction::triggered, this, [this]() {
            emit connectionToggleRequested();
        });
    }

    // Copy diagnostics to clipboard
    auto* copyDiagAction = menu->addAction(QStringLiteral("Copy diagnostics to clipboard"));
    connect(copyDiagAction, &QAction::triggered, this, [this]() {
        emit diagnosticsCopyRequested();
    });

    return menu;
}

QString TunerApplet::coreDiagnosticsText(RadioModel* model)
{
    const TunerModel* tuner = model ? model->tunerModel() : nullptr;
    const AccessoryDataModel* data = model ? model->accessoryDataModel() : nullptr;
    if (!tuner) { return QString(); }
    const auto time = [](qint64 ms) {
        return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms).toString(Qt::ISODate)
                      : QStringLiteral("--");
    };
    const bool connected = tuner->connectionPhase() == TunerModel::ConnectionPhase::Connected;
    QString text = QStringLiteral("TGXL Diagnostics (the Core's connection)\n");
    text += QStringLiteral("Connected: %1\n").arg(connected ? QStringLiteral("Yes")
                                                              : QStringLiteral("No"));
    text += QStringLiteral("IP: %1\n").arg(tuner->tgxlIp().isEmpty() ? QStringLiteral("--")
                                                                     : tuner->tgxlIp());
    text += QStringLiteral("Address: %1:%2\n")
                .arg(tuner->configuredHost().isEmpty() ? QStringLiteral("--")
                                                       : tuner->configuredHost())
                .arg(tuner->configuredPort());
    text += QStringLiteral("Model: %1\nSerial: %2\nFirmware: %3\n")
                .arg(tuner->deviceModel(), tuner->deviceSerial(), tuner->deviceVersion());
    if (!tuner->connectionError().isEmpty()) {
        text += QStringLiteral("Last error: %1\n").arg(tuner->connectionError());
    }
    if (data) {
        text += QStringLiteral("Connected since: %1\n").arg(time(data->tgxlConnectedSinceMs()));
        text += QStringLiteral("Last response time: %1 ms\n").arg(data->tgxlLastRttMs());
        text += QStringLiteral("Missed keepalives: %1\n").arg(data->tgxlKeepaliveMissed());
        text += QStringLiteral("Reconnects: %1\n").arg(data->tgxlReconnectCount());
        text += QStringLiteral("Lines in/out: %1 / %2\n")
                    .arg(data->tgxlFramesIn()).arg(data->tgxlFramesOut());
        text += QStringLiteral("Bytes in/out: %1 / %2\n")
                    .arg(data->tgxlBytesIn()).arg(data->tgxlBytesOut());
        text += QStringLiteral("Last line: %1\n").arg(time(data->tgxlLastFrameMs()));
    }
    return text;
}

TuneMemory TunerApplet::currentMem() const
{
    TuneMemory mem;
    mem.antenna   = m_currentAntenna;
    mem.band      = m_currentBand;
    mem.c1        = m_lastC1;
    mem.l         = m_lastL;
    mem.c2        = m_lastC2;
    mem.savedAtMs = QDateTime::currentMSecsSinceEpoch();
    return mem;
}
} // namespace NereusSDR
