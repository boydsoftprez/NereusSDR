// =================================================================
// src/models/TransmitModel.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-26 — tunePowerByBand[14] + per-MAC persistence (G.3, Phase 3M-1a)
//                 ported by J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
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

#pragma once

#include "Band.h"

#include <QObject>
#include <QString>
#include <array>
#include <atomic>

namespace NereusSDR {

// VAX slot: which audio source owns the transmitter.
// MicDirect = hardware mic, Vax1–Vax4 = virtual audio crossbar slots.
enum class VaxSlot {
    None = 0,
    MicDirect,
    Vax1,
    Vax2,
    Vax3,
    Vax4
};

QString vaxSlotToString(VaxSlot s);
VaxSlot vaxSlotFromString(const QString& s);

// Transmit state management.
// Includes MOX, tune, TX frequency, power, mic gain, and PureSignal state.
class TransmitModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool   mox       READ isMox       WRITE setMox       NOTIFY moxChanged)
    Q_PROPERTY(bool   tune      READ isTune      WRITE setTune      NOTIFY tuneChanged)
    Q_PROPERTY(int    power     READ power       WRITE setPower     NOTIFY powerChanged)
    Q_PROPERTY(float  micGain   READ micGain     WRITE setMicGain   NOTIFY micGainChanged)
    Q_PROPERTY(bool   pureSig   READ pureSigEnabled WRITE setPureSigEnabled NOTIFY pureSigChanged)

public:
    explicit TransmitModel(QObject* parent = nullptr);
    ~TransmitModel() override;

    bool isMox() const { return m_mox; }
    void setMox(bool mox);

    bool isTune() const { return m_tune; }
    void setTune(bool tune);

    int power() const { return m_power; }
    void setPower(int power);

    float micGain() const { return m_micGain; }
    void setMicGain(float gain);

    bool pureSigEnabled() const { return m_pureSigEnabled; }
    void setPureSigEnabled(bool enabled);

    VaxSlot txOwnerSlot() const { return m_txOwnerSlot.load(std::memory_order_acquire); }
    void setTxOwnerSlot(VaxSlot s);

    void loadFromSettings();

    // ── Per-band tune power (G.3) ─────────────────────────────────────────
    //
    // Porting from Thetis console.cs:12094 [v2.10.3.13]:
    //   private int[] tunePower_by_band;
    //
    // Default 50W per band on first init:
    //   console.cs:1819-1820 [v2.10.3.13]:
    //     tunePower_by_band = new int[(int)Band.LAST];
    //     for (int i = 0; i < (int)Band.LAST; i++) tunePower_by_band[i] = 50;
    //
    // NereusSDR uses scalar per-band AppSettings keys instead of Thetis's
    // pipe-delimited string (console.cs:3087-3091 save, :4904-4910 restore).

    /// Return the tune-power value (watts) for the given band.
    /// Default 50W on first init.  Returns 50 as a safe fallback for
    /// out-of-range band values.
    int tunePowerForBand(Band band) const;

    /// Set the tune-power value (watts) for the given band.
    /// Clamped to [0, 100].  Emits tunePowerByBandChanged when the value
    /// actually changes.  No-op for out-of-range band values.
    void setTunePowerForBand(Band band, int watts);

    /// Set the per-MAC AppSettings scope.  Must be called before load() / save().
    /// Mirrors the AlexController::setMacAddress() pattern.
    void setMacAddress(const QString& mac);

    /// Restore all per-band tune-power values from AppSettings under the
    /// current MAC scope.  No-op when no MAC has been set.
    /// Cite: console.cs:4904-4910 [v2.10.3.13] — Thetis pipe-delimited
    /// format; NereusSDR uses per-band scalar keys.
    void load();

    /// Flush all per-band tune-power values to AppSettings under the
    /// current MAC scope.  No-op when no MAC has been set.
    /// Cite: console.cs:3087-3091 [v2.10.3.13].
    void save();

signals:
    void moxChanged(bool mox);
    void tuneChanged(bool tune);
    void powerChanged(int power);
    void micGainChanged(float gain);
    void pureSigChanged(bool enabled);
    void txOwnerSlotChanged(VaxSlot s);

    /// Emitted when a per-band tune-power value changes.
    void tunePowerByBandChanged(Band band, int watts);

private:
    bool m_mox{false};
    bool m_tune{false};
    int m_power{100};
    float m_micGain{0.0f};
    bool m_pureSigEnabled{false};
    std::atomic<VaxSlot> m_txOwnerSlot{VaxSlot::MicDirect};  // Atomic for lock-free reads from the audio thread.

    // Per-band tune power storage.
    // From Thetis console.cs:12094 [v2.10.3.13]: private int[] tunePower_by_band;
    // Initialised to 50W per band in the constructor
    // (console.cs:1819-1820 [v2.10.3.13]).
    std::array<int, static_cast<std::size_t>(Band::Count)> m_tunePowerByBand{};

    // Per-MAC AppSettings scope (mirrors AlexController pattern).
    QString m_mac;
};

} // namespace NereusSDR
