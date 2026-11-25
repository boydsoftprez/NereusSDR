// =================================================================
// tests/tst_tx_profile_file.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original file.
//
// One transmit profile file, and import into the per-radio bank. No radio,
// no audio device; nothing here keys a radio.
//
// Modification history (NereusSDR):
//   2026-11-17 - Created: the file's round trip and refusals, and import's
//                 defaults and unknown keys. Brian Sutton (W4KZO),
//                 AI-assisted via xAI Grok.
//   2026-11-22 - Every field round trips (both curves, every switch); the
//                 wiring keys stay this radio's; names, values and a file
//                 with no settings are refused in plain words; every
//                 factory profile imports; a name with two underscores in
//                 a row is refused and one with a single underscore saves
//                 and reloads. J.J. Boyd (KG4VCF), AI-assisted via Claude
//                 Code.
//   2026-11-25 - Only a name whose keys would not come back is refused;
//                 the names that met an escape save and reload; an old
//                 profile with double underscores selects and deletes. J.J.
//                 Boyd (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/CfcProfile.h"
#include "core/MicProfileManager.h"
#include "core/ParaEqCurve.h"
#include "core/TxProfileFile.h"
#include "models/TransmitModel.h"

#include "OperatorWording.h"

#include <array>

using namespace NereusSDR;

namespace {
const QString kMac = QStringLiteral("aa:bb:cc:11:22:33");

QString field(const QString& name, const QString& key)
{
    return QStringLiteral("hardware/%1/tx/profile/%2/%3").arg(kMac, name, key);
}

// Every profile setting away from its factory value, the wiring included.
void setEveryField(TransmitModel& tx)
{
    tx.setMicGainDb(-13);
    tx.setMicBoost(false);
    tx.setMicXlr(false);
    tx.setLineIn(true);
    tx.setLineInBoost(-4.5);
    tx.setMicTipRing(false);
    tx.setMicBias(true);
    tx.setMicPttDisabled(true);
    tx.setLineInGain(9);
    tx.setUserDigOut(5);
    tx.setVoxThresholdDb(-31);
    tx.setVoxGainScalar(2.5f);
    tx.setVoxHangTimeMs(700);
    tx.setAntiVoxGainDb(-5);
    tx.setMonitorVolume(0.25f);
    tx.setMicSource(MicSource::Radio);
    tx.setTxEqEnabled(true);
    tx.setTxEqPreamp(3);
    const std::array<int, 10> eqFreq{30, 60, 120, 240, 480, 960, 1900, 3800, 7600, 15000};
    for (int i = 0; i < 10; ++i) {
        tx.setTxEqBand(i, i - 5);
        tx.setTxEqFreq(i, eqFreq[static_cast<std::size_t>(i)]);
    }
    ParaEqCurve::TxEqPoints points = ParaEqCurve::defaultTxEqPoints();
    tx.setTxEqParaEqData(ParaEqCurve::txEqParaEqDataFromPoints(points));
    tx.setTxEqUseLegacy(false);
    tx.setTxLevelerOn(false);
    tx.setTxLevelerMaxGain(11);
    tx.setTxLevelerDecay(250);
    tx.setTxAlcMaxGain(2);
    tx.setTxAlcDecay(20);
    tx.setPhaseRotatorEnabled(true);
    tx.setPhaseReverseEnabled(true);
    tx.setPhaseRotatorFreqHz(400);
    tx.setPhaseRotatorStages(6);
    const std::array<int, 10> cfcFreq{0, 150, 300, 600, 1100, 2100, 3100, 4100, 5100, 9000};
    const std::array<int, 10> cfcComp{4, 5, 6, 7, 8, 7, 6, 5, 4, 3};
    const std::array<int, 10> cfcPost{-2, -1, 0, 1, 2, 3, 2, 1, 0, -1};
    tx.setCfcEnabled(true);
    tx.setCfcPostEqEnabled(true);
    tx.setCfcPrecompDb(4);
    tx.setCfcPostEqGainDb(-3);
    for (int i = 0; i < 10; ++i) {
        tx.setCfcEqFreq(i, cfcFreq[static_cast<std::size_t>(i)]);
        tx.setCfcCompression(i, cfcComp[static_cast<std::size_t>(i)]);
        tx.setCfcPostEqBandGain(i, cfcPost[static_cast<std::size_t>(i)]);
    }
    tx.setCfcParaEqData(CfcProfile::encode(
        CfcProfile::legacyProfile(cfcFreq, cfcComp, cfcPost, 4, -3)));
    tx.setCpdrLevelDb(5);
    tx.setAmCarrierLevel(80);
    tx.setCessbOn(true);
    tx.setFilterLow(150);
    tx.setFilterHigh(2700);
    tx.setDexpEnabled(true);
    tx.setDexpDetectorTauMs(30);
    tx.setDexpAttackTimeMs(5);
    tx.setDexpReleaseTimeMs(200);
    tx.setDexpExpansionRatioDb(12);
    tx.setDexpHysteresisRatioDb(3);
    tx.setDexpLookAheadEnabled(false);
    tx.setDexpLookAheadMs(80);
    tx.setDexpLowCutHz(400);
    tx.setDexpHighCutHz(2000);
    tx.setDexpSideChannelFilterEnabled(false);
}

TxProfileFile::Document documentOf(const QString& name, const QHash<QString, QVariant>& values)
{
    TxProfileFile::Document document;
    document.name = name;
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        document.keys.insert(it.key(), it.value().toString());
    }
    return document;
}

