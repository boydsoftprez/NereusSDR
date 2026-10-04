// =================================================================
// src/core/OcMatrix.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/HPSDR/Penny.cs:33-150
//   Project Files/Source/Console/enums.cs:443-457
// Ported from mi0bot-Thetis (mi0bot/OpenHPSDR-Thetis fork) sources:
//   Project Files/Source/Console/HPSDR/Penny.cs:158-159, 162-165
//   Project Files/Source/Console/enums.cs:272-325
//   (RXABitMasks[], TXABitMasks[], setBandABitMask,
//    TX pin action mapping, TXPinActions enum)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Per-MAC persistence via AppSettings;
//                Qt6-native Band enum (14 bands incl. GEN/WWV/XVTR
//                vs Thetis's 12). TXPinAction enum mirrors Thetis
//                enums.cs TXPinActions exactly (7 values: MOX through
//                MOX_TUNE_TWOTONE); no VOX/PA_IN (those do not exist
//                in Thetis TXPinActions as of [@501e3f5]).
//   2026-09-30 - Shared-input filters (ruling (c)): extCtrlBandIndex, the
//                band order mi0bot's HL2 receive arm compares (Penny.cs
//                158-159, enums.cs 272-325 [@c26a8a4]). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Added the enums.cs headers (ramdor/Thetis and the mi0bot
//                fork) this file cites. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-30 - extCtrlBandIndex now serves only the range test; the HL2
//                pins are ordered by frequency. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
// =================================================================
//
// === Verbatim Thetis Console/HPSDR/Penny.cs header ===
/*
*
* Copyright (C) 2008 Bill Tracey, KD5TFD, bill@ewjt.com
* Copyright (C) 2010-2020  Doug Wigley
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
//
// this module contains code to support the Penelope Transmitter board
//
//
// The mi0bot/OpenHPSDR-Thetis fork's Penny.cs [@c26a8a4] carries the same
// header as above, byte-for-byte.
//
// --- From enums.cs (ramdor/Thetis) ---
/*  enums.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2000-2025 Original authors
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

// --- From enums.cs (mi0bot/OpenHPSDR-Thetis fork) ---
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

// =================================================================

#pragma once

#include "core/NereusCoreExport.h"
#include <QObject>
#include <QReadWriteLock>
#include <QString>
#include <array>
#include "models/Band.h"

namespace NereusSDR {

// OC (open-collector) output assignments per band per pin per
// {RX, TX}. Plus TX Pin Action mapping (which pin triggers on
// which TX event: MOX, Tune, TwoTone, MOX+Tune, MOX+TwoTone,
// Tune+TwoTone, or MOX+Tune+TwoTone).
//
// Source: HPSDR/Penny.cs:33-150 + Console/enums.cs:443-457 [@501e3f5]
class NEREUS_CORE_EXPORT OcMatrix : public QObject {
    Q_OBJECT

public:
    // Mirrors Thetis enums.cs:443-457 TXPinActions [@501e3f5].
    // Note: Thetis uses FIRST=-1 and LAST as sentinels for range validation
    // in setTXPinAction(); those sentinels are not needed in this C++ API.
    // From Thetis Console/enums.cs:443-457 [@501e3f5]
    enum class TXPinAction {
        Mox           = 0,   // MOX: tx only
        Tune          = 1,   // TUNE: tune
        TwoTone       = 2,   // TWOTONE: twoTone
        MoxTune       = 3,   // MOX_TUNE: tx or tune
        MoxTwoTone    = 4,   // MOX_TWOTONE: tx or twoTone
        TuneTwoTone   = 5,   // TUNE_TWOTONE: tune or twoTone
        MoxTuneTwoTone = 6,  // MOX_TUNE_TWOTONE: tx or tune or twoTone (Thetis init default)
        Count
    };

    explicit OcMatrix(QObject* parent = nullptr);

    // ── Per-band × per-pin × per-mode pin assignments ──────────────────────
    // From Thetis HPSDR/Penny.cs:117-132 [@501e3f5] setBandABitMask
    bool   pinEnabled(Band band, int pin, bool tx) const;
    void   setPin(Band band, int pin, bool tx, bool enabled);
    quint8 maskFor(Band band, bool tx) const;  // 7-bit mask, bit N = pin N

    // Shared-input filters, ruling (c) 2026-09-30: a band's index as
    // mi0bot's UpdateExtCtrl computes it, the order its HL2 receive arm
    // compares ("the filter for the high band"):
    //   From mi0bot-Thetis HPSDR/Penny.cs:158-159 [@c26a8a4]
    //     int idx = (int)band - (int)Band.B160M;
    //     int idxb = (int)bandb - (int)Band.B160M;
    // over mi0bot's Band enum (enums.cs:272-325 [@c26a8a4]): B160M .. B6M
    // are 0 .. 10, B2M 11, WWV 12, VHF0 .. VHF13 13 .. 26, BLMF 27, B120M
    // .. B11M 28 .. 40. GEN comes before B160M (-1); NereusSDR's XVTR has
    // no place in that enum and is -1 too.
    // NereusSDR uses it only for mi0bot's range test (idx < 0 or idx > 40
    // sends no pins, Penny.cs:162-165 [@c26a8a4]); which receiver's band
    // the pins follow is ordered by frequency (SharedInputLowPass.h,
    // maintainer ruling 2026-09-30), not by this index.
    static constexpr int extCtrlBandIndex(Band band) noexcept
    {
        const int n = static_cast<int>(band);
        if (n >= static_cast<int>(Band::Band160m) && n <= static_cast<int>(Band::Band6m)) {
            return n - static_cast<int>(Band::Band160m);
        }
        if (band == Band::Band2m) { return 11; }
        if (band == Band::WWV)    { return 12; }
        if (n >= static_cast<int>(Band::SwlFirst) && n <= static_cast<int>(Band::SwlLast)) {
            return 28 + (n - static_cast<int>(Band::SwlFirst));
        }
        return -1;
    }

    // ── TX pin action mapping ───────────────────────────────────────────────
    // From Thetis HPSDR/Penny.cs:94-100 [@501e3f5] setTXPinAction
    // (group dimension collapsed: NereusSDR tracks one action per pin)
    TXPinAction pinAction(int pin) const;
    void        setPinAction(int pin, TXPinAction action);

    // ── Persistence ─────────────────────────────────────────────────────────
    void setMacAddress(const QString& mac);
    void load();   // hydrate from AppSettings under hardware/<mac>/oc/...
    void save();   // persist current state to AppSettings

    // Clear every cell, restore Thetis init defaults
    // (pins all clear; all pin actions → MoxTuneTwoTone per Penny.cs:59)
    void resetDefaults();

signals:
    void changed();  // fires on any state mutation; UI re-reads

private:
    static constexpr int kBandCount   = int(Band::Count);  // 14
    static constexpr int kPinCount    = 7;
    static constexpr int kActionCount = int(TXPinAction::Count);

    // [band][pin][tx?] — 3D bool array, matches Thetis RXABitMasks[] / TXABitMasks[]
    std::array<std::array<std::array<bool, 2>, kPinCount>, kBandCount> m_pins{};

    // [pin] — per-pin TX action, matches Thetis TXPinAction[group,pin]
    // (group dimension collapsed: UI tracks one action per pin, not per group)
    std::array<TXPinAction, kPinCount> m_pinActions{};

    QString m_mac;

    // Guards m_pins / m_pinActions. Connection thread reads (maskFor,
    // pinEnabled, pinAction) take a read lock; GUI-thread writes (setPin,
    // setPinAction, load, resetDefaults) take a write lock. Writers release
    // the lock before emitting changed() so slot handlers cannot re-enter
    // under the write lock. (Codex PR #94 review: data race between
    // buildCodecContext()'s maskFor() and OcOutputsHfTab's setPin().)
    mutable QReadWriteLock m_lock;

    QString persistenceKey() const;  // "hardware/<mac>/oc"

    // Persistence slug for a TXPinAction value
    static QString actionSlug(TXPinAction action);
    static TXPinAction actionFromSlug(const QString& slug);
};

} // namespace NereusSDR
