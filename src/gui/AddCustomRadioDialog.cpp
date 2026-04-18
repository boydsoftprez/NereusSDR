// =================================================================
// src/gui/AddCustomRadioDialog.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/frmAddCustomRadio.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/frmAddCustomRadio.Designer.cs (upstream has no top-of-file header — project-level LICENSE applies)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*  frmAddCustomRadio.cs

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

//
// Upstream source 'Project Files/Source/Console/frmAddCustomRadio.Designer.cs' has no top-of-file GPL header —
// project-level Thetis LICENSE applies.

#include "AddCustomRadioDialog.h"
#include "core/BoardCapabilities.h"
#include "core/LogCategories.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QDialogButtonBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QHostAddress>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

AddCustomRadioDialog::AddCustomRadioDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Add Custom Radio"));
    setMinimumWidth(420);
    setModal(true);

    buildUi();
    validateFields();
}

AddCustomRadioDialog::~AddCustomRadioDialog() = default;

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

void AddCustomRadioDialog::buildUi()
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setSpacing(10);
    outerLayout->setContentsMargins(14, 14, 14, 14);

    // --- Explanatory label ---
    // Thetis frmAddCustomRadio.Designer.cs:24 — labelTS1 bold Courier New 9.75pt,
    // multi-line warning text (resource string). NereusSDR uses a plain label.
    auto* infoLabel = new QLabel(this);
    infoLabel->setText(
        QStringLiteral(
            "Add a radio that is not visible on the local subnet.\n"
            "Use this for radios accessed over VPN or a static route.\n"
            "Discovery will not find it automatically — it is saved\n"
            "to your settings and shown in the list at launch."));
    infoLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #8090a0; font-size: 12px; padding: 4px; }"));
    outerLayout->addWidget(infoLabel);

    // --- Form ---
    auto* formGroup = new QGroupBox(QStringLiteral("Radio Details"), this);
    auto* form = new QFormLayout(formGroup);
    form->setSpacing(8);
    form->setContentsMargins(10, 14, 10, 10);

    static const QString kFieldStyle = QStringLiteral(
        "QLineEdit, QSpinBox, QComboBox {"
        "  background: #12202e; color: #c8d8e8;"
        "  border: 1px solid #304050; border-radius: 3px; padding: 4px 6px;"
        "}"
        "QLineEdit:focus, QSpinBox:focus, QComboBox:focus {"
        "  border-color: #00b4d8;"
        "}");

    // Name — NereusSDR addition (friendly label for the saved entry)
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(QStringLiteral("e.g. Remote ANAN-G2 (VPN)"));
    m_nameEdit->setStyleSheet(kFieldStyle);
    form->addRow(QStringLiteral("Name:"), m_nameEdit);

    // IP — from Thetis txtSpecificRadio default "192.168.0.155:1024" (Designer.cs:64)
    // NereusSDR splits it into separate IP and Port fields.
    m_ipEdit = new QLineEdit(this);
    m_ipEdit->setPlaceholderText(QStringLiteral("192.168.0.155"));
    m_ipEdit->setStyleSheet(kFieldStyle);
    // Permissive validator — exact validity checked in validateFields via QHostAddress
    QRegularExpression ipRe(QStringLiteral(
        R"(^((25[0-5]|2[0-4]\d|[01]?\d\d?)\.){3}(25[0-5]|2[0-4]\d|[01]?\d\d?)$)"));
    m_ipEdit->setValidator(new QRegularExpressionValidator(ipRe, m_ipEdit));
    form->addRow(QStringLiteral("IP Address:"), m_ipEdit);

    // Port — from Thetis txtSpecificRadio default ":1024" (Designer.cs:64)
    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(1024);   // From Thetis Designer.cs:64 default "192.168.0.155:1024"
    m_portSpin->setStyleSheet(kFieldStyle);
    form->addRow(QStringLiteral("Port:"), m_portSpin);

    // MAC — optional; Thetis didn't have this field. NereusSDR adds it to enable
    // Pin-to-MAC (AppSettings::saveRadio flag from Task 15).
    m_macEdit = new QLineEdit(this);
    m_macEdit->setPlaceholderText(QStringLiteral("AA:BB:CC:DD:EE:FF  (optional)"));
    m_macEdit->setStyleSheet(kFieldStyle);
    form->addRow(QStringLiteral("MAC Address:"), m_macEdit);

    // Board combo — expanded from Thetis txtBoard (ReadOnly; Thetis populated it
    // externally from the picker). NereusSDR makes it a writable combo.
    // Source: frmAddCustomRadio.Designer.cs:86 "Board:" label, txtBoard ReadOnly
    m_boardCombo = new QComboBox(this);
    m_boardCombo->setStyleSheet(kFieldStyle);
    populateBoardCombo();
    form->addRow(QStringLiteral("Board:"), m_boardCombo);

    // Protocol — from Thetis comboProtocol (Designer.cs:79-84)
    // Items "Protocol 1" / "Protocol 2"; SelectedIndex = 0 (frmAddCustomRadio.cs:60)
    m_protocolCombo = new QComboBox(this);
    m_protocolCombo->setStyleSheet(kFieldStyle);
    populateProtocolCombo();
    form->addRow(QStringLiteral("Protocol:"), m_protocolCombo);

    outerLayout->addWidget(formGroup);

    // --- Options ---
    auto* optGroup = new QGroupBox(QStringLiteral("Options"), this);
    auto* optLayout = new QVBoxLayout(optGroup);
    optLayout->setContentsMargins(10, 14, 10, 10);

    static const QString kCheckStyle = QStringLiteral(
        "QCheckBox { color: #c8d8e8; }"
        "QCheckBox::indicator { border: 1px solid #304050; background: #12202e; width: 14px; height: 14px; }"
        "QCheckBox::indicator:checked { background: #00b4d8; }");

    // Pin to MAC — AppSettings::saveRadio pinToMac flag (Task 15)
    m_pinToMacCheck = new QCheckBox(
        QStringLiteral("Pin to MAC  (skip radios at this IP with a different MAC)"), this);
    m_pinToMacCheck->setStyleSheet(kCheckStyle);
    optLayout->addWidget(m_pinToMacCheck);

    // Auto-connect — AppSettings::saveRadio autoConnect flag (Task 15/17)
    m_autoConnectCheck = new QCheckBox(
        QStringLiteral("Auto-connect on launch"), this);
    m_autoConnectCheck->setStyleSheet(kCheckStyle);
    optLayout->addWidget(m_autoConnectCheck);

    outerLayout->addWidget(optGroup);

    // --- Test button + result label ---
    auto* testLayout = new QHBoxLayout();
    m_testButton = new QPushButton(QStringLiteral("Test Connection"), this);
    m_testButton->setAutoDefault(false);
    m_testButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: #304050; color: #c8d8e8;"
        "  border: 1px solid #405060; border-radius: 4px; padding: 5px 10px;"
        "}"
        "QPushButton:hover { background: #405060; }"
        "QPushButton:disabled { background: #1a2a3a; color: #404858; }"));
    connect(m_testButton, &QPushButton::clicked, this, &AddCustomRadioDialog::onTestClicked);
    testLayout->addWidget(m_testButton);

    m_testResultLabel = new QLabel(this);
    m_testResultLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #8090a0; font-size: 11px; padding-left: 6px; }"));
    testLayout->addWidget(m_testResultLabel, /*stretch=*/1);
    outerLayout->addLayout(testLayout);

    // --- OK / Cancel ---
    auto* btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    m_okButton     = btnBox->button(QDialogButtonBox::Ok);
    m_cancelButton = btnBox->button(QDialogButtonBox::Cancel);

    // Style buttons like ConnectionPanel primary/secondary styles
    m_okButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: #00b4d8; color: #fff;"
        "  border: none; border-radius: 4px; padding: 6px 16px; font-weight: bold;"
        "}"
        "QPushButton:hover { background: #0096b7; }"
        "QPushButton:disabled { background: #303850; color: #606878; }"));
    m_cancelButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: #304050; color: #c8d8e8;"
        "  border: 1px solid #405060; border-radius: 4px; padding: 6px 16px;"
        "}"
        "QPushButton:hover { background: #405060; }"));

    connect(m_okButton,     &QPushButton::clicked, this, &AddCustomRadioDialog::onAccept);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    outerLayout->addWidget(btnBox);

    // Dark theme
    setStyleSheet(QStringLiteral(
        "QDialog { background: #0f0f1a; }"
        "QGroupBox {"
        "  color: #8090a0;"
        "  border: 1px solid #203040;"
        "  border-radius: 4px;"
        "  margin-top: 8px;"
        "  padding-top: 12px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 10px;"
        "  padding: 0 3px;"
        "}"
        "QLabel { color: #a0b0c0; }"));

    // Wire validation to field changes
    connect(m_nameEdit,     &QLineEdit::textChanged, this, &AddCustomRadioDialog::validateFields);
    connect(m_ipEdit,       &QLineEdit::textChanged, this, &AddCustomRadioDialog::validateFields);
    connect(m_portSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AddCustomRadioDialog::validateFields);
    connect(m_macEdit, &QLineEdit::textChanged, this, &AddCustomRadioDialog::validateFields);
    connect(m_boardCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddCustomRadioDialog::onBoardChanged);
}

