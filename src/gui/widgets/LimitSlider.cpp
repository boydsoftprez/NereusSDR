// =================================================================
// src/gui/widgets/LimitSlider.cpp  (NereusSDR)
// =================================================================
//
// Thetis PrettyTrackBar limit bar/interaction, ported to QSlider.  The
// geometry follows Thetis: the usable travel runs from half a handle inside
// the groove at each end, and the bar spans from the limit to the top end.
//
// =================================================================

#include "LimitSlider.h"

#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QToolTip>

#include <algorithm>
#include <utility>

namespace NereusSDR {

namespace {

// The horizontal travel of the groove, inset by half the handle, as Thetis
// computes it (width - headWidth - padding).
void travelFor(const QSlider& slider, int* low, int* high)
{
    QStyleOptionSlider opt;
    opt.initFrom(&slider);
    opt.orientation = slider.orientation();
    opt.maximum = slider.maximum();
    opt.minimum = slider.minimum();
    opt.sliderPosition = slider.sliderPosition();
    opt.sliderValue = slider.value();
    opt.upsideDown = slider.invertedAppearance();
    opt.tickPosition = slider.tickPosition();
    opt.tickInterval = slider.tickInterval();

    const QRect groove = slider.style()->subControlRect(
        QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, &slider);
    const QRect handle = slider.style()->subControlRect(
        QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, &slider);
    if (!groove.isValid() || !handle.isValid()) {
        // A stylesheet can hide the standard sub-controls; fall back to the
        // widget's own width inset by a handle radius.
        *low = 6;
        *high = std::max(*low + 1, slider.width() - 6);
        return;
    }
    if (slider.orientation() == Qt::Horizontal) {
        *low = groove.left() + handle.width() / 2;
        *high = groove.right() - handle.width() / 2;
    } else {
        *low = groove.top() + handle.height() / 2;
        *high = groove.bottom() - handle.height() / 2;
    }
    if (*high < *low) { std::swap(*low, *high); }
}

}  // namespace

LimitSlider::LimitSlider(Qt::Orientation orientation, QWidget* parent)
    : QSlider(orientation, parent)
{
    // Right-drag sets the limit, so no context menu competes for the button.
    setContextMenuPolicy(Qt::PreventContextMenu);
}

void LimitSlider::setLimitValue(int value)
{
    const int clamped = std::clamp(value, minimum(), maximum());
    if (clamped == m_limitValue) { return; }
    m_limitValue = clamped;
    update();
}

void LimitSlider::setLimitEnabled(bool enabled)
{
    if (enabled == m_limitEnabled) { return; }
    m_limitEnabled = enabled;
    update();
}

void LimitSlider::setLimitBarColor(const QColor& color)
{
    if (color == m_limitBarColor) { return; }
    m_limitBarColor = color;
    update();
}

void LimitSlider::setLimitTextFormatter(std::function<QString(int)> formatter)
{
    m_limitTextFormatter = std::move(formatter);
}

QString LimitSlider::limitText(int value) const
{
    return m_limitTextFormatter ? m_limitTextFormatter(value)
                                : QString::number(value);
}

void LimitSlider::paintEvent(QPaintEvent* event)
{
    QSlider::paintEvent(event);

    if (!m_limitEnabled || maximum() <= minimum()) { return; }

    int low = 0;
    int high = 0;
    travelFor(*this, &low, &high);
    if (high <= low) { return; }

    const double span = static_cast<double>(maximum() - minimum());
    const double fraction =
        static_cast<double>(m_limitValue - minimum()) / span;

    int at = low + static_cast<int>(fraction * (high - low));
    if (invertedAppearance()) { at = high - static_cast<int>(fraction * (high - low)); }

    // At the top of the range there is no span to shade, but the operator
    // still needs to see the insert at 100 %: draw a short marker at the end.
    if (m_limitValue >= maximum()) {
        at = invertedAppearance() ? low : high - 3;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_limitBarColor);

    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const QRect groove = style()->subControlRect(
        QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);

    if (orientation() == Qt::Horizontal) {
        // From the limit to the top end. Non-inverted: to the right.
        const int start = invertedAppearance() ? low : at;
        const int end = invertedAppearance() ? at : high;
        painter.drawRect(QRect(start, groove.center().y() - 1,
                               std::max(1, end - start), 3));
    } else {
        // From the limit to the top end. Non-inverted: upwards.
        const int end = invertedAppearance() ? high : low;
        const int top = std::min(at, end);
        painter.drawRect(QRect(groove.center().x() - 1, top, 3,
                               std::max(1, qAbs(end - at))));
    }
}

int LimitSlider::valueForPosition(const QPoint& pos) const
{
    int low = 0;
    int high = 0;
    travelFor(*this, &low, &high);
    if (high <= low) { return m_limitValue; }

    int coord = (orientation() == Qt::Horizontal) ? pos.x() : pos.y();
    coord = std::clamp(coord, low, high);

    double fraction = static_cast<double>(coord - low)
                      / static_cast<double>(high - low);
    if (invertedAppearance()) { fraction = 1.0 - fraction; }
    if (orientation() == Qt::Vertical) { fraction = 1.0 - fraction; }

    return minimum() + qRound(fraction * (maximum() - minimum()));
}

void LimitSlider::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton && m_limitEnabled) {
        m_draggingLimit = true;
        const int value = valueForPosition(event->position().toPoint());
        if (value != m_limitValue) {
            m_limitValue = value;
            update();
        }
        emit limitChanged(m_limitValue);
        emit limitDragStarted();
        // Follow the cursor with the value: the bar alone says where, not how
        // much. Re-shown on every move so it stays for the whole drag.
        QToolTip::showText(event->globalPosition().toPoint(),
                           QStringLiteral("Limit %1").arg(limitText(m_limitValue)),
                           this, rect(), 2000);
        event->accept();
        return;
    }
    QSlider::mousePressEvent(event);
}

void LimitSlider::mouseMoveEvent(QMouseEvent* event)
{
    if (m_draggingLimit && (event->buttons() & Qt::RightButton)) {
        const int value = valueForPosition(event->position().toPoint());
        if (value != m_limitValue) {
            m_limitValue = value;
            update();
        }
        emit limitChanged(m_limitValue);
        QToolTip::showText(event->globalPosition().toPoint(),
                           QStringLiteral("Limit %1").arg(limitText(m_limitValue)),
                           this, rect(), 2000);
        event->accept();
        return;
    }
    QSlider::mouseMoveEvent(event);
}

void LimitSlider::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_draggingLimit && event->button() == Qt::RightButton) {
        m_draggingLimit = false;
        QToolTip::hideText();
        emit limitDragFinished();
        event->accept();
        return;
    }
    QSlider::mouseReleaseEvent(event);
}

}  // namespace NereusSDR
