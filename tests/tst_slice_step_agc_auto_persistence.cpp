// no-port-check: NereusSDR-original test.
//
// tst_slice_step_agc_auto_persistence.cpp
//
// JJ's report, 2026-10-23: "step, and agc auto selection on or off does not
// seem to persist between app launches." Each slice's tuning step
// (Slice<N>/StepHz, one on every band, JJ's ruling 2026-11-05) and its AGC-T
// AUTO switch
// (Slice<N>/AgcAutoEnabled) are set, written to the settings file, read
// back from that file into fresh objects, and must be what the operator
// left, slice by slice:
//
//   * a SliceModel on its own (saveToSettings, then restoreFromSettings);
//   * a radio connected from this computer (the save the change schedules,
//     then the slices a new RadioModel makes at connect).
//
// The remote Core's halves are in tst_core_saves_remote_slice_step (a
// window's slice across a Core restart) and tst_slice_claims_expiry (a
// paired device's slice closed when it leaves and made again when it
// returns).
//
// No radio and no audio device: a connection that sends nothing. The
// settings store is the test sandbox's (TestSandboxInit).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-23  J.J. Boyd / KG4VCF  Step and AGC-T AUTO persistence lane.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-11-05  J.J. Boyd / KG4VCF  One step per slice on every band.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QStandardPaths>

#include "core/AppSettings.h"
#include "core/RadioConnection.h"
#include "core/WdspTypes.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class SilentRadioConnection final : public RadioConnection {
    Q_OBJECT
