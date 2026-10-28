// =================================================================
// src/gui/containers/ContentFade.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Fade on RX / Fade on TX for every container item: a port
//                 of Thetis fade() (one step of the animated dim), the fade
//                 cover rectangle and colour of renderFadeCover, and an
//                 overlay that dims per-instance widget content.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

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

#include "ContentFade.h"

#include "ContainerWidget.h"
#include "gui/meters/MeterItem.h"

#include <QEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

// From Thetis MeterManager.cs:33160-33162 [v2.10.3.15]
constexpr int kFadeDurationMs = 500; // fade to take half a second
// From Thetis MeterManager.cs:33163 [v2.10.3.15]
constexpr int kFadeRange = 207; // 255-48 = 207

// From Thetis MeterManager.cs:37837-37840 [v2.10.3.15]
constexpr double kCoverInflateFraction = 0.01;
constexpr double kCoverInflateMinimumPx = 2.0;

} // namespace

namespace ContentFade {

// From Thetis MeterManager.cs:33152-33177 [v2.10.3.15]
// [2.10.1.0] MW0LGE
int nextValue(int current, bool fadeOnRx, bool fadeOnTx, bool transmitting, int updateIntervalMs)
{
    // Thetis's Disabled is passed as both flags true, which the early
    // returns and the dim condition below treat the same way.
    if (!fadeOnTx && transmitting && current == kUndimmedLevel) { return kUndimmedLevel; }
    if (!fadeOnRx && !transmitting && current == kUndimmedLevel) { return kUndimmedLevel; }

    // NereusSDR: an interval of 0 (an item that takes every poll) uses the
    // default interval; Thetis's quickest interval is never 0.
    int updateInterval = updateIntervalMs > 0 ? updateIntervalMs : kDefaultUpdateIntervalMs;// m.QuickestUpdateInterval(m.MOX, false);
    updateInterval = std::min(updateInterval, kFadeDurationMs);
    // fade to take half a second
    const int stepsNeeded = static_cast<int>(std::ceil(kFadeDurationMs / static_cast<float>(updateInterval)));
    const int stepSize = static_cast<int>(std::ceil(kFadeRange / static_cast<float>(stepsNeeded))); // 255-48 = 207

    int value = current;
    if ((transmitting && fadeOnTx) || (!transmitting && fadeOnRx)) {
        value -= stepSize;
        if (value < kDimmedLevel) { value = kDimmedLevel; }
    } else {
        value += stepSize;
        if (value > kUndimmedLevel) { value = kUndimmedLevel; }
    }
    return value;
}

// From Thetis MeterManager.cs:37837-37843 [v2.10.3.15]
QRectF coverRect(const QRectF& itemRect)
{
    const double wi = std::max(kCoverInflateMinimumPx, itemRect.width() * kCoverInflateFraction);
    const double hi = std::max(kCoverInflateMinimumPx, itemRect.height() * kCoverInflateFraction);
    // increase size slightly as indictator lines stroke width of 3 will be outside bounds
    return itemRect.adjusted(-wi, -hi, wi, hi);
}

// Thetis paints the cover in the container's BackgroundColour
// (MeterManager.cs:37844 [v2.10.3.15]).
QColor coverColour(const QWidget* content)
{
    for (const QWidget* w = content; w != nullptr; w = w->parentWidget()) {
        if (const auto* container = qobject_cast<const ContainerWidget*>(w)) {
            const QColor painted = ContainerWidget::contentBackground(container->backgroundColor());
            if (painted.isValid()) { return painted; }
            break;
        }
    }
    return content ? content->palette().color(QPalette::Window) : QColor(Qt::black);
}

QVector<ContentProperty> properties()
{
    ContentProperty rx;
    rx.key = QString::fromLatin1(kFadeOnRxKey);
    rx.label = QObject::tr("Fade on RX");
    rx.kind = ContentProperty::Kind::Toggle;
    rx.defaultValue = false;
    ContentProperty tx;
    tx.key = QString::fromLatin1(kFadeOnTxKey);
    tx.label = QObject::tr("Fade on TX");
    tx.kind = ContentProperty::Kind::Toggle;
    tx.defaultValue = false;
    return {rx, tx};
}

void attach(QWidget* content, const QJsonObject& config, ContentRenderMode mode)
{
    if (!content || mode != ContentRenderMode::Live || overlayFor(content)) { return; }
    const bool rx = config.value(QLatin1String(kFadeOnRxKey)).toBool(false);
    const bool tx = config.value(QLatin1String(kFadeOnTxKey)).toBool(false);
    if (!rx && !tx) { return; }
    // Parented to the content (Qt ownership).
    new ContentFadeOverlay(content, rx, tx);
}

void setTransmitting(QWidget* content, bool transmitting)
{
    if (ContentFadeOverlay* overlay = overlayFor(content)) { overlay->setTransmitting(transmitting); }
}

ContentFadeOverlay* overlayFor(QWidget* content)
{
    return content ? content->findChild<ContentFadeOverlay*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
}

} // namespace ContentFade

ContentFadeOverlay::ContentFadeOverlay(QWidget* content, bool fadeOnRx, bool fadeOnTx)
    : QWidget(content)
    , m_fadeOnRx(fadeOnRx)
    , m_fadeOnTx(fadeOnTx)
{
    setObjectName(QStringLiteral("contentFadeOverlay"));
    // Thetis guards only its button boxes against clicks while faded; the
    // veil itself takes no input.
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::NoFocus);
    m_timer.setInterval(kDefaultUpdateIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &ContentFadeOverlay::advance);
    content->installEventFilter(this);
    follow();
    show();
    if (!settled()) { m_timer.start(); }
}

void ContentFadeOverlay::setTransmitting(bool transmitting)
{
    m_transmitting = transmitting;
    if (!settled() && !m_timer.isActive()) { m_timer.start(); }
}

bool ContentFadeOverlay::settled() const
{
    return ContentFade::nextValue(m_value, m_fadeOnRx, m_fadeOnTx, m_transmitting, kDefaultUpdateIntervalMs) == m_value;
}

void ContentFadeOverlay::advance()
{
    const int next = ContentFade::nextValue(m_value, m_fadeOnRx, m_fadeOnTx, m_transmitting, kDefaultUpdateIntervalMs);
    if (next != m_value) {
        m_value = next;
        update();
    }
    if (settled()) { m_timer.stop(); }
}

void ContentFadeOverlay::follow()
{
    if (QWidget* content = parentWidget()) {
        setGeometry(content->rect());
        raise();
    }
}

bool ContentFadeOverlay::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget()
        && (event->type() == QEvent::Resize || event->type() == QEvent::ChildAdded)) {
        follow();
    }
    return QWidget::eventFilter(watched, event);
}

// From Thetis MeterManager.cs:37835 [v2.10.3.15]: alpha 255 - fade.
void ContentFadeOverlay::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const int alpha = ContentFade::kUndimmedLevel - m_value;
    if (alpha <= 0) { return; }
    QColor colour = ContentFade::coverColour(parentWidget());
    colour.setAlpha(alpha);
    QPainter p(this);
    p.fillRect(rect(), colour);
}

} // namespace NereusSDR
