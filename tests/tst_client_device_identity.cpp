// =================================================================
// tests/tst_client_device_identity.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. The names this computer's window
// gives a Core (slice control and shared listening plan Task 8b): each
// profile is its own device, so a profile other than the default carries
// its name; the default profile's names are exactly today's.
//
// Modification history (NereusSDR):
//   2026-09-29: created for NereusSDR by J.J. Boyd (KG4VCF), slice control
//               and shared listening plan Task 8b, with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "core/security/ClientDeviceIdentity.h"
#include "core/security/DeviceStore.h"

using namespace NereusSDR;

class TstClientDeviceIdentity : public QObject {
    Q_OBJECT

private slots:
    // The default profile (empty) gives today's names, byte for byte.
    void theDefaultProfileKeepsTodaysNames()
    {
        for (const QString& host : {QStringLiteral("MacBook-Pro.local"),
                                    QStringLiteral("shack-pc.example.org"), QString()}) {
            QCOMPARE(ClientDeviceIdentity::deviceNameFrom(host, QString()),
                     ClientDeviceIdentity::deviceNameFrom(host));
            QCOMPARE(ClientDeviceIdentity::shortNameFrom(host, QString()),
                     ClientDeviceIdentity::shortNameFrom(host));
            QCOMPARE(ClientDeviceIdentity::deviceNameFrom(host, QStringLiteral("  ")),
                     ClientDeviceIdentity::deviceNameFrom(host));
        }
        QCOMPARE(ClientDeviceIdentity::machineName(QString()), ClientDeviceIdentity::machineName());
        QCOMPARE(ClientDeviceIdentity::machineShortName(QString()),
                 ClientDeviceIdentity::machineShortName());
    }

    // Two profiles on one computer: two plainly different names.
    void aProfileIsInTheName()
    {
        const QString host = QStringLiteral("MacBook-Pro.local");
        const QString profiled =
            ClientDeviceIdentity::deviceNameFrom(host, QStringLiteral("radxa_5c_r3"));
        QCOMPARE(profiled, QStringLiteral("MacBook-Pro (radxa_5c_r3)"));
        QVERIFY(profiled != ClientDeviceIdentity::deviceNameFrom(host));
        QCOMPARE(ClientDeviceIdentity::shortNameFrom(host, QStringLiteral("radxa_5c_r3")),
                 QStringLiteral("MacBook-Pro (radxa_5c_r3)"));
        QVERIFY(DeviceStore::isValidName(profiled));
        QVERIFY(DeviceStore::isValidShortName(
            ClientDeviceIdentity::shortNameFrom(host, QStringLiteral("radxa_5c_r3"))));
    }

    // Long names are cut on the computer's side so the profile always shows,
    // and the whole fits what the Core accepts.
    void aLongNameKeepsItsProfileAndFits()
    {
        const QString host = QString(80, QLatin1Char('h'));
        const QString profile = QString(40, QLatin1Char('p'));
        const QString name = ClientDeviceIdentity::deviceNameFrom(host, profile);
        const QString shortName = ClientDeviceIdentity::shortNameFrom(host, profile);
        QVERIFY(name.endsWith(QLatin1Char(')')));
        QVERIFY(name.contains(QStringLiteral(" (p")));
        QVERIFY(name.toUtf8().size() <= ClientDeviceIdentity::kMaxNameBytes);
        QVERIFY(shortName.endsWith(QLatin1Char(')')));
        QVERIFY(shortName.toUtf8().size() <= ClientDeviceIdentity::kMaxShortNameBytes);
        QVERIFY(DeviceStore::isValidName(name));
        QVERIFY(DeviceStore::isValidShortName(shortName));
        QVERIFY(name.startsWith(QLatin1Char('h')));
        QVERIFY(shortName.startsWith(QLatin1Char('h')));
    }
};

QTEST_MAIN(TstClientDeviceIdentity)
#include "tst_client_device_identity.moc"
