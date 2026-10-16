// =================================================================
// tests/tst_mac_voice_isolation.cpp  (NereusSDR-native)
// =================================================================
// no-port-check: NereusSDR-original.
//
// The macOS voice isolation opt-out sends one Core Audio property set. These
// tests pin what it sends and how it reads the answer, through a fake setter:
// the real call reconfigures every audio device on the machine, so a test
// must not make it. They also pin the window's run of it on a thread of its
// own: started once, and a waiter held until it returns.
//
// Modification history (NereusSDR):
//   2026-10-10  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//               The property set, and no Core Audio call in a test run.
//   2026-10-15  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//               The status it answered with; the run on a thread of its own
//               and the wait for it, which gives up after its limit.
// =================================================================
#include "core/MacVoiceIsolation.h"

#include <QRegularExpression>
#include <QScopeGuard>
#include <QSemaphore>
#include <QThread>
#include <QtTest/QtTest>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

using NereusSDR::VoiceIsolationOptOut;
using NereusSDR::VoiceIsolationOptOutReport;

namespace {

struct Captured {
    int calls = 0;
    AudioObjectID objectId = 0;
    AudioObjectPropertyAddress address = {};
    UInt32 qualifierSize = 0;
    CFStringRef qualifier = nullptr;
    UInt32 dataSize = 0;
    UInt32 data = 0xFFFFFFFF;
    OSStatus answer = kAudioHardwareNoError;
};
Captured g_captured;

OSStatus fakeSetProperty(AudioObjectID objectId, const AudioObjectPropertyAddress* address,
                         UInt32 qualifierSize, const void* qualifier,
                         UInt32 dataSize, const void* data)
{
    ++g_captured.calls;
    g_captured.objectId = objectId;
    g_captured.address = *address;
    g_captured.qualifierSize = qualifierSize;
    g_captured.qualifier = qualifier ? *static_cast<const CFStringRef*>(qualifier) : nullptr;
    g_captured.dataSize = dataSize;
    if (data && dataSize == sizeof(UInt32)) {
        g_captured.data = *static_cast<const UInt32*>(data);
    }
    return g_captured.answer;
}

// How long a held opt-out must keep a waiter waiting, and how long a
// released one may take to let it go.
constexpr int kHeldMs = 200;
constexpr int kReleasedMs = 5000;

} // namespace

class TstMacVoiceIsolation : public QObject {
    Q_OBJECT
private slots:
    void init()
    {
        g_captured = Captured{};
        NereusSDR::resetSystemVoiceIsolationOptOutForTest();
    }

    void cleanup() { NereusSDR::resetSystemVoiceIsolationOptOutForTest(); }

    void disablesVoiceIsolationDspForTheBundle()
    {
        const CFStringRef bundleId = CFSTR("com.example.app");
        QCOMPARE(NereusSDR::optOutOfSystemVoiceIsolationWith(&fakeSetProperty, bundleId),
                 VoiceIsolationOptOut::Applied);
        QCOMPARE(g_captured.calls, 1);
        QCOMPARE(g_captured.objectId, AudioObjectID(kAudioObjectSystemObject));
        QCOMPARE(g_captured.address.mSelector, AudioObjectPropertySelector(0x61445350)); // 'aDSP'
        QCOMPARE(g_captured.address.mScope, AudioObjectPropertyScope(kAudioObjectPropertyScopeGlobal));
        QCOMPARE(g_captured.address.mElement, AudioObjectPropertyElement(kAudioObjectPropertyElementMain));
        QCOMPARE(g_captured.qualifierSize, UInt32(sizeof(CFStringRef)));
        QVERIFY(g_captured.qualifier == bundleId);
        QCOMPARE(g_captured.dataSize, UInt32(sizeof(UInt32)));
        QCOMPARE(g_captured.data, UInt32(0));
    }

