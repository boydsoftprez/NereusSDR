// =================================================================
// src/gui/meters/MeterWidget.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-02 — Composite reading/replay/cadence contracts by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via OpenAI Codex.
//   2026-09-30 - the geometry buffer is written only in a frame that binds
//                 it (drawsGeometryLayer); with no geometry pipeline (a
//                 shader failed to load) every frame's write stayed pending
//                 in Qt's Metal backend. GUI memory leak follow-up. J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - Task 25: keep unbound filter/click-box and producerless
//                 PB SNR items saved but not rendered. J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-24 - R-R3-49: an item whose feature is not built yet
//                 (the Voice Rec/Play control) is kept and saved but not
//                 drawn and takes no clicks. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-24 - R-R3-49: the Discord control was removed; a saved one is
//                 dropped on load with one log line and the rest of the
//                 container loads. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 39 (D14, R-IOS-13): an unavailable
//                 binding's items are drawn dimmed with the reason as their
//                 tooltip (setBindingUnavailable). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
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

#include "MeterWidget.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/LegacyContainerImporter.h"
#include "MeterItem.h"
#include "core/LogCategories.h"
#include "gui/UnbuiltFeatures.h"
#include "MeterPoller.h"

// All item types for deserializeItems() registry
#include "SpacerItem.h"
#include "FadeCoverItem.h"
#include "LEDItem.h"
#include "HistoryGraphItem.h"
#include "MagicEyeItem.h"
#include "NeedleScalePwrItem.h"
#include "SignalTextItem.h"
#include "DialItem.h"
#include "TextOverlayItem.h"
#include "WebImageItem.h"
#include "FilterDisplayItem.h"
#include "RotatorItem.h"
#include "BandButtonItem.h"
#include "ModeButtonItem.h"
#include "FilterButtonItem.h"
#include "AntennaButtonItem.h"
#include "TuneStepButtonItem.h"
#include "OtherButtonItem.h"
#include "VoiceRecordPlayItem.h"
#include "VfoDisplayItem.h"
#include "ClockItem.h"
#include "ClickBoxItem.h"
#include "DataOutItem.h"

#include <QHelpEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QToolTip>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QStringList>

#ifdef NEREUS_GPU_SPECTRUM
#include <QFile>
#include <rhi/qshader.h>
#endif

namespace NereusSDR {

// ============================================================================
// Constructor / Destructor
// ============================================================================

MeterWidget::MeterWidget(QWidget* parent)
    : MeterBaseClass(parent)
{
    setMinimumSize(100, 80);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(false);
    setMouseTracking(true);

#ifdef NEREUS_GPU_SPECTRUM
    // Platform-specific QRhi backend selection.
    // Order matters: setApi() first, then WA_NativeWindow.
    // Mirrors SpectrumWidget.cpp:97-110 exactly.
#ifdef Q_OS_MAC
    setApi(QRhiWidget::Api::Metal);
    setAttribute(Qt::WA_NativeWindow);
#elif defined(Q_OS_WIN)
    setApi(QRhiWidget::Api::Direct3D11);
    setAttribute(Qt::WA_NativeWindow);
#endif
#else
    // CPU fallback: dark background
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0x0f, 0x0f, 0x1a));
    setPalette(pal);
#endif

    qCDebug(lcMeter) << "MeterWidget created";
}

MeterWidget::~MeterWidget()
{
    emit aboutToDestroy();
    qCDebug(lcMeter) << "MeterWidget destroyed";
}

// ============================================================================
// Item Management
// ============================================================================

void MeterWidget::addItem(MeterItem* item)
{
    if (!item || m_items.contains(item)) { return; }
    if (!item->parent()) {
        item->setParent(this);
    }
    m_items.append(item);
    if (m_unitMode) { item->setUnitMode(*m_unitMode); }
    if (m_powerScale > 0) { item->setPowerScale(m_powerScale); }
    item->resetForTxTransition(m_mox);
    for (int binding : item->readingBindings()) {
        if (!item->hasMmioBinding()) { item->setBindingUnavailable(binding, m_unavailableBindings.value(binding)); }
        const auto cached = m_lastBindingValue.constFind(binding);
        if (!item->hasMmioBinding() && cached != m_lastBindingValue.constEnd()) { item->pushBindingValue(binding, cached.value()); }
    }
#ifdef NEREUS_GPU_SPECTRUM
    markOverlayDirty();
    m_bgDirty = true;
#endif
    update();
    if (item->hasMmioBinding()) {
        const auto cached = m_lastMmioReading.constFind(item->mmioSourceKey());
        if (cached != m_lastMmioReading.constEnd()) { updateMmioValue(item, cached->value, cached->reason); }
    }
    emit itemAdded(item);
}

void MeterWidget::removeItem(MeterItem* item)
{
    if (m_items.removeOne(item)) {
        m_legacyRecords.removeIf([item](const LegacyRecord& record) { return record.item == item; });
#ifdef NEREUS_GPU_SPECTRUM
        markOverlayDirty();
        m_bgDirty = true;
#endif
        update();
        emit itemRemoved(item);
    }
}

