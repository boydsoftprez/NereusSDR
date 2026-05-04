// =================================================================
// src/gui/meters/MeterWidget.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
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
#include "MeterItem.h"
#include "core/LogCategories.h"

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
#include "DiscordButtonItem.h"
#include "VfoDisplayItem.h"
#include "ClockItem.h"
#include "ClickBoxItem.h"
#include "DataOutItem.h"

#include <QPainter>
#include <QPaintEvent>
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
#ifdef NEREUS_GPU_SPECTRUM
    markOverlayDirty();
    m_bgDirty = true;
#endif
    update();
}

void MeterWidget::removeItem(MeterItem* item)
{
    if (m_items.removeOne(item)) {
#ifdef NEREUS_GPU_SPECTRUM
        markOverlayDirty();
        m_bgDirty = true;
#endif
        update();
    }
}

void MeterWidget::clearItems()
{
    for (MeterItem* item : m_items) {
        if (item->parent() == this) {
            delete item;
        }
    }
    m_items.clear();
#ifdef NEREUS_GPU_SPECTRUM
    markOverlayDirty();
    m_bgDirty = true;
#endif
    update();
}

void MeterWidget::updateMeterValue(int bindingId, double value)
{
    for (MeterItem* item : m_items) {
        if (item->bindingId() == bindingId) {
            item->setValue(value);
        }
    }
#ifdef NEREUS_GPU_SPECTRUM
    markDynamicDirty();
#else
    update();
#endif
}

void MeterWidget::rescalePowerMeters(int paMaxWatts)
{
    if (paMaxWatts <= 0) { return; }

    const double red = static_cast<double>(paMaxWatts);
    const double top = red * 1.2;   // 20% headroom past the red zone

    for (MeterItem* item : m_items) {
        if (item->objectName() == QStringLiteral("PowerBar")) {
            // BarItem: update range + redThreshold.  setRange/setRedThreshold
            // are public on BarItem.  Cast through QObject::qobject_cast so
            // a future swap (NeedleItem etc.) doesn't crash.
            if (BarItem* bar = qobject_cast<BarItem*>(item)) {
                bar->setRange(0.0, top);
                bar->setRedThreshold(red);
            }
        } else if (item->objectName() == QStringLiteral("PowerScale")) {
            if (ScaleItem* scale = qobject_cast<ScaleItem*>(item)) {
                scale->setRange(0.0, top);
                // QRP radios benefit from finer ticks: 7 ticks works for
                // 100/200/1000 W scales but a 0-6 W scale wants 4-6
                // labels max so the numbers don't overlap.
                scale->setMajorTicks(paMaxWatts <= 10 ? 5 : 7);
            }
        }
    }
#ifdef NEREUS_GPU_SPECTRUM
    markDynamicDirty();
#else
    update();
#endif
}

// ============================================================================
// Serialization
// ============================================================================

QString MeterWidget::serializeItems() const
{
    QStringList lines;
    for (const MeterItem* item : m_items) {
        lines << item->serialize();
    }
    return lines.join(QLatin1Char('\n'));
}

bool MeterWidget::deserializeItems(const QString& data)
{
    if (data.isEmpty()) { return false; }

    clearItems();
    QStringList lines = data.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        if (line.isEmpty()) { continue; }

        QString type = line.section(QLatin1Char('|'), 0, 0);
        MeterItem* item = nullptr;
        // Core types (MeterItem.h)
        if (type == QStringLiteral("BAR")) {
            item = new BarItem();
        } else if (type == QStringLiteral("SOLID")) {
            item = new SolidColourItem();
        } else if (type == QStringLiteral("IMAGE")) {
            item = new ImageItem();
        } else if (type == QStringLiteral("SCALE")) {
            item = new ScaleItem();
        } else if (type == QStringLiteral("TEXT")) {
            item = new TextItem();
        } else if (type == QStringLiteral("NEEDLE")) {
            item = new NeedleItem();
        }
        // Phase 3G-4 passive types
        else if (type == QStringLiteral("SPACER")) {
            item = new SpacerItem();
        } else if (type == QStringLiteral("FADECOVER")) {
            item = new FadeCoverItem();
        } else if (type == QStringLiteral("LED")) {
            item = new LEDItem();
        } else if (type == QStringLiteral("HISTORY")) {
            item = new HistoryGraphItem();
        } else if (type == QStringLiteral("MAGICEYE")) {
            item = new MagicEyeItem();
        } else if (type == QStringLiteral("NEEDLESCALEPWR")) {
            item = new NeedleScalePwrItem();
        } else if (type == QStringLiteral("SIGNALTEXT")) {
            item = new SignalTextItem();
        } else if (type == QStringLiteral("DIAL")) {
            item = new DialItem();
        } else if (type == QStringLiteral("TEXTOVERLAY")) {
            item = new TextOverlayItem();
        } else if (type == QStringLiteral("WEBIMAGE")) {
            item = new WebImageItem();
        } else if (type == QStringLiteral("FILTERDISPLAY")) {
            item = new FilterDisplayItem();
        } else if (type == QStringLiteral("ROTATOR")) {
            item = new RotatorItem();
        }
        // Phase 3G-5 interactive types
        else if (type == QStringLiteral("BANDBTNS")) {
            item = new BandButtonItem();
        } else if (type == QStringLiteral("MODEBTNS")) {
            item = new ModeButtonItem();
        } else if (type == QStringLiteral("FILTERBTNS")) {
            item = new FilterButtonItem();
        } else if (type == QStringLiteral("ANTENNABTNS")) {
            item = new AntennaButtonItem();
        } else if (type == QStringLiteral("TUNESTEPBTNS")) {
            item = new TuneStepButtonItem();
        } else if (type == QStringLiteral("OTHERBTNS")) {
            item = new OtherButtonItem();
        } else if (type == QStringLiteral("VOICERECPLAY")) {
            item = new VoiceRecordPlayItem();
        } else if (type == QStringLiteral("DISCORDBTNS")) {
            item = new DiscordButtonItem();
        } else if (type == QStringLiteral("VFO")) {
            item = new VfoDisplayItem();
        } else if (type == QStringLiteral("CLOCK")) {
            item = new ClockItem();
        } else if (type == QStringLiteral("CLICKBOX")) {
            item = new ClickBoxItem();
        } else if (type == QStringLiteral("DATAOUT")) {
            item = new DataOutItem();
        }

        if (item && item->deserialize(line)) {
            addItem(item);
        } else {
            delete item;
        }
    }
    return !m_items.isEmpty();
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
        if (item->hitTest(pos, w, h)) {
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
        if (item->hitTest(pos, w, h)) {
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
        if (item->hitTest(pos, w, h)) {
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
        if (item->hitTest(pos, w, h)) {
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
}

// From Thetis MeterManager.cs:31366-31368 — the per-item render gate
// evaluated by the container's paint loop for every meter item.
bool MeterWidget::shouldRender(const MeterItem* item) const
{
    if (!item) { return false; }
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
    blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
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
        if (m_geomVertCount > 0) {
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
    if (m_geomPipeline && m_geomVertCount > 0) {
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
