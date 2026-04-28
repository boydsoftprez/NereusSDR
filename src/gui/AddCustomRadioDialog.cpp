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
//   2026-04-27 — Phase 3Q Task 4: replaced board (HPSDRHW) dropdown with
//                 16-SKU model (HPSDRModel) picker organised by silicon
//                 family; replaced OK/Cancel with Probe-and-connect /
//                 Save-offline / Cancel; added onProbeClicked() /
//                 onSaveOfflineClicked(); added showInlineError /
//                 showInlineInfo / showProbingOverlay / hideProbingOverlay
//                 inline-feedback helpers. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
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
#include "core/HpsdrModel.h"
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
#include <QFrame>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QHostAddress>
#include <QStandardItemModel>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Style constants (local to this translation unit)
// ---------------------------------------------------------------------------

static const QString kFieldStyle = QStringLiteral(
    "QLineEdit, QSpinBox, QComboBox {"
    "  background: #12202e; color: #c8d8e8;"
    "  border: 1px solid #304050; border-radius: 3px; padding: 4px 6px;"
    "}"
    "QLineEdit:focus, QSpinBox:focus, QComboBox:focus {"
    "  border-color: #00b4d8;"
    "}");

static const QString kPrimaryButtonStyle = QStringLiteral(
    "QPushButton {"
    "  background: #00b4d8; color: #fff;"
    "  border: none; border-radius: 4px; padding: 6px 16px; font-weight: bold;"
    "}"
    "QPushButton:hover { background: #0096b7; }"
    "QPushButton:disabled { background: #303850; color: #606878; }");

static const QString kSecondaryButtonStyle = QStringLiteral(
    "QPushButton {"
    "  background: #304050; color: #c8d8e8;"
    "  border: 1px solid #405060; border-radius: 4px; padding: 6px 16px;"
    "}"
    "QPushButton:hover { background: #405060; }");

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

