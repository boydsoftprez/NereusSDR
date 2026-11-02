// no-port-check: NereusSDR-original SWR face in the PureSignal meter shape.
// Modification history (NereusSDR):
//   2026-11-01 - SWR New: the transmit SWR reading drawn as the PureSignal
//                 feedback meter (rounded track, in-range band, accent fill,
//                 marker and ticks). J.J. Boyd (KG4VCF), AI-assisted via
//                 xAI Grok.
#pragma once
#include "gui/meters/MeterItem.h"
namespace NereusSDR {
class SwrNewItem : public MeterItem {
    Q_OBJECT
public:
    explicit SwrNewItem(QObject* parent = nullptr);
    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& painter, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;
};
} // namespace NereusSDR
