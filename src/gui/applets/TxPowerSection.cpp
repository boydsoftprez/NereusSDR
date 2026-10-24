#include "TxPowerSection.h"

#include "gui/StyleConstants.h"
#include "gui/applets/AccessoryReadouts.h"
#include "gui/instruments/BarInstrument.h"
#include "core/ControlRanges.h"
#include "core/HpsdrModel.h"
#include "models/MainPowerMeter.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

// =================================================================
// src/gui/applets/TxPowerSection.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs - original licence from Thetis
//   source is included below.
//
// Extracted from src/gui/applets/TxApplet.h: the forward-power and SWR
// gauges, the RF Power slider row and the Tune Power slider row.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-20 - The mi0bot-Thetis quotes and MI0BOT tags above the slider
//                rescale and the dB labels, as TxApplet.cpp carried them.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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

TxPowerSection::TxPowerSection(QWidget* parent, int rowSpacing,
                               const MainPowerMeter* meter,
                               const QMargins& outerMargins)
    : QWidget(parent)
{
    m_outerMargins = outerMargins;
    buildUI(rowSpacing, meter);
}

void TxPowerSection::buildUI(int rowSpacing, const MainPowerMeter* meter)
{
    // The column the applet puts its rows in, with the applet's own
    // contents margins. The rows below are added to it unchanged.
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(m_outerMargins);
    vbox->setSpacing(rowSpacing);

// ── 1. Forward Power gauge — per-SKU scale ──────────────────────────────
// Default ticks at construction match the Hermes-class 100 W radio
// (0 / 40 / 80 / 100 / 120 — AetherSDR TxApplet.cpp:71); rescaleFwdGaugeForModel()
// reapplies a per-SKU range when RadioModel::currentRadioChanged fires
// (wired in wireControls()).  Bench-reported #167 follow-up: HL2 reading
// 1-5 W on a 0-120 W scale was a barely-visible sliver; per-SKU scaling
// gives each radio a properly-sized meter.
m_fwdPowerGauge = AccessoryReadouts::makeSlimBar(
    AccessoryReadouts::forwardPower(120.0, 100.0), QStringLiteral("Power"), this);
m_fwdPowerGauge->setAccessibleName(QStringLiteral("Forward power gauge"));
rescaleFwdGaugeForModel(HPSDRModel::FIRST, meter);
vbox->addWidget(m_fwdPowerGauge);

// The approved SWR row uses the same text and limit as the accessory panels.
m_swrGauge = AccessoryReadouts::makeSlimBar(
    AccessoryReadouts::swr(), AccessoryReadouts::swrName(), this);
m_swrGauge->setAccessibleName(QStringLiteral("SWR gauge"));
vbox->addWidget(m_swrGauge);

// ── 3. RF Power slider row ───────────────────────────────────────────────
// Label fixedWidth 62, value fixedWidth 22  (AetherSDR TxApplet.cpp:87–104)
{
    auto* rfSlider = new QSlider(Qt::Horizontal, this);
    rfSlider->setRange(0, 100);
    rfSlider->setValue(100);
    rfSlider->setAccessibleName(QStringLiteral("RF power"));
    rfSlider->setObjectName(QStringLiteral("TxRfPowerSlider"));

    auto* rfValue = new QLabel(QStringLiteral("100"), this);
    rfValue->setFixedWidth(22);
    rfValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rfValue->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextPrimary));

    m_rfPowerSlider = rfSlider;
    m_rfPowerValue  = rfValue;

    auto* row = new QHBoxLayout;
    row->setSpacing(4);

    auto* lbl = new QLabel(QStringLiteral("RF Power:"), this);
    lbl->setFixedWidth(62);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }").arg(Style::kTitleText));
    row->addWidget(lbl);

    rfSlider->setFixedHeight(18);
    rfSlider->setEnabled(true);   // Phase 3M-1a H.3: wired
    // Tooltip set by rescalePowerSlidersForModel() (Issue #175 Task 7) -
    // Thetis-faithful "Transmit Drive - relative value" wording, applied
    // every time the active SKU changes.  No initial setToolTip here.
    row->addWidget(rfSlider, 1);
    row->addWidget(rfValue);

    vbox->addLayout(row);
}

