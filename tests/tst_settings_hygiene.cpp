// =================================================================
// tests/tst_settings_hygiene.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test. No Thetis port at this layer.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-20 — Original test for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/SettingsHygiene.h"
#include "core/BoardCapabilities.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

// Helper: fresh AppSettings sandbox (TestSandboxInit ensures test-mode path).
static AppSettings& testSettings()
{
    return AppSettings::instance();
}

// Helper: build a Hermes-like BoardCapabilities with attenuator.maxDb=31.
static BoardCapabilities makeHermesCaps()
{
    return BoardCapsTable::forBoard(HPSDRHW::Hermes);
}

// Helper: build an HL2 BoardCapabilities with attenuator.maxDb=63 + hasIoBoardHl2.
static BoardCapabilities makeHl2Caps()
{
    return BoardCapsTable::forBoard(HPSDRHW::HermesLite);
}

static const QString kTestMac = QStringLiteral("00:11:22:33:44:55");

class TestSettingsHygiene : public QObject {
    Q_OBJECT

private slots:

    void init()
    {
        // Clear all test-mode settings before each test.
        testSettings().clear();
    }

    // ── Empty settings → no issues ────────────────────────────────────────

    void emptySettings_noIssues()
    {
        SettingsHygiene h;
        h.validate(kTestMac, makeHermesCaps());
        QVERIFY(!h.hasIssues());
        QCOMPARE(h.issueCount(), 0);
    }

    // ── S-ATT clamp checks ────────────────────────────────────────────────

    void att45_onHl2_caps63_noIssue()
    {
        // HL2 maxDb=63 → persisted 45 dB is within range → no Warning.
        testSettings().setValue(
            QStringLiteral("hardware/%1/sAtt").arg(kTestMac), 45);

        SettingsHygiene h;
        h.validate(kTestMac, makeHl2Caps());

        // Filter for S-ATT warnings only.
        int attWarnings = 0;
        for (const auto& issue : h.issues()) {
            if (issue.key.contains(QStringLiteral("sAtt"))) { ++attWarnings; }
        }
        QCOMPARE(attWarnings, 0);
    }

    void att45_onHermes_caps31_oneWarning()
    {
        // Hermes maxDb=31 → persisted 45 dB exceeds range → 1 Warning.
        testSettings().setValue(
            QStringLiteral("hardware/%1/sAtt").arg(kTestMac), 45);

        SettingsHygiene h;
        h.validate(kTestMac, makeHermesCaps());

        int attWarnings = 0;
        for (const auto& issue : h.issues()) {
            if (issue.key.contains(QStringLiteral("sAtt"))) { ++attWarnings; }
        }
        QCOMPARE(attWarnings, 1);

        // Severity must be Warning.
        for (const auto& issue : h.issues()) {
            if (issue.key.contains(QStringLiteral("sAtt"))) {
                QCOMPARE(issue.severity, SettingsHygiene::Severity::Warning);
            }
        }
    }

    void att31_onHermes_caps31_noIssue()
    {
        // Exactly at the limit → no issue.
        testSettings().setValue(
            QStringLiteral("hardware/%1/sAtt").arg(kTestMac), 31);

        SettingsHygiene h;
        h.validate(kTestMac, makeHermesCaps());

        int attWarnings = 0;
        for (const auto& issue : h.issues()) {
            if (issue.key.contains(QStringLiteral("sAtt"))) { ++attWarnings; }
        }
        QCOMPARE(attWarnings, 0);
    }

    // ── Reset to defaults clears persisted ATT ────────────────────────────

    void resetSettingsToDefaults_clampsAtt()
    {
        // Persist an out-of-range ATT value.
        testSettings().setValue(
            QStringLiteral("hardware/%1/sAtt").arg(kTestMac), 45);

        SettingsHygiene h;
        BoardCapabilities caps = makeHermesCaps();
        h.validate(kTestMac, caps);
        QVERIFY(h.hasIssues());

        // Reset should clamp to maxDb.
        h.resetSettingsToDefaults(kTestMac, caps);

        // After reset, the persisted value should be clamped.
        int stored = testSettings().value(
            QStringLiteral("hardware/%1/sAtt").arg(kTestMac), -1).toInt();
        QCOMPARE(stored, caps.attenuator.maxDb);

        // Validate again → no more ATT issue.
        int attWarnings = 0;
        for (const auto& issue : h.issues()) {
            if (issue.key.contains(QStringLiteral("sAtt"))) { ++attWarnings; }
        }
        QCOMPARE(attWarnings, 0);
    }

    // ── issuesChanged signal ──────────────────────────────────────────────

    void validate_emitsIssuesChanged()
    {
        SettingsHygiene h;
        QSignalSpy spy(&h, &SettingsHygiene::issuesChanged);
        h.validate(kTestMac, makeHermesCaps());
        QCOMPARE(spy.count(), 1);
    }

    void resetToDefaults_emitsIssuesChanged()
    {
        SettingsHygiene h;
        QSignalSpy spy(&h, &SettingsHygiene::issuesChanged);
        h.resetSettingsToDefaults(kTestMac, makeHermesCaps());
        // validate() called internally by resetSettingsToDefaults → issuesChanged
        QVERIFY(spy.count() >= 1);
    }

    // ── forgetRadio clears all hardware/<mac> settings ────────────────────

    void forgetRadio_removesAllMacKeys()
    {
        testSettings().setValue(
            QStringLiteral("hardware/%1/sAtt").arg(kTestMac), 45);
        testSettings().setValue(
            QStringLiteral("hardware/%1/radioInfo/sampleRate").arg(kTestMac),
            QStringLiteral("192000"));

        SettingsHygiene h;
        h.forgetRadio(kTestMac);

        QVERIFY(!testSettings().contains(
            QStringLiteral("hardware/%1/sAtt").arg(kTestMac)));
        QVERIFY(!testSettings().contains(
            QStringLiteral("hardware/%1/radioInfo/sampleRate").arg(kTestMac)));
    }

    // ── Apollo check ──────────────────────────────────────────────────────

    void apolloEnabled_onNonApolloBoard_oneWarning()
    {
        // HL2 does not have Apollo (hasApollo = false for HL2).
        BoardCapabilities caps = makeHl2Caps();

        // Persist Apollo enabled.
        testSettings().setValue(
            QStringLiteral("hardware/%1/apollo/enabled").arg(kTestMac),
            QStringLiteral("True"));

        SettingsHygiene h;
        h.validate(kTestMac, caps);

        int apolloWarnings = 0;
        for (const auto& issue : h.issues()) {
            if (issue.key.contains(QStringLiteral("apollo"))) { ++apolloWarnings; }
        }
        QCOMPARE(apolloWarnings, 1);
    }
};

QTEST_GUILESS_MAIN(TestSettingsHygiene)
#include "tst_settings_hygiene.moc"
