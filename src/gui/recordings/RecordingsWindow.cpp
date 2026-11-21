// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original. See RecordingsWindow.h.
// =================================================================
// src/gui/recordings/RecordingsWindow.cpp  (NereusSDR)
// =================================================================
//
// Colours follow the approved mockup (docs/architecture/2026-11-15-
// recording-voice-keyer-dsr-design/recordings-window/), the Setup window's
// look; the spec's words win over the mockup's sample text.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 7.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 25: saving a
//                 stretch, sound only and opening a saved stretch.
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "gui/recordings/RecordingsWindow.h"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QDir>
#include <QFileDialog>
#include <QGraphicsOpacityEffect>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QStyle>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

#include "core/recordings/RecordingPlayer.h"
#include "gui/ScopedChildWidget.h"
#include "gui/recordings/FlagRecordPlay.h"
#include "gui/recordings/RevealFile.h"
#include "models/recordings/RecordingsAccess.h"

namespace NereusSDR {

namespace {

// The mockup's 0.4 for everything while recordings cannot be used.
constexpr qreal kDimmedOpacity = 0.4;
// Progress of a copy, in thousandths.
constexpr int kProgressSteps = 1000;

constexpr auto kWindowSheet = R"(
RecordingsWindow { background: #08080a; }
QLabel { color: #dcdce1; font-size: 12px; }
QLabel#RecordingsDim { color: #a6a6ab; }
QLabel#RecordingsNote { color: #c8c8cc; background: #1b1b1f; border: 1px solid #3a3a40;
    border-radius: 6px; padding: 6px 9px; }
QGroupBox { color: #c4c4ca; font-weight: bold; border: 1px solid #2c2c31; border-radius: 4px;
    margin-top: 8px; padding: 6px 8px 6px 8px; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
QPushButton { height: 22px; padding: 0 11px; border-radius: 5px; border: 1px solid #38383e;
    background: #1b1b1f; color: #e8e8ec; font-size: 12px; }
QPushButton:hover { background: #222228; }
QPushButton:disabled { color: #5c5c62; }
QPushButton#RecordingsDelete { border-color: #6b2c2e; color: #ffb4b4; }
QPushButton#RecordingsDelete:disabled { border-color: #38383e; color: #5c5c62; }
QPushButton[segment="true"] { border-radius: 0; padding: 0 9px; }
QPushButton[segment="true"]:checked { background: #2f5f92; color: #ffffff; }
QLineEdit { height: 22px; border-radius: 5px; border: 1px solid #38383e; background: #040406;
    color: #ffffff; padding: 0 7px; font-size: 12px; }
QTableWidget { background: #08080a; color: #dcdce1; border: 1px solid #2c2c31; font-size: 12px;
    selection-background-color: #1e3d5f; selection-color: #ffffff; gridline-color: #141417; }
QTableWidget::item { padding: 0 6px; border-bottom: 1px solid #141417; }
QHeaderView::section { background: #1a1a1e; color: #dcdce1; border: 0; border-right: 1px solid #2c2c31;
    padding: 3px 8px; font-size: 12px; }
QToolButton#RecordingsRowPlay { min-width: 20px; max-width: 20px; min-height: 18px; max-height: 18px;
    border-radius: 4px; border: 1px solid #38383e; background: #1b1b1f; color: #8fd0a0; font-size: 9px; }
QToolButton#RecordingsRowPlay[playing="true"] { background: #2f5a40; border-color: #3f7a55; color: #ffffff; }
QToolButton#RecordingsRowPlay:disabled { color: #5c5c62; }
QProgressBar { background: #040406; border: 1px solid #2c2c31; border-radius: 3px; max-height: 8px; }
QProgressBar::chunk { background: #4caf6a; border-radius: 2px; }
)";

constexpr auto kDeleteSheet = R"(
QDialog { background: #1b1b1f; }
QLabel { color: #dcdce1; font-size: 12px; }
QPushButton { height: 24px; padding: 0 14px; border-radius: 5px; border: 1px solid #38383e;
    background: #222226; color: #f0f0f2; font-size: 12px; }
QPushButton#RecordingsConfirmDelete { background: #7a2c2e; border-color: #c25a5c; color: #ffd9d9; }
)";

QString playingId(RecordingsAccess* access)
{
    if (access == nullptr) {
        return {};
    }
    const std::optional<PlaybackState> playback = access->playback();
    return playback.has_value() && playback->playing ? playback->recordingId : QString();
}

// A table cell that is only text.
QTableWidgetItem* cell(const QString& text, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter)
{
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(align);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return item;
}

QString dimDash()
{
    return QStringLiteral("–");
}

}  // namespace

// ---------------------------------------------------------------------------
// The delete confirm.

RecordingsDeleteConfirm::RecordingsDeleteConfirm(const QList<RecordingEntry>& entries, QWidget* parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("RecordingsDeleteConfirm"));
    setWindowTitle(QStringLiteral("Delete recordings"));
    setModal(true);
    setStyleSheet(QString::fromLatin1(kDeleteSheet));
    auto* box = new QVBoxLayout(this);
    box->setContentsMargins(16, 14, 16, 12);
    box->setSpacing(8);

    qint64 bytes = 0;
    QStringList lines;
    for (const RecordingEntry& e : entries) {
        bytes += e.sizeBytes;
        lines.append(QStringLiteral("<span style=\"color:#a6a6ab;\">%1, %2, %3</span>")
                         .arg(RecordingsWindow::whenText(e.startUtc), RecordingsWindow::kindText(e.kind),
                              RecordingsWindow::sizeText(e.sizeBytes)));
    }
    const QString head = entries.size() == 1
        ? QStringLiteral("Delete %1 from the Core?").arg(RecordingsWindow::entryTitle(entries.first()).toHtmlEscaped())
        : QStringLiteral("Delete %1 recordings from the Core?").arg(entries.size());
    m_text = new QLabel(this);
    m_text->setObjectName(QStringLiteral("RecordingsConfirmText"));
    m_text->setTextFormat(Qt::RichText);
    m_text->setWordWrap(true);
    m_text->setMinimumWidth(380);
    m_text->setText(QStringLiteral("<b>%1</b><br>%2<br><br>It is removed for every computer and phone. "
                                   "This frees %3 and cannot be undone. Copies already saved on a "
                                   "computer stay.")
                        .arg(head, lines.join(QStringLiteral("<br>")), RecordingsWindow::sizeText(bytes)));
    box->addWidget(m_text);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    m_cancel = new QPushButton(QStringLiteral("Cancel"), this);
    m_cancel->setObjectName(QStringLiteral("RecordingsConfirmCancel"));
    m_delete = new QPushButton(QStringLiteral("Delete"), this);
    m_delete->setObjectName(QStringLiteral("RecordingsConfirmDelete"));
    m_cancel->setDefault(true);
    buttons->addWidget(m_cancel);
    buttons->addWidget(m_delete);
    box->addLayout(buttons);
    connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_delete, &QPushButton::clicked, this, &QDialog::accept);
}

QString RecordingsDeleteConfirm::text() const
{
    return m_text->text();
}

// ---------------------------------------------------------------------------
// Words.

QString RecordingsWindow::kindText(RecordingKind kind)
{
    switch (kind) {
    case RecordingKind::Contact:
        return QStringLiteral("Contact");
    case RecordingKind::WholePan:
        return QStringLiteral("Whole pan");
    case RecordingKind::Sound:
        return QStringLiteral("Sound");
    }
    return QStringLiteral("Contact");
}

QString RecordingsWindow::kindTagColour(RecordingKind kind, bool background)
{
    // recordings-panel/ .tag.c, .tag.w and .tag.s.
    switch (kind) {
    case RecordingKind::WholePan:
        return background ? QStringLiteral("#1e2a3a") : QStringLiteral("#8cc4ff");
    case RecordingKind::Sound:
        return background ? QStringLiteral("#2a2418") : QStringLiteral("#d8a55f");
    case RecordingKind::Contact:
        break;
    }
    return background ? QStringLiteral("#1f2a1f") : QStringLiteral("#8fc79c");
}

QString RecordingsWindow::sizeText(qint64 bytes)
{
    const double mb = static_cast<double>(std::max<qint64>(0, bytes)) / 1.0e6;
    if (mb >= 10000.0) {
        return QStringLiteral("%1 GB").arg(qRound64(mb / 1000.0));
    }
    if (mb >= 1000.0) {
        return QStringLiteral("%1 GB").arg(mb / 1000.0, 0, 'f', 1);
    }
    if (mb >= 10.0) {
        return QStringLiteral("%1 MB").arg(qRound(mb));
    }
    return QStringLiteral("%1 MB").arg(mb, 0, 'f', 1);
}

QString RecordingsWindow::whenText(const QDateTime& startUtc)
{
    return QLocale::c().toString(startUtc.toUTC(), QStringLiteral("d MMM HH:mm"));
}

QString RecordingsWindow::entryTitle(const RecordingEntry& entry)
{
    // A name the Core made from the recording itself says no more than the
    // row's own columns: show frequency, mode and call the mockup's way.
    if (entry.name == recordingBaseName(entry.startUtc, entry.frequencyHz, entry.mode, entry.call)) {
        QString title = recordingFrequencyText(entry.frequencyHz);
        if (!entry.mode.isEmpty()) {
            title += QLatin1Char(' ') + entry.mode;
        }
        if (!entry.call.isEmpty()) {
            title += QStringLiteral("  ") + entry.call;
        }
        return title;
    }
    return entry.name;
}

QString RecordingsWindow::freeSpaceText(const QJsonObject& document)
{
    const QJsonObject index = document.value(QStringLiteral("index")).toObject();
    if (!index.contains(QStringLiteral("freeBytes"))) {
        return {};
    }
    const QString free = sizeText(index.value(QStringLiteral("freeBytes")).toInteger());
    const QJsonObject settings = document.value(QStringLiteral("settings")).toObject();
    const QString disk = QDir::fromNativeSeparators(settings.value(QStringLiteral("disk")).toString());
    QString name;
    for (const QJsonValue& value : settings.value(QStringLiteral("disks")).toArray()) {
        const QJsonObject d = value.toObject();
        const QString root = QDir::fromNativeSeparators(d.value(QStringLiteral("root")).toString());
        if ((disk.isEmpty() && d.value(QStringLiteral("own")).toBool()) || (!disk.isEmpty() && root == disk)) {
            name = d.value(QStringLiteral("name")).toString();
            break;
        }
    }
    return name.isEmpty() ? QStringLiteral("%1 free").arg(free) : QStringLiteral("%1 free on %2").arg(free, name);
}

// ---------------------------------------------------------------------------
// The window.

RecordingsWindow* RecordingsWindow::showFor(RecordingsAccess* access, QWidget* parent)
{
    RecordingsWindow* window =
        parent != nullptr ? parent->findChild<RecordingsWindow*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
    if (window == nullptr) {
        window = new RecordingsWindow(access, parent);
        if (parent == nullptr) {
            window->setAttribute(Qt::WA_DeleteOnClose);
        }
    } else if (window->access() != access) {
        window->setAccess(access);
    }
    window->show();
    window->raise();
    window->activateWindow();
    return window;
}

RecordingsWindow::RecordingsWindow(RecordingsAccess* access, QWidget* parent)
    : QWidget(parent, Qt::Window)
{
    setObjectName(QStringLiteral("RecordingsWindow"));
    setWindowTitle(QStringLiteral("Recordings"));
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(QString::fromLatin1(kWindowSheet));
    resize(kDefaultWidth, kDefaultHeight);

    m_renamePrompt = [this](const QString& current) -> std::optional<QString> {
        bool ok = false;
        const QString name = QInputDialog::getText(this, QStringLiteral("Rename"),
                                                   QStringLiteral("A name for this recording:"),
                                                   QLineEdit::Normal, current, &ok);
        return ok ? std::optional<QString>(name) : std::nullopt;
    };
    m_fileChooser = [this](const QString& suggested, bool folder) {
        return folder ? QFileDialog::getExistingDirectory(this, QStringLiteral("Save copies here"), suggested)
                      : QFileDialog::getSaveFileName(this, QStringLiteral("Save a copy here"), suggested);
    };
    m_confirm = [](RecordingsDeleteConfirm& dialog) { return dialog.exec() == QDialog::Accepted; };
    m_revealFile = [](const QString& path) { return RevealFile::reveal(path); };

    m_positionTimer = new QTimer(this);
    m_positionTimer->setInterval(kPositionRefreshMs);
    connect(m_positionTimer, &QTimer::timeout, this, &RecordingsWindow::refreshPlayback);

    buildUi();
    setAccess(access);
}

RecordingsWindow::~RecordingsWindow() = default;

RecordingsAccess* RecordingsWindow::access() const
{
    return m_access.data();
}

void RecordingsWindow::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(14, 10, 14, 12);
    outer->setSpacing(8);

    m_note = new QLabel(this);
    m_note->setObjectName(QStringLiteral("RecordingsNote"));
    m_note->setWordWrap(true);
    m_note->hide();
    outer->addWidget(m_note);

    m_body = new QWidget(this);
    auto* body = new QVBoxLayout(m_body);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(8);
    outer->addWidget(m_body, 1);

    m_store = new QGroupBox(QStringLiteral("On the Core"), m_body);
    auto* store = new QVBoxLayout(m_store);
    store->setContentsMargins(8, 6, 8, 6);
    store->setSpacing(3);
    m_totals = new QLabel(m_store);
    m_totals->setObjectName(QStringLiteral("RecordingsTotals"));
    m_totals->setTextFormat(Qt::RichText);
    m_totalsDetail = new QLabel(m_store);
    m_totalsDetail->setObjectName(QStringLiteral("RecordingsDim"));
    m_totalsDetail->setWordWrap(true);
    store->addWidget(m_totals);
    store->addWidget(m_totalsDetail);
    body->addWidget(m_store);

    auto* tools = new QHBoxLayout;
    tools->setSpacing(0);
    m_filterGroup = new QButtonGroup(this);
    m_filterGroup->setExclusive(true);
    const std::pair<Filter, const char*> filters[] = {{Filter::All, "All"},
                                                      {Filter::Contact, "Contact"},
                                                      {Filter::WholePan, "Whole pan"},
                                                      {Filter::Sound, "Sound"}};
    for (const auto& [which, text] : filters) {
        auto* b = new QPushButton(QString::fromLatin1(text), m_body);
        b->setCheckable(true);
        b->setProperty("segment", true);
        b->setChecked(which == m_filter);
        b->setObjectName(QStringLiteral("RecordingsFilter%1").arg(static_cast<int>(which)));
        m_filterGroup->addButton(b, static_cast<int>(which));
        tools->addWidget(b);
    }
    connect(m_filterGroup, &QButtonGroup::idClicked, this,
            [this](int id) { setFilter(static_cast<Filter>(id)); });
    tools->addStretch(1);
    m_search = new QLineEdit(m_body);
    m_search->setObjectName(QStringLiteral("RecordingsSearch"));
    m_search->setPlaceholderText(QStringLiteral("Search call or name"));
    m_search->setClearButtonEnabled(true);
    m_search->setFixedWidth(220);
    connect(m_search, &QLineEdit::textChanged, this, &RecordingsWindow::setSearchText);
    tools->addWidget(m_search);
    body->addLayout(tools);

    m_table = new QTableWidget(0, ColCount, m_body);
    m_table->setObjectName(QStringLiteral("RecordingsTable"));
    m_table->setHorizontalHeaderLabels({QString(), QStringLiteral("When (UTC)"), QStringLiteral("Kind"),
                                        QStringLiteral("Frequency"), QStringLiteral("Mode"), QStringLiteral("Call"),
                                        QStringLiteral("Name"), QStringLiteral("Length"), QStringLiteral("Size")});
    m_table->horizontalHeaderItem(ColPlay)->setToolTip(QStringLiteral("Sort the one playing to the top"));
    m_table->horizontalHeaderItem(ColLength)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_table->horizontalHeaderItem(ColSize)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setDefaultSectionSize(22);
    m_table->setShowGrid(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setWordWrap(false);
    m_table->setFocusPolicy(Qt::StrongFocus);
    QHeaderView* header = m_table->horizontalHeader();
    header->setSectionsClickable(true);
    header->setSortIndicatorShown(true);
    header->setHighlightSections(false);
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(ColName, QHeaderView::Stretch);
    header->setMinimumSectionSize(28);
    header->setSortIndicator(m_sortColumn, m_sortOrder);
    connect(header, &QHeaderView::sectionClicked, this, [this](int section) {
        const auto column = static_cast<Column>(section);
        if (column == m_sortColumn) {
            sortBy(column, m_sortOrder == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder);
        } else {
            // Newest, longest and biggest first; words from A.
            const bool downFirst = column == ColWhen || column == ColLength || column == ColSize || column == ColPlay;
            sortBy(column, downFirst ? Qt::DescendingOrder : Qt::AscendingOrder);
        }
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        if (!m_rebuilding) {
            refreshButtons();
        }
    });
    // A whole-pan row's right-click: open it on the pan, or play just its
    // sound (Task 25).
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QWidget::customContextMenuRequested, this, &RecordingsWindow::showRowMenu);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (QTableWidgetItem* item = m_table->item(row, ColWhen)) {
            togglePlay(item->data(Qt::UserRole).toString());
        }
    });
    body->addWidget(m_table, 1);

    auto* acts = new QHBoxLayout;
    acts->setSpacing(6);
    const auto button = [this](const QString& text, const char* name) {
        auto* b = new QPushButton(text, m_body);
        b->setObjectName(QString::fromLatin1(name));
        return b;
    };
    m_play = button(QStringLiteral("Play"), "RecordingsPlay");
    // Port step 2: a direct REC control. The per-slice flag button is not
    // ported yet; this records the first slice.
    m_record = button(QStringLiteral("REC"), "RecordingsRecord");
    acts->addWidget(m_record);
    m_rename = button(QStringLiteral("Rename…"), "RecordingsRename");
    m_soundOnly = button(QStringLiteral("Save sound only…"), "RecordingsSoundOnly");
    m_copy = button(QStringLiteral("Save a copy here…"), "RecordingsCopy");
    m_reveal = button(RevealFile::actionText(), "RecordingsReveal");
    m_delete = button(QStringLiteral("Delete…"), "RecordingsDelete");
    acts->addWidget(m_play);
    acts->addWidget(m_rename);
    acts->addWidget(m_soundOnly);
    acts->addStretch(1);
    acts->addWidget(m_copy);
    acts->addWidget(m_reveal);
    acts->addWidget(m_delete);
    body->addLayout(acts);
    connect(m_play, &QPushButton::clicked, this, &RecordingsWindow::playSelected);
    connect(m_record, &QPushButton::clicked, this, [this] {
        if (m_access == nullptr) { return; }
        m_recordOn = !m_recordOn;
        if (m_recordOn) {
            m_access->start(0);
            m_record->setText(QStringLiteral("STOP"));
        } else {
            m_access->stop(0);
            m_record->setText(QStringLiteral("REC"));
        }
    });
    connect(m_rename, &QPushButton::clicked, this, &RecordingsWindow::renameSelected);
    connect(m_soundOnly, &QPushButton::clicked, this, &RecordingsWindow::saveSoundOfSelected);
    connect(m_copy, &QPushButton::clicked, this, &RecordingsWindow::saveCopiesOfSelected);
    connect(m_reveal, &QPushButton::clicked, this, &RecordingsWindow::showSelectedInFinder);
    connect(m_delete, &QPushButton::clicked, this, &RecordingsWindow::deleteSelected);

    auto* status = new QHBoxLayout;
    status->setSpacing(8);
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("RecordingsDim"));
    m_copyProgress = new QProgressBar(this);
    m_copyProgress->setObjectName(QStringLiteral("RecordingsCopyProgress"));
    m_copyProgress->setRange(0, kProgressSteps);
    m_copyProgress->setTextVisible(false);
    m_copyProgress->setFixedWidth(180);
    m_copyProgress->hide();
    status->addWidget(m_status, 1);
    status->addWidget(m_copyProgress);
    outer->addLayout(status);
}

void RecordingsWindow::setAccess(RecordingsAccess* access)
{
    for (const QMetaObject::Connection& c : m_accessConnections) {
        disconnect(c);
    }
    m_accessConnections.clear();
    m_access = access;
    m_entries.clear();
    m_loading.clear();
    m_listRevision = -1;
    m_loadInFlight = false;
    m_reloadAgain = false;
    m_copyQueue.clear();
    m_copying.reset();
    m_copyProgress->hide();
    m_listReader = access != nullptr ? access->newListReader() : 0;
    if (access != nullptr) {
        m_accessConnections = {
            connect(access, &RecordingsAccess::documentChanged, this, &RecordingsWindow::onDocumentChanged),
            connect(access, &RecordingsAccess::stateChanged, this, &RecordingsWindow::onDocumentChanged),
            connect(access, &RecordingsAccess::listChanged, this, &RecordingsWindow::onListChanged),
            connect(access, &RecordingsAccess::playbackChanged, this, &RecordingsWindow::refreshPlayback),
            connect(access, &RecordingsAccess::refused, this,
                    [this](const QString& reason) { m_status->setText(reason); }),
            connect(access, &RecordingsAccess::stretchSaving, this,
                    [this](const QString& note) { m_status->setText(note); }),
            connect(access, &RecordingsAccess::downloadProgress, this, &RecordingsWindow::onDownloadProgress),
            connect(access, &RecordingsAccess::downloadFinished, this, &RecordingsWindow::onDownloadFinished),
        };
    }
    onDocumentChanged();
}

QString RecordingsWindow::unavailableReason() const
{
    return m_access ? m_access->unavailableReason() : RecordingsAccess::notConnectedReason();
}

void RecordingsWindow::refreshAvailability()
{
    const QString why = unavailableReason();
    m_note->setText(why);
    m_note->setVisible(!why.isEmpty());
    m_body->setEnabled(why.isEmpty());
    if (why.isEmpty()) {
        m_body->setGraphicsEffect(nullptr);
    } else if (m_body->graphicsEffect() == nullptr) {
        auto* dim = new QGraphicsOpacityEffect(m_body);
        dim->setOpacity(kDimmedOpacity);
        m_body->setGraphicsEffect(dim);
    }
}

void RecordingsWindow::onDocumentChanged()
{
    refreshAvailability();
    if (!unavailableReason().isEmpty()) {
        m_listRevision = -1;
        m_loadInFlight = false;
        m_reloadAgain = false;
        m_loading.clear();
        if (!m_entries.isEmpty()) {
            m_entries.clear();
            rebuildRows();
        }
        refreshTotals();
        refreshButtons();
        return;
    }
    const QJsonObject index = m_access->document().value(QStringLiteral("index")).toObject();
    if (!index.isEmpty()) {
        const qint64 revision = index.value(QStringLiteral("revision")).toInteger();
        if (revision != m_listRevision) {
            m_listRevision = revision;
            reload();
        }
    }
    refreshTotals();
    refreshButtons();
}

void RecordingsWindow::reload()
{
    if (m_loadInFlight) {
        m_reloadAgain = true;
        return;
    }
    m_loading.clear();
    m_loadInFlight = true;
    requestPage(0);
}

void RecordingsWindow::requestPage(int offset)
{
    RecordingsListQuery query;
    query.offset = offset;
    query.count = kPageSize;
    m_access->requestList(m_listReader, query);
}

void RecordingsWindow::onListChanged(int reader)
{
    if (reader != m_listReader || !m_access || !m_loadInFlight) {
        return;
    }
    if (m_reloadAgain) {
        // The index moved while this load ran: start it again.
        m_reloadAgain = false;
        m_loading.clear();
        requestPage(0);
        return;
    }
    const RecordingsListPage page = m_access->listed(reader);
    m_loading += page.entries;
    if (!page.entries.isEmpty() && m_loading.size() < page.total) {
        requestPage(static_cast<int>(m_loading.size()));
        return;
    }
    m_loadInFlight = false;
    m_entries = m_loading;
    m_loading.clear();
    rebuildRows();
    refreshTotals();
    if (!m_pendingSelect.isEmpty() && entryById(m_pendingSelect).has_value()) {
        const QString id = m_pendingSelect;
        m_pendingSelect.clear();
        selectRecording(id);
    }
}

std::optional<RecordingEntry> RecordingsWindow::entryById(const QString& id) const
{
    for (const RecordingEntry& e : m_entries) {
        if (e.id == id) {
            return e;
        }
    }
    return std::nullopt;
}

QList<RecordingEntry> RecordingsWindow::visibleEntries() const
{
    std::optional<RecordingKind> kind;
    if (m_filter == Filter::Contact) {
        kind = RecordingKind::Contact;
    } else if (m_filter == Filter::WholePan) {
        kind = RecordingKind::WholePan;
    } else if (m_filter == Filter::Sound) {
        kind = RecordingKind::Sound;
    }
    QList<RecordingEntry> out;
    const QString q = m_searchText.trimmed();
    for (const RecordingEntry& e : filterRecordings(m_entries, std::nullopt, kind)) {
        if (q.isEmpty() || e.name.contains(q, Qt::CaseInsensitive) || e.call.contains(q, Qt::CaseInsensitive)) {
            out.append(e);
        }
    }
    const QString playing = playingId(m_access);
    const Column column = m_sortColumn;
    // -1, 0 or 1 for a before b in the column's own order.
    const auto compare = [&playing, column](const RecordingEntry& a, const RecordingEntry& b) -> int {
        const auto three = [](auto x, auto y) { return x < y ? -1 : (y < x ? 1 : 0); };
        switch (column) {
        case ColPlay:
            return three(a.id == playing, b.id == playing);
        case ColWhen:
            return three(a.startUtc, b.startUtc);
        case ColKind:
            return QString::compare(kindText(a.kind), kindText(b.kind));
        case ColFrequency:
            return three(a.frequencyHz, b.frequencyHz);
        case ColMode:
            return QString::compare(a.mode, b.mode, Qt::CaseInsensitive);
        case ColCall:
            return QString::compare(a.call, b.call, Qt::CaseInsensitive);
        case ColName:
            return QString::localeAwareCompare(a.name.toLower(), b.name.toLower());
        case ColLength:
            return three(a.lengthMs, b.lengthMs);
        case ColSize:
            return three(a.sizeBytes, b.sizeBytes);
        case ColCount:
            break;
        }
        return 0;
    };
    const bool down = m_sortOrder == Qt::DescendingOrder;
    // Ties keep the Core's order, newest first.
    std::stable_sort(out.begin(), out.end(), [&compare, down](const RecordingEntry& a, const RecordingEntry& b) {
        const int c = compare(a, b);
        return down ? c > 0 : c < 0;
    });
    return out;
}

QStringList RecordingsWindow::visibleIds() const
{
    QStringList ids;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (QTableWidgetItem* item = m_table->item(row, ColWhen)) {
            ids.append(item->data(Qt::UserRole).toString());
        }
    }
    return ids;
}

QStringList RecordingsWindow::selectedIds() const
{
    QStringList ids;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem* item = m_table->item(row, ColWhen);
        if (item != nullptr && item->isSelected()) {
            ids.append(item->data(Qt::UserRole).toString());
        }
    }
    return ids;
}

QList<RecordingEntry> RecordingsWindow::selectedEntries() const
{
    QList<RecordingEntry> out;
    for (const QString& id : selectedIds()) {
        if (const std::optional<RecordingEntry> e = entryById(id)) {
            out.append(*e);
        }
    }
    return out;
}

void RecordingsWindow::setSelectedIds(const QStringList& ids)
{
    {
        const QSignalBlocker block(m_table->selectionModel());
        m_table->clearSelection();
        for (int row = 0; row < m_table->rowCount(); ++row) {
            QTableWidgetItem* item = m_table->item(row, ColWhen);
            if (item != nullptr && ids.contains(item->data(Qt::UserRole).toString())) {
                m_table->selectionModel()->select(m_table->model()->index(row, 0),
                                                  QItemSelectionModel::Select | QItemSelectionModel::Rows);
            }
        }
    }
    m_table->viewport()->update();
    refreshButtons();
}

void RecordingsWindow::selectRecording(const QString& id)
{
    if (id.isEmpty()) {
        return;
    }
    if (!entryById(id).has_value()) {
        // Not here yet: select it when the list comes.
        m_pendingSelect = id;
        return;
    }
    if (!visibleIds().contains(id)) {
        m_searchText.clear();
        {
            const QSignalBlocker block(m_search);
            m_search->clear();
        }
        setFilter(Filter::All);
    }
    setSelectedIds({id});
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (m_table->item(row, ColWhen)->data(Qt::UserRole).toString() == id) {
            m_table->scrollToItem(m_table->item(row, ColWhen));
            break;
        }
    }
}

void RecordingsWindow::setFilter(Filter filter)
{
    m_filter = filter;
    if (QAbstractButton* b = m_filterGroup->button(static_cast<int>(filter))) {
        const QSignalBlocker block(b);
        b->setChecked(true);
    }
    rebuildRows();
}

QPushButton* RecordingsWindow::filterButtonForTest(Filter filter) const
{
    return qobject_cast<QPushButton*>(m_filterGroup->button(static_cast<int>(filter)));
}

void RecordingsWindow::setSearchText(const QString& text)
{
    m_searchText = text;
    if (m_search->text() != text) {
        const QSignalBlocker block(m_search);
        m_search->setText(text);
    }
    rebuildRows();
}

void RecordingsWindow::sortBy(Column column, Qt::SortOrder order)
{
    m_sortColumn = column;
    m_sortOrder = order;
    m_table->horizontalHeader()->setSortIndicator(column, order);
    rebuildRows();
}

void RecordingsWindow::rebuildRows()
{
    const QStringList keep = selectedIds();
    m_rebuilding = true;
    const QList<RecordingEntry> rows = visibleEntries();
    m_table->clearContents();
    m_table->setRowCount(static_cast<int>(rows.size()));
    for (int r = 0; r < rows.size(); ++r) {
        const RecordingEntry& e = rows.at(r);
        auto* play = new QToolButton(m_table);
        play->setObjectName(QStringLiteral("RecordingsRowPlay"));
        play->setProperty("recordingId", e.id);
        play->setFocusPolicy(Qt::NoFocus);
        if (e.kind == RecordingKind::WholePan) {
            // A whole-pan stretch opens on the pan, like rewind.
            const QString why = stretchReason();
            play->setEnabled(why.isEmpty());
            play->setToolTip(why.isEmpty() ? openOnPanTip() : why);
            play->setAccessibleDescription(why);
        } else {
            play->setToolTip(QStringLiteral("Play on your speaker"));
        }
        connect(play, &QToolButton::clicked, this, [this, id = e.id]() { togglePlay(id); });
        auto* holder = new QWidget(m_table);
        auto* hl = new QHBoxLayout(holder);
        hl->setContentsMargins(5, 0, 0, 0);
        hl->addWidget(play);
        hl->addStretch(1);
        auto* playItem = cell(QString());
        m_table->setItem(r, ColPlay, playItem);
        m_table->setCellWidget(r, ColPlay, holder);

        auto* when = cell(whenText(e.startUtc));
        when->setData(Qt::UserRole, e.id);
        m_table->setItem(r, ColWhen, when);
        auto* kind = cell(kindText(e.kind));
        kind->setForeground(QColor::fromString(kindTagColour(e.kind, false)));
        m_table->setItem(r, ColKind, kind);
        m_table->setItem(r, ColFrequency, cell(e.kind == RecordingKind::WholePan ? panRangeText(e)
                                                                                : recordingFrequencyText(e.frequencyHz)));
        m_table->setItem(r, ColMode, cell(e.mode.isEmpty() ? dimDash() : e.mode));
        auto* call = cell(e.call.isEmpty() ? dimDash() : e.call);
        if (e.call.isEmpty()) {
            call->setForeground(QColor::fromString(QStringLiteral("#6c6c72")));
        }
        m_table->setItem(r, ColCall, call);
        auto* name = cell(e.name);
        name->setToolTip(e.name);
        m_table->setItem(r, ColName, name);
        m_table->setItem(r, ColLength, cell(FlagRecordPlay::lengthText(e.lengthMs), Qt::AlignRight | Qt::AlignVCenter));
        m_table->setItem(r, ColSize, cell(sizeText(e.sizeBytes), Qt::AlignRight | Qt::AlignVCenter));
    }
    m_rebuilding = false;
    setSelectedIds(keep);
    refreshPlayback();
}

void RecordingsWindow::refreshTotals()
{
    const QString why = unavailableReason();
    if (!why.isEmpty() || !m_access) {
        m_totals->setText(QStringLiteral("<b>Unknown</b> while recordings cannot be reached"));
        m_totalsDetail->setText(QStringLiteral("The Digital Spectrum Recorder's loop is separate and not counted "
                                               "here."));
        return;
    }
    const QJsonObject document = m_access->document();
    const QJsonObject index = document.value(QStringLiteral("index")).toObject();
    const qint64 count = index.value(QStringLiteral("count")).toInteger(m_entries.size());
    const qint64 used = index.value(QStringLiteral("usedBytes")).toInteger();
    qint64 pan = 0;
    qint64 sound = 0;
    for (const RecordingEntry& e : m_entries) {
        (e.kind == RecordingKind::WholePan ? pan : sound) += e.sizeBytes;
    }
    m_totals->setText(QStringLiteral("<b>%1</b> saved in %2")
                          .arg(sizeText(used),
                               count == 1 ? QStringLiteral("one recording")
                                          : QStringLiteral("%1 recordings").arg(count)));
    QString detail = QStringLiteral("Whole pan %1, sound %2.").arg(sizeText(pan), sizeText(sound));
    const QString free = freeSpaceText(document);
    if (!free.isEmpty()) {
        detail += QLatin1Char(' ') + free + QLatin1Char('.');
    }
    detail += QStringLiteral(" The Digital Spectrum Recorder's loop is separate and not counted here.");
    m_totalsDetail->setText(detail);
}

void RecordingsWindow::refreshButtons()
{
    const QList<RecordingEntry> chosen = selectedEntries();
    const bool ready = unavailableReason().isEmpty();
    const bool one = ready && chosen.size() == 1;
    const bool any = ready && !chosen.isEmpty();
    const bool wholePan = one && chosen.first().kind == RecordingKind::WholePan;
    const bool here = m_access && m_access->coreIsOnThisComputer();
    const auto reason = [](QPushButton* b, bool enabled, const QString& tip, const QString& why) {
        b->setEnabled(enabled);
        b->setToolTip(enabled || why.isEmpty() ? tip : why);
        b->setAccessibleDescription(enabled ? QString() : why);
    };

    const bool playingThis = one && playingId(m_access) == chosen.first().id;
    m_play->setText(playingThis ? QStringLiteral("Stop") : QStringLiteral("Play"));
    const QString stretchWhy = wholePan ? stretchReason() : QString();
    reason(m_play, one && stretchWhy.isEmpty(),
           wholePan ? openOnPanTip()
                    : QStringLiteral("Play on this computer's speaker. Transmit while it plays to send it on the air."),
           stretchWhy);
    reason(m_rename, one, QStringLiteral("Give this recording a name of your own"), QString());
    reason(m_soundOnly, wholePan && stretchWhy.isEmpty(),
           QStringLiteral("Save its sound as a recording of its own"),
           wholePan ? stretchWhy : QStringLiteral("Pick one whole-pan recording to save its sound."));
    reason(m_copy, any, QStringLiteral("Copy from the Core to this computer"), QString());
    reason(m_reveal, one && here, QStringLiteral("Show the recording's file in its folder"),
           here ? QString() : coreElsewhereReason());
    reason(m_delete, any, QStringLiteral("Delete from the Core, after you confirm"), QString());
    m_delete->setText(chosen.size() > 1 ? QStringLiteral("Delete %1…").arg(chosen.size())
                                        : QStringLiteral("Delete…"));
}

void RecordingsWindow::refreshPlayback()
{
    const std::optional<PlaybackState> playback = m_access ? m_access->playback() : std::nullopt;
    const QString playing = playback.has_value() && playback->playing ? playback->recordingId : QString();
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem* when = m_table->item(row, ColWhen);
        if (when == nullptr) {
            continue;
        }
        const QString id = when->data(Qt::UserRole).toString();
        const bool on = !playing.isEmpty() && id == playing;
        if (QWidget* holder = m_table->cellWidget(row, ColPlay)) {
            if (auto* play = holder->findChild<QToolButton*>(QStringLiteral("RecordingsRowPlay"))) {
                play->setText(on ? QStringLiteral("■") : QStringLiteral("▶"));
                if (play->property("playing").toBool() != on) {
                    play->setProperty("playing", on);
                    play->style()->unpolish(play);
                    play->style()->polish(play);
                }
                const std::optional<RecordingEntry> row = entryById(id);
                const bool wholePan = row.has_value() && row->kind == RecordingKind::WholePan;
                if (play->isEnabled()) {
                    play->setToolTip(on ? QStringLiteral("Stop")
                                     : wholePan ? openOnPanTip() : QStringLiteral("Play on your speaker"));
                }
            }
        }
        if (QTableWidgetItem* length = m_table->item(row, ColLength)) {
            const std::optional<RecordingEntry> e = entryById(id);
            const QString total = FlagRecordPlay::lengthText(e.has_value() ? e->lengthMs : 0);
            length->setText(on ? QStringLiteral("%1 / %2").arg(FlagRecordPlay::lengthText(playback->positionMs), total)
                               : total);
        }
    }
    if (playing.isEmpty()) {
        m_positionTimer->stop();
    } else if (!m_positionTimer->isActive()) {
        m_positionTimer->start();
    }
    if (!m_rebuilding) {
        refreshButtons();
    }
}

void RecordingsWindow::togglePlay(const QString& id)
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
    if (playingId(m_access) == id) {
        m_access->stopPlay();
    } else {
        m_access->play(id);
    }
    refreshPlayback();
}

void RecordingsWindow::playSelected()
{
    const QList<RecordingEntry> chosen = selectedEntries();
    if (chosen.size() == 1) {
        togglePlay(chosen.first().id);
    }
}

void RecordingsWindow::renameSelected()
{
    const QList<RecordingEntry> chosen = selectedEntries();
    if (chosen.size() != 1 || !m_access || !m_renamePrompt) {
        return;
    }
    const RecordingEntry e = chosen.first();
    QPointer<RecordingsWindow> self(this);
    const std::optional<QString> name = m_renamePrompt(e.name);
    if (!self || !name.has_value() || !m_access) {
        return;
    }
    const QString trimmed = name->trimmed();
    if (trimmed.isEmpty() || trimmed == e.name) {
        return;
    }
    QString why;
    if (!isValidRecordingName(trimmed, &why)) {
        m_status->setText(why);
        return;
    }
    m_access->rename(e.id, trimmed);
}

void RecordingsWindow::saveCopiesOfSelected()
{
    const QList<RecordingEntry> chosen = selectedEntries();
    if (chosen.isEmpty() || !m_access || !m_fileChooser) {
        return;
    }
    const QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const auto fileOf = [](const RecordingEntry& e) { return e.fileName.isEmpty() ? e.name : e.fileName; };
    QPointer<RecordingsWindow> self(this);
    const bool several = chosen.size() > 1;
    const QString picked =
        m_fileChooser(several ? downloads : QDir(downloads).filePath(fileOf(chosen.first())), several);
    if (!self || picked.isEmpty()) {
        return;
    }
    for (const RecordingEntry& e : chosen) {
        m_copyQueue.append({e.id, several ? QDir(picked).filePath(fileOf(e)) : picked, entryTitle(e)});
    }
    if (m_copying.has_value()) {
        m_copyCount += static_cast<int>(chosen.size());
        return;
    }
    m_copyNumber = 0;
    m_copyCount = static_cast<int>(m_copyQueue.size());
    startNextCopy();
}

void RecordingsWindow::startNextCopy()
{
    if (m_copyQueue.isEmpty() || !m_access) {
        m_copying.reset();
        m_copyProgress->hide();
        return;
    }
    m_copying = m_copyQueue.takeFirst();
    ++m_copyNumber;
    m_copyProgress->setValue(0);
    m_copyProgress->show();
    m_status->setText(m_copyCount > 1 ? QStringLiteral("Copying %1 of %2: %3").arg(m_copyNumber).arg(m_copyCount).arg(m_copying->title)
                                      : QStringLiteral("Copying %1").arg(m_copying->title));
    m_access->download(m_copying->id, m_copying->toPath);
}

void RecordingsWindow::onDownloadProgress(const QString& id, qint64 done, qint64 total)
{
    if (!m_copying.has_value() || m_copying->id != id || total <= 0) {
        return;
    }
    m_copyProgress->setValue(static_cast<int>(std::clamp<qint64>(done * kProgressSteps / total, 0, kProgressSteps)));
}

void RecordingsWindow::onDownloadFinished(const QString& id, bool ok, const QString& error)
{
    if (!m_copying.has_value() || m_copying->id != id) {
        return;
    }
    const Copy finished = *m_copying;
    m_copying.reset();
    if (ok) {
        m_copyProgress->setValue(kProgressSteps);
        m_status->setText(QStringLiteral("Saved a copy of %1 at %2")
                              .arg(finished.title, QDir::toNativeSeparators(finished.toPath)));
    } else {
        m_status->setText(QStringLiteral("%1 was not copied: %2").arg(finished.title, error));
    }
    if (m_copyQueue.isEmpty()) {
        m_copyProgress->hide();
        return;
    }
    startNextCopy();
}

void RecordingsWindow::showSelectedInFinder()
{
    const QList<RecordingEntry> chosen = selectedEntries();
    if (chosen.size() != 1 || !m_access || !m_access->coreIsOnThisComputer() || !m_revealFile) {
        return;
    }
    const QString folder = QDir::fromNativeSeparators(
        m_access->document().value(QStringLiteral("index")).toObject().value(QStringLiteral("folder")).toString());
    if (folder.isEmpty()) {
        return;
    }
    m_revealFile(QDir::toNativeSeparators(QDir(folder).filePath(chosen.first().fileName)));
}

void RecordingsWindow::deleteSelected()
{
    const QList<RecordingEntry> chosen = selectedEntries();
    if (chosen.isEmpty() || !m_access || !m_confirm) {
        return;
    }
    QPointer<RecordingsWindow> self(this);
    ScopedChildWidget<RecordingsDeleteConfirm> confirm(chosen, this);
    const bool yes = m_confirm(*confirm.get());
    if (!self || !yes || !m_access) {
        return;
    }
    QStringList ids;
    for (const RecordingEntry& e : chosen) {
        ids.append(e.id);
    }
    m_access->remove(ids);
}

// ---- Saved whole-pan stretches (recording plan Task 25) -----------------

QString RecordingsWindow::panRangeText(const RecordingEntry& entry)
{
    if (entry.spanHz <= 0) {
        return recordingFrequencyText(entry.frequencyHz);
    }
    constexpr double kHzPerMhz = 1.0e6;
    const double low = (double(entry.centreHz) - double(entry.spanHz) / 2.0) / kHzPerMhz;
    const double high = (double(entry.centreHz) + double(entry.spanHz) / 2.0) / kHzPerMhz;
    return QStringLiteral("%1 to %2").arg(low, 0, 'f', 3).arg(high, 0, 'f', 3);
}

QString RecordingsWindow::stretchReason() const
{
    const QString why = unavailableReason();
    if (!why.isEmpty()) {
        return why;
    }
    return m_access ? m_access->unavailableReason(RecordingsAccess::kDsrVersion)
                    : RecordingsAccess::notConnectedReason();
}

void RecordingsWindow::saveSoundOfSelected()
{
    const QList<RecordingEntry> chosen = selectedEntries();
    if (!m_access || chosen.size() != 1 || chosen.first().kind != RecordingKind::WholePan) {
        return;
    }
    const QString why = stretchReason();
    if (!why.isEmpty()) {
        m_status->setText(why);
        return;
    }
    m_access->saveSoundOnly(chosen.first().id);
}

void RecordingsWindow::playSoundOnly(const QString& id)
{
    if (!m_access || !stretchReason().isEmpty()) {
        return;
    }
    if (playingId(m_access) == id) {
        m_access->stopPlay();
    } else {
        m_access->play(id);
    }
    refreshPlayback();
}

void RecordingsWindow::showRowMenu(const QPoint& pos)
{
    const int row = m_table->rowAt(pos.y());
    QTableWidgetItem* when = row >= 0 ? m_table->item(row, ColWhen) : nullptr;
    if (when == nullptr) {
        return;
    }
    const QString id = when->data(Qt::UserRole).toString();
    const std::optional<RecordingEntry> e = entryById(id);
    if (!e.has_value() || e->kind != RecordingKind::WholePan) {
        return;
    }
    const QString why = stretchReason();
    QMenu menu(this);
    QAction* open = menu.addAction(QStringLiteral("Open on the pan"));
    QAction* sound = menu.addAction(playingId(m_access) == id ? QStringLiteral("Stop its sound")
                                                              : QStringLiteral("Play its sound only"));
    for (QAction* a : {open, sound}) {
        a->setEnabled(why.isEmpty());
        a->setToolTip(why);
    }
    menu.setToolTipsVisible(true);
    QAction* chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));
    if (chosen == open) {
        togglePlay(id);
    } else if (chosen == sound) {
        playSoundOnly(id);
    }
}

}  // namespace NereusSDR
