// =================================================================
// src/gui/meters/ItemColourLook.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. The rows name the Thetis default
// colours the container items already carry, by the items' own cited
// constants; no Thetis code is ported here.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Written by J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-26 - Contrast round: the dial's pad, ring and off zones, the
//                 off LED, the rotator beam, the button press and the filter
//                 setting-on fill take roles that reach 3:1 / 4.5:1; derived().
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/meters/ItemColourLook.h"

#include "gui/meters/ButtonBoxItem.h"
#include "gui/meters/ClockItem.h"
#include "gui/meters/DialItem.h"
#include "gui/meters/FadeCoverItem.h"
#include "gui/meters/FilterDisplayItem.h"
#include "gui/meters/HistoryGraphItem.h"
#include "gui/meters/LEDItem.h"
#include "gui/meters/MagicEyeItem.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/NeedleScalePwrItem.h"
#include "gui/meters/NereusBandButtonItem.h"
#include "gui/meters/RotatorItem.h"
#include "gui/meters/SignalTextItem.h"
#include "gui/meters/SpacerItem.h"
#include "gui/meters/TextOverlayItem.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/meters/WaveformMeterItem.h"
#include "gui/meters/WebImageItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/presets/CompositePresetItem.h"
#include "gui/styles/WorkspacePreview.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMutex>
#include <QPushButton>
#include <QSet>

Q_LOGGING_CATEGORY(lcItemColourLook, "nereus.gui.itemcolourlook")

