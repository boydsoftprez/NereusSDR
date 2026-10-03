// =================================================================
// src/gui/setup/TgxlAdvancedPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-native Setup -> Network -> TGXL Advanced page.
//
// Phase 3P-II Phase 4 Task 85.
//
// Design reference:
//   docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-design.md
//   section 5.7 and footer.
//
// AI tooling: Anthropic Claude Code.
//
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: the counters are the
//                                    model's (the Core's in a remote
//                                    window); in a remote window the page is
//                                    a view of the Core's antenna names,
//                                    tune memory, counters and Tuner Genius
//                                    fault history (restored there: the Core
//                                    now records them), cleared through
//                                    clearAccessoryFaults. The fault table
//                                    shows when and what happened in plain
//                                    words. AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  R-R3-47 / R-R3-22: a remote window
//                                    builds every section. The tuner's own
//                                    settings (name, network, Save &
//                                    Reboot, Revert) go to the Core as
//                                    typed requests, which it sends the
//                                    tuner as this page's own commands; the
//                                    page asks the same Save & Reboot
//                                    question and, before network changes,
//                                    the Network section's own warning; the
//                                    tuner's answers, its values and the
//                                    Core's refusals show on the page.
//                                    AI-assisted via Anthropic Claude Code.
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

#include "TgxlAdvancedPage.h"

#include <QAbstractTableModel>
#include <QCheckBox>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QModelIndex>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTableView>
#include <QVBoxLayout>
#include <QVariant>

#include "../../core/AppSettings.h"
#include "../../core/ConnectionDiagnostics.h"
#include "../../core/FaultLog.h"
#include "../../core/PgxlConnection.h"
#include "../../core/StationDeviceSettings.h"
#include "../../core/TgxlConnection.h"
#include "../../core/TuneMemoryStore.h"
#include "../../models/Band.h"
#include "../../core/session/IStationLink.h"
#include "../../models/AccessoryDataModel.h"
#include "../../models/AccessorySettingsModel.h"
#include "../../models/TunerModel.h"
#include "../../models/RadioModel.h"
#include "../OperatorReasonText.h"
#include "../PgxlSaveRebootDialog.h"

namespace NereusSDR {

// ---------------------------------------------------------------------------
// TgxlFaultLogTableModel (private inline class)
// ---------------------------------------------------------------------------
// Maps FaultLog::events() into a QAbstractTableModel for the QTableView.
// R-R3-47: 2 columns, When and What happened (plain words); the detail the
// connection gave is the row's tooltip. A Tuner Genius fault carries no
// power readings.
// Named with Tgxl prefix to avoid duplicate symbol with PgxlAdvancedPage's
// identically-shaped model.
// ---------------------------------------------------------------------------

class TgxlFaultLogTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit TgxlFaultLogTableModel(FaultLog* faultLog, QObject* parent = nullptr)
        : QAbstractTableModel(parent), m_faultLog(faultLog)
    {
        connect(m_faultLog, &FaultLog::changed,
                this, &TgxlFaultLogTableModel::onFaultLogChanged);
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
        return 2;
    }

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override
    {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
            return QVariant();
        }
        switch (section) {
        case 0: return QStringLiteral("When");
        case 1: return QStringLiteral("What happened");
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
        if (role == Qt::ToolTipRole) {
            return ev.detail.isEmpty() ? ev.text : ev.text + QStringLiteral("\n") + ev.detail;
        }
        switch (index.column()) {
        case 0: {
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(ev.whenMs);
            return dt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
        }
        case 1: return ev.text;
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
// TuneMemoryTableModel (private inline class)
// ---------------------------------------------------------------------------
// Maps TuneMemoryStore::listAll() into a QAbstractTableModel for the
// tune-memory QTableView. 6 columns: Band, Antenna, C1, L, C2, Saved at.
// ---------------------------------------------------------------------------

class TuneMemoryTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit TuneMemoryTableModel(TuneMemoryStore* store, QObject* parent = nullptr)
        : QAbstractTableModel(parent), m_store(store)
    {
        connect(m_store, &TuneMemoryStore::changed, this, &TuneMemoryTableModel::onStoreChanged);
        refresh();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        if (parent.isValid()) {
            return 0;
        }
        return m_entries.size();
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
        case 0: return QStringLiteral("Band");
        case 1: return QStringLiteral("Antenna");
        case 2: return QStringLiteral("C1");
        case 3: return QStringLiteral("L");
        case 4: return QStringLiteral("C2");
        case 5: return QStringLiteral("Saved at");
        default: return QVariant();
        }
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || role != Qt::DisplayRole) {
            return QVariant();
        }
        if (index.row() >= m_entries.size()) {
            return QVariant();
        }
        const TuneMemory& mem = m_entries.at(index.row());
        switch (index.column()) {
        case 0: return bandLabel(mem.band);
        case 1: return QStringLiteral("ANT %1").arg(mem.antenna);
        case 2: return mem.c1;
        case 3: return mem.l;
        case 4: return mem.c2;
        case 5: {
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(mem.savedAtMs);
            return dt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
        }
        default: return QVariant();
        }
    }

