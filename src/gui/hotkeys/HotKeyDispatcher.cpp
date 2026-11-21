// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 2.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-07 - J.J. Boyd (KG4VCF), hot keys: VOX on or off (vox.toggle).
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original.
#include "gui/hotkeys/HotKeyDispatcher.h"

#include "core/LogCategories.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "models/SliceModel.h"

#include <QDateTime>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <utility>

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
#include <time.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace NereusSDR {

namespace {

// How long after a send the next tune still builds on that target rather than
// the slice's own frequency. Long enough for a remote slice to catch up, short
// enough that a drag or a typed frequency soon after is what the next key
// starts from. NereusSDR's choice, not a value from a reference.
constexpr qint64 kInFlightWindowMs = 250;

// A clock that stops while the machine sleeps. Qt's own can't serve: QElapsedTimer
// reads std::chrono::steady_clock, which keeps counting through sleep on macOS
// (libc++ src/chrono.cpp, CLOCK_MONOTONIC_RAW) and on Windows (QueryPerformanceCounter;
// Microsoft, "Acquiring high-resolution time stamps").
qint64 awakeClockMs()
{
#if defined(Q_OS_MAC)
    // man clock_gettime: CLOCK_UPTIME_RAW "does not increment while the system is asleep".
    return static_cast<qint64>(clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 1000000ULL);
#elif defined(Q_OS_WIN)
    // Microsoft, QueryUnbiasedInterruptTime: "does not include time the system spends
    // in sleep or hibernation". Units of 100 ns. MinGW declares it too
    // (mingw-w64 realtimeapiset.h, _WIN32_WINNT >= 0x0601). The page does not mention
    // connected standby.
    ULONGLONG ticks = 0;
    QueryUnbiasedInterruptTime(&ticks);
    return static_cast<qint64>(ticks / 10000ULL);
#elif defined(Q_OS_LINUX)
    // clock_gettime(2): CLOCK_MONOTONIC "does not count time that the system is suspended".
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<qint64>(now.tv_sec) * 1000 + now.tv_nsec / 1000000;
#else
#error "The hot key sleep failsafe needs a clock that stops during sleep on this platform."
#endif
}

const QString& idTxHold()
{
    static const QString kId = QStringLiteral("tx.hold");
    return kId;
}

bool isTuneAction(const QString& id)
{
    return id.startsWith(QLatin1String("tune."));
}

// Signed steps for one use of a Turn action.
int stepsOfTurnAction(const QString& id)
{
    if (id == QLatin1String("tune.up10")) {
        return 10;
    }
    if (id == QLatin1String("tune.down10")) {
        return -10;
    }
    return id.endsWith(QLatin1String(".down")) ? -1 : 1;
}

// Design 7.2: volume and mute change only what this device hears, so they work
// on a listened slice.
bool worksWhenListened(const QString& id)
{
    return id == QLatin1String("volume.up") || id == QLatin1String("volume.down") || id == QLatin1String("mute");
}

// V6, and the plan's Lock case. Lock itself, DSP and audio stay usable.
bool refusedWhenLocked(const QString& id)
{
    return isTuneAction(id) || id.startsWith(QLatin1String("band.")) || id.startsWith(QLatin1String("mode."));
}

} // namespace

HotKeyDispatcher::HotKeyDispatcher(HotKeyBindings* bindings, HotKeyTargets* targets, QObject* parent)
    : QObject(parent)
    , m_bindings(bindings)
    , m_targets(targets)
{
    m_flushTimer.setSingleShot(true);
    m_flushTimer.setTimerType(Qt::PreciseTimer);
    m_flushTimer.setInterval(static_cast<int>(TuneCoalescer::kMinIntervalMs));
    connect(&m_flushTimer, &QTimer::timeout, this, &HotKeyDispatcher::flushTune);

    m_wallClock = [] { return QDateTime::currentMSecsSinceEpoch(); };
    m_awakeClock = [] { return awakeClockMs(); };
    m_sleepTimer.setInterval(kSleepTickMs);
    connect(&m_sleepTimer, &QTimer::timeout, this, &HotKeyDispatcher::checkForSleep);

    connect(m_bindings, &HotKeyBindings::enabledChanged, this, [this](bool on) {
        if (!on) {
            releaseAllHolds();
            dropAllTurns();
        }
    });
}

bool HotKeyDispatcher::enabled() const
{
    return m_bindings->enabled();
}

