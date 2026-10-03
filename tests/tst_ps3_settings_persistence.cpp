// no-port-check: NereusSDR-original acceptance tests for validated PS3
// configuration persistence and non-actuating hydration.

#include <QtTest>
#include <QRegularExpression>

#include "core/AppSettings.h"
#include "models/PureSignalSettings.h"

#include <limits>

using namespace NereusSDR;

namespace {
const QString radioA = QStringLiteral("00:1C:2D:03:04:05");
const QString radioB = QStringLiteral("00:1C:2D:03:04:06");
}

class TestPs3SettingsPersistence : public QObject {
    Q_OBJECT
private slots:
    void init() { AppSettings::instance().clear(); }
    void defaultsMatchPs3Contract();
    void allAcceptedValuesRoundTripPerRadio();
    void invalidEditRejectsWholeCandidate();
    void corruptFieldsFallBackIndependentlyAndRemainVisible();
    void loadingDesiredAutomaticIntentEmitsNoConfigurationWrite();
    void legacyAutoCalKeyIsStillRead();
};

void TestPs3SettingsPersistence::defaultsMatchPs3Contract()
{
    PureSignalSettings settings;
    QCOMPARE(settings.autoCalEnabled(), false);
    QCOMPARE(settings.runCalibrationProcessing(), true);
    QCOMPARE(settings.autoAttenuate(), true);
    QCOMPARE(settings.quickAttenuate(), false);
    // Fix wave RD-I7: Thetis udPSMoxDelay Value 0.2
    // (PSForm.Designer.cs:368-372 [v2.10.3.15]).
    QCOMPARE(settings.moxDelaySeconds(), 0.2);
    QCOMPARE(settings.loopDelaySeconds(), 0.0);
    QCOMPARE(settings.requestedTxDelayNs(), 150.0);
    QCOMPARE(settings.hardwarePeakOverrideEnabled(), false);
    QCOMPARE(settings.hardwarePeakOverride(), 0.0);
}

void TestPs3SettingsPersistence::allAcceptedValuesRoundTripPerRadio()
{
    PureSignalSettings settings;
    settings.setRadioIdentity(radioA.toLower());
    PureSignalSettingsValues requested;
    requested.autoCalEnabled = true;
    requested.runCalibrationProcessing = false;
    requested.autoAttenuate = false;
    requested.quickAttenuate = true;
    requested.moxDelaySeconds = 0.7;
    requested.loopDelaySeconds = 17.25;
    requested.requestedTxDelayNs = 123456.5;
    requested.hardwarePeakOverrideEnabled = true;
    requested.hardwarePeakOverride = 0.4125;
    QVERIFY(settings.apply(requested));
    QVERIFY(settings.save());
    auto& app = AppSettings::instance();
    QVERIFY(app.save());
    app.clear();
    app.load();

    PureSignalSettings other;
    QVERIFY(other.load(radioB));
    QVERIFY(other.values() == PureSignalSettingsValues{});

    PureSignalSettings restored;
    QVERIFY(restored.load(radioA));
    QVERIFY(restored.values() == requested);
    QCOMPARE(restored.settingsPrefix(),
             QStringLiteral("hardware/00:1C:2D:03:04:05/pureSignal/"));
}

void TestPs3SettingsPersistence::invalidEditRejectsWholeCandidate()
{
    PureSignalSettings settings;
    PureSignalSettingsValues accepted;
    accepted.loopDelaySeconds = 9.0;
    QVERIFY(settings.apply(accepted));

    QSignalSpy changed(&settings, &PureSignalSettings::configurationChanged);
    QSignalSpy rejected(&settings, &PureSignalSettings::editRejected);
    auto invalid = accepted;
    invalid.autoAttenuate = false;
    invalid.moxDelaySeconds = 0.09;
    QVERIFY(!settings.apply(invalid));
    QVERIFY(settings.values() == accepted);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(rejected.count(), 1);
    // Fix wave RD-I7: Thetis udPSMoxDelay Maximum 1.0.
    invalid.moxDelaySeconds = 1.1;
    QVERIFY(!settings.apply(invalid));
    QCOMPARE(rejected.count(), 2);
    auto atMaximum = accepted;
    atMaximum.moxDelaySeconds = 1.0;
    QVERIFY(settings.apply(atMaximum));
    QVERIFY(settings.apply(accepted));

    settings.setRequestedTxDelayNs(std::numeric_limits<double>::infinity());
    settings.setLoopDelaySeconds(100.01);
    settings.setHardwarePeakOverride(-1.0);
    QVERIFY(settings.values() == accepted);
}

