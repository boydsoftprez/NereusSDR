// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original. Shaped like the CWX applet's memory
// panel (CwxApplet.cpp); no logic is ported.
// =================================================================
// src/gui/applets/VoiceKeyerApplet.cpp  (NereusSDR)
// =================================================================
//
// See VoiceKeyerApplet.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 12.
// =================================================================

#include "gui/applets/VoiceKeyerApplet.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QLabel>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#include "core/recordings/VoiceKeyer.h"
#include "gui/StyleConstants.h"
#include "gui/recordings/LocalMicClipRecorder.h"
#include "models/recordings/RecordingsAccess.h"

namespace NereusSDR {

namespace {

constexpr qreal kDimmedOpacity = 0.4;
constexpr int kButtonHeight = 21;
constexpr int kProgressHeight = 2;
constexpr int kLevelSteps = 1000;

QString buttonSheet(const char* look)
{
    QString sheet = Style::buttonBaseStyle() + QStringLiteral("QPushButton { padding: 0 6px; font-size: 11px; }");
    const QLatin1String l(look);
    if (l == QLatin1String("red")) {
        sheet += QStringLiteral("QPushButton { background: #7a2c2e; border: 1px solid #c25a5c; color: #ffd9d9; }");
    } else if (l == QLatin1String("redDim")) {
        sheet += QStringLiteral("QPushButton { background: #4a1c1e; border: 1px solid #c25a5c; color: #ffd9d9; }");
    } else if (l == QLatin1String("green")) {
        sheet += QStringLiteral("QPushButton { background: #2f5a40; border: 1px solid #3f7a55; color: #cfe9d6; }");
    } else if (l == QLatin1String("empty")) {
        sheet += QStringLiteral("QPushButton { color: #6c6c72; font-weight: 600; }");
    } else if (l == QLatin1String("held")) {
        sheet += QStringLiteral("QPushButton { color: #6c6c72; }");
    }
    return sheet;
}

void setButtonLook(QPushButton* button, const char* look)
{
    if (button->property("look").toString() == QLatin1String(look)) {
        return;
    }
    button->setProperty("look", QLatin1String(look));
    button->setStyleSheet(buttonSheet(look));
}

QString secondsText(qint64 ms)
{
    return QStringLiteral("%1 s").arg(static_cast<double>(std::max<qint64>(0, ms)) / 1000.0, 0, 'f', 1);
}

}  // namespace

VoiceKeyerApplet::VoiceKeyerApplet(RadioModel* model, RecordingsAccess* access, QWidget* parent)
    : AppletWidget(model, parent)
{
    setObjectName(QStringLiteral("VoiceKeyerApplet"));
    m_openChooser = [this]() {
        return QFileDialog::getOpenFileName(this, QStringLiteral("Load a sound file"),
                                            QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
                                            QStringLiteral("Sound files (*.wav *.mp3)"));
    };
    m_saveChooser = [this](const QString& suggested) {
        return QFileDialog::getSaveFileName(this, QStringLiteral("Save as WAV"), suggested,
                                            QStringLiteral("WAV files (*.wav)"));
    };
    m_nameAsker = [this](int n, const QString& current) -> std::optional<QString> {
        bool ok = false;
        const QString name = QInputDialog::getText(this, QStringLiteral("Rename M%1").arg(n),
                                                   QStringLiteral("Name for M%1 (CQ, Call, 73...)").arg(n),
                                                   QLineEdit::Normal, current, &ok);
        if (!ok) {
            return std::nullopt;
        }
        return name;
    };
    m_now = [] {
        static QElapsedTimer clock;
        if (!clock.isValid()) {
            clock.start();
        }
        return clock.elapsed();
    };
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(kRefreshMs);
    connect(m_refreshTimer, &QTimer::timeout, this, &VoiceKeyerApplet::refresh);
    m_blinkTimer = new QTimer(this);
    m_blinkTimer->setInterval(kBlinkMs);
    connect(m_blinkTimer, &QTimer::timeout, this, [this]() {
        m_blinkOn = !m_blinkOn;
        refresh();
    });
    buildUi();
    setAccess(access);
}

VoiceKeyerApplet::~VoiceKeyerApplet() = default;

void VoiceKeyerApplet::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 5);
    root->setSpacing(4);

    m_note = new QLabel(this);
    m_note->setObjectName(QStringLiteral("VoiceKeyerNote"));
    m_note->setWordWrap(true);
    m_note->setStyleSheet(QStringLiteral("QLabel { color: #c8c8cc; background: #1b1b1f; border: 1px solid #3a3a40;"
                                         " border-radius: 4px; padding: 5px 6px; font-size: 10.5px; }"));
    m_note->hide();
    root->addWidget(m_note);

    m_body = new QWidget(this);
    auto* box = new QVBoxLayout(m_body);
    box->setContentsMargins(0, 0, 0, 0);
    box->setSpacing(4);
    root->addWidget(m_body);

    const auto makeButton = [this](const QString& text, const QString& objectName) {
        auto* b = new QPushButton(text, m_body);
        b->setObjectName(objectName);
        b->setFixedHeight(kButtonHeight);
        b->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        b->setFocusPolicy(Qt::TabFocus);
        setButtonLook(b, "plain");
        return b;
    };

    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);
        m_record = makeButton(QStringLiteral("● Record"), QStringLiteral("VoiceKeyerRecord"));
        m_record->setAccessibleName(QStringLiteral("Record a message"));
        m_record->setToolTip(QStringLiteral("Light this, then click a message to record your voice into it"));
        m_stop = makeButton(QStringLiteral("■ Stop"), QStringLiteral("VoiceKeyerStop"));
        m_stop->setAccessibleName(QStringLiteral("Stop"));
        m_stop->setToolTip(QStringLiteral("Stop sending, listening or recording"));
        row->addWidget(m_record, 1);
        row->addWidget(m_stop, 1);
        box->addLayout(row);
        connect(m_record, &QPushButton::clicked, this, &VoiceKeyerApplet::onRecordClicked);
        connect(m_stop, &QPushButton::clicked, this, &VoiceKeyerApplet::onStopClicked);
    }

    auto* grid = new QGridLayout;
    grid->setSpacing(4);
    for (int i = 0; i < kSlots; ++i) {
        const int n = i + 1;
        QPushButton* b = makeButton(QStringLiteral("M%1").arg(n), QStringLiteral("VoiceKeyerMessage%1").arg(n));
        b->setProperty("message", n);
        b->setContextMenuPolicy(Qt::CustomContextMenu);
        b->installEventFilter(this);
        auto* progress = new QFrame(b);
        progress->setObjectName(QStringLiteral("VoiceKeyerProgress"));
        progress->setAttribute(Qt::WA_TransparentForMouseEvents);
        progress->hide();
        m_messages[static_cast<size_t>(i)] = b;
        m_progress[static_cast<size_t>(i)] = progress;
        grid->addWidget(b, i / 4, i % 4);
        connect(b, &QPushButton::clicked, this, [this, n]() { onMessageClicked(n); });
        connect(b, &QPushButton::customContextMenuRequested, this,
                [this, n, b](const QPoint& at) { showMessageMenu(n, b->mapToGlobal(at)); });
    }
    box->addLayout(grid);

    m_level = new QProgressBar(m_body);
    m_level->setObjectName(QStringLiteral("VoiceKeyerLevel"));
    m_level->setRange(0, kLevelSteps);
    m_level->setValue(0);
    m_level->setTextVisible(false);
    m_level->setFixedHeight(6);
    m_level->setAccessibleName(QStringLiteral("Microphone level while recording"));
    m_level->setToolTip(QStringLiteral("Microphone level while recording"));
    m_level->setStyleSheet(QStringLiteral(
        "QProgressBar { background: #050507; border: 1px solid #1f1f23; border-radius: 2px; }"
        "QProgressBar::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #4caf6a,"
        " stop:0.7 #4caf6a, stop:0.71 #d8a55f, stop:0.9 #d8a55f, stop:0.91 #c25a5c, stop:1 #c25a5c); }"));
    box->addWidget(m_level);

    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);
        m_repeat = greenToggle(QStringLiteral("Rpt"), 40, kButtonHeight);
        m_repeat->setObjectName(QStringLiteral("VoiceKeyerRepeat"));
        m_repeat->setAccessibleName(QStringLiteral("Repeat"));
        m_repeat->setToolTip(QStringLiteral("Send the message again after the wait, until you stop it"));
        m_repeatSlider = new QSlider(Qt::Horizontal, m_body);
        m_repeatSlider->setObjectName(QStringLiteral("VoiceKeyerRepeatDelay"));
        m_repeatSlider->setRange(VoiceKeyer::kRepeatDelayMinSec, VoiceKeyer::kRepeatDelayMaxSec);
        m_repeatSlider->setValue(VoiceKeyer::kRepeatDelayDefaultSec);
        m_repeatSlider->setFixedHeight(18);
        m_repeatSlider->setAccessibleName(QStringLiteral("Seconds between repeats"));
        m_repeatSlider->setToolTip(QStringLiteral("Seconds between repeats"));
        m_repeatValue = insetValue(QString::number(VoiceKeyer::kRepeatDelayDefaultSec), 38);
        row->addWidget(m_repeat);
        row->addWidget(m_repeatSlider, 1);
        row->addWidget(m_repeatValue);
        box->addLayout(row);
        connect(m_repeat, &QPushButton::clicked, this, [this]() { sendRepeat(); });
        connect(m_repeatSlider, &QSlider::valueChanged, this, [this](int v) {
            m_repeatValue->setText(QString::number(v));
            if (!m_updatingFromModel && !m_repeatSlider->isSliderDown()) {
                sendRepeat();
            }
        });
        connect(m_repeatSlider, &QSlider::sliderReleased, this, [this]() { sendRepeat(); });
    }

    m_import = makeButton(QStringLiteral("Import WAV…"), QStringLiteral("VoiceKeyerImport"));
    m_import->setAccessibleName(QStringLiteral("Import WAV"));
    m_import->setToolTip(QStringLiteral(
        "Load a WAV or MP3 file into the first empty message (right-click a message to load into that one)"));
    connect(m_import, &QPushButton::clicked, this, &VoiceKeyerApplet::importFirstEmpty);
    box->addWidget(m_import);

    m_heldNote = new QLabel(m_body);
    m_heldNote->setObjectName(QStringLiteral("VoiceKeyerHeldNote"));
    m_heldNote->setWordWrap(true);
    m_heldNote->setStyleSheet(QStringLiteral("QLabel { color: #e8d9a0; font-size: 10.5px; }"));
    m_heldNote->hide();
    box->addWidget(m_heldNote);
    m_take = new QPushButton(QStringLiteral("Take transmit"), m_body);
    m_take->setObjectName(QStringLiteral("VoiceKeyerTakeTransmit"));
    m_take->setAccessibleName(QStringLiteral("Take transmit"));
    m_take->setToolTip(QStringLiteral("Take transmit for this window, so a message can be sent from here"));
    m_take->setFixedHeight(kButtonHeight);
    m_take->setStyleSheet(Style::buttonBaseStyle());
    m_take->hide();
    connect(m_take, &QPushButton::clicked, this, &VoiceKeyerApplet::takeTransmitRequested);
    box->addWidget(m_take);

    m_status = new QLabel(m_body);
    m_status->setObjectName(QStringLiteral("VoiceKeyerStatus"));
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    m_status->setMinimumHeight(32);
    m_status->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_status->setStyleSheet(QStringLiteral("QLabel { color: #a6a6ac; font-size: 11px; }"));
    box->addWidget(m_status);
}

