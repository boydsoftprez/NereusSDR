// no-port-check: NereusSDR-original unit-test file.
// =================================================================
// tests/tst_mic_profile_manager_cfc_live_path.cpp  (NereusSDR)
// =================================================================
//
// Phase 3M-3a-ii Batch 4.5 — live-path round-trip tests for the 41
// new CFC + CPDR + CESSB + PhRot bundle keys.
//
// Closes the gap between Batch 4 (MicProfileManager bundle keys +
// defaults + 21 factory profile rows) and the TxCfcDialog [Save] /
// profile-combo activation flow, which round-trips state through
// MicProfileManager::captureLiveValues / applyValuesToModel via the
// public saveProfile / setActiveProfile API.
//
// Coverage:
//   1. Capture round-trip   — set 41 TM properties to non-default
//                              values, save profile, assert all 41
//                              bundle keys read back from AppSettings
//                              with the expected stringified values.
//   2. Apply round-trip     — create a profile under non-default values
//                              via saveProfile, mutate TM to baseline,
//                              setActiveProfile, assert all 41 TM
//                              getters return the saved values.
//   3. Profile-switch lifecycle — State A save, State B save, switch
//                                  back to A, assert TM matches A.
//                                  Models the dialog [Save] → combo
//                                  switch flow end-to-end.
//   4. Boolean serialization sanity — verify the 5 new bool keys
//                                      (CFCEnabled / CFCPostEqEnabled /
//                                      CFCPhaseRotatorEnabled /
//                                      CFCPhaseReverseEnabled / CESSB_On)
//                                      round-trip through True/False.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/MicProfileManager.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

static const QString kMacA = QStringLiteral("aa:bb:cc:11:22:33");

static QString profileKey(const QString& mac, const QString& name, const QString& field)
{
    return QStringLiteral("hardware/%1/tx/profile/%2/%3").arg(mac, name, field);
}

class TstMicProfileManagerCfcLivePath : public QObject {
    Q_OBJECT

private slots:

    void typedProfileNotificationExists()
    {
        TransmitModel tx;
        QVERIFY(tx.metaObject()->indexOfSignal("cfcEditProfileChanged(NereusSDR::CfcEditProfile)") >= 0
                || tx.metaObject()->indexOfSignal("cfcEditProfileChanged(CfcEditProfile)") >= 0);
    }

    void nestedProfileBatchesPublishOnlyTheFinalState()
    {
        TransmitModel tx;
        QSignalSpy aggregate(&tx, &TransmitModel::cfcEditProfileChanged);
        QSignalSpy band(&tx, &TransmitModel::cfcCompressionChanged);
        tx.beginCfcProfileUpdate();
        tx.setCfcCompression(1, 6);
        tx.beginCfcProfileUpdate();
        tx.setCfcPostEqBandGain(1, -7);
        tx.setCfcParaEqData("unsupported-saved-blob");
        tx.endCfcProfileUpdate();
        QCOMPARE(aggregate.count(), 0);
        tx.setCfcPrecompDb(4);
        tx.endCfcProfileUpdate();
        QCOMPARE(aggregate.count(), 1);
        const CfcEditProfile p = qvariant_cast<CfcEditProfile>(aggregate[0][0]);
        QCOMPARE(p.compression.gainsDb[1], 6.0);
        QCOMPARE(p.postEq.gainsDb[1], -7.0);
        QCOMPARE(p.compression.globalGainDb, 4.0);
        QVERIFY(!p.compression.useQ);
        QCOMPARE(tx.cfcParaEqData(), QString("unsupported-saved-blob"));
        QCOMPARE(band.count(), 1);
        tx.setCfcCompression(1, 6);
        QCOMPARE(aggregate.count(), 1);
        QCOMPARE(band.count(), 1);
    }

    void initTestCase()
    {
        AppSettings::instance().clear();
    }

    void init()
    {
        AppSettings::instance().clear();
    }

    // =========================================================================
    // §1  Capture round-trip
    //
    // Set every one of the 41 new TM properties to a non-default value,
    // saveProfile via the public API (which calls captureLiveValues
    // internally), then read the AppSettings bundle keys back and
    // compare to the stringified value the brief table specifies.
    // =========================================================================

