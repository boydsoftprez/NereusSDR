// no-port-check: NereusSDR-original read-only source routing; no DSP reads.
// Modification history (NereusSDR):
//   2026-10-04 — Selected RX source identity and RX-only presentation reset by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "ContainerSourceAdapter.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "gui/meters/MeterPoller.h"
#include <QJsonArray>
#include <QJsonDocument>
namespace NereusSDR {
bool ContainerSourceAdapter::followsSelectedRx(const QJsonObject& context)
{
    return context.value("rxSourceMode").toString() == QStringLiteral("followSelectedRx");
}
SliceModel* ContainerSourceAdapter::slice(RadioModel* model, const QJsonObject& context, SliceModel* inherited, const QString& currentSessionId)
{
    if (!model) { return nullptr; }
    // A foreign session remains unavailable rather than reading this station.
    if (context.contains("sessionId") && !context.value("sessionId").toString().isEmpty()
        && context.value("sessionId").toString() != currentSessionId) { return nullptr; }
    if (followsSelectedRx(context)) { return inherited; }
    if (context.contains("sliceId")) { return model->sliceById(context.value("sliceId").toInt(-1)); }
    if (context.contains("rxSource")) { return model->sliceById(context.value("rxSource").toInt() - 1); }
    return inherited;
}
QByteArray ContainerSourceAdapter::sourceIdentity(RadioModel* model, const QJsonObject& context, SliceModel* inherited,
                                                 const QString& currentSessionId, bool ready)
{
    const SliceModel* source = slice(model, context, inherited, currentSessionId);
    // Epochs use strings so the full 64-bit identity survives JSON numbers.
    const QJsonArray identity{currentSessionId, source ? source->sliceIndex() : -1,
        source ? source->streamIndex() : -1,
        source ? QString::number(source->streamEpoch()) : QString(),
        ready && source && source->streamIndex() >= 0};
    return QJsonDocument(identity).toJson(QJsonDocument::Compact);
}
double ContainerSourceAdapter::reading(RadioModel* model, const QJsonObject& context, SliceModel* inherited,
                                      int binding, bool ready, bool extendedReadings,
                                      const std::function<double(const SliceModel*)>& maxBin, const QString& currentSessionId)
{
    SliceModel* source = slice(model, context, inherited, currentSessionId);
    if (!ready || !source || source->streamIndex() < 0) { return kNoMeterReadingDbm; }
    switch (binding) {
    case MeterBinding::SignalPeak: return source->signalPeakDbm();
    case MeterBinding::SignalAvg: return source->signalAverageDbm();
    case MeterBinding::SignalMaxBin: return maxBin ? maxBin(source) : kNoMeterReadingDbm;
    // No sanctioned cache supplies passband SNR. Never manufacture a zero.
    case MeterBinding::PbSnr: return kNoMeterReadingDbm;
    default: break;
    }
    if (!extendedReadings) { return kNoMeterReadingDbm; }
    switch (binding) {
    case MeterBinding::AdcPeak: return source->adcPeakDbfs();
    case MeterBinding::AdcAvg: return source->adcAverageDbfs();
    case MeterBinding::AgcGain: return source->agcGainDb();
    case MeterBinding::AgcPeak: return source->agcPeakDb();
    case MeterBinding::AgcAvg: return source->agcAverageDb();
    default: return kNoMeterReadingDbm;
    }
}
}
