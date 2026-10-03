// no-port-check: NereusSDR-original integration tests for stable-slice saves.
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtEndian>
#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspEngine.h"
#include "core/dsp/ChannelConfig.h"
#include "core/dsp/NnrLoadGovernor.h"
#include "core/wdsp_api.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

extern "C" {
extern const unsigned char nnr_model_1_data[];
extern const unsigned int nnr_model_1_size;
}

using namespace NereusSDR;

namespace {
constexpr int kRateHz = 48000;

// The bundled premium model with one required tensor renamed: the file
// opens, but the network cannot be built, so the premium slot of every
// channel opened while this path is set is unavailable.
QString writeUnusablePremiumModel(const QTemporaryDir& directory)
{
    QByteArray broken(reinterpret_cast<const char*>(nnr_model_1_data), nnr_model_1_size);
    const auto count = qFromLittleEndian<quint32>(broken.constData() + 12);
    for (quint32 i = 0; i < count; ++i) {
        const int offset = 32 + static_cast<int>(i) * 72;
        if (broken.mid(offset, 6) == "enc1_w") {
            broken[offset] = 'x';
            const QString path = directory.filePath(QStringLiteral("unusable-premium.bin"));
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(broken) != broken.size()) {
                return {};
            }
            return path;
        }
    }
    return {};
}
} // namespace

class TestNnrRadioPersistence : public QObject {
    Q_OBJECT
private slots:
    void init() { AppSettings::instance().clear(); }

    void inactiveSliceAndRemovedSliceFlushBeforeTheirIdentityDisappears()
    {
        const QString mac = QStringLiteral("00:1C:2D:03:04:05");
        RadioModel radio;
        const int aId = radio.addSlice();
        const int bId = radio.addSlice();
        const int cId = radio.addSlice();
        QVERIFY(aId >= 0 && bId >= 0 && cId >= 0);
        auto* a = radio.sliceById(aId);
        auto* b = radio.sliceById(bId);
        auto* c = radio.sliceById(cId);
        for (SliceModel* slice : {a, b, c}) {
            slice->setSettingsRadioIdentity(mac);
        }
        radio.setActiveSliceById(aId);
        b->setNnrAlpha(2.25);
        b->setNnrMaskFloorDb(-32.75);
        b->setActiveNr(NrSlot::NNR);
        c->setNnrReleaseMs(125.5);
        c->setActiveNr(NrSlot::NR2);
        const QString bPrefix = b->nnrSettingsPrefix();
        const QString cPrefix = c->nnrSettingsPrefix();
        radio.removeSlice(bId);
        QVERIFY(!radio.sliceById(bId));
        QCOMPARE(radio.sliceById(cId), c);
        radio.flushPendingSettingsSave();

        // Clear memory and reload the actual atomic settings file. Neither
        // operation relies on a dialog close or the active receiver's ID.
        AppSettings::instance().clear();
        AppSettings::instance().load();
        QCOMPARE(AppSettings::instance().value(bPrefix + "NnrAlpha").toDouble(), 2.25);
        QCOMPARE(AppSettings::instance().value(bPrefix + "NnrMaskFloorDb").toDouble(), -32.75);
        QCOMPARE(AppSettings::instance().value(bPrefix + "NrActive").toInt(), 8);
        QCOMPARE(AppSettings::instance().value(cPrefix + "NnrReleaseMs").toDouble(), 125.5);
        QCOMPARE(AppSettings::instance().value(cPrefix + "NrActive").toInt(), 2);

        // Focus changes after the edit cannot resurrect the previous NNR
        // choice. Selection and tuning use the same durable namespace.
        c->setActiveNr(NrSlot::Off);
        radio.setActiveSliceById(cId);
        radio.flushPendingSettingsSave();
        AppSettings::instance().clear();
        AppSettings::instance().load();
        QCOMPARE(AppSettings::instance().value(cPrefix + "NrActive").toInt(), 0);
        QCOMPARE(AppSettings::instance().value(cPrefix + "NnrReleaseMs").toDouble(), 125.5);
    }

