// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original. See RecordingsPanel.h.
// =================================================================
// src/gui/recordings/RecordingsPanel.cpp  (NereusSDR)
// =================================================================
//
// Colours and sizes follow the approved mockup (docs/architecture/2026-11-
// 15-recording-voice-keyer-dsr-design/recordings-panel/), drawn there at
// twice the app's points.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 7.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 25: a whole-pan
//                 row opens on the pan, and its menu plays its sound only.
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "gui/recordings/RecordingsPanel.h"

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QStyle>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#include "core/recordings/RecordingPlayer.h"
#include "gui/ScopedChildWidget.h"
#include "gui/recordings/FlagRecordPlay.h"
#include "gui/recordings/RecordingsWindow.h"
#include "gui/recordings/RevealFile.h"
#include "models/recordings/RecordingsAccess.h"

namespace NereusSDR {

namespace {

// The mockup's rows are 62 high and their buttons 40 at twice the points.
constexpr int kRowHeight = 31;
constexpr int kPlaySize = 20;
constexpr int kIconPx = 9;
constexpr qreal kDimmedOpacity = 0.4;
constexpr int kProgressSteps = 1000;
const QString kSlicePrefix = QStringLiteral("slice:");

constexpr auto kPanelSheet = R"(
QFrame#RecordingsRow { background: transparent; border-radius: 4px; }
QFrame#RecordingsRow[odd="true"] { background: #0a0a0c; }
QFrame#RecordingsRow[look="playing"] { background: transparent; }
QFrame#RecordingsRow[look="recording"] { background: #1a0d0e; border: 1px solid #7a2c2e; }
QPushButton#RecordingsRowPlay { min-width: 20px; max-width: 20px; min-height: 20px; max-height: 20px;
    padding: 0; border-radius: 4px; border: 1px solid #2c2c31;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #222226, stop:1 #1b1b1f); }
QPushButton#RecordingsRowPlay:hover { background: #26262b; }
QPushButton#RecordingsRowPlay[look="playing"] { background: #1e3d5f; border-color: #4a7ba8; }
QPushButton#RecordingsRowPlay[look="recording"] { background: #7a2c2e; border-color: #c25a5c; }
QLabel#RecordingsRowTitle { color: #dcdce1; font-size: 11.5px; font-weight: 600; }
QLabel#RecordingsRowTitle[look="playing"] { color: #cfe4ff; }
QLabel#RecordingsRowTitle[look="recording"] { color: #ffd9d9; }
QLabel#RecordingsRowSub { color: #7e7e85; font-size: 9.5px; }
QLabel#RecordingsRowLength { color: #7e7e85; font-size: 10px; font-family: Menlo, "SF Mono", monospace; }
QProgressBar#RecordingsRowProgress { background: #1a1a1e; border: 0; border-radius: 1px;
    min-height: 2px; max-height: 2px; }
QProgressBar#RecordingsRowProgress::chunk { background: #3a9cff; border-radius: 1px; }
QLineEdit#RecordingsRename { color: #ffffff; background: #050507; border: 1px solid #4a7ba8;
    border-radius: 3px; padding: 0 3px; font-size: 11.5px; font-weight: 600; }
QPushButton#RecordingsAll { height: 20px; padding: 0 7px; border-radius: 4px; border: 1px solid #2c2c31;
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #222226, stop:1 #1b1b1f);
    color: #f0f0f2; font-size: 10.5px; font-weight: 700; }
QLabel#RecordingsUsage { color: #7e7e85; font-size: 9.5px; }
QLabel#RecordingsPanelNote { color: #c8c8cc; background: #1b1b1f; border: 1px solid #3a3a40;
    border-radius: 4px; padding: 5px 6px; font-size: 10.5px; }
QLabel#RecordingsEmpty { color: #7e7e85; font-size: 10.5px; }
)";

enum class Glyph { Triangle, Square, Pan };

