// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 18.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "gui/setup/KnobsSetupPage.h"

#include "core/hotkeys/HotKeyAction.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "gui/hotkeys/ControllerHost.h"
#include "gui/hotkeys/devices/DeviceInput.h"
#include "gui/hotkeys/devices/FlexControlInput.h"
#include "gui/hotkeys/midi/MidiProfile.h"
#include "gui/setup/HotKeySetupCommon.h"
#include "gui/styles/WorkspacePreview.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPainter>
#include <QSignalBlocker>
#include <QSlider>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace NereusSDR {

using namespace HotKeySetup;

namespace {

constexpr int kColumnControl = 0;
constexpr int kColumnGesture = 1;
constexpr int kColumnAction = 2;
constexpr int kColumnDown = 3;
constexpr int kColumnActsOn = 4;
constexpr int kColumnCount = 5;
constexpr int kRowHeight = 30;
constexpr int kHeaderHeight = 28;
constexpr int kMostRowsShown = 7;

// The mockup's stage and device list. Its 380 clips the ShuttlePro's ring, so a little more.
constexpr int kStageHeight = 190;
constexpr int kStageWidth = 396;
constexpr int kListWidth = 150;
constexpr int kDeviceRowHeight = 46;

constexpr int kSpeedFastest = 1;

QString checkBoxStyle()
{
    return QString::fromLatin1(Style::kCheckBoxStyle)
        + QStringLiteral("QCheckBox { background: transparent; }QCheckBox:disabled { color: #5a6a7a; }");
}

const QString& notPluggedIn()
{
    static const QString kText = QStringLiteral("Not plugged in");
    return kText;
}

struct ControlRowSpec {
    QString control;
    ControlGesture gesture;
};

// A control on the drawing, at the mockup's place, and the rows it has. One thing on
// the drawing can be two controls to the device: a wheel you can also press.
struct ControlSpec {
    QString cap;    // on the drawing
    QString name;   // in the list
    int x;
    int y;
    int width;
    int height;
    bool round;
    QList<ControlRowSpec> rows;
};

struct DeviceDrawing {
    QList<ControlSpec> controls;
    bool display{false};
};

ControlSpec wheel(const QString& name, int x, int y, int size, const QList<ControlRowSpec>& rows)
{
    return {name, name, x, y, size, size, true, rows};
}

ControlSpec bar(const QString& name, int x, int y, int width, const QList<ControlRowSpec>& rows)
{
    return {name, name, x, y, width, 26, false, rows};
}

ControlSpec button(const QString& cap, const QString& name, int x, int y, const QList<ControlRowSpec>& rows)
{
    return {cap, name, x, y, std::max(38, int(cap.size()) * 7 + 8), 36, false, rows};
}

DeviceDrawing drawingOf(DeviceId id)
{
    using G = ControlGesture;
    const auto turn = [](const QString& control) { return QList<ControlRowSpec>{{control, G::Turn}}; };
    const auto press = [](const QString& control) { return QList<ControlRowSpec>{{control, G::Press}}; };
    const auto tapped = [](const QString& control) {
        return QList<ControlRowSpec>{{control, G::Tap}, {control, G::DoubleTap}, {control, G::Hold}};
    };
    const auto pressHold = [](const QString& control) {
        return QList<ControlRowSpec>{{control, G::Press}, {control, G::Hold}};
    };
    const auto numbered = [](int n) { return QStringLiteral("button%1").arg(n); };

    DeviceDrawing drawing;
    QList<ControlSpec>& c = drawing.controls;
    switch (id) {
    case DeviceId::Rc28:
        c.append(wheel(QStringLiteral("Wheel"), 40, 45, 90, turn(QStringLiteral("wheel"))));
        c.append(button(QStringLiteral("F1"), QStringLiteral("F1"), 165, 40, pressHold(QStringLiteral("f1"))));
        c.append(button(QStringLiteral("F2"), QStringLiteral("F2"), 165, 80, pressHold(QStringLiteral("f2"))));
        c.append(bar(QStringLiteral("TX bar"), 40, 150, 230, press(QStringLiteral("tx"))));
        break;
    case DeviceId::PowerMate:
        c.append(wheel(QStringLiteral("Wheel"), 60, 40, 110,
                       {{QStringLiteral("wheel"), G::Turn}, {QStringLiteral("press"), G::Press}}));
        break;
    case DeviceId::ShuttleXpress:
        c.append(wheel(QStringLiteral("Jog"), 90, 60, 80, turn(QStringLiteral("jog"))));
        c.append(bar(QStringLiteral("Ring"), 40, 150, 200, turn(QStringLiteral("ring"))));
        for (int i = 0; i < 5; ++i) {
            c.append(button(QStringLiteral("B%1").arg(i + 1), QStringLiteral("Button %1").arg(i + 1), 20 + i * 52, 12,
                            press(numbered(i + 1))));
        }
        break;
    case DeviceId::ShuttlePro2:
        c.append(wheel(QStringLiteral("Jog"), 280, 60, 70, turn(QStringLiteral("jog"))));
        c.append(bar(QStringLiteral("Ring"), 240, 150, 150, turn(QStringLiteral("ring"))));
        for (int i = 0; i < 15; ++i) {
            c.append(button(QStringLiteral("B%1").arg(i + 1), QStringLiteral("Button %1").arg(i + 1),
                            14 + (i % 5) * 44, 14 + (i / 5) * 44, press(numbered(i + 1))));
        }
        break;
    case DeviceId::FlexControl:
        c.append(wheel(QStringLiteral("Wheel"), 90, 30, 110, turn(QStringLiteral("wheel"))));
        c.append(button(QStringLiteral("Knob"), QStringLiteral("Knob"), 128, 148, tapped(QStringLiteral("knob"))));
        c.append(button(QStringLiteral("1"), QStringLiteral("Button 1"), 15, 150, tapped(numbered(1))));
        c.append(button(QStringLiteral("2"), QStringLiteral("Button 2"), 230, 150, tapped(numbered(2))));
        c.append(button(QStringLiteral("3"), QStringLiteral("Button 3"), 280, 150, tapped(numbered(3))));
        break;
    case DeviceId::TMate2:
        drawing.display = true;
        c.append(wheel(QStringLiteral("Main"), 40, 90, 80, turn(QStringLiteral("main"))));
        c.append(wheel(QStringLiteral("E1"), 150, 110, 50, turn(QStringLiteral("e1"))));
        c.append(wheel(QStringLiteral("E2"), 220, 110, 50, turn(QStringLiteral("e2"))));
        for (int i = 0; i < 6; ++i) {
            c.append(button(QStringLiteral("K%1").arg(i + 1), QStringLiteral("Key %1").arg(i + 1), 290 + (i % 2) * 44,
                            40 + (i / 2) * 44, press(numbered(i + 1))));
        }
        break;
    case DeviceId::StreamDeckPlus:
        for (int i = 0; i < 8; ++i) {
            c.append(button(QStringLiteral("K%1").arg(i + 1), QStringLiteral("Key %1").arg(i + 1), 20 + (i % 4) * 62,
                            10 + (i / 4) * 44, press(numbered(i + 1))));
        }
        for (int i = 0; i < 4; ++i) {
            const QString control = QStringLiteral("dial%1").arg(i + 1);
            ControlSpec dial = wheel(QStringLiteral("D%1").arg(i + 1), 24 + i * 62, 140, 42,
                                     {{control, G::Turn}, {control, G::Press}});
            dial.name = QStringLiteral("Dial %1").arg(i + 1);
            c.append(dial);
        }
        break;
    case DeviceId::UlanziDial: {
        c.append(wheel(QStringLiteral("Dial"), 100, 55, 90,
                       {{QStringLiteral("wheel"), G::Turn}, {QStringLiteral("press"), G::Press}}));
        c.append(button(QStringLiteral("Top left"), QStringLiteral("Top left"), 40, 10, press(QStringLiteral("topLeft"))));
        c.append(button(QStringLiteral("Top mid"), QStringLiteral("Top mid"), 125, 10, press(QStringLiteral("topMiddle"))));
        c.append(button(QStringLiteral("Top right"), QStringLiteral("Top right"), 210, 10, press(QStringLiteral("topRight"))));
        c.append(button(QStringLiteral("Left top"), QStringLiteral("Left top"), 15, 70, press(QStringLiteral("leftTop"))));
        c.append(button(QStringLiteral("Left bot"), QStringLiteral("Left bot"), 15, 115, press(QStringLiteral("leftBottom"))));
        c.append(button(QStringLiteral("Right top"), QStringLiteral("Right top"), 235, 70, press(QStringLiteral("rightTop"))));
        c.append(button(QStringLiteral("Right bot"), QStringLiteral("Right bot"), 235, 115, press(QStringLiteral("rightBottom"))));
        break;
    }
    }
    return drawing;
}

QString gestureName(ControlGesture gesture)
{
    switch (gesture) {
    case ControlGesture::Press:
        return QStringLiteral("Press");
    case ControlGesture::Hold:
        return QStringLiteral("Hold");
    case ControlGesture::Tap:
        return QStringLiteral("Tap");
    case ControlGesture::DoubleTap:
        return QStringLiteral("Double tap");
    case ControlGesture::Turn:
        return QStringLiteral("Turn");
    }
    return {};
}

QString rowName(const QString& what, const QString& control, ControlGesture gesture)
{
    const QString tag = gesture == ControlGesture::DoubleTap ? QStringLiteral("DoubleTap") : gestureName(gesture);
    return QStringLiteral("knob%1:%2:%3").arg(what, control, tag);
}

QString lightsText(DeviceId id)
{
    switch (id) {
    case DeviceId::Rc28:
        return QStringLiteral("Lights follow the radio");
    case DeviceId::FlexControl:
        return QStringLiteral("Aux lights follow the radio");
    case DeviceId::TMate2:
        return QStringLiteral("Show the radio on the display");
    case DeviceId::StreamDeckPlus:
        return QStringLiteral("Show the radio on keys and strip");
    default:
        return QStringLiteral("Lights follow the radio");
    }
}

QString speedText(int speed)
{
    return speed <= kSpeedFastest ? QStringLiteral("Every click") : QStringLiteral("One step per %1 clicks").arg(speed);
}

// The fixed words DeviceInput and FlexControlInput give for a device someone else
// holds. tst_hot_key_setup_knobs reads them from there, so a change there fails it.
bool heldByAnotherApp(const QString& reason)
{
    return reason.startsWith(QLatin1String("Another app is using it."));
}

bool blockedByMac(const QString& reason)
{
    return reason.startsWith(QLatin1String("macOS is blocking it."));
}

// One device in the list. A button, so Tab, the screen reader and the tooltip all work.
class KnobDeviceRow : public QAbstractButton {
public:
    KnobDeviceRow(const QString& name, QWidget* parent)
        : QAbstractButton(parent)
    {
        setText(name);
        setCheckable(true);
        setAutoExclusive(true);
        setFocusPolicy(Qt::TabFocus);
        setCursor(Qt::PointingHandCursor);
        setFixedHeight(kDeviceRowHeight);
    }

