// =================================================================
// src/gui/SMeterWidget.h  (NereusSDR)
// =================================================================
// Source attribution (AetherSDR, GPLv3):
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 section 5 requirements.
// =================================================================
// Modification history (NereusSDR):
//   2026-05-19  Ported in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
//                 Layout from AetherSDR src/gui/SMeterWidget.{h,cpp} [@0cd4559].
//                 Tasks 34/35/36 commit lands the header + constructor +
//                 paintEvent arc/scale rendering + scale test.
//                 RxMode enum expanded from 2 entries (upstream AetherSDR) to 4:
//                 SignalAverage (WDSP RXA_S_AV) and MaxBin (GetDetectMaxBin)
//                 added per NereusSDR design doc
//                 docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-design.md
//                 ss5.4.1 and ss5.4.3.
//                 contextMenuEvent declared; body implemented in Task 38.
//                 Peak hold config slots (setPeakHoldEnabled/setPeakHoldTimeMs/
//                 setPeakDecayRate/resetPeak) ported from upstream verbatim.
//                 Peak hold decay logic (Task 37) deferred.
//                 Right-click context menu (Tasks 38-39) deferred.
// =================================================================
#pragma once

#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>

// Forward declaration for contextMenuEvent
class QContextMenuEvent;

namespace NereusSDR {

// Analog S-Meter gauge widget matching the SmartSDR look.
//
// S-unit scale:
//   S0 = -127 dBm, S1 = -121 dBm, ... S9 = -73 dBm  (6 dB per S-unit)
//   S9+10 = -63 dBm, S9+20 = -53 dBm, S9+40 = -33 dBm, S9+60 = -13 dBm
//
// The needle sweeps from S0 (left) to S9+60 (right) across a 180 deg arc.
// Below S9 the scale markings are white; above S9 they are red.
//
// From AetherSDR src/gui/SMeterWidget.h [@0cd4559]
class SMeterWidget : public QWidget {
    Q_OBJECT

public:
    explicit SMeterWidget(QWidget* parent = nullptr);

    QSize sizeHint() const override { return {280, 140}; }
    QSize minimumSizeHint() const override { return {200, 100}; }

    // Current reading in dBm.
    float levelDbm() const { return m_levelDbm; }

    // Reading as S-units string (e.g. "S7", "S9+20").
    QString sUnitsText() const;

    // From AetherSDR src/gui/SMeterWidget.h:32-34 [@0cd4559]
    enum class TxMode { Power, SWR, Level, Compression };
    // RxMode expanded from AetherSDR's 2-entry enum (SMeter, SMeterPeak) to
    // 4 entries per design doc ss5.4.1.
    // SignalAverage -> WDSP RXA_S_AV (Task 31 WdspEngine::getRxaSignalAverage).
    // MaxBin        -> WDSP GetDetectMaxBin (Task 32 WdspEngine::getMaxBinDbm).
    enum class RxMode { SMeter, SignalAverage, SMeterPeak, MaxBin };
    enum class DecayRate { Fast, Medium, Slow };

    // Test-only accessors. Production code uses paintEvent's internal logic.
    float testPowerScaleMax()  const { return m_powerScaleMax; }
    float testPowerRedStart()  const { return m_powerRedStart; }

public slots:
    // Update the displayed RX level (S-meter dBm).
    // From AetherSDR src/gui/SMeterWidget.h:38 [@0cd4559]
    void setLevel(float dbm);

    // Update TX meter values.
    // From AetherSDR src/gui/SMeterWidget.h:41 [@0cd4559]
    void setTxMeters(float fwdPower, float swr);

    // Update mic/compression meter values.
    // From AetherSDR src/gui/SMeterWidget.h:44 [@0cd4559]
    void setMicMeters(float micLevel, float compLevel, float micPeak, float compPeak);

    // Switch between RX and TX needle source.
    // From AetherSDR src/gui/SMeterWidget.h:47 [@0cd4559]
    void setTransmitting(bool tx);

    // Dropdown-driven mode selection.
    // setRxMode accepts "S-Meter", "Sig Avg", "S-Meter Peak", "Max Bin".
    // From AetherSDR src/gui/SMeterWidget.h:50-51 [@0cd4559]
    void setTxMode(const QString& mode);
    void setRxMode(const QString& mode);

    // Set TX power gauge scale: barefoot (120W), Aurora (600W), amplifier (2000W).
    // From AetherSDR src/gui/SMeterWidget.h:54 [@0cd4559]
    void setPowerScale(int maxWatts, bool hasAmplifier);

