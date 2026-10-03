// no-port-check: NereusSDR-original. iPhone app plan Task 25 (R-IOS-18):
// the station computer's VAX channels as the mirrored `vax` object
// (StationVax, vaxVersion 1) and the `vaxLevels` record stream. A device
// that declared vax 1 sees the channels as the station computer's VAX
// applet shows them, changes a level or a mute as the applet does (saved
// under the applet's own keys), changes the transmit level only while it
// may transmit, and gets the meters 5 times a second only while it
// subscribes. Loopback link, no radio, no audio device opened.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-28  J.J. Boyd / KG4VCF  Created (iPhone app plan Task 25,
//                                    R-IOS-18). AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QtTest>

#include <QCoreApplication>
#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cmath>
#include <memory>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/security/DeviceAuthenticator.h"
#include "core/security/DeviceStore.h"
#include "core/security/StationIdentity.h"
#include "core/session/MirrorPolicy.h"
#include "core/session/MirrorSchema.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationServer.h"
#include "core/session/StationVaxFacade.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/applets/VaxApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "OperatorWording.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kMac = QStringLiteral("AA:BB:CC:DD:EE:5A");

std::unique_ptr<RadioModel> makeStationRadioModel()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::Saturn);
    model->setHpsdrModelForTest(HPSDRModel::ANAN_G2);
    RadioInfo info;
    info.macAddress = kMac;
    info.name = QStringLiteral("Bench G2");
    info.boardType = HPSDRHW::Saturn;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    model->addSlice(QStringLiteral("pan-0"));
    return model;
}