void HotKeyDispatcher::press(const QString& id, const HotKeySource& source, QChar pin)
{
    if (!m_bindings->enabled()) {
        return;
    }
    const HotKeyAction* const action = HotKeyActionList::find(id);
    if (action == nullptr) {
        qCWarning(lcHotKeys) << "Unknown hot key action:" << id;
        return;
    }
    if (action->gesture == HotKeyGesture::Hold) {
        startHold(*action, source);
        return;
    }
    SliceModel* slice = nullptr;
    if (!admit(*action, pin, slice)) {
        return;
    }
    if (action->gesture == HotKeyGesture::Turn) {
        addTurnSteps(*action, slice, stepsOfTurnAction(id));
    } else {
        execute(*action, slice);
    }
}

void HotKeyDispatcher::release(const QString& id, const HotKeySource& source)
{
    // No off switch check: what is held is in the ledger, and letting go must
    // always work. Turning the switch off has already emptied it.
    if (id == idTxHold()) {
        endTransmitHold(source);
    } else {
        endCwKey(id, source);
    }
}

void HotKeyDispatcher::turn(const QString& idUp, const QString& idDown, int steps,
                            const HotKeySource& /*source*/, QChar pin)
{
    if (steps == 0 || !m_bindings->enabled()) {
        return;
    }
    const QString& id = steps > 0 ? idUp : idDown;
    const HotKeyAction* const action = HotKeyActionList::find(id);
    if (action == nullptr) {
        qCWarning(lcHotKeys) << "Unknown hot key action:" << id;
        return;
    }
    // A hold would have no release to end it, and a detent is not a decision to key.
    if (action->gesture == HotKeyGesture::Hold || action->transmits) {
        qCWarning(lcHotKeys) << "A knob can't run a transmit action:" << id;
        return;
    }
    SliceModel* slice = nullptr;
    if (!admit(*action, pin, slice)) {
        return;
    }
    const int count = std::abs(steps);
    if (action->gesture == HotKeyGesture::Turn) {
        addTurnSteps(*action, slice, stepsOfTurnAction(id) * count);
        return;
    }
    for (int i = 0; i < count; ++i) {
        execute(*action, slice);
    }
}

void HotKeyDispatcher::releaseAll(const HotKeySource& source)
{
    endTransmitHold(source);
    QStringList down;
    for (const CwKeyHold& hold : std::as_const(m_cwKeysDown)) {
        if (hold.source == source) {
            down.append(hold.id);
        }
    }
    for (const QString& id : std::as_const(down)) {
        endCwKey(id, source);
    }
}

void HotKeyDispatcher::releaseAllHolds()
{
    const QList<HotKeySource> holding = m_holdSources;
    for (const HotKeySource& source : holding) {
        endTransmitHold(source);
    }
    const QList<CwKeyHold> keys = m_cwKeysDown;
    for (const CwKeyHold& hold : keys) {
        endCwKey(hold.id, hold.source);
    }
}

void HotKeyDispatcher::setSleepClocksForTest(ClockMs wallClock, ClockMs awakeClock)
{
    m_wallClock = std::move(wallClock);
    m_awakeClock = std::move(awakeClock);
    noteSleepClocks();
}

void HotKeyDispatcher::noteSleepClocks()
{
    m_lastWallMs = m_wallClock();
    m_lastAwakeMs = m_awakeClock();
}

void HotKeyDispatcher::checkForSleep()
{
    if (!m_sleepTimer.isActive()) {
        return;
    }
    const qint64 wallGained = m_wallClock() - m_lastWallMs;
    const qint64 awakeGained = m_awakeClock() - m_lastAwakeMs;
    noteSleepClocks();
    // The wall clock can also be moved forward by itself: a time sync, or someone
    // setting the clock. Epoch time ignores a daylight change unless the machine's
    // clock itself jumps. A jump past the threshold during a hold drops that hold,
    // which fails toward key-up. Moved back, it reads as no sleep.
    if (wallGained - awakeGained <= kSleepGapMs) {
        return;
    }
    qCInfo(lcHotKeys) << "Holds dropped: the machine slept for" << wallGained - awakeGained << "ms";
    // As the other failsafes: holds drop, and a latched MOX or a TUNE stays on.
    releaseAllHolds();
    emit holdsDroppedBySleep();
}

void HotKeyDispatcher::updateSleepWatch()
{
    const bool holding = !m_holdSources.isEmpty() || !m_cwKeysDown.isEmpty();
    if (holding == m_sleepTimer.isActive()) {
        return;
    }
    if (holding) {
        noteSleepClocks();
        m_sleepTimer.start();
    } else {
        m_sleepTimer.stop();
    }
}

void HotKeyDispatcher::onTransmitWentOff()
{
    m_holdKeyed = false;
    noteTransmitHold();
    emit transmitKeyWentOff();
}

