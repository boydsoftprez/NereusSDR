// =================================================================
// src/core/RadioStatus.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/HPSDR/NetworkIOImports.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//                 Aggregates PA forward/reflected power, SWR, current, and
//                 PTT-source from status packets. SWR formula from
//                 console.cs:6642 SWR(adc_fwd, adc_rev) [@501e3f5].
//                 getFwdPower/getRevPower/getExciterPower DllImport declarations
//                 from NetworkIOImports.cs:261-267 [@501e3f5].
//                 PA voltage is not a separately-exposed status field in
//                 Thetis (voltage is derived inside computeAlexFwdPower /
//                 computeAlexRevPower, never returned directly); dropped here
//                 per source-first deviation policy.
// =================================================================

// --- From console.cs ---

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

// --- From NetworkIOImports.cs ---
// NetworkIOImports.cs — Upstream source has no top-of-file GPL header;
// project-level LICENSE (GPLv2-or-later) applies per Thetis repo root.

#pragma once

#include "PttSource.h"
#include <QObject>
#include <QVector>
#include <cmath>

namespace NereusSDR {

// RadioStatus: aggregates live PA telemetry and PTT state received from the
// radio hardware via status/meter packets.
//
// Setters are called by the connection layer (P1RadioConnection::parseEp6Frame /
// P2RadioConnection::parseStatusPacket) after each status update.
//
// SWR formula:
//   From Thetis console.cs:6642 [@501e3f5]:
//     if (adc_fwd == 0 && adc_rev == 0) return 1.0;
//     else if (adc_rev > adc_fwd)       return 50.0;
//     double Ef = ScaledVoltage(adc_fwd);
//     double Er = ScaledVoltage(adc_rev);
//     swr = (Ef + Er) / (Ef - Er);
//
//   NereusSDR's setters receive watts (already scaled) rather than raw ADC
//   counts; the ARRL formula (equivalent form) is used:
//     rho = sqrt(refl/fwd);  swr = (1 + rho) / (1 - rho)
//   clamped to [1.0, 99.0].
class RadioStatus : public QObject {
    Q_OBJECT
public:
    explicit RadioStatus(QObject* parent = nullptr);

    // ── Live readouts ─────────────────────────────────────────────────────
    // Set by connection layer via setters below.

    // From Thetis NetworkIOImports.cs:267 getFwdPower() DllImport [@501e3f5]
    double forwardPowerWatts() const;

    // From Thetis NetworkIOImports.cs:264 getRevPower() DllImport [@501e3f5]
    double reflectedPowerWatts() const;

    // Computed from forward/reflected; 1.0 = 1:1 matched.
    // Formula: console.cs:6642 SWR(adc_fwd, adc_rev) [@501e3f5] (watts-domain equivalent)
    double swrRatio() const;

    // From Thetis NetworkIOImports.cs:261 getExciterPower() DllImport [@501e3f5]
    // Units: milliwatts (exciter drive level, not PA output)
    int exciterPowerMw() const;

    // PA temperature in degrees Celsius. Set when hardware reports it
    // (P2 status byte or per-board extension). Not all boards expose this.
    double paTemperatureCelsius() const;

    // PA supply current in amps. Set when hardware reports it.
    double paCurrentAmps() const;

    // Which source most recently asserted MOX.
    PttSource activePttSource() const;

    // True while the radio is in TX state.
    bool isTransmitting() const;

    // ── Setters (called by connection layer) ──────────────────────────────
    void setForwardPower(double watts);
    void setReflectedPower(double watts);   // also recomputes SWR
    void setExciterPowerMw(int mw);
    void setPaTemperature(double celsius);
    void setPaCurrent(double amps);
    void setActivePttSource(PttSource source);  // also flips isTransmitting
    void setTransmitting(bool tx);

    // ── PTT history (last 8 events) ───────────────────────────────────────
    struct PttEvent {
        qint64    timestampMs;
        PttSource source;
        bool      isStart;  // true = MOX asserted; false = MOX released
    };

    QVector<PttEvent> recentPttEvents() const;

signals:
    void paTemperatureChanged(double celsius);
    void paCurrentChanged(double amps);
    void powerChanged(double forward, double reflected, double swr);
    void pttChanged(PttSource source, bool transmitting);

private:
    double    m_forward{0.0};
    double    m_reflected{0.0};
    double    m_swr{1.0};
    int       m_exciterMw{0};
    double    m_paTemp{0.0};
    double    m_paCurrent{0.0};
    PttSource m_pttSource{PttSource::None};
    bool      m_transmitting{false};

    // Capped at 8 events (ring buffer of recent PTT transitions).
    QVector<PttEvent> m_pttHistory;

    void recordPttEvent(PttSource s, bool start);

    // SWR computation (watts-domain ARRL formula equivalent to
    // Thetis console.cs:6642 SWR(adc_fwd,adc_rev) [@501e3f5]).
    // Returns value in [1.0, 99.0].
    static double computeSwr(double fwd, double refl);

    // Maximum PTT history ring size.
    // no-port-check: NereusSDR-original constant.
    static constexpr int kMaxPttHistory = 8;
};

} // namespace NereusSDR
