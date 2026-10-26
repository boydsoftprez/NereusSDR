#pragma once

// =================================================================
// src/gui/meters/MeterItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/common.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.designer.cs (update interval range
//     and default): upstream source has no top-of-file GPL header, the
//     setup.cs header below covers its partial class
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - setUpdateIntervalMs() is virtual: the LED raises an
//                 interval under 50 ms to 100 ms. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - readingBindingsBesideMmio(): the readings an item still
//                 takes while data source variable 1 feeds it (a HISTORY
//                 item's axis 1). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - acceptsUpdate() is virtual: the HISTORY item keeps the
//                 update interval per axis. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-26 - Magic eye and bezel size range (kMinMeterEyeScale,
//                 kMaxMeterEyeScale, kDefaultMeterEyeScale). J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Several data source (MMIO) variables per item: index 0 is
//                 the existing pair, index 1 and up feed setSecondaryValue;
//                 mmioVariableCount() ports Thetis MeterVariables.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Fade on RX / Fade on TX on every item: setTransmitting,
//                 fade flags, fadeValue() and advanceFade() (Thetis fade(),
//                 via ContentFade::nextValue), setForeignSession; a virtual
//                 clearHistory() (Thetis clsMeterItem.ClearHistory).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - BAR building block: Segments and Solid styles, a peak hold
//                 marker separate from the live marker, peak value colour,
//                 ignore history, power limit, marker stroke width and the
//                 clsBarItem new-item defaults (kBarItemThetisDefaults).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - NEEDLE building block: shadow and peak hold switches, a
//                 peak hold colour, the history arc with ignore history
//                 after clearHistory(), the power limit (normalise to
//                 100 W) and the clsNeedleItem new-item defaults
//                 (kNeedleThetisDefaults). Saved needles keep their look.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - VHF S9 rule per slice: setAboveS9Frequency() /
//                 aboveS9Frequency(), a formatValue overload taking the
//                 flag (S9 = -93 dBm at or above 30 MHz, kS9DbmBelowS9Frequency
//                 / kS9DbmAboveS9Frequency from common.cs); BarItem, TextItem
//                 and NeedleItem S-unit text and calibration use it.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - Per-item update interval: setUpdateIntervalMs(),
//                 updateIntervalMs() (0 = every poll), acceptsUpdate();
//                 new items take Thetis's 100 ms. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-25 - New items take Thetis defaults: applyNewItemDefaults()
//                 hook (no-op in the base), SolidColourItem override. Saved
//                 items keep their look. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - TEXT, SCALE and IMAGE building blocks: TextItem font
//                 family, italic, centring and the %value%, %valueWithType%
//                 and %group% tokens; ImageItem dark mode and the -dark,
//                 -small and -large variant lookup (variantPath); the
//                 clsText, clsScaleItem and clsImage new-item defaults
//                 (kTextThetisDefaults, kScaleThetisDefaults,
//                 kImageThetisDefaults). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-25 - publishedReadings(): named values an item publishes for
//                 data out and text overlay tokens (empty in the base).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-12 - Shared accessory display readings and independent legal
//                 marker / display scale by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-04 — Selected RX source identity and RX-only presentation reset by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Composite reading/replay/cadence contracts by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
//   2026-09-23: R-R3-13: isNoMeterReading() shared no-reading predicate
//                 (-400 dBm sentinel or non-finite); TextItem, BarItem and
//                 NeedleItem show "--" and rest at the scale minimum, with
//                 read-only text accessors for tests. J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
//   2026-09-23: R-R3-13 fix wave: no reading applies only to
//                 receive-signal bindings (isReceiveSignalBinding); a TX
//                 meter keeps WDSP's -400 zero-power floor as a number.
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-09-25 - R-R3-32 (remote-window parity Task 6):
//                 isHardwareTelemetryBinding, isNoReadingBinding. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

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

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
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
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

// --- From setup.cs ---
//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
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
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

// --- From common.cs ---
//=================================================================
// common.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2012  FlexRadio Systems
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
// You may contact us via email at: gpl@flexradio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    4616 W. Howard Lane  Suite 1-150
//    Austin, TX 78728
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

#include <QObject>
#include <QSet>
#include <QHash>
#include <QVector>
#include <QRect>
#include <QColor>
#include <QImage>
#include <QMap>
#include <QPointF>
#include <QString>
#include <QUuid>
#include <QVariant>
#include <cmath>
#include <limits>
#include <optional>

class QPainter;
class QMouseEvent;
class QWheelEvent;

namespace NereusSDR {

// Free function — maps a MeterBinding::* constant to the centered title
// string ScaleItem renders when setShowType(true). Ported verbatim from
// Thetis MeterManager.cs:2258-2318 ReadingName(). Returns an empty
// QString for unmapped binding IDs (ScaleItem skips empty titles).
QString readingName(int bindingId);

// No reading (R-R3-13, NereusSDR-native).  The same rule the analog
// SMeterWidget and the slice flag VfoLevelBar use: a value at or below the
// -400 dBm sentinel, or a non-finite one, is not a signal.  Every numeric
// meter item consults this before formatting or plotting a value, so a
// poller can feed the sentinel when there is nothing to show and the item
// shows "--" instead of a floor number or a frozen last value.
inline constexpr double kNoMeterReadingDbm = -400.0;
inline bool isNoMeterReading(double dbm)
{
    return !std::isfinite(dbm) || dbm <= kNoMeterReadingDbm;
}

// Receive-signal bindings (SignalPeak..AgcAvg and SignalMaxBin): exactly the
// bindings MeterPoller feeds the sentinel to (local poll() and remote
// pollRemoteRxMeters()). Existing primitive transmit bindings retain their
// raw -400 semantics when available. A transmit
// meter at true zero reads exactly -400 from WDSP (meter.c floors with
// 10 * log10(x + 1.0e-40)), which is a real reading and keeps its number.
bool isReceiveSignalBinding(int bindingId);

// R-R3-32 (remote-window parity Task 6): the hardware telemetry bindings
// (HwVolts, HwAmps, HwTemperature), which MeterPoller feeds the sentinel to
// while RadioModel::paReadings() has no such reading. No volts, amps or
// temperature reading can be at or below -400.
bool isHardwareTelemetryBinding(int bindingId);
// Added stage-peak sources are clamped above -400 by the existing model
// transform, so the sentinel can safely represent absence on those GUI IDs.
bool isSourceClampedPeakBinding(int bindingId);
inline bool isNoReadingBinding(int bindingId)
{
    return isReceiveSignalBinding(bindingId) || isHardwareTelemetryBinding(bindingId)
        || isSourceClampedPeakBinding(bindingId);
}

// Update interval (ms). Thetis sets one per item group
// (igs.UpdateInterval = (int)nudMeterItemUpdateRate.Value, Thetis
// setup.cs:25526 [v2.10.3.15]); NereusSDR keeps one per item.
// From Thetis setup.designer.cs:55695-55699 [v2.10.3.15] (nudMeterItemUpdateRate.Value)
constexpr int kDefaultUpdateIntervalMs = 100;
// From Thetis setup.designer.cs:55685-55689 [v2.10.3.15] (nudMeterItemUpdateRate.Minimum)
constexpr int kMinUpdateIntervalMs = 15;
// From Thetis setup.designer.cs:55680-55684 [v2.10.3.15] (nudMeterItemUpdateRate.Maximum)
constexpr int kMaxUpdateIntervalMs = 5000;

class MeterItem : public QObject {
    Q_OBJECT

public:
    enum class Layer {
        Background,      // Pipeline 1
        Geometry,        // Pipeline 2
        OverlayStatic,   // Pipeline 3 (cached)
        OverlayDynamic   // Pipeline 3 (per-update)
    };

    // Unit mode for signal-level readouts. Corresponds to Thetis
    // radSReading / radDBM / radUV radio buttons (console.cs / display.cs).
    // Fan-out controlled by MultimeterPage → MultimeterUnitMode AppSettings key.
    enum class MeterUnit { S, dBm, uV };

    // Format a signal-level dBm value in the requested unit.
    // S: IARU S-meter scale — S9 = -73 dBm at HF, 6 dB per S unit.
    // dBm: plain numeric string (with or without decimal).
    // uV: microvolts at 50Ω derived from dBm.
    // Always a number: whether a value is no reading is the item's call
    // (isNoReading(), from its binding); noReadingText() is what it shows.
    static QString formatValue(float dBm, MeterUnit unit, bool decimal = true);
    // The same, with S9 at -93 dBm when the item's slice is at or above the
    // S9 frequency (AboveS9Frequency.h). dBm and uV are unaffected.
    static QString formatValue(float dBm, MeterUnit unit, bool decimal, bool aboveS9);

