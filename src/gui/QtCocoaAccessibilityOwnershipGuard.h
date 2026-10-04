// NereusSDR — app-local compatibility correction for the Qt 6.11.0 Cocoa
// synthetic accessibility ownership regression. JJ Boyd (KG4VCF),
// 2026-10-03, with OpenAI Codex assistance.
#pragma once

#include <QString>

// Call on the main GUI thread immediately after QApplication construction.
// Unaffected Qt versions/platforms are left alone. False means the exact
// supported ABI of the targeted Qt 6.11.0 Cocoa runtime cannot be established; the caller
// must refuse startup rather than run the known vulnerable Cocoa path.
bool installQtCocoaAccessibilityOwnershipGuard(QString* rejectionReason = nullptr);
