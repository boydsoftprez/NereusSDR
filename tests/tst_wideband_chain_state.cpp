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

} // namespace

class TestWidebandChainState : public QObject {
    Q_OBJECT
private slots:

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
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);

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
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);

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
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
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
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
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
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
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
        QVERIFY2((cmdGeneralWbMask(conn) & 0x01) == 0x00,
            "and the old chain's wideband stream must stop");
    }

    // Codex review round 5, P2. The round-2 gate tested widebandAdcs <= 0,
    // which looked like the unambiguous choice and is not sufficient.
    // ANAN-100D (Angelia) and ANAN-200D (Orion) are PROTOCOL 1 boards whose
    // capability rows advertise widebandAdcs = 2, so the gate passed, the
    // chain was marked wideband and the Alex filter bypassed, while the only
    // wire push is a P2RadioConnection cast that no-ops on P1. Receive
    // filtering was lost for a stream that could never arrive: exactly the
    // harm the gate was added to prevent.
    // Codex review round 5, P2. The gate tested widebandAdcs <= 0, which was
    // right, and ANAN-100D (Angelia) and ANAN-200D (Orion) still reached
    // extended mode because THEIR CAPABILITY ROWS WERE WRONG: both declare
    // .protocol = Protocol1 and then advertised widebandAdcs = 2, contradicting
    // their own protocol field and every other Protocol1 row, all of which set
    // 0 with "wideband mechanism differs; deferred to 3F-W".
    //
    // Fixed in the table rather than by adding a second gate in front of it,
    // and pinned as an invariant over every board rather than as two per-SKU
    // assertions, so the next row added cannot reintroduce it. NereusSDR has
    // no Protocol 1 wideband receive path: the only wire push is a
    // P2RadioConnection cast, so any P1 board claiming wideband ADCs buys a
    // bypassed preselector and no stream.
    void no_protocol1_board_advertises_wideband_adcs()
    {
        // The whole table, so a SKU added later is covered by construction
        // rather than by remembering to extend a list here.
        int protocol1Rows = 0;
        for (const BoardCapabilities& caps : BoardCapsTable::all()) {
            if (caps.protocol != ProtocolVersion::Protocol1) { continue; }
            ++protocol1Rows;
            QVERIFY2(caps.widebandAdcs == 0,
                qPrintable(QStringLiteral("%1 declares Protocol1 but advertises "
                    "widebandAdcs=%2. There is no P1 wideband receive path, so "
                    "extended view would bypass its preselector for a stream "
                    "that never arrives.")
                    .arg(caps.displayName).arg(caps.widebandAdcs)));
        }
        QVERIFY2(protocol1Rows > 0,
            "the table should contain Protocol1 boards; if it does not, this "
            "invariant is passing vacuously");
    }

    // The consequence at the model level, for the board that carried the bad
    // row.
    void an_anan_100d_does_not_reach_extended_mode()
    {
        P2RadioConnection conn;
        RadioModel model;
        model.injectConnectionForTest(&conn);
        model.setHpsdrModelForTest(HPSDRModel::ANAN100D);
        model.configureStreamPool(/*userDdcCount*/ 4, /*maxSlices*/ 4, 192000);

        const int a = model.addSlice();
        SliceModel* slice = model.sliceById(a);
        QVERIFY(slice);

        slice->setWidebandExtensionRequested(true);

        QVERIFY2(!model.widebandActiveForChainForTest(0),
            "ANAN-100D is a Protocol 1 board with no wideband path, so the "
            "preselector must stay in");
    }

    // The capable case, so the gate above is not simply switching the feature
    // off for everyone.
    void a_wideband_capable_board_still_engages()
    {
        P2RadioConnection conn;
        RadioModel model;
        model.injectConnectionForTest(&conn);
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
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