    // S9 level of the S-unit scale below and at or above the S9 frequency.
    // From Thetis common.cs:881-887 [v2.10.3.15] (GetSMeterUnits)
    static constexpr float kS9DbmBelowS9Frequency = -73.0f;
    // From Thetis common.cs:881-887 [v2.10.3.15] (GetSMeterUnits: //MW0LGE_[2.9.0.7] fixed to 93)
    static constexpr float kS9DbmAboveS9Frequency = -93.0f;
    static constexpr float s9Dbm(bool aboveS9)
    {
        return aboveS9 ? kS9DbmAboveS9Frequency : kS9DbmBelowS9Frequency;
    }

    // Whether the item's slice is at or above the S9 frequency
    // (isAboveS9Frequency, AboveS9Frequency.h). MainWindow sets it on every
    // item from the slice each entry resolves to. Runtime only, not saved.
    virtual void setAboveS9Frequency(bool above) { m_aboveS9Frequency = above; }
    bool aboveS9Frequency() const               { return m_aboveS9Frequency; }
    // No reading in the requested unit: "--" for dBm and S, "-- µV" for uV.
    static QString noReadingText(MeterUnit unit);

    virtual void setUnitMode(MeterUnit u)   { m_unitMode = u; }
    MeterUnit unitMode() const              { return m_unitMode; }
    void setShowDecimal(bool d)             { m_showDecimal = d; }
    bool showDecimal() const                { return m_showDecimal; }

    explicit MeterItem(QObject* parent = nullptr) : QObject(parent) {}
    ~MeterItem() override = default;

    // Position (normalized 0-1)
    float x() const { return m_x; }
    float y() const { return m_y; }
    float itemWidth() const { return m_w; }
    float itemHeight() const { return m_h; }
    void setRect(float x, float y, float w, float h) {
        m_x = x; m_y = y; m_w = w; m_h = h;
    }

    int bindingId() const { return m_bindingId; }
    void setBindingId(int id) { m_bindingId = id; }
    double value() const { return m_value; }
    virtual void setValue(double v) { m_value = v; }
    virtual QSet<int> readingBindings() const { return m_bindingId >= 0 ? QSet<int>{m_bindingId} : QSet<int>{}; }
    virtual void pushBindingValue(int binding, double value) { if (binding == m_bindingId) { setValue(value); } }
    // The readings an item still takes from the radio while data source
    // variable 1 (index 0) feeds its main reading: none by default. Thetis
    // reads each HISTORY axis on its own (MeterManager.cs:18096-18165
    // [v2.10.3.15]), so a HISTORY item's axis 1 keeps its reading.
    virtual QSet<int> readingBindingsBesideMmio() const { return {}; }
    virtual bool takeStaticPresentationChange() { return false; }
    virtual bool advanceMeter(qint64 monotonicMs) { Q_UNUSED(monotonicMs); return false; }
    virtual void resetForTxTransition(bool inTx);
    // GUI source handoff: RX presentation only, preserving TX/PA/MMIO state.
    virtual void resetRxSource();
    virtual void setPowerScale(int watts);
    virtual void setPowerScale(int limitWatts, double topWatts);
    // Composites can render individual unavailable channels while keeping
    // useful siblings visible. Empty reason restores availability.
    virtual void setBindingUnavailable(int binding, const QString& reason) { m_unavailableBindings[binding] = reason; }
    QString bindingUnavailableReason(int binding) const { return m_unavailableBindings.value(binding); }
    // Provider support is runtime state, independent of a missing sample.
    enum class BindingSupport { Unknown, Supported, Unsupported };
    Q_ENUM(BindingSupport)
    virtual void setBindingSupport(int binding, BindingSupport support) { m_bindingSupport[binding] = support; }
    BindingSupport bindingSupport(int binding) const { return m_bindingSupport.value(binding, BindingSupport::Unknown); }
    // Explicit unavailability applies to any channel. Available legacy TX
    // primitives preserve true-zero -400; sentinel-capable bindings also
    // recognize the no-reading value without an availability reason.
    bool isNoReading(double v) const
    {
        return !bindingUnavailableReason(m_bindingId).isEmpty()
            || (!hasMmioBinding() && isNoReadingBinding(m_bindingId) && isNoMeterReading(v));
    }

    int zOrder() const { return m_zOrder; }
    void setZOrder(int z) { m_zOrder = z; }

    // Visibility filter (from Thetis clsMeterItem, MeterManager.cs:6741-6744 /
    // 6937-7096). The container's paint loop applies the rule from
    // MeterManager.cs:31366-31368 — see MeterWidget::shouldRender().
    // displayGroup == 0 means "always visible" (matches Thetis default).
    bool onlyWhenRx() const { return m_onlyWhenRx; }
    void setOnlyWhenRx(bool v) { m_onlyWhenRx = v; }
    bool onlyWhenTx() const { return m_onlyWhenTx; }
    void setOnlyWhenTx(bool v) { m_onlyWhenTx = v; }
    int  displayGroup() const { return m_displayGroup; }
    void setDisplayGroup(int g) { m_displayGroup = g; }

    // Fade on RX / Fade on TX: the animated dim of Thetis fade()
    // (MeterManager.cs:33153-33177 [v2.10.3.15], ContentFade::nextValue).
    // MeterWidget draws a veil of the container background at alpha
    // 255 - fadeValue() over the item; advanceFade() steps the value once
    // per fadeIntervalMs(). Runtime state (transmitting, value, foreign
    // session) is not saved; the two flags are saved in the entry context
    // (ContentFade::kFadeOnRxKey / kFadeOnTxKey) when set explicitly.
    // ButtonBoxItem and FadeCoverItem keep the flags in their records and
    // override the accessors.
    virtual void setTransmitting(bool tx) { m_transmitting = tx; }
    bool transmitting() const { return m_transmitting; }
    virtual void setFadeOnRx(bool on) { m_fadeOnRx = on; m_explicitFade = true; }
    virtual void setFadeOnTx(bool on) { m_fadeOnTx = on; m_explicitFade = true; }
    virtual bool fadeOnRx() const { return m_fadeOnRx; }
    virtual bool fadeOnTx() const { return m_fadeOnTx; }
    // True once a fade setter ran (the entry then saves both keys).
    virtual bool hasExplicitFade() const { return m_explicitFade; }
    // kDimmedLevel (48) .. 255; 255 is undimmed.
    int fadeValue() const { return m_fadeValue; }
    // An entry whose source names another session's radio: its fade stays
    // in the receive state whatever this window transmits.
    void setForeignSession(bool foreign) { m_foreignSession = foreign; }
    bool foreignSession() const { return m_foreignSession; }
    // The fade step interval: the item's update interval when set, else
    // kDefaultUpdateIntervalMs, at most 500 ms.
    int fadeIntervalMs() const;
    // Steps the fade once for each fadeIntervalMs() since the last step.
    // The first call, and a call while the fade is at rest, only take the
    // time. Returns true when the value changed. MeterWidget::advanceMeters
    // calls it for every item, beside advanceMeter().
    bool advanceFade(qint64 monotonicMs);

    // From Thetis MeterManager.cs:7072-7074 [v2.10.3.15] (clsMeterItem
    // ClearHistory): no history in the base.
    virtual void clearHistory() {}

    // Per-item update interval in ms; 0 (the value of a saved item without
    // one) takes every poll. MeterWidget::updateMeterValue asks
    // acceptsUpdate() before delivering a reading: true when the interval
    // has passed since the last accepted reading, which it then records.
    // Negative values are stored as 0. Virtual so an item can adjust the
    // value it keeps (LEDItem raises 1-49 ms to 100 ms).
    virtual void setUpdateIntervalMs(int ms);
    int updateIntervalMs() const { return m_updateIntervalMs; }
    // Virtual so an item with more than one reading (HistoryGraphItem) can
    // keep the interval per reading.
    virtual bool acceptsUpdate(qint64 monotonicMs);

    // Phase 3G-6 block 5 — MMIO binding. Null guid + empty (or
    // "--DEFAULT--") variable name means "no MMIO binding, fall back
    // to the WDSP-backed bindingId()". Set by the per-item editor's
    // "Variable…" picker popup. Serialization is deliberately out of
    // scope for block 5 — set lives only in the in-memory item.
    QUuid   mmioGuid() const { return m_mmioGuid; }
    QString mmioVariable() const { return m_mmioVariable; }
    bool    hasMmioBinding() const {
        return !m_mmioGuid.isNull()
            && !m_mmioVariable.isEmpty()
            && m_mmioVariable != QLatin1String("--DEFAULT--");
    }
    void setMmioBinding(const QUuid& guid, const QString& variableName) {
        m_mmioGuid = guid;
        m_mmioVariable = variableName;
    }
    void clearMmioBinding() {
        m_mmioGuid = QUuid();
        m_mmioVariable.clear();
    }

