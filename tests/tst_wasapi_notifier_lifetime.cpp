// =================================================================
// tests/tst_wasapi_notifier_lifetime.cpp (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original Windows COM lifetime regression.
// 2026-10-31: retain the notifier's MTA beyond its setup worker, and retain
// failed-cleanup registrations. J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//
// The native child creates only QCoreApplication and endpoint notifiers:
// no CoreInit, settings, radio, discovery, station server or audio stream.
// It observes the process MTA from fresh threads which never initialize COM.
#include "core/audio/WasapiSystemWin.h" // Windows target version before Qt/PCH
#include "TestFunctionGroups.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QtTest/QtTest>

#include <atomic>
#include <cstdio>
#include <memory>
#include <thread>

using namespace NereusSDR;

namespace {

struct Apartment {
    HRESULT result = E_FAIL;
    APTTYPE type = APTTYPE_CURRENT;
    APTTYPEQUALIFIER qualifier = APTTYPEQUALIFIER_NONE;
};

Apartment queryFromFreshThread()
{
    Apartment apartment;
    std::thread worker([&apartment] {
        apartment.result = CoGetApartmentType(&apartment.type, &apartment.qualifier);
    });
    worker.join();
    return apartment;
}

bool implicitMta(const Apartment& apartment)
{
    // Microsoft's APTTYPEQUALIFIER contract: an uninitialized thread can
    // inherit the process MTA. This query must not call CoInitializeEx.
    // https://learn.microsoft.com/en-us/windows/win32/api/objidl/ne-objidl-apttypequalifier
    return apartment.result == S_OK && apartment.type == APTTYPE_MTA
           && apartment.qualifier == APTTYPEQUALIFIER_IMPLICIT_MTA;
}

int nativeLifetimeChild(bool twoNotifiers)
{
    QJsonObject report;
    const auto record = [&report](const QString& stage, const Apartment& apartment) {
        report.insert(stage, QJsonObject{{"hresult", QString::number(quint32(apartment.result), 16)},
                                        {"type", int(apartment.type)},
                                        {"qualifier", int(apartment.qualifier)}});
    };
    const auto finish = [&report](int code, const QString& reason) {
        report.insert("reason", reason);
        std::fprintf(stdout, "%s\n", QJsonDocument(report).toJson(QJsonDocument::Compact).constData());
        return code;
    };
    const Apartment before = queryFromFreshThread();
    record(QStringLiteral("before"), before);
    if (before.result != CO_E_NOTINITIALIZED) {
        return finish(1, QStringLiteral("Baseline already has COM/MTA support; native test is contaminated"));
    }

    auto first = std::make_unique<WasapiEndpointNotifier>();
    if (!first->registered()) {
        return finish(2, QStringLiteral("Real notifier registration unavailable; inspect Windows audio service/COM diagnostics"));
    }
    const Apartment held = queryFromFreshThread();
    record(QStringLiteral("heldAfterSetupWorkerJoined"), held);
    if (!implicitMta(held)) {
        return finish(3, QStringLiteral("Registered notifier lost its process MTA after setup worker exit"));
    }
    std::unique_ptr<WasapiEndpointNotifier> second;
    if (twoNotifiers) {
        second = std::make_unique<WasapiEndpointNotifier>();
        if (!second->registered()) {
            return finish(4, QStringLiteral("Second real notifier registration unavailable"));
        }
    }
    first.reset();
    if (second) {
        const Apartment oneRemaining = queryFromFreshThread();
        record(QStringLiteral("oneNotifierRemaining"), oneRemaining);
        if (!implicitMta(oneRemaining)) {
            return finish(5, QStringLiteral("Destroying one notifier released the other's MTA"));
        }
        second.reset();
    }
    // Observe eventual COM teardown, not an assumed fixed delay. Each query
    // uses a new thread, so an earlier implicit apartment cannot mask release.
    QElapsedTimer deadline;
    deadline.start();
    Apartment after;
    do {
        after = queryFromFreshThread();
        if (after.result == CO_E_NOTINITIALIZED) {
            break;
        }
        std::this_thread::yield();
    } while (deadline.elapsed() < 2000);
    record(QStringLiteral("afterTeardown"), after);
    if (after.result != CO_E_NOTINITIALIZED) {
        return finish(6, QStringLiteral("MTA remains after the final notifier's successful teardown"));
    }
    return finish(0, QStringLiteral("Notifier retains MTA through registration and releases it at teardown"));
}

struct Probe {
    QStringList events;
    int leases = 0;
    int workerCount = 0;
    int increments = 0;
    int decrements = 0;
    int creates = 0;
    int unregisters = 0;
    int releases = 0;
    bool registered = false;
    bool admitted = true;
    bool lostMtaWhileRegistered = false;
    bool releasedWithoutLease = false;
    HRESULT incrementResult = S_OK;
    HRESULT createResult = S_OK;
    HRESULT registerResult = S_OK;
    HRESULT unregisterResult = S_OK;
    HRESULT decrementResult = S_OK;
    IMMNotificationClient* client = nullptr;
};

// The fake is owned by the fixture; COM Release records the production
// notifier's ownership ending without deleting the fixture's observation data.
class FakeEnumerator final : public IMMDeviceEnumerator {
public:
    explicit FakeEnumerator(std::shared_ptr<Probe> probe) : m_probe(std::move(probe)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 2; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        m_probe->events.append(QStringLiteral("release-enumerator"));
        ++m_probe->releases;
        m_probe->releasedWithoutLease |= m_probe->leases == 0;
        return 0;
    }
    HRESULT STDMETHODCALLTYPE EnumAudioEndpoints(EDataFlow, DWORD, IMMDeviceCollection**) override
    { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDefaultAudioEndpoint(EDataFlow, ERole, IMMDevice**) override
    { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDevice(LPCWSTR, IMMDevice**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE RegisterEndpointNotificationCallback(IMMNotificationClient* client) override
    {
        m_probe->events.append(QStringLiteral("register"));
        if (SUCCEEDED(m_probe->registerResult)) {
            m_probe->registered = true;
            m_probe->client = client;
        }
        return m_probe->registerResult;
    }
    HRESULT STDMETHODCALLTYPE UnregisterEndpointNotificationCallback(IMMNotificationClient* client) override
    {
        m_probe->events.append(QStringLiteral("unregister"));
        ++m_probe->unregisters;
        // A real callback may still be delivered while unregister executes.
        // Its object must still exist; cleanup must not hold the sink's lock.
        if (client != m_probe->client) {
            return E_POINTER;
        }
        client->OnDeviceAdded(L"test-endpoint");
        if (SUCCEEDED(m_probe->unregisterResult) || m_probe->unregisterResult == E_NOTFOUND) {
            m_probe->registered = false;
        }
        return m_probe->unregisterResult;
    }
private:
    std::shared_ptr<Probe> m_probe;
};

struct Fixture {
    std::shared_ptr<Probe> probe = std::make_shared<Probe>();
    std::shared_ptr<FakeEnumerator> enumerator = std::make_shared<FakeEnumerator>(probe);

    WasapiNotifierApi api() const
    {
        return {
            [state = probe](CO_MTA_USAGE_COOKIE* cookie) {
                ++state->increments;
                state->events.append(QStringLiteral("increment"));
                if (SUCCEEDED(state->incrementResult)) {
                    ++state->leases;
                    *cookie = reinterpret_cast<CO_MTA_USAGE_COOKIE>(quintptr(1));
                }
                return state->incrementResult;
            },
            [state = probe](CO_MTA_USAGE_COOKIE) {
                state->events.append(QStringLiteral("decrement"));
                ++state->decrements;
                --state->leases;
                state->lostMtaWhileRegistered |= state->registered && state->leases == 0;
                return state->decrementResult;
            },
            [state = probe, deviceEnumerator = enumerator](IMMDeviceEnumerator** out) {
                ++state->creates;
                if (SUCCEEDED(state->createResult)) {
                    *out = deviceEnumerator.get();
                }
                return state->createResult;
            },
            [state = probe](const std::function<void()>& fn) {
                if (!state->admitted) {
                    return false;
                }
                ++state->workerCount;
                fn();
                --state->workerCount;
                state->events.append(QStringLiteral("worker-left"));
                state->lostMtaWhileRegistered |= state->registered && state->leases == 0;
                return true;
            }};
    }
};

} // namespace

class TestWasapiNotifierLifetime : public QObject {
    Q_OBJECT
private slots:
    void leaseSurvivesSetupAndEndsAfterUnregisterAndRelease()
    {
        Fixture fixture;
        int notices = 0;
        {
            WasapiEndpointNotifier notifier(fixture.api());
            QVERIFY(notifier.registered());
            QCOMPARE(fixture.probe->workerCount, 0);
            QCOMPARE(fixture.probe->leases, 1);
            QCOMPARE(fixture.probe->decrements, 0);
            QVERIFY(!fixture.probe->lostMtaWhileRegistered);
            notifier.setSink([&notices](AudioNotice) { ++notices; });
            fixture.probe->client->OnDeviceAdded(L"test-endpoint");
            QCOMPARE(notices, 1);
        }
        QCOMPARE(fixture.probe->events, QStringList({"increment", "register", "worker-left",
                  "unregister", "release-enumerator", "worker-left", "decrement"}));
        QCOMPARE(fixture.probe->leases, 0);
        QCOMPARE(notices, 1);
        QVERIFY(!fixture.probe->releasedWithoutLease);
        QVERIFY(!fixture.probe->lostMtaWhileRegistered);
    }

    void failedIncrementNeverCreatesOrDecrements()
    {
        Fixture fixture;
        fixture.probe->incrementResult = E_OUTOFMEMORY;
        { WasapiEndpointNotifier notifier(fixture.api()); QVERIFY(!notifier.registered()); }
        QCOMPARE(fixture.probe->creates, 0);
        QCOMPARE(fixture.probe->decrements, 0);
    }

    void failedSetupAdmissionCreatesNoRegistration()
    {
        Fixture fixture;
        fixture.probe->admitted = false;
        { WasapiEndpointNotifier notifier(fixture.api()); QVERIFY(!notifier.registered()); }
        QCOMPARE(fixture.probe->increments, 0);
        QCOMPARE(fixture.probe->creates, 0);
        QCOMPARE(fixture.probe->decrements, 0);
    }

    void failedDecrementIsNotRetried()
    {
        Fixture fixture;
        fixture.probe->decrementResult = E_FAIL;
        { WasapiEndpointNotifier notifier(fixture.api()); QVERIFY(notifier.registered()); }
        QCOMPARE(fixture.probe->decrements, 1);
        QCOMPARE(fixture.probe->releases, 1);
    }

    void failedCreationBalancesLease()
    {
        Fixture fixture;
        fixture.probe->createResult = E_FAIL;
        { WasapiEndpointNotifier notifier(fixture.api()); QVERIFY(!notifier.registered()); }
        QCOMPARE(fixture.probe->decrements, 1);
        QCOMPARE(fixture.probe->unregisters, 0);
        QCOMPARE(fixture.probe->leases, 0);
    }

    void failedRegistrationReleasesEnumeratorBeforeLease()
    {
        Fixture fixture;
        fixture.probe->registerResult = E_FAIL;
        { WasapiEndpointNotifier notifier(fixture.api()); QVERIFY(!notifier.registered()); }
        QCOMPARE(fixture.probe->releases, 1);
        QCOMPARE(fixture.probe->unregisters, 0);
        QCOMPARE(fixture.probe->decrements, 1);
        QVERIFY(!fixture.probe->releasedWithoutLease);
    }

    void notFoundAtUnregisterReleasesNormally()
    {
        Fixture fixture;
        fixture.probe->unregisterResult = E_NOTFOUND;
        { WasapiEndpointNotifier notifier(fixture.api()); QVERIFY(notifier.registered()); }
        QCOMPARE(fixture.probe->releases, 1);
        QCOMPARE(fixture.probe->decrements, 1);
    }

    void failedCleanupRetainsClientEnumeratorAndLease_data()
    {
        QTest::addColumn<bool>("admitted");
        QTest::newRow("unregister-fails") << true;
        QTest::newRow("cleanup-com-unavailable") << false;
    }

    void failedCleanupRetainsClientEnumeratorAndLease()
    {
        QFETCH(bool, admitted);
        Fixture fixture;
        int notices = 0;
        {
            WasapiEndpointNotifier notifier(fixture.api());
            QVERIFY(notifier.registered());
            notifier.setSink([&notices](AudioNotice) { ++notices; });
            fixture.probe->admitted = admitted;
            fixture.probe->unregisterResult = E_FAIL;
        }
        QCOMPARE(fixture.probe->releases, 0);
        QCOMPARE(fixture.probe->decrements, 0);
        QCOMPARE(fixture.probe->leases, 1);
        QVERIFY(fixture.probe->client != nullptr);
        // Unknown cleanup outcome must retain the callback owner, but its
        // sink has been disabled: late notices cannot touch the old consumer.
        QCOMPARE(fixture.probe->client->OnDeviceAdded(L"late-endpoint"), S_OK);
        QCOMPARE(notices, 0);
    }

    void realNotifierKeepsMtaAfterSetupWorkerExits()
    { runNativeChild(QStringLiteral("--native-mta-lifetime")); }

    void twoRealNotifiersHoldMtaIndependently()
    { runNativeChild(QStringLiteral("--native-two-notifiers")); }

private:
    void runNativeChild(const QString& mode)
    {
        QProcess child;
        child.setProcessChannelMode(QProcess::MergedChannels);
        child.start(QCoreApplication::applicationFilePath(), {mode});
        QVERIFY2(child.waitForStarted(3000), qPrintable(child.errorString()));
        if (!child.waitForFinished(10000)) {
            child.kill(); // only this test's owned child, never a station/Core
            child.waitForFinished(3000);
            QFAIL(qPrintable(QStringLiteral("Native notifier child exceeded 10 seconds: ")
                             + QString::fromUtf8(child.readAll())));
        }
        const QByteArray output = child.readAll();
        QVERIFY2(child.exitStatus() == QProcess::NormalExit, output.constData());
        QVERIFY2(child.exitCode() == 0, output.constData());
        qInfo().noquote() << output.trimmed();
    }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--native-mta-lifetime"))) {
        return nativeLifetimeChild(false);
    }
    if (app.arguments().contains(QStringLiteral("--native-two-notifiers"))) {
        return nativeLifetimeChild(true);
    }
    TestWasapiNotifierLifetime test;
    const std::optional<QStringList> arguments = TestFunctionGroups::arguments(
        test.metaObject(), app.arguments(), "NEREUS_WASAPI_NOTIFIER_TEST_GROUP",
        {{QStringLiteral("native"),
          {QStringLiteral("realNotifierKeepsMtaAfterSetupWorkerExits"),
           QStringLiteral("twoRealNotifiersHoldMtaIndependently")}}});
    if (!arguments) {
        return 1;
    }
    return QTest::qExec(&test, *arguments);
}

#include "tst_wasapi_notifier_lifetime.moc"
