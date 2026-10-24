#include "TxChainSection.h"

#include "gui/ComboStyle.h"
#include "gui/StyleConstants.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWidget>

// =================================================================
// src/gui/applets/TxChainSection.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs - original licence from Thetis
//   source is included below.
//
// Extracted from src/gui/applets/TxApplet.h: the LEV, EQ and CFC buttons
// and the TX Profile combo.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-20 - A copy of this row in a container is told the outer
//                 margins the applet's column gives the row (outerMargins);
//                 the applet's own row is told none.
//                 Brian Sutton, AI-assisted via OpenCode.
//   2026-10-19 - Extracted from TxApplet.{h,cpp} as part of splitting the
//                 TX applet into per-concern section objects. Behaviour,
//                 appearance, object names and tooltips are unchanged.
//                 Brian Sutton, AI-assisted via OpenAI Codex.
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
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
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines.
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

// =================================================================
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   Layout pattern from AetherSDR `src/gui/TxApplet.{h,cpp}`.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.

namespace NereusSDR {

TxChainSection::TxChainSection(QWidget* parent, int rowSpacing,
                               const QMargins& outerMargins)
    : QWidget(parent)
{
    m_outerMargins = outerMargins;
    buildUI(rowSpacing);
}

void TxChainSection::buildUI(int rowSpacing)
{
    // The column the applet puts its rows in, with the applet's own
    // contents margins. The rows below are added to it unchanged.
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(m_outerMargins);
    vbox->setSpacing(rowSpacing);

    // ── 4e. TX-processing quick toggles: [LEV] [EQ] [CFC] ──────────────────
    // Phase 3M-3a-i Batch 2 (Task F): introduced the row of 3 (LEV/EQ/PROC).
    // Phase 3M-3a-ii Batch 6 (Task A + F, then post-bench cleanup):
    // PROC was promoted in Batch 6, then dropped here in the cleanup pass —
    // PhoneCwApplet already had a wired PROC button + slider sitting un-wired
    // since 3I-3 (NyiOverlay-marked).  Two PROC controls confused users.
    // Row is now 3 buttons (LEV / EQ / CFC); PROC lives on PhoneCwApplet.
    // All three share the same VOX/MON styling family (compact 22-px-tall,
    // expanding width, green-checked LED look).
    //
    //   LEV  — checkable, bidirectional with TransmitModel::txLevelerOn.
    //   EQ   — checkable, bidirectional with TransmitModel::txEqEnabled.
    //          Left-click toggles.  Right-click → TxEqDialog (3M-3a-i Batch 3).
    //   CFC  — checkable, bidirectional with TransmitModel::cfcEnabled.
    //          Left-click toggles.  Right-click → modeless TxCfcDialog
    //          (10-band per-band CFC editor; mirrors Thetis frmCFCConfig
    //          [v2.10.3.13]).
    //
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        const QString btnStyle = Style::buttonBaseStyle()
            + Style::greenCheckedStyle();

        m_levBtn = new QPushButton(QStringLiteral("LEV"), this);
        m_levBtn->setCheckable(true);
        m_levBtn->setFixedHeight(22);
        m_levBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_levBtn->setStyleSheet(btnStyle);
        m_levBtn->setAccessibleName(QStringLiteral("TX Leveler enable"));
        m_levBtn->setObjectName(QStringLiteral("TxLevButton"));
        m_levBtn->setToolTip(QStringLiteral(
            "TX Leveler: slow speech-leveling AGC. Improves intelligibility on weak speech."));
        row->addWidget(m_levBtn, 1);

        m_eqBtn = new QPushButton(QStringLiteral("EQ"), this);
        m_eqBtn->setCheckable(true);
        m_eqBtn->setFixedHeight(22);
        m_eqBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_eqBtn->setStyleSheet(btnStyle);
        m_eqBtn->setAccessibleName(QStringLiteral("TX EQ enable"));
        m_eqBtn->setObjectName(QStringLiteral("TxEqButton"));
        m_eqBtn->setToolTip(QStringLiteral(
            "TX 10-band graphic EQ.  Left-click to toggle.\n"
            "Right-click to open the EQ dialog."));
        // Custom context-menu policy so right-click hits a slot (not the
        // default menu).
        m_eqBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        row->addWidget(m_eqBtn, 1);

        m_cfcBtn = new QPushButton(QStringLiteral("CFC"), this);
        m_cfcBtn->setCheckable(true);
        m_cfcBtn->setFixedHeight(22);
        m_cfcBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_cfcBtn->setStyleSheet(btnStyle);
        m_cfcBtn->setAccessibleName(QStringLiteral("Continuous Frequency Compressor enable"));
        m_cfcBtn->setObjectName(QStringLiteral("TxCfcButton"));
        m_cfcBtn->setToolTip(QStringLiteral(
            "CFC: 10-band continuous frequency compressor. Left-click to "
            "toggle. Right-click to open the CFC dialog."));
        // Right-click → modeless TxCfcDialog (mirrors EQ button pattern).
        m_cfcBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        row->addWidget(m_cfcBtn, 1);

        vbox->addLayout(row);
    }

    // ── Profile combo row (full width) ──────────────────────────────────────
    // Tune Mode combo removed (ATU phase, no plan yet).
    // (AetherSDR TxApplet.cpp:131–153)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        // ── Phase 3M-1c J.1 ─ TX Profile combo ─────────────────────────────────
        // Populated from MicProfileManager via setMicProfileManager().
        // Right-click → emit txProfileMenuRequested (mirrors Thetis
        // comboTXProfile_MouseDown at console.cs:44519-44522 [v2.10.3.13]).
        m_profileCombo = new QComboBox(this);
        m_profileCombo->addItem(QStringLiteral("Default"));
        m_profileCombo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_profileCombo->setFixedHeight(22);
        applyComboStyle(m_profileCombo);
        m_profileCombo->setAccessibleName(QStringLiteral("TX profile"));
        m_profileCombo->setToolTip(QStringLiteral(
            "TX Profile: left-click to switch.  Right-click to edit "
            "(Setup → Audio → TX Profile)."));
        // Custom context-menu policy so right-click emits
        // customContextMenuRequested instead of the default popup.
        m_profileCombo->setContextMenuPolicy(Qt::CustomContextMenu);
        row->addWidget(m_profileCombo, 1);

        vbox->addLayout(row);
    }
}

} // namespace NereusSDR