    void setStateLine(const QString& line)
    {
        m_line = line;
        setProperty("stateLine", line);
        setAccessibleName(QStringLiteral("%1: %2").arg(text(), line));
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        if (isChecked()) {
            painter.fillRect(rect(), WorkspacePreview::roleColour(QColor(0x10, 0x22, 0x3a), "selected-top"));
        }
        painter.setPen(WorkspacePreview::roleColour(QColor(0x30, 0x40, 0x50), "border"));
        painter.drawLine(rect().bottomLeft(), rect().bottomRight());
        if (hasFocus()) {
            painter.setPen(QPen(WorkspacePreview::roleColour(QColor(0x00, 0xb4, 0xd8), "accent"), 2.0));
            painter.drawRect(rect().adjusted(1, 1, -1, -1));
        }

        const QRect inner = rect().adjusted(8, 3, -6, -2);
        QFont font = painter.font();
        font.setPixelSize(12);
        painter.setFont(font);
        const QColor name = isChecked() ? WorkspacePreview::roleColour(QColor(0xc8, 0xd8, 0xe8), "selected-text")
                                        : WorkspacePreview::roleColour(QColor(0xc8, 0xd8, 0xe8), "text");
        painter.setPen(isEnabled() ? name : WorkspacePreview::roleColour(QColor(0x5a, 0x6a, 0x7a), "text-inactive"));
        painter.drawText(inner, Qt::AlignLeft | Qt::AlignTop, text());
        font.setPixelSize(10);
        painter.setFont(font);
        // The chosen row's fill takes the selection's text color to stay readable.
        const QColor line = isChecked() ? WorkspacePreview::roleColour(QColor(0xc8, 0xd8, 0xe8), "selected-text")
                                        : WorkspacePreview::roleColour(QColor(0x80, 0x90, 0xa0), "text-secondary");
        painter.setPen(isEnabled() ? line : WorkspacePreview::roleColour(QColor(0x5a, 0x6a, 0x7a), "text-inactive"));
        painter.drawText(inner.adjusted(0, 15, 0, 0), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_line);
    }

private:
    QString m_line;
};

// One control on the drawing.
class KnobControlCap : public QAbstractButton {
public:
    KnobControlCap(const ControlSpec& spec, QWidget* parent)
        : QAbstractButton(parent)
        , m_round(spec.round)
    {
        setText(spec.cap);
        setCheckable(true);
        setAutoExclusive(true);
        setFocusPolicy(Qt::TabFocus);
        setCursor(Qt::PointingHandCursor);
        setAccessibleName(spec.name);
        setToolTip(spec.name);
        setGeometry(spec.x, spec.y, spec.width, spec.height);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QColor edge = WorkspacePreview::roleColour(QColor(0x30, 0x40, 0x50), "border");
        qreal edgeWidth = 1.0;
        if (isChecked() || hasFocus()) {
            edge = WorkspacePreview::roleColour(QColor(0x00, 0xb4, 0xd8), "accent");
            edgeWidth = hasFocus() ? 2.0 : 1.0;
        }
        painter.setPen(QPen(edge, edgeWidth));
        painter.setBrush(isChecked() ? WorkspacePreview::roleColour(QColor(0x10, 0x22, 0x3a), "selected-top")
                                     : WorkspacePreview::roleColour(QColor(0x13, 0x13, 0x26), "button"));
        if (m_round) {
            painter.drawEllipse(box);
        } else {
            painter.drawRoundedRect(box, 4.0, 4.0);
        }
        QFont font = painter.font();
        font.setPixelSize(10);
        painter.setFont(font);
        const QColor name = isChecked() ? WorkspacePreview::roleColour(QColor(0xc8, 0xd8, 0xe8), "selected-text")
                                        : WorkspacePreview::roleColour(QColor(0xc8, 0xd8, 0xe8), "text");
        painter.setPen(isEnabled() ? name : WorkspacePreview::roleColour(QColor(0x5a, 0x6a, 0x7a), "text-inactive"));
        painter.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, text());
    }

private:
    bool m_round;
};

