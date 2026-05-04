// =================================================================
// src/gui/applets/PhoneCwApplet.cpp  (NereusSDR)
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
//   2026-04-28 — Phase 3M-1b: Mic Gain slider (#5) wired bidirectionally to
//                 TransmitModel::micGainDb (relocated from TxApplet J.1).
//                 Range from BoardCapabilities::micGainMinDb/Max; label shows
//                 dB value; slider greyed when micMute == false.
//                 Mic level gauge (#1) wired to AudioEngine::pcMicInputLevel()
//                 via 50 ms QTimer; linear→dBFS (20*log10); clamped to
//                 gauge range [-40, +10]. Silent at floor when mic not active.
//   2026-05-03 — Phase 3M-3a-iii Task 15: VOX (#10) and DEXP (#11) rows
//                 wired to TransmitModel.  Threshold sliders retuned to dB
//                 ranges; both rows gain a thin DexpPeakMeter strip below
//                 the threshold slider (slider-stack layout).  100 ms
//                 timer drives both meters from TxChannel meter readers
//                 (Task 6).  Right-click on either [ON] button emits
//                 openSetupRequested("Transmit", "DEXP/VOX") so MainWindow
//                 can jump to the DexpVoxPage (Task 14).
//   2026-05-04 — Phase 3M-3a-iii bench polish: VOX row (#10) relocated to
//                 TxApplet (Option B - full row under TUNE/MOX).  Members
//                 m_voxBtn/m_voxSlider/m_voxLvlLabel/m_voxDlySlider/
//                 m_voxDlyLabel/m_voxPeakMeter removed.  DEXP row (#11)
//                 stays.  pollDexpMeters() trimmed to drive only the DEXP
//                 strip; VOX peak polling lives on TxApplet now.
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

#include "PhoneCwApplet.h"
#include "gui/HGauge.h"
#include "gui/ComboStyle.h"
#include "gui/widgets/DexpPeakMeter.h"
#include "NyiOverlay.h"
#include "core/BoardCapabilities.h"
#include "core/AudioEngine.h"
#include "core/MoxController.h"
#include "core/TxChannel.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"
#include "gui/StyleConstants.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace NereusSDR {

// Column widths shared across Phone / FM pages (from AetherSDR PhoneCwApplet.cpp)
static constexpr int kLeftColW = 70;
static constexpr int kValueW   = 36;
// kGap (4) removed — only used by the CW page, now a placeholder (Phase 3M-2).

// NYI phase tags
static const QString kNyiPhone  = QStringLiteral("Phase 3I-1");
// kNyiCw removed — CW page is now a placeholder (Phase 3M-2 deferred).
static const QString kNyiProc   = QStringLiteral("Phase 3I-3");
static const QString kNyiVax    = QStringLiteral("Phase 3-VAX");
static const QString kNyiFm     = QStringLiteral("Phase 3I-1");

// Phone/CW-specific button background — bluer (#1a3a5a) than the
// canonical kButtonBg (#1a2a3a) used by Style::buttonBaseStyle().
// Retained as a tightly-scoped file-local exception per
// docs/architecture/ui-audit-polish-plan.md §A2: "tightly-scoped local
// blocks for legitimate exceptions." The Phone/CW applet's bluer button
// theming is intentional visual differentiation; the canonical helper
// would flatten it (also adds padding: 2px 4px which the original lacked).
static inline QString phoneButtonStyle()
{
    return QStringLiteral(
        "QPushButton { background: #1a3a5a; border: 1px solid %1;"
        "  border-radius: 3px; color: %2; font-size: 10px; font-weight: bold; }"
        "QPushButton:hover { background: %3; }"
    ).arg(NereusSDR::Style::kBorder,
          NereusSDR::Style::kTextPrimary,
          NereusSDR::Style::kButtonAltHover);
}

// CTCSS tones (standard 38-tone list from Thetis setup.cs)
static const QStringList kCtcssTones = {
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
    QStringLiteral("241.8"), QStringLiteral("254.1"),
};


// ── PhoneCwApplet ─────────────────────────────────────────────────────────────

PhoneCwApplet::PhoneCwApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    buildUI();
    wireControls();
}

PhoneCwApplet::~PhoneCwApplet()
{
    if (m_micLevelTimer) {
        m_micLevelTimer->stop();
    }
    if (m_dexpMeterTimer) {
        m_dexpMeterTimer->stop();
    }
}

void PhoneCwApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Phone/CW/FM tab button row ───────────────────────────────────────────
    m_tabGroup = new QButtonGroup(this);
    m_tabGroup->setExclusive(true);
    {
        auto* tabRow = new QHBoxLayout;
        tabRow->setSpacing(2);
        tabRow->setContentsMargins(4, 2, 4, 2);

        m_phoneTabBtn = new QPushButton(QStringLiteral("Phone"), this);
        m_phoneTabBtn->setCheckable(true);
        m_phoneTabBtn->setChecked(true);
        m_phoneTabBtn->setFixedHeight(20);
        m_phoneTabBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_phoneTabBtn->setStyleSheet(phoneButtonStyle() + NereusSDR::Style::blueCheckedStyle());
        m_tabGroup->addButton(m_phoneTabBtn, 0);
        tabRow->addWidget(m_phoneTabBtn);

        m_cwTabBtn = new QPushButton(QStringLiteral("CW"), this);
        m_cwTabBtn->setCheckable(true);
        m_cwTabBtn->setFixedHeight(20);
        m_cwTabBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_cwTabBtn->setStyleSheet(phoneButtonStyle() + NereusSDR::Style::blueCheckedStyle());
        m_tabGroup->addButton(m_cwTabBtn, 1);
        tabRow->addWidget(m_cwTabBtn);

        // FM is a separate FmApplet per reconciled design spec

        root->addLayout(tabRow);
    }

    // ── Stacked widget: Phone (0) / CW (1) ──────────────────────────────────
    m_stack = new QStackedWidget(this);

    auto* phonePage = new QWidget(m_stack);
    buildPhonePage(phonePage);
    m_stack->addWidget(phonePage);  // index 0

    auto* cwPage = new QWidget(m_stack);
    buildCwPage(cwPage);
    m_stack->addWidget(cwPage);     // index 1

    auto* fmPage = new QWidget(m_stack);
    buildFmPage(fmPage);
    m_stack->addWidget(fmPage);     // index 2

    m_stack->setCurrentIndex(0);
    root->addWidget(m_stack);

    // ── Wire tab buttons via QButtonGroup ────────────────────────────────────
    connect(m_tabGroup, &QButtonGroup::idToggled, this,
            [this](int id, bool checked) {
        if (checked) {
            m_stack->setCurrentIndex(id);
        }
    });
}

