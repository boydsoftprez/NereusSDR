// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 2.
//                AI-assisted implementation via Anthropic Claude Code.
//   2026-11-07 - J.J. Boyd (KG4VCF), hot keys: VOX on or off (vox.toggle).
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
//
// The one dispatcher, against a fake HotKeyTargets with an injected clock: the
// off switch, slice choice, refusals, burst limit and hold ledger
// (R-HK-02, R-HK-06, R-HK-08, R-HK-14).
#include <QtTest>

#include <QHash>
#include <QList>
#include <QPair>
#include <QRegularExpression>
#include <QSet>

#include <memory>

#include "core/hotkeys/HotKeyBindings.h"
#include "core/hotkeys/TuneCoalescer.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/HotKeyTargets.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

const HotKeySource kSpace{HotKeySourceKind::Keyboard, {}};
const HotKeySource kPedal{HotKeySourceKind::Midi, QStringLiteral("pedal")};

struct TuneCall {
    SliceModel* slice;
    double hz;
    qint64 atMs;
};

struct SliceCall {
    QString id;
    SliceModel* slice;
    int steps;
};

class FakeTargets : public HotKeyTargets {
public:
    SliceModel* window{nullptr};
    QHash<QChar, SliceModel*> open;
    QSet<const SliceModel*> listenedSlices;
    QString refusalText{QStringLiteral("Slice A is controlled by another device.")};
    QHash<QString, QString> reasons;
    bool transmittingNow{false};
    bool tuneOrTwoTone{false};
    bool keyHoldAdmits{true};
    bool tuneMovesSlice{false};  // false: the slice's frequency stays stale
    bool tuneAccepts{true};
    qint64 clock{1000};

    mutable int anyCall{0};
    QList<TuneCall> tunes;
    QList<SliceCall> sliceActions;
    QStringList windowActions;
    QStringList transmitActions;
    QStringList cwMessages;
    QList<bool> keyHolds;
    QList<QPair<QString, bool>> cwKeys;
    QStringList toasts;

    SliceModel* windowSlice() const override { ++anyCall; return window; }
    SliceModel* sliceByLetter(QChar letter) const override { ++anyCall; return open.value(letter, nullptr); }
    QChar letterOf(const SliceModel* slice) const override
    {
        ++anyCall;
        for (auto it = open.cbegin(); it != open.cend(); ++it) {
            if (it.value() == slice) {
                return it.key();
            }
        }
        return {};
    }
    QString sliceRefusal(const SliceModel* slice) const override
    {
        ++anyCall;
        return listenedSlices.contains(slice) ? refusalText : QString();
    }
    bool listened(const SliceModel* slice) const override { ++anyCall; return listenedSlices.contains(slice); }
    bool tuneTo(SliceModel* slice, double hz) override
    {
        ++anyCall;
        tunes.append({slice, hz, clock});
        if (tuneMovesSlice) {
            slice->setFrequency(hz);
        }
        return tuneAccepts;
    }
    void runSliceAction(const QString& id, SliceModel* slice, int steps) override
    {
        ++anyCall;
        sliceActions.append({id, slice, steps});
    }
    void runWindowAction(const QString& id) override { ++anyCall; windowActions.append(id); }
    QString unavailableReason(const QString& id) const override { ++anyCall; return reasons.value(id); }
    bool transmitting() const override { ++anyCall; return transmittingNow; }
    bool tuneOrTwoToneOn() const override { ++anyCall; return tuneOrTwoTone; }
    bool keyHold(bool down) override
    {
        ++anyCall;
        keyHolds.append(down);
        return down ? keyHoldAdmits : true;
    }
    void runTransmitAction(const QString& id) override { ++anyCall; transmitActions.append(id); }
    void runCwMessage(const QString& id) override { ++anyCall; cwMessages.append(id); }
    void setCwKeyDown(const QString& id, bool down) override { ++anyCall; cwKeys.append({id, down}); }
    void toast(const QString& text) override { ++anyCall; toasts.append(text); }
    qint64 nowMs() const override { return clock; }

    int anythingRan() const
    {
        return int(tunes.size() + sliceActions.size() + windowActions.size() + transmitActions.size()
                   + cwMessages.size() + keyHolds.size() + cwKeys.size());
    }
};

// Clocks a test moves by hand, so no case depends on load.
struct SleepClocks {
    explicit SleepClocks(HotKeyDispatcher& dispatcher)
    {
        dispatcher.setSleepClocksForTest([this] { return wall; }, [this] { return awakeTime; });
    }
    void awake(qint64 ms)
    {
        wall += ms;
        awakeTime += ms;
    }
    void sleep(qint64 ms) { wall += ms; }

    qint64 wall{1000000};
    qint64 awakeTime{5000};
};

} // namespace

class TestHotKeyDispatcher : public QObject {
    Q_OBJECT

private:
    // A fresh set for every test: QTest makes one object per run, so init() rebuilds.
    std::unique_ptr<HotKeyBindings> m_bindings;
    std::unique_ptr<FakeTargets> m_targets;
    std::unique_ptr<HotKeyDispatcher> m_dispatcher;
    std::unique_ptr<SliceModel> m_a;
    std::unique_ptr<SliceModel> m_b;