// The stage the controls stand on, with the device's display when it has one.
class KnobDrawing : public QWidget {
public:
    explicit KnobDrawing(QWidget* parent)
        : QWidget(parent)
    {
        setFixedHeight(kStageHeight);
        setMinimumWidth(kStageWidth);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setShowsDisplay(bool shows)
    {
        m_display = shows;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(WorkspacePreview::roleColour(QColor(0x30, 0x40, 0x50), "border"));
        painter.setBrush(WorkspacePreview::roleColour(QColor(0x0f, 0x0f, 0x1a), "inset"));
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 6.0, 6.0);
        if (m_display) {
            const QRect screen(40, 14, 230, 62);
            painter.setPen(WorkspacePreview::roleColour(QColor(0x30, 0x40, 0x50), "border"));
            painter.setBrush(WorkspacePreview::roleColour(QColor(0x13, 0x13, 0x26), "button"));
            painter.drawRoundedRect(screen, 3.0, 3.0);
            painter.setPen(WorkspacePreview::roleColour(QColor(0x80, 0x90, 0xa0), "text-secondary"));
            QFont font = painter.font();
            font.setPixelSize(11);
            painter.setFont(font);
            painter.drawText(screen, Qt::AlignCenter, QStringLiteral("Display"));
        }
    }

private:
    bool m_display{false};
};

} // namespace

