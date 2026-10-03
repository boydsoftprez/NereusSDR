// no-port-check: NereusSDR-original. transmitSettingsVersion 15: cfc.setProfile,
// the CFC dialog's whole band editor (each band's frequency, compression,
// post-EQ gain and Q, the range, and the pre-compression and post-EQ gain)
// applied at once from an app, against the revision the app last saw.
// Spoken over the link directly as the phone speaks it. It is the device's
// own cfcParaEqData write: the write's gates, the side-effect delta, and the
// kept profile in the result. Loopback link, no radio, no audio device,
// never keyed.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29  J.J. Boyd / KG4VCF  Created (transmitSettingsVersion 15,
//                                    Setup description version 19).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/CfcProfile.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/MoxController.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationServer.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

// A profile of `bands` bands from 100 to 3100 Hz, each value distinct.
CfcProfile::Profile makeProfile(int bands)
{
    CfcProfile::Profile p;
    p.minHz = 100.0;
    p.maxHz = 3100.0;
    p.postMinHz = p.minHz;
    p.postMaxHz = p.maxHz;
    p.precompDb = 4.5;
    p.postEqGainDb = -3.0;
    for (int i = 0; i < bands; ++i) {
        const double f = p.minHz + (p.maxHz - p.minHz) * i / (bands - 1);
        p.f.push_back(std::nearbyint(f * 1000.0) / 1000.0);
        p.postF.push_back(p.f.back());
        p.g.push_back(double(i % 16));
        p.e.push_back(double((i % 9) - 4));
        p.qg.push_back(2.0 + 0.5 * (i % 5));
        p.qe.push_back(3.0 + 0.25 * (i % 4));
    }
    return p;
}

QString revisionOf(const QString& published)
{
    return QJsonDocument::fromJson(published.toUtf8()).object()
        .value(QStringLiteral("revision")).toString();
}

const QString kStale = QStringLiteral(
    "The CFC settings changed on the Core. Check the new values and try again.");
const QString kUpdateApp = QStringLiteral("Update this app to change the CFC settings on this Core.");

std::unique_ptr<RadioModel> makeStationRadioModel()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::HermesLite);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:01");
    info.name = QStringLiteral("Bench HL2");
    info.boardType = HPSDRHW::HermesLite;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    return model;
}

