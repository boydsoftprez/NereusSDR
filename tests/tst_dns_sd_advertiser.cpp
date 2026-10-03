// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_dns_sd_advertiser.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 16 (D36, R-IOS-16): the Core's Bonjour record and
// DnsSdAdvertiser. The record and the advertiser's bookkeeping run on every
// platform against a fake backend. On macOS the real backend registers a
// record that never leaves this computer (kDnsSdThisComputerOnly, dns_sd's
// local-only interface) and a DNSServiceBrowse in this test finds it and
// reads its TXT keys, before and after a claim. Nothing reaches a network.
//
//   cmake --build build --target tst_dns_sd_advertiser
//   QT_QPA_PLATFORM=offscreen ctest --test-dir build \
//       -R '^tst_dns_sd_advertiser$' --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-25: iPhone app Task 71 (R-IOS-02): the `devices` TXT
//               entry. J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QNetworkInterface>
#include <QRegularExpression>
#include <QSignalSpy>

#include "core/session/DnsSdAdvertiser.h"

#include "OperatorWording.h"

#ifdef Q_OS_MAC
#include <QElapsedTimer>
#include <QSocketNotifier>

#include <arpa/inet.h>
#include <dns_sd.h>
#endif

using namespace NereusSDR;

namespace {

DnsSdRecord record()
{
    DnsSdRecord value;
    value.instanceName = QStringLiteral("KG4VCF/shack");
    value.label = QStringLiteral("KG4VCF/shack");
    for (int i = 0; i < kStationLanIdentityBytes; ++i) {
        value.identity.append(static_cast<char>(0x10 + i));
    }
    value.claimed = false;
    value.pairing = StationLanPairing::Click;
    return value;
}

QMap<QByteArray, QByteArray> asMap(const DnsSdTxtEntries& entries)
{
    QMap<QByteArray, QByteArray> map;
    for (const auto& [key, value] : entries) {
        map.insert(key, value);
    }
    return map;
}

struct FakeBackend final : DnsSdBackend {
    struct Calls {
        bool available = true;
        bool acceptUpdate = true;
        int registers = 0;
        int updates = 0;
        int unregisters = 0;
        quint16 port = 0;
        DnsSdRecord record;
        DnsSdTxtEntries txt;
    };
    explicit FakeBackend(std::shared_ptr<Calls> calls) : m_calls(std::move(calls)) {}

    bool isAvailable() override { return m_calls->available; }
    bool registerService(quint16 port, const DnsSdRecord& value, const DnsSdTxtEntries& txt) override
    {
        ++m_calls->registers;
        m_calls->port = port;
        m_calls->record = value;
        m_calls->txt = txt;
        return true;
    }
    bool updateTxt(const DnsSdRecord& value, const DnsSdTxtEntries& txt) override
    {
        if (!m_calls->acceptUpdate) {
            return false;
        }
        ++m_calls->updates;
        m_calls->record = value;
        m_calls->txt = txt;
        return true;
    }
    void unregisterService() override { ++m_calls->unregisters; }

    std::shared_ptr<Calls> m_calls;
};

#ifdef Q_OS_MAC
// A browse for the service type on this computer only, which resolves the
// instance named `instance` and reports its TXT record.
class LocalBrowse {
public:
    explicit LocalBrowse(QString instance) : m_instance(std::move(instance)) {}
    ~LocalBrowse()
    {
        m_resolveNotifier.reset();
        m_browseNotifier.reset();
        if (m_resolve) {
            DNSServiceRefDeallocate(m_resolve);
        }
        if (m_browse) {
            DNSServiceRefDeallocate(m_browse);
        }
    }

    bool start()
    {
        if (DNSServiceBrowse(&m_browse, 0, kDNSServiceInterfaceIndexLocalOnly, kDnsSdServiceType,
                             nullptr, &LocalBrowse::onBrowse, this)
            != kDNSServiceErr_NoError) {
            return false;
        }
        m_browseNotifier = watch(m_browse);
        return true;
    }

    /// Resolves again (a fresh read of the TXT record).
    void resolve()
    {
        m_resolveNotifier.reset();
        if (m_resolve) {
            DNSServiceRefDeallocate(m_resolve);
            m_resolve = nullptr;
        }
        m_txt.reset();
        const QByteArray name = m_instance.toUtf8();
        if (DNSServiceResolve(&m_resolve, 0, kDNSServiceInterfaceIndexLocalOnly, name.constData(),
                              kDnsSdServiceType, "local.", &LocalBrowse::onResolve, this)
            == kDNSServiceErr_NoError) {
            m_resolveNotifier = watch(m_resolve);
        }
    }

