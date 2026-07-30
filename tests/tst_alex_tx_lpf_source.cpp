// =================================================================
// tests/tst_alex_tx_lpf_source.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Expected wire
// values are cited to Thetis in comments, but nothing here is a port.
//
// RF-SAFETY. The Alex TX low-pass filter must be selected from the
// TRANSMIT frequency, never from a receive frequency.
//
// Before this test existed, P2RadioConnection::setReceiverFrequency
// recomputed BOTH the HPF and the LPF from the receive frequency it was
// handed, and both Alex words took their LPF from that single mask. On a
// 2-ADC radio that means the TX low-pass tracked whichever DDC was
// retuned last:
//
//   Slice A on 28.400 MHz (10 m), slice B added on 3.700 MHz (80 m) to
//   monitor a net. Binding B calls setReceiverFrequency(3, 3700000),
//   which sets the LPF mask to 0x04 (the 80 m low-pass). Keying up on
//   slice A then puts ~100 W of 28.4 MHz into a ~4 MHz low-pass.
//
// Thetis keeps two independent LPF masks and routes writes by intent:
//   From Thetis ChannelMaster/netInterface.c:682-726 [v2.10.3.15]
//     void SetAlexLPFBits(int bits, bool isTX, bool isMox)
//     if (isMox || isTX)   -> write Alex1 (prbpfilter2, Alex1LPFMask)
//     if (isMox || !isTX)  -> write Alex0 (prbpfilter,  AlexLPFMask)
//   Upstream comment preserved verbatim (netInterface.c:686-690):
//     // LPF bits can be used in older radioas as part of RX filtering too.
//     // Change to protocol 2 from 4.3 onwards: TX settings are encoded in
//     // the Alex1 word to remain comparible with older hardware, the logic
//     // will be:
//     // if MOX, write settings to alex0 and alex1
//     // if not MOX, write to alex1 if a TX setting else write to alex0
//
// and every TX-word write is fed from the transmit frequency:
//   From Thetis console.cs:15464-15468 [v2.10.3.15]
//     private void UpdateTXDDSFreq()
//     { ... setAlexLPF(tx_dds_freq_mhz, true); ... }
// Upstream inline attribution preserved verbatim (console.cs:15471):
//   if (MOX)//[2.10.3.13]MW0LGE
//   From Thetis console.cs:15487-15498 [v2.10.3.15]
//     private void UpdateAlexTXFilter()
//     { if (!_mox) { ... setAlexLPF(rx1_dds_freq_mhz, false); } }
//
// so a receive retune can only ever reach Alex0, and only while the radio
// is not transmitting.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/P2RadioConnection.h"
#include "core/TxSliceArbiter.h"
#include "core/accessories/AlexController.h"
#include "core/codec/AlexFilterMap.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// Byte offsets from Thetis ChannelMaster/network.c:1040-1050 [v2.10.3.15]:
// Alex1 at 1428-1431, Alex0 at 1432-1435.
constexpr int kAlex1Offset = 1428;
constexpr int kAlex0Offset = 1432;

quint32 readBE32(const quint8* buf, int offset)
{
    return (quint32(buf[offset])     << 24)
         | (quint32(buf[offset + 1]) << 16)
         | (quint32(buf[offset + 2]) << 8)
         |  quint32(buf[offset + 3]);
}

// Inverse of the LPF scatter in P2CodecOrionMkII::buildAlex0 /
// P2RadioConnection::buildAlex0. Recovers the AlexFilterMap mask so the
// assertions can be written against computeLpf() directly.
// Bit map from Thetis ChannelMaster/netInterface.c:691-702 [v2.10.3.15]:
//   30_20[20], 60_40[21], 80[22], 160[23], 6[29], 12_10[30], 17_15[31]
quint8 lpfMaskFromReg(quint32 reg)
{
    quint8 bits = 0;
    if (reg & (1u << 20)) { bits |= 0x01; }  // 30/20 m
    if (reg & (1u << 21)) { bits |= 0x02; }  // 60/40 m
    if (reg & (1u << 22)) { bits |= 0x04; }  // 80 m
    if (reg & (1u << 23)) { bits |= 0x08; }  // 160 m
    if (reg & (1u << 29)) { bits |= 0x10; }  // 6 m
    if (reg & (1u << 30)) { bits |= 0x20; }  // 12/10 m
    if (reg & (1u << 31)) { bits |= 0x40; }  // 17/15 m
    return bits;
}

