// no-port-check: NereusSDR-original test. Cites Thetis only to anchor the
// wire byte the assertions read.
//
// Codex review, PR #293. Two defects in the widebandExtensionRequestedChanged
// handler, both about treating one slice's edge as the whole radio's state.
//
//   P1  Two slices sharing a chain both want extended view. The handler
//       forwarded the changing slice's boolean straight to
//       setWidebandActive(chain) and setWidebandEnabled(adc), so whichever
//       slice cleared last turned the chain off while the other still needed
//       it: the Alex preselector came back in and the P2 wideband-enable bit
//       dropped underneath a pan that was still zoomed out.
//
//   P2  Boards whose BoardCapabilities::widebandAdcs is 0 cannot deliver a
//       wideband stream at all. The P2 cast no-ops there, so no samples
//       arrive, but setWidebandActive still forced the preselector into
//       bypass. The operator lost receive filtering for a view the radio was
//       never going to provide. widebandAdcs was declared per SKU and read by
//       nothing.

#include <QtTest/QtTest>

#include "core/BoardCapabilities.h"
#include "core/P2RadioConnection.h"
#include "core/DdcAssignment.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// CmdGeneral byte 23 is the per-ADC wideband enable bitmask; bit N is ADCN.
// From Thetis ChannelMaster/network.c:879 [v2.10.3.15]:
//   packetbuf[23] = (char)_InterlockedAnd(&prn->wb_enable, 0xff);
constexpr int kCmdGeneralWbEnableByte = 23;

quint8 cmdGeneralWbMask(const P2RadioConnection& conn)
{
    quint8 buf[60] = {};
    conn.composeCmdGeneralForTest(buf);
    return buf[kCmdGeneralWbEnableByte];
}

/// Declare a live connection to a wideband-capable Protocol 2 radio.
///
/// Two facts, not one. widebandActiveForChain gates on the board row's
/// widebandAdcs AND on the protocol of the radio actually connected, and
/// those are genuinely different questions (Codex review round 6, PR #293):
/// ANVELINAPRO3 and REDPITAYA both resolve to the kOrionMKII row, which
/// declares Protocol2 with widebandAdcs = 2, and both have real Protocol 1
/// codecs in P1RadioConnection::selectCodec. So a test that declares only
/// the board is describing a connection that may or may not exist, and the
/// gate it thinks it is exercising may not be the one that answered.
void declareP2Radio(RadioModel& model, HPSDRModel board)
{
    model.setHpsdrModelForTest(board);
    RadioInfo info;
    info.protocol = ProtocolVersion::Protocol2;
    model.setLastRadioInfoForTest(info);
    model.configureStreamPool(4, 4, 192000);
}

} // namespace

class TestWidebandChainState : public QObject {
    Q_OBJECT
private slots:

    void physical_adc_capture_is_independent_of_its_filter_chain()
    {
        P2RadioConnection conn;
        RadioModel model;
        model.injectConnectionForTest(&conn);
        declareP2Radio(model, HPSDRModel::ANAN_G2);
        // An explicit synthetic topology, not a claim about this P2 SKU.
        model.setWidebandTopologyForTest(2, 2, 1);
        model.configureStreamPool(4, 4, 192000);
        SliceModel* slice = model.sliceById(model.addSlice());
        QVERIFY(slice);
        QVERIFY(slice->streamIndex() >= 0);

        DdcAssignment assignment{};
        assignment.streamDdc[slice->streamIndex()] = 2;
        assignment.rate[2] = 192000;
        assignment.ddcEnable = 0x04;
        assignment.adcCtrl1 = (1 << 4); // DDC2 on physical ADC1.
        model.publishDdcAssignmentForTest(assignment);
        QCOMPARE(model.sliceAdcIndex(slice->sliceIndex()), 1);
        QCOMPARE(slice->chainIndex(), 0);

        slice->setWidebandExtensionRequested(true);
        QCOMPARE(cmdGeneralWbMask(conn), quint8(0x02));
        QCOMPARE(model.alexController().adcState(0).effective,
                 AlexController::BpfEffective::WidebandLocked);
        QVERIFY(model.alexController().adcState(1).effective
                != AlexController::BpfEffective::WidebandLocked);
        slice->setWidebandExtensionRequested(false);
        QCOMPARE(cmdGeneralWbMask(conn), quint8(0));
        QVERIFY(model.alexController().adcState(0).effective
                != AlexController::BpfEffective::WidebandLocked);
    }