    QString mmioSourceKey() const { return m_mmioGuid.toString(QUuid::WithoutBraces) + QChar(0) + m_mmioVariable; }

    // Several variables per item (Thetis MeterVariables). Index 0 is the
    // pair above; index 1 and up are delivered through setSecondaryValue
    // and never replace the item's main reading. Indexes outside
    // 0..kMaxMmioVariables-1 are ignored.
    // From Thetis MeterManager.cs:22684-22738 [v2.10.3.15] — ANANMM reads 7.
    static constexpr int kMaxMmioVariables = 7;
    void    setMmioBinding(int index, const QUuid& guid, const QString& variableName);
    QUuid   mmioGuid(int index) const;
    QString mmioVariable(int index) const;
    bool    hasMmioBinding(int index) const;
    QString mmioSourceKey(int index) const;
    // True when any index, 0 included, reads a data source.
    bool    hasAnyMmioBinding() const;
    // A data source reading for variable `index` (1 and up). A non-finite
    // value means the source has no reading. No-op by default.
    virtual void setSecondaryValue(int index, double value) { Q_UNUSED(index); Q_UNUSED(value); }
    // How many variables a container item type reads (Thetis MeterVariables).
    static int mmioVariableCount(const QString& typeId);

    virtual Layer renderLayer() const = 0;
    virtual void paint(QPainter& p, int widgetW, int widgetH) = 0;
    virtual void emitVertices(QVector<float>& verts, int widgetW, int widgetH) {
        Q_UNUSED(verts); Q_UNUSED(widgetW); Q_UNUSED(widgetH);
    }

    // Multi-layer support: items that participate in multiple pipelines
    // override participatesIn() to return true for each layer they render to,
    // and override paintForLayer() to dispatch per-layer painting.
    // Default: single-layer (backward-compatible with existing items).
    virtual bool participatesIn(Layer layer) const { return layer == renderLayer(); }
    virtual void paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer) {
        Q_UNUSED(layer);
        paint(p, widgetW, widgetH);
    }

    virtual QString serialize() const;
    virtual bool deserialize(const QString& data);

    // New items only: ContainerContentRegistry::makeEntry calls this on the
    // item it allocates, before taking its record. Overrides call the base
    // first, then copy their k<Item>ThetisDefaults table onto the item.
    // Constructors keep the older values, so a saved record (or one with
    // fields missing) restores exactly as before. No-op in the base.
    virtual void applyNewItemDefaults();

    // Named values this item publishes for data out and text overlay
    // tokens, keyed by lowercase Thetis reading name (see
    // ThetisReadingNames.h), as Thetis copies clock and VFO display
    // readings into all_readings (MeterManager.cs:31057-31099
    // [v2.10.3.15]). Empty in the base.
    virtual QHash<QString, QVariant> publishedReadings() const { return {}; }

    // --- Mouse interaction (Phase 3G-5) ---
    virtual bool hitTest(const QPointF& pos, int widgetW, int widgetH) const;
    virtual bool handleMousePress(QMouseEvent* event, int widgetW, int widgetH);
    virtual bool handleMouseRelease(QMouseEvent* event, int widgetW, int widgetH);
    virtual bool handleMouseMove(QMouseEvent* event, int widgetW, int widgetH);
    virtual bool handleWheel(QWheelEvent* event, int widgetW, int widgetH);

public:
    // --- Stacked-row metadata (runtime only, not serialized) ---
    //
    // Thetis-parity stack model with a NereusSDR pixel floor.
    // When m_stackSlot >= 0 the item is part of a bar-row stack.
    // At every MeterWidget reflow the outer m_y/m_h are recomputed
    // from:
    //
    //   slotHNorm = slotHeightPx / widgetHeightPx
    //   slotYNorm = bandTop + stackSlot * slotHNorm
    //   m_y       = slotYNorm + m_slotLocalY * slotHNorm
    //   m_h       = m_slotLocalH * slotHNorm
    //
    // where slotHeightPx = max(0.05 * widgetHeightPx, 24) —
    // Thetis `_fHeight = 0.05f` (MeterManager.cs:21266) with a
    // 24-pixel floor so rows stay readable when the container is
    // small. bandTop is derived per-reflow from the composite
    // sitting above the stack (max of y+h among items with
    // itemHeight() > 0.30), so the stack shifts automatically when
    // a composite is added or removed.
    //
    // m_slotLocalY/m_slotLocalH hold the preset factory's
    // canonical within-slot 0..1 layout (SolidBg 0..1, BarItem
    // 0.2..0.8, ScaleItem 0..1 etc.) and are the source of truth
    // across reflows — the outer m_y/m_h are derived every time.
    //
    // These fields are deliberately NOT serialized: on load
    // MeterWidget::inferStackFromGeometry() walks deserialized
    // items and rebuilds the stack tagging from geometry so
    // existing containers keep working without a format bump.
    int   stackSlot() const { return m_stackSlot; }
    float slotLocalY() const { return m_slotLocalY; }
    float slotLocalH() const { return m_slotLocalH; }
    void  setStackSlot(int s) { m_stackSlot = s; }
    void  setSlotLocalY(float y) { m_slotLocalY = y; }
    void  setSlotLocalH(float h) { m_slotLocalH = h; }
    void  clearStackMetadata() {
        m_stackSlot = -1;
        m_slotLocalY = 0.0f;
        m_slotLocalH = 1.0f;
    }

    // Recompute m_y/m_h from stack metadata. No-op when
    // m_stackSlot < 0. Called from
    // MeterWidget::reflowStackedItems() on every resize.
    // `bandTop` is the normalized y at which the stack starts
    // (0 when no composite is present, else the composite's
    // max y+h).
    void layoutInStackSlot(int widgetHeightPx, int slotHeightPx,
                           float bandTop);

protected:
    QRect pixelRect(int widgetW, int widgetH) const {
        return QRect(
            static_cast<int>(m_x * widgetW),
            static_cast<int>(m_y * widgetH),
            static_cast<int>(m_w * widgetW),
            static_cast<int>(m_h * widgetH)
        );
    }

    QHash<int, QString> m_unavailableBindings;
    QHash<int, BindingSupport> m_bindingSupport;
    float m_x{0.0f};
    float m_y{0.0f};
    float m_w{1.0f};
    float m_h{1.0f};
    int m_bindingId{-1};
    double m_value{-140.0};
    int m_zOrder{0};

    // Stack metadata (runtime only — see public accessors).
    int   m_stackSlot{-1};
    float m_slotLocalY{0.0f};
    float m_slotLocalH{1.0f};

    // Visibility filter (see public accessors above)
    bool m_onlyWhenRx{false};
    bool m_onlyWhenTx{false};
    int  m_displayGroup{0};

    // Fade on RX / TX (see public accessors above).
    bool   m_fadeOnRx{false};
    bool   m_fadeOnTx{false};
    bool   m_explicitFade{false};
    bool   m_transmitting{false};
    bool   m_foreignSession{false};
    // From Thetis MeterManager.cs:6778 [v2.10.3.15] (_fadeValue = 255)
    int    m_fadeValue{255};
    qint64 m_lastFadeStepMs{-1};

    // Update interval (see public accessors above); runtime only below.
    int    m_updateIntervalMs{0};
    qint64 m_lastAcceptedUpdateMs{-1};

    // MMIO identity is captured by ContainerContentRegistry (Task2);
    // the old pipe primitive record still has no MMIO fields.
    // When hasMmioBinding() is true, MeterPoller
    // reads the value from the bound endpoint's variable cache
    // instead of the WDSP-backed bindingId() path.
    QUuid   m_mmioGuid;
    QString m_mmioVariable;
    // Variables 1 and up (slot i holds index i + 1); empty when none.
    struct MmioVariableBinding { QUuid guid; QString variable; };
    QVector<MmioVariableBinding> m_extraMmio;

    // Task 3.2 — unit-mode fan-out (S / dBm / µV).
    // Set by MultimeterPage → ContainerManager::forEachMeterItem broadcast.
    // Subclasses that display signal-level readouts consult m_unitMode /
    // m_showDecimal in their paint() text-format path.
    MeterUnit m_unitMode{MeterUnit::dBm};
    bool      m_showDecimal{true};
    // VHF S9 rule (setAboveS9Frequency): S9 = -93 dBm at or above 30 MHz.
    bool      m_aboveS9Frequency{false};
};

// ---------------------------------------------------------------------------
// SolidColourItem — Background fill
// ---------------------------------------------------------------------------
class SolidColourItem : public MeterItem {
    Q_OBJECT
public:
    explicit SolidColourItem(QObject* parent = nullptr) : MeterItem(parent) {}

    void setColour(const QColor& c) { m_colour = c; }
    QColor colour() const { return m_colour; }

