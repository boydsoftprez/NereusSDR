// no-port-check: NereusSDR-original.
// =================================================================
// src/core/meters/TxMeterPump.cpp  (NereusSDR)
// =================================================================
//
// See TxMeterPump.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 39 (D14, R-IOS-13), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27: Remote-window parity Task 33 follow-up (R-R3-49): read()
//               works the COMP reading too (compressionDb). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-28: A9 (iPhone app plan Task 39): readFrom works the seven
//               stage readings the container meters show too. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "core/meters/TxMeterPump.h"

#include "core/RadioStatus.h"
#include "core/TxChannel.h"
#include "core/WdspEngine.h"
#include "core/WdspTypes.h"
#include "models/RadioModel.h"

#include <QTimer>

namespace NereusSDR {

TxMeterPump::TxMeterPump(RadioModel* model, QObject* parent)
    : QObject(parent)
    , m_model(model)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(kIntervalMs);
    m_timer->setTimerType(Qt::PreciseTimer);
    connect(m_timer, &QTimer::timeout, this, &TxMeterPump::poll);
}

TxMeterPump::~TxMeterPump() = default;

void TxMeterPump::setModel(RadioModel* model)
{
    m_model = model;
}

void TxMeterPump::setSource(Source source)
{
    m_source = std::move(source);
}

TxMeterReadings TxMeterPump::read(const RadioStatus& status, const TxChannel* tx)
{
    if (tx == nullptr) {
        return readFrom(status, {});
    }
    // txMeter reads the transmit lane's last reading (R-R3-39).
    return readFrom(status, [tx](TxMeterType meter) { return tx->txMeter(meter); });
}

TxMeterReadings TxMeterPump::readFrom(const RadioStatus& status,
                                      const std::function<double(TxMeterType)>& readRaw)
{
    TxMeterReadings readings;
    readings.forwardPowerWatts = status.forwardPowerWatts();
    readings.reflectedPowerWatts = status.reflectedPowerWatts();
    readings.swr = status.swrRatio();
    if (readRaw) {
        // D14, R-R3-49: Thetis's ALC and MIC readings, as the desktop's
        // meters show them (MeterPoller: TxAlc and TxMic).
        readings.alcDb = thetisTxReading(ThetisTxReading::Alc, readRaw);
        readings.micLevelDb = thetisTxReading(ThetisTxReading::Mic, readRaw);
        // Parity Task 33 follow-up (R-R3-49): the COMP reading the
        // desktop's Compression meters show (MeterPoller: TxComp).
        // From Thetis console.cs:46979 [v2.10.3.15]:
        //   updateMetersReading(Reading.COMP, (float)Math.Max(-30.0f, -WDSP.CalculateTXMeter(1, WDSP.MeterType.COMP)), 0);
        // with dsp.cs:1013-1014 [v2.10.3.15]:
        //   case MeterType.COMP:
        //       val = GetTXAMeter(channel, txaMeterType.TXA_COMP_AV);
        readings.compressionDb = thetisTxReading(ThetisTxReading::Comp, readRaw);
        // A9 (txReadingsVersion 3): the seven readings a local window's
        // container meters show (MeterPoller's kTxReadings: TxEq,
        // TxLeveler, TxLevelerGain, TxCfc, TxCfcGain, TxAlcGain and
        // TxAlcGroup), each worked by thetisTxReading as Thetis's MOX
        // branch works it.
        // From Thetis console.cs:46971-46986 [v2.10.3.15]:
        //   updateMetersReading(Reading.EQ, (float)Math.Max(-30.0f, -WDSP.CalculateTXMeter(1, WDSP.MeterType.EQ)), 0);
        //   updateMetersReading(Reading.LEVELER, (float)Math.Max(-30.0f, -WDSP.CalculateTXMeter(1, WDSP.MeterType.LEVELER)), 0);
        //   updateMetersReading(Reading.LVL_G, (float)Math.Max(0, WDSP.CalculateTXMeter(1, WDSP.MeterType.LVL_G)), 0);
        //   updateMetersReading(Reading.CFC_G, (float)Math.Max(0, -WDSP.CalculateTXMeter(1, WDSP.MeterType.CFC_G)), 0);
        //   updateMetersReading(Reading.CFC_AV, (float)Math.Max(-30.0f, -WDSP.CalculateTXMeter(1, WDSP.MeterType.CFC_AV)), 0);
        //   updateMetersReading(Reading.ALC_G, (float)Math.Max(-195.0f, -WDSP.CalculateTXMeter(1, WDSP.MeterType.ALC_G)), 0);
        //   updateMetersReading(Reading.ALC_GROUP, (float)Math.Max(-30.0f, -WDSP.CalculateTXMeter(1, WDSP.MeterType.ALC_PK)) + (float)Math.Max(0, -WDSP.CalculateTXMeter(1, WDSP.MeterType.ALC_G)), 0);
        readings.eqDb = thetisTxReading(ThetisTxReading::Eq, readRaw);
        readings.levelerDb = thetisTxReading(ThetisTxReading::Leveler, readRaw);
        readings.levelerGainDb = thetisTxReading(ThetisTxReading::LvlG, readRaw);
        readings.cfcDb = thetisTxReading(ThetisTxReading::CfcAv, readRaw);
        readings.cfcGainDb = thetisTxReading(ThetisTxReading::CfcG, readRaw);
        readings.alcGainDb = thetisTxReading(ThetisTxReading::AlcG, readRaw);
        readings.alcGroupDb = thetisTxReading(ThetisTxReading::AlcGroup, readRaw);
    }
    return readings;
}

TxMeterReadings TxMeterPump::readNow() const
{
    if (m_source) {
        return m_source();
    }
    if (m_model.isNull()) {
        return {};
    }
    WdspEngine* wdsp = m_model->wdspEngine();
    const TxChannel* tx = wdsp != nullptr ? wdsp->txChannel(WdspEngine::kTxChannelId) : nullptr;
    return read(m_model->radioStatus(), tx);
}

void TxMeterPump::start()
{
    if (!m_timer->isActive()) {
        m_timer->start();
    }
}

void TxMeterPump::stop()
{
    m_timer->stop();
}

bool TxMeterPump::isRunning() const
{
    return m_timer->isActive();
}

void TxMeterPump::poll()
{
    emit readingsTaken(readNow());
}

} // namespace NereusSDR
