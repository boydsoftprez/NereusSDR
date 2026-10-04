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
//   2026-10-04 — Live Core status and passive numeric correction readouts,
//                 by J.J. Boyd (KG4VCF), with OpenAI Codex assistance.

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
#include "core/AppSettings.h"
#include "core/session/PureSignalSessionFacade.h"
#include "models/PureSignalSettings.h"
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
    m_lblPs->setText(tr("PureSignal 3"));
    hbox->addWidget(m_lblPs);

    m_lblCorrPeak = new QLabel(tr("Corr @peak: —"), this);
    m_lblCorrPeak->setObjectName(QStringLiteral("lblCorrPeak"));
    m_lblCorrPeak->setAlignment(Qt::AlignCenter);
    applyBackground(m_lblCorrPeak, kDimGray());
    hbox->addWidget(m_lblCorrPeak);

    setLayout(hbox);

    wireToModel();
    updateDisplay();
    updateTooltip();

    // The session facade follows coordinator lifetime and remote status.
    // It also clears readings on disconnect, without duplicate late bindings.
}

PsaIndicatorWidget::~PsaIndicatorWidget() = default;

// ── Wiring ────────────────────────────────────────────────────────────────

void PsaIndicatorWidget::wireToModel()
{
    if (!m_radioModel) {
        return;
    }
    PureSignalSessionFacade* facade = m_radioModel->pureSignalFacade();
    const auto refresh = [this, facade]() {
        const Ps3StatusSnapshot status = facade->statusSnapshot();
        const bool available = facade->available();
        setPsEnabled(available && (status.psEnabled || status.correctionsApplied
            || m_radioModel->pureSignalSettings()->autoCalEnabled()));
        setMox(available && status.mox);
        setInvertRedBlue(AppSettings::instance().value("InvertRedBluePsa", "False") == "True");
        setHideFeedback(AppSettings::instance().value("HideFeedbackLevel", "False") == "True");
        psInfo(status.feedbackLevel, status.feedbackLevel > 128 && status.feedbackLevel <= 181,
               status.correctionsApplied, false, computeFeedbackColour());
        const bool measured = available && status.correctionsApplied
            && status.correctionSummaryValid;
        m_lblCorrPeak->setText(measured
            ? tr("Corr @peak: %1x").arg(status.correctionGainAtPeak, 0, 'f', 4)
            : tr("Corr @peak: —"));
        applyBackground(m_lblCorrPeak, measured ? kSeaGreen() : kDimGray());
        m_lblCorrPeak->setToolTip(measured
            ? tr("AmpView correction gain at the full-scale envelope endpoint. "
                 "Phase correction span: %1°.")
                .arg(status.correctionPhaseSpanDegrees, 0, 'f', 2)
            : tr("Correction gain is unavailable until the Core has an applied "
                 "correction and a measured AmpView curve."));
    };
    connect(facade, &PureSignalSessionFacade::statusChanged, this, refresh);
    connect(m_radioModel->pureSignalSettings(), &PureSignalSettings::autoCalEnabledChanged,
            this, refresh);
    refresh();
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
        m_hasFeedbackReading = false;
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
        m_hasFeedbackReading = false;
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
    // Direct numeric setter refreshes color; psInfo marks a fresh reading.
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
    // NereusSDR live status intentionally differs from Thetis's attempt
    // pulse (ucInfoBar.cs:808-825 [v2.10.3.15]): a collection stall must
    // still update feedback and applied-correction status on every report.
    Q_UNUSED(feedbackLevelOk);
    Q_UNUSED(feedbackColour);
    Q_UNUSED(calibrationAttemptsChanged);
    m_feedbackLevel = level;
    m_correctionsApplied = correctionsApplied;
    m_hasFeedbackReading = m_mox;
    updateDisplay();
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
    // NereusSDR operator request: a feedback readout must never mutate
    // the color legend or visibility preference when clicked.
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
        m_lblPs->setText(tr("PureSignal 3"));
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
            m_lblPs->setText(tr("PureSignal 3"));
            applyBackground(m_lblPs, kSeaGreen());
        }

        applyBackground(m_lblFb, computeFeedbackColour());

        // Operator-requested passive numeric status supersedes the legacy
        // click-to-hide preference. FeedbackLevel is the raw WDSP reading.
        m_lblFb->setText(QString::number(m_feedbackLevel));
    } else {
        applyBackground(m_lblFb, kSeaGreen());
        applyBackground(m_lblPs, kSeaGreen());
        m_lblFb->setText(m_useSmallFonts ? tr("FB") : tr("Feedback"));
        m_lblPs->setText(tr("PureSignal 3"));
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
