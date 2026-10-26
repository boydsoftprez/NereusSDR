// =================================================================
// src/gui/SMeterWidget.cpp  (NereusSDR)
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
//     (clsNeedleItem's attack / decay ratios, history and ClearHistory,
//     and renderNeedle's history wedge, for a container copy's dial)
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22  J.J. Boyd (KG4VCF): the right-click menu is freed only if
//               it is still there after exec(), since a parent that dies
//               while it is open takes it along. With AI-assisted
//               transformation via Claude Code.
//   2026-10-12 - Add explicit display marker/top scale by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-05-19  Ported in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
//                 Layout from AetherSDR src/gui/SMeterWidget.{h,cpp} [@0cd4559].
//                 Tasks 34/35/36 commit: constructor + setPowerScale +
//                 paintEvent arc/scale/tick/needle/label rendering ported.
//                 contextMenuEvent stubbed (Task 38 implements body).
//                 Peak hold decay logic preserved from upstream (animateNeedle,
//                 updatePeakHoldValue, setPeakHoldEnabled/TimeMs/DecayRate,
//                 resetPeak). Peak hold context-menu wiring (Task 38) deferred.
//   2026-05-19  Task 37: added testAdvanceTime(int ms) + testSetPeakHoldTimeMs(int ms) +
//                 testPeakLevel() test seams; m_testTimeOffsetMs member for synthetic
//                 time injection into updatePeakHoldValue(). No production behavior
//                 changed; decay constants (Fast 20/Medium 10/Slow 5 dB/s) already
//                 ported verbatim from AetherSDR src/gui/SMeterWidget.cpp:675-681 [@0cd4559].
//   2026-05-19  Task 38: right-click context menu implemented. buildContextMenu()
//                 factory builds the TX Mode / RX Mode / Peak Hold submenus.
//                 contextMenuEvent() delegates + exec()s. txModeToLabel() and
//                 rxModeToLabel() helpers convert enum to the label strings used
//                 by setTxMode() / setRxMode(). AppSettings persistence added to
//                 setTxMode() (SMeter_TxSelect int 0..3), setRxMode()
//                 (SMeter_RxSelect int 0..3), setPeakHoldEnabled()
//                 (PeakHoldEnabled "True"/"False"), and setPeakDecayRate(QString)
//                 (PeakDecayRate "Fast"/"Medium"/"Slow").
//                 NereusSDR-native: AetherSDR has no contextMenuEvent; it uses
//                 an inline settings strip in AppletPanel (removed by Task 40).
//   2026-05-19  Task 47: constructor now loads the 4 persisted keys from
//                 AppSettings (SMeter_TxSelect, SMeter_RxSelect,
//                 PeakHoldEnabled, PeakDecayRate) and applies them via the
//                 existing public setters so the widget starts in the saved
//                 state across restarts.
//                 NereusSDR-native (AetherSDR has no AppSettings persistence).
//   2026-05-24  Task 13 (Phase 3P-III): RadioModel* constructor overload +
//                 connectToRadioModel() helper + txScaleIs2kWForTesting() seam.
//                 SMeterWidget now subscribes to RadioModel::externalAmpOperateChanged
//                 (cross-vendor aggregator) rather than PgxlConnection::statusUpdated
//                 directly when constructed via the RadioModel* overload.
//                 Existing QWidget* constructor and MainWindow wiring unchanged.
//   2026-09-18  Vintage meter faces (Lee, AI-assisted via Anthropic Claude
//                 Code).  paintEvent now dispatches on FaceStyle: the AetherSDR
//                 paint body moved unchanged into paintClassic(); paintVintage()
//                 draws the six gui/VintageMeterFace themes (single scale that
//                 follows RX / TX mode, red band above S9 / the TX red line,
//                 lance pointer, S-units and dBm readouts flanking the hub).
//                 "Meter Face" context submenu + SMeter_FaceStyle persistence.
//                 NereusSDR-native; no upstream equivalent.
//   2026-09-22  No-reading display (R-R3-13) by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//                 setLevel() treats a level at or below -400 dBm, or a
//                 non-finite one, as no reading: both faces show "--" and
//                 "-- dBm", the pointer falls to the scale minimum and both
//                 peak markers clear; the next real level displays as before.
//                 The RX readout text and the peak-marker conditions moved
//                 unchanged out of paintClassic() / paintVintage() into
//                 shared helpers.  sUnitsText() returns "--" with no reading.
//                 NereusSDR-native; no upstream equivalent.
//   2026-09-24  The vintage faces' steps above S9 read ControlRanges.h
//                 (iPhone app Task 19, R-IOS-06) by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-09-27  TX modes without a reading (R-R3-49, remote-window parity
//                 Task 33) by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//                 setTxModeUnavailable(): a TX mode a remote window's Core
//                 does not send shows "--" with the reason, never 0.  The TX
//                 readout text of both faces moved unchanged into txReadout().
//                 NereusSDR-native; no upstream equivalent.
//   2026-10-08  Per-copy settings (SettingsStore::Instance) for a dial
//                 added to a container (workspace preview) by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.  The constructor takes the store; an
//                 Instance dial starts at today's defaults, and its setters
//                 emit instanceSettingsChanged() in place of the AppSettings
//                 writes.  The Global (Main Panel) path is unchanged.
//                 NereusSDR-native; no upstream equivalent.
//   2026-10-08  An added dial's face follows Setup's Meter Styles face
//                 (SMeter_FaceStyle, via SMeterSetupFace) until the copy
//                 picks its own; the Main Panel's dial announces the key's
//                 changes.  Test-only plate limit for wide containers:
//                 paintVintage() draws in plateRect(), which is rect()
//                 unless setPlateLimitForTest() is on.  Geometry only; the
//                 face drawing and the background colour are unchanged.
//                 By J.J. Boyd (KG4VCF), with AI-assisted implementation
//                 via Anthropic Claude Code.  NereusSDR-native.
//   2026-10-08  Workspace preview data colours by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//                 The classic face's background and the vintage faces'
//                 surround read WorkspacePreview::dataBackground() (navy
//                 until a choice is made); the face cache key carries it.
//                 NereusSDR-native; no upstream equivalent.
//   2026-10-08  JJ's dial and data colour decisions (D20, D22) by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.  plateRect() limits every dial's plate to
//                 kPlateMaxAspect and centres it (setPlateLimitForTest()
//                 removed).  The vintage faces paint no surround: outside
//                 the bezel the widget is clear and not opaque, so the
//                 container's background shows.  The classic face's
//                 background follows the container's (the user's colour
//                 when set).  NereusSDR-native; no upstream equivalent.
// =================================================================
// Modification history (NereusSDR):
//   2026-10-12 - Apply the approved workspace-only 11 px text minimum.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-26 - TX modes Alc, AlcGroup, Volts and Amps (setAlcMeters,
//                 setSupplyMeters): scales, readouts and both faces' TX
//                 scales from Thetis ANANMM's needles (MeterManager.cs
//                 [v2.10.3.15]); a container copy's menu offers them.  The
//                 VHF S9 rule (setAboveS9Frequency, sScaleDbm/rxFraction):
//                 S9 at -93 dBm at or above 30 MHz.  J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Per-copy appearance and dynamics: peak hold, needle,
//                 transmit needle and power scale colours, shadow, dark
//                 mode, update interval, Thetis attack / decay ratios
//                 (clsNeedleItem.Update) and the history wedge
//                 (renderNeedle) with ClearHistory's ignore time, from
//                 Thetis MeterManager.cs [v2.10.3.15].  J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
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

#include "SMeterWidget.h"
#include "gui/styles/WorkspacePreview.h"

#include <algorithm>
#include <cmath>

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QMenu>
#include <QPointer>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QSet>
#include <QtMath>
#include <QFontMetrics>

#include "core/AppSettings.h"
#include "gui/VintageMeterFace.h"
#include "gui/meters/AboveS9Frequency.h"
#include "gui/meters/MeterItem.h"
#include "models/RadioModel.h"