    Layer renderLayer() const override { return Layer::Background; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;
    void applyNewItemDefaults() override;

private:
    QColor m_colour{0x0f, 0x0f, 0x1a};
};

// ---------------------------------------------------------------------------
// ImageItem — Static image
// ---------------------------------------------------------------------------
// Image variants by size: renderImage picks "-small" at or below this
// diagonal (px) and "-large" at or above the next.
// From Thetis MeterManager.cs:40644-40647 [v2.10.3.15] (renderImage)
constexpr double kImageSmallDiagonalPx = 450.0;
constexpr double kImageLargeDiagonalPx = 1200.0;

class ImageItem : public MeterItem {
    Q_OBJECT
public:
    explicit ImageItem(QObject* parent = nullptr) : MeterItem(parent) {}

    void setImage(const QImage& img) { m_image = img; }
    void setImagePath(const QString& path);
    QString imagePath() const { return m_imagePath; }

    // Thetis clsImage.DarkMode: prefer the "-dark" file when it exists.
    void setDarkMode(bool on) { m_darkMode = on; }
    bool darkMode() const { return m_darkMode; }

    // The file renderImage draws for basePath: the "-dark" file when dark
    // mode is on and it exists, then that name with "-small" (diagonal at or
    // below kImageSmallDiagonalPx) or "-large" (at or above
    // kImageLargeDiagonalPx) when that file exists. The suffix goes before
    // the extension (meter.png -> meter-dark-small.png). Only stats local
    // files next to the one the user chose; returns basePath otherwise.
    static QString variantPath(const QString& basePath, bool darkMode, double diagonalPx);

    Layer renderLayer() const override { return Layer::Background; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;
    void applyNewItemDefaults() override;

private:
    QImage   m_image;
    QString  m_imagePath;
    bool     m_darkMode{false};
    // The file m_image was last loaded from (runtime only).
    QString  m_loadedPath;
};

// New IMAGE items (makeEntry) take the clsImage constructor's values.
// From Thetis MeterManager.cs:16035-16046 [v2.10.3.15]
struct ImageItemThetisDefaults {
    bool darkMode;
};
inline constexpr ImageItemThetisDefaults kImageThetisDefaults{
    false,                      // _darkMode = false
};

// ---------------------------------------------------------------------------
// BarItem — Horizontal or vertical bar meter
// ---------------------------------------------------------------------------
// History duration range (ms).
// From Thetis setup.designer.cs:55011-55015 [v2.10.3.15] (nudMeterItemHistoryDuration.Minimum)
constexpr int kMinBarHistoryDurationMs = 50;
// From Thetis setup.designer.cs:55006-55010 [v2.10.3.15] (nudMeterItemHistoryDuration.Maximum)
constexpr int kMaxBarHistoryDurationMs = 30000;
// Ignore history after a transition (ms).
// From Thetis setup.designer.cs:55150-55154 [v2.10.3.15] (nudMeterItemIgnoreHistoryDuration.Minimum)
constexpr int kMinBarIgnoreHistoryMs = 0;
// From Thetis setup.designer.cs:55145-55149 [v2.10.3.15] (nudMeterItemIgnoreHistoryDuration.Maximum)
constexpr int kMaxBarIgnoreHistoryMs = 4000;
// From Thetis setup.designer.cs:55161-55165 [v2.10.3.15] (nudMeterItemIgnoreHistoryDuration.Value)
constexpr int kDefaultBarIgnoreHistoryMs = 2000;
// Magic eye and bezel size (1.0 = the full width).
// From Thetis setup.designer.cs:55523-55527 [v2.10.3.15] (nudMeterItemEyeScale.Minimum)
constexpr float kMinMeterEyeScale = 0.01f;
// From Thetis setup.designer.cs:55518-55522 [v2.10.3.15] (nudMeterItemEyeScale.Maximum)
constexpr float kMaxMeterEyeScale = 1.0f;
// From Thetis setup.designer.cs:55533-55537 [v2.10.3.15] (nudMeterItemEyeScale.Value;
// nudMeterItemEyeBezelScale :55450-55469 has the same range and value)
constexpr float kDefaultMeterEyeScale = 1.0f;
// Power limit of the scale (W).
// From Thetis setup.designer.cs:55313-55317 [v2.10.3.15] (nudMeterItemsPowerLimit.Minimum, 1 with scale 1)
constexpr double kMinBarPowerLimitW = 0.1;
// From Thetis setup.designer.cs:55308-55312 [v2.10.3.15] (nudMeterItemsPowerLimit.Maximum)
constexpr double kMaxBarPowerLimitW = 5000.0;
// From Thetis setup.designer.cs:55323-55327 [v2.10.3.15] (nudMeterItemsPowerLimit.Value)
constexpr double kDefaultBarPowerLimitW = 100.0;

class BarItem : public MeterItem {
    Q_OBJECT
public:
    enum class Orientation { Horizontal, Vertical };
    // From Thetis clsBarItem.BarStyle (MeterManager.cs:21247-21254 [v2.10.3.15]):
    //   None, Line, SolidFilled, GradientFilled, Segments.
    // Filled is NereusSDR's original full-height bar; Edge is a NereusSDR
    // extension (mapped from Thetis console.cs edge meters). Segments and
    // Solid are renderHBar's Segments and SolidFilled
    // (MeterManager.cs:37424-37673 [v2.10.3.15]). Appended so saved values
    // keep their meaning.
    enum class BarStyle { Filled, Edge, Line, Segments, Solid };

    // Segments / Solid geometry for a bar w x h pixels.
    // From Thetis MeterManager.cs:37458-37470 [v2.10.3.15] (renderHBar)
    struct SegmentGeometry {
        int   blockSize;
        int   gapSize;
        int   step;
        float barHeight;
    };
    static SegmentGeometry segmentGeometry(float w, float h);

    explicit BarItem(QObject* parent = nullptr) : MeterItem(parent) {}

    void setOrientation(Orientation o) { m_orientation = o; }
    Orientation orientation() const { return m_orientation; }

    void setRange(double minVal, double maxVal) { m_minVal = minVal; m_maxVal = maxVal; }
    double minVal() const { return m_minVal; }
    double maxVal() const { return m_maxVal; }

    void setBarColor(const QColor& c) { m_barColor = c; }
    QColor barColor() const { return m_barColor; }

    void setBarRedColor(const QColor& c) { m_barRedColor = c; }
    QColor barRedColor() const { return m_barRedColor; }

    void setRedThreshold(double t) { m_redThreshold = t; }
    double redThreshold() const { return m_redThreshold; }

    void setAttackRatio(float a) { m_attackRatio = a; }
    float attackRatio() const { return m_attackRatio; }

    void setDecayRatio(float d) { m_decayRatio = d; }
    float decayRatio() const { return m_decayRatio; }

    void setBarStyle(BarStyle s) { m_barStyle = s; }
    BarStyle barStyle() const { return m_barStyle; }

    // Edge mode colors (from Thetis console.cs:12612-12678)
    void setEdgeBackgroundColor(const QColor& c) { m_edgeBgColor = c; }
    QColor edgeBackgroundColor() const { return m_edgeBgColor; }

    void setEdgeLowColor(const QColor& c) { m_edgeLowColor = c; }
    QColor edgeLowColor() const { return m_edgeLowColor; }

    void setEdgeHighColor(const QColor& c) { m_edgeHighColor = c; }
    QColor edgeHighColor() const { return m_edgeHighColor; }

    void setEdgeAvgColor(const QColor& c) { m_edgeAvgColor = c; }
    QColor edgeAvgColor() const { return m_edgeAvgColor; }

    // --- Phase A1: ShowValue / ShowPeakValue / FontColour ---
    // From Thetis clsBarItem (MeterManager.cs:19939-19940, 21541, 21550,
    // 21706-21708). ShowValue draws the current value as text top-left of
    // the bar; ShowPeakValue draws the rolling peak top-right. FontColour
    // is the shared text color (Thetis default Yellow).
    void setShowValue(bool on) { m_showValue = on; }
    bool showValue() const { return m_showValue; }

    void setShowPeakValue(bool on) { m_showPeakValue = on; }
    bool showPeakValue() const { return m_showPeakValue; }

    void setFontColour(const QColor& c) { m_fontColour = c; }
    QColor fontColour() const { return m_fontColour; }

    // ShowPeakValue uses a separate colour so the current-value text
    // can be white while the peak text is red (Thetis default — see
    // Setup → Appearance → Meters/Gadgets "Peak Value" swatch, which
    // ships red). If unset, falls back to m_fontColour for
    // backward compat with pre-Phase-E BarItems that only had one
    // font colour.
    void setPeakFontColour(const QColor& c) { m_peakFontColour = c; }
    QColor peakFontColour() const
    {
        return m_peakFontColour.isValid() ? m_peakFontColour : m_fontColour;
    }

