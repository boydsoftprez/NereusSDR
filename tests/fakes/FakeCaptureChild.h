// =================================================================
// tests/fakes/FakeCaptureChild.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Scripted stand-in for the
// nereus-audio-capture helper, run by a test binary re-executing itself
// with --fake-capture-child <scenario>; no Thetis logic.
//
// Scenarios (N is the generation of the Open being answered):
//   ready                 Hello; on Open: Opening, Ready, then a 480-frame
//                         PCM record of a 1 kHz tone every 10 ms.  Stop
//                         ends PCM and answers Stopped.
//   hang-open             Hello; on Open: Opening, then nothing at all
//                         (Stop and Shutdown are ignored too).  With
//                         NEREUS_FAKE_CAPTURE_HANG_DIR set, it then creates
//                         the empty file <dir>/<its pid>.  Before that
//                         Open, Shutdown still ends it.
//   no-hello              Never writes anything and ignores every command.
//   permission-then-ready Hello; on Open: Permission, 300 ms later as ready.
//   crash-after-ready     As ready; exits with code 3 after 100 ms of PCM.
//   malformed             Hello, then a record with bad magic at once.
//   oversize              Hello; on Open: Opening, Ready, then one PCM
//                         record whose header claims frameCount 4801.
//   input-lost            As ready; after 100 ms of PCM sends Failed /
//                         input-lost and stops PCM.
//   ignore-stop           As ready but never answers Stop.
//   stale                 Hello; on Open: Opening for N, then Ready and PCM
//                         tagged with generation N-1 (4294967295 when N is 1).
// Every scenario exits with code 0 on stdin EOF, and on Shutdown unless
// it is ignoring commands.  An unknown scenario exits with code 2.
// =================================================================

#pragma once

#include <QString>

namespace NereusSDR::Test {

int runFakeCaptureChild(const QString& scenario);

} // namespace NereusSDR::Test
