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
    const int b = int(band);
    if (b < 0 || b >= kBandCount || pin < 0 || pin >= kPinCount) { return; }
    {
        QWriteLocker locker(&m_lock);
        auto& slot = m_pins[b][pin][tx ? 1 : 0];
        if (slot == enabled) { return; }
        slot = enabled;
    }
    // Issue #191: persist immediately. The HF / SWL OC tab toggle handlers
    // call setPin() directly and don't follow up with save(); without this,
    // user edits never reach AppSettings and are lost on app restart.
    save();
    emit changed();
}

// Derived from Thetis HPSDR/Penny.cs:264-275 [@501e3f5] mask construction loop
// (for(int nPin=0; nPin<7; nPin++) mask |= nBit << nPin)
quint8 OcMatrix::maskFor(Band band, bool tx) const
{
    const int b = int(band);
    if (b < 0 || b >= kBandCount) { return 0; }
    QReadLocker locker(&m_lock);
    quint8 mask = 0;
    for (int pin = 0; pin < kPinCount; ++pin) {
        if (m_pins[b][pin][tx ? 1 : 0]) { mask |= quint8(1 << pin); }
    }
    return mask;
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

void OcMatrix::setMacAddress(const QString& mac)
{
    m_mac = mac;
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

    {
        QWriteLocker locker(&m_lock);

        // Hydrate per-band per-pin bits (RX and TX)
        // Key scheme: hardware/<mac>/oc/rx/<bandKey>/pin<n>  (n = 1..7)
        //             hardware/<mac>/oc/tx/<bandKey>/pin<n>
        for (int b = 0; b < kBandCount; ++b) {
            const QString bandSlug = bandKeyName(Band(b));
            for (int pin = 0; pin < kPinCount; ++pin) {
                const QString rxKey = QStringLiteral("%1/rx/%2/pin%3").arg(base, bandSlug).arg(pin + 1);
                const QString txKey = QStringLiteral("%1/tx/%2/pin%3").arg(base, bandSlug).arg(pin + 1);
                m_pins[b][pin][0] = s.value(rxKey, QStringLiteral("False")).toString() == QStringLiteral("True");
                m_pins[b][pin][1] = s.value(txKey, QStringLiteral("False")).toString() == QStringLiteral("True");
            }
        }

        // Hydrate per-pin TX actions
        // Key scheme: hardware/<mac>/oc/actions/pin<n>/action
        for (int pin = 0; pin < kPinCount; ++pin) {
            const QString k = QStringLiteral("%1/actions/pin%2/action").arg(base).arg(pin + 1);
            const QString defaultSlug = actionSlug(TXPinAction::MoxTuneTwoTone);
            const QString slug = s.value(k, defaultSlug).toString();
            m_pinActions[pin] = actionFromSlug(slug);
        }
    }

    emit changed();
}

void OcMatrix::save()
{
    if (m_mac.isEmpty()) { return; }
    auto& s = AppSettings::instance();
    const QString base = persistenceKey();

    QReadLocker locker(&m_lock);

    for (int b = 0; b < kBandCount; ++b) {
        const QString bandSlug = bandKeyName(Band(b));
        for (int pin = 0; pin < kPinCount; ++pin) {
            const QString rxKey = QStringLiteral("%1/rx/%2/pin%3").arg(base, bandSlug).arg(pin + 1);
            const QString txKey = QStringLiteral("%1/tx/%2/pin%3").arg(base, bandSlug).arg(pin + 1);
            s.setValue(rxKey, m_pins[b][pin][0] ? QStringLiteral("True") : QStringLiteral("False"));
            s.setValue(txKey, m_pins[b][pin][1] ? QStringLiteral("True") : QStringLiteral("False"));
        }
    }

    for (int pin = 0; pin < kPinCount; ++pin) {
        const QString k = QStringLiteral("%1/actions/pin%2/action").arg(base).arg(pin + 1);
        s.setValue(k, actionSlug(m_pinActions[pin]));
    }
}

// Thetis Penny.cs:53-63 init: pins all 0; actions all MOX_TUNE_TWOTONE
// From Thetis HPSDR/Penny.cs:55-65 [@501e3f5]
void OcMatrix::resetDefaults()
{
    {
        QWriteLocker locker(&m_lock);
        for (auto& band : m_pins) {
            for (auto& pin : band) {
                for (auto& slot : pin) { slot = false; }
            }
        }
        m_pinActions.fill(TXPinAction::MoxTuneTwoTone);
    }
    // Issue #191: persist immediately. Reset OC defaults button calls
    // resetDefaults() with no follow-up save().
    save();
    emit changed();
}

} // namespace NereusSDR