    FakeTargets& t() { return *m_targets; }
    HotKeyDispatcher& d() { return *m_dispatcher; }

private slots:
    void init()
    {
        m_bindings = std::make_unique<HotKeyBindings>();
        m_targets = std::make_unique<FakeTargets>();
        m_dispatcher = std::make_unique<HotKeyDispatcher>(m_bindings.get(), m_targets.get());
        m_a = std::make_unique<SliceModel>(0);
        m_b = std::make_unique<SliceModel>(1);
        m_a->setFrequency(14200000.0);
        m_a->setStepHz(100);
        m_b->setFrequency(7100000.0);
        m_b->setStepHz(100);
        t().window = m_a.get();
        t().open.insert(QLatin1Char('A'), m_a.get());
        t().open.insert(QLatin1Char('B'), m_b.get());
    }

    void cleanup()
    {
        m_dispatcher.reset();
        m_targets.reset();
        m_bindings.reset();
        m_a.reset();
        m_b.reset();
    }

    // ---- TuneCoalescer, the pure part ----

    void coalescer_firstAddSendsAtOnce()
    {
        TuneCoalescer c;
        QCOMPARE(c.add(3, 0), 3);
        QCOMPARE(c.pending(), 0);
    }

    void coalescer_holdsUntilTheIntervalHasPassed()
    {
        TuneCoalescer c;
        c.add(1, 100);
        QCOMPARE(c.add(2, 119), 0);
        QCOMPARE(c.pending(), 2);
        QCOMPARE(c.takeDue(119), 0);
        QCOMPARE(c.takeDue(120), 2);
        QCOMPARE(c.pending(), 0);
    }

    void coalescer_sumsOppositeSteps()
    {
        TuneCoalescer c;
        c.add(1, 0);
        c.add(5, 1);
        c.add(-3, 2);
        QCOMPARE(c.pending(), 2);
        QCOMPARE(c.add(-2, 3), 0);
        QCOMPARE(c.takeDue(100), 0);
    }

    void coalescer_dropKeepsTheInterval()
    {
        TuneCoalescer c;
        c.add(1, 0);
        c.add(4, 5);
        c.drop();
        QCOMPARE(c.pending(), 0);
        QCOMPARE(c.add(1, 10), 0);
        QCOMPARE(c.takeDue(20), 1);
    }

    // ---- Off switch ----

    void offSwitch_callsNoTarget()
    {
        m_bindings->setEnabled(false);
        d().press(QStringLiteral("tune.up"), kSpace);
        d().press(QStringLiteral("tx.hold"), kSpace);
        d().press(QStringLiteral("zoom.in"), kSpace);
        d().release(QStringLiteral("tx.hold"), kSpace);
        d().turn(QStringLiteral("tune.up"), QStringLiteral("tune.down"), 3, kSpace);
        d().flushTune();
        QCOMPARE(t().anyCall, 0);
    }

    void offSwitch_duringHoldDropsItOnce()
    {
        d().press(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, QList<bool>{true});
        m_bindings->setEnabled(false);
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
        d().release(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds.size(), 2);
    }

    void offSwitch_duringCwKeyReleasesIt()
    {
        d().press(QStringLiteral("cw.dot"), kSpace);
        m_bindings->setEnabled(false);
        QCOMPARE(t().cwKeys.size(), 2);
        QCOMPARE(t().cwKeys.last(), qMakePair(QStringLiteral("cw.dot"), false));
    }

    void offSwitch_dropsQueuedSteps()
    {
        d().press(QStringLiteral("tune.up"), kSpace);
        t().clock += 5;
        d().press(QStringLiteral("tune.up"), kSpace);
        m_bindings->setEnabled(false);
        t().clock += 100;
        d().flushTune();
        QCOMPARE(t().tunes.size(), 1);
    }

    void offSwitch_onAgainHasNoLeftovers()
    {
        d().press(QStringLiteral("tx.hold"), kSpace);
        m_bindings->setEnabled(false);
        m_bindings->setEnabled(true);
        // The earlier hold is gone, so this press is a new key.
        d().press(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, (QList<bool>{true, false, true}));
    }

    // ---- Which slice (7.2) ----

    void slice_noPinUsesTheWindowSlice()
    {
        d().press(QStringLiteral("filter.next"), kSpace);
        QCOMPARE(t().sliceActions.size(), 1);
        QCOMPARE(t().sliceActions.at(0).slice, m_a.get());
        QCOMPARE(t().sliceActions.at(0).steps, 1);
    }

    void slice_bindingPinActsOnThatSlice()
    {
        m_bindings->setPin(QStringLiteral("filter.next"), QLatin1Char('B'));
        d().press(QStringLiteral("filter.next"), kSpace);
        QCOMPARE(t().sliceActions.size(), 1);
        QCOMPARE(t().sliceActions.at(0).slice, m_b.get());
    }

    void slice_closedPinRunsNothingAndSaysSo()
    {
        m_bindings->setPin(QStringLiteral("filter.next"), QLatin1Char('B'));
        t().open.remove(QLatin1Char('B'));
        d().press(QStringLiteral("filter.next"), kSpace);
        QCOMPARE(t().anythingRan(), 0);
        QCOMPARE(t().toasts, QStringList{QStringLiteral("Slice B isn't open.")});
    }

