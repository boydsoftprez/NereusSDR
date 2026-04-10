#include "MeterWidget.h"
#include "MeterItem.h"
#include "core/LogCategories.h"

#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>

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

// ============================================================================
// Serialization (stubs — full implementation in Task 7)
// ============================================================================

QString MeterWidget::serializeItems() const
{
    return {};
}

bool MeterWidget::deserializeItems(const QString& data)
{
    Q_UNUSED(data);
    return false;
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
#ifdef NEREUS_GPU_SPECTRUM
    markOverlayDirty();
    m_bgDirty = true;
#endif
}

void MeterWidget::drawItems(QPainter& p)
{
    const int w = width();
    const int h = height();
    for (MeterItem* item : m_items) {
        item->paint(p, w, h);
    }
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
        qWarning() << "MeterWidget: failed to load shader" << path;
        return {};
    }
    QShader s = QShader::fromSerialized(f.readAll());
    if (!s.isValid()) {
        qWarning() << "MeterWidget: invalid shader" << path;
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
    m_geomPipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
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
                if (item->renderLayer() == MeterItem::Layer::Background) {
                    item->paint(p, w, h);
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
            if (item->renderLayer() == MeterItem::Layer::Geometry) {
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
                if (item->renderLayer() == MeterItem::Layer::OverlayStatic) {
                    item->paint(p, w, h);
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
                if (item->renderLayer() == MeterItem::Layer::OverlayDynamic) {
                    item->paint(p, w, h);
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
