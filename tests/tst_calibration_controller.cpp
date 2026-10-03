// tests/tst_calibration_controller.cpp  (NereusSDR)
//
// TDD tests for CalibrationController (Phase 3P-G commit 1).
// no-port-check: test file — no Thetis attribution required.
//
// Covers:
//  - Defaults (freqFactor=1.0, freqFactor10M=1.0, using10M=false, all offsets=0)
//  - Individual setters + changed() signal emission (idempotent no-fire)
//  - effectiveFreqCorrectionFactor() picks correct factor based on using10MHzRef
//  - Persistence round-trip (load/save under a test MAC)

#include "core/CalibrationController.h"
#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/ConnectionState.h"
#include "core/HpsdrModel.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"

#include <QtTest/QtTest>
#include <QScopeGuard>
#include <QSignalSpy>

class TstCalibrationController : public QObject {
    Q_OBJECT

private slots:
    void defaults_allCorrect();
    void setFreqCorrectionFactor_emitsChanged();
    void setFreqCorrectionFactor_idempotentNoSignal();
    void setFreqCorrectionFactor10M_emitsChanged();
    void setUsing10MHzRef_emitsChanged();
    void effectiveFreqCorrectionFactor_normalMode();
    void effectiveFreqCorrectionFactor_10MHzMode();
    void setRx1_6mLnaOffset_emitsChanged();
    void setRx2_6mLnaOffset_emitsChanged();
    void setTxDisplayOffsetDb_emitsChanged();
    void setPaCurrentSensitivity_emitsChanged();
    void setPaCurrentOffset_emitsChanged();
    void persistence_roundTrip();
    void saveWritesOnlyChangedKeys();
    void coreReloadsTheCalibrationAWindowSaved();
    void voltCalibrationDefaultsFollowTheModel();
    void unsetOrLegacyVoltCalibrationTakesTheModelDefaults();
    void aSavedVoltCalibrationSurvivesAModelChange();
    void theDefaultButtonRestoresTheModelDefaultsAndSavesThem();
    void voltCalibrationSettersClampAsThetis();
    void sixMeterLnaStoredValuesAreKeptAndUnsetOnesTakeThirteen();
};

void TstCalibrationController::defaults_allCorrect()
{
    NereusSDR::CalibrationController ctrl;

    // Source: setup.cs:5137 udHPSDRFreqCorrectFactor default 1.0 [@501e3f5]
    QCOMPARE(ctrl.freqCorrectionFactor(), 1.0);
    // Source: setup.cs:22701 btnHPSDRFreqCalReset10MHz → 1.0 [@501e3f5]
    QCOMPARE(ctrl.freqCorrectionFactor10M(), 1.0);
    // Source: setup.cs:22690 chkUsing10MHzRef default unchecked [@501e3f5]
    QCOMPARE(ctrl.using10MHzRef(), false);
    QCOMPARE(ctrl.effectiveFreqCorrectionFactor(), 1.0);
    // From Thetis setup.designer.cs:12070-12074 and 12112-12116 [v2.10.3.15]:
    // ud6mRx2LNAGainOffset.Value = 13; ud6mLNAGainOffset.Value = 13.
    QCOMPARE(ctrl.rx1_6mLnaOffset(), 13.0);
    QCOMPARE(ctrl.rx2_6mLnaOffset(), 13.0);
    // Source: setup.cs:14325 udTXDisplayCalOffset default 0 [@501e3f5]
    QCOMPARE(ctrl.txDisplayOffsetDb(), 0.0);
    // From Thetis console.cs:24937-24938 [v2.10.3.15]:
    //   private float _amp_voff = 360.0f;
    //   private float _amp_sens = 120.0f;
    QCOMPARE(ctrl.paCurrentSensitivity(), 120.0);
    QCOMPARE(ctrl.paCurrentOffset(), 360.0);
}