    void slice_pinArgumentOverridesTheBinding()
    {
        m_bindings->setPin(QStringLiteral("filter.next"), QLatin1Char('B'));
        d().press(QStringLiteral("filter.next"), kSpace, QLatin1Char('A'));
        QCOMPARE(t().sliceActions.at(0).slice, m_a.get());

        t().window = m_a.get();
        d().turn(QStringLiteral("tune.up"), QStringLiteral("tune.down"), 1, kSpace, QLatin1Char('B'));
        QCOMPARE(t().tunes.size(), 1);
        QCOMPARE(t().tunes.at(0).slice, m_b.get());
    }

    void slice_closedPinArgumentSaysSo()
    {
        d().press(QStringLiteral("filter.next"), kSpace, QLatin1Char('D'));
        QCOMPARE(t().anythingRan(), 0);
        QCOMPARE(t().toasts, QStringList{QStringLiteral("Slice D isn't open.")});
    }

    void slice_noWindowSliceRunsNothing()
    {
        t().window = nullptr;
        d().press(QStringLiteral("filter.next"), kSpace);
        d().press(QStringLiteral("tune.up"), kSpace);
        QCOMPARE(t().anythingRan(), 0);
    }

    // ---- Listened ----

    void listened_refuses_data()
    {
        QTest::addColumn<QString>("id");
        for (const char* id : {"tune.up", "filter.next", "mode.next", "band.next", "nr.toggle", "rit.toggle",
                               "step.larger", "slice.lock", "tune.goto"}) {
            QTest::newRow(id) << QString::fromLatin1(id);
        }
    }

    void listened_refuses()
    {
        QFETCH(QString, id);
        t().listenedSlices.insert(m_a.get());
        d().press(id, kSpace);
        QCOMPARE(t().anythingRan(), 0);
        QCOMPARE(t().toasts, QStringList{t().refusalText});
    }

    void listened_audioStillRuns()
    {
        t().listenedSlices.insert(m_a.get());
        d().press(QStringLiteral("volume.up"), kSpace);
        t().clock += 100;
        d().press(QStringLiteral("volume.down"), kSpace);
        d().press(QStringLiteral("mute"), kSpace);
        QCOMPARE(t().sliceActions.size(), 3);
        QCOMPARE(t().sliceActions.at(0).id, QStringLiteral("volume.up"));
        QCOMPARE(t().sliceActions.at(1).id, QStringLiteral("volume.down"));
        QCOMPARE(t().sliceActions.at(2).id, QStringLiteral("mute"));
        QVERIFY(t().toasts.isEmpty());
    }

    // ---- Lock (V6) ----

    void lock_refuses_data()
    {
        QTest::addColumn<QString>("id");
        for (const char* id : {"tune.up", "tune.down10", "tune.goto", "band.next", "band.previous", "mode.next",
                               "mode.previous"}) {
            QTest::newRow(id) << QString::fromLatin1(id);
        }
    }

    void lock_refuses()
    {
        QFETCH(QString, id);
        m_a->setLocked(true);
        d().press(id, kSpace);
        QCOMPARE(t().anythingRan(), 0);
        QCOMPARE(t().toasts, QStringList{QStringLiteral("Slice A is locked.")});
    }

    void lock_turnRefuses()
    {
        m_a->setLocked(true);
        d().turn(QStringLiteral("tune.up"), QStringLiteral("tune.down"), 4, kSpace);
        QCOMPARE(t().anythingRan(), 0);
        QCOMPARE(t().toasts, QStringList{QStringLiteral("Slice A is locked.")});
    }

    void lock_leavesTheLockKeyAndVolumeWorking()
    {
        m_a->setLocked(true);
        d().press(QStringLiteral("slice.lock"), kSpace);
        d().press(QStringLiteral("volume.up"), kSpace);
        QCOMPARE(t().sliceActions.size(), 2);
        QVERIFY(t().toasts.isEmpty());
    }

    // ---- Unavailable ----

    void unavailable_runsNothingAndToastsTheReason_data()
    {
        QTest::addColumn<QString>("id");
        for (const char* id : {"tune.up", "filter.next", "zoom.in", "bandstack.next", "tx.mox", "tx.hold", "cw.msg1",
                               "cw.dot", "voice.msg1"}) {
            QTest::newRow(id) << QString::fromLatin1(id);
        }
    }

    void unavailable_runsNothingAndToastsTheReason()
    {
        QFETCH(QString, id);
        const QString reason = QStringLiteral("That isn't ready.");
        t().reasons.insert(id, reason);
        d().press(id, kSpace);
        QCOMPARE(t().anythingRan(), 0);
        QCOMPARE(t().toasts, QStringList{reason});
    }

    void unavailable_pressStartsNoHold()
    {
        t().reasons.insert(QStringLiteral("tx.hold"), QStringLiteral("No radio."));
        d().press(QStringLiteral("tx.hold"), kSpace);
        t().reasons.clear();
        d().release(QStringLiteral("tx.hold"), kSpace);
        QVERIFY(t().keyHolds.isEmpty());
    }

