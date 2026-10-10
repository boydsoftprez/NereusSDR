// =================================================================
// tests/tst_audio_device_catalog.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original acceptance test for the native audio
// device types, the sample format conversion (whose port is attributed in
// DeviceSampleFormat.cpp), the no-device rule and the live device
// catalogue (R-AUD-03, R-AUD-07, R-AUD-32, V-SW-1, V-SW-10).  Fake engines
// only; no device is opened.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 3 (R-AUD-03, R-AUD-07, R-AUD-32).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: Windows test fix (R-AUD-03): the device thread asks for
//               Windows' 1 ms timer, and the rounds check says what it
//               counted. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#include <QtTest>

#include "RealtimeTestLoad.h"
#include "core/IAudioBus.h"
#include "core/audio/AudioDeviceCatalog.h"
#include "core/audio/AudioDeviceTypes.h"
#include "core/audio/AudioTestBarrier.h"
#include "core/audio/DeviceSampleFormat.h"
#include "core/audio/PortAudioBus.h"
#include "fakes/FakeAudioEngineBackend.h"
#include "fakes/FakeMatcherAudioBus.h"

#include <QElapsedTimer>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QThread>

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <timeapi.h>
#endif

using namespace NereusSDR;

namespace {

AudioDeviceInfo device(const QString& id, const QString& name,
                       AudioDeviceDirection direction = AudioDeviceDirection::Output,
                       int channels = 2)
{
    AudioDeviceInfo info;
    info.backend = AudioBackendId::CoreAudio;
    info.direction = direction;
    info.id = id;
    info.name = name;
    info.channelCount = channels;
    return info;
}

bool hasDevice(const QList<AudioDeviceInfo>& list, const QString& id)
{
    for (const AudioDeviceInfo& info : list) {
        if (info.id == id) {
            return true;
        }
    }
    return false;
}

// Windows sleeps in steps of its system timer, 15.6 ms unless a program
// asks for 1 ms; the device thread below paces itself at 2 ms, as a device
// callback would run, so it asks for 1 ms while it runs.  Elsewhere a
// no-op.  (Measured on Windows 11: Sleep(2) takes 15.6 ms, and 2.8 ms
// after timeBeginPeriod(1).)
struct OneMillisecondTimer {
#if defined(Q_OS_WIN)
    OneMillisecondTimer() { m_set = timeBeginPeriod(1) == TIMERR_NOERROR; }
    ~OneMillisecondTimer()
    {
        if (m_set) {
            timeEndPeriod(1);
        }
    }
    OneMillisecondTimer(const OneMillisecondTimer&) = delete;
    OneMillisecondTimer& operator=(const OneMillisecondTimer&) = delete;

private:
    bool m_set = false;
#endif
};

struct CatalogRig {
    std::shared_ptr<FakeAudioEngineBackend> engine = std::make_shared<FakeAudioEngineBackend>();
    std::unique_ptr<AudioDeviceCatalog> catalog;

    explicit CatalogRig(int debounceMs = AudioDeviceCatalog::kDebounceMs)
    {
        engine->setDevices({device(QStringLiteral("spk"), QStringLiteral("Speakers")),
                            device(QStringLiteral("mic"), QStringLiteral("Mic"),
                                   AudioDeviceDirection::Input)});
        engine->setDefault(AudioDeviceDirection::Output, QStringLiteral("spk"));
        std::vector<std::shared_ptr<IAudioEngineBackend>> backends{engine};
        catalog = std::make_unique<AudioDeviceCatalog>(std::move(backends));
        catalog->setDebounceIntervalForTest(debounceMs);
    }
};

} // namespace

class TstAudioDeviceCatalog : public QObject {
    Q_OBJECT

private slots:
    // The time-bounded catalogue cases keep up with the wall clock.
    void cleanup() { NereusSDR::RealtimeTestLoad::printLoadAverageIfFailed(); }

    // -- Keys and labels ------------------------------------------------
    void engineKeysAndLabels()
    {
        struct Row {
            AudioEngineKind kind;
            const char* key;
            const char* label;
            AudioBackendId backend;
        };
        const std::array<Row, 8> rows{{
            {AudioEngineKind::PortAudio, "PortAudio", "Older drivers", AudioBackendId::PortAudio},
            {AudioEngineKind::CoreAudio, "CoreAudio", "Core Audio", AudioBackendId::CoreAudio},
            {AudioEngineKind::WindowsShared, "WindowsShared", "Windows audio, shared", AudioBackendId::Wasapi},
            {AudioEngineKind::WindowsExclusive, "WindowsExclusive", "Windows audio, exclusive", AudioBackendId::Wasapi},
            {AudioEngineKind::Asio, "ASIO", "ASIO", AudioBackendId::Asio},
            {AudioEngineKind::PipeWire, "PipeWire", "PipeWire", AudioBackendId::PipeWire},
            {AudioEngineKind::PulseAudio, "PulseAudio", "PulseAudio", AudioBackendId::PulseAudio},
            {AudioEngineKind::AlsaDirect, "AlsaDirect", "ALSA, direct", AudioBackendId::AlsaDirect},
        }};
        for (const Row& row : rows) {
            QCOMPARE(audioEngineKey(row.kind), QString::fromLatin1(row.key));
            QCOMPARE(audioEngineLabel(row.kind), QString::fromLatin1(row.label));
            QCOMPARE(audioBackendFor(row.kind), row.backend);
            const std::optional<AudioEngineKind> back = audioEngineFromKey(QString::fromLatin1(row.key));
            QVERIFY(back.has_value());
            QCOMPARE(*back, row.kind);
        }
        QVERIFY(!audioEngineFromKey(QStringLiteral("Wasapi")).has_value());
        QVERIFY(!audioEngineFromKey(QString()).has_value());
        QVERIFY(!audioEngineFromKey(QStringLiteral("asio")).has_value());
    }