// One device signed in with the Core's token, speaking the link directly.
struct Device {
    Device(StationServer* server, QObject* parent, const QHash<QByteArray, int>& features,
           quint16 minor = kSessionProtocolMinor)
    {
        app = new LoopbackTransport(QStringLiteral("app"), parent);
        station = new LoopbackTransport(QStringLiteral("station"), server);
        station->linkTo(app);
        server->acceptTransport(station);
        static_cast<void>(QTest::qWaitFor([this]() { return !app->received().isEmpty(); }, 5000));
        app->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, minor, 0, QStringLiteral("CFC phone"),
            {kSessionProtocolMajor}, features)));
        app->sendText(SessionMessages::encode(SessionMessages::authRequest(server->token())));
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
    SessionMessage invoke(const QByteArray& verb, const QList<MirrorUpdate>& arguments)
    {
        const quint32 id = ++nextId;
        app->sendText(SessionMessages::encode(SessionMessages::commandInvoke(verb, id, arguments)));
        SessionMessage found;
        found.reason = QStringLiteral("no command.result arrived");
        static_cast<void>(QTest::qWaitFor([&]() {
            for (const SessionMessage& message : messages()) {
                if (message.kind == SessionMessageKind::CommandResult
                    && message.commandId == id) {
                    found = message;
                    return true;
                }
            }
            return false;
        }, 3000));
        return found;
    }
    SessionMessage setProfile(const QString& profileJson, const QString& expectedRevision)
    {
        return invoke(QByteArrayLiteral("cfc.setProfile"),
                      {MirrorUpdate{0, "profileJson", MirrorWireKind::Utf8, QVariant(profileJson)},
                       MirrorUpdate{0, "expectedRevision", MirrorWireKind::Utf8,
                                    QVariant(expectedRevision)}});
    }
    // The index in messages() of the command.result for `id`, or -1.
    int resultIndex(quint32 id) const
    {
        const QList<SessionMessage> all = messages();
        for (int i = 0; i < all.size(); ++i) {
            if (all[i].kind == SessionMessageKind::CommandResult && all[i].commandId == id) {
                return i;
            }
        }
        return -1;
    }
    // The last `transmit` value this device heard for `name`, at or before
    // message `upTo` (all when -1).
    QVariant transmitValue(const QByteArray& name, int upTo = -1) const
    {
        QVariant value;
        const QList<SessionMessage> all = messages();
        for (int i = 0; i < all.size() && (upTo < 0 || i <= upTo); ++i) {
            const SessionMessage& message = all[i];
            if ((message.kind != SessionMessageKind::ObjectCreate
                 && message.kind != SessionMessageKind::Delta)
                || message.objectKey != QByteArrayLiteral("transmit")) {
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

    LoopbackTransport* app = nullptr;
    LoopbackTransport* station = nullptr;
    quint32 nextId = 8800;
};

QString resultValue(const SessionMessage& result, const QByteArray& name)
{
    for (const MirrorUpdate& value : result.updates) {
        if (value.name == name) {
            return value.value.toString();
        }
    }
    return {};
}

} // namespace

class TstCfcSetProfile : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_securityDir.isValid());
        AppSettings::setProfileOverride(
            QStringLiteral("cfc-set-profile-%1").arg(QCoreApplication::applicationPid()));
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

    // Version 15 is offered to every app; the Core's published profile
    // reaches only an app that declared cfcProfile 1.
    void theProfileReachesADeclaringAppOnly()
    {
        Device plain(m_server.get(), this, {});
        QVERIFY(plain.ready());
        QCOMPARE(plain.capability("transmitSettingsVersion"), kTransmitSettingsCfcProfileVersion);
        QVERIFY(!plain.transmitValue("cfcProfile").isValid());
        QVERIFY(plain.transmitValue("cfcParaEqData").isValid());
        Device phone(m_server.get(), this, {{"cfcProfile", 1}});
        QVERIFY(phone.ready());
        QCOMPARE(phone.transmitValue("cfcProfile").toString(),
                 m_core->transmitModel().cfcProfile());
    }

    // The whole editor is applied at once for 5, 10 and 18 bands: the kept
    // profile comes back in the side-effect delta, then in the result, and
    // the revision moves.
    void aProfileIsAppliedWhole_data()
    {
        QTest::addColumn<int>("bands");
        QTest::newRow("5") << 5;
        QTest::newRow("10") << 10;
        QTest::newRow("18") << 18;
    }
    void aProfileIsAppliedWhole()
    {
        QFETCH(int, bands);
        Device phone(m_server.get(), this, {{"cfcProfile", 1}});
        QVERIFY(phone.ready());
        Device plain(m_server.get(), this, {});
        QVERIFY(plain.ready());
        const TransmitModel& tx = m_core->transmitModel();
        const QString before = tx.cfcProfile();
        const CfcProfile::Profile wanted = makeProfile(bands);
        const SessionMessage result =
            phone.setProfile(CfcProfile::publishedJson(wanted, QStringLiteral("saved")),
                             revisionOf(before));
        QVERIFY2(result.accepted, qPrintable(result.reason));
        const QString kept = CfcProfile::publishedJson(wanted, QStringLiteral("saved"));
        QCOMPARE(tx.cfcProfile(), kept);
        QCOMPARE(resultValue(result, "profile"), kept);
        QVERIFY(revisionOf(kept) != revisionOf(before));

        CfcProfile::Profile saved;
        QVERIFY(CfcProfile::decode(tx.cfcParaEqData(), saved));
        QCOMPARE(int(saved.f.size()), bands);
        QCOMPARE(saved.precompDb, 4.5);
        QCOMPARE(saved.postEqGainDb, -3.0);
        QCOMPARE(tx.cfcPrecompDb(), 5);
        QCOMPARE(tx.cfcPostEqGainDb(), -3);

        const int at = phone.resultIndex(result.commandId);
        QVERIFY(at > 0);
        QCOMPARE(phone.transmitValue("cfcProfile", at).toString(), kept);
        QCOMPARE(phone.transmitValue("cfcParaEqData", at).toString(), tx.cfcParaEqData());
        // An app that did not declare cfcProfile hears the saved blob only.
        QTRY_COMPARE(plain.transmitValue("cfcParaEqData").toString(), tx.cfcParaEqData());
        QVERIFY(!plain.transmitValue("cfcProfile").isValid());
    }

    // A revision the Core has moved past is refused, and nothing changes.
    void aStaleRevisionIsRefused()
    {
        Device phone(m_server.get(), this, {{"cfcProfile", 1}});
        QVERIFY(phone.ready());
        const QString first = m_core->transmitModel().cfcProfile();
        QVERIFY(phone.setProfile(CfcProfile::publishedJson(makeProfile(10), QStringLiteral("saved")),
                                 revisionOf(first)).accepted);
        const QString now = m_core->transmitModel().cfcProfile();
        const QString data = m_core->transmitModel().cfcParaEqData();
        const SessionMessage stale = phone.setProfile(
            CfcProfile::publishedJson(makeProfile(5), QStringLiteral("saved")), revisionOf(first));
        QVERIFY(!stale.accepted);
        QCOMPARE(stale.reason, kStale);
        QVERIFY(resultValue(stale, "profile").isEmpty());
        QCOMPARE(m_core->transmitModel().cfcProfile(), now);
        QCOMPARE(m_core->transmitModel().cfcParaEqData(), data);
    }

    // A profile outside the dialog's choices is refused whole with the
    // reader's reason, as is one the Core cannot read.
    void aProfileOutsideTheChoicesChangesNothing()
    {
        Device phone(m_server.get(), this, {{"cfcProfile", 1}});
        QVERIFY(phone.ready());
        const QString before = m_core->transmitModel().cfcParaEqData();
        const QString revision = revisionOf(m_core->transmitModel().cfcProfile());

        CfcProfile::Profile loud = makeProfile(10);
        loud.g[3] = 17.0;
        SessionMessage result =
            phone.setProfile(CfcProfile::publishedJson(loud, QStringLiteral("saved")), revision);
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, QStringLiteral("Choose each band's compression from 0 to 16 dB."));

        CfcProfile::Profile gain = makeProfile(18);
        gain.e[5] = 25.0;
        result = phone.setProfile(CfcProfile::publishedJson(gain, QStringLiteral("saved")), revision);
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, QStringLiteral("Choose each band's post-EQ gain from -24 to 24 dB."));

        CfcProfile::Profile precomp = makeProfile(5);
        precomp.precompDb = 16.5;
        result = phone.setProfile(CfcProfile::publishedJson(precomp, QStringLiteral("saved")), revision);
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, QStringLiteral("Choose a pre-compression from 0 to 16 dB."));

        CfcProfile::Profile wide = makeProfile(5);
        wide.maxHz = 20001.0;
        wide.f.back() = 20001.0;
        result = phone.setProfile(CfcProfile::publishedJson(wide, QStringLiteral("saved")), revision);
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, QStringLiteral("Choose a low and a high end from 0 to 20000 Hz, the "
                                               "high end at least 1000 Hz above the low end."));

        result = phone.setProfile(QStringLiteral("[1,2]"), revision);
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, QStringLiteral("The CFC settings were not understood."));
        result = phone.invoke(QByteArrayLiteral("cfc.setProfile"),
                              {MirrorUpdate{0, "profileJson", MirrorWireKind::Utf8,
                                            QVariant(CfcProfile::publishedJson(
                                                makeProfile(5), QStringLiteral("saved")))}});
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, QStringLiteral("The CFC settings were not understood."));
        QCOMPARE(m_core->transmitModel().cfcParaEqData(), before);
    }

    // An app before the Core's radio-identity minor is told to update.
    void anOlderAppIsRefused()
    {
        Device older(m_server.get(), this, {},
                     quint16(kRadioIdentitySessionProtocolMinor - 1));
        QVERIFY(older.ready());
        const QString before = m_core->transmitModel().cfcParaEqData();
        const SessionMessage result = older.setProfile(
            CfcProfile::publishedJson(makeProfile(10), QStringLiteral("saved")),
            revisionOf(m_core->transmitModel().cfcProfile()));
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, kUpdateApp);
        QCOMPARE(m_core->transmitModel().cfcParaEqData(), before);
    }

    // On a Core that allows remote transmit, the profile meets the gate a
    // cfcParaEqData write meets, with the same words.
    void theTransmitGateIsTheWritesGate()
    {
        m_server->setRemoteTransmitAllowed(true);
        Device phone(m_server.get(), this, {{"cfcProfile", 1}});
        QVERIFY(phone.ready());
        const QString before = m_core->transmitModel().cfcParaEqData();
        const SessionMessage result = phone.setProfile(
            CfcProfile::publishedJson(makeProfile(10), QStringLiteral("saved")),
            revisionOf(m_core->transmitModel().cfcProfile()));
        QVERIFY(!result.accepted);
        QVERIFY(!result.reason.isEmpty());

        const quint32 writeId = 4242;
        phone.app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            QByteArrayLiteral("transmit"),
            {MirrorUpdate{1, "cfcParaEqData", MirrorWireKind::Utf8,
                          QVariant(CfcProfile::encode(makeProfile(10)))}},
            writeId)));
        QString writeReason;
        QVERIFY(QTest::qWaitFor([&]() {
            for (const SessionMessage& message : phone.messages()) {
                if (message.kind == SessionMessageKind::PropertyResult
                    && message.writeId == writeId && !message.propertyResults.isEmpty()) {
                    writeReason = message.propertyResults.first().reason;
                    return true;
                }
            }
            return false;
        }, 3000));
        QCOMPARE(result.reason, writeReason);
        QCOMPARE(m_core->transmitModel().cfcParaEqData(), before);
    }

    // A peer that may not change the transmit settings is told that first,
    // whatever revision or values it sent: not that its table is stale, and
    // not which value is out of range.
    void permissionIsCheckedBeforeRevisionAndValues()
    {
        m_server->setRemoteTransmitAllowed(true);
        Device phone(m_server.get(), this, {{"cfcProfile", 1}});
        QVERIFY(phone.ready());
        const QString before = m_core->transmitModel().cfcParaEqData();

        const quint32 writeId = 4343;
        phone.app->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            QByteArrayLiteral("transmit"),
            {MirrorUpdate{1, "cfcParaEqData", MirrorWireKind::Utf8,
                          QVariant(CfcProfile::encode(makeProfile(10)))}},
            writeId)));
        QString writeReason;
        QVERIFY(QTest::qWaitFor([&]() {
            for (const SessionMessage& message : phone.messages()) {
                if (message.kind == SessionMessageKind::PropertyResult
                    && message.writeId == writeId && !message.propertyResults.isEmpty()) {
                    writeReason = message.propertyResults.first().reason;
                    return true;
                }
            }
            return false;
        }, 3000));
        QVERIFY(!writeReason.isEmpty());

        SessionMessage result = phone.setProfile(
            CfcProfile::publishedJson(makeProfile(10), QStringLiteral("saved")),
            QStringLiteral("not-the-cores-revision"));
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, writeReason);

        CfcProfile::Profile wide = makeProfile(5);
        wide.maxHz = 20001.0;
        wide.f.back() = 20001.0;
        result = phone.setProfile(CfcProfile::publishedJson(wide, QStringLiteral("saved")),
                                  revisionOf(m_core->transmitModel().cfcProfile()));
        QVERIFY(!result.accepted);
        QCOMPARE(result.reason, writeReason);
        QCOMPARE(m_core->transmitModel().cfcParaEqData(), before);
    }

    // Since transmitSettingsVersion 13 the Core takes the CFC settings on
    // the air, as a local window does, so it takes the profile too.
    void theProfileIsTakenOnTheAir()
    {
        Device phone(m_server.get(), this, {{"cfcProfile", 1}});
        QVERIFY(phone.ready());
        MoxController* const mox = m_core->moxController();
        QVERIFY(mox);
        mox->setMoxCheck({});
        mox->setMox(true);
        QTRY_VERIFY(mox->state() == MoxState::Tx);
        const CfcProfile::Profile wanted = makeProfile(18);
        const SessionMessage set = phone.setProfile(
            CfcProfile::publishedJson(wanted, QStringLiteral("saved")),
            revisionOf(m_core->transmitModel().cfcProfile()));
        QVERIFY2(set.accepted, qPrintable(set.reason));
        QCOMPARE(m_core->transmitModel().cfcProfile(),
                 CfcProfile::publishedJson(wanted, QStringLiteral("saved")));
        mox->setMox(false);
        QTRY_VERIFY(mox->state() == MoxState::Rx);
    }

private:
    QTemporaryDir m_securityDir;
    std::unique_ptr<RadioModel> m_core;
    std::unique_ptr<StationServer> m_server;
};

QTEST_MAIN(TstCfcSetProfile)
#include "tst_cfc_set_profile.moc"