    // Two slices, one chain. The chain stays wideband until the LAST of them
    // stops asking.
    void one_slice_clearing_does_not_drop_a_chain_another_slice_still_needs()
    {
        P2RadioConnection conn;
        RadioModel model;
        model.injectConnectionForTest(&conn);
        // A wideband-capable board, or the capability gate below correctly
        // refuses the whole thing and this test would pass for the wrong
        // reason. The default profile advertises widebandAdcs == 0.
        declareP2Radio(model, HPSDRModel::ANAN_G2);

        const int a = model.addSlice();
        const int b = model.addSlice();
        SliceModel* sliceA = model.sliceById(a);
        SliceModel* sliceB = model.sliceById(b);
        QVERIFY(sliceA && sliceB);
        // Both land on chain 0 on a fresh model, which is the sharing case.
        QCOMPARE(sliceA->chainIndex(), 0);
        QCOMPARE(sliceB->chainIndex(), 0);

        sliceA->setWidebandExtensionRequested(true);
        sliceB->setWidebandExtensionRequested(true);
        QCOMPARE(cmdGeneralWbMask(conn) & 0x01, 0x01);

        // B zooms back in. A is still zoomed out, so the chain must hold.
        sliceB->setWidebandExtensionRequested(false);
        QVERIFY2((cmdGeneralWbMask(conn) & 0x01) == 0x01,
            "slice A still requires wideband, so clearing slice B must not "
            "drop the shared chain's wideband enable");

        // Only when the last requester clears does it drop.
        sliceA->setWidebandExtensionRequested(false);
        QCOMPARE(cmdGeneralWbMask(conn) & 0x01, 0x00);
    }

    // Order must not matter: clearing the slice that asked FIRST is the same
    // case, and a fix keyed on "the last slice to change" would pass the test
    // above and fail this one.
    void clearing_the_first_requester_also_holds_the_chain()
    {
        P2RadioConnection conn;
        RadioModel model;
        model.injectConnectionForTest(&conn);
        // A wideband-capable board, or the capability gate below correctly
        // refuses the whole thing and this test would pass for the wrong
        // reason. The default profile advertises widebandAdcs == 0.
        declareP2Radio(model, HPSDRModel::ANAN_G2);

        const int a = model.addSlice();
        const int b = model.addSlice();
        SliceModel* sliceA = model.sliceById(a);
        SliceModel* sliceB = model.sliceById(b);
        QVERIFY(sliceA && sliceB);

        sliceA->setWidebandExtensionRequested(true);
        sliceB->setWidebandExtensionRequested(true);

        sliceA->setWidebandExtensionRequested(false);
        QVERIFY2((cmdGeneralWbMask(conn) & 0x01) == 0x01,
            "slice B still requires wideband after slice A cleared");
    }

    // A board that advertises no wideband ADCs must neither enable the stream
    // nor bypass the preselector for it.
    void a_board_without_wideband_adcs_does_not_bypass_the_preselector()
    {
        P2RadioConnection conn;
        RadioModel model;
        model.injectConnectionForTest(&conn);
        // Hermes Lite 2: BoardCapabilities.cpp gives it widebandAdcs = 0,
        // "P1 board — wideband mechanism differs; deferred to 3F-W".
        model.setHpsdrModelForTest(HPSDRModel::HERMESLITE);
        QCOMPARE(model.boardCapabilities().widebandAdcs, 0);

        const int a = model.addSlice();
        SliceModel* slice = model.sliceById(a);
        QVERIFY(slice);

        slice->setWidebandExtensionRequested(true);

        QVERIFY2((cmdGeneralWbMask(conn) & 0xff) == 0x00,
            "a board with widebandAdcs == 0 cannot stream wideband, so no "
            "enable bit may be set");
        QVERIFY2(!model.widebandActiveForChainForTest(0),
            "and the Alex preselector must not be forced into bypass for a "
            "wideband view the radio cannot deliver");
    }

