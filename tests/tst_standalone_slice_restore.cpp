// no-port-check: NereusSDR-original test.
// 2026-10-29 - Separate fresh-slice seeding from explicit restoration and
// verify authenticated admission with stale letter state and a full DDC pool.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-11-06 - A desktop brings back every slice from the radio's saved layout.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
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
#include "core/RadioConnection.h"
#include "core/ReceiveLayoutStore.h"
#include "core/MoxController.h"
#include "core/TxSliceArbiter.h"
#include "gui/ReceiveLayoutNotices.h"

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

const QString kDesktopMac = QStringLiteral("AA:BB:CC:DD:EE:41");
const QString kPan0 = QStringLiteral("pan-0");
const QString kPan1 = QStringLiteral("pan-1");
const QString kPan2 = QStringLiteral("pan-2");

// Nothing a desktop connect has waiting to save may be for a band its slice is
// not on: a connect writes no other band's memory. Reports through qFail, so a
// test that goes on still fails.
void expectNoCaptureForAnotherBand(const RadioModel& model)
{
    for (auto it = model.m_pendingBandSettings.cbegin();
         it != model.m_pendingBandSettings.cend(); ++it) {
        const SliceModel* slice = model.sliceById(it.key());
        if (!slice) {
            continue;
        }
        const QString own = QStringLiteral("Slice%1/Band%2/")
            .arg(it.key())
            .arg(bandKeyName(bandFromFrequency(slice->frequency())));
        const QString anyBand = QStringLiteral("Slice%1/Band").arg(it.key());
        for (auto key = it.value().cbegin(); key != it.value().cend(); ++key) {
            if (key.key().startsWith(anyBand) && !key.key().startsWith(own)) {
                QTest::qFail(qPrintable(QStringLiteral("a connect would write %1 with the slice on %2")
                                            .arg(key.key()).arg(slice->frequency())),
                             __FILE__, __LINE__);
                return;
            }
        }
    }
}

// A window with no Core connecting, in connectToRadioImpl's order, then the
// queued step of its connect handler. The first model of a test is the run
// before the restart, the next one the run after it.
void connectDesktop(RadioModel& model, int ddcs, int maxSlices, const QStringList& pans)
{
    RadioInfo info;
    info.macAddress = kDesktopMac;
    info.protocol = ProtocolVersion::Protocol2;
    model.setLastRadioInfoForTest(info);
    model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
    const int start = model.addStartingSlices(kDesktopMac);
    if (SliceModel* slice = model.sliceById(start)) {
        model.setActiveSlice(static_cast<int>(model.slices().indexOf(slice)));
        model.loadSliceState(slice);
    }
    model.configureStreamPool(ddcs, maxSlices, kRateHz);
    model.bindUnboundSlices();
    TxSliceArbiter* arbiter = model.txSliceArbiter();
    arbiter->setMacAddress(kDesktopMac);
    arbiter->load();
    MainWindow::populatePanSlices(&model, pans, false, false);
    arbiter->finishRestore();
    expectNoCaptureForAnotherBand(model);
}

// Letter A was last on 40 m, and 20 m and 40 m's USB filter have values of their
// own that a connect must leave alone.
void saveLetterAOn40WithOwn20()
{
    SliceModel saved;
    saved.setSliceIndex(0);
    saved.setFrequency(14074000.0);
    saved.setDspMode(DSPMode::DIGU);
    saved.setAgcThreshold(-77);
    saved.saveToSettings(Band::Band20m);
    saved.setFrequency(7150000.0);
    saved.setDspMode(DSPMode::USB);
    saved.setFilter(321, 2345);
    saved.saveToSettings(Band::Band40m);
    saved.setFrequency(7074000.0);
    saved.setDspMode(DSPMode::LSB);
    saved.setAgcThreshold(-55);
    saved.saveToSettings(Band::Band40m);
}

// QCOMPARE in a helper only returns from the helper, so this is called last.
void expectLetterAMemoriesKept()
{
    auto& settings = AppSettings::instance();
    QCOMPARE(settings.value(QStringLiteral("Slice0/Band20m/Frequency")).toDouble(), 14074000.0);
    QCOMPARE(settings.value(QStringLiteral("Slice0/Band20m/DspMode")).toInt(),
             static_cast<int>(DSPMode::DIGU));
    QCOMPARE(settings.value(QStringLiteral("Slice0/Band20m/AgcThreshold")).toInt(), -77);
    QCOMPARE(settings.value(QStringLiteral("Slice0/Band40m/Frequency")).toDouble(), 7074000.0);
    QCOMPARE(settings.value(QStringLiteral("Slice0/Band40m/AgcThreshold")).toInt(), -55);
    QCOMPARE(settings.value(QStringLiteral("Slice0/Band40m/ModeUSB/FilterLow")).toInt(), 321);
    QCOMPARE(settings.value(QStringLiteral("Slice0/Band40m/ModeUSB/FilterHigh")).toInt(), 2345);
    QCOMPARE(settings.value(QStringLiteral("Slice0/LastBand")).toString(), QStringLiteral("40m"));
}