void TstCalibrationController::setFreqCorrectionFactor_emitsChanged()
{
    NereusSDR::CalibrationController ctrl;
    QSignalSpy spy(&ctrl, &NereusSDR::CalibrationController::changed);
    ctrl.setFreqCorrectionFactor(1.000001);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ctrl.freqCorrectionFactor(), 1.000001);
}

void TstCalibrationController::setFreqCorrectionFactor_idempotentNoSignal()
{
    NereusSDR::CalibrationController ctrl;
    ctrl.setFreqCorrectionFactor(1.5);
    QSignalSpy spy(&ctrl, &NereusSDR::CalibrationController::changed);
    ctrl.setFreqCorrectionFactor(1.5); // same value — no signal
    QCOMPARE(spy.count(), 0);
}

void TstCalibrationController::setFreqCorrectionFactor10M_emitsChanged()
{
    NereusSDR::CalibrationController ctrl;
    QSignalSpy spy(&ctrl, &NereusSDR::CalibrationController::changed);
    ctrl.setFreqCorrectionFactor10M(0.9999998);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ctrl.freqCorrectionFactor10M(), 0.9999998);
}

void TstCalibrationController::setUsing10MHzRef_emitsChanged()
{
    NereusSDR::CalibrationController ctrl;
    QSignalSpy spy(&ctrl, &NereusSDR::CalibrationController::changed);
    ctrl.setUsing10MHzRef(true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ctrl.using10MHzRef(), true);
    // Idempotent:
    ctrl.setUsing10MHzRef(true);
    QCOMPARE(spy.count(), 1);
}

// Source: setup.cs:14036-14050 udHPSDRFreqCorrectFactor_ValueChanged [@501e3f5]
void TstCalibrationController::effectiveFreqCorrectionFactor_normalMode()
{
    NereusSDR::CalibrationController ctrl;
    ctrl.setFreqCorrectionFactor(1.000002);
    ctrl.setFreqCorrectionFactor10M(0.999998);
    ctrl.setUsing10MHzRef(false);
    // Normal mode: returns the standard factor
    QCOMPARE(ctrl.effectiveFreqCorrectionFactor(), 1.000002);
}

void TstCalibrationController::effectiveFreqCorrectionFactor_10MHzMode()
{
    NereusSDR::CalibrationController ctrl;
    ctrl.setFreqCorrectionFactor(1.000002);
    ctrl.setFreqCorrectionFactor10M(0.999998);
    ctrl.setUsing10MHzRef(true);
    // 10 MHz mode: returns the 10 MHz factor
    QCOMPARE(ctrl.effectiveFreqCorrectionFactor(), 0.999998);
}

void TstCalibrationController::setRx1_6mLnaOffset_emitsChanged()
{
    NereusSDR::CalibrationController ctrl;
    QSignalSpy spy(&ctrl, &NereusSDR::CalibrationController::changed);
    ctrl.setRx1_6mLnaOffset(2.0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ctrl.rx1_6mLnaOffset(), 2.0);
}

void TstCalibrationController::setRx2_6mLnaOffset_emitsChanged()
{
    NereusSDR::CalibrationController ctrl;
    QSignalSpy spy(&ctrl, &NereusSDR::CalibrationController::changed);
    ctrl.setRx2_6mLnaOffset(-1.5);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ctrl.rx2_6mLnaOffset(), -1.5);
}

void TstCalibrationController::setTxDisplayOffsetDb_emitsChanged()
{
    NereusSDR::CalibrationController ctrl;
    QSignalSpy spy(&ctrl, &NereusSDR::CalibrationController::changed);
    ctrl.setTxDisplayOffsetDb(0.5);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ctrl.txDisplayOffsetDb(), 0.5);
}

void TstCalibrationController::setPaCurrentSensitivity_emitsChanged()
{
    NereusSDR::CalibrationController ctrl;
    QSignalSpy spy(&ctrl, &NereusSDR::CalibrationController::changed);
    ctrl.setPaCurrentSensitivity(2.5);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ctrl.paCurrentSensitivity(), 2.5);
}