    // Return the TuneMemory at the given row, or std::nullopt if out of range.
    std::optional<TuneMemory> memoryAt(int row) const
    {
        if (row < 0 || row >= m_entries.size()) {
            return std::nullopt;
        }
        return m_entries.at(row);
    }

    TuneMemoryStore* store() const { return m_store; }

private slots:
    void onStoreChanged()
    {
        refresh();
    }

private:
    void refresh()
    {
        beginResetModel();
        m_entries = m_store->listAll();
        endResetModel();
    }

    TuneMemoryStore*     m_store{nullptr};
    QVector<TuneMemory>  m_entries;
};

// ---------------------------------------------------------------------------
// TgxlAdvancedPage
// ---------------------------------------------------------------------------

TgxlAdvancedPage::TgxlAdvancedPage(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    // R-R3-47 / R-R3-22: the model's counters (its own connection in a
    // local window, the Core's in a remote one).
    , m_diagnostics(model ? model->tgxlDiagnostics() : new ConnectionDiagnostics(this))
    // Phase 3P-II Phase 4 Task 94: FaultLog is now owned by RadioModel (shared instance).
    // Use m_model->tgxlFaultLog() when m_model is non-null; fall back to a local instance
    // (same key) when m_model is null (unit-test construction without a live RadioModel).
    , m_faultLog(model ? model->tgxlFaultLog()
                       : new FaultLog(QStringLiteral("TGXL_FaultHistory"), this))
    // Phase 3P-II Phase 4 Task 89: use the RadioModel's shared TuneMemoryStore so
    // saves from TunerApplet's context menu are visible here and vice versa.
    // Fallback to a local instance when m_model is null (e.g. in unit tests).
    , m_tuneMemoryStore(model ? model->tuneMemoryStore() : new TuneMemoryStore(this))
    , m_faultTableModel(new TgxlFaultLogTableModel(m_faultLog, this))
    , m_tuneMemTableModel(new TuneMemoryTableModel(m_tuneMemoryStore, this))
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
        // R-R3-47 / R-R3-22: every section, as in a local window. The
        // tuner's own settings go to the Core, which sends the tuner this
        // page's own commands (a remote window never opens a connection to
        // the tuner); the antenna names, tune memory, counters and fault
        // history are the Core's.
        m_remoteNote = new QLabel;
        m_remoteNote->setObjectName(QStringLiteral("tgxlAdvancedRemoteNote"));
        m_remoteNote->setWordWrap(true);
        topLay->addWidget(m_remoteNote);
        m_deviceAnswer = new QLabel;
        m_deviceAnswer->setObjectName(QStringLiteral("tgxlAdvancedDeviceAnswer"));
        m_deviceAnswer->setWordWrap(true);
        topLay->addWidget(m_deviceAnswer);
        buildIdentitySection(topLay);
        buildAntennaLabelsSection(topLay);
        buildNetworkSection(topLay);
        buildTuneMemorySection(topLay);
        buildDiagnosticsSection(topLay);
        buildFaultHistorySection(topLay);
        buildFooter(topLay);
        topLay->addStretch();
        if (TunerModel* tuner = m_model->tunerModel()) {
            connect(tuner, &TunerModel::stationConnectionChanged,
                    this, &TgxlAdvancedPage::refreshRemoteIdentity);
            connect(tuner, &TunerModel::stateChanged,
                    this, &TgxlAdvancedPage::refreshRemoteIdentity);
            connect(tuner, &TunerModel::presenceChanged,
                    this, &TgxlAdvancedPage::refreshRemoteIdentity);
        }
        connect(m_model->accessorySettingsModel(), &AccessorySettingsModel::tgxlChanged,
                this, &TgxlAdvancedPage::refreshRemoteDevice);
        // A request for the tuner's own settings the Core refused: the
        // Core's values stay and its words show here.
        connect(m_model, &RadioModel::accessoryRequestRefused, this,
                [this](const QString& device, const QString& reason) {
            if (device == QLatin1String("tgxl")) {
                showRemoteOutcome(false, reason);
            }
        });
        connect(m_diagnostics, &ConnectionDiagnostics::changed,
                this, &TgxlAdvancedPage::onDiagnosticsChanged);
        connect(m_model->accessoryDataModel(), &AccessoryDataModel::labelsChanged,
                this, &TgxlAdvancedPage::refreshRemote);
        connect(m_model, &RadioModel::stationLinkStateChanged,
                this, &TgxlAdvancedPage::refreshRemote);
        connect(m_model->accessoryDataModel(), &AccessoryDataModel::tuneMemoryChanged, this,
                [this] {
            const QSignalBlocker block(m_autoRecallCheck);
            m_autoRecallCheck->setChecked(
                m_model->accessoryDataModel()->autoTuneMemoryRecall());
        });
        onDiagnosticsChanged();
        refreshRemote();
        applySetupIds();
        return;
    }

    buildIdentitySection(topLay);
    buildAntennaLabelsSection(topLay);
    buildNetworkSection(topLay);
    buildTuneMemorySection(topLay);
    buildDiagnosticsSection(topLay);
    // 2026-05-22 menu cleanup: Fault History section removed. TGXL has
    // no documented fault taxonomy and nothing in the codebase ever
    // appends to tgxlFaultLog, so the table was always empty. The
    // RadioModel-owned FaultLog instance and its accessor remain in
    // place for symmetry with PGXL and as a hook for future TGXL fault
    // events if 4O3A publishes a taxonomy.
    buildFooter(topLay);
    topLay->addStretch();

    // Wire TgxlConnection signals
    if (m_model) {
        TgxlConnection* tgxl = m_model->tgxlConnection();
        if (tgxl) {
            connect(tgxl, &TgxlConnection::connected,
                    this, &TgxlAdvancedPage::onTgxlConnected);
            connect(tgxl, &TgxlConnection::disconnected,
                    this, &TgxlAdvancedPage::onTgxlDisconnected);
            connect(tgxl, &TgxlConnection::statusUpdated,
                    this, &TgxlAdvancedPage::onTgxlStatusUpdated);
            connect(tgxl, &TgxlConnection::setupResponse,
                    this, &TgxlAdvancedPage::onSetupResponse);
            connect(tgxl, &TgxlConnection::ifconfResponse,
                    this, &TgxlAdvancedPage::onIfconfResponse);

            // 2026-05-22 bench fix: the firmware + serial labels populate
            // from TgxlConnection::statusUpdated keys (version, serial),
            // which fires when TgxlConnection sees the R-frame body for
            // its initial `info` query. That query runs on the V-frame
            // handshake (TgxlConnection.cpp:191) -- typically minutes
            // before the operator opens this page. By the time we attach
            // the slot here, the signal has long since fired with no
            // subscribers, and the labels stay at "--".
            //
            // Re-issue `info` when the page opens with an already-
            // connected TGXL so the response triggers our newly-wired
            // slot and the labels populate immediately. No-op if not
            // connected; onTgxlConnected will fire if a future connect
            // happens.
            if (tgxl->isConnected()) {
                tgxl->sendCommand(QStringLiteral("info"));
            }

            // R-R3-47: RadioModel binds its counters to this connection
            // for its whole life (StationAccessoryData publishes them).
        }
    }

    connect(m_diagnostics, &ConnectionDiagnostics::changed,
            this, &TgxlAdvancedPage::onDiagnosticsChanged);
    onDiagnosticsChanged();

    connect(m_tuneMemoryStore, &TuneMemoryStore::changed,
            this, &TgxlAdvancedPage::onTuneMemoryChanged);

    // Initial UI state
    updateConnectionUi(m_model && m_model->tgxlConnection()
                       && m_model->tgxlConnection()->isConnected());

    applySetupIds();
}