// One device on a Core, speaking the link directly as the phone does.
struct Device {
    // `key` null: signs in with the Core's token. Otherwise pairs that key
    // with the Core first and signs in with it, declaring deviceAuth, as
    // the phone does (only a paired device may transmit).
    Device(StationServer* server, QObject* parent, QHash<QByteArray, int> features,
           const StationIdentity* key = nullptr)
    {
        app = new LoopbackTransport(QStringLiteral("app"), parent);
        station = new LoopbackTransport(QStringLiteral("station"), server);
        station->linkTo(app);
        if (key != nullptr) {
            PairedDevice record;
            record.id = key->fingerprint();
            record.publicKeySpki = key->publicKeySpki();
            record.name = QStringLiteral("VAX phone");
            record.kind = QStringLiteral("phone");
            server->deviceStore()->add(record);
            features.insert("deviceAuth", 1);
        }
        server->acceptTransport(station);
        QVERIFY2(QTest::qWaitFor([this]() { return !app->received().isEmpty(); }, 5000),
                 "the Core never greeted the device");
        app->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("VAX phone"),
            {kSessionProtocolMajor}, features)));
        if (key == nullptr) {
            app->sendText(SessionMessages::encode(SessionMessages::authRequest(server->token())));
            return;
        }
        QByteArray challenge;
        for (const SessionMessage& message : messages()) {
            if (message.kind == SessionMessageKind::Hello) {
                challenge = StationIdentity::fromBase64Url(message.challenge);
            }
        }
        QString pin = server->certificateFingerprint();
        pin.remove(QLatin1Char(':'));
        const QByteArray certHash = QByteArray::fromHex(pin.toLatin1());
        const SessionDeviceBlock block{
            StationIdentity::toBase64Url(key->fingerprint()),
            StationIdentity::toBase64Url(key->publicKeySpki()),
            QStringLiteral("VAX phone"), QStringLiteral("phone"),
            StationIdentity::toBase64Url(key->sign(DeviceAuthenticator::transcript(
                challenge, certHash, server->stationIdentity().publicKeySpki(),
                key->publicKeySpki())))};
        app->sendText(SessionMessages::encode(SessionMessages::authRequest(QString(), block)));
    }
    bool ready() const
    {
        return QTest::qWaitFor([this]() {
            return app->receivedKinds().contains(QByteArrayLiteral("snapshot.complete"));
        }, 5000);
    }
    QList<SessionMessage> messages() const
    {
        QList<SessionMessage> out;
        for (const QByteArray& wire : app->received()) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)) {
                out.append(message);
            }
        }
        return out;
    }
    // The last value this device heard for `name` on the `vax` object.
    QVariant vaxValue(const QByteArray& name) const
    {
        QVariant value;
        for (const SessionMessage& message : messages()) {
            if ((message.kind != SessionMessageKind::ObjectCreate
                 && message.kind != SessionMessageKind::Delta)
                || message.objectKey != QByteArrayLiteral("vax")) {
                continue;
            }
            for (const MirrorUpdate& update : message.updates) {
                if (update.name == name) {
                    value = update.value;
                }
            }
        }
        return value;
    }
    bool hasVaxObject() const
    {
        for (const SessionMessage& message : messages()) {
            if (message.objectKey == QByteArrayLiteral("vax")
                || message.className == QByteArrayLiteral("StationVax")) {
                return true;
            }
        }
        return false;
    }
    // The capability's value, or -1 when the entry is absent.
    int capability(const QByteArray& name) const
    {
        int value = -1;
        for (const SessionMessage& message : messages()) {
            if (message.kind != SessionMessageKind::Capabilities) {
                continue;
            }
            value = -1;
            for (const MirrorUpdate& update : message.updates) {
                if (update.name == name) {
                    value = update.value.toInt();
                }
            }
        }
        return value;
    }
    SessionPropertyResult write(const QByteArray& name, MirrorWireKind kind,
                                const QVariant& value)
    {
        const quint32 writeId = ++nextId;
        app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            QByteArrayLiteral("vax"), {MirrorUpdate{1, name, kind, value}}, writeId)));
        SessionPropertyResult found;
        const bool arrived = QTest::qWaitFor([&]() {
            for (const SessionMessage& message : messages()) {
                if (message.kind == SessionMessageKind::PropertyResult
                    && message.writeId == writeId && !message.propertyResults.isEmpty()) {
                    found = message.propertyResults.first();
                    return true;
                }
            }
            return false;
        }, 3000);
        if (!arrived) {
            found.reason = QStringLiteral("no property.result arrived");
        }
        // QVERIFY's check without its bare return (this returns a value);
        // the timeout also stays in found.reason for the caller.
        QTest::qVerify(arrived, "arrived", "", __FILE__, __LINE__);
        return found;
    }
    SessionMessage invoke(const QByteArray& verb, const QList<MirrorUpdate>& arguments)
    {
        const quint32 id = ++nextId;
        app->sendText(SessionMessages::encode(SessionMessages::commandInvoke(verb, id, arguments)));
        SessionMessage found;
        const bool arrived = QTest::qWaitFor([&]() {
            for (const SessionMessage& message : messages()) {
                if (message.kind == SessionMessageKind::CommandResult
                    && message.commandId == id) {
                    found = message;
                    return true;
                }
            }
            return false;
        }, 3000);
        if (!arrived) {
            found.reason = QStringLiteral("no command.result arrived");
        }
        // As write() does.
        QTest::qVerify(arrived, "arrived", "", __FILE__, __LINE__);
        return found;
    }
    // Every vaxLevels record this device received, oldest first.
    QList<QJsonObject> levelRecords() const
    {
        QList<QJsonObject> out;
        for (const SessionMessage& message : messages()) {
            if (message.kind != SessionMessageKind::RecordBatch
                || message.recordBatch.stream != QLatin1String(StationVax::kLevelsStream)) {
                continue;
            }
            for (const RecordUpsert& upsert : message.recordBatch.upserts) {
                out.append(upsert.fields);
            }
        }
        return out;
    }

    LoopbackTransport* app = nullptr;
    LoopbackTransport* station = nullptr;
    quint32 nextId = 7700;
};

const QHash<QByteArray, int> kVaxFeatures{{"vax", 1}, {"remoteTx", 1}};

MirrorUpdate textArg(const QByteArray& name, const QString& value)
{
    return MirrorUpdate{0, name, MirrorWireKind::Utf8, QVariant(value)};
}

MirrorUpdate intArg(const QByteArray& name, qlonglong value)
{
    return MirrorUpdate{0, name, MirrorWireKind::Int64, QVariant(value)};
}

} // namespace

