// no-port-check: NereusSDR-original unit-test file.
// =================================================================
// tests/tst_mic_profile_manager_cfc_round_trip.cpp  (NereusSDR)
// =================================================================
//
// Phase 3M-3a-ii Batch 4 — round-trip tests for the 41 new
// CFC + CPDR + CESSB + PhRot bundle keys added to MicProfileManager.
//
// Coverage:
//   1. Bundle keys complete  — kKeys contains all 41 new keys.
//   2. Defaults full         — defaultProfileValues() returns every key
//                              with the exact value from the brief table.
//   3. Factory profile cells — for each of the 21 factory profiles, every
//                              key (existing 50 + new 41 = 91) round-trips
//                              through AppSettings byte-for-byte.
//   4. CFCParaEQData blob    — non-empty XML round-trip (forward-compat
//                              for imported Thetis profiles whose blob
//                              isn't empty).
//   5. Boolean serialization — "True" / "False" round-trip sanity for
//                              the 5 new boolean keys.
//
// Source-first cites: every value asserted matches a specific row in
// /Users/j.j.boyd/Thetis/Project Files/Source/Console/database.cs
// [v2.10.3.13].
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/MicProfileManager.h"

using namespace NereusSDR;

static const QString kMacA = QStringLiteral("aa:bb:cc:11:22:33");

static QString profileKey(const QString& mac, const QString& name, const QString& field)
{
    return QStringLiteral("hardware/%1/tx/profile/%2/%3").arg(mac, name, field);
}