namespace NereusSDR::ItemColourLook {

namespace {

// The table. Each row: the field, one default it holds, the palette role
// (WorkspacePalette.h, Longpath's "Glass and Depth" roles) or nullptr with
// the reason the classic colour stays. A field may have several defaults:
// Thetis's (new items, each item's cited constant) and NereusSDR's earlier
// starting colours (saved items keep them; the member initialisers).
//
// The roles keep the colour's meaning, after Longpath's house rules (warm is
// measured, blue is touchable, green is on, red is transmit or danger):
//   readings, needles, bar fills, markers and history trails -> measured
//   over-range and peak markers -> danger; transmit -> tx; receive -> live
//   main ink -> text; labels and units -> text-secondary; scales -> text-scale
//   wells behind readouts -> inset; plain panels -> panel (both backdrops)
//   buttons -> raised-top, button-hover, border; chosen -> selected-top
constexpr Row kRows[] = {
    // ── Container chrome (ContainerWidget) ─────────────────────────────
    // ContainerWidget::kLegacyContentBackground, NereusSDR's container
    // background; Thetis's container back colour is the same dark fill.
    {kContainerBackground, qRgb(0x0f, 0x0f, 0x1a), "panel", true, nullptr},

    // ── SOLID (SolidColourItem) ────────────────────────────────────────
    // From Thetis MeterManager.cs:7663 [v2.10.3.15] (_colour = Color.Black)
    {"Solid.colour", qRgb(0, 0, 0), "panel", true, nullptr},
    // NereusSDR's earlier SOLID default (MeterItem.h m_colour)
    {"Solid.colour", qRgb(0x0f, 0x0f, 0x1a), "panel", true, nullptr},

    // ── BAR (BarItem) ──────────────────────────────────────────────────
    // kBarItemThetisDefaults, From Thetis MeterManager.cs:21284-21302 [v2.10.3.15]
    {"Bar.barColor", kBarItemThetisDefaults.colour, "measured", false, nullptr},
    {"Bar.barRedColor", kBarItemThetisDefaults.colourHigh, "danger", false, nullptr},
    {"Bar.markerColour", kBarItemThetisDefaults.markerColour, "measured", false, nullptr},
    {"Bar.peakHoldMarkerColour", kBarItemThetisDefaults.peakHoldMarkerColour, "danger", false, nullptr},
    {"Bar.peakFontColour", kBarItemThetisDefaults.peakValueColour, "danger", false, nullptr},
    // NereusSDR's earlier BAR colours (MeterItem.h member initialisers)
    {"Bar.barColor", qRgb(0x00, 0xb4, 0xd8), "measured", false, nullptr},
    {"Bar.barRedColor", qRgb(0xff, 0x44, 0x44), "danger", false, nullptr},
    {"Bar.edgeBackgroundColor", qRgb(0x00, 0x00, 0x00), "inset", false, nullptr},
    {"Bar.edgeLowColor", qRgb(0xff, 0xff, 0xff), "text", false, nullptr},
    {"Bar.edgeHighColor", qRgb(0xff, 0x00, 0x00), "danger", false, nullptr},
    {"Bar.edgeAvgColor", qRgb(0xff, 0xff, 0x00), "measured", false, nullptr},
    {"Bar.fontColour", qRgb(0xff, 0xff, 0x00), "measured", false, nullptr},
    {"Bar.peakFontColour", qRgb(0xff, 0xff, 0x00), "measured", false, nullptr},
    {"Bar.markerColour", qRgb(0xff, 0xff, 0x00), "measured", false, nullptr},
    // Thetis Red(128), the clsMeterItem history colour (MeterItem.h m_historyColour)
    {"Bar.historyColour", qRgba(0xff, 0x00, 0x00, 128), "measured", false, nullptr},

    // ── SCALE (ScaleItem) ──────────────────────────────────────────────
    // kScaleThetisDefaults, From Thetis MeterManager.cs:16118-16140 [v2.10.3.15]
    {"Scale.lowColour", kScaleThetisDefaults.lowColour, "text-scale", false, nullptr},
    {"Scale.highColour", kScaleThetisDefaults.highColour, "gauge-danger", false, nullptr},
    {"Scale.titleColour", kScaleThetisDefaults.titleColour, "text-secondary", false, nullptr},
    // NereusSDR's earlier SCALE colours (MeterItem.h member initialisers)
    {"Scale.tickColor", qRgb(0xc8, 0xd8, 0xe8), "text-scale", false, nullptr},
    {"Scale.labelColor", qRgb(0x80, 0x90, 0xa0), "text-scale", false, nullptr},
    {"Scale.titleColour", qRgb(0xff, 0x00, 0x00), "text-secondary", false, nullptr},
    {"Scale.lowColour", qRgb(0xc8, 0xd8, 0xe8), "text-scale", false, nullptr},
    {"Scale.highColour", qRgb(0xff, 0x80, 0x40), "gauge-danger", false, nullptr},

    // ── TEXT (TextItem) ────────────────────────────────────────────────
    // kTextThetisDefaults, From Thetis MeterManager.cs:22259-22267 [v2.10.3.15]
    {"Text.textColor", kTextThetisDefaults.colour, "text", false, nullptr},
    // NereusSDR's earlier TEXT colour (MeterItem.h m_textColor)
    {"Text.textColor", qRgb(0xc8, 0xd8, 0xe8), "text", false, nullptr},

    // ── NEEDLE (NeedleItem) ────────────────────────────────────────────
    // kNeedleThetisDefaults, From Thetis MeterManager.cs:21942-21973 [v2.10.3.15]
    {"Needle.needleColor", kNeedleThetisDefaults.colour, "measured", false, nullptr},
    {"Needle.peakHoldColour", kNeedleThetisDefaults.peakHoldColour, "danger", false, nullptr},
    {"Needle.historyColor", kNeedleThetisDefaults.historyColour, "measured", false, nullptr},
    // NereusSDR's earlier NEEDLE colours (MeterItem.h member initialisers)
    {"Needle.peakHoldColour", qRgb(0xff, 0xaa, 0x00), "danger", false, nullptr},
    {"Needle.historyColor", qRgba(0, 0, 0, 0), nullptr, false, "transparent: no history trail is drawn"},

    // ── NEEDLESCALEPWR (NeedleScalePwrItem) ────────────────────────────
    // kNeedleScalePwrThetisDefaults, From Thetis MeterManager.cs:16229-16260 [v2.10.3.15]
    {"PowerScale.lowColour", kNeedleScalePwrThetisDefaults.lowColour, "text-scale", false, nullptr},
    {"PowerScale.highColour", kNeedleScalePwrThetisDefaults.highColour, "gauge-danger", false, nullptr},
    // NereusSDR's earlier power scale colours (NeedleScalePwrItem.h)
    {"PowerScale.lowColour", qRgb(0x80, 0x80, 0x80), "text-scale", false, nullptr},

    // ── SPACER (SpacerItem) ────────────────────────────────────────────
    // kSpacerThetisDefaults, From Thetis MeterManager.cs:17443-17444 [v2.10.3.15]
    {"Spacer.colour1", kSpacerThetisDefaults.colour1, "panel", true, nullptr},
    {"Spacer.colour2", kSpacerThetisDefaults.colour2, "panel", true, nullptr},

    // ── FADECOVER (FadeCoverItem) ──────────────────────────────────────
    // FadeCoverItem.h colour1 / colour2, Thetis's (32, 32, 32) back colour
    {"FadeCover.colour1", qRgb(0x20, 0x20, 0x20), "panel", true, nullptr},
    {"FadeCover.colour2", qRgb(0x20, 0x20, 0x20), "panel", true, nullptr},

    // ── LED (LEDItem) ──────────────────────────────────────────────────
    // kLedThetisDefaults, From Thetis MeterManager.cs:20836-20839 [v2.10.3.15]
    {"Led.trueColour", kLedThetisDefaults.trueColour, "live", false, nullptr},
    {"Led.falseColour", kLedThetisDefaults.falseColour, "text-scale", false, nullptr},
    {"Led.panelBackColour1", kLedThetisDefaults.panelBack1, "panel", true, nullptr},
    {"Led.panelBackColour2", kLedThetisDefaults.panelBack2, "panel", true, nullptr},
    // NereusSDR's earlier LED colours and its threshold colours (LEDItem.h)
    {"Led.trueColour", qRgb(0x00, 0xff, 0x88), "live", false, nullptr},
    {"Led.falseColour", qRgb(0x40, 0x40, 0x40), "text-scale", false, nullptr},
    {"Led.amberColour", qRgb(0xff, 0xb8, 0x00), "measured", false, nullptr},
    {"Led.redColour", qRgb(0xff, 0x44, 0x44), "tx", false, nullptr},

    // ── HISTORY (HistoryGraphItem) ─────────────────────────────────────
    // kHistoryThetisDefaults, From Thetis MeterManager.cs:17528-17555 [v2.10.3.15]
    {"History.backColour", kHistoryThetisDefaults.backColour, "panel", true, nullptr},
    {"History.lineColor0", kHistoryThetisDefaults.axis0Colour, "measured", false, nullptr},
    {"History.lineColor1", kHistoryThetisDefaults.axis1Colour, "dsp-on", false, nullptr},
    {"History.linesColour", kHistoryThetisDefaults.linesColour, "spectrum-grid", false, nullptr},
    {"History.timeColour", kHistoryThetisDefaults.timeColour, "text-secondary", false, nullptr},
    // NereusSDR's earlier history colours (HistoryGraphItem.h)
    {"History.lineColor0", qRgb(0x00, 0xb4, 0xd8), "measured", false, nullptr},
    {"History.lineColor1", qRgb(0xff, 0xb8, 0x00), "dsp-on", false, nullptr},
    {"History.backColour", qRgb(0x0a, 0x0a, 0x18), "panel", true, nullptr},
    {"History.linesColour", qRgb(0x20, 0x30, 0x40), "border", false, nullptr},
    {"History.timeColour", qRgb(0x80, 0x90, 0xa0), "text-secondary", false, nullptr},

    // ── MAGICEYE (MagicEyeItem) ────────────────────────────────────────
    // kMagicEyeThetisDefaults, From Thetis MeterManager.cs:23588 [v2.10.3.15] and :6763
    {"MagicEye.glowColor", kMagicEyeThetisDefaults.colour, "live", false, nullptr},
    {"MagicEye.historyColour", kMagicEyeThetisDefaults.historyColour, "measured", false, nullptr},
    // NereusSDR's earlier eye glow (MagicEyeItem.h)
    {"MagicEye.glowColor", qRgb(0x00, 0xff, 0x88), "live", false, nullptr},

    // ── SIGNALTEXT (SignalTextItem) ────────────────────────────────────
    // kSignalTextThetisDefaults, From Thetis MeterManager.cs:23001-23062, 21644-21676 [v2.10.3.15]
    {"SignalText.colour", kSignalTextThetisDefaults.fontColour, "measured", false, nullptr},
    {"SignalText.peakValueColour", kSignalTextThetisDefaults.peakValueColour, "danger", false, nullptr},
    {"SignalText.typeColour", kSignalTextThetisDefaults.typeColour, "text-secondary", false, nullptr},
    {"SignalText.subMarkerColour", kSignalTextThetisDefaults.subMarkerColour, "text-secondary", false, nullptr},
    {"SignalText.backgroundColour", kSignalTextThetisDefaults.backgroundColour, "panel", true, nullptr},
    // NereusSDR's earlier signal text colours (SignalTextItem.h, after Thetis :20333-20335)
    {"SignalText.colour", qRgb(0xff, 0x00, 0x00), "measured", false, nullptr},
    {"SignalText.markerColour", qRgb(0xff, 0xff, 0x00), "measured", false, nullptr},
    {"SignalText.typeColour", qRgb(0x80, 0x90, 0xa0), "text-secondary", false, nullptr},
    {"SignalText.backgroundColour", qRgb(0x0a, 0x0a, 0x18), "panel", true, nullptr},

    // ── DIAL (DialItem) ────────────────────────────────────────────────
    // kDialThetisDefaults, From Thetis MeterManager.cs:16797-16807 [v2.10.3.15]
    {"Dial.textColour", kDialThetisDefaults.textColour, "text", false, nullptr},
    {"Dial.circleColour", kDialThetisDefaults.circleColour, "inset", false, nullptr},
    {"Dial.padColour", kDialThetisDefaults.padColour, "accent", false, nullptr},
    {"Dial.padPressedColour", kDialThetisDefaults.padPressedColour, "selected-top", false, nullptr},
    {"Dial.buttonOnColour", kDialThetisDefaults.buttonOnColour, "selected-top", false, nullptr},
    {"Dial.buttonOffColour", kDialThetisDefaults.buttonOffColour, "panel", true, nullptr},
    {"Dial.buttonHighlightColour", kDialThetisDefaults.buttonHighlightColour, "button-hover", false, nullptr},
    {"Dial.ringColour", kDialThetisDefaults.ringColour, "text-scale", false, nullptr},
    {"Dial.slowColour", kDialThetisDefaults.slowColour, "accent", false, nullptr},
    {"Dial.holdColour", kDialThetisDefaults.holdColour, "ok", false, nullptr},
    {"Dial.fastColour", kDialThetisDefaults.fastColour, "danger", false, nullptr},
    // NereusSDR's earlier dial colours (DialItem.h member initialisers)
    {"Dial.textColour", qRgb(0xc8, 0xd8, 0xe8), "text", false, nullptr},
    {"Dial.circleColour", qRgb(0x1a, 0x2a, 0x3a), "inset", false, nullptr},
    {"Dial.padColour", qRgb(0x15, 0x20, 0x30), "accent", false, nullptr},
    {"Dial.ringColour", qRgb(0x40, 0x50, 0x60), "text-scale", false, nullptr},
    {"Dial.buttonOnColour", qRgb(0x00, 0x70, 0xc0), "selected-top", false, nullptr},
    {"Dial.buttonOffColour", qRgb(0x30, 0x40, 0x50), "panel", true, nullptr},
    {"Dial.buttonHighlightColour", qRgb(0x00, 0x90, 0xe0), "button-hover", false, nullptr},
    {"Dial.slowColour", qRgb(0x00, 0xff, 0x88), "ok", false, nullptr},
    {"Dial.holdColour", qRgb(0xff, 0xb8, 0x00), "measured", false, nullptr},
    {"Dial.fastColour", qRgb(0xff, 0x44, 0x44), "danger", false, nullptr},

    // ── TEXTOVERLAY (TextOverlayItem) ──────────────────────────────────
    // kTextOverlayThetisDefaults, From Thetis MeterManager.cs:20138-20143 [v2.10.3.15]
    {"TextOverlay.textColour1", kTextOverlayThetisDefaults.textColour1, "text", false, nullptr},
    {"TextOverlay.textColour2", kTextOverlayThetisDefaults.textColour2, "text", false, nullptr},
    {"TextOverlay.textBackColour1", kTextOverlayThetisDefaults.textBackColour1, "raised-top", false, nullptr},
    {"TextOverlay.textBackColour2", kTextOverlayThetisDefaults.textBackColour2, "raised-top", false, nullptr},
    {"TextOverlay.panelBackColour1", kTextOverlayThetisDefaults.panelBackColour1, "panel", true, nullptr},
    {"TextOverlay.panelBackColour2", kTextOverlayThetisDefaults.panelBackColour2, "panel", true, nullptr},

    // ── WEBIMAGE (WebImageItem) ────────────────────────────────────────
    // WebImageItem.h m_fallbackColor, Thetis's (32, 32, 32) back colour
    {"WebImage.fallbackColor", qRgb(0x20, 0x20, 0x20), "panel", true, nullptr},

    // ── FILTERDISPLAY (FilterDisplayItem) ──────────────────────────────
    // kFilterDisplayThetisDefaults, From Thetis MeterManager.cs:27651-27705, 18316-18350 [v2.10.3.15]
    {"FilterDisplay.dataLineColour", kFilterDisplayThetisDefaults.dataLineColour, "dsp-on", false, nullptr},
    {"FilterDisplay.waterfallLowColour", kFilterDisplayThetisDefaults.waterfallLowColour, nullptr, false,
     "the bottom of the waterfall's colour scale: a data colour, like the pan waterfall's"},
    {"FilterDisplay.textColour", kFilterDisplayThetisDefaults.textColour, "text", false, nullptr},
    {"FilterDisplay.numberHighlightColour", kFilterDisplayThetisDefaults.numberHighlightColour, "danger-bg", false, nullptr},
    {"FilterDisplay.edgesColourRX", kFilterDisplayThetisDefaults.edgesColourRx, "measured", false, nullptr},
    {"FilterDisplay.edgesColourTX", kFilterDisplayThetisDefaults.edgesColourTx, "tx", false, nullptr},
    {"FilterDisplay.edgeHighlightColour", kFilterDisplayThetisDefaults.edgeHighlightColour, "text", false, nullptr},
    {"FilterDisplay.meterBackColour", kFilterDisplayThetisDefaults.meterBackColour, "panel", true, nullptr},
    {"FilterDisplay.notchColour", kFilterDisplayThetisDefaults.notchColour, "warn", false, nullptr},
    {"FilterDisplay.notchHighlightColour", kFilterDisplayThetisDefaults.notchHighlightColour, "ok", false, nullptr},
    {"FilterDisplay.extentsColour", kFilterDisplayThetisDefaults.extentsColour, "text-scale", false, nullptr},
    {"FilterDisplay.snapLineColour", kFilterDisplayThetisDefaults.snapLineColour, "text-scale", false, nullptr},
    {"FilterDisplay.settingOnColour", kFilterDisplayThetisDefaults.settingOnColour, "selected-top", false, nullptr},
    {"FilterDisplay.buttonHighlightColour", kFilterDisplayThetisDefaults.buttonHighlightColour, "button-hover", false, nullptr},
    // FilterDisplayItem.h m_dataFillColour, LimeGreen at alpha 0x40 (NereusSDR)
    {"FilterDisplay.dataFillColour", qRgba(0x32, 0xcd, 0x32, 0x40), "dsp-on", false, nullptr},
    // NereusSDR's earlier notch colour, Orange (FilterDisplayItem.h)
    {"FilterDisplay.notchColour", qRgb(0xff, 0xa5, 0x00), "warn", false, nullptr},

    // ── ROTATOR (RotatorItem) ──────────────────────────────────────────
    // RotatorItem.h, from Thetis clsRotatorItem MeterManager.cs:15100-15106 [v2.10.3.15]
    {"Rotator.bigBlobColour", qRgb(0xff, 0x00, 0x00), "measured", false, nullptr},
    {"Rotator.smallBlobColour", qRgb(0xff, 0xff, 0xff), "text", false, nullptr},
    {"Rotator.outerTextColour", qRgb(0x80, 0x80, 0x80), "text-secondary", false, nullptr},
    {"Rotator.arrowColour", qRgb(0xff, 0xff, 0xff), "text", false, nullptr},
    {"Rotator.beamWidthColour", qRgb(0x40, 0x40, 0x40), "text-secondary", false, nullptr},
    {"Rotator.backgroundColour", qRgb(0x20, 0x20, 0x20), "panel", true, nullptr},
    // RotatorItem.h m_controlColour, LimeGreen
    {"Rotator.controlColour", qRgb(0x32, 0xcd, 0x32), "ok", false, nullptr},

    // ── VFO (VfoDisplayItem) ───────────────────────────────────────────
    // kVfoDisplayThetisDefaults, From Thetis MeterManager.cs:14309-14347, 25768 [v2.10.3.15]
    {"VfoDisplay.frequencyColour", kVfoDisplayThetisDefaults.frequencyColour, "measured", false, nullptr},
    {"VfoDisplay.frequencySmallColour", kVfoDisplayThetisDefaults.frequencySmallColour, "measured", false, nullptr},
    {"VfoDisplay.modeColour", kVfoDisplayThetisDefaults.modeColour, "text-secondary", false, nullptr},
    {"VfoDisplay.filterColour", kVfoDisplayThetisDefaults.filterColour, "text-secondary", false, nullptr},
    {"VfoDisplay.titleColour", kVfoDisplayThetisDefaults.titleColour, "text-secondary", false, nullptr},
    {"VfoDisplay.splitBackColour", kVfoDisplayThetisDefaults.splitBackColour, "raised-top", false, nullptr},
    {"VfoDisplay.splitColour", kVfoDisplayThetisDefaults.splitColour, "warn", false, nullptr},
    {"VfoDisplay.rxColour", kVfoDisplayThetisDefaults.rxColour, "live", false, nullptr},
    {"VfoDisplay.txColour", kVfoDisplayThetisDefaults.txColour, "tx", false, nullptr},
    {"VfoDisplay.bandColour", kVfoDisplayThetisDefaults.bandColour, "text", false, nullptr},
    {"VfoDisplay.digitHighlightColour", kVfoDisplayThetisDefaults.digitHighlightColour, "button-hover", false, nullptr},
    {"VfoDisplay.bandTextColour", kVfoDisplayThetisDefaults.bandTextColour, "ok", false, nullptr},
    {"VfoDisplay.lockColour", kVfoDisplayThetisDefaults.lockColour, "ok", false, nullptr},
    {"VfoDisplay.syncColour", kVfoDisplayThetisDefaults.syncColour, "ok", false, nullptr},
    {"VfoDisplay.backgroundColour", kVfoDisplayThetisDefaults.backColour, "panel", true, nullptr},
    // NereusSDR's earlier VFO colours (VfoDisplayItem.h member initialisers)
    {"VfoDisplay.rxColour", qRgb(0x00, 0xff, 0x00), "live", false, nullptr},
    {"VfoDisplay.backgroundColour", qRgb(0x0a, 0x0a, 0x18), "panel", true, nullptr},

    // ── CLOCK (ClockItem) ──────────────────────────────────────────────
    // kClockThetisDefaults, From Thetis MeterManager.cs:15413-15415, 25825 [v2.10.3.15]
    {"Clock.timeColour", kClockThetisDefaults.timeColour, "measured", false, nullptr},
    {"Clock.dateColour", kClockThetisDefaults.dateColour, "text-secondary", false, nullptr},
    {"Clock.typeTitleColour", kClockThetisDefaults.typeTitleColour, "text-secondary", false, nullptr},
    {"Clock.backColour", kClockThetisDefaults.backColour, "panel", true, nullptr},
    // NereusSDR's earlier clock colours (ClockItem.h member initialisers)
    {"Clock.timeColour", qRgb(0xc8, 0xd8, 0xe8), "text", false, nullptr},
    {"Clock.dateColour", qRgb(0x80, 0x90, 0xa0), "text-secondary", false, nullptr},
    {"Clock.typeTitleColour", qRgb(0x70, 0x80, 0x90), "text-tertiary", false, nullptr},

    // ── Button grids (ButtonBoxItem: bands, modes, filters, steps,
    //    antennas, other, voice) ─────────────────────────────────────────
    // ButtonBoxItem.h ButtonState, NereusSDR's grid colours (Thetis's are
    // not ported; the grids save no colour)
    {"ButtonBox.fillColour", qRgb(0x1a, 0x2a, 0x3a), "raised-top", false, nullptr},
    {"ButtonBox.offColour", qRgb(0x1a, 0x2a, 0x3a), "raised-top", false, nullptr},
    {"ButtonBox.hoverColour", qRgb(0x20, 0x40, 0x60), "button-hover", false, nullptr},
    {"ButtonBox.clickColour", qRgb(0x00, 0xb4, 0xd8), "selected-top", false, nullptr},
    {"ButtonBox.borderColour", qRgb(0x20, 0x50, 0x70), "border", false, nullptr},
    {"ButtonBox.textColour", qRgb(0xc8, 0xd8, 0xe8), "text", false, nullptr},
    // The chosen button: blue (bands, modes, filters, steps, macros)
    {"ButtonBox.onColour", qRgb(0x00, 0x70, 0xc0), "selected-top", false, nullptr},
    // Engaged green (other buttons, voice PLAY)
    {"ButtonBox.onColour", qRgb(0x00, 0x60, 0x40), "ok-bg", false, nullptr},
    // Antennas (AntennaButtonItem.cpp): RX LimeGreen, Aux/XVTR Orange,
    // TX red, the toggle Yellow; voice REC red
    {"ButtonBox.onColour", qRgb(0x00, 0xff, 0x00), "ok-bg", false, nullptr},
    {"ButtonBox.onColour", qRgb(0xff, 0xa5, 0x00), "measured-bg", false, nullptr},
    {"ButtonBox.onColour", qRgb(0xff, 0x44, 0x44), "danger-bg", false, nullptr},
    {"ButtonBox.onColour", qRgb(0xff, 0xff, 0x00), "badge-warn-bg", false, nullptr},

    // ── NEREUSBAND (NereusBandButtonItem): NereusButtonFace.h ──────────
    // Already the workspace face in both looks: the same values as the roles.
    {"NereusBand.buttonTextColor", qRgb(0xdc, 0xdc, 0xe1), "text", false, nullptr},
    {"NereusBand.backgroundTopColor", qRgb(0x22, 0x22, 0x27), "raised-top", false, nullptr},
    {"NereusBand.backgroundBottomColor", qRgb(0x14, 0x14, 0x17), "raised-bottom", false, nullptr},
    {"NereusBand.selectedTopColor", qRgb(0x30, 0x62, 0x47), nullptr, false,
     "already the workspace face: the raised green of a checked processing button, which no palette role holds"},
    {"NereusBand.selectedBottomColor", qRgb(0x1d, 0x3d, 0x2c), nullptr, false,
     "already the workspace face: the raised green of a checked processing button, which no palette role holds"},

    // ── Waveform faces (WaveformMeterItem): the trace already holds the
    //    workspace trace colour (WaveformMeterItem.cpp, WaveformMeterStyle.h)
    {"Waveform.traceColor", qRgb(0x7f, 0xae, 0x91), "dsp-on", false, nullptr},

    // ── Bar faces (BarPresetItem: meter.signal ... meter.swr, mic, alc,
    //    custom, the S-meter bar) ────────────────────────────────────────
    // kBarPresetThetisDefaults and BarPresetColours, From Thetis
    // MeterManager.cs:22822-25375 [v2.10.3.15] (the Add*Bar builders), the
    // clsBarItem constructor :21284-21302 and the clsScaleItem constructor
    // :16118-16124. "barColor" is the marker (cb.MarkerColour).
    {"BarPreset.barColor", BarPresetColours::kYellow, "measured", false, nullptr},
    {"BarPreset.barColor", BarPresetColours::kOrange, "measured", false, nullptr},
    {"BarPreset.backdropColor", BarPresetColours::kBackground, "panel", true, nullptr},
    {"BarPreset.titleColor", BarPresetColours::kDarkGray, "text-secondary", false, nullptr},
    {"BarPreset.lowColor", BarPresetColours::kWhite, "text-scale", false, nullptr},
    {"BarPreset.highColor", BarPresetColours::kRed, "gauge-danger", false, nullptr},
    {"BarPreset.subColor", BarPresetColours::kDarkGray, "text-secondary", false, nullptr},
    {"BarPreset.peakHoldColor", BarPresetColours::kRed, "danger", false, nullptr},
    {"BarPreset.peakValueColor", BarPresetColours::kRed, "danger", false, nullptr},
    {"BarPreset.segLowColor", BarPresetColours::kCadetBlue, "measured", false, nullptr},
    {"BarPreset.segLowColor", BarPresetColours::kDarkCyan, "measured", false, nullptr},
    {"BarPreset.segLowColor", BarPresetColours::kRed, "measured", false, nullptr},
    {"BarPreset.segHighColor", BarPresetColours::kOrange, "danger", false, nullptr},
    // An unset low or high colour draws the flavour's fill (white on the
    // Mic, ALC and processor bars) or red.
    {"BarPreset.segLowColor", BarPresetColours::kWhite, "measured", false, nullptr},
    {"BarPreset.segHighColor", BarPresetColours::kRed, "danger", false, nullptr},
    // The history trail: each bar's own hue at alpha 128 in Thetis; one
    // measured trail with the look on (its alpha kept).
    {"BarPreset.historyColor", BarPresetColours::kRedHistory, "measured", false, nullptr},
    {"BarPreset.historyColor", BarPresetColours::kCornflowerHistory, "measured", false, nullptr},
    {"BarPreset.historyColor", BarPresetColours::kPurpleHistory, "measured", false, nullptr},
    {"BarPreset.historyColor", BarPresetColours::kLemonChiffonHistory, "measured", false, nullptr},
    {"BarPreset.historyColor", BarPresetColours::kPaleTurquoiseHistory, "measured", false, nullptr},
    {"BarPreset.historyColor", BarPresetColours::kPeachPuffHistory, "measured", false, nullptr},
    {"BarPreset.historyColor", BarPresetColours::kVioletHistory, "measured", false, nullptr},
    {"BarPreset.historyColor", BarPresetColours::kOrangeHistory, "measured", false, nullptr},

    // ── Composite faces (CompositePresetItem): "<kind>.<config key>" and
    //    "<kind>.<channel>.<key>". The starting values are
    //    CompositePresetItem::initialise() (#a9a9a9 title, #202020 backdrop,
    //    #c8d8e8 low, #ff4444 high) and the Thetis tables applied to new
    //    faces (kPowerSwrThetisDefaults, kMagicEyeThetisDefaults,
    //    kSignalTextThetisDefaults; the history face's axis colours From
    //    Thetis MeterManager.cs:17530-17531 [v2.10.3.15]).
    {"PowerSwrPreset.titleColor", qRgb(0xa9, 0xa9, 0xa9), "text-secondary", false, nullptr},
    {"PowerSwrPreset.backdropColor", qRgb(0x20, 0x20, 0x20), "panel", true, nullptr},
    {"PowerSwrPreset.lowColor", qRgb(0xc8, 0xd8, 0xe8), "text-scale", false, nullptr},
    {"PowerSwrPreset.highColor", qRgb(0xff, 0x44, 0x44), "danger", false, nullptr},
    {"MagicEyePreset.titleColor", qRgb(0xa9, 0xa9, 0xa9), "text-secondary", false, nullptr},
    {"MagicEyePreset.backdropColor", qRgb(0x20, 0x20, 0x20), "panel", true, nullptr},
    {"MagicEyePreset.lowColor", qRgb(0xc8, 0xd8, 0xe8), "text-scale", false, nullptr},
    {"MagicEyePreset.highColor", qRgb(0xff, 0x44, 0x44), "danger", false, nullptr},
    {"SignalTextPreset.titleColor", qRgb(0xa9, 0xa9, 0xa9), "text-secondary", false, nullptr},
    {"SignalTextPreset.backdropColor", qRgb(0x20, 0x20, 0x20), "panel", true, nullptr},
    {"SignalTextPreset.lowColor", qRgb(0xc8, 0xd8, 0xe8), "text-scale", false, nullptr},
    {"SignalTextPreset.highColor", qRgb(0xff, 0x44, 0x44), "danger", false, nullptr},
    {"HistoryGraphPreset.titleColor", qRgb(0xa9, 0xa9, 0xa9), "text-secondary", false, nullptr},
    {"HistoryGraphPreset.backdropColor", qRgb(0x20, 0x20, 0x20), "panel", true, nullptr},
    {"HistoryGraphPreset.lowColor", qRgb(0xc8, 0xd8, 0xe8), "text-scale", false, nullptr},
    {"HistoryGraphPreset.highColor", qRgb(0xff, 0x44, 0x44), "danger", false, nullptr},
    {"VfoDisplayPreset.titleColor", qRgb(0xa9, 0xa9, 0xa9), "text-secondary", false, nullptr},
    {"VfoDisplayPreset.backdropColor", qRgb(0x20, 0x20, 0x20), "panel", true, nullptr},
    {"VfoDisplayPreset.lowColor", qRgb(0xc8, 0xd8, 0xe8), "text-scale", false, nullptr},
    {"VfoDisplayPreset.highColor", qRgb(0xff, 0x44, 0x44), "danger", false, nullptr},
    {"ClockPreset.titleColor", qRgb(0xa9, 0xa9, 0xa9), "text-secondary", false, nullptr},
    {"ClockPreset.backdropColor", qRgb(0x20, 0x20, 0x20), "panel", true, nullptr},
    {"ClockPreset.lowColor", qRgb(0xc8, 0xd8, 0xe8), "text-scale", false, nullptr},
    {"ClockPreset.highColor", qRgb(0xff, 0x44, 0x44), "danger", false, nullptr},
    {"ContestPreset.titleColor", qRgb(0xa9, 0xa9, 0xa9), "text-secondary", false, nullptr},
    {"ContestPreset.backdropColor", qRgb(0x20, 0x20, 0x20), "panel", true, nullptr},
    {"ContestPreset.lowColor", qRgb(0xc8, 0xd8, 0xe8), "text-scale", false, nullptr},
    {"ContestPreset.highColor", qRgb(0xff, 0x44, 0x44), "danger", false, nullptr},
    {"PowerSwrPreset.peakHoldColor", kPowerSwrThetisDefaults.peakHoldColour, "danger", false, nullptr},
    {"PowerSwrPreset.peakValueColor", kPowerSwrThetisDefaults.peakValueColour, "danger", false, nullptr},
    {"PowerSwrPreset.Power.color", qRgb(0xff, 0xff, 0x00), "measured", false, nullptr},
    {"PowerSwrPreset.Power.historyColor", qRgba(0xff, 0x00, 0x00, 128), "measured", false, nullptr},
    {"PowerSwrPreset.SWR.color", qRgb(0xff, 0xff, 0x00), "measured", false, nullptr},
    {"PowerSwrPreset.SWR.historyColor", qRgba(0xff, 0xa5, 0x00, 128), "measured", false, nullptr},
    {"MagicEyePreset.Signal.color", kMagicEyeThetisDefaults.colour, "live", false, nullptr},
    {"MagicEyePreset.Signal.historyColor", kMagicEyeThetisDefaults.historyColour, "measured", false, nullptr},
    {"MagicEyePreset.Signal.historyColor", qRgba(0xff, 0x00, 0x00, 128), "measured", false, nullptr},
    {"SignalTextPreset.Signal.color", kSignalTextThetisDefaults.fontColour, "measured", false, nullptr},
    {"SignalTextPreset.Signal.historyColor", qRgba(0xff, 0x00, 0x00, 128), "measured", false, nullptr},
    {"SignalTextPreset.peakValueColor", kSignalTextThetisDefaults.peakValueColour, "danger", false, nullptr},
    {"SignalTextPreset.typeColor", kSignalTextThetisDefaults.typeColour, "text-secondary", false, nullptr},
    {"SignalTextPreset.subMarkerColor", kSignalTextThetisDefaults.subMarkerColour, "text-secondary", false, nullptr},
    {"SignalTextPreset.backdropColor", kSignalTextThetisDefaults.backgroundColour, "panel", true, nullptr},
    {"HistoryGraphPreset.Signal.color", qRgb(0xff, 0x00, 0x00), "measured", false, nullptr},
    {"HistoryGraphPreset.Signal.color", qRgb(0xff, 0xff, 0x00), "measured", false, nullptr},
    {"HistoryGraphPreset.Signal.historyColor", qRgba(0xff, 0x00, 0x00, 128), "measured", false, nullptr},
    {"HistoryGraphPreset.Trace 2.color", qRgb(0xff, 0xff, 0x00), "dsp-on", false, nullptr},
    {"HistoryGraphPreset.Trace 2.color", qRgb(0xff, 0x00, 0x00), "dsp-on", false, nullptr},
    {"HistoryGraphPreset.Trace 2.historyColor", qRgba(0xff, 0x00, 0x00, 128), "measured", false, nullptr},

    // ANAN multi meter and cross needle: the needles, scale and readouts are
    // drawn over Thetis's meter face art (CompositePresetItem::paintAnan,
    // paintNeedles), which the palette does not restyle.
    {"AnanMM.titleColor", qRgb(0xa9, 0xa9, 0xa9), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.backdropColor", qRgb(0xf1, 0xee, 0xe2), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.lowColor", qRgb(0x20, 0x30, 0x40), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.highColor", qRgb(0xff, 0x44, 0x44), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.powerScaleColor", kAnanThetisDefaults.powerScaleColour, nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.peakHoldColor", kAnanThetisDefaults.peakHoldColour, nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.subNeedleColor", kAnanThetisDefaults.subNeedleColour, nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.Signal.color", qRgb(233, 51, 50), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.Signal.historyColor", qRgba(233, 51, 50, 64), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.Volts.color", qRgb(0, 0, 0), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.Volts.historyColor", qRgba(0, 0, 0, 64), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.Amps.color", qRgb(0, 0, 0), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.Amps.historyColor", qRgba(0, 0, 0, 64), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.Power.color", qRgb(233, 51, 50), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.Power.historyColor", qRgba(233, 51, 50, 64), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.SWR.color", qRgb(0, 0, 0), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.SWR.historyColor", qRgba(0x64, 0x95, 0xed, 0x40), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.Compression.color", qRgb(0, 0, 0), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.Compression.historyColor", qRgba(0, 0, 0, 64), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.ALC group.color", qRgb(0, 0, 0), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"AnanMM.ALC group.historyColor", qRgba(0, 0, 0, 64), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"CrossNeedle.titleColor", qRgb(0xa9, 0xa9, 0xa9), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"CrossNeedle.backdropColor", qRgb(0xf1, 0xee, 0xe2), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"CrossNeedle.lowColor", qRgb(0x20, 0x30, 0x40), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"CrossNeedle.highColor", qRgb(0xff, 0x44, 0x44), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"CrossNeedle.powerScaleColor", kCrossThetisDefaults.powerScaleColour, nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"CrossNeedle.peakHoldColor", kCrossThetisDefaults.peakHoldColour, nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"CrossNeedle.subNeedleColor", kCrossThetisDefaults.subNeedleColour, nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"CrossNeedle.Forward.color", qRgb(0, 0, 0), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"CrossNeedle.Forward.historyColor", qRgba(255, 0, 0, 96), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"CrossNeedle.Reflected.color", qRgb(0, 0, 0), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
    {"CrossNeedle.Reflected.historyColor", qRgba(100, 149, 237, 96), nullptr, false, "drawn over the meter face art, which keeps its own colours"},
};

struct Index {
    QHash<QString, QVector<int>> byField;
};

const Index& index()
{
    static const Index built = [] {
        Index result;
        const QVector<Row>& all = rows();
        for (int i = 0; i < all.size(); ++i) {
            result.byField[QString::fromLatin1(all[i].field)].append(i);
        }
        return result;
    }();
    return built;
}

// A field no row names is a slip at the call site: say so once.
void warnUnknown(const QString& field)
{
    static QMutex mutex;
    static QSet<QString> warned;
    const QMutexLocker locker(&mutex);
    if (warned.contains(field)) {
        return;
    }
    warned.insert(field);
    qCWarning(lcItemColourLook) << "unknown colour field" << field;
}

QColor mapped(const Row& row, const QColor& stored, const QObject* surface)
{
    if (row.role == nullptr) {
        return stored;
    }
    if (row.backdrop) {
        return WorkspacePreview::dataBackground(stored, row.role, surface);
    }
    return WorkspacePreview::roleColour(stored, row.role);
}

void addJsonColours(QVector<FieldColour>& out, const QString& prefix, const QJsonObject& object,
                    const QStringList& skipPrefixes)
{
    for (auto it = object.begin(); it != object.end(); ++it) {
        const QString& key = it.key();
        if (!(key.endsWith(QLatin1String("Color")) || key.endsWith(QLatin1String("Colour")))) {
            continue;
        }
        bool skip = false;
        for (const QString& p : skipPrefixes) {
            if (key.startsWith(p)) {
                skip = true;
            }
        }
        if (skip || !it.value().isString() || it.value().toString().isEmpty()) {
            continue;
        }
        const QColor colour(it.value().toString());
        if (colour.isValid()) {
            out.append({prefix + QLatin1Char('.') + key, colour});
        }
    }
}

void addButtons(QVector<FieldColour>& out, const ButtonBoxItem& grid)
{
    for (int i = 0; i < grid.buttonCount(); ++i) {
        const ButtonBoxItem::ButtonState& b = grid.button(i);
        out.append({QStringLiteral("ButtonBox.fillColour"), b.fillColour});
        out.append({QStringLiteral("ButtonBox.hoverColour"), b.hoverColour});
        out.append({QStringLiteral("ButtonBox.clickColour"), b.clickColour});
        out.append({QStringLiteral("ButtonBox.borderColour"), b.borderColour});
        out.append({QStringLiteral("ButtonBox.onColour"), b.onColour});
        out.append({QStringLiteral("ButtonBox.offColour"), b.offColour});
        out.append({QStringLiteral("ButtonBox.textColour"), b.textColour});
    }
}

} // namespace

const QVector<Row>& rows()
{
    static const QVector<Row> all(std::begin(kRows), std::end(kRows));
    return all;
}

const Row* defaultRow(const QString& field, const QColor& stored)
{
    if (!stored.isValid()) {
        return nullptr;
    }
    const auto it = index().byField.constFind(field);
    if (it == index().byField.constEnd()) {
        return nullptr;
    }
    const QRgb value = stored.rgba();
    for (int i : it.value()) {
        const Row& row = rows()[i];
        if (row.classic == value) {
            return &row;
        }
    }
    return nullptr;
}

bool listed(const QString& field, const QColor& stored)
{
    return defaultRow(field, stored) != nullptr;
}

bool followsLook(const QString& field, const QColor& stored)
{
    if (!WorkspacePreview::enabled()) {
        return false;
    }
    const Row* row = defaultRow(field, stored);
    return row != nullptr && row->role != nullptr;
}

QColor effective(const QString& field, const QColor& stored, const QObject* surface)
{
    if (!WorkspacePreview::enabled()) {
        return stored;
    }
    if (!index().byField.contains(field)) {
        warnUnknown(field);
        return stored;
    }
    const Row* row = defaultRow(field, stored);
    return row != nullptr ? mapped(*row, stored, surface) : stored;
}

QColor effective(const char* field, const QColor& stored, const QObject* surface)
{
    if (!WorkspacePreview::enabled()) {
        return stored;
    }
    return effective(QString::fromLatin1(field), stored, surface);
}

QColor derived(const char* field, const QColor& stored, const char* role, const QColor& classic)
{
    if (!followsLook(QString::fromLatin1(field), stored)) {
        return classic;
    }
    return WorkspacePreview::roleColour(classic, role);
}

QString followsLookNote()
{
    return QStringLiteral("Follows the workspace look");
}

void paintSwatch(QPushButton* button, const QString& field, const QColor& stored)
{
    if (button == nullptr) {
        return;
    }
    const QColor shown = effective(field, stored);
    // The sheet WorkspacePreview's live pass reads as a user colour swatch
    // (colourSwatchSheet()), so the shown colour is never translated again.
    button->setStyleSheet(QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
                              .arg(shown.name(QColor::HexArgb)));
    // The button's own tip (if any) stays; the note follows it while the
    // field follows the look.
    static constexpr auto kBaseTip = "nereusSwatchBaseToolTip";
    if (!button->property(kBaseTip).isValid()) {
        button->setProperty(kBaseTip, button->toolTip());
    }
    const QString base = button->property(kBaseTip).toString();
    if (!followsLook(field, stored)) {
        button->setToolTip(base);
    } else if (base.isEmpty()) {
        button->setToolTip(followsLookNote());
    } else {
        button->setToolTip(base + QLatin1Char('\n') + followsLookNote());
    }
}

QVector<FieldColour> colourFieldsOf(const MeterItem& item)
{
    QVector<FieldColour> out;
    const auto add = [&out](const char* field, const QColor& colour) {
        if (colour.isValid()) {
            out.append({QString::fromLatin1(field), colour});
        }
    };

    if (const auto* composite = qobject_cast<const CompositePresetItem*>(&item)) {
        const QJsonObject config = composite->configuration();
        const QString kind = config.value(QStringLiteral("kind")).toString();
        // The vfo* and clock* keys mirror the VFO display and clock the face
        // holds; those items are listed below with their own fields.
        addJsonColours(out, kind, config, {QStringLiteral("vfo"), QStringLiteral("clock")});
        const QJsonArray channels = config.value(QStringLiteral("channels")).toArray();
        for (const QJsonValue& channel : channels) {
            const QJsonObject c = channel.toObject();
            addJsonColours(out, kind + QLatin1Char('.') + c.value(QStringLiteral("name")).toString(), c, {});
        }
        for (const MeterItem* inner : composite->internalItems()) {
            if (inner != nullptr) {
                out += colourFieldsOf(*inner);
            }
        }
        return out;
    }
    if (const auto* bar = qobject_cast<const BarPresetItem*>(&item)) {
        addJsonColours(out, QStringLiteral("BarPreset"), bar->configuration(), {});
        return out;
    }
    if (const auto* waveform = qobject_cast<const WaveformMeterItem*>(&item)) {
        addJsonColours(out, QStringLiteral("Waveform"), waveform->configuration(), {});
        return out;
    }
    if (const auto* nereus = qobject_cast<const NereusBandButtonItem*>(&item)) {
        add("NereusBand.buttonTextColor", nereus->buttonTextColor());
        add("NereusBand.backgroundTopColor", nereus->backgroundTopColor());
        add("NereusBand.backgroundBottomColor", nereus->backgroundBottomColor());
        add("NereusBand.selectedTopColor", nereus->selectedTopColor());
        add("NereusBand.selectedBottomColor", nereus->selectedBottomColor());
        return out;
    }
    if (const auto* grid = qobject_cast<const ButtonBoxItem*>(&item)) {
        addButtons(out, *grid);
        return out;
    }
    if (const auto* solid = qobject_cast<const SolidColourItem*>(&item)) {
        add("Solid.colour", solid->colour());
    } else if (const auto* bar = qobject_cast<const BarItem*>(&item)) {
        add("Bar.barColor", bar->barColor());
        add("Bar.barRedColor", bar->barRedColor());
        add("Bar.edgeBackgroundColor", bar->edgeBackgroundColor());
        add("Bar.edgeLowColor", bar->edgeLowColor());
        add("Bar.edgeHighColor", bar->edgeHighColor());
        add("Bar.edgeAvgColor", bar->edgeAvgColor());
        add("Bar.fontColour", bar->fontColour());
        add("Bar.peakFontColour", bar->peakFontColour());
        add("Bar.historyColour", bar->historyColour());
        add("Bar.markerColour", bar->markerColour());
        add("Bar.peakHoldMarkerColour", bar->peakHoldMarkerColour());
    } else if (const auto* scale = qobject_cast<const ScaleItem*>(&item)) {
        add("Scale.tickColor", scale->tickColor());
        add("Scale.labelColor", scale->labelColor());
        add("Scale.titleColour", scale->titleColour());
        add("Scale.lowColour", scale->lowColour());
        add("Scale.highColour", scale->highColour());
    } else if (const auto* text = qobject_cast<const TextItem*>(&item)) {
        add("Text.textColor", text->textColor());
    } else if (const auto* needle = qobject_cast<const NeedleItem*>(&item)) {
        add("Needle.needleColor", needle->needleColor());
        add("Needle.peakHoldColour", needle->peakHoldColour());
        add("Needle.historyColor", needle->historyColor());
    } else if (const auto* spacer = qobject_cast<const SpacerItem*>(&item)) {
        add("Spacer.colour1", spacer->colour1());
        add("Spacer.colour2", spacer->colour2());
    } else if (const auto* fade = qobject_cast<const FadeCoverItem*>(&item)) {
        add("FadeCover.colour1", fade->colour1());
        add("FadeCover.colour2", fade->colour2());
    } else if (const auto* led = qobject_cast<const LEDItem*>(&item)) {
        add("Led.trueColour", led->trueColour());
        add("Led.falseColour", led->falseColour());
        add("Led.panelBackColour1", led->panelBackColour1());
        add("Led.panelBackColour2", led->panelBackColour2());
    } else if (const auto* history = qobject_cast<const HistoryGraphItem*>(&item)) {
        add("History.lineColor0", history->lineColor0());
        add("History.lineColor1", history->lineColor1());
        add("History.backColour", history->backColour());
        add("History.linesColour", history->linesColour());
        add("History.timeColour", history->timeColour());
    } else if (const auto* eye = qobject_cast<const MagicEyeItem*>(&item)) {
        add("MagicEye.glowColor", eye->glowColor());
        add("MagicEye.historyColour", eye->historyColour());
    } else if (const auto* power = qobject_cast<const NeedleScalePwrItem*>(&item)) {
        add("PowerScale.lowColour", power->lowColour());
        add("PowerScale.highColour", power->highColour());
    } else if (const auto* signal = qobject_cast<const SignalTextItem*>(&item)) {
        add("SignalText.colour", signal->colour());
        add("SignalText.peakValueColour", signal->peakValueColour());
        add("SignalText.markerColour", signal->markerColour());
        add("SignalText.typeColour", signal->typeColour());
        add("SignalText.subMarkerColour", signal->subMarkerColour());
        add("SignalText.backgroundColour", signal->backgroundColour());
    } else if (const auto* dial = qobject_cast<const DialItem*>(&item)) {
        add("Dial.textColour", dial->textColour());
        add("Dial.circleColour", dial->circleColour());
        add("Dial.padColour", dial->padColour());
        add("Dial.padPressedColour", dial->padPressedColour());
        add("Dial.ringColour", dial->ringColour());
        add("Dial.buttonOnColour", dial->buttonOnColour());
        add("Dial.buttonOffColour", dial->buttonOffColour());
        add("Dial.buttonHighlightColour", dial->buttonHighlightColour());
        add("Dial.slowColour", dial->slowColour());
        add("Dial.holdColour", dial->holdColour());
        add("Dial.fastColour", dial->fastColour());
    } else if (const auto* overlay = qobject_cast<const TextOverlayItem*>(&item)) {
        add("TextOverlay.textColour1", overlay->textColour1());
        add("TextOverlay.textColour2", overlay->textColour2());
        add("TextOverlay.textBackColour1", overlay->textBackColour1());
        add("TextOverlay.textBackColour2", overlay->textBackColour2());
        add("TextOverlay.panelBackColour1", overlay->panelBackColour1());
        add("TextOverlay.panelBackColour2", overlay->panelBackColour2());
    } else if (const auto* web = qobject_cast<const WebImageItem*>(&item)) {
        add("WebImage.fallbackColor", web->fallbackColor());
    } else if (const auto* filter = qobject_cast<const FilterDisplayItem*>(&item)) {
        add("FilterDisplay.dataLineColour", filter->dataLineColour());
        add("FilterDisplay.dataFillColour", filter->dataFillColour());
        add("FilterDisplay.edgesColourRX", filter->edgesColourRX());
        add("FilterDisplay.edgesColourTX", filter->edgesColourTX());
        add("FilterDisplay.notchColour", filter->notchColour());
        add("FilterDisplay.meterBackColour", filter->meterBackColour());
        add("FilterDisplay.textColour", filter->textColour());
        add("FilterDisplay.numberHighlightColour", filter->numberHighlightColour());
        add("FilterDisplay.edgeHighlightColour", filter->edgeHighlightColour());
        add("FilterDisplay.extentsColour", filter->extentsColour());
        add("FilterDisplay.notchHighlightColour", filter->notchHighlightColour());
        add("FilterDisplay.snapLineColour", filter->snapLineColour());
        add("FilterDisplay.settingOnColour", filter->settingOnColour());
        add("FilterDisplay.buttonHighlightColour", filter->buttonHighlightColour());
        add("FilterDisplay.waterfallLowColour", filter->waterfallLowColour());
    } else if (const auto* rotator = qobject_cast<const RotatorItem*>(&item)) {
        add("Rotator.controlColour", rotator->controlColour());
        add("Rotator.bigBlobColour", rotator->bigBlobColour());
        add("Rotator.smallBlobColour", rotator->smallBlobColour());
        add("Rotator.outerTextColour", rotator->outerTextColour());
        add("Rotator.arrowColour", rotator->arrowColour());
        add("Rotator.beamWidthColour", rotator->beamWidthColour());
        add("Rotator.backgroundColour", rotator->backgroundColour());
    } else if (const auto* vfo = qobject_cast<const VfoDisplayItem*>(&item)) {
        add("VfoDisplay.frequencyColour", vfo->frequencyColour());
        add("VfoDisplay.modeColour", vfo->modeColour());
        add("VfoDisplay.filterColour", vfo->filterColour());
        add("VfoDisplay.bandColour", vfo->bandColour());
        add("VfoDisplay.rxColour", vfo->rxColour());
        add("VfoDisplay.txColour", vfo->txColour());
        add("VfoDisplay.frequencySmallColour", vfo->frequencySmallColour());
        add("VfoDisplay.titleColour", vfo->titleColour());
        add("VfoDisplay.splitBackColour", vfo->splitBackColour());
        add("VfoDisplay.splitColour", vfo->splitColour());
        add("VfoDisplay.digitHighlightColour", vfo->digitHighlightColour());
        add("VfoDisplay.bandTextColour", vfo->bandTextColour());
        add("VfoDisplay.lockColour", vfo->lockColour());
        add("VfoDisplay.syncColour", vfo->syncColour());
        add("VfoDisplay.backgroundColour", vfo->backgroundColour());
    } else if (const auto* clock = qobject_cast<const ClockItem*>(&item)) {
        add("Clock.timeColour", clock->timeColour());
        add("Clock.dateColour", clock->dateColour());
        add("Clock.typeTitleColour", clock->typeTitleColour());
        add("Clock.backColour", clock->backColour());
    }
    return out;
}

} // namespace NereusSDR::ItemColourLook