    // Codex review round 3, P2. The handler runs on request-property edges
    // only, so removing the slice that was the sole requester left the chain
    // bypassed and the radio still streaming wideband, until some other slice
    // happened to toggle the property. removeSlice has to recompute too.
    void removing_the_last_requester_clears_the_chain()
    {
        P2RadioConnection conn;
        RadioModel model;
        model.injectConnectionForTest(&conn);
        declareP2Radio(model, HPSDRModel::ANAN_G2);
        model.configureStreamPool(/*userDdcCount*/ 4, /*maxSlices*/ 4, 192000);

        const int a = model.addSlice();
        const int b = model.addSlice();
        SliceModel* sliceB = model.sliceById(b);
        QVERIFY(sliceB);
        Q_UNUSED(a);

        // Only B asks for it.
        sliceB->setWidebandExtensionRequested(true);
        QCOMPARE(cmdGeneralWbMask(conn) & 0x01, 0x01);

        model.removeSlice(b);

        QVERIFY2((cmdGeneralWbMask(conn) & 0x01) == 0x00,
            "the only slice requesting wideband is gone, so the radio must "
            "stop streaming it rather than wait for another slice to toggle");
        QVERIFY2(!model.widebandActiveForChainForTest(0),
            "and the preselector must come back in");
    }

    // The other half: a survivor still asking must keep the chain up when a
    // different slice is removed.
    void removing_a_slice_holds_a_chain_a_survivor_still_needs()
    {
        P2RadioConnection conn;
        RadioModel model;
        model.injectConnectionForTest(&conn);
        declareP2Radio(model, HPSDRModel::ANAN_G2);
        model.configureStreamPool(/*userDdcCount*/ 4, /*maxSlices*/ 4, 192000);

        const int a = model.addSlice();
        const int b = model.addSlice();
        SliceModel* sliceA = model.sliceById(a);
        SliceModel* sliceB = model.sliceById(b);
        QVERIFY(sliceA && sliceB);

        sliceA->setWidebandExtensionRequested(true);
        sliceB->setWidebandExtensionRequested(true);

        model.removeSlice(b);

        QVERIFY2((cmdGeneralWbMask(conn) & 0x01) == 0x01,
            "slice A is still zoomed out, so removing B must not drop the "
            "chain");
    }

    // Codex review round 4, P1. A slice can change chain without its request
    // property moving: on a dual-chain radio, picking EXT1 moves its DDC from
    // chain 0 to chain 1. Reconciling only the chains named by an edge left
    // the old chain bypassed and streaming while the new one stayed filtered
    // with no stream.
    //
    // The fix is structural rather than another hook. publishDdcAssignment
    // already recomputes DDC, chain and psPaused for every slice from the
    // assignment, so wideband is reconciled for EVERY chain in the same pass.
    // Migration is then covered by construction, not by remembering to add a
    // trigger for it.
    void a_slice_changing_chains_reconciles_both_of_them()
    {
        P2RadioConnection conn;
        RadioModel model;
        model.injectConnectionForTest(&conn);
        declareP2Radio(model, HPSDRModel::ANAN_G2);
        model.configureStreamPool(/*userDdcCount*/ 4, /*maxSlices*/ 4, 192000);

        const int a = model.addSlice();
        SliceModel* slice = model.sliceById(a);
        QVERIFY(slice);
        const int stream = slice->streamIndex();
        QVERIFY(stream >= 0);

        slice->setWidebandExtensionRequested(true);
        QCOMPARE(slice->chainIndex(), 0);
        QCOMPARE(cmdGeneralWbMask(conn) & 0x03, 0x01);

        // Move the slice's DDC onto ADC1, which is what selecting an RX-only
        // antenna does. Nothing touches widebandExtensionRequested.
        DdcAssignment moved{};
        moved.streamDdc[stream] = 2;
        moved.rate[2]           = 192000;
        moved.ddcEnable         = 0x04;
        // DDC2's ADC selector is bits 5:4 of adcCtrl1; 01 there is ADC1.
        moved.adcCtrl1          = (1 << 4);
        model.publishDdcAssignmentForTest(moved);

        QCOMPARE(slice->chainIndex(), 1);
        QVERIFY2(!model.widebandActiveForChainForTest(0),
            "the chain the slice left must stop being held wideband");
        QVERIFY2(model.widebandActiveForChainForTest(1),
            "the chain it moved to must pick the request up");
        QCOMPARE(cmdGeneralWbMask(conn) & 0x03, 0x02);
    }