    void unavailable_afterPressStillReleases()
    {
        d().press(QStringLiteral("tx.hold"), kSpace);
        t().reasons.insert(QStringLiteral("tx.hold"), QStringLiteral("No radio."));
        d().release(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
    }

    // ---- Burst (R-HK-14) ----

    void burst_tenPressesLimitedAndNoStepLost()
    {
        t().tuneMovesSlice = true;
        for (int i = 0; i < 10; ++i) {
            d().press(QStringLiteral("tune.up"), kSpace);
            d().flushTune();
            t().clock += 3;
        }
        t().clock += TuneCoalescer::kMinIntervalMs;
        d().flushTune();

        for (int i = 1; i < t().tunes.size(); ++i) {
            QVERIFY(t().tunes.at(i).atMs - t().tunes.at(i - 1).atMs >= TuneCoalescer::kMinIntervalMs);
        }
        QVERIFY(t().tunes.size() < 10);
        QCOMPARE(m_a->frequency(), 14201000.0);
    }

    void burst_turnAddsAndSubtracts()
    {
        d().turn(QStringLiteral("tune.up"), QStringLiteral("tune.down"), 5, kSpace);
        QCOMPARE(t().tunes.size(), 1);
        QCOMPARE(t().tunes.at(0).hz, 14200500.0);

        t().tuneMovesSlice = true;
        t().clock += TuneCoalescer::kMinIntervalMs;
        d().turn(QStringLiteral("tune.up"), QStringLiteral("tune.down"), -3, kSpace);
        QCOMPARE(t().tunes.size(), 2);
        QCOMPARE(t().tunes.at(1).hz, 14200200.0);
    }

    void burst_turnOfZeroDoesNothing()
    {
        d().turn(QStringLiteral("tune.up"), QStringLiteral("tune.down"), 0, kSpace);
        QCOMPARE(t().anyCall, 0);
    }

    void burst_snapsToTheStepGrid_data()
    {
        QTest::addColumn<int>("steps");
        QTest::addColumn<double>("expected");
        QTest::newRow("up") << 1 << 14200200.0;
        QTest::newRow("down") << -1 << 14200000.0;
    }

    void burst_snapsToTheStepGrid()
    {
        QFETCH(int, steps);
        QFETCH(double, expected);
        m_a->setFrequency(14200123.0);
        d().turn(QStringLiteral("tune.up"), QStringLiteral("tune.down"), steps, kSpace);
        QCOMPARE(t().tunes.size(), 1);
        QCOMPARE(t().tunes.at(0).hz, expected);
    }

    void burst_secondTuneBuildsOnTheInFlightTarget()
    {
        // tuneMovesSlice stays false: the slice has not caught up yet.
        d().press(QStringLiteral("tune.up"), kSpace);
        t().clock += TuneCoalescer::kMinIntervalMs;
        d().press(QStringLiteral("tune.up"), kSpace);
        QCOMPARE(t().tunes.size(), 2);
        QCOMPARE(t().tunes.at(0).hz, 14200100.0);
        QCOMPARE(t().tunes.at(1).hz, 14200200.0);
    }

    void burst_afterTheBurstBuildsOnTheSlice()
    {
        // Something else moved the slice long after the last key.
        d().press(QStringLiteral("tune.up"), kSpace);
        t().clock += 5000;
        m_a->setFrequency(14300000.0);
        d().press(QStringLiteral("tune.up"), kSpace);
        QCOMPARE(t().tunes.at(1).hz, 14300100.0);
    }

    void burst_refusedTuneForgetsTheTarget()
    {
        t().tuneAccepts = false;
        d().press(QStringLiteral("tune.up"), kSpace);
        t().tuneAccepts = true;
        t().clock += TuneCoalescer::kMinIntervalMs;
        d().press(QStringLiteral("tune.up"), kSpace);
        QCOMPARE(t().tunes.at(1).hz, 14200100.0);
    }

    void burst_tenStepKeysCountTen()
    {
        d().press(QStringLiteral("tune.up10"), kSpace);
        QCOMPARE(t().tunes.at(0).hz, 14201000.0);

        t().tuneMovesSlice = true;
        t().clock += TuneCoalescer::kMinIntervalMs;
        d().press(QStringLiteral("tune.down10"), kSpace);
        QCOMPARE(t().tunes.at(1).hz, 14200000.0);
    }

    void burst_tenStepKnobMultiplies()
    {
        d().turn(QStringLiteral("tune.up10"), QStringLiteral("tune.down10"), 2, kSpace);
        QCOMPARE(t().tunes.at(0).hz, 14202000.0);
    }

    void burst_twoSlicesKeepSeparateLimits()
    {
        d().press(QStringLiteral("tune.up"), kSpace);
        d().press(QStringLiteral("tune.up"), kSpace, QLatin1Char('B'));
        QCOMPARE(t().tunes.size(), 2);
        QCOMPARE(t().tunes.at(1).slice, m_b.get());
        QCOMPARE(t().tunes.at(1).hz, 7100100.0);
    }

    void burst_timerFlushesWhatIsQueued()
    {
        d().press(QStringLiteral("tune.up"), kSpace);
        d().press(QStringLiteral("tune.up"), kSpace);
        QCOMPARE(t().tunes.size(), 1);
        t().clock += TuneCoalescer::kMinIntervalMs;
        // Real time only bounds the wait here; the clock is already past due.
        QTRY_COMPARE_WITH_TIMEOUT(t().tunes.size(), 2, 10000);
    }

    void burst_sliceClosedWhileQueuedSendsNothing()
    {
        auto c = std::make_unique<SliceModel>(2);
        c->setFrequency(3600000.0);
        t().open.insert(QLatin1Char('C'), c.get());
        d().press(QStringLiteral("tune.up"), kSpace, QLatin1Char('C'));
        d().press(QStringLiteral("tune.up"), kSpace, QLatin1Char('C'));
        c.reset();
        t().open.remove(QLatin1Char('C'));
        t().clock += 100;
        d().flushTune();
        QCOMPARE(t().tunes.size(), 1);
    }

    // ---- Drop ----

    void drop_lockWhileQueued()
    {
        d().press(QStringLiteral("tune.up"), kSpace);
        d().press(QStringLiteral("tune.up"), kSpace);
        d().press(QStringLiteral("tune.up"), kSpace);
        QCOMPARE(t().tunes.size(), 1);
        m_a->setLocked(true);
        t().clock += 100;
        d().flushTune();
        QCOMPARE(t().tunes.size(), 1);

        m_a->setLocked(false);
        m_a->setFrequency(14200000.0);
        d().press(QStringLiteral("tune.up"), kSpace);
        QCOMPARE(t().tunes.size(), 2);
        QCOMPARE(t().tunes.at(1).hz, 14200100.0);
    }

    void drop_lockedPressClearsTheQueue()
    {
        d().press(QStringLiteral("tune.up"), kSpace);
        d().press(QStringLiteral("tune.up"), kSpace);
        m_a->setLocked(true);
        d().press(QStringLiteral("tune.up"), kSpace);
        m_a->setLocked(false);
        t().clock += 100;
        d().flushTune();
        QCOMPARE(t().tunes.size(), 1);
    }

    void drop_listenedWhileQueued()
    {
        d().press(QStringLiteral("tune.up"), kSpace);
        d().press(QStringLiteral("tune.up"), kSpace);
        t().listenedSlices.insert(m_a.get());
        t().clock += 100;
        d().flushTune();
        QCOMPARE(t().tunes.size(), 1);
    }

    // ---- Hold ledger (R-HK-08) ----

    void ledger_pressKeysAndReleaseUnkeys()
    {
        d().press(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, QList<bool>{true});
        d().release(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
    }

    void ledger_latchedMoxSurvivesLettingGo()
    {
        t().transmittingNow = true;
        d().press(QStringLiteral("tx.hold"), kSpace);
        d().release(QStringLiteral("tx.hold"), kSpace);
        QVERIFY(t().keyHolds.isEmpty());
    }

    void ledger_twoSourcesLastReleaseUnkeys()
    {
        d().press(QStringLiteral("tx.hold"), kSpace);
        d().press(QStringLiteral("tx.hold"), kPedal);
        QCOMPARE(t().keyHolds, QList<bool>{true});
        d().release(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, QList<bool>{true});
        d().release(QStringLiteral("tx.hold"), kPedal);
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
    }

    void ledger_twoSourcesEitherOrder()
    {
        d().press(QStringLiteral("tx.hold"), kSpace);
        d().press(QStringLiteral("tx.hold"), kPedal);
        d().release(QStringLiteral("tx.hold"), kPedal);
        QCOMPARE(t().keyHolds, QList<bool>{true});
        d().release(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
    }

    void ledger_refusedKeyHoldReleaseDoesNothing()
    {
        t().keyHoldAdmits = false;
        d().press(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, QList<bool>{true});
        d().release(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, QList<bool>{true});
    }

    void ledger_refusedKeyHoldCanTryAgain()
    {
        t().keyHoldAdmits = false;
        d().press(QStringLiteral("tx.hold"), kSpace);
        t().keyHoldAdmits = true;
        d().press(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, (QList<bool>{true, true}));
        d().release(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, (QList<bool>{true, true, false}));
    }

    void ledger_transmitWentOffClearsTheClaim()
    {
        d().press(QStringLiteral("tx.hold"), kSpace);
        d().onTransmitWentOff();
        // A key from elsewhere: the MOX button, a time-out recovery.
        t().transmittingNow = true;
        d().release(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, QList<bool>{true});
    }

    void ledger_tuneOnAtReleaseStaysOnAndClears()
    {
        d().press(QStringLiteral("tx.hold"), kSpace);
        t().tuneOrTwoTone = true;
        d().release(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, QList<bool>{true});

        // The ledger is clear: the next press is a new hold, its own release unkeys.
        t().tuneOrTwoTone = false;
        d().press(QStringLiteral("tx.hold"), kSpace);
        d().release(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, (QList<bool>{true, true, false}));
    }

    void ledger_duplicatePressIgnored()
    {
        d().press(QStringLiteral("tx.hold"), kSpace);
        d().press(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, QList<bool>{true});
        d().release(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
    }

    void ledger_duplicatePressAfterTransmitWentOffDoesNotKeyAgain()
    {
        d().press(QStringLiteral("tx.hold"), kSpace);
        d().onTransmitWentOff();
        d().press(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, QList<bool>{true});
    }

    void ledger_releaseWithNoPressIgnored()
    {
        d().release(QStringLiteral("tx.hold"), kSpace);
        d().release(QStringLiteral("cw.dot"), kSpace);
        QVERIFY(t().keyHolds.isEmpty());
        QVERIFY(t().cwKeys.isEmpty());
    }

    void ledger_otherSourceReleaseDoesNotCount()
    {
        d().press(QStringLiteral("tx.hold"), kSpace);
        d().release(QStringLiteral("tx.hold"), kPedal);
        QCOMPARE(t().keyHolds, QList<bool>{true});
    }

    void ledger_releaseAllIsOneSourceOnly()
    {
        d().press(QStringLiteral("tx.hold"), kSpace);
        d().press(QStringLiteral("tx.hold"), kPedal);
        d().press(QStringLiteral("cw.dot"), kSpace);
        d().press(QStringLiteral("cw.dash"), kPedal);

        d().releaseAll(kPedal);
        QCOMPARE(t().keyHolds, QList<bool>{true});
        QVERIFY(t().cwKeys.contains(qMakePair(QStringLiteral("cw.dash"), false)));
        QVERIFY(!t().cwKeys.contains(qMakePair(QStringLiteral("cw.dot"), false)));

        d().releaseAll(kSpace);
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
        QVERIFY(t().cwKeys.contains(qMakePair(QStringLiteral("cw.dot"), false)));
    }

    void ledger_releaseAllWithTuneOnStaysOn()
    {
        d().press(QStringLiteral("tx.hold"), kSpace);
        t().tuneOrTwoTone = true;
        d().releaseAll(kSpace);
        QCOMPARE(t().keyHolds, QList<bool>{true});
    }

    void ledger_releaseAllHoldsIsEveryone()
    {
        d().press(QStringLiteral("tx.hold"), kSpace);
        d().press(QStringLiteral("tx.hold"), kPedal);
        d().press(QStringLiteral("cw.dot"), kPedal);
        d().releaseAllHolds();
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
        QCOMPARE(t().cwKeys.last(), qMakePair(QStringLiteral("cw.dot"), false));
        d().release(QStringLiteral("tx.hold"), kSpace);
        d().release(QStringLiteral("cw.dot"), kPedal);
        QCOMPARE(t().keyHolds.size(), 2);
        QCOMPARE(t().cwKeys.size(), 2);
    }

    // ---- The sleep failsafe ----

    void sleep_aGapDropsEveryHoldFromEverySource()
    {
        SleepClocks clocks(d());
        QSignalSpy dropped(&d(), &HotKeyDispatcher::holdsDroppedBySleep);
        d().press(QStringLiteral("tx.hold"), kSpace);
        d().press(QStringLiteral("tx.hold"), kPedal);
        d().press(QStringLiteral("cw.dot"), kPedal);

        clocks.sleep(3600000);
        d().checkForSleep();

        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
        QCOMPARE(t().cwKeys.last(), qMakePair(QStringLiteral("cw.dot"), false));
        QCOMPARE(dropped.size(), 1);
        QVERIFY(t().toasts.isEmpty());
        QVERIFY(!d().sleepWatchRunning());
    }

    void sleep_theThresholdIsAboveAndNotAt()
    {
        SleepClocks clocks(d());
        d().press(QStringLiteral("tx.hold"), kSpace);
        clocks.sleep(HotKeyDispatcher::kSleepGapMs);
        d().checkForSleep();
        QCOMPARE(t().keyHolds, QList<bool>{true});
        clocks.sleep(HotKeyDispatcher::kSleepGapMs + 1);
        d().checkForSleep();
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));
    }

    void sleep_aNormalTickOrAStallDoesNotDrop()
    {
        SleepClocks clocks(d());
        QSignalSpy dropped(&d(), &HotKeyDispatcher::holdsDroppedBySleep);
        d().press(QStringLiteral("tx.hold"), kSpace);
        for (const qint64 awakeMs : {qint64(HotKeyDispatcher::kSleepTickMs), qint64(2000), qint64(60000)}) {
            clocks.awake(awakeMs);
            d().checkForSleep();
        }
        QCOMPARE(t().keyHolds, QList<bool>{true});
        QCOMPARE(dropped.size(), 0);
        QVERIFY(d().sleepWatchRunning());
    }

    void sleep_theWallClockSetBackIsNoSleep()
    {
        SleepClocks clocks(d());
        d().press(QStringLiteral("tx.hold"), kSpace);
        clocks.wall -= 3600000;
        d().checkForSleep();
        QCOMPARE(t().keyHolds, QList<bool>{true});
    }

    void sleep_theTimerRunsOnlyWhileSomethingIsHeld()
    {
        QVERIFY(!d().sleepWatchRunning());
        d().press(QStringLiteral("tx.mox"), kSpace);
        QVERIFY(!d().sleepWatchRunning());

        d().press(QStringLiteral("tx.hold"), kSpace);
        QVERIFY(d().sleepWatchRunning());
        d().press(QStringLiteral("cw.dot"), kPedal);
        d().release(QStringLiteral("tx.hold"), kSpace);
        QVERIFY(d().sleepWatchRunning());
        d().release(QStringLiteral("cw.dot"), kPedal);
        QVERIFY(!d().sleepWatchRunning());

        // A refused hold joins nothing.
        t().reasons.insert(QStringLiteral("tx.hold"), QStringLiteral("No."));
        d().press(QStringLiteral("tx.hold"), kSpace);
        QVERIFY(!d().sleepWatchRunning());
    }

    void sleep_theCheckWithNothingHeldDoesNothing()
    {
        SleepClocks clocks(d());
        QSignalSpy dropped(&d(), &HotKeyDispatcher::holdsDroppedBySleep);
        clocks.sleep(3600000);
        d().checkForSleep();
        QCOMPARE(dropped.size(), 0);
    }

    void sleep_aHoldWithoutTheClaimLeavesALatchedMox()
    {
        SleepClocks clocks(d());
        t().transmittingNow = true;
        d().press(QStringLiteral("tx.hold"), kSpace);
        clocks.sleep(3600000);
        d().checkForSleep();
        QVERIFY(t().keyHolds.isEmpty());
        QVERIFY(!d().sleepWatchRunning());
    }

    void sleep_aTuneTakenOverStaysOn()
    {
        SleepClocks clocks(d());
        d().press(QStringLiteral("tx.hold"), kSpace);
        t().tuneOrTwoTone = true;
        clocks.sleep(3600000);
        d().checkForSleep();
        QCOMPARE(t().keyHolds, QList<bool>{true});
    }

    void sleep_aLaterHoldWorksAndTheOldReleaseDoesNothing()
    {
        SleepClocks clocks(d());
        d().press(QStringLiteral("tx.hold"), kSpace);
        clocks.sleep(3600000);
        d().checkForSleep();
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));

        d().release(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, (QList<bool>{true, false}));

        // Time asleep before this hold is not held against it.
        clocks.sleep(3600000);
        d().press(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, (QList<bool>{true, false, true}));
        clocks.awake(HotKeyDispatcher::kSleepTickMs);
        d().checkForSleep();
        QCOMPARE(t().keyHolds, (QList<bool>{true, false, true}));
        d().release(QStringLiteral("tx.hold"), kSpace);
        QCOMPARE(t().keyHolds, (QList<bool>{true, false, true, false}));
    }

