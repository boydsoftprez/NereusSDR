// =================================================================
// src/gui/PsaIndicatorWidget.cpp  (NereusSDR)
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
//                 Code.  See PsaIndicatorWidget.h for the full
//                 state-machine map, design-doc references, and the
//                 line-range-by-line-range mapping into ucInfoBar.cs
//                 and PSForm.cs.
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


#include "gui/PsaIndicatorWidget.h"

#include "core/MoxController.h"
#include "core/PureSignal.h"
#include "models/RadioModel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>

namespace NereusSDR {

// ── Color palette ─────────────────────────────────────────────────────────
//
// Source: System.Drawing named colors used in ucInfoBar.cs:843-892
// [v2.10.3.13] verbatim.  Docs at PSForm.cs:1123-1138 [v2.10.3.13]
// for the FeedbackColourLevel computation.
//
// DimGray   — System.Drawing.Color.DimGray   (105, 105, 105)
// SeaGreen  — System.Drawing.Color.SeaGreen  ( 46, 139,  87)
// Lime      — System.Drawing.Color.Lime      (  0, 255,   0)
// Yellow    — System.Drawing.Color.Yellow    (255, 255,   0)
// Red       — System.Drawing.Color.Red       (255,   0,   0)
// DodgerBlue— System.Drawing.Color.DodgerBlue( 30, 144, 255)
//
// Constants are kept named so the test cases can compare against
// well-defined values rather than magic literals scattered across
// the file.
namespace {
inline QColor kDimGray()    { return QColor(105, 105, 105); }
inline QColor kSeaGreen()   { return QColor( 46, 139,  87); }
inline QColor kLime()       { return QColor(  0, 255,   0); }
inline QColor kYellow()     { return QColor(255, 255,   0); }
inline QColor kRed()        { return QColor(255,   0,   0); }
inline QColor kDodgerBlue() { return QColor( 30, 144, 255); }

// Foreground for badge text.  System.Drawing default ForeColor is
// ControlText (system-theme dependent).  Thetis's bottom banner runs
// dark, so labels render with near-black text on the colored
// backgrounds.  Use a clearly contrasting value here.
inline QColor kFgText()     { return QColor( 16,  16,  16); }
} // namespace

PsaIndicatorWidget::PsaIndicatorWidget(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_radioModel(model)
{
    setObjectName(QStringLiteral("psaIndicator"));

    // Layout: two compact, side-by-side QLabel "badges".  Spacing
    // matches the existing bottom-banner separator gaps so the pair
    // visually nests with m_rxDashboard (left) and m_stationBlock
    // (right).
    auto* hbox = new QHBoxLayout(this);
    hbox->setContentsMargins(0, 0, 0, 0);
    hbox->setSpacing(4);

    m_lblFb = new QLabel(this);
    m_lblFb->setObjectName(QStringLiteral("lblFB"));
    m_lblFb->setAlignment(Qt::AlignCenter);
    m_lblFb->setMinimumWidth(64);
    m_lblFb->setText(tr("Feedback"));
    hbox->addWidget(m_lblFb);

    m_lblPs = new QLabel(this);
    m_lblPs->setObjectName(QStringLiteral("lblPS"));
    m_lblPs->setAlignment(Qt::AlignCenter);
    m_lblPs->setMinimumWidth(86);
    m_lblPs->setText(tr("Pure Signal2"));
    hbox->addWidget(m_lblPs);

    setLayout(hbox);

    wireToModel();
    updateDisplay();
    updateTooltip();

    // Phase 3M-4 bench-fix: PureSignal coordinator is constructed
    // late inside RadioModel's WDSP-init lambda — AFTER PsaIndicator-
    // Widget is built.  Subscribe to the late-bind seam (RadioModel::
    // pureSignalCoordinatorReady, added in Task 13 commit 2271f8a) so
    // we re-wire when the coordinator becomes available.  Mirrors the
    // PureSignalApplet pattern at PureSignalApplet.cpp:118-123.
    //
    // ANAN-G2E bench-fix 2026-05-23 (JJ Boyd): handle disconnect /
    // reconnect.  RadioModel emits pureSignalCoordinatorReady(nullptr)
    // on disconnect (RadioModel.cpp:6800-6801) and a fresh non-null
    // pointer on each reconnect (RadioModel.cpp:3438).  The original
    // guard `if (m_pureSignal) { return; }` made m_pureSignal a stale
    // pointer after disconnect and prevented re-wiring on reconnect —
    // the badge stayed grey because no autoCalEnabledChanged signal
    // from the new coordinator reached setPsEnabled.  Bench-confirmed
    // on G2E PS run at 11:03-11:08 (log line 1278 reconnect was the
    // failing case; corrApplied=1 in calcc engine, m_psEnabled stuck
    // at false in the widget).
    if (m_radioModel) {
        connect(m_radioModel, &RadioModel::pureSignalCoordinatorReady,
                this, [this](PureSignal* ps) {
                    if (!ps) {
                        // Coordinator torn down — clear stale pointer
                        // and grey the badge so the next reconnect can
                        // re-wire cleanly.  The old PureSignal's signal
                        // connections were auto-disconnected by Qt when
                        // its unique_ptr destructor ran in RadioModel::
                        // teardown, so no explicit disconnect needed.
                        m_pureSignal = nullptr;
                        m_psEnabled = false;
                        m_correctionsApplied = false;
                        m_calChangedSinceLastDraw = false;
                        updateDisplay();
                        updateTooltip();
                        return;
                    }
                    // Fresh coordinator: force wireToModel to re-seed
                    // m_pureSignal + (re)install signal hooks under
                    // Qt::UniqueConnection against the new instance.
                    m_pureSignal = nullptr;
                    wireToModel();
                    updateDisplay();
                    updateTooltip();
                });
    }
}

PsaIndicatorWidget::~PsaIndicatorWidget() = default;

// ── Wiring ────────────────────────────────────────────────────────────────

void PsaIndicatorWidget::wireToModel()
{
    if (!m_radioModel) {
        return;
    }
    m_pureSignal = m_radioModel->pureSignal();
    if (m_pureSignal) {
        // Phase 3M-4 bench-fix Round 2: PSAEnabled in Thetis tracks
        // PSForm.AutoCalEnabled, NOT the master enable.  From console.cs:
        // 2280-2284 [v2.10.3.13]: `infoBar.PSAEnabled = psform.AutoCalEnabled;`.
        // Round 1 wired this to enabledChanged which auto-fires true on
        // ctor (PureSignal.cpp:98 [v2.10.3.13 port]) — that made the badge
        // visible but at the wrong semantic layer.  The correct seam is
        // autoCalEnabledChanged so the badge greys out when the user
        // toggles PS-A off in TxApplet.
        connect(m_pureSignal, &PureSignal::autoCalEnabledChanged,
                this, &PsaIndicatorWidget::setPsEnabled,
                Qt::UniqueConnection);

        // Phase 3M-4 bench-fix Round 2: consolidated PSInfo dispatch.
        // PureSignal::psInfoChanged fires only when (m_autoCalEnabled &&
        // HasInfoChanged) per PSForm.cs:614-619 timer1code [v2.10.3.13]
        // — exactly the gate Thetis uses before calling
        // console.InfoBarFeedbackLevel.  All five fields arrive atomically,
        // matching ucInfoBar.PSInfo() (ucInfoBar.cs:808-825 [v2.10.3.13]).
        connect(m_pureSignal, &PureSignal::psInfoChanged,
                this, &PsaIndicatorWidget::psInfo,
                Qt::UniqueConnection);

        // Out-of-band UI mirror state.  invertRedBlueChanged and
        // hideFeedbackChanged update the badge colours and tooltip text
        // independent of the polling cadence — keep these as direct
        // subscriptions.  The previous per-field signals
        // (correctingChanged / feedbackLevelChanged / calibrationCount-
        // Changed) were dropped: psInfoChanged carries the same data
        // atomically and avoids the lossy lambda that ping-ponged
        // correctingChanged values into the wrong setters.
        connect(m_pureSignal, &PureSignal::invertRedBlueChanged,
                this, &PsaIndicatorWidget::setInvertRedBlue,
                Qt::UniqueConnection);
        connect(m_pureSignal, &PureSignal::hideFeedbackChanged,
                this, &PsaIndicatorWidget::setHideFeedback,
                Qt::UniqueConnection);

        // Seed initial state from the coordinator (covers the case where
        // the widget is constructed mid-session, e.g. after reconnect).
        m_psEnabled          = m_pureSignal->isAutoCalEnabled();
        m_correctionsApplied = m_pureSignal->correctionsBeingApplied();
        m_feedbackLevel      = m_pureSignal->feedbackLevel();
        m_invertRedBlue      = m_pureSignal->invertRedBlue();
        m_hideFeedback       = m_pureSignal->hideFeedback();
    }

    // MoxController for MOX state.  Routed via hardwareFlipped(bool isTx)
    // per Thetis OnMoxChangeHandler at ucInfoBar.cs:554-561 [v2.10.3.13].
    // Phase 3M-4 bench-fix: Qt::UniqueConnection makes wireToModel() safe
    // to call twice (ctor + pureSignalCoordinatorReady late-bind seam).
    if (auto* mox = m_radioModel->moxController()) {
        connect(mox, &MoxController::hardwareFlipped,
                this, &PsaIndicatorWidget::setMox,
                Qt::UniqueConnection);
        m_mox = mox->isMox();
    }
}

// ── Test accessors ────────────────────────────────────────────────────────

QString PsaIndicatorWidget::fbText() const
{
    return m_lblFb ? m_lblFb->text() : QString();
}

QString PsaIndicatorWidget::psText() const
{
    return m_lblPs ? m_lblPs->text() : QString();
}

QColor PsaIndicatorWidget::fbBackgroundColor() const
{
    return m_lblFb ? m_lblFb->palette().color(QPalette::Window) : QColor();
}

QColor PsaIndicatorWidget::psBackgroundColor() const
{
    return m_lblPs ? m_lblPs->palette().color(QPalette::Window) : QColor();
}

// ── State setters ─────────────────────────────────────────────────────────

void PsaIndicatorWidget::setPsEnabled(bool on)
{
    if (on == m_psEnabled) {
        return;
    }
    m_psEnabled = on;
    if (!m_psEnabled) {
        // From Thetis ucInfoBar.cs:832-833 [v2.10.3.13]:
        //   if (!_psEnabled) setPSboolsToFalse();
        // setPSboolsToFalse (ucInfoBar.cs:562-567 [v2.10.3.13]) clears
        // _bCalibrationAttemptsChanged + _bCorrectionsBeingApplied +
        // _bFeedbackLevelOk.  m_correcting was a NereusSDR-only field
        // dropped in Round 2.
        m_correctionsApplied      = false;
        m_calChangedSinceLastDraw = false;
    }
    updateDisplay();
}

void PsaIndicatorWidget::setMox(bool on)
{
    if (on == m_mox) {
        return;
    }
    m_mox = on;
    // From Thetis ucInfoBar.cs:554-561 [v2.10.3.13] OnMoxChangeHandler:
    //   if (!_mox) setPSboolsToFalse();
    if (!m_mox) {
        m_correctionsApplied      = false;
        m_calChangedSinceLastDraw = false;
    }
    updateDisplay();
}

void PsaIndicatorWidget::setCorrectionsBeingApplied(bool on)
{
    if (on == m_correctionsApplied) {
        return;
    }
    m_correctionsApplied = on;
    updateDisplay();
}

void PsaIndicatorWidget::setFeedbackLevel(int level)
{
    // Phase 3M-4 bench-fix Round 2: setFeedbackLevel only stores the
    // numeric value and refreshes the colour through computeFeedbackColour().
    // The FB label "Feedback" → numeric flip is now driven exclusively by
    // psInfo()'s calibrationAttemptsChanged path (matches Thetis
    // ucInfoBar.cs:812-823 [v2.10.3.13] PSInfo body — _bCalibration-
    // AttemptsChanged is set per-call from a parameter, not bumped from
    // level changes).
    if (level == m_feedbackLevel) {
        return;
    }
    m_feedbackLevel = level;
    updateDisplay();
}

void PsaIndicatorWidget::psInfo(int level, bool feedbackLevelOk,
                                 bool correctionsApplied,
                                 bool calibrationAttemptsChanged,
                                 const QColor& feedbackColour)
{
    // From Thetis ucInfoBar.cs:808-825 PSInfo() [v2.10.3.13]:
    //   public void PSInfo(int level, bool bFeedbackLevelOk,
    //                      bool bCorrectionsBeingApplied,
    //                      bool bCalibrationAttemptsChanged,
    //                      Color feedbackColour)
    //   {
    //       if (_shutDown) return;
    //       _bCalibrationAttemptsChanged = bCalibrationAttemptsChanged;
    //       if (_bCalibrationAttemptsChanged && _mox) {
    //           _nFeedbackLevel = level;
    //           _feedbackColour = feedbackColour;
    //           _bCorrectionsBeingApplied = bCorrectionsBeingApplied;
    //           _bFeedbackLevelOk = bFeedbackLevelOk;
    //           updatePSDisplay();
    //           _psTimer.Start();
    //       }
    //   }
    //
    // Two parameters Thetis stores but updatePSDisplay() does not read:
    //   - _bFeedbackLevelOk: kept for API parity, no widget effect.
    //   - _feedbackColour:  Thetis's updatePSDisplay applies this
    //     directly to lblFB.BackColor.  NereusSDR derives the colour
    //     locally via computeFeedbackColour() from m_feedbackLevel +
    //     m_invertRedBlue and the two agree by construction (the
    //     coordinator emits the colour computed from its own
    //     m_invertRedBlue, and the widget subscribes to
    //     invertRedBlueChanged so they stay synchronised).  Accepting
    //     the parameter for byte-for-byte signature parity.
    //
    // The fade timer (_psTimer) that gradually dims lblFB.BackColor
    // between PSInfo() calls is intentionally deferred — colour stays
    // sticky until the next psInfo() with calChanged=true OR a state
    // change (setMox / setPsEnabled).  Track as a UX follow-up if bench
    // shows the fade is needed.
    Q_UNUSED(feedbackLevelOk);
    Q_UNUSED(feedbackColour);

    m_calChangedSinceLastDraw = calibrationAttemptsChanged;

    if (m_calChangedSinceLastDraw && m_mox) {
        m_feedbackLevel      = level;
        m_correctionsApplied = correctionsApplied;
        updateDisplay();
    }
}

void PsaIndicatorWidget::setInvertRedBlue(bool on)
{
    if (on == m_invertRedBlue) {
        return;
    }
    m_invertRedBlue = on;
    updateDisplay();
    updateTooltip();
}

void PsaIndicatorWidget::setHideFeedback(bool on)
{
    if (on == m_hideFeedback) {
        return;
    }
    m_hideFeedback = on;
    updateDisplay();
    updateTooltip();
}

void PsaIndicatorWidget::setUseSmallFonts(bool on)
{
    if (on == m_useSmallFonts) {
        return;
    }
    m_useSmallFonts = on;
    updateDisplay();
}

// ── Test helpers ──────────────────────────────────────────────────────────

void PsaIndicatorWidget::simulateLeftClickOnFb()
{
    if (!m_lblFb) { return; }
    QMouseEvent ev(QEvent::MouseButtonPress,
                   m_lblFb->geometry().center(),
                   m_lblFb->mapToGlobal(m_lblFb->geometry().center()),
                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    mousePressEvent(&ev);
}

void PsaIndicatorWidget::simulateRightClickOnFb()
{
    if (!m_lblFb) { return; }
    QMouseEvent ev(QEvent::MouseButtonPress,
                   m_lblFb->geometry().center(),
                   m_lblFb->mapToGlobal(m_lblFb->geometry().center()),
                   Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    mousePressEvent(&ev);
}

void PsaIndicatorWidget::simulateLeftClickOnPs()
{
    if (!m_lblPs) { return; }
    QMouseEvent ev(QEvent::MouseButtonPress,
                   m_lblPs->geometry().center(),
                   m_lblPs->mapToGlobal(m_lblPs->geometry().center()),
                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    mousePressEvent(&ev);
}

void PsaIndicatorWidget::simulateRightClickOnPs()
{
    if (!m_lblPs) { return; }
    QMouseEvent ev(QEvent::MouseButtonPress,
                   m_lblPs->geometry().center(),
                   m_lblPs->mapToGlobal(m_lblPs->geometry().center()),
                   Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    mousePressEvent(&ev);
}

// ── Mouse event handler ───────────────────────────────────────────────────

void PsaIndicatorWidget::mousePressEvent(QMouseEvent* event)
{
    // From Thetis ucInfoBar.cs:1042-1054 lblFB_MouseDown [v2.10.3.13]:
    //   if (e.Button == MouseButtons.Left)
    //       SwapRedBlue = !puresignal.InvertRedBlue;
    //   else if (e.Button == MouseButtons.Right)
    //       HideFeedback = !HideFeedback;
    //
    // The PS label is intentionally passive — Thetis registers no
    // lblPS_MouseDown handler.  Clicks on PS fall through to base
    // class.
    if (m_lblFb && m_lblFb->geometry().contains(event->pos())) {
        if (event->button() == Qt::LeftButton) {
            emit invertRedBlueRequested();
        } else if (event->button() == Qt::RightButton) {
            emit hideFeedbackToggleRequested();
        }
    }
    QWidget::mousePressEvent(event);
}

// ── 6-state machine ───────────────────────────────────────────────────────

void PsaIndicatorWidget::updateDisplay()
{
    if (!m_lblFb || !m_lblPs) {
        return;
    }

    // From Thetis ucInfoBar.cs:839-899 updatePSDisplay() [v2.10.3.13]:
    //   if (!_psEnabled) {
    //       lblFB.BackColor = DimGray; lblPS.BackColor = DimGray;
    //       lblFB.Text = _useSmallFonts ? "FB" : "Feedback";
    //       lblPS.Text = "Pure Signal2";
    //       return;
    //   }
    //   if (_mox) {
    //       if (_bCorrectionsBeingApplied) {
    //           lblPS.Text = _useSmallFonts ? "Correct" : "Correcting";
    //           lblPS.BackColor = Lime;
    //       } else {
    //           lblPS.Text = "Pure Signal2";
    //           lblPS.BackColor = SeaGreen;
    //       }
    //       lblFB.BackColor = _feedbackColour;
    //       if (_hideFeedback || !_bCalibrationAttemptsChanged)
    //           lblFB.Text = _useSmallFonts ? "FB" : "Feedback";
    //       else
    //           lblFB.Text = _nFeedbackLevel.ToString();
    //   } else {
    //       lblFB.BackColor = SeaGreen;
    //       lblPS.BackColor = SeaGreen;
    //       lblFB.Text = _useSmallFonts ? "FB" : "Feedback";
    //       lblPS.Text = "Pure Signal2";
    //   }
    if (!m_psEnabled) {
        applyBackground(m_lblFb, kDimGray());
        applyBackground(m_lblPs, kDimGray());
        m_lblFb->setText(m_useSmallFonts ? tr("FB") : tr("Feedback"));
        m_lblPs->setText(tr("Pure Signal2"));
        return;
    }

    if (m_mox) {
        // From Thetis ucInfoBar.cs:856-865 updatePSDisplay [v2.10.3.13]:
        //   if (_bCorrectionsBeingApplied) {
        //       lblPS.Text = _useSmallFonts ? "Correct" : "Correcting";
        //       lblPS.BackColor = Lime;
        //   } else {
        //       lblPS.Text = "Pure Signal2";
        //       lblPS.BackColor = SeaGreen;
        //   }
        // The Lime/SeaGreen split is decided purely by
        // _bCorrectionsBeingApplied — Thetis has no separate "Correcting"
        // sub-flag.  Phase 3M-4 bench-fix Round 2 dropped NereusSDR's
        // earlier nested m_correcting branch (a phantom flag that mirrored
        // the puresignal helper's `Correcting` derived property at
        // PSForm.cs:1106-1108 [v2.10.3.13], but that property is consumed
        // by PSForm.timer1code's PSInfo CO indicator at PSForm.cs:577-585
        // — NOT by ucInfoBar.PSInfo).
        if (m_correctionsApplied) {
            m_lblPs->setText(m_useSmallFonts ? tr("Correct")
                                             : tr("Correcting"));
            applyBackground(m_lblPs, kLime());
        } else {
            m_lblPs->setText(tr("Pure Signal2"));
            applyBackground(m_lblPs, kSeaGreen());
        }

        applyBackground(m_lblFb, computeFeedbackColour());

        if (m_hideFeedback || !m_calChangedSinceLastDraw) {
            m_lblFb->setText(m_useSmallFonts ? tr("FB") : tr("Feedback"));
        } else {
            m_lblFb->setText(QString::number(m_feedbackLevel));
        }
    } else {
        applyBackground(m_lblFb, kSeaGreen());
        applyBackground(m_lblPs, kSeaGreen());
        m_lblFb->setText(m_useSmallFonts ? tr("FB") : tr("Feedback"));
        m_lblPs->setText(tr("Pure Signal2"));
    }
}

QColor PsaIndicatorWidget::computeFeedbackColour() const
{
    // From Thetis PSForm.cs:1123-1138 FeedbackColourLevel [v2.10.3.13]:
    //   if (FeedbackLevel > 181) {
    //       if (_bInvertRedBlue) return Color.Red;
    //       return Color.DodgerBlue;
    //   }
    //   else if (FeedbackLevel > 128) return Color.Lime;
    //   else if (FeedbackLevel > 90)  return Color.Yellow;
    //   else {
    //       if (_bInvertRedBlue) return Color.DodgerBlue;
    //       return Color.Red;
    //   }
    if (m_feedbackLevel > 181) {
        return m_invertRedBlue ? kRed() : kDodgerBlue();
    }
    if (m_feedbackLevel > 128) {
        return kLime();
    }
    if (m_feedbackLevel > 90) {
        return kYellow();
    }
    return m_invertRedBlue ? kDodgerBlue() : kRed();
}

void PsaIndicatorWidget::updateTooltip()
{
    if (!m_lblFb) {
        return;
    }
    // From Thetis ucInfoBar.cs:1081-1096 setToolTips() [v2.10.3.13]:
    //   string fb = "";
    //   if (!HideFeedback) fb = "Showing level, ";
    //   if (puresignal.InvertRedBlue)
    //       toolTip1.SetToolTip(lblFB,
    //         fb + "Blue 0-90, Yellow 91-128, Green 129-181, Red 182+");
    //   else
    //       toolTip1.SetToolTip(lblFB,
    //         fb + "Red 0-90, Yellow 91-128, Green 129-181, Blue 182+");
    QString prefix = m_hideFeedback ? QString() : tr("Showing level, ");
    QString legend = m_invertRedBlue
        ? tr("Blue 0-90, Yellow 91-128, Green 129-181, Red 182+")
        : tr("Red 0-90, Yellow 91-128, Green 129-181, Blue 182+");
    m_lblFb->setToolTip(prefix + legend);
}

void PsaIndicatorWidget::applyBackground(QLabel* label, const QColor& bg)
{
    if (!label) {
        return;
    }

    // Drive the background via QPalette so fbBackgroundColor() /
    // psBackgroundColor() can read it back without parsing a stylesheet
    // string.  Stylesheet still applied for the rounded "badge" look.
    QPalette p = label->palette();
    p.setColor(QPalette::Window, bg);
    p.setColor(QPalette::WindowText, kFgText());
    label->setPalette(p);
    label->setAutoFillBackground(true);

    label->setStyleSheet(QStringLiteral(
        "QLabel { background-color: %1; color: #101010; "
        "padding: 1px 6px; border-radius: 3px; "
        "font-weight: bold; font-size: 11px; }")
        .arg(bg.name()));
}

} // namespace NereusSDR
