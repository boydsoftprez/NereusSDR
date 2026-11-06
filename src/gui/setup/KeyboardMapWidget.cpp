// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 8.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "gui/setup/KeyboardMapWidget.h"

#include "core/hotkeys/HotKeyAction.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "core/hotkeys/HotKeyChord.h"

#include <QAbstractButton>
#include <QHash>
#include <QPainter>

#include <array>
#include <optional>

namespace NereusSDR {

namespace {

constexpr int kKeyWidth = 46;
constexpr int kKeyHeight = 44;
constexpr int kKeyGap = 3;
constexpr int kBlockGap = 12;
constexpr int kRowsTall = 6;

enum class KeyKind {
    Normal,
    Modifier,  // changes other keys, holds nothing
    SetOnly,   // Tab, Enter and the like: the list's Set can use them, the map offers no action
    Escape,    // cancels a Set, so it holds nothing
    Gap,       // an empty place on the row
};

struct KeySpec {
    const char* label;  // what the cap says
    const char* key;    // the portable key name, keypad keys as Num+Key
    int width;
    KeyKind kind;
};

constexpr KeySpec normal(const char* name, int width = kKeyWidth)
{
    return {name, name, width, KeyKind::Normal};
}

constexpr KeySpec gap() { return {"", "", kKeyWidth, KeyKind::Gap}; }

using Row = QVector<KeySpec>;

QVector<Row> mainBlock()
{
    static const QByteArray kCommand = KeyboardMapWidget::commandKeyName().toLatin1();
    return {
        {{"Esc", "Esc", kKeyWidth, KeyKind::Escape}, normal("F1"), normal("F2"), normal("F3"), normal("F4"),
         normal("F5"), normal("F6"), normal("F7"), normal("F8"), normal("F9"), normal("F10"), normal("F11"),
         normal("F12")},
        {normal("`"), normal("1"), normal("2"), normal("3"), normal("4"), normal("5"), normal("6"), normal("7"),
         normal("8"), normal("9"), normal("0"), normal("-"), normal("="),
         {"Bksp", "Backspace", 70, KeyKind::SetOnly}},
        {{"Tab", "Tab", 60, KeyKind::SetOnly}, normal("Q"), normal("W"), normal("E"), normal("R"), normal("T"),
         normal("Y"), normal("U"), normal("I"), normal("O"), normal("P"), normal("["), normal("]"),
         normal("\\", 56)},
        {{"Caps", "CapsLock", 72, KeyKind::Modifier}, normal("A"), normal("S"), normal("D"), normal("F"),
         normal("G"), normal("H"), normal("J"), normal("K"), normal("L"), normal(";"), normal("'"),
         {"Enter", "Return", 84, KeyKind::SetOnly}},
        {{"Shift", "", 92, KeyKind::Modifier}, normal("Z"), normal("X"), normal("C"), normal("V"), normal("B"),
         normal("N"), normal("M"), normal(","), normal("."), normal("/"),
         {"Shift", "", 120, KeyKind::Modifier}},
        {{kCommand.constData(), "", 60, KeyKind::Modifier}, {"Alt", "", 50, KeyKind::Modifier},
         {"Space", "Space", 280, KeyKind::Normal}, {"Alt", "", 50, KeyKind::Modifier},
         {kCommand.constData(), "", 60, KeyKind::Modifier}},
    };
}

QVector<Row> navigationBlock()
{
    return {
        {normal("Ins"), normal("Home"), normal("PgUp")},
        {normal("Del"), normal("End"), {"PgDn", "PgDown", kKeyWidth, KeyKind::Normal}},
        {},
        {gap(), normal("Up"), gap()},
        {normal("Left"), normal("Down"), normal("Right")},
    };
}

QVector<Row> keypadBlock()
{
    const auto pad = [](const char* label, const char* key) {
        return KeySpec{label, key, kKeyWidth, KeyKind::Normal};
    };
    return {
        {{"Num", "NumLock", kKeyWidth, KeyKind::Modifier}, pad("/", "Num+/"), pad("*", "Num+*"), pad("-", "Num+-")},
        {pad("7", "Num+7"), pad("8", "Num+8"), pad("9", "Num+9"), pad("Num +", "Num++")},
        {pad("4", "Num+4"), pad("5", "Num+5"), pad("6", "Num+6"), gap()},
        {pad("1", "Num+1"), pad("2", "Num+2"), pad("3", "Num+3"),
         {"Enter", "Num+Enter", kKeyWidth, KeyKind::SetOnly}},
        {pad("0", "Num+0"), pad(".", "Num+."), gap(), gap()},
    };
}

// What the same key types with Shift held, on the US layout the map draws. The
// router sees the typed character, so Shift+1 reaches it as Shift+!.
QString shiftedKey(const QString& key)
{
    static const QHash<QString, QString> kShifted{
        {QStringLiteral("`"), QStringLiteral("~")},  {QStringLiteral("1"), QStringLiteral("!")},
        {QStringLiteral("2"), QStringLiteral("@")},  {QStringLiteral("3"), QStringLiteral("#")},
        {QStringLiteral("4"), QStringLiteral("$")},  {QStringLiteral("5"), QStringLiteral("%")},
        {QStringLiteral("6"), QStringLiteral("^")},  {QStringLiteral("7"), QStringLiteral("&")},
        {QStringLiteral("8"), QStringLiteral("*")},  {QStringLiteral("9"), QStringLiteral("(")},
        {QStringLiteral("0"), QStringLiteral(")")},  {QStringLiteral("-"), QStringLiteral("_")},
        {QStringLiteral("="), QStringLiteral("+")},  {QStringLiteral("["), QStringLiteral("{")},
        {QStringLiteral("]"), QStringLiteral("}")},  {QStringLiteral("\\"), QStringLiteral("|")},
        {QStringLiteral(";"), QStringLiteral(":")},  {QStringLiteral("'"), QStringLiteral("\"")},
        {QStringLiteral(","), QStringLiteral("<")},  {QStringLiteral("."), QStringLiteral(">")},
        {QStringLiteral("/"), QStringLiteral("?")},
    };
    return kShifted.value(key, key);
}

QString layerPrefix(int layer)
{
    switch (layer) {
    case KeyboardMapWidget::LayerShift:
        return QStringLiteral("Shift+");
    case KeyboardMapWidget::LayerCtrl:
        return QStringLiteral("Ctrl+");
    case KeyboardMapWidget::LayerCtrlShift:
        return QStringLiteral("Ctrl+Shift+");
    default:
        return {};
    }
}

bool layerHasShift(int layer)
{
    return layer == KeyboardMapWidget::LayerShift || layer == KeyboardMapWidget::LayerCtrlShift;
}

// A short name that fits on a cap. The full name is its tooltip.
QString shortName(QString name)
{
    name.replace(QLatin1String("Go to frequency"), QLatin1String("Go to freq"));
    name.replace(QLatin1String("Noise reduction"), QLatin1String("Noise reduce"));
    name.replace(QLatin1String("CW straight key"), QLatin1String("CW straight"));
    name.replace(QLatin1String("Add band stack entry"), QLatin1String("Stack add"));
    name.replace(QLatin1String("Open band stack panel"), QLatin1String("Stack panel"));
    name.replace(QLatin1String("Band stack "), QLatin1String("Stack "));
    name.replace(QLatin1String("Tune "), QString());
    name.replace(QLatin1String(" one step"), QString());
    name.replace(QLatin1String(" ten steps"), QLatin1String(" x10"));
    name.replace(QLatin1String("CW message "), QLatin1String("CW "));
    name.replace(QLatin1String("Voice message "), QLatin1String("Voice "));
    name.replace(QLatin1String("Filter "), QLatin1String("Filt "));
    name.replace(QLatin1String("Volume "), QLatin1String("Vol "));
    name.replace(QLatin1String("previous"), QLatin1String("prev"));
    return name;
}

} // namespace

// One key cap. A button, so Tab, the screen reader and the tooltip all work.
class KeyboardMapKey : public QAbstractButton {
public:
    KeyboardMapKey(const KeySpec& spec, QWidget* parent)
        : QAbstractButton(parent)
        , m_label(QString::fromLatin1(spec.label))
        , m_key(QString::fromLatin1(spec.key))
        , m_kind(spec.kind)
    {
        setFocusPolicy(Qt::TabFocus);
        setCursor(Qt::PointingHandCursor);
    }

