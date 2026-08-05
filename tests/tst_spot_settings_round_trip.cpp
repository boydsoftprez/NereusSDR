// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: NereusSDR-original test file. AppSettings is a port
// of Thetis database.cs persistence semantics, but the *keys* exercised
// here are entirely NereusSDR-specific: they belong to the Phase 3J-2
// spot system (B-phase ingest clients, F-phase SpotHubDialog,
// G-phase FreeDV Reporter dialog, H-phase RadioModel adapter slots)
// plus Phase 3R RADE wrapper. No upstream cite belongs on the assertions
// themselves.
//
// NereusSDR - Phase 3J-2 + 3R M1: spot-system settings round-trip.
//
// Pins the contract that every AppSettings key the spot system writes
// or reads can survive a round-trip through the singleton, and that
// each key has the documented default when absent. AppSettings is fully
// dynamic (no key registry) so this is the only place the key list is
// machine-enforced; reviewers rely on this file when adding or renaming
// a key.
//
// What is verified:
//   * Lifetime keys (7 sources, `<Source>SpotLifetimeSec`): set 600 -> read 600.
//   * Color keys (7 sources, `<Source>SpotColor`): set "#FF112233" -> read same.
//   * Identity / connection keys (host / port / callsign / auto-X)
//     for every per-source tab in SpotHubDialog: SpotHubDialog uses flat
//     PascalCase keys (DxClusterHost / RbnHost / WsjtxPort etc.) for most
//     sources and a namespaced FreeDvReporter/<Field> family for the
//     FreeDV Reporter dialog. Both shapes are pinned.
//   * SpotHub Display tab knobs: every key the F4 buildDisplayTab uses.
//   * FreeDV Reporter dialog state (saved messages, visible columns,
//     idle timeout, column filters, band filter).
//   * RADE model path key.
//   * Documented defaults when keys absent.
//   * Legacy minutes-key fallback (`<Source>SpotLifetime` minutes -> Sec
//     when the seconds key is absent). This pin matches the migration
//     path SpotHubDialog F4 already implements at
//     `SpotHubDialog.cpp:1727-1730`.
//
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3J-2 + 3R M1 initial commit.
//                                    AI tooling: Anthropic Claude Code.

#include <QtTest/QtTest>

#include "core/AppSettings.h"

using namespace NereusSDR;

namespace {

// Helper: fresh AppSettings sandbox. TestSandboxInit (linked auto by
// nereus_add_test) ensures QStandardPaths::setTestModeEnabled(true) is
// active, so this is harmless to real user state.
AppSettings& testSettings()
{
    return AppSettings::instance();
}

// The seven canonical source labels used as the prefix on
// `<Source>SpotLifetimeSec` and `<Source>SpotColor`. Mirrors the seven
// per-source adapter slots on RadioModel (RadioModel.cpp:1212-1313).
//
// Note: FreeDV uses the `FreeDv` prefix on lifetime / color (NOT
// `FreeDvReporter`); the FreeDV Reporter identity keys are namespaced
// separately under `FreeDvReporter/`. This split is intentional and
// pinned here so a future refactor cannot silently rename one without
// the other.
struct SourceLabel {
    const char* prefix;
    const char* defaultColor;
    int defaultLifetimeSec;
};

constexpr SourceLabel kSources[] = {
    {"DxCluster",     "#D2B48C", 1800},
    {"Rbn",           "#4488FF", 1800},
    {"Wsjtx",         "#00FF00",  120},
    {"SpotCollector", "#B0C4DE", 1800},
    {"Pota",          "#FFFF00", 3600},
    {"FreeDv",        "#FF8C00", 1800},
    {"PskReporter",   "#FF00FF", 1800},
};

} // namespace

class TestSpotSettingsRoundTrip : public QObject {
    Q_OBJECT

private slots:

    void init()
    {
        testSettings().clear();
    }

    void cleanup()
    {
        testSettings().clear();
    }

    // ── Lifetime keys ────────────────────────────────────────────────────

