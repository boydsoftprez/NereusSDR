#pragma once

// =================================================================
// src/gui/meters/MeterPoller.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

#include "core/WdspTypes.h"

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVector>

namespace NereusSDR {

class RxChannel;
class MeterWidget;

// Binding IDs map to WDSP meter types (RxMeterType enum values)
namespace MeterBinding {
    // RX meters (0-49)
    constexpr int SignalPeak   = 0;    // RxMeterType::SignalPeak
    constexpr int SignalAvg    = 1;    // RxMeterType::SignalAvg
    constexpr int AdcPeak      = 2;    // RxMeterType::AdcPeak
    constexpr int AdcAvg       = 3;    // RxMeterType::AdcAvg
    constexpr int AgcGain      = 4;    // RxMeterType::AgcGain
    constexpr int AgcPeak      = 5;    // RxMeterType::AgcPeak
    constexpr int AgcAvg       = 6;    // RxMeterType::AgcAvg

    // RX meters — new (Phase 3G-4)
    constexpr int SignalMaxBin = 7;    // Spectral peak bin
    constexpr int PbSnr        = 8;    // Peak-to-baseline SNR

    // TX meters (100+). From Thetis MeterManager.cs Reading enum.
    // Stub values until TxChannel exists (Phase 3I-1).
    // PWR/SWR are hardware PA measurements, not WDSP meters.
    constexpr int TxPower        = 100;  // Forward power (hardware PA)
    constexpr int TxReversePower = 101;  // Reverse power (hardware PA)
    constexpr int TxSwr          = 102;  // SWR (computed fwd/rev ratio)
    constexpr int TxMic          = 103;  // TXA_MIC_AV
    constexpr int TxComp         = 104;  // TXA_COMP_AV
    constexpr int TxAlc          = 105;  // TXA_ALC_AV

    // TX meters — new (Phase 3G-4)
    constexpr int TxEq           = 106;  // From Thetis MeterManager.cs EQ reading
    constexpr int TxLeveler      = 107;  // TXA_LEVELER_AV
    constexpr int TxLevelerGain  = 108;  // TXA_LEVELER_GAIN
    constexpr int TxAlcGain      = 109;  // TXA_ALC_GAIN
    constexpr int TxAlcGroup     = 110;  // TXA_ALC_GROUP
    constexpr int TxCfc          = 111;  // TXA_CFC_AV
    constexpr int TxCfcGain      = 112;  // TXA_CFC_GAIN

    // Hardware readings (200+)
    constexpr int HwVolts        = 200;  // PA supply voltage
    constexpr int HwAmps         = 201;  // PA supply current
    constexpr int HwTemperature  = 202;  // PA temperature

    // Rotator readings (300+)
    constexpr int RotatorAz      = 300;  // Azimuth (0-360)
    constexpr int RotatorEle     = 301;  // Elevation (0-90)
}

class MeterPoller : public QObject {
    Q_OBJECT

public:
    explicit MeterPoller(QObject* parent = nullptr);
    ~MeterPoller() override;

    void setRxChannel(RxChannel* channel);
    void addTarget(MeterWidget* widget);
    void removeTarget(MeterWidget* widget);

    void setInterval(int ms);
    int interval() const;

    void start();
    void stop();

signals:
    // Emitted on each poll tick with the current S-meter (SignalAvg) dBm value.
    // Connect to VfoWidget::setSmeter to drive the VFO level bar.
    void smeterUpdated(double dbm);

private slots:
    void poll();

private:
    QTimer m_timer;
    QPointer<RxChannel> m_rxChannel;
    // QPointer auto-clears to nullptr when the MeterWidget is destroyed.
    // ContainerManager's float/dock swap replaces MeterWidgets mid-session;
    // without QPointer, poll() would dereference the deleted old widgets.
    QVector<QPointer<MeterWidget>> m_targets;
};

} // namespace NereusSDR