    void micChannelKeys()
    {
        QCOMPARE(micChannelKey(MicChannelPick::Left), QStringLiteral("Left"));
        QCOMPARE(micChannelKey(MicChannelPick::Right), QStringLiteral("Right"));
        QCOMPARE(micChannelKey(MicChannelPick::Both), QStringLiteral("Both"));
        QCOMPARE(micChannelFromKey(QStringLiteral("Right")), std::optional<MicChannelPick>(MicChannelPick::Right));
        QCOMPARE(micChannelFromKey(QStringLiteral("Both")), std::optional<MicChannelPick>(MicChannelPick::Both));
        QCOMPARE(micChannelFromKey(QStringLiteral("Left")), std::optional<MicChannelPick>(MicChannelPick::Left));
        QVERIFY(!micChannelFromKey(QStringLiteral("Stereo")).has_value());
    }

    void channelPairs()
    {
        using Pairs = QList<AudioChannelPair>;
        QCOMPARE(audioChannelPairs(2), (Pairs{{1, 2}}));
        QCOMPARE(audioChannelPairs(1), (Pairs{{1, 1}}));
        QCOMPARE(audioChannelPairs(10), (Pairs{{1, 2}, {3, 2}, {5, 2}, {7, 2}, {9, 2}}));
        QCOMPARE(audioChannelPairs(5), (Pairs{{1, 2}, {3, 2}, {5, 1}}));
        QVERIFY(audioChannelPairs(0).isEmpty());
    }

    void pairLabels()
    {
        QCOMPARE(audioPairLabel(AudioDeviceDirection::Output, {3, 2}), QStringLiteral("Outputs 3-4"));
        QCOMPARE(audioPairLabel(AudioDeviceDirection::Input, {1, 2}), QStringLiteral("Inputs 1-2"));
        QCOMPARE(audioPairLabel(AudioDeviceDirection::Output, {5, 1}), QStringLiteral("Output 5"));
        QCOMPARE(audioPairLabel(AudioDeviceDirection::Input, {5, 1}), QStringLiteral("Input 5"));

        AudioDeviceInfo focusrite = device(QStringLiteral("f"), QStringLiteral("Focusrite USB ASIO"),
                                           AudioDeviceDirection::Output, 10);
        const QString expected = QStringLiteral("Focusrite USB ASIO") + QLatin1Char(' ')
            + QChar(0x00B7) + QLatin1Char(' ') + QStringLiteral("Outputs 3-4");
        QCOMPARE(audioDeviceEntryLabel(focusrite, {3, 2}), expected);
        focusrite.channelCount = 2;
        QCOMPARE(audioDeviceEntryLabel(focusrite, {1, 2}), QStringLiteral("Focusrite USB ASIO"));
        focusrite.channelCount = 1;
        QCOMPARE(audioDeviceEntryLabel(focusrite, {1, 1}), QStringLiteral("Focusrite USB ASIO"));
    }

    // -- Sample formats -------------------------------------------------
    void writeIntegerFormats()
    {
        const std::array<float, 2> stereo{0.5f, -0.25f};

        std::array<std::int16_t, 4> i16{};
        i16.fill(99);
        writeStereoToDevice(stereo.data(), 1, i16.data(), DeviceSampleFormat::Int16, 4, {3, 2}, true, nullptr);
        QCOMPARE(i16, (std::array<std::int16_t, 4>{0, 0, 16384, -8192}));

        std::array<std::int32_t, 4> i32{};
        i32.fill(99);
        writeStereoToDevice(stereo.data(), 1, i32.data(), DeviceSampleFormat::Int32, 4, {3, 2}, true, nullptr);
        QCOMPARE(i32, (std::array<std::int32_t, 4>{0, 0, 1073741824, -536870912}));

        std::array<unsigned char, 6> i24{};
        writeStereoToDevice(stereo.data(), 1, i24.data(), DeviceSampleFormat::Int24Packed, 2, {1, 2}, true, nullptr);
        QCOMPARE(i24, (std::array<unsigned char, 6>{0x00, 0x00, 0x40, 0x00, 0x00, 0xE0}));

        std::array<std::int32_t, 2> i24in32{};
        writeStereoToDevice(stereo.data(), 1, i24in32.data(), DeviceSampleFormat::Int24In32Lsb, 2, {1, 2}, true, nullptr);
        QCOMPARE(i24in32, (std::array<std::int32_t, 2>{4194304, -2097152}));
    }

