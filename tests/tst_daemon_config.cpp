// =================================================================
// tests/tst_daemon_config.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// R1 Task 9: DaemonConfig parses nereusd's own "key = value" config file
// (default /etc/nereusd.conf, overridable with --config). Test bodies are
// the brief's own (task-9-brief.md Step 1) verbatim.
//
// R1 Task 9 fix round 1: three more slots pin resolveDaemonProfileArgument
// (--profile support, closing the gap described in DaemonConfig.h's own
// comment on that function and task-9-report.md section 5). This is new
// glue logic this task wrote, not a re-test of AppSettings::
// isValidProfileName()'s own accept/reject rules -- those already have
// dedicated coverage in tests/tst_app_settings_profile.cpp.
//
// R1 merge-blocker fix round: sampleFileKeysAndParserKeysAgree pins the
// shipped packaging/nereusd.conf.sample against the parser, because the
// sample file documented keys the daemon did nothing with.
//
// R-R3-22 / R-R3-47 (2026-09-24): station_bind, and the older
// station_tci_bind still read beneath it.
//
// iPhone app Task 12 (2026-09-24): the listener is on by default (47910,
// every interface), a file that sets remote_port or remote_bind keeps its
// meaning, and pairing_lan_click.
//
// Remote Daemon R2, Task 1: resolveDaemonProfileArgument() gained a
// `wasSet` parameter and inverted its absent-flag default (nereusd's
// own reserved profile instead of silently sharing the GUI's directory).
// absentProfileArgumentResolvesToReservedDaemonProfile() below replaces
// the old emptyProfileArgumentMeansNoProfile() slot, which pinned exactly
// the contract this task inverts; explicitlyEmptyProfileArgumentStillShares()
// pins the escape hatch that keeps the old behaviour reachable on purpose.
// tests/tst_daemon_settings_profile.cpp is the fuller seam test for this
// change (constants, the settings/log directory move, the first-run seed
// marker); the slots here stay focused on resolveDaemonProfileArgument()'s
// own argument-resolution contract.
//
// iPhone app plan Task 27 (2026-09-26): rendezvous_servers and relay.
//
// 2026-09-23: listenAddressFor() pins remote_bind "::" as dual stack, and
// the shipped sample may no longer pin sample_rate_hz, by J.J. Boyd
// (KG4VCF), with AI-assisted implementation via Anthropic Claude Code.
// 2026-09-27: comments follow sample_rate_hz becoming a starting value only
// (R-R3-49), by J.J. Boyd (KG4VCF), with AI-assisted implementation via
// Anthropic Claude Code.
// =================================================================

#include <QtTest>
#include <QFile>
#include <QTemporaryFile>
#include <QTextStream>
#include <QRegularExpression>
#include "core/AppSettings.h"
#include "core/daemon/DaemonConfig.h"

using namespace NereusSDR;

class TstDaemonConfig : public QObject {
    Q_OBJECT
private slots:
    void rejectsMalformedRadioIdentityBeforeStartingListener()
    {
        DaemonConfig cfg = DaemonConfig::defaults();
        QString error;
        for (const QString& value : {QString(), QStringLiteral("aa:BB:01:02:03:04")}) {
            cfg.radioMac = value;
            QVERIFY(cfg.validate(&error));
        }
        for (const QString& value : {QStringLiteral("aa:bb:cc"), QStringLiteral("gg:01:02:03:04:05"),
                                    QStringLiteral("aa-bb-cc-dd-ee-ff"), QStringLiteral("aa:bb:cc:dd:ee:ff\n")}) {
            cfg.radioMac = value;
            QVERIFY(!cfg.validate(&error));
            QVERIFY(error.contains(QStringLiteral("radio_mac")));
        }
    }

    void coreNameBoundsUseUtf8Bytes()
    {
        DaemonConfig cfg = DaemonConfig::defaults();
        QString error;
        cfg.coreName = QString::fromUtf8("🛰 Shack");
        QVERIFY(cfg.validate(&error));
        cfg.coreName = QString(128, QLatin1Char('A'));
        QVERIFY(cfg.validate(&error));
        cfg.coreName = QString(65, QChar(0x00e9));
        QVERIFY(!cfg.validate(&error));
        cfg.coreName = QStringLiteral("Shack\nForged");
        QVERIFY(!cfg.validate(&error));
        cfg.coreName = QString(QChar(0xd800));
        QVERIFY(!cfg.validate(&error));
    }

    void defaultsAreValid()
    {
        QString err;
        QVERIFY(DaemonConfig::defaults().validate(&err));
        QVERIFY2(err.isEmpty(), qPrintable(err));
    }

