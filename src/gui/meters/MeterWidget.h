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

class MeterWidget : public MeterBaseClass {
    Q_OBJECT

public:
    explicit MeterWidget(QWidget* parent = nullptr);
    ~MeterWidget() override;

    QSize sizeHint() const override { return {260, 300}; }

    void addItem(MeterItem* item);
    void removeItem(MeterItem* item);
    void clearItems();
    QVector<MeterItem*> items() const { return m_items; }

    void updateMeterValue(int bindingId, double value);

    // Container-level mode state, consumed by the Thetis visibility filter
    // rule (MeterManager.cs:31366-31368). mox == true means TX active;
    // displayGroup selects which TX group is currently visible. Both default
    // to the Thetis "RX, no group restriction" state.
    void setMox(bool mox);
    bool mox() const { return m_mox; }
    void setDisplayGroup(int group);
    int  displayGroup() const { return m_displayGroup; }

    // Returns true if an item should paint under the current mode/group.
    // Mirrors Thetis MeterManager.cs:31366-31368 verbatim.
    bool shouldRender(const MeterItem* item) const;

    QString serializeItems() const;
    bool deserializeItems(const QString& data);

    // Thetis-parity stack layout with a NereusSDR pixel floor
    // (Phase 3G-9 post-revert).
    //
    // reflowStackedItems() is called on every resize. It computes a
    // per-reflow slot height (0.05 * widgetH normalized, floored at
    // 24 px) and a bandTop (max of y+h over composite items with
    // itemHeight > 0.30), then re-lays every stacked item.
    //
    // inferStackFromGeometry() rebuilds m_stackSlot + m_slotLocalY/H
    // after a deserialize from the saved geometry so existing
    // containers keep participating in reflow-on-resize without a
    // persistence format bump.
    void reflowStackedItems();
    void inferStackFromGeometry();

protected:
#ifdef NEREUS_GPU_SPECTRUM
    void initialize(QRhiCommandBuffer* cb) override;
    void render(QRhiCommandBuffer* cb) override;
    void releaseResources() override;
#endif
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void drawItems(QPainter& p);
    QVector<MeterItem*> m_items;

    // Visibility filter state — see setMox/setDisplayGroup doc.
    bool m_mox{false};
    int  m_displayGroup{0};

#ifdef NEREUS_GPU_SPECTRUM
    bool m_rhiInitialized{false};

    void initBackgroundPipeline();
    void initGeometryPipeline();
    void initOverlayPipeline();
    void renderGpuFrame(QRhiCommandBuffer* cb);

    // Pipeline 1: Background (textured quad, cached)
    QRhiGraphicsPipeline*       m_bgPipeline{nullptr};
    QRhiShaderResourceBindings* m_bgSrb{nullptr};
    QRhiBuffer*                 m_bgVbo{nullptr};
    QRhiTexture*                m_bgGpuTex{nullptr};
    QRhiSampler*                m_bgSampler{nullptr};
    QImage m_bgImage;
    bool   m_bgDirty{true};

    // Pipeline 2: Geometry (vertex-colored, per-frame)
    QRhiGraphicsPipeline*       m_geomPipeline{nullptr};
    QRhiShaderResourceBindings* m_geomSrb{nullptr};
    QRhiBuffer*                 m_geomVbo{nullptr};
    static constexpr int kMaxGeomVerts = 4096;
    static constexpr int kGeomVertStride = 6;  // x, y, r, g, b, a
    int m_geomVertCount{0};

    // Pipeline 3: Overlay (QPainter → texture, split static/dynamic)
    QRhiGraphicsPipeline*       m_ovPipeline{nullptr};
    QRhiShaderResourceBindings* m_ovSrb{nullptr};
    QRhiBuffer*                 m_ovVbo{nullptr};
    QRhiTexture*                m_ovGpuTex{nullptr};
    QRhiSampler*                m_ovSampler{nullptr};
    QImage m_overlayStatic;
    bool   m_overlayStaticDirty{true};
    QImage m_overlayDynamic;
    bool   m_overlayDynamicDirty{true};
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
