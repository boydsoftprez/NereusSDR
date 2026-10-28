// no-port-check: NereusSDR-original test.
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
#define private public
#include "models/RadioModel.h"
#undef private
#include "gui/MainWindow.h"
#include "models/Band.h"
#include "models/SliceModel.h"

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
bool expectSliceBRestored(const SliceModel* b)
{
    if (!b) {
        QTest::qFail("slice B was not created", __FILE__, __LINE__);
        return false;
    }
    if (b->frequency() != kSliceBFreqHz || b->dspMode() != DSPMode::LSB
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
        QVERIFY(expectSliceBRestored(b));
        QCOMPARE(a->frequency(), kSliceAFreqHz);
    }

    // The session verbs call addSlice() with the flag left false, on a Core
    // that manages the receive layout. The letter comes back, and the save
    // the Core then makes keeps Auto AGC instead of writing the defaults.
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
        QVERIFY(expectSliceBRestored(b));
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
        QVERIFY(expectSliceBRestored(b));
        QCOMPARE(b->nnrMaskFloorDb(), -31.25);
        QCOMPARE(b->activeNr(), NrSlot::NNR);
        QCOMPARE(a->frequency(), kSliceAFreqHz);
    }
};

QTEST_MAIN(TestStandaloneSliceRestore)
#include "tst_standalone_slice_restore.moc"