RecordingsAccess* VoiceKeyerApplet::access() const
{
    return m_access.data();
}

void VoiceKeyerApplet::setAccess(RecordingsAccess* access)
{
    for (const QMetaObject::Connection& c : m_accessConnections) {
        disconnect(c);
    }
    m_accessConnections.clear();
    m_access = access;
    m_awaitingAnswer = false;
    m_clipImports.clear();
    m_exports.clear();
    if (access != nullptr) {
        m_accessConnections = {
            connect(access, &RecordingsAccess::documentChanged, this, &VoiceKeyerApplet::onDocumentChanged),
            connect(access, &RecordingsAccess::stateChanged, this, &VoiceKeyerApplet::onDocumentChanged),
            connect(access, &RecordingsAccess::refused, this,
                    [this](const QString& reason) {
                        if (m_awaitingAnswer) {
                            m_awaitingAnswer = false;
                            say(reason);
                        }
                    }),
            connect(access, &RecordingsAccess::voiceImportFinished, this,
                    [this](int n, bool ok, const QString& error) {
                        const QString clip = m_clipImports.take(n);
                        if (!clip.isEmpty()) {
                            QFile::remove(clip);
                        }
                        say(ok ? QStringLiteral("Saved M%1.").arg(n)
                               : QStringLiteral("M%1 was not saved: %2").arg(n).arg(error));
                    }),
            connect(access, &RecordingsAccess::voiceExportFinished, this,
                    [this](int n, bool ok, const QString& error) {
                        const QString to = m_exports.take(n);
                        say(ok ? QStringLiteral("Saved M%1 as %2.").arg(n).arg(QFileInfo(to).fileName())
                               : QStringLiteral("M%1 was not saved as a WAV file: %2").arg(n).arg(error));
                    }),
        };
    }
    onDocumentChanged();
}

