#pragma once

// =================================================================
// src/gui/meters/LEDItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.designer.cs (LED update interval,
//     offset, size and padding ranges): upstream source has no top-of-file
//     GPL header, the setup.cs header below covers its partial class
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-10-26 - Options from Thetis clsLed / renderLed: placement by
//                 offset and size inside a panel width x padding tall, the
//                 colour fade between false and true (250 ms, blink edge
//                 and drop 100 ms), panel RX/TX colours with the flip fade,
//                 show true / show false, process when hidden, light mode,
//                 isTrue(), an update interval under 50 ms raised to 100 ms,
//                 and kLedThetisDefaults for new items. Fields appended to
//                 the record; saved LEDs keep the fill look and animation.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Condition: a Thetis-style LED condition in NereusSDR's
//                 expression language (core/LedConditionExpression.h),
//                 its %name% values from the entry's slice readings, the
//                 clock values and meter data sources, appended to the
//                 record; empty, the thresholds decide as before.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - No TX (Thetis NoTxTrue, MeterManager.cs:21177-21211):
//                 while transmitting, the LED turning true asks for a stop
//                 (txStopRequested, "Led Indicator stopped MOX"), once per
//                 transition; preview copies never ask. The session rule
//                 (ledStopAppliesToSession) and the stop's route
//                 (stopTransmitForLed: StopAllTx on a local radio, the
//                 window's own release at the Core on a remote one). It
//                 only ever stops; nothing here keys. Appended to the
//                 record. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-26 - Fix round 1: No TX when false (Thetis NoTxFalse), saved
//                 after No TX; no stop while the condition has an error
//                 (Thetis runs Update only for a valid script); the
//                 warning the user sees in NereusSDR's words. J.J. Boyd
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

#include "MeterItem.h"
#include "core/LedConditionExpression.h"
#include <QColor>
#include <QJsonObject>
#include <QVariant>

#include <optional>

namespace NereusSDR {

class MeterWidget;
class RadioModel;

// The No TX stop's message, as LEDItem::txStopRequested carries it.
// From Thetis MeterManager.cs:21210 [v2.10.3.15]:
//   _console.StopAllTx("Led Indicator stopped MOX");
inline constexpr char kLedNoTxStopMessage[] = "Led Indicator stopped MOX";
// The warning the user sees for that stop (MainWindow passes it to the
// stop), in NereusSDR's wording for a transmit stop.
inline constexpr char kLedNoTxStopUserText[] = "An LED stopped transmitting.";

// LED update interval range.
// From Thetis setup.designer.cs:63735-63744 [v2.10.3.15] (nudLedIndicator_UpdateInterval)
constexpr int kLedMinUpdateIntervalMs = 50;
constexpr int kLedMaxUpdateIntervalMs = 10000;
// From Thetis MeterManager.cs:27446 [v2.10.3.15]: an interval under 50 ms
// becomes 100 ms.
constexpr int kLedRaisedUpdateIntervalMs = 100;
// Offset and size range. From Thetis setup.designer.cs:63894-63912 and
// 63956-63974 [v2.10.3.15] (nudLedIndicator_xSize / xOffset: -10..10, step 0.001)
constexpr double kLedMinOffsetSize = -10.0;
constexpr double kLedMaxOffsetSize = 10.0;
// Panel padding range. From Thetis setup.designer.cs:63987-64005 [v2.10.3.15]
// (nudLedIndicator_PanelPadding: 0.001..10, step 0.001)
constexpr double kLedMinPanelPadding = 0.001;
constexpr double kLedMaxPanelPadding = 10.0;
// The fill look's padding, in pixels, of an LED made before the options.
constexpr float kLedFillDefaultPadding = 2.0f;
// renderLed transition speeds. From Thetis MeterManager.cs:34050 [v2.10.3.15]
// (int intervalSpeed = 250) and :34058, :34073 (intervalSpeed = 100 on the
// blink edge and the blink drop).
constexpr int kLedFadeSpeedMs = 250;
constexpr int kLedBlinkSpeedMs = 100;

// From Thetis clsLed (MeterManager.cs:19448+)
// LED indicator with 3 shapes, 2 styles, blink/pulsate animations.
//
// Two looks:
// * Fill (a record saved before the options, and the NereusSDR look): the
//   light fills the item inset by padding pixels; blink toggles every five
//   paints and pulsate cycles the alpha.
// * Placed (new items; Thetis renderLed, MeterManager.cs:34010-34130
//   [v2.10.3.15]): a panel width x padding tall in the RX or TX panel
//   colour, and the light at offset x width, size x width inside it,
//   fading between the false and true colours.
// Show true / show false apply to both looks. NereusSDR's green / amber /
// red thresholds stay in both (the true colour is amber or red above those
// thresholds).
class LEDItem : public MeterItem {
    Q_OBJECT

public:
    // From Thetis clsLed LedShape enum
    enum class LedShape { Square, Round, Triangle };
    // From Thetis clsLed LedStyle enum
    enum class LedStyle { Flat, ThreeD };
    // From Thetis MeterManager.cs:27462-27476 [v2.10.3.15]
    // (igs.IgnoreHistoryDuration 0 / 1 / 2). Derived from blink() and
    // pulsate(); blink wins when both are set, as in renderLed (:34065).
    enum class LightMode { OnOff = 0, Blink = 1, Pulsate = 2 };