void TestPs3SettingsPersistence::corruptFieldsFallBackIndependentlyAndRemainVisible()
{
    PureSignalSettings settings;
    settings.setRadioIdentity(radioA);
    auto& app = AppSettings::instance();
    const QString prefix = settings.settingsPrefix();
    app.setValue(prefix + QStringLiteral("MoxDelaySeconds"), 0.01);
    app.setValue(prefix + QStringLiteral("LoopDelaySeconds"), 12.5);
    app.setValue(prefix + QStringLiteral("RequestedTxDelayNs"), QStringLiteral("bogus"));
    app.setValue(prefix + QStringLiteral("HardwarePeakOverride"), 0.37);
    app.setValue(prefix + QStringLiteral("HardwarePeakOverrideEnabled"), true);
    // Which settings fell back goes to the log; the notice an app may show
    // as sent is plain words (iPhone app Part A fix wave, R-IOS-01).
    QTest::ignoreMessage(QtInfoMsg, QRegularExpression(QStringLiteral(
        "Saved PureSignal settings not used.*MoxDelaySeconds.*RequestedTxDelayNs")));
    QVERIFY(settings.load());

    QCOMPARE(settings.moxDelaySeconds(), 0.2);
    QCOMPARE(settings.loopDelaySeconds(), 12.5);
    QCOMPARE(settings.requestedTxDelayNs(), 150.0);
    QCOMPARE(settings.hardwarePeakOverrideEnabled(), true);
    QCOMPARE(settings.hardwarePeakOverride(), 0.37);
    QCOMPARE(settings.lastLoadError(),
             QStringLiteral("Some saved PureSignal settings could not be used, so their "
                            "defaults are in use."));
}

void TestPs3SettingsPersistence::loadingDesiredAutomaticIntentEmitsNoConfigurationWrite()
{
    PureSignalSettings settings;
    settings.setRadioIdentity(radioA);
    AppSettings::instance().setValue(
        settings.settingsPrefix() + QStringLiteral("autoCalEnabled"), true);
    QSignalSpy writes(&settings, &PureSignalSettings::configurationChanged);
    QVERIFY(settings.load());
    QCOMPARE(settings.autoCalEnabled(), true);
    QCOMPARE(writes.count(), 0);
}

// Fix wave minor: a preference saved under the old camelCase leaf, as a
// bool, is still read; the PascalCase leaf wins once it exists.
void TestPs3SettingsPersistence::legacyAutoCalKeyIsStillRead()
{
    PureSignalSettings settings;
    settings.setRadioIdentity(radioA);
    auto& app = AppSettings::instance();
    const QString prefix = settings.settingsPrefix();
    app.setValue(prefix + QStringLiteral("autoCalEnabled"), QStringLiteral("true"));
    QVERIFY(settings.load());
    QCOMPARE(settings.autoCalEnabled(), true);
    QVERIFY(settings.lastLoadError().isEmpty());

    app.setValue(prefix + QStringLiteral("AutoCalEnabled"), QStringLiteral("False"));
    QVERIFY(settings.load());
    QCOMPARE(settings.autoCalEnabled(), false);
}

QTEST_APPLESS_MAIN(TestPs3SettingsPersistence)
#include "tst_ps3_settings_persistence.moc"
