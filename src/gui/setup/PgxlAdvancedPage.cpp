// =================================================================
// src/gui/setup/PgxlAdvancedPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-native Setup -> Network -> PGXL Advanced page.
//
// Phase 3P-II Phase 4 Tasks 78-84.
//
// Design reference:
//   docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-design.md
//   sections 5.6.1 through 5.6.6 and footer.
//
// AI tooling: Anthropic Claude Code.
//
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: the counters are the
//                                    model's (the Core's in a remote
//                                    window); in a remote window the page is
//                                    a view of the Core's output limit,
//                                    counters and fault history plus its
//                                    commands (setPgxlPowerCap,
//                                    clearAccessoryFaults); each fault row
//                                    carries its plain words. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: a remote window
//                                    builds every section. The amp's own
//                                    settings (name, bias, fan, LED,
//                                    network, Save & Reboot, Revert) go to
//                                    the Core as typed requests, which it
//                                    sends the amp as this page's own
//                                    commands; the page asks the same
//                                    Save & Reboot question and, before
//                                    network changes, the Network section's
//                                    own warning; the amp's answers, its
//                                    values and the Core's refusals show on
//                                    the page. Pairing settings reach the
//                                    Core as station settings. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22 fix wave: a fixed
//                                    network setting needs an address and a
//                                    netmask (both windows); the remote
//                                    window's network warning and question
//                                    in words true there; only this
//                                    device's refusals reload the page.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: a local window asks
//                                    the same plain question as a remote one
//                                    before applying network settings
//                                    (operator decision 2026-09-24).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-R3-49 (parity Task 9, operator
//                                    amendment 2026-09-25): a local
//                                    window's tab gets an Operate button
//                                    beside the state badge, as a remote
//                                    window's tab has. It sends the local
//                                    applet's own line through this
//                                    computer's PgxlConnection and reads
//                                    Operate or Standby from the amp's
//                                    report. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  iPhone app plan Task 77 fix round 3
//                                    (R-IOS-02, R-IOS-03, R-IOS-13):
//                                    Operate waits while a Tuner Genius
//                                    cycle runs; a faulted amp is offered
//                                    Standby. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Task 77 fix round 4: Standby while
//                                    operate=1 is unconfirmed. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-29 -- R-R3-49 / R-IOS-18: Setup description version 15 ids.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: Fix wave RD-I11: the nickname field takes one word (no
//               spaces or '='), with a one-word placeholder. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: Fix round 1: a saved, mirrored or device-read name with
//               spaces is offered with underscores, so the one-word box
//               takes edits. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "PgxlAdvancedPage.h"

#include <QAbstractTableModel>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QTableView>
#include <QVBoxLayout>
#include <QVariant>

#include "../../core/AppSettings.h"
#include "../../core/ConnectionDiagnostics.h"
#include "../../core/FaultLog.h"
#include "../../core/StationDeviceSettings.h"
#include "../../core/PgxlConnection.h"
#include "../../core/session/IStationLink.h"
#include "../../core/StationAccessoryData.h"
#include "../../models/AccessoryDataModel.h"
#include "../../models/AccessorySettingsModel.h"
#include "../../models/AmplifierModel.h"
#include "../../models/RadioModel.h"
#include "../OperatorReasonText.h"
#include "../PgxlSaveRebootDialog.h"

namespace NereusSDR {

// ---------------------------------------------------------------------------
// FaultLogTableModel (private inline class)
// ---------------------------------------------------------------------------
// Maps FaultLog::events() into a QAbstractTableModel for the QTableView.
// 6 columns: When, State, FWD W, SWR, Temp C, Likely cause.
// ---------------------------------------------------------------------------

class FaultLogTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit FaultLogTableModel(FaultLog* faultLog, QObject* parent = nullptr)
        : QAbstractTableModel(parent), m_faultLog(faultLog)
    {
        connect(m_faultLog, &FaultLog::changed, this, &FaultLogTableModel::onFaultLogChanged);
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        if (parent.isValid()) {
            return 0;
        }
        return m_faultLog->events().size();
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        if (parent.isValid()) {
            return 0;
        }
        return 6;
    }

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override
    {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
            return QVariant();
        }
        switch (section) {
        case 0: return QStringLiteral("When");
        case 1: return QStringLiteral("State");
        case 2: return QStringLiteral("FWD W");
        case 3: return QStringLiteral("SWR");
        case 4: return QStringLiteral("Temp C");
        case 5: return QStringLiteral("Likely Cause");
        default: return QVariant();
        }
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid()
            || (role != Qt::DisplayRole && role != Qt::ToolTipRole)) {
            return QVariant();
        }
        const auto events = m_faultLog->events();
        if (index.row() >= events.size()) {
            return QVariant();
        }
        const FaultEvent& ev = events.at(index.row());
        // R-R3-47: every row says what happened in plain words.
        if (role == Qt::ToolTipRole) {
            return ev.text;
        }
        switch (index.column()) {
        case 0: {
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(ev.whenMs);
            return dt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
        }
        case 1: return ev.state;
        case 2: return QString::number(static_cast<double>(ev.fwdAtFaultW), 'f', 1);
        case 3: return QString::number(static_cast<double>(ev.swrAtFault), 'f', 2);
        case 4: return QString::number(static_cast<double>(ev.tempAtFaultC), 'f', 1);
        case 5: return ev.likelyCause;
        default: return QVariant();
        }
    }

private slots:
    void onFaultLogChanged()
    {
        beginResetModel();
        endResetModel();
    }

private:
    FaultLog* m_faultLog{nullptr};
};

// ---------------------------------------------------------------------------
// PgxlAdvancedPage
// ---------------------------------------------------------------------------