QIcon glyphIcon(Glyph glyph, const QColor& colour)
{
    const qreal dpr = std::max<qreal>(2.0, qApp != nullptr ? qApp->devicePixelRatio() : 1.0);
    QPixmap pixmap(QSize(kIconPx, kIconPx) * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    // The mockup's 20-unit glyphs.
    p.scale(kIconPx / 20.0, kIconPx / 20.0);
    p.setPen(Qt::NoPen);
    p.setBrush(colour);
    QPainterPath path;
    switch (glyph) {
    case Glyph::Triangle:
        path.moveTo(6.0, 3.5);
        path.lineTo(16.0, 10.0);
        path.lineTo(6.0, 16.5);
        path.closeSubpath();
        p.drawPath(path);
        break;
    case Glyph::Square:
        p.drawRoundedRect(QRectF(4.5, 4.5, 11.0, 11.0), 1.5, 1.5);
        break;
    case Glyph::Pan:
        path.moveTo(2.0, 15.0);
        path.lineTo(6.0, 9.0);
        path.lineTo(9.0, 13.0);
        path.lineTo(13.0, 5.0);
        path.lineTo(18.0, 15.0);
        path.closeSubpath();
        p.drawPath(path);
        break;
    }
    return QIcon(pixmap);
}

void restyle(QWidget* w)
{
    w->style()->unpolish(w);
    w->style()->polish(w);
}

QString sliceName(int sliceId)
{
    return QStringLiteral("Slice %1").arg(QChar(QLatin1Char('A').unicode() + std::clamp(sliceId, 0, 25)));
}

}  // namespace

RecordingsPanel::RecordingsPanel(RadioModel* model, RecordingsAccess* access, QWidget* parent)
    : AppletWidget(model, parent)
{
    setObjectName(QStringLiteral("RecordingsPanel"));
    m_fileChooser = [this](const QString& suggested) {
        return QFileDialog::getSaveFileName(this, QStringLiteral("Save a copy here"), suggested);
    };
    m_confirm = [](RecordingsDeleteConfirm& dialog) { return dialog.exec() == QDialog::Accepted; };
    m_reveal = [](const QString& path) { return RevealFile::reveal(path); };
    m_now = [] { return QDateTime::currentDateTimeUtc(); };
    m_positionTimer = new QTimer(this);
    m_positionTimer->setInterval(kPositionRefreshMs);
    connect(m_positionTimer, &QTimer::timeout, this, &RecordingsPanel::refreshPlayback);
    buildUi();
    setAccess(access);
}

RecordingsPanel::~RecordingsPanel() = default;

void RecordingsPanel::buildUi()
{
    setStyleSheet(styleSheet() + QString::fromLatin1(kPanelSheet));
    auto* box = new QVBoxLayout(this);
    box->setContentsMargins(4, 3, 4, 4);
    box->setSpacing(3);

    m_note = new QLabel(this);
    m_note->setObjectName(QStringLiteral("RecordingsPanelNote"));
    m_note->setWordWrap(true);
    m_note->hide();
    box->addWidget(m_note);

    m_listBox = new QWidget(this);
    m_list = new QVBoxLayout(m_listBox);
    m_list->setContentsMargins(0, 0, 0, 0);
    m_list->setSpacing(0);
    box->addWidget(m_listBox);

    auto* foot = new QHBoxLayout;
    foot->setContentsMargins(2, 2, 2, 0);
    foot->setSpacing(6);
    m_all = new QPushButton(QStringLiteral("All recordings…"), this);
    m_all->setObjectName(QStringLiteral("RecordingsAll"));
    m_all->setToolTip(QStringLiteral("Open the list of every recording on the Core: search, sort, delete "
                                     "several, save copies"));
    connect(m_all, &QPushButton::clicked, this, [this]() { emit allRecordingsRequested(QString()); });
    m_usage = new QLabel(this);
    m_usage->setObjectName(QStringLiteral("RecordingsUsage"));
    m_usage->setTextFormat(Qt::RichText);
    m_usage->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    foot->addWidget(m_all);
    foot->addStretch(1);
    foot->addWidget(m_usage);
    box->addLayout(foot);
}

RecordingsAccess* RecordingsPanel::access() const
{
    return m_access.data();
}

