// no-port-check: test-only. Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// =================================================================
// Protocol 2: the band outputs reach the wire, and the receive low-pass
// follows RX1 as Thetis chooses it.
// =================================================================
//
// Plan Task 14 (3M-1 transmit, Phase 3F section 16.3.2, R-R3-49).
//
// 1. Byte 1401 of the high-priority packet carries the OC outputs:
//      From Thetis ChannelMaster/network.c:1030-1031 [v2.10.3.15]
//        // Open Collector Outputs
//        packetbuf[1401] = (prn->oc_output << 1) & 0xfe;
//    No NereusSDR Protocol 2 codec wrote it, so an ANAN-G2 sent no band
//    data to an amplifier or band decoder at all. The band rule is the one
//    Penny.cs applies on every protocol: the transmitting VFO's band while
//    keyed, VFO A's while not (Penny.cs:174-177 [v2.10.3.15]), by VFO
//    frequency (console.cs:29101-29102 [v2.10.3.15]).
//
// 2. The receive low-pass (Alex0 while unkeyed) is RX1's, or the higher of
//    RX1 and RX2 when RX2 shares the front end:
//      From Thetis console.cs:15487-15498 [v2.10.3.15]
//        private void UpdateAlexTXFilter()
//        { if (!_mox) {
//            if (!_rx2_preamp_present && chkRX2.Checked)
//            { if (rx1_dds_freq_mhz > rx2_dds_freq_mhz) setAlexLPF(rx1_dds_freq_mhz, false);
//              else setAlexLPF(rx2_dds_freq_mhz, false); }
//            else setAlexLPF(rx1_dds_freq_mhz, false); } }
//    NereusSDR used whichever DDC was retuned last, so adding slice B on a
//    lower band put slice A behind B's low-pass.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/OcMatrix.h"
#include "core/P2RadioConnection.h"
#include "core/ReceiverManager.h"
#include "core/TxSliceArbiter.h"
#include "core/codec/AlexFilterMap.h"
#include "core/codec/P2CodecSaturn.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr double k10mHz = 28400000.0;
constexpr double k20mHz = 14200000.0;
constexpr double k40mHz =  7100000.0;
constexpr double k80mHz =  3700000.0;

constexpr int kOcByte      = 1401;
constexpr int kAlex0Offset = 1432;

class ConnectedP2 final : public P2RadioConnection {
public:
    ConnectedP2() { setState(ConnectionState::Connected); }
};

struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

quint32 readBE32(const quint8* buf, int offset)
{
    return (quint32(buf[offset])     << 24)
         | (quint32(buf[offset + 1]) << 16)
         | (quint32(buf[offset + 2]) << 8)
         |  quint32(buf[offset + 3]);
}

// Inverse of the LPF scatter in P2CodecOrionMkII::buildAlex0.
// Bit map from Thetis ChannelMaster/netInterface.c:691-702 [v2.10.3.15].
quint8 lpfMaskFromReg(quint32 reg)
{
    quint8 bits = 0;
    if (reg & (1u << 20)) { bits |= 0x01; }
    if (reg & (1u << 21)) { bits |= 0x02; }
    if (reg & (1u << 22)) { bits |= 0x04; }
    if (reg & (1u << 23)) { bits |= 0x08; }
    if (reg & (1u << 29)) { bits |= 0x10; }
    if (reg & (1u << 30)) { bits |= 0x20; }
    if (reg & (1u << 31)) { bits |= 0x40; }
    return bits;
}

quint8 highPriorityByte(P2RadioConnection& conn, int offset)
{
    quint8 buf[1444] = {};
    conn.composeCmdHighPriorityForTest(buf);
    return buf[offset];
}

quint8 alex0Lpf(P2RadioConnection& conn)
{
    quint8 buf[1444] = {};
    conn.composeCmdHighPriorityForTest(buf);
    return lpfMaskFromReg(readBE32(buf, kAlex0Offset));
}

// The wire form of an OC mask, network.c:1031.
quint8 wireOc(quint8 mask)
{
    return quint8((mask << 1) & 0xfe);
}

