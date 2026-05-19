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
#include "../../core/PgxlConnection.h"
#include "../../models/RadioModel.h"
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
        if (!index.isValid() || role != Qt::DisplayRole) {
            return QVariant();
        }
        const auto events = m_faultLog->events();
        if (index.row() >= events.size()) {
            return QVariant();
        }
        const FaultEvent& ev = events.at(index.row());
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
    , m_diagnostics(new ConnectionDiagnostics(this))
    , m_faultLog(new FaultLog(QStringLiteral("PGXL_FaultHistory"), this))
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

            // Bind diagnostics helper to the PGXL connection
            m_diagnostics->bindTo(pgxl);
        }
    }

    connect(m_diagnostics, &ConnectionDiagnostics::changed,
            this, &PgxlAdvancedPage::onDiagnosticsChanged);

    // Initial UI state
    updateConnectionUi(m_model && m_model->pgxlConnection()
                       && m_model->pgxlConnection()->isConnected());
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
    m_nickname->setPlaceholderText(QStringLiteral("e.g. Shack PGXL"));
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

    m_meffaLabel = new QLabel(QStringLiteral("--"));
    form->addRow(QStringLiteral("MeFFA:"), m_meffaLabel);

    topLay->addWidget(box);

    // Nickname editingFinished -> writeSetup
    connect(m_nickname, &QLineEdit::editingFinished, this, [this]() {
        if (m_model && m_model->pgxlConnection() && m_model->pgxlConnection()->isConnected()) {
            m_model->pgxlConnection()->writeSetup(
                {{QStringLiteral("nickname"), m_nickname->text().trimmed()}});
        }
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("PGXL_Nickname"), m_nickname->text().trimmed());
    });

    // Load persisted nickname
    auto& s = AppSettings::instance();
    m_nickname->setText(s.value(QStringLiteral("PGXL_Nickname"), QString{}).toString());
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

    auto* warnLabel = new QLabel(
        QStringLiteral("PGXL must be unicast-reachable from this host after the change; "
                       "if you lose connection, use Scan LAN to rediscover."));
    warnLabel->setWordWrap(true);
    warnLabel->setStyleSheet(QStringLiteral("color: #e8c01e;"));
    lay->addWidget(warnLabel);

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

    m_pairModeCombo = new QComboBox;
    m_pairModeCombo->addItem(QStringLiteral("flexradio (LAN PTT & band)"),
                             QStringLiteral("flexradio"));
    m_pairModeCombo->addItem(QStringLiteral("amplifier create (no PTT, manual band)"),
                             QStringLiteral("amplifier"));
    m_pairModeCombo->addItem(QStringLiteral("None"),
                             QStringLiteral("none"));
    form->addRow(QStringLiteral("Pair Mode:"), m_pairModeCombo);

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
    form->addRow(QStringLiteral("Slice Binding:"), sliceWidget);

    auto* sliceGroup = new QButtonGroup(this);
    sliceGroup->addButton(m_sliceA);
    sliceGroup->addButton(m_sliceB);

    lay->addLayout(form);

    auto* infoLabel = new QLabel(
        QStringLiteral("Changes apply on next PGXL connect."));
    infoLabel->setStyleSheet(QStringLiteral("color: #999; font-style: italic;"));
    lay->addWidget(infoLabel);

    topLay->addWidget(box);

    // Persist defaults from AppSettings
    auto& s = AppSettings::instance();
    QString pairMode = s.value(QStringLiteral("PGXL_PairMode"),
                               QStringLiteral("flexradio")).toString();
    int modeIdx = m_pairModeCombo->findData(pairMode);
    m_pairModeCombo->setCurrentIndex(modeIdx >= 0 ? modeIdx : 0);

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

    connect(m_pairModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
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
    addRow(3, QStringLiteral("Reconnects (session)"),  m_reconnectCountLabel);
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
        m_faultLog->clear();
    });
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
        m_nickname->setText(fields.value(QStringLiteral("nickname")));
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
}

void PgxlAdvancedPage::onApplyIfconf()
{
    if (!m_model || !m_model->pgxlConnection()) {
        return;
    }
    if (!m_model->pgxlConnection()->isConnected()) {
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
    QString mode = m_pairModeCombo->currentData().toString();
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("PGXL_PairMode"), mode);

    // PGXL_PairAttempt is true for both flexradio and amplifier modes
    bool attempt = (mode == QStringLiteral("flexradio")
                    || mode == QStringLiteral("amplifier"));
    s.setValue(QStringLiteral("PGXL_PairAttempt"),
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

}  // namespace NereusSDR

// FaultLogTableModel uses Q_OBJECT so we need the moc file included.
// The class is defined in the .cpp, so we include the moc manually.
#include "PgxlAdvancedPage.moc"
