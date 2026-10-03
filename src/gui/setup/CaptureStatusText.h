#pragma once

// =================================================================
// src/gui/setup/CaptureStatusText.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Operator wording for the PC
// microphone capture status shown on the Audio Setup pages; no Thetis
// logic.
//
// Design: docs/architecture/2026-09-22-optional-microphone-capture-design.md
// (operator status beside the microphone controls).  Requirement R-R3-36.
//
// Modification history (NereusSDR):
//   2026-09-22: J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
// =================================================================

#include "core/audio/CaptureSupervisor.h"

#include <QString>

namespace NereusSDR {

// Plain-English text for a capture status.  Never mentions the helper
// process, protocol or generation; raw diagnostics go to the log instead.
// An empty device name reads as "the system default microphone".
QString captureStatusText(const CaptureSupervisor::Status& status);

} // namespace NereusSDR
