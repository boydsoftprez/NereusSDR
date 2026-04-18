#pragma once

// =================================================================
// src/core/mmio/FormatParser.h  (NereusSDR)
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

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QUuid>
#include <QVariant>

namespace NereusSDR {

// Phase 3G-6 block 5 — format-driven parsers for MMIO transport
// payloads. Each function consumes a raw payload and returns a
// flat key->value map where keys are dotted paths prefixed with
// the endpoint's guid string, matching Thetis's discovery scheme
// from MeterManager.cs:41325-41422.
//
// Keys look like "550e8400-e29b-41d4-a716-446655440000.gateway.temperature".
// The guid prefix keeps variables from different endpoints distinct
// inside the shared per-endpoint cache.
//
// These are free functions with no class; the functional form keeps
// parsers side-effect-free and trivially testable.
namespace FormatParser {

// JSON recursive descent. Every leaf node becomes a key whose path
// reflects nested object / array structure. Numbers become double /
// int, bool becomes bool, strings become QString. Null values are
// skipped. From Thetis MeterManager.cs:41325-41402.
QHash<QString, QVariant> parseJson(const QByteArray& payload, const QUuid& endpointGuid);

// XML element walk. Element text content and attributes become keys.
// From Thetis MeterManager.cs:41404-41422.
QHash<QString, QVariant> parseXml(const QByteArray& payload, const QUuid& endpointGuid);

// RAW colon-delimited key:value pairs. Values are stored as QString.
// Numeric auto-detection happens downstream if the consumer needs it.
// From Thetis MeterManager.cs:41344-41349.
QHash<QString, QVariant> parseRaw(const QByteArray& payload, const QUuid& endpointGuid);

} // namespace FormatParser

} // namespace NereusSDR