void VoiceKeyerApplet::setMicSourceFactory(std::function<std::unique_ptr<MicClipSource>()> factory)
{
    m_sourceFactory = std::move(factory);
}

void VoiceKeyerApplet::syncFromModel()
{
    onDocumentChanged();
}

void VoiceKeyerApplet::setTransmitPermitted(bool permitted, const QString& reason)
{
    m_permitted = permitted;
    m_permittedReason = reason;
    refresh();
}

void VoiceKeyerApplet::setTakeTransmitOffered(bool offered)
{
    m_takeOffered = offered;
    refresh();
}

QString VoiceKeyerApplet::unavailableReason() const
{
    return m_access ? m_access->unavailableReason(kNeedsVersion) : RecordingsAccess::notConnectedReason();
}

qint64 VoiceKeyerApplet::now() const
{
    return m_now ? m_now() : 0;
}

QPushButton* VoiceKeyerApplet::messageButtonForTest(int n) const
{
    return n >= 1 && n <= kSlots ? m_messages[static_cast<size_t>(n - 1)] : nullptr;
}

QFrame* VoiceKeyerApplet::messageProgressForTest(int n) const
{
    return n >= 1 && n <= kSlots ? m_progress[static_cast<size_t>(n - 1)] : nullptr;
}

void VoiceKeyerApplet::onDocumentChanged()
{
    const bool wasSending = m_snapshot.sending.has_value();
    m_snapshot = m_access ? VoiceKeyerSnapshot::from(m_access->document()) : VoiceKeyerSnapshot{};
    m_snapshotAtMs = now();
    if (m_snapshot.sending.has_value() != wasSending) {
        m_awaitingAnswer = false;
        if (m_snapshot.sending.has_value()) {
            m_message.clear();
        }
    }
    {
        m_updatingFromModel = true;
        const QSignalBlocker blockRepeat(m_repeat);
        m_repeat->setChecked(m_snapshot.repeat);
        if (!m_repeatSlider->isSliderDown()) {
            m_repeatSlider->setValue(std::clamp(m_snapshot.repeatDelaySec, VoiceKeyer::kRepeatDelayMinSec,
                                                VoiceKeyer::kRepeatDelayMaxSec));
        }
        m_updatingFromModel = false;
    }
    refresh();
}

