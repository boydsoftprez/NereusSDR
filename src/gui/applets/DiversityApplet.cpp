// =================================================================
// src/gui/applets/DiversityApplet.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/DiversityForm.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-18 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Layout ports Thetis DiversityForm.cs (DIV enable + RX1/RX2 source + Gain(-20..+20 dB) + Phase(0..360°) + ESC Off/Auto/Manual). All controls NYI — wired in later phase.
// =================================================================

//=================================================================
// DiversityForm.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
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

#include "DiversityApplet.h"
#include "NyiOverlay.h"
#include "gui/ComboStyle.h"
#include "gui/StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <QLabel>

namespace NereusSDR {

DiversityApplet::DiversityApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    // Do NOT add appletTitleBar() here — AppletPanelWidget::wrapWithTitleBar
    // already prepends a host-side title bar from appletTitle(). Adding our
    // own here results in a double header. Same fix in PureSignalApplet.

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 2, 4, 4);
    vbox->setSpacing(2);

    // --- Control 1+2: Enable toggle + source selector ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        // Control 1: diversity enable (green toggle)
        m_enableBtn = greenToggle(QStringLiteral("DIV"));
        m_enableBtn->setCheckable(true);
        row->addWidget(m_enableBtn);

        // Control 2: source selector — RX1 / RX2
        m_sourceCombo = new QComboBox(this);
        m_sourceCombo->addItems({QStringLiteral("RX1"), QStringLiteral("RX2")});
        applyComboStyle(m_sourceCombo);
        row->addWidget(m_sourceCombo);
        row->addStretch();

        vbox->addLayout(row);

        NyiOverlay::markNyi(m_enableBtn,   QStringLiteral("3F"));
        NyiOverlay::markNyi(m_sourceCombo, QStringLiteral("3F"));
    }

    // --- Control 3: Gain slider (-20..+20 dB) ---
    m_gainSlider = new QSlider(Qt::Horizontal, this);
    m_gainSlider->setRange(-20, 20);
    m_gainSlider->setValue(0);
    m_gainValue  = insetValue(QStringLiteral("0"));
    vbox->addLayout(sliderRow(QStringLiteral("Gain"), m_gainSlider, m_gainValue));
    NyiOverlay::markNyi(m_gainSlider, QStringLiteral("3F"));

    // --- Control 4: Phase slider (0..360°) ---
    m_phaseSlider = new QSlider(Qt::Horizontal, this);
    m_phaseSlider->setRange(0, 360);
    m_phaseSlider->setValue(0);
    m_phaseValue  = insetValue(QStringLiteral("0"));
    vbox->addLayout(sliderRow(QStringLiteral("Phase"), m_phaseSlider, m_phaseValue));
    NyiOverlay::markNyi(m_phaseSlider, QStringLiteral("3F"));

    // --- Control 5: ESC mode selector — Off / Auto / Manual ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("ESC"), this);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextSecondary));
        lbl->setFixedWidth(62);
        row->addWidget(lbl);

        m_escCombo = new QComboBox(this);
        m_escCombo->addItems({QStringLiteral("Off"),
                              QStringLiteral("Auto"),
                              QStringLiteral("Manual")});
        applyComboStyle(m_escCombo);
        row->addWidget(m_escCombo, 1);

        vbox->addLayout(row);
        NyiOverlay::markNyi(m_escCombo, QStringLiteral("3F"));
    }

    // --- Control 6: R2 Gain + Phase sliders ---
    m_r2GainSlider = new QSlider(Qt::Horizontal, this);
    m_r2GainSlider->setRange(-20, 20);
    m_r2GainSlider->setValue(0);
    m_r2GainValue  = insetValue(QStringLiteral("0"));
    vbox->addLayout(sliderRow(QStringLiteral("R2 Gain"), m_r2GainSlider, m_r2GainValue));
    NyiOverlay::markNyi(m_r2GainSlider, QStringLiteral("3F"));

    m_r2PhaseSlider = new QSlider(Qt::Horizontal, this);
    m_r2PhaseSlider->setRange(0, 360);
    m_r2PhaseSlider->setValue(0);
    m_r2PhaseValue  = insetValue(QStringLiteral("0"));
    vbox->addLayout(sliderRow(QStringLiteral("R2 Phase"), m_r2PhaseSlider, m_r2PhaseValue));
    NyiOverlay::markNyi(m_r2PhaseSlider, QStringLiteral("3F"));

    vbox->addStretch();
    root->addWidget(body);
}

void DiversityApplet::syncFromModel()
{
    // NYI — Phase 3F
}

} // namespace NereusSDR
