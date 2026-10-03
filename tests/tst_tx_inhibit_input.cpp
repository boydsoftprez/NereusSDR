// no-port-check: test harness citing Thetis PollTXInhibit and the P1/P2
// status parsers as the behavioural spec it asserts against; contains no
// ported logic of its own.
// =================================================================
// tests/tst_tx_inhibit_input.cpp  (NereusSDR)
// =================================================================
//
// Task 13 (receiver and transmit gaps plan): the radio's TX inhibit input
// reaches the keying gate, read the way Thetis reads it.
//
// The spec, from Thetis v2.10.3.15:
//
//   ChannelMaster/networkproto1.c:332-336 (Protocol 1, each ep6 subframe):
//     switch (ControlBytesIn[0] & 0xf8)
//     case 0x00: // C0 0000 0000
//       prn->user_dig_in = ((ControlBytesIn[1] >> 1) & 0xf);
//
//   ChannelMaster/network.c:531 + 756 (Protocol 2, high-priority status,
//   60 bytes on port 1025): ReadUDPFrame copies readbuf + 4 (after the
//   sequence number), then
//     prn->user_dig_in = prn->ReadBufp[55];
//   so the byte is datagram byte 59. console.cs's comment says "byte 59",
//   network.c's code says ReadBufp[55]; both name the same byte.
//
//   ChannelMaster/netInterface.c:245-289: getUserI01 = bit 0,
//   getUserI02 = bit 1 (P1 names); getUserI04_p2 = bit 0,
//   getUserI05_p2 = bit 1 (P2 names).
//
//   Console/console.cs:25849-25887 PollTXInhibit, every 100 ms:
//     if (_useTxInhibit && HardwareSpecific.Model != HPSDRModel.HPSDR)
//       P1: ANAN_G2E, ANAN7000D, ANAN8000D, REDPITAYA -> !getUserI02()
//           //DH1KLM should be in P1  //N1GP G2E added
//           otherwise                              -> !getUserI01()
//       P2: ANAN7000D, ANAN8000D, ANAN_G2, ANAN_G2_1K, ANVELINAPRO3,
//           REDPITAYA                              -> !getUserI05_p2()
//           otherwise                              -> !getUserI04_p2()
//       if (_reverseTxInhibit) inhibit_input = !inhibit_input;
//
//   mi0bot-Thetis console.cs PollTXInhibit [v2.10.3.13-beta2 @c26a8a4]
//   has no Hermes Lite 2 branch: the HL2 reads !getUserI01() on P1.
//
// Also covered: a change reaches the gate within one status frame (not
// only on the 100 ms poll), the Setup checkboxes reach the monitor on
// a local radio and, from a remote window, on the Core, and a window's
// txInhibited follows the Core's input once its box is on.
//
// No hardware; nothing keys a radio; no audio device is opened.
//
// Modification history (NereusSDR):
//   2026-09-25: created (Task 13), by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: checkpoint A: a remote window's box reaches the Core's
//               monitor and the window's txInhibited follows the input.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QByteArray>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "core/MoxController.h"
#include "core/P1RadioConnection.h"
#include "core/P2RadioConnection.h"
#include "core/safety/TxInhibitMonitor.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "gui/setup/TransmitSetupPages.h"
#include "models/RadioModel.h"

#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::safety::TxInhibitMonitor;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kEnabledKey  = QStringLiteral("TxInhibitMonitorEnabled");
const QString kReversedKey = QStringLiteral("TxInhibitMonitorReversed");

// A 1032-byte ep6 datagram with both subframes' sync words and the given
// C0..C4 in each. Layout per networkproto1.c:320-331 [v2.10.3.15].
QByteArray makeEp6Frame(const quint8 sub0[5], const quint8 sub1[5])
{
    QByteArray pkt(1032, '\0');
    auto* p = reinterpret_cast<quint8*>(pkt.data());
    p[0] = 0xEF; p[1] = 0xFE; p[2] = 0x01; p[3] = 0x06;
    p[8] = 0x7F; p[9] = 0x7F; p[10] = 0x7F;
    for (int i = 0; i < 5; ++i) { p[11 + i] = sub0[i]; }
    p[520] = 0x7F; p[521] = 0x7F; p[522] = 0x7F;
    for (int i = 0; i < 5; ++i) { p[523 + i] = sub1[i]; }
    return pkt;
}