    void writeClampsAndTruncates()
    {
        const std::array<float, 2> loud{1.5f, -1.5f};
        std::array<std::int32_t, 2> i32{};
        writeStereoToDevice(loud.data(), 1, i32.data(), DeviceSampleFormat::Int32, 2, {1, 2}, true, nullptr);
        QCOMPARE(i32[0], std::int32_t(2147483647));
        QCOMPARE(i32[1], std::int32_t(-2147483647 - 1));

        std::array<std::int16_t, 2> i16{};
        writeStereoToDevice(loud.data(), 1, i16.data(), DeviceSampleFormat::Int16, 2, {1, 2}, true, nullptr);
        QCOMPARE(i16, (std::array<std::int16_t, 2>{32767, -32768}));

        // Truncation toward zero: 1.9 and -1.9 steps of 2^-15.
        const std::array<float, 2> fine{float(1.9 / 32768.0), float(-1.9 / 32768.0)};
        writeStereoToDevice(fine.data(), 1, i16.data(), DeviceSampleFormat::Int16, 2, {1, 2}, true, nullptr);
        QCOMPARE(i16, (std::array<std::int16_t, 2>{1, -1}));
    }

    void writeMonoAndFloat()
    {
        const std::array<float, 2> stereo{0.5f, -0.25f};
        std::array<float, 1> mono{};
        writeStereoToDevice(stereo.data(), 1, mono.data(), DeviceSampleFormat::Float32, 1, {1, 2}, true, nullptr);
        QCOMPARE(mono[0], 0.125f);

        // A one-channel pair on a five-channel device.
        std::array<float, 5> five{};
        five.fill(9.0f);
        writeStereoToDevice(stereo.data(), 1, five.data(), DeviceSampleFormat::Float32, 5, {5, 1}, true, nullptr);
        QCOMPARE(five, (std::array<float, 5>{0.0f, 0.0f, 0.0f, 0.0f, 0.125f}));

        std::array<double, 4> f64{};
        writeStereoToDevice(stereo.data(), 1, f64.data(), DeviceSampleFormat::Float64, 4, {1, 2}, true, nullptr);
        QCOMPARE(f64, (std::array<double, 4>{0.5, -0.25, 0.0, 0.0}));
    }

    void writePlanarMatchesInterleaved()
    {
        const std::array<float, 4> stereo{0.5f, -0.25f, 0.125f, 0.75f};
        std::array<std::int32_t, 8> interleaved{};
        writeStereoToDevice(stereo.data(), 2, interleaved.data(), DeviceSampleFormat::Int32, 4, {3, 2}, true, nullptr);
        std::array<std::array<std::int32_t, 2>, 4> planesData{};
        std::array<void*, 4> planes{planesData[0].data(), planesData[1].data(),
                                    planesData[2].data(), planesData[3].data()};
        writeStereoToDevice(stereo.data(), 2, nullptr, DeviceSampleFormat::Int32, 4, {3, 2}, false, planes.data());
        for (int f = 0; f < 2; ++f) {
            for (int c = 0; c < 4; ++c) {
                QCOMPARE(planesData[std::size_t(c)][std::size_t(f)], interleaved[std::size_t(f * 4 + c)]);
            }
        }
    }

    void readMicPicks()
    {
        const std::array<std::int32_t, 2> in{1073741824, 536870912};
        std::array<float, 2> out{};
        readDeviceToStereo(in.data(), nullptr, true, DeviceSampleFormat::Int32, 2, {1, 2},
                           MicChannelPick::Left, 1, out.data());
        QCOMPARE(out, (std::array<float, 2>{0.5f, 0.5f}));
        readDeviceToStereo(in.data(), nullptr, true, DeviceSampleFormat::Int32, 2, {1, 2},
                           MicChannelPick::Right, 1, out.data());
        QCOMPARE(out, (std::array<float, 2>{0.25f, 0.25f}));
        readDeviceToStereo(in.data(), nullptr, true, DeviceSampleFormat::Int32, 2, {1, 2},
                           MicChannelPick::Both, 1, out.data());
        QCOMPARE(out, (std::array<float, 2>{0.75f, 0.75f}));

        // Planar input through planes gives the same values.
        const std::array<std::int32_t, 1> left{1073741824};
        const std::array<std::int32_t, 1> right{536870912};
        const std::array<const void*, 2> planes{left.data(), right.data()};
        const std::array<MicChannelPick, 3> picks{MicChannelPick::Left, MicChannelPick::Right, MicChannelPick::Both};
        for (MicChannelPick pick : picks) {
            std::array<float, 2> a{};
            std::array<float, 2> b{};
            readDeviceToStereo(in.data(), nullptr, true, DeviceSampleFormat::Int32, 2, {1, 2}, pick, 1, a.data());
            readDeviceToStereo(nullptr, planes.data(), false, DeviceSampleFormat::Int32, 2, {1, 2}, pick, 1, b.data());
            QCOMPARE(b, a);
        }
    }

