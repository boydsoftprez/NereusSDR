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

// Helper: build an HL2 BoardCapabilities with signed −28..+31 dB attenuator
// range (issue #175 follow-up — capped at +31 per maintainer approval to
// avoid mi0bot's off-by-one wire wraparound at userDb=32).
static BoardCapabilities makeHl2Caps()
{
    return BoardCapsTable::forBoard(HPSDRHW::HermesLite);
}

// Helper: a BPF1-algorithm board (ANAN-G2 / Saturn), for the keep-the-data side
// of the Saturn BPF1 rules.
static BoardCapabilities makeSaturnCaps()
{
    return BoardCapsTable::forBoard(HPSDRHW::Saturn);
}

static const QString kTestMac = QStringLiteral("00:11:22:33:44:55");

// ── Saturn BPF1 on-disk key contract ──────────────────────────────────────
//
// The six per-row slugs AntennaAlexAlex1Tab persists BPF1 band edges under.
// Spelled out as literals on purpose: this is the on-disk contract with every
// settings file already in the field, so a hygiene pass that does not use
// these exact strings cannot see a user's stored data.  Keeping the literals
// here (rather than importing the shared slug list) means renaming the shared
// constant cannot quietly make these tests vacuous.
static const QStringList kBpf1SlugsOnDisk = {
    QStringLiteral("1_5MHz"), QStringLiteral("6_5MHz"), QStringLiteral("9_5MHz"),
    QStringLiteral("13MHz"),  QStringLiteral("20MHz"),  QStringLiteral("6mBP"),
};

// Writes exactly what AntennaAlexAlex1Tab::onBpf1CheckChanged /
// onBpf1SpinChanged write: setHardwareValue(mac, "alex/bpf1/<slug>/<leaf>", v),
// which lands as hardware/<mac>/alex/bpf1/<slug>/<leaf>.
static void writeBpf1EdgesLikeSetupTab(const QString& mac)
{
    auto& s = testSettings();
    for (const QString& slug : kBpf1SlugsOnDisk) {
        s.setHardwareValue(mac, QStringLiteral("alex/bpf1/%1/enabled").arg(slug),
                           QStringLiteral("True"));
        s.setHardwareValue(mac, QStringLiteral("alex/bpf1/%1/start").arg(slug), 1.5);
        s.setHardwareValue(mac, QStringLiteral("alex/bpf1/%1/end").arg(slug), 2.099999);
    }
}

// Every leaf AntennaAlexAlex1Tab writes under a BPF1 row.
static const QStringList kBpf1LeavesOnDisk = {
    QStringLiteral("enabled"), QStringLiteral("start"), QStringLiteral("end"),
};

// Counts how many of the keys writeBpf1EdgesLikeSetupTab() wrote are still present.
static int storedBpf1KeyCount(const QString& mac)
{
    auto& s = testSettings();
    int count = 0;
    for (const QString& slug : kBpf1SlugsOnDisk) {
        for (const QString& leaf : kBpf1LeavesOnDisk) {
            const QString key = QStringLiteral("hardware/%1/alex/bpf1/").arg(mac)
                                + slug + QLatin1Char('/') + leaf;
            if (s.contains(key)) { ++count; }
        }
    }
    return count;
}

// Total key count writeBpf1EdgesLikeSetupTab() lays down.
static int expectedBpf1KeyCount()
{
    return static_cast<int>(kBpf1SlugsOnDisk.size() * kBpf1LeavesOnDisk.size());
}

static int bpf1IssueCount(const SettingsHygiene& h)
{
    int count = 0;
    for (const auto& issue : h.issues()) {
        if (issue.key.contains(QStringLiteral("alex/bpf1"))) { ++count; }
    }
    return count;
}

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

    void att30_onHl2_signedRange_noIssue()
    {
        // HL2 user-facing range: signed −28..+31 dB (issue #175 follow-up,
        // capped at +31 per maintainer approval).  Persisted 30 dB is within
        // range → no Warning.
        testSettings().setValue(
            QStringLiteral("hardware/%1/sAtt").arg(kTestMac), 30);

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

    // ── Saturn BPF1 key-naming regression ─────────────────────────────────
    //
    // SettingsHygiene used to probe and remove hardware/<mac>/alex/bpf1/160m/…
    // (11 ham-band names) while AntennaAlexAlex1Tab persists BPF1 edges under
    // hardware/<mac>/alex/bpf1/1_5MHz/… (6 crossover slugs).  The two name
    // sets never overlapped, so both the detector and the cleanup were silent
    // no-ops: a 7000DLE owner who downgraded to a Hermes kept stale BPF1 data
    // forever and was never told.

    void bpf1EdgesFromSetupTab_onNonBpf1Board_areReported()
    {
        writeBpf1EdgesLikeSetupTab(kTestMac);

        SettingsHygiene h;
        h.validate(kTestMac, makeHermesCaps());

        QCOMPARE(bpf1IssueCount(h), 1);
    }

    void resetSettingsToDefaults_removesBpf1EdgesFromSetupTab()
    {
        writeBpf1EdgesLikeSetupTab(kTestMac);
        QCOMPARE(storedBpf1KeyCount(kTestMac), expectedBpf1KeyCount());

        SettingsHygiene h;
        h.resetSettingsToDefaults(kTestMac, makeHermesCaps());

        // Every slug, and every leaf under it, including "enabled", which the
        // old loop never removed even for the band names it did know about.
        QCOMPARE(storedBpf1KeyCount(kTestMac), 0);

        // And the detector agrees the data is gone.
        QCOMPARE(bpf1IssueCount(h), 0);
    }

    void bpf1EdgesFromSetupTab_onSaturnBoard_areKept()
    {
        writeBpf1EdgesLikeSetupTab(kTestMac);

        SettingsHygiene h;
        BoardCapabilities caps = makeSaturnCaps();
        h.validate(kTestMac, caps);
        QCOMPARE(bpf1IssueCount(h), 0);

        // Reset must not throw away band edges the board actually uses.
        h.resetSettingsToDefaults(kTestMac, caps);
        QCOMPARE(storedBpf1KeyCount(kTestMac), expectedBpf1KeyCount());
    }
};

QTEST_GUILESS_MAIN(TestSettingsHygiene)
#include "tst_settings_hygiene.moc"
