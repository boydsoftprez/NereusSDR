// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/recordings/VoiceButton.cpp  (NereusSDR)
// =================================================================
//
// See VoiceButton.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 12.
//   2026-11-16 - J.J. Boyd (KG4VCF), against the desktop-voice-button
//                 mockup: an older Core dims VOICE as a whole, and held
//                 elsewhere the box's messages dim with Take transmit live.
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "gui/recordings/VoiceButton.h"

#include <QApplication>
#include <QCursor>
#include <QElapsedTimer>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#include "gui/StyleConstants.h"
#include "gui/applets/TxKeyingSection.h"
#include "models/recordings/RecordingsAccess.h"

namespace NereusSDR {

namespace {

// The mockup's #vBtn.off and .t.off (desktop-voice-button/index.html).
constexpr qreal kDimmedOpacity = 0.45;
constexpr qreal kHeldTileOpacity = 0.42;
constexpr int kTileHeight = 37;
constexpr int kBoxGap = 4;
constexpr int kProgressHeight = 3;
// How long the press that closed the box counts as a press on VOICE.
constexpr int kPressReplayMs = 300;

constexpr auto kBoxSheet = R"(
QFrame#VoiceBox { background: #16161a; border: 1px solid #3a3a42; }
QFrame#VoiceBox QLabel { background: transparent; }
QLabel#VoiceBoxTitle { color: #cfcfd4; font-size: 12px; font-weight: 700; letter-spacing: 1px; }
QPushButton#VoiceBoxPanelLink { background: transparent; border: 0; color: #4aa3ff; font-size: 12px; padding: 0; }
QPushButton#VoiceBoxPanelLink:hover { color: #7cbcff; }
QPushButton#VoiceTile { text-align: left; border-radius: 5px; border: 1px solid #2c2c31;
    border-top-color: #38383e; padding: 0;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #212126, stop:1 #151518); }
QPushButton#VoiceTile:hover { border-color: #4a4a52; }
QPushButton#VoiceTile[look="blank"] { background: #111114; border-style: dashed; }
QPushButton#VoiceTile[look="sending"] { border-color: #ff453a;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #7a1a14, stop:1 #5a1410); }
QLabel#VoiceTileNumber { color: #8e8e96; font-size: 9px; font-weight: 700; }
QLabel#VoiceTileName { color: #e8e8ec; font-size: 12px; font-weight: 600; }
QLabel#VoiceTileName[look="blank"] { color: #5e5e64; font-weight: 400; }
QLabel#VoiceTileLength { color: #8e8e96; font-size: 9px; font-family: Menlo, "SF Mono", monospace; }
QLabel#VoiceTileNumber[look="sending"], QLabel#VoiceTileLength[look="sending"] { color: #ffd0cc; }
QFrame#VoiceTileProgress { background: #ff453a; border: 0; }
QFrame#VoiceBoxHolder { background: #2a2410; border: 1px solid #6a5a18; border-radius: 5px; }
QLabel#VoiceBoxHolderNote { color: #e8d9a0; font-size: 10px; }
QPushButton#VoiceBoxTake { min-height: 25px; border-radius: 5px; border: 0; background: #e8c02a; color: #111111;
    font-size: 11px; font-weight: 700; }
QLabel#VoiceBoxNote { color: #ffd0cc; font-size: 10px; }
QFrame#VoiceBoxRule { background: #2a2a30; border: 0; min-height: 1px; max-height: 1px; }
QLabel#VoiceBoxRepeat { color: #dcdce1; font-size: 11px; }
QLabel#VoiceBoxRepeatSub { color: #8e8e96; font-size: 9px; }
QPushButton#VoiceBoxRepeatToggle { min-width: 38px; max-width: 38px; min-height: 22px; max-height: 22px;
    border-radius: 11px; border: 0; background: #3a3a40; color: #dcdce1; font-size: 9px; font-weight: 700; }
QPushButton#VoiceBoxRepeatToggle:checked { background: #30a14e; color: #ffffff; }
QLabel#VoiceBoxFoot { color: #8e8e96; font-size: 9px; }
)";

void restyle(QWidget* w)
{
    w->style()->unpolish(w);
    w->style()->polish(w);
}

void setLook(QWidget* w, const char* look)
{
    if (w->property("look").toString() != QLatin1String(look)) {
        w->setProperty("look", QLatin1String(look));
        restyle(w);
    }
}