void HotKeyDispatcher::noteTransmitHold()
{
    const bool on = transmitHoldOn();
    if (on != m_transmitHoldOn) {
        m_transmitHoldOn = on;
        emit transmitHoldChanged(on);
    }
}

void HotKeyDispatcher::flushTune()
{
    m_turnQueues.removeIf([](const TurnQueue& queue) { return queue.slice.isNull(); });

    const qint64 now = m_targets->nowMs();
    bool stillPending = false;
    for (TurnQueue& queue : m_turnQueues) {
        if (queue.coalescer.pending() == 0) {
            continue;
        }
        if (queue.isTune && !tuneStillAllowed(queue.slice)) {
            queue.coalescer.drop();
            queue.hasInFlight = false;
            continue;
        }
        const int due = queue.coalescer.takeDue(now);
        if (due != 0) {
            sendTurnSteps(queue, due);
        }
        stillPending = stillPending || queue.coalescer.pending() != 0;
    }
    if (stillPending) {
        armFlushTimer();
    }
}

bool HotKeyDispatcher::admit(const HotKeyAction& action, QChar pin, SliceModel*& slice)
{
    const QString reason = m_targets->unavailableReason(action.id);
    if (!reason.isEmpty()) {
        m_targets->toast(reason);
        return false;
    }
    if (action.scope != HotKeyScope::Slice) {
        return true;
    }

    const QChar letter = pin.isNull() ? m_bindings->pinFor(action.id) : pin.toUpper();
    if (letter.isNull()) {
        slice = m_targets->windowSlice();
        if (slice == nullptr) {
            return false;
        }
    } else {
        slice = m_targets->sliceByLetter(letter);
        if (slice == nullptr) {
            m_targets->toast(QStringLiteral("Slice %1 isn't open.").arg(letter));
            return false;
        }
    }

    if (worksWhenListened(action.id)) {
        return true;
    }
    const QString refusal = m_targets->sliceRefusal(slice);
    if (m_targets->listened(slice) || !refusal.isEmpty()) {
        dropTune(slice);
        if (!refusal.isEmpty()) {
            m_targets->toast(refusal);
        }
        return false;
    }
    if (slice->locked() && refusedWhenLocked(action.id)) {
        dropTune(slice);
        m_targets->toast(QStringLiteral("Slice %1 is locked.").arg(m_targets->letterOf(slice)));
        return false;
    }
    return true;
}

void HotKeyDispatcher::execute(const HotKeyAction& action, SliceModel* slice)
{
    switch (action.scope) {
    case HotKeyScope::Slice:
        m_targets->runSliceAction(action.id, slice, 1);
        break;
    case HotKeyScope::Pan:
    case HotKeyScope::Window:
        m_targets->runWindowAction(action.id);
        break;
    case HotKeyScope::TransmitSlice:
        // VOX is a transmit button too, the TX applet's.
        if (action.id.startsWith(QLatin1String("tx.")) || action.id == QLatin1String("vox.toggle")) {
            m_targets->runTransmitAction(action.id);
        } else if (action.id.startsWith(QLatin1String("cw."))) {
            m_targets->runCwMessage(action.id);
        } else {
            // Voice messages have no entry point of their own yet.
            m_targets->runWindowAction(action.id);
        }
        break;
    }
}

void HotKeyDispatcher::addTurnSteps(const HotKeyAction& action, SliceModel* slice, int steps)
{
    const bool isTune = isTuneAction(action.id);
    auto queue = std::find_if(m_turnQueues.begin(), m_turnQueues.end(), [slice, isTune](const TurnQueue& q) {
        return q.slice == slice && q.isTune == isTune;
    });
    if (queue == m_turnQueues.end()) {
        m_turnQueues.append(TurnQueue{slice, isTune, {}, 0.0, 0, false});
        queue = std::prev(m_turnQueues.end());
    }

    const int due = queue->coalescer.add(steps, m_targets->nowMs());
    if (due != 0) {
        sendTurnSteps(*queue, due);
    }
    if (queue->coalescer.pending() != 0) {
        armFlushTimer();
    }
}

void HotKeyDispatcher::sendTurnSteps(TurnQueue& queue, int steps)
{
    if (queue.isTune) {
        sendTune(queue, steps);
    } else if (steps > 0) {
        m_targets->runSliceAction(QStringLiteral("volume.up"), queue.slice, steps);
    } else {
        m_targets->runSliceAction(QStringLiteral("volume.down"), queue.slice, -steps);
    }
}