    // Codex review round 5, P2. The round-2 gate tested widebandAdcs <= 0,
    // which looked like the unambiguous choice and is not sufficient.
    // ANAN-100D (Angelia) and ANAN-200D (Orion) are PROTOCOL 1 boards whose
    // capability rows advertise widebandAdcs = 2, so the gate passed, the
    // chain was marked wideband and the Alex filter bypassed, while the only
    // wire push is a P2RadioConnection cast that no-ops on P1. Receive
    // filtering was lost for a stream that could never arrive: exactly the
    // harm the gate was added to prevent.
    //
    // Codex review round 5, P2, restated by plan Task 5 (the operator's
    // ruling of 2026-09-24, "follow thetis"). ANAN-100D (Angelia) and
    // ANAN-200D (Orion) run Protocol 1 or Protocol 2 firmware, and one row
    // serves both, so the row now carries their Protocol 2 wideband (ADC0)
    // and the Protocol 1 answer comes from BoardCapsTable::widebandAdcsFor.
    //
    // The invariant is over every row, not only the Protocol1 ones: no board
    // offers wideband while running Protocol 1. Thetis's Protocol 1 receive
    // loop takes EP6 only (networkproto1.c:181-201 [v2.10.3.15]), and
    // NereusSDR has no P1 wideband receive path, so any board offering it
    // there buys a bypassed preselector and no stream.
    //
    // Driven through the model, not only through the table helper: a check
    // of widebandAdcsFor alone could not fail, because that helper returns 0
    // on Protocol 1 by construction. What matters is that the model refuses,
    // so every row is stood into a RadioModel running Protocol 1 and asked
    // for extended view, the way an_anan_100d_does_not_reach_extended_mode
    // does for one board. A P2RadioConnection is injected on purpose: its
    // cast is then no gate, and the only thing that can refuse is the
    // protocol rule the two model readers take from widebandAdcsFor.
    //
    // The same harness on Protocol 2 must ENGAGE for every row that offers
    // wideband there (and only for those), or the Protocol 1 refusals would
    // prove nothing.
    void no_board_offers_wideband_while_running_protocol1()
    {
        int rows = 0;
        int engagedOnP2 = 0;
        for (const BoardCapabilities& caps : BoardCapsTable::all()) {
            ++rows;
            QVERIFY2(BoardCapsTable::widebandAdcsFor(caps, ProtocolVersion::Protocol1) == 0,
                qPrintable(QStringLiteral("%1 offers %2 wideband ADCs on Protocol 1. "
                    "There is no P1 wideband receive path, so extended view "
                    "would bypass its preselector for a stream that never "
                    "arrives.")
                    .arg(caps.displayName)
                    .arg(BoardCapsTable::widebandAdcsFor(caps, ProtocolVersion::Protocol1))));

            for (ProtocolVersion protocol : {ProtocolVersion::Protocol1,
                                             ProtocolVersion::Protocol2}) {
                P2RadioConnection conn;
                RadioModel model;
                model.injectConnectionForTest(&conn);
                model.setHpsdrModelForTest(defaultModelForBoard(caps.board));
                model.setBoardRowForTest(caps);
                RadioInfo info;
                info.protocol = protocol;
                model.setLastRadioInfoForTest(info);
                model.configureStreamPool(/*userDdcCount*/ 4, /*maxSlices*/ 4, 192000);

                const int a = model.addSlice();
                SliceModel* slice = model.sliceById(a);
                QVERIFY(slice);
                slice->setWidebandExtensionRequested(true);

                const bool engaged = model.widebandActiveForChainForTest(0);
                if (protocol == ProtocolVersion::Protocol1) {
                    QVERIFY2(!engaged,
                        qPrintable(QStringLiteral("%1 running Protocol 1 reached "
                            "extended view: its preselector was bypassed for a "
                            "wideband stream Protocol 1 never delivers.")
                            .arg(caps.displayName)));
                    QVERIFY2((cmdGeneralWbMask(conn) & 0xff) == 0x00,
                        qPrintable(QStringLiteral("%1 running Protocol 1 set a "
                            "wideband enable bit").arg(caps.displayName)));
                } else {
                    const bool offers = BoardCapsTable::widebandAdcsFor(caps, protocol) > 0;
                    QVERIFY2(engaged == offers,
                        qPrintable(QStringLiteral("%1 running Protocol 2: offers "
                            "wideband %2, engaged %3").arg(caps.displayName)
                            .arg(offers).arg(engaged)));
                    if (engaged) { ++engagedOnP2; }
                }
                slice->setWidebandExtensionRequested(false);
                model.injectConnectionForTest(nullptr);
            }
        }
        QVERIFY2(rows > 0, "an empty table would pass this vacuously");
        QVERIFY2(engagedOnP2 > 0,
                 "no row engaged on Protocol 2 either, so the Protocol 1 "
                 "refusals above prove nothing about the protocol gate");
    }

