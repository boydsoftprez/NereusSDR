#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/recordings/RecordingsPanel.h  (NereusSDR)
// =================================================================
//
// The small Recordings panel: a right-column applet (Containers > Applets,
// id "Recordings") for quick listening, fed by RecordingsAccess. Design:
// docs/architecture/2026-11-15-recording-voice-keyer-dsr-design.md,
// sections 3 (Recordings panel) and 6.4; mockup recordings-panel/.
//
//   Rows      the six latest recordings, newest first, each with its play
//             button (a click plays it on this computer's speaker, again
//             stops it; a double click on the row plays it too), its name,
//             kind and time, and its length. While a slice records, a red
//             row for it sits at the top with a stop button and the
//             running time.
//   Menu      right-click a row: Play (or Stop), Rename... (in place),
//             Save a copy here..., Show in Finder (the Core on this computer
//             only), Delete... (after a confirm that says it cannot be
//             undone).
//   Footer    All recordings... (the Recordings window) and how many are on
//             the Core, their size and the free space.
//
// No settings of its own. A whole-pan stretch opens on the pan once the
// Digital Spectrum Recorder is built; until then its play is disabled with
// that reason. Not usable (not connected, or a Core without recordings):
// the panel stays, dimmed, with the access's reason at its top. Playing
// never keys the radio.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 7.
// =================================================================

#include <QDateTime>
#include <QList>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

#include "core/recordings/RecordingNames.h"
#include "gui/NereusGuiExport.h"
#include "gui/applets/AppletWidget.h"

class QFrame;
class QLabel;
class QLineEdit;
class QMenu;
class QProgressBar;
class QPushButton;
class QTimer;
class QVBoxLayout;

namespace NereusSDR {

class RecordingsAccess;
class RecordingsDeleteConfirm;

class NEREUS_GUI_EXPORT RecordingsPanel : public AppletWidget {
    Q_OBJECT

public:
    /// Rows shown, a recording slice's red row among them.
    static constexpr int kRows = 6;
    /// How often the playing row's position is read while it plays.
    static constexpr int kPositionRefreshMs = 250;

    RecordingsPanel(RadioModel* model, RecordingsAccess* access, QWidget* parent = nullptr);
    ~RecordingsPanel() override;

    QString appletId() const override { return QStringLiteral("recordings"); }
    QString appletTitle() const override { return QStringLiteral("Recordings"); }
    void syncFromModel() override;

    void setAccess(RecordingsAccess* access);
    RecordingsAccess* access() const;
    /// The words for a recording slice's red row, "7.185.000 LSB"; none
    /// unless set.
    void setSliceTextSource(std::function<QString(int)> source) { m_sliceText = std::move(source); }

    /// The rows as shown, top to bottom: a recording's id, or "slice:N" for
    /// slice N's red row.
    QStringList rowIds() const;
    /// The recordings the panel has from the Core, newest first.
    QList<RecordingEntry> entries() const { return m_entries; }

    /// Fills `menu` with a row's right-click menu.
    void populateRowMenu(QMenu* menu, const QString& id);
    /// Rename in place: the row's name becomes an editor.
    void startRename(const QString& id);

    // ---- Test seams.
    QFrame* rowForTest(const QString& id) const;
    QPushButton* rowPlayButtonForTest(const QString& id) const;
    QLabel* rowTitleForTest(const QString& id) const;
    QLabel* rowLengthForTest(const QString& id) const;
    QProgressBar* rowProgressForTest(const QString& id) const;
    QLineEdit* renameEditorForTest() const { return m_editor; }
    QPushButton* allButtonForTest() const { return m_all; }
    QLabel* usageForTest() const { return m_usage; }
    QLabel* noteForTest() const { return m_note; }
    /// Replaces the file dialog: the path for the copy, empty cancels.
    void setFileChooserForTest(std::function<QString(const QString&)> chooser) { m_fileChooser = std::move(chooser); }
    /// Replaces exec() of the delete confirm: true to delete.
    void setConfirmForTest(std::function<bool(RecordingsDeleteConfirm&)> confirm) { m_confirm = std::move(confirm); }
    /// Replaces RevealFile::reveal (a test must not open Finder).
    void setRevealForTest(std::function<bool(const QString&)> reveal) { m_reveal = std::move(reveal); }
    /// Replaces the clock "today" and "yesterday" use.
    void setClockForTest(std::function<QDateTime()> now) { m_now = std::move(now); }

signals:
    /// All recordings... was pressed, with the recording to select (empty
    /// for none).
    void allRecordingsRequested(const QString& recordingId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Row {
        QString id;
        QPointer<QFrame> frame;
        QPointer<QPushButton> play;
        QPointer<QLabel> title;
        QPointer<QLabel> length;
        QPointer<QProgressBar> progress;
    };

    void buildUi();
    void onDocumentChanged();
    void onListChanged(int reader);
    void requestListIfStale();
    void rebuildRows();
    void refreshPlayback();
    void refreshFooter();
    void togglePlay(const QString& id);
    void saveCopy(const QString& id);
    void reveal(const QString& id);
    void confirmDelete(const QString& id);
    void finishRename(bool keep);
    void showRowMenu(const QString& id, const QPoint& globalPos);
    std::optional<RecordingEntry> entryById(const QString& id) const;
    const Row* rowById(const QString& id) const;
    QString unavailableReason() const;
    QString dayText(const QDateTime& startUtc) const;

    QPointer<RecordingsAccess> m_access;
    QList<QMetaObject::Connection> m_accessConnections;
    int m_listReader = 0;
    qint64 m_listRevision = -1;
    QList<RecordingEntry> m_entries;
    QList<Row> m_rows;
    QSet<QString> m_copies;
    QString m_copyStatus;

    QLabel* m_note = nullptr;
    QWidget* m_listBox = nullptr;
    QVBoxLayout* m_list = nullptr;
    QPushButton* m_all = nullptr;
    QLabel* m_usage = nullptr;
    QTimer* m_positionTimer = nullptr;
    QString m_editingId;
    QPointer<QLineEdit> m_editor;

    std::function<QString(int)> m_sliceText;
    std::function<QString(const QString&)> m_fileChooser;
    std::function<bool(RecordingsDeleteConfirm&)> m_confirm;
    std::function<bool(const QString&)> m_reveal;
    std::function<QDateTime()> m_now;
};

}  // namespace NereusSDR
