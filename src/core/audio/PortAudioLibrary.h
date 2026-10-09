// =================================================================
// src/core/audio/PortAudioLibrary.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  One lock and one reference count
// for the PortAudio library in this process (R-AUD-06).
//
// Every PortAudio library call made outside an audio callback holds
// mutex(): Pa_Initialize and Pa_Terminate, the device and host API
// queries, and opening, starting, stopping and closing a stream.  It is
// never taken inside a callback.  Pa_StopStream waits for the callback
// to return while the lock is held; the callback takes no lock, so that
// wait always ends.  The lock is recursive: an open holds it and calls
// the query helpers, which take it again.
//
// acquire() and release() wrap Pa_Initialize and Pa_Terminate and count
// this process's references.  reinitialize() is Rescan: with no stream
// open it terminates PortAudio as many times as it is referenced and
// initialises it as many times again, all under the lock, so PortAudio
// lists the devices present now and no other thread ever sees it half
// way.  With a stream open it does nothing and says so (Pa_Terminate
// would close that stream under its owner).
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 7 fix (R-AUD-06). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include <mutex>

namespace NereusSDR::PortAudioLibrary {

std::recursive_mutex& mutex();

// Pa_Initialize under the lock; true when it succeeded (one reference).
bool acquire();
// Pa_Terminate under the lock for one reference; nothing with none.
void release();
// This process's references now.
int references();

// Rescan: true when PortAudio was started again, false when a stream is
// open or there is no reference (the next acquire() lists afresh).
bool reinitialize();

// A PortAudioBus stream opened or closed (under the lock).
void streamOpened();
void streamClosed();
int openStreams();

} // namespace NereusSDR::PortAudioLibrary
