// =================================================================
// src/core/audio/RealtimeAudioPriority.cpp  (NereusSDR-native)
// =================================================================
// 2026-05-25  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
// See RealtimeAudioPriority.h for design rationale.
// =================================================================
#include "RealtimeAudioPriority.h"

#include <QtGlobal>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcRtAudio, "nereussdr.rt_audio")

namespace NereusSDR {

// Token struct holds whatever platform-specific state is needed to
// undo the elevation in leave().  Per-platform members are gated so
// the struct stays minimal on platforms that do not need them.
struct AudioPriorityToken {
#ifdef Q_OS_MAC
    // os_workgroup_join_token_s is a struct, not a pointer; store it
    // inline.  Member is opaque from this TU's perspective; the macOS
    // implementation below defines the actual layout via the system
    // header inclusion.
    // Note: forward-declared as a pointer wrapper to keep the system
    // header out of this header file.
    void* macWorkgroupJoinToken{nullptr};
    void* macWorkgroupHandle{nullptr};  // retained os_workgroup_t
    bool  macQosElevated{false};
#endif
#ifdef Q_OS_LINUX
    int  linuxPrevPolicy{0};
    int  linuxPrevPriority{0};
    bool linuxSchedFifoSet{false};
#endif
#ifdef Q_OS_WIN
    void* winAvrtHandle{nullptr};   // HANDLE from AvSetMmThreadCharacteristics
#endif
};

} // namespace NereusSDR

// ---------------------------------------------------------------- macOS

#ifdef Q_OS_MAC

#include <CoreAudio/CoreAudio.h>
#include <os/workgroup.h>
#include <pthread/qos.h>
#include <pthread.h>

namespace NereusSDR {

namespace {

// Query the default output audio device's os_workgroup_t.  Returns
// nullptr on failure (caller continues without workgroup membership).
os_workgroup_t fetchDefaultOutputWorkgroup()
{
    AudioObjectPropertyAddress defaultOutAddr = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain,
    };
    AudioDeviceID deviceId = kAudioObjectUnknown;
    UInt32 deviceSize = sizeof(deviceId);
    OSStatus st = AudioObjectGetPropertyData(
        kAudioObjectSystemObject, &defaultOutAddr,
        0, nullptr, &deviceSize, &deviceId);
    if (st != noErr || deviceId == kAudioObjectUnknown) {
        qCInfo(lcRtAudio) << "Could not resolve default output device for"
                          << "workgroup query (OSStatus" << st << "). Continuing"
                          << "without workgroup membership.";
        return nullptr;
    }

    AudioObjectPropertyAddress wgAddr = {
        kAudioDevicePropertyIOThreadOSWorkgroup,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain,
    };
    os_workgroup_t wg = nullptr;
    // sizeof(os_workgroup_t): the CoreAudio property returns a pointer
    // value; size of the destination storage is the pointer size.  The
    // explicit type form keeps clang-tidy bugprone-sizeof-expression
    // quiet (it warns on sizeof(variable) when the variable is a pointer).
    UInt32 wgSize = sizeof(os_workgroup_t);
    st = AudioObjectGetPropertyData(
        deviceId, &wgAddr, 0, nullptr, &wgSize, &wg);
    if (st != noErr || wg == nullptr) {
        qCInfo(lcRtAudio) << "Default output device has no IO workgroup"
                          << "(OSStatus" << st << "). pthread QoS still"
                          << "applies; workgroup deadline coordination skipped.";
        return nullptr;
    }
    return wg;
}

} // namespace