namespace NereusSDR {

namespace {

// The vintage theme a face draws with: in dark mode a face with a light
// card takes Blackface's card, mirror, ink and pointer, keeping its own
// bezel, hardware and band colours.  NereusSDR-native.
VintageFaceTheme shownTheme(SMeterWidget::FaceStyle face, bool darkMode)
{
    VintageFaceTheme t = VintageMeterFace::theme(static_cast<int>(face));
    if (darkMode && face != SMeterWidget::FaceStyle::Classic && qGray(t.faceTop) > 128) {
        const VintageFaceTheme& dark =
            VintageMeterFace::theme(static_cast<int>(SMeterWidget::FaceStyle::Blackface));
        t.faceTop = dark.faceTop;
        t.faceBottom = dark.faceBottom;
        t.ink = dark.ink;
        t.inkSoft = dark.inkSoft;
        t.mirror = dark.mirror;
        t.mirrorOpa = dark.mirrorOpa;
        t.pointer = dark.pointer;
    }
    return t;
}

// The classic face's colours (the AetherSDR paint body's).
const QColor kClassicNeedle(0xff, 0xff, 0xff);
const QColor kClassicPeakHold(0xff, 0x44, 0x44, 0xcc);
const QColor kClassicTxScale(0x00, 0x80, 0xd0);

} // namespace

SMeterSetupFace& SMeterSetupFace::instance()
{
    static SMeterSetupFace notice;
    return notice;
}

int SMeterSetupFace::current()
{
    return static_cast<int>(SMeterWidget::faceStyleFromKey(
        AppSettings::instance()
            .value("SMeter_FaceStyle", SMeterWidget::faceStyleKey(SMeterWidget::FaceStyle::AgedCream))
            .toString()));
}

// --- Construction ------------------------------------------------------------
// From AetherSDR src/gui/SMeterWidget.cpp:13-47 [@0cd4559]

SMeterWidget::SMeterWidget(QWidget* parent)
    : SMeterWidget(SettingsStore::Global, parent)
{
}

SMeterWidget::SMeterWidget(SettingsStore store, QWidget* parent)
    : QWidget(parent)
    , m_settingsStore(store)
{
    setMinimumSize(minimumSizeHint());
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // Performance: paintEvent fills rect() opaquely as the first step
    // (line ~443).  Tell Qt this widget needs no transparent compositing
    // pass under it — Qt skips the parent backing-store rebuild and saves
    // one IOSurface memmove per paint.  SMeterWidget paints at 30 Hz
    // (needle animation) + on peak decay (20 Hz), so this fires frequently.
    setAttribute(Qt::WA_OpaquePaintEvent);
    // NereusSDR-native: a vintage face is not opaque outside its bezel;
    // updatePaintOpacity() below sets the attribute for the face shown.

    m_needleFraction = dbmToFraction(m_levelDbm);
    m_targetNeedleFraction = m_needleFraction;
    m_peakHoldDecayStartDbm = m_peakHoldDbm;

    // NereusSDR: the history's defaults (ANANMM's signal needle; the
    // ignore time of every Thetis meter item).
    m_historyColor = QColor::fromRgba(kDefaultHistoryRgba);
    m_historyDurationMs = kDefaultHistoryDurationMs;
    m_ignoreHistoryMs = kDefaultBarIgnoreHistoryMs;
    m_historyTimer.setInterval(kNeedleAnimationIntervalMs);
    connect(&m_historyTimer, &QTimer::timeout, this, &SMeterWidget::sampleHistory);

    m_needleAnimation.setTimerType(Qt::PreciseTimer);
    m_needleAnimation.setInterval(kNeedleAnimationIntervalMs);
    connect(&m_needleAnimation, &QTimer::timeout, this, &SMeterWidget::animateNeedle);

    // Peak hold decay: drops 0.5 dB every 50 ms after a new peak
    m_peakDecay.setInterval(50);
    connect(&m_peakDecay, &QTimer::timeout, this, [this]() {
        m_peakDbm -= 0.5f;
        if (m_peakDbm < m_levelDbm) {
            m_peakDbm = m_levelDbm;
            m_peakDecay.stop();
        }
        updateNeedleTarget();
        update();
    });

    // Hard reset peak hold every 10 seconds
    m_peakReset.setInterval(10000);
    m_peakReset.start();
    connect(&m_peakReset, &QTimer::timeout, this, [this]() {
        m_peakDbm = m_levelDbm;
        updateNeedleTarget();
        update();
    });

    // An Instance dial (NereusSDR-native) starts at the defaults the
    // Global path below uses when no key is saved, and reads no key; its
    // owner applies the copy's own settings after construction.
    if (m_settingsStore == SettingsStore::Instance) {
        setTxMode(QStringLiteral("Power"));
        setRxMode(QStringLiteral("Signal"));
        setPeakHoldEnabled(true);
        setPeakDecayRate(QStringLiteral("Medium"));
        m_faceStyle = FaceStyle::AgedCream;
        updatePaintOpacity();
        // Setup's face changed: a copy that follows it shows the new face.
        connect(&SMeterSetupFace::instance(), &SMeterSetupFace::changed, this, [this](int face) {
            if (!m_followsSetupFace) {
                return;
            }
            m_faceStyle = static_cast<FaceStyle>(face);
            updatePaintOpacity();
            m_faceCache = QPixmap();
            m_faceCacheKey.clear();
            update();
        });
        return;
    }

    // Load persisted mode and peak-hold settings from AppSettings.
    // Defaults match the member initialisers: TxSelect=0 (Power),
    // RxSelect=0 (Signal), PeakHoldEnabled=true, PeakDecayRate=Medium.
    // Task 47 - NereusSDR-native (AetherSDR has no AppSettings persistence).
    auto& s = AppSettings::instance();

    const int txSel = s.value("SMeter_TxSelect", 0).toInt();
    const QString txLabels[] = { "Power", "SWR", "Level", "Compression" };
    setTxMode(txLabels[qBound(0, txSel, 3)]);

    const int rxSel = s.value("SMeter_RxSelect", 0).toInt();
    const QString rxLabels[] = { "Signal", "Sig Avg", "Signal Peak", "Max Bin" };
    setRxMode(rxLabels[qBound(0, rxSel, 3)]);

    const bool peakOn = s.value("PeakHoldEnabled", QString("True")).toString() == "True";
    setPeakHoldEnabled(peakOn);

    const QString decayRate = s.value("PeakDecayRate", QString("Medium")).toString();
    setPeakDecayRate(decayRate);

    // Meter face.  Read directly (not via setFaceStyle) so constructing the
    // widget does not write the default back to AppSettings.
    m_faceStyle = faceStyleFromKey(
        s.value("SMeter_FaceStyle", faceStyleKey(FaceStyle::AgedCream)).toString());
    updatePaintOpacity();
}

// RadioModel* overload: delegates to the QWidget* constructor then wires the
// cross-vendor external-amp aggregator signals.
// NereusSDR-native. Task 13 (Phase 3P-III).
SMeterWidget::SMeterWidget(RadioModel* model, QWidget* parent)
    : SMeterWidget(parent)
{
    if (model) {
        connectToRadioModel(model);
    }
}

// Subscribe to RadioModel's cross-vendor external-amp signals.
// Called from the RadioModel* constructor overload only.
// NereusSDR-native; no upstream equivalent. Task 13.
void SMeterWidget::connectToRadioModel(RadioModel* model)
{
    // externalAmpOperateChanged(bool): flip the TX power scale to 2 kW when
    // any external amp enters OPERATE; revert to barefoot scale when all amps
    // leave OPERATE. Mirrors the logic MainWindow wires for the PGXL path.
    connect(model, &RadioModel::externalAmpOperateChanged, this,
            [this](bool inOp) {
        setPowerScale(/*maxWatts=*/0, inOp);
    });

    // externalAmpFwdSwrUpdated(int, float): forward RF-Kit power telemetry to
    // the TX needle. Mirrors how MainWindow feeds ampMetersChanged to the
    // PGXL-sourced S-meter needle.
    connect(model, &RadioModel::externalAmpFwdSwrUpdated, this,
            [this](int fwd, float swr) {
        setTxMeters(static_cast<float>(fwd), swr);
    });
}

// --- Public interface --------------------------------------------------------
// From AetherSDR src/gui/SMeterWidget.cpp:51-143 [@0cd4559]

void SMeterWidget::setLevel(float dbm)
{
    // No reading (R-R3-13, NereusSDR-native).  A level at or below the
    // -400 dBm sentinel, or a non-finite one, is not a signal: the readouts
    // show "--", the pointer falls to the scale minimum (the sentinel maps
    // there through dbmToFraction) and both peak markers clear, so the next
    // real level starts afresh.  Repeats change nothing and repaint nothing.
    if (!std::isfinite(dbm) || dbm <= kNoReadingDbm) {
        if (m_noReading) {
            return;
        }
        m_noReading = true;
        m_levelDbm = kNoReadingDbm;
        m_peakDbm = kNoReadingDbm;
        m_peakDecay.stop();
        m_peakHoldDbm = kNoReadingDbm;
        m_peakHoldDecayStartDbm = kNoReadingDbm;
        m_peakHoldTimerRunning = false;
        updateNeedleTarget();
        if (!m_transmitting) {
            update();
        }
        return;
    }

    // Sub-perceivable change guard.  MeterPoller pushes this 10 times a
    // second; radio noise causes the dBm reading to wiggle by a few tenths
    // even on a quiet band.  Below 0.5 dB the needle position is the same
    // pixel and the S-unit text doesn't change.  Skipping the animation
    // rebuild + repaint for those sub-threshold ticks was the largest
    // single contributor to the macOS paint-pipeline saturation observed
    // in the 2026-05-24 bench profile.
    // NereusSDR (R-R3-13): never skip the first real level after no reading.
    if (!m_noReading && std::abs(dbm - m_levelDbm) < 0.5f) {
        return;
    }
    m_noReading = false;
    m_levelDbm = dbm;

    // Peak hold (existing needle/triangle behavior)
    if (dbm > m_peakDbm) {
        m_peakDbm = dbm;
        m_peakDecay.start();
    }

    // Configurable peak hold line
    if (m_peakHoldEnabled && ignoringHistory()) {
        // NereusSDR: within a clear's ignore time the reading does not
        // reach the peak hold, which rides on the reading (Thetis's peak
        // marker is the history's maximum, and the history ignores it).
        m_peakHoldDbm = dbm;
        m_peakHoldDecayStartDbm = dbm;
        m_peakHoldTimerRunning = false;
    } else if (m_peakHoldEnabled) {
        if (dbm > m_peakHoldDbm) {
            m_peakHoldDbm = dbm;
            m_peakHoldDecayStartDbm = dbm;
            m_peakHoldTimer.start();
            m_peakHoldTimerRunning = true;
        }
        updatePeakHoldValue();
    }

    updateNeedleTarget();

    if (!m_transmitting) {
        update();
    }
}

void SMeterWidget::setTxMeters(float fwdPower, float swr)
{
    m_txPower = fwdPower;
    m_txSwr = swr;

    updateNeedleTarget();

    // Repaint whenever TX power data arrives - either because moxChanged set
    // m_transmitting, or because RF power is flowing regardless (e.g. VOX,
    // hardware-keyed CW, or interlock race where setTransmitting arrives late).
    if (m_transmitting || m_txPower > 0.5f) {
        update();
    }
}

void SMeterWidget::setMicMeters(float micLevel, float compLevel, float micPeak, float compPeak)
{
    Q_UNUSED(micLevel);
    Q_UNUSED(compLevel);
    m_micLevel = micPeak;
    // compPeak is raw dBFS from COMPPEAK. Silence gate at -30.
    const float comp = (compPeak > -30.0f) ? qBound(-25.0f, compPeak, 0.0f) : 0.0f;
    m_compLevel = comp;

    updateNeedleTarget();

    if (m_transmitting && (m_txMode == TxMode::Level || m_txMode == TxMode::Compression)) {
        update();
    }
}

// NereusSDR-native: the appended TX modes' readings.
void SMeterWidget::setAlcMeters(float alcGain, float alcGroup)
{
    m_alcGain = alcGain;
    m_alcGroup = alcGroup;
    updateNeedleTarget();
    if (m_transmitting && (m_txMode == TxMode::Alc || m_txMode == TxMode::AlcGroup)) {
        update();
    }
}

void SMeterWidget::setSupplyMeters(float volts, float amps)
{
    m_volts = volts;
    m_amps = amps;
    updateNeedleTarget();
    if (m_transmitting && (m_txMode == TxMode::Volts || m_txMode == TxMode::Amps)) {
        update();
    }
}

// The VHF S9 rule for the dial's slice.  NereusSDR-native.
void SMeterWidget::setAboveS9Frequency(bool above)
{
    if (m_aboveS9Frequency == above) {
        return;
    }
    m_aboveS9Frequency = above;
    updateNeedleTarget();
    update();
}

// A reading on the S scale: Thetis raises a signal-strength reading by
// dbmOffsetForAboveS9Frequency at or above the S9 frequency, so a scale
// with S9 at -73 dBm shows S9 at -93 dBm (kAboveS9OffsetDb,
// AboveS9Frequency.h).
float SMeterWidget::sScaleDbm(float dbm) const
{
    return m_aboveS9Frequency ? dbm + static_cast<float>(kAboveS9OffsetDb) : dbm;
}

void SMeterWidget::setTransmitting(bool tx)
{
    m_transmitting = tx;
    if (!tx) {
        // Clear TX values immediately on un-key so the RX reading becomes the
        // animation target as soon as transmit ends.
        m_txPower = 0.0f;
        m_txSwr   = 1.0f;
    }
    refreshTxToolTip();
    updateNeedleTarget();
    update();
}

void SMeterWidget::setTxMode(const QString& mode)
{
    if (mode == "Power")            m_txMode = TxMode::Power;
    else if (mode == "SWR")         m_txMode = TxMode::SWR;
    else if (mode == "Level")       m_txMode = TxMode::Level;
    else if (mode == "Compression") m_txMode = TxMode::Compression;
    // NereusSDR: the appended modes, by txModeToLabel()'s names.
    for (TxMode appended : {TxMode::Alc, TxMode::AlcGroup, TxMode::Volts, TxMode::Amps}) {
        if (mode == txModeToLabel(appended)) {
            m_txMode = appended;
        }
    }

    // Persist TX mode selection.
    // Key SMeter_TxSelect (int 0..3): Power=0, SWR=1, Level=2, Compression=3.
    // NereusSDR-native key per design doc ss5.4.2.
    if (m_settingsStore == SettingsStore::Global) {
        AppSettings::instance().setValue("SMeter_TxSelect",
                                         static_cast<int>(m_txMode));
    } else {
        emit instanceSettingsChanged();
    }
    refreshTxToolTip();
    updateNeedleTarget();
    update();
}

// NereusSDR extends upstream's 2-mode dispatch to 4 modes.
// Upstream (AetherSDR src/gui/SMeterWidget.cpp:133-144 [@0cd4559]):
//   "S-Meter" -> SMeter, anything else -> SMeterPeak.
// NereusSDR adds "Sig Avg" (SignalAverage) and "Max Bin" (MaxBin)
// per design doc ss5.4.1.
// Context menu uses "Signal" / "Sig Avg" / "Signal Peak" / "Max Bin"
// per design doc ss5.4.2; "S-Meter" is the legacy external-caller form.
void SMeterWidget::setRxMode(const QString& mode)
{
    if (mode == "S-Meter" || mode == "Signal") {
        m_rxMode = RxMode::SMeter;
        m_source = "S-Meter";
    } else if (mode == "Sig Avg") {
        m_rxMode = RxMode::SignalAverage;
        m_source = "Sig Avg";
    } else if (mode == "Signal Peak") {
        m_rxMode = RxMode::SMeterPeak;
        m_source = "S-Meter Peak";
    } else if (mode == "Max Bin") {
        m_rxMode = RxMode::MaxBin;
        m_source = "Max Bin";
    } else {
        // Default covers "S-Meter Peak" and any unrecognized string
        m_rxMode = RxMode::SMeterPeak;
        m_source = "S-Meter Peak";
    }

    // Persist RX mode selection.
    // Key SMeter_RxSelect (int 0..3): Signal=0, SignalAverage=1, SignalPeak=2, MaxBin=3.
    // Range widened from AetherSDR's 0..1 per design doc ss5.4.2.
    if (m_settingsStore == SettingsStore::Global) {
        AppSettings::instance().setValue("SMeter_RxSelect",
                                         static_cast<int>(m_rxMode));
    } else {
        emit instanceSettingsChanged();
    }
    updateNeedleTarget();
    update();
}

// --- Needle target -----------------------------------------------------------
// From AetherSDR src/gui/SMeterWidget.cpp:146-178 [@0cd4559]

void SMeterWidget::updateNeedleTarget()
{
    updatePeakHoldValue();

    if (m_transmitting && currentTxModeUnavailable()) {
        // Parity Task 33: no reading in this TX mode; the pointer rests.
        m_targetNeedleFraction = 0.0f;
    } else if (m_transmitting) {
        m_targetNeedleFraction = txValueToFraction(currentTxValue());
    } else if (m_rxMode == RxMode::SMeterPeak) {
        m_targetNeedleFraction = rxFraction(m_peakDbm);
    } else {
        // SMeter, SignalAverage, and MaxBin all use m_levelDbm as target;
        // the *source* of that value differs (caller feeds the right WDSP meter).
        m_targetNeedleFraction = rxFraction(m_levelDbm);
    }

    const bool needleAtTarget = qAbs(m_targetNeedleFraction - m_needleFraction) <= kNeedleSnapEpsilon;
    if (needleAtTarget) {
        m_needleFraction = m_targetNeedleFraction;
    }

    const bool peakHoldAnimating = m_peakHoldEnabled
        && m_peakHoldTimerRunning
        && m_peakHoldTimer.elapsed() > m_peakHoldTimeMs
        && m_peakHoldDbm > m_levelDbm + 0.01f;

    if (needleAtTarget && !peakHoldAnimating) {
        if (m_needleAnimation.isActive()) {
            m_needleAnimation.stop();
        }
        return;
    }

    if (!m_needleAnimation.isActive()) {
        m_needleElapsed.restart();
        m_needleAnimation.start();
    }
}

// From AetherSDR src/gui/SMeterWidget.cpp:181-211 [@0cd4559]
void SMeterWidget::animateNeedle()
{
    const qint64 elapsedMs = m_needleElapsed.restart();
    if (elapsedMs <= 0) {
        return;
    }

    updatePeakHoldValue();

    const bool needleAtTarget = moveNeedle(static_cast<float>(elapsedMs) / 1000.0f);

    const bool peakHoldAnimating = m_peakHoldEnabled
        && m_peakHoldTimerRunning
        && m_peakHoldTimer.elapsed() > m_peakHoldTimeMs
        && m_peakHoldDbm > m_levelDbm + 0.01f;

    if (needleAtTarget && !peakHoldAnimating) {
        m_needleAnimation.stop();
    }

    // Visual-change guard.  The animation tick computes a new needle
    // fraction every 33 ms, but when the needle is settled or moving by
    // sub-pixel amounts the repaint is wasted work (paintEvent is the
    // single most expensive widget paint in the app per profile).  Skip
    // the update() unless the needle moved by at least 0.005 (1 pixel
    // on a 200 px arc) OR the peak hold marker is animating its decay.
    if (std::abs(m_needleFraction - m_lastDrawnNeedleFraction) > 0.005f
        || peakHoldAnimating) {
        m_lastDrawnNeedleFraction = m_needleFraction;
        update();
    }
}

// One step of the needle toward its target, `elapsedSeconds` after the
// last; true when it is there.  The body is animateNeedle()'s, moved here
// unchanged (NereusSDR) apart from the copy's ratios.
bool SMeterWidget::moveNeedle(float elapsedSeconds)
{
    const float delta = m_targetNeedleFraction - m_needleFraction;
    const float timeConstant = (delta >= 0.0f) ? kNeedleAttackTimeSeconds
                                                : kNeedleReleaseTimeSeconds;
    // NereusSDR: a copy's own attack or decay ratio moves the needle that
    // share of the way to the reading on each update, as Thetis does.
    // From Thetis MeterManager.cs:22013-22016 [v2.10.3.15] (clsNeedleItem.Update)
    const std::optional<float> ratio = (delta >= 0.0f) ? m_attackRatio : m_decayRatio;
    const float alpha = ratio ? *ratio : 1.0f - std::exp(-elapsedSeconds / timeConstant);
    const bool needleAtTarget = qAbs(delta) <= kNeedleSnapEpsilon;
    if (needleAtTarget) {
        m_needleFraction = m_targetNeedleFraction;
    } else {
        m_needleFraction += delta * alpha;
    }
    return needleAtTarget;
}

// From AetherSDR src/gui/SMeterWidget.cpp:214-231 [@0cd4559]
// m_testTimeOffsetMs is 0 in production; testAdvanceTime() sets it for unit tests.
void SMeterWidget::updatePeakHoldValue()
{
    if (!m_peakHoldEnabled || !m_peakHoldTimerRunning) {
        return;
    }

    const qint64 elapsedMs = m_peakHoldTimer.elapsed() + m_testTimeOffsetMs;
    if (elapsedMs <= m_peakHoldTimeMs) {
        return;
    }

    const float decayElapsedSeconds =
        static_cast<float>(elapsedMs - m_peakHoldTimeMs) / 1000.0f;
    m_peakHoldDbm = m_peakHoldDecayStartDbm - (m_peakDecayDbPerSec * decayElapsedSeconds);
    if (m_peakHoldDbm <= m_levelDbm) {
        m_peakHoldDbm = m_levelDbm;
    }
}

// Test-only: advance synthetic time offset by ms and apply pending decay.
// Not called in production.  Each call to testAdvanceTime accumulates; call
// resetPeak() or re-enable peak hold between test cases to reset state.
void SMeterWidget::testAdvanceTime(int ms)
{
    m_testTimeOffsetMs += static_cast<qint64>(ms);
    updatePeakHoldValue();
}

// --- sUnitsText --------------------------------------------------------------
// From AetherSDR src/gui/SMeterWidget.cpp:233-242 [@0cd4559]

QString SMeterWidget::sUnitsText() const
{
    // NereusSDR: no reading has no S-unit value (R-R3-13).
    if (m_noReading) {
        return QStringLiteral("--");
    }
    // NereusSDR: on the S scale (the VHF S9 rule).
    const float level = sScaleDbm(m_levelDbm);
    if (level <= S0_DBM) return "S0";
    if (level <= S9_DBM) {
        const int s = qRound((level - S0_DBM) / DB_PER_S);
        return QString("S%1").arg(qBound(0, s, 9));
    }
    const int over = qRound(level - S9_DBM);
    return QString("S9+%1").arg(over);
}

// --- RX readouts and peak markers --------------------------------------------
// Shared by paintClassic() and paintVintage().  The S-unit / dBm formatting
// and the two marker conditions are the ones both paint bodies used inline,
// moved here unchanged; only the no-reading cases are new.  NereusSDR-native
// (R-R3-13).

QString SMeterWidget::rxSUnitsReadout() const
{
    if (m_noReading) {
        return QStringLiteral("--");
    }
    // NereusSDR: on the S scale (the VHF S9 rule); the dBm readout is not.
    const float displayDbm = sScaleDbm((m_rxMode == RxMode::SMeterPeak) ? m_peakDbm : m_levelDbm);
    if (displayDbm <= S0_DBM) {
        return QStringLiteral("S0");
    }
    if (displayDbm <= S9_DBM) {
        return QString("S%1").arg(qBound(0, qRound((displayDbm - S0_DBM) / DB_PER_S), 9));
    }
    return QString("S9+%1").arg(qRound(displayDbm - S9_DBM));
}

QString SMeterWidget::rxDbmReadout() const
{
    if (m_noReading) {
        return QStringLiteral("-- dBm");
    }
    const float displayDbm = (m_rxMode == RxMode::SMeterPeak) ? m_peakDbm : m_levelDbm;
    return QString("%1 dBm").arg(displayDbm, 0, 'f', 0);
}

// Peak marker (classic triangle / vintage wedge): RX Signal Peak mode only.
bool SMeterWidget::peakMarkerVisible() const
{
    return !m_noReading && !m_transmitting && m_rxMode == RxMode::SMeterPeak
        && m_peakDbm > m_levelDbm + 1.0f;
}

// Peak hold line: configurable overlay, independent of RX mode.
bool SMeterWidget::peakHoldLineVisible() const
{
    return !m_noReading && m_peakHoldEnabled && !m_transmitting
        && sScaleDbm(m_peakHoldDbm) > S0_DBM + 1.0f;
}

// --- Mapping -----------------------------------------------------------------
// From AetherSDR src/gui/SMeterWidget.cpp:246-258 [@0cd4559]

float SMeterWidget::dbmToFraction(float dbm) const
{
    // S0 to S9 occupies the left 60% of the arc
    // S9 to S9+60 occupies the right 40%
    const float clamped = qBound(S0_DBM, dbm, MAX_DBM);

    if (clamped <= S9_DBM) {
        // Linear within S0..S9 -> 0.0..0.6
        return 0.6f * (clamped - S0_DBM) / (S9_DBM - S0_DBM);
    }
    // Linear within S9..S9+60 -> 0.6..1.0
    return 0.6f + 0.4f * (clamped - S9_DBM) / (MAX_DBM - S9_DBM);
}

// From AetherSDR src/gui/SMeterWidget.cpp:260-276 [@0cd4559]
float SMeterWidget::txValueToFraction(float value) const
{
    switch (m_txMode) {
    case TxMode::Power:
        return qBound(0.0f, value / m_powerScaleMax, 1.0f);
    case TxMode::SWR:
        // 1.0-3.0
        return qBound(0.0f, (value - 1.0f) / 2.0f, 1.0f);
    case TxMode::Level:
        // -40 to +5
        return qBound(0.0f, (value + 40.0f) / 45.0f, 1.0f);
    case TxMode::Compression:
        // Gain reduction: 0 = none, -25 = heavy compression
        return qBound(0.0f, (value + 25.0f) / 25.0f, 1.0f);
    // NereusSDR: the appended modes, linear over Thetis's calibration range.
    case TxMode::Alc:
        return qBound(0.0f, (value - kAlcMinDb) / (kAlcMaxDb - kAlcMinDb), 1.0f);
    case TxMode::AlcGroup:
        return qBound(0.0f, (value - kAlcGroupMinDb) / (kAlcGroupMaxDb - kAlcGroupMinDb), 1.0f);
    case TxMode::Volts:
        return qBound(0.0f, (value - kVoltsMin) / (kVoltsMax - kVoltsMin), 1.0f);
    case TxMode::Amps:
        return qBound(0.0f, (value - kAmpsMin) / (kAmpsMax - kAmpsMin), 1.0f);
    }
    return 0.0f;
}

// Parity Task 33 (R-R3-49): the TX readout both faces draw.  The per-mode
// text is the faces' own (classic: "1.5", vintage: "1.5 : 1"); a mode with
// no reading here reads "--".  NereusSDR-native.
QString SMeterWidget::txReadout(bool swrRatio) const
{
    if (currentTxModeUnavailable()) {
        return QStringLiteral("--");
    }
    switch (m_txMode) {
    case TxMode::Power:       return QString("%1 W").arg(m_txPower, 0, 'f', 0);
    case TxMode::SWR:         return swrRatio ? QString("%1 : 1").arg(m_txSwr, 0, 'f', 1)
                                              : QString("%1").arg(m_txSwr, 0, 'f', 1);
    case TxMode::Level:       return QString("%1 dB").arg(m_micLevel, 0, 'f', 0);
    case TxMode::Compression: return QString("%1 dB").arg(m_compLevel, 0, 'f', 0);
    // Units from Thetis MeterManager.cs:2333-2334, 2337, 2366 [v2.10.3.15] (ReadingUnits)
    case TxMode::Alc:         return QString("%1 dB").arg(m_alcGain, 0, 'f', 0);
    case TxMode::AlcGroup:    return QString("%1 dB").arg(m_alcGroup, 0, 'f', 0);
    case TxMode::Volts:       return QString("%1 V").arg(m_volts, 0, 'f', 1);
    case TxMode::Amps:        return QString("%1 A").arg(m_amps, 0, 'f', 1);
    }
    return QString();
}

bool SMeterWidget::currentTxModeUnavailable() const
{
    return !m_txUnavailable[static_cast<std::size_t>(m_txMode)].isEmpty();
}

void SMeterWidget::setTxModeUnavailable(TxMode mode, const QString& reason)
{
    QString& slot = m_txUnavailable[static_cast<std::size_t>(mode)];
    if (slot == reason) {
        return;
    }
    slot = reason;
    refreshTxToolTip();
    updateNeedleTarget();
    update();
}

QString SMeterWidget::txModeUnavailableReason(TxMode mode) const
{
    return m_txUnavailable[static_cast<std::size_t>(mode)];
}

void SMeterWidget::refreshTxToolTip()
{
    // The reason shows while it applies: transmitting in that mode.
    setToolTip(m_transmitting ? m_txUnavailable[static_cast<std::size_t>(m_txMode)] : QString());
}

// From AetherSDR src/gui/SMeterWidget.cpp:278-287 [@0cd4559]
float SMeterWidget::currentTxValue() const
{
    switch (m_txMode) {
    case TxMode::Power:       return m_txPower;
    case TxMode::SWR:         return m_txSwr;
    case TxMode::Level:       return m_micLevel;
    case TxMode::Compression: return m_compLevel;
    case TxMode::Alc:         return m_alcGain;
    case TxMode::AlcGroup:    return m_alcGroup;
    case TxMode::Volts:       return m_volts;
    case TxMode::Amps:        return m_amps;
    }
    return 0.0f;
}

// --- Paint -------------------------------------------------------------------
// From AetherSDR src/gui/SMeterWidget.cpp:291-640 [@0cd4559]

void SMeterWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (m_faceStyle == FaceStyle::Classic) {
        paintClassic(p);
    } else {
        paintVintage(p);
    }
}

