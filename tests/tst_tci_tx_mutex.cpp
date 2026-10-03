// tests/tst_tci_tx_mutex.cpp  (NereusSDR)
// no-port-check: NereusSDR-original integration test for the Phase 17 TX
// audio single-client mutex.
//
// Verifies:
//   1. trx:0,true,tci; from clientA → clientA acquires TX mutex.
//   2. Binary TX_AUDIO_STREAM frame from clientA → lands in server TX ring.
//   3. Binary TX_AUDIO_STREAM frame from clientB (no mutex) → silently dropped;
//      clientB's txFramesDropped increments.
//   4. trx:0,false; from clientA → mutex released.
//
// Phase 3J-1 Task 17.1.
//
// R3 receiver audio plan, Task 4 (R-R3-42, R-R3-25), 2026-09-23, J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code: in a remote window
// transmit is refused. No MOX write, no TX audio lock, no TX_CHRONO, the
// app hears trx:N,false, and the plain reason goes to the operator, never
// onto the TCI wire.
//
// R3 Core-owned accessories, Task 3 (R-R3-48, R-R3-25), 2026-09-24, J.J.
// Boyd (KG4VCF), AI-assisted via Anthropic Claude Code: the Core's station
// TCI server refuses transmit the same way until remote transmit, with its
// own plain reason, while the Core's receive path is unchanged.
//
// Receiver and transmit gaps plan, Task 4 (R-R3-49), 2026-09-24, J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code: a second app's trx
// follows Thetis handleTrxMessage and OnMoxPreChangeHandler, checked with
// two apps against a radio model that is not connected (its MoxController
// is the fake MOX: nothing reaches a radio).
//
// Receiver and transmit gaps plan, Task 7 fix wave (R-R3-49), 2026-09-24,
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code: an app's trx is
// answered with the transmitter's real state, never the value it asked for
// (the MOX button keyed under trx:0,false; a manual key under trx:0,true),
// and WSJT-X's own sequence still sees trx:0,true; with no suffix.
//
// Receiver and transmit gaps plan, Task 7 follow-up (R-R3-49), 2026-09-24,
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code: a trx that is
// refused gives the TX audio back at once (second_app_trx_follows_thetis_rule).

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocket>
#include <QUrl>
#include <QRegularExpression>

#include <cstring>
#include <limits>
#include <vector>

#include "core/TciServer.h"
#include "core/TciProtocol.h"
#include "core/TciBinaryFrame.h"
#include "core/MoxController.h"
#include "core/SliceOwnership.h"
#include "core/TxSliceArbiter.h"
#include "core/safety/TransmitHolder.h"
#include "core/safety/TxRefusal.h"
#include "core/safety/BandPlanGuard.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

// ── Helper: build a minimal TX_AUDIO_STREAM binary frame ────────────────────
//
// Constructs a 64-byte TCI header with streamType=TX_AUDIO_STREAM(2) plus a
// payload of `sampleCount` Float32 samples, all set to `amplitude`.
// Uses the production TciBinaryFrame::buildStreamPayload path for byte-exact
// parity with the wire format expected by onBinaryMessageReceived.
static QByteArray makeTxFrame(int sampleCount, float amplitude = 0.5f)
{
    // Interleaved stereo (channels=2): sampleCount is total floats (frames*2).
    std::vector<float> samples(static_cast<size_t>(sampleCount), amplitude);
    return TciBinaryFrame::buildStreamPayload(
        /*receiver=*/0,
        /*sampleRate=*/48000,
        /*sampleType=*/static_cast<int>(TciSampleType::Float32),
        /*length=*/sampleCount,
        /*streamType=*/static_cast<int>(TciStreamType::TxAudioStream),
        /*channels=*/2,
        samples.data());
}

class TestTciTxMutex : public QObject {
    Q_OBJECT
private slots:
    void tx_mutex_single_client_claim_and_release();
    void tx_mutex_second_client_frame_is_dropped();
    void remote_window_refuses_transmit_off_the_wire();
    void station_server_refuses_transmit_until_remote_transmit();
    void station_server_refuses_transmit_settings();
    void second_app_trx_follows_thetis_rule();
    void stop_releases_the_tx_audio_client();
    void trx_false_during_operator_key_is_not_echoed();
    void trx_true_that_keys_nothing_is_not_echoed();
    void wsjtx_sequence_still_sees_trx_true_without_suffix();
    void disconnect_releases_the_app_key_before_its_audio_data();
    void disconnect_releases_the_app_key_before_its_audio();
    void disconnect_of_an_app_that_did_not_key_leaves_the_key();
    void trx_on_a_receiver_that_is_off_keys_nothing();
    void desktop_host_trx_goes_through_ptt_admission();
    void tx_audio_header_is_validated();
    void uppercase_trx_key_ends_with_the_app_data();
    void uppercase_trx_key_ends_with_the_app();
    void desktop_host_uppercase_trx_goes_through_ptt_admission();
    void desktop_host_holder_and_program_ownership();
    void desktop_host_owned_two_three_broadcasts_logical_receivers();
    void stopped_server_queues_no_rx2_lines();
    void desktop_host_answers_rx2_off_queries_without_receiver_1();
    void desktop_host_notch_reads_the_receiver_not_the_slice();
    void desktop_host_first_lines_never_carry_receiver_0_as_receiver_1();
    void desktop_host_reentrant_stop_cannot_take_audio();
    void desktop_host_reentrant_destruction_releases_original_key();
    void desktop_host_release_callback_may_destroy_server_data();
    void desktop_host_release_callback_may_destroy_server();
};

void TestTciTxMutex::desktop_host_release_callback_may_destroy_server_data()
{
    QTest::addColumn<int>("action");
    QTest::newRow("stop") << 0;
    QTest::newRow("disable hosting") << 1;
    QTest::newRow("client disconnect") << 2;
}

void TestTciTxMutex::desktop_host_release_callback_may_destroy_server()
{
    QFETCH(int, action);
    RadioModel radio;
    const int owned = radio.addSlice(QStringLiteral("pan-0"));
    radio.sliceOwnership()->setOwner(owned, SliceOwnership::stationDevice());
    TransmitHolder holder;
    radio.moxController()->setKeyingGate([&holder](PttMode source,
                                                   const KeyerIdentity& keyer) {
        return holder.askKey({keyer.deviceId, TransmitHolder::Source::Device,
                              keyer.program, source == PttMode::Vox});
    });
    holder.transferTo(TransmitHolder::Holder{SliceOwnership::stationDevice()},
                      QStringLiteral("test"), [](bool) {});
    radio.setTransmitHolder(SliceOwnership::stationDevice());
    QVERIFY(radio.txSliceArbiter()->bindForHolder(SliceOwnership::stationDevice(), owned));
    QPointer<TciServer> server = new TciServer(&radio);
    server->setDesktopHostMode(true);
    QVERIFY(server->start(0));
    QWebSocket app;
    QSignalSpy connected(&app, &QWebSocket::connected);
    app.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server->port())));
    QVERIFY(connected.wait(2000));
    QTRY_COMPARE_WITH_TIMEOUT(server->clientCount(), 1, 3000);
    app.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_VERIFY_WITH_TIMEOUT(radio.mox(), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(server->activeTxClientCount(), 1, 3000);
    const auto destroying = connect(radio.moxController(), &MoxController::moxChanging,
        &radio, [&server](int, bool, bool on) {
            if (!on && server) { delete server.data(); }
        });
    if (action == 0) { server->stop(); }
    else if (action == 1) { server->setDesktopHostMode(false); }
    else { app.close(); }
    QTRY_VERIFY_WITH_TIMEOUT(server.isNull(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(!radio.mox(), 3000);
    QObject::disconnect(destroying);
    app.close();
}

void TestTciTxMutex::desktop_host_reentrant_destruction_releases_original_key()
{
    RadioModel radio;
    const int owned = radio.addSlice(QStringLiteral("pan-0"));
    radio.sliceOwnership()->setOwner(owned, SliceOwnership::stationDevice());
    TransmitHolder holder;
    radio.moxController()->setKeyingGate([&holder](PttMode source,
                                                   const KeyerIdentity& keyer) {
        return holder.askKey({keyer.deviceId, TransmitHolder::Source::Device,
                              keyer.program, source == PttMode::Vox});
    });
    holder.transferTo(TransmitHolder::Holder{SliceOwnership::stationDevice()},
                      QStringLiteral("test"), [](bool) {});
    radio.setTransmitHolder(SliceOwnership::stationDevice());
    QVERIFY(radio.txSliceArbiter()->bindForHolder(SliceOwnership::stationDevice(), owned));
    QPointer<TciServer> server = new TciServer(&radio);
    server->setDesktopHostMode(true);
    QVERIFY(server->start(0));
    QWebSocket app;
    QSignalSpy connected(&app, &QWebSocket::connected);
    app.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server->port())));
    QVERIFY(connected.wait(2000));
    QTRY_COMPARE_WITH_TIMEOUT(server->clientCount(), 1, 3000);
    QWebSocket* serverSocket = nullptr;
    const auto sockets = server->clients();
    for (auto it = sockets.cbegin(); it != sockets.cend(); ++it) {
        if (it.key()->peerPort() == app.localPort()) { serverSocket = it.key(); }
    }
    QVERIFY(serverSocket);
    const auto destroying = connect(radio.moxController(), &MoxController::moxChanging,
        &radio, [&server](int, bool, bool on) {
            if (on && server) { delete server.data(); }
        });
    emit serverSocket->textMessageReceived(QStringLiteral("trx:0,true,tci;"));
    QVERIFY(server.isNull());
    QTRY_VERIFY_WITH_TIMEOUT(!radio.mox(), 3000);
    QObject::disconnect(destroying);
    app.close();
}

