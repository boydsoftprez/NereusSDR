// no-port-check: NereusSDR-original acceptance tests for NNR configuration.
#include <QRegularExpression>
#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {
const QString radioA = QStringLiteral("00:1C:2D:03:04:05");
const QString radioB = QStringLiteral("00:1C:2D:03:04:06");

NnrSettings nonDefault()
{
    return {1, -31.25, NrPosition::PreAgc, 1.75, 12.5, 2.75, 13.5, 17.25, 83.5};
}
}

class TestNnrSettings : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        AppSettings::instance().clear();
    }

    void allValuesPersistWhileDisabledAndRemainIsolated()
    {
        SliceModel a;
        a.setSliceIndex(3);
        a.setSettingsRadioIdentity(radioA.toLower());
        QVERIFY(a.applyNnrSettings(nonDefault()));
        QCOMPARE(a.activeNr(), NrSlot::Off);
        a.saveNnrSettings();

        SliceModel b;
        b.setSliceIndex(7);
        b.setSettingsRadioIdentity(radioA);
        b.setNnrMaskFloorDb(-22.5);
        b.setNnrReleaseMs(10.5);
        b.setActiveNr(NrSlot::NNR);
        b.saveNnrSettings();

        SliceModel otherRadio;
        otherRadio.setSliceIndex(3);
        otherRadio.setSettingsRadioIdentity(radioB);
        otherRadio.setNnrAlpha(2.5);
        otherRadio.setActiveNr(NrSlot::NR4);
        otherRadio.saveNnrSettings();

        auto& settings = AppSettings::instance();
        settings.save();
        settings.clear();
        settings.load();

        SliceModel restored;
        restored.setSliceIndex(3);
        restored.setSettingsRadioIdentity(radioA);
        restored.restoreNnrSettings();
        QVERIFY(restored.nnrSettings() == nonDefault());
        QCOMPARE(restored.activeNr(), NrSlot::Off);

        restored.setSettingsRadioIdentity(radioB);
        restored.restoreNnrSettings();
        QCOMPARE(restored.nnrAlpha(), 2.5);
        QCOMPARE(restored.nnrMaskFloorDb(), -25.0);
        QCOMPARE(restored.activeNr(), NrSlot::NR4);

        restored.setSettingsRadioIdentity(radioA);
        restored.setSliceIndex(7);
        restored.restoreNnrSettings();
        QCOMPARE(restored.nnrMaskFloorDb(), -22.5);
        QCOMPARE(restored.nnrReleaseMs(), 10.5);
        QCOMPARE(restored.activeNr(), NrSlot::NNR);
    }

    void switchingAwayFromNnrAndResetSurviveReload()
    {
        SliceModel slice;
        slice.setSettingsRadioIdentity(radioA);
        QVERIFY(slice.applyNnrSettings(nonDefault()));
        slice.setActiveNr(NrSlot::NNR);
        slice.setActiveNr(NrSlot::NR2);
        slice.resetNnrTuning();
        QCOMPARE(slice.nnrModelSlot(), 1); // Reset preserves model selection.
        QCOMPARE(slice.activeNr(), NrSlot::NR2);
        slice.saveNnrSettings();

        SliceModel restored;
        restored.setSettingsRadioIdentity(radioA);
        restored.restoreNnrSettings();
        NnrSettings expected;
        expected.modelSlot = 1;
        QVERIFY(restored.nnrSettings() == expected);
        QCOMPARE(restored.activeNr(), NrSlot::NR2);
    }

    void refusedEditCannotChangeOrPersistAnyField()
    {
        SliceModel slice;
        slice.setSettingsRadioIdentity(radioA);
        slice.setNnrSettingsApplier([](const NnrSettings&, QString* reason) -> std::optional<NnrSettings> {
            *reason = QStringLiteral("Premium model unavailable");
            return std::nullopt;
        });
        QSignalSpy changed(&slice, &SliceModel::nnrConfigurationChanged);
        QVERIFY(!slice.applyNnrSettings(nonDefault()));
        QVERIFY(slice.nnrSettings() == NnrSettings{});
        QCOMPARE(changed.count(), 0);
        QVERIFY(slice.nnrLastError().contains(QLatin1String("unavailable")));
        slice.saveNnrSettings();
        QCOMPARE(AppSettings::instance().value(slice.nnrSettingsPrefix() + "NnrModelSlot").toInt(), 0);
        QCOMPARE(AppSettings::instance().value(slice.nnrSettingsPrefix() + "NnrMaskFloorDb").toDouble(), -25.0);
    }

    void recordsAcceptedReadbackInsteadOfRequestedValue()
    {
        SliceModel slice;
        slice.setNnrSettingsApplier([](const NnrSettings& requested, QString*) {
            auto accepted = requested;
            accepted.maskFloorDb = -30.0;
            return std::optional<NnrSettings>{accepted};
        });
        slice.setNnrMaskFloorDb(-31.25);
        QCOMPARE(slice.nnrMaskFloorDb(), -30.0);
    }

    void invalidValuesAndUnknownEnumAreRejected()
    {
        SliceModel slice;
        slice.setNnrAlpha(std::numeric_limits<double>::quiet_NaN());
        slice.setNnrMaskFloorDb(-51.0);
        slice.setNnrTauSeconds(std::numeric_limits<double>::infinity());
        slice.setNnrModelSlot(2);
        slice.setNnrPosition(static_cast<NrPosition>(8));
        slice.setNnrAttackMs(-1.0);
        slice.setNnrReleaseMs(501.0);
        slice.setActiveNr(static_cast<NrSlot>(9));
        QVERIFY(slice.nnrSettings() == NnrSettings{});
        QCOMPARE(slice.activeNr(), NrSlot::Off);
    }

    void diagnosticModesAreNeverPersisted()
    {
        SliceModel slice;
        slice.setSettingsRadioIdentity(radioA);
        NnrDiagnostics diagnostics;
        diagnostics.testMode = 2;
        diagnostics.outputMode = 0;
        slice.updateNnrDiagnostics(diagnostics);
        slice.saveNnrSettings();
        SliceModel restored;
        restored.setSettingsRadioIdentity(radioA);
        restored.restoreNnrSettings();
        QCOMPARE(restored.nnrTestMode(), 0);
        QCOMPARE(restored.nnrOutputMode(), 1);
        for (const auto& key : AppSettings::instance().allKeys()) {
            QVERIFY(!key.contains("TestMode"));
            QVERIFY(!key.contains("OutputMode"));
        }
    }

    void corruptFieldDoesNotDiscardIndependentGoodFields()
    {
        SliceModel slice;
        slice.setSettingsRadioIdentity(radioA);
        auto& settings = AppSettings::instance();
        settings.setValue(slice.nnrSettingsPrefix() + "NnrAlpha", "not-a-number");
        settings.setValue(slice.nnrSettingsPrefix() + "NnrReleaseMs", 47.25);
        // Which settings fell back goes to the log; the notice an app may
        // show as sent is plain words (iPhone app Part A fix wave).
        QTest::ignoreMessage(QtInfoMsg,
                             QRegularExpression(QStringLiteral("Saved NNR settings not used.*NnrAlpha")));
        slice.restoreNnrSettings();
        QCOMPARE(slice.nnrAlpha(), 1.0);
        QCOMPARE(slice.nnrReleaseMs(), 47.25);
        QCOMPARE(slice.nnrLastError(),
                 QStringLiteral("Some saved noise reduction settings could not be used, so "
                                "their defaults are in use."));
    }

    void migrationUsesLoadedLastOwnerAndPreservesLegacy()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("settings.xml");
        AppSettings settings(path);
        settings.setLastConnected(radioA.toLower());
        settings.setValue("Slice3/NrActive", 4);
        settings.setValue("Slice7/NrActive", 5);
        settings.setValue("Slice8/NrActive", 123);
        settings.save();

        AppSettings restored(path);
        restored.load(); // Migration precedes any new connection identity.
        const QString aPrefix = "hardware/" + radioA + "/";
        const QString bPrefix = "hardware/" + radioB + "/";
        QCOMPARE(restored.value(aPrefix + "slices/3/nnr/NrActive").toInt(), 4);
        QCOMPARE(restored.value(aPrefix + "slices/7/nnr/NrActive").toInt(), 5);
        QVERIFY(!restored.contains(aPrefix + "slices/8/nnr/NrActive"));
        QCOMPARE(restored.value("Slice3/NrActive").toInt(), 4);
        QVERIFY(!restored.contains(bPrefix + "slices/3/nnr/NrActive"));
        restored.setValue("Slice3/NrActive", 2);
        restored.migrateLegacyNnrSettings();
        QCOMPARE(restored.value(aPrefix + "slices/3/nnr/NrActive").toInt(), 4);
    }

    void missingOwnerAndExistingNamespaceDoNotMigrate()
    {
        QTemporaryDir directory;
        AppSettings settings(directory.filePath("settings.xml"));
        settings.setValue("Slice0/NrActive", 3);
        settings.migrateLegacyNnrSettings();
        QCOMPARE(settings.allKeys().size(), 1);
        settings.setLastConnected(radioA);
        const QString prefix = "hardware/" + radioA + "/slices/0/nnr/";
        settings.setValue(prefix + "NnrAlpha", 2.0);
        settings.migrateLegacyNnrSettings();
        QVERIFY(!settings.contains(prefix + "NrActive"));
        QCOMPARE(settings.value(prefix + "NnrAlpha").toDouble(), 2.0);
    }

    // R-R3-40: the runtime NNR limit is a slice property that is never saved,
    // never changes the saved choice, and says why in plain words.
    void runtimeLimitIsNeverSavedAndExplainsItself()
    {
        SliceModel slice;
        slice.setSliceIndex(2);
        slice.setSettingsRadioIdentity(radioA);
        slice.setNnrModelSlot(1);
        slice.setActiveNr(NrSlot::NNR);
        QCOMPARE(slice.nnrLimit(), 0);
        QVERIFY(slice.nnrLimitText().isEmpty());

        QSignalSpy changed(&slice, &SliceModel::nnrLimitChanged);
        slice.setNnrLimit(static_cast<int>(NnrLimit::StandardOnly));
        slice.setNnrLimit(static_cast<int>(NnrLimit::StandardOnly));
        slice.setNnrLimit(3);    // not a limit
        slice.setNnrLimit(-1);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(slice.nnrLimit(), 1);
        QCOMPARE(slice.nnrLimitText(),
                 QStringLiteral("Noise reduction is using the Standard model. "
                                "This computer could not keep up with Premium."));
        slice.setNnrLimit(static_cast<int>(NnrLimit::Off));
        QCOMPARE(slice.nnrLimitText(),
                 QStringLiteral("Noise reduction was turned off. This computer could not keep up."));
        QCOMPARE(slice.nnrModelSlot(), 1);
        QCOMPARE(slice.activeNr(), NrSlot::NNR);

        slice.saveNnrSettings();
        const QString prefix = slice.nnrSettingsPrefix();
        for (const QString& key : AppSettings::instance().allKeys()) {
            QVERIFY2(!(key.startsWith(prefix) && key.contains(QStringLiteral("Limit"))),
                     qPrintable(key));
        }
        SliceModel restored;
        restored.setSliceIndex(2);
        restored.setSettingsRadioIdentity(radioA);
        restored.restoreNnrSettings();
        QCOMPARE(restored.nnrLimit(), 0);
        QCOMPARE(restored.nnrModelSlot(), 1);
        QCOMPARE(restored.activeNr(), NrSlot::NNR);
    }

    // Choosing a model while limited asks for the saved choice back, even
    // the model already saved; without a limit it asks for nothing.
    void choosingAModelWhileLimitedAsksToTryAgain()
    {
        SliceModel slice;
        slice.setNnrModelSlot(1);
        QSignalSpy retry(&slice, &SliceModel::nnrRetryRequested);
        slice.setNnrModelSlot(1);
        slice.setNnrModelSlot(0);
        slice.setNnrModelSlot(1);
        QCOMPARE(retry.count(), 0);
        slice.setNnrLimit(static_cast<int>(NnrLimit::StandardOnly));
        slice.setNnrModelSlot(1);   // the equality check would skip this
        QCOMPARE(retry.count(), 1);
        slice.requestNnrRetry();
        QCOMPARE(retry.count(), 2);
    }

    // The station mirror carries the limit to a remote GUI.
    void stationMirrorAppliesOnlyValidLimits()
    {
        SliceModel slice;
        QVERIFY(slice.applyStationNnrDiagnostic("nnrLimit", 2));
        QCOMPARE(slice.nnrLimit(), 2);
        QVERIFY(slice.applyStationNnrDiagnostic("nnrLimit", 0));
        QCOMPARE(slice.nnrLimit(), 0);
        QVERIFY(!slice.applyStationNnrDiagnostic("nnrLimit", 3));
        QVERIFY(!slice.applyStationNnrDiagnostic("nnrLimit", QStringLiteral("off")));
        QCOMPARE(slice.nnrLimit(), 0);
    }
};

QTEST_APPLESS_MAIN(TestNnrSettings)
#include "tst_nnr_settings.moc"
