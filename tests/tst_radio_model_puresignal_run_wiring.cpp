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
    // Models the case where pureSig was persisted true (PureSignalTab
    // "Enable" was on at last save) and a fresh connect cycle pushes it.
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

    // ── 8. Persistence load: pureSignal/enabled key seeds the model ─────────
    // TransmitModel::loadFromSettings(mac) reads the existing
    // hardware/<mac>/pureSignal/enabled key (set by HardwarePage's
    // PureSignalTab "Enable" checkbox via setHardwareValue).  Verifies the
    // single-source-of-truth contract: Setup persists, model loads on
    // connect.
    void loadFromSettings_readsPureSignalEnabledKey()
    {
        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");

        // Pre-seed the AppSettings as the PureSignalTab "Enable" toggle does.
        AppSettings::instance().setHardwareValue(
            mac, QStringLiteral("pureSignal/enabled"),
            QStringLiteral("True"));

        TransmitModel tm;
        // Default state before load.
        QCOMPARE(tm.pureSigEnabled(), false);

        tm.loadFromSettings(mac);

        // Model picks up the persisted state.
        QCOMPARE(tm.pureSigEnabled(), true);
    }

    void loadFromSettings_defaultFalseWhenKeyAbsent()
    {
        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");
        // Don't set the key — defaults to "False" → false.

        TransmitModel tm;
        tm.setPureSigEnabled(true);  // pre-load state to verify load overrides
        tm.loadFromSettings(mac);

        // Absent key → false default per Thetis PSForm.cs:234 [v2.10.3.13].
        QCOMPARE(tm.pureSigEnabled(), false);
    }
};

QTEST_APPLESS_MAIN(TstRadioModelPuresignalRunWiring)
#include "tst_radio_model_puresignal_run_wiring.moc"
