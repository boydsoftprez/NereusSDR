// =================================================================
// tests/fakes/FakeCaptureChild.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Scripted stand-in for the
// nereus-audio-capture helper, run by a test binary re-executing itself
// with --fake-capture-child <scenario>; no Thetis logic.
//
// Protocol 3: AttachRing attaches the window's region and answers
// RingAttached; "streaming" builds a clock matcher (48 kHz in and out,
// 480-frame bursts) in the region and writes 480 stereo frames of a 1 kHz
// tone at 0.5 every 10 ms, posting the wake after each write.  No
// scenario but oversize and pcm-record sends a Pcm record.
//
// Every Ready reports "Fake microphone" at 48000 Hz, 1 channel, latency
// 1500 us and a 480-frame buffer.
//
// Scenarios (N is the generation of the Open being answered):
//   ready                 Hello; on Open: Opening, Ready, then streaming.
//                         Stop ends streaming and answers Stopped.
//   hang-open             Hello; on Open: Opening, then nothing at all
//                         (Stop and Shutdown are ignored too).  With
//                         NEREUS_FAKE_CAPTURE_HANG_DIR set, it then creates
//                         the empty file <dir>/<its pid>.  Before that
//                         Open, Shutdown still ends it.
//   no-hello              Never writes anything and ignores every command.
//   permission-then-ready Hello; on Open: Permission, 300 ms later as ready.
//   crash-after-ready     As ready; exits with code 3 after 100 ms of streaming.
//   malformed             Hello, then a record with bad magic at once.
//   oversize              Hello; on Open: Opening, Ready, then one PCM
//                         record whose header claims frameCount 4801.
//   input-lost            As ready; after 100 ms of streaming sends Failed /
//                         input-lost and stops streaming.
//   ignore-stop           As ready but never answers Stop.
//   stale                 Hello; on Open: Opening for N, then Ready tagged
//                         with generation N-1 (4294967295 when N is 1), and
//                         streaming into N's ring.
//   probe                 As ready; a ProbeEnable before its Ready exits with
//                         code 4.  While enabled, a ProbeHit every 50 ms with
//                         captureNs 1000 x (times enabled) + (hits since).
//   version-1             A protocol 1 Hello in a version 1 record header.
//   pcm-record            Hello; on Open: Opening, Ready, streaming and one
//                         valid 480-frame Pcm record.
//   busy                  Hello; on Open: Opening, then Failed /
//                         device-in-use.
//   bad-ring              Hello; on Open: Opening, Ready, then a wake with
//                         the region's ring header zeroed.
// Every scenario accepts ProbeEnable; only probe acts on it (V-HW-8).
// Every scenario answers AsioDescribe with one driver, "Fake ASIO" (4
// outputs, 2 inputs), AsioOpen with running (256 frames, 48 kHz) or, with
// no uses, closed, and accepts AsioControlPanel (Task 15).
// Every scenario exits with code 0 on stdin EOF, and on Shutdown unless
// it is ignoring commands.  An unknown scenario exits with code 2.
//
// Modification history (NereusSDR):
//   2026-10-08: native audio plan Task 1 (V-HW-8): probe and version-1
//               scenarios. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-09: native audio plan Task 13 (R-AUD-17): protocol 3, the
//               tone through a clock matcher ring in the window's shared
//               region; pcm-record, busy and bad-ring scenarios.  J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 15 (R-AUD-19): the ASIO records.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QString>

namespace NereusSDR::Test {

int runFakeCaptureChild(const QString& scenario);

} // namespace NereusSDR::Test
