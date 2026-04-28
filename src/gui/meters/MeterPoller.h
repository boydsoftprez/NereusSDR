#pragma once

// =================================================================
// src/gui/meters/MeterPoller.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-04-26 — Phase 3M-1a H.2: TX meter bindings on MOX engage/release.
//                 setTxChannel() + setInTx(bool) slot added.  TX poll
//                 reads TXA_OUT_PK / TXA_ALC_PK / TXA_ALC_AV / TXA_ALC_GAIN
//                 via GetTXAMeter() when in TX mode.
//                 Cite: Thetis dsp.cs:999-1050 [v2.10.3.13] CalculateTXMeter.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

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

#include "core/WdspTypes.h"

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVector>

namespace NereusSDR {

class RxChannel;
class TxChannel;
class MeterWidget;
class RadioStatus;

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

    // ── TX meter bindings (H.2, Phase 3M-1a) ─────────────────────────────
    //
    // setTxChannel: register the TX channel for TX-meter polling.
    // Call with nullptr to detach (e.g. on radio disconnect).
    // Non-owning; RadioModel/WdspEngine own the object.
    //
    // setInTx: switch the poll set between RX and TX meters.
    // Ported from Thetis dsp.cs:999-1050 [v2.10.3.13] CalculateTXMeter:
    //   case MeterType.TXA_OUT_PK:   val = GetTXAMeter(channel, TXA_OUT_PK);
    //   case MeterType.TXA_ALC_PK:   val = GetTXAMeter(channel, TXA_ALC_PK);
    //   case MeterType.TXA_ALC_AV:   val = GetTXAMeter(channel, TXA_ALC_AV);
    //   case MeterType.TXA_ALC_GAIN: val = GetTXAMeter(channel, TXA_ALC_GAIN);
    // When isTx=true the poll() path skips the RX meter loop and calls
    // GetTXAMeter for the four TX binding IDs (TxAlc, TxAlcGain, TxPower stub).
    // When isTx=false the poll() path resumes normal RX meter polling.
    void setTxChannel(TxChannel* channel);

    void addTarget(MeterWidget* widget);
    void removeTarget(MeterWidget* widget);

    void setInterval(int ms);
    int interval() const;

    void start();
    void stop();

    /// Wire PA telemetry signals from RadioStatus into the meter targets
    /// for MeterBinding::TxPower / TxReversePower / TxSwr.
    /// Call once during integration; call with nullptr to detach cleanly.
    /// Safe to call again with a new pointer — the previous connection is
    /// disconnected automatically before re-connecting.
    /// Cite: Thetis console.cs PollPAPWR loop [v2.10.3.13] (RadioStatus
    /// aggregates forward/reflected/swr from that loop via powerChanged).
    void setRadioStatus(RadioStatus* status);

public slots:
    // Switch between RX and TX meter polling.
    // Connected to MoxController::moxStateChanged(bool) by MainWindow (H.2).
    // From Thetis dsp.cs:995-1050 [v2.10.3.13] CalculateTXMeter (TX branch)
    // vs CalculateRXMeter (RX branch). Switches happen at MOX engage/release,
    // not mid-poll, matching Thetis's integer-tick dispatch via UpdateTimer.
    void setInTx(bool isTx);

signals:
    // Emitted on each poll tick with the current S-meter (SignalAvg) dBm value.
    // Connect to VfoWidget::setSmeter to drive the VFO level bar.
    void smeterUpdated(double dbm);

private slots:
    void poll();

private:
    // ── TX poll helper ────────────────────────────────────────────────────────
    // Reads the 4 WDSP TX meters gated for 3M-1a and pushes them to all
    // registered MeterWidget targets.
    // Porting from Thetis dsp.cs:999-1050 [v2.10.3.13] CalculateTXMeter.
    void pollTxMeters();

    QTimer m_timer;
    QPointer<RxChannel> m_rxChannel;
    // Non-owning TX channel pointer (H.2).  Valid only while WdspEngine has
    // opened the TX channel (after createTxChannel()).  Guarded in poll().
    // QPointer auto-clears when TxChannel is destroyed — matches m_rxChannel
    // pattern (3M-1a review fixup: prevents dangling-pointer dereference if
    // WdspEngine destroys the channel without calling setTxChannel(nullptr)).
    QPointer<TxChannel> m_txChannel;
    // m_inTx: true while MOX is active; flipped by setInTx(bool).
    // From Thetis dsp.cs:995-1050 [v2.10.3.13] TX/RX meter dispatch.
    bool m_inTx{false};
    // QPointer auto-clears to nullptr when the MeterWidget is destroyed.
    // ContainerManager's float/dock swap replaces MeterWidgets mid-session;
    // without QPointer, poll() would dereference the deleted old widgets.
    QVector<QPointer<MeterWidget>> m_targets;

    // PA telemetry wiring (Part B — Phase 3M-0 Task 7).
    // m_radioStatus is a non-owning raw pointer (RadioModel owns the object).
    // m_powerConn holds the single connection to RadioStatus::powerChanged;
    // disconnected on re-set or when status is nullptr.
    RadioStatus*            m_radioStatus{nullptr};
    QMetaObject::Connection m_powerConn;
};

} // namespace NereusSDR
