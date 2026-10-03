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
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>
#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

constexpr int kOverlayHeight  = 22;
constexpr int kBadgeSize      = 16;
constexpr int kLeftMargin     = 4;
constexpr int kChTagWidth     = 36;
constexpr int kPillWidth      = 60;
constexpr int kInterPillGap   = 4;
constexpr int kRightPad       = 4;

// The remote display row's font. Not chosen per platform: each platform
// resolves "monospace" to its own face (DejaVu Sans Mono on Linux, a
// proportional face on macOS), so the row measures every form in the font it
// actually got rather than assuming a width per character.
QFont remoteStatusFont()
{
    return QFont(QStringLiteral("monospace"), 9);
}

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
    const int lit = (txPillLit() ? 1 : 0) + (m_wideBpf ? 1 : 0)
                  + (m_diversityActive ? 1 : 0) + (m_psPaused ? 1 : 0);
    w += lit * (kInterPillGap + kPillWidth);
    if (!m_remoteDisplayStatus.isEmpty()) {
        // Rounded up from the fractional advance, with the painter's metrics:
        // a row sized to the rounded-down integer advance elided every line
        // by its last character.
        // Sized for the longest form; a narrower row paints a shorter one.
        const int text = remoteStatusTextWidth(m_remoteDisplayStatus);
        w = std::max(w, std::min(360, text + kLeftMargin));
    }
    return QSize(w + kRightPad, kOverlayHeight * (m_remoteDisplayStatus.isEmpty() ? 1 : 2));
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

void SpectrumStatusOverlay::setTakeTransmitOffered(bool offered, const QString& holderName,
                                                   bool holderOnAir)
{
    if (m_takeOffered == offered && m_takeHolderName == holderName
        && m_takeHolderOnAir == holderOnAir) {
        return;
    }
    m_takeOffered = offered;
    m_takeHolderName = offered ? holderName : QString();
    m_takeHolderOnAir = offered && holderOnAir;
    updateStatusToolTip();
    updateGeometry();
    update();
}

QString SpectrumStatusOverlay::takeTransmitToolTip() const
{
    if (!m_takeOffered || m_txBound) {
        return {};
    }
    const QString name = m_takeHolderName.isEmpty() ? QStringLiteral("Another device")
                                                    : m_takeHolderName;
    return m_takeHolderOnAir
               ? QStringLiteral("%1 is on the air. Click TX to take transmit.").arg(name)
               : QStringLiteral("%1 has the transmitter. Click TX to take transmit.").arg(name);
}

