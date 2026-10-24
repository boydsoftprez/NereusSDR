// =================================================================
// src/gui/applets/RxControlSections.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.resx (upstream has no top-of-file header — project-level LICENSE applies)
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// Container copies of the RX applet's jobs. Widget meaning, tooltips and
// ranges follow RxApplet. The face is the workspace raised gradient used
// by the PureSignal and TX applet buttons (Style::buttonBaseStyle through
// WorkspacePreview::lookQss): light top, dark bottom, radius round(h * 7 / 26),
// lit green or lit amber when a toggle is on.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24 - The mode combo takes its items from the applet only;
//                 Thetis headers byte for byte and the tooltip cites
//                 stamped at v2.10.3.15. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-23 - RX Filter can hide its width readout and its passband.
//                 Brian Sutton, AI-assisted via xAI Grok.
//   2026-10-23 - RX applet jobs as container sections. Brian Sutton,
//                 AI-assisted via xAI Grok.
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
// ApacheLabs G2E support added throughout Thetis in various files, all changes marked  //N1GP G2E added
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Final modifictions by MW0LGE Richard Samphire - 19th April 2026
// Nothing further added by him after this date, and his repo is now in archive https://github.com/ramdor/Thetis
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

//
// Upstream source 'Project Files/Source/Console/console.resx' has no top-of-file GPL header —
// project-level Thetis LICENSE applies.

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

// =================================================================
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   Layout pattern from AetherSDR `src/gui/RxApplet.{h,cpp}`.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.

#include "RxControlSections.h"

#include "core/ControlRanges.h"

#include <QJsonObject>
#include "gui/StyleConstants.h"
#include "gui/styles/WorkspaceComponents.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/widgets/FilterPassbandWidget.h"
#include "gui/widgets/TriBtn.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace NereusSDR {
namespace {

void column(QWidget* widget)
{
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(rxCopyMargins());
    layout->setSpacing(2);
}

QVBoxLayout* rows(QWidget* widget)
{
    return static_cast<QVBoxLayout*>(widget->layout());
}

void raisedButton(QPushButton* button, const QString& checked)
{
    button->setMinimumHeight(28);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setStyleSheet(Style::buttonBaseStyle() + checked
        + QStringLiteral("QPushButton { font-size: 12px; padding: 4px 5px; }"));
}

void raisedMomentary(QPushButton* button)
{
    button->setCheckable(false);
    raisedButton(button, QString());
}

void workspaceCombo(QComboBox* combo)
{
    combo->setFixedHeight(28);
    combo->setStyleSheet(WorkspacePreview::lookQss(QStringLiteral(
        "QComboBox {"
        "  background: %1; color: %2;"
        "  border: 1px solid %3; border-radius: 3px;"
        "  padding: 2px 6px; font-size: 12px;"
        "}"
        "QComboBox::drop-down { border: none; width: 18px; }"
        "QComboBox::down-arrow {"
        "  image: url(:/icons/spin-down.svg); width: 10px; height: 10px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background: %1; color: %2;"
        "  selection-background-color: %4;"
        "  border: 1px solid %3;"
        "}"
    ).arg(Style::kButtonBg, Style::kTextPrimary, Style::kBorder, Style::kAccent), combo));
}

void workspaceSlider(QSlider* slider)
{
    slider->setMinimumHeight(Workspace::Components::kSliderHandleSpanPx);
    slider->setStyleSheet(Style::sliderHStyle());
}

void workspaceSpin(QSpinBox* spin)
{
    spin->setFixedHeight(28);
    spin->setStyleSheet(WorkspacePreview::lookQss(QStringLiteral(
        "QSpinBox {"
        "  background: %1; color: %2;"
        "  border: 1px solid %3; border-radius: 3px;"
        "  padding: 2px 4px; font-size: 12px;"
        "}"
    ).arg(Style::kButtonBg, Style::kTextPrimary, Style::kBorder), spin));
}

void caption(QLabel* label)
{
    label->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 12px; }")
                             .arg(Style::kTextPrimary));
}

void readout(QLabel* label)
{
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(Style::insetValueStyle());
}

void workspaceArrow(TriBtn* button)
{
    button->setFixedSize(28, 28);
    button->setStyleSheet(Style::buttonBaseStyle()
        + QStringLiteral("QPushButton { padding: 0; min-width: 0; font-size: 12px; }"));
}

} // namespace

