// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - rotor routes, end stops and the overlap (rotor control plan,
// Task 3a).
//
// RotorRoute is ported from Longpath src/core/BeamHeading.{h,cpp}
// [@551576e] and extended to rotors with overlap. The expected values come
// from JJ's (KG4VCF) Easy Rotor Control on a Yaesu 450-degree rotor, not
// from this implementation: tests/data/rotor/erc-gs232b-range-2026-10-07.log
// records a counter-clockwise stop at south (reads 183), a clockwise stop
// 446 degrees later (reads 269), modulo-360 replies (north reads 360), and
// where each set command went:
//   W000 from 292 went clockwise to 360          (+68)
//   W010 from 183 went clockwise through north   (+187)
//   W180 from 301 went to 180, not 540           (-121)
// The north stop case (350 to 010 travels -340) is Longpath's own example
// in BeamHeading.h. Model numbers are Hamlib's: ROT_MAKE_MODEL(family, n)
// = 100 * family + n, include/hamlib/rotlist.h [@50fc454].
//
// Modification history (NereusSDR):
//   2026-10-08: Initial version. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.

#include <QtTest>

#include "core/RotorModels.h"
#include "core/RotorRoute.h"
#include "OperatorWording.h"

#include <QFile>
#include <QRegularExpression>
#include <QSet>

#include <limits>

using namespace NereusSDR;
using namespace NereusSDR::RotorRoute;