    bool found() const { return m_found; }
    std::optional<QByteArray> txt() const { return m_txt; }
    quint16 port() const { return m_port; }

private:
    std::unique_ptr<QSocketNotifier> watch(DNSServiceRef ref)
    {
        auto notifier = std::make_unique<QSocketNotifier>(DNSServiceRefSockFD(ref),
                                                          QSocketNotifier::Read);
        QObject::connect(notifier.get(), &QSocketNotifier::activated, notifier.get(),
                         [ref]() { DNSServiceProcessResult(ref); });
        return notifier;
    }

    static void DNSSD_API onBrowse(DNSServiceRef, DNSServiceFlags flags, uint32_t,
                                   DNSServiceErrorType error, const char* name, const char*,
                                   const char*, void* context)
    {
        auto* self = static_cast<LocalBrowse*>(context);
        if (error == kDNSServiceErr_NoError && (flags & kDNSServiceFlagsAdd)
            && QString::fromUtf8(name) == self->m_instance) {
            self->m_found = true;
        }
    }

    static void DNSSD_API onResolve(DNSServiceRef, DNSServiceFlags, uint32_t,
                                    DNSServiceErrorType error, const char*, const char*,
                                    uint16_t port, uint16_t txtLength, const unsigned char* txt,
                                    void* context)
    {
        auto* self = static_cast<LocalBrowse*>(context);
        if (error == kDNSServiceErr_NoError) {
            self->m_port = ntohs(port);
            self->m_txt = QByteArray(reinterpret_cast<const char*>(txt), txtLength);
        }
    }