RxAntennasSection::RxAntennasSection(QWidget* parent)
    : QWidget(parent)
{
    column(this);
    auto* row = new QHBoxLayout;
    row->setSpacing(4);
    m_rx = new QPushButton(QStringLiteral("ANT1"), this);
    m_rx->setObjectName(QStringLiteral("RxCopyRxAntButton"));
    raisedMomentary(m_rx);
    // From Thetis console.resx:8277 [v2.10.3.15] — chkRxAnt.ToolTip
    m_rx->setToolTip(QStringLiteral("Toggles receive antenna between RX and TX antennas for RX1"));
    m_tx = new QPushButton(QStringLiteral("ANT1"), this);
    m_tx->setObjectName(QStringLiteral("RxCopyTxAntButton"));
    raisedMomentary(m_tx);
    m_tx->setToolTip(QStringLiteral("Select the transmit antenna port"));
    row->addWidget(m_rx, 1);
    row->addWidget(m_tx, 1);
    rows(this)->addLayout(row);
}

RxModeSection::RxModeSection(QWidget* parent)
    : QWidget(parent)
{
    column(this);
    m_combo = new QComboBox(this);
    m_combo->setObjectName(QStringLiteral("RxCopyModeCombo"));
    // The items, their tooltips and the choice come from the applet's own
    // mode combo (RxApplet::syncCopy).
    workspaceCombo(m_combo);
    m_combo->setToolTip(QStringLiteral("Select operating mode"));
    rows(this)->addWidget(m_combo);
}

RxStepSection::RxStepSection(QWidget* parent)
    : QWidget(parent)
{
    column(this);
    auto* row = new QHBoxLayout;
    row->setSpacing(4);
    auto* lbl = new QLabel(QStringLiteral("STEP:"), this);
    caption(lbl);
    row->addWidget(lbl);
    m_down = new TriBtn(TriBtn::Left, this);
    m_down->setObjectName(QStringLiteral("RxCopyStepDown"));
    workspaceArrow(m_down);
    row->addWidget(m_down);
    m_label = new QLabel(QStringLiteral("100 Hz"), this);
    m_label->setObjectName(QStringLiteral("RxCopyStepLabel"));
    readout(m_label);
    row->addWidget(m_label, 1);
    m_up = new TriBtn(TriBtn::Right, this);
    m_up->setObjectName(QStringLiteral("RxCopyStepUp"));
    workspaceArrow(m_up);
    row->addWidget(m_up);
    rows(this)->addLayout(row);
}

RxFilterSection::RxFilterSection(QWidget* parent)
    : QWidget(parent)
{
    column(this);
    m_width = new QLabel(QStringLiteral("2.9K"), this);
    m_width->setObjectName(QStringLiteral("RxCopyFilterWidth"));
    readout(m_width);
    m_width->setToolTip(QStringLiteral("Current filter passband width"));
    rows(this)->addWidget(m_width);

    auto* host = new QWidget(this);
    m_grid = new QGridLayout(host);
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setSpacing(2);
    m_grid->setColumnStretch(0, 1);
    m_grid->setColumnStretch(1, 1);
    m_grid->setColumnStretch(2, 1);
    rows(this)->addWidget(host);

    m_passband = new FilterPassbandWidget(this);
    m_passband->setObjectName(QStringLiteral("RxCopyFilterPassband"));
    m_passband->setFixedHeight(WorkspacePreview::enabled() ? 49 : 48);
    m_passband->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    rows(this)->addWidget(m_passband);
}

namespace {

QColor configColor(const QJsonObject& config, const char* key, const QColor& fallback)
{
    const QColor parsed(config.value(QLatin1String(key)).toString());
    return parsed.isValid() ? parsed : fallback;
}

} // namespace

void RxFilterSection::applyConfig(const QJsonObject& config)
{
    if (m_width) { m_width->setVisible(!config.value(QLatin1String("hideTop")).toBool(false)); }
    if (m_passband) { m_passband->setVisible(!config.value(QLatin1String("hideBottom")).toBool(false)); }

    const int columns = qBound(1, config.value(QLatin1String("columns")).toInt(3), 32);
    m_columns = columns;
    if (m_grid) {
        const int existing = m_grid->columnCount();
        for (int i = 0; i < existing; ++i) { m_grid->setColumnStretch(i, 0); }
        for (int i = 0; i < m_columns; ++i) { m_grid->setColumnStretch(i, 1); }
    }

    m_textColor = configColor(config, "buttonText", m_textColor);
    m_backgroundTop = configColor(config, "backgroundTop", m_backgroundTop);
    m_backgroundBottom = configColor(config, "backgroundBottom", m_backgroundBottom);
    m_selectedTop = configColor(config, "selectedTop", m_selectedTop);
    m_selectedBottom = configColor(config, "selectedBottom", m_selectedBottom);
}

