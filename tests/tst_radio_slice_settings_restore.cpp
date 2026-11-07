// no-port-check: NereusSDR-original test.
//
// tst_radio_slice_settings_restore.cpp
//
// RadioModel's half of "settings come back after a restart" (settings
// restore audit 2026-10-11):
//
//   * a change is saved under the slice it was made on, not under the
//     active slice (gap 5), and that includes AGC-T AUTO and its offset,
//     which scheduled no save at all (gap 2), and the FM, DIG and RTTY
//     values, which had no save trigger either (gap 1);
//   * a slice restored before its WDSP channel exists pushes its whole
//     restored state to that channel when the channel is switched on, the
//     way slice A's is pushed at connect (gap 4).
//
// No radio and no audio device: a connection that sends nothing, and WDSP
// channels opened through the same seam tst_notch_channel_sync uses. The
// settings store is the test sandbox's (TestSandboxInit).
//
// Modification history (NereusSDR):
//   2026-11-05 - A newly added receiver starts with RIT and XIT off, on a
//                letter used before too; a restored one keeps its own.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.

#include <QtTest/QtTest>

#include <QDir>
#include <QStandardPaths>

#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/DspControlThread.h"
#include "core/HpsdrModel.h"
#include "core/daemon/DaemonAgcSource.h"
#include "core/RadioConnection.h"
#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspEngine.h"
#include "core/WdspTypes.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "core/SliceOwnership.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

// File scope, not an anonymous namespace: moc cannot generate a
// staticMetaObject for a Q_OBJECT class with internal linkage.
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

constexpr int    kRateHz       = 192000;
constexpr double kSliceAFreqHz = 14200000.0;
constexpr double kSliceBFreqHz = 7100000.0;

// Detaches a stack-injected connection however the scope is left.
struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

bool laneIdle(RadioModel& model)
{
    return model.receiveLane() == nullptr || model.receiveLane()->waitIdleForTest(600000);
}

QString sliceKey(int index, const char* key)
{
    return QStringLiteral("Slice%1/").arg(index) + QLatin1String(key);
}

} // namespace

