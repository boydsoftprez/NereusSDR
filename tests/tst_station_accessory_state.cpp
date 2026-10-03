// no-port-check: NereusSDR-original. R-R3-22 Core accessory ownership tests.
// J.J. Boyd (KG4VCF), September 2026; AI-assisted via OpenAI Codex.
// 2026-09-23: R-R3-47 / R-R3-22 status objects (`amplifier`, `rfkit`): the
// one Power Genius gauge conversion from captured status lines, the RF-Kit
// readings from its REST replies, the connection phases, what a current and
// an older app are offered, the read-only refusal, and the control
// document's fixtures (tests/fixtures/accessories). J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-47 / R-R3-22 accessory records and settings
// (`accessoryData`, accessoryDataVersion 1): offered to current apps only
// and read-only, its fixture, the interlock enum, the Tuner Genius faults
// the Core records (a live connection dropping, an attempt ending at an
// error, never an operator's disconnect), and the power-cap alert the Core
// raises. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-47 / R-R3-22: remotePgxlControlVersion 3 and
// remoteTgxlControlVersion 1 (last), the read-only `accessorySettings`
// object and its fixture. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
// Claude Code.
// 2026-09-24: Lane B takes integration (R-IOS-01, R-R3-21): the plain
// refusals of a Core that does not own its accessories. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-49 / R-R3-47: remoteTgxlControlVersion 2 and the Tuner
// Genius's antenna, operate and bypass refusals on the wire. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-49 fix wave: remoteTgxlControlVersion 3. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-24: R-R3-49 (parity Task 1): transmitSettingsVersion now travels
// last. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-25: R-R3-49 (parity Task 8): remoteTgxlControlVersion 4 and the
// refusals of moveTgxlRelay, scanTgxlLan and setTgxlAddress on the wire.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-25: R-R3-49 (parity Task 9): remotePgxlControlVersion 4 and the
// refusals of setPgxlOperate, scanPgxlLan and setPgxlAddress on the wire.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-25: R-R3-49 (parity Task 10): remoteRfKitControlVersion 4 and
// accessoryDataVersion 2, the refusals of setRfKitOperate, setRfKitAntenna,
// setRfKitTciMode and setRfKitAddress on the wire, and the RF-Kit's
// connection counts in the accessoryData fixture. J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// 2026-09-25: checkpoint carry: parity Tasks 8 to 10's stations sign the
// peer in with an upgraded Core's token (seedUpgradedCoreToken). J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
#include <QtTest/QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <cmath>
#include "core/AppSettings.h"
#include "core/SmartSdrApiListener.h"
#include "core/LanDiscovery.h"
#include "core/StationPgxlController.h"
#include "core/PgxlConnection.h"
#include "core/PgxlStatusGauges.h"
#include "core/Rf2ksConnection.h"
#include "core/ConnectionDiagnostics.h"
#include "core/FaultLog.h"
#include "core/StationAccessoryData.h"
#include "core/TgxlConnection.h"
#include "core/TuneMemoryStore.h"
#include "core/TxInterlockPolicy.h"
#include "OperatorWording.h"
#include "core/TxSliceArbiter.h"
#include "core/session/SessionMessages.h"
#include "core/session/StateMirror.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationServer.h"
#include "models/AccessoryDataModel.h"
#include "models/AccessorySettingsModel.h"
#include "models/AmplifierModel.h"
#include "models/RadioModel.h"
#include "models/RfKitModel.h"
#include "models/StationTciModel.h"
#include "models/SliceModel.h"

#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

// Status lines as captured or as the repository's PGXL parser tests carry
// them (tst_pgxl_connection_parse, tst_pgxl_connection_setup and the design
// doc's setup read reply). The transmit frames use the same keys with a
// 60 dBm (1000 W) peak and the -24.5 dB return loss the conversion's own
// bench note uses (SWR 1.13).
constexpr const char* kPgxlOperate = "R1|0|state=OPERATE temp=42.5 vac=240 fwd=1480.0 swr=2.1";
constexpr const char* kPgxlTransmit = "S0|status state=TRANSMIT_A peakfwd=60.0 swr=-24.5 id=22.5";
constexpr const char* kPgxlStandby = "S0|status state=STANDBY peakfwd=60.0 swr=-24.5 id=0.0";
constexpr const char* kPgxlFault = "S0|state state=FAULT fwd=1820.0 swr=2.85 temp=78.0";
constexpr const char* kPgxlSetup = "R2|0|nickname=ShackAmp fan=auto meffa=off led=65";

// REST replies as tst_rf2ks_connection_parse carries them.
constexpr const char* kRfKitInfo =
    R"({"device":"RF2K-S","software_version":{"GUI":200,"controller":267},"custom_device_name":"KG4VCF"})";
constexpr const char* kRfKitPower =
    R"({"temperature":{"value":27.0,"unit":"°C"},"voltage":{"value":52.7,"unit":"V"},"current":{"value":0.0,"unit":"A"},"forward":{"value":850,"max_value":1200,"unit":"W"},"reflected":{"value":3,"max_value":20,"unit":"W"},"swr":{"value":1.4,"max_value":2.1,"unit":""}})";
constexpr const char* kRfKitPowerIdle =
    R"({"temperature":{"value":0.0,"unit":"°C"},"voltage":{"value":0.0,"unit":"V"},"current":{"value":0.0,"unit":"A"},"forward":{"value":0,"max_value":0,"unit":"W"},"reflected":{"value":0,"max_value":0,"unit":"W"},"swr":{"value":1.0,"max_value":1.0,"unit":""}})";

QString fixturePath(const QString& name)
{
    return QStringLiteral(NEREUS_SOURCE_DIR "/tests/fixtures/accessories/") + name;
}

// Every message a StateMirror sends for one watched object: its schema, its
// object, the snapshot marker, and then each later delta, one per line.
class MirrorRecorder {
public:
    MirrorRecorder(const QByteArray& key, QObject* object)
    {
        m_mirror.watch(key, object);
        QObject::connect(&m_mirror, &StateMirror::sessionMessageReady, &m_mirror,
                         [this](const SessionMessage& message) {
            m_lines.append(SessionMessages::encode(message));
        });
        m_mirror.attachSession();
    }
    void flush() { m_mirror.flushCoalescedDeltas(); }
    QByteArray text() const { return m_lines.join('\n') + '\n'; }

private:
    StateMirror m_mirror;
    QList<QByteArray> m_lines;
};

// The fixture is the recorded text. NEREUS_WRITE_ACCESSORY_FIXTURES=1
// rewrites it from the objects (after a deliberate contract change).
bool matchesFixture(const QString& name, const QByteArray& recorded, QString* why)
{
    if (qEnvironmentVariableIntValue("NEREUS_WRITE_ACCESSORY_FIXTURES") == 1) {
        QFile out(fixturePath(name));
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            *why = QStringLiteral("cannot write ") + out.fileName();
            return false;
        }
        out.write(recorded);
        return true;
    }
    QFile in(fixturePath(name));
    if (!in.open(QIODevice::ReadOnly)) {
        *why = QStringLiteral("missing ") + in.fileName();
        return false;
    }
    const QByteArray expected = in.readAll();
    if (expected != recorded) {
        *why = QStringLiteral("fixture %1 differs; recorded:\n%2")
                   .arg(name, QString::fromUtf8(recorded));
        return false;
    }
    return true;
}

QList<SessionMessage> decodeLines(const QByteArray& text)
{
    QList<SessionMessage> messages;
    for (const QByteArray& line : text.split('\n')) {
        if (line.trimmed().isEmpty()) {
            continue;
        }
        SessionMessage message;
        if (!SessionMessages::decode(line, &message)) {
            return {};
        }
        messages.append(message);
    }
    return messages;
}

LoopbackTransport* connectRawPeer(QObject* owner, StationServer& server, quint16 minor,
                                  LoopbackTransport** peerOut)
{
    auto* core = new LoopbackTransport(QStringLiteral("core"), owner);
    auto* peer = new LoopbackTransport(QStringLiteral("raw-gui"), owner);
    core->linkTo(peer);
    server.acceptTransport(core);
    peer->sendText(SessionMessages::encode(SessionMessages::hello(
        kSessionProtocolMajor, minor, 0, QStringLiteral("accessory-test"))));
    peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server.token())));
    *peerOut = peer;
    return core;
}

QList<SessionMessage> receivedMessages(const LoopbackTransport* peer)
{
    QList<SessionMessage> messages;
    for (const QByteArray& wire : peer->received()) {
        SessionMessage message;
        if (SessionMessages::decode(wire, &message)) {
            messages.append(message);
        }
    }
    return messages;
}

bool snapshotDone(const LoopbackTransport* peer)
{
    for (const SessionMessage& m : receivedMessages(peer)) {
        if (m.kind == SessionMessageKind::SnapshotComplete) {
            return true;
        }
    }
    return false;
}

bool namesAccessory(const SessionMessage& m)
{
    return m.objectKey == "amplifier" || m.objectKey == "rfkit"
        || m.objectKey == "accessoryData" || m.objectKey == "accessorySettings"
        || m.className == "AmplifierModel" || m.className == "RfKitModel"
        || m.className == "AccessoryDataModel" || m.className == "AccessorySettingsModel";
}

} // namespace

class StationAccessoryStateTest : public QObject {
    Q_OBJECT
    static void prepare(RadioModel& model)
    {
        model.enableStationAccessoryIdentity();
        model.setReceiveOnlyStationPolicy(true);
        RadioInfo info;
        info.macAddress = QStringLiteral("aa:bb:cc:dd:ee:61");
        model.setLastRadioInfoForTest(info);
        model.setConnectionStateForTest(ConnectionState::Connected);
        model.smartSdrListener()->setListenEndpointForTesting(QHostAddress::LocalHost, 0);
    }
private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void masterRequiresStationAndLiveMac()
    {
        RadioModel model;
        QString reason;
        QVERIFY(!model.setFourO3AEnabledForStation(true, &reason));
        QVERIFY(!reason.isEmpty());
        model.enableStationAccessoryIdentity();
        QVERIFY(!model.setFourO3AEnabledForStation(true, &reason));
        model.setFourO3AEnabled(true);
        QVERIFY(!model.smartSdrListener()->isListening());
        QVERIFY(!model.fourO3AEnabled());

        RadioModel remote(RadioModel::Role::Remote);
        QVERIFY(!remote.setFourO3AEnabledForStation(true, &reason));
        remote.setFourO3AEnabled(true);
        QVERIFY(!remote.smartSdrListener()->isListening());
    }