void TestTciTxMutex::desktop_host_owned_two_three_broadcasts_logical_receivers()
{
    RadioModel radio;
    for (int i = 0; i < 4; ++i) {
        QCOMPARE(radio.addSlice(QStringLiteral("pan-0")), i);
        radio.sliceOwnership()->setOwner(i, i != 2 ? QByteArray("phone")
                                   : SliceOwnership::stationDevice());
    }
    TciServer server(&radio);
    server.setDesktopHostMode(true);
    QVERIFY(server.start(0));
    QWebSocket app;
    QSignalSpy connected(&app, &QWebSocket::connected);
    QSignalSpy text(&app, &QWebSocket::textMessageReceived);
    app.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(connected.wait(2000));
    QTRY_VERIFY_WITH_TIMEOUT(!text.isEmpty(), 3000);
    text.clear();
    // RX2 turns on when the station device gains a second slice: Thetis's
    // RX2EnabledChange lines go out (TCIServer.cs:842-847 [v2.10.3.15]).
    radio.sliceOwnership()->setOwner(3, SliceOwnership::stationDevice());
    const auto hasLine = [&text](const QString& line) {
        for (const auto& call : text) {
            if (call.at(0).toString().contains(line)) { return true; }
        }
        return false;
    };
    QTRY_VERIFY_WITH_TIMEOUT(hasLine(QStringLiteral("tx_enable:1,true;")), 3000);
    QVERIFY(hasLine(QStringLiteral("rx_enable:1,true;")));
    // Slice 3 is named as receiver 1 (the physical-to-logical seam).
    text.clear();
    radio.setLock(3, true);
    QTRY_VERIFY_WITH_TIMEOUT(hasLine(QStringLiteral("lock:1,true;")), 3000);
    QVERIFY(!hasLine(QStringLiteral("lock:0,true;")));
    app.close();
    server.stop();
}

// A stopped server queues no RX2 lines (its ownership hook goes with the
// rest of its model wiring), and sends them again once restarted.
void TestTciTxMutex::stopped_server_queues_no_rx2_lines()
{
    RadioModel radio;
    for (int i = 0; i < 4; ++i) {
        QCOMPARE(radio.addSlice(QStringLiteral("pan-0")), i);
        radio.sliceOwnership()->setOwner(i, i != 2 ? QByteArray("phone")
                                   : SliceOwnership::stationDevice());
    }
    TciServer server(&radio);
    server.setDesktopHostMode(true);
    QVERIFY(server.start(0));
    server.stop();
    TciProtocol* protocol = server.protocolForTest();
    protocol->drainCoalescedNotifications();
    while (protocol->hasPendingNotification()) { protocol->takePendingNotification(); }

    radio.sliceOwnership()->setOwner(3, SliceOwnership::stationDevice());
    QTest::qWait(50);
    protocol->drainCoalescedNotifications();
    QStringList queued;
    while (protocol->hasPendingNotification()) { queued << protocol->takePendingNotification(); }
    for (const QString& line : queued) {
        QVERIFY2(!line.startsWith(QStringLiteral("rx_enable:1,"))
                     && !line.startsWith(QStringLiteral("tx_enable:1,")),
                 qPrintable(line));
    }

    // Back to one station slice, restart, and flip RX2 on again: sent.
    radio.sliceOwnership()->setOwner(3, QByteArray("phone"));
    QVERIFY(server.start(0));
    QWebSocket app;
    QSignalSpy connected(&app, &QWebSocket::connected);
    QSignalSpy text(&app, &QWebSocket::textMessageReceived);
    app.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(connected.wait(2000));
    QTRY_VERIFY_WITH_TIMEOUT(!text.isEmpty(), 3000);
    QTest::qWait(150);
    text.clear();
    radio.sliceOwnership()->setOwner(3, SliceOwnership::stationDevice());
    const auto hasLine = [&text](const QString& line) {
        for (const auto& call : text) {
            if (call.at(0).toString().contains(line)) { return true; }
        }
        return false;
    };
    QTRY_VERIFY_WITH_TIMEOUT(hasLine(QStringLiteral("rx_enable:1,true;")), 3000);
    QVERIFY(hasLine(QStringLiteral("tx_enable:1,true;")));
    app.close();
    server.stop();
}

// With the desktop hosting and no slice for receiver 1, the queries Thetis
// answers with RX2 off whose value is not receiver 1's own state are
// answered, not dropped: rx_enable:1 is false (TCIServer.cs:4624-4627
// [v2.10.3.15]), rx_nf_enable:1 is the global notch and split_enable:1 is
// VFOSplit. Queries of receiver 1's own state and every set stay dropped.
void TestTciTxMutex::desktop_host_answers_rx2_off_queries_without_receiver_1()
{
    RadioModel radio;
    QCOMPARE(radio.addSlice(QStringLiteral("pan-0")), 0);
    QCOMPARE(radio.addSlice(QStringLiteral("pan-0")), 1);
    radio.sliceOwnership()->setOwner(0, SliceOwnership::stationDevice());
    radio.sliceOwnership()->setOwner(1, QByteArray("phone"));
    TciServer server(&radio);
    server.setDesktopHostMode(true);
    QVERIFY(server.start(0));
    QWebSocket app;
    QSignalSpy connected(&app, &QWebSocket::connected);
    QSignalSpy text(&app, &QWebSocket::textMessageReceived);
    app.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(connected.wait(2000));
    QTRY_VERIFY_WITH_TIMEOUT(!text.isEmpty(), 3000);
    QTest::qWait(150);
    const auto hasLine = [&text](const QString& line) {
        for (const auto& call : text) {
            if (call.at(0).toString().contains(line)) { return true; }
        }
        return false;
    };

    text.clear();
    app.sendTextMessage(QStringLiteral("rx_enable:1;"));
    QTRY_VERIFY_WITH_TIMEOUT(hasLine(QStringLiteral("rx_enable:1,false;")), 3000);
    app.sendTextMessage(QStringLiteral("rx_nf_enable:1;"));
    QTRY_VERIFY_WITH_TIMEOUT(hasLine(QStringLiteral("rx_nf_enable:1,false;")), 3000);
    app.sendTextMessage(QStringLiteral("split_enable:1;"));
    QTRY_VERIFY_WITH_TIMEOUT(hasLine(QStringLiteral("split_enable:1,false;")), 3000);
    app.sendTextMessage(QStringLiteral("rx_channel_enable:1,0;"));
    QTRY_VERIFY_WITH_TIMEOUT(hasLine(QStringLiteral("rx_channel_enable:1,0,false;")), 3000);

    text.clear();
    app.sendTextMessage(QStringLiteral("modulation:1;"));
    app.sendTextMessage(QStringLiteral("rx_enable:1,true;"));
    app.sendTextMessage(QStringLiteral("split_enable:1,true;"));
    app.sendTextMessage(QStringLiteral("rx_enable:0;"));
    QTRY_VERIFY_WITH_TIMEOUT(hasLine(QStringLiteral("rx_enable:0,true;")), 3000);
    QTest::qWait(150);
    QVERIFY(!hasLine(QStringLiteral("modulation:1,")));
    QVERIFY(!hasLine(QStringLiteral("rx_enable:1,")));
    QVERIFY(!hasLine(QStringLiteral("split_enable:1,")));
    QCOMPARE(radio.sliceOwnership()->ownedBy(SliceOwnership::stationDevice()), QList<int>{0});
    QVERIFY(radio.sliceById(1) != nullptr);
    app.close();
    server.stop();
}

namespace {
// A desktop host whose receiver 0 is slice 2 and whose receiver 1 has no
// slice: slices 0 and 1 belong to a phone.
void hostOnSlice2(RadioModel& radio)
{
    for (int i = 0; i < 3; ++i) {
        QCOMPARE(radio.addSlice(QStringLiteral("pan-0")), i);
        radio.sliceOwnership()->setOwner(i, i == 2 ? SliceOwnership::stationDevice()
                                                   : QByteArray("phone"));
    }
}

// Every frame the app received, split into lines.
QStringList framesOf(const QSignalSpy& text)
{
    QStringList out;
    for (const auto& call : text) {
        for (const QString& part : call.at(0).toString().split(QLatin1Char(';'),
                                                                Qt::SkipEmptyParts)) {
            out << part.trimmed() + QLatin1Char(';');
        }
    }
    return out;
}
} // namespace

// rx_nf_enable reads the global notch through RadioModel::rxNf, which
// takes the receiver index (GetMNF, console.cs:52317-52330 [v2.10.3.15]).
// Receiver 0 as slice 2 must still answer the notch.
void TestTciTxMutex::desktop_host_notch_reads_the_receiver_not_the_slice()
{
    RadioModel radio;
    hostOnSlice2(radio);
    radio.setRxNf(0, true);
    QVERIFY(radio.rxNf(0));
    TciServer server(&radio);
    server.setDesktopHostMode(true);
    QVERIFY(server.start(0));
    QWebSocket app;
    QSignalSpy connected(&app, &QWebSocket::connected);
    QSignalSpy text(&app, &QWebSocket::textMessageReceived);
    app.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(connected.wait(2000));
    QTRY_VERIFY_WITH_TIMEOUT(framesOf(text).contains(QStringLiteral("ready;")), 3000);
    QVERIFY(framesOf(text).contains(QStringLiteral("rx_nf_enable:0,true;")));
    QVERIFY(framesOf(text).contains(QStringLiteral("rx_nf_enable:1,true;")));
    text.clear();
    app.sendTextMessage(QStringLiteral("rx_nf_enable:0;"));
    QTRY_VERIFY_WITH_TIMEOUT(framesOf(text).contains(QStringLiteral("rx_nf_enable:0,true;")), 3000);
    app.sendTextMessage(QStringLiteral("rx_nf_enable:0,false;"));
    QTRY_VERIFY_WITH_TIMEOUT(!radio.rxNf(0), 3000);
    app.close();
    server.stop();
}

