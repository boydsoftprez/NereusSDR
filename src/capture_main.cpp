// =================================================================
// src/capture_main.cpp  (NereusSDR)
// =================================================================
// nereus-audio-capture: the private helper process that owns the PC
// microphone for NereusSDR and nereusd, so a native input open that never
// returns cannot stall receiving, reconnecting or exiting.
//
// no-port-check: NereusSDR-original. Entry point only; the control loop is
// runCaptureHelper() in src/core/audio/CaptureHelper.cpp. It constructs no
// RadioModel, DSP channel or widget.
//
// Design: docs/architecture/2026-09-22-optional-microphone-capture-design.md
// Requirement R-R3-36.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 13 (R-AUD-17): a QCoreApplication,
//               so the native engines' streams (QObjects of the helper's
//               main thread) get their queued events.  J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 15 (R-AUD-01, R-AUD-22): on Windows
//               the helper installs the ASIO driver adapter and the
//               cmASIO set-up of the mic.  J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#include "core/audio/CaptureHelper.h"

// Only the nereus-audio-capture target on Windows defines this (CMake);
// the tests that build this file never carry the ASIO driver code.
#if defined(NEREUS_CAPTURE_HELPER_ASIO)
#include "core/audio/cmasio.h"
#endif

#include <QCoreApplication>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
#if defined(NEREUS_CAPTURE_HELPER_ASIO)
    NereusSDR::setCaptureHelperAsio(NereusSDR::CmAsio::helperHooks());
#endif
    return NereusSDR::runCaptureHelper(argc, argv);
}
