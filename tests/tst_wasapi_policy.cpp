// =================================================================
// tests/tst_wasapi_policy.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  The Windows audio engine's
// decisions (R-AUD-02, R-AUD-03, R-AUD-11, R-AUD-14, R-AUD-16), tested on
// every system: the exclusive format order, the shared and exclusive
// periods, which endpoint notices reach the catalogue, the transport, and
// how a Windows audio result opens and what it posts.  No device.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 9 (R-AUD-01, R-AUD-02, R-AUD-11,
//               R-AUD-16). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/audio/WasapiPolicy.h"

using namespace NereusSDR;

namespace {

const WasapiEnginePeriods kPeriods{480, 48, 48, 480};

} // namespace

class TestWasapiPolicy : public QObject {
    Q_OBJECT

private slots:
    void exclusiveFormatOrderIsFloatThenInt32Then24In32ThenInt16()
    {
        const QList<WasapiFormatCandidate> order = wasapiExclusiveFormatOrder();
        QCOMPARE(order.size(), 4);
        QVERIFY((order[0] == WasapiFormatCandidate{WasapiSampleType::Float, 32, 32}));
        QVERIFY((order[1] == WasapiFormatCandidate{WasapiSampleType::Int, 32, 32}));
        QVERIFY((order[2] == WasapiFormatCandidate{WasapiSampleType::Int, 32, 24}));
        QVERIFY((order[3] == WasapiFormatCandidate{WasapiSampleType::Int, 16, 16}));

        QCOMPARE(wasapiDeviceFormat(order[0]), DeviceSampleFormat::Float32);
        QCOMPARE(wasapiDeviceFormat(order[1]), DeviceSampleFormat::Int32);
        QCOMPARE(wasapiDeviceFormat(order[2]), DeviceSampleFormat::Int32);
        QCOMPARE(wasapiDeviceFormat(order[3]), DeviceSampleFormat::Int16);
    }

    void mixFormatsTheConverterWrites()
    {
        QCOMPARE(wasapiMixSampleFormat({WasapiSampleType::Float, 32, 32}),
                 std::optional<DeviceSampleFormat>(DeviceSampleFormat::Float32));
        QCOMPARE(wasapiMixSampleFormat({WasapiSampleType::Float, 64, 64}),
                 std::optional<DeviceSampleFormat>(DeviceSampleFormat::Float64));
        QCOMPARE(wasapiMixSampleFormat({WasapiSampleType::Int, 16, 16}),
                 std::optional<DeviceSampleFormat>(DeviceSampleFormat::Int16));
        QCOMPARE(wasapiMixSampleFormat({WasapiSampleType::Int, 24, 24}),
                 std::optional<DeviceSampleFormat>(DeviceSampleFormat::Int24Packed));
        QCOMPARE(wasapiMixSampleFormat({WasapiSampleType::Int, 32, 24}),
                 std::optional<DeviceSampleFormat>(DeviceSampleFormat::Int32));
        QCOMPARE(wasapiMixSampleFormat({WasapiSampleType::Int, 32, 32}),
                 std::optional<DeviceSampleFormat>(DeviceSampleFormat::Int32));
        QVERIFY(!wasapiMixSampleFormat({WasapiSampleType::Int, 8, 8}).has_value());
        QVERIFY(!wasapiMixSampleFormat({WasapiSampleType::Float, 16, 16}).has_value());
        QVERIFY(!wasapiMixSampleFormat({WasapiSampleType::Int, 32, 33}).has_value());
    }

    void sharedPeriodIsTheSmallestOrTheRequestRoundedUp()
    {
        QCOMPARE(wasapiSharedPeriodFrames(kPeriods, 0), 48);
        QCOMPARE(wasapiSharedPeriodFrames(kPeriods, 100), 144);
        QCOMPARE(wasapiSharedPeriodFrames(kPeriods, 1000), 480);
        QCOMPARE(wasapiSharedPeriodFrames(kPeriods, 48), 48);
        QCOMPARE(wasapiSharedPeriodFrames(kPeriods, 49), 96);
    }

