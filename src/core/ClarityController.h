// =================================================================
// src/core/ClarityController.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

//=================================================================
// display.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley (W5WC)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Waterfall AGC Modifications Copyright (C) 2013 Phil Harman (VK6APH)
// Transitions to directX and continual modifications Copyright (C) 2020-2025 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

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