// An ANAN-G2 (Saturn board, P2CodecSaturn: stream 0 -> DDC2, stream 1 ->
// DDC3, ...) with an injected, Connected connection and the production
// ReceiverManager -> connection wiring.
struct G2Session {
    G2Session()
    {
        AppSettings::instance().clear();
        oc.setPin(Band::Band20m, 0, /*tx=*/false, true);
        oc.setPin(Band::Band40m, 1, /*tx=*/false, true);
        oc.setPin(Band::Band40m, 2, /*tx=*/true,  true);
        oc.setPin(Band::Band20m, 3, /*tx=*/true,  true);

        model.setBoardForTest(HPSDRHW::Saturn);
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.setOcMatrix(&oc);
        model.configureStreamPool(5, 5, 192000);
        model.receiverManager()->setP2Codec(&codec);
        model.injectConnectionForTest(&conn);
        detach.model = &model;
        model.wireReceiverManagerHardwarePushesForTest();
        for (int st = 0; st < 5; ++st) {
            model.receiverManager()->createReceiver();
        }
    }
    ~G2Session() { AppSettings::instance().clear(); }

    int add(double hz)
    {
        const int id = model.addSlice();
        model.sliceById(id)->setFrequency(hz);
        return id;
    }

    OcMatrix         oc;
    P2CodecSaturn    codec;
    RadioModel       model;
    ConnectedP2      conn;
    DetachConnection detach;
};

} // namespace

class TestP2BandOutputsAndRxLpf : public QObject {
    Q_OBJECT

private slots:

    // ── Byte 1401 on the G2, unkeyed and keyed ───────────────────────────
    void g2_byte1401_carriesTheBandOutputs()
    {
        G2Session s;
        const int a = s.add(k20mHz);
        const int b = s.add(k40mHz);
        s.model.setActiveSlice(a);

        // Unkeyed: the receive mask of RX1 (slice A).
        QCOMPARE(highPriorityByte(s.conn, kOcByte),
                 wireOc(s.oc.maskFor(Band::Band20m, /*tx=*/false)));

        // Keyed on B: the transmit mask of B's band.
        QVERIFY(s.model.txSliceArbiter()->requestHandoff(b));
        s.conn.setMox(true);
        QCOMPARE(highPriorityByte(s.conn, kOcByte),
                 wireOc(s.oc.maskFor(Band::Band40m, /*tx=*/true)));
        s.conn.setMox(false);
        QCOMPARE(highPriorityByte(s.conn, kOcByte),
                 wireOc(s.oc.maskFor(Band::Band20m, /*tx=*/false)));

        // Slice A closed: B stands in for RX1.
        s.model.removeSlice(a);
        QCOMPARE(highPriorityByte(s.conn, kOcByte),
                 wireOc(s.oc.maskFor(Band::Band40m, /*tx=*/false)));
    }

    // ── The G2 receive low-pass is RX1's, not the last retune's ──────────
    //
    // The G2's RX2 has its own front end (rx2PreampPresent), so Thetis uses
    // RX1 alone.
    //
    // Shared-input filters, ruling (c) 2026-09-30: both slices here share
    // ADC0's input, so the low-pass now follows the highest counted slice.
    // A is the highest at every step below, so the answers are the same;
    // tst_shared_input_filters covers a higher B.
    void g2_receiveLowPass_isRx1s()
    {
        G2Session s;
        const int a = s.add(k20mHz);
        QCOMPARE(alex0Lpf(s.conn), codec::alex::computeLpf(k20mHz / 1e6));

        const int b = s.add(k80mHz);
        QCOMPARE(alex0Lpf(s.conn), codec::alex::computeLpf(k20mHz / 1e6));

        s.model.sliceById(b)->setFrequency(k40mHz);
        QCOMPARE(alex0Lpf(s.conn), codec::alex::computeLpf(k20mHz / 1e6));

        // A's own retune moves it.
        s.model.sliceById(a)->setFrequency(k10mHz);
        QCOMPARE(alex0Lpf(s.conn), codec::alex::computeLpf(k10mHz / 1e6));

        // A closed: B stands in for RX1.
        s.model.removeSlice(a);
        QCOMPARE(alex0Lpf(s.conn), codec::alex::computeLpf(k40mHz / 1e6));
    }

