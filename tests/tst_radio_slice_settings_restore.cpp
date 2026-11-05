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
        {
            RadioModel radio;
            SliceModel* slice = radio.sliceById(radio.addSlice());
            QVERIFY(slice);
            QCOMPARE(slice->sliceIndex(), 0);
            QVERIFY(!slice->ritEnabled());
            QCOMPARE(slice->ritHz(), 0);
            QVERIFY(!slice->xitEnabled());
            QCOMPARE(slice->xitHz(), 0);
            QCOMPARE(radio.txFrequencyForSlice(slice),
                     static_cast<quint64>(slice->frequency()));
            // Only RIT and XIT start over: the letter's preferences still come.
            QCOMPARE(slice->afGain(), kSavedAfGain);
            // Turning XIT on later cannot jump to the old offset.
            slice->setXitEnabled(true);
            QCOMPARE(radio.txFrequencyForSlice(slice),
                     static_cast<quint64>(slice->frequency()));
            slice->setXitEnabled(false);
        }
    }

    void newReceiverAddedOnAPanStartsWithRitAndXitOff()
    {
        // The other way an operator asks: +RX on a pan with the default,
        // not the startup restore.
        saveLetterWithOffsets();
        if (QTest::currentTestFailed()) { return; }
        RadioModel radio;
        const int id = radio.addSlice(QStringLiteral("pan-0"), /*restoreSavedState=*/false);
        SliceModel* slice = radio.sliceById(id);
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
            QVERIFY(radio.addSlice() >= 0);
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

    void bandRestoreKeepsAReceiversRitAndXit()
    {
        // A band change on a live receiver reads the same keys it just saved.
        RadioModel radio;
        SliceModel* slice = radio.sliceById(radio.addSlice());
        QVERIFY(slice);
        slice->setFrequency(14'200'000.0);
        slice->setRitHz(250);
        slice->setRitEnabled(true);
        slice->setXitHz(700);
        slice->setXitEnabled(true);
        slice->saveToSettings(Band::Band20m);
        slice->setFrequency(7'100'000.0);
        slice->saveToSettings(Band::Band40m);
        slice->restoreFromSettings(Band::Band20m);
        verifyOffsetsKept(radio, slice);
    }

    void neverUsedLetterIsUnchanged()
    {
        RadioModel radio;
        SliceModel* slice = radio.sliceById(radio.addSlice());
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
