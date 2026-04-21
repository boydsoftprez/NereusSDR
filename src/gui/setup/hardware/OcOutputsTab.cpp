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

#include "core/BoardCapabilities.h"
#include "core/OcMatrix.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"

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

    m_subTabs = new QTabWidget(this);

    // ── HF sub-sub-tab (Task 2 full implementation) ───────────────────────
    // Source: Thetis tpOCHFControl (setup.designer.cs) [@501e3f5]
    m_hfTab = new OcOutputsHfTab(model, m_ocMatrix, this);
    m_subTabs->addTab(m_hfTab, tr("HF"));

    // ── SWL sub-sub-tab (placeholder — follow-up commit) ─────────────────
    // Source: Thetis tpOCSWLControl (setup.designer.cs) [@501e3f5]
    m_swlTab = new QWidget(this);
    {
        auto* swlLayout = new QVBoxLayout(m_swlTab);
        auto* placeholder = new QLabel(
            tr("SWL band plan — coming in a follow-up commit"), m_swlTab);
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setStyleSheet(QStringLiteral(
            "color: rgba(255,255,255,0.5); font-style: italic;"));
        swlLayout->addStretch();
        swlLayout->addWidget(placeholder);
        swlLayout->addStretch();
    }
    m_subTabs->addTab(m_swlTab, tr("SWL"));

    layout->addWidget(m_subTabs);
}

// ── populate ─────────────────────────────────────────────────────────────────

void OcOutputsTab::populate(const RadioInfo& info, const BoardCapabilities& /*caps*/)
{
    // Route the MAC address into OcMatrix so per-MAC AppSettings keys are
    // scoped correctly, then reload.
    if (m_ocMatrix) {
        m_ocMatrix->setMacAddress(info.macAddress);
        m_ocMatrix->load();  // fires OcMatrix::changed() → HF tab re-syncs
    }
}

// ── restoreSettings ───────────────────────────────────────────────────────────

void OcOutputsTab::restoreSettings(const QMap<QString, QVariant>& /*settings*/)
{
    // OcMatrix owns all OC state under hardware/<mac>/oc/... via AppSettings.
    // populate() already called load(), so nothing to do here.
    // This stub satisfies the HardwarePage API contract.
}

} // namespace NereusSDR