QString VoiceKeyerApplet::statusText() const
{
    if (m_recordingSlot > 0) {
        const qint64 ms = m_recordingShownMs.has_value() ? *m_recordingShownMs
                                                         : (m_recorder ? m_recorder->elapsedMs() : 0);
        return QStringLiteral("Recording %1 · %2 of 2:00. Click it again or Stop to finish.")
            .arg(m_snapshot.label(m_recordingSlot), VoiceKeyerSnapshot::clockText(ms));
    }
    if (const std::optional<VoiceKeyerSnapshot::Sending>& s = m_snapshot.sending) {
        const qint64 elapsed = std::max<qint64>(0, now() - m_snapshotAtMs);
        if (s->waitingMs > 0) {
            const qint64 secs = std::max<qint64>(1, (std::max<qint64>(0, s->waitingMs - elapsed) + 999) / 1000);
            return QStringLiteral("Repeat · %1 again in %2 s").arg(m_snapshot.label(s->n)).arg(secs);
        }
        const qint64 position = s->lengthMs > 0 ? std::min(s->lengthMs, s->positionMs + elapsed) : s->positionMs;
        const QString by = s->by.isEmpty() ? QString() : QStringLiteral(" from %1").arg(s->by);
        return QStringLiteral("Sending %1%2 · %3 / %4")
            .arg(m_snapshot.label(s->n), by, secondsText(position), secondsText(s->lengthMs));
    }
    if (!m_message.isEmpty()) {
        return m_message;
    }
    if (m_armed) {
        return m_armedFor > 0
            ? QStringLiteral("Record is on: click M%1 to record into it, then speak.").arg(m_armedFor)
            : QStringLiteral("Record is on: click the message to record into, then speak.");
    }
    return QStringLiteral("Click a message to send it. Right-click for Listen, Rename, Load WAV.");
}