PgxlAdvancedPage::PgxlAdvancedPage(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    // R-R3-47 / R-R3-22: the model's counters (bound to its own connection
    // in a local window, the Core's in a remote one); a local instance only
    // when the page is built without a model (unit tests).
    , m_diagnostics(model ? model->pgxlDiagnostics() : new ConnectionDiagnostics(this))
    // Phase 3P-II Phase 4 Task 94: FaultLog is now owned by RadioModel (shared instance).
    // Use m_model->pgxlFaultLog() when m_model is non-null; fall back to a local instance
    // (same key) when m_model is null (unit-test construction without a live RadioModel).
    , m_faultLog(model ? model->pgxlFaultLog()
                       : new FaultLog(QStringLiteral("PGXL_FaultHistory"), this))
    , m_faultTableModel(new FaultLogTableModel(m_faultLog, this))
{
    // Scrollable outer shell
    auto* outerLay = new QVBoxLayout(this);
    outerLay->setContentsMargins(0, 0, 0, 0);
    outerLay->setSpacing(0);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    outerLay->addWidget(scrollArea);

    auto* inner = new QWidget;
    scrollArea->setWidget(inner);

    auto* topLay = new QVBoxLayout(inner);
    topLay->setContentsMargins(12, 12, 12, 12);
    topLay->setSpacing(16);

    if (isRemote()) {
        // R-R3-47 / R-R3-22: every section, as in a local window. The amp's
        // own settings go to the Core, which sends the amp this page's own
        // commands (a remote window never opens a connection to the amp);
        // the output limit, counters and fault history are the Core's.
        buildRemoteSections(topLay);
        topLay->addStretch();
        applySetupIds();
        return;
    }

    buildIdentitySection(topLay);
    buildHardwareSection(topLay);
    buildNetworkSection(topLay);
    buildPairingSection(topLay);
    buildDiagnosticsSection(topLay);
    buildFaultHistorySection(topLay);
    buildFooter(topLay);
    topLay->addStretch();

    // Wire PgxlConnection signals
    if (m_model) {
        PgxlConnection* pgxl = m_model->pgxlConnection();
        if (pgxl) {
            connect(pgxl, &PgxlConnection::connected,
                    this, &PgxlAdvancedPage::onPgxlConnected);
            connect(pgxl, &PgxlConnection::disconnected,
                    this, &PgxlAdvancedPage::onPgxlDisconnected);
            connect(pgxl, &PgxlConnection::statusUpdated,
                    this, &PgxlAdvancedPage::onPgxlStatusUpdated);
            connect(pgxl, &PgxlConnection::setupResponse,
                    this, &PgxlAdvancedPage::onSetupResponse);
            connect(pgxl, &PgxlConnection::ifconfResponse,
                    this, &PgxlAdvancedPage::onIfconfResponse);
            // R-R3-47: RadioModel binds its counters to this connection
            // for its whole life (StationAccessoryData publishes them).
            // R-R3-49 (parity Task 9): Operate follows the connection.
            connect(pgxl, &PgxlConnection::connected,
                    this, &PgxlAdvancedPage::updateOperateButton);
            connect(pgxl, &PgxlConnection::disconnected,
                    this, &PgxlAdvancedPage::updateOperateButton);
        }
        // R-R3-49 (parity Task 9): and the amp's reported state.
        if (AmplifierModel* amp = m_model->amplifierModel()) {
            connect(amp, &AmplifierModel::statusChanged,
                    this, &PgxlAdvancedPage::updateOperateButton);
        }
        // Group B fix wave (M5): and whether the radio is on the air.
        connect(m_model, &RadioModel::coreOnAirChanged,
                this, &PgxlAdvancedPage::updateOperateButton);
        // Task 77 fix round 3: and whether a Tuner Genius cycle runs.
        connect(m_model, &RadioModel::pgxlSwitchWaitChanged,
                this, &PgxlAdvancedPage::updateOperateButton);
    }

    connect(m_diagnostics, &ConnectionDiagnostics::changed,
            this, &PgxlAdvancedPage::onDiagnosticsChanged);
    onDiagnosticsChanged();

    // Initial UI state
    updateConnectionUi(m_model && m_model->pgxlConnection()
                       && m_model->pgxlConnection()->isConnected());
    updateOperateButton();

    applySetupIds();
}

// R-R3-49 (parity Task 9, operator amendment 2026-09-25): the local tab's
// Operate reads the action the amp's reported state allows (Standby while
// it operates, Operate otherwise), as a remote window's tab does. It is
// offered while this computer is connected to the amp. Group B fix wave
// (M5, the operator's ruling 2026-09-25): it waits while the radio is on
// the air, as the applet's OPERATE and a remote window's do, with the same
// reason.
void PgxlAdvancedPage::updateOperateButton()
{
    if (!m_operateBtn) {
        return;
    }
    const PgxlConnection* pgxl = m_model ? m_model->pgxlConnection() : nullptr;
    const AmplifierModel* amp = m_model ? m_model->amplifierModel() : nullptr;
    const bool connected = pgxl && pgxl->isConnected();
    const bool operating = amp && amp->operate();
    // Task 77 fix round 3: a faulted amp is offered Standby (operate=0),
    // which ends a changeover the fault left waiting; and the button waits
    // while a Tuner Genius cycle runs.
    // Round 4: and while operate=1 is unconfirmed, Standby too.
    const bool unconfirmed = m_model && m_model->ampOperateUnconfirmed();
    const bool faulted = (amp && amp->state() == AmplifierModel::State::Fault) || unconfirmed;
    const bool onAir = m_model && m_model->isCoreOnAir();
    const bool tuning = m_model && m_model->pgxlSwitchWaitsForTuner();
    m_operateBtn->setText(operating || faulted ? tr("Standby") : tr("Operate"));
    m_operateBtn->setEnabled(connected && !onAir && !tuning);
    m_operateBtn->setToolTip(!connected ? tr("The Power Genius is not connected.")
                             : onAir ? RadioModel::onAirReason()
                             : tuning ? RadioModel::tunerTuningReason()
                             : unconfirmed ? tr("The amplifier has not reported operate. Put it in standby.")
                             : faulted ? tr("The amplifier reports a fault. Put it in standby.")
                             : operating ? tr("Put the Power Genius in standby.")
                                         : tr("Put the Power Genius in operate."));
}

void PgxlAdvancedPage::onOperateClicked()
{
    PgxlConnection* pgxl = m_model ? m_model->pgxlConnection() : nullptr;
    const AmplifierModel* amp = m_model ? m_model->amplifierModel() : nullptr;
    if (!pgxl || !pgxl->isConnected() || !amp) {
        updateOperateButton();
        return;
    }
    // Group B fix wave (M5): refused on the air, by the Core's own rule.
    // Parity mini-round (ruling c): with the remote window's reason.
    // Task 77 fix round 3: and while a Tuner Genius cycle runs.
    // Task 77 fix round 3: a faulted amp goes to standby. Round 4: so does
    // one whose operate=1 is unconfirmed.
    const bool faulted = amp->state() == AmplifierModel::State::Fault
        || m_model->ampOperateUnconfirmed();
    const bool wantOperate = !amp->operate() && !faulted;
    if (m_model->refuseLocalAccessorySwitchOnAir(QStringLiteral("pgxl"),
                                                 /*standbyRequested=*/!wantOperate)) {
        updateOperateButton();
        return;
    }
    // The local applet's own line (MainWindow's AmpApplet::operateToggled
    // handler). Bench-fix 2026-05-19: pcap stream 11 (.19
    // PowerGeniusDesktop -> .235 PGXL :9008) shows the actually-used wire
    // command for OPERATE is `operate=1` (key=value), not bare `operate`.
    // PGXL rejected `operate` / `standby` with error 50000016 every click.
    // The button follows the amp's report, not the click.
    pgxl->sendCommand(wantOperate ? QStringLiteral("operate=1")
                                  : QStringLiteral("operate=0"));
}

PgxlAdvancedPage::~PgxlAdvancedPage() = default;

// ---------------------------------------------------------------------------
// Section builders
// ---------------------------------------------------------------------------

void PgxlAdvancedPage::buildIdentitySection(QVBoxLayout* topLay)
{
    auto* box = new QGroupBox(QStringLiteral("Identity & Status"));
    auto* form = new QFormLayout(box);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_nickname = new QLineEdit;
    m_nickname->setPlaceholderText(QStringLiteral("e.g. Shack_PGXL"));
    // RD-I11: the amp and tuner take the name as one word of a `setup`
    // line; a space or '=' would start another field. Neither can be typed.
    m_nickname->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[^\\s=]*")), m_nickname));
    m_nickname->setToolTip(QStringLiteral("One word: no spaces or equals signs."));
    form->addRow(QStringLiteral("Nickname:"), m_nickname);

    m_firmwareVersion = new QLabel(QStringLiteral("--"));
    form->addRow(QStringLiteral("Firmware:"), m_firmwareVersion);

    m_serialLabel = new QLabel(QStringLiteral("--"));
    form->addRow(QStringLiteral("Serial:"), m_serialLabel);

    m_stateBadge = new QLabel(QStringLiteral("Offline"));
    m_stateBadge->setAutoFillBackground(true);
    m_stateBadge->setAlignment(Qt::AlignCenter);
    m_stateBadge->setStyleSheet(
        QStringLiteral("background: #555; color: #ccc; border-radius: 3px; padding: 2px 6px;"));
    // R-R3-49 (parity Task 9, operator amendment 2026-09-25): Operate beside
    // the badge, as a remote window's tab has it. updateOperateButton sets
    // its words from the amp's report and whether it is offered. A remote
    // window's tab has its own Operate (FourO3APage, setPgxlOperate) above
    // this page, so the page adds none there.
    auto* stateRow = new QHBoxLayout;
    stateRow->setContentsMargins(0, 0, 0, 0);
    stateRow->addWidget(m_stateBadge);
    if (!isRemote()) {
        m_operateBtn = new QPushButton(tr("Operate"));
        m_operateBtn->setObjectName(QStringLiteral("pgxlOperateButton"));
        m_operateBtn->setEnabled(false);
        connect(m_operateBtn, &QPushButton::clicked,
                this, &PgxlAdvancedPage::onOperateClicked);
        stateRow->addWidget(m_operateBtn);
    }
    stateRow->addStretch();
    form->addRow(QStringLiteral("State:"), stateRow);

    m_meffaLabel = new QLabel(QStringLiteral("--"));
    form->addRow(QStringLiteral("MeFFA:"), m_meffaLabel);

    topLay->addWidget(box);

    // Nickname editingFinished -> writeSetup
    connect(m_nickname, &QLineEdit::editingFinished, this, [this]() {
        if (isRemote()) {
            // R-R3-47 / R-R3-22: the Core renames the amp (the same `setup
            // nickname=`) and keeps the name. One request per edit.
            if (m_updatingFromDevice || !m_nickname->isModified()) {
                return;
            }
            m_nickname->setModified(false);
            IStationLink* link = m_model->stationLink();
            const IStationLink::CommandOutcome outcome = link
                ? link->requestPgxlName(m_nickname->text().trimmed())
                : IStationLink::CommandOutcome{ false, tr("Connect to the Core first.") };
            // Follow-up 3: this page shows the Core's refusal; no toast too.
            m_model->noteAccessoryRequestShownOnPage(outcome.commandId, this);
            showRemoteOutcome(outcome.sent, outcome.reason);
            return;
        }
        if (m_model && m_model->pgxlConnection() && m_model->pgxlConnection()->isConnected()) {
            m_model->pgxlConnection()->writeSetup(
                {{QStringLiteral("nickname"), m_nickname->text().trimmed()}});
        }
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("PGXL_Nickname"), m_nickname->text().trimmed());
    });

    // Load persisted nickname
    // Fix round 1 (minor 4): a name saved with spaces before names were one
    // word is offered with underscores, so the box takes edits again (the
    // validator never accepts the old text, and editingFinished never fires).
    auto& s = AppSettings::instance();
    m_nickname->setText(PgxlConnection::asSetupToken(
        s.value(QStringLiteral("PGXL_Nickname"), QString{}).toString()));
}

