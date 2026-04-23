// =================================================================
// src/gui/widgets/VfoWidget.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.resx (upstream has no top-of-file header — project-level LICENSE applies)
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/enums.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/radio.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/dsp.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/HPSDR/specHPSDR.cs, original licence from Thetis source is included below
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

//
// Upstream source 'Project Files/Source/Console/console.resx' has no top-of-file GPL header —
// project-level Thetis LICENSE applies.

//=================================================================
// display.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley (W5WC)
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
// Waterfall AGC Modifications Copyright (C) 2013 Phil Harman (VK6APH)
// Transitions to directX and continual modifications Copyright (C) 2020-2025 Richard Samphire (MW0LGE)
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

/*  enums.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2000-2025 Original authors
Copyright (C) 2020-2025 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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

//=================================================================
// radio.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Copyright (C) 2019-2026  Richard Samphire
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

/*  wdsp.cs

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013-2017 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at  

warren@wpratt.com

*/
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

/*
*
* Copyright (C) 2010-2018  Doug Wigley 
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "VfoWidget.h"
#include "DspParamPopup.h"
#include "VaxChannelSelector.h"
#include "gui/applets/NyiOverlay.h"
#include "core/BoardCapabilities.h"
#include "core/SkuUiProfile.h"
#include "core/HpsdrModel.h"
#include "gui/styles/PopupMenuStyle.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMenu>
#include <QFontDatabase>
#include <QSignalBlocker>

#include <cmath>
#include <algorithm>

namespace NereusSDR {

// Styles — matching dark theme from CONTRIBUTING.md
static const char* kFlatBtn =
    "QPushButton {"
    "  background: transparent; border: none;"
    "  padding: 1px 4px; font-size: 11px; font-weight: bold;"
    "}";

static const char* kTabBtn =
    "QPushButton {"
    "  background: transparent; border: none;"
    "  color: #6888a0; font-size: 12px; font-weight: bold;"
    "  padding: 2px 6px;"
    "}"
    "QPushButton:checked {"
    "  color: #00b4d8;"
    "  border-bottom: 2px solid #00b4d8;"
    "}";

// From AetherSDR VfoWidget.cpp:158-162 — kDspToggle style
static const char* kDspToggle =
    "QPushButton {"
    "  background: #1a2a3a; border: 1px solid #304050;"
    "  border-radius: 2px; color: #c8d8e8;"
    "  font-size: 13px; font-weight: bold;"
    "  padding: 2px 4px; min-width: 32px;"
    "}"
    "QPushButton:checked {"
    "  background: #1a6030; color: #ffffff;"
    "  border: 1px solid #20a040;"
    "}"
    "QPushButton:hover {"
    "  border: 1px solid #0090e0;"
    "}";

// From VfoStyles.h kModeBtn — blue-checked mode/filter button (AetherSDR pattern)
static const char* kModeBtn =
    "QPushButton {"
    "  background: #1a2a3a; border: 1px solid #304050; border-radius: 2px;"
    "  color: #c8d8e8; font-size: 13px; font-weight: bold; padding: 3px;"
    "}"
    "QPushButton:checked {"
    "  background: #0070c0; color: #ffffff; border: 1px solid #0090e0;"
    "}"
    "QPushButton:hover { border: 1px solid #0090e0; }";

// ---- Construction ----

VfoWidget::VfoWidget(QWidget* parent)
    : QWidget(parent)
{
    // Width fixed at kWidgetW per original AetherSDR pattern. Height grows
    // to fit content (no fixed height). DSP tab's 4-col grid uses ~60 px
    // buttons so 4×60=240 + margins fit within the 252 px flag width.
    setFixedWidth(kWidgetW);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setMouseTracking(true);

    buildUI();
}

// Floating buttons (m_closeBtn/m_lockBtn/m_recBtn/m_playBtn) are parented to
// parentWidget() (SpectrumWidget), i.e. they are SIBLINGS of VfoWidget in
// SpectrumWidget's children list. Qt's parent chain already owns them — do
// NOT explicitly delete here. Explicit deletes caused issue #113: Qt's
// QObjectPrivate::deleteChildren() walks SpectrumWidget's children in an
// order that freed a floating button before ~VfoWidget ran, leaving the
// button pointer dangling and SIGSEGV'ing the delete. Same fix as fd03d51;
// regressed by ff94942.
VfoWidget::~VfoWidget() = default;

void VfoWidget::buildUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    // From AetherSDR VfoWidget.cpp:237-238 — margins (6, 2, 6, 0)
    mainLayout->setContentsMargins(6, 2, 6, 0);
    mainLayout->setSpacing(2);

    buildHeaderRow();
    buildFrequencyRow();
    buildSmeterRow();
    buildTabBar();

    // Tab content stacked widget — HIDDEN by default (compact flag)
    // From AetherSDR VfoWidget.cpp:545 — m_tabStack->hide()
    m_tabStack = new QStackedWidget(this);
    buildAudioTab();
    buildDspTab();
    buildModeTab();

    buildXRitTab();

    // VAX tab — Phase 3O Sub-Phase 8 Task 8.2. Hosts VaxChannelSelector
    // (visible only when the VAX tab is active, same as every other mode tab).
    auto* vaxTabWidget = new QWidget;
    auto* vaxTabLayout = new QHBoxLayout(vaxTabWidget);
    vaxTabLayout->setContentsMargins(10, 4, 10, 4);
    vaxTabLayout->setSpacing(3);
    auto* vaxTabLbl = new QLabel(QStringLiteral("VAX"), vaxTabWidget);
    vaxTabLbl->setStyleSheet(QStringLiteral("color:#8090a0;font-size:10px;"));
    vaxTabLayout->addWidget(vaxTabLbl);
    m_vaxSelector = new VaxChannelSelector(vaxTabWidget);
    vaxTabLayout->addWidget(m_vaxSelector);
    vaxTabLayout->addStretch(1);
    m_tabStack->addWidget(vaxTabWidget);

    mainLayout->addWidget(m_tabStack);
    m_tabStack->hide();  // Hidden by default — click tab to expand
    m_activeTab = -1;    // No tab active initially

    // Per user directive 2026-04-23: flag height should shrink to fit the
    // ACTIVE tab only (not the tallest tab). QStackedWidget defaults to
    // sizing by the tallest child; we override by marking hidden pages as
    // QSizePolicy::Ignored so the stack reports the size of the active page.
    // The tab-switcher in buildTabBar calls adjustStackSize() to re-apply.
    for (int i = 0; i < m_tabStack->count(); ++i) {
        m_tabStack->widget(i)->setSizePolicy(
            QSizePolicy::Ignored, QSizePolicy::Ignored);
    }

    setLayout(mainLayout);
    adjustSize();

    // Floating buttons are children of our PARENT (SpectrumWidget)
    // so they render outside the VFO flag bounds. Deferred until
    // first updatePosition() when parentWidget() is available.
}

void VfoWidget::buildHeaderRow()
{
    auto* hdr = new QHBoxLayout;
    hdr->setSpacing(2);
    hdr->setContentsMargins(0, 0, 0, 0);

    // RX antenna button (blue)
    m_rxAntBtn = new QPushButton(QStringLiteral("ANT1"), this);
    m_rxAntBtn->setObjectName(QStringLiteral("m_rxAntBtn"));
    m_rxAntBtn->setStyleSheet(QString(kFlatBtn) +
        QStringLiteral("QPushButton { color: #4488ff; }"));
    m_rxAntBtn->setFixedHeight(18);
    // From Thetis console.resx:8277 — chkRxAnt.ToolTip
    m_rxAntBtn->setToolTip(QStringLiteral("Toggles receive antenna between RX and TX antennas for RX1"));
    connect(m_rxAntBtn, &QPushButton::clicked, this, [this]() {
        QMenu menu(this);
        menu.setStyleSheet(QString::fromLatin1(kPopupMenu));   // Phase 3P-I-a T15 — issue #98
        for (const QString& ant : m_antennaList) {
            QAction* act = menu.addAction(ant);
            act->setCheckable(true);
            act->setChecked(ant == m_rxAntBtn->text());
        }
        QAction* sel = menu.exec(m_rxAntBtn->mapToGlobal(
            QPoint(0, m_rxAntBtn->height())));
        if (sel) {
            m_rxAntBtn->setText(sel->text());
            emit rxAntennaChanged(sel->text());
        }
    });
    hdr->addWidget(m_rxAntBtn);

    // RX Bypass button (grey, BYPS) — Phase 3P-I-b T9.
    // Gated on caps.hasRxBypassRelay && SkuUiProfile.hasRxBypassUi.
    // Toggles AlexController::rxOutOnTx. From Thetis HPSDR/Alex.cs:61
    // "public static bool RxOutOnTx = false;" [v2.10.3.13 @501e3f5].
    m_rxBypassBtn = new QPushButton(QStringLiteral("BYPS"), this);
    m_rxBypassBtn->setObjectName(QStringLiteral("m_rxBypassBtn"));
    m_rxBypassBtn->setCheckable(true);
    m_rxBypassBtn->setStyleSheet(QString(kFlatBtn) +
        QStringLiteral("QPushButton { color: #888888; }"
                       "QPushButton:checked { color: #ffcc44; background: #2a2a1a; }"));
    m_rxBypassBtn->setFixedHeight(18);
    m_rxBypassBtn->setToolTip(QStringLiteral(
        "RX Bypass on TX — routes RX path through bypass relay during transmit. "
        "Maps to Thetis chkRxOutOnTx (Alex.cs:61)."));
    m_rxBypassBtn->setVisible(false);  // hidden until setBoardCapabilities + setHpsdrSku confirm gates
    connect(m_rxBypassBtn, &QPushButton::toggled, this, [this](bool on) {
        if (m_updatingFromModel) { return; }
        emit rxBypassToggled(on);
    });
    hdr->addWidget(m_rxBypassBtn);

    // TX antenna button (red)
    m_txAntBtn = new QPushButton(QStringLiteral("ANT1"), this);
    m_txAntBtn->setObjectName(QStringLiteral("m_txAntBtn"));
    m_txAntBtn->setStyleSheet(QString(kFlatBtn) +
        QStringLiteral("QPushButton { color: #ff4444; }"));
    m_txAntBtn->setFixedHeight(18);
    // NereusSDR native — no single Thetis TX-antenna tooltip (TX ant is configured
    // via Alex board setup in Setup dialog, not via a main-window toggle)
    m_txAntBtn->setToolTip(QStringLiteral("Select TX antenna"));
    connect(m_txAntBtn, &QPushButton::clicked, this, [this]() {
        QMenu menu(this);
        menu.setStyleSheet(QString::fromLatin1(kPopupMenu));   // Phase 3P-I-a T15 — issue #98
        for (const QString& ant : m_antennaList) {
            QAction* act = menu.addAction(ant);
            act->setCheckable(true);
            act->setChecked(ant == m_txAntBtn->text());
        }
        QAction* sel = menu.exec(m_txAntBtn->mapToGlobal(
            QPoint(0, m_txAntBtn->height())));
        if (sel) {
            m_txAntBtn->setText(sel->text());
            emit txAntennaChanged(sel->text());
        }
    });
    hdr->addWidget(m_txAntBtn);

    // Filter width label (cyan)
    m_filterWidthLbl = new QLabel(QStringLiteral("2.9K"), this);
    m_filterWidthLbl->setStyleSheet(
        QStringLiteral("color: #00c8ff; font-size: 11px; font-weight: bold;"));
    m_filterWidthLbl->setFixedHeight(18);
    hdr->addWidget(m_filterWidthLbl);

    hdr->addStretch();

    // TX badge
    m_txBadge = new QPushButton(QStringLiteral("TX"), this);
    m_txBadge->setFixedSize(28, 18);
    m_txBadge->setCheckable(true);
    m_txBadge->setStyleSheet(
        QStringLiteral("QPushButton { background: #1a2a3a; border: 1px solid #304050;"
                        "border-radius: 3px; color: #6888a0; font-size: 10px; font-weight: bold; }"
                        "QPushButton:checked { background: #6a3030; border-color: #ff4444; color: #ff8080; }"));
    // NereusSDR native — Thetis has no per-slice TX badge (it uses chkMOX for TX state)
    m_txBadge->setToolTip(QStringLiteral("Indicates this slice is the TX slice"));
    hdr->addWidget(m_txBadge);

    // Split badge — hidden in Stage 1; wired in Stage 2 when split semantics land
    m_splitBadge = new QLabel(QStringLiteral("SPLIT"), this);
    m_splitBadge->setFixedSize(36, 18);
    m_splitBadge->setAlignment(Qt::AlignCenter);
    m_splitBadge->setStyleSheet(
        QStringLiteral("background: #1a2a3a; border: 1px solid #304050;"
                        "border-radius: 3px; color: #6888a0; font-size: 10px; font-weight: bold;"));
    m_splitBadge->setVisible(false);
    hdr->addWidget(m_splitBadge);

    // Slice letter badge
    m_sliceBadge = new QLabel(QStringLiteral("A"), this);
    m_sliceBadge->setFixedSize(18, 18);
    m_sliceBadge->setAlignment(Qt::AlignCenter);
    m_sliceBadge->setStyleSheet(
        QStringLiteral("background: #0070c0; color: white; font-size: 11px;"
                        "font-weight: bold; border-radius: 3px;"));
    hdr->addWidget(m_sliceBadge);

    static_cast<QVBoxLayout*>(layout())->addLayout(hdr);
}

void VfoWidget::buildFrequencyRow()
{
    m_freqStack = new QStackedWidget(this);
    m_freqStack->setFixedHeight(30);

    // Display label
    m_freqLabel = new QLabel(this);
    m_freqLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_freqLabel->setStyleSheet(
        QStringLiteral("color: #00e5ff; font-size: 24px; font-weight: bold;"
                        "font-family: 'Consolas', 'Menlo', monospace;"
                        "background: transparent;"
                        "border: 1px solid rgba(255,255,255,50);"
                        "border-radius: 3px; padding: 0 4px;"));
    updateFreqLabel();
    m_freqStack->addWidget(m_freqLabel);

    // Edit field
    m_freqEdit = new QLineEdit(this);
    m_freqEdit->setAlignment(Qt::AlignRight);
    m_freqEdit->setStyleSheet(
        QStringLiteral("color: #00e5ff; font-size: 20px; font-weight: bold;"
                        "font-family: 'Consolas', 'Menlo', monospace;"
                        "background: #0a0a18; border: 1px solid #00b4d8;"
                        "border-radius: 3px; padding: 0 4px;"));
    connect(m_freqEdit, &QLineEdit::returnPressed, this, [this]() {
        const double hz = parseUserFrequency(m_freqEdit->text());
        if (hz > 0.0) {
            const double clamped = std::clamp(hz, 100000.0, 61440000.0);
            m_frequency = clamped;
            updateFreqLabel();
            emit frequencyChanged(clamped);
        }
        m_freqStack->setCurrentIndex(0);
    });
    connect(m_freqEdit, &QLineEdit::editingFinished, this, [this]() {
        m_freqStack->setCurrentIndex(0);
    });
    m_freqStack->addWidget(m_freqEdit);

    // Double-click on label → edit
    m_freqLabel->installEventFilter(this);

    static_cast<QVBoxLayout*>(layout())->addWidget(m_freqStack);
}

void VfoWidget::buildSmeterRow()
{
    m_levelBar = new VfoLevelBar(this);
    m_levelBar->setValue(float(m_smeterDbm));  // seed with cached value (default -127)
    static_cast<QVBoxLayout*>(layout())->addWidget(m_levelBar);
}

void VfoWidget::buildTabBar()
{
    auto* tabLayout = new QHBoxLayout;
    tabLayout->setSpacing(0);
    tabLayout->setContentsMargins(0, 0, 0, 0);

    // Tab labels — from AetherSDR VfoWidget.cpp:522
    // [🔊] | DSP | USB | X/RIT | VAX
    QStringList tabLabels = {
        QString::fromUtf8("\xF0\x9F\x94\x8A"),  // 🔊 speaker
        QStringLiteral("DSP"),
        SliceModel::modeName(m_currentMode),
        QStringLiteral("X/RIT"),
        QStringLiteral("VAX")
    };

    // NereusSDR native — Thetis has a fixed single-panel layout with all controls
    // visible simultaneously. The tabbed sub-panel is a NereusSDR UX pattern.
    static const char* kTabTooltips[] = {
        "Show/hide audio controls (AF gain, AGC, pan, mute, squelch)",
        "Show/hide DSP controls (NB, NR, ANF, SNB, APF)",
        "Show/hide mode and filter controls",
        "Show/hide RIT/XIT and frequency-lock controls",
        "Show/hide VAX audio routing controls"
    };

    for (int i = 0; i < tabLabels.size(); ++i) {
        // Add separator before each tab except the first
        // From AetherSDR VfoWidget.cpp:523-530
        if (i > 0) {
            auto* sep = new QLabel(QStringLiteral("|"), this);
            sep->setStyleSheet(QStringLiteral(
                "QLabel { background: transparent; border: none;"
                "color: rgba(255,255,255,80); font-size: 13px; padding: 0; }"));
            sep->setFixedWidth(6);
            tabLayout->addWidget(sep);
        }

        auto* btn = new QPushButton(tabLabels[i], this);
        btn->setCheckable(true);
        btn->setStyleSheet(kTabBtn);
        btn->setFixedHeight(24);  // 24px from AetherSDR
        btn->setToolTip(QString::fromLatin1(kTabTooltips[i]));
        connect(btn, &QPushButton::clicked, this, [this, i]() {
            if (m_activeTab == i) {
                // Toggle: clicking active tab hides content
                m_tabStack->hide();
                m_activeTab = -1;
                for (auto* b : m_tabButtons) { b->setChecked(false); }
            } else {
                m_activeTab = i;
                // Mark all pages Ignored except the active one, so the
                // QStackedWidget sizes to the ACTIVE page only (not to
                // the tallest page). Per user directive 2026-04-23.
                for (int j = 0; j < m_tabStack->count(); ++j) {
                    m_tabStack->widget(j)->setSizePolicy(
                        j == i ? QSizePolicy::Preferred : QSizePolicy::Ignored,
                        j == i ? QSizePolicy::Preferred : QSizePolicy::Ignored);
                }
                m_tabStack->setCurrentIndex(i);
                m_tabStack->show();
                for (int j = 0; j < m_tabButtons.size(); ++j) {
                    m_tabButtons[j]->setChecked(j == i);
                }
            }
            adjustSize();
            // Notify parent to reposition
            if (parentWidget()) {
                parentWidget()->update();
            }
        });
        tabLayout->addWidget(btn, 1);  // stretch equally
        m_tabButtons.append(btn);
    }

    static_cast<QVBoxLayout*>(layout())->addLayout(tabLayout);
}

void VfoWidget::buildAudioTab()
{
    auto* audioWidget = new QWidget;
    auto* audioLayout = new QVBoxLayout(audioWidget);
    audioLayout->setContentsMargins(4, 4, 4, 4);
    audioLayout->setSpacing(4);

    // 1. AF Gain slider (preserved exactly as-is — already live-wired)
    {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel(QStringLiteral("AF"), audioWidget);
        label->setStyleSheet(QStringLiteral("color: #8899aa; font-size: 11px;"));
        label->setFixedWidth(24);
        row->addWidget(label);

        m_afGainSlider = new QSlider(Qt::Horizontal, audioWidget);
        m_afGainSlider->setRange(0, 100);
        m_afGainSlider->setValue(50);
        m_afGainSlider->setStyleSheet(
            QStringLiteral("QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
                            "QSlider::handle:horizontal { background: #00b4d8; width: 12px; margin: -3px 0; border-radius: 6px; }"));
        // From Thetis console.resx:8433 — ptbAF.ToolTip
        m_afGainSlider->setToolTip(QStringLiteral("AF Gain - Monitor Volume for RX/TX"));
        row->addWidget(m_afGainSlider);

        m_afGainLabel = new QLabel(QStringLiteral("50"), audioWidget);
        m_afGainLabel->setStyleSheet(QStringLiteral("color: #c8d8e8; font-size: 11px;"));
        m_afGainLabel->setFixedWidth(24);
        m_afGainLabel->setAlignment(Qt::AlignRight);
        row->addWidget(m_afGainLabel);

        connect(m_afGainSlider, &QSlider::valueChanged, this, [this](int val) {
            m_afGainLabel->setText(QString::number(val));
            if (!m_updatingFromModel) {
                emit afGainChanged(val);
            }
        });
        audioLayout->addLayout(row);
    }

    // 2. AGC 5-button row — replaces m_agcCmb (live-wired, no NYI badge)
    {
        static const char* kAgcLabels[] = { "Off", "Long", "Slow", "Med", "Fast" };
        // From Thetis console.resx:4554 (comboAGC.ToolTip) + console.cs:27987-28041
        // Thetis sets dynamic tooltip per AGC mode change; we use static variants.
        static const char* kAgcTooltips[] = {
            "Automatic Gain Control Mode Setting:\nFixed - Set gain with AGC-T control",
            "Automatic Gain Control Mode Setting:\nLong (Attack 2ms, Hang 2000ms, Decay 2000ms)",
            "Automatic Gain Control Mode Setting:\nSlow (Attack 2ms, Hang 1000ms, Decay 500ms)",
            "Automatic Gain Control Mode Setting:\nMedium (Attack 2ms, Hang OFF, Decay 250ms)",
            "Automatic Gain Control Mode Setting:\nFast (Attack 2ms, Hang OFF, Decay 50ms)"
        };
        auto* row = new QHBoxLayout;
        row->setSpacing(2);
        row->setContentsMargins(0, 0, 0, 0);
        for (int i = 0; i < 5; ++i) {
            m_agcBtns[i] = new QPushButton(
                QString::fromLatin1(kAgcLabels[i]), audioWidget);
            m_agcBtns[i]->setCheckable(true);
            m_agcBtns[i]->setStyleSheet(kDspToggle);
            m_agcBtns[i]->setToolTip(QString::fromLatin1(kAgcTooltips[i]));
            row->addWidget(m_agcBtns[i]);
        }
        // Default: Med (index 3) — matches AGCMode::Med
        m_agcBtns[3]->setChecked(true);

        // Exclusive toggle: clicking one un-checks the others, emits agcModeChanged
        for (int i = 0; i < 5; ++i) {
            connect(m_agcBtns[i], &QPushButton::clicked, this, [this, i](bool checked) {
                if (!checked) {
                    // Don't allow unchecking; keep it checked
                    m_agcBtns[i]->setChecked(true);
                    return;
                }
                // Uncheck siblings
                for (int j = 0; j < 5; ++j) {
                    if (j != i) {
                        m_agcBtns[j]->setChecked(false);
                    }
                }
                if (!m_updatingFromModel) {
                    emit agcModeChanged(static_cast<AGCMode>(i));
                }
            });
        }
        audioLayout->addLayout(row);
    }

    // 3. Audio pan slider row (NYI)
    {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel(QStringLiteral("Pan"), audioWidget);
        label->setStyleSheet(QStringLiteral("color: #8899aa; font-size: 11px;"));
        label->setFixedWidth(24);
        row->addWidget(label);

        m_panSlider = new QSlider(Qt::Horizontal, audioWidget);
        m_panSlider->setRange(-100, 100);
        m_panSlider->setSingleStep(1);
        m_panSlider->setValue(0);
        m_panSlider->setStyleSheet(
            QStringLiteral("QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
                            "QSlider::handle:horizontal { background: #00b4d8; width: 12px; margin: -3px 0; border-radius: 6px; }"));
        m_panSlider->setToolTip(QStringLiteral("Audio pan: left/right stereo balance (−100 = full left, 0 = center, +100 = full right)\nFrom Thetis radio.cs:1386 — WDSP patchpanel.c:159"));
        row->addWidget(m_panSlider);

        m_panLabel = new QLabel(QStringLiteral("0"), audioWidget);
        m_panLabel->setStyleSheet(QStringLiteral("color: #c8d8e8; font-size: 11px;"));
        m_panLabel->setFixedWidth(24);
        m_panLabel->setAlignment(Qt::AlignRight);
        row->addWidget(m_panLabel);

        connect(m_panSlider, &QSlider::valueChanged, this, [this](int val) {
            m_panLabel->setText(QString::number(val));
            if (!m_updatingFromModel) {
                emit panChanged(val / 100.0);
            }
        });
        audioLayout->addLayout(row);
    }

    // 4. Mute + BIN row (NYI)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_muteBtn = new QPushButton(QStringLiteral("Mute"), audioWidget);
        m_muteBtn->setCheckable(true);
        m_muteBtn->setStyleSheet(kDspToggle);
        m_muteBtn->setToolTip(QStringLiteral("Mute RX audio output (SetRXAPanelRun)\nFrom Thetis dsp.cs:393 — WDSP patchpanel.c:126"));
        row->addWidget(m_muteBtn);

        m_binBtn = new QPushButton(QStringLiteral("BIN"), audioWidget);
        m_binBtn->setCheckable(true);
        m_binBtn->setStyleSheet(kDspToggle);
        m_binBtn->setToolTip(QStringLiteral("Binaural audio: I/Q channels separate for headphone stereo image (SetRXAPanelBinaural)\nFrom Thetis radio.cs:1145 — WDSP patchpanel.c:187"));
        row->addWidget(m_binBtn);

        row->addStretch();

        connect(m_muteBtn, &QPushButton::toggled, this, [this](bool on) {
            if (!m_updatingFromModel) {
                emit muteChanged(on);
            }
        });
        connect(m_binBtn, &QPushButton::toggled, this, [this](bool on) {
            if (!m_updatingFromModel) {
                emit binauralChanged(on);
            }
        });
        audioLayout->addLayout(row);
    }

    // 5. Squelch row — SQL toggle + SQL threshold slider (NYI)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_sqlBtn = new QPushButton(QStringLiteral("SQL"), audioWidget);
        m_sqlBtn->setCheckable(true);
        m_sqlBtn->setStyleSheet(kDspToggle);
        m_sqlBtn->setFixedWidth(40);
        // From Thetis console.resx:5631 — chkSquelch.ToolTip
        m_sqlBtn->setToolTip(QStringLiteral("Squelch Enable"));
        row->addWidget(m_sqlBtn);

        m_sqlSlider = new QSlider(Qt::Horizontal, audioWidget);
        m_sqlSlider->setRange(0, 100);
        m_sqlSlider->setSingleStep(1);
        m_sqlSlider->setValue(0);
        m_sqlSlider->setStyleSheet(
            QStringLiteral("QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
                            "QSlider::handle:horizontal { background: #00b4d8; width: 12px; margin: -3px 0; border-radius: 6px; }"));
        // NereusSDR native — Thetis ptbSquelch has no ToolTip entry in console.resx
        m_sqlSlider->setToolTip(QStringLiteral("Squelch threshold. SSB: 0–100 maps to 0.0–1.0 linear. AM: dB scale. FM: linear 0–1."));
        row->addWidget(m_sqlSlider);

        connect(m_sqlBtn, &QPushButton::toggled, this, [this](bool on) {
            if (!m_updatingFromModel) {
                emit squelchEnabledChanged(on);
            }
        });
        connect(m_sqlSlider, &QSlider::valueChanged, this, [this](int val) {
            if (!m_updatingFromModel) {
                emit squelchThreshChanged(val);
            }
        });
        audioLayout->addLayout(row);
        // Squelch button and slider are live-wired — no NYI badge
    }

    // 6. AGC threshold slider row
    // From Thetis Project Files/Source/Console/console.cs:45977 — agc_thresh_point, range -160..0
    {
        m_agcTContainer = new QWidget(audioWidget);
        auto* containerLayout = new QVBoxLayout(m_agcTContainer);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(1);

        // First row: AGC-T label + slider + dB value + AUTO badge
        auto* row = new QHBoxLayout;
        m_agcTLabelWidget = new QLabel(QStringLiteral("AGC-T"), m_agcTContainer);
        m_agcTLabelWidget->setStyleSheet(QStringLiteral("color: #8899aa; font-size: 11px;"));
        m_agcTLabelWidget->setFixedWidth(40);
        row->addWidget(m_agcTLabelWidget);

        m_agcTSlider = new QSlider(Qt::Horizontal, m_agcTContainer);
        m_agcTSlider->setRange(-160, 0);
        m_agcTSlider->setSingleStep(1);
        m_agcTSlider->setValue(-20);
        // Thetis: slider right = more gain. WDSP threshold is inverse
        // (lower threshold = more gain), so invert the visual direction.
        m_agcTSlider->setInvertedAppearance(true);
        m_agcTSlider->setStyleSheet(
            QStringLiteral("QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
                            "QSlider::handle:horizontal { background: #00b4d8; width: 12px; margin: -3px 0; border-radius: 6px; }"));
        // From Thetis console.resx:8397 — ptbRF.ToolTip (ptbRF is the AGC-T slider)
        m_agcTSlider->setToolTip(QStringLiteral("AGC Max Gain - Operates similarly to traditional RF Gain. Right click AUTO based on noise floor."));
        row->addWidget(m_agcTSlider);

        m_agcTLabel = new QLabel(QStringLiteral("-20"), m_agcTContainer);
        m_agcTLabel->setStyleSheet(QStringLiteral("color: #c8d8e8; font-size: 11px;"));
        m_agcTLabel->setFixedWidth(32);
        m_agcTLabel->setAlignment(Qt::AlignRight);
        row->addWidget(m_agcTLabel);

        m_agcAutoLabel = new QPushButton(QStringLiteral("AUTO"), m_agcTContainer);
        m_agcAutoLabel->setStyleSheet(
            QStringLiteral("QPushButton { background: #1a1a1a; border: 1px solid #445;"
                            "color: #556; font-size: 7px; padding: 0 3px; border-radius: 2px; }"
                            "QPushButton:hover { border-color: #adff2f; }"));
        m_agcAutoLabel->setFixedHeight(14);
        m_agcAutoLabel->setFixedWidth(30);
        m_agcAutoLabel->setCursor(Qt::PointingHandCursor);
        // From Thetis v2.10.3.13 setup.designer.cs:38679 — chkAutoAGCRX1.ToolTip
        m_agcAutoLabel->setToolTip(QStringLiteral("Automatically adjust AGC based on Noise Floor"));
        connect(m_agcAutoLabel, &QPushButton::clicked, this, [this]() {
            emit autoAgcToggled(!m_autoAgcActive);
        });
        row->addWidget(m_agcAutoLabel);

        containerLayout->addLayout(row);

        // Second row: info sub-line (hidden by default)
        m_agcInfoLabel = new QLabel(m_agcTContainer);
        m_agcInfoLabel->setStyleSheet(QStringLiteral("color: #33aa33; font-size: 7px; padding: 0 2px;"));
        m_agcInfoLabel->hide();
        containerLayout->addWidget(m_agcInfoLabel);

        connect(m_agcTSlider, &QSlider::valueChanged, this, [this](int val) {
            m_agcTLabel->setText(QString::number(val));
            if (!m_updatingFromModel) {
                emit agcThreshChanged(val);
            }
        });

        // Right-click on AGC-T slider → directly open Setup dialog
        m_agcTSlider->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_agcTSlider, &QWidget::customContextMenuRequested,
                this, [this](const QPoint& /*pos*/) {
            emit openSetupRequested();
        });

        audioLayout->addWidget(m_agcTContainer);
    }

    audioLayout->addStretch();
    m_tabStack->addWidget(audioWidget);
}

