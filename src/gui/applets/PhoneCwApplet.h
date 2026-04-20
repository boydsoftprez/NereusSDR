#pragma once

// =================================================================
// src/gui/applets/PhoneCwApplet.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
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

#pragma once

#include "AppletWidget.h"

class QStackedWidget;
class QButtonGroup;
class QSlider;
class QPushButton;
class QLabel;
class QComboBox;

namespace NereusSDR {

class HGauge;

// PhoneCwApplet — QStackedWidget with Phone (0) and CW (1) pages.
// FM is a separate FmApplet per the reconciled design spec.
// Phone page: 13 controls — Phase 3I-1 / 3I-3 / 3-VAX
// CW page:     9 controls — Phase 3I-2
class PhoneCwApplet : public AppletWidget {
    Q_OBJECT

public:
    explicit PhoneCwApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("PHCW"); }
    QString appletTitle() const override { return QStringLiteral("Phone / CW"); }
    void syncFromModel() override;

    // Switch the stacked widget page: 0=Phone, 1=CW, 2=FM
    void showPage(int index);

private:
    void buildUI();
    void buildPhonePage(QWidget* page);
    void buildCwPage(QWidget* page);
    void buildFmPage(QWidget* page);
    // ── Shared ───────────────────────────────────────────────────────────────
    QStackedWidget* m_stack{nullptr};
    QButtonGroup*   m_tabGroup{nullptr};
    QPushButton*    m_phoneTabBtn{nullptr};
    QPushButton*    m_cwTabBtn{nullptr};

    // ── Phone page (13 controls) ──────────────────────────────────────────────
    // #1  Mic level gauge (HGauge -40..+10 dBFS, yellow -10, red 0)
    HGauge*      m_levelGauge{nullptr};
    // #2  Compression gauge (HGauge -25..0 dB, reversed=true)
    HGauge*      m_compGauge{nullptr};
    // #3  Mic profile combo
    QComboBox*   m_micProfileCombo{nullptr};
    // #4  Mic source combo (fixed 55px)
    QComboBox*   m_micSourceCombo{nullptr};
    // #5  Mic level slider (0-100) + inset "50"
    QSlider*     m_micLevelSlider{nullptr};
    QLabel*      m_micLevelLabel{nullptr};
    // #6  +ACC button (green toggle, 48px)
    QPushButton* m_accBtn{nullptr};
    // #7  PROC button (green 48px) + slider (0-100) + inset "50"
    QPushButton* m_procBtn{nullptr};
    QSlider*     m_procSlider{nullptr};
    QLabel*      m_procLabel{nullptr};
    // #8  VAX button (blue toggle, 48px)
    QPushButton* m_vaxBtn{nullptr};
    // #9  MON button (green 48px) + slider (0-100) + inset "50"
    QPushButton* m_monBtn{nullptr};
    QSlider*     m_monSlider{nullptr};
    QLabel*      m_monLabel{nullptr};
    // #10 VOX toggle (36px) + level slider + delay slider + 2 insets
    QPushButton* m_voxBtn{nullptr};
    QSlider*     m_voxSlider{nullptr};
    QLabel*      m_voxLvlLabel{nullptr};
    QSlider*     m_voxDlySlider{nullptr};
    QLabel*      m_voxDlyLabel{nullptr};
    // #11 DEXP toggle (36px) + level slider + inset
    QPushButton* m_dexpBtn{nullptr};
    QSlider*     m_dexpSlider{nullptr};
    QLabel*      m_dexpLabel{nullptr};
    // #12 TX filter Low/High sliders + 2 insets
    QSlider*     m_txFiltLowSlider{nullptr};
    QLabel*      m_txFiltLowLabel{nullptr};
    QSlider*     m_txFiltHighSlider{nullptr};
    QLabel*      m_txFiltHighLabel{nullptr};
    // #13 AM Carrier level slider (0-100) + inset "25"
    QSlider*     m_amCarSlider{nullptr};
    QLabel*      m_amCarLabel{nullptr};

    // ── CW page (9 controls) ─────────────────────────────────────────────────
    // #1  ALC gauge (HGauge 0-100, yellow/red 80)
    HGauge*      m_alcGauge{nullptr};
    // #2  CW speed slider (1-60 WPM) + inset "20"
    QSlider*     m_speedSlider{nullptr};
    QLabel*      m_speedLabel{nullptr};
    // #3  CW pitch stepper ◀ [inset "600 Hz" 56px] ▶
    QPushButton* m_pitchDown{nullptr};
    QLabel*      m_pitchLabel{nullptr};
    QPushButton* m_pitchUp{nullptr};
    // #4  Delay slider (0-1000ms) + inset "300"
    QSlider*     m_delaySlider{nullptr};
    QLabel*      m_delayLabel{nullptr};
    // #5  Sidetone toggle (48px) + slider + inset "50"
    QPushButton* m_sidetoneBtn{nullptr};
    QSlider*     m_sidetoneSlider{nullptr};
    QLabel*      m_sidetoneLabel{nullptr};
    // #6  Break-in (QSK) amber toggle
    QPushButton* m_breakinBtn{nullptr};
    // #7  Iambic blue toggle
    QPushButton* m_iambicBtn{nullptr};
    // #8  Firmware keyer green toggle
    QPushButton* m_fwKeyerBtn{nullptr};
    // #9  CW pan slider (-100..100) + inset "C"
    QSlider*     m_cwPanSlider{nullptr};
    QLabel*      m_cwPanLabel{nullptr};

    // ── FM page (8 controls) ─────────────────────────────────────────────────
    // #1  FM MIC slider (0-100) + inset value
    QSlider*     m_fmMicSlider{nullptr};
    QLabel*      m_fmMicLabel{nullptr};
    // #2  Deviation buttons 5.0k / 2.5k (blue toggle)
    QPushButton* m_dev5kBtn{nullptr};
    QPushButton* m_dev25kBtn{nullptr};
    // #3  CTCSS enable (green) + tone combo
    QPushButton* m_ctcssBtn{nullptr};
    QComboBox*   m_ctcssCombo{nullptr};
    // #4  Simplex toggle (green)
    QPushButton* m_simplexBtn{nullptr};
    // #5  Repeater offset slider (0-10000 kHz) + inset "600"
    QSlider*     m_rptOffsetSlider{nullptr};
    QLabel*      m_rptOffsetLabel{nullptr};
    // #6  Offset direction: [-] [+] [Rev] (blue toggles)
    QPushButton* m_offsetMinusBtn{nullptr};
    QPushButton* m_offsetPlusBtn{nullptr};
    QPushButton* m_offsetRevBtn{nullptr};
    // #7  FM TX Profile combo
    QComboBox*   m_fmProfileCombo{nullptr};
    // #8  FM Memory combo + nav
    QComboBox*   m_fmMemCombo{nullptr};
    QPushButton* m_fmMemPrev{nullptr};
    QPushButton* m_fmMemNext{nullptr};
};

} // namespace NereusSDR