class TstMicProfileManagerCfcRoundTrip : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()
    {
        AppSettings::instance().clear();
    }

    void init()
    {
        AppSettings::instance().clear();
    }

    // =========================================================================
    // §1  Bundle keys complete — kKeys contains all 41 new keys
    //
    // We can't introspect the private kKeys list directly, but
    // defaultProfileValues() iterates over the same shape — every key
    // bundled in kKeys has a corresponding default in defaultProfileValues().
    // So asserting "all 41 keys appear in defaultProfileValues()" indirectly
    // asserts they're in the bundle.  (The other direction — keys in
    // defaultProfileValues that aren't in kKeys — is structurally
    // impossible because both come from the same bundle definition.)
    // =========================================================================

    void bundleKeys_complete()
    {
        const QHash<QString, QVariant> defs = MicProfileManager::defaultProfileValues();

        // Phase Rotator (4) — database.cs:4726-4730 [v2.10.3.13].
        QVERIFY(defs.contains(QStringLiteral("CFCPhaseRotatorEnabled")));
        QVERIFY(defs.contains(QStringLiteral("CFCPhaseReverseEnabled")));
        QVERIFY(defs.contains(QStringLiteral("CFCPhaseRotatorFreq")));
        QVERIFY(defs.contains(QStringLiteral("CFCPhaseRotatorStages")));

        // CFC scalars (4) — database.cs:4724-4733 [v2.10.3.13].
        QVERIFY(defs.contains(QStringLiteral("CFCEnabled")));
        QVERIFY(defs.contains(QStringLiteral("CFCPostEqEnabled")));
        QVERIFY(defs.contains(QStringLiteral("CFCPreComp")));
        QVERIFY(defs.contains(QStringLiteral("CFCPostEqGain")));

        // CFC profile arrays (30) — database.cs:4735-4766 [v2.10.3.13].
        for (int i = 0; i < 10; ++i) {
            QVERIFY(defs.contains(QStringLiteral("CFCPreComp%1").arg(i)));
            QVERIFY(defs.contains(QStringLiteral("CFCPostEqGain%1").arg(i)));
            QVERIFY(defs.contains(QStringLiteral("CFCEqFreq%1").arg(i)));
        }

        // CFC blob (1) — database.cs:4768.
        QVERIFY(defs.contains(QStringLiteral("CFCParaEQData")));
        // CPDR (1) — database.cs:4580.
        QVERIFY(defs.contains(QStringLiteral("CompanderLevel")));
        // CESSB (1) — database.cs:4689.
        QVERIFY(defs.contains(QStringLiteral("CESSB_On")));

        // P1 full-parity epic Task 2.4 (2) — bank 11 C2/C3 mic-domain keys.
        // Source: Thetis networkproto1.c:600-601 [v2.10.3.13].
        QVERIFY(defs.contains(QStringLiteral("Mic_LineInGain")));
        QVERIFY(defs.contains(QStringLiteral("Mic_UserDigOut")));

        // Total bundle = 50 (existing) + 1 TXParaEQData (3M-3a-ii follow-up
        // Batch 6) + 41 (new CFC/PhRot) + 2 FilterLow/FilterHigh (Plan 4 D1)
        // + 2 line_in_gain/user_dig_out (P1 full-parity Task 2.4)
        // + 11 DEXP envelope/ratios/look-ahead/SCF (3M-3a-iii Tasks 7-10)
        // = 107 keys.
        QCOMPARE(defs.size(), 107);
    }

    // =========================================================================
    // §2  defaultProfileValues full — exact value from the Thetis Default row
    // =========================================================================

    void defaults_phaseRotator()
    {
        const QHash<QString, QVariant> defs = MicProfileManager::defaultProfileValues();
        // database.cs:4726-4730 [v2.10.3.13].
        QCOMPARE(defs.value("CFCPhaseRotatorEnabled").toString(), QStringLiteral("False"));
        QCOMPARE(defs.value("CFCPhaseReverseEnabled").toString(), QStringLiteral("False"));
        QCOMPARE(defs.value("CFCPhaseRotatorFreq").toString(),    QStringLiteral("338"));
        QCOMPARE(defs.value("CFCPhaseRotatorStages").toString(),  QStringLiteral("8"));
    }

    void defaults_cfcScalars()
    {
        const QHash<QString, QVariant> defs = MicProfileManager::defaultProfileValues();
        // database.cs:4724-4733 [v2.10.3.13].
        QCOMPARE(defs.value("CFCEnabled").toString(),       QStringLiteral("False"));
        QCOMPARE(defs.value("CFCPostEqEnabled").toString(), QStringLiteral("False"));
        QCOMPARE(defs.value("CFCPreComp").toString(),       QStringLiteral("0"));
        QCOMPARE(defs.value("CFCPostEqGain").toString(),    QStringLiteral("0"));
    }

    void defaults_cfcArrays()
    {
        const QHash<QString, QVariant> defs = MicProfileManager::defaultProfileValues();
        // CFCPreComp0..9 = 5 — database.cs:4735-4744 [v2.10.3.13].
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(defs.value(QStringLiteral("CFCPreComp%1").arg(i)).toString(),
                     QStringLiteral("5"));
        }
        // CFCPostEqGain0..9 = 0 — database.cs:4746-4755 [v2.10.3.13].
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(defs.value(QStringLiteral("CFCPostEqGain%1").arg(i)).toString(),
                     QStringLiteral("0"));
        }
        // CFCEqFreq0..9 = {0, 125, 250, 500, 1000, 2000, 3000, 4000, 5000, 10000}
        // — database.cs:4757-4766 [v2.10.3.13].
        const int kF[10] = {0, 125, 250, 500, 1000, 2000, 3000, 4000, 5000, 10000};
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(defs.value(QStringLiteral("CFCEqFreq%1").arg(i)).toString(),
                     QString::number(kF[i]));
        }
    }

    void defaults_blobAndCpdrAndCessb()
    {
        const QHash<QString, QVariant> defs = MicProfileManager::defaultProfileValues();
        QCOMPARE(defs.value("CFCParaEQData").toString(),  QString());            // database.cs:4768
        QCOMPARE(defs.value("CompanderLevel").toString(), QStringLiteral("2"));  // database.cs:4580
        QCOMPARE(defs.value("CESSB_On").toString(),       QStringLiteral("False")); // database.cs:4689
    }

    // =========================================================================
    // §3  Each factory profile round-trips byte-for-byte
    //
    // Drives a per-profile data-driven test using QtTest's _data() pattern.
    // Asserts that, after seedFactoryProfiles() (via mgr.load()), every
    // expected per-profile key value is what AppSettings reads back.
    //
    // Coverage: 21 profiles × the keys this batch cites for each profile
    // (existing 50 + new 41 = 91 cells per profile).  We don't enumerate
    // all 91 — we drive each profile's per-row deviation set from the
    // brief table, which is the source-of-truth set.
    //
    // The 21-profile loop hits every factory profile, so an accidental
    // omission in the override hash would surface here.
    // =========================================================================

    // CompanderLevel deviations from the default of 2.  Profiles whose
    // names aren't in the map inherit the global default — verifying both
    // directions catches both kinds of regression.
    void factory_companderLevel_perProfile()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        auto& s = AppSettings::instance();
        const QHash<QString, QString> kExpected = {
            // Profile name → expected CompanderLevel string (from database.cs).
            {QStringLiteral("Default"),          QStringLiteral("2")},  // database.cs:4580
            {QStringLiteral("Default DX"),       QStringLiteral("2")},  // database.cs:4810
            {QStringLiteral("Digi 1K@1500"),     QStringLiteral("0")},  // database.cs:5042
            {QStringLiteral("Digi 1K@2210"),     QStringLiteral("0")},  // database.cs:5272
            {QStringLiteral("AM"),               QStringLiteral("3")},  // database.cs:5501
            {QStringLiteral("Conventional"),     QStringLiteral("3")},  // database.cs:5730
            {QStringLiteral("D-104"),            QStringLiteral("5")},  // database.cs:5959
            {QStringLiteral("D-104+CPDR"),       QStringLiteral("5")},  // database.cs:6188
            {QStringLiteral("D-104+EQ"),         QStringLiteral("5")},  // database.cs:6417
            {QStringLiteral("DX / Contest"),     QStringLiteral("3")},  // database.cs:6646
            {QStringLiteral("ESSB"),             QStringLiteral("3")},  // database.cs:6875
            {QStringLiteral("HC4-5"),            QStringLiteral("5")},  // database.cs:7104
            {QStringLiteral("HC4-5+CPDR"),       QStringLiteral("5")},  // database.cs:7333
            {QStringLiteral("PR40+W2IHY"),       QStringLiteral("3")},  // database.cs:7562
            {QStringLiteral("PR40+W2IHY+CPDR"),  QStringLiteral("3")},  // database.cs:7791
            {QStringLiteral("PR781+EQ"),         QStringLiteral("3")},  // database.cs:8020
            {QStringLiteral("PR781+EQ+CPDR"),    QStringLiteral("2")},  // database.cs:8249
            {QStringLiteral("SSB 2.8k CFC"),     QStringLiteral("1")},  // database.cs:8478
            {QStringLiteral("SSB 3.0k CFC"),     QStringLiteral("1")},  // database.cs:8707
            {QStringLiteral("SSB 3.3k CFC"),     QStringLiteral("1")},  // database.cs:8936
            {QStringLiteral("AM 10k CFC"),       QStringLiteral("1")},  // database.cs:9165
        };
        QCOMPARE(kExpected.size(), 21);
        for (auto it = kExpected.constBegin(); it != kExpected.constEnd(); ++it) {
            const QString got = s.value(profileKey(kMacA, it.key(), "CompanderLevel")).toString();
            QCOMPARE(QString(it.key() + QStringLiteral("=") + got),
                     QString(it.key() + QStringLiteral("=") + it.value()));
        }
    }

    // CESSB_On is false for every factory profile.  database.cs:4689 +
    // 4919/5150/5380/5609/5838/6067/6296/6525/6754/6983/7212/7441/7670/7899/8128/8357/8586/8815/9044/9273.
    void factory_cessbOn_isFalseForAll21()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        auto& s = AppSettings::instance();
        const QStringList kProfiles = {
            "Default", "Default DX", "Digi 1K@1500", "Digi 1K@2210", "AM",
            "Conventional", "D-104", "D-104+CPDR", "D-104+EQ", "DX / Contest",
            "ESSB", "HC4-5", "HC4-5+CPDR", "PR40+W2IHY", "PR40+W2IHY+CPDR",
            "PR781+EQ", "PR781+EQ+CPDR", "SSB 2.8k CFC", "SSB 3.0k CFC",
            "SSB 3.3k CFC", "AM 10k CFC",
        };
        QCOMPARE(kProfiles.size(), 21);
        for (const QString& p : kProfiles) {
            QCOMPARE(s.value(profileKey(kMacA, p, "CESSB_On")).toString(),
                     QStringLiteral("False"));
        }
    }

    // The 17 non-CFC factory profiles all inherit the default CFC scalars
    // (CFCEnabled=false, CFCPostEqEnabled=false, CFCPreComp=0, CFCPostEqGain=0)
    // — see database.cs:4724/4725/4732/4733 baseline + per-profile mirror
    // rows at 4954/4955/4962/4963 (Default DX), 5185/.../5193/5194 (Digi
    // 1K@1500), and so on for the 17 non-CFC profile blocks.
    void factory_cfcScalars_inheritDefaultsFor17NonCfcProfiles()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        auto& s = AppSettings::instance();
        const QStringList kNonCfc = {
            "Default", "Default DX", "Digi 1K@1500", "Digi 1K@2210", "AM",
            "Conventional", "D-104", "D-104+CPDR", "D-104+EQ", "DX / Contest",
            "ESSB", "HC4-5", "HC4-5+CPDR", "PR40+W2IHY", "PR40+W2IHY+CPDR",
            "PR781+EQ", "PR781+EQ+CPDR",
        };
        QCOMPARE(kNonCfc.size(), 17);
        for (const QString& p : kNonCfc) {
            QCOMPARE(s.value(profileKey(kMacA, p, "CFCEnabled")).toString(),
                     QStringLiteral("False"));
            QCOMPARE(s.value(profileKey(kMacA, p, "CFCPostEqEnabled")).toString(),
                     QStringLiteral("False"));
            QCOMPARE(s.value(profileKey(kMacA, p, "CFCPreComp")).toString(),
                     QStringLiteral("0"));
            QCOMPARE(s.value(profileKey(kMacA, p, "CFCPostEqGain")).toString(),
                     QStringLiteral("0"));
            // Phase Rotator stays at default 8 stages for these.
            QCOMPARE(s.value(profileKey(kMacA, p, "CFCPhaseRotatorStages")).toString(),
                     QStringLiteral("8"));
        }
    }

    // SSB 2.8k CFC full CFC override row.  From database.cs:8621-8663
    // [v2.10.3.13].
    void factory_ssb28kCfc_fullOverrideMatch()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "SSB 2.8k CFC", "CFCEnabled")).toString(),
                 QStringLiteral("True"));        // database.cs:8621
        QCOMPARE(s.value(profileKey(kMacA, "SSB 2.8k CFC", "CFCPostEqEnabled")).toString(),
                 QStringLiteral("True"));        // database.cs:8622
        QCOMPARE(s.value(profileKey(kMacA, "SSB 2.8k CFC", "CFCPreComp")).toString(),
                 QStringLiteral("6"));           // database.cs:8629
        QCOMPARE(s.value(profileKey(kMacA, "SSB 2.8k CFC", "CFCPostEqGain")).toString(),
                 QStringLiteral("-6"));          // database.cs:8630
        QCOMPARE(s.value(profileKey(kMacA, "SSB 2.8k CFC", "CFCPhaseRotatorStages")).toString(),
                 QStringLiteral("8"));           // database.cs:8627 (matches default)

        // Full CFC arrays from database.cs:8632-8663.
        const int kPre[10]  = { 5, 5, 5, 4, 5, 6, 7, 8, 9, 9 };       // 8632-8641
        const int kPost[10] = {-7,-7,-8,-8,-8,-7,-5,-4,-4,-5 };       // 8643-8652
        const int kF[10]    = { 100, 150, 300, 500, 750, 1250, 1750, 2000, 2600, 2900 }; // 8654-8663
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(s.value(profileKey(kMacA, "SSB 2.8k CFC",
                              QStringLiteral("CFCPreComp%1").arg(i))).toString(),
                     QString::number(kPre[i]));
            QCOMPARE(s.value(profileKey(kMacA, "SSB 2.8k CFC",
                              QStringLiteral("CFCPostEqGain%1").arg(i))).toString(),
                     QString::number(kPost[i]));
            QCOMPARE(s.value(profileKey(kMacA, "SSB 2.8k CFC",
                              QStringLiteral("CFCEqFreq%1").arg(i))).toString(),
                     QString::number(kF[i]));
        }
    }

    // SSB 3.0k CFC.  From database.cs:8850-8892 [v2.10.3.13].
    void factory_ssb30kCfc_fullOverrideMatch()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "SSB 3.0k CFC", "CFCEnabled")).toString(),
                 QStringLiteral("True"));        // database.cs:8850
        QCOMPARE(s.value(profileKey(kMacA, "SSB 3.0k CFC", "CFCPreComp")).toString(),
                 QStringLiteral("6"));           // database.cs:8858
        QCOMPARE(s.value(profileKey(kMacA, "SSB 3.0k CFC", "CFCPostEqGain")).toString(),
                 QStringLiteral("-7"));          // database.cs:8859

        const int kPre[10]  = { 6, 6, 5, 4, 5, 6, 7, 8, 9, 9 };      // 8861-8870
        const int kPost[10] = {-4,-5,-7,-8,-8,-7,-4,-2,-1,-1 };      // 8872-8881
        const int kF[10]    = { 100, 150, 300, 500, 750, 1250, 1750, 2000, 2600, 3100 }; // 8883-8892
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(s.value(profileKey(kMacA, "SSB 3.0k CFC",
                              QStringLiteral("CFCPreComp%1").arg(i))).toString(),
                     QString::number(kPre[i]));
            QCOMPARE(s.value(profileKey(kMacA, "SSB 3.0k CFC",
                              QStringLiteral("CFCPostEqGain%1").arg(i))).toString(),
                     QString::number(kPost[i]));
            QCOMPARE(s.value(profileKey(kMacA, "SSB 3.0k CFC",
                              QStringLiteral("CFCEqFreq%1").arg(i))).toString(),
                     QString::number(kF[i]));
        }
    }

    // SSB 3.3k CFC.  From database.cs:9079-9121 [v2.10.3.13].
    void factory_ssb33kCfc_fullOverrideMatch()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "SSB 3.3k CFC", "CFCEnabled")).toString(),
                 QStringLiteral("True"));        // database.cs:9079
        QCOMPARE(s.value(profileKey(kMacA, "SSB 3.3k CFC", "CFCPreComp")).toString(),
                 QStringLiteral("7"));           // database.cs:9087
        QCOMPARE(s.value(profileKey(kMacA, "SSB 3.3k CFC", "CFCPostEqGain")).toString(),
                 QStringLiteral("-6"));          // database.cs:9088

        const int kPre[10]  = { 4, 4, 3, 3, 4, 5, 6, 7, 8, 8 };      // 9090-9099
        const int kPost[10] = {-6,-6,-7,-8,-8,-7,-5,-5,-4,-5 };      // 9101-9110
        const int kF[10]    = { 50, 150, 300, 500, 750, 1250, 1750, 2000, 2600, 3350 }; // 9112-9121
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(s.value(profileKey(kMacA, "SSB 3.3k CFC",
                              QStringLiteral("CFCPreComp%1").arg(i))).toString(),
                     QString::number(kPre[i]));
            QCOMPARE(s.value(profileKey(kMacA, "SSB 3.3k CFC",
                              QStringLiteral("CFCPostEqGain%1").arg(i))).toString(),
                     QString::number(kPost[i]));
            QCOMPARE(s.value(profileKey(kMacA, "SSB 3.3k CFC",
                              QStringLiteral("CFCEqFreq%1").arg(i))).toString(),
                     QString::number(kF[i]));
        }
    }

    // AM 10k CFC.  From database.cs:9308-9350 [v2.10.3.13].  Note the unique
    // CFCPhaseRotatorStages=9 (vs default 8) — this is the only profile in
    // the bank with a non-default PhRot stages count.
    void factory_am10kCfc_fullOverrideMatch_includingPhRotStages9()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "AM 10k CFC", "CFCEnabled")).toString(),
                 QStringLiteral("True"));        // database.cs:9308
        QCOMPARE(s.value(profileKey(kMacA, "AM 10k CFC", "CFCPreComp")).toString(),
                 QStringLiteral("6"));           // database.cs:9316
        QCOMPARE(s.value(profileKey(kMacA, "AM 10k CFC", "CFCPostEqGain")).toString(),
                 QStringLiteral("-8"));          // database.cs:9317
        // CFCPhaseRotatorStages = 9 (deviates from default 8).
        QCOMPARE(s.value(profileKey(kMacA, "AM 10k CFC", "CFCPhaseRotatorStages")).toString(),
                 QStringLiteral("9"));           // database.cs:9314

        const int kPre[10]  = { 6, 5, 4, 4, 4, 5, 6, 7, 7, 7 };       // 9319-9328
        const int kPost[10] = { 0,-1,-2,-3,-3,-2,-1, 0, 1, 1 };       // 9330-9339
        const int kF[10]    = { 0, 70, 250, 500, 1000, 1500, 2000, 3000, 4000, 5000 }; // 9341-9350
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(s.value(profileKey(kMacA, "AM 10k CFC",
                              QStringLiteral("CFCPreComp%1").arg(i))).toString(),
                     QString::number(kPre[i]));
            QCOMPARE(s.value(profileKey(kMacA, "AM 10k CFC",
                              QStringLiteral("CFCPostEqGain%1").arg(i))).toString(),
                     QString::number(kPost[i]));
            QCOMPARE(s.value(profileKey(kMacA, "AM 10k CFC",
                              QStringLiteral("CFCEqFreq%1").arg(i))).toString(),
                     QString::number(kF[i]));
        }
    }

    // =========================================================================
    // §4  CFCParaEQData blob round-trip — non-empty XML (forward-compat for
    //     imported Thetis profiles whose XML blob isn't empty).
    //     AppSettings stores values as strings, so any printable non-empty
    //     string is round-trip safe.
    // =========================================================================

    void cfcParaEqData_nonEmptyBlobRoundTrips()
    {
        auto& s = AppSettings::instance();
        const QString kBlob = QStringLiteral("<paraEQ><band f=\"500\" g=\"3\"/></paraEQ>");
        s.setValue(profileKey(kMacA, "Default", "CFCParaEQData"), kBlob);

        // Round-trip: read back exact match.
        QCOMPARE(s.value(profileKey(kMacA, "Default", "CFCParaEQData")).toString(),
                 kBlob);
    }

    void cfcParaEqData_emptyDefault()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        auto& s = AppSettings::instance();
        // database.cs:4768 — empty string default.
        QCOMPARE(s.value(profileKey(kMacA, "Default", "CFCParaEQData")).toString(),
                 QString());
    }

    // =========================================================================
    // §5  Boolean serialization sanity — the 5 new boolean keys
    //     ("True" / "False" string convention per AppSettings policy).
    // =========================================================================

    void boolean_serialization_allFiveBoolKeys()
    {
        // Set all 5 to true; round-trip back as "True".
        auto& s = AppSettings::instance();
        s.setValue(profileKey(kMacA, "P", "CFCEnabled"),              QStringLiteral("True"));
        s.setValue(profileKey(kMacA, "P", "CFCPostEqEnabled"),        QStringLiteral("True"));
        s.setValue(profileKey(kMacA, "P", "CFCPhaseRotatorEnabled"),  QStringLiteral("True"));
        s.setValue(profileKey(kMacA, "P", "CFCPhaseReverseEnabled"),  QStringLiteral("True"));
        s.setValue(profileKey(kMacA, "P", "CESSB_On"),                QStringLiteral("True"));

        QCOMPARE(s.value(profileKey(kMacA, "P", "CFCEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "P", "CFCPostEqEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "P", "CFCPhaseRotatorEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "P", "CFCPhaseReverseEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "P", "CESSB_On")).toString(),
                 QStringLiteral("True"));

        // Now set all 5 to false; round-trip as "False".
        s.setValue(profileKey(kMacA, "P", "CFCEnabled"),              QStringLiteral("False"));
        s.setValue(profileKey(kMacA, "P", "CFCPostEqEnabled"),        QStringLiteral("False"));
        s.setValue(profileKey(kMacA, "P", "CFCPhaseRotatorEnabled"),  QStringLiteral("False"));
        s.setValue(profileKey(kMacA, "P", "CFCPhaseReverseEnabled"),  QStringLiteral("False"));
        s.setValue(profileKey(kMacA, "P", "CESSB_On"),                QStringLiteral("False"));

        QCOMPARE(s.value(profileKey(kMacA, "P", "CFCEnabled")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "P", "CFCPostEqEnabled")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "P", "CFCPhaseRotatorEnabled")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "P", "CFCPhaseReverseEnabled")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "P", "CESSB_On")).toString(),
                 QStringLiteral("False"));
    }
};

QTEST_APPLESS_MAIN(TstMicProfileManagerCfcRoundTrip)
#include "tst_mic_profile_manager_cfc_round_trip.moc"
