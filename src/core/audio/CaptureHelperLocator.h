// =================================================================
// src/core/audio/CaptureHelperLocator.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Finds the installed
// nereus-audio-capture helper beside its parent for each platform
// layout; no Thetis logic.
//
// Design: docs/architecture/2026-09-22-optional-microphone-capture-design.md
// (Packaging and boundaries of the guarantee).  Requirement R-R3-36.
// =================================================================

#pragma once

#include <QCoreApplication>
#include <QString>

namespace NereusSDR {

// Returns the absolute helper path or an empty string when none exists.
//
// Lookup order, first existing executable file wins:
//   macOS    <appDir>/../Helpers/nereus-audio-capture  (inside the .app)
//            <appDir>/nereus-audio-capture             (beside nereusd)
//   Linux    <appDir>/nereus-audio-capture
//            <appDir>/../lib/nereus/nereus-audio-capture
//   Windows  <appDir>/nereus-audio-capture.exe
QString locateCaptureHelper(const QString& applicationDir = QCoreApplication::applicationDirPath());

} // namespace NereusSDR
