#include "ClarityController.h"

#include <QDateTime>
#include <QtNumeric>

#include <cmath>

namespace NereusSDR {

ClarityController::ClarityController(QObject* parent)
    : QObject(parent)
{
}

void ClarityController::setEnabled(bool on)
{
    m_enabled = on;
}

void ClarityController::setTransmitting(bool tx)
{
    m_transmitting = tx;
}

void ClarityController::notifyManualOverride()
{
    if (m_paused) {
        return;
    }
    m_paused = true;
    emit pausedChanged(true);
}

void ClarityController::retuneNow()
{
    if (m_paused) {
        m_paused = false;
        emit pausedChanged(false);
    }
    // Snap EWMA state — next frame re-anchors instead of slow-lerping
    // from the pre-override floor.
    m_hasSmoothed = false;
    m_hasEmitted  = false;
}

void ClarityController::snapToFloor(float floorDbm)
{
    if (qIsNaN(floorDbm) || !m_enabled) {
        return;
    }
    m_smoothedFloor    = floorDbm;
    m_hasSmoothed      = true;

    float low  = floorDbm + m_lowMarginDb;
    float high = floorDbm + m_highMarginDb;
    if ((high - low) < m_minGapDb) {
        const float center = (low + high) / 2.0f;
        low  = center - m_minGapDb / 2.0f;
        high = center + m_minGapDb / 2.0f;
    }

    m_lastLow          = low;
    m_lastHigh         = high;
    m_lastEmittedFloor = floorDbm;
    m_hasEmitted       = true;
    emit waterfallThresholdsChanged(low, high);
}

void ClarityController::setPollIntervalMs(int ms)      { m_pollIntervalMs = ms; }
void ClarityController::setSmoothingTauSec(float sec)  { m_tauSec = sec; }
void ClarityController::setDeadbandDb(float db)        { m_deadbandDb = db; }
void ClarityController::setLowMarginDb(float db)       { m_lowMarginDb = db; }
void ClarityController::setHighMarginDb(float db)      { m_highMarginDb = db; }
void ClarityController::setMinGapDb(float db)          { m_minGapDb = db; }
void ClarityController::setPercentile(float p)         { m_estimator.setPercentile(p); }

void ClarityController::feedBins(const QVector<float>& bins, qint64 nowMs)
{
    if (!m_enabled || m_transmitting || m_paused) {
        return;
    }
    if (nowMs < 0) {
        nowMs = QDateTime::currentMSecsSinceEpoch();
    }

    // Cadence gate: skip frames that land inside the current poll window.
    if (m_pollIntervalMs > 0 && m_hasEmitted &&
        (nowMs - m_lastPollMs) < m_pollIntervalMs) {
        return;
    }

    const float rawFloor = m_estimator.estimate(bins);
    if (qIsNaN(rawFloor)) {
        return;
    }

    // EWMA smoothing. τ ≤ 0 is passthrough (no smoothing) for test
    // isolation. Otherwise alpha = 1 - exp(-dt/τ) where dt is the
    // interval since last computation.
    float smoothed;
    if (!m_hasSmoothed || m_tauSec <= 0.0f) {
        smoothed = rawFloor;
    } else {
        const float dt  = static_cast<float>(nowMs - m_lastPollMs) / 1000.0f;
        const float alpha = 1.0f - std::exp(-dt / m_tauSec);
        smoothed = alpha * rawFloor + (1.0f - alpha) * m_smoothedFloor;
    }
    m_smoothedFloor = smoothed;
    m_hasSmoothed   = true;
    m_lastPollMs    = nowMs;

    // Deadband gate: suppress emission when floor hasn't drifted enough.
    if (m_hasEmitted && std::abs(smoothed - m_lastEmittedFloor) < m_deadbandDb) {
        return;
    }

    // Compute thresholds with margin around the smoothed floor.
    float low  = smoothed + m_lowMarginDb;
    float high = smoothed + m_highMarginDb;

    // Min-gap clamp — empty-band failure mode (§6.2.4).
    if ((high - low) < m_minGapDb) {
        const float center = (low + high) / 2.0f;
        low  = center - m_minGapDb / 2.0f;
        high = center + m_minGapDb / 2.0f;
    }

    m_lastLow          = low;
    m_lastHigh         = high;
    m_lastEmittedFloor = smoothed;
    m_hasEmitted       = true;
    emit waterfallThresholdsChanged(low, high);
}

}  // namespace NereusSDR