// ── 4. Tune Power slider row ─────────────────────────────────────────────
// Label fixedWidth 62, value fixedWidth 22  (AetherSDR TxApplet.cpp:107–128)
{
    auto* tunSlider = new QSlider(Qt::Horizontal, this);
    tunSlider->setRange(0, 100);
    tunSlider->setValue(10);
    tunSlider->setAccessibleName(QStringLiteral("Tune power"));

    auto* tunValue = new QLabel(QStringLiteral("10"), this);
    tunValue->setFixedWidth(22);
    tunValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    tunValue->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextPrimary));

    m_tunePwrSlider = tunSlider;
    m_tunePwrValue  = tunValue;

    auto* row = new QHBoxLayout;
    row->setSpacing(4);

    auto* lbl = new QLabel(QStringLiteral("Tune Pwr:"), this);
    lbl->setFixedWidth(62);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }").arg(Style::kTitleText));
    row->addWidget(lbl);

    tunSlider->setFixedHeight(18);
    tunSlider->setEnabled(true);  // Phase 3M-1a H.3: wired
    // Tooltip set by rescalePowerSlidersForModel() (Issue #175 Task 7) -
    // Thetis-faithful "Tune and/or 2Tone Drive - relative value" wording,
    // applied every time the active SKU changes.  No initial setToolTip
    // here.
    row->addWidget(tunSlider, 1);
    row->addWidget(tunValue);

    vbox->addLayout(row);
}
}

void TxPowerSection::rescaleFwdGaugeForModel(HPSDRModel model,
                                                const MainPowerMeter* meter)
{
    if (!m_fwdPowerGauge) { return; }
    const int limit = meter ? meter->visualLimitW() : paMaxWattsFor(model);
    const double top = meter ? meter->scaleTopW()
        : limit * ControlRanges::kRfPowerGaugeHeadroom;
    ReadingDescriptor reading = AccessoryReadouts::forwardPower(top, limit);
    // Per-SKU PA rating and 20 percent headroom remain the radio rule.
    // Keep sub-watt resolution for a QRP radio, whole watts for an amplifier.
    reading.decimals = limit <= 10 ? 1 : 0;
    const auto fmt = [limit](double watts) {
        return QString::number(watts, 'f', limit <= 10 ? 1 : 0);
    };
    reading.ticks = {{0.0, fmt(0.0)}, {limit / 3.0, fmt(limit / 3.0)},
                     {2.0 * limit / 3.0, fmt(2.0 * limit / 3.0)},
                     {static_cast<double>(limit), fmt(limit)}, {top, fmt(top)}};
    m_fwdPowerGauge->setReading(reading, QStringLiteral("Power"));
}

void TxPowerSection::refreshPowerReadouts(const MainPowerMeter* meter,
                                            bool transmitting,
                                            HPSDRModel hardwareModel)
{
    if (!meter || !m_fwdPowerGauge || !m_swrGauge) { return; }
    if (!transmitting) {
        m_fwdPowerSmoothedW = 0.0;
        m_fwdPowerGauge->clearValue();
        m_swrGauge->clearValue();
        m_fwdPowerGauge->setCompactNote({}, {});
        return;
    }
    m_fwdPowerGauge->setValue(meter->onAmplifier() ? meter->forwardW() : m_fwdPowerSmoothedW);
    m_swrGauge->setValue(meter->swr());
    if (meter->onAmplifier()) {
        const int decimals = paMaxWattsFor(hardwareModel) <= 10 ? 1 : 0;
        m_fwdPowerGauge->setCompactNote(QStringLiteral("DRIVE"),
            QString::number(meter->driveW(), 'f', decimals) + QStringLiteral(" W"));
    } else {
        m_fwdPowerGauge->setCompactNote({}, {});
    }
}

