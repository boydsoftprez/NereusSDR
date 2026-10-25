// =================================================================
// tests/fakes/NoCaptureHelper.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test fixture; no Thetis logic.
//
// A test's AudioEngine builds its capture supervisor with the default
// program, which locateCaptureHelper() finds beside the test binary.  A
// runtime folder can hold the product's nereus-audio-capture there (a
// combined build), and the first microphone demand (a connected local
// model with the PC mic, Test Mic, the delay probe, a remote window's mic)
// would then start the real helper and open a real microphone.
//
// useNoCaptureHelper() points the engine's supervisor at a program that
// cannot exist, so such a demand fails with HelperMissing and nothing is
// started.  Call it before any capture demand (the engine asserts that);
// a test that needs a helper installs its own options afterwards, such as
// the scripted FakeCaptureChild.
// =================================================================

#pragma once

#include "core/AudioEngine.h"
#include "core/audio/CaptureSupervisor.h"
#include "models/RadioModel.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QStringLiteral>

namespace NereusSDR::Test {

// The program sits under this test binary's own path, as if the binary
// were a folder.  The binary is a regular file, so nothing can ever exist
// there, unlike a fixed name in the shared temp folder, which any process
// could create and the supervisor would then run.
inline CaptureSupervisor::Options missingCaptureHelperOptions()
{
    CaptureSupervisor::Options options;
    options.program = QCoreApplication::applicationFilePath()
        + QStringLiteral("/no-capture-helper");
    if (QFileInfo::exists(options.program)) {
        qFatal("NoCaptureHelper: %s exists", qPrintable(options.program));
    }
    return options;
}

inline void useNoCaptureHelper(AudioEngine& engine)
{
    engine.setCaptureSupervisorOptionsForTest(missingCaptureHelperOptions());
}

inline void useNoCaptureHelper(RadioModel& model)
{
    if (AudioEngine* const engine = model.audioEngine()) {
        useNoCaptureHelper(*engine);
    }
}

} // namespace NereusSDR::Test
