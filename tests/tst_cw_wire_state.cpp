// no-port-check: NereusSDR-original. Tests for the CW wire state helpers;
// upstream file names appear only in cites for the expected values.
// =================================================================
// tests/tst_cw_wire_state.cpp  (NereusSDR)
// =================================================================
//
// CwWireState defaults and clamps, the hang, RF delay and radio sidetone
// level helpers, and the P2 CmdTx byte 5 composer (R-CW-11, R-CW-13,
// R-CW-16, R-CW-38).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the CW wire state helper cases. J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/codec/CodecContext.h"
#include "core/codec/CwWireState.h"

using namespace NereusSDR;

class TestCwWireState : public QObject {
    Q_OBJECT
private slots:
    void defaults_areThetisKeyerDefaultsWithEnableOff()
    {
        const CwWireState s;
        QCOMPARE(s.enable, false);
        QCOMPARE(s.breakIn, false);
        QCOMPARE(s.keyerMode, int(CwKeyerMode::IambicB));
        QCOMPARE(s.speedWpm, 25);
        QCOMPARE(s.weight, 50);
        QCOMPARE(s.strictSpacing, false);
        QCOMPARE(s.reversePaddles, false);
        QCOMPARE(s.sidetoneLevel, 0);
        QCOMPARE(s.sidetoneFreqHz, 600);
        QCOMPARE(s.rfDelayMs, 7);
        QCOMPARE(s.hangMs, 310);
        QCOMPARE(s.edgeLengthMs, 9);
        QCOMPARE(s.qskTxAttenuation, false);

        const CodecContext ctx;
        QVERIFY(ctx.cw == CwWireState{});
        QCOMPARE(ctx.p2CwEdgeLength, 9);
    }

    void equality_comparesEveryField()
    {
        CwWireState a;
        CwWireState b;
        QVERIFY(a == b);
        b.qskTxAttenuation = true;
        QVERIFY(!(a == b));
    }

    void clamps_data()
    {
        QTest::addColumn<int>("speed");
        QTest::addColumn<int>("weight");
        QTest::addColumn<int>("level");
        QTest::addColumn<int>("rfDelay");
        QTest::addColumn<int>("hang");
        QTest::addColumn<int>("speedOut");
        QTest::addColumn<int>("weightOut");
        QTest::addColumn<int>("levelOut");
        QTest::addColumn<int>("rfDelayOut");
        QTest::addColumn<int>("hangOut");

        QTest::newRow("in range") << 30 << 55 << 90 << 12 << 310
                                  << 30 << 55 << 90 << 12 << 310;
        QTest::newRow("low") << 0 << 10 << -5 << -1 << -20
                             << 1 << 33 << 0 << 0 << 0;
        QTest::newRow("high") << 99 << 90 << 200 << 1000 << 1500
                              << 60 << 66 << 127 << 255 << 1023;
        QTest::newRow("edges") << 60 << 33 << 127 << 255 << 1023
                               << 60 << 33 << 127 << 255 << 1023;
    }

    void clamps()
    {
        QFETCH(int, speed);
        QFETCH(int, weight);
        QFETCH(int, level);
        QFETCH(int, rfDelay);
        QFETCH(int, hang);
        CwWireState s;
        s.speedWpm = speed;
        s.weight = weight;
        s.sidetoneLevel = level;
        s.rfDelayMs = rfDelay;
        s.hangMs = hang;
        s.sidetoneFreqHz = 5000;  // not a clamped field
        applyCwWireClamps(s);
        QTEST(s.speedWpm, "speedOut");
        QTEST(s.weight, "weightOut");
        QTEST(s.sidetoneLevel, "levelOut");
        QTEST(s.rfDelayMs, "rfDelayOut");
        QTEST(s.hangMs, "hangOut");
        QCOMPARE(s.sidetoneFreqHz, 5000);
    }

    void hang()
    {
        QCOMPARE(cwHangMs(CwBreakInMode::Semi, 300, 10), 310);
        QCOMPARE(cwHangMs(CwBreakInMode::Semi, 1000, 500), 1023);
        QCOMPARE(cwHangMs(CwBreakInMode::Qsk, 300, 10), 10);
        QCOMPARE(cwHangMs(CwBreakInMode::Off, 300, 10), 0);
    }

