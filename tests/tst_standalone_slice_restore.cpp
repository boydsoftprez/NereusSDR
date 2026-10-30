// no-port-check: NereusSDR-original test.
// 2026-10-29 - Separate fresh-slice seeding from explicit restoration and
// verify authenticated admission with stale letter state and a full DDC pool.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//
// tst_standalone_slice_restore.cpp
//
// Without a Core, receivers B to D come back at connect like A (settings
// restore audit 2026-10-11, gap 6). Their keys were always written, but
// only slice A was loaded at connect; the slices made for the other pans
// started fresh, on slice A's frequency.
//
// 2026-10-27: a slice the operator adds (+RX, a layout change) and a slice
// a layout-managed Core mints with addSlice() come back the same way,
// including Auto AGC and its offset. The Core's save of that new slice
// keeps the restored values. A letter with nothing saved still opens on
// the active slice.
//
// Drives MainWindow::populatePanSlices, the static step the connect
// handler runs for every pan that has no slice yet, and addSlice() on a
// model marked as managing a receive layout. No window is made, no radio
// and no audio device. The settings store is the test sandbox's
// (TestSandboxInit).

#include <QtTest/QtTest>

#include <QDir>
#include <QStandardPaths>

#include "core/AppSettings.h"
#include "core/WdspTypes.h"
#include "core/WdspEngine.h"
#define private public
#include "models/RadioModel.h"
#undef private
#include "gui/MainWindow.h"
#include "models/Band.h"
#include "models/SliceModel.h"
#include "MultiDeviceHarness.h"

using namespace NereusSDR;

namespace {

constexpr int    kRateHz       = 192000;
constexpr double kSliceAFreqHz = 14200000.0;
constexpr double kSliceBFreqHz = 7100000.0;

// What slice B was closed with: 40m LSB, its own AF gain, mute, and
// Auto AGC (on, offset +12.5 dB).
void saveSliceB()
{
    SliceModel saved;
    saved.setSliceIndex(1);
    saved.setFrequency(kSliceBFreqHz);
    saved.setDspMode(DSPMode::LSB);
    saved.setAfGain(33);
    saved.setMuted(true);
    saved.setAutoAgcEnabled(true);
    saved.setAutoAgcOffset(12.5);
    saved.saveToSettings(Band::Band40m);
}

// QVERIFY in a helper only returns from the helper, so this reports the
// failure and returns false for the slot to check.
bool expectSliceBRestored(const SliceModel* b, double frequency = kSliceBFreqHz,
                          DSPMode mode = DSPMode::LSB)
{
    if (!b) {
        QTest::qFail("slice B was not created", __FILE__, __LINE__);
        return false;
    }
    if (b->frequency() != frequency || b->dspMode() != mode
        || b->afGain() != 33 || !b->muted() || !b->autoAgcEnabled()
        || !qFuzzyCompare(b->autoAgcOffset(), 12.5)) {
        QTest::qFail(qPrintable(QStringLiteral(
            "slice B was %1 Hz %2 AF %3 mute %4 auto %5 offset %6")
            .arg(b->frequency())
            .arg(SliceModel::modeName(b->dspMode()))
            .arg(b->afGain())
            .arg(b->muted())
            .arg(b->autoAgcEnabled())
            .arg(b->autoAgcOffset())),
            __FILE__, __LINE__);
        return false;
    }
    return true;
}

} // namespace

class TestStandaloneSliceRestore : public QObject {
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

    void theSliceMadeForAnEmptyPanAtConnectComesBackAsItWasSaved()
    {
        saveSliceB();

        RadioModel model;
        model.configureStreamPool(2, 2, kRateHz);
        SliceModel* a = model.sliceById(model.addSlice());
        QVERIFY(a);
        a->setPanKey(QStringLiteral("pan-0"));
        a->setFrequency(kSliceAFreqHz);

        // The connect handler's call: not asked for by the operator.
        MainWindow::populatePanSlices(&model,
                                      {QStringLiteral("pan-0"), QStringLiteral("pan-1")},
                                      false, false);

        SliceModel* b = model.sliceById(1);
        QVERIFY(expectSliceBRestored(b));
        QCOMPARE(b->panKey(), QStringLiteral("pan-1"));
        // Slice A is left alone.
        QCOMPARE(a->frequency(), kSliceAFreqHz);
    }