void MeterWidget::replaceItems(const QVector<MeterItem*>& items)
{
    ContainerContentRegistry registry;
    QHash<QString, MeterItem*> replacements;
    for (MeterItem* item : items) { if (item) { replacements.insert(registry.captureMeterItem(*item).id,item); } }
    QVector<LegacyRecord> retained;
    for (const auto& record : m_legacyRecords) {
        if (!record.item) { retained.append(record); continue; }
        MeterItem* replacement = replacements.value(registry.captureMeterItem(*record.item).id,nullptr);
        if (replacement) { retained.append({record.raw,replacement}); }
    }
    // Reconciliation callers may reuse current objects. Detach those owned by
    // this widget before clearing, so clearItems cannot destroy a replacement.
    for (MeterItem* item : items) { if (item && item->parent() == this) { item->setParent(nullptr); } }
    clearItems();
    for (MeterItem* item : items) { addItem(item); }
    m_legacyRecords = retained;
}

void MeterWidget::clearItems()
{
    for (MeterItem* item : m_items) {
        if (item->parent() == this) {
            delete item;
        }
    }
    m_items.clear();
    m_legacyRecords.clear();
    emit displayVisibilityChanged();
#ifdef NEREUS_GPU_SPECTRUM
    markOverlayDirty();
    m_bgDirty = true;
#endif
    update();
}

void MeterWidget::updateMeterValue(int bindingId, double value)
{
    const auto old = m_lastBindingValue.constFind(bindingId);
    const bool inputChanged = old == m_lastBindingValue.constEnd() || old.value() != value;
    m_lastBindingValue[bindingId] = value;
    // Delivery precedes repaint shortcuts: repeated primitive samples keep
    // smoothing, and composites retain input for the shared frame clock.
    for (MeterItem* item : m_items) {
        if (item->hasMmioBinding() || !item->readingBindings().contains(bindingId)) { continue; }
        const BarItem* bar = qobject_cast<BarItem*>(item);
        const NeedleItem* needle = qobject_cast<NeedleItem*>(item);
        const double before = bar ? bar->smoothedValue() : (needle ? needle->smoothedValue() : item->value());
        const double peakBefore = bar ? bar->peakValue() : 0.0;
        item->pushBindingValue(bindingId, value);
        const double after = bar ? bar->smoothedValue() : (needle ? needle->smoothedValue() : item->value());
        // Legacy history polylines shift as samples arrive even after the
        // live marker settles; composite history reports changes on advance.
        const bool changed = inputChanged || before != after || (bar && peakBefore != bar->peakValue())
            || (bar && bar->showHistory()) || (needle && needle->historyEnabled());
        if (changed) { invalidateItemLayers(item); }
    }
}

void MeterWidget::updateMmioValue(MeterItem* item, double value, const QString& unavailableReason)
{
    if (!item || !m_items.contains(item)) { return; }
    const BarItem* bar = qobject_cast<BarItem*>(item);
    const NeedleItem* needle = qobject_cast<NeedleItem*>(item);
    const double before = bar ? bar->smoothedValue() : (needle ? needle->smoothedValue() : item->value());
    const double oldPeak = bar ? bar->peakValue() : 0.0;
    const bool availabilityChanged = item->bindingUnavailableReason(item->bindingId()) != unavailableReason;
    m_lastMmioReading[item->mmioSourceKey()] = {value, unavailableReason};
    QSet<int> bindings = item->readingBindings(); bindings.insert(item->bindingId());
    for (int binding : bindings) { item->setBindingUnavailable(binding, unavailableReason); }
    const bool inputChanged = item->value() != value;
    item->setValue(value);
    const double after = bar ? bar->smoothedValue() : (needle ? needle->smoothedValue() : item->value());
    if (availabilityChanged || inputChanged || before != after || (bar && oldPeak != bar->peakValue())
        || (bar && bar->showHistory()) || (needle && needle->historyEnabled())) {
        invalidateItemLayers(item);
    }
}

void MeterWidget::invalidateItemLayers(const MeterItem* item)
{
    ++m_readingInvalidations;
#ifdef NEREUS_GPU_SPECTRUM
    // Legacy Background-only dynamic items need their cached texture rebuilt.
    // A complete face keeps its backdrop static and paints moving parts in
    // OverlayDynamic/Geometry, so normal readings preserve its static caches.
    const bool hasDynamicLayer = item->participatesIn(MeterItem::Layer::OverlayDynamic)
        || item->participatesIn(MeterItem::Layer::Geometry);
    if (!hasDynamicLayer && item->participatesIn(MeterItem::Layer::Background)) { m_bgDirty = true; }
    if (!hasDynamicLayer && item->participatesIn(MeterItem::Layer::OverlayStatic)) { m_overlayStaticDirty = true; }
    markDynamicDirty();
#else
    Q_UNUSED(item);
    update();
#endif
}

void MeterWidget::invalidateReadingLayers(bool staticLayers)
{
#ifdef NEREUS_GPU_SPECTRUM
    if (staticLayers) { m_bgDirty = true; markOverlayDirty(); }
    else { markDynamicDirty(); }
#else
    Q_UNUSED(staticLayers);
    update();
#endif
}

void MeterWidget::advanceMeters(qint64 monotonicMs)
{
    for (MeterItem* item : m_items) {
        if (item->advanceMeter(monotonicMs)) { invalidateItemLayers(item); }
    }
}