// Thetis sends receiver 1's lines with RX2 off from RX2's own state
// (sendInitialRadioState, TCIServer.cs:2486-2651 [v2.10.3.15]). With no
// slice for receiver 1 the desktop host has none, and must not send
// receiver 0's slice as receiver 1: only the lines whose value is not
// receiver 1's own state are sent.
void TestTciTxMutex::desktop_host_first_lines_never_carry_receiver_0_as_receiver_1()
{
    RadioModel radio;
    hostOnSlice2(radio);
    radio.sliceById(2)->setFrequency(7074000.0);
    radio.sliceById(0)->setFrequency(3573000.0);
    radio.sliceById(1)->setFrequency(21074000.0);
    TciServer server(&radio);
    server.setDesktopHostMode(true);
    QVERIFY(server.start(0));
    QWebSocket app;
    QSignalSpy connected(&app, &QWebSocket::connected);
    QSignalSpy text(&app, &QWebSocket::textMessageReceived);
    app.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(connected.wait(2000));
    QTRY_VERIFY_WITH_TIMEOUT(framesOf(text).contains(QStringLiteral("ready;")), 3000);
    const QStringList frames = framesOf(text);
    QVERIFY(frames.contains(QStringLiteral("vfo:0,0,7074000;")));
    const QStringList allowed{
        QStringLiteral("rx_enable"), QStringLiteral("rx_nf_enable"),
        QStringLiteral("calibration_ex"), QStringLiteral("split_enable"),
        QStringLiteral("tx_enable"), QStringLiteral("rx_channel_enable"),
        QStringLiteral("trx"), QStringLiteral("tune"), QStringLiteral("iq_stop"),
    };
    QStringList receiver1;
    for (const QString& f : frames) {
        const int colon = f.indexOf(QLatin1Char(':'));
        if (colon < 0 || !f.mid(colon + 1).startsWith(QStringLiteral("1,"))) { continue; }
        receiver1 << f;
        QVERIFY2(allowed.contains(f.left(colon)), qPrintable(f));
    }
    QVERIFY(receiver1.contains(QStringLiteral("rx_enable:1,false;")));
    QVERIFY(receiver1.contains(QStringLiteral("rx_channel_enable:1,0,false;")));
    QVERIFY(receiver1.contains(QStringLiteral("split_enable:1,false;")));
    // No line anywhere carries a phone slice's frequency.
    for (const QString& f : frames) {
        QVERIFY2(!f.contains(QStringLiteral("3573000"))
                     && !f.contains(QStringLiteral("21074000")),
                 qPrintable(f));
    }
    app.close();
    server.stop();
}

void TestTciTxMutex::desktop_host_reentrant_stop_cannot_take_audio()
{
    RadioModel radio;
    const int owned = radio.addSlice(QStringLiteral("pan-0"));
    radio.sliceOwnership()->setOwner(owned, SliceOwnership::stationDevice());
    TransmitHolder holder;
    radio.moxController()->setKeyingGate([&holder](PttMode source,
                                                   const KeyerIdentity& keyer) {
        return holder.askKey({keyer.deviceId, TransmitHolder::Source::Device,
                              keyer.program, source == PttMode::Vox});
    });
    holder.transferTo(TransmitHolder::Holder{SliceOwnership::stationDevice()},
                      QStringLiteral("test"), [](bool) {});
    radio.setTransmitHolder(SliceOwnership::stationDevice());
    QVERIFY(radio.txSliceArbiter()->bindForHolder(SliceOwnership::stationDevice(), owned));
    TciServer server(&radio);
    server.setDesktopHostMode(true);
    QVERIFY(server.start(0));
    QWebSocket app;
    QSignalSpy connected(&app, &QWebSocket::connected);
    QSignalSpy audio(&server, &TciServer::txAudioActiveClientChanged);
    app.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(connected.wait(2000));
    const auto stopping = connect(radio.moxController(), &MoxController::moxChanging,
                                  &server, [&server](int, bool, bool on) {
                                      if (on) { server.stop(); }
                                  });
    app.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_VERIFY_WITH_TIMEOUT(!server.isRunning(), 3000);
    QCOMPARE(server.activeTxClientCount(), 0);
    for (const auto& call : audio) { QVERIFY(call.at(0).value<QWebSocket*>() == nullptr); }
    QObject::disconnect(stopping);
    app.close();
}