    void bindFailureIsObservedAndRepeatedEnableRetries()
    {
        QTcpServer blocker;
        QVERIFY(blocker.listen(QHostAddress::LocalHost, 0));
        const auto port = blocker.serverPort();
        RadioModel model;
        prepare(model);
        model.smartSdrListener()->setListenEndpointForTesting(QHostAddress::LocalHost, port);
        QSignalSpy status(&model, &RadioModel::fourO3AStatusChanged);
        QString reason;
        // Acceptance is persisted intent; actual bind failure is a separate
        // state. A checkbox must never masquerade as a listening socket.
        QVERIFY(model.setFourO3AEnabledForStation(true, &reason));
        QVERIFY(reason.isEmpty());
        QVERIFY(model.fourO3AEnabled());
        QVERIFY(!model.fourO3AListening());
        QVERIFY(!model.fourO3AListenerError().isEmpty());
        QVERIFY(!status.isEmpty());
        AppSettings disk(AppSettings::instance().filePath());
        disk.load();
        QCOMPARE(disk.hardwareValue(model.currentRadioMac(),
            QStringLiteral("peripherals/FourO3A_Enabled")).toString(), QStringLiteral("True"));
        QVERIFY(model.setFourO3AEnabledForStation(false, &reason));
        QVERIFY(model.fourO3AListenerError().isEmpty());
        QVERIFY(model.setFourO3AEnabledForStation(true, &reason));
        QVERIFY(!model.fourO3AListenerError().isEmpty());
        blocker.close();
        QVERIFY(model.setFourO3AEnabledForStation(true, &reason));
        QVERIFY(model.fourO3AListening());
        QVERIFY(model.fourO3AListenerError().isEmpty());
        QVERIFY(model.setFourO3AEnabledForStation(false, &reason));
        QVERIFY(!model.fourO3AListening());
        QVERIFY(!model.fourO3AEnabled());
        QVERIFY(model.fourO3AListenerError().isEmpty());
    }

    void remoteObservationCannotStartListenerOrWriteSettings()
    {
        RadioModel remote(RadioModel::Role::Remote);
        QSignalSpy enabled(&remote, &RadioModel::fourO3AEnabledChanged);
        QVERIFY(remote.applyMirroredValue("fourO3AEnabled", true).isEmpty());
        QVERIFY(remote.applyMirroredValue("fourO3AListening", true).isEmpty());
        QVERIFY(remote.fourO3AEnabled());
        QVERIFY(remote.fourO3AListening());
        QVERIFY(!remote.smartSdrListener()->isListening());
        QVERIFY(!remote.applyMirroredValue("fourO3AEnabled", QStringLiteral("True")).isEmpty());
        QVERIFY(remote.applyMirroredValue("fourO3AListenerError", QStringLiteral("bind failed")).isEmpty());
        QCOMPARE(remote.fourO3AListenerError(), QStringLiteral("bind failed"));
        remote.clearRemoteFourO3AState();
        QVERIFY(!remote.fourO3AEnabled());
        QVERIFY(!remote.fourO3AListening());
        QVERIFY(remote.fourO3AListenerError().isEmpty());
        QCOMPARE(enabled.count(), 2);
        QVERIFY(AppSettings::instance().hardwareValue(QStringLiteral("aa:bb:cc:dd:ee:61"),
            QStringLiteral("peripherals/FourO3A_Enabled")).toString().isEmpty());
    }

    void debouncedPgxlBandUsesCurrentBinding()
    {
        QTcpServer amp;
        QVERIFY(amp.listen(QHostAddress::LocalHost, 0));
        RadioModel model;
        prepare(model);
        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        const int b = model.addSlice();
        auto* sliceA = model.sliceById(a);
        auto* sliceB = model.sliceById(b);
        QVERIFY(sliceA && sliceB);
        sliceA->setFrequency(14200000);
        sliceB->setFrequency(3700000);
        QString reason;
        QVERIFY(model.setFourO3AEnabledForStation(true, &reason));
        auto* pgxl = model.pgxlConnection();
        QSignalSpy frames(pgxl, &PgxlConnection::testFrameWrittenForTesting);
        // R-R3-47: the Core admits the amp (discovery plus the same serial
        // in its own info reply, as captured) and only then pairs it. The
        // loopback fixture stands in for the amp; nothing real is contacted.
        QSignalSpy paired(pgxl, &PgxlConnection::pairingResult);
        QVERIFY2(model.configurePgxlForStation(QStringLiteral("127.0.0.1"), amp.serverPort(),
                                               &reason), qPrintable(reason));
        QTRY_VERIFY(amp.hasPendingConnections());
        auto* peer = amp.nextPendingConnection();
        peer->write("V3.8.9\n"); peer->flush();
        const auto sequenceOf = [&](const QString& command) -> quint32 {
            const QRegularExpression rx(QStringLiteral("^C(\\d+)\\|") + command);
            for (const auto& row : frames) {
                const auto match = rx.match(row.first().toString());
                if (match.hasMatch()) { return match.captured(1).toUInt(); }
            }
            return 0;
        };
        QTRY_VERIFY(sequenceOf(QStringLiteral("info$")) != 0);
        peer->write(QStringLiteral("R%1|0|serial=10-200/24-0046  version=3.8.9 protocol=1.0 mains=240\n")
                        .arg(sequenceOf(QStringLiteral("info$"))).toUtf8());
        peer->flush();
        auto* controller = model.findChild<StationPgxlController*>();
        QVERIFY(controller);
        QTRY_VERIFY(controller->findChild<LanDiscovery*>() != nullptr);
        controller->findChild<LanDiscovery*>()->injectDatagramForTesting(
            QStringLiteral("PowerGeniusXL ip=127.0.0.1 v=3.8.9 serial=10-200/24-0046 nickname=PowerGeniusXL"),
            amp.serverPort());
        QTRY_VERIFY(pgxl->isConnected());
        QTRY_VERIFY(sequenceOf(QStringLiteral("flexradio ampslice=A serial=")) != 0);
        peer->write(QStringLiteral("R%1|0|\n")
                        .arg(sequenceOf(QStringLiteral("flexradio ampslice=A serial="))).toUtf8());
        peer->flush();
        QTRY_COMPARE(paired.count(), 1);
        QVERIFY(paired.first().first().toBool());
        const auto hasInitialBand = [&] {
            for (const auto& frame : frames) {
                if (frame.first().toString().contains(QStringLiteral("band=14200000"))) { return true; }
            }
            return false;
        };
        QTRY_VERIFY(hasInitialBand());
        frames.clear();
        sliceA->setFrequency(14210000); // schedules old binding's debounce
        QVERIFY(model.txSliceArbiter()->requestHandoff(b));
        sliceB->setFrequency(3750000);
        model.setActiveSlice(a);
        sliceA->setFrequency(14220000); // viewed, unrelated receiver
        const auto bands = [&] {
            QStringList result;
            for (const auto& row : frames) {
                const QString frame = row.first().toString();
                if (frame.contains(QStringLiteral(" band="))) { result.append(frame); }
            }
            return result;
        };
        QTRY_VERIFY_WITH_TIMEOUT(bands().join(QLatin1Char(' ')).contains(QStringLiteral("band=3750000")), 800);
        QTest::qWait(250);
        QVERIFY(!bands().join(QLatin1Char(' ')).contains(QStringLiteral("band=142")));
        QVERIFY(model.setFourO3AEnabledForStation(false, &reason));
    }

    void headlessListenerFollowsBoundSliceAcrossRetuneModeAndRemoval()
    {
        RadioModel model;
        prepare(model);
        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        const int b = model.addSlice();
        QVERIFY(a >= 0 && b >= 0);
        auto* sliceA = model.sliceById(a);
        auto* sliceB = model.sliceById(b);
        sliceA->setFrequency(14200000);
        sliceA->setDspMode(DSPMode::USB);
        sliceB->setFrequency(3700000);
        sliceB->setDspMode(DSPMode::LSB);
        model.setActiveSlice(b); // Looking at B must not change TX-bound A.
        QString reason;
        QVERIFY(model.setFourO3AEnabledForStation(true, &reason));
        QTcpSocket subscriber;
        QByteArray received;
        connect(&subscriber, &QTcpSocket::readyRead, &subscriber,
                [&] { received += subscriber.readAll(); });
        subscriber.connectToHost(QHostAddress::LocalHost, model.smartSdrListener()->serverPort());
        QTRY_COMPARE(subscriber.state(), QAbstractSocket::ConnectedState);
        subscriber.write("C1|sub slice all\n");
        subscriber.flush();
        QTRY_VERIFY(received.contains("RF_frequency=14.200000"));
        QVERIFY(received.contains("mode=USB"));
        received.clear();
        sliceB->setFrequency(3750000);
        QTest::qWait(60);
        QVERIFY(!received.contains("RF_frequency=3.750000"));
        sliceA->setFrequency(14250000);
        QTRY_VERIFY(received.contains("RF_frequency=14.250000"));
        received.clear();
        sliceA->setDspMode(DSPMode::AM);
        QTRY_VERIFY(received.contains("mode=AM"));
        received.clear();
        QVERIFY(model.txSliceArbiter()->requestHandoff(b));
        QTRY_VERIFY(received.contains("RF_frequency=3.750000"));
        QTRY_VERIFY(received.contains("mode=LSB"));
        received.clear();
        sliceA->setFrequency(14300000);
        QTest::qWait(60);
        QVERIFY(!received.contains("RF_frequency=14.300000"));
        model.removeSlice(b);
        QTRY_VERIFY(received.contains("RF_frequency=14.300000"));
        QTRY_VERIFY(received.contains("mode=AM"));
        QVERIFY(model.setFourO3AEnabledForStation(false, &reason));
        QTRY_COMPARE(subscriber.state(), QAbstractSocket::UnconnectedState);
    }

    // R-R3-47: the one Power Genius conversion, from the captured lines.
    // Same arithmetic and transmit gate as MainWindow's former handler.
    void pgxlGaugesFromCapturedLines()
    {
        PgxlConnection conn;
        AmplifierModel amp;
        connect(&conn, &PgxlConnection::statusUpdated, &amp, &AmplifierModel::applyStatusFrame);
        QVERIFY(!amp.present());

        conn.injectLineForTesting(QString::fromLatin1(kPgxlOperate));
        QVERIFY(amp.present());
        QCOMPARE(amp.state(), AmplifierModel::State::Operate);
        QCOMPARE(amp.deviceState(), QStringLiteral("OPERATE"));
        QVERIFY(amp.operate());
        QVERIFY(!amp.transmitting());
        QCOMPARE(amp.temperatureC(), 42.5);
        QCOMPARE(amp.mainsVoltageV(), 240.0);
        // Not transmitting: the latched peak is not shown.
        QCOMPARE(amp.forwardPowerW(), 0.0);
        QCOMPARE(amp.swr(), 1.0);

        conn.injectLineForTesting(QString::fromLatin1(kPgxlTransmit));
        QCOMPARE(amp.state(), AmplifierModel::State::TransmitA);
        QVERIFY(amp.transmitting());
        QVERIFY(std::abs(amp.forwardPowerW() - 1000.0) < 1e-3);
        QVERIFY(std::abs(amp.swr() - 1.12668) < 1e-4);
        QCOMPARE(amp.drainCurrentA(), 22.5);

        // A frame without a state keeps the last state's transmit gate.
        conn.injectLineForTesting(QStringLiteral("S0|status peakfwd=57.0 swr=-20.0"));
        QVERIFY(std::abs(amp.forwardPowerW() - 501.187) < 1e-2);
        QVERIFY(std::abs(amp.swr() - 1.22222) < 1e-4);

        // Leaving transmit reads 0 W and 1.0 even with the latched peak in
        // the same frame; false and zero are values, not gaps.
        conn.injectLineForTesting(QString::fromLatin1(kPgxlStandby));
        QCOMPARE(amp.state(), AmplifierModel::State::Standby);
        QVERIFY(!amp.operate());
        QVERIFY(!amp.transmitting());
        QCOMPARE(amp.forwardPowerW(), 0.0);
        QCOMPARE(amp.swr(), 1.0);
        QCOMPARE(amp.drainCurrentA(), 0.0);

        conn.injectLineForTesting(QString::fromLatin1(kPgxlFault));
        QCOMPARE(amp.state(), AmplifierModel::State::Fault);
        QCOMPARE(amp.temperatureC(), 78.0);
        QCOMPARE(amp.swr(), 1.0);

        conn.injectLineForTesting(QString::fromLatin1(kPgxlSetup));
        QCOMPARE(amp.efficiencyText(), QStringLiteral("off"));

        // The helpers RadioModel's meter and fault paths share.
        QCOMPARE(pgxlReturnLossToSwr(0.0f), 99.0f);
        QCOMPARE(pgxlReturnLossToSwr(2.85f), 99.0f);
        QCOMPARE(pgxlReturnLossToSwr(-0.001f), 99.0f);
        QVERIFY(std::abs(pgxlDbmToWatts(30.0f) - 1.0f) < 1e-6f);
        QVERIFY(pgxlStateIsOperate(QStringLiteral("IDLE")));
        QVERIFY(!pgxlStateIsOperate(QStringLiteral("STANDBY")));
        QCOMPARE(AmplifierModel::stateFromWord(QStringLiteral("FAULT_TEMP")),
                 AmplifierModel::State::Fault);
        QCOMPARE(AmplifierModel::stateFromWord(QStringLiteral("SOMETHING_NEW")),
                 AmplifierModel::State::Unknown);
    }