    void sharedPeriodWithoutAudioClient3IsTheDefaultPath()
    {
        QCOMPARE(wasapiSharedPeriodFrames(std::nullopt, 0), 0);
        QCOMPARE(wasapiSharedPeriodFrames(std::nullopt, 256), 0);
    }

    void exclusivePeriods()
    {
        QCOMPARE(wasapiAlignedPeriodHns(441, 44100), std::int64_t(100000));
        QCOMPARE(wasapiExclusivePeriodHns(30000, 0, 48000), std::int64_t(30000));
        QCOMPARE(wasapiExclusivePeriodHns(30000, 256, 48000), std::int64_t(53333));
        // A request below the device's minimum runs at the minimum.
        QCOMPARE(wasapiExclusivePeriodHns(30000, 64, 48000), std::int64_t(30000));
        QCOMPARE(wasapiFramesForHns(53333, 48000), 256);
        QCOMPARE(wasapiFramesForHns(100000, 44100), 441);
    }

    void framesToWriteAtEachEvent()
    {
        QCOMPARE(wasapiFramesToWrite(true, 256, 0), 256);
        QCOMPARE(wasapiFramesToWrite(true, 256, 128), 256);
        QCOMPARE(wasapiFramesToWrite(false, 480, 96), 384);
        QCOMPARE(wasapiFramesToWrite(false, 480, 480), 0);
        QCOMPARE(wasapiFramesToWrite(false, 480, 600), 0);
    }

    void listNoticesForAddedRemovedStateAndName()
    {
        for (const WasapiNotification n : {WasapiNotification::DeviceAdded,
                                           WasapiNotification::DeviceRemoved,
                                           WasapiNotification::StateChanged,
                                           WasapiNotification::NameChanged}) {
            QCOMPARE(wasapiNoticeFor(n, WasapiFlow::All, WasapiRole::Console),
                     std::optional<AudioNotice>(AudioNotice::DevicesChanged));
        }
        QVERIFY(!wasapiNoticeFor(WasapiNotification::OtherPropertyChanged, WasapiFlow::All,
                                 WasapiRole::Console)
                     .has_value());
    }

    void defaultNoticesOnlyForTheConsoleRole()
    {
        QCOMPARE(wasapiNoticeFor(WasapiNotification::DefaultChanged, WasapiFlow::Render,
                                 WasapiRole::Console),
                 std::optional<AudioNotice>(AudioNotice::DefaultOutputChanged));
        QCOMPARE(wasapiNoticeFor(WasapiNotification::DefaultChanged, WasapiFlow::Capture,
                                 WasapiRole::Console),
                 std::optional<AudioNotice>(AudioNotice::DefaultInputChanged));
        for (const WasapiRole role : {WasapiRole::Multimedia, WasapiRole::Communications}) {
            for (const WasapiFlow flow : {WasapiFlow::Render, WasapiFlow::Capture}) {
                QVERIFY(!wasapiNoticeFor(WasapiNotification::DefaultChanged, flow, role)
                             .has_value());
            }
        }
    }

    void transportFromEnumeratorAndFormFactor()
    {
        QCOMPARE(wasapiTransport(QStringLiteral("BTHENUM"), 0), AudioTransport::Bluetooth);
        QCOMPARE(wasapiTransport(QStringLiteral("bthenum"), 0), AudioTransport::Bluetooth);
        QCOMPARE(wasapiTransport(QStringLiteral("BTHHFENUM"), 3), AudioTransport::Bluetooth);
        QCOMPARE(wasapiTransport(QStringLiteral("BthHfEnum"), 3), AudioTransport::Bluetooth);
        QCOMPARE(wasapiTransport(QStringLiteral("USB"), 1), AudioTransport::Usb);
        QCOMPARE(wasapiTransport(QStringLiteral("HDAUDIO"), 9), AudioTransport::Hdmi);
        QCOMPARE(wasapiTransport(QStringLiteral("HDAUDIO"), 1), AudioTransport::Unknown);
        QCOMPARE(wasapiTransport(QString(), 0), AudioTransport::Unknown);
        QCOMPARE(kWasapiFormFactorDigitalAudioDisplayDevice, 9);
    }