TgxlAdvancedPage::~TgxlAdvancedPage() = default;

// ---------------------------------------------------------------------------
// Section builders
// ---------------------------------------------------------------------------

void TgxlAdvancedPage::buildIdentitySection(QVBoxLayout* topLay)
{
    auto* box = new QGroupBox(QStringLiteral("Identity & Status"));
    auto* form = new QFormLayout(box);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_nickname = new QLineEdit;
    m_nickname->setPlaceholderText(QStringLiteral("e.g. Shack_TGXL"));
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
    form->addRow(QStringLiteral("State:"), m_stateBadge);

    m_variantLabel = new QLabel(QStringLiteral("--"));
    form->addRow(QStringLiteral("Variant:"), m_variantLabel);

    topLay->addWidget(box);

    // Nickname editingFinished -> writeSetup
    connect(m_nickname, &QLineEdit::editingFinished, this, [this]() {
        if (isRemote()) {
            // R-R3-47 / R-R3-22: the Core renames the tuner (the same
            // `setup nickname=`) and keeps the name. One request per edit.
            if (m_updatingFromDevice || !m_nickname->isModified()) {
                return;
            }
            m_nickname->setModified(false);
            IStationLink* link = m_model->stationLink();
            const IStationLink::CommandOutcome outcome = link
                ? link->requestTgxlName(m_nickname->text().trimmed())
                : IStationLink::CommandOutcome{ false, tr("Connect to the Core first.") };
            // Follow-up 3: this page shows the Core's refusal; no toast too.
            m_model->noteAccessoryRequestShownOnPage(outcome.commandId, this);
            showRemoteOutcome(outcome.sent, outcome.reason);
            return;
        }
        if (m_model && m_model->tgxlConnection() && m_model->tgxlConnection()->isConnected()) {
            m_model->tgxlConnection()->writeSetup(
                {{QStringLiteral("nickname"), m_nickname->text().trimmed()}});
        }
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("TGXL_Nickname"), m_nickname->text().trimmed());
    });

    // Load persisted nickname
    // Fix round 1 (minor 4): a name saved with spaces before names were one
    // word is offered with underscores, so the box takes edits again (the
    // validator never accepts the old text, and editingFinished never fires).
    auto& s = AppSettings::instance();
    m_nickname->setText(PgxlConnection::asSetupToken(
        s.value(QStringLiteral("TGXL_Nickname"), QString{}).toString()));
}