void VfoWidget::buildDspTab()
{
    auto* dspWidget = new QWidget;
    // Default sizing — mirrors AetherSDR VfoWidget.cpp:933 exactly.
    auto* dspLayout = new QVBoxLayout(dspWidget);
    dspLayout->setContentsMargins(2, 2, 2, 2);
    dspLayout->setSpacing(3);

    // Sub-epic C-1 USER-APPROVED layout: 3×4 DSP button grid.
    // NB first (preserves cycling), then NR mutual-exclusion group (NR1-4/DFNR/MNR),
    // then ANF + SNB as independent toggles on row 2. APF toggle moves to its
    // own slider row below the grid (consistent with its CW-only visibility gate).
    //
    //   Row 0: NB  | NR1  | NR2 | NR3
    //   Row 1: NR4 | DFNR | MNR | (empty)
    //   Row 2: ANF | SNB  |     |
    //
    // Overrides earlier horizontal-bank design per user directive 2026-04-23.

    auto makeToggle = [dspWidget](const QString& label) -> QPushButton* {
        auto* btn = new QPushButton(label, dspWidget);
        btn->setCheckable(true);
        btn->setStyleSheet(kDspToggle);
        return btn;
    };

    // QGridLayout added directly to dspLayout — matches AetherSDR
    // VfoWidget.cpp:938 (no subgrid widget wrapper). Grid fills the outer
    // VBoxLayout width, QPushButton default Expanding policy stretches
    // buttons to fill each cell.
    auto* dspGrid = new QGridLayout;
    dspGrid->setSpacing(3);

    // NB cycling button (row 0, col 0) — tri-state Off → NB → NB2 → Off.
    // Mirrors Thetis chkNB — label switches "NB"/"NB2"; checked = active.
    // From Thetis console.cs:43513-43560 [v2.10.3.13].
    m_nbButton = makeToggle(QStringLiteral("NB"));
    m_nbButton->setToolTip(tr(
        "Noise blanker — left-click cycles Off \u2192 NB \u2192 NB2 \u2192 Off,\n"
        "right-click opens Setup \u2192 DSP \u2192 NB/SNB.\n"
        "NB  (nob.c, Whitney): time-domain impulse blanker, suited to\n"
        "      sporadic crashes (powerline / ignition).\n"
        "NB2 (nobII.c): second-generation with hold/interpolate modes,\n"
        "      suited to denser impulse noise."));
    // Right-click → Setup page. Mirrors Thetis chkNB_MouseDown
    // (console.cs:44447 [v2.10.3.13]) which calls ShowSetupTab(NB_Tab).
    m_nbButton->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_nbButton, &QWidget::customContextMenuRequested,
            this, [this](const QPoint&) { emit openNbSetupRequested(); });
    dspGrid->addWidget(m_nbButton, 0, 0);

    // Row 0: NB (col 0) | NR1 | NR2 | NR3
    // All NR mutex group buttons share the same kDspToggle style as NB/ANF/SNB
    // so the entire 3×4 grid is visually uniform (Option A, user feedback 2026-04-23).
    m_nr1Btn = makeToggle(QStringLiteral("NR1"));
    m_nr2Btn = makeToggle(QStringLiteral("NR2"));
    m_nr3Btn = makeToggle(QStringLiteral("NR3"));
    m_nr1Btn->setContextMenuPolicy(Qt::CustomContextMenu);
    m_nr2Btn->setContextMenuPolicy(Qt::CustomContextMenu);
    m_nr3Btn->setContextMenuPolicy(Qt::CustomContextMenu);
    // Tooltips — Sub-epic C-1.
    // From Thetis console.resx:3879 — chkNR.ToolTip (closest analogue for NR1)
    m_nr1Btn->setToolTip(QStringLiteral("NR1: Adaptive LMS noise reduction — left-click activates, right-click adjusts knobs"));
    m_nr2Btn->setToolTip(QStringLiteral("NR2: EMNR (Enhanced Multiband Noise Reduction) — left-click activates, right-click adjusts knobs"));
    m_nr3Btn->setToolTip(QStringLiteral("NR3: RNNR (Recurrent Neural Net noise reduction) — left-click activates, right-click adjusts knobs"));
    // 4×2 layout (option B) — four cols consistently filled.
    //   Row 0: NB  | NR1  | NR2 | NR3
    //   Row 1: NR4 | DFNR | MNR | ANF
    //   Row 2: SNB (alone)
    dspGrid->addWidget(m_nr1Btn, 0, 1);
    dspGrid->addWidget(m_nr2Btn, 0, 2);
    dspGrid->addWidget(m_nr3Btn, 0, 3);

    // Row 1: NR4 | DFNR | MNR | (col 3 empty)
    m_nr4Btn  = makeToggle(QStringLiteral("NR4"));
    m_dfnrBtn = makeToggle(QStringLiteral("DFNR"));  // Full label — was "DFN" (truncated at 28px); now fits at uniform width
    m_bnrBtn  = makeToggle(QStringLiteral("BNR"));   // Hidden permanently (NVIDIA deferred)
    m_mnrBtn  = makeToggle(QStringLiteral("MNR"));
    m_nr4Btn->setContextMenuPolicy(Qt::CustomContextMenu);
    m_dfnrBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    m_bnrBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    m_mnrBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    m_nr4Btn->setToolTip(QStringLiteral("NR4: SBNR (Spectral Baseline NR) — left-click activates, right-click adjusts knobs"));
    m_dfnrBtn->setToolTip(QStringLiteral("DFNR: DeepFilter noise reduction — left-click activates, right-click adjusts knobs"));
    m_bnrBtn->setToolTip(QStringLiteral("BNR: NVIDIA noise reduction — left-click activates, right-click adjusts knobs"));
    m_mnrBtn->setToolTip(QStringLiteral("MNR: macOS noise reduction — left-click activates, right-click adjusts knobs"));
    dspGrid->addWidget(m_nr4Btn,  1, 0);
    dspGrid->addWidget(m_dfnrBtn, 1, 1);
    dspGrid->addWidget(m_mnrBtn,  1, 2);
    // col (1,3) intentionally empty

