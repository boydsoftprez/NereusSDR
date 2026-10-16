// =================================================================
// src/core/MacVoiceIsolation.cpp  (NereusSDR-native)
// =================================================================
// no-port-check: NereusSDR-original.
// See MacVoiceIsolation.h for what this is for.
//
// Modification history (NereusSDR):
//   2026-10-10  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//               The opt-out; nothing in a test run.
//   2026-10-15  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//               The window's run of it on a thread of its own, only when
//               the app's Mic Mode is Voice Isolation, the wait for it up
//               to a limit, and its log lines.
// =================================================================
#include "core/MacVoiceIsolation.h"

#include "core/LogCategories.h"
#include "core/MacMicMode.h"
#include "core/audio/AudioTestBarrier.h"

#include <QElapsedTimer>
#include <QLoggingCategory>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

namespace NereusSDR {

namespace {

// 'aDSP' on the audio system object: "voice isolation DSP enabled" for one
// client. The qualifier is a CFStringRef bundle identifier (omitted, the
// call applies to the calling process), the data a UInt32, zero to disable.
// Not in the public SDK headers. Read from CoreAudio.framework's
// HALSystem::SetPropertyData on macOS 27.0, which logs "Client attempting to
// %s voice isolation DSP for bundle ID %s" and forwards to
// System_Input_Processing_Notification_Handler::set_enabled.
constexpr AudioObjectPropertySelector kVoiceIsolationDspEnabledSelector = 0x61445350; // 'aDSP'

// One started opt-out, shared with its detached thread.
struct OptOutRun {
    std::mutex mutex;
    std::condition_variable finished;
    bool done = false;
    bool logged = false;
    bool gaveUp = false;  // a waiter hit its limit; nobody waits any more
    VoiceIsolationOptOutReport report;
};

std::mutex g_runMutex;
std::shared_ptr<OptOutRun> g_run;

std::shared_ptr<OptOutRun> currentRun()
{
    std::lock_guard<std::mutex> lock(g_runMutex);
    return g_run;
}

QString micModeName(const std::optional<int>& micMode)
{
    if (!micMode) {
        return QStringLiteral("none");
    }
    switch (*micMode) {
    case kMicModeStandard:
        return QStringLiteral("Standard");
    case kMicModeWideSpectrum:
        return QStringLiteral("Wide Spectrum");
    case kMicModeVoiceIsolation:
        return QStringLiteral("Voice Isolation");
    default:
        return QStringLiteral("mode %1").arg(*micMode);
    }
}

// Logged by the first waiter rather than the opt-out's own thread, which
// can start before logging is set up; by the thread only when it answers
// after a waiter gave up on it (no waited time then).
void logReport(const VoiceIsolationOptOutReport& report, std::optional<qint64> waitedMs)
{
    const QString mode = micModeName(report.micMode);
    switch (report.outcome) {
    case VoiceIsolationOptOut::Applied:
        if (waitedMs) {
            qCInfo(lcAudio).nospace().noquote()
                << "Mic Mode is " << mode << "; voice isolation opt-out applied in "
                << report.callMs << " ms; the device list waited " << *waitedMs << " ms for it";
        } else {
            qCInfo(lcAudio).nospace().noquote()
                << "Mic Mode is " << mode << "; voice isolation opt-out applied in "
                << report.callMs << " ms, after the device list stopped waiting for it";
        }
        break;
    case VoiceIsolationOptOut::NotNeeded:
        qCInfo(lcAudio).nospace().noquote()
            << "Mic Mode is " << mode << ", so no voice isolation opt-out (read in "
            << report.callMs << " ms)";
        break;
    case VoiceIsolationOptOut::NotApplicable:
        if (report.micMode) {
            qCInfo(lcAudio).nospace().noquote()
                << "Mic Mode is " << mode << ", but this macOS has no voice isolation"
                << " opt-out (status " << report.status << ", after " << report.callMs << " ms)";
        } else {
            qCInfo(lcAudio) << "No Mic Mode here, so no voice isolation opt-out";
        }
        break;
    case VoiceIsolationOptOut::Refused:
        qCWarning(lcAudio).nospace().noquote()
            << "Mic Mode is " << mode << "; macOS refused the voice isolation opt-out (status "
            << report.status << ", after " << report.callMs
            << " ms); audio start may be slow until Mic Mode is set to Standard";
        break;
    }
}

} // namespace

VoiceIsolationOptOut optOutOfSystemVoiceIsolationWith(AudioObjectSetPropertyFn setProperty,
                                                      CFStringRef bundleId,
                                                      std::int32_t* status)
{
    const AudioObjectPropertyAddress address = {
        kVoiceIsolationDspEnabledSelector,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    const UInt32 disabled = 0;
    const OSStatus answer = bundleId
        ? setProperty(kAudioObjectSystemObject, &address,
                      sizeof(bundleId), &bundleId, sizeof(disabled), &disabled)
        : setProperty(kAudioObjectSystemObject, &address,
                      0, nullptr, sizeof(disabled), &disabled);
    if (status) {
        *status = answer;
    }
    if (answer == kAudioHardwareNoError) {
        return VoiceIsolationOptOut::Applied;
    }
    if (answer == kAudioHardwareUnknownPropertyError
        || answer == kAudioHardwareUnsupportedOperationError) {
        return VoiceIsolationOptOut::NotApplicable;
    }
    return VoiceIsolationOptOut::Refused;
}

VoiceIsolationOptOut optOutOfSystemVoiceIsolation(std::int32_t* status)
{
    // A test run makes no audio call at all, and this one reconfigures
    // every audio device on the machine.
    if (audioDevicesBarredForTestRun()) {
        return VoiceIsolationOptOut::NotApplicable;
    }
    CFBundleRef bundle = CFBundleGetMainBundle();
    CFStringRef bundleId = bundle ? CFBundleGetIdentifier(bundle) : nullptr;
    return optOutOfSystemVoiceIsolationWith(&AudioObjectSetPropertyData, bundleId, status);
}

void startSystemVoiceIsolationOptOut()
{
    startSystemVoiceIsolationOptOutWith(
        [] {
            // A test run reads nothing either.
            return audioDevicesBarredForTestRun() ? std::optional<int>()
                                                  : preferredMicrophoneMode();
        },
        &optOutOfSystemVoiceIsolation);
}

void startSystemVoiceIsolationOptOutWith(std::function<std::optional<int>()> readMicMode,
                                         std::function<VoiceIsolationOptOut(std::int32_t*)> optOut)
{
    std::shared_ptr<OptOutRun> run;
    {
        std::lock_guard<std::mutex> lock(g_runMutex);
        if (g_run) {
            return;
        }
        run = std::make_shared<OptOutRun>();
        g_run = run;
    }
    // Detached: the call can take seconds, and nothing at exit waits for it.
    std::thread([run, readMicMode = std::move(readMicMode), optOut = std::move(optOut)] {
        QElapsedTimer clock;
        clock.start();
        std::int32_t status = 0;
        // Only a Voice Isolation window starts the loop, and the call can
        // take seconds: no call, and no wait beyond this read, otherwise.
        const std::optional<int> micMode = readMicMode();
        VoiceIsolationOptOut outcome = VoiceIsolationOptOut::NotApplicable;
        if (micMode == kMicModeVoiceIsolation) {
            outcome = optOut(&status);
        } else if (micMode) {
            outcome = VoiceIsolationOptOut::NotNeeded;
        }
        {
            std::lock_guard<std::mutex> lock(run->mutex);
            run->report = VoiceIsolationOptOutReport{ outcome, status, clock.elapsed(), micMode };
            // Nobody is left waiting to log a late answer, so say it here,
            // before `done` lets a test reset and finish.
            if (run->gaveUp && !run->logged) {
                run->logged = true;
                logReport(run->report, std::nullopt);
            }
            run->done = true;
        }
        run->finished.notify_all();
    }).detach();
}

std::optional<VoiceIsolationOptOutReport> waitForSystemVoiceIsolationOptOut(
    std::chrono::milliseconds limit)
{
    const std::shared_ptr<OptOutRun> run = currentRun();
    if (!run) {
        return std::nullopt;
    }
    QElapsedTimer waited;
    waited.start();
    std::unique_lock<std::mutex> lock(run->mutex);
    // Done, or another waiter already gave up on it.
    const bool answered = run->finished.wait_for(lock, limit, [&run] {
        return run->done || run->gaveUp;
    });
    if (!run->done) {
        // A call stuck in Core Audio must not leave the device list empty
        // for the whole session: list without it, and say why only once.
        const bool first = !answered;
        run->gaveUp = true;
        lock.unlock();
        // Let go of any waiter already blocked, which would otherwise wait
        // out its own limit.
        run->finished.notify_all();
        if (first) {
            qCWarning(lcAudio).nospace()
                << "The voice isolation opt-out has not returned after " << limit.count()
                << " ms; listing audio devices without waiting for it";
        }
        return std::nullopt;
    }
    const VoiceIsolationOptOutReport report = run->report;
    const bool first = !run->logged;
    run->logged = true;
    lock.unlock();
    // After a give-up the opt-out's thread has logged it already.
    if (first) {
        logReport(report, waited.elapsed());
    }
    return report;
}

void resetSystemVoiceIsolationOptOutForTest()
{
    std::shared_ptr<OptOutRun> run;
    {
        std::lock_guard<std::mutex> lock(g_runMutex);
        run = std::exchange(g_run, nullptr);
    }
    if (run) {
        // Its thread is done with everything the test lent it.
        std::unique_lock<std::mutex> lock(run->mutex);
        run->finished.wait(lock, [&run] { return run->done; });
    }
}

} // namespace NereusSDR
