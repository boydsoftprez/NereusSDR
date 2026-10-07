// no-port-check: NereusSDR-original. The dialects a CAT virtual serial port speaks.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/core/cat/CatPtyDialects.h  (NereusSDR)
// =================================================================
//
// Modification history (NereusSDR):
//   2026-10-07  J.J. Boyd / KG4VCF  Created: one list of the PTY dialects
//                                    for the settings check, the PTY
//                                    transport, the local CAT page and the
//                                    Core's platform property, which each
//                                    held their own copy before.
//                                    AI tooling: Claude Code.
// =================================================================

#pragma once

#include <QString>
#include <QStringList>

#include <array>

namespace NereusSDR {

/// One dialect a CAT virtual serial port speaks: the value a channel's
/// `ptyDialect` holds, and plain words for a client that cannot show the
/// value itself.
struct CatPtyDialect {
    const char* value;
    const char* label;
};

/// Every dialect, in the order the CAT pages offer them. The first is a
/// channel's default (CatEndpointConfig::ptyDialect).
inline constexpr std::array<CatPtyDialect, 2> kCatPtyDialects{{
    {"Thetis", "Kenwood and ZZ commands"},
    {"Rigctld", "Hamlib rigctld"},
}};

/// Whether `value` names one of kCatPtyDialects.
inline bool isCatPtyDialect(const QString& value)
{
    for (const CatPtyDialect& dialect : kCatPtyDialects) {
        if (value == QLatin1String(dialect.value)) {
            return true;
        }
    }
    return false;
}

/// The dialects' values, in order.
inline QStringList catPtyDialectValues()
{
    QStringList values;
    for (const CatPtyDialect& dialect : kCatPtyDialects) {
        values.append(QString::fromLatin1(dialect.value));
    }
    return values;
}

} // namespace NereusSDR