void VoiceKeyerApplet::refresh()
{
    const QString why = unavailableReason();
    const bool usable = why.isEmpty();
    m_note->setText(why);
    m_note->setVisible(!usable);
    m_body->setEnabled(usable);
    if (usable) {
        m_body->setGraphicsEffect(nullptr);
    } else if (m_body->graphicsEffect() == nullptr) {
        auto* dim = new QGraphicsOpacityEffect(m_body);
        dim->setOpacity(kDimmedOpacity);
        m_body->setGraphicsEffect(dim);
    }
    if (!usable && m_recordingSlot > 0) {
        // The link went, or the Core cannot take a message: nothing is kept.
        if (m_recorder) {
            m_recorder->cancel();
        }
        m_recordingSlot = 0;
    }
    if (!usable) {
        m_armed = false;
    }
    m_body->setAccessibleDescription(why);

    const bool recording = m_recordingSlot > 0;
    setButtonLook(m_record, recording ? (m_blinkOn ? "red" : "redDim") : (m_armed ? "red" : "plain"));
    m_record->setAccessibleDescription(recording ? QStringLiteral("Recording") : (m_armed ? QStringLiteral("On")
                                                                                         : QString()));
    const bool held = usable && !m_permitted;
    m_heldNote->setText(m_permittedReason);
    m_heldNote->setVisible(held);
    m_take->setVisible(held && m_takeOffered);
    refreshMessages();
    m_status->setText(statusText());
    const bool live = recording || m_snapshot.sending.has_value();
    if (live && !m_refreshTimer->isActive()) {
        m_refreshTimer->start();
    } else if (!live && m_refreshTimer->isActive()) {
        m_refreshTimer->stop();
    }
}

