// =================================================================
// src/gui/applets/DigitalApplet.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-18 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Layout ports Thetis setup.cs DIGx tab (digital-mode slider rows) + console.cs VAC wiring. All controls NYI — wired in later phase.
// =================================================================

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

//
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

#include "DigitalApplet.h"
#include "NyiOverlay.h"
#include "gui/ComboStyle.h"
#include "gui/StyleConstants.h"

#include <QComboBox>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace NereusSDR {

// --------------------------------------------------------------------------
// DigitalApplet
// --------------------------------------------------------------------------

DigitalApplet::DigitalApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
}

void DigitalApplet::buildSliderRow(QVBoxLayout* root, const QString& label,
                                    QSlider*& sliderOut, QLabel*& valueLblOut)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(4);

    auto* lbl = new QLabel(label, this);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextSecondary));
    lbl->setFixedWidth(50);
    row->addWidget(lbl);

    sliderOut = new QSlider(Qt::Horizontal, this);
    sliderOut->setRange(0, 100);
    sliderOut->setValue(50);
    sliderOut->setFixedHeight(18);
    row->addWidget(sliderOut, 1);

    valueLblOut = insetValue(QStringLiteral("50"));
    row->addWidget(valueLblOut);

    root->addLayout(row);
}

void DigitalApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(appletTitleBar(QStringLiteral("Digital / VAC")));

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 2, 4, 4);
    vbox->setSpacing(2);

    // -----------------------------------------------------------------------
    // VAC 1 group: enable button + device combo
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_vac1Btn = greenToggle(QStringLiteral("VAC 1"));
        m_vac1Btn->setCheckable(true);
        m_vac1Btn->setFixedWidth(50);
        row->addWidget(m_vac1Btn);

        m_vac1DevCombo = new QComboBox(this);
        m_vac1DevCombo->setToolTip(QStringLiteral("VAC 1 audio device"));
        applyComboStyle(m_vac1DevCombo);
        row->addWidget(m_vac1DevCombo, 1);

        vbox->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // VAC 2 group: enable button + device combo
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_vac2Btn = greenToggle(QStringLiteral("VAC 2"));
        m_vac2Btn->setCheckable(true);
        m_vac2Btn->setFixedWidth(50);
        row->addWidget(m_vac2Btn);

        m_vac2DevCombo = new QComboBox(this);
        m_vac2DevCombo->setToolTip(QStringLiteral("VAC 2 audio device"));
        applyComboStyle(m_vac2DevCombo);
        row->addWidget(m_vac2DevCombo, 1);

        vbox->addLayout(row);
    }

    vbox->addWidget(divider());

    // -----------------------------------------------------------------------
    // Control 5: Sample rate combo — "48000", "96000", "192000"
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Rate"), this);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextSecondary));
        lbl->setFixedWidth(50);
        row->addWidget(lbl);

        m_sampleRateCombo = new QComboBox(this);
        m_sampleRateCombo->addItems({
            QStringLiteral("48000"),
            QStringLiteral("96000"),
            QStringLiteral("192000")
        });
        applyComboStyle(m_sampleRateCombo);
        row->addWidget(m_sampleRateCombo, 1);

        vbox->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // Control 6: Stereo/Mono toggle (blue)
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_stereoBtn = blueToggle(QStringLiteral("Stereo"));
        m_stereoBtn->setCheckable(true);
        m_stereoBtn->setChecked(true);
        row->addWidget(m_stereoBtn, 1);

        vbox->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // Control 7: Buffer size combo — "256", "512", "1024", "2048"
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Buffer"), this);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextSecondary));
        lbl->setFixedWidth(50);
        row->addWidget(lbl);

        m_bufferSizeCombo = new QComboBox(this);
        m_bufferSizeCombo->addItems({
            QStringLiteral("256"),
            QStringLiteral("512"),
            QStringLiteral("1024"),
            QStringLiteral("2048")
        });
        applyComboStyle(m_bufferSizeCombo);
        row->addWidget(m_bufferSizeCombo, 1);

        vbox->addLayout(row);
    }

    vbox->addWidget(divider());

    // -----------------------------------------------------------------------
    // RX Gain + TX Gain sliders
    // -----------------------------------------------------------------------
    buildSliderRow(vbox, QStringLiteral("RX Gain"), m_rxGainSlider, m_rxGainLbl);
    buildSliderRow(vbox, QStringLiteral("TX Gain"), m_txGainSlider, m_txGainLbl);

    // -----------------------------------------------------------------------
    // rigctld channel combo
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("rigctld"), this);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextSecondary));
        lbl->setFixedWidth(50);
        row->addWidget(lbl);

        m_rigctldCombo = new QComboBox(this);
        m_rigctldCombo->addItems({
            QStringLiteral("Ch 1"),
            QStringLiteral("Ch 2"),
            QStringLiteral("Ch 3"),
            QStringLiteral("Ch 4")
        });
        applyComboStyle(m_rigctldCombo);
        row->addWidget(m_rigctldCombo, 1);

        vbox->addLayout(row);
    }

    vbox->addStretch();
    root->addWidget(body);

    // -----------------------------------------------------------------------
    // Mark all controls NYI — Phase 3-VAX
    // -----------------------------------------------------------------------
    const QString kPhase = QStringLiteral("Phase 3-VAX");
    NyiOverlay::markNyi(m_vac1Btn,          kPhase);
    NyiOverlay::markNyi(m_vac1DevCombo,     kPhase);
    NyiOverlay::markNyi(m_vac2Btn,          kPhase);
    NyiOverlay::markNyi(m_vac2DevCombo,     kPhase);
    NyiOverlay::markNyi(m_sampleRateCombo,  kPhase);
    NyiOverlay::markNyi(m_stereoBtn,        kPhase);
    NyiOverlay::markNyi(m_bufferSizeCombo,  kPhase);
    NyiOverlay::markNyi(m_rxGainSlider,     kPhase);
    NyiOverlay::markNyi(m_txGainSlider,     kPhase);
    NyiOverlay::markNyi(m_rigctldCombo,     kPhase);
}

void DigitalApplet::syncFromModel()
{
    // NYI — no model wiring until Phase 3-VAX
}

} // namespace NereusSDR