    void readOtherFormatsAndPairs()
    {
        // Pair 3-4 of a four-channel Int16 device.
        const std::array<std::int16_t, 4> i16{100, 200, 16384, -8192};
        std::array<float, 2> out{};
        readDeviceToStereo(i16.data(), nullptr, true, DeviceSampleFormat::Int16, 4, {3, 2},
                           MicChannelPick::Right, 1, out.data());
        QCOMPARE(out, (std::array<float, 2>{-0.25f, -0.25f}));

        const std::array<unsigned char, 6> i24{0x00, 0x00, 0x40, 0x00, 0x00, 0xE0};
        readDeviceToStereo(i24.data(), nullptr, true, DeviceSampleFormat::Int24Packed, 2, {1, 2},
                           MicChannelPick::Both, 1, out.data());
        QCOMPARE(out, (std::array<float, 2>{0.25f, 0.25f}));

        // The high byte of an Int24In32Lsb word carries nothing.
        const std::array<std::uint32_t, 2> i24in32{0x7F400000u, 0x00E00000u};
        readDeviceToStereo(i24in32.data(), nullptr, true, DeviceSampleFormat::Int24In32Lsb, 2, {1, 2},
                           MicChannelPick::Left, 1, out.data());
        QCOMPARE(out, (std::array<float, 2>{0.5f, 0.5f}));
        readDeviceToStereo(i24in32.data(), nullptr, true, DeviceSampleFormat::Int24In32Lsb, 2, {1, 2},
                           MicChannelPick::Right, 1, out.data());
        QCOMPARE(out, (std::array<float, 2>{-0.25f, -0.25f}));

        // One channel: every pick hears it once.
        const std::array<float, 1> mono{0.3f};
        readDeviceToStereo(mono.data(), nullptr, true, DeviceSampleFormat::Float32, 1, {1, 1},
                           MicChannelPick::Both, 1, out.data());
        QCOMPARE(out, (std::array<float, 2>{0.3f, 0.3f}));
    }

    // -- No-device rule -------------------------------------------------
    void testRunIsBarred()
    {
        QVERIFY(audioDevicesBarredForTestRun());
        QCOMPARE(PortAudioBus::portAudioBarredForTestRun(), audioDevicesBarredForTestRun());
    }

    // -- Catalogue ------------------------------------------------------
    void startListsOnceOnItsOwnThread()
    {
        CatalogRig rig;
        QSignalSpy changed(rig.catalog.get(), &IAudioDeviceCatalog::devicesChanged);
        rig.catalog->start();
        QCOMPARE(rig.engine->enumerateCalls(), 1);
        QVERIFY(rig.engine->hasNoticeSink());
        QCOMPARE(rig.catalog->backends(), QList<AudioBackendId>{AudioBackendId::CoreAudio});
        QVERIFY(rig.catalog->backendRunning(AudioBackendId::CoreAudio));
        QVERIFY(!rig.catalog->backendRunning(AudioBackendId::Asio));
        const QList<AudioDeviceInfo> outs = rig.catalog->devices(AudioBackendId::CoreAudio, AudioDeviceDirection::Output);
        QCOMPARE(outs.size(), 1);
        QCOMPARE(outs.front().id, QStringLiteral("spk"));
        QVERIFY(outs.front().isDefault);
        QCOMPARE(rig.catalog->devices(AudioBackendId::CoreAudio, AudioDeviceDirection::Input).size(), 1);
        const std::optional<AudioDeviceInfo> def = rig.catalog->defaultDevice(AudioBackendId::CoreAudio, AudioDeviceDirection::Output);
        QVERIFY(def.has_value());
        QCOMPARE(def->name, QStringLiteral("Speakers"));
        QVERIFY(!rig.catalog->defaultDevice(AudioBackendId::CoreAudio, AudioDeviceDirection::Input).has_value());
        QCOMPARE(changed.count(), 1);

        const std::vector<QThread*> threads = rig.engine->enumerateThreads();
        QCOMPARE(threads.size(), std::size_t(1));
        QVERIFY(threads.front() != QThread::currentThread());
        for (QThread* t : rig.engine->defaultThreads()) {
            QCOMPARE(t, threads.front());
        }

        // The queued copy of the first list does not signal again.
        QTest::qWait(50);
        QCOMPARE(changed.count(), 1);

        rig.catalog->stop();
        QVERIFY(!rig.engine->hasNoticeSink());
    }

    void addedDeviceShowsWithinDebounce()
    {
        CatalogRig rig;
        rig.catalog->start();
        QSignalSpy changed(rig.catalog.get(), &IAudioDeviceCatalog::devicesChanged);
        rig.engine->addDevice(device(QStringLiteral("usb"), QStringLiteral("USB Codec")));
        QElapsedTimer clock;
        clock.start();
        rig.engine->postNotice(AudioNotice::DevicesChanged);
        QTRY_VERIFY_WITH_TIMEOUT(
            hasDevice(rig.catalog->devices(AudioBackendId::CoreAudio, AudioDeviceDirection::Output),
                      QStringLiteral("usb")),
            3000);
        const qint64 elapsed = clock.elapsed();
        QVERIFY2(elapsed >= AudioDeviceCatalog::kDebounceMs - 20, qPrintable(QString::number(elapsed)));
        QVERIFY2(elapsed <= AudioDeviceCatalog::kDebounceMs + 100, qPrintable(QString::number(elapsed)));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(rig.engine->enumerateCalls(), 2);
        for (QThread* t : rig.engine->enumerateThreads()) {
            QVERIFY(t != QThread::currentThread());
            QCOMPARE(t, rig.engine->enumerateThreads().front());
        }
    }

