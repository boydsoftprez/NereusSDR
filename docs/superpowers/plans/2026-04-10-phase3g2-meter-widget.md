# Phase 3G-2: MeterWidget GPU Renderer — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a QRhi-based meter rendering engine that runs inside ContainerWidget, following SpectrumWidget's proven pipeline pattern, with live WDSP meter data binding.

**Architecture:** MeterWidget inherits QRhiWidget (like SpectrumWidget) and renders MeterItems through three logical GPU pipeline groups: (1) textured-quad for cached backgrounds/images, (2) vertex-colored geometry for animated bars/needles, (3) QPainter→texture overlay for text readouts and tick marks (split into static and dynamic layers). A MeterPoller polls RxChannel::getMeter() at 100ms intervals and pushes values to bound MeterItems via signal. ContainerWidget::setContent() installs MeterWidget into Container #0.

**Tech Stack:** C++20, Qt6 (QRhiWidget, QRhi, QPainter), GLSL 440 shaders via qt_add_shaders, WDSP GetRXAMeter, AppSettings XML persistence.

**Visual style authority:** AetherSDR (colors, fonts, spacing from SMeterWidget + AppletPanel). Behavior/logic authority: Thetis MeterManager.cs.

---

## File Structure

### New files

| File | Responsibility |
|------|----------------|
| `src/gui/meters/MeterWidget.h` | QRhiWidget subclass — GPU init, render loop, pipeline management |
| `src/gui/meters/MeterWidget.cpp` | Pipeline init, render pass, overlay painting, item layout |
| `src/gui/meters/MeterItem.h` | Base class + all concrete item types (BarItem, TextItem, ScaleItem, SolidColourItem, ImageItem) |
| `src/gui/meters/MeterItem.cpp` | Item rendering helpers, serialization, data binding |
| `src/gui/meters/ItemGroup.h` | Composite N MeterItems into named preset |
| `src/gui/meters/ItemGroup.cpp` | Group layout, serialization, preset factory |
| `src/gui/meters/MeterPoller.h` | QTimer-based WDSP meter polling + signal emission |
| `src/gui/meters/MeterPoller.cpp` | Poll loop, smoothing, value distribution |
| `resources/shaders/meter_textured.vert` | Textured quad vertex shader (Pipelines 1 & 3) |
| `resources/shaders/meter_textured.frag` | Textured quad fragment shader (Pipelines 1 & 3) |
| `resources/shaders/meter_geometry.vert` | Vertex-colored geometry vertex shader (Pipeline 2) |
| `resources/shaders/meter_geometry.frag` | Vertex-colored geometry fragment shader (Pipeline 2) |

### Modified files

| File | Change |
|------|--------|
| `CMakeLists.txt:190-197` | Add meter source files to target_sources |
| `CMakeLists.txt:262-271` | Add meter shaders to qt_add_shaders |
| `src/core/LogCategories.h:19` | Add `Q_DECLARE_LOGGING_CATEGORY(lcMeter)` |
| `src/core/LogCategories.cpp` | Add `Q_LOGGING_CATEGORY(lcMeter, "nereus.meter")` |
| `src/gui/MainWindow.h` | Forward-declare MeterPoller, add member pointer |
| `src/gui/MainWindow.cpp:223-232` | Install MeterWidget into Container #0, wire MeterPoller |

---

## Task 1: Meter Shaders

**Files:**
- Create: `resources/shaders/meter_textured.vert`
- Create: `resources/shaders/meter_textured.frag`
- Create: `resources/shaders/meter_geometry.vert`
- Create: `resources/shaders/meter_geometry.frag`
- Modify: `CMakeLists.txt:262-271`

- [ ] **Step 1: Create textured quad vertex shader**

Create `resources/shaders/meter_textured.vert`:
```glsl
#version 440

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

layout(location = 0) out vec2 v_uv;

void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
    v_uv = texcoord;
}
```

This is identical to SpectrumWidget's `overlay.vert` — same fullscreen quad pattern.

- [ ] **Step 2: Create textured quad fragment shader**

Create `resources/shaders/meter_textured.frag`:
```glsl
#version 440

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D tex;

void main()
{
    fragColor = texture(tex, v_uv);
}
```

Same as `overlay.frag` — samples texture with UV, alpha preserved.

- [ ] **Step 3: Create vertex-colored geometry vertex shader**

Create `resources/shaders/meter_geometry.vert`:
```glsl
#version 440

layout(location = 0) in vec2 inPos;    // (x_ndc, y_ndc)
layout(location = 1) in vec4 inColor;  // (r, g, b, a)

layout(location = 0) out vec4 v_color;

void main()
{
    gl_Position = vec4(inPos, 0.0, 1.0);
    v_color = inColor;
}
```

Same as `spectrum.vert` — pass-through position + color.

- [ ] **Step 4: Create vertex-colored geometry fragment shader**

Create `resources/shaders/meter_geometry.frag`:
```glsl
#version 440

layout(location = 0) in vec4 v_color;
layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = v_color;
}
```

Same as `spectrum.frag` — output interpolated vertex color.

- [ ] **Step 5: Add shaders to CMakeLists.txt**

In `CMakeLists.txt`, inside the `if(Qt6ShaderTools_FOUND)` block (after line 271), add the meter shaders to a new `qt_add_shaders` call:

```cmake
        qt_add_shaders(NereusSDR "nereus_meter_shaders"
            PREFIX "/shaders"
            FILES
                resources/shaders/meter_textured.vert
                resources/shaders/meter_textured.frag
                resources/shaders/meter_geometry.vert
                resources/shaders/meter_geometry.frag
        )
```

- [ ] **Step 6: Verify shaders compile**

Run:
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build -j$(nproc)
```

Expected: Build succeeds. `.qsb` files appear in `build/.qsb/resources/shaders/meter_*.qsb`.

- [ ] **Step 7: Commit**

```bash
git add resources/shaders/meter_textured.vert resources/shaders/meter_textured.frag \
       resources/shaders/meter_geometry.vert resources/shaders/meter_geometry.frag \
       CMakeLists.txt