#ifndef HAVE_BNR
    m_bnrBtn->hide();  // Hidden permanently: NVIDIA BNR integration deferred.
    // BNR not added to grid — hidden and parented to dspWidget for lifecycle.
#endif
#ifndef HAVE_MNR
    m_mnrBtn->hide();  // Hidden on non-macOS platforms.
#endif

    // Row 2: ANF | SNB | (cols 2-3 empty)
    m_anfToggle = makeToggle(QStringLiteral("ANF"));
    // From Thetis console.resx:4062 — chkANF.ToolTip
    m_anfToggle->setToolTip(QStringLiteral("Automatic Notch Filter"));
    m_snbToggle = makeToggle(QStringLiteral("SNB"));
    // From Thetis console.resx:3927 — chkDSPNB2.ToolTip (labeled "SNB" in Thetis UI)
    m_snbToggle->setToolTip(tr(
        "Spectral Noise Blanker — left-click toggles, right-click opens\n"
        "Setup \u2192 DSP \u2192 NB/SNB. Runs independently of NB/NB2 and\n"
        "targets tonal/wideband statics that time-domain blankers can't\n"
        "see."));
    // Right-click → Setup page. Mirrors Thetis chkDSPNB2_MouseDown
    // (console.cs:44451 [v2.10.3.13]) which calls ShowSetupTab(NB_Tab).
    m_snbToggle->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_snbToggle, &QWidget::customContextMenuRequested,
            this, [this](const QPoint&) { emit openNbSetupRequested(); });
    dspGrid->addWidget(m_anfToggle, 1, 3);  // fills row 1 col 4
    dspGrid->addWidget(m_snbToggle, 2, 0);  // alone on row 2

    // Uniform size for all 9 grid buttons: 64×26 px.
    // User directive: natural button size (not cramped), zero gaps between
    // buttons so their 1px borders abut forming a continuous "board" around
    // the grid. SizePolicy::Fixed prevents cells from widening beyond button
    // size so the sub-grid hugs its content instead of stretching to the
    // flag width.
    for (auto* btn : {m_nbButton, m_nr1Btn, m_nr2Btn, m_nr3Btn,
                      m_nr4Btn, m_dfnrBtn, m_mnrBtn,
                      m_anfToggle, m_snbToggle}) {
        if (btn) {
            btn->setFixedHeight(26);
            btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }
    }

    dspLayout->addLayout(dspGrid);

    // APF toggle + tune slider row — below the 3×4 grid.
    // The toggle acts as the enable button; slider + Hz label are only visible
    // when APF is enabled AND mode is CW (gated by applyModeVisibility).
    m_apfToggle = makeToggle(QStringLiteral("APF"));
    m_apfToggle->setFixedSize(63, 26);
    m_apfToggle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    // From Thetis console.resx:348 — chkCWAPFEnabled.ToolTip
    m_apfToggle->setToolTip(QStringLiteral("Enables APF"));

    {
        auto* apfRow = new QHBoxLayout;
        apfRow->setSpacing(2);

        apfRow->addWidget(m_apfToggle);

        m_apfLabel = nullptr;  // No longer needed — toggle replaces label widget.

        m_apfTuneSlider = new QSlider(Qt::Horizontal, dspWidget);
        m_apfTuneSlider->setRange(-500, 500);
        m_apfTuneSlider->setSingleStep(1);
        m_apfTuneSlider->setValue(0);
        // From Thetis console.resx:303 — ptbCWAPFFreq.ToolTip
        m_apfTuneSlider->setToolTip(QStringLiteral("Sets the CW APF Frequency."));
        apfRow->addWidget(m_apfTuneSlider);

        m_apfTuneLabel = new QLabel(QStringLiteral("0 Hz"), dspWidget);
        m_apfTuneLabel->setStyleSheet(QStringLiteral("color: #8899aa; font-size: 11px;"));
        m_apfTuneLabel->setFixedWidth(44);
        m_apfTuneLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        apfRow->addWidget(m_apfTuneLabel);

        dspLayout->addLayout(apfRow);
    }

    // Mode containers — embedded, hidden by default (S1.9 wires visibility)
    m_fmContainer = new FmOptContainer(dspWidget);
    m_fmContainer->setVisible(false);
    dspLayout->addWidget(m_fmContainer);

    m_digContainer = new DigOffsetContainer(dspWidget);
    m_digContainer->setVisible(false);
    dspLayout->addWidget(m_digContainer);

    m_rttyContainer = new RttyMarkShiftContainer(dspWidget);
    m_rttyContainer->setVisible(false);
    dspLayout->addWidget(m_rttyContainer);

    // If m_slice was set before buildUI ran, forward it to the containers now.
    if (m_slice) {
        m_fmContainer->setSlice(m_slice.data());
        m_digContainer->setSlice(m_slice.data());
        m_rttyContainer->setSlice(m_slice.data());
    }

    // Signal wiring for the 6 toggles
    // NB button emits nbModeCycled() on click; MainWindow calls cycleNbMode()
    // and pushes the new mode back via setNbMode(). clicked() not toggled()
    // so we don't double-fire on programmatic setChecked() calls.
    connect(m_nbButton, &QPushButton::clicked, this, [this] {
        if (!m_updatingFromModel) { emit nbModeCycled(); }
    });
    // Sub-epic C-1: NR bank left-click = setActiveNr(slot) mutual exclusion.
    auto wireNrBtnToggle = [this](QPushButton* btn, NereusSDR::NrSlot slot) {
        connect(btn, &QPushButton::toggled, this, [this, slot](bool on) {
            if (m_updatingFromModel || !m_slice) {
                return;
            }
            m_slice->setActiveNr(on ? slot : NereusSDR::NrSlot::Off);
        });
    };
    wireNrBtnToggle(m_nr1Btn,  NereusSDR::NrSlot::NR1);
    wireNrBtnToggle(m_nr2Btn,  NereusSDR::NrSlot::NR2);
    wireNrBtnToggle(m_nr3Btn,  NereusSDR::NrSlot::NR3);
    wireNrBtnToggle(m_nr4Btn,  NereusSDR::NrSlot::NR4);
    wireNrBtnToggle(m_dfnrBtn, NereusSDR::NrSlot::DFNR);
    wireNrBtnToggle(m_bnrBtn,  NereusSDR::NrSlot::BNR);
    wireNrBtnToggle(m_mnrBtn,  NereusSDR::NrSlot::MNR);

    // Sub-epic C-1: NR bank right-click = DspParamPopup quick controls.
    connect(m_nr1Btn,  &QPushButton::customContextMenuRequested, this,
            [this](const QPoint& pos) { showNr1Popup(m_nr1Btn->mapToGlobal(pos)); });
    connect(m_nr2Btn,  &QPushButton::customContextMenuRequested, this,
            [this](const QPoint& pos) { showNr2Popup(m_nr2Btn->mapToGlobal(pos)); });
    connect(m_nr3Btn,  &QPushButton::customContextMenuRequested, this,
            [this](const QPoint& pos) { showNr3Popup(m_nr3Btn->mapToGlobal(pos)); });
    connect(m_nr4Btn,  &QPushButton::customContextMenuRequested, this,
            [this](const QPoint& pos) { showNr4Popup(m_nr4Btn->mapToGlobal(pos)); });
    connect(m_dfnrBtn, &QPushButton::customContextMenuRequested, this,
            [this](const QPoint& pos) { showDfnrPopup(m_dfnrBtn->mapToGlobal(pos)); });
    connect(m_bnrBtn,  &QPushButton::customContextMenuRequested, this,
            [this](const QPoint& pos) { showBnrPopup(m_bnrBtn->mapToGlobal(pos)); });
    connect(m_mnrBtn,  &QPushButton::customContextMenuRequested, this,
            [this](const QPoint& pos) { showMnrPopup(m_mnrBtn->mapToGlobal(pos)); });
    connect(m_anfToggle, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) { emit anfChanged(on); }
    });
    connect(m_snbToggle, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) { emit snbChanged(on); }
    });
    connect(m_apfToggle, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) { emit apfChanged(on); }
        // Re-evaluate APF slider visibility regardless of source (S1.9)
        applyModeVisibility(m_currentMode);
    });

    // APF tune slider — label updates always; emit only when user-driven
    connect(m_apfTuneSlider, &QSlider::valueChanged, this, [this](int hz) {
        m_apfTuneLabel->setText(QString::number(hz) + QStringLiteral(" Hz"));
        if (!m_updatingFromModel) { emit apfTuneHzChanged(hz); }
    });

    // NYI badges — NB1, NB2, NR, ANF, NR2, SNB, APF, FM/DIG/RTTY containers are
    // all live-wired (no badge). Remaining controls with badges are below.

    m_tabStack->addWidget(dspWidget);
}

