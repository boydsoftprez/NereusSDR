#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/recordings/DsrHoursMap.h  (NereusSDR)
// =================================================================
//
// The map of kept hours beside the waterfall's time strip while it is
// rewound: every hour the Digital Spectrum Recorder keeps, newest at the
// top, shaded by how busy the band was (dark quiet, bright busy), this
// window's own 20 minutes as a thin bracket on its left edge, hour marks,
// and a white mark where the waterfall's top row is. A click or a drag
// jumps there (jumpRequested). Design:
// docs/architecture/2026-11-15-recording-voice-keyer-dsr-design.md, section
// 3 (Moving through hours), 6.7, A12; mockup dsr-rewind/hours.html.
//
// SAVE (Task 25) puts a yellow bracket on it, the stretch to save: drag
// its top or bottom edge to move that end, or its middle to move it whole
// (bracketChanged). At least 30 seconds, inside what is kept.
//
// With an older Core or the recorder off it stays in place, dimmed and
// disabled, with the reason as its tooltip (SpectrumWidget also writes the
// reason on the waterfall).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 24.
//   2026-11-16 - J.J. Boyd (KG4VCF), recording plan Task 25: saving a
//                 stretch, sound only and opening a saved stretch.
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QList>
#include <QString>
#include <QWidget>

#include "core/recordings/dsr/DsrReplay.h"

namespace NereusSDR {

class DsrHoursMap : public QWidget {
    Q_OBJECT

public:
    // The mockup's map: 44 px wide, from 22 px below the waterfall's top.
    static constexpr int kWidth = 44;
    static constexpr int kTopGap = 22;

    explicit DsrHoursMap(QWidget* parent = nullptr);

    /// Every kept hour, newest first (dsr.hours).
    void setHours(const QList<DsrHour>& hours);
    /// The oldest kept moment and now, in ms since 1970 UTC.
    void setRange(qint64 oldestUtcMs, qint64 newestUtcMs);
    /// How far back this window's own waterfall reaches, in ms.
    void setOwnSpanMs(qint64 ms);
    /// The waterfall's top row: the white mark.
    void setPosition(qint64 utcMs);
    /// Empty: in use. Otherwise disabled and dimmed, with this reason.
    void setUnavailableReason(const QString& reason);
    QString unavailableReason() const { return m_reason; }

    qint64 utcMsAtY(int y) const;
    int yForUtcMs(qint64 utcMs) const;
    qint64 positionUtcMs() const { return m_positionMs; }
    int hourCount() const { return int(m_hours.size()); }

    // ---- The stretch to save (Task 25; mockup hours.html #bracket).
    // The mockup's shortest stretch and the grab band on each edge.
    static constexpr qint64 kMinBracketMs = 30000;
    static constexpr int kBracketGrabPx = 4;
    /// Shows the bracket from `fromUtcMs` to `toUtcMs`, kept inside the
    /// map's range.
    void setBracket(qint64 fromUtcMs, qint64 toUtcMs);
    void clearBracket();
    bool hasBracket() const { return m_hasBracket; }
    qint64 bracketFromUtcMs() const { return m_bracketFromMs; }
    qint64 bracketToUtcMs() const { return m_bracketToMs; }
    /// The bracket's rectangle on the map (empty when there is none).
    QRect bracketRect() const;

signals:
    void jumpRequested(qint64 utcMs);
    /// The bracket moved or changed length (also on setBracket).
    void bracketChanged(qint64 fromUtcMs, qint64 toUtcMs);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    static QColor heat(double activity);
    void jumpAt(int y);
    void dragBracketTo(int y);
    enum class BracketDrag { None, From, To, Whole };

    QList<DsrHour> m_hours;
    qint64 m_oldestMs = 0;
    qint64 m_newestMs = 0;
    qint64 m_ownSpanMs = 0;
    qint64 m_positionMs = 0;
    QString m_reason;
    bool m_dragging = false;
    bool m_hasBracket = false;
    qint64 m_bracketFromMs = 0;
    qint64 m_bracketToMs = 0;
    BracketDrag m_bracketDrag = BracketDrag::None;
    int m_bracketDragY = 0;
    qint64 m_bracketDragFromMs = 0;
    qint64 m_bracketDragToMs = 0;
};

} // namespace NereusSDR