QHash<QString, QVariant> variants(const QHash<QString, QString>& keys)
{
    QHash<QString, QVariant> out;
    for (auto it = keys.constBegin(); it != keys.constEnd(); ++it) {
        out.insert(it.key(), it.value());
    }
    return out;
}
} // namespace

class TstTxProfileFile : public QObject {
    Q_OBJECT
private slots:
    void init() { AppSettings::instance().clear(); }

    void roundTripKeepsTheNameAndKeys()
    {
        TxProfileFile::Document in;
        in.name = QStringLiteral("DX");
        in.keys.insert(QStringLiteral("MicGain"), QStringLiteral("-12"));
        in.keys.insert(QStringLiteral("FilterLow"), QStringLiteral("150"));
        QByteArray bytes;
        QString error;
        QVERIFY(TxProfileFile::encode(in, &bytes, &error));
        QVERIFY(error.isEmpty());
        TxProfileFile::Document out;
        QVERIFY(TxProfileFile::decode(bytes, &out, &error));
        QCOMPARE(out.name, QStringLiteral("DX"));
        QCOMPARE(out.keys.value(QStringLiteral("MicGain")), QStringLiteral("-12"));
        QCOMPARE(out.keys.value(QStringLiteral("FilterLow")), QStringLiteral("150"));
    }

    void aNewerFileIsRefused()
    {
        const QByteArray bytes = QByteArrayLiteral(
            "{\"nereusTxProfileVersion\":2,\"name\":\"DX\",\"keys\":{}}");
        TxProfileFile::Document out;
        QString error;
        QVERIFY(!TxProfileFile::decode(bytes, &out, &error));
        QVERIFY(error.contains(QStringLiteral("newer NereusSDR")));
    }

    void aFileThatIsNotAProfileIsRefused()
    {
        TxProfileFile::Document out;
        QString error;
        QVERIFY(!TxProfileFile::decode(QByteArrayLiteral("[]"), &out, &error));
        QCOMPARE(error, QStringLiteral("This file is not a transmit profile."));
        QVERIFY(!TxProfileFile::decode(QByteArray(TxProfileFile::kMaxBytes + 1, 'x'), &out, &error));
        QCOMPARE(error, QStringLiteral("The transmit profile is too large."));
    }