// The AetherSDR paint body, unchanged.
// NereusSDR 2026-09-22 (R-R3-13): the RX readout text and the two peak-marker
// conditions now come from the helpers above, shared with paintVintage(), so
// both faces show the no-reading display.
void SMeterWidget::paintClassic(QPainter& p)
{
    const int w = width();
    const int h = height();

    // Background
    p.fillRect(rect(), WorkspacePreview::dataBackground(QColor(0x0f, 0x0f, 0x1a), "panel", this));

    // -- Arc geometry ---------------------------------------------------------
    // Large radius with center far below widget -> shallow ~70 deg arc segment
    const float cx = w * 0.5f;
    const float radius = w * 0.85f;
    const float cy = h + radius - h * 0.65f;  // arc center well below widget
    const float needleCy = h + 6.0f;          // needle origin just below widget bottom

    // Convert arc degrees to radians
    const float arcStartRad = qDegreesToRadians(ARC_START_DEG);
    const float arcEndRad   = qDegreesToRadians(ARC_END_DEG);
    const float arcSpanRad  = arcEndRad - arcStartRad;

    // fraction 0.0 -> left end (ARC_END_DEG), fraction 1.0 -> right end (ARC_START_DEG)
    auto fractionToAngle = [&](float frac) -> float {
        return arcEndRad - frac * arcSpanRad;  // radians
    };

    // -- Draw colored outer arc (RX scale) ------------------------------------
    // White from S0 to S9, red from S9+
    {
        const QRectF outerArc(cx - radius, cy - radius, radius * 2, radius * 2);
        const float s9Deg = qRadiansToDegrees(fractionToAngle(0.6f));

        QPen whitePen(QColor(0xc8, 0xd8, 0xe8), 3);
        p.setPen(whitePen);
        p.drawArc(outerArc,
                  static_cast<int>(s9Deg * 16),
                  static_cast<int>((ARC_END_DEG - s9Deg) * 16));

        QPen redPen(QColor(0xff, 0x44, 0x44), 3);
        p.setPen(redPen);
        p.drawArc(outerArc,
                  static_cast<int>(ARC_START_DEG * 16),
                  static_cast<int>((s9Deg - ARC_START_DEG) * 16));
    }

    // -- Draw colored inner arc (TX scale) -- 6px gap -------------------------
    const float arcGap = 6.0f;
    const QColor blueColor(0x00, 0x80, 0xd0);
    const QColor redColor(0xff, 0x44, 0x44);
    // NereusSDR: a copy's power scale colour below the red zone.
    const QColor powerColor = m_powerScaleColor.isValid() ? m_powerScaleColor : blueColor;
    const QColor& lowArcColor = (m_txMode == TxMode::Power) ? powerColor : blueColor;
    {
        const float innerR = radius - arcGap;
        const QRectF innerArc(cx - innerR, cy - innerR, innerR * 2, innerR * 2);

        // Determine where the arc color splits (fraction where red begins)
        float redFrac = -1.0f;  // -1 = no red zone
        switch (m_txMode) {
        case TxMode::Power:       redFrac = m_powerRedStart / m_powerScaleMax; break;
        case TxMode::SWR:         redFrac = (2.5f - 1.0f) / 2.0f; break; // 0.75
        case TxMode::Level:       redFrac = (0.0f + 40.0f) / 45.0f; break; // ~0.89
        case TxMode::Compression: redFrac = -1.0f; break; // all blue
        // NereusSDR: Thetis's needles for these carry no red zone.
        case TxMode::Alc:
        case TxMode::AlcGroup:
        case TxMode::Volts:
        case TxMode::Amps:        redFrac = -1.0f; break;
        }

        if (redFrac < 0.0f) {
            // Entire arc is blue
            p.setPen(QPen(blueColor, 3));
            p.drawArc(innerArc,
                      static_cast<int>(ARC_START_DEG * 16),
                      static_cast<int>((ARC_END_DEG - ARC_START_DEG) * 16));
        } else {
            const float splitDeg = qRadiansToDegrees(fractionToAngle(redFrac));
            p.setPen(QPen(lowArcColor, 3));
            p.drawArc(innerArc,
                      static_cast<int>(splitDeg * 16),
                      static_cast<int>((ARC_END_DEG - splitDeg) * 16));
            p.setPen(QPen(redColor, 3));
            p.drawArc(innerArc,
                      static_cast<int>(ARC_START_DEG * 16),
                      static_cast<int>((splitDeg - ARC_START_DEG) * 16));
        }
    }

    // -- Tick drawing helpers -------------------------------------------------
    QFont tickFont = font();
    tickFont.setPixelSize(WorkspacePreview::readableTextPixelSize(qMax(10, h / 10)));
    tickFont.setBold(true);
    p.setFont(tickFont);
    const QFontMetrics tfm(tickFont);

    // Direction from needle origin through arc point, normalized
    auto needleDir = [&](float angle) -> std::pair<float, float> {
        const float arcX = cx + radius * std::cos(angle);
        const float arcY = cy - radius * std::sin(angle);
        const float dx = arcX - cx;
        const float dy = arcY - needleCy;
        const float len = std::sqrt(dx * dx + dy * dy);
        return {dx / len, dy / len};
    };

    // Outside tick (RX): extends outward from the arc, label above
    auto drawOutsideTick = [&](float frac, const QString& label, const QColor& color,
                               bool showLabel) {
        const float angle = fractionToAngle(frac);
        const float arcX = cx + radius * std::cos(angle);
        const float arcY = cy - radius * std::sin(angle);
        auto [ux, uy] = needleDir(angle);

        const QPointF inner(arcX + 2 * ux, arcY + 2 * uy);
        const QPointF outer(arcX + 14 * ux, arcY + 14 * uy);

        p.setPen(QPen(color, 1.5));
        p.drawLine(inner, outer);

        if (showLabel) {
            const QPointF labelPt(arcX + 26 * ux, arcY + 26 * uy);
            const int tw = tfm.horizontalAdvance(label);
            p.setPen(color);
            p.drawText(QPointF(labelPt.x() - tw / 2.0,
                               labelPt.y() + tfm.ascent() / 2.0), label);
        }
    };

    // Inside tick (TX): extends inward from the inner colored arc
    const float innerArcR = radius - arcGap;
    auto drawInsideTick = [&](float frac, const QString& label,
                              const QColor& tickColor, const QColor& labelColor,
                              bool showLabel) {
        const float angle = fractionToAngle(frac);
        // Start from the inner colored arc, not the outer arc
        const float iArcX = cx + innerArcR * std::cos(angle);
        const float iArcY = cy - innerArcR * std::sin(angle);
        auto [ux, uy] = needleDir(angle);

        const QPointF outer(iArcX - 2 * ux, iArcY - 2 * uy);
        const QPointF inner(iArcX - 14 * ux, iArcY - 14 * uy);

        p.setPen(QPen(tickColor, 1.5));
        p.drawLine(inner, outer);

        if (showLabel) {
            const QPointF labelPt(iArcX - 26 * ux, iArcY - 26 * uy);
            const int tw = tfm.horizontalAdvance(label);
            p.setPen(labelColor);
            p.drawText(QPointF(labelPt.x() - tw / 2.0,
                               labelPt.y() + tfm.ascent() / 2.0), label);
        }
    };

    const QColor whiteColor(0xc8, 0xd8, 0xe8);

    // -- Outside ticks (RX): S-meter scale -- odd S-units only ----------------
    for (int s = 1; s <= 9; s += 2) {
        const float dbm = S0_DBM + s * DB_PER_S;
        drawOutsideTick(dbmToFraction(dbm), QString::number(s), whiteColor, true);
    }
    for (int over : {20, 40}) {
        const float dbm = S9_DBM + over;
        drawOutsideTick(dbmToFraction(dbm), QString("+%1").arg(over), redColor, true);
    }

    // -- Inside ticks (TX): scale depends on TX mode --------------------------
    switch (m_txMode) {
    case TxMode::Power: {
        // Dynamic scale based on m_powerScaleMax
        int maxW = static_cast<int>(m_powerScaleMax);
        int redW = static_cast<int>(m_powerRedStart);
        int tickStep, labelStep;
        if (maxW >= 2000) {         // PGXL: ticks every 100W, labels every 500W
            tickStep = 100; labelStep = 500;
        } else if (maxW >= 600) {   // Aurora: ticks every 50W, labels every 100W
            tickStep = 50; labelStep = 100;
        } else {                    // Barefoot: ticks every 10W, labels every 40W
            tickStep = 10; labelStep = 40;
        }
        for (int pw = 0; pw <= maxW; pw += tickStep) {
            const float frac = static_cast<float>(pw) / m_powerScaleMax;
            const QColor& tc = (pw >= redW) ? redColor : powerColor;
            const QColor& lc = (pw >= redW) ? redColor : whiteColor;
            bool isLabeled = (pw % labelStep == 0) || pw == maxW || pw == redW;
            QString label = (pw >= 1000) ? QString("%1k").arg(pw / 1000.0f, 0, 'f', (pw % 1000) ? 1 : 0)
                                         : QString::number(pw);
            drawInsideTick(frac, label, tc, lc, isLabeled);
        }
        break;
    }
    case TxMode::SWR: {
        // 1.0-3.0, ticks at 1, 1.5, 2, 2.5, 3.  Red starting at 2.5.
        for (float s : {1.0f, 1.5f, 2.0f, 2.5f, 3.0f}) {
            const float frac = (s - 1.0f) / 2.0f;
            const bool red = (s >= 2.5f);
            const QColor& tc = red ? redColor : blueColor;
            const QColor& lc = red ? redColor : whiteColor;
            QString label = (s == static_cast<int>(s))
                ? QString::number(static_cast<int>(s))
                : QString::number(s, 'f', 1);
            drawInsideTick(frac, label, tc, lc, true);
        }
        break;
    }
    case TxMode::Level: {
        // -40 to +5, ticks at -40, -30, -20, -10, 0.  Red at 0.
        for (int db : {-40, -30, -20, -10, 0}) {
            const float frac = (db + 40.0f) / 45.0f;
            const bool red = (db >= 0);
            const QColor& tc = red ? redColor : blueColor;
            const QColor& lc = red ? redColor : whiteColor;
            drawInsideTick(frac, QString::number(db), tc, lc, true);
        }
        break;
    }
    case TxMode::Compression: {
        // -25 to 0, ticks at -25, -20, -15, -10, -5, 0.  All default color.
        for (int db : {-25, -20, -15, -10, -5, 0}) {
            const float frac = (db + 25.0f) / 25.0f;
            drawInsideTick(frac, QString::number(db), blueColor, whiteColor, true);
        }
        break;
    }
    // NereusSDR: the appended modes, ticks at Thetis's calibration keys
    // (all blue, no red zone).
    case TxMode::Alc: {
        for (float db = kAlcMinDb; db <= kAlcMaxDb; db += kAlcStepDb) {
            drawInsideTick(txValueToFraction(db), QString::number(static_cast<int>(db)),
                           blueColor, whiteColor, true);
        }
        break;
    }
    case TxMode::AlcGroup: {
        for (float db : {kAlcGroupMinDb, kAlcGroupMidDb, kAlcGroupMaxDb}) {
            drawInsideTick(txValueToFraction(db), QString::number(static_cast<int>(db)),
                           blueColor, whiteColor, true);
        }
        break;
    }
    case TxMode::Volts: {
        for (float v : {kVoltsMin, kVoltsMid, kVoltsMax}) {
            drawInsideTick(txValueToFraction(v), QString::number(static_cast<double>(v), 'g', 3),
                           blueColor, whiteColor, true);
        }
        break;
    }
    case TxMode::Amps: {
        // Every key ticked, every second one labelled.
        for (float a = kAmpsMin; a <= kAmpsMax; a += kAmpsStep) {
            const bool labelled = std::fmod(a - kAmpsMin, 2.0f * kAmpsStep) == 0.0f;
            drawInsideTick(txValueToFraction(a), QString::number(static_cast<int>(a)),
                           blueColor, whiteColor, labelled);
        }
        break;
    }
    }

    // -- Draw needle ----------------------------------------------------------
    // Needle originates from needleCy (just below widget) rather than the
    // arc center, so the pivot is barely out of frame.
    // When transmitting, needle tracks the selected TX meter instead of RX.
    // NereusSDR: the history wedge, from the needle's origin out to its
    // tip between the lowest and highest positions in the history, filled
    // in the history colour; drawn under the needle as Thetis does.
    // From Thetis MeterManager.cs:40785-40852 [v2.10.3.15] (renderNeedle)
    if (m_showHistory) {
        const auto [lo, hi] = historyRange();
        const float tipR = radius + 14;
        QPolygonF wedge;
        wedge << QPointF(cx, needleCy);
        const int steps = std::max(1, static_cast<int>(std::ceil((hi - lo) * 100.0f)));
        for (int i = 0; i <= steps; ++i) {
            const float angle = fractionToAngle(lo + (hi - lo) * static_cast<float>(i) / steps);
            wedge << QPointF(cx + tipR * std::cos(angle), cy - tipR * std::sin(angle));
        }
        p.setPen(Qt::NoPen);
        p.setBrush(m_historyColor);
        p.drawPolygon(wedge);
        p.setBrush(Qt::NoBrush);
    }

    {
        const float angle = fractionToAngle(m_needleFraction);

        // Needle extends to the end of the outer (RX) ticks: radius + 14
        const float tipR = radius + 14;
        const float tipX = cx + tipR * std::cos(angle);
        const float tipY = cy - tipR * std::sin(angle);

        // Needle shadow
        // NereusSDR: unless the copy turns it off.
        if (m_shadow) {
            p.setPen(QPen(QColor(0, 0, 0, 80), 3));
            p.drawLine(QPointF(cx + 1, needleCy + 1), QPointF(tipX + 1, tipY + 1));
        }

        // Needle
        // NereusSDR: in the copy's receive or transmit needle colour.
        p.setPen(QPen(shownNeedleColor(kClassicNeedle), 2));
        p.drawLine(QPointF(cx, needleCy), QPointF(tipX, tipY));
    }

    // Draw peak marker (small triangle) - only in RX S-Meter Peak mode
    if (peakMarkerVisible()) {
        const float frac = rxFraction(m_peakDbm);
        const float angle = fractionToAngle(frac);
        const float markerR = radius - 2;

        const float cosA = std::cos(angle);
        const float sinA = std::sin(angle);

        const QPointF tip(cx + markerR * cosA, cy - markerR * sinA);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0xff, 0xaa, 0x00));
        const float perpCos = -sinA;
        const float perpSin = cosA;
        const float sz = 3.0f;
        QPainterPath tri;
        tri.moveTo(tip);
        tri.lineTo(tip.x() - 6 * cosA + sz * perpCos,
                   tip.y() + 6 * sinA + sz * perpSin);
        tri.lineTo(tip.x() - 6 * cosA - sz * perpCos,
                   tip.y() + 6 * sinA - sz * perpSin);
        tri.closeSubpath();
        p.drawPath(tri);
    }

    // -- Draw peak hold line (configurable overlay, independent of RX mode) ---
    if (peakHoldLineVisible()) {
        float frac = rxFraction(m_peakHoldDbm);
        if (m_peakHoldDbm <= m_levelDbm + 0.01f) {
            frac = m_needleFraction;
        } else {
            frac = qMax(frac, m_needleFraction);
        }
        const float angle = fractionToAngle(frac);

        const float cosA = std::cos(angle);
        const float sinA = std::sin(angle);
        const QPointF inner(cx + (radius - 4) * cosA,
                            cy - (radius - 4) * sinA);
        const QPointF outer(cx + (radius + 10) * cosA,
                            cy - (radius + 10) * sinA);

        // NereusSDR: in the copy's peak hold colour.
        p.setPen(QPen(m_peakHoldColor.isValid() ? m_peakHoldColor : kClassicPeakHold, 2));
        p.drawLine(inner, outer);
    }

    // -- Text readout -- all top-aligned on the same baseline -----------------
    QFont srcFont = font();
    srcFont.setPixelSize(WorkspacePreview::readableTextPixelSize(qMax(9, h / 14)));
    const QFontMetrics sfm(srcFont);
    const int topY = sfm.height() + 2;

    QFont valFont = font();
    valFont.setPixelSize(WorkspacePreview::readableTextPixelSize(qMax(13, h / 8)));
    valFont.setBold(true);
    const QFontMetrics vfm(valFont);

    if (m_transmitting) {
        // TX mode: show TX source label (center), mode name (left), value (right)
        // NereusSDR: txModeToLabel(), which names the appended modes too.
        const QString srcLabel = txModeToLabel(m_txMode);
        p.setFont(srcFont);
        p.setPen(QColor(0x80, 0x90, 0xa0));
        p.drawText((w - sfm.horizontalAdvance(srcLabel)) / 2, topY, srcLabel);

        p.setFont(valFont);
        // Left: mode name in cyan
        p.setPen(QColor(0x00, 0xb4, 0xd8));
        p.drawText(6, topY, "TX");

        // Right: formatted value
        const QString valText = txReadout(false);
        p.setPen(QColor(0xc8, 0xd8, 0xe8));
        p.drawText(w - vfm.horizontalAdvance(valText) - 6, topY, valText);
    } else {
        // RX mode: show source label (center), S-units (left), dBm (right)
        p.setFont(srcFont);
        p.setPen(QColor(0x80, 0x90, 0xa0));
        p.drawText((w - sfm.horizontalAdvance(m_source)) / 2, topY, m_source);

        p.setFont(valFont);
        p.setPen(QColor(0x00, 0xb4, 0xd8));
        // Show S-units based on the displayed value
        const QString sText = rxSUnitsReadout();
        p.drawText(6, topY, sText);

        const QString dbmText = rxDbmReadout();
        p.setPen(QColor(0xc8, 0xd8, 0xe8));
        p.drawText(w - vfm.horizontalAdvance(dbmText) - 6, topY, dbmText);
    }
}