QList<int> slicesInRecord()
{
    QList<int> ids;
    const auto layout = ReceiveLayoutStore::load(AppSettings::instance(), kDesktopMac);
    for (const ReceiveSliceState& state : layout.slices) {
        ids.append(state.id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

} // namespace

// Records anything that could put the radio on the air.
class KeyWatchConnection final : public RadioConnection {
    Q_OBJECT
public:
    int moxCalls{0};
    int txIqCalls{0};

    explicit KeyWatchConnection(QObject* parent = nullptr) : RadioConnection(parent)
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
    void setMox(bool on) override
    {
        if (on) {
            ++moxCalls;
        }
    }
    void setAntennaRouting(AntennaRouting) override {}
    void setAlexRxBpf(AlexRxBpf) override {}
    void setWatchdogEnabled(bool enabled) override { m_watchdogEnabled = enabled; }
    void sendTxIq(const float*, int) override { ++txIqCalls; }
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

    // +RX and a layout change ask for a new receiver. JJ's ruling, 2026-11-05:
    // it is a copy of the slice the operator is on, and the letter's saved
    // AF gain, mute and Auto AGC stay out of it.
    void aSliceTheOperatorAddsIsACopyOfTheOpenSlice()
    {
        saveSliceB();

        RadioModel model;
        model.configureStreamPool(2, 2, kRateHz);
        SliceModel* a = model.sliceById(model.addSlice());
        QVERIFY(a);
        a->setPanKey(QStringLiteral("pan-0"));
        a->setFrequency(kSliceAFreqHz);
        a->setAfGain(41);

        MainWindow::populatePanSlices(&model,
                                      {QStringLiteral("pan-0"), QStringLiteral("pan-1")},
                                      true, false);

        SliceModel* b = model.sliceById(1);
        QVERIFY(b);
        QCOMPARE(b->frequency(), kSliceAFreqHz);
        QCOMPARE(b->dspMode(), a->dspMode());
        QCOMPARE(b->afGain(), 41);
        QVERIFY(!b->muted());
        QVERIFY(!b->autoAgcEnabled());
        QCOMPARE(b->autoAgcOffset(), 20.0);
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

    // A used letter takes the open slice's NNR suppression too.
    // 2026-10-29: this operator-created slice inherits NR and VFO together.
    void aUsedLetterTakesTheOpenSlicesNoiseReduction()
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
        QVERIFY(b);
        QCOMPARE(b->frequency(), kSliceAFreqHz);
        QCOMPARE(b->dspMode(), a->dspMode());
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
        const QString mac = QStringLiteral("AA:BB:CC:DD:EE:78");
        AppSettings::instance().setValue(
            QStringLiteral("hardware/%1/slices/1/SampleRate").arg(AppSettings::normalizedRadioMac(mac)),
            384000);
        RadioModel model;
        RadioInfo info;
        info.macAddress = mac;
        model.setLastRadioInfoForTest(info);
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
        // A stale lock stays out of a new receiver (JJ's ruling, 2026-11-05).
        // A stale RADE mode must never start a decoder as an intermediate
        // step of fresh creation.
        settings.setValue(QStringLiteral("Slice1/Locked"), QStringLiteral("True"));
        settings.setValue(QStringLiteral("Slice1/Band40m/DspMode"),
                          static_cast<int>(DSPMode::RADE_U));
        const QString mac = core.model->currentRadioMac();
        QVERIFY(!mac.isEmpty());
        settings.setValue(QStringLiteral("hardware/%1/slices/1/SampleRate")
                              .arg(AppSettings::normalizedRadioMac(mac)), 384000);
        QVERIFY(DeviceLayoutStore::load(settings, mac, incoming.key.fingerprint()).isEmpty());
        LoopbackTransport* app = core.signIn(incoming);
        QVERIFY(admitted(app));
        SliceModel* b = core.model->sliceById(1);
        QVERIFY(b);
        QCOMPARE(b->frequency(), kSliceAFreqHz);
        QCOMPARE(b->dspMode(), DSPMode::USB);
        QCOMPARE(b->afGain(), a->afGain());
        QVERIFY(!b->muted());
        QVERIFY(!b->locked());
        QCOMPARE(b->streamIndex(), a->streamIndex());
        QCOMPARE(b->sampleRateHz(), kRateHz);
        QCOMPARE(a->sampleRateHz(), kRateHz);
        QCOMPARE(core.model->streamSampleRateHzForTest(a->streamIndex()), kRateHz);
        QCOMPARE(core.model->activeStreamCount(), 1);
        QCOMPARE(core.model->sliceOwnership()->mark(1).owner, incoming.key.fingerprint());
        QVERIFY(core.model->wdspEngine()->radeChannel(1) == nullptr);
        QVERIFY(b->filterLow() >= 0);
        // What the copy has reaches the file, over what the letter held.
        core.model->flushPendingSettingsSave();
        settings.clear();
        settings.load();
        QCOMPARE(settings.value(QStringLiteral("Slice1/AgcAutoEnabled")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(settings.value(QStringLiteral("Slice1/AgcAutoOffset")).toDouble(), 20.0);
        QCOMPARE(settings.value(QStringLiteral("Slice1/Locked")).toString(),
                 QStringLiteral("False"));

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
        settings.setValue(QStringLiteral("hardware/%1/slices/2/SampleRate")
                              .arg(AppSettings::normalizedRadioMac(mac)), 384000);
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

    // JJ's ruling 2026-11-05: a desktop brings back every slice, as a Core
    // does. A second slice on a pan used to be gone after a relaunch.
    void twoSlicesOnOnePanComeBackAfterARestart()
    {
        {
            RadioModel before;
            connectDesktop(before, 2, 5, {kPan0});
            SliceModel* a = before.sliceById(0);
            QVERIFY(a);
            a->setFrequency(kSliceAFreqHz);
            before.addSliceOnPan(kPan0, false, true);
            SliceModel* b = before.sliceById(1);
            QVERIFY(b);
            b->setFrequency(kSliceBFreqHz);
            b->setDspMode(DSPMode::LSB);
            b->setAfGain(33);
            b->setMuted(true);
            before.flushPendingSettingsSave();
        }
        RadioModel after;
        connectDesktop(after, 2, 5, {kPan0});
        QCOMPARE(after.slices().size(), 2);
        SliceModel* b = after.sliceById(1);
        QVERIFY(b);
        QCOMPARE(b->panKey(), kPan0);
        QCOMPARE(b->frequency(), kSliceBFreqHz);
        QCOMPARE(b->dspMode(), DSPMode::LSB);
        QCOMPARE(b->afGain(), 33);
        QVERIFY(b->muted());
        QVERIFY(b->streamIndex() >= 0);
        QCOMPARE(after.sliceById(0)->frequency(), kSliceAFreqHz);
        QCOMPARE(after.sliceById(0)->panKey(), kPan0);
    }

    // Two pans a little apart keep a receiver each: on one receiver they
    // would tune in lockstep.
    void slicesOnTwoPansKeepAReceiverEach()
    {
        {
            RadioModel before;
            connectDesktop(before, 2, 5, {kPan0, kPan1});
            before.sliceById(0)->setFrequency(kSliceAFreqHz);
            before.sliceById(1)->setFrequency(kSliceAFreqHz + 10000.0);
            before.flushPendingSettingsSave();
        }
        RadioModel after;
        connectDesktop(after, 2, 5, {kPan0, kPan1});
        QCOMPARE(after.slices().size(), 2);
        QCOMPARE(after.sliceById(1)->panKey(), kPan1);
        QVERIFY(after.sliceById(0)->streamIndex() != after.sliceById(1)->streamIndex());
    }

    void aTransmitSliceThatIsNotAComesBackAndNothingKeys()
    {
        {
            RadioModel before;
            connectDesktop(before, 2, 5, {kPan0, kPan1});
            before.sliceById(1)->setFrequency(kSliceBFreqHz);
            QVERIFY(before.txSliceArbiter()->requestHandoff(1));
            // Saved when it moves, not only at the disconnect.
            QCOMPARE(AppSettings::instance()
                         .value(QStringLiteral("hardware/%1/TxBoundSliceId").arg(kDesktopMac))
                         .toInt(),
                     1);
            before.flushPendingSettingsSave();
        }
        RadioModel after;
        KeyWatchConnection radio;
        after.injectConnectionForTest(&radio);
        const auto detach = qScopeGuard([&] { after.injectConnectionForTest(nullptr); });
        connectDesktop(after, 2, 5, {kPan0, kPan1});
        SliceModel* b = after.sliceById(1);
        QVERIFY(b);
        QCOMPARE(after.txBoundSlice(), b);
        QVERIFY(b->isTxSlice());
        QVERIFY(!after.sliceById(0)->isTxSlice());
        QVERIFY(!after.moxController()->isMox());
        QCOMPARE(radio.moxCalls, 0);
        QCOMPARE(radio.txIqCalls, 0);
        // The first slice's bind did not write over the saved choice.
        after.flushPendingSettingsSave();
        after.txSliceArbiter()->save();
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("hardware/%1/TxBoundSliceId").arg(kDesktopMac))
                     .toInt(),
                 1);
    }

    // More slices than the radio has room for: the rest stay saved, the
    // window says so once, and a radio with room brings them back.
    void slicesThatDoNotFitStaySavedAndComeBackLater()
    {
        {
            RadioModel before;
            connectDesktop(before, 3, 5, {kPan0, kPan1, kPan2});
            before.sliceById(0)->setFrequency(kSliceAFreqHz);
            before.sliceById(1)->setFrequency(kSliceBFreqHz);
            before.sliceById(2)->setFrequency(3600000.0);
            before.sliceById(2)->setDspMode(DSPMode::LSB);
            before.sliceById(2)->setAfGain(21);
            before.flushPendingSettingsSave();
        }
        QCOMPARE(slicesInRecord(), QList<int>({0, 1, 2}));
        {
            RadioModel small;
            QSignalSpy rejected(&small, &RadioModel::sliceAddRejected);
            ReceiveLayoutNotices notices;
            QStringList toasts;
            connect(&small, &RadioModel::receiveLayoutRestoreStatusChanged, &small, [&] {
                const QString toast = notices.toastFor(small.receiveLayoutRestoreState(),
                                                       small.receiveLayoutRestoreMessage(), false);
                if (!toast.isEmpty()) {
                    toasts.append(toast);
                }
            });
            connectDesktop(small, 2, 5, {kPan0, kPan1, kPan2});
            QCOMPARE(small.slices().size(), 2);
            QVERIFY(small.sliceById(2) == nullptr);
            QCOMPARE(rejected.count(), 0);
            QCOMPARE(toasts.size(), 1);
            QVERIFY2(toasts.first().contains(QStringLiteral("Receiver C")),
                     qPrintable(toasts.first()));
            QVERIFY2(OperatorWording::isPlain(toasts.first()), qPrintable(toasts.first()));
            // A Core's holds are not a desktop's: the radio's slices save.
            QVERIFY(!small.receiveLayoutPendingAdmission());
            QVERIFY(!small.m_receiveLayoutManaged);
            QVERIFY(!small.m_receiveLayoutProtected);
            QVERIFY(!small.sliceKeysAreHeld(0));
            QVERIFY(!small.sliceKeysAreHeld(1));
            // A new slice takes a letter nobody is waiting for.
            small.addSliceOnPan(kPan0, false, true);
            QVERIFY(small.sliceById(3) != nullptr);
            QVERIFY(small.sliceById(2) == nullptr);
            small.sliceById(1)->setFrequency(kSliceBFreqHz + 1000.0);
            small.flushPendingSettingsSave();
            QCOMPARE(slicesInRecord(), QList<int>({0, 1, 2, 3}));
            QCOMPARE(toasts.size(), 1);
            small.removeSlice(3);
            small.flushPendingSettingsSave();
        }
        QCOMPARE(slicesInRecord(), QList<int>({0, 1, 2}));
        RadioModel roomy;
        connectDesktop(roomy, 3, 5, {kPan0, kPan1, kPan2});
        QCOMPARE(roomy.slices().size(), 3);
        SliceModel* c = roomy.sliceById(2);
        QVERIFY(c);
        QCOMPARE(c->frequency(), 3600000.0);
        QCOMPARE(c->dspMode(), DSPMode::LSB);
        QCOMPARE(c->afGain(), 21);
        QCOMPARE(c->panKey(), kPan2);
        QVERIFY(roomy.receiveLayoutRestoreMessage().isEmpty());
    }

    // A letter past the radio's slice limit is the same case.
    void aLetterPastTheSliceLimitStaysSaved()
    {
        {
            RadioModel before;
            connectDesktop(before, 3, 5, {kPan0, kPan1, kPan2});
            before.flushPendingSettingsSave();
        }
        {
            RadioModel small;
            connectDesktop(small, 3, 2, {kPan0, kPan1, kPan2});
            QCOMPARE(small.slices().size(), 2);
            small.sliceById(0)->setFrequency(kSliceAFreqHz);
            small.flushPendingSettingsSave();
        }
        QCOMPARE(slicesInRecord(), QList<int>({0, 1, 2}));
    }

    // No record: exactly today's connect. One slice plus one per empty pan,
    // and an existing install's letters come back through their own keys.
    void firstRunWithNoRecordBehavesAsBefore()
    {
        saveSliceB();
        RadioModel model;
        connectDesktop(model, 2, 5, {kPan0, kPan1});
        QCOMPARE(model.slices().size(), 2);
        QCOMPARE(model.sliceById(0)->panKey(), kPan0);
        QVERIFY(expectSliceBRestored(model.sliceById(1)));
        QCOMPARE(model.sliceById(1)->panKey(), kPan1);
        QCOMPARE(model.txBoundSlice(), model.sliceById(0));
        // And the first save starts the record with all of it.
        model.flushPendingSettingsSave();
        QCOMPARE(slicesInRecord(), QList<int>({0, 1}));
    }

    // A slice closed before a quit: the close's own save runs before the
    // slice leaves the list, so the record must catch up by the disconnect's
    // flush, inside the save timer.
    void aClosedSliceStaysClosed_data()
    {
        QTest::addColumn<QString>("panOfB");
        QTest::newRow("sharing a pan") << kPan0;
        QTest::newRow("on its own pan") << kPan1;
    }

    void aClosedSliceStaysClosed()
    {
        QFETCH(QString, panOfB);
        {
            RadioModel before;
            connectDesktop(before, 2, 5, {kPan0, kPan1});
            before.sliceById(1)->setPanKey(panOfB);
            before.sliceById(1)->setFrequency(kSliceBFreqHz);
            before.flushPendingSettingsSave();
            QCOMPARE(slicesInRecord(), QList<int>({0, 1}));
            before.removeSlice(1);
            before.flushPendingSettingsSave();
        }
        RadioModel after;
        connectDesktop(after, 2, 5, {kPan0, kPan1});
        QCOMPARE(after.slices().size(), 1);
        QVERIFY(after.sliceById(1) == nullptr);
        QCOMPARE(slicesInRecord(), QList<int>({0}));
    }

    // A closed A: the window starts on what was open, on its letter.
    void closingAStartsTheNextRunOnTheNextLetter()
    {
        {
            RadioModel before;
            connectDesktop(before, 2, 5, {kPan0, kPan1});
            before.sliceById(1)->setFrequency(kSliceBFreqHz);
            before.flushPendingSettingsSave();
            before.removeSlice(0);
            before.sliceById(1)->setPanKey(kPan0);
            before.flushPendingSettingsSave();
        }
        RadioModel after;
        connectDesktop(after, 2, 5, {kPan0});
        QCOMPARE(after.slices().size(), 1);
        QVERIFY(after.sliceById(0) == nullptr);
        QCOMPARE(after.sliceById(1)->frequency(), kSliceBFreqHz);
        QCOMPARE(after.windowSliceId(), 1);
        QCOMPARE(after.activeSlice(), after.sliceById(1));
    }

    // A layout from the menu is not a restore: it makes a new receiver from
    // the slice the operator is on, not the letter's saved values.
    void aLayoutChangeFromTheMenuIsNotARestore()
    {
        {
            RadioModel before;
            connectDesktop(before, 2, 5, {kPan0});
            before.sliceById(0)->setFrequency(kSliceAFreqHz);
            before.flushPendingSettingsSave();
        }
        saveSliceB();
        RadioModel model;
        connectDesktop(model, 2, 5, {kPan0});
        QCOMPARE(model.slices().size(), 1);
        MainWindow::populatePanSlices(&model, {kPan0, kPan1}, true, false);
        SliceModel* b = model.sliceById(1);
        QVERIFY(b);
        QCOMPARE(b->frequency(), kSliceAFreqHz);
        QVERIFY(!b->muted());
        QCOMPARE(b->panKey(), kPan1);
    }

    // The window shows fewer pans than the radio's record names. The window
    // keeps its layout, and those slices go on its first pan, as shrinking
    // a layout puts them.
    void aSliceOnAPanTheWindowDoesNotShowGoesOnItsFirstPan()
    {
        {
            RadioModel before;
            connectDesktop(before, 2, 5, {kPan0, kPan1});
            before.sliceById(1)->setFrequency(kSliceBFreqHz);
            before.flushPendingSettingsSave();
        }
        RadioModel after;
        connectDesktop(after, 2, 5, {kPan0});
        QCOMPARE(after.slices().size(), 2);
        QCOMPARE(after.sliceById(1)->panKey(), kPan0);
        QCOMPARE(after.sliceById(1)->frequency(), kSliceBFreqHz);
    }

    // A layout change reaches the save path by itself, with no disconnect.
    void aNewSliceReachesTheRecordWithoutADisconnect()
    {
        RadioModel model;
        connectDesktop(model, 2, 5, {kPan0});
        model.addSliceOnPan(kPan0, false, true);
        QTRY_COMPARE(slicesInRecord(), QList<int>({0, 1}));
    }

    // A paired device's entry is its own: a window with Run Core off neither
    // starts it nor loses it.
    void aPairedDevicesEntryIsNeitherRestoredNorLost()
    {
        saveSliceB();
        const QByteArray device = QByteArray(32, '\x11');
        QList<ReceiveSliceState> saved;
        saved.append({0, kPan0, 7074000.0, DSPMode::USB, SliceOwnership::stationDevice(), device});
        saved.append({1, kPan0, kSliceBFreqHz, DSPMode::LSB, {}, {}});
        QVERIFY(ReceiveLayoutStore::stage(AppSettings::instance(), kDesktopMac, saved));
        {
            RadioModel model;
            connectDesktop(model, 2, 5, {kPan0});
            QCOMPARE(model.slices().size(), 1);
            QVERIFY(model.sliceById(0) == nullptr);
            QVERIFY(model.sliceById(1) != nullptr);
            model.sliceById(1)->setAfGain(40);
            model.flushPendingSettingsSave();
        }
        const auto layout = ReceiveLayoutStore::load(AppSettings::instance(), kDesktopMac);
        QCOMPARE(layout.slices.size(), 2);
        QCOMPARE(layout.slices.at(0).heldFor.isEmpty() ? layout.slices.at(1).heldFor
                                                       : layout.slices.at(0).heldFor,
                 device);
    }

    // A record that cannot be read is left as it is, and the window starts as
    // it does with none.
    void anUnreadableRecordIsKeptAndTheWindowSaysSo()
    {
        const QString key = QStringLiteral("hardware/%1/receiveLayout")
                                .arg(AppSettings::normalizedRadioMac(kDesktopMac));
        AppSettings::instance().setValue(key, QStringLiteral("{not json"));
        const QVariant before = AppSettings::instance().value(key);
        RadioModel model;
        connectDesktop(model, 2, 5, {kPan0, kPan1});
        QCOMPARE(model.slices().size(), 2);
        QCOMPARE(model.receiveLayoutRestoreState(), QStringLiteral("invalid"));
        model.sliceById(1)->setFrequency(kSliceBFreqHz);
        model.flushPendingSettingsSave();
        QCOMPARE(AppSettings::instance().value(key), before);
    }

    // Connecting again keeps the slices on screen: a slice closed while the
    // radio was off stays closed, though the record could not yet say so.
    void aReconnectDoesNotReopenAClosedSlice()
    {
        RadioModel model;
        connectDesktop(model, 2, 5, {kPan0});
        model.addSliceOnPan(kPan0, false, true);
        model.flushPendingSettingsSave();
        QCOMPARE(slicesInRecord(), QList<int>({0, 1}));
        model.removeSlice(1);
        QCOMPARE(model.addStartingSlices(kDesktopMac), -1);
        MainWindow::populatePanSlices(&model, {kPan0, kPan1}, false, false);
        QCOMPARE(model.slices().size(), 1);
        model.flushPendingSettingsSave();
        QCOMPARE(slicesInRecord(), QList<int>({0}));
    }

    // A connect that drops before its queued restore ran saves nothing over
    // the record, and the next connect still brings it all back.
    void aConnectDroppedBeforeTheRestoreLosesNothing()
    {
        {
            RadioModel before;
            connectDesktop(before, 2, 5, {kPan0, kPan1});
            before.flushPendingSettingsSave();
        }
        RadioModel model;
        RadioInfo info;
        info.macAddress = kDesktopMac;
        info.protocol = ProtocolVersion::Protocol2;
        model.setLastRadioInfoForTest(info);
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        QVERIFY(model.addStartingSlices(kDesktopMac) >= 0);
        model.scheduleSettingsSave();
        model.flushPendingSettingsSave();
        QCOMPARE(slicesInRecord(), QList<int>({0, 1}));
        QCOMPARE(model.addStartingSlices(kDesktopMac), -1);
        model.configureStreamPool(2, 5, kRateHz);
        model.bindUnboundSlices();
        MainWindow::populatePanSlices(&model, {kPan0, kPan1}, false, false);
        QCOMPARE(model.slices().size(), 2);
    }

    // Connecting to another radio in the same run keeps what is on screen, as
    // it always has. That radio's record is not replaced by another radio's
    // slices, and this radio's record stays this radio's.
    void anotherRadiosRecordIsLeftAlone()
    {
        const QString other = QStringLiteral("AA:BB:CC:DD:EE:42");
        QList<ReceiveSliceState> saved;
        saved.append({0, kPan0, kSliceBFreqHz, DSPMode::LSB, {}, {}});
        saved.append({2, kPan0, 3600000.0, DSPMode::LSB, {}, {}});
        QVERIFY(ReceiveLayoutStore::stage(AppSettings::instance(), other, saved));

        RadioModel model;
        connectDesktop(model, 2, 5, {kPan0, kPan1});
        model.flushPendingSettingsSave();
        QCOMPARE(slicesInRecord(), QList<int>({0, 1}));

        QCOMPARE(model.addStartingSlices(other), -1);
        MainWindow::populatePanSlices(&model, {kPan0, kPan1}, false, false);
        QCOMPARE(model.slices().size(), 2);
        model.sliceById(1)->setFrequency(kSliceBFreqHz);
        model.flushPendingSettingsSave();
        const auto layout = ReceiveLayoutStore::load(AppSettings::instance(), other);
        QCOMPARE(layout.slices.size(), 2);
        QCOMPARE(layout.slices.at(1).id, 2);
        QCOMPARE(slicesInRecord(), QList<int>({0, 1}));
    }

    // Slices sharing a receiver take the highest rate saved among them.
    void slicesSharingAReceiverTakeTheHighestSavedRate()
    {
        {
            RadioModel before;
            connectDesktop(before, 2, 5, {kPan0});
            before.sliceById(0)->setFrequency(kSliceAFreqHz);
            before.addSliceOnPan(kPan0, false, true);
            before.sliceById(1)->setFrequency(kSliceAFreqHz + 5000.0);
            before.flushPendingSettingsSave();
        }
        const QString mac = AppSettings::normalizedRadioMac(kDesktopMac);
        AppSettings::instance().setValue(
            QStringLiteral("hardware/%1/slices/1/SampleRate").arg(mac), 384000);
        RadioModel after;
        connectDesktop(after, 2, 5, {kPan0});
        QVERIFY(after.waitForReceiveLaneForTest());
        QCOMPARE(after.sliceById(0)->streamIndex(), after.sliceById(1)->streamIndex());
        QCOMPARE(after.streamSampleRateHzForTest(after.sliceById(0)->streamIndex()), 384000);
        QCOMPARE(after.sliceById(0)->sampleRateHz(), 384000);
        QCOMPARE(after.sliceById(1)->sampleRateHz(), 384000);
    }

    // The starting slice is made on the constructor's 14.225 MHz, holding the
    // letter's last band's values, and moves to its last band when its saved
    // state loads. What was captured before that is not an edit: it used to
    // write 14.225 MHz and those values over the letter's 20 m memory.
    void aConnectLeavesAnotherBandsMemoryAlone()
    {
        saveLetterAOn40WithOwn20();
        {
            RadioModel model;
            connectDesktop(model, 2, 5, {kPan0});
            QCOMPARE(model.sliceById(0)->frequency(), 7074000.0);
            model.flushPendingSettingsSave();
        }
        expectLetterAMemoriesKept();
    }

    void aConnectWithARecordLeavesAnotherBandsMemoryAlone()
    {
        {
            RadioModel first;
            connectDesktop(first, 2, 5, {kPan0});
            first.flushPendingSettingsSave();
        }
        QCOMPARE(slicesInRecord(), QList<int>({0}));
        saveLetterAOn40WithOwn20();
        {
            RadioModel model;
            connectDesktop(model, 2, 5, {kPan0});
            QCOMPARE(model.sliceById(0)->frequency(), 7074000.0);
            model.flushPendingSettingsSave();
        }
        expectLetterAMemoriesKept();
    }

    // With a connection, as at a real connect, the slice's handlers are wired,
    // and the restore itself captures as it goes.
    void aWiredConnectLeavesAnotherBandsMemoryAlone()
    {
        saveLetterAOn40WithOwn20();
        {
            KeyWatchConnection connection;
            RadioModel model;
            model.injectConnectionForTest(&connection);
            auto detach = qScopeGuard([&] { model.injectConnectionForTest(nullptr); });
            connectDesktop(model, 2, 5, {kPan0});
            QCOMPARE(model.sliceById(0)->frequency(), 7074000.0);
            model.flushPendingSettingsSave();
        }
        expectLetterAMemoriesKept();
    }

    // What the operator does after the connect is still saved on that band,
    // with a band change straight after it.
    void anEditAfterTheConnectIsStillSaved()
    {
        saveLetterAOn40WithOwn20();
        KeyWatchConnection connection;
        RadioModel model;
        model.injectConnectionForTest(&connection);
        auto detach = qScopeGuard([&] { model.injectConnectionForTest(nullptr); });
        connectDesktop(model, 2, 5, {kPan0});
        model.sliceById(0)->setAgcThreshold(-41);
        model.sliceById(0)->setFrequency(14100000.0);
        model.flushPendingSettingsSave();
        auto& settings = AppSettings::instance();
        QCOMPARE(settings.value(QStringLiteral("Slice0/Band40m/AgcThreshold")).toInt(), -41);
        QCOMPARE(settings.value(QStringLiteral("Slice0/Band20m/Frequency")).toDouble(), 14100000.0);
    }

    // JJ's ruling: extras that do not fit stay saved. The limit drops on a
    // connect to the same radio, with the slices live.
    void aSliceClosedByASmallerLimitStaysSavedAndComesBack()
    {
        RadioModel model;
        connectDesktop(model, 3, 5, {kPan0, kPan1, kPan2});
        model.sliceById(2)->setFrequency(3600000.0);
        model.flushPendingSettingsSave();
        QCOMPARE(slicesInRecord(), QList<int>({0, 1, 2}));

        QCOMPARE(model.addStartingSlices(kDesktopMac), -1);
        model.configureStreamPool(3, 2, kRateHz);
        model.bindUnboundSlices();
        QVERIFY(!model.sliceById(2));
        MainWindow::populatePanSlices(&model, {kPan0, kPan1, kPan2}, false, false);
        QVERIFY(!model.sliceById(2));
        model.flushPendingSettingsSave();
        QCOMPARE(slicesInRecord(), QList<int>({0, 1, 2}));

        QCOMPARE(model.addStartingSlices(kDesktopMac), -1);
        model.configureStreamPool(3, 5, kRateHz);
        model.bindUnboundSlices();
        MainWindow::populatePanSlices(&model, {kPan0, kPan1, kPan2}, false, false);
        SliceModel* c = model.sliceById(2);
        QVERIFY(c);
        QCOMPARE(c->frequency(), 3600000.0);
        model.flushPendingSettingsSave();
        QCOMPARE(slicesInRecord(), QList<int>({0, 1, 2}));
    }

    // They go with the mode: a DIGU copy with a zero offset does not demodulate
    // the same. Audio pan, binaural and diversity stay at their defaults.
    void aNewReceiverTakesTheOpenSlicesModeSettings()
    {
        RadioModel model;
        model.configureStreamPool(2, 2, kRateHz);
        SliceModel* a = model.sliceById(model.addSlice(kPan0));
        QVERIFY(a);
        a->setFrequency(kSliceAFreqHz);
        a->setDiglOffsetHz(-1234);
        a->setDiguOffsetHz(1357);
        a->setRttyMarkHz(2125);
        a->setRttyShiftHz(425);
        a->setApfEnabled(true);
        a->setApfTuneHz(120);
        a->setFmCtcssMode(1);
        a->setFmCtcssValueHz(123.0);
        a->setFmOffsetHz(600000);
        a->setFmTxMode(FmTxMode::High);
        a->setFmReverse(true);
        a->setAudioPan(0.25);
        a->setBinauralEnabled(true);
        const SliceModel fresh;
        QVERIFY(a->diglOffsetHz() != fresh.diglOffsetHz());
        QVERIFY(a->diguOffsetHz() != fresh.diguOffsetHz());
        QVERIFY(a->rttyMarkHz() != fresh.rttyMarkHz());
        QVERIFY(a->rttyShiftHz() != fresh.rttyShiftHz());
        QVERIFY(a->apfTuneHz() != fresh.apfTuneHz());
        QVERIFY(a->fmCtcssMode() != fresh.fmCtcssMode());
        QVERIFY(a->fmCtcssValueHz() != fresh.fmCtcssValueHz());
        QVERIFY(a->fmOffsetHz() != fresh.fmOffsetHz());
        QVERIFY(a->fmTxMode() != fresh.fmTxMode());

        MainWindow::populatePanSlices(&model, {kPan0, kPan1}, true, false);
        SliceModel* b = model.sliceById(1);
        QVERIFY(b);
        QCOMPARE(b->diglOffsetHz(), a->diglOffsetHz());
        QCOMPARE(b->diguOffsetHz(), a->diguOffsetHz());
        QCOMPARE(b->rttyMarkHz(), a->rttyMarkHz());
        QCOMPARE(b->rttyShiftHz(), a->rttyShiftHz());
        QVERIFY(b->apfEnabled());
        QCOMPARE(b->apfTuneHz(), a->apfTuneHz());
        QCOMPARE(b->fmCtcssMode(), a->fmCtcssMode());
        QCOMPARE(b->fmCtcssValueHz(), a->fmCtcssValueHz());
        QCOMPARE(b->fmOffsetHz(), a->fmOffsetHz());
        QCOMPARE(b->fmTxMode(), a->fmTxMode());
        QVERIFY(b->fmReverse());
        QCOMPARE(b->audioPan(), fresh.audioPan());
        QCOMPARE(b->binauralEnabled(), fresh.binauralEnabled());
        QCOMPARE(b->diversityEnabled(), fresh.diversityEnabled());
    }

    // The slice the operator was on comes back with the rest of the layout.
    void theActiveSliceComesBackAndNothingKeys()
    {
        {
            RadioModel before;
            connectDesktop(before, 2, 5, {kPan0, kPan1});
            QVERIFY(before.setActiveSliceById(1));
            before.flushPendingSettingsSave();
        }
        KeyWatchConnection radio;
        RadioModel after;
        after.injectConnectionForTest(&radio);
        const auto detach = qScopeGuard([&] { after.injectConnectionForTest(nullptr); });
        connectDesktop(after, 2, 5, {kPan0, kPan1});
        QCOMPARE(after.activeSlice(), after.sliceById(1));
        QVERIFY(after.sliceById(1)->isActiveSlice());
        QVERIFY(!after.sliceById(0)->isActiveSlice());
        QCOMPARE(after.txBoundSlice(), after.sliceById(0));
        QCOMPARE(radio.moxCalls, 0);
        QCOMPARE(radio.txIqCalls, 0);
    }

    // The two are saved apart: the transmit slice is not moved by the active one.
    void theActiveAndTransmitSlicesComeBackApart()
    {
        {
            RadioModel before;
            connectDesktop(before, 2, 5, {kPan0, kPan1});
            QVERIFY(before.txSliceArbiter()->requestHandoff(1));
            QVERIFY(before.setActiveSliceById(0));
            before.flushPendingSettingsSave();
        }
        RadioModel after;
        connectDesktop(after, 2, 5, {kPan0, kPan1});
        QCOMPARE(after.activeSlice(), after.sliceById(0));
        QCOMPARE(after.txBoundSlice(), after.sliceById(1));
    }

    // Saved letter gone from the record: today's starting slice.
    void aSavedActiveSliceThatIsGoneFallsBackToTheStartingSlice()
    {
        {
            RadioModel before;
            connectDesktop(before, 2, 5, {kPan0, kPan1});
            before.flushPendingSettingsSave();
        }
        AppSettings::instance().setValue(
            QStringLiteral("hardware/%1/ActiveSliceId")
                .arg(AppSettings::normalizedRadioMac(kDesktopMac)), 3);
        RadioModel after;
        connectDesktop(after, 2, 5, {kPan0, kPan1});
        QCOMPARE(after.activeSlice(), after.sliceById(0));
    }

    // A reconnect keeps what is on screen, whatever was last saved.
    void aReconnectKeepsTheActiveSliceOnScreen()
    {
        RadioModel model;
        connectDesktop(model, 2, 5, {kPan0, kPan1});
        QVERIFY(model.setActiveSliceById(1));
        model.flushPendingSettingsSave();
        QVERIFY(model.setActiveSliceById(0));

        QCOMPARE(model.addStartingSlices(kDesktopMac), -1);
        model.configureStreamPool(2, 5, kRateHz);
        model.bindUnboundSlices();
        MainWindow::populatePanSlices(&model, {kPan0, kPan1}, false, false);
        QCOMPARE(model.activeSlice(), model.sliceById(0));
    }
};

QTEST_MAIN(TestStandaloneSliceRestore)
#include "tst_standalone_slice_restore.moc"