class TstStationVax : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_securityDir.isValid());
        AppSettings::setProfileOverride(
            QStringLiteral("station-vax-%1").arg(QCoreApplication::applicationPid()));
        AppSettings::instance().clear();
    }
    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }
    void init()
    {
        AppSettings::instance().clear();
        m_core = makeStationRadioModel();
        m_server = std::make_unique<StationServer>(
            m_core.get(), AppSettings::instance(),
            NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    }
    void cleanup()
    {
        m_server.reset();
        m_core.reset();
    }

    // The object's properties, in wire order, each with its direction.
    void theObjectsPropertiesAreAppendedInOrder()
    {
        StationVax vax;
        const MirrorSchema& schema = MirrorSchema::forObject(&vax);
        const struct {
            const char* name;
            MirrorWireKind kind;
            bool twoWay;
        } expected[] = {
            {"ch1Slices", MirrorWireKind::Utf8, false},
            {"ch2Slices", MirrorWireKind::Utf8, false},
            {"ch3Slices", MirrorWireKind::Utf8, false},
            {"ch4Slices", MirrorWireKind::Utf8, false},
            {"ch1RxGain", MirrorWireKind::Float64, true},
            {"ch2RxGain", MirrorWireKind::Float64, true},
            {"ch3RxGain", MirrorWireKind::Float64, true},
            {"ch4RxGain", MirrorWireKind::Float64, true},
            {"ch1Muted", MirrorWireKind::Bool, true},
            {"ch2Muted", MirrorWireKind::Bool, true},
            {"ch3Muted", MirrorWireKind::Bool, true},
            {"ch4Muted", MirrorWireKind::Bool, true},
            {"ch1Device", MirrorWireKind::Utf8, false},
            {"ch2Device", MirrorWireKind::Utf8, false},
            {"ch3Device", MirrorWireKind::Utf8, false},
            {"ch4Device", MirrorWireKind::Utf8, false},
            {"txSlice", MirrorWireKind::Utf8, false},
            {"txGain", MirrorWireKind::Float64, true},
        };
        quint16 ordinal = 0;
        for (const auto& e : expected) {
            const MirrorProperty* prop = schema.byName(e.name);
            QVERIFY2(prop, e.name);
            QCOMPARE(prop->kind, e.kind);
            QCOMPARE(prop->ordinal, ordinal++);
            QCOMPARE(MirrorPolicy::inboundAllowed("StationVax", e.name), e.twoWay);
        }
    }

    // Only a device that declared vax 1 is told and sent the object.
    void onlyADeviceThatAsksGetsTheObject()
    {
        Device older(m_server.get(), this, {{"remoteTx", 1}});
        QVERIFY(older.ready());
        QCOMPARE(older.capability("vaxVersion"), -1);
        QVERIFY(!older.hasVaxObject());

        Device phone(m_server.get(), this, kVaxFeatures);
        QVERIFY(phone.ready());
        QCOMPARE(m_server->vaxVersion(), 1);
        QCOMPARE(phone.capability("vaxVersion"), 1);
        QVERIFY(phone.hasVaxObject());
        QCOMPARE(phone.vaxValue("ch1Device").toString(), StationVax::deviceName(1));
        QCOMPARE(phone.vaxValue("ch1RxGain").toDouble(), 1.0);
        QCOMPARE(phone.vaxValue("ch1Muted").toBool(), false);
    }

    // A headless Core publishes no VAX devices, so it offers no object.
    void aHeadlessCoreOffersNone()
    {
        m_core->localAudioDevices()->setVaxOutputsAllowed(false);
        Device phone(m_server.get(), this, kVaxFeatures);
        QVERIFY(phone.ready());
        QCOMPARE(m_server->vaxVersion(), 0);
        QCOMPARE(phone.capability("vaxVersion"), 0);
        QVERIFY(!phone.hasVaxObject());
        const SessionPropertyResult refused =
            phone.write("ch1Muted", MirrorWireKind::Bool, QVariant(true));
        QVERIFY(!refused.accepted);
        QVERIFY(!m_core->localAudioDevices()->vaxMuted(1));
    }

    // A level or a mute from a device applies to the Core's engine at once
    // and is saved under the applet's own keys, so the station computer's
    // applet shows it, now and after a restart.
    void aDeviceChangesALevelAndAMuteAsTheAppletDoes()
    {
        AudioEngine* audio = m_core->localAudioDevices();
        Device phone(m_server.get(), this, kVaxFeatures);
        QVERIFY(phone.ready());

        const SessionPropertyResult gain =
            phone.write("ch2RxGain", MirrorWireKind::Float64, QVariant(0.25));
        QVERIFY2(gain.accepted, qPrintable(gain.reason));
        QCOMPARE(audio->vaxRxGain(2), 0.25f);
        QCOMPARE(AppSettings::instance().value(StationVax::rxGainKey(2)).toString(),
                 QStringLiteral("0.250"));
        const SessionPropertyResult mute =
            phone.write("ch3Muted", MirrorWireKind::Bool, QVariant(true));
        QVERIFY2(mute.accepted, qPrintable(mute.reason));
        QVERIFY(audio->vaxMuted(3));
        QCOMPARE(AppSettings::instance().value(StationVax::mutedKey(3)).toString(),
                 QStringLiteral("True"));

        // The applet on the station computer, built from its saved keys as
        // at the next start, reads the device's change.
        AudioEngine fresh;
        VaxApplet applet(m_core.get(), &fresh);
        QCOMPARE(fresh.vaxRxGain(2), 0.25f);
        QVERIFY(fresh.vaxMuted(3));

        // Out of range: refused with the range, nothing changed.
        for (const double wrong : {1.5, -0.1, std::nan("")}) {
            const SessionPropertyResult refused =
                phone.write("ch2RxGain", MirrorWireKind::Float64, QVariant(wrong));
            QVERIFY(!refused.accepted);
            QCOMPARE(refused.reason, QStringLiteral("A VAX level goes from 0 to 1."));
            QCOMPARE(audio->vaxRxGain(2), 0.25f);
        }
        // Outbound properties are the Core's.
        const SessionPropertyResult device =
            phone.write("ch1Device", MirrorWireKind::Utf8, QVariant(QStringLiteral("x")));
        QVERIFY(!device.accepted);
    }

    // A change on the station computer (its applet) reaches the device.
    void aStationChangeReachesTheDevice()
    {
        AudioEngine* audio = m_core->localAudioDevices();
        Device phone(m_server.get(), this, kVaxFeatures);
        QVERIFY(phone.ready());
        audio->setVaxRxGain(4, 0.5f);
        audio->setVaxMuted(1, true);
        audio->setVaxTxGain(0.75f);
        QTRY_COMPARE(phone.vaxValue("ch4RxGain").toDouble(), 0.5);
        QTRY_COMPARE(phone.vaxValue("ch1Muted").toBool(), true);
        QTRY_COMPARE(phone.vaxValue("txGain").toDouble(), 0.75);

        // The slices feeding each channel and the transmit slice, by letter,
        // as the applet's tags show them.
        SliceModel* a = m_core->sliceById(0);
        SliceModel* b = m_core->sliceById(1);
        QVERIFY(a && b);
        a->setVaxChannel(2);
        b->setVaxChannel(2);
        QTRY_COMPARE(phone.vaxValue("ch2Slices").toString(), QStringLiteral("AB"));
        b->setVaxChannel(3);
        QTRY_COMPARE(phone.vaxValue("ch2Slices").toString(), QStringLiteral("A"));
        QTRY_COMPARE(phone.vaxValue("ch3Slices").toString(), QStringLiteral("B"));
        QCOMPARE(m_server->stationVax()->txSlice(),
                 a->isTxSlice() ? QStringLiteral("A") : b->isTxSlice() ? QStringLiteral("B")
                                                                      : QString());
    }

    // The transmit level only from a device that may transmit.
    void theTransmitLevelNeedsTransmit()
    {
        AudioEngine* audio = m_core->localAudioDevices();
        audio->setVaxTxGain(1.0f);
        m_server->setRemoteTransmitAllowed(false);
        QTemporaryDir keyDir;
        QVERIFY(keyDir.isValid());
        const StationIdentity key = StationIdentity::loadOrCreate(keyDir.path());
        Device phone(m_server.get(), this, kVaxFeatures, &key);
        QVERIFY(phone.ready());
        const SessionPropertyResult refused =
            phone.write("txGain", MirrorWireKind::Float64, QVariant(0.5));
        QVERIFY(!refused.accepted);
        QVERIFY(!refused.reason.isEmpty());
        QCOMPARE(m_server->txDecisionFor(phone.station).refusal.text, refused.reason);
        QCOMPARE(audio->vaxTxGain(), 1.0f);
        // A receive level still applies: it keys nothing.
        QVERIFY(phone.write("ch1RxGain", MirrorWireKind::Float64, QVariant(0.5)).accepted);

        m_server->setRemoteTransmitAllowed(true);
        QTRY_VERIFY(m_server->txDecisionFor(phone.station).permitted);
        const SessionPropertyResult taken =
            phone.write("txGain", MirrorWireKind::Float64, QVariant(0.5));
        QVERIFY2(taken.accepted, qPrintable(taken.reason));
        QCOMPARE(audio->vaxTxGain(), 0.5f);
        QCOMPARE(AppSettings::instance().value(StationVax::txGainKey()).toString(),
                 QStringLiteral("0.500"));
    }

    // The meters: read 5 times a second only while a device subscribes,
    // and sent only when one moved.
    void theMetersGoOnlyToADeviceWithTheToolOpen()
    {
        double rx[4] = {0.1, 0.2, 0.3, 0.4};
        double tx = 0.05;
        m_server->setVaxLevelReaderForTest([&](double* r, double* t) {
            for (int i = 0; i < 4; ++i) {
                r[i] = rx[i];
            }
            *t = tx;
        });
        Device phone(m_server.get(), this, kVaxFeatures);
        QVERIFY(phone.ready());
        QVERIFY(!m_server->vaxLevelsPollingForTest());
        QCOMPARE(StationVax::kLevelsIntervalMs, 200);

        const SessionMessage subscribed = phone.invoke(
            "records.subscribe",
            {textArg("stream", QString::fromLatin1(StationVax::kLevelsStream)),
             intArg("backlog", 1)});
        QVERIFY2(subscribed.accepted, qPrintable(subscribed.reason));
        QVERIFY(m_server->vaxLevelsPollingForTest());
        m_server->pollVaxLevelsForTest();
        QTRY_COMPARE(phone.levelRecords().size(), 1);
        QJsonObject first = phone.levelRecords().first();
        QCOMPARE(first.value(QStringLiteral("ch1Level")).toDouble(), 0.1);
        QCOMPARE(first.value(QStringLiteral("ch4Level")).toDouble(), 0.4);
        QCOMPARE(first.value(QStringLiteral("txLevel")).toDouble(), 0.05);
        QVERIFY(first.contains(QStringLiteral("atMs")));

        // Nothing moved: nothing sent.
        m_server->pollVaxLevelsForTest();
        QTest::qWait(100);
        QCOMPARE(phone.levelRecords().size(), 1);
        rx[2] = 0.6;
        m_server->pollVaxLevelsForTest();
        QTRY_COMPARE(phone.levelRecords().size(), 2);
        QCOMPARE(phone.levelRecords().last().value(QStringLiteral("ch3Level")).toDouble(), 0.6);

        const SessionMessage left = phone.invoke(
            "records.unsubscribe",
            {textArg("stream", QString::fromLatin1(StationVax::kLevelsStream))});
        QVERIFY(left.accepted);
        QVERIFY(!m_server->vaxLevelsPollingForTest());
    }

    void newWordingIsPlain()
    {
        for (const QString& reason :
             {QStringLiteral("A VAX level goes from 0 to 1."),
              QStringLiteral("Update this app to change VAX on the Core's computer.")}) {
            QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
            QVERIFY2(OperatorWording::coreCalledStationIn(reason).isEmpty(), qPrintable(reason));
        }
    }

private:
    QTemporaryDir m_securityDir;
    std::unique_ptr<RadioModel> m_core;
    std::unique_ptr<StationServer> m_server;
};

QTEST_MAIN(TstStationVax)
#include "tst_station_vax.moc"
