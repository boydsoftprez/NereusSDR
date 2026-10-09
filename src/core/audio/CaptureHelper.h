// =================================================================
// src/core/audio/CaptureHelper.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Control loop of the
// nereus-audio-capture helper process, which owns the native microphone
// so a stuck open can never stall NereusSDR itself; no Thetis logic.
//
// Design: docs/architecture/2026-09-22-optional-microphone-capture-design.md
// (Process and PCM contract).  Requirement R-R3-36.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan early-review fix wave (R-AUD-02, bug 1):
//               captureHostApiIndex(), the mic opens on its saved host API.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 13 (R-AUD-17): the mic goes through
//               the shared ring, no PCM records.  J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QByteArray>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace NereusSDR {

// Reads protocol records on stdin, writes protocol records on stdout and
// logs on stderr.  Sends Hello first.  A stdin reader thread parses the
// parent's records; stdin EOF, a read error or a protocol error ends the
// process at once with std::_Exit(0), even while the main thread is inside
// a native audio call.  The main thread runs Configure / AttachRing / Open /
// Stop / Shutdown in order.  R-AUD-17: the microphone opens on the saved
// engine, and its input callback writes the clock matcher in the window's
// shared ring (AttachRing) and posts the ring's wake; every 10 ms the main
// thread sends probe hits and watches for a lost input.  Returns 0 after
// Shutdown.
int runCaptureHelper(int argc, char** argv);

// R-R3-21: the input device names a test run's helper answers from. A test
// run (PortAudioBus::portAudioBarredForTestRun) never initialises
// PortAudio, so the helper looks a named device up here instead: a name
// not on the list fails as a missing device, a listed one fails without
// opening anything. Call before runCaptureHelper; ignored outside a test run.
void setCaptureHelperTestDevices(const QStringList& names);

// R-AUD-02 (bug 1): the PortAudio host API index the mic opens on, from
// the PortAudio host APIs (index, name) listed now.  A mic saved with its
// host API name (driverApi) opens on that host API, never on a device of
// the same name under another one; with no driverApi, or one not listed,
// the saved index stands, as PortAudioBackend::createOutput does.
int captureHostApiIndex(const QString& driverApi, int savedHostApiIndex,
                        const QVector<QPair<int, QString>>& hostApis);

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
