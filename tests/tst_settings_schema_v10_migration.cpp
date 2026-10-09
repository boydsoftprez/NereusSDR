// =================================================================
// tests/tst_settings_schema_v10_migration.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// SettingsSchemaVersion v9 -> v10 makes two-tone one station setting, as in
// Thetis, once per settings file:
//   - every hardware/<mac>/tx/profile/<name>/<two-tone key> is removed (the
//     eight keys TX profiles carried before two-tone left them);
//   - a live hardware/<mac>/tx/TwoToneLevel saved as exactly -6 (the retired
//     default) becomes 0, Thetis's default; any other level is kept;
//   - SettingsSchemaVersion ends at 10.
// =================================================================
// Modification history (NereusSDR):
//   2026-10-09: original test for NereusSDR by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"

using namespace NereusSDR;

namespace {
const QString kVersionKey = QStringLiteral("SettingsSchemaVersion");
const QString kMacA = QStringLiteral("00:1c:c0:a2:13:dd");
const QString kMacB = QStringLiteral("aa:bb:cc:dd:ee:01");
const QString kMacC = QStringLiteral("aa:bb:cc:dd:ee:02");
const QString kMacD = QStringLiteral("aa:bb:cc:dd:ee:03");

const QStringList kTwoToneSuffixes = {
    QStringLiteral("TwoToneFreq1"),      QStringLiteral("TwoToneFreq2"),
    QStringLiteral("TwoToneLevel"),      QStringLiteral("TwoTonePower"),
    QStringLiteral("TwoToneFreq2Delay"), QStringLiteral("TwoToneInvert"),
    QStringLiteral("TwoTonePulsed"),     QStringLiteral("TwoToneDrivePowerOrigin"),
};

QString txKey(const QString& mac, const QString& key)
{
    return QStringLiteral("hardware/%1/tx/%2").arg(mac, key);
}

QString profileKey(const QString& mac, const QString& profile, const QString& key)
{
    return QStringLiteral("hardware/%1/tx/profile/%2/%3").arg(mac, profile, key);
}

void clearAll()
{
    auto& s = AppSettings::instance();
    for (const QString& key : s.allKeys()) {
        if (key.startsWith(QLatin1String("hardware/"))) {
            s.remove(key);
        }
    }
    s.remove(kVersionKey);
}
} // namespace

class TestSettingsSchemaV10Migration : public QObject {
    Q_OBJECT
private slots:
    void init() { clearAll(); }
    void cleanup() { clearAll(); }

    void v9_to_v10_removes_two_tone_keys_from_every_profile()
    {
        auto& s = AppSettings::instance();
        s.setValue(kVersionKey, QStringLiteral("9"));
        const QStringList macs = {kMacA, kMacB};
        // "Old Saved" stands for a profile saved before two-tone left
        // profiles: deleting it walks only today's key list, so without the
        // upgrade its two-tone keys would be stranded.
        const QStringList profiles = {QStringLiteral("Default"), QStringLiteral("Old Saved")};
        for (const QString& mac : macs) {
            for (const QString& profile : profiles) {
                for (const QString& suffix : kTwoToneSuffixes) {
                    s.setValue(profileKey(mac, profile, suffix), QStringLiteral("-6"));
                }
                s.setValue(profileKey(mac, profile, QStringLiteral("MicGain")),
                           QStringLiteral("-6"));
                s.setValue(profileKey(mac, profile, QStringLiteral("CompanderLevel")),
                           QStringLiteral("3"));
                // A key sharing the prefix but not one of the eight stays.
                s.setValue(profileKey(mac, profile, QStringLiteral("TwoToneFuture")),
                           QStringLiteral("1"));
            }
        }
        // The live keys are not profile keys: they stay (Level aside).
        s.setValue(txKey(kMacA, QStringLiteral("TwoToneFreq1")), QStringLiteral("700"));
        s.setValue(txKey(kMacA, QStringLiteral("TwoToneFreq2")), QStringLiteral("1900"));

        s.ensureSettingsAtVersion(10);

        for (const QString& mac : macs) {
            for (const QString& profile : profiles) {
                for (const QString& suffix : kTwoToneSuffixes) {
                    QVERIFY2(!s.contains(profileKey(mac, profile, suffix)),
                             qPrintable(profileKey(mac, profile, suffix)));
                }
                QCOMPARE(s.value(profileKey(mac, profile, QStringLiteral("MicGain"))).toString(),
                         QStringLiteral("-6"));
                QCOMPARE(s.value(profileKey(mac, profile, QStringLiteral("CompanderLevel")))
                             .toString(),
                         QStringLiteral("3"));
                QCOMPARE(s.value(profileKey(mac, profile, QStringLiteral("TwoToneFuture")))
                             .toString(),
                         QStringLiteral("1"));
            }
        }
        QCOMPARE(s.value(txKey(kMacA, QStringLiteral("TwoToneFreq1"))).toString(),
                 QStringLiteral("700"));
        QCOMPARE(s.value(txKey(kMacA, QStringLiteral("TwoToneFreq2"))).toString(),
                 QStringLiteral("1900"));
        QCOMPARE(s.value(kVersionKey).toString(), QStringLiteral("10"));
    }

