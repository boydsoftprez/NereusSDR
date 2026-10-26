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
// Ported from Thetis sources:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//     (ANANMM's needle scales and history defaults)
// =================================================================
// Modification history (NereusSDR):
//   2026-10-12 - Add explicit display marker/top scale by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
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
//                 Peak hold decay logic was already ported in this commit.
//                 Right-click context menu (Tasks 38-39) deferred.
//   2026-05-19  Task 37: test seams added: testAdvanceTime(int ms),
//                 testSetPeakHoldTimeMs(int ms), testPeakLevel().
//                 m_testTimeOffsetMs member added for synthetic time injection.
//                 No production API or behavior changed.
//   2026-05-19  Task 38: right-click context menu implemented (NereusSDR-native
//                 UX; AetherSDR uses an inline settings strip in AppletPanel
//                 which Task 40 removes). buildContextMenu() returns QMenu*;
//                 contextMenuEvent() delegates to it and exec()s.
//                 txModeToLabel() / rxModeToLabel() private helpers added.
//                 setTxMode() and setRxMode() now persist to AppSettings:
//                 SMeter_TxSelect (int 0..3) and SMeter_RxSelect (int 0..3).
//                 setPeakHoldEnabled() and setPeakDecayRate(QString) now persist
//                 to AppSettings: PeakHoldEnabled ("True"/"False") and
//                 PeakDecayRate ("Fast"/"Medium"/"Slow").
//                 buildContextMenuForTesting() public accessor for Task 39 test.
//   2026-09-18  Vintage meter faces (Lee, AI-assisted via Anthropic Claude
//                 Code).  FaceStyle enum + setFaceStyle(): six vintage panel-
//                 meter faces drawn by gui/VintageMeterFace (design carried
//                 over from Lee's TubeMeter project) alongside the original
//                 AetherSDR look, kept as FaceStyle::Classic.  "Meter Face"
//                 submenu appended to the context menu (index 3, so the
//                 Task 39 indices are unchanged); persisted as
//                 SMeter_FaceStyle.  The static face is cached in a pixmap;
//                 only pointer, markers and readouts are drawn per frame.
//                 NereusSDR-native; no upstream equivalent.
//   2026-09-22  No-reading display (R-R3-13) by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//                 kNoReadingDbm + m_noReading: a level at or below -400 dBm,
//                 or a non-finite one, is no reading.  Both faces then show
//                 "--" and "-- dBm", the pointer rests at the scale minimum
//                 and the peak markers clear until the next real value.
//                 rxSUnitsReadout() / rxDbmReadout() / peakMarkerVisible() /
//                 peakHoldLineVisible() hold the readout text and marker
//                 conditions both faces share.  Test seams added.
//   2026-09-24  settingsChanged() signal (R-R3-21) by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code:
//                 emitted when the face, peak hold or peak decay changes, so
//                 Setup > Appearance > Meter Styles follows a right-click
//                 change while it is open.
//                 NereusSDR-native; no upstream equivalent.
//   2026-09-24  The S-unit reference moved to core/ControlRanges.h (iPhone
//                 app Task 19, R-IOS-06) by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code; the
//                 Core's catalogue sends the same scale to an app.
//   2026-09-27  TX modes without a reading (R-R3-49, remote-window parity
//                 Task 33) by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//                 setTxModeUnavailable(): a TX mode a remote window's Core
//                 does not send shows "--" with the reason, never 0.  The TX
//                 readout text of both faces moved unchanged into txReadout().
//                 NereusSDR-native; no upstream equivalent.
//   2026-10-08  Per-copy settings for a dial added to a container
//                 (workspace preview) by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//                 SettingsStore::Instance: the TX mode, RX mode, peak hold,
//                 decay and face start at today's defaults and never read
//                 or write the AppSettings keys; instanceSettingsChanged()
//                 tells the owner to keep them.  peakHoldEnabled() and
//                 peakDecayRate() accessors.  The Main Panel's dial
//                 (SettingsStore::Global) is unchanged.
//                 NereusSDR-native; no upstream equivalent.
//   2026-10-08  An added dial's face follows Setup until the copy picks
//                 its own, and a test-only plate limit for wide containers
//                 (workspace preview), by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//                 setFollowsSetupFace() / followsSetupFace() and
//                 SMeterSetupFace (the SMeter_FaceStyle change notice);
//                 setPlateLimitForTest() and plateRect().  The paint code
//                 changes only in the rectangle the vintage plate is drawn
//                 in; with the limit off (the product) it is rect().
//                 NereusSDR-native; no upstream equivalent.
//   2026-10-08  JJ's dial decision (D20) by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//                 The plate limit is the product for every dial and
//                 setPlateLimitForTest() is gone; outside the bezel the
//                 vintage faces leave the widget clear, so the container
//                 behind shows (updatePaintOpacity()).
//                 NereusSDR-native; no upstream equivalent.
//   2026-10-26  Four more TX modes, appended without renumbering (saved
//                 layouts store the value): Alc (ANANMM's ALC_G "alc_comp"),
//                 AlcGroup (ALC_GROUP), Volts and Amps, with their scales
//                 from Thetis MeterManager.cs (AddAnanMM) [v2.10.3.15];
//                 setAlcMeters() / setSupplyMeters() feed them.  The VHF S9
//                 rule per slice: setAboveS9Frequency() raises a reading on
//                 the S scale by 20 dB at or above 30 MHz, so S9 reads at
//                 -93 dBm there.  J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-10-26  Per-copy appearance and dynamics, the Thetis meter item
//                 settings (setup.cs:25519-25577 [v2.10.3.15]) for a
//                 container copy: peak hold colour, needle and transmit
//                 needle colours, shadow, power scale colour, dark mode,
//                 update interval, attack and decay ratios, and the
//                 history wedge with its colour, duration and ignore time;
//                 clearHistory() (Thetis ClearHistory).  Unset, each is the
//                 dial as it was drawn before.  J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

