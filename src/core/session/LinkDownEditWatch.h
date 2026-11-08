#pragma once
// =================================================================
// src/core/session/LinkDownEditWatch.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Settings persistence audit, row W11.
//
// What a window on a Core changed on its applets and slices while the link
// was down. Nothing is queued: the edit never reaches the Core and the next
// snapshot replaces it. This only remembers it so that the window can say it
// did not stick, the way SettingsProxy::offlineEditsSuperseded() already does
// for Setup values.
//
// StationClient starts the watch when a session that reached ready ends and
// a redial is due, pauses it when the first frame of the next session
// arrives, and asks takeSuperseded() at that session's snapshot-complete
// marker. A window that never reached ready, or whose operator disconnected,
// is not on a Core and has no watch running.
//
// Telling an edit from a setter that ran from code is the point of this
// class. A model setter looks the same from either, so three things have to
// agree before an edit is counted:
//
//   - The property is one the Core takes from a window
//     (MirrorPolicy::inboundAllowed). Readings, and MOX and TUNE, which have
//     their own handling, are Outbound and never count.
//   - It changed with operator input close behind it. An event filter on the
//     application notes keyboard, wheel, button, drag and touch input while
//     the watch runs. Teardown, timers, and a snapshot's deferred side
//     effects have none. An edit made by something other than this window's
//     own input (a CAT or TCI client on this computer) goes unsaid; that is
//     the side to err on, since the toast claims the operator did something.
//   - At the snapshot, the Core's value differs from the value edited to.
//     An edit that went back where it started lost nothing.
//
// Filtered by input type alone through QEvent, so this file stays free of
// GUI headers (Rule R1).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-05 - Created for the settings persistence audit. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVariant>

#include "core/NereusCoreExport.h"

namespace NereusSDR {

class StateMirror;
struct MirrorUpdate;

class NEREUS_CORE_EXPORT LinkDownEditWatch : public QObject {
    Q_OBJECT

public:
    /// How long after operator input a change still counts as that input's.
    /// A control that holds its write on a short timer lands inside this.
    static constexpr qint64 kInputReachMs = 500;

    explicit LinkDownEditWatch(QObject* parent = nullptr);
    ~LinkDownEditWatch() override;

    /// Begin watching these retained objects. Edits already recorded stay.
    void start(const QHash<QByteArray, QPointer<QObject>>& objects);
    /// Stop recording; the objects and edits are kept for resume().
    void pause();
    /// A redial that got a first frame and then failed: watch again.
    void resume();
    /// Forget everything: the operator's disconnect, or a permanent end.
    void clear();

    /// True from start() until clear() or takeSuperseded().
    bool isArmed() const { return m_armed; }

    /// Ends the watch. Returns "object.property" for each edit whose object
    /// still exists and now holds something other than the value edited to.
    QStringList takeSuperseded();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void onChanged(const QByteArray& objectKey, const QList<MirrorUpdate>& updates);
    void setRecording(bool recording);
    bool inputIsRecent() const;

    QHash<QByteArray, QPointer<QObject>> m_objects;
    /// objectKey -> property name -> the wire value last edited to.
    QHash<QByteArray, QHash<QByteArray, QVariant>> m_edits;
    StateMirror* m_mirror = nullptr;
    QElapsedTimer m_sinceInput;
    bool m_buttonDown = false;
    bool m_armed = false;
    bool m_recording = false;
};

} // namespace NereusSDR
