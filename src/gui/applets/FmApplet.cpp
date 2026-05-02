// =================================================================
// src/gui/applets/FmApplet.cpp  (NereusSDR)
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
//                 Layout ports Thetis setup.cs FM tab (38-tone CTCSS list, 5.0k/2.5k deviation presets, Simplex button) + console.cs FM mode wiring. All controls NYI — wired in later phase.
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

#include "FmApplet.h"
#include "gui/widgets/TriBtn.h"
#include "gui/StyleConstants.h"
#include "NyiOverlay.h"

#include <QComboBox>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace NereusSDR {

// --------------------------------------------------------------------------
// File-local style helpers
// --------------------------------------------------------------------------

// §A2 legitimate exception: FmApplet buttons share the canonical base
// (background: #1a2a3a / border: #205070 / text: #c8d8e8) but use
// kButtonHover (#203040) rather than kButtonAltHover (#204060) used by
// Style::buttonBaseStyle(). Preserving the original hover to avoid an
// unreviewed visual change. Checked states delegate to the canonical
// blueCheckedStyle() / greenCheckedStyle() helpers.
static inline QString fmButtonStyle()
{
    return QStringLiteral(
        "QPushButton {"
        "  background: %1; color: %2;"
        "  border: 1px solid %3; border-radius: 3px;"
        "  padding: 2px 4px; font-size: 10px; font-weight: bold;"
        "}"
        "QPushButton:hover { background: %4; }"
    ).arg(Style::kButtonBg,
          Style::kTextPrimary,
          Style::kBorder,
          Style::kButtonHover);
}

// --------------------------------------------------------------------------
// FmApplet
// --------------------------------------------------------------------------

FmApplet::FmApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
}

void FmApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    // Body margins: (4,2,4,2), spacing 2 — per task spec
    root->setContentsMargins(4, 2, 4, 2);
    root->setSpacing(2);

    // -----------------------------------------------------------------------
    // Control 1: FM Mic slider
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("FM Mic"), this);
        lbl->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 10px; }").arg(NereusSDR::Style::kTextSecondary));
        lbl->setFixedWidth(44);
        row->addWidget(lbl);

        m_micSlider = new QSlider(Qt::Horizontal, this);
        m_micSlider->setRange(0, 100);
        m_micSlider->setValue(50);
        m_micSlider->setFixedHeight(18);
        row->addWidget(m_micSlider, 1);

        m_micValueLbl = new QLabel(QStringLiteral("50"), this);
        m_micValueLbl->setStyleSheet(Style::insetValueStyle());
        m_micValueLbl->setFixedWidth(30);
        m_micValueLbl->setAlignment(Qt::AlignCenter);
        row->addWidget(m_micValueLbl);

        root->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // Control 2: Deviation buttons — "5.0k" + "2.5k", mutually exclusive (blue)
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Dev"), this);
        lbl->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 10px; }").arg(NereusSDR::Style::kTextSecondary));
        lbl->setFixedWidth(44);
        row->addWidget(lbl);

        m_dev5kBtn = new QPushButton(QStringLiteral("5.0k"), this);
        m_dev5kBtn->setCheckable(true);
        m_dev5kBtn->setChecked(true);
        m_dev5kBtn->setFixedHeight(22);
        m_dev5kBtn->setStyleSheet(fmButtonStyle() + Style::blueCheckedStyle());
        row->addWidget(m_dev5kBtn, 1);

        m_dev25kBtn = new QPushButton(QStringLiteral("2.5k"), this);
        m_dev25kBtn->setCheckable(true);
        m_dev25kBtn->setFixedHeight(22);
        m_dev25kBtn->setStyleSheet(fmButtonStyle() + Style::blueCheckedStyle());
        row->addWidget(m_dev25kBtn, 1);

        root->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // Control 3 + 4: CTCSS enable + tone combo
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_ctcssBtn = new QPushButton(QStringLiteral("CTCSS"), this);
        m_ctcssBtn->setCheckable(true);
        m_ctcssBtn->setFixedHeight(22);
        m_ctcssBtn->setFixedWidth(52);
        m_ctcssBtn->setStyleSheet(fmButtonStyle() + Style::greenCheckedStyle());
        row->addWidget(m_ctcssBtn);

        m_ctcssCombo = new QComboBox(this);
        m_ctcssCombo->setFixedHeight(22);
        // Standard CTCSS tones (Hz) — from CTCSS standard tone table
        const QStringList ctcssTones = {
            QStringLiteral("67.0"),  QStringLiteral("71.9"),  QStringLiteral("74.4"),
            QStringLiteral("77.0"),  QStringLiteral("79.7"),  QStringLiteral("82.5"),
            QStringLiteral("85.4"),  QStringLiteral("88.5"),  QStringLiteral("91.5"),
            QStringLiteral("94.8"),  QStringLiteral("97.4"),  QStringLiteral("100.0"),
            QStringLiteral("103.5"), QStringLiteral("107.2"), QStringLiteral("110.9"),
            QStringLiteral("114.8"), QStringLiteral("118.8"), QStringLiteral("123.0"),
            QStringLiteral("127.3"), QStringLiteral("131.8"), QStringLiteral("136.5"),
            QStringLiteral("141.3"), QStringLiteral("146.2"), QStringLiteral("151.4"),
            QStringLiteral("156.7"), QStringLiteral("162.2"), QStringLiteral("167.9"),
            QStringLiteral("173.8"), QStringLiteral("179.9"), QStringLiteral("186.2"),
            QStringLiteral("192.8"), QStringLiteral("203.5"), QStringLiteral("210.7"),
            QStringLiteral("218.1"), QStringLiteral("225.7"), QStringLiteral("233.6"),
            QStringLiteral("241.8"), QStringLiteral("250.3")
        };
        m_ctcssCombo->addItems(ctcssTones);
        row->addWidget(m_ctcssCombo, 1);

        root->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // Control 5: Simplex toggle
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_simplexBtn = new QPushButton(QStringLiteral("Simplex"), this);
        m_simplexBtn->setCheckable(true);
        m_simplexBtn->setFixedHeight(22);
        m_simplexBtn->setStyleSheet(fmButtonStyle() + Style::greenCheckedStyle());
        row->addWidget(m_simplexBtn, 1);

        root->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // Control 6: Repeater offset stepper — ◀ + inset value + ▶
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Offset"), this);
        lbl->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 10px; }").arg(NereusSDR::Style::kTextSecondary));
        lbl->setFixedWidth(44);
        row->addWidget(lbl);

        auto* stepLeft = new TriBtn(TriBtn::Left, this);
        row->addWidget(stepLeft);

        m_offsetLbl = new QLabel(QStringLiteral("0.600 MHz"), this);
        m_offsetLbl->setStyleSheet(Style::insetValueStyle());
        m_offsetLbl->setFixedWidth(60);
        m_offsetLbl->setAlignment(Qt::AlignCenter);
        row->addWidget(m_offsetLbl);

        auto* stepRight = new TriBtn(TriBtn::Right, this);
        row->addWidget(stepRight);

        root->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // Control 7: Offset direction — "-" + "+" + "Rev", mutually exclusive
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Dir"), this);
        lbl->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 10px; }").arg(NereusSDR::Style::kTextSecondary));
        lbl->setFixedWidth(44);
        row->addWidget(lbl);

        m_offsetNegBtn = new QPushButton(QStringLiteral("-"), this);
        m_offsetNegBtn->setCheckable(true);
        m_offsetNegBtn->setChecked(true);
        m_offsetNegBtn->setFixedHeight(22);
        m_offsetNegBtn->setStyleSheet(fmButtonStyle() + Style::blueCheckedStyle());
        row->addWidget(m_offsetNegBtn, 1);

        m_offsetPosBtn = new QPushButton(QStringLiteral("+"), this);
        m_offsetPosBtn->setCheckable(true);
        m_offsetPosBtn->setFixedHeight(22);
        m_offsetPosBtn->setStyleSheet(fmButtonStyle() + Style::blueCheckedStyle());
        row->addWidget(m_offsetPosBtn, 1);

        m_offsetRevBtn = new QPushButton(QStringLiteral("Rev"), this);
        m_offsetRevBtn->setCheckable(true);
        m_offsetRevBtn->setFixedHeight(22);
        m_offsetRevBtn->setStyleSheet(fmButtonStyle() + Style::blueCheckedStyle());
        row->addWidget(m_offsetRevBtn, 1);

        root->addLayout(row);
    }

    // -----------------------------------------------------------------------
    // Control 8: FM TX Profile combo
    // -----------------------------------------------------------------------
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Profile"), this);
        lbl->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 10px; }").arg(NereusSDR::Style::kTextSecondary));
        lbl->setFixedWidth(44);
        row->addWidget(lbl);

        m_txProfileCombo = new QComboBox(this);
        m_txProfileCombo->setFixedHeight(22);
        m_txProfileCombo->addItem(QStringLiteral("Default"));
        row->addWidget(m_txProfileCombo, 1);

        root->addLayout(row);
    }

    root->addStretch();

    // -----------------------------------------------------------------------
    // Mark all controls NYI — Phase 3I-3
    // -----------------------------------------------------------------------
    const QString kPhase = QStringLiteral("Phase 3I-3");
    NyiOverlay::markNyi(m_micSlider,       kPhase);
    NyiOverlay::markNyi(m_dev5kBtn,        kPhase);
    NyiOverlay::markNyi(m_dev25kBtn,       kPhase);
    NyiOverlay::markNyi(m_ctcssBtn,        kPhase);
    NyiOverlay::markNyi(m_ctcssCombo,      kPhase);
    NyiOverlay::markNyi(m_simplexBtn,      kPhase);
    NyiOverlay::markNyi(m_offsetLbl,       kPhase);
    NyiOverlay::markNyi(m_offsetNegBtn,    kPhase);
    NyiOverlay::markNyi(m_offsetPosBtn,    kPhase);
    NyiOverlay::markNyi(m_offsetRevBtn,    kPhase);
    NyiOverlay::markNyi(m_txProfileCombo,  kPhase);
}

void FmApplet::syncFromModel()
{
    // NYI — no model wiring until Phase 3I-3
}

} // namespace NereusSDR