void HotKeyDispatcher::sendTune(TurnQueue& queue, int steps)
{
    SliceModel* const slice = queue.slice;
    const int stepHz = slice->stepHz();
    if (stepHz <= 0) {
        qCWarning(lcHotKeys) << "Tune with no step size ignored:" << stepHz;
        queue.coalescer.drop();
        return;
    }

    const qint64 now = m_targets->nowMs();
    const bool burst = queue.hasInFlight && now - queue.inFlightAtMs <= kInFlightWindowMs;
    const double base = burst ? queue.inFlightHz : slice->frequency();
    const double step = stepHz;
    // Snapped as the receiver drag snaps, src/gui/SpectrumWidget.cpp:14022.
    const double hz = std::round((base + steps * step) / step) * step;

    if (m_targets->tuneTo(slice, hz)) {
        queue.inFlightHz = hz;
        queue.inFlightAtMs = now;
        queue.hasInFlight = true;
    } else {
        // Refused, or already there at a band limit: start again from the slice.
        queue.coalescer.drop();
        queue.hasInFlight = false;
    }
}

void HotKeyDispatcher::dropTune(const SliceModel* slice)
{
    for (TurnQueue& queue : m_turnQueues) {
        if (queue.isTune && queue.slice == slice) {
            queue.coalescer.drop();
            queue.hasInFlight = false;
        }
    }
}

void HotKeyDispatcher::dropAllTurns()
{
    for (TurnQueue& queue : m_turnQueues) {
        queue.coalescer.drop();
        queue.hasInFlight = false;
    }
    m_flushTimer.stop();
}

bool HotKeyDispatcher::tuneStillAllowed(const SliceModel* slice) const
{
    return !slice->locked() && !m_targets->listened(slice) && m_targets->sliceRefusal(slice).isEmpty();
}

void HotKeyDispatcher::armFlushTimer()
{
    if (!m_flushTimer.isActive()) {
        m_flushTimer.start();
    }
}

void HotKeyDispatcher::startHold(const HotKeyAction& action, const HotKeySource& source)
{
    if (action.id == idTxHold()) {
        // Duplicates first: key repeat must not retry or toast, whatever has
        // happened to the key since.
        if (m_holdSources.contains(source)) {
            return;
        }
        const QString reason = m_targets->unavailableReason(action.id);
        if (!reason.isEmpty()) {
            m_targets->toast(reason);
            return;
        }
        if (!m_holdKeyed && !m_targets->transmitting()) {
            // Claim first, so a transmit-off signal raised inside keyHold is not lost.
            m_holdKeyed = true;
            if (!m_targets->keyHold(true)) {
                m_holdKeyed = false;
                return;
            }
        }
        m_holdSources.append(source);
        updateSleepWatch();
        noteTransmitHold();
        return;
    }

    const auto holdsThis = [&action, &source](const CwKeyHold& hold) {
        return hold.id == action.id && hold.source == source;
    };
    if (std::any_of(m_cwKeysDown.cbegin(), m_cwKeysDown.cend(), holdsThis)) {
        return;
    }
    const QString reason = m_targets->unavailableReason(action.id);
    if (!reason.isEmpty()) {
        m_targets->toast(reason);
        return;
    }
    const bool alreadyDown = std::any_of(m_cwKeysDown.cbegin(), m_cwKeysDown.cend(),
                                         [&action](const CwKeyHold& hold) { return hold.id == action.id; });
    m_cwKeysDown.append(CwKeyHold{action.id, source});
    updateSleepWatch();
    if (!alreadyDown) {
        m_targets->setCwKeyDown(action.id, true);
    }
}

void HotKeyDispatcher::endTransmitHold(const HotKeySource& source)
{
    const qsizetype removed = m_holdSources.removeAll(source);
    updateSleepWatch();
    if (removed == 0 || !m_holdSources.isEmpty() || !m_holdKeyed) {
        noteTransmitHold();
        return;
    }
    m_holdKeyed = false;
    noteTransmitHold();
    // TUNE or two-tone took over the key during the hold: they stay on after
    // you let go, like the buttons.
    if (!m_targets->tuneOrTwoToneOn()) {
        m_targets->keyHold(false);
    }
}

void HotKeyDispatcher::endCwKey(const QString& id, const HotKeySource& source)
{
    const qsizetype removed = m_cwKeysDown.removeIf([&id, &source](const CwKeyHold& hold) {
        return hold.id == id && hold.source == source;
    });
    updateSleepWatch();
    if (removed == 0) {
        return;
    }
    const bool stillDown = std::any_of(m_cwKeysDown.cbegin(), m_cwKeysDown.cend(),
                                       [&id](const CwKeyHold& hold) { return hold.id == id; });
    if (!stillDown) {
        m_targets->setCwKeyDown(id, false);
    }
}

} // namespace NereusSDR
