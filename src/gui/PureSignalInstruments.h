// =================================================================
// src/gui/PureSignalInstruments.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/PSForm.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-13 — Sourced feedback ranges and original Qt instruments;
//                 J.J. Boyd (KG4VCF), assisted by OpenAI Codex.
// =================================================================

/*  PSForm.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2000-2025 Original authors
Copyright (C) 2020-2026 Richard Samphire MW0LGE

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
#include "gui/PureSignalViewStyle.h"
#include "core/dsp/Ps3Snapshot.h"
#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <algorithm>
#include <array>
namespace NereusSDR {
// Thetis range boundaries are facts; the meter and caption design are native Qt.
// From Thetis PSForm.cs:1123-1138 [v2.10.3.15] — FeedbackColourLevel boundaries.
inline constexpr std::array<int, 3> kPsFeedbackBoundaries{90, 128, 181};
// From Thetis PSForm.cs:1113-1115 [v2.10.3.15] — visual endpoint only;
// vendored WDSP calcc.c:1280 computes 256.0 * ratio without a raw clamp.
inline constexpr int kPsFeedbackVisualMaximum = 256;
inline int psFeedbackClass(int level)
{
    return static_cast<int>(std::lower_bound(kPsFeedbackBoundaries.begin(),
        kPsFeedbackBoundaries.end(), level) - kPsFeedbackBoundaries.begin());
}
inline QString psFeedbackCaption(int level)
{
    const std::array<QString, 4> captions{QStringLiteral("Too low"),
        QStringLiteral("Low / marginal"), QStringLiteral("In range"), QStringLiteral("High")};
    return captions[psFeedbackClass(level)];
}
inline QColor psFeedbackColour(int level)
{
    const std::array<QColor, 4> colours{
        WorkspacePreview::statusColour(QColor("#ff6060"), "error"),
        WorkspacePreview::statusColour(QColor("#ffd24a"), "warning"),
        WorkspacePreview::statusColour(QColor("#20c060"), "active"),
        WorkspacePreview::statusColour(QColor("#ffd24a"), "warning")};
    return colours[psFeedbackClass(level)];
}
inline QString psEngineCaption(int state)
{
    // Control-state facts: vendored WDSP 2.10 calcc.c:2115-2127 [@b02d5bac].
    const QStringList states{QStringLiteral("Reset"), QStringLiteral("Wait"),
        QStringLiteral("MOX delay"), QStringLiteral("Setup"), QStringLiteral("Collect"),
        QStringLiteral("MOX check"), QStringLiteral("Calculate"), QStringLiteral("Delay"),
        QStringLiteral("Stay on"), QStringLiteral("Turn on")};
    return state >= 0 && state < states.size() ? states[state] : QStringLiteral("Unknown");
}
class PureSignalFeedbackMeter final : public QWidget {
public:
    explicit PureSignalFeedbackMeter(QWidget* parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(38);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setToolTip(QStringLiteral("Latest raw calibration feedback level (info[4]). Visual scale: 0–256; readings above 256 remain visible numerically. In range: 129–181. May be retained between collections. "
            "This is a feedback range indicator, not distortion improvement."));
    }
    void setLevel(int level, bool available) { m_level = level; m_available = available; update(); }
    int level() const { return m_level; }
    bool available() const { return m_available; }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QFont textFont = PureSignalViewStyle::font(font());
        textFont.setPixelSize(11);
        painter.setFont(textFont);
        const QRectF bar(3, 4, width() - 6, 12);
        const auto x = [&bar](double value) { return bar.left() + value / kPsFeedbackVisualMaximum * bar.width(); };
        painter.setPen(Qt::NoPen);
        painter.setBrush(PureSignalViewStyle::border());
        painter.drawRoundedRect(bar, 3, 3);
        painter.setBrush(QColor(psFeedbackColour(150).red(), psFeedbackColour(150).green(),
            psFeedbackColour(150).blue(), 65));
        painter.drawRect(QRectF(x(kPsFeedbackBoundaries[1] + 1), bar.top(),
            x(kPsFeedbackBoundaries[2]) - x(kPsFeedbackBoundaries[1] + 1), bar.height()));
        if (m_available) {
            painter.setBrush(WorkspacePreview::roleColour(QColor("#7098bb"), "accent"));
            painter.drawRoundedRect(QRectF(bar.left(), bar.top() + 4,
                x(std::clamp(m_level, 0, kPsFeedbackVisualMaximum)) - bar.left(), 4), 2, 2);
            painter.setPen(QPen(PureSignalViewStyle::text(), 2));
            painter.drawLine(QPointF(x(std::clamp(m_level, 0, kPsFeedbackVisualMaximum)), 1),
                QPointF(x(std::clamp(m_level, 0, kPsFeedbackVisualMaximum)), 19));
        }
        painter.setPen(PureSignalViewStyle::muted());
        const std::array<int, 5> ticks{0, kPsFeedbackBoundaries[0] + 1,
            kPsFeedbackBoundaries[1] + 1, kPsFeedbackBoundaries[2] + 1, kPsFeedbackVisualMaximum};
        for (int tick : ticks) {
            const QString text = tick == kPsFeedbackVisualMaximum
                ? QStringLiteral("256+") : QString::number(tick);
            const int textWidth = painter.fontMetrics().horizontalAdvance(text);
            const qreal left = std::clamp(x(tick) - textWidth / 2.0, 0.0,
                static_cast<double>(width() - textWidth));
            painter.drawText(QRectF(left, 23, textWidth + 1, 14), Qt::AlignLeft, text);
        }
    }
private:
    int m_level{0};
    bool m_available{false};
};
// GUI-only projection of the authoritative snapshot. No actions, polling or DSP.
class PureSignalInstruments final : public QWidget {
public:
    PureSignalInstruments(const QString& prefix, bool compact, QWidget* parent = nullptr)
        : QWidget(parent), m_compact(compact)
    {
        setObjectName(prefix + QStringLiteral("Instruments"));
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(compact ? 0 : 16, compact ? 0 : 16, compact ? 0 : 16, compact ? 0 : 16);
        root->setSpacing(compact ? 7 : 12);
        auto label = [this, &prefix](const QString& suffix, int px, bool strong = false) {
            auto* item = new QLabel(this);
            item->setObjectName(prefix + suffix);
            QFont textFont = PureSignalViewStyle::font(font());
            textFont.setPixelSize(px);
            textFont.setWeight(strong ? QFont::Medium : QFont::Normal);
            item->setFont(textFont);
            item->setWordWrap(true);
            return item;
        };
        auto* correctionRow = new QHBoxLayout;
        correctionRow->addWidget(new QLabel(QStringLiteral("CORRECTION"), this));
        m_correction = label("CorrectionState", compact ? 14 : 24, true);
        correctionRow->addWidget(m_correction, 1, Qt::AlignRight);
        if (compact) { root->addLayout(correctionRow); }
        auto* feedbackRow = new QHBoxLayout;
        feedbackRow->addWidget(new QLabel(QStringLiteral("CALIBRATION FEEDBACK"), this));
        m_classification = label("FeedbackClass", 12);
        feedbackRow->addWidget(m_classification, 1, Qt::AlignRight);
        root->addLayout(feedbackRow);
        m_feedback = label("FeedbackLevel", compact ? 26 : 40, true);
        root->addWidget(m_feedback);
        m_meter = new PureSignalFeedbackMeter(this);
        m_meter->setObjectName(prefix + QStringLiteral("FeedbackMeter"));
        root->addWidget(m_meter);
        if (!compact) {
            root->addSpacing(12);
            root->addLayout(correctionRow);
        }
        m_quality = label("FeedbackPredicate", 11);
        m_quality->setToolTip(QStringLiteral("The existing isCorrecting getter means feedback >90. "
            "It does not identify an engine calculation stage."));
        root->addWidget(m_quality);
        m_activity = label("Activity", 12);
        root->addWidget(m_activity);
        m_successes = label("SuccessfulCalibrations", 12);
        m_attempts = label("CalibrationAttempts", 12);
        root->addWidget(m_successes);
        root->addWidget(m_attempts);
        if (!compact) {
            m_legacyInfo13 = label("LegacyInfo13", 12);
            root->addWidget(m_legacyInfo13);
            root->addStretch();
        }
        setStyleSheet(QStringLiteral("QWidget#%1 { background: %2; border: 1px solid %3; border-radius: 6px; } QLabel { color: %4; background: transparent; border: none; }")
            .arg(objectName(), PureSignalViewStyle::background(this).name(), PureSignalViewStyle::border().name(), PureSignalViewStyle::text().name()));
        setStatus({}, false);
    }
    void setStatus(const Ps3StatusSnapshot& status, bool available)
    {
        // From Thetis PSForm.cs:1100-1108 [v2.10.3.15] — applied and feedback predicates are distinct.
        const bool applied = available && status.correctionsApplied;
        const bool correcting = available && status.feedbackLevel > kPsFeedbackBoundaries[0];
        m_correction->setText(!available ? QStringLiteral("Unavailable")
            : applied ? QStringLiteral("● Applied") : QStringLiteral("Off"));
        const QString correctionStyle = QStringLiteral("color: %1;").arg(
            applied ? (correcting ? psFeedbackColour(150) : psFeedbackColour(100)).name()
                    : PureSignalViewStyle::muted().name());
        if (m_correction->styleSheet() != correctionStyle) {
            m_correction->setStyleSheet(correctionStyle);
        }
        const int feedbackSize = m_compact ? 26 : 40;
        m_feedback->setText(available
            ? QStringLiteral("<span style='font-size:%1px;font-weight:600'>%2</span> "
                "<span style='font-size:11px'>raw</span>")
                .arg(feedbackSize).arg(status.feedbackLevel)
            : QStringLiteral("–"));
        const QString feedbackStyle = QStringLiteral("color: %1;").arg(
            WorkspacePreview::roleColour(QColor("#ffd24a"), "measured").name());
        if (m_feedback->styleSheet() != feedbackStyle) {
            m_feedback->setStyleSheet(feedbackStyle);
        }
        m_classification->setText(available ? psFeedbackCaption(status.feedbackLevel) : QStringLiteral("No radio"));
        const QString classificationStyle = QStringLiteral("color: %1;").arg(available
            ? psFeedbackColour(status.feedbackLevel).name() : PureSignalViewStyle::muted().name());
        if (m_classification->styleSheet() != classificationStyle) {
            m_classification->setStyleSheet(classificationStyle);
        }
        m_meter->setLevel(status.feedbackLevel, available);
        m_quality->setText(!available ? QStringLiteral("No current feedback status") : correcting
            ? QStringLiteral("Feedback above correction threshold (>90)") : QStringLiteral("Feedback at or below correction threshold (≤90)"));
        // Engine stage is calcc state; the separate correctionBusy IQC update
        // flag remains in PsForm's raw/native diagnostics (PureSignal.cpp:1260;
        // vendored WDSP iqc.c:551-560 [@b02d5bac]).
        m_activity->setText(available ? QStringLiteral("Calibration stage: %1 (%2)")
            .arg(psEngineCaption(status.engineState)).arg(status.engineState)
            : QStringLiteral("Calibration stage: –"));
        m_successes->setText(QStringLiteral("Successful calibrations  %1").arg(available ? QString::number(status.successfulCalibrations) : QStringLiteral("–")));
        m_attempts->setText(QStringLiteral("Calibration attempts  %1").arg(available ? QString::number(status.attemptedCalibrations) : QStringLiteral("–")));
        if (m_legacyInfo13) { m_legacyInfo13->setText(QStringLiteral("Legacy info[13]  %1").arg(available ? QString::number(status.dogCount) : QStringLiteral("–"))); }
    }
private:
    bool m_compact{false};
    QLabel* m_correction;
    QLabel* m_classification;
    QLabel* m_feedback;
    QLabel* m_quality;
    QLabel* m_activity;
    QLabel* m_successes;
    QLabel* m_attempts;
    QLabel* m_legacyInfo13{nullptr};
    PureSignalFeedbackMeter* m_meter;
};
} // namespace NereusSDR
