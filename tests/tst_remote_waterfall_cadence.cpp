// tests/tst_remote_waterfall_cadence.cpp
//
// no-port-check: NereusSDR-original. The remote waterfall's row cadence;
// Thetis has no remote display, so there is no upstream source to cite.
//
// Modification history (NereusSDR):
//   2026-10-22  J.J. Boyd (KG4VCF), AI-assisted via Claude Code: the
//                 NEREUS_TRACE_REMOTE_ROWS trace logs a starve when set and
//                 nothing when unset; the ticker test stops its thread even
//                 when it fails, so a failure is reported and not a fatal
//                 "QThread: Destroyed while thread is still running".
//   2026-10-21  J.J. Boyd (KG4VCF), AI-assisted via Claude Code: the
//                 ticker and glide at the Core's row period; the row clock's
//                 grid locked to the rows' due times; then the review's fix
//                 wave: rows bound by their hand-off follow the Core's
//                 drift, a stall plays out gliding, a deep hold's rows all
//                 stay queued, rows due after the transmit view lets go are
//                 kept, and a drawn frame takes at most two rows.
//
// A remote pan asks the Core for ceil(update period x fps / 1000) frames a
// row, so a 42 ms update period at 30 fps gets a row every 66.67 ms. The
// waterfall ticker, which takes one queued row a tick, ran at the 42 ms
// update period: one tick in three found no row, and the waterfall moved,
// moved and paused several times a second (JJ's window, 2026-10-21). These
// pin the ticker, the scroll glide and the time scale to the Core's row
// period on a remote pan, and the ticker's slots to a drift-free cadence
// for a period that is not whole milliseconds.
//
// The row clock (RemoteWaterfallRowClock): a recording of that build still
// showed the waterfall stop about 50 ms once a second and jump most of a
// row after, because the glide ran from when each row reached the main
// thread. These run the clock against rows arriving with jitter and drift
// and frames at 60 Hz, as the window does.

#include "gui/SpectrumWidget.h"
#include "gui/spectrum/RemoteWaterfallRowClock.h"
#include "gui/spectrum/WaterfallTicker.h"
#include "core/session/media/SpectrumEndpoint.h"

#include <QtTest>
#include <QRegularExpression>
#include <QScopeGuard>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr qint64 kMs = 1'000'000;
// Two frames a row at 30 fps, as RemoteMediaController computes it.
constexpr qint64 kTwoFrameRowNs = 2 * 1'000'000'000LL / 30;

SpectrumEndpointContext remoteContext(int fps, int framesPerLine)
{
    SpectrumEndpointContext context;
    context.codec = {43, 1, -180.0f, 0.0f, 400, 400, 0};
    context.exactCentreHz = 14200000.0;
    context.exactSpanHz = 48000.0;
    context.targetFps = fps;
    context.framesPerLine = framesPerLine;
    return context;
}

constexpr qint64 kFrameNs = 1'000'000'000LL / 60;

// One row: when it is due on screen and when it was handed to the widget.
struct GridRow {
    qint64 dueNs{0};
    qint64 handedNs{0};
    qint64 nominalDueNs{0};   // the due time without its jitter
};

// What the clock did over a run of frames.
struct GridRun {
    int pushed{0};
    int framesWithTwoRows{0};
    int maxRowsInAFrame{0};
    // Frames after the first row went in where the glide did not move, and
    // the last of them.
    int stillFrames{0};
    qint64 lastStillNs{-1};
    // Rows that went in later than the first frame of their slide.
    int rowsInLate{0};
    // Frames that put rows in with the glide already at the full row: the
    // rows jumped in rather than glided.
    int framesPutWithoutGlide{0};
    // The largest step of the glide on one frame, in rows.
    double maxGlideStepRows{0.0};
    // Each row put in alone on its frame: its index, slot and the frame it
    // went in on.
    struct Put {
        size_t row{0};
        qint64 slotNs{0};
        qint64 atNs{0};
    };
    std::vector<Put> puts;
};

// Rows (in hand-off order) reach the queue at handedNs; a frame every
// kFrameNs from 0 to endNs advances the clock. Frames inside
// [stallFromNs, stallToNs) do not happen.
GridRun runGrid(RemoteWaterfallRowClock& clock, const std::vector<GridRow>& rows,
                qint64 endNs, qint64 stallFromNs = -1, qint64 stallToNs = -1)
{
    GridRun run;
    std::vector<size_t> queue;
    size_t next = 0;
    double lastPosition = -1.0;
    const double frameRows = double(kFrameNs) / double(clock.periodNs());
    for (qint64 now = 0; now <= endNs; now += kFrameNs) {
        while (next < rows.size() && rows[next].handedNs <= now) {
            queue.push_back(next++);
        }
        if (now >= stallFromNs && now < stallToNs) {
            continue;
        }
        const int put = queue.empty()
            ? clock.advance(now, 0, now, now)
            : clock.advance(now, int(queue.size()), rows[queue.front()].dueNs,
                            rows[queue.front()].handedNs);
        run.maxRowsInAFrame = std::max(run.maxRowsInAFrame, put);
        if (put >= 2) {
            ++run.framesWithTwoRows;
        }
        if (put > 0 && clock.progress() >= 1.0f) {
            ++run.framesPutWithoutGlide;
        }
        if (put == 1) {
            run.puts.push_back({queue.front(), clock.lastSlotNs(), now});
            // In on the first frame of its slide: the glide has come less
            // than one frame of the row.
            if (clock.progress() > frameRows * 1.05) {
                ++run.rowsInLate;
            }
        }
        queue.erase(queue.begin(), queue.begin() + put);
        run.pushed += put;
        const double position = double(run.pushed) + double(clock.progress());
        if (run.pushed > 0 && lastPosition >= 0.0) {
            if (position <= lastPosition) {
                ++run.stillFrames;
                run.lastStillNs = now;
            }
            run.maxGlideStepRows = std::max(run.maxGlideStepRows, position - lastPosition);
        }
        if (run.pushed > 0) {
            lastPosition = position;
        }
    }
    return run;
}

// A fixed pseudo-random offset in [-spanNs, spanNs] for row k.
qint64 jitterNs(qint64 k, qint64 spanNs)
{
    if (spanNs <= 0) {
        return 0;
    }
    std::uint32_t seed = 12345u + std::uint32_t(k) * 2654435761u;
    seed = seed * 1664525u + 1013904223u;
    seed ^= seed >> 13;
    return qint64(seed % std::uint32_t(2 * spanNs + 1)) - spanNs;
}

