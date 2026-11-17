// =================================================================
// tests/tst_dsr_desktop_rewind.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. Rewind on the desktop waterfall
// (spec 3 "Rewind sound", "Rewind picture", "PTT while rewound", "Moving
// through hours", 6.7, A11, A12; recording plan Task 24), against a fake
// recordings access: a strip drag past the window's own history asks the
// Core for rows and dims with "Drawing" until they come; a click on a past
// signal rewinds with the clicked moment and frequency and the picture then
// moves with the sound; the past spectrum replaces the trace; LIVE asks for
// live sound, and after PTT took the slice live it still does; your
// transmit rows are marked; the hours map and a typed time jump; an older
// Core or the recorder off keeps the strip as it was, with the hours map
// and the past click disabled with the reason.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code. Recording plan Task 24.
//   2026-11-16 - Rewound, the past spectrum is asked for once a display
//                 frame for the moment playing, one at a time, and a late
//                 answer asks again at once. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalSpy>
#include <QTimeZone>

#include "core/ConnectionState.h"
#include "core/recordings/dsr/DsrReplay.h"
#include "fakes/FakeRecordingsAccess.h"
#include "gui/SpectrumWidget.h"
#include "gui/recordings/DsrHoursMap.h"
#include "gui/recordings/DsrRewindLink.h"

using namespace NereusSDR;

namespace {

// 14:32:10 UTC on the mockup's day.
const qint64 kNowMs = QDateTime(QDate(2026, 11, 14), QTime(14, 32, 10), QTimeZone::utc()).toMSecsSinceEpoch();
constexpr int kRowMs = 50;               // the waterfall's row period
constexpr qint64 kOwnHistoryMs = 20000;  // this window's own history: 400 rows
constexpr int kOwnRows = int(kOwnHistoryMs / kRowMs);
constexpr qint64 kKeptMs = 3LL * 3600LL * 1000LL;   // the Core keeps three hours
constexpr int kBins = 600;

QVector<float> flatRow(float dbm)
{
    return QVector<float>(kBins, dbm);
}

QJsonObject dsrDocument(bool enabled, const QJsonArray& rewound = {}, const QString& reason = {})
{
    QJsonObject doc = FakeRecordingsAccess::document({}, 1);
    doc.insert(QStringLiteral("dsr"), QJsonObject{{QStringLiteral("enabled"), enabled},
                                                  {QStringLiteral("recording"), enabled},
                                                  {QStringLiteral("reason"), reason},
                                                  {QStringLiteral("oldestUtcMs"), kNowMs - kKeptMs},
                                                  {QStringLiteral("rewound"), rewound}});
    return doc;
}

QJsonArray rewoundSlice(int slice, qint64 utcMs, qint64 hz, qint64 liveHz)
{
    return QJsonArray{QJsonObject{{QStringLiteral("slice"), slice},
                                  {QStringLiteral("utcNs"), utcMs * 1000000LL},
                                  {QStringLiteral("frequencyHz"), hz},
                                  {QStringLiteral("liveFrequencyHz"), liveHz}}};
}

void mouse(QWidget* w, QEvent::Type type, const QPoint& pos, Qt::MouseButtons buttons)
{
    QMouseEvent ev(type, QPointF(pos), w->mapToGlobal(QPointF(pos)), Qt::LeftButton, buttons, Qt::NoModifier);
    QApplication::sendEvent(w, &ev);
}

void click(QWidget* w, const QPoint& pos)
{
    mouse(w, QEvent::MouseButtonPress, pos, Qt::LeftButton);
    mouse(w, QEvent::MouseButtonRelease, pos, Qt::NoButton);
}

void drag(QWidget* w, const QPoint& from, const QPoint& to)
{
    mouse(w, QEvent::MouseButtonPress, from, Qt::LeftButton);
    mouse(w, QEvent::MouseMove, to, Qt::LeftButton);
    mouse(w, QEvent::MouseButtonRelease, to, Qt::NoButton);
}

QList<FakeRecordingsAccess::Call> callsOf(const FakeRecordingsAccess& fake, const QString& verb)
{
    QList<FakeRecordingsAccess::Call> out;
    for (const auto& c : fake.calls) {
        if (c.verb == verb) {
            out.append(c);
        }
    }
    return out;
}

// What the Core answers for a request: every asked row at -40 dBm (a hot
// row on these levels) over the pan, `transmit` on the newest.
DsrPastFrames answerFor(const FakeRecordingsAccess::Call& call, double firstHz, double hzPerPoint, int points,
                        bool newestTransmit = false)
{
    DsrPastFrames f;
    f.slice = call.sliceId;
    f.firstBinHz = firstHz;
    f.hzPerBin = hzPerPoint;
    f.binCount = points;
    const int n = call.count;
    const qint64 step = n > 1 ? (call.utcNs - call.fromUtcNs) / (n - 1) : 0;
    for (int i = 0; i < n; ++i) {
        f.utcNs.append(call.utcNs - qint64(i) * step);
        f.rows.append(QVector<float>(points, -40.0f));
        f.transmit.append(newestTransmit && i == 0);
    }
    return f;
}

} // namespace