AddCustomRadioDialog::AddCustomRadioDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Add Custom Radio"));
    setMinimumWidth(460);
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

    // MAC — optional; NereusSDR addition to enable Pin-to-MAC.
    m_macEdit = new QLineEdit(this);
    m_macEdit->setPlaceholderText(QStringLiteral("AA:BB:CC:DD:EE:FF  (optional)"));
    m_macEdit->setStyleSheet(kFieldStyle);
    form->addRow(QStringLiteral("MAC Address:"), m_macEdit);

    // Model combo — Phase 3Q Task 4: replaces board combo with 16-SKU picker.
    // Organized by silicon family (disabled header items for visual grouping).
    // From design §4.4: "Auto-detect" default; user picks a specific SKU when
    // they know the product — necessary for shared-silicon families.
    m_modelCombo = new QComboBox(this);
    m_modelCombo->setObjectName(QStringLiteral("modelCombo"));
    m_modelCombo->setStyleSheet(kFieldStyle);
    populateModelCombo();
    form->addRow(QStringLiteral("Model:"), m_modelCombo);

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

    // Pin to MAC — AppSettings::saveRadio pinToMac flag
    m_pinToMacCheck = new QCheckBox(
        QStringLiteral("Pin to MAC  (skip radios at this IP with a different MAC)"), this);
    m_pinToMacCheck->setStyleSheet(kCheckStyle);
    optLayout->addWidget(m_pinToMacCheck);

    // Auto-connect — AppSettings::saveRadio autoConnect flag
    m_autoConnectCheck = new QCheckBox(
        QStringLiteral("Auto-connect on launch"), this);
    m_autoConnectCheck->setStyleSheet(kCheckStyle);
    optLayout->addWidget(m_autoConnectCheck);

    outerLayout->addWidget(optGroup);

    // --- Inline feedback band (hidden by default) ---
    // Used by showInlineError / showInlineInfo to surface probe results.
    // Error = #c14848 red; Info = #5985b8 blue (design §6.3-6.5).
    m_feedbackFrame = new QFrame(this);
    m_feedbackFrame->setFrameShape(QFrame::StyledPanel);
    m_feedbackFrame->setContentsMargins(0, 0, 0, 0);
    auto* feedbackLayout = new QHBoxLayout(m_feedbackFrame);
    feedbackLayout->setContentsMargins(10, 6, 10, 6);

    m_feedbackLabel = new QLabel(m_feedbackFrame);
    m_feedbackLabel->setWordWrap(true);
    feedbackLayout->addWidget(m_feedbackLabel);
    m_feedbackFrame->hide();
    outerLayout->addWidget(m_feedbackFrame);

    // --- Action button row ---
    // Phase 3Q Task 4: "Probe and connect now" (primary, default) +
    // "Save offline" (secondary) + "Cancel" (secondary, right-aligned).
    // Design §4.4: probe button is the dialog's default action.
    m_probeButton = new QPushButton(QStringLiteral("Probe and connect now"), this);
    m_probeButton->setObjectName(QStringLiteral("probeButton"));
    m_probeButton->setStyleSheet(kPrimaryButtonStyle);
    m_probeButton->setDefault(true);
    connect(m_probeButton, &QPushButton::clicked,
            this, &AddCustomRadioDialog::onProbeClicked);

    m_saveOfflineButton = new QPushButton(QStringLiteral("Save offline"), this);
    m_saveOfflineButton->setObjectName(QStringLiteral("saveOfflineButton"));
    m_saveOfflineButton->setStyleSheet(kSecondaryButtonStyle);
    connect(m_saveOfflineButton, &QPushButton::clicked,
            this, &AddCustomRadioDialog::onSaveOfflineClicked);

    auto* cancelButton = new QPushButton(QStringLiteral("Cancel"), this);
    cancelButton->setStyleSheet(kSecondaryButtonStyle);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    auto* btnRow = new QHBoxLayout();
    btnRow->addWidget(m_probeButton);
    btnRow->addWidget(m_saveOfflineButton);
    btnRow->addStretch();
    btnRow->addWidget(cancelButton);
    outerLayout->addLayout(btnRow);

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
    connect(m_nameEdit,     &QLineEdit::textChanged,
            this, &AddCustomRadioDialog::validateFields);
    connect(m_ipEdit,       &QLineEdit::textChanged,
            this, &AddCustomRadioDialog::validateFields);
    connect(m_portSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AddCustomRadioDialog::validateFields);
    connect(m_macEdit, &QLineEdit::textChanged,
            this, &AddCustomRadioDialog::validateFields);
    connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddCustomRadioDialog::onModelChanged);
}

// ---------------------------------------------------------------------------
// Populate combos
// ---------------------------------------------------------------------------