    // Rolling high-water-mark of all values seen via setValue(). Consumed
    // by ShowPeakValue text render and (in A3) the peak-hold marker.
    double peakValue() const { return m_peakValue; }

    // Smoothed value the bar draws, and the ShowValue / ShowPeakValue text
    // it draws (R-R3-13: "--" with no reading).  Read-only; paint() uses
    // the same text.
    double smoothedValue() const { return m_smoothedValue; }
    QString valueText() const;
    QString peakValueText() const;

    /// Snap the smoothed value + peak-hold marker to the supplied target
    /// value, bypassing the attack/decay smoothing that setValue() applies.
    /// Used on TX-end transitions: a single setValue(0) only decays one
    /// tick (e.g. 60 → 54 with decayRatio=0.1) so the bar appears stuck
    /// after un-key. clearSmoothing(0.0) snaps it.
    /// NereusSDR-original safety/UX: bench-reported #167 follow-up where
    /// Power and SWR bars stayed at the last sample after MOX-off.
    void clearSmoothing(double v) {
        m_value          = v;
        m_smoothedValue  = v;
        m_peakValue      = v;
    }

    // --- Phase A2: ShowHistory / HistoryColour / HistoryDuration ---
    // From Thetis clsBarItem (MeterManager.cs:19938, 19881, 19945-19946,
    // 20040-20053). History draws a translucent trailing fill behind the
    // live bar showing recent values. Thetis default HistoryDuration is
    // 4000ms (see addSMeterBar:21539 and AddADCMaxMag:21633).
    void setShowHistory(bool on) { m_showHistory = on; }
    bool showHistory() const { return m_showHistory; }

    void setHistoryColour(const QColor& c) { m_historyColour = c; }
    QColor historyColour() const { return m_historyColour; }

    void setHistoryDurationMs(int ms) { m_historyDurationMs = ms; }
    int historyDurationMs() const { return m_historyDurationMs; }

    // Introspection for tests + the render pass.
    int historySampleCount() const { return static_cast<int>(m_history.size()); }

    // --- Phase A3: ShowMarker / MarkerColour / PeakHoldMarkerColour ---
    // From Thetis clsBarItem (MeterManager.cs:21543-21544). ShowMarker
    // draws a thin vertical line at the live smoothed value; the separate
    // peak-hold marker draws at the decaying peakValue(). The peak-hold
    // decay ratio is independent of attack/decay smoothing — when non-zero,
    // peakValue() falls toward the live value when values drop.
    void setShowMarker(bool on) { m_showMarker = on; }
    bool showMarker() const { return m_showMarker; }

    void setMarkerColour(const QColor& c) { m_markerColour = c; }
    QColor markerColour() const { return m_markerColour; }

    void setPeakHoldMarkerColour(const QColor& c) { m_peakHoldMarkerColour = c; }
    QColor peakHoldMarkerColour() const { return m_peakHoldMarkerColour; }

    void setPeakHoldDecayRatio(float r) { m_peakHoldDecayRatio = r; }
    float peakHoldDecayRatio() const { return m_peakHoldDecayRatio; }

    // Peak hold marker, drawn at the peak independently of the live marker.
    // From Thetis MeterManager.cs:21497-21501, 37600-37602 [v2.10.3.15]
    // (PeakHold). Unset (records saved before the option existed, and
    // bars built in code) follows showMarker(), which drew both markers.
    void setPeakHold(bool on) { m_peakHold = on; }
    bool peakHold() const { return m_peakHold.value_or(m_showMarker); }

    // Peak value text colour (Thetis PeakValueColour,
    // MeterManager.cs:21492-21496 [v2.10.3.15]); the same slot as
    // peakFontColour().
    void setPeakValueColour(const QColor& c) { setPeakFontColour(c); }
    QColor peakValueColour() const { return peakFontColour(); }

    // Marker line width (Thetis StrokeWidth). NereusSDR drew 2 px markers.
    void setStrokeWidth(float w) { m_strokeWidth = w > 0.0f ? w : 1.0f; }
    float strokeWidth() const { return m_strokeWidth; }

    // After a transition the next IgnoreHistory ms of readings do not
    // reach the history or the peak. From Thetis MeterManager.cs:21367-21378,
    // 21422-21426, 21450-21457 [v2.10.3.15] (IgnoreHistoryDuration,
    // ClearHistory). Clamped to kMinBarIgnoreHistoryMs..kMaxBarIgnoreHistoryMs.
    void setIgnoreHistoryMs(int ms);
    int ignoreHistoryMs() const { return m_ignoreHistoryMs; }
    void clearHistory() override;
    int ignoringReadings() const { return m_ignoringNext; }

    // Power limit (W): a calibrated forward or reverse power bar reads
    // value * 100 / limit. From Thetis MeterManager.cs:41053-41056,
    // 28292 [v2.10.3.15]. Clamped to kMinBarPowerLimitW..kMaxBarPowerLimitW.
    void setPowerLimit(double watts);
    double powerLimit() const { return m_powerLimit; }

    // --- Phase A4: ScaleCalibration (non-linear value → X map) ---
    // From Thetis clsBarItem.ScaleCalibration (MeterManager.cs:20192,
    // 21547-21549 addSMeterBar, 21638-21640 AddADCMaxMag). When calibration
    // waypoints are set, the bar uses piecewise-linear interpolation
    // between them instead of the linear minVal..maxVal mapping. Used by
    // the S-meter to map the non-linear dBm→S-unit curve. When empty,
    // BarItem falls back to linear range mapping (pre-A4 behavior).
    void addScaleCalibration(double value, float normalizedX);
    void clearScaleCalibration() { m_scaleCalibration.clear(); }
    QMap<double, float> scaleCalibration() const { return m_scaleCalibration; }
    int scaleCalibrationSize() const
    {
        return static_cast<int>(m_scaleCalibration.size());
    }

    // Map a raw value to a normalized X position in [0, 1]. Uses
    // ScaleCalibration if populated, otherwise linear range mapping.
    // Consumed by paint() for bar fill length, live marker, peak marker,
    // and history polyline — the single source of truth for "where on
    // the bar does this value sit".
    float valueToNormalizedX(double value) const;

    // Override setValue() for exponential smoothing
    // From Thetis MeterManager.cs — attack/decay smoothing
    void setValue(double v) override;