KnobsSetupPage::KnobsSetupPage(ControllerHost* host, HotKeyBindings* bindings, QWidget* parent)
    : SetupPage(QStringLiteral("Knobs"), parent)
    , m_host(host)
    , m_bindings(bindings)
    , m_usbSupported(DeviceInput::supported())
    , m_serialSupported(FlexControlInput::supported())
    , m_platform(thisPlatform())
{
    auto* const statement = new QLabel(
        QStringLiteral("Knobs plug into this computer, so they work the same in a remote window."));
    statement->setObjectName(QStringLiteral("knobsThisComputer"));
    statement->setWordWrap(true);
    statement->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    addContent(statement);

    auto* const columns = new QHBoxLayout;
    columns->setSpacing(12);

    auto* const listFrame = new QWidget;
    listFrame->setObjectName(QStringLiteral("knobsDevices"));
    listFrame->setFixedWidth(kListWidth);
    buildList(listFrame);
    columns->addWidget(listFrame, 0, Qt::AlignTop);

    auto* const right = new QVBoxLayout;
    right->setSpacing(8);

    m_state = new QLabel;
    m_state->setObjectName(QStringLiteral("knobsState"));
    m_state->setWordWrap(true);
    m_state->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; color: #c8d8e8; }"));
    right->addWidget(m_state);

    m_note = new QLabel;
    m_note->setObjectName(QStringLiteral("knobsNote"));
    m_note->setWordWrap(true);
    m_note->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    right->addWidget(m_note);

    m_drawing = new KnobDrawing(this);
    m_drawing->setObjectName(QStringLiteral("knobsDrawing"));
    right->addWidget(m_drawing);

    m_table = new QTableWidget(0, kColumnCount);
    m_table->setObjectName(QStringLiteral("knobsList"));
    m_table->setProperty("nereusSetupId", QStringLiteral("keys.knobs.controls"));
    m_table->setHorizontalHeaderLabels({QStringLiteral("Control"), QStringLiteral("Gesture"), QStringLiteral("Action"),
                                        QStringLiteral("Other direction"), QStringLiteral("Acts on")});
    m_table->setStyleSheet(listStyle());
    m_table->setShowGrid(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);     // chooseControl picks the rows
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(kRowHeight);
    QHeaderView* const header = m_table->horizontalHeader();
    header->setFixedHeight(kHeaderHeight);
    header->setSectionResizeMode(kColumnControl, QHeaderView::Fixed);
    header->setSectionResizeMode(kColumnGesture, QHeaderView::Fixed);
    header->setSectionResizeMode(kColumnAction, QHeaderView::Stretch);
    header->setSectionResizeMode(kColumnDown, QHeaderView::Stretch);
    header->setSectionResizeMode(kColumnActsOn, QHeaderView::Fixed);
    m_table->setColumnWidth(kColumnControl, 80);
    m_table->setColumnWidth(kColumnGesture, 76);
    m_table->setColumnWidth(kColumnActsOn, 118);
    connect(m_table, &QTableWidget::cellClicked, this, [this](int tableRow, int) {
        if (tableRow >= 0 && tableRow < int(m_rows.size())) {
            chooseControl(m_rows[std::size_t(tableRow)].control);
        }
    });
    right->addWidget(m_table);

    m_status = new QLabel;
    m_status->setObjectName(QStringLiteral("knobsStatus"));
    m_status->setWordWrap(true);
    m_status->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    right->addWidget(m_status);

    buildBehavior();
    right->addWidget(m_behavior);

    columns->addLayout(right, 1);
    addContent(columns);

    if (m_host) {
        connect(m_host.data(), &ControllerHost::deviceStateChanged, this, &KnobsSetupPage::refreshStates);
        HotKeyDeviceBindings* const bound = store();
        connect(bound, &HotKeyDeviceBindings::bindingsChanged, this, [this] {
            syncRows();
            syncBehavior();
        });
        connect(bound, &HotKeyDeviceBindings::bindingRefused, this,
                [this](DeviceId, const QString&, ControlGesture, const QString& reason) { setStatus(reason); });
    }
    if (m_bindings) {
        connect(m_bindings.data(), &HotKeyBindings::enabledChanged, this, &KnobsSetupPage::refreshStates);
    }

    // A build with no USB support opens on the first device it can use.
    const QList<DeviceId> devices = HotKeyDeviceBindings::devices();
    const auto usable = std::find_if(devices.cbegin(), devices.cend(), [this](DeviceId id) { return deviceSupported(id); });
    showDevice(usable != devices.cend() ? *usable : m_device);
}

KnobsSetupPage::~KnobsSetupPage() = default;

ControllerHost* KnobsSetupPage::controllerHost() const
{
    return m_host;
}

KnobsSetupPage::Platform KnobsSetupPage::thisPlatform()
{
#if defined(Q_OS_WIN)
    return Platform::Windows;
#elif defined(Q_OS_LINUX)
    return Platform::Linux;
#else
    return Platform::Mac;
#endif
}

QString KnobsSetupPage::stateWord(bool connected, const QString& reason)
{
    if (connected) {
        return QStringLiteral("Connected");
    }
    if (reason.isEmpty() || reason == notPluggedIn()) {
        return notPluggedIn();
    }
    if (heldByAnotherApp(reason)) {
        return QStringLiteral("Another app has it");
    }
    if (reason == QLatin1String("A deck plugin is using it.")) {
        return QStringLiteral("A deck plugin has it");
    }
    return QStringLiteral("Can't open it");
}

QString KnobsSetupPage::appToQuit(DeviceId id, Platform platform)
{
    // The apps the design names for these two (its section 7.5, and the mockup's plugin
    // rows give which system each runs on). No source names one for the other six.
    switch (id) {
    case DeviceId::StreamDeckPlus:
        return platform == Platform::Linux ? QStringLiteral("StreamController") : QStringLiteral("the Stream Deck app");
    case DeviceId::UlanziDial:
        return platform == Platform::Linux ? QString() : QStringLiteral("Ulanzi Studio");
    default:
        return {};
    }
}

QString KnobsSetupPage::stateFix(DeviceId id, bool connected, const QString& reason, Platform platform)
{
    if (connected || reason.isEmpty() || reason == notPluggedIn()) {
        return {};
    }
    if (heldByAnotherApp(reason)) {
        const QString app = appToQuit(id, platform);
        return QStringLiteral("Another app has this device. Quit %1, and it connects on its own.")
            .arg(app.isEmpty() ? QStringLiteral("the other app using it") : app);
    }
    return reason;
}

