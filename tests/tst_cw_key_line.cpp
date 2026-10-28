// no-port-check: NereusSDR-original test. Thetis and mi0bot file names
// appear only as cites for the bit layout asserted.
// =================================================================
// tests/tst_cw_key_line.cpp  (NereusSDR)
// =================================================================
//
// CwKeyLine (R-CW-12, R-CW-15): each setter's bit in p1Bits (dot 4, dash 2,
// cwx 1, HL2 cwxPtt 8; Thetis networkproto1.c:738-741 [v2.10.3.15], mi0bot
// networkproto1.c:1247-1252 [v2.10.3.13-beta2]), the change callback, and
// two writer threads driving different bits that never lose one.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, the host key line tests. J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <atomic>
#include <thread>

#include "core/CwKeyLine.h"

using namespace NereusSDR;

class TestCwKeyLine : public QObject {
    Q_OBJECT
private slots:
    void eachSetterAloneSetsItsBit_data()
    {
        QTest::addColumn<int>("which");
        QTest::addColumn<int>("p1");
        QTest::addColumn<int>("hl2");
        QTest::newRow("dot")    << 0 << 4 << 4;
        QTest::newRow("dash")   << 1 << 2 << 2;
        QTest::newRow("cwx")    << 2 << 1 << 1;
        QTest::newRow("cwxPtt") << 3 << 0 << 8;
    }
    void eachSetterAloneSetsItsBit()
    {
        QFETCH(int, which);
        QFETCH(int, p1);
        QFETCH(int, hl2);
        CwKeyLine line;
        QCOMPARE(int(line.p1Bits(false)), 0);
        QCOMPARE(int(line.p1Bits(true)), 0);
        switch (which) {
        case 0: line.setDot(true); break;
        case 1: line.setDash(true); break;
        case 2: line.setCwx(true); break;
        default: line.setCwxPtt(true); break;
        }
        QCOMPARE(int(line.p1Bits(false)), p1);
        QCOMPARE(int(line.p1Bits(true)), hl2);
        QCOMPARE(line.dot(), which == 0);
        QCOMPARE(line.dash(), which == 1);
        QCOMPARE(line.cwx(), which == 2);
        QCOMPARE(line.cwxPtt(), which == 3);

        line.setDot(false);
        line.setDash(false);
        line.setCwx(false);
        line.setCwxPtt(false);
        QCOMPARE(int(line.p1Bits(true)), 0);
    }

    void allBitsCombine()
    {
        CwKeyLine line;
        line.setDot(true);
        line.setCwx(true);
        QCOMPARE(int(line.p1Bits(false)), 5);
        line.setDash(true);
        line.setCwxPtt(true);
        QCOMPARE(int(line.p1Bits(false)), 7);
        QCOMPARE(int(line.p1Bits(true)), 15);
    }

    // The callback runs on the writer's thread for a change only.
    void onChangeCalledOnChangeOnly()
    {
        CwKeyLine line;
        int calls = 0;
        line.setOnChange([&calls]() { ++calls; });
        line.setDot(true);
        QCOMPARE(calls, 1);
        line.setDot(true);
        QCOMPARE(calls, 1);
        line.setDash(false);
        QCOMPARE(calls, 1);
        line.setDash(true);
        line.setDot(false);
        QCOMPARE(calls, 3);
    }

    // Two threads, each driving its own bits 100,000 times: neither
    // thread's writes ever undo the other's. Each thread checks its own bits
    // after every write, and the last state holds both threads' last values.
    void concurrentWritersNeverLoseABit()
    {
        constexpr int kIterations = 100000;
        CwKeyLine line;
        std::atomic<int> lostA{0};
        std::atomic<int> lostB{0};
        std::thread a([&]() {
            for (int i = 0; i < kIterations; ++i) {
                const bool on = (i & 1) == 0;
                line.setDot(on);
                line.setCwx(!on);
                if (line.dot() != on || line.cwx() != !on) {
                    lostA.fetch_add(1);
                }
            }
            line.setDot(true);
            line.setCwx(true);
        });
        std::thread b([&]() {
            for (int i = 0; i < kIterations; ++i) {
                const bool on = (i & 1) != 0;
                line.setDash(on);
                line.setCwxPtt(!on);
                if (line.dash() != on || line.cwxPtt() != !on) {
                    lostB.fetch_add(1);
                }
            }
            line.setDash(true);
            line.setCwxPtt(false);
        });
        a.join();
        b.join();
        QCOMPARE(lostA.load(), 0);
        QCOMPARE(lostB.load(), 0);
        QCOMPARE(int(line.p1Bits(true)), 0x07);
    }
};

QTEST_GUILESS_MAIN(TestCwKeyLine)
#include "tst_cw_key_line.moc"