void AddCustomRadioDialog::populateModelCombo()
{
    // Phase 3Q Task 4: model-based population with silicon-family headers.
    // 16 SKUs from HPSDRModel (enums.cs:109 [v2.10.3.13]), FIRST+1..LAST-1.
    // Family grouping matches design §4.4.
    //
    // Uses disabled QStandardItems for family header rows so the user cannot
    // accidentally select a family label; enabled items carry an int data
    // value matching their HPSDRModel enum int.

    // Helper: add a non-selectable separator line.
    auto addSeparator = [&]() {
        m_modelCombo->addItem(QStringLiteral("──────────"));
        auto* model = qobject_cast<QStandardItemModel*>(m_modelCombo->model());
        if (model) {
            auto* item = model->item(m_modelCombo->count() - 1);
            if (item) {
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            }
        }
    };

    // Helper: add a bold, non-selectable silicon-family header.
    auto addFamilyHeader = [&](const QString& label) {
        m_modelCombo->addItem(QStringLiteral("  ") + label);
        auto* model = qobject_cast<QStandardItemModel*>(m_modelCombo->model());
        if (model) {
            auto* item = model->item(m_modelCombo->count() - 1);
            if (item) {
                QFont f = item->font();
                f.setBold(true);
                item->setFont(f);
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            }
        }
    };

    // Helper: add a selectable SKU item indented under the family header.
    auto addSku = [&](HPSDRModel m) {
        m_modelCombo->addItem(
            QStringLiteral("    %1").arg(
                QString::fromUtf8(displayName(m))),
            QVariant::fromValue(static_cast<int>(m)));
    };

    // --- Auto-detect always first (FIRST sentinel value = -1) ---
    // When the user probes successfully the probe result fills in the model;
    // when FIRST is stored we treat it as "auto-detect".
    m_modelCombo->addItem(
        QStringLiteral("Auto-detect (probe will fill this in)"),
        QVariant::fromValue(static_cast<int>(HPSDRModel::FIRST)));

    addSeparator();

    // --- Atlas / Metis ---
    addFamilyHeader(QStringLiteral("Atlas / Metis"));
    addSku(HPSDRModel::HPSDR);           // "HPSDR (Atlas/Metis)"

    // --- Hermes (1 ADC) ---
    addFamilyHeader(QStringLiteral("Hermes (1 ADC)"));
    addSku(HPSDRModel::HERMES);          // "Hermes"
    addSku(HPSDRModel::ANAN10);          // "ANAN-10"
    addSku(HPSDRModel::ANAN100);         // "ANAN-100"

    // --- Hermes II (1 ADC) ---
    addFamilyHeader(QStringLiteral("Hermes II (1 ADC)"));
    addSku(HPSDRModel::ANAN10E);         // "ANAN-10E"
    addSku(HPSDRModel::ANAN100B);        // "ANAN-100B"

    // --- Angelia (2 ADC) ---
    addFamilyHeader(QStringLiteral("Angelia (2 ADC)"));
    addSku(HPSDRModel::ANAN100D);        // "ANAN-100D"

    // --- Orion (2 ADC · 50 V) ---
    addFamilyHeader(QStringLiteral("Orion (2 ADC · 50 V)"));
    addSku(HPSDRModel::ANAN200D);        // "ANAN-200D"

    // --- Orion MkII (2 ADC · MKII BPF) ---
    addFamilyHeader(QStringLiteral("Orion MkII (2 ADC · MKII BPF)"));
    addSku(HPSDRModel::ORIONMKII);       // "Orion MkII"
    addSku(HPSDRModel::ANAN7000D);       // "ANAN-7000DLE"
    addSku(HPSDRModel::ANAN8000D);       // "ANAN-8000DLE"
    addSku(HPSDRModel::ANVELINAPRO3);    // "Anvelina Pro 3"
    addSku(HPSDRModel::REDPITAYA);       // "Red Pitaya"  //DH1KLM

    // --- Hermes Lite 2 ---
    addFamilyHeader(QStringLiteral("Hermes Lite 2"));
    addSku(HPSDRModel::HERMESLITE);      // "Hermes Lite 2"  //MI0BOT

    // --- Saturn (ANAN-G2) ---  //G8NJJ
    addFamilyHeader(QStringLiteral("Saturn (ANAN-G2)"));
    addSku(HPSDRModel::ANAN_G2);         // "ANAN-G2"    //G8NJJ
    addSku(HPSDRModel::ANAN_G2_1K);      // "ANAN-G2 1K"  //G8NJJ

    m_modelCombo->setCurrentIndex(0);    // Auto-detect default
}