// The mockup's five sound bars (a 26 by 20 drawing), at half size.
QIcon waveIcon(const QColor& colour)
{
    const qreal dpr = std::max<qreal>(2.0, qApp != nullptr ? qApp->devicePixelRatio() : 1.0);
    const QSize size(13, 10);
    QPixmap pixmap(size * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size.width() / 26.0, size.height() / 20.0);
    QPen pen(colour, 2.6);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    const struct { qreal x, y1, y2; } bars[] = {{3, 8, 12}, {8, 4, 16}, {13, 1.5, 18.5}, {18, 5, 15}, {23, 8, 12}};
    for (const auto& b : bars) {
        p.drawLine(QPointF(b.x, b.y1), QPointF(b.x, b.y2));
    }
    return QIcon(pixmap);
}

QString baseStyle()
{
    return Style::buttonBaseStyle() + QStringLiteral("QPushButton { padding: 2px; }");
}

}  // namespace

// ---- VoiceKeyerSnapshot ----

VoiceKeyerSnapshot VoiceKeyerSnapshot::from(const QJsonObject& recordingsDocument)
{
    VoiceKeyerSnapshot s;
    for (int i = 0; i < static_cast<int>(s.messages.size()); ++i) {
        s.messages[static_cast<size_t>(i)].n = i + 1;
    }
    const QJsonObject vk = recordingsDocument.value(QStringLiteral("voiceKeyer")).toObject();
    for (const QJsonValue& value : vk.value(QStringLiteral("slots")).toArray()) {
        const QJsonObject o = value.toObject();
        const int n = o.value(QStringLiteral("n")).toInt(0);
        if (n < 1 || n > static_cast<int>(s.messages.size())) {
            continue;
        }
        Slot& slot = s.messages[static_cast<size_t>(n - 1)];
        slot.name = o.value(QStringLiteral("name")).toString();
        slot.lengthMs = o.value(QStringLiteral("lengthMs")).toInteger();
        slot.empty = o.value(QStringLiteral("empty")).toBool(true);
    }
    s.repeat = vk.value(QStringLiteral("repeat")).toBool(false);
    s.repeatDelaySec = vk.value(QStringLiteral("repeatDelaySec")).toInt(5);
    const QJsonValue sendingValue = vk.value(QStringLiteral("sending"));
    if (sendingValue.isObject()) {
        const QJsonObject o = sendingValue.toObject();
        Sending sending;
        sending.n = o.value(QStringLiteral("n")).toInt(0);
        sending.positionMs = o.value(QStringLiteral("positionMs")).toInteger();
        sending.lengthMs = o.value(QStringLiteral("lengthMs")).toInteger();
        sending.waitingMs = o.value(QStringLiteral("waitingMs")).toInteger();
        sending.by = o.value(QStringLiteral("by")).toString();
        sending.deviceId = o.value(QStringLiteral("deviceId")).toString();
        if (sending.n >= 1 && sending.n <= static_cast<int>(s.messages.size())) {
            s.sending = sending;
        }
    }
    return s;
}

QString VoiceKeyerSnapshot::label(int n) const
{
    if (n < 1 || n > static_cast<int>(messages.size())) {
        return QString();
    }
    const QString name = messages[static_cast<size_t>(n - 1)].name;
    return name.isEmpty() ? QStringLiteral("M%1").arg(n) : name;
}

QString VoiceKeyerSnapshot::clockText(qint64 ms)
{
    const qint64 s = std::max<qint64>(0, ms) / 1000;
    return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
}

// ---- VoiceButton ----

VoiceButton::VoiceButton(TxKeyingSection* section, RecordingsAccess* access)
    : QObject(section), m_section(section)
{
    setObjectName(QStringLiteral("VoiceButton"));
    m_now = [] {
        static QElapsedTimer clock;
        if (!clock.isValid()) {
            clock.start();
        }
        return clock.elapsed();
    };
    m_timer = new QTimer(this);
    m_timer->setInterval(kRefreshMs);
    connect(m_timer, &QTimer::timeout, this, &VoiceButton::refresh);
    if (QPushButton* voice = button()) {
        m_icon = waveIcon(QColor(0xdc, 0xdc, 0xe1));
        voice->setIcon(m_icon);
        voice->setIconSize(QSize(13, 10));
        voice->installEventFilter(this);
        connect(voice, &QPushButton::clicked, this, &VoiceButton::onVoiceClicked);
    }
    // MOX stops a message going out, as VOICE does (spec 6.6).
    if (section != nullptr && section->moxButton() != nullptr) {
        connect(section->moxButton(), &QPushButton::clicked, this, [this]() {
            if (m_snapshot.sending.has_value()) {
                stopSending();
            }
        });
    }
    buildBox();
    setAccess(access);
}