QString KnobsSetupPage::deviceNote(DeviceId id, bool connected, const QString& reason, Platform platform)
{
    if (id == DeviceId::FlexControl && platform == Platform::Linux) {
        // Group names: packaging/nereusd.service.in:167-171. That dialout is what stands in
        // the way: AetherSDR src/core/FlexControlManager.h:65-66 [@1e0718ad].
        return QStringLiteral("On Linux it shows up as a serial port, so your account must be in the dialout group "
                              "(uucp on Arch). Add it, then log out and back in.");
    }
    if (id != DeviceId::UlanziDial) {
        return {};
    }
    switch (platform) {
    case Platform::Windows:
        // Design 7.6, from src/gui/hotkeys/devices/UlanziWindowsInput.h:71-73.
        return QStringLiteral("On Windows its keys also reach the focused window, and its Ctrl chords may not arrive.");
    case Platform::Linux:
        return QStringLiteral("On Linux it needs an access rule. Copy 60-nereussdr-controllers.rules to "
                              "/etc/udev/rules.d, then plug it in again.");
    case Platform::Mac: {
        const bool here = connected || (!reason.isEmpty() && reason != notPluggedIn());
        if (!here || blockedByMac(reason)) {
            return {};
        }
        return QStringLiteral("macOS asks for Input Monitoring for this dial. Turn on NereusSDR in System Settings, "
                              "under Privacy & Security, Input Monitoring.");
    }
    }
    return {};
}

void KnobsSetupPage::buildList(QWidget* parent)
{
    auto* const layout = new QVBoxLayout(parent);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    for (const DeviceId id : HotKeyDeviceBindings::devices()) {
        auto* const row = new KnobDeviceRow(HotKeyDeviceBindings::deviceName(id), parent);
        row->setObjectName(QStringLiteral("knobDevice:") + HotKeyDeviceBindings::deviceKey(id));
        connect(row, &QAbstractButton::clicked, this, [this, id] { selectDevice(id); });
        layout->addWidget(row);
        m_deviceRows.insert(id, row);
    }
}

void KnobsSetupPage::buildBehavior()
{
    m_behavior = addSection(QStringLiteral("Behavior"));
    m_behavior->setObjectName(QStringLiteral("knobsBehavior"));
    auto* const layout = qobject_cast<QVBoxLayout*>(m_behavior->layout());

    auto* const speedRow = new QHBoxLayout;
    speedRow->setSpacing(6);
    speedRow->addWidget(new QLabel(QStringLiteral("Speed:"), m_behavior));
    m_speed = new QSlider(Qt::Horizontal, m_behavior);
    m_speed->setObjectName(QStringLiteral("knobSpeed"));
    m_speed->setProperty("nereusSetupId", QStringLiteral("keys.knobs.speed"));
    m_speed->setAccessibleName(QStringLiteral("Speed"));
    m_speed->setRange(kSpeedFastest, kSpeedSlowest);
    m_speed->setPageStep(1);
    m_speed->setStyleSheet(QString::fromLatin1(Style::kSliderStyle));
    m_speed->setFixedWidth(110);
    m_speed->installEventFilter(this);
    speedRow->addWidget(m_speed);
    m_speedValue = new QLabel(m_behavior);
    m_speedValue->setObjectName(QStringLiteral("knobSpeedValue"));
        speedRow->addWidget(m_speedValue);
    speedRow->addStretch();
    layout->addLayout(speedRow);

    auto* const checksRow = new QHBoxLayout;
    checksRow->setSpacing(14);
    m_reverse = new QCheckBox(QStringLiteral("Reverse direction"), m_behavior);
    m_reverse->setObjectName(QStringLiteral("knobReverse"));
    m_reverse->setProperty("nereusSetupId", QStringLiteral("keys.knobs.reverse"));
    m_reverse->setStyleSheet(checkBoxStyle());
    checksRow->addWidget(m_reverse);

    m_lights = new QCheckBox(m_behavior);
    m_lights->setObjectName(QStringLiteral("knobLights"));
    m_lights->setProperty("nereusSetupId", QStringLiteral("keys.knobs.lights"));
    m_lights->setStyleSheet(checkBoxStyle());
    checksRow->addWidget(m_lights);
    checksRow->addStretch();
    layout->addLayout(checksRow);

    // The slider's right end is the fast end, though a higher saved number is slower.
    connect(m_speed, &QSlider::valueChanged, this, [this](int value) {
        if (HotKeyDeviceBindings* const bound = store()) {
            bound->setSpeed(m_device, kSpeedSlowest + kSpeedFastest - value);
        }
    });
    connect(m_reverse, &QCheckBox::toggled, this, [this](bool on) {
        if (HotKeyDeviceBindings* const bound = store()) {
            bound->setReversed(m_device, on);
        }
    });
    connect(m_lights, &QCheckBox::toggled, this, [this](bool on) {
        if (m_host) {
            m_host->setLightsEnabled(m_device, on);
        }
    });
}

bool KnobsSetupPage::selectDevice(DeviceId id)
{
    if (!deviceSupported(id)) {
        return false;
    }
    if (id != m_device) {
        showDevice(id);
    }
    return true;
}

void KnobsSetupPage::showDevice(DeviceId id)
{
    m_device = id;
    setStatus({});

    for (QWidget* const cap : m_controlCaps) {
        cap->setObjectName({});
        delete cap;
    }
    m_controlCaps.clear();
    const DeviceDrawing drawing = drawingOf(id);
    static_cast<KnobDrawing*>(m_drawing)->setShowsDisplay(drawing.display);
    for (int i = 0; i < drawing.controls.size(); ++i) {
        auto* const cap = new KnobControlCap(drawing.controls.at(i), m_drawing);
        cap->setObjectName(QStringLiteral("knobCap:") + drawing.controls.at(i).name);
        connect(cap, &QAbstractButton::clicked, this, [this, i] { chooseControl(i); });
        cap->show();
        m_controlCaps.push_back(cap);
    }

    buildRows();
    syncBehavior();
    refreshStates();
    chooseControl(0);
}

