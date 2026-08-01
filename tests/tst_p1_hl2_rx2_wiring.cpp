// no-port-check: NereusSDR-original wiring test, no ported logic.
#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/HpsdrModel.h"
#include "core/P1RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/ReceiverManager.h"
#include "core/codec/CodecContext.h"
#include "core/codec/P1CodecHl2.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "fakes/P1FakeRadio.h"

using namespace NereusSDR;
using NereusSDR::Test::P1FakeRadio;

namespace {
// ReceiverManager has no getter for the config it last computed, so read it
// off the signal it already emits.
PsDdcConfig lastConfig(QSignalSpy& spy)
{
    Q_ASSERT(spy.count() > 0);
    return spy.last().at(0).value<PsDdcConfig>();
}
} // namespace

class TestP1Hl2Rx2Wiring : public QObject {
    Q_OBJECT
private:
    RadioInfo makeInfo(P1FakeRadio& fake) const {
        RadioInfo info;
        info.address         = fake.localAddress();
        info.port            = fake.localPort();
        info.boardType       = HPSDRHW::HermesLite;
        info.protocol        = ProtocolVersion::Protocol1;
        info.macAddress      = QStringLiteral("aa:bb:cc:11:22:33");
        info.firmwareVersion = 72;
        info.name            = QStringLiteral("FakeHL2");
        return info;
    }

private slots:
    // ReceiverManager::setRx2Enabled had no caller anywhere in src/ or
    // tests/, so m_rx2Enabled was permanently false and the rx2 arms of
    // applyPureSignalDdcConfig (P1CodecHl2.cpp:569-572 and :593-596, both
    // correctly ported all along) could never fire.
    void rx2_enabled_reaches_the_codec()
    {
        P1CodecHl2 codec;          // setP1Codec takes a raw pointer
        ReceiverManager rm;
        rm.setP1Codec(&codec);
        rm.setHpsdrModel(HPSDRModel::HERMESLITE);
        rm.setRx1Rate(192000);

        QSignalSpy spy(&rm, &ReceiverManager::ddcConfigChanged);

        rm.setRx2Rate(96000);
        rm.setRx2Enabled(true);

        QVERIFY(spy.count() > 0);
        const PsDdcConfig cfg = lastConfig(spy);
        QCOMPARE(int(cfg.ddcEnable), 1 + 2);      // DDC0 + DDC1
        QCOMPARE(int(cfg.rate[1]), 96000);
    }

    void rx2_disabled_drops_ddc1()
    {
        P1CodecHl2 codec;
        ReceiverManager rm;
        rm.setP1Codec(&codec);
        rm.setHpsdrModel(HPSDRModel::HERMESLITE);
        rm.setRx1Rate(192000);
        rm.setRx2Rate(96000);
        rm.setRx2Enabled(true);

        QSignalSpy spy(&rm, &ReceiverManager::ddcConfigChanged);
        rm.setRx2Enabled(false);

        QVERIFY(spy.count() > 0);
        const PsDdcConfig cfg = lastConfig(spy);
        QCOMPARE(int(cfg.ddcEnable), 1);          // DDC0 only
        QCOMPARE(int(cfg.rate[1]), 0);
    }

    // The production path. A second live stream must reach ReceiverManager
    // without anyone calling setRx2Enabled by hand, which is the wiring that
    // was missing. Driven through slice frequency changes because
    // requestDdcAssignment hangs off SliceModel::frequencyChanged.
    void second_live_stream_enables_rx2_end_to_end()
    {
        P1CodecHl2 codec;
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        model.receiverManager()->setP1Codec(&codec);
        model.receiverManager()->setHpsdrModel(HPSDRModel::HERMESLITE);
        model.configureStreamPool(/*userDdcCount=*/2, /*maxSlices=*/5, 192000);

        const int idA = model.addSlice();
        SliceModel* a = model.sliceById(idA);
        QVERIFY(a != nullptr);
        a->setFrequency(14200000);

        // Only stream 0 so far.
        QVERIFY(!model.receiverManager()->rx2Enabled());

        // 7.1 MHz is far outside slice A's 192 kHz window, so the allocator
        // must claim stream 1 rather than co-host on stream 0.
        const int idB = model.addSlice();
        SliceModel* b = model.sliceById(idB);
        QVERIFY(b != nullptr);
        b->setFrequency(7100000);

        QVERIFY(model.buildStreamConfigsForCodecForTest()[1].live);
        QVERIFY(model.receiverManager()->rx2Enabled());
    }