struct AlexWords {
    quint8 rxLpf{0};   // Alex0 LPF mask
    quint8 txLpf{0};   // Alex1 LPF mask
};

class ConnectedP2RadioConnection final : public P2RadioConnection {
public:
    AntennaRouting lastRouting;

    ConnectedP2RadioConnection()
    {
        setState(ConnectionState::Connected);
    }

    void setAntennaRouting(AntennaRouting routing) override
    {
        lastRouting = routing;
        P2RadioConnection::setAntennaRouting(routing);
    }
};

AlexWords composeAlexWords(P2RadioConnection& conn)
{
    quint8 buf[1444] = {};
    conn.composeCmdHighPriorityForTest(buf);
    return AlexWords{
        lpfMaskFromReg(readBE32(buf, kAlex0Offset)),
        lpfMaskFromReg(readBE32(buf, kAlex1Offset)),
    };
}

// The DDC the 2-ADC P2 boards put the first receiver on (console.cs:8216
// UpdateDDCs). Slice B in the bug report landed on the next stream up.
constexpr int kStreamA = 2;
constexpr int kStreamB = 3;

constexpr quint64 k10mHz = 28400000ULL;   // slice A, transmitting
constexpr quint64 k80mHz =  3700000ULL;   // slice B, monitoring a net

} // namespace