void TstCalibrationController::setPaCurrentOffset_emitsChanged()
{
    NereusSDR::CalibrationController ctrl;
    QSignalSpy spy(&ctrl, &NereusSDR::CalibrationController::changed);
    ctrl.setPaCurrentOffset(0.5);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ctrl.paCurrentOffset(), 0.5);
}

void TstCalibrationController::persistence_roundTrip()
{
    const QString testMac = QStringLiteral("00:11:22:33:44:AA");

    // Write side
    {
        NereusSDR::CalibrationController ctrl;
        ctrl.setMacAddress(testMac);
        ctrl.setFreqCorrectionFactor(1.000003);
        ctrl.setFreqCorrectionFactor10M(0.999997);
        ctrl.setUsing10MHzRef(true);
        ctrl.setRx1_6mLnaOffset(3.0);
        ctrl.setRx2_6mLnaOffset(-2.0);
        ctrl.setTxDisplayOffsetDb(0.25);
        ctrl.setPaCurrentSensitivity(1.5);
        ctrl.setPaCurrentOffset(0.05);
        ctrl.save();
    }

    // Read side
    {
        NereusSDR::CalibrationController ctrl;
        ctrl.setMacAddress(testMac);
        ctrl.load();

        QCOMPARE(ctrl.freqCorrectionFactor(), 1.000003);
        QCOMPARE(ctrl.freqCorrectionFactor10M(), 0.999997);
        QCOMPARE(ctrl.using10MHzRef(), true);
        QCOMPARE(ctrl.rx1_6mLnaOffset(), 3.0);
        QCOMPARE(ctrl.rx2_6mLnaOffset(), -2.0);
        QCOMPARE(ctrl.txDisplayOffsetDb(), 0.25);
        QCOMPARE(ctrl.paCurrentSensitivity(), 1.5);
        QCOMPARE(ctrl.paCurrentOffset(), 0.05);
        // Effective factor should be the 10M one since using10M=true
        QCOMPARE(ctrl.effectiveFreqCorrectionFactor(), 0.999997);
    }
}

// R-R3-46: a Core whose calibration keys a remote window just wrote
// reloads its own controller (the P2 codec reads it for every command),
// keeps a PA forward-power table as its connect does, and a later save of
// its controller keeps the window's values.
void TstCalibrationController::coreReloadsTheCalibrationAWindowSaved()
{
    using namespace NereusSDR;
    const QString mac = QStringLiteral("AA:BB:CC:DD:EE:52");
    AppSettings& settings = AppSettings::instance();
    settings.clearHardwareValues(mac);
    const auto clean = qScopeGuard([&settings, mac] { settings.clearHardwareValues(mac); });

    RadioModel core;
    core.setBoardForTest(HPSDRHW::Saturn);
    RadioInfo info;
    info.macAddress = mac;
    info.boardType = HPSDRHW::Saturn;
    core.setLastRadioInfoForTest(info);
    core.setConnectionStateForTest(ConnectionState::Connected);
    QStringList reloads;
    core.setHardwareApplyObserverForTest([&reloads](const QString& name) { reloads << name; });

    // What a window's CalibrationController::save() sends: every cal key,
    // and a PA table it never had (boardClass 0).
    const QString base = QStringLiteral("hardware/%1/cal/").arg(mac);
    settings.setValue(base + QStringLiteral("freqFactor"), QStringLiteral("1.000002"));
    settings.setValue(base + QStringLiteral("freqFactor10M"), QStringLiteral("0.999998"));
    settings.setValue(base + QStringLiteral("using10M"), QStringLiteral("True"));
    settings.setValue(QStringLiteral("hardware/%1/paCalibration/boardClass").arg(mac),
                      QStringLiteral("0"));
    core.scheduleRemoteHardwareApply(base + QStringLiteral("freqFactor"));
    core.scheduleRemoteHardwareApply(base + QStringLiteral("using10M"));
    QTRY_COMPARE(reloads, QStringList{QStringLiteral("cal")});
    QCOMPARE(core.calibrationController().freqCorrectionFactor(), 1.000002);
    QCOMPARE(core.calibrationController().effectiveFreqCorrectionFactor(), 0.999998);
    QVERIFY(core.calibrationController().paCalProfile().boardClass != PaCalBoardClass::None);

    // The Core's own later save (its shutdown save, or its own Setup) keeps them.
    core.calibrationControllerMutable().save();
    QCOMPARE(settings.value(base + QStringLiteral("freqFactor")).toDouble(), 1.000002);
    QCOMPARE(settings.value(base + QStringLiteral("using10M")).toString(), QStringLiteral("True"));

    // Another radio's key, or a remote-role model, reloads nothing.
    reloads.clear();
    core.scheduleRemoteHardwareApply(QStringLiteral("hardware/11:22:33:44:55:66/cal/freqFactor"));
    RadioModel remote(RadioModel::Role::Remote);
    remote.setHardwareApplyObserverForTest([&reloads](const QString& name) { reloads << name; });
    remote.scheduleRemoteHardwareApply(base + QStringLiteral("freqFactor"));
    QTest::qWait(120);
    QVERIFY(reloads.isEmpty());
}

