// =================================================================
// tests/tst_p2_ddc_mask_ownership.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Thetis file
//   names appear only inside source-cite comments documenting which
//   upstream line each assertion pins. No Thetis logic is ported here.
//
// Phase 3F Sub-Epic I closeout. The codec's DDC enable mask was
// overwritten the instant it was sent. RadioModel::publishDdcAssignment
// calls ReceiverManager::activateReceiver for each newly live stream,
// that runs rebuildHardwareMapping, that emits
// hardwareReceiverCountChanged(N), and RadioModel forwards it to
// P2RadioConnection::setActiveReceiverCount(N), which rewrote CmdRx
// byte 7 as a flat DDC0..N-1 range. On an ANAN-G2 the codec had just
// asked for DDC2 + DDC3 (0x0C) and the count rewrote it to DDC0 + DDC1
// (0x03), so the radio stopped streaming BOTH slices.
//
// Ownership, established from upstream: Thetis writes the P2 enable mask
// in exactly one place, NetworkIO.EnableRxs(DDCEnable) at
// console.cs:8537 [v2.10.3.15], and DDCEnable comes entirely from
// UpdateDDCs's per-board switch. Inside EnableRxs the receiver COUNT is
// DERIVED from the mask (ChannelMaster/netInterface.c:1229-1236
// [v2.10.3.15]), never the other way round. So the codec owns the mask.
//
// The sibling defect in the same publish path — applyDdcAssignment being
// called directly across the GUI / connection thread boundary — is
// covered by tests/tst_p2_ddc_assignment_marshalling.cpp.
// =================================================================

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QThread>

#include "core/DdcAssignment.h"
#include "core/P2RadioConnection.h"
#include "core/ReceiverManager.h"
#include "core/codec/P2CodecSaturn.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// ── Board / DDC constants under test ───────────────────────────────────────
//
// Saturn (ANAN-G2) is the SKU in the defect report: 2 ADCs, 7 DDCs, and
// DDC0 / DDC1 reserved for the PureSignal or diversity sync pair, so user
// slices start at DDC2. From Thetis console.cs:8244-8245 [v2.10.3.15]
// (DDCEnable = DDC2 for rx1) and console.cs:8301 [v2.10.3.15]
// (DDCEnable += DDC3 for rx2), with console.cs:8199 [v2.10.3.15] giving the
// bit values: int DDC0 = 1, DDC1 = 2, DDC2 = 4, DDC3 = 8;
constexpr quint8 kMaskDdc2      = 0x04;   // slice A alone
constexpr quint8 kMaskDdc2Ddc3  = 0x0C;   // slice A + slice B on another band
constexpr quint8 kMaskCount1    = 0x01;   // what a count of 1 would derive
constexpr quint8 kMaskCount2    = 0x03;   // what a count of 2 would derive

// CmdRx byte 7 is the enable bitmask.
// From Thetis ChannelMaster/network.c:1097-1103 [v2.10.3.15]:
//   packetbuf[7] = (prn->rx[6].enable << 6 | ... | prn->rx[0].enable) & 0xff;
constexpr int kCmdRxEnableByte = 7;
constexpr int kCmdRxSyncByte = 1363;

quint8 cmdRxEnableMask(const P2RadioConnection& conn)
{
    quint8 buf[1444] = {};
    conn.composeCmdRxForTest(buf);
    return buf[kCmdRxEnableByte];
}

int cmdRxRateKhz(const P2RadioConnection& conn, int ddc)
{
    quint8 buf[1444] = {};
    conn.composeCmdRxForTest(buf);
    const int base = 17 + (ddc * 6);
    return (static_cast<int>(buf[base + 1]) << 8)
           | static_cast<int>(buf[base + 2]);
}

quint8 cmdRxSyncMask(const P2RadioConnection& conn)
{
    quint8 buf[1444] = {};
    conn.composeCmdRxForTest(buf);
    return buf[kCmdRxSyncByte];
}

// A Saturn-class assignment for `liveStreams` streams, shaped the way
// P2CodecSaturn shapes one. Used by the connection-level tests so they
// exercise applyDdcAssignment without standing up a RadioModel.
DdcAssignment saturnAssignment(int liveStreams)
{
    static constexpr int kStreamToDdc[5] = {2, 3, 4, 5, 6};
    DdcAssignment a{};
    for (int i = 0; i < liveStreams; ++i) {
        const int ddc = kStreamToDdc[i];
        a.streamDdc[i] = ddc;
        a.ddcEnable |= (1 << ddc);
        a.rate[ddc] = 192000;
        ++a.nDdc;
    }
    return a;
}

