// =================================================================
// src/gui/PsaIndicatorWidget.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/ucInfoBar.cs (lblFB / lblPS sub-
//     controls + updatePSDisplay state machine + lblFB_MouseDown
//     click handlers + setToolTips dynamic tooltip)
//   Project Files/Source/Console/PSForm.cs    (FeedbackColourLevel
//     color computation at lines 1123-1138)
// original licences from Thetis source are included below.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-06 — Phase 3M-4 Task 10: created by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude
//                 Code.  Source-first 1:1 port of the Thetis bottom-
//                 banner PSA indicator pair (ucInfoBar.cs:820-1098)
//                 [v2.10.3.13] plus the FeedbackColourLevel computation
//                 (PSForm.cs:1123-1138).  Two side-by-side QLabel
//                 badges in MainWindow's bottom-banner HBox, inserted
//                 between m_rxDashboard and m_stationBlock per design
//                 doc §4 #5 option B.  6-state machine: PS off (DimGray),
//                 PS armed no-MOX (SeaGreen), TX hide-feedback / no-
//                 cal-change (Feedback color / SeaGreen), TX numeric
//                 (Feedback color / SeaGreen), TX correcting (Feedback
//                 color / Lime), TX corrections-applied not-correcting
//                 (Feedback color / SeaGreen).  Color thresholds 0-90 /
//                 91-128 / 129-181 / 182+ verbatim.  Tooltip text
//                 verbatim with "Showing level, " prefix gating.  Left-
//                 click on FB emits invertRedBlueRequested; right-click
//                 emits hideFeedbackToggleRequested; PS label is
//                 passive per Thetis.
// =================================================================

// --- From ucInfoBar.cs ---
/*  ucInfoBar.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

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

// --- From PSForm.cs ---
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

#include <QColor>
#include <QString>
#include <QWidget>

class QLabel;
class QMouseEvent;

namespace NereusSDR {

class RadioModel;
class PureSignal;

class PsaIndicatorWidget : public QWidget {
    Q_OBJECT

public:
    // Constructs the widget.  When `model` is non-null and a PureSignal
    // coordinator + MoxController are reachable, the widget auto-wires
    // itself to their signals.  Tests pass nullptr and drive state via
    // the public setters.
    explicit PsaIndicatorWidget(RadioModel* model, QWidget* parent = nullptr);
    ~PsaIndicatorWidget() override;

    // ── Test accessors ────────────────────────────────────────────────────
    QString fbText() const;
    QString psText() const;
    QColor  fbBackgroundColor() const;
    QColor  psBackgroundColor() const;

    // ── State setters (driven from PureSignal coordinator + MoxController;
    //    also used as test entry points to drive the 6-state machine
    //    directly without spinning up a full RadioModel) ─────────────────
    void setPsEnabled(bool on);
    void setMox(bool on);
    void setCorrectionsBeingApplied(bool on);
    void setFeedbackLevel(int level);     // 0..255
    void setInvertRedBlue(bool on);
    void setHideFeedback(bool on);

    // Phase 3M-4 bench-fix Round 2: byte-for-byte port of Thetis
    // ucInfoBar.PSInfo (ucInfoBar.cs:808-825 [v2.10.3.13]).  Updates all 5
    // UI state fields atomically and gates the redraw on
    // (calibrationAttemptsChanged && m_mox).  Wired in wireToModel() to
    // PureSignal::psInfoChanged which itself fires only when the coordinator
    // has m_autoCalEnabled true and HasInfoChanged true (PSForm.cs:614-619
    // [v2.10.3.13]).  Tests drive this slot directly to exercise the
    // 6-state machine without spinning up a coordinator.
    void psInfo(int level, bool feedbackLevelOk,
                bool correctionsApplied,
                bool calibrationAttemptsChanged,
                const QColor& feedbackColour);

    // From Thetis ucInfoBar.cs:1236-1243 [v2.10.3.13] —
    // _useSmallFonts toggle.  Compact "FB"/"Correct" vs full
    // "Feedback"/"Correcting".  Auto-driven from width on resize once
    // wired into a layout; tests set explicitly.
    void setUseSmallFonts(bool on);

    // ── Test helpers: simulate mouse events on the FB label ───────────────
    void simulateLeftClickOnFb();
    void simulateRightClickOnFb();
    void simulateLeftClickOnPs();
    void simulateRightClickOnPs();

protected:
    // Forwards left/right clicks on the FB label to invertRedBlueRequested
    // / hideFeedbackToggleRequested per ucInfoBar.cs:1042-1054 [v2.10.3.13].
    // Clicks on the PS label are intentionally ignored (Thetis has no
    // lblPS_MouseDown handler).
    void mousePressEvent(QMouseEvent* event) override;

signals:
    // Click-driven state-flip requests.  MainWindow connects these back
    // to PureSignal::setInvertRedBlue / setHideFeedback so the Setup
    // checkboxes (Task 11) and FB-label clicks share state.
    void invertRedBlueRequested();
    void hideFeedbackToggleRequested();

private:
    // Auto-wires to PureSignal + MoxController signals when both are
    // reachable from m_radioModel.  No-op when nullptr.
    void wireToModel();

    // Ports updatePSDisplay verbatim.  See header docblock above for
    // the 6-state map.
    void updateDisplay();

    // Computes the FB-label background color from m_feedbackLevel and
    // m_invertRedBlue.  Mirrors PSForm.cs:1123-1138 FeedbackColourLevel
    // [v2.10.3.13].  Standalone helper (not a delegation to
    // PureSignal::feedbackColour) because tests construct the widget
    // with model=nullptr and need this behavior independent of the
    // coordinator.
    QColor computeFeedbackColour() const;

    // Ports setToolTips verbatim from ucInfoBar.cs:1081-1096 [v2.10.3.13].
    void updateTooltip();

    // Applies a solid background color to a QLabel via stylesheet.
    // Background color drives the "badge" look; foreground stays the
    // default near-white for legibility against all 6 background
    // colors (matches Thetis lblFB / lblPS rendering).
    void applyBackground(QLabel* label, const QColor& bg);

    RadioModel*  m_radioModel{nullptr};
    PureSignal*  m_pureSignal{nullptr};

    QLabel* m_lblFb{nullptr};
    QLabel* m_lblPs{nullptr};

    // State (mirrors ucInfoBar.cs members from lines 802-806 + 1186
    // [v2.10.3.13]).  Phase 3M-4 bench-fix Round 2 dropped the
    // NereusSDR-only `m_correcting` field — Thetis ucInfoBar has only
    // 5 PS state fields and the Lime/SeaGreen split is decided purely
    // by `_bCorrectionsBeingApplied` (ucInfoBar.cs:856-865 [v2.10.3.13]).
    // The FB-label background colour is recomputed locally from
    // m_feedbackLevel + m_invertRedBlue via computeFeedbackColour();
    // psInfo()'s `feedbackColour` parameter is accepted for API parity
    // but not stored (the local computation matches the coordinator's
    // emit by construction — both use the same PSForm.cs:1123-1138
    // [v2.10.3.13] formula).
    bool m_psEnabled{false};
    bool m_mox{false};
    bool m_correctionsApplied{false};
    bool m_calChangedSinceLastDraw{false};
    int  m_feedbackLevel{0};
    bool m_invertRedBlue{false};
    bool m_hideFeedback{false};
    bool m_useSmallFonts{false};
    int m_lastRemoteAttempts{0};
};

} // namespace NereusSDR