void TgxlAdvancedPage::buildAntennaLabelsSection(QVBoxLayout* topLay)
{
    auto* box = new QGroupBox(QStringLiteral("Antenna Labels"));
    auto* form = new QFormLayout(box);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto& s = AppSettings::instance();

    m_ant1Label = new QLineEdit;
    m_ant1Label->setPlaceholderText(QStringLiteral("e.g. 80 m dipole"));
    m_ant1Label->setText(s.value(QStringLiteral("TGXL_Ant1_Label"), QString{}).toString());
    form->addRow(QStringLiteral("ANT 1:"), m_ant1Label);

    m_ant2Label = new QLineEdit;
    m_ant2Label->setPlaceholderText(QStringLiteral("e.g. vertical"));
    m_ant2Label->setText(s.value(QStringLiteral("TGXL_Ant2_Label"), QString{}).toString());
    form->addRow(QStringLiteral("ANT 2:"), m_ant2Label);

    m_ant3Label = new QLineEdit;
    m_ant3Label->setPlaceholderText(QStringLiteral("e.g. beverage"));
    m_ant3Label->setText(s.value(QStringLiteral("TGXL_Ant3_Label"), QString{}).toString());
    form->addRow(QStringLiteral("ANT 3:"), m_ant3Label);

    auto* infoLabel = new QLabel(
        QStringLiteral("Labels appear on TunerApplet antenna buttons and "
                       "the Peripherals status string (Task 95)."));
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet(QStringLiteral("color: #999; font-style: italic;"));
    form->addRow(QString{}, infoLabel);

    topLay->addWidget(box);

    connect(m_ant1Label, &QLineEdit::editingFinished, this, &TgxlAdvancedPage::onAnt1LabelEdited);
    connect(m_ant2Label, &QLineEdit::editingFinished, this, &TgxlAdvancedPage::onAnt2LabelEdited);
    connect(m_ant3Label, &QLineEdit::editingFinished, this, &TgxlAdvancedPage::onAnt3LabelEdited);
}

void TgxlAdvancedPage::buildNetworkSection(QVBoxLayout* topLay)
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
    connect(m_dhcpCheck, &QCheckBox::toggled, this, &TgxlAdvancedPage::onDhcpToggled);
    connect(m_applyIfconfBtn, &QPushButton::clicked, this, &TgxlAdvancedPage::onApplyIfconf);

    // Initial DHCP gate
    onDhcpToggled(m_dhcpCheck->isChecked());
}

void TgxlAdvancedPage::buildTuneMemorySection(QVBoxLayout* topLay)
{
    auto* box = new QGroupBox(QStringLiteral("Tune Memory Management"));
    auto* lay = new QVBoxLayout(box);

    // Auto-recall toggle
    auto& s = AppSettings::instance();
    m_autoRecallCheck = new QCheckBox(
        QStringLiteral("Auto-recall tune memory on band/antenna change"));
    bool autoRecall = s.value(QStringLiteral("TGXL_AutoTuneMemoryRecall"),
                              QStringLiteral("False")).toString() == QStringLiteral("True");
    m_autoRecallCheck->setChecked(autoRecall);
    lay->addWidget(m_autoRecallCheck);

    // Table view
    m_tuneMemTable = new QTableView;
    m_tuneMemTable->setModel(m_tuneMemTableModel);
    m_tuneMemTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tuneMemTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tuneMemTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tuneMemTable->setAlternatingRowColors(true);
    m_tuneMemTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tuneMemTable->verticalHeader()->setVisible(false);
    m_tuneMemTable->setMinimumHeight(160);
    lay->addWidget(m_tuneMemTable);

    // Button row: per-row Clear + bulk Clear All
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();

    auto* clearRowBtn = new QPushButton(QStringLiteral("Clear Selected"));
    btnRow->addWidget(clearRowBtn);

    auto* clearAllBtn = new QPushButton(QStringLiteral("Clear All"));
    btnRow->addWidget(clearAllBtn);
    lay->addLayout(btnRow);

    topLay->addWidget(box);

    // Auto-recall persist
    connect(m_autoRecallCheck, &QCheckBox::toggled, this, [](bool checked) {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("TGXL_AutoTuneMemoryRecall"),
                   checked ? QStringLiteral("True") : QStringLiteral("False"));
    });

    // Clear selected row
    connect(clearRowBtn, &QPushButton::clicked, this, [this]() {
        const QModelIndexList sel = m_tuneMemTable->selectionModel()->selectedRows();
        if (sel.isEmpty()) {
            return;
        }
        auto mem = m_tuneMemTableModel->memoryAt(sel.first().row());
        if (mem.has_value()) {
            m_tuneMemoryStore->clear(mem->antenna, mem->band);
        }
    });

    // Clear all rows
    connect(clearAllBtn, &QPushButton::clicked, this, [this]() {
        m_tuneMemoryStore->clearAll();
    });
}

void TgxlAdvancedPage::buildDiagnosticsSection(QVBoxLayout* topLay)
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

