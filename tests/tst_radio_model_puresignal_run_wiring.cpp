// no-port-check: NereusSDR-original unit-test file.  All Thetis source
// citations below are cite comments documenting which upstream lines each
// assertion verifies; no Thetis logic is ported in this test file.
// =================================================================
// tests/tst_radio_model_puresignal_run_wiring.cpp  (NereusSDR)
// =================================================================
//
// Model-to-connection wiring tests for Task 2.5 of the P1 full-parity epic.
//
// Verifies the TransmitModel::pureSig → RadioConnection::setPuresignalRun
// plumbing installed in RadioModel::wireConnectionSignals + the initial-push
// pattern in RadioModel::connectToRadio.  The wire-byte side of the bit
// (bank 11 C2 bit 6, mask 0x40) is already pinned by
// tst_p1_puresignal_run_setter; this file pins the higher-level model-layer
// wiring contract.
//
// Strategy:
//   - Same-thread harness: construct P1RadioConnection + TransmitModel on
//     the test thread (no QThread) and use Qt::DirectConnection so the
//     wiring is synchronous.  This mirrors the Qt::QueuedConnection used in
//     RadioModel::wireConnectionSignals, with the same signal→slot binding
//     graph.
//   - Toggle TransmitModel::setPureSigEnabled and assert bank-11 C2 bit 6
//     reflects the new value via captureBank11ForTest.
//   - Initial-push: set the model state BEFORE wiring to mirror the
//     RadioModel::connectToRadio FIFO invariant where the persisted model
//     state is pushed to the connection prior to the first C&C frame.
//
// Source cites:
//   Thetis ChannelMaster/networkproto1.c:599-600 [v2.10.3.13]
//     case 11:
//       C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
//   Thetis PSForm.cs:240 [v2.10.3.13]
//     _psenabled = value;
//     if (_psenabled) NetworkIO.SetPureSignal(1);
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/P1RadioConnection.h"
#include "core/PureSignal.h"
#include "core/TxChannel.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TstRadioModelPuresignalRunWiring : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()  { AppSettings::instance().clear(); }
    void init()          { AppSettings::instance().clear(); }
    void cleanup()       { AppSettings::instance().clear(); }

    // ── 1. Initial connect: applet enable state default → wire bit clear ────
    // Mirrors the RadioModel::connectToRadio initial-push:
    //   QMetaObject::invokeMethod(m_connection,
    //     [conn, ps = m_transmitModel.pureSigEnabled()]() {
    //       conn->setPuresignalRun(ps);
    //     });
    // With a fresh TransmitModel (default false), the initial push must
    // leave bank 11 C2 bit 6 clear on the wire.
    void initialPush_default_clearsBit6()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);  // codec path

        TransmitModel tm;
        // Initial-push pattern (RadioModel::connectToRadio Task 2.5 block).
        conn.setPuresignalRun(tm.pureSigEnabled());

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        // C2 bit 6 (mask 0x40) must be 0 by default.
        QCOMPARE(int(quint8(bank11[2]) & 0x40), 0);
    }

    // ── 2. Initial connect: persisted true → wire bit set on first frame ────
    // Models the case where pureSig was persisted true (user PS-enable toggle
    // was on at last save) and a fresh connect cycle pushes it.
    void initialPush_persistedTrue_setsBit6()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        TransmitModel tm;
        tm.setPureSigEnabled(true);  // model carries the persisted state

        // Initial-push: connection-side setter takes the model state.
        conn.setPuresignalRun(tm.pureSigEnabled());

        const QByteArray bank11 = conn.captureBank11ForTest();
        QCOMPARE(bank11.size(), 5);
        // Bit 6 (mask 0x40) must be set.
        QCOMPARE(int(quint8(bank11[2]) & 0x40), 0x40);
    }

    // ── 3. Toggle on: model setter → wired signal → connection setter ───────
    // The end-to-end wiring path: pureSigChanged signal fires from
    // setPureSigEnabled, the connect() installed in
    // RadioModel::wireConnectionSignals routes it to setPuresignalRun, and
    // bank 11 C2 bit 6 reflects the new state.
    void toggleOn_modelToConnection_setsBit6()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        TransmitModel tm;

        // Mirror RadioModel::wireConnectionSignals Task 2.5 connect pair —
        // DirectConnection because both objects live on the test thread.
        QObject::connect(&tm, &TransmitModel::pureSigChanged,
                         &conn, &RadioConnection::setPuresignalRun,
                         Qt::DirectConnection);

        // Sanity: default state is bit clear before toggle.
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[2]) & 0x40), 0);

        tm.setPureSigEnabled(true);

        // After the model setter fires pureSigChanged, the wiring must have
        // landed setPuresignalRun(true) on the connection — bank 11 C2 bit 6
        // reflects 0x40.
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[2]) & 0x40), 0x40);
    }

    // ── 4. Toggle off: model false → connection clears bit 6 ────────────────
    // After toggle on, toggle off must clear bit 6 via the same wiring.
    void toggleOff_modelToConnection_clearsBit6()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        TransmitModel tm;
        QObject::connect(&tm, &TransmitModel::pureSigChanged,
                         &conn, &RadioConnection::setPuresignalRun,
                         Qt::DirectConnection);

        // Set up: enable then disable.
        tm.setPureSigEnabled(true);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[2]) & 0x40), 0x40);

        tm.setPureSigEnabled(false);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[2]) & 0x40), 0);
    }

    // ── 5. Round-trip: false → true → false bit-tracks across the wiring ────
    // Each transition must land the correct wire bit through the model
    // signal→connection slot path.
    void roundTrip_falseTrueFalse_wiringBitTracks()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        TransmitModel tm;
        QObject::connect(&tm, &TransmitModel::pureSigChanged,
                         &conn, &RadioConnection::setPuresignalRun,
                         Qt::DirectConnection);

        // Start false (default — no signal expected, but capture is clear).
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[2]) & 0x40), 0);

        tm.setPureSigEnabled(true);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[2]) & 0x40), 0x40);

        tm.setPureSigEnabled(false);
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[2]) & 0x40), 0);
    }

    // ── 6. Idempotent setter: same value twice → wiring fires only on change ─
    // pureSigChanged is gated by the inequality guard in setPureSigEnabled,
    // so repeat-set with the same value emits no signal — verified via
    // QSignalSpy on the model side and bit stability on the wire side.
    void idempotentSet_noDoubleFireOnWire()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        TransmitModel tm;
        QObject::connect(&tm, &TransmitModel::pureSigChanged,
                         &conn, &RadioConnection::setPuresignalRun,
                         Qt::DirectConnection);

        QSignalSpy spy(&tm, &TransmitModel::pureSigChanged);

        tm.setPureSigEnabled(true);   // change → 1 signal
        tm.setPureSigEnabled(true);   // same value → no signal
        tm.setPureSigEnabled(true);   // same value → no signal
        QCOMPARE(spy.count(), 1);

        // Bit 6 stays set across the redundant calls.
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[2]) & 0x40), 0x40);
    }

    // ── 7. Cross-bit guard: pureSig wiring does not perturb C2 line_in_gain ─
    // The wire bit lives in C2 bit 6; line_in_gain occupies C2 low 5 bits.
    // The wiring must not corrupt the line_in_gain bits when toggling
    // pureSig only (defaults: lineInGain = 0 → low 5 bits = 0).
    // Source: Thetis networkproto1.c:599-600 [v2.10.3.13]
    //   C2 = (prn->mic.line_in_gain & 0b00011111) | ((prn->puresignal_run & 1) << 6);
    void crossBitGuard_pureSigWiringLeavesLineInGainAlone()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        TransmitModel tm;
        QObject::connect(&tm, &TransmitModel::pureSigChanged,
                         &conn, &RadioConnection::setPuresignalRun,
                         Qt::DirectConnection);

        tm.setPureSigEnabled(true);

        const QByteArray bank11 = conn.captureBank11ForTest();
        // C2 low 5 bits (mask 0x1F) must remain 0 (lineInGain default 0).
        QCOMPARE(int(quint8(bank11[2]) & 0x1F), 0);
        // Whole C2 byte must equal exactly 0x40 (only bit 6 set).
        QCOMPARE(int(quint8(bank11[2])), 0x40);
    }

    // ── 8. Persistence load: per-MAC key is NOT read (Task 15) ──────────────
    // Phase 3M-4 Task 15 removed the dead per-MAC pureSignal/enabled read
    // from TransmitModel::loadFromSettings.  Per design doc §9.1 the
    // canonical PS-enable persistence path is per-TX-profile via
    // MicProfileManager Pure_Signal_Enabled (Task 7); Thetis matches —
    // PSEnabled is implicit-via-profile-recall, not stored as a per-radio
    // sticky.  These tests assert the dead read is gone: even if the
    // (orphaned) per-MAC key is set, loadFromSettings() must NOT seed the
    // model from it — the model's pureSigEnabled() must remain at whatever
    // value it had before the load call.
    void loadFromSettings_doesNotReadPerMacKey()
    {
        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");

        // Pre-seed the orphaned per-MAC key (no live writer exists; this
        // simulates a stale entry left over from an old install or a
        // future wayward writer regression).
        AppSettings::instance().setHardwareValue(
            mac, QStringLiteral("pureSignal/enabled"),
            QStringLiteral("True"));

        TransmitModel tm;
        // Default state before load.
        QCOMPARE(tm.pureSigEnabled(), false);

        tm.loadFromSettings(mac);

        // Per Task 15: the dead read is removed.  The orphaned per-MAC
        // key MUST NOT seed the model; pureSigEnabled stays at default.
        QCOMPARE(tm.pureSigEnabled(), false);
    }

    void loadFromSettings_preservesPriorPureSigStateWhenKeyAbsent()
    {
        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");
        // Don't set the key.

        TransmitModel tm;
        tm.setPureSigEnabled(true);  // pre-load state
        tm.loadFromSettings(mac);

        // loadFromSettings must NOT touch pureSigEnabled (Task 15: dead read
        // removed).  Pre-load state survives.
        QCOMPARE(tm.pureSigEnabled(), true);
    }

    // The auto-cal checkbox is a preference. Until the cmd-state machine
    // actually enters a TurnOn* state, the codec must see PS as stopped.
    //
    // Mutation caught: currentCodecContext reading isAutoCalEnabled() turns
    // on feedback DDC routing before PSEnabled has changed.
    void codecContext_autoCalPreferenceAlone_doesNotRunPuresignal()
    {
        TxChannel tx(/*WDSP TX channel*/ 1);
        RadioModel model;
        PureSignal* ps = model.installPureSignalForTest(&tx);
        QVERIFY(ps != nullptr);

        ps->setEnabled(true);
        ps->setTimersEnabled(false);
        ps->setAutoCalEnabled(true);

        QVERIFY(ps->isAutoCalEnabled());
        QCOMPARE(model.currentCodecContextForTest().puresignalRun, false);
    }

    // Single Cal never enables the auto-cal preference, but it does set the
    // effective PSEnabled state. The codec must follow that real run state.
    //
    // Mutation caught: deriving puresignalRun from the preference leaves the
    // DDC0/DDC1 pair off during Single Cal.
    void codecContext_singleCalRun_isEffectiveWithoutAutoCalPreference()
    {
        TxChannel tx(/*WDSP TX channel*/ 1);
        RadioModel model;
        PureSignal* ps = model.installPureSignalForTest(&tx);
        QVERIFY(ps != nullptr);

        ps->setEnabled(true);
        ps->setTimersEnabled(false);
        ps->singleCalibrate();

        int info[16] = {};
        ps->processNewInfo(info); // Off -> TurnOnSingleCalibrate
        ps->processNewInfo(info); // PSEnabled=true

        QVERIFY(!ps->isAutoCalEnabled());
        QCOMPARE(model.currentCodecContextForTest().puresignalRun, true);
    }

    // Disabling after Single Cal drains through TurnOff and Off. Only the
    // effective true->false edge should request a new DDC assignment; the
    // intermediate cmd-state visits do not change the wire run predicate.
    void codecContext_disable_returnsFalse_withOneDdcRecompute()
    {
        TxChannel tx(/*WDSP TX channel*/ 1);
        RadioModel model;
        PureSignal* ps = model.installPureSignalForTest(&tx);
        QVERIFY(ps != nullptr);

        ps->setEnabled(true);
        ps->setTimersEnabled(false);
        ps->singleCalibrate();

        int info[16] = {};
        ps->processNewInfo(info);
        ps->processNewInfo(info);
        QCOMPARE(model.currentCodecContextForTest().puresignalRun, true);

        QSignalSpy assignmentSpy(&model, &RadioModel::ddcAssignmentRequested);
        ps->reset();
        info[15] = 0; // EngineState::LRESET
        ps->processNewInfo(info); // SingleCalibrate -> TurnOff
        ps->processNewInfo(info); // TurnOff -> Off
        ps->processNewInfo(info); // Off -> PSEnabled=false

        QCOMPARE(model.currentCodecContextForTest().puresignalRun, false);
        QCOMPARE(assignmentSpy.count(), 1);
    }

    // ── Codex Fix C: Single Cal triggers setPuresignalRun(true) via the
    //                 new PureSignal::psEnabledChanged fan-out ─────────────
    //
    // The legacy wiring connected autoCalEnabledChanged → setPuresignalRun.
    // Single Cal does NOT toggle autoCalEnabled, so under the legacy wiring
    // a Single Cal would leave bank 11 C2 bit 6 clear on the wire — i.e.
    // PsccPump would never see the radio in puresignal_run mode and DDC0
    // / DDC1 would not swap to TX freq during MOX.
    //
    // After Fix C, the cmd-state machine's TurnOnSingleCalibrate visit
    // emits psEnabledChanged(true) per Thetis PSForm.cs:662 [v2.10.3.13]
    // `if (!PSEnabled) PSEnabled = true;` inside case eCMDState
    // .TurnOnSingleCalibrate.  RadioModel::wireConnectionSignals reroutes
    // the connection's setPuresignalRun slot to psEnabledChanged so Single
    // Cal lands bank 11 C2 bit 6 = 0x40 on the wire.

    void singleCal_triggersPuresignalRunOnWire_viaPsEnabledChanged()
    {
        P1RadioConnection conn;
        conn.setBoardForTest(HPSDRHW::HermesII);

        // Construct a PureSignal coordinator with a real TxChannel so the
        // cmd-state machine's m_tx-guarded transitions execute.
        TxChannel tx(/*WDSP TX channel*/ 1);
        PureSignal ps(/*engine=*/nullptr, &tx, /*fb=*/nullptr,
                      /*mox=*/nullptr, /*stepAtt=*/nullptr,
                      /*twoTone=*/nullptr);

        // Mirror the post-Fix-C wiring in RadioModel::wireConnectionSignals —
        // psEnabledChanged is the radio/DDC fan-out, NOT autoCalEnabledChanged.
        QObject::connect(&ps,   &PureSignal::psEnabledChanged,
                         &conn, &RadioConnection::setPuresignalRun,
                         Qt::DirectConnection);

        ps.setEnabled(true);
        ps.setTimersEnabled(false);

        // Sanity: bit 6 starts clear before any cal kicks off.
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[2]) & 0x40), 0);

        // Single-cal entry path: no auto-cal, just btnPSCalibrate.
        ps.singleCalibrate();   // sets _singleCalON=true

        // Drive cmd-state ticks until the TurnOnSingleCalibrate visit fires
        // psEnabledChanged(true), which routes through to setPuresignalRun.
        int info[16] = {};
        ps.processNewInfo(info);   // Off → next has _singleCalON
        ps.processNewInfo(info);   // TurnOnSingleCalibrate → PSEnabled=true

        // Bank 11 C2 bit 6 must reflect 0x40 now.
        QCOMPARE(int(quint8(conn.captureBank11ForTest()[2]) & 0x40), 0x40);
    }
};

QTEST_GUILESS_MAIN(TstRadioModelPuresignalRunWiring)
#include "tst_radio_model_puresignal_run_wiring.moc"
