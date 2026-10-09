// =================================================================
// src/core/audio/PortAudioLibrary.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See PortAudioLibrary.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 7 fix (R-AUD-06). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/PortAudioLibrary.h"

#include "core/LogCategories.h"

#include <portaudio.h>

namespace NereusSDR::PortAudioLibrary {

namespace {

// Both guarded by mutex().
int g_references = 0;
int g_openStreams = 0;

} // namespace

std::recursive_mutex& mutex()
{
    static std::recursive_mutex lock;
    return lock;
}

bool acquire()
{
    std::lock_guard<std::recursive_mutex> lock(mutex());
    const PaError err = Pa_Initialize();
    if (err != paNoError) {
        qCWarning(lcAudio) << "Pa_Initialize failed:" << Pa_GetErrorText(err);
        return false;
    }
    ++g_references;
    return true;
}

void release()
{
    std::lock_guard<std::recursive_mutex> lock(mutex());
    if (g_references <= 0) {
        return;
    }
    --g_references;
    Pa_Terminate();
}

int references()
{
    std::lock_guard<std::recursive_mutex> lock(mutex());
    return g_references;
}

bool reinitialize()
{
    std::lock_guard<std::recursive_mutex> lock(mutex());
    if (g_openStreams > 0) {
        qCWarning(lcAudio) << "Older drivers: PortAudio not listed again;" << g_openStreams
                           << "of its streams are still open, so the list is unchanged";
        return false;
    }
    if (g_references <= 0) {
        return false;   // not running: the next start lists the devices present then
    }
    const int references = g_references;
    for (int i = 0; i < references; ++i) {
        Pa_Terminate();
    }
    g_references = 0;
    for (int i = 0; i < references; ++i) {
        const PaError err = Pa_Initialize();
        if (err != paNoError) {
            qCWarning(lcAudio) << "Older drivers: PortAudio did not start again:"
                               << Pa_GetErrorText(err);
            break;
        }
        ++g_references;
    }
    if (g_references != references) {
        return false;
    }
    qCInfo(lcAudio) << "Older drivers: PortAudio started again to list the devices present now";
    return true;
}

void streamOpened()
{
    std::lock_guard<std::recursive_mutex> lock(mutex());
    ++g_openStreams;
}

void streamClosed()
{
    std::lock_guard<std::recursive_mutex> lock(mutex());
    if (g_openStreams > 0) {
        --g_openStreams;
    }
}

int openStreams()
{
    std::lock_guard<std::recursive_mutex> lock(mutex());
    return g_openStreams;
}

} // namespace NereusSDR::PortAudioLibrary