    void sendsNoQualifierWithoutABundleIdentifier()
    {
        QCOMPARE(NereusSDR::optOutOfSystemVoiceIsolationWith(&fakeSetProperty, nullptr),
                 VoiceIsolationOptOut::Applied);
        QCOMPARE(g_captured.calls, 1);
        QCOMPARE(g_captured.qualifierSize, UInt32(0));
        QVERIFY(g_captured.qualifier == nullptr);
        QCOMPARE(g_captured.data, UInt32(0));
    }

    void anUnknownPropertyMeansThisMacHasNoSwitch()
    {
        g_captured.answer = kAudioHardwareUnknownPropertyError;
        QCOMPARE(NereusSDR::optOutOfSystemVoiceIsolationWith(&fakeSetProperty, nullptr),
                 VoiceIsolationOptOut::NotApplicable);
    }

    // An older macOS may say the operation is unsupported rather than the
    // property unknown; either way there is no switch, and no warning.
    void anUnsupportedOperationAlsoMeansNoSwitch()
    {
        g_captured.answer = kAudioHardwareUnsupportedOperationError;
        QCOMPARE(NereusSDR::optOutOfSystemVoiceIsolationWith(&fakeSetProperty, nullptr),
                 VoiceIsolationOptOut::NotApplicable);
    }

    // Every test executable runs in test mode, and several run the real
    // nereusd and helper entry points.
    void aTestRunMakesNoCoreAudioCall()
    {
        QCOMPARE(NereusSDR::optOutOfSystemVoiceIsolation(), VoiceIsolationOptOut::NotApplicable);
    }

    void anyOtherErrorIsARefusal()
    {
        g_captured.answer = kAudioHardwareBadPropertySizeError;
        QCOMPARE(NereusSDR::optOutOfSystemVoiceIsolationWith(&fakeSetProperty, nullptr),
                 VoiceIsolationOptOut::Refused);
    }

    // The log needs the status Core Audio answered with, refusal or not.
    void reportsTheStatusCoreAudioAnswered()
    {
        std::int32_t status = 1;
        g_captured.answer = kAudioHardwareBadPropertySizeError;
        QCOMPARE(NereusSDR::optOutOfSystemVoiceIsolationWith(&fakeSetProperty, nullptr, &status),
                 VoiceIsolationOptOut::Refused);
        QCOMPARE(status, std::int32_t(kAudioHardwareBadPropertySizeError));

        g_captured.answer = kAudioHardwareNoError;
        QCOMPARE(NereusSDR::optOutOfSystemVoiceIsolationWith(&fakeSetProperty, nullptr, &status),
                 VoiceIsolationOptOut::Applied);
        QCOMPARE(status, std::int32_t(kAudioHardwareNoError));
    }

    // nereusd and the mic helper never start one, and their device walks
    // must not wait on it.
    void waitReturnsAtOnceWhenNoOptOutWasStarted()
    {
        QCOMPARE(NereusSDR::waitForSystemVoiceIsolationOptOut(), std::nullopt);
    }

    // The window's walk waits for the started opt-out to return, and gets
    // what it did.
    void waitHoldsUntilTheStartedOptOutReturns()
    {
        // Shared with the opt-out's thread, which is detached.
        const auto release = std::make_shared<QSemaphore>();
        const auto called = std::make_shared<std::atomic<bool>>(false);
        NereusSDR::startSystemVoiceIsolationOptOutWith([release, called](std::int32_t* status) {
            called->store(true);
            release->acquire();
            *status = std::int32_t(kAudioHardwareBadPropertySizeError);
            return VoiceIsolationOptOut::Refused;
        });

        QSemaphore waited;
        std::optional<VoiceIsolationOptOutReport> report;
        std::thread waiter([&] {
            report = NereusSDR::waitForSystemVoiceIsolationOptOut();
            waited.release();
        });
        // Leave nothing blocked behind a failed check.
        const auto unblock = qScopeGuard([&] {
            release->release(1000);
            waiter.join();
        });

        QVERIFY(!waited.tryAcquire(1, kHeldMs));
        QVERIFY(called->load());
        release->release();
        QVERIFY(waited.tryAcquire(1, kReleasedMs));
        QVERIFY(report.has_value());
        QCOMPARE(report->outcome, VoiceIsolationOptOut::Refused);
        QCOMPARE(report->status, std::int32_t(kAudioHardwareBadPropertySizeError));
        QVERIFY(report->callMs >= kHeldMs);

        // A later walk gets the same answer without waiting.
        const std::optional<VoiceIsolationOptOutReport> again =
            NereusSDR::waitForSystemVoiceIsolationOptOut();
        QVERIFY(again.has_value());
        QCOMPARE(again->outcome, VoiceIsolationOptOut::Refused);
    }