// 2026-05-22 menu cleanup: buildFaultHistorySection() removed. The
// section was always empty because nothing in the codebase populates
// tgxlFaultLog (TGXL has no documented fault taxonomy per design §4.7).
// The FaultLog and TgxlFaultLogTableModel instances and the header
// declarations are kept as no-op state in case 4O3A publishes a TGXL
// fault taxonomy later; restoring the UI would just need to re-add the
// build method and the buildFaultHistorySection(topLay) call.
//
// R-R3-47 / R-R3-22: restored for a remote window. The Core records the
// Tuner Genius's faults (a live connection dropping, a connection ending
// at an error; StationTgxlController::faultObserved), so there the table
// has a producer. A local window still records none and keeps the section
// out.
void TgxlAdvancedPage::buildFaultHistorySection(QVBoxLayout* topLay)
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
    m_faultTable->setMinimumHeight(120);
    lay->addWidget(m_faultTable);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    m_clearFaultsBtn = new QPushButton(QStringLiteral("Clear All"));
    btnRow->addWidget(m_clearFaultsBtn);
    lay->addLayout(btnRow);
    topLay->addWidget(box);

    connect(m_clearFaultsBtn, &QPushButton::clicked, this, [this]() {
        IStationLink* link = isRemote() ? m_model->stationLink() : nullptr;
        if (!isRemote()) {
            m_faultLog->clear();
            return;
        }
        const IStationLink::CommandOutcome outcome = link
            ? link->requestClearAccessoryFaults(QStringLiteral("tgxl"))
            : IStationLink::CommandOutcome{ false, tr("Connect to the Core first.") };
        // Follow-up 3: this page shows the Core's refusal; no toast too.
        m_model->noteAccessoryRequestShownOnPage(outcome.commandId, this);
        if (!outcome.sent && m_remoteNote) {
            m_remoteNote->setText(OperatorReasonText::forDisplay(outcome.reason));
        }
    });
}

bool TgxlAdvancedPage::isRemote() const
{
    return m_model && m_model->role() == RadioModel::Role::Remote;
}

void TgxlAdvancedPage::refreshRemote()
{
    if (!isRemote()) {
        return;
    }
    const IStationLink* link = m_model->stationLink();
    const bool available = link && link->accessoryDataAvailable();
    if (m_clearFaultsBtn) {
        m_clearFaultsBtn->setEnabled(available);
    }
    // The Core's names, unless the operator is typing one.
    if (available) {
        const QStringList labels = m_model->accessoryDataModel()->tgxlAntennaLabels();
        QLineEdit* edits[] = { m_ant1Label, m_ant2Label, m_ant3Label };
        for (int i = 0; i < 3; ++i) {
            if (edits[i] && !edits[i]->hasFocus()) {
                edits[i]->setText(labels.value(i));
            }
        }
    }
    if (m_remoteNote) {
        m_remoteNote->setText(available
            ? tr("The Core keeps these for the station's Tuner Genius. Changes here take effect "
                 "there and show in every window.")
            : tr("This Core does not share its Tuner Genius records with this app. Updating the "
                 "Core may help."));
    }
    refreshRemoteIdentity();
    refreshRemoteDevice();
}

bool TgxlAdvancedPage::deviceSettingsAvailable() const
{
    const IStationLink* link = isRemote() ? m_model->stationLink() : nullptr;
    return link && link->tgxlDeviceSettingsAvailable();
}

bool TgxlAdvancedPage::remoteTunerConnected() const
{
    const IStationLink* link = isRemote() ? m_model->stationLink() : nullptr;
    return link && link->stationLinkReady() && m_model->tunerModel()
        && m_model->tunerModel()->connectionPhase() == TunerModel::ConnectionPhase::Connected;
}

// The tuner's identity and state as the Core reports them (`tuner`), shown
// the way onTgxlStatusUpdated shows the tuner's own status keys.
void TgxlAdvancedPage::refreshRemoteIdentity()
{
    if (!isRemote() || !m_firmwareVersion || !m_model->tunerModel()) {
        return;
    }
    const TunerModel* tuner = m_model->tunerModel();
    const auto orDash = [](const QString& text) {
        return text.isEmpty() ? QStringLiteral("--") : text;
    };
    QMap<QString, QString> kvs;
    kvs.insert(QStringLiteral("version"), orDash(tuner->deviceVersion()));
    kvs.insert(QStringLiteral("serial"), orDash(tuner->deviceSerial()));
    if (remoteTunerConnected()) {
        kvs.insert(QStringLiteral("one_by_three"),
                   tuner->hasAntennaSwitch() ? QStringLiteral("1") : QStringLiteral("0"));
        // The TunerApplet's three states (TunerApplet.cpp, the operate
        // button): operate alone is OPERATE, operate with bypass is BYPASS,
        // anything else STANDBY.
        kvs.insert(QStringLiteral("state"),
                   tuner->isOperate() && !tuner->isBypass() ? QStringLiteral("OPERATE")
                   : tuner->isOperate()                     ? QStringLiteral("BYPASS")
                                                            : QStringLiteral("STANDBY"));
    }
    onTgxlStatusUpdated(kvs);
    updateRemoteControls();
}

// The tuner's own settings as the Core last heard them, and its last answer.
void TgxlAdvancedPage::refreshRemoteDevice()
{
    if (!isRemote() || !m_nickname) {
        return;
    }
    const AccessorySettingsModel::Device tuner = m_model->accessorySettingsModel()->tgxl();
    m_updatingFromDevice = true;
    if (!tuner.nickname.isEmpty() && !m_nickname->hasFocus()) {
        m_nickname->setText(PgxlConnection::asSetupToken(tuner.nickname));
        m_nickname->setModified(false);
    }
    if (tuner.networkKnown) {
        m_dhcpCheck->setChecked(tuner.dhcp);
        QLineEdit* edits[] = { m_ipEdit, m_netmaskEdit, m_gatewayEdit };
        const QString values[] = { tuner.address, tuner.netmask, tuner.gateway };
        for (int i = 0; i < 3; ++i) {
            if (!edits[i]->hasFocus()) {
                edits[i]->setText(values[i]);
            }
        }
    }
    m_updatingFromDevice = false;
    if (!deviceSettingsAvailable()) {
        m_deviceAnswer->setText(IStationLink::tgxlDeviceSettingsUnavailableReason());
    } else {
        m_deviceAnswer->setText(tuner.answerCount > 0 ? tuner.answer : QString());
    }
    updateRemoteControls();
}

