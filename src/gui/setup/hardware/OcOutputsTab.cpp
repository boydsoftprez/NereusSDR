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
#include "OcOutputsHfTab.h"
#include "OcOutputsSwlTab.h"

#include "core/BoardCapabilities.h"
#include "core/OcMatrix.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
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

    m_subTabs = new QTabWidget(this);

    // ── HF sub-sub-tab (Task 2 full implementation) ───────────────────────
    // Source: Thetis tpOCHFControl (setup.designer.cs) [@501e3f5]
    m_hfTab = new OcOutputsHfTab(model, m_ocMatrix, this);
    m_subTabs->addTab(m_hfTab, tr("HF"));

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
            tr("VHF band plan — pending XVTR mapping (Phase 3F+)"), m_vhfTab);
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setStyleSheet(QStringLiteral(
            "color: rgba(255,255,255,0.5); font-style: italic;"));
        vhfLayout->addStretch();
        vhfLayout->addWidget(placeholder);
        vhfLayout->addStretch();
    }
    m_subTabs->addTab(m_vhfTab, tr("VHF"));

    // ── SWL sub-sub-tab (Phase 3L HL2 Filter visibility) ──────────────────
    // Source: Thetis tpOCSWLControl (setup.designer.cs) [@501e3f5]
    // mi0bot setup.designer.cs:tpOCSWLControl, grpExtCtrlSWL [v2.10.3.13-beta2]
    m_swlTab = new OcOutputsSwlTab(model, m_ocMatrix, this);
    m_subTabs->addTab(m_swlTab, tr("SWL"));

    layout->addWidget(m_subTabs);
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
}

// ── restoreSettings ───────────────────────────────────────────────────────────

void OcOutputsTab::restoreSettings(const QMap<QString, QVariant>& /*settings*/)
{
    // OcMatrix owns all OC state under hardware/<mac>/oc/... via AppSettings.
    // populate() already called load(), so nothing to do here.
    // This stub satisfies the HardwarePage API contract.
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
        cb->setToolTip(tr("Toggle bit %1 of user_dig_out (Pin %2 on the "
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

} // namespace NereusSDR