// Rows due from 1 s every periodNs (the Core's row spacing on this clock),
// each due time moved by up to dueJitterNs either way, handed over leadNs
// before it is due.
std::vector<GridRow> gridRows(qint64 periodNs, qint64 dueJitterNs, qint64 leadNs,
                              qint64 endNs)
{
    std::vector<GridRow> rows;
    for (qint64 k = 0;; ++k) {
        GridRow row;
        row.nominalDueNs = 1'000 * kMs + k * periodNs;
        row.dueNs = row.nominalDueNs + jitterNs(k, dueJitterNs);
        row.handedNs = row.dueNs - leadNs;
        if (row.handedNs > endNs) {
            break;
        }
        rows.push_back(row);
    }
    // Handed over in order: the hand-off jitter never reorders rows.
    for (size_t k = 1; k < rows.size(); ++k) {
        rows[k].handedNs = std::max(rows[k].handedNs, rows[k - 1].handedNs);
    }
    return rows;
}

// The largest |slot - due| of the rows put in alone with slots from fromNs
// on; nominal: against the due time without its jitter.
double maxSlotErrorMs(const GridRun& run, const std::vector<GridRow>& rows, qint64 fromNs,
                      bool nominal = false)
{
    double worst = 0.0;
    for (const GridRun::Put& put : run.puts) {
        if (put.slotNs < fromNs) {
            continue;
        }
        const qint64 due = nominal ? rows[put.row].nominalDueNs : rows[put.row].dueNs;
        worst = std::max(worst, std::abs(double(put.slotNs - due)) / double(kMs));
    }
    return worst;
}

} // namespace