// What a remote window can change: the tuner's own settings only when the
// Core offers them; Apply, Revert and Save & Reboot only while the Core is
// connected to the tuner (as a local window's need its own connection).
void TgxlAdvancedPage::updateRemoteControls()
{
    if (!isRemote() || !m_revertBtn) {
        return;
    }
    const bool available = deviceSettingsAvailable();
    const bool connected = remoteTunerConnected();
    m_nickname->setEnabled(available);
    m_dhcpCheck->setEnabled(available);
    const bool manual = available && !m_dhcpCheck->isChecked();
    m_ipEdit->setEnabled(manual);
    m_netmaskEdit->setEnabled(manual);
    m_gatewayEdit->setEnabled(manual);
    updateConnectionUi(connected);
    m_applyIfconfBtn->setEnabled(available && connected);
    m_revertBtn->setEnabled(available && connected);
    m_saveAndRebootBtn->setEnabled(available && connected && m_pendingSaveReboot);
}

void TgxlAdvancedPage::showRemoteOutcome(bool sent, const QString& reason)
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

bool TgxlAdvancedPage::confirmRemote(const QString& title, const QString& text)
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

QString TgxlAdvancedPage::networkQuestionText()
{
    return QStringLiteral("The Tuner Genius will switch to these network settings. If NereusSDR "
                          "cannot reach it afterwards, enter its new address for the "
                          "Tuner Genius on the Peripherals page and connect again.");
}

QString TgxlAdvancedPage::networkWarningText()
{
    return QStringLiteral("TGXL must be unicast-reachable from this host after the change; "
                          "if you lose connection, use Scan LAN to rediscover.");
}

QString TgxlAdvancedPage::firmwareTextForTesting() const
{
    return m_firmwareVersion ? m_firmwareVersion->text() : QString();
}

QString TgxlAdvancedPage::variantTextForTesting() const
{
    return m_variantLabel ? m_variantLabel->text() : QString();
}

QString TgxlAdvancedPage::networkProblemForTesting() const
{
    return m_networkProblem && !m_networkProblem->isHidden() ? m_networkProblem->text()
                                                             : QString();
}

QString TgxlAdvancedPage::deviceAnswerForTesting() const
{
    return m_deviceAnswer ? m_deviceAnswer->text() : QString();
}

int TgxlAdvancedPage::faultRowCountForTesting() const
{
    return m_faultTableModel ? m_faultTableModel->rowCount() : 0;
}

QString TgxlAdvancedPage::faultTextForTesting(int row) const
{
    return m_faultTableModel
        ? m_faultTableModel->data(m_faultTableModel->index(row, 1), Qt::DisplayRole).toString()
        : QString();
}

QString TgxlAdvancedPage::reconnectCountTextForTesting() const
{
    return m_reconnectCountLabel ? m_reconnectCountLabel->text() : QString();
}

QString TgxlAdvancedPage::antennaLabelForTesting(int index) const
{
    QLineEdit* edits[] = { m_ant1Label, m_ant2Label, m_ant3Label };
    return index >= 1 && index <= 3 && edits[index - 1] ? edits[index - 1]->text() : QString();
}

int TgxlAdvancedPage::tuneMemoryRowCountForTesting() const
{
    return m_tuneMemoryStore ? m_tuneMemoryStore->listAll().size() : 0;
}