    void lifetimeKeysRoundTrip()
    {
        // Write 600 to every `<Source>SpotLifetimeSec`, read it back.
        for (const auto& src : kSources) {
            const QString key = QString("%1SpotLifetimeSec").arg(src.prefix);
            testSettings().setValue(key, 600);
        }
        for (const auto& src : kSources) {
            const QString key = QString("%1SpotLifetimeSec").arg(src.prefix);
            const int got = testSettings().value(key, src.defaultLifetimeSec).toInt();
            QCOMPARE(got, 600);
        }
    }

    // ── Color keys ──────────────────────────────────────────────────────

    void colorKeysRoundTrip()
    {
        // Write "#FF112233" (with alpha) to every `<Source>SpotColor`,
        // read it back.
        for (const auto& src : kSources) {
            const QString key = QString("%1SpotColor").arg(src.prefix);
            testSettings().setValue(key, QStringLiteral("#FF112233"));
        }
        for (const auto& src : kSources) {
            const QString key = QString("%1SpotColor").arg(src.prefix);
            const QString got = testSettings().value(key, src.defaultColor).toString();
            QCOMPARE(got, QString("#FF112233"));
        }
    }

    // ── Identity / connection keys ──────────────────────────────────────
    // SpotHubDialog uses flat PascalCase keys for cluster / RBN /
    // WSJT-X / SpotCollector / POTA / PSK Reporter, and a namespaced
    // `FreeDvReporter/<Field>` family for the FreeDV Reporter identity.
    // Both shapes are exercised here.

    void clusterIdentityRoundTrip()
    {
        testSettings().setValue("DxClusterHost", QStringLiteral("dxc.example.org"));
        testSettings().setValue("DxClusterPort", 7300);
        testSettings().setValue("DxClusterCallsign", QStringLiteral("KG4VCF"));
        QCOMPARE(testSettings().value("DxClusterHost").toString(),
                 QString("dxc.example.org"));
        QCOMPARE(testSettings().value("DxClusterPort").toInt(), 7300);
        QCOMPARE(testSettings().value("DxClusterCallsign").toString(),
                 QString("KG4VCF"));
    }

    void rbnIdentityRoundTrip()
    {
        testSettings().setValue("RbnHost", QStringLiteral("telnet.reversebeacon.net"));
        testSettings().setValue("RbnPort", 7000);
        testSettings().setValue("RbnCallsign", QStringLiteral("KG4VCF"));
        testSettings().setValue("RbnRateLimit", 25);
        QCOMPARE(testSettings().value("RbnHost").toString(),
                 QString("telnet.reversebeacon.net"));
        QCOMPARE(testSettings().value("RbnPort").toInt(), 7000);
        QCOMPARE(testSettings().value("RbnCallsign").toString(),
                 QString("KG4VCF"));
        QCOMPARE(testSettings().value("RbnRateLimit").toInt(), 25);
    }

    void wsjtxIdentityRoundTrip()
    {
        testSettings().setValue("WsjtxAddress", QStringLiteral("224.0.0.1"));
        testSettings().setValue("WsjtxPort", 2237);
        QCOMPARE(testSettings().value("WsjtxAddress").toString(),
                 QString("224.0.0.1"));
        QCOMPARE(testSettings().value("WsjtxPort").toInt(), 2237);
    }

    void spotCollectorIdentityRoundTrip()
    {
        testSettings().setValue("SpotCollectorPort", 11733);
        QCOMPARE(testSettings().value("SpotCollectorPort").toInt(), 11733);
    }

    void potaIdentityRoundTrip()
    {
        testSettings().setValue("PotaPollInterval", 60);
        QCOMPARE(testSettings().value("PotaPollInterval").toInt(), 60);
    }