    explicit LEDItem(QObject* parent = nullptr);

    void setLedShape(LedShape s) { m_shape = s; }
    LedShape ledShape() const { return m_shape; }

    void setLedStyle(LedStyle s) { m_style = s; }
    LedStyle ledStyle() const { return m_style; }

    void setTrueColour(const QColor& c) { m_trueColour = c; }
    QColor trueColour() const { return m_trueColour; }

    void setFalseColour(const QColor& c) { m_falseColour = c; }
    QColor falseColour() const { return m_falseColour; }

    void setPanelBackColour1(const QColor& c) { m_panelBack1 = c; }
    QColor panelBackColour1() const { return m_panelBack1; }

    void setPanelBackColour2(const QColor& c) { m_panelBack2 = c; }
    QColor panelBackColour2() const { return m_panelBack2; }

    void setShowBackPanel(bool show) { m_showBackPanel = show; }
    bool showBackPanel() const { return m_showBackPanel; }

    void setBlink(bool b) { m_blink = b; }
    bool blink() const { return m_blink; }

    void setPulsate(bool p) { m_pulsate = p; }
    bool pulsate() const { return m_pulsate; }

    void setLightMode(LightMode mode);
    LightMode lightMode() const;

    // Fill look: pixels inset on each side. Placed look: the panel height
    // as a ratio of the width.
    void setPadding(float p) { m_padding = p; }
    float padding() const { return m_padding; }

    // Placed look on / off (absent in a saved record = off, the fill look).
    void setPlaced(bool on) { m_placed = on; }
    bool placed() const { return m_placed; }

    // Placed look: the light's centre and size as ratios of the width.
    void setOffsetX(float v) { m_offsetX = v; }
    float offsetX() const { return m_offsetX; }
    void setOffsetY(float v) { m_offsetY = v; }
    float offsetY() const { return m_offsetY; }
    void setSizeX(float v) { m_sizeX = v; }
    float sizeX() const { return m_sizeX; }
    void setSizeY(float v) { m_sizeY = v; }
    float sizeY() const { return m_sizeY; }

    // Draw the light while true / while false (Thetis ShowTrue / ShowFalse).
    void setShowTrue(bool on) { m_showTrue = on; }
    bool showTrue() const { return m_showTrue; }
    void setShowFalse(bool on) { m_showFalse = on; }
    bool showFalse() const { return m_showFalse; }

    // Thetis UpdateAlways (key led_process_when_hidden): keep evaluating
    // while the container is hidden. Readings already reach a hidden
    // container's items, so this is kept for the condition and No TX.
    void setProcessWhenHidden(bool on);
    bool processWhenHidden() const { return m_processWhenHidden; }

