// =================================================================
// src/gui/meters/presets/PresetGeometry.h  (NereusSDR)
// =================================================================
//
// Shared geometry helpers for preset MeterItem subclasses. The
// free functions here are NereusSDR-original scaffolding (not a
// Thetis port) — they factor out the "letterbox the background
// image inside the container item rect so pivot/radius/needle
// geometry stays glued to the image" math that multiple presets
// (AnanMultiMeterItem, CrossNeedleItem, ...) share. Thetis's
// rendering pipeline has no direct equivalent: in the upstream
// C# code each needle item is positioned against the raw face
// image bitmap, and the arc-drift bug this helper prevents only
// exists in the NereusSDR port because our containers allow
// non-default aspect ratios that stretch the background image.
//
// Keep this file small and header-only. No Thetis-derived logic;
// no upstream header block required.
// =================================================================
#pragma once

#include <QImage>
#include <QRect>

namespace NereusSDR {

/// Letterbox a preset's background image inside the container item rect.
///
/// Preserves the image's natural aspect ratio and centers the drawn
/// area within `itemRect`, so pivot/radius math anchored to the
/// returned rect stays glued to the painted meter face when the
/// container aspect ratio differs from the image's natural aspect
/// ratio.
///
/// Behaviour:
/// - If `!anchored`, or the background is null / zero-height, the
///   raw `itemRect` is returned unchanged (pre-port "stretch to
///   fill" behaviour).
/// - Otherwise the image is letterboxed (horizontally if the item
///   is wider than the image aspect, vertically if taller).
///
/// Used by AnanMultiMeterItem (7-needle composite) and
/// CrossNeedleItem (fwd/reflected composite) to fix arc drift at
/// non-default container aspect ratios.
inline QRect letterboxedBgRect(const QRect& itemRect,
                               const QImage& background,
                               bool anchored)
{
    // Guard against (a) anchor disabled, (b) background image failed
    // to load, and (c) degenerate image dimensions that would divide
    // by zero in the aspect-ratio calculation. In any of those cases,
    // fall back to the item's pixel rect (pre-port behaviour).
    if (!anchored || background.isNull() || background.height() <= 0) {
        return itemRect;
    }
    if (itemRect.width() <= 0 || itemRect.height() <= 0) {
        return itemRect;
    }
    const float imgAspect = static_cast<float>(background.width())
                          / static_cast<float>(background.height());
    const float rectAspect = static_cast<float>(itemRect.width())
                           / static_cast<float>(itemRect.height());

    if (rectAspect > imgAspect) {
        // Item wider than the image — letterbox horizontally.
        const int drawW = static_cast<int>(itemRect.height() * imgAspect);
        const int drawX = itemRect.x() + (itemRect.width() - drawW) / 2;
        return QRect(drawX, itemRect.y(), drawW, itemRect.height());
    }
    // Item taller than (or matches) the image — letterbox vertically.
    const int drawH = static_cast<int>(itemRect.width() / imgAspect);
    const int drawY = itemRect.y() + (itemRect.height() - drawH) / 2;
    return QRect(itemRect.x(), drawY, itemRect.width(), drawH);
}

} // namespace NereusSDR