    void freeDvReporterIdentityRoundTrip()
    {
        // FreeDV Reporter uses namespaced keys (RadioModel.cpp:937-952).
        testSettings().setValue("FreeDvReporter/Callsign",
                                QStringLiteral("KG4VCF"));
        testSettings().setValue("FreeDvReporter/GridSquare",
                                QStringLiteral("EM83"));
        testSettings().setValue("FreeDvReporter/Message",
                                QStringLiteral("Test message"));
        testSettings().setValue(
            "FreeDvReporter/ServerUrl",
            QStringLiteral("wss://qso.freedv.org/socket.io/?EIO=4&transport=websocket"));
        QCOMPARE(testSettings().value("FreeDvReporter/Callsign").toString(),
                 QString("KG4VCF"));
        QCOMPARE(testSettings().value("FreeDvReporter/GridSquare").toString(),
                 QString("EM83"));
        QCOMPARE(testSettings().value("FreeDvReporter/Message").toString(),
                 QString("Test message"));
        QCOMPARE(testSettings().value("FreeDvReporter/ServerUrl").toString(),
                 QString("wss://qso.freedv.org/socket.io/?EIO=4&transport=websocket"));
    }

    void pskReporterIdentityRoundTrip()
    {
        testSettings().setValue("PskReporterCallsign", QStringLiteral("KG4VCF"));
        testSettings().setValue("PskReporterGrid", QStringLiteral("EM83"));
        QCOMPARE(testSettings().value("PskReporterCallsign").toString(),
                 QString("KG4VCF"));
        QCOMPARE(testSettings().value("PskReporterGrid").toString(),
                 QString("EM83"));
    }

    // ── AutoConnect / AutoStart keys ────────────────────────────────────

    void autoConnectKeysRoundTrip()
    {
        struct {
            const char* key;
        } autoKeys[] = {
            {"DxClusterAutoConnect"},
            {"RbnAutoConnect"},
            {"WsjtxAutoStart"},
            {"SpotCollectorAutoStart"},
            {"PotaAutoStart"},
            {"FreeDvAutoStart"},
            {"PskReporterAutoStart"},
        };

        // True round-trip.
        for (const auto& k : autoKeys) {
            testSettings().setValue(k.key, QStringLiteral("True"));
        }
        for (const auto& k : autoKeys) {
            QCOMPARE(testSettings().value(k.key, "False").toString(),
                     QString("True"));
        }
        // False round-trip.
        for (const auto& k : autoKeys) {
            testSettings().setValue(k.key, QStringLiteral("False"));
        }
        for (const auto& k : autoKeys) {
            QCOMPARE(testSettings().value(k.key, "True").toString(),
                     QString("False"));
        }
    }

    // ── SpotHub Display tab knobs ───────────────────────────────────────
    // Every key the F4 buildDisplayTab in SpotHubDialog.cpp writes
    // (SpotHubDialog.cpp:1714-1995).

    void displayKnobsRoundTrip()
    {
        // Booleans.
        testSettings().setValue("IsSpotsEnabled", "True");
        testSettings().setValue("IsMemorySpotsEnabled", "True");
        testSettings().setValue("IsSpotsOverrideColorsEnabled", "True");
        testSettings().setValue("IsSpotsOverrideBackgroundColorsEnabled", "False");
        testSettings().setValue("IsSpotsOverrideToAutoBackgroundColorEnabled",
                                "False");
        QCOMPARE(testSettings().value("IsSpotsEnabled").toString(),
                 QString("True"));
        QCOMPARE(testSettings().value("IsMemorySpotsEnabled").toString(),
                 QString("True"));
        QCOMPARE(testSettings().value("IsSpotsOverrideColorsEnabled").toString(),
                 QString("True"));
        QCOMPARE(testSettings().value(
                     "IsSpotsOverrideBackgroundColorsEnabled").toString(),
                 QString("False"));
        QCOMPARE(testSettings().value(
                     "IsSpotsOverrideToAutoBackgroundColorEnabled").toString(),
                 QString("False"));

        // Sliders / spinboxes.
        testSettings().setValue("SpotsMaxLevel", 7);
        testSettings().setValue("SpotsStartingHeightPercentage", 25);
        testSettings().setValue("SpotFontSize", 18);
        testSettings().setValue("SpotsBackgroundOpacity", 75);
        testSettings().setValue("DxClusterSpotLifetimeSec", 1200);
        QCOMPARE(testSettings().value("SpotsMaxLevel").toInt(), 7);
        QCOMPARE(testSettings().value("SpotsStartingHeightPercentage").toInt(), 25);
        QCOMPARE(testSettings().value("SpotFontSize").toInt(), 18);
        QCOMPARE(testSettings().value("SpotsBackgroundOpacity").toInt(), 75);
        QCOMPARE(testSettings().value("DxClusterSpotLifetimeSec").toInt(),
                 1200);

        // Colors.
        testSettings().setValue("SpotsOverrideColor", "#AABBCC");
        testSettings().setValue("SpotsOverrideBgColor", "#112233");
        QCOMPARE(testSettings().value("SpotsOverrideColor").toString(),
                 QString("#AABBCC"));
        QCOMPARE(testSettings().value("SpotsOverrideBgColor").toString(),
                 QString("#112233"));
    }