// --- From MeterManager.cs ---
/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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

#include "core/ControlRanges.h"

#include <QColor>
#include <QVector>
#include <QWidget>
#include <array>
#include <optional>
#include <utility>
#include <QPixmap>
#include <QTimer>
#include <QElapsedTimer>

// Forward declarations
class QContextMenuEvent;
class QMenu;
class QPainter;

namespace NereusSDR {

class RadioModel;

// The face chosen on Setup > Appearance > Meter Styles (the SMeter_FaceStyle
// key) changed.  Emitted by the page and by the Main Panel's dial, the two
// writers of that key; an added dial that has no face of its own follows
// it.  Main thread only.  NereusSDR-native.
class SMeterSetupFace : public QObject {
    Q_OBJECT

public:
    static SMeterSetupFace& instance();
    // The face SMeter_FaceStyle holds now (Aged Cream when absent), as an
    // SMeterWidget::FaceStyle value.
    static int current();
    // Tells every follower that SMeter_FaceStyle now holds `face`.
    void notify(int face) { emit changed(face); }

signals:
    void changed(int face);
};

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

    // Where the widget keeps its TX mode, RX mode, peak hold, decay and
    // face.  Global: the Main Panel's dial and its AppSettings keys
    // (SMeter_TxSelect, SMeter_RxSelect, PeakHoldEnabled, PeakDecayRate,
    // SMeter_FaceStyle), read in the constructor and written by the
    // setters.  Instance: a dial added to a container (workspace preview);
    // it starts at today's defaults (Power, Signal, peak hold on, Medium,
    // Aged Cream), never touches those keys, and emits
    // instanceSettingsChanged() when one of the five changes, so its owner
    // keeps them in the workspace document.  NereusSDR-native.
    enum class SettingsStore { Global, Instance };
    explicit SMeterWidget(SettingsStore store, QWidget* parent = nullptr);
    SettingsStore settingsStore() const { return m_settingsStore; }

    // Overload that subscribes to RadioModel::externalAmpOperateChanged so
    // the 2 kW power scale flips for both PGXL and RF-Kit without the caller
    // having to wire each brand-specific signal.
    // NereusSDR-native; no upstream equivalent. Task 13.
    explicit SMeterWidget(RadioModel* model, QWidget* parent = nullptr);

