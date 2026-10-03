// no-port-check: NereusSDR-original receive-layout hydration regression tests.

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/RadeChannel.h"
#include "core/RadioConnection.h"
#include "core/WdspEngine.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QSignalSpy>
#include <QFile>

#include <limits>

using namespace NereusSDR;

namespace {

class NullConnection final : public RadioConnection {
public:
    using RadioConnection::RadioConnection;

    void init() override {}
    void connectToRadio(const RadioInfo&) override {}
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
    void setWatchdogEnabled(bool) override {}
};

constexpr double kManifestFrequencyHz = 14293200.0;
constexpr double kOriginalFrequencyHz = 7200000.0;

QString bandPrefix(Band band)
{
    return QStringLiteral("Slice0/Band%1/").arg(bandKeyName(band));
}

QString modePrefix(Band band, DSPMode mode)
{
    return bandPrefix(band) + QStringLiteral("Mode%1/").arg(SliceModel::modeName(mode));
}

} // namespace

class TstReceiveSliceSeed : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // The normal Qt test sandbox is shared by concurrently running test
        // executables. Disk readback must own a separate singleton profile.
        AppSettings::setProfileOverride(QStringLiteral("receive-seed-test-%1")
                                       .arg(QCoreApplication::applicationPid()));
    }
    void cleanupTestCase() { QFile::remove(AppSettings::instance().filePath()); }
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void restoresPreferencesThenSeedsManifestWithoutSignalsOrRade()
    {
        auto& settings = AppSettings::instance();
        const Band band = bandFromFrequency(kManifestFrequencyHz);
        const QString legacyPrefix = bandPrefix(band);
        const QString manifestModePrefix = modePrefix(band, DSPMode::RADE_U);
        const QString mac = QStringLiteral("AA:BB:CC:DD:EE:01");
        const QString normalizedMac = AppSettings::normalizedRadioMac(mac);
        const QString nnrPrefix = QStringLiteral("hardware/%1/slices/0/nnr/")
                                      .arg(normalizedMac);

        // A different legacy mode and filter prove that the manifest-mode
        // choice does not retain the legacy filter selected by restoreFromSettings.
        settings.setValue(legacyPrefix + QStringLiteral("Frequency"), kOriginalFrequencyHz);
        settings.setValue(legacyPrefix + QStringLiteral("DspMode"), static_cast<int>(DSPMode::LSB));
        settings.setValue(legacyPrefix + QStringLiteral("FilterLow"), -3100);
        settings.setValue(legacyPrefix + QStringLiteral("FilterHigh"), -180);
        settings.setValue(modePrefix(band, DSPMode::LSB) + QStringLiteral("FilterLow"), -2900);
        settings.setValue(modePrefix(band, DSPMode::LSB) + QStringLiteral("FilterHigh"), -220);
        settings.setValue(manifestModePrefix + QStringLiteral("FilterLow"), 700);
        settings.setValue(manifestModePrefix + QStringLiteral("FilterHigh"), 2300);
        settings.setValue(legacyPrefix + QStringLiteral("AgcMode"), static_cast<int>(AGCMode::Fast));
        settings.setValue(QStringLiteral("Slice0/AfGain"), 73);
        settings.setValue(QStringLiteral("Slice0/Locked"), QStringLiteral("True"));
        settings.setValue(nnrPrefix + QStringLiteral("NnrModelSlot"), 1);
        settings.setValue(nnrPrefix + QStringLiteral("NnrAlpha"), 0.8);
        // Read through the real sandboxed AppSettings file, rather than a
        // fake settings adapter, before checking that hydration is read-only.
        QVERIFY(settings.save());
        settings.clear();
        settings.load();
        const auto before = settings.snapshot({QStringLiteral("Slice0/"),
                                               QStringLiteral("hardware/")});

        RadioModel radio;
        SliceModel slice(0, &radio);
        slice.setSettingsRadioIdentity(mac);
        QCOMPARE(slice.settingsRadioIdentity(), normalizedMac);
        QSignalSpy frequencyChanges(&slice, &SliceModel::frequencyChanged);
        QSignalSpy modeChanges(&slice, &SliceModel::dspModeChanged);
        QSignalSpy filterChanges(&slice, &SliceModel::filterChanged);

        QVERIFY(slice.restoreReceiveState(kManifestFrequencyHz, DSPMode::RADE_U));

        QCOMPARE(slice.frequency(), kManifestFrequencyHz);
        QCOMPARE(slice.band(), band);
        QCOMPARE(static_cast<int>(slice.dspMode()), static_cast<int>(DSPMode::RADE_U));
        QCOMPARE(slice.filterLow(), 700);
        QCOMPARE(slice.filterHigh(), 2300);
        QCOMPARE(static_cast<int>(slice.agcMode()), static_cast<int>(AGCMode::Fast));
        QCOMPARE(slice.afGain(), 73);
        QCOMPARE(slice.nnrModelSlot(), 1);
        QCOMPARE(slice.nnrAlpha(), 0.8);
        QVERIFY(slice.locked());
        QCOMPARE(frequencyChanges.count(), 0);
        QCOMPARE(modeChanges.count(), 0);
        QCOMPARE(filterChanges.count(), 0);
        QVERIFY(radio.wdspEngine()->radeChannel(slice.sliceIndex()) == nullptr);
        QCOMPARE(settings.snapshot({QStringLiteral("Slice0/"), QStringLiteral("hardware/")}),
                 before);
    }

    void fallsBackToManifestModeDefaultInsteadOfLegacyFilter()
    {
        auto& settings = AppSettings::instance();
        const Band band = bandFromFrequency(kManifestFrequencyHz);
        const QString legacyPrefix = bandPrefix(band);
        settings.setValue(legacyPrefix + QStringLiteral("DspMode"), static_cast<int>(DSPMode::LSB));
        settings.setValue(legacyPrefix + QStringLiteral("FilterLow"), -3200);
        settings.setValue(legacyPrefix + QStringLiteral("FilterHigh"), -100);

        RadioModel radio;
        SliceModel slice(0, &radio);
        QVERIFY(slice.restoreReceiveState(kManifestFrequencyHz, DSPMode::AM));

        const auto expected = SliceModel::defaultFilterForMode(DSPMode::AM);
        QCOMPARE(slice.filterLow(), expected.first);
        QCOMPARE(slice.filterHigh(), expected.second);
    }

    void retainsLegacyBandFilterWhenItsValidModeMatchesManifest()
    {
        auto& settings = AppSettings::instance();
        const Band band = bandFromFrequency(kManifestFrequencyHz);
        const QString legacyPrefix = bandPrefix(band);
        settings.setValue(legacyPrefix + QStringLiteral("DspMode"), static_cast<int>(DSPMode::AM));
        settings.setValue(legacyPrefix + QStringLiteral("FilterLow"), -4800);
        settings.setValue(legacyPrefix + QStringLiteral("FilterHigh"), 4800);

        RadioModel radio;
        SliceModel slice(0, &radio);
        QVERIFY(slice.restoreReceiveState(kManifestFrequencyHz, DSPMode::AM));

        QCOMPARE(slice.filterLow(), -4800);
        QCOMPARE(slice.filterHigh(), 4800);
    }

    void ignoresLegacyFilterWithoutValidModeSentinel()
    {
        auto& settings = AppSettings::instance();
        const Band band = bandFromFrequency(kManifestFrequencyHz);
        const QString legacyPrefix = bandPrefix(band);
        settings.setValue(legacyPrefix + QStringLiteral("FilterLow"), -3200);
        settings.setValue(legacyPrefix + QStringLiteral("FilterHigh"), -100);

        RadioModel radio;
        SliceModel slice(0, &radio);
        QVERIFY(slice.restoreReceiveState(kManifestFrequencyHz, DSPMode::AM));

        const auto expected = SliceModel::defaultFilterForMode(DSPMode::AM);
        QCOMPARE(slice.filterLow(), expected.first);
        QCOMPARE(slice.filterHigh(), expected.second);
    }

    void refusesInvalidDescriptorsWithoutMutation()
    {
        RadioModel radio;
        SliceModel slice(0, &radio);
        slice.setFrequency(kOriginalFrequencyHz);
        slice.setDspMode(DSPMode::LSB);
        const double originalFrequency = slice.frequency();
        const DSPMode originalMode = slice.dspMode();

        QVERIFY(!slice.restoreReceiveState(std::numeric_limits<double>::quiet_NaN(), DSPMode::USB));
        QVERIFY(!slice.restoreReceiveState(SliceModel::kMaxReceiveFrequencyHz + 1.0, DSPMode::USB));
        QVERIFY(!slice.restoreReceiveState(kManifestFrequencyHz, static_cast<DSPMode>(999)));
        QCOMPARE(slice.frequency(), originalFrequency);
        QCOMPARE(static_cast<int>(slice.dspMode()), static_cast<int>(originalMode));
    }

    void refusesRemoteConnectedAndLiveDspModelsWithoutMutation()
    {
        RadioModel remote(RadioModel::Role::Remote);
        SliceModel remoteSlice(0, &remote);
        QVERIFY(!remoteSlice.restoreReceiveState(kManifestFrequencyHz, DSPMode::USB));

        RadioModel connected;
        SliceModel connectedSlice(0, &connected);
        NullConnection connection;
        connected.injectConnectionForTest(&connection);
        QVERIFY(!connectedSlice.restoreReceiveState(kManifestFrequencyHz, DSPMode::USB));
        connected.injectConnectionForTest(nullptr);

        RadioModel live;
        SliceModel liveSlice(0, &live);
        liveSlice.setFrequency(kOriginalFrequencyHz);
        RadeChannel* const rade = live.wdspEngine()->createRadeChannel(1);
        QVERIFY(rade != nullptr);
        QVERIFY(!liveSlice.restoreReceiveState(kManifestFrequencyHz, DSPMode::RADE_U));
        QCOMPARE(liveSlice.frequency(), kOriginalFrequencyHz);
    }
};

QTEST_MAIN(TstReceiveSliceSeed)
#include "tst_receive_slice_seed.moc"