    // renderLayer() stays at Geometry for legacy code that queries it,
    // but participatesIn() opts the bar out of the GPU Geometry pass
    // and into the OverlayDynamic QPainter pass. Phase D1b needs every
    // A-phase render (history polyline, ShowValue text, ShowMarker +
    // PeakHold lines, BarStyle::Line baseline, calibrated value
    // positioning) and none of those can be expressed as a GPU vertex
    // quad without major shader work. Running through the overlay
    // QPainter is simpler and matches how ScaleItem + TextItem already
    // render in the same render cycle. emitVertices() below is kept
    // as a safety no-op fall-through in case some other code path
    // still iterates Geometry items directly.
    Layer renderLayer() const override { return Layer::Geometry; }
    bool participatesIn(Layer layer) const override {
        return layer == Layer::OverlayDynamic;
    }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    void emitVertices(QVector<float>& verts, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;
    void applyNewItemDefaults() override;

private:
    void paintEdge(QPainter& p, int widgetW, int widgetH);
    void paintSegmentsOrSolid(QPainter& p, const QRect& rect, float fracLive);

    Orientation m_orientation{Orientation::Horizontal};
    double      m_minVal{-140.0};
    double      m_maxVal{0.0};
    // AetherSDR cyan bar color
    QColor      m_barColor{0x00, 0xb4, 0xd8};
    QColor      m_barRedColor{0xff, 0x44, 0x44};
    double      m_redThreshold{1000.0}; // disabled by default (above maxVal)
    // From Thetis MeterManager.cs
    float       m_attackRatio{0.8f};
    float       m_decayRatio{0.2f};
    double      m_smoothedValue{-140.0};
    BarStyle    m_barStyle{BarStyle::Filled};
    QColor      m_edgeBgColor{Qt::black};
    QColor      m_edgeLowColor{Qt::white};
    QColor      m_edgeHighColor{Qt::red};
    QColor      m_edgeAvgColor{Qt::yellow};
    // Phase A1 — clsBarItem text/peak fields
    bool        m_showValue{false};
    bool        m_showPeakValue{false};
    QColor      m_fontColour{Qt::yellow};
    QColor      m_peakFontColour{};  // invalid = use m_fontColour
    double      m_peakValue{-std::numeric_limits<double>::infinity()};
    // Phase A2 — clsBarItem history fields
    bool        m_showHistory{false};
    QColor      m_historyColour{255, 0, 0, 128};  // Thetis default Red(128)
    int         m_historyDurationMs{4000};         // Thetis default
    QVector<double> m_history;                     // rolling sample buffer
    // Phase A3 — clsBarItem marker fields
    bool        m_showMarker{false};
    QColor      m_markerColour{Qt::yellow};
    QColor      m_peakHoldMarkerColour{Qt::red};
    float       m_peakHoldDecayRatio{0.0f};        // 0 = hold forever
    std::optional<bool> m_peakHold;                // unset = follows m_showMarker
    float       m_strokeWidth{2.0f};               // NereusSDR's 2 px markers
    int         m_ignoreHistoryMs{0};              // saved bars never ignored
    int         m_ignoringNext{0};
    double      m_powerLimit{kDefaultBarPowerLimitW};
    // Phase A4 — non-linear value→X calibration waypoints.
    // QMap keeps entries sorted by key (value) which is what the
    // interpolation in valueToNormalizedX() relies on.
    QMap<double, float> m_scaleCalibration;
};

// New BAR items (makeEntry) take the clsBarItem constructor's values.
// From Thetis MeterManager.cs:21284-21302 [v2.10.3.15]
struct BarItemThetisDefaults {
    int                historyDurationMs;
    int                ignoreHistoryMs;
    bool               showHistory;
    bool               showValue;
    bool               showPeakValue;
    BarItem::BarStyle  style;
    QRgb               colour;
    QRgb               colourHigh;
    QRgb               markerColour;
    QRgb               peakHoldMarkerColour;
    QRgb               peakValueColour;
    float              strokeWidth;
    bool               peakHold;
    bool               showMarker;
};
inline constexpr BarItemThetisDefaults kBarItemThetisDefaults{
    2000,                       // _msHistoryDuration = 2000
    2000,                       // _msIgnoreHistoryDuration = 2000
    false,                      // _showHistory = false
    true,                       // _showValue = true
    true,                       // _showPeakValue = true
    BarItem::BarStyle::Line,    // _style = BarStyle.Line
    qRgb(0xff, 0x00, 0x00),     // _colour = Color.Red
    qRgb(0xff, 0xa5, 0x00),     // _colourHigh = Color.Orange
    qRgb(0xff, 0xff, 0x00),     // _markerColour = Color.Yellow
    qRgb(0xff, 0x00, 0x00),     // _peakHoldMarkerColour = Color.Red
    qRgb(0xff, 0x00, 0x00),     // _peakValueColour = Color.Red
    3.0f,                       // _strokeWidth = 3f
    false,                      // _peakHold = false
    true,                       // _showMarker = true
};

// ---------------------------------------------------------------------------
// ScaleItem — Tick marks and labels
// ---------------------------------------------------------------------------
class ScaleItem : public MeterItem {
    Q_OBJECT
public:
    enum class Orientation { Horizontal, Vertical };

    explicit ScaleItem(QObject* parent = nullptr) : MeterItem(parent) {}

    void setOrientation(Orientation o) { m_orientation = o; }
    Orientation orientation() const { return m_orientation; }

    void setRange(double minVal, double maxVal) { m_minVal = minVal; m_maxVal = maxVal; }
    double minVal() const { return m_minVal; }
    double maxVal() const { return m_maxVal; }

    void setMajorTicks(int n) { m_majorTicks = n; }
    int majorTicks() const { return m_majorTicks; }

    void setMinorTicks(int n) { m_minorTicks = n; }
    int minorTicks() const { return m_minorTicks; }

    void setTickColor(const QColor& c) { m_tickColor = c; }
    QColor tickColor() const { return m_tickColor; }

    void setLabelColor(const QColor& c) { m_labelColor = c; }
    QColor labelColor() const { return m_labelColor; }

    void setFontSize(int sz) { m_fontSize = sz; }
    int fontSize() const { return m_fontSize; }

    // --- Phase B2: ShowType centered title ---
    // From Thetis clsScaleItem (MeterManager.cs:14827 ShowType property,
    // 31879-31886 render pass). When true, paint() draws readingName()
    // of the scale's bindingId centered in the top strip of the scale
    // rect. Used by every bar-row preset to label its row with the
    // canonical Thetis reading name in red.
    void setShowType(bool on) { m_showType = on; }
    bool showType() const { return m_showType; }

    void setTitleColour(const QColor& c) { m_titleColour = c; }
    QColor titleColour() const { return m_titleColour; }

    // --- Phase B3: GeneralScale two-tone render + baseline at y + h*0.85 ---
    // From Thetis MeterManager.cs:32338-32423 generalScale(). When
    // scaleStyle() == GeneralScale, paint() draws a two-tone horizontal
    // baseline split at centrePerc with major+minor ticks extending
    // upward from the baseline, matching Thetis exactly. The default
    // ScaleStyle::Linear leaves the pre-B3 NereusSDR evenly-spaced tick
    // renderer untouched, so existing Filled presets don't regress.
    //
    // Opt in by calling setScaleStyle(GeneralScale) and configuring
    // setGeneralScaleParams(low, high, start, end, lowInc, highInc,
    // centrePerc). setLowColour / setHighColour override the default
    // tick colours for the two halves of the scale.
    enum class ScaleStyle { Linear, GeneralScale };

    void setScaleStyle(ScaleStyle s) { m_scaleStyle = s; }
    ScaleStyle scaleStyle() const { return m_scaleStyle; }

    void setLowColour(const QColor& c) { m_lowColour = c; }
    QColor lowColour() const { return m_lowColour; }

    void setHighColour(const QColor& c) { m_highColour = c; }
    QColor highColour() const { return m_highColour; }

    void setGeneralScaleParams(int lowLongTicks,
                               int highLongTicks,
                               int lowStartNumber,
                               int highEndNumber,
                               int lowIncrement,
                               int highIncrement,
                               float centrePerc)
    {
        m_lowLongTicks   = lowLongTicks;
        m_highLongTicks  = highLongTicks;
        m_lowStartNumber = lowStartNumber;
        m_highEndNumber  = highEndNumber;
        m_lowIncrement   = lowIncrement;
        m_highIncrement  = highIncrement;
        m_centrePerc     = centrePerc;
    }

    int lowLongTicks() const { return m_lowLongTicks; }
    int highLongTicks() const { return m_highLongTicks; }

    Layer renderLayer() const override { return Layer::OverlayStatic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;
    void applyNewItemDefaults() override;

private:
    Orientation m_orientation{Orientation::Horizontal};
    double      m_minVal{-140.0};
    double      m_maxVal{0.0};
    int         m_majorTicks{7};
    int         m_minorTicks{5};
    // AetherSDR colors
    QColor      m_tickColor{0xc8, 0xd8, 0xe8};
    QColor      m_labelColor{0x80, 0x90, 0xa0};
    int         m_fontSize{10};
    // Phase B2 — ShowType title
    bool        m_showType{false};
    QColor      m_titleColour{Qt::red};   // Thetis default per screenshot
    // Phase B3 — GeneralScale two-tone renderer
    ScaleStyle  m_scaleStyle{ScaleStyle::Linear};
    QColor      m_lowColour{0xc8, 0xd8, 0xe8};
    QColor      m_highColour{0xff, 0x80, 0x40};
    int         m_lowLongTicks{6};
    int         m_highLongTicks{3};
    int         m_lowStartNumber{-1};
    int         m_highEndNumber{60};
    int         m_lowIncrement{2};
    int         m_highIncrement{20};
    float       m_centrePerc{-1.0f};  // -1 = auto from tick counts
};

// New SCALE items (makeEntry) take the clsScaleItem constructor's values.
// From Thetis MeterManager.cs:16118-16140 [v2.10.3.15]
struct ScaleItemThetisDefaults {
    QRgb lowColour;
    QRgb highColour;
    QRgb titleColour;
    int  fontSize;
};
inline constexpr ScaleItemThetisDefaults kScaleThetisDefaults{
    qRgb(0xff, 0xff, 0xff),     // _lowColour = System.Drawing.Color.White
    qRgb(0xff, 0x00, 0x00),     // _highColour = System.Drawing.Color.Red
    qRgb(0xa9, 0xa9, 0xa9),     // _fontColourMeterType = System.Drawing.Color.DarkGray
    20,                         // _fontSize = 20f
};

// Forward-declare the private render helper called from ScaleItem::paint.
// (No public API; kept in the cpp file.)

// ---------------------------------------------------------------------------
// TextItem — Dynamic value readout
// ---------------------------------------------------------------------------
class TextItem : public MeterItem {
    Q_OBJECT
public:
    explicit TextItem(QObject* parent = nullptr) : MeterItem(parent) {}

    void setLabel(const QString& label) { m_label = label; }
    QString label() const { return m_label; }

    void setTextColor(const QColor& c) { m_textColor = c; }
    QColor textColor() const { return m_textColor; }