    QSize sizeHint() const override { return {280, 140}; }
    QSize minimumSizeHint() const override { return {200, 100}; }

    // Current reading in dBm; -400 (kNoReadingDbm) when there is no reading.
    float levelDbm() const { return m_levelDbm; }

    // Reading as S-units string (e.g. "S7", "S9+20"), or "--" when there is
    // no reading.
    QString sUnitsText() const;

    // From AetherSDR src/gui/SMeterWidget.h:32-34 [@0cd4559]
    // NereusSDR appends Alc, AlcGroup, Volts and Amps (Thetis ANANMM's
    // ALC_G, ALC_GROUP, VOLTS and AMPS needles, MeterManager.cs:23837-23894,
    // 24029-24084 [v2.10.3.15]).  Append only: a saved dial stores the
    // value (SMeter_TxSelect) or its name.
    enum class TxMode { Power, SWR, Level, Compression, Alc, AlcGroup, Volts, Amps };
    static constexpr int kTxModeCount = 8;
    // RxMode expanded from AetherSDR's 2-entry enum (SMeter, SMeterPeak) to
    // 4 entries per design doc ss5.4.1.
    // SignalAverage -> WDSP RXA_S_AV (Task 31 WdspEngine::getRxaSignalAverage).
    // MaxBin        -> WDSP GetDetectMaxBin (Task 32 WdspEngine::getMaxBinDbm).
    enum class RxMode { SMeter, SignalAverage, SMeterPeak, MaxBin };
    enum class DecayRate { Fast, Medium, Slow };

    // Meter face.  The first six are the vintage panel-meter themes of
    // gui/VintageMeterFace, in its theme-table order; Classic is the flat
    // AetherSDR / SmartSDR look this widget was ported with.
    // NereusSDR-native; no upstream equivalent.
    enum class FaceStyle { AgedCream, VuAmber, CollinsWhite, Blackface, Carbon, Ice, Classic };
    FaceStyle faceStyle() const { return m_faceStyle; }

    // SettingsStore::Instance only: draw the face chosen on Setup's Meter
    // Styles page (SMeter_FaceStyle, read, never written) and follow it
    // through SMeterSetupFace, until setFaceStyle() gives the copy a face
    // of its own.  A face shown this way does not emit
    // instanceSettingsChanged().  NereusSDR-native.
    void setFollowsSetupFace(bool follow);
    bool followsSetupFace() const { return m_followsSetupFace; }

    // The rectangle the vintage face (bezel, card, scale, pointer) is drawn
    // in (NereusSDR-native): rect(), or for a widget wider than
    // kPlateMaxAspect times its height, a centred rectangle that wide, with
    // whatever is behind the widget showing on both sides.  Every dial (the
    // Main Panel's and added copies alike) keeps the Main Panel dial's
    // proportions this way.  The Classic face has no plate and ignores it.
    QRectF plateRect() const;
    // The Main Panel's dial at its normal width in the mixed-container and
    // profile-dx captures at mac-1512x869@2: the widget's rect, which the
    // plate is drawn in, is 298 x 140 logical px (596 x 280 device px; the
    // bezel's outer edge, 1 px inside, is 296 x 138).
    static constexpr double kPlateMaxAspect = 298.0 / 140.0;

    // Current RX/TX mode accessors.
    // Used by MeterPoller::pollSMeter() (Task 41) to branch on the active
    // WDSP source without coupling the poller to AppSettings.
    // NereusSDR-native (AetherSDR reads mode directly from its combo box).
    RxMode rxMode() const { return m_rxMode; }
    TxMode txMode() const { return m_txMode; }
    // Peak hold state, as the right-click menu shows it.  NereusSDR-native.
    bool peakHoldEnabled() const { return m_peakHoldEnabled; }
    DecayRate peakDecayRate() const;

