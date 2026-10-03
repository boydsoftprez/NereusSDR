#pragma once
#include <optional>

// =================================================================
// src/gui/meters/MeterWidget.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Composite reading/replay/cadence contracts by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-25 - iPhone app plan Task 39 (D14, R-IOS-13): a binding can be
//                 marked unavailable with a plain reason; its items are
//                 drawn dimmed and the reason is their tooltip (a meter a
//                 remote window cannot show yet is disabled, never hidden).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - drawsGeometryLayer(): the geometry buffer is written only
//                 in a frame that binds it (GUI memory leak follow-up).
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

#include "MeterItem.h"
#include <QWidget>
#include <QHash>
#include <QImage>
#include <QVector>
#include <QPointer>

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
    // Replace live items while retaining inert legacy siblings and source order.
    void replaceItems(const QVector<MeterItem*>& items);
    QVector<MeterItem*> items() const { return m_items; }

    void updateMeterValue(int bindingId, double value);
    // MMIO remains an individual item's cached endpoint source.
    void updateMmioValue(MeterItem* item, double value, const QString& unavailableReason = {},
                         MeterItem::BindingSupport support = MeterItem::BindingSupport::Supported);
    void advanceMeters(qint64 monotonicMs);
#ifdef NEREUS_BUILD_TESTS
    quint64 readingInvalidationsForTest() const { return m_readingInvalidations; }
#ifdef NEREUS_GPU_SPECTRUM
    void clearReadingLayerDirtyFlagsForTest() { m_bgDirty = m_overlayStaticDirty = m_overlayDynamicDirty = false; }
    bool backgroundDirtyForTest() const { return m_bgDirty; }
    bool overlayStaticDirtyForTest() const { return m_overlayStaticDirty; }
    bool overlayDynamicDirtyForTest() const { return m_overlayDynamicDirty; }
#endif
#endif
    void resetForTxTransition(bool inTx);
    // Source changes discard old samples before the new source is replayed.
    void clearReadingCache();
    int powerScale() const { return m_powerScale; }
    // GUI model adapters notify the actual owning face after child state changes.
    void invalidatePresentation(const MeterItem* item);
    void setUnitMode(MeterItem::MeterUnit unit);
    MeterItem::MeterUnit unitMode() const;


    // iPhone app plan Task 39 (NereusSDR-native): the items bound to
    // `bindingId` cannot show a reading here (a remote window's Core does
    // not send it). They stay where they are, drawn dimmed, and hovering
    // one shows `reason`. An empty reason makes the binding available
    // again.
    void setBindingUnavailable(int bindingId, const QString& reason);
    void setBindingSupport(int bindingId, MeterItem::BindingSupport support);
    MeterItem::BindingSupport bindingSupport(int bindingId) const
    { return m_bindingSupport.value(bindingId, MeterItem::BindingSupport::Unknown); }
    QString bindingUnavailableReason(int bindingId) const
    {
        return m_unavailableBindings.value(bindingId);
    }
    // The reason for the unavailable item under `pos`, or empty.
    QString unavailableReasonAt(const QPointF& pos) const;

    // Rescale the Power BarItem + ScaleItem pair (objectName "PowerBar" /
    // "PowerScale") for the connected SKU's PA ceiling.  20% headroom
    // past the red zone, sub-watt tick resolution for QRP radios.
    // Bench-reported #167 follow-up: 0-120 W default made HL2 (5 W max)
    // and ANAN-G2-1K (1000 W max) both unreadable on the same scale.
    // Subscribed by MainWindow to RadioModel::currentRadioChanged.
    void rescalePowerMeters(int paMaxWatts);

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

    // R-R3-49: false for an item that fronts a feature not built yet
    // (UnbuiltFeatures). Such an item loads and is saved with its
    // container, but is not drawn, takes no clicks and is not offered.
    static bool itemFeatureBuilt(const MeterItem* item);

    // Whether a GPU frame writes and binds the geometry vertex buffer: only
    // with a geometry pipeline to draw it and vertices to draw. The write
    // and the draw both take it, because Qt's Metal backend keeps a partial
    // dynamic-buffer write pending until the buffer is bound, so a buffer
    // written every frame and never bound (the pipeline failed to build,
    // for example a missing shader) keeps every frame's copy.
    static bool drawsGeometryLayer(bool hasPipeline, int vertCount)
    {
        return hasPipeline && vertCount > 0;
    }

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

signals:
    // Emitted while derived members still exist; QWidget::destroyed may run
    // later while a QPointer still appears live on some Qt backends.
    void aboutToDestroy();
    // R-R3-21: an item was added (restore, a preset, Container settings
    // Apply), so MainWindow can give it the saved meter settings and the
    // active slice's state.
    void itemAdded(NereusSDR::MeterItem* item);
    void itemRemoved(NereusSDR::MeterItem* item);
    void displayVisibilityChanged();

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
    bool event(QEvent* event) override;

private:
    friend class ItemGroup; // Legacy preset transfer retains inert serialized siblings.
    void drawItems(QPainter& p);
    // Task 39: dims each drawn item whose binding is unavailable.
    void drawUnavailableVeils(QPainter& p) const;
    QHash<int, QString> m_unavailableBindings;
    QHash<int, MeterItem::BindingSupport> m_bindingSupport;
    QVector<MeterItem*> m_items;
    struct LegacyRecord { QString raw; QPointer<MeterItem> item; };
    QVector<LegacyRecord> m_legacyRecords;

    // Visibility filter state — see setMox/setDisplayGroup doc.
    bool m_mox{false};
    int  m_displayGroup{0};

    // Raw latest values seed new items; delivery still occurs on each poll.
    QHash<int, double> m_lastBindingValue;
    struct MmioReading { double value; QString reason; MeterItem::BindingSupport support; };
    QHash<QString, MmioReading> m_lastMmioReading;
    quint64 m_readingInvalidations{0};
    void invalidateItemLayers(const MeterItem* item);
    int m_powerScale{0};
    std::optional<MeterItem::MeterUnit> m_unitMode;
    void invalidateReadingLayers(bool staticLayers = false);

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