public:
    explicit SilentRadioConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setMox(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setWatchdogEnabled(bool) override {}
    void sendTxIq(const float*, int) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

namespace {

constexpr double kSliceAFreqHz = 14200000.0;
constexpr double kSliceBFreqHz = 7100000.0;
// Two steps off the 100 Hz default, one per slice, both on kTuneStepList.
constexpr int kSliceAStepHz = 1000;
constexpr int kSliceBStepHz = 5000;

struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

// Writes the store to its file, empties it and reads the file back: what a
// quit and a relaunch leave of the settings.
void relaunchSettings()
{
    auto& s = AppSettings::instance();
    QString error;
    QVERIFY2(s.save(&error), qPrintable(error));
    s.clear();
    QVERIFY(!s.contains(QStringLiteral("Slice0/LastBand")));
    s.load();
}

} // namespace

class TestSliceStepAgcAutoPersistence : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // The settings these tests write are the test sandbox's (Qt's test
    // mode, set up by TestSandboxInit), never the operator's.
    void theSettingsFileIsTheSandboxsNotTheOperators()
    {
        QVERIFY(QStandardPaths::isTestModeEnabled());
        const QString path = AppSettings::instance().filePath();
        const QString home = QDir::homePath();
        QVERIFY2(!path.startsWith(home + QStringLiteral("/.config/NereusSDR")), qPrintable(path));
        QVERIFY2(!path.startsWith(home + QStringLiteral("/Library/Preferences/NereusSDR")),
                 qPrintable(path));
    }

    // Each slice's own step and AUTO, through the file and back, into
    // slices that start from the other value.
    void eachSlicesStepAndAutoComeBackFromTheFile()
    {
        {
            SliceModel a;
            a.setSliceIndex(0);
            a.setFrequency(kSliceAFreqHz);
            a.setStepHz(kSliceAStepHz);
            a.setAutoAgcEnabled(true);
            a.saveToSettings(Band::Band20m);

            SliceModel b;
            b.setSliceIndex(1);
            b.setFrequency(kSliceBFreqHz);
            b.setStepHz(kSliceBStepHz);
            b.setAutoAgcEnabled(false);
            b.saveToSettings(Band::Band40m);
        }
        relaunchSettings();

        SliceModel a;
        a.setSliceIndex(0);
        a.setAutoAgcEnabled(false);
        a.restoreFromSettings(Band::Band20m);
        QCOMPARE(a.stepHz(), kSliceAStepHz);
        QVERIFY(a.autoAgcEnabled());

        SliceModel b;
        b.setSliceIndex(1);
        b.setAutoAgcEnabled(true);
        b.restoreFromSettings(Band::Band40m);
        QCOMPARE(b.stepHz(), kSliceBStepHz);
        QVERIFY(!b.autoAgcEnabled());
    }

    // A radio connected from this computer: the operator changes each
    // slice's step and AUTO, the change's own scheduled save writes them,
    // and the slices a new RadioModel makes at the next connect have them.
    void aLocalRadiosSlicesComeBackWithTheirStepAndAuto()
    {
        {
            RadioModel radio;
            SilentRadioConnection conn;
            radio.injectConnectionForTest(&conn);
            DetachConnection detach{&radio};

            SliceModel* a = radio.sliceById(radio.addSlice(QStringLiteral("pan-0")));
            SliceModel* b = radio.sliceById(radio.addSlice(QStringLiteral("pan-1")));
            QVERIFY(a && b);
            a->setFrequency(kSliceAFreqHz);
            b->setFrequency(kSliceBFreqHz);
            radio.flushPendingSettingsSave();

            a->setStepHz(kSliceAStepHz);
            a->setAutoAgcEnabled(true);
            b->setStepHz(kSliceBStepHz);
            b->setAutoAgcEnabled(true);
            // The quit's flush (MainWindow::closeEvent).
            radio.flushPendingSettingsSave();
        }
        relaunchSettings();

        RadioModel radio;
        SilentRadioConnection conn;
        radio.injectConnectionForTest(&conn);
        DetachConnection detach{&radio};
        SliceModel* a = radio.sliceById(radio.addSlice(QStringLiteral("pan-0"), true));
        SliceModel* b = radio.sliceById(radio.addSlice(QStringLiteral("pan-1"), true));
        QVERIFY(a && b);
        QCOMPARE(a->frequency(), kSliceAFreqHz);
        QCOMPARE(b->frequency(), kSliceBFreqHz);
        QCOMPARE(a->stepHz(), kSliceAStepHz);
        QCOMPARE(b->stepHz(), kSliceBStepHz);
        QVERIFY(a->autoAgcEnabled());
        QVERIFY(b->autoAgcEnabled());
    }

    // JJ's ruling, 2026-11-05: one step per slice, on every band. Thetis keeps
    // one step as well (console.cs:1984, saved at console.cs:3205 [v2.10.3.15]).
    void theStepIsSavedOnTheSliceWithNoBandPart()
    {
        SliceModel slice;
        slice.setSliceIndex(1);
        slice.setFrequency(kSliceBFreqHz);
        slice.setStepHz(kSliceBStepHz);
        slice.saveToSettings(Band::Band40m);

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("Slice1/StepHz")).toInt(), kSliceBStepHz);
        QVERIFY(!s.contains(QStringLiteral("Slice1/Band40m/StepHz")));
    }

    // A band change never touches the step, either way, on a band visited
    // before or not, with an older per-band step on disk for each. The step
    // is still there at a restart.
    void aBandChangeLeavesTheStepAloneAndItIsThereAtARestart()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Slice0/Band40m/StepHz"), 25);
        s.setValue(QStringLiteral("Slice0/Band20m/StepHz"), 50);
        {
            RadioModel radio;
            SilentRadioConnection conn;
            radio.injectConnectionForTest(&conn);
            DetachConnection detach{&radio};
            radio.addSlice(QStringLiteral("pan-0"));
            radio.setActiveSlice(0);
            SliceModel* slice = radio.sliceById(0);
            QVERIFY(slice);
            slice->setFrequency(3700000.0);
            radio.flushPendingSettingsSave();
            slice->setStepHz(kSliceAStepHz);

            radio.onBandButtonClicked(Band::Band20m); // first visit, up the bands
            QCOMPARE(slice->stepHz(), kSliceAStepHz);
            radio.onBandButtonClicked(Band::Band40m); // first visit, down
            QCOMPARE(slice->stepHz(), kSliceAStepHz);
            slice->setStepHz(kSliceBStepHz);
            radio.onBandButtonClicked(Band::Band20m); // visited, up
            QCOMPARE(slice->stepHz(), kSliceBStepHz);
            radio.onBandButtonClicked(Band::Band40m); // visited, down
            QCOMPARE(slice->stepHz(), kSliceBStepHz);
            radio.flushPendingSettingsSave();
        }
        relaunchSettings();
        // The older values stay on disk, unread: nothing deletes them.
        QCOMPARE(s.value(QStringLiteral("Slice0/Band20m/StepHz")).toInt(), 50);
        QCOMPARE(s.value(QStringLiteral("Slice0/Band40m/StepHz")).toInt(), 25);

        RadioModel radio;
        SilentRadioConnection conn;
        radio.injectConnectionForTest(&conn);
        DetachConnection detach{&radio};
        SliceModel* slice = radio.sliceById(radio.addSlice(QStringLiteral("pan-0"), true));
        QVERIFY(slice);
        QCOMPARE(slice->stepHz(), kSliceBStepHz);
    }

    // A band change does not read the saved step: a step not saved yet (a
    // slice whose keys are held back has its save skipped) stays.
    void aBandChangesRestoreKeepsAStepNotSavedYet()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Slice0/StepHz"), kSliceAStepHz);
        s.setValue(QStringLiteral("Slice0/Band40m/AgcHang"), 1234);
        SliceModel slice;
        slice.setSliceIndex(0);
        slice.setStepHz(kSliceBStepHz);

        slice.restoreFromSettingsForBandChange(Band::Band40m);

        QCOMPARE(slice.stepHz(), kSliceBStepHz);
        QCOMPARE(slice.agcHang(), 1234); // the rest of the band does restore
    }

    // Settings from before the ruling: the step of the band the slice was last
    // on, whichever band is being restored. The first save writes the new key
    // and leaves the older values on disk.
    void anOlderPerBandStepIsReadWhenTheNewKeyIsAbsent()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Slice0/LastBand"), QStringLiteral("40m"));
        s.setValue(QStringLiteral("Slice0/Band40m/StepHz"), kSliceAStepHz);
        s.setValue(QStringLiteral("Slice0/Band20m/StepHz"), kSliceBStepHz);

        SliceModel slice;
        slice.setSliceIndex(0);
        slice.restoreFromSettings(Band::Band20m);
        QCOMPARE(slice.stepHz(), kSliceAStepHz);

        slice.saveToSettings(Band::Band20m);
        QCOMPARE(s.value(QStringLiteral("Slice0/StepHz")).toInt(), kSliceAStepHz);
        QCOMPARE(s.value(QStringLiteral("Slice0/Band40m/StepHz")).toInt(), kSliceAStepHz);
        QCOMPARE(s.value(QStringLiteral("Slice0/Band20m/StepHz")).toInt(), kSliceBStepHz);
    }

    // Files from before Slice<N>/LastBand have only the restored band to go on.
    void anOlderFileWithNoLastBandReadsTheRestoredBandsStep()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Slice0/Band40m/StepHz"), kSliceAStepHz);

        SliceModel slice;
        slice.setSliceIndex(0);
        slice.restoreFromSettings(Band::Band40m);
        QCOMPARE(slice.stepHz(), kSliceAStepHz);
    }

    // Once the new key exists, the older value does not put an old step back.
    void theNewKeyWinsOverAnOlderPerBandStep()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Slice0/LastBand"), QStringLiteral("40m"));
        s.setValue(QStringLiteral("Slice0/StepHz"), kSliceBStepHz);
        s.setValue(QStringLiteral("Slice0/Band40m/StepHz"), kSliceAStepHz);

        SliceModel slice;
        slice.setSliceIndex(0);
        slice.restoreFromSettings(Band::Band40m);
        QCOMPARE(slice.stepHz(), kSliceBStepHz);
    }

    // The older flat VfoStepHz key lands where the step is read.
    void theFlatLegacyStepComesBack()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("VfoFrequency"), 14100000.0);
        s.setValue(QStringLiteral("VfoStepHz"), kSliceAStepHz);
        SliceModel::migrateLegacyKeys();
        QCOMPARE(s.value(QStringLiteral("Slice0/StepHz")).toInt(), kSliceAStepHz);

        SliceModel slice;
        slice.setSliceIndex(0);
        slice.restoreFromSettings(Band::Band40m);
        QCOMPARE(slice.stepHz(), kSliceAStepHz);
    }
};

QTEST_MAIN(TestSliceStepAgcAutoPersistence)
#include "tst_slice_step_agc_auto_persistence.moc"
