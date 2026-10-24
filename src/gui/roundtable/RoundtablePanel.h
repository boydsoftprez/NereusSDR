#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/roundtable/RoundtablePanel.h  (NereusSDR)
// =================================================================
//
// One Roundtable room as the desktop shows it (design 11.1, 11.2 Panel;
// mockup docs/architecture/reporter-brainstorm/mockups/placement.html):
// the room header and its connection line, the talker, the readings
// (Station, km, Offset, Reading, Age; nearest first), the earlier turns of
// the last 30 minutes (Name on unknown turns), the messages and the box.
// One scroll with the box pinned to its foot (U11).
//
// It follows a slice (the room RoundtableModel::sliceRoomId names) unless
// it reads another room (readRoom: a bar "Reading 7.249 · Tune A here ·
// Back to my slice"). Only Tune A here tunes (U1): the panel asks with
// tuneRequested and its owner tunes the slice.
//
// While you transmit, readings of you take the room (U3): one TX line
// ("You · 9 hear you"), then every reading of you; earlier turns and
// messages wait below.
//
// The same panel sits in the right-column applet (RoundtableApplet) and in
// the pop-out window (RoundtableWindow). setSplit() moves the messages and
// the box into a right-hand column for the window's widest shape;
// setPopped() makes Escape step back through the window instead of
// leaving the panel.
//
// Every time it reads comes from RoundtableModel::nowMs(), so a test's
// clock drives the ages, the stale rows and the 2-second marks.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 13).
// =================================================================

#include "gui/NereusGuiExport.h"

#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QWidget>

class QFrame;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QToolButton;
class QVBoxLayout;

namespace NereusSDR {

class RoundtableModel;

class NEREUS_GUI_EXPORT RoundtablePanel : public QWidget {
    Q_OBJECT

public:
    /// Design 11.1: a reading turns grey past 20 s.
    static constexpr qint64 kStaleReadingMs = 20000;
    /// Design 11.1 (U6, U11): a mention clears, and a new message loses
    /// its mark, once on screen for 2 seconds.
    static constexpr qint64 kOnScreenMs = 2000;
    /// Design 11.1: messages of 500 code points, the counter from 400.
    static constexpr int kMessageLimit = 500;
    static constexpr int kCounterFrom = 400;
    /// Wire spec 5.3: up to 10 lines.
    static constexpr int kMessageLines = 10;
    /// Design 11.2: the messages column of the widest window.
    static constexpr int kMessagesColumnWidth = 320;

    /// `model` is not owned.
    explicit RoundtablePanel(RoundtableModel* model, QWidget* parent = nullptr);
    ~RoundtablePanel() override;

    /// Shows the room this slice is in (or the empty state) and stops
    /// reading any other room.
    void followSlice(int sliceId);
    int followedSlice() const { return m_sliceId; }
    /// The followed slice's frequency in Hz, for the empty state's header
    /// and its list of rooms on the same band.
    void setSliceFrequency(qint64 hz);
    /// Reads `roomId` without tuning (U1); the slice's own room is the same
    /// as backToSlice().
    void readRoom(const QString& roomId);
    /// Back to following the slice.
    void backToSlice();

    /// The room shown (the read one, else the slice's), or empty.
    QString roomId() const;
    /// Reading a room other than the slice's.
    bool isReading() const { return !m_readId.isEmpty(); }

    /// Messages and the box in a right-hand column (the window's widest
    /// shape), or under the room in the one scroll.
    void setSplit(bool split);
    bool isSplit() const { return m_split; }
    /// In its own window: no pop-out arrow, and Escape asks the window to
    /// step back (stepBackRequested) instead of leaving the panel.
    void setPopped(bool popped);
    bool isPopped() const { return m_popped; }

