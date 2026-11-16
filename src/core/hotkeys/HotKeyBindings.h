// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 1.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 8: clearKey, for the
//                Keyboard page's map. AI-assisted via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// The user's key bindings and slice pins, the rebinding rules, defaults, import
// and export. Only values that differ from the defaults go in AppSettings.
#pragma once

#include "core/NereusCoreExport.h"

#include <QChar>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

namespace NereusSDR {

enum class HotKeyBindResult {
    Bound,
    RefusedTransmitPlainKey,
    RefusedLatchedOnSpace,
    RefusedReserved,
    Conflict,
    RefusedInvalid,  // no such action, or the text is not a key
};

struct HotKeyBindOutcome {
    HotKeyBindResult result;
    QString reason;      // shown to the user; empty when Bound
    QString conflictId;  // the action holding the key, on Conflict
};

class NEREUS_CORE_EXPORT HotKeyBindings : public QObject {
    Q_OBJECT

public:
    // Starts at the defaults; load() reads the saved values.
    explicit HotKeyBindings(QObject* parent = nullptr);

    void load();
    void save();

    QStringList keysFor(const QString& id) const;
    QString actionForChord(const QString& chord) const;  // empty if unbound
    QChar pinFor(const QString& id) const;               // null for the selected slice
    // A letter A to D on a slice action; a null letter returns to the selected slice.
    void setPin(const QString& id, QChar letter);

    // On Bound, chord becomes the action's only key. With replaceConflict, an
    // action already holding chord loses that one key.
    HotKeyBindOutcome bind(const QString& id, const QString& chord, bool replaceConflict);
    void clear(const QString& id);         // removes every key, keeps the pin
    void clearKey(const QString& id, const QString& chord);  // removes that one key
    void resetToDefault(const QString& id);  // keys and pin; takes its keys back from others
    void resetAll();                       // keys and pins; leaves enabled() alone

    bool enabled() const;
    void setEnabled(bool on);

    QString exportText() const;
    // Lines that fail are skipped and returned with reasons; good lines apply.
    // A key on a good line leaves any action the text does not list.
    QStringList importText(const QString& text);

    static QString reservedBy(const QString& chord);  // menu item name, or empty

signals:
    void bindingsChanged();
    void enabledChanged(bool on);

private:
    void setDefaults();
    void rebuildChordIndex();

    QHash<QString, QStringList> m_keys;
    QHash<QString, QChar> m_pins;
    QHash<QString, QString> m_actionByChord;
    bool m_enabled{true};
};

} // namespace NereusSDR
