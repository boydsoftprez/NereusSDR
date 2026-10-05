// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_spot_source_host.cpp  (NereusSDR)
// =================================================================
//
// Parity Task 19 (R-IOS-25, R-R3-49; the iPhone app plan's Task 21 station
// half): SpotSourceHost, the one place the spot sources start and stop.
//
//   - Each placement starts only its own sources from Auto-Connect /
//     Auto-Start: a window running its own radio every one, the Core the
//     station's (DX cluster, RBN, POTA, PSK Reporter), a remote window its
//     own WSJT-X and SpotCollector.
//   - The Core's verbs refuse in plain words (an unknown source, a
//     computer's own listener, no callsign, a cluster that is not
//     connected) and stopping a stopped source changes nothing.
//   - A remote window's station-source buttons are sent to the Core, its
//     own listeners run here, and the Core's state shows through
//     applyStationValue.
//   - A spot's record carries the fields the link names, with the mode the
//     desktop's own resolver gives it (resolvedMode, recordStreamVersion 2).
//   - The WSJT-X and SpotCollector settings are this computer's; the
//     station sources' are the Core's.
//
// No packet leaves the machine: the cluster points at 127.0.0.1 on a port
// nothing listens on, WSJT-X and SpotCollector bind loopback UDP ports,
// POTA and PSK Reporter are never started against their servers.
//
//   cmake --build build --target tst_spot_source_host
//   QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_spot_source_host$' \
//       --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-28: the spot record's resolvedMode (R-IOS-25). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QSignalSpy>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/DxClusterClient.h"
#include "core/PotaClient.h"
#include "core/PskReporterClient.h"
#include "core/SpotCollectorClient.h"
#include "core/SpotSourceHost.h"
#include "core/WsjtxClient.h"
#include "core/settings/SettingsScope.h"
#include "models/Band.h"
#include "models/SpotModeResolver.h"
#include "models/SpotModel.h"

using namespace NereusSDR;

namespace {

struct Sources {
    DxClusterClient dx;
    DxClusterClient rbn;
    WsjtxClient wsjtx;
    SpotCollectorClient spotCollector;
    PotaClient pota;
    PskReporterClient psk;
    SpotModel spots;
    SpotSourceHost host{&dx, &rbn, &wsjtx, &spotCollector, &pota, &psk, &spots};
};

void setAutoStartsOnLoopback()
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("DxClusterAutoConnect"), QStringLiteral("True"));
    s.setValue(QStringLiteral("DxClusterHost"), QStringLiteral("127.0.0.1"));
    s.setValue(QStringLiteral("DxClusterPort"), 18391);
    s.setValue(QStringLiteral("DxClusterCallsign"), QStringLiteral("KG4VCF"));
    s.setValue(QStringLiteral("RbnAutoConnect"), QStringLiteral("True"));
    s.setValue(QStringLiteral("RbnHost"), QStringLiteral("127.0.0.1"));
    s.setValue(QStringLiteral("RbnPort"), 18392);
    s.setValue(QStringLiteral("RbnCallsign"), QStringLiteral("KG4VCF"));
    s.setValue(QStringLiteral("WsjtxAutoStart"), QStringLiteral("True"));
    s.setValue(QStringLiteral("WsjtxAddress"), QStringLiteral("127.0.0.1"));
    s.setValue(QStringLiteral("WsjtxPort"), 28391);
    s.setValue(QStringLiteral("SpotCollectorAutoStart"), QStringLiteral("True"));
    s.setValue(QStringLiteral("SpotCollectorPort"), 28392);
}

} // namespace