// P2RadioConnection::setState is protected, and RadioModel gates the wire
// push on RadioConnection::isConnected(). Exposing it from a test-local
// subclass keeps the seam out of production entirely; qobject_cast in
// invokeCodecDdcAssignment still matches, because this IS a
// P2RadioConnection.
class TestableP2Connection : public P2RadioConnection {
    Q_OBJECT
public:
    using P2RadioConnection::P2RadioConnection;
    void markConnectedForTest() { setState(ConnectionState::Connected); }
};

// Detaches a stack-injected RadioConnection on scope exit however the scope
// is left. QCOMPARE / QVERIFY return from the enclosing slot on failure, so
// a trailing injectConnectionForTest(nullptr) would be skipped on exactly
// the run where it matters most. Same guard tst_stream_pool_binding uses.
struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

// Reproduces the production wire from RadioModel::wireConnectionSignals:
//
//   connect(m_receiverManager, &ReceiverManager::hardwareReceiverCountChanged,
//           this, [this](int count) {
//       if (m_connection) {
//           QMetaObject::invokeMethod(m_connection, [conn = m_connection, count]() {
//               conn->setActiveReceiverCount(count);
//           });
//       }
//   });
//
// injectConnectionForTest deliberately does no wiring, so without this the
// clobber cannot happen and the model-level tests would pass vacuously.
void wireCountPushLikeProduction(RadioModel& model, RadioConnection* conn)
{
    QObject::connect(model.receiverManager(),
                     &ReceiverManager::hardwareReceiverCountChanged,
                     &model, [conn](int count) {
        QMetaObject::invokeMethod(conn, [conn, count]() {
            conn->setActiveReceiverCount(count);
        });
    });
}

// connectToRadio creates one ReceiverManager receiver per stream after
// sizing the pool. Without them setDdcMapping has no receiver to route onto
// and activateReceiver never fires, which is the very signal under test.
void createReceiversForPool(RadioModel& model, int streams)
{
    for (int st = 0; st < streams; ++st) {
        model.receiverManager()->createReceiver();
    }
}

} // namespace

class TestP2DdcMaskOwnership : public QObject {
    Q_OBJECT

private slots:

    // ═══════════════════════════════════════════════════════════════════
    // Section 1 — ownership at the connection boundary.
    //
    // These assert the invariant directly rather than through a call
    // ordering, which is what makes it hold for every rebuild trigger
    // (activateReceiver, deactivateReceiver, destroyReceiver,
    // setDdcMapping, reset), not just the one in the defect report.
    // ═══════════════════════════════════════════════════════════════════

    // Defect 1, minimal form: the codec asks for DDC2 + DDC3, a receiver
    // count of 2 arrives immediately afterwards. 0x0C must survive.
    void codec_mask_survives_a_count_derived_push()
    {
        P2RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::Saturn);

        conn.applyDdcAssignment(saturnAssignment(/*liveStreams*/ 2));
        QCOMPARE(cmdRxEnableMask(conn), kMaskDdc2Ddc3);

        // rebuildHardwareMapping -> hardwareReceiverCountChanged(2) ->
        // setActiveReceiverCount(2). Before the fix this rewrote byte 7 to
        // 0x03 and the radio dropped both slices.
        conn.setActiveReceiverCount(2);

