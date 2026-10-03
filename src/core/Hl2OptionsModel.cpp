// =================================================================
// src/core/Hl2OptionsModel.cpp  (NereusSDR)
// =================================================================
//
// Ported from mi0bot-Thetis source:
//   Project Files/Source/Console/setup.designer.cs:groupBoxHL2RXOptions
//   Project Files/Source/Console/setup.cs handlers (chk* / ud* family)
//   (mi0bot v2.10.3.13-beta2 / @c26a8a4)
//
// See Hl2OptionsModel.h for the full design + scope rationale.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — New for Phase 3L HL2 Filter visibility brainstorm.
//                J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Anthropic Claude Code.
//   2026-09-23 - R-R3-46: save() writes only changed keys, so a remote
//                window never rewrites the transmit timings. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
//
//=================================================================
// setup.cs (mi0bot fork)
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
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves his       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "Hl2OptionsModel.h"

#include "core/AppSettings.h"

#include <QtGlobal>

#include <cmath>

namespace NereusSDR {

namespace {
constexpr const char* kKeySwapAudio   = "hl2/swapAudioChannels";
constexpr const char* kKeyCl2Enable   = "hl2/cl2Enable";
constexpr const char* kKeyCl2FreqMHz  = "hl2/cl2FreqMHz";
constexpr const char* kKeyExt10MHz    = "hl2/ext10MHz";
constexpr const char* kKeyDiscReset   = "hl2/disconnectReset";
constexpr const char* kKeyPttHang     = "hl2/pttHangMs";
constexpr const char* kKeyTxLatency   = "hl2/txLatencyMs";
constexpr const char* kKeyPsSync      = "hl2/psSync";
constexpr const char* kKeyBandVolts   = "hl2/bandVolts";

QString boolStr(bool on) { return on ? QStringLiteral("True") : QStringLiteral("False"); }
bool   strBool(const QVariant& v, bool def)
{
    if (!v.isValid()) { return def; }
    return v.toString().compare(QStringLiteral("True"), Qt::CaseInsensitive) == 0;
}
} // namespace

bool Hl2OptionsModel::parseCl2FreqMHz(const QString& text, int* kHz)
{
    bool ok = false;
    const double mhz = text.trimmed().toDouble(&ok);
    // The range bound keeps the kHz product inside int before the caller
    // checks the real range.
    if (!ok || !std::isfinite(mhz) || std::fabs(mhz) > 1.0e6) {
        return false;
    }
    if (kHz) {
        *kHz = static_cast<int>(std::llround(mhz * 1000.0));
    }
    return true;
}

QString Hl2OptionsModel::cl2FreqMHzText(int kHz)
{
    // 'g' with ten digits: "116", "24.576", "10.7", the text a remote
    // window's decimal value turns into on the Core.
    return QString::number(kHz / 1000.0, 'g', 10);
}

Hl2OptionsModel::Hl2OptionsModel(QObject* parent)
    : QObject(parent)
{
}

void Hl2OptionsModel::setMacAddress(const QString& mac)
{
    m_mac = mac;
}

void Hl2OptionsModel::load()
{
    if (m_mac.isEmpty()) { return; }
    auto& s = AppSettings::instance();

    m_swapAudioChannels = strBool(s.hardwareValue(m_mac, QString::fromLatin1(kKeySwapAudio)), false);
    m_cl2Enabled        = strBool(s.hardwareValue(m_mac, QString::fromLatin1(kKeyCl2Enable)), false);
    // A saved frequency that is not a number loads the default, not the
    // bottom of the range; one outside the range is clamped to it.
    int cl2FreqKHz = kDefaultCl2FreqKHz;
    if (!parseCl2FreqMHz(s.hardwareValue(m_mac, QString::fromLatin1(kKeyCl2FreqMHz),
                                         cl2FreqMHzText(kDefaultCl2FreqKHz)).toString(),
                         &cl2FreqKHz)) {
        cl2FreqKHz = kDefaultCl2FreqKHz;
    }
    m_cl2FreqKHz        = qBound(kCl2FreqMinKHz, cl2FreqKHz, kCl2FreqMaxKHz);
    m_ext10MHz          = strBool(s.hardwareValue(m_mac, QString::fromLatin1(kKeyExt10MHz)), false);
    m_disconnectReset   = strBool(s.hardwareValue(m_mac, QString::fromLatin1(kKeyDiscReset)), false);
    m_pttHangMs         = qBound(kPttHangMinMs,
        s.hardwareValue(m_mac, QString::fromLatin1(kKeyPttHang),
                        kDefaultPttHangMs).toInt(), kPttHangMaxMs);
    m_txLatencyMs       = qBound(kTxLatencyMinMs,
        s.hardwareValue(m_mac, QString::fromLatin1(kKeyTxLatency),
                        kDefaultTxLatencyMs).toInt(), kTxLatencyMaxMs);
    m_psSync            = strBool(s.hardwareValue(m_mac, QString::fromLatin1(kKeyPsSync)), false);
    m_bandVolts         = strBool(s.hardwareValue(m_mac, QString::fromLatin1(kKeyBandVolts)), false);

    emit changed();
}

void Hl2OptionsModel::save() const
{
    if (m_mac.isEmpty()) { return; }
    auto& s = AppSettings::instance();

    // R-R3-46: write only the keys whose saved value differs, read with
    // load()'s own defaults (as OcMatrix::save does). Every setter saves,
    // so in a remote window one checkbox used to rewrite all nine keys on
    // the Core, including the two transmit timings a receive-only Core
    // refuses (StationServer's transmit hardware keys).
    const auto store = [this, &s](const char* key, const QString& value,
                                  const QString& fallback) {
        const QString name = QString::fromLatin1(key);
        if (s.hardwareValue(m_mac, name, fallback).toString() != value) {
            s.setHardwareValue(m_mac, name, value);
        }
    };
    const QString off = boolStr(false);
    store(kKeySwapAudio,  boolStr(m_swapAudioChannels), off);
    store(kKeyCl2Enable,  boolStr(m_cl2Enabled),        off);
    store(kKeyCl2FreqMHz, cl2FreqMHzText(m_cl2FreqKHz),
          cl2FreqMHzText(kDefaultCl2FreqKHz));
    store(kKeyExt10MHz,   boolStr(m_ext10MHz),          off);
    store(kKeyDiscReset,  boolStr(m_disconnectReset),   off);
    store(kKeyPttHang,    QString::number(m_pttHangMs),
          QString::number(kDefaultPttHangMs));
    store(kKeyTxLatency,  QString::number(m_txLatencyMs),
          QString::number(kDefaultTxLatencyMs));
    store(kKeyPsSync,     boolStr(m_psSync),            off);
    store(kKeyBandVolts,  boolStr(m_bandVolts),         off);
}

#define NEREUS_SETTER_BOOL(method, member, signalName) \
void Hl2OptionsModel::method(bool on)                  \
{                                                       \
    if (member == on) { return; }                       \
    member = on;                                        \
    emit signalName(on);                                \
    emit changed();                                     \
    save();                                             \
}

#define NEREUS_SETTER_INT(method, member, signalName, mn, mx) \
void Hl2OptionsModel::method(int v)                            \
{                                                              \
    const int clamped = qBound(mn, v, mx);                     \
    if (member == clamped) { return; }                         \
    member = clamped;                                          \
    emit signalName(clamped);                                  \
    emit changed();                                            \
    save();                                                    \
}

NEREUS_SETTER_BOOL(setSwapAudioChannels, m_swapAudioChannels, swapAudioChannelsChanged)
NEREUS_SETTER_BOOL(setCl2Enabled,        m_cl2Enabled,        cl2EnabledChanged)
NEREUS_SETTER_INT (setCl2FreqKHz,        m_cl2FreqKHz,        cl2FreqKHzChanged,
                   kCl2FreqMinKHz, kCl2FreqMaxKHz)
NEREUS_SETTER_BOOL(setExt10MHz,          m_ext10MHz,          ext10MHzChanged)
NEREUS_SETTER_BOOL(setDisconnectReset,   m_disconnectReset,   disconnectResetChanged)
NEREUS_SETTER_INT (setPttHangMs,         m_pttHangMs,         pttHangMsChanged,
                   kPttHangMinMs, kPttHangMaxMs)
NEREUS_SETTER_INT (setTxLatencyMs,       m_txLatencyMs,       txLatencyMsChanged,
                   kTxLatencyMinMs, kTxLatencyMaxMs)
NEREUS_SETTER_BOOL(setPsSync,            m_psSync,            psSyncChanged)
NEREUS_SETTER_BOOL(setBandVolts,         m_bandVolts,         bandVoltsChanged)

#undef NEREUS_SETTER_BOOL
#undef NEREUS_SETTER_INT

} // namespace NereusSDR
