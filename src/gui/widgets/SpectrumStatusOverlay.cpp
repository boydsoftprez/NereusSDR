// no-port-check: NereusSDR-original. No upstream port. See header.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/widgets/SpectrumStatusOverlay.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. See header for full
// Modification history (NereusSDR).
// =================================================================

#include "gui/widgets/SpectrumStatusOverlay.h"
#include "gui/StyleConstants.h"

#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>

namespace NereusSDR {

namespace {

constexpr int kOverlayHeight  = 22;
constexpr int kBadgeSize      = 16;
constexpr int kLeftMargin     = 4;
constexpr int kBadgeGap       = 6;   // gap between slice badge and freq text
constexpr int kFreqWidth      = 180;
constexpr int kChTagWidth     = 36;
constexpr int kPillWidth      = 60;
constexpr int kInterPillGap   = 4;
constexpr int kRightPad       = 4;

} // namespace

SpectrumStatusOverlay::SpectrumStatusOverlay(QWidget* parent) : QWidget(parent)
{
    setFixedHeight(kOverlayHeight);
    // Default minimum width covers: left pad + badge + gap + freq text + ch tag + right pad.
    setMinimumWidth(kLeftMargin + kBadgeSize + kBadgeGap + kFreqWidth + kChTagWidth + kRightPad);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
}

SpectrumStatusOverlay::~SpectrumStatusOverlay() = default;

void SpectrumStatusOverlay::setSliceLetter(QChar letter)
{
    if (m_sliceLetter == letter) { return; }
    m_sliceLetter = letter;
    update();
}

void SpectrumStatusOverlay::setFrequencyHz(qint64 hz)
{
    if (m_frequencyHz == hz) { return; }
    m_frequencyHz = hz;
    update();
}

void SpectrumStatusOverlay::setMode(const QString& mode)
{
    if (m_mode == mode) { return; }
    m_mode = mode;
    update();
}

void SpectrumStatusOverlay::setChainIndex(int idx)
{
    if (m_chainIndex == idx) { return; }
    m_chainIndex = idx;
    update();
}

void SpectrumStatusOverlay::setTxBound(bool tx)
{
    if (m_txBound == tx) { return; }
    m_txBound = tx;
    update();
}

void SpectrumStatusOverlay::setWideBpf(bool wide, const QString& reason)
{
    if (m_wideBpf == wide && m_wideReason == reason) { return; }
    m_wideBpf = wide;
    m_wideReason = reason;
    update();
}

void SpectrumStatusOverlay::setDiversityActive(bool div)
{
    if (m_diversityActive == div) { return; }
    m_diversityActive = div;
    update();
}

void SpectrumStatusOverlay::setPsPaused(bool paused)
{
    if (m_psPaused == paused) { return; }
    m_psPaused = paused;
    update();
}

void SpectrumStatusOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background panel: rgba(20, 30, 45, 240) with subtle border.
    p.setBrush(QColor(20, 30, 45, 240));
    p.setPen(QColor(Style::kBorderSubtle));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 3, 3);

    int x = kLeftMargin;
    const int y = (height() - kBadgeSize) / 2;

    // Slice letter badge.
    QColor sliceColor;
    switch (m_sliceLetter.toLatin1()) {
        case 'A': sliceColor = QColor(0x00, 0xd4, 0xff); break;
        case 'B': sliceColor = QColor(0xff, 0x40, 0xff); break;
        case 'C': sliceColor = QColor(0x40, 0xff, 0x40); break;
        case 'D': sliceColor = QColor(0xff, 0xff, 0x00); break;
        default:  sliceColor = QColor(0x00, 0xd4, 0xff); break;
    }
    p.setBrush(sliceColor);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(x, y, kBadgeSize, kBadgeSize, 2, 2);
    p.setPen(Qt::black);
    p.setFont(QFont(QStringLiteral("monospace"), 10, QFont::Bold));
    p.drawText(QRect(x, y, kBadgeSize, kBadgeSize), Qt::AlignCenter,
               QString(m_sliceLetter));
    x += kBadgeSize + kBadgeGap;

    // Freq + mode text. Format: "MHz.kkk MODE" (e.g. "14.225 USB").
    p.setPen(QColor(Style::kTextPrimary));
    p.setFont(QFont(QStringLiteral("monospace"), 10, QFont::Bold));
    const QString freqText = QStringLiteral("%1.%2 %3")
        .arg(m_frequencyHz / 1000000)
        .arg((m_frequencyHz / 1000) % 1000, 3, 10, QChar('0'))
        .arg(m_mode);
    p.drawText(x, y, kFreqWidth, kBadgeSize, Qt::AlignVCenter, freqText);
    x += kFreqWidth;

    // CH tag (always visible).
    p.setBrush(QColor(0x1a, 0x2a, 0x3a));
    p.setPen(QColor(0x30, 0x40, 0x50));
    p.drawRoundedRect(x, y + 1, kChTagWidth, 14, 2, 2);
    p.setPen(QColor(Style::kTitleText));
    p.setFont(QFont(QStringLiteral("monospace"), 9, QFont::Bold));
    p.drawText(QRect(x, y + 1, kChTagWidth, 14), Qt::AlignCenter,
               QStringLiteral("CH %1").arg(m_chainIndex));
    x += kChTagWidth + kInterPillGap;

    // Optional pills: TX, WIDE, DIV, PS HOLD.
    auto drawPill = [&](const QString& text, const QColor& bg,
                        const QColor& fg, const QColor& border) {
        p.setBrush(bg);
        p.setPen(border);
        p.drawRoundedRect(x, y + 1, kPillWidth, 14, 2, 2);
        p.setPen(fg);
        p.drawText(QRect(x, y + 1, kPillWidth, 14), Qt::AlignCenter, text);
        x += kPillWidth + kInterPillGap;
    };

    if (m_txBound) {
        drawPill(QStringLiteral("TX"),
                 QColor(0xcc, 0x22, 0x22), QColor(Qt::white),
                 QColor(0xff, 0x44, 0x44));
    }
    if (m_wideBpf) {
        drawPill(QStringLiteral("WIDE"),
                 QColor(0x60, 0x40, 0x00), QColor(0xff, 0xb8, 0x00),
                 QColor(0x90, 0x60, 0x00));
    }
    if (m_diversityActive) {
        drawPill(QStringLiteral("DIV"),
                 QColor(0x00, 0x60, 0x40), QColor(0x00, 0xff, 0x88),
                 QColor(0x00, 0xa0, 0x60));
    }
    if (m_psPaused) {
        drawPill(QStringLiteral("PS HOLD"),
                 QColor(0x60, 0x40, 0x00), QColor(0xff, 0xb8, 0x00),
                 QColor(0x90, 0x60, 0x00));
    }

    setMinimumWidth(x + kRightPad);
}

void SpectrumStatusOverlay::mousePressEvent(QMouseEvent* event)
{
    // Hit-test the badges. Layout mirrors paintEvent.
    const int clickX = event->pos().x();

    // CH tag starts after: left margin + badge + gap + freq width.
    int hitX = kLeftMargin + kBadgeSize + kBadgeGap + kFreqWidth;
    if (clickX >= hitX && clickX < hitX + kChTagWidth) {
        emit chainTagClicked(m_chainIndex);
        return;
    }
    hitX += kChTagWidth + kInterPillGap;

    // Optional pills in paint order: TX, WIDE, DIV, PS HOLD.
    if (m_txBound) {
        if (clickX >= hitX && clickX < hitX + kPillWidth) {
            emit txBadgeClicked();
            return;
        }
        hitX += kPillWidth + kInterPillGap;
    }
    if (m_wideBpf) {
        if (clickX >= hitX && clickX < hitX + kPillWidth) {
            emit wideBadgeClicked();
            return;
        }
        hitX += kPillWidth + kInterPillGap;
    }
    // DIV / PS HOLD currently informational; no click handlers wired.
    QWidget::mousePressEvent(event);
}

} // namespace NereusSDR