    void setFontSize(int sz) { m_fontSize = sz; }
    int fontSize() const { return m_fontSize; }

    void setBold(bool bold) { m_bold = bold; }
    bool bold() const { return m_bold; }

    void setSuffix(const QString& suffix) { m_suffix = suffix; }
    QString suffix() const { return m_suffix; }

    void setDecimals(int decimals) { m_decimals = decimals; }
    int decimals() const { return m_decimals; }

    // Text shown when value is below minValidValue (no signal / TX off)
    void setIdleText(const QString& text) { m_idleText = text; }
    QString idleText() const { return m_idleText; }
    void setMinValidValue(double v) { m_minValidValue = v; }
    double minValidValue() const { return m_minValidValue; }

    // Font family; empty draws the application font (records saved before
    // the family was kept).
    void setFontFamily(const QString& family) { m_fontFamily = family; }
    QString fontFamily() const { return m_fontFamily; }

    void setItalic(bool italic) { m_italic = italic; }
    bool italic() const { return m_italic; }

    // Thetis clsText.Centre: centred in the item, else drawn from its top
    // left corner. Saved records without the field stay centred.
    void setCentred(bool centred) { m_centred = centred; }
    bool centred() const { return m_centred; }

    // Text for the %group% token: the name of the container's display
    // group (Thetis clsMeter.DisplayGroupText). Runtime only, not saved.
    void setDisplayGroupText(const QString& text) { m_displayGroupText = text; }
    QString displayGroupText() const { return m_displayGroupText; }

    // Thetis clsText label tokens. %value% shows the reading to one
    // decimal, %valueWithType% adds its unit, %group% (any case) the
    // display group's name.
    static QString valueToken()         { return QStringLiteral("%value%"); }
    static QString valueWithTypeToken() { return QStringLiteral("%valueWithType%"); }
    static QString groupToken()         { return QStringLiteral("%group%"); }

    // The unit Thetis appends for %valueWithType% (ReadingUnits) for a
    // MeterBinding id; empty when Thetis has none.
    static QString readingUnits(int bindingId);

    // The text paint() draws: a token's text, the label, the idle text, the
    // formatted value, or "--" with the unit when there is no reading
    // (R-R3-13).
    QString displayText() const;

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;
    void applyNewItemDefaults() override;

private:
    QString m_label;
    // AetherSDR colors
    QColor  m_textColor{0xc8, 0xd8, 0xe8};
    int     m_fontSize{13};
    bool    m_bold{true};
    QString m_suffix{QStringLiteral(" dBm")};
    int     m_decimals{1};
    QString m_idleText;
    double  m_minValidValue{-139.0};  // below default -140 = always show
    QString m_fontFamily;             // empty = application font
    bool    m_italic{false};
    bool    m_centred{true};          // NereusSDR always centred before
    QString m_displayGroupText;       // runtime only
};

// New TEXT items (makeEntry) take the clsText constructor's values.
// From Thetis MeterManager.cs:22259-22267 [v2.10.3.15]
struct TextItemThetisDefaults {
    const char* fontFamily;
    bool        bold;
    bool        italic;
    QRgb        colour;
    bool        centred;
    int         fontSize;
};
inline constexpr TextItemThetisDefaults kTextThetisDefaults{
    "Trebuchet MS",             // _fontFamily = "Trebuchet MS"
    false,                      // _fontStyle = FontStyle.Regular
    false,                      // _fontStyle = FontStyle.Regular
    qRgb(0xff, 0xff, 0xff),     // _color = System.Drawing.Color.White
    false,                      // _centred = false
    9,                          // _fontSize = 9f
};

// ---------------------------------------------------------------------------
// NeedleItem — Arc-style S-meter needle (AetherSDR SMeterWidget port)
// Participates in all 4 render layers:
//   P1 Background:     arc colored segments (white S0-S9, red S9+)
//   P2 Geometry:       needle quad + peak marker triangle
//   P3 OverlayStatic:  tick marks, tick labels, source label
//   P3 OverlayDynamic: S-unit readout (left), dBm readout (right)
// ---------------------------------------------------------------------------
class NeedleItem : public MeterItem {
    Q_OBJECT
public:
    // --- ANANMM/CrossNeedle extensions (Phase 3G-4) ---
    enum class NeedleDirection { Clockwise, CounterClockwise };

    explicit NeedleItem(QObject* parent = nullptr);

    // --- Arc geometry constants (from AetherSDR SMeterWidget.cpp) ---
    static constexpr float kArcStartDeg = 55.0f;   // right end (S9+60)
    static constexpr float kArcEndDeg   = 125.0f;  // left end (S0)
    static constexpr float kRadiusRatio = 0.85f;    // radius = width * 0.85
    static constexpr float kCenterYRatio = 0.65f;   // cy = h + radius - h*0.65
    // Aspect ratio for scaling arc when height is constrained.
    // Higher value = wider arc filling more of the panel width.
    static constexpr float kTargetAspect = 2.8f;

    // --- S-meter scale constants (from AetherSDR SMeterWidget.h) ---
    static constexpr float kS0Dbm  = -127.0f;      // S0
    static constexpr float kS9Dbm  = -73.0f;        // S9 = S0 + 9*6
    static constexpr float kMaxDbm = -13.0f;         // S9+60
    static constexpr float kDbPerS = 6.0f;           // dB per S-unit

    // --- Smoothing (from AetherSDR SMOOTH_ALPHA) ---
    static constexpr float kSmoothAlpha = 0.3f;

    // --- Peak hold: Medium preset (from AetherSDR) ---
    static constexpr int   kPeakHoldFrames   = 5;    // 500ms at 100ms poll
    static constexpr float kPeakDecayPerFrame = 1.0f; // 10 dB/sec = 1dB/100ms
    static constexpr int   kPeakResetFrames  = 100;   // 10s hard reset

    void setSourceLabel(const QString& label) { m_sourceLabel = label; }
    QString sourceLabel() const { return m_sourceLabel; }

    // --- ANANMM/CrossNeedle extension setters/getters (Phase 3G-4) ---
    void setScaleCalibration(const QMap<float, QPointF>& cal) { m_scaleCalibration = cal; }
    QMap<float, QPointF> scaleCalibration() const { return m_scaleCalibration; }

    void setNeedleOffset(const QPointF& off) { m_needleOffset = off; }
    QPointF needleOffset() const { return m_needleOffset; }

    void setRadiusRatio(const QPointF& r) { m_radiusRatio = r; }
    QPointF radiusRatio() const { return m_radiusRatio; }

    void setLengthFactor(float f) { m_lengthFactor = f; }
    float lengthFactor() const { return m_lengthFactor; }

    void setStrokeWidth(float w) { m_strokeWidth = w; }
    float strokeWidth() const { return m_strokeWidth; }

    void setDirection(NeedleDirection d) { m_direction = d; }
    NeedleDirection direction() const { return m_direction; }

    void setHistoryEnabled(bool v) { m_historyEnabled = v; }
    bool historyEnabled() const { return m_historyEnabled; }
    // Thetis ShowHistory: draw the history arc.
    void setShowHistory(bool v) { m_historyEnabled = v; }
    bool showHistory() const { return m_historyEnabled; }

    // How long the history arc and the peak hold look back. Clamped to
    // kMinBarHistoryDurationMs..kMaxBarHistoryDurationMs (the same setup
    // control serves every item). From Thetis MeterManager.cs:22100-22108,
    // 22018-22025 [v2.10.3.15]
    void setHistoryDuration(int ms);
    int historyDuration() const { return m_historyDuration; }
    void setHistoryDurationMs(int ms) { setHistoryDuration(ms); }
    int historyDurationMs() const { return m_historyDuration; }

    // After clearHistory() the next IgnoreHistory ms of readings move the
    // needle but do not reach the history or the peak.
    // From Thetis MeterManager.cs:22018-22029, 22131-22139 [v2.10.3.15]
    // Clamped to kMinBarIgnoreHistoryMs..kMaxBarIgnoreHistoryMs.
    void setIgnoreHistoryMs(int ms);
    int ignoreHistoryMs() const { return m_ignoreHistoryMs; }
    void clearHistory() override;
    int ignoringReadings() const { return m_ignoringNext; }
    int historySize() const { return static_cast<int>(m_history.size()); }
    // MinHistory / MaxHistory: the live value with no history.
    // From Thetis MeterManager.cs:22109-22130 [v2.10.3.15]
    float minHistory() const;
    float maxHistory() const;

    // Thetis Shadow: the shadow under the needle. On for saved needles.
    void setShadow(bool on) { m_shadow = on; }
    bool shadow() const { return m_shadow; }