QString RxFilterSection::presetButtonStyle() const
{
    const auto gradient = [](const QColor& top, const QColor& bottom) {
        return QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2)")
            .arg(top.name(QColor::HexRgb), bottom.name(QColor::HexRgb));
    };
    return QStringLiteral(
        "QPushButton {"
        "  background: %1; color: %2;"
        "  border: 1px solid #3a3a42; border-radius: 8px;"
        "  font-size: 12px; font-weight: bold; padding: 4px 5px;"
        "}"
        "QPushButton:checked {"
        "  background: %3; color: %2; border: 1px solid #36b056;"
        "}")
        .arg(gradient(m_backgroundTop, m_backgroundBottom),
             m_textColor.name(QColor::HexRgb),
             gradient(m_selectedTop, m_selectedBottom));
}

RxPanSection::RxPanSection(QWidget* parent)
    : QWidget(parent)
{
    column(this);
    auto* row = new QHBoxLayout;
    row->setSpacing(4);
    auto* left = new QLabel(QStringLiteral("L"), this);
    caption(left);
    row->addWidget(left);
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setObjectName(QStringLiteral("RxCopyPanSlider"));
    m_slider->setRange(0, 100);
    m_slider->setValue(50);
    workspaceSlider(m_slider);
    row->addWidget(m_slider, 1);
    auto* right = new QLabel(QStringLiteral("R"), this);
    caption(right);
    row->addWidget(right);
    rows(this)->addLayout(row);
}

RxSquelchSection::RxSquelchSection(QWidget* parent)
    : QWidget(parent)
{
    column(this);
    auto* row = new QHBoxLayout;
    row->setSpacing(4);
    m_button = new QPushButton(QStringLiteral("SQL"), this);
    m_button->setObjectName(QStringLiteral("RxCopySqlButton"));
    m_button->setCheckable(true);
    raisedButton(m_button, Style::greenCheckedStyle());
    row->addWidget(m_button);
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setObjectName(QStringLiteral("RxCopySqlSlider"));
    m_slider->setRange(ControlRanges::kSsqlThreshMin, ControlRanges::kSsqlThreshMax);
    m_slider->setSingleStep(ControlRanges::kSsqlThreshStep);
    m_slider->setValue(20);
    workspaceSlider(m_slider);
    row->addWidget(m_slider, 1);
    rows(this)->addLayout(row);
}

RxAttenuatorSection::RxAttenuatorSection(QWidget* parent)
    : QWidget(parent)
{
    column(this);
    auto* row = new QHBoxLayout;
    row->setSpacing(4);
    m_label = new QLabel(QStringLiteral("ATT"), this);
    m_label->setObjectName(QStringLiteral("RxCopyAttLabel"));
    caption(m_label);
    row->addWidget(m_label);
    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("RxCopyAttenuatorStack"));
    m_preamp = new QComboBox(m_stack);
    m_preamp->setObjectName(QStringLiteral("RxCopyPreampCombo"));
    workspaceCombo(m_preamp);
    m_stack->addWidget(m_preamp);
    m_step = new QSpinBox(m_stack);
    m_step->setObjectName(QStringLiteral("RxCopyStepAttSpin"));
    m_step->setSuffix(QStringLiteral(" dB"));
    m_step->setRange(0, 31);
    workspaceSpin(m_step);
    m_stack->addWidget(m_step);
    m_stack->setCurrentIndex(0);
    row->addWidget(m_stack, 1);
    rows(this)->addLayout(row);

    m_rx1Preamp = new QCheckBox(QStringLiteral("RX1 preamp"), this);
    m_rx1Preamp->setObjectName(QStringLiteral("RxCopyRx1Preamp"));
    m_rx1Preamp->setStyleSheet(WorkspacePreview::lookQss(QStringLiteral(
        "QCheckBox { color: %1; font-size: 12px; }"
        "QCheckBox::indicator { width: 14px; height: 14px; }").arg(Style::kTextPrimary),
        m_rx1Preamp));
    m_rx1Preamp->hide();
    rows(this)->addWidget(m_rx1Preamp);
}

