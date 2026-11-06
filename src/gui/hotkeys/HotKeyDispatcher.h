// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 2.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-06 - J.J. Boyd (KG4VCF), exported: tests take its signal addresses across
//                the Windows DLL. AI-assisted via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
// The only code that runs a hot key action. Keyboard, MIDI, knobs and deck
// plugins all call it. It finds the slice, applies refusals, limits bursts and
// keeps the hold ledger, then calls HotKeyTargets. Main thread only.
#pragma once

#include "core/hotkeys/HotKeyAction.h"
#include "core/hotkeys/TuneCoalescer.h"
#include "gui/NereusGuiExport.h"
#include "gui/hotkeys/HotKeyTargets.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <functional>

namespace NereusSDR {

class HotKeyBindings;
class SliceModel;

class NEREUS_GUI_EXPORT HotKeyDispatcher : public QObject {
    Q_OBJECT

public:
    HotKeyDispatcher(HotKeyBindings* bindings, HotKeyTargets* targets, QObject* parent = nullptr);

    // A Turn action takes one step per press, ten for tune.up10 and tune.down10.
    void press(const QString& id, const HotKeySource& source, QChar pin = {});
    void release(const QString& id, const HotKeySource& source);
    void turn(const QString& idUp, const QString& idDown, int steps, const HotKeySource& source, QChar pin = {});
    void releaseAll(const HotKeySource& source);    // failsafes, one source
    void releaseAllHolds();                         // the off switch

    // A hold drops when its release can't arrive, and across sleep it can't. While
    // anything is held, a timer compares the wall clock with a clock that stops
    // while the machine sleeps. Wall time gained on it was time asleep.
    //
    // A stall moves both clocks together, so a 2 s hitch on this thread reads as a
    // gap near zero, however long it is. So the threshold only has to clear a hitch
    // landing between the two clock reads, which is why it is above 2 s, and it
    // matches the 3 s the radio links allow for silence. A shorter sleep goes unseen.
    static constexpr qint64 kSleepGapMs = 3000;
    // Costs nothing at rest: it runs only while something is held. A hold still
    // down on wake drops within one tick of waking.
    static constexpr int kSleepTickMs = 1000;

    using ClockMs = std::function<qint64()>;
    void setSleepClocksForTest(ClockMs wallClock, ClockMs awakeClock);
    bool sleepWatchRunning() const { return m_sleepTimer.isActive(); }

public slots:
    void onTransmitWentOff();                       // clears the ledger's claim
    void flushTune();                               // the timer's slot; public for tests
    void checkForSleep();                           // the timer's slot; public for tests

signals:
    // After the holds dropped. Whoever pairs presses with releases forgets its keys.
    void holdsDroppedBySleep();

private:
    // Steps waiting on one slice. Tune and levels limit separately, so a volume
    // knob can't hold back a tuning knob.
    struct TurnQueue {
        QPointer<SliceModel> slice;
        bool isTune{false};
        TuneCoalescer coalescer;
        double inFlightHz{0.0};
        qint64 inFlightAtMs{0};
        bool hasInFlight{false};
    };

    struct CwKeyHold {
        QString id;
        HotKeySource source;
    };

    bool admit(const HotKeyAction& action, QChar pin, SliceModel*& slice);
    void execute(const HotKeyAction& action, SliceModel* slice);
    void addTurnSteps(const HotKeyAction& action, SliceModel* slice, int steps);
    void sendTurnSteps(TurnQueue& queue, int steps);
    void sendTune(TurnQueue& queue, int steps);
    void dropTune(const SliceModel* slice);
    void dropAllTurns();
    bool tuneStillAllowed(const SliceModel* slice) const;
    void armFlushTimer();

    void startHold(const HotKeyAction& action, const HotKeySource& source);
    void endTransmitHold(const HotKeySource& source);
    void endCwKey(const QString& id, const HotKeySource& source);
    void updateSleepWatch();
    void noteSleepClocks();

    HotKeyBindings* m_bindings;
    HotKeyTargets* m_targets;
    QTimer m_flushTimer;
    QList<TurnQueue> m_turnQueues;

    // The hold ledger. m_holdKeyed: a hold keyed the radio and the key has not
    // gone off since, so the last release may unkey. Holding without that claim
    // (a latched MOX, say) leaves the key alone.
    QList<HotKeySource> m_holdSources;
    bool m_holdKeyed{false};
    QList<CwKeyHold> m_cwKeysDown;

    QTimer m_sleepTimer;
    ClockMs m_wallClock;
    ClockMs m_awakeClock;
    qint64 m_lastWallMs{0};
    qint64 m_lastAwakeMs{0};
};

} // namespace NereusSDR