class TestDsrDesktopRewind : public QObject {
    Q_OBJECT

private:
    qint64 m_clockMs = kNowMs;

    // A shown pan with fixed levels, a held ticker, a 20 s history of rows
    // 50 ms apart ending now, slice A (0) on a fake access with the
    // recorder in use.
    void prepare(SpectrumWidget& w, DsrRewindLink& link, FakeRecordingsAccess& fake)
    {
        m_clockMs = kNowMs - kOwnHistoryMs;
        w.setWaterfallClockForTest([this] { return m_clockMs; });
        w.setConnectionState(ConnectionState::Connected);
        w.setWaterfallTickerPausedForTest(true);
        w.setWfAgcEnabled(false);
        w.setWfLowThresholdForTest(-130.0f);
        w.setWfHighThresholdForTest(-40.0f);
        w.setWfUpdatePeriodMs(kRowMs);
        w.setWaterfallHistoryMs(kOwnHistoryMs);
        w.setFrequencyRange(7185000.0, 192000.0);
        w.setStepSize(100);
        w.resize(1000, 600);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.pushWaterfallRowForTest(flatRow(-130.0f));
        QTRY_VERIFY(!w.waterfallHistoryResizePendingForTest());
        for (int i = 1; i < kOwnRows; ++i) {
            m_clockMs += kRowMs;
            w.pushWaterfallRowForTest(flatRow(-130.0f));
        }
        QCOMPARE(w.waterfallHistoryRowsForTest(), kOwnRows);
        QCOMPARE(w.waterfallUtcMsForAge(0), kNowMs - kRowMs);
        fake.version = 4;
        fake.setDocument(dsrDocument(true));
        link.setAccess(&fake);
        QVERIFY(w.dsrRewindAvailable());
    }

    QPoint stripPoint(const SpectrumWidget& w, double fromTop) const
    {
        const QRect strip = w.waterfallTimeScaleRectForTest();
        return QPoint(strip.center().x(), strip.top() + int(fromTop * (strip.height() - 1)));
    }