    // VHF S9 rule (NereusSDR-native wiring of Thetis's): whether the slice
    // the dial reads is at or above the S9 frequency (isAboveS9Frequency,
    // gui/meters/AboveS9Frequency.h).  When it is, a reading is raised by
    // kAboveS9OffsetDb on the S scale and in the S-unit readout, so S9 sits
    // at -93 dBm; the dBm readout stays the reading.  Not saved.
    bool aboveS9Frequency() const { return m_aboveS9Frequency; }

    // Per-copy appearance and dynamics (NereusSDR-native wiring of the
    // Thetis meter item settings; a container copy keeps them in its entry,
    // SMeterDialContent).  An invalid colour or an empty ratio is the face's
    // own, as the dial was drawn before these settings existed.
    QColor peakHoldColor() const { return m_peakHoldColor; }
    // The receive needle (Thetis's primary needle) and the transmit needle
    // (ANANMM's sub needles: power, SWR, ALC, volts, amps).
    QColor needleColor() const { return m_needleColor; }
    QColor subNeedleColor() const { return m_subNeedleColor; }
    bool shadow() const { return m_shadow; }
    // The power scale below its red zone (TX mode Power).
    QColor powerScaleColor() const { return m_powerScaleColor; }
    // A vintage face with a light card draws Blackface's card, ink and
    // pointer; a dark face and the Classic face are already dark.
    bool darkMode() const { return m_darkMode; }
    int updateIntervalMs() const;
    // Thetis ratios: each update moves the needle by this share of the way
    // to the reading.  Empty: the built-in time constants.
    std::optional<float> attackRatio() const { return m_attackRatio; }
    std::optional<float> decayRatio() const { return m_decayRatio; }
    bool showHistory() const { return m_showHistory; }
    QColor historyColor() const { return m_historyColor; }
    int historyDurationMs() const { return m_historyDurationMs; }
    int ignoreHistoryMs() const { return m_ignoreHistoryMs; }

    // The colours a face draws when the copy has none of its own.
    static QColor faceNeedleColor(FaceStyle face, bool darkMode);
    static QColor facePeakHoldColor(FaceStyle face);
    static QColor facePowerScaleColor(FaceStyle face, bool darkMode);

    // Test-only: the history the wedge spans (needle fractions, lowest and
    // highest; the needle itself when empty), its length, one sample (the
    // history timer's tick), and whether a clear's ignore time is running.
    std::pair<float, float> testHistoryRange() const { return historyRange(); }
    int testHistorySize() const { return static_cast<int>(m_history.size()); }
    void testSampleHistory() { sampleHistory(); }
    bool testIgnoringHistory() const { return ignoringHistory(); }
    // Test-only: one needle update `ms` after the last.
    void testStepNeedle(int ms) { moveNeedle(static_cast<float>(ms) / 1000.0f); }
    // Test-only: the needle colour drawn now (receive or transmit).
    QColor testNeedleColorShown() const { return shownNeedleColor(faceNeedleColor(m_faceStyle, m_darkMode)); }

    // Test-only accessors. Production code uses paintEvent's internal logic.
    float testPowerScaleMax()  const { return m_powerScaleMax; }
    float testPowerRedStart()  const { return m_powerRedStart; }

    // Test-only: returns true when the power scale is at the 2 kW (external amp)
    // setting (m_powerScaleMax >= 2000.0f). Used by tst_smeter_widget_external_amp
    // to verify that externalAmpOperateChanged(true) triggers setPowerScale.
    // NereusSDR-native test seam; no upstream equivalent. Task 13.
    bool txScaleIs2kWForTesting() const { return m_powerScaleMax >= 2000.0f; }

    // Test-only: build and return the context menu without exec()-ing it.
    // Allows tests to inspect menu structure without a live event loop.
    // NereusSDR-native test seam; no upstream equivalent.
    QMenu* buildContextMenuForTesting() { return buildContextMenu(this); }

    // Test-only: advance internal synthetic clock by `ms` milliseconds and
    // apply any pending peak hold decay without waiting for real timer ticks.
    // Production code never calls this.
    // From AetherSDR src/gui/SMeterWidget.cpp:214-231 [@0cd4559] (decay logic
    // tested synthetically here).
    void testAdvanceTime(int ms);