git commit -m "Add meter GPU shaders (textured quad + vertex-colored geometry)"
```

---

## Task 2: Logging Category

**Files:**
- Modify: `src/core/LogCategories.h:19`
- Modify: `src/core/LogCategories.cpp`

- [ ] **Step 1: Declare lcMeter category**

In `src/core/LogCategories.h`, after line 19 (`Q_DECLARE_LOGGING_CATEGORY(lcContainer)`), add:

```cpp
Q_DECLARE_LOGGING_CATEGORY(lcMeter)
```

- [ ] **Step 2: Define lcMeter category**

In `src/core/LogCategories.cpp`, find the existing category definitions (search for `Q_LOGGING_CATEGORY(lcContainer`) and add after it:

```cpp
Q_LOGGING_CATEGORY(lcMeter, "nereus.meter")
```

- [ ] **Step 3: Verify build**

Run:
```bash
cmake --build build -j$(nproc)
```

Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/core/LogCategories.h src/core/LogCategories.cpp
git commit -m "Add lcMeter logging category"
```

---

## Task 3: MeterWidget QRhiWidget Shell

**Files:**
- Create: `src/gui/meters/MeterWidget.h`
- Create: `src/gui/meters/MeterWidget.cpp`
- Modify: `CMakeLists.txt:190-197`

- [ ] **Step 1: Create MeterWidget header**

Create `src/gui/meters/MeterWidget.h`:

```cpp
#pragma once

#include <QWidget>
#include <QImage>
#include <QVector>

#ifdef NEREUS_GPU_SPECTRUM
#include <QRhiWidget>
#include <rhi/qrhi.h>
using MeterBaseClass = QRhiWidget;
#else
using MeterBaseClass = QWidget;
#endif

namespace NereusSDR {

class MeterItem;

// GPU-accelerated meter rendering widget.
// One MeterWidget per ContainerWidget. Renders all MeterItems in a single
// draw pass using three pipeline groups:
//   Pipeline 1 (textured quad): cached background textures, images
//   Pipeline 2 (vertex-colored geometry): animated bars, needles
//   Pipeline 3 (QPainter → texture overlay): text readouts, tick marks
//
// Mirrors SpectrumWidget's QRhiWidget pattern exactly:
//   Metal on macOS, D3D11 on Windows.
//
// From Phase 3G-2 plan, modeled on SpectrumWidget.h:58-319
class MeterWidget : public MeterBaseClass {
    Q_OBJECT

public:
    explicit MeterWidget(QWidget* parent = nullptr);
    ~MeterWidget() override;

    QSize sizeHint() const override { return {260, 300}; }

    // --- Item management ---
    void addItem(MeterItem* item);
    void removeItem(MeterItem* item);
    void clearItems();
    QVector<MeterItem*> items() const { return m_items; }

    // --- Data update (called by MeterPoller) ---
    void updateMeterValue(int bindingId, double value);

    // --- Serialization ---
    QString serializeItems() const;
    bool deserializeItems(const QString& data);

protected:
#ifdef NEREUS_GPU_SPECTRUM
    void initialize(QRhiCommandBuffer* cb) override;
    void render(QRhiCommandBuffer* cb) override;
    void releaseResources() override;
#endif
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    // --- Drawing helpers (CPU fallback) ---
    void drawItems(QPainter& p);

    // --- Item data ---
    QVector<MeterItem*> m_items;

#ifdef NEREUS_GPU_SPECTRUM
    bool m_rhiInitialized{false};

    // GPU pipeline init helpers
    void initBackgroundPipeline();
    void initGeometryPipeline();
    void initOverlayPipeline();
    void renderGpuFrame(QRhiCommandBuffer* cb);

    // ---- Background GPU resources (Pipeline 1: textured quad) ----
    QRhiGraphicsPipeline*       m_bgPipeline{nullptr};
    QRhiShaderResourceBindings* m_bgSrb{nullptr};
    QRhiBuffer*                 m_bgVbo{nullptr};
    QRhiTexture*                m_bgGpuTex{nullptr};
    QRhiSampler*                m_bgSampler{nullptr};
    QImage m_bgImage;
    bool   m_bgDirty{true};

    // ---- Geometry GPU resources (Pipeline 2: vertex-colored) ----
    QRhiGraphicsPipeline*       m_geomPipeline{nullptr};
    QRhiShaderResourceBindings* m_geomSrb{nullptr};
    QRhiBuffer*                 m_geomVbo{nullptr};
    // Max vertices for all bar/needle items combined
    static constexpr int kMaxGeomVerts = 4096;
    static constexpr int kGeomVertStride = 6;  // x, y, r, g, b, a
    int m_geomVertCount{0};

    // ---- Overlay GPU resources (Pipeline 3: QPainter → texture) ----
    QRhiGraphicsPipeline*       m_ovPipeline{nullptr};
    QRhiShaderResourceBindings* m_ovSrb{nullptr};
    QRhiBuffer*                 m_ovVbo{nullptr};
    QRhiTexture*                m_ovGpuTex{nullptr};
    QRhiSampler*                m_ovSampler{nullptr};

    // Static overlay (ticks, labels — cached, re-rendered on resize/config)
    QImage m_overlayStatic;
    bool   m_overlayStaticDirty{true};

    // Dynamic overlay (value readouts — re-rendered each meter update)
    QImage m_overlayDynamic;
    bool   m_overlayDynamicDirty{true};

    // Combined overlay for upload
    bool   m_overlayNeedsUpload{true};

    void markOverlayDirty() {
        m_overlayStaticDirty = true;
        m_overlayDynamicDirty = true;
        update();
    }
    void markDynamicDirty() {
        m_overlayDynamicDirty = true;
        update();
    }
#endif
};

} // namespace NereusSDR
```

- [ ] **Step 2: Create MeterWidget implementation — constructor and CPU fallback**

Create `src/gui/meters/MeterWidget.cpp`:

```cpp
#include "MeterWidget.h"
#include "MeterItem.h"
#include "core/LogCategories.h"

#include <QPainter>
#include <QResizeEvent>

#ifdef NEREUS_GPU_SPECTRUM
#include <rhi/qshader.h>
#include <QFile>
#endif

namespace NereusSDR {

MeterWidget::MeterWidget(QWidget* parent)
    : MeterBaseClass(parent)
{
    setMinimumSize(100, 80);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAutoFillBackground(false);

#ifdef NEREUS_GPU_SPECTRUM
    // Platform-specific QRhi backend — mirrors SpectrumWidget.cpp:97-117
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

// --- Item management ---

void MeterWidget::addItem(MeterItem* item)
{
    if (!item || m_items.contains(item)) {
        return;
    }
    item->setParent(this);
    m_items.append(item);
#ifdef NEREUS_GPU_SPECTRUM
    m_bgDirty = true;
    markOverlayDirty();
#endif
    update();
}

void MeterWidget::removeItem(MeterItem* item)
{
    m_items.removeAll(item);
#ifdef NEREUS_GPU_SPECTRUM
    m_bgDirty = true;
    markOverlayDirty();
#endif
    update();
}

void MeterWidget::clearItems()
{
    qDeleteAll(m_items);
    m_items.clear();
#ifdef NEREUS_GPU_SPECTRUM
    m_bgDirty = true;
    markOverlayDirty();
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
#endif
    update();
}

void MeterWidget::resizeEvent(QResizeEvent* event)
{
    MeterBaseClass::resizeEvent(event);
#ifdef NEREUS_GPU_SPECTRUM
    m_bgDirty = true;
    markOverlayDirty();
#endif
}

// --- CPU fallback paint ---

void MeterWidget::paintEvent(QPaintEvent* event)
{
#ifdef NEREUS_GPU_SPECTRUM
    // GPU path handles rendering in render()
    MeterBaseClass::paintEvent(event);
    return;
#else
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), QColor(0x0f, 0x0f, 0x1a));
    drawItems(p);
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

} // namespace NereusSDR
```

- [ ] **Step 3: Add GPU pipeline initialization**

Append to `src/gui/meters/MeterWidget.cpp`, before the closing `} // namespace`:

```cpp
// ============================================================================
// GPU Rendering Path (QRhiWidget)
// Mirrors SpectrumWidget.cpp:1257-1765
// ============================================================================

#ifdef NEREUS_GPU_SPECTRUM

// Fullscreen quad: position (x,y) + texcoord (u,v)
// From SpectrumWidget.cpp:1266 (identical)
static const float kQuadData[] = {
    -1, -1,  0, 1,   // bottom-left
     1, -1,  1, 1,   // bottom-right
    -1,  1,  0, 0,   // top-left
     1,  1,  1, 0,   // top-right
};

static QShader loadShader(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "MeterWidget: cannot load shader" << path;
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

    m_bgVbo = r->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(kQuadData));
    m_bgVbo->create();

    int w = qMax(width(), 64);
    int h = qMax(height(), 64);
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

    QShader vs = loadShader(QStringLiteral(":/shaders/resources/shaders/meter_textured.vert.qsb"));
    QShader fs = loadShader(QStringLiteral(":/shaders/resources/shaders/meter_textured.frag.qsb"));
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

    QShader vs = loadShader(QStringLiteral(":/shaders/resources/shaders/meter_geometry.vert.qsb"));
    QShader fs = loadShader(QStringLiteral(":/shaders/resources/shaders/meter_geometry.frag.qsb"));
    if (!vs.isValid() || !fs.isValid()) { return; }

    QRhiVertexInputLayout layout;
    layout.setBindings({{kGeomVertStride * sizeof(float)}});
    layout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},
        {0, 1, QRhiVertexInputAttribute::Float4, 2 * sizeof(float)},
    });

    // Alpha blending for geometry compositing
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;

    m_geomPipeline = r->newGraphicsPipeline();
    m_geomPipeline->setShaderStages({{QRhiShaderStage::Vertex, vs}, {QRhiShaderStage::Fragment, fs}});
    m_geomPipeline->setVertexInputLayout(layout);
    m_geomPipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    m_geomPipeline->setShaderResourceBindings(m_geomSrb);
    m_geomPipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    m_geomPipeline->setTargetBlends({blend});
    m_geomPipeline->create();
}

void MeterWidget::initOverlayPipeline()
{
    // Mirrors SpectrumWidget.cpp:1338-1397
    QRhi* r = rhi();

    m_ovVbo = r->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(kQuadData));
    m_ovVbo->create();

    int w = qMax(width(), 64);
    int h = qMax(height(), 64);
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

    QShader vs = loadShader(QStringLiteral(":/shaders/resources/shaders/meter_textured.vert.qsb"));
    QShader fs = loadShader(QStringLiteral(":/shaders/resources/shaders/meter_textured.frag.qsb"));
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
        qWarning() << "MeterWidget: QRhi init failed — no GPU backend";
        return;
    }
    qCDebug(lcMeter) << "MeterWidget: QRhi backend:" << r->backendName();

    auto* batch = r->nextResourceUpdateBatch();

    initBackgroundPipeline();
    initGeometryPipeline();
    initOverlayPipeline();

    batch->uploadStaticBuffer(m_bgVbo, kQuadData);
    batch->uploadStaticBuffer(m_ovVbo, kQuadData);

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

    // ---- Background texture (Pipeline 1 — cached, re-rendered on config change) ----
    if (m_bgDirty) {
        const int tw = qMax(w, 64);
        const int th = qMax(h, 64);
        if (m_bgImage.size() != QSize(tw, th)) {
            m_bgImage = QImage(tw, th, QImage::Format_RGBA8888_Premultiplied);
            m_bgGpuTex->setPixelSize(QSize(tw, th));
            m_bgGpuTex->create();
            m_bgSrb->setBindings({
                QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_bgGpuTex, m_bgSampler),
            });
            m_bgSrb->create();
        }
        // Paint background items (SolidColourItem, ImageItem)
        m_bgImage.fill(QColor(0x0f, 0x0f, 0x1a));
        QPainter bp(&m_bgImage);
        for (MeterItem* item : m_items) {
            if (item->renderLayer() == MeterItem::Layer::Background) {
                item->paint(bp, w, h);
            }
        }
        bp.end();
        batch->uploadTexture(m_bgGpuTex, QRhiTextureUploadEntry(0, 0,
            QRhiTextureSubresourceUploadDescription(m_bgImage)));
        m_bgDirty = false;
    }

    // ---- Geometry vertices (Pipeline 2 — updated every meter frame) ----
    {
        QVector<float> verts;
        verts.reserve(kMaxGeomVerts * kGeomVertStride);
        for (MeterItem* item : m_items) {
            if (item->renderLayer() == MeterItem::Layer::Geometry) {
                item->emitVertices(verts, w, h);
            }
        }
        m_geomVertCount = verts.size() / kGeomVertStride;
        if (m_geomVertCount > 0) {
            int bytes = qMin(static_cast<int>(verts.size()), kMaxGeomVerts * kGeomVertStride)
                        * static_cast<int>(sizeof(float));
            batch->updateDynamicBuffer(m_geomVbo, 0, bytes, verts.constData());
        }
    }

    // ---- Overlay texture (Pipeline 3 — split static/dynamic) ----
    {
        const qreal dpr = devicePixelRatioF();
        const int pw = static_cast<int>(w * dpr);
        const int ph = static_cast<int>(h * dpr);
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
            QPainter sp(&m_overlayStatic);
            sp.setRenderHint(QPainter::Antialiasing, false);
            for (MeterItem* item : m_items) {
                if (item->renderLayer() == MeterItem::Layer::OverlayStatic) {
                    item->paint(sp, w, h);
                }
            }
            sp.end();
            m_overlayStaticDirty = false;
            m_overlayDynamicDirty = true;  // force recomposite
        }

        if (m_overlayDynamicDirty) {
            // Composite: start from static, paint dynamic on top
            m_overlayDynamic = m_overlayStatic.copy();
            QPainter dp(&m_overlayDynamic);
            dp.setRenderHint(QPainter::Antialiasing, false);
            for (MeterItem* item : m_items) {
                if (item->renderLayer() == MeterItem::Layer::OverlayDynamic) {
                    item->paint(dp, w, h);
                }
            }
            dp.end();
            m_overlayNeedsUpload = true;
            m_overlayDynamicDirty = false;
        }

        if (m_overlayNeedsUpload) {
            batch->uploadTexture(m_ovGpuTex, QRhiTextureUploadEntry(0, 0,
                QRhiTextureSubresourceUploadDescription(m_overlayDynamic)));
            m_overlayNeedsUpload = false;
        }
    }

    cb->resourceUpdate(batch);

    // ---- Begin render pass ----
    const QColor clearColor(0x0f, 0x0f, 0x1a);  // AetherSDR meter background
    cb->beginPass(renderTarget(), clearColor, {1.0f, 0});

    const QSize outputSize = renderTarget()->pixelSize();

    // Draw background (Pipeline 1)
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

    // Draw geometry (Pipeline 2 — bars, needles)
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

    // Draw overlay (Pipeline 3 — text, ticks)
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
```

- [ ] **Step 4: Add MeterWidget to CMakeLists.txt**

In `CMakeLists.txt`, in the `target_sources` block (after the `src/gui/containers/ContainerManager.cpp` line, around line 197), add:

```cmake
    src/gui/meters/MeterWidget.cpp
```

- [ ] **Step 5: Create stub MeterItem.h (minimal, enough for MeterWidget to compile)**

Create `src/gui/meters/MeterItem.h`:

```cpp
#pragma once

#include <QObject>
#include <QVector>

class QPainter;

namespace NereusSDR {

// Base class for all meter rendering items.
// Each item knows its position (normalized 0-1), data binding, and
// which GPU pipeline layer it renders to.
//
// From Thetis MeterManager.cs clsMeterItem — position, size, data source,
// visual properties, serialization. Rendering uses AetherSDR visual style.
class MeterItem : public QObject {
    Q_OBJECT

public:
    // Which GPU pipeline layer this item renders to.
    enum class Layer {
        Background,      // Pipeline 1: textured quad (cached)
        Geometry,        // Pipeline 2: vertex-colored (per-frame)
        OverlayStatic,   // Pipeline 3: QPainter static (cached)
        OverlayDynamic   // Pipeline 3: QPainter dynamic (per-update)
    };

    explicit MeterItem(QObject* parent = nullptr) : QObject(parent) {}
    ~MeterItem() override = default;

    // --- Position (normalized 0-1, relative to MeterWidget) ---
    // From Thetis clsMeterItem._topLeft / _size (normalized PointF/SizeF)
    float x() const { return m_x; }
    float y() const { return m_y; }
    float itemWidth() const { return m_w; }
    float itemHeight() const { return m_h; }
    void setRect(float x, float y, float w, float h) {
        m_x = x; m_y = y; m_w = w; m_h = h;
    }

    // --- Data binding ---
    int bindingId() const { return m_bindingId; }
    void setBindingId(int id) { m_bindingId = id; }
    double value() const { return m_value; }
    virtual void setValue(double v) { m_value = v; }

    // --- Z-order ---
    // From Thetis clsMeterItem._zOrder
    int zOrder() const { return m_zOrder; }
    void setZOrder(int z) { m_zOrder = z; }

    // --- Rendering ---
    virtual Layer renderLayer() const = 0;
    virtual void paint(QPainter& p, int widgetW, int widgetH) = 0;
    virtual void emitVertices(QVector<float>& verts, int widgetW, int widgetH) {
        Q_UNUSED(verts); Q_UNUSED(widgetW); Q_UNUSED(widgetH);
    }

    // --- Serialization ---
    virtual QString serialize() const;
    virtual bool deserialize(const QString& data);

protected:
    // Pixel rect from normalized coords
    QRect pixelRect(int widgetW, int widgetH) const {
        return QRect(
            static_cast<int>(m_x * widgetW),
            static_cast<int>(m_y * widgetH),
            static_cast<int>(m_w * widgetW),
            static_cast<int>(m_h * widgetH)
        );
    }

    float m_x{0.0f};
    float m_y{0.0f};
    float m_w{1.0f};
    float m_h{1.0f};
    int m_bindingId{-1};
    double m_value{-140.0};
    int m_zOrder{0};
};

} // namespace NereusSDR
```

- [ ] **Step 6: Verify build**

Run:
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build -j$(nproc)
```

Expected: Build succeeds. MeterWidget compiles with stub MeterItem.

- [ ] **Step 7: Commit**

```bash
git add src/gui/meters/MeterWidget.h src/gui/meters/MeterWidget.cpp \
       src/gui/meters/MeterItem.h CMakeLists.txt
git commit -m "Add MeterWidget QRhiWidget shell with 3-pipeline GPU init"
```

---

## Task 4: MeterItem Concrete Types

**Files:**
- Modify: `src/gui/meters/MeterItem.h`
- Create: `src/gui/meters/MeterItem.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add concrete item type declarations to MeterItem.h**

Append to `src/gui/meters/MeterItem.h`, before closing `} // namespace`:

```cpp
// --- Concrete item types ---
// From Thetis MeterManager.cs: clsBarItem, clsText, clsScaleItem,
// clsSolidColour, clsImage

// Solid background fill.
// From Thetis clsSolidColour — renders to Pipeline 1 (Background).
class SolidColourItem : public MeterItem {
    Q_OBJECT
public:
    explicit SolidColourItem(QObject* parent = nullptr) : MeterItem(parent) {}
    void setColour(const QColor& c) { m_colour = c; }
    QColor colour() const { return m_colour; }
    Layer renderLayer() const override { return Layer::Background; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;
private:
    QColor m_colour{0x0f, 0x0f, 0x1a};
};

// Static image.
// From Thetis clsImage — renders to Pipeline 1 (Background).
class ImageItem : public MeterItem {
    Q_OBJECT
public:
    explicit ImageItem(QObject* parent = nullptr) : MeterItem(parent) {}
    void setImage(const QImage& img) { m_image = img; }
    void setImagePath(const QString& path);
    Layer renderLayer() const override { return Layer::Background; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;
private:
    QImage m_image;
    QString m_imagePath;
};

// Horizontal or vertical bar meter.
// From Thetis clsBarItem — renders to Pipeline 2 (Geometry).
// Bar fill, peak marker, attack/decay smoothing.
class BarItem : public MeterItem {
    Q_OBJECT
public:
    enum class Orientation { Horizontal, Vertical };

    explicit BarItem(QObject* parent = nullptr) : MeterItem(parent) {}

    Orientation orientation() const { return m_orientation; }
    void setOrientation(Orientation o) { m_orientation = o; }

    // dBm range mapping
    // From Thetis clsBarItem: MinValue / MaxValue
    void setRange(double minVal, double maxVal) { m_minVal = minVal; m_maxVal = maxVal; }
    double minVal() const { return m_minVal; }
    double maxVal() const { return m_maxVal; }

    // Bar colors (AetherSDR style)
    void setBarColor(const QColor& c) { m_barColor = c; }
    void setBarRedColor(const QColor& c) { m_barRedColor = c; }
    void setRedThreshold(double t) { m_redThreshold = t; }

    // Smoothing — from Thetis clsBarItem: AttackRatio=0.8, DecayRatio=0.2
    void setAttackRatio(float a) { m_attackRatio = a; }
    void setDecayRatio(float d) { m_decayRatio = d; }

    void setValue(double v) override;

    Layer renderLayer() const override { return Layer::Geometry; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    void emitVertices(QVector<float>& verts, int widgetW, int widgetH) override;

    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    Orientation m_orientation{Orientation::Horizontal};
    double m_minVal{-140.0};
    double m_maxVal{0.0};
    double m_smoothedValue{-140.0};
    QColor m_barColor{0x00, 0xb4, 0xd8};      // AetherSDR accent cyan
    QColor m_barRedColor{0xff, 0x44, 0x44};    // AetherSDR red zone
    double m_redThreshold{1000.0};             // disabled by default (above max)
    // From Thetis MeterManager.cs AttackRatio/DecayRatio defaults
    float m_attackRatio{0.8f};
    float m_decayRatio{0.2f};
};

// Scale with tick marks and labels.
// From Thetis clsScaleItem — renders to Pipeline 3 (OverlayStatic).
class ScaleItem : public MeterItem {
    Q_OBJECT
public:
    enum class Orientation { Horizontal, Vertical };

    explicit ScaleItem(QObject* parent = nullptr) : MeterItem(parent) {}

    Orientation orientation() const { return m_orientation; }
    void setOrientation(Orientation o) { m_orientation = o; }
    void setRange(double minVal, double maxVal) { m_minVal = minVal; m_maxVal = maxVal; }
    void setMajorTicks(int count) { m_majorTicks = count; }
    void setMinorTicks(int count) { m_minorTicks = count; }
    void setTickColor(const QColor& c) { m_tickColor = c; }
    void setLabelColor(const QColor& c) { m_labelColor = c; }
    void setFontSize(int pt) { m_fontSize = pt; }

    Layer renderLayer() const override { return Layer::OverlayStatic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;

    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    Orientation m_orientation{Orientation::Horizontal};
    double m_minVal{-140.0};
    double m_maxVal{0.0};
    int m_majorTicks{7};
    int m_minorTicks{5};
    QColor m_tickColor{0xc8, 0xd8, 0xe8};     // AetherSDR text color
    QColor m_labelColor{0x80, 0x90, 0xa0};     // AetherSDR dim text
    int m_fontSize{10};
};

// Text readout (dynamic value display).
// From Thetis clsText — renders to Pipeline 3 (OverlayDynamic).
class TextItem : public MeterItem {
    Q_OBJECT
public:
    explicit TextItem(QObject* parent = nullptr) : MeterItem(parent) {}

    void setLabel(const QString& label) { m_label = label; }
    QString label() const { return m_label; }
    void setTextColor(const QColor& c) { m_textColor = c; }
    void setFontSize(int pt) { m_fontSize = pt; }
    void setBold(bool b) { m_bold = b; }
    void setSuffix(const QString& s) { m_suffix = s; }
    void setDecimals(int d) { m_decimals = d; }

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;

    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    QString m_label;
    QColor m_textColor{0xc8, 0xd8, 0xe8};     // AetherSDR text color
    int m_fontSize{13};
    bool m_bold{true};
    QString m_suffix{QStringLiteral(" dBm")};
    int m_decimals{1};
};
```

- [ ] **Step 2: Implement concrete item types**

Create `src/gui/meters/MeterItem.cpp`:

```cpp
#include "MeterItem.h"

#include <QPainter>
#include <QImage>
#include <QStringList>

#include <cmath>
#include <algorithm>

namespace NereusSDR {

// ============================================================================
// MeterItem base serialization
// ============================================================================

QString MeterItem::serialize() const
{
    // Format: x|y|w|h|bindingId|zOrder
    QStringList p;
    p << QString::number(static_cast<double>(m_x));
    p << QString::number(static_cast<double>(m_y));
    p << QString::number(static_cast<double>(m_w));
    p << QString::number(static_cast<double>(m_h));
    p << QString::number(m_bindingId);
    p << QString::number(m_zOrder);
    return p.join(QLatin1Char('|'));
}

bool MeterItem::deserialize(const QString& data)
{
    QStringList p = data.split(QLatin1Char('|'));
    if (p.size() < 6) { return false; }
    m_x = p[0].toFloat();
    m_y = p[1].toFloat();
    m_w = p[2].toFloat();
    m_h = p[3].toFloat();
    m_bindingId = p[4].toInt();
    m_zOrder = p[5].toInt();
    return true;
}

// ============================================================================
// SolidColourItem
// ============================================================================

void SolidColourItem::paint(QPainter& p, int widgetW, int widgetH)
{
    p.fillRect(pixelRect(widgetW, widgetH), m_colour);
}

QString SolidColourItem::serialize() const
{
    return QStringLiteral("SOLID|") + MeterItem::serialize()
           + QLatin1Char('|') + m_colour.name(QColor::HexArgb);
}

bool SolidColourItem::deserialize(const QString& data)
{
    // SOLID|x|y|w|h|bindingId|zOrder|#aarrggbb
    QStringList p = data.split(QLatin1Char('|'));
    if (p.size() < 8 || p[0] != QStringLiteral("SOLID")) { return false; }
    QString base = p.mid(1, 6).join(QLatin1Char('|'));
    if (!MeterItem::deserialize(base)) { return false; }
    m_colour = QColor(p[7]);
    return true;
}

// ============================================================================
// ImageItem
// ============================================================================

void ImageItem::setImagePath(const QString& path)
{
    m_imagePath = path;
    m_image.load(path);
}

void ImageItem::paint(QPainter& p, int widgetW, int widgetH)
{
    if (m_image.isNull()) { return; }
    p.drawImage(pixelRect(widgetW, widgetH), m_image);
}

QString ImageItem::serialize() const
{
    return QStringLiteral("IMAGE|") + MeterItem::serialize()
           + QLatin1Char('|') + m_imagePath;
}

bool ImageItem::deserialize(const QString& data)
{
    QStringList p = data.split(QLatin1Char('|'));
    if (p.size() < 8 || p[0] != QStringLiteral("IMAGE")) { return false; }
    QString base = p.mid(1, 6).join(QLatin1Char('|'));
    if (!MeterItem::deserialize(base)) { return false; }
    setImagePath(p[7]);
    return true;
}

// ============================================================================
// BarItem
// ============================================================================

void BarItem::setValue(double v)
{
    // Exponential smoothing — from Thetis MeterManager.cs AttackRatio/DecayRatio
    if (v > m_smoothedValue) {
        m_smoothedValue = m_attackRatio * v + (1.0 - m_attackRatio) * m_smoothedValue;
    } else {
        m_smoothedValue = m_decayRatio * v + (1.0 - m_decayRatio) * m_smoothedValue;
    }
    MeterItem::setValue(v);
}

void BarItem::paint(QPainter& p, int widgetW, int widgetH)
{
    // CPU fallback — draw filled rect
    QRect r = pixelRect(widgetW, widgetH);
    double frac = std::clamp((m_smoothedValue - m_minVal) / (m_maxVal - m_minVal), 0.0, 1.0);

    if (m_orientation == Orientation::Horizontal) {
        int fillW = static_cast<int>(r.width() * frac);
        bool inRed = m_smoothedValue >= m_redThreshold;
        p.fillRect(r.x(), r.y(), fillW, r.height(),
                   inRed ? m_barRedColor : m_barColor);
    } else {
        int fillH = static_cast<int>(r.height() * frac);
        bool inRed = m_smoothedValue >= m_redThreshold;
        p.fillRect(r.x(), r.bottom() - fillH, r.width(), fillH,
                   inRed ? m_barRedColor : m_barColor);
    }
}

void BarItem::emitVertices(QVector<float>& verts, int widgetW, int widgetH)
{
    // Emit a triangle strip quad for the filled portion of the bar.
    // NDC: (-1,-1) = bottom-left, (1,1) = top-right.
    double frac = std::clamp((m_smoothedValue - m_minVal) / (m_maxVal - m_minVal), 0.0, 1.0);

    // Convert normalized item rect to NDC
    float ndcL = m_x * 2.0f - 1.0f;
    float ndcR = (m_x + m_w) * 2.0f - 1.0f;
    float ndcT = 1.0f - m_y * 2.0f;
    float ndcB = 1.0f - (m_y + m_h) * 2.0f;

    Q_UNUSED(widgetW);
    Q_UNUSED(widgetH);

    // Determine fill color
    bool inRed = m_smoothedValue >= m_redThreshold;
    QColor c = inRed ? m_barRedColor : m_barColor;
    float cr = c.redF();
    float cg = c.greenF();
    float cb = c.blueF();
    float ca = c.alphaF();

    if (m_orientation == Orientation::Horizontal) {
        float fillR = ndcL + (ndcR - ndcL) * static_cast<float>(frac);
        // Triangle strip: BL, BR, TL, TR
        float quad[] = {
            ndcL,  ndcB,  cr, cg, cb, ca,
            fillR, ndcB,  cr, cg, cb, ca,
            ndcL,  ndcT,  cr, cg, cb, ca,
            fillR, ndcT,  cr, cg, cb, ca,
        };
        verts.append(quad, quad + 24);
    } else {
        float fillT = ndcB + (ndcT - ndcB) * static_cast<float>(frac);
        float quad[] = {
            ndcL, ndcB,  cr, cg, cb, ca,
            ndcR, ndcB,  cr, cg, cb, ca,
            ndcL, fillT, cr, cg, cb, ca,
            ndcR, fillT, cr, cg, cb, ca,
        };
        verts.append(quad, quad + 24);
    }
}

QString BarItem::serialize() const
{
    QStringList p;
    p << QStringLiteral("BAR");
    p << MeterItem::serialize();
    p << QString::number(static_cast<int>(m_orientation));
    p << QString::number(m_minVal);
    p << QString::number(m_maxVal);
    p << m_barColor.name(QColor::HexArgb);
    p << m_barRedColor.name(QColor::HexArgb);
    p << QString::number(m_redThreshold);
    p << QString::number(static_cast<double>(m_attackRatio));
    p << QString::number(static_cast<double>(m_decayRatio));
    return p.join(QLatin1Char('|'));
}

bool BarItem::deserialize(const QString& data)
{
    QStringList p = data.split(QLatin1Char('|'));
    if (p.size() < 16 || p[0] != QStringLiteral("BAR")) { return false; }
    QString base = p.mid(1, 6).join(QLatin1Char('|'));
    if (!MeterItem::deserialize(base)) { return false; }
    m_orientation = static_cast<Orientation>(p[7].toInt());
    m_minVal = p[8].toDouble();
    m_maxVal = p[9].toDouble();
    m_barColor = QColor(p[10]);
    m_barRedColor = QColor(p[11]);
    m_redThreshold = p[12].toDouble();
    m_attackRatio = p[13].toFloat();
    m_decayRatio = p[14].toFloat();
    return true;
}

// ============================================================================
// ScaleItem
// ============================================================================

void ScaleItem::paint(QPainter& p, int widgetW, int widgetH)
{
    QRect r = pixelRect(widgetW, widgetH);

    QPen tickPen(m_tickColor, 1.5);
    p.setPen(tickPen);

    QFont font = p.font();
    font.setPixelSize(m_fontSize);
    p.setFont(font);

    double range = m_maxVal - m_minVal;
    if (range <= 0) { return; }

    if (m_orientation == Orientation::Horizontal) {
        int y1 = r.top();
        int y2 = r.top() + r.height() / 3;       // major tick height
        int y3 = r.top() + r.height() / 5;       // minor tick height
        int yLabel = r.bottom();

        // Major ticks
        for (int i = 0; i <= m_majorTicks; ++i) {
            double frac = static_cast<double>(i) / m_majorTicks;
            int x = r.left() + static_cast<int>(r.width() * frac);
            p.drawLine(x, y1, x, y2);

            double val = m_minVal + range * frac;
            QString label = QString::number(val, 'f', 0);
            p.setPen(m_labelColor);
            QRect textRect(x - 20, y2 + 2, 40, yLabel - y2 - 2);
            p.drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, label);
            p.setPen(tickPen);
        }

        // Minor ticks
        int totalMinor = m_majorTicks * m_minorTicks;
        for (int i = 0; i <= totalMinor; ++i) {
            if (i % m_minorTicks == 0) { continue; }  // skip major positions
            double frac = static_cast<double>(i) / totalMinor;
            int x = r.left() + static_cast<int>(r.width() * frac);
            p.drawLine(x, y1, x, y3);
        }
    } else {
        // Vertical scale
        int x1 = r.right();
        int x2 = r.right() - r.width() / 3;
        int x3 = r.right() - r.width() / 5;

        for (int i = 0; i <= m_majorTicks; ++i) {
            double frac = static_cast<double>(i) / m_majorTicks;
            int y = r.bottom() - static_cast<int>(r.height() * frac);
            p.drawLine(x2, y, x1, y);

            double val = m_minVal + range * frac;
            QString label = QString::number(val, 'f', 0);
            p.setPen(m_labelColor);
            QRect textRect(r.left(), y - m_fontSize / 2, x2 - r.left() - 4, m_fontSize + 2);
            p.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, label);
            p.setPen(tickPen);
        }

        int totalMinor = m_majorTicks * m_minorTicks;
        for (int i = 0; i <= totalMinor; ++i) {
            if (i % m_minorTicks == 0) { continue; }
            double frac = static_cast<double>(i) / totalMinor;
            int y = r.bottom() - static_cast<int>(r.height() * frac);
            p.drawLine(x3, y, x1, y);
        }
    }
}

QString ScaleItem::serialize() const
{
    QStringList p;
    p << QStringLiteral("SCALE");
    p << MeterItem::serialize();
    p << QString::number(static_cast<int>(m_orientation));
    p << QString::number(m_minVal);
    p << QString::number(m_maxVal);
    p << QString::number(m_majorTicks);
    p << QString::number(m_minorTicks);
    p << m_tickColor.name(QColor::HexArgb);
    p << m_labelColor.name(QColor::HexArgb);
    p << QString::number(m_fontSize);
    return p.join(QLatin1Char('|'));
}

bool ScaleItem::deserialize(const QString& data)
{
    QStringList p = data.split(QLatin1Char('|'));
    if (p.size() < 16 || p[0] != QStringLiteral("SCALE")) { return false; }
    QString base = p.mid(1, 6).join(QLatin1Char('|'));
    if (!MeterItem::deserialize(base)) { return false; }
    m_orientation = static_cast<Orientation>(p[7].toInt());
    m_minVal = p[8].toDouble();
    m_maxVal = p[9].toDouble();
    m_majorTicks = p[10].toInt();
    m_minorTicks = p[11].toInt();
    m_tickColor = QColor(p[12]);
    m_labelColor = QColor(p[13]);
    m_fontSize = p[14].toInt();
    return true;
}

// ============================================================================
// TextItem
// ============================================================================

void TextItem::paint(QPainter& p, int widgetW, int widgetH)
{
    QRect r = pixelRect(widgetW, widgetH);

    QFont font = p.font();
    font.setPixelSize(m_fontSize);
    font.setBold(m_bold);
    p.setFont(font);
    p.setPen(m_textColor);

    // Format value with suffix
    QString text;
    if (m_bindingId >= 0) {
        text = QString::number(m_value, 'f', m_decimals) + m_suffix;
    } else {
        text = m_label;
    }
    p.drawText(r, Qt::AlignCenter, text);
}

QString TextItem::serialize() const
{
    QStringList p;
    p << QStringLiteral("TEXT");
    p << MeterItem::serialize();
    p << m_label;
    p << m_textColor.name(QColor::HexArgb);
    p << QString::number(m_fontSize);
    p << (m_bold ? QStringLiteral("1") : QStringLiteral("0"));
    p << m_suffix;
    p << QString::number(m_decimals);
    return p.join(QLatin1Char('|'));
}

bool TextItem::deserialize(const QString& data)
{
    QStringList p = data.split(QLatin1Char('|'));
    if (p.size() < 14 || p[0] != QStringLiteral("TEXT")) { return false; }
    QString base = p.mid(1, 6).join(QLatin1Char('|'));
    if (!MeterItem::deserialize(base)) { return false; }
    m_label = p[7];
    m_textColor = QColor(p[8]);
    m_fontSize = p[9].toInt();
    m_bold = (p[10] == QStringLiteral("1"));
    m_suffix = p[11];
    m_decimals = p[12].toInt();
    return true;
}

} // namespace NereusSDR
```

- [ ] **Step 3: Add MeterItem.cpp to CMakeLists.txt**

In `CMakeLists.txt`, after the `src/gui/meters/MeterWidget.cpp` line, add:

```cmake
    src/gui/meters/MeterItem.cpp
```

- [ ] **Step 4: Verify build**

Run:
```bash
cmake --build build -j$(nproc)
```

Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add src/gui/meters/MeterItem.h src/gui/meters/MeterItem.cpp CMakeLists.txt
git commit -m "Add MeterItem base class and concrete types (Bar, Text, Scale, SolidColour, Image)"
```

---

## Task 5: ItemGroup

**Files:**
- Create: `src/gui/meters/ItemGroup.h`
- Create: `src/gui/meters/ItemGroup.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create ItemGroup header**

Create `src/gui/meters/ItemGroup.h`:

```cpp
#pragma once

#include "MeterItem.h"

#include <QString>
#include <QVector>

namespace NereusSDR {

// Composites N MeterItems into a named, reusable meter preset.
// From Thetis MeterManager.cs clsItemGroup — groups items into functional
// meter types (S-Meter, Power, SWR, etc.).
//
// The group owns its items and manages their lifecycle. Items within
// a group use positions relative to the group's own rect.
class ItemGroup : public QObject {
    Q_OBJECT

public:
    explicit ItemGroup(const QString& name, QObject* parent = nullptr);
    ~ItemGroup() override;

    QString name() const { return m_name; }
    void setName(const QString& name) { m_name = name; }

    // Group rect (normalized 0-1, relative to MeterWidget)
    float x() const { return m_x; }
    float y() const { return m_y; }
    float groupWidth() const { return m_w; }
    float groupHeight() const { return m_h; }
    void setRect(float x, float y, float w, float h);

    // --- Item management ---
    void addItem(MeterItem* item);
    void removeItem(MeterItem* item);
    QVector<MeterItem*> items() const { return m_items; }

    // --- Serialization ---
    QString serialize() const;
    static ItemGroup* deserialize(const QString& data, QObject* parent = nullptr);

    // --- Preset factory ---
    // Creates a horizontal bar meter group with scale and readout.
    // AetherSDR visual style, Thetis data binding.
    static ItemGroup* createHBarPreset(int bindingId, double minVal, double maxVal,
                                        const QString& name, QObject* parent = nullptr);

private:
    QString m_name;
    float m_x{0.0f};
    float m_y{0.0f};
    float m_w{1.0f};
    float m_h{1.0f};
    QVector<MeterItem*> m_items;
};

} // namespace NereusSDR
```

- [ ] **Step 2: Implement ItemGroup**

Create `src/gui/meters/ItemGroup.cpp`:

```cpp
#include "ItemGroup.h"

#include <QStringList>

namespace NereusSDR {

ItemGroup::ItemGroup(const QString& name, QObject* parent)
    : QObject(parent)
    , m_name(name)
{
}

ItemGroup::~ItemGroup()
{
    qDeleteAll(m_items);
}

void ItemGroup::setRect(float x, float y, float w, float h)
{
    m_x = x;
    m_y = y;
    m_w = w;
    m_h = h;
}

void ItemGroup::addItem(MeterItem* item)
{
    if (!item || m_items.contains(item)) { return; }
    item->setParent(this);
    m_items.append(item);
}

void ItemGroup::removeItem(MeterItem* item)
{
    m_items.removeAll(item);
}

QString ItemGroup::serialize() const
{
    // GROUP|name|x|y|w|h|itemCount|item1|item2|...
    QStringList p;
    p << QStringLiteral("GROUP");
    p << m_name;
    p << QString::number(static_cast<double>(m_x));
    p << QString::number(static_cast<double>(m_y));
    p << QString::number(static_cast<double>(m_w));
    p << QString::number(static_cast<double>(m_h));
    p << QString::number(m_items.size());
    for (const MeterItem* item : m_items) {
        p << item->serialize();
    }
    return p.join(QLatin1Char('\n'));
}

ItemGroup* ItemGroup::deserialize(const QString& data, QObject* parent)
{
    QStringList lines = data.split(QLatin1Char('\n'));
    if (lines.size() < 7) { return nullptr; }

    QStringList header = lines[0].split(QLatin1Char('|'));
    if (header.size() < 2 || header[0] != QStringLiteral("GROUP")) { return nullptr; }

    // Header line: GROUP|name, then next lines: x|y|w|h|itemCount
    // For simplicity, first line is "GROUP", fields on subsequent lines
    QString name = lines[1];
    float x = lines[2].toFloat();
    float y = lines[3].toFloat();
    float w = lines[4].toFloat();
    float h = lines[5].toFloat();
    int count = lines[6].toInt();

    auto* group = new ItemGroup(name, parent);
    group->setRect(x, y, w, h);

    for (int i = 0; i < count && (7 + i) < lines.size(); ++i) {
        const QString& itemData = lines[7 + i];
        if (itemData.isEmpty()) { continue; }

        // Detect type from first field
        QString type = itemData.section(QLatin1Char('|'), 0, 0);
        MeterItem* item = nullptr;
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
        }

        if (item && item->deserialize(itemData)) {
            group->addItem(item);
        } else {
            delete item;
        }
    }

    return group;
}

ItemGroup* ItemGroup::createHBarPreset(int bindingId, double minVal, double maxVal,
                                        const QString& name, QObject* parent)
{
    // Creates a horizontal bar meter group matching AetherSDR visual style.
    // Layout (within group rect, all normalized 0-1):
    //   Top 20%: label text (static) + value readout (dynamic)
    //   Mid 30%: horizontal bar
    //   Bot 50%: horizontal scale with ticks
    auto* group = new ItemGroup(name, parent);

    // Background
    auto* bg = new SolidColourItem();
    bg->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    bg->setColour(QColor(0x0f, 0x0f, 0x1a));  // AetherSDR meter bg
    bg->setZOrder(0);
    group->addItem(bg);

    // Label (static overlay)
    auto* label = new TextItem();
    label->setRect(0.02f, 0.0f, 0.5f, 0.2f);
    label->setLabel(name);
    label->setBindingId(-1);  // no data binding — static label
    label->setTextColor(QColor(0x80, 0x90, 0xa0));  // AetherSDR dim text
    label->setFontSize(10);
    label->setBold(false);
    label->setZOrder(10);
    group->addItem(label);

    // Value readout (dynamic overlay)
    auto* readout = new TextItem();
    readout->setRect(0.5f, 0.0f, 0.48f, 0.2f);
    readout->setBindingId(bindingId);
    readout->setTextColor(QColor(0xc8, 0xd8, 0xe8));  // AetherSDR text
    readout->setFontSize(13);
    readout->setBold(true);
    readout->setSuffix(QStringLiteral(" dBm"));
    readout->setDecimals(1);
    readout->setZOrder(10);
    group->addItem(readout);

    // Bar (geometry pipeline)
    auto* bar = new BarItem();
    bar->setRect(0.02f, 0.22f, 0.96f, 0.28f);
    bar->setOrientation(BarItem::Orientation::Horizontal);
    bar->setRange(minVal, maxVal);
    bar->setBindingId(bindingId);
    bar->setBarColor(QColor(0x00, 0xb4, 0xd8));      // AetherSDR accent cyan
    bar->setBarRedColor(QColor(0xff, 0x44, 0x44));    // AetherSDR red
    bar->setRedThreshold(maxVal * 0.9);
    bar->setZOrder(5);
    group->addItem(bar);

    // Scale (static overlay)
    auto* scale = new ScaleItem();
    scale->setRect(0.02f, 0.52f, 0.96f, 0.46f);
    scale->setOrientation(ScaleItem::Orientation::Horizontal);
    scale->setRange(minVal, maxVal);
    scale->setMajorTicks(7);
    scale->setMinorTicks(5);
    scale->setTickColor(QColor(0xc8, 0xd8, 0xe8));
    scale->setLabelColor(QColor(0x80, 0x90, 0xa0));
    scale->setFontSize(9);
    scale->setZOrder(10);
    group->addItem(scale);

    return group;
}

} // namespace NereusSDR
```

- [ ] **Step 3: Add ItemGroup.cpp to CMakeLists.txt**

In `CMakeLists.txt`, after the `src/gui/meters/MeterItem.cpp` line, add:

```cmake
    src/gui/meters/ItemGroup.cpp
```

- [ ] **Step 4: Verify build**

Run:
```bash
cmake --build build -j$(nproc)
```

Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add src/gui/meters/ItemGroup.h src/gui/meters/ItemGroup.cpp CMakeLists.txt
git commit -m "Add ItemGroup composite for reusable meter presets"
```

---

## Task 6: MeterPoller (WDSP Data Binding)

**Files:**
- Create: `src/gui/meters/MeterPoller.h`
- Create: `src/gui/meters/MeterPoller.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create MeterPoller header**

Create `src/gui/meters/MeterPoller.h`:

```cpp
#pragma once

#include "core/WdspTypes.h"

#include <QObject>
#include <QTimer>

namespace NereusSDR {

class RxChannel;
class MeterWidget;

// Binding IDs for meter data sources.
// These map to WDSP meter types. Future phases add TX meter bindings.
// From Thetis MeterManager.cs Reading enum (subset for Phase 3G-2).
namespace MeterBinding {
    constexpr int SignalPeak   = 0;    // RxMeterType::SignalPeak
    constexpr int SignalAvg    = 1;    // RxMeterType::SignalAvg
    constexpr int AdcPeak      = 2;    // RxMeterType::AdcPeak
    constexpr int AdcAvg       = 3;    // RxMeterType::AdcAvg
    constexpr int AgcGain      = 4;    // RxMeterType::AgcGain
    constexpr int AgcPeak      = 5;    // RxMeterType::AgcPeak
    constexpr int AgcAvg       = 6;    // RxMeterType::AgcAvg
}

// Polls WDSP RX meters at a fixed interval and pushes values to
// bound MeterWidgets.
//
// From Thetis MeterManager.cs _meterThread + UpdateMeters() pattern.
// Simplified: single QTimer on main thread instead of background thread,
// since GetRXAMeter is lock-free and returns immediately.
class MeterPoller : public QObject {
    Q_OBJECT

public:
    explicit MeterPoller(QObject* parent = nullptr);
    ~MeterPoller() override;

    // Set the WDSP channel to poll.
    // RxChannel::getMeter() is lock-free; safe to call from main thread.
    void setRxChannel(RxChannel* channel);

    // Register a MeterWidget to receive updates.
    void addTarget(MeterWidget* widget);
    void removeTarget(MeterWidget* widget);

    // Polling interval in ms (default 100 = 10 fps).
    // From Thetis MeterManager.cs default UpdateInterval = 100ms
    void setInterval(int ms);
    int interval() const;

    void start();
    void stop();

private slots:
    void poll();

private:
    QTimer m_timer;
    RxChannel* m_rxChannel{nullptr};
    QVector<MeterWidget*> m_targets;
};

} // namespace NereusSDR
```

- [ ] **Step 2: Implement MeterPoller**

Create `src/gui/meters/MeterPoller.cpp`:

```cpp
#include "MeterPoller.h"
#include "MeterWidget.h"
#include "core/RxChannel.h"
#include "core/LogCategories.h"

namespace NereusSDR {

MeterPoller::MeterPoller(QObject* parent)
    : QObject(parent)
{
    m_timer.setInterval(100);  // 10 fps default
    connect(&m_timer, &QTimer::timeout, this, &MeterPoller::poll);
}

MeterPoller::~MeterPoller() = default;

void MeterPoller::setRxChannel(RxChannel* channel)
{
    m_rxChannel = channel;
    qCDebug(lcMeter) << "MeterPoller: RxChannel set, channelId:"
                      << (channel ? channel->channelId() : -1);
}

void MeterPoller::addTarget(MeterWidget* widget)
{
    if (widget && !m_targets.contains(widget)) {
        m_targets.append(widget);
    }
}

void MeterPoller::removeTarget(MeterWidget* widget)
{
    m_targets.removeAll(widget);
}

void MeterPoller::setInterval(int ms)
{
    m_timer.setInterval(ms);
}

int MeterPoller::interval() const
{
    return m_timer.interval();
}

void MeterPoller::start()
{
    if (!m_rxChannel) {
        qCWarning(lcMeter) << "MeterPoller: cannot start without RxChannel";
        return;
    }
    m_timer.start();
    qCDebug(lcMeter) << "MeterPoller: started at" << m_timer.interval() << "ms interval";
}

void MeterPoller::stop()
{
    m_timer.stop();
    qCDebug(lcMeter) << "MeterPoller: stopped";
}

void MeterPoller::poll()
{
    if (!m_rxChannel) { return; }

    // Poll all RX meter types.
    // RxChannel::getMeter() calls GetRXAMeter() which is lock-free.
    // From Thetis MeterManager.cs UpdateMeters() — polls all bound readings.
    for (int bindingId = MeterBinding::SignalPeak;
         bindingId <= MeterBinding::AgcAvg; ++bindingId) {
        double value = m_rxChannel->getMeter(static_cast<RxMeterType>(bindingId));
        for (MeterWidget* target : m_targets) {
            target->updateMeterValue(bindingId, value);
        }
    }
}

} // namespace NereusSDR
```

- [ ] **Step 3: Add MeterPoller.cpp to CMakeLists.txt**

In `CMakeLists.txt`, after the `src/gui/meters/ItemGroup.cpp` line, add:

```cmake
    src/gui/meters/MeterPoller.cpp
```

- [ ] **Step 4: Verify build**

Run:
```bash
cmake --build build -j$(nproc)
```

Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add src/gui/meters/MeterPoller.h src/gui/meters/MeterPoller.cpp CMakeLists.txt
git commit -m "Add MeterPoller for WDSP meter polling at 100ms intervals"
```

---

## Task 7: MeterWidget Serialization

**Files:**
- Modify: `src/gui/meters/MeterWidget.h` (already has declarations)
- Modify: `src/gui/meters/MeterWidget.cpp`

- [ ] **Step 1: Implement serializeItems and deserializeItems**

Add to `src/gui/meters/MeterWidget.cpp`, in the non-`#ifdef` section (after `drawItems`), before the GPU `#ifdef` block:

```cpp
// --- Serialization ---
// Stored in AppSettings as MeterContent_{containerId}
// Format: newline-separated item serializations

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
        }

        if (item && item->deserialize(line)) {
            addItem(item);
        } else {
            delete item;
        }
    }
    return !m_items.isEmpty();
}
```

- [ ] **Step 2: Add `#include "ItemGroup.h"` to MeterWidget.cpp includes**

At the top of `MeterWidget.cpp`, after `#include "MeterItem.h"`, add:

```cpp
#include "ItemGroup.h"
```

- [ ] **Step 3: Verify build**

Run:
```bash
cmake --build build -j$(nproc)
```

Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/gui/meters/MeterWidget.cpp
git commit -m "Add MeterWidget item serialization (pipe-delimited, AppSettings compatible)"
```

---

## Task 8: Wire Container #0 with Live Meter

**Files:**
- Modify: `src/gui/MainWindow.h`
- Modify: `src/gui/MainWindow.cpp`

This is the integration task that connects everything and satisfies the verification target.

- [ ] **Step 1: Add forward declarations and member to MainWindow.h**

In `src/gui/MainWindow.h`, add forward declarations (after line 18, `class ContainerManager;`):

```cpp
class MeterWidget;
class MeterPoller;
```

Add member variables (after `m_vDelta` on line 68):

```cpp
    // Meter system (Phase 3G-2)
    MeterWidget* m_meterWidget{nullptr};
    MeterPoller* m_meterPoller{nullptr};
    void populateDefaultMeter();
```

- [ ] **Step 2: Implement populateDefaultMeter and wire to Container #0**

In `src/gui/MainWindow.cpp`, add includes at the top (after existing includes):

```cpp
#include "meters/MeterWidget.h"
#include "meters/MeterItem.h"
#include "meters/ItemGroup.h"
#include "meters/MeterPoller.h"
```

Add the `populateDefaultMeter()` method (after `createDefaultContainers()`):

```cpp
void MainWindow::populateDefaultMeter()
{
    // Phase 3G-2: Install MeterWidget into Container #0.
    // Creates a horizontal bar meter bound to WDSP SignalPeak.
    ContainerWidget* c0 = m_containerManager->panelContainer();
    if (!c0) {
        qCWarning(lcContainer) << "No panel container for meter widget";
        return;
    }

    m_meterWidget = new MeterWidget();

    // Create default H_BAR preset (AetherSDR visual style)
    // Verification target: live horizontal bar meter bound to SIGNAL_STRENGTH
    ItemGroup* group = ItemGroup::createHBarPreset(
        MeterBinding::SignalPeak, -140.0, 0.0,
        QStringLiteral("Signal"), m_meterWidget);

    // Install group items into MeterWidget
    for (MeterItem* item : group->items()) {
        m_meterWidget->addItem(item);
    }

    // Install MeterWidget as Container #0 content
    c0->setContent(m_meterWidget);

    qCDebug(lcMeter) << "Installed MeterWidget in Container #0 with signal meter";
}
```

- [ ] **Step 3: Wire MeterPoller to RxChannel**

In `MainWindow::buildUI()`, after the FFT wiring block (after `m_fftThread->start();` around line 220), add:

```cpp
    // --- Meter Poller (Phase 3G-2) ---
    m_meterPoller = new MeterPoller(this);

    // Populate Container #0 with default meter
    populateDefaultMeter();

    if (m_meterWidget) {
        m_meterPoller->addTarget(m_meterWidget);
    }

    // Wire RxChannel to poller when WDSP creates it.
    // RxChannel is created in RadioModel::onConnectionStateChanged() after
    // connect. We start polling as soon as the channel exists.
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int index) {
        if (index == 0 && m_radioModel->wdspEngine()) {
            RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
            if (rxCh) {
                m_meterPoller->setRxChannel(rxCh);
                m_meterPoller->start();
                qCDebug(lcMeter) << "MeterPoller started on RxChannel 0";
            }
        }
    });