    // The consequence at the model level, for the board that carried the bad
    // row, running Protocol 1.
    void an_anan_100d_does_not_reach_extended_mode()
    {
        P2RadioConnection conn;
        RadioModel model;
        model.injectConnectionForTest(&conn);
        model.setHpsdrModelForTest(HPSDRModel::ANAN100D);
        RadioInfo p1;
        p1.protocol = ProtocolVersion::Protocol1;
        model.setLastRadioInfoForTest(p1);
        model.configureStreamPool(/*userDdcCount*/ 4, /*maxSlices*/ 4, 192000);

        const int a = model.addSlice();
        SliceModel* slice = model.sliceById(a);
        QVERIFY(slice);

        slice->setWidebandExtensionRequested(true);

        QVERIFY2(!model.widebandActiveForChainForTest(0),
            "ANAN-100D running Protocol 1 has no wideband path, so the "
            "preselector must stay in");
        QVERIFY2((cmdGeneralWbMask(conn) & 0xff) == 0x00,
            "and no P2 wideband enable bit may be set for it");
    }

    // Plan Task 5: the same boards on Protocol 2 get wideband as Thetis
    // gives it, on ADC0 (console.cs:43552-43558 [v2.10.3.15],
    // NetworkIO.SetWBEnable(0, 1)), and only ADC0.
    void an_anan_100d_or_200d_on_protocol2_engages_adc0()
    {
        for (HPSDRModel board : {HPSDRModel::ANAN100D, HPSDRModel::ANAN200D}) {
            P2RadioConnection conn;
            RadioModel model;
            model.injectConnectionForTest(&conn);
            declareP2Radio(model, board);
            QCOMPARE(BoardCapsTable::widebandAdcsFor(model.boardCapabilities(),
                                                     ProtocolVersion::Protocol2), 1);

            const int a = model.addSlice();
            SliceModel* slice = model.sliceById(a);
            QVERIFY(slice);

            slice->setWidebandExtensionRequested(true);

            QVERIFY2(model.widebandActiveForChainForTest(0),
                "on Protocol 2 these boards offer wideband on ADC0");
            QCOMPARE(cmdGeneralWbMask(conn) & 0xff, 0x01);
        }
    }

