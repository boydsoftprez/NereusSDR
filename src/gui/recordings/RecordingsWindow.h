#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/recordings/RecordingsWindow.h  (NereusSDR)
// =================================================================
//
// The Recordings window, opened from All recordings... on a flag or in the
// Recordings panel: every recording kept on the Core in one list, fed by
// RecordingsAccess so it is the same with the Core here or across the link.
// Design: docs/architecture/2026-11-15-recording-voice-keyer-dsr-design.md,
// sections 3 (Recordings window) and 6.4; mockup recordings-window/.
//
//   On the Core   what is saved (count and size), whole pan against sound,
//                 the free space on the recordings disk; the Digital
//                 Spectrum Recorder's loop is not counted.
//   Filter        All, Contact, Whole pan, Sound; search over name and call.
//   Table         play, when (UTC), kind, frequency, mode, call, name,
//                 length, size; a click on a column sorts by it, again
//                 turns it round. Click, Shift and Cmd select; a double
//                 click plays.
//   Buttons       Play, Rename..., Save sound only... (whole pan only, and
//                 not until the Digital Spectrum Recorder is built), Save a
//                 copy here... (a file dialog, then the copy with its
//                 progress), Show in Finder (the Core on this computer only),
//                 Delete... (several at once, after a confirm that says it
//                 cannot be undone).
//
// Playback is this computer's own and never keys the radio. Not usable (not
// connected, or a Core without recordings): everything stays in place,
// dimmed, with the access's reason at the top.
//
// The window pages the whole list from the Core with a list reader of its
// own and asks again whenever the Core's index moves (a recording kept,
// renamed or deleted from any device).
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

#include <QDateTime>
#include <QDialog>
#include <QJsonObject>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <functional>
#include <optional>

#include "core/recordings/RecordingNames.h"
#include "gui/NereusGuiExport.h"

class QButtonGroup;
class QGroupBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QTimer;

namespace NereusSDR {

class RecordingsAccess;

/// "Delete this recording from the Core?" with each recording, the space it
/// frees and that it cannot be undone; Cancel and Delete.
class NEREUS_GUI_EXPORT RecordingsDeleteConfirm : public QDialog {
    Q_OBJECT

public:
    RecordingsDeleteConfirm(const QList<RecordingEntry>& entries, QWidget* parent = nullptr);

    /// The words of the confirm, as shown.
    QString text() const;
    QPushButton* deleteButton() const { return m_delete; }
    QPushButton* cancelButton() const { return m_cancel; }

private:
    QLabel* m_text = nullptr;
    QPushButton* m_delete = nullptr;
    QPushButton* m_cancel = nullptr;
};

class NEREUS_GUI_EXPORT RecordingsWindow : public QWidget {
    Q_OBJECT

public:
    enum class Filter { All, Contact, WholePan, Sound };
    enum Column { ColPlay, ColWhen, ColKind, ColFrequency, ColMode, ColCall, ColName, ColLength, ColSize,
                  ColCount };

    /// Recordings asked for in one page (the Core sends at most 100).
    static constexpr int kPageSize = 100;
    /// How often the playing row's position is read while it plays.
    static constexpr int kPositionRefreshMs = 250;
    static constexpr int kDefaultWidth = 900;
    static constexpr int kDefaultHeight = 560;

    /// Opens the window for `access`: the one already open under `parent`,
    /// or a new one. Shown, raised and returned.
    static RecordingsWindow* showFor(RecordingsAccess* access, QWidget* parent);

    explicit RecordingsWindow(RecordingsAccess* access, QWidget* parent = nullptr);
    ~RecordingsWindow() override;

    void setAccess(RecordingsAccess* access);
    RecordingsAccess* access() const;

    /// Selects that recording (and shows it) once it is in the list; empty
    /// clears nothing.
    void selectRecording(const QString& id);

    void setFilter(Filter filter);
    Filter filter() const { return m_filter; }
    void setSearchText(const QString& text);
    /// Sorts by `column` (not ColPlay's button but whether it plays).
    void sortBy(Column column, Qt::SortOrder order);
    Column sortColumn() const { return m_sortColumn; }
    Qt::SortOrder sortOrder() const { return m_sortOrder; }

    /// Every recording the window has from the Core, newest first.
    QList<RecordingEntry> entries() const { return m_entries; }
    /// The rows as shown, top to bottom.
    QStringList visibleIds() const;
    QStringList selectedIds() const;
    void setSelectedIds(const QStringList& ids);

    void playSelected();
    void renameSelected();
    void saveCopiesOfSelected();
    void showSelectedInFinder();
    void deleteSelected();
    /// Plays a whole-pan stretch's sound only on the speaker.
    void playSoundOnly(const QString& id);

