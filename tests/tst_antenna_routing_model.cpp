// no-port-check: test fixture asserts RadioModel antenna pump (Phase 3P-I-a T14)
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/Band.h"
#include "core/RadioConnection.h"
#include "core/accessories/AlexController.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

// Minimal RadioConnection that captures setAntennaRouting calls.
// Uses setState(Connected) in the constructor so the base-class
// isConnected() (non-virtual, reads m_state) returns true, which
// allows applyAlexAntennaForBand to pass its guard check.
class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    QList<AntennaRouting> calls;

    explicit MockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        // Mark as connected so RadioModel::applyAlexAntennaForBand()
        // passes the !isConnected() early-return guard.
        setState(ConnectionState::Connected);
    }

    // --- Pure virtuals from RadioConnection ---
    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setMox(bool) override {}
    void setAntennaRouting(AntennaRouting r) override {
        calls.append(r);
    }
    void setWatchdogEnabled(bool enabled) override { m_watchdogEnabled = enabled; }
    void sendTxIq(const float*, int) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

class TestAntennaRoutingModel : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        AppSettings::instance().clear();
    }

    void cleanup() {
        AppSettings::instance().clear();
    }

    // T9/T12: writing AlexController for the current band reaches the wire.
    // User clicks RX Ant 2 while tuned to Band20m → setAntennaRouting called
    // with trxAnt == 2.
    void rxAntChange_for_current_band_reaches_wire() {
        RadioModel model;
        // Set hasAlex=true so applyAlexAntennaForBand takes the antenna-read
        // branch rather than the hasAlex=false zero-routing branch.
        model.setCapsForTest(/*hasAlex=*/true);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);

        // m_lastBand defaults to Band20m; set RxAnt on that same band.
        // The antennaChanged signal fires → lambda sees b == m_lastBand →
        // applyAlexAntennaForBand(Band20m) → mock->setAntennaRouting called.
        model.alexControllerMutable().setRxAnt(model.lastBand(), 2);

        QVERIFY(mock->calls.size() >= 1);
        QCOMPARE(mock->calls.last().trxAnt, 2);

        // Null out m_connection before deleting mock so RadioModel's
        // destructor does not dereference a deleted pointer.
        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // T10: band boundary crossing reapplies the new band's stored antenna.
    // User stored RxAnt=3 for 40m, then tunes from 20m → 40m.
    void band_crossing_reapplies_new_band_antenna() {
        RadioModel model;
        // Set hasAlex=true so the antenna value (not zero) is sent to the wire.
        model.setCapsForTest(/*hasAlex=*/true);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);

        // Store antenna 3 for 40m. Because m_lastBand == Band20m,
        // the antennaChanged(Band40m) is filtered out (b != m_lastBand),
        // so no call is recorded yet.
        model.alexControllerMutable().setRxAnt(Band::Band40m, 3);
        mock->calls.clear();

        // Simulate band crossing: sets m_lastBand = Band40m and calls
        // applyAlexAntennaForBand(Band40m) because the band changed.
        model.setLastBandForTest(Band::Band40m);

        QVERIFY(mock->calls.size() >= 1);
        QCOMPARE(mock->calls.last().trxAnt, 3);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // T11: connect applies persisted state.
    // Antenna stored for Band20m before connection; onConnectedForTest()
    // mirrors the Connected state handler that calls applyAlexAntennaForBand.
    void connect_applies_persisted_antenna() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);
        // Store the antenna before connection (no mock yet, no wire call).
        model.alexControllerMutable().setRxAnt(Band::Band20m, 2);

        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        mock->calls.clear();

        // Simulate the Connected state handler. setLastBandForTest with the
        // same band (Band20m == default m_lastBand) is a no-op for the
        // crossing path; onConnectedForTest() applies unconditionally.
        model.onConnectedForTest();

        QVERIFY(mock->calls.size() >= 1);
        QCOMPARE(mock->calls.last().trxAnt, 2);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // T10 follow-up: band crossing also refreshes the slice's cached
    // rxAntenna/txAntenna labels so the VFO Flag / RxApplet buttons
    // show the new band's value. Prior to the 2026-04-22 fix, T10
    // only reapplied to the wire — slice labels stayed on the old
    // band and the user reported "it did not switch automatically"
    // (which was really "the UI label didn't update"). KG4VCF caught
    // it on the bench against ANAN-G2.
    void band_crossing_refreshes_slice_labels() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);
        model.addSlice();
        SliceModel* slice = model.sliceAt(0);
        QVERIFY(slice);

        // Store antenna 3 for 40m in the controller but NOT on the slice
        // (the slice default is "ANT1"). Because m_lastBand starts at
        // Band20m, the intermediate antennaChanged(Band40m) emission is
        // filtered by T9's band-match check, so slice->rxAntenna stays
        // at the default.
        model.alexControllerMutable().setRxAnt(Band::Band40m, 3);
        QCOMPARE(slice->rxAntenna(), QStringLiteral("ANT1"));

        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);

        // Simulate band crossing. setLastBandForTest fires both the
        // wire reapply (applyAlexAntennaForBand) AND the slice label
        // refresh (refreshAntennasFromAlex) — mirrors production T10.
        model.setLastBandForTest(Band::Band40m);

        QCOMPARE(slice->rxAntenna(), QStringLiteral("ANT3"));

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // !caps.hasAlex → zeros on the wire (Thetis Alex.cs:312-317 SetAntBits(0,0,0,0,false) parity).
    void hasAlex_false_writes_zero_routing() {
        RadioModel model;
        // setCapsForTest(false): the static override returns hasAlex=false,
        // which causes applyAlexAntennaForBand to zero trxAnt and txAnt.
        model.setCapsForTest(/*hasAlex=*/false);

        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        mock->calls.clear();

        model.onConnectedForTest();

        QVERIFY(mock->calls.size() >= 1);
        QCOMPARE(mock->calls.last().trxAnt, 0);
        QCOMPARE(mock->calls.last().txAnt,  0);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // ── Phase 3P-I-b (T6) full composition ─────────────────────────────────────

    // RX-only ant propagates on RX band: rxOnlyAnt=2 → routing.rxOnlyAnt=2,
    // rxOut=true. From Thetis Alex.cs:350-361 [@501e3f5].
    void rxOnly_propagates_on_rx_band() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);

        model.alexControllerMutable().setRxOnlyAnt(Band::Band20m, 2);
        mock->calls.clear();
        model.applyAlexAntennaForBandForTest(Band::Band20m, /*isTx=*/false);

        QVERIFY(mock->calls.size() >= 1);
        QCOMPARE(mock->calls.last().rxOnlyAnt, 2);
        QCOMPARE(mock->calls.last().rxOut, true);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // isTx=true + Ext1OutOnTx=true → rxOnlyAnt=2 (Ext1 mapped), rxOut=true.
    // From Thetis Alex.cs:341-345 [@501e3f5].
    void ext1_on_tx_maps_rxOnly_2() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);

        model.alexControllerMutable().setExt1OutOnTx(true);
        mock->calls.clear();
        model.applyAlexAntennaForBandForTest(Band::Band20m, /*isTx=*/true);

        QVERIFY(mock->calls.size() >= 1);
        QCOMPARE(mock->calls.last().rxOnlyAnt, 2);
        QCOMPARE(mock->calls.last().rxOut, true);
        QCOMPARE(mock->calls.last().tx, true);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // isTx=true + Ext2OutOnTx=true → rxOnlyAnt=1 (Ext2 mapped), rxOut=true.
    // From Thetis Alex.cs:341 [@501e3f5].
    void ext2_on_tx_maps_rxOnly_1() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);

        model.alexControllerMutable().setExt2OutOnTx(true);
        mock->calls.clear();
        model.applyAlexAntennaForBandForTest(Band::Band20m, /*isTx=*/true);

        QVERIFY(mock->calls.size() >= 1);
        QCOMPARE(mock->calls.last().rxOnlyAnt, 1);
        QCOMPARE(mock->calls.last().rxOut, true);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // xvtrActive=true + rxOnlyAnt=3 → routing.rxOnlyAnt=3 preserved (XVTR port kept).
    // From Thetis Alex.cs:352-354 [@501e3f5].
    void xvtr_active_preserves_rxOnly_3() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);

        model.alexControllerMutable().setRxOnlyAnt(Band::Band20m, 3);
        model.alexControllerMutable().setXvtrActive(true);
        mock->calls.clear();
        model.applyAlexAntennaForBandForTest(Band::Band20m, /*isTx=*/false);

        QVERIFY(mock->calls.size() >= 1);
        QCOMPARE(mock->calls.last().rxOnlyAnt, 3);
        QCOMPARE(mock->calls.last().rxOut, true);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // xvtrActive=false + rxOnlyAnt=3 → clamped to 0 (3-3=0, XVTR port suppressed).
    // From Thetis Alex.cs:357-358 [@501e3f5]:
    //   "if (rx_only_ant >= 3) rx_only_ant -= 3; // do not use XVTR ant port..."
    void xvtr_inactive_clamps_rxOnly_3_to_0() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);

        model.alexControllerMutable().setRxOnlyAnt(Band::Band20m, 3);
        model.alexControllerMutable().setXvtrActive(false);
        mock->calls.clear();
        model.applyAlexAntennaForBandForTest(Band::Band20m, /*isTx=*/false);

        QVERIFY(mock->calls.size() >= 1);
        QCOMPARE(mock->calls.last().rxOnlyAnt, 0);  // 3 - 3 = 0
        QCOMPARE(mock->calls.last().rxOut, false);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // rxOutOverride=true + rxOnlyAnt=1 (rxOut=true) → trxAnt forced to 4, rxOut=false.
    // From Thetis Alex.cs:368-374 [@501e3f5].
    //G8NJJ  [Aries adjacency — Thetis Alex.cs:376 "G8NJJ support for external Aries ATU"]
    void rxOutOverride_forces_trxAnt_4_on_rx() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);

        model.alexControllerMutable().setRxOnlyAnt(Band::Band20m, 1);
        model.alexControllerMutable().setRxOutOverride(true);
        mock->calls.clear();
        model.applyAlexAntennaForBandForTest(Band::Band20m, /*isTx=*/false);

        QVERIFY(mock->calls.size() >= 1);
        QCOMPARE(mock->calls.last().trxAnt, 4);
        QCOMPARE(mock->calls.last().rxOut, false);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // !caps.hasAlex → all fields zeroed + tx=false (full zero-out check).
    // Complements hasAlex_false_writes_zero_routing with rxOnlyAnt/rxOut fields.
    // From Thetis Alex.cs:312-316 [@501e3f5].
    void hasAlex_false_emits_all_zero() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/false);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        mock->calls.clear();

        model.applyAlexAntennaForBandForTest(Band::Band20m, /*isTx=*/false);

        QVERIFY(mock->calls.size() >= 1);
        const AntennaRouting& r = mock->calls.last();
        QCOMPARE(r.rxOnlyAnt, 0);
        QCOMPARE(r.trxAnt,    0);
        QCOMPARE(r.txAnt,     0);
        QCOMPARE(r.rxOut,     false);
        QCOMPARE(r.tx,        false);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }
};

QTEST_MAIN(TestAntennaRoutingModel)
#include "tst_antenna_routing_model.moc"