void PgxlAdvancedPage::buildHardwareSection(QVBoxLayout* topLay)
{
    auto* box = new QGroupBox(QStringLiteral("Hardware"));
    auto* lay = new QVBoxLayout(box);

    // Pending indicator (initially hidden)
    m_hwPendingLabel = new QLabel(
        QStringLiteral("Changes pending -- Save & Reboot required to apply."));
    m_hwPendingLabel->setStyleSheet(
        QStringLiteral("color: #e87c1e; font-style: italic;"));
    m_hwPendingLabel->setVisible(false);
    lay->addWidget(m_hwPendingLabel);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Bias mode
    auto* biasGroup = new QWidget;
    auto* biasLay = new QHBoxLayout(biasGroup);
    biasLay->setContentsMargins(0, 0, 0, 0);
    m_biasClassA  = new QRadioButton(QStringLiteral("Class A"));
    m_biasClassAB = new QRadioButton(QStringLiteral("Class AB"));
    biasLay->addWidget(m_biasClassA);
    biasLay->addWidget(m_biasClassAB);
    biasLay->addStretch();
    form->addRow(QStringLiteral("Bias Mode:"), biasGroup);

    auto* biasButtonGroup = new QButtonGroup(this);
    biasButtonGroup->addButton(m_biasClassA);
    biasButtonGroup->addButton(m_biasClassAB);

    // Load persisted bias
    auto& s = AppSettings::instance();
    QString biasMode = s.value(QStringLiteral("PGXL_BiasMode"),
                               QStringLiteral("ClassAB")).toString();
    if (biasMode == QStringLiteral("ClassA")) {
        m_biasClassA->setChecked(true);
    } else {
        m_biasClassAB->setChecked(true);
    }

    // Fan mode
    m_fanModeCombo = new QComboBox;
    m_fanModeCombo->addItems({QStringLiteral("Auto"),
                              QStringLiteral("Quiet"),
                              QStringLiteral("Continuous")});
    QString fanMode = s.value(QStringLiteral("PGXL_FanMode"),
                               QStringLiteral("Auto")).toString();
    int fanIdx = m_fanModeCombo->findText(fanMode);
    m_fanModeCombo->setCurrentIndex(fanIdx >= 0 ? fanIdx : 0);
    form->addRow(QStringLiteral("Fan Mode:"), m_fanModeCombo);

    // LED intensity
    auto* ledWidget = new QWidget;
    auto* ledLay = new QHBoxLayout(ledWidget);
    ledLay->setContentsMargins(0, 0, 0, 0);
    m_ledSlider = new QSlider(Qt::Horizontal);
    m_ledSlider->setRange(0, 100);
    m_ledSlider->setTickInterval(10);
    int ledVal = s.value(QStringLiteral("PGXL_LedIntensity"), 75).toInt();
    m_ledSlider->setValue(ledVal);
    m_ledValueLabel = new QLabel(QString::number(ledVal));
    m_ledValueLabel->setMinimumWidth(30);
    ledLay->addWidget(m_ledSlider);
    ledLay->addWidget(m_ledValueLabel);
    form->addRow(QStringLiteral("LED Intensity:"), ledWidget);

    // TX power cap
    auto* powerCapWidget = new QWidget;
    auto* powerCapLay = new QHBoxLayout(powerCapWidget);
    powerCapLay->setContentsMargins(0, 0, 0, 0);
    m_powerCapCheck = new QCheckBox(QStringLiteral("Enable soft cap:"));
    bool capEnabled = s.value(QStringLiteral("PGXL_PowerCapEnabled"),
                               QStringLiteral("False")).toString() == QStringLiteral("True");
    m_powerCapCheck->setChecked(capEnabled);
    m_powerCapSpin = new QSpinBox;
    m_powerCapSpin->setRange(100, 2000);
    m_powerCapSpin->setSuffix(QStringLiteral(" W"));
    m_powerCapSpin->setValue(s.value(QStringLiteral("PGXL_PowerCapW"), 1500).toInt());
    m_powerCapSpin->setEnabled(capEnabled);
    powerCapLay->addWidget(m_powerCapCheck);
    powerCapLay->addWidget(m_powerCapSpin);
    powerCapLay->addStretch();
    form->addRow(QStringLiteral("TX Power Cap:"), powerCapWidget);

    lay->addLayout(form);
    topLay->addWidget(box);

    // Connections
    connect(m_biasClassA,  &QRadioButton::toggled, this, &PgxlAdvancedPage::onBiasModeChanged);
    connect(m_biasClassAB, &QRadioButton::toggled, this, &PgxlAdvancedPage::onBiasModeChanged);
    connect(m_fanModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PgxlAdvancedPage::onFanModeChanged);
    connect(m_ledSlider, &QSlider::valueChanged,
            this, &PgxlAdvancedPage::onLedSliderChanged);
    connect(m_powerCapCheck, &QCheckBox::toggled,
            this, &PgxlAdvancedPage::onPowerCapToggled);
    connect(m_powerCapSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PgxlAdvancedPage::onPowerCapWattsChanged);
}

void PgxlAdvancedPage::buildNetworkSection(QVBoxLayout* topLay)
{
    auto* box = new QGroupBox(QStringLiteral("Network"));
    auto* lay = new QVBoxLayout(box);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_dhcpCheck = new QCheckBox(QStringLiteral("Use DHCP"));
    form->addRow(QString{}, m_dhcpCheck);

    // IP validator: simple 0-255.0-255.0-255.0-255
    static const QRegularExpression ipRe(
        QStringLiteral(
            "^(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\."
            "(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\."
            "(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)\\."
            "(25[0-5]|2[0-4]\\d|[01]?\\d\\d?)$"));
    auto* ipValidator = new QRegularExpressionValidator(ipRe, this);

    m_ipEdit = new QLineEdit;
    m_ipEdit->setPlaceholderText(QStringLiteral("192.168.1.x"));
    m_ipEdit->setValidator(ipValidator);
    form->addRow(QStringLiteral("IP Address:"), m_ipEdit);

    m_netmaskEdit = new QLineEdit;
    m_netmaskEdit->setPlaceholderText(QStringLiteral("255.255.255.0"));
    m_netmaskEdit->setValidator(new QRegularExpressionValidator(ipRe, this));
    form->addRow(QStringLiteral("Netmask:"), m_netmaskEdit);

    m_gatewayEdit = new QLineEdit;
    m_gatewayEdit->setPlaceholderText(QStringLiteral("192.168.1.1"));
    m_gatewayEdit->setValidator(new QRegularExpressionValidator(ipRe, this));
    form->addRow(QStringLiteral("Gateway:"), m_gatewayEdit);

    lay->addLayout(form);

    // M4: a remote window's words (no Scan LAN there); local unchanged.
    auto* warnLabel = new QLabel(isRemote() ? networkQuestionText() : networkWarningText());
    warnLabel->setWordWrap(true);
    warnLabel->setStyleSheet(QStringLiteral("color: #e8c01e;"));
    lay->addWidget(warnLabel);

    // I5: a fixed setting without an address, a netmask or a usable
    // gateway is not sent; the reason shows here.
    m_networkProblem = new QLabel;
    m_networkProblem->setWordWrap(true);
    m_networkProblem->setVisible(false);
    lay->addWidget(m_networkProblem);

    m_applyIfconfBtn = new QPushButton(QStringLiteral("Apply Network Settings"));
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(m_applyIfconfBtn);
    lay->addLayout(btnRow);

    topLay->addWidget(box);

    // DHCP toggle gates the IP fields
    connect(m_dhcpCheck, &QCheckBox::toggled, this, &PgxlAdvancedPage::onDhcpToggled);
    connect(m_applyIfconfBtn, &QPushButton::clicked, this, &PgxlAdvancedPage::onApplyIfconf);

    // Initial DHCP gate
    onDhcpToggled(m_dhcpCheck->isChecked());
}

