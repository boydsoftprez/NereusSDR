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
// Modification history (NereusSDR):
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
// =================================================================
#include "SMeterWidget.h"

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QSet>
#include <QtMath>
#include <QFontMetrics>

#include "core/AppSettings.h"

namespace NereusSDR {

// --- Construction ------------------------------------------------------------
// From AetherSDR src/gui/SMeterWidget.cpp:13-47 [@0cd4559]

SMeterWidget::SMeterWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(minimumSizeHint());
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_needleFraction = dbmToFraction(m_levelDbm);
    m_targetNeedleFraction = m_needleFraction;
    m_peakHoldDecayStartDbm = m_peakHoldDbm;

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
}

// --- Public interface --------------------------------------------------------
// From AetherSDR src/gui/SMeterWidget.cpp:51-143 [@0cd4559]

void SMeterWidget::setLevel(float dbm)
{
    // Sub-perceivable change guard.  MeterPoller pushes this 10 times a
    // second; radio noise causes the dBm reading to wiggle by a few tenths
    // even on a quiet band.  Below 0.5 dB the needle position is the same
    // pixel and the S-unit text doesn't change.  Skipping the animation
    // rebuild + repaint for those sub-threshold ticks was the largest
    // single contributor to the macOS paint-pipeline saturation observed
    // in the 2026-05-24 bench profile.
    if (std::abs(dbm - m_levelDbm) < 0.5f) {
        return;
    }
    m_levelDbm = dbm;

    // Peak hold (existing needle/triangle behavior)
    if (dbm > m_peakDbm) {
        m_peakDbm = dbm;
        m_peakDecay.start();
    }

    // Configurable peak hold line
    if (m_peakHoldEnabled) {
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

void SMeterWidget::setTransmitting(bool tx)
{
    m_transmitting = tx;
    if (!tx) {
        // Clear TX values immediately on un-key so the RX reading becomes the
        // animation target as soon as transmit ends.
        m_txPower = 0.0f;
        m_txSwr   = 1.0f;
    }
    updateNeedleTarget();
    update();
}

void SMeterWidget::setTxMode(const QString& mode)
{
    if (mode == "Power")            m_txMode = TxMode::Power;
    else if (mode == "SWR")         m_txMode = TxMode::SWR;
    else if (mode == "Level")       m_txMode = TxMode::Level;
    else if (mode == "Compression") m_txMode = TxMode::Compression;

    // Persist TX mode selection.
    // Key SMeter_TxSelect (int 0..3): Power=0, SWR=1, Level=2, Compression=3.
    // NereusSDR-native key per design doc ss5.4.2.
    AppSettings::instance().setValue("SMeter_TxSelect",
                                     static_cast<int>(m_txMode));
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
    AppSettings::instance().setValue("SMeter_RxSelect",
                                     static_cast<int>(m_rxMode));
    updateNeedleTarget();
    update();
}

// --- Needle target -----------------------------------------------------------
// From AetherSDR src/gui/SMeterWidget.cpp:146-178 [@0cd4559]

void SMeterWidget::updateNeedleTarget()
{
    updatePeakHoldValue();

    if (m_transmitting) {
        m_targetNeedleFraction = txValueToFraction(currentTxValue());
    } else if (m_rxMode == RxMode::SMeterPeak) {
        m_targetNeedleFraction = dbmToFraction(m_peakDbm);
    } else {
        // SMeter, SignalAverage, and MaxBin all use m_levelDbm as target;
        // the *source* of that value differs (caller feeds the right WDSP meter).
        m_targetNeedleFraction = dbmToFraction(m_levelDbm);
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

    const float delta = m_targetNeedleFraction - m_needleFraction;
    const float elapsedSeconds = static_cast<float>(elapsedMs) / 1000.0f;
    const float timeConstant = (delta >= 0.0f) ? kNeedleAttackTimeSeconds
                                                : kNeedleReleaseTimeSeconds;
    const float alpha = 1.0f - std::exp(-elapsedSeconds / timeConstant);
    const bool needleAtTarget = qAbs(delta) <= kNeedleSnapEpsilon;
    if (needleAtTarget) {
        m_needleFraction = m_targetNeedleFraction;
    } else {
        m_needleFraction += delta * alpha;
    }

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
    if (m_levelDbm <= S0_DBM) return "S0";
    if (m_levelDbm <= S9_DBM) {
        const int s = qRound((m_levelDbm - S0_DBM) / DB_PER_S);
        return QString("S%1").arg(qBound(0, s, 9));
    }
    const int over = qRound(m_levelDbm - S9_DBM);
    return QString("S9+%1").arg(over);
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
    }
    return 0.0f;
}

// From AetherSDR src/gui/SMeterWidget.cpp:278-287 [@0cd4559]
float SMeterWidget::currentTxValue() const
{
    switch (m_txMode) {
    case TxMode::Power:       return m_txPower;
    case TxMode::SWR:         return m_txSwr;
    case TxMode::Level:       return m_micLevel;
    case TxMode::Compression: return m_compLevel;
    }
    return 0.0f;
}

// --- Paint -------------------------------------------------------------------
// From AetherSDR src/gui/SMeterWidget.cpp:291-640 [@0cd4559]

void SMeterWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int w = width();
    const int h = height();

    // Background
    p.fillRect(rect(), QColor(0x0f, 0x0f, 0x1a));

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
        }

        if (redFrac < 0.0f) {
            // Entire arc is blue
            p.setPen(QPen(blueColor, 3));
            p.drawArc(innerArc,
                      static_cast<int>(ARC_START_DEG * 16),
                      static_cast<int>((ARC_END_DEG - ARC_START_DEG) * 16));
        } else {
            const float splitDeg = qRadiansToDegrees(fractionToAngle(redFrac));
            p.setPen(QPen(blueColor, 3));
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
    tickFont.setPixelSize(qMax(10, h / 10));
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
            const QColor& tc = (pw >= redW) ? redColor : blueColor;
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
    }

    // -- Draw needle ----------------------------------------------------------
    // Needle originates from needleCy (just below widget) rather than the
    // arc center, so the pivot is barely out of frame.
    // When transmitting, needle tracks the selected TX meter instead of RX.
    {
        const float angle = fractionToAngle(m_needleFraction);

        // Needle extends to the end of the outer (RX) ticks: radius + 14
        const float tipR = radius + 14;
        const float tipX = cx + tipR * std::cos(angle);
        const float tipY = cy - tipR * std::sin(angle);

        // Needle shadow
        p.setPen(QPen(QColor(0, 0, 0, 80), 3));
        p.drawLine(QPointF(cx + 1, needleCy + 1), QPointF(tipX + 1, tipY + 1));

        // Needle
        p.setPen(QPen(QColor(0xff, 0xff, 0xff), 2));
        p.drawLine(QPointF(cx, needleCy), QPointF(tipX, tipY));
    }

    // Draw peak marker (small triangle) - only in RX S-Meter Peak mode
    if (!m_transmitting && m_rxMode == RxMode::SMeterPeak
        && m_peakDbm > m_levelDbm + 1.0f) {
        const float frac = dbmToFraction(m_peakDbm);
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
    if (m_peakHoldEnabled && !m_transmitting
        && m_peakHoldDbm > S0_DBM + 1.0f) {
        float frac = dbmToFraction(m_peakHoldDbm);
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

        p.setPen(QPen(QColor(0xff, 0x44, 0x44, 0xcc), 2));
        p.drawLine(inner, outer);
    }

    // -- Text readout -- all top-aligned on the same baseline -----------------
    QFont srcFont = font();
    srcFont.setPixelSize(qMax(9, h / 14));
    const QFontMetrics sfm(srcFont);
    const int topY = sfm.height() + 2;

    QFont valFont = font();
    valFont.setPixelSize(qMax(13, h / 8));
    valFont.setBold(true);
    const QFontMetrics vfm(valFont);

    if (m_transmitting) {
        // TX mode: show TX source label (center), mode name (left), value (right)
        static const char* txLabels[] = {"Power", "SWR", "Level", "Compression"};
        const QString srcLabel = txLabels[static_cast<int>(m_txMode)];
        p.setFont(srcFont);
        p.setPen(QColor(0x80, 0x90, 0xa0));
        p.drawText((w - sfm.horizontalAdvance(srcLabel)) / 2, topY, srcLabel);

        p.setFont(valFont);
        // Left: mode name in cyan
        p.setPen(QColor(0x00, 0xb4, 0xd8));
        p.drawText(6, topY, "TX");

        // Right: formatted value
        QString valText;
        switch (m_txMode) {
        case TxMode::Power:       valText = QString("%1 W").arg(m_txPower, 0, 'f', 0); break;
        case TxMode::SWR:         valText = QString("%1").arg(m_txSwr, 0, 'f', 1); break;
        case TxMode::Level:       valText = QString("%1 dB").arg(m_micLevel, 0, 'f', 0); break;
        case TxMode::Compression: valText = QString("%1 dB").arg(m_compLevel, 0, 'f', 0); break;
        }
        p.setPen(QColor(0xc8, 0xd8, 0xe8));
        p.drawText(w - vfm.horizontalAdvance(valText) - 6, topY, valText);
    } else {
        // RX mode: show source label (center), S-units (left), dBm (right)
        p.setFont(srcFont);
        p.setPen(QColor(0x80, 0x90, 0xa0));
        p.drawText((w - sfm.horizontalAdvance(m_source)) / 2, topY, m_source);

        const float displayDbm = (m_rxMode == RxMode::SMeterPeak) ? m_peakDbm : m_levelDbm;

        p.setFont(valFont);
        p.setPen(QColor(0x00, 0xb4, 0xd8));
        // Show S-units based on the displayed value
        QString sText;
        if (displayDbm <= S0_DBM) {
            sText = "S0";
        } else if (displayDbm <= S9_DBM) {
            sText = QString("S%1").arg(qBound(0, qRound((displayDbm - S0_DBM) / DB_PER_S), 9));
        } else {
            sText = QString("S9+%1").arg(qRound(displayDbm - S9_DBM));
        }
        p.drawText(6, topY, sText);

        const QString dbmText = QString("%1 dBm").arg(displayDbm, 0, 'f', 0);
        p.setPen(QColor(0xc8, 0xd8, 0xe8));
        p.drawText(w - vfm.horizontalAdvance(dbmText) - 6, topY, dbmText);
    }
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
// From AetherSDR src/gui/SMeterWidget.cpp:660-698 [@0cd4559]

void SMeterWidget::setPeakHoldEnabled(bool enabled)
{
    m_peakHoldEnabled = enabled;
    m_peakHoldDbm = m_levelDbm;
    m_peakHoldDecayStartDbm = m_levelDbm;
    m_peakHoldTimerRunning = false;

    // Persist peak hold enabled state.
    // Key PeakHoldEnabled ("True"/"False") per design doc ss5.4.2.
    AppSettings::instance().setValue("PeakHoldEnabled",
                                     enabled ? QString("True") : QString("False"));
    updateNeedleTarget();
    update();
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
    AppSettings::instance().setValue("PeakDecayRate", canonical);
}

void SMeterWidget::resetPeak()
{
    m_peakHoldDbm = m_levelDbm;
    m_peakHoldDecayStartDbm = m_levelDbm;
    m_peakHoldTimerRunning = false;
    updateNeedleTarget();
    update();
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
    menu->exec(ev->globalPos());
    menu->deleteLater();
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
    for (const auto& e : txEntries) {
        auto* a = txMenu->addAction(QLatin1String(e.label));
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