    void resultsMirrorAudioClientCodes()
    {
        QCOMPARE(wasapiResultFor(0x00000000u), WasapiResult::Ok);
        QCOMPARE(wasapiResultFor(0x00000001u), WasapiResult::Ok);
        QCOMPARE(wasapiResultFor(0x88890004u), WasapiResult::DeviceInvalidated);
        QCOMPARE(wasapiResultFor(0x8889000Au), WasapiResult::DeviceInUse);
        QCOMPARE(wasapiResultFor(0x8889000Eu), WasapiResult::ExclusiveNotAllowed);
        QCOMPARE(wasapiResultFor(0x88890008u), WasapiResult::UnsupportedFormat);
        QCOMPARE(wasapiResultFor(0x88890010u), WasapiResult::ServiceNotRunning);
        QCOMPARE(wasapiResultFor(0x88890019u), WasapiResult::BufferSizeNotAligned);
        QCOMPARE(wasapiResultFor(0x80004005u), WasapiResult::Other);   // E_FAIL
        QCOMPARE(wasapiResultFor(0x88890001u), WasapiResult::Other);
    }

    void openResultsAndStreamEvents()
    {
        QCOMPARE(wasapiOpenResult(WasapiResult::Ok), AudioOpenResult::Opened);
        QCOMPARE(wasapiOpenResult(WasapiResult::DeviceInUse), AudioOpenResult::InUse);
        QCOMPARE(wasapiOpenResult(WasapiResult::ExclusiveNotAllowed), AudioOpenResult::InUse);
        QCOMPARE(wasapiOpenResult(WasapiResult::DeviceInvalidated), AudioOpenResult::NotFound);
        for (const WasapiResult r : {WasapiResult::UnsupportedFormat,
                                     WasapiResult::BufferSizeNotAligned,
                                     WasapiResult::ServiceNotRunning, WasapiResult::Other}) {
            QCOMPARE(wasapiOpenResult(r), AudioOpenResult::Failed);
            QVERIFY(!wasapiStreamEvent(r).has_value());
        }

        using Kind = AudioStreamEvent::Kind;
        QCOMPARE(wasapiStreamEvent(WasapiResult::DeviceInUse), std::optional<Kind>(Kind::DeviceBusy));
        QCOMPARE(wasapiStreamEvent(WasapiResult::ExclusiveNotAllowed),
                 std::optional<Kind>(Kind::DeviceBusy));
        QCOMPARE(wasapiStreamEvent(WasapiResult::DeviceInvalidated),
                 std::optional<Kind>(Kind::DeviceLost));
        QVERIFY(!wasapiStreamEvent(WasapiResult::Ok).has_value());
    }

    void runningStreamFailures()
    {
        using Kind = AudioStreamEvent::Kind;
        QCOMPARE(wasapiRunningStreamEvent(WasapiResult::DeviceInvalidated, false), Kind::DeviceLost);
        QCOMPARE(wasapiRunningStreamEvent(WasapiResult::DeviceInvalidated, true), Kind::FormatChanged);
        QCOMPARE(wasapiRunningStreamEvent(WasapiResult::DeviceInUse, true), Kind::DeviceBusy);
        QCOMPARE(wasapiRunningStreamEvent(WasapiResult::ExclusiveNotAllowed, true), Kind::DeviceBusy);
        QCOMPARE(wasapiRunningStreamEvent(WasapiResult::ServiceNotRunning, true), Kind::ResetRequested);
        QCOMPARE(wasapiRunningStreamEvent(WasapiResult::Other, false), Kind::ResetRequested);
    }
};

QTEST_GUILESS_MAIN(TestWasapiPolicy)
#include "tst_wasapi_policy.moc"