// --- Vintage face ------------------------------------------------------------
// NereusSDR-native; no upstream equivalent.  Artwork by gui/VintageMeterFace
// (design carried over from Lee's TubeMeter project).  Unlike the classic face,
// which prints the RX scale outside the arc and the TX scale inside it, the
// vintage card carries one scale at a time: the S scale on receive, the
// selected TX scale on transmit.  Scale ranges and red-line points are the
// same ones the classic face uses (dbmToFraction / txValueToFraction).

void SMeterWidget::paintVintage(QPainter& p)
{
    // NereusSDR: the copy's dark mode and power scale colour.
    VintageFaceTheme theme = shownTheme(m_faceStyle, m_darkMode);
    if (m_powerScaleColor.isValid()) {
        theme.green = m_powerScaleColor.rgb();
    }
    // NereusSDR-native: the plate keeps the Main Panel dial's proportions.
    const QRectF bounds = plateRect();

    // -- Static artwork, cached -----------------------------------------------
    const qreal dpr = devicePixelRatioF();
    const QString scaleKey = m_transmitting
        ? QString("tx%1:%2:%3").arg(static_cast<int>(m_txMode))
                               .arg(m_powerScaleMax).arg(m_powerRedStart)
        : QString("rx%1").arg(static_cast<int>(m_rxMode));
    // NereusSDR-native: no surround.  Outside the bezel's rounded outline
    // the face leaves the widget clear, so the container's own background
    // (the user's colour when set) shows at the corners and beside a
    // centred plate.
    const QColor surround(Qt::transparent);
    const QString key = QString("%1x%2@%3|%4|%5|%6|%7|%8|%9")
        .arg(width()).arg(height()).arg(dpr)
        .arg(static_cast<int>(m_faceStyle)).arg(scaleKey).arg(bounds.width())
        .arg(WorkspacePreview::enabled())
        .arg(m_darkMode)
        .arg(m_powerScaleColor.isValid() ? m_powerScaleColor.name() : QString());

    if (key != m_faceCacheKey || m_faceCache.isNull()) {
        VintageScale scale;
        QString title;
        QString legend;
        auto tick = [&scale](float frac, bool major, const QString& label = QString()) {
            scale.ticks.append({frac, major, label});
        };
        auto redFrom = [&scale](float frac) {
            scale.bands.append({frac, 1.0f, VintageScaleBand::Color::Red});
        };

        if (!m_transmitting) {
            title = QStringLiteral("SIGNAL STRENGTH");
            switch (m_rxMode) {
            case RxMode::SMeter:        legend = QStringLiteral("S UNITS"); break;
            case RxMode::SignalAverage: legend = QStringLiteral("S UNITS  AVERAGE"); break;
            case RxMode::SMeterPeak:    legend = QStringLiteral("S UNITS  PEAK"); break;
            case RxMode::MaxBin:        legend = QStringLiteral("S UNITS  MAX BIN"); break;
            }
            // Every S-unit gets a tick, odd ones a numeral; every 10 dB over
            // S9 a tick, every 20 dB a numeral.  Red from S9 up; the band
            // starts a hair past the S9 tick so "9" itself stays in ink, as
            // on the classic face (S9 white, +20 / +40 red).
            for (int su = 0; su <= 9; ++su) {
                const bool odd = (su % 2) == 1;
                tick(dbmToFraction(S0_DBM + su * DB_PER_S), odd,
                     odd ? QString::number(su) : QString());
            }
            for (int over = ControlRanges::kSMeterOverS9StepDb;
                 over <= static_cast<int>(MAX_DBM - S9_DBM);
                 over += ControlRanges::kSMeterOverS9StepDb) {
                const bool major = (over % 20) == 0;
                tick(dbmToFraction(S9_DBM + over), major,
                     major ? QString("+%1").arg(over) : QString());
            }
            redFrom(dbmToFraction(S9_DBM) + 0.002f);
        } else {
            switch (m_txMode) {
            case TxMode::Power: {
                title  = QStringLiteral("FORWARD POWER");
                legend = QStringLiteral("WATTS");
                const int maxW = static_cast<int>(m_powerScaleMax);
                int tickStep, labelStep;
                if (maxW >= 2000)     { tickStep = 100; labelStep = 500; }
                else if (maxW >= 600) { tickStep = 50;  labelStep = 100; }
                else                  { tickStep = 10;  labelStep = 20;  }
                for (int pw = 0; pw <= maxW; pw += tickStep) {
                    const bool major = (pw % labelStep) == 0;
                    const QString label = (pw >= 1000)
                        ? QString("%1k").arg(pw / 1000.0f, 0, 'f', (pw % 1000) ? 1 : 0)
                        : QString::number(pw);
                    tick(static_cast<float>(pw) / m_powerScaleMax, major,
                         major ? label : QString());
                }
                // NereusSDR: a copy's power scale colour below the red
                // zone (the theme's green carries it).
                if (m_powerScaleColor.isValid()) {
                    scale.bands.append({0.0f, m_powerRedStart / m_powerScaleMax - 0.001f,
                                        VintageScaleBand::Color::Green});
                }
                redFrom(m_powerRedStart / m_powerScaleMax);
                break;
            }
            case TxMode::SWR:
                title  = QStringLiteral("STANDING WAVE RATIO");
                legend = QStringLiteral("S.W.R.");
                for (int tenths = 10; tenths <= 30; ++tenths) {
                    const bool major = (tenths % 5) == 0;
                    tick((tenths - 10) / 20.0f, major,
                         major ? QString::number(tenths / 10.0, 'f', (tenths % 10) ? 1 : 0)
                               : QString());
                }
                redFrom((2.5f - 1.0f) / 2.0f);
                break;
            case TxMode::Level:
                title  = QStringLiteral("MIC LEVEL");
                legend = QStringLiteral("DECIBELS");
                for (int db = -40; db <= 5; db += 5) {
                    const bool major = (db % 10) == 0;
                    tick((db + 40.0f) / 45.0f, major, major ? QString::number(db) : QString());
                }
                redFrom((0.0f + 40.0f) / 45.0f);
                break;
            case TxMode::Compression:
                title  = QStringLiteral("COMPRESSION");
                legend = QStringLiteral("DECIBELS");
                for (int db = -25; db <= 0; ++db) {
                    const bool major = (db % 5) == 0;
                    tick((db + 25.0f) / 25.0f, major, major ? QString::number(db) : QString());
                }
                break;
            // NereusSDR: the appended modes.  Titles from Thetis
            // MeterManager.cs:2272-2273, 2276, 2305 [v2.10.3.15] (ReadingName);
            // majors at the calibration keys, no red zone.
            case TxMode::Alc:
                title  = QStringLiteral("ALC COMPRESSION");
                legend = QStringLiteral("DECIBELS");
                for (int db = static_cast<int>(kAlcMinDb); db <= static_cast<int>(kAlcMaxDb); ++db) {
                    const bool major = (db % static_cast<int>(kAlcStepDb)) == 0;
                    tick(txValueToFraction(static_cast<float>(db)), major,
                         major ? QString::number(db) : QString());
                }
                break;
            case TxMode::AlcGroup:
                title  = QStringLiteral("ALC GROUP");
                legend = QStringLiteral("DECIBELS");
                for (int db = static_cast<int>(kAlcGroupMinDb); db <= static_cast<int>(kAlcGroupMaxDb); db += 5) {
                    const bool major = db == static_cast<int>(kAlcGroupMinDb)
                        || db == static_cast<int>(kAlcGroupMidDb) || db == static_cast<int>(kAlcGroupMaxDb);
                    tick(txValueToFraction(static_cast<float>(db)), major,
                         major ? QString::number(db) : QString());
                }
                break;
            case TxMode::Volts:
                title  = QStringLiteral("VOLTS");
                legend = QStringLiteral("VOLTS DC");
                for (int tenths = static_cast<int>(kVoltsMin * 10.0f); tenths <= static_cast<int>(kVoltsMax * 10.0f);
                     tenths += 5) {
                    const bool major = (tenths % 25) == 0;
                    tick(txValueToFraction(tenths / 10.0f), major,
                         major ? QString::number(tenths / 10.0, 'f', (tenths % 10) ? 1 : 0) : QString());
                }
                break;
            case TxMode::Amps:
                title  = QStringLiteral("AMPS");
                legend = QStringLiteral("AMPERES");
                for (int a = static_cast<int>(kAmpsMin); a <= static_cast<int>(kAmpsMax); a += static_cast<int>(kAmpsStep)) {
                    const bool major = (a % (2 * static_cast<int>(kAmpsStep))) == 0;
                    tick(txValueToFraction(static_cast<float>(a)), major,
                         major ? QString::number(a) : QString());
                }
                break;
            }
        }

        m_faceCache = QPixmap((QSizeF(size()) * dpr).toSize());
        m_faceCache.setDevicePixelRatio(dpr);
        // Clear outside the bezel (NereusSDR-native).
        m_faceCache.fill(Qt::transparent);
        QPainter fp(&m_faceCache);
        VintageMeterFace::paintStatic(fp, bounds, surround, theme,
                                      scale, title, legend);
        m_faceCacheKey = key;
    }
    p.drawPixmap(0, 0, m_faceCache);

    const VintageMeterFace::Geometry g = VintageMeterFace::geometryFor(bounds);
    const double u = g.unit;

    // -- Readouts flanking the hub: S-units (or TX) left, value right ---------
    {
        QString leftText;
        QString rightText;
        QColor leftColor(theme.ink);
        if (m_transmitting) {
            leftText  = QStringLiteral("TX");
            leftColor = QColor(theme.red);
            rightText = txReadout(true);
        } else {
            leftText  = rxSUnitsReadout();
            rightText = rxDbmReadout();
        }

        const double gap  = std::max(12.0, 44.0 * u);
        const double px   = std::max(11.0, g.face.height() * 0.115);
        const double boxH = px * 1.4;
        const double cy   = g.pivot.y() - px * 0.18;
        const double pad  = g.face.width() * 0.04;

        p.setFont(VintageMeterFace::faceFont(px, true));
        p.setPen(leftColor);
        p.drawText(QRectF(g.face.left() + pad, cy - boxH / 2.0,
                          g.pivot.x() - gap - g.face.left() - pad, boxH),
                   Qt::AlignRight | Qt::AlignVCenter, leftText);
        p.setFont(VintageMeterFace::faceFont(px * 0.86, false));
        p.setPen(QColor(theme.ink));
        p.drawText(QRectF(g.pivot.x() + gap, cy - boxH / 2.0,
                          g.face.right() - pad - g.pivot.x() - gap, boxH),
                   Qt::AlignLeft | Qt::AlignVCenter, rightText);
    }

    // -- Peak marker (RX Signal Peak mode): wedge riding outside the arc ------
    if (peakMarkerVisible()) {
        const float frac = rxFraction(m_peakDbm);
        const double a = qDegreesToRadians(VintageMeterFace::angleDeg(frac));
        const QPointF radial(std::sin(a), -std::cos(a));
        const QPointF across(std::cos(a), std::sin(a));
        const QPointF tipPt  = VintageMeterFace::pointAt(g, frac, g.radius + 1.0 * u);
        const QPointF basePt = tipPt + radial * std::max(5.0, 13.0 * u);
        const double half = std::max(2.5, 5.5 * u);
        QPainterPath wedge;
        wedge.moveTo(tipPt);
        wedge.lineTo(basePt + across * half);
        wedge.lineTo(basePt - across * half);
        wedge.closeSubpath();
        p.setPen(QPen(QColor(theme.ink), 0.6));
        p.setBrush(QColor(theme.yellow));
        p.drawPath(wedge);
    }

    // -- History wedge (same rules as the classic face) -----------------------
    if (m_showHistory) {
        const auto [lo, hi] = historyRange();
        QPolygonF wedge;
        wedge << g.pivot;
        const int steps = std::max(1, static_cast<int>(std::ceil((hi - lo) * 100.0f)));
        for (int i = 0; i <= steps; ++i) {
            wedge << VintageMeterFace::pointAt(g, lo + (hi - lo) * static_cast<float>(i) / steps, g.radius);
        }
        p.save();
        p.setClipRect(g.face);
        p.setPen(Qt::NoPen);
        p.setBrush(m_historyColor);
        p.drawPolygon(wedge);
        p.restore();
    }

    // -- Peak hold line (same rules as the classic face) ----------------------
    if (peakHoldLineVisible()) {
        float frac = rxFraction(m_peakHoldDbm);
        if (m_peakHoldDbm <= m_levelDbm + 0.01f) {
            frac = m_needleFraction;
        } else {
            frac = qMax(frac, m_needleFraction);
        }
        p.setPen(QPen(m_peakHoldColor.isValid() ? m_peakHoldColor : QColor(theme.red),
                      std::max(1.6, 3.0 * u), Qt::SolidLine, Qt::FlatCap));
        p.drawLine(VintageMeterFace::pointAt(g, frac, g.radius + 1.5 * u),
                   VintageMeterFace::pointAt(g, frac, g.radius + 15.0 * u));
    }

    // NereusSDR: the copy's needle colour and shadow.
    theme.pointer = shownNeedleColor(QColor(theme.pointer)).rgb();
    VintageMeterFace::paintPointer(p, g, theme, m_needleFraction, m_shadow);
    VintageMeterFace::paintHub(p, g, theme);
}