AudioPriorityToken* elevateAudioThreadPriority()
{
    auto* token = new AudioPriorityToken();

    // Step 1: elevate the pthread QoS class.  USER_INTERACTIVE is the
    // highest user-facing class on Apple Silicon and is the one Apple
    // documents for real-time audio threads.
    if (pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0) == 0) {
        token->macQosElevated = true;
    } else {
        qCWarning(lcRtAudio) << "pthread_set_qos_class_self_np(USER_INTERACTIVE)"
                             << "failed; thread continues at default QoS.";
    }

    // Step 2: join the default output device's os_workgroup_t.  Soft-
    // fails when the device has no IO workgroup (e.g. some virtual
    // devices) - QoS elevation alone still helps in that case.
    if (os_workgroup_t wg = fetchDefaultOutputWorkgroup()) {
        // Allocate the join token on the heap; os_workgroup_join_token_s
        // is opaque-by-design but the SDK header exposes its size via the
        // os_workgroup_join_token_s typedef.  Box it for the lifetime of
        // the AudioPriorityToken.
        auto* joinTok = new os_workgroup_join_token_s();
        int err = os_workgroup_join(wg, joinTok);
        if (err == 0) {
            token->macWorkgroupHandle    = wg;   // retained by os_workgroup_join
            token->macWorkgroupJoinToken = joinTok;
            qCInfo(lcRtAudio) << "DSP thread joined default output workgroup";
        } else {
            qCWarning(lcRtAudio) << "os_workgroup_join failed errno=" << err
                                 << "thread continues without workgroup membership.";
            delete joinTok;
            // Release the workgroup reference returned by fetchDefaultOutputWorkgroup.
            os_release(wg);
        }
    }

    return token;
}

void leaveAudioThreadPriority(AudioPriorityToken* token)
{
    if (token == nullptr) { return; }
    if (token->macWorkgroupHandle != nullptr
        && token->macWorkgroupJoinToken != nullptr) {
        auto* joinTok = static_cast<os_workgroup_join_token_s*>(
            token->macWorkgroupJoinToken);
        auto wg = static_cast<os_workgroup_t>(token->macWorkgroupHandle);
        os_workgroup_leave(wg, joinTok);
        delete joinTok;
        os_release(wg);
    }
    delete token;
}

void elevateGuiMainThreadPriority()
{
    const int err = pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    if (err == 0) {
        qCInfo(lcRtAudio) << "GUI main thread elevated to USER_INTERACTIVE QoS";
    } else {
        qCWarning(lcRtAudio) << "Failed to elevate GUI main thread (errno"
                             << err << "); continuing at default QoS.";
    }
}

void elevateComputeThreadPriority()
{
    const int err = pthread_set_qos_class_self_np(QOS_CLASS_USER_INITIATED, 0);
    if (err == 0) {
        qCInfo(lcRtAudio) << "Compute thread elevated to USER_INITIATED QoS";
    } else {
        qCWarning(lcRtAudio) << "Failed to elevate compute thread (errno"
                             << err << "); continuing at default QoS.";
    }
}

} // namespace NereusSDR

#endif  // Q_OS_MAC

// ---------------------------------------------------------------- Linux

#ifdef Q_OS_LINUX

#include <pthread.h>
#include <sched.h>
#include <unistd.h>   // nice()
#include <cerrno>
#include <cstring>