    // Test-only: bypass the 100 ms minimum hold-time clamp so tests can set
    // holdTimeMs = 0 and have the full testAdvanceTime(N) count as decay time.
    // Production code uses setPeakHoldTimeMs() which enforces the clamp.
    void testSetPeakHoldTimeMs(int ms) { m_peakHoldTimeMs = ms; }

    // Test-only: read the configurable peak hold level in dBm.
    // Production code reads m_peakHoldDbm indirectly via paintEvent.
    float testPeakLevel() const { return m_peakHoldDbm; }

    // Test-only: the RX readouts both faces draw (S-units on the left, dBm
    // on the right), and whether either peak marker would be drawn.
    // NereusSDR-native test seams; no upstream equivalent.
    QString testSUnitsReadout() const { return rxSUnitsReadout(); }
    QString testDbmReadout() const { return rxDbmReadout(); }
    bool testPeakMarkersShown() const { return peakMarkerVisible() || peakHoldLineVisible(); }

    // Test-only: the TX value the needle heads for and the TX readout the
    // faces draw, in the current TX mode.  NereusSDR-native.
    float testTxValue() const { return currentTxValue(); }
    QString testTxReadout() const { return txReadout(false); }

    // Test-only: where the pointer is heading and where it is now
    // (0.0 = scale minimum, 1.0 = scale maximum).
    float testNeedleTarget() const { return m_targetNeedleFraction; }
    float testNeedleFraction() const { return m_needleFraction; }

signals:
    // R-R3-21: the face, peak hold or peak decay changed (and was saved),
    // from the right-click menu or from Setup.
    void settingsChanged();
    // SettingsStore::Instance only: the TX mode, RX mode, peak hold, decay
    // or face changed (from the right-click menu).  NereusSDR-native.
    void instanceSettingsChanged();

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

    // ALC compression (TxAlcGain, dB) and ALC group (TxAlcGroup, dB), for
    // TxMode::Alc and TxMode::AlcGroup.  NereusSDR-native.
    void setAlcMeters(float alcGain, float alcGroup);

    // PA supply voltage (HwVolts, V) and current (HwAmps, A), for
    // TxMode::Volts and TxMode::Amps.  NereusSDR-native.
    void setSupplyMeters(float volts, float amps);

    // The VHF S9 rule for the slice the dial reads (see aboveS9Frequency()).
    void setAboveS9Frequency(bool above);

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
    // Display geometry only; no transmit or hardware limit is changed.
    void setPowerDisplayScale(double legalMarkerW, double scaleTopW);

    // Peak hold configuration.
    // From AetherSDR src/gui/SMeterWidget.h:57-61 [@0cd4559]
    void setPeakHoldEnabled(bool enabled);
    void setPeakHoldTimeMs(int ms);
    void setPeakDecayRate(DecayRate rate);
    void setPeakDecayRate(const QString& rate);
    void resetPeak();

    // Select the meter face; persists SMeter_FaceStyle.
    // NereusSDR-native; no upstream equivalent.
    void setFaceStyle(FaceStyle style);

    // Per-copy appearance and dynamics (see peakHoldColor()).  None of
    // these reads or writes an AppSettings key.  NereusSDR-native.
    void setPeakHoldColor(const QColor& color);
    void setNeedleColor(const QColor& color);
    void setSubNeedleColor(const QColor& color);
    void setShadow(bool on);
    void setPowerScaleColor(const QColor& color);
    void setDarkMode(bool on);
    // Clamped to kMinUpdateIntervalMs..kMaxUpdateIntervalMs (MeterItem.h).
    void setUpdateIntervalMs(int ms);
    // Clamped to 0..1.
    void setAttackRatio(std::optional<float> ratio);
    void setDecayRatio(std::optional<float> ratio);
    void setShowHistory(bool on);
    void setHistoryColor(const QColor& color);
    // Clamped to kMinBarHistoryDurationMs..kMaxBarHistoryDurationMs.
    void setHistoryDurationMs(int ms);
    // Clamped to kMinBarIgnoreHistoryMs..kMaxBarIgnoreHistoryMs.
    void setIgnoreHistoryMs(int ms);
    // Thetis ClearHistory: the history empties, the next ignoreHistoryMs()
    // of readings reach neither the history nor the peak hold, and the
    // peak hold drops to the reading.  A container copy calls it on each
    // transmit change and on a band change, as Thetis's ZeroOut does.
    void clearHistory();