    void v9_to_v10_resets_only_a_live_level_of_minus_six()
    {
        auto& s = AppSettings::instance();
        s.setValue(kVersionKey, QStringLiteral("9"));
        s.setValue(txKey(kMacA, QStringLiteral("TwoToneLevel")), QStringLiteral("-6"));
        s.setValue(txKey(kMacB, QStringLiteral("TwoToneLevel")), QStringLiteral("-6.0"));
        s.setValue(txKey(kMacC, QStringLiteral("TwoToneLevel")), QStringLiteral("-12"));
        s.setValue(txKey(kMacD, QStringLiteral("TwoToneLevel")), QStringLiteral("0"));

        s.ensureSettingsAtVersion(10);

        // "0" is what TransmitModel::persistOne writes for a level of 0.
        QCOMPARE(s.value(txKey(kMacA, QStringLiteral("TwoToneLevel"))).toString(),
                 QString::number(0.0));
        QCOMPARE(s.value(txKey(kMacB, QStringLiteral("TwoToneLevel"))).toString(),
                 QStringLiteral("0"));
        QCOMPARE(s.value(txKey(kMacC, QStringLiteral("TwoToneLevel"))).toString(),
                 QStringLiteral("-12"));
        QCOMPARE(s.value(txKey(kMacD, QStringLiteral("TwoToneLevel"))).toString(),
                 QStringLiteral("0"));
        QCOMPARE(s.value(kVersionKey).toString(), QStringLiteral("10"));
    }

    void a_file_already_at_v10_is_not_touched()
    {
        // It runs once: -6 chosen again, or a profile key written, after the
        // upgrade stays.
        auto& s = AppSettings::instance();
        s.setValue(kVersionKey, QStringLiteral("10"));
        s.setValue(txKey(kMacA, QStringLiteral("TwoToneLevel")), QStringLiteral("-6"));
        s.setValue(profileKey(kMacA, QStringLiteral("Default"), QStringLiteral("TwoToneLevel")),
                   QStringLiteral("-6"));

        s.ensureSettingsAtVersion(10);

        QCOMPARE(s.value(txKey(kMacA, QStringLiteral("TwoToneLevel"))).toString(),
                 QStringLiteral("-6"));
        QCOMPARE(s.value(profileKey(kMacA, QStringLiteral("Default"),
                                    QStringLiteral("TwoToneLevel")))
                     .toString(),
                 QStringLiteral("-6"));
        QCOMPARE(s.value(kVersionKey).toString(), QStringLiteral("10"));
    }
};

QTEST_MAIN(TestSettingsSchemaV10Migration)
#include "tst_settings_schema_v10_migration.moc"