void VfoWidget::buildModeTab()
{
    auto* modeWidget = new QWidget;
    auto* modeLayout = new QVBoxLayout(modeWidget);
    modeLayout->setContentsMargins(4, 4, 4, 4);
    modeLayout->setSpacing(4);

    // Mode combo row
    {
        auto* modeRow = new QHBoxLayout;
        modeRow->setSpacing(2);
        modeRow->setContentsMargins(0, 0, 0, 0);

        m_modeCmb = new QComboBox(modeWidget);
        // NereusSDR native — Thetis uses discrete radio buttons (radModeUSB, radModeLSB, ...)
        // rather than a combo box. No single Thetis control has an equivalent tooltip.
        m_modeCmb->setToolTip(QStringLiteral("Select demodulation mode"));
        // From Thetis enums.cs DSPMode — common modes
        m_modeCmb->addItems({
            QStringLiteral("LSB"), QStringLiteral("USB"),
            QStringLiteral("AM"), QStringLiteral("CWL"),
            QStringLiteral("CWU"), QStringLiteral("FM"),
            QStringLiteral("DIGU"), QStringLiteral("DIGL"),
            QStringLiteral("SAM")
        });
        m_modeCmb->setCurrentText(QStringLiteral("USB"));
        m_modeCmb->setStyleSheet(
            QStringLiteral("QComboBox { background: #1a2a3a; color: #c8d8e8;"
                            "border: 1px solid #304050; border-radius: 3px;"
                            "padding: 1px 4px; font-size: 11px; }"
                            "QComboBox::drop-down { border: none; }"
                            "QComboBox QAbstractItemView { background: #1a2a3a; color: #c8d8e8;"
                            "selection-background-color: #0070c0; }"));
        connect(m_modeCmb, &QComboBox::currentTextChanged,
                this, [this](const QString& text) {
            if (!m_updatingFromModel) {
                DSPMode mode = SliceModel::modeFromName(text);
                m_currentMode = mode;
                applyModeVisibility(mode);    // S1.9 — user-driven mode change
                rebuildFilterButtons(mode);
                // Update mode tab label
                if (m_tabButtons.size() > 2) {
                    m_tabButtons[2]->setText(text);
                }
                emit modeChanged(mode);
            }
        });
        modeRow->addWidget(m_modeCmb, 1);  // stretch — combo fills available space
        modeLayout->addLayout(modeRow);
    }

    // Filter preset buttons (dynamic per mode)
    m_filterBtnContainer = new QWidget(modeWidget);
    modeLayout->addWidget(m_filterBtnContainer);
    rebuildFilterButtons(DSPMode::USB);

    // RF Gain slider removed — AGC-T (Audio tab) controls the same WDSP
    // max_gain parameter. Revisit when spectrum-overlay AGC-T line lands.

    modeLayout->addStretch();
    m_tabStack->addWidget(modeWidget);
}

void VfoWidget::buildXRitTab()
{
    // NereusSDR native X/RIT tab — AetherSDR pattern, control surfaces are native
    // (per feedback_source_first_ui_vs_dsp.md). DSP state is all stubs from S1.6;
    // Stage 2 wires the WDSP/SliceModel calls and removes NYI badges.
    auto* ritWidget = new QWidget;
    auto* vbox = new QVBoxLayout(ritWidget);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(2);

    static const char* kZeroBtn =
        "QPushButton {"
        "  background: #1a2a3a; border: 1px solid #304050; border-radius: 2px;"
        "  color: #c8d8e8; font-size: 12px; font-weight: bold; padding: 1px;"
        "}"
        "QPushButton:hover { border: 1px solid #0090e0; }";

    // --- RIT row ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_ritBtn = new QPushButton(QStringLiteral("RIT"), ritWidget);
        m_ritBtn->setCheckable(true);
        m_ritBtn->setStyleSheet(kDspToggle);
        m_ritBtn->setFixedHeight(22);
        // From Thetis console.resx:4335 — chkRIT.ToolTip
        m_ritBtn->setToolTip(QStringLiteral("Receive Incremental Tuning - offset RX frequency by value below in Hz."));
        row->addWidget(m_ritBtn);

        m_ritLabel = new ScrollableLabel(ritWidget);
        m_ritLabel->setRange(-10000, 10000);
        m_ritLabel->setStep(m_stepHz);
        m_ritLabel->setValue(0);
        m_ritLabel->setFormat([](int v) {
            return QString::asprintf("%+d Hz", v);
        });
        row->addWidget(m_ritLabel, 1);

        m_ritZeroBtn = new QPushButton(QStringLiteral("0"), ritWidget);
        m_ritZeroBtn->setFixedWidth(20);
        m_ritZeroBtn->setFlat(true);
        m_ritZeroBtn->setStyleSheet(kZeroBtn);
        // From Thetis console.resx:4185 — btnRITReset.ToolTip
        m_ritZeroBtn->setToolTip(QStringLiteral("Clear RIT"));
        row->addWidget(m_ritZeroBtn);

        vbox->addLayout(row);
    }

    // --- XIT row ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_xitBtn = new QPushButton(QStringLiteral("XIT"), ritWidget);
        m_xitBtn->setCheckable(true);
        m_xitBtn->setStyleSheet(kDspToggle);
        m_xitBtn->setFixedHeight(22);
        // From Thetis console.resx:4416 — chkXIT.ToolTip
        // XIT stored in SliceModel for Phase 3M-1 TX use; client offset displayed now.
        m_xitBtn->setToolTip(QStringLiteral("Transmit Incremental Tuning - offset TX frequency by the value below in Hz."));
        row->addWidget(m_xitBtn);

        m_xitLabel = new ScrollableLabel(ritWidget);
        m_xitLabel->setRange(-10000, 10000);
        m_xitLabel->setStep(m_stepHz);
        m_xitLabel->setValue(0);
        m_xitLabel->setFormat([](int v) {
            return QString::asprintf("%+d Hz", v);
        });
        row->addWidget(m_xitLabel, 1);

        m_xitZeroBtn = new QPushButton(QStringLiteral("0"), ritWidget);
        m_xitZeroBtn->setFixedWidth(20);
        m_xitZeroBtn->setFlat(true);
        m_xitZeroBtn->setStyleSheet(kZeroBtn);
        // From Thetis console.resx:4224 — btnXITReset.ToolTip
        m_xitZeroBtn->setToolTip(QStringLiteral("Clear XIT"));
        row->addWidget(m_xitZeroBtn);

        vbox->addLayout(row);
    }

    // --- Bottom row: LOCK + STEP cycle ---
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        m_xritLockBtn = new QPushButton(QStringLiteral("LOCK"), ritWidget);
        m_xritLockBtn->setCheckable(true);
        m_xritLockBtn->setStyleSheet(kDspToggle);
        m_xritLockBtn->setFixedHeight(22);
        // From Thetis console.resx:5787 — chkVFOLock.ToolTip
        m_xritLockBtn->setToolTip(QStringLiteral("Keeps the VFO from changing while in the middle of a QSO."));
        row->addWidget(m_xritLockBtn);

        // Step cycle button — NOT NYI (wires to live SliceModel::setStepHz)
        m_stepCycleBtn = new QPushButton(
            QStringLiteral("%1 Hz").arg(m_stepHz), ritWidget);
        m_stepCycleBtn->setFlat(true);
        m_stepCycleBtn->setStyleSheet(
            QStringLiteral("QPushButton {"
                           "  background: #1a2a3a; border: 1px solid #304050; border-radius: 2px;"
                           "  color: #c8d8e8; font-size: 11px; font-weight: bold; padding: 2px 4px;"
                           "}"
                           "QPushButton:hover { border: 1px solid #0090e0; }"));
        m_stepCycleBtn->setFixedHeight(22);
        // NereusSDR native — Thetis has no equivalent step-cycle button
        // (Thetis uses wheel on the VFO display directly; step size is implicit)
        m_stepCycleBtn->setToolTip(QStringLiteral("Cycle tuning step size (click to advance to next step)"));
        row->addWidget(m_stepCycleBtn, 1);

        vbox->addLayout(row);
    }

    vbox->addStretch();

    // --- Signal wiring ---
    connect(m_ritBtn, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) {
            emit ritEnabledChanged(on);
        }
    });

    connect(m_ritLabel, &ScrollableLabel::valueChanged, this, [this](int hz) {
        if (!m_updatingFromModel) {
            emit ritHzChanged(hz);
        }
    });

    connect(m_ritZeroBtn, &QPushButton::clicked, this, [this]() {
        m_ritLabel->setValue(0);
        if (!m_updatingFromModel) {
            emit ritHzChanged(0);
        }
    });

    connect(m_xitBtn, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) {
            emit xitEnabledChanged(on);
        }
    });

    connect(m_xitLabel, &ScrollableLabel::valueChanged, this, [this](int hz) {
        if (!m_updatingFromModel) {
            emit xitHzChanged(hz);
        }
    });

    connect(m_xitZeroBtn, &QPushButton::clicked, this, [this]() {
        m_xitLabel->setValue(0);
        if (!m_updatingFromModel) {
            emit xitHzChanged(0);
        }
    });

    connect(m_xritLockBtn, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) {
            applyLockedState(on);
        }
    });

    connect(m_stepCycleBtn, &QPushButton::clicked, this, [this]() {
        emit stepCycleRequested();
    });

    // RIT controls are live — no NYI badge.
    // XIT stored for 3M-1 (TX phase); keep NYI badge with TX note.
    NyiOverlay::markNyi(m_xitBtn,      QStringLiteral("XIT — TX gated by Phase 3M-1"));
    NyiOverlay::markNyi(m_xitLabel,    QStringLiteral("XIT — TX gated by Phase 3M-1"));
    NyiOverlay::markNyi(m_xitZeroBtn,  QStringLiteral("XIT — TX gated by Phase 3M-1"));
    // LOCK is live in S2.9 — no NYI badge.

    m_tabStack->addWidget(ritWidget);
}

