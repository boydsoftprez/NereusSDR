// =================================================================
// tests/tst_level_calibration.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test file. The Thetis citations below
// document the upstream behaviour exercised; no C# is translated here.
//
// Level calibration storage, reset and TCI report:
//   - RX1_MeterCalOffsetDb stays the meter offset, with the Thetis
//     per-model default (clsHardwareSpecific.cs:408-423 [v2.10.3.15]);
//     a value saved before this change reads back unchanged.
//   - RX1_DisplayCalOffsetDb is the display offset, with the Thetis
//     per-model default (clsHardwareSpecific.cs:424-440 [v2.10.3.15]).
//     It feeds only TCI calibration_ex (TCIServer.cs:1160-1176
//     [v2.10.3.15]) and never moves the panadapter, which follows the
//     meter offset (console.cs:12305-12311 [v2.10.3.15]).
//   - Reset (console.cs:46868-46886 [v2.10.3.15], ResetLevelCalibration)
//     returns the meter and display offsets to the model's defaults and
//     touches no other calibration.
//   - Both are kept per radio model (rx_meter_cal_offset_by_radio,
//     console.cs:196-197 [v2.10.3.15]); a one-value calibration saved by an
//     earlier build moves to the model connected when it is first loaded.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29 - Written by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"
#include "core/StepAttenuatorController.h"
#include "core/TciProtocol.h"
#include "models/RadioModel.h"
#ifdef HAVE_WEBSOCKETS
#include "core/TciServer.h"
#endif

using namespace NereusSDR;

namespace {

const QString kMac = QStringLiteral("AA:BB:CC:DD:1C:01");
const QString kMeterKey = QStringLiteral("RX1_MeterCalOffsetDb");
const QString kDisplayKey = QStringLiteral("RX1_DisplayCalOffsetDb");
const QString kPreampKey = QStringLiteral("RX1_PreampOffsetsDb");
const QString kMeterByRadioKey = QStringLiteral("RxMeterCalOffsetDbByRadio");
const QString kDisplayByRadioKey = QStringLiteral("RxDisplayCalOffsetDbByRadio");

void setUpLocal(RadioModel& model, HPSDRModel radio)
{
    model.setHpsdrModelForTest(radio);
    RadioInfo info;
    info.macAddress = kMac;
    info.boardType = boardForModel(radio);
    model.setLastRadioInfoForTest(info);
    model.setConnectionStateForTest(ConnectionState::Connected);
}

} // namespace