void SpectrumStatusOverlay::setWideBpf(bool wide, const QString& reason)
{
    if (m_wideBpf == wide && m_wideReason == reason) { return; }
    m_wideBpf = wide;
    m_wideReason = reason;
    // The reason was stored and never read: AlexAdcState::reasonText is
    // documented "for WIDE badge tooltip" (AlexController.h:97) but nothing
    // surfaced it, so the pill said WIDE and nothing said why. This overlay
    // keeps that RF reason alongside any remote-display observation: the
    // badge states the preselector is bypassed, and the tooltip says why.
    updateStatusToolTip();
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

void SpectrumStatusOverlay::setRemoteDisplayStatus(const PanStatusText& status)
{
    QStringList forms;
    for (const QString& form : status.shortForms()) {
        const QString text = form.simplified().left(512);
        if (!text.isEmpty()) {
            forms.append(text);
        }
    }
    const QString text = forms.value(0);
    const QString explanation = status.explanation.trimmed().left(2048);
    if (m_remoteDisplayForms == forms && m_remoteDisplayExplanation == explanation) {
        return;
    }
    const bool rowChanged = m_remoteDisplayForms != forms;
    m_remoteDisplayStatus = text;
    m_remoteDisplayForms = forms;
    m_remoteDisplayExplanation = explanation;
    updateStatusToolTip();
    if (rowChanged) {
        setFixedHeight(kOverlayHeight * (text.isEmpty() ? 1 : 2));
        updateGeometry();
        update();
    }
}

QRect SpectrumStatusOverlay::remoteStatusRect() const
{
    return QRect(kLeftMargin, kOverlayHeight,
                 std::max(0, width() - kLeftMargin - kRightPad), kOverlayHeight);
}

QString SpectrumStatusOverlay::visibleRemoteDisplayStatus() const
{
    if (m_remoteDisplayForms.isEmpty()) { return {}; }
    // The longest form that fits the row, measured with the painter's font
    // on this widget's paint device and rounded up, as sizeHint rounds.
    // Never elided: a row too narrow for every form paints the shortest,
    // and the hover text still says it all.
    const int room = remoteStatusRowWidth();
    for (const QString& form : m_remoteDisplayForms) {
        if (remoteStatusTextWidth(form) <= room) {
            return form;
        }
    }
    return m_remoteDisplayForms.constLast();
}

int SpectrumStatusOverlay::remoteStatusRowWidth() const
{
    return remoteStatusRect().width();
}

int SpectrumStatusOverlay::remoteStatusTextWidth(const QString& text) const
{
    const QFontMetricsF metrics(remoteStatusFont(), this);
    return int(std::ceil(metrics.horizontalAdvance(text)));
}

void SpectrumStatusOverlay::updateStatusToolTip()
{
    // The hover text is the full explanation; the painted row is only its
    // short form.
    QString text = m_remoteDisplayExplanation;
    if (m_wideBpf && !m_wideReason.isEmpty()) {
        if (!text.isEmpty()) { text += QLatin1Char('\n'); }
        text += m_wideReason;
    }
    const QString take = takeTransmitToolTip();
    if (!take.isEmpty()) {
        if (!text.isEmpty()) { text += QLatin1Char('\n'); }
        text += take;
    }
    setToolTip(text);
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
    const int y = (kOverlayHeight - kBadgeSize) / 2;

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
    } else if (m_takeOffered) {
        // iPhone app plan Task 78: another device holds transmit; the pill
        // offers to take it. Outlined, not filled: this window is not on
        // the air. Red outline while the holder is.
        drawPill(QStringLiteral("TAKE TX"), QColor(0x1a, 0x2a, 0x3a),
                 m_takeHolderOnAir ? QColor(0xff, 0x80, 0x80) : QColor(0xc8, 0xd8, 0xe8),
                 m_takeHolderOnAir ? QColor(0xff, 0x44, 0x44) : QColor(0x60, 0x78, 0x90));
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

    if (!m_remoteDisplayForms.isEmpty()) {
        p.setFont(remoteStatusFont());
        p.setPen(QColor(Style::kTitleText));
        p.drawText(remoteStatusRect(), Qt::AlignLeft | Qt::AlignVCenter,
                   visibleRemoteDisplayStatus());
    }

    // NOT setMinimumWidth(x + kRightPad) any more. Growing the minimum here
    // let setGeometry's clamp expand the widget rightward from its fixed x
    // after the parent had already placed it, which is how the strip crept
    // back over the dBm range arrows. sizeHint() derives the same width from
    // the pill flags instead, so the parent can place it correctly up front.
}

QRect SpectrumStatusOverlay::badgeRect(Badge badge) const
{
    // Layout mirrors paintEvent. Only the first row contains command badges;
    // the optional remote-status row is observation only. Shared hit geometry
    // keeps mousePressEvent and callers reading a region back in agreement.

    // CH tag is first now: the slice badge and freq/mode text they used to sit
    // behind are no longer painted (see paintEvent). This offset MUST track
    // paintEvent's `x` or every pill becomes unclickable or hits its neighbour.
    int hitX = kLeftMargin;
    if (badge == Badge::ChainTag) {
        return QRect(hitX, 0, kChTagWidth, kOverlayHeight);
    }
    hitX += kChTagWidth + kInterPillGap;

    // Optional pills in paint order: TX, WIDE, DIV, PS HOLD. An unlit pill is
    // not painted and takes no width, so everything after it shifts left --
    // and the pill itself has no region at all.
    if (txPillLit()) {
        if (badge == Badge::Tx) {
            return QRect(hitX, 0, kPillWidth, kOverlayHeight);
        }
        hitX += kPillWidth + kInterPillGap;
    } else if (badge == Badge::Tx) {
        return QRect();
    }
    if (m_wideBpf) {
        if (badge == Badge::Wide) {
            return QRect(hitX, 0, kPillWidth, kOverlayHeight);
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
    const QPoint position = event->pos();
    const auto hits = [position](const QRect& r) {
        return r.isValid() && r.contains(position);
    };

    if (hits(badgeRect(Badge::ChainTag))) {
        emit chainTagClicked(m_chainIndex);
        return;
    }
    if (hits(badgeRect(Badge::Tx))) {
        // Task 78: the pill that offers a take asks for it; the lit pill
        // keeps its handoff meaning.
        if (!m_txBound && m_takeOffered) {
            emit takeTransmitClicked();
        } else {
            emit txBadgeClicked();
        }
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
