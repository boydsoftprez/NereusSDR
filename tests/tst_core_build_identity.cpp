// no-port-check: NereusSDR-original Core build identity tests.
#include "MultiDeviceHarness.h"

#include "core/BuildIdentity.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/session/StationClient.h"
#include "core/settings/SettingsProxy.h"

namespace {
std::optional<StationCapabilities> lastCapabilities(const LoopbackTransport* app)
{
    std::optional<StationCapabilities> result;
    for (const QByteArray& wire : app->received()) {
        SessionMessage message;
        if (SessionMessages::decode(wire, &message)
            && message.kind == SessionMessageKind::Capabilities) {
            result = StationCapabilities::fromUpdates(message.updates);
        }
    }
    return result;
}

MirrorUpdate buildEntry(const QByteArray& name, const QVariant& value,
                        MirrorWireKind kind = MirrorWireKind::Utf8)
{
    return {0, name, kind, value};
}
} // namespace

class TstCoreBuildIdentity : public QObject {
    Q_OBJECT
private slots:
    void codecRejectsInvalidMetadataWithoutAffectingOtherCapabilities();
    void pairedOptInAndOldMinor();
    void replacementClearsOldCoreIdentity();
};

void TstCoreBuildIdentity::codecRejectsInvalidMetadataWithoutAffectingOtherCapabilities()
{
    const CoreBuildInfo valid{QStringLiteral("0.5.2"), QStringLiteral("branch@abc-dirty")};
    const QByteArray json = valid.toJson();
    QVERIFY(!json.isEmpty());
    const auto decoded = CoreBuildInfo::fromJson(json);
    QVERIFY(decoded);
    QCOMPARE(decoded->productVersion, valid.productVersion);
    QCOMPARE(decoded->sourceTag, valid.sourceTag);
    QVERIFY(CoreBuildInfo::fromJson(CoreBuildInfo{QStringLiteral("0.5.2"), {}}.toJson()));
    QVERIFY(CoreBuildInfo::fromJson(
        R"({"productVersion":"0.5.2","sourceTag":"","future":true})"));

    const QList<QByteArray> invalid{
        {}, "[]", "{}", R"({"productVersion":1,"sourceTag":"a"})",
        R"({"productVersion":"0.5.2"})", R"({"productVersion":"","sourceTag":"a"})",
        R"({"productVersion":"0.5.2","sourceTag":false})",
        QByteArray(4097, 'x'),
        QByteArray(R"({"productVersion":"0.5.2","sourceTag":")")
            + QByteArray(1025, 'x') + R"("})"};
    for (const QByteArray& candidate : invalid) {
        QVERIFY2(!CoreBuildInfo::fromJson(candidate), candidate.constData());
    }
    QVERIFY((CoreBuildInfo{QStringLiteral("v\n1"), {}}.toJson().isEmpty()));
    QVERIFY((CoreBuildInfo{QStringLiteral("v"), QStringLiteral("tag\x7f")}.toJson().isEmpty()));
    QVERIFY((CoreBuildInfo{QString(129, QLatin1Char('x')), {}}.toJson().isEmpty()));
    const QString accent(64, QChar(0x00e9)); // 128 UTF-8 bytes, 64 UTF-16 units.
    QVERIFY(!(CoreBuildInfo{accent, {}}.toJson().isEmpty()));
    QVERIFY((CoreBuildInfo{accent + QChar(0x00e9), {}}.toJson().isEmpty()));
    const QString emoji = QString::fromUtf8(QByteArray::fromHex("f09f9880"));
    const auto emojiRoundtrip = CoreBuildInfo::fromJson(
        CoreBuildInfo{QStringLiteral("v"), emoji}.toJson());
    QVERIFY(emojiRoundtrip);
    QCOMPARE(emojiRoundtrip->sourceTag, emoji);
    QVERIFY((CoreBuildInfo{QStringLiteral("v"), QString(QChar(0xd83d))}.toJson().isEmpty()));
    QVERIFY((CoreBuildInfo{QStringLiteral("v"), QString(QChar(0xde00))}.toJson().isEmpty()));
    QVERIFY(!(CoreBuildInfo{QStringLiteral("v"), QString(512, QChar(0x00e9))}.toJson().isEmpty()));
    QVERIFY((CoreBuildInfo{QStringLiteral("v"), QString(513, QChar(0x00e9))}.toJson().isEmpty()));

    const MirrorUpdate entry = buildEntry("coreBuildInfo", QString::fromUtf8(json));
    auto decode = [](const QList<MirrorUpdate>& entries) {
        return StationCapabilities::fromUpdates(entries).coreBuildInfo;
    };
    QVERIFY(decode({entry}));
    QVERIFY(!decode({entry, entry}));
    QVERIFY(!decode({buildEntry("coreBuildInfo", 1LL, MirrorWireKind::Int64)}));
    MirrorUpdate badOrdinal = entry;
    badOrdinal.ordinal = 1;
    QVERIFY(!decode({badOrdinal}));
    StationCapabilities caps = StationCapabilities::fromUpdates(
        {buildEntry("stationName", QStringLiteral("test")), badOrdinal});
    QCOMPARE(caps.stationName, QStringLiteral("test"));
    QVERIFY(!caps.coreBuildInfo);
}