// ---------------------------------------------------------------------------
// Populate combos
// ---------------------------------------------------------------------------

void AddCustomRadioDialog::populateBoardCombo()
{
    // Iterate every HPSDRHW value (skip Unknown).
    // Source: HpsdrModel.h:40-52 — Atlas(0) … SaturnMKII(11), Unknown(999)
    // DisplayName comes from BoardCapsTable::forBoard(hw).displayName.
    // (BoardCapabilities.h:57 const char* displayName)
    // Protocol auto-select on board change: BoardCapsTable::forBoard(hw).protocol
    static const HPSDRHW kBoards[] = {
        HPSDRHW::Atlas,
        HPSDRHW::Hermes,
        HPSDRHW::HermesII,
        HPSDRHW::Angelia,
        HPSDRHW::Orion,
        HPSDRHW::OrionMKII,
        HPSDRHW::HermesLite,
        HPSDRHW::Saturn,
        HPSDRHW::SaturnMKII,
    };

    for (HPSDRHW hw : kBoards) {
        const BoardCapabilities& caps = BoardCapsTable::forBoard(hw);
        QString label = QString::fromUtf8(caps.displayName);
        if (label.isEmpty()) {
            // Fallback — shouldn't happen but be safe
            label = QStringLiteral("Board %1").arg(static_cast<int>(hw));
        }
        m_boardCombo->addItem(label, QVariant::fromValue(static_cast<int>(hw)));
    }

    // Default to HermesLite (most common hobbyist board)
    int hlIdx = m_boardCombo->findData(QVariant::fromValue(static_cast<int>(HPSDRHW::HermesLite)));
    if (hlIdx >= 0) {
        m_boardCombo->setCurrentIndex(hlIdx);
    }
}

