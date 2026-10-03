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
// =================================================================

#include "core/audio/CaptureHelper.h"

int main(int argc, char** argv)
{
    return NereusSDR::runCaptureHelper(argc, argv);
}