void TstCalibrationController::saveWritesOnlyChangedKeys()
{
    // R-R3-46. The Calibration tab saves after every edit. In a remote
    // window every key it writes goes to the Core, and a receive-only Core
    // refuses the transmit ones, so a receive calibration edit writes only
    // the key that changed; what load() reads back is unchanged.
    const QString mac = QStringLiteral("00:11:22:33:44:AB");
    NereusSDR::AppSettings& s = NereusSDR::AppSettings::instance();
    s.clearHardwareValues(mac);
    const auto clean = qScopeGuard([&s, mac] { s.clearHardwareValues(mac); });
    const QString base = QStringLiteral("hardware/%1/cal/").arg(mac);

    NereusSDR::CalibrationController ctrl;
    ctrl.setMacAddress(mac);
    ctrl.load();
    ctrl.setFreqCorrectionFactor(1.000004);
    ctrl.save();
    QCOMPARE(s.value(base + QStringLiteral("freqFactor")).toString(),
             QStringLiteral("1.000004"));
    for (const QString& key : {QStringLiteral("txDisplayOffset"), QStringLiteral("paSens"),
                               QStringLiteral("paOffset"), QStringLiteral("levelOffset")}) {
        QVERIFY2(!s.contains(base + key), qPrintable(key));
    }
    QVERIFY(!s.contains(QStringLiteral("hardware/%1/paCalibration/boardClass").arg(mac)));

    // A changed transmit value is still saved, and reads back.
    ctrl.setTxDisplayOffsetDb(0.5);
    ctrl.save();
    NereusSDR::CalibrationController reader;
    reader.setMacAddress(mac);
    reader.load();
    QCOMPARE(reader.freqCorrectionFactor(), 1.000004);
    QCOMPARE(reader.txDisplayOffsetDb(), 0.5);
}

// Thetis sets the volt calibration from HardwareSpecific.GetDefaultVoltCalibration
// for the radio's model (clsHardwareSpecific.cs:265-292 [v2.10.3.15]).
void TstCalibrationController::voltCalibrationDefaultsFollowTheModel()
{
    NereusSDR::CalibrationController ctrl;
    ctrl.setHardwareModel(NereusSDR::HPSDRModel::ANAN7000D);
    QCOMPARE(ctrl.paCurrentSensitivity(), 88.0);
    QCOMPARE(ctrl.paCurrentOffset(), 340.0);
    QSignalSpy spy(&ctrl, &NereusSDR::CalibrationController::changed);
    ctrl.setHardwareModel(NereusSDR::HPSDRModel::ANAN_G2);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(ctrl.paCurrentSensitivity(), static_cast<double>(66.23f));
    QCOMPARE(ctrl.paCurrentOffset(), static_cast<double>(0.001f));
}

