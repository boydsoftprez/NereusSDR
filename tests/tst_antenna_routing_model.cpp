// no-port-check: test fixture asserts RadioModel antenna pump (Phase 3P-I-a T14)
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/Band.h"
#include "core/RadioConnection.h"
#include "core/accessories/AlexController.h"
#include "core/AppSettings.h"
#include "core/TxSliceArbiter.h"

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
        SliceModel* slice = model.sliceById(0);
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

    // ── Issue #257 regression coverage ────────────────────────────────────────

    // Issue #257 (Bug 2a): picking ANT* after an EXT* / BYPS / XVTR pick must
    // also clear rxOnlyAnt so the RX-bypass mux releases and the main TX/RX
    // input is restored. Without the fix, m_rxOnlyAnt stays non-zero and the
    // wire keeps engaging the bypass mux even though the UI shows "ANT1".
    //
    // Mirrors the user-reported flow on w3ub's ANAN-7000DLE Mk II.
    void issue257_picking_ANT_releases_rxOnly_mux() {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::OrionMKII);  // ANAN-7000D family — SKU labels BYPS/EXT1/XVTR
        model.setCapsForTest(/*hasAlex=*/true);
        model.addSlice();
        SliceModel* slice = model.sliceById(0);
        QVERIFY(slice);

        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        model.wireSliceSignalsForTest();

        // Stage 1: user picks "EXT1" → rxOnlyAnt should land at 2.
        // (BYPS=1, EXT1=2, XVTR=3 in the SkuUiProfile for ANAN-7000D.)
        slice->setRxAntenna(QStringLiteral("EXT1"));
        QCOMPARE(model.alexController().rxOnlyAnt(model.lastBand()), 2);

        // Stage 2: user picks "ANT1" → handler must clear rxOnlyAnt to 0.
        // This is the regression check: pre-fix, setRxOnlyAnt(0) was never
        // called and the mux stayed engaged.
        slice->setRxAntenna(QStringLiteral("ANT1"));
        QCOMPARE(model.alexController().rxOnlyAnt(model.lastBand()), 0);
        QCOMPARE(model.alexController().rxAnt(model.lastBand()),     1);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // Issue #257 (Bug 2b): the slice's cached m_rxAntenna label must reflect
    // the SKU-specific RX-only label (BYPS/EXT1/XVTR for ANAN-7000D) when
    // rxOnlyAnt != 0. Pre-fix, refreshAntennasFromAlex always returned
    // "ANT1/2/3" from alex.rxAnt(band), so the indicator showed "ANT1" while
    // the radio was actually routing through EXT1 — the user saw the wrong
    // label AND couldn't restore ANT1 because setRxAntenna's equality guard
    // no-op'd the next ANT1 pick.
    void issue257_refresh_shows_rxOnly_label_on_mkii_sku() {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::OrionMKII);  // ANAN-7000D family
        model.setCapsForTest(/*hasAlex=*/true);
        model.addSlice();
        SliceModel* slice = model.sliceById(0);
        QVERIFY(slice);

        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        model.wireSliceSignalsForTest();

        // Seed rxOnlyAnt=2 (EXT1 slot on ANAN-7000D) via the controller.
        // The antennaChanged signal triggers RadioModel's connect lambda,
        // which calls applyAlexAntennaForBand + refreshAntennasFromAlex.
        model.alexControllerMutable().setRxOnlyAnt(model.lastBand(), 2);

        // Slice's cached rxAntenna should now show the SKU label, NOT "ANT1".
        QCOMPARE(slice->rxAntenna(), QStringLiteral("EXT1"));

        // Clearing rxOnlyAnt back to 0 should drop the label back to "ANT1".
        model.alexControllerMutable().setRxOnlyAnt(model.lastBand(), 0);
        QCOMPARE(slice->rxAntenna(), QStringLiteral("ANT1"));

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

    // Group B fix wave: a TX antenna change while the radio transmits, as
    // Thetis applies it: setup.cs ProcessAlexAntRadioButton sets the band's
    // TX antenna and console.AlexAntCtrlEnabled re-runs
    // UpdateAlexAntSelection(RX1Band, _mox, ...) at once, so the relays
    // take the new antenna on the TX routing (tx = true), never the
    // receive routing mid-transmission. The out-on-TX flags only set
    // Alex's statics there (chkRxOutOnTx / chkEXT1OutOnTx /
    // chkEXT2OutOnTx), so they reach the relays at the next MOX edge.
    void txAntennaChangedWhileTransmittingKeepsTheTxRouting() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);
        model.addSlice(QStringLiteral("pan-0"));
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        const Band band = model.lastBand();

        model.onMoxHardwareFlipped(true);
        QVERIFY(!mock->calls.isEmpty());
        QVERIFY(mock->calls.last().tx);
        mock->calls.clear();

        model.alexControllerMutable().setTxAnt(band, 2);
        QVERIFY(!mock->calls.isEmpty());
        QVERIFY(mock->calls.last().tx);
        QCOMPARE(mock->calls.last().txAnt, 2);
        QCOMPARE(mock->calls.last().trxAnt, 2);

        // A flag while transmitting waits for the next MOX edge.
        mock->calls.clear();
        model.alexControllerMutable().setRxOutOnTx(true);
        QVERIFY(mock->calls.isEmpty());

        // Back on receive: the receive routing, with the new TX antenna kept
        // for the next transmission.
        model.onMoxHardwareFlipped(false);
        QVERIFY(!mock->calls.isEmpty());
        QVERIFY(!mock->calls.last().tx);
        mock->calls.clear();
        model.onMoxHardwareFlipped(true);
        QVERIFY(mock->calls.last().tx);
        QCOMPARE(mock->calls.last().txAnt, 2);
        QVERIFY(mock->calls.last().rxOut);

        // Off the air a TX antenna change keeps the receive routing.
        model.onMoxHardwareFlipped(false);
        mock->calls.clear();
        model.alexControllerMutable().setTxAnt(band, 3);
        QVERIFY(!mock->calls.isEmpty());
        QVERIFY(!mock->calls.last().tx);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // Parity Task 12 (group B fix wave re-review, Minor 2): a radio that
    // goes away while transmitting leaves no TX routing behind. After the
    // next connect an antenna change goes out as the receive routing and a
    // relay flag is applied at once, not held for a MOX edge.
    void disconnectWhileTransmittingLeavesTheReceiveRouting() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);
        model.addSlice(QStringLiteral("pan-0"));
        auto* first = new MockConnection();
        model.injectConnectionForTest(first);
        const Band band = model.lastBand();
        model.onMoxHardwareFlipped(true);
        QVERIFY(!first->calls.isEmpty());
        QVERIFY(first->calls.last().tx);

        model.disconnectFromRadio();
        delete first;

        auto* second = new MockConnection();
        model.injectConnectionForTest(second);
        model.alexControllerMutable().setTxAnt(band, 2);
        QVERIFY(!second->calls.isEmpty());
        QVERIFY(!second->calls.last().tx);
        second->calls.clear();
        model.alexControllerMutable().setRxOutOverride(true);
        QVERIFY(!second->calls.isEmpty());
        QVERIFY(!second->calls.last().tx);

        model.injectConnectionForTest(nullptr);
        delete second;
    }

    // Band crossing while keyed keeps the TX routing. Thetis re-runs the
    // antenna selection on a VFO A change with tx = _mox
    // (console.cs:31713-31717 [v2.10.3.15] txtVFOAFreq_LostFocus:
    //   Alex.getAlex().UpdateAlexAntSelection(RX1Band, _mox, alex_ant_ctrl_enabled, false);
    // ), so the relays take the new band's transmit antenna at once and
    // never the receive routing mid-transmission.
    void keyedBandCrossingOfTheTxSliceKeepsTheTxRouting() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);
        model.addSlice(QStringLiteral("pan-0"));
        SliceModel* slice = model.sliceById(0);
        QVERIFY(slice);
        QCOMPARE(model.txBoundSlice(), slice);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        model.alexControllerMutable().setTxAnt(Band::Band40m, 2);
        model.alexControllerMutable().setRxAnt(Band::Band40m, 3);
        model.enableBandTrackingForTest();
        slice->setFrequency(14074000.0);
        QCOMPARE(model.lastBand(), Band::Band20m);

        model.onMoxHardwareFlipped(true);
        QVERIFY(mock->calls.last().tx);
        mock->calls.clear();

        slice->setFrequency(7074000.0);
        QCOMPARE(model.lastBand(), Band::Band40m);
        QVERIFY(!mock->calls.isEmpty());
        for (const AntennaRouting& r : std::as_const(mock->calls)) {
            QVERIFY2(r.tx, "a receive routing went out while keyed");
        }
        QCOMPARE(mock->calls.last().txAnt, 2);
        QCOMPARE(mock->calls.last().trxAnt, 2);

        // Back on receive: the new band's receive antenna.
        mock->calls.clear();
        model.onMoxHardwareFlipped(false);
        QVERIFY(!mock->calls.isEmpty());
        QVERIFY(!mock->calls.last().tx);
        QCOMPARE(mock->calls.last().trxAnt, 3);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // A receive slice crossing a band edge while another slice transmits
    // sends no receive routing mid-transmission; its routing goes out when
    // the radio returns to receive (the NereusSDR slice rule the antenna
    // change handler already follows while keyed).
    void keyedBandCrossingOfAReceiveSliceSendsNoReceiveRouting() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);
        model.addSlice(QStringLiteral("pan-0"));
        model.addSlice(QStringLiteral("pan-1"));
        SliceModel* tx = model.txBoundSlice();
        QVERIFY(tx);
        SliceModel* rx = (tx == model.sliceById(0)) ? model.sliceById(1) : model.sliceById(0);
        QVERIFY(rx && rx != tx);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        model.alexControllerMutable().setRxAnt(Band::Band40m, 3);
        model.enableBandTrackingForTest();
        tx->setFrequency(14074000.0);
        rx->setFrequency(14200000.0);

        model.onMoxHardwareFlipped(true);
        mock->calls.clear();

        rx->setFrequency(7074000.0);
        for (const AntennaRouting& r : std::as_const(mock->calls)) {
            QVERIFY2(r.tx, "a receive routing went out while keyed");
        }

        // Unkey: the receive band's routing, the band the receive slice
        // crossed to (40 m, ANT3), not the transmit slice's 20 m. Thetis
        // HdwMOXChanged routes the transmit band at key-down and rx1_band
        // on return to receive (console.cs:29161-29169 [v2.10.3.15]).
        mock->calls.clear();
        model.onMoxHardwareFlipped(false);
        QVERIFY(!mock->calls.isEmpty());
        QVERIFY(!mock->calls.last().tx);
        QCOMPARE(mock->calls.last().trxAnt, 3);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // ── Setup's antenna table is the only per-band store ─────────────────────

    // Reported on Discord (OZ1CT, 2026-10-22): go from 80 m to 20 m and back
    // to 80 m, and 80 m's row of Setup's antenna table holds 20 m's antennas.
    // The slice saved one antenna label for the whole slice, and the band
    // button's restore wrote it into the band it returned to.
    void band_return_keeps_setup_antennas() {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::OrionMKII);
        model.setCapsForTest(/*hasAlex=*/true);
        AlexController& alex = model.alexControllerMutable();
        alex.setRxAnt(Band::Band80m, 3);
        alex.setTxAnt(Band::Band80m, 3);
        alex.setRxAnt(Band::Band20m, 2);
        alex.setTxAnt(Band::Band20m, 2);

        model.addSlice();
        model.setActiveSlice(0);
        SliceModel* slice = model.sliceById(0);
        QVERIFY(slice);

        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        model.wireSliceSignalsForTest();

        slice->setFrequency(3700000.0);   // 80 m
        QCOMPARE(model.lastBand(), Band::Band80m);
        QCOMPARE(slice->rxAntenna(), QStringLiteral("ANT3"));

        model.onBandButtonClicked(Band::Band20m);   // first visit: the seed
        QCOMPARE(model.lastBand(), Band::Band20m);
        QCOMPARE(slice->rxAntenna(), QStringLiteral("ANT2"));

        model.onBandButtonClicked(Band::Band80m);   // second visit: the restore
        QCOMPARE(model.lastBand(), Band::Band80m);

        QCOMPARE(alex.rxAnt(Band::Band80m), 3);
        QCOMPARE(alex.txAnt(Band::Band80m), 3);
        QCOMPARE(alex.rxAnt(Band::Band20m), 2);
        QCOMPARE(alex.txAnt(Band::Band20m), 2);
        QCOMPARE(slice->rxAntenna(), QStringLiteral("ANT3"));
        QCOMPARE(slice->txAntenna(), QStringLiteral("ANT3"));
        QVERIFY(!mock->calls.isEmpty());
        QCOMPARE(mock->calls.last().trxAnt, 3);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // Reported with the case above: switch the radio off and on again, and
    // the band the slice was on (20 m) takes another antenna in Setup's
    // table. The connect-time restore of the slice's last band wrote the
    // slice's own saved label (Slice0/RxAntenna, TxAntenna, which settings
    // files from earlier builds still hold) over that band's row.
    void reconnect_keeps_setup_antennas() {
        auto& s = AppSettings::instance();
        const auto cleanup = qScopeGuard([&s] {
            for (const QString& key : {QStringLiteral("Slice0/LastBand"),
                                       QStringLiteral("Slice0/Band20m/Frequency"),
                                       QStringLiteral("Slice0/RxAntenna"),
                                       QStringLiteral("Slice0/TxAntenna")}) {
                s.remove(key);
            }
        });
        s.setValue(QStringLiteral("Slice0/LastBand"), QStringLiteral("20m"));
        s.setValue(QStringLiteral("Slice0/Band20m/Frequency"), 14200000.0);
        s.setValue(QStringLiteral("Slice0/RxAntenna"), QStringLiteral("ANT1"));
        s.setValue(QStringLiteral("Slice0/TxAntenna"), QStringLiteral("ANT1"));

        RadioModel model;
        model.setBoardForTest(HPSDRHW::OrionMKII);
        model.setCapsForTest(/*hasAlex=*/true);
        AlexController& alex = model.alexControllerMutable();
        alex.setRxAnt(Band::Band20m, 3);
        alex.setTxAnt(Band::Band20m, 3);

        model.addSlice();
        model.setActiveSlice(0);
        SliceModel* slice = model.sliceById(0);
        QVERIFY(slice);

        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        model.wireSliceSignalsForTest();

        model.loadSliceStateForTest(slice);

        QCOMPARE(slice->frequency(), 14200000.0);
        QCOMPARE(alex.rxAnt(Band::Band20m), 3);
        QCOMPARE(alex.txAnt(Band::Band20m), 3);
        QCOMPARE(slice->rxAntenna(), QStringLiteral("ANT3"));
        QCOMPARE(slice->txAntenna(), QStringLiteral("ANT3"));

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // PR 637 review: a Setup edit to the transmit slice's band, while
    // another slice is active, reaches the transmit slice's TX label, so
    // key-down (applyTxAntennaFromBoundSlice) does not write the old label
    // back into the row.
    void setup_tx_edit_reaches_the_transmit_slice() {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::OrionMKII);
        model.setCapsForTest(/*hasAlex=*/true);
        model.addSlice(QStringLiteral("pan-0"));
        model.addSlice(QStringLiteral("pan-1"));
        model.setActiveSlice(0);
        SliceModel* active = model.sliceById(0);
        SliceModel* tx = model.sliceById(1);
        QVERIFY(active && tx);
        QVERIFY(model.txSliceArbiter()->requestHandoff(1));
        QCOMPARE(model.txBoundSlice(), tx);
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        model.enableBandTrackingForTest();
        tx->setFrequency(7100000.0);
        active->setFrequency(14200000.0);

        AlexController& alex = model.alexControllerMutable();
        alex.setTxAnt(Band::Band40m, 2);
        QCOMPARE(tx->txAntenna(), QStringLiteral("ANT2"));

        model.onMoxHardwareFlipped(true);
        model.onMoxHardwareFlipped(false);
        QCOMPARE(alex.txAnt(Band::Band40m), 2);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // PR 637 review: a slice tuned onto the band the station is already on
    // (no crossing) takes that band's labels, so as the transmit slice it
    // does not carry its old band's TX antenna into this band's row.
    void slice_moving_onto_the_station_band_takes_its_labels() {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::OrionMKII);
        model.setCapsForTest(/*hasAlex=*/true);
        model.addSlice(QStringLiteral("pan-0"));
        model.addSlice(QStringLiteral("pan-1"));
        model.setActiveSlice(0);
        SliceModel* active = model.sliceById(0);
        SliceModel* tx = model.sliceById(1);
        QVERIFY(active && tx);
        QVERIFY(model.txSliceArbiter()->requestHandoff(1));
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        AlexController& alex = model.alexControllerMutable();
        alex.setRxAnt(Band::Band40m, 3);
        alex.setTxAnt(Band::Band40m, 3);
        alex.setRxAnt(Band::Band20m, 2);
        alex.setTxAnt(Band::Band20m, 2);
        model.enableBandTrackingForTest();
        tx->setFrequency(7100000.0);
        active->setFrequency(14200000.0);
        QCOMPARE(model.lastBand(), Band::Band20m);
        QCOMPARE(tx->txAntenna(), QStringLiteral("ANT3"));

        tx->setFrequency(14100000.0);   // onto 20 m, the station's band
        QCOMPARE(tx->rxAntenna(), QStringLiteral("ANT2"));
        QCOMPARE(tx->txAntenna(), QStringLiteral("ANT2"));

        model.onMoxHardwareFlipped(true);
        model.onMoxHardwareFlipped(false);
        QCOMPARE(alex.txAnt(Band::Band20m), 2);
        QCOMPARE(alex.txAnt(Band::Band40m), 3);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // PR 637 review: on connect every slice, not only the active one, names
    // its antennas from the connected radio's table, so a radio's rows and
    // labels do not carry across to another radio.
    void connect_names_every_slice_from_the_radios_table() {
        auto& s = AppSettings::instance();
        const QString macA = QStringLiteral("00:11:22:33:44:0A");
        const QString macB = QStringLiteral("00:11:22:33:44:0B");
        const auto cleanup = qScopeGuard([&s, macA, macB] {
            s.clearHardwareValues(macA);
            s.clearHardwareValues(macB);
        });
        s.setValue(QStringLiteral("hardware/%1/alex/antenna/40m/rx").arg(macB), QStringLiteral("3"));
        s.setValue(QStringLiteral("hardware/%1/alex/antenna/40m/tx").arg(macB), QStringLiteral("2"));

        RadioModel model;
        model.setBoardForTest(HPSDRHW::OrionMKII);
        model.setCapsForTest(/*hasAlex=*/true);
        AlexController& alex = model.alexControllerMutable();
        alex.setMacAddress(macA);
        model.addSlice(QStringLiteral("pan-0"));
        model.addSlice(QStringLiteral("pan-1"));
        model.setActiveSlice(0);
        SliceModel* active = model.sliceById(0);
        SliceModel* other = model.sliceById(1);
        QVERIFY(active && other);
        // The other slice neither active nor transmitting, so only the
        // connect-time refresh names it.
        QVERIFY(model.txSliceArbiter()->requestHandoff(0));
        model.enableBandTrackingForTest();
        other->setFrequency(7100000.0);
        active->setFrequency(14200000.0);
        QCOMPARE(other->rxAntenna(), QStringLiteral("ANT1"));

        // Radio B: its table loads, then the connection comes up.
        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);
        alex.setMacAddress(macB);
        alex.load();
        model.onConnectionStateChangedForTest(ConnectionState::Connected);

        QCOMPARE(other->rxAntenna(), QStringLiteral("ANT3"));
        QCOMPARE(other->txAntenna(), QStringLiteral("ANT2"));
        QCOMPARE(active->rxAntenna(), QStringLiteral("ANT1"));
        QCOMPARE(alex.rxAnt(Band::Band40m), 3);
        QCOMPARE(alex.txAnt(Band::Band40m), 2);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }
};

QTEST_MAIN(TestAntennaRoutingModel)
#include "tst_antenna_routing_model.moc"