void VoiceKeyerApplet::refreshMessages()
{
    const std::optional<VoiceKeyerSnapshot::Sending>& s = m_snapshot.sending;
    const qint64 elapsed = std::max<qint64>(0, now() - m_snapshotAtMs);
    for (int i = 0; i < kSlots; ++i) {
        const int n = i + 1;
        const VoiceKeyerSnapshot::Slot& slot = m_snapshot.messages[static_cast<size_t>(i)];
        QPushButton* b = m_messages[static_cast<size_t>(i)];
        QFrame* progress = m_progress[static_cast<size_t>(i)];
        const bool recordingThis = m_recordingSlot == n;
        const bool sendingThis = s.has_value() && s->n == n;
        b->setText(m_snapshot.label(n));
        const char* look = recordingThis ? "red"
            : sendingThis               ? "green"
            : slot.empty                ? "empty"
            : (!m_permitted && !m_armed) ? "held"
                                         : "plain";
        setButtonLook(b, look);
        QString tip;
        if (slot.empty) {
            tip = QStringLiteral("M%1 is empty: light Record, then click here").arg(n);
        } else if (!m_permitted && !m_armed) {
            tip = m_permittedReason;
        } else {
            tip = QStringLiteral("%1, %2").arg(m_snapshot.label(n), secondsText(slot.lengthMs));
        }
        b->setToolTip(tip);
        b->setAccessibleName(slot.empty ? QStringLiteral("M%1, empty").arg(n)
                                        : QStringLiteral("M%1, %2").arg(n).arg(m_snapshot.label(n)));
        double fraction = -1.0;
        if (recordingThis) {
            const qint64 ms = m_recordingShownMs.has_value() ? *m_recordingShownMs
                                                             : (m_recorder ? m_recorder->elapsedMs() : 0);
            fraction = static_cast<double>(ms) / LocalMicClipRecorder::kMaxMs;
        } else if (sendingThis && s->waitingMs <= 0 && s->lengthMs > 0) {
            fraction = static_cast<double>(std::min(s->lengthMs, s->positionMs + elapsed)) / s->lengthMs;
        }
        if (fraction >= 0.0) {
            progress->setStyleSheet(recordingThis ? QStringLiteral("QFrame { background: #ff6b6b; border: 0; }")
                                                  : QStringLiteral("QFrame { background: #4caf6a; border: 0; }"));
            progress->setGeometry(0, b->height() - kProgressHeight,
                                  static_cast<int>(std::lround(b->width() * std::clamp(fraction, 0.0, 1.0))),
                                  kProgressHeight);
            progress->show();
            progress->raise();
        } else {
            progress->hide();
        }
    }
}

void VoiceKeyerApplet::say(const QString& words)
{
    m_message = words;
    refresh();
}

void VoiceKeyerApplet::armRecord(int n)
{
    if (!unavailableReason().isEmpty() || m_recordingSlot > 0) {
        return;
    }
    m_armed = true;
    m_armedFor = n;
    m_message.clear();
    refresh();
}

void VoiceKeyerApplet::onMessageClicked(int n)
{
    if (!m_access || !unavailableReason().isEmpty()) {
        return;
    }
    if (m_recordingSlot == n) {
        stopRecording();
        return;
    }
    if (m_recordingSlot > 0) {
        say(QStringLiteral("Stop recording first."));
        return;
    }
    if (m_armed) {
        startRecording(n);
        return;
    }
    const std::optional<VoiceKeyerSnapshot::Sending>& s = m_snapshot.sending;
    if (s.has_value() && s->n == n) {
        m_awaitingAnswer = true;
        m_message.clear();
        m_access->stopVoice();
        return;
    }
    if (m_snapshot.messages[static_cast<size_t>(n - 1)].empty) {
        say(QStringLiteral("M%1 is empty. Light Record, then click it to record.").arg(n));
        return;
    }
    if (!m_permitted) {
        say(m_permittedReason);
        return;
    }
    m_awaitingAnswer = true;
    m_message.clear();
    m_access->sendVoice(n);
    refresh();
}

void VoiceKeyerApplet::onRecordClicked()
{
    if (m_recordingSlot > 0) {
        stopRecording();
        return;
    }
    m_armed = !m_armed;
    m_armedFor = 0;
    m_message.clear();
    refresh();
}

void VoiceKeyerApplet::onStopClicked()
{
    if (m_recordingSlot > 0) {
        stopRecording();
        return;
    }
    if (m_armed) {
        m_armed = false;
        refresh();
        return;
    }
    if (!m_access) {
        return;
    }
    if (m_snapshot.sending.has_value()) {
        m_awaitingAnswer = true;
        m_message.clear();
        m_access->stopVoice();
        return;
    }
    if (m_access->playback().has_value()) {
        m_access->stopPlay();
    }
}