// Thetis initVoltsAmpsCalibration (setup.cs:24365-24368 [v2.10.3.15]):
//   if (!_bSensSet || !_bVoffSet) btnAmpDefault_Click(this, EventArgs.Empty);
// A value counts as set once one other than the designer's 1 was loaded.
// Older NereusSDR saved sensitivity 1 and offset 0, which never scaled the
// reading; they take the model's defaults and are not written back.
void TstCalibrationController::unsetOrLegacyVoltCalibrationTakesTheModelDefaults()
{
    const QString mac = QStringLiteral("00:11:22:33:44:AC");
    NereusSDR::AppSettings& s = NereusSDR::AppSettings::instance();
    s.clearHardwareValues(mac);
    const auto clean = qScopeGuard([&s, mac] { s.clearHardwareValues(mac); });
    const QString base = QStringLiteral("hardware/%1/cal/").arg(mac);
    s.setValue(base + QStringLiteral("paSens"), QStringLiteral("1"));
    s.setValue(base + QStringLiteral("paOffset"), QStringLiteral("0"));

    NereusSDR::CalibrationController ctrl;
    ctrl.setHardwareModel(NereusSDR::HPSDRModel::ANAN7000D);
    ctrl.setMacAddress(mac);
    ctrl.load();
    QCOMPARE(ctrl.paCurrentSensitivity(), 88.0);
    QCOMPARE(ctrl.paCurrentOffset(), 340.0);
    ctrl.loadTransmitCalibration();
    QCOMPARE(ctrl.paCurrentSensitivity(), 88.0);
    QCOMPARE(ctrl.paCurrentOffset(), 340.0);
    ctrl.save();
    QCOMPARE(s.value(base + QStringLiteral("paSens")).toString(), QStringLiteral("1"));
    QCOMPARE(s.value(base + QStringLiteral("paOffset")).toString(), QStringLiteral("0"));
}

void TstCalibrationController::aSavedVoltCalibrationSurvivesAModelChange()
{
    const QString mac = QStringLiteral("00:11:22:33:44:AD");
    NereusSDR::AppSettings& s = NereusSDR::AppSettings::instance();
    s.clearHardwareValues(mac);
    const auto clean = qScopeGuard([&s, mac] { s.clearHardwareValues(mac); });
    {
        NereusSDR::CalibrationController ctrl;
        ctrl.setHardwareModel(NereusSDR::HPSDRModel::ANAN8000D);
        ctrl.setMacAddress(mac);
        ctrl.load();
        ctrl.setPaCurrentSensitivity(95.0);
        ctrl.setPaCurrentOffset(300.0);
        ctrl.setHardwareModel(NereusSDR::HPSDRModel::ANAN_G2);
        QCOMPARE(ctrl.paCurrentSensitivity(), 95.0);
        QCOMPARE(ctrl.paCurrentOffset(), 300.0);
        ctrl.save();
    }
    NereusSDR::CalibrationController reader;
    reader.setHardwareModel(NereusSDR::HPSDRModel::ANAN7000D);
    reader.setMacAddress(mac);
    reader.load();
    QCOMPARE(reader.paCurrentSensitivity(), 95.0);
    QCOMPARE(reader.paCurrentOffset(), 300.0);
}