    // R-R3-49 (remote-window parity Task 33): a TX mode this window has no
    // reading for (a remote window whose Core does not send it).  While
    // transmitting in that mode both faces show "--", the pointer rests at
    // the scale minimum and the widget's tool tip says `reason`, never a 0
    // standing in for a reading.  An empty reason clears it.  The mode stays
    // offered in the right-click menu.  NereusSDR-native.
    void setTxModeUnavailable(TxMode mode, const QString& reason);
    QString txModeUnavailableReason(TxMode mode) const;

    // Persisted name <-> enum for SMeter_FaceStyle, and the menu label.
    // Public for Setup > Appearance > Meter Styles, which offers the same
    // faces (R-R3-21).
    static QString faceStyleKey(FaceStyle style);
    static FaceStyle faceStyleFromKey(const QString& key);
    static QString faceStyleLabel(FaceStyle style);

protected:
    void paintEvent(QPaintEvent* event) override;
    // Right-click context menu delegates to buildContextMenu().
    // NereusSDR replaces AetherSDR's inline settings strip with a
    // right-click menu per design doc ss5.4.2. AetherSDR has no
    // contextMenuEvent; this is a NereusSDR-native UX surface.
    void contextMenuEvent(QContextMenuEvent* ev) override;

private:
    // paintEvent dispatches on m_faceStyle.  paintClassic is the AetherSDR
    // paint body, unchanged; paintVintage draws via gui/VintageMeterFace.
    void paintClassic(QPainter& p);
    void paintVintage(QPainter& p);
    // NereusSDR-native: the Classic face fills rect() and stays opaque; a
    // vintage face leaves its corners outside the bezel clear, so Qt
    // paints what is behind the widget first.  Call after m_faceStyle
    // changes.
    void updatePaintOpacity();


    void updateNeedleTarget();
    void animateNeedle();
    // One step of the needle toward its target (NereusSDR: animateNeedle's
    // body, so a test can take one update).  True when it is there.
    bool moveNeedle(float elapsedSeconds);
    void updatePeakHoldValue();

    // RX readouts both faces draw: S-units ("S7", "S9+20") and dBm
    // ("-97 dBm") for the value on display, or "--" and "-- dBm" when there
    // is no reading.  NereusSDR-native.
    QString rxSUnitsReadout() const;
    QString rxDbmReadout() const;
    // TX readout in the current TX mode ("--" when the mode has no
    // reading); `swrRatio` draws SWR as "1.5 : 1" (the vintage faces).
    // NereusSDR-native (the per-mode text is the faces' existing text).
    QString txReadout(bool swrRatio) const;
    bool currentTxModeUnavailable() const;
    void refreshTxToolTip();

    // Whether a face draws the RX Signal Peak marker / the peak hold line.
    // The conditions both faces share; false while there is no reading.
    // NereusSDR-native.
    bool peakMarkerVisible() const;
    bool peakHoldLineVisible() const;

    // The history (NereusSDR-native port of Thetis clsNeedleItem's): one
    // needle fraction per update interval while it is shown.
    void sampleHistory();
    std::pair<float, float> historyRange() const;
    bool ignoringHistory() const;
    // The needle colour now: the copy's own for receive or transmit, else
    // `faceColor`.
    QColor shownNeedleColor(const QColor& faceColor) const;

    // Connect to RadioModel's cross-vendor external-amp aggregator signals.
    // Called from the RadioModel* constructor overload only.
    // NereusSDR-native. Task 13.
    void connectToRadioModel(RadioModel* model);

