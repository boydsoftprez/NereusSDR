#include "MmioEndpointsDialog.h"

#include "../../core/mmio/ExternalVariableEngine.h"
#include "../../core/mmio/MmioEndpoint.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QSplitter>
#include <QFrame>

namespace NereusSDR {

namespace {

constexpr const char* kDialogStyle =
    "QDialog { background: #0f0f1a; color: #c8d8e8; }"
    "QLabel  { color: #c8d8e8; font-size: 11px; }"
    "QLineEdit, QSpinBox, QComboBox {"
    "  background: #0a0a18; color: #c8d8e8;"
    "  border: 1px solid #1e2e3e; border-radius: 3px;"
    "  padding: 2px 4px; min-height: 18px;"
    "}"
    "QComboBox QAbstractItemView {"
    "  background: #0a0a18; color: #c8d8e8;"
    "  border: 1px solid #205070;"
    "  selection-background-color: #00b4d8;"
    "}"
    "QListWidget, QTreeWidget {"
    "  background: #0a0a18; color: #c8d8e8;"
    "  border: 1px solid #203040;"
    "  alternate-background-color: #111122;"
    "}"
    "QPushButton {"
    "  background: #1a2a3a; color: #c8d8e8;"
    "  border: 1px solid #205070; border-radius: 3px;"
    "  padding: 3px 10px; min-height: 20px;"
    "}"
    "QPushButton:hover { background: #203040; }"
    "QPushButton:pressed { background: #00b4d8; color: #0f0f1a; }";

constexpr const char* kSectionHeaderStyle =
    "QLabel { color: #8aa8c0; font-weight: bold; font-size: 11px;"
    "  background: #1a2a38; padding: 3px 6px; }";

} // namespace

MmioEndpointsDialog::MmioEndpointsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("MMIO Endpoints"));
    setMinimumSize(900, 500);
    resize(1000, 560);
    setStyleSheet(QLatin1String(kDialogStyle));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    root->addWidget(splitter, 1);

    // -------- Left column: endpoint list --------
    auto* leftWrap = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftWrap);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(4);

    auto* lblEndpoints = new QLabel(QStringLiteral("Endpoints"), leftWrap);
    lblEndpoints->setStyleSheet(QLatin1String(kSectionHeaderStyle));
    leftLayout->addWidget(lblEndpoints);

    m_list = new QListWidget(leftWrap);
    leftLayout->addWidget(m_list, 1);

    auto* leftBtnRow = new QHBoxLayout();
    m_btnAdd    = new QPushButton(QStringLiteral("+ Add"),  leftWrap);
    m_btnRemove = new QPushButton(QStringLiteral("Remove"), leftWrap);
    leftBtnRow->addWidget(m_btnAdd);
    leftBtnRow->addWidget(m_btnRemove);
    leftBtnRow->addStretch();
    leftLayout->addLayout(leftBtnRow);

    splitter->addWidget(leftWrap);

    // -------- Center column: editor --------
    auto* centerWrap = new QWidget(splitter);
    auto* centerLayout = new QVBoxLayout(centerWrap);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(4);

    auto* lblEditor = new QLabel(QStringLiteral("Endpoint properties"), centerWrap);
    lblEditor->setStyleSheet(QLatin1String(kSectionHeaderStyle));
    centerLayout->addWidget(lblEditor);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(4);

    m_editName = new QLineEdit(centerWrap);
    form->addRow(QStringLiteral("Name"), m_editName);

    m_comboTransport = new QComboBox(centerWrap);
    m_comboTransport->addItem(QStringLiteral("UDP listener"),
        static_cast<int>(MmioEndpoint::Transport::UdpListener));
    m_comboTransport->addItem(QStringLiteral("TCP listener"),
        static_cast<int>(MmioEndpoint::Transport::TcpListener));
    m_comboTransport->addItem(QStringLiteral("TCP client"),
        static_cast<int>(MmioEndpoint::Transport::TcpClient));
    m_comboTransport->addItem(QStringLiteral("Serial"),
        static_cast<int>(MmioEndpoint::Transport::Serial));
    form->addRow(QStringLiteral("Transport"), m_comboTransport);

    m_comboFormat = new QComboBox(centerWrap);
    m_comboFormat->addItem(QStringLiteral("JSON"),
        static_cast<int>(MmioEndpoint::Format::Json));
    m_comboFormat->addItem(QStringLiteral("XML"),
        static_cast<int>(MmioEndpoint::Format::Xml));
    m_comboFormat->addItem(QStringLiteral("RAW (key:value)"),
        static_cast<int>(MmioEndpoint::Format::Raw));
    form->addRow(QStringLiteral("Format"), m_comboFormat);

    m_editHost = new QLineEdit(centerWrap);
    m_editHost->setPlaceholderText(QStringLiteral("0.0.0.0 or hostname"));
    form->addRow(QStringLiteral("Host"), m_editHost);

    m_spinPort = new QSpinBox(centerWrap);
    m_spinPort->setRange(0, 65535);
    form->addRow(QStringLiteral("Port"), m_spinPort);

    m_editDevice = new QLineEdit(centerWrap);
    m_editDevice->setPlaceholderText(QStringLiteral("/dev/tty.usbserial (serial only)"));
    form->addRow(QStringLiteral("Serial device"), m_editDevice);

    m_spinBaud = new QSpinBox(centerWrap);
    m_spinBaud->setRange(300, 921600);
    m_spinBaud->setValue(9600);
    form->addRow(QStringLiteral("Baud rate"), m_spinBaud);

    centerLayout->addLayout(form);

    auto* centerBtnRow = new QHBoxLayout();
    m_btnApply = new QPushButton(QStringLiteral("Apply changes"), centerWrap);
    m_lblStatus = new QLabel(QStringLiteral("—"), centerWrap);
    m_lblStatus->setStyleSheet(QStringLiteral("QLabel { color: #8090a0; }"));
    centerBtnRow->addWidget(m_btnApply);
    centerBtnRow->addStretch();
    centerBtnRow->addWidget(m_lblStatus);
    centerLayout->addLayout(centerBtnRow);
    centerLayout->addStretch();

    splitter->addWidget(centerWrap);

    // -------- Right column: discovered variables tree --------
    auto* rightWrap = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightWrap);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(4);

    auto* lblVars = new QLabel(QStringLiteral("Discovered variables"), rightWrap);
    lblVars->setStyleSheet(QLatin1String(kSectionHeaderStyle));
    rightLayout->addWidget(lblVars);

    m_treeVariables = new QTreeWidget(rightWrap);
    m_treeVariables->setColumnCount(2);
    m_treeVariables->setHeaderLabels({QStringLiteral("Variable"),
                                       QStringLiteral("Value")});
    m_treeVariables->setRootIsDecorated(false);
    m_treeVariables->setAlternatingRowColors(true);
    rightLayout->addWidget(m_treeVariables, 1);

    splitter->addWidget(rightWrap);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 0);
    splitter->setStretchFactor(2, 1);
    splitter->setSizes({220, 320, 460});

    // -------- Footer --------
    auto* footer = new QHBoxLayout();
    footer->addStretch();
    m_btnClose = new QPushButton(QStringLiteral("Close"), this);
    footer->addWidget(m_btnClose);
    root->addLayout(footer);

    // Wire signals
    connect(m_list, &QListWidget::currentRowChanged, this,
            &MmioEndpointsDialog::onEndpointSelectionChanged);
    connect(m_btnAdd,    &QPushButton::clicked, this, &MmioEndpointsDialog::onAddEndpoint);
    connect(m_btnRemove, &QPushButton::clicked, this, &MmioEndpointsDialog::onRemoveEndpoint);
    connect(m_btnApply,  &QPushButton::clicked, this, &MmioEndpointsDialog::onApplyEdits);
    connect(m_btnClose,  &QPushButton::clicked, this, &QDialog::accept);

    rebuildEndpointList();
}

