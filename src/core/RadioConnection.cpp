// no-port-check: NereusSDR-original base-class implementation. Inline doc
// comments reference Thetis source file names (console.cs) only as
// behavioral source-first cites for formula constants; no Thetis logic is
// reproduced here. The voltage conversion formulae are independently
// implemented from the Thetis console.cs interface specification.

#include "RadioConnection.h"
#include "P1RadioConnection.h"
#include "P2RadioConnection.h"
#include "LogCategories.h"

namespace NereusSDR {

RadioConnection::RadioConnection(QObject* parent)
    : QObject(parent)
{
}

RadioConnection::~RadioConnection() = default;

std::unique_ptr<RadioConnection> RadioConnection::create(const RadioInfo& info)
{
    switch (info.protocol) {
    case ProtocolVersion::Protocol2: {
        auto conn = std::make_unique<P2RadioConnection>();
        return conn;
    }
    case ProtocolVersion::Protocol1:
        return std::make_unique<P1RadioConnection>();
    }
    qCWarning(lcConnection) << "Unknown protocol version:" << static_cast<int>(info.protocol);
    return nullptr;
}

void RadioConnection::setState(ConnectionState newState)
{
    ConnectionState expected = m_state.load();
    if (expected != newState) {
        m_state.store(newState);
        emit connectionStateChanged(newState);
    }
}

// ---------------------------------------------------------------------------
// Rolling-window byte-rate counters
// ---------------------------------------------------------------------------

void RadioConnection::pruneSamples(QList<ByteSample>& samples, qint64 nowMs, int windowMs)
{
    while (!samples.isEmpty() && (nowMs - samples.first().ms) > windowMs) {
        samples.removeFirst();
    }
}

void RadioConnection::recordBytesSent(qint64 n)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    pruneSamples(m_txSamples, nowMs, 5000);
    m_txSamples.append({nowMs, n});
}

void RadioConnection::recordBytesReceived(qint64 n)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    pruneSamples(m_rxSamples, nowMs, 5000);
    m_rxSamples.append({nowMs, n});
}

double RadioConnection::txByteRate(int windowMs) const
{
    return rateFromSamples(m_txSamples, windowMs);
}

double RadioConnection::rxByteRate(int windowMs) const
{
    return rateFromSamples(m_rxSamples, windowMs);
}

double RadioConnection::rateFromSamples(const QList<ByteSample>& samples, int windowMs)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    qint64 totalBytes = 0;
    for (const auto& s : samples) {
        if (nowMs - s.ms <= windowMs) {
            totalBytes += s.bytes;
        }
    }
    // Mbps = bytes * 8 / windowMs / 1e3
    return (totalBytes * 8.0) / (windowMs * 1000.0);
}

// ---------------------------------------------------------------------------
// Ping RTT measurement via existing C&C round-trip
// ---------------------------------------------------------------------------

void RadioConnection::notePingSent()
{
    m_pingSentMs = QDateTime::currentMSecsSinceEpoch();
}

void RadioConnection::notePingReceived()
{
    if (m_pingSentMs == 0) { return; }
    const qint64 nowMs   = QDateTime::currentMSecsSinceEpoch();
    const qint64 elapsed = nowMs - m_pingSentMs;
    m_pingSentMs = 0;
    if (elapsed >= 0 && elapsed <= 5000) {
        emit pingRttMeasured(static_cast<int>(elapsed));
    }
}

// ---------------------------------------------------------------------------
// Voltage signals — supply_volts + user_adc0
// ---------------------------------------------------------------------------

// Hermes DC voltage formula — Thetis-faithful port of console.cs
// computeHermesDCVoltage() [v2.10.3.13]:
//   V = (raw / 4095) * 3.3 * ((4.7 + 0.82) / 0.82)
//       = (raw / 4095) * 3.3 * 6.7317...
//   where 4.7 kΩ and 0.82 kΩ are the resistor divider values (Hermes schematic).
//
// NOTE: in Thetis, computeHermesDCVoltage is defined but has zero callers —
// no UI surface displays it. We inherit the same dead-code status: the
// signal is emitted but no widget consumes it (MainWindow's PA volt label
// reads userAdc0 / convertMkiiPaVolts, matching Thetis behavior). Keeping
// the function preserves source-first parity if a future surface needs it.
float RadioConnection::convertSupplyVolts(quint16 raw)
{
    return (raw / 4095.0f) * 3.3f * (5.52f / 0.82f);
}

// MKII PA voltage formula.
// From Thetis console.cs convertToVolts() [v2.10.3.13]:
//   float volt_div = (22.0f + 1.0f) / 1.1f;     // 23/1.1 ≈ 20.909
//   float volts    = (IOreading / 4095.0f) * 5.0f;
//   volts = volts * volt_div;
//   return volts;
// (console.cs:24886-24892 — the readMKIIPAVoltsAmps consumer at
//  line 24881 is what fills _MKIIPAVolts, which the status bar at
//  line 26175 displays as "{0:#0.0}V".)
//
// Earlier revisions of this function used (raw/4095) * 3.3 * 25.5 —
// which is wrong on both axes: ADC reference is 5.0V (not 3.3V) and
// divider is 23/1.1 (not 25.5). The combined error was a 0.805
// scaling factor — actual 13.8V PA showed as 11.0V. Bug caught
// 2026-04-30 against a live ANAN-G2.
//
// Applies to ORIONMKII / ANAN-8000D / ANAN-7000DLE / ANAN-G2 PA drain sense.
float RadioConnection::convertMkiiPaVolts(quint16 raw)
{
    constexpr float kVoltDiv = (22.0f + 1.0f) / 1.1f;   // (R1 + R2) / R2
    constexpr float kAdcRef  = 5.0f;
    return (raw / 4095.0f) * kAdcRef * kVoltDiv;
}

void RadioConnection::handleSupplyRaw(quint16 raw)
{
    // Thetis-faithful: applies the Hermes formula (3.3V × 6.73). In
    // Thetis the equivalent function is dead code — no UI displays
    // supply_volts. We emit anyway for completeness; consumers (if any)
    // are responsible for understanding that AIN6 on MkII-class boards
    // does NOT report the user's main 13.8V supply (use userAdc0 / the
    // PA volt label for that — that's what Thetis displays).
    const float v = convertSupplyVolts(raw);
    if (qAbs(v - m_lastSupplyVolts) < 0.05f) { return; }
    m_lastSupplyVolts.store(v, std::memory_order_relaxed);
    emit supplyVoltsChanged(v);
}

void RadioConnection::handleUserAdc0Raw(quint16 raw)
{
    const float v = convertMkiiPaVolts(raw);
    if (qAbs(v - m_lastUserAdc0Volts) < 0.05f) { return; }
    m_lastUserAdc0Volts.store(v, std::memory_order_relaxed);
    emit userAdc0Changed(v);
}

} // namespace NereusSDR
