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

#include "core/P2RadioConnection.h"
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