    // Peak hold configuration.
    // From AetherSDR src/gui/SMeterWidget.h:57-61 [@0cd4559]
    void setPeakHoldEnabled(bool enabled);
    void setPeakHoldTimeMs(int ms);
    void setPeakDecayRate(DecayRate rate);
    void setPeakDecayRate(const QString& rate);
    void resetPeak();

protected:
    void paintEvent(QPaintEvent* event) override;
    // Right-click context menu. Body implemented in Task 38.
    // From AetherSDR src/gui/SMeterWidget.h (design doc ss5.4.1) [@0cd4559]
    void contextMenuEvent(QContextMenuEvent* ev) override;

private:
    void updateNeedleTarget();
    void animateNeedle();
    void updatePeakHoldValue();

    // Map dBm to fraction (0.0 = left, 1.0 = right) for RX S-meter scale
    // From AetherSDR src/gui/SMeterWidget.h:71-72 [@0cd4559]
    float dbmToFraction(float dbm) const;

    // Map TX value to fraction based on current TX mode
    // From AetherSDR src/gui/SMeterWidget.h:75 [@0cd4559]
    float txValueToFraction(float value) const;

    // Get the current TX value based on mode
    // From AetherSDR src/gui/SMeterWidget.h:78 [@0cd4559]
    float currentTxValue() const;

    // RX state
    // From AetherSDR src/gui/SMeterWidget.h:81-83 [@0cd4559]
    float   m_levelDbm{-127.0f};    // current RX reading
    float   m_peakDbm{-127.0f};     // RX peak hold
    QString m_source{"S-Meter Peak"};

    // TX meter values (updated continuously, used when transmitting)
    // From AetherSDR src/gui/SMeterWidget.h:86-89 [@0cd4559]
    float   m_txPower{0.0f};
    float   m_txSwr{1.0f};
    float   m_micLevel{-50.0f};
    float   m_compLevel{0.0f};

    // Mode state
    // From AetherSDR src/gui/SMeterWidget.h:92-94 [@0cd4559]
    TxMode  m_txMode{TxMode::Power};
    RxMode  m_rxMode{RxMode::SMeter};
    bool    m_transmitting{false};

    // From AetherSDR src/gui/SMeterWidget.h:96-99 [@0cd4559]
    QTimer  m_needleAnimation;
    QElapsedTimer m_needleElapsed;
    QTimer  m_peakDecay;
    QTimer  m_peakReset;    // hard reset peak hold every 10 seconds

    // From AetherSDR src/gui/SMeterWidget.h:101-102 [@0cd4559]
    float   m_needleFraction{0.0f};
    float   m_targetNeedleFraction{0.0f};

    // Peak hold line state
    // From AetherSDR src/gui/SMeterWidget.h:105-111 [@0cd4559]
    bool           m_peakHoldEnabled{false};
    float          m_peakHoldDbm{-127.0f};
    float          m_peakHoldDecayStartDbm{-127.0f};
    int            m_peakHoldTimeMs{1000};
    float          m_peakDecayDbPerSec{10.0f};  // Medium default
    QElapsedTimer  m_peakHoldTimer;
    bool           m_peakHoldTimerRunning{false};

    // S-unit reference: S0 = -127 dBm, each S-unit = 6 dB
    // From AetherSDR src/gui/SMeterWidget.h:114-117 [@0cd4559]
    static constexpr float S0_DBM  = -127.0f;
    static constexpr float S9_DBM  = -73.0f;
    static constexpr float MAX_DBM = -13.0f;  // S9+60
    static constexpr float DB_PER_S = 6.0f;

    // From AetherSDR src/gui/SMeterWidget.h:119-122 [@0cd4559]
    static constexpr int kNeedleAnimationIntervalMs = 8;
    static constexpr float kNeedleAttackTimeSeconds = 0.045f;
    static constexpr float kNeedleReleaseTimeSeconds = 0.180f;
    static constexpr float kNeedleSnapEpsilon = 0.001f;

    // TX Power gauge scale (dynamic)
    // From AetherSDR src/gui/SMeterWidget.h:125-126 [@0cd4559]
    float m_powerScaleMax{120.0f};
    float m_powerRedStart{100.0f};

    // Arc geometry: shallow arc spanning ~70 deg (like SmartSDR)
    // From AetherSDR src/gui/SMeterWidget.h:129-130 [@0cd4559]
    static constexpr float ARC_START_DEG = 55.0f;   // right end (degrees from +X axis)
    static constexpr float ARC_END_DEG   = 125.0f;  // left end
};

} // namespace NereusSDR