// ── Phone page (13 controls) ──────────────────────────────────────────────────

void PhoneCwApplet::buildPhonePage(QWidget* page)
{
    // From AetherSDR PhoneCwApplet.cpp buildPhonePanel():
    // contentsMargins(4,2,4,2), spacing 2
    auto* vbox = new QVBoxLayout(page);
    vbox->setContentsMargins(4, 2, 4, 2);
    vbox->setSpacing(2);

    // ── Control 1: Mic level gauge ───────────────────────────────────────────
    // HGauge(-40, +10, redStart=0, yellowStart=-10)
    // Ticks: -40/-30/-20/-10/0/+5/+10
    m_levelGauge = new HGauge(page);
    m_levelGauge->setRange(-40.0, 10.0);
    m_levelGauge->setYellowStart(-10.0);
    m_levelGauge->setRedStart(0.0);
    m_levelGauge->setTitle(QStringLiteral("Level"));
    m_levelGauge->setUnit(QStringLiteral("dB"));
    m_levelGauge->setTickLabels({QStringLiteral("-40dB"), QStringLiteral("-30"),
                                  QStringLiteral("-20"),  QStringLiteral("-10"),
                                  QStringLiteral("0"),    QStringLiteral("+5"),
                                  QStringLiteral("+10")});
    m_levelGauge->setAccessibleName(QStringLiteral("Microphone level gauge"));
    vbox->addWidget(m_levelGauge);

    // ── Control 2: Compression gauge ─────────────────────────────────────────
    // HGauge(-25, 0, redStart=1, reversed=true)
    // Ticks: -25/-20/-15/-10/-5/0
    m_compGauge = new HGauge(page);
    m_compGauge->setRange(-25.0, 0.0);
    m_compGauge->setRedStart(1.0);
    m_compGauge->setReversed(true);
    m_compGauge->setTitle(QStringLiteral("Compression"));
    m_compGauge->setTickLabels({QStringLiteral("-25dB"), QStringLiteral("-20"),
                                 QStringLiteral("-15"),   QStringLiteral("-10"),
                                 QStringLiteral("-5"),    QStringLiteral("0")});
    m_compGauge->setAccessibleName(QStringLiteral("Compression gauge"));
    vbox->addWidget(m_compGauge);
    vbox->addSpacing(4);

    // ── Control 3: Mic profile combo ─────────────────────────────────────────
    m_micProfileCombo = new QComboBox(page);
    m_micProfileCombo->setFixedHeight(22);
    m_micProfileCombo->addItems({QStringLiteral("Default"), QStringLiteral("DX"),
                                 QStringLiteral("Contest"), QStringLiteral("Custom")});
    m_micProfileCombo->setAccessibleName(QStringLiteral("Microphone profile"));
    applyComboStyle(m_micProfileCombo);
    vbox->addWidget(m_micProfileCombo);

    // ── Control 4: Mic source + Control 5: Mic level slider + Control 6: +ACC ─
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        // Control 4: Mic source combo (fixedWidth 55, fixedHeight 22)
        m_micSourceCombo = new QComboBox(page);
        m_micSourceCombo->setFixedWidth(55);
        m_micSourceCombo->setFixedHeight(22);
        m_micSourceCombo->addItems({QStringLiteral("MIC"), QStringLiteral("BAL"),
                                    QStringLiteral("LINE"), QStringLiteral("ACC"),
                                    QStringLiteral("PC")});
        m_micSourceCombo->setAccessibleName(QStringLiteral("Microphone source"));
        applyComboStyle(m_micSourceCombo);
        row->addWidget(m_micSourceCombo);

        // Control 5a: Mic gain slider
        // Range and initial value set in wireControls() once RadioModel is ready.
        // Placeholder range (0,100) is overwritten before the widget is visible.
        // Phase 3M-1b: bidirectional with TransmitModel::micGainDb.
        m_micLevelSlider = new QSlider(Qt::Horizontal, page);
        m_micLevelSlider->setRange(0, 100);
        m_micLevelSlider->setValue(50);
        m_micLevelSlider->setStyleSheet(NereusSDR::Style::sliderHStyle());
        m_micLevelSlider->setToolTip(QStringLiteral("Microphone input gain (dB)"));
        m_micLevelSlider->setAccessibleName(QStringLiteral("Microphone gain"));
        row->addWidget(m_micLevelSlider, 1);

        // Control 5b: Value label — widened to 35px for "-40 dB" / "+10 dB" strings.
        // Phase 3M-1b: shows dB value e.g. "-6 dB".
        m_micLevelLabel = new QLabel(QStringLiteral("-6 dB"), page);
        m_micLevelLabel->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 10px; }").arg(NereusSDR::Style::kTextPrimary));
        m_micLevelLabel->setFixedWidth(35);
        m_micLevelLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(m_micLevelLabel);

        // Control 6: +ACC button (green, checkable, fixedWidth 48, fixedHeight 22)
        m_accBtn = new QPushButton(QStringLiteral("+ACC"), page);
        m_accBtn->setCheckable(true);
        m_accBtn->setFixedWidth(48);
        m_accBtn->setFixedHeight(22);
        m_accBtn->setStyleSheet(phoneButtonStyle() + NereusSDR::Style::greenCheckedStyle());
        m_accBtn->setAccessibleName(QStringLiteral("Accessory mic input"));
        row->addWidget(m_accBtn);

        vbox->addLayout(row);
    }

    // ── Control 7: PROC + Control 8: PROC slider + Control 9: VAX ───────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        // Control 7: PROC button (green, checkable, fixedWidth 48, fixedHeight 22)
        // Phase 3M-3a-ii post-bench cleanup: bidirectional with
        // TransmitModel::cpdrOn (wired in wireControls()).
        m_procBtn = new QPushButton(QStringLiteral("PROC"), page);
        m_procBtn->setCheckable(true);
        m_procBtn->setFixedWidth(48);
        m_procBtn->setFixedHeight(22);
        m_procBtn->setStyleSheet(phoneButtonStyle() + NereusSDR::Style::greenCheckedStyle());
        m_procBtn->setAccessibleName(QStringLiteral("Speech processor"));
        m_procBtn->setObjectName(QStringLiteral("PhoneCwProcButton"));
        m_procBtn->setToolTip(QStringLiteral(
            "CPDR speech compressor — left-click toggles."));
        row->addWidget(m_procBtn);

        // Control 8: PROC slider (0..20 dB CPDR level) + numeric "X dB"
        // value label above-right.  Range from Thetis ptbCPDR
        // (console.Designer.cs:6042-6043 [v2.10.3.13]):
        //   ptbCPDR.Maximum = 20;  ptbCPDR.Minimum = 0;
        // Replaces the previous 3-position NOR/DX/DX+ placeholder.
        auto* procGroup = new QWidget(page);
        auto* procVbox = new QVBoxLayout(procGroup);
        procVbox->setContentsMargins(0, 0, 0, 0);
        procVbox->setSpacing(0);

        // Numeric value label, right-aligned: 8px kTextPrimary — compact
        // value readout matching adjacent slider tick labels.
        m_procValueLabel = new QLabel(QStringLiteral("0 dB"), procGroup);
        m_procValueLabel->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 8px; }").arg(NereusSDR::Style::kTextPrimary));
        m_procValueLabel->setAlignment(Qt::AlignRight | Qt::AlignBottom);
        procVbox->addWidget(m_procValueLabel);

        m_procSlider = new QSlider(Qt::Horizontal, procGroup);
        // From Thetis console.Designer.cs:6042-6043 [v2.10.3.13]:
        //   ptbCPDR.Maximum = 20;  ptbCPDR.Minimum = 0;
        m_procSlider->setRange(0, 20);
        m_procSlider->setTickInterval(1);
        m_procSlider->setTickPosition(QSlider::NoTicks);
        m_procSlider->setPageStep(1);
        m_procSlider->setFixedHeight(14);
        m_procSlider->setStyleSheet(NereusSDR::Style::sliderHStyle());
        m_procSlider->setAccessibleName(QStringLiteral("CPDR speech compressor level (dB)"));
        m_procSlider->setObjectName(QStringLiteral("PhoneCwProcSlider"));
        m_procSlider->setToolTip(QStringLiteral(
            "CPDR speech compressor level (dB).  Range 0..20 dB matches "
            "Thetis ptbCPDR."));
        procVbox->addWidget(m_procSlider);

        row->addWidget(procGroup, 1);

        // Control 9: VAX button (blue, checkable, fixedWidth 48, fixedHeight 22)
        m_vaxBtn = new QPushButton(QStringLiteral("VAX"), page);
        m_vaxBtn->setCheckable(true);
        m_vaxBtn->setFixedWidth(48);
        m_vaxBtn->setFixedHeight(22);
        m_vaxBtn->setStyleSheet(phoneButtonStyle() + NereusSDR::Style::blueCheckedStyle());
        m_vaxBtn->setAccessibleName(QStringLiteral("VAX digital audio"));
        row->addWidget(m_vaxBtn);

        vbox->addLayout(row);
    }

    // ── Control 10: MON button + level slider ────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_monBtn = new QPushButton(QStringLiteral("MON"), page);
        m_monBtn->setCheckable(true);
        m_monBtn->setFixedWidth(48);
        m_monBtn->setFixedHeight(22);
        m_monBtn->setStyleSheet(phoneButtonStyle() + NereusSDR::Style::greenCheckedStyle());
        m_monBtn->setAccessibleName(QStringLiteral("TX monitor"));
        row->addWidget(m_monBtn);

        m_monSlider = new QSlider(Qt::Horizontal, page);
        m_monSlider->setRange(0, 100);
        m_monSlider->setValue(50);
        m_monSlider->setStyleSheet(NereusSDR::Style::sliderHStyle());
        m_monSlider->setAccessibleName(QStringLiteral("Monitor level"));
        row->addWidget(m_monSlider, 1);

        auto* monLabel = new QLabel(QStringLiteral("50"), page);
        monLabel->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 10px; }").arg(NereusSDR::Style::kTextPrimary));
        monLabel->setFixedWidth(22);
        monLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(monLabel);

        vbox->addLayout(row);
    }

    // (Control 10 VOX row relocated to TxApplet 2026-05-04 — bench polish
    //  feedback that VOX engage surface should sit next to MOX/TUNE on the
    //  TX right pane, not buried on the Phone tab.  See TxApplet section 4b.)

    // ── Control 11: DEXP toggle + threshold slider-stack ────────────────────
    // Phase 3M-3a-iii Task 15.  Layout:
    //   [DEXP btn 36px] | { Threshold slider top, DexpPeakMeter strip below }
    //   | [-50 dB inset]
    //
    // Threshold slider range -160..0 dB matches Thetis ptbNoiseGate scale
    // per console.cs:28974-28980 [v2.10.3.13] (the picNoiseGate paint maps
    // (value + 160) / 160 to the strip width).  Default -50 dB.
    //
    // Slider is DECORATIVE — see wireControls() for the verbatim Thetis
    // quirk quote.
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_dexpBtn = new QPushButton(QStringLiteral("DEXP"), page);
        m_dexpBtn->setCheckable(true);
        m_dexpBtn->setFixedWidth(48);
        m_dexpBtn->setFixedHeight(22);
        m_dexpBtn->setStyleSheet(phoneButtonStyle() + NereusSDR::Style::greenCheckedStyle());
        m_dexpBtn->setAccessibleName(QStringLiteral("Downward expander / noise gate"));
        m_dexpBtn->setObjectName(QStringLiteral("PhoneCwDexpButton"));
        row->addWidget(m_dexpBtn);

        // Threshold slider-stack: slider on top, DexpPeakMeter strip below.
        auto* dexpStackGroup = new QWidget(page);
        auto* dexpStackVbox = new QVBoxLayout(dexpStackGroup);
        dexpStackVbox->setContentsMargins(0, 0, 0, 0);
        dexpStackVbox->setSpacing(1);

        m_dexpSlider = new QSlider(Qt::Horizontal, dexpStackGroup);
        // Range -160..0 dB matches Thetis ptbNoiseGate scale per
        // console.cs:28974-28980 [v2.10.3.13] picNoiseGate_Paint.
        m_dexpSlider->setRange(-160, 0);
        m_dexpSlider->setValue(-50);
        m_dexpSlider->setFixedHeight(14);
        m_dexpSlider->setStyleSheet(NereusSDR::Style::sliderHStyle());
        m_dexpSlider->setAccessibleName(QStringLiteral("DEXP threshold marker (dB)"));
        m_dexpSlider->setObjectName(QStringLiteral("PhoneCwDexpThresholdSlider"));
        dexpStackVbox->addWidget(m_dexpSlider);

        m_dexpPeakMeter = new DexpPeakMeter(dexpStackGroup);
        m_dexpPeakMeter->setObjectName(QStringLiteral("PhoneCwDexpPeakMeter"));
        m_dexpPeakMeter->setAccessibleName(QStringLiteral("DEXP live mic peak"));
        dexpStackVbox->addWidget(m_dexpPeakMeter);

        row->addWidget(dexpStackGroup, 1);

        m_dexpLabel = new QLabel(QStringLiteral("-50 dB"), page);
        m_dexpLabel->setStyleSheet(NereusSDR::Style::insetValueStyle());
        m_dexpLabel->setFixedWidth(38);
        m_dexpLabel->setAlignment(Qt::AlignCenter);
        row->addWidget(m_dexpLabel);

        vbox->addLayout(row);
    }

    // TX filter Low/High Cut sliders removed — superseded by the Lo/Hi
    // spinboxes on TxApplet (Plan 4 Cluster C) which are wired end-to-end
    // through TransmitModel::filterChanged → SetTXABandpassFreqs.  These
    // sliders were NYI placeholders that never reached WDSP.

    // ── Control 13: AM Carrier level slider (0-100) + inset "25" ────────────
    // Spec: Phase 3I-3
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("AM Car:"), page);
        lbl->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 10px; }").arg(NereusSDR::Style::kTextSecondary));
        lbl->setFixedWidth(40);
        row->addWidget(lbl);

        m_amCarSlider = new QSlider(Qt::Horizontal, page);
        m_amCarSlider->setRange(0, 100);
        m_amCarSlider->setValue(25);
        m_amCarSlider->setStyleSheet(NereusSDR::Style::sliderHStyle());
        m_amCarSlider->setAccessibleName(QStringLiteral("AM carrier level"));
        row->addWidget(m_amCarSlider, 1);

        m_amCarLabel = new QLabel(QStringLiteral("25"), page);
        m_amCarLabel->setStyleSheet(NereusSDR::Style::insetValueStyle());
        m_amCarLabel->setFixedWidth(30);
        m_amCarLabel->setAlignment(Qt::AlignCenter);
        row->addWidget(m_amCarLabel);

        vbox->addLayout(row);
    }

    // ── Mark Phone controls NYI (wired controls NOT marked) ──────────────────
    // #1  m_levelGauge       — wired (Phase 3M-1b mic level gauge)
    // #5  m_micLevelSlider   — wired (Phase 3M-1b mic gain)
    // #7  m_procBtn / m_procSlider — wired (Phase 3M-3a-ii post-bench cleanup)
    // #10 VOX row relocated to TxApplet 2026-05-04 (no NYI mark needed —
    //     members removed entirely)
    // #11 m_dexpBtn / m_dexpSlider — wired (Phase 3M-3a-iii Task 15;
    //     m_dexpSlider is decorative-only per Thetis quirk, see wireControls())
    NyiOverlay::markNyi(m_compGauge,        kNyiProc);    // #2 — Phase 3I-3
    NyiOverlay::markNyi(m_micProfileCombo,  kNyiPhone);   // #3
    NyiOverlay::markNyi(m_micSourceCombo,   kNyiPhone);   // #4
    NyiOverlay::markNyi(m_accBtn,           kNyiPhone);   // #6
    NyiOverlay::markNyi(m_vaxBtn,           kNyiVax);     // #8 — Phase 3-VAX
    NyiOverlay::markNyi(m_monBtn,           kNyiPhone);   // #9
    NyiOverlay::markNyi(m_monSlider,        kNyiPhone);   // #9 slider
    NyiOverlay::markNyi(m_amCarSlider,      kNyiProc);    // #13 — Phase 3I-3
}

