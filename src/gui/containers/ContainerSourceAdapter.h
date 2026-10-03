#pragma once
// no-port-check: NereusSDR-original read-only adapter for calibrated slice caches.
// Modification history (NereusSDR):
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QJsonObject>
#include <functional>
namespace NereusSDR {
class RadioModel;
class SliceModel;
class ContainerSourceAdapter {
public:
    static SliceModel* slice(RadioModel* model, const QJsonObject& context, SliceModel* inherited, const QString& currentSessionId = {});
    static double reading(RadioModel* model, const QJsonObject& context, SliceModel* inherited,
                          int binding, bool ready, bool extendedReadings,
                          const std::function<double(const SliceModel*)>& maxBin, const QString& currentSessionId = {});
};
}