void AddCustomRadioDialog::populateProtocolCombo()
{
    // From Thetis frmAddCustomRadio.Designer.cs:81-82 — items "Protocol 1" / "Protocol 2"
    // From frmAddCustomRadio.cs:60 — SelectedIndex = 0 (Protocol 1 default)
    // Phase 3Q Task 4: add "Auto-detect" sentinel (-1) for Save-offline guard.
    m_protocolCombo->addItem(QStringLiteral("Auto-detect"),
                             QVariant::fromValue(-1));
    m_protocolCombo->addItem(QStringLiteral("Protocol 1"),
                             QVariant::fromValue(static_cast<int>(ProtocolVersion::Protocol1)));
    m_protocolCombo->addItem(QStringLiteral("Protocol 2"),
                             QVariant::fromValue(static_cast<int>(ProtocolVersion::Protocol2)));
    m_protocolCombo->setCurrentIndex(0);  // Auto-detect default; probe will fill this in
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void AddCustomRadioDialog::onModelChanged(int /*index*/)
{
    // When the user picks a specific SKU, auto-select the correct protocol
    // from BoardCapabilities unless they've already pinned it.
    const int modelInt = m_modelCombo->currentData().toInt();
    const auto model = static_cast<HPSDRModel>(modelInt);
    if (model == HPSDRModel::FIRST) {
        return;  // Auto-detect — don't touch protocol
    }

    const HPSDRHW hw = boardForModel(model);
    const BoardCapabilities& caps = BoardCapsTable::forBoard(hw);
    const int protoInt = static_cast<int>(caps.protocol);
    const int idx = m_protocolCombo->findData(QVariant::fromValue(protoInt));
    if (idx >= 0) {
        m_protocolCombo->setCurrentIndex(idx);
    }
}

void AddCustomRadioDialog::onProbeClicked()
{
    // Design §4.4 / §5.1: probe the entered address before accepting.
    // Failure preserves the form + shows a typed error; button flips to "Retry probe".
    const QString ipText = m_ipEdit->text().trimmed();
    QHostAddress addr;
    if (!addr.setAddress(ipText)) {
        showInlineError(QStringLiteral("\"%1\" isn't a valid IP address.").arg(ipText));
        return;
    }

    const bool nameOk = !m_nameEdit->text().trimmed().isEmpty();
    if (!nameOk) {
        showInlineError(QStringLiteral("Please enter a name for this radio."));
        return;
    }

    const quint16 port = static_cast<quint16>(m_portSpin->value());

    showProbingOverlay();  // disables buttons, shows "Probing…" text

    // Heap-allocate with 'this' parent so it is auto-destroyed if the dialog
    // closes mid-probe (e.g. Cancel or window close).
    auto* disc = new RadioDiscovery(this);

    connect(disc, &RadioDiscovery::radioDiscovered, this,
            [this, disc](const RadioInfo& info) {
                hideProbingOverlay();
                m_probedInfo = info;

                // User-picked model overrides the probe-detected silicon family default.
                // Only override when the user chose something other than Auto-detect.
                const int userModelInt = m_modelCombo->currentData().toInt();
                const auto userModel = static_cast<HPSDRModel>(userModelInt);
                if (userModel != HPSDRModel::FIRST) {
                    m_probedInfo.modelOverride = userModel;
                }

                disc->deleteLater();
                QDialog::accept();  // dialog closes; caller saves + connects
            });

    connect(disc, &RadioDiscovery::probeFailed, this,
            [this, disc, ipText](const QHostAddress&, quint16) {
                hideProbingOverlay();
                showInlineError(
                    QStringLiteral(
                        "Couldn't reach %1 after 1.5 s.\n"
                        "Radio may be off, IP may be wrong, or VPN tunnel may be down.\n"
                        "Your form is preserved — change a field and retry, "
                        "or save offline for later.")
                        .arg(ipText));
                m_probeButton->setText(QStringLiteral("Retry probe"));
                disc->deleteLater();
            });

    disc->probeAddress(addr, port, std::chrono::milliseconds(1500));
}

void AddCustomRadioDialog::onSaveOfflineClicked()
{
    // Design §6.4: protocol must be set explicitly when there's no probe to
    // learn from. If the user left it on "Auto-detect" (-1), inform them.
    const int protoIdx = m_protocolCombo->currentIndex();
    if (m_protocolCombo->itemData(protoIdx).toInt() < 0) {
        showInlineInfo(
            QStringLiteral(
                "Saving without probing — please set Protocol explicitly.\n"
                "Pick Protocol 1 (HL2 / classic ANAN) or Protocol 2 (Saturn / ANAN-G2)."));
        m_protocolCombo->setStyleSheet(
            kFieldStyle + QStringLiteral("QComboBox { border: 1px solid #5985b8; }"));
        return;
    }

    // Validate required fields before accepting
    const bool nameOk = !m_nameEdit->text().trimmed().isEmpty();
    QHostAddress addr;
    const bool ipOk = addr.setAddress(m_ipEdit->text().trimmed());
    if (!nameOk || !ipOk) {
        validateFields();
        return;
    }

    m_savedOffline = true;
    QDialog::accept();
}

void AddCustomRadioDialog::validateFields()
{
    // Enable action buttons only when minimal required fields are valid.
    const bool nameOk = !m_nameEdit->text().trimmed().isEmpty();
    QHostAddress addr;
    const bool ipOk = addr.setAddress(m_ipEdit->text().trimmed());
    const bool portOk = (m_portSpin->value() >= 1 && m_portSpin->value() <= 65535);

    // MAC: optional — if non-empty must match AA:BB:CC:DD:EE:FF
    bool macOk = true;
    const QString mac = m_macEdit->text().trimmed();
    if (!mac.isEmpty()) {
        static const QRegularExpression kMacRe(
            QStringLiteral(R"(^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$)"));
        macOk = kMacRe.match(mac).hasMatch();
    }

    const bool fieldsOk = nameOk && ipOk && portOk && macOk;

    if (m_probeButton) {
        m_probeButton->setEnabled(fieldsOk);
    }
    if (m_saveOfflineButton) {
        m_saveOfflineButton->setEnabled(fieldsOk);
    }

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

// ---------------------------------------------------------------------------
// Inline feedback helpers
// ---------------------------------------------------------------------------

void AddCustomRadioDialog::showInlineError(const QString& message)
{
    // Error band: dark red border + lighter red background, white text.
    // Colour: #c14848 error red (design §6.3).
    m_feedbackFrame->setStyleSheet(QStringLiteral(
        "QFrame { background: #2a1010; border: 1px solid #c14848; border-radius: 4px; }"));
    m_feedbackLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #e07070; font-size: 12px; }"));
    m_feedbackLabel->setText(message);
    m_feedbackFrame->show();
}

void AddCustomRadioDialog::showInlineInfo(const QString& message)
{
    // Info band: dark blue border + blue text.
    // Colour: #5985b8 info blue (design §6.4).
    m_feedbackFrame->setStyleSheet(QStringLiteral(
        "QFrame { background: #101828; border: 1px solid #5985b8; border-radius: 4px; }"));
    m_feedbackLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #8aabcf; font-size: 12px; }"));
    m_feedbackLabel->setText(message);
    m_feedbackFrame->show();
}

void AddCustomRadioDialog::clearInlineBand()
{
    m_feedbackFrame->hide();
    m_feedbackLabel->clear();
}

void AddCustomRadioDialog::showProbingOverlay()
{
    // Disable action buttons and show a status message while probe is in flight.
    m_probeButton->setEnabled(false);
    m_saveOfflineButton->setEnabled(false);
    showInlineInfo(QStringLiteral("Probing %1:%2 …")
        .arg(m_ipEdit->text().trimmed())
        .arg(m_portSpin->value()));
}

void AddCustomRadioDialog::hideProbingOverlay()
{
    // Re-enable buttons (validateFields will re-check); clear the spinner text.
    validateFields();
    clearInlineBand();
}

// ---------------------------------------------------------------------------
// Result accessors
// ---------------------------------------------------------------------------

RadioInfo AddCustomRadioDialog::result() const
{
    // If probe succeeded, return the probed info (already populated).
    // If saved offline, synthesise from the form fields.
    if (!m_savedOffline && m_probedInfo.macAddress.length() > 0) {
        return m_probedInfo;
    }

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

    // Model override — user-selected SKU (or FIRST for auto-detect)
    const int userModelInt = m_modelCombo->currentData().toInt();
    info.modelOverride = static_cast<HPSDRModel>(userModelInt);

    // Derive board type and capabilities from model (best-effort for offline saves)
    const HPSDRHW hw = boardForModel(info.modelOverride == HPSDRModel::FIRST
                                         ? HPSDRModel::HERMESLITE   // safe default for offline
                                         : info.modelOverride);
    info.boardType = hw;
    const BoardCapabilities& caps = BoardCapsTable::forBoard(hw);
    info.adcCount            = caps.adcCount;
    info.maxReceivers        = caps.maxReceivers;
    info.maxSampleRate       = caps.maxSampleRate;
    info.hasDiversityReceiver = caps.hasDiversityReceiver;
    info.hasPureSignal        = caps.hasPureSignal;

    // Protocol — from Thetis comboProtocol (Designer.cs:79-84)
    const int protoInt = m_protocolCombo->currentData().toInt();
    if (protoInt >= 0) {
        info.protocol = static_cast<ProtocolVersion>(protoInt);
    } else {
        // Auto-detect sentinel: default to P1 for offline saves
        info.protocol = ProtocolVersion::Protocol1;
    }

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
