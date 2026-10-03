// =================================================================
// tests/tst_diversity_partner_ddc.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Thetis file names
// appear only in comments naming the upstream lines each assertion checks.
//
// Diversity's second leg is DDC1, synchronized to DDC0. Thetis tunes DDC0,
// DDC1 and DDC2 to the RX1 frequency on every board outside the Hermes
// class (console.cs:15398-15423 UpdateRX1DDSFreq [v2.10.3.15]), so the
// partner always mixes the same signal as the primary. On the Hermes class
// DDC1 is RX2's own receiver (console.cs:15446-15459 UpdateRX2DDSFreq [v2.10.3.15]).
//
// Protocol 2: P2RadioConnection keeps one frequency per DDC and only the
// DDCs that carry a receiver were ever tuned, so DDC1 went out at 0 Hz.
// Protocol 1: networkproto1.c:497-511 [v2.10.3.15] already sends RX1's
// frequency in DDC1's bank on the nddc == 5 boards; that case pins it.
// =================================================================

#include <QtTest/QtTest>

#include "core/P2RadioConnection.h"
#include "core/codec/CodecContext.h"
#include "core/codec/P1CodecStandard.h"

using namespace NereusSDR;

namespace {

constexpr int kHighPriorityLen = 1444;

// CmdHighPriority carries DDC n's phase word at bytes 9 + 4n .. 12 + 4n.
quint32 ddcPhaseWord(const quint8* buf, int ddc)
{
    const int at = 9 + 4 * ddc;
    return (quint32(buf[at]) << 24) | (quint32(buf[at + 1]) << 16)
         | (quint32(buf[at + 2]) << 8) | quint32(buf[at + 3]);
}

} // namespace

class TestDiversityPartnerDdc : public QObject {
    Q_OBJECT

private slots:
    // Diversity on: slice A's receiver sits on DDC0 and DDC1 is its
    // synchronized partner with no receiver of its own.
    void p2_partner_ddc1_follows_rx1_when_rx1_is_on_ddc0()
    {
        P2RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.setReceiverFrequency(0, 14'200'000);

        quint8 buf[kHighPriorityLen] = {};
        conn.composeCmdHighPriorityForTest(buf);

        QVERIFY(ddcPhaseWord(buf, 0) != 0);
        QCOMPARE(ddcPhaseWord(buf, 1), ddcPhaseWord(buf, 0));
        QCOMPARE(ddcPhaseWord(buf, 2), ddcPhaseWord(buf, 0));
    }

    // Diversity off: RX1 is on DDC2, and Thetis still keeps DDC0 and DDC1
    // on its frequency, so turning diversity on finds the pair already
    // tuned (captures/thetis-3865-lsb-pcap-analysis.md: RX[0] and RX[1]
    // "Not active, set same as RX2").
    void p2_pair_follows_rx1_when_rx1_is_on_ddc2()
    {
        P2RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::OrionMKII);
        conn.setReceiverFrequency(2, 7'150'000);

        quint8 buf[kHighPriorityLen] = {};
        conn.composeCmdHighPriorityForTest(buf);

        QVERIFY(ddcPhaseWord(buf, 2) != 0);
        QCOMPARE(ddcPhaseWord(buf, 0), ddcPhaseWord(buf, 2));
        QCOMPARE(ddcPhaseWord(buf, 1), ddcPhaseWord(buf, 2));
    }

    // RX2 (DDC3) and the receivers above it keep their own frequencies.
    void p2_rx2_and_above_are_not_touched()
    {
        P2RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.setReceiverFrequency(2, 7'150'000);
        conn.setReceiverFrequency(3, 21'300'000);

        quint8 buf[kHighPriorityLen] = {};
        conn.composeCmdHighPriorityForTest(buf);

        QVERIFY(ddcPhaseWord(buf, 3) != ddcPhaseWord(buf, 2));
        QCOMPARE(ddcPhaseWord(buf, 1), ddcPhaseWord(buf, 2));
    }

    // Hermes class: DDC1 is RX2's receiver, tuned on its own.
    void p2_hermes_class_ddc1_stays_rx2()
    {
        P2RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::HermesII);
        conn.setReceiverFrequency(0, 14'200'000);

        quint8 buf[kHighPriorityLen] = {};
        conn.composeCmdHighPriorityForTest(buf);
        QVERIFY(ddcPhaseWord(buf, 0) != 0);
        QCOMPARE(ddcPhaseWord(buf, 1), quint32(0));

        conn.setReceiverFrequency(1, 3'750'000);
        conn.composeCmdHighPriorityForTest(buf);
        QVERIFY(ddcPhaseWord(buf, 1) != 0);
        QVERIFY(ddcPhaseWord(buf, 1) != ddcPhaseWord(buf, 0));
    }

    // Protocol 1, Orion class (nddc == 5) with diversity: DDC1's bank
    // (bank 3, C0 0x06) carries RX1's frequency, not RX2's or TX's.
    void p1_orion_class_partner_bank_carries_rx1()
    {
        P1CodecStandard codec;
        CodecContext ctx;
        ctx.diversity     = true;
        ctx.p1PsNDdc      = 5;
        ctx.activeRxCount = 5;
        ctx.rxFreqHz[0]   = 14'200'000;
        ctx.rxFreqHz[1]   = 0;
        ctx.txFreqHz      = 7'000'000;

        quint8 out[5] = {};
        codec.composeCcForBank(3, ctx, out);

        const quint32 hz = (quint32(out[1]) << 24) | (quint32(out[2]) << 16)
                         | (quint32(out[3]) << 8) | quint32(out[4]);
        QCOMPARE(out[0] & 0xFE, 0x06);
        QCOMPARE(hz, quint32(14'200'000));
    }
};

QTEST_APPLESS_MAIN(TestDiversityPartnerDdc)
#include "tst_diversity_partner_ddc.moc"
