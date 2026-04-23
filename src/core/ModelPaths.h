#pragma once

// =================================================================
// src/core/ModelPaths.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original (no Thetis equivalent — Thetis bundles rnnoise
// as a DLL and loads it via P/Invoke; NereusSDR resolves the .bin
// file path at runtime across platform install layouts).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-23 — Written for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted development via Anthropic Claude Code.
// =================================================================

#include <QString>

namespace NereusSDR::ModelPaths {

// Path to the bundled rnnoise "Default (large)" model. Resolves to:
//   macOS:   .app/Contents/Resources/models/rnnoise/Default_large.bin
//   Linux:   <exe-dir>/../share/NereusSDR/models/rnnoise/Default_large.bin
//            OR <XDG_DATA>/NereusSDR/models/rnnoise/Default_large.bin
//   Windows: <exe-dir>/models/rnnoise/Default_large.bin
//   Dev:     <source>/third_party/rnnoise/models/Default_large.bin
//            (located via relative path from the build-dir binary)
//
// Returns empty QString if the file isn't found at any of the search
// locations. The caller should warn and disable NR3 in that case.
// See docs/architecture/phase3g-rx-epic-c1-nr-xplat-plan.md Task 22
// for packaging details.
QString rnnoiseDefaultLargeBin();

} // namespace NereusSDR::ModelPaths