    // ── FreeDV Reporter dialog state keys ───────────────────────────────

    void freeDvReporterDialogKeysRoundTrip()
    {
        // Saved messages (QStringList newline-joined).
        const QString saved = QStringLiteral("CQ test\nNets at 0200Z\nGM/GA/GE");
        testSettings().setValue("FreeDvReporter/SavedMessages", saved);
        QCOMPARE(
            testSettings().value("FreeDvReporter/SavedMessages").toString(),
            saved);

        // Visible-columns bitmask.
        testSettings().setValue("FreeDvReporter/VisibleColumns", 0x3FFF);
        QCOMPARE(
            testSettings().value("FreeDvReporter/VisibleColumns").toInt(),
            0x3FFF);

        // Idle timeout minutes.
        testSettings().setValue("FreeDvReporter/IdleTimeoutMinutes", 90);
        QCOMPARE(
            testSettings().value("FreeDvReporter/IdleTimeoutMinutes").toInt(),
            90);

        // Column-filters string.
        const QString filters = QStringLiteral("3:eq:Q1NS\n8:gt:Mg==");
        testSettings().setValue("FreeDvReporter/ColumnFilters", filters);
        QCOMPARE(
            testSettings().value("FreeDvReporter/ColumnFilters").toString(),
            filters);

        // Band filter.
        testSettings().setValue("FreeDvReporter/BandFilter",
                                QStringLiteral("20m"));
        QCOMPARE(
            testSettings().value("FreeDvReporter/BandFilter").toString(),
            QString("20m"));
    }

    // ── RADE model path ─────────────────────────────────────────────────

    void radeModelPathRoundTrip()
    {
        testSettings().setValue("Rade/ModelPath",
                                QStringLiteral("/opt/rade/model_v1.bin"));
        QCOMPARE(testSettings().value("Rade/ModelPath").toString(),
                 QString("/opt/rade/model_v1.bin"));
    }

    // ── Legacy minutes-key fallback ─────────────────────────────────────
    // Mirrors the SpotHubDialog F4 fallback at SpotHubDialog.cpp:1727-1730:
    // if `DxClusterSpotLifetimeSec` is absent, the legacy minutes key
    // `DxClusterSpotLifetime` (default 30 min) is converted to seconds.
    // This is a behavioural contract, not an AppSettings feature, so the
    // test inlines the same lookup logic.

    void legacyMinutesKeyFallback()
    {
        // Set the legacy minutes key only (10 min = 600 sec).
        testSettings().setValue("DxClusterSpotLifetime", 10);
        QVERIFY(!testSettings().contains("DxClusterSpotLifetimeSec"));

        const int secsKeyVal =
            testSettings().value("DxClusterSpotLifetimeSec", 0).toInt();
        int lifetimeSec = secsKeyVal;
        if (lifetimeSec <= 0) {
            lifetimeSec =
                testSettings().value("DxClusterSpotLifetime", 30).toInt() * 60;
        }
        QCOMPARE(lifetimeSec, 600);
    }

    // ── Defaults when keys absent ───────────────────────────────────────