// ── CW page — placeholder until Phase 3M-2 ───────────────────────────────────
// Per docs/superpowers/plans/2026-05-01-ui-polish-right-panel.md §Task 7.
// CW TX (speed, pitch, sidetone, break-in, iambic, firmware keyer) is deferred
// to Phase 3M-2. The original 9-control NYI page has been replaced with a
// single placeholder message so users aren't left clicking dead controls.

void PhoneCwApplet::buildCwPage(QWidget* page)
{
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 24, 8, 24);
    layout->setAlignment(Qt::AlignCenter);

    auto* label = new QLabel(QStringLiteral(
        "CW TX coming in Phase 3M-2.\n\n"
        "Speed, pitch, sidetone, break-in, iambic, firmware keyer\n"
        "controls will appear here when CW TX is wired up.\n\n"
        "For now, see Setup \xe2\x86\x92 DSP \xe2\x86\x92 CW for available CW config."
    ), page);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }"
    ).arg(NereusSDR::Style::kTextSecondary));
    layout->addWidget(label);
}

// ── FM page (8 controls) ──────────────────────────────────────────────────────

void PhoneCwApplet::buildFmPage(QWidget* page)
{
    auto* vbox = new QVBoxLayout(page);
    vbox->setContentsMargins(4, 2, 4, 4);
    vbox->setSpacing(2);

    // ── Control 1: FM MIC slider (0-100) + inset value ───────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("FM MIC:"), page);
        lbl->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 10px; }").arg(NereusSDR::Style::kTextSecondary));
        lbl->setFixedWidth(kLeftColW);
        row->addWidget(lbl);

        m_fmMicSlider = new QSlider(Qt::Horizontal, page);
        m_fmMicSlider->setRange(0, 100);
        m_fmMicSlider->setValue(50);
        m_fmMicSlider->setStyleSheet(NereusSDR::Style::sliderHStyle());
        m_fmMicSlider->setAccessibleName(QStringLiteral("FM microphone level"));
        row->addWidget(m_fmMicSlider, 1);

        m_fmMicLabel = new QLabel(QStringLiteral("50"), page);
        m_fmMicLabel->setStyleSheet(NereusSDR::Style::insetValueStyle());
        m_fmMicLabel->setFixedWidth(kValueW);
        m_fmMicLabel->setAlignment(Qt::AlignCenter);
        row->addWidget(m_fmMicLabel);

        vbox->addLayout(row);
    }

    // ── Control 2: Deviation [5.0k] [2.5k] blue toggles ─────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Dev:"), page);
        lbl->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 10px; }").arg(NereusSDR::Style::kTextSecondary));
        lbl->setFixedWidth(28);
        row->addWidget(lbl);

        m_dev5kBtn = new QPushButton(QStringLiteral("5.0k"), page);
        m_dev5kBtn->setCheckable(true);
        m_dev5kBtn->setChecked(true);
        m_dev5kBtn->setFixedHeight(22);
        m_dev5kBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_dev5kBtn->setStyleSheet(phoneButtonStyle() + NereusSDR::Style::blueCheckedStyle());
        m_dev5kBtn->setAccessibleName(QStringLiteral("5 kHz deviation"));
        row->addWidget(m_dev5kBtn, 1);

        m_dev25kBtn = new QPushButton(QStringLiteral("2.5k"), page);
        m_dev25kBtn->setCheckable(true);
        m_dev25kBtn->setFixedHeight(22);
        m_dev25kBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_dev25kBtn->setStyleSheet(phoneButtonStyle() + NereusSDR::Style::blueCheckedStyle());
        m_dev25kBtn->setAccessibleName(QStringLiteral("2.5 kHz deviation (narrow FM)"));
        row->addWidget(m_dev25kBtn, 1);

        row->addStretch();
        vbox->addLayout(row);
    }

    // ── Control 3: CTCSS enable (green) + tone combo ─────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_ctcssBtn = new QPushButton(QStringLiteral("CTCSS"), page);
        m_ctcssBtn->setCheckable(true);
        m_ctcssBtn->setFixedHeight(22);
        m_ctcssBtn->setFixedWidth(52);
        m_ctcssBtn->setStyleSheet(phoneButtonStyle() + NereusSDR::Style::greenCheckedStyle());
        m_ctcssBtn->setAccessibleName(QStringLiteral("CTCSS sub-audible tone squelch"));
        row->addWidget(m_ctcssBtn);

        m_ctcssCombo = new QComboBox(page);
        m_ctcssCombo->addItems(kCtcssTones);
        applyComboStyle(m_ctcssCombo);
        m_ctcssCombo->setAccessibleName(QStringLiteral("CTCSS tone frequency"));
        row->addWidget(m_ctcssCombo, 1);

        vbox->addLayout(row);
    }

    // ── Control 4: Simplex toggle ─────────────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_simplexBtn = new QPushButton(QStringLiteral("Simplex"), page);
        m_simplexBtn->setCheckable(true);
        m_simplexBtn->setFixedHeight(22);
        m_simplexBtn->setStyleSheet(phoneButtonStyle() + NereusSDR::Style::greenCheckedStyle());
        m_simplexBtn->setAccessibleName(QStringLiteral("Simplex (no repeater offset)"));
        row->addWidget(m_simplexBtn);
        row->addStretch();

        vbox->addLayout(row);
    }

    vbox->addSpacing(4);

    // ── Control 5: Repeater offset slider (0-10000 kHz) + inset "600" ────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Offset:"), page);
        lbl->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 10px; }").arg(NereusSDR::Style::kTextSecondary));
        lbl->setFixedWidth(kLeftColW);
        row->addWidget(lbl);

        m_rptOffsetSlider = new QSlider(Qt::Horizontal, page);
        m_rptOffsetSlider->setRange(0, 10000);
        m_rptOffsetSlider->setValue(600);
        m_rptOffsetSlider->setStyleSheet(NereusSDR::Style::sliderHStyle());
        m_rptOffsetSlider->setAccessibleName(QStringLiteral("Repeater offset (kHz)"));
        row->addWidget(m_rptOffsetSlider, 1);

        m_rptOffsetLabel = new QLabel(QStringLiteral("600"), page);
        m_rptOffsetLabel->setStyleSheet(NereusSDR::Style::insetValueStyle());
        m_rptOffsetLabel->setFixedWidth(kValueW);
        m_rptOffsetLabel->setAlignment(Qt::AlignCenter);
        row->addWidget(m_rptOffsetLabel);

        vbox->addLayout(row);
    }

    // ── Control 6: Offset direction [-] [+] [Rev] ────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Dir:"), page);
        lbl->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 10px; }").arg(NereusSDR::Style::kTextSecondary));
        lbl->setFixedWidth(24);
        row->addWidget(lbl);

        m_offsetMinusBtn = new QPushButton(QStringLiteral("-"), page);
        m_offsetMinusBtn->setCheckable(true);
        m_offsetMinusBtn->setFixedHeight(22);
        m_offsetMinusBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_offsetMinusBtn->setStyleSheet(phoneButtonStyle() + NereusSDR::Style::blueCheckedStyle());
        m_offsetMinusBtn->setAccessibleName(QStringLiteral("Negative repeater offset"));
        row->addWidget(m_offsetMinusBtn, 1);

        m_offsetPlusBtn = new QPushButton(QStringLiteral("+"), page);
        m_offsetPlusBtn->setCheckable(true);
        m_offsetPlusBtn->setFixedHeight(22);
        m_offsetPlusBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_offsetPlusBtn->setStyleSheet(phoneButtonStyle() + NereusSDR::Style::blueCheckedStyle());
        m_offsetPlusBtn->setAccessibleName(QStringLiteral("Positive repeater offset"));
        row->addWidget(m_offsetPlusBtn, 1);

        m_offsetRevBtn = new QPushButton(QStringLiteral("Rev"), page);
        m_offsetRevBtn->setCheckable(true);
        m_offsetRevBtn->setFixedHeight(22);
        m_offsetRevBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_offsetRevBtn->setStyleSheet(phoneButtonStyle() + NereusSDR::Style::blueCheckedStyle());
        m_offsetRevBtn->setAccessibleName(QStringLiteral("Reverse repeater offset"));
        row->addWidget(m_offsetRevBtn, 1);

        vbox->addLayout(row);
    }

    vbox->addSpacing(4);

    // ── Control 7: FM TX Profile combo ───────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Profile:"), page);
        lbl->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 10px; }").arg(NereusSDR::Style::kTextSecondary));
        lbl->setFixedWidth(kLeftColW);
        row->addWidget(lbl);

        m_fmProfileCombo = new QComboBox(page);
        m_fmProfileCombo->addItems({QStringLiteral("Default"),
                                    QStringLiteral("Narrow"),
                                    QStringLiteral("Wide")});
        applyComboStyle(m_fmProfileCombo);
        m_fmProfileCombo->setAccessibleName(QStringLiteral("FM TX profile"));
        row->addWidget(m_fmProfileCombo, 1);

        vbox->addLayout(row);
    }

    // ── Control 8: FM Memory combo + ◀ ▶ nav ────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* lbl = new QLabel(QStringLiteral("Memory:"), page);
        lbl->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 10px; }").arg(NereusSDR::Style::kTextSecondary));
        lbl->setFixedWidth(kLeftColW);
        row->addWidget(lbl);

        m_fmMemCombo = new QComboBox(page);
        m_fmMemCombo->addItem(QStringLiteral("(none)"));
        applyComboStyle(m_fmMemCombo);
        m_fmMemCombo->setAccessibleName(QStringLiteral("FM memory channel"));
        row->addWidget(m_fmMemCombo, 1);

        m_fmMemPrev = new QPushButton(QStringLiteral("\u25c4"), page);
        m_fmMemPrev->setFixedSize(22, 22);
        m_fmMemPrev->setStyleSheet(phoneButtonStyle());
        m_fmMemPrev->setAccessibleName(QStringLiteral("Previous FM memory"));
        row->addWidget(m_fmMemPrev);

        m_fmMemNext = new QPushButton(QStringLiteral("\u25ba"), page);
        m_fmMemNext->setFixedSize(22, 22);
        m_fmMemNext->setStyleSheet(phoneButtonStyle());
        m_fmMemNext->setAccessibleName(QStringLiteral("Next FM memory"));
        row->addWidget(m_fmMemNext);

        vbox->addLayout(row);
    }

    vbox->addStretch();

    // ── Mark all FM controls NYI (Phase 3I-1) ────────────────────────────────
    NyiOverlay::markNyi(m_fmMicSlider,     kNyiFm);
    NyiOverlay::markNyi(m_fmMicLabel,      kNyiFm);
    NyiOverlay::markNyi(m_dev5kBtn,        kNyiFm);
    NyiOverlay::markNyi(m_dev25kBtn,       kNyiFm);
    NyiOverlay::markNyi(m_ctcssBtn,        kNyiFm);
    NyiOverlay::markNyi(m_ctcssCombo,      kNyiFm);
    NyiOverlay::markNyi(m_simplexBtn,      kNyiFm);
    NyiOverlay::markNyi(m_rptOffsetSlider, kNyiFm);
    NyiOverlay::markNyi(m_rptOffsetLabel,  kNyiFm);
    NyiOverlay::markNyi(m_offsetMinusBtn,  kNyiFm);
    NyiOverlay::markNyi(m_offsetPlusBtn,   kNyiFm);
    NyiOverlay::markNyi(m_offsetRevBtn,    kNyiFm);
    NyiOverlay::markNyi(m_fmProfileCombo,  kNyiFm);
    NyiOverlay::markNyi(m_fmMemCombo,      kNyiFm);
    NyiOverlay::markNyi(m_fmMemPrev,       kNyiFm);
    NyiOverlay::markNyi(m_fmMemNext,       kNyiFm);
}