class TstLevelCalibration : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // A meter offset saved before this change reads back as it was, and the
    // receive offset built on it is unchanged.
    void meterOffset_savedValueUnchanged()
    {
        AppSettings::instance().setValue(kMeterKey, QStringLiteral("-3.000000"));
        RadioModel model;
        setUpLocal(model, HPSDRModel::ANAN_G2);
        QCOMPARE(model.rxMeterCalOffsetDb(), -3.0);
        QCOMPARE(model.calibrationMeter(0), -3.0);
        QCOMPARE(model.calibrationMeter(1), -3.0);
        QCOMPARE(model.rxMeterOffsetDb(),
                 model.rxPreampOffsetDb() - 3.0 + model.rx6mGainOffsetDb());
        // It now lives in the connected model's entry.
        QVERIFY(!AppSettings::instance().contains(kMeterKey));
        QVERIFY(AppSettings::instance().contains(kMeterByRadioKey));
    }

    // Thetis keeps the meter and display calibration per model
    // (rx_meter_cal_offset_by_radio, console.cs:196-197, 10182, 10190,
    // 14892-14895 [v2.10.3.15]): calibrating one radio leaves another's.
    void perModel_twoModelsKeepSeparateValues()
    {
        RadioModel model;
        setUpLocal(model, HPSDRModel::ANAN_G2);
        model.setRxMeterCalOverrideDb(-2.5);
        model.setRxDisplayCalOverrideDb(3.0);

        model.setHpsdrModelForTest(HPSDRModel::ANAN7000D);
        QCOMPARE(model.rxMeterCalOffsetDb(),
                 static_cast<double>(rxMeterCalOffsetDefaultFor(HPSDRModel::ANAN7000D)));
        QCOMPARE(model.rxDisplayCalOffsetDb(),
                 static_cast<double>(rxDisplayCalOffsetDefaultFor(HPSDRModel::ANAN7000D)));
        model.setRxMeterCalOverrideDb(1.25);

        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        QCOMPARE(model.rxMeterCalOffsetDb(), -2.5);
        QCOMPARE(model.rxDisplayCalOffsetDb(), 3.0);
        model.setHpsdrModelForTest(HPSDRModel::ANAN7000D);
        QCOMPARE(model.rxMeterCalOffsetDb(), 1.25);
        // Clearing one model's entry leaves the other's.
        model.setRxMeterCalOverrideDb(std::nullopt);
        QCOMPARE(model.rxMeterCalOffsetDb(),
                 static_cast<double>(rxMeterCalOffsetDefaultFor(HPSDRModel::ANAN7000D)));
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        QCOMPARE(model.rxMeterCalOffsetDb(), -2.5);
    }

    // A calibration saved by an earlier build (one value) belongs to the
    // model connected when it is first loaded; another model reads its own
    // default, and the user's value is never lost.
    void perModel_earlierValueMovesToConnectedModel()
    {
        AppSettings::instance().setValue(kMeterKey, QStringLiteral("-3.000000"));
        AppSettings::instance().setValue(kDisplayKey, QStringLiteral("7.500000"));
        RadioModel model;
        setUpLocal(model, HPSDRModel::ANAN_G2);
        QCOMPARE(model.rxMeterCalOffsetDb(), -3.0);
        QCOMPARE(model.rxDisplayCalOffsetDb(), 7.5);

        model.setHpsdrModelForTest(HPSDRModel::HERMES);
        QCOMPARE(model.rxMeterCalOffsetDb(),
                 static_cast<double>(rxMeterCalOffsetDefaultFor(HPSDRModel::HERMES)));
        QCOMPARE(model.rxDisplayCalOffsetDb(),
                 static_cast<double>(rxDisplayCalOffsetDefaultFor(HPSDRModel::HERMES)));

        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        QCOMPARE(model.rxMeterCalOffsetDb(), -3.0);
        QCOMPARE(model.rxDisplayCalOffsetDb(), 7.5);
    }

    // A window of an earlier build that writes the one-value key: the value
    // goes to the connected model.
    void perModel_earlierWindowWriteGoesToConnectedModel()
    {
        RadioModel model;
        setUpLocal(model, HPSDRModel::ANAN_G2);
        model.setRxMeterCalOverrideDb(-2.0);
        model.setHpsdrModelForTest(HPSDRModel::ANAN7000D);
        AppSettings::instance().setValue(kMeterKey, QStringLiteral("4.000000"));
        QVERIFY(model.applyLevelCalibrationSetting(kMeterKey));
        QCOMPARE(model.rxMeterCalOffsetDb(), 4.0);
        QVERIFY(!AppSettings::instance().contains(kMeterKey));
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        QCOMPARE(model.rxMeterCalOffsetDb(), -2.0);
    }

    // Reset returns every model's entry to its default, as Thetis's
    // ResetLevelCalibration loops over every model (console.cs:46870-46874
    // [v2.10.3.15]).
    void reset_returnsEveryModelToDefault()
    {
        RadioModel model;
        setUpLocal(model, HPSDRModel::ANAN_G2);
        model.setRxMeterCalOverrideDb(-2.0);
        model.setHpsdrModelForTest(HPSDRModel::ANAN7000D);
        model.setRxDisplayCalOverrideDb(5.0);
        model.resetLevelCalibration();
        QVERIFY(!AppSettings::instance().contains(kMeterByRadioKey));
        QVERIFY(!AppSettings::instance().contains(kDisplayByRadioKey));
        QCOMPARE(model.rxDisplayCalOffsetDb(),
                 static_cast<double>(rxDisplayCalOffsetDefaultFor(HPSDRModel::ANAN7000D)));
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        QCOMPARE(model.rxMeterCalOffsetDb(),
                 static_cast<double>(rxMeterCalOffsetDefaultFor(HPSDRModel::ANAN_G2)));
    }

    // With nothing saved, each model reads its Thetis default.
    void meterOffset_absentUsesModelDefault()
    {
        for (HPSDRModel radio : { HPSDRModel::ANAN_G2, HPSDRModel::ANAN7000D,
                                  HPSDRModel::HERMES, HPSDRModel::ANAN_G2E }) {
            RadioModel model;
            setUpLocal(model, radio);
            QCOMPARE(model.rxMeterCalOffsetDb(),
                     static_cast<double>(rxMeterCalOffsetDefaultFor(radio)));
            QCOMPARE(model.calibrationMeter(0),
                     static_cast<double>(rxMeterCalOffsetDefaultFor(radio)));
            QVERIFY(!AppSettings::instance().contains(kMeterKey));
        }
    }

    void displayOffset_absentUsesModelDefault()
    {
        for (HPSDRModel radio : { HPSDRModel::ANAN_G2, HPSDRModel::ANAN7000D,
                                  HPSDRModel::HERMES, HPSDRModel::ANAN_G2E }) {
            RadioModel model;
            setUpLocal(model, radio);
            QCOMPARE(model.rxDisplayCalOffsetDb(),
                     static_cast<double>(rxDisplayCalOffsetDefaultFor(radio)));
            QCOMPARE(model.calibrationDisplay(0),
                     static_cast<double>(rxDisplayCalOffsetDefaultFor(radio)));
            QCOMPARE(model.calibrationDisplay(1),
                     static_cast<double>(rxDisplayCalOffsetDefaultFor(radio)));
        }
    }

    // The display offset reaches TCI only: the meter offset and the
    // panadapter's calibration stay where they were.
    void displayOffset_neverMovesPanadapter()
    {
        RadioModel model;
        setUpLocal(model, HPSDRModel::ANAN_G2);
        const double meterBefore = model.rxMeterOffsetDb();
        const double keyedBefore = model.keyedDisplayOffsetDb(true);
        QSignalSpy meterSpy(&model, &RadioModel::rxMeterOffsetChanged);
        QSignalSpy calSpy(&model, &RadioModel::levelCalibrationChanged);

        AppSettings::instance().setValue(kDisplayKey, QStringLiteral("7.500000"));
        QVERIFY(model.applyLevelCalibrationSetting(kDisplayKey));

        QCOMPARE(model.calibrationDisplay(0), 7.5);
        QCOMPARE(model.rxMeterOffsetDb(), meterBefore);
        QCOMPARE(model.keyedDisplayOffsetDb(true), keyedBefore);
        QCOMPARE(meterSpy.count(), 0);
        QCOMPARE(calSpy.count(), 1);
    }

    // A meter offset change takes effect at once.
    void meterOffset_applyRefreshesMeter()
    {
        RadioModel model;
        setUpLocal(model, HPSDRModel::ANAN_G2);
        QSignalSpy meterSpy(&model, &RadioModel::rxMeterOffsetChanged);
        QSignalSpy calSpy(&model, &RadioModel::levelCalibrationChanged);
        const double preamp = model.rxPreampOffsetDb();

        AppSettings::instance().setValue(kMeterKey, QStringLiteral("-1.500000"));
        QVERIFY(model.applyLevelCalibrationSetting(kMeterKey));

        QCOMPARE(model.rxMeterOffsetDb(), preamp - 1.5 + model.rx6mGainOffsetDb());
        QCOMPARE(meterSpy.count(), 1);
        QCOMPARE(calSpy.count(), 1);
    }

    void applyLevelCalibrationSetting_ignoresOtherKeys()
    {
        RadioModel model;
        setUpLocal(model, HPSDRModel::ANAN_G2);
        QSignalSpy calSpy(&model, &RadioModel::levelCalibrationChanged);
        QVERIFY(!model.applyLevelCalibrationSetting(QStringLiteral("MultimeterDelayMs")));
        QVERIFY(!model.applyLevelCalibrationSetting(
            QStringLiteral("hardware/%1/cal/txDisplayOffset").arg(kMac)));
        QCOMPARE(calSpy.count(), 0);
    }

    // Level Cal's preamp offsets: absent, each of the ten settings reads
    // Thetis's defaults (console.cs:1999-2009 [v2.10.3.15]), so readings
    // taken before this change are unchanged.
    void preampOffsets_absentReadTheDefaults()
    {
        RadioModel model;
        setUpLocal(model, HPSDRModel::ANAN100);
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(model.rx1PreampOffsetDbFor(static_cast<PreampMode>(i)),
                     rxPreampOffsetDbFor(i));
            // Thetis leaves rx2_preamp_offset's -40 and -50 entries at 0
            // (console.cs:2011-2019 [v2.10.3.15]).
            const bool unset = (i == 5 || i == 6);
            QCOMPARE(model.rx2PreampOffsetDbFor(static_cast<PreampMode>(i)),
                     unset ? 0.0f : rxPreampOffsetDbFor(i));
        }
        StepAttenuatorController ctrl;
        model.setStepAttController(&ctrl);
        ctrl.setStepAttEnabled(false);
        ctrl.setPreampMode(PreampMode::Off);
        QCOMPARE(model.rxPreampOffsetDb(), 20.0);
        ctrl.setPreampMode(PreampMode::On);
        QCOMPARE(model.rxPreampOffsetDb(), 0.0);
        model.setStepAttController(nullptr);
    }

    // A saved list drives the receive offset of the selected setting; a
    // list that is not ten numbers reads the defaults.
    void preampOffsets_savedListDrivesTheOffset()
    {
        AppSettings::instance().setValue(
            kPreampKey,
            QStringLiteral("18.500|0.000|9.250|20.000|30.000|40.000|50.000|10.000|20.000|30.000"));
        RadioModel model;
        setUpLocal(model, HPSDRModel::ANAN100);
        StepAttenuatorController ctrl;
        model.setStepAttController(&ctrl);
        ctrl.setStepAttEnabled(false);
        ctrl.setPreampMode(PreampMode::Off);
        QCOMPARE(model.rxPreampOffsetDb(), 18.5);
        QCOMPARE(model.rx1PreampOffsetDbFor(PreampMode::Minus10), 9.25f);
        model.setStepAttController(nullptr);

        AppSettings::instance().setValue(kPreampKey, QStringLiteral("1|2|3"));
        QCOMPARE(model.rx1PreampOffsetDbFor(PreampMode::Off), 20.0f);
        AppSettings::instance().setValue(
            kPreampKey, QStringLiteral("x|0|10|20|30|40|50|10|20|30"));
        QCOMPARE(model.rx1PreampOffsetDbFor(PreampMode::Off), 20.0f);
    }

    // Setting one entry saves all ten at three decimals, as Thetis saves
    // rx1_preamp_offset (console.cs:3202-3203 [v2.10.3.15]), and moves the
    // meter at once. RX2's entries are held only while the program runs,
    // as Thetis's rx2_preamp_offset is.
    void preampOffsets_setSavesAllTen()
    {
        RadioModel model;
        setUpLocal(model, HPSDRModel::ANAN100);
        StepAttenuatorController ctrl;
        model.setStepAttController(&ctrl);
        ctrl.setStepAttEnabled(false);
        ctrl.setPreampMode(PreampMode::Off);
        model.refreshRxMeterOffset();
        QSignalSpy meterSpy(&model, &RadioModel::rxMeterOffsetChanged);
        model.setRx1PreampOffsetDb(PreampMode::Off, 17.12345f);
        QCOMPARE(AppSettings::instance().value(kPreampKey).toString(),
                 QStringLiteral("17.123|0.000|10.000|20.000|30.000|40.000|50.000|10.000|20.000|30.000"));
        QCOMPARE(model.rx1PreampOffsetDbFor(PreampMode::Off), 17.123f);
        QVERIFY(meterSpy.count() >= 1);
        model.setRx2PreampOffsetDb(PreampMode::Off, 16.5f);
        QCOMPARE(model.rx2PreampOffsetDbFor(PreampMode::Off), 16.5f);
        QCOMPARE(AppSettings::instance().value(kPreampKey).toString().left(6),
                 QStringLiteral("17.123"));
        model.setStepAttController(nullptr);
    }

    // Reset leaves the preamp offsets alone, as Thetis's
    // ResetLevelCalibration does (console.cs:46868-46886 [v2.10.3.15]).
    void reset_leavesPreampOffsets()
    {
        RadioModel model;
        setUpLocal(model, HPSDRModel::ANAN100);
        model.setRx1PreampOffsetDb(PreampMode::Off, 18.0f);
        model.resetLevelCalibration();
        QCOMPARE(model.rx1PreampOffsetDbFor(PreampMode::Off), 18.0f);
        QVERIFY(AppSettings::instance().contains(kPreampKey));
    }

    // Reset returns the meter and display offsets to the model's defaults
    // and leaves every other calibration alone.
    void reset_clearsMeterAndDisplayOnly()
    {
        AppSettings& s = AppSettings::instance();
        s.setValue(kMeterKey, QStringLiteral("-3.000000"));
        s.setValue(kDisplayKey, QStringLiteral("7.500000"));
        s.setHardwareValue(kMac, QStringLiteral("cal/txDisplayOffset"), QStringLiteral("2.5"));
        s.setHardwareValue(kMac, QStringLiteral("cal/rx1_6mLna"), QStringLiteral("4.0"));

        RadioModel model;
        setUpLocal(model, HPSDRModel::ANAN7000D);
        QSignalSpy meterSpy(&model, &RadioModel::rxMeterOffsetChanged);
        QSignalSpy calSpy(&model, &RadioModel::levelCalibrationChanged);

        QVERIFY(model.levelCalibrationResetAvailable());
        QVERIFY(model.requestResetLevelCalibration().isEmpty());

        QVERIFY(!s.contains(kMeterKey));
        QVERIFY(!s.contains(kDisplayKey));
        QCOMPARE(model.calibrationMeter(0),
                 static_cast<double>(rxMeterCalOffsetDefaultFor(HPSDRModel::ANAN7000D)));
        QCOMPARE(model.calibrationDisplay(0),
                 static_cast<double>(rxDisplayCalOffsetDefaultFor(HPSDRModel::ANAN7000D)));
        QCOMPARE(s.hardwareValue(kMac, QStringLiteral("cal/txDisplayOffset")).toString(),
                 QStringLiteral("2.5"));
        QCOMPARE(s.hardwareValue(kMac, QStringLiteral("cal/rx1_6mLna")).toString(),
                 QStringLiteral("4.0"));
        QCOMPARE(meterSpy.count(), 1);
        QCOMPARE(calSpy.count(), 1);
    }

    // A remote window follows the Core's level calibration settings.
    void remote_followsStationSetting()
    {
        RadioModel remote(RadioModel::Role::Remote);
        QSignalSpy calSpy(&remote, &RadioModel::levelCalibrationChanged);
        remote.reportStationSettingChanged(kDisplayKey);
        QCOMPARE(calSpy.count(), 1);
        remote.reportStationSettingChanged(kMeterKey);
        QCOMPARE(calSpy.count(), 2);
        remote.reportStationSettingChanged(QString());
        QCOMPARE(calSpy.count(), 3);
        remote.reportStationSettingChanged(kMeterByRadioKey);
        QCOMPARE(calSpy.count(), 4);
        remote.reportStationSettingChanged(kDisplayByRadioKey);
        QCOMPARE(calSpy.count(), 5);
        remote.reportStationSettingChanged(QStringLiteral("BandPlanName"));
        QCOMPARE(calSpy.count(), 5);
    }

    // With no Core, a remote window cannot reset and says why.
    void remote_resetWithoutCoreRefused()
    {
        RadioModel remote(RadioModel::Role::Remote);
        QVERIFY(!remote.levelCalibrationResetAvailable());
        QVERIFY(!remote.requestResetLevelCalibration().isEmpty());
    }

    // TCI calibration_ex carries the meter and display offsets
    // (TCIServer.cs:1160-1176 [v2.10.3.15]).
    void tci_calibrationExLine()
    {
        AppSettings::instance().setValue(kMeterKey, QStringLiteral("-3.000000"));
        AppSettings::instance().setValue(kDisplayKey, QStringLiteral("7.500000"));
        RadioModel model;
        setUpLocal(model, HPSDRModel::ANAN_G2);
        TciProtocol proto(&model);
        QCOMPARE(proto.calibrationExLineFor(0),
                 QStringLiteral("calibration_ex:0,-3.000000,7.500000,0.000000,0.000000,0.000000;"));
        QCOMPARE(proto.calibrationExLineFor(1),
                 QStringLiteral("calibration_ex:1,-3.000000,7.500000,0.000000,0.000000,0.000000;"));
    }