MmioEndpointsDialog::~MmioEndpointsDialog() = default;

void MmioEndpointsDialog::rebuildEndpointList()
{
    m_list->clear();
    const QList<MmioEndpoint*> eps = ExternalVariableEngine::instance().endpoints();
    for (MmioEndpoint* ep : eps) {
        const QString label = ep->name().isEmpty()
            ? QStringLiteral("(unnamed) ") + ep->guid().toString(QUuid::WithoutBraces).left(8)
            : ep->name();
        auto* item = new QListWidgetItem(label, m_list);
        item->setData(Qt::UserRole, ep->guid());
    }
    if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
    } else {
        loadEditorFromEndpoint(nullptr);
    }
}

void MmioEndpointsDialog::onEndpointSelectionChanged()
{
    MmioEndpoint* ep = currentEndpoint();
    loadEditorFromEndpoint(ep);
    refreshVariablesTree(ep);

    // Listen for discovered variables on this endpoint so the tree
    // refreshes live while the dialog is open.
    if (ep) {
        connect(ep, &MmioEndpoint::variablesDiscovered,
                this, &MmioEndpointsDialog::onVariablesDiscovered,
                Qt::UniqueConnection);
    }
}

void MmioEndpointsDialog::loadEditorFromEndpoint(MmioEndpoint* ep)
{
    const bool enable = ep != nullptr;
    m_editName->setEnabled(enable);
    m_comboTransport->setEnabled(enable);
    m_comboFormat->setEnabled(enable);
    m_editHost->setEnabled(enable);
    m_spinPort->setEnabled(enable);
    m_editDevice->setEnabled(enable);
    m_spinBaud->setEnabled(enable);
    m_btnApply->setEnabled(enable);
    if (!ep) {
        m_editName->clear();
        m_editHost->clear();
        m_spinPort->setValue(0);
        m_editDevice->clear();
        m_spinBaud->setValue(9600);
        m_lblStatus->setText(QStringLiteral("—"));
        return;
    }
    m_editName->setText(ep->name());
    m_comboTransport->setCurrentIndex(
        m_comboTransport->findData(static_cast<int>(ep->transport())));
    m_comboFormat->setCurrentIndex(
        m_comboFormat->findData(static_cast<int>(ep->format())));
    m_editHost->setText(ep->host());
    m_spinPort->setValue(ep->port());
    m_editDevice->setText(ep->serialDevice());
    m_spinBaud->setValue(ep->serialBaud());
    m_lblStatus->setText(QStringLiteral("guid %1")
                          .arg(ep->guid().toString(QUuid::WithoutBraces).left(8)));
}