void MeterWidget::resetForTxTransition(bool inTx)
{
    setMox(inTx);
    for (auto it = m_lastBindingValue.begin(); it != m_lastBindingValue.end();) {
        if (it.key() >= MeterBinding::TxPower && it.key() < MeterBinding::HwVolts) { it = m_lastBindingValue.erase(it); }
        else { ++it; }
    }
    for (MeterItem* item : m_items) { item->resetForTxTransition(inTx); }
    invalidateReadingLayers(true);
}

void MeterWidget::clearReadingCache()
{
    m_lastBindingValue.clear();
}

void MeterWidget::setUnitMode(MeterItem::MeterUnit unit)
{
    m_unitMode = unit;
    for (MeterItem* item : m_items) { item->setUnitMode(unit); }
    invalidateReadingLayers(true);
}

MeterItem::MeterUnit MeterWidget::unitMode() const
{
    // Legacy MultimeterPage still applies units directly to items.
    if (!m_items.isEmpty()) { return m_items.first()->unitMode(); }
    return m_unitMode.value_or(MeterItem::MeterUnit::dBm);
}

void MeterWidget::rescalePowerMeters(int paMaxWatts)
{
    if (paMaxWatts <= 0) { return; }
    m_powerScale = paMaxWatts;
    for (MeterItem* item : m_items) { item->setPowerScale(paMaxWatts); }
    invalidateReadingLayers(true);
}

// ============================================================================
// Serialization
// ============================================================================

QString MeterWidget::serializeItems() const
{
    QStringList lines;
    QSet<const MeterItem*> emitted;
    for (const auto& record : m_legacyRecords) {
        if (!record.item) { lines.append(record.raw); continue; }
        if (!m_items.contains(record.item)) { continue; }
        QString current = record.item->serialize();
        const QStringList rawFields = record.raw.split(QLatin1Char('|'));
        const int knownCount = current.split(QLatin1Char('|')).size();
        if (!record.raw.trimmed().startsWith(QLatin1Char('{')) && rawFields.size() > knownCount) { current += QLatin1Char('|') + rawFields.mid(knownCount).join(QLatin1Char('|')); }
        lines.append(current); emitted.insert(record.item);
    }
    for (const MeterItem* item : m_items) { if (!emitted.contains(item)) { lines.append(item->serialize()); } }
    return lines.join(QLatin1Char('\n'));
}

bool MeterWidget::deserializeItems(const QString& data)
{
    if (data.isEmpty()) { return false; }
    const DocumentResult imported = LegacyContainerImporter::fromClipboard(data);
    if (!imported.ok || imported.document.containers.size() != 1) { return false; }
    const QVector<ContentEntry> entries = imported.document.containers.first().contents;
    ContainerContentRegistry registry;
    QVector<MeterItem*> decoded;
    for (const auto& entry : entries) {
        if (MeterItem* item = registry.createMeterItem(entry,nullptr,ContentRenderMode::Validation)) { decoded.append(item); }
    }
    // Validate the entire payload before touching live content or fetching URLs.
    if (entries.isEmpty()) { return false; }
    if (decoded.isEmpty() && !m_items.isEmpty()) {
        bool allMalformed = true;
        for (const auto& entry : entries) { allMalformed = allMalformed && entry.extensions.value(QStringLiteral("legacyMalformed")).toBool(); }
        if (allMalformed) { return false; }
    }
    qDeleteAll(decoded);
    clearItems();
    for (const auto& entry : entries) {
        MeterItem* item = registry.createMeterItem(entry,nullptr);
        if (item) { addItem(item); }
        m_legacyRecords.append({entry.config.value(QStringLiteral("legacyRecord")).toString(),item});
    }
    return true;
}

// ============================================================================
// Paint / Resize Events
// ============================================================================

void MeterWidget::paintEvent(QPaintEvent* event)
{
#ifdef NEREUS_GPU_SPECTRUM
    // GPU path: delegate to QRhiWidget base class
    MeterBaseClass::paintEvent(event);
#else
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), QColor(0x0f, 0x0f, 0x1a));
    drawItems(p);
#endif
}

void MeterWidget::resizeEvent(QResizeEvent* event)
{
    MeterBaseClass::resizeEvent(event);
    reflowStackedItems();
#ifdef NEREUS_GPU_SPECTRUM
    markOverlayDirty();
    m_bgDirty = true;
#endif
}

void MeterWidget::reflowStackedItems()
{
    const int h = height();
    if (h <= 0) { return; }

    // Stack rows share the vertical band (bandTop .. 1.0) equally,
    // so N stacked rows always fill the available space under the
    // composite band. Thetis uses a fixed 5% per row because its
    // meter containers are fixed-aspect (MeterManager.cs:21266) —
    // NereusSDR's containers are freely resizable, so a fixed 5%
    // would leave an empty gap below the stack whenever the user
    // sizes the container larger than N × 5%. A 24px pixel floor
    // keeps rows readable when the container is small enough that
    // the per-slot share would otherwise drop below that; in that
    // case the bottom rows overflow the widget (same as Thetis).
    constexpr int kMinRowHeightPx = 24;

    float bandTop = 0.0f;
    int maxSlot = -1;
    for (const MeterItem* item : m_items) {
        if (!item) { continue; }
        if (item->itemHeight() > 0.30f) {
            const float bottom = item->y() + item->itemHeight();
            if (bottom > bandTop) { bandTop = bottom; }
        }
        if (item->stackSlot() >= 0) {
            maxSlot = qMax(maxSlot, item->stackSlot());
        }
    }

    const int numSlots = maxSlot + 1;
    int slotHeightPx = kMinRowHeightPx;
    if (numSlots > 0) {
        const int availablePx = static_cast<int>((1.0f - bandTop)
                                                 * static_cast<float>(h));
        slotHeightPx = qMax(availablePx / numSlots, kMinRowHeightPx);
    }

    for (MeterItem* item : m_items) {
        if (!item) { continue; }
        item->layoutInStackSlot(h, slotHeightPx, bandTop);
    }
}