    void failedSaveRemainsVisibleAndRetriesTheAcceptedValues()
    {
        auto& settings = AppSettings::instance();
        RadioModel radio;
        const int id = radio.addSlice();
        auto* slice = radio.sliceById(id);
        slice->setSettingsRadioIdentity("AA:BB:CC:DD:EE:08");
        const QString prefix = slice->nnrSettingsPrefix();
        slice->setNnrAlpha(2.75);
        // A directory at the target file path deterministically refuses an
        // atomic file replacement, including when tests run with elevated rights.
        QFile::remove(settings.filePath());
        QVERIFY(QDir().mkpath(settings.filePath()));
        radio.flushPendingSettingsSave();
        QVERIFY(!radio.settingsSaveError().isEmpty());
        QCOMPARE(slice->nnrAlpha(), 2.75);
        QVERIFY(QDir().rmdir(settings.filePath()));
        radio.flushPendingSettingsSave();
        QVERIFY(radio.settingsSaveError().isEmpty());
        settings.clear();
        settings.load();
        QCOMPARE(settings.value(prefix + "NnrAlpha").toDouble(), 2.75);
    }

    void configuredSlicesRestoreTheirOwnSettingsBeforeUse()
    {
        const QString mac = QStringLiteral("00:1C:2D:03:04:05");
        auto& settings = AppSettings::instance();
        settings.setValue("hardware/" + mac + "/slices/0/nnr/NnrAlpha", 2.0);
        settings.setValue("hardware/" + mac + "/slices/1/nnr/NnrAlpha", 3.0);
        settings.setValue("hardware/" + mac + "/slices/1/nnr/NrActive", 8);
        RadioModel radio;
        const int aId = radio.addSlice();
        const int bId = radio.addSlice();
        auto* a = radio.sliceById(aId);
        auto* b = radio.sliceById(bId);
        a->setSettingsRadioIdentity(mac);
        b->setSettingsRadioIdentity(mac);
        radio.loadSliceState(a);
        radio.loadSliceState(b);
        QCOMPARE(a->nnrAlpha(), 2.0);
        QCOMPARE(b->nnrAlpha(), 3.0);
        QCOMPARE(b->activeNr(), NrSlot::NNR);
        QVERIFY(!b->nnrRunning()); // Saved intent is not a live receiver.
    }

    // R-R3-40 stale-cache regression. The slice's saved choice is Premium,
    // but the premium model was not usable when its receiver opened, so the
    // receiver refused it. Once the receiver is rebuilt with Premium
    // available, turning NNR on must run Premium, and choosing Premium again
    // must never be ignored. Before the fix, RxChannel re-applied a cached
    // tuning that defaults to Standard and only changes on success, so WDSP
    // ran Standard while the slice showed Premium, and the equality check
    // in SliceModel ignored re-selecting Premium.
    void runningModelFollowsTheSavedChoice()
    {
        const QString mac = QStringLiteral("00:1C:2D:03:04:09");
        auto& settings = AppSettings::instance();
        settings.setValue("hardware/" + mac + "/slices/0/nnr/NnrModelSlot", 1);

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString unusable = writeUnusablePremiumModel(directory);
        QVERIFY(!unusable.isEmpty());
        const QByteArray encoded = QFile::encodeName(unusable);
        SetNNRModelPathSlot(1, encoded.constData());
        const auto restorePath = qScopeGuard([] { SetNNRModelPathSlot(1, ""); });

        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        RadioInfo info;
        info.macAddress = mac;
        model.setLastRadioInfoForTest(info);
        model.configureStreamPool(1, 1, kRateHz);
        const int id = model.addSlice();
        SliceModel* slice = model.sliceById(id);
        QVERIFY(slice);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrModelSlot(), 1);
        slice->setFrequency(14200000.0);
        model.openRxChannelPool(1, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(model.waitForReceiveLaneForTest());
        QVERIFY(engine->rxChannel(id));
        QVERIFY(model.waitForReceiveLaneForTest());
        QVERIFY(!engine->rxChannel(id)->nnrDiagnostics().modelAvailable[1]);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrModelSlot(), 1);   // the saved choice is kept