// --- Face style --------------------------------------------------------------
// NereusSDR-native; no upstream equivalent.

void SMeterWidget::setFaceStyle(FaceStyle style)
{
    m_faceStyle = style;
    updatePaintOpacity();
    m_faceCache = QPixmap();
    m_faceCacheKey.clear();
    if (m_settingsStore == SettingsStore::Global) {
        AppSettings::instance().setValue("SMeter_FaceStyle", faceStyleKey(style));
        // Added dials that follow Setup's face show it too.
        SMeterSetupFace::instance().notify(static_cast<int>(style));
    } else {
        // A face picked for this copy: it no longer follows Setup's.
        m_followsSetupFace = false;
        emit instanceSettingsChanged();
    }
    update();
    emit settingsChanged();
}

void SMeterWidget::setFollowsSetupFace(bool follow)
{
    if (m_settingsStore != SettingsStore::Instance) {
        return;
    }
    m_followsSetupFace = follow;
    if (follow) {
        m_faceStyle = static_cast<FaceStyle>(SMeterSetupFace::current());
        updatePaintOpacity();
        m_faceCache = QPixmap();
        m_faceCacheKey.clear();
        update();
    }
}

void SMeterWidget::updatePaintOpacity()
{
    setAttribute(Qt::WA_OpaquePaintEvent, m_faceStyle == FaceStyle::Classic);
}