void MeterWidget::inferStackFromGeometry()
{
    // Walk deserialized items, cluster non-composite, non-background
    // items into rows by overlapping y-intervals, and assign stack
    // metadata so resize reflow works on loaded containers too. The
    // clustering is deliberately forgiving: any item with
    // itemHeight() < 0.15 counts as a stack candidate, and clusters
    // whose y-intervals touch get merged.
    //
    // "Composite" detection stays loose — we only care that big
    // needle panels (h > 0.3) are excluded from the stack tagging
    // so they don't get pulled into the reflow path.
    //
    // The stack band top is inferred from the topmost cluster's
    // y-min, rounded to the nearest standard band start (0.0 or
    // 0.70). This lets bar rows that were saved under a compressed
    // ANAN MM come back with the correct bandTop.
    if (m_items.isEmpty()) { return; }
    const int widgetH = height();
    if (widgetH <= 0) { return; }

    struct Cluster {
        float yMin;
        float yMax;
        QVector<MeterItem*> members;
    };
    // Cluster only when y-intervals genuinely OVERLAP by more than an
    // epsilon. Touching boundaries (yMax of row N == yMin of row N+1,
    // the normal stack-row layout) or sub-ULP floating-point drift from
    // repeated serialize/deserialize cycles must NOT merge: if adjacent
    // rows collapse into one cluster they all collapse onto stack slot
    // 0, cramming N rows into one row's height and visibly compressing
    // the meter a little more on every reparent trip.
    constexpr float kOverlapEps = 0.002f;
    QVector<Cluster> clusters;
    for (MeterItem* item : m_items) {
        if (!item) { continue; }
        const float h = item->itemHeight();
        if (h <= 0.0f || h >= 0.30f) { continue; }  // skip composites + backgrounds
        const float yMin = item->y();
        const float yMax = item->y() + h;
        bool placed = false;
        for (Cluster& c : clusters) {
            if (yMin + kOverlapEps < c.yMax && yMax > c.yMin + kOverlapEps) {
                c.yMin = qMin(c.yMin, yMin);
                c.yMax = qMax(c.yMax, yMax);
                c.members.append(item);
                placed = true;
                break;
            }
        }
        if (!placed) {
            clusters.append({yMin, yMax, {item}});
        }
    }
    if (clusters.isEmpty()) { return; }

    std::sort(clusters.begin(), clusters.end(),
              [](const Cluster& a, const Cluster& b) { return a.yMin < b.yMin; });

    // No per-item bandTop to set — reflowStackedItems() detects
    // the composite band dynamically on every call.
    for (int i = 0; i < clusters.size(); ++i) {
        Cluster& c = clusters[i];
        const float oldYMin  = c.yMin;
        const float oldSlotH = c.yMax - c.yMin;
        if (oldSlotH <= 0.0f) { continue; }
        for (MeterItem* mi : c.members) {
            const float localY = (mi->y() - oldYMin) / oldSlotH;
            const float localH = mi->itemHeight() / oldSlotH;
            mi->setSlotLocalY(qBound(0.0f, localY, 1.0f));
            mi->setSlotLocalH(qBound(0.0f, localH, 1.0f));
            mi->setStackSlot(i);
        }
    }

    reflowStackedItems();
}

void MeterWidget::mousePressEvent(QMouseEvent* event)
{
    const int w = width();
    const int h = height();
    const QPointF pos = event->position();

    for (int i = m_items.size() - 1; i >= 0; --i) {
        MeterItem* item = m_items[i];
        if (itemFeatureBuilt(item) && item->hitTest(pos, w, h)) {
            if (item->handleMousePress(event, w, h)) {
                update();
                return;
            }
        }
    }
    MeterBaseClass::mousePressEvent(event);
}

void MeterWidget::mouseReleaseEvent(QMouseEvent* event)
{
    const int w = width();
    const int h = height();
    const QPointF pos = event->position();

    for (int i = m_items.size() - 1; i >= 0; --i) {
        MeterItem* item = m_items[i];
        if (itemFeatureBuilt(item) && item->hitTest(pos, w, h)) {
            if (item->handleMouseRelease(event, w, h)) {
                update();
                return;
            }
        }
    }
    MeterBaseClass::mouseReleaseEvent(event);
}

void MeterWidget::mouseMoveEvent(QMouseEvent* event)
{
    const int w = width();
    const int h = height();
    const QPointF pos = event->position();

    for (int i = m_items.size() - 1; i >= 0; --i) {
        MeterItem* item = m_items[i];
        if (itemFeatureBuilt(item) && item->hitTest(pos, w, h)) {
            if (item->handleMouseMove(event, w, h)) {
                update();
                return;
            }
        }
    }
    MeterBaseClass::mouseMoveEvent(event);
}