// From Thetis setup.cs:24346-24352 [v2.10.3.15] btnAmpDefault_Click.
void TstCalibrationController::theDefaultButtonRestoresTheModelDefaultsAndSavesThem()
{
    const QString mac = QStringLiteral("00:11:22:33:44:AE");
    NereusSDR::AppSettings& s = NereusSDR::AppSettings::instance();
    s.clearHardwareValues(mac);
    const auto clean = qScopeGuard([&s, mac] { s.clearHardwareValues(mac); });
    const QString base = QStringLiteral("hardware/%1/cal/").arg(mac);

    NereusSDR::CalibrationController ctrl;
    ctrl.setHardwareModel(NereusSDR::HPSDRModel::ANAN7000D);
    ctrl.setMacAddress(mac);
    ctrl.load();
    ctrl.setPaCurrentSensitivity(95.0);
    ctrl.setPaCurrentOffset(300.0);
    ctrl.restoreDefaultVoltCalibration();
    QCOMPARE(ctrl.paCurrentSensitivity(), 88.0);
    QCOMPARE(ctrl.paCurrentOffset(), 340.0);
    ctrl.save();
    QCOMPARE(s.value(base + QStringLiteral("paSens")).toDouble(), 88.0);
    QCOMPARE(s.value(base + QStringLiteral("paOffset")).toDouble(), 340.0);
}

// From Thetis console.cs:24939-24959 [v2.10.3.15] AmpVoff / AmpSens:
//   if (tmp < 0) tmp = 0.0f;           (voff)
//   if (tmp < 0.001f) tmp = 0.001f;    (sens)
void TstCalibrationController::voltCalibrationSettersClampAsThetis()
{
    NereusSDR::CalibrationController ctrl;
    ctrl.setPaCurrentOffset(-5.0);
    QCOMPARE(ctrl.paCurrentOffset(), 0.0);
    ctrl.setPaCurrentSensitivity(0.0);
    QCOMPARE(ctrl.paCurrentSensitivity(), 0.001);
}

// JJ's ruling (match Thetis): 13 dB where nothing was stored; a stored
// value, 0 included, is the operator's and is kept.
void TstCalibrationController::sixMeterLnaStoredValuesAreKeptAndUnsetOnesTakeThirteen()
{
    const QString mac = QStringLiteral("00:11:22:33:44:AF");
    NereusSDR::AppSettings& s = NereusSDR::AppSettings::instance();
    s.clearHardwareValues(mac);
    const auto clean = qScopeGuard([&s, mac] { s.clearHardwareValues(mac); });
    const QString base = QStringLiteral("hardware/%1/cal/").arg(mac);

    {
        NereusSDR::CalibrationController ctrl;
        ctrl.setMacAddress(mac);
        ctrl.load();
        QCOMPARE(ctrl.rx1_6mLnaOffset(), 13.0);
        QCOMPARE(ctrl.rx2_6mLnaOffset(), 13.0);
        ctrl.save();
        QVERIFY(!s.contains(base + QStringLiteral("rx1_6mLna")));
        QVERIFY(!s.contains(base + QStringLiteral("rx2_6mLna")));
    }

    s.setValue(base + QStringLiteral("rx1_6mLna"), QStringLiteral("0"));
    s.setValue(base + QStringLiteral("rx2_6mLna"), QStringLiteral("7.5"));
    NereusSDR::CalibrationController ctrl;
    ctrl.setMacAddress(mac);
    ctrl.load();
    QCOMPARE(ctrl.rx1_6mLnaOffset(), 0.0);
    QCOMPARE(ctrl.rx2_6mLnaOffset(), 7.5);
    ctrl.save();
    QCOMPARE(s.value(base + QStringLiteral("rx1_6mLna")).toString(), QStringLiteral("0"));
    QCOMPARE(s.value(base + QStringLiteral("rx2_6mLna")).toString(), QStringLiteral("7.5"));

    // An operator's 0 set now is saved against the 13 dB default.
    NereusSDR::CalibrationController fresh;
    s.clearHardwareValues(mac);
    fresh.setMacAddress(mac);
    fresh.load();
    fresh.setRx1_6mLnaOffset(0.0);
    fresh.save();
    QCOMPARE(s.value(base + QStringLiteral("rx1_6mLna")).toString(), QStringLiteral("0"));
}

QTEST_MAIN(TstCalibrationController)
#include "tst_calibration_controller.moc"
