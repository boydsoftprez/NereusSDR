// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 1.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "core/hotkeys/HotKeyBindings.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyChord.h"

#include <QSet>

#include <algorithm>

#include <optional>
#include <utility>

namespace NereusSDR {

namespace {

constexpr QChar kKeySeparator = QLatin1Char(' ');
constexpr QChar kFieldSeparator = QLatin1Char('\t');

const QString& enabledKey()
{
    static const QString kKey = QStringLiteral("HotKeys/Enabled");
    return kKey;
}

const QString& bindingPrefix()
{
    static const QString kPrefix = QStringLiteral("HotKeys/Binding/");
    return kPrefix;
}

const QString& pinPrefix()
{
    static const QString kPrefix = QStringLiteral("HotKeys/Pin/");
    return kPrefix;
}

const QString& exportHeader()
{
    static const QString kHeader = QStringLiteral("# NereusSDR shortcuts 1");
    return kHeader;
}

// Design section 8: "Transmit, CW or voice action". This goes by group, so the
// two stop actions, which do not transmit, follow the same rule.
bool plainKeysRefused(const HotKeyAction& action)
{
    return action.transmits || action.group == QLatin1String("CW") || action.group == QLatin1String("Voice");
}

bool stayOnAfterRelease(const QString& id)
{
    return id == QLatin1String("tx.mox") || id == QLatin1String("tx.tune") || id == QLatin1String("tx.twotone");
}

// The keys an action may not take, whoever holds them now. The keypad flag does
// not make a key safe: Num+5 is still a plain number to the hand pressing it.
std::optional<HotKeyBindOutcome> refusal(const HotKeyAction& action, const HotKeyChord& chord)
{
    const QString text = chord.toString();

    const QString menuItem = HotKeyBindings::reservedBy(text);
    if (!menuItem.isEmpty()) {
        return HotKeyBindOutcome{HotKeyBindResult::RefusedReserved,
                                 QStringLiteral("%1 is the %2 menu shortcut.").arg(text, menuItem), {}};
    }

    const bool noHandModifier = !chord.ctrl && !chord.shift && !chord.alt && !chord.meta;
    if (plainKeysRefused(action) && noHandModifier && chord.isLetterOrNumberOrArrow()) {
        return HotKeyBindOutcome{
            HotKeyBindResult::RefusedTransmitPlainKey,
            QStringLiteral("%1 is a plain key, so a stray press would transmit. "
                           "Use Space, a function key or a combination.").arg(text),
            {}};
    }

    if (stayOnAfterRelease(action.id) && text == QLatin1String("Space")) {
        return HotKeyBindOutcome{
            HotKeyBindResult::RefusedLatchedOnSpace,
            QStringLiteral("%1 stays on after you let go, so it needs a combination.").arg(action.name),
            {}};
    }

    return std::nullopt;
}

QStringList splitKeys(const QString& joined)
{
    return joined.split(kKeySeparator, Qt::SkipEmptyParts);
}

QChar validPin(QChar letter)
{
    const QChar upper = letter.toUpper();
    if (upper >= QLatin1Char('A') && upper <= QLatin1Char('D')) {
        return upper;
    }
    return {};
}

bool sameKeys(const QStringList& a, const QStringList& b)
{
    return QSet<QString>(a.cbegin(), a.cend()) == QSet<QString>(b.cbegin(), b.cend());
}

} // namespace

HotKeyBindings::HotKeyBindings(QObject* parent)
    : QObject(parent)
{
    setDefaults();
    rebuildChordIndex();
}

void HotKeyBindings::setDefaults()
{
    m_keys.clear();
    m_pins.clear();
    for (const HotKeyAction& action : HotKeyActionList::all()) {
        m_keys.insert(action.id, action.defaultKeys);
    }
}

void HotKeyBindings::rebuildChordIndex()
{
    m_actionByChord.clear();
    for (const HotKeyAction& action : HotKeyActionList::all()) {
        const QStringList keys = m_keys.value(action.id);
        for (const QString& key : keys) {
            m_actionByChord.insert(key, action.id);
        }
    }
}

void HotKeyBindings::load()
{
    AppSettings& settings = AppSettings::instance();

    const bool wasEnabled = m_enabled;
    m_enabled = settings.value(enabledKey(), QStringLiteral("True")).toString() == QLatin1String("True");

    // A setting for an action that no longer exists goes away for good.
    const QStringList allKeys = settings.allKeys();
    for (const QString& key : allKeys) {
        QString id;
        if (key.startsWith(bindingPrefix())) {
            id = key.mid(bindingPrefix().size());
        } else if (key.startsWith(pinPrefix())) {
            id = key.mid(pinPrefix().size());
        } else {
            continue;
        }
        if (HotKeyActionList::find(id) == nullptr) {
            qCWarning(lcHotKeys) << "Dropped a saved setting for an unknown action:" << key;
            settings.remove(key);
        }
    }

    setDefaults();

    // Saved keys first, so a key the user chose beats a default that later
    // moved onto it.
    QHash<QString, QString> savedOwner;
    QSet<QString> saved;
    for (const HotKeyAction& action : HotKeyActionList::all()) {
        const QString key = bindingPrefix() + action.id;
        if (!settings.contains(key)) {
            continue;
        }
        saved.insert(action.id);
        QStringList kept;
        const QStringList stored = splitKeys(settings.value(key).toString());
        for (const QString& text : stored) {
            const std::optional<HotKeyChord> chord = HotKeyChord::parse(text);
            if (!chord.has_value() || refusal(action, *chord).has_value()) {
                qCWarning(lcHotKeys) << "Dropped a saved key that is not allowed:" << action.id << text;
                continue;
            }
            const QString canonical = chord->toString();
            if (savedOwner.contains(canonical)) {
                qCWarning(lcHotKeys) << "Dropped a saved key another action holds:" << action.id << text;
                continue;
            }
            savedOwner.insert(canonical, action.id);
            kept.append(canonical);
        }
        m_keys.insert(action.id, kept);
    }
    for (const HotKeyAction& action : HotKeyActionList::all()) {
        if (saved.contains(action.id)) {
            continue;
        }
        QStringList& keys = m_keys[action.id];
        keys.removeIf([&savedOwner](const QString& k) { return savedOwner.contains(k); });
    }

    for (const HotKeyAction& action : HotKeyActionList::all()) {
        const QString key = pinPrefix() + action.id;
        if (!settings.contains(key)) {
            continue;
        }
        const QString text = settings.value(key).toString();
        const QChar pin = text.size() == 1 ? validPin(text.at(0)) : QChar();
        if (pin.isNull() || action.scope != HotKeyScope::Slice) {
            qCWarning(lcHotKeys) << "Dropped a saved pin that is not allowed:" << action.id << text;
            continue;
        }
        m_pins.insert(action.id, pin);
    }

    rebuildChordIndex();
    emit bindingsChanged();
    if (wasEnabled != m_enabled) {
        emit enabledChanged(m_enabled);
    }
}

void HotKeyBindings::save()
{
    AppSettings& settings = AppSettings::instance();

    if (m_enabled) {
        settings.remove(enabledKey());
    } else {
        settings.setValue(enabledKey(), QStringLiteral("False"));
    }

    for (const HotKeyAction& action : HotKeyActionList::all()) {
        const QStringList keys = m_keys.value(action.id);
        const QString bindingKey = bindingPrefix() + action.id;
        if (sameKeys(keys, action.defaultKeys)) {
            settings.remove(bindingKey);
        } else {
            settings.setValue(bindingKey, keys.join(kKeySeparator));
        }

        const QString pinKey = pinPrefix() + action.id;
        if (m_pins.contains(action.id)) {
            settings.setValue(pinKey, QString(m_pins.value(action.id)));
        } else {
            settings.remove(pinKey);
        }
    }

    settings.save();
}

QStringList HotKeyBindings::keysFor(const QString& id) const
{
    return m_keys.value(id);
}

QString HotKeyBindings::actionForChord(const QString& chord) const
{
    const std::optional<HotKeyChord> parsed = HotKeyChord::parse(chord);
    if (!parsed.has_value()) {
        return {};
    }
    return m_actionByChord.value(parsed->toString());
}

QChar HotKeyBindings::pinFor(const QString& id) const
{
    return m_pins.value(id);
}

void HotKeyBindings::setPin(const QString& id, QChar letter)
{
    const HotKeyAction* const action = HotKeyActionList::find(id);
    if (action == nullptr) {
        qCWarning(lcHotKeys) << "Pin for an unknown action ignored:" << id;
        return;
    }
    if (letter.isNull()) {
        if (m_pins.remove(id) > 0) {
            emit bindingsChanged();
        }
        return;
    }
    const QChar pin = validPin(letter);
    if (pin.isNull() || action->scope != HotKeyScope::Slice) {
        qCWarning(lcHotKeys) << "Pin that is not allowed ignored:" << id << letter;
        return;
    }
    if (m_pins.value(id) != pin) {
        m_pins.insert(id, pin);
        emit bindingsChanged();
    }
}

HotKeyBindOutcome HotKeyBindings::bind(const QString& id, const QString& chord, bool replaceConflict)
{
    const HotKeyAction* const action = HotKeyActionList::find(id);
    if (action == nullptr) {
        return {HotKeyBindResult::RefusedInvalid, QStringLiteral("There is no action named %1.").arg(id), {}};
    }
    const std::optional<HotKeyChord> parsed = HotKeyChord::parse(chord);
    if (!parsed.has_value()) {
        return {HotKeyBindResult::RefusedInvalid, QStringLiteral("%1 is not a key.").arg(chord), {}};
    }
    if (const std::optional<HotKeyBindOutcome> refused = refusal(*action, *parsed); refused.has_value()) {
        return *refused;
    }

    const QString canonical = parsed->toString();
    const QString owner = m_actionByChord.value(canonical);
    if (!owner.isEmpty() && owner != id) {
        if (!replaceConflict) {
            const HotKeyAction* const ownerAction = HotKeyActionList::find(owner);
            return {HotKeyBindResult::Conflict,
                    QStringLiteral("%1 is already %2.").arg(canonical, ownerAction->name), owner};
        }
        m_keys[owner].removeAll(canonical);
    }

    const QStringList before = m_keys.value(id);
    m_keys.insert(id, QStringList{canonical});
    rebuildChordIndex();
    if (before != m_keys.value(id) || (!owner.isEmpty() && owner != id)) {
        emit bindingsChanged();
    }
    return {HotKeyBindResult::Bound, {}, {}};
}

void HotKeyBindings::clear(const QString& id)
{
    if (HotKeyActionList::find(id) == nullptr || m_keys.value(id).isEmpty()) {
        return;
    }
    m_keys.insert(id, QStringList{});
    rebuildChordIndex();
    emit bindingsChanged();
}

void HotKeyBindings::resetToDefault(const QString& id)
{
    const HotKeyAction* const action = HotKeyActionList::find(id);
    if (action == nullptr) {
        return;
    }
    const auto keysBefore = m_keys;
    const auto pinsBefore = m_pins;
    for (const QString& key : action->defaultKeys) {
        const QString owner = m_actionByChord.value(key);
        if (!owner.isEmpty() && owner != id) {
            m_keys[owner].removeAll(key);
        }
    }
    m_keys.insert(id, action->defaultKeys);
    m_pins.remove(id);
    rebuildChordIndex();
    if (keysBefore != m_keys || pinsBefore != m_pins) {
        emit bindingsChanged();
    }
}

void HotKeyBindings::resetAll()
{
    const auto keysBefore = m_keys;
    const auto pinsBefore = m_pins;
    setDefaults();
    rebuildChordIndex();
    if (keysBefore != m_keys || pinsBefore != m_pins) {
        emit bindingsChanged();
    }
}

bool HotKeyBindings::enabled() const
{
    return m_enabled;
}

void HotKeyBindings::setEnabled(bool on)
{
    if (m_enabled == on) {
        return;
    }
    m_enabled = on;
    emit enabledChanged(on);
}

QString HotKeyBindings::exportText() const
{
    QString text = exportHeader() + QLatin1Char('\n');
    for (const HotKeyAction& action : HotKeyActionList::all()) {
        text += action.id + kFieldSeparator + m_keys.value(action.id).join(kKeySeparator) + kFieldSeparator;
        if (m_pins.contains(action.id)) {
            text += m_pins.value(action.id);
        }
        text += QLatin1Char('\n');
    }
    return text;
}

QStringList HotKeyBindings::importText(const QString& text)
{
    struct GoodLine {
        QString id;
        QStringList keys;
        QChar pin;
    };
    QList<GoodLine> good;
    QHash<QString, QString> claimedBy;
    QStringList skipped;

    const auto skip = [&skipped](const QString& line, const QString& reason) {
        QString shown = line;
        shown.replace(kFieldSeparator, kKeySeparator);
        skipped.append(QStringLiteral("%1: %2").arg(shown.trimmed(), reason));
    };

    const QStringList lines = text.split(QLatin1Char('\n'));
    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
        if (line.trimmed().isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        const QStringList fields = line.split(kFieldSeparator);
        if (fields.size() < 2 || fields.size() > 3) {
            skip(line, QStringLiteral("This line is not an action, its keys and a pin."));
            continue;
        }

        const QString& id = fields.at(0);
        const HotKeyAction* const action = HotKeyActionList::find(id);
        if (action == nullptr) {
            skip(line, QStringLiteral("There is no action named %1.").arg(id));
            continue;
        }
        const bool listedTwice = std::any_of(good.cbegin(), good.cend(),
                                             [&id](const GoodLine& g) { return g.id == id; });
        if (listedTwice) {
            skip(line, QStringLiteral("%1 is listed twice.").arg(id));
            continue;
        }

        GoodLine parsed{id, {}, {}};
        QString problem;
        const QStringList keyTexts = splitKeys(fields.at(1));
        for (const QString& keyText : keyTexts) {
            const std::optional<HotKeyChord> chord = HotKeyChord::parse(keyText);
            if (!chord.has_value()) {
                problem = QStringLiteral("%1 is not a key.").arg(keyText);
                break;
            }
            if (const auto refused = refusal(*action, *chord); refused.has_value()) {
                problem = refused->reason;
                break;
            }
            const QString canonical = chord->toString();
            const QString holder = claimedBy.value(canonical);
            if (!holder.isEmpty()) {
                problem = QStringLiteral("%1 is already %2 in this file.")
                              .arg(canonical, HotKeyActionList::find(holder)->name);
                break;
            }
            if (!parsed.keys.contains(canonical)) {
                parsed.keys.append(canonical);
            }
        }
        if (problem.isEmpty() && fields.size() == 3 && !fields.at(2).isEmpty()) {
            const QString& pinText = fields.at(2);
            parsed.pin = pinText.size() == 1 ? validPin(pinText.at(0)) : QChar();
            if (parsed.pin.isNull()) {
                problem = QStringLiteral("%1 is not Slice A to Slice D.").arg(pinText);
            } else if (action->scope != HotKeyScope::Slice) {
                problem = QStringLiteral("%1 can't be set to one slice.").arg(action->name);
            }
        }
        if (!problem.isEmpty()) {
            skip(line, problem);
            continue;
        }

        for (const QString& key : std::as_const(parsed.keys)) {
            claimedBy.insert(key, id);
        }
        good.append(parsed);
    }

    if (good.isEmpty()) {
        return skipped;
    }

    // Take the keys from everyone else first, so the file's order can't matter.
    for (const HotKeyAction& action : HotKeyActionList::all()) {
        m_keys[action.id].removeIf([&claimedBy](const QString& k) { return claimedBy.contains(k); });
    }
    for (const GoodLine& line : std::as_const(good)) {
        m_keys.insert(line.id, line.keys);
        if (line.pin.isNull()) {
            m_pins.remove(line.id);
        } else {
            m_pins.insert(line.id, line.pin);
        }
    }
    rebuildChordIndex();
    emit bindingsChanged();
    return skipped;
}

QString HotKeyBindings::reservedBy(const QString& chord)
{
    const std::optional<HotKeyChord> parsed = HotKeyChord::parse(chord);
    if (!parsed.has_value()) {
        return {};
    }

    // Item names as the menus show them, src/gui/MainWindow.cpp:12534-14047.
    static const QHash<QString, QString> kReserved = [] {
        QHash<QString, QString> table{
            {QStringLiteral("Ctrl+,"), QStringLiteral("Settings")},
            {QStringLiteral("Ctrl+Q"), QStringLiteral("Quit")},
            {QStringLiteral("Ctrl+K"), QStringLiteral("Connect")},
            {QStringLiteral("Ctrl+L"), QStringLiteral("Pan Layout")},
            {QStringLiteral("Ctrl+R"), QStringLiteral("Add slice on active pan")},
            {QStringLiteral("Ctrl+M"), QStringLiteral("Minimal Mode")},
            {QStringLiteral("Ctrl+X"), QStringLiteral("CWX")},
            {QStringLiteral("Ctrl+Shift+A"), QStringLiteral("Arrange Workspace")},
            {QStringLiteral("Ctrl+Shift+S"), QStringLiteral("Spot Hub")},
            {QStringLiteral("Ctrl+Shift+R"), QStringLiteral("FreeDV Reporter")},
            {QStringLiteral("Ctrl+Shift+D"), QStringLiteral("Diversity")},
            {QStringLiteral("Ctrl+Shift+K"), QStringLiteral("Disconnect")},
            {QStringLiteral("Ctrl+Shift+N"), QStringLiteral("TNF")},
            {QStringLiteral("Ctrl+Shift+X"), QStringLiteral("Clear Spots")},
        };
        for (int n = 1; n <= 9; ++n) {
            table.insert(QStringLiteral("Ctrl+%1").arg(n), QStringLiteral("Profile %1").arg(n));
        }
        return table;
    }();
    return kReserved.value(parsed->toString());
}

} // namespace NereusSDR
