// no-port-check: NereusSDR-original PureSignal-shaped copies of the ready-made bars.
// Modification history (NereusSDR):
//   2026-11-01 - One PureSignal feedback-meter face for every Ready-made
//                 meters entry whose name contains "bar", except SWR bar,
//                 which already has SWR New. Each copy keeps that bar's
//                 reading and scale. J.J. Boyd (KG4VCF), AI-assisted via
//                 xAI Grok.
#pragma once
#include "BarPresetItem.h"
#include <QString>
#include <functional>
#include <memory>
namespace NereusSDR {
class PsStyleBarItem : public BarPresetItem {
    Q_OBJECT
public:
    static std::unique_ptr<MeterItem> create(const QString& typeId);
    static void eachSpec(const std::function<void(const QString& typeId, const QString& title)>& visit);
    QString typeId() const override;
    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    bool participatesIn(Layer layer) const override { return layer == Layer::OverlayDynamic; }
    void paint(QPainter& painter, int widgetW, int widgetH) override;
    void paintForLayer(QPainter& painter, int widgetW, int widgetH, Layer layer) override;
private:
    struct Spec { const char* typeId; const char* title; const char* flavor; const char* caption; };
    explicit PsStyleBarItem(const Spec& spec, QObject* parent = nullptr);
    static const Spec* specs(int& count);
    QString m_catalogType;
    QString m_caption;
};
} // namespace NereusSDR