    // Nothing saved for the slice: it starts as a new slice always has, on
    // the frequency of the slice already there.
    void aSliceWithNothingSavedStartsFresh()
    {
        RadioModel model;
        model.configureStreamPool(2, 2, kRateHz);
        SliceModel* a = model.sliceById(model.addSlice());
        QVERIFY(a);
        a->setPanKey(QStringLiteral("pan-0"));
        a->setFrequency(kSliceAFreqHz);

        MainWindow::populatePanSlices(&model,
                                      {QStringLiteral("pan-0"), QStringLiteral("pan-1")},
                                      false, false);

        SliceModel* b = model.sliceById(1);
        QVERIFY(b);
        QCOMPARE(b->frequency(), kSliceAFreqHz);
        QVERIFY(!b->muted());
        QVERIFY(!b->autoAgcEnabled());
        QCOMPARE(b->autoAgcOffset(), 20.0);
    }

    // +RX and a layout change pass restoreSavedState false. A saved letter
    // still comes back, Auto AGC included.
    // 2026-10-29: receiver preferences come back; the active VFO seeds +RX.
    void aSliceTheOperatorAddsComesBackAsItWasSaved()
    {
        saveSliceB();

        RadioModel model;
        model.configureStreamPool(2, 2, kRateHz);
        SliceModel* a = model.sliceById(model.addSlice());
        QVERIFY(a);
        a->setPanKey(QStringLiteral("pan-0"));
        a->setFrequency(kSliceAFreqHz);

        MainWindow::populatePanSlices(&model,
                                      {QStringLiteral("pan-0"), QStringLiteral("pan-1")},
                                      true, false);

        SliceModel* b = model.sliceById(1);
        QVERIFY(expectSliceBRestored(b, kSliceAFreqHz, a->dspMode()));
        QCOMPARE(a->frequency(), kSliceAFreqHz);
    }