void MeterWidget::wheelEvent(QWheelEvent* event)
{
    const int w = width();
    const int h = height();
    const QPointF pos = event->position();

    for (int i = m_items.size() - 1; i >= 0; --i) {
        MeterItem* item = m_items[i];
        if (itemFeatureBuilt(item) && item->hitTest(pos, w, h)) {
            if (item->handleWheel(event, w, h)) {
                update();
                return;
            }
        }
    }
    MeterBaseClass::wheelEvent(event);
}

void MeterWidget::drawItems(QPainter& p)
{
    const int w = width();
    const int h = height();
    for (MeterItem* item : m_items) {
        if (!shouldRender(item)) { continue; }
        item->paint(p, w, h);
    }
    drawUnavailableVeils(p);
}

// iPhone app plan Task 39: an unavailable binding's items, dimmed by the
// widget's own background colour over them.
void MeterWidget::drawUnavailableVeils(QPainter& p) const
{
    const int w = width();
    const int h = height();
    for (const MeterItem* item : m_items) {
        if (!shouldRender(item)) { continue; }
        QSet<int> bindings = item->readingBindings();
        if (item->hasMmioBinding()) { bindings.insert(item->bindingId()); }
        bool allUnavailable = !bindings.isEmpty();
        for (int binding : bindings) { allUnavailable = allUnavailable && !item->bindingUnavailableReason(binding).isEmpty(); }
        if (!allUnavailable) { continue; }
        // The item's own rectangle (MeterItem::pixelRect's arithmetic).
        const QRect rect(static_cast<int>(item->x() * w), static_cast<int>(item->y() * h),
                         static_cast<int>(item->itemWidth() * w),
                         static_cast<int>(item->itemHeight() * h));
        p.fillRect(rect, QColor(0x0f, 0x0f, 0x1a, 170));
    }
}

void MeterWidget::setBindingUnavailable(int bindingId, const QString& reason)
{
    for (MeterItem* item : m_items) {
        if (!item->hasMmioBinding() && item->readingBindings().contains(bindingId)) { item->setBindingUnavailable(bindingId, reason); }
    }
    const auto it = m_unavailableBindings.constFind(bindingId);
    const bool unavailable = it != m_unavailableBindings.constEnd();
    if ((!unavailable && reason.isEmpty()) || (unavailable && it.value() == reason)) {
        return;
    }
    if (reason.isEmpty()) {
        m_unavailableBindings.remove(bindingId);
    } else {
        m_unavailableBindings.insert(bindingId, reason);
    }
#ifdef NEREUS_GPU_SPECTRUM
    markOverlayDirty();
#else
    update();
#endif
}

QString MeterWidget::unavailableReasonAt(const QPointF& pos) const
{
    const int w = width();
    const int h = height();
    for (int i = m_items.size() - 1; i >= 0; --i) {
        const MeterItem* item = m_items.at(i);
        if (!shouldRender(item) || !item->hitTest(pos, w, h)) {
            continue;
        }
        QStringList reasons;
        QSet<int> bindings = item->readingBindings();
        if (item->hasMmioBinding()) { bindings.insert(item->bindingId()); }
        for (int binding : bindings) {
            const QString reason = item->bindingUnavailableReason(binding);
            if (!reason.isEmpty() && !reasons.contains(reason)) { reasons.append(reason); }
        }
        if (!reasons.isEmpty()) { return reasons.join(QLatin1Char('\n')); }
    }
    return {};
}

bool MeterWidget::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {
        auto* help = static_cast<QHelpEvent*>(event);
        const QString reason = unavailableReasonAt(help->pos());
        if (!reason.isEmpty()) {
            QToolTip::showText(help->globalPos(), reason, this);
            return true;
        }
    }
    return MeterBaseClass::event(event);
}

// R-R3-49: false for an item that fronts a feature not built yet (the
// Voice Rec/Play control while the voice recorder is not built).
bool MeterWidget::itemFeatureBuilt(const MeterItem* item)
{
    if (qobject_cast<const VoiceRecordPlayItem*>(item) != nullptr) {
        return UnbuiltFeatures::isBuilt(UnbuiltFeature::Voice);
    }
    if (qobject_cast<const FilterDisplayItem*>(item) != nullptr) {
        return UnbuiltFeatures::isBuilt(UnbuiltFeature::ContainerFilterDisplay);
    }
    if (qobject_cast<const ClickBoxItem*>(item) != nullptr) {
        return UnbuiltFeatures::isBuilt(UnbuiltFeature::ContainerClickBox);
    }
    if (item->bindingId() == MeterBinding::PbSnr) {
        return UnbuiltFeatures::isBuilt(UnbuiltFeature::PbSnr);
    }
    return true;
}

// From Thetis MeterManager.cs:31366-31368 — the per-item render gate
// evaluated by the container's paint loop for every meter item.
bool MeterWidget::shouldRender(const MeterItem* item) const
{
    if (!item) { return false; }
    // NereusSDR R-R3-49 (not in Thetis): an item whose feature is not built
    // yet stays in the container, and is saved with it, but is not drawn.
    if (!itemFeatureBuilt(item)) { return false; }
    const bool baseOk =
        ((m_mox && item->onlyWhenTx()) || (!m_mox && item->onlyWhenRx()))
        || (!item->onlyWhenTx() && !item->onlyWhenRx());
    if (!baseOk) { return false; }
    const int ig = item->displayGroup();
    return (m_displayGroup == 0 || ig == 0 || ig == m_displayGroup);
}

