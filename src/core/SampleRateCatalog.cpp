// =================================================================
// src/core/SampleRateCatalog.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// See SampleRateCatalog.h for cited line ranges and the buffer-size note.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

//=================================================================
// setup.cs
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
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
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

#include "SampleRateCatalog.h"

#include "AppSettings.h"
#include "LogCategories.h"

#include <QString>
#include <QVariant>

#include <algorithm>

namespace NereusSDR {

std::vector<int> allowedSampleRates(ProtocolVersion proto,
                                     const BoardCapabilities& caps,
                                     HPSDRModel model)
{
    // Plan Task 5: the per-protocol rate ladder lives beside the board row
    // (BoardCapsTable::sampleRatesFor), with the Thetis setup.cs:847-851
    // and mi0bot HL2 cites. This name stays for its existing callers.
    return BoardCapsTable::sampleRatesFor(caps, proto, model);
}

int defaultSampleRate(ProtocolVersion proto,
                      const BoardCapabilities& caps,
                      HPSDRModel model)
{
    const auto allowed = allowedSampleRates(proto, caps, model);
    if (allowed.empty()) {
        // Board registry is broken — caller should never hit this, but
        // returning 0 is safer than reading past end. Log and let the
        // caller decide.
        qCWarning(lcConnection) << "defaultSampleRate: no allowed rates for"
                                 << static_cast<int>(proto) << static_cast<int>(model);
        return 0;
    }
    if (std::find(allowed.begin(), allowed.end(), kDefaultSampleRate) != allowed.end()) {
        return kDefaultSampleRate;
    }
    return allowed.front();
}

int resolveSampleRate(const AppSettings& settings,
                      const QString& mac,
                      ProtocolVersion proto,
                      const BoardCapabilities& caps,
                      HPSDRModel model)
{
    const int persisted = settings.hardwareValue(
        mac, QStringLiteral("radioInfo/sampleRate")).toInt();
    if (persisted <= 0) {
        return defaultSampleRate(proto, caps, model);
    }
    const auto allowed = allowedSampleRates(proto, caps, model);
    if (std::find(allowed.begin(), allowed.end(), persisted) == allowed.end()) {
        qCWarning(lcConnection) << "Persisted sample rate" << persisted
                                 << "not valid for" << mac
                                 << "— falling back to default";
        return defaultSampleRate(proto, caps, model);
    }
    return persisted;
}

int resolveActiveRxCount(const AppSettings& settings,
                         const QString& mac,
                         const BoardCapabilities& caps)
{
    const int persisted = settings.hardwareValue(
        mac, QStringLiteral("radioInfo/activeRxCount")).toInt();
    if (persisted < 1) {
        return 1;
    }
    return std::min(persisted, caps.maxReceivers);
}

} // namespace NereusSDR