    void sleep_theTimerRunsTheCheck()
    {
        SleepClocks clocks(d());
        d().press(QStringLiteral("tx.hold"), kSpace);
        clocks.sleep(3600000);
        QTRY_COMPARE_WITH_TIMEOUT(t().keyHolds, (QList<bool>{true, false}), 30000);
    }

    void sleep_theRealClocksDoNotDropAHold()
    {
        d().press(QStringLiteral("tx.hold"), kSpace);
        d().checkForSleep();
        QTest::qWait(50);
        d().checkForSleep();
        QCOMPARE(t().keyHolds, QList<bool>{true});
    }

    // ---- CW keys ----

    void cw_pressAndRelease_data()
    {
        QTest::addColumn<QString>("id");
        for (const char* id : {"cw.dot", "cw.dash", "cw.straight"}) {
            QTest::newRow(id) << QString::fromLatin1(id);
        }
    }

    void cw_pressAndRelease()
    {
        QFETCH(QString, id);
        d().press(id, kSpace);
        QCOMPARE(t().cwKeys.size(), 1);
        QCOMPARE(t().cwKeys.at(0), qMakePair(id, true));
        d().press(id, kSpace);
        QCOMPARE(t().cwKeys.size(), 1);
        d().release(id, kSpace);
        QCOMPARE(t().cwKeys.size(), 2);
        QCOMPARE(t().cwKeys.at(1), qMakePair(id, false));
    }

