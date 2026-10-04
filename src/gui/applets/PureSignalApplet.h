// =================================================================
// src/gui/applets/PureSignalApplet.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/PSForm.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-04 — Live Core status and passive numeric correction readouts,
//                 by J.J. Boyd (KG4VCF), with OpenAI Codex assistance.

//   2026-04-18 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Layout ports Thetis PSForm.cs (PureSignal feedback/correction controls). All controls NYI — wired in later phase (3M-4).
//   2026-05-06 — Phase 3M-4 Task 13: replaced NyiOverlay::markNyi calls with
//                 live wiring to the PureSignal coordinator (Task 7).  Added
//                 right-click pattern (every control routes to
//                 openPureSignalDialogRequested signal; MainWindow connects
//                 it to openPureSignalDialog) per design doc §8.4.2.  Save /
//                 Restore use QFileDialog with default folder
//                 ~/.config/NereusSDR/PureSignal/.  J.J. Boyd (KG4VCF), with
//                 AI-assisted source-first protocol via Anthropic Claude Code.
//   2026-09-22 — Routed PS3 actions and readback through the shared
//                 PureSignalSessionFacade; station asset IDs replace desktop
//                 correction paths across the Core boundary.
// =================================================================

/*  PSForm.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2000-2025 Original authors
Copyright (C) 2020-2025 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#pragma once
#include "AppletWidget.h"
#include <QPointer>
#include <QVariantMap>

class QPushButton;
class QLabel;

namespace NereusSDR {

class HGauge;
class DspAssetDialog;
class PureSignalSessionFacade;
enum class Ps3Action;

// PureSignal / PS-A feedback predistortion controls.
//
// Actions and readback use RadioModel's shared PureSignalSessionFacade so the
// same surface works for local and remote station sessions.
//
// Controls:
//   1. Calibrate button      — QPushButton (non-toggle) → Single action
//   2. Auto-cal toggle       — QPushButton green "Auto" ↔ session settings
//   3. Feedback level gauge  — HGauge (0-100, yellow@70, red@90, title "FB Level")
//                              ← feedbackLevelChanged 0..255 → 0..100
//   4. Correction mag gauge  — HGauge (0-100, yellow@80, red@95, title "Correction")
//                              ← correctionPeakChanged 0..1 → 0..100
//   5. Save coefficients     — QPushButton "Save" → station SaveCorrection
//   6. Restore coefficients  — QPushButton "Restore" → station asset manager
//   7. Two-tone test         — QPushButton green toggle "2-Tone" → SetTwoTone
//   8. Status LEDs           — 3x QLabel (24x14 rounded: "Cal", "Run", "Fbk")
//      Cal LED active during LSETUP/LCOLLECT/LCALC (driven by calStateChanged)
//      Run LED active during LSTAYON
//      Fbk LED active when feedback samples flow (feedbackActiveChanged)
//
// Plus: info readout labels (Iterations, Feedback dB, Correction dB) — driven
// by calibrationCountChanged + feedbackLevelChanged + correctionPeakChanged.
//
// Right-click on every control emits openPureSignalDialogRequested, which
// MainWindow routes to openPureSignalDialog (Tools → PureSignal…) per the
// Thetis right-click-to-associated-window pattern (cf. chkFWCATUBypass_MouseDown
// at console.cs:46149-46152 [v2.10.3.13]).
class PureSignal;

class PureSignalApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit PureSignalApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("pure_signal"); }
    QString appletTitle() const override { return QStringLiteral("PureSignal"); }
    void    syncFromModel() override;

public slots:
    // ── Phase 3M-4 Task 13: late-bound coordinator wiring ──────────────────
    //
    // Production always retains RadioModel's shared facade. This slot only
    // replaces that facade's local coordinator for the existing test seam.
    void setPureSignal(PureSignal* coordinator);

signals:
    // ── Phase 3M-4 Task 13: right-click → open PsForm ──────────────────────
    // Mirrors the Thetis right-click-to-associated-window pattern (e.g.
    // chkFWCATUBypass_MouseDown at console.cs:46149-46152 [v2.10.3.13]).
    // MainWindow connects this signal to openPureSignalDialog (Tools →
    // PureSignal…); the same singleton dialog instance services every right-
    // click on every PureSignalApplet control.
    void openPureSignalDialogRequested();

private:
    void buildUI();
    void wireRightClicks();
    void wireFacade();
    void refreshFromFacade();
    void requestAction(Ps3Action action, const QVariantMap& arguments = {});
    void showRestoreDialog();
    void setLedActive(QLabel* led, bool active);
    void setupRightClick(QWidget* widget);

    // Shared facade owned by RadioModel. The QPointer safely clears when the
    // model tears down its session services.
    QPointer<PureSignalSessionFacade> m_facade;
    QPointer<DspAssetDialog> m_restoreDialog;

    // Control 1 — calibrate button (non-toggle)
    QPushButton* m_calibrateBtn  = nullptr;
    // Control 2 — auto-cal toggle (green)
    QPushButton* m_autoCalBtn    = nullptr;

    // Control 3 — feedback level gauge (0-100, yellow@70, red@90)
    HGauge* m_feedbackGauge      = nullptr;
    // Control 4 — correction magnitude gauge (0-100, yellow@80, red@95)
    HGauge* m_correctionGauge    = nullptr;

    // Control 5 — save coefficients
    QPushButton* m_saveBtn       = nullptr;
    // Control 6 — restore coefficients
    QPushButton* m_restoreBtn    = nullptr;

    // Control 7 — two-tone test (green toggle)
    QPushButton* m_twoToneBtn    = nullptr;

    // Control 8 — status LEDs: "Cal", "Run", "Fbk"
    QLabel* m_led[3]             = {};

    // Info readout labels
    QLabel* m_iterations         = nullptr;
    QLabel* m_feedbackDb         = nullptr;
    QLabel* m_correctionDb       = nullptr;
    QLabel* m_status             = nullptr;
};

} // namespace NereusSDR