void TgxlAdvancedPage::buildFooter(QVBoxLayout* topLay)
{
    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    topLay->addWidget(line);

    auto* row = new QHBoxLayout;
    row->addStretch();

    m_revertBtn = new QPushButton(QStringLiteral("Revert"));
    m_saveAndRebootBtn = new QPushButton(QStringLiteral("Save & Reboot Tuner"));
    m_saveAndRebootBtn->setEnabled(false);

    row->addWidget(m_revertBtn);
    row->addWidget(m_saveAndRebootBtn);
    topLay->addLayout(row);

    connect(m_revertBtn, &QPushButton::clicked,
            this, &TgxlAdvancedPage::onRevert);
    connect(m_saveAndRebootBtn, &QPushButton::clicked,
            this, &TgxlAdvancedPage::onSaveAndReboot);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void TgxlAdvancedPage::onTgxlConnected()
{
    updateConnectionUi(true);
    // Request current settings from device
    if (m_model && m_model->tgxlConnection()) {
        m_model->tgxlConnection()->readSetup();
        m_model->tgxlConnection()->readIfconf();
    }
}

void TgxlAdvancedPage::onTgxlDisconnected()
{
    updateConnectionUi(false);
    m_stateBadge->setText(QStringLiteral("Offline"));
    m_stateBadge->setStyleSheet(
        QStringLiteral("background: #555; color: #ccc; border-radius: 3px; padding: 2px 6px;"));
}

void TgxlAdvancedPage::onTgxlStatusUpdated(const QMap<QString, QString>& kvs)
{
    m_updatingFromDevice = true;

    if (kvs.contains(QStringLiteral("version"))) {
        m_firmwareVersion->setText(kvs.value(QStringLiteral("version")));
    }
    if (kvs.contains(QStringLiteral("serial"))) {
        m_serialLabel->setText(kvs.value(QStringLiteral("serial")));
    }

    // Model variant: "one_by_three" key indicates 3x1 (3 antennas, 1 radio)
    if (kvs.contains(QStringLiteral("one_by_three"))) {
        bool is3x1 = (kvs.value(QStringLiteral("one_by_three")) == QStringLiteral("1"));
        m_variantLabel->setText(is3x1 ? QStringLiteral("3x1") : QStringLiteral("1x1"));
    }

    // State badge - same color coding as PGXL
    if (kvs.contains(QStringLiteral("state"))) {
        QString state = kvs.value(QStringLiteral("state")).toUpper();
        m_stateBadge->setText(state);
        if (state == QStringLiteral("OPERATE")) {
            m_stateBadge->setStyleSheet(
                QStringLiteral("background: #2d8a2d; color: #fff;"
                               " border-radius: 3px; padding: 2px 6px;"));
        } else if (state == QStringLiteral("BYPASS")) {
            m_stateBadge->setStyleSheet(
                QStringLiteral("background: #888; color: #fff;"
                               " border-radius: 3px; padding: 2px 6px;"));
        } else if (state == QStringLiteral("STANDBY")) {
            m_stateBadge->setStyleSheet(
                QStringLiteral("background: #888; color: #fff;"
                               " border-radius: 3px; padding: 2px 6px;"));
        } else if (state.startsWith(QStringLiteral("FAULT"))) {
            m_stateBadge->setStyleSheet(
                QStringLiteral("background: #aa2222; color: #fff;"
                               " border-radius: 3px; padding: 2px 6px;"));
        } else {
            m_stateBadge->setStyleSheet(
                QStringLiteral("background: #c88000; color: #fff;"
                               " border-radius: 3px; padding: 2px 6px;"));
        }
    }

    m_updatingFromDevice = false;
}

void TgxlAdvancedPage::onSetupResponse(const QMap<QString, QString>& fields)
{
    m_updatingFromDevice = true;

    if (fields.contains(QStringLiteral("nickname"))) {
        m_nickname->setText(PgxlConnection::asSetupToken(fields.value(QStringLiteral("nickname"))));
        // Sync to AppSettings
        AppSettings::instance().setValue(QStringLiteral("TGXL_Nickname"),
                                         fields.value(QStringLiteral("nickname")));
    }

    m_updatingFromDevice = false;
}

void TgxlAdvancedPage::onIfconfResponse(const QMap<QString, QString>& fields)
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

void TgxlAdvancedPage::onDiagnosticsChanged()
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

void TgxlAdvancedPage::onTuneMemoryChanged()
{
    // TuneMemoryTableModel self-updates on store changed(); no extra work needed here.
    // Slot is available for future per-change side effects.
}

void TgxlAdvancedPage::onSaveAndReboot()
{
    if (isRemote()) {
        // R-R3-47 / R-R3-22: the same question as a local window, then the
        // Core sends the tuner `save`.
        if (!confirmRemote(QStringLiteral("Save & Reboot TGXL"),
                           PgxlSaveRebootDialog::message())) {
            return;
        }
        IStationLink* link = m_model->stationLink();
        const IStationLink::CommandOutcome outcome = link
            ? link->requestTgxlSaveAndRestart()
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
    if (!m_model || !m_model->tgxlConnection()) {
        return;
    }

    // Reuse PgxlSaveRebootDialog with TGXL-specific window title.
    // The dialog's text says "PGXL" internally; a constructor parameter
    // to customize the device name is deferred (design note: "bench testing
    // confirms whether TGXL accepts the save verb"). For now the modal
    // informs the operator; if TGXL bench confirms Save unsupported the
    // button can be hidden via a runtime gate.
    PgxlSaveRebootDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Save & Reboot TGXL"));
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    m_model->tgxlConnection()->save();
    m_stateBadge->setText(QStringLiteral("Rebooting..."));
    m_stateBadge->setStyleSheet(
        QStringLiteral("background: #c88000; color: #fff;"
                       " border-radius: 3px; padding: 2px 6px;"));
    m_pendingSaveReboot = false;
}

void TgxlAdvancedPage::onRevert()
{
    if (isRemote()) {
        // R-R3-47 / R-R3-22: the Core asks the tuner for its settings again
        // (`setup read`, `ifconf read`); they arrive on accessorySettings.
        IStationLink* link = m_model->stationLink();
        const IStationLink::CommandOutcome outcome = link
            ? link->requestTgxlReadSettings()
            : IStationLink::CommandOutcome{ false, tr("Connect to the Core first.") };
        // Follow-up 3: this page shows the Core's refusal; no toast too.
        m_model->noteAccessoryRequestShownOnPage(outcome.commandId, this);
        showRemoteOutcome(outcome.sent, outcome.reason);
        setPendingState(false);
        return;
    }
    // Reload fields from device
    if (m_model && m_model->tgxlConnection()
            && m_model->tgxlConnection()->isConnected()) {
        m_model->tgxlConnection()->readSetup();
        m_model->tgxlConnection()->readIfconf();
    }
    setPendingState(false);
}

// ---------------------------------------------------------------------------
// Antenna label slots
// ---------------------------------------------------------------------------

void TgxlAdvancedPage::onAnt1LabelEdited()
{
    const QString label = m_ant1Label->text().trimmed();
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("TGXL_Ant1_Label"), label);
    // Phase 3P-II Phase 4 Task 95: propagate to TunerApplet via SetupDialog forward.
    emit antennaLabelChanged(1, label);
}

void TgxlAdvancedPage::onAnt2LabelEdited()
{
    const QString label = m_ant2Label->text().trimmed();
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("TGXL_Ant2_Label"), label);
    // Phase 3P-II Phase 4 Task 95: propagate to TunerApplet via SetupDialog forward.
    emit antennaLabelChanged(2, label);
}