void MeterWidget::setMox(bool mox)
{
    if (m_mox == mox) { return; }
    m_mox = mox;
#ifdef NEREUS_GPU_SPECTRUM
    markOverlayDirty();
    m_bgDirty = true;
#endif
    update();
    emit displayVisibilityChanged();
}

void MeterWidget::setDisplayGroup(int group)
{
    if (m_displayGroup == group) { return; }
    m_displayGroup = group;
#ifdef NEREUS_GPU_SPECTRUM
    markOverlayDirty();
    m_bgDirty = true;
#endif
    update();
    emit displayVisibilityChanged();
}

// ============================================================================
// GPU Rendering Path (QRhiWidget)
// Mirrors SpectrumWidget.cpp:1257-1765 exactly.
// ============================================================================

#ifdef NEREUS_GPU_SPECTRUM

// Fullscreen quad: position (x,y) + texcoord (u,v)
// From AetherSDR SpectrumWidget.cpp:1779 / SpectrumWidget.cpp:1266
static const float kMeterQuadData[] = {
    -1, -1,  0, 1,   // bottom-left
     1, -1,  1, 1,   // bottom-right
    -1,  1,  0, 0,   // top-left
     1,  1,  1, 0,   // top-right
};

static QShader loadMeterShader(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qCWarning(lcMeter) << "MeterWidget: failed to load shader" << path;
        return {};
    }
    QShader s = QShader::fromSerialized(f.readAll());
    if (!s.isValid()) {
        qCWarning(lcMeter) << "MeterWidget: invalid shader" << path;
    }
    return s;
}

void MeterWidget::initBackgroundPipeline()
{
    QRhi* r = rhi();

    m_bgVbo = r->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(kMeterQuadData));
    m_bgVbo->create();

    const int w = qMax(width(), 64);
    const int h = qMax(height(), 64);
    m_bgGpuTex = r->newTexture(QRhiTexture::RGBA8, QSize(w, h));
    m_bgGpuTex->create();

    m_bgSampler = r->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                 QRhiSampler::None,
                                 QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_bgSampler->create();

    m_bgSrb = r->newShaderResourceBindings();
    m_bgSrb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_bgGpuTex, m_bgSampler),
    });
    m_bgSrb->create();

    QShader vs = loadMeterShader(QStringLiteral(":/shaders/resources/shaders/meter_textured.vert.qsb"));
    QShader fs = loadMeterShader(QStringLiteral(":/shaders/resources/shaders/meter_textured.frag.qsb"));
    if (!vs.isValid() || !fs.isValid()) { return; }

    m_bgPipeline = r->newGraphicsPipeline();
    m_bgPipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vs},
        {QRhiShaderStage::Fragment, fs},
    });

    QRhiVertexInputLayout layout;
    layout.setBindings({{4 * sizeof(float)}});
    layout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},
        {0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)},
    });
    m_bgPipeline->setVertexInputLayout(layout);
    m_bgPipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    m_bgPipeline->setShaderResourceBindings(m_bgSrb);
    m_bgPipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    m_bgPipeline->create();

    // Initialize backing QImage
    m_bgImage = QImage(w, h, QImage::Format_RGBA8888);
    m_bgDirty = true;
}

void MeterWidget::initGeometryPipeline()
{
    QRhi* r = rhi();

    m_geomVbo = r->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                              kMaxGeomVerts * kGeomVertStride * sizeof(float));
    m_geomVbo->create();

    m_geomSrb = r->newShaderResourceBindings();
    m_geomSrb->setBindings({});
    m_geomSrb->create();

    QShader vs = loadMeterShader(QStringLiteral(":/shaders/resources/shaders/meter_geometry.vert.qsb"));
    QShader fs = loadMeterShader(QStringLiteral(":/shaders/resources/shaders/meter_geometry.frag.qsb"));
    if (!vs.isValid() || !fs.isValid()) { return; }

    QRhiVertexInputLayout layout;
    layout.setBindings({{kGeomVertStride * sizeof(float)}});
    layout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},
        {0, 1, QRhiVertexInputAttribute::Float4, 2 * sizeof(float)},
    });

    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;

    m_geomPipeline = r->newGraphicsPipeline();
    m_geomPipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vs},
        {QRhiShaderStage::Fragment, fs},
    });
    m_geomPipeline->setVertexInputLayout(layout);
    m_geomPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
    m_geomPipeline->setShaderResourceBindings(m_geomSrb);
    m_geomPipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    m_geomPipeline->setTargetBlends({blend});
    m_geomPipeline->create();
}

