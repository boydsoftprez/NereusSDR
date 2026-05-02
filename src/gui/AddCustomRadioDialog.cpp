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
#include "StyleConstants.h"
#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "core/LogCategories.h"

#include <QDateTime>
#include <QTimer>
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
// Per docs/architecture/ui-audit-polish-plan.md §D — Task 22.
// All palette constants verified against StyleConstants.h:
//   Style::kTextPrimary (#c8d8e8), Style::kOverlayBorder (#304050),
//   Style::kAccent (#00b4d8), Style::kAppBg (#0f0f1a),
//   Style::kTextSecondary (#8090a0).
//
// §D exception values (no canonical match — dialog-specific):
//   #12202e  — field input background; deeper inset than kButtonBg (#1a2a3a),
//               lighter than kPanelBg (#0a0a18). Kept as named constant.
//   #0096b7  — primary button hover; darker accent variant not in palette.
//   #303850  — primary button disabled bg; bluish-gray not in palette.
//   #606878  — primary button disabled fg; not in palette.
//   #405060  — secondary button border and hover; not in palette.

// Field input background — deeper inset than kButtonBg; no canonical match.
// §D documented exception.
static constexpr const char* kFieldInputBg = "#12202e";

static const QString kFieldStyle =
    QStringLiteral(
        "QLineEdit, QSpinBox, QComboBox {"
        "  background: %1; color: %2;"
        "  border: 1px solid %3; border-radius: 3px; padding: 4px 6px;"
        "}"
        "QLineEdit:focus, QSpinBox:focus, QComboBox:focus {"
        "  border-color: %4;"
        "}")
    .arg(kFieldInputBg, Style::kTextPrimary, Style::kOverlayBorder, Style::kAccent);

// Primary (accent-filled) button — kAccent bg, white fg, darker hover.
// Hover #0096b7 and disabled colors have no canonical match; §D exception.
static const QString kPrimaryButtonStyle =
    QStringLiteral(
        "QPushButton {"
        "  background: %1; color: #fff;"
        "  border: none; border-radius: 4px; padding: 6px 16px; font-weight: bold;"
        "}"
        "QPushButton:hover { background: #0096b7; }"        // §D exception: accent-dark hover
        "QPushButton:disabled { background: #303850; color: #606878; }")  // §D exception: disabled
    .arg(Style::kAccent);

// Secondary (outline) button — kOverlayBorder bg, kTextPrimary fg.
// Border/hover #405060 has no canonical match; §D exception.
static const QString kSecondaryButtonStyle =
    QStringLiteral(
        "QPushButton {"
        "  background: %1; color: %2;"
        "  border: 1px solid #405060; border-radius: 4px; padding: 6px 16px;"  // §D: #405060 exception
        "}"
        "QPushButton:hover { background: #405060; }")       // §D exception: secondary hover
    .arg(Style::kOverlayBorder, Style::kTextPrimary);

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

