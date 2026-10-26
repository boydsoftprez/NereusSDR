// =================================================================
// src/gui/setup/hardware/OcOutputsTab.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.designer.cs:tpOCHFControl,
//   tpOCSWLControl (tcOCOutputs container tab pages)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Initial placeholder stub. J.J. Boyd (KG4VCF).
//   2026-04-20 — Refactored into parent QTabWidget hosting two sub-sub-tabs:
//                HF (OcOutputsHfTab — full RX/TX matrix + actions + USB BCD
//                + Ext PA + live OC state) and SWL (placeholder for a
//                follow-up commit). Phase 3P-D Task 2. J.J. Boyd (KG4VCF).
//   2026-04-30 — SWL placeholder replaced with real OcOutputsSwlTab (13
//                SWL bands × 7 pins matrix + reset).  Phase 3L HL2 Filter
//                visibility brainstorm.  J.J. Boyd (KG4VCF), with
//                AI-assisted transformation via Anthropic Claude Code.
//   2026-05-02 — P1 full-parity §4.3: User Dig Out group with 4 bit-toggle
//                checkboxes wired to TransmitModel::userDigOut, gated on
//                BoardCapabilities::hasPennyLane. J.J. Boyd (KG4VCF), with
//                AI-assisted transformation via Anthropic Claude Code.
//   2026-09-23 - R-R3-46: User Dig Out and the HF / SWL transmit
//                 fields follow the transmit permission. J.J. Boyd (KG4VCF), AI-
//                 assisted via Anthropic Claude Code.
//   2026-09-23 - R-R3-21: populate() routes the MAC into
//                 PennyLaneController so Penny Ext Control saves per radio.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49: the VHF sub-tab is hidden until transverters are
//                 built (UnbuiltFeatures).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-46 / R-R3-49 (remote-window parity Task 13): User
//                 Dig Out follows the transmit settings gate; the TX pins,
//                 pin actions and resets follow their own gates.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - setN2adrHintVisible forwards to the HF tab. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - D25: the HF and SWL sub-tabs scroll down inside their own
//                areas (SetupFit::scrollingTab) and the sub-tab pane fills
//                the tab, so the sub-tab strip stays at the top; the VHF
//                note wraps. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-10-26 - Anvelina DX Outputs group (DX OUT 7..10, names, All on /
//                All off) under User Dig Out, on an ANVELINAPRO3 only; held
//                disabled with the reason while the radio is stopped
//                (J.J. Boyd KG4VCF, AI-assisted: Claude Code).
// =================================================================

//=================================================================
// setup.designer.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves his       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "OcOutputsTab.h"
#include "HardwareTransmitGate.h"
#include "OcOutputsHfTab.h"
#include "OcOutputsSwlTab.h"
#include "gui/UnbuiltFeatures.h"
#include "gui/setup/SetupFit.h"

#include "core/AnvelinaDxOutputs.h"
#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "core/OcMatrix.h"
#include "core/accessories/PennyLaneController.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QVBoxLayout>

