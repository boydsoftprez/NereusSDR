// no-port-check: test-only. Thetis file names appear only in source-cite
// comments that document which upstream line each assertion verifies.
// No Thetis logic is ported here; this file is NereusSDR-original.
//
// =================================================================
// The Protocol 1 receive filters follow a receiver that still exists.
// =================================================================
//
// Phase 3F section 16.3.2. Thetis takes the receive-side filter selections
// from RX1: the Alex high-pass, the receive half of the low-pass (bank 10
// C4 while unkeyed) and the OC outputs, which on the HL2 drive the N2ADR
// filter board:
//   From Thetis console.cs:15398-15403 [v2.10.3.15]
//     private void UpdateRX1DDSFreq()
//     {
//         if (initializing) return;
//         setAlex1HPF(_rx1_dds_freq);
//         UpdateAlexTXFilter();
//         UpdateAlexRXFilter();
//
// Thetis's RX1 cannot be closed; NereusSDR's slice A can. Routing by frame
// slot (plan Task 11) leaves slice B on slot 1 (slot 2 on the Orion class)
// after A is closed, with slot 0 still holding A's last frequency. Before
// this fix the connection read slot 0 for all three selections.
//
// The ruling: the live receiver in the lowest frame slot stands in for RX1
// until a receiver returns on a lower slot.
//
// What the model-level cases prove about each selection:
//   - The high-pass (bank 10 C3). RadioModel::republishAlexAdcSlices pushes
//     a per-chain decision from every bound slice on each slice bind,
//     removal and retune, and it wins over the connection's own value. So
//     the high-pass already followed slice B through the model; these cases
//     guard it. The connection's own value, used when the model has no
//     decision, is covered by the connection-level cases at the end.
//   - The receive low-pass (bank 10 C4) on a board whose RX2 has its own
//     front end (rx2PreampPresent, the Orion class): RX1 only. Stuck on A.
//   - The OC byte (bank 0 C2). Stuck on A on every board.
// Both of the last two select LOW-pass filters, so the harmful direction is
// A on a low band and B on a high one: B then listens through A's low-pass.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-09-30 - HL2 with B on 20 m and C on 80 m expects B's receive pins, not
//                the N2ADR bypass (0x00): JJ's ruling on HL2 Auto.
//                J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/OcMatrix.h"
#include "core/P1RadioConnection.h"
#include "core/ReceiverManager.h"
#include "core/codec/AlexFilterMap.h"
#include "core/BoardCapabilities.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

constexpr double k10mHz = 28400000.0;
constexpr double k20mHz = 14200000.0;
constexpr double k40mHz =  7100000.0;
constexpr double k80mHz =  3700000.0;

// setState is protected on RadioConnection; the model pushes only to a
// connection that reports Connected.
class ConnectedP1 final : public P1RadioConnection {
public:
    ConnectedP1() { setState(ConnectionState::Connected); }
};

struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

quint8 hpfBits(const P1RadioConnection& conn)
{
    return quint8(conn.captureBank10ForTest()[3]) & 0x7F;   // bit 7 is the T/R relay
}

quint8 lpfBits(const P1RadioConnection& conn)
{
    return quint8(conn.captureBank10ForTest()[4]);
}

// Bank 0 C2 = (ocByte << 1) & 0xFE.
quint8 ocByte(const P1RadioConnection& conn)
{
    quint8 bank0[5] = {};
    conn.composeCcForBankForTest(0, bank0);
    return quint8(bank0[2] >> 1);
}

// A radio session reduced to what the filters depend on: a model and an
// injected, Connected P1 connection for `board`, with the production
// ReceiverManager -> connection wiring, one receiver per stream, and an OC
// matrix that gives 20 m, 40 m and 80 m distinct receive pins.
struct Session {
    explicit Session(HPSDRHW board)
    {
        AppSettings::instance().clear();
        oc.setPin(Band::Band20m, 0, /*tx=*/false, true);
        oc.setPin(Band::Band40m, 1, /*tx=*/false, true);
        oc.setPin(Band::Band80m, 2, /*tx=*/false, true);

        model.setBoardForTest(board);
        conn.setBoardForTest(board);
        conn.setOcMatrix(&oc);
        model.injectConnectionForTest(&conn);
        detach.model = &model;
        model.wireReceiverManagerHardwarePushesForTest();

        const int streams = (board == HPSDRHW::HermesLite) ? 2
                          : (board == HPSDRHW::Atlas)      ? 3
                                                           : 4;
        model.configureStreamPool(streams, streams, 192000);
        for (int i = 0; i < streams; ++i) {
            model.receiverManager()->createReceiver();
        }
    }
    ~Session() { AppSettings::instance().clear(); }