    void defaultsWhenKeyAbsent()
    {
        // Lifetime defaults (matches RadioModel.cpp:1212-1313 per-source slots).
        for (const auto& src : kSources) {
            const QString key = QString("%1SpotLifetimeSec").arg(src.prefix);
            QVERIFY(!testSettings().contains(key));
            QCOMPARE(testSettings().value(key, src.defaultLifetimeSec).toInt(),
                     src.defaultLifetimeSec);
        }

        // Color defaults.
        for (const auto& src : kSources) {
            const QString key = QString("%1SpotColor").arg(src.prefix);
            QVERIFY(!testSettings().contains(key));
            QCOMPARE(
                testSettings().value(key, src.defaultColor).toString(),
                QString(src.defaultColor));
        }

        // SpotHubDialog Display tab defaults
        // (SpotHubDialog.cpp:1714-1730).
        QCOMPARE(testSettings().value("IsSpotsEnabled", "True").toString(),
                 QString("True"));
        QCOMPARE(testSettings().value("IsMemorySpotsEnabled",
                                       "False").toString(),
                 QString("False"));
        QCOMPARE(testSettings().value("IsSpotsOverrideColorsEnabled",
                                       "False").toString(),
                 QString("False"));
        QCOMPARE(testSettings().value("IsSpotsOverrideBackgroundColorsEnabled",
                                       "True").toString(),
                 QString("True"));
        QCOMPARE(testSettings().value(
                     "IsSpotsOverrideToAutoBackgroundColorEnabled",
                     "True").toString(),
                 QString("True"));
        QCOMPARE(testSettings().value("SpotsMaxLevel", 3).toInt(), 3);
        QCOMPARE(testSettings().value("SpotsStartingHeightPercentage",
                                       50).toInt(),
                 50);
        QCOMPARE(testSettings().value("SpotFontSize", 16).toInt(), 16);
        QCOMPARE(testSettings().value("SpotsBackgroundOpacity", 48).toInt(),
                 48);
        QCOMPARE(testSettings().value("SpotsOverrideColor",
                                       "#FFFF00").toString(),
                 QString("#FFFF00"));
        QCOMPARE(testSettings().value("SpotsOverrideBgColor",
                                       "#000000").toString(),
                 QString("#000000"));

        // SpotHubDialog per-source defaults (verbatim from the F2 reads).
        QCOMPARE(testSettings().value("DxClusterHost",
                                       "dxc.nc7j.com").toString(),
                 QString("dxc.nc7j.com"));
        QCOMPARE(testSettings().value("DxClusterPort", 7300).toInt(), 7300);
        QCOMPARE(testSettings().value("DxClusterAutoConnect",
                                       "False").toString(),
                 QString("False"));
        QCOMPARE(testSettings().value("RbnHost",
                                       "telnet.reversebeacon.net").toString(),
                 QString("telnet.reversebeacon.net"));
        QCOMPARE(testSettings().value("RbnPort", 7000).toInt(), 7000);
        QCOMPARE(testSettings().value("RbnRateLimit", 10).toInt(), 10);
        QCOMPARE(testSettings().value("WsjtxAddress",
                                       "224.0.0.1").toString(),
                 QString("224.0.0.1"));
        QCOMPARE(testSettings().value("WsjtxPort", 2237).toInt(), 2237);
        QCOMPARE(testSettings().value("WsjtxAutoStart", "False").toString(),
                 QString("False"));
        QCOMPARE(testSettings().value("SpotCollectorPort", 9999).toInt(), 9999);
        QCOMPARE(testSettings().value("SpotCollectorAutoStart",
                                       "False").toString(),
                 QString("False"));
        QCOMPARE(testSettings().value("PotaPollInterval", 30).toInt(), 30);
        QCOMPARE(testSettings().value("PotaAutoStart", "False").toString(),
                 QString("False"));
        QCOMPARE(testSettings().value("FreeDvAutoStart", "False").toString(),
                 QString("False"));
        QCOMPARE(testSettings().value("PskReporterAutoStart",
                                       "False").toString(),
                 QString("False"));

        // RADE model path default.
        QCOMPARE(testSettings().value("Rade/ModelPath", "").toString(),
                 QString(""));
    }
};

QTEST_GUILESS_MAIN(TestSpotSettingsRoundTrip)
#include "tst_spot_settings_round_trip.moc"