    void tenNoticesRelistOnce()
    {
        CatalogRig rig;
        rig.catalog->start();
        QSignalSpy changed(rig.catalog.get(), &IAudioDeviceCatalog::devicesChanged);
        const int before = rig.engine->enumerateCalls();
        rig.engine->addDevice(device(QStringLiteral("usb"), QStringLiteral("USB Codec")));
        for (int i = 0; i < 10; ++i) {
            rig.engine->postNotice(AudioNotice::DevicesChanged);
            QTest::qWait(20);
        }
        QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 3000);
        // Long enough for a second window to have run, had one started.
        QTest::qWait(AudioDeviceCatalog::kDebounceMs + 200);
        QCOMPARE(rig.engine->enumerateCalls() - before, 1);
        QCOMPARE(changed.count(), 1);
    }

    void laterNoticeDoesNotRestartWindow()
    {
        CatalogRig rig;
        rig.catalog->start();
        QSignalSpy changed(rig.catalog.get(), &IAudioDeviceCatalog::devicesChanged);
        const int before = rig.engine->enumerateCalls();
        rig.engine->addDevice(device(QStringLiteral("usb"), QStringLiteral("USB Codec")));
        QElapsedTimer clock;
        clock.start();
        rig.engine->postNotice(AudioNotice::DevicesChanged);
        QTest::qWait(300);
        rig.engine->postNotice(AudioNotice::DevicesChanged);
        QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 3000);
        const qint64 elapsed = clock.elapsed();
        // A restarted window would end near 800 ms.
        QVERIFY2(elapsed < AudioDeviceCatalog::kDebounceMs + 200, qPrintable(QString::number(elapsed)));
        QTest::qWait(AudioDeviceCatalog::kDebounceMs + 200);
        QCOMPARE(rig.engine->enumerateCalls() - before, 1);
    }

    void defaultChangeSignalsWithoutDebounce()
    {
        CatalogRig rig;
        rig.engine->addDevice(device(QStringLiteral("hdmi"), QStringLiteral("HDMI")));
        rig.catalog->start();
        QSignalSpy defaults(rig.catalog.get(), &IAudioDeviceCatalog::defaultChanged);
        const int before = rig.engine->enumerateCalls();
        rig.engine->setDefault(AudioDeviceDirection::Output, QStringLiteral("hdmi"));
        QElapsedTimer clock;
        clock.start();
        rig.engine->postNotice(AudioNotice::DefaultOutputChanged);
        QTRY_COMPARE_WITH_TIMEOUT(defaults.count(), 1, 3000);
        QVERIFY2(clock.elapsed() < AudioDeviceCatalog::kDebounceMs / 2,
                 qPrintable(QString::number(clock.elapsed())));
        QCOMPARE(defaults.front().front().value<AudioDeviceDirection>(), AudioDeviceDirection::Output);
        QCOMPARE(rig.engine->enumerateCalls(), before);
        const std::optional<AudioDeviceInfo> def = rig.catalog->defaultDevice(AudioBackendId::CoreAudio, AudioDeviceDirection::Output);
        QVERIFY(def.has_value());
        QCOMPARE(def->id, QStringLiteral("hdmi"));
        for (const AudioDeviceInfo& info : rig.catalog->devices(AudioBackendId::CoreAudio, AudioDeviceDirection::Output)) {
            QCOMPARE(info.isDefault, info.id == QStringLiteral("hdmi"));
        }

        rig.engine->setDefault(AudioDeviceDirection::Input, QStringLiteral("mic"));
        rig.engine->postNotice(AudioNotice::DefaultInputChanged);
        QTRY_COMPARE_WITH_TIMEOUT(defaults.count(), 2, 3000);
        QCOMPARE(defaults.at(1).front().value<AudioDeviceDirection>(), AudioDeviceDirection::Input);
        QCOMPARE(rig.engine->enumerateCalls(), before);
    }

    void removeAndReaddRelistsBothTimes()
    {
        CatalogRig rig(100);
        rig.catalog->start();
        const auto outputs = [&] {
            return rig.catalog->devices(AudioBackendId::CoreAudio, AudioDeviceDirection::Output);
        };
        QVERIFY(hasDevice(outputs(), QStringLiteral("spk")));
        rig.engine->removeDevice(QStringLiteral("spk"), AudioDeviceDirection::Output);
        rig.engine->postNotice(AudioNotice::DevicesChanged);
        QTRY_VERIFY_WITH_TIMEOUT(!hasDevice(outputs(), QStringLiteral("spk")), 3000);
        QCOMPARE(rig.engine->enumerateCalls(), 2);
        rig.engine->addDevice(device(QStringLiteral("spk"), QStringLiteral("Speakers")));
        rig.engine->postNotice(AudioNotice::DevicesChanged);
        QTRY_VERIFY_WITH_TIMEOUT(hasDevice(outputs(), QStringLiteral("spk")), 3000);
        QCOMPARE(rig.engine->enumerateCalls(), 3);
    }

    void openBusKeepsReadingThroughRelists()
    {
        CatalogRig rig(50);
        rig.catalog->start();

        AudioStreamRequest request;
        request.deviceId = QStringLiteral("other");
        std::unique_ptr<IAudioBus> bus = rig.engine->createOutput(request);
        QCOMPARE(rig.engine->outputRequests().size(), std::size_t(1));
        QCOMPARE(rig.engine->outputRequests().front().deviceId, QStringLiteral("other"));
        auto* fake = rig.engine->lastOutput();
        QVERIFY(fake == bus.get());
        QVERIFY(bus->takesStereoMix());
        QVERIFY(bus->open(AudioFormat{}));
        QVERIFY(bus->matcherStats().has_value());
        QVERIFY(bus->delayParts().matcherFillMs >= 0.0);
        std::atomic<int> events{0};
        bus->setStreamEventSink([&events](const AudioStreamEvent&) { events.fetch_add(1); });

        // A device thread: writes a block and reads a block every 2 ms.
        const OneMillisecondTimer timer;
        constexpr int kBlock = 96;
        std::atomic<bool> run{true};
        std::atomic<qint64> worstGapMs{0};
        std::atomic<int> rounds{0};
        std::thread deviceThread([&] {
            std::vector<float> block(std::size_t(2 * kBlock), 0.25f);
            QElapsedTimer gap;
            gap.start();
            while (run.load()) {
                bus->push(reinterpret_cast<const char*>(block.data()),
                          qint64(block.size() * sizeof(float)));
                fake->pumpForTest(kBlock);
                const qint64 g = gap.restart();
                if (rounds.load() > 0 && g > worstGapMs.load()) {
                    worstGapMs.store(g);
                }
                rounds.fetch_add(1);
                // QThread::msleep is Sleep() on Windows, which honours the
                // 1 ms timer; MinGW's std::this_thread::sleep_for does not.
                QThread::msleep(2);
            }
        });

        QTest::qWait(100);
        const int roundsBefore = rounds.load();
        const std::int64_t pumpedBefore = fake->pumpedFrames();
        for (int i = 0; i < 5; ++i) {
            rig.engine->addDevice(device(QStringLiteral("dev%1").arg(i), QStringLiteral("Dev")));
            rig.engine->postNotice(AudioNotice::DevicesChanged);
            QTest::qWait(30);
            rig.engine->removeDevice(QStringLiteral("dev%1").arg(i), AudioDeviceDirection::Output);
            rig.engine->postNotice(AudioNotice::DevicesChanged);
            rig.engine->postNotice(AudioNotice::DefaultOutputChanged);
            QTest::qWait(80);
        }
        run.store(false);
        deviceThread.join();

        QVERIFY(rig.engine->enumerateCalls() >= 3);
        QVERIFY(bus->isOpen());
        QVERIFY2(rounds.load() - roundsBefore > 50,
                 qPrintable(QStringLiteral("%1 rounds, worst gap %2 ms")
                                .arg(rounds.load() - roundsBefore)
                                .arg(worstGapMs.load())));
        QVERIFY(fake->pumpedFrames() > pumpedBefore);
        QCOMPARE(events.load(), 0);
        QVERIFY2(worstGapMs.load() < 100, qPrintable(QString::number(worstGapMs.load())));
        // The last read carried the written level: the matcher kept playing.
        QVERIFY(std::abs(fake->lastRead()[0] - 0.25f) < 0.01f);

        fake->emitEventForTest(AudioStreamEvent{AudioStreamEvent::Kind::DeviceLost, QStringLiteral("gone")});
        QCOMPARE(events.load(), 1);
        bus->close();
    }

    void noticeFromForeignThread()
    {
        CatalogRig rig;
        rig.catalog->setDebounceIntervalForTest(50);
        rig.catalog->start();
        rig.engine->addDevice(device(QStringLiteral("usb"), QStringLiteral("USB Codec")));
        std::thread poster([&] { rig.engine->postNotice(AudioNotice::DevicesChanged); });
        poster.join();
        QTRY_VERIFY_WITH_TIMEOUT(
            hasDevice(rig.catalog->devices(AudioBackendId::CoreAudio, AudioDeviceDirection::Output),
                      QStringLiteral("usb")),
            3000);
        for (QThread* t : rig.engine->enumerateThreads()) {
            QVERIFY(t != QThread::currentThread());
        }
    }

    void startGivesUpAfterThreeSeconds()
    {
        CatalogRig rig;
        rig.engine->holdEnumerate();
        QSignalSpy changed(rig.catalog.get(), &IAudioDeviceCatalog::devicesChanged);
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("Audio device list took longer than 3000 ms; continuing without it; waiting on CoreAudio")));
        QElapsedTimer clock;
        clock.start();
        rig.catalog->start();
        const qint64 elapsed = clock.elapsed();
        QVERIFY2(elapsed >= AudioDeviceCatalog::kStartWaitMs - 50, qPrintable(QString::number(elapsed)));
        QVERIFY2(elapsed < AudioDeviceCatalog::kStartWaitMs + 1000, qPrintable(QString::number(elapsed)));
        QVERIFY(rig.catalog->devices(AudioBackendId::CoreAudio, AudioDeviceDirection::Output).isEmpty());
        rig.engine->releaseEnumerate();
        QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 3000);
        QVERIFY(hasDevice(rig.catalog->devices(AudioBackendId::CoreAudio, AudioDeviceDirection::Output),
                          QStringLiteral("spk")));
    }

    void stopGivesUpOnHungBackend()
    {
        auto rig = std::make_unique<CatalogRig>(10);
        rig->catalog->start();
        std::shared_ptr<FakeAudioEngineBackend> engine = rig->engine;
        engine->holdEnumerate();
        engine->postNotice(AudioNotice::DevicesChanged);
        // The re-list is inside enumerate() and stays there.
        QTRY_COMPARE_WITH_TIMEOUT(engine->enumerateCalls(), 2, 3000);

        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("Audio device list did not stop within 3000 ms; leaving it to finish; waiting on CoreAudio")));
        QElapsedTimer clock;
        clock.start();
        rig->catalog->stop();
        const qint64 elapsed = clock.elapsed();
        QVERIFY2(elapsed >= AudioDeviceCatalog::kStopWaitMs - 50, qPrintable(QString::number(elapsed)));
        QVERIFY2(elapsed < AudioDeviceCatalog::kStopWaitMs + 1000, qPrintable(QString::number(elapsed)));
        // Not cleared while the thread is still inside the backend: the
        // thread clears it once the call returns.
        QVERIFY(engine->hasNoticeSink());

        // The catalogue goes away while its thread is still inside the
        // backend; releasing the backend afterwards must touch nothing freed.
        QSignalSpy destroyed(rig->catalog.get(), &QObject::destroyed);
        rig.reset();
        QCOMPARE(destroyed.count(), 1);
        QVERIFY(engine.use_count() > 1);   // the orphaned thread still holds it
        engine->releaseEnumerate();
        // The thread finishes, its worker and State go, and the thread
        // object is deleted from this thread's queue.
        QTRY_COMPARE_WITH_TIMEOUT(engine.use_count(), long(1), 3000);
        QVERIFY(!engine->hasNoticeSink());
        QCOMPARE(engine->sinkSetsDuringCalls(), 0);
        QTest::qWait(50);
    }

    // A Rescan asked for before stop() left the thread behind never runs:
    // that thread calls no backend after its call returns.
    void stoppedRunRescansNothing()
    {
        auto native = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::CoreAudio);
        auto older = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::PortAudio);
        {
            std::vector<std::shared_ptr<IAudioEngineBackend>> backends{native, older};
            AudioDeviceCatalog catalog(std::move(backends));
            catalog.setDebounceIntervalForTest(10);
            catalog.start();
            native->holdEnumerate();
            native->postNotice(AudioNotice::DevicesChanged);
            QTRY_COMPARE_WITH_TIMEOUT(native->enumerateCalls(), 2, 3000);
            catalog.rescanOlderDrivers();   // queued behind the held list
            QTest::ignoreMessage(QtWarningMsg,
                                 QRegularExpression(QStringLiteral("Audio device list did not stop within 3000 ms")));
            catalog.stop();
        }
        native->releaseEnumerate();
        QTRY_COMPARE_WITH_TIMEOUT(native.use_count(), long(1), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(older.use_count(), long(1), 3000);
        QCOMPARE(older->rescanCount(), 0);
        QCOMPARE(native->enumerateCalls(), 2);
        QCOMPARE(native->sinkSetsDuringCalls(), 0);
    }

    void restartSkipsBackendStillInsideAnOldCall()
    {
        CatalogRig rig(10);
        rig.catalog->start();
        rig.engine->holdEnumerate();
        rig.engine->postNotice(AudioNotice::DevicesChanged);
        QTRY_COMPARE_WITH_TIMEOUT(rig.engine->enumerateCalls(), 2, 3000);

        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("Audio device list did not stop within 3000 ms")));
        rig.catalog->stop();

        // The left-behind thread is still inside enumerate(): the new run
        // lists the backend as not running and never calls it.
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("Audio device list skips CoreAudio while an earlier call into it has not returned")));
        QSignalSpy defaults(rig.catalog.get(), &IAudioDeviceCatalog::defaultChanged);
        QElapsedTimer clock;
        clock.start();
        rig.catalog->start();
        QVERIFY2(clock.elapsed() < 1000, qPrintable(QString::number(clock.elapsed())));
        // The skipped backend keeps the default it last listed.
        QTest::qWait(50);
        QCOMPARE(defaults.count(), 0);
        QVERIFY(!rig.catalog->backendRunning(AudioBackendId::CoreAudio));
        QVERIFY(rig.catalog->devices(AudioBackendId::CoreAudio, AudioDeviceDirection::Output).isEmpty());
        QCOMPARE(rig.catalog->backends(), QList<AudioBackendId>{AudioBackendId::CoreAudio});
        const int defaultsBefore = rig.engine->defaultCalls();
        rig.engine->postNotice(AudioNotice::DefaultOutputChanged);
        rig.engine->postNotice(AudioNotice::DevicesChanged);
        QTest::qWait(150);
        QCOMPARE(rig.engine->enumerateCalls(), 2);
        QCOMPARE(rig.engine->defaultCalls(), defaultsBefore);

        // Once the old call returns, the new run hears the backend's
        // notices, and the next rescan lists it.
        rig.engine->releaseEnumerate();
        QTest::qWait(100);
        QCOMPARE(rig.engine->sinkSetsDuringCalls(), 0);
        rig.engine->setDefault(AudioDeviceDirection::Output, QStringLiteral("other"));
        rig.engine->postNotice(AudioNotice::DefaultOutputChanged);
        QTRY_COMPARE_WITH_TIMEOUT(defaults.count(), 1, 3000);
        rig.engine->setDefault(AudioDeviceDirection::Output, QStringLiteral("spk"));
        rig.catalog->rescanOlderDrivers();
        QTRY_VERIFY_WITH_TIMEOUT(rig.catalog->backendRunning(AudioBackendId::CoreAudio), 3000);
        QVERIFY(hasDevice(rig.catalog->devices(AudioBackendId::CoreAudio, AudioDeviceDirection::Output),
                          QStringLiteral("spk")));
        QCOMPARE(rig.engine->enumerateCalls(), 3);
        QCOMPARE(rig.engine->maxConcurrentCalls(), 1);
    }

    void notRunningBackendListsNothing()
    {
        CatalogRig rig;
        rig.engine->setRunning(false);
        rig.catalog->start();
        QVERIFY(!rig.catalog->backendRunning(AudioBackendId::CoreAudio));
        QCOMPARE(rig.engine->enumerateCalls(), 0);
        QVERIFY(rig.catalog->devices(AudioBackendId::CoreAudio, AudioDeviceDirection::Output).isEmpty());
        QCOMPARE(rig.catalog->backends(), QList<AudioBackendId>{AudioBackendId::CoreAudio});

        QSignalSpy changed(rig.catalog.get(), &IAudioDeviceCatalog::devicesChanged);
        rig.engine->setRunning(true);
        rig.engine->postNotice(AudioNotice::DevicesChanged);
        QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 3000);
        QVERIFY(rig.catalog->backendRunning(AudioBackendId::CoreAudio));
    }

    void rescanOlderDriversRescansPortAudioOnly()
    {
        auto older = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::PortAudio);
        auto native = std::make_shared<FakeAudioEngineBackend>(AudioBackendId::CoreAudio);
        std::vector<std::shared_ptr<IAudioEngineBackend>> backends{native, older};
        AudioDeviceCatalog catalog(std::move(backends));
        catalog.start();
        QCOMPARE(catalog.backends(), (QList<AudioBackendId>{AudioBackendId::CoreAudio, AudioBackendId::PortAudio}));
        AudioDeviceInfo pa = device(QStringLiteral("pa"), QStringLiteral("Line"));
        pa.backend = AudioBackendId::PortAudio;
        pa.hostApi = QStringLiteral("MME");
        older->addDevice(pa);
        catalog.rescanOlderDrivers();
        QTRY_VERIFY_WITH_TIMEOUT(
            hasDevice(catalog.devices(AudioBackendId::PortAudio, AudioDeviceDirection::Output),
                      QStringLiteral("pa")),
            3000);
        QCOMPARE(older->rescanCount(), 1);
        QCOMPARE(native->rescanCount(), 0);
        QVERIFY(catalog.devices(AudioBackendId::CoreAudio, AudioDeviceDirection::Output).isEmpty());
    }

    void fakeInputDeliversToSink()
    {
        struct Sink final : IAudioInputSink {
            int frames = 0;
            float first = 0.0f;
            int rate = 0;
            void onInput(const float* stereo, int n, int sampleRate, std::int64_t) override
            {
                frames += n;
                first = stereo[0];
                rate = sampleRate;
            }
        } sink;
        FakeAudioEngineBackend engine;
        AudioStreamRequest request;
        request.direction = AudioDeviceDirection::Input;
        request.sampleRate = 44100;
        std::unique_ptr<IAudioInputStream> input = engine.createInput(request, MicChannelPick::Right, &sink);
        QCOMPARE(engine.inputRequests().size(), std::size_t(1));
        QCOMPARE(engine.inputRequests().front().pick, MicChannelPick::Right);
        QVERIFY(input->open());
        engine.lastInput()->setInputSamples({0.5f, 0.5f});
        QCOMPARE(engine.lastInput()->pumpForTest(64), 64);
        QCOMPARE(sink.frames, 64);
        QCOMPARE(sink.first, 0.5f);
        QCOMPARE(sink.rate, 44100);
        QCOMPARE(input->sampleRate(), 44100);
    }
};

QTEST_GUILESS_MAIN(TstAudioDeviceCatalog)
#include "tst_audio_device_catalog.moc"
