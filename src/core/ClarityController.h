// src/core/ClarityController.h
//
// Clarity adaptive display tuning controller (Phase 3G-9c). Wraps the
// NoiseFloorEstimator with EWMA smoothing, deadband gating, poll-rate
// throttling, TX/override pause, and a cold-start grace window, then
// emits Waterfall Low/High threshold updates downstream.
//
// Lineage: Thetis processNoiseFloor() in display.cs:5866 uses a similar
// lerp-toward-bin-average approach over an attack-time window (default
// 2000 ms). Clarity inherits the "slow EWMA over estimated floor" idea
// but swaps the mean estimator for a percentile so no upstream filter is
// needed and strong carriers do not pull the estimate up.
//
// Locked parameters from 2026-04-15-display-refactor-design.md §6.2.2:
//   - 30th percentile (NoiseFloorEstimator default)
//   - 500 ms poll interval (2 Hz)
//   - τ = 3 s EWMA smoothing
//   - ±2 dB deadband before a threshold update is emitted
//   - 30 dB minimum gap clamp (§6.2.4 empty-band failure mode)
//
// Threshold shape: lowMarginDb / highMarginDb straddle the smoothed floor.
// Defaults (-5, +55) give 60 dB total dynamic range, comfortably above the
// 30 dB minimum gap. Replaces the PR2 running-min/max AGC with its 12 dB
// margin — see waterfall-tuning.md "Open questions for PR3".

#pragma once

#include "core/NoiseFloorEstimator.h"

#include <QObject>
#include <QVector>

namespace NereusSDR {

class ClarityController : public QObject {
    Q_OBJECT
public:
    explicit ClarityController(QObject* parent = nullptr);

    bool isEnabled() const noexcept     { return m_enabled;     }
    bool isPaused() const noexcept      { return m_paused;      }
    bool isTransmitting() const noexcept{ return m_transmitting;}
    float smoothedFloor() const noexcept{ return m_smoothedFloor;}

    // Last emitted thresholds (qQNaN() before any emission).
    float lastLow() const noexcept  { return m_lastLow;  }
    float lastHigh() const noexcept { return m_lastHigh; }

    // Tunables — default to spec-locked values, exposed for tests.
    void setPollIntervalMs(int ms);
    void setSmoothingTauSec(float sec);
    void setDeadbandDb(float db);
    void setLowMarginDb(float db);
    void setHighMarginDb(float db);
    void setMinGapDb(float db);
    void setPercentile(float p);

public slots:
    // Master toggle. When off, controller stops acting on feedBins()
    // and does not emit. Currently-displayed thresholds are left alone.
    void setEnabled(bool on);

    // TX pause via MOX signal — per spec §6.2.4 failure mode mitigation.
    void setTransmitting(bool tx);

    // User dragged a threshold slider while Clarity was on. Pause until
    // the user clicks Re-tune or toggles Clarity off/on.
    void notifyManualOverride();

    // "Re-tune now" button hook — resumes after manual override and
    // snaps smoothing to the latest estimate.
    void retuneNow();

    // Band-switch snap — PanadapterModel feeds the stored floor for the
    // new band. EWMA state re-anchors at this value and thresholds are
    // emitted immediately so the waterfall snaps to the remembered state.
    // NaN input is ignored (band has no stored Clarity data).
    void snapToFloor(float floorDbm);

    // Per-frame FFT bin vector (dB) from FFTEngine. The controller may
    // no-op if disabled, paused, transmitting, or inside the poll window.
    //
    // `nowMs` is an optional monotonic timestamp for deterministic tests.
    // Production callers omit it and the implementation uses the Qt wall
    // clock. Not a test-only surface — any caller can pin a time source
    // (useful for replay/playback features).
    void feedBins(const QVector<float>& bins, qint64 nowMs = -1);

signals:
    // Thresholds should change. Deadband-gated so consumers (SpectrumWidget)
    // receive a stable stream, not per-frame jitter.
    void waterfallThresholdsChanged(float lowDbm, float highDbm);

    // Status badge feed for SpectrumOverlayPanel. Green when active, amber
    // when paused (TX, manual override, or disabled).
    void pausedChanged(bool paused);

private:
    NoiseFloorEstimator m_estimator;

    bool   m_enabled       = false;
    bool   m_transmitting  = false;
    bool   m_paused        = false;

    // EWMA state — NaN until first valid frame.
    float  m_smoothedFloor = 0.0f;  // placeholder for RED stub
    bool   m_hasSmoothed   = false;

    // Last emitted thresholds + floor — track for deadband.
    float  m_lastLow           = 0.0f;
    float  m_lastHigh          = 0.0f;
    float  m_lastEmittedFloor  = 0.0f;
    bool   m_hasEmitted        = false;

    // Cadence — milliseconds since epoch of last poll.
    qint64 m_lastPollMs    = 0;

    // Tunables (spec defaults).
    int    m_pollIntervalMs = 500;
    float  m_tauSec         = 3.0f;
    float  m_deadbandDb     = 2.0f;
    float  m_lowMarginDb    = -5.0f;
    float  m_highMarginDb   = 55.0f;
    float  m_minGapDb       = 30.0f;
};

}  // namespace NereusSDR