// ── Issue #175 Task 7: per-SKU power-slider rescale + dB labels ─────────────
//
// From mi0bot-Thetis console.cs:2098-2108 [v2.10.3.13-beta2]
//   if (HPSDRHW.HermesLite == Audio.LastRadioHardware ||
//       HPSDRModel.HERMESLITE == HardwareSpecific.Model)     // MI0BOT: Need an early indication of hardware type due to HL2 rx attenuator can be negative
//   {
//       ptbPWR.Maximum = 90;        // MI0BOT: Changes for HL2 only having a 16 step output attenuator
//       ptbPWR.Value = 0;
//       ptbPWR.LargeChange = 6;
//       ptbPWR.SmallChange = 6;
//       ptbTune.Maximum = 99;
//       ptbTune.Value = 0;
//       ptbTune.LargeChange = 3;
//       ptbTune.SmallChange = 3;
//       ...
//   }
//
// HL2: RF Power slider 0..90 step 6 (16-step output attenuator,
//      0.5 dB/step); Tune slider 0..99 step 3 (33 sub-steps).
// Other SKUs: canonical Thetis 0..100 step 1.
//
// MI0BOT: Changes for HL2 only having a 16 step output attenuator
// [original inline comment from mi0bot console.cs:2098]
void TxPowerSection::rescalePowerSlidersForModel(HPSDRModel model)
{
    if (!m_rfPowerSlider || !m_tunePwrSlider) { return; }

    // Cache for updatePowerSliderLabels() so the formatter sees the same
    // SKU the slider was rescaled for, even when called outside the live
    // currentRadioChanged path (unit tests, headless harness).
    m_powerSliderModel = model;

    const QSignalBlocker rfBlock(m_rfPowerSlider);
    const QSignalBlocker tunBlock(m_tunePwrSlider);

    m_rfPowerSlider->setRange(0, rfPowerSliderMaxFor(model));
    m_rfPowerSlider->setSingleStep(rfPowerSliderStepFor(model));
    m_rfPowerSlider->setPageStep(rfPowerSliderStepFor(model));

    m_tunePwrSlider->setRange(0, tuneSliderMaxFor(model));
    m_tunePwrSlider->setSingleStep(tuneSliderStepFor(model));
    m_tunePwrSlider->setPageStep(tuneSliderStepFor(model));

    // Tooltip - Thetis-faithful on every SKU.  Replaces the previous
    // "RF output power (0-100 W)" / "Tune carrier power for current band
    // (0-100 W)" wording, which contradicted Thetis semantics on every
    // SKU (the slider is a relative drive level, not watts).
    //
    // From Thetis console.resx + mi0bot-Thetis console.resx [v2.10.3.13-beta2]
    //   <data ...><value>Transmit Drive - This is a relative value and is
    //   not meant to imply watts, unless the PA profile is configured
    //   with MAX watts @ 100%</value></data>
    // TxApplet's drive source gate sets them (kRfPowerTooltip,
    // kTunePowerTooltip), with its reason while a slider is grayed, in every
    // copy of this row.

    updatePowerSliderLabels();
}

// From mi0bot-Thetis console.cs:29245-29274 [v2.10.3.13-beta2]
//   if (HardwareSpecific.Model == HPSDRModel.HERMESLITE)       // MI0BOT: HL2 has only 15 output power levels
//   {
//       ...
//       lblPWR.Text = "Drive:  " + ((Math.Round(drv / 6.0) / 2) - 7.5).ToString() + "dB";
//   }
//
// HL2 RF Power label formula:
//   dB = (round(drv/6.0)/2) - 7.5     // 0.5 dB steps in [-7.5, 0]
// HL2 Tune slider label formula (slider 0..99 -> dB -16.5..0):
//   dB = (slider/3 - 33) / 2          // inverse of the persistence formula
//
// MI0BOT: HL2 has only 15 output power levels
// [original inline comment from mi0bot console.cs:29245]
void TxPowerSection::updatePowerSliderLabels()
{
    if (!m_rfPowerSlider || !m_tunePwrSlider
        || !m_rfPowerValue || !m_tunePwrValue) { return; }

    // Use the cached SKU set by rescalePowerSlidersForModel() so the
    // label format always matches the slider's effective range (HL2
    // 0..90 step 6 / 0..99 step 3, all others 0..100 step 1).
    const HPSDRModel model = m_powerSliderModel;
    const int        rfVal  = m_rfPowerSlider->value();
    const int        tunVal = m_tunePwrSlider->value();

    // The shown values come from HpsdrModel.h (rfPowerShownFor,
    // tuneSliderShownFor: mi0bot's UpdateDriveLabel and UpdateTuneLabel),
    // which the Core's catalogue reads too.
    const int decimals = powerSliderShownDecimalsFor(model);
    m_rfPowerValue->setText(QString::number(rfPowerShownFor(model, rfVal), 'f', decimals));
    m_tunePwrValue->setText(QString::number(tuneSliderShownFor(model, tunVal), 'f', decimals));
}

} // namespace NereusSDR
