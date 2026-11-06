// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 8.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// A drawing of the keyboard on Setup's Keyboard page: one layer of modifiers at
// a time, each key colored by the group of the action it runs. Each key's chord
// is the text KeyboardRouter::chordFromEvent makes, so the map and the keys agree.
#pragma once

#include "gui/NereusGuiExport.h"

#include <QColor>
#include <QPointer>
#include <QString>
#include <QVector>
#include <QWidget>

namespace NereusSDR {

class HotKeyBindings;
class KeyboardMapKey;

class NEREUS_GUI_EXPORT KeyboardMapWidget : public QWidget {
    Q_OBJECT

public:
    enum Layer { LayerPlain = 0, LayerShift = 1, LayerCtrl = 2, LayerCtrlShift = 3 };

    explicit KeyboardMapWidget(HotKeyBindings* bindings, QWidget* parent = nullptr);

    void setLayer(int layer);
    int layer() const { return m_layer; }

    // Every other key goes dim. Empty shows them all alike.
    void setHighlightedGroup(const QString& group);
    // The key with the white outline. Empty for none.
    void setPickedChord(const QString& chord);

    // The group a reserved menu key shows as, beside the action groups.
    static QString menuGroup();
    static QColor groupColor(const QString& group);
    // A chord, or text holding chords, as this computer's keyboard names them:
    // on a Mac Qt's Ctrl is the Command key and its Meta is Control.
    static QString displayText(QString text);
    // The Ctrl key's name on this computer.
    static QString commandKeyName();

signals:
    void keyClicked(QString chord);

private:
    void buildKeys();
    void refresh();

    QPointer<HotKeyBindings> m_bindings;
    int m_layer{LayerPlain};
    QString m_group;
    QString m_picked;
    QVector<KeyboardMapKey*> m_keys;
};

} // namespace NereusSDR