    void cw_refusedPressMeansNoRelease()
    {
        t().reasons.insert(QStringLiteral("cw.dot"), QStringLiteral("Not in a CW mode."));
        d().press(QStringLiteral("cw.dot"), kSpace);
        t().reasons.clear();
        d().release(QStringLiteral("cw.dot"), kSpace);
        QVERIFY(t().cwKeys.isEmpty());
    }

    void cw_twoSourcesOnOneKeyLastReleaseLifts()
    {
        d().press(QStringLiteral("cw.dot"), kSpace);
        d().press(QStringLiteral("cw.dot"), kPedal);
        QCOMPARE(t().cwKeys.size(), 1);
        d().release(QStringLiteral("cw.dot"), kSpace);
        QCOMPARE(t().cwKeys.size(), 1);
        d().release(QStringLiteral("cw.dot"), kPedal);
        QCOMPARE(t().cwKeys.size(), 2);
    }

    void cw_messagesGoToTheSender()
    {
        d().press(QStringLiteral("cw.msg3"), kSpace);
        d().press(QStringLiteral("cw.stop"), kSpace);
        QCOMPARE(t().cwMessages, (QStringList{QStringLiteral("cw.msg3"), QStringLiteral("cw.stop")}));
    }

    // ---- Levels ----

    void levels_turnSumsUnderTheLimit()
    {
        d().turn(QStringLiteral("volume.up"), QStringLiteral("volume.down"), 2, kSpace);
        QCOMPARE(t().sliceActions.size(), 1);
        QCOMPARE(t().sliceActions.at(0).id, QStringLiteral("volume.up"));
        QCOMPARE(t().sliceActions.at(0).slice, m_a.get());
        QCOMPARE(t().sliceActions.at(0).steps, 2);

        t().clock += 5;
        d().turn(QStringLiteral("volume.up"), QStringLiteral("volume.down"), 3, kSpace);
        t().clock += 5;
        d().turn(QStringLiteral("volume.up"), QStringLiteral("volume.down"), 1, kSpace);
        d().flushTune();
        QCOMPARE(t().sliceActions.size(), 1);

        t().clock += TuneCoalescer::kMinIntervalMs;
        d().flushTune();
        QCOMPARE(t().sliceActions.size(), 2);
        QCOMPARE(t().sliceActions.at(1).id, QStringLiteral("volume.up"));
        QCOMPARE(t().sliceActions.at(1).steps, 4);
    }