void AddCustomRadioDialog::populateProtocolCombo()
{
    // From Thetis frmAddCustomRadio.Designer.cs:81-82 — items "Protocol 1" / "Protocol 2"
    // From frmAddCustomRadio.cs:60 — SelectedIndex = 0 (Protocol 1 default)
    m_protocolCombo->addItem(QStringLiteral("Protocol 1"),
                             QVariant::fromValue(static_cast<int>(ProtocolVersion::Protocol1)));
    m_protocolCombo->addItem(QStringLiteral("Protocol 2"),
                             QVariant::fromValue(static_cast<int>(ProtocolVersion::Protocol2)));
    m_protocolCombo->setCurrentIndex(0);  // P1 default — frmAddCustomRadio.cs:60
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void AddCustomRadioDialog::onBoardChanged(int /*index*/)
{
    // Auto-select the correct protocol from BoardCapsTable when the user changes
    // the board. The user can still manually override after.
    int hwInt = m_boardCombo->currentData().toInt();
    HPSDRHW hw = static_cast<HPSDRHW>(hwInt);
    const BoardCapabilities& caps = BoardCapsTable::forBoard(hw);

    int protoInt = static_cast<int>(caps.protocol);
    int idx = m_protocolCombo->findData(QVariant::fromValue(protoInt));
    if (idx >= 0) {
        m_protocolCombo->setCurrentIndex(idx);
    }
}

void AddCustomRadioDialog::onTestClicked()
{
    // Thetis frmAddCustomRadio.cs did not implement a real test probe —
    // the button is absent from the Designer.cs and the .cs logic only reads
    // back the filled fields. A real unicast probe (bind QUdpSocket to the
    // selected IP, send P1/P2 probe, wait for reply via parseP1Reply /
    // parseP2Reply) would require NIC enumeration and asynchronous callbacks
    // outside the scope of Task 16. Show an informative message instead.
    m_testResultLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #d4a030; font-size: 11px; padding-left: 6px; }"));
    m_testResultLabel->setText(
        QStringLiteral("Test not yet implemented — save and try connecting from the main list."));

    qCDebug(lcDiscovery) << "AddCustomRadioDialog: test probe stub for"
                         << m_ipEdit->text() << "port" << m_portSpin->value();
}

void AddCustomRadioDialog::validateFields()
{
    if (!m_okButton) {
        return;
    }

    bool nameOk = !m_nameEdit->text().trimmed().isEmpty();

    // IP: must parse as a valid IPv4 address
    QHostAddress addr;
    bool ipOk = addr.setAddress(m_ipEdit->text().trimmed());

    // Port: QSpinBox already clamps 1..65535, always valid if set
    bool portOk = (m_portSpin->value() >= 1 && m_portSpin->value() <= 65535);

    // MAC: optional — if non-empty must match AA:BB:CC:DD:EE:FF
    bool macOk = true;
    const QString mac = m_macEdit->text().trimmed();
    if (!mac.isEmpty()) {
        static const QRegularExpression kMacRe(
            QStringLiteral(R"(^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$)"));
        macOk = kMacRe.match(mac).hasMatch();
    }

    m_okButton->setEnabled(nameOk && ipOk && portOk && macOk);

    // Visual feedback on MAC field
    if (!macOk) {
        m_macEdit->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #12202e; color: #e06060;"
            "  border: 1px solid #804040; border-radius: 3px; padding: 4px 6px; }"));
    } else {
        m_macEdit->setStyleSheet(QStringLiteral(
            "QLineEdit { background: #12202e; color: #c8d8e8;"
            "  border: 1px solid #304050; border-radius: 3px; padding: 4px 6px; }"
            "QLineEdit:focus { border-color: #00b4d8; }"));
    }
}