```

Also add needed include for RxChannel if not already present:

```cpp
#include "core/RxChannel.h"
```

And add includes for MeterBinding namespace:

The `MeterBinding` constants are in `MeterPoller.h` which is already included.

- [ ] **Step 4: Verify build**

Run:
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build -j$(nproc)
```

Expected: Build succeeds cleanly on the current platform.

- [ ] **Step 5: Run and verify**

Launch the app:
```bash
./build/NereusSDR
```

Expected behavior:
1. Container #0 (right panel) shows the MeterWidget instead of the placeholder label.
2. The MeterWidget displays a horizontal bar with a scale underneath and value readout on top.
3. Before connecting to a radio, the bar shows -140 dBm (minimum).
4. After connecting to the ANAN-G2, the bar animates with live signal strength at ~10fps.
5. All rendering is through the QRhi GPU pipeline (Metal on macOS, D3D11 on Windows).

- [ ] **Step 6: Commit**

```bash
git add src/gui/MainWindow.h src/gui/MainWindow.cpp
git commit -m "Wire MeterWidget + MeterPoller into Container #0 with live signal meter"
```

---

## Task 9: Cross-Platform Verification

- [ ] **Step 1: Build on current platform**

Run:
```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build -j$(nproc)
```

Expected: Zero warnings from meter files. Build succeeds.