    void levels_turnDownUsesTheDownAction()
    {
        d().turn(QStringLiteral("volume.up"), QStringLiteral("volume.down"), -2, kSpace);
        QCOMPARE(t().sliceActions.at(0).id, QStringLiteral("volume.down"));
        QCOMPARE(t().sliceActions.at(0).steps, 2);
    }

    void levels_doNotShareATuneLimit()
    {
        d().press(QStringLiteral("tune.up"), kSpace);
        d().press(QStringLiteral("volume.up"), kSpace);
        QCOMPARE(t().tunes.size(), 1);
        QCOMPARE(t().sliceActions.size(), 1);
    }

    // ---- Pan, window and transmit scope ----

    void scope_windowActionsLookUpNoSlice_data()
    {
        QTest::addColumn<QString>("id");
        for (const char* id : {"zoom.in", "zoom.out", "slice.next", "bandstack.next", "bandstack.previous",
                               "bandstack.add", "bandstack.panel"}) {
            QTest::newRow(id) << QString::fromLatin1(id);
        }
    }

    void scope_windowActionsLookUpNoSlice()
    {
        QFETCH(QString, id);
        t().window = nullptr;
        d().press(id, kSpace);
        QCOMPARE(t().windowActions, QStringList{id});
        QVERIFY(t().toasts.isEmpty());
    }