    void rfDelay()
    {
        QCOMPARE(cwRfDelayMs(7, 25), 7);
        QCOMPARE(cwRfDelayMs(300, 5), 180);
        QCOMPARE(cwRfDelayMs(200, 30), 30);
        // Speed 0 is treated as 1 (900), then the 255 cap, then 50.
        QCOMPARE(cwRfDelayMs(50, 0), 50);
        QCOMPARE(cwRfDelayMs(1000, 0), 255);
    }

    void sidetoneLevel()
    {
        // 63.5 rounds to 64.
        QCOMPARE(cwRadioSidetoneLevel(true, true, false, CwBreakInMode::Semi, 0.5f), 64);
        QCOMPARE(cwRadioSidetoneLevel(true, true, false, CwBreakInMode::Qsk, 0.5f), 64);
        // 36.5 rounds to 37.
        QCOMPARE(cwRadioSidetoneLevel(true, true, false, CwBreakInMode::Off, 0.5f), 37);
        QCOMPARE(cwRadioSidetoneLevel(true, true, true, CwBreakInMode::Semi, 0.5f), 0);
        QCOMPARE(cwRadioSidetoneLevel(true, false, false, CwBreakInMode::Semi, 1.0f), 0);
        QCOMPARE(cwRadioSidetoneLevel(false, true, false, CwBreakInMode::Semi, 1.0f), 0);
        QCOMPARE(cwRadioSidetoneLevel(true, true, false, CwBreakInMode::Semi, 1.0f), 127);
        QCOMPARE(cwRadioSidetoneLevel(true, true, false, CwBreakInMode::Semi, 0.0f), 0);
    }

    void modeControl_data()
    {
        QTest::addColumn<QString>("field");
        QTest::addColumn<int>("expected");
        QTest::newRow("eer") << QStringLiteral("eer") << 0x01;
        QTest::newRow("enable") << QStringLiteral("enable") << 0x02;
        QTest::newRow("reverse") << QStringLiteral("reverse") << 0x04;
        QTest::newRow("iambic A") << QStringLiteral("iambicA") << 0x08;
        QTest::newRow("level") << QStringLiteral("level") << 0x10;
        QTest::newRow("iambic B") << QStringLiteral("iambicB") << (0x08 | 0x20);
        QTest::newRow("strict") << QStringLiteral("strict") << 0x40;
        QTest::newRow("break-in") << QStringLiteral("breakIn") << 0x80;
        QTest::newRow("none") << QString() << 0x00;
    }

    void modeControl()
    {
        QFETCH(QString, field);
        QFETCH(int, expected);
        CwWireState s;
        s.keyerMode = int(CwKeyerMode::Straight);
        bool eer = false;
        if (field == QLatin1String("eer")) {
            eer = true;
        } else if (field == QLatin1String("enable")) {
            s.enable = true;
        } else if (field == QLatin1String("reverse")) {
            s.reversePaddles = true;
        } else if (field == QLatin1String("iambicA")) {
            s.keyerMode = int(CwKeyerMode::IambicA);
        } else if (field == QLatin1String("level")) {
            s.sidetoneLevel = 1;
        } else if (field == QLatin1String("iambicB")) {
            s.keyerMode = int(CwKeyerMode::IambicB);
        } else if (field == QLatin1String("strict")) {
            s.strictSpacing = true;
        } else if (field == QLatin1String("breakIn")) {
            s.breakIn = true;
        }
        QCOMPARE(int(composeP2CwModeControl(s, eer)), expected);
    }

    void modeControl_keepsEerWithEverythingSet()
    {
        CwWireState s;
        s.enable = true;
        s.breakIn = true;
        s.reversePaddles = true;
        s.strictSpacing = true;
        s.sidetoneLevel = 90;
        s.keyerMode = int(CwKeyerMode::IambicB);
        QCOMPARE(int(composeP2CwModeControl(s, true)), 0xFF);
        QCOMPARE(int(composeP2CwModeControl(s, false)), 0xFE);
    }
};

QTEST_APPLESS_MAIN(TestCwWireState)
#include "tst_cw_wire_state.moc"