void VoiceKeyerApplet::startRecording(int n)
{
    if (m_snapshot.onAir(n)) {
        say(VoiceKeyerSnapshot::onAirReason(n));
        return;
    }
    if (!m_recorder) {
        std::unique_ptr<MicClipSource> source = m_sourceFactory ? m_sourceFactory() : nullptr;
        if (!source) {
            say(QStringLiteral("This computer's microphone is not available for recording."));
            return;
        }
        m_recorder = std::make_unique<LocalMicClipRecorder>(std::move(source));
        connect(m_recorder.get(), &LocalMicClipRecorder::levelChanged, this,
                [this](float level) { m_level->setValue(static_cast<int>(std::lround(level * kLevelSteps))); });
        connect(m_recorder.get(), &LocalMicClipRecorder::finished, this, &VoiceKeyerApplet::onRecordingFinished);
        connect(m_recorder.get(), &LocalMicClipRecorder::failed, this, [this](const QString& reason) {
            m_recordingSlot = 0;
            m_blinkTimer->stop();
            say(reason);
        });
    }
    m_armed = false;
    m_armedFor = 0;
    m_message.clear();
    m_recordingShownMs.reset();
    m_recordingSlot = n;
    m_blinkOn = true;
    m_blinkTimer->start();
    m_recorder->start();
    refresh();
}

void VoiceKeyerApplet::stopRecording()
{
    if (m_recorder && m_recorder->isRecording()) {
        m_recorder->stop();   // finished() or failed()
        return;
    }
    m_recordingSlot = 0;
    m_recordingShownMs.reset();
    m_blinkTimer->stop();
    refresh();
}

void VoiceKeyerApplet::onRecordingFinished(const QString& path, qint64 ms)
{
    const int n = m_recordingSlot;
    m_recordingSlot = 0;
    m_blinkTimer->stop();
    m_level->setValue(0);
    if (n < 1 || !m_access) {
        QFile::remove(path);
        refresh();
        return;
    }
    m_clipImports.insert(n, path);
    m_access->importVoice(n, path, QString());
    say(ms >= LocalMicClipRecorder::kMaxMs
            ? QStringLiteral("Recorded M%1 (2:00, the most a message holds). Sending it to the Core.").arg(n)
            : QStringLiteral("Recorded M%1 (%2). Sending it to the Core.").arg(n).arg(secondsText(ms)));
}

QString VoiceKeyerApplet::nameForImport(int n, const QString& path) const
{
    // A message with a name keeps it; one without takes the file's.
    if (!m_snapshot.messages[static_cast<size_t>(n - 1)].name.isEmpty()) {
        return QString();
    }
    return QFileInfo(path).completeBaseName().left(kNameMaxChars);
}

void VoiceKeyerApplet::importInto(int n)
{
    if (!m_access || n < 1 || n > kSlots) {
        return;
    }
    if (m_snapshot.onAir(n)) {
        say(VoiceKeyerSnapshot::onAirReason(n));
        return;
    }
    const QString path = m_openChooser ? m_openChooser() : QString();
    if (path.isEmpty()) {
        return;
    }
    m_access->importVoice(n, path, nameForImport(n, path));
    say(QStringLiteral("Loading %1 into M%2.").arg(QFileInfo(path).fileName()).arg(n));
}

void VoiceKeyerApplet::importFirstEmpty()
{
    if (!m_access) {
        return;
    }
    const int n = m_access->firstEmptyVoiceSlot();
    if (n == 0) {
        say(QStringLiteral("All eight messages are full."));
        return;
    }
    importInto(n);
}

void VoiceKeyerApplet::saveAs(int n)
{
    if (!m_access || !m_saveChooser) {
        return;
    }
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    const QString to = m_saveChooser(QDir(folder).filePath(m_snapshot.label(n) + QStringLiteral(".wav")));
    if (to.isEmpty()) {
        return;
    }
    m_exports.insert(n, to);
    m_access->exportVoice(n, to);
    say(QStringLiteral("Saving M%1 as %2.").arg(n).arg(QFileInfo(to).fileName()));
}

