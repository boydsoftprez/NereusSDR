// =================================================================
// src/core/audio/RealtimeAudioPriority.h  (NereusSDR-native)
// =================================================================
// 2026-05-25  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
//
// Elevate the calling thread's scheduling priority for audio DSP work
// so the OS scheduler does not preempt the DSP feeder during heavy
// system load (parallel compiles, Spotlight indexing, Time Machine
// snapshots, etc).
//
// Bench symptom 2026-05-25 KG4VCF: severe audio jitter on a remote
// receiver while a `cmake --build` is in flight on the same Mac.
// PortAudio's own callback thread is already real-time on macOS
// (PortAudio handles that internally via pthread_setschedparam +
// SCHED_FIFO).  The starvation point is the DSP feeder thread
// (RxDspWorker on m_dspThread) which runs at default Qt::QThread
// priority i.e. POSIX SCHED_OTHER on macOS.  Heavy compile jobs at
// the same QoS preempt it; ring buffer drains; underrun; click.
//
// Three platform paths:
//
// macOS: pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE) +
//        join the audio output device's os_workgroup_t so the
//        scheduler treats the DSP thread as a peer of the audio I/O
//        thread.  Apple Silicon: the workgroup also pins membership
//        to a performance core.
//
// Linux: pthread_setschedparam(SCHED_FIFO, sched_get_priority_max - 1).
//        Requires CAP_SYS_NICE or rtprio rlimit; soft-fails if absent
//        (returns nullptr, caller continues at default priority).
//
// Windows: AvSetMmThreadCharacteristicsW(L"Pro Audio", ...).  Linked
//          against avrt.lib.
//
// API: opaque token returned by elevate(); must be passed back to
// leave() before the thread exits to release any OS resources.
// Pass nullptr to leave() is a no-op (safe to call unconditionally).
// =================================================================
#pragma once

namespace NereusSDR {

// Opaque per-thread token.  Constructed on the elevating thread; the
// same token must be passed to leave() on the same thread before it
// exits.  Forward-declared here to keep platform headers out of the
// public API.
struct AudioPriorityToken;

// Elevate the calling thread's scheduling priority for audio DSP work.
// Idempotent: calling repeatedly from the same thread leaks the prior
// token; callers should pair every elevate() with exactly one leave().
//
// Returns: non-null token on supported platforms when elevation
// succeeded, nullptr on unsupported platforms or when the OS refused
// the elevation (e.g. Linux without CAP_SYS_NICE).  Either way, the
// thread continues to function; only scheduling priority differs.
AudioPriorityToken* elevateAudioThreadPriority();

// Release any OS resources held by the token (workgroup membership
// on macOS, characteristic handle on Windows).  Safe to call with
// nullptr.  Must be called on the same thread that called elevate().
void leaveAudioThreadPriority(AudioPriorityToken* token);

// Elevate the calling thread to USER_INTERACTIVE QoS for GUI work.
// Intended for the main GUI / UI thread so heavy user-initiated
// background work (parallel compiles, mdworker indexing, Time Machine
// snapshots, etc.) does not preempt the UI refresh cycle.  No workgroup
// join, no cleanup token: QoS is a per-thread attribute that resets
// when the thread exits.  Soft-fails to default priority if the OS
// refuses (e.g. Linux without rtprio rlimit).
//
// Bench symptom 2026-05-25 KG4VCF: "whole program stutters when a
// build happens".  Audio-thread elevation alone was not enough; the
// main thread was still being preempted by the build's compile jobs
// at the same QoS class, producing visibly choppy spectrum / waterfall
// rendering.
void elevateGuiMainThreadPriority();

// Elevate the calling thread to USER_INITIATED QoS for spectrum / FFT
// compute work.  Less aggressive than audio (USER_INTERACTIVE) or GUI
// (USER_INTERACTIVE); avoids fighting the GUI thread for the highest
// scheduling class while still beating typical compile jobs which run
// at DEFAULT.
void elevateComputeThreadPriority();

// Elevate the calling thread to USER_INTERACTIVE QoS for latency-
// critical work that needs audio/GUI-tier scheduling but doesn't own
// an audio device (so no workgroup to join).
//
// Bench symptom 2026-05-26 KG4VCF: USER_INITIATED on ConnectionThread
// + FFTEngine thread + WaterfallTickerThread still let those threads
// be preempted by parallel compile workers under heavy build load,
// producing waterfall stutter and occasional audio underruns (UDP
// packet drops on the I/Q recv socket).  Promoting them to
// USER_INTERACTIVE puts them in the same scheduling class as the GUI
// and DSP threads -- compile workers (DEFAULT QoS) consistently lose
// the time slice race.
void elevateLatencyCriticalThreadPriority();

} // namespace NereusSDR