    QPoint waterfallPoint(const SpectrumWidget& w, double fromLeft, int row) const
    {
        const QRect wf = w.waterfallRectForTest();
        return QPoint(wf.left() + int(fromLeft * wf.width() * 0.8), wf.top() + row);
    }

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.info=false\nnereus.*.debug=false"));
    }

    // Past the window's own minutes the rows come from the Core: asked once,
    // with the pan's own view, the picture dimmed with "Drawing" until they
    // come; the strip moves 20 minutes per height.
    void aDragPastTheWindowsMinutesAsksTheCore()
    {
        SpectrumWidget w;
        FakeRecordingsAccess fake;
        DsrRewindLink link(&w, [] { return 0; });
        prepare(w, link, fake);
        if (QTest::currentTestFailed()) {
            return;
        }
        const int visible = w.waterfallRectForTest().height();
        const int ownMax = kOwnRows - w.liveWaterfallForTest().height();
        QVERIFY(ownMax > 0);
        QCOMPARE(w.maxWaterfallHistoryOffsetRows(), w.waterfallAgeForUtcMs(kNowMs - kKeptMs));

        // A quarter of the strip's height: five minutes back.
        drag(&w, stripPoint(w, 0.9), stripPoint(w, 0.65));
        const QRect strip = w.waterfallTimeScaleRectForTest();
        Q_UNUSED(strip);
        const int offset = w.waterfallHistoryOffsetRowsForTest();
        const int perHeight = int(20LL * 60LL * 1000LL / kRowMs);
        const int dragged = int(std::lround(double(stripPoint(w, 0.9).y() - stripPoint(w, 0.65).y())
                                            / w.waterfallRectForTest().height() * perHeight));
        QVERIFY2(std::abs(offset - dragged) <= 1, qPrintable(QStringLiteral("%1 vs %2").arg(offset).arg(dragged)));
        QVERIFY(!w.wfLive());
        QVERIFY(offset > kOwnRows);

        const auto asked = callsOf(fake, QStringLiteral("requestPastRows"));
        QCOMPARE(asked.size(), 1);
        const auto& call = asked.first();
        QCOMPARE(call.sliceId, 0);
        QCOMPARE(call.utcNs, w.waterfallUtcMsForAge(offset) * 1000000LL);
        QVERIFY(call.fromUtcNs < call.utcNs);
        QVERIFY(call.count >= w.liveWaterfallForTest().height() - 2);
        QCOMPARE(call.view.pixels, w.width() - w.reservedRightEdgeWidth());
        QCOMPARE(call.view.fftSize, 4096);
        QCOMPARE(call.view.centreHz, 7185000);
        QCOMPARE(call.view.spanHz, 192000);
        QVERIFY(w.dsrDrawingForTest());
        Q_UNUSED(visible);

        // Not asked again while it is drawn.
        drag(&w, stripPoint(w, 0.65), stripPoint(w, 0.64));
        QCOMPARE(callsOf(fake, QStringLiteral("requestPastRows")).size(), 1);

        // The answer fills the picture and the dim goes.
        const auto ask = callsOf(fake, QStringLiteral("requestPastRows")).first();
        w.setWaterfallHistoryOffsetForTest(offset);
        const double firstHz = 7185000.0 - 96000.0;
        const int points = call.view.pixels;
        emit fake.pastRowsReady(answerFor(ask, firstHz, 192000.0 / points, points));
        QVERIFY(!w.dsrDrawingForTest());
        QVERIFY(w.dsrCachedRowsForTest() > 0);
        const QImage& picture = w.liveWaterfallForTest();
        const QRgb hot = w.dbmToRgbForTest(-40.0f);
        QCOMPARE(reinterpret_cast<const QRgb*>(picture.constScanLine(0))[picture.width() / 2], hot);
        QCOMPARE(reinterpret_cast<const QRgb*>(picture.constScanLine(picture.height() / 2))[picture.width() / 3],
                 hot);
        // Another pan's rows (another slice) are not this one's.
        auto other = answerFor(ask, firstHz, 192000.0 / points, points);
        other.slice = 1;
        const int cached = w.dsrCachedRowsForTest();
        emit fake.pastRowsReady(other);
        QCOMPARE(w.dsrCachedRowsForTest(), cached);
    }

    // A click on a past signal rewinds slice A there: the clicked moment and
    // frequency, the clicked row to the top, and from then the picture
    // moves with the sound; the past spectrum replaces the trace.
    void aClickOnAPastSignalRewinds()
    {
        SpectrumWidget w;
        FakeRecordingsAccess fake;
        DsrRewindLink link(&w, [] { return 0; });
        prepare(w, link, fake);
        if (QTest::currentTestFailed()) {
            return;
        }
        QSignalSpy tuned(&w, &SpectrumWidget::frequencyClicked);
        w.setWaterfallHistoryOffsetForTest(20);
        const int row = 30;
        const QPoint at = waterfallPoint(w, 0.25, row);
        const qint64 expectedMs = w.waterfallUtcMsForAge(20 + row);
        const QRect spec = w.spectrumRectForTest();
        const double expectedHz = 7185000.0 - 96000.0 + double(at.x() - spec.left()) / spec.width() * 192000.0;
        click(&w, at);

        QCOMPARE(tuned.count(), 0);
        const auto rewinds = callsOf(fake, QStringLiteral("rewind"));
        QCOMPARE(rewinds.size(), 1);
        QCOMPARE(rewinds.first().sliceId, 0);
        QCOMPARE(rewinds.first().utcNs, expectedMs * 1000000LL);
        QVERIFY2(std::abs(double(rewinds.first().hz) - expectedHz) <= 100.0,
                 qPrintable(QStringLiteral("%1 vs %2").arg(rewinds.first().hz).arg(expectedHz)));
        QCOMPARE(rewinds.first().hz % 100, 0);
        QVERIFY(w.dsrRewound());
        QCOMPARE(w.waterfallTopUtcMs(), expectedMs);
        const auto spectra = callsOf(fake, QStringLiteral("requestPastSpectrum"));
        QCOMPARE(spectra.size(), 1);
        QCOMPARE(spectra.first().utcNs, expectedMs * 1000000LL);

        // The Core plays it: the document lists slice A.
        fake.setDocument(dsrDocument(true, rewoundSlice(0, expectedMs, rewinds.first().hz, 7185000)));
        QVERIFY(w.dsrRewound());
        const int offset = w.waterfallHistoryOffsetRowsForTest();
        QCOMPARE(offset, 20 + row);

        // The past spectrum: the trace is the moment played, at -50 dBm.
        DsrPastFrames frame;
        frame.slice = 0;
        frame.firstBinHz = 7185000.0 - 96000.0;
        frame.hzPerBin = 192000.0 / 800.0;
        frame.binCount = 800;
        frame.utcNs = {expectedMs * 1000000LL};
        frame.rows = {QVector<float>(800, -50.0f)};
        emit fake.pastSpectrumReady(frame);
        QVERIFY(!w.renderedPixelsForTest().isEmpty());
        for (const float v : w.renderedPixelsForTest()) {
            QCOMPARE(v, -50.0f);
        }

        // A new row: the picture moves with the sound, the offset holds and
        // the top row is still a moment the sound has reached.
        m_clockMs += kRowMs;
        w.pushWaterfallRowForTest(flatRow(-130.0f));
        QCOMPARE(w.waterfallHistoryOffsetRowsForTest(), offset);
        QCOMPARE(w.waterfallTopUtcMs(), expectedMs + kRowMs);
        // Paused but not rewound, the picture holds still instead (as before).
        w.setDsrRewound(false);
        m_clockMs += kRowMs;
        w.pushWaterfallRowForTest(flatRow(-130.0f));
        QCOMPARE(w.waterfallHistoryOffsetRowsForTest(), offset + 1);
    }

    // Rewound, the past spectrum is asked for on every display frame, not
    // once a waterfall row, for the moment playing (one frame later each
    // time, between rows too); never two at once; an answer that comes a
    // frame or more late asks again at once. The paint timer is stopped
    // and ticked by hand on the test's clock.
    void thePastSpectrumFollowsTheFrameRate()
    {
        SpectrumWidget w;
        FakeRecordingsAccess fake;
        DsrRewindLink link(&w, [] { return 0; });
        prepare(w, link, fake);
        if (QTest::currentTestFailed()) {
            return;
        }
        QTimer& paint = w.displayTimerForTest();
        paint.stop();
        w.setDisplayFps(50);
        const int periodMs = w.displayTimerIntervalForTest();
        QCOMPARE(periodMs, 20);
        const auto tick = [&paint] { QMetaObject::invokeMethod(&paint, "timeout", Qt::DirectConnection); };
        w.setWaterfallHistoryOffsetForTest(100);
        w.setDsrRewound(true, 0);
        const auto asks = [&fake] { return callsOf(fake, QStringLiteral("requestPastSpectrum")); };
        QCOMPARE(asks().size(), 1);
        int answered = 0;
        const auto answerNewest = [&] {
            const auto asked = asks();
            if (asked.size() <= answered) {
                return;
            }
            answered = int(asked.size());
            DsrPastFrames f;
            f.slice = 0;
            f.firstBinHz = 7185000.0 - 96000.0;
            f.hzPerBin = 192000.0 / 800.0;
            f.binCount = 800;
            f.utcNs = {asked.last().utcNs};
            f.rows = {QVector<float>(800, -50.0f)};
            emit fake.pastSpectrumReady(f);
        };
        answerNewest();

        // One second: a frame every 20 ms, a row every 50 ms, the Core
        // answering each ask at once.
        int frames = 0;
        int rows = 0;
        for (int t = 1; t <= 1000; ++t) {
            ++m_clockMs;
            if (t % periodMs == 0) {
                tick();
                ++frames;
                answerNewest();
            }
            if (t % kRowMs == 0) {
                w.pushWaterfallRowForTest(flatRow(-130.0f));
                ++rows;
                answerNewest();
            }
        }
        QCOMPARE(frames, 50);
        QCOMPARE(rows, 20);
        const auto asked = asks();
        QCOMPARE(asked.size(), 1 + frames);
        for (qsizetype i = 1; i < asked.size(); ++i) {
            QCOMPARE(asked.at(i).utcNs - asked.at(i - 1).utcNs, qint64(periodMs) * 1000000LL);
        }

        // Unanswered: no second ask, however many frames pass.
        tick();   // nothing new since the last frame's ask
        m_clockMs += periodMs;
        tick();
        const int waiting = int(asks().size());
        QCOMPARE(waiting, 1 + frames + 1);
        m_clockMs += periodMs;
        tick();
        m_clockMs += kRowMs - periodMs - periodMs / 2;   // a row between frames
        w.pushWaterfallRowForTest(flatRow(-130.0f));
        m_clockMs += periodMs / 2;
        tick();
        m_clockMs += periodMs;
        tick();
        QCOMPARE(int(asks().size()), waiting);
        // The late answer asks again at once, for the moment now.
        answerNewest();
        QCOMPARE(int(asks().size()), waiting + 1);
        QCOMPARE(asks().last().utcNs - asks().at(waiting - 1).utcNs, qint64(3 * periodMs) * 1000000LL);
    }

    // LIVE asks for live sound; after PTT took the slice live (the
    // document drops it unasked) the picture is live and LIVE stays red
    // until pressed, and pressing it still asks for live.
    void liveAndPushToTalk()
    {
        SpectrumWidget w;
        FakeRecordingsAccess fake;
        DsrRewindLink link(&w, [] { return 0; });
        prepare(w, link, fake);
        if (QTest::currentTestFailed()) {
            return;
        }
        const qint64 played = kNowMs - 5000;
        fake.setDocument(dsrDocument(true, rewoundSlice(0, played, 7190000, 7185000)));
        QVERIFY(w.dsrRewound());
        QVERIFY(!w.wfLive());
        QCOMPARE(w.waterfallTopUtcMs(), w.waterfallUtcMsForAge(w.waterfallAgeForUtcMs(played)));
        click(&w, w.waterfallLiveButtonRectForTest().center());
        QCOMPARE(callsOf(fake, QStringLiteral("live")).size(), 1);
        QCOMPARE(callsOf(fake, QStringLiteral("live")).first().sliceId, 0);
        QVERIFY(w.wfLive());
        fake.setDocument(dsrDocument(true));
        QVERIFY(!w.dsrLiveReturnPending());

        // Rewound again, then keyed: the Core took it live on the played
        // frequency.
        fake.setDocument(dsrDocument(true, rewoundSlice(0, played, 7190000, 7185000)));
        QVERIFY(w.dsrRewound());
        fake.setDocument(dsrDocument(true));
        QVERIFY(!w.dsrRewound());
        QVERIFY(w.wfLive());
        QVERIFY(w.dsrLiveReturnPending());
        click(&w, w.waterfallLiveButtonRectForTest().center());
        QCOMPARE(callsOf(fake, QStringLiteral("live")).size(), 2);
        QVERIFY(!w.dsrLiveReturnPending());
        // LIVE on a live pan with nothing to return to asks for nothing.
        click(&w, w.waterfallLiveButtonRectForTest().center());
        QCOMPARE(callsOf(fake, QStringLiteral("live")).size(), 2);
    }

    // Rows of your transmit are marked as yours: this window's own and the
    // Core's.
    void yourTransmitRowsAreMarked()
    {
        SpectrumWidget w;
        FakeRecordingsAccess fake;
        DsrRewindLink link(&w, [] { return 0; });
        prepare(w, link, fake);
        if (QTest::currentTestFailed()) {
            return;
        }
        w.setWaterfallStopOnTx(false);
        w.setMoxOverlay(true);
        m_clockMs += kRowMs;
        w.pushWaterfallRowForTest(flatRow(-130.0f));
        w.setMoxOverlay(false);
        m_clockMs += kRowMs;
        w.pushWaterfallRowForTest(flatRow(-130.0f));
        QVERIFY(!w.historyRowTransmitForTest(0));
        QVERIFY(w.historyRowTransmitForTest(1));
        QVERIFY(!w.historyRowTransmitForTest(2));

        // The Core's rows past the window's own.
        w.setWaterfallHistoryOffsetForTest(kOwnRows + 100);
        const auto asked = callsOf(fake, QStringLiteral("requestPastRows"));
        QCOMPARE(asked.size(), 1);
        emit fake.pastRowsReady(answerFor(asked.first(), 7185000.0 - 96000.0, 240.0, 800, true));
        const int topAge = w.waterfallAgeForUtcMs(asked.first().utcNs / 1000000LL);
        QVERIFY(w.historyRowTransmitForTest(topAge));
        QVERIFY(!w.historyRowTransmitForTest(topAge + 2));
    }

    // The hours map beside the strip: shown once rewound, asks for the
    // hours, newest at the top; a click jumps there, and while rewound the
    // sound follows. A typed time jumps too.
    void theHoursMapAndATypedTimeJump()
    {
        SpectrumWidget w;
        FakeRecordingsAccess fake;
        DsrRewindLink link(&w, [] { return 0; });
        prepare(w, link, fake);
        if (QTest::currentTestFailed()) {
            return;
        }
        w.setWaterfallHistoryOffsetForTest(10);
        auto* map = qobject_cast<DsrHoursMap*>(w.dsrHoursMapForTest());
        QVERIFY(map != nullptr);
        QVERIFY(map->isVisible());
        QVERIFY(map->isEnabled());
        QCOMPARE(callsOf(fake, QStringLiteral("requestHours")).size(), 1);
        QList<DsrHour> hours;
        for (int h = 0; h < 4; ++h) {
            const qint64 start = ((kNowMs / 3600000LL) - h) * 3600000LL;
            hours.append(DsrHour{start * 1000000LL, 0.2 * h});
        }
        emit fake.hoursReady(hours);
        QCOMPARE(map->hourCount(), 4);
        QVERIFY(map->yForUtcMs(kNowMs - 60000) < map->yForUtcMs(kNowMs - 7200000));
        QVERIFY(map->x() + map->width() <= w.waterfallTimeScaleRectForTest().left());

        // Rewound, then a click on the map two hours back.
        fake.setDocument(dsrDocument(true, rewoundSlice(0, kNowMs - 3000, 7190000, 7185000)));
        const int y = map->yForUtcMs(kNowMs - 7200000);
        click(map, QPoint(map->width() / 2, y));
        const qint64 expected = map->utcMsAtY(y);
        QVERIFY(std::abs(w.waterfallTopUtcMs() - expected) <= kRowMs);
        const auto seeks = callsOf(fake, QStringLiteral("seekPast"));
        QCOMPARE(seeks.size(), 1);
        QCOMPARE(seeks.first().utcNs, w.waterfallTopUtcMs() * 1000000LL);
        QVERIFY(w.dsrDrawingForTest());

        // Typed time: the box opens with the clicked strip time, and Go jumps.
        w.openDsrGoto(w.waterfallTopUtcMs(), w.waterfallRectForTest().top() + 40);
        QWidget* box = w.dsrGotoBoxForTest();
        QVERIFY(box != nullptr && box->isVisible());
        auto* edit = box->findChild<QLineEdit*>(QStringLiteral("DsrGotoEdit"));
        auto* go = box->findChild<QPushButton*>(QStringLiteral("DsrGotoGo"));
        auto* note = box->findChild<QLabel*>(QStringLiteral("DsrGotoNote"));
        QVERIFY(edit != nullptr && go != nullptr && note != nullptr);
        QCOMPARE(edit->text(), QDateTime::fromMSecsSinceEpoch(w.waterfallTopUtcMs(), QTimeZone::utc())
                                   .toString(QStringLiteral("HH:mm")));
        edit->setText(QStringLiteral("nonsense"));
        go->click();
        QVERIFY(box->isVisible());
        QCOMPARE(note->text(), QStringLiteral("Type a time like 13:05"));
        edit->setText(QStringLiteral("10:00"));   // before the oldest kept (11:32:10)
        go->click();
        QVERIFY(box->isVisible());
        QCOMPARE(note->text(), QStringLiteral("The Core keeps back to 11:32 UTC"));
        edit->setText(QStringLiteral("13:05"));
        go->click();
        QVERIFY(!box->isVisible());
        const qint64 at1305 = QDateTime(QDate(2026, 11, 14), QTime(13, 5), QTimeZone::utc()).toMSecsSinceEpoch();
        QVERIFY(std::abs(w.waterfallTopUtcMs() - at1305) <= kRowMs);
        QCOMPARE(callsOf(fake, QStringLiteral("seekPast")).size(), 2);
    }

    void aTypedTimeReads()
    {
        const auto at = [](int h, int m, int s, int dayBack = 0) {
            return QDateTime(QDate(2026, 11, 14).addDays(-dayBack), QTime(h, m, s), QTimeZone::utc())
                .toMSecsSinceEpoch();
        };
        QCOMPARE(SpectrumWidget::parseTypedUtcTime(QStringLiteral("13:05"), kNowMs), std::optional<qint64>(at(13, 5, 0)));
        QCOMPARE(SpectrumWidget::parseTypedUtcTime(QStringLiteral(" 9:07:30 "), kNowMs),
                 std::optional<qint64>(at(9, 7, 30)));
        // Later than now today is yesterday's.
        QCOMPARE(SpectrumWidget::parseTypedUtcTime(QStringLiteral("15:00"), kNowMs),
                 std::optional<qint64>(at(15, 0, 0, 1)));
        QVERIFY(!SpectrumWidget::parseTypedUtcTime(QStringLiteral("25:00"), kNowMs));
        QVERIFY(!SpectrumWidget::parseTypedUtcTime(QStringLiteral("13:60"), kNowMs));
        QVERIFY(!SpectrumWidget::parseTypedUtcTime(QStringLiteral("1305"), kNowMs));
    }

    // An older Core, or the recorder off: the strip works as before within
    // the window's own minutes, nothing is asked of the Core, a click on
    // the waterfall tunes as before, and the hours map is there, disabled,
    // with the reason.
    void anOlderCoreOrTheRecorderOffKeepsTheStripAsItWas_data()
    {
        QTest::addColumn<bool>("olderCore");
        QTest::addColumn<QString>("reason");
        QTest::newRow("older Core") << true << RecordingsAccess::needsNewerCoreReason();
        QTest::newRow("recorder off") << false << DsrRewindLink::recorderOffReason();
    }

    void anOlderCoreOrTheRecorderOffKeepsTheStripAsItWas()
    {
        QFETCH(bool, olderCore);
        QFETCH(QString, reason);
        SpectrumWidget w;
        FakeRecordingsAccess fake;
        DsrRewindLink link(&w, [] { return 0; });
        prepare(w, link, fake);
        if (QTest::currentTestFailed()) {
            return;
        }
        if (olderCore) {
            fake.setState(RecordingsAccess::State::NeedsNewerCore);
        } else {
            fake.setDocument(dsrDocument(false));
        }
        QVERIFY(!w.dsrRewindAvailable());
        QCOMPARE(w.dsrUnavailableReason(), reason);
        const int ownMax = kOwnRows - w.liveWaterfallForTest().height();
        QCOMPARE(w.maxWaterfallHistoryOffsetRows(), ownMax);
        drag(&w, stripPoint(w, 1.0), stripPoint(w, 0.0) - QPoint(0, 200));
        QCOMPARE(w.waterfallHistoryOffsetRowsForTest(), ownMax);
        QCOMPARE(callsOf(fake, QStringLiteral("requestPastRows")).size(), 0);

        auto* map = qobject_cast<DsrHoursMap*>(w.dsrHoursMapForTest());
        QVERIFY(map != nullptr);
        QVERIFY(map->isVisible());
        QVERIFY(!map->isEnabled());
        QCOMPARE(map->toolTip(), reason);
        QCOMPARE(callsOf(fake, QStringLiteral("requestHours")).size(), 0);

        QSignalSpy tuned(&w, &SpectrumWidget::frequencyClicked);
        click(&w, waterfallPoint(w, 0.4, 20));
        QCOMPARE(callsOf(fake, QStringLiteral("rewind")).size(), 0);
        QCOMPARE(tuned.count(), 1);
    }
};

QTEST_MAIN(TestDsrDesktopRewind)
#include "tst_dsr_desktop_rewind.moc"