void MmioEndpointsDialog::refreshVariablesTree(MmioEndpoint* ep)
{
    m_treeVariables->clear();
    if (!ep) { return; }
    const QStringList names = ep->variableNames();
    for (const QString& name : names) {
        const QVariant v = ep->valueForName(name);
        auto* item = new QTreeWidgetItem(m_treeVariables);
        item->setText(0, name);
        item->setText(1, v.toString());
    }
}

MmioEndpoint* MmioEndpointsDialog::currentEndpoint() const
{
    QListWidgetItem* sel = m_list->currentItem();
    if (!sel) { return nullptr; }
    const QUuid guid = sel->data(Qt::UserRole).toUuid();
    return ExternalVariableEngine::instance().endpoint(guid);
}

void MmioEndpointsDialog::onAddEndpoint()
{
    auto* ep = new MmioEndpoint();
    ep->setGuid(QUuid::createUuid());
    ep->setName(QStringLiteral("New endpoint"));
    ep->setTransport(MmioEndpoint::Transport::UdpListener);
    ep->setFormat(MmioEndpoint::Format::Json);
    ep->setHost(QStringLiteral("0.0.0.0"));
    ep->setPort(12345);
    ExternalVariableEngine::instance().addEndpoint(ep);
    rebuildEndpointList();
    // Select the new one.
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(Qt::UserRole).toUuid() == ep->guid()) {
            m_list->setCurrentRow(i);
            break;
        }
    }
}

void MmioEndpointsDialog::onRemoveEndpoint()
{
    MmioEndpoint* ep = currentEndpoint();
    if (!ep) { return; }
    const QUuid g = ep->guid();
    ExternalVariableEngine::instance().removeEndpoint(g);
    rebuildEndpointList();
}

void MmioEndpointsDialog::onApplyEdits()
{
    MmioEndpoint* ep = currentEndpoint();
    if (!ep) { return; }
    ep->setName(m_editName->text());
    ep->setTransport(static_cast<MmioEndpoint::Transport>(
        m_comboTransport->currentData().toInt()));
    ep->setFormat(static_cast<MmioEndpoint::Format>(
        m_comboFormat->currentData().toInt()));
    ep->setHost(m_editHost->text());
    ep->setPort(static_cast<quint16>(m_spinPort->value()));
    ep->setSerialDevice(m_editDevice->text());
    ep->setSerialBaud(m_spinBaud->value());
    ExternalVariableEngine::instance().updateEndpoint(ep);
    rebuildEndpointList();
    m_lblStatus->setText(QStringLiteral("Applied. Worker restarted."));
}

void MmioEndpointsDialog::onVariablesDiscovered()
{
    refreshVariablesTree(currentEndpoint());
}

} // namespace NereusSDR
