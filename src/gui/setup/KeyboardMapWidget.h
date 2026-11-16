// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 8.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys CI fixes: the map reflows to fit
//                Setup's width, and its chrome takes the workspace palette.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// A drawing of the keyboard on Setup's Keyboard page: one layer of modifiers at
// a time, each key colored by the group of the action it runs. Each key's chord
// is the text KeyboardRouter::chordFromEvent makes, so the map and the keys agree.
//
// The map fits the width it is given and never scrolls sideways. The navigation
// keys and keypad stand beside the main block when there is room, and below it
// when there isn't. Keys shrink with the width; a key always keeps its letter and
// shows its action's name when the name fits. The tooltip has the full name.
#pragma once

#include "gui/NereusGuiExport.h"

#include <QColor>
#include <QPointer>
#include <QRect>
#include <QSize>
#include <QString>
#include <QVector>
#include <QWidget>

namespace NereusSDR {

class HotKeyBindings;
class KeyboardMapKey;

// One group's color beside its name in the map's legend. The group colors are
// data, as a trace's color is, so the legend draws them apart from its chips.
class NEREUS_GUI_EXPORT KeyGroupSwatch : public QWidget {
    Q_OBJECT

public:
    explicit KeyGroupSwatch(const QColor& color, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QColor m_color;
};

class NEREUS_GUI_EXPORT KeyboardMapWidget : public QWidget {
    Q_OBJECT

public:
    enum Layer { LayerPlain = 0, LayerShift = 1, LayerCtrl = 2, LayerCtrlShift = 3 };

    explicit KeyboardMapWidget(HotKeyBindings* bindings, QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    // True when the navigation keys and keypad stand below the main block.
    bool stacked() const { return m_stacked; }

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

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    struct PlacedKey {
        KeyboardMapKey* key;
        int block;
        QRect rect;  // at full size, inside its block
    };

    void buildKeys();
    void placeKeys(int width);
    void refresh();

    QPointer<HotKeyBindings> m_bindings;
    int m_layer{LayerPlain};
    QString m_group;
    QString m_picked;
    QVector<KeyboardMapKey*> m_keys;
    QVector<PlacedKey> m_placed;
    QVector<QSize> m_blocks;  // main, navigation, keypad, at full size
    bool m_stacked{false};
};

} // namespace NereusSDR
