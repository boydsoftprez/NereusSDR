// =================================================================
// src/core/OcMatrix.cpp  (NereusSDR)
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
//   2026-09-23 - R-R3-46: save() writes only the keys whose saved value
//                differs. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-30 - Added the enums.cs header this file cites. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Applied the TX pin actions on the wire: maskFor(band, tx,
//                tune, twoTone) ports Penny.cs UpdateExtCtrl's adjust step,
//                adjustForTXAction and adjustForRX (J.J. Boyd KG4VCF,
//                AI-assisted: Claude Code)
//   2026-10-26 - Fix wave: the raw-bits lookup cites Penny.cs:156-186, and
//                the reduced group lookup keeps its inline comments
//                (J.J. Boyd KG4VCF, AI-assisted: Claude Code)
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

// =================================================================

#include "OcMatrix.h"

#include <QReadLocker>
#include <QWriteLocker>

#include "AppSettings.h"

namespace NereusSDR {

// Thetis Penny.cs:53-63 init: all pin actions default to MOX_TUNE_TWOTONE
// From Thetis HPSDR/Penny.cs:59 [@501e3f5]
OcMatrix::OcMatrix(QObject* parent) : QObject(parent)
{
    m_pinActions.fill(TXPinAction::MoxTuneTwoTone);  // Thetis init: MOX_TUNE_TWOTONE
}

// From Thetis HPSDR/Penny.cs:117-132 [@501e3f5] setBandABitMask (rx path)
bool OcMatrix::pinEnabled(Band band, int pin, bool tx) const
{
    const int b = int(band);
    if (b < 0 || b >= kBandCount || pin < 0 || pin >= kPinCount) { return false; }
    QReadLocker locker(&m_lock);
    return m_pins[b][pin][tx ? 1 : 0];
}

// From Thetis HPSDR/Penny.cs:117-132 [@501e3f5] setBandABitMask
void OcMatrix::setPin(Band band, int pin, bool tx, bool enabled)
{
    if (!m_loaded) { return; }
    const int b = int(band);
    if (b < 0 || b >= kBandCount || pin < 0 || pin >= kPinCount) { return; }
    {
        QWriteLocker locker(&m_lock);
        auto& slot = m_pins[b][pin][tx ? 1 : 0];
        if (slot == enabled) { return; }
        slot = enabled;
    }
    if (enabled) {
        // Any real pin makes the matrix a live pattern again; a later
        // all-zero save is only permitted after an explicit Reset.
        AppSettings::instance().remove(
            persistenceKey() + QStringLiteral("/userCleared"));
    }
    // Issue #191: persist immediately. The HF / SWL OC tab toggle handlers
    // call setPin() directly and don't follow up with save(); without this,
    // user edits never reach AppSettings and are lost on app restart.
    save();
    emit changed();
}

// The two-argument form: tune and twoTone false. With every action at its
// MOX_TUNE_TWOTONE default the transmit bits pass through unchanged.
quint8 OcMatrix::maskFor(Band band, bool tx) const
{
    return maskFor(band, tx, /*tune=*/false, /*twoTone=*/false);
}

// From Thetis HPSDR/Penny.cs:156-186 [v2.10.3.15] UpdateExtCtrl
// From Thetis-Enhanced (EU2AV) HPSDR/Penny.cs:156-186 [@567a7ec]
// The raw bits are the band's TX or RX pins (Penny.cs:170-178,
// bits = TXABitMasks[idx] / RXABitMasks[idx]), built here from the
// per-pin matrix; SplitPins and VFOBTX are not ported (one band in).
quint8 OcMatrix::maskFor(Band band, bool tx, bool tune, bool twoTone) const
{
    const int b = int(band);
    // [original inline comment from Penny.cs:162] //26
    if (b < 0 || b >= kBandCount) { return 0; }
    QReadLocker locker(&m_lock);
    quint8 bits = 0;
    for (int pin = 0; pin < kPinCount; ++pin) {
        if (m_pins[b][pin][tx ? 1 : 0]) { bits |= quint8(1 << pin); }
    }

    // adjust for pin action and/or PA
    if (tx) {
        bits = adjustForTxAction(bits, tx, tune, twoTone);
    } else {
        bits = adjustForRx(bits);
    }
    //
    return bits;
}

// From Thetis HPSDR/Penny.cs:262-296 [v2.10.3.15] adjustForRX
// From Thetis-Enhanced (EU2AV) HPSDR/Penny.cs:262-296 [@567a7ec]
//
// A pin whose RXPinPA flag is set only closes while xPA is on:
//   From Thetis HPSDR/Penny.cs:277-282 [v2.10.3.15]
//     bool bConisderPA = RXPinPA[group, nPin];
//     int nBit = !bConisderPA || (bConisderPA && pa) ? 1 : 0;
// Thetis's group lookup (getGroup, SplitPins, VFOBTX) chooses which group's
// PA flag to read; NereusSDR tracks one flag per pin, reduced away, and no
// NereusSDR band falls outside the three groups ("some issue finding band,
// skip" cannot happen).
// Reduced lines' comments:
//   [original inline comment from Penny.cs:271] // need to find the group that is VFOA/VFOB
//   [original inline comment from Penny.cs:285] // some issue finding band, skip
quint8 OcMatrix::adjustForRx(quint8 bits) const
{
    int mask = 0;

    for (int nPin = 0; nPin < kPinCount; nPin++) {
        const bool bConisderPA = m_rxPinPa[nPin];
        const int nBit = !bConisderPA || m_extPaEnabled ? 1 : 0;

        mask |= nBit << nPin;
    }

    return quint8(bits & mask);
}

// From Thetis HPSDR/Penny.cs:297-360 [v2.10.3.15] adjustForTXAction
// From Thetis-Enhanced (EU2AV) HPSDR/Penny.cs:297-360 [@567a7ec]
//
// One action per pin (NereusSDR collapses Thetis's [group, pin] array; see
// m_pinActions), and the TXPinPA flag decides whether the action closes the
// pin at all:
//   From Thetis HPSDR/Penny.cs:319-324 [v2.10.3.15]
//     bool bConisderPA = TXPinPA[group, nPin];
//     if (!bConisderPA || (bConisderPA && pa))
// The group lookup is reduced as in adjustForRx; its comments:
//   [original inline comment from Penny.cs:306] // need to find the group that is VFOA/VFOB
//   [original inline comment from Penny.cs:320] // some issue finding band, skip
quint8 OcMatrix::adjustForTxAction(quint8 bits, bool tx, bool tune, bool twoTone) const
{
    int mask = 0;

    for (int nPin = 0; nPin < kPinCount; nPin++) {
        const TXPinAction action = m_pinActions[nPin];
        const bool bConisderPA = m_txPinPa[nPin]; // does this pin follow xPA?

        int nBit = 0;

        if (!bConisderPA || m_extPaEnabled) {
            switch (action) {
            case TXPinAction::Mox: // tx only
                nBit = tx && !tune && !twoTone ? 1 : 0;
                break;
            case TXPinAction::Tune: // tune
                nBit = tune ? 1 : 0;
                break;
            case TXPinAction::TwoTone: // twoTone
                nBit = twoTone ? 1 : 0;
                break;
            case TXPinAction::MoxTune: // tx or tune
                nBit = (tx || tune) && !twoTone ? 1 : 0;
                break;
            case TXPinAction::MoxTwoTone: // tx or twoTone
                // Thetis writes (tx || twoTone) & !tune; the bitwise & on
                // two bools gives the same result as && and would draw a
                // -Wbool-operation warning here.
                nBit = (tx || twoTone) && !tune ? 1 : 0;
                break;
            case TXPinAction::TuneTwoTone: // tune or twoTone
                nBit = tune || twoTone ? 1 : 0;
                break;
            case TXPinAction::MoxTuneTwoTone: // tx or tune or twoTone
                nBit = tx || tune || twoTone ? 1 : 0;
                break;
            case TXPinAction::Count:
                break;
            }
        }

        mask |= nBit << nPin;
    }

    return quint8(bits & mask);
}

// From Thetis HPSDR/Penny.cs:94-100 [@501e3f5] setTXPinAction
// (group parameter collapsed; NereusSDR tracks one action per pin)
OcMatrix::TXPinAction OcMatrix::pinAction(int pin) const
{
    if (pin < 0 || pin >= kPinCount) { return TXPinAction::MoxTuneTwoTone; }
    QReadLocker locker(&m_lock);
    return m_pinActions[pin];
}

// From Thetis HPSDR/Penny.cs:94-100 [@501e3f5] setTXPinAction
void OcMatrix::setPinAction(int pin, TXPinAction action)
{
    if (!m_loaded) { return; }
    if (pin < 0 || pin >= kPinCount) { return; }
    {
        QWriteLocker locker(&m_lock);
        if (m_pinActions[pin] == action) { return; }
        m_pinActions[pin] = action;
    }
    // Issue #191: persist immediately. The HF tab pin-action lambda calls
    // setPinAction() directly without an explicit save(); without this,
    // user edits never reach AppSettings and are lost on app restart.
    save();
    emit changed();
}

namespace {

// OC state is keyed by the identity that reaches the radio: the MAC from
// discovery, or MANUAL:ip:port for a hand-added entry. The same radio can
// arrive under either, and a MAC can arrive with different separators or
// letter case. Compare the two without separators so a spelling change is
// not mistaken for a different radio (which used to reload an empty matrix
// and then save it over the real pattern).
QString normalisedOcIdentity(QString identity)
{
    identity = identity.trimmed();
    if (identity.startsWith(QLatin1String("MANUAL"), Qt::CaseInsensitive)) {
        // Addresses are not MACs; keep the dots so two hosts cannot collide.
        return identity.toUpper();
    }
    identity.remove(QLatin1Char(':'));
    identity.remove(QLatin1Char('-'));
    identity.remove(QLatin1Char('_'));
    identity.remove(QLatin1Char('.'));
    return identity.toUpper();
}

} // namespace

void OcMatrix::setMacAddress(const QString& mac)
{
    if (m_mac == mac) {
        return;
    }
    // Same radio, different spelling: keep the spelling already used for
    // this radio's saved keys so saves cannot fork into a second block.
    if (!m_mac.isEmpty()
        && normalisedOcIdentity(m_mac) == normalisedOcIdentity(mac)) {
        return;
    }
    m_mac = mac;
    m_loaded = false;
}

// ── External PA (xPA) ──────────────────────────────────────────────────────
// From Thetis HPSDR/Penny.cs:60-91 [v2.10.3.15] TXPinPA / RXPinPA /
// setTXPinPA; Thetis stores one flag per band group, NereusSDR one per pin.
bool OcMatrix::txPinPa(int pin) const
{
    if (pin < 0 || pin >= kPinCount) { return false; }
    QReadLocker locker(&m_lock);
    return m_txPinPa[pin];
}

void OcMatrix::setTxPinPa(int pin, bool on)
{
    if (!m_loaded) { return; }
    if (pin < 0 || pin >= kPinCount) { return; }
    {
        QWriteLocker locker(&m_lock);
        if (m_txPinPa[pin] == on) { return; }
        m_txPinPa[pin] = on;
    }
    save();
    emit changed();
}

bool OcMatrix::rxPinPa(int pin) const
{
    if (pin < 0 || pin >= kPinCount) { return false; }
    QReadLocker locker(&m_lock);
    return m_rxPinPa[pin];
}

void OcMatrix::setRxPinPa(int pin, bool on)
{
    if (!m_loaded) { return; }
    if (pin < 0 || pin >= kPinCount) { return; }
    {
        QWriteLocker locker(&m_lock);
        if (m_rxPinPa[pin] == on) { return; }
        m_rxPinPa[pin] = on;
    }
    save();
    emit changed();
}

bool OcMatrix::extPaEnabled() const
{
    QReadLocker locker(&m_lock);
    return m_extPaEnabled;
}

// From Thetis console.cs:46667-46690 [v2.10.3.15]
// chkExternalPA_CheckedChanged: stores the button and re-pushes the OC bits
// (Penny.UpdateExtCtrl) so a PA-flagged pin opens or closes at once.
void OcMatrix::setExtPaEnabled(bool on)
{
    {
        QWriteLocker locker(&m_lock);
        if (m_extPaEnabled == on) { return; }
        m_extPaEnabled = on;
    }
    save();
    emit extPaEnabledChanged(on);
    emit changed();
}

bool OcMatrix::anyPinPa() const
{
    QReadLocker locker(&m_lock);
    for (int pin = 0; pin < kPinCount; ++pin) {
        if (m_txPinPa[pin] || m_rxPinPa[pin]) { return true; }
    }
    return false;
}

QString OcMatrix::persistenceKey() const
{
    return QStringLiteral("hardware/%1/oc").arg(m_mac);
}

// Stable persistence slug for TXPinAction values
// (slugs are persisted in AppSettings; must not change between releases)
QString OcMatrix::actionSlug(TXPinAction action)
{
    switch (action) {
    case TXPinAction::Mox:            return QStringLiteral("mox");
    case TXPinAction::Tune:           return QStringLiteral("tune");
    case TXPinAction::TwoTone:        return QStringLiteral("twotone");
    case TXPinAction::MoxTune:        return QStringLiteral("mox_tune");
    case TXPinAction::MoxTwoTone:     return QStringLiteral("mox_twotone");
    case TXPinAction::TuneTwoTone:    return QStringLiteral("tune_twotone");
    case TXPinAction::MoxTuneTwoTone: return QStringLiteral("mox_tune_twotone");
    default:                          return QStringLiteral("mox_tune_twotone");
    }
}

OcMatrix::TXPinAction OcMatrix::actionFromSlug(const QString& slug)
{
    if (slug == QStringLiteral("mox"))             { return TXPinAction::Mox; }
    if (slug == QStringLiteral("tune"))            { return TXPinAction::Tune; }
    if (slug == QStringLiteral("twotone"))         { return TXPinAction::TwoTone; }
    if (slug == QStringLiteral("mox_tune"))        { return TXPinAction::MoxTune; }
    if (slug == QStringLiteral("mox_twotone"))     { return TXPinAction::MoxTwoTone; }
    if (slug == QStringLiteral("tune_twotone"))    { return TXPinAction::TuneTwoTone; }
    if (slug == QStringLiteral("mox_tune_twotone")){ return TXPinAction::MoxTuneTwoTone; }
    return TXPinAction::MoxTuneTwoTone;  // default per Thetis Penny.cs:59
}

void OcMatrix::load()
{
    if (m_mac.isEmpty()) { return; }
    auto& s = AppSettings::instance();
    const QString base = persistenceKey();
    const bool hadLoaded = m_loaded;
    const bool userCleared =
        s.value(base + QStringLiteral("/userCleared"), QStringLiteral("False"))
            .toString() == QStringLiteral("True");

    // A station can be reached under its MAC (discovery) or a MANUAL:IP:PORT
    // placeholder (a saved manual entry), and the identity can differ between
    // sessions. When this identity has no matrix saved, adopt the only other
    // identity that has one (one radio) instead of starting empty and later
    // saving the empty matrix over the real pattern.
    QString readBase = base;
    bool adopt = false;
    bool baseHasData = false;
    QStringList otherBases;
    const QStringList all = s.allKeys();
    for (const QString& key : all) {
        if (!key.startsWith(QStringLiteral("hardware/")) || !key.contains(QStringLiteral("/oc/"))) {
            continue;
        }
        const QString owner = key.section(QLatin1Char('/'), 1, 1);
        const QString candidate = QStringLiteral("hardware/%1/oc").arg(owner);
        if (candidate == base) {
            baseHasData = true;
        } else if (!otherBases.contains(candidate)) {
            otherBases.append(candidate);
        }
    }

    // How many pins a candidate block actually has set. A block that exists
    // but holds only defaults is not a pattern worth adopting.
    const auto truePinCount = [&s](const QString& candidate) {
        int count = 0;
        for (int b = 0; b < kBandCount; ++b) {
            const QString bandSlug = bandKeyName(Band(b));
            for (int pin = 0; pin < kPinCount; ++pin) {
                for (int tx = 0; tx < 2; ++tx) {
                    const QString key = QStringLiteral("%1/%2/%3/pin%4")
                                            .arg(candidate,
                                                 tx ? QStringLiteral("tx")
                                                    : QStringLiteral("rx"),
                                                 bandSlug)
                                            .arg(pin + 1);
                    if (s.value(key, QStringLiteral("False")).toString()
                        == QStringLiteral("True")) {
                        ++count;
                    }
                }
            }
        }
        return count;
    };

    if ((!baseHasData || truePinCount(base) == 0) && !userCleared) {
        QString bestBase;
        int bestCount = 0;
        for (const QString& candidate : otherBases) {
            const int count = truePinCount(candidate);
            if (count > bestCount) {
                bestBase = candidate;
                bestCount = count;
            }
        }
        const QString owner = bestBase.section(QLatin1Char('/'), 1, 1);
        const bool sameRadio = !bestBase.isEmpty()
            && normalisedOcIdentity(owner) == normalisedOcIdentity(m_mac);
        if (sameRadio || (otherBases.size() == 1 && bestCount > 0)) {
            readBase = bestBase;
            adopt = true;
        } else if (hadLoaded && bestCount > 0) {
            // The identity spelling changed while this session already held
            // the pattern: keep the pattern and write it under the new name.
            readBase = bestBase;
            adopt = true;
        }
    }

    const bool haveSource = baseHasData || adopt;

    bool extPaBefore = false;
    {
        QReadLocker locker(&m_lock);
        extPaBefore = m_extPaEnabled;
    }

    if (haveSource) {
        QWriteLocker locker(&m_lock);

        // Hydrate per-band per-pin bits (RX and TX)
        // Key scheme: hardware/<mac>/oc/rx/<bandKey>/pin<n>  (n = 1..7)
        //             hardware/<mac>/oc/tx/<bandKey>/pin<n>
        for (int b = 0; b < kBandCount; ++b) {
            const QString bandSlug = bandKeyName(Band(b));
            for (int pin = 0; pin < kPinCount; ++pin) {
                const QString rxKey = QStringLiteral("%1/rx/%2/pin%3").arg(readBase, bandSlug).arg(pin + 1);
                const QString txKey = QStringLiteral("%1/tx/%2/pin%3").arg(readBase, bandSlug).arg(pin + 1);
                m_pins[b][pin][0] = s.value(rxKey, QStringLiteral("False")).toString() == QStringLiteral("True");
                m_pins[b][pin][1] = s.value(txKey, QStringLiteral("False")).toString() == QStringLiteral("True");
            }
        }

        // Hydrate per-pin TX actions
        // Key scheme: hardware/<mac>/oc/actions/pin<n>/action
        for (int pin = 0; pin < kPinCount; ++pin) {
            const QString k = QStringLiteral("%1/actions/pin%2/action").arg(readBase).arg(pin + 1);
            const QString defaultSlug = actionSlug(TXPinAction::MoxTuneTwoTone);
            const QString slug = s.value(k, defaultSlug).toString();
            m_pinActions[pin] = actionFromSlug(slug);
        }

        // Hydrate the external-PA (xPA) per-pin flags and the button state.
        for (int pin = 0; pin < kPinCount; ++pin) {
            const QString txKey = QStringLiteral("%1/extPa/txPin%2").arg(readBase).arg(pin + 1);
            const QString rxKey = QStringLiteral("%1/extPa/rxPin%2").arg(readBase).arg(pin + 1);
            m_txPinPa[pin] = s.value(txKey, QStringLiteral("False")).toString() == QStringLiteral("True");
            m_rxPinPa[pin] = s.value(rxKey, QStringLiteral("False")).toString() == QStringLiteral("True");
        }
        m_extPaEnabled =
            s.value(QStringLiteral("%1/extPa/enabled").arg(readBase),
                    QStringLiteral("False")).toString() == QStringLiteral("True");
    }

    // Loaded means "safe to save". An identity with no pattern of its own,
    // and no single unambiguous pattern to adopt, must not be marked loaded:
    // a later save would write defaults over a real matrix. A radio with no
    // OC keys anywhere at all is a fresh radio, where defaults are correct.
    const bool freshRadio = !baseHasData && otherBases.isEmpty();
    m_loaded = haveSource || freshRadio || hadLoaded;

    if (m_extPaEnabled != extPaBefore) {
        emit extPaEnabledChanged(m_extPaEnabled);
    }
    emit changed();

    // Adopted from the other identity: write it under this one so the next
    // session starts from the same pattern whatever identity it resolves.
    if (adopt) {
        save();
    }
}
void OcMatrix::save()
{
    if (m_mac.isEmpty()) { return; }
    // Never write an unloaded matrix: before load(), every cell reads as its
    // default (all false), and saving that erased the operator's pattern when
    // the radio identity changed between sessions (MAC vs MANUAL:IP:PORT).
    if (!m_loaded) { return; }
    auto& s = AppSettings::instance();
    const QString base = persistenceKey();
    const bool userCleared =
        s.value(base + QStringLiteral("/userCleared"), QStringLiteral("False"))
            .toString() == QStringLiteral("True");

    QReadLocker locker(&m_lock);

    // Never persist an all-zero pin matrix unless the operator cleared it
    // deliberately (resetDefaults, or unchecking the last pin). This is the
    // last line of defence against an unloaded or identity-mismatched
    // matrix wiping the saved pattern.
    int truePins = 0;
    for (int b = 0; b < kBandCount; ++b) {
        for (int pin = 0; pin < kPinCount; ++pin) {
            if (m_pins[b][pin][0]) { ++truePins; }
            if (m_pins[b][pin][1]) { ++truePins; }
        }
    }
    const bool writePins = truePins > 0 || userCleared;

    // R-R3-46: write only the keys whose saved value differs, reading them
    // with load()'s own defaults, so an unchanged cell reads back the same
    // either way. One pin click used to rewrite every band and pin (385
    // keys), which in a remote window is 385 writes to the Core, and on the
    // Core 385 settings updates back to the window.
    const auto store = [&s](const QString& key, const QString& value, const QString& fallback) {
        if (s.value(key, fallback).toString() != value) {
            s.setValue(key, value);
        }
    };
    const QString kTrue = QStringLiteral("True");
    const QString kFalse = QStringLiteral("False");
    if (writePins) {
        for (int b = 0; b < kBandCount; ++b) {
            const QString bandSlug = bandKeyName(Band(b));
            for (int pin = 0; pin < kPinCount; ++pin) {
                const QString rxKey = QStringLiteral("%1/rx/%2/pin%3").arg(base, bandSlug).arg(pin + 1);
                const QString txKey = QStringLiteral("%1/tx/%2/pin%3").arg(base, bandSlug).arg(pin + 1);
                store(rxKey, m_pins[b][pin][0] ? kTrue : kFalse, kFalse);
                store(txKey, m_pins[b][pin][1] ? kTrue : kFalse, kFalse);
            }
        }
    }

    const QString defaultSlug = actionSlug(TXPinAction::MoxTuneTwoTone);
    for (int pin = 0; pin < kPinCount; ++pin) {
        const QString k = QStringLiteral("%1/actions/pin%2/action").arg(base).arg(pin + 1);
        store(k, actionSlug(m_pinActions[pin]), defaultSlug);
    }

    // External PA (xPA): the per-pin PA flags and the button state, per radio.
    for (int pin = 0; pin < kPinCount; ++pin) {
        const QString txKey = QStringLiteral("%1/extPa/txPin%2").arg(base).arg(pin + 1);
        const QString rxKey = QStringLiteral("%1/extPa/rxPin%2").arg(base).arg(pin + 1);
        store(txKey, m_txPinPa[pin] ? kTrue : kFalse, kFalse);
        store(rxKey, m_rxPinPa[pin] ? kTrue : kFalse, kFalse);
    }
    store(QStringLiteral("%1/extPa/enabled").arg(base),
          m_extPaEnabled ? kTrue : kFalse, kFalse);
}

// Thetis Penny.cs:53-63 init: pins all 0; actions all MOX_TUNE_TWOTONE
// From Thetis HPSDR/Penny.cs:55-65 [@501e3f5]
void OcMatrix::resetDefaults()
{
    bool extPaWas = false;
    {
        QWriteLocker locker(&m_lock);
        for (auto& band : m_pins) {
            for (auto& pin : band) {
                for (auto& slot : pin) { slot = false; }
            }
        }
        m_pinActions.fill(TXPinAction::MoxTuneTwoTone);
        m_txPinPa.fill(false);
        m_rxPinPa.fill(false);
        extPaWas = m_extPaEnabled;
        m_extPaEnabled = false;
    }
    // Issue #191: persist immediately. Reset OC defaults button calls
    // resetDefaults() with no follow-up save(). Mark this as a deliberate
    // clear so the all-zero matrix is allowed to persist and later loads do
    // not adopt another identity's pattern back again.
    AppSettings::instance().setValue(
        persistenceKey() + QStringLiteral("/userCleared"), QStringLiteral("True"));
    save();
    if (extPaWas) { emit extPaEnabledChanged(false); }
    emit changed();
}

} // namespace NereusSDR
