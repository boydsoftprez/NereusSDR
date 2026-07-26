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
constexpr int kChTagWidth     = 36;
constexpr int kPillWidth      = 60;
constexpr int kInterPillGap   = 4;
constexpr int kRightPad       = 4;

} // namespace

SpectrumStatusOverlay::SpectrumStatusOverlay(QWidget* parent) : QWidget(parent)
{
    setFixedHeight(kOverlayHeight);
    // Default minimum width covers the no-pill case: left pad + ch tag + right
    // pad. The slice badge and freq/mode text are no longer painted, so
    // reserving their width here would leave a wide empty strip on every pan.
    setMinimumWidth(kLeftMargin + kChTagWidth + kRightPad);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
}

SpectrumStatusOverlay::~SpectrumStatusOverlay() = default;

QSize SpectrumStatusOverlay::sizeHint() const
{
    // Computed from the pill FLAGS, not read back from minimumWidth().
    //
    // minimumWidth() is only updated at the end of paintEvent, so a hint that
    // read it was always one repaint stale: lighting the TX pill set the flag,
    // the parent re-anchored using the OLD narrower width, and only then did
    // paintEvent grow the minimum -- at which point setGeometry's clamp
    // expanded the widget rightward from its fixed x and walked it straight
    // back under the dBm range arrows. Deriving from the flags means the hint
    // is right the instant a pill changes, before anything paints.
    //
    // Mirrors paintEvent's and badgeRect's layout; all three must agree.
    int w = kLeftMargin + kChTagWidth;
    const int lit = (m_txBound ? 1 : 0) + (m_wideBpf ? 1 : 0)
                  + (m_diversityActive ? 1 : 0) + (m_psPaused ? 1 : 0);
    w += lit * (kInterPillGap + kPillWidth);
    return QSize(w + kRightPad, kOverlayHeight);
}

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
    // The reason was stored and never read: AlexAdcState::reasonText is
    // documented "for WIDE badge tooltip" (AlexController.h:97) but nothing
    // surfaced it, so the pill said WIDE and nothing said why. This overlay
    // carries no other tooltip, so hanging it on the widget is the whole
    // disambiguation surface: the badge states the RF fact (the preselector
    // is bypassed) and the tooltip names which of the causes produced it.
    setToolTip(wide ? reason : QString());
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

    // Slice letter, frequency and mode are deliberately NOT painted here.
    //
    // They restated the VFO flag sitting a few pixels away on the same pan:
    // flag and strip both showed "A", both showed 7.244, both showed LSB. The
    // strip's only unique content is the chain tag and the bypass / diversity
    // / PureSignal pills, so that is all it carries now. It also stops a pan
    // with no slice from asserting a meaningless "A 0.000 USB".
    //
    // The setters (setSliceLetter / setFrequencyHz / setMode) are kept: they
    // are cheap, PanadapterApplet already calls them, and the values are the
    // obvious source for a tooltip or a compact mode if either is wanted
    // later. Only the painting is gone.

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

    // NOT setMinimumWidth(x + kRightPad) any more. Growing the minimum here
    // let setGeometry's clamp expand the widget rightward from its fixed x
    // after the parent had already placed it, which is how the strip crept
    // back over the dBm range arrows. sizeHint() derives the same width from
    // the pill flags instead, so the parent can place it correctly up front.
}

QRect SpectrumStatusOverlay::badgeRect(Badge badge) const
{
    // Layout mirrors paintEvent. Only the horizontal extent is tested, so the
    // region spans the full height; that is the hit rule, stated rather than
    // narrowed. Kept as the single source of the hit geometry so
    // mousePressEvent and any caller reading a region back cannot drift.

    // CH tag is first now: the slice badge and freq/mode text they used to sit
    // behind are no longer painted (see paintEvent). This offset MUST track
    // paintEvent's `x` or every pill becomes unclickable or hits its neighbour.
    int hitX = kLeftMargin;
    if (badge == Badge::ChainTag) {
        return QRect(hitX, 0, kChTagWidth, height());
    }
    hitX += kChTagWidth + kInterPillGap;

    // Optional pills in paint order: TX, WIDE, DIV, PS HOLD. An unlit pill is
    // not painted and takes no width, so everything after it shifts left --
    // and the pill itself has no region at all.
    if (m_txBound) {
        if (badge == Badge::Tx) {
            return QRect(hitX, 0, kPillWidth, height());
        }
        hitX += kPillWidth + kInterPillGap;
    } else if (badge == Badge::Tx) {
        return QRect();
    }
    if (m_wideBpf) {
        if (badge == Badge::Wide) {
            return QRect(hitX, 0, kPillWidth, height());
        }
        hitX += kPillWidth + kInterPillGap;
    } else if (badge == Badge::Wide) {
        return QRect();
    }
    return QRect();
}

void SpectrumStatusOverlay::mousePressEvent(QMouseEvent* event)
{
    // Hit-test the badges through badgeRect so the regions that respond are
    // the regions callers can read back.
    const int clickX = event->pos().x();
    const auto hits = [clickX](const QRect& r) {
        return r.isValid() && clickX >= r.left() && clickX < r.left() + r.width();
    };

    if (hits(badgeRect(Badge::ChainTag))) {
        emit chainTagClicked(m_chainIndex);
        return;
    }
    if (hits(badgeRect(Badge::Tx))) {
        emit txBadgeClicked();
        return;
    }
    if (hits(badgeRect(Badge::Wide))) {
        emit wideBadgeClicked();
        return;
    }
    // DIV / PS HOLD currently informational; no click handlers wired.
    QWidget::mousePressEvent(event);
}

} // namespace NereusSDR