namespace NereusSDR {

AudioPriorityToken* elevateAudioThreadPriority()
{
    auto* token = new AudioPriorityToken();

    sched_param prev{};
    int prevPolicy = SCHED_OTHER;
    pthread_getschedparam(pthread_self(), &prevPolicy, &prev);
    token->linuxPrevPolicy   = prevPolicy;
    token->linuxPrevPriority = prev.sched_priority;

    // SCHED_FIFO at priority_max - 1.  Requires CAP_SYS_NICE or a non-
    // zero rtprio rlimit; on a vanilla user install this often returns
    // EPERM.  Soft-fail: log and continue at default policy.
    const int rtMin = sched_get_priority_min(SCHED_FIFO);
    const int rtMax = sched_get_priority_max(SCHED_FIFO);
    const int target = (rtMax > rtMin) ? (rtMax - 1) : rtMin;
    sched_param wanted{};
    wanted.sched_priority = target;
    const int err = pthread_setschedparam(pthread_self(),
                                          SCHED_FIFO, &wanted);
    if (err == 0) {
        token->linuxSchedFifoSet = true;
        qCInfo(lcRtAudio) << "DSP thread set to SCHED_FIFO priority" << target;
    } else {
        qCInfo(lcRtAudio) << "pthread_setschedparam(SCHED_FIFO) failed"
                          << "(errno" << err << strerror(err) << ")."
                          << "Operator may need CAP_SYS_NICE or rtprio rlimit.";
    }
    return token;
}

void leaveAudioThreadPriority(AudioPriorityToken* token)
{
    if (token == nullptr) { return; }
    if (token->linuxSchedFifoSet) {
        sched_param prev{};
        prev.sched_priority = token->linuxPrevPriority;
        pthread_setschedparam(pthread_self(),
                              token->linuxPrevPolicy, &prev);
    }
    delete token;
}

void elevateGuiMainThreadPriority()
{
    // Linux without rtprio caps: nice the calling thread.  Less
    // effective than SCHED_FIFO but available without privilege.
    // Caller can set rtprio rlimit / CAP_SYS_NICE to get a stronger
    // effect.
    if (nice(-5) == -1 && errno != 0) {
        qCInfo(lcRtAudio) << "nice(-5) for GUI main thread failed (errno"
                          << errno << strerror(errno) << "); continuing"
                          << "at default.";
    }
}

void elevateComputeThreadPriority()
{
    if (nice(-3) == -1 && errno != 0) {
        qCInfo(lcRtAudio) << "nice(-3) for compute thread failed (errno"
                          << errno << strerror(errno) << "); continuing"
                          << "at default.";
    }
}

} // namespace NereusSDR

#endif  // Q_OS_LINUX

// ---------------------------------------------------------------- Windows

#ifdef Q_OS_WIN

#include <windows.h>
#include <avrt.h>

namespace NereusSDR {

AudioPriorityToken* elevateAudioThreadPriority()
{
    auto* token = new AudioPriorityToken();
    DWORD taskIndex = 0;
    HANDLE h = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
    if (h != nullptr) {
        token->winAvrtHandle = h;
        qCInfo(lcRtAudio) << "DSP thread joined 'Pro Audio' MMCSS task class.";
    } else {
        qCWarning(lcRtAudio) << "AvSetMmThreadCharacteristicsW('Pro Audio') failed"
                             << "(GetLastError=" << GetLastError() << ").";
    }
    return token;
}

void leaveAudioThreadPriority(AudioPriorityToken* token)
{
    if (token == nullptr) { return; }
    if (token->winAvrtHandle != nullptr) {
        AvRevertMmThreadCharacteristics(
            static_cast<HANDLE>(token->winAvrtHandle));
    }
    delete token;
}

void elevateGuiMainThreadPriority()
{
    if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST)) {
        qCWarning(lcRtAudio) << "SetThreadPriority(HIGHEST) for GUI failed"
                             << "(GetLastError=" << GetLastError() << ").";
    }
}

void elevateComputeThreadPriority()
{
    if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL)) {
        qCWarning(lcRtAudio) << "SetThreadPriority(ABOVE_NORMAL) for compute failed"
                             << "(GetLastError=" << GetLastError() << ").";
    }
}

} // namespace NereusSDR

#endif  // Q_OS_WIN

// ---------------------------------------------------------------- other

#if !defined(Q_OS_MAC) && !defined(Q_OS_LINUX) && !defined(Q_OS_WIN)

namespace NereusSDR {

AudioPriorityToken* elevateAudioThreadPriority() { return nullptr; }
void leaveAudioThreadPriority(AudioPriorityToken*) {}
void elevateGuiMainThreadPriority() {}
void elevateComputeThreadPriority() {}

} // namespace NereusSDR

#endif