void PgxlAdvancedPage::buildPairingSection(QVBoxLayout* topLay)
{
    auto* box = new QGroupBox(QStringLiteral("Pairing & Band Source"));
    auto* lay = new QVBoxLayout(box);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // 2026-05-22 menu cleanup: was a 3-way combo (flexradio / amplifier
    // / none) but RadioModel only ever consumed the derived
    // PGXL_PairAttempt boolean; "flexradio" and "amplifier" were
    // functionally identical. Replaced with a single Auto-pair checkbox.
    m_pairAttemptCheckbox = new QCheckBox(
        QStringLiteral("Auto-pair on connect"));
    m_pairAttemptCheckbox->setToolTip(
        QStringLiteral("When enabled, NereusSDR pairs the Power Genius with "
                       "the radio each time it connects. Turn this off only "
                       "if the pairing is done some other way."));
    form->addRow(QStringLiteral("Pairing:"), m_pairAttemptCheckbox);

    // TX antenna
    auto* antWidget = new QWidget;
    auto* antLay = new QHBoxLayout(antWidget);
    antLay->setContentsMargins(0, 0, 0, 0);
    m_txAntAnt1 = new QRadioButton(QStringLiteral("ANT1"));
    m_txAntAnt2 = new QRadioButton(QStringLiteral("ANT2"));
    antLay->addWidget(m_txAntAnt1);
    antLay->addWidget(m_txAntAnt2);
    antLay->addStretch();
    form->addRow(QStringLiteral("TX Antenna:"), antWidget);

    auto* antGroup = new QButtonGroup(this);
    antGroup->addButton(m_txAntAnt1);
    antGroup->addButton(m_txAntAnt2);

    // Slice binding
    auto* sliceWidget = new QWidget;
    auto* sliceLay = new QHBoxLayout(sliceWidget);
    sliceLay->setContentsMargins(0, 0, 0, 0);
    m_sliceA = new QRadioButton(QStringLiteral("Slice A"));
    m_sliceB = new QRadioButton(QStringLiteral("Slice B"));
    sliceLay->addWidget(m_sliceA);
    sliceLay->addWidget(m_sliceB);
    sliceLay->addStretch();
    form->addRow(QStringLiteral("Follows slice:"), sliceWidget);

    auto* sliceGroup = new QButtonGroup(this);
    sliceGroup->addButton(m_sliceA);
    sliceGroup->addButton(m_sliceB);

    lay->addLayout(form);

    auto* infoLabel = new QLabel(
        QStringLiteral("Changes apply on next PGXL connect."));
    infoLabel->setStyleSheet(QStringLiteral("color: #999; font-style: italic;"));
    lay->addWidget(infoLabel);

    topLay->addWidget(box);

    // Persist defaults from AppSettings.
    // 2026-05-22 menu cleanup: PGXL_PairMode is no longer the source of
    // truth; PGXL_PairAttempt is the canonical persisted key. Default
    // True matches the prior behavior (combo defaulted to "flexradio"
    // which set PairAttempt=True).
    auto& s = AppSettings::instance();
    const bool pairAttempt = s.value(QStringLiteral("PGXL_PairAttempt"),
                                     QStringLiteral("True"))
                                .toString() == QStringLiteral("True");
    m_pairAttemptCheckbox->setChecked(pairAttempt);

    QString txAnt = s.value(QStringLiteral("PGXL_TxAnt"),
                            QStringLiteral("ANT1")).toString();
    if (txAnt == QStringLiteral("ANT2")) {
        m_txAntAnt2->setChecked(true);
    } else {
        m_txAntAnt1->setChecked(true);
    }

    QString slice = s.value(QStringLiteral("PGXL_FlexAmpSlice"),
                            QStringLiteral("A")).toString();
    if (slice == QStringLiteral("B")) {
        m_sliceB->setChecked(true);
    } else {
        m_sliceA->setChecked(true);
    }

    connect(m_pairAttemptCheckbox, &QCheckBox::toggled,
            this, &PgxlAdvancedPage::onPairModeChanged);
    connect(m_txAntAnt1,  &QRadioButton::toggled, this, &PgxlAdvancedPage::onTxAntChanged);
    connect(m_txAntAnt2,  &QRadioButton::toggled, this, &PgxlAdvancedPage::onTxAntChanged);
    connect(m_sliceA, &QRadioButton::toggled, this, &PgxlAdvancedPage::onSliceBindingChanged);
    connect(m_sliceB, &QRadioButton::toggled, this, &PgxlAdvancedPage::onSliceBindingChanged);
}

void PgxlAdvancedPage::buildDiagnosticsSection(QVBoxLayout* topLay)
{
    auto* box = new QGroupBox(QStringLiteral("Diagnostics"));
    auto* grid = new QGridLayout(box);

    auto addRow = [&](int row, const QString& label, QLabel*& valueOut) {
        grid->addWidget(new QLabel(label + QStringLiteral(":")), row, 0, Qt::AlignRight);
        valueOut = new QLabel(QStringLiteral("--"));
        grid->addWidget(valueOut, row, 1, Qt::AlignLeft);
    };

    addRow(0, QStringLiteral("Uptime"),               m_uptimeLabel);
    addRow(1, QStringLiteral("Last RTT"),              m_rttLabel);
    addRow(2, QStringLiteral("Keepalive Missed"),      m_keepaliveMissedLabel);
    addRow(3, QStringLiteral("Reconnects (this run)"),  m_reconnectCountLabel);
    addRow(4, QStringLiteral("Frames In"),             m_framesInLabel);
    addRow(5, QStringLiteral("Frames Out"),            m_framesOutLabel);
    addRow(6, QStringLiteral("Bytes In"),              m_bytesInLabel);
    addRow(7, QStringLiteral("Bytes Out"),             m_bytesOutLabel);

    grid->setColumnStretch(1, 1);
    topLay->addWidget(box);
}

void PgxlAdvancedPage::buildFaultHistorySection(QVBoxLayout* topLay)
{
    auto* box = new QGroupBox(QStringLiteral("Fault History"));
    auto* lay = new QVBoxLayout(box);

    m_faultTable = new QTableView;
    m_faultTable->setModel(m_faultTableModel);
    m_faultTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_faultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_faultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_faultTable->setAlternatingRowColors(true);
    m_faultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_faultTable->verticalHeader()->setVisible(false);
    m_faultTable->setMinimumHeight(160);
    lay->addWidget(m_faultTable);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto* clearAllBtn = new QPushButton(QStringLiteral("Clear All"));
    btnRow->addWidget(clearAllBtn);
    lay->addLayout(btnRow);

    topLay->addWidget(box);

    connect(clearAllBtn, &QPushButton::clicked, this, [this]() {
        // R-R3-47: a remote window asks the Core, which keeps the history.
        if (isRemote()) {
            IStationLink* link = m_model->stationLink();
            const IStationLink::CommandOutcome outcome = link
                ? link->requestClearAccessoryFaults(QStringLiteral("pgxl"))
                : IStationLink::CommandOutcome{ false, tr("Connect to the Core first.") };
            // Follow-up 3: this page shows the Core's refusal; no toast too.
            m_model->noteAccessoryRequestShownOnPage(outcome.commandId, this);
            if (!outcome.sent && m_remoteNote) {
                m_remoteNote->setText(OperatorReasonText::forDisplay(outcome.reason));
            }
            return;
        }
        m_faultLog->clear();
    });
    m_clearFaultsBtn = clearAllBtn;
}

