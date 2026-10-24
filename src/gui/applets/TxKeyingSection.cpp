#include "TxKeyingSection.h"

#include "gui/StyleConstants.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWidget>
#include <utility>

// =================================================================
// src/gui/applets/TxKeyingSection.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs - original licence from Thetis
//   source is included below.
//
// Extracted from src/gui/applets/TxApplet.h: the TUNE and MOX buttons, the
// transmit-slice letter row, the label that says who holds transmit, and
// the Take transmit button.
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

TxKeyingSection::TxKeyingSection(QWidget* parent, int rowSpacing,
                                 const QMargins& outerMargins)
    : QWidget(parent)
{
    m_outerMargins = outerMargins;
    buildUI(rowSpacing);
}

void TxKeyingSection::buildUI(int rowSpacing)
{
    // The column the applet puts its rows in, with the applet's own
    // contents margins. The rows below are added to it unchanged.
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(m_outerMargins);
    vbox->setSpacing(rowSpacing);

auto* row = new QHBoxLayout;
row->setSpacing(2);

const QString btnStyle = Style::buttonBaseStyle()
    + QStringLiteral("QPushButton { padding: 2px; }");
const QString redChecked = Style::redCheckedStyle();

m_tuneBtn = new QPushButton(QStringLiteral("TUNE"), this);
m_tuneBtn->setCheckable(true);
m_tuneBtn->setFixedHeight(22);
m_tuneBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
m_tuneBtn->setStyleSheet(btnStyle + redChecked);
m_tuneBtn->setEnabled(true);  // Phase 3M-1a H.3: wired
m_tuneBtn->setAccessibleName(QStringLiteral("Tune carrier"));
m_tuneBtn->setToolTip(QStringLiteral("Enable TUNE carrier (single-tone CW)"));
row->addWidget(m_tuneBtn, 1);

m_moxBtn = new QPushButton(QStringLiteral("MOX"), this);
m_moxBtn->setCheckable(true);
m_moxBtn->setFixedHeight(22);
m_moxBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
m_moxBtn->setStyleSheet(btnStyle + redChecked);
m_moxBtn->setEnabled(true);  // Phase 3M-1a H.3: wired
m_moxBtn->setAccessibleName(QStringLiteral("MOX transmit"));
m_moxBtn->setToolTip(QStringLiteral("Manual transmit (MOX)"));
row->addWidget(m_moxBtn, 1);

vbox->addLayout(row);

// Slice control plan Task 11 (U8): which slice transmits, one letter
// per slice this window controls; the checked one is the transmit
// slice (refreshTransmitSliceChoices fills it).
m_txSliceRow = new QHBoxLayout;
m_txSliceRow->setSpacing(2);
vbox->addLayout(m_txSliceRow);

// Fix wave I4: who holds transmit on the Core, in a remote window
// (setTransmitHolderText). Empty, and so not shown, otherwise.
m_holderLabel = new QLabel(this);
m_holderLabel->setObjectName(QStringLiteral("TxHolderLabel"));
m_holderLabel->setAccessibleName(QStringLiteral("Who holds transmit"));
m_holderLabel->setWordWrap(true);
m_holderLabel->setStyleSheet(QStringLiteral(
    "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextPrimary));
m_holderLabel->setVisible(false);
vbox->addWidget(m_holderLabel);

// iPhone app plan Task 78 (R-IOS-02, R-IOS-30): beside the line
// that says another device (or the radio) holds transmit, the way
// to take it back; the pan's TX pill offers the same.
m_takeTransmitBtn = new QPushButton(QStringLiteral("Take transmit"), this);
m_takeTransmitBtn->setObjectName(QStringLiteral("TxTakeTransmitButton"));
m_takeTransmitBtn->setAccessibleName(QStringLiteral("Take transmit"));
m_takeTransmitBtn->setToolTip(
    QStringLiteral("Take transmit for this window, from the device that has it or "
                   "when no device does, so MOX, TUNE and programs transmit from here."));
m_takeTransmitBtn->setVisible(false);
vbox->addWidget(m_takeTransmitBtn);
}

void TxKeyingSection::clearTransmitSliceButtons()
{
    if (!m_txSliceRow) { return; }
    for (QPushButton* button : std::as_const(m_txSliceButtons)) {
        m_txSliceRow->removeWidget(button);
        button->deleteLater();
    }
    m_txSliceButtons.clear();
}

QPushButton* TxKeyingSection::addTransmitSliceButton(const QString& letter,
                                                    int sliceId,
                                                    bool checked,
                                                    bool enabled,
                                                    const QString& toolTip)
{
    if (!m_txSliceRow) { return nullptr; }
    const QString btnStyle = Style::buttonBaseStyle()
        + QStringLiteral("QPushButton { padding: 2px; }") + Style::greenCheckedStyle();
    auto* button = new QPushButton(letter, this);
    button->setObjectName(QStringLiteral("TxSliceButton%1").arg(letter));
    button->setCheckable(true);
    button->setChecked(checked);
    button->setFixedHeight(20);
    button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    button->setStyleSheet(btnStyle);
    button->setProperty("sliceId", sliceId);
    button->setAccessibleName(QStringLiteral("Transmit on slice %1").arg(letter));
    // Disabled, never hidden: the reason is the tooltip.
    button->setEnabled(enabled);
    button->setToolTip(toolTip.isEmpty()
        ? QStringLiteral("Transmit on slice %1").arg(letter)
        : toolTip);
    m_txSliceRow->addWidget(button, 1);
    m_txSliceButtons.append(button);
    return button;
}

} // namespace NereusSDR