        // Premium becomes usable; the receiver is rebuilt underneath the slice.
        SetNNRModelPathSlot(1, "");
        ChannelConfig config;
        config.sampleRate = kRateHz;
        config.bufferSize = bufferSizeForRate(kRateHz);
        config.filterSize = 4096;
        QVERIFY(engine->rebuildRxChannel(id, config) >= 0);
        RxChannel* channel = engine->rxChannel(id);
        QVERIFY(channel);
        QVERIFY(model.waitForReceiveLaneForTest());
        QVERIFY(channel->nnrDiagnostics().modelAvailable[1]);

        slice->setActiveNr(NrSlot::NNR);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->activeNr(), NrSlot::NNR);
        QVERIFY(model.waitForReceiveLaneForTest());
        const int afterSelect = channel->nnrDiagnostics().actualModelSlot;
        const bool runningAfterSelect = channel->nnrDiagnostics().running;
        const int shownAfterSelect = slice->nnrActualModelSlot();

        slice->setNnrModelSlot(1);   // the operator chooses Premium again
        QVERIFY(model.waitForReceiveLaneForTest());
        const int afterReselect = channel->nnrDiagnostics().actualModelSlot;
        qInfo("slice shows model %d; WDSP runs model %d after NNR on (running %d, "
              "shown actual %d), model %d after choosing Premium again",
              slice->nnrModelSlot(), afterSelect, int(runningAfterSelect),
              shownAfterSelect, afterReselect);
        QVERIFY(runningAfterSelect);
        QCOMPARE(afterSelect, 1);
        QCOMPARE(shownAfterSelect, 1);
        QCOMPARE(afterReselect, 1);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrModelSlot(), 1);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrActualModelSlot(), 1);
        model.flushPendingSettingsSave();
        QCOMPARE(settings.value("hardware/" + mac + "/slices/0/nnr/NnrModelSlot").toInt(), 1);
    }

    // R-R3-40: a receiver that cannot keep up steps back at runtime (Premium
    // to Standard to off) while the saved choice stays Premium with NNR on;
    // "Try again", choosing a model, NNR off and on, and a Core restart all
    // bring the saved choice back.
    void overloadStepsBackAtRuntimeAndTheOperatorGetsTheSavedChoiceBack()
    {
        const QString mac = QStringLiteral("00:1C:2D:03:04:0A");
        const QString prefix = "hardware/" + mac + "/slices/0/nnr/";
        auto& settings = AppSettings::instance();
        settings.setValue(prefix + "NnrModelSlot", 1);
        settings.setValue(prefix + "NrActive", static_cast<int>(NrSlot::NNR));

        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        RadioInfo info;
        info.macAddress = mac;
        model.setLastRadioInfoForTest(info);
        model.configureStreamPool(1, 1, kRateHz);
        const int id = model.addSlice();
        SliceModel* slice = model.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14200000.0);
        model.openRxChannelPool(1, bufferSizeForRate(kRateHz), kRateHz);
        RxChannel* channel = engine->rxChannel(id);
        QVERIFY(channel);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->activeNr(), NrSlot::NNR);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrActualModelSlot(), 1);
        QVERIFY(model.waitForReceiveLaneForTest());
        QVERIFY(slice->nnrRunning());
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrLimit(), 0);

        const qint64 tick = NnrLoadGovernor::kNnrCheckIntervalMs;
        qint64 now = 0;
        const QHash<int, std::optional<double>> overloaded{{id, 0.95}};
        // Checks until the limit changes; returns how many it took.
        const auto runUntilStep = [&](const QHash<int, std::optional<double>>& loads) {
            const int before = slice->nnrLimit();
            int checks = 0;
            while (slice->nnrLimit() == before && checks < 40) {
                model.governNnrLoadForTest(now, loads);
                now += tick;
                ++checks;
            }
            return checks;
        };

        QCOMPARE(runUntilStep(overloaded), 5);   // time base + 2 s
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrLimit(), static_cast<int>(NnrLimit::StandardOnly));
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(channel->nnrLimit(), static_cast<int>(NnrLimit::StandardOnly));
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrModelSlot(), 1);                 // saved choice kept
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->activeNr(), NrSlot::NNR);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrLimitText(),
                 QStringLiteral("Noise reduction is using the Standard model. "
                                "This computer could not keep up with Premium."));
        // A tuning edit takes the DSP lock, which applies the pending limit:
        // WDSP runs Standard, the accepted choice stays Premium.
        slice->setNnrAlpha(2.0);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrActualModelSlot(), 0);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrModelSlot(), 1);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(channel->nnrTuning().modelSlot, 1);
        QVERIFY(model.waitForReceiveLaneForTest());
        QVERIFY(slice->nnrRunning());

        // Settle: nothing more for 5 s, then 2 s more of overload turns it off.
        const int settleChecks = runUntilStep(overloaded);
        QVERIFY2(settleChecks >= 10, "stepped again inside the settle time");
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrLimit(), static_cast<int>(NnrLimit::Off));
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrLimitText(),
                 QStringLiteral("Noise reduction was turned off. This computer could not keep up."));
        slice->setNnrAlpha(2.25);
        QVERIFY(model.waitForReceiveLaneForTest());
        QVERIFY(!slice->nnrRunning());
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->activeNr(), NrSlot::NNR);
        // Off is the floor, and light load never raises a level.
        runUntilStep({{id, 0.1}});
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrLimit(), static_cast<int>(NnrLimit::Off));

        model.flushPendingSettingsSave();
        settings.clear();
        settings.load();
        QCOMPARE(settings.value(prefix + "NnrModelSlot").toInt(), 1);
        QCOMPARE(settings.value(prefix + "NrActive").toInt(), static_cast<int>(NrSlot::NNR));
        for (const QString& key : settings.allKeys()) {
            QVERIFY2(!key.contains(QStringLiteral("NnrLimit"), Qt::CaseInsensitive)
                         && !(key.startsWith(prefix) && key.contains(QStringLiteral("Limit"))),
                     qPrintable(key));
        }

        // "Try again" runs the saved choice at once.
        slice->requestNnrRetry();
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrLimit(), 0);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(channel->nnrLimit(), 0);
        QVERIFY(model.waitForReceiveLaneForTest());
        QVERIFY(slice->nnrRunning());
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrActualModelSlot(), 1);

        // Choosing the saved model again is the operator asking again too.
        QCOMPARE(runUntilStep(overloaded), 5);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrLimit(), static_cast<int>(NnrLimit::StandardOnly));
        slice->setNnrModelSlot(1);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrLimit(), 0);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrActualModelSlot(), 1);

        // So is turning NNR off and on.
        QCOMPARE(runUntilStep(overloaded), 5);
        slice->setActiveNr(NrSlot::Off);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrLimit(), 0);
        QVERIFY(model.waitForReceiveLaneForTest());
        QVERIFY(!slice->nnrRunning());
        // NNR off: an overloaded receiver gets no step.
        QCOMPARE(runUntilStep(overloaded), 40);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrLimit(), 0);
        slice->setActiveNr(NrSlot::NNR);
        QVERIFY(model.waitForReceiveLaneForTest());
        QVERIFY(slice->nnrRunning());
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrActualModelSlot(), 1);

        // A Core restart starts with no limit: it is never saved.
        QCOMPARE(runUntilStep(overloaded), 5);
        model.flushPendingSettingsSave();
        RadioModel restarted;
        restarted.setLastRadioInfoForTest(info);
        const int restartedId = restarted.addSlice();
        QCOMPARE(restarted.sliceById(restartedId)->nnrLimit(), 0);
        QCOMPARE(restarted.sliceById(restartedId)->nnrModelSlot(), 1);
    }

    // R-R3-40: an overloaded receiver whose NNR is selected but not running
    // (here the saved Premium model cannot be built) is overloaded by
    // something else, so NNR is never blamed for it.
    void nnrSelectedButNotRunningIsNeverSteppedBack()
    {
        const QString mac = QStringLiteral("00:1C:2D:03:04:0B");
        const QString prefix = "hardware/" + mac + "/slices/0/nnr/";
        auto& settings = AppSettings::instance();
        settings.setValue(prefix + "NnrModelSlot", 1);
        settings.setValue(prefix + "NrActive", static_cast<int>(NrSlot::NNR));

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString unusable = writeUnusablePremiumModel(directory);
        QVERIFY(!unusable.isEmpty());
        const QByteArray encoded = QFile::encodeName(unusable);
        SetNNRModelPathSlot(1, encoded.constData());
        const auto restorePath = qScopeGuard([] { SetNNRModelPathSlot(1, ""); });

        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        RadioInfo info;
        info.macAddress = mac;
        model.setLastRadioInfoForTest(info);
        model.configureStreamPool(1, 1, kRateHz);
        const int id = model.addSlice();
        SliceModel* slice = model.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14200000.0);
        model.openRxChannelPool(1, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(model.waitForReceiveLaneForTest());
        QVERIFY(engine->rxChannel(id));
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->activeNr(), NrSlot::NNR);
        QVERIFY(model.waitForReceiveLaneForTest());
        QVERIFY(!slice->nnrRunning());

        qint64 now = 0;
        for (int check = 0; check < 40; ++check) {
            model.governNnrLoadForTest(now, {{id, 0.95}});
            now += NnrLoadGovernor::kNnrCheckIntervalMs;
        }
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrLimit(), 0);
    }

    // R-R3-40: turning NNR off or on clears a limit even with no receiver
    // open (disconnected), like it does with one.
    void turningNnrOffOrOnWithoutAReceiverClearsTheLimit()
    {
        RadioModel model;
        const int id = model.addSlice();
        SliceModel* slice = model.sliceById(id);
        QVERIFY(slice);
        QVERIFY(!model.wdspEngine() || !model.wdspEngine()->rxChannel(id));
        slice->setActiveNr(NrSlot::NNR);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->activeNr(), NrSlot::NNR);

        slice->setNnrLimit(static_cast<int>(NnrLimit::StandardOnly));
        slice->setActiveNr(NrSlot::Off);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->activeNr(), NrSlot::Off);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrLimit(), 0);

        slice->setNnrLimit(static_cast<int>(NnrLimit::Off));
        slice->setActiveNr(NrSlot::NNR);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->activeNr(), NrSlot::NNR);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrLimit(), 0);
    }

    // R-R3-40: choosing Standard while held at Standard (saved Premium) runs
    // Standard throughout: the new choice is applied before the limit is
    // cleared, so Premium never runs in between.
    void choosingStandardWhileHeldAtStandardNeverRunsPremium()
    {
        const QString mac = QStringLiteral("00:1C:2D:03:04:0C");
        const QString prefix = "hardware/" + mac + "/slices/0/nnr/";
        auto& settings = AppSettings::instance();
        settings.setValue(prefix + "NnrModelSlot", 1);
        settings.setValue(prefix + "NrActive", static_cast<int>(NrSlot::NNR));

        RadioModel model;
        WdspEngine* engine = model.wdspEngine();
        engine->m_initialized = true;   // friend access (NEREUS_BUILD_TESTS)
        RadioInfo info;
        info.macAddress = mac;
        model.setLastRadioInfoForTest(info);
        model.configureStreamPool(1, 1, kRateHz);
        const int id = model.addSlice();
        SliceModel* slice = model.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14200000.0);
        model.openRxChannelPool(1, bufferSizeForRate(kRateHz), kRateHz);
        QVERIFY(model.waitForReceiveLaneForTest());
        QVERIFY(engine->rxChannel(id));
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrActualModelSlot(), 1);

        qint64 now = 0;
        while (slice->nnrLimit() == 0 && now < 40 * NnrLoadGovernor::kNnrCheckIntervalMs) {
            model.governNnrLoadForTest(now, {{id, 0.95}});
            now += NnrLoadGovernor::kNnrCheckIntervalMs;
        }
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrLimit(), static_cast<int>(NnrLimit::StandardOnly));
        slice->setNnrAlpha(2.0);   // a locked call applies the pending limit
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrActualModelSlot(), 0);

        QList<int> running;
        connect(slice, &SliceModel::nnrDiagnosticsChanged, this,
                [&running, slice] { running.append(slice->nnrActualModelSlot()); });
        slice->setNnrModelSlot(0);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrLimit(), 0);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrModelSlot(), 0);
        QVERIFY(model.waitForReceiveLaneForTest());
        QCOMPARE(slice->nnrActualModelSlot(), 0);
        QVERIFY2(!running.contains(1), "Premium ran while the operator chose Standard");
    }
};

QTEST_MAIN(TestNnrRadioPersistence)
#include "tst_nnr_radio_persistence.moc"