void VoiceKeyerApplet::rename(int n)
{
    if (!m_access || !m_nameAsker) {
        return;
    }
    const std::optional<QString> name = m_nameAsker(n, m_snapshot.messages[static_cast<size_t>(n - 1)].name);
    if (!name.has_value()) {
        return;
    }
    m_awaitingAnswer = true;
    m_access->renameVoice(n, name->trimmed().left(kNameMaxChars));
}

void VoiceKeyerApplet::sendRepeat()
{
    if (!m_access || m_updatingFromModel) {
        return;
    }
    m_awaitingAnswer = true;
    m_access->setVoiceRepeat(m_repeat->isChecked(), m_repeatSlider->value());
}

void VoiceKeyerApplet::populateMessageMenu(QMenu* menu, int n)
{
    if (menu == nullptr || n < 1 || n > kSlots) {
        return;
    }
    menu->setToolTipsVisible(true);
    const bool usable = unavailableReason().isEmpty() && m_access;
    const bool has = !m_snapshot.messages[static_cast<size_t>(n - 1)].empty;
    const bool onAir = m_snapshot.onAir(n);
    const QString onAirWhy = VoiceKeyerSnapshot::onAirReason(n);
    const auto add = [menu](const QString& text, const QString& name, bool enabled, const QString& why) {
        QAction* a = menu->addAction(text);
        a->setObjectName(name);
        a->setEnabled(enabled);
        if (!enabled && !why.isEmpty()) {
            a->setToolTip(why);
            a->setStatusTip(why);
        }
        return a;
    };
    QAction* listen = add(QStringLiteral("Listen on your speaker"), QStringLiteral("VoiceListen"), usable && has,
                          has ? QString() : QStringLiteral("M%1 is empty.").arg(n));
    QAction* record = add(QStringLiteral("Record into this message"), QStringLiteral("VoiceRecordInto"),
                          usable && !onAir && m_recordingSlot == 0, onAir ? onAirWhy : QString());
    QAction* renameAction = add(QStringLiteral("Rename…"), QStringLiteral("VoiceRename"), usable, QString());
    menu->addSeparator();
    QAction* load = add(QStringLiteral("Load WAV…"), QStringLiteral("VoiceLoad"), usable && !onAir,
                        onAir ? onAirWhy : QString());
    QAction* save = add(QStringLiteral("Save as WAV…"), QStringLiteral("VoiceSave"), usable && has,
                        has ? QString() : QStringLiteral("M%1 is empty.").arg(n));
    menu->addSeparator();
    QAction* clear = add(QStringLiteral("Clear"), QStringLiteral("VoiceClear"), usable && has && !onAir,
                         onAir ? onAirWhy : (has ? QString() : QStringLiteral("M%1 is empty.").arg(n)));
    connect(listen, &QAction::triggered, this, [this, n]() {
        if (m_access) {
            m_awaitingAnswer = true;
            m_access->listenVoice(n);
        }
    });
    connect(record, &QAction::triggered, this, [this, n]() { startRecording(n); });
    connect(renameAction, &QAction::triggered, this, [this, n]() { rename(n); });
    connect(load, &QAction::triggered, this, [this, n]() { importInto(n); });
    connect(save, &QAction::triggered, this, [this, n]() { saveAs(n); });
    connect(clear, &QAction::triggered, this, [this, n]() {
        if (m_access) {
            m_awaitingAnswer = true;
            m_access->clearVoice(n);
        }
    });
}

void VoiceKeyerApplet::showMessageMenu(int n, const QPoint& globalPos)
{
    QMenu menu(this);
    populateMessageMenu(&menu, n);
    menu.exec(globalPos);
}

void VoiceKeyerApplet::showRecordingForTest(int n, qint64 elapsedMs, float level)
{
    m_armed = false;
    m_recordingSlot = n;
    m_recordingShownMs = elapsedMs;
    m_blinkOn = true;
    m_level->setValue(static_cast<int>(std::lround(level * kLevelSteps)));
    refresh();
}

bool VoiceKeyerApplet::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Resize && watched->property("message").isValid()) {
        refreshMessages();
    }
    return AppletWidget::eventFilter(watched, event);
}

}  // namespace NereusSDR
