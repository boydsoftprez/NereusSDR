// =================================================================
// src/gui/containers/MmioEndpointsDialog.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-10-25 — The 4-character ID with a Copy button, Direction,
//                 Terminator and the UDP send target; new data
//                 sources start on 127.0.0.1:9000 as in Thetis. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

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
#include <QClipboard>
#include <QGuiApplication>

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
    // R-R3-21 (operator wording, 2026-09-24): plain words, MMIO kept in
    // brackets so Thetis users can find it.
    setWindowTitle(QStringLiteral("Meter Data Sources (MMIO)"));
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

    auto* lblEndpoints = new QLabel(QStringLiteral("Data sources"), leftWrap);
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

    auto* lblEditor = new QLabel(QStringLiteral("Data source settings"), centerWrap);
    lblEditor->setStyleSheet(QLatin1String(kSectionHeaderStyle));
    centerLayout->addWidget(lblEditor);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(4);

    m_editName = new QLineEdit(centerWrap);
    form->addRow(QStringLiteral("Name"), m_editName);

    // The 4-character ID other items and outside programs use to name
    // this data source.
    auto* idRow = new QHBoxLayout();
    m_editId = new QLineEdit(centerWrap);
    m_editId->setObjectName(QStringLiteral("mmioFourCharId"));
    m_editId->setReadOnly(true);
    m_btnCopyId = new QPushButton(QStringLiteral("Copy"), centerWrap);
    m_btnCopyId->setObjectName(QStringLiteral("mmioCopyId"));
    m_btnCopyId->setToolTip(QStringLiteral("Copy the ID to the clipboard"));
    idRow->addWidget(m_editId, 1);
    idRow->addWidget(m_btnCopyId);
    form->addRow(QStringLiteral("ID"), idRow);

    m_comboTransport = new QComboBox(centerWrap);
    m_comboTransport->addItem(QStringLiteral("UDP listener"),
        static_cast<int>(MmioEndpoint::Transport::UdpListener));
    m_comboTransport->addItem(QStringLiteral("TCP listener"),
        static_cast<int>(MmioEndpoint::Transport::TcpListener));
    m_comboTransport->addItem(QStringLiteral("TCP client"),
        static_cast<int>(MmioEndpoint::Transport::TcpClient));
    m_comboTransport->addItem(QStringLiteral("Serial"),
        static_cast<int>(MmioEndpoint::Transport::Serial));
    form->addRow(QStringLiteral("Connection type"), m_comboTransport);

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

    m_comboDirection = new QComboBox(centerWrap);
    m_comboDirection->setObjectName(QStringLiteral("mmioDirection"));
    m_comboDirection->addItem(QStringLiteral("In (receive)"),
        static_cast<int>(MmioEndpoint::Direction::In));
    m_comboDirection->addItem(QStringLiteral("Out (send)"),
        static_cast<int>(MmioEndpoint::Direction::Out));
    m_comboDirection->addItem(QStringLiteral("Both"),
        static_cast<int>(MmioEndpoint::Direction::Both));
    form->addRow(QStringLiteral("Direction"), m_comboDirection);

    m_comboTerminator = new QComboBox(centerWrap);
    m_comboTerminator->setObjectName(QStringLiteral("mmioTerminator"));
    m_comboTerminator->addItem(QStringLiteral("None"),
        static_cast<int>(MmioEndpoint::Terminator::None));
    m_comboTerminator->addItem(QStringLiteral("CR"),
        static_cast<int>(MmioEndpoint::Terminator::Cr));
    m_comboTerminator->addItem(QStringLiteral("LF"),
        static_cast<int>(MmioEndpoint::Terminator::Lf));
    m_comboTerminator->addItem(QStringLiteral("CR LF"),
        static_cast<int>(MmioEndpoint::Terminator::CrLf));
    m_comboTerminator->addItem(QStringLiteral("Custom"),
        static_cast<int>(MmioEndpoint::Terminator::Custom));
    m_comboTerminator->setToolTip(QStringLiteral("Sent after each outgoing message"));
    form->addRow(QStringLiteral("Terminator"), m_comboTerminator);

    m_editCustomTerminator = new QLineEdit(centerWrap);
    m_editCustomTerminator->setObjectName(QStringLiteral("mmioCustomTerminator"));
    m_editCustomTerminator->setPlaceholderText(QStringLiteral("for example ;\\r\\n"));
    form->addRow(QStringLiteral("Custom terminator"), m_editCustomTerminator);

    m_editUdpTargetHost = new QLineEdit(centerWrap);
    m_editUdpTargetHost->setObjectName(QStringLiteral("mmioUdpTargetHost"));
    m_editUdpTargetHost->setPlaceholderText(QStringLiteral("127.0.0.1 (UDP only)"));
    form->addRow(QStringLiteral("Send to address"), m_editUdpTargetHost);

    m_spinUdpTargetPort = new QSpinBox(centerWrap);
    m_spinUdpTargetPort->setObjectName(QStringLiteral("mmioUdpTargetPort"));
    m_spinUdpTargetPort->setRange(0, 65535);
    m_spinUdpTargetPort->setValue(10000);
    form->addRow(QStringLiteral("Send to port"), m_spinUdpTargetPort);

    centerLayout->addLayout(form);

    auto* centerBtnRow = new QHBoxLayout();
    m_btnApply = new QPushButton(QStringLiteral("Apply changes"), centerWrap);
    m_lblStatus = new QLabel(QStringLiteral("\u2013"), centerWrap);
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

    auto* lblVars = new QLabel(QStringLiteral("Values received"), rightWrap);
    lblVars->setStyleSheet(QLatin1String(kSectionHeaderStyle));
    rightLayout->addWidget(lblVars);

    m_treeVariables = new QTreeWidget(rightWrap);
    m_treeVariables->setColumnCount(2);
    m_treeVariables->setHeaderLabels({QStringLiteral("Name"),
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
    connect(m_btnCopyId, &QPushButton::clicked, this, &MmioEndpointsDialog::onCopyId);
    connect(m_comboTransport, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MmioEndpointsDialog::updateFieldStates);
    connect(m_comboDirection, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MmioEndpointsDialog::updateFieldStates);
    connect(m_comboTerminator, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MmioEndpointsDialog::updateFieldStates);

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
    m_editId->setEnabled(enable);
    m_btnCopyId->setEnabled(enable);
    m_comboDirection->setEnabled(enable);
    m_comboTerminator->setEnabled(enable);
    m_btnApply->setEnabled(enable);
    if (!ep) {
        m_editName->clear();
        m_editId->clear();
        m_editCustomTerminator->clear();
        m_editUdpTargetHost->clear();
        m_spinUdpTargetPort->setValue(10000);
        updateFieldStates();
        m_editHost->clear();
        m_spinPort->setValue(0);
        m_editDevice->clear();
        m_spinBaud->setValue(9600);
        m_lblStatus->setText(QStringLiteral("\u2013"));
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
    m_editId->setText(ep->fourChar());
    m_comboDirection->setCurrentIndex(
        m_comboDirection->findData(static_cast<int>(ep->direction())));
    m_comboTerminator->setCurrentIndex(
        m_comboTerminator->findData(static_cast<int>(ep->terminator())));
    m_editCustomTerminator->setText(ep->customTerminator());
    m_editUdpTargetHost->setText(ep->udpTargetHost());
    m_spinUdpTargetPort->setValue(ep->udpTargetPort());
    updateFieldStates();
    m_lblStatus->setText(QStringLiteral("ID %1").arg(ep->fourChar()));
}

void MmioEndpointsDialog::updateFieldStates()
{
    // Fields that do not apply stay visible and disabled.
    const bool haveEndpoint = currentEndpoint() != nullptr;
    const bool sends = m_comboDirection->currentData().toInt()
                       != static_cast<int>(MmioEndpoint::Direction::In);
    const bool udp = m_comboTransport->currentData().toInt()
                     == static_cast<int>(MmioEndpoint::Transport::UdpListener);
    const bool custom = m_comboTerminator->currentData().toInt()
                        == static_cast<int>(MmioEndpoint::Terminator::Custom);
    m_comboTerminator->setEnabled(haveEndpoint && sends);
    m_editCustomTerminator->setEnabled(haveEndpoint && sends && custom);
    m_editUdpTargetHost->setEnabled(haveEndpoint && sends && udp);
    m_spinUdpTargetPort->setEnabled(haveEndpoint && sends && udp);
}

void MmioEndpointsDialog::onCopyId()
{
    if (QClipboard* clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(m_editId->text());
    }
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
    ep->setName(QStringLiteral("New data source"));
    ep->setTransport(MmioEndpoint::Transport::UdpListener);
    ep->setFormat(MmioEndpoint::Format::Json);
    // From Thetis MeterManager.cs:41409-41419 [v2.10.3.15] — a new
    // endpoint is a UDP listener on 127.0.0.1:9000, direction IN, no
    // terminator.
    ep->setHost(QStringLiteral("127.0.0.1"));
    ep->setPort(9000);
    ep->setDirection(MmioEndpoint::Direction::In);
    ep->setTerminator(MmioEndpoint::Terminator::None);
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
    ep->setDirection(static_cast<MmioEndpoint::Direction>(
        m_comboDirection->currentData().toInt()));
    ep->setTerminator(static_cast<MmioEndpoint::Terminator>(
        m_comboTerminator->currentData().toInt()));
    ep->setCustomTerminator(m_editCustomTerminator->text());
    ep->setUdpTargetHost(m_editUdpTargetHost->text());
    ep->setUdpTargetPort(static_cast<quint16>(m_spinUdpTargetPort->value()));
    ExternalVariableEngine::instance().updateEndpoint(ep);
    rebuildEndpointList();
    m_lblStatus->setText(QStringLiteral("Applied. The data source restarted with the new settings."));
}

void MmioEndpointsDialog::onVariablesDiscovered()
{
    refreshVariablesTree(currentEndpoint());
}

} // namespace NereusSDR
