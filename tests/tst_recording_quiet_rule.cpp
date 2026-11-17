// =================================================================
// tests/tst_recording_quiet_rule.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. The "stop by itself when it goes
// quiet" rule (spec V4, A3) on an injected clock.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/recordings/QuietRule.h"

using namespace NereusSDR;

class TestRecordingQuietRule : public QObject {
    Q_OBJECT

private slots:
    void squelchOnNothingHeardIsQuietAtTwoMinutes()
    {
        QuietRule rule;
        rule.setMinutes(2);
        rule.start(0);
        rule.onLevel(30000, /*squelchOn*/ true, /*aboveSquelch*/ false, -50.0, -120.0);
        QVERIFY(!rule.quiet(119999));
        QVERIFY(rule.quiet(120000));
    }

    void transmitMovesTheQuietTime()
    {
        QuietRule rule;
        rule.setMinutes(2);
        rule.start(0);
        rule.onTransmit(60000);
        QVERIFY(!rule.quiet(120000));
        QVERIFY(!rule.quiet(179999));
        QVERIFY(rule.quiet(180000));
    }

    void squelchOnHeardResets()
    {
        QuietRule rule;
        rule.setMinutes(2);
        rule.start(0);
        rule.onLevel(90000, true, true, -130.0, -120.0);
        QVERIFY(!rule.quiet(209999));
        QVERIFY(rule.quiet(210000));
    }

    void squelchOffSixDbAboveTheFloorResets()
    {
        QuietRule rule;
        rule.setMinutes(2);
        rule.start(0);
        rule.onLevel(60000, /*squelchOn*/ false, false, -120.0 + 6.1, -120.0);
        QVERIFY(!rule.quiet(120000));
        QVERIFY(rule.quiet(180000));
    }

    void squelchOffBelowSixDbDoesNotReset()
    {
        QuietRule rule;
        rule.setMinutes(2);
        rule.start(0);
        rule.onLevel(60000, false, false, -120.0 + 5.9, -120.0);
        QVERIFY(rule.quiet(120000));
    }

    void minutesAreSettable()
    {
        QuietRule rule;
        rule.setMinutes(5);
        rule.start(1000);
        QVERIFY(!rule.quiet(1000 + 5 * 60000 - 1));
        QVERIFY(rule.quiet(1000 + 5 * 60000));
    }
};

QTEST_GUILESS_MAIN(TestRecordingQuietRule)
#include "tst_recording_quiet_rule.moc"