// ---------------------------------------------------------------------------
// R-R3-47 / R-R3-22: a remote window's view of the Core
// ---------------------------------------------------------------------------

bool PgxlAdvancedPage::isRemote() const
{
    return m_model && m_model->role() == RadioModel::Role::Remote;
}

bool PgxlAdvancedPage::remoteDataAvailable() const
{
    const IStationLink* link = isRemote() ? m_model->stationLink() : nullptr;
    return link && link->accessoryDataAvailable();
}

void PgxlAdvancedPage::buildRemoteSections(QVBoxLayout* topLay)
{
    m_remoteNote = new QLabel;
    m_remoteNote->setObjectName(QStringLiteral("pgxlAdvancedRemoteNote"));
    m_remoteNote->setWordWrap(true);
    topLay->addWidget(m_remoteNote);
    // R-R3-47 / R-R3-22: the amp's last answer, or why this Core cannot
    // change the amp's own settings for this app.
    m_deviceAnswer = new QLabel;
    m_deviceAnswer->setObjectName(QStringLiteral("pgxlAdvancedDeviceAnswer"));
    m_deviceAnswer->setWordWrap(true);
    topLay->addWidget(m_deviceAnswer);

    buildIdentitySection(topLay);
    buildHardwareSection(topLay);
    buildNetworkSection(topLay);
    buildPairingSection(topLay);
    buildDiagnosticsSection(topLay);
    buildFaultHistorySection(topLay);
    buildFooter(topLay);

    // The output limit is the Core's (setPgxlPowerCap), sent when the
    // value is complete.
    m_powerCapSpin->setKeyboardTracking(false);
    m_powerCapSpin->setToolTip(tr("The Core shows an alert in every window when the Power "
                                  "Genius output goes above this."));
    // One request to the amp when the slider is let go, not one per step.
    m_ledSlider->setTracking(false);
    connect(m_ledSlider, &QSlider::sliderMoved, this, [this](int value) {
        m_ledValueLabel->setText(QString::number(value));
    });

    connect(m_diagnostics, &ConnectionDiagnostics::changed,
            this, &PgxlAdvancedPage::onDiagnosticsChanged);
    onDiagnosticsChanged();
    AmplifierModel* amp = m_model->amplifierModel();
    connect(amp, &AmplifierModel::stationConnectionChanged,
            this, &PgxlAdvancedPage::refreshRemoteIdentity);
    connect(amp, &AmplifierModel::statusChanged, this, &PgxlAdvancedPage::refreshRemoteIdentity);
    connect(m_model->accessorySettingsModel(), &AccessorySettingsModel::pgxlChanged,
            this, &PgxlAdvancedPage::refreshRemoteDevice);
    connect(m_model->accessoryDataModel(), &AccessoryDataModel::powerCapChanged,
            this, &PgxlAdvancedPage::refreshRemote);
    connect(m_model, &RadioModel::stationLinkStateChanged, this, &PgxlAdvancedPage::refreshRemote);
    // A Power Genius request the Core refused (its output limit, or the
    // amp's own settings): the Core's values stay and its words show here.
    // Only the amp's refusals; never an unrelated one.
    connect(m_model, &RadioModel::accessoryRequestRefused, this,
            [this](const QString& device, const QString& reason) {
        if (device == QLatin1String("pgxl")) {
            refreshRemote();
            showRemoteOutcome(false, reason);
        }
    });
    refreshRemote();
}

bool PgxlAdvancedPage::deviceSettingsAvailable() const
{
    const IStationLink* link = isRemote() ? m_model->stationLink() : nullptr;
    return link && link->pgxlDeviceSettingsAvailable();
}

bool PgxlAdvancedPage::remoteAmpConnected() const
{
    const IStationLink* link = isRemote() ? m_model->stationLink() : nullptr;
    return link && link->stationLinkReady()
        && m_model->amplifierModel()->connectionPhase()
               == AmplifierModel::ConnectionPhase::Connected;
}

// The amp's identity and state as the Core reports them (`amplifier`),
// shown the way onPgxlStatusUpdated shows the amp's own status keys.
void PgxlAdvancedPage::refreshRemoteIdentity()
{
    if (!isRemote() || !m_firmwareVersion) {
        return;
    }
    const AmplifierModel* amp = m_model->amplifierModel();
    const auto orDash = [](const QString& text) {
        return text.isEmpty() ? QStringLiteral("--") : text;
    };
    QMap<QString, QString> kvs;
    kvs.insert(QStringLiteral("version"), orDash(amp->deviceVersion()));
    kvs.insert(QStringLiteral("serial"), orDash(amp->deviceSerial()));
    kvs.insert(QStringLiteral("meffa"), orDash(amp->efficiencyText()));
    if (remoteAmpConnected() && !amp->deviceState().isEmpty()) {
        kvs.insert(QStringLiteral("state"), amp->deviceState());
    }
    onPgxlStatusUpdated(kvs);
    updateRemoteControls();
}

// The amp's own settings as the Core last heard them, and its last answer.
void PgxlAdvancedPage::refreshRemoteDevice()
{
    if (!isRemote() || !m_nickname) {
        return;
    }
    const AccessorySettingsModel::Device amp = m_model->accessorySettingsModel()->pgxl();
    m_updatingFromDevice = true;
    if (!amp.nickname.isEmpty() && !m_nickname->hasFocus()) {
        m_nickname->setText(PgxlConnection::asSetupToken(amp.nickname));
        m_nickname->setModified(false);
    }
    if (amp.biasMode == QLatin1String("ClassA")) {
        m_biasClassA->setChecked(true);
    } else if (amp.biasMode == QLatin1String("ClassAB")) {
        m_biasClassAB->setChecked(true);
    }
    if (!amp.fanMode.isEmpty()) {
        const int index = m_fanModeCombo->findText(amp.fanMode);
        if (index >= 0) {
            m_fanModeCombo->setCurrentIndex(index);
        }
    }
    if (amp.ledIntensity >= 0 && !m_ledSlider->isSliderDown()) {
        m_ledSlider->setValue(amp.ledIntensity);
        m_ledValueLabel->setText(QString::number(amp.ledIntensity));
    }
    if (amp.networkKnown) {
        m_dhcpCheck->setChecked(amp.dhcp);
        QLineEdit* edits[] = { m_ipEdit, m_netmaskEdit, m_gatewayEdit };
        const QString values[] = { amp.address, amp.netmask, amp.gateway };
        for (int i = 0; i < 3; ++i) {
            if (!edits[i]->hasFocus()) {
                edits[i]->setText(values[i]);
            }
        }
    }
    m_updatingFromDevice = false;
    if (!deviceSettingsAvailable()) {
        m_deviceAnswer->setText(IStationLink::pgxlDeviceSettingsUnavailableReason());
    } else {
        m_deviceAnswer->setText(amp.answerCount > 0 ? amp.answer : QString());
    }
    updateRemoteControls();
}

// What a remote window can change: the amp's own settings only when the
// Core offers them; Apply, Revert and Save & Reboot only while the Core is
// connected to the amp (as a local window's need its own connection).
void PgxlAdvancedPage::updateRemoteControls()
{
    if (!isRemote() || !m_revertBtn) {
        return;
    }
    const bool available = deviceSettingsAvailable();
    const bool connected = remoteAmpConnected();
    for (QWidget* widget : std::initializer_list<QWidget*>{
             m_nickname, m_biasClassA, m_biasClassAB, m_fanModeCombo, m_ledSlider,
             m_dhcpCheck}) {
        widget->setEnabled(available);
    }
    const bool manual = available && !m_dhcpCheck->isChecked();
    m_ipEdit->setEnabled(manual);
    m_netmaskEdit->setEnabled(manual);
    m_gatewayEdit->setEnabled(manual);
    updateConnectionUi(connected);
    m_applyIfconfBtn->setEnabled(available && connected);
    m_revertBtn->setEnabled(available && connected);
    m_saveAndRebootBtn->setEnabled(available && connected && m_pendingSaveReboot);
}

