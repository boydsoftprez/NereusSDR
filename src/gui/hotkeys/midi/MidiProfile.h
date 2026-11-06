// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 10.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// A named list of MIDI bindings, kept in AppSettings under
// HotKeys/Midi/Profiles/<name>/ (design V7). A controller starts empty: its
// controls differ per model, so they are learned.
#pragma once

#include "gui/NereusGuiExport.h"

#include <QChar>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

namespace NereusSDR {

enum class MidiControlKind { Button, AbsoluteKnob, RelativeKnob };

struct MidiBinding {
    QString controlKey;    // ch1.note60, ch1.cc7, ch1.bend
    MidiControlKind kind;
    QString actionId;      // a Button's action; a knob's turn up
    QString actionIdDown;  // a knob's turn down; empty does nothing that way
    QChar pin;             // a slice letter, or null for the selected slice
};

class NEREUS_GUI_EXPORT MidiProfile {
public:
    MidiProfile() = default;
    explicit MidiProfile(const QString& name);

    QString name() const { return m_name; }
    const QList<MidiBinding>& bindings() const { return m_bindings; }

    // Why this can't be bound, shown to the user; empty when it can. A knob can't
    // run a Hold or transmitting action, because HotKeyDispatcher::turn would ignore
    // it. A Button may run anything.
    static QString refusalFor(MidiControlKind kind, const QString& actionId, const QString& actionIdDown);

    // Empty when bound, replacing any binding on the same control. Otherwise the
    // reason, and nothing is stored.
    QString bind(const MidiBinding& binding);
    void clearControl(const QString& controlKey);

    // For a test of what the host does with a list the checks never saw.
    void appendUncheckedForTest(const MidiBinding& binding) { m_bindings.append(binding); }

    // Reads this name's list. A binding whose action no longer exists is dropped
    // with a warning, as is one a knob could not run.
    void load();
    void save() const;

    static QStringList savedNames();
    static void removeSaved(const QString& name);
    static QString activeName();                       // empty when none is chosen
    static void setActiveName(const QString& name);

private:
    QString m_name;
    QList<MidiBinding> m_bindings;
};

} // namespace NereusSDR

Q_DECLARE_METATYPE(NereusSDR::MidiControlKind)
