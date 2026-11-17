// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/recordings/FlagRecordPlay.cpp  (NereusSDR)
// =================================================================
//
// See FlagRecordPlay.h. Colours and sizes follow the approved mockups
// (docs/architecture/2026-11-15-recording-voice-keyer-dsr-design/
// desktop-flag/ and recording-together/), drawn there at twice the app's
// points.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 6.
//   2026-11-15 - J.J. Boyd (KG4VCF), recording plan Task 7: its own list
//                 reader, asking for the slice's last eight contact
//                 recordings. AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-11-15 - J.J. Boyd (KG4VCF), recording plan Task 10: the ON AIR
//                 look, the playing label and Play's ring in red while the
//                 recording goes out. AI-assisted implementation via
//                 Anthropic Claude Code.
// =================================================================

#include "gui/recordings/FlagRecordPlay.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QMenu>
#include <QMetaMethod>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>

#include <algorithm>
#include <optional>

#include "core/recordings/RecordingPlayer.h"
#include "gui/ScopedChildWidget.h"
#include "gui/recordings/RevealFile.h"
#include "gui/widgets/VfoWidget.h"
#include "models/recordings/RecordingsAccess.h"

namespace NereusSDR {

namespace {

// The side buttons' size (VfoWidget::buildFloatingButtons) and the icon
// drawn inside them: the mockup's 20-unit glyphs at half scale.
constexpr int kIconPx = 10;
constexpr qreal kGlyphScale = 0.5;
// The labels beside the buttons: 17 points high in the mockup, here 18 so
// the text centres on the button's 20; 3 points from the button.
constexpr int kLabelHeight = 18;
constexpr int kLabelGap = 3;
constexpr int kLabelTop = 1;
// The Kept notice: 10 points under the flag, 315 points wide, centred
// under the flag and its side buttons.
constexpr int kKeptGap = 10;
constexpr int kKeptMinWidth = 315;

// Mockup colours.
constexpr auto kIdleInk = "#c8c8d0";
constexpr auto kRecordRed = "#ff453a";
constexpr auto kArmedRing = "#ff6b62";
constexpr auto kArmedRim = "#7a2c2e";
constexpr auto kPlayBlue = "#3a9cff";
// On the air (recording-together mockup): the playing label's ink, rim and
// fill, and the ON AIR badge.
constexpr auto kOnAirInk = "#ffd0cc";
constexpr auto kOnAirFill = "rgba(90,20,16,240)";
constexpr auto kOnAirName = "#ffffff";

// A disabled button's glyph: the resting ink at the mockup's 0.38 opacity
// (the sheet draws a disabled button like an enabled one).
constexpr qreal kDisabledInkAlpha = 0.38;

enum class Glyph { Dot, Ring, Triangle, Square };

QColor disabledInk()
{
    QColor ink = QColor::fromString(QLatin1String(kIdleInk));
    ink.setAlphaF(kDisabledInkAlpha);
    return ink;
}

QIcon glyphIcon(Glyph glyph, const QColor& colour)
{
    const qreal dpr = std::max<qreal>(2.0, qApp != nullptr ? qApp->devicePixelRatio() : 1.0);
    QPixmap pixmap(QSize(kIconPx, kIconPx) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(kGlyphScale, kGlyphScale);
    switch (glyph) {
    case Glyph::Dot:
        p.setPen(Qt::NoPen);
        p.setBrush(colour);
        p.drawEllipse(QPointF(10.0, 10.0), 6.5, 6.5);
        break;
    case Glyph::Ring:
        p.setPen(QPen(colour, 2.4));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(10.0, 10.0), 5.6, 5.6);
        break;
    case Glyph::Triangle: {
        QPainterPath path;
        path.moveTo(6.0, 3.5);
        path.lineTo(16.0, 10.0);
        path.lineTo(6.0, 16.5);
        path.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(colour);
        p.drawPath(path);
        break;
    }
    case Glyph::Square:
        p.setPen(Qt::NoPen);
        p.setBrush(colour);
        p.drawRoundedRect(QRectF(4.5, 4.5, 11.0, 11.0), 1.5, 1.5);
        break;
    }
    p.end();
    return QIcon(pixmap);
}

// The side buttons' resting sheet with another rim.
QString rimmedSheet(const char* rim)
{
    return QStringLiteral(
               "QPushButton { background: rgba(20,30,50,230); border: 1px solid %1;"
               "  border-radius: 10px; color: #c8d8e8; font-size: 11px; padding: 0; }"
               "QPushButton:hover { background: rgba(40,55,80,240); }")
        .arg(QLatin1String(rim));
}

QString labelSheet(const char* ink, const char* rim, bool mono)
{
    return QStringLiteral(
               "QLabel { background: rgba(12,12,14,240); border: 1px solid %1; border-radius: 9px;"
               "  color: %2; padding: 0 7px; font-size: 11px; font-weight: 600; %3 }")
        .arg(QLatin1String(rim), QLatin1String(ink),
             mono ? QStringLiteral("font-family: 'Menlo', 'Consolas', monospace;") : QString());
}

QString onAirLabelSheet()
{
    return QStringLiteral(
               "QLabel { background: %1; border: 1px solid %2; border-radius: 9px;"
               "  color: %3; padding: 0 7px; font-size: 11px; font-weight: 600;"
               "  font-family: 'Menlo', 'Consolas', monospace; }")
        .arg(QLatin1String(kOnAirFill), QLatin1String(kRecordRed), QLatin1String(kOnAirInk));
}

const char* kKeptSheet =
    "QFrame#FlagRecordKept { background: #1b1b1f; border: 1px solid #3d3d41; border-radius: 6px; }"
    "QLabel { background: transparent; border: none; color: #dcdce1; font-size: 11px; }"
    "QPushButton { background: #222226; border: 1px solid #2c2c31; border-radius: 4px;"
    "  color: #f0f0f2; font-size: 11px; font-weight: 600; padding: 0 7px; min-height: 21px; }"
    "QPushButton:hover { background: #2c2c31; }"
    "QPushButton:disabled { color: #5e5e66; }"
    "QPushButton#FlagRecordKeptPlay { background: #1e3d5f; border-color: #4a7ba8; }"
    "QPushButton#FlagRecordKeptPlay:hover { background: #2a4f78; }";

QString numberWord(int n)
{
    static const char* const kWords[] = {"zero", "one", "two",   "three", "four", "five",
                                         "six",  "seven", "eight", "nine",  "ten"};
    if (n >= 0 && n <= 10) {
        return QString::fromLatin1(kWords[n]);
    }
    return QString::number(n);
}

// "Stopped on the iPhone", "Stopped on JJ's iPad", "Stopped on another
// device": a one-word device name reads with "the".
QString stoppedOn(const QString& device)
{
    const QString name = device.trimmed();
    if (name.isEmpty() || name == QStringLiteral("Another device")) {
        return QStringLiteral("Stopped on another device");
    }
    if (!name.contains(QLatin1Char(' ')) && !name.contains(QLatin1Char('\''))
        && !name.contains(QChar(0x2019))) {
        return QStringLiteral("Stopped on the %1").arg(name);
    }
    return QStringLiteral("Stopped on %1").arg(name);
}

QString entryLabel(const RecordingEntry& e)
{
    QString text = recordingFrequencyText(e.frequencyHz);
    if (!e.mode.isEmpty()) {
        text += QLatin1Char(' ') + e.mode;
    }
    if (!e.call.isEmpty()) {
        text += QStringLiteral("  ") + e.call;
    }
    return text;
}

}  // namespace

FlagRecordPlay::FlagRecordPlay(VfoWidget* flag, RecordingsAccess* access, int sliceId, QObject* parent)
    : QObject(parent)
    , m_flag(flag)
    , m_sliceId(sliceId)
    , m_reveal([](const QString& path) { return RevealFile::reveal(path); })
    , m_now([] { return QDateTime::currentDateTimeUtc(); })
{
    setObjectName(QStringLiteral("FlagRecordPlay"));
    m_keptTimer = new QTimer(this);
    m_keptTimer->setSingleShot(true);
    m_keptTimer->setInterval(kKeptNoticeMs);
    connect(m_keptTimer, &QTimer::timeout, this, &FlagRecordPlay::hideKeptNotice);
    m_blinkTimer = new QTimer(this);
    m_blinkTimer->setInterval(kBlinkMs);
    connect(m_blinkTimer, &QTimer::timeout, this, [this]() {
        m_blinkOn = !m_blinkOn;
        applyRecordLook();
    });
    m_positionTimer = new QTimer(this);
    m_positionTimer->setInterval(kPositionRefreshMs);
    connect(m_positionTimer, &QTimer::timeout, this, &FlagRecordPlay::applyPlayLook);

    if (flag != nullptr) {
        connect(flag, &VfoWidget::floatingButtonsBuilt, this, &FlagRecordPlay::attachButtons);
        connect(flag, &VfoWidget::floatingButtonsRaised, this, &FlagRecordPlay::raiseOwn);
        connect(flag, &VfoWidget::recordToggled, this, &FlagRecordPlay::onRecordClicked);
        connect(flag, &VfoWidget::playToggled, this, &FlagRecordPlay::onPlayClicked);
        flag->installEventFilter(this);
    }
    setAccess(access);
    attachButtons();
}

FlagRecordPlay::~FlagRecordPlay()
{
    // The labels and the notice are the pan's children; they go with this.
    delete m_recordLabel.data();
    delete m_playLabel.data();
    delete m_kept.data();
}

RecordingsAccess* FlagRecordPlay::access() const
{
    return m_access.data();
}

void FlagRecordPlay::setAccess(RecordingsAccess* access)
{
    for (const QMetaObject::Connection& c : m_accessConnections) {
        disconnect(c);
    }
    m_accessConnections.clear();
    m_access = access;
    m_entries.clear();
    m_listRevision = -1;
    m_listReader = access != nullptr ? access->newListReader() : 0;
    if (access != nullptr) {
        m_accessConnections = {
            connect(access, &RecordingsAccess::documentChanged, this, &FlagRecordPlay::onDocumentChanged),
            connect(access, &RecordingsAccess::stateChanged, this, &FlagRecordPlay::onDocumentChanged),
            connect(access, &RecordingsAccess::listChanged, this, &FlagRecordPlay::onListChanged),
            connect(access, &RecordingsAccess::playbackChanged, this, &FlagRecordPlay::refresh),
            connect(access, &RecordingsAccess::kept, this, &FlagRecordPlay::onKept),
            // A refused start or stop leaves the button as the Core has it.
            connect(access, &RecordingsAccess::refused, this, &FlagRecordPlay::refresh),
        };
    }
    hideKeptNotice();
    onDocumentChanged();
}

void FlagRecordPlay::attachButtons()
{
    if (!m_flag || m_recordButton) {
        return;
    }
    m_recordButton = m_flag->recordButton();
    m_playButton = m_flag->playButton();
    QWidget* pan = m_flag->parentWidget();
    if (!m_recordButton || !m_playButton || pan == nullptr) {
        return;
    }
    m_recordButton->setIconSize(QSize(kIconPx, kIconPx));
    m_playButton->setIconSize(QSize(kIconPx, kIconPx));
    m_recordButton->installEventFilter(this);
    m_playButton->installEventFilter(this);

    m_recordLabel = new QLabel(pan);
    m_recordLabel->setObjectName(QStringLiteral("FlagRecordLabel"));
    m_recordLabel->setFixedHeight(kLabelHeight);
    m_recordLabel->setTextFormat(Qt::PlainText);
    m_recordLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_recordLabel->hide();

    m_playLabel = new QLabel(pan);
    m_playLabel->setObjectName(QStringLiteral("FlagPlayLabel"));
    m_playLabel->setFixedHeight(kLabelHeight);
    m_playLabel->setTextFormat(Qt::RichText);
    m_playLabel->setStyleSheet(labelSheet("#8cc4ff", "#24507e", true));
    m_playLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_playLabel->hide();

    m_kept = new QFrame(pan);
    m_kept->setObjectName(QStringLiteral("FlagRecordKept"));
    m_kept->setStyleSheet(QString::fromLatin1(kKeptSheet));
    m_kept->setMinimumWidth(kKeptMinWidth);
    auto* row = new QHBoxLayout(m_kept);
    row->setContentsMargins(9, 4, 5, 4);
    row->setSpacing(7);
    m_keptText = new QLabel(m_kept);
    m_keptText->setTextFormat(Qt::RichText);
    row->addWidget(m_keptText, 1);
    m_keptAll = new QPushButton(QStringLiteral("All recordings\u2026"), m_kept);
    m_keptAll->setObjectName(QStringLiteral("FlagRecordKeptAll"));
    m_keptAll->setToolTip(QStringLiteral("Open the list of every recording on the Core"));
    row->addWidget(m_keptAll);
    m_keptPlay = new QPushButton(QStringLiteral("Play"), m_kept);
    m_keptPlay->setObjectName(QStringLiteral("FlagRecordKeptPlay"));
    m_keptPlay->setToolTip(QStringLiteral("Play this recording on this computer's speaker"));
    row->addWidget(m_keptPlay);
    connect(m_keptAll, &QPushButton::clicked, this, [this]() {
        const QString id = m_keptId;
        hideKeptNotice();
        emit allRecordingsRequested(id);
    });
    connect(m_keptPlay, &QPushButton::clicked, this, [this]() {
        const QString id = m_keptId;
        hideKeptNotice();
        if (m_access && !id.isEmpty()) {
            m_access->play(id);
        }
    });
    m_kept->hide();

    // The buttons go with the flag (destroyFloatingButtons): the labels too.
    connect(m_recordButton, &QObject::destroyed, this, [this]() {
        delete m_recordLabel.data();
        delete m_playLabel.data();
        delete m_kept.data();
    });
    refresh();
}

bool FlagRecordPlay::eventFilter(QObject* watched, QEvent* event)
{
    const QEvent::Type type = event->type();
    if (watched == m_playButton && type == QEvent::ContextMenu) {
        showPlayMenu(static_cast<QContextMenuEvent*>(event)->globalPos());
        return true;
    }
    if ((watched == m_recordButton || watched == m_playButton)
        && (type == QEvent::Move || type == QEvent::Show || type == QEvent::Hide)) {
        placeLabels();
    }
    if (watched == m_flag
        && (type == QEvent::Move || type == QEvent::Resize || type == QEvent::Show || type == QEvent::Hide)) {
        placeKeptNotice();
    }
    return QObject::eventFilter(watched, event);
}

QString FlagRecordPlay::unavailableReason() const
{
    if (!m_access) {
        return RecordingsAccess::notConnectedReason();
    }
    return m_access->unavailableReason(1);
}

QString FlagRecordPlay::sliceName() const
{
    return QStringLiteral("Slice %1").arg(QChar(QLatin1Char('A').unicode() + m_sliceId));
}

QJsonObject FlagRecordPlay::sliceDocument() const
{
    if (!m_access || !unavailableReason().isEmpty()) {
        return {};
    }
    const QJsonArray slices = m_access->document().value(QStringLiteral("slices")).toArray();
    for (const QJsonValue& v : slices) {
        const QJsonObject slice = v.toObject();
        if (slice.value(QStringLiteral("slice")).toInt(-1) == m_sliceId) {
            return slice;
        }
    }
    return {};
}

bool FlagRecordPlay::hasListener(const char* signal) const
{
    const QMetaMethod method = metaObject()->method(metaObject()->indexOfSignal(signal));
    return method.isValid() && isSignalConnected(method);
}

bool FlagRecordPlay::isPlaying() const
{
    if (!m_access) {
        return false;
    }
    const std::optional<PlaybackState> playback = m_access->playback();
    return playback.has_value() && playback->playing;
}

bool FlagRecordPlay::isOnAir() const
{
    if (!m_access) {
        return false;
    }
    const std::optional<PlaybackState> playback = m_access->playback();
    return playback.has_value() && playback->playing && playback->onAir;
}

QList<RecordingEntry> FlagRecordPlay::recentRecordings() const
{
    QList<RecordingEntry> out;
    for (const RecordingEntry& e : m_entries) {
        if (e.kind == RecordingKind::Contact && e.sliceId == m_sliceId) {
            out.append(e);
            if (out.size() == kMenuRecordings) {
                break;
            }
        }
    }
    return out;
}

QString FlagRecordPlay::latestRecordingId() const
{
    const QList<RecordingEntry> recent = recentRecordings();
    return recent.isEmpty() ? QString() : recent.first().id;
}

QString FlagRecordPlay::lengthText(qint64 ms)
{
    const qint64 seconds = std::max<qint64>(0, ms) / 1000;
    const qint64 h = seconds / 3600;
    const qint64 m = (seconds / 60) % 60;
    const qint64 s = seconds % 60;
    if (h > 0) {
        return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
}

QString FlagRecordPlay::recordLabelText() const
{
    switch (m_recordLook) {
    case RecordLook::Recording:
        return QStringLiteral("\u25CF ") + lengthText(m_elapsedMs)
            + (m_myVoice ? QStringLiteral("  + your voice") : QString());
    case RecordLook::Armed:
        return QStringLiteral("Waits for you to transmit");
    case RecordLook::Idle:
        break;
    }
    return {};
}

bool FlagRecordPlay::playLabelParts(QString* name, QString* times) const
{
    if (!m_access) {
        return false;
    }
    const std::optional<PlaybackState> playback = m_access->playback();
    if (!playback.has_value() || !playback->playing) {
        return false;
    }
    *name = QStringLiteral("Recording");
    for (const RecordingEntry& e : m_entries) {
        if (e.id == playback->recordingId) {
            *name = entryLabel(e);
            break;
        }
    }
    *times = lengthText(playback->positionMs) + QStringLiteral(" / ") + lengthText(playback->lengthMs);
    return true;
}

QString FlagRecordPlay::playLabelText() const
{
    QString name;
    QString times;
    if (!playLabelParts(&name, &times)) {
        return {};
    }
    return (isOnAir() ? QStringLiteral("ON AIR  ") : QString()) + name + QStringLiteral("  ") + times;
}

QString FlagRecordPlay::keptReasonText(const QJsonObject& kept, int quietMinutes, bool coreHere)
{
    const QString reason = kept.value(QStringLiteral("reason")).toString();
    if (reason == QStringLiteral("device")) {
        return stoppedOn(kept.value(QStringLiteral("device")).toString());
    }
    if (reason == QStringLiteral("quiet")) {
        const int minutes = std::max(1, quietMinutes);
        return minutes == 1 ? QStringLiteral("Stopped by itself after one quiet minute")
                            : QStringLiteral("Stopped by itself after %1 quiet minutes").arg(numberWord(minutes));
    }
    if (reason == QStringLiteral("diskFull")) {
        return QStringLiteral("Stopped: the disk is full");
    }
    if (reason == QStringLiteral("wav")) {
        return QStringLiteral("Kept as WAV: the MP3 encoder failed");
    }
    return coreHere ? QStringLiteral("Stopped on this computer") : QStringLiteral("Stopped on the Core");
}

void FlagRecordPlay::onDocumentChanged()
{
    requestListIfStale();
    refresh();
}

void FlagRecordPlay::requestListIfStale()
{
    if (!m_access || !unavailableReason().isEmpty()) {
        m_entries.clear();
        m_listRevision = -1;
        return;
    }
    const QJsonObject index = m_access->document().value(QStringLiteral("index")).toObject();
    if (index.isEmpty()) {
        return;
    }
    const qint64 revision = index.value(QStringLiteral("revision")).toInteger();
    if (revision == m_listRevision) {
        return;
    }
    m_listRevision = revision;
    RecordingsListQuery query;
    query.count = kMenuRecordings;
    query.sliceId = m_sliceId;
    query.kind = RecordingKind::Contact;
    m_access->requestList(m_listReader, query);
}

void FlagRecordPlay::onListChanged(int reader)
{
    // Other readers' answers are theirs.
    if (reader != m_listReader || !m_access) {
        return;
    }
    m_entries = m_access->listed(reader).entries;
    refresh();
}

void FlagRecordPlay::onKept(const QJsonObject& event)
{
    if (event.value(QStringLiteral("slice")).toInt(-1) != m_sliceId || !m_kept || !m_keptText) {
        return;
    }
    const QJsonObject settings = m_access ? m_access->document().value(QStringLiteral("settings")).toObject()
                                          : QJsonObject();
    const int quiet = settings.value(QStringLiteral("quietMinutes")).toInt(2);
    const bool here = m_access && m_access->coreIsOnThisComputer();
    m_keptId = event.value(QStringLiteral("id")).toString();
    const QString name = event.value(QStringLiteral("name")).toString();
    m_keptText->setText(QStringLiteral("Kept: %1, %2<br><span style=\"color:#8e8e96; font-size:11px;\">%3</span>")
                            .arg(name.toHtmlEscaped(),
                                 lengthText(event.value(QStringLiteral("lengthMs")).toInteger()),
                                 keptReasonText(event, quiet, here).toHtmlEscaped()));
    m_keptAll->setEnabled(hasListener("allRecordingsRequested(QString)"));
    m_keptPlay->setEnabled(!m_keptId.isEmpty());
    m_kept->adjustSize();
    m_kept->show();
    placeKeptNotice();
    m_keptTimer->start();
}

void FlagRecordPlay::hideKeptNotice()
{
    m_keptTimer->stop();
    if (m_kept) {
        m_kept->hide();
    }
}

void FlagRecordPlay::onRecordClicked()
{
    if (m_access && unavailableReason().isEmpty()) {
        if (m_recordLook == RecordLook::Recording) {
            m_access->stop(m_sliceId);
        } else {
            m_access->start(m_sliceId);
        }
    }
    refresh();
}

void FlagRecordPlay::onPlayClicked()
{
    if (m_access && unavailableReason().isEmpty()) {
        if (isPlaying()) {
            m_access->stopPlay();
        } else if (const QString id = latestRecordingId(); !id.isEmpty()) {
            m_access->play(id);
        }
    }
    refresh();
}

void FlagRecordPlay::refresh()
{
    const QJsonObject slice = sliceDocument();
    const QString state = slice.value(QStringLiteral("state")).toString();
    m_recordLook = state == QStringLiteral("recording") ? RecordLook::Recording
        : state == QStringLiteral("armed")              ? RecordLook::Armed
                                                        : RecordLook::Idle;
    m_elapsedMs = slice.value(QStringLiteral("elapsedMs")).toInteger();
    m_myVoice = slice.value(QStringLiteral("myVoice")).toBool();
    if (m_recordLook == RecordLook::Recording || !unavailableReason().isEmpty()) {
        hideKeptNotice();
    }
    if (m_recordLook == RecordLook::Recording) {
        if (!m_blinkTimer->isActive()) {
            m_blinkOn = true;
            m_blinkTimer->start();
        }
    } else {
        m_blinkTimer->stop();
        m_blinkOn = true;
    }
    if (isPlaying()) {
        if (!m_positionTimer->isActive()) {
            m_positionTimer->start();
        }
    } else {
        m_positionTimer->stop();
    }
    applyRecordLook();
    applyPlayLook();
}

void FlagRecordPlay::applyRecordLook()
{
    if (!m_recordButton) {
        return;
    }
    QPushButton* b = m_recordButton.data();
    const QString reason = unavailableReason();
    const bool usable = reason.isEmpty();
    b->setEnabled(usable);
    {
        const QSignalBlocker block(b);
        b->setChecked(usable && m_recordLook == RecordLook::Recording);
    }
    QString tip;
    QString sheet;
    QIcon icon;
    if (!usable) {
        tip = reason;
        sheet = VfoWidget::floatingButtonStyleSheet();
        icon = glyphIcon(Glyph::Dot, disabledInk());
        b->setAccessibleName(QStringLiteral("Record"));
    } else if (m_recordLook == RecordLook::Recording) {
        tip = QStringLiteral("Stop recording %1").arg(sliceName());
        sheet = rimmedSheet(kRecordRed);
        QColor red = QColor::fromString(QLatin1String(kRecordRed));
        if (!m_blinkOn) {
            red.setAlphaF(0.35);
        }
        icon = glyphIcon(Glyph::Dot, red);
        b->setAccessibleName(QStringLiteral("Stop recording"));
    } else if (m_recordLook == RecordLook::Armed) {
        tip = QStringLiteral("Waits for you to transmit. Click to record %1 now.").arg(sliceName());
        sheet = rimmedSheet(kArmedRim);
        icon = glyphIcon(Glyph::Ring, QColor(QLatin1String(kArmedRing)));
        b->setAccessibleName(QStringLiteral("Record"));
    } else {
        const QJsonObject settings =
            m_access->document().value(QStringLiteral("settings")).toObject();
        const bool myVoice = settings.value(QStringLiteral("keepMyVoice")).toBool(true);
        tip = myVoice ? QStringLiteral("Record %1: what you hear, and your own voice when you transmit")
                            .arg(sliceName())
                      : QStringLiteral("Record %1: what you hear").arg(sliceName());
        sheet = VfoWidget::floatingButtonStyleSheet();
        icon = glyphIcon(Glyph::Dot, QColor(QLatin1String(kIdleInk)));
        b->setAccessibleName(QStringLiteral("Record"));
    }
    b->setToolTip(tip);
    b->setAccessibleDescription(usable ? QString() : reason);
    if (b->property("_flagRecordSheet").toString() != sheet) {
        b->setProperty("_flagRecordSheet", sheet);
        b->setStyleSheet(sheet);
    }
    b->setIcon(icon);
    placeLabels();
}

void FlagRecordPlay::applyPlayLook()
{
    if (!m_playButton) {
        return;
    }
    QPushButton* b = m_playButton.data();
    const QString reason = unavailableReason();
    const bool playing = reason.isEmpty() && isPlaying();
    const QString latest = latestRecordingId();
    QString why = reason;
    if (why.isEmpty() && !playing && latest.isEmpty()) {
        why = QStringLiteral("Nothing recorded on %1.").arg(sliceName());
    }
    b->setEnabled(why.isEmpty());
    {
        const QSignalBlocker block(b);
        b->setChecked(playing);
    }
    QString sheet = VfoWidget::floatingButtonStyleSheet();
    if (!why.isEmpty()) {
        b->setToolTip(why);
        b->setIcon(glyphIcon(Glyph::Triangle, disabledInk()));
        b->setAccessibleName(QStringLiteral("Play"));
    } else if (playing && isOnAir()) {
        // On the air: the ring and the glyph in red.
        b->setToolTip(QStringLiteral("Stop playing"));
        b->setIcon(glyphIcon(Glyph::Square, QColor(QLatin1String(kRecordRed))));
        b->setAccessibleName(QStringLiteral("Stop playing"));
        sheet = rimmedSheet(kRecordRed);
    } else if (playing) {
        b->setToolTip(QStringLiteral("Stop playing"));
        b->setIcon(glyphIcon(Glyph::Square, QColor(QLatin1String(kPlayBlue))));
        b->setAccessibleName(QStringLiteral("Stop playing"));
        sheet = rimmedSheet(kPlayBlue);
    } else {
        b->setToolTip(QStringLiteral("Play the latest recording of %1 on this computer's speaker. "
                                     "Right-click for others.")
                          .arg(sliceName()));
        b->setIcon(glyphIcon(Glyph::Triangle, QColor(QLatin1String(kIdleInk))));
        b->setAccessibleName(QStringLiteral("Play"));
    }
    b->setAccessibleDescription(why);
    if (b->property("_flagRecordSheet").toString() != sheet) {
        b->setProperty("_flagRecordSheet", sheet);
        b->setStyleSheet(sheet);
    }
    placeLabels();
}

void FlagRecordPlay::placeLabels()
{
    const auto place = [this](QLabel* label, QPushButton* button, const QString& text) {
        if (label == nullptr) {
            return;
        }
        if (button == nullptr || !button->isVisible() || text.isEmpty() || !m_flag) {
            label->hide();
            return;
        }
        label->setText(text);
        label->adjustSize();
        const bool columnLeftOfFlag = button->x() < m_flag->x();
        const int x = columnLeftOfFlag ? button->x() - kLabelGap - label->width()
                                       : button->x() + button->width() + kLabelGap;
        label->move(x, button->y() + kLabelTop);
        label->show();
        label->raise();
    };
    if (m_recordLabel) {
        const bool armed = m_recordLook == RecordLook::Armed;
        const QString sheet = armed ? labelSheet("#c9a3a1", "#4a2a28", false)
                                    : labelSheet("#ff8a84", "#7a2a26", true);
        if (m_recordLabel->styleSheet() != sheet) {
            m_recordLabel->setStyleSheet(sheet);
        }
        place(m_recordLabel.data(), m_recordButton.data(),
              unavailableReason().isEmpty() ? recordLabelText() : QString());
    }
    if (m_playLabel) {
        QString html;
        QString name;
        QString times;
        const bool onAir = isOnAir();
        if (playLabelParts(&name, &times)) {
            // The recording's name in the window's own font, the times in
            // the label's monospace. On the air: the ON AIR badge first, the
            // name in white.
            const QString badge = onAir
                ? QStringLiteral("<span style=\"font-family:'%1'; font-weight:800; background-color:%2;"
                                 " color:#ffffff;\">&nbsp;ON AIR&nbsp;</span>&nbsp;&nbsp;")
                      .arg(QApplication::font().family().toHtmlEscaped(), QLatin1String(kRecordRed))
                : QString();
            html = badge
                + QStringLiteral("<span style=\"font-family:'%1'; color:%2;\">%3</span>&nbsp;&nbsp;%4")
                      .arg(QApplication::font().family().toHtmlEscaped(),
                           QLatin1String(onAir ? kOnAirName : "#dcdce1"),
                           name.toHtmlEscaped().replace(QStringLiteral("  "), QStringLiteral("&nbsp;&nbsp;")),
                           times.toHtmlEscaped());
        }
        const QString sheet = onAir ? onAirLabelSheet() : labelSheet("#8cc4ff", "#24507e", true);
        if (m_playLabel->styleSheet() != sheet) {
            m_playLabel->setStyleSheet(sheet);
        }
        place(m_playLabel.data(), m_playButton.data(), html);
    }
}

void FlagRecordPlay::placeKeptNotice()
{
    if (!m_kept || !m_flag) {
        return;
    }
    if (!m_flag->isVisible()) {
        m_kept->hide();
        return;
    }
    if (!m_kept->isVisible()) {
        return;
    }
    QWidget* pan = m_kept->parentWidget();
    QRect unit = m_flag->geometry();
    if (m_recordButton && m_recordButton->isVisible()) {
        unit = unit.united(m_recordButton->geometry());
    }
    const int left = unit.center().x() - m_kept->width() / 2;
    const int maxX = pan != nullptr ? std::max(0, pan->width() - m_kept->width()) : std::max(0, left);
    m_kept->move(std::clamp(left, 0, maxX), m_flag->y() + m_flag->height() + kKeptGap);
    m_kept->raise();
}

void FlagRecordPlay::raiseOwn()
{
    for (QWidget* w : {static_cast<QWidget*>(m_recordLabel.data()), static_cast<QWidget*>(m_playLabel.data()),
                       static_cast<QWidget*>(m_kept.data())}) {
        if (w != nullptr && w->isVisible()) {
            w->raise();
        }
    }
}

void FlagRecordPlay::populatePlayMenu(QMenu* menu)
{
    QPointer<FlagRecordPlay> self(this);
    QAction* header = menu->addAction(QStringLiteral("Play on this computer's speaker"));
    header->setEnabled(false);

    const std::optional<PlaybackState> playback = m_access ? m_access->playback() : std::nullopt;
    const QString nowId = playback.has_value() && playback->playing ? playback->recordingId : QString();
    const QDate today = m_now().date();
    for (const RecordingEntry& e : recentRecordings()) {
        const QDateTime start = e.startUtc.toUTC();
        const qint64 days = start.date().daysTo(today);
        const QString day = days <= 0 ? QStringLiteral("today")
            : days == 1               ? QStringLiteral("yesterday")
                                      : QStringLiteral("%1 days ago").arg(days);
        const QString when = QStringLiteral("%1 %2Z \u00B7 %3")
                                 .arg(day, start.toString(QStringLiteral("HHmm")), lengthText(e.lengthMs));
        QString text = entryLabel(e);
        if (e.id == nowId) {
            text = QStringLiteral("\u25B6 ") + text;
        }
        QAction* a = menu->addAction(text + QLatin1Char('\t') + when);
        a->setToolTip(e.name);
        connect(a, &QAction::triggered, this, [self, id = e.id]() {
            if (self && self->m_access) {
                self->m_access->play(id);
                self->refresh();
            }
        });
    }
    menu->addSeparator();

    QAction* all = menu->addAction(QStringLiteral("All recordings\u2026"));
    all->setEnabled(hasListener("allRecordingsRequested(QString)"));
    connect(all, &QAction::triggered, this, [self]() {
        if (self) {
            emit self->allRecordingsRequested(QString());
        }
    });

    const bool here = m_access && m_access->coreIsOnThisComputer();
    const QString folder = m_access ? m_access->document()
                                          .value(QStringLiteral("index"))
                                          .toObject()
                                          .value(QStringLiteral("folder"))
                                          .toString()
                                    : QString();
    QAction* reveal = nullptr;
    if (here) {
        reveal = menu->addAction(RevealFile::actionText());
        reveal->setEnabled(!folder.isEmpty());
    } else {
        const QString why = QStringLiteral("The Core is on another computer.");
        reveal = menu->addAction(RevealFile::actionText() + QLatin1Char('\t') + why);
        reveal->setToolTip(why);
        reveal->setEnabled(false);
    }
    connect(reveal, &QAction::triggered, this, [self, folder]() {
        if (self && self->m_reveal) {
            self->m_reveal(folder);
        }
    });

    QAction* settings = menu->addAction(QStringLiteral("Recording settings\u2026"));
    settings->setEnabled(hasListener("recordingSettingsRequested()"));
    connect(settings, &QAction::triggered, this, [self]() {
        if (self) {
            emit self->recordingSettingsRequested();
        }
    });
    menu->setToolTipsVisible(true);
}

void FlagRecordPlay::showPlayMenu(const QPoint& globalPos)
{
    if (!m_playButton || !m_playButton->isEnabled()) {
        return;
    }
    ScopedChildWidget<QMenu> menuOwner(m_playButton.data());
    populatePlayMenu(menuOwner.get());
    menuOwner.get()->exec(globalPos);
}

}  // namespace NereusSDR