void PgxlAdvancedPage::showRemoteOutcome(bool sent, const QString& reason)
{
    if (sent) {
        return;
    }
    // Not sent, or refused by the Core: the Core's values stay on the page.
    refreshRemoteDevice();
    if (m_deviceAnswer) {
        m_deviceAnswer->setText(OperatorReasonText::forDisplay(reason));
    }
}

bool PgxlAdvancedPage::confirmRemote(const QString& title, const QString& text)
{
    if (m_confirmForTesting) {
        return m_confirmForTesting(title, text);
    }
    if (text == PgxlSaveRebootDialog::message()) {
        PgxlSaveRebootDialog dlg(this);
        dlg.setWindowTitle(title);
        return dlg.exec() == QDialog::Accepted;
    }
    QMessageBox box(QMessageBox::Warning, title, text, QMessageBox::Cancel, this);
    QPushButton* apply = box.addButton(title, QMessageBox::AcceptRole);
    box.setDefaultButton(QMessageBox::Cancel);
    box.exec();
    return box.clickedButton() == apply;
}

QString PgxlAdvancedPage::networkQuestionText()
{
    return QStringLiteral("The Power Genius will switch to these network settings. If NereusSDR "
                          "cannot reach it afterwards, enter its new address for the "
                          "Power Genius on the Peripherals page and connect again.");
}

QString PgxlAdvancedPage::networkWarningText()
{
    return QStringLiteral("PGXL must be unicast-reachable from this host after the change; "
                          "if you lose connection, use Scan LAN to rediscover.");
}

QString PgxlAdvancedPage::firmwareTextForTesting() const
{
    return m_firmwareVersion ? m_firmwareVersion->text() : QString();
}

QString PgxlAdvancedPage::networkProblemForTesting() const
{
    return m_networkProblem && !m_networkProblem->isHidden() ? m_networkProblem->text()
                                                             : QString();
}

QString PgxlAdvancedPage::deviceAnswerForTesting() const
{
    return m_deviceAnswer ? m_deviceAnswer->text() : QString();
}

void PgxlAdvancedPage::refreshRemote()
{
    if (!isRemote()) {
        return;
    }
    const bool available = remoteDataAvailable();
    const AccessoryDataModel* data = m_model->accessoryDataModel();
    m_updatingFromDevice = true;
    m_powerCapCheck->setChecked(data->powerCapEnabled());
    m_powerCapSpin->setValue(data->powerCapW());
    m_updatingFromDevice = false;
    m_powerCapCheck->setEnabled(available);
    m_powerCapSpin->setEnabled(available && data->powerCapEnabled());
    if (m_clearFaultsBtn) {
        m_clearFaultsBtn->setEnabled(available);
    }
    m_remoteNote->setText(available
        ? tr("The Core keeps these for the station's Power Genius. Changes here take effect "
             "there and show in every window.")
        : tr("This Core does not share its Power Genius records with this app. Updating the "
             "Core may help."));
    refreshRemoteIdentity();
    refreshRemoteDevice();
}

void PgxlAdvancedPage::sendRemotePowerCap()
{
    IStationLink* link = m_model->stationLink();
    const IStationLink::CommandOutcome outcome = link
        ? link->requestPgxlPowerCap(m_powerCapCheck->isChecked(), m_powerCapSpin->value())
        : IStationLink::CommandOutcome{ false, tr("Connect to the Core first.") };
    // Follow-up 3: this page shows the Core's refusal; no toast too.
    m_model->noteAccessoryRequestShownOnPage(outcome.commandId, this);
    if (!outcome.sent) {
        refreshRemote();
        m_remoteNote->setText(OperatorReasonText::forDisplay(outcome.reason));
    }
}

int PgxlAdvancedPage::faultRowCountForTesting() const
{
    return m_faultTableModel ? m_faultTableModel->rowCount() : 0;
}

QString PgxlAdvancedPage::faultTextForTesting(int row) const
{
    return m_faultTableModel
        ? m_faultTableModel->data(m_faultTableModel->index(row, 0), Qt::ToolTipRole).toString()
        : QString();
}

QString PgxlAdvancedPage::reconnectCountTextForTesting() const
{
    return m_reconnectCountLabel ? m_reconnectCountLabel->text() : QString();
}

QString PgxlAdvancedPage::remoteNoteForTesting() const
{
    return m_remoteNote ? m_remoteNote->text() : QString();
}