QRectF SMeterWidget::plateRect() const
{
    const QRectF full = rect();
    const double maxWidth = std::round(full.height() * kPlateMaxAspect);
    if (full.width() <= maxWidth) {
        return full;
    }
    return QRectF(std::round((full.width() - maxWidth) / 2.0), full.top(), maxWidth, full.height());
}

QString SMeterWidget::faceStyleKey(FaceStyle style)
{
    switch (style) {
    case FaceStyle::AgedCream:    return "AgedCream";
    case FaceStyle::VuAmber:      return "VuAmber";
    case FaceStyle::CollinsWhite: return "CollinsWhite";
    case FaceStyle::Blackface:    return "Blackface";
    case FaceStyle::Carbon:       return "Carbon";
    case FaceStyle::Ice:          return "Ice";
    case FaceStyle::Classic:      return "Classic";
    }
    return "AgedCream";
}

SMeterWidget::FaceStyle SMeterWidget::faceStyleFromKey(const QString& key)
{
    for (int i = 0; i <= static_cast<int>(FaceStyle::Classic); ++i) {
        const FaceStyle style = static_cast<FaceStyle>(i);
        if (faceStyleKey(style) == key) {
            return style;
        }
    }
    return FaceStyle::AgedCream;
}

QString SMeterWidget::faceStyleLabel(FaceStyle style)
{
    if (style == FaceStyle::Classic) {
        return "Classic (flat)";
    }
    return QLatin1String(VintageMeterFace::theme(static_cast<int>(style)).name);
}