    QString m_instance;
    DNSServiceRef m_browse = nullptr;
    DNSServiceRef m_resolve = nullptr;
    std::unique_ptr<QSocketNotifier> m_browseNotifier;
    std::unique_ptr<QSocketNotifier> m_resolveNotifier;
    bool m_found = false;
    std::optional<QByteArray> m_txt;
    quint16 m_port = 0;
};

// Resolves until the TXT record holds `key` = `value`, or 5 s pass.
QMap<QByteArray, QByteArray> resolveUntil(LocalBrowse& browse, const QByteArray& key,
                                          const QByteArray& value)
{
    QMap<QByteArray, QByteArray> entries;
    QElapsedTimer waited;
    waited.start();
    while (waited.elapsed() < 5000) {
        browse.resolve();
        QElapsedTimer attempt;
        attempt.start();
        while (!browse.txt() && attempt.elapsed() < 1000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
        if (browse.txt()) {
            const auto decoded = decodeDnsSdTxtRecord(*browse.txt());
            if (decoded) {
                entries = asMap(*decoded);
                if (entries.value(key) == value) {
                    return entries;
                }
            }
        }
    }
    return entries;
}
#endif

} // namespace

class TstDnsSdAdvertiser : public QObject {
    Q_OBJECT

private slots:
    void txtRecordCarriesTheKeysInOrder()
    {
        QString error;
        const DnsSdTxtEntries entries = dnsSdTxtEntries(record(), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const QByteArray id = record().identity.toBase64(QByteArray::Base64UrlEncoding
                                                         | QByteArray::OmitTrailingEquals);
        QCOMPARE(id.size(), 43);
        // iPhone app Task 71: `devices`, the sixth, after name; iPhone app
        // plan Task 25 (R-IOS-16): `radio`, the seventh.
        const DnsSdTxtEntries expected{
            {"v", "1"}, {"id", id.left(22)}, {"claimed", "0"}, {"pair", "click"},
            {"name", "KG4VCF/shack"}, {"devices", "0"}, {"radio", "offline"}};
        QCOMPARE(entries, expected);

        const QByteArray bytes = encodeDnsSdTxtRecord(record(), &error);
        QCOMPARE(bytes.at(0), char(3));
        QCOMPARE(bytes.mid(1, 3), QByteArray("v=1"));
        const auto decoded = decodeDnsSdTxtRecord(bytes, &error);
        QVERIFY2(decoded, qPrintable(error));
        QCOMPARE(*decoded, expected);

        DnsSdRecord claimed = record();
        claimed.claimed = true;
        claimed.pairing = StationLanPairing::Closed;
        claimed.label.clear();
        const auto map = asMap(dnsSdTxtEntries(claimed));
        QCOMPARE(map.value("claimed"), QByteArray("1"));
        QCOMPARE(map.value("pair"), QByteArray("closed"));
        QVERIFY(map.contains("name"));
        QVERIFY(map.value("name").isEmpty());

        // The longest label fits one TXT entry uncut.
        DnsSdRecord longest = record();
        longest.label = QString(32, QLatin1Char('K')) + QLatin1Char('/') + QString(32, QLatin1Char('s'));
        QCOMPARE(asMap(dnsSdTxtEntries(longest)).value("name").size(), kStationLanMaxLabelBytes);
    }

    // iPhone app plan Task 25 (R-IOS-16): `radio`, the station's radio state
    // in the announcement's words, so a phone shows "Waiting for a radio"
    // before it connects.
    void theRadioEntryFollowsTheState()
    {
        for (const auto& [state, word] :
             {std::pair{StationLanRadio::Offline, QByteArray("offline")},
              std::pair{StationLanRadio::Connected, QByteArray("connected")},
              std::pair{StationLanRadio::Waiting, QByteArray("waiting")}}) {
            DnsSdRecord stated = record();
            stated.radio = state;
            const DnsSdTxtEntries entries = dnsSdTxtEntries(stated);
            QCOMPARE(entries.last().first, QByteArray("radio"));
            QCOMPARE(entries.last().second, word);
            QCOMPARE(asMap(entries).value("v"), QByteArray("1"));
        }
    }

    void theDevicesEntryFollowsTheCount()
    {
        DnsSdRecord two = record();
        two.devicesConnected = 2;
        QCOMPARE(asMap(dnsSdTxtEntries(two)).value("devices"), QByteArray("2"));
        QCOMPARE(dnsSdTxtEntries(two).at(5).first, QByteArray("devices"));
        QCOMPARE(asMap(dnsSdTxtEntries(two)).value("v"), QByteArray("1"));
        // 0 to 4 only.
        DnsSdRecord five = record();
        five.devicesConnected = 5;
        QString error;
        QVERIFY(dnsSdTxtEntries(five, &error).isEmpty());
        QVERIFY(!error.isEmpty());

        // Updated in place when the count changes.
        auto calls = std::make_shared<FakeBackend::Calls>();
        DnsSdAdvertiser advertiser(std::make_unique<FakeBackend>(calls));
        QVERIFY(advertiser.start(47910, record()));
        advertiser.update(two);
        QCOMPARE(calls->registers, 1);
        QCOMPARE(calls->updates, 1);
        QCOMPARE(asMap(calls->txt).value("devices"), QByteArray("2"));
    }

    void txtDecodingIsStrictAboutLengths()
    {
        QString error;
        QVERIFY(!decodeDnsSdTxtRecord(QByteArray("\x05v=1", 4), &error));
        QVERIFY(!decodeDnsSdTxtRecord(QByteArray("\x00", 1), &error));
        QVERIFY(!decodeDnsSdTxtRecord(QByteArray("\x02=1", 3), &error));
        const auto flag = decodeDnsSdTxtRecord(QByteArray("\x04" "flag", 5), &error);
        QVERIFY(flag);
        QCOMPARE(flag->first(), qMakePair(QByteArray("flag"), QByteArray()));
        QVERIFY(decodeDnsSdTxtRecord(QByteArray(), &error)->isEmpty());
    }

    void unusableRecordsAreRefused()
    {
        auto calls = std::make_shared<FakeBackend::Calls>();
        DnsSdAdvertiser advertiser(std::make_unique<FakeBackend>(calls));
        DnsSdRecord badIdentity = record();
        badIdentity.identity.chop(1);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("refused")));
        QVERIFY(!advertiser.start(47910, badIdentity));
        DnsSdRecord badLabel = record();
        badLabel.label = QStringLiteral("KG4VCF shack");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("refused")));
        QVERIFY(!advertiser.start(47910, badLabel));
        DnsSdRecord badName = record();
        badName.instanceName = QString(64, QLatin1Char('n'));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("refused")));
        QVERIFY(!advertiser.start(47910, badName));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("refused")));
        QVERIFY(!advertiser.start(0, record()));
        QCOMPARE(calls->registers, 0);
        QVERIFY(!advertiser.isActive());
    }

    void instanceNamesFitOneDnsLabel()
    {
        QCOMPARE(dnsSdInstanceName(QStringLiteral("KG4VCF/shack")), QStringLiteral("KG4VCF/shack"));
        QCOMPARE(dnsSdInstanceName(QString()), QStringLiteral("NereusSDR Core"));
        QCOMPARE(dnsSdInstanceName(QStringLiteral(" \t ")), QStringLiteral("NereusSDR Core"));
        QCOMPARE(dnsSdInstanceName(QStringLiteral("Shack\nCore")), QStringLiteral("ShackCore"));
        const QString longest = QString(32, QLatin1Char('K')) + QLatin1Char('/')
            + QString(32, QLatin1Char('s'));
        QCOMPARE(dnsSdInstanceName(longest).toUtf8().size(), kDnsSdMaxInstanceNameBytes);
        // Cut at a character, never inside one: 31 two-byte letters then one
        // more that would make 64 bytes.
        const QString wide(32, QChar(0x00e9));
        QCOMPARE(dnsSdInstanceName(wide), QString(31, QChar(0x00e9)));
    }

    void onlyServedAddressesAreAdvertised()
    {
        QVERIFY(!dnsSdInterfaceForListener(QHostAddress::LocalHost));
        QVERIFY(!dnsSdInterfaceForListener(QHostAddress::LocalHostIPv6));
        QVERIFY(!dnsSdInterfaceForListener(QHostAddress()));
        QVERIFY(!dnsSdInterfaceForListener(QHostAddress::Broadcast));
        QVERIFY(!dnsSdInterfaceForListener(QHostAddress(QStringLiteral("239.255.42.99"))));
        // TEST-NET-1: no interface holds it.
        QVERIFY(!dnsSdInterfaceForListener(QHostAddress(QStringLiteral("192.0.2.200"))));
        QCOMPARE(dnsSdInterfaceForListener(QHostAddress::Any), kDnsSdAllInterfaces);
        QCOMPARE(dnsSdInterfaceForListener(QHostAddress::AnyIPv4), kDnsSdAllInterfaces);
        QCOMPARE(dnsSdInterfaceForListener(QHostAddress::AnyIPv6), kDnsSdAllInterfaces);
        for (const QNetworkInterface& interface : QNetworkInterface::allInterfaces()) {
            if (interface.flags().testFlag(QNetworkInterface::IsLoopBack) || interface.index() <= 0) {
                continue;
            }
            for (const QNetworkAddressEntry& entry : interface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    QCOMPARE(dnsSdInterfaceForListener(entry.ip()),
                             static_cast<quint32>(interface.index()));
                    return;
                }
            }
        }
    }

    void advertiserFollowsTheRecord()
    {
        auto calls = std::make_shared<FakeBackend::Calls>();
        DnsSdAdvertiser advertiser(std::make_unique<FakeBackend>(calls));
        QVERIFY(advertiser.isAvailable());
        QVERIFY(advertiser.start(47910, record()));
        QVERIFY(advertiser.isActive());
        QCOMPARE(calls->registers, 1);
        QCOMPARE(calls->port, quint16(47910));
        QCOMPARE(calls->txt, dnsSdTxtEntries(record()));

        // The same again: nothing to do.
        QVERIFY(advertiser.start(47910, record()));
        advertiser.update(record());
        QCOMPARE(calls->registers, 1);
        QCOMPARE(calls->updates, 0);

        // A claim changes claimed and pair in place.
        DnsSdRecord claimed = record();
        claimed.claimed = true;
        claimed.pairing = StationLanPairing::Closed;
        advertiser.update(claimed);
        QCOMPARE(calls->updates, 1);
        QCOMPARE(asMap(calls->txt).value("claimed"), QByteArray("1"));
        QCOMPARE(asMap(calls->txt).value("pair"), QByteArray("closed"));
        QCOMPARE(advertiser.record(), claimed);

        // A rename: `name` follows; the instance name moves by registering
        // again.
        DnsSdRecord renamed = claimed;
        renamed.label = QStringLiteral("KG4VCF/portable");
        renamed.instanceName = QStringLiteral("KG4VCF/portable");
        advertiser.update(renamed);
        QCOMPARE(calls->registers, 2);
        QCOMPARE(calls->unregisters, 1);
        QCOMPARE(asMap(calls->txt).value("name"), QByteArray("KG4VCF/portable"));

        // A backend that cannot replace the TXT record registers again.
        calls->acceptUpdate = false;
        DnsSdRecord reopened = renamed;
        reopened.pairing = StationLanPairing::Code;
        advertiser.update(reopened);
        QCOMPARE(calls->registers, 3);
        QCOMPARE(asMap(calls->txt).value("pair"), QByteArray("code"));

        // Another port registers again.
        QVERIFY(advertiser.start(47911, reopened));
        QCOMPARE(calls->registers, 4);
        QCOMPARE(calls->port, quint16(47911));

        advertiser.stop();
        QVERIFY(!advertiser.isActive());
        QCOMPARE(calls->unregisters, 4);
        advertiser.update(record());
        QCOMPARE(calls->registers, 4);
    }

    void aFailureAfterRegisteringStopsTheAdvertisement()
    {
        auto calls = std::make_shared<FakeBackend::Calls>();
        auto backend = std::make_unique<FakeBackend>(calls);
        FakeBackend* raw = backend.get();
        DnsSdAdvertiser advertiser(std::move(backend));
        QSignalSpy failed(&advertiser, &DnsSdAdvertiser::failed);
        QVERIFY(advertiser.start(47910, record()));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("stopped")));
        raw->failed(QStringLiteral("name conflict"));
        QCOMPARE(failed.count(), 1);
        QVERIFY(!advertiser.isActive());
    }

    void noBonjourIsSaidOnceInPlainWords()
    {
        const QString text = DnsSdAdvertiser::unavailableText();
        QVERIFY2(OperatorWording::isPlain(text), qPrintable(OperatorWording::internalTermIn(text)));
        QVERIFY2(OperatorWording::coreCalledStationIn(text).isEmpty(), qPrintable(text));

        auto calls = std::make_shared<FakeBackend::Calls>();
        calls->available = false;
        DnsSdAdvertiser advertiser(std::make_unique<FakeBackend>(calls));
        QVERIFY(!advertiser.isAvailable());
        QTest::ignoreMessage(QtWarningMsg, qPrintable(text));
        QVERIFY(!advertiser.start(47910, record()));
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        QVERIFY(!advertiser.start(47910, record()));
        DnsSdRecord claimed = record();
        claimed.claimed = true;
        QVERIFY(!advertiser.start(47910, claimed));
        QCOMPARE(calls->registers, 0);
        QVERIFY(!advertiser.isActive());
    }

    void platformBackendMatchesTheBuild()
    {
        DnsSdAdvertiser advertiser;
#if defined(Q_OS_MAC)
        QVERIFY(advertiser.isAvailable());
#elif defined(Q_OS_LINUX)
        // Available only with Avahi running; either way the call answers.
        qInfo() << "Bonjour on this Linux:" << advertiser.isAvailable();
#endif
    }