    // The session verbs call addSlice() with the flag left false, on a Core
    // that manages the receive layout. The letter comes back, and the save
    // the Core then makes keeps Auto AGC instead of writing the defaults.
    // 2026-10-29: only explicit restoration restores the letter's VFO.
    void aLayoutManagedCoreRestoresASliceAddedWithoutTheFlag()
    {
        saveSliceB();

        RadioModel model;
        model.m_receiveLayoutManaged = true;
        model.configureStreamPool(2, 2, kRateHz);
        SliceModel* a = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        QVERIFY(a);
        a->setFrequency(kSliceAFreqHz);
        SliceModel* b = model.sliceById(model.addSlice(QStringLiteral("pan-1")));
        QVERIFY(expectSliceBRestored(b, kSliceAFreqHz, a->dspMode()));
        QCOMPARE(a->frequency(), kSliceAFreqHz);

        auto& settings = AppSettings::instance();
        settings.setValue(QStringLiteral("Slice1/AgcAutoEnabled"), QStringLiteral("SENTINEL"));
        settings.setValue(QStringLiteral("Slice1/AgcAutoOffset"), 0.0);
        model.flushPendingSettingsSave();
        QCOMPARE(settings.value(QStringLiteral("Slice1/AgcAutoEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(settings.value(QStringLiteral("Slice1/AgcAutoOffset")).toDouble(), 12.5);
    }

    // The open slice's reducer and its knobs, NNR suppression included,
    // move onto a slice that has nothing saved.
    void aNewSliceTakesTheOpenSlicesNoiseReduction()
    {
        RadioModel model;
        model.configureStreamPool(2, 2, kRateHz);
        SliceModel* a = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        QVERIFY(a);
        a->setNnrMaskFloorDb(-31.25);
        a->setNnrAlpha(2.25);
        a->setNr1Taps(96);
        a->setDfnrAttenLimit(18.0);
        a->setMnrStrength(0.4);
        a->setActiveNr(NrSlot::NNR);

        SliceModel* b = model.sliceById(model.addSlice(QStringLiteral("pan-1")));
        QVERIFY(b);
        QCOMPARE(b->nnrMaskFloorDb(), -31.25);
        QCOMPARE(b->nnrAlpha(), 2.25);
        QCOMPARE(b->nr1Taps(), 96);
        QCOMPARE(b->dfnrAttenLimit(), 18.0);
        QCOMPARE(b->mnrStrength(), 0.4);
        QCOMPARE(b->activeNr(), NrSlot::NNR);
        QCOMPARE(a->nnrMaskFloorDb(), -31.25);
        QCOMPARE(a->activeNr(), NrSlot::NNR);
    }

    // A saved letter keeps its receiver state and still takes the open
    // slice's NNR suppression.
    // 2026-10-29: this operator-created slice inherits NR and VFO together.
    void aRestoredLetterTakesTheOpenSlicesNoiseReduction()
    {
        saveSliceB();

        RadioModel model;
        model.configureStreamPool(2, 2, kRateHz);
        SliceModel* a = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        QVERIFY(a);
        a->setPanKey(QStringLiteral("pan-0"));
        a->setFrequency(kSliceAFreqHz);
        a->setNnrMaskFloorDb(-31.25);
        a->setActiveNr(NrSlot::NNR);

        MainWindow::populatePanSlices(&model,
                                      {QStringLiteral("pan-0"), QStringLiteral("pan-1")},
                                      true, false);

        SliceModel* b = model.sliceById(1);
        QVERIFY(expectSliceBRestored(b, kSliceAFreqHz, a->dspMode()));
        QCOMPARE(b->nnrMaskFloorDb(), -31.25);
        QCOMPARE(b->activeNr(), NrSlot::NNR);
        QCOMPARE(a->frequency(), kSliceAFreqHz);
    }

    void explicitStartupRestoreKeepsTheLettersIndependentNoiseReduction()
    {
        const QString mac = QStringLiteral("AA:BB:CC:DD:EE:77");
        saveSliceB();
        {
            SliceModel saved;
            saved.setSliceIndex(1);
            saved.setSettingsRadioIdentity(mac);
            saved.setFrequency(kSliceBFreqHz);
            saved.setDspMode(DSPMode::LSB);
            saved.setNnrMaskFloorDb(-37.5);
            saved.setNnrAlpha(2.5);
            saved.setNr1Taps(96);
            saved.setActiveNr(NrSlot::NNR);
            saved.saveToSettings(Band::Band40m);
        }

        RadioModel model;
        RadioInfo info;
        info.macAddress = mac;
        model.setLastRadioInfoForTest(info);
        model.configureStreamPool(2, 2, kRateHz);
        SliceModel* a = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        QVERIFY(a);
        a->setFrequency(kSliceAFreqHz);
        a->setActiveNr(NrSlot::Off);
        a->setNnrMaskFloorDb(-12.0);

        MainWindow::populatePanSlices(&model,
                                      {QStringLiteral("pan-0"), QStringLiteral("pan-1")},
                                      false, false);
        SliceModel* b = model.sliceById(1);
        QVERIFY(b);
        QCOMPARE(b->frequency(), kSliceBFreqHz);
        QCOMPARE(b->activeNr(), NrSlot::NNR);
        QCOMPARE(b->nnrMaskFloorDb(), -37.5);
        QCOMPARE(b->nnrAlpha(), 2.5);
        QCOMPARE(b->nr1Taps(), 96);
        QCOMPARE(a->activeNr(), NrSlot::Off);
        b->saveToSettings(b->band());
        QVERIFY(AppSettings::instance().save());
        AppSettings::instance().clear();
        AppSettings::instance().load();
        SliceModel roundTrip;
        roundTrip.setSliceIndex(1);
        roundTrip.setSettingsRadioIdentity(mac);
        roundTrip.loadFromSettings();
        roundTrip.restoreFromSettings(Band::Band40m);
        QCOMPARE(roundTrip.activeNr(), NrSlot::NNR);
        QCOMPARE(roundTrip.nnrMaskFloorDb(), -37.5);
        QCOMPARE(roundTrip.nnrAlpha(), 2.5);
        QCOMPARE(roundTrip.nr1Taps(), 96);
    }

    void explicitStartupRestoredSampleRateReachesTheDdc()
    {
        saveSliceB();
        AppSettings::instance().setValue(QStringLiteral("Slice1/Band40m/SampleRate"), 384000);
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        model.configureStreamPool(2, 2, kRateHz);
        SliceModel* a = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        QVERIFY(a);
        a->setFrequency(kSliceAFreqHz);

        MainWindow::populatePanSlices(&model,
                                      {QStringLiteral("pan-0"), QStringLiteral("pan-1")},
                                      false, false);
        QVERIFY(model.waitForReceiveLaneForTest());
        SliceModel* b = model.sliceById(1);
        QVERIFY(b);
        QCOMPARE(b->sampleRateHz(), 384000);
        QCOMPARE(model.streamSampleRateHzForTest(b->streamIndex()), 384000);
        QCOMPARE(model.streamSampleRateHzForTest(a->streamIndex()), kRateHz);
    }

    void authenticatedNewDeviceIgnoresStaleLetterVfoWhenAllDdcsAreOccupied()
    {
        Core core;
        core.model->m_receiveLayoutManaged = true;
        core.model->m_receiveLayoutMac = AppSettings::normalizedRadioMac(core.model->currentRadioMac());
        core.model->configureStreamPool(1, 5, kRateHz);
        SliceModel* a = core.model->sliceById(0);
        QVERIFY(a);
        a->setFrequency(kSliceAFreqHz);
        a->setDspMode(DSPMode::USB);
        QVERIFY(a->streamIndex() >= 0);
        QCOMPARE(core.model->activeStreamCount(), 1);
        Device first;
        Device incoming(QStringLiteral("New Mac"), QStringLiteral("computer"));
        core.pair(first);
        core.pair(incoming);
        QVERIFY(admitted(core.signIn(first)));

        saveSliceB();
        auto& settings = AppSettings::instance();
        // A stale lock must survive as a receiver preference without
        // vetoing the initial station VFO seed. A stale RADE mode must
        // never start a decoder as an intermediate step of fresh creation.
        settings.setValue(QStringLiteral("Slice1/Locked"), QStringLiteral("True"));
        settings.setValue(QStringLiteral("Slice1/Band40m/DspMode"),
                          static_cast<int>(DSPMode::RADE_U));
        settings.setValue(QStringLiteral("Slice1/Band40m/SampleRate"), 384000);
        const QString mac = core.model->currentRadioMac();
        QVERIFY(!mac.isEmpty());
        QVERIFY(DeviceLayoutStore::load(settings, mac, incoming.key.fingerprint()).isEmpty());
        LoopbackTransport* app = core.signIn(incoming);
        QVERIFY(admitted(app));
        SliceModel* b = core.model->sliceById(1);
        QVERIFY(expectSliceBRestored(b, kSliceAFreqHz, DSPMode::USB));
        QVERIFY(b->locked());
        QCOMPARE(b->streamIndex(), a->streamIndex());
        QCOMPARE(b->sampleRateHz(), kRateHz);
        QCOMPARE(a->sampleRateHz(), kRateHz);
        QCOMPARE(core.model->streamSampleRateHzForTest(a->streamIndex()), kRateHz);
        QCOMPARE(core.model->activeStreamCount(), 1);
        QCOMPARE(core.model->sliceOwnership()->mark(1).owner, incoming.key.fingerprint());
        QVERIFY(core.model->wdspEngine()->radeChannel(1) == nullptr);
        QVERIFY(b->filterLow() >= 0);
        core.model->flushPendingSettingsSave();
        settings.clear();
        settings.load();
        QCOMPARE(settings.value(QStringLiteral("Slice1/AgcAutoEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(settings.value(QStringLiteral("Slice1/AgcAutoOffset")).toDouble(), 12.5);

        // The next +RX uses this device's active slice even when the
        // station-level active slice belongs to another operator.
        b->setLocked(false);
        b->setFrequency(kSliceAFreqHz + 10000.0);
        b->setDspMode(DSPMode::DIGU);
        b->setNnrMaskFloorDb(-32.0);
        b->setActiveNr(NrSlot::NNR);
        QVERIFY(core.invoke(app, "setActiveSliceById", {int64("sliceId", 1)})
                    .value(QStringLiteral("accepted")).toBool());
        core.model->setActiveSlice(0);
        settings.setValue(QStringLiteral("Slice2/LastBand"), bandKeyName(Band::Band40m));
        settings.setValue(QStringLiteral("Slice2/Band40m/Frequency"), kSliceBFreqHz);
        settings.setValue(QStringLiteral("Slice2/Band40m/DspMode"), static_cast<int>(DSPMode::LSB));
        settings.setValue(QStringLiteral("Slice2/Band40m/SampleRate"), 384000);
        QVERIFY(core.invoke(app, "addSlice", {utf8("initialPanId", QString())})
                    .value(QStringLiteral("accepted")).toBool());
        SliceModel* c = core.model->sliceById(2);
        QVERIFY(c);
        QCOMPARE(c->frequency(), b->frequency());
        QCOMPARE(c->dspMode(), DSPMode::DIGU);
        QCOMPARE(c->activeNr(), NrSlot::NNR);
        QCOMPARE(c->nnrMaskFloorDb(), -32.0);
        QCOMPARE(c->streamIndex(), b->streamIndex());
        QCOMPARE(c->sampleRateHz(), kRateHz);
        QCOMPARE(core.model->streamSampleRateHzForTest(a->streamIndex()), kRateHz);
        QCOMPARE(core.model->activeStreamCount(), 1);
    }
};

QTEST_MAIN(TestStandaloneSliceRestore)
#include "tst_standalone_slice_restore.moc"
