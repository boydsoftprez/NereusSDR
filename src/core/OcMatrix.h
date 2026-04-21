// =================================================================
// src/core/OcMatrix.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/HPSDR/Penny.cs:33-150
//   Project Files/Source/Console/enums.cs:443-457
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
// =================================================================

#pragma once

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
class OcMatrix : public QObject {
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