    QString chord() const { return m_chord; }

    void applyState(HotKeyBindings* bindings, int layer, const QString& group, const QString& picked)
    {
        QString keyName = m_key;
        QString shownLabel = m_label;
        const bool numeric = keyName.startsWith(QLatin1String("Num+"));
        if (layerHasShift(layer) && !numeric && m_kind == KeyKind::Normal) {
            keyName = shiftedKey(keyName);
            if (keyName != m_key) {
                shownLabel = keyName;
            }
        }
        m_shownLabel = shownLabel;

        m_chord.clear();
        if (m_kind != KeyKind::Modifier) {
            const std::optional<HotKeyChord> chord = HotKeyChord::parse(layerPrefix(layer) + keyName);
            if (chord.has_value()) {
                m_chord = chord->toString();
            }
        }
        setObjectName(m_chord.isEmpty() ? QString() : m_chord);

        const QString ownerId = bindings != nullptr && !m_chord.isEmpty() ? bindings->actionForChord(m_chord) : QString();
        const HotKeyAction* const owner = ownerId.isEmpty() ? nullptr : HotKeyActionList::find(ownerId);
        const QString menuItem = m_chord.isEmpty() ? QString() : HotKeyBindings::reservedBy(m_chord);

        QString tip;
        if (owner != nullptr) {
            m_group = owner->group;
            m_action = shortName(owner->name);
            tip = owner->name;
        } else if (!menuItem.isEmpty()) {
            m_group = KeyboardMapWidget::menuGroup();
            m_action = menuItem;
            tip = QStringLiteral("%1 menu shortcut").arg(menuItem);
        } else {
            m_group.clear();
            m_action.clear();
        }
        setProperty("keyGroup", m_group);

        const bool holdsSomething = owner != nullptr || !menuItem.isEmpty();
        bool usable = bindings != nullptr;
        switch (m_kind) {
        case KeyKind::Modifier:
            usable = false;
            tip = QStringLiteral("This key changes other keys. It can't run an action alone.");
            break;
        case KeyKind::Escape:
            usable = usable && holdsSomething;
            if (!holdsSomething) {
                tip = QStringLiteral("Escape cancels setting a key, so it can't run an action.");
            }
            break;
        case KeyKind::SetOnly:
            usable = usable && holdsSomething;
            if (!holdsSomething) {
                tip = QStringLiteral("Use Set in the list below to put an action on this key.");
            }
            break;
        case KeyKind::Normal:
        case KeyKind::Gap:
            break;
        }
        setEnabled(usable);
        setToolTip(tip);
        const QString shownChord = KeyboardMapWidget::displayText(m_chord.isEmpty() ? m_label : m_chord);
        setAccessibleName(m_action.isEmpty() ? shownChord : QStringLiteral("%1: %2").arg(shownChord, m_action));
        setAccessibleDescription(tip);

        m_dim = !group.isEmpty() && group != m_group;
        m_picked = !m_chord.isEmpty() && m_chord == picked;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        if (m_dim) {
            painter.setOpacity(0.25);
        }

        const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        const QColor fill = m_group.isEmpty() ? QColor(0x13, 0x13, 0x26) : KeyboardMapWidget::groupColor(m_group);
        QColor edge(0x30, 0x40, 0x50);
        qreal edgeWidth = 1.0;
        if (m_picked) {
            edge = Qt::white;
            edgeWidth = 2.0;
        } else if (hasFocus()) {
            edge = QColor(0x00, 0xb4, 0xd8);
            edgeWidth = 2.0;
        }
        painter.setPen(QPen(edge, edgeWidth));
        painter.setBrush(fill);
        painter.drawRoundedRect(box, 4.0, 4.0);

        const QRect inner = rect().adjusted(3, 2, -3, -2);
        QFont font = painter.font();
        font.setPixelSize(10);
        painter.setFont(font);
        painter.setPen(isEnabled() ? QColor(0xc8, 0xd8, 0xe8) : QColor(0x60, 0x70, 0x80));
        painter.drawText(inner, Qt::AlignLeft | Qt::AlignTop, m_shownLabel);

        if (!m_action.isEmpty()) {
            font.setPixelSize(9);
            painter.setFont(font);
            painter.setPen(Qt::white);
            const QRect lower = inner.adjusted(0, 12, 0, 0);
            painter.setClipRect(lower);  // a long name stops short of the key's own label
            painter.drawText(lower, Qt::AlignLeft | Qt::AlignBottom | Qt::TextWordWrap, m_action);
        }
    }

private:
    QString m_label;
    QString m_key;
    KeyKind m_kind;
    QString m_shownLabel;
    QString m_chord;
    QString m_group;
    QString m_action;
    bool m_dim{false};
    bool m_picked{false};
};

KeyboardMapWidget::KeyboardMapWidget(HotKeyBindings* bindings, QWidget* parent)
    : QWidget(parent)
    , m_bindings(bindings)
{
    buildKeys();
    if (m_bindings) {
        connect(m_bindings.data(), &HotKeyBindings::bindingsChanged, this, &KeyboardMapWidget::refresh);
    }
    refresh();
}

void KeyboardMapWidget::buildKeys()
{
    const int tallest = kRowsTall * kKeyHeight + (kRowsTall - 1) * kKeyGap;
    int blockX = 0;
    const auto layBlock = [this, &blockX, tallest](const QVector<Row>& rows) {
        const int blockHeight = int(rows.size()) * kKeyHeight + (int(rows.size()) - 1) * kKeyGap;
        int y = tallest - blockHeight;  // the smaller blocks sit on the bottom row
        int widest = 0;
        for (const Row& row : rows) {
            int x = blockX;
            for (const KeySpec& spec : row) {
                if (spec.kind != KeyKind::Gap) {
                    auto* const key = new KeyboardMapKey(spec, this);
                    key->setGeometry(x, y, spec.width, kKeyHeight);
                    connect(key, &QAbstractButton::clicked, this, [this, key] {
                        if (!key->chord().isEmpty()) {
                            emit keyClicked(key->chord());
                        }
                    });
                    m_keys.append(key);
                }
                x += spec.width + kKeyGap;
            }
            widest = qMax(widest, x - blockX - kKeyGap);
            y += kKeyHeight + kKeyGap;
        }
        blockX += widest + kBlockGap;
    };
    layBlock(mainBlock());
    layBlock(navigationBlock());
    layBlock(keypadBlock());
    setFixedSize(blockX - kBlockGap, tallest);
}

void KeyboardMapWidget::refresh()
{
    for (KeyboardMapKey* const key : std::as_const(m_keys)) {
        key->applyState(m_bindings.data(), m_layer, m_group, m_picked);
    }
}

void KeyboardMapWidget::setLayer(int layer)
{
    m_layer = qBound(int(LayerPlain), layer, int(LayerCtrlShift));
    refresh();
}

void KeyboardMapWidget::setHighlightedGroup(const QString& group)
{
    m_group = group;
    refresh();
}

void KeyboardMapWidget::setPickedChord(const QString& chord)
{
    m_picked = chord;
    refresh();
}

QString KeyboardMapWidget::menuGroup()
{
    return QStringLiteral("Menu");
}

QColor KeyboardMapWidget::groupColor(const QString& group)
{
    static const QHash<QString, QColor> kColors{
        {QStringLiteral("Tuning"), QColor(0x17, 0x6a, 0x78)},
        {QStringLiteral("Filter"), QColor(0x1f, 0x6e, 0x45)},
        {QStringLiteral("Audio"), QColor(0x7a, 0x5a, 0x1e)},
        {QStringLiteral("Band and mode"), QColor(0x2f, 0x5f, 0x92)},
        {QStringLiteral("Display"), QColor(0x3f, 0x4a, 0x56)},
        {QStringLiteral("DSP"), QColor(0x3f, 0x6b, 0x2f)},
        {QStringLiteral("Transmit"), QColor(0xa3, 0x2a, 0x2a)},
        {QStringLiteral("CW"), QColor(0x5b, 0x46, 0x90)},
        {QStringLiteral("Voice"), QColor(0x8a, 0x3a, 0x6c)},
        {menuGroup(), QColor(0x2a, 0x2a, 0x2f)},
    };
    return kColors.value(group);
}

QString KeyboardMapWidget::displayText(QString text)
{
#ifdef Q_OS_MAC
    const QString swapping = QStringLiteral("\u0001");
    text.replace(QLatin1String("Ctrl+"), swapping);
    text.replace(QLatin1String("Meta+"), QLatin1String("Ctrl+"));
    text.replace(swapping, QLatin1String("Cmd+"));
#endif
    text.replace(QLatin1String("Num+"), QLatin1String("Num "));
    return text;
}

QString KeyboardMapWidget::commandKeyName()
{
#ifdef Q_OS_MAC
    return QStringLiteral("Cmd");
#else
    return QStringLiteral("Ctrl");
#endif
}

} // namespace NereusSDR