VoiceButton::~VoiceButton() = default;

QPushButton* VoiceButton::button() const
{
    return m_section ? m_section->voiceButton() : nullptr;
}

RecordingsAccess* VoiceButton::access() const
{
    return m_access.data();
}

void VoiceButton::setAccess(RecordingsAccess* access)
{
    for (const QMetaObject::Connection& c : m_accessConnections) {
        disconnect(c);
    }
    m_accessConnections.clear();
    m_access = access;
    m_awaitingAnswer = false;
    m_lastRefusal.clear();
    if (access != nullptr) {
        m_accessConnections = {
            connect(access, &RecordingsAccess::documentChanged, this, &VoiceButton::onDocumentChanged),
            connect(access, &RecordingsAccess::stateChanged, this, &VoiceButton::onDocumentChanged),
            connect(access, &RecordingsAccess::refused, this,
                    [this](const QString& reason) {
                        if (!m_awaitingAnswer) {
                            return;
                        }
                        m_awaitingAnswer = false;
                        m_lastRefusal = reason;
                        refreshBox();
                        emit refusalShown(reason);
                    }),
        };
    }
    onDocumentChanged();
}

void VoiceButton::setTransmitPermitted(bool permitted, const QString& reason)
{
    m_permitted = permitted;
    m_permittedReason = reason;
    refresh();
}

void VoiceButton::setTakeTransmitOffered(bool offered)
{
    m_takeOffered = offered;
    refreshBox();
}

QString VoiceButton::unavailableReason() const
{
    return m_access ? m_access->unavailableReason(kNeedsVersion) : RecordingsAccess::notConnectedReason();
}

qint64 VoiceButton::now() const
{
    return m_now ? m_now() : 0;
}

QPushButton* VoiceButton::tileForTest(int n) const
{
    return n >= 1 && n <= static_cast<int>(m_tiles.size()) ? m_tiles[static_cast<size_t>(n - 1)] : nullptr;
}

bool VoiceButton::boxOpen() const
{
    return m_box != nullptr && m_box->isVisible();
}