    // main() starts it once; a second start sends nothing more.
    void startIsIdempotent()
    {
        // Shared with the opt-out's thread, which is detached.
        const auto entered = std::make_shared<QSemaphore>();
        const auto release = std::make_shared<QSemaphore>();
        const auto optOut = [entered, release](std::int32_t* status) {
            entered->release();
            release->acquire();
            *status = 0;
            return VoiceIsolationOptOut::Applied;
        };
        NereusSDR::startSystemVoiceIsolationOptOutWith(optOut);
        NereusSDR::startSystemVoiceIsolationOptOutWith(optOut);
        const auto unblock = qScopeGuard([&] { release->release(1000); });

        QVERIFY(entered->tryAcquire(1, kReleasedMs));
        // A second run, had one started, would be in its call by now.
        QVERIFY(!entered->tryAcquire(1, kHeldMs));
        release->release();
        const std::optional<VoiceIsolationOptOutReport> report =
            NereusSDR::waitForSystemVoiceIsolationOptOut();
        QVERIFY(report.has_value());
        QCOMPARE(report->outcome, VoiceIsolationOptOut::Applied);
    }

    // An opt-out that never returns must not keep the device list empty for
    // the whole session: the walk stops waiting after the limit, says so,
    // and later walks do not wait again. The answer, when it comes, is
    // still there to read.
    void theWaitGivesUpAfterItsLimit()
    {
        const auto release = std::make_shared<QSemaphore>();
        NereusSDR::startSystemVoiceIsolationOptOutWith([release](std::int32_t* status) {
            release->acquire();
            *status = 0;
            return VoiceIsolationOptOut::Applied;
        });

        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral(
                                 "voice isolation opt-out has not returned after 100 ms")));
        QSemaphore waited;
        std::optional<VoiceIsolationOptOutReport> first;
        std::thread waiter([&] {
            first = NereusSDR::waitForSystemVoiceIsolationOptOut(std::chrono::milliseconds(100));
            waited.release();
        });
        // Leave nothing blocked behind a failed check.
        const auto unblock = qScopeGuard([&] {
            release->release(1000);
            waiter.join();
        });

        QVERIFY(waited.tryAcquire(1, kReleasedMs));
        QVERIFY(!first.has_value());

        QElapsedTimer again;
        again.start();
        QVERIFY(!NereusSDR::waitForSystemVoiceIsolationOptOut().has_value());
        QVERIFY2(again.elapsed() < kReleasedMs, qPrintable(QString::number(again.elapsed())));

        release->release();
        std::optional<VoiceIsolationOptOutReport> late;
        QElapsedTimer answered;
        answered.start();
        while (!late.has_value() && answered.elapsed() < kReleasedMs) {
            late = NereusSDR::waitForSystemVoiceIsolationOptOut();
            if (!late.has_value()) {
                QThread::msleep(1);
            }
        }
        QVERIFY(late.has_value());
        QCOMPARE(late->outcome, VoiceIsolationOptOut::Applied);
    }
};

QTEST_APPLESS_MAIN(TstMacVoiceIsolation)
#include "tst_mac_voice_isolation.moc"