#ifdef HAVE_WEBSOCKETS
    // A change reaches connected apps for both receivers
    // (MeterCalOffsetChangedHandlers / DisplayOffsetChangedHandlers,
    // TCIServer.cs:6785-6786 [v2.10.3.15]).
    void tci_changeBroadcastsBothReceivers()
    {
        RadioModel model;
        setUpLocal(model, HPSDRModel::ANAN_G2);
        TciServer server(&model);
        TciProtocol* p = server.protocolForTest();
        QVERIFY(p);
        while (p->hasPendingNotification()) {
            p->takePendingNotification();
        }
        AppSettings::instance().setValue(kDisplayKey, QStringLiteral("7.500000"));
        model.applyLevelCalibrationSetting(kDisplayKey);
        QStringList out;
        while (p->hasPendingNotification()) {
            out << p->takePendingNotification();
        }
        const QString meter = QString::number(
            static_cast<double>(rxMeterCalOffsetDefaultFor(HPSDRModel::ANAN_G2)), 'f', 6);
        QCOMPARE(out, (QStringList{
            QStringLiteral("calibration_ex:0,%1,7.500000,0.000000,0.000000,0.000000;").arg(meter),
            QStringLiteral("calibration_ex:1,%1,7.500000,0.000000,0.000000,0.000000;").arg(meter)}));
    }
#endif
};

QTEST_MAIN(TstLevelCalibration)
#include "tst_level_calibration.moc"
