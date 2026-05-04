#pragma once

// =================================================================
// src/gui/meters/MeterWidget.h  (NereusSDR)
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