    int add(double hz)
    {
        const int id = model.addSlice();
        model.sliceById(id)->setFrequency(hz);
        return id;
    }
    int slotOf(int id)
    {
        return model.receiverManager()
            ->receiverConfig(model.sliceById(id)->streamIndex()).hardwareRx;
    }

    OcMatrix         oc;
    RadioModel       model;
    ConnectedP1      conn;
    DetachConnection detach;
};

} // namespace

class TestP1RxFilterFollowsLiveReceiver : public QObject {
    Q_OBJECT

private slots:

    // ── The brief's direction: A on a high band closed, B on a low band ──
    //
    // The harmful filter in this direction is the high-pass: a 20 m
    // high-pass in front of a 40 m receiver. Green before the fix as well,
    // because the model's per-chain decision (republishAlexAdcSlices) wins
    // over the connection's slot-0 value; kept as the guard for it.
    void closingAHigh_hpfFollowsB_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<int>("slotB");
        QTest::newRow("Hermes, B on slot 1")      << int(HPSDRHW::Hermes)     << 1;
        QTest::newRow("Orion class, B on slot 2") << int(HPSDRHW::Angelia)    << 2;
        QTest::newRow("HL2, B on slot 1")         << int(HPSDRHW::HermesLite) << 1;
    }
    void closingAHigh_hpfFollowsB()
    {
        QFETCH(int, board);
        QFETCH(int, slotB);
        const HPSDRHW hw = HPSDRHW(board);
        Session s(hw);

        const int a = s.add(k20mHz);
        const int b = s.add(k40mHz);
        QCOMPARE(s.slotOf(a), 0);
        QCOMPARE(s.slotOf(b), slotB);

        s.model.removeSlice(a);
        QCOMPARE(s.slotOf(b), slotB);
        QCOMPARE(s.conn.rx1SlotForTest(), slotB);
        QCOMPARE(hpfBits(s.conn), codec::alex::computeRxPreselector(k40mHz / 1e6, hw));

        // ...and every retune of B moves it.
        s.model.sliceById(b)->setFrequency(k80mHz);
        QCOMPARE(hpfBits(s.conn), codec::alex::computeRxPreselector(k80mHz / 1e6, hw));
    }

    // ── The harmful low-pass direction: A on 40 m closed, B on 20 m ──────
    //
    // Red before the fix: the OC byte stayed on 40 m on every board, and on
    // the Orion class (RX2 has its own front end, so the low-pass reads RX1
    // only) the receive low-pass stayed on 40 m too.
    void closingALow_lowPassSelectionsFollowB_data()
    {
        closingAHigh_hpfFollowsB_data();
    }
    void closingALow_lowPassSelectionsFollowB()
    {
        QFETCH(int, board);
        QFETCH(int, slotB);
        const HPSDRHW hw = HPSDRHW(board);
        Session s(hw);

        const int a = s.add(k40mHz);
        const int b = s.add(k20mHz);
        QCOMPARE(s.slotOf(b), slotB);

        s.model.removeSlice(a);
        QCOMPARE(s.slotOf(b), slotB);

        QCOMPARE(ocByte(s.conn), s.oc.maskFor(Band::Band20m, /*tx=*/false));
        QCOMPARE(lpfBits(s.conn), codec::alex::computeLpf(k20mHz / 1e6));
        QCOMPARE(hpfBits(s.conn), codec::alex::computeRxPreselector(k20mHz / 1e6, hw));

        // B's retune keeps driving all three.
        s.model.sliceById(b)->setFrequency(k40mHz);
        QCOMPARE(ocByte(s.conn), s.oc.maskFor(Band::Band40m, /*tx=*/false));
        QCOMPARE(lpfBits(s.conn), codec::alex::computeLpf(k40mHz / 1e6));
    }

    // ── A slice back below B hands the filters back ──────────────────────
    void addingASliceBelowBHandsTheFiltersBack_data()
    {
        closingAHigh_hpfFollowsB_data();
    }
    void addingASliceBelowBHandsTheFiltersBack()
    {
        QFETCH(int, board);
        QFETCH(int, slotB);
        const HPSDRHW hw = HPSDRHW(board);
        Session s(hw);

        const int a = s.add(k40mHz);
        const int b = s.add(k20mHz);
        s.model.removeSlice(a);
        QCOMPARE(s.conn.rx1SlotForTest(), slotB);
        QCOMPARE(ocByte(s.conn), s.oc.maskFor(Band::Band20m, /*tx=*/false));

        // The new slice takes the free stream 0, which is frame slot 0.
        const int c = s.add(k80mHz);
        QCOMPARE(s.slotOf(c), 0);
        QCOMPARE(s.conn.rx1SlotForTest(), 0);
        // On the HL2 in Auto the receive pins follow the highest-frequency
        // slice on the input (JJ's ruling of 2026-09-30), so B's 20 m stays
        // on the wire; elsewhere the OC byte follows the RX1 stand-in, C.
        const quint8 bothLive = (hw == HPSDRHW::HermesLite)
            ? s.oc.maskFor(Band::Band20m, /*tx=*/false)
            : s.oc.maskFor(Band::Band80m, /*tx=*/false);
        QCOMPARE(ocByte(s.conn), bothLive);

        // Close it again and B takes the role back.
        s.model.removeSlice(c);
        QCOMPARE(s.conn.rx1SlotForTest(), slotB);
        QCOMPARE(ocByte(s.conn), s.oc.maskFor(Band::Band20m, /*tx=*/false));
    }

    // ── The connection's own high-pass, with no model decision ───────────
    //
    // Before the fix only a frequency on slot 0 recomputed it, so with slot
    // 0 no longer live it kept slot 0's last value whatever the live
    // receiver did.
    void connectionHpf_followsTheLowestLiveSlot_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<int>("slotB");
        QTest::newRow("Hermes")      << int(HPSDRHW::Hermes)     << 1;
        QTest::newRow("Orion class") << int(HPSDRHW::Angelia)    << 2;
        QTest::newRow("HL2")         << int(HPSDRHW::HermesLite) << 1;
    }
    void connectionHpf_followsTheLowestLiveSlot()
    {
        QFETCH(int, board);
        QFETCH(int, slotB);
        const HPSDRHW hw = HPSDRHW(board);
        P1RadioConnection conn;
        conn.setBoardForTest(hw);

        conn.setLiveReceiverSlots((1u << 0) | (1u << slotB));
        conn.setReceiverFrequency(0, quint64(k20mHz));
        conn.setReceiverFrequency(slotB, quint64(k40mHz));
        QCOMPARE(hpfBits(conn), codec::alex::computeRxPreselector(k20mHz / 1e6, hw));

        // Slice A closed: the stand-in moves to B's slot at once.
        conn.setLiveReceiverSlots(1u << slotB);
        QCOMPARE(conn.rx1SlotForTest(), slotB);
        QCOMPARE(hpfBits(conn), codec::alex::computeRxPreselector(k40mHz / 1e6, hw));

        // B's retune moves it; a retune of the dead slot 0 does not.
        conn.setReceiverFrequency(slotB, quint64(k80mHz));
        QCOMPARE(hpfBits(conn), codec::alex::computeRxPreselector(k80mHz / 1e6, hw));
        conn.setReceiverFrequency(0, quint64(k20mHz));
        QCOMPARE(hpfBits(conn), codec::alex::computeRxPreselector(k80mHz / 1e6, hw));

        // Every stream suspended (PureSignal transmitting on HermesII):
        // the stand-in does not move.
        conn.setLiveReceiverSlots(0);
        QCOMPARE(conn.rx1SlotForTest(), slotB);
        QCOMPARE(hpfBits(conn), codec::alex::computeRxPreselector(k80mHz / 1e6, hw));

        // Slot 0 live again: it is RX1 again.
        conn.setLiveReceiverSlots((1u << 0) | (1u << slotB));
        QCOMPARE(conn.rx1SlotForTest(), 0);
        QCOMPARE(hpfBits(conn), codec::alex::computeRxPreselector(k20mHz / 1e6, hw));
    }

    // ── Every live slot is announced (Phase 3F section 16.3.2) ───────────
    //
    // The announced receiver count sizes the EP6 frame. It was the max of
    // the codec's count and the NUMBER of active receivers, which stops
    // being enough once routing is by frame slot: with slice A closed,
    // slices B and C sit on slots 1 and 2 and the count says 2, so slot 2
    // is not in the frame and slice C is silent. The Atlas (HPSDR) is the
    // board this reaches, because Thetis gives it no P1_rxcount:
    //   From Thetis console.cs:8533-8534 [v2.10.3.15]
    //     case HPSDRModel.HPSDR:
    //         break;
    // so nothing else raises its count above the connect-time seed of 2.
    void atlasWithAClosed_sliceCIsAnnounced()
    {
        Session s(HPSDRHW::Atlas);

        const int a = s.add(k20mHz);
        const int b = s.add(k40mHz);
        const int c = s.add(k80mHz);
        QCOMPARE(s.slotOf(a), 0);
        QCOMPARE(s.slotOf(b), 1);
        QCOMPARE(s.slotOf(c), 2);
        QVERIFY(s.conn.activeRxCountForTest() >= 3);

        s.model.removeSlice(a);
        QCOMPARE(s.slotOf(b), 1);
        QCOMPARE(s.slotOf(c), 2);
        QVERIFY2(s.conn.activeRxCountForTest() >= 3,
                 qPrintable(QStringLiteral("announced %1 receivers; slot 2 is live")
                                .arg(s.conn.activeRxCountForTest())));

        // Closing C as well lets the count come back down to B's slot.
        s.model.removeSlice(c);
        QCOMPARE(s.conn.activeRxCountForTest(), 2);
    }

    // Until a slot set arrives, and whenever slot 0 is live, slot 0 is RX1:
    // byte-for-byte the pre-fix behaviour.
    void noSlotSet_slotZeroIsRx1()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::Hermes);
        conn.setReceiverFrequency(1, quint64(k40mHz));
        conn.setReceiverFrequency(0, quint64(k20mHz));
        QCOMPARE(conn.rx1SlotForTest(), 0);
        QCOMPARE(hpfBits(conn),
                 codec::alex::computeRxPreselector(k20mHz / 1e6, HPSDRHW::Hermes));
    }
    // ── The receive low-pass passes the highest live receiver (M6) ───────
    //
    // Thetis's rule for a front end two receivers share takes the higher of
    // RX1 and RX2 (console.cs:15487-15499 [v2.10.3.15]); with more live
    // slices on one front end the low-pass has to pass the highest of them,
    // or a slice above it is filtered out. Where RX2 has a front end of its
    // own (rx2PreampPresent) RX1 decides alone, as before.
    void receiveLowPass_passesTheHighestLiveReceiver_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<bool>("shared");
        QTest::newRow("Hermes (one front end)") << int(HPSDRHW::Hermes)     << true;
        QTest::newRow("HL2 (one front end)")    << int(HPSDRHW::HermesLite) << true;
        QTest::newRow("Angelia (RX2 its own)")  << int(HPSDRHW::Angelia)    << false;
    }
    void receiveLowPass_passesTheHighestLiveReceiver()
    {
        QFETCH(int, board);
        QFETCH(bool, shared);
        const HPSDRHW hw = HPSDRHW(board);
        P1RadioConnection conn;
        conn.setBoardForTest(hw);
        QCOMPARE(!BoardCapsTable::forBoard(hw).rx2PreampPresent, shared);

        conn.setLiveReceiverSlots((1u << 0) | (1u << 1));
        conn.setReceiverFrequency(0, quint64(k80mHz));
        conn.setReceiverFrequency(1, quint64(k40mHz));
        QCOMPARE(lpfBits(conn),
                 codec::alex::computeLpf((shared ? k40mHz : k80mHz) / 1e6));

        // A third slice on a higher band.
        conn.setLiveReceiverSlots((1u << 0) | (1u << 1) | (1u << 2));
        conn.setReceiverFrequency(2, quint64(k10mHz));
        QCOMPARE(lpfBits(conn),
                 codec::alex::computeLpf((shared ? k10mHz : k80mHz) / 1e6));

        // It closes: the next highest again.
        conn.setLiveReceiverSlots((1u << 0) | (1u << 1));
        QCOMPARE(lpfBits(conn),
                 codec::alex::computeLpf((shared ? k40mHz : k80mHz) / 1e6));

        // The second closes too: RX1 alone, whatever slot 1 last held.
        conn.setLiveReceiverSlots(1u << 0);
        QCOMPARE(lpfBits(conn), codec::alex::computeLpf(k80mHz / 1e6));
    }
};

QTEST_MAIN(TestP1RxFilterFollowsLiveReceiver)
#include "tst_p1_rx_filter_follows_live_receiver.moc"