    /// Moves the keyboard to the first earlier turn or message (Enter from
    /// the window's room list).
    void focusFirstItem();
    /// Re-reads the clock: ages, stale rows, the keyed time, the connection
    /// line and the 2-second marks. Runs every second on its own.
    void refreshNow();

signals:
    /// Tune A here: tune the slice to `hz` in the room's `family`.
    void tuneRequested(qint64 hz, const QString& family);
    void popOutRequested();
    void archiveRequested();
    void settingsRequested();
    /// Escape while popped out.
    void stepBackRequested();
    /// The menu's Wide window while popped out (the window's Wide preset).
    void widePresetRequested();
    /// The room shown changed (a room read, or back to the slice).
    void shownRoomChanged(const QString& roomId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct Pending {
        QString roomId;
        QString text;
    };

    void onRoomChanged(const QString& roomId);
    void rebuild();
    void rebuildHead(const QJsonObject& room);
    void fitCount();
    void rebuildTalker(const QJsonObject& room);
    void rebuildReadings(const QJsonObject& room);
    void rebuildEarlier(const QJsonObject& room);
    void rebuildMessages(const QJsonObject& room);
    void rebuildEmpty();
    void updateConnectionLine(const QJsonObject& room);
    void updateFooter();
    void updatePills();
    void checkOnScreen();
    void placeMessages();
    void scrollMessagesToEnd();
    bool messagesAtEnd() const;
    QScrollArea* messagesScroll() const;

    void send();
    void onPostResult(const QString& clientId, bool ok, const QString& idOrReason);
    void openNameEditor(QWidget* row, qint64 turnId);
    void submitName(qint64 turnId, const QString& name);
    void showMessageMenu(QWidget* anchor, const QJsonObject& message);
    void showReadingMenu(QWidget* row, const QJsonObject& member);
    void showTurnMenu(QWidget* row, const QJsonObject& turn);
    void showPanelMenu();
    void jumpToMention();
    void escapePressed();
    void moveFocus(int step);
    void leavePanel();

    QString myCallsign(const QJsonObject& room) const;
    QString myRef(const QJsonObject& room) const;
    bool mineMessage(const QJsonObject& room, const QJsonObject& message) const;
    bool names(const QJsonObject& message, const QString& callsign) const;
    bool tunedHere() const;
    bool live() const;
    bool awayHere(const QJsonObject& room) const;
    QJsonObject openTurn(const QJsonObject& room) const;
    QStringList nameSuggestions(const QJsonObject& room) const;
    QList<qint64> unreadMentionIds(const QJsonObject& room) const;
    QJsonObject summaryOf(const QString& roomId) const;

    QPointer<RoundtableModel> m_model;
    int m_sliceId = -1;
    qint64 m_sliceHz = 0;
    QString m_readId;
    QString m_shownId;
    bool m_split = false;
    bool m_popped = false;

    // Per room: the message ids already in view when the room first
    // showed, the ones that arrived since (blue edge, "new"), and when each
    // marked one came on screen.
    QSet<qint64> m_known;
    QSet<qint64> m_new;
    QSet<qint64> m_everOnScreen;
    QHash<qint64, qint64> m_onScreenSince;
    QSet<qint64> m_mentionsMarked;
    QSet<qint64> m_showMuted;
    QSet<qint64> m_myMessages;
    QHash<QString, Pending> m_pending;
    QString m_notSentReason;
    QString m_earlierSignature;
    QString m_messagesSignature;
    bool m_firstShow = true;
    qint64 m_nameEditorTurn = 0;

    // Layout.
    QScrollArea* m_scroll = nullptr;
    QWidget* m_content = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
    QWidget* m_readBar = nullptr;
    QLabel* m_readLabel = nullptr;
    QPointer<QLabel> m_countLabel;
    QString m_countOneLine;
    QString m_countTwoLines;
    QWidget* m_head = nullptr;
    QLabel* m_connection = nullptr;
    QWidget* m_txLine = nullptr;
    QWidget* m_talker = nullptr;
    QWidget* m_readings = nullptr;
    QWidget* m_earlier = nullptr;
    QWidget* m_messages = nullptr;
    QWidget* m_empty = nullptr;
    QWidget* m_msgColumn = nullptr;
    QScrollArea* m_msgScroll = nullptr;
    QWidget* m_footer = nullptr;
    QPushButton* m_mentionPill = nullptr;
    QPushButton* m_newPill = nullptr;
    QWidget* m_notSent = nullptr;
    QPlainTextEdit* m_box = nullptr;
    QPushButton* m_send = nullptr;
    QLabel* m_counter = nullptr;
    QLabel* m_hint = nullptr;
    QToolButton* m_popButton = nullptr;
    QList<QPointer<QWidget>> m_navItems;
    QHash<qint64, QPointer<QWidget>> m_messageRows;

    QTimer m_tick;
    bool m_settingBox = false;
};

} // namespace NereusSDR