void RecordingsPanel::setAccess(RecordingsAccess* access)
{
    for (const QMetaObject::Connection& c : m_accessConnections) {
        disconnect(c);
    }
    m_accessConnections.clear();
    m_access = access;
    m_entries.clear();
    m_listRevision = -1;
    m_copies.clear();
    m_copyStatus.clear();
    m_listReader = access != nullptr ? access->newListReader() : 0;
    if (access != nullptr) {
        m_accessConnections = {
            connect(access, &RecordingsAccess::documentChanged, this, &RecordingsPanel::onDocumentChanged),
            connect(access, &RecordingsAccess::stateChanged, this, &RecordingsPanel::onDocumentChanged),
            connect(access, &RecordingsAccess::listChanged, this, &RecordingsPanel::onListChanged),
            connect(access, &RecordingsAccess::playbackChanged, this, &RecordingsPanel::rebuildRows),
            connect(access, &RecordingsAccess::downloadProgress, this,
                    [this](const QString& id, qint64 done, qint64 total) {
                        if (m_copies.contains(id) && total > 0) {
                            m_copyStatus = QStringLiteral("Copying, %1%").arg(done * 100 / total);
                            refreshFooter();
                        }
                    }),
            connect(access, &RecordingsAccess::downloadFinished, this,
                    [this](const QString& id, bool ok, const QString& error) {
                        if (m_copies.remove(id)) {
                            m_copyStatus = ok ? QString() : QStringLiteral("Not copied: %1").arg(error);
                            refreshFooter();
                        }
                    }),
        };
    }
    onDocumentChanged();
}

void RecordingsPanel::syncFromModel()
{
    onDocumentChanged();
}

QString RecordingsPanel::unavailableReason() const
{
    return m_access ? m_access->unavailableReason() : RecordingsAccess::notConnectedReason();
}

QString RecordingsPanel::stretchReason() const
{
    return m_access ? m_access->unavailableReason(RecordingsAccess::kDsrVersion)
                    : RecordingsAccess::notConnectedReason();
}

void RecordingsPanel::onDocumentChanged()
{
    const QString why = unavailableReason();
    m_note->setText(why);
    m_note->setVisible(!why.isEmpty());
    m_listBox->setEnabled(why.isEmpty());
    m_all->setEnabled(why.isEmpty());
    for (QWidget* w : {m_listBox, static_cast<QWidget*>(m_all)}) {
        if (why.isEmpty()) {
            w->setGraphicsEffect(nullptr);
        } else if (w->graphicsEffect() == nullptr) {
            auto* dim = new QGraphicsOpacityEffect(w);
            dim->setOpacity(kDimmedOpacity);
            w->setGraphicsEffect(dim);
        }
    }
    m_all->setAccessibleDescription(why);
    requestListIfStale();
    rebuildRows();
}

void RecordingsPanel::requestListIfStale()
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
    query.count = kRows;
    m_access->requestList(m_listReader, query);
}

void RecordingsPanel::onListChanged(int reader)
{
    if (reader != m_listReader || !m_access) {
        return;
    }
    m_entries = m_access->listed(reader).entries;
    rebuildRows();
}

std::optional<RecordingEntry> RecordingsPanel::entryById(const QString& id) const
{
    for (const RecordingEntry& e : m_entries) {
        if (e.id == id) {
            return e;
        }
    }
    return std::nullopt;
}

const RecordingsPanel::Row* RecordingsPanel::rowById(const QString& id) const
{
    for (const Row& row : m_rows) {
        if (row.id == id) {
            return &row;
        }
    }
    return nullptr;
}

QStringList RecordingsPanel::rowIds() const
{
    QStringList ids;
    for (const Row& row : m_rows) {
        ids.append(row.id);
    }
    return ids;
}

QFrame* RecordingsPanel::rowForTest(const QString& id) const
{
    const Row* row = rowById(id);
    return row != nullptr ? row->frame.data() : nullptr;
}

QPushButton* RecordingsPanel::rowPlayButtonForTest(const QString& id) const
{
    const Row* row = rowById(id);
    return row != nullptr ? row->play.data() : nullptr;
}

QLabel* RecordingsPanel::rowTitleForTest(const QString& id) const
{
    const Row* row = rowById(id);
    return row != nullptr ? row->title.data() : nullptr;
}

QLabel* RecordingsPanel::rowLengthForTest(const QString& id) const
{
    const Row* row = rowById(id);
    return row != nullptr ? row->length.data() : nullptr;
}

QProgressBar* RecordingsPanel::rowProgressForTest(const QString& id) const
{
    const Row* row = rowById(id);
    return row != nullptr ? row->progress.data() : nullptr;
}

QString RecordingsPanel::dayText(const QDateTime& startUtc) const
{
    const QDateTime start = startUtc.toUTC();
    const qint64 days = start.date().daysTo(m_now().toUTC().date());
    const QString day = days <= 0 ? QStringLiteral("today")
        : days == 1               ? QStringLiteral("yesterday")
                                  : QStringLiteral("%1 days ago").arg(days);
    return QStringLiteral("%1 %2Z").arg(day, start.toString(QStringLiteral("HHmm")));
}