RxAgcSection::RxAgcSection(QWidget* parent)
    : QWidget(parent)
{
    column(this);
    auto* header = new QHBoxLayout;
    header->setSpacing(4);
    m_combo = new QComboBox(this);
    m_combo->setObjectName(QStringLiteral("RxCopyAgcCombo"));
    for (const ControlRanges::AgcModeItem& item : ControlRanges::kAgcModes) {
        m_combo->addItem(QString::fromLatin1(item.label), item.id);
    }
    workspaceCombo(m_combo);
    // From Thetis console.resx:4554 [v2.10.3.15] — comboAGC.ToolTip
    m_combo->setToolTip(QStringLiteral("Automatic Gain Control Mode Setting"));
    header->addWidget(m_combo);
    auto* name = new QLabel(QStringLiteral("AGC-T"), this);
    caption(name);
    header->addWidget(name);
    header->addStretch(1);
    m_value = new QLabel(QStringLiteral("-20"), this);
    m_value->setObjectName(QStringLiteral("RxCopyAgcValue"));
    readout(m_value);
    header->addWidget(m_value);
    m_auto = new QPushButton(QStringLiteral("AUTO"), this);
    m_auto->setObjectName(QStringLiteral("RxCopyAgcAuto"));
    m_auto->setCheckable(true);
    raisedButton(m_auto, Style::greenCheckedStyle());
    // From Thetis setup.designer.cs:38795 [v2.10.3.15] — chkAutoAGCRX1.ToolTip
    m_auto->setToolTip(QStringLiteral("Automatically adjust AGC based on Noise Floor"));
    header->addWidget(m_auto);
    rows(this)->addLayout(header);

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setObjectName(QStringLiteral("RxCopyAgcSlider"));
    m_slider->setRange(ControlRanges::kAgcThresholdMinDb, ControlRanges::kAgcThresholdMaxDb);
    m_slider->setSingleStep(ControlRanges::kAgcThresholdStepDb);
    m_slider->setValue(-20);
    workspaceSlider(m_slider);
    // From Thetis console.resx:8397 [v2.10.3.15] — ptbRF.ToolTip (ptbRF is the AGC-T slider)
    m_slider->setToolTip(QStringLiteral(
        "AGC Max Gain - Operates similarly to traditional RF Gain. Right click AUTO based on noise floor."));
    rows(this)->addWidget(m_slider);

    m_info = new QLabel(this);
    m_info->setObjectName(QStringLiteral("RxCopyAgcInfo"));
    caption(m_info);
    m_info->hide();
    rows(this)->addWidget(m_info);
}

RxRitSection::RxRitSection(QWidget* parent)
    : QWidget(parent)
{
    column(this);
    auto* row = new QHBoxLayout;
    row->setSpacing(4);
    m_on = new QPushButton(QStringLiteral("RIT"), this);
    m_on->setObjectName(QStringLiteral("RxCopyRitOn"));
    m_on->setCheckable(true);
    raisedButton(m_on, Style::amberCheckedStyle());
    // From Thetis console.resx:4335 [v2.10.3.15] — chkRIT.ToolTip
    m_on->setToolTip(QStringLiteral(
        "Receive Incremental Tuning - offset RX frequency by value below in Hz."));
    row->addWidget(m_on, 1);
    m_zero = new QPushButton(QStringLiteral("0"), this);
    m_zero->setObjectName(QStringLiteral("RxCopyRitZero"));
    raisedMomentary(m_zero);
    row->addWidget(m_zero);
    m_minus = new TriBtn(TriBtn::Left, this);
    workspaceArrow(m_minus);
    row->addWidget(m_minus);
    m_label = new QLabel(QStringLiteral("+0 Hz"), this);
    m_label->setObjectName(QStringLiteral("RxCopyRitLabel"));
    readout(m_label);
    row->addWidget(m_label, 1);
    m_plus = new TriBtn(TriBtn::Right, this);
    workspaceArrow(m_plus);
    row->addWidget(m_plus);
    rows(this)->addLayout(row);
}

RxXitSection::RxXitSection(QWidget* parent)
    : QWidget(parent)
{
    column(this);
    auto* row = new QHBoxLayout;
    row->setSpacing(4);
    m_on = new QPushButton(QStringLiteral("XIT"), this);
    m_on->setObjectName(QStringLiteral("RxCopyXitOn"));
    m_on->setCheckable(true);
    raisedButton(m_on, Style::amberCheckedStyle());
    // From Thetis console.resx:4416 [v2.10.3.15] — chkXIT.ToolTip
    m_on->setToolTip(QStringLiteral(
        "Transmit Incremental Tuning - offset TX frequency by the value below in Hz."));
    row->addWidget(m_on, 1);
    m_zero = new QPushButton(QStringLiteral("0"), this);
    m_zero->setObjectName(QStringLiteral("RxCopyXitZero"));
    raisedMomentary(m_zero);
    row->addWidget(m_zero);
    m_minus = new TriBtn(TriBtn::Left, this);
    workspaceArrow(m_minus);
    row->addWidget(m_minus);
    m_label = new QLabel(QStringLiteral("+0 Hz"), this);
    m_label->setObjectName(QStringLiteral("RxCopyXitLabel"));
    readout(m_label);
    row->addWidget(m_label, 1);
    m_plus = new TriBtn(TriBtn::Right, this);
    workspaceArrow(m_plus);
    row->addWidget(m_plus);
    rows(this)->addLayout(row);
}

} // namespace NereusSDR