void KnobsSetupPage::buildRows()
{
    for (int tableRow = 0; tableRow < m_table->rowCount(); ++tableRow) {
        for (int column = 0; column < kColumnCount; ++column) {
            retireNames(m_table->cellWidget(tableRow, column));
        }
    }
    m_table->setRowCount(0);
    m_rows.clear();

    const DeviceDrawing drawing = drawingOf(m_device);
    for (int i = 0; i < drawing.controls.size(); ++i) {
        const ControlSpec& spec = drawing.controls.at(i);
        for (const ControlRowSpec& row : spec.rows) {
            addRow(i, row.control, row.gesture);
            const int tableRow = m_table->rowCount() - 1;
            auto* const control = new QTableWidgetItem(spec.name);
            control->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            m_table->setItem(tableRow, kColumnControl, control);
            auto* const gesture = new QTableWidgetItem(gestureName(row.gesture));
            gesture->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            m_table->setItem(tableRow, kColumnGesture, gesture);
        }
    }
    const int shown = std::min(int(m_rows.size()), kMostRowsShown);
    m_table->setFixedHeight(kHeaderHeight + shown * kRowHeight + 2);
    syncRows();
}

void KnobsSetupPage::addRow(int control, const QString& controlKey, ControlGesture gesture)
{
    const int tableRow = m_table->rowCount();
    m_table->insertRow(tableRow);
    const int rowIndex = int(m_rows.size());

    Row row;
    row.control = control;
    row.controlKey = controlKey;
    row.gesture = gesture;

    const auto makeCombo = [this](const QString& name, const QString& accessible) {
        auto* const combo = new QComboBox;
        combo->setObjectName(name);
        combo->setAccessibleName(accessible);
        // The list's columns are short of room, so the arrow's empty place gives it back.
        combo->setStyleSheet(comboStyle() + QStringLiteral("QComboBox::drop-down { border: none; width: 6px; }"));
        combo->installEventFilter(this);
        return combo;
    };

    row.action = makeCombo(rowName(QStringLiteral("Action"), controlKey, gesture), QStringLiteral("Action"));
    fillActionCombo(row.action, QStringLiteral("None"));
    markActions(row.action, row);
    m_table->setCellWidget(tableRow, kColumnAction, makeCell(row.action));
    connect(row.action, &QComboBox::activated, this, [this, rowIndex](int index) {
        onActionChosen(rowIndex, m_rows[std::size_t(rowIndex)].action->itemData(index).toString());
    });

    if (gesture == ControlGesture::Turn) {
        row.down = makeCombo(rowName(QStringLiteral("Down"), controlKey, gesture), QStringLiteral("Other direction"));
        fillActionCombo(row.down, QStringLiteral("None"));
        markActions(row.down, row);
        m_table->setCellWidget(tableRow, kColumnDown, makeCell(row.down));
        connect(row.down, &QComboBox::activated, this, [this, rowIndex](int index) {
            onDownChosen(rowIndex, m_rows[std::size_t(rowIndex)].down->itemData(index).toString());
        });
    } else if (m_device == DeviceId::Rc28 && controlKey == QLatin1String("tx")) {
        // The one place the TX bar's setting can be read beside what it changes.
        row.staysOn = new QCheckBox(QStringLiteral("Stays on"));
        row.staysOn->setObjectName(QStringLiteral("knobStaysOn"));
        row.staysOn->setProperty("nereusSetupId", QStringLiteral("keys.knobs.txStaysOn"));
        row.staysOn->setAccessibleName(QStringLiteral("TX bar stays on"));
        row.staysOn->setStyleSheet(checkBoxStyle());
        m_table->setCellWidget(tableRow, kColumnDown, makeCell(row.staysOn));
        connect(row.staysOn, &QCheckBox::toggled, this, [this](bool on) {
            if (HotKeyDeviceBindings* const bound = store()) {
                bound->setRc28TxStaysOn(on);
            }
        });
    } else {
        auto* const none = new QTableWidgetItem(QStringLiteral("None"));
        none->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        none->setForeground(WorkspacePreview::roleColour(QColor(0x5a, 0x6a, 0x7a), "text-inactive"));
        none->setToolTip(QStringLiteral("Only a control you turn has another direction."));
        m_table->setItem(tableRow, kColumnDown, none);
    }

    row.actsOn = makeCombo(rowName(QStringLiteral("ActsOn"), controlKey, gesture), QStringLiteral("Acts on"));
    m_table->setCellWidget(tableRow, kColumnActsOn, makeCell(row.actsOn));
    connect(row.actsOn, &QComboBox::activated, this, [this, rowIndex](int index) {
        onActsOnChosen(rowIndex, m_rows[std::size_t(rowIndex)].actsOn->itemData(index).toString());
    });

    m_rows.push_back(row);
}

// An action this row can't run is shown disabled, with why.
void KnobsSetupPage::markActions(QComboBox* combo, const Row& row) const
{
    auto* const model = qobject_cast<QStandardItemModel*>(combo->model());
    if (model == nullptr) {
        return;
    }
    const bool flexHold = m_device == DeviceId::FlexControl && row.gesture == ControlGesture::Hold;
    for (int i = 1; i < combo->count(); ++i) {
        const QString id = combo->itemData(i).toString();
        QStandardItem* const item = model->item(i);
        if (id.isEmpty() || item == nullptr) {
            continue;
        }
        QString reason = HotKeyDeviceBindings::refusalFor(row.gesture, id, QString());
        if (reason.isEmpty() && flexHold) {
            // The device says it was held once it's over, so no release follows.
            const HotKeyAction* const action = HotKeyActionList::find(id);
            if (action != nullptr && action->gesture == HotKeyGesture::Hold) {
                reason = QStringLiteral("This hold can't run %1, since the FlexControl sends no release.").arg(action->name);
            }
        }
        if (reason.isEmpty() && m_reason) {
            reason = m_reason(id);
        }
        const Qt::ItemFlags usable = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        item->setFlags(reason.isEmpty() ? item->flags() | usable : item->flags() & ~usable);
        item->setToolTip(reason);
    }
}