    // No TX (Thetis NoTxTrue, key led_notx_true): while this window's
    // radio transmits, the LED turning true asks for a stop. Thetis
    // checks it on every LED update (MeterManager.cs:21192-21195
    // [v2.10.3.15]) and StopAllTx ends the key, so it stops once; here
    // txStopRequested is emitted once per transition of (No TX on, true,
    // transmitting, processed) to all four. An entry bound to another
    // session's radio never asks (effectiveTransmitting), and neither
    // does a hidden meter's LED unless processWhenHidden is on (Thetis
    // updates a meter only while it is enabled or UpdateAlways,
    // MeterManager.cs:2393 [v2.10.3.15]), nor an LED whose condition has
    // an error (Thetis runs Update only while _valid, :21179).
    void setNoTx(bool on);
    bool noTx() const { return m_noTx; }
    // No TX when false (Thetis NoTxFalse, key led_notx_false,
    // MeterManager.cs:21196-21199 [v2.10.3.15]): the same, for the LED
    // turning false.
    void setNoTxFalse(bool on);
    bool noTxFalse() const { return m_noTxFalse; }
    // False for a copy that only shows the entry (ContentRenderMode
    // Preview or Validation): it never asks for a stop. True by default.
    void setStopEnabled(bool enabled);
    bool stopEnabled() const { return m_stopEnabled; }

    // Threshold-based mode: value >= threshold activates that color
    void setGreenThreshold(double t) { m_greenThreshold = t; }
    double greenThreshold() const { return m_greenThreshold; }
    void setAmberThreshold(double t) { m_amberThreshold = t; }
    double amberThreshold() const { return m_amberThreshold; }
    void setRedThreshold(double t) { m_redThreshold = t; }
    double redThreshold() const { return m_redThreshold; }

    // The LED's true state (Thetis ConditionResult): the condition's
    // result when one is set (false while it has an error), else the
    // reading at or above a threshold (the state in which a fill-look LED
    // is lit).
    bool isTrue() const { return m_isTrue; }

    // The condition (Thetis LED condition, MeterManager.cs:21066-21100
    // [v2.10.3.15]), in the language of core/LedConditionExpression.h, not
    // C#. Empty: the thresholds decide, as before. Set: the condition
    // alone decides, and the light shows the true and false colours
    // (amber and red belong to the thresholds).
    void setCondition(const QString& expression);
    QString condition() const { return m_condition; }
    bool hasCondition() const;
    // Why the last evaluation failed, or empty. The LED is false while
    // this is set.
    QString conditionError() const { return m_conditionError; }
    // The value of %name%: a Thetis reading name from the entry's slice
    // readings (the hosting meter's latest values), a clock value
    // (time_utc, time_loc, date_utc, date_loc), or a meter data source
    // value written <4 character ID>.<value name>. std::nullopt for an
    // unknown name; an invalid QVariant for a known name with no value yet.
    std::optional<QVariant> conditionValue(const QString& name) const;
    // Checks an expression for the editor. A known name with no value yet
    // stands in as 0 (empty text for a clock value), so only mistakes show.
    LedConditionResult checkCondition(const QString& expression) const;
    // Works the condition out now; true when the true state or the error
    // changed. Readings, the update interval in advanceMeter and
    // setCondition call it.
    bool evaluateCondition();

    // Placed look runtime state (not saved): Thetis ColorFade 0..1 between
    // the false and true colours, and the panel flip fade 48..255.
    float colourFade() const { return m_colourFade; }
    int panelFadeValue() const { return m_panelFadeValue; }
    // The light colour the placed look draws now.
    QColor placedLightColour() const;
    // The panel colour the placed look draws now, alpha = the flip fade.
    QColor placedPanelColour() const;
    // Placed look rectangles, in pixels. The panel is clipped to the
    // item's height.
    QRect placedPanelRect(int widgetW, int widgetH) const;
    QRectF placedLightRect(int widgetW, int widgetH) const;