    void scope_transmitActionsIgnoreSliceRefusals_data()
    {
        QTest::addColumn<QString>("id");
        for (const char* id : {"tx.mox", "tx.tune", "tx.twotone", "vox.toggle"}) {
            QTest::newRow(id) << QString::fromLatin1(id);
        }
    }

    void scope_transmitActionsIgnoreSliceRefusals()
    {
        QFETCH(QString, id);
        m_a->setLocked(true);
        t().listenedSlices.insert(m_a.get());
        d().press(id, kSpace);
        QCOMPARE(t().transmitActions, QStringList{id});
        QVERIFY(t().toasts.isEmpty());
    }

    void scope_releaseOfALatchedActionDoesNothing()
    {
        d().press(QStringLiteral("tx.mox"), kSpace);
        d().release(QStringLiteral("tx.mox"), kSpace);
        QCOMPARE(t().transmitActions.size(), 1);
        QVERIFY(t().keyHolds.isEmpty());
    }

    // ---- A knob on something other than a level or tune ----

    void turn_pressActionRunsOncePerStep()
    {
        d().turn(QStringLiteral("zoom.in"), QStringLiteral("zoom.out"), -3, kSpace);
        QCOMPARE(t().windowActions.size(), 3);
        QCOMPARE(t().windowActions.at(0), QStringLiteral("zoom.out"));
    }

    void turn_refusedActionToastsOnce()
    {
        m_a->setLocked(true);
        d().turn(QStringLiteral("band.next"), QStringLiteral("band.previous"), 4, kSpace);
        QCOMPARE(t().toasts.size(), 1);
    }

    void turn_neverKeys_data()
    {
        QTest::addColumn<QString>("id");
        for (const char* id : {"tx.hold", "tx.mox", "vox.toggle", "cw.dot", "cw.msg1"}) {
            QTest::newRow(id) << QString::fromLatin1(id);
        }
    }

    void turn_neverKeys()
    {
        QFETCH(QString, id);
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("A knob can't run a transmit action")));
        d().turn(id, id, 2, kSpace);
        QCOMPARE(t().anythingRan(), 0);
    }

    void unknownAction_isIgnoredWithAWarning()
    {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Unknown hot key action")));
        d().press(QStringLiteral("no.such"), kSpace);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Unknown hot key action")));
        d().turn(QStringLiteral("no.such"), QStringLiteral("no.such"), 1, kSpace);
        d().release(QStringLiteral("no.such"), kSpace);
        QCOMPARE(t().anyCall, 0);
    }
};

QTEST_MAIN(TestHotKeyDispatcher)
#include "tst_hot_key_dispatcher.moc"
