// no-port-check: NereusSDR-original receive-layout session regression.
//
// R-R3-34: a Core layout restore after a Remote GUI authenticated must
// re-seed the existing session as one settled snapshot.  The loopback
// transport is intentionally non-TLS; TLS has focused StationServer tests.

#include <QtTest/QtTest>

#include <QFile>
#include <QPointer>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/ReceiveLayoutStore.h"
#include "core/session/MirrorSchema.h"
#include "core/session/SessionMessages.h"
#define private public
#include "core/session/StationClient.h"
#undef private
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:44");

QList<ReceiveSliceState> restoredLayout()
{
    // The saved order is deliberately B then A; stable identity, not list
    // position, keeps the GUI's existing A object attached to receiver 0.
    return {
        {2, QStringLiteral("pan-1"), 7'200'000.0, DSPMode::LSB},
        {0, QStringLiteral("pan-0"), 14'293'200.0, DSPMode::USB},
    };
}

bool hasOnlyFrequencyWrite(const QList<QByteArray>& wires)
{
    int writes = 0;
    for (const QByteArray& wire : wires) {
        SessionMessage message;
        if (!SessionMessages::decode(wire, &message)
            || message.kind != SessionMessageKind::PropertyWrite) {
            continue;
        }
        ++writes;
        if (message.objectKey != QByteArrayLiteral("slice:0")
            || message.updates.size() != 1
            || message.updates.first().name != QByteArrayLiteral("frequency")) {
            return false;
        }
    }
    return writes == 1;
}

bool hasOnlyPropertyWrite(const QList<QByteArray>& wires, const QByteArray& objectKey,
                          const QByteArray& property)
{
    int writes = 0;
    for (const QByteArray& wire : wires) {
        SessionMessage message;
        if (!SessionMessages::decode(wire, &message)
            || message.kind != SessionMessageKind::PropertyWrite) {
            continue;
        }
        ++writes;
        if (message.objectKey != objectKey || message.updates.size() != 1
            || message.updates.first().name != property) {
            return false;
        }
    }
    return writes == 1;
}

bool containsRejectedResult(const QList<QByteArray>& wires, quint32 writeId,
                            const QByteArray& property)
{
    for (const QByteArray& wire : wires) {
        SessionMessage message;
        if (!SessionMessages::decode(wire, &message)
            || message.kind != SessionMessageKind::PropertyResult
            || message.writeId != writeId) {
            continue;
        }
        for (const SessionPropertyResult& result : message.propertyResults) {
            if (result.property == property) {
                return !result.accepted && !result.reason.isEmpty();
            }
        }
    }
    return false;
}

class SnapshotHoldingTransport final : public LoopbackTransport {
public:
    explicit SnapshotHoldingTransport(QObject* parent)
        : LoopbackTransport(QStringLiteral("layout-station"), parent)
    {
    }

    void holdNextSnapshotComplete() { m_holdSnapshotComplete = true; }
    bool hasHeldSnapshotComplete() const { return !m_heldSnapshotComplete.isEmpty(); }

    void releaseSnapshotComplete()
    {
        const QByteArray held = m_heldSnapshotComplete;
        m_heldSnapshotComplete.clear();
        m_holdSnapshotComplete = false;
        LoopbackTransport::sendText(held);
    }

    void sendText(const QByteArray& wire) override
    {
        SessionMessage message;
        if (m_holdSnapshotComplete && SessionMessages::decode(wire, &message)
            && message.kind == SessionMessageKind::SnapshotComplete) {
            m_heldSnapshotComplete = wire;
            return;
        }
        LoopbackTransport::sendText(wire);
    }

private:
    bool m_holdSnapshotComplete = false;
    QByteArray m_heldSnapshotComplete;
};

QList<SessionSchemaField> oldRadioSchema()
{
    RadioModel shape(RadioModel::Role::Remote);
    const MirrorSchema& schema = MirrorSchema::forObject(&shape);
    QList<SessionSchemaField> fields;
    quint16 ordinal = 0;
    for (const MirrorProperty& property : schema.properties()) {
        if (property.name == "receiveLayoutRestoreState"
            || property.name == "receiveLayoutRestoreMessage") {
            continue;
        }
        fields.append({ordinal++, property.name, property.kind});
    }
    return fields;
}

} // namespace

