#pragma once

#include "core/WdspTypes.h"

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVector>

namespace NereusSDR {

class RxChannel;
class MeterWidget;

// Binding IDs map to WDSP meter types (RxMeterType enum values)
namespace MeterBinding {
    // RX meters (0-49)
    constexpr int SignalPeak   = 0;    // RxMeterType::SignalPeak
    constexpr int SignalAvg    = 1;    // RxMeterType::SignalAvg
    constexpr int AdcPeak      = 2;    // RxMeterType::AdcPeak
    constexpr int AdcAvg       = 3;    // RxMeterType::AdcAvg
    constexpr int AgcGain      = 4;    // RxMeterType::AgcGain
    constexpr int AgcPeak      = 5;    // RxMeterType::AgcPeak
    constexpr int AgcAvg       = 6;    // RxMeterType::AgcAvg

    // RX meters — new (Phase 3G-4)
    constexpr int SignalMaxBin = 7;    // Spectral peak bin
    constexpr int PbSnr        = 8;    // Peak-to-baseline SNR

    // TX meters (100+). From Thetis MeterManager.cs Reading enum.
    // Stub values until TxChannel exists (Phase 3I-1).
    // PWR/SWR are hardware PA measurements, not WDSP meters.
    constexpr int TxPower        = 100;  // Forward power (hardware PA)
    constexpr int TxReversePower = 101;  // Reverse power (hardware PA)
    constexpr int TxSwr          = 102;  // SWR (computed fwd/rev ratio)
    constexpr int TxMic          = 103;  // TXA_MIC_AV
    constexpr int TxComp         = 104;  // TXA_COMP_AV
    constexpr int TxAlc          = 105;  // TXA_ALC_AV

    // TX meters — new (Phase 3G-4)
    constexpr int TxEq           = 106;  // From Thetis MeterManager.cs EQ reading
    constexpr int TxLeveler      = 107;  // TXA_LEVELER_AV
    constexpr int TxLevelerGain  = 108;  // TXA_LEVELER_GAIN
    constexpr int TxAlcGain      = 109;  // TXA_ALC_GAIN
    constexpr int TxAlcGroup     = 110;  // TXA_ALC_GROUP
    constexpr int TxCfc          = 111;  // TXA_CFC_AV
    constexpr int TxCfcGain      = 112;  // TXA_CFC_GAIN

    // Hardware readings (200+)
    constexpr int HwVolts        = 200;  // PA supply voltage
    constexpr int HwAmps         = 201;  // PA supply current
    constexpr int HwTemperature  = 202;  // PA temperature

    // Rotator readings (300+)
    constexpr int RotatorAz      = 300;  // Azimuth (0-360)
    constexpr int RotatorEle     = 301;  // Elevation (0-90)
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

signals:
    // Emitted on each poll tick with the current S-meter (SignalAvg) dBm value.
    // Connect to VfoWidget::setSmeter to drive the VFO level bar.
    void smeterUpdated(double dbm);

private slots:
    void poll();

private:
    QTimer m_timer;
    QPointer<RxChannel> m_rxChannel;
    QVector<MeterWidget*> m_targets;
};

} // namespace NereusSDR