    // Thetis PeakHold: a marker at the highest value in the history. A
    // saved needle without the setting keeps NereusSDR's marker: on for
    // the built-in S-meter arc, off for a calibrated face.
    void setPeakHold(bool on) { m_peakHold = on; }
    bool peakHold() const { return m_peakHold.value_or(m_scaleCalibration.isEmpty()); }
    void setPeakHoldColour(const QColor& c) { m_peakHoldColour = c; }
    QColor peakHoldColour() const { return m_peakHoldColour; }
    // The value the peak hold marker shows.
    float peakValue() const;

    void setHistoryColor(const QColor& c) { m_historyColor = c; }
    QColor historyColor() const { return m_historyColor; }

    void setNormaliseTo100W(bool v) { m_normaliseTo100W = v; }
    bool normaliseTo100W() const { return m_normaliseTo100W; }

    // Power limit (W): with normalise to 100 W on, a reading is drawn at
    // value * 100 / limit. From Thetis MeterManager.cs:41053-41056
    // [v2.10.3.15] (getPerc). Clamped to kMinBarPowerLimitW..
    // kMaxBarPowerLimitW; below the minimum recovers to the default.
    void setMaxPower(float p);
    float maxPower() const { return m_maxPower; }
    void setPowerLimit(double watts) { setMaxPower(static_cast<float>(watts)); }
    double powerLimit() const { return static_cast<double>(m_maxPower); }

    // The value the face is positioned by for a reading: the VHF S9 rule
    // for signal strength, else the power limit.
    // From Thetis MeterManager.cs:41047-41057 [v2.10.3.15] (getPerc)
    float scaleValue(float value) const;

    // The needle tip for a value, in widget pixels, as paint() and
    // emitVertices() draw it.
    QPointF needleTipAt(float value, int widgetW, int widgetH) const;
    QPointF needlePivot(int widgetW, int widgetH) const;

    // New items (makeEntry) take the clsNeedleItem constructor's values.
    void applyNewItemDefaults() override;

    void setNeedleColor(const QColor& c) { m_needleColor = c; }
    QColor needleColor() const { return m_needleColor; }

    void setAttackRatio(float a) { m_attackRatio = a; }
    float attackRatio() const { return m_attackRatio; }

    void setDecayRatio(float d) { m_decayRatio = d; }
    float decayRatio() const { return m_decayRatio; }

    // Interpolate needle position from scale calibration map
    // From Thetis MeterManager.cs calibration point interpolation
    QPointF calibratedPosition(float value) const;

    void setValue(double v) override;

    // Smoothed value as currently held by the needle. Named
    // m_smoothedDbm historically because the needle started life as a
    // dBm-only S-meter; for calibrated needles (ANANMM Volts/Amps/
    // Power/SWR/Compression/ALC) it holds the value in the calibration
    // map's native units (volts, amps, watts, etc.) — see setValue()
    // for the per-mode clamp logic. Exposed so tests can assert the
    // post-smoothing value without round-tripping through paint
    // geometry.
    float smoothedValue() const { return m_smoothedDbm; }

    // The two readouts paintOverlayDynamic() draws for an uncalibrated
    // needle: S-units (left) and the value in the selected unit (right).
    // Both show "--" with no reading (R-R3-13).
    QString sUnitsReadout() const;
    QString valueReadout() const;

    // Multi-layer: participates in all 4 pipeline layers
    bool participatesIn(Layer layer) const override;
    Layer renderLayer() const override { return Layer::Geometry; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    void paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer) override;
    void emitVertices(QVector<float>& verts, int widgetW, int widgetH) override;

    QString serialize() const override;
    bool deserialize(const QString& data) override;

private:
    // From AetherSDR SMeterWidget.cpp dbmToFraction()
    float dbmToFraction(float dbm) const;
    // dbmToFraction of a reading, moved for the VHF S9 rule.
    float arcFraction(float dbm) const;
    // From AetherSDR SMeterWidget.cpp sUnitsText()
    QString sUnitsText(float dbm) const;

    void paintBackground(QPainter& p, int widgetW, int widgetH);
    void paintOverlayStatic(QPainter& p, int widgetW, int widgetH);
    void paintOverlayDynamic(QPainter& p, int widgetW, int widgetH);

    // Compute aspect-locked drawing rect (2:1 like AetherSDR 280x140),
    // centered within the item's pixel rect.
    QRect meterRect(int widgetW, int widgetH) const;

    // Pivot and arc of the needle for one widget size, shared by paint()
    // and emitVertices().
    struct NeedleGeometry {
        bool  calibrated{false};
        float left{0.0f};
        float top{0.0f};
        float pw{0.0f};
        float ph{0.0f};
        float pivotX{0.0f};
        float pivotY{0.0f};
        float arcCx{0.0f};
        float arcCy{0.0f};
        float arcRadius{0.0f};
    };
    NeedleGeometry needleGeometry(int widgetW, int widgetH) const;
    QPointF tipFor(const NeedleGeometry& g, float value) const;
    // The history arc outline: the pivot, then tips from minHistory() to
    // maxHistory(). Empty when there is nothing to draw.
    QVector<QPointF> historyOutline(const NeedleGeometry& g) const;
    // The peak marker triangle on the S-meter arc.
    QVector<QPointF> arcPeakMarker(const NeedleGeometry& g, float value) const;
    // The raw value range a calibrated face clamps the needle to.
    float rawCalibrationMin() const;
    float rawCalibrationMax() const;
    float rawFromScale(float key) const;
    int historyIntervalMs() const;

    QString m_sourceLabel{QStringLiteral("S-Meter")};
    float   m_smoothedDbm{kS0Dbm};
    float   m_peakDbm{kS0Dbm};
    int     m_peakHoldCounter{0};
    int     m_peakResetCounter{0};

    // --- ANANMM/CrossNeedle extensions (Phase 3G-4) ---
    // Custom scale calibration: value -> normalized (x,y) position on gauge face.
    // When non-empty, paint() uses this instead of dbmToFraction().
    // From Thetis MeterManager.cs AddAnanMM() calibration tables
    QMap<float, QPointF> m_scaleCalibration;

    // Needle geometry overrides
    QPointF m_needleOffset{0.0, 0.0};     // Center offset for needle pivot
    QPointF m_radiusRatio{1.0, 1.0};      // Elliptical scaling
    float   m_lengthFactor{1.0f};         // Needle length multiplier
    float   m_strokeWidth{1.5f};          // Needle line thickness

    // Direction
    NeedleDirection m_direction{NeedleDirection::Clockwise};

    // History trail
    bool   m_historyEnabled{false};
    int    m_historyDuration{4000};  // ms
    QColor m_historyColor{Qt::transparent};
    int    m_ignoreHistoryMs{0};     // saved needles never ignored
    int    m_ignoringNext{0};
    QVector<float> m_history;        // smoothed values, oldest first

    // Shadow and peak hold. Saved needles: shadow on, NereusSDR's amber
    // (#ffaa00) peak marker on the S-meter arc.
    bool   m_shadow{true};
    std::optional<bool> m_peakHold;  // unset = on for the arc, off calibrated
    QColor m_peakHoldColour{0xff, 0xaa, 0x00};

    // Power normalisation
    bool  m_normaliseTo100W{false};
    float m_maxPower{100.0f};

    // Needle color (default white for S-meter backward compat)
    QColor m_needleColor{Qt::white};

    // Attack/decay smoothing ratios (defaults match S-meter behavior)
    float m_attackRatio{0.3f};   // kSmoothAlpha default
    float m_decayRatio{0.3f};    // kSmoothAlpha default
};

// New NEEDLE items (makeEntry) take the clsNeedleItem constructor's values.
// From Thetis MeterManager.cs:21942-21973 [v2.10.3.15]; the history colour
// from the clsMeterItem constructor, MeterManager.cs:6763 [v2.10.3.15].
struct NeedleItemThetisDefaults {
    bool  shadow;
    bool  peakHold;
    QRgb  peakHoldColour;
    bool  showHistory;
    int   historyDurationMs;
    int   ignoreHistoryMs;
    QRgb  colour;
    float strokeWidth;
    float attackRatio;
    float decayRatio;
    QRgb  historyColour;
};
inline constexpr NeedleItemThetisDefaults kNeedleThetisDefaults{
    true,                           // _shadow = true
    false,                          // _peakHold = false
    qRgb(0xff, 0x00, 0x00),         // _peakHoldMarkerColour = Color.Red
    false,                          // _showHistory = false
    500,                            // _msHistoryDuration = 500
    2000,                           // _msIgnoreHistoryDuration = 2000
    qRgb(0xff, 0xff, 0xff),         // _colour = Color.White
    3.0f,                           // _strokeWidth = 3f
    0.8f,                           // AttackRatio = 0.8f
    0.2f,                           // DecayRatio = 0.2f
    qRgba(0xff, 0xff, 0xff, 128),   // _historyColour = FromArgb(128, 255, 255, 255)
};

} // namespace NereusSDR