void KnobsSetupPage::syncRows()
{
    for (Row& row : m_rows) {
        syncRow(row);
    }
}

void KnobsSetupPage::syncRow(Row& row)
{
    const HotKeyDeviceBinding binding = bindingOf(row);
    setComboData(row.action, binding.actionId);
    row.action->setToolTip(row.action->currentText());
    if (row.down != nullptr) {
        setComboData(row.down, binding.actionIdDown);
        row.down->setToolTip(row.down->currentText());
    }

    const HotKeyAction* const action = HotKeyActionList::find(binding.actionId);
    const QSignalBlocker blocker(row.actsOn);
    row.actsOn->clear();
    if (action == nullptr) {
        row.actsOn->addItem(QStringLiteral("None"));
        row.actsOn->setEnabled(false);
        setReason(row.actsOn, QStringLiteral("Choose an action first."));
    } else if (action->scope == HotKeyScope::Slice) {
        row.actsOn->addItem(QStringLiteral("Selected slice"), QString());
        for (const QChar letter : {QLatin1Char('A'), QLatin1Char('B'), QLatin1Char('C'), QLatin1Char('D')}) {
            row.actsOn->addItem(QStringLiteral("Slice %1").arg(letter), QString(letter));
        }
        row.actsOn->setCurrentIndex(binding.pin.isNull() ? 0 : std::max(0, row.actsOn->findData(QString(binding.pin))));
        row.actsOn->setEnabled(true);
        setReason(row.actsOn, QString());
    } else {
        row.actsOn->addItem(fixedTargetName(action->scope));
        row.actsOn->setEnabled(false);
        setReason(row.actsOn, fixedTargetReason(action->scope));
    }

    if (row.staysOn != nullptr) {
        const QSignalBlocker staysBlocker(row.staysOn);
        HotKeyDeviceBindings* const bound = store();
        row.staysOn->setChecked(bound != nullptr && bound->rc28TxStaysOn());
        const HotKeyAction* const hold = HotKeyActionList::find(QStringLiteral("tx.hold"));
        const bool canStay = binding.actionId == QLatin1String("tx.hold");
        row.staysOn->setEnabled(canStay);
        setReason(row.staysOn, canStay ? QStringLiteral("One press keys and the next lets go.")
                                       : QStringLiteral("Only %1 can stay on.").arg(hold != nullptr ? hold->name : QString()));
    }
}

void KnobsSetupPage::syncBehavior()
{
    HotKeyDeviceBindings* const bound = store();
    {
        const QSignalBlocker speedBlocker(m_speed);
        const QSignalBlocker reverseBlocker(m_reverse);
        const int speed = bound != nullptr ? bound->speed(m_device) : kSpeedFastest;
        m_speed->setValue(kSpeedSlowest + kSpeedFastest - std::min(speed, kSpeedSlowest));
        m_speedValue->setText(speedText(speed));
        m_reverse->setChecked(bound != nullptr && bound->reversed(m_device));
    }
    const QSignalBlocker lightsBlocker(m_lights);
    const bool hasLights = ControllerHost::hasLights(m_device);
    m_lights->setText(lightsText(m_device));
    m_lights->setChecked(hasLights && m_host && m_host->lightsEnabled(m_device));
    applyAvailability();
}

void KnobsSetupPage::chooseControl(int control)
{
    if (control < 0 || control >= int(m_controlCaps.size())) {
        return;
    }
    static_cast<QAbstractButton*>(m_controlCaps[std::size_t(control)])->setChecked(true);
    QItemSelection selection;
    int first = -1;
    for (int i = 0; i < int(m_rows.size()); ++i) {
        if (m_rows[std::size_t(i)].control == control) {
            first = first < 0 ? i : first;
            const QModelIndex left = m_table->model()->index(i, 0);
            const QModelIndex right = m_table->model()->index(i, kColumnCount - 1);
            selection.select(left, right);
        }
    }
    m_table->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
    if (first >= 0) {
        m_table->scrollTo(m_table->model()->index(first, 0));
    }
}

void KnobsSetupPage::refreshStates()
{
    for (const DeviceId id : HotKeyDeviceBindings::devices()) {
        auto* const row = static_cast<KnobDeviceRow*>(m_deviceRows.value(id));
        const bool supported = deviceSupported(id);
        const bool connected = connectedNow(id);
        const QString reason = reasonNow(id);
        const QString word = supported ? stateWord(connected, reason) : QStringLiteral("Not in this build");
        // R-HK-19: this line follows testedOnRealDevice and nothing else, and that says
        // false until a real device has been tried and the code is changed to say so.
        const QString tested = HotKeyDeviceBindings::testedOnRealDevice(id) ? QStringLiteral("Tested")
                                                                            : QStringLiteral("Untested");
        row->setStateLine(QStringLiteral("%1. %2.").arg(word, tested));
        row->setEnabled(supported);
        const QString fix = supported ? stateFix(id, connected, reason, m_platform) : unsupportedReason(id);
        setReason(row, fix);
        row->setChecked(id == m_device);
    }

    const bool supported = deviceSupported(m_device);
    const bool connected = connectedNow(m_device);
    const QString reason = reasonNow(m_device);
    const QString fix = supported ? stateFix(m_device, connected, reason, m_platform) : unsupportedReason(m_device);
    m_state->setText(fix);
    m_state->setVisible(!fix.isEmpty());
    const QString note = deviceNote(m_device, connected, reason, m_platform);
    m_note->setText(note);
    m_note->setVisible(!note.isEmpty());
    applyAvailability();
}

