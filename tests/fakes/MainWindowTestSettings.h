// tests/fakes/MainWindowTestSettings.h
//
// Settings every test that builds a MainWindow seeds first, so the
// window's first-run prompts do not open during a test.
//
// Why this exists (R-R3-21): on Linux, MainWindow's constructor posts
// showAudioDiagnoseDialog() when the local AudioEngine finds no audio
// backend and Audio/LinuxFirstRunSeen is unset. That dialog runs exec(),
// so the first event-loop turn a test makes (QTRY_*, qWait,
// processEvents) blocks in a modal loop nobody dismisses: three
// MainWindow tests hung for the whole 120 s ctest TIMEOUT on a Linux
// host with no sound server. The Mac never shows it (the check is
// Q_OS_LINUX only), which is how it went unseen. The app's real first
// run is unchanged; only these tests mark it as already seen.
//
// Header-only, NereusSDR-original test support; nothing here is ported.

#pragma once

#include <QString>

#include "core/AppSettings.h"

namespace NereusSDR::Test {

// Marks the Linux audio first-run dialog as already seen. Use this alone
// in a test that needs the VAX first-run check to still run (it keys off
// audio/FirstRunComplete, which this leaves untouched).
inline void suppressLinuxAudioFirstRun(AppSettings& settings = AppSettings::instance())
{
    settings.setValue(QStringLiteral("Audio/LinuxFirstRunSeen"), QStringLiteral("True"));
}

// Marks both audio first-run prompts as done: the VAX first-run dialog
// (audio/FirstRunComplete) and the Linux audio first-run dialog.
inline void markAudioFirstRunDone(AppSettings& settings = AppSettings::instance())
{
    settings.setValue(QStringLiteral("audio/FirstRunComplete"), QStringLiteral("True"));
    suppressLinuxAudioFirstRun(settings);
}

} // namespace NereusSDR::Test