void MeterWidget::initOverlayPipeline()
{
    // Mirrors SpectrumWidget.cpp:1338-1397 exactly.
    QRhi* r = rhi();

    m_ovVbo = r->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(kMeterQuadData));
    m_ovVbo->create();

    const int w = qMax(width(), 64);
    const int h = qMax(height(), 64);
    const qreal dpr = devicePixelRatioF();
    const int pw = static_cast<int>(w * dpr);
    const int ph = static_cast<int>(h * dpr);
    m_ovGpuTex = r->newTexture(QRhiTexture::RGBA8, QSize(pw, ph));
    m_ovGpuTex->create();

    m_ovSampler = r->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                 QRhiSampler::None,
                                 QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    m_ovSampler->create();

    m_ovSrb = r->newShaderResourceBindings();
    m_ovSrb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_ovGpuTex, m_ovSampler),
    });
    m_ovSrb->create();

    QShader vs = loadMeterShader(QStringLiteral(":/shaders/resources/shaders/meter_textured.vert.qsb"));
    QShader fs = loadMeterShader(QStringLiteral(":/shaders/resources/shaders/meter_textured.frag.qsb"));
    if (!vs.isValid() || !fs.isValid()) { return; }

    m_ovPipeline = r->newGraphicsPipeline();
    m_ovPipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vs},
        {QRhiShaderStage::Fragment, fs},
    });

    QRhiVertexInputLayout layout;
    layout.setBindings({{4 * sizeof(float)}});
    layout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},
        {0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)},
    });
    m_ovPipeline->setVertexInputLayout(layout);
    m_ovPipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    m_ovPipeline->setShaderResourceBindings(m_ovSrb);
    m_ovPipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());

    // Alpha blending for overlay compositing
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    // QPainter overlay images already contain premultiplied RGB.
    // Multiply only the destination by remaining alpha, matching CPU composition.
    blend.srcColor = QRhiGraphicsPipeline::One;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    m_ovPipeline->setTargetBlends({blend});
    m_ovPipeline->create();

    m_overlayStatic = QImage(pw, ph, QImage::Format_RGBA8888_Premultiplied);
    m_overlayStatic.setDevicePixelRatio(dpr);
    m_overlayDynamic = QImage(pw, ph, QImage::Format_RGBA8888_Premultiplied);
    m_overlayDynamic.setDevicePixelRatio(dpr);
}

void MeterWidget::initialize(QRhiCommandBuffer* cb)
{
    if (m_rhiInitialized) { return; }

    QRhi* r = rhi();
    if (!r) {
        qCWarning(lcMeter) << "MeterWidget: QRhi init failed — no GPU backend";
        return;
    }
    qCDebug(lcMeter) << "MeterWidget: QRhi backend:" << r->backendName();

    auto* batch = r->nextResourceUpdateBatch();

    initBackgroundPipeline();
    initGeometryPipeline();
    initOverlayPipeline();

    // Upload quad VBO data for textured pipelines
    batch->uploadStaticBuffer(m_bgVbo, kMeterQuadData);
    batch->uploadStaticBuffer(m_ovVbo, kMeterQuadData);

    cb->resourceUpdate(batch);
    m_rhiInitialized = true;
}