void VfoWidget::rebuildFilterButtons(DSPMode mode)
{
    // Remove old layout and buttons
    if (m_filterBtnContainer->layout()) {
        QLayoutItem* item;
        while ((item = m_filterBtnContainer->layout()->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete m_filterBtnContainer->layout();
    }

    auto* grid = new QHBoxLayout(m_filterBtnContainer);
    grid->setSpacing(2);
    grid->setContentsMargins(0, 0, 0, 0);

    // Per-mode filter presets — ported from Thetis console.cs:5180-5575
    // Showing a selection of useful widths for each mode family
    struct Preset { const char* label; int low; int high; };

    QVector<Preset> presets;
    switch (mode) {
    case DSPMode::LSB:
        // From Thetis console.cs:5191-5231 (LSB F1-F10)
        presets = {{"5.0K",-5100,-100}, {"3.8K",-3900,-100}, {"2.9K",-3000,-100},
                   {"2.7K",-2800,-100}, {"2.4K",-2500,-100}, {"1.8K",-1900,-100}};
        break;
    case DSPMode::USB:
        // From Thetis console.cs:5233-5273 (USB F1-F10)
        presets = {{"5.0K",100,5100}, {"3.8K",100,3900}, {"2.9K",100,3000},
                   {"2.7K",100,2800}, {"2.4K",100,2500}, {"1.8K",100,1900}};
        break;
    case DSPMode::CWL:
    case DSPMode::CWU: {
        // From Thetis console.cs:5359-5441 — centered on cw_pitch (600 Hz)
        int sign = (mode == DSPMode::CWL) ? -1 : 1;
        int p = 600;  // From Thetis display.cs:1023
        presets = {{"1.0K", sign*(p-500), sign*(p+500)},
                   {"500",  sign*(p-250), sign*(p+250)},
                   {"400",  sign*(p-200), sign*(p+200)},
                   {"250",  sign*(p-125), sign*(p+125)},
                   {"100",  sign*(p-50),  sign*(p+50)}};
        break;
    }
    case DSPMode::AM:
    case DSPMode::SAM:
        // From Thetis console.cs:5443-5525 (AM/SAM F1-F10)
        presets = {{"20K",-10000,10000}, {"10K",-5000,5000}, {"8.0K",-4000,4000},
                   {"6.0K",-3000,3000}, {"5.0K",-2500,2500}};
        break;
    case DSPMode::FM:
        presets = {{"16K",-8000,8000}, {"12K",-6000,6000}, {"8.0K",-4000,4000}};
        break;
    case DSPMode::DIGU: {
        // From Thetis console.cs:5317-5357, offset=1500
        int o = 1500;
        presets = {{"3.0K",o-1500,o+1500}, {"2.0K",o-1000,o+1000},
                   {"1.0K",o-500,o+500}, {"600",o-300,o+300}};
        break;
    }
    case DSPMode::DIGL: {
        // From Thetis console.cs:5275-5315, offset=2210
        int o = 2210;
        presets = {{"3.0K",-(o+1500),-(o-1500)}, {"2.0K",-(o+1000),-(o-1000)},
                   {"1.0K",-(o+500),-(o-500)}, {"600",-(o+300),-(o-300)}};
        break;
    }
    default:
        presets = {{"10K",-5000,5000}, {"6.0K",-3000,3000}};
        break;
    }

    auto [defLow, defHigh] = SliceModel::defaultFilterForMode(mode);

    for (const auto& p : presets) {
        auto* btn = new QPushButton(QString::fromLatin1(p.label), m_filterBtnContainer);
        btn->setCheckable(true);
        btn->setStyleSheet(kModeBtn);
        btn->setFixedHeight(26);
        int low = p.low;
        int high = p.high;
        btn->setProperty("filterLow", low);
        btn->setProperty("filterHigh", high);
        // Tooltip: show the filter edges so the user knows what they're selecting
        btn->setToolTip(QStringLiteral("Select filter preset: %1 Hz to %2 Hz")
            .arg(low).arg(high));
        // Check if this matches current filter
        if (low == defLow && high == defHigh) {
            btn->setChecked(true);
        }
        // Exclusive toggle: click selects this preset, emits filterChanged
        connect(btn, &QPushButton::clicked, this, [this, low, high, btn](bool checked) {
            if (!checked) {
                // Don't allow unchecking the active preset — keep it toggled on
                btn->setChecked(true);
                return;
            }
            // Uncheck all other filter buttons (exclusive group)
            for (auto* child : m_filterBtnContainer->findChildren<QPushButton*>()) {
                if (child != btn) {
                    child->setChecked(false);
                }
            }
            if (!m_updatingFromModel) {
                emit filterChanged(low, high);
            }
        });
        grid->addWidget(btn);
    }

    m_filterBtnContainer->setLayout(grid);
}

// ---- State setters (guarded) ----

void VfoWidget::setFrequency(double hz)
{
    m_updatingFromModel = true;
    m_frequency = hz;
    updateFreqLabel();
    m_updatingFromModel = false;
}

void VfoWidget::setMode(DSPMode mode)
{
    m_updatingFromModel = true;
    m_currentMode = mode;
    QString name = SliceModel::modeName(mode);
    m_modeCmb->setCurrentText(name);
    if (m_tabButtons.size() > 2) {
        m_tabButtons[2]->setText(name);
    }
    rebuildFilterButtons(mode);
    applyModeVisibility(mode);    // S1.9 — model-driven mode change
    m_updatingFromModel = false;
}

void VfoWidget::setFilter(int low, int high)
{
    m_updatingFromModel = true;
    m_filterWidthLbl->setText(formatFilterWidth(low, high));
    // Update checked state of filter buttons — match by stored property
    for (auto* btn : m_filterBtnContainer->findChildren<QPushButton*>()) {
        int bLow = btn->property("filterLow").toInt();
        int bHigh = btn->property("filterHigh").toInt();
        btn->setChecked(bLow == low && bHigh == high);
    }
    m_updatingFromModel = false;
}

void VfoWidget::setAgcMode(AGCMode mode)
{
    m_updatingFromModel = true;
    int idx = static_cast<int>(mode);
    for (int i = 0; i < 5; ++i) {
        if (m_agcBtns[i]) {
            m_agcBtns[i]->setChecked(i == idx);
        }
    }
    m_updatingFromModel = false;
}

void VfoWidget::setAfGain(int gain)
{
    m_updatingFromModel = true;
    m_afGainSlider->setValue(gain);
    m_afGainLabel->setText(QString::number(gain));
    m_updatingFromModel = false;
}

void VfoWidget::setRfGain(int)
{
    // RF Gain slider removed — AGC-T controls the same parameter.
}

void VfoWidget::setRxAntenna(const QString& ant)
{
    m_updatingFromModel = true;
    m_rxAntBtn->setText(ant);
    m_updatingFromModel = false;
}

void VfoWidget::setTxAntenna(const QString& ant)
{
    m_updatingFromModel = true;
    m_txAntBtn->setText(ant);
    m_updatingFromModel = false;
}

void VfoWidget::setStepHz(int hz)
{
    m_stepHz = hz;
    if (m_ritLabel) {
        m_ritLabel->setStep(hz);
    }
    if (m_xitLabel) {
        m_xitLabel->setStep(hz);
    }
    if (m_stepCycleBtn) {
        m_stepCycleBtn->setText(QStringLiteral("%1 Hz").arg(hz));
    }
}

void VfoWidget::setSliceIndex(int index)
{
    m_sliceIndex = index;
    static const QChar letters[] = {'A', 'B', 'C', 'D'};
    if (index >= 0 && index < 4) {
        m_sliceBadge->setText(QString(letters[index]));
        QColor c = sliceColor(index);
        m_sliceBadge->setStyleSheet(
            QStringLiteral("background: %1; color: white; font-size: 11px;"
                            "font-weight: bold; border-radius: 3px;").arg(c.name()));
    }
}

void VfoWidget::setTxSlice(bool isTx)
{
    m_txBadge->setChecked(isTx);
}

void VfoWidget::setAntennaList(const QStringList& ants)
{
    m_antennaList = ants;
}

void VfoWidget::setSmeter(double dbm)
{
    m_smeterDbm = dbm;
    if (m_levelBar) {
        m_levelBar->setValue(float(dbm));
    }
}

void VfoWidget::setRitEnabled(bool v)
{
    if (m_ritBtn && m_ritBtn->isChecked() != v) {
        m_updatingFromModel = true;
        m_ritBtn->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setRitHz(int hz)
{
    if (m_ritLabel && m_ritLabel->value() != hz) {
        m_updatingFromModel = true;
        m_ritLabel->setValue(hz);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setXitEnabled(bool v)
{
    if (m_xitBtn && m_xitBtn->isChecked() != v) {
        m_updatingFromModel = true;
        m_xitBtn->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setXitHz(int hz)
{
    if (m_xitLabel && m_xitLabel->value() != hz) {
        m_updatingFromModel = true;
        m_xitLabel->setValue(hz);
        m_updatingFromModel = false;
    }
}

// ---- DSP tab state setters (S1.8b) ----

// Label + styling mirror Thetis console.cs:43518-43546 [v2.10.3.13]:
//   Off → label "NB", dim background, unchecked
//   NB  → label "NB", active background, checked
//   NB2 → label "NB2", active background, indeterminate (shown as checked)
void VfoWidget::setNbMode(NereusSDR::NbMode m)
{
    if (!m_nbButton) { return; }
    m_updatingFromModel = true;
    switch (m) {
        case NereusSDR::NbMode::Off:
            m_nbButton->setText(QStringLiteral("NB"));
            m_nbButton->setChecked(false);
            break;
        case NereusSDR::NbMode::NB:
            m_nbButton->setText(QStringLiteral("NB"));
            m_nbButton->setChecked(true);
            break;
        case NereusSDR::NbMode::NB2:
            m_nbButton->setText(QStringLiteral("NB2"));
            m_nbButton->setChecked(true);
            break;
    }
    m_updatingFromModel = false;
}

void VfoWidget::setNr2Enabled(bool v)
{
    // Legacy adapter called by MainWindow — forward to the NR2 slot button.
    // Kept for API compatibility during the transition; Task 18 will rewire
    // MainWindow to call onActiveNrChanged directly via the signal.
    onActiveNrChanged(v ? NereusSDR::NrSlot::NR2 : NereusSDR::NrSlot::Off);
}

void VfoWidget::onActiveNrChanged(NereusSDR::NrSlot slot)
{
    if (!m_nr1Btn) { return; }  // not yet built
    QSignalBlocker b1(m_nr1Btn),  b2(m_nr2Btn),  b3(m_nr3Btn), b4(m_nr4Btn);
    QSignalBlocker b5(m_dfnrBtn), b6(m_bnrBtn),  b7(m_mnrBtn);
    m_nr1Btn->setChecked(slot  == NereusSDR::NrSlot::NR1);
    m_nr2Btn->setChecked(slot  == NereusSDR::NrSlot::NR2);
    m_nr3Btn->setChecked(slot  == NereusSDR::NrSlot::NR3);
    m_nr4Btn->setChecked(slot  == NereusSDR::NrSlot::NR4);
    m_dfnrBtn->setChecked(slot == NereusSDR::NrSlot::DFNR);
    m_bnrBtn->setChecked(slot  == NereusSDR::NrSlot::BNR);
    m_mnrBtn->setChecked(slot  == NereusSDR::NrSlot::MNR);
}

void VfoWidget::setSnbEnabled(bool v)
{
    if (m_snbToggle && m_snbToggle->isChecked() != v) {
        m_updatingFromModel = true;
        m_snbToggle->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setApfEnabled(bool v)
{
    if (m_apfToggle && m_apfToggle->isChecked() != v) {
        m_updatingFromModel = true;
        m_apfToggle->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setApfTuneHz(int hz)
{
    if (m_apfTuneSlider && m_apfTuneSlider->value() != hz) {
        m_updatingFromModel = true;
        m_apfTuneSlider->setValue(hz);
        m_updatingFromModel = false;
    }
}

// ---- Mode container visibility (S1.9) ----

void VfoWidget::applyModeVisibility(DSPMode mode)
{
    // Mode containers embedded in DspTab — show only the one matching
    // the active demodulation mode.
    if (m_fmContainer) {
        m_fmContainer->setVisible(mode == DSPMode::FM);
    }
    if (m_digContainer) {
        m_digContainer->setVisible(mode == DSPMode::DIGL || mode == DSPMode::DIGU);
    }
    if (m_rttyContainer) {
        // RTTY is a DIGL sub-mode — mark/shift controls shown alongside DIG offset
        m_rttyContainer->setVisible(mode == DSPMode::DIGL);
    }

    // APF tune slider — visible only when APF is enabled AND mode is CW.
    // m_apfToggle (the enable button) is always visible; only the slider + Hz
    // label are gated. m_apfLabel was removed in the Sub-epic C-1 3×4 redesign.
    bool apfVisible = (m_apfToggle && m_apfToggle->isChecked())
                      && (mode == DSPMode::CWL || mode == DSPMode::CWU);
    if (m_apfTuneSlider) {
        m_apfTuneSlider->setVisible(apfVisible);
    }
    if (m_apfTuneLabel) {
        m_apfTuneLabel->setVisible(apfVisible);
    }
}

// ---- Audio tab state setters (S1.8c — guarded against re-emit) ----

void VfoWidget::setMuted(bool v)
{
    if (m_muteBtn && m_muteBtn->isChecked() != v) {
        m_updatingFromModel = true;
        m_muteBtn->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setAudioPan(double pan)
{
    if (m_panSlider) {
        int val = static_cast<int>(std::round(pan * 100.0));
        if (m_panSlider->value() != val) {
            m_updatingFromModel = true;
            m_panSlider->setValue(val);
            if (m_panLabel) {
                m_panLabel->setText(QString::number(val));
            }
            m_updatingFromModel = false;
        }
    }
}

void VfoWidget::setSsqlEnabled(bool v)
{
    if (m_sqlBtn && m_sqlBtn->isChecked() != v) {
        m_updatingFromModel = true;
        m_sqlBtn->setChecked(v);
        m_updatingFromModel = false;
    }
}

void VfoWidget::setSsqlThresh(double dB)
{
    if (m_sqlSlider) {
        int val = static_cast<int>(std::round(dB));
        val = std::max(0, std::min(100, val));
        if (m_sqlSlider->value() != val) {
            m_updatingFromModel = true;
            m_sqlSlider->setValue(val);
            m_updatingFromModel = false;
        }
    }
}

void VfoWidget::setAgcThreshold(int dBu)
{
    if (m_agcTSlider) {
        int val = std::max(-160, std::min(0, dBu));
        if (m_agcTSlider->value() != val) {
            m_updatingFromModel = true;
            m_agcTSlider->setValue(val);
            if (m_agcTLabel) {
                m_agcTLabel->setText(QString::number(val));
            }
            m_updatingFromModel = false;
        }
    }
}

void VfoWidget::updateAgcAutoVisuals(bool autoOn, float noiseFloorDbm, double offset)
{
    m_autoAgcActive = autoOn;
    m_noiseFloorDbm = noiseFloorDbm;

    if (!m_agcTSlider || !m_agcTContainer) {
        return;
    }

    if (autoOn) {
        // AUTO badge → bright green (active) — only the button illuminates
        if (m_agcAutoLabel) {
            m_agcAutoLabel->setStyleSheet(
                QStringLiteral("QPushButton { background: #1a2a1a; border: 1px solid #adff2f;"
                                "color: #adff2f; font-size: 7px; padding: 0 3px; border-radius: 2px; }"
                                "QPushButton:hover { background: #2a3a2a; }"));
        }

        // Show info sub-line
        if (m_agcInfoLabel) {
            m_agcInfoLabel->setText(
                QStringLiteral("NF %1 dB \u00b7 offset +%2")
                    .arg(static_cast<int>(noiseFloorDbm))
                    .arg(static_cast<int>(offset)));
            m_agcInfoLabel->show();
        }
    } else {
        // AUTO badge → dim gray (inactive)
        if (m_agcAutoLabel) {
            m_agcAutoLabel->setStyleSheet(
                QStringLiteral("QPushButton { background: #1a1a1a; border: 1px solid #445;"
                                "color: #556; font-size: 7px; padding: 0 3px; border-radius: 2px; }"
                                "QPushButton:hover { border-color: #adff2f; }"));
        }
        // Hide info sub-line
        if (m_agcInfoLabel) {
            m_agcInfoLabel->hide();
        }
    }
}

void VfoWidget::setBinauralEnabled(bool v)
{
    if (m_binBtn && m_binBtn->isChecked() != v) {
        m_updatingFromModel = true;
        m_binBtn->setChecked(v);
        m_updatingFromModel = false;
    }
}

// ---- Slice coupling (for mode container binding only) ----

void VfoWidget::setSlice(SliceModel* slice)
{
    // Drop prior VAX bindings before m_slice is reassigned — otherwise a
    // repeat setSlice() leaks connections: each click would re-invoke the
    // old lambda, and vaxChannelChanged from a stale SliceModel could
    // clobber the selector away from the currently bound slice.
    if (m_vaxSelector) {
        if (m_slice) {
            disconnect(m_slice, &SliceModel::vaxChannelChanged,
                       m_vaxSelector, &VaxChannelSelector::setValue);
        }
        disconnect(m_vaxSelector, &VaxChannelSelector::valueChanged,
                   this, nullptr);
    }

    m_slice = QPointer<SliceModel>(slice);
    if (m_fmContainer) {
        m_fmContainer->setSlice(slice);
    }
    if (m_digContainer) {
        m_digContainer->setSlice(slice);
    }
    if (m_rttyContainer) {
        m_rttyContainer->setSlice(slice);
    }

    // Sub-epic C-1: NR bank — sync from slice activeNr and initial state.
    if (slice) {
        connect(slice, &SliceModel::activeNrChanged,
                this, &VfoWidget::onActiveNrChanged);
        onActiveNrChanged(slice->activeNr());
    }

    // VAX selector — bidirectional wiring (Phase 3O Sub-Phase 8 Task 8.2)
    if (m_vaxSelector && slice) {
        connect(m_vaxSelector, &VaxChannelSelector::valueChanged,
                this, [this](int ch) {
            if (m_slice) {
                m_slice->setVaxChannel(ch);
            }
        });
        connect(slice, &SliceModel::vaxChannelChanged,
                m_vaxSelector, &VaxChannelSelector::setValue);
        // Sync widget to current model state (e.g. restored from AppSettings)
        m_vaxSelector->setValue(slice->vaxChannel());
    }
}

// ---- Floating control buttons (AetherSDR pattern) ----
// Close, Lock, Record, Play — rendered on parent SpectrumWidget

static const char* kFloatingBtn =
    "QPushButton {"
    "  background: rgba(255,255,255,15); border: none;"
    "  border-radius: 10px; color: #c8d8e8; font-size: 11px; padding: 0;"
    "}"
    "QPushButton:hover {"
    "  background: rgba(255,255,255,40);"
    "}";

static const char* kFloatingBtnClose =
    "QPushButton {"
    "  background: rgba(255,255,255,15); border: none;"
    "  border-radius: 10px; color: #c8d8e8; font-size: 11px; padding: 0;"
    "}"
    "QPushButton:hover {"
    "  background: rgba(204,32,32,180); color: #ffffff;"
    "}";

void VfoWidget::buildFloatingButtons()
{
    QWidget* parent = parentWidget();
    if (!parent || m_closeBtn) {
        return;  // Already built or no parent
    }

    auto makeBtn = [&](const QString& text, const char* style) -> QPushButton* {
        auto* btn = new QPushButton(text, parent);
        btn->setFixedSize(20, 20);
        btn->setStyleSheet(style);
        btn->show();
        return btn;
    };

    // Close button — wired
    m_closeBtn = makeBtn(QStringLiteral("\u2715"), kFloatingBtnClose);
    // NereusSDR native — Thetis has no per-slice close button
    m_closeBtn->setToolTip(QStringLiteral("Close slice"));
    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        emit closeRequested(m_sliceIndex);
    });

    // Lock button — wired
    m_lockBtn = makeBtn(QStringLiteral("\U0001F513"), kFloatingBtn);
    // From Thetis console.resx:5787 — chkVFOLock.ToolTip
    m_lockBtn->setToolTip(QStringLiteral("Keeps the VFO from changing while in the middle of a QSO."));
    m_lockBtn->setCheckable(true);
    connect(m_lockBtn, &QPushButton::toggled, this, [this](bool locked) {
        if (!m_updatingFromModel) {
            applyLockedState(locked);
        }
    });

    // Record button — checkable, NYI-badged (no consumer in Stage 1)
    m_recBtn = makeBtn(QStringLiteral("\u23FA"), kFloatingBtn);
    // From Thetis console.resx:2028 — ckQuickRec.ToolTip
    m_recBtn->setToolTip(QStringLiteral("Quick Record of \"off the air\" signals"));
    m_recBtn->setCheckable(true);
    connect(m_recBtn, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) {
            emit recordToggled(on);
        }
    });
    NyiOverlay::markNyi(m_recBtn, QStringLiteral("phase3g10-stage2"));

    // Play button — checkable, NYI-badged (no consumer in Stage 1)
    m_playBtn = makeBtn(QStringLiteral("\u25B6"), kFloatingBtn);
    // From Thetis console.resx:1941 — ckQuickPlay.ToolTip
    m_playBtn->setToolTip(QStringLiteral("Quick Playback of signals recorded \"off the air\""));
    m_playBtn->setCheckable(true);
    connect(m_playBtn, &QPushButton::toggled, this, [this](bool on) {
        if (!m_updatingFromModel) {
            emit playToggled(on);
        }
    });
    NyiOverlay::markNyi(m_playBtn, QStringLiteral("phase3g10-stage2"));
}

// ---- Lock state: applyLockedState + setLocked (S1.8a review — I3) ----
// applyLockedState is the single path for all lock changes — called by both
// the floating m_lockBtn toggled lambda and the X/RIT m_xritLockBtn toggled
// lambda.  setLocked is the inbound edge driven by SliceModel::lockedChanged.

void VfoWidget::applyLockedState(bool on)
{
    // Snapshot the incoming guard state so we can restore it around each
    // button update and correctly decide whether to emit at the end.
    const bool wasUpdating = m_updatingFromModel;

    // Update state
    m_locked = on;

    // Drive floating lock button — set guard while calling setChecked so its
    // toggled signal does not re-enter applyLockedState.
    if (m_lockBtn) {
        m_updatingFromModel = true;
        m_lockBtn->setChecked(on);
        m_lockBtn->setText(on ? QStringLiteral("\U0001F512") : QStringLiteral("\U0001F513"));
        if (on) {
            m_lockBtn->setStyleSheet(QStringLiteral(
                "QPushButton { background: rgba(255,100,100,80); border: none;"
                "  border-radius: 10px; color: #c8d8e8; font-size: 11px; padding: 0; }"
                "QPushButton:hover { background: rgba(255,100,100,120); }"));
        } else {
            m_lockBtn->setStyleSheet(kFloatingBtn);
        }
        m_updatingFromModel = wasUpdating;
    }

    // Drive X/RIT lock button — same pattern.
    if (m_xritLockBtn) {
        m_updatingFromModel = true;
        m_xritLockBtn->setChecked(on);
        m_updatingFromModel = wasUpdating;
    }

    // Only emit lockChanged when the change originates from a user action
    // (i.e., guard was false when this call began).  When called from
    // setLocked() the guard is set true and we skip the emit, preventing
    // a model → widget → model feedback loop.
    if (!wasUpdating) {
        emit lockChanged(on);
    }
}

void VfoWidget::setLocked(bool v)
{
    if (m_locked == v) {
        return;
    }
    // Guard true → applyLockedState will update both buttons but will NOT
    // emit lockChanged back toward the model.
    m_updatingFromModel = true;
    applyLockedState(v);
    m_updatingFromModel = false;
}

void VfoWidget::positionFloatingButtons()
{
    if (!m_closeBtn) {
        return;
    }

    // Stack vertically on the opposite side of the flag from the VFO marker
    // From AetherSDR VfoWidget.cpp:1724-1749
    int btnX;
    if (m_onLeft) {
        // Flag is on the left of marker → buttons on right side of flag
        btnX = x() + width() + 2;
    } else {
        // Flag is on the right of marker → buttons on left side of flag
        btnX = x() - 22;
    }

    // Clamp to parent bounds
    if (parentWidget()) {
        btnX = std::clamp(btnX, 0, parentWidget()->width() - 20);
    }

    int btnY = y();
    QPushButton* btns[] = {m_closeBtn, m_lockBtn, m_recBtn, m_playBtn};
    for (QPushButton* btn : btns) {
        btn->move(btnX, btnY);
        btn->setVisible(isVisible());
        if (isVisible()) {
            btn->raise();
        }
        btnY += 22;
    }
}

// ---- Positioning ----

void VfoWidget::updatePosition(int vfoX, int specTop, FlagDir dir)
{
    // Build floating buttons on first call (parent is now available)
    if (!m_closeBtn && parentWidget()) {
        buildFloatingButtons();
    }

    int flagW = width();
    int parentW = parentWidget() ? parentWidget()->width() : 2000;
    bool onLeft = false;

    if (dir == FlagDir::ForceLeft) {
        onLeft = true;
    } else if (dir == FlagDir::ForceRight) {
        onLeft = false;
    } else {
        // Auto: flag goes OPPOSITE side of passband so it doesn't cover signals.
        // From AetherSDR VfoWidget.cpp:1696 — onLeft = !lowerSideband
        bool lowerSideband = (m_currentMode == DSPMode::LSB ||
                              m_currentMode == DSPMode::DIGL ||
                              m_currentMode == DSPMode::CWL);
        onLeft = !lowerSideband;
    }

    int x;
    if (onLeft) {
        x = vfoX - flagW;
        // Flip to right if clipped off left edge
        if (x < 0) {
            x = vfoX;
            onLeft = false;
        }
    } else {
        x = vfoX;
        // Flip to left if clipped off right edge
        if (x + flagW > parentW) {
            x = vfoX - flagW;
            onLeft = true;
        }
    }

    // Final clamp to stay on screen
    x = std::clamp(x, 0, std::max(0, parentW - flagW));

    m_onLeft = onLeft;
    move(x, specTop);
    positionFloatingButtons();
}

// ---- Painting ----

void VfoWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Dark panel background — from AetherSDR VfoWidget::paintEvent
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x0a, 0x0a, 0x14, 230));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 4, 4);

    // Subtle border
    p.setPen(QColor(255, 255, 255, 30));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 4, 4);

    // Colored top border matching slice color
    QColor c = sliceColor(m_sliceIndex);
    p.setPen(QPen(c, 2));
    p.drawLine(2, 1, width() - 3, 1);
}

void VfoWidget::mousePressEvent(QMouseEvent* event)
{
    event->accept();
    emit sliceActivationRequested(m_sliceIndex);

    // Double-click on frequency area → enter edit mode
    if (event->type() == QEvent::MouseButtonDblClick) {
        QRect freqRect = m_freqStack->geometry();
        if (freqRect.contains(event->pos())) {
            // Format current frequency as MHz for editing
            double mhz = m_frequency / 1e6;
            m_freqEdit->setText(QString::number(mhz, 'f', 6));
            m_freqEdit->selectAll();
            m_freqStack->setCurrentIndex(1);
            m_freqEdit->setFocus();
        }
    }
}

void VfoWidget::wheelEvent(QWheelEvent* event)
{
    event->accept();
    if (m_locked) {
        return;
    }
    int delta = event->angleDelta().y();
    if (delta == 0) {
        return;
    }
    int steps = (delta > 0) ? 1 : -1;
    double newFreq = m_frequency + steps * m_stepHz;
    newFreq = std::clamp(newFreq, 100000.0, 61440000.0);

    if (!qFuzzyCompare(newFreq, m_frequency)) {
        m_frequency = newFreq;
        updateFreqLabel();
        emit frequencyChanged(newFreq);
    }
}

// ---- Helpers ----

double VfoWidget::parseUserFrequency(const QString& raw)
{
    QString s = raw.trimmed();
    if (s.isEmpty()) { return -1.0; }

    // Detect and strip unit suffix (longest match first so "MHz" wins over "Hz").
    double mult = 0.0;
    bool hasUnit = false;
    const auto tryStripSuffix = [&](const char* suffix, double m) {
        if (s.endsWith(QLatin1String(suffix), Qt::CaseInsensitive)) {
            s.chop(qstrlen(suffix));
            mult = m;
            hasUnit = true;
            return true;
        }
        return false;
    };
    tryStripSuffix("MHz", 1e6)
        || tryStripSuffix("kHz", 1e3)
        || tryStripSuffix("Hz",  1.0)
        || tryStripSuffix("M",   1e6)
        || tryStripSuffix("K",   1e3);
    s = s.trimmed();
    if (s.isEmpty()) { return -1.0; }

    const int nDots   = s.count(QLatin1Char('.'));
    const int nCommas = s.count(QLatin1Char(','));

    // Normalize separators. The goal: end up with at most one '.' as the
    // decimal separator, with any grouping separators removed.
    if (nDots >= 2 && nCommas == 0) {
        // "7.230.000" (or with unit: "7.230.000 Hz") — dots are thousand
        // separators. When no unit was given, default to Hz since that's
        // the only sensible interpretation of a multi-dot number.
        s.remove(QLatin1Char('.'));
        if (!hasUnit) { mult = 1.0; }
    } else if (nCommas >= 2 && nDots == 0) {
        // "7,230,000" — US thousand-separated Hz value.
        s.remove(QLatin1Char(','));
        if (!hasUnit) { mult = 1.0; }
    } else if (nDots > 0 && nCommas > 0) {
        // Mixed: the last occurrence is the decimal, the rest are thousands.
        // The presence of thousand separators means the user is writing a
        // Hz value (e.g. "7,230,000.50"); no unit makes no other sense.
        if (s.lastIndexOf(QLatin1Char('.')) > s.lastIndexOf(QLatin1Char(','))) {
            s.remove(QLatin1Char(','));
        } else {
            s.remove(QLatin1Char('.'));
            s.replace(QLatin1Char(','), QLatin1Char('.'));
        }
        if (!hasUnit) { mult = 1.0; }
    } else if (nCommas == 1 && nDots == 0) {
        // Single comma — ambiguous. If a unit suffix was already parsed, a
        // three-digit tail is a US-style thousands separator
        // (e.g. "7,230 kHz" → 7,230 kHz), anything else is EU decimal
        // ("7,23 MHz" → 7.23 MHz). Without a unit, fall through to EU
        // decimal — the historical behavior — because a bare "7,23" with
        // no grouping context reads as a decimal in every locale that
        // writes it that way.
        const int commaIdx  = s.indexOf(QLatin1Char(','));
        const int tailCount = s.size() - commaIdx - 1;
        if (hasUnit && tailCount == 3) {
            s.remove(QLatin1Char(','));
        } else {
            s.replace(QLatin1Char(','), QLatin1Char('.'));
        }
    }
    // else: at most a single '.' (C-locale ready) or a plain integer.

    bool ok = false;
    const double v = s.toDouble(&ok);
    if (!ok || v < 0.0) { return -1.0; }

    if (mult != 0.0) {
        return v * mult;
    }

    // Plain number, no unit, no grouping separators. Matches the Thetis
    // MHz-decimal convention when a decimal point is present. For bare
    // integers, pick the first unit (MHz → kHz → Hz) whose interpretation
    // lies in the Red Pitaya tuning range — this rescues users who typed
    // "7230" (intending kHz) or "7230000" (intending Hz) without guessing
    // wrong like the prior heuristic did (issue #73).
    constexpr double kMinHz = 100000.0;     // 100 kHz floor
    constexpr double kMaxHz = 61440000.0;   // 61.44 MHz ceiling
    const bool isDecimal = (nDots == 1);
    if (isDecimal) {
        return v * 1e6;  // Thetis convention: decimal number is MHz
    }
    const double asMHz = v * 1e6;
    const double asKHz = v * 1e3;
    const double asHz  = v;
    if (asMHz >= kMinHz && asMHz <= kMaxHz) { return asMHz; }
    if (asKHz >= kMinHz && asKHz <= kMaxHz) { return asKHz; }
    if (asHz  >= kMinHz && asHz  <= kMaxHz) { return asHz;  }
    // No interpretation in range: fall back to MHz; caller will clamp.
    return asMHz;
}

void VfoWidget::updateFreqLabel()
{
    // Format: "14.225.000" (MHz with period separators every 3 digits after decimal)
    // From Thetis txtVFOAFreq format: freq.ToString("f6")
    double mhz = m_frequency / 1e6;
    int intPart = static_cast<int>(mhz);
    int fracPart = static_cast<int>(std::round((mhz - intPart) * 1e6));
    int khz = fracPart / 1000;
    int hz = fracPart % 1000;

    m_freqLabel->setText(QStringLiteral("%1.%2.%3")
        .arg(intPart)
        .arg(khz, 3, 10, QLatin1Char('0'))
        .arg(hz, 3, 10, QLatin1Char('0')));

    // Also update filter width display
    // (will be properly synced from setFilter)
}

QString VfoWidget::formatFilterWidth(int low, int high) const
{
    int width = std::abs(high - low);
    if (width >= 1000) {
        return QStringLiteral("%1K").arg(width / 1000.0, 0, 'f', 1);
    }
    return QString::number(width);
}

QColor VfoWidget::sliceColor(int index)
{
    // From AetherSDR SliceColors.h
    switch (index) {
    case 0: return QColor(0x00, 0xd4, 0xff);  // cyan
    case 1: return QColor(0xff, 0x40, 0xff);  // magenta
    case 2: return QColor(0x40, 0xff, 0x40);  // green
    case 3: return QColor(0xff, 0xff, 0x00);  // yellow
    default: return QColor(0x00, 0xd4, 0xff);
    }
}

// Phase 3P-I-a T15 — gate RX/TX ANT buttons on Alex presence and antenna count.
// Spec: docs/architecture/antenna-routing-design.md §6.1 Rule 2 + Rule 4.
void VfoWidget::setBoardCapabilities(const BoardCapabilities& caps)
{
    m_hasAlex = caps.hasAlex;
    m_hasRxBypassRelay = caps.hasRxBypassRelay;
    const bool showAnt = caps.hasAlex && caps.antennaInputCount >= 3;
    if (m_rxAntBtn) { m_rxAntBtn->setVisible(showAnt); }
    if (m_txAntBtn) { m_txAntBtn->setVisible(showAnt); }
    if (m_rxBypassBtn) { m_rxBypassBtn->setVisible(m_hasRxBypassRelay && m_hasRxOutOnTxUi); }
}

// Phase 3P-I-b T9 — per-SKU BYPS button gate. Called by MainWindow on
// RadioModel::currentRadioChanged alongside setBoardCapabilities.
//
// The button toggles AlexController::rxOutOnTx (Thetis Alex.cs:61
// chkRxOutOnTx), so gate on profile.hasRxOutOnTx — not hasRxBypassUi,
// which is the separate chkDisableRXOut (Alex.cs:65 rx_out_override)
// "Disable RX Bypass relay" override.
// From Thetis setup.cs:6268-6273 [v2.10.3.13] — chkRxOutOnTx visibility per HPSDRModel.
// G8NJJ. will need more work ofr high power PA  [original inline comment from setup.cs:6277, ANAN_G2_1K branch of the same per-SKU switch]
void VfoWidget::setHpsdrSku(HPSDRModel sku)
{
    const SkuUiProfile profile = skuUiProfileFor(sku);
    m_hasRxOutOnTxUi = profile.hasRxOutOnTx;
    if (m_rxBypassBtn) { m_rxBypassBtn->setVisible(m_hasRxBypassRelay && m_hasRxOutOnTxUi); }
}

// Phase 3P-I-b T9 — reflect AlexController::rxOutOnTx into the BYPS button.
// Guard against signal re-emission so model → UI sync doesn't loop back.
void VfoWidget::setRxBypassActive(bool on)
{
    if (!m_rxBypassBtn) { return; }
    if (m_rxBypassBtn->isChecked() == on) { return; }
    const bool prev = m_updatingFromModel;
    m_updatingFromModel = true;
    m_rxBypassBtn->setChecked(on);
    m_updatingFromModel = prev;
}

// ---- Sub-epic C-1: NR bank DspParamPopup builders (Task 15) ----
// Each popup shows the 3-5 most-adjusted knobs for the given NR slot.
// "More Settings…" fires openNrSetupRequested(slot) routed by MainWindow in Task 18.
// Ranges and defaults from Thetis setup.cs [v2.10.3.13] + AetherSDR MainWindow.cpp
// [@0cd4559] lines 7980-8324.

void VfoWidget::showNr1Popup(const QPoint& globalPos)
{
    if (!m_slice) { return; }
    auto* p = new DspParamPopup(this);

    // NR1 (ANR — Adaptive LMS).
    // From Thetis setup.cs udDSPNR1Taps/udDSPNR1Delay/udDSPNR1Gain/udDSPNR1Leak ranges
    // [v2.10.3.13].  Gain/Leakage stored as WDSP-domain values; sliders use UI units.
    p->addSlider(QStringLiteral("Taps"), 16, 128, m_slice->nr1Taps(),
                 [](int v) { return QString::number(v); },
                 [this](int v) { if (m_slice) m_slice->setNr1Taps(v); });
    p->addSlider(QStringLiteral("Delay"), 1, 256, m_slice->nr1Delay(),
                 [](int v) { return QString::number(v); },
                 [this](int v) { if (m_slice) m_slice->setNr1Delay(v); });
    // Gain: UI units = WDSP value / 1e-6. Slider range 0-999 = 0.0-0.000999 WDSP.
    const int uiGain = static_cast<int>(m_slice->nr1Gain() / 1e-6);
    p->addSlider(QStringLiteral("Gain"), 0, 999, uiGain,
                 [](int v) { return QString::number(v); },
                 [this](int v) { if (m_slice) m_slice->setNr1Gain(v * 1e-6); });
    // Leakage: UI units = WDSP value / 1e-3. Slider range 0-999 = 0.0-0.999e-3 WDSP.
    const int uiLeak = static_cast<int>(m_slice->nr1Leakage() / 1e-3);
    p->addSlider(QStringLiteral("Leak"), 0, 999, uiLeak,
                 [](int v) { return QString::number(v); },
                 [this](int v) { if (m_slice) m_slice->setNr1Leakage(v * 1e-3); });
    p->addRadioGroup(QStringLiteral("Position"),
                     {QStringLiteral("Pre-AGC"), QStringLiteral("Post-AGC")},
                     static_cast<int>(m_slice->nr1Position()),
                     [this](int v) {
                         if (m_slice) m_slice->setNr1Position(static_cast<NereusSDR::NrPosition>(v));
                     });
    p->finalize([this]() { emit openNrSetupRequested(NereusSDR::NrSlot::NR1); }, nullptr);
    p->showAt(globalPos);
}

void VfoWidget::showNr2Popup(const QPoint& globalPos)
{
    if (!m_slice) { return; }
    auto* p = new DspParamPopup(this);

    // NR2 (EMNR — Enhanced Multiband Noise Reduction).
    // From Thetis setup.designer.cs grpDSPGainMethod / grpDSPNR2NPEMethod /
    // chkDSPNR2AE / chkNR2PostProc_enable_rx1 labels [v2.10.3.13].
    p->addRadioGroup(QStringLiteral("Gain Method"),
                     {QStringLiteral("Linear"), QStringLiteral("Log"),
                      QStringLiteral("Gamma"), QStringLiteral("Trained")},
                     static_cast<int>(m_slice->nr2GainMethod()),
                     [this](int v) {
                         if (m_slice) m_slice->setNr2GainMethod(static_cast<NereusSDR::EmnrGainMethod>(v));
                     });
    // From Thetis setup.designer.cs grpDSPNR2NPEMethod / radDSPNR2OSMS/MMSE/NSTAT [v2.10.3.13].
    p->addRadioGroup(QStringLiteral("NPE Method"),
                     {QStringLiteral("OSMS"), QStringLiteral("MMSE"), QStringLiteral("NSTAT")},
                     static_cast<int>(m_slice->nr2NpeMethod()),
                     [this](int v) {
                         if (m_slice) m_slice->setNr2NpeMethod(static_cast<NereusSDR::EmnrNpeMethod>(v));
                     });
    // From Thetis setup.designer.cs chkDSPNR2AE.Text = "AE Filter" [v2.10.3.13].
    p->addCheckbox(QStringLiteral("AE Filter"), m_slice->nr2AeFilter(),
                   [this](bool v) { if (m_slice) m_slice->setNr2AeFilter(v); });
    // From Thetis setup.designer.cs chkNR2PostProc_enable_rx1.Text = "Noise post proc" [v2.10.3.13].
    p->addCheckbox(QStringLiteral("Noise post proc"), m_slice->nr2Post2Run(),
                   [this](bool v) { if (m_slice) m_slice->setNr2Post2Run(v); });
    // From Thetis setup.designer.cs labelTS476.Text = "Factor:" / labelTS475.Text = "Rate:" [v2.10.3.13].
    const int post2Factor = static_cast<int>(m_slice->nr2Post2Factor());
    p->addSlider(QStringLiteral("Factor"), 0, 30, post2Factor,
                 [](int v) { return QString::number(v); },
                 [this](int v) { if (m_slice) m_slice->setNr2Post2Factor(static_cast<double>(v)); });
    const int post2Rate = static_cast<int>(m_slice->nr2Post2Rate());
    p->addSlider(QStringLiteral("Rate"), 0, 30, post2Rate,
                 [](int v) { return QString::number(v); },
                 [this](int v) { if (m_slice) m_slice->setNr2Post2Rate(static_cast<double>(v)); });
    p->finalize([this]() { emit openNrSetupRequested(NereusSDR::NrSlot::NR2); }, nullptr);
    p->showAt(globalPos);
}

void VfoWidget::showNr3Popup(const QPoint& globalPos)
{
    if (!m_slice) { return; }
    auto* p = new DspParamPopup(this);

    // NR3 (RNNR — Recurrent Neural Net NR).
    // From Thetis setup.cs udRNNR position + RXANR3FixedGain [v2.10.3.13]
    // and AetherSDR MainWindow.cpp:8200-8260 [@0cd4559].
    p->addRadioGroup(QStringLiteral("Position"),
                     {QStringLiteral("Pre-AGC"), QStringLiteral("Post-AGC")},
                     static_cast<int>(m_slice->nr3Position()),
                     [this](int v) {
                         if (m_slice) m_slice->setNr3Position(static_cast<NereusSDR::NrPosition>(v));
                     });
    // From Thetis setup.designer.cs chkNR3_RNNoiseFixedGain.Text =
    // "Use fixed gain for input samples" [v2.10.3.13].
    p->addCheckbox(QStringLiteral("Use fixed gain for input samples"), m_slice->nr3UseDefaultGain(),
                   [this](bool v) { if (m_slice) m_slice->setNr3UseDefaultGain(v); });
    // "Load Model…" opens Setup NR3 page where file dialog lives (Task 17).
    p->finalize([this]() { emit openNrSetupRequested(NereusSDR::NrSlot::NR3); }, nullptr);
    p->showAt(globalPos);
}

void VfoWidget::showNr4Popup(const QPoint& globalPos)
{
    if (!m_slice) { return; }
    auto* p = new DspParamPopup(this);

    // NR4 (SBNR — Spectral Baseline NR).
    // From Thetis setup.designer.cs labelTS446/473 "Reduction", labelTS449/471 "Smoothing",
    // labelTS451/468 "Whitening", labelTS453/466 "Rescale", labelTS455/459 "SNRthresh",
    // radNR4_algo1/2/3 "Algo 1/2/3" [v2.10.3.13].
    const int reduction = static_cast<int>(m_slice->nr4Reduction());
    p->addSlider(QStringLiteral("Reduction"), 0, 20, reduction,
                 [](int v) { return QString::number(v) + QStringLiteral(" dB"); },
                 [this](int v) { if (m_slice) m_slice->setNr4Reduction(static_cast<double>(v)); });
    const int smoothing = static_cast<int>(m_slice->nr4Smoothing());
    p->addSlider(QStringLiteral("Smoothing"), 0, 100, smoothing,
                 [](int v) { return QString::number(v) + QStringLiteral("%"); },
                 [this](int v) { if (m_slice) m_slice->setNr4Smoothing(static_cast<double>(v)); });
    const int whitening = static_cast<int>(m_slice->nr4Whitening());
    p->addSlider(QStringLiteral("Whitening"), 0, 100, whitening,
                 [](int v) { return QString::number(v) + QStringLiteral("%"); },
                 [this](int v) { if (m_slice) m_slice->setNr4Whitening(static_cast<double>(v)); });
    const int rescale = static_cast<int>(m_slice->nr4Rescale());
    p->addSlider(QStringLiteral("Rescale"), 0, 20, rescale,
                 [](int v) { return QString::number(v) + QStringLiteral(" dB"); },
                 [this](int v) { if (m_slice) m_slice->setNr4Rescale(static_cast<double>(v)); });
    const int snrThresh = static_cast<int>(m_slice->nr4PostThresh());
    p->addSlider(QStringLiteral("SNRthresh"), -30, 0, snrThresh,
                 [](int v) { return QString::number(v) + QStringLiteral(" dB"); },
                 [this](int v) { if (m_slice) m_slice->setNr4PostThresh(static_cast<double>(v)); });
    p->addRadioGroup(QStringLiteral("Algo"),
                     {QStringLiteral("Algo 1"), QStringLiteral("Algo 2"), QStringLiteral("Algo 3")},
                     static_cast<int>(m_slice->nr4Algo()),
                     [this](int v) {
                         if (m_slice) m_slice->setNr4Algo(static_cast<NereusSDR::SbnrAlgo>(v));
                     });
    p->finalize([this]() { emit openNrSetupRequested(NereusSDR::NrSlot::NR4); }, nullptr);
    p->showAt(globalPos);
}

void VfoWidget::showDfnrPopup(const QPoint& globalPos)
{
    if (!m_slice) { return; }
    auto* p = new DspParamPopup(this);

    // DFNR (DeepFilter NR) — AetherSDR post-WDSP filter, not in Thetis.
    // Ranges from AetherSDR MainWindow.cpp:7980-8080 [@0cd4559].
    // Labels are NereusSDR-native (no Thetis equivalent).
    const int attenLimit = static_cast<int>(m_slice->dfnrAttenLimit());
    p->addSlider(QStringLiteral("Attenuation Limit"), 0, 100, attenLimit,
                 [](int v) { return QString::number(v) + QStringLiteral(" dB"); },
                 [this](int v) { if (m_slice) m_slice->setDfnrAttenLimit(static_cast<double>(v)); });
    // PostFilterBeta range 0-100 slider displayed as /100 → 0.00-1.00.
    const int beta = static_cast<int>(m_slice->dfnrPostFilterBeta() * 100.0);
    p->addSlider(QStringLiteral("Post-Filter Beta"), 0, 100, beta,
                 [](int v) { return QString::number(v / 100.0, 'f', 2); },
                 [this](int v) { if (m_slice) m_slice->setDfnrPostFilterBeta(v / 100.0); });
    p->finalize([this]() { emit openNrSetupRequested(NereusSDR::NrSlot::DFNR); }, nullptr);
    p->showAt(globalPos);
}

void VfoWidget::showBnrPopup(const QPoint& globalPos)
{
    if (!m_slice) { return; }
    auto* p = new DspParamPopup(this);

    // BNR (NVIDIA Noise Removal) — button hidden unless HAVE_BNR; popup
    // included for completeness in case BNR is enabled in a future build.
    // AetherSDR MainWindow.cpp:8080-8100 [@0cd4559].
    const int strength = static_cast<int>(m_slice->bnrStrength() * 100.0);
    p->addSlider(QStringLiteral("Strength"), 0, 100, strength,
                 [](int v) { return QString::number(v) + QStringLiteral("%"); },
                 [this](int v) { if (m_slice) m_slice->setBnrStrength(v / 100.0); });
    p->finalize([this]() { emit openNrSetupRequested(NereusSDR::NrSlot::BNR); }, nullptr);
    p->showAt(globalPos);
}

void VfoWidget::showMnrPopup(const QPoint& globalPos)
{
    if (!m_slice) { return; }
    auto* p = new DspParamPopup(this);

    // MNR (macOS CoreML noise reduction) — button hidden off-macOS unless HAVE_MNR.
    // AetherSDR MainWindow.cpp:8100 [@0cd4559].
    const int strength = static_cast<int>(m_slice->mnrStrength() * 100.0);
    p->addSlider(QStringLiteral("Strength"), 0, 100, strength,
                 [](int v) { return QString::number(v) + QStringLiteral("%"); },
                 [this](int v) { if (m_slice) m_slice->setMnrStrength(v / 100.0); });
    p->finalize([this]() { emit openNrSetupRequested(NereusSDR::NrSlot::MNR); }, nullptr);
    p->showAt(globalPos);
}

} // namespace NereusSDR