    // ── Connection level: the higher of the two where RX2 shares the filter
    void receiveLowPass_rule_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<bool>("separateRx2");
        QTest::newRow("Hermes (RX2 shares the filter)") << int(HPSDRHW::Hermes) << false;
        QTest::newRow("G2 (RX2 has its own)")           << int(HPSDRHW::Saturn) << true;
    }
    void receiveLowPass_rule()
    {
        QFETCH(int, board);
        QFETCH(bool, separateRx2);
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW(board));
        QCOMPARE(BoardCapsTable::forBoard(HPSDRHW(board)).rx2PreampPresent, separateRx2);

        // RX1 on DDC2, RX2 on DDC3, as the G2 codec routes them.
        conn.setLiveReceiverSlots((1u << 2) | (1u << 3));
        conn.setReceiverFrequency(2, quint64(k20mHz));
        conn.setReceiverFrequency(3, quint64(k80mHz));
        QCOMPARE(alex0Lpf(conn), codec::alex::computeLpf(k20mHz / 1e6));

        conn.setReceiverFrequency(3, quint64(k10mHz));
        QCOMPARE(alex0Lpf(conn),
                 codec::alex::computeLpf((separateRx2 ? k20mHz : k10mHz) / 1e6));

        // RX2 closed: RX1 alone.
        conn.setLiveReceiverSlots(1u << 2);
        QCOMPARE(alex0Lpf(conn), codec::alex::computeLpf(k20mHz / 1e6));
    }

    // ── The band comes from the VFO, not the DDC centre ──────────────────
    void g2_bandIsTheVfosNotTheCentres()
    {
        G2Session s;
        constexpr double kVfoHz    = 14010000.0;
        constexpr double kCentreHz = 13950000.0;
        QVERIFY(bandFromFrequency(kCentreHz) != Band::Band20m);

        const int a = s.add(kVfoHz);
        QVERIFY(s.model.requestStreamCentre(a, kCentreHz));
        QCOMPARE(highPriorityByte(s.conn, kOcByte),
                 wireOc(s.oc.maskFor(Band::Band20m, /*tx=*/false)));
    }
    // ── The receive low-pass passes the highest live receiver (M6) ───────
    //
    // Thetis's two-receiver rule (console.cs:15487-15499 [v2.10.3.15])
    // extended to every live slice on a shared front end, as the RX1
    // stand-in extends RX1. A third slice on a higher band.
    void receiveLowPass_passesTheHighestLiveReceiver_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<bool>("separateRx2");
        QTest::newRow("Hermes (one front end)") << int(HPSDRHW::Hermes) << false;
        QTest::newRow("G2 (RX2 has its own)")   << int(HPSDRHW::Saturn) << true;
    }
    void receiveLowPass_passesTheHighestLiveReceiver()
    {
        QFETCH(int, board);
        QFETCH(bool, separateRx2);
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW(board));

        conn.setLiveReceiverSlots((1u << 2) | (1u << 3) | (1u << 4));
        conn.setReceiverFrequency(2, quint64(k80mHz));
        conn.setReceiverFrequency(3, quint64(k40mHz));
        conn.setReceiverFrequency(4, quint64(k10mHz));
        QCOMPARE(alex0Lpf(conn),
                 codec::alex::computeLpf((separateRx2 ? k80mHz : k10mHz) / 1e6));

        // The third closes: the higher of the two left.
        conn.setLiveReceiverSlots((1u << 2) | (1u << 3));
        QCOMPARE(alex0Lpf(conn),
                 codec::alex::computeLpf((separateRx2 ? k80mHz : k40mHz) / 1e6));

        // The middle one closes instead: the third is still passed.
        conn.setLiveReceiverSlots((1u << 2) | (1u << 4));
        QCOMPARE(alex0Lpf(conn),
                 codec::alex::computeLpf((separateRx2 ? k80mHz : k10mHz) / 1e6));
    }
};

QTEST_MAIN(TestP2BandOutputsAndRxLpf)
#include "tst_p2_band_outputs_and_rx_lpf.moc"