    void aSettingThatIsNotTextIsRefused()
    {
        const QByteArray bytes = QByteArrayLiteral(
            "{\"nereusTxProfileVersion\":1,\"name\":\"DX\",\"keys\":{\"MicGain\":-12}}");
        TxProfileFile::Document out;
        QString error;
        QVERIFY(!TxProfileFile::decode(bytes, &out, &error));
        QCOMPARE(error, QStringLiteral("A setting in the transmit profile is not text."));
    }

    void importFillsMissingKeysSelectsAndDropsUnknownOnes()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMac);
        mgr.load();
        TransmitModel tx;
        tx.setMicGainDb(-3);
        QHash<QString, QVariant> values;
        values.insert(QStringLiteral("MicGain"), QStringLiteral("-18"));
        values.insert(QStringLiteral("NotAProfileKey"), QStringLiteral("nope"));
        QVERIFY(mgr.importProfile(QStringLiteral(" Desk,Night "), values, &tx));
        QCOMPARE(mgr.activeProfileName(), QStringLiteral("Desk_Night"));
        QCOMPARE(tx.micGainDb(), -18);
        QCOMPARE(AppSettings::instance().value(field(QStringLiteral("Desk_Night"),
                                                     QStringLiteral("MicGain"))).toString(),
                 QStringLiteral("-18"));
        QCOMPARE(AppSettings::instance().value(field(QStringLiteral("Desk_Night"),
                                                     QStringLiteral("FilterLow"))).toString(),
                 MicProfileManager::defaultProfileValues()
                     .value(QStringLiteral("FilterLow")).toString());
        QVERIFY(!AppSettings::instance().contains(
            field(QStringLiteral("Desk_Night"), QStringLiteral("NotAProfileKey"))));
    }

    // Every field leaves in the file and comes back on another radio's
    // model, both curves and every switch included; the wiring keys are not
    // in the file and the other radio keeps its own.
    void everyFieldRoundTripsAndTheWiringStaysThisRadios()
    {
        TransmitModel source;
        setEveryField(source);
        const QHash<QString, QVariant> live = MicProfileManager::liveProfileValues(&source);
        QVERIFY(!live.value(QStringLiteral("TXParaEQData")).toString().isEmpty());
        QVERIFY(!live.value(QStringLiteral("CFCParaEQData")).toString().isEmpty());
        const QHash<QString, QVariant> portable = MicProfileManager::portableValues(live);
        for (const QString& key : MicProfileManager::radioWiringKeys()) {
            QVERIFY2(live.contains(key), qPrintable(key));
            QVERIFY2(!portable.contains(key), qPrintable(key));
        }
        QCOMPARE(portable.size(), live.size() - MicProfileManager::radioWiringKeys().size());

        QByteArray bytes;
        QString error;
        QVERIFY(TxProfileFile::encode(documentOf(QStringLiteral("Everything"), portable),
                                      &bytes, &error));
        TxProfileFile::Document decoded;
        QVERIFY2(TxProfileFile::decode(bytes, &decoded, &error), qPrintable(error));

        MicProfileManager mgr;
        mgr.setMacAddress(kMac);
        mgr.load();
        TransmitModel target;  // factory wiring: XLR, tip/ring, PC mic
        const QHash<QString, QVariant> targetWiring = MicProfileManager::liveProfileValues(&target);
        QString reason;
        QVERIFY2(mgr.importProfile(decoded.name, variants(decoded.keys), &target, &reason),
                 qPrintable(reason));
        const QHash<QString, QVariant> after = MicProfileManager::liveProfileValues(&target);
        for (auto it = portable.constBegin(); it != portable.constEnd(); ++it) {
            // Mic_LineInGain is stored but not applied: the live index
            // follows Line_Input_Level (applyValuesToModel).
            if (it.key() == QLatin1String("Mic_LineInGain")) {
                continue;
            }
            QVERIFY2(after.value(it.key()).toString() == it.value().toString(),
                     qPrintable(QStringLiteral("%1: %2 became %3").arg(
                         it.key(), it.value().toString(), after.value(it.key()).toString())));
        }
        for (auto it = portable.constBegin(); it != portable.constEnd(); ++it) {
            QCOMPARE(AppSettings::instance().value(field(QStringLiteral("Everything"), it.key()))
                         .toString(), it.value().toString());
        }
        for (const QString& key : MicProfileManager::radioWiringKeys()) {
            QVERIFY2(after.value(key) == targetWiring.value(key), qPrintable(key));
            QVERIFY2(AppSettings::instance().value(field(QStringLiteral("Everything"), key))
                         == targetWiring.value(key), qPrintable(key));
        }
    }

    // A wiring key in a file (one written by hand, or by an older build)
    // is ignored.
    void aWiringKeyInAFileIsIgnored()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMac);
        mgr.load();
        TransmitModel tx;
        tx.setMicXlr(true);
        tx.setMicPttDisabled(false);
        QHash<QString, QVariant> values;
        values.insert(QStringLiteral("MicGain"), QStringLiteral("-9"));
        values.insert(QStringLiteral("Mic_XLR"), QStringLiteral("False"));
        values.insert(QStringLiteral("Mic_PTT_Disabled"), QStringLiteral("True"));
        values.insert(QStringLiteral("Mic_Source"), QStringLiteral("Radio"));
        QVERIFY(mgr.importProfile(QStringLiteral("Wired"), values, &tx));
        QCOMPARE(tx.micGainDb(), -9);
        QVERIFY(tx.micXlr());
        QVERIFY(!tx.micPttDisabled());
        QCOMPARE(tx.micSource(), MicSource::Pc);
        QCOMPARE(MicProfileManager::unknownProfileKeys(values), QStringList{});
    }

    // An exported factory profile imports again: the checks take every
    // value the factory bank holds.
    void everyFactoryProfileImports()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMac);
        mgr.load();
        const QStringList names = mgr.profileNames();
        QVERIFY(names.size() > 1);
        for (const QString& name : names) {
            const QHash<QString, QVariant> stored = MicProfileManager::portableValues(
                MicProfileManager::readStoredProfile(kMac, name));
            QVERIFY2(!stored.isEmpty(), qPrintable(name));
            const QString refusal = MicProfileManager::importValuesRefusal(stored);
            QVERIFY2(refusal.isEmpty(), qPrintable(name + QStringLiteral(": ") + refusal));
            QVERIFY2(MicProfileManager::profileNameRefusal(name).isEmpty(), qPrintable(name));
        }
    }

    void badNamesAreRefused_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("reason");
        const QString control = QStringLiteral(
            "Use a transmit profile name without tabs, line breaks or other control characters.");
        QTest::newRow("tab") << QStringLiteral("Desk\tNight") << control;
        QTest::newRow("line break") << QStringLiteral("Desk\nNight") << control;
        QTest::newRow("bell") << (QStringLiteral("Desk") + QChar(0x07)) << control;
        QTest::newRow("delete") << (QStringLiteral("Desk") + QChar(0x7F)) << control;
        QTest::newRow("lone surrogate") << (QStringLiteral("Desk") + QChar(0xD800)) << control;
        QTest::newRow("not a character") << (QStringLiteral("Desk") + QChar(0xFFFE)) << control;
        QTest::newRow("too long") << QString(65, QLatin1Char('x'))
            << QStringLiteral("Use a transmit profile name of 64 characters or fewer.");
        QTest::newRow("blank") << QStringLiteral("   ")
            << QStringLiteral("Give the transmit profile a name.");
        // The settings file reads either back as another name ("B", and
        // "D-104+CPDR").
        const QString underscores =
            QStringLiteral("A transmit profile name cannot hold these double underscores. "
                           "Use single underscores instead.");
        QTest::newRow("escaped letter") << QStringLiteral("__u0042__") << underscores;
        QTest::newRow("escaped word") << QStringLiteral("D-104__plus__CPDR") << underscores;
        // Meets the "/" after it: "Rig__c__s__MicGain" reads "Rig:s__MicGain".
        QTest::newRow("ends in a short code") << QStringLiteral("Rig__c") << underscores;
        QTest::newRow("commas made underscores") << QStringLiteral("Rig,,c") << underscores;
    }

    // Save and import share the name rule, and so does a file's name.
    void badNamesAreRefused()
    {
        QFETCH(QString, name);
        QFETCH(QString, reason);
        QCOMPARE(MicProfileManager::profileNameRefusal(
                     MicProfileManager::normalizedProfileName(name)), reason);
        QVERIFY(MicProfileManager::profileNameRefusal(QString(64, QLatin1Char('x'))).isEmpty());
        QVERIFY(MicProfileManager::profileNameRefusal(QStringLiteral("D-104+CPDR")).isEmpty());

        MicProfileManager mgr;
        mgr.setMacAddress(kMac);
        mgr.load();
        TransmitModel tx;
        const QStringList before = mgr.profileNames();
        QHash<QString, QVariant> values;
        values.insert(QStringLiteral("MicGain"), QStringLiteral("-9"));
        QString shown;
        QVERIFY(!mgr.importProfile(name, values, &tx, &shown));
        QCOMPARE(shown, reason);
        QVERIFY(!mgr.saveProfile(name, &tx));
        QCOMPARE(mgr.profileNames(), before);

        if (!name.trimmed().isEmpty()) {
            QJsonObject root{{QStringLiteral("nereusTxProfileVersion"), TxProfileFile::kVersion},
                             {QStringLiteral("name"), name},
                             {QStringLiteral("keys"),
                              QJsonObject{{QStringLiteral("MicGain"), QStringLiteral("-9")}}}};
            TxProfileFile::Document out;
            QString error;
            QVERIFY(!TxProfileFile::decode(QJsonDocument(root).toJson(), &out, &error));
            QCOMPARE(error, reason);
        }
    }

    void aNameSavesAndReloads_data()
    {
        QTest::addColumn<QString>("name");
        for (const char* name : {"Desk_Night", "plan c", "c", "Rig s", "A lb B", "Heil plus gate",
                                 "_c_", "My__Profile", "a__b"}) {
            QTest::newRow(name) << QString::fromLatin1(name);
        }
    }

    // A name survives the settings file: saved, written to disk, read back,
    // and the same profile again.
    void aNameSavesAndReloads()
    {
        QFETCH(QString, name);
        QVERIFY(MicProfileManager::profileNameRefusal(name).isEmpty());
        {
            MicProfileManager mgr;
            mgr.setMacAddress(kMac);
            mgr.load();
            TransmitModel tx;
            tx.setMicGainDb(-21);
            QVERIFY(mgr.saveProfile(name, &tx));
        }
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("NereusSDR.settings"));
        const QStringList keys = AppSettings::instance().allKeys();
        {
            AppSettings disk(path);
            for (const QString& key : keys) {
                disk.setValue(key, AppSettings::instance().value(key));
            }
            QString error;
            QVERIFY2(disk.save(&error), qPrintable(error));
        }
        AppSettings back(path);
        back.load();
        QStringList reloaded = back.allKeys();
        QStringList expected = keys;
        reloaded.sort();
        expected.sort();
        QCOMPARE(reloaded, expected);
        QCOMPARE(back.value(field(name, QStringLiteral("MicGain"))).toString(),
                 QStringLiteral("-21"));

        // The reloaded file, as this computer's settings again.
        AppSettings::instance().clear();
        for (const QString& key : reloaded) {
            AppSettings::instance().setValue(key, back.value(key));
        }
        MicProfileManager mgr;
        mgr.setMacAddress(kMac);
        mgr.load();
        QVERIFY(mgr.profileNames().contains(name));
        TransmitModel tx;
        QVERIFY(mgr.setActiveProfile(name, &tx));
        QCOMPARE(tx.micGainDb(), -21);
    }

    // Two commas become two underscores, which most names may hold.
    void twoCommasAreTaken()
    {
        const QString name = MicProfileManager::normalizedProfileName(QStringLiteral("a,,b"));
        QCOMPARE(name, QStringLiteral("a__b"));
        QVERIFY(MicProfileManager::profileNameRefusal(name).isEmpty());
    }

    // A profile an earlier build saved under a name with double
    // underscores, refused now or not, can still be selected and deleted.
    void anExistingDoubleUnderscoreProfileSelectsAndDeletes()
    {
        MicProfileManager mgr;
        mgr.setMacAddress(kMac);
        mgr.load();
        auto& settings = AppSettings::instance();
        const QString manifest = QStringLiteral("hardware/%1/tx/profile/_names").arg(kMac);
        const QStringList names{QStringLiteral("My__Profile"), QStringLiteral("Rig__c")};
        for (const QString& name : names) {
            settings.setValue(manifest, settings.value(manifest).toString()
                                            + QLatin1Char(',') + name);
            settings.setValue(field(name, QStringLiteral("MicGain")), QStringLiteral("-23"));
        }
        for (const QString& name : names) {
            TransmitModel tx;
            QVERIFY2(mgr.setActiveProfile(name, &tx), qPrintable(name));
            QCOMPARE(mgr.activeProfileName(), name);
            QCOMPARE(tx.micGainDb(), -23);
            QVERIFY2(mgr.deleteProfile(name), qPrintable(name));
            QVERIFY(!mgr.profileNames().contains(name));
            QVERIFY(!settings.contains(field(name, QStringLiteral("MicGain"))));
        }
    }

    void badValuesAreRefusedNamingTheSetting_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QString>("value");
        QTest::addColumn<QString>("reason");
        QTest::newRow("not a number") << QStringLiteral("MicGain") << QStringLiteral("loud")
            << QStringLiteral("Choose a mic level from -50 to 70 dB.");
        QTest::newRow("out of range") << QStringLiteral("CompanderLevel") << QStringLiteral("99")
            << QStringLiteral("Choose a PROC level from 0 to 20 dB.");
        QTest::newRow("switch") << QStringLiteral("Lev_On") << QStringLiteral("yes")
            << QStringLiteral("The transmit profile's leveler setting is not on or off.");
        QTest::newRow("band") << QStringLiteral("TXEQ3") << QStringLiteral("40")
            << QStringLiteral("Choose ten TX EQ band levels, each from -12 to 15 dB.");
        QTest::newRow("filter low") << QStringLiteral("FilterLow") << QStringLiteral("6000")
            << QStringLiteral("Choose a TX filter low cut from 0 to 5000 Hz.");
        QTest::newRow("filter high") << QStringLiteral("FilterHigh") << QStringLiteral("50")
            << QStringLiteral("Choose a TX filter high cut from 200 to 10000 Hz.");
        QTest::newRow("filter order") << QStringLiteral("FilterLow") << QStringLiteral("3000")
            << QStringLiteral("Choose a TX filter low cut below its high cut.");
        QTest::newRow("TX EQ curve") << QStringLiteral("TXParaEQData") << QStringLiteral("zzz")
            << QStringLiteral("The Core could not read that equalizer curve. Save the curve again and retry.");
        QTest::newRow("CFC curve") << QStringLiteral("CFCParaEQData") << QStringLiteral("zzz")
            << QStringLiteral("The Core could not read that equalizer curve. Save the curve again and retry.");
        QTest::newRow("VOX gain") << QStringLiteral("VOX_GainScalar") << QStringLiteral("nan")
            << QStringLiteral("Choose a VOX gain from 0 to 100.");
        QTest::newRow("line in gain") << QStringLiteral("Mic_LineInGain") << QStringLiteral("40")
            << QStringLiteral("Choose a line in gain step from 0 to 31.");
    }

    // A value its setting would not take refuses the whole import, in the
    // words the link uses for that setting, and nothing is stored.
    void badValuesAreRefusedNamingTheSetting()
    {
        QFETCH(QString, key);
        QFETCH(QString, value);
        QFETCH(QString, reason);
        QHash<QString, QVariant> values;
        values.insert(QStringLiteral("MicGain"), QStringLiteral("-9"));
        values.insert(key, value);
        QCOMPARE(MicProfileManager::importValuesRefusal(values), reason);

        MicProfileManager mgr;
        mgr.setMacAddress(kMac);
        mgr.load();
        TransmitModel tx;
        QString shown;
        QVERIFY(!mgr.importProfile(QStringLiteral("Bad"), values, &tx, &shown));
        QCOMPARE(shown, reason);
        QVERIFY(!mgr.profileNames().contains(QStringLiteral("Bad")));
        QVERIFY(!AppSettings::instance().contains(field(QStringLiteral("Bad"), QStringLiteral("MicGain"))));
    }

    // A file with no setting this build keeps imports nothing, instead of
    // the factory defaults over a profile.
    void aFileWithNoSettingsIsRefused()
    {
        TxProfileFile::Document out;
        QString error;
        QVERIFY(!TxProfileFile::decode(QByteArrayLiteral(
            "{\"nereusTxProfileVersion\":1,\"name\":\"DX\",\"keys\":{}}"), &out, &error));
        QCOMPARE(error, QStringLiteral("The transmit profile has no settings."));

        MicProfileManager mgr;
        mgr.setMacAddress(kMac);
        mgr.load();
        TransmitModel tx;
        QHash<QString, QVariant> values;
        values.insert(QStringLiteral("SomethingNewer"), QStringLiteral("1"));
        values.insert(QStringLiteral("Mic_XLR"), QStringLiteral("True"));
        QCOMPARE(MicProfileManager::unknownProfileKeys(values),
                 QStringList{QStringLiteral("SomethingNewer")});
        QString shown;
        QVERIFY(!mgr.importProfile(QStringLiteral("Default"), values, &tx, &shown));
        QCOMPARE(shown, QStringLiteral("The transmit profile has no settings."));
    }

    // Every refusal import and the file can give is plain words.
    void refusalsArePlainWords()
    {
        for (const QString& reason :
             {QStringLiteral("A setting in the transmit profile is not text."),
              QStringLiteral("The transmit profile has no settings."),
              QStringLiteral("The transmit profile was not understood."),
              QStringLiteral("The transmit profile is too large."),
              QStringLiteral("This file is not a transmit profile."),
              QStringLiteral("This transmit profile is from a newer NereusSDR. "
                             "Update this app to import it."),
              QStringLiteral("Give the transmit profile a name."),
              QStringLiteral("Use a transmit profile name of 64 characters or fewer."),
              QStringLiteral("Use a transmit profile name without tabs, line breaks or other "
                             "control characters."),
              QStringLiteral("A transmit profile name cannot hold these double underscores. "
                             "Use single underscores instead."),
              QStringLiteral("The active transmit profile changed while you were choosing the "
                             "file. Nothing was saved or exported."),
              QStringLiteral("Another export is still waiting for the Core. Try again when it "
                             "has finished."),
              QStringLiteral("Connect a radio to import a transmit profile."),
              QStringLiteral("The transmit profile was stored but could not be selected."),
              QStringLiteral("The transmit profile's leveler setting is not on or off."),
              QStringLiteral("Choose a line in gain step from 0 to 31."),
              QStringLiteral("Choose a VOX gain from 0 to 100."),
              QStringLiteral("Choose a TX filter low cut from 0 to 5000 Hz."),
              QStringLiteral("Choose a TX filter high cut from 200 to 10000 Hz."),
              QStringLiteral("Choose a TX filter low cut below its high cut.")}) {
            QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        }
    }

    void importWithoutARadioDoesNothing()
    {
        MicProfileManager mgr;
        TransmitModel tx;
        QVERIFY(!mgr.importProfile(QStringLiteral("Desk"), {}, &tx));
        QVERIFY(mgr.profileNames().isEmpty());
    }
};

QTEST_MAIN(TstTxProfileFile)
#include "tst_tx_profile_file.moc"