void RecordingsPanel::rebuildRows()
{
    const QString editText = m_editor ? m_editor->text() : QString();
    m_editor.clear();
    m_rows.clear();
    // Later, not now: a row's own button may be what asked for this.
    while (QLayoutItem* item = m_list->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->hide();
            w->removeEventFilter(this);
            w->deleteLater();
        }
        delete item;
    }

    const bool ready = unavailableReason().isEmpty();
    const QJsonObject document = m_access && ready ? m_access->document() : QJsonObject();
    const bool myVoice =
        document.value(QStringLiteral("settings")).toObject().value(QStringLiteral("keepMyVoice")).toBool(true);
    const std::optional<PlaybackState> playback = m_access && ready ? m_access->playback() : std::nullopt;
    const QString playing = playback.has_value() && playback->playing ? playback->recordingId : QString();

    const auto addRow = [this](const QString& id, const QString& look, int index) -> Row& {
        Row row;
        row.id = id;
        row.frame = new QFrame(m_listBox);
        row.frame->setObjectName(QStringLiteral("RecordingsRow"));
        row.frame->setProperty("recordingId", id);
        row.frame->setProperty("look", look);
        row.frame->setProperty("odd", index % 2 == 0);
        row.frame->setFixedHeight(kRowHeight);
        row.frame->installEventFilter(this);
        auto* grid = new QGridLayout(row.frame);
        grid->setContentsMargins(2, 0, 4, 0);
        grid->setHorizontalSpacing(6);
        grid->setVerticalSpacing(0);
        row.play = new QPushButton(row.frame);
        row.play->setObjectName(QStringLiteral("RecordingsRowPlay"));
        row.play->setProperty("look", look);
        row.play->setIconSize(QSize(kIconPx, kIconPx));
        row.play->setFocusPolicy(Qt::NoFocus);
        row.title = new QLabel(row.frame);
        row.title->setObjectName(QStringLiteral("RecordingsRowTitle"));
        row.title->setProperty("look", look);
        row.title->setTextFormat(Qt::RichText);
        row.length = new QLabel(row.frame);
        row.length->setObjectName(QStringLiteral("RecordingsRowLength"));
        row.length->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row.progress = new QProgressBar(row.frame);
        row.progress->setObjectName(QStringLiteral("RecordingsRowProgress"));
        row.progress->setRange(0, kProgressSteps);
        row.progress->setTextVisible(false);
        row.progress->setVisible(look == QStringLiteral("playing"));
        grid->addWidget(row.play, 0, 0, 2, 1, Qt::AlignVCenter);
        grid->addWidget(row.title, 0, 1, Qt::AlignBottom);
        grid->addWidget(row.length, 0, 2, 2, 1, Qt::AlignVCenter);
        grid->addWidget(row.progress, 2, 1, 1, 2);
        grid->setColumnStretch(1, 1);
        m_list->addWidget(row.frame);
        m_rows.append(row);
        return m_rows.last();
    };
    const auto addSub = [](Row& row, const QString& html) {
        auto* sub = new QLabel(row.frame);
        sub->setObjectName(QStringLiteral("RecordingsRowSub"));
        sub->setTextFormat(Qt::RichText);
        sub->setText(html);
        static_cast<QGridLayout*>(row.frame->layout())->addWidget(sub, 1, 1, Qt::AlignTop);
    };

    int shown = 0;
    // The slices recording now, in red at the top.
    for (const QJsonValue& value : document.value(QStringLiteral("slices")).toArray()) {
        const QJsonObject slice = value.toObject();
        if (slice.value(QStringLiteral("state")).toString() != QStringLiteral("recording") || shown >= kRows) {
            continue;
        }
        const int sliceId = slice.value(QStringLiteral("slice")).toInt();
        Row& row = addRow(kSlicePrefix + QString::number(sliceId), QStringLiteral("recording"), shown++);
        row.play->setIcon(glyphIcon(Glyph::Square, QColor::fromString(QStringLiteral("#ffd9d9"))));
        row.play->setToolTip(QStringLiteral("Stop recording %1").arg(sliceName(sliceId)));
        row.play->setAccessibleName(QStringLiteral("Stop recording"));
        connect(row.play, &QPushButton::clicked, this, [this, sliceId]() {
            if (m_access) {
                m_access->stop(sliceId);
            }
        });
        row.title->setText(QStringLiteral("<span style=\"color:#ff453a;\">●</span> Recording %1")
                               .arg(sliceName(sliceId)));
        QString where = m_sliceText ? m_sliceText(sliceId) : QString();
        const QString what = myVoice ? QStringLiteral("what you hear and your voice") : QStringLiteral("what you hear");
        addSub(row, (where.isEmpty() ? what : where + QStringLiteral(" · ") + what).toHtmlEscaped());
        row.length->setText(FlagRecordPlay::lengthText(slice.value(QStringLiteral("elapsedMs")).toInteger()));
    }

    for (const RecordingEntry& e : m_entries) {
        if (shown >= kRows) {
            break;
        }
        const bool on = e.id == playing;
        Row& row = addRow(e.id, on ? QStringLiteral("playing") : QString(), shown++);
        if (e.kind == RecordingKind::WholePan) {
            row.play->setIcon(glyphIcon(Glyph::Pan, QColor::fromString(QStringLiteral("#5e5e66"))));
            const QString why = stretchReason();
            row.play->setEnabled(why.isEmpty());
            row.play->setToolTip(why.isEmpty() ? RecordingsWindow::openOnPanTip() : why);
            row.play->setAccessibleDescription(why);
            row.play->setAccessibleName(QStringLiteral("Open on the pan"));
        } else {
            row.play->setIcon(glyphIcon(on ? Glyph::Square : Glyph::Triangle,
                                        QColor::fromString(on ? QStringLiteral("#ffffff") : QStringLiteral("#dcdce1"))));
            row.play->setToolTip(on ? QStringLiteral("Stop")
                                    : QStringLiteral("Play on this computer's speaker. Transmit while it plays "
                                                     "to send it on the air."));
            row.play->setAccessibleName(on ? QStringLiteral("Stop") : QStringLiteral("Play"));
            connect(row.play, &QPushButton::clicked, this, [this, id = e.id]() { togglePlay(id); });
        }
        const QString title = RecordingsWindow::entryTitle(e);
        if (e.id == m_editingId) {
            m_editor = new QLineEdit(row.frame);
            m_editor->setObjectName(QStringLiteral("RecordingsRename"));
            m_editor->setText(editText.isEmpty() ? e.name : editText);
            m_editor->installEventFilter(this);
            connect(m_editor, &QLineEdit::returnPressed, this, [this]() { finishRename(true); });
            static_cast<QGridLayout*>(row.frame->layout())->replaceWidget(row.title, m_editor);
            row.title->hide();
        }
        row.title->setText(title.toHtmlEscaped());
        row.title->setToolTip(e.name);
        addSub(row, QStringLiteral("<span style=\"color:%1; background:%2;\">&nbsp;%3&nbsp;</span>&nbsp; %4")
                        .arg(RecordingsWindow::kindTagColour(e.kind, false),
                             RecordingsWindow::kindTagColour(e.kind, true), RecordingsWindow::kindText(e.kind),
                             dayText(e.startUtc)));
        row.length->setText(FlagRecordPlay::lengthText(e.lengthMs));
    }
    if (m_editor) {
        m_editor->setFocus();
        m_editor->selectAll();
    } else {
        m_editingId.clear();
    }
    if (shown == 0) {
        auto* empty = new QLabel(ready ? QStringLiteral("Nothing recorded. Record on a slice's flag.")
                                       : QStringLiteral("No recordings to show."),
                                 m_listBox);
        empty->setObjectName(QStringLiteral("RecordingsEmpty"));
        empty->setContentsMargins(4, 6, 4, 6);
        m_list->addWidget(empty);
    }
    for (const Row& row : m_rows) {
        for (QWidget* w : {static_cast<QWidget*>(row.frame.data()), static_cast<QWidget*>(row.play.data()),
                           static_cast<QWidget*>(row.title.data())}) {
            restyle(w);
        }
    }
    refreshPlayback();
    refreshFooter();
}

