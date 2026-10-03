// =================================================================
// src/core/audio/CaptureHelper.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Control loop of the
// nereus-audio-capture helper process, which owns the native microphone
// so a stuck open can never stall NereusSDR itself; no Thetis logic.
//
// Design: docs/architecture/2026-09-22-optional-microphone-capture-design.md
// (Process and PCM contract).  Requirement R-R3-36.
// =================================================================

#pragma once

#include <QByteArray>
#include <QStringList>
#include <QtGlobal>

namespace NereusSDR {

// Reads protocol records on stdin, writes protocol records on stdout and
// logs on stderr.  Sends Hello first.  A stdin reader thread parses the
// parent's records; stdin EOF, a read error or a protocol error ends the
// process at once with std::_Exit(0), even while the main thread is inside
// a native audio call.  The main thread runs Configure / Open / Stop /
// Shutdown in order and pumps 480-frame PCM records every 10 ms while a
// microphone is open.  Returns 0 after Shutdown.
int runCaptureHelper(int argc, char** argv);

// R-R3-21: the input device names a test run's helper answers from. A test
// run (PortAudioBus::portAudioBarredForTestRun) never initialises
// PortAudio, so the helper looks a named device up here instead: a name
// not on the list fails as a missing device, a listed one fails without
// opening anything. Call before runCaptureHelper; ignored outside a test run.
void setCaptureHelperTestDevices(const QStringList& names);

// Process-level pipe plumbing shared by the helper and its scripted test
// double.  Not for use inside NereusSDR itself.
namespace CaptureHelperIo {

// Call once at process start, before any other thread exists.  Puts
// stdin/stdout in binary mode, ignores SIGPIPE, and moves the protocol
// stream to a private descriptor while pointing descriptor 1 at stderr, so
// stray native-library prints can never corrupt a record.
void prepareStdio();

// Writes one complete record to the protocol stream.  Writes from any
// thread are serialized by one mutex; each record is written whole and
// unbuffered.  Returns false when the parent is gone.
bool writeRecord(const QByteArray& record);

// Blocking read from stdin.  Returns the byte count, or 0 / -1 on EOF or
// error.
qint64 readInput(char* buffer, qint64 size);

} // namespace CaptureHelperIo

} // namespace NereusSDR