- [ ] **Step 2: Verify no Phase 3G-1 regression**

Launch the app and verify:
- Container #0 still docks in the right splitter panel
- Floating a container still works (title bar → float button)
- Container state persists across restart (quit and relaunch)
- Axis-lock repositioning works for overlay-docked containers
- No crashes on resize

- [ ] **Step 3: Verify GPU rendering path**

Check the debug log for:
```
MeterWidget: QRhi backend: Metal    (macOS)
MeterWidget: QRhi backend: D3D11   (Windows)
```

If the GPU path fails, the CPU fallback should still render the bar via QPainter.

- [ ] **Step 4: Commit any fixes**

If any cross-platform issues were found and fixed:
```bash
git add -u
git commit -m "Fix cross-platform issues in MeterWidget GPU rendering"
```

---

## Verification Checklist

| Requirement | Task |
|-------------|------|
| MeterWidget : QRhiWidget | Task 3 |
| Pipeline 1 (textured quad) | Task 3 (initBackgroundPipeline) |
| Pipeline 2 (vertex-colored geometry) | Task 3 (initGeometryPipeline) |
| Pipeline 3 (QPainter overlay, static+dynamic split) | Task 3 (initOverlayPipeline) |
| BarItem (H_BAR) | Task 4 |
| TextItem | Task 4 |
| ScaleItem (H_SCALE) | Task 4 |
| SolidColourItem | Task 4 |
| ImageItem | Task 4 |
| ItemGroup | Task 5 |
| WDSP polling (MeterPoller, 100ms) | Task 6 |
| Item serialization (pipe-delimited) | Task 7 |
| Container #0 populated | Task 8 |
| Live H_BAR bound to SignalPeak | Task 8 |
| No Phase 3G-1 regression | Task 9 |
| Cross-platform (macOS + Windows) | Task 9 |
| Shaders match SpectrumWidget convention | Task 1 |
| AetherSDR visual style | Tasks 4, 5 (color constants throughout) |