// --- Power scale -------------------------------------------------------------
// From AetherSDR src/gui/SMeterWidget.cpp:642-656 [@0cd4559]
// Upstream parameter is named hasAmplifier. NereusSDR plan uses hasAmplifier
// but documents it as pgxlConnected in the design table. The third branch
// (wScale > 500 -> generalized formula) does NOT exist in AetherSDR upstream:
// upstream treats ANY maxWatts > 100 as Aurora (600/500). The design table's
// "generalized formula" row is subsumed by the Aurora branch at all tested
// wScale values. Preserving upstream behavior verbatim per source-first protocol.
void SMeterWidget::setPowerScale(int maxWatts, bool hasAmplifier)
{
    if (hasAmplifier) {
        m_powerScaleMax = 2000.0f;
        m_powerRedStart = 1500.0f;
    } else if (maxWatts > 100) {
        m_powerScaleMax = 600.0f;
        m_powerRedStart = 500.0f;
    } else {
        m_powerScaleMax = 120.0f;
        m_powerRedStart = 100.0f;
    }
    updateNeedleTarget();
    update();
}

// --- Peak hold configuration -------------------------------------------------
void SMeterWidget::setPowerDisplayScale(double legalMarkerW, double scaleTopW)
{
    m_powerRedStart = static_cast<float>(legalMarkerW);
    m_powerScaleMax = static_cast<float>(scaleTopW);
    updateNeedleTarget();
    update();
}

// From AetherSDR src/gui/SMeterWidget.cpp:660-698 [@0cd4559]

void SMeterWidget::setPeakHoldEnabled(bool enabled)
{
    m_peakHoldEnabled = enabled;
    m_peakHoldDbm = m_levelDbm;
    m_peakHoldDecayStartDbm = m_levelDbm;
    m_peakHoldTimerRunning = false;

    // Persist peak hold enabled state.
    // Key PeakHoldEnabled ("True"/"False") per design doc ss5.4.2.
    if (m_settingsStore == SettingsStore::Global) {
        AppSettings::instance().setValue("PeakHoldEnabled",
                                         enabled ? QString("True") : QString("False"));
    } else {
        emit instanceSettingsChanged();
    }
    updateNeedleTarget();
    update();
    emit settingsChanged();
}

void SMeterWidget::setPeakHoldTimeMs(int ms)
{
    m_peakHoldTimeMs = qBound(100, ms, 2000);
}

void SMeterWidget::setPeakDecayRate(DecayRate rate)
{
    switch (rate) {
    case DecayRate::Fast:   m_peakDecayDbPerSec = 20.0f; break;
    case DecayRate::Medium: m_peakDecayDbPerSec = 10.0f; break;
    case DecayRate::Slow:   m_peakDecayDbPerSec = 5.0f;  break;
    }
}

void SMeterWidget::setPeakDecayRate(const QString& rate)
{
    if (rate == "Fast")        setPeakDecayRate(DecayRate::Fast);
    else if (rate == "Slow")   setPeakDecayRate(DecayRate::Slow);
    else                       setPeakDecayRate(DecayRate::Medium);

    // Persist decay rate selection.
    // Key PeakDecayRate ("Fast"/"Medium"/"Slow") per design doc ss5.4.2.
    // Normalize the stored string to the canonical set.
    const QString canonical = (rate == "Fast")  ? QString("Fast")
                            : (rate == "Slow")  ? QString("Slow")
                                                : QString("Medium");
    if (m_settingsStore == SettingsStore::Global) {
        AppSettings::instance().setValue("PeakDecayRate", canonical);
    } else {
        emit instanceSettingsChanged();
    }
    emit settingsChanged();
}

// The decay rate the menu shows checked.  NereusSDR-native.
SMeterWidget::DecayRate SMeterWidget::peakDecayRate() const
{
    if (qFuzzyCompare(m_peakDecayDbPerSec, 20.0f)) { return DecayRate::Fast; }
    if (qFuzzyCompare(m_peakDecayDbPerSec, 5.0f)) { return DecayRate::Slow; }
    return DecayRate::Medium;
}

void SMeterWidget::resetPeak()
{
    m_peakHoldDbm = m_levelDbm;
    m_peakHoldDecayStartDbm = m_levelDbm;
    m_peakHoldTimerRunning = false;
    updateNeedleTarget();
    update();
}

// --- Per-copy appearance and dynamics ----------------------------------------
// NereusSDR-native setters for the Thetis meter item settings a container
// copy keeps (SMeterDialContent).  None touches AppSettings.

void SMeterWidget::setPeakHoldColor(const QColor& color)
{
    m_peakHoldColor = color;
    update();
}

void SMeterWidget::setNeedleColor(const QColor& color)
{
    m_needleColor = color;
    update();
}

void SMeterWidget::setSubNeedleColor(const QColor& color)
{
    m_subNeedleColor = color;
    update();
}

void SMeterWidget::setShadow(bool on)
{
    m_shadow = on;
    update();
}

void SMeterWidget::setPowerScaleColor(const QColor& color)
{
    m_powerScaleColor = color;
    m_faceCache = QPixmap();
    m_faceCacheKey.clear();
    update();
}

void SMeterWidget::setDarkMode(bool on)
{
    m_darkMode = on;
    m_faceCache = QPixmap();
    m_faceCacheKey.clear();
    update();
}

int SMeterWidget::updateIntervalMs() const
{
    return m_needleAnimation.interval();
}

// The needle's update and the history's sampling run at the interval, as
// Thetis's Update does.
void SMeterWidget::setUpdateIntervalMs(int ms)
{
    const int interval = std::clamp(ms, kMinUpdateIntervalMs, kMaxUpdateIntervalMs);
    m_needleAnimation.setInterval(interval);
    m_historyTimer.setInterval(interval);
}

void SMeterWidget::setAttackRatio(std::optional<float> ratio)
{
    m_attackRatio = ratio ? std::optional<float>(std::clamp(*ratio, 0.0f, 1.0f)) : std::nullopt;
}

void SMeterWidget::setDecayRatio(std::optional<float> ratio)
{
    m_decayRatio = ratio ? std::optional<float>(std::clamp(*ratio, 0.0f, 1.0f)) : std::nullopt;
}

void SMeterWidget::setShowHistory(bool on)
{
    m_showHistory = on;
    if (on) {
        m_historyTimer.start();
    } else {
        m_historyTimer.stop();
        m_history.clear();
    }
    update();
}

void SMeterWidget::setHistoryColor(const QColor& color)
{
    m_historyColor = color.isValid() ? color : QColor::fromRgba(kDefaultHistoryRgba);
    update();
}

void SMeterWidget::setHistoryDurationMs(int ms)
{
    m_historyDurationMs = std::clamp(ms, kMinBarHistoryDurationMs, kMaxBarHistoryDurationMs);
}

void SMeterWidget::setIgnoreHistoryMs(int ms)
{
    m_ignoreHistoryMs = std::clamp(ms, kMinBarIgnoreHistoryMs, kMaxBarIgnoreHistoryMs);
}

