#include "TxVoxSection.h"

#include "gui/StyleConstants.h"
#include "gui/widgets/DexpPeakMeter.h"
#include "gui/widgets/InsetValueLabelFit.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

// =================================================================
// src/gui/applets/TxVoxSection.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs - original licence from Thetis
//   source is included below.
//
// Extracted from src/gui/applets/TxApplet.h: the VOX toggle, the threshold
// slider with the live DexpPeakMeter strip, the hold-time slider and their
// inset readouts.
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

TxVoxSection::TxVoxSection(QWidget* parent, int rowSpacing,
                           const QMargins& outerMargins)
    : QWidget(parent)
{
    m_outerMargins = outerMargins;
    buildUI(rowSpacing);
}

void TxVoxSection::buildUI(int rowSpacing)
{
    // The column the applet puts its rows in, with the applet's own
    // contents margins. The row below is added to it unchanged.
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(m_outerMargins);
    vbox->setSpacing(rowSpacing);

auto* row = new QHBoxLayout;
row->setSpacing(4);

m_voxBtn = new QPushButton(QStringLiteral("VOX"), this);
m_voxBtn->setCheckable(true);
m_voxBtn->setFixedWidth(48);
m_voxBtn->setFixedHeight(22);
m_voxBtn->setStyleSheet(Style::buttonBaseStyle()
                        + Style::greenCheckedStyle());
m_voxBtn->setAccessibleName(QStringLiteral("VOX voice-operated transmit"));
m_voxBtn->setObjectName(QStringLiteral("TxVoxButton"));
m_voxBtn->setToolTip(QStringLiteral(
    "VOX: voice-operated transmit.  Left-click to toggle.\n"
    "Right-click to open the DEXP/VOX setup page."));
// CustomContextMenu so right-click hits the openSetupRequested slot
// instead of the default platform menu.
m_voxBtn->setContextMenuPolicy(Qt::CustomContextMenu);
row->addWidget(m_voxBtn);

// Threshold slider-stack: slider on top, DexpPeakMeter strip below.
auto* voxStackGroup = new QWidget(this);
auto* voxStackVbox = new QVBoxLayout(voxStackGroup);
voxStackVbox->setContentsMargins(0, 0, 0, 0);
voxStackVbox->setSpacing(1);

m_voxSlider = new QSlider(Qt::Horizontal, voxStackGroup);
// From Thetis console.Designer.cs:6018-6019 [v2.10.3.13]:
//   ptbVOX.Maximum = 0; ptbVOX.Minimum = -80;
m_voxSlider->setRange(-80, 0);
m_voxSlider->setValue(-20);
m_voxSlider->setFixedHeight(14);
m_voxSlider->setStyleSheet(Style::sliderHStyle());
m_voxSlider->setAccessibleName(QStringLiteral("VOX threshold (dB)"));
m_voxSlider->setObjectName(QStringLiteral("TxVoxThresholdSlider"));
voxStackVbox->addWidget(m_voxSlider);

m_voxPeakMeter = new DexpPeakMeter(voxStackGroup);
m_voxPeakMeter->setObjectName(QStringLiteral("TxVoxPeakMeter"));
m_voxPeakMeter->setAccessibleName(QStringLiteral("VOX live mic peak"));
voxStackVbox->addWidget(m_voxPeakMeter);

row->addWidget(voxStackGroup, 1);

m_voxLvlLabel = new QLabel(QStringLiteral("-20 dB"), this);
m_voxLvlLabel->setStyleSheet(Style::insetValueStyle());
m_voxLvlLabel->setFixedWidth(38);
m_voxLvlLabel->setAlignment(Qt::AlignCenter);
row->addWidget(m_voxLvlLabel);
new InsetValueLabelFit(m_voxLvlLabel, m_voxSlider, QStringLiteral(" dB"), 38);

m_voxDlySlider = new QSlider(Qt::Horizontal, this);
// From Thetis setup.designer.cs:45005-45013 [v2.10.3.13]:
//   udDEXPHold.Maximum = 2000; udDEXPHold.Minimum = 1; (units: ms)
m_voxDlySlider->setRange(1, 2000);
m_voxDlySlider->setValue(500);
m_voxDlySlider->setStyleSheet(Style::sliderHStyle());
m_voxDlySlider->setAccessibleName(QStringLiteral("VOX hold time (ms)"));
m_voxDlySlider->setObjectName(QStringLiteral("TxVoxHoldSlider"));
row->addWidget(m_voxDlySlider, 1);

m_voxDlyLabel = new QLabel(QStringLiteral("500 ms"), this);
m_voxDlyLabel->setStyleSheet(Style::insetValueStyle());
m_voxDlyLabel->setFixedWidth(42);
m_voxDlyLabel->setAlignment(Qt::AlignCenter);
row->addWidget(m_voxDlyLabel);
new InsetValueLabelFit(m_voxDlyLabel, m_voxDlySlider, QStringLiteral(" ms"), 42);

vbox->addLayout(row);
}

void TxVoxSection::setPeakLevel(double normalized01)
{
    if (m_voxPeakMeter) {
        m_voxPeakMeter->setSignalLevel(normalized01);
    }
}

void TxVoxSection::setThresholdMarker(double normalized01)
{
    if (m_voxPeakMeter) {
        m_voxPeakMeter->setThresholdMarker(normalized01);
    }
}

} // namespace NereusSDR