void TgxlAdvancedPage::onAnt3LabelEdited()
{
    const QString label = m_ant3Label->text().trimmed();
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("TGXL_Ant3_Label"), label);
    // Phase 3P-II Phase 4 Task 95: propagate to TunerApplet via SetupDialog forward.
    emit antennaLabelChanged(3, label);
}

// ---------------------------------------------------------------------------
// Network section slots
// ---------------------------------------------------------------------------

void TgxlAdvancedPage::onDhcpToggled(bool checked)
{
    m_ipEdit->setEnabled(!checked);
    m_netmaskEdit->setEnabled(!checked);
    m_gatewayEdit->setEnabled(!checked);
    updateRemoteControls();
}

void TgxlAdvancedPage::onApplyIfconf()
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
        // R-R3-47 / R-R3-22: new network settings can take the tuner off the
        // Core's network, so the window asks first, in the Network
        // section's own words; nothing is sent without a yes.
        if (!confirmRemote(QStringLiteral("Apply Network Settings"),
                           networkQuestionText())) {
            return;
        }
        IStationLink* link = m_model->stationLink();
        const IStationLink::CommandOutcome outcome = link
            ? link->requestTgxlNetwork(m_dhcpCheck->isChecked(), m_ipEdit->text().trimmed(),
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
    if (!m_model || !m_model->tgxlConnection()) {
        return;
    }
    if (!m_model->tgxlConnection()->isConnected()) {
        return;
    }
    // Operator decision 2026-09-24: a local window asks the same question
    // before new network settings as a remote one; nothing is sent
    // without a yes.
    if (!confirmRemote(QStringLiteral("Apply Network Settings"), networkQuestionText())) {
        return;
    }
    m_model->tgxlConnection()->writeIfconf(
        m_ipEdit->text().trimmed(),
        m_netmaskEdit->text().trimmed(),
        m_gatewayEdit->text().trimmed(),
        m_dhcpCheck->isChecked());
    setPendingState(true);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void TgxlAdvancedPage::setPendingState(bool pending)
{
    m_pendingSaveReboot = pending;
    if (m_saveAndRebootBtn) {
        m_saveAndRebootBtn->setEnabled(pending);
    }
    updateRemoteControls();
}

void TgxlAdvancedPage::updateConnectionUi(bool connected)
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

QString TgxlAdvancedPage::formatMs(qint64 ms)
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

QString TgxlAdvancedPage::formatBytes(quint64 bytes)
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

void TgxlAdvancedPage::applySetupIds()
{
    // Setup description version 15: this page's ids.
    const std::pair<QWidget*, const char*> setupIds[] = {
        {m_nickname, "nickname"},
        {m_ant1Label, "ant1Label"},
        {m_ant2Label, "ant2Label"},
        {m_ant3Label, "ant3Label"},
        {m_dhcpCheck, "dhcp"},
        {m_ipEdit, "address"},
        {m_netmaskEdit, "netmask"},
        {m_gatewayEdit, "gateway"},
        {m_applyIfconfBtn, "applyNetwork"},
        {m_autoRecallCheck, "autoRecall"},
        {m_uptimeLabel, "connectedSinceMs"},
        {m_rttLabel, "lastRttMs"},
        {m_keepaliveMissedLabel, "keepaliveMissed"},
        {m_reconnectCountLabel, "reconnectCount"},
        {m_framesInLabel, "framesIn"},
        {m_framesOutLabel, "framesOut"},
        {m_bytesInLabel, "bytesIn"},
        {m_bytesOutLabel, "bytesOut"},
        {m_revertBtn, "revert"},
        {m_saveAndRebootBtn, "saveReboot"}};
    for (const auto& [widget, id] : setupIds) {
        if (widget) {
            widget->setProperty("nereusSetupId", QStringLiteral("catNetwork.tunerGenius.") + QLatin1String(id));
        }
    }
}

}  // namespace NereusSDR

// TgxlFaultLogTableModel and TuneMemoryTableModel use Q_OBJECT so we need
// the moc file included. Both classes are defined in this .cpp, so we
// include the moc manually.
#include "TgxlAdvancedPage.moc"
