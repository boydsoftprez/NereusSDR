#pragma once

#include "core/WdspTypes.h"

#include <QObject>
#include <QTimer>
#include <QVector>

namespace NereusSDR {

class RxChannel;
class MeterWidget;

// Binding IDs map to WDSP meter types (RxMeterType enum values)
namespace MeterBinding {
    constexpr int SignalPeak   = 0;    // RxMeterType::SignalPeak
    constexpr int SignalAvg    = 1;    // RxMeterType::SignalAvg
    constexpr int AdcPeak      = 2;    // RxMeterType::AdcPeak
    constexpr int AdcAvg       = 3;    // RxMeterType::AdcAvg
    constexpr int AgcGain      = 4;    // RxMeterType::AgcGain
    constexpr int AgcPeak      = 5;    // RxMeterType::AgcPeak
    constexpr int AgcAvg       = 6;    // RxMeterType::AgcAvg
}

class MeterPoller : public QObject {
    Q_OBJECT

public:
    explicit MeterPoller(QObject* parent = nullptr);
    ~MeterPoller() override;

    void setRxChannel(RxChannel* channel);
    void addTarget(MeterWidget* widget);
    void removeTarget(MeterWidget* widget);

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