    // Codex review round 6, PR #293. The table invariant above is necessary
    // and not sufficient.
    //
    // A row's .protocol is what the board usually speaks, not what THIS
    // connection is speaking. ANVELINAPRO3 and REDPITAYA both resolve to
    // HPSDRHW::OrionMKII (HpsdrModel.h:157 and :159), whose row declares
    // Protocol2 with widebandAdcs = 2, and both have real Protocol 1 codecs
    // in P1RadioConnection::selectCodec (P1CodecAnvelinaPro3,
    // P1CodecRedPitaya). So a live P1 connection can reach the wideband
    // decision holding a row that advertises wideband, and no amount of
    // auditing the table fixes that: the table is right, the connection is
    // the thing that differs.
    //
    // Isolates gate 2 deliberately. The board here has widebandAdcs = 2, so
    // gate 1 passes and only the protocol gate can refuse.
    void a_protocol1_connection_is_refused_by_a_wideband_capable_row()
    {
        P2RadioConnection conn;
        RadioModel model;
        model.injectConnectionForTest(&conn);
        model.setHpsdrModelForTest(HPSDRModel::ANVELINAPRO3);

        // Precondition: this is the awkward case, not a board that would be
        // refused by the capability gate anyway.
        QVERIFY2(model.boardCapabilities().widebandAdcs > 0,
            "AnvelinaPro3 must still resolve to the kOrionMKII row; if it "
            "stops, this case no longer isolates the protocol gate");

        RadioInfo p1;
        p1.protocol = ProtocolVersion::Protocol1;
        model.setLastRadioInfoForTest(p1);

        const int a = model.addSlice();
        SliceModel* slice = model.sliceById(a);
        QVERIFY(slice);

        slice->setWidebandExtensionRequested(true);

        QVERIFY2(!model.widebandActiveForChainForTest(0),
            "a Protocol 1 connection has no wideband receive path, whatever "
            "the board row advertises, so the preselector must stay in");
        QVERIFY2((cmdGeneralWbMask(conn) & 0xff) == 0x00,
            "and no P2 wideband enable bit may be set for it");
    }

    // The same board on the connection its row describes. Without this the
    // case above could pass by refusing AnvelinaPro3 outright, which would be
    // a different bug wearing the same green tick.
    void the_same_board_on_protocol2_still_engages()
    {
        P2RadioConnection conn;
        RadioModel model;
        model.injectConnectionForTest(&conn);
        declareP2Radio(model, HPSDRModel::ANVELINAPRO3);

        const int a = model.addSlice();
        SliceModel* slice = model.sliceById(a);
        QVERIFY(slice);

        slice->setWidebandExtensionRequested(true);

        QVERIFY2(model.widebandActiveForChainForTest(0),
            "the protocol gate must refuse P1 connections, not this board");
    }

    // The capable case, so the gate above is not simply switching the feature
    // off for everyone.
    void a_wideband_capable_board_still_engages()
    {
        P2RadioConnection conn;
        RadioModel model;
        model.injectConnectionForTest(&conn);
        declareP2Radio(model, HPSDRModel::ANAN_G2);
        QVERIFY(model.boardCapabilities().widebandAdcs > 0);

        const int a = model.addSlice();
        SliceModel* slice = model.sliceById(a);
        QVERIFY(slice);

        slice->setWidebandExtensionRequested(true);

        QCOMPARE(cmdGeneralWbMask(conn) & 0x01, 0x01);
        QVERIFY(model.widebandActiveForChainForTest(0));
    }
};

QTEST_MAIN(TestWidebandChainState)
#include "tst_wideband_chain_state.moc"