    void capture_phaseRotator_4keys()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setPhaseRotatorEnabled(true);
        tx.setPhaseReverseEnabled(true);
        tx.setPhaseRotatorFreqHz(450);
        tx.setPhaseRotatorStages(7);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("PhRot"), &tx));

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "PhRot", "CFCPhaseRotatorEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "PhRot", "CFCPhaseReverseEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "PhRot", "CFCPhaseRotatorFreq")).toString(),
                 QStringLiteral("450"));
        QCOMPARE(s.value(profileKey(kMacA, "PhRot", "CFCPhaseRotatorStages")).toString(),
                 QStringLiteral("7"));
    }

    void capture_cfcScalars_4keys()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setCfcEnabled(true);
        tx.setCfcPostEqEnabled(true);
        tx.setCfcPrecompDb(8);
        tx.setCfcPostEqGainDb(-4);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("CfcScalars"), &tx));

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "CfcScalars", "CFCEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "CfcScalars", "CFCPostEqEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "CfcScalars", "CFCPreComp")).toString(),
                 QStringLiteral("8"));
        QCOMPARE(s.value(profileKey(kMacA, "CfcScalars", "CFCPostEqGain")).toString(),
                 QStringLiteral("-4"));
    }

    void capture_cfcArrays_30keys()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        // Three distinct shapes, one per array, so a key swap regresses.
        const int kFreq[10] = { 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000 };
        const int kComp[10] = {   2,   3,   4,   5,   6,   7,   8,   9,  10,  11 };
        const int kPost[10] = {  -1,  -2,  -3,  -4,  -5,   1,   2,   3,   4,   5 };
        for (int i = 0; i < 10; ++i) {
            tx.setCfcEqFreq(i, kFreq[i]);
            tx.setCfcCompression(i, kComp[i]);
            tx.setCfcPostEqBandGain(i, kPost[i]);
        }

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("CfcArrays"), &tx));

        auto& s = AppSettings::instance();
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(s.value(profileKey(kMacA, "CfcArrays",
                              QStringLiteral("CFCEqFreq%1").arg(i))).toString(),
                     QString::number(kFreq[i]));
            QCOMPARE(s.value(profileKey(kMacA, "CfcArrays",
                              QStringLiteral("CFCPreComp%1").arg(i))).toString(),
                     QString::number(kComp[i]));
            QCOMPARE(s.value(profileKey(kMacA, "CfcArrays",
                              QStringLiteral("CFCPostEqGain%1").arg(i))).toString(),
                     QString::number(kPost[i]));
        }
    }

    void capture_cfcParaEqDataBlob()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        const QString kBlob = QStringLiteral("<paraEQ><band f=\"500\" g=\"3\"/></paraEQ>");
        tx.setCfcParaEqData(kBlob);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("Blob"), &tx));

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "Blob", "CFCParaEQData")).toString(), kBlob);
    }

    void capture_cpdrLevel_singleKey()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setCpdrLevelDb(7);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("Cpdr"), &tx));

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "Cpdr", "CompanderLevel")).toString(),
                 QStringLiteral("7"));
    }

    // cpdrOn is global console state, NOT in the profile bundle.
    // Verify that toggling cpdrOn on the model does NOT cause a
    // CompanderOn/cpdrOn key to leak into the bundle.
    void capture_cpdrOn_isNotInBundle()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setCpdrLevelDb(4);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("NoOn"), &tx));

        auto& s = AppSettings::instance();
        // The standard CPDR key is present.
        QCOMPARE(s.value(profileKey(kMacA, "NoOn", "CompanderLevel")).toString(),
                 QStringLiteral("4"));
        // No "CompanderOn" or "cpdrOn" or "CPDROn" flag bundled with the profile.
        QVERIFY(!s.contains(profileKey(kMacA, "NoOn", "CompanderOn")));
        QVERIFY(!s.contains(profileKey(kMacA, "NoOn", "cpdrOn")));
        QVERIFY(!s.contains(profileKey(kMacA, "NoOn", "CPDROn")));
    }

    void capture_cessbOn_singleKey()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setCessbOn(true);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("Cessb"), &tx));

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "Cessb", "CESSB_On")).toString(),
                 QStringLiteral("True"));
    }

    // =========================================================================
    // §2  Apply round-trip
    //
    // Save a profile with non-default 41 keys, mutate TM, then
    // setActiveProfile and assert TM getters return the saved values.
    // =========================================================================

    void apply_phaseRotator_4keys()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setPhaseRotatorEnabled(true);
        tx.setPhaseReverseEnabled(true);
        tx.setPhaseRotatorFreqHz(421);
        tx.setPhaseRotatorStages(11);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("PhRotApply"), &tx));

        // Mutate TM back to defaults.
        tx.setPhaseRotatorEnabled(false);
        tx.setPhaseReverseEnabled(false);
        tx.setPhaseRotatorFreqHz(338);
        tx.setPhaseRotatorStages(8);

        QVERIFY(mgr.setActiveProfile(QStringLiteral("PhRotApply"), &tx));
        QCOMPARE(tx.phaseRotatorEnabled(), true);
        QCOMPARE(tx.phaseReverseEnabled(), true);
        QCOMPARE(tx.phaseRotatorFreqHz(),  421);
        QCOMPARE(tx.phaseRotatorStages(),  11);
    }

    void apply_cfcScalars_4keys()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setCfcEnabled(true);
        tx.setCfcPostEqEnabled(true);
        tx.setCfcPrecompDb(9);
        tx.setCfcPostEqGainDb(-7);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("CfcScalarsApply"), &tx));

        tx.setCfcEnabled(false);
        tx.setCfcPostEqEnabled(false);
        tx.setCfcPrecompDb(0);
        tx.setCfcPostEqGainDb(0);

        QVERIFY(mgr.setActiveProfile(QStringLiteral("CfcScalarsApply"), &tx));
        QCOMPARE(tx.cfcEnabled(), true);
        QCOMPARE(tx.cfcPostEqEnabled(), true);
        QCOMPARE(tx.cfcPrecompDb(),    9);
        QCOMPARE(tx.cfcPostEqGainDb(), -7);
    }

    void apply_cfcArrays_30keys()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        const int kFreq[10] = {  50, 150, 350, 600, 850, 1100, 1450, 1850, 2400, 3500 };
        const int kComp[10] = {   1,   2,   3,   4,   5,    6,    7,    8,    9,   10 };
        const int kPost[10] = {  -9,  -7,  -5,  -3,  -1,    1,    3,    5,    7,    9 };
        for (int i = 0; i < 10; ++i) {
            tx.setCfcEqFreq(i, kFreq[i]);
            tx.setCfcCompression(i, kComp[i]);
            tx.setCfcPostEqBandGain(i, kPost[i]);
        }

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("CfcArraysApply"), &tx));

        // Mutate TM to defaults.
        for (int i = 0; i < 10; ++i) {
            tx.setCfcEqFreq(i, 0);
            tx.setCfcCompression(i, 5);
            tx.setCfcPostEqBandGain(i, 0);
        }

        QVERIFY(mgr.setActiveProfile(QStringLiteral("CfcArraysApply"), &tx));
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(tx.cfcEqFreq(i),         kFreq[i]);
            QCOMPARE(tx.cfcCompression(i),    kComp[i]);
            QCOMPARE(tx.cfcPostEqBandGain(i), kPost[i]);
        }
    }

    void apply_cfcParaEqDataBlob()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        const QString kBlob =
            QStringLiteral("<paraEQ><band f=\"1000\" g=\"-2\"/><band f=\"2000\" g=\"4\"/></paraEQ>");
        tx.setCfcParaEqData(kBlob);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("BlobApply"), &tx));

        tx.setCfcParaEqData(QString());
        QCOMPARE(tx.cfcParaEqData(), QString());

        QVERIFY(mgr.setActiveProfile(QStringLiteral("BlobApply"), &tx));
        QCOMPARE(tx.cfcParaEqData(), kBlob);
    }

    void apply_cpdrLevel_singleKey()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setCpdrLevelDb(6);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("CpdrApply"), &tx));

        tx.setCpdrLevelDb(2);
        QVERIFY(mgr.setActiveProfile(QStringLiteral("CpdrApply"), &tx));
        QCOMPARE(tx.cpdrLevelDb(), 6);
    }

    void apply_cessbOn_singleKey()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setCessbOn(true);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("CessbApply"), &tx));

        tx.setCessbOn(false);
        QVERIFY(mgr.setActiveProfile(QStringLiteral("CessbApply"), &tx));
        QCOMPARE(tx.cessbOn(), true);
    }

    // =========================================================================
    // §3  Profile-switch lifecycle
    //
    // Set TM to State A, save as ProfileA.  Set TM to State B, save as
    // ProfileB.  Switch active to ProfileA.  Assert every one of the 41
    // TM getters reports State A.  This tests the dialog-side flow end
    // to end (TxCfcDialog [Save] → combo switch).
    // =========================================================================

    void profileSwitch_lifecycle_endToEnd()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);

        // ── State A ─────────────────────────────────────────────────────────
        tx.setPhaseRotatorEnabled(true);
        tx.setPhaseReverseEnabled(false);
        tx.setPhaseRotatorFreqHz(338);
        tx.setPhaseRotatorStages(8);
        tx.setCfcEnabled(true);
        tx.setCfcPostEqEnabled(true);
        tx.setCfcPrecompDb(6);
        tx.setCfcPostEqGainDb(-6);
        const int kFreqA[10] = { 100, 150, 300, 500, 750, 1250, 1750, 2000, 2600, 2900 };
        const int kCompA[10] = {   5,   5,   5,   4,   5,    6,    7,    8,    9,    9 };
        const int kPostA[10] = {  -7,  -7,  -8,  -8,  -8,   -7,   -5,   -4,   -4,   -5 };
        for (int i = 0; i < 10; ++i) {
            tx.setCfcEqFreq(i, kFreqA[i]);
            tx.setCfcCompression(i, kCompA[i]);
            tx.setCfcPostEqBandGain(i, kPostA[i]);
        }
        tx.setCfcParaEqData(QStringLiteral("<paraEQ>A</paraEQ>"));
        tx.setCpdrLevelDb(1);
        tx.setCessbOn(false);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("ProfileA"), &tx));

        // ── State B (different in every key) ────────────────────────────────
        tx.setPhaseRotatorEnabled(false);
        tx.setPhaseReverseEnabled(true);
        tx.setPhaseRotatorFreqHz(500);
        tx.setPhaseRotatorStages(9);
        tx.setCfcEnabled(false);
        tx.setCfcPostEqEnabled(false);
        tx.setCfcPrecompDb(0);
        tx.setCfcPostEqGainDb(0);
        const int kFreqB[10] = {   0, 125, 250, 500, 1000, 2000, 3000, 4000, 5000, 10000 };
        const int kCompB[10] = {   5,   5,   5,   5,    5,    5,    5,    5,    5,     5 };
        const int kPostB[10] = {   0,   0,   0,   0,    0,    0,    0,    0,    0,     0 };
        for (int i = 0; i < 10; ++i) {
            tx.setCfcEqFreq(i, kFreqB[i]);
            tx.setCfcCompression(i, kCompB[i]);
            tx.setCfcPostEqBandGain(i, kPostB[i]);
        }
        tx.setCfcParaEqData(QStringLiteral("<paraEQ>B</paraEQ>"));
        tx.setCpdrLevelDb(2);
        tx.setCessbOn(true);

        QVERIFY(mgr.saveProfile(QStringLiteral("ProfileB"), &tx));

        // Switch active to ProfileA — every TM getter should snap back to State A.
        QVERIFY(mgr.setActiveProfile(QStringLiteral("ProfileA"), &tx));

        QCOMPARE(tx.phaseRotatorEnabled(), true);
        QCOMPARE(tx.phaseReverseEnabled(), false);
        QCOMPARE(tx.phaseRotatorFreqHz(),  338);
        QCOMPARE(tx.phaseRotatorStages(),  8);
        QCOMPARE(tx.cfcEnabled(),          true);
        QCOMPARE(tx.cfcPostEqEnabled(),    true);
        QCOMPARE(tx.cfcPrecompDb(),        6);
        QCOMPARE(tx.cfcPostEqGainDb(),     -6);
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(tx.cfcEqFreq(i),         kFreqA[i]);
            QCOMPARE(tx.cfcCompression(i),    kCompA[i]);
            QCOMPARE(tx.cfcPostEqBandGain(i), kPostA[i]);
        }
        QCOMPARE(tx.cfcParaEqData(), QStringLiteral("<paraEQ>A</paraEQ>"));
        QCOMPARE(tx.cpdrLevelDb(),   1);
        QCOMPARE(tx.cessbOn(),       false);

        // And switch to ProfileB — every getter snaps to State B.
        QVERIFY(mgr.setActiveProfile(QStringLiteral("ProfileB"), &tx));
        QCOMPARE(tx.phaseRotatorEnabled(), false);
        QCOMPARE(tx.phaseReverseEnabled(), true);
        QCOMPARE(tx.phaseRotatorFreqHz(),  500);
        QCOMPARE(tx.phaseRotatorStages(),  9);
        QCOMPARE(tx.cfcEnabled(),          false);
        QCOMPARE(tx.cfcPostEqEnabled(),    false);
        QCOMPARE(tx.cfcPrecompDb(),        0);
        QCOMPARE(tx.cfcPostEqGainDb(),     0);
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(tx.cfcEqFreq(i),         kFreqB[i]);
            QCOMPARE(tx.cfcCompression(i),    kCompB[i]);
            QCOMPARE(tx.cfcPostEqBandGain(i), kPostB[i]);
        }
        QCOMPARE(tx.cfcParaEqData(), QStringLiteral("<paraEQ>B</paraEQ>"));
        QCOMPARE(tx.cpdrLevelDb(),   2);
        QCOMPARE(tx.cessbOn(),       true);

        // Switch back to ProfileA one more time — full restoration.
        QVERIFY(mgr.setActiveProfile(QStringLiteral("ProfileA"), &tx));
        QCOMPARE(tx.cfcEnabled(),         true);
        QCOMPARE(tx.cfcPrecompDb(),       6);
        QCOMPARE(tx.cfcParaEqData(), QStringLiteral("<paraEQ>A</paraEQ>"));
        QCOMPARE(tx.cpdrLevelDb(),       1);
    }

    // =========================================================================
    // §4  Boolean serialization sanity
    //
    // Each of the 5 new bool keys must round-trip through "True"/"False"
    // strings (per AppSettings convention) — both directions, capture
    // and apply.
    // =========================================================================

    void boolean_capture_allFiveKeys_TrueSide()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setPhaseRotatorEnabled(true);
        tx.setPhaseReverseEnabled(true);
        tx.setCfcEnabled(true);
        tx.setCfcPostEqEnabled(true);
        tx.setCessbOn(true);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("Bools"), &tx));

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "Bools", "CFCPhaseRotatorEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "Bools", "CFCPhaseReverseEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "Bools", "CFCEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "Bools", "CFCPostEqEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(profileKey(kMacA, "Bools", "CESSB_On")).toString(),
                 QStringLiteral("True"));
    }

    void boolean_capture_allFiveKeys_FalseSide()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        tx.setPhaseRotatorEnabled(false);
        tx.setPhaseReverseEnabled(false);
        tx.setCfcEnabled(false);
        tx.setCfcPostEqEnabled(false);
        tx.setCessbOn(false);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("BoolsFalse"), &tx));

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(profileKey(kMacA, "BoolsFalse", "CFCPhaseRotatorEnabled")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "BoolsFalse", "CFCPhaseReverseEnabled")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "BoolsFalse", "CFCEnabled")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "BoolsFalse", "CFCPostEqEnabled")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(profileKey(kMacA, "BoolsFalse", "CESSB_On")).toString(),
                 QStringLiteral("False"));
    }

    void boolean_apply_allFiveKeys_RoundTrip()
    {
        TransmitModel tx;
        tx.loadFromSettings(kMacA);
        // True snapshot.
        tx.setPhaseRotatorEnabled(true);
        tx.setPhaseReverseEnabled(true);
        tx.setCfcEnabled(true);
        tx.setCfcPostEqEnabled(true);
        tx.setCessbOn(true);

        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("AllTrue"), &tx));

        // False snapshot saved under a different name.
        tx.setPhaseRotatorEnabled(false);
        tx.setPhaseReverseEnabled(false);
        tx.setCfcEnabled(false);
        tx.setCfcPostEqEnabled(false);
        tx.setCessbOn(false);
        QVERIFY(mgr.saveProfile(QStringLiteral("AllFalse"), &tx));

        // Apply AllTrue → every flag must be true.
        QVERIFY(mgr.setActiveProfile(QStringLiteral("AllTrue"), &tx));
        QCOMPARE(tx.phaseRotatorEnabled(), true);
        QCOMPARE(tx.phaseReverseEnabled(), true);
        QCOMPARE(tx.cfcEnabled(),          true);
        QCOMPARE(tx.cfcPostEqEnabled(),    true);
        QCOMPARE(tx.cessbOn(),             true);

        // Apply AllFalse → every flag must be false.
        QVERIFY(mgr.setActiveProfile(QStringLiteral("AllFalse"), &tx));
        QCOMPARE(tx.phaseRotatorEnabled(), false);
        QCOMPARE(tx.phaseReverseEnabled(), false);
        QCOMPARE(tx.cfcEnabled(),          false);
        QCOMPARE(tx.cfcPostEqEnabled(),    false);
        QCOMPARE(tx.cessbOn(),             false);
    }
};

QTEST_APPLESS_MAIN(TstMicProfileManagerCfcLivePath)
#include "tst_mic_profile_manager_cfc_live_path.moc"
