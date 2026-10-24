#include "TxFilterSection.h"

#include "gui/StyleConstants.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

// =================================================================
// src/gui/applets/TxFilterSection.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs - original licence from Thetis
//   source is included below.
//
// Extracted from src/gui/applets/TxApplet.h: the TX bandpass filter cutoff spinboxes and the filter status label.
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

TxFilterSection::TxFilterSection(QWidget* parent, int rowSpacing,
                                 const QMargins& outerMargins)
    : QWidget(parent)
{
    m_outerMargins = outerMargins;
    buildUI(rowSpacing);
}

void TxFilterSection::buildUI(int rowSpacing)
{
    // The column the applet puts its rows in, with the applet's own
    // contents margins. The rows below are added to it unchanged.
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(m_outerMargins);
    vbox->setSpacing(rowSpacing);

    // ── Plan 4 Cluster C (Task 4 / D2+D3+D9-status): TX BW spinbox row ──────
    // Low/High cutoff spinboxes for the TX bandpass filter.  Bidirectional with
    // TransmitModel::filterLow / filterHigh (wired in wireControls).  Defaults
    // 100 / 2900 Hz from the model (m_filterLow{100} / m_filterHigh{2900}).
    {
        auto* bwRow = new QHBoxLayout;
        bwRow->setSpacing(4);

        // "TX BW" section label — 56 px wide, bold, kTitleText colour, 10 px.
        auto* bwLbl = new QLabel(QStringLiteral("TX BW"), this);
        bwLbl->setFixedWidth(56);
        bwLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; font-weight: bold; }"
        ).arg(Style::kTitleText));
        bwRow->addWidget(bwLbl);

        // "Lo" sub-label
        auto* loLbl = new QLabel(QStringLiteral("Lo"), this);
        loLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 9.5px; }"
        ).arg(Style::kTextSecondary));
        bwRow->addWidget(loLbl);

        // Low-cutoff spinbox — range [0, 5000] Hz
        m_txFilterLowSpin = new QSpinBox(this);
        m_txFilterLowSpin->setRange(0, 5000);
        m_txFilterLowSpin->setSuffix(QStringLiteral(" Hz"));
        m_txFilterLowSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
        m_txFilterLowSpin->setMinimumWidth(72);
        m_txFilterLowSpin->setAccessibleName(QStringLiteral("TX filter low cutoff"));
        m_txFilterLowSpin->setToolTip(QStringLiteral(
            "TX bandpass filter lower cutoff (Hz).  0 Hz for voice SSB modes."));
        bwRow->addWidget(m_txFilterLowSpin);

        // "Hi" sub-label
        auto* hiLbl = new QLabel(QStringLiteral("Hi"), this);
        hiLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 9.5px; }"
        ).arg(Style::kTextSecondary));
        bwRow->addWidget(hiLbl);

        // High-cutoff spinbox — range [200, 10000] Hz
        m_txFilterHighSpin = new QSpinBox(this);
        m_txFilterHighSpin->setRange(200, 10000);
        m_txFilterHighSpin->setSuffix(QStringLiteral(" Hz"));
        m_txFilterHighSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
        m_txFilterHighSpin->setMinimumWidth(72);
        m_txFilterHighSpin->setAccessibleName(QStringLiteral("TX filter high cutoff"));
        m_txFilterHighSpin->setToolTip(QStringLiteral(
            "TX bandpass filter upper cutoff (Hz).  2900 Hz for typical SSB voice."));
        bwRow->addWidget(m_txFilterHighSpin);

        vbox->addLayout(bwRow);

        // D9 status label — orange tint, right-aligned, 9 px bold.
        // Displays e.g. "100-2900 Hz · 2.8k BW" (asymmetric) or "±2900 Hz · 5.8k BW"
        // (symmetric modes).  Refreshed by filterChanged + dspModeChanged.
        m_txFilterStatusLabel = new QLabel(this);
        m_txFilterStatusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_txFilterStatusLabel->setStyleSheet(QStringLiteral(
            // Plan 4 D9 (Cluster E): colour centralised in Style::kTxFilterOverlayLabel.
            "QLabel { color: %1; font-size: 9px; font-weight: bold; }"
        ).arg(QLatin1String(Style::kTxFilterOverlayLabel)));
        m_txFilterStatusLabel->setAccessibleName(QStringLiteral("TX filter status"));
        vbox->addWidget(m_txFilterStatusLabel);
    }
}

} // namespace NereusSDR