    // Build the right-click context menu (TX Mode / RX Mode / Peak Hold).
    // Returns a heap-allocated QMenu* parented to `parent`.
    // NereusSDR-native: AetherSDR does not have a contextMenuEvent;
    // its settings live in an AppletPanel inline strip (removed in Task 40).
    QMenu* buildContextMenu(QObject* parent);

    // Convert TxMode/RxMode enum to the label string accepted by
    // setTxMode() / setRxMode(), so action handlers can call the
    // same public slots used by external callers.
    // NereusSDR-native (no upstream equivalent).
    static QString txModeToLabel(TxMode mode);
    static QString rxModeToLabel(RxMode mode);

    // Map dBm to fraction (0.0 = left, 1.0 = right) for RX S-meter scale
    // From AetherSDR src/gui/SMeterWidget.h:71-72 [@0cd4559]
    float dbmToFraction(float dbm) const;

    // A reading as the S scale positions it: raised by the VHF S9 offset
    // at or above the S9 frequency.  rxFraction() is dbmToFraction() of
    // that.  The scale's ticks use dbmToFraction() directly.
    // NereusSDR-native.
    float sScaleDbm(float dbm) const;
    float rxFraction(float dbm) const { return dbmToFraction(sScaleDbm(dbm)); }

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

    // No reading: set by setLevel() for a level at or below kNoReadingDbm
    // or a non-finite one, cleared by the next real level.  NereusSDR-native.
    bool    m_noReading{false};

    // Visual-change guard for animateNeedle's update() throttle.  Stores
    // the needle fraction last drawn so we can skip the repaint when the
    // physics tick produces sub-pixel movement.  Initialised to a value
    // outside the valid 0..1 range so the first frame always paints.
    float   m_lastDrawnNeedleFraction{-1.0f};

    // TX meter values (updated continuously, used when transmitting)
    // From AetherSDR src/gui/SMeterWidget.h:86-89 [@0cd4559]
    float   m_txPower{0.0f};
    float   m_txSwr{1.0f};
    float   m_micLevel{-50.0f};
    float   m_compLevel{0.0f};
    // NereusSDR: the appended TX modes' values, at the reading Thetis
    // shows with none (MeterManager.cs:2077-2086, 2124-2133 [v2.10.3.15]).
    float   m_alcGain{0.0f};
    float   m_alcGroup{-30.0f};
    float   m_volts{0.0f};
    float   m_amps{0.0f};

    // VHF S9 rule (setAboveS9Frequency).  NereusSDR-native.
    bool    m_aboveS9Frequency{false};

    // Global (the AppSettings keys) or Instance (no AppSettings).
    // NereusSDR-native.
    SettingsStore m_settingsStore{SettingsStore::Global};
    // Instance only: the face shown is Setup's (SMeter_FaceStyle), not the
    // copy's own.  NereusSDR-native.
    bool m_followsSetupFace{false};

    // Mode state
    // From AetherSDR src/gui/SMeterWidget.h:92-94 [@0cd4559]
    TxMode  m_txMode{TxMode::Power};
    // Parity Task 33: why each TX mode has no reading here (by TxMode).
    std::array<QString, kTxModeCount> m_txUnavailable;
    RxMode  m_rxMode{RxMode::SMeter};
    bool    m_transmitting{false};

    // Vintage face: the static artwork (bezel, card, scale, lettering) is
    // rendered once per size / theme / scale and blitted each frame.
    FaceStyle m_faceStyle{FaceStyle::AgedCream};
    QPixmap   m_faceCache;
    QString   m_faceCacheKey;

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

    // Test-only: synthetic time offset added to m_peakHoldTimer.elapsed() in
    // updatePeakHoldValue(). Always 0 in production; testAdvanceTime() sets it.
    qint64         m_testTimeOffsetMs{0};