void VoiceButton::buildBox()
{
    // A popup window, owned by the section (gone with it).
    m_box = new QFrame(m_section, Qt::Popup | Qt::FramelessWindowHint);
    m_box->setObjectName(QStringLiteral("VoiceBox"));
    // An opaque window that paints its own background: a translucent
    // popup showed the panel underneath through it.
    m_box->setAttribute(Qt::WA_StyledBackground, true);
    m_box->setAutoFillBackground(true);
    {
        QPalette pal = m_box->palette();
        pal.setColor(QPalette::Window, QColor(0x16, 0x16, 0x1a));
        m_box->setPalette(pal);
    }
    m_box->setStyleSheet(QString::fromLatin1(kBoxSheet));
    m_box->setAccessibleName(QStringLiteral("Voice keyer messages"));
    m_box->installEventFilter(this);
    auto* box = new QVBoxLayout(m_box);
    box->setContentsMargins(8, 8, 8, 7);
    box->setSpacing(6);

    auto* head = new QHBoxLayout;
    auto* title = new QLabel(QStringLiteral("VOICE KEYER"), m_box);
    title->setObjectName(QStringLiteral("VoiceBoxTitle"));
    m_panelLink = new QPushButton(QStringLiteral("Voice keyer panel ›"), m_box);
    m_panelLink->setObjectName(QStringLiteral("VoiceBoxPanelLink"));
    m_panelLink->setCursor(Qt::PointingHandCursor);
    m_panelLink->setToolTip(QStringLiteral("Open the voice keyer panel to record, import and rename messages"));
    connect(m_panelLink, &QPushButton::clicked, this, [this]() {
        closeBox();
        emit panelRequested(0);
    });
    head->addWidget(title);
    head->addStretch(1);
    head->addWidget(m_panelLink);
    box->addLayout(head);

    auto* grid = new QGridLayout;
    grid->setSpacing(5);
    for (int i = 0; i < static_cast<int>(m_tiles.size()); ++i) {
        const int n = i + 1;
        auto* tile = new QPushButton(m_box);
        tile->setObjectName(QStringLiteral("VoiceTile"));
        tile->setProperty("message", n);
        tile->setFixedHeight(kTileHeight);
        tile->setMinimumWidth(60);
        tile->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        auto* inner = new QGridLayout(tile);
        inner->setContentsMargins(6, 3, 6, 3);
        inner->setHorizontalSpacing(4);
        inner->setVerticalSpacing(0);
        auto* number = new QLabel(QStringLiteral("M%1").arg(n), tile);
        number->setObjectName(QStringLiteral("VoiceTileNumber"));
        auto* length = new QLabel(tile);
        length->setObjectName(QStringLiteral("VoiceTileLength"));
        length->setAlignment(Qt::AlignRight | Qt::AlignTop);
        auto* name = new QLabel(tile);
        name->setObjectName(QStringLiteral("VoiceTileName"));
        name->setTextFormat(Qt::PlainText);
        for (QLabel* l : {number, length, name}) {
            l->setAttribute(Qt::WA_TransparentForMouseEvents);
        }
        name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        inner->addWidget(number, 0, 0);
        inner->addWidget(length, 0, 1);
        inner->addWidget(name, 1, 0, 1, 2);
        auto* progress = new QFrame(tile);
        progress->setObjectName(QStringLiteral("VoiceTileProgress"));
        progress->setAttribute(Qt::WA_TransparentForMouseEvents);
        progress->hide();
        m_tiles[static_cast<size_t>(i)] = tile;
        m_tileNames[static_cast<size_t>(i)] = name;
        m_tileLengths[static_cast<size_t>(i)] = length;
        m_tileProgress[static_cast<size_t>(i)] = progress;
        grid->addWidget(tile, i / 2, i % 2);
        connect(tile, &QPushButton::clicked, this, [this, n]() { onTileClicked(n); });
    }
    box->addLayout(grid);

    m_holderFrame = new QFrame(m_box);
    m_holderFrame->setObjectName(QStringLiteral("VoiceBoxHolder"));
    auto* holder = new QVBoxLayout(m_holderFrame);
    holder->setContentsMargins(7, 6, 7, 6);
    holder->setSpacing(5);
    m_holderNote = new QLabel(m_holderFrame);
    m_holderNote->setObjectName(QStringLiteral("VoiceBoxHolderNote"));
    m_holderNote->setWordWrap(true);
    m_take = new QPushButton(QStringLiteral("Take transmit"), m_holderFrame);
    m_take->setObjectName(QStringLiteral("VoiceBoxTake"));
    m_take->setAccessibleName(QStringLiteral("Take transmit"));
    m_take->setToolTip(QStringLiteral("Take transmit for this window, so a message can be sent from here"));
    connect(m_take, &QPushButton::clicked, this, [this]() { emit takeTransmitRequested(); });
    holder->addWidget(m_holderNote);
    holder->addWidget(m_take);
    m_holderFrame->hide();
    box->addWidget(m_holderFrame);

    m_note = new QLabel(m_box);
    m_note->setObjectName(QStringLiteral("VoiceBoxNote"));
    m_note->setWordWrap(true);
    m_note->hide();
    box->addWidget(m_note);

    auto* rule = new QFrame(m_box);
    rule->setObjectName(QStringLiteral("VoiceBoxRule"));
    box->addWidget(rule);
    auto* repeatRow = new QHBoxLayout;
    auto* repeatText = new QVBoxLayout;
    repeatText->setSpacing(1);
    auto* repeatLabel = new QLabel(QStringLiteral("Repeat the message"), m_box);
    repeatLabel->setObjectName(QStringLiteral("VoiceBoxRepeat"));
    m_repeatSub = new QLabel(m_box);
    m_repeatSub->setObjectName(QStringLiteral("VoiceBoxRepeatSub"));
    repeatText->addWidget(repeatLabel);
    repeatText->addWidget(m_repeatSub);
    m_repeat = new QPushButton(m_box);
    m_repeat->setObjectName(QStringLiteral("VoiceBoxRepeatToggle"));
    m_repeat->setCheckable(true);
    m_repeat->setAccessibleName(QStringLiteral("Repeat the message"));
    m_repeat->setToolTip(QStringLiteral("Send the message again after the wait, until you stop it"));
    connect(m_repeat, &QPushButton::clicked, this, [this](bool on) {
        if (m_access) {
            m_access->setVoiceRepeat(on, m_snapshot.repeatDelaySec);
        }
    });
    repeatRow->addLayout(repeatText, 1);
    repeatRow->addWidget(m_repeat, 0, Qt::AlignVCenter);
    box->addLayout(repeatRow);

    auto* foot = new QLabel(QStringLiteral("Click a message to send it. Escape, MOX or the red VOICE stops it."), m_box);
    foot->setObjectName(QStringLiteral("VoiceBoxFoot"));
    foot->setWordWrap(true);
    box->addWidget(foot);
}