void MeterWidget::renderGpuFrame(QRhiCommandBuffer* cb)
{
    QRhi* r = rhi();
    const int w = width();
    const int h = height();
    if (w <= 0 || h <= 0) { return; }

    auto* batch = r->nextResourceUpdateBatch();

    // ---- Pipeline 1: Background ----
    // Repaint if dirty (size change or items changed)
    {
        const QSize bgSize(qMax(w, 64), qMax(h, 64));
        if (m_bgImage.size() != bgSize) {
            m_bgImage = QImage(bgSize, QImage::Format_RGBA8888);
            m_bgGpuTex->setPixelSize(bgSize);
            m_bgGpuTex->create();
            m_bgSrb->setBindings({
                QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_bgGpuTex, m_bgSampler),
            });
            m_bgSrb->create();
            m_bgDirty = true;
        }

        if (m_bgDirty) {
            m_bgImage.fill(QColor(0x0f, 0x0f, 0x1a));
            QPainter p(&m_bgImage);
            p.setRenderHint(QPainter::Antialiasing, false);
            for (MeterItem* item : m_items) {
                if (!shouldRender(item)) { continue; }
                if (item->participatesIn(MeterItem::Layer::Background)) {
                    item->paintForLayer(p, w, h, MeterItem::Layer::Background);
                }
            }
            batch->uploadTexture(m_bgGpuTex, QRhiTextureUploadEntry(0, 0,
                QRhiTextureSubresourceUploadDescription(m_bgImage)));
            m_bgDirty = false;
        }
    }

    // ---- Pipeline 2: Geometry ----
    // Build vertex array from all Geometry items
    {
        QVector<float> verts;
        verts.reserve(kMaxGeomVerts * kGeomVertStride);
        for (MeterItem* item : m_items) {
            if (!shouldRender(item)) { continue; }
            if (item->participatesIn(MeterItem::Layer::Geometry)) {
                item->emitVertices(verts, w, h);
            }
        }
        m_geomVertCount = qMin(verts.size() / kGeomVertStride, kMaxGeomVerts);
        if (drawsGeometryLayer(m_geomPipeline != nullptr, m_geomVertCount)) {
            batch->updateDynamicBuffer(m_geomVbo, 0,
                m_geomVertCount * kGeomVertStride * sizeof(float), verts.constData());
        }
    }

    // ---- Pipeline 3: Overlay ----
    {
        const qreal dpr = devicePixelRatioF();
        const int pw = static_cast<int>(w * dpr);
        const int ph = static_cast<int>(h * dpr);

        // Handle resize
        if (m_overlayStatic.size() != QSize(pw, ph)) {
            m_overlayStatic = QImage(pw, ph, QImage::Format_RGBA8888_Premultiplied);
            m_overlayStatic.setDevicePixelRatio(dpr);
            m_overlayDynamic = QImage(pw, ph, QImage::Format_RGBA8888_Premultiplied);
            m_overlayDynamic.setDevicePixelRatio(dpr);
            m_ovGpuTex->setPixelSize(QSize(pw, ph));
            m_ovGpuTex->create();
            m_ovSrb->setBindings({
                QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_ovGpuTex, m_ovSampler),
            });
            m_ovSrb->create();
            m_overlayStaticDirty = true;
            m_overlayDynamicDirty = true;
        }

        if (m_overlayStaticDirty) {
            m_overlayStatic.fill(Qt::transparent);
            QPainter p(&m_overlayStatic);
            p.setRenderHint(QPainter::Antialiasing, false);
            for (MeterItem* item : m_items) {
                if (!shouldRender(item)) { continue; }
                if (item->participatesIn(MeterItem::Layer::OverlayStatic)) {
                    item->paintForLayer(p, w, h, MeterItem::Layer::OverlayStatic);
                }
            }
            m_overlayStaticDirty = false;
            m_overlayDynamicDirty = true;
        }

        if (m_overlayDynamicDirty) {
            // Copy static into dynamic, then paint dynamic items on top
            m_overlayDynamic = m_overlayStatic.copy();
            QPainter p(&m_overlayDynamic);
            p.setRenderHint(QPainter::Antialiasing, false);
            for (MeterItem* item : m_items) {
                if (!shouldRender(item)) { continue; }
                if (item->participatesIn(MeterItem::Layer::OverlayDynamic)) {
                    item->paintForLayer(p, w, h, MeterItem::Layer::OverlayDynamic);
                }
            }
            // Task 39: over everything drawn so far (the overlay is the top
            // layer), so an unavailable item's bar is dimmed too.
            drawUnavailableVeils(p);
            m_overlayDynamicDirty = false;
            m_overlayNeedsUpload = true;
        }

        if (m_overlayNeedsUpload) {
            batch->uploadTexture(m_ovGpuTex, QRhiTextureUploadEntry(0, 0,
                QRhiTextureSubresourceUploadDescription(m_overlayDynamic)));
            m_overlayNeedsUpload = false;
        }
    }

    cb->resourceUpdate(batch);

    // ---- Begin render pass ----
    const QColor clearColor(0x0f, 0x0f, 0x1a);
    cb->beginPass(renderTarget(), clearColor, {1.0f, 0});

    const QSize outputSize = renderTarget()->pixelSize();

    // Draw background
    if (m_bgPipeline) {
        cb->setGraphicsPipeline(m_bgPipeline);
        cb->setShaderResources(m_bgSrb);
        cb->setViewport({0, 0,
            static_cast<float>(outputSize.width()),
            static_cast<float>(outputSize.height())});
        const QRhiCommandBuffer::VertexInput vbuf(m_bgVbo, 0);
        cb->setVertexInput(0, 1, &vbuf);
        cb->draw(4);
    }

    // Draw geometry
    if (drawsGeometryLayer(m_geomPipeline != nullptr, m_geomVertCount)) {
        cb->setGraphicsPipeline(m_geomPipeline);
        cb->setShaderResources(m_geomSrb);
        cb->setViewport({0, 0,
            static_cast<float>(outputSize.width()),
            static_cast<float>(outputSize.height())});
        const QRhiCommandBuffer::VertexInput vbuf(m_geomVbo, 0);
        cb->setVertexInput(0, 1, &vbuf);
        cb->draw(m_geomVertCount);
    }

    // Draw overlay
    if (m_ovPipeline) {
        cb->setGraphicsPipeline(m_ovPipeline);
        cb->setShaderResources(m_ovSrb);
        cb->setViewport({0, 0,
            static_cast<float>(outputSize.width()),
            static_cast<float>(outputSize.height())});
        const QRhiCommandBuffer::VertexInput vbuf(m_ovVbo, 0);
        cb->setVertexInput(0, 1, &vbuf);
        cb->draw(4);
    }

    cb->endPass();
}

void MeterWidget::render(QRhiCommandBuffer* cb)
{
    if (!m_rhiInitialized) { return; }
    renderGpuFrame(cb);
}

void MeterWidget::releaseResources()
{
    delete m_bgPipeline;    m_bgPipeline = nullptr;
    delete m_bgSrb;         m_bgSrb = nullptr;
    delete m_bgVbo;         m_bgVbo = nullptr;
    delete m_bgGpuTex;      m_bgGpuTex = nullptr;
    delete m_bgSampler;     m_bgSampler = nullptr;

    delete m_geomPipeline;  m_geomPipeline = nullptr;
    delete m_geomSrb;       m_geomSrb = nullptr;
    delete m_geomVbo;       m_geomVbo = nullptr;

    delete m_ovPipeline;    m_ovPipeline = nullptr;
    delete m_ovSrb;         m_ovSrb = nullptr;
    delete m_ovVbo;         m_ovVbo = nullptr;
    delete m_ovGpuTex;      m_ovGpuTex = nullptr;
    delete m_ovSampler;     m_ovSampler = nullptr;

    m_rhiInitialized = false;
}

#endif // NEREUS_GPU_SPECTRUM

} // namespace NereusSDR
