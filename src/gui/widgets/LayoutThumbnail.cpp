// no-port-check: AetherSDR-derived NereusSDR file. Painted pan-layout
// preview tile, structurally from AetherSDR PanLayoutDialog.cpp
// LayoutThumbnail [@c6481cb]. Registered in
// docs/attribution/aethersdr-contributor-index.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/widgets/LayoutThumbnail.cpp  (NereusSDR)
// =================================================================
//
// Ported (structurally) from AetherSDR PanLayoutDialog.cpp [@c6481cb].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// AetherSDR carries no per-file headers, so this is a project-level
// citation per docs/attribution/HOW-TO-PORT.md rule 6.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02  J.J. Boyd / KG4VCF  Bottom-banner + pan-menu epic.
//                                    Nine layouts rather than
//                                    AetherSDR's twelve; see design
//                                    §8.3. AI-assisted transformation
//                                    via Anthropic Claude Code.
// =================================================================
#include "gui/widgets/LayoutThumbnail.h"

#include <QFont>
#include <QPainter>

namespace NereusSDR {

// Nine layouts, Single first. AetherSDR orders Single last; NereusSDR leads
// with it because it is where an operator starts and returns.
const QVector<PanLayoutGeometry> kPanLayouts = {
    {QStringLiteral("1"),   QStringLiteral("Single"),        1, {1}},
    {QStringLiteral("2v"),  QStringLiteral("A / B"),         2, {1, 1}},
    {QStringLiteral("2h"),  QStringLiteral("A | B"),         2, {2}},
    {QStringLiteral("12h"), QStringLiteral("A / B|C"),       3, {1, 2}},
    {QStringLiteral("2h1"), QStringLiteral("A|B / C"),       3, {2, 1}},
    {QStringLiteral("3v"),  QStringLiteral("A / B / C"),     3, {1, 1, 1}},
    {QStringLiteral("2x2"), QStringLiteral("A|B / C|D"),     4, {2, 2}},
    {QStringLiteral("4v"),  QStringLiteral("A/B/C/D"),       4, {1, 1, 1, 1}},
    {QStringLiteral("3h2"), QStringLiteral("A|B|C / D|E"),   5, {3, 2}},
};

namespace {
// From AetherSDR PanLayoutDialog.cpp:33,43,53-56 [@c6481cb].
constexpr int kThumbW = 120;
constexpr int kThumbH = 90;
constexpr int kPad    = 4;
constexpr int kGap    = 3;
constexpr char kLetters[] = "ABCDEFGH";
} // namespace

LayoutThumbnail::LayoutThumbnail(const PanLayoutGeometry& layout,
                                 bool isCurrent, bool enabled,
                                 QWidget* parent)
    : QWidget(parent), m_layout(layout), m_current(isCurrent),
      m_enabled(enabled)
{
    setFixedSize(kThumbW, kThumbH);
    setCursor(enabled ? Qt::PointingHandCursor : Qt::ForbiddenCursor);
}

QVector<QRect> LayoutThumbnail::cellRects() const
{
    QVector<QRect> out;
    const int w = width()  - kPad * 2;
    const int h = height() - kPad * 2;
    const int totalRows = m_layout.rows.size();
    if (totalRows == 0) {
        return out;
    }
    const int rowH = (h - kGap * (totalRows - 1)) / totalRows;

    for (int r = 0; r < totalRows; ++r) {
        const int cols = m_layout.rows[r];
        if (cols <= 0) {
            continue;
        }
        const int colW = (w - kGap * (cols - 1)) / cols;
        const int y    = kPad + r * (rowH + kGap);
        for (int c = 0; c < cols; ++c) {
            out.append(QRect(kPad + c * (colW + kGap), y, colW, rowH));
        }
    }
    return out;
}

void LayoutThumbnail::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QColor bg = m_current ? QColor(0x00, 0x60, 0x7a) : QColor(0x1a, 0x2a, 0x3a);
    if (!m_enabled) { bg = QColor(0x10, 0x10, 0x18); }
    p.fillRect(rect(), bg);

    QColor border = m_current ? QColor(0x00, 0xb4, 0xd8) : QColor(0x30, 0x40, 0x50);
    if (!m_enabled) { border = QColor(0x20, 0x20, 0x30); }
    p.setPen(QPen(border, m_current ? 2 : 1));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);

    const QColor cellColor = m_enabled ? QColor(0x2a, 0x5a, 0x8a)
                                       : QColor(0x1a, 0x1a, 0x2a);
    const QColor textColor = m_enabled ? QColor(0xc8, 0xd8, 0xe8)
                                       : QColor(0x40, 0x40, 0x50);

    const QVector<QRect> cells = cellRects();
    for (int i = 0; i < cells.size(); ++i) {
        p.setPen(Qt::NoPen);
        p.setBrush(cellColor);
        p.drawRoundedRect(cells[i], 3, 3);

        if (i < static_cast<int>(sizeof(kLetters) - 1)) {
            p.setPen(textColor);
            p.setFont(QFont(QStringLiteral("sans-serif"), 14, QFont::Bold));
            p.drawText(cells[i], Qt::AlignCenter, QString(QLatin1Char(kLetters[i])));
        }
    }
}

} // namespace NereusSDR