void VoiceButton::onDocumentChanged()
{
    const VoiceKeyerSnapshot before = m_snapshot;
    m_snapshot = m_access ? VoiceKeyerSnapshot::from(m_access->document()) : VoiceKeyerSnapshot{};
    m_snapshotAtMs = now();
    // What was asked has happened: a later refusal is not an answer to it.
    if (m_snapshot.sending.has_value() != before.sending.has_value()) {
        m_awaitingAnswer = false;
    }
    if (m_snapshot.sending.has_value()) {
        if (!m_timer->isActive()) {
            m_timer->start();
        }
    } else {
        m_timer->stop();
    }
    refresh();
}

void VoiceButton::refresh()
{
    QPushButton* voice = button();
    if (voice == nullptr) {
        return;
    }
    const QString why = unavailableReason();
    const bool usable = why.isEmpty();
    const std::optional<VoiceKeyerSnapshot::Sending>& s = m_snapshot.sending;
    voice->setEnabled(usable);
    QString text = QStringLiteral("VOICE");
    QString style = baseStyle();
    m_fill = 0.0;
    if (usable && s.has_value()) {
        const qint64 elapsed = std::max<qint64>(0, now() - m_snapshotAtMs);
        if (s->waitingMs > 0) {
            const qint64 left = std::max<qint64>(0, s->waitingMs - elapsed);
            const qint64 secs = std::max<qint64>(1, (left + 999) / 1000);
            text = QStringLiteral("M%1 again in %2 s").arg(s->n).arg(secs);
            style += QStringLiteral("QPushButton { background: #42140f; border: 1px solid #c0392b; color: #ffd0cc; }");
        } else {
            const qint64 position = s->lengthMs > 0 ? std::min(s->lengthMs, s->positionMs + elapsed)
                                                    : s->positionMs + elapsed;
            text = QStringLiteral("M%1 %2").arg(s->n).arg(VoiceKeyerSnapshot::clockText(position));
            m_fill = s->lengthMs > 0 ? std::clamp(static_cast<double>(position) / s->lengthMs, 0.0, 1.0) : 0.0;
            const double edge = std::clamp(m_fill, 0.0, 0.999);
            style += QStringLiteral(
                         "QPushButton { color: #ffffff; border: 1px solid #ff453a;"
                         " background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #a8322a, stop:%1 #a8322a,"
                         " stop:%2 #5e1611, stop:1 #5e1611); }")
                         .arg(edge, 0, 'f', 3)
                         .arg(edge + 0.001, 0, 'f', 3);
        }
        voice->setAccessibleName(QStringLiteral("Stop the voice keyer"));
        voice->setToolTip(QStringLiteral("Click to stop the message (or press Escape)"));
    } else {
        if (boxOpen()) {
            style += QStringLiteral("QPushButton { border-color: #4a76b0; }");
        }
        voice->setAccessibleName(QStringLiteral("Voice keyer"));
        voice->setToolTip(usable ? (m_permitted ? QStringLiteral("Voice keyer: send one of your eight recorded messages")
                                                : m_permittedReason)
                                 : why);
    }
    voice->setAccessibleDescription(usable ? (m_permitted ? QString() : m_permittedReason) : why);
    voice->setProperty("look", !usable ? "unavailable"
                                       : s.has_value() ? (s->waitingMs > 0 ? "waiting" : "sending")
                                                       : (m_permitted ? "idle" : "held"));
    if (voice->text() != text) {
        voice->setText(text);
        // The message and its time need the room the sound bars take.
        voice->setIcon(usable && s.has_value() ? QIcon() : m_icon);
    }
    if (style != m_lastStyle) {
        m_lastStyle = style;
        voice->setStyleSheet(style);
    }
    // Held elsewhere: VOICE dims (it still opens the box, which says why).
    // An older Core dims all of it too, label and box with the icon.
    const bool dim = !usable || (!m_permitted && !s.has_value());
    if (dim && voice->graphicsEffect() == nullptr) {
        auto* effect = new QGraphicsOpacityEffect(voice);
        effect->setOpacity(kDimmedOpacity);
        voice->setGraphicsEffect(effect);
    } else if (!dim && voice->graphicsEffect() != nullptr) {
        voice->setGraphicsEffect(nullptr);
    }
    if (!usable && boxOpen()) {
        closeBox();
    }
    refreshBox();
}

