// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 1.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// A key chord held as text in Qt's portable form. Core code cannot use
// QKeySequence (Rule R1), so this parses the text itself. Qt writes its
// modifiers in another order; parse() accepts any order and toString() gives
// the one canonical text that bindings store and compare.
#pragma once

#include "core/NereusCoreExport.h"

#include <QString>

#include <optional>

namespace NereusSDR {

struct NEREUS_CORE_EXPORT HotKeyChord {
    bool ctrl{false};
    bool shift{false};
    bool alt{false};
    bool meta{false};
    bool num{false};  // keypad
    QString key;      // a Qt portable key name; letters are upper case

    // Empty for text that is not one modifier list and one known key.
    static std::optional<HotKeyChord> parse(const QString& text);

    QString toString() const;  // canonical order Ctrl+Shift+Alt+Meta+Num+Key
    bool hasNoModifier() const;
    bool isLetterOrNumberOrArrow() const;  // looks at the key, not the modifiers
    bool operator==(const HotKeyChord&) const = default;
};

} // namespace NereusSDR