#ifdef Q_OS_MAC
    void appleBackendRegistersAndBrowseSeesIt()
    {
        DnsSdRecord value = record();
        // A name no other run shares, on this computer only.
        value.instanceName = QStringLiteral("NereusSDR test %1").arg(QCoreApplication::applicationPid());
        value.interfaceIndex = kDnsSdThisComputerOnly;
        DnsSdAdvertiser advertiser;
        QVERIFY(advertiser.start(47913, value));

        LocalBrowse browse(value.instanceName);
        QVERIFY(browse.start());
        QTRY_VERIFY_WITH_TIMEOUT(browse.found(), 5000);
        QMap<QByteArray, QByteArray> txt = resolveUntil(browse, "pair", "click");
        QCOMPARE(browse.port(), quint16(47913));
        QCOMPARE(txt.value("v"), QByteArray("1"));
        QCOMPARE(txt.value("id"), asMap(dnsSdTxtEntries(value)).value("id"));
        QCOMPARE(txt.value("id").size(), kDnsSdIdentityPrefixChars);
        QCOMPARE(txt.value("claimed"), QByteArray("0"));
        QCOMPARE(txt.value("pair"), QByteArray("click"));
        QCOMPARE(txt.value("name"), QByteArray("KG4VCF/shack"));

        // The Core is claimed: claimed and pair change in place.
        DnsSdRecord claimed = value;
        claimed.claimed = true;
        claimed.pairing = StationLanPairing::Closed;
        advertiser.update(claimed);
        QVERIFY(advertiser.isActive());
        txt = resolveUntil(browse, "claimed", "1");
        QCOMPARE(txt.value("claimed"), QByteArray("1"));
        QCOMPARE(txt.value("pair"), QByteArray("closed"));

        // A rename reaches `name`.
        DnsSdRecord renamed = claimed;
        renamed.label = QStringLiteral("KG4VCF/portable");
        advertiser.update(renamed);
        txt = resolveUntil(browse, "name", "KG4VCF/portable");
        QCOMPARE(txt.value("name"), QByteArray("KG4VCF/portable"));
        advertiser.stop();
    }
#endif
};

QTEST_GUILESS_MAIN(TstDnsSdAdvertiser)
#include "tst_dns_sd_advertiser.moc"
