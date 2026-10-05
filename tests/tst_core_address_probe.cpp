// no-port-check: NereusSDR-original loopback checks; no Core/radio/network.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include "gui/CoreAddressProbe.h"
#include "gui/CoreAddressController.h"
#include "core/AppSettings.h"
#include "core/security/StationIdentity.h"
#include "core/session/SessionTransport.h"
#include "fakes/LoopbackTransport.h"
using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;
namespace {
struct WireEvidence {
    QList<QByteArray> applicationFrames;
    QPointer<LoopbackTransport> near;
    QPointer<LoopbackTransport> far;
    bool closed = false;
    bool stopped = false;
    bool started = false;
};
class RecordingTransport final : public LoopbackTransport {
public:
    explicit RecordingTransport(WireEvidence& evidence)
        : LoopbackTransport(QStringLiteral("probe")), m_evidence(evidence) {}
    void sendText(const QByteArray& wire) override {
        m_evidence.applicationFrames.append(wire);
        LoopbackTransport::sendText(wire);
    }
    bool sendBinary(const QByteArray& wire) override {
        m_evidence.applicationFrames.append(wire);
        return LoopbackTransport::sendBinary(wire);
    }
private:
    WireEvidence& m_evidence;
};
class HelloRung final : public PathRung {
public:
    HelloRung(SessionMessage hello, QByteArray certificate, WireEvidence& evidence,
              bool answer = true, bool open = true, PathOutcome failure = PathOutcome::Ready)
        : m_hello(std::move(hello)), m_certificate(std::move(certificate)),
          m_evidence(evidence), m_answer(answer), m_open(open), m_failure(failure) {}
    void start() override {
        m_evidence.started = true;
        if (m_failure != PathOutcome::Ready) {
            emit ended(m_failure, QStringLiteral("Test listener refused the connection."));
            return;
        }
        if (!m_open) { return; }
        auto* near = new RecordingTransport(m_evidence);
        auto* far = new LoopbackTransport(QStringLiteral("test Core"), this);
        near->linkTo(far);
        near->setPeerCertificateSha256(m_certificate);
        m_evidence.near = near;
        m_evidence.far = far;
        connect(near, &SessionTransport::closed, this, [this] { m_evidence.closed = true; });
        emit opened(near);
        if (m_answer) { far->sendText(SessionMessages::encode(m_hello)); }
    }
    void stop() override { m_evidence.stopped = true; }
    int rank() const override { return PathRacer::Direct; }
    PathKind kind() const override { return PathKind::Direct; }
    QString address() const override { return QStringLiteral("loopback.test:47910"); }
private:
    SessionMessage m_hello;
    QByteArray m_certificate;
    WireEvidence& m_evidence;
    bool m_answer;
    bool m_open;
    PathOutcome m_failure;
};
SessionMessage signedHello(const StationIdentity& identity, const QByteArray& cert) {
    SessionMessage hello = SessionMessages::hello(kSessionProtocolMajor, kSessionProtocolMinor,
                                                 0, QStringLiteral("Untrusted hello display name"));
    hello.stationIdentity = SessionStationIdentity{StationIdentity::toBase64Url(identity.publicKeySpki()),
                                                  StationIdentity::toBase64Url(identity.certBinding(cert))};
    return hello;
}
}
class TstCoreAddressProbe : public QObject {
    Q_OBJECT
private slots:
    void refusedListenerNeverSignsInOrWrites() {
        WireEvidence evidence;
        CoreAddressProbe probe(nullptr, [&](const QUrl&) {
            return new HelloRung({}, {}, evidence, false, false, PathOutcome::NoAnswer);
        });
        QSignalSpy result(&probe, &CoreAddressProbe::finished);
        probe.start(QUrl(QStringLiteral("wss://listener.test:47910")), QByteArray(32, 'i'));
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreAddressProbe::Outcome>(result.first().at(0)), CoreAddressProbe::Outcome::Refused);
        QVERIFY(result.first().at(1).toString().contains(QStringLiteral("refused")));
        QVERIFY(evidence.applicationFrames.isEmpty());
        QVERIFY(evidence.near.isNull());
        QVERIFY(!probe.pending());
    }

    void cancellationObserverStartRetainsSingleOwnedRace() {
        WireEvidence first;
        WireEvidence observer;
        WireEvidence outer;
        CoreAddressProbe probe(nullptr, [&](const QUrl& url) {
            WireEvidence& evidence = url.host() == QLatin1String("first.test") ? first
                : url.host() == QLatin1String("observer.test") ? observer : outer;
            return new HelloRung({}, {}, evidence, false);
        }, 5000);
        const QByteArray identity(32, 'i');
        probe.start(QUrl(QStringLiteral("wss://first.test:47910")), identity);
        QTRY_VERIFY_WITH_TIMEOUT(first.near, 1000);
        bool replaced = false;
        connect(&probe, &CoreAddressProbe::finished, &probe, [&](CoreAddressProbe::Outcome outcome, const QString&) {
            if (!replaced && outcome == CoreAddressProbe::Outcome::Cancelled) {
                replaced = true;
                probe.start(QUrl(QStringLiteral("wss://observer.test:47910")), identity);
            }
        });
        probe.start(QUrl(QStringLiteral("wss://outer.test:47910")), identity);
        QTRY_VERIFY_WITH_TIMEOUT(observer.near, 1000);
        probe.cancel();
        QVERIFY(first.closed);
        // The reentrant observer owns the latest check, and cancelling it must
        // close its actual transport rather than leaving an orphan race alive.
        QVERIFY(observer.closed);
        QTRY_VERIFY_WITH_TIMEOUT(first.near.isNull() && observer.near.isNull() && outer.near.isNull(), 1000);
        QVERIFY(first.applicationFrames.isEmpty() && observer.applicationFrames.isEmpty() && outer.applicationFrames.isEmpty());
    }

    void terminalProbeCallbackCanStartAnotherCheck() {
        QTemporaryDir directory;
        StationIdentity identity = StationIdentity::loadOrCreate(directory.path());
        QVERIFY(identity.isValid());
        const QByteArray cert(32, 'c');
        WireEvidence first;
        WireEvidence second;
        int dials = 0;
        CoreAddressProbe probe(nullptr, [&](const QUrl&) {
            return new HelloRung(signedHello(identity, cert), cert, ++dials == 1 ? first : second);
        });
        QSignalSpy results(&probe, &CoreAddressProbe::finished);
        int completions = 0;
        connect(&probe, &CoreAddressProbe::finished, &probe, [&](CoreAddressProbe::Outcome outcome, const QString&) {
            if (++completions == 1 && outcome == CoreAddressProbe::Outcome::Verified) {
                probe.start(QUrl(QStringLiteral("wss://second.test:47911")), identity.fingerprint());
            }
        });
        probe.start(QUrl(QStringLiteral("wss://first.test:47910")), identity.fingerprint());
        QTRY_COMPARE_WITH_TIMEOUT(results.count(), 2, 1000);
        for (const QList<QVariant>& result : results) {
            QCOMPARE(qvariant_cast<CoreAddressProbe::Outcome>(result.at(0)), CoreAddressProbe::Outcome::Verified);
        }
        QVERIFY(first.closed && second.closed);
        QTRY_VERIFY_WITH_TIMEOUT(first.near.isNull() && second.near.isNull(), 1000);
        QVERIFY(first.applicationFrames.isEmpty() && second.applicationFrames.isEmpty());
        QVERIFY(!probe.pending());
    }
    void newCheckCannotConsumeOldCancellationOrHello() {
        QTemporaryDir directory;
        StationIdentity identity = StationIdentity::loadOrCreate(directory.filePath(QStringLiteral("key")));
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget target;
        target.id = QStringLiteral("one");
        target.connection.url = QStringLiteral("wss://configured.test:47910");
        target.connection.identityFingerprint = identity.fingerprint();
        QVERIFY(store.upsert(target));
        const QByteArray cert(32, 'c');
        WireEvidence first;
        WireEvidence second;
        int dials = 0;
        CoreAddressController controller(store, nullptr, [&](const QUrl&) {
            const bool initial = ++dials == 1;
            return new HelloRung(signedHello(identity, cert), cert, initial ? first : second, !initial);
        });
        controller.inspectTarget(target.id);
        QSignalSpy results(&controller, &CoreAddressController::finished);
        const quint64 oldId = controller.addAddress(QStringLiteral("first.test"), 47910);
        QTRY_VERIFY_WITH_TIMEOUT(first.far, 1000);
        first.far->sendText(SessionMessages::encode(signedHello(identity, cert)));
        const quint64 newId = controller.addAddress(QStringLiteral("second.test"), 47911);
        QTRY_COMPARE_WITH_TIMEOUT(results.count(), 2, 1000);
        QCOMPARE(results.at(0).at(0).toULongLong(), oldId);
        QCOMPARE(qvariant_cast<CoreAddressController::Outcome>(results.at(0).at(1)), CoreAddressController::Outcome::Cancelled);
        QCOMPARE(results.at(1).at(0).toULongLong(), newId);
        QCOMPARE(qvariant_cast<CoreAddressController::Outcome>(results.at(1).at(1)), CoreAddressController::Outcome::Saved);
        QCOMPARE(store.target(target.id)->manualAddresses, QStringList{QStringLiteral("wss://second.test:47911")});
        QVERIFY(first.closed && second.closed);
        QTRY_VERIFY_WITH_TIMEOUT(first.near.isNull() && second.near.isNull(), 1000);
        QVERIFY(first.applicationFrames.isEmpty() && second.applicationFrames.isEmpty());
    }

    void staleChecksCannotSave_data() {
        QTest::addColumn<QString>("change");
        for (const char* change : {"cancel", "switch", "forget", "recreate", "reload", "failed-reload", "identity", "pin", "trust", "owner"}) {
            QTest::newRow(change) << QString::fromLatin1(change);
        }
    }
    void staleChecksCannotSave() {
        QFETCH(QString, change);
        QTemporaryDir directory;
        StationIdentity identity = StationIdentity::loadOrCreate(directory.filePath(QStringLiteral("key")));
        QVERIFY(identity.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget target;
        target.id = QStringLiteral("one");
        target.connection.url = QStringLiteral("wss://configured.test:47910");
        target.connection.identityFingerprint = identity.fingerprint();
        QVERIFY(store.upsert(target));
        const QByteArray cert(32, 'c');
        WireEvidence evidence;
        auto controller = std::make_unique<CoreAddressController>(store, nullptr, [&](const QUrl&) {
            return new HelloRung(signedHello(identity, cert), cert, evidence, false);
        });
        controller->inspectTarget(target.id);
        QSignalSpy result(controller.get(), &CoreAddressController::finished);
        controller->addAddress(QStringLiteral("new.test"), 47911);
        QTRY_VERIFY_WITH_TIMEOUT(evidence.far, 1000);
        // Queue a valid old hello, then invalidate its permission to write.
        evidence.far->sendText(SessionMessages::encode(signedHello(identity, cert)));
        if (change == QLatin1String("cancel")) { controller->cancel(); }
        else if (change == QLatin1String("switch")) { controller->inspectTarget(QStringLiteral("other")); }
        else if (change == QLatin1String("owner")) { controller.reset(); }
        else if (change == QLatin1String("forget") || change == QLatin1String("recreate")) {
            QVERIFY(store.remove(target.id));
            if (change == QLatin1String("recreate")) { QVERIFY(store.upsert(target)); }
        } else if (change == QLatin1String("reload")) { QVERIFY(store.load()); }
        else if (change == QLatin1String("failed-reload")) {
            settings.setValue(QStringLiteral("ConnectionTargets/V3"), QStringLiteral("malformed"));
            QVERIFY(!store.load());
        } else {
            if (change == QLatin1String("identity")) { target.connection.identityFingerprint = QByteArray(32, 'x'); }
            if (change == QLatin1String("pin")) { target.connection.fingerprint = QStringLiteral("different pin"); }
            if (change == QLatin1String("trust")) { target.connection.allowUnpinned = true; }
            QVERIFY(store.upsert(target));
        }
        if (controller) {
            QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
            const auto outcome = qvariant_cast<CoreAddressController::Outcome>(result.first().at(1));
            QVERIFY(outcome == CoreAddressController::Outcome::Cancelled || outcome == CoreAddressController::Outcome::StaleTarget);
        }
        QTRY_VERIFY_WITH_TIMEOUT(evidence.near.isNull(), 1000);
        const auto saved = store.target(target.id);
        QVERIFY(!saved || saved->manualAddresses.isEmpty());
        QVERIFY(evidence.applicationFrames.isEmpty());
    }
    void editKeepsOldEntryUntilProofAndDurableSave_data() {
        QTest::addColumn<QString>("variant");
        for (const char* variant : {"success", "wrong-core", "save-failure", "old-entry-changed"}) {
            QTest::newRow(variant) << QString::fromLatin1(variant);
        }
    }
    void editKeepsOldEntryUntilProofAndDurableSave() {
        QFETCH(QString, variant);
        QTemporaryDir directory;
        StationIdentity identity = StationIdentity::loadOrCreate(directory.filePath(QStringLiteral("key")));
        QVERIFY(identity.isValid());
        const QString settingsPath = directory.filePath(QStringLiteral("settings.xml"));
        AppSettings settings(settingsPath);
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget target;
        target.id = QStringLiteral("one");
        target.connection.url = QStringLiteral("wss://configured.test:47910");
        target.connection.identityFingerprint = identity.fingerprint();
        const QString old = QStringLiteral("wss://old.test:47910");
        target.manualAddresses = {old};
        target.connection.cachedAddresses = {old};
        target.connection.coreAddresses = {QStringLiteral("wss://[2001:db8::5]:47910")};
        QVERIFY(store.upsert(target));
        const QByteArray cert(32, 'c');
        WireEvidence evidence;
        CoreAddressController controller(store, nullptr, [&](const QUrl&) {
            return new HelloRung(signedHello(identity, cert), cert, evidence, false);
        });
        controller.inspectTarget(target.id);
        QSignalSpy result(&controller, &CoreAddressController::finished);
        controller.editAddress(old, QStringLiteral("new.test"), 47911);
        QCOMPARE(store.target(target.id)->manualAddresses, QStringList{old});
        QTRY_VERIFY_WITH_TIMEOUT(evidence.far, 1000);
        SessionMessage hello = signedHello(identity, cert);
        if (variant == QLatin1String("wrong-core")) { hello.stationIdentity.reset(); }
        if (variant == QLatin1String("save-failure")) {
            QVERIFY(QFile::remove(settingsPath));
            QVERIFY(QDir().mkpath(settingsPath));
        }
        if (variant == QLatin1String("old-entry-changed")) {
            QVERIFY(store.updateManualAddress(target.id, old, QStringLiteral("wss://concurrent.test:47910")));
        }
        const QString before = settings.value(QStringLiteral("ConnectionTargets/V3")).toString();
        evidence.far->sendText(SessionMessages::encode(hello));
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        const auto outcome = qvariant_cast<CoreAddressController::Outcome>(result.first().at(1));
        if (variant == QLatin1String("success")) {
            QCOMPARE(outcome, CoreAddressController::Outcome::Saved);
            QCOMPARE(store.target(target.id)->manualAddresses, QStringList{QStringLiteral("wss://new.test:47911")});
        } else {
            if (variant == QLatin1String("wrong-core")) { QCOMPARE(outcome, CoreAddressController::Outcome::Refused); }
            if (variant == QLatin1String("save-failure")) { QCOMPARE(outcome, CoreAddressController::Outcome::SaveFailed); }
            if (variant == QLatin1String("old-entry-changed")) { QCOMPARE(outcome, CoreAddressController::Outcome::StaleTarget); }
            QCOMPARE(settings.value(QStringLiteral("ConnectionTargets/V3")).toString(), before);
            QVERIFY(!result.first().at(2).toString().isEmpty());
        }
        QCOMPARE(store.target(target.id)->connection.cachedAddresses, QStringList{old});
        QCOMPARE(store.target(target.id)->connection.coreAddresses, QStringList{QStringLiteral("wss://[2001:db8::5]:47910")});
        QVERIFY(evidence.applicationFrames.isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(evidence.near.isNull(), 1000);
    }
    void inspectionAndManualRemovalDoNotDialOrSelect() {
        QTemporaryDir directory;
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget target;
        target.id = QStringLiteral("one");
        target.connection.url = QStringLiteral("wss://configured.test:47910");
        target.connection.identityFingerprint = QByteArray(32, 'i');
        target.manualAddresses = {QStringLiteral("wss://[2001:db8::5]:47910")};
        target.connection.cachedAddresses = target.manualAddresses;
        target.connection.coreAddresses = target.manualAddresses;
        QVERIFY(store.upsert(target));
        int dials = 0;
        CoreAddressController controller(store, nullptr, [&](const QUrl&) -> PathRung* { ++dials; return nullptr; });
        controller.inspectTarget(target.id);
        QCOMPARE(dials, 0);
        QVERIFY(controller.removeAddress(target.manualAddresses.first()));
        QCOMPARE(dials, 0);
        QCOMPARE(store.selectedId(), QStringLiteral("local"));
        QVERIFY(store.target(target.id)->manualAddresses.isEmpty());
        QCOMPARE(store.target(target.id)->connection.cachedAddresses, target.connection.cachedAddresses);
        QCOMPARE(store.target(target.id)->connection.coreAddresses, target.connection.coreAddresses);
        QCOMPARE(store.target(target.id)->connection.url, target.connection.url);
    }
    void refusesUnknownUnpairedUnpinnedInvalidDuplicateAndFullTargets_data() {
        QTest::addColumn<QString>("variant");
        for (const char* variant : {"unknown", "unpaired", "unpinned", "invalid", "duplicate", "full", "old-missing"}) {
            QTest::newRow(variant) << QString::fromLatin1(variant);
        }
    }
    void refusesUnknownUnpairedUnpinnedInvalidDuplicateAndFullTargets() {
        QFETCH(QString, variant);
        QTemporaryDir directory;
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget target;
        target.id = QStringLiteral("one");
        target.connection.url = QStringLiteral("wss://configured.test:47910");
        target.connection.identityFingerprint = QByteArray(32, 'i');
        if (variant == QLatin1String("unpaired")) { target.connection.identityFingerprint.clear(); }
        if (variant == QLatin1String("unpinned")) { target.connection.allowUnpinned = true; }
        if (variant == QLatin1String("duplicate")) { target.manualAddresses = {QStringLiteral("wss://new.test:47911")}; }
        if (variant == QLatin1String("full")) {
            target.manualAddresses = {QStringLiteral("wss://a.test:1"), QStringLiteral("wss://b.test:2"),
                QStringLiteral("wss://c.test:3"), QStringLiteral("wss://d.test:4")};
        }
        QVERIFY(store.upsert(target));
        int dials = 0;
        CoreAddressController controller(store, nullptr, [&](const QUrl&) -> PathRung* { ++dials; return nullptr; });
        controller.inspectTarget(variant == QLatin1String("unknown") ? QStringLiteral("missing") : target.id);
        const QString before = settings.value(QStringLiteral("ConnectionTargets/V3")).toString();
        QSignalSpy result(&controller, &CoreAddressController::finished);
        if (variant == QLatin1String("old-missing")) {
            controller.editAddress(QStringLiteral("wss://missing.test:1"), QStringLiteral("new.test"), 47911);
        } else { controller.addAddress(variant == QLatin1String("invalid") ? QStringLiteral("https://new.test") : QStringLiteral("new.test"), 47911); }
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(dials, 0);
        QCOMPARE(qvariant_cast<CoreAddressController::Outcome>(result.first().at(1)), CoreAddressController::Outcome::Refused);
        QVERIFY(!result.first().at(2).toString().isEmpty());
        QCOMPARE(settings.value(QStringLiteral("ConnectionTargets/V3")).toString(), before);
    }

    void checkedAddressSavesFreshRecordWithoutLearningHelloName() {
        QTemporaryDir directory;
        StationIdentity identity = StationIdentity::loadOrCreate(directory.filePath(QStringLiteral("key")));
        QVERIFY(identity.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("settings.xml")));
        CoreTargetStore store(settings);
        QVERIFY(store.load());
        SavedCoreTarget target;
        target.id = QStringLiteral("one");
        target.label = QStringLiteral("Local nickname");
        target.connection.url = QStringLiteral("wss://configured.test:47910");
        target.connection.identityFingerprint = identity.fingerprint();
        QVERIFY(store.upsert(target));
        QVERIFY(store.select(target.id));
        const QByteArray cert(32, 'c');
        WireEvidence evidence;
        CoreAddressController controller(store, nullptr, [&](const QUrl&) {
            return new HelloRung(signedHello(identity, cert), cert, evidence);
        });
        controller.inspectTarget(target.id);
        QSignalSpy result(&controller, &CoreAddressController::finished);
        const quint64 operation = controller.addAddress(QStringLiteral("New.TEST"), 47911);
        // A later authenticated observation must survive the probe's save.
        QVERIFY(store.rememberAddress(target.id, QStringLiteral("wss://history.test:47910")));
        target = *store.target(target.id);
        target.lastRadioName = QStringLiteral("Fresh radio evidence");
        QVERIFY(store.upsert(target));
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(result.first().at(0).toULongLong(), operation);
        QCOMPARE(qvariant_cast<CoreAddressController::Outcome>(result.first().at(1)), CoreAddressController::Outcome::Saved);
        QCOMPARE(store.target(target.id)->manualAddresses, QStringList{QStringLiteral("wss://new.test:47911")});
        QCOMPARE(store.target(target.id)->connection.cachedAddresses, QStringList{QStringLiteral("wss://history.test:47910")});
        QCOMPARE(store.target(target.id)->lastRadioName, QStringLiteral("Fresh radio evidence"));
        QCOMPARE(store.target(target.id)->label, QStringLiteral("Local nickname"));
        const QJsonObject saved = QJsonDocument::fromJson(settings.value(QStringLiteral("ConnectionTargets/V3"))
            .toString().toUtf8()).object().value(QStringLiteral("cores")).toArray().first().toObject();
        QVERIFY(!saved.contains(QStringLiteral("lastKnownCoreName")));
        QCOMPARE(store.selectedId(), target.id);
        QVERIFY(evidence.applicationFrames.isEmpty());
    }

    void exactIdentityClosesWinnerWithoutSending() {
        QTemporaryDir directory;
        StationIdentity identity = StationIdentity::loadOrCreate(directory.path());
        QVERIFY(identity.isValid());
        const QByteArray cert(32, 'c');
        WireEvidence evidence;
        QUrl dialled;
        CoreAddressProbe probe(nullptr, [&](const QUrl& url) {
            dialled = url;
            return new HelloRung(signedHello(identity, cert), cert, evidence);
        });
        QSignalSpy result(&probe, &CoreAddressProbe::finished);
        probe.start(QUrl(QStringLiteral("wss://loopback.test:47910")), identity.fingerprint());
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreAddressProbe::Outcome>(result.first().at(0)), CoreAddressProbe::Outcome::Verified);
        QVERIFY(evidence.started);
        QCOMPARE(dialled, QUrl(QStringLiteral("wss://loopback.test:47910")));
        QVERIFY(evidence.closed);
        QTRY_VERIFY_WITH_TIMEOUT(evidence.near.isNull(), 1000);
        QVERIFY(evidence.applicationFrames.isEmpty());
        QVERIFY(!probe.pending());
    }
    void invalidIdentity_data() {
        QTest::addColumn<int>("variant");
        QTest::newRow("wrong-key") << 0;
        QTest::newRow("missing-identity") << 1;
        QTest::newRow("different-peer-certificate") << 2;
        QTest::newRow("empty-peer-certificate") << 3;
        QTest::newRow("forged-binding") << 4;
    }
    void invalidIdentity() {
        QFETCH(int, variant);
        QTemporaryDir directory;
        StationIdentity identity = StationIdentity::loadOrCreate(directory.path());
        QVERIFY(identity.isValid());
        const QByteArray cert(32, 'c');
        QByteArray actualCert = cert;
        QByteArray expected = identity.fingerprint();
        SessionMessage hello = signedHello(identity, cert);
        if (variant == 0) { expected = QByteArray(32, 'x'); }
        if (variant == 1) { hello.stationIdentity.reset(); }
        if (variant == 2) { actualCert = QByteArray(32, 'd'); }
        if (variant == 3) { actualCert.clear(); }
        if (variant == 4) { hello.stationIdentity->certBinding = StationIdentity::toBase64Url(QByteArray(64, 'x')); }
        WireEvidence evidence;
        CoreAddressProbe probe(nullptr, [&](const QUrl&) { return new HelloRung(hello, actualCert, evidence); });
        QSignalSpy result(&probe, &CoreAddressProbe::finished);
        probe.start(QUrl(QStringLiteral("wss://loopback.test:47910")), expected);
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreAddressProbe::Outcome>(result.first().at(0)), CoreAddressProbe::Outcome::Refused);
        QVERIFY(!result.first().at(1).toString().isEmpty());
        QVERIFY(evidence.closed);
        QTRY_VERIFY_WITH_TIMEOUT(evidence.near.isNull(), 1000);
        QVERIFY(evidence.applicationFrames.isEmpty());
    }
    void wholeDeadlineIncludesSocketOpen_data() {
        QTest::addColumn<bool>("opens");
        QTest::newRow("socket-never-opens") << false;
        QTest::newRow("hello-never-arrives") << true;
    }
    void wholeDeadlineIncludesSocketOpen() {
        QFETCH(bool, opens);
        WireEvidence evidence;
        CoreAddressProbe probe(nullptr, [&](const QUrl&) { return new HelloRung({}, {}, evidence, false, opens); }, 30);
        QSignalSpy result(&probe, &CoreAddressProbe::finished);
        probe.start(QUrl(QStringLiteral("wss://loopback.test:47910")), QByteArray(32, 'i'));
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 1000);
        QCOMPARE(qvariant_cast<CoreAddressProbe::Outcome>(result.first().at(0)), CoreAddressProbe::Outcome::TimedOut);
        QVERIFY(evidence.stopped);
        QTRY_VERIFY_WITH_TIMEOUT(evidence.near.isNull(), 1000);
        QVERIFY(evidence.applicationFrames.isEmpty());
    }
    void cancelAndOwnerDestructionClosePendingLinks() {
        for (const bool destroy : {false, true}) {
            WireEvidence evidence;
            auto probe = std::make_unique<CoreAddressProbe>(nullptr, [&](const QUrl&) {
                return new HelloRung({}, {}, evidence, false);
            });
            QSignalSpy result(probe.get(), &CoreAddressProbe::finished);
            probe->start(QUrl(QStringLiteral("wss://loopback.test:47910")), QByteArray(32, 'i'));
            QTRY_VERIFY_WITH_TIMEOUT(evidence.near, 1000);
            if (destroy) { probe.reset(); }
            else {
                probe->cancel();
                QCOMPARE(result.count(), 1);
                QCOMPARE(qvariant_cast<CoreAddressProbe::Outcome>(result.first().at(0)), CoreAddressProbe::Outcome::Cancelled);
            }
            QVERIFY(evidence.closed);
            QTRY_VERIFY_WITH_TIMEOUT(evidence.near.isNull(), 1000);
            QVERIFY(evidence.applicationFrames.isEmpty());
        }
    }
};
QTEST_GUILESS_MAIN(TstCoreAddressProbe)
#include "tst_core_address_probe.moc"