    // Changing the announced receiver count changes the ep6 slot layout on
    // both sides at once (parseEp6Frame's slotBytes = 6 * numRx + 2). A bare
    // assignment leaves frames in flight that were composed under the old
    // layout, and parseEp6Frame cannot detect the mismatch: its 7F 7F 7F sync
    // check is layout-independent, so those frames are misparsed into
    // corrupted audio rather than rejected. restartStreamWithCount does the
    // stop, prime, start, prime cycle that makes both sides agree.
    void ps_ddc_config_count_change_restarts_the_stream()
    {
        P1FakeRadio fake;
        fake.setAutoStreamEnabled(false);   // no ep6 noise; we only count stops
        fake.start();

        P1RadioConnection conn;
        conn.init();
        // setAutoStreamEnabled(false) below means no ep6 frame arrives after
        // the one we send by hand to complete the handshake. Without this,
        // P1RadioConnection's own silence watchdog (kWatchdogSilenceMs =
        // 2000, P1RadioConnection.h:471) fires ~2 s in, tears the link down
        // to LinkLost, and reconnects on its own -- which also calls
        // sendMetisStop() and would satisfy the assertion below for a
        // reason unrelated to the fix under test. Push both timings well
        // past this test's lifetime so that confound cannot fire.
        conn.setReconnectTimingForTest(30000, 30000);
        conn.connectToRadio(makeInfo(fake));
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);
        // setAutoStreamEnabled(false) means the fake never sends an ep6
        // frame on its own, so the Connecting -> Connected handshake (which
        // fires on the first 1032-byte datagram, P1RadioConnection.cpp
        // onReadyRead) needs exactly one sent by hand. buildEp6Frame's
        // default numRx=1 is fine here: only the state transition and the
        // stop count are asserted below, not sample content.
        fake.sendEp6Frames(1);
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);

        const int stopsBefore = fake.metisStopCount();

        PsDdcConfig cfg{};
        cfg.p1RxCount = 4;      // connect seeds 2 (P1RadioConnection.cpp:695)
        cfg.nDdc      = 4;
        conn.applyPsDdcConfig(cfg);

        QTRY_VERIFY_WITH_TIMEOUT(fake.metisStopCount() > stopsBefore, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);
    }

    // The same count must not restart. restartStreamWithCount is idempotent
    // and a spurious stop/start costs the operator an audio dropout.
    void ps_ddc_config_same_count_does_not_restart()
    {
        P1FakeRadio fake;
        fake.setAutoStreamEnabled(false);
        fake.start();

        P1RadioConnection conn;
        conn.init();
        // See ps_ddc_config_count_change_restarts_the_stream: without this,
        // the silence watchdog reconnects on its own after ~2 s and would
        // add a spurious stop this test must not observe.
        conn.setReconnectTimingForTest(30000, 30000);
        conn.connectToRadio(makeInfo(fake));
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);
        // See ps_ddc_config_count_change_restarts_the_stream: one manual
        // ep6 frame is required to reach Connected when auto-stream is off.
        fake.sendEp6Frames(1);
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);

        PsDdcConfig first{};
        first.p1RxCount = 4;
        first.nDdc      = 4;
        conn.applyPsDdcConfig(first);
        // fake.isRunning() was already true before this call (from the
        // connect handshake above), so waiting on it alone would return
        // immediately on its stale pre-restart value without ever pumping
        // the event loop far enough to deliver this restart's stop
        // datagram -- exactly the race that made stopsBefore below read 0
        // instead of 1 the first time this test was written. Wait for the
        // stop count itself to move first, then for the matching start.
        QTRY_VERIFY_WITH_TIMEOUT(fake.metisStopCount() >= 1, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);

        const int stopsBefore = fake.metisStopCount();
        conn.applyPsDdcConfig(first);     // identical config, second time

        QTest::qWait(200);
        QCOMPARE(fake.metisStopCount(), stopsBefore);
    }

    // ── Two axes, one announced count ────────────────────────────────────
    //
    // Bench-caught 2026-08-01 (J.J. Boyd, KG4VCF) on a live HL2 with
    // PureSignal on. Removing the second panadapter dropped the announced
    // receiver count to 1 while PS still needed 4, so DDC2 and DDC3 left the
    // ep6 frame entirely for seven seconds. The log line that showed it:
    //   14:07:33  HL2 TX edge: mox=1 ... activeRx=1     (PS needs 4)
    //   14:07:33  applyPsDdcConfig ... activeRx=4       (repaired at key-down)
    //
    // Two independent things want a say in the count and neither knows about
    // the other: the DDC configuration (PureSignal, diversity) and the
    // panadapters. Last writer won, and the panadapter writer did not even
    // restart the stream. The announced count is now max(codec, pan), so
    // neither axis can starve the other.
    //
    // Latent before this branch: userDdcCount was 1 on the HL2, so the pan
    // count never moved and had nothing to race with.
    void a_pan_removal_cannot_lower_the_count_puresignal_needs()
    {
        P1FakeRadio fake;
        fake.setAutoStreamEnabled(false);
        fake.start();

        P1RadioConnection conn;
        conn.init();
        // See ps_ddc_config_count_change_restarts_the_stream for why the
        // silence watchdog has to be pushed past this test's lifetime.
        conn.setReconnectTimingForTest(30000, 30000);
        conn.connectToRadio(makeInfo(fake));
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);
        fake.sendEp6Frames(1);
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);

        PsDdcConfig ps{};
        ps.p1RxCount = 4;       // PureSignal on: DDC0/1 sync pair + 2 + 3
        ps.nDdc      = 4;
        conn.applyPsDdcConfig(ps);
        QTRY_COMPARE_WITH_TIMEOUT(conn.activeRxCountForTest(), 4, 3000);

        // The operator removes the second panadapter. One receiver is all
        // the panadapters want; it is not all PureSignal needs.
        conn.setActiveReceiverCount(1);

        QTest::qWait(200);
        QCOMPARE(conn.activeRxCountForTest(), 4);
    }

    // The other direction. More panadapters than the DDC configuration asks
    // for must still be announced, and must restart the stream: the count is
    // the ep6 slot layout, and a bare assignment leaves the two sides parsing
    // different geometries with no way to notice (the 7F 7F 7F sync check is
    // layout-independent).
    void a_pan_count_above_the_codec_floor_is_announced_and_restarts()
    {
        P1FakeRadio fake;
        fake.setAutoStreamEnabled(false);
        fake.start();

        P1RadioConnection conn;
        conn.init();
        conn.setReconnectTimingForTest(30000, 30000);
        conn.connectToRadio(makeInfo(fake));
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);
        fake.sendEp6Frames(1);
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);

        // Connect seeds the codec axis at 2 (P1RadioConnection.cpp:695).
        QCOMPARE(conn.activeRxCountForTest(), 2);
        const int stopsBefore = fake.metisStopCount();

        conn.setActiveReceiverCount(3);

        QTRY_COMPARE_WITH_TIMEOUT(conn.activeRxCountForTest(), 3, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(fake.metisStopCount() > stopsBefore, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);
    }

    // Turning PureSignal off releases its floor rather than pinning the count
    // at 4 forever. The panadapter axis is what remains, and on a board whose
    // connect default is 2 that is what should be announced.
    void dropping_the_puresignal_floor_returns_the_count_to_the_pan_axis()
    {
        P1FakeRadio fake;
        fake.setAutoStreamEnabled(false);
        fake.start();

        P1RadioConnection conn;
        conn.init();
        conn.setReconnectTimingForTest(30000, 30000);
        conn.connectToRadio(makeInfo(fake));
        QTRY_VERIFY_WITH_TIMEOUT(fake.isRunning(), 3000);
        fake.sendEp6Frames(1);
        QTRY_COMPARE_WITH_TIMEOUT(conn.state(), ConnectionState::Connected, 3000);

        PsDdcConfig on{};
        on.p1RxCount = 4;
        on.nDdc      = 4;
        conn.applyPsDdcConfig(on);
        QTRY_COMPARE_WITH_TIMEOUT(conn.activeRxCountForTest(), 4, 3000);

        // PureSignal off. The codec drops to the two-DDC announcement
        // (P1CodecHl2::applyPureSignalDdcConfig, the approved deviation).
        PsDdcConfig off{};
        off.p1RxCount = 2;
        off.nDdc      = 4;
        conn.applyPsDdcConfig(off);

        QTRY_COMPARE_WITH_TIMEOUT(conn.activeRxCountForTest(), 2, 3000);
    }
};

QTEST_MAIN(TestP1Hl2Rx2Wiring)
#include "tst_p1_hl2_rx2_wiring.moc"
