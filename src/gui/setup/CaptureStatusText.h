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
//   2026-10-09: native audio plan Task 16 fix round (R-AUD-09, R-AUD-11):
//               micRoleStatusText().  J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "core/audio/CaptureSupervisor.h"
#include "core/audio/IAudioStreamHost.h"

#include <QString>

namespace NereusSDR {

// Plain-English text for a capture status.  Never mentions the helper
// process, protocol or generation; raw diagnostics go to the log instead.
// An empty device name reads as "the system default microphone".
QString captureStatusText(const CaptureSupervisor::Status& status);

// The status line while the chosen mic is missing or held by another
// program (the mic role silent for that reason): "PC mic not connected" or
// "PC mic in use by another program", shown in amber in place of the
// capture status.  Empty otherwise.
QString micRoleStatusText(const AudioRoleStatus& role);

} // namespace NereusSDR