void AddCustomRadioDialog::setEditTarget(const RadioInfo& info,
                                         bool pinToMac,
                                         bool autoConnect)
{
    setWindowTitle(QStringLiteral("Edit Radio — %1").arg(info.name.isEmpty()
        ? info.address.toString() : info.name));

    // Pre-populate form fields from the saved entry.
    m_nameEdit->setText(info.name);
    m_ipEdit->setText(info.address.toString());
    m_portSpin->setValue(info.port);
    // Synthetic MANUAL:IP:PORT keys: don't expose to user — leave MAC blank
    // so re-save through the synthetic path is consistent. Real MACs round-trip.
    if (!info.macAddress.startsWith(QStringLiteral("MANUAL:"))) {
        m_macEdit->setText(info.macAddress);
    }

    // Select the matching SKU model in the combo (best-effort).
    if (info.modelOverride != HPSDRModel::FIRST) {
        for (int i = 0; i < m_modelCombo->count(); ++i) {
            const HPSDRModel m = static_cast<HPSDRModel>(m_modelCombo->itemData(i).toInt());
            if (m == info.modelOverride) {
                m_modelCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    // Select the protocol in the combo.
    for (int i = 0; i < m_protocolCombo->count(); ++i) {
        const int v = m_protocolCombo->itemData(i).toInt();
        if (v == static_cast<int>(info.protocol)) {
            m_protocolCombo->setCurrentIndex(i);
            break;
        }
    }

    m_pinToMacCheck->setChecked(pinToMac);
    m_autoConnectCheck->setChecked(autoConnect);

    // Seed m_probedInfo with the existing entry so result() reuses the
    // same MAC on save — otherwise an edit of a probe-verified entry
    // would synthesise a new MANUAL: key and split into a duplicate row.
    // m_probedInfoFromProbe stays false: this is *not* a probe-verified
    // payload, so result() must build from the live form fields rather
    // than echoing this stale snapshot back. (Codex P1 review against
    // PR #158 — without this gate, IP/port/protocol/model edits were
    // silently discarded; only the name was overlaid on save.)
    if (!info.macAddress.startsWith(QStringLiteral("MANUAL:"))) {
        m_probedInfo = info;
        m_probedInfoFromProbe = false;
    }

    validateFields();  // re-evaluate Save button enablement
}

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
    infoLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 12px; padding: 4px; }")
        .arg(Style::kTextSecondary));
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

    // Checkbox style uses kTextPrimary + kOverlayBorder + kAccent.
    // Indicator bg uses kFieldInputBg (dialog-specific inset; §D exception).
    // kCheckBoxStyle adds font-size:12px + border-radius:2px which this omits
    // by design (dialog fields use tighter sizing). §D documented exception.
    static const QString kCheckStyle =
        QStringLiteral(
            "QCheckBox { color: %1; }"
            "QCheckBox::indicator { border: 1px solid %2; background: %3; width: 14px; height: 14px; }"
            "QCheckBox::indicator:checked { background: %4; }")
        .arg(Style::kTextPrimary, Style::kOverlayBorder, kFieldInputBg, Style::kAccent);

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
    // Two-step flow per user feedback: Probe verifies (stays open), Save commits.
    // Probe is optional — user can Save without probing if they accept Protocol
    // explicitly. Save uses probed RadioInfo when available, else synthesises
    // from the form fields.
    m_probeButton = new QPushButton(QStringLiteral("Probe"), this);
    m_probeButton->setObjectName(QStringLiteral("probeButton"));
    m_probeButton->setStyleSheet(kSecondaryButtonStyle);
    m_probeButton->setToolTip(QStringLiteral(
        "Verify the radio responds at the given IP. Optional — leaves the "
        "dialog open so you can review and edit before saving."));
    connect(m_probeButton, &QPushButton::clicked,
            this, &AddCustomRadioDialog::onProbeClicked);

    m_saveOfflineButton = new QPushButton(QStringLiteral("Save"), this);
    m_saveOfflineButton->setObjectName(QStringLiteral("saveOfflineButton"));
    m_saveOfflineButton->setStyleSheet(kPrimaryButtonStyle);
    m_saveOfflineButton->setDefault(true);
    m_saveOfflineButton->setToolTip(QStringLiteral(
        "Save this radio to the saved-radio list. Uses verified data if a "
        "probe succeeded, else uses the form fields."));
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

    // Dark theme — §D: #a0b0c0 label fg has no canonical match (off-palette
    // label warm-blue used in this dialog only; §D documented exception).
    setStyleSheet(
        QStringLiteral(
            "QDialog { background: %1; }"
            "QGroupBox {"
            "  color: %2;"
            "  border: 1px solid %3;"
            "  border-radius: 4px;"
            "  margin-top: 8px;"
            "  padding-top: 12px;"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin;"
            "  left: 10px;"
            "  padding: 0 3px;"
            "}"
            "QLabel { color: #a0b0c0; }")          // §D exception: off-palette label warm-blue
        .arg(Style::kAppBg, Style::kTextSecondary, Style::kBorderSubtle));

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
                m_probedInfo = info;
                m_probedInfoFromProbe = true;   // real probe — result() trusts this payload

                // User-picked model overrides the probe-detected silicon family default.
                // Only override when the user chose something other than Auto-detect.
                const int userModelInt = m_modelCombo->currentData().toInt();
                const auto userModel = static_cast<HPSDRModel>(userModelInt);
                if (userModel != HPSDRModel::FIRST) {
                    m_probedInfo.modelOverride = userModel;
                }

                // Reflect probe-discovered fields back into the form so the
                // user can review them before saving. Don't overwrite
                // user-edited Name / Port; do fill MAC and select the silicon
                // family default if the user left Auto-detect.
                if (m_macEdit->text().trimmed().isEmpty()) {
                    m_macEdit->setText(m_probedInfo.macAddress);
                }
                if (userModel == HPSDRModel::FIRST
                    && m_probedInfo.boardType != HPSDRHW::Unknown) {
                    // Pick the SKU that matches the probe-reported boardType.
                    // Falls back gracefully if no exact match is in the combo.
                    for (int i = 0; i < m_modelCombo->count(); ++i) {
                        const HPSDRModel m = static_cast<HPSDRModel>(
                            m_modelCombo->itemData(i).toInt());
                        if (m == HPSDRModel::FIRST) continue;
                        if (boardForModel(m) == m_probedInfo.boardType) {
                            m_modelCombo->setCurrentIndex(i);
                            break;
                        }
                    }
                }
                // Reflect detected protocol on the combo so Save uses it.
                for (int i = 0; i < m_protocolCombo->count(); ++i) {
                    const int v = m_protocolCombo->itemData(i).toInt();
                    if (v == static_cast<int>(m_probedInfo.protocol)) {
                        m_protocolCombo->setCurrentIndex(i);
                        break;
                    }
                }

                // Default Name to "<board> @ .<last-octet>" if user left it blank
                // (matches design §4.4 "Defaults to Model · IP suffix").
                if (m_nameEdit->text().trimmed().isEmpty()) {
                    const QString boardName = QString::fromUtf8(
                        BoardCapsTable::forBoard(m_probedInfo.boardType).displayName);
                    const QStringList octets = m_probedInfo.address.toString().split('.');
                    const QString suffix = octets.size() == 4
                        ? QStringLiteral(" @ .%1").arg(octets.last())
                        : QString();
                    m_nameEdit->setText(boardName + suffix);
                }

                disc->deleteLater();
                hideProbingOverlay();

                // Stay open; user reviews and clicks Save when ready.
                const QString name = m_probedInfo.name.isEmpty()
                    ? m_probedInfo.address.toString()
                    : m_probedInfo.name;
                showInlineInfo(QStringLiteral(
                    "✓ Verified %1 (board %2, fw %3, MAC %4) — click Save to add it.")
                    .arg(name)
                    .arg(QString::fromUtf8(BoardCapsTable::forBoard(
                        m_probedInfo.boardType).displayName))
                    .arg(m_probedInfo.firmwareVersion)
                    .arg(m_probedInfo.macAddress));
                m_probeButton->setText(QStringLiteral("Probe again"));
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
    // The button is now the unified "Save". Two paths feed it:
    //
    //   1. Probe-then-save: m_probedInfo is populated (probe succeeded).
    //      Use it directly — protocol, board, MAC, firmware all from probe.
    //
    //   2. Save without probe: synthesise from form fields. Protocol must
    //      be set explicitly (Auto-detect won't fly without a network
    //      verification to learn it from).
    const bool wasProbed = !m_probedInfo.macAddress.isEmpty();

    if (!wasProbed) {
        const int protoIdx = m_protocolCombo->currentIndex();
        if (m_protocolCombo->itemData(protoIdx).toInt() < 0) {
            showInlineInfo(
                QStringLiteral(
                    "Pick Protocol (P1 / P2) before saving — without a probe "
                    "we have no way to learn it.\n"
                    "Protocol 1 = HL2 / classic ANAN. Protocol 2 = Saturn / ANAN-G2."));
            m_protocolCombo->setStyleSheet(
                kFieldStyle + QStringLiteral("QComboBox { border: 1px solid #5985b8; }"));  // §D exception: info-blue highlight
            return;
        }
    }

    // Validate required fields before accepting
    const bool nameOk = !m_nameEdit->text().trimmed().isEmpty();
    QHostAddress addr;
    const bool ipOk = addr.setAddress(m_ipEdit->text().trimmed());
    if (!nameOk || !ipOk) {
        validateFields();
        return;
    }

    m_savedOffline = !wasProbed;
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

    // Visual feedback on MAC field.
    // #e06060 / #804040 = MAC error highlight; no canonical match (§D exception).
    if (!macOk) {
        m_macEdit->setStyleSheet(
            QStringLiteral(
                "QLineEdit { background: %1; color: #e06060;"    // §D exception: error red fg
                "  border: 1px solid #804040; border-radius: 3px; padding: 4px 6px; }")  // §D exception: error border
            .arg(kFieldInputBg));
    } else {
        m_macEdit->setStyleSheet(
            QStringLiteral(
                "QLineEdit { background: %1; color: %2;"
                "  border: 1px solid %3; border-radius: 3px; padding: 4px 6px; }"
                "QLineEdit:focus { border-color: %4; }")
            .arg(kFieldInputBg, Style::kTextPrimary, Style::kOverlayBorder, Style::kAccent));
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
    // Probe-then-save path: m_probedInfo carries a real probe payload and
    // m_probedInfoFromProbe gates this branch. The flag exists to
    // disambiguate from setEditTarget(), which also pre-populates
    // m_probedInfo (to keep the existing MAC stable across edits) but
    // does not constitute a verified probe — falling through that case
    // dropped IP/port/protocol/model edits on the floor (Codex P1
    // review against PR #158, AddCustomRadioDialog.cpp:760).
    if (!m_savedOffline && m_probedInfoFromProbe
        && m_probedInfo.macAddress.length() > 0) {
        RadioInfo verified = m_probedInfo;
        const QString userName = m_nameEdit->text().trimmed();
        if (!userName.isEmpty()) {
            verified.name = userName;
        }
        return verified;
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
