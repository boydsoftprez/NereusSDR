// src/gui/widgets/OverflowChip.h
#pragma once

#include <QStringList>
#include <QWidget>

class QLabel;

namespace NereusSDR {

// OverflowChip — small "…" pill that surfaces items the host has hidden
// for layout-fit reasons.
//
// Per docs/architecture/2026-08-02-bottom-banner-and-pan-menu-design.md
// §5-6: when ChromeBarController folds >= 1 banner item to fit the bar
// width, this chip appears at the strip end and its tooltip lists what
// was folded. Hovering tells the user where items went; widening the
// window restores them and the chip vanishes.
//
// The chip itself is presentation-only: MainWindow owns the fold
// decision (ChromeBarController) and just calls setDroppedItems() on
// each ChromeBarController::foldStateChanged.
class OverflowChip : public QWidget {
    Q_OBJECT

public:
    explicit OverflowChip(QWidget* parent = nullptr);

    // Replace the dropped-items list and rebuild the multi-line tooltip.
    // Does not touch visibility: this widget is registered with
    // ChromeBarController at rung 0, so the controller is the sole writer
    // of setVisible (via the availability axis, driven by
    // !items.isEmpty() at the call site). See MainWindow's
    // ChromeBarController::foldStateChanged connection.
    void setDroppedItems(const QStringList& items);
    QStringList droppedItems() const noexcept { return m_items; }

    bool hasDroppedItems() const noexcept { return !m_items.isEmpty(); }

private:
    QStringList m_items;
    QLabel*     m_glyph{nullptr};
};

} // namespace NereusSDR