namespace {

constexpr double kOverlapRange = 450.0;

// One position reply in the capture: the run and step it belongs to.
struct Reply {
    QString run;
    int     step{0};
    double  azDeg{0.0};
};

QVector<Reply> rangeCapture()
{
    QVector<Reply> replies;
    QFile f(QStringLiteral(NEREUS_TEST_DATA_DIR "/rotor/erc-gs232b-range-2026-10-07.log"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return replies;
    }
    static const QRegularExpression runRe(QStringLiteral("^# ---- run: (\\S+)"));
    static const QRegularExpression stepRe(QStringLiteral("^# step (\\d+):"));
    static const QRegularExpression azRe(QStringLiteral("reply=b'AZ=(\\d+)"));
    QString run;
    int step = 0;
    while (!f.atEnd()) {
        const QString line = QString::fromLatin1(f.readLine());
        if (const auto m = runRe.match(line); m.hasMatch()) {
            run = m.captured(1);
            step = 0;
            continue;
        }
        if (const auto m = stepRe.match(line); m.hasMatch()) {
            step = m.captured(1).toInt();
            continue;
        }
        if (const auto m = azRe.match(line); m.hasMatch()) {
            replies.append(Reply{run, step, m.captured(1).toDouble()});
        }
    }
    return replies;
}

QVector<double> replies(const QVector<Reply>& all, const QString& run, int step)
{
    QVector<double> out;
    for (const Reply& r : all) {
        if (r.run == run && r.step == step) {
            out.append(r.azDeg);
        }
    }
    return out;
}

// Feed replies in order; returns the span position after the last.
double feed(SpanTracker& t, const QVector<double>& az)
{
    for (double a : az) {
        t.update(a);
    }
    return t.spanPositionDeg();
}

} // namespace

class TestRotorRoute : public QObject {
    Q_OBJECT

private slots:
    void wrapsAngles();
    void spanPositionsOnTheOverlapRotor();
    void captureMovesFromTheirSpanPositions();
    void captureMovesFollowedThroughTheReplies();
    void clockwiseRunToTheStopReachesSpan449();
    void firstReplyInTheOverlapBandIsUnknown();
    void staleSpellPlacesTheRotorAgain();
    void noEndStopTakesTheShorterWay();
    void northStopRange360TakesTheLongWay();
    void unknownSpanHasNoRoute();
    void offsetAppliesToReplyAndIsRemovedFromTarget();
    void longWayNoteIsPlain();
    void modelListMatchesHamlib();
};

void TestRotorRoute::wrapsAngles()
{
    QCOMPARE(wrap360(0.0), 0.0);
    QCOMPARE(wrap360(360.0), 0.0);
    QCOMPARE(wrap360(629.0), 269.0);
    QCOMPARE(wrap360(-10.0), 350.0);
    const double tiny = wrap360(-1e-17);
    QVERIFY2(tiny >= 0.0 && tiny < 360.0, qPrintable(QString::number(tiny)));
    QCOMPARE(stopCompassDeg(EndStop::North), 0.0);
    QCOMPARE(stopCompassDeg(EndStop::South), 180.0);
    QCOMPARE(compassForSpan(EndStop::South, 449.0), 269.0);
    QCOMPARE(compassForSpan(EndStop::South, 3.0), 183.0);
    QCOMPARE(clampRangeDeg(360.0), 360.0);
    QCOMPARE(clampRangeDeg(450.0), 450.0);
    QCOMPARE(clampRangeDeg(1000.0), 450.0);
    QCOMPARE(clampRangeDeg(90.0), 360.0);
    QCOMPARE(clampRangeDeg(std::numeric_limits<double>::quiet_NaN()), 360.0);
}

void TestRotorRoute::spanPositionsOnTheOverlapRotor()
{
    // South stop, 450 degrees: 180 to 270 exist twice, 270 through north
    // to 180 once.
    const auto at183 = spanPositions(EndStop::South, kOverlapRange, 183.0);
    QCOMPARE(at183.count, 2);
    QCOMPARE(at183.first, 3.0);
    QCOMPARE(at183.second, 363.0);

    const auto at269 = spanPositions(EndStop::South, kOverlapRange, 269.0);
    QCOMPARE(at269.count, 2);
    QCOMPARE(at269.second, 449.0);

    const auto at270 = spanPositions(EndStop::South, kOverlapRange, 270.0);
    QCOMPARE(at270.count, 2);
    QCOMPARE(at270.second, 450.0);

    const auto at275 = spanPositions(EndStop::South, kOverlapRange, 275.0);
    QCOMPARE(at275.count, 1);
    QCOMPARE(at275.first, 95.0);

    const auto at0 = spanPositions(EndStop::South, kOverlapRange, 0.0);
    QCOMPARE(at0.count, 1);
    QCOMPARE(at0.first, 180.0);

    const auto at179 = spanPositions(EndStop::South, kOverlapRange, 179.0);
    QCOMPARE(at179.count, 1);
    QCOMPARE(at179.first, 359.0);

    QCOMPARE(spanPositions(EndStop::None, kOverlapRange, 90.0).count, 0);
    QCOMPARE(spanPositions(EndStop::South, kOverlapRange,
                           std::numeric_limits<double>::quiet_NaN()).count, 0);
}

void TestRotorRoute::captureMovesFromTheirSpanPositions()
{
    // 183 to 010: from the counter-clockwise end (span 3), clockwise
    // through north; counter-clockwise would cross the stop.
    const Move m1 = planOnSpan(3.0, 10.0, EndStop::South, kOverlapRange);
    QVERIFY(m1.routeKnown);
    QCOMPARE(m1.travelDeg, 187.0);
    QCOMPARE(m1.targetSpanDeg, 190.0);
    QCOMPARE(m1.targetDeg, 10.0);

    // 292 to 000: +68.
    const Move m2 = planOnSpan(112.0, 0.0, EndStop::South, kOverlapRange);
    QVERIFY(m2.routeKnown);
    QCOMPARE(m2.travelDeg, 68.0);

    // 301 to 180: 180 has span 0 and 360; 0 is nearer, -121, never the
    // short way across the stop.
    const Move m3 = planOnSpan(121.0, 180.0, EndStop::South, kOverlapRange);
    QVERIFY(m3.routeKnown);
    QCOMPARE(m3.travelDeg, -121.0);
    QCOMPARE(m3.targetSpanDeg, 0.0);

    // 360 is north: same route as 0.
    QCOMPARE(planOnSpan(112.0, 360.0, EndStop::South, kOverlapRange).travelDeg, 68.0);
}

void TestRotorRoute::captureMovesFollowedThroughTheReplies()
{
    const QVector<Reply> all = rangeCapture();
    QVERIFY2(!all.isEmpty(), "range capture not found");

    // Run erc-range starts at 292, outside the overlap band: placed at once.
    SpanTracker t(EndStop::South, kOverlapRange);
    QCOMPARE(feed(t, replies(all, QStringLiteral("erc-range"), 0)), 112.0);
    QCOMPARE(t.planTo(0.0).travelDeg, 68.0);
    // Step 1, W000: the replies end at 360 (north), span 180 = 112 + 68.
    QCOMPARE(feed(t, replies(all, QStringLiteral("erc-range"), 1)), 180.0);
    QCOMPARE(t.headingDeg(), 0.0);

    // Run erc-ccw starts at 301; W180 goes to the counter-clockwise end.
    SpanTracker c(EndStop::South, kOverlapRange);
    QCOMPARE(feed(c, replies(all, QStringLiteral("erc-ccw"), 0)), 121.0);
    QCOMPARE(c.planTo(180.0).travelDeg, -121.0);
    // The stop reads 183 on JJ's rotor: span 3.
    QCOMPARE(feed(c, replies(all, QStringLiteral("erc-ccw"), 5)), 3.0);
    // Step 6, W010 from 183: +187 clockwise through north, ending at
    // span 190, which is where the replies put it.
    QCOMPARE(c.planTo(10.0).travelDeg, 187.0);
    QCOMPARE(feed(c, replies(all, QStringLiteral("erc-ccw"), 6)), 190.0);
}

void TestRotorRoute::clockwiseRunToTheStopReachesSpan449()
{
    const QVector<Reply> all = rangeCapture();
    QVERIFY2(!all.isEmpty(), "range capture not found");

    // Run erc-ends: from 301 (span 121), W180 and L held to the
    // counter-clockwise end, then R held to the clockwise end.
    SpanTracker t(EndStop::South, kOverlapRange);
    QCOMPARE(feed(t, replies(all, QStringLiteral("erc-ends"), 0)), 121.0);
    feed(t, replies(all, QStringLiteral("erc-ends"), 9));
    QCOMPARE(feed(t, replies(all, QStringLiteral("erc-ends"), 10)), 3.0);
    QCOMPARE(t.headingDeg(), 183.0);

    const QVector<double> cw = replies(all, QStringLiteral("erc-ends"), 11);
    QVERIFY(!cw.isEmpty());
    QCOMPARE(cw.last(), 269.0);
    const double start = t.spanPositionDeg();
    double highest = start;
    for (double a : cw) {
        t.update(a);
        QVERIFY2(t.spanPositionDeg() >= 0.0 && t.spanPositionDeg() <= kOverlapRange,
                 qPrintable(QString::number(t.spanPositionDeg())));
        highest = std::max(highest, t.spanPositionDeg());
    }
    QCOMPARE(t.spanPositionDeg(), 449.0);
    QCOMPARE(highest, 449.0);
    QCOMPARE(t.spanPositionDeg() - start, 446.0);

    // Step 12, W302 from the clockwise end: 302 has one span position
    // (122), so the way back is -327; the replies end at 301 (span 121),
    // a degree short.
    QCOMPARE(t.planTo(302.0).travelDeg, -327.0);
    QCOMPARE(feed(t, replies(all, QStringLiteral("erc-ends"), 12)), 121.0);
}

void TestRotorRoute::firstReplyInTheOverlapBandIsUnknown()
{
    const QVector<Reply> all = rangeCapture();
    QVERIFY2(!all.isEmpty(), "range capture not found");

    // Connecting while the rotor sits at 183 (step 6 of erc-ccw starts
    // there): 183 is span 3 or 363, so neither the span nor a route is
    // known until a reply outside 180..270.
    const QVector<double> run = replies(all, QStringLiteral("erc-ccw"), 6);
    QCOMPARE(run.first(), 183.0);

    SpanTracker t(EndStop::South, kOverlapRange);
    bool placed = false;
    for (double a : run) {
        t.update(a);
        const bool inBand = spanPositions(EndStop::South, kOverlapRange, a).count == 2;
        if (!placed && inBand) {
            QCOMPARE(t.spanPositionDeg(), kUnknownSpanDeg);
            QVERIFY(!t.spanKnown());
            const Move m = t.planTo(10.0);
            QVERIFY(!m.routeKnown);
            QCOMPARE(m.travelDeg, 0.0);
            QCOMPARE(m.targetSpanDeg, kUnknownSpanDeg);
            continue;
        }
        if (!placed) {
            // The first reply outside the band: 275, span 95.
            QCOMPARE(a, 275.0);
            QCOMPARE(t.spanPositionDeg(), 95.0);
            placed = true;
        }
        QVERIFY(t.spanKnown());
        QVERIFY(t.planTo(10.0).routeKnown);
    }
    QVERIFY(placed);
    QCOMPARE(t.spanPositionDeg(), 190.0);
}

void TestRotorRoute::staleSpellPlacesTheRotorAgain()
{
    SpanTracker t(EndStop::South, kOverlapRange);
    t.update(300.0);
    QCOMPARE(t.spanPositionDeg(), 120.0);
    t.markStale();
    QCOMPARE(t.spanPositionDeg(), kUnknownSpanDeg);
    QCOMPARE(t.headingDeg(), -1.0);
    // After a stale spell a heading in the band is not placed.
    t.update(200.0);
    QCOMPARE(t.spanPositionDeg(), kUnknownSpanDeg);
    t.update(250.0);
    QCOMPARE(t.spanPositionDeg(), kUnknownSpanDeg);
    t.update(272.0);
    QCOMPARE(t.spanPositionDeg(), 92.0);

    // Not a number changes nothing.
    QVERIFY(!t.update(std::numeric_limits<double>::quiet_NaN()));
    QCOMPARE(t.spanPositionDeg(), 92.0);

    // Configure forgets the position.
    t.configure(EndStop::North, 360.0);
    QCOMPARE(t.spanPositionDeg(), kUnknownSpanDeg);
    QCOMPARE(t.endStop(), EndStop::North);
    QCOMPARE(t.rangeDeg(), 360.0);
}

void TestRotorRoute::noEndStopTakesTheShorterWay()
{
    QCOMPARE(planFree(350.0, 10.0).travelDeg, 20.0);
    QCOMPARE(planFree(10.0, 350.0).travelDeg, -20.0);
    QCOMPARE(planFree(90.0, 260.0).travelDeg, 170.0);
    QCOMPARE(planFree(90.0, 280.0).travelDeg, -170.0);
    QVERIFY(planFree(90.0, 280.0).routeKnown);
    QVERIFY(planFree(90.0, 280.0).note.isEmpty());

    SpanTracker t(EndStop::None, kFullTurnDeg);
    QVERIFY(!t.planTo(10.0).routeKnown);
    t.update(350.0);
    QCOMPARE(t.spanPositionDeg(), kUnknownSpanDeg);
    const Move m = t.planTo(10.0);
    QVERIFY(m.routeKnown);
    QCOMPARE(m.travelDeg, 20.0);
    QCOMPARE(m.targetSpanDeg, kUnknownSpanDeg);
    QCOMPARE(t.headingDeg(), 350.0);
    // Past north and on: no span, no stop to keep track of.
    t.update(5.0);
    QCOMPARE(t.planTo(350.0).travelDeg, -15.0);
}

void TestRotorRoute::northStopRange360TakesTheLongWay()
{
    // Longpath's example: a north-stop rotor from 350 to 10 is a
    // three-hundred-and-forty-degree move.
    const Move m = planOnSpan(350.0, 10.0, EndStop::North, kFullTurnDeg);
    QVERIFY(m.routeKnown);
    QCOMPARE(m.travelDeg, -340.0);
    QVERIFY(!m.note.isEmpty());

    SpanTracker t(EndStop::North, kFullTurnDeg);
    t.update(350.0);
    QCOMPARE(t.spanPositionDeg(), 350.0);
    QCOMPARE(t.planTo(10.0).travelDeg, -340.0);
    QCOMPARE(t.planTo(340.0).travelDeg, -10.0);
}

void TestRotorRoute::unknownSpanHasNoRoute()
{
    const Move m = planOnSpan(kUnknownSpanDeg, 90.0, EndStop::South, kOverlapRange);
    QVERIFY(!m.routeKnown);
    QCOMPARE(m.travelDeg, 0.0);
    QCOMPARE(m.targetDeg, 90.0);
    QVERIFY(m.note.isEmpty());

    SpanTracker t(EndStop::South, kOverlapRange);
    QVERIFY(!t.planTo(90.0).routeKnown);
    QCOMPARE(t.headingDeg(), -1.0);
}

void TestRotorRoute::offsetAppliesToReplyAndIsRemovedFromTarget()
{
    // Added to the reported heading, wrapping past north.
    QCOMPARE(applyOffset(100.0, 5.0), 105.0);
    QCOMPARE(applyOffset(358.0, 5.0), 3.0);
    QCOMPARE(applyOffset(360.0, 0.0), 0.0);
    QCOMPARE(applyOffset(2.0, -5.0), 357.0);
    // Removed from a target before it is sent.
    QCOMPARE(removeOffset(105.0, 5.0), 100.0);
    QCOMPARE(removeOffset(3.0, 5.0), 358.0);
    QCOMPARE(removeOffset(357.0, -5.0), 2.0);
    // A round trip gives the reply back.
    for (double reported : {0.0, 10.0, 183.0, 269.0, 359.0}) {
        QCOMPARE(removeOffset(applyOffset(reported, 7.5), 7.5), reported);
    }

    // The tracker sees the heading after the offset: with 5 added, the
    // controller's 358 is 003 to the north-stop rotor.
    SpanTracker t(EndStop::North, kFullTurnDeg);
    t.update(applyOffset(358.0, 5.0));
    QCOMPARE(t.headingDeg(), 3.0);
    QCOMPARE(t.spanPositionDeg(), 3.0);
}

void TestRotorRoute::longWayNoteIsPlain()
{
    const Move m = planOnSpan(350.0, 10.0, EndStop::North, kFullTurnDeg);
    QVERIFY2(OperatorWording::isPlain(m.note), qPrintable(m.note));
    QVERIFY(!m.note.contains(QChar(0x2014)));
    // A short move says nothing.
    QVERIFY(planOnSpan(10.0, 40.0, EndStop::North, kFullTurnDeg).note.isEmpty());
}

void TestRotorRoute::modelListMatchesHamlib()
{
    // Hamlib include/hamlib/rotlist.h [@50fc454]: ROT_MAKE_MODEL(a, b) is
    // 100 * a + b. Families: DUMMY 0, ROTOREZ 4, GS232A 6, SPID 9, M2 10,
    // ARS 11, ETHER6 15, PROSISTEL 17.
    const auto make = [](int family, int n) { return 100 * family + n; };
    const QSet<int> hamlib{
        make(6, 1),   // ROT_MODEL_GS232A
        make(6, 3),   // ROT_MODEL_GS232B
        make(4, 4),   // ROT_MODEL_ERC
        make(4, 3),   // ROT_MODEL_DCU
        make(4, 1),   // ROT_MODEL_ROTOREZ
        make(4, 5),   // ROT_MODEL_RT21
        make(9, 1),   // ROT_MODEL_SPID_ROT2PROG
        make(9, 2),   // ROT_MODEL_SPID_ROT1PROG
        make(9, 3),   // ROT_MODEL_SPID_MD01_ROT2PROG
        make(10, 1),  // ROT_MODEL_RC2800
        make(17, 1),  // ROT_MODEL_PROSISTEL_D_AZ
        make(11, 1),  // ROT_MODEL_RCI_AZEL
        make(6, 6),   // ROT_MODEL_GS232
        make(6, 5),   // ROT_MODEL_GS23
        make(6, 2),   // ROT_MODEL_GS232_GENERIC
        make(15, 1),  // ROT_MODEL_ETHER6
        make(0, 1),   // ROT_MODEL_DUMMY
    };

    const QVector<RotorModel> models = commonRotorModels();
    QCOMPARE(models.size(), hamlib.size());
    QSet<int> seen;
    for (const RotorModel& m : models) {
        QVERIFY2(hamlib.contains(m.hamlibId), qPrintable(QString::number(m.hamlibId)));
        QVERIFY2(!seen.contains(m.hamlibId), qPrintable(QString::number(m.hamlibId)));
        seen.insert(m.hamlibId);
        for (const QString& text : {m.name, m.note}) {
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
            QVERIFY2(!text.contains(QChar(0x2014)), qPrintable(text));
        }
    }

    // Hamlib's own ERC driver, by DF9GR.
    bool erc = false;
    for (const RotorModel& m : models) {
        if (m.hamlibId == 404) {
            erc = m.name.contains(QStringLiteral("ERC"));
        }
    }
    QVERIFY(erc);

    // The other model numbers the notes name.
    QCOMPARE(make(17, 3), 1703);  // ROT_MODEL_PROSISTEL_COMBI_TRACK_AZEL
    QCOMPARE(make(11, 2), 1102);  // ROT_MODEL_RCI_AZ

    const QVector<int> bauds = commonRotorBauds();
    QVERIFY(bauds.contains(9600));
    QCOMPARE(bauds.first(), 1200);
    QCOMPARE(bauds.last(), 115200);
}

QTEST_APPLESS_MAIN(TestRotorRoute)
#include "tst_rotor_route.moc"