void VoiceButton::refreshBox()
{
    if (m_box == nullptr) {
        return;
    }
    const bool usable = unavailableReason().isEmpty();
    const std::optional<VoiceKeyerSnapshot::Sending>& s = m_snapshot.sending;
    for (int i = 0; i < static_cast<int>(m_tiles.size()); ++i) {
        const int n = i + 1;
        const VoiceKeyerSnapshot::Slot& slot = m_snapshot.messages[static_cast<size_t>(i)];
        QPushButton* tile = m_tiles[static_cast<size_t>(i)];
        QLabel* name = m_tileNames[static_cast<size_t>(i)];
        QLabel* length = m_tileLengths[static_cast<size_t>(i)];
        QFrame* progress = m_tileProgress[static_cast<size_t>(i)];
        const bool sending = s.has_value() && s->n == n;
        name->setText(slot.empty ? QStringLiteral("Empty") : m_snapshot.label(n));
        length->setText(slot.empty ? QString() : VoiceKeyerSnapshot::clockText(slot.lengthMs + 999));
        const char* look = sending ? "sending" : slot.empty ? "blank" : "";
        setLook(tile, look);
        setLook(name, slot.empty ? "blank" : "");
        setLook(length, sending ? "sending" : "");
        if (QLabel* number = tile->findChild<QLabel*>(QStringLiteral("VoiceTileNumber"))) {
            setLook(number, sending ? "sending" : "");
        }
        const bool sendable = usable && m_permitted;
        // An empty message opens the panel to record it, so it never waits
        // on transmit; a message that keys follows the transmit gate.
        tile->setEnabled(usable && (slot.empty || sendable));
        if (!usable) {
            tile->setToolTip(unavailableReason());
        } else if (slot.empty) {
            tile->setToolTip(QStringLiteral("M%1 is empty: open the voice keyer panel to record or import it").arg(n));
        } else if (!sendable) {
            tile->setToolTip(m_permittedReason);
        } else {
            tile->setToolTip(QStringLiteral("Send %1 (%2)").arg(m_snapshot.label(n),
                                                                 VoiceKeyerSnapshot::clockText(slot.lengthMs + 999)));
        }
        tile->setAccessibleName(slot.empty ? QStringLiteral("M%1, empty").arg(n)
                                           : QStringLiteral("M%1, %2").arg(n).arg(m_snapshot.label(n)));
        tile->setAccessibleDescription(tile->isEnabled() ? QString() : tile->toolTip());
        // Held elsewhere, a message that would key dims, as the mockup's
        // .t.off; Take transmit below stays bright.
        const bool dimTile = usable && !m_permitted && !slot.empty;
        if (dimTile && tile->graphicsEffect() == nullptr) {
            auto* effect = new QGraphicsOpacityEffect(tile);
            effect->setOpacity(kHeldTileOpacity);
            tile->setGraphicsEffect(effect);
        } else if (!dimTile && tile->graphicsEffect() != nullptr) {
            tile->setGraphicsEffect(nullptr);
        }
        if (sending && s->waitingMs <= 0) {
            progress->setGeometry(0, tile->height() - kProgressHeight,
                                  static_cast<int>(std::lround(tile->width() * m_fill)), kProgressHeight);
            progress->show();
            progress->raise();
        } else {
            progress->hide();
        }
    }
    {
        const QSignalBlocker block(m_repeat);
        m_repeat->setChecked(m_snapshot.repeat);
    }
    m_repeat->setText(m_snapshot.repeat ? QStringLiteral("On") : QStringLiteral("Off"));
    m_repeat->setEnabled(usable);
    m_repeatSub->setText(m_snapshot.repeat
                             ? QStringLiteral("Sends again every %1 s until you stop it").arg(m_snapshot.repeatDelaySec)
                             : QStringLiteral("Off"));
    const bool held = usable && !m_permitted;
    m_holderNote->setText(m_permittedReason);
    m_holderFrame->setVisible(held);
    m_take->setVisible(held && m_takeOffered);
    m_note->setText(m_lastRefusal);
    m_note->setVisible(!m_lastRefusal.isEmpty());
    if (boxOpen()) {
        m_box->adjustSize();
        positionBox();
    }
}