class TstSpotSourceHost : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void theCoreStartsOnlyTheStationSources()
    {
        setAutoStartsOnLoopback();
        Sources s;
        QSignalSpy dxError(&s.dx, &DxClusterClient::connectionError);
        QSignalSpy rbnError(&s.rbn, &DxClusterClient::connectionError);
        s.host.restoreAutoStart(SpotSourceHost::Placement::StationSources);
        QCOMPARE(s.host.state(SpotSourceHost::kDxCluster), SpotSourceHost::kConnecting);
        QTRY_VERIFY_WITH_TIMEOUT(dxError.count() > 0, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(rbnError.count() > 0, 5000);
        QCOMPARE(s.host.state(SpotSourceHost::kDxCluster), SpotSourceHost::kError);
        QVERIFY(!s.wsjtx.isListening());
        QVERIFY(!s.spotCollector.isListening());
    }

    void aRemoteWindowStartsOnlyItsOwnListeners()
    {
        setAutoStartsOnLoopback();
        Sources s;
        QSignalSpy dxError(&s.dx, &DxClusterClient::connectionError);
        s.host.restoreAutoStart(SpotSourceHost::Placement::WindowSources);
        QVERIFY(s.wsjtx.isListening());
        QVERIFY(s.spotCollector.isListening());
        QTest::qWait(300);
        QCOMPARE(dxError.count(), 0);
        QVERIFY(!s.dx.isConnected());
        QCOMPARE(s.host.state(SpotSourceHost::kDxCluster), SpotSourceHost::kOff);
    }

    void aLocalWindowStartsEverySourceAsBefore()
    {
        setAutoStartsOnLoopback();
        Sources s;
        QSignalSpy dxError(&s.dx, &DxClusterClient::connectionError);
        s.host.restoreAutoStart(SpotSourceHost::Placement::Everything);
        QVERIFY(s.wsjtx.isListening());
        QVERIFY(s.spotCollector.isListening());
        QTRY_VERIFY_WITH_TIMEOUT(dxError.count() > 0, 5000);
    }

    void theCoresVerbsRefuseInPlainWords()
    {
        Sources s;
        QString reason;
        QVERIFY(!s.host.connectSource(QStringLiteral("nowhere"), &reason));
        QCOMPARE(reason, QStringLiteral("The Core does not run that spot source."));
        QVERIFY(!s.host.connectSource(SpotSourceHost::kWsjtx, &reason));
        QCOMPARE(reason, QStringLiteral("WSJT-X and SpotCollector listen on each computer, not "
                                        "on the Core."));
        QVERIFY(!s.host.connectSource(SpotSourceHost::kDxCluster, &reason));
        QCOMPARE(reason, QStringLiteral("Enter your callsign in Spot Hub first."));
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"), QStringLiteral("KG4VCF"));
        QVERIFY(!s.host.connectSource(SpotSourceHost::kPskReporter, &reason));
        QCOMPARE(reason, QStringLiteral("Enter your callsign and grid square in Spot Hub first."));
        QVERIFY(!s.host.sendCommand(SpotSourceHost::kDxCluster, QStringLiteral("sh/dx"), &reason));
        QCOMPARE(reason, QStringLiteral("The DX cluster is not connected."));
        QVERIFY(!s.host.sendCommand(SpotSourceHost::kRbn, QStringLiteral("  "), &reason));
        QCOMPARE(reason, QStringLiteral("Type a command first."));
        QVERIFY(!s.host.sendCommand(SpotSourceHost::kPota, QStringLiteral("x"), &reason));
        QCOMPARE(reason, QStringLiteral("Only the DX cluster and the Reverse Beacon Network take "
                                        "typed commands."));
        for (const QString& text :
             {QStringLiteral("The Core does not run that spot source."),
              QStringLiteral("Enter your callsign in Spot Hub first."),
              QStringLiteral("The DX cluster is not connected."),
              SpotSourceHost::readOnlyReason()}) {
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
        }
        // Stopping what is not running changes nothing and is taken.
        QSignalSpy changed(&s.host, &SpotSourceHost::sourcesChanged);
        QVERIFY(s.host.disconnectSource(SpotSourceHost::kPota, &reason));
        QCOMPARE(changed.count(), 0);
        QVERIFY(!s.host.disconnectSource(SpotSourceHost::kWsjtx, &reason));
    }

    void theCoreConnectsFromItsSavedSettings()
    {
        auto& settings = AppSettings::instance();
        settings.setValue(QStringLiteral("DxClusterHost"), QStringLiteral("127.0.0.1"));
        settings.setValue(QStringLiteral("DxClusterPort"), 18393);
        settings.setValue(QStringLiteral("User/Callsign"), QStringLiteral("KG4VCF"));
        Sources s;
        QSignalSpy dxError(&s.dx, &DxClusterClient::connectionError);
        QString reason;
        QVERIFY(s.host.connectSource(SpotSourceHost::kDxCluster, &reason));
        QCOMPARE(s.host.dxClusterState(), SpotSourceHost::kConnecting);
        QTRY_VERIFY_WITH_TIMEOUT(dxError.count() > 0, 5000);
        QCOMPARE(s.host.dxClusterState(), SpotSourceHost::kError);
        QVERIFY(!s.host.dxClusterText().isEmpty());
    }

    void aRemoteWindowSendsTheStationSourcesToTheCore()
    {
        Sources s;
        QStringList sent;
        s.host.setStationForwarder([&sent](const QByteArray& verb, const QString& source,
                                           const QString& text, QString* reason) {
            sent.append(QString::fromUtf8(verb) + QLatin1Char(':') + source
                        + (text.isEmpty() ? QString() : QLatin1Char(':') + text));
            if (source == SpotSourceHost::kPota) {
                *reason = QStringLiteral("Not connected to the Core.");
                return false;
            }
            return true;
        });
        QSignalSpy refused(&s.host, &SpotSourceHost::sourceRefused);
        QSignalSpy dxError(&s.dx, &DxClusterClient::connectionError);
        s.host.connectCluster(QStringLiteral("127.0.0.1"), 18394, QStringLiteral("KG4VCF"));
        s.host.connectRbn(QStringLiteral("127.0.0.1"), 18395, QStringLiteral("KG4VCF"));
        s.host.startPota(30);
        s.host.startPskReporter(QStringLiteral("KG4VCF"), QStringLiteral("EM73"));
        s.host.typeCommand(SpotSourceHost::kDxCluster, QStringLiteral("sh/dx 20"));
        s.host.disconnectCluster();
        s.host.clearAllSpots();
        QCOMPARE(sent, (QStringList{QStringLiteral("spots.connect:dxCluster"),
                                    QStringLiteral("spots.connect:rbn"),
                                    QStringLiteral("spots.connect:pota"),
                                    QStringLiteral("spots.connect:pskReporter"),
                                    QStringLiteral("spots.sendCommand:dxCluster:sh/dx 20"),
                                    QStringLiteral("spots.disconnect:dxCluster"),
                                    QStringLiteral("spots.clearAll:")}));
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.first().at(0).toString(), SpotSourceHost::kPota);
        // Nothing dialled here.
        QTest::qWait(200);
        QCOMPARE(dxError.count(), 0);
        QVERIFY(!s.psk.isAutoSendActive());
        // This computer's own listener runs here.
        s.host.startWsjtx(QStringLiteral("127.0.0.1"), 28393);
        QVERIFY(s.wsjtx.isListening());
        QCOMPARE(s.host.state(SpotSourceHost::kWsjtx), SpotSourceHost::kConnected);

        // The Core's state, as its `spotSources` object sends it.
        QSignalSpy changed(&s.host, &SpotSourceHost::sourceChanged);
        QVERIFY(s.host.applyStationValue("dxClusterState", QStringLiteral("connected")));
        QVERIFY(s.host.applyStationValue("dxClusterText", QStringLiteral("")));
        QVERIFY(!s.host.applyStationValue("wsjtxState", QStringLiteral("connected")));
        QVERIFY(!s.host.applyStationValue("dxClusterState", 3));
        QCOMPARE(s.host.dxClusterState(), SpotSourceHost::kConnected);
        QVERIFY(s.host.isRunning(SpotSourceHost::kDxCluster));
        QCOMPARE(changed.count(), 1);
        s.host.clearStationValues();
        QCOMPARE(s.host.dxClusterState(), SpotSourceHost::kOff);
    }

    void aSpotRecordCarriesTheLinksFields()
    {
        SpotData spot;
        spot.index = 7;
        spot.callsign = QStringLiteral("JA1ABC");
        spot.rxFreqMhz = 14.025;
        spot.mode = QStringLiteral("CW");
        spot.source = QStringLiteral("Cluster");
        spot.spotterCallsign = QStringLiteral("W3LPL");
        spot.comment = QStringLiteral("big signal");
        spot.timestamp = QDateTime(QDate(2026, 9, 26), QTime(18, 24), Qt::UTC);
        const QJsonObject f = SpotSourceHost::spotRecordFields(spot, nullptr);
        QCOMPARE(f.keys(), (QStringList{"band", "call", "comment", "dxccColour", "dxccPriority",
                                        "frequencyHz", "mode", "resolvedMode", "source",
                                        "spotter", "timeUtc"}));
        QCOMPARE(f.value("frequencyHz").toDouble(), 14025000.0);
        QCOMPARE(f.value("call").toString(), QStringLiteral("JA1ABC"));
        QCOMPARE(f.value("timeUtc").toString(), QStringLiteral("2026-09-26T18:24:00Z"));
        QCOMPARE(f.value("band").toInt(), static_cast<int>(Band::Band20m));
        QCOMPARE(f.value("dxccColour").toString(), QString());
        QCOMPARE(f.value("dxccPriority").toInt(), 0);
        QCOMPARE(f.value("resolvedMode").toInt(), static_cast<int>(DSPMode::CWU));
    }

    // Spot resolved mode (R-IOS-25, recordStreamVersion 2): the record's
    // resolvedMode is the desktop's own resolver's answer for the same spot,
    // as the slice's dspMode number, and absent when it has none.
    void aSpotRecordCarriesTheDesktopsResolvedMode_data()
    {
        QTest::addColumn<QString>("mode");
        QTest::addColumn<QString>("comment");
        QTest::addColumn<QString>("source");
        QTest::addColumn<double>("mhz");
        QTest::addColumn<int>("expected");  // -1: no resolvedMode

        const auto row = [](const char* name, const char* mode, const char* comment,
                            const char* source, double mhz, int expected) {
            QTest::newRow(name) << QString::fromLatin1(mode) << QString::fromLatin1(comment)
                                << QString::fromLatin1(source) << mhz << expected;
        };
        const auto m = [](DSPMode mode) { return static_cast<int>(mode); };
        row("explicit CW, upper", "CW", "", "Cluster", 14.025, m(DSPMode::CWU));
        row("explicit CW, lower", "CW", "", "Cluster", 7.010, m(DSPMode::CWL));
        row("comment FT8", "", "FT8 -12 dB", "PSK", 14.074, m(DSPMode::DIGU));
        row("comment RTTY last word", "", "TNX QSO RTTY", "Cluster", 14.085, m(DSPMode::DIGL));
        row("band CW segment", "", "big signal", "Cluster", 14.0699, m(DSPMode::CWU));
        // AetherSDR's band inference names DIGU for a digital segment, which
        // its radio mode table does not map: no mode, as on the desktop.
        row("band digital segment", "", "", "RBN", 14.0700, -1);
        row("band phone, upper", "", "", "POTA", 14.250, m(DSPMode::USB));
        row("band phone, lower", "", "", "POTA", 7.200, m(DSPMode::LSB));
        row("SSB below 10 MHz", "SSB", "", "POTA", 3.800, m(DSPMode::LSB));
        row("NFM is FM", "NFM", "", "Cluster", 29.600, m(DSPMode::FM));
        row("FreeDV on 40 m", "", "", "FreeDV", 7.177, m(DSPMode::RADE_L));
        row("FreeDV on 20 m", "", "", "FreeDV", 14.236, m(DSPMode::RADE_U));
        row("unknown mode", "OTHR", "", "Cluster", 14.250, -1);
        row("below the bands", "", "", "Cluster", 0.500, -1);
    }

    void aSpotRecordCarriesTheDesktopsResolvedMode()
    {
        QFETCH(QString, mode);
        QFETCH(QString, comment);
        QFETCH(QString, source);
        QFETCH(double, mhz);
        QFETCH(int, expected);
        SpotData spot;
        spot.index = 3;
        spot.callsign = QStringLiteral("K1ABC");
        spot.rxFreqMhz = mhz;
        spot.txFreqMhz = mhz;
        spot.mode = mode;
        spot.comment = comment;
        spot.source = source;
        const std::optional<DSPMode> desktop = SpotModeResolver::dspModeForSpot(spot);
        QCOMPARE(desktop ? static_cast<int>(*desktop) : -1, expected);
        const QJsonObject f = SpotSourceHost::spotRecordFields(spot, nullptr);
        if (expected < 0) {
            QVERIFY(!f.contains(QStringLiteral("resolvedMode")));
        } else {
            QVERIFY(f.value(QStringLiteral("resolvedMode")).isDouble());
            QCOMPARE(f.value(QStringLiteral("resolvedMode")).toInt(), expected);
        }
    }

    void aTwoMetreSpotCarriesBandTwentySeven()
    {
        // 2 m is its own band (JJ's ruling 2026-09-28): its number is 27,
        // appended after every band that existed before, so no existing
        // spot's band number moves. Either side of 2 m stays GEN (11).
        SpotData spot;
        spot.callsign = QStringLiteral("W1AW");
        spot.mode = QStringLiteral("FT8");
        spot.source = QStringLiteral("PSK");
        spot.timestamp = QDateTime(QDate(2026, 9, 28), QTime(12, 0), Qt::UTC);
        spot.rxFreqMhz = 144.174;
        QCOMPARE(SpotSourceHost::spotRecordFields(spot, nullptr).value("band").toInt(), 27);
        spot.rxFreqMhz = 144.0;
        QCOMPARE(SpotSourceHost::spotRecordFields(spot, nullptr).value("band").toInt(), 27);
        spot.rxFreqMhz = 148.0;
        QCOMPARE(SpotSourceHost::spotRecordFields(spot, nullptr).value("band").toInt(), 27);
        spot.rxFreqMhz = 143.999;
        QCOMPARE(SpotSourceHost::spotRecordFields(spot, nullptr).value("band").toInt(), 11);
        spot.rxFreqMhz = 148.001;
        QCOMPARE(SpotSourceHost::spotRecordFields(spot, nullptr).value("band").toInt(), 11);
        spot.rxFreqMhz = 50.313;
        QCOMPARE(SpotSourceHost::spotRecordFields(spot, nullptr).value("band").toInt(), 10);
    }

    void theListenersAreThisComputersAndTheRestTheCores()
    {
        for (const char* key : {"WsjtxPort", "WsjtxAddress", "WsjtxAutoStart",
                                "SpotCollectorPort", "SpotCollectorAutoStart"}) {
            QCOMPARE(classifySettingsKey(QString::fromLatin1(key)), SettingsScope::OperatorLocal);
        }
        for (const char* key : {"DxClusterHost", "RbnCallsign", "PotaPollInterval",
                                "PskReporterAutoStart", "User/Callsign"}) {
            QCOMPARE(classifySettingsKey(QString::fromLatin1(key)), SettingsScope::Station);
        }
    }
};

QTEST_GUILESS_MAIN(TstSpotSourceHost)
#include "tst_spot_source_host.moc"