// A case-0x00 frame carrying C1 in subframe 0; subframe 1 is a 0x08 frame
// (no user inputs), so only subframe 0 reports.
QByteArray p1StatusWithC1(quint8 c1)
{
    const quint8 sub0[5] = {0x00, c1, 0x00, 0x00, 0x00};
    const quint8 sub1[5] = {0x08, 0x00, 0x00, 0x00, 0x00};
    return makeEp6Frame(sub0, sub1);
}

// The C1 byte whose bits 1..4 carry `userDigIn` (the inverse of
// networkproto1.c:336's (C1 >> 1) & 0xf).
quint8 c1ForUserInputs(quint8 userDigIn)
{
    return static_cast<quint8>((userDigIn & 0x0f) << 1);
}

// A 60-byte high-priority status datagram with `userDigIn` at datagram
// byte 59 (ReadBufp[55]) and a decoy at datagram byte 55 (ReadBufp[51]).
QByteArray p2StatusWithUserInputs(quint8 userDigIn, quint8 decoyAt55 = 0xAA)
{
    QByteArray pkt(60, '\0');
    auto* p = reinterpret_cast<quint8*>(pkt.data());
    p[3] = 0x01;              // sequence number 1
    p[55] = decoyAt55;        // ReadBufp[51]: user_adc1 high byte, not the inputs
    p[59] = userDigIn;        // ReadBufp[55]: user_dig_in
    return pkt;
}

// Which user-input bit each model's TX inhibit is on, per protocol.
// Written out from console.cs:25857-25876 [v2.10.3.15], not computed.
quint8 expectedInputMask(HPSDRModel model, int protocol)
{
    if (protocol == 1) {
        switch (model) {
        case HPSDRModel::ANAN_G2E:   //N1GP G2E added
        case HPSDRModel::ANAN7000D:
        case HPSDRModel::ANAN8000D:
        case HPSDRModel::REDPITAYA:  //DH1KLM should be in P1
            return 0x02;             // getUserI02
        default:
            return 0x01;             // getUserI01
        }
    }
    switch (model) {
    case HPSDRModel::ANAN7000D:
    case HPSDRModel::ANAN8000D:
    case HPSDRModel::ANAN_G2:
    case HPSDRModel::ANAN_G2_1K:
    case HPSDRModel::ANVELINAPRO3:
    case HPSDRModel::REDPITAYA:
        return 0x02;                 // getUserI05_p2
    default:
        return 0x01;                 // getUserI04_p2
    }
}

class ScopedRemoteBackend {
public:
    explicit ScopedRemoteBackend(ISettingsBackend* backend)
    {
        AppSettings::instance().setRemoteBackend(backend);
    }
    ~ScopedRemoteBackend() { AppSettings::instance().setRemoteBackend(nullptr); }
};

void resetSettings()
{
    AppSettings::instance().remove(kEnabledKey);
    AppSettings::instance().remove(kReversedKey);
}

} // namespace

class TestTxInhibitInput : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_securityDir;