void PgxlAdvancedPage::buildFooter(QVBoxLayout* topLay)
{
    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    topLay->addWidget(line);

    auto* row = new QHBoxLayout;
    row->addStretch();

    m_revertBtn = new QPushButton(QStringLiteral("Revert"));
    m_saveAndRebootBtn = new QPushButton(QStringLiteral("Save & Reboot Amp"));
    m_saveAndRebootBtn->setEnabled(false);

    row->addWidget(m_revertBtn);
    row->addWidget(m_saveAndRebootBtn);
    topLay->addLayout(row);

    connect(m_revertBtn, &QPushButton::clicked,
            this, &PgxlAdvancedPage::onRevert);
    connect(m_saveAndRebootBtn, &QPushButton::clicked,
            this, &PgxlAdvancedPage::onSaveAndReboot);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void PgxlAdvancedPage::onPgxlConnected()
{
    updateConnectionUi(true);
    // Request current settings
    if (m_model && m_model->pgxlConnection()) {
        m_model->pgxlConnection()->readSetup();
        m_model->pgxlConnection()->readIfconf();
    }
}

void PgxlAdvancedPage::onPgxlDisconnected()
{
    updateConnectionUi(false);
    m_stateBadge->setText(QStringLiteral("Offline"));
    m_stateBadge->setStyleSheet(
        QStringLiteral("background: #555; color: #ccc; border-radius: 3px; padding: 2px 6px;"));
}

void PgxlAdvancedPage::onPgxlStatusUpdated(const QMap<QString, QString>& kvs)
{
    m_updatingFromDevice = true;

    if (kvs.contains(QStringLiteral("version"))) {
        m_firmwareVersion->setText(kvs.value(QStringLiteral("version")));
    }
    if (kvs.contains(QStringLiteral("serial"))) {
        m_serialLabel->setText(kvs.value(QStringLiteral("serial")));
    }
    if (kvs.contains(QStringLiteral("meffa"))) {
        m_meffaLabel->setText(kvs.value(QStringLiteral("meffa")));
    }

    // State badge
    if (kvs.contains(QStringLiteral("state"))) {
        QString state = kvs.value(QStringLiteral("state")).toUpper();
        m_stateBadge->setText(state);
        if (state == QStringLiteral("OPERATE")) {
            m_stateBadge->setStyleSheet(
                QStringLiteral("background: #2d8a2d; color: #fff;"
                               " border-radius: 3px; padding: 2px 6px;"));
        } else if (state.startsWith(QStringLiteral("FAULT"))) {
            m_stateBadge->setStyleSheet(
                QStringLiteral("background: #aa2222; color: #fff;"
                               " border-radius: 3px; padding: 2px 6px;"));
        } else if (state == QStringLiteral("STANDBY")) {
            m_stateBadge->setStyleSheet(
                QStringLiteral("background: #888; color: #fff;"
                               " border-radius: 3px; padding: 2px 6px;"));
        } else {
            m_stateBadge->setStyleSheet(
                QStringLiteral("background: #c88000; color: #fff;"
                               " border-radius: 3px; padding: 2px 6px;"));
        }
    }

    m_updatingFromDevice = false;
}

void PgxlAdvancedPage::onSetupResponse(const QMap<QString, QString>& fields)
{
    m_updatingFromDevice = true;

    if (fields.contains(QStringLiteral("nickname"))) {
        m_nickname->setText(PgxlConnection::asSetupToken(fields.value(QStringLiteral("nickname"))));
    }
    if (fields.contains(QStringLiteral("bias"))) {
        QString bias = fields.value(QStringLiteral("bias")).toLower();
        if (bias == QStringLiteral("classa") || bias == QStringLiteral("class_a")
                || bias == QStringLiteral("a")) {
            m_biasClassA->setChecked(true);
        } else {
            m_biasClassAB->setChecked(true);
        }
    }
    if (fields.contains(QStringLiteral("fan"))) {
        QString fan = fields.value(QStringLiteral("fan"));
        int idx = m_fanModeCombo->findText(fan, Qt::MatchFixedString | Qt::MatchCaseSensitive);
        if (idx < 0) {
            // Try case-insensitive
            idx = m_fanModeCombo->findText(fan, Qt::MatchFixedString);
        }
        if (idx >= 0) {
            m_fanModeCombo->setCurrentIndex(idx);
        }
    }
    if (fields.contains(QStringLiteral("led"))) {
        bool ok = false;
        int ledVal = fields.value(QStringLiteral("led")).toInt(&ok);
        if (ok) {
            m_ledSlider->setValue(ledVal);
        }
    }

    m_updatingFromDevice = false;
}

void PgxlAdvancedPage::onIfconfResponse(const QMap<QString, QString>& fields)
{
    m_updatingFromDevice = true;

    if (fields.contains(QStringLiteral("dhcp"))) {
        bool dhcp = (fields.value(QStringLiteral("dhcp")).toLower()
                     == QStringLiteral("true")
                     || fields.value(QStringLiteral("dhcp")) == QStringLiteral("1"));
        m_dhcpCheck->setChecked(dhcp);
        onDhcpToggled(dhcp);
    }
    if (fields.contains(QStringLiteral("ip"))) {
        m_ipEdit->setText(fields.value(QStringLiteral("ip")));
    }
    if (fields.contains(QStringLiteral("netmask"))) {
        m_netmaskEdit->setText(fields.value(QStringLiteral("netmask")));
    }
    if (fields.contains(QStringLiteral("gateway"))) {
        m_gatewayEdit->setText(fields.value(QStringLiteral("gateway")));
    }

    m_updatingFromDevice = false;
}

void PgxlAdvancedPage::onDiagnosticsChanged()
{
    m_uptimeLabel->setText(formatMs(m_diagnostics->uptimeMs()));
    m_rttLabel->setText(QString::number(m_diagnostics->lastRttMs()) + QStringLiteral(" ms"));
    m_keepaliveMissedLabel->setText(QString::number(m_diagnostics->keepaliveMissed()));
    m_reconnectCountLabel->setText(QString::number(m_diagnostics->reconnectCount()));
    m_framesInLabel->setText(QString::number(m_diagnostics->framesIn()));
    m_framesOutLabel->setText(QString::number(m_diagnostics->framesOut()));
    m_bytesInLabel->setText(formatBytes(m_diagnostics->bytesIn()));
    m_bytesOutLabel->setText(formatBytes(m_diagnostics->bytesOut()));
}

void PgxlAdvancedPage::onSaveAndReboot()
{
    if (isRemote()) {
        // R-R3-47 / R-R3-22: the same question as a local window, then the
        // Core sends the amp `save`.
        if (!confirmRemote(QStringLiteral("Save & Reboot PGXL"),
                           PgxlSaveRebootDialog::message())) {
            return;
        }
        IStationLink* link = m_model->stationLink();
        const IStationLink::CommandOutcome outcome = link
            ? link->requestPgxlSaveAndRestart()
            : IStationLink::CommandOutcome{ false, tr("Connect to the Core first.") };
        // Follow-up 3: this page shows the Core's refusal; no toast too.
        m_model->noteAccessoryRequestShownOnPage(outcome.commandId, this);
        if (!outcome.sent) {
            showRemoteOutcome(false, outcome.reason);
            return;
        }
        m_stateBadge->setText(QStringLiteral("Rebooting..."));
        m_stateBadge->setStyleSheet(
            QStringLiteral("background: #c88000; color: #fff;"
                           " border-radius: 3px; padding: 2px 6px;"));
        setPendingState(false);
        return;
    }
    if (!m_model || !m_model->pgxlConnection()) {
        return;
    }

    PgxlSaveRebootDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    m_model->pgxlConnection()->save();
    m_stateBadge->setText(QStringLiteral("Rebooting..."));
    m_stateBadge->setStyleSheet(
        QStringLiteral("background: #c88000; color: #fff;"
                       " border-radius: 3px; padding: 2px 6px;"));
    m_pendingSaveReboot = false;
}

void PgxlAdvancedPage::onRevert()
{
    if (isRemote()) {
        // R-R3-47 / R-R3-22: the Core asks the amp for its settings again
        // (`setup read`, `ifconf read`); they arrive on accessorySettings.
        IStationLink* link = m_model->stationLink();
        const IStationLink::CommandOutcome outcome = link
            ? link->requestPgxlReadSettings()
            : IStationLink::CommandOutcome{ false, tr("Connect to the Core first.") };
        // Follow-up 3: this page shows the Core's refusal; no toast too.
        m_model->noteAccessoryRequestShownOnPage(outcome.commandId, this);
        showRemoteOutcome(outcome.sent, outcome.reason);
        setPendingState(false);
        return;
    }
    // Reload fields from device
    if (m_model && m_model->pgxlConnection()
            && m_model->pgxlConnection()->isConnected()) {
        m_model->pgxlConnection()->readSetup();
        m_model->pgxlConnection()->readIfconf();
    }
    setPendingState(false);
}

// ---------------------------------------------------------------------------
// Hardware section slots
// ---------------------------------------------------------------------------

void PgxlAdvancedPage::onBiasModeChanged()
{
    if (m_updatingFromDevice) {
        return;
    }
    QString mode = m_biasClassA->isChecked()
                   ? QStringLiteral("ClassA")
                   : QStringLiteral("ClassAB");
    if (isRemote()) {
        // Both buttons report the change; one request per change.
        auto* button = qobject_cast<QRadioButton*>(sender());
        if (button && !button->isChecked()) {
            return;
        }
        sendRemoteHardware(QStringLiteral("biasMode"), mode);
        return;
    }
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("PGXL_BiasMode"), mode);

    if (m_model && m_model->pgxlConnection() && m_model->pgxlConnection()->isConnected()) {
        QString wireVal = m_biasClassA->isChecked()
                          ? QStringLiteral("a")
                          : QStringLiteral("ab");
        m_model->pgxlConnection()->writeSetup({{QStringLiteral("bias"), wireVal}});
    }
    setPendingState(true);
}

void PgxlAdvancedPage::onFanModeChanged(int /*index*/)
{
    if (m_updatingFromDevice) {
        return;
    }
    QString mode = m_fanModeCombo->currentText().toLower();
    if (isRemote()) {
        sendRemoteHardware(QStringLiteral("fanMode"), m_fanModeCombo->currentText());
        return;
    }
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("PGXL_FanMode"), m_fanModeCombo->currentText());

    if (m_model && m_model->pgxlConnection() && m_model->pgxlConnection()->isConnected()) {
        m_model->pgxlConnection()->writeSetup({{QStringLiteral("fan"), mode}});
    }
    setPendingState(true);
}

void PgxlAdvancedPage::onLedSliderChanged(int value)
{
    m_ledValueLabel->setText(QString::number(value));
    if (m_updatingFromDevice) {
        return;
    }
    if (isRemote()) {
        sendRemoteHardware(QStringLiteral("ledIntensity"), QString::number(value));
        return;
    }
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("PGXL_LedIntensity"), value);

    if (m_model && m_model->pgxlConnection() && m_model->pgxlConnection()->isConnected()) {
        m_model->pgxlConnection()->writeSetup(
            {{QStringLiteral("led"), QString::number(value)}});
    }
    setPendingState(true);
}

void PgxlAdvancedPage::onPowerCapToggled(bool checked)
{
    if (m_updatingFromDevice) {
        return;
    }
    m_powerCapSpin->setEnabled(checked);
    if (isRemote()) {
        sendRemotePowerCap();
        return;
    }
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("PGXL_PowerCapEnabled"),
               checked ? QStringLiteral("True") : QStringLiteral("False"));
    setPendingState(true);
}

void PgxlAdvancedPage::onPowerCapWattsChanged(int watts)
{
    if (m_updatingFromDevice) {
        return;
    }
    if (isRemote()) {
        sendRemotePowerCap();
        return;
    }
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("PGXL_PowerCapW"), watts);
    setPendingState(true);
}

// ---------------------------------------------------------------------------
// Network section slots
// ---------------------------------------------------------------------------