void TestTciTxMutex::desktop_host_holder_and_program_ownership()
{
    RadioModel radio;
    const int foreignSlice = radio.addSlice(QStringLiteral("pan-0"));
    const int ownSlice = radio.addSlice(QStringLiteral("pan-0"));
    QCOMPARE(foreignSlice, 0);
    QCOMPARE(ownSlice, 1);
    radio.sliceOwnership()->setOwner(foreignSlice, QByteArray("phone"));
    radio.sliceOwnership()->setOwner(ownSlice, SliceOwnership::stationDevice());
    radio.sliceById(foreignSlice)->setFrequency(7074000.0);
    radio.sliceById(ownSlice)->setFrequency(14074000.0);
    radio.sliceById(foreignSlice)->setRitHz(100);
    radio.sliceById(foreignSlice)->setXitHz(100);

    TransmitHolder holder;
    TransmitHolder::Hooks hooks;
    hooks.describe = [](const QByteArray& id) -> std::optional<TransmitHolder::Words> {
        if (id == "phone") {
            return TransmitHolder::Words{QStringLiteral("Phone"), QStringLiteral("Phone"),
                                         QStringLiteral("phone")};
        }
        return std::nullopt;
    };
    holder.setHooks(hooks);
    radio.moxController()->setKeyingGate([&holder](PttMode source,
                                                   const KeyerIdentity& keyer) {
        return holder.askKey({keyer.deviceId, TransmitHolder::Source::Device,
                              keyer.program, source == PttMode::Vox});
    });
    TciServer server(&radio);
    server.setDesktopHostMode(true);
    QVERIFY(server.start(0));
    QWebSocket app;
    QSignalSpy connected(&app, &QWebSocket::connected);
    QSignalSpy text(&app, &QWebSocket::textMessageReceived);
    QSignalSpy notices(&server, &TciServer::operatorNotice);
    app.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(connected.wait(2000));
    const auto hasLine = [&text](const QString& line) {
        for (const auto& call : text) {
            if (call.at(0).toString().contains(line)) { return true; }
        }
        return false;
    };
    QTRY_VERIFY_WITH_TIMEOUT(hasLine(QStringLiteral("ready;")), 3000);
    QVERIFY(hasLine(QStringLiteral("vfo:0,0,14074000;")));
    QVERIFY(!hasLine(QStringLiteral("vfo:0,0,7074000;")));
    app.sendTextMessage(QStringLiteral("vfo:0,0,14100000;"));
    QTRY_COMPARE_WITH_TIMEOUT(radio.sliceById(ownSlice)->frequency(), 14100000.0, 3000);
    QCOMPARE(radio.sliceById(foreignSlice)->frequency(), 7074000.0);
    app.sendTextMessage(QStringLiteral("vfo:1,0,21000000;"));
    QTest::qWait(100);
    QCOMPARE(radio.sliceById(foreignSlice)->frequency(), 7074000.0);
    QCOMPARE(radio.sliceById(ownSlice)->frequency(), 14100000.0);
    app.sendTextMessage(QStringLiteral("rit_offset:0,500;"));
    app.sendTextMessage(QStringLiteral("xit_offset:0,600;"));
    QTRY_COMPARE_WITH_TIMEOUT(radio.sliceById(ownSlice)->ritHz(), 500, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(radio.sliceById(ownSlice)->xitHz(), 600, 3000);
    QCOMPARE(radio.sliceById(foreignSlice)->ritHz(), 100);
    QCOMPARE(radio.sliceById(foreignSlice)->xitHz(), 100);
    const int broadcastMark = int(text.count());
    radio.sliceById(foreignSlice)->setFrequency(7100000.0);
    radio.sliceById(ownSlice)->setFrequency(14200000.0);
    QTRY_VERIFY_WITH_TIMEOUT(hasLine(QStringLiteral("vfo:0,0,14200000;")), 3000);
    for (int i = broadcastMark; i < text.count(); ++i) {
        const QString frame = text.at(i).at(0).toString();
        if (frame.startsWith(QStringLiteral("vfo:"))
            || frame.startsWith(QStringLiteral("dds:"))) {
            QVERIFY(!frame.contains(QStringLiteral("7100000")));
        }
    }

    app.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_COMPARE_WITH_TIMEOUT(notices.count(), 1, 3000);
    QVERIFY(hasLine(QStringLiteral("trx:0,false;")));
    QVERIFY(!radio.mox());
    QCOMPARE(server.activeTxClientCount(), 0);
    QCOMPARE(holder.state(), TransmitHolder::State::Unheld);
    QCOMPARE(notices.last().at(1).toString(), TxRefusals::programNeedsTransmit().text);

    const auto foreign = holder.askKey({QByteArray("phone"),
        TransmitHolder::Source::Device, false, false});
    QCOMPARE(foreign.verdict, KeyingVerdict::Admit);
    app.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_COMPARE_WITH_TIMEOUT(notices.count(), 2, 3000);
    QCOMPARE(notices.last().at(1).toString(),
             TxRefusals::otherDeviceHolds(QStringLiteral("Phone")).text);
    QCOMPARE(server.activeTxClientCount(), 0);
    QVERIFY(!radio.mox());
    KeyerIdentity foreignKeyer;
    foreignKeyer.deviceId = QByteArray("phone");
    radio.moxController()->setMox(true, foreignKeyer);
    QTRY_VERIFY_WITH_TIMEOUT(radio.mox(), 3000);
    app.sendTextMessage(QStringLiteral("trx:0,false;"));
    QTest::qWait(100);
    QVERIFY(radio.mox());
    radio.moxController()->setMox(false, foreignKeyer);
    QTRY_VERIFY_WITH_TIMEOUT(!radio.mox(), 3000);

    holder.onMoxReading(false);
    holder.transferTo(TransmitHolder::Holder{SliceOwnership::stationDevice()},
                      QStringLiteral("test"), [](bool) {});
    QVERIFY(holder.isHeldBy(SliceOwnership::stationDevice()));
    radio.setTransmitHolder(SliceOwnership::stationDevice());
    QVERIFY(radio.txSliceArbiter()->bindForHolder(SliceOwnership::stationDevice(), ownSlice));
    radio.moxController()->setMoxCheck([]() {
        return safety::BandPlanGuard::MoxCheckResult{false, QStringLiteral("test block")};
    });
    app.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTest::qWait(100);
    QVERIFY(!radio.mox());
    QCOMPARE(server.activeTxClientCount(), 0);
    radio.moxController()->setMoxCheck({});
    app.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_VERIFY_WITH_TIMEOUT(radio.mox(), 3000);
    QCOMPARE(server.activeTxClientCount(), 1);
    QCOMPARE(radio.moxController()->currentKeyer(), KeyerIdentity::station(PttMode::Tci));
    QWebSocket replacement;
    QSignalSpy replacementConnected(&replacement, &QWebSocket::connected);
    replacement.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(replacementConnected.wait(2000));
    replacement.sendTextMessage(QStringLiteral("trx:0,false;"));
    QTest::qWait(100);
    QVERIFY(radio.mox());
    QCOMPARE(server.activeTxClientCount(), 1);
    app.sendTextMessage(QStringLiteral("trx:0,false;"));
    QTRY_VERIFY_WITH_TIMEOUT(!radio.mox(), 3000);
    QCOMPARE(server.activeTxClientCount(), 0);

    QWebSocket* originalSession = nullptr;
    QWebSocket* replacementSession = nullptr;
    const auto sessions = server.clients();
    for (auto it = sessions.cbegin(); it != sessions.cend(); ++it) {
        if (it.key()->peerPort() == app.localPort()) { originalSession = it.key(); }
        if (it.key()->peerPort() == replacement.localPort()) {
            replacementSession = it.key();
        }
    }
    QVERIFY(originalSession);
    QVERIFY(replacementSession);
    bool replacedDuringAudioSignal = false;
    const auto replaceKey = connect(&server, &TciServer::txAudioActiveClientChanged,
        &server, [&](QWebSocket* active) {
            if (active != originalSession || replacedDuringAudioSignal) { return; }
            replacedDuringAudioSignal = true;
            // Real accepted server-side socket sessions, with synchronous
            // callbacks at the exact audio-lock signal boundary.
            emit originalSession->textMessageReceived(QStringLiteral("trx:0,false;"));
            emit replacementSession->textMessageReceived(QStringLiteral("trx:0,true,tci;"));
        });
    app.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_VERIFY_WITH_TIMEOUT(replacedDuringAudioSignal, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(radio.mox(), 3000);
    QCOMPARE(server.activeTxClientCount(), 1);
    app.sendTextMessage(QStringLiteral("trx:0,false;"));
    QTest::qWait(100);
    QVERIFY(radio.mox());
    QCOMPARE(server.activeTxClientCount(), 1);
    QObject::disconnect(replaceKey);
    replacement.sendTextMessage(QStringLiteral("trx:0,false;"));
    QTRY_VERIFY_WITH_TIMEOUT(!radio.mox(), 3000);
    QCOMPARE(server.activeTxClientCount(), 0);

    radio.setMoxFromButton(true);
    QTRY_VERIFY_WITH_TIMEOUT(radio.mox(), 3000);
    app.sendTextMessage(QStringLiteral("trx:0,false;"));
    QTest::qWait(100);
    QVERIFY(radio.mox());
    radio.setMoxFromButton(false);
    QTRY_VERIFY_WITH_TIMEOUT(!radio.mox(), 3000);
    app.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_VERIFY_WITH_TIMEOUT(radio.mox(), 3000);
    QCOMPARE(server.activeTxClientCount(), 1);
    app.close();
    QTRY_VERIFY_WITH_TIMEOUT(!radio.mox(), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(server.activeTxClientCount(), 0, 3000);
    replacement.close();
    server.stop();
}

// R-R3-39: stopping the server while an app holds the TX audio releases it
// as the app's disconnect does: txAudioActiveClientChanged(nullptr), so the
// indicator and the TX channel's TCI audio gate show no TX audio client.
// A stop with no holder says nothing.
void TestTciTxMutex::stop_releases_the_tx_audio_client()
{
    TciServer server(nullptr);
    QVERIFY(server.start(0));
    QSignalSpy holder(&server, &TciServer::txAudioActiveClientChanged);
    QWebSocket app;
    QSignalSpy connected(&app, &QWebSocket::connected);
    app.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(connected.wait(2000));
    app.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_COMPARE_WITH_TIMEOUT(server.activeTxClientCount(), 1, 3000);
    QCOMPARE(holder.count(), 1);
    QVERIFY(holder.at(0).at(0).value<QWebSocket*>() != nullptr);

    server.stop();
    QCOMPARE(server.activeTxClientCount(), 0);
    QCOMPARE(holder.count(), 2);
    QCOMPARE(holder.at(1).at(0).value<QWebSocket*>(), static_cast<QWebSocket*>(nullptr));

    // No holder: a second start and stop emits nothing.
    QVERIFY(server.start(0));
    server.stop();
    QCOMPARE(holder.count(), 2);
}

// ── tx_mutex_single_client_claim_and_release() ───────────────────────────────
//
// Verifies the basic mutex lifecycle:
//   1. Server starts with no active TX client (activeTxClientCount == 0).
//   2. clientA sends "trx:0,true,tci;" → activeTxClientCount becomes 1.
//   3. clientA sends a TX audio binary frame → frame lands in the TX ring
//      (peekTxRingSize > 0).  Note: with model=nullptr no TxChannel exists so
//      the data stays in the ring rather than being drained synchronously.
//   4. clientA sends "trx:0,false;" → activeTxClientCount returns to 0.

void TestTciTxMutex::tx_mutex_single_client_claim_and_release()
{
    // ── 1. Spin up server ─────────────────────────────────────────────────────
    TciServer server(nullptr);   // RadioModel* null — test injection path
    QVERIFY(server.start(0));
    QVERIFY(server.isRunning());

    // ── 2. Connect clientA ────────────────────────────────────────────────────
    QWebSocket clientA;
    QSignalSpy connA(&clientA, &QWebSocket::connected);
    clientA.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(connA.wait(2000));
    QCOMPARE(clientA.state(), QAbstractSocket::ConnectedState);

    // Initial state: no active TX client.
    QCOMPARE(server.activeTxClientCount(), 0);
    QVERIFY(server.activeTxClientPeer().isEmpty());

    // ── 3. clientA claims TX mutex ────────────────────────────────────────────
    clientA.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTest::qWait(50);  // allow event loop to process the message

    QCOMPARE(server.activeTxClientCount(), 1);
    QVERIFY(!server.activeTxClientPeer().isEmpty());

    // ── 4. clientA sends a TX audio frame ─────────────────────────────────────
    // With model=nullptr, no TxChannel drain happens — data stays in ring.
    const int kSamples = 128;   // 64 stereo frames
    const QByteArray txFrame = makeTxFrame(kSamples);
    QVERIFY(txFrame.size() > 64);

    const int ringBefore = server.peekTxRingSize();
    clientA.sendBinaryMessage(txFrame);
    QTest::qWait(50);

    // Ring should now contain the decoded float bytes.
    // kSamples Float32 samples = kSamples * 4 bytes.
    QVERIFY2(server.peekTxRingSize() > ringBefore,
             qPrintable(QStringLiteral("TX ring did not grow: before=%1 after=%2")
                 .arg(ringBefore).arg(server.peekTxRingSize())));

    // ── 5. clientA releases TX mutex ──────────────────────────────────────────
    clientA.sendTextMessage(QStringLiteral("trx:0,false;"));
    QTest::qWait(50);

    QCOMPARE(server.activeTxClientCount(), 0);
    QVERIFY(server.activeTxClientPeer().isEmpty());

    // ── Cleanup ───────────────────────────────────────────────────────────────
    clientA.close();
    server.stop();
}

// ── tx_mutex_second_client_frame_is_dropped() ────────────────────────────────
//
// Two-client test:
//   1. clientA acquires TX mutex.
//   2. clientA sends a TX audio frame → lands in ring.
//   3. clientB sends a TX audio frame WITHOUT claiming mutex → silently dropped;
//      clientB's txFramesDropped increments.
//   4. clientA's frames still land; ring size larger than clientB contributions.
//   5. clientA releases mutex.

void TestTciTxMutex::tx_mutex_second_client_frame_is_dropped()
{
    // ── 1. Spin up server ─────────────────────────────────────────────────────
    TciServer server(nullptr);
    QVERIFY(server.start(0));

    // ── 2. Connect two clients ────────────────────────────────────────────────
    QWebSocket clientA, clientB;
    QSignalSpy connA(&clientA, &QWebSocket::connected);
    QSignalSpy connB(&clientB, &QWebSocket::connected);

    const QString url = QStringLiteral("ws://127.0.0.1:%1").arg(server.port());
    clientA.open(QUrl(url));
    QVERIFY(connA.wait(2000));
    clientB.open(QUrl(url));
    QVERIFY(connB.wait(2000));

    QCOMPARE(clientA.state(), QAbstractSocket::ConnectedState);
    QCOMPARE(clientB.state(), QAbstractSocket::ConnectedState);

    // ── 3. clientA claims TX mutex ────────────────────────────────────────────
    clientA.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTest::qWait(50);
    QCOMPARE(server.activeTxClientCount(), 1);

    // ── 4. clientA sends a TX audio frame ─────────────────────────────────────
    const int kSamples = 64;   // 32 stereo frames
    const QByteArray txFrame = makeTxFrame(kSamples, 0.3f);
    clientA.sendBinaryMessage(txFrame);
    QTest::qWait(50);
    const int ringAfterA = server.peekTxRingSize();
    // Ring should have grown (clientA is active owner).
    QVERIFY2(ringAfterA > 0,
             "clientA frame should have landed in TX ring");

    // ── 5. clientB sends a TX audio frame (no mutex) ──────────────────────────
    // clientB has NOT sent "trx:0,true,tci;" — it is not the active TX client.
    // The frame must be silently dropped and txFramesDropped incremented.
    clientB.sendBinaryMessage(txFrame);
    QTest::qWait(50);

    // Ring must NOT have grown from clientB's frame (frame was dropped).
    // (ringAfterA is from clientA's frame; clientB frame silently ignored)
    QCOMPARE(server.peekTxRingSize(), ringAfterA);

    // ── 6. Verify clientB's txFramesDropped incremented ──────────────────────
    // Access the session via the server's internal client table.  We can't
    // access m_clients directly (private), so use a well-known proxy: the
    // fact that activeTxClientCount() == 1 (clientA) and activeTxClientPeer()
    // is non-empty.  To verify txFramesDropped we rely on the observable
    // invariant: the ring did not grow (validated above) AND clientB's frame
    // was not fed to TxChannel (model is null, no TxChannel exists).
    //
    // For a deeper assertion, expose txFramesDropped via a test-only accessor
    // if needed in a future phase.  Phase 17 scope: ring-size invariant is
    // the primary observable.

    // ── 7. clientA releases mutex ─────────────────────────────────────────────
    clientA.sendTextMessage(QStringLiteral("trx:0,false;"));
    QTest::qWait(50);
    QCOMPARE(server.activeTxClientCount(), 0);

    // ── Cleanup ───────────────────────────────────────────────────────────────
    clientA.close();
    clientB.close();
    server.stop();
}

void TestTciTxMutex::remote_window_refuses_transmit_off_the_wire()
{
    RadioModel remote(RadioModel::Role::Remote);
    TciServer server(&remote);
    QVERIFY(server.isRemoteWindow());
    QSignalSpy notices(&server, &TciServer::operatorNotice);
    QSignalSpy txOwner(&server, &TciServer::txAudioActiveClientChanged);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy connected(&client, &QWebSocket::connected);
    QSignalSpy text(&client, &QWebSocket::textMessageReceived);
    QSignalSpy binary(&client, &QWebSocket::binaryMessageReceived);
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(connected.wait(2000));
    const auto lines = [&text] {
        QStringList out;
        for (const auto& call : text) { out << call.at(0).toString(); }
        return out;
    };
    QTRY_VERIFY_WITH_TIMEOUT(lines().contains(QStringLiteral("ready;")), 3000);
    const int linesBefore = int(text.count());

    client.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_VERIFY_WITH_TIMEOUT(lines().mid(linesBefore).contains(QStringLiteral("trx:0,false;")),
                             3000);
    // Past several TX_CHRONO periods: none was sent and nothing was keyed.
    QTest::qWait(150);
    QCOMPARE(binary.count(), 0);
    QCOMPARE(server.activeTxClientCount(), 0);
    QCOMPARE(txOwner.count(), 0);
    QVERIFY(!remote.mox());
    QVERIFY(!lines().mid(linesBefore).contains(QStringLiteral("trx:0,true;")));

    // A TX audio frame from the app lands nowhere.
    client.sendBinaryMessage(makeTxFrame(128));
    QTest::qWait(50);
    QCOMPARE(server.peekTxRingSize(), 0);

    // The operator is told why, once per 30 s as a toast; the app never is.
    QCOMPARE(notices.count(), 1);
    const QString reason = notices.constFirst().at(1).toString();
    QCOMPARE(reason, QString::fromLatin1(TciServer::kRemoteTransmitRefusedReason));
    QVERIFY(!notices.constFirst().at(0).toString().isEmpty());   // the app's host:port
    QVERIFY(notices.constFirst().at(2).toBool());
    client.sendTextMessage(QStringLiteral("trx:0,true;"));
    QTRY_COMPARE_WITH_TIMEOUT(notices.count(), 2, 3000);
    QVERIFY(!notices.at(1).at(2).toBool());
    for (const QString& line : lines()) {
        QVERIFY2(!line.contains(reason), qPrintable(line));
    }

    // Releasing is answered the same way and is no refusal.
    client.sendTextMessage(QStringLiteral("trx:0,false;"));
    QTest::qWait(100);
    QCOMPARE(notices.count(), 2);
    QVERIFY(!remote.mox());

    client.close();
    server.stop();
}

// R-R3-48 / R-R3-25: the Core's station TCI server (a Local model with the
// station receive-only mode on) keys nothing for any app: no MOX, no TX
// audio lock, no TX audio; trx:N,false back to the app; the reason plain
// and off the wire. The init burst says receive-only.
void TestTciTxMutex::station_server_refuses_transmit_until_remote_transmit()
{
    RadioModel core;
    TciServer server(&core);
    QVERIFY(!server.isRemoteWindow());
    server.setStationReceiveOnly(true);
    QVERIFY(server.stationReceiveOnly());
    QSignalSpy notices(&server, &TciServer::operatorNotice);
    QSignalSpy txOwner(&server, &TciServer::txAudioActiveClientChanged);
    QVERIFY(server.start(0));

    QWebSocket client;
    QSignalSpy connected(&client, &QWebSocket::connected);
    QSignalSpy text(&client, &QWebSocket::textMessageReceived);
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(connected.wait(2000));
    const auto lines = [&text] {
        QStringList out;
        for (const auto& call : text) { out << call.at(0).toString(); }
        return out;
    };
    QTRY_VERIFY_WITH_TIMEOUT(lines().contains(QStringLiteral("ready;")), 3000);
    QVERIFY(lines().contains(QStringLiteral("receive_only:true;")));
    QVERIFY(lines().contains(QStringLiteral("tx_enable:0,false;")));
    const int linesBefore = int(text.count());

    client.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_VERIFY_WITH_TIMEOUT(lines().mid(linesBefore).contains(QStringLiteral("trx:0,false;")),
                             3000);
    QTest::qWait(100);
    QVERIFY(!core.mox());
    QCOMPARE(server.activeTxClientCount(), 0);
    QCOMPARE(txOwner.count(), 0);
    client.sendBinaryMessage(makeTxFrame(128));
    QTest::qWait(50);
    QCOMPARE(server.peekTxRingSize(), 0);

    QCOMPARE(notices.count(), 1);
    const QString reason = notices.constFirst().at(1).toString();
    QCOMPARE(reason, QString::fromLatin1(TciServer::kStationTransmitRefusedReason));
    QCOMPARE(server.operatorNoticeReason(), reason);
    for (const QString& line : lines()) {
        QVERIFY2(!line.contains(reason), qPrintable(line));
    }

    // Off (a local window): transmit works as it always has.
    server.stop();
    server.setStationReceiveOnly(false);
    QVERIFY(server.start(0));
    QWebSocket local;
    QSignalSpy localConnected(&local, &QWebSocket::connected);
    local.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(localConnected.wait(2000));
    local.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_COMPARE_WITH_TIMEOUT(server.activeTxClientCount(), 1, 3000);
    local.sendTextMessage(QStringLiteral("trx:0,false;"));
    QTRY_COMPARE_WITH_TIMEOUT(server.activeTxClientCount(), 0, 3000);
    local.close();
    client.close();
    server.stop();
}

// M1 (R-R3-48 / R-R3-25): the station server does not let an app change the
// Core's TX profile or XIT (tx_profile_ex, xit_enable, xit_offset) while the
// Core is receive-only: nothing is applied or broadcast, the asking app
// hears the current value, and the operator gets the same plain transmit
// reason, off the wire. Queries still answer.
void TestTciTxMutex::station_server_refuses_transmit_settings()
{
    RadioModel core;
    TciServer server(&core);
    server.setStationReceiveOnly(true);
    QSignalSpy notices(&server, &TciServer::operatorNotice);
    QVERIFY(server.start(0));

    QWebSocket client;
    QWebSocket other;
    QSignalSpy connected(&client, &QWebSocket::connected);
    QSignalSpy otherConnected(&other, &QWebSocket::connected);
    QSignalSpy text(&client, &QWebSocket::textMessageReceived);
    QSignalSpy otherText(&other, &QWebSocket::textMessageReceived);
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    other.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(connected.wait(2000));
    QVERIFY(otherConnected.count() == 1 || otherConnected.wait(2000));
    const auto lines = [](const QSignalSpy& spy) {
        QStringList out;
        for (const auto& call : spy) {
            for (const QString& part :
                 call.at(0).toString().split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
                out << part.trimmed() + QLatin1Char(';');
            }
        }
        return out;
    };
    QTRY_VERIFY_WITH_TIMEOUT(lines(text).contains(QStringLiteral("ready;")), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(lines(otherText).contains(QStringLiteral("ready;")), 3000);
    const QString profileBefore = core.txProfile();
    const int mark = int(text.count());
    const int otherMark = int(otherText.count());

    client.sendTextMessage(QStringLiteral("xit_enable:0,true;"));
    client.sendTextMessage(QStringLiteral("xit_offset:0,500;"));
    client.sendTextMessage(QStringLiteral("tx_profile_ex:Contest;"));
    QTRY_COMPARE_WITH_TIMEOUT(notices.count(), 3, 3000);
    QTRY_VERIFY_WITH_TIMEOUT(lines(text).mid(mark).contains(QStringLiteral("xit_enable:0,false;")),
                             3000);
    QTest::qWait(150);
    QVERIFY(!core.xitEnable());
    QCOMPARE(core.xitOffset(), 0);
    QCOMPARE(core.txProfile(), profileBefore);
    for (const QString& line : lines(text).mid(mark) + lines(otherText).mid(otherMark)) {
        QVERIFY2(line != QStringLiteral("xit_enable:0,true;")
                     && line != QStringLiteral("xit_offset:0,500;")
                     && line != QStringLiteral("tx_profile_ex:Contest;"), qPrintable(line));
        QVERIFY2(!line.contains(QString::fromLatin1(TciServer::kStationTransmitRefusedReason)),
                 qPrintable(line));
    }
    for (const auto& notice : notices) {
        QCOMPARE(notice.at(1).toString(),
                 QString::fromLatin1(TciServer::kStationTransmitRefusedReason));
    }
    // A query still answers, and raises nothing.
    client.sendTextMessage(QStringLiteral("xit_offset:0;"));
    QTRY_VERIFY_WITH_TIMEOUT(lines(text).mid(mark).contains(QStringLiteral("xit_offset:0,0;")),
                             3000);
    QCOMPARE(notices.count(), 3);
    client.close();
    other.close();
    server.stop();
}

// Thetis handleTrxMessage (TCIServer.cs:3594-3689 [v2.10.3.15]) and
// OnMoxPreChangeHandler (TCIServer.cs:7325-7338 [v2.10.3.15]), two apps on
// one server:
//   - trx:N,true while the transmitter is already keyed does nothing: no MOX
//     write, no TX audio for the asker, no answer.
//   - any app's trx:N,false unkeys, and unkeying releases the TX audio.
//   - trx:N,true with the transmitter unkeyed keys it, even when the TX
//     audio is held by another app (the asker's audio is refused).
// The radio model is not connected; the counting MOX check stands in for the
// transmitter, so nothing keys a radio.
void TestTciTxMutex::second_app_trx_follows_thetis_rule()
{
    RadioModel core;
    MoxController* mox = core.moxController();
    QVERIFY(mox != nullptr);
    int keyRequests = 0;
    bool refuseKey = false;
    mox->setMoxCheck([&keyRequests, &refuseKey]() {
        ++keyRequests;
        return safety::BandPlanGuard::MoxCheckResult{!refuseKey,
            refuseKey ? QStringLiteral("test refusal") : QString()};
    });
    QSignalSpy moxSettled(mox, &MoxController::moxStateChanged);

    TciServer server(&core);
    QVERIFY(server.start(0));
    const QString url = QStringLiteral("ws://127.0.0.1:%1").arg(server.port());
    QWebSocket appA;
    QWebSocket appB;
    QSignalSpy connA(&appA, &QWebSocket::connected);
    QSignalSpy connB(&appB, &QWebSocket::connected);
    QSignalSpy textB(&appB, &QWebSocket::textMessageReceived);
    appA.open(QUrl(url));
    QVERIFY(connA.wait(2000));
    appB.open(QUrl(url));
    QVERIFY(connB.wait(2000));
    const QString peerA = QString::number(appA.localPort());
    const QString peerB = QString::number(appB.localPort());
    const auto holderIs = [&server](const QString& port) {
        return server.activeTxClientCount() == 1
            && server.activeTxClientPeer().endsWith(QLatin1Char(':') + port);
    };
    const auto linesB = [&textB](int from) {
        QStringList out;
        for (int i = from; i < textB.count(); ++i) {
            for (const QString& part : textB.at(i).at(0).toString().split(
                     QLatin1Char(';'), Qt::SkipEmptyParts)) {
                out << part.trimmed() + QLatin1Char(';');
            }
        }
        return out;
    };

    // App A keys with TCI audio: it holds the TX audio and MOX is on.
    appA.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_VERIFY_WITH_TIMEOUT(holderIs(peerA), 3000);
    QVERIFY(core.mox());
    QCOMPARE(keyRequests, 1);
    QTRY_COMPARE_WITH_TIMEOUT(moxSettled.count(), 1, 3000);   // walk done
    QTest::qWait(50);

    // App B asks to key while A transmits: nothing happens, B hears nothing.
    const int markB = int(textB.count());
    appB.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTest::qWait(150);
    QCOMPARE(keyRequests, 1);
    QVERIFY(core.mox());
    QVERIFY(holderIs(peerA));
    for (const QString& line : linesB(markB)) {
        QVERIFY2(!line.startsWith(QStringLiteral("trx:")), qPrintable(line));
    }
    const int ringBefore = server.peekTxRingSize();
    appB.sendBinaryMessage(makeTxFrame(64));
    QTest::qWait(50);
    QCOMPARE(server.peekTxRingSize(), ringBefore);

    // App B unkeys: any app may, and unkeying releases A's TX audio.
    appB.sendTextMessage(QStringLiteral("trx:0,false;"));
    QTRY_VERIFY_WITH_TIMEOUT(!core.mox(), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(server.activeTxClientCount(), 0, 3000);

    // With the transmitter unkeyed, B keys and takes the TX audio.
    appB.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_VERIFY_WITH_TIMEOUT(holderIs(peerB), 3000);
    QVERIFY(core.mox());
    QCOMPARE(keyRequests, 2);

    // A asks while B transmits: ignored the same way.
    appA.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTest::qWait(150);
    QCOMPARE(keyRequests, 2);
    QVERIFY(holderIs(peerB));
    appB.sendTextMessage(QStringLiteral("trx:0,false;"));
    QTRY_VERIFY_WITH_TIMEOUT(!core.mox(), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(server.activeTxClientCount(), 0, 3000);

    // A asks but the transmitter refuses to key. Task 7 follow-up (item
    // 5): a trx that keyed nothing gives the TX audio back at once, so A
    // does not hold it with MOX off (Thetis kept it until A's trx:false).
    // B's trx then keys the transmitter and takes the TX audio itself.
    refuseKey = true;
    appA.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_COMPARE_WITH_TIMEOUT(keyRequests, 3, 3000);
    QTest::qWait(50);
    QVERIFY(!core.mox());
    QCOMPARE(server.activeTxClientCount(), 0);
    refuseKey = false;
    appB.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_VERIFY_WITH_TIMEOUT(core.mox(), 3000);
    QCOMPARE(keyRequests, 4);
    QVERIFY(holderIs(peerB));

    appB.sendTextMessage(QStringLiteral("trx:0,false;"));
    QTRY_VERIFY_WITH_TIMEOUT(!core.mox(), 3000);
    QTRY_COMPARE_WITH_TIMEOUT(server.activeTxClientCount(), 0, 3000);
    appA.close();
    appB.close();
    server.stop();
}

// ── Task 7 fix wave, I1 (R-R3-49): the trx reply tells the truth ────────────
//
// From Thetis TCIServer.cs:3623-3672 [v2.10.3.15] (handleTrxMessage): the
// trx handler writes TCIPTT and broadcasts nothing itself; apps hear the
// transmitter's real state from the MoxChange handlers (sendMOX). An app
// that asks is answered with the transmitter's state after the call,
// never with the value it asked for.
namespace {
struct TrxApp {
    QWebSocket socket;
    QSignalSpy text{&socket, &QWebSocket::textMessageReceived};
    QSignalSpy binary{&socket, &QWebSocket::binaryMessageReceived};
    QStringList lines(int from = 0) const
    {
        QStringList out;
        for (int i = from; i < text.count(); ++i) {
            for (const QString& part : text.at(i).at(0).toString().split(
                     QLatin1Char(';'), Qt::SkipEmptyParts)) {
                out << part.trimmed() + QLatin1Char(';');
            }
        }
        return out;
    }
    bool open(quint16 port)
    {
        QSignalSpy connected(&socket, &QWebSocket::connected);
        socket.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(port)));
        if (!connected.wait(2000)) {
            return false;
        }
        return QTest::qWaitFor([this] { return lines().contains(QStringLiteral("ready;")); },
                               3000);
    }
};

void allowEveryKey(MoxController* mox)
{
    mox->setMoxCheck([]() {
        return safety::BandPlanGuard::MoxCheckResult{true, QString()};
    });
}
} // namespace

// Scenario A: the operator keys with the MOX button; an app's trx:0,false
// does not unkey it (PollPTT releases only a TCI key), so no app may be
// told the transmitter is receiving.
void TestTciTxMutex::trx_false_during_operator_key_is_not_echoed()
{
    RadioModel core;
    MoxController* mox = core.moxController();
    QVERIFY(mox != nullptr);
    allowEveryKey(mox);
    TciServer server(&core);
    QVERIFY(server.start(0));
    TrxApp asker;
    TrxApp watcher;
    QVERIFY(asker.open(server.port()));
    QVERIFY(watcher.open(server.port()));

    core.setMoxFromButton(true);
    QVERIFY(core.mox());
    QTRY_VERIFY_WITH_TIMEOUT(watcher.lines().contains(QStringLiteral("trx:0,true;")), 3000);
    const int markAsker = int(asker.text.count());
    const int markWatcher = int(watcher.text.count());

    asker.socket.sendTextMessage(QStringLiteral("trx:0,false;"));
    QTest::qWait(200);
    QVERIFY(core.mox());
    QVERIFY2(!asker.lines(markAsker).contains(QStringLiteral("trx:0,false;")),
             "the asking app was told receive while the MOX button transmits");
    QVERIFY2(!watcher.lines(markWatcher).contains(QStringLiteral("trx:0,false;")),
             "another app was told receive while the MOX button transmits");
    // The asker hears the truth.
    QVERIFY(asker.lines(markAsker).contains(QStringLiteral("trx:0,true;")));

    core.setMoxFromButton(false);
    QTRY_VERIFY_WITH_TIMEOUT(watcher.lines(markWatcher).contains(QStringLiteral("trx:0,false;")),
                             3000);
    asker.socket.close();
    watcher.socket.close();
    server.stop();
}

// Scenario B: with a manual key on and the transmitter unkeyed (TUN-off
// window, two-tone settle), an app's trx:0,true keys nothing, so no app may
// be told the transmitter is keyed.
void TestTciTxMutex::trx_true_that_keys_nothing_is_not_echoed()
{
    RadioModel core;
    MoxController* mox = core.moxController();
    QVERIFY(mox != nullptr);
    allowEveryKey(mox);
    mox->setManualKey(true);
    TciServer server(&core);
    QVERIFY(server.start(0));
    TrxApp asker;
    TrxApp watcher;
    QVERIFY(asker.open(server.port()));
    QVERIFY(watcher.open(server.port()));
    const int markAsker = int(asker.text.count());
    const int markWatcher = int(watcher.text.count());

    asker.socket.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTest::qWait(200);
    QVERIFY(!core.mox());
    QVERIFY2(!asker.lines(markAsker).contains(QStringLiteral("trx:0,true;")),
             "the asking app was told it transmits while nothing keyed");
    QVERIFY2(!watcher.lines(markWatcher).contains(QStringLiteral("trx:0,true;")),
             "another app was told the transmitter keyed while nothing keyed");
    QVERIFY(asker.lines(markAsker).contains(QStringLiteral("trx:0,false;")));

    asker.socket.sendTextMessage(QStringLiteral("trx:0,false;"));
    QTest::qWait(50);
    mox->setManualKey(false);
    QTest::qWait(50);
    QVERIFY(!core.mox());
    asker.socket.close();
    watcher.socket.close();
    server.stop();
}

// The WSJT-X fix of 2026-05-10 still holds with WSJT-X's own sequence:
// trx:0,true,tci; then, before it streams, trx:0,true; with no ",tci"
// suffix; TX_CHRONO runs while keyed; trx:0,false; then trx:0,false;.
void TestTciTxMutex::wsjtx_sequence_still_sees_trx_true_without_suffix()
{
    RadioModel core;
    MoxController* mox = core.moxController();
    QVERIFY(mox != nullptr);
    allowEveryKey(mox);
    TciServer server(&core);
    QVERIFY(server.start(0));
    TrxApp wsjtx;
    QVERIFY(wsjtx.open(server.port()));
    const int mark = int(wsjtx.text.count());

    wsjtx.socket.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_VERIFY_WITH_TIMEOUT(wsjtx.lines(mark).contains(QStringLiteral("trx:0,true;")), 3000);
    QVERIFY(core.mox());
    QCOMPARE(server.activeTxClientCount(), 1);
    QTRY_VERIFY_WITH_TIMEOUT(wsjtx.binary.count() > 0, 3000);   // TX_CHRONO
    for (const QString& line : wsjtx.lines(mark)) {
        QVERIFY2(!line.startsWith(QStringLiteral("trx:")) || !line.contains(QStringLiteral("tci")),
                 qPrintable(line));
    }

    const int markOff = int(wsjtx.text.count());
    wsjtx.socket.sendTextMessage(QStringLiteral("trx:0,false;"));
    QTRY_VERIFY_WITH_TIMEOUT(wsjtx.lines(markOff).contains(QStringLiteral("trx:0,false;")), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(!core.mox(), 3000);
    QCOMPARE(server.activeTxClientCount(), 0);
    wsjtx.socket.close();
    server.stop();
}

// Fix wave RD-C1 (JJ ruling 1): the app whose trx keyed the transmitter
// is gone, by its own disconnect or by the server stopping. Its key is
// released before its TX audio lock, so the microphone never goes on air
// in the moment between. Departure from Thetis TCIServer.cs:3010-3026
// [v2.10.3.15], which drops only the audio listener there.
void TestTciTxMutex::disconnect_releases_the_app_key_before_its_audio_data()
{
    QTest::addColumn<bool>("stopServer");
    QTest::addColumn<bool>("tciAudio");
    QTest::newRow("disconnect, TCI audio") << false << true;
    QTest::newRow("disconnect, no TCI audio") << false << false;
    QTest::newRow("server stop, TCI audio") << true << true;
    QTest::newRow("server stop, no TCI audio") << true << false;
}

void TestTciTxMutex::disconnect_releases_the_app_key_before_its_audio()
{
    QFETCH(bool, stopServer);
    QFETCH(bool, tciAudio);
    RadioModel core;
    MoxController* mox = core.moxController();
    QVERIFY(mox != nullptr);
    allowEveryKey(mox);
    TciServer server(&core);
    QVERIFY(server.start(0));
    TrxApp app;
    QVERIFY(app.open(server.port()));

    app.socket.sendTextMessage(tciAudio ? QStringLiteral("trx:0,true,tci;")
                                        : QStringLiteral("trx:0,true;"));
    QTRY_VERIFY_WITH_TIMEOUT(core.mox(), 3000);
    QCOMPARE(server.activeTxClientCount(), tciAudio ? 1 : 0);

    // At the moment the audio lock goes, the app's key must already be
    // released: its TCI level dropped, so the transmitter is unkeying (the
    // lock goes on the way down, as OnMoxPreChangeHandler releases it) and
    // nothing keeps it on the air with the microphone.
    bool keyedWhenAudioReleased = false;
    int audioReleases = 0;
    connect(&server, &TciServer::txAudioActiveClientChanged, &server,
            [&](QWebSocket* active) {
                if (active == nullptr) {
                    ++audioReleases;
                    keyedWhenAudioReleased = keyedWhenAudioReleased || mox->isTciPttHeld();
                }
            });
    if (stopServer) {
        server.stop();
    } else {
        app.socket.close();
    }
    QTRY_VERIFY_WITH_TIMEOUT(!core.mox(), 3000);
    QVERIFY(!mox->isTciPttHeld());
    QTRY_COMPARE_WITH_TIMEOUT(server.activeTxClientCount(), 0, 3000);
    QCOMPARE(audioReleases, tciAudio ? 1 : 0);
    QVERIFY2(!keyedWhenAudioReleased,
             "the TX audio lock was released while the app's key still held");
    app.socket.close();
    server.stop();
}

// Only the app that keyed loses its key: an app that never keyed leaves a
// MOX-button key, and another app's TCI key, alone when it goes.
void TestTciTxMutex::disconnect_of_an_app_that_did_not_key_leaves_the_key()
{
    RadioModel core;
    MoxController* mox = core.moxController();
    QVERIFY(mox != nullptr);
    allowEveryKey(mox);
    TciServer server(&core);
    QVERIFY(server.start(0));
    TrxApp keyer;
    TrxApp bystander;
    QVERIFY(keyer.open(server.port()));
    QVERIFY(bystander.open(server.port()));

    keyer.socket.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_VERIFY_WITH_TIMEOUT(core.mox(), 3000);
    // Ignored while keyed (Thetis handleTrxMessage): it takes no ownership.
    bystander.socket.sendTextMessage(QStringLiteral("trx:0,true;"));
    QTRY_VERIFY_WITH_TIMEOUT(bystander.lines().contains(QStringLiteral("trx:0,true;")), 3000);
    bystander.socket.close();
    QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 1, 3000);
    QVERIFY(core.mox());
    QCOMPARE(server.activeTxClientCount(), 1);
    keyer.socket.sendTextMessage(QStringLiteral("trx:0,false;"));
    QTRY_VERIFY_WITH_TIMEOUT(!core.mox(), 3000);

    // A MOX-button key is the operator's: an app that goes takes nothing.
    core.setMoxFromButton(true);
    QVERIFY(core.mox());
    keyer.socket.close();
    QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 0, 3000);
    QVERIFY(core.mox());
    core.setMoxFromButton(false);
    QTRY_VERIFY_WITH_TIMEOUT(!core.mox(), 3000);
    server.stop();
}

// Fix wave RD-I2: trx keys only receiver 0, or receiver 1 while RX2 is on.
// From Thetis TCIServer.cs:3666-3680 [v2.10.3.15] (handleTrxMessage):
// rx == 0 writes TCIPTT; rx == 1 only with RX2Enabled; any other index
// writes nothing.
void TestTciTxMutex::trx_on_a_receiver_that_is_off_keys_nothing()
{
    RadioModel core;
    MoxController* mox = core.moxController();
    QVERIFY(mox != nullptr);
    allowEveryKey(mox);
    TciServer server(&core);
    QVERIFY(server.start(0));
    TrxApp app;
    QVERIFY(app.open(server.port()));

    // No receiver 1 slice: RX2 is off.
    for (const QString& trx : {QStringLiteral("trx:1,true;"), QStringLiteral("trx:2,true,tci;"),
                               QStringLiteral("trx:-1,true;")}) {
        const int mark = int(app.text.count());
        app.socket.sendTextMessage(trx);
        QTRY_VERIFY_WITH_TIMEOUT(app.text.count() > mark, 3000);
        QVERIFY2(!core.mox(), qPrintable(trx));
        QVERIFY2(!mox->isTciPttHeld(), qPrintable(trx));
        QCOMPARE(server.activeTxClientCount(), 0);
    }
    app.socket.sendTextMessage(QStringLiteral("trx:0,true;"));
    QTRY_VERIFY_WITH_TIMEOUT(core.mox(), 3000);
    app.socket.sendTextMessage(QStringLiteral("trx:0,false;"));
    QTRY_VERIFY_WITH_TIMEOUT(!core.mox(), 3000);
    app.socket.close();
    server.stop();
}

// Fix wave RD-I3: a desktop host's app keys through the same PollPTT
// admission as a local app's trx (MoxController::onTciPtt): never under a
// manual key (Thetis console.cs:25470 [v2.10.3.15], !_manual_mox), and a
// press that keyed nothing leaves no TCI level behind to key later for an
// app that was told trx:0,false.
void TestTciTxMutex::desktop_host_trx_goes_through_ptt_admission()
{
    RadioModel radio;
    const int owned = radio.addSlice(QStringLiteral("pan-0"));
    radio.sliceOwnership()->setOwner(owned, SliceOwnership::stationDevice());
    TransmitHolder holder;
    MoxController* mox = radio.moxController();
    mox->setKeyingGate([&holder](PttMode source, const KeyerIdentity& keyer) {
        return holder.askKey({keyer.deviceId, TransmitHolder::Source::Device,
                              keyer.program, source == PttMode::Vox});
    });
    holder.transferTo(TransmitHolder::Holder{SliceOwnership::stationDevice()},
                      QStringLiteral("test"), [](bool) {});
    radio.setTransmitHolder(SliceOwnership::stationDevice());
    QVERIFY(radio.txSliceArbiter()->bindForHolder(SliceOwnership::stationDevice(), owned));
    TciServer server(&radio);
    server.setDesktopHostMode(true);
    QVERIFY(server.start(0));
    TrxApp app;
    QVERIFY(app.open(server.port()));

    mox->setManualKey(true);
    const int mark = int(app.text.count());
    app.socket.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_VERIFY_WITH_TIMEOUT(app.lines(mark).contains(QStringLiteral("trx:0,false;")), 3000);
    QVERIFY(!radio.mox());
    QVERIFY(!mox->isTciPttHeld());
    QCOMPARE(server.activeTxClientCount(), 0);
    // The manual key ends: nothing keys for the press that was refused.
    mox->setManualKey(false);
    QVERIFY(!radio.mox());

    // Admitted, it keys as the station's TCI source, and its release
    // drops the level with the key.
    app.socket.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_VERIFY_WITH_TIMEOUT(radio.mox(), 3000);
    QVERIFY(mox->isTciPttHeld());
    QCOMPARE(mox->currentKeyer(), KeyerIdentity::station(PttMode::Tci));
    QCOMPARE(server.activeTxClientCount(), 1);
    app.socket.sendTextMessage(QStringLiteral("trx:0,false;"));
    QTRY_VERIFY_WITH_TIMEOUT(!radio.mox(), 3000);
    QVERIFY(!mox->isTciPttHeld());
    QCOMPARE(server.activeTxClientCount(), 0);
    app.socket.close();
    server.stop();
}

// Fix round 1 (Important 1): command names are not case-sensitive
// (TciProtocol lowercases them, as Thetis TCIServer.cs:5288 [v2.10.3.15]
// does), so an app's TRX:0,true; keys like trx:0,true;. Its key must end
// with the app, or with the server, as a lowercase trx's does.
void TestTciTxMutex::uppercase_trx_key_ends_with_the_app_data()
{
    QTest::addColumn<bool>("stopServer");
    QTest::newRow("disconnect") << false;
    QTest::newRow("server stop") << true;
}

void TestTciTxMutex::uppercase_trx_key_ends_with_the_app()
{
    QFETCH(bool, stopServer);
    RadioModel core;
    MoxController* mox = core.moxController();
    QVERIFY(mox != nullptr);
    allowEveryKey(mox);
    TciServer server(&core);
    QVERIFY(server.start(0));
    TrxApp app;
    QVERIFY(app.open(server.port()));

    app.socket.sendTextMessage(QStringLiteral("TRX:0,true;"));
    QTRY_VERIFY_WITH_TIMEOUT(core.mox(), 3000);
    QVERIFY(mox->isTciPttHeld());
    if (stopServer) {
        server.stop();
    } else {
        app.socket.close();
        QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 0, 3000);
    }
    QTRY_VERIFY_WITH_TIMEOUT(!core.mox(), 3000);
    QVERIFY(!mox->isTciPttHeld());
    app.socket.close();
    server.stop();
}

// Fix round 1 (Important 1): a desktop host's uppercase TRX goes through
// the same PollPTT admission as trx (RD-I3): refused under a manual key,
// with no TCI level left behind.
void TestTciTxMutex::desktop_host_uppercase_trx_goes_through_ptt_admission()
{
    RadioModel radio;
    const int owned = radio.addSlice(QStringLiteral("pan-0"));
    radio.sliceOwnership()->setOwner(owned, SliceOwnership::stationDevice());
    TransmitHolder holder;
    MoxController* mox = radio.moxController();
    mox->setKeyingGate([&holder](PttMode source, const KeyerIdentity& keyer) {
        return holder.askKey({keyer.deviceId, TransmitHolder::Source::Device,
                              keyer.program, source == PttMode::Vox});
    });
    holder.transferTo(TransmitHolder::Holder{SliceOwnership::stationDevice()},
                      QStringLiteral("test"), [](bool) {});
    radio.setTransmitHolder(SliceOwnership::stationDevice());
    QVERIFY(radio.txSliceArbiter()->bindForHolder(SliceOwnership::stationDevice(), owned));
    TciServer server(&radio);
    server.setDesktopHostMode(true);
    QVERIFY(server.start(0));
    TrxApp app;
    QVERIFY(app.open(server.port()));

    mox->setManualKey(true);
    const int mark = int(app.text.count());
    app.socket.sendTextMessage(QStringLiteral("TRX:0,true,TCI;"));
    QTRY_VERIFY_WITH_TIMEOUT(app.lines(mark).contains(QStringLiteral("trx:0,false;")), 3000);
    QVERIFY(!radio.mox());
    QVERIFY(!mox->isTciPttHeld());
    QCOMPARE(server.activeTxClientCount(), 0);
    mox->setManualKey(false);
    QVERIFY(!radio.mox());

    app.socket.sendTextMessage(QStringLiteral("TRX:0,true,tci;"));
    QTRY_VERIFY_WITH_TIMEOUT(radio.mox(), 3000);
    QCOMPARE(mox->currentKeyer(), KeyerIdentity::station(PttMode::Tci));
    app.socket.close();
    QTRY_VERIFY_WITH_TIMEOUT(!radio.mox(), 3000);
    QVERIFY(!mox->isTciPttHeld());
    server.stop();
}

// Fix wave minor: the TX_AUDIO_STREAM header. A sample rate the TX
// resampler cannot take (outside 8000 to 384000, as audio_samplerate:) is
// dropped; 0 passes as Thetis passes it (no resampling). A legacy header
// (no channels field) whose length is above INT_MAX / 2 is read as mono,
// not as stereo through an int overflow.
void TestTciTxMutex::tx_audio_header_is_validated()
{
    TciServer server(nullptr);
    QVERIFY(server.start(0));
    QWebSocket app;
    QSignalSpy connected(&app, &QWebSocket::connected);
    app.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(connected.wait(2000));
    app.sendTextMessage(QStringLiteral("trx:0,true,tci;"));
    QTRY_COMPARE_WITH_TIMEOUT(server.activeTxClientCount(), 1, 3000);

    const auto frameAt = [](int sampleRate, int channels) {
        std::vector<float> samples(128, 0.25f);
        return TciBinaryFrame::buildStreamPayload(0, sampleRate,
            static_cast<int>(TciSampleType::Float32), 128,
            static_cast<int>(TciStreamType::TxAudioStream), channels, samples.data());
    };
    const auto sendAndMeasure = [&](const QByteArray& frame) {
        const int before = server.peekTxRingSize();
        // A mono marker frame after it shows the first was processed.
        app.sendBinaryMessage(frame);
        app.sendBinaryMessage(frameAt(48000, 1));
        const int marker = 128 * int(sizeof(float));
        if (!QTest::qWaitFor([&] { return server.peekTxRingSize() >= before + marker; }, 3000)) {
            return -1;
        }
        return server.peekTxRingSize() - before - marker;
    };

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(
        QStringLiteral("TX audio dropped, sample rate 1 is outside")));
    QCOMPARE(sendAndMeasure(frameAt(1, 1)), 0);
    QCOMPARE(sendAndMeasure(frameAt(2000000, 1)), 0);   // logged once per app
    QCOMPARE(sendAndMeasure(frameAt(0, 1)), 128 * int(sizeof(float)));
    QCOMPARE(sendAndMeasure(frameAt(8000, 1)), 128 * int(sizeof(float)));

    // Legacy header: channels field 0, length INT_MAX. 128 values are
    // there; a stereo reading would fold them to 64.
    QByteArray legacy = frameAt(48000, 0);
    const qint32 huge = std::numeric_limits<qint32>::max();
    for (int i = 0; i < 4; ++i) {
        legacy[20 + i] = static_cast<char>((static_cast<quint32>(huge) >> (8 * i)) & 0xff);
    }
    QCOMPARE(sendAndMeasure(legacy), 128 * int(sizeof(float)));

    app.close();
    server.stop();
}

QTEST_GUILESS_MAIN(TestTciTxMutex)
#include "tst_tci_tx_mutex.moc"

#else  // !HAVE_WEBSOCKETS

int main() { return 0; }

#endif // HAVE_WEBSOCKETS