    // R-R3-47: the Tuner Genius's connection-state shape, driven by the
    // amp's connection. Loopback only; no amp is contacted.
    void amplifierPhaseFollowsItsConnection()
    {
        AppSettings::instance().setValue(QStringLiteral("PGXL_AutoReconnect"),
                                         QStringLiteral("False"));
        QTcpServer ampServer;
        QVERIFY(ampServer.listen(QHostAddress::LocalHost, 0));
        PgxlConnection conn;
        AmplifierModel amp;
        amp.bindConnection(&conn);
        using Phase = TunerModel::ConnectionPhase;
        amp.setAccessoryEnabled(false);
        QCOMPARE(amp.connectionPhase(), Phase::Disabled);
        amp.setAccessoryEnabled(true);
        QCOMPARE(amp.connectionPhase(), Phase::Disconnected);

        conn.connectToPgxl(QStringLiteral("127.0.0.1"), ampServer.serverPort());
        QTRY_VERIFY(ampServer.hasPendingConnections());
        QTcpSocket* device = ampServer.nextPendingConnection();
        device->write("V3.8.9\nR9|0|state=STANDBY temp=30.0 vac=230 id=0.0\n");
        device->flush();
        QTRY_COMPARE(amp.connectionPhase(), Phase::Connected);
        QCOMPARE(amp.configuredHost(), QStringLiteral("127.0.0.1"));
        QCOMPARE(amp.configuredPort(), int(ampServer.serverPort()));
        QCOMPARE(amp.deviceVersion(), QStringLiteral("3.8.9"));
        QTRY_VERIFY(amp.present());
        QCOMPARE(amp.state(), AmplifierModel::State::Standby);

        // The amp goes away: the phase says so, present says the readings
        // are no longer live, and the last readings stay.
        device->close();
        QTRY_COMPARE(amp.connectionPhase(), Phase::Disconnected);
        QVERIFY(!amp.present());
        QCOMPARE(amp.temperatureC(), 30.0);
        QCOMPARE(amp.mainsVoltageV(), 230.0);

        // The station's switch off reads Disabled.
        amp.setAccessoryEnabled(false);
        QCOMPARE(amp.connectionPhase(), Phase::Disabled);
    }

    // R-R3-47: the RF-Kit's readings from its REST replies.
    void rfKitReadingsFromRestReplies()
    {
        Rf2ksConnection conn;
        RfKitModel rfKit;
        rfKit.bindConnection(&conn);
        conn.injectJsonForTesting(QStringLiteral("/info"), kRfKitInfo);
        QCOMPARE(rfKit.deviceModel(), QStringLiteral("RF2K-S"));
        QCOMPARE(rfKit.deviceVersion(), QStringLiteral("G200C267"));
        QCOMPARE(rfKit.deviceNickname(), QStringLiteral("KG4VCF"));
        QVERIFY(!rfKit.present());

        conn.injectJsonForTesting(QStringLiteral("/power"), kRfKitPower);
        conn.injectJsonForTesting(QStringLiteral("/operate-mode"),
                                  R"({"operate_mode":"OPERATE"})");
        QVERIFY(rfKit.present());
        QVERIFY(rfKit.operate());
        QCOMPARE(rfKit.forwardPowerW(), 850.0);
        QCOMPARE(rfKit.reflectedPowerW(), 3.0);
        QVERIFY(std::abs(rfKit.swr() - 1.4) < 1e-6);
        QCOMPARE(rfKit.temperatureC(), 27.0);
        QVERIFY(std::abs(rfKit.voltageV() - 52.7) < 1e-5);
        QCOMPARE(rfKit.currentA(), 0.0);

        // Standby and an idle amp: false and zeros are readings too.
        QSignalSpy changed(&rfKit, &RfKitModel::statusChanged);
        conn.injectJsonForTesting(QStringLiteral("/operate-mode"),
                                  R"({"operate_mode":"STANDBY"})");
        conn.injectJsonForTesting(QStringLiteral("/power"), kRfKitPowerIdle);
        QVERIFY(changed.count() >= 2);
        QVERIFY(!rfKit.operate());
        QCOMPARE(rfKit.forwardPowerW(), 0.0);
        QCOMPARE(rfKit.swr(), 1.0);
        QCOMPARE(rfKit.voltageV(), 0.0);

        using Phase = TunerModel::ConnectionPhase;
        rfKit.setAccessoryEnabled(false);
        QCOMPARE(rfKit.connectionPhase(), Phase::Disabled);
        rfKit.setAccessoryEnabled(true);
        QCOMPARE(rfKit.connectionPhase(), Phase::Disconnected);
    }