    // Per-copy appearance and dynamics (setPeakHoldColor() and the rest).
    // NereusSDR-native.
    QColor m_peakHoldColor;
    QColor m_needleColor;
    QColor m_subNeedleColor;
    bool   m_shadow{true};
    QColor m_powerScaleColor;
    bool   m_darkMode{false};
    std::optional<float> m_attackRatio;
    std::optional<float> m_decayRatio;
    bool   m_showHistory{false};
    QColor m_historyColor;              // set in the constructor
    int    m_historyDurationMs{0};      // set in the constructor
    int    m_ignoreHistoryMs{0};        // set in the constructor
    QVector<float> m_history;
    QTimer m_historyTimer;
    // clearHistory()'s ignore time, and the synthetic offset at the clear.
    QElapsedTimer m_ignoreTimer;
    qint64 m_ignoreStartOffsetMs{0};

    // S-unit reference: S0 = -127 dBm, each S-unit = 6 dB. The values (with
    // their AetherSDR cite, src/gui/SMeterWidget.h:114-117 [@0cd4559]) live
    // in core/ControlRanges.h, which the Core's catalogue reads too.
    static constexpr float S0_DBM  = ControlRanges::kSMeterS0Dbm;
    static constexpr float S9_DBM  = ControlRanges::kSMeterS9Dbm;
    static constexpr float MAX_DBM = ControlRanges::kSMeterMaxDbm;  // S9+60
    static constexpr float DB_PER_S = ControlRanges::kSMeterDbPerSUnit;

    // At or below this level, or not finite, the widget has no reading.
    // -400 dBm is the value MeterPoller feeds when Max Bin has nothing to
    // report, locally (passed through unchanged) and on a remote GUI with
    // no reading.  NereusSDR-native.
    static constexpr float kNoReadingDbm = -400.0f;

    // From AetherSDR src/gui/SMeterWidget.h:119-122 [@0cd4559]
    // NereusSDR bench-2026-05-24: bumped 8 -> 33 ms (125 Hz -> 30 Hz).
    // The 8 ms tick was a primary contributor to macOS QCALayerBackingStore
    // backing-store thrash; first cut to 16 ms helped some but the bench
    // was still jittery.  30 Hz still looks smooth for needle motion and
    // matches our spectrum / waterfall display cadence.
    static constexpr int kNeedleAnimationIntervalMs = 33;
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

public:
    // Scales of the appended TX modes: the calibration range of Thetis
    // ANANMM's needle for each reading (linear here; no red zone, as
    // Thetis's needles carry no HighPoint).
    // From Thetis MeterManager.cs:24048-24054 [v2.10.3.15] (ni6, ALC_G "alc_comp")
    static constexpr float kAlcMinDb = 0.0f;
    static constexpr float kAlcMaxDb = 30.0f;
    static constexpr float kAlcStepDb = 5.0f;
    // From Thetis MeterManager.cs:24082-24084 [v2.10.3.15] (ni7, ALC_GROUP)
    static constexpr float kAlcGroupMinDb = -30.0f;
    static constexpr float kAlcGroupMidDb = 0.0f;
    static constexpr float kAlcGroupMaxDb = 25.0f;
    // From Thetis MeterManager.cs:23855-23857 [v2.10.3.15] (ni2, VOLTS)
    static constexpr float kVoltsMin = 10.0f;
    static constexpr float kVoltsMid = 12.5f;
    static constexpr float kVoltsMax = 15.0f;
    // From Thetis MeterManager.cs:23884-23894 [v2.10.3.15] (ni3, AMPS)
    static constexpr float kAmpsMin = 0.0f;
    static constexpr float kAmpsMax = 20.0f;
    static constexpr float kAmpsStep = 2.0f;

    // The update interval a dial has with no setting of its own: the
    // needle animation tick above (NereusSDR-native).
    static constexpr int kDefaultUpdateIntervalMs = kNeedleAnimationIntervalMs;
    // The history defaults: ANANMM's signal needle (AddAnanMM).
    // From Thetis MeterManager.cs:23803 [v2.10.3.15] (ni.HistoryDuration = 4000;)
    static constexpr int kDefaultHistoryDurationMs = 4000;
    // From Thetis MeterManager.cs:23807 [v2.10.3.15] (ni.HistoryColour = FromArgb(64, 233, 51, 50))
    static constexpr QRgb kDefaultHistoryRgba = qRgba(233, 51, 50, 64);
};

} // namespace NereusSDR
