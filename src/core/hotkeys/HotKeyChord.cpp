// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 1.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "core/hotkeys/HotKeyChord.h"

#include <QStringList>
#include <QStringView>

#include <array>

namespace NereusSDR {

namespace {

constexpr int kHighestFunctionKey = 35;  // Qt::Key_F35

bool* modifierFlag(HotKeyChord& chord, QStringView name)
{
    if (name.compare(QLatin1String("Ctrl"), Qt::CaseInsensitive) == 0) {
        return &chord.ctrl;
    }
    if (name.compare(QLatin1String("Shift"), Qt::CaseInsensitive) == 0) {
        return &chord.shift;
    }
    if (name.compare(QLatin1String("Alt"), Qt::CaseInsensitive) == 0) {
        return &chord.alt;
    }
    if (name.compare(QLatin1String("Meta"), Qt::CaseInsensitive) == 0) {
        return &chord.meta;
    }
    if (name.compare(QLatin1String("Num"), Qt::CaseInsensitive) == 0) {
        return &chord.num;
    }
    return nullptr;
}

// Qt's portable names. tst_hot_key_bindings checks each one against QKeySequence.
QString canonicalKey(QStringView text)
{
    if (text.size() == 1) {
        const QChar c = text.at(0);
        if (!c.isPrint() || c.isSpace()) {
            return {};
        }
        return QString(c.toUpper());
    }

    static const std::array<QLatin1String, 25> kNames = {
        QLatin1String("Esc"), QLatin1String("Tab"), QLatin1String("Backtab"), QLatin1String("Backspace"),
        QLatin1String("Return"), QLatin1String("Enter"), QLatin1String("Ins"), QLatin1String("Del"),
        QLatin1String("Pause"), QLatin1String("Print"), QLatin1String("SysReq"), QLatin1String("Home"),
        QLatin1String("End"), QLatin1String("Left"), QLatin1String("Up"), QLatin1String("Right"),
        QLatin1String("Down"), QLatin1String("PgUp"), QLatin1String("PgDown"), QLatin1String("CapsLock"),
        QLatin1String("NumLock"), QLatin1String("ScrollLock"), QLatin1String("Menu"), QLatin1String("Help"),
        QLatin1String("Space"),
    };
    for (const QLatin1String& name : kNames) {
        if (text.compare(name, Qt::CaseInsensitive) == 0) {
            return QString(name);
        }
    }

    if (text.size() >= 2 && (text.at(0) == QLatin1Char('F') || text.at(0) == QLatin1Char('f'))
        && text.at(1) != QLatin1Char('0') && text.at(1) != QLatin1Char('+')) {
        bool ok = false;
        const int number = text.sliced(1).toInt(&ok);
        if (ok && number >= 1 && number <= kHighestFunctionKey) {
            return QStringLiteral("F%1").arg(number);
        }
    }
    return {};
}

} // namespace

std::optional<HotKeyChord> HotKeyChord::parse(const QString& text)
{
    HotKeyChord chord;
    QStringView rest(text);
    while (true) {
        const qsizetype plus = rest.indexOf(QLatin1Char('+'));
        if (plus <= 0) {
            break;  // no modifier left; "+" on its own is a key
        }
        bool* const flag = modifierFlag(chord, rest.first(plus));
        if (flag == nullptr) {
            break;
        }
        if (*flag) {
            return std::nullopt;  // the same modifier twice
        }
        *flag = true;
        rest = rest.sliced(plus + 1);
    }

    chord.key = canonicalKey(rest);
    if (chord.key.isEmpty()) {
        return std::nullopt;
    }
    return chord;
}

QString HotKeyChord::toString() const
{
    QString text;
    if (ctrl) {
        text += QStringLiteral("Ctrl+");
    }
    if (shift) {
        text += QStringLiteral("Shift+");
    }
    if (alt) {
        text += QStringLiteral("Alt+");
    }
    if (meta) {
        text += QStringLiteral("Meta+");
    }
    if (num) {
        text += QStringLiteral("Num+");
    }
    return text + key;
}

bool HotKeyChord::hasNoModifier() const
{
    return !ctrl && !shift && !alt && !meta && !num;
}

bool HotKeyChord::isLetterOrNumberOrArrow() const
{
    if (key.size() == 1) {
        return key.at(0).isLetterOrNumber();
    }
    return key == QLatin1String("Left") || key == QLatin1String("Right") || key == QLatin1String("Up")
        || key == QLatin1String("Down");
}

} // namespace NereusSDR
