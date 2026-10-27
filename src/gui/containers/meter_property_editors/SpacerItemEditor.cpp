// =================================================================
// src/gui/containers/meter_property_editors/SpacerItemEditor.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.designer.cs (Setup control ranges):
//     upstream source has no top-of-file GPL header, the setup.cs header
//     below covers its partial class
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - PR 677 review fix wave (I3): the Thetis header and
//                 provenance row for the spacer padding range this editor
//                 takes from setup.designer.cs. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

// --- From setup.cs ---
//=================================================================
// setup.cs
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
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
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

// --- From setup.designer.cs ---
// Upstream source 'Project Files/Source/Console/setup.designer.cs' has no top-of-file GPL header —
// project-level LICENSE applies (it is the designer half of the Setup class in setup.cs).

#include "SpacerItemEditor.h"
#include "gui/meters/ItemColourLook.h"
#include "../../meters/MeterItem.h"
#include "../../meters/SpacerItem.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QColorDialog>

namespace NereusSDR {

namespace {
// Padding row: range, step and decimals from Thetis
// setup.designer.cs:63167-63192 [v2.10.3.15] (nudMeterItemSpacerPadding).
constexpr double kPaddingMin = 0.001;
constexpr double kPaddingMax = 10.0;
constexpr double kPaddingStep = 0.001;
constexpr int kPaddingDecimals = 3;
} // namespace

SpacerItemEditor::SpacerItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void SpacerItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    SpacerItem* spacer = qobject_cast<SpacerItem*>(item);
    if (!spacer) { return; }

    beginProgrammaticUpdate();
    ItemColourLook::paintSwatch(m_btnColour1, QStringLiteral("Spacer.colour1"), spacer->colour1());
    ItemColourLook::paintSwatch(m_btnColour2, QStringLiteral("Spacer.colour2"), spacer->colour2());
    m_spinPadding->setValue(static_cast<double>(spacer->padding()));
    m_chkRxTxColours->setChecked(spacer->rxTxColourMode());
    m_chkPaddingSetsHeight->setChecked(spacer->paddingSetsHeight());
    endProgrammaticUpdate();
}

void SpacerItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Spacer"));

    // Colour 1
    m_btnColour1 = new QPushButton(this);
    m_btnColour1->setFixedSize(40, 18);
    connect(m_btnColour1, &QPushButton::clicked, this, [this]() {
        SpacerItem* spacer = qobject_cast<SpacerItem*>(m_item);
        if (!spacer) { return; }
        const QColor chosen = QColorDialog::getColor(spacer->colour1(), this,
                                                     QStringLiteral("Color 1"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            spacer->setColour1(chosen);
            ItemColourLook::paintSwatch(m_btnColour1, QStringLiteral("Spacer.colour1"), chosen);
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Color 1"), m_btnColour1);

    // Colour 2
    m_btnColour2 = new QPushButton(this);
    m_btnColour2->setFixedSize(40, 18);
    connect(m_btnColour2, &QPushButton::clicked, this, [this]() {
        SpacerItem* spacer = qobject_cast<SpacerItem*>(m_item);
        if (!spacer) { return; }
        const QColor chosen = QColorDialog::getColor(spacer->colour2(), this,
                                                     QStringLiteral("Color 2"),
                                                     QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            spacer->setColour2(chosen);
            ItemColourLook::paintSwatch(m_btnColour2, QStringLiteral("Spacer.colour2"), chosen);
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Color 2"), m_btnColour2);

    // Receive / transmit colors (off: a gradient from color 1 to color 2)
    m_chkRxTxColours = makeCheckRow(QStringLiteral("Receive / transmit colors"));
    m_chkRxTxColours->setToolTip(QStringLiteral(
        "Show color 1 while receiving and color 2 while transmitting, fading in on each change. "
        "Off: a gradient from color 1 at the top to color 2 at the bottom."));
    connect(m_chkRxTxColours, &QCheckBox::toggled, this, [this](bool on) {
        SpacerItem* spacer = qobject_cast<SpacerItem*>(m_item);
        if (isProgrammaticUpdate() || !spacer) { return; }
        spacer->setRxTxColourMode(on);
        notifyChanged();
    });

    // Padding: range and step from Thetis setup.designer.cs:63167-63192
    // [v2.10.3.15] (nudMeterItemSpacerPadding: 0.001-10, step 0.001).
    m_spinPadding = makeDoubleRow(QStringLiteral("Padding"), kPaddingMin, kPaddingMax, kPaddingStep, kPaddingDecimals);
    m_spinPadding->setToolTip(QStringLiteral(
        "Size of the spacer as a ratio of its width, used when padding sets the height."));
    connect(m_spinPadding, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        SpacerItem* spacer = qobject_cast<SpacerItem*>(m_item);
        if (!spacer) { return; }
        spacer->setPadding(static_cast<float>(v));
        notifyChanged();
    });

    m_chkPaddingSetsHeight = makeCheckRow(QStringLiteral("Padding sets height"));
    m_chkPaddingSetsHeight->setToolTip(QStringLiteral(
        "Draw the spacer as tall as its width times the padding. Off: it fills its area."));
    connect(m_chkPaddingSetsHeight, &QCheckBox::toggled, this, [this](bool on) {
        SpacerItem* spacer = qobject_cast<SpacerItem*>(m_item);
        if (isProgrammaticUpdate() || !spacer) { return; }
        spacer->setPaddingSetsHeight(on);
        notifyChanged();
    });
}

} // namespace NereusSDR
