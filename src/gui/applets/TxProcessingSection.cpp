#include "TxProcessingSection.h"

#include "gui/StyleConstants.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

// =================================================================
// src/gui/applets/TxProcessingSection.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs - original licence from Thetis
//   source is included below.
//
// Extracted from src/gui/applets/TxApplet.h: the 2-Tone, PS-A and DUP buttons, and the SWR protection LED.
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

TxProcessingSection::TxProcessingSection(QWidget* parent, int rowSpacing,
                                         const QMargins& outerMargins)
    : QWidget(parent)
{
    m_outerMargins = outerMargins;
    buildUI(rowSpacing);
}

void TxProcessingSection::buildUI(int rowSpacing)
{
    // The column the applet puts its rows in, with the applet's own
    // contents margins. The rows below are added to it unchanged.
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(m_outerMargins);
    vbox->setSpacing(rowSpacing);

    // ── Button row 3: 2-Tone + PS-A + DUP ───────────────────────────────────
    // Phase 3M-1a H.3: 2-Tone and PS-A are hidden until their owning phases land.
    //   2-Tone: TODO [3M-3]: visible when 2-tone test feature lands.
    //   PS-A:   TODO [3M-4]: visible when PureSignal lands.
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(2);

        // ── Phase 3M-1c J.2 ─ 2-TONE button ────────────────────────────────────
        // Mirrors Thetis chk2TONE_CheckedChanged (console.cs:44728-44760
        // [v2.10.3.13]).  Wired to TwoToneController via
        // setTwoToneController().  The TUN-stop pre-step + 300 ms settle
        // delay live inside TwoToneController::setActive (Phase I.3) so the
        // button itself just dispatches setActive(checked).
        m_twoToneBtn = new QPushButton(QStringLiteral("2-Tone"), this);
        m_twoToneBtn->setCheckable(true);
        m_twoToneBtn->setFixedHeight(22);
        m_twoToneBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_twoToneBtn->setStyleSheet(Style::buttonBaseStyle() + Style::redCheckedStyle());
        m_twoToneBtn->setAccessibleName(QStringLiteral("2-tone test"));
        m_twoToneBtn->setToolTip(QStringLiteral(
            "Continuous or pulsed two-tone IMD test "
            "(configure in Settings > Test > Two-Tone IMD)."));
        row->addWidget(m_twoToneBtn, 1);

        // PS-A: green when checked — #006030/#008040 are a darker green than kGreenBg=#006040.
        // Phase 3M-4 Task 13: un-hidden under capability gating.  Visibility
        // is set by setBoardCapabilities() / MainWindow board-change handler;
        // hidden by default until that fires (matches the Phase 3M-1a comment
        // pattern of hidden-until-capability-known).
        m_psaBtn = new QPushButton(QStringLiteral("PS-A"), this);
        m_psaBtn->setObjectName(QStringLiteral("TxAppletPsaBtn"));
        m_psaBtn->setCheckable(true);
        m_psaBtn->setFixedHeight(22);
        m_psaBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_psaBtn->setStyleSheet(Style::buttonBaseStyle()
            + QStringLiteral("QPushButton:checked {"
                             " background: #006030; border: 1px solid #008040; color: #fff; }"));
        m_psaBtn->setAccessibleName(QStringLiteral("PS-A PureSignal"));
        m_psaBtn->setToolTip(QStringLiteral(
            "Toggle PureSignal auto-calibration. Right-click to open PureSignal..."));
        // Phase 3M-4 bench-fix: default visible so the button shows on every
        // PS-capable board even if MainWindow's capability gate fires after
        // applet construction (timing race observed at bench).  setBoardCapabilities
        // (TxApplet.cpp:1985) hides for caps.hasPureSignal=false (Atlas/HL2).
        row->addWidget(m_psaBtn, 1);

        m_dupBtn = new QPushButton(QStringLiteral("DUP"), this);
        m_dupBtn->setObjectName(QStringLiteral("TxAppletDupBtn"));
        m_dupBtn->setCheckable(true);
        m_dupBtn->setFixedHeight(22);
        m_dupBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        m_dupBtn->setStyleSheet(Style::buttonBaseStyle() + Style::greenCheckedStyle());
        m_dupBtn->setAccessibleName(QStringLiteral("Display duplex"));
        row->addWidget(m_dupBtn, 1);

        vbox->addLayout(row);
    }

    // ── SWR protection LED inset ─────────────────────────────────────────────
    // xPA button removed (external-PA hardware-specific phase, no plan yet).
    // SWR Prot LED now occupies its own full-width inset row.
    // Inset: fixedHeight 22, bg #0a0a18, border #1e2e3e (AetherSDR TxApplet.cpp:224–253)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        // Inset container for SWR protection LED (styled like AetherSDR atuInset)
        auto* inset = new QWidget(this);
        inset->setFixedHeight(22);
        inset->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        inset->setObjectName(QStringLiteral("xpaInset"));
        inset->setStyleSheet(QStringLiteral(
            "#xpaInset { background: %1; border: 1px solid %2; border-radius: 3px; }"
            "#xpaInset QLabel { border: none; background: transparent; }"
        ).arg(Style::kInsetBg, Style::kInsetBorder));

        auto* insetLayout = new QHBoxLayout(inset);
        insetLayout->setContentsMargins(4, 0, 4, 0);
        insetLayout->setSpacing(2);

        // 15. SWR protection LED — inactive: #405060, 9px bold
        // Matches AetherSDR makeIndicator() pattern (TxApplet.cpp:22–27)
        m_swrProtLed = new QLabel(QStringLiteral("SWR Prot"), inset);
        m_swrProtLed->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 9px; font-weight: bold; }"
        ).arg(Style::kTextInactive));
        m_swrProtLed->setAlignment(Qt::AlignCenter);
        m_swrProtLed->setAccessibleName(QStringLiteral("SWR protection indicator"));
        insetLayout->addWidget(m_swrProtLed);

        row->addWidget(inset, 1); // full width (xPA removed)
        vbox->addLayout(row);
    }
}

} // namespace NereusSDR
