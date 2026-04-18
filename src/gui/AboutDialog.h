#pragma once

// =================================================================
// src/gui/AboutDialog.h  (NereusSDR)
// =================================================================
//
// Design and layout inspired by AetherSDR (ten9876/AetherSDR, GPLv3):
//   src/gui/MainWindow.cpp (about-box section) and
//   src/gui/TitleBar.{h,cpp}. AetherSDR has no per-file headers;
//   project-level citation per docs/attribution/HOW-TO-PORT.md rule 6.
//
// No Thetis-derived code — Thetis's About is in console.cs and was not
// used as a source for this dialog.
//
// no-port-check: this file mentions Thetis sources (console.cs) and
// contributor callsigns only as data in the displayed contributor list;
// no Thetis code is ported here. AetherSDR design lineage is cited
// above per HOW-TO-PORT.md rule 6.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Implemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted authoring via Anthropic
//                 Claude Code. Contributor list, copyright string,
//                 and §5(d) notice content are NereusSDR-specific.
// =================================================================

#include <QDialog>

namespace NereusSDR {

class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);

private:
    void buildUI();
};

} // namespace NereusSDR