void KnobsSetupPage::applyAvailability()
{
    const QString reason = unavailableReason();
    const bool usable = reason.isEmpty();
    m_table->setEnabled(usable);
    setReason(m_table, reason);
    m_behavior->setEnabled(usable);
    setReason(m_behavior, reason);

    const bool hasLights = ControllerHost::hasLights(m_device);
    m_lights->setEnabled(usable && hasLights);
    setReason(m_lights, !usable       ? reason
                        : !hasLights ? QStringLiteral("NereusSDR doesn't drive any lights on this device.")
                                     : QString());

    // Says why when the device isn't what's in the way.
    if (!usable && deviceSupported(m_device)) {
        m_state->setText(reason);
        m_state->setVisible(true);
    }
}

void KnobsSetupPage::setUnavailableReasonProvider(ReasonProvider provider)
{
    m_reason = std::move(provider);
    refreshAvailability();
}

void KnobsSetupPage::refreshAvailability()
{
    for (const Row& row : m_rows) {
        markActions(row.action, row);
        if (row.down != nullptr) {
            markActions(row.down, row);
        }
    }
    refreshStates();
}

void KnobsSetupPage::setStatus(const QString& text)
{
    m_status->setText(text);
    m_status->setVisible(!text.isEmpty());
}

void KnobsSetupPage::onActionChosen(int rowIndex, const QString& id)
{
    Row& row = m_rows[std::size_t(rowIndex)];
    chooseControl(row.control);
    HotKeyDeviceBinding binding = bindingOf(row);
    // The other direction follows only if it was this action's own.
    if (row.gesture == ControlGesture::Turn && binding.actionIdDown == MidiProfile::otherDirection(binding.actionId)) {
        binding.actionIdDown = MidiProfile::otherDirection(id);
    }
    binding.actionId = id;
    const HotKeyAction* const action = HotKeyActionList::find(id);
    if (action == nullptr || action->scope != HotKeyScope::Slice) {
        binding.pin = QChar();
    }
    setStatus({});
    store()->setBinding(m_device, row.controlKey, row.gesture, binding);
    syncRow(row);   // a refusal changed nothing, so the row goes back
}

void KnobsSetupPage::onDownChosen(int rowIndex, const QString& id)
{
    Row& row = m_rows[std::size_t(rowIndex)];
    chooseControl(row.control);
    HotKeyDeviceBinding binding = bindingOf(row);
    binding.actionIdDown = id;
    setStatus({});
    store()->setBinding(m_device, row.controlKey, row.gesture, binding);
    syncRow(row);
}

void KnobsSetupPage::onActsOnChosen(int rowIndex, const QString& letter)
{
    Row& row = m_rows[std::size_t(rowIndex)];
    chooseControl(row.control);
    HotKeyDeviceBinding binding = bindingOf(row);
    binding.pin = letter.isEmpty() ? QChar() : letter.at(0);
    setStatus({});
    store()->setBinding(m_device, row.controlKey, row.gesture, binding);
    syncRow(row);
}

bool KnobsSetupPage::deviceSupported(DeviceId id) const
{
    switch (id) {
    case DeviceId::FlexControl:
        return m_serialSupported;
    case DeviceId::UlanziDial:
        // Only its Windows backend uses hidapi (UlanziWindowsInput.cpp:37).
        return m_platform != Platform::Windows || m_usbSupported;
    default:
        return m_usbSupported;
    }
}

QString KnobsSetupPage::unsupportedReason(DeviceId id) const
{
    if (deviceSupported(id)) {
        return {};
    }
    return id == DeviceId::FlexControl ? FlexControlInput::unsupportedReason() : DeviceInput::unsupportedReason();
}

QString KnobsSetupPage::unavailableReason() const
{
    if (!m_host || !m_bindings) {
        return QStringLiteral("Knobs belong to a radio window, and this page has none.");
    }
    if (!deviceSupported(m_device)) {
        return unsupportedReason(m_device);
    }
    if (!m_bindings->enabled()) {
        return switchedOffReason();
    }
    return {};
}

HotKeyDeviceBindings* KnobsSetupPage::store() const
{
    return m_host ? m_host->deviceBindings() : nullptr;
}

HotKeyDeviceBinding KnobsSetupPage::bindingOf(const Row& row) const
{
    HotKeyDeviceBindings* const bound = store();
    return bound != nullptr ? bound->binding(m_device, row.controlKey, row.gesture) : HotKeyDeviceBinding{};
}

bool KnobsSetupPage::connectedNow(DeviceId id) const
{
    return m_host && m_host->deviceConnected(id);
}

QString KnobsSetupPage::reasonNow(DeviceId id) const
{
    return m_host ? m_host->deviceReason(id) : QString();
}

void KnobsSetupPage::setSupportedForTest(bool usb, bool serial)
{
    m_usbSupported = usb;
    m_serialSupported = serial;
    if (!deviceSupported(m_device)) {
        for (const DeviceId id : HotKeyDeviceBindings::devices()) {
            if (deviceSupported(id)) {
                showDevice(id);
                return;
            }
        }
    }
    refreshStates();
}

void KnobsSetupPage::setPlatformForTest(Platform platform)
{
    m_platform = platform;
    refreshStates();
}

bool KnobsSetupPage::eventFilter(QObject* watched, QEvent* event)
{
    // Scrolling the page with the pointer over a row must not change that row.
    if (event->type() == QEvent::Wheel && (qobject_cast<QComboBox*>(watched) != nullptr || watched == m_speed)) {
        event->ignore();
        return true;
    }
    return SetupPage::eventFilter(watched, event);
}

} // namespace NereusSDR