    // From Thetis MeterManager.cs:27446 [v2.10.3.15]: 1-49 ms becomes
    // 100 ms; 0 (a saved item without an interval) stays every poll.
    void setUpdateIntervalMs(int ms) override;
    void setValue(double v) override;
    void setTransmitting(bool tx) override;
    bool advanceMeter(qint64 monotonicMs) override;
    void applyNewItemDefaults() override;

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

signals:
    // No TX: stop transmitting on the radio this LED's entry belongs to,
    // with `message` as the warning. MainWindow forwards it after the
    // session check (ledStopAppliesToSession, stopTransmitForLed).
    void txStopRequested(const QString& message);

private:
    // Emits txStopRequested on the No TX rising edge; called wherever the
    // true state, transmit state or No TX changes.
    void checkNoTx();
    QColor currentColor() const;
    // The colour the light shows when fully true: the true colour, or
    // amber / red above those thresholds.
    QColor trueEndColour() const;
    bool effectiveTransmitting() const { return transmitting() && !foreignSession(); }
    // Thetis do_cover_fade (MeterManager.cs:34019 [v2.10.3.15]).
    bool coverFadeActive() const;
    // Restarts the panel fade at 48 when the transmit state changed since
    // the last flip (MeterManager.cs:34020-34026 [v2.10.3.15]).
    bool checkPanelFlip();
    // One placed-look frame of renderLed; returns true when anything moved.
    bool stepPlaced(int updateIntervalMs);
    void paintFill(QPainter& p, int widgetW, int widgetH);
    void paintPlaced(QPainter& p, int widgetW, int widgetH);
    void drawShape(QPainter& p, const QRect& ledRect, const QColor& color);
    MeterWidget* hostMeter() const;
    // Evaluates the condition once per update interval.
    bool stepCondition(qint64 monotonicMs);

    LedShape m_shape{LedShape::Round};
    LedStyle m_style{LedStyle::Flat};
    QColor   m_trueColour{0x00, 0xff, 0x88};   // green
    QColor   m_falseColour{0x40, 0x40, 0x40};   // grey (off)
    QColor   m_panelBack1{0x20, 0x20, 0x20};
    QColor   m_panelBack2{0x20, 0x20, 0x20};
    bool     m_showBackPanel{false};
    bool     m_blink{false};
    bool     m_pulsate{false};
    float    m_padding{kLedFillDefaultPadding};

    // Appended options. Absent in a saved record: these values.
    bool     m_placed{false};
    // From Thetis MeterManager.cs:20841-20846 [v2.10.3.15]
    float    m_offsetX{0.5f};
    float    m_offsetY{0.05f};
    float    m_sizeX{0.05f};
    float    m_sizeY{0.05f};
    // From Thetis MeterManager.cs:20872-20873 [v2.10.3.15]
    bool     m_showTrue{true};
    bool     m_showFalse{true};
    bool     m_processWhenHidden{false};
    // Appended after the options (base64 UTF-8); absent = empty.
    QString  m_condition;
    QString  m_conditionError;
    qint64   m_lastConditionMs{-1};
    // Appended after the condition; absent = off. Thetis's constructor
    // (MeterManager.cs:20854-20855 [v2.10.3.15]) reads
    //   _notxtrue = false;
    //   _notxtrue = false;
    // setting _notxtrue twice and never _notxfalse, an upstream slip that
    // C#'s default of false hides; both are initialised here.
    bool     m_noTx{false};
    bool     m_noTxFalse{false};
    // Runtime (not saved): a preview copy's false, and each No TX state
    // last checked, so a stop is asked once per transition.
    bool     m_stopEnabled{true};
    bool     m_noTxLatched{false};
    bool     m_noTxFalseLatched{false};

    // Threshold-based state
    double m_greenThreshold{0.0};
    double m_amberThreshold{1000.0};  // disabled by default (above any real value)
    double m_redThreshold{1000.0};
    QColor m_amberColour{0xff, 0xb8, 0x00};
    QColor m_redColour{0xff, 0x44, 0x44};
    bool   m_isTrue{false};

    // Animation state (fill look)
    bool  m_ledOn{false};
    int   m_blinkCounter{0};
    float m_pulsateAlpha{1.0f};
    float m_pulsateDir{-0.05f};  // alpha change per frame