namespace NereusSDR {

OcOutputsTab::OcOutputsTab(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_ocMatrix(model ? &model->ocMatrixMutable() : nullptr)
{
    // OcMatrix is now owned by RadioModel so the codec layer (P1/P2
    // buildCodecContext) and the UI share one instance. Phase 3P-D Task 3.
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── User Dig Out group (P1 full-parity §4.3) ────────────────────────────
    // Lives above the QTabWidget so it's visible regardless of which
    // sub-sub-tab (HF / VHF / SWL) is active. user_dig_out is a single 4-bit
    // global TX-side property (not per-band), so it doesn't belong in the
    // per-band OC matrix grid.
    //
    // TODO(future): Thetis tpPennyCtrl (setup.cs:6364 [v2.10.3.13]) is the
    // OC Control / Hermes Ctrl tab that hosts the OC matrix; it does NOT
    // expose per-bit User Dig Out checkboxes upstream — those bits ship
    // out via networkproto1.c bank 11 C3 but with no UI surface in Thetis.
    // This minimal 4-checkbox UI closes the NereusSDR audit gap (hasPennyLane
    // populated on every HPSDR board but previously had zero consumers); a
    // richer per-pin "OC pin function" combo-box matrix is left as a future
    // task if/when bench testing identifies a real user need.
    buildUserDigOutGroup();
    layout->addWidget(m_userDigOutGroup);

    // ── Anvelina DX Outputs (DX OUT 7..10) ──────────────────────────────────
    // Directly under User Dig Out, above the HF / SWL sub-tabs: like User
    // Dig Out these are global outputs, not per band (Spec:
    // Open_Collector_Anvelina_DX for Thetis (EU2AV, 2026-05-21), page 2).
    buildDxGroup();
    layout->addWidget(m_dxSection);

    m_subTabs = new QTabWidget(this);

    // ── HF sub-sub-tab (Task 2 full implementation) ───────────────────────
    // Source: Thetis tpOCHFControl (setup.designer.cs) [@501e3f5]
    m_hfTab = new OcOutputsHfTab(model, m_ocMatrix, this);
    m_subTabs->addTab(SetupFit::scrollingTab(m_hfTab), tr("HF"));

    // ── VHF sub-sub-tab (placeholder — Phase 3L follow-up) ────────────────
    // mi0bot mirrors HF onto a VHF tab for transverter / 2m+ band plans
    // (setup.designer.cs:tpOCVHFControl, grpExtCtrlVHF, btnCtrlVHFReset
    // [v2.10.3.13-beta2]).  Stubbed here for tab-count parity with mi0bot —
    // wiring a real VHF matrix needs the XVTR plan (Phase 3F+ XVTR) to
    // settle first since "VHF band" is not first-class in OpenHPSDR P1
    // without a transverter mapping layer.
    m_vhfTab = new QWidget(this);
    {
        auto* vhfLayout = new QVBoxLayout(m_vhfTab);
        auto* placeholder = new QLabel(
            tr("Open collector outputs for transverter bands are not available in this version."), m_vhfTab);
        placeholder->setAlignment(Qt::AlignCenter);
        // D25: the note wraps; a hidden tab's page still sets the pane's
        // minimum width.
        placeholder->setWordWrap(true);
        placeholder->setStyleSheet(QStringLiteral(
            "color: rgba(255,255,255,0.5); font-style: italic;"));
        vhfLayout->addStretch();
        vhfLayout->addWidget(placeholder);
        vhfLayout->addStretch();
    }
    const int vhfIndex = m_subTabs->addTab(m_vhfTab, tr("VHF"));
    // R-R3-49: hidden until transverters are built.
    m_subTabs->setTabVisible(vhfIndex, UnbuiltFeatures::isBuilt(UnbuiltFeature::Transverters));

    // ── SWL sub-sub-tab (Phase 3L HL2 Filter visibility) ──────────────────
    // Source: Thetis tpOCSWLControl (setup.designer.cs) [@501e3f5]
    // mi0bot setup.designer.cs:tpOCSWLControl, grpExtCtrlSWL [v2.10.3.13-beta2]
    m_swlTab = new OcOutputsSwlTab(model, m_ocMatrix, this);
    m_subTabs->addTab(SetupFit::scrollingTab(m_swlTab), tr("SWL"));

    layout->addWidget(m_subTabs, 1);
}

// ── populate ─────────────────────────────────────────────────────────────────

void OcOutputsTab::populate(const RadioInfo& info, const BoardCapabilities& caps)
{
    // Route the MAC address into OcMatrix so per-MAC AppSettings keys are
    // scoped correctly, then reload.
    if (m_ocMatrix) {
        m_ocMatrix->setMacAddress(info.macAddress);
        m_ocMatrix->load();  // fires OcMatrix::changed() → HF tab re-syncs
    }

    // R-R3-21: Penny Ext Control saves per radio. Route the MAC here too so
    // a window that did not connect the radio itself (a remote window, whose
    // model never runs the local connect path) still reads and saves the
    // radio's own key; the HF tab's checkbox follows extCtrlEnabledChanged.
    if (m_model != nullptr) {
        PennyLaneController& penny = m_model->pennyLaneControllerMutable();
        penny.setMacAddress(info.macAddress);
        penny.load();
    }

    // P1 full-parity §4.3: User Dig Out group visibility gates on hasPennyLane.
    // True on every HPSDR-class board (Atlas/Hermes/HermesII/Angelia/Orion +
    // ANAN family); false on Hermes Lite 2 + RX-only kits where there's no
    // Penny companion board to receive the user_dig_out bits.
    //
    // BoardCapabilities flag origin: setup.cs:6364 (Thetis tpPennyCtrl
    // tab insertion under AddHPSDRPages) — see BoardCapabilities.cpp:392.
    if (m_userDigOutGroup != nullptr) {
        m_userDigOutGroup->setVisible(caps.hasPennyLane);
    }

    // Initial sync: mirror current TransmitModel::userDigOut into checkboxes.
    // TransmitModel persistence (TransmitModel.cpp:510 persistOne →
    // hardware/<mac>/tx/UserDigOut) already restored the value from
    // AppSettings during RadioModel's connect path.
    if (m_model != nullptr) {
        onModelUserDigOutChanged(m_model->transmitModel().userDigOut());
    }

    // Anvelina DX Outputs: the radio's own state and names, per MAC (as the
    // OC matrix above, so a window that did not connect the radio itself
    // reads the radio's keys too). load() fires changed() -> syncDxFromModel.
    if (m_model != nullptr) {
        AnvelinaDxOutputs& dx = m_model->anvelinaDxOutputsMutable();
        dx.setMacAddress(info.macAddress);
        dx.load();
    }
    applyDxVisibility();
    syncDxFromModel();
    applyDxRunning();
}

// ── restoreSettings ───────────────────────────────────────────────────────────

void OcOutputsTab::restoreSettings(const QMap<QString, QVariant>& /*settings*/)
{
    // OcMatrix owns all OC state under hardware/<mac>/oc/... via AppSettings.
    // populate() already called load(), so nothing to do here.
    // This stub satisfies the HardwarePage API contract.
}

// ── Transmit permission (R-R3-46) ────────────────────────────────────────────

void OcOutputsTab::setTransmitPermitted(bool permitted, const QString& reason)
{
    // R-R3-46 / R-R3-49 (parity Task 13): only the HF tab's hidden extras
    // (hot switching, external PA) still wait for remote transmit.
    if (m_hfTab) {
        m_hfTab->setTransmitPermitted(permitted, reason);
    }
}

void OcOutputsTab::setUserDigOutPermitted(bool permitted, const QString& reason)
{
    HardwareTransmitGate::apply(m_userDigOutGroup, permitted, reason);
}

void OcOutputsTab::setTransmitPinsPermitted(bool permitted, const QString& reason)
{
    if (m_hfTab) {
        m_hfTab->setTransmitPinsPermitted(permitted, reason);
    }
    if (m_swlTab) {
        m_swlTab->setTransmitPinsPermitted(permitted, reason);
    }
}

void OcOutputsTab::setPinActionsPermitted(bool permitted, const QString& reason)
{
    if (m_hfTab) {
        m_hfTab->setPinActionsPermitted(permitted, reason);
    }
}

void OcOutputsTab::setN2adrHintVisible(bool visible)
{
    if (m_hfTab) {
        m_hfTab->setN2adrHintVisible(visible);
    }
}

// ── User Dig Out group (P1 full-parity §4.3) ────────────────────────────────

void OcOutputsTab::buildUserDigOutGroup()
{
    m_userDigOutGroup = new QGroupBox(tr("User Dig Out"), this);
    m_userDigOutGroup->setToolTip(tr(
        "User-controllable digital output pins routed via the Penny / "
        "Penny-Lane companion board (OpenHPSDR Protocol 1 bank 11 C3 low "
        "4 bits). Pin 1 = bit 0, ... Pin 4 = bit 3."));

    auto* row = new QHBoxLayout(m_userDigOutGroup);
    row->setContentsMargins(8, 4, 8, 4);
    row->setSpacing(12);

    // bit 0 → Pin 1, bit 1 → Pin 2, bit 2 → Pin 3, bit 3 → Pin 4.
    // Match the wire-layout convention: networkproto1.c:601 packs
    //   C3 = prn->user_dig_out & 0b00001111;
    // so bit 0 is the lowest pin index.
    for (int bit = 0; bit < kUserDigOutBits; ++bit) {
        auto* cb = new QCheckBox(tr("Pin %1").arg(bit + 1), m_userDigOutGroup);
        cb->setToolTip(tr("Toggle bit %1 of User Dig Out (Pin %2 on the "
                           "Penny / Penny-Lane companion board).")
                           .arg(bit).arg(bit + 1));
        m_userDigOutChecks[static_cast<std::size_t>(bit)] = cb;
        row->addWidget(cb);

        // Each checkbox toggle recomputes the full nibble and pushes it
        // into TransmitModel.  The model setter is idempotent + masks to
        // 4 bits, so off-by-one nibble computations would round-trip
        // through the mask without crashing.
        connect(cb, &QCheckBox::toggled, this, [this](bool /*on*/) {
            onCheckboxToggled();
        });
    }
    row->addStretch();

    // Listen for external TransmitModel writes (e.g. CAT control, settings
    // restore) and mirror them into the checkboxes.  m_syncingFromModel
    // guards against the resulting toggled() callbacks writing back.
    if (m_model != nullptr) {
        connect(&m_model->transmitModel(), &TransmitModel::userDigOutChanged,
                this, &OcOutputsTab::onModelUserDigOutChanged);
    }
}

void OcOutputsTab::onCheckboxToggled()
{
    if (m_syncingFromModel) {
        return;  // echo-loop guard — reflecting model state, don't write back
    }
    if (m_model == nullptr) {
        return;
    }

    // Recompute the 4-bit nibble: bit i = checkbox i checked.
    int nibble = 0;
    for (int bit = 0; bit < kUserDigOutBits; ++bit) {
        if (m_userDigOutChecks[static_cast<std::size_t>(bit)] != nullptr
            && m_userDigOutChecks[static_cast<std::size_t>(bit)]->isChecked()) {
            nibble |= (1 << bit);
        }
    }
    // TransmitModel::setUserDigOut masks to 0x0F + idempotent guard +
    // persists via persistOne("UserDigOut", ...) at hardware/<mac>/tx/UserDigOut.
    m_model->transmitModel().setUserDigOut(nibble);
}

void OcOutputsTab::onModelUserDigOutChanged(int nibble)
{
    // Mirror the model nibble into the 4 checkboxes.  Set the guard so
    // each setChecked() doesn't fire onCheckboxToggled() → setUserDigOut()
    // and recurse.  QSignalBlocker is finer-grained but the flag is
    // simpler and matches the m_updatingFromModel pattern documented in
    // CLAUDE.md (GUI↔Model Sync section).
    m_syncingFromModel = true;
    for (int bit = 0; bit < kUserDigOutBits; ++bit) {
        auto* cb = m_userDigOutChecks[static_cast<std::size_t>(bit)];
        if (cb == nullptr) {
            continue;
        }
        const bool wantChecked = (nibble & (1 << bit)) != 0;
        if (cb->isChecked() != wantChecked) {
            cb->setChecked(wantChecked);
        }
    }
    m_syncingFromModel = false;
}

// ── Anvelina DX Outputs (DX OUT 7..10) ──────────────────────────────────────

namespace {

int dxOutputAt(int column)
{
    return AnvelinaDxOutputs::kFirstOutput + column;
}

} // namespace

void OcOutputsTab::buildDxGroup()
{
    static_assert(kDxOutputs == AnvelinaDxOutputs::kOutputCount,
                  "one column per Anvelina DX output");

    m_dxSection = new QWidget(this);
    auto* section = new QVBoxLayout(m_dxSection);
    section->setContentsMargins(0, 0, 0, 0);
    section->setSpacing(0);

    m_dxGroup = new QGroupBox(tr("Anvelina DX Outputs"), m_dxSection);
    auto* row = new QHBoxLayout(m_dxGroup);
    row->setContentsMargins(8, 4, 8, 4);
    row->setSpacing(12);

    // One column per output: the box, then its name under it. A name field
    // is 124 px where the row has room and narrows (not the page widens)
    // when the Settings window is at its own size or smaller.
    constexpr int kNameFieldWidth = 124;
    for (int column = 0; column < kDxOutputs; ++column) {
        const int out = dxOutputAt(column);
        auto* col = new QVBoxLayout;
        col->setSpacing(4);

        auto* cb = new QCheckBox(tr("DX OUT %1").arg(out), m_dxGroup);
        m_dxChecks[static_cast<std::size_t>(column)] = cb;
        col->addWidget(cb);

        auto* name = new QLineEdit(m_dxGroup);
        name->setPlaceholderText(tr("Name"));
        name->setToolTip(tr("Your name for this output, shown here only"));
        name->setAccessibleName(tr("Name for DX OUT %1").arg(out));
        name->setMaximumWidth(kNameFieldWidth);
        m_dxNames[static_cast<std::size_t>(column)] = name;
        col->addWidget(name);

        row->addLayout(col);

        if (m_model != nullptr) {
            connect(cb, &QCheckBox::toggled, this, [this, out](bool on) {
                m_model->anvelinaDxOutputsMutable().setOutputOn(out, on);
            });
            connect(name, &QLineEdit::textEdited, this, [this, out](const QString& text) {
                m_model->anvelinaDxOutputsMutable().setLabel(out, text);
            });
        }
    }
    row->addStretch();

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(6);
    m_dxAllOn = new QPushButton(tr("All on"), m_dxGroup);
    m_dxAllOff = new QPushButton(tr("All off"), m_dxGroup);
    buttons->addWidget(m_dxAllOn);
    buttons->addWidget(m_dxAllOff);
    row->addLayout(buttons);
    row->setAlignment(buttons, Qt::AlignBottom);

    if (m_model != nullptr) {
        connect(m_dxAllOn, &QPushButton::clicked, this, [this]() {
            m_model->anvelinaDxOutputsMutable().setAll(true);
        });
        connect(m_dxAllOff, &QPushButton::clicked, this, [this]() {
            m_model->anvelinaDxOutputsMutable().setAll(false);
        });
    }

    section->addWidget(m_dxGroup);

    // The gateware drives the outputs only while the radio runs
    // (Gateware n1gp-Anvelina_PROIII Orion.v:920-923 [@8e86a61]); stopped,
    // the choices stay saved and the line says why the boxes are grey.
    m_dxReason = new QLabel(tr("The radio holds these outputs off while it is stopped. "
                               "Your choices are kept and come back when it runs."),
                            m_dxSection);
    m_dxReason->setWordWrap(true);
    m_dxReason->setContentsMargins(2, 4, 0, 0);
    m_dxReason->setStyleSheet(QStringLiteral("font-style: italic;"));
    section->addWidget(m_dxReason);

    if (m_model != nullptr) {
        connect(&m_model->anvelinaDxOutputsMutable(), &AnvelinaDxOutputs::changed,
                this, &OcOutputsTab::syncDxFromModel);
        // The same connected-and-streaming state the HF tab's live OC pin
        // row follows (OcOutputsHfTab, connectionStateChanged).
        connect(m_model, &RadioModel::connectionStateChanged,
                this, &OcOutputsTab::applyDxRunning);
    }

    applyDxVisibility();
    syncDxFromModel();
    applyDxRunning();
}

void OcOutputsTab::syncDxFromModel()
{
    if (m_model == nullptr) {
        return;
    }
    const AnvelinaDxOutputs& dx = m_model->anvelinaDxOutputs();
    for (int column = 0; column < kDxOutputs; ++column) {
        const int out = dxOutputAt(column);
        if (QCheckBox* cb = m_dxChecks[static_cast<std::size_t>(column)]) {
            const QSignalBlocker block(cb);
            cb->setChecked(dx.outputOn(out));
        }
        if (QLineEdit* name = m_dxNames[static_cast<std::size_t>(column)]) {
            // Only on a real change, so typing keeps its cursor.
            const QString label = dx.label(out);
            if (name->text() != label) {
                const QSignalBlocker block(name);
                name->setText(label);
            }
        }
    }
}

void OcOutputsTab::applyDxRunning()
{
    const bool running = m_model != nullptr && m_model->isConnected();
    const QString stopped = tr("The radio holds the DX outputs off while it is stopped.");
    for (int column = 0; column < kDxOutputs; ++column) {
        if (QCheckBox* cb = m_dxChecks[static_cast<std::size_t>(column)]) {
            cb->setEnabled(running);
            cb->setToolTip(running
                               ? tr("Switches USEROUT%1 on the Anvelina's DX connector.")
                                     .arg(dxOutputAt(column))
                               : stopped);
        }
    }
    for (QPushButton* button : {m_dxAllOn, m_dxAllOff}) {
        if (button != nullptr) {
            button->setEnabled(running);
            button->setToolTip(running ? QString() : stopped);
        }
    }
    if (m_dxReason != nullptr) {
        m_dxReason->setVisible(!running);
    }
}

void OcOutputsTab::applyDxVisibility()
{
    // Hardware the radio lacks is hidden: only the Anvelina PRO III has the
    // DX connector. Its board reads OrionMKII, so the model decides.
    const bool anvelina = m_model != nullptr
        && m_model->hardwareProfile().model == HPSDRModel::ANVELINAPRO3;
    if (m_dxSection != nullptr) {
        m_dxSection->setVisible(anvelina);
    }
}

// ── Test seams (P1 full-parity §4.3) ─────────────────────────────────────────

bool OcOutputsTab::userDigOutGroupVisibleForTest() const
{
    // Match Task 4.2's CwSetupPage::sidetoneRowVisibleForTest() pattern:
    // use !isHidden() rather than isVisibleTo() so tests don't need to
    // show() the parent dialog.
    return m_userDigOutGroup != nullptr && !m_userDigOutGroup->isHidden();
}

bool OcOutputsTab::userDigOutCheckboxCheckedForTest(int bit) const
{
    if (bit < 0 || bit >= kUserDigOutBits) {
        return false;
    }
    auto* cb = m_userDigOutChecks[static_cast<std::size_t>(bit)];
    return cb != nullptr && cb->isChecked();
}

void OcOutputsTab::toggleUserDigOutCheckboxForTest(int bit, bool checked)
{
    if (bit < 0 || bit >= kUserDigOutBits) {
        return;
    }
    auto* cb = m_userDigOutChecks[static_cast<std::size_t>(bit)];
    if (cb == nullptr) {
        return;
    }
    // setChecked() fires the same toggled() signal path as a user click,
    // so this exercises the full wire-up to TransmitModel::setUserDigOut.
    cb->setChecked(checked);
}

// ── Test seams (Anvelina DX Outputs) ────────────────────────────────────────

bool OcOutputsTab::dxGroupVisibleForTest() const
{
    return m_dxSection != nullptr && !m_dxSection->isHidden();
}

QCheckBox* OcOutputsTab::dxCheckboxForTest(int out) const
{
    const int column = out - AnvelinaDxOutputs::kFirstOutput;
    if (column < 0 || column >= kDxOutputs) {
        return nullptr;
    }
    return m_dxChecks[static_cast<std::size_t>(column)];
}

QLineEdit* OcOutputsTab::dxNameFieldForTest(int out) const
{
    const int column = out - AnvelinaDxOutputs::kFirstOutput;
    if (column < 0 || column >= kDxOutputs) {
        return nullptr;
    }
    return m_dxNames[static_cast<std::size_t>(column)];
}

bool OcOutputsTab::dxReasonVisibleForTest() const
{
    return m_dxReason != nullptr && !m_dxReason->isHidden();
}

} // namespace NereusSDR