void RecordingsPanel::refreshPlayback()
{
    const std::optional<PlaybackState> playback = m_access ? m_access->playback() : std::nullopt;
    const QString playing = playback.has_value() && playback->playing ? playback->recordingId : QString();
    if (const Row* row = rowById(playing)) {
        const std::optional<RecordingEntry> e = entryById(playing);
        const qint64 length = e.has_value() && e->lengthMs > 0 ? e->lengthMs : playback->lengthMs;
        row->length->setText(QStringLiteral("%1 / %2").arg(FlagRecordPlay::lengthText(playback->positionMs),
                                                          FlagRecordPlay::lengthText(length)));
        row->progress->setValue(length > 0 ? static_cast<int>(std::clamp<qint64>(
                                                 playback->positionMs * kProgressSteps / length, 0, kProgressSteps))
                                           : 0);
    }
    if (playing.isEmpty()) {
        m_positionTimer->stop();
    } else if (!m_positionTimer->isActive()) {
        m_positionTimer->start();
    }
}

void RecordingsPanel::refreshFooter()
{
    if (!m_access || !unavailableReason().isEmpty()) {
        m_usage->setText(QStringLiteral("Unknown while<br>not connected"));
        return;
    }
    const QJsonObject document = m_access->document();
    const QJsonObject index = document.value(QStringLiteral("index")).toObject();
    const qint64 count = index.value(QStringLiteral("count")).toInteger();
    QString first = QStringLiteral("%1 on the Core, %2")
                        .arg(count)
                        .arg(RecordingsWindow::sizeText(index.value(QStringLiteral("usedBytes")).toInteger()));
    if (!m_copyStatus.isEmpty()) {
        first = m_copyStatus;
    }
    const QString free = RecordingsWindow::freeSpaceText(document);
    m_usage->setText(free.isEmpty() ? first.toHtmlEscaped()
                                    : first.toHtmlEscaped() + QStringLiteral("<br>") + free.toHtmlEscaped());
}