// From Thetis MeterManager.cs:22018-22029 [v2.10.3.15] (clsNeedleItem.Update):
// one value per update, the list sized by the duration.  NereusSDR keeps
// the needle's position (Thetis keeps its value), so the wedge spans the
// scale as the needle moved over it.
void SMeterWidget::sampleHistory()
{
    if (ignoringHistory()) {
        return;
    }
    const std::pair<float, float> before = historyRange();
    // signal history
    m_history.append(m_needleFraction); // adds to end of the list
    // HistoryDuration is never less than UpdateInterval, so the list keeps
    // one value at least.
    // From Thetis MeterManager.cs:22100-22108 [v2.10.3.15] (HistoryDuration)
    const int keep = std::max(1, m_historyDurationMs / updateIntervalMs());
    const int numberToRemove = static_cast<int>(m_history.size()) - keep;
    // the list is sized based on delay
    if (numberToRemove > 0) {
        m_history.remove(0, numberToRemove); // remove the oldest, the head of the list
    }
    if (historyRange() != before) {
        update();
    }
}

// From Thetis MeterManager.cs:22140-22154 [v2.10.3.15] (History): the
// value itself while the list is empty.
std::pair<float, float> SMeterWidget::historyRange() const
{
    if (m_history.isEmpty()) {
        return {m_needleFraction, m_needleFraction};
    }
    const auto [lo, hi] = std::minmax_element(m_history.cbegin(), m_history.cend());
    return {*lo, *hi};
}

// Thetis counts the readings it ignores (IgnoreHistoryDuration /
// UpdateInterval); NereusSDR times the same span.
bool SMeterWidget::ignoringHistory() const
{
    return m_ignoreTimer.isValid()
        && m_ignoreTimer.elapsed() + m_testTimeOffsetMs - m_ignoreStartOffsetMs < m_ignoreHistoryMs;
}

// From Thetis MeterManager.cs:22131-22139 [v2.10.3.15] (clsNeedleItem.ClearHistory)
void SMeterWidget::clearHistory()
{
    m_history.clear();
    // ignore next N readings to make up 2 second of ignore
    m_ignoreTimer.start();
    m_ignoreStartOffsetMs = m_testTimeOffsetMs;
    // NereusSDR: Thetis's peak hold marker is the history's maximum
    // (renderNeedle, MeterManager.cs:40785-40801), so it clears with it.
    resetPeak();
}

QColor SMeterWidget::faceNeedleColor(FaceStyle face, bool darkMode)
{
    return face == FaceStyle::Classic ? kClassicNeedle : QColor(shownTheme(face, darkMode).pointer);
}

QColor SMeterWidget::facePeakHoldColor(FaceStyle face)
{
    return face == FaceStyle::Classic ? kClassicPeakHold : QColor(shownTheme(face, false).red);
}

QColor SMeterWidget::facePowerScaleColor(FaceStyle face, bool darkMode)
{
    return face == FaceStyle::Classic ? kClassicTxScale : QColor(shownTheme(face, darkMode).ink);
}

QColor SMeterWidget::shownNeedleColor(const QColor& faceColor) const
{
    const QColor& own = m_transmitting ? m_subNeedleColor : m_needleColor;
    return own.isValid() ? own : faceColor;
}

// --- Context menu ------------------------------------------------------------
// NereusSDR-native UX per design doc ss5.4.2. AetherSDR does not have a
// contextMenuEvent; it uses an inline settings strip in AppletPanel (removed
// by Task 40). The menu structure matches the design doc exactly:
//   TX Mode (exclusive) -> Power / SWR / Level / Compression
//   RX Mode (exclusive) -> Signal / Sig Avg / Signal Peak / Max Bin
//   Peak Hold -> Enabled (toggle) / Decay (Fast/Medium/Slow) / Reset

void SMeterWidget::contextMenuEvent(QContextMenuEvent* ev)
{
    QMenu* menu = buildContextMenu(this);
    // The menu's parent can die in exec() and take the menu with it; free it
    // only if it is still there (see ScopedChildWidget.h).
    const QPointer<QMenu> guard(menu);
    menu->exec(ev->globalPos());
    if (guard) {
        guard->deleteLater();
    }
}

QMenu* SMeterWidget::buildContextMenu(QObject* parent)
{
    auto* menu = new QMenu(qobject_cast<QWidget*>(parent));

    // ---- TX Mode submenu (exclusive action group) ----------------------------
    QMenu* txMenu = menu->addMenu("TX Mode");
    auto* txGroup = new QActionGroup(menu);
    txGroup->setExclusive(true);

    struct TxEntry { const char* label; TxMode mode; };
    const TxEntry txEntries[] = {
        {"Power",       TxMode::Power},
        {"SWR",         TxMode::SWR},
        {"Level",       TxMode::Level},
        {"Compression", TxMode::Compression},
    };
    // NereusSDR: a container copy (SettingsStore::Instance), whose owner
    // feeds every TX reading, also offers the appended modes.  Its menu is
    // sectioned by Thetis ANANMM's display groups (MeterManager.cs:
    // 24131-24135 [v2.10.3.15]: PWR/SWR, Comp, ALC, Volts/Amps), with Level
    // and Compression in the Comp section.  The Main Panel's dial offers
    // the four modes it is fed, unsectioned, as before.
    struct TxSection { QString label; TxMode mode; int group; };
    QVector<TxSection> txOrder;
    for (const auto& e : txEntries) {
        txOrder.append({QLatin1String(e.label), e.mode,
                        (e.mode == TxMode::Power || e.mode == TxMode::SWR) ? 1 : 2});
    }
    if (m_settingsStore == SettingsStore::Instance) {
        txOrder.append({txModeToLabel(TxMode::Alc), TxMode::Alc, 2});
        txOrder.append({txModeToLabel(TxMode::AlcGroup), TxMode::AlcGroup, 3});
        txOrder.append({txModeToLabel(TxMode::Volts), TxMode::Volts, 4});
        txOrder.append({txModeToLabel(TxMode::Amps), TxMode::Amps, 4});
    }
    int lastGroup = 0;
    for (const TxSection& e : txOrder) {
        if (m_settingsStore == SettingsStore::Instance && lastGroup != 0 && e.group != lastGroup) {
            txMenu->addSeparator();
        }
        lastGroup = e.group;
        auto* a = txMenu->addAction(e.label);
        a->setCheckable(true);
        a->setChecked(m_txMode == e.mode);
        txGroup->addAction(a);
        const TxMode capturedMode = e.mode;
        connect(a, &QAction::triggered, this, [this, capturedMode]() {
            setTxMode(txModeToLabel(capturedMode));
        });
    }

    // ---- RX Mode submenu (exclusive action group) ----------------------------
    // Labels per design doc ss5.4.2 (Signal / Sig Avg / Signal Peak / Max Bin).
    QMenu* rxMenu = menu->addMenu("RX Mode");
    auto* rxGroup = new QActionGroup(menu);
    rxGroup->setExclusive(true);

    struct RxEntry { const char* label; RxMode mode; };
    const RxEntry rxEntries[] = {
        {"Signal",       RxMode::SMeter},
        {"Sig Avg",      RxMode::SignalAverage},
        {"Signal Peak",  RxMode::SMeterPeak},
        {"Max Bin",      RxMode::MaxBin},
    };
    for (const auto& e : rxEntries) {
        auto* a = rxMenu->addAction(QLatin1String(e.label));
        a->setCheckable(true);
        a->setChecked(m_rxMode == e.mode);
        rxGroup->addAction(a);
        const RxMode capturedMode = e.mode;
        connect(a, &QAction::triggered, this, [this, capturedMode]() {
            setRxMode(rxModeToLabel(capturedMode));
        });
    }

    // ---- Peak Hold submenu ---------------------------------------------------
    QMenu* peakMenu = menu->addMenu("Peak Hold");

    // Enabled toggle
    auto* enabledA = peakMenu->addAction("Enabled");
    enabledA->setCheckable(true);
    enabledA->setChecked(m_peakHoldEnabled);
    connect(enabledA, &QAction::triggered, this,
            [this](bool checked) { setPeakHoldEnabled(checked); });

    // Decay sub-submenu (exclusive action group)
    QMenu* decayMenu = peakMenu->addMenu("Decay");
    auto* decayGroup = new QActionGroup(menu);
    decayGroup->setExclusive(true);

    struct DecayEntry { const char* label; DecayRate rate; float dbPerSec; };
    const DecayEntry decayEntries[] = {
        {"Fast",   DecayRate::Fast,   20.0f},
        {"Medium", DecayRate::Medium, 10.0f},
        {"Slow",   DecayRate::Slow,    5.0f},
    };
    for (const auto& e : decayEntries) {
        const QString labelWithRate = QString("%1 (%2 dB/s)")
            .arg(QLatin1String(e.label))
            .arg(static_cast<int>(e.dbPerSec));
        auto* a = decayMenu->addAction(labelWithRate);
        a->setCheckable(true);
        a->setChecked(qFuzzyCompare(m_peakDecayDbPerSec, e.dbPerSec));
        decayGroup->addAction(a);
        const QString rateName(QLatin1String(e.label));
        connect(a, &QAction::triggered, this, [this, rateName]() {
            setPeakDecayRate(rateName);
        });
    }

    peakMenu->addSeparator();

    // Reset transient action
    auto* resetA = peakMenu->addAction("Reset");
    connect(resetA, &QAction::triggered, this, &SMeterWidget::resetPeak);

    // ---- Meter Face submenu (exclusive action group) -------------------------
    // Appended last so the TX Mode / RX Mode / Peak Hold indices stay put.
    QMenu* faceMenu = menu->addMenu("Meter Face");
    auto* faceGroup = new QActionGroup(menu);
    faceGroup->setExclusive(true);
    for (int i = 0; i <= static_cast<int>(FaceStyle::Classic); ++i) {
        const FaceStyle style = static_cast<FaceStyle>(i);
        if (style == FaceStyle::Classic) {
            faceMenu->addSeparator();
        }
        auto* a = faceMenu->addAction(faceStyleLabel(style));
        a->setCheckable(true);
        a->setChecked(m_faceStyle == style);
        faceGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, style]() {
            setFaceStyle(style);
        });
    }

    return menu;
}

// Convert TxMode enum to the label string accepted by setTxMode().
// NereusSDR-native; no upstream equivalent.
QString SMeterWidget::txModeToLabel(TxMode mode)
{
    switch (mode) {
    case TxMode::Power:       return "Power";
    case TxMode::SWR:         return "SWR";
    case TxMode::Level:       return "Level";
    case TxMode::Compression: return "Compression";
    // Thetis MeterManager.cs:2272-2273, 2276, 2305 [v2.10.3.15] (ReadingName)
    case TxMode::Alc:         return "ALC Compression";
    case TxMode::AlcGroup:    return "ALC Group";
    case TxMode::Volts:       return "Volts";
    case TxMode::Amps:        return "Amps";
    }
    return "Power";
}

// Convert RxMode enum to the canonical label string accepted by setRxMode().
// Uses the context-menu display strings (Signal / Sig Avg / Signal Peak / Max Bin)
// per design doc ss5.4.2.
// NereusSDR-native; no upstream equivalent.
QString SMeterWidget::rxModeToLabel(RxMode mode)
{
    switch (mode) {
    case RxMode::SMeter:         return "Signal";
    case RxMode::SignalAverage:  return "Sig Avg";
    case RxMode::SMeterPeak:     return "Signal Peak";
    case RxMode::MaxBin:         return "Max Bin";
    }
    return "Signal";
}

} // namespace NereusSDR