class TestAlexTxLpfSource : public QObject {
    Q_OBJECT
private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void crossBandMoxUsesTheTxBoundSliceForTxRoutingAndLpf()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);
        model.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);

        auto* conn = new ConnectedP2RadioConnection();
        conn->setBoardForTest(HPSDRHW::Saturn);
        conn->setReceiverFrequency(kStreamA, k80mHz);
        model.injectConnectionForTest(conn);

        const int a = model.addSlice();
        SliceModel* const listening = model.sliceById(a);
        listening->setFrequency(k80mHz);
        model.setActiveSlice(0);

        const int c = model.addSlice();
        SliceModel* const transmitting = model.sliceById(c);
        transmitting->setFrequency(k10mHz);

        model.alexControllerMutable().setTxAnt(Band::Band80m, 1);
        model.alexControllerMutable().setTxAnt(Band::Band10m, 1);

        // Editing a listening slice records intent only. The physical Alex
        // state must change when that slice becomes TX-bound, even before
        // any second antenna click.
        transmitting->setTxAntenna(QStringLiteral("ANT3"));
        QCOMPARE(model.alexController().txAnt(Band::Band10m), 1);

        QVERIFY(model.requestTxHandoffToSlice(c));
        model.onMoxHardwareFlipped(/*isTx=*/true);

        QCOMPARE(conn->lastRouting.txAnt, 3);
        QCOMPARE(conn->lastRouting.trxAnt, 3);
        const AlexWords words = composeAlexWords(*conn);
        QCOMPARE(words.txLpf, codec::alex::computeLpf(28.4));
        QCOMPARE(words.rxLpf, codec::alex::computeLpf(28.4));

        model.injectConnectionForTest(nullptr);
        delete conn;
    }

    void differentBandEnumsSharingOnePhysicalPreselectorStayFiltered()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);
        model.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0); // Band20m
        const int c = model.addSlice();
        model.sliceById(c)->setFrequency(21200000.0); // Band15m

        QCOMPARE(codec::alex::computeRxPreselector(14.2, HPSDRHW::Saturn),
                 codec::alex::computeRxPreselector(21.2, HPSDRHW::Saturn));
        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::Filtered);
    }

    // ── The reported failure ────────────────────────────────────────────
    //
    // ANAN-G2 (Saturn). Slice A transmits on 10 m; slice B is added on
    // 80 m afterwards, so the 80 m receive retune is the LAST write to
    // reach the connection. The TX word must still carry the 10 m LPF.
    void rxRetuneOnAnotherBand_leavesTxLpfOnTheTxBand()
    {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);

        // Slice A: transmit and receive both on 10 m.
        conn.setTxFrequency(k10mHz);
        conn.setReceiverFrequency(kStreamA, k10mHz);

        // Operator adds slice B on 80 m to monitor a net. This is the
        // retune that used to drag the TX low-pass down to 4 MHz.
        conn.setReceiverFrequency(kStreamB, k80mHz);

        const AlexWords w = composeAlexWords(conn);
        QCOMPARE(w.txLpf, codec::alex::computeLpf(28.4));   // 0x20, 12/10 m
        QVERIFY2(w.txLpf != codec::alex::computeLpf(3.7),
                 "TX low-pass followed a receive retune onto 80 m");
    }

    // Same hazard, opposite order: the TX slice is retuned first and the
    // monitoring slice never moves again. Guards against a fix that merely
    // makes the LAST writer win.
    void txRetuneAfterRxRetune_stillSelectsTheTxBand()
    {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);

        conn.setReceiverFrequency(kStreamB, k80mHz);
        conn.setTxFrequency(k10mHz);
        conn.setReceiverFrequency(kStreamA, k10mHz);
        conn.setReceiverFrequency(kStreamB, k80mHz);

        QCOMPARE(composeAlexWords(conn).txLpf, codec::alex::computeLpf(28.4));
    }

    // The TX word tracks the transmit frequency across every LPF band.
    // Table from Thetis console.cs:7177-7241 setAlexLPF [v2.10.3.15], as
    // ported into AlexFilterMap::computeLpf.
    void txLpfFollowsTxFrequency_data()
    {
        QTest::addColumn<double>("txMhz");
        QTest::newRow("160m") <<  1.9;
        QTest::newRow("80m")  <<  3.7;
        QTest::newRow("40m")  <<  7.15;
        QTest::newRow("20m")  << 14.2;
        QTest::newRow("15m")  << 21.3;
        QTest::newRow("10m")  << 28.4;
        QTest::newRow("6m")   << 50.1;
    }

    void txLpfFollowsTxFrequency()
    {
        QFETCH(double, txMhz);

        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);

        // A receive slice parked on 80 m for the whole sweep — it must
        // never influence the TX word.
        conn.setReceiverFrequency(kStreamB, k80mHz);
        conn.setTxFrequency(static_cast<quint64>(txMhz * 1e6));

        QCOMPARE(composeAlexWords(conn).txLpf, codec::alex::computeLpf(txMhz));
    }

    // ── No regression for the single-slice case ─────────────────────────
    //
    // One slice, simplex: RX and TX are the same frequency, so both words
    // carry the same LPF and the wire is byte-identical to the pre-fix
    // output.
    void singleSliceSimplex_bothWordsCarryTheSameLpf()
    {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);

        conn.setReceiverFrequency(kStreamA, 14200000ULL);
        conn.setTxFrequency(14200000ULL);

        const AlexWords w = composeAlexWords(conn);
        QCOMPARE(w.txLpf, codec::alex::computeLpf(14.2));
        QCOMPARE(w.rxLpf, codec::alex::computeLpf(14.2));
    }

    // ── Thetis MOX routing (netInterface.c:682-726) ─────────────────────
    //
    // While transmitting, Alex0 must ALSO carry the TX low-pass: on older
    // hardware Alex0's LPF is the one in circuit. That is the `isMox`
    // branch, and losing it would put the receive band's low-pass in the
    // TX path on those radios.
    //   From Thetis console.cs:29083-29099 HdwMOXChanged [v2.10.3.15]
    //     if (tx) { ... UpdateTXDDSFreq(); ... }
    //   which calls setAlexLPF(tx_dds_freq_mhz, true) with _mox now true.
    void duringMox_alex0AlsoCarriesTheTxLpf()
    {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);

        conn.setTxFrequency(k10mHz);
        conn.setReceiverFrequency(kStreamB, k80mHz);
        conn.setMox(true);

        const AlexWords w = composeAlexWords(conn);
        QCOMPARE(w.txLpf, codec::alex::computeLpf(28.4));
        QCOMPARE(w.rxLpf, codec::alex::computeLpf(28.4));
    }

    // Dropping MOX hands Alex0 back to the receive-derived value.
    //   From Thetis console.cs:29140-29148 [v2.10.3.15] — the MOX-off arm
    //   calls UpdateRX1DDSFreq() (-> UpdateAlexTXFilter, now unguarded)
    //   before UpdateTXDDSFreq().
    void afterMox_alex0ReturnsToTheRxLpf()
    {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);

        conn.setTxFrequency(k10mHz);
        conn.setReceiverFrequency(kStreamB, k80mHz);
        conn.setMox(true);
        conn.setMox(false);

        const AlexWords w = composeAlexWords(conn);
        QCOMPARE(w.rxLpf, codec::alex::computeLpf(3.7));
        QCOMPARE(w.txLpf, codec::alex::computeLpf(28.4));
    }

    // A receive retune that arrives mid-transmission must not disturb
    // either word. Thetis cannot even reach the RX-derived write while
    // transmitting: UpdateAlexTXFilter is wrapped in `if (!_mox)`
    // (console.cs:15487-15498 [v2.10.3.15]).
    void rxRetuneDuringMox_doesNotDisturbEitherWord()
    {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);

        conn.setTxFrequency(k10mHz);
        conn.setReceiverFrequency(kStreamA, k10mHz);
        conn.setMox(true);

        const AlexWords before = composeAlexWords(conn);
        conn.setReceiverFrequency(kStreamB, k80mHz);
        const AlexWords after = composeAlexWords(conn);

        QCOMPARE(after.txLpf, before.txLpf);
        QCOMPARE(after.rxLpf, before.rxLpf);
        QCOMPARE(after.txLpf, codec::alex::computeLpf(28.4));
    }

    // ── The legacy (no-codec) composition path ──────────────────────────
    //
    // P2RadioConnection::composeCmdHighPriority falls back to its own
    // buildAlex0/buildAlex1 when no per-board codec is selected. That
    // path carried the identical defect and must be fixed with it.
    void legacyPath_txLpfIsAlsoSourcedFromTx()
    {
        P2RadioConnection conn;   // no setBoardForTest -> legacy builders

        conn.setTxFrequency(k10mHz);
        conn.setReceiverFrequency(kStreamB, k80mHz);

        QCOMPARE(composeAlexWords(conn).txLpf, codec::alex::computeLpf(28.4));
    }

    // ── The LPF is never bypassed ───────────────────────────────────────
    //
    // Design doc §4: the low-pass is TX-only and must never be switched
    // out. Thetis has no bypass encoding for it — the `else` arm of
    // setAlexLPF falls through to 0x10, the 6 m (widest) low-pass, rather
    // than to zero (console.cs:7237-7241 [v2.10.3.15]).
    void txLpfIsNeverZero()
    {
        P2RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);

        // Fresh connection, before any frequency has been set.
        QVERIFY(composeAlexWords(conn).txLpf != 0);

        conn.setReceiverFrequency(kStreamB, k80mHz);
        QVERIFY(composeAlexWords(conn).txLpf != 0);

        conn.setTxFrequency(k10mHz);
        QVERIFY(composeAlexWords(conn).txLpf != 0);

        conn.setMox(true);
        const AlexWords w = composeAlexWords(conn);
        QVERIFY(w.txLpf != 0);
        QVERIFY(w.rxLpf != 0);
    }
};

QTEST_MAIN(TestAlexTxLpfSource)
#include "tst_alex_tx_lpf_source.moc"