void RecordingsPanel::togglePlay(const QString& id)
{
    if (!m_access || !unavailableReason().isEmpty()) {
        return;
    }
    const std::optional<RecordingEntry> e = entryById(id);
    if (!e.has_value()) {
        return;
    }
    if (e->kind == RecordingKind::WholePan) {
        // Opens on the pan: slice A plays it like rewind.
        if (stretchReason().isEmpty()) {
            m_access->openStretch(id);
        }
        return;
    }
    const std::optional<PlaybackState> playback = m_access->playback();
    if (playback.has_value() && playback->playing && playback->recordingId == id) {
        m_access->stopPlay();
    } else {
        m_access->play(id);
    }
}

void RecordingsPanel::startRename(const QString& id)
{
    if (!entryById(id).has_value()) {
        return;
    }
    m_editingId = id;
    rebuildRows();
}

void RecordingsPanel::finishRename(bool keep)
{
    const QString id = m_editingId;
    const QString text = m_editor ? m_editor->text().trimmed() : QString();
    m_editingId.clear();
    const std::optional<RecordingEntry> e = entryById(id);
    if (keep && m_access && e.has_value() && !text.isEmpty() && text != e->name) {
        QString why;
        if (isValidRecordingName(text, &why)) {
            m_access->rename(id, text);
        } else {
            m_copyStatus = why;
        }
    }
    // Deferred: this may run inside the editor's own signal.
    QTimer::singleShot(0, this, &RecordingsPanel::rebuildRows);
}

void RecordingsPanel::saveCopy(const QString& id)
{
    const std::optional<RecordingEntry> e = entryById(id);
    if (!e.has_value() || !m_access || !m_fileChooser) {
        return;
    }
    const QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QPointer<RecordingsPanel> self(this);
    const QString to = m_fileChooser(QDir(downloads).filePath(e->fileName.isEmpty() ? e->name : e->fileName));
    if (!self || to.isEmpty() || !m_access) {
        return;
    }
    m_copies.insert(id);
    m_copyStatus = QStringLiteral("Copying, 0%");
    refreshFooter();
    m_access->download(id, to);
}

void RecordingsPanel::reveal(const QString& id)
{
    const std::optional<RecordingEntry> e = entryById(id);
    if (!e.has_value() || !m_access || !m_access->coreIsOnThisComputer() || !m_reveal) {
        return;
    }
    const QString folder = QDir::fromNativeSeparators(
        m_access->document().value(QStringLiteral("index")).toObject().value(QStringLiteral("folder")).toString());
    if (!folder.isEmpty()) {
        m_reveal(QDir::toNativeSeparators(QDir(folder).filePath(e->fileName)));
    }
}