void PgxlAdvancedPage::onDhcpToggled(bool checked)
{
    m_ipEdit->setEnabled(!checked);
    m_netmaskEdit->setEnabled(!checked);
    m_gatewayEdit->setEnabled(!checked);
    updateRemoteControls();
}

void PgxlAdvancedPage::onApplyIfconf()
{
    // I5: the same check the Core makes, before anything is asked or sent.
    const QString problem = StationDeviceSettings::networkProblem(
        m_dhcpCheck->isChecked(), m_ipEdit->text(), m_netmaskEdit->text(),
        m_gatewayEdit->text());
    m_networkProblem->setText(problem);
    m_networkProblem->setVisible(!problem.isEmpty());
    if (!problem.isEmpty()) {
        return;
    }
    if (isRemote()) {
        // R-R3-47 / R-R3-22: new network settings can take the amp off the
        // Core's network, so the window asks first, in the Network
        // section's own words; nothing is sent without a yes.
        if (!confirmRemote(QStringLiteral("Apply Network Settings"),
                           networkQuestionText())) {
            return;
        }
        IStationLink* link = m_model->stationLink();
        const IStationLink::CommandOutcome outcome = link
            ? link->requestPgxlNetwork(m_dhcpCheck->isChecked(), m_ipEdit->text().trimmed(),
                                       m_netmaskEdit->text().trimmed(),
                                       m_gatewayEdit->text().trimmed())
            : IStationLink::CommandOutcome{ false, tr("Connect to the Core first.") };
        // Follow-up 3: this page shows the Core's refusal; no toast too.
        m_model->noteAccessoryRequestShownOnPage(outcome.commandId, this);
        if (!outcome.sent) {
            showRemoteOutcome(false, outcome.reason);
            return;
        }
        setPendingState(true);
        return;
    }
    if (!m_model || !m_model->pgxlConnection()) {
        return;
    }
    if (!m_model->pgxlConnection()->isConnected()) {
        return;
    }
    // Operator decision 2026-09-24: a local window asks the same question
    // before new network settings as a remote one; nothing is sent
    // without a yes.
    if (!confirmRemote(QStringLiteral("Apply Network Settings"), networkQuestionText())) {
        return;
    }
    m_model->pgxlConnection()->writeIfconf(
        m_ipEdit->text().trimmed(),
        m_netmaskEdit->text().trimmed(),
        m_gatewayEdit->text().trimmed(),
        m_dhcpCheck->isChecked());
    setPendingState(true);
}

// ---------------------------------------------------------------------------
// Pairing section slots
// ---------------------------------------------------------------------------

void PgxlAdvancedPage::onPairModeChanged(int /*index*/)
{
    if (m_updatingFromDevice) {
        return;
    }
    // 2026-05-22 menu cleanup: checkbox drives PGXL_PairAttempt directly.
    // PGXL_PairMode is no longer written (was unread by RadioModel).
    const bool attempt = m_pairAttemptCheckbox
                         && m_pairAttemptCheckbox->isChecked();
    AppSettings::instance().setValue(
        QStringLiteral("PGXL_PairAttempt"),
        attempt ? QStringLiteral("True") : QStringLiteral("False"));
}

void PgxlAdvancedPage::onTxAntChanged()
{
    if (m_updatingFromDevice) {
        return;
    }
    QString ant = m_txAntAnt2->isChecked()
                  ? QStringLiteral("ANT2")
                  : QStringLiteral("ANT1");
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("PGXL_TxAnt"), ant);
}

void PgxlAdvancedPage::onSliceBindingChanged()
{
    if (m_updatingFromDevice) {
        return;
    }
    QString slice = m_sliceB->isChecked()
                    ? QStringLiteral("B")
                    : QStringLiteral("A");
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("PGXL_FlexAmpSlice"), slice);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void PgxlAdvancedPage::setPendingState(bool pending)
{
    m_pendingSaveReboot = pending;
    if (m_hwPendingLabel) {
        m_hwPendingLabel->setVisible(pending);
    }
    if (m_saveAndRebootBtn) {
        m_saveAndRebootBtn->setEnabled(pending);
    }
    updateRemoteControls();
}

// R-R3-47 / R-R3-22: one hardware setting, through the Core.
void PgxlAdvancedPage::sendRemoteHardware(const QString& setting, const QString& value)
{
    IStationLink* link = m_model->stationLink();
    const IStationLink::CommandOutcome outcome = link
        ? link->requestPgxlHardware(setting, value)
        : IStationLink::CommandOutcome{ false, tr("Connect to the Core first.") };
    // Follow-up 3: this page shows the Core's refusal; no toast too.
    m_model->noteAccessoryRequestShownOnPage(outcome.commandId, this);
    if (!outcome.sent) {
        showRemoteOutcome(false, outcome.reason);
        return;
    }
    setPendingState(true);
}

void PgxlAdvancedPage::updateConnectionUi(bool connected)
{
    if (m_applyIfconfBtn) {
        m_applyIfconfBtn->setEnabled(connected);
    }
    if (m_revertBtn) {
        m_revertBtn->setEnabled(connected);
    }
    if (!connected) {
        if (m_stateBadge) {
            m_stateBadge->setText(QStringLiteral("Offline"));
            m_stateBadge->setStyleSheet(
                QStringLiteral("background: #555; color: #ccc;"
                               " border-radius: 3px; padding: 2px 6px;"));
        }
    }
}

QString PgxlAdvancedPage::formatMs(qint64 ms)
{
    if (ms <= 0) {
        return QStringLiteral("--");
    }
    qint64 totalSec = ms / 1000;
    qint64 h = totalSec / 3600;
    qint64 m = (totalSec % 3600) / 60;
    qint64 s = totalSec % 60;
    if (h > 0) {
        return QStringLiteral("%1h %2m %3s")
            .arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    }
    if (m > 0) {
        return QStringLiteral("%1m %2s").arg(m).arg(s, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1s").arg(s);
}

QString PgxlAdvancedPage::formatBytes(quint64 bytes)
{
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        return QStringLiteral("%1 GB")
            .arg(static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
    }
    if (bytes >= 1024ULL * 1024ULL) {
        return QStringLiteral("%1 MB")
            .arg(static_cast<double>(bytes) / (1024.0 * 1024.0), 0, 'f', 1);
    }
    if (bytes >= 1024ULL) {
        return QStringLiteral("%1 KB")
            .arg(static_cast<double>(bytes) / 1024.0, 0, 'f', 1);
    }
    return QStringLiteral("%1 B").arg(bytes);
}

void PgxlAdvancedPage::applySetupIds()
{
    // Setup description version 15: this page's ids.
    const std::pair<QWidget*, const char*> setupIds[] = {
        {m_nickname, "nickname"},
        {m_fanModeCombo, "fanMode"},
        {m_ledSlider, "ledIntensity"},
        {m_powerCapCheck, "powerCap"},
        {m_powerCapSpin, "powerCapW"},
        {m_dhcpCheck, "dhcp"},
        {m_ipEdit, "address"},
        {m_netmaskEdit, "netmask"},
        {m_gatewayEdit, "gateway"},
        {m_applyIfconfBtn, "applyNetwork"},
        {m_pairAttemptCheckbox, "pairAttempt"},
        {m_uptimeLabel, "connectedSinceMs"},
        {m_rttLabel, "lastRttMs"},
        {m_keepaliveMissedLabel, "keepaliveMissed"},
        {m_reconnectCountLabel, "reconnectCount"},
        {m_framesInLabel, "framesIn"},
        {m_framesOutLabel, "framesOut"},
        {m_bytesInLabel, "bytesIn"},
        {m_bytesOutLabel, "bytesOut"},
        {m_clearFaultsBtn, "clearFaults"},
        {m_revertBtn, "revert"},
        {m_saveAndRebootBtn, "saveReboot"}};
    for (const auto& [widget, id] : setupIds) {
        if (widget) {
            widget->setProperty("nereusSetupId", QStringLiteral("catNetwork.powerGenius.") + QLatin1String(id));
        }
    }
}

}  // namespace NereusSDR

// FaultLogTableModel uses Q_OBJECT so we need the moc file included.
// The class is defined in the .cpp, so we include the moc manually.
#include "PgxlAdvancedPage.moc"