void AddCustomRadioDialog::onAccept()
{
    // Final validation guard before accepting
    QHostAddress addr;
    const bool ipOk = addr.setAddress(m_ipEdit->text().trimmed());
    const bool nameOk = !m_nameEdit->text().trimmed().isEmpty();
    const QString mac = m_macEdit->text().trimmed();
    bool macOk = true;
    if (!mac.isEmpty()) {
        static const QRegularExpression kMacRe(
            QStringLiteral(R"(^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$)"));
        macOk = kMacRe.match(mac).hasMatch();
    }

    if (!nameOk || !ipOk || !macOk) {
        validateFields();
        return;
    }

    QDialog::accept();
}

// ---------------------------------------------------------------------------
// Result accessors
// ---------------------------------------------------------------------------

RadioInfo AddCustomRadioDialog::result() const
{
    RadioInfo info;
    info.name    = m_nameEdit->text().trimmed();

    // IP + Port — split from Thetis txtSpecificRadio "IP:Port" (Designer.cs:64)
    info.address = QHostAddress(m_ipEdit->text().trimmed());
    info.port    = static_cast<quint16>(m_portSpin->value());

    // MAC — optional; empty string is valid for radios without a known MAC
    info.macAddress = m_macEdit->text().trimmed().toUpper();

    // If the user left MAC blank, synthesise a unique key from IP so that
    // AppSettings can store and find the entry. Use a synthetic prefix.
    if (info.macAddress.isEmpty()) {
        info.macAddress = QStringLiteral("MANUAL:%1:%2")
            .arg(info.address.toString())
            .arg(info.port);
    }

    // Board type
    info.boardType = static_cast<HPSDRHW>(m_boardCombo->currentData().toInt());

    // Capabilities from board
    const BoardCapabilities& caps = BoardCapsTable::forBoard(info.boardType);
    info.adcCount            = caps.adcCount;
    info.maxReceivers        = caps.maxReceivers;
    info.maxSampleRate       = caps.maxSampleRate;
    info.hasDiversityReceiver = caps.hasDiversityReceiver;
    info.hasPureSignal        = caps.hasPureSignal;

    // Protocol — from Thetis comboProtocol (Designer.cs:79-84)
    info.protocol = static_cast<ProtocolVersion>(m_protocolCombo->currentData().toInt());

    info.firmwareVersion = 0;   // Unknown for manually added radios
    info.inUse           = false;

    return info;
}

bool AddCustomRadioDialog::pinToMac() const
{
    return m_pinToMacCheck->isChecked();
}

bool AddCustomRadioDialog::autoConnect() const
{
    return m_autoConnectCheck->isChecked();
}

} // namespace NereusSDR
