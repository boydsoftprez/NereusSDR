// =================================================================
// src/core/mmio/FourChar.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/common.cs, original licence from
//   Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-25 — Reimplemented in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code. Ports Common.FourChar and
//                 convertToFourChar, the 4-character MMIO endpoint ID.
// =================================================================

//=================================================================
// common.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2012  FlexRadio Systems
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
// You may contact us via email at: gpl@flexradio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    4616 W. Howard Lane  Suite 1-150
//    Austin, TX 78728
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

#include "FourChar.h"

#include <QByteArray>
#include <QCryptographicHash>

#include <array>

namespace NereusSDR {

namespace {

// From Thetis common.cs:1274 [v2.10.3.15] — the 62-character alphabet.
constexpr char kFourCharAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
constexpr int kFourCharAlphabetLength = static_cast<int>(sizeof(kFourCharAlphabet)) - 1;

// From Thetis common.cs:1272-1286 [v2.10.3.15] — convertToFourChar.
QString convertToFourChar(const QByteArray& base64Hash)
{
    std::array<int, 4> indices{0, 0, 0, 0};
    for (int i = 0; i < base64Hash.size(); ++i) {
        indices[i % 4] = (indices[i % 4] + static_cast<unsigned char>(base64Hash.at(i)))
                         % kFourCharAlphabetLength;
    }
    QString result;
    result.reserve(4);
    for (int i = 0; i < 4; ++i) {
        result.append(QLatin1Char(kFourCharAlphabet[indices[i]]));
    }
    return result;
}

} // namespace

// From Thetis common.cs:1261-1270 [v2.10.3.15] — FourChar.
QString fourChar(const QString& data1, int data2, const QUuid& guid)
{
    const QString input = data1 + QLatin1Char(':') + QString::number(data2)
                        + QLatin1Char(':') + guid.toString(QUuid::WithoutBraces);
    const QByteArray hashBytes =
        QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha256);
    return convertToFourChar(hashBytes.toBase64());
}

} // namespace NereusSDR