private slots:
    void initTestCase() { QVERIFY(m_securityDir.isValid()); }
    void init() { resetSettings(); }
    void cleanup() { resetSettings(); }

    // ---- Protocol 1 status parsing ----------------------------------------

    void p1CaseZeroCarriesUserInputsInC1Bits1To4()
    {
        P1RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &RadioConnection::userDigitalInputsChanged);

        // C1 = 1110 0101: bit 0 is ADC overload, bits 5-7 are not inputs.
        // (C1 >> 1) & 0xf = 0010.
        conn.parseEp6FrameForTest(p1StatusWithC1(0xE5));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<quint8>(), quint8(0x02));
    }

    void p1PttBitInC0IsStillCaseZero()
    {
        // C0 bit 0 is PTT (networkproto1.c:329); the switch masks 0xf8.
        P1RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &RadioConnection::userDigitalInputsChanged);
        const quint8 sub0[5] = {0x01, c1ForUserInputs(0x0F), 0x00, 0x00, 0x00};
        const quint8 sub1[5] = {0x08, 0x00, 0x00, 0x00, 0x00};
        conn.parseEp6FrameForTest(makeEp6Frame(sub0, sub1));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<quint8>(), quint8(0x0F));
    }

    void p1OtherStatusTypesCarryNoInputs()
    {
        P1RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &RadioConnection::userDigitalInputsChanged);
        for (const quint8 c0 : {quint8(0x08), quint8(0x10), quint8(0x18), quint8(0x20)}) {
            const quint8 sub[5] = {c0, 0xFF, 0xFF, 0xFF, 0xFF};
            conn.parseEp6FrameForTest(makeEp6Frame(sub, sub));
        }
        QCOMPARE(spy.count(), 0);
    }

    void p1ReportsOnlyChanges()
    {
        P1RadioConnection conn;
        conn.init();
        QSignalSpy spy(&conn, &RadioConnection::userDigitalInputsChanged);
        conn.parseEp6FrameForTest(p1StatusWithC1(c1ForUserInputs(0x01)));
        conn.parseEp6FrameForTest(p1StatusWithC1(c1ForUserInputs(0x01)));
        QCOMPARE(spy.count(), 1);
        conn.parseEp6FrameForTest(p1StatusWithC1(c1ForUserInputs(0x00)));
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(0).value<quint8>(), quint8(0x00));
    }

    // ---- Protocol 2 status parsing ----------------------------------------

    void p2HighPriorityByte59IsTheUserInputs()
    {
        P2RadioConnection conn;
        QSignalSpy spy(&conn, &RadioConnection::userDigitalInputsChanged);
        conn.processHighPriorityStatusForTest(p2StatusWithUserInputs(0x13, 0xAA));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<quint8>(), quint8(0x13));

        // The same value again is not reported; a change is.
        conn.processHighPriorityStatusForTest(p2StatusWithUserInputs(0x13, 0x55));
        QCOMPARE(spy.count(), 1);
        conn.processHighPriorityStatusForTest(p2StatusWithUserInputs(0x10, 0x55));
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(0).value<quint8>(), quint8(0x10));
    }

    // ---- The per-model bit choice -----------------------------------------

    void perModelBitChoice_data()
    {
        QTest::addColumn<int>("model");
        QTest::addColumn<int>("protocol");
        for (int m = static_cast<int>(HPSDRModel::HERMES);
             m < static_cast<int>(HPSDRModel::LAST); ++m) {
            for (const int protocol : {1, 2}) {
                const QString row = QStringLiteral("model %1, P%2").arg(m).arg(protocol);
                QTest::newRow(qPrintable(row)) << m << protocol;
            }
        }
    }

    void perModelBitChoice()
    {
        QFETCH(int, model);
        QFETCH(int, protocol);
        const auto m = static_cast<HPSDRModel>(model);
        const quint8 mask = expectedInputMask(m, protocol);

        // The radio reports the bit set while the input is not asserted:
        // inhibit_input = !getUserIxx().
        QVERIFY(!TxInhibitMonitor::inhibitInputFromUserIo(m, protocol, mask));
        QVERIFY(TxInhibitMonitor::inhibitInputFromUserIo(m, protocol, 0x00));
        // Every other bit is ignored.
        QVERIFY(!TxInhibitMonitor::inhibitInputFromUserIo(m, protocol, 0xFF));
        QVERIFY(TxInhibitMonitor::inhibitInputFromUserIo(
            m, protocol, static_cast<quint8>(0xFF ^ mask)));
    }

    void hermesLite2ReadsTheGenericP1Input()
    {
        // mi0bot-Thetis PollTXInhibit has no HL2 branch.
        QCOMPARE(expectedInputMask(HPSDRModel::HERMESLITE, 1), quint8(0x01));
        QVERIFY(!TxInhibitMonitor::inhibitInputFromUserIo(HPSDRModel::HERMESLITE, 1, 0x01));
        QVERIFY(TxInhibitMonitor::inhibitInputFromUserIo(HPSDRModel::HERMESLITE, 1, 0x02));
    }

    // ---- The monitor reading the radio ------------------------------------

    void monitorReadsTheRadioWithTheReverseOption()
    {
        TxInhibitMonitor mon;
        mon.setEnabled(true);
        mon.attachRadioInput(HPSDRModel::ANAN_G2, 2);
        mon.notifyUserDigitalInputs(0x02);             // I05 set: not asserted
        QVERIFY(!mon.inhibited());
        mon.notifyUserDigitalInputs(0x01);             // I05 clear: asserted
        QVERIFY(mon.inhibited());
        QCOMPARE(mon.lastSource(), TxInhibitMonitor::Source::UserIo01);

        mon.setReverseLogic(true);
        QVERIFY(!mon.inhibited());
        mon.notifyUserDigitalInputs(0x02);
        QVERIFY(mon.inhibited());
    }

    void monitorFollowsAModelChange()
    {
        TxInhibitMonitor mon;
        mon.setEnabled(true);
        mon.attachRadioInput(HPSDRModel::HERMES, 1);
        mon.notifyUserDigitalInputs(0x01);             // I01 set
        QVERIFY(!mon.inhibited());
        mon.setRadioModel(HPSDRModel::ANAN7000D);      // now I02, which is clear
        QVERIFY(mon.inhibited());
    }

    void hpsdrModelNeverReadsTheInput()
    {
        // console.cs:25855: _useTxInhibit && Model != HPSDR, before the
        // reverse is applied, so a reversed HPSDR is not inhibited either.
        TxInhibitMonitor mon;
        mon.setEnabled(true);
        mon.attachRadioInput(HPSDRModel::HPSDR, 1);
        mon.notifyUserDigitalInputs(0x00);
        QVERIFY(!mon.inhibited());
        mon.setReverseLogic(true);
        QVERIFY(!mon.inhibited());
    }

    void switchedOffOrDetachedIsNotInhibited()
    {
        TxInhibitMonitor mon;
        mon.setEnabled(true);
        mon.attachRadioInput(HPSDRModel::HERMES, 1);
        mon.notifyUserDigitalInputs(0x00);
        QVERIFY(mon.inhibited());

        // console.cs:25882-25883: switched off in Setup clears it.
        mon.setEnabled(false);
        QVERIFY(!mon.inhibited());
        mon.setEnabled(true);
        QVERIFY(mon.inhibited());

        // No radio, no input.
        mon.detachRadioInput();
        QVERIFY(!mon.inhibited());
    }

    void inputReadsZeroUntilTheFirstReport()
    {
        // Thetis prn->user_dig_in starts at 0, so !getUserI01() reads
        // asserted until the radio's first status says otherwise.
        TxInhibitMonitor mon;
        mon.setEnabled(true);
        mon.attachRadioInput(HPSDRModel::HERMES, 1);
        QVERIFY(mon.inhibited());
        mon.notifyUserDigitalInputs(0x01);
        QVERIFY(!mon.inhibited());
    }

    // ---- A change reaches the gate within one status frame ----------------

    void p1FrameReachesTheKeyingGateWithinOneFrame()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN8000D);
        P1RadioConnection conn;
        conn.init();
        model.injectConnectionForTest(&conn);
        const auto detach = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });
        model.setUseTxInhibit(true);
        model.wireTxInhibitInputForTest();
        QVERIFY(model.moxController() != nullptr);

        conn.parseEp6FrameForTest(p1StatusWithC1(c1ForUserInputs(0x02)));  // I02 set
        QCoreApplication::processEvents();
        QVERIFY(!model.txInhibit().inhibited());
        QVERIFY(!model.moxController()->isTxInhibited());

        QElapsedTimer clock;
        clock.start();
        conn.parseEp6FrameForTest(p1StatusWithC1(c1ForUserInputs(0x01)));  // I02 clear
        QCoreApplication::processEvents();
        QVERIFY2(clock.elapsed() < 100, "the check ran long enough for the 100 ms poll");
        QVERIFY(model.txInhibit().inhibited());
        QVERIFY(model.moxController()->isTxInhibited());

        conn.parseEp6FrameForTest(p1StatusWithC1(c1ForUserInputs(0x03)));
        QCoreApplication::processEvents();
        QVERIFY(!model.moxController()->isTxInhibited());
    }

    void p2FrameReachesTheKeyingGateWithinOneFrame()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        P2RadioConnection conn;
        model.injectConnectionForTest(&conn);
        const auto detach = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });
        model.setUseTxInhibit(true);
        model.setReverseTxInhibit(true);
        model.wireTxInhibitInputForTest();

        // Reversed: I05 set now means inhibit.
        conn.processHighPriorityStatusForTest(p2StatusWithUserInputs(0x02));
        QCoreApplication::processEvents();
        QVERIFY(model.moxController()->isTxInhibited());
        conn.processHighPriorityStatusForTest(p2StatusWithUserInputs(0x01));
        QCoreApplication::processEvents();
        QVERIFY(!model.moxController()->isTxInhibited());
    }

    void disconnectDetachesTheInput()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::HERMES);
        P1RadioConnection conn;
        conn.init();
        model.injectConnectionForTest(&conn);
        auto detach = qScopeGuard([&model] { model.injectConnectionForTest(nullptr); });
        model.setUseTxInhibit(true);
        model.wireTxInhibitInputForTest();
        conn.parseEp6FrameForTest(p1StatusWithC1(c1ForUserInputs(0x00)));
        QCoreApplication::processEvents();
        QVERIFY(model.txInhibit().inhibited());

        model.teardownTxInhibitInputForTest();
        QVERIFY(!model.txInhibit().inhibited());
    }

    // ---- The Setup checkboxes ---------------------------------------------

    void localCheckboxesReachTheMonitorAndAreSaved()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::HERMES);
        PowerPage page(&model);
        auto* use = page.findChild<QCheckBox*>(QStringLiteral("chkTXInhibit"));
        auto* reverse = page.findChild<QCheckBox*>(QStringLiteral("chkTXInhibitReverse"));
        QVERIFY(use != nullptr && reverse != nullptr);
        QVERIFY(!model.txInhibit().isEnabled());

        use->setChecked(true);
        QVERIFY(model.txInhibit().isEnabled());
        QCOMPARE(AppSettings::instance().value(kEnabledKey).toString(), QStringLiteral("True"));

        reverse->setChecked(true);
        QVERIFY(model.txInhibit().isReverseLogic());
        QCOMPARE(AppSettings::instance().value(kReversedKey).toString(), QStringLiteral("True"));

        use->setChecked(false);
        reverse->setChecked(false);
        QVERIFY(!model.txInhibit().isEnabled());
        QVERIFY(!model.txInhibit().isReverseLogic());
    }

    void remoteCheckboxesReachTheCore()
    {
        QTemporaryDir dir;
        AppSettings coreSettings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel core;
        {
            StationServer server(&core, coreSettings,
                                 NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
            RadioModel window(RadioModel::Role::Remote);
            SettingsProxy proxy;
            StationClient client(&window, &proxy);
            auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
            auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
            stationEnd->linkTo(clientEnd);
            QSignalSpy completed(&client, &StationClient::handshakeComplete);
            client.startSession(clientEnd, server.token());
            server.acceptTransport(stationEnd);
            QVERIFY(completed.wait(5000) || !completed.isEmpty());

            ScopedRemoteBackend backend(&proxy);
            PowerPage page(&window);
            auto* use = page.findChild<QCheckBox*>(QStringLiteral("chkTXInhibit"));
            auto* reverse = page.findChild<QCheckBox*>(QStringLiteral("chkTXInhibitReverse"));
            QVERIFY(use != nullptr && reverse != nullptr);

            use->setChecked(true);
            QTRY_COMPARE(coreSettings.value(kEnabledKey).toString(), QStringLiteral("True"));
            QTRY_VERIFY(core.txInhibit().isEnabled());

            reverse->setChecked(true);
            QTRY_VERIFY(core.txInhibit().isReverseLogic());

            use->setChecked(false);
            QTRY_VERIFY(!core.txInhibit().isEnabled());

            // A settings reset on the Core reads the defaults (off).
            coreSettings.remove(kReversedKey);
            QTRY_VERIFY(!core.txInhibit().isReverseLogic());
        }
    }

    // Checkpoint A: the three pieces joined. Remote-window parity Task 5
    // takes the two External TX Inhibit keys from a window while the radio
    // is off the air, gaps Task 13 applies them to the Core's live monitor,
    // and parity Task 6 mirrors the monitor to the window as txInhibited.
    // All through the real paths: the window's Setup box, the session, the
    // Core's settings, the Core's Protocol 1 status parser, and back.
    void remoteBoxGatesTheCoreAndTheWindowFollowsTheInput()
    {
        QTemporaryDir dir;
        AppSettings coreSettings(dir.filePath(QStringLiteral("station.settings")));
        coreSettings.setValue(QStringLiteral("SettingsSchemaVersion"), QStringLiteral("7"));
        RadioModel core;
        core.setHpsdrModelForTest(HPSDRModel::ANAN8000D);  // P1: inhibit on I02
        P1RadioConnection conn;
        conn.init();
        core.injectConnectionForTest(&conn);
        const auto detach = qScopeGuard([&core] {
            core.teardownTxInhibitInputForTest();
            core.injectConnectionForTest(nullptr);
        });
        core.wireTxInhibitInputForTest();
        const auto feed = [&conn](quint8 userDigIn) {
            conn.parseEp6FrameForTest(p1StatusWithC1(c1ForUserInputs(userDigIn)));
            QCoreApplication::processEvents();
        };
        {
            StationServer server(&core, coreSettings,
                                 NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
            RadioModel window(RadioModel::Role::Remote);
            SettingsProxy proxy;
            StationClient client(&window, &proxy);
            auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
            auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
            stationEnd->linkTo(clientEnd);
            QSignalSpy completed(&client, &StationClient::handshakeComplete);
            client.startSession(clientEnd, server.token());
            server.acceptTransport(stationEnd);
            QVERIFY(completed.wait(5000) || !completed.isEmpty());

            ScopedRemoteBackend backend(&proxy);
            PowerPage page(&window);
            auto* use = page.findChild<QCheckBox*>(QStringLiteral("chkTXInhibit"));
            auto* reverse = page.findChild<QCheckBox*>(QStringLiteral("chkTXInhibitReverse"));
            QVERIFY(use != nullptr && reverse != nullptr);

            // The input reads "inhibit" (I02 clear), but the box is off.
            feed(0x01);
            QVERIFY(!core.isTxInhibited());
            QVERIFY(!window.isTxInhibited());

            // The window's box reaches the Core's monitor; the window then
            // shows the Core's inhibit.
            use->setChecked(true);
            QTRY_COMPARE(coreSettings.value(kEnabledKey).toString(), QStringLiteral("True"));
            QTRY_VERIFY(core.txInhibit().isEnabled());
            QTRY_VERIFY(core.isTxInhibited());
            QTRY_VERIFY(core.moxController()->isTxInhibited());
            QTRY_VERIFY(window.isTxInhibited());

            // The input changes on the radio; the window follows it.
            feed(0x02);
            QTRY_VERIFY(!core.isTxInhibited());
            QTRY_VERIFY(!window.isTxInhibited());
            feed(0x00);
            QTRY_VERIFY(window.isTxInhibited());
            feed(0x02);
            QTRY_VERIFY(!window.isTxInhibited());

            // Reversed logic from the window: I02 set now inhibits.
            reverse->setChecked(true);
            QTRY_VERIFY(core.txInhibit().isReverseLogic());
            QTRY_VERIFY(window.isTxInhibited());

            // Box off from the window: the Core's gate opens, the window follows.
            use->setChecked(false);
            QTRY_VERIFY(!core.txInhibit().isEnabled());
            QTRY_VERIFY(!core.isTxInhibited());
            QTRY_VERIFY(!window.isTxInhibited());
        }
    }
};

QTEST_MAIN(TestTxInhibitInput)
#include "tst_tx_inhibit_input.moc"