void TstCoreBuildIdentity::pairedOptInAndOldMinor()
{
    const QString savedVersion = QCoreApplication::applicationVersion();
    const QString savedTag = BuildIdentity::buildTag();
    const auto restore = qScopeGuard([&]() {
        QCoreApplication::setApplicationVersion(savedVersion);
        BuildIdentity::setBuildTag(savedTag);
    });
    QCoreApplication::setApplicationVersion(QStringLiteral("0.5.2"));
    BuildIdentity::setBuildTag(QStringLiteral("test@123-dirty"));
    Core core;
    Device device(QStringLiteral("Desktop"), QStringLiteral("computer"));
    core.pair(device);
    LoopbackTransport* opted = core.signIn(device, {{"deviceAuth", 1}, {"coreBuildInfo", 1}});
    QVERIFY(admitted(opted));
    const auto identity = lastCapabilities(opted);
    QVERIFY(identity && identity->coreBuildInfo);
    QCOMPARE(identity->coreBuildInfo->productVersion, QStringLiteral("0.5.2"));
    QCOMPARE(identity->coreBuildInfo->sourceTag, QStringLiteral("test@123-dirty"));

    LoopbackTransport* noFeature = core.signIn(device, {{"deviceAuth", 1}});
    QVERIFY(admitted(noFeature));
    QVERIFY(lastCapabilities(noFeature));
    QVERIFY(!lastCapabilities(noFeature)->coreBuildInfo);
    LoopbackTransport* old = core.signIn(device, {{"deviceAuth", 1}, {"coreBuildInfo", 1}},
                                        quint16(kRadioIdentitySessionProtocolMinor - 1));
    QVERIFY(admitted(old));
    QVERIFY(lastCapabilities(old));
    QVERIFY(!lastCapabilities(old)->coreBuildInfo);

    QCoreApplication::setApplicationVersion({});
    LoopbackTransport* unknown = core.signIn(device, {{"deviceAuth", 1}, {"coreBuildInfo", 1}});
    QVERIFY(admitted(unknown));
    QVERIFY(lastCapabilities(unknown));
    QVERIFY(!lastCapabilities(unknown)->coreBuildInfo);
}

void TstCoreBuildIdentity::replacementClearsOldCoreIdentity()
{
    const QString savedVersion = QCoreApplication::applicationVersion();
    const QString savedTag = BuildIdentity::buildTag();
    const auto restore = qScopeGuard([&]() {
        QCoreApplication::setApplicationVersion(savedVersion);
        BuildIdentity::setBuildTag(savedTag);
    });
    QCoreApplication::setApplicationVersion(QStringLiteral("0.5.2"));
    BuildIdentity::setBuildTag(QStringLiteral("test@123"));
    Core first;
    Core second;
    QTemporaryDir keyDir;
    auto key = std::make_shared<const ClientDeviceIdentity>(
        ClientDeviceIdentity::loadOrCreate(keyDir.path()));
    QVERIFY(key->isValid());
    PairedDevice record;
    record.id = key->fingerprint();
    record.publicKeySpki = key->publicKeySpki();
    record.name = QStringLiteral("Desktop");
    record.kind = QStringLiteral("computer");
    QVERIFY(first.server->deviceStore()->add(record));
    QVERIFY(second.server->deviceStore()->add(record));
    RadioModel remote(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&remote, &proxy);
    client.setDeviceIdentity(key, QStringLiteral("Desktop"));
    const auto connectTo = [&client](Core& core) {
        auto* app = new LoopbackTransport(QStringLiteral("window"));
        auto* station = new LoopbackTransport(QStringLiteral("core"));
        app->setPeerCertificateSha256(core.certSha256());
        station->linkTo(app);
        client.startSession(app, {}, {}, core.server->stationIdentity().fingerprint());
        core.server->acceptTransport(station);
    };
    connectTo(first);
    QTRY_VERIFY(client.capabilities().coreBuildInfo);
    QCOMPARE(client.capabilities().coreBuildInfo->sourceTag, QStringLiteral("test@123"));

    QCoreApplication::setApplicationVersion({});
    connectTo(second);
    QVERIFY(!client.capabilities().coreBuildInfo);
    QTRY_VERIFY(client.isHandshakeComplete());
    QVERIFY(!client.capabilities().coreBuildInfo);
    client.disconnectFromStation(QStringLiteral("done"));
}

QTEST_MAIN(TstCoreBuildIdentity)
#include "tst_core_build_identity.moc"