    // ---- Words shared with the Recordings panel.
    /// "Contact", "Whole pan", "Sound".
    static QString kindText(RecordingKind kind);
    /// "15 MB", "4.6 MB", "1.4 GB".
    static QString sizeText(qint64 bytes);
    /// "14 Nov 15:02", in UTC.
    static QString whenText(const QDateTime& startUtc);
    /// What a row is called: its name when renamed, else frequency, mode
    /// and call.
    static QString entryTitle(const RecordingEntry& entry);
    /// The kind's tag colours (text, background), as the mockups draw them.
    static QString kindTagColour(RecordingKind kind, bool background);
    /// Save sound only and whole-pan play until the recorder exists.
    static QString soundOnlyUnbuiltReason()
    {
        return QStringLiteral("Available when the Digital Spectrum Recorder is built.");
    }
    /// A whole-pan row's frequency column: the pan's range, "7.089 to
    /// 7.281" (MHz), or its frequency when the range is not known.
    static QString panRangeText(const RecordingEntry& entry);
    static QString openOnPanTip() { return QStringLiteral("Open on the pan, like rewind"); }
    static QString coreElsewhereReason()
    {
        return QStringLiteral("The Core is on another computer. Use Save a copy here.");
    }
    /// "<free> free on <disk>" from the document, empty when unknown.
    static QString freeSpaceText(const QJsonObject& document);

    // ---- Test seams.
    QLabel* noteForTest() const { return m_note; }
    QLabel* totalsForTest() const { return m_totals; }
    QLabel* totalsDetailForTest() const { return m_totalsDetail; }
    QTableWidget* tableForTest() const { return m_table; }
    QLineEdit* searchForTest() const { return m_search; }
    QPushButton* filterButtonForTest(Filter filter) const;
    QPushButton* playButtonForTest() const { return m_play; }
    QPushButton* renameButtonForTest() const { return m_rename; }
    QPushButton* soundOnlyButtonForTest() const { return m_soundOnly; }
    QPushButton* copyButtonForTest() const { return m_copy; }
    QPushButton* revealButtonForTest() const { return m_reveal; }
    QPushButton* deleteButtonForTest() const { return m_delete; }
    QProgressBar* copyProgressForTest() const { return m_copyProgress; }
    QLabel* statusForTest() const { return m_status; }
    /// Replaces the rename prompt: the new name, or none to cancel.
    void setRenamePromptForTest(std::function<std::optional<QString>(const QString&)> prompt)
    {
        m_renamePrompt = std::move(prompt);
    }
    /// Replaces the file dialog: the path for one copy (`folder` false,
    /// given the suggested path) or the folder for several; empty cancels.
    void setFileChooserForTest(std::function<QString(const QString&, bool)> chooser)
    {
        m_fileChooser = std::move(chooser);
    }
    /// Replaces exec() of the delete confirm: true to delete.
    void setConfirmForTest(std::function<bool(RecordingsDeleteConfirm&)> confirm) { m_confirm = std::move(confirm); }
    /// Replaces RevealFile::reveal (a test must not open Finder).
    void setRevealForTest(std::function<bool(const QString&)> reveal) { m_revealFile = std::move(reveal); }

private:
    void buildUi();
    void onDocumentChanged();
    void onListChanged(int reader);
    void reload();
    void requestPage(int offset);
    void rebuildRows();
    void refreshTotals();
    void refreshButtons();
    void refreshPlayback();
    void refreshAvailability();
    void togglePlay(const QString& id);
    /// Why a whole-pan stretch cannot be opened or its sound saved, or
    /// empty (an older Core says it needs an update).
    QString stretchReason() const;
    void saveSoundOfSelected();
    void showRowMenu(const QPoint& pos);
    void startNextCopy();
    void onDownloadProgress(const QString& id, qint64 done, qint64 total);
    void onDownloadFinished(const QString& id, bool ok, const QString& error);
    std::optional<RecordingEntry> entryById(const QString& id) const;
    QList<RecordingEntry> selectedEntries() const;
    QList<RecordingEntry> visibleEntries() const;
    QString unavailableReason() const;

    QPointer<RecordingsAccess> m_access;
    QList<QMetaObject::Connection> m_accessConnections;
    int m_listReader = 0;
    qint64 m_listRevision = -1;
    bool m_loadInFlight = false;
    bool m_reloadAgain = false;
    QList<RecordingEntry> m_loading;
    QList<RecordingEntry> m_entries;
    QString m_pendingSelect;

    Filter m_filter = Filter::All;
    QString m_searchText;
    Column m_sortColumn = ColWhen;
    Qt::SortOrder m_sortOrder = Qt::DescendingOrder;

    struct Copy {
        QString id;
        QString toPath;
        QString title;
    };
    QList<Copy> m_copyQueue;
    std::optional<Copy> m_copying;
    int m_copyNumber = 0;
    int m_copyCount = 0;

    QWidget* m_body = nullptr;
    QLabel* m_note = nullptr;
    QGroupBox* m_store = nullptr;
    QLabel* m_totals = nullptr;
    QLabel* m_totalsDetail = nullptr;
    QButtonGroup* m_filterGroup = nullptr;
    QLineEdit* m_search = nullptr;
    QTableWidget* m_table = nullptr;
    QPushButton* m_play = nullptr;
    QPushButton* m_rename = nullptr;
    QPushButton* m_soundOnly = nullptr;
    QPushButton* m_copy = nullptr;
    QPushButton* m_reveal = nullptr;
    QPushButton* m_delete = nullptr;
    QProgressBar* m_copyProgress = nullptr;
    QLabel* m_status = nullptr;
    QTimer* m_positionTimer = nullptr;
    bool m_rebuilding = false;

    std::function<std::optional<QString>(const QString&)> m_renamePrompt;
    std::function<QString(const QString&, bool)> m_fileChooser;
    std::function<bool(RecordingsDeleteConfirm&)> m_confirm;
    std::function<bool(const QString&)> m_revealFile;
};

}  // namespace NereusSDR