        QCOMPARE(cmdRxEnableMask(conn), kMaskDdc2Ddc3);
        QVERIFY2(cmdRxEnableMask(conn) != kMaskCount2,
                 "CmdRx byte 7 was rewritten as a flat DDC0..N-1 range; a "
                 "receiver count cannot name DDCs on a board whose user "
                 "DDCs do not start at 0");
    }

    // Single-slice is the primary regression risk, so it gets its own case:
    // one live stream on a Saturn is DDC2, and a count of 1 would derive
    // DDC0.
    void single_slice_codec_mask_survives_a_count_derived_push()
    {
        P2RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::Saturn);

        conn.applyDdcAssignment(saturnAssignment(/*liveStreams*/ 1));
        QCOMPARE(cmdRxEnableMask(conn), kMaskDdc2);

        conn.setActiveReceiverCount(1);

        QCOMPARE(cmdRxEnableMask(conn), kMaskDdc2);
        QVERIFY2(cmdRxEnableMask(conn) != kMaskCount1,
                 "single-slice DDC2 was rewritten to DDC0 by a receiver count");
    }

    // Repeated pushes must not wear the ownership down, and a push that
    // matches the current state (the steady-state VFO tick, where
    // applyDdcAssignment finds nothing changed) must still hold ownership.
    void ownership_holds_across_a_no_change_reassignment()
    {
        P2RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::Saturn);

        conn.applyDdcAssignment(saturnAssignment(2));
        // Same assignment again: every field already matches, so
        // applyDdcAssignment's `changed` stays false and it sends nothing.
        conn.applyDdcAssignment(saturnAssignment(2));

        conn.setActiveReceiverCount(2);
        conn.setActiveReceiverCount(1);
        conn.setActiveReceiverCount(5);

        QCOMPARE(cmdRxEnableMask(conn), kMaskDdc2Ddc3);
    }

    // The other half of the ownership rule: BEFORE any codec has computed a
    // mask there is no owner, so the count-derived seed is unchanged. This
    // is the state tests/data/p2_baseline_bytes.json is frozen against
    // (tst_p2_regression_freeze constructs a bare P2RadioConnection and
    // calls setActiveReceiverCount with no assignment in between), so this
    // case is what keeps the wire-lock baseline honest.
    void count_still_seeds_the_mask_before_the_codec_speaks()
    {
        P2RadioConnection conn(nullptr);
        conn.setBoardForTest(HPSDRHW::Saturn);

        conn.setActiveReceiverCount(1);
        QCOMPARE(cmdRxEnableMask(conn), kMaskCount1);

        conn.setActiveReceiverCount(2);
        QCOMPARE(cmdRxEnableMask(conn), kMaskCount2);
    }

    // ═══════════════════════════════════════════════════════════════════
    // Section 2 — the reported scenario, end to end through RadioModel.
    // ═══════════════════════════════════════════════════════════════════

    // Slice A on 20m, then slice B on 40m, on an ANAN-G2. The allocator
    // gives B its own stream, the codec enables DDC2 + DDC3, and
    // publishDdcAssignment's activateReceiver(1) drives the count push.
    void adding_a_second_slice_leaves_the_codec_mask_on_the_wire()
    {
        RadioModel model;
        TestableP2Connection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.markConnectedForTest();
        model.injectConnectionForTest(&conn);
        DetachConnection detach{&model};
        wireCountPushLikeProduction(model, &conn);

        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5,
                                  /*defaultRateHz*/ 192000);
        createReceiversForPool(model, 5);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        QCOMPARE(cmdRxEnableMask(conn), kMaskDdc2);

        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(7150000.0);

        // Two streams, so two DDCs. This is the exact byte the defect
        // report tracked: the codec sent 0x0C and the count rewrote it 0x03,
        // killing reception on BOTH slices.
        QVERIFY(model.sliceById(a)->streamIndex()
                != model.sliceById(b)->streamIndex());
        QCOMPARE(cmdRxEnableMask(conn), kMaskDdc2Ddc3);
    }

    // The count push has other producers than publishDdcAssignment, so
    // reordering the two calls would only have fixed the reported path.
    // Anything that rebuilds the hardware mapping must leave the mask
    // alone: here a receiver is activated with no slice on it at all,
    // which no assignment recompute follows.
    void an_unrelated_hardware_remap_leaves_the_mask_alone()
    {
        RadioModel model;
        TestableP2Connection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.markConnectedForTest();
        model.injectConnectionForTest(&conn);
        DetachConnection detach{&model};
        wireCountPushLikeProduction(model, &conn);

        model.configureStreamPool(5, 5, 192000);
        createReceiversForPool(model, 5);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        const int b = model.addSlice();
        model.sliceById(b)->setFrequency(7150000.0);
        QCOMPARE(cmdRxEnableMask(conn), kMaskDdc2Ddc3);

        // No slice binding involved, no codec recompute: just a receiver
        // flipping active, which is enough to reach rebuildHardwareMapping
        // and hardwareReceiverCountChanged(3).
        QSignalSpy countSpy(model.receiverManager(),
                            &ReceiverManager::hardwareReceiverCountChanged);
        model.receiverManager()->activateReceiver(4);
        QVERIFY2(countSpy.count() > 0,
                 "the rebuild never fired, so this test proves nothing");

        QCOMPARE(cmdRxEnableMask(conn), kMaskDdc2Ddc3);

        // ...and the reverse edge too.
        model.receiverManager()->deactivateReceiver(4);
        QCOMPARE(cmdRxEnableMask(conn), kMaskDdc2Ddc3);
    }

    // Do not regress single-slice operation. One slice on a Saturn is DDC2
    // and stays DDC2 through the whole publish, including its own
    // activateReceiver(0).
    //
    // This is a regression guard, not a defect reproduction: it holds both
    // before and after the fix, and that is the point. Single-slice survived
    // the old code because activateReceiver(0) fires exactly once and every
    // later assignment finds the receiver already active, so no count push
    // follows it. Adding a SECOND slice is a real activation edge, which is
    // why that case broke and this one did not.
    void a_single_slice_session_still_lands_on_its_board_ddc()
    {
        RadioModel model;
        TestableP2Connection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.markConnectedForTest();
        model.injectConnectionForTest(&conn);
        DetachConnection detach{&model};
        wireCountPushLikeProduction(model, &conn);

        model.configureStreamPool(5, 5, 192000);
        createReceiversForPool(model, 5);

        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(14200000.0);
        QCOMPARE(cmdRxEnableMask(conn), kMaskDdc2);

        // Retuning inside the band re-runs the whole path on every tick.
        model.sliceById(a)->setFrequency(14225000.0);
        model.sliceById(a)->setFrequency(14250000.0);
        QCOMPARE(cmdRxEnableMask(conn), kMaskDdc2);
    }

    // A radio-state edge must take the same complete-assignment path as a
    // slice/rate bind. The five user streams deliberately carry distinct
    // rates so a partial PS writer cannot collapse DDC4-6 while updating the
    // DDC0/DDC1 feedback pair.
    //
    // Mutation caught: changing refreshDdcAssignmentForRadioState back to a
    // client-only publish (or reconnecting the legacy PsDdcConfig P2 writer)
    // leaves the old mask/rates on the wire and/or produces no full request.
    void radio_state_edges_rewrite_one_complete_assignment()
    {
        RadioModel model;
        TestableP2Connection conn;
        conn.setBoardForTest(HPSDRHW::Saturn);
        conn.markConnectedForTest();
        model.injectConnectionForTest(&conn);
        DetachConnection detach{&model};

        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5,
                                  /*defaultRateHz*/ 192000);
        createReceiversForPool(model, 5);

        const std::array<double, 5> frequencies{
            14200000.0, 7150000.0, 3800000.0, 21200000.0, 28400000.0
        };
        for (double frequency : frequencies) {
            const int id = model.addSlice();
            model.sliceById(id)->setFrequency(frequency);
        }
        QCOMPARE(model.activeStreamCount(), 5);

        const std::array<int, 5> userRateHz{
            48000, 96000, 192000, 384000, 768000
        };
        for (int stream = 0; stream < 5; ++stream) {
            model.setStreamSampleRate(stream, userRateHz[stream]);
        }

        QCOMPARE(cmdRxEnableMask(conn), quint8{0x7c}); // DDC2..DDC6
        for (int stream = 0; stream < 5; ++stream) {
            QCOMPARE(cmdRxRateKhz(conn, stream + 2), userRateHz[stream] / 1000);
        }

        QSignalSpy assignmentSpy(&model, &RadioModel::ddcAssignmentRequested);

        // MOX alone keeps all five user DDCs. It still performs one complete
        // recompute because MOX is an input to every per-board codec.
        model.setDdcContextForTest(/*mox*/ true, /*puresignalRun*/ false,
                                   /*diversity*/ false);
        model.refreshDdcAssignmentForRadioState();
        QCOMPARE(assignmentSpy.count(), 1);
        QCOMPARE(cmdRxEnableMask(conn), quint8{0x7c});
        for (int stream = 0; stream < 5; ++stream) {
            QCOMPARE(cmdRxRateKhz(conn, stream + 2), userRateHz[stream] / 1000);
        }

        // Effective PS adds the DDC0/DDC1 pair without rewriting any user
        // DDC. DDC1 is synchronized to DDC0.
        assignmentSpy.clear();
        model.setDdcContextForTest(/*mox*/ true, /*puresignalRun*/ true,
                                   /*diversity*/ false);
        model.refreshDdcAssignmentForRadioState();
        QCOMPARE(assignmentSpy.count(), 1);
        QCOMPARE(cmdRxEnableMask(conn), quint8{0x7f});
        QCOMPARE(cmdRxSyncMask(conn), quint8{0x02});
        QCOMPARE(cmdRxRateKhz(conn, 0), 192);
        QCOMPARE(cmdRxRateKhz(conn, 1), 192);
        for (int stream = 0; stream < 5; ++stream) {
            QCOMPARE(cmdRxRateKhz(conn, stream + 2), userRateHz[stream] / 1000);
        }

        // Diversity migrates stream 0 to the DDC0/DDC1 pair. Streams 1..4
        // remain on DDC3..6 with their distinct rates.
        assignmentSpy.clear();
        model.setDdcContextForTest(/*mox*/ false, /*puresignalRun*/ false,
                                   /*diversity*/ true);
        model.refreshDdcAssignmentForRadioState();
        QCOMPARE(assignmentSpy.count(), 1);
        QCOMPARE(cmdRxEnableMask(conn), quint8{0x7b});
        QCOMPARE(cmdRxSyncMask(conn), quint8{0x02});
        QCOMPARE(cmdRxRateKhz(conn, 0), userRateHz[0] / 1000);
        QCOMPARE(cmdRxRateKhz(conn, 1), userRateHz[0] / 1000);
        for (int stream = 1; stream < 5; ++stream) {
            QCOMPARE(cmdRxRateKhz(conn, stream + 2), userRateHz[stream] / 1000);
        }
    }
};

QTEST_MAIN(TestP2DdcMaskOwnership)
#include "tst_p2_ddc_mask_ownership.moc"