class TstRemoteWaterfallCadence : public QObject {
    Q_OBJECT

private slots:
    void tickSlotsDoNotDriftForAFractionalPeriod()
    {
        // Each tick fires when its timer says, and arms the next from there.
        qint64 elapsedNs = 0;
        for (qint64 tick = 1; tick <= 150; ++tick) {
            const int waitMs = WaterfallTicker::msUntilTick(elapsedNs, tick, kTwoFrameRowNs);
            elapsedNs += qint64(waitMs) * kMs;
            // Never before its slot, and never a whole millisecond after it.
            QVERIFY(elapsedNs >= tick * kTwoFrameRowNs);
            QVERIFY(elapsedNs - tick * kTwoFrameRowNs < kMs);
        }
        // 150 rows of 66.67 ms are 10 s, not 150 x 67 ms = 10.05 s.
        QVERIFY(qAbs(elapsedNs - 10'000 * kMs) < kMs);
    }

    void tickAlreadyDueWaitsNothing()
    {
        QCOMPARE(WaterfallTicker::msUntilTick(70 * kMs, 1, kTwoFrameRowNs), 0);
        QCOMPARE(WaterfallTicker::msUntilTick(0, 1, 42 * kMs), 42);
    }

    void tickerKeepsItsBounds()
    {
        WaterfallTicker ticker;
        ticker.setUpdatePeriodNs(kTwoFrameRowNs);
        QCOMPARE(ticker.updatePeriodNs(), kTwoFrameRowNs);
        ticker.setUpdatePeriodMs(2);
        QCOMPARE(ticker.updatePeriodNs(), WaterfallTicker::kMinPeriodNs);
        ticker.setUpdatePeriodNs(2'000 * kMs);
        QCOMPARE(ticker.updatePeriodNs(), WaterfallTicker::kMaxPeriodNs);
    }

    void tickerKeepsTickingOnItsThread()
    {
        // The timer is single-shot now: each tick must arm the next.
        QThread thread;
        auto* ticker = new WaterfallTicker();
        ticker->moveToWorkerThread(&thread);
        connect(&thread, &QThread::finished, ticker, &QObject::deleteLater);
        QAtomicInt ticks;
        connect(ticker, &WaterfallTicker::tick, ticker, [&ticks] { ticks.ref(); },
                Qt::DirectConnection);
        thread.start();
        auto stopThread = qScopeGuard([&thread, ticker] {
            ticker->stop();
            thread.quit();
            thread.wait(5000);
        });
        ticker->setUpdatePeriodNs(WaterfallTicker::kMinPeriodNs);
        ticker->start();
        // Five ticks of 10 ms; the wait is generous for a loaded machine.
        QTRY_VERIFY_WITH_TIMEOUT(ticks.loadRelaxed() >= 5, 5000);
        stopThread.dismiss();
        ticker->stop();
        thread.quit();
        QVERIFY(thread.wait(5000));
    }

    void remotePanTicksAtTheCoresRowPeriod()
    {
        SpectrumWidget widget;
        widget.resize(420, 240);
        widget.setWfUpdatePeriodMs(42);
        QCOMPARE(widget.waterfallTickerPeriodNsForTest(), 42 * kMs);
        QCOMPARE(widget.waterfallRowPeriodMsForTest(), 42.0);

        widget.setRemoteSpectrumContext(remoteContext(30, 2), 14200000.0, 192000.0);
        QCOMPARE(widget.waterfallTickerPeriodNsForTest(), kTwoFrameRowNs);
        QVERIFY(qAbs(widget.waterfallRowPeriodMsForTest() - 66.6667) < 0.001);

        // The same picture at one frame a row: the ticker follows it.
        widget.setRemoteSpectrumContext(remoteContext(30, 1), 14200000.0, 192000.0);
        QCOMPARE(widget.waterfallTickerPeriodNsForTest(), 1'000'000'000LL / 30);

        // The update period setting no longer sets a remote pan's cadence.
        widget.setWfUpdatePeriodMs(50);
        QCOMPARE(widget.waterfallTickerPeriodNsForTest(), 1'000'000'000LL / 30);
    }

    void contextWithoutARowPeriodKeepsTheUpdatePeriod()
    {
        SpectrumWidget widget;
        widget.resize(420, 240);
        widget.setWfUpdatePeriodMs(42);
        // An older Core sends no frame rate: nothing to tick at but the pan's own.
        widget.setRemoteSpectrumContext(remoteContext(0, 0), 14200000.0, 192000.0);
        QCOMPARE(widget.waterfallTickerPeriodNsForTest(), 42 * kMs);

        widget.setRemoteSpectrumContext(remoteContext(30, 2), 14200000.0, 192000.0);
        QCOMPARE(widget.waterfallTickerPeriodNsForTest(), kTwoFrameRowNs);
        widget.clearRemoteSpectrum();
        QCOMPARE(widget.waterfallTickerPeriodNsForTest(), 42 * kMs);
    }

    void glideSpansTheCoresRowPeriod()
    {
        SpectrumWidget widget;
        widget.resize(420, 240);
        widget.setWfUpdatePeriodMs(42);
        widget.setDssRowDivider(1);
        // Local: a 42 ms push period is one whole row's glide.
        QVERIFY(qAbs(widget.dssScrollIncrementForTest(42) - 1.0f) < 1e-4f);
        widget.setRemoteSpectrumContext(remoteContext(30, 2), 14200000.0, 192000.0);
        // Remote: 42 ms is under two thirds of the way to the next row.
        QVERIFY(qAbs(widget.dssScrollIncrementForTest(42) - 0.63f) < 1e-4f);
    }

    // The grid the rows go in on (RemoteWaterfallRowClock). The cases run
    // the clock against rows as JJ's window had them (2026-10-21): handed
    // over about 200 ms before they are due, a row every 66.67 ms, frames
    // at 60 Hz, the audio delay follower stepping 15 to 40 ms, the Core's
    // clock drifting -500 to -5000 ppm against this one.

    void gridPlaysASteadyStreamOneRowASlot()
    {
        // 30 s of rows, each due time moved up to 7.5 ms either way (15 ms
        // from earliest to latest), handed over 200 ms early.
        RemoteWaterfallRowClock clock;
        clock.setPeriodNs(kTwoFrameRowNs);
        const qint64 endNs = 30'000 * kMs;
        const std::vector<GridRow> rows =
            gridRows(kTwoFrameRowNs, 7'500'000, 200 * kMs, endNs);
        const GridRun run = runGrid(clock, rows, endNs);
        // Every row goes in on the first frame of its slide, one a slot.
        QCOMPARE(run.rowsInLate, 0);
        QCOMPARE(run.framesWithTwoRows, 0);
        QCOMPARE(run.framesPutWithoutGlide, 0);
        // The glide moves on every frame and never rests.
        QCOMPARE(run.stillFrames, 0);
        QCOMPARE(clock.starvedSlots(), 0);
        QCOMPARE(clock.reanchors(), 0);
        // The first row's slot is its due time; after 2 s every slot is
        // within 10 ms of its row's due time (the due times themselves
        // wander 7.5 ms about a steady grid).
        QCOMPARE(run.puts.front().slotNs, rows.front().dueNs);
        const double worst = maxSlotErrorMs(run, rows, rows.front().dueNs + 2'000 * kMs);
        QVERIFY2(worst < 10.0, qPrintable(QString::number(worst)));
        QVERIFY(run.pushed >= int(rows.size()) - 4);
    }

    void gridFollowsAStepOfTheAudioDelay()
    {
        // The delay follower steps 20 ms at row 150 (10 s in), either way.
        for (const qint64 stepNs : {20 * kMs, -20 * kMs}) {
            RemoteWaterfallRowClock clock;
            clock.setPeriodNs(kTwoFrameRowNs);
            const qint64 endNs = 30'000 * kMs;
            std::vector<GridRow> rows =
                gridRows(kTwoFrameRowNs, 7'500'000, 200 * kMs, endNs);
            for (size_t k = 150; k < rows.size(); ++k) {
                rows[k].dueNs += stepNs;
                rows[k].nominalDueNs += stepNs;
                rows[k].handedNs += stepNs;
            }
            std::sort(rows.begin(), rows.end(), [](const GridRow& a, const GridRow& b) {
                return a.handedNs < b.handedNs;
            });
            const GridRun run = runGrid(clock, rows, endNs);
            // Taken up over many rows, never as one jump: no frame's glide
            // step is more than twice a steady frame's.
            const double steadyStep = double(kFrameNs) / double(kTwoFrameRowNs);
            QVERIFY2(run.maxGlideStepRows <= 2.0 * steadyStep,
                     qPrintable(QString::number(run.maxGlideStepRows)));
            QCOMPARE(run.stillFrames, 0);
            QCOMPARE(run.framesWithTwoRows, 0);
            QCOMPARE(clock.reanchors(), 0);
            // 5 s after the step the grid is within 5 ms of the new due
            // times. Measured against the due times without their jitter:
            // each row's own due time wanders 7.5 ms about them, which a
            // grid that never jumps cannot follow row by row.
            const qint64 afterNs = rows[150].nominalDueNs + 5'000 * kMs;
            const double worst = maxSlotErrorMs(run, rows, afterNs, true);
            QVERIFY2(worst < 5.0, qPrintable(QString::number(worst)));
        }
    }

    void gridFollowsTheCoresClockDrift()
    {
        // The Core's rows 0.5% long, then 0.5% short, on this clock, for
        // 120 s: the rate takes it up.
        for (const double drift : {0.005, -0.005}) {
            RemoteWaterfallRowClock clock;
            clock.setPeriodNs(kTwoFrameRowNs);
            const qint64 endNs = 120'000 * kMs;
            const qint64 spacing = qint64(double(kTwoFrameRowNs) * (1.0 + drift));
            const std::vector<GridRow> rows = gridRows(spacing, 7'500'000, 200 * kMs, endNs);
            const GridRun run = runGrid(clock, rows, endNs);
            QCOMPARE(clock.starvedSlots(), 0);
            QCOMPARE(run.framesWithTwoRows, 0);
            QCOMPARE(run.stillFrames, 0);
            const double worst = maxSlotErrorMs(run, rows, rows.front().dueNs + 10'000 * kMs);
            QVERIFY2(worst < 10.0, qPrintable(QString::number(worst)));
            // Slower for rows spaced long, faster for rows spaced short.
            QVERIFY(drift > 0 ? clock.rate() < 1.0 : clock.rate() > 1.0);
        }
    }

    void gridKeepsItsRateThroughLateHandOffs()
    {
        // Fix wave, second review: rows handed over as they are due (no
        // lead, 5.5 ms of jitter), from a Core whose rows are 0.5% long on
        // this clock, one hand-off in 150 coming 20 ms late, for 300 s. The
        // late row finds its slide started and the glide resting; it goes
        // in on the later of a row period on and its hand-off-bound slot,
        // and the rate is kept. Starting the grid again there reset the
        // rate to 1 on most such rows.
        RemoteWaterfallRowClock clock;
        clock.setPeriodNs(kTwoFrameRowNs);
        const qint64 endNs = 300'000 * kMs;
        const qint64 spacing = qint64(double(kTwoFrameRowNs) * 1.005);
        std::vector<GridRow> rows = gridRows(spacing, 5'500'000, 0, endNs);
        for (size_t k = 149; k < rows.size(); k += 150) {
            rows[k].dueNs += 20 * kMs;
            rows[k].handedNs += 20 * kMs;
        }
        std::sort(rows.begin(), rows.end(), [](const GridRow& a, const GridRow& b) {
            return a.handedNs < b.handedNs;
        });
        std::vector<size_t> queue;
        size_t next = 0;
        int pushed = 0;
        int starvedSeen = 0;
        bool resting = false;
        int starvedRowsIn = 0;
        double worstRateStep = 0.0;
        for (qint64 now = 0; now <= endNs; now += kFrameNs) {
            while (next < rows.size() && rows[next].handedNs <= now) {
                queue.push_back(next++);
            }
            const double rateBefore = clock.rate();
            const int put = queue.empty()
                ? clock.advance(now, 0, now, now)
                : clock.advance(now, int(queue.size()), rows[queue.front()].dueNs,
                                rows[queue.front()].handedNs);
            if (resting && put > 0) {
                // The row the glide rested for goes in: the rate learnt
                // against the Core's rows is kept.
                ++starvedRowsIn;
                worstRateStep = std::max(worstRateStep, std::abs(clock.rate() - rateBefore));
                resting = false;
            }
            if (clock.starvedSlots() != starvedSeen) {
                starvedSeen = clock.starvedSlots();
                resting = true;
            }
            queue.erase(queue.begin(), queue.begin() + put);
            pushed += put;
        }
        // The late hand-offs did starve slots: the case is exercised.
        QVERIFY2(starvedRowsIn >= 3, qPrintable(QString::number(starvedRowsIn)));
        QCOMPARE(clock.reanchors(), 0);
        QVERIFY2(worstRateStep < 0.001, qPrintable(QString::number(worstRateStep, 'f', 5)));
        // Slower for the Core's rows spaced long.
        QVERIFY(clock.rate() < 0.997);
        QVERIFY(pushed >= int(rows.size()) - 3);
    }

    void gridStartsAgainWhenTheHoldGrows()
    {
        // The audio's hold grows 400 ms at row 150: the grid starts again
        // at that row rather than slewing toward it for seconds, and the
        // waterfall rests meanwhile rather than play rows early.
        RemoteWaterfallRowClock clock;
        clock.setPeriodNs(kTwoFrameRowNs);
        const qint64 endNs = 20'000 * kMs;
        std::vector<GridRow> rows = gridRows(kTwoFrameRowNs, 7'500'000, 200 * kMs, endNs);
        for (size_t k = 150; k < rows.size(); ++k) {
            rows[k].dueNs += 400 * kMs;
            rows[k].nominalDueNs += 400 * kMs;
            rows[k].handedNs += 400 * kMs;
        }
        const GridRun run = runGrid(clock, rows, endNs);
        QCOMPARE(clock.reanchors(), 1);
        QCOMPARE(run.framesWithTwoRows, 0);
        // No row's slot is before its due time by more than the grid's
        // own wander.
        for (const GridRun::Put& put : run.puts) {
            QVERIFY2(put.slotNs >= rows[put.row].dueNs - 10 * kMs,
                     qPrintable(QString::number(put.row)));
        }
        // Row 150 starts the grid at its own due time, and the rows after
        // it are on their due times at once.
        const auto first = std::find_if(run.puts.cbegin(), run.puts.cend(),
                                        [](const GridRun::Put& put) { return put.row == 150; });
        QVERIFY(first != run.puts.cend());
        QCOMPARE(first->slotNs, rows[150].dueNs);
        const double worst = maxSlotErrorMs(run, rows, rows[150].dueNs);
        QVERIFY2(worst < 10.0, qPrintable(QString::number(worst)));
    }

    void gridRunsARowBehindRowsWithNoDueTime()
    {
        // An older Core sends no due times: each row is due as it reaches
        // the widget, with the main thread's hand-off up to 5.5 ms either
        // way (11 ms from earliest to latest). The grid runs about a row
        // behind, one row a slot, the glide moving on every frame.
        RemoteWaterfallRowClock clock;
        clock.setPeriodNs(kTwoFrameRowNs);
        const qint64 endNs = 30'000 * kMs;
        const std::vector<GridRow> rows = gridRows(kTwoFrameRowNs, 5'500'000, 0, endNs);
        const GridRun run = runGrid(clock, rows, endNs);
        // The first rows find the grid's margin for the hand-off's jitter:
        // the glide may rest once.
        QVERIFY(clock.starvedSlots() <= 1);
        QCOMPARE(run.framesWithTwoRows, 0);
        QCOMPARE(clock.reanchors(), 0);
        // After 3 s: never still, and each slot a row period (plus the
        // jitter's margin) after its row came, steadily.
        QVERIFY2(run.lastStillNs < 3'000 * kMs, qPrintable(QString::number(run.lastStillNs)));
        double minLateMs = 1e9;
        double maxLateMs = 0.0;
        for (const GridRun::Put& put : run.puts) {
            if (put.atNs < 3'000 * kMs) {
                continue;
            }
            const double lateMs = double(put.slotNs - rows[put.row].dueNs) / double(kMs);
            minLateMs = std::min(minLateMs, lateMs);
            maxLateMs = std::max(maxLateMs, lateMs);
        }
        QVERIFY2(minLateMs >= 66.0 && maxLateMs < 66.67 + 30.0,
                 qPrintable(QStringLiteral("%1 %2").arg(minLateMs).arg(maxLateMs)));
        // No oscillation: the lateness stays inside the hand-off's jitter
        // plus a frame.
        QVERIFY2(maxLateMs - minLateMs < 11.0 + 16.7,
                 qPrintable(QStringLiteral("%1 %2").arg(minLateMs).arg(maxLateMs)));
    }

    void gridFollowsDriftForRowsBoundByTheirHandOff_data()
    {
        QTest::addColumn<double>("drift");
        QTest::addColumn<qint64>("leadNs");
        QTest::newRow("no due time, Core rows long") << 0.005 << qint64(0);
        QTest::newRow("no due time, Core rows short") << -0.005 << qint64(0);
        QTest::newRow("due inside a row period, Core rows long") << 0.005 << qint64(20 * kMs);
        QTest::newRow("due inside a row period, Core rows short")
            << -0.005 << qint64(20 * kMs);
    }

    void gridFollowsDriftForRowsBoundByTheirHandOff()
    {
        // Fix wave finding 2: rows that cannot go in by their due time (no
        // due time from an older Core, or due less than a row period after
        // they were handed over) still teach the rate the Core's clock,
        // either way. With their early errors held at zero the rate could
        // only fall: a faster Core's rows piled up and the grid started
        // again with a stop and a jump about every 30 s. 300 s of rows
        // 0.5% long, then 0.5% short, handed over with up to 5.5 ms of
        // jitter.
        QFETCH(double, drift);
        QFETCH(qint64, leadNs);
        RemoteWaterfallRowClock clock;
        clock.setPeriodNs(kTwoFrameRowNs);
        const qint64 endNs = 300'000 * kMs;
        const qint64 spacing = qint64(double(kTwoFrameRowNs) * (1.0 + drift));
        const std::vector<GridRow> rows = gridRows(spacing, 5'500'000, leadNs, endNs);
        const GridRun run = runGrid(clock, rows, endNs);
        QCOMPARE(clock.reanchors(), 0);
        QCOMPARE(run.framesWithTwoRows, 0);
        QVERIFY(clock.starvedSlots() <= 1);
        // After 3 s the glide moves on every frame.
        QVERIFY2(run.lastStillNs < 3'000 * kMs, qPrintable(QString::number(run.lastStillNs)));
        QVERIFY(run.pushed >= int(rows.size()) - 3);
        // Slower for rows spaced long, faster for rows spaced short.
        QVERIFY2(drift > 0 ? clock.rate() < 1.0 : clock.rate() > 1.0,
                 qPrintable(QString::number(clock.rate(), 'f', 5)));
        // Each slot a row period and the margin after its row was handed
        // over, steadily: within the hand-off's jitter and a frame.
        double minLateMs = 1e9;
        double maxLateMs = 0.0;
        for (const GridRun::Put& put : run.puts) {
            if (put.atNs < 10'000 * kMs) {
                continue;
            }
            const double lateMs =
                double(put.slotNs - rows[put.row].handedNs) / double(kMs);
            minLateMs = std::min(minLateMs, lateMs);
            maxLateMs = std::max(maxLateMs, lateMs);
        }
        QVERIFY2(minLateMs >= 66.0 && maxLateMs - minLateMs < 11.0 + 16.7,
                 qPrintable(QStringLiteral("%1 %2").arg(minLateMs).arg(maxLateMs)));
    }

    void gridRestsOnceForRowsThatComeLate()
    {
        // The link stalls: nothing is handed over from 5.0 s to 5.3 s, so
        // the rows due from 5.2 s (handed 200 ms early, as ever) come
        // together at 5.3 s, up to 100 ms after they were due. The glide
        // rests once, then the rows play one a slot and the grid locks back
        // onto the due times.
        RemoteWaterfallRowClock clock;
        clock.setPeriodNs(kTwoFrameRowNs);
        const qint64 endNs = 20'000 * kMs;
        std::vector<GridRow> rows = gridRows(kTwoFrameRowNs, 0, 200 * kMs, endNs);
        for (GridRow& row : rows) {
            if (row.handedNs > 5'000 * kMs && row.handedNs < 5'300 * kMs) {
                row.handedNs = 5'300 * kMs;
            }
        }
        const GridRun run = runGrid(clock, rows, endNs);
        QCOMPARE(clock.starvedSlots(), 1);
        QVERIFY2(run.stillFrames > 0 && run.stillFrames < 12,
                 qPrintable(QString::number(run.stillFrames)));
        QCOMPARE(run.framesWithTwoRows, 0);
        QCOMPARE(clock.reanchors(), 0);
        const double worst = maxSlotErrorMs(run, rows, 12'000 * kMs);
        QVERIFY2(worst < 5.0, qPrintable(QString::number(worst)));
    }

    void gridPlaysAStallOutTwoRowsAFrame()
    {
        // No frame for 400 ms (the main thread held): the next frames put
        // the six rows due in at most two at a time.
        RemoteWaterfallRowClock clock;
        clock.setPeriodNs(kTwoFrameRowNs);
        const qint64 endNs = 8'000 * kMs;
        const std::vector<GridRow> rows = gridRows(kTwoFrameRowNs, 0, 200 * kMs, endNs);
        const GridRun run = runGrid(clock, rows, endNs, 3'000 * kMs, 3'400 * kMs);
        QCOMPARE(run.maxRowsInAFrame, 2);
        QVERIFY(run.framesWithTwoRows >= 1 && run.framesWithTwoRows <= 3);
        // Gliding: every frame that puts rows in, the play-out after the
        // stall too, starts a glide rather than jumping the rows in at the
        // full row (fix wave finding 5).
        QCOMPARE(run.framesPutWithoutGlide, 0);
        QVERIFY2(run.maxGlideStepRows <= 2.0,
                 qPrintable(QString::number(run.maxGlideStepRows)));
        QCOMPARE(clock.starvedSlots(), 0);
        QCOMPARE(clock.reanchors(), 0);
        const double worst = maxSlotErrorMs(run, rows, 4'000 * kMs);
        QVERIFY2(worst < 1.0, qPrintable(QString::number(worst)));
    }

    void gridStopsAndStartsAgain()
    {
        RemoteWaterfallRowClock clock;
        // Stopped: nothing goes in.
        QCOMPARE(clock.advance(10 * kTwoFrameRowNs, 5, 0, 0), 0);
        clock.setPeriodNs(kTwoFrameRowNs);
        // The first row starts the grid at its due time: it slides in from
        // a row period before.
        const qint64 due = 500 * kMs;
        QCOMPARE(clock.advance(due - kTwoFrameRowNs - kMs, 1, due, 0), 0);
        QCOMPARE(clock.progress(), 1.0f);
        QCOMPARE(clock.advance(due - kTwoFrameRowNs, 1, due, 0), 1);
        QCOMPARE(clock.lastSlotNs(), due);
        QCOMPARE(clock.progress(), 0.0f);
        // A reset or a new period: the next row starts the grid again.
        clock.reset();
        QVERIFY(!clock.anchored());
        QCOMPARE(clock.advance(2 * due - kTwoFrameRowNs, 1, 2 * due, 0), 1);
        QCOMPARE(clock.lastSlotNs(), 2 * due);
        clock.setPeriodNs(1'000'000'000LL / 30);
        QVERIFY(!clock.anchored());
    }

    void remotePanKeepsEveryRowOfADeepHold()
    {
        // Fix wave finding 1: rows go over about a hold before they are
        // due, so a 1.2 s audio hold at a 16.67 ms row period has 72 rows
        // queued at once. A queue of 32 dropped the row due soonest, every
        // time, until none was left to go in. The queue holds a full hold's
        // rows; each goes in on its own slot. Frames at 120 Hz.
        SpectrumWidget widget;
        widget.resize(420, 240);
        widget.setWaterfallTickerPausedForTest(true);
        const SpectrumEndpointContext context = remoteContext(60, 1);
        widget.setRemoteSpectrumContext(context, 14200000.0, 192000.0);
        const qint64 periodNs = 1'000'000'000LL / 60;
        QCOMPARE(widget.remoteRowClockForTest().periodNs(), periodNs);
        const RemoteSpectrumCapture capture{context, 14200000.0, 192000.0};
        const QVector<float> row(400, -100.0f);
        constexpr qint64 kHoldNs = 1'200 * kMs;
        constexpr qint64 kFrame120Ns = 1'000'000'000LL / 120;
        const qint64 endNs = 5'000 * kMs;
        const quint64 droppedBefore = widget.remoteRowsDropped();
        const quint64 pushedBefore = widget.remoteRowsPushedForTest();
        qint64 handed = 0;
        int framesWithTwo = 0;
        int deepest = 0;
        for (qint64 now = 0; now <= endNs; now += kFrame120Ns) {
            widget.setRemoteRowNowNsForTest(now);
            while (handed * periodNs <= now) {
                const qint64 dueNs = handed * periodNs + kHoldNs;
                QVERIFY(widget.enqueueRemoteWaterfallRow(row, {}, capture, dueNs - now));
                ++handed;
            }
            deepest = std::max(deepest, widget.remoteRowQueueDepthForTest());
            const quint64 before = widget.remoteRowsPushedForTest();
            widget.pumpRemoteWaterfallForTest();
            if (widget.remoteRowsPushedForTest() - before > 1) {
                ++framesWithTwo;
            }
        }
        QCOMPARE(widget.remoteRowsDropped(), droppedBefore);
        QCOMPARE(framesWithTwo, 0);
        // Every row whose slide has started by the end is in.
        const qint64 dueByEnd = (endNs + periodNs - kHoldNs) / periodNs + 1;
        const qint64 pushed = qint64(widget.remoteRowsPushedForTest() - pushedBefore);
        QVERIFY2(pushed >= dueByEnd - 1 && pushed <= dueByEnd + 1,
                 qPrintable(QStringLiteral("%1 of %2").arg(pushed).arg(dueByEnd)));
        QCOMPARE(widget.remoteRowClockForTest().reanchors(), 0);
        QCOMPARE(widget.remoteRowClockForTest().starvedSlots(), 0);
        // The hold's rows were all queued at once.
        QVERIFY2(deepest >= 72, qPrintable(QString::number(deepest)));
    }

    void remoteRowsDueAfterTheTransmitViewLetsGoAreKept()
    {
        // Fix wave finding 3: while the transmit view owns the waterfall no
        // receive row goes in, and the rows that fall due meanwhile are
        // dropped. Rows handed over after the un-key and due after the
        // transmit view lets go are the receive that follows: they stay
        // and go in on their slots.
        SpectrumWidget widget;
        widget.resize(420, 240);
        widget.setWaterfallTickerPausedForTest(true);
        const SpectrumEndpointContext context = remoteContext(30, 2);
        widget.setRemoteSpectrumContext(context, 14200000.0, 192000.0);
        const RemoteSpectrumCapture capture{context, 14200000.0, 192000.0};
        const QVector<float> row(400, -100.0f);
        const quint64 pushedBefore = widget.remoteRowsPushedForTest();
        widget.setRemoteRowNowNsForTest(0);
        for (const qint64 dueMs : {100, 200, 300, 400}) {
            QVERIFY(widget.enqueueRemoteWaterfallRow(row, {}, capture, dueMs * kMs));
        }
        widget.setTxExternalWaterfall(true);
        widget.setRemoteRowNowNsForTest(150 * kMs);
        widget.pumpRemoteWaterfallForTest();
        widget.tickRemoteWaterfallPumpForTest();
        QCOMPARE(widget.remoteRowsPushedForTest(), pushedBefore);
        QCOMPARE(widget.remoteRowQueueDepthForTest(), 3);
        QVERIFY(!widget.remoteRowClockForTest().anchored());
        widget.setRemoteRowNowNsForTest(250 * kMs);
        widget.pumpRemoteWaterfallForTest();
        QCOMPARE(widget.remoteRowsPushedForTest(), pushedBefore);
        QCOMPARE(widget.remoteRowQueueDepthForTest(), 2);
        // It lets go at 320 ms: the row due at 300 ms fell due under it and
        // is dropped, though no pump ran in between.
        widget.setRemoteRowNowNsForTest(320 * kMs);
        widget.setTxExternalWaterfall(false);
        QCOMPARE(widget.remoteRowQueueDepthForTest(), 1);
        widget.pumpRemoteWaterfallForTest();
        QCOMPARE(widget.remoteRowsPushedForTest(), pushedBefore);
        // The row due at 400 ms slides in from a row period before.
        widget.setRemoteRowNowNsForTest(400 * kMs - kTwoFrameRowNs);
        widget.pumpRemoteWaterfallForTest();
        QCOMPARE(widget.remoteRowsPushedForTest(), pushedBefore + 1);
        QCOMPARE(widget.remoteRowClockForTest().lastSlotNs(), 400 * kMs);
    }

    void aDrawnFrameTakesAtMostTwoRows()
    {
        // Fix wave finding 7: the ticker's pump and the drawn frame's both
        // advance the clock. Two rows is a drawn frame's budget across
        // both: the rows the ticks put in since the last drawn frame show
        // first on the next one, and come off its budget.
        SpectrumWidget widget;
        widget.resize(420, 240);
        widget.setWaterfallTickerPausedForTest(true);
        const SpectrumEndpointContext context = remoteContext(30, 2);
        widget.setRemoteSpectrumContext(context, 14200000.0, 192000.0);
        const RemoteSpectrumCapture capture{context, 14200000.0, 192000.0};
        const QVector<float> row(400, -100.0f);
        widget.setRemoteRowNowNsForTest(0);
        for (int k = 0; k < 20; ++k) {
            QVERIFY(widget.enqueueRemoteWaterfallRow(row, {}, capture,
                                                     100 * kMs + k * kTwoFrameRowNs));
        }
        const auto pushed = [&widget, before = widget.remoteRowsPushedForTest()] {
            return int(widget.remoteRowsPushedForTest() - before);
        };
        const qint64 startNs = 100 * kMs - kTwoFrameRowNs;
        widget.setRemoteRowNowNsForTest(startNs);
        widget.pumpRemoteWaterfallForTest();
        QCOMPARE(pushed(), 1);
        // A frame a moment on takes none: the next row's slide has not
        // started.
        widget.setRemoteRowNowNsForTest(startNs + kMs);
        widget.pumpRemoteWaterfallForTest();
        QCOMPARE(pushed(), 1);
        // No frame for six row periods, then a tick: rows are far behind
        // the grid, and the tick takes two.
        const qint64 behindNs = 100 * kMs + 6 * kTwoFrameRowNs;
        widget.setRemoteRowNowNsForTest(behindNs);
        widget.tickRemoteWaterfallPumpForTest();
        QCOMPARE(pushed(), 3);
        // The next drawn frame shows the tick's two: its budget is spent.
        widget.setRemoteRowNowNsForTest(behindNs + kMs);
        widget.pumpRemoteWaterfallForTest();
        QCOMPARE(pushed(), 3);
        // The frame after takes its own two.
        widget.setRemoteRowNowNsForTest(behindNs + kMs + kFrameNs);
        widget.pumpRemoteWaterfallForTest();
        QCOMPARE(pushed(), 5);
    }

    void ticksQueuedBehindAStallTakeAtMostTwoRows()
    {
        // Fix wave, second review: the ticker posts its ticks queued and
        // does not coalesce them. After a 400 ms main-thread stall the six
        // ticks posted meanwhile land back to back on one steady time:
        // they share one budget of two rows, not two rows each. A pan
        // that is not drawing still takes two rows a tick, a row period
        // apart, and catches up.
        SpectrumWidget widget;
        widget.resize(420, 240);
        widget.setWaterfallTickerPausedForTest(true);
        const SpectrumEndpointContext context = remoteContext(30, 2);
        widget.setRemoteSpectrumContext(context, 14200000.0, 192000.0);
        const RemoteSpectrumCapture capture{context, 14200000.0, 192000.0};
        const QVector<float> row(400, -100.0f);
        widget.setRemoteRowNowNsForTest(0);
        for (int k = 0; k < 20; ++k) {
            QVERIFY(widget.enqueueRemoteWaterfallRow(row, {}, capture,
                                                     100 * kMs + k * kTwoFrameRowNs));
        }
        const auto pushed = [&widget, before = widget.remoteRowsPushedForTest()] {
            return int(widget.remoteRowsPushedForTest() - before);
        };
        widget.setRemoteRowNowNsForTest(100 * kMs - kTwoFrameRowNs);
        widget.pumpRemoteWaterfallForTest();
        QCOMPARE(pushed(), 1);
        const qint64 stallEndNs = 100 * kMs - kTwoFrameRowNs + 400 * kMs;
        widget.setRemoteRowNowNsForTest(stallEndNs);
        for (int tick = 0; tick < 6; ++tick) {
            widget.tickRemoteWaterfallPumpForTest();
        }
        QCOMPARE(pushed(), 3);
        // The pan is not drawing: a tick a row period on takes two more,
        // and the next.
        widget.setRemoteRowNowNsForTest(stallEndNs + kTwoFrameRowNs);
        widget.tickRemoteWaterfallPumpForTest();
        QCOMPARE(pushed(), 5);
        // A tick a little early (the ticker's own jitter) still has a
        // budget of its own.
        widget.setRemoteRowNowNsForTest(stallEndNs + 2 * kTwoFrameRowNs - 2 * kMs);
        widget.tickRemoteWaterfallPumpForTest();
        QCOMPARE(pushed(), 7);
    }

    void remotePanPutsEarlyRowsInOnTheGrid()
    {
        SpectrumWidget widget;
        widget.resize(420, 240);
        widget.setWaterfallTickerPausedForTest(true);
        widget.setWfUpdatePeriodMs(42);
        QVERIFY(!widget.remoteRowClockForTest().running());
        const SpectrumEndpointContext context = remoteContext(30, 2);
        widget.setRemoteSpectrumContext(context, 14200000.0, 192000.0);
        QCOMPARE(widget.remoteRowClockForTest().periodNs(), kTwoFrameRowNs);
        const RemoteSpectrumCapture capture{context, 14200000.0, 192000.0};

        const QVector<float> row(400, -100.0f);
        const quint64 pushedBefore = widget.remoteRowsPushedForTest();
        // Handed over at 0, due at 200 ms.
        widget.setRemoteRowNowNsForTest(0);
        QVERIFY(widget.enqueueRemoteWaterfallRow(row, {}, capture, 200 * kMs, 7u));
        // Before its slide starts (a row period before it is due): not in.
        widget.setRemoteRowNowNsForTest(200 * kMs - kTwoFrameRowNs - kMs);
        widget.pumpRemoteWaterfallForTest();
        QCOMPARE(widget.remoteRowsPushedForTest(), pushedBefore);
        QCOMPARE(widget.remoteRowClockForTest().progress(), 1.0f);
        widget.setRemoteRowNowNsForTest(200 * kMs - kTwoFrameRowNs);
        widget.pumpRemoteWaterfallForTest();
        QCOMPARE(widget.remoteRowsPushedForTest(), pushedBefore + 1);
        QCOMPARE(widget.remoteRowClockForTest().lastSlotNs(), 200 * kMs);
        QCOMPARE(widget.remoteRowClockForTest().progress(), 0.0f);
        // Half way at half a row on, and the whole row at its slot.
        widget.setRemoteRowNowNsForTest(200 * kMs - kTwoFrameRowNs / 2);
        widget.pumpRemoteWaterfallForTest();
        QVERIFY(qAbs(widget.remoteRowClockForTest().progress() - 0.5f) < 0.01f);
        // The next row, handed over at 150 ms, due a row after the first;
        // a tune by a quarter of the span before it goes in.
        QVector<float> split(400, -140.0f);
        for (int x = 200; x < 400; ++x) {
            split[x] = -60.0f;
        }
        widget.setRemoteRowNowNsForTest(150 * kMs);
        QVERIFY(widget.enqueueRemoteWaterfallRow(split, {}, capture,
                                                 50 * kMs + kTwoFrameRowNs, 8u));
        widget.setCenterFrequency(14200000.0 + 12000.0);
        widget.setRemoteRowNowNsForTest(200 * kMs);
        widget.pumpRemoteWaterfallForTest();
        QCOMPARE(widget.remoteRowClockForTest().progress(), 0.0f);
        QCOMPARE(widget.remoteRowsPushedForTest(), pushedBefore + 2);
        QCOMPARE(widget.remoteRowClockForTest().lastSlotNs(), 200 * kMs + kTwoFrameRowNs);
        // Drawn at the frequency it was captured at: the strong half moved
        // a quarter of the width left.
        const QVector<float> drawn = widget.lastRemoteRowPushedForTest();
        QCOMPARE(drawn.size(), 400);
        QVERIFY(std::abs(drawn.at(90) + 140.0f) < 1.0f);
        QVERIFY(std::abs(drawn.at(110) + 60.0f) < 1.0f);
        QVERIFY(std::abs(drawn.at(290) + 60.0f) < 1.0f);

        // Rows handed over early and not yet due are forgotten with the
        // presenter's items (a key); a row already due stays.
        widget.setRemoteRowNowNsForTest(300 * kMs);
        QVERIFY(widget.enqueueRemoteWaterfallRow(row, {}, capture, -10 * kMs, 9u));
        QVERIFY(widget.enqueueRemoteWaterfallRow(row, {}, capture, 150 * kMs, 10u));
        QVERIFY(widget.enqueueRemoteWaterfallRow(row, {}, capture, 220 * kMs, 11u));
        QCOMPARE(widget.forgetEarlyRemoteRows(), 2);
        QCOMPARE(widget.remoteRowQueueDepthForTest(), 1);
        QVERIFY(!widget.remoteRowClockForTest().anchored());

        widget.clearRemoteSpectrum();
        QVERIFY(!widget.remoteRowClockForTest().running());
    }

    void remoteRowTraceLogsAStarveOnlyWhenSet()
    {
        // NEREUS_TRACE_REMOTE_ROWS=1, read when the widget is built: a row
        // anchors the grid, the clock reaches the next slot with no row in
        // hand and the trace says so. Unset, the same run logs nothing.
        const QByteArray saved = qgetenv("NEREUS_TRACE_REMOTE_ROWS");
        const bool wasSet = qEnvironmentVariableIsSet("NEREUS_TRACE_REMOTE_ROWS");
        const auto restore = [&] {
            if (wasSet) {
                qputenv("NEREUS_TRACE_REMOTE_ROWS", saved);
            } else {
                qunsetenv("NEREUS_TRACE_REMOTE_ROWS");
            }
        };
        const SpectrumEndpointContext context = remoteContext(30, 2);
        const RemoteSpectrumCapture capture{context, 14200000.0, 192000.0};
        const QVector<float> row(400, -100.0f);
        const auto run = [&](SpectrumWidget& widget) {
            widget.resize(420, 240);
            widget.setWaterfallTickerPausedForTest(true);
            widget.setRemoteRowNowNsForTest(0);
            widget.setRemoteSpectrumContext(context, 14200000.0, 192000.0);
            QVERIFY(widget.enqueueRemoteWaterfallRow(row, {}, capture, 100 * kMs, 7u));
            widget.setRemoteRowNowNsForTest(100 * kMs - kTwoFrameRowNs);
            widget.pumpRemoteWaterfallForTest();
            QCOMPARE(widget.remoteRowClockForTest().lastSlotNs(), 100 * kMs);
            widget.setRemoteRowNowNsForTest(101 * kMs);
            widget.pumpRemoteWaterfallForTest();
            QCOMPARE(widget.remoteRowClockForTest().starvedSlots(), 1);
        };

        qputenv("NEREUS_TRACE_REMOTE_ROWS", "1");
        {
            SpectrumWidget widget;
            restore();
            QTest::ignoreMessage(QtInfoMsg, QRegularExpression(
                QStringLiteral("^REMOTE_ROWS pan=\\d+ REANCHOR .*reason=period-change ")));
            QTest::ignoreMessage(QtInfoMsg, QRegularExpression(
                QStringLiteral("^REMOTE_ROWS pan=\\d+ REANCHOR now=33\\.3 reason=first-row"
                               " error=0\\.0 slot=100\\.0 queue=1 ")));
            QTest::ignoreMessage(QtInfoMsg, QRegularExpression(
                QStringLiteral("^REMOTE_ROWS pan=\\d+ PAINT_GAP now=101\\.0 gap=67\\.7 ")));
            QTest::ignoreMessage(QtInfoMsg, QRegularExpression(
                QStringLiteral("^REMOTE_ROWS pan=\\d+ STARVE now=101\\.0 lastSlot=100\\.0"
                               " nextSlot=166\\.7 queue=0 frontDueMinusNow=none frontAge=none"
                               " rate=1\\.00000 smoothedErr=0\\.0 sinceLastPut=67\\.7$")));
            run(widget);
        }

        qunsetenv("NEREUS_TRACE_REMOTE_ROWS");
        static QStringList traced;
        traced.clear();
        static QtMessageHandler previous = nullptr;
        previous = qInstallMessageHandler([](QtMsgType type, const QMessageLogContext& where,
                                             const QString& message) {
            if (message.contains(QLatin1String("REMOTE_ROWS"))) {
                traced.append(message);
            }
            if (previous) {
                previous(type, where, message);
            }
        });
        {
            SpectrumWidget widget;
            run(widget);
        }
        qInstallMessageHandler(previous);
        restore();
        QVERIFY2(traced.isEmpty(), qPrintable(traced.join(QLatin1Char('\n'))));
    }
};

QTEST_MAIN(TstRemoteWaterfallCadence)
#include "tst_remote_waterfall_cadence.moc"