class TstReceiveLayoutSession : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // prepareReceiveLayout() intentionally uses the process settings
        // singleton.  Scope it before first use, while certificates remain
        // in each test's own QTemporaryDir below.
        AppSettings::setProfileOverride(QStringLiteral("receive-layout-session-%1")
                                        .arg(QCoreApplication::applicationPid()));
    }

    void init()
    {
        AppSettings::instance().clear();
        QVERIFY(AppSettings::instance().save());
    }

    void cleanupTestCase()
    {
        QFile::remove(AppSettings::instance().filePath());
    }

    void authenticatedClientReceivesSettledReseedWithoutSessionReplacement()
    {
        QTemporaryDir securityDirectory;
        QVERIFY(securityDirectory.isValid());

        // Offline Core bootstrap: A and an eventual extra receiver already
        // exist when the Remote GUI first authenticates.
        RadioModel core;
        QCOMPARE(core.addSlice(QStringLiteral("pan-0")), 0);
        QCOMPARE(core.addSlice(QStringLiteral("pan-0")), 1);
        SliceModel* const coreA = core.sliceById(0);
        QVERIFY(coreA != nullptr);
        coreA->setLocked(true);

        // Legacy state belongs to the shared A object and is restored as part
        // of the manifest hydration, without individual change notifies.
        auto& settings = AppSettings::instance();
        settings.setValue(QStringLiteral("Slice0/AfGain"), 37);
        settings.setValue(QStringLiteral("Slice0/Locked"), QStringLiteral("True"));
        settings.setValue(QStringLiteral("Slice0/Band20m/DspMode"),
                          static_cast<int>(DSPMode::USB));
        settings.setValue(QStringLiteral("Slice0/Band20m/ModeUSB/FilterLow"), 250);
        settings.setValue(QStringLiteral("Slice0/Band20m/ModeUSB/FilterHigh"), 2750);
        QString error;
        QVERIFY2(ReceiveLayoutStore::stage(settings, kMac, restoredLayout(), &error),
                 qPrintable(error));
        QVERIFY2(settings.save(&error), qPrintable(error));

        StationServer server(&core, settings, NereusSDR::Test::seedUpgradedCoreToken(securityDirectory.path()));
        server.setMediaEnabled(true);

        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        auto* stationEnd = new LoopbackTransport(QStringLiteral("layout-station"), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("layout-client"), this);
        stationEnd->linkTo(clientEnd);

        QSignalSpy handshakes(&client, &StationClient::handshakeComplete);
        QSignalSpy snapshots(&client, &StationClient::stateSnapshotApplied);
        QSignalSpy mediaReady(&server, &StationServer::mediaSessionStarted);
        client.startSession(clientEnd, server.token());
        server.acceptTransport(stationEnd);
        QTRY_COMPARE(handshakes.count(), 1);
        QTRY_COMPARE(snapshots.count(), 1);
        QTRY_COMPARE(mediaReady.count(), 1);

        SliceModel* const remoteA = remote.sliceById(0);
        QPointer<SliceModel> remoteExtra = remote.sliceById(1);
        QVERIFY(remoteA != nullptr);
        QVERIFY(remoteA->locked());
        QVERIFY(remoteExtra != nullptr);
        const quint32 clientEpoch = client.sessionEpoch();
        const quint64 serverEpoch = server.sessionEpoch();
        QVERIFY(clientEpoch > 0);
        QVERIFY(serverEpoch > 0);
        stationEnd->clearReceived();

        // This emits receiveLayoutHydrated after reconciling A/B/extra.
        // StationServer must attachSession() again, not create a new session
        // or rely on the suppressed per-slice signals to reach the GUI.
        core.prepareReceiveLayout(kMac);
        QTRY_COMPARE(snapshots.count(), 2);
        QCOMPARE(handshakes.count(), 1);
        QCOMPARE(mediaReady.count(), 1);
        QCOMPARE(client.sessionEpoch(), clientEpoch);
        QCOMPARE(server.sessionEpoch(), serverEpoch);

        // The A QObject is retained on both sides; extra 1 is gone and B is
        // added exactly once.  The complete snapshot carries every settled
        // manifest field plus A's silent legacy AF/filter state.
        QCOMPARE(core.sliceById(0), coreA);
        QCOMPARE(core.slices().size(), 2);
        QCOMPARE(core.slices().at(0)->sliceIndex(), 2);
        QCOMPARE(core.slices().at(1)->sliceIndex(), 0);
        QCOMPARE(remote.slices().size(), 2);
        QCOMPARE(remote.sliceById(0), remoteA);
        QVERIFY(remote.sliceById(1) == nullptr);
        QTRY_VERIFY(remoteExtra.isNull());
        SliceModel* const remoteB = remote.sliceById(2);
        QVERIFY(remoteB != nullptr);
        QCOMPARE(remoteA->frequency(), 14'293'200.0);
        QCOMPARE(remoteA->dspMode(), DSPMode::USB);
        QCOMPARE(remoteA->panKey(), QStringLiteral("pan-0"));
        QCOMPARE(remoteA->afGain(), 37);
        QCOMPARE(remoteA->filterLow(), 250);
        QCOMPARE(remoteA->filterHigh(), 2750);
        QCOMPARE(remoteB->frequency(), 7'200'000.0);
        QCOMPARE(remoteB->dspMode(), DSPMode::LSB);
        QCOMPARE(remoteB->panKey(), QStringLiteral("pan-1"));

        // The status is Core-owned outbound telemetry.  It reaches the GUI
        // through the StationClient adapter and cannot be changed by it.
        QTRY_COMPARE(remote.receiveLayoutRestoreState(), QStringLiteral("pending"));
        QVERIFY(!remote.receiveLayoutRestoreMessage().isEmpty());
        constexpr quint32 kReadOnlyWriteId = 41;
        const QList<MirrorUpdate> readOnlyAttempt{
            {0, QByteArrayLiteral("receiveLayoutRestoreState"), MirrorWireKind::Utf8,
             QStringLiteral("accepted")},
        };
        clientEnd->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            QByteArrayLiteral("radio"), readOnlyAttempt, kReadOnlyWriteId)));
        QTRY_VERIFY(containsRejectedResult(clientEnd->received(), kReadOnlyWriteId,
                                            QByteArrayLiteral("receiveLayoutRestoreState")));
        QCOMPARE(core.receiveLayoutRestoreState(), QStringLiteral("pending"));
        QCOMPARE(remote.receiveLayoutRestoreState(), QStringLiteral("pending"));

        // A real client edit is the flush barrier for all inbound snapshot
        // work.  Exactly one frequency write proves forwarding resumed after
        // SnapshotComplete and that no reseed field echoed back as a write.
        QVERIFY(remoteA->locked());
        remoteA->setFrequency(14'074'000.0);
        QCOMPARE(remoteA->frequency(), 14'293'200.0); // operator tuning still respects lock
        remoteA->setLocked(false);
        QTRY_VERIFY(!coreA->locked());
        stationEnd->clearReceived();
        remoteA->setFrequency(14'074'000.0);
        QTRY_COMPARE(coreA->frequency(), 14'074'000.0);
        QVERIFY(hasOnlyFrequencyWrite(stationEnd->received()));
    }

    void sameSessionReseedQueuesOperatorEditUntilSnapshotComplete()
    {
        QTemporaryDir securityDirectory;
        QVERIFY(securityDirectory.isValid());

        RadioModel core;
        QCOMPARE(core.addSlice(QStringLiteral("pan-0")), 0);
        SliceModel* const coreA = core.sliceById(0);
        QVERIFY(coreA != nullptr);

        auto& settings = AppSettings::instance();
        QString error;
        QVERIFY2(ReceiveLayoutStore::stage(settings, kMac, restoredLayout(), &error),
                 qPrintable(error));
        QVERIFY2(settings.save(&error), qPrintable(error));

        StationServer server(&core, settings, NereusSDR::Test::seedUpgradedCoreToken(securityDirectory.path()));
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        auto* stationEnd = new SnapshotHoldingTransport(this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("layout-client"), this);
        stationEnd->linkTo(clientEnd);

        QSignalSpy snapshots(&client, &StationClient::stateSnapshotApplied);
        client.startSession(clientEnd, server.token());
        server.acceptTransport(stationEnd);
        QTRY_COMPARE(snapshots.count(), 1);
        SliceModel* const remoteA = remote.sliceById(0);
        QVERIFY(remoteA != nullptr);

        stationEnd->clearReceived();
        stationEnd->holdNextSnapshotComplete();
        core.prepareReceiveLayout(kMac);
        QTRY_VERIFY(stationEnd->hasHeldSnapshotComplete());
        QTRY_COMPARE(remoteA->frequency(), 14'293'200.0);

        // The schema burst is in flight, so the GUI edit must be retained
        // locally but cannot reach Core before the final marker.
        const int coreGainBeforeEdit = coreA->afGain();
        remoteA->setAfGain(58);
        QCOMPARE(remoteA->afGain(), 58);
        client.onWriteFlushTick();
        QCoreApplication::processEvents();
        QVERIFY(!stationEnd->receivedKinds().contains(QByteArrayLiteral("property.write")));
        QCOMPARE(coreA->afGain(), coreGainBeforeEdit);

        stationEnd->releaseSnapshotComplete();
        QTRY_COMPARE(snapshots.count(), 2);
        client.onWriteFlushTick();
        QTRY_COMPARE(coreA->afGain(), 58);
        QVERIFY(hasOnlyPropertyWrite(stationEnd->received(), QByteArrayLiteral("slice:0"),
                                     QByteArrayLiteral("afGain")));
    }

    // Slice control fix wave, round 2: a lone device restores an older
    // manifest (no owners in it). The restore releases its slice to nobody
    // with the device still listening; it adopts the slice back and can
    // change it (unlock, AF gain), not "Nobody controls slice A".
    void aLoneDeviceChangesItsOwnSliceAfterRestoringAnOlderManifest()
    {
        QTemporaryDir securityDirectory;
        QVERIFY(securityDirectory.isValid());

        RadioModel core;
        QCOMPARE(core.addSlice(QStringLiteral("pan-0")), 0);
        SliceModel* const coreA = core.sliceById(0);
        QVERIFY(coreA != nullptr);
        coreA->setLocked(true);

        auto& settings = AppSettings::instance();
        QString error;
        QVERIFY2(ReceiveLayoutStore::stage(settings, kMac, restoredLayout(), &error),
                 qPrintable(error));
        QVERIFY2(settings.save(&error), qPrintable(error));

        StationServer server(&core, settings,
                             NereusSDR::Test::seedUpgradedCoreToken(securityDirectory.path()));
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        auto* stationEnd = new LoopbackTransport(QStringLiteral("layout-station"), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("layout-client"), this);
        stationEnd->linkTo(clientEnd);

        QSignalSpy snapshots(&client, &StationClient::stateSnapshotApplied);
        client.startSession(clientEnd, server.token());
        server.acceptTransport(stationEnd);
        QTRY_COMPARE(snapshots.count(), 1);
        SliceOwnership* const ownership = core.sliceOwnership();
        const QByteArray device = ownership->mark(0).owner;
        QVERIFY(!device.isEmpty());
        QVERIFY(device != SliceOwnership::stationDevice());

        core.prepareReceiveLayout(kMac);
        QTRY_COMPARE(snapshots.count(), 2);
        QTRY_COMPARE(ownership->mark(0).owner, device);
        QCOMPARE(ownership->listenersOf(0), QList<QByteArray>{device});

        SliceModel* const remoteA = remote.sliceById(0);
        QVERIFY(remoteA != nullptr);
        QTRY_VERIFY(remoteA->locked());
        remoteA->setLocked(false);
        client.onWriteFlushTick();
        QTRY_VERIFY(!coreA->locked());
        remoteA->setAfGain(61);
        client.onWriteFlushTick();
        QTRY_COMPARE(coreA->afGain(), 61);
    }

    void reconnectToOlderPeerClearsOptionalRestoreStatus()
    {
        QTemporaryDir securityDirectory;
        QVERIFY(securityDirectory.isValid());

        RadioModel core;
        core.prepareReceiveLayout(kMac);
        StationServer server(&core, AppSettings::instance(), NereusSDR::Test::seedUpgradedCoreToken(securityDirectory.path()));
        RadioModel remote(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&remote, &proxy);
        QSignalSpy handshakes(&client, &StationClient::handshakeComplete);

        auto* newStation = new LoopbackTransport(QStringLiteral("new-layout-station"), this);
        auto* firstClient = new LoopbackTransport(QStringLiteral("first-layout-client"), this);
        newStation->linkTo(firstClient);
        client.startSession(firstClient, server.token());
        server.acceptTransport(newStation);
        QTRY_COMPARE(handshakes.count(), 1);
        QTRY_COMPARE(remote.receiveLayoutRestoreState(), QStringLiteral("pending"));
        QVERIFY(!remote.receiveLayoutRestoreMessage().isEmpty());

        auto* oldStation = new LoopbackTransport(QStringLiteral("old-layout-station"), this);
        auto* reconnectClient = new LoopbackTransport(QStringLiteral("reconnect-layout-client"), this);
        oldStation->linkTo(reconnectClient);
        client.startSession(reconnectClient, QStringLiteral("old-peer-token"));
        QCOMPARE(remote.receiveLayoutRestoreState(), QString());
        QCOMPARE(remote.receiveLayoutRestoreMessage(), QString());

        StationCapabilities capabilities;
        capabilities.stationName = QStringLiteral("older Core");
        capabilities.radioConnected = true;
        capabilities.effectiveMaxSlices = 2;
        capabilities.boardMaxSlices = 2;
        oldStation->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 6, QStringLiteral("older Core"))));
        oldStation->sendText(SessionMessages::encode(SessionMessages::authResult(true, {}, false)));
        oldStation->sendText(
            SessionMessages::encode(SessionMessages::capabilities(capabilities.toUpdates())));
        // This authenticates and completes a schema-compatible snapshot
        // whose RadioModel explicitly predates the two optional fields.
        oldStation->sendText(SessionMessages::encode(
            SessionMessages::schema(QByteArrayLiteral("RadioModel"), oldRadioSchema())));
        oldStation->sendText(SessionMessages::encode(SessionMessages::snapshotComplete()));
        QTRY_COMPARE(handshakes.count(), 2);
        QVERIFY(client.isHandshakeComplete());
        QCOMPARE(remote.receiveLayoutRestoreState(), QString());
        QCOMPARE(remote.receiveLayoutRestoreMessage(), QString());
    }
};

QTEST_MAIN(TstReceiveLayoutSession)
#include "tst_receive_layout_session.moc"