void VoiceButton::openBox()
{
    QPushButton* voice = button();
    if (m_box == nullptr || voice == nullptr || !unavailableReason().isEmpty()) {
        return;
    }
    m_lastRefusal.clear();
    refreshBox();
    m_box->adjustSize();
    positionBox();
    m_box->show();
    refresh();
}

void VoiceButton::closeBox()
{
    if (m_box != nullptr && m_box->isVisible()) {
        m_box->hide();
    }
    refresh();
}

void VoiceButton::positionBox()
{
    QPushButton* voice = button();
    if (voice == nullptr || m_section == nullptr) {
        return;
    }
    // The row: from TUNE's left edge to VOICE's right edge.
    QWidget* left = m_section->tuneButton() != nullptr ? m_section->tuneButton() : voice;
    const QPoint rowTopLeft = left->mapToGlobal(QPoint(0, 0));
    const QPoint rowRight = voice->mapToGlobal(QPoint(voice->width(), 0));
    const int width = std::max(rowRight.x() - rowTopLeft.x(), 200);
    m_box->setFixedWidth(width);
    m_box->adjustSize();
    int y = rowTopLeft.y() - m_box->height() - kBoxGap;
    if (const QScreen* screen = voice->screen()) {
        y = std::max(y, screen->availableGeometry().top());
    }
    m_box->move(rowTopLeft.x(), y);
}

void VoiceButton::onVoiceClicked()
{
    if (m_snapshot.sending.has_value()) {
        stopSending();
        return;
    }
    if (m_box != nullptr && m_box->property("closedByPressOnVoice").toBool()) {
        // The press that closed the box was on VOICE itself: it only closes.
        m_box->setProperty("closedByPressOnVoice", false);
        return;
    }
    if (boxOpen()) {
        closeBox();
    } else {
        openBox();
    }
}

void VoiceButton::onTileClicked(int n)
{
    if (!m_access || n < 1 || n > static_cast<int>(m_snapshot.messages.size())) {
        return;
    }
    if (m_snapshot.messages[static_cast<size_t>(n - 1)].empty) {
        closeBox();
        emit panelRequested(n);
        return;
    }
    if (!m_permitted || !unavailableReason().isEmpty()) {
        return;
    }
    m_awaitingAnswer = true;
    m_lastRefusal.clear();
    closeBox();
    m_access->sendVoice(n);
}

void VoiceButton::stopSending()
{
    if (!m_access) {
        return;
    }
    m_awaitingAnswer = true;
    m_access->stopVoice();
}

bool VoiceButton::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Escape && m_snapshot.sending.has_value()
            && (watched == button() || watched == m_box)) {
            stopSending();
            return true;
        }
    }
    if (watched == m_box && event->type() == QEvent::Hide) {
        // A popup closes on any press outside it; a press on VOICE must
        // not open it again at once.
        QPushButton* voice = button();
        const bool onVoice = voice != nullptr && QApplication::mouseButtons() != Qt::NoButton
            && voice->rect().contains(voice->mapFromGlobal(QCursor::pos()));
        m_box->setProperty("closedByPressOnVoice", onVoice);
        if (onVoice) {
            QTimer::singleShot(kPressReplayMs, m_box, [box = m_box]() {
                box->setProperty("closedByPressOnVoice", false);
            });
        }
        QTimer::singleShot(0, this, &VoiceButton::refresh);
    }
    if (watched == button() && event->type() == QEvent::Resize) {
        refresh();
    }
    return QObject::eventFilter(watched, event);
}

}  // namespace NereusSDR