void RecordingsPanel::confirmDelete(const QString& id)
{
    const std::optional<RecordingEntry> e = entryById(id);
    if (!e.has_value() || !m_access || !m_confirm) {
        return;
    }
    QPointer<RecordingsPanel> self(this);
    ScopedChildWidget<RecordingsDeleteConfirm> confirm(QList<RecordingEntry>{*e}, this);
    const bool yes = m_confirm(*confirm.get());
    if (self && yes && m_access) {
        m_access->remove({id});
    }
}

void RecordingsPanel::populateRowMenu(QMenu* menu, const QString& id)
{
    const std::optional<RecordingEntry> e = entryById(id);
    if (!e.has_value()) {
        return;
    }
    QPointer<RecordingsPanel> self(this);
    const std::optional<PlaybackState> playback = m_access ? m_access->playback() : std::nullopt;
    const bool on = playback.has_value() && playback->playing && playback->recordingId == id;
    if (e->kind == RecordingKind::WholePan) {
        const QString why = stretchReason();
        QAction* open = menu->addAction(QStringLiteral("Open on the pan"));
        QAction* sound = menu->addAction(on ? QStringLiteral("Stop its sound") : QStringLiteral("Play its sound only"));
        for (QAction* a : {open, sound}) {
            a->setEnabled(why.isEmpty());
            a->setToolTip(why);
        }
        connect(open, &QAction::triggered, this, [self, id]() {
            if (self) {
                self->togglePlay(id);
            }
        });
        connect(sound, &QAction::triggered, this, [self, id, on]() {
            if (self && self->m_access && self->stretchReason().isEmpty()) {
                if (on) {
                    self->m_access->stopPlay();
                } else {
                    self->m_access->play(id);
                }
            }
        });
    } else {
        QAction* play = menu->addAction(on ? QStringLiteral("Stop") : QStringLiteral("Play"));
        connect(play, &QAction::triggered, this, [self, id]() {
            if (self) {
                self->togglePlay(id);
            }
        });
    }
    QAction* rename = menu->addAction(QStringLiteral("Rename…"));
    connect(rename, &QAction::triggered, this, [self, id]() {
        if (self) {
            self->startRename(id);
        }
    });
    QAction* copy = menu->addAction(QStringLiteral("Save a copy here…"));
    connect(copy, &QAction::triggered, this, [self, id]() {
        if (self) {
            self->saveCopy(id);
        }
    });
    const bool here = m_access && m_access->coreIsOnThisComputer();
    QAction* show = nullptr;
    if (here) {
        show = menu->addAction(RevealFile::actionText());
    } else {
        const QString why = QStringLiteral("The Core is on another computer.");
        show = menu->addAction(RevealFile::actionText() + QLatin1Char('\t') + why);
        show->setToolTip(why);
        show->setEnabled(false);
    }
    connect(show, &QAction::triggered, this, [self, id]() {
        if (self) {
            self->reveal(id);
        }
    });
    menu->addSeparator();
    QAction* del = menu->addAction(QStringLiteral("Delete…"));
    connect(del, &QAction::triggered, this, [self, id]() {
        if (self) {
            self->confirmDelete(id);
        }
    });
    menu->setToolTipsVisible(true);
}

void RecordingsPanel::showRowMenu(const QString& id, const QPoint& globalPos)
{
    if (!entryById(id).has_value() || !unavailableReason().isEmpty()) {
        return;
    }
    ScopedChildWidget<QMenu> menuOwner(this);
    populateRowMenu(menuOwner.get(), id);
    menuOwner.get()->exec(globalPos);
}

bool RecordingsPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_editor.data() && event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        finishRename(false);
        return true;
    }
    if (auto* frame = qobject_cast<QFrame*>(watched); frame != nullptr && frame->objectName() == QStringLiteral("RecordingsRow")) {
        const QString id = frame->property("recordingId").toString();
        if (event->type() == QEvent::MouseButtonDblClick) {
            togglePlay(id);
            return true;
        }
        if (event->type() == QEvent::ContextMenu) {
            showRowMenu(id, static_cast<QContextMenuEvent*>(event)->globalPos());
            return true;
        }
    }
    return AppletWidget::eventFilter(watched, event);
}

}  // namespace NereusSDR