class TestRadioSliceSettingsRestore : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── A change is saved under the slice it was made on ────────────────

    void aChangeToANonActiveSliceIsSavedUnderThatSlice_data()
    {
        QTest::addColumn<QByteArray>("property");
        QTest::addColumn<QVariant>("value");
        QTest::addColumn<QString>("key");
        QTest::addColumn<QString>("stored");

        const auto row = [](const char* property, const QVariant& value, const char* key,
                            const char* stored) {
            QTest::newRow(property) << QByteArray(property) << value
                                    << QString::fromLatin1(key) << QString::fromLatin1(stored);
        };
        // Handlers that saved the active slice instead.
        row("afGain",       33,    "AfGain",       "33");
        row("rfGain",       61,    "RfGain",       "61");
        row("muted",        true,  "Muted",        "True");
        row("locked",       true,  "Locked",       "True");
        row("ritHz",        150,   "RitHz",        "150");
        row("snbEnabled",   true,  "SnbEnabled",   "True");
        row("anfEnabled",   true,  "AnfEnabled",   "True");
        row("nb1Threshold", 42,    "Nb1Threshold", "42");
        row("audioPan",     -0.5,  "AudioPan",     "-0.5");
        row("ssqlEnabled",  true,  "SsqlEnabled",  "True");
        row("apfTuneHz",    120,   "ApfTuneHz",    "120");
        row("binauralEnabled", true, "BinauralEnabled", "True");
        // AGC-T AUTO and its offset scheduled no save at all.
        row("autoAgcEnabled", true, "AgcAutoEnabled", "True");
        row("autoAgcOffset",  12.5, "AgcAutoOffset",  "12.5");
        // Neither did the FM, DIG and RTTY values.
        row("fmCtcssMode",    1,      "FmCtcssMode",    "1");
        row("fmCtcssValueHz", 123.0,  "FmCtcssValueHz", "123");
        row("fmOffsetHz",     600000, "FmOffsetHz",     "600000");
        row("fmTxMode",       QVariant::fromValue(FmTxMode::High), "FmTxMode", "0");
        row("fmReverse",      true,   "FmReverse",      "True");
        row("diglOffsetHz",   1500,   "DiglOffsetHz",   "1500");
        row("diguOffsetHz",   -1200,  "DiguOffsetHz",   "-1200");
        row("rttyMarkHz",     2125,   "RttyMarkHz",     "2125");
        row("rttyShiftHz",    850,    "RttyShiftHz",    "850");
        // Diversity's blend scheduled no save either.
        row("diversityPhaseDeg", 73.0, "Band20m/DiversityPhaseDeg", "73");
        row("diversityGainDb",   -6.5, "Band20m/DiversityGainDb",   "-6.5");
        row("diversityFineNullEnabled", true, "Band20m/DiversityFineNullEnabled", "True");
    }

    void aChangeToANonActiveSliceIsSavedUnderThatSlice()
    {
        QFETCH(QByteArray, property);
        QFETCH(QVariant, value);
        QFETCH(QString, key);
        QFETCH(QString, stored);

        RadioModel radio;
        SilentRadioConnection conn;
        radio.injectConnectionForTest(&conn);
        DetachConnection detach{&radio};

        SliceModel* a = radio.sliceById(radio.addSlice());
        SliceModel* b = radio.sliceById(radio.addSlice());
        QVERIFY(a);
        QVERIFY(b);
        QCOMPARE(radio.activeSlice(), a);
        QCOMPARE(b->sliceIndex(), 1);
        // Anything the two adds scheduled is written and out of the way.
        radio.flushPendingSettingsSave();
        AppSettings::instance().clear();

        QVERIFY2(b->setProperty(property.constData(), value), property.constData());
        radio.flushPendingSettingsSave();

        auto& s = AppSettings::instance();
        const QString own = QStringLiteral("Slice1/") + key;
        QVERIFY2(s.contains(own), qPrintable(own + QStringLiteral(" was not saved")));
        QCOMPARE(s.value(own).toString(), stored);
        // The active slice did not change and is not what gets written.
        QVERIFY2(!s.contains(QStringLiteral("Slice0/LastBand")),
                 "the change was saved under the active slice");
    }

    // A manual AGC-T change still turns AUTO off (Thetis), and that is
    // saved for the same slice.
    void aManualThresholdChangeTurnsAutoOffAndSavesIt()
    {
        RadioModel radio;
        SilentRadioConnection conn;
        radio.injectConnectionForTest(&conn);
        DetachConnection detach{&radio};

        radio.addSlice();
        SliceModel* b = radio.sliceById(radio.addSlice());
        QVERIFY(b);
        b->setAutoAgcEnabled(true);
        radio.flushPendingSettingsSave();
        QCOMPARE(AppSettings::instance().value(sliceKey(1, "AgcAutoEnabled")).toString(),
                 QStringLiteral("True"));

        b->setAgcThreshold(b->agcThreshold() - 7);
        QVERIFY(!b->autoAgcEnabled());
        radio.flushPendingSettingsSave();
        QCOMPARE(AppSettings::instance().value(sliceKey(1, "AgcAutoEnabled")).toString(),
                 QStringLiteral("False"));
    }

    // The AUTO tick writes the threshold twice a second. Its own writes
    // must schedule no save and must not turn AUTO off: only what the
    // operator changes is saved.
    void theAutoTicksOwnThresholdUpdateSchedulesNoSave()
    {
#ifndef HAVE_FFTW3
        QSKIP("FFTEngine has no FFTW3 backend in this build");
#endif
        qRegisterMetaType<QVector<float>>();
        qRegisterMetaType<MediaSourceKey>();
        RadioModel radio;
        SilentRadioConnection conn;
        radio.injectConnectionForTest(&conn);
        DetachConnection detach{&radio};
        radio.setBoardForTest(HPSDRHW::Saturn);
        radio.configureStreamPool(5, 5, 192000);
        radio.setConnectionStateForTest(ConnectionState::Connected);

        radio.addSlice(QStringLiteral("tick-pan-a"));
        SliceModel* b = radio.sliceById(radio.addSlice(QStringLiteral("tick-pan-b")));
        QVERIFY(b);
        QVERIFY(b->streamIndex() >= 0);
        // The station's AGC source measures each stream's noise floor.
        DaemonAgcSource source(&radio);
        QTRY_COMPARE(source.activeStreamCount(), 2);

        // A noise floor for B's own stream (the tick skips a slice without
        // one). Fixed broad-band pseudo-noise, as tst_daemon_agc_source.
        QVector<float> noise;
        noise.reserve(4096 * 2);
        quint32 state = 0x4d595df4U;
        for (int i = 0; i < 4096 * 2; ++i) {
            state = state * 1664525U + 1013904223U;
            noise.append((static_cast<float>((state >> 8) & 0xffffU) / 32767.5f - 1.0f) * 0.20f);
        }
        for (int frame = 0; frame < 90 && !b->stationAutoAgcNoiseFloorValid(); ++frame) {
            radio.forkIqToTapsForTest(b->streamIndex(), noise);
            QTest::qWait(35);
        }
        QVERIFY(b->stationAutoAgcNoiseFloorValid());

        // A threshold the tick will certainly move, then AUTO on, and all
        // of that written.
        b->setAgcThreshold(-42);
        b->setAutoAgcEnabled(true);
        radio.flushPendingSettingsSave();
        QString thresholdKey;
        for (const QString& k : AppSettings::instance().allKeys()) {
            if (k.startsWith(QStringLiteral("Slice1/Band")) && k.endsWith(QStringLiteral("/AgcThreshold"))) {
                thresholdKey = k;
            }
        }
        QVERIFY(!thresholdKey.isEmpty());
        QCOMPARE(AppSettings::instance().value(thresholdKey).toInt(), -42);

        radio.runAutoAgcTickForTest();
        QVERIFY(b->agcThreshold() != -42);   // the tick did write
        QVERIFY(b->autoAgcEnabled());        // and it is not a manual change

        // Longer than the 500 ms save timer: nothing was scheduled, so
        // what is saved is still what the operator set.
        QTest::qWait(900);
        QCOMPARE(AppSettings::instance().value(thresholdKey).toInt(), -42);
        QCOMPARE(AppSettings::instance().value(sliceKey(1, "AgcAutoEnabled")).toString(),
                 QStringLiteral("True"));
    }

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

    // ── Lock on at quit ─────────────────────────────────────────────────

    // A desktop's starting slice is restored twice: addSlice() brings back
    // its preferences, Locked included, with no tuning, and loadSliceState
    // then restores the last band. The lock from the first must not veto
    // the frequency in the second, or the slice comes back on the
    // constructor's 14.225 MHz and the next save damages the 20 m slot.
    void aLockedSliceComesBackOnItsSavedFrequency()
    {
        auto& s = AppSettings::instance();
        {
            SliceModel slice;
            slice.setSliceIndex(0);
            slice.setFrequency(7'074'000.0);
            slice.setDspMode(DSPMode::LSB);
            slice.setLocked(true);
            slice.saveToSettings(Band::Band40m);
        }
        QVERIFY(s.contains(QStringLiteral("Slice0/Band40m/Frequency")));

        RadioModel radio;
        SliceModel slice;
        slice.setSliceIndex(0);
        slice.restoreFromSettings(Band::Band40m, false);
        QVERIFY(slice.locked());
        QCOMPARE_NE(slice.frequency(), 7'074'000.0);
        radio.loadSliceStateForTest(&slice);

        QCOMPARE(slice.frequency(), 7'074'000.0);
        QVERIFY(slice.locked());
        // And a lock still stops a tune.
        slice.setFrequency(7'100'000.0);
        QCOMPARE(slice.frequency(), 7'074'000.0);
    }

    // ── The transmit filter through RADE ────────────────────────────────

    void theTransmitFilterComesBackAfterRade()
    {
        const QString mac = QStringLiteral("aa:bb:cc:dd:ee:01");
        RadioModel radio;
        TransmitModel& tx = radio.transmitModel();
        tx.loadFromSettings(mac);
        tx.setFilterLow(200);
        tx.setFilterHigh(2800);

        radio.snapTransmitFilterForModeForTest(DSPMode::RADE_U);
        QCOMPARE(tx.filterLow(), 650);
        QCOMPARE(tx.filterHigh(), 2350);

        radio.snapTransmitFilterForModeForTest(DSPMode::USB);
        QCOMPARE(tx.filterLow(), 200);
        QCOMPARE(tx.filterHigh(), 2800);
        // And on the disk's side too; nothing is left waiting.
        TransmitModel reloaded;
        reloaded.loadFromSettings(mac);
        QCOMPARE(reloaded.filterLow(), 200);
        QCOMPARE(reloaded.filterHigh(), 2800);
        QVERIFY(!reloaded.filterBeforeRade().has_value());
    }

    void theFilterBeforeRadeSurvivesARestartInRade()
    {
        const QString mac = QStringLiteral("aa:bb:cc:dd:ee:02");
        {
            RadioModel radio;
            TransmitModel& tx = radio.transmitModel();
            tx.loadFromSettings(mac);
            tx.setFilterLow(200);
            tx.setFilterHigh(2800);
            radio.snapTransmitFilterForModeForTest(DSPMode::RADE_U);
        }
        RadioModel restarted;
        TransmitModel& tx = restarted.transmitModel();
        tx.loadFromSettings(mac);
        QCOMPARE(tx.filterLow(), 650);
        QCOMPARE(tx.filterHigh(), 2350);
        // A slice restored in RADE snaps once more at connect, and that must
        // not replace the kept filter with RADE's own passband.
        restarted.snapTransmitFilterForModeForTest(DSPMode::RADE_U);
        restarted.snapTransmitFilterForModeForTest(DSPMode::USB);
        QCOMPARE(tx.filterLow(), 200);
        QCOMPARE(tx.filterHigh(), 2800);
    }

    void aFilterEditedInRadeStaysAfterLeaving()
    {
        const QString mac = QStringLiteral("aa:bb:cc:dd:ee:03");
        RadioModel radio;
        TransmitModel& tx = radio.transmitModel();
        tx.loadFromSettings(mac);
        tx.setFilterLow(200);
        tx.setFilterHigh(2800);
        radio.snapTransmitFilterForModeForTest(DSPMode::RADE_U);
        tx.setFilterLow(700);
        tx.setFilterHigh(2200);

        radio.snapTransmitFilterForModeForTest(DSPMode::USB);
        QCOMPARE(tx.filterLow(), 700);
        QCOMPARE(tx.filterHigh(), 2200);
        TransmitModel reloaded;
        reloaded.loadFromSettings(mac);
        QVERIFY(!reloaded.filterBeforeRade().has_value());
    }

    void aFilterAboveThePassbandComesBackWhole()
    {
        // setFilterLow swaps when it would pass the current high, so 2400 to
        // 2900 put back over 650 to 2350 must not come out as 2350 to 2900,
        // and 100 to 500 entering RADE must not come out as 500 to 2350.
        const QString mac = QStringLiteral("aa:bb:cc:dd:ee:04");
        RadioModel radio;
        TransmitModel& tx = radio.transmitModel();
        tx.loadFromSettings(mac);
        tx.setFilterHigh(2900);
        tx.setFilterLow(2400);
        radio.snapTransmitFilterForModeForTest(DSPMode::RADE_U);
        radio.snapTransmitFilterForModeForTest(DSPMode::USB);
        QCOMPARE(tx.filterLow(), 2400);
        QCOMPARE(tx.filterHigh(), 2900);

        tx.setFilterLow(100);
        tx.setFilterHigh(500);
        radio.snapTransmitFilterForModeForTest(DSPMode::RADE_U);
        QCOMPARE(tx.filterLow(), 650);
        QCOMPARE(tx.filterHigh(), 2350);
        radio.snapTransmitFilterForModeForTest(DSPMode::USB);
        QCOMPARE(tx.filterLow(), 100);
        QCOMPARE(tx.filterHigh(), 500);
    }

    // A filter already at the passband with nothing kept (saved before the
    // filter was kept, or the operator's own) leaves as it always has.
    void aPassbandWithNothingKeptLeavesAsBefore()
    {
        const QString mac = QStringLiteral("aa:bb:cc:dd:ee:05");
        RadioModel radio;
        TransmitModel& tx = radio.transmitModel();
        tx.loadFromSettings(mac);
        tx.setFilterLow(650);
        tx.setFilterHigh(2350);
        radio.snapTransmitFilterForModeForTest(DSPMode::RADE_U);
        QVERIFY(!tx.filterBeforeRade().has_value());
        radio.snapTransmitFilterForModeForTest(DSPMode::USB);
        QCOMPARE(tx.filterLow(), 100);
        QCOMPARE(tx.filterHigh(), 3900);
    }

    // ── A restored slice runs on what it shows ──────────────────────────

    // Slice B is restored while it has no WDSP channel (as the Core's
    // startup layout and a device's slice made again are), so no live push
    // reaches a channel. When its channel is switched on it must carry the
    // restored state, not WDSP's defaults.
    void aRestoredSliceBPushesItsStateToItsChannel()
    {
        // What slice B was saved with.
        {
            SliceModel saved;
            saved.setSliceIndex(1);
            saved.setFrequency(kSliceBFreqHz);
            saved.setAgcThreshold(-77);
            saved.setAgcHang(123);
            saved.setAgcSlope(3);
            saved.setAgcAttack(4);
            saved.setAgcDecay(321);
            saved.setAgcHangThreshold(55);
            saved.setAgcFixedGain(17);
            saved.setAgcMaxGain(88);
            saved.setNbMode(NbMode::NB);
            saved.setSnbEnabled(true);
            saved.setAnfEnabled(true);
            saved.setMuted(true);
            saved.setBinauralEnabled(true);
            saved.setApfEnabled(true);
            saved.setSsqlEnabled(true);
            saved.saveToSettings(Band::Band40m);
        }

        QTemporaryDir config;
        QVERIFY(config.isValid());
        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        QVERIFY(engine != nullptr);
        engine->setSynchronousInitForTest(true);
        QVERIFY(engine->initialize(config.path()));

        model.configureStreamPool(2, 2, kRateHz);
        const int a = model.addSlice();
        QVERIFY(a >= 0);
        model.sliceById(a)->setFrequency(kSliceAFreqHz);
        const int b = model.addSlice();
        QCOMPARE(b, 1);
        SliceModel* sliceB = model.sliceById(b);
        QVERIFY(sliceB);
        QVERIFY(engine->rxChannel(b) == nullptr);

        // Restored to the model before any channel exists.
        sliceB->restoreFromSettings(Band::Band40m);
        QCOMPARE(sliceB->agcThreshold(), -77);
        QCOMPARE(sliceB->agcHang(), 123);
        QCOMPARE(sliceB->agcDecay(), 321);
        QVERIFY(sliceB->streamIndex() >= 0);

        // The pool opens and switches on every bound slice's channel.
        model.openRxChannelPool(2, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(laneIdle(model));

        RxChannel* ch = engine->rxChannel(b);
        QVERIFY(ch != nullptr);
        QVERIFY(ch->isActive());
        QCOMPARE(ch->agcThreshold(), -77);
        QCOMPARE(ch->agcHang(), sliceB->agcHang());
        QCOMPARE(ch->agcSlope(), sliceB->agcSlope());
        QCOMPARE(ch->agcAttack(), sliceB->agcAttack());
        QCOMPARE(ch->agcDecay(), sliceB->agcDecay());
        QCOMPARE(ch->agcHangThreshold(), sliceB->agcHangThreshold());
        QCOMPARE(ch->agcFixedGain(), sliceB->agcFixedGain());
        QCOMPARE(ch->agcMaxGain(), sliceB->agcMaxGain());
        QCOMPARE(ch->nbMode(), NbMode::NB);
        QVERIFY(ch->snbEnabled());
        // ANF had a live push only, so a restored ANF never reached a
        // channel, slice A's included.
        QVERIFY(ch->captureState().anfEnabled);
        QVERIFY(ch->muted());
        QVERIFY(ch->binauralEnabled());
        QVERIFY(ch->apfEnabled());
        QVERIFY(ch->ssqlEnabled());
        QVERIFY(laneIdle(model));
    }

    // JJ's ruling 2026-11-05: a newly added receiver starts with RIT and XIT
    // off, on a letter used before too. A restored receiver keeps its own.

    void newReceiverOnAReusedLetterStartsWithRitAndXitOff()
    {
        saveLetterWithOffsets();
        if (QTest::currentTestFailed()) { return; }
        RadioModel radio;
        SliceModel* slice = radio.sliceById(radio.addSlice(QString(), false, /*newReceiver=*/true));
        QVERIFY(slice);
        QCOMPARE(slice->sliceIndex(), 0);
        QVERIFY(!slice->ritEnabled());
        QCOMPARE(slice->ritHz(), 0);
        QVERIFY(!slice->xitEnabled());
        QCOMPARE(slice->xitHz(), 0);
        QCOMPARE(radio.txFrequencyForSlice(slice), static_cast<quint64>(slice->frequency()));
        // With no slice to copy, it opens on defaults, not the letter's.
        QCOMPARE(slice->afGain(), kDefaultAfGain);
        // Turning XIT on later cannot jump to the old offset.
        slice->setXitEnabled(true);
        QCOMPARE(radio.txFrequencyForSlice(slice), static_cast<quint64>(slice->frequency()));
    }

    void newReceiverAddedOnAPanStartsWithRitAndXitOff()
    {
        saveLetterWithOffsets();
        if (QTest::currentTestFailed()) { return; }
        RadioModel radio;
        radio.addSliceOnPan(QStringLiteral("pan-0"), /*restoreSavedState=*/false,
                            /*newReceiver=*/true);
        SliceModel* slice = radio.sliceById(0);
        QVERIFY(slice);
        QVERIFY(!slice->ritEnabled());
        QVERIFY(!slice->xitEnabled());
    }

    void newReceiverLeavesNoOldOffsetForTheNextStart()
    {
        // A receiver added and then left alone saves nothing of its own. The
        // old offsets must not come back on it at the next start.
        saveLetterWithOffsets();
        if (QTest::currentTestFailed()) { return; }
        {
            RadioModel radio;
            QVERIFY(radio.addSlice(QString(), false, /*newReceiver=*/true) >= 0);
        }
        RadioModel restarted;
        SliceModel* slice = restarted.sliceById(
            restarted.addSlice(QString(), /*restoreSavedState=*/true));
        QVERIFY(slice);
        QVERIFY(!slice->ritEnabled());
        QCOMPARE(slice->ritHz(), 0);
        QVERIFY(!slice->xitEnabled());
        QCOMPARE(slice->xitHz(), 0);
    }

    void newReceiverSchedulesASaveSoACrashCannotBringOffsetsBack()
    {
        saveLetterWithOffsets();
        if (QTest::currentTestFailed()) { return; }
        // The file holds the old offsets, as a settings file does after the
        // last session's save.
        AppSettings::instance().save();
        {
            AppSettings before(AppSettings::instance().filePath());
            before.load();
            QCOMPARE(before.value(QStringLiteral("Slice0/XitEnabled")).toString(),
                     QStringLiteral("True"));
        }
        RadioModel radio;
        QVERIFY(radio.addSlice(QString(), false, /*newReceiver=*/true) >= 0);
        // The save the forget asked for reaches the settings file, as the
        // 500 ms timer's would.
        radio.flushPendingSettingsSave();
        AppSettings onDisk(AppSettings::instance().filePath());
        onDisk.load();
        QVERIFY(onDisk.value(QStringLiteral("Slice0/XitEnabled")).toString()
                != QStringLiteral("True"));
        QVERIFY(onDisk.value(QStringLiteral("Slice0/RitEnabled")).toString()
                != QStringLiteral("True"));
        QCOMPARE(onDisk.value(QStringLiteral("Slice0/XitHz"), 0).toInt(), 0);
    }

    void newReceiverBeforeConnectLeavesTheLettersBandMemoryAlone()
    {
        // Ctrl+R or +RX with no radio connected. With no active slice to seed
        // from, the slice sits on the constructor's frequency, which is not
        // the operator's: it must not reach the letter's band memory, or
        // connect lands there instead of where the letter was saved.
        {
            RadioModel first;
            SliceModel* saved = first.sliceById(first.addSlice());
            QVERIFY(saved);
            saved->setFrequency(14'100'000.0);
            saved->saveToSettings(Band::Band20m);
            saved->setFrequency(7'074'000.0);
            saved->setXitHz(700);
            saved->setXitEnabled(true);
            saved->saveToSettings(Band::Band40m);
            first.flushPendingSettingsSave();
        }
        auto& settings = AppSettings::instance();
        settings.setValue(QStringLiteral("Slice0/LastBand"), QStringLiteral("40m"));
        settings.setValue(QStringLiteral("Slice0/Band20m/Frequency"), 14'100'000.0);
        settings.setValue(QStringLiteral("Slice0/Band40m/Frequency"), 7'074'000.0);

        RadioModel radio;
        SliceModel* slice = radio.sliceById(radio.addSlice(QString(), false, /*newReceiver=*/true));
        QVERIFY(slice);
        QVERIFY(!slice->xitEnabled());
        radio.flushPendingSettingsSave();
        QCOMPARE(settings.value(QStringLiteral("Slice0/LastBand")).toString(),
                 QStringLiteral("40m"));
        QCOMPARE(settings.value(QStringLiteral("Slice0/Band20m/Frequency")).toDouble(),
                 14'100'000.0);
        QCOMPARE(settings.value(QStringLiteral("Slice0/Band40m/Frequency")).toDouble(),
                 7'074'000.0);
        // And the removal still went out with that save.
        QVERIFY(!settings.contains(QStringLiteral("Slice0/XitHz")));
        radio.loadSliceStateForTest(slice);
        QCOMPARE(slice->frequency(), 7'074'000.0);
    }

    void aRefusedAddKeepsTheLettersOffsets()
    {
        // A new pan with no receiver left: a normal outcome on a 2-DDC HL2.
        // No receiver was made, so the letter loses nothing.
        RadioModel radio;
        radio.configureStreamPool(1, 2, kRateHz);
        const int a = radio.addSlice(QStringLiteral("pan-a"));
        QVERIFY(a >= 0);
        radio.flushPendingSettingsSave();
        auto& settings = AppSettings::instance();
        const int refusedId = a + 1;
        const QString prefix = QStringLiteral("Slice%1/").arg(refusedId);
        settings.setValue(prefix + QStringLiteral("LastBand"), QStringLiteral("20m"));
        settings.setValue(prefix + QStringLiteral("RitEnabled"), QStringLiteral("True"));
        settings.setValue(prefix + QStringLiteral("RitHz"), 250);
        settings.setValue(prefix + QStringLiteral("XitEnabled"), QStringLiteral("True"));
        settings.setValue(prefix + QStringLiteral("XitHz"), 700);
        // The default mode's memory on the band the seed is on, which the
        // seed's mode change used to write the constructor's filter over.
        SliceModel* openSlice = radio.sliceById(a);
        QVERIFY(openSlice);
        openSlice->setDspMode(DSPMode::CWU);
        const QString filterLowBefore = prefix
            + QStringLiteral("Band%1/Mode%2/FilterLow")
                  .arg(bandKeyName(bandFromFrequency(openSlice->frequency())),
                       SliceModel::modeName(DSPMode::USB));
        settings.setValue(filterLowBefore, 1234);

        QSignalSpy rejected(&radio, &RadioModel::sliceAddRejected);
        radio.addSliceOnPan(QStringLiteral("pan-b"), /*restoreSavedState=*/false,
                            /*newReceiver=*/true);
        QCOMPARE(rejected.count(), 1);
        QVERIFY(radio.sliceById(refusedId) == nullptr);
        QCOMPARE(settings.value(prefix + QStringLiteral("RitEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(settings.value(prefix + QStringLiteral("RitHz")).toInt(), 250);
        QCOMPARE(settings.value(prefix + QStringLiteral("XitEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(settings.value(prefix + QStringLiteral("XitHz")).toInt(), 700);
        QVERIFY(!radio.hasPendingBandSettingsForTest(refusedId));
        // Nor did the attempt write the letter's per-mode filter memory.
        QCOMPARE(settings.value(filterLowBefore).toInt(), 1234);
    }

    void removingAReceiverThenAddingItAgainStartsWithRitAndXitOff()
    {
        RadioModel radio;
        radio.configureStreamPool(2, 2, kRateHz);
        QVERIFY(radio.addSlice() >= 0);
        const int id = radio.addSlice();
        SliceModel* first = radio.sliceById(id);
        QVERIFY(first);
        first->setXitHz(700);
        first->setXitEnabled(true);
        first->setRitHz(250);
        first->setRitEnabled(true);
        radio.flushPendingSettingsSave();
        radio.removeSlice(id);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(radio.sliceById(id) == nullptr);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Slice%1/XitHz").arg(id)).toInt(), 700);

        const int again = radio.addSlice(QString(), false, /*newReceiver=*/true);
        QCOMPARE(again, id);
        SliceModel* second = radio.sliceById(again);
        QVERIFY(second);
        QVERIFY(!second->ritEnabled());
        QCOMPARE(second->ritHz(), 0);
        QVERIFY(!second->xitEnabled());
        QCOMPARE(second->xitHz(), 0);
    }

    void aPathThatDoesNotSayNewKeepsTheLettersRitAndXit()
    {
        // The default: a caller nobody thought about loses nothing.
        saveLetterWithOffsets();
        if (QTest::currentTestFailed()) { return; }
        RadioModel radio;
        SliceModel* slice = radio.sliceById(radio.addSlice());
        verifyOffsetsKept(radio, slice);
        if (QTest::currentTestFailed()) { return; }
        QCOMPARE(slice->afGain(), kSavedAfGain);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Slice0/XitHz")).toInt(), 700);
    }

    void startupRestoreOfALetterKeepsItsRitAndXit()
    {
        // MainWindow's saved pans and the Core's startup ask for this.
        saveLetterWithOffsets();
        if (QTest::currentTestFailed()) { return; }
        RadioModel radio;
        SliceModel* slice = radio.sliceById(
            radio.addSlice(QString(), /*restoreSavedState=*/true));
        verifyOffsetsKept(radio, slice);
    }

    void startingSliceAtConnectKeepsItsRitAndXit()
    {
        // The window's slice at connect goes through addSlice() with no
        // restore asked for, and is a restored receiver all the same. It keeps
        // the values before loadSliceState, for the connect that skips that,
        // and after it. The saved values stay for the next start.
        saveLetterWithOffsets();
        if (QTest::currentTestFailed()) { return; }
        RadioModel radio;
        const int id = radio.addStartingSlices(QString());
        SliceModel* slice = radio.sliceById(id);
        verifyOffsetsKept(radio, slice);
        if (QTest::currentTestFailed()) { return; }
        radio.loadSliceStateForTest(slice);
        verifyOffsetsKept(radio, slice);
        if (QTest::currentTestFailed()) { return; }
        QVERIFY(AppSettings::instance().contains(QStringLiteral("Slice0/XitEnabled")));
    }

    void deviceSliceMadeAgainKeepsItsRitAndXit()
    {
        // restoreSliceFor, a device's saved slice coming back.
        saveLetterWithOffsets();
        if (QTest::currentTestFailed()) { return; }
        RadioModel radio;
        radio.configureStreamPool(2, 2, 192000);
        ReceiveSliceState state;
        state.id = 0;
        state.frequencyHz = 14'200'000.0;
        state.dspMode = DSPMode::USB;
        QString reason;
        const int id = radio.restoreSliceFor(QByteArrayLiteral("device"), 0, state, &reason);
        QVERIFY2(id >= 0, qPrintable(reason));
        verifyOffsetsKept(radio, radio.sliceById(id));
    }

    void bandRestoreReadsRitAndXitFromTheStore()
    {
        // The values must come from the store: the live ones change between
        // the save and the restore.
        RadioModel radio;
        SliceModel* slice = radio.sliceById(radio.addSlice());
        QVERIFY(slice);
        slice->setFrequency(14'200'000.0);
        slice->setRitHz(250);
        slice->setRitEnabled(true);
        slice->setXitHz(700);
        slice->setXitEnabled(true);
        slice->saveToSettings(Band::Band20m);
        slice->setRitEnabled(false);
        slice->setRitHz(0);
        slice->setXitEnabled(false);
        slice->setXitHz(0);
        slice->restoreFromSettings(Band::Band20m);
        verifyOffsetsKept(radio, slice);
    }

    void aBandChangeKeepsAReceiversRitAndXit()
    {
        // The real band button: saves the slice, then moves and back.
        RadioModel radio;
        SliceModel* slice = radio.sliceById(radio.addSlice());
        QVERIFY(slice);
        slice->setFrequency(14'200'000.0);
        slice->setRitHz(250);
        slice->setRitEnabled(true);
        slice->setXitHz(700);
        slice->setXitEnabled(true);
        radio.onBandButtonClicked(slice, Band::Band40m);
        QVERIFY(slice->frequency() < 8'000'000.0);
        verifyOffsetsKept(radio, slice);
        if (QTest::currentTestFailed()) { return; }
        radio.onBandButtonClicked(slice, Band::Band20m);
        QCOMPARE(slice->frequency(), 14'200'000.0);
        verifyOffsetsKept(radio, slice);
    }

    // ── +RX: a new slice is a copy of the slice you are on (JJ, 2026-11-05) ──

    void aNewReceiverCopiesEachValueFromTheSliceYouAreOn()
    {
        RadioModel radio;
        SliceModel* seed = radio.sliceById(radio.addSlice());
        QVERIFY(seed);
        setSeedValues(*seed);
        SliceModel* slice = radio.sliceById(radio.addSlice(QString(), false, /*newReceiver=*/true));
        QVERIFY(slice);
        QVERIFY(slice != seed);
        verifyCopyOfSeedValues(*slice);
        if (QTest::currentTestFailed()) { return; }
        QCOMPARE(slice->frequency(), seed->frequency());
        QCOMPARE(slice->dspMode(), seed->dspMode());
    }

    void aNewReceiverLeavesTheSeedAsItWas()
    {
        RadioModel radio;
        SliceModel* seed = radio.sliceById(radio.addSlice());
        QVERIFY(seed);
        setSeedValues(*seed);
        const double frequency = seed->frequency();
        QVERIFY(radio.addSlice(QString(), false, /*newReceiver=*/true) >= 0);
        radio.flushPendingSettingsSave();
        verifyCopyOfSeedValues(*seed);
        if (QTest::currentTestFailed()) { return; }
        QCOMPARE(seed->frequency(), frequency);
        QVERIFY(seed->locked());
        QVERIFY(seed->muted());
        QVERIFY(seed->ritEnabled());
        QCOMPARE(seed->ritHz(), 300);
        QVERIFY(seed->xitEnabled());
        QCOMPARE(seed->xitHz(), 400);
    }

    void aNewReceiverDoesNotCopyLockMuteRitOrXit()
    {
        RadioModel radio;
        SliceModel* seed = radio.sliceById(radio.addSlice());
        QVERIFY(seed);
        setSeedValues(*seed);
        SliceModel* slice = radio.sliceById(radio.addSlice(QString(), false, /*newReceiver=*/true));
        QVERIFY(slice);
        QVERIFY(!slice->locked());
        QVERIFY(!slice->muted());
        QVERIFY(!slice->ritEnabled());
        QCOMPARE(slice->ritHz(), 0);
        QVERIFY(!slice->xitEnabled());
        QCOMPARE(slice->xitHz(), 0);
    }

    void aReusedLetterBringsNoneOfItsOwnSettings()
    {
        saveLetterCOn20m();
        if (QTest::currentTestFailed()) { return; }
        RadioModel radio;
        SliceModel* seed = openSeedOn40m(radio);
        QVERIFY(seed);
        const int c = radio.addSlice(QString(), false, /*newReceiver=*/true);
        QCOMPARE(c, 2);
        SliceModel* slice = radio.sliceById(c);
        QVERIFY(slice);
        verifyCopyOfSeedValues(*slice);
        if (QTest::currentTestFailed()) { return; }
        QVERIFY(!slice->locked());
        QVERIFY(!slice->muted());
        QVERIFY(!slice->ritEnabled());
        QVERIFY(!slice->xitEnabled());
        QCOMPARE(slice->frequency(), seed->frequency());
    }

    void aReusedLettersBandMemorySurvivesTheCopysSave()
    {
        saveLetterCOn20m();
        if (QTest::currentTestFailed()) { return; }
        RadioModel radio;
        QVERIFY(openSeedOn40m(radio));
        QCOMPARE(radio.addSlice(QString(), false, /*newReceiver=*/true), 2);
        radio.flushPendingSettingsSave();
        auto& settings = AppSettings::instance();
        const auto band20m = [](const char* key) {
            return QStringLiteral("Slice2/Band20m/") + QLatin1String(key);
        };
        QCOMPARE(settings.value(band20m("Frequency")).toDouble(), kLetterFrequencyHz);
        QCOMPARE(settings.value(band20m("AgcThreshold")).toInt(), kLetterAgcThreshold);
        QCOMPARE(settings.value(band20m("AgcHang")).toInt(), kLetterAgcHang);
        QCOMPARE(settings.value(band20m("AgcMode")).toInt(), static_cast<int>(AGCMode::Long));
        QCOMPARE(settings.value(band20m("NbMode")).toInt(), static_cast<int>(NbMode::NB2));
        // The copy saved under its own band, on its own letter.
        QCOMPARE(settings.value(QStringLiteral("Slice2/LastBand")).toString(),
                 QStringLiteral("40m"));
        QCOMPARE(settings.value(QStringLiteral("Slice2/Band40m/AgcThreshold")).toInt(),
                 kSeedAgcThreshold);
        // The letter's 20 m comes back when the operator goes there, and the
        // copy's lock and mute (off) stay.
        SliceModel* slice = radio.sliceById(2);
        QVERIFY(slice);
        radio.onBandButtonClicked(slice, Band::Band20m);
        QCOMPARE(slice->frequency(), kLetterFrequencyHz);
        QCOMPARE(slice->agcThreshold(), kLetterAgcThreshold);
        QCOMPARE(slice->nbMode(), NbMode::NB2);
        QVERIFY(!slice->locked());
        QVERIFY(!slice->muted());
    }

    void anOperatorChangeAfterTheCopySavesInTheNormalWay()
    {
        RadioModel radio;
        SliceModel* seed = radio.sliceById(radio.addSlice());
        QVERIFY(seed);
        seed->setFrequency(7'100'000.0);
        SliceModel* slice = radio.sliceById(radio.addSlice(QString(), false, /*newReceiver=*/true));
        QVERIFY(slice);
        slice->setAgcThreshold(-37);
        slice->saveToSettings(Band::Band40m);
        const QString own = QStringLiteral("Slice%1/Band40m/AgcThreshold").arg(slice->sliceIndex());
        QCOMPARE(AppSettings::instance().value(own).toInt(), -37);
        QVERIFY(seed->agcThreshold() != -37);
        const QString seedOwn = QStringLiteral("Slice%1/Band40m/AgcThreshold").arg(seed->sliceIndex());
        QVERIFY(AppSettings::instance().value(seedOwn, 0).toInt() != -37);
    }

    void aTrueRestoreStillBringsTheLetterBack()
    {
        saveLetterCOn20m();
        if (QTest::currentTestFailed()) { return; }
        RadioModel radio;
        QVERIFY(openSeedOn40m(radio));
        const int c = radio.addSlice(QString(), /*restoreSavedState=*/true);
        QCOMPARE(c, 2);
        SliceModel* slice = radio.sliceById(c);
        QVERIFY(slice);
        QCOMPARE(slice->frequency(), kLetterFrequencyHz);
        QCOMPARE(slice->agcThreshold(), kLetterAgcThreshold);
        QCOMPARE(slice->nbMode(), NbMode::NB2);
        QCOMPARE(slice->stepHz(), kLetterStepHz);
        QCOMPARE(slice->afGain(), kLetterAfGain);
        QVERIFY(slice->locked());
        QVERIFY(slice->muted());
        QVERIFY(slice->ritEnabled());
        QCOMPARE(slice->ritHz(), 250);
        QVERIFY(slice->xitEnabled());
        QCOMPARE(slice->xitHz(), 700);
    }

    void aNewReceiverComesBackAtARestartWithWhatItHad()
    {
        saveLetterCOn20m();
        if (QTest::currentTestFailed()) { return; }
        {
            RadioModel radio;
            QVERIFY(openSeedOn40m(radio));
            QCOMPARE(radio.addSlice(QString(), false, /*newReceiver=*/true), 2);
            radio.flushPendingSettingsSave();
        }
        // The file, not the store this run still holds.
        QVERIFY(AppSettings::instance().save());
        AppSettings::instance().clear();
        AppSettings::instance().load();
        RadioModel radio;
        QVERIFY(radio.addSlice(QString(), true) >= 0);
        QVERIFY(radio.addSlice(QString(), true) >= 0);
        const int c = radio.addSlice(QString(), /*restoreSavedState=*/true);
        QCOMPARE(c, 2);
        SliceModel* slice = radio.sliceById(c);
        QVERIFY(slice);
        QCOMPARE(slice->frequency(), kSeedFrequencyHz);
        verifyCopyOfSeedValues(*slice);
        if (QTest::currentTestFailed()) { return; }
        QVERIFY(!slice->locked());
        QVERIFY(!slice->muted());
        QVERIFY(!slice->ritEnabled());
        QVERIFY(!slice->xitEnabled());
    }

    void aModeChangeOnTheCopyBehavesAsItWouldOnTheSeed()
    {
        auto& settings = AppSettings::instance();
        // Letter C's own per-mode memory on the seed's band, from another life.
        settings.setValue(QStringLiteral("Slice2/Band40m/ModeCWU/FilterLow"), 1500);
        settings.setValue(QStringLiteral("Slice2/Band40m/ModeCWU/FilterHigh"), 1700);
        settings.setValue(QStringLiteral("Slice2/Band40m/ModeAM/FilterLow"), -1000);
        settings.setValue(QStringLiteral("Slice2/Band40m/ModeAM/FilterHigh"), 1000);
        // And on another band, which is not touched.
        settings.setValue(QStringLiteral("Slice2/Band20m/ModeCWU/FilterLow"), 1500);
        settings.setValue(QStringLiteral("Slice2/Band20m/ModeCWU/FilterHigh"), 1700);

        RadioModel radio;
        SliceModel* seed = radio.sliceById(radio.addSlice());
        QVERIFY(seed);
        QVERIFY(radio.addSlice() == 1);
        seed->setFrequency(kSeedFrequencyHz);
        seed->setDspMode(DSPMode::CWU);
        seed->setFilter(400, 900);
        seed->setDspMode(DSPMode::LSB);  // remembers 400..900 for CW on this band
        seed->setFilter(-2400, -300);

        SliceModel* slice = radio.sliceById(radio.addSlice(QString(), false, /*newReceiver=*/true));
        QVERIFY(slice);
        QCOMPARE(slice->sliceIndex(), 2);
        QCOMPARE(slice->dspMode(), DSPMode::LSB);
        QCOMPARE(slice->filterLow(), -2400);
        QCOMPARE(slice->filterHigh(), -300);

        // A mode the seed remembers: the seed's filter.
        slice->setDspMode(DSPMode::CWU);
        QCOMPARE(slice->filterLow(), 400);
        QCOMPARE(slice->filterHigh(), 900);
        // A mode the seed has no memory of: the mode's default, as on the seed.
        slice->setDspMode(DSPMode::LSB);
        slice->setDspMode(DSPMode::AM);
        seed->setDspMode(DSPMode::AM);
        QCOMPARE(slice->filterLow(), seed->filterLow());
        QCOMPARE(slice->filterHigh(), seed->filterHigh());

        // The other band's memory is the letter's still.
        QCOMPARE(settings.value(QStringLiteral("Slice2/Band20m/ModeCWU/FilterLow")).toInt(), 1500);
        QCOMPARE(settings.value(QStringLiteral("Slice2/Band20m/ModeCWU/FilterHigh")).toInt(), 1700);
    }

    void aDevicesNewReceiverCopiesThatDevicesOwnSlice()
    {
        // The slice "you are on" is the asking device's, not the station's.
        RadioModel radio;
        SliceModel* station = radio.sliceById(radio.addSlice());
        QVERIFY(station);
        station->setAgcThreshold(-11);
        const QByteArray device("device-under-test");
        int own = -1;
        {
            const SliceOwnership::CreatorScope creator(radio.sliceOwnership(), device);
            own = radio.addSlice(QString(), false, /*newReceiver=*/true);
        }
        QVERIFY(own >= 0);
        radio.sliceOwnership()->setActive(device, own);
        radio.sliceById(own)->setAgcThreshold(-44);
        SliceModel* made = nullptr;
        {
            const SliceOwnership::CreatorScope creator(radio.sliceOwnership(), device);
            made = radio.sliceById(radio.addSlice(QString(), false, /*newReceiver=*/true));
        }
        QVERIFY(made);
        QCOMPARE(made->agcThreshold(), -44);
    }

    void withNoSeedAReusedLetterOpensOnDefaultsAndKeepsItsMemory()
    {
        saveLetterCOn20m();
        if (QTest::currentTestFailed()) { return; }
        // The first slice of a session, so nothing to copy: letter A, given
        // what saveLetterCOn20m left on C.
        auto& settings = AppSettings::instance();
        const QString lastBand = settings.value(QStringLiteral("Slice2/LastBand")).toString();
        for (const char* key : {"LastBand", "Locked", "Muted", "AfGain", "StepHz", "AgcAutoEnabled"}) {
            settings.setValue(QStringLiteral("Slice0/") + QLatin1String(key),
                              settings.value(QStringLiteral("Slice2/") + QLatin1String(key)));
        }
        settings.setValue(QStringLiteral("Slice0/Band20m/AgcThreshold"), kLetterAgcThreshold);
        settings.setValue(QStringLiteral("Slice0/Band20m/NbMode"), static_cast<int>(NbMode::NB2));
        settings.setValue(QStringLiteral("Slice0/NrActive"), static_cast<int>(NrSlot::NR2));

        RadioModel radio;
        SliceModel* slice = radio.sliceById(radio.addSlice(QString(), false, /*newReceiver=*/true));
        QVERIFY(slice);
        QCOMPARE(slice->sliceIndex(), 0);
        const SliceModel defaults;
        QVERIFY(!slice->locked());
        QVERIFY(!slice->muted());
        QCOMPARE(slice->afGain(), defaults.afGain());
        QCOMPARE(slice->stepHz(), defaults.stepHz());
        QCOMPARE(slice->agcThreshold(), defaults.agcThreshold());
        QCOMPARE(slice->nbMode(), defaults.nbMode());
        QCOMPARE(slice->activeNr(), defaults.activeNr());
        QCOMPARE(slice->autoAgcEnabled(), defaults.autoAgcEnabled());
        radio.flushPendingSettingsSave();
        // Nothing to copy, and its constructor values are not the operator's.
        QCOMPARE(settings.value(QStringLiteral("Slice0/Band20m/AgcThreshold")).toInt(),
                 kLetterAgcThreshold);
        QCOMPARE(settings.value(QStringLiteral("Slice0/LastBand")).toString(), lastBand);
    }

    void aNewReceiverLeavesTheRouteAndVaxChannelAsTheyWere()
    {
        // Not part of the copy, and read as they are for any slice.
        auto& settings = AppSettings::instance();
        settings.setValue(QStringLiteral("Slice1/VaxChannel"), QStringLiteral("3"));
        settings.setValue(QStringLiteral("Slice1/OutputRoute"), QStringLiteral("Headphones"));
        RadioModel radio;
        QVERIFY(radio.addSlice() == 0);
        SliceModel* slice = radio.sliceById(radio.addSlice(QString(), false, /*newReceiver=*/true));
        QVERIFY(slice);
        QCOMPARE(slice->vaxChannel(), 3);
        QCOMPARE(slice->outputRoute(), SliceModel::OutputRoute::Headphones);
    }

    void aNewReceiverWhileALayoutIsHeldWritesNothingOverTheLetter()
    {
        saveLetterCOn20m();
        if (QTest::currentTestFailed()) { return; }
        auto& settings = AppSettings::instance();
        settings.setValue(QStringLiteral("Slice2/Band40m/ModeUSB/FilterLow"), 1234);
        RadioModel radio;
        SliceModel* seed = openSeedOn40m(radio);
        QVERIFY(seed);
        radio.flushPendingSettingsSave();
        // A layout waiting for admission holds every slice key. One only protected
        // holds its own record, not the keys of a receiver that is running.
        radio.setReceiveLayoutHoldForTest(/*isProtected=*/true, /*pendingAdmission=*/true);
        SliceModel* slice = radio.sliceById(radio.addSlice(QString(), false, /*newReceiver=*/true));
        QVERIFY(slice);
        // The slice is still the copy; only the store waits.
        verifyCopyOfSeedValues(*slice);
        if (QTest::currentTestFailed()) { return; }
        radio.flushPendingSettingsSave();
        QCOMPARE(settings.value(QStringLiteral("Slice2/XitHz")).toInt(), 700);
        QCOMPARE(settings.value(QStringLiteral("Slice2/Locked")).toString(), QStringLiteral("True"));
        QCOMPARE(settings.value(QStringLiteral("Slice2/LastBand")).toString(), QStringLiteral("20m"));
        QCOMPARE(settings.value(QStringLiteral("Slice2/Band40m/ModeUSB/FilterLow")).toInt(), 1234);
        QVERIFY(!settings.contains(QStringLiteral("Slice2/Band40m/AgcThreshold")));
    }

    void neverUsedLetterIsUnchanged_data()
    {
        QTest::addColumn<bool>("newReceiver");
        QTest::newRow("new receiver") << true;
        QTest::newRow("other path") << false;
    }

    void neverUsedLetterIsUnchanged()
    {
        QFETCH(bool, newReceiver);
        RadioModel radio;
        SliceModel* slice = radio.sliceById(radio.addSlice(QString(), false, newReceiver));
        QVERIFY(slice);
        QVERIFY(!slice->ritEnabled());
        QCOMPARE(slice->ritHz(), 0);
        QVERIFY(!slice->xitEnabled());
        QCOMPARE(slice->xitHz(), 0);
        QCOMPARE(radio.txFrequencyForSlice(slice), static_cast<quint64>(slice->frequency()));
    }

    void remoteWindowSliceNeverReadsTheLetter()
    {
        // A remote window's mirror of a Core slice takes its values from the
        // Core. It did not read local settings before and still does not.
        saveLetterWithOffsets();
        if (QTest::currentTestFailed()) { return; }
        RadioModel remote{RadioModel::Role::Remote};
        SliceModel* slice = remote.sliceById(remote.addSliceWithStationId(0));
        QVERIFY(slice);
        QVERIFY(!slice->ritEnabled());
        QVERIFY(!slice->xitEnabled());
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Slice0/XitHz")).toInt(), 700);
    }

private:
    static constexpr int kSavedAfGain = 37;
    static constexpr int kDefaultAfGain = 50;

    static constexpr double kSeedFrequencyHz = 7'100'000.0;
    static constexpr int kSeedAgcThreshold = -33;

    static constexpr double kLetterFrequencyHz = 14'250'000.0;
    static constexpr int kLetterAgcThreshold = -71;
    static constexpr int kLetterAgcHang = 777;
    static constexpr int kLetterStepHz = 2500;
    static constexpr int kLetterAfGain = 19;

    // Every value the ruling names, none of them a default.
    static void setSeedValues(SliceModel& seed)
    {
        seed.setFrequency(kSeedFrequencyHz);
        seed.setDspMode(DSPMode::LSB);
        seed.setFilter(-2750, -250);
        seed.setAgcThreshold(kSeedAgcThreshold);
        seed.setAgcHang(444);
        seed.setAgcSlope(3);
        seed.setAgcAttack(5);
        seed.setAgcDecay(555);
        seed.setAgcFixedGain(33);
        seed.setAgcHangThreshold(44);
        seed.setAgcMaxGain(77);
        seed.setAgcMode(AGCMode::Slow);
        seed.setRfGain(66);
        seed.setAutoAgcEnabled(true);
        seed.setAutoAgcOffset(12.5);
        seed.setNbMode(NbMode::NB);
        seed.setNb1Threshold(45);
        seed.setNb1TransitionMs(0.03);
        seed.setNb1LeadMs(0.04);
        seed.setNb1LagMs(0.05);
        seed.setNb2Mode(2);
        seed.setSnbEnabled(true);
        seed.setSnbK1(9.0);
        seed.setSnbK2(21.0);
        seed.setSnbOutputBandwidthHz(5000);
        seed.setActiveNr(NrSlot::NR2);
        seed.setNr2TrainT1(0.7);
        seed.setSsqlThresh(25.0);
        seed.setSsqlEnabled(true);
        seed.setAmsqThresh(-99.0);
        seed.setAmsqEnabled(true);
        seed.setFmsqThresh(-20.0);
        seed.setFmsqEnabled(true);
        seed.setStepHz(5000);
        seed.setAfGain(23);
        // The ones a new receiver does not take.
        seed.setRitHz(300);
        seed.setRitEnabled(true);
        seed.setXitHz(400);
        seed.setXitEnabled(true);
        seed.setMuted(true);
        seed.setLocked(true);
    }

    static void verifyCopyOfSeedValues(const SliceModel& slice)
    {
        QCOMPARE(slice.dspMode(), DSPMode::LSB);
        QCOMPARE(slice.filterLow(), -2750);
        QCOMPARE(slice.filterHigh(), -250);
        QCOMPARE(slice.agcThreshold(), kSeedAgcThreshold);
        QCOMPARE(slice.agcHang(), 444);
        QCOMPARE(slice.agcSlope(), 3);
        QCOMPARE(slice.agcAttack(), 5);
        QCOMPARE(slice.agcDecay(), 555);
        QCOMPARE(slice.agcFixedGain(), 33);
        QCOMPARE(slice.agcHangThreshold(), 44);
        QCOMPARE(slice.agcMaxGain(), 77);
        QCOMPARE(slice.agcMode(), AGCMode::Slow);
        QCOMPARE(slice.rfGain(), 66);
        QVERIFY(slice.autoAgcEnabled());
        QCOMPARE(slice.autoAgcOffset(), 12.5);
        QCOMPARE(slice.nbMode(), NbMode::NB);
        QCOMPARE(slice.nb1Threshold(), 45);
        QCOMPARE(slice.nb1TransitionMs(), 0.03);
        QCOMPARE(slice.nb1LeadMs(), 0.04);
        QCOMPARE(slice.nb1LagMs(), 0.05);
        QCOMPARE(slice.nb2Mode(), 2);
        QVERIFY(slice.snbEnabled());
        QCOMPARE(slice.snbK1(), 9.0);
        QCOMPARE(slice.snbK2(), 21.0);
        QCOMPARE(slice.snbOutputBandwidthHz(), 5000);
        QCOMPARE(slice.activeNr(), NrSlot::NR2);
        QCOMPARE(slice.nr2TrainT1(), 0.7);
        QVERIFY(slice.ssqlEnabled());
        QCOMPARE(slice.ssqlThresh(), 25.0);
        QVERIFY(slice.amsqEnabled());
        QCOMPARE(slice.amsqThresh(), -99.0);
        QVERIFY(slice.fmsqEnabled());
        QCOMPARE(slice.fmsqThresh(), -20.0);
        QCOMPARE(slice.stepHz(), 5000);
        QCOMPARE(slice.afGain(), 23);
    }

    // A, then B, with A set up as the slice you are on, on 40 m. C is next.
    static SliceModel* openSeedOn40m(RadioModel& radio)
    {
        SliceModel* seed = radio.sliceById(radio.addSlice());
        if (!seed || radio.addSlice() != 1) {
            return nullptr;
        }
        setSeedValues(*seed);
        return seed;
    }

    // Letter C as a session left it: on 20 m, locked, muted, its own AGC, NB
    // and step, RIT and XIT on.
    static void saveLetterCOn20m()
    {
        RadioModel radio;
        QVERIFY(radio.addSlice() == 0);
        QVERIFY(radio.addSlice() == 1);
        SliceModel* slice = radio.sliceById(radio.addSlice());
        QVERIFY(slice);
        QCOMPARE(slice->sliceIndex(), 2);
        slice->setFrequency(kLetterFrequencyHz);
        slice->setAgcThreshold(kLetterAgcThreshold);
        slice->setAgcHang(kLetterAgcHang);
        slice->setAgcMode(AGCMode::Long);
        slice->setNbMode(NbMode::NB2);
        slice->setStepHz(kLetterStepHz);
        slice->setAfGain(kLetterAfGain);
        slice->setRitHz(250);
        slice->setRitEnabled(true);
        slice->setXitHz(700);
        slice->setXitEnabled(true);
        slice->setMuted(true);
        slice->setLocked(true);
        slice->saveToSettings(Band::Band20m);
        radio.flushPendingSettingsSave();
    }

    // Saves letter A with RIT and XIT on, and AF gain as a preference that a
    // new receiver still takes, through the slice's own save.
    static void saveLetterWithOffsets()
    {
        RadioModel radio;
        SliceModel* slice = radio.sliceById(radio.addSlice());
        QVERIFY(slice);
        QCOMPARE(slice->sliceIndex(), 0);
        slice->setFrequency(14'200'000.0);
        slice->setRitHz(250);
        slice->setRitEnabled(true);
        slice->setXitHz(700);
        slice->setXitEnabled(true);
        slice->setAfGain(kSavedAfGain);
        slice->saveToSettings(Band::Band20m);
        radio.flushPendingSettingsSave();
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Slice0/XitHz")).toInt(), 700);
    }

    static void verifyOffsetsKept(RadioModel& radio, SliceModel* slice)
    {
        QVERIFY(slice);
        QVERIFY(slice->ritEnabled());
        QCOMPARE(slice->ritHz(), 250);
        QVERIFY(slice->xitEnabled());
        QCOMPARE(slice->xitHz(), 700);
        QCOMPARE(radio.txFrequencyForSlice(slice),
                 static_cast<quint64>(slice->frequency()) + 700);
    }
};

QTEST_MAIN(TestRadioSliceSettingsRestore)
#include "tst_radio_slice_settings_restore.moc"