    void parsesAWellFormedFile()
    {
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write("radio_mac = 00:1C:2D:05:37:2A\n"
                "sample_rate_hz = 384000\n"
                "slice_count = 3\n"
                "audio_device = hw:CARD=Device\n"
                "display_application_bytes_per_second = 2400000\n"
                "spectrum_sample_units_per_second = 1800000\n");
        f.flush();
        QString err;
        DaemonConfig c = DaemonConfig::fromFile(f.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(c.radioMac, QStringLiteral("00:1C:2D:05:37:2A"));
        QCOMPARE(c.sampleRateHz, 384000);
        QCOMPARE(c.sliceCount, 3);
        QCOMPARE(c.audioDevice, QStringLiteral("hw:CARD=Device"));
        QVERIFY(c.sampleRateExplicit);
        QVERIFY(c.displayApplicationBytesPerSecond.has_value());
        QVERIFY(c.spectrumSampleUnitsPerSecond.has_value());
        QCOMPARE(*c.displayApplicationBytesPerSecond, quint64(2400000));
        QCOMPARE(*c.spectrumSampleUnitsPerSecond, quint64(1800000));
        const std::optional<DisplayBudgetLimits> limits = c.displayBudgetLimits();
        QVERIFY(limits.has_value());
        QCOMPARE(limits->applicationBytesPerSecond, quint64(2400000));
        QCOMPARE(limits->spectrumSampleUnitsPerSecond, quint64(1800000));
        QCOMPARE(limits->generation, quint32(1));
    }

    // sampleRateHz always holds a usable rate (validate() rejects <= 0), so
    // it cannot itself express "the operator did not ask for one".
    // DaemonApp::applyConfigToSettings seeds the rate into the per-MAC
    // AppSettings key the shared connect path reads, when that radio has
    // no saved rate, which is persisted state the GUI reads back on its
    // next launch. Without a separate "was it asked for" flag, a bare
    // `nereusd` with no config file (a non-fatal case: server_main warns
    // and continues with defaults) would seed the 192000 struct default
    // into a radio the config file never mentioned. Pinned in both
    // directions.
    void sampleRateExplicitOnlyWhenTheKeyIsPresent()
    {
        QTemporaryFile absent;
        QVERIFY(absent.open());
        absent.write("radio_mac = aa:bb:cc:dd:ee:ff\n"
                     "slice_count = 1\n");
        absent.flush();
        QString err;
        const DaemonConfig noKey = DaemonConfig::fromFile(absent.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QVERIFY(!noKey.sampleRateExplicit);
        QCOMPARE(noKey.sampleRateHz, DaemonConfig::defaults().sampleRateHz);

        // Defaults, and an unreadable file that falls back to them, are
        // equally "not asked for".
        QVERIFY(!DaemonConfig::defaults().sampleRateExplicit);
        const DaemonConfig missing =
            DaemonConfig::fromFile(QStringLiteral("/nonexistent/nereusd.conf"), &err);
        QVERIFY(!missing.sampleRateExplicit);

        // A present but unparseable value keeps the default rate, and must
        // not count as explicit either.
        QTemporaryFile garbage;
        QVERIFY(garbage.open());
        garbage.write("sample_rate_hz = not-a-number\n");
        garbage.flush();
        const DaemonConfig bad = DaemonConfig::fromFile(garbage.fileName(), &err);
        QVERIFY(!bad.sampleRateExplicit);

        // And the value the operator did ask for still round-trips, even
        // when it happens to equal the default.
        QTemporaryFile same;
        QVERIFY(same.open());
        same.write("sample_rate_hz = 192000\n");
        same.flush();
        const DaemonConfig explicitDefault =
            DaemonConfig::fromFile(same.fileName(), &err);
        QVERIFY(explicitDefault.sampleRateExplicit);
        QCOMPARE(explicitDefault.sampleRateHz, 192000);
    }

    // Every key nereusd.conf.sample documents must parse, and nothing it
    // does not document may. The reason this is pinned: the sample file
    // shipped `log_level` for a while, which parsed into a struct field
    // that no production code ever read, so an operator setting it got
    // silence. Removing a key from the struct without removing it from
    // the sample file (or the reverse) now fails here.
    void sampleFileKeysAndParserKeysAgree()
    {
        const QStringList documented = {
            QStringLiteral("radio_mac"),
            QStringLiteral("sample_rate_hz"),
            QStringLiteral("slice_count"),
            QStringLiteral("audio_device"),
            // Remote Daemon R2 Task 18: both reach DaemonApp::
            // startStationServer(), which is what this pinning test is
            // for -- a key that reaches DaemonConfig must reach behaviour
            // AND the sample file.
            QStringLiteral("remote_port"),
            QStringLiteral("remote_bind"),
            QStringLiteral("core_name"),
            QStringLiteral("display_application_bytes_per_second"),
            QStringLiteral("spectrum_sample_units_per_second"),
            // R-R3-23: reaches DaemonMediaController::setAudioTargetBitrate().
            QStringLiteral("audio_bitrate"),
            // R-R3-41: reaches startDaemonThreadPlacement() in server_main.
            QStringLiteral("thread_placement"),
            // R-R3-08/37/40: reaches DaemonApp::startStationServer(), which
            // owns the display load governor.
            QStringLiteral("display_adaptive"),
            // R-R3-23: reaches DaemonMediaController::setAudioLosslessAllowed().
            QStringLiteral("audio_lossless"),
            // R-R3-22 / R-R3-47 / R-R3-48: reaches RadioModel::setStationBind()
            // and enableStationTci(). (The older station_tci_bind is read
            // too but no longer documented as a key of its own; see
            // stationBindReadsTheOlderTciName.)
            QStringLiteral("station_bind"),
            // iPhone app Task 12: reaches StationServer::
            // setPairingLanClickAllowed() from DaemonApp::startStationServer().
            QStringLiteral("pairing_lan_click"),
            // iPhone app plan Task 34: reaches StationServer::
            // setRemoteTransmitAllowed() and the model's receive-only policy
            // from DaemonApp::start().
            QStringLiteral("remote_transmit"),
            // iPhone app plan Task 27: reach RendezvousClient from
            // DaemonApp::startStationServer().
            QStringLiteral("rendezvous_servers"),
            QStringLiteral("relay"),
            // iPhone app Task 17: reach StationStatusPage from DaemonApp::
            // startStationServer(), and the control socket's place (the
            // daemon's and every console command's).
            QStringLiteral("status_page"),
            QStringLiteral("status_port"),
            QStringLiteral("state_directory"),
        };

        // Each documented key parses without an "unknown key" complaint.
        // fromFile logs unknown keys via qCWarning; QTest turns an
        // unexpected qWarning into a test failure only with
        // QTest::failOnWarning, so assert on the parsed value instead.
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write("radio_mac = aa:bb:cc:dd:ee:ff\n"
                "sample_rate_hz = 96000\n"
                "slice_count = 2\n"
                "audio_device = default\n"
                "remote_port = 4711\n"
                "remote_bind = 0.0.0.0\n"
                "core_name = Rock 5C\n"
                "display_application_bytes_per_second = 2400000\n"
                "spectrum_sample_units_per_second = 1800000\n"
                "audio_bitrate = 24000\n"
                "thread_placement = off\n"
                "display_adaptive = off\n"
                "audio_lossless = deny\n"
                "station_bind = 192.168.1.20\n"
                "pairing_lan_click = deny\n"
                "remote_transmit = deny\n"
                "rendezvous_servers = rv.example.net:8443, rv.nereussdr.com\n"
                "relay = deny\n"
                "status_page = off\n"
                "status_port = 8080\n"
                "state_directory = /var/lib/nereusd\n");
        f.flush();
        QString err;
        const DaemonConfig c = DaemonConfig::fromFile(f.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(c.radioMac, QStringLiteral("aa:bb:cc:dd:ee:ff"));
        QCOMPARE(c.sampleRateHz, 96000);
        QCOMPARE(c.sliceCount, 2);
        QCOMPARE(c.audioDevice, QStringLiteral("default"));
        QCOMPARE(c.remotePort, 4711);
        QCOMPARE(c.remoteBind, QStringLiteral("0.0.0.0"));
        QCOMPARE(c.coreName, QStringLiteral("Rock 5C"));
        QCOMPARE(c.audioBitrate, 24000);
        QCOMPARE(c.threadPlacement, false);
        QCOMPARE(c.displayAdaptive, false);
        QCOMPARE(c.audioLosslessAllowed, false);
        QCOMPARE(c.stationBind, QStringLiteral("192.168.1.20"));
        QCOMPARE(c.pairingLanClickAllowed, false);
        QCOMPARE(c.remoteTransmitAllowed, false);
        QCOMPARE(c.rendezvousServers,
                 QStringList({QStringLiteral("rv.example.net:8443"),
                              QStringLiteral("rv.nereussdr.com")}));
        QCOMPARE(c.relayAllowed, false);
        QCOMPARE(c.statusPage, false);
        QCOMPARE(c.statusPort, 8080);
        QCOMPARE(c.stateDirectory, QStringLiteral("/var/lib/nereusd"));
        const std::optional<DisplayBudgetLimits> limits = c.displayBudgetLimits();
        QVERIFY(limits.has_value());
        QCOMPARE(limits->applicationBytesPerSecond, quint64(2400000));
        QCOMPARE(limits->spectrumSampleUnitsPerSecond, quint64(1800000));
        QCOMPARE(limits->generation, quint32(1));

        // And the shipped sample file documents exactly those keys, no
        // more. Parsed straight out of the packaging file so the two
        // cannot drift.
        QFile sample(QStringLiteral(NEREUS_SOURCE_DIR
                                    "/packaging/nereusd.conf.sample"));
        QVERIFY2(sample.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(sample.fileName()));
        QStringList found;
        QTextStream ts(&sample);
        while (!ts.atEnd()) {
            QString line = ts.readLine().trimmed();
            // Optional settings are documented as commented placeholder
            // assignments so copying the sample cannot accidentally enable
            // an unmeasured production limit. They still belong to the
            // parser/sample parity contract.
            if (line.startsWith(QLatin1Char('#'))) {
                line.remove(0, 1);
                line = line.trimmed();
                const int eq = line.indexOf(QLatin1Char('='));
                const QString placeholder = eq > 0 ? line.mid(eq + 1).trimmed() : QString();
                if (eq > 0 && placeholder.startsWith(QLatin1Char('<'))
                    && placeholder.endsWith(QLatin1Char('>'))) {
                    found << line.left(eq).trimmed();
                }
                continue;
            }
            const int hash = line.indexOf(QLatin1Char('#'));
            if (hash >= 0) { line.truncate(hash); }
            line = line.trimmed();
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq > 0) { found << line.left(eq).trimmed(); }
        }
        found.sort();
        QStringList expected = documented;
        expected.sort();
        QCOMPARE(found, expected);
    }

    // iPhone app plan Task 27: the remote access service is rv.nereussdr.com
    // and the relay is allowed unless the file says otherwise; an empty list
    // names no service; an entry that is not a server address is skipped with
    // a warning; any relay value but allow or deny keeps allow.
    void rendezvousDefaultsAndBadValues()
    {
        const DaemonConfig defaults = DaemonConfig::defaults();
        QCOMPARE(defaults.rendezvousServers, QStringList({QStringLiteral("rv.nereussdr.com")}));
        QCOMPARE(defaults.relayAllowed, true);

        QTemporaryFile empty;
        QVERIFY(empty.open());
        empty.write("rendezvous_servers =\n");
        empty.flush();
        QString err;
        QCOMPARE(DaemonConfig::fromFile(empty.fileName(), &err).rendezvousServers,
                 QStringList());

        QTemporaryFile bad;
        QVERIFY(bad.open());
        bad.write("rendezvous_servers = http://rv.example.net ws://rv.example.net ws://127.0.0.1:8710\n"
                  "relay = maybe\n");
        bad.flush();
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("rendezvous_servers entry.*http")));
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(
                                 QStringLiteral("rendezvous_servers entry.*ws://rv.example.net")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("relay must be")));
        const DaemonConfig parsed = DaemonConfig::fromFile(bad.fileName(), &err);
        // A plain ws:// address only to this computer (a service running
        // beside the Core).
        QCOMPARE(parsed.rendezvousServers, QStringList({QStringLiteral("ws://127.0.0.1:8710")}));
        QCOMPARE(parsed.relayAllowed, true);
    }

    // R-R3-23: 24000 and 48000 are the only encoder profiles. R-R3-21: a
    // missing key is 48000 (fullband) and an explicit 24000 still works; any
    // other value logs exactly one warning and keeps 48000, like
    // remote_port's unparseable-value handling, never a startup error.
    void audioBitrateAcceptsOnlyTheTwoProfiles_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::addColumn<int>("expected");
        QTest::addColumn<bool>("warns");
        QTest::newRow("missing") << QByteArray("slice_count = 1\n") << 48000 << false;
        QTest::newRow("24000") << QByteArray("audio_bitrate = 24000\n") << 24000 << false;
        QTest::newRow("48000") << QByteArray("audio_bitrate = 48000\n") << 48000 << false;
        QTest::newRow("32000") << QByteArray("audio_bitrate = 32000\n") << 48000 << true;
        QTest::newRow("zero") << QByteArray("audio_bitrate = 0\n") << 48000 << true;
        QTest::newRow("negative") << QByteArray("audio_bitrate = -48000\n") << 48000 << true;
        QTest::newRow("words") << QByteArray("audio_bitrate = fast\n") << 48000 << true;
        QTest::newRow("empty") << QByteArray("audio_bitrate =\n") << 48000 << true;
        QTest::newRow("24k-then-bad")
            << QByteArray("audio_bitrate = 24000\naudio_bitrate = 96000\n") << 48000 << true;
    }

    void audioBitrateAcceptsOnlyTheTwoProfiles()
    {
        QFETCH(QByteArray, text);
        QFETCH(int, expected);
        QFETCH(bool, warns);
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(text);
        f.flush();
        if (warns) {
            QTest::ignoreMessage(QtWarningMsg,
                QRegularExpression(QStringLiteral("audio_bitrate must be 24000 or 48000, keeping 48000")));
        }
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        QString err;
        const DaemonConfig c = DaemonConfig::fromFile(f.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(c.audioBitrate, expected);
        QVERIFY(c.validate(&err));
    }

    // R-R3-41: thread_placement is auto (the default) or off; any other
    // value logs one warning and keeps auto.
    void threadPlacementAcceptsAutoOrOff_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::addColumn<bool>("expected");
        QTest::addColumn<bool>("warns");
        QTest::newRow("missing") << QByteArray("slice_count = 1\n") << true << false;
        QTest::newRow("auto") << QByteArray("thread_placement = auto\n") << true << false;
        QTest::newRow("off") << QByteArray("thread_placement = off\n") << false << false;
        QTest::newRow("words") << QByteArray("thread_placement = yes\n") << true << true;
        QTest::newRow("off-then-bad")
            << QByteArray("thread_placement = off\nthread_placement = on\n") << true << true;
    }

    void threadPlacementAcceptsAutoOrOff()
    {
        QFETCH(QByteArray, text);
        QFETCH(bool, expected);
        QFETCH(bool, warns);
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(text);
        f.flush();
        if (warns) {
            QTest::ignoreMessage(QtWarningMsg,
                QRegularExpression(QStringLiteral("thread_placement must be auto or off, keeping auto")));
        }
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        QString err;
        const DaemonConfig c = DaemonConfig::fromFile(f.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(c.threadPlacement, expected);
    }

    // R-R3-08/37/40: display_adaptive is on (the default) or off; any other
    // value logs one warning and keeps on.
    void displayAdaptiveAcceptsOnOrOff_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::addColumn<bool>("expected");
        QTest::addColumn<bool>("warns");
        QTest::newRow("missing") << QByteArray("slice_count = 1\n") << true << false;
        QTest::newRow("on") << QByteArray("display_adaptive = on\n") << true << false;
        QTest::newRow("off") << QByteArray("display_adaptive = off\n") << false << false;
        QTest::newRow("words") << QByteArray("display_adaptive = auto\n") << true << true;
        QTest::newRow("off-then-bad")
            << QByteArray("display_adaptive = off\ndisplay_adaptive = yes\n") << true << true;
    }

    void displayAdaptiveAcceptsOnOrOff()
    {
        QFETCH(QByteArray, text);
        QFETCH(bool, expected);
        QFETCH(bool, warns);
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(text);
        f.flush();
        if (warns) {
            QTest::ignoreMessage(QtWarningMsg,
                QRegularExpression(QStringLiteral("display_adaptive must be on or off, keeping on")));
        }
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        QString err;
        const DaemonConfig c = DaemonConfig::fromFile(f.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(c.displayAdaptive, expected);
        QCOMPARE(DaemonConfig::defaults().displayAdaptive, true);
    }

    // R-R3-23: lossless audio is allowed unless the file says deny. Any
    // other value logs exactly one warning and keeps allow, like
    // audio_bitrate, never a startup error.
    void audioLosslessIsAllowOrDeny_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::addColumn<bool>("expected");
        QTest::addColumn<bool>("warns");
        QTest::newRow("missing") << QByteArray("slice_count = 1\n") << true << false;
        QTest::newRow("allow") << QByteArray("audio_lossless = allow\n") << true << false;
        QTest::newRow("deny") << QByteArray("audio_lossless = deny\n") << false << false;
        QTest::newRow("Deny") << QByteArray("audio_lossless = Deny\n") << false << false;
        QTest::newRow("comment") << QByteArray("audio_lossless = deny # digital modes off\n")
                                 << false << false;
        QTest::newRow("words") << QByteArray("audio_lossless = no\n") << true << true;
        QTest::newRow("empty") << QByteArray("audio_lossless =\n") << true << true;
        QTest::newRow("deny-then-bad")
            << QByteArray("audio_lossless = deny\naudio_lossless = maybe\n") << true << true;
    }

    void audioLosslessIsAllowOrDeny()
    {
        QFETCH(QByteArray, text);
        QFETCH(bool, expected);
        QFETCH(bool, warns);
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(text);
        f.flush();
        if (warns) {
            QTest::ignoreMessage(QtWarningMsg,
                QRegularExpression(QStringLiteral("audio_lossless must be allow or deny, keeping allow")));
        }
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        QString err;
        const DaemonConfig c = DaemonConfig::fromFile(f.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(c.audioLosslessAllowed, expected);
        QVERIFY(c.validate(&err));
        QVERIFY(DaemonConfig::defaults().audioLosslessAllowed);
    }

    void validateRefusesAnUnsupportedAudioBitrate()
    {
        DaemonConfig c = DaemonConfig::defaults();
        QCOMPARE(c.audioBitrate, 48000);
        QString err;
        QVERIFY(c.validate(&err));
        c.audioBitrate = 24000;
        QVERIFY(c.validate(&err));
        c.audioBitrate = 96000;
        QVERIFY(!c.validate(&err));
        QVERIFY(err.contains(QStringLiteral("audio_bitrate")));
    }

    void absentDisplayBudgetPairPreservesLegacyMode()
    {
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write("radio_mac = aa:bb:cc:dd:ee:ff\n");
        f.flush();

        QString err;
        const DaemonConfig c = DaemonConfig::fromFile(f.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QVERIFY(!c.displayApplicationBytesPerSecond.has_value());
        QVERIFY(!c.spectrumSampleUnitsPerSecond.has_value());
        QVERIFY(!c.displayBudgetLimits().has_value());
        QVERIFY(c.validate(&err));
    }

    void rejectsInvalidDisplayBudgetPair_data()
    {
        QTest::addColumn<QByteArray>("contents");

        QTest::newRow("application-only")
            << QByteArray("display_application_bytes_per_second = 1\n");
        QTest::newRow("samples-only")
            << QByteArray("spectrum_sample_units_per_second = 1\n");
        QTest::newRow("zero-application")
            << QByteArray("display_application_bytes_per_second = 0\n"
                          "spectrum_sample_units_per_second = 1\n");
        QTest::newRow("zero-samples")
            << QByteArray("display_application_bytes_per_second = 1\n"
                          "spectrum_sample_units_per_second = 0\n");
        QTest::newRow("negative-application")
            << QByteArray("display_application_bytes_per_second = -1\n"
                          "spectrum_sample_units_per_second = 1\n");
        QTest::newRow("negative-samples")
            << QByteArray("display_application_bytes_per_second = 1\n"
                          "spectrum_sample_units_per_second = -1\n");
        QTest::newRow("fractional-application")
            << QByteArray("display_application_bytes_per_second = 1.5\n"
                          "spectrum_sample_units_per_second = 1\n");
        QTest::newRow("fractional-samples")
            << QByteArray("display_application_bytes_per_second = 1\n"
                          "spectrum_sample_units_per_second = 1.5\n");
        QTest::newRow("malformed-application")
            << QByteArray("display_application_bytes_per_second = many\n"
                          "spectrum_sample_units_per_second = 1\n");
        QTest::newRow("malformed-samples")
            << QByteArray("display_application_bytes_per_second = 1\n"
                          "spectrum_sample_units_per_second = many\n");
        QTest::newRow("above-json-safe-application")
            << QByteArray("display_application_bytes_per_second = 9007199254740992\n"
                          "spectrum_sample_units_per_second = 1\n");
        QTest::newRow("above-json-safe-samples")
            << QByteArray("display_application_bytes_per_second = 1\n"
                          "spectrum_sample_units_per_second = 9007199254740992\n");
        QTest::newRow("quint64-overflow")
            << QByteArray("display_application_bytes_per_second = 18446744073709551616\n"
                          "spectrum_sample_units_per_second = 1\n");
    }

    void rejectsInvalidDisplayBudgetPair()
    {
        QFETCH(QByteArray, contents);
        QTemporaryFile f;
        QVERIFY(f.open());
        QCOMPARE(f.write(contents), qint64(contents.size()));
        f.flush();

        QString parseError;
        const DaemonConfig c = DaemonConfig::fromFile(f.fileName(), &parseError);
        QVERIFY2(parseError.isEmpty(), qPrintable(parseError));
        QString validationError;
        QVERIFY(!c.validate(&validationError));
        QVERIFY2(validationError.contains(QStringLiteral("display_application_bytes_per_second")),
                 qPrintable(validationError));
        QVERIFY(!c.displayBudgetLimits().has_value());
    }

    void acceptsJsonSafeDisplayBudgetMaximum()
    {
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write("display_application_bytes_per_second = 9007199254740991\n"
                "spectrum_sample_units_per_second = 9007199254740991\n");
        f.flush();

        QString err;
        const DaemonConfig c = DaemonConfig::fromFile(f.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QVERIFY(c.validate(&err));
        const std::optional<DisplayBudgetLimits> limits = c.displayBudgetLimits();
        QVERIFY(limits.has_value());
        QCOMPARE(limits->applicationBytesPerSecond, quint64(9007199254740991ULL));
        QCOMPARE(limits->spectrumSampleUnitsPerSecond, quint64(9007199254740991ULL));
    }

    // Remote Daemon R2 Task 18: the listener is OPT IN. A default-
    // constructed config must not bind anything, and must bind loopback
    // when it does. Pinned because flipping either default silently turns
    // every existing nereusd install into a network service.
    // iPhone app Task 12 (R-IOS-08): with no remote_port line the Core
    // listens on 47910 on every interface, IPv4 and IPv6 (remote_bind empty;
    // DaemonApp binds QHostAddress::Any), and announces.
    void listenerIsOnEveryInterfaceByDefault()
    {
        const DaemonConfig d = DaemonConfig::defaults();
        QCOMPARE(d.remotePort, 47910);
        QVERIFY(d.remoteBind.isEmpty());
        QVERIFY(d.pairingLanClickAllowed);
        QString err;
        QVERIFY(d.validate(&err));

        QTemporaryFile f;
        QVERIFY(f.open());
        f.write("slice_count = 2\n");
        f.flush();
        const DaemonConfig noListenerLines = DaemonConfig::fromFile(f.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(noListenerLines.remotePort, 47910);
        QVERIFY(noListenerLines.remoteBind.isEmpty());

        // The shipped sample leaves both keys commented out, so a Core
        // installed from it listens by default too.
        const DaemonConfig sample = DaemonConfig::fromFile(
            QStringLiteral(NEREUS_SOURCE_DIR "/packaging/nereusd.conf.sample"), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(sample.remotePort, 47910);
        QVERIFY(sample.remoteBind.isEmpty());
        QVERIFY(sample.pairingLanClickAllowed);
        QVERIFY(sample.validate(&err));
    }

    // A Core whose file sets the port or the bind keeps what the file meant
    // before the default changed: the Rock, with remote_port set and no
    // remote_bind, still listens on its port on this machine only; a file
    // with only remote_bind stays off; remote_port = 0 is off.
    void aFileThatSetsTheListenerKeepsItsMeaning_data()
    {
        QTest::addColumn<QByteArray>("contents");
        QTest::addColumn<int>("port");
        QTest::addColumn<QString>("bind");
        QTest::newRow("port only (the Rock)")
            << QByteArray("remote_port = 50055\n") << 50055 << QStringLiteral("127.0.0.1");
        QTest::newRow("port and bind")
            << QByteArray("remote_port = 50055\nremote_bind = 0.0.0.0\n") << 50055
            << QStringLiteral("0.0.0.0");
        QTest::newRow("bind only") << QByteArray("remote_bind = 0.0.0.0\n") << 0
                                   << QStringLiteral("0.0.0.0");
        QTest::newRow("off") << QByteArray("remote_port = 0\n") << 0 << QStringLiteral("127.0.0.1");
        QTest::newRow("unparseable port still counts as set")
            << QByteArray("remote_port = many\n") << 0 << QStringLiteral("127.0.0.1");
    }

    void aFileThatSetsTheListenerKeepsItsMeaning()
    {
        QFETCH(QByteArray, contents);
        QFETCH(int, port);
        QFETCH(QString, bind);
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(contents);
        f.flush();
        QString err;
        if (contents.contains("many")) {
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("remote_port")));
        }
        const DaemonConfig c = DaemonConfig::fromFile(f.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(c.remotePort, port);
        QCOMPARE(c.remoteBind, bind);
    }

    void remoteBindMustBeEmptyOrAnAddress()
    {
        DaemonConfig c = DaemonConfig::defaults();
        QString err;
        c.remoteBind = QStringLiteral("not-an-address");
        QVERIFY(!c.validate(&err));
        QVERIFY(err.contains(QStringLiteral("remote_bind")));
        c.remoteBind = QStringLiteral("::1");
        QVERIFY(c.validate(&err));
    }

    void pairingLanClickIsAllowOrDeny_data()
    {
        QTest::addColumn<QByteArray>("contents");
        QTest::addColumn<bool>("allowed");
        QTest::addColumn<bool>("warns");
        QTest::newRow("absent") << QByteArray("slice_count = 1\n") << true << false;
        QTest::newRow("allow") << QByteArray("pairing_lan_click = allow\n") << true << false;
        QTest::newRow("deny") << QByteArray("pairing_lan_click = deny\n") << false << false;
        QTest::newRow("Deny") << QByteArray("pairing_lan_click = Deny\n") << false << false;
        QTest::newRow("garbage") << QByteArray("pairing_lan_click = sometimes\n") << true << true;
    }

    void pairingLanClickIsAllowOrDeny()
    {
        QFETCH(QByteArray, contents);
        QFETCH(bool, allowed);
        QFETCH(bool, warns);
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(contents);
        f.flush();
        if (warns) {
            QTest::ignoreMessage(QtWarningMsg,
                                 QRegularExpression(QStringLiteral("pairing_lan_click must be")));
        }
        QString err;
        const DaemonConfig c = DaemonConfig::fromFile(f.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(c.pairingLanClickAllowed, allowed);
    }

    // iPhone app plan Task 34 (R-IOS-02): remote_transmit is allow by
    // default (the shipped sample says so too); deny keeps the Core
    // receive-only; anything else warns and denies.
    void remoteTransmitIsAllowOrDeny_data()
    {
        QTest::addColumn<QByteArray>("contents");
        QTest::addColumn<bool>("allowed");
        QTest::addColumn<bool>("warns");
        QTest::newRow("absent") << QByteArray("slice_count = 1\n") << true << false;
        QTest::newRow("allow") << QByteArray("remote_transmit = allow\n") << true << false;
        QTest::newRow("deny") << QByteArray("remote_transmit = deny\n") << false << false;
        QTest::newRow("Allow") << QByteArray("remote_transmit = Allow\n") << true << false;
        QTest::newRow("garbage") << QByteArray("remote_transmit = yes\n") << false << true;
    }

    void remoteTransmitIsAllowOrDeny()
    {
        QFETCH(QByteArray, contents);
        QFETCH(bool, allowed);
        QFETCH(bool, warns);
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(contents);
        f.flush();
        if (warns) {
            QTest::ignoreMessage(QtWarningMsg,
                                 QRegularExpression(QStringLiteral("remote_transmit must be")));
        }
        QString err;
        const DaemonConfig c = DaemonConfig::fromFile(f.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(c.remoteTransmitAllowed, allowed);
        QVERIFY(DaemonConfig::defaults().remoteTransmitAllowed);
        const DaemonConfig sample = DaemonConfig::fromFile(
            QStringLiteral(NEREUS_SOURCE_DIR "/packaging/nereusd.conf.sample"), &err);
        QVERIFY(sample.remoteTransmitAllowed);
    }

    // iPhone app Task 17 (R-IOS-08): the status page is on by default, on
    // TCP 47911; status_page takes on or off, anything else warns once and
    // keeps on; a status_port that is not a number warns and keeps 47911,
    // and validate() refuses one outside 1-65535.
    void statusPageIsOnOrOff_data()
    {
        QTest::addColumn<QByteArray>("contents");
        QTest::addColumn<bool>("on");
        QTest::addColumn<bool>("warns");
        QTest::newRow("absent") << QByteArray("slice_count = 1\n") << true << false;
        QTest::newRow("on") << QByteArray("status_page = on\n") << true << false;
        QTest::newRow("off") << QByteArray("status_page = off\n") << false << false;
        QTest::newRow("Off") << QByteArray("status_page = Off\n") << false << false;
        QTest::newRow("garbage") << QByteArray("status_page = maybe\n") << true << true;
    }

    void statusPageIsOnOrOff()
    {
        QFETCH(QByteArray, contents);
        QFETCH(bool, on);
        QFETCH(bool, warns);
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(contents);
        f.flush();
        if (warns) {
            QTest::ignoreMessage(QtWarningMsg,
                                 QRegularExpression(QStringLiteral("status_page must be")));
        }
        QString err;
        const DaemonConfig c = DaemonConfig::fromFile(f.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(c.statusPage, on);
        QCOMPARE(c.statusPort, 47911);
        QVERIFY(c.validate(&err));
    }

    void statusPortDefaultsAndRange()
    {
        const DaemonConfig d = DaemonConfig::defaults();
        QVERIFY(d.statusPage);
        QCOMPARE(d.statusPort, 47911);
        QVERIFY(d.stateDirectory.isEmpty());

        QTemporaryFile f;
        QVERIFY(f.open());
        f.write("status_port = eighty\n");
        f.flush();
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("status_port is not a number")));
        QString err;
        const DaemonConfig c = DaemonConfig::fromFile(f.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(c.statusPort, 47911);

        DaemonConfig bad = DaemonConfig::defaults();
        for (int port : {0, -1, 65536}) {
            bad.statusPort = port;
            QVERIFY(!bad.validate(&err));
            QVERIFY(err.contains(QStringLiteral("status_port")));
        }
        bad.statusPort = 65535;
        QVERIFY(bad.validate(&err));
    }

    // state_directory: empty keeps the profile's directory; set, it must be
    // an absolute path. The shipped sample names the packaged Core's
    // StateDirectory, so `sudo nereusd status` finds the control socket
    // through the default --config.
    void stateDirectoryIsEmptyOrAbsolute()
    {
        DaemonConfig c = DaemonConfig::defaults();
        QString err;
        c.stateDirectory = QStringLiteral("relative/dir");
        QVERIFY(!c.validate(&err));
        QVERIFY(err.contains(QStringLiteral("state_directory")));
        c.stateDirectory = QStringLiteral("/var/lib/nereusd");
        QVERIFY(c.validate(&err));

        const DaemonConfig sample = DaemonConfig::fromFile(
            QStringLiteral(NEREUS_SOURCE_DIR "/packaging/nereusd.conf.sample"), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(sample.stateDirectory, QStringLiteral("/var/lib/nereusd"));
        QVERIFY(sample.statusPage);
        QCOMPARE(sample.statusPort, 47911);
    }

    void rejectsRemotePortOutOfRange()
    {
        DaemonConfig c = DaemonConfig::defaults();
        QString err;

        // 0 is the documented "disabled" value, not an error.
        c.remotePort = 0;
        QVERIFY(c.validate(&err));

        c.remotePort = 65536;
        QVERIFY(!c.validate(&err));
        QVERIFY(!err.isEmpty());

        c.remotePort = -1;
        QVERIFY(!c.validate(&err));
        QVERIFY(!err.isEmpty());
    }

    // R-R3-22 / R-R3-47: one station network key for every station
    // listener. The Task 3 name station_tci_bind keeps working: read when
    // station_bind is empty or absent, never over it.
    void stationBindReadsTheOlderTciName_data()
    {
        QTest::addColumn<QByteArray>("text");
        QTest::addColumn<QString>("expected");
        QTest::newRow("absent") << QByteArray("slice_count = 1\n") << QString();
        QTest::newRow("new name") << QByteArray("station_bind = 10.0.0.7\n")
                                  << QStringLiteral("10.0.0.7");
        QTest::newRow("older name") << QByteArray("station_tci_bind = 192.168.1.20\n")
                                    << QStringLiteral("192.168.1.20");
        QTest::newRow("both, new first")
            << QByteArray("station_bind = 10.0.0.7\nstation_tci_bind = 192.168.1.20\n")
            << QStringLiteral("10.0.0.7");
        QTest::newRow("both, older first")
            << QByteArray("station_tci_bind = 192.168.1.20\nstation_bind = 10.0.0.7\n")
            << QStringLiteral("10.0.0.7");
        QTest::newRow("new name empty")
            << QByteArray("station_bind =\nstation_tci_bind = 192.168.1.20\n")
            << QStringLiteral("192.168.1.20");
        QTest::newRow("every address") << QByteArray("station_bind = 0.0.0.0\n")
                                       << QStringLiteral("0.0.0.0");
    }

    void stationBindReadsTheOlderTciName()
    {
        QFETCH(QByteArray, text);
        QFETCH(QString, expected);
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(text);
        f.flush();
        QString err;
        const DaemonConfig c = DaemonConfig::fromFile(f.fileName(), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(c.stationBind, expected);
        QVERIFY(c.validate(&err));
    }

    void stationBindMustBeAnAddress()
    {
        DaemonConfig c = DaemonConfig::defaults();
        c.stationBind = QStringLiteral("station-lan");
        QString err;
        QVERIFY(!c.validate(&err));
        QVERIFY(err.contains(QStringLiteral("station_bind")));
        c.stationBind = QStringLiteral("192.168.1.20");
        QVERIFY(c.validate(&err));
    }

    // A Core installed from the shipped sample must leave a new radio's
    // starting rate to the board default and the operator. An active
    // sample_rate_hz line would seed every new radio at that rate
    // (DaemonApp::applyConfigToSettings; a saved rate still wins, R-R3-49),
    // so the sample only documents it.
    void shippedSampleLeavesTheSampleRateToTheOperator()
    {
        QString err;
        const DaemonConfig c = DaemonConfig::fromFile(
            QStringLiteral(NEREUS_SOURCE_DIR "/packaging/nereusd.conf.sample"), &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QVERIFY2(!c.sampleRateExplicit,
                 "packaging/nereusd.conf.sample sets sample_rate_hz, which seeds that "
                 "rate into every new radio");
        QVERIFY(c.validate(&err));
    }

    // "::" is every address of both families; Qt alone would make it
    // IPv6-only. Everything else binds exactly what it names.
    void listenAddressForMapsTheIpv6AnyAddressToDualStack()
    {
        const QHostAddress anyAddress = DaemonConfig::listenAddressFor(QStringLiteral("::"));
        QCOMPARE(anyAddress.protocol(), QAbstractSocket::AnyIPProtocol);
        QVERIFY(anyAddress == QHostAddress::Any);

        const QHostAddress ipv4Any = DaemonConfig::listenAddressFor(QStringLiteral("0.0.0.0"));
        QCOMPARE(ipv4Any.protocol(), QAbstractSocket::IPv4Protocol);
        QVERIFY(ipv4Any == QHostAddress::AnyIPv4);

        QCOMPARE(DaemonConfig::listenAddressFor(QStringLiteral("127.0.0.1")),
                 QHostAddress(QHostAddress::LocalHost));
        QCOMPARE(DaemonConfig::listenAddressFor(QStringLiteral("::1")),
                 QHostAddress(QHostAddress::LocalHostIPv6));
        QCOMPARE(DaemonConfig::listenAddressFor(QStringLiteral("2602:ff8f:0:2::47")),
                 QHostAddress(QStringLiteral("2602:ff8f:0:2::47")));
        QVERIFY(DaemonConfig::listenAddressFor(QStringLiteral("not-an-address")).isNull());
    }

    void rejectsSliceCountBelowOne()
    {
        DaemonConfig c = DaemonConfig::defaults();
        c.sliceCount = 0;
        QString err;
        QVERIFY(!c.validate(&err));
        QVERIFY(!err.isEmpty());
    }

    void missingFileYieldsDefaultsAndAnError()
    {
        QString err;
        DaemonConfig c = DaemonConfig::fromFile("/nonexistent/nereusd.conf", &err);
        QVERIFY(!err.isEmpty());
        QCOMPARE(c.sliceCount, DaemonConfig::defaults().sliceCount);
        // No file at all is no remote_port line either: the Core listens.
        QCOMPARE(c.remotePort, DaemonConfig::kDefaultRemotePort);
        QVERIFY(c.remoteBind.isEmpty());
    }

    // Remote Daemon R2, Task 1: absent (wasSet == false) now reserves
    // nereusd's own profile instead of sharing the GUI's -- this is the
    // exact contract inversion the pre-Task-1 emptyProfileArgumentMeans
    // NoProfile() slot pinned in the other direction.
    void absentProfileArgumentResolvesToReservedDaemonProfile()
    {
        QString err;
        const QString profile = resolveDaemonProfileArgument(QString(), false, &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(profile, QString(AppSettings::kDaemonProfileName));
    }

    // The escape hatch: an operator who explicitly types --profile ""
    // (wasSet == true, value still empty) opts back into sharing the
    // GUI's own settings/log directory -- exactly what every --profile
    // argument did before this task.
    void explicitlyEmptyProfileArgumentStillShares()
    {
        QString err;
        const QString profile = resolveDaemonProfileArgument(QString(), true, &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QVERIFY(profile.isEmpty());
    }

    void validProfileNameIsAccepted()
    {
        QString err;
        const QString profile =
            resolveDaemonProfileArgument(QStringLiteral("hf"), true, &err);
        QVERIFY2(err.isEmpty(), qPrintable(err));
        QCOMPARE(profile, QStringLiteral("hf"));
    }

    void invalidProfileNameIsRejected()
    {
        QString err;
        const QString profile =
            resolveDaemonProfileArgument(QStringLiteral("with space"), true, &err);
        QVERIFY(!err.isEmpty());
        QVERIFY(profile.isEmpty());
    }
};

QTEST_MAIN(TstDaemonConfig)
#include "tst_daemon_config.moc"
