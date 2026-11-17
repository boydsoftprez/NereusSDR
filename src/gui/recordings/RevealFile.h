#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/recordings/RevealFile.h  (NereusSDR)
// =================================================================
//
// Show in Finder, File Explorer or the file manager: opens a folder, or the
// folder holding a file with the file picked where the platform can. Used
// where a recording or the recordings folder is on this computer (the Core
// runs here). Design: docs/architecture/2026-11-15-recording-voice-keyer-
// dsr-design.md, sections 3 and 6.4.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 6.
// =================================================================

#include <QString>

namespace NereusSDR::RevealFile {

/// The menu words for this platform: "Show in Finder", "Show in File
/// Explorer" or "Show in folder".
QString actionText();

/// Opens `path` in the platform's file manager: a folder is opened, a file
/// is shown in its folder (picked in Finder and File Explorer). False, with
/// a warning in the log, when the path does not exist or nothing opened.
bool reveal(const QString& path);

}  // namespace NereusSDR::RevealFile