// ── wireControls — Phase 3M-1b mic gain slider + mic level gauge ─────────────
//
// Wires the two now-active Phone page controls:
//
//  #5 Mic gain slider (m_micLevelSlider):
//    - Range reset from BoardCapabilities::micGainMinDb/Max (per-board).
//    - Initial value from TransmitModel::micGainDb() (default -6 dB).
//    - UI → Model: slider valueChanged → tx.setMicGainDb(val).
//    - Model → UI: micGainDbChanged → update slider + label (QSignalBlocker).
//    - Mic mute: micMuteChanged(false) → slider disabled (greyed).
//
//  #1 Mic level gauge (m_levelGauge):
//    - 50 ms QTimer (20 fps) polls AudioEngine::pcMicInputLevel().
//    - linear 0..1 → dBFS (20*log10); floor at -60 dBFS → clamped to gauge
//      min (-40 dB). Reads 0.0f (floor) when TX input bus is not open.
//    - Guard: NaN/Inf input clamps to -40 dB (gauge floor).
//
// m_updatingFromModel inherited from AppletWidget prevents feedback loops.
void PhoneCwApplet::wireControls()
{
    if (!m_model) {
        return;
    }

    TransmitModel& tx = m_model->transmitModel();

    // ── #5 Mic gain slider ────────────────────────────────────────────────────
    // Reset range from per-board capabilities (overrides placeholder 0..100).
    // From Thetis console.cs:19151-19171 [v2.10.3.13]:
    //   private int mic_gain_min = -40;   private int mic_gain_max = 10;
    {
        const BoardCapabilities& caps = m_model->boardCapabilities();
        QSignalBlocker b(m_micLevelSlider);
        m_micLevelSlider->setRange(caps.micGainMinDb, caps.micGainMaxDb);
        m_micLevelSlider->setValue(tx.micGainDb());
        m_micLevelLabel->setText(QStringLiteral("%1 dB").arg(tx.micGainDb()));
        // micMute == true → mic in use → slider enabled.
        // (Thetis counter-intuitive naming: console.cs:28752 [v2.10.3.13])
        m_micLevelSlider->setEnabled(tx.micMute());
    }

    // UI → Model
    connect(m_micLevelSlider, &QSlider::valueChanged, this, [this, &tx](int val) {
        if (m_updatingFromModel) { return; }
        m_micLevelLabel->setText(QStringLiteral("%1 dB").arg(val));
        tx.setMicGainDb(val);
    });

    // Model → UI (micGainDbChanged)
    connect(&tx, &TransmitModel::micGainDbChanged, this, [this](int dB) {
        m_updatingFromModel = true;
        {
            QSignalBlocker b(m_micLevelSlider);
            m_micLevelSlider->setValue(dB);
        }
        m_micLevelLabel->setText(QStringLiteral("%1 dB").arg(dB));
        m_updatingFromModel = false;
    });

    // Mic mute state → slider enabled / greyed.
    // NOTE: micMute == true means mic IS in use; false means muted/disabled.
    connect(&tx, &TransmitModel::micMuteChanged, this, [this](bool micInUse) {
        m_micLevelSlider->setEnabled(micInUse);
    });

    // ── #7 PROC button + slider (CPDR speech compressor) ────────────────────
    // Phase 3M-3a-ii post-bench cleanup: wires the un-wired NyiOverlay-marked
    // PROC controls inherited from 3I-3.  The duplicate [PROC] in TxApplet
    // was removed in the same commit.
    //
    //   m_procBtn    ↔ TransmitModel::cpdrOn (bidirectional, echo-guarded).
    //   m_procSlider ↔ TransmitModel::cpdrLevelDb (bidirectional, dB range
    //                  0..20 from console.Designer.cs:6042-6043 [v2.10.3.13]).
    //   m_procValueLabel mirrors slider value as "X dB".
    //
    // Initial UI state seeded from current TM via QSignalBlocker so the
    // setters don't bounce.
    if (m_procBtn) {
        {
            QSignalBlocker b(m_procBtn);
            m_procBtn->setChecked(tx.cpdrOn());
        }
        // UI → Model
        connect(m_procBtn, &QPushButton::toggled, this, [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.setCpdrOn(on);
        });
        // Model → UI
        connect(&tx, &TransmitModel::cpdrOnChanged, this, [this](bool on) {
            m_updatingFromModel = true;
            {
                QSignalBlocker b(m_procBtn);
                m_procBtn->setChecked(on);
            }
            m_updatingFromModel = false;
        });
    }

    if (m_procSlider) {
        {
            QSignalBlocker b(m_procSlider);
            m_procSlider->setValue(tx.cpdrLevelDb());
        }
        if (m_procValueLabel) {
            m_procValueLabel->setText(QStringLiteral("%1 dB").arg(tx.cpdrLevelDb()));
        }
        // UI → Model + value-label refresh.
        connect(m_procSlider, &QSlider::valueChanged, this, [this, &tx](int v) {
            if (m_procValueLabel) {
                m_procValueLabel->setText(QStringLiteral("%1 dB").arg(v));
            }
            if (m_updatingFromModel) { return; }
            tx.setCpdrLevelDb(v);
        });
        // Model → UI
        connect(&tx, &TransmitModel::cpdrLevelDbChanged, this, [this](int dB) {
            m_updatingFromModel = true;
            {
                QSignalBlocker b(m_procSlider);
                m_procSlider->setValue(dB);
            }
            if (m_procValueLabel) {
                m_procValueLabel->setText(QStringLiteral("%1 dB").arg(dB));
            }
            m_updatingFromModel = false;
        });
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Phase 3M-3a-iii Task 15: DEXP (#11) row wiring.
    //
    // (VOX wiring relocated to TxApplet 2026-05-04 — bench polish feedback.
    // See TxApplet::wireControls section 4b for the new home.)
    //
    // Bidirectional bind for [DEXP ON] toggle, plus the FAITHFUL Thetis
    // decorative-slider quirk for the DEXP threshold (preserved verbatim
    // — see inline comment on m_dexpSlider connect()).  100 ms timer at
    // the end of this block drives the DEXP peak meter from TxChannel.
    // ─────────────────────────────────────────────────────────────────────────

    // ── #11 DEXP [ON] toggle ↔ TransmitModel::dexpEnabled ────────────────────
    if (m_dexpBtn) {
        {
            QSignalBlocker b(m_dexpBtn);
            m_dexpBtn->setChecked(tx.dexpEnabled());
        }
        // UI → Model
        connect(m_dexpBtn, &QPushButton::toggled, this, [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.setDexpEnabled(on);
        });
        // Model → UI
        connect(&tx, &TransmitModel::dexpEnabledChanged, this, [this](bool on) {
            m_updatingFromModel = true;
            {
                QSignalBlocker b(m_dexpBtn);
                m_dexpBtn->setChecked(on);
            }
            m_updatingFromModel = false;
        });
    }

    // ── #11 DEXP threshold slider — DECORATIVE-ONLY (Thetis quirk) ───────────
    //
    // FAITHFUL Thetis quirk per console.cs:28962-28970 [v2.10.3.13].  The
    // stock Thetis ptbNoiseGate scroll handler reads (verbatim):
    //
    //   private void ptbNoiseGate_Scroll(object sender, System.EventArgs e)
    //   {
    //       lblNoiseGateVal.Text = ptbNoiseGate.Value.ToString();
    //
    //       if (sender.GetType() == typeof(PrettyTrackBar))
    //       {
    //           ptbNoiseGate.Focus();
    //       }
    //   }
    //
    // The slider value is NEVER pushed to WDSP.  It is only read by
    // picNoiseGate_Paint (console.cs:28972-28981 [v2.10.3.13]) to draw the
    // threshold marker line on the live mic peak meter:
    //
    //   int noise_x = (int)(((float)ptbNoiseGate.Value + 160.0)
    //                       * (picNoiseGate.Width - 1) / 160.0);
    //
    // We replicate that exact behavior — the slider value updates
    // m_dexpThresholdMarkerDb (UI-only) and is read by DexpPeakMeter to
    // draw the marker line.  Do NOT wire this slider to
    // TxChannel::setDexpAttackThreshold or any WDSP setter.
    if (m_dexpSlider) {
        m_dexpThresholdMarkerDb = static_cast<double>(m_dexpSlider->value());
        if (m_dexpPeakMeter) {
            // Map -160..0 dB → 0..1 (Thetis console.cs:28974 scaling).
            m_dexpPeakMeter->setThresholdMarker(
                (m_dexpThresholdMarkerDb + 160.0) / 160.0);
        }
        if (m_dexpLabel) {
            m_dexpLabel->setText(QStringLiteral("%1 dB")
                .arg(static_cast<int>(m_dexpThresholdMarkerDb)));
        }
        connect(m_dexpSlider, &QSlider::valueChanged, this, [this](int dB) {
            m_dexpThresholdMarkerDb = static_cast<double>(dB);
            if (m_dexpPeakMeter) {
                // Map -160..0 dB → 0..1 (Thetis console.cs:28974 scaling).
                m_dexpPeakMeter->setThresholdMarker((dB + 160.0) / 160.0);
            }
            if (m_dexpLabel) {
                m_dexpLabel->setText(QStringLiteral("%1 dB").arg(dB));
            }
        });
    }

    // ── Right-click on DEXP [ON] → emit openSetupRequested ───────────────────
    // MainWindow listens and jumps the SetupDialog to the "DEXP/VOX" leaf
    // (DexpVoxPage from Task 14).  Mirrors the SpeechProcessorPage cross-link
    // pattern in TransmitSetupPages.h:201.  (VOX-button right-click moved
    // to TxApplet 2026-05-04 with the rest of the VOX surface.)
    if (m_dexpBtn) {
        m_dexpBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_dexpBtn, &QPushButton::customContextMenuRequested, this,
                [this](const QPoint&) {
            emit openSetupRequested(QStringLiteral("Transmit"),
                                    QStringLiteral("DEXP/VOX"));
        });
    }

    // ── 100 ms timer driving the DEXP peak meter strip ───────────────────────
    // Cadence matches Thetis UpdateNoiseGate Task.Delay(100) at
    // console.cs:25347 [v2.10.3.13].  Stops automatically when the applet
    // is destroyed (parented to `this`).
    // (VOX peak-meter polling lives on TxApplet now — 2026-05-04 bench polish.)
    m_dexpMeterTimer = new QTimer(this);
    m_dexpMeterTimer->setInterval(100);
    connect(m_dexpMeterTimer, &QTimer::timeout,
            this, &PhoneCwApplet::pollDexpMeters);
    m_dexpMeterTimer->start();

    // ── #1 Mic level gauge ────────────────────────────────────────────────────
    // 50 ms timer (20 fps) — same polling cadence as VAX/HGauge meter precedent.
    // Reads AudioEngine::pcMicInputLevel() (linear 0..1, thread-safe atomic) +
    // applies the TransmitModel mic-gain (dB) so the gauge reflects the
    // post-gain level the TX path will see. This gives users immediate
    // visual feedback when adjusting the Mic Gain slider — keep peaks
    // around -3 dBFS (yellow zone) for clean SSB.
    //
    // Returns 0.0f raw when m_txInputBus is null (mic not configured / RX mode).
    m_micLevelTimer = new QTimer(this);
    m_micLevelTimer->setInterval(50);
    connect(m_micLevelTimer, &QTimer::timeout, this, [this]() {
        if (!m_model || !m_levelGauge) { return; }

        AudioEngine* ae = m_model->audioEngine();
        if (!ae) { return; }

        const float rawLinear = ae->pcMicInputLevel();

        // Apply mic-gain dB → linear scaling so the gauge tracks the slider.
        const int gainDb = m_model->transmitModel().micGainDb();
        const double gainLinear = std::pow(10.0, static_cast<double>(gainDb) / 20.0);
        const double scaled = static_cast<double>(rawLinear) * gainLinear;

        // Guard: NaN/Inf and very small values (≤1e-6) → floor at -60 dBFS.
        double dB = -60.0;
        if (std::isfinite(scaled) && scaled > 1e-6) {
            dB = 20.0 * std::log10(scaled);
        }

        // Clamp to gauge range [-40, +10].
        m_levelGauge->setValue(qBound(-40.0, dB, 10.0));
    });
    m_micLevelTimer->start();
}

// ── syncFromModel ─────────────────────────────────────────────────────────────

void PhoneCwApplet::syncFromModel()
{
    if (!m_model) { return; }

    // Mic gain slider + label (Phase 3M-1b)
    if (m_micLevelSlider) {
        TransmitModel& tx = m_model->transmitModel();
        const BoardCapabilities& caps = m_model->boardCapabilities();

        m_updatingFromModel = true;
        QSignalBlocker b(m_micLevelSlider);
        m_micLevelSlider->setRange(caps.micGainMinDb, caps.micGainMaxDb);
        m_micLevelSlider->setValue(tx.micGainDb());
        m_micLevelLabel->setText(QStringLiteral("%1 dB").arg(tx.micGainDb()));
        m_micLevelSlider->setEnabled(tx.micMute());
        m_updatingFromModel = false;
    }

    // PROC button + slider (Phase 3M-3a-ii post-bench cleanup) — bidirectional
    // sync to TransmitModel::cpdrOn / cpdrLevelDb.
    if (m_procBtn) {
        TransmitModel& tx = m_model->transmitModel();
        m_updatingFromModel = true;
        QSignalBlocker b(m_procBtn);
        m_procBtn->setChecked(tx.cpdrOn());
        m_updatingFromModel = false;
    }
    if (m_procSlider) {
        TransmitModel& tx = m_model->transmitModel();
        m_updatingFromModel = true;
        {
            QSignalBlocker b(m_procSlider);
            m_procSlider->setValue(tx.cpdrLevelDb());
        }
        if (m_procValueLabel) {
            m_procValueLabel->setText(QStringLiteral("%1 dB").arg(tx.cpdrLevelDb()));
        }
        m_updatingFromModel = false;
    }

    // (VOX state sync relocated to TxApplet 2026-05-04 with the rest of the
    //  VOX surface — bench polish feedback.  See TxApplet::syncFromModel.)

    // DEXP [ON] toggle (Phase 3M-3a-iii Task 15) — bidirectional sync to
    // TransmitModel::dexpEnabled.  m_dexpSlider is decorative-only (not
    // bound to TM) and so does not participate in syncFromModel; its
    // current value is the source of truth for the meter marker.
    if (m_dexpBtn) {
        TransmitModel& tx = m_model->transmitModel();
        m_updatingFromModel = true;
        QSignalBlocker b(m_dexpBtn);
        m_dexpBtn->setChecked(tx.dexpEnabled());
        m_updatingFromModel = false;
    }

    // Other controls wired in Phase 3I-1 (Phone/FM) / Phase 3I-2 (CW)
}

// ── pollDexpMeters — Phase 3M-3a-iii Task 15 ─────────────────────────────────
// 100 ms tick that drives the DEXP peak-meter strip on the Phone tab.
// (VOX peak-meter polling moved to TxApplet 2026-05-04 with the rest of
// the VOX surface.)
//
//   DEXP peak (raw negative dB from TxChannel::getTxMicMeterDb()):
//     • Add Thetis +3 dB display offset (console.cs:25354 [v2.10.3.13]:
//       `noise_gate_data = num + 3.0f`).
//     • Map -160..0 dB → 0..1 normalized.  Matches Thetis console.cs:28974
//       [v2.10.3.13]: `signal_x = (noise_gate_data + 160) * width / 160`.
//     • Threshold marker is the UI-only m_dexpThresholdMarkerDb, refreshed
//       eagerly by the slider valueChanged handler (no redraw needed here).
void PhoneCwApplet::pollDexpMeters()
{
    if (!m_model) { return; }
    TxChannel* ch = m_model->txChannel();
    if (!ch) { return; }

    // DEXP peak: dB → 0..1 over -160..0 range, matching Thetis
    // console.cs:28974 [v2.10.3.13]:
    //   signal_x = (noise_gate_data + 160) * width / 160
    //
    // Gate on (MOX OR voxEnabled).  Thetis gates `UpdateNoiseGate` strictly
    // on `_mox` (console.cs:25351 [v2.10.3.13]), so the noise-gate strip is
    // dead in RX.  NereusSDR extends the gate to also include voxEnabled
    // because Task 18's continuous-pump mode keeps real mic data flowing
    // through the WDSP TX pipeline whenever VOX is engaged - so showing
    // the live envelope is honest about what the DSP is actually doing.
    // In pure RX (no MOX, no VOX), match Thetis: hold the meter at zero.
    // Threshold marker is unconditional (NereusSDR-spin) so users can
    // pre-position it before keying.  Bench feedback 2026-05-04.
    const bool moxOn       = m_model->moxController()
                                 ? m_model->moxController()->isMox()
                                 : false;
    const bool voxEnabled  = m_model->transmitModel().voxEnabled();
    const bool meterLive   = moxOn || voxEnabled;

    if (m_dexpPeakMeter) {
        if (meterLive) {
            const double dexpDb     = ch->getTxMicMeterDb();
            // Thetis +3 dB display offset from console.cs:25354 [v2.10.3.13]:
            //   noise_gate_data = num + 3.0f;
            const double dexpDbAdj  = dexpDb + 3.0;
            const double dexpPeak01 = std::clamp((dexpDbAdj + 160.0) / 160.0,
                                                  0.0, 1.0);
            m_dexpPeakMeter->setSignalLevel(dexpPeak01);
        } else {
            // Pure RX (no MOX, no VOX): hold at zero (Thetis-faithful).
            m_dexpPeakMeter->setSignalLevel(0.0);
        }
    }
}

// ── showPage — switch stacked widget to the given page index ─────────────────

void PhoneCwApplet::showPage(int index)
{
    if (m_stack) {
        m_stack->setCurrentIndex(index);
    }
    // Sync the tab selector buttons so they reflect the active page
    if (m_tabGroup) {
        QAbstractButton* btn = m_tabGroup->button(index);
        if (btn && !btn->isChecked()) {
            btn->setChecked(true);
        }
    }
}

} // namespace NereusSDR