    // R-R3-47 / R-R3-22: a current app is offered both objects and the two
    // versions; an older app gets neither, byte for byte what it got
    // before; a Core that does not own its accessories offers nothing; and
    // a write to either object is refused in plain words.
    void coreOffersStatusObjectsOnlyToCurrentApps()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto run = [&](bool owns, quint16 minor, QList<SessionMessage>* out) {
            RadioModel station;
            if (owns) {
                station.enableStationAccessoryIdentity();
            }
            station.setTgxlLanScanWindowMsForTest(50);
            station.setPgxlLanScanWindowMsForTest(50);
            // The Core's amp has sent one reading (the captured operate line).
            station.amplifierModel()->applyStatusFrame(
                {{QStringLiteral("state"), QStringLiteral("OPERATE")},
                 {QStringLiteral("temp"), QStringLiteral("42.5")},
                 {QStringLiteral("vac"), QStringLiteral("240")}});
            AppSettings settings(dir.filePath(QStringLiteral("s-%1-%2.settings")
                                                  .arg(owns).arg(minor)));
            StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
            LoopbackTransport* peer = nullptr;
            connectRawPeer(this, server, minor, &peer);
            QTRY_VERIFY(snapshotDone(peer));
            if (minor >= kRadioIdentitySessionProtocolMinor && owns) {
                peer->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
                    "amplifier",
                    {MirrorUpdate{0, "operate", MirrorWireKind::Bool, QVariant(true)}}, 7)));
                peer->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
                    "rfkit",
                    {MirrorUpdate{0, "operate", MirrorWireKind::Bool, QVariant(true)}}, 8)));
                // R-R3-47: the accessory records change only by command.
                peer->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
                    "accessoryData",
                    {MirrorUpdate{0, "interlockMode", MirrorWireKind::Enum,
                                  QVariant(qlonglong(2))}}, 9)));
                // R-R3-47 / R-R3-22: nor can a device's own settings be
                // written; they change only through their commands.
                peer->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
                    "accessorySettings",
                    {MirrorUpdate{0, "pgxlNickname", MirrorWireKind::Utf8,
                                  QVariant(QStringLiteral("Written"))}}, 10)));
                QTRY_VERIFY([&] {
                    int results = 0;
                    for (const SessionMessage& m : receivedMessages(peer)) {
                        results += m.kind == SessionMessageKind::PropertyResult ? 1 : 0;
                    }
                    return results == 4;
                }());
                QVERIFY(station.accessorySettingsModel()->pgxlNickname().isEmpty());
            }
            *out = receivedMessages(peer);
        };

        QList<SessionMessage> current;
        run(true, kRadioIdentitySessionProtocolMinor, &current);
        StationCapabilities caps;
        bool sawAmplifier = false;
        bool sawRfKit = false;
        bool sawAccessoryData = false;
        bool sawAccessorySettings = false;
        QStringList refusals;
        for (const SessionMessage& m : current) {
            if (m.kind == SessionMessageKind::Capabilities) {
                caps = StationCapabilities::fromUpdates(m.updates);
                const QList<QByteArray> expectedTail{
                    QByteArrayLiteral("remotePgxlControlVersion"),
                    QByteArrayLiteral("remoteRfKitControlVersion"),
                    QByteArrayLiteral("stationTciVersion"),
                    QByteArrayLiteral("accessoryDataVersion"),
                    QByteArrayLiteral("remoteTgxlControlVersion"),
                    QByteArrayLiteral("stationIdentityVersion"),
                    QByteArrayLiteral("deviceAdminVersion"),
                    QByteArrayLiteral("pairingVersion"),
                    QByteArrayLiteral("stationCatalogVersion"),
                    QByteArrayLiteral("displayExtrasVersion"),
                    QByteArrayLiteral("transmitSettingsVersion"),
                    QByteArrayLiteral("bandSelectVersion"),
                    QByteArrayLiteral("meterReadingsVersion"),
                    QByteArrayLiteral("dspInfoVersion"),
                    QByteArrayLiteral("recordStreamVersion"),
                    QByteArrayLiteral("stationRadiosVersion"),
                    QByteArrayLiteral("txDisplayVersion"),
                    QByteArrayLiteral("displayClockVersion"),
                    QByteArrayLiteral("controlChannelVersion"),
                    QByteArrayLiteral("txMonitorAudioVersion"),
                    QByteArrayLiteral("stationFreedvVersion"),
                    QByteArrayLiteral("mediaReplaceVersion"),
                    QByteArrayLiteral("controlSwitchVersion"),
                    QByteArrayLiteral("relayAllowed"),
                    QByteArrayLiteral("supportBundleVersion"),
                    QByteArrayLiteral("mediaTunnelVersion"),
                    QByteArrayLiteral("mediaRelayRoutingVersion"),
                    QByteArrayLiteral("remoteIqVersion"),
                    QByteArrayLiteral("txModMonitorVersion"),
                    QByteArrayLiteral("accessoryTxVersion"),
                };
                QList<QByteArray> actualTail;
                QVERIFY(m.updates.size() >= expectedTail.size());
                for (const auto& update : m.updates.sliced(m.updates.size() - expectedTail.size())) {
                    actualTail.append(update.name);
                }
                QCOMPARE(actualTail, expectedTail);
            }
            if (m.kind == SessionMessageKind::ObjectCreate && m.objectKey == "amplifier") {
                sawAmplifier = true;
                QCOMPARE(m.className, QByteArrayLiteral("AmplifierModel"));
                for (const MirrorUpdate& u : m.updates) {
                    if (u.name == "state") {
                        QCOMPARE(u.kind, MirrorWireKind::Enum);
                        QCOMPARE(u.value.toLongLong(), 4LL);
                    }
                    if (u.name == "temperatureC") {
                        QCOMPARE(u.value.toDouble(), 42.5);
                    }
                }
            }
            if (m.kind == SessionMessageKind::ObjectCreate && m.objectKey == "rfkit") {
                sawRfKit = true;
            }
            if (m.kind == SessionMessageKind::ObjectCreate && m.objectKey == "accessoryData") {
                sawAccessoryData = true;
                QCOMPARE(m.className, QByteArrayLiteral("AccessoryDataModel"));
            }
            if (m.kind == SessionMessageKind::ObjectCreate
                && m.objectKey == "accessorySettings") {
                sawAccessorySettings = true;
                QCOMPARE(m.className, QByteArrayLiteral("AccessorySettingsModel"));
            }
            if (m.kind == SessionMessageKind::PropertyResult) {
                QCOMPARE(m.propertyResults.size(), 1);
                QVERIFY(!m.propertyResults.first().accepted);
                refusals.append(m.propertyResults.first().reason);
            }
        }
        // R-R3-47: 2 once the Core's PGXL commands are offered (Task 2), 3
        // with the amp's own settings (Task 6).
        // R-R3-49 (parity Task 9): 4 with the amp's OPERATE and STANDBY,
        // the Core's LAN scan and the saved address.
        QCOMPARE(caps.remotePgxlControlVersion, 4);
        // R-R3-47: the Tuner Genius's own settings (Task 6); 2 with its
        // antenna, operate and bypass (R-R3-49); 3 when setTgxlOperate on
        // puts the tuner in OPERATE whole (R-R3-49 fix wave); 4 with the
        // relay nudge, the Core's LAN scan and the saved address (parity
        // Task 8).
        QCOMPARE(caps.remoteTgxlControlVersion, 4);
        // R-R3-47: 2 once the Core's RF-Kit commands are offered (Task 3);
        // 3 with Reset amp error (I4); 4 with OPERATE, the antennas, TCI
        // mode and the saved address (R-R3-49, parity Task 10).
        QCOMPARE(caps.remoteRfKitControlVersion, 4);
        QCOMPARE(caps.accessoryTxVersion, 1);
        // R-R3-47: the accessory records and settings (Task 4); 2 with the
        // RF-Kit's connection counts (R-R3-49, parity Task 10).
        QCOMPARE(caps.accessoryDataVersion, 3);  // 3: rfkitRttAvgMs (group B fix wave, M7)
        QVERIFY(sawAmplifier);
        QVERIFY(sawRfKit);
        QVERIFY(sawAccessoryData);
        QVERIFY(sawAccessorySettings);
        refusals.sort();
        // R-R3-25 / R-R3-47: the Core is receive-only (StationServer sets
        // it), so the amp's `operate` gets the receive-only reason.
        QStringList expected{AmplifierModel::receiveOnlyOperateReason(),
                             RfKitModel::readOnlyReason(),
                             AccessoryDataModel::readOnlyReason(),
                             AccessorySettingsModel::readOnlyReason()};
        expected.sort();
        QCOMPARE(refusals, expected);

        // An older app: neither object, neither version.
        QList<SessionMessage> older;
        run(true, quint16(kRadioIdentitySessionProtocolMinor - 1), &older);
        for (const SessionMessage& m : older) {
            QVERIFY2(!namesAccessory(m), m.objectKey.constData());
            for (const MirrorUpdate& u : m.updates) {
                QVERIFY(u.name != "remotePgxlControlVersion");
                QVERIFY(u.name != "remoteRfKitControlVersion");
                QVERIFY(u.name != "stationTciVersion");
                QVERIFY(u.name != "accessoryDataVersion");
                QVERIFY(u.name != "remoteTgxlControlVersion");
            }
        }

        // A Core that does not own its accessories: version 0, no objects.
        QList<SessionMessage> notOwning;
        run(false, kRadioIdentitySessionProtocolMinor, &notOwning);
        for (const SessionMessage& m : notOwning) {
            QVERIFY(!namesAccessory(m));
            if (m.kind == SessionMessageKind::Capabilities) {
                const StationCapabilities c = StationCapabilities::fromUpdates(m.updates);
                QCOMPARE(c.remotePgxlControlVersion, 0);
                QCOMPARE(c.remoteRfKitControlVersion, 0);
                QCOMPARE(c.accessoryDataVersion, 0);
                QCOMPARE(c.remoteTgxlControlVersion, 0);
            }
        }
    }

    // R-R3-47 / R-R3-22: the verbs for the amp's and tuner's own settings
    // need their versions (an older app, and a Core that does not own its
    // accessories, are refused in plain words); a malformed request is not
    // understood; with no device the Core says so; nothing reaches a device.
    void deviceSettingsVerbsNeedTheirVersions()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto results = [&](bool owns, quint16 minor,
                                 const QList<SessionMessage>& invokes) {
            RadioModel station;
            if (owns) {
                station.enableStationAccessoryIdentity();
            }
            station.setTgxlLanScanWindowMsForTest(50);
            station.setPgxlLanScanWindowMsForTest(50);
            AppSettings settings(dir.filePath(QStringLiteral("v-%1-%2.settings")
                                                  .arg(owns).arg(minor)));
            StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
            LoopbackTransport* peer = nullptr;
            connectRawPeer(this, server, minor, &peer);
            [&] { QTRY_VERIFY(snapshotDone(peer)); }();
            for (const SessionMessage& invoke : invokes) {
                peer->sendText(SessionMessages::encode(invoke));
            }
            QStringList reasons;
            // The comparison below says what was missing if one never came.
            (void)QTest::qWaitFor([&] {
                reasons.clear();
                for (const SessionMessage& m : receivedMessages(peer)) {
                    if (m.kind == SessionMessageKind::CommandResult) {
                        reasons.append(m.accepted ? QStringLiteral("accepted") : m.reason);
                    }
                }
                return reasons.size() == invokes.size();
            }, 3000);
            return reasons;
        };
        const MirrorUpdate name{0, "name", MirrorWireKind::Utf8, QVariant(QStringLiteral("Amp"))};
        const QList<SessionMessage> invokes{
            SessionMessages::commandInvoke("setPgxlName", 21, {name}),
            SessionMessages::commandInvoke("saveTgxlSettings", 22, {}),
            SessionMessages::commandInvoke("resetRfKitError", 25, {}),
        };

        QCOMPARE(results(true, quint16(kRadioIdentitySessionProtocolMinor - 1), invokes),
                 (QStringList{
                     QStringLiteral("Update this app to change the Power Genius's own "
                                    "settings on this Core."),
                     QStringLiteral("Update this app to change the Tuner Genius's own "
                                    "settings on this Core."),
                     QStringLiteral("Update this app to reset the RF-Kit amplifier's error on "
                                    "this Core.")}));
        QCOMPARE(results(false, kRadioIdentitySessionProtocolMinor, invokes),
                 (QStringList{QStringLiteral("This Core cannot change its amplifier and tuner "
                                             "settings."),
                              QStringLiteral("This Core cannot change its amplifier and tuner "
                                             "settings."),
                              QStringLiteral("This Core cannot reset its RF-Kit amplifier's "
                                             "error.")}));
        // I4: Reset amp error with no amp admitted, or with arguments.
        const QStringList rfkit = results(true, kRadioIdentitySessionProtocolMinor, {
            SessionMessages::commandInvoke("resetRfKitError", 26, {}),
            SessionMessages::commandInvoke("resetRfKitError", 27, {name}),
        });
        QCOMPARE(rfkit, (QStringList{
            QStringLiteral("The Core is not connected to the RF-Kit amplifier."),
            QStringLiteral("The request to reset the RF-Kit amplifier's error was not "
                           "understood.")}));
        const QStringList current = results(true, kRadioIdentitySessionProtocolMinor, {
            SessionMessages::commandInvoke("setPgxlName", 21, {name}),
            SessionMessages::commandInvoke("readTgxlSettings", 22, {}),
            SessionMessages::commandInvoke(
                "setPgxlHardware", 23,
                {MirrorUpdate{0, "fanMode", MirrorWireKind::Utf8, QVariant(QStringLiteral("Auto"))},
                 MirrorUpdate{0, "ledIntensity", MirrorWireKind::Int64, QVariant(qlonglong(5))}}),
            SessionMessages::commandInvoke(
                "setTgxlNetwork", 24,
                {MirrorUpdate{0, "dhcp", MirrorWireKind::Bool, QVariant(true)}}),
        });
        QCOMPARE(current, (QStringList{
            QStringLiteral("The Core is not connected to the Power Genius."),
            QStringLiteral("The Core is not connected to the Tuner Genius."),
            QStringLiteral("The request to change the Power Genius hardware was not understood."),
            QStringLiteral("The request to change the Tuner Genius network settings was not "
                           "understood.")}));
        for (const QString& reason : current) {
            QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        }
    }

    // R-R3-49 / R-R3-47 (remoteTgxlControlVersion 2): the Tuner Genius's
    // antenna, operate and bypass on the wire. An older app, and a Core
    // that does not own its accessories, are refused in plain words; a
    // malformed request is not understood; a port outside 1 to 3 is
    // refused; with no tuner the Core says so; on the air it says so, on a
    // receive-only Core as well; nothing reaches a tuner.
    void tunerSwitchingVerbsNeedTheirVersionAndAQuietRadio()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto results = [&](bool owns, quint16 minor, bool onAir,
                                 const QList<SessionMessage>& invokes) {
            RadioModel station;
            if (owns) {
                station.enableStationAccessoryIdentity();
            }
            station.setTgxlLanScanWindowMsForTest(50);
            station.setPgxlLanScanWindowMsForTest(50);
            AppSettings settings(dir.filePath(QStringLiteral("t-%1-%2-%3.settings")
                                                  .arg(owns).arg(minor).arg(onAir)));
            StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
            if (onAir) {
                station.transmitModel().setMox(true);
            }
            LoopbackTransport* peer = nullptr;
            connectRawPeer(this, server, minor, &peer);
            [&] { QTRY_VERIFY(snapshotDone(peer)); }();
            for (const SessionMessage& invoke : invokes) {
                peer->sendText(SessionMessages::encode(invoke));
            }
            QStringList reasons;
            // The comparison below says what was missing if one never came.
            (void)QTest::qWaitFor([&] {
                reasons.clear();
                for (const SessionMessage& m : receivedMessages(peer)) {
                    if (m.kind == SessionMessageKind::CommandResult) {
                        reasons.append(m.accepted ? QStringLiteral("accepted") : m.reason);
                    }
                }
                return reasons.size() == invokes.size();
            }, 3000);
            return reasons;
        };
        const auto intArg = [](const char* name, qlonglong v) {
            return MirrorUpdate{0, name, MirrorWireKind::Int64, QVariant(v)};
        };
        const auto boolArg = [](const char* name, bool v) {
            return MirrorUpdate{0, name, MirrorWireKind::Bool, QVariant(v)};
        };
        const QList<SessionMessage> right{
            SessionMessages::commandInvoke("setTgxlAntenna", 31, {intArg("port", 2)}),
            SessionMessages::commandInvoke("setTgxlOperate", 32, {boolArg("on", true)}),
            SessionMessages::commandInvoke("setTgxlBypass", 33, {boolArg("on", false)}),
        };
        const QString update = QStringLiteral("Update this app to switch the Tuner Genius on "
                                              "this Core.");
        const QString receiveOnly = QStringLiteral("This Core is set to receive only.");
        QCOMPARE(results(true, quint16(kRadioIdentitySessionProtocolMinor - 1), false, right),
                 (QStringList{receiveOnly, receiveOnly, receiveOnly}));
        const QString notOwning = QStringLiteral("This Core cannot change its amplifier and "
                                                 "tuner settings.");
        QCOMPARE(results(false, kRadioIdentitySessionProtocolMinor, false, right),
                 (QStringList{receiveOnly, receiveOnly, receiveOnly}));
        const QString noTuner = QStringLiteral("The Core is not connected to the Tuner Genius.");
        QCOMPARE(results(true, kRadioIdentitySessionProtocolMinor, false, right),
                 (QStringList{receiveOnly, receiveOnly, receiveOnly}));
        const QString onAir = QStringLiteral("The radio is on the air. Try again when it stops.");
        QCOMPARE(results(true, kRadioIdentitySessionProtocolMinor, true, right),
                 (QStringList{receiveOnly, receiveOnly, receiveOnly}));
        const QStringList wrong = results(true, kRadioIdentitySessionProtocolMinor, false, {
            SessionMessages::commandInvoke("setTgxlAntenna", 41, {intArg("port", 4)}),
            SessionMessages::commandInvoke("setTgxlAntenna", 42, {boolArg("port", true)}),
            SessionMessages::commandInvoke("setTgxlOperate", 43, {intArg("on", 1)}),
            SessionMessages::commandInvoke("setTgxlBypass", 44, {}),
        });
        QCOMPARE(wrong, (QStringList{
            receiveOnly,
            QStringLiteral("The request to switch the Tuner Genius antenna was not understood."),
            QStringLiteral("The request to put the Tuner Genius in operate or standby was not "
                           "understood."),
            QStringLiteral("The request to bypass the Tuner Genius was not understood.")}));
        for (const QString& reason : wrong + QStringList{update, notOwning, noTuner, onAir}) {
            QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        }
    }

    // R-R3-49 (parity Task 8, remoteTgxlControlVersion 4): the relay
    // nudge, the Core's LAN scan and the saved address on the wire. An older
    // app, and a Core that does not own its accessories, are refused in
    // bfab2b9e's words; a malformed request is not understood; the nudge
    // with no tuner and the address with no radio say so; on the air the
    // nudge is refused, on a receive-only Core as well, while the scan and
    // the address are taken (parity mini-round, the operator's rulings a
    // and b: they only listen or save).
    void tunerRelayScanAndAddressVerbsNeedVersionFourAndAQuietRadio()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto results = [&](bool owns, quint16 minor, bool onAir,
                                 const QList<SessionMessage>& invokes) {
            RadioModel station;
            if (owns) {
                station.enableStationAccessoryIdentity();
            }
            station.setTgxlLanScanWindowMsForTest(50);
            station.setPgxlLanScanWindowMsForTest(50);
            AppSettings settings(dir.filePath(QStringLiteral("r-%1-%2-%3.settings")
                                                  .arg(owns).arg(minor).arg(onAir)));
            StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
            if (onAir) {
                station.transmitModel().setMox(true);
            }
            LoopbackTransport* peer = nullptr;
            connectRawPeer(this, server, minor, &peer);
            [&] { QTRY_VERIFY(snapshotDone(peer)); }();
            for (const SessionMessage& invoke : invokes) {
                peer->sendText(SessionMessages::encode(invoke));
            }
            QStringList reasons;
            (void)QTest::qWaitFor([&] {
                reasons.clear();
                for (const SessionMessage& m : receivedMessages(peer)) {
                    if (m.kind == SessionMessageKind::CommandResult) {
                        reasons.append(m.accepted ? QStringLiteral("accepted") : m.reason);
                    }
                }
                return reasons.size() == invokes.size();
            }, 3000);
            return reasons;
        };
        const auto intArg = [](const char* name, qlonglong v) {
            return MirrorUpdate{0, name, MirrorWireKind::Int64, QVariant(v)};
        };
        const auto textArg = [](const char* name, const QString& v) {
            return MirrorUpdate{0, name, MirrorWireKind::Utf8, QVariant(v)};
        };
        const QList<SessionMessage> right{
            SessionMessages::commandInvoke("moveTgxlRelay", 51,
                                           {intArg("relay", 0), intArg("direction", 1)}),
            SessionMessages::commandInvoke("scanTgxlLan", 52, {}),
            SessionMessages::commandInvoke("setTgxlAddress", 53,
                                           {textArg("host", QStringLiteral("192.0.2.9")),
                                            intArg("port", 9010)}),
        };
        const QString update = QStringLiteral("Update this app to switch the Tuner Genius on "
                                              "this Core.");
        QCOMPARE(results(true, quint16(kRadioIdentitySessionProtocolMinor - 1), false, right),
                 (QStringList{update, update, update}));
        const QString notOwning = QStringLiteral("This Core cannot change its amplifier and "
                                                 "tuner settings.");
        QCOMPARE(results(false, kRadioIdentitySessionProtocolMinor, false, right),
                 (QStringList{notOwning, notOwning, notOwning}));
        const QString onAir = QStringLiteral("The radio is on the air. Try again when it stops.");
        const QString noTuner = QStringLiteral("The Core is not connected to the Tuner Genius.");
        const QString noRadio = QStringLiteral("Connect the Core to a radio before setting up its "
                                               "Tuner Genius XL.");
        // The scan answers when its window ends, after the other two.
        QCOMPARE(results(true, kRadioIdentitySessionProtocolMinor, true, right),
                 (QStringList{onAir, noRadio, QStringLiteral("accepted")}));
        const QStringList wrong = results(true, kRadioIdentitySessionProtocolMinor, false, {
            right.at(0),
            right.at(2),
            SessionMessages::commandInvoke("moveTgxlRelay", 61,
                                           {intArg("relay", 3), intArg("direction", 1)}),
            SessionMessages::commandInvoke("moveTgxlRelay", 62,
                                           {intArg("relay", 0), intArg("direction", 0)}),
            SessionMessages::commandInvoke("moveTgxlRelay", 63, {intArg("relay", 0)}),
            SessionMessages::commandInvoke("scanTgxlLan", 64, {intArg("seconds", 3)}),
            SessionMessages::commandInvoke("setTgxlAddress", 65,
                                           {textArg("host", QStringLiteral("192.0.2.9")),
                                            textArg("port", QStringLiteral("9010"))}),
        });
        const QString relayNotUnderstood =
            QStringLiteral("The request to move a Tuner Genius relay was not understood.");
        QCOMPARE(wrong, (QStringList{
            noTuner, noRadio, relayNotUnderstood, relayNotUnderstood, relayNotUnderstood,
            QStringLiteral("The request to scan for a Tuner Genius was not understood."),
            QStringLiteral("The request to save the Tuner Genius address was not understood.")}));
        for (const QString& reason : wrong + QStringList{update, notOwning, onAir}) {
            QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        }
    }

    // R-R3-49 (parity Task 9, remotePgxlControlVersion 4): the Power
    // Genius's OPERATE and STANDBY, the Core's LAN scan and the saved
    // address on the wire, refused as the Tuner Genius's are.
    void ampOperateScanAndAddressVerbsNeedVersionFourAndAQuietRadio()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto results = [&](bool owns, quint16 minor, bool onAir,
                                 const QList<SessionMessage>& invokes) {
            RadioModel station;
            if (owns) {
                station.enableStationAccessoryIdentity();
            }
            station.setTgxlLanScanWindowMsForTest(50);
            station.setPgxlLanScanWindowMsForTest(50);
            AppSettings settings(dir.filePath(QStringLiteral("p-%1-%2-%3.settings")
                                                  .arg(owns).arg(minor).arg(onAir)));
            StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
            if (onAir) {
                station.transmitModel().setMox(true);
            }
            LoopbackTransport* peer = nullptr;
            connectRawPeer(this, server, minor, &peer);
            [&] { QTRY_VERIFY(snapshotDone(peer)); }();
            for (const SessionMessage& invoke : invokes) {
                peer->sendText(SessionMessages::encode(invoke));
            }
            QStringList reasons;
            (void)QTest::qWaitFor([&] {
                reasons.clear();
                for (const SessionMessage& m : receivedMessages(peer)) {
                    if (m.kind == SessionMessageKind::CommandResult) {
                        reasons.append(m.accepted ? QStringLiteral("accepted") : m.reason);
                    }
                }
                return reasons.size() == invokes.size();
            }, 3000);
            return reasons;
        };
        const auto intArg = [](const char* name, qlonglong v) {
            return MirrorUpdate{0, name, MirrorWireKind::Int64, QVariant(v)};
        };
        const auto textArg = [](const char* name, const QString& v) {
            return MirrorUpdate{0, name, MirrorWireKind::Utf8, QVariant(v)};
        };
        const auto boolArg = [](const char* name, bool v) {
            return MirrorUpdate{0, name, MirrorWireKind::Bool, QVariant(v)};
        };
        const QList<SessionMessage> right{
            SessionMessages::commandInvoke("setPgxlOperate", 71, {boolArg("on", true)}),
            SessionMessages::commandInvoke("scanPgxlLan", 72, {}),
            SessionMessages::commandInvoke("setPgxlAddress", 73,
                                           {textArg("host", QStringLiteral("192.0.2.9")),
                                            intArg("port", 9008)}),
        };
        const QString update = QStringLiteral("Update this app to switch the Power Genius on "
                                              "this Core.");
        const QString receiveOnly = QStringLiteral("This Core is set to receive only.");
        QCOMPARE(results(true, quint16(kRadioIdentitySessionProtocolMinor - 1), false, right),
                 (QStringList{receiveOnly, update, update}));
        const QString notOwning = QStringLiteral("This Core cannot change its amplifier and "
                                                 "tuner settings.");
        QCOMPARE(results(false, kRadioIdentitySessionProtocolMinor, false, right),
                 (QStringList{receiveOnly, notOwning, notOwning}));
        const QString onAir = QStringLiteral("The radio is on the air. Try again when it stops.");
        const QString noAmp = QStringLiteral("The Core is not connected to the Power Genius.");
        const QString noRadio = QStringLiteral("Connect the Core to a radio before setting up its "
                                               "Power Genius.");
        // Parity mini-round (rulings a and b): OPERATE waits on the air; the
        // scan (answered when its window ends) and the address do not.
        QCOMPARE(results(true, kRadioIdentitySessionProtocolMinor, true, right),
                 (QStringList{receiveOnly, noRadio, QStringLiteral("accepted")}));
        const QStringList wrong = results(true, kRadioIdentitySessionProtocolMinor, false, {
            right.at(0),
            right.at(2),
            SessionMessages::commandInvoke("setPgxlOperate", 81, {intArg("on", 1)}),
            SessionMessages::commandInvoke("setPgxlOperate", 82, {}),
            SessionMessages::commandInvoke("scanPgxlLan", 83, {intArg("seconds", 3)}),
            SessionMessages::commandInvoke("setPgxlAddress", 84,
                                           {textArg("host", QStringLiteral("192.0.2.9")),
                                            textArg("port", QStringLiteral("9008"))}),
        });
        const QString operateNotUnderstood = QStringLiteral(
            "The request to put the Power Genius in operate or standby was not understood.");
        QCOMPARE(wrong, (QStringList{
            receiveOnly, noRadio, operateNotUnderstood, operateNotUnderstood,
            QStringLiteral("The request to scan for a Power Genius was not understood."),
            QStringLiteral("The request to save the Power Genius address was not understood.")}));
        for (const QString& reason : wrong + QStringList{update, notOwning, onAir}) {
            QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        }
    }

    // R-R3-49 (parity Task 10): the RF-Kit's OPERATE, antenna, TCI mode and
    // saved address on the wire: refused below minor 11, on a Core that
    // does not own its accessories, on the air (the switches; the address
    // is taken there, parity mini-round), and with the wrong arguments;
    // nothing reaches an amp.
    void rfKitOperateAntennaTciAndAddressVerbsNeedVersionFourAndAQuietRadio()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto results = [&](bool owns, quint16 minor, bool onAir,
                                 const QList<SessionMessage>& invokes) {
            RadioModel station;
            if (owns) {
                station.enableStationAccessoryIdentity();
            }
            station.setTgxlLanScanWindowMsForTest(50);
            station.setPgxlLanScanWindowMsForTest(50);
            AppSettings settings(dir.filePath(QStringLiteral("r-%1-%2-%3.settings")
                                                  .arg(owns).arg(minor).arg(onAir)));
            StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
            if (onAir) {
                station.transmitModel().setMox(true);
            }
            LoopbackTransport* peer = nullptr;
            connectRawPeer(this, server, minor, &peer);
            [&] { QTRY_VERIFY(snapshotDone(peer)); }();
            for (const SessionMessage& invoke : invokes) {
                peer->sendText(SessionMessages::encode(invoke));
            }
            QStringList reasons;
            (void)QTest::qWaitFor([&] {
                reasons.clear();
                for (const SessionMessage& m : receivedMessages(peer)) {
                    if (m.kind == SessionMessageKind::CommandResult) {
                        reasons.append(m.accepted ? QStringLiteral("accepted") : m.reason);
                    }
                }
                return reasons.size() == invokes.size();
            }, 3000);
            return reasons;
        };
        const auto intArg = [](const char* name, qlonglong v) {
            return MirrorUpdate{0, name, MirrorWireKind::Int64, QVariant(v)};
        };
        const auto textArg = [](const char* name, const QString& v) {
            return MirrorUpdate{0, name, MirrorWireKind::Utf8, QVariant(v)};
        };
        const auto boolArg = [](const char* name, bool v) {
            return MirrorUpdate{0, name, MirrorWireKind::Bool, QVariant(v)};
        };
        const QList<SessionMessage> right{
            SessionMessages::commandInvoke("setRfKitOperate", 71, {boolArg("on", true)}),
            SessionMessages::commandInvoke("setRfKitAntenna", 72, {intArg("port", 2)}),
            SessionMessages::commandInvoke("setRfKitTciMode", 73, {}),
            SessionMessages::commandInvoke("setRfKitAddress", 74,
                                           {textArg("host", QStringLiteral("192.0.2.9")),
                                            intArg("port", 8080)}),
        };
        const QString update = QStringLiteral("Update this app to switch the RF-Kit amplifier on "
                                              "this Core.");
        const QString receiveOnly = QStringLiteral("This Core is set to receive only.");
        QCOMPARE(results(true, quint16(kRadioIdentitySessionProtocolMinor - 1), false, right),
                 (QStringList{update, update, update, update}));
        const QString notOwning = QStringLiteral("This Core cannot change its amplifier and "
                                                 "tuner settings.");
        QCOMPARE(results(false, kRadioIdentitySessionProtocolMinor, false, right),
                 (QStringList{notOwning, notOwning, notOwning, notOwning}));
        const QString onAir = QStringLiteral("The radio is on the air. Try again when it stops.");
        const QString noAmp = QStringLiteral("The Core is not connected to the RF-Kit amplifier.");
        const QString noRadio = QStringLiteral("Connect the Core to a radio before setting up its "
                                               "RF-Kit amplifier.");
        // Parity mini-round (rulings a and b): the three switches wait on
        // the air; the address does not (it only saves).
        QCOMPARE(results(true, kRadioIdentitySessionProtocolMinor, true, right),
                 (QStringList{receiveOnly, receiveOnly, onAir, noRadio}));
        const QStringList wrong = results(true, kRadioIdentitySessionProtocolMinor, false, {
            right.at(0),
            right.at(1),
            right.at(2),
            right.at(3),
            SessionMessages::commandInvoke("setRfKitOperate", 81, {intArg("on", 1)}),
            SessionMessages::commandInvoke("setRfKitOperate", 82, {}),
            SessionMessages::commandInvoke("setRfKitAntenna", 83,
                                           {textArg("port", QStringLiteral("2"))}),
            SessionMessages::commandInvoke("setRfKitAntenna", 84, {intArg("port", 5)}),
            SessionMessages::commandInvoke("setRfKitTciMode", 85,
                                           {textArg("mode", QStringLiteral("TCI"))}),
            SessionMessages::commandInvoke("setRfKitAddress", 86,
                                           {textArg("host", QStringLiteral("192.0.2.9")),
                                            textArg("port", QStringLiteral("8080"))}),
        });
        const QString operateNotUnderstood = QStringLiteral(
            "The request to put the RF-Kit amplifier in operate or standby was not understood.");
        QCOMPARE(wrong, (QStringList{
            receiveOnly, receiveOnly, noAmp, noRadio, operateNotUnderstood, operateNotUnderstood,
            QStringLiteral("The request to switch the RF-Kit amplifier's antenna was not "
                           "understood."),
            receiveOnly,
            QStringLiteral("The request to put the RF-Kit amplifier in TCI mode was not "
                           "understood."),
            QStringLiteral("The request to save the RF-Kit amplifier address was not "
                           "understood.")}));
        for (const QString& reason : wrong + QStringList{update, notOwning, onAir}) {
            QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        }
    }

    // R-R3-47: the control document's fixtures are what the objects send.
    // Each file is a list of session messages, one JSON object per line:
    // the schema, the object, the snapshot marker, then deltas.
    void amplifierFixtureIsWhatTheCoreSends()
    {
        PgxlConnection conn;
        AmplifierModel amp;
        connect(&conn, &PgxlConnection::statusUpdated, &amp, &AmplifierModel::applyStatusFrame);
        TunerModel::StationConnectionState state;
        state.configuredHost = QStringLiteral("192.0.2.40");
        state.configuredPort = 9008;
        state.phase = TunerModel::ConnectionPhase::Connected;
        state.deviceVersion = QStringLiteral("3.8.9");
        amp.setStationConnectionState(state);
        conn.injectLineForTesting(QString::fromLatin1(kPgxlOperate));
        conn.injectLineForTesting(QString::fromLatin1(kPgxlSetup));

        MirrorRecorder recorder("amplifier", &amp);
        conn.injectLineForTesting(QString::fromLatin1(kPgxlTransmit));
        recorder.flush();
        conn.injectLineForTesting(QString::fromLatin1(kPgxlStandby));
        recorder.flush();
        // R-R3-48: paired with the radio, the amp follows its band.
        amp.setBandFollow(TunerModel::BandFollow::Following);
        recorder.flush();
        QString why;
        QVERIFY2(matchesFixture(QStringLiteral("amplifier.jsonl"), recorder.text(), &why),
                 qPrintable(why));

        // The fixture parses, and a remote window's object applies it.
        QFile file(fixturePath(QStringLiteral("amplifier.jsonl")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QList<SessionMessage> messages = decodeLines(file.readAll());
        QCOMPARE(messages.size(), 6);
        QCOMPARE(messages.at(0).kind, SessionMessageKind::Schema);
        QCOMPARE(messages.at(1).kind, SessionMessageKind::ObjectCreate);
        QCOMPARE(messages.at(2).kind, SessionMessageKind::SnapshotComplete);
        AmplifierModel window;
        for (const SessionMessage& m : messages) {
            for (const MirrorUpdate& u : m.updates) {
                QVERIFY2(window.applyStationValue(u.name, u.value), u.name.constData());
            }
        }
        QCOMPARE(window.connectionPhase(), TunerModel::ConnectionPhase::Connected);
        QCOMPARE(window.configuredHost(), QStringLiteral("192.0.2.40"));
        QCOMPARE(window.state(), AmplifierModel::State::Standby);
        QCOMPARE(window.forwardPowerW(), 0.0);
        QCOMPARE(window.efficiencyText(), QStringLiteral("off"));
        QCOMPARE(window.bandFollow(), TunerModel::BandFollow::Following);
    }

    void rfKitFixtureIsWhatTheCoreSends()
    {
        Rf2ksConnection conn;
        RfKitModel rfKit;
        rfKit.bindConnection(&conn);
        TunerModel::StationConnectionState state;
        state.configuredHost = QStringLiteral("192.0.2.41");
        state.configuredPort = 8080;
        state.phase = TunerModel::ConnectionPhase::Connected;
        rfKit.setStationConnectionState(state);
        conn.injectJsonForTesting(QStringLiteral("/info"), kRfKitInfo);
        conn.injectJsonForTesting(QStringLiteral("/power"), kRfKitPower);
        conn.injectJsonForTesting(QStringLiteral("/operate-mode"),
                                  R"({"operate_mode":"OPERATE"})");
        // R-R3-47: the interface, antenna and tuner rows (bodies as in
        // tst_rf2ks_connection_parse).
        conn.injectJsonForTesting(QStringLiteral("/operational-interface"),
                                  R"({"operational_interface":"UDP","error":""})");
        conn.injectJsonForTesting(QStringLiteral("/antennas"),
            R"({"antennas":[{"type":"INTERNAL","number":1,"state":"ACTIVE"},{"type":"INTERNAL","number":2,"state":"AVAILABLE"},{"type":"INTERNAL","number":3,"state":"DISABLED"},{"type":"EXTERNAL","state":"AVAILABLE"}]})");
        conn.injectJsonForTesting(QStringLiteral("/antennas/active"),
                                  R"({"type":"INTERNAL","number":1})");

        MirrorRecorder recorder("rfkit", &rfKit);
        conn.injectJsonForTesting(QStringLiteral("/operate-mode"),
                                  R"({"operate_mode":"STANDBY"})");
        conn.injectJsonForTesting(QStringLiteral("/power"), kRfKitPowerIdle);
        recorder.flush();
        // R-R3-47 / R-R3-48: a tune lands, the amp is switched to TCI and
        // the band-follow line names the address to enter on it.
        conn.injectJsonForTesting(QStringLiteral("/tuner"),
            R"({"mode":"AUTO","setup":"LC","L":{"value":1200,"unit":"nH"},"C":{"value":345,"unit":"pF"},"tuned_frequency":{"value":3891,"unit":"kHz"},"segment_size":{"value":9,"unit":"kHz"}})");
        conn.injectJsonForTesting(QStringLiteral("/operational-interface"),
                                  R"({"operational_interface":"TCI","error":""})");
        conn.injectJsonForTesting(QStringLiteral("/antennas/active"),
                                  R"({"type":"INTERNAL","number":2})");
        rfKit.setBandFollow(TunerModel::BandFollow::Waiting, QStringLiteral("192.0.2.10"), 50001);
        recorder.flush();
        QString why;
        QVERIFY2(matchesFixture(QStringLiteral("rfkit.jsonl"), recorder.text(), &why),
                 qPrintable(why));

        QFile file(fixturePath(QStringLiteral("rfkit.jsonl")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QList<SessionMessage> messages = decodeLines(file.readAll());
        QCOMPARE(messages.size(), 5);
        RfKitModel window;
        for (const SessionMessage& m : messages) {
            for (const MirrorUpdate& u : m.updates) {
                QVERIFY2(window.applyStationValue(u.name, u.value), u.name.constData());
            }
        }
        QCOMPARE(window.deviceModel(), QStringLiteral("RF2K-S"));
        QVERIFY(window.present());
        QVERIFY(!window.operate());
        QCOMPARE(window.forwardPowerW(), 0.0);
        QCOMPARE(window.operationalInterface(), QStringLiteral("TCI"));
        QCOMPARE(window.antennaPresentMask(), 0b0111);
        QCOMPARE(window.antennaDisabledMask(), 0b0100);
        QCOMPARE(window.activeAntennaNumber(), 2);
        QVERIFY(!window.activeAntennaExternal());
        QCOMPARE(window.tunerMode(), RfKitModel::TunerMode::Auto);
        QCOMPARE(window.tunerInductanceNh(), 1200);
        QCOMPARE(window.tunerCapacitancePf(), 345);
        QCOMPARE(window.tunerFrequencyKhz(), 3891);
        QCOMPARE(window.tunerSegmentKhz(), 9);
        QCOMPARE(window.tunerSetup(), QStringLiteral("LC"));
        QCOMPARE(window.bandFollow(), TunerModel::BandFollow::Waiting);
        QCOMPARE(window.bandFollowAddress(), QStringLiteral("192.0.2.10"));
        QCOMPARE(window.bandFollowPort(), 50001);
        QCOMPARE(window.bandFollowText(),
                 QStringLiteral("Band follow: enter 192.0.2.10, port 50001 as the TCI server on "
                                "the amplifier."));
    }

    // The fixed enum values, as the document and enums.json list them.
    void enumFixtureMatchesTheEnums()
    {
        QFile file(fixturePath(QStringLiteral("enums.json")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
        const auto check = [&](const char* name, const QMetaEnum& meta) {
            const QJsonObject values = root.value(QLatin1String(name)).toObject();
            QCOMPARE(values.size(), meta.keyCount());
            for (int i = 0; i < meta.keyCount(); ++i) {
                QString key = QString::fromLatin1(meta.key(i));
                key[0] = key[0].toLower();
                QVERIFY2(values.contains(key), qPrintable(key));
                QCOMPARE(values.value(key).toInt(-1), meta.value(i));
            }
        };
        check("connectionPhase", QMetaEnum::fromType<TunerModel::ConnectionPhase>());
        check("amplifierState", QMetaEnum::fromType<AmplifierModel::State>());
        // R-R3-47 / R-R3-48: band follow and the RF-Kit tuner's mode.
        check("bandFollow", QMetaEnum::fromType<TunerModel::BandFollow>());
        check("rfkitTunerMode", QMetaEnum::fromType<RfKitModel::TunerMode>());
        // R-R3-47 / R-R3-22: the interlock mode on `accessoryData`.
        check("interlockMode", QMetaEnum::fromType<AccessoryDataModel::InterlockMode>());
        const QJsonObject reasons = root.value(QLatin1String("refusals")).toObject();
        QCOMPARE(reasons.value(QLatin1String("amplifierReadOnly")).toString(),
                 AmplifierModel::readOnlyReason());
        QCOMPARE(reasons.value(QLatin1String("rfkitReadOnly")).toString(),
                 RfKitModel::readOnlyReason());
        QCOMPARE(reasons.value(QLatin1String("stationTciReadOnly")).toString(),
                 StationTciModel::readOnlyReason());
        QCOMPARE(reasons.value(QLatin1String("accessoryDataReadOnly")).toString(),
                 AccessoryDataModel::readOnlyReason());
        // R-R3-47 / R-R3-22: the devices' own settings on `accessorySettings`.
        QCOMPARE(reasons.value(QLatin1String("accessorySettingsReadOnly")).toString(),
                 AccessorySettingsModel::readOnlyReason());
        QVERIFY(OperatorWording::isPlain(AccessorySettingsModel::readOnlyReason()));
    }

    // R-R3-47 / R-R3-22: the accessorySettings fixture is what the Core
    // sends: the amp's settings as it last reported them at attach; then
    // the amp's answer to a new name, and the tuner's network settings.
    void accessorySettingsFixtureIsWhatTheCoreSends()
    {
        AccessorySettingsModel settings;
        AccessorySettingsModel::Device amp;
        amp.nickname = QStringLiteral("Shack PGXL");
        amp.biasMode = QStringLiteral("ClassAB");
        amp.fanMode = QStringLiteral("Auto");
        amp.ledIntensity = 75;
        amp.networkKnown = true;
        amp.dhcp = false;
        amp.address = QStringLiteral("192.168.1.50");
        amp.netmask = QStringLiteral("255.255.255.0");
        amp.gateway = QStringLiteral("192.168.1.1");
        amp.answer = QStringLiteral("The Power Genius sent its network settings.");
        amp.answerAccepted = true;
        amp.answerCount = 2;
        settings.setPgxl(amp);

        MirrorRecorder recorder("accessorySettings", &settings);
        amp.nickname = QStringLiteral("Contest PGXL");
        amp.answer = QStringLiteral("The Power Genius took the new name.");
        amp.answerCount = 3;
        settings.setPgxl(amp);
        recorder.flush();
        AccessorySettingsModel::Device tuner;
        tuner.nickname = QStringLiteral("Tuner_Genius_XL");
        tuner.networkKnown = true;
        tuner.dhcp = true;
        tuner.address = QStringLiteral("192.168.1.60");
        tuner.netmask = QStringLiteral("255.255.255.0");
        tuner.gateway = QStringLiteral("192.168.1.1");
        tuner.answer = QStringLiteral("The Tuner Genius sent its network settings.");
        tuner.answerAccepted = true;
        tuner.answerCount = 1;
        settings.setTgxl(tuner);
        recorder.flush();
        QString why;
        QVERIFY2(matchesFixture(QStringLiteral("accessorySettings.jsonl"), recorder.text(), &why),
                 qPrintable(why));

        QFile file(fixturePath(QStringLiteral("accessorySettings.jsonl")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QList<SessionMessage> messages = decodeLines(file.readAll());
        QVERIFY(!messages.isEmpty());
        AccessorySettingsModel window;
        for (const SessionMessage& m : messages) {
            for (const MirrorUpdate& u : m.updates) {
                QVERIFY2(window.applyStationValue(u.name, u.value), u.name.constData());
            }
        }
        QCOMPARE(window.pgxl(), amp);
        QCOMPARE(window.tgxl(), tuner);
        QVERIFY(OperatorWording::isPlain(window.pgxlAnswer()));
        // The tuner has no hardware settings on the wire.
        QVERIFY(!window.applyStationValue("tgxlBiasMode", QStringLiteral("ClassA")));
    }

    // R-R3-47 / R-R3-22: the accessoryData fixture is what the Core sends:
    // a Power Genius fault, the interlock policy, the output limit, a tune
    // memory and antenna names at attach; then a Tuner Genius fault, the
    // output going over the limit, and the amp's counters.
    void accessoryDataFixtureIsWhatTheCoreSends()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("PGXL_TxInterlockMode"), QStringLiteral("Block"));
        s.setValue(QStringLiteral("PGXL_TxInterlockGraceMs"), 2000);
        s.setValue(QStringLiteral("PGXL_TxSwrGate"), QStringLiteral("True"));
        s.setValue(QStringLiteral("PGXL_TxSwrGateMax"), QStringLiteral("2.5"));
        s.setValue(QStringLiteral("PGXL_PowerCapEnabled"), QStringLiteral("True"));
        s.setValue(QStringLiteral("PGXL_PowerCapW"), 1500);
        s.setValue(QStringLiteral("TGXL_AutoTuneMemoryRecall"), QStringLiteral("True"));
        s.setValue(QStringLiteral("TGXL_Ant1_Label"), QStringLiteral("80 m dipole"));
        s.setValue(QStringLiteral("RfKit_Ant2_Label"), QStringLiteral("Beam"));

        FaultLog pgxl(QStringLiteral("PGXL_FaultHistory"));
        FaultLog tgxl(QStringLiteral("TGXL_FaultHistory"));
        FaultLog rfkit(QStringLiteral("RfKit_FaultHistory"));
        // The captured fault line's readings (kPgxlFault): SWR 2.85 is
        // above 2.5, so the likely cause is high SWR.
        pgxl.capture(FaultEvent{1790000000000, QStringLiteral("FAULT"), 1820.0f, 2.85f, 78.0f,
                                FaultLog::likelyCauseFor(1820.0f, 2.85f, 78.0f)});
        TuneMemoryStore memory;
        memory.store(TuneMemory{1, Band::Band20m, 120, 45, 200, 1790000000000});
        TxInterlockPolicy policy;
        ConnectionDiagnostics pgxlDiag;
        ConnectionDiagnostics tgxlDiag;
        AccessoryDataModel data;
        StationAccessoryData::Sources sources;
        sources.pgxlFaults = &pgxl;
        sources.tgxlFaults = &tgxl;
        sources.rfkitFaults = &rfkit;
        sources.pgxlDiagnostics = &pgxlDiag;
        sources.tgxlDiagnostics = &tgxlDiag;
        sources.interlock = &policy;
        sources.tuneMemory = &memory;
        // R-R3-49 (parity Task 10): the RF-Kit connection whose counts are
        // published (never dialled here).
        Rf2ksConnection rfkitConnection;
        sources.rfkitConnection = &rfkitConnection;
        StationAccessoryData core(&data, sources);

        MirrorRecorder recorder("accessoryData", &data);
        FaultEvent link{1790000060000, QStringLiteral("link"), 0.0f, 0.0f, 0.0f, QString()};
        link.text = QStringLiteral("The Tuner Genius stopped answering.");
        tgxl.capture(link);
        recorder.flush();
        core.onForwardPower(1600.0);
        recorder.flush();
        ConnectionDiagnostics::Counters counters;
        counters.connectedSinceMs = 1790000000000;
        counters.lastRttMs = 12;
        counters.reconnectCount = 1;
        counters.framesIn = 42;
        counters.framesOut = 40;
        counters.bytesIn = 2048;
        counters.bytesOut = 1024;
        counters.lastFrameMs = 1790000059000;
        pgxlDiag.applyMirroredCounters(counters);
        recorder.flush();
        // R-R3-49 (parity Task 10): one failed poll of an amp not yet
        // answering, and the retry it schedules.
        rfkitConnection.testMarkPollFailure();
        core.publishAll();
        recorder.flush();
        QString why;
        QVERIFY2(matchesFixture(QStringLiteral("accessoryData.jsonl"), recorder.text(), &why),
                 qPrintable(why));

        QFile file(fixturePath(QStringLiteral("accessoryData.jsonl")));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QList<SessionMessage> messages = decodeLines(file.readAll());
        QCOMPARE(messages.size(), 7);
        AccessoryDataModel window;
        for (const SessionMessage& m : messages) {
            for (const MirrorUpdate& u : m.updates) {
                QVERIFY2(window.applyStationValue(u.name, u.value), u.name.constData());
            }
        }
        QCOMPARE(window.faultRevision(), data.faultRevision());
        QCOMPARE(window.interlockMode(), AccessoryDataModel::InterlockMode::Block);
        QCOMPARE(window.interlockGraceMs(), 2000);
        QVERIFY(window.interlockSwrGateEnabled());
        QCOMPARE(window.interlockSwrGateMax(), 2.5);
        QVERIFY(window.powerCapEnabled());
        QCOMPARE(window.powerCapW(), 1500);
        QVERIFY(window.powerCapExceeded());
        QCOMPARE(window.powerCapAlertCount(), 1);
        QCOMPARE(window.powerCapAlertText(),
                 QStringLiteral("Power Genius output 1600 W is above the 1500 W limit."));
        QVERIFY(OperatorWording::isPlain(window.powerCapAlertText()));
        QCOMPARE(window.pgxlReconnectCount(), 1);
        QCOMPARE(window.pgxlBytesIn(), 2048);
        QCOMPARE(window.rfkitPollsFailed(), 1);
        QCOMPARE(window.rfkitReconnectCount(), 1);
        QCOMPARE(window.tgxlAntenna1Label(), QStringLiteral("80 m dipole"));
        QCOMPARE(window.rfkitAntenna2Label(), QStringLiteral("Beam"));
        QVERIFY(window.autoTuneMemoryRecall());
        const QVector<TuneMemory> memories = TuneMemoryStore::fromJson(window.tuneMemory());
        QCOMPARE(memories.size(), 1);
        QCOMPARE(memories.first().band, Band::Band20m);
        QCOMPARE(memories.first().l, 45);
        FaultLog windowPgxl(QStringLiteral("PGXL_FaultHistory_Window"));
        FaultLog windowTgxl(QStringLiteral("TGXL_FaultHistory_Window"));
        windowPgxl.applyMirroredJson(window.pgxlFaults());
        windowTgxl.applyMirroredJson(window.tgxlFaults());
        QCOMPARE(windowPgxl.events().size(), 1);
        QCOMPARE(windowPgxl.events().first().text,
                 QStringLiteral("The Power Genius reported a fault. Likely cause: high SWR."));
        QCOMPARE(windowTgxl.events().first().device, QStringLiteral("tgxl"));
        QCOMPARE(windowTgxl.events().first().whenMs, 1790000060000);
    }

    // R-R3-47: the Core records the Tuner Genius's faults: a live
    // connection dropping, and an attempt ending at an error. An operator's
    // disconnect is not a fault.
    void coreRecordsTunerGeniusFaults()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        RadioModel model;
        model.enableStationAccessoryIdentity();
        model.setReceiveOnlyStationPolicy(true);
        RadioInfo radio;
        radio.macAddress = QStringLiteral("aa:bb:cc:dd:ee:47");
        model.setLastRadioInfoForTest(radio);
        model.setConnectionStateForTest(ConnectionState::Connected);
        model.setPeripheralValue(QStringLiteral("FourO3A_Enabled"), QStringLiteral("True"));
        model.tgxlConnection()->testSetReconnectBackoffUnitMs(60000);
        const auto infoSequence = [](const QSignalSpy& frames) {
            const QRegularExpression rx(QStringLiteral("^C(\\d+)\\|info$"));
            for (const auto& args : frames) {
                const auto match = rx.match(args.first().toString());
                if (match.hasMatch()) { return match.captured(1).toUInt(); }
            }
            return 0u;
        };
        const auto admit = [&](const QString& product, const QString& serial) -> QTcpSocket* {
            QSignalSpy frames(model.tgxlConnection(), &TgxlConnection::testFrameWrittenForTesting);
            QString reason;
            if (!model.configureTgxlForStation(QStringLiteral("127.0.0.1"), server.serverPort(),
                                               &reason)) {
                return nullptr;
            }
            if (!QTest::qWaitFor([&] { return server.hasPendingConnections(); }, 2000)) {
                return nullptr;
            }
            QTcpSocket* peer = server.nextPendingConnection();
            peer->write("V1.2.17\n");
            peer->flush();
            if (!QTest::qWaitFor([&] { return infoSequence(frames) != 0; }, 2000)) {
                return nullptr;
            }
            peer->write(QStringLiteral("R%1|0|info serial=241288-1 version=1.2.17 "
                                       "nickname=Tuner_Genius_XL 3way=1\n")
                            .arg(infoSequence(frames)).toUtf8());
            peer->flush();
            if (!QTest::qWaitFor([&] {
                    return !model.tgxlConnection()->identityInfo().serial.isEmpty(); }, 2000)) {
                return nullptr;
            }
            auto* discovery = model.findChild<LanDiscovery*>();
            if (discovery == nullptr) { return nullptr; }
            discovery->injectDatagramForTesting(
                QStringLiteral("%1 ip=127.0.0.1 v=1.2.17 serial=%2 nickname=Tuner_Genius_XL")
                    .arg(product, serial), server.serverPort());
            return peer;
        };

        // Admitted, then an operator's disconnect: no fault.
        QTcpSocket* peer = admit(QStringLiteral("TunerGeniusXL"), QStringLiteral("241288-1"));
        QVERIFY(peer);
        QTRY_COMPARE(model.tunerModel()->connectionPhase(), TunerModel::ConnectionPhase::Connected);
        QString reason;
        QVERIFY(model.disconnectTgxlForStation(&reason));
        QCoreApplication::processEvents();
        QVERIFY(model.tgxlFaultLog()->events().isEmpty());
        const qint64 revision = model.accessoryDataModel()->faultRevision();

        // Admitted, then the tuner goes away: one fault, in plain words.
        peer = admit(QStringLiteral("TunerGeniusXL"), QStringLiteral("241288-1"));
        QVERIFY(peer);
        QTRY_COMPARE(model.tunerModel()->connectionPhase(), TunerModel::ConnectionPhase::Connected);
        peer->disconnectFromHost();
        QTRY_COMPARE(model.tgxlFaultLog()->events().size(), 1);
        FaultEvent ev = model.tgxlFaultLog()->events().first();
        QCOMPARE(ev.device, QStringLiteral("tgxl"));
        QCOMPARE(ev.state, QStringLiteral("link"));
        QCOMPARE(ev.text, QStringLiteral("The Tuner Genius stopped answering."));
        QVERIFY(ev.whenMs > 0);
        QVERIFY(model.accessoryDataModel()->faultRevision() > revision);
        QVERIFY(model.accessoryDataModel()->tgxlFaults().contains(ev.text));
        QVERIFY(model.disconnectTgxlForStation(&reason));

        // Something else answers at the address: the attempt ends at an
        // error, recorded once with what the Core found.
        peer = admit(QStringLiteral("PowerGeniusXL"), QStringLiteral("241288-1"));
        QVERIFY(peer);
        QTRY_COMPARE(model.tgxlFaultLog()->events().size(), 2);
        ev = model.tgxlFaultLog()->events().first();
        QCOMPARE(ev.state, QStringLiteral("connection"));
        QCOMPARE(ev.text, QStringLiteral("The Core could not connect to the Tuner Genius."));
        QVERIFY(ev.detail.contains(QStringLiteral("PowerGeniusXL")));
        QVERIFY(OperatorWording::isPlain(ev.text));
        QVERIFY(model.disconnectTgxlForStation(&reason));
    }

    // R-R3-47 / R-R3-22: the Core raises the power-cap alert from the amp's
    // own readings, once per time over the limit, as MainWindow did.
    void coreRaisesThePowerCapAlert()
    {
        RadioModel core;
        AccessoryDataModel* data = core.accessoryDataModel();
        QString reason;
        QVERIFY(!core.setPgxlPowerCapForStation(true, 50, &reason));
        QVERIFY(OperatorWording::isPlain(reason));
        QVERIFY(!core.setPgxlPowerCapForStation(true, 2001, &reason));
        // Off: no alert however high.
        core.pgxlConnection()->injectLineForTesting(QString::fromLatin1(kPgxlTransmit));
        QCOMPARE(data->powerCapAlertCount(), 0);

        QVERIFY(core.setPgxlPowerCapForStation(true, 800, &reason));
        QVERIFY(data->powerCapEnabled());
        QCOMPARE(data->powerCapW(), 800);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("PGXL_PowerCapW")).toInt(), 800);
        core.pgxlConnection()->injectLineForTesting(QString::fromLatin1(kPgxlTransmit));   // 1000 W
        QCOMPARE(data->powerCapAlertCount(), 1);
        QVERIFY(data->powerCapExceeded());
        QCOMPARE(data->powerCapAlertText(),
                 QStringLiteral("Power Genius output 1000 W is above the 800 W limit."));
        core.pgxlConnection()->injectLineForTesting(QString::fromLatin1(kPgxlTransmit));
        QCOMPARE(data->powerCapAlertCount(), 1);   // one alert per time over
        core.pgxlConnection()->injectLineForTesting(
            QStringLiteral("S0|status state=TRANSMIT_A peakfwd=50.0 swr=-24.5"));   // 100 W
        QVERIFY(!data->powerCapExceeded());
        core.pgxlConnection()->injectLineForTesting(QString::fromLatin1(kPgxlTransmit));
        QCOMPARE(data->powerCapAlertCount(), 2);

        // A window that writes the limit through the settings reaches it too.
        AppSettings::instance().setValue(QStringLiteral("PGXL_PowerCapEnabled"),
                                         QStringLiteral("False"));
        core.applyRemoteAccessorySetting(QStringLiteral("PGXL_PowerCapEnabled"));
        QVERIFY(!data->powerCapEnabled());
        QVERIFY(!data->powerCapExceeded());
    }
};
QTEST_GUILESS_MAIN(StationAccessoryStateTest)
#include "tst_station_accessory_state.moc"
