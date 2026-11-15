// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// =================================================================
// src/core/hotkeys/HotKeyBindings.h  (Longpath)
// =================================================================
//
// Welche Taste welche Handlung ausloest, die Regeln fuers Umbelegen,
// Vorgaben, Import und Export. In AppSettings steht nur, was von der
// Vorgabe abweicht.
//
// Uebernommen aus dem Upstream NereusSDR (`src/core/hotkeys/
// HotKeyBindings.h` @ 9036ffc04, J.J. Boyd KG4VCF). Longpath-Abweichung:
// die Liste der Menue-Kuerzel kommt aus den Menues selbst
// (setReservedChords), nicht aus einer Tabelle.
//
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 1.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 8: clearKey, for the
//                Keyboard page's map. AI-assisted via Anthropic Claude Code.
// Modification history (Longpath):
//   2026-11-10 - Uebernommen fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
//   2026-11-11 - Schalter „F-Tasten zuerst" (macOS, FKeyPriority).
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================
// no-port-check: NereusSDR-original, in Longpath uebernommen.


#include <QChar>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

namespace Longpath {

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

class HotKeyBindings : public QObject {
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

    // Nur macOS: F1…F12 liefern die F-Tasten, solange Longpath vorne ist
    // (FKeyPriority). Vorgabe an.
    bool fKeysFirst() const;
    void setFKeysFirst(bool on);

    // Nur macOS: der F-Tasten-Modus bleibt gestellt, solange Longpath
    // LAEUFT, nicht nur solange es vorne ist. Vorgabe an (Betreiber
    // 2026-11-14) -- sonst landet ein F10 fuer den Sprachspeicher am
    // Mac und schaltet ihn stumm. Siehe FKeyPriority.h.
    bool fKeysWhileRunning() const;
    void setFKeysWhileRunning(bool on);

    QString exportText() const;
    // Lines that fail are skipped and returned with reasons; good lines apply.
    // A key on a good line leaves any action the text does not list.
    QStringList importText(const QString& text);

    // Der Menuepunkt, der diese Taste schon hat, oder leer. Die Tabelle
    // fuellt MainWindow beim Start aus seinen eigenen Menues
    // (setReservedChords), damit sie nicht neben den Menues herlaeuft
    // und veraltet -- Upstream fuehrt sie von Hand.
    static QString reservedBy(const QString& chord);
    static void setReservedChords(const QHash<QString, QString>& menuItemByChord);

signals:
    void bindingsChanged();
    void enabledChanged(bool on);
    void fKeysFirstChanged(bool on);
    void fKeysWhileRunningChanged(bool on);

private:
    void setDefaults();
    void rebuildChordIndex();

    QHash<QString, QStringList> m_keys;
    QHash<QString, QChar> m_pins;
    QHash<QString, QString> m_actionByChord;
    bool m_enabled{true};
    bool m_fKeysFirst{true};
    bool m_fKeysWhileRunning{true};
};

} // namespace Longpath