    // Animation state (placed look): Thetis ColorFade, PulsateUp,
    // BlinkCount, OldConditionResult, FadeValue and MOXFadeToggle.
    float  m_colourFade{0.0f};
    // Thetis starts _pulsate_up = true (MeterManager.cs:20865 [v2.10.3.15]),
    // which lights a false blink or pulsate LED once when it is created.
    // NereusSDR starts it false so a false LED stays dark.
    bool   m_pulsateUp{false};
    int    m_blinkCount{0};
    bool   m_oldIsTrue{false};
    int    m_panelFadeValue{255};
    bool   m_moxFadeToggle{false};
    qint64 m_lastStepMs{-1};
};

// New LEDs (makeEntry) take Thetis's LED.
// From Thetis MeterManager.cs:20825-20873 [v2.10.3.15] (clsLed constructor).
// NereusSDR's green / amber / red thresholds keep their defaults.
struct LedItemThetisDefaults {
    bool     placed;
    QRgb     trueColour;
    QRgb     falseColour;
    QRgb     panelBack1;
    QRgb     panelBack2;
    bool     showBackPanel;
    float    offsetX;
    float    offsetY;
    float    sizeX;
    float    sizeY;
    float    padding;
    LEDItem::LedShape shape;
    LEDItem::LedStyle style;
    bool     blink;
    bool     pulsate;
    bool     showTrue;
    bool     showFalse;
    bool     processWhenHidden;
};
inline constexpr LedItemThetisDefaults kLedThetisDefaults{
    true,                       // renderLed placement (:34010-34130)
    qRgb(0, 255, 0),            // _true_colour = Color.FromArgb(0, 255, 0) (:20836)
    qRgb(192, 192, 192),        // _false_colour = Color.FromArgb(192, 192, 192) (:20837)
    qRgb(32, 32, 32),           // _panel_back_colour_1 = Color.FromArgb(32, 32, 32) (:20838)
    qRgb(32, 32, 32),           // _panel_back_colour_2 = Color.FromArgb(32, 32, 32) (:20839)
    true,                       // _show_back_panel = true (:20840)
    0.5f,                       // _x_offset = 0.5f (:20841)
    0.05f,                      // _y_offset = 0.05f (:20842)
    0.05f,                      // _x_size = 0.05f (:20845)
    0.05f,                      // _y_size = 0.05f (:20846)
    0.1f,                       // _padding = 0.1f (:20847)
    LEDItem::LedShape::Square,  // _led_shape = Led_Shape.SQUARE (:20848)
    LEDItem::LedStyle::Flat,    // _led_style = Led_Style.FLAT (:20849)
    false,                      // _blink = false (:20867)
    false,                      // _pulsate = false (:20866)
    true,                       // _show_true = true (:20872)
    true,                       // _show_false = true (:20873)
    false,                      // led_process_when_hidden default false (:27492)
};

// Container item parity Task 27: an LED's stop applies to this window's
// radio only when the entry's context names no session or names this
// window's (MainWindow::containerSessionId()). An entry bound to another
// session's radio follows that session, never this window.
bool ledStopAppliesToSession(const QJsonObject& entryContext, const QString& currentSessionId);

// How stopTransmitForLed reached the radio.
enum class LedStopRoute {
    None,    // a remote window that cannot reach the Core's transmit
    Local,   // RadioModel::stopAllTx on this process's radio
    Remote,  // this window's MOX, TUNE and two-tone released at the Core
};

// Forwards an LED's stop to `radio`. Only ever stops; nothing keys.
// * A local radio: RadioModel::stopAllTx(message), the port of Thetis
//   StopAllTx (MOX, manual MOX, TUNE and two-tone off, and the warning).
// * A remote window: the Core takes no stop-all verb from a window, so the
//   stop goes through the session's own release, the MOX button's off
//   (RadioModel::setMoxFromButton(false)): TUNE and two-tone off, then
//   this device's key (screen, program or VOX) released at the Core. The
//   Core refuses to end another device's key, by its holder rule.
//   Without remote transmit to the Core nothing is sent (None).
LedStopRoute stopTransmitForLed(RadioModel& radio, const QString& message);

} // namespace NereusSDR
