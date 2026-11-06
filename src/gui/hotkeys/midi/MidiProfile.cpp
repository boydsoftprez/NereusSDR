// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 10.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 11: otherDirection, and a
//                changed binding keeps its row. AI-assisted via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "gui/hotkeys/midi/MidiProfile.h"

#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/hotkeys/HotKeyActionList.h"

#include <QPair>
#include <QSet>
#include <QUrl>

#include <algorithm>

namespace NereusSDR {

namespace {

const QString& profilesPrefix()
{
    static const QString kPrefix = QStringLiteral("HotKeys/Midi/Profiles/");
    return kPrefix;
}

const QString& activeKey()
{
    static const QString kKey = QStringLiteral("HotKeys/Midi/ActiveProfile");
    return kKey;
}

// A name is the user's text and a settings key is an XML tag, so the name goes
// in percent encoded.
QString prefixOf(const QString& name)
{
    return profilesPrefix() + QString::fromLatin1(QUrl::toPercentEncoding(name)) + QLatin1Char('/');
}

QString bindingPrefix(const QString& prefix, int index)
{
    return prefix + QStringLiteral("Binding%1/").arg(index);
}

void removeAllUnder(AppSettings& settings, const QString& prefix)
{
    const QStringList keys = settings.allKeys();
    for (const QString& key : keys) {
        if (key.startsWith(prefix)) {
            settings.remove(key);
        }
    }
}

QString kindText(MidiControlKind kind)
{
    switch (kind) {
    case MidiControlKind::Button:
        return QStringLiteral("Button");
    case MidiControlKind::AbsoluteKnob:
        return QStringLiteral("AbsoluteKnob");
    case MidiControlKind::RelativeKnob:
        return QStringLiteral("RelativeKnob");
    }
    return {};
}

bool kindFromText(const QString& text, MidiControlKind& kind)
{
    for (const MidiControlKind candidate :
         {MidiControlKind::Button, MidiControlKind::AbsoluteKnob, MidiControlKind::RelativeKnob}) {
        if (text == kindText(candidate)) {
            kind = candidate;
            return true;
        }
    }
    return false;
}

// Why a knob can't run this one action; empty when it can.
QString knobRefusal(const QString& id)
{
    const HotKeyAction* const action = HotKeyActionList::find(id);
    if (action == nullptr) {
        return QStringLiteral("That action no longer exists.");
    }
    if (action->gesture == HotKeyGesture::Hold || action->transmits) {
        return QStringLiteral("A knob can't run %1. Bind it to a button.").arg(action->name);
    }
    return {};
}

} // namespace

MidiProfile::MidiProfile(const QString& name)
    : m_name(name)
{
}

QString MidiProfile::refusalFor(MidiControlKind kind, const QString& actionId, const QString& actionIdDown)
{
    if (actionId.isEmpty()) {
        return QStringLiteral("Choose an action first.");
    }
    if (kind == MidiControlKind::Button) {
        return HotKeyActionList::find(actionId) != nullptr ? QString() : QStringLiteral("That action no longer exists.");
    }
    const QString upRefusal = knobRefusal(actionId);
    if (!upRefusal.isEmpty() || actionIdDown.isEmpty()) {
        return upRefusal;
    }
    return knobRefusal(actionIdDown);
}

QString MidiProfile::otherDirection(const QString& actionId)
{
    static const QList<QPair<QString, QString>> kPairs = {
        {QStringLiteral("tune.up"), QStringLiteral("tune.down")},
        {QStringLiteral("tune.up10"), QStringLiteral("tune.down10")},
        {QStringLiteral("step.larger"), QStringLiteral("step.smaller")},
        {QStringLiteral("filter.next"), QStringLiteral("filter.previous")},
        {QStringLiteral("volume.up"), QStringLiteral("volume.down")},
        {QStringLiteral("bandstack.next"), QStringLiteral("bandstack.previous")},
        {QStringLiteral("band.next"), QStringLiteral("band.previous")},
        {QStringLiteral("mode.next"), QStringLiteral("mode.previous")},
        {QStringLiteral("zoom.in"), QStringLiteral("zoom.out")},
    };
    for (const QPair<QString, QString>& pair : kPairs) {
        if (pair.first == actionId) {
            return pair.second;
        }
        if (pair.second == actionId) {
            return pair.first;
        }
    }
    return {};
}

QString MidiProfile::bind(const MidiBinding& binding)
{
    if (binding.controlKey.isEmpty()) {
        return QStringLiteral("Move a control first.");
    }
    const QString reason = refusalFor(binding.kind, binding.actionId, binding.actionIdDown);
    if (!reason.isEmpty()) {
        return reason;
    }
    for (MidiBinding& existing : m_bindings) {
        if (existing.controlKey == binding.controlKey) {
            existing = binding;
            return {};
        }
    }
    m_bindings.append(binding);
    return {};
}

void MidiProfile::clearControl(const QString& controlKey)
{
    m_bindings.removeIf([&controlKey](const MidiBinding& binding) { return binding.controlKey == controlKey; });
}

void MidiProfile::load()
{
    m_bindings.clear();
    const AppSettings& settings = AppSettings::instance();
    const QString prefix = prefixOf(m_name);
    const int count = settings.value(prefix + QStringLiteral("Count"), 0).toInt();
    for (int i = 0; i < count; ++i) {
        const QString at = bindingPrefix(prefix, i);
        MidiBinding binding{};
        binding.controlKey = settings.value(at + QStringLiteral("ControlKey")).toString();
        binding.actionId = settings.value(at + QStringLiteral("ActionId")).toString();
        binding.actionIdDown = settings.value(at + QStringLiteral("ActionIdDown")).toString();
        const QString pin = settings.value(at + QStringLiteral("Pin")).toString();
        if (pin.size() == 1) {
            binding.pin = pin.at(0);
        }
        if (!kindFromText(settings.value(at + QStringLiteral("Kind")).toString(), binding.kind)) {
            qCWarning(lcHotKeys) << "Dropped a saved MIDI binding of no known kind:" << m_name << binding.controlKey;
            continue;
        }
        const QString reason = bind(binding);
        if (!reason.isEmpty()) {
            qCWarning(lcHotKeys) << "Dropped a saved MIDI binding:" << m_name << binding.controlKey
                                 << binding.actionId << binding.actionIdDown << reason;
        }
    }
}

void MidiProfile::save() const
{
    AppSettings& settings = AppSettings::instance();
    const QString prefix = prefixOf(m_name);
    removeAllUnder(settings, prefix);
    // Count is what marks a profile with no rows as saved.
    settings.setValue(prefix + QStringLiteral("Count"), m_bindings.size());
    for (int i = 0; i < m_bindings.size(); ++i) {
        const MidiBinding& binding = m_bindings.at(i);
        const QString at = bindingPrefix(prefix, i);
        settings.setValue(at + QStringLiteral("ControlKey"), binding.controlKey);
        settings.setValue(at + QStringLiteral("Kind"), kindText(binding.kind));
        settings.setValue(at + QStringLiteral("ActionId"), binding.actionId);
        settings.setValue(at + QStringLiteral("ActionIdDown"), binding.actionIdDown);
        settings.setValue(at + QStringLiteral("Pin"), binding.pin.isNull() ? QString() : QString(binding.pin));
    }
    settings.save();
}

QStringList MidiProfile::savedNames()
{
    QSet<QString> names;
    const QStringList keys = AppSettings::instance().allKeys();
    for (const QString& key : keys) {
        if (key.startsWith(profilesPrefix())) {
            const QString encoded = key.mid(profilesPrefix().size()).section(QLatin1Char('/'), 0, 0);
            names.insert(QUrl::fromPercentEncoding(encoded.toLatin1()));
        }
    }
    QStringList sorted(names.cbegin(), names.cend());
    std::sort(sorted.begin(), sorted.end());
    return sorted;
}

void MidiProfile::removeSaved(const QString& name)
{
    AppSettings& settings = AppSettings::instance();
    removeAllUnder(settings, prefixOf(name));
    settings.save();
}

QString MidiProfile::activeName()
{
    return AppSettings::instance().value(activeKey()).toString();
}

void MidiProfile::setActiveName(const QString& name)
{
    AppSettings& settings = AppSettings::instance();
    settings.setValue(activeKey(), name);
    settings.save();
}

} // namespace NereusSDR
