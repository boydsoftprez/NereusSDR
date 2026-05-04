// =================================================================
// src/gui/meters/ItemGroup.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
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

#include "ItemGroup.h"
#include "MeterWidget.h"
#include "MeterPoller.h"

#include "SpacerItem.h"
#include "FadeCoverItem.h"
#include "LEDItem.h"
#include "HistoryGraphItem.h"
#include "MagicEyeItem.h"
#include "NeedleScalePwrItem.h"
#include "SignalTextItem.h"
#include "DialItem.h"
#include "TextOverlayItem.h"
#include "WebImageItem.h"
#include "FilterDisplayItem.h"
#include "RotatorItem.h"
#include "ButtonBoxItem.h"
#include "BandButtonItem.h"
#include "ModeButtonItem.h"
#include "FilterButtonItem.h"
#include "AntennaButtonItem.h"
#include "TuneStepButtonItem.h"
#include "OtherButtonItem.h"
#include "VoiceRecordPlayItem.h"
#include "DiscordButtonItem.h"
#include "VfoDisplayItem.h"
#include "ClockItem.h"
#include "ClickBoxItem.h"
#include "DataOutItem.h"

#include <QStringList>
#include <QtAlgorithms>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

ItemGroup::ItemGroup(const QString& name, QObject* parent)
    : QObject(parent)
    , m_name(name)
{
}

ItemGroup::~ItemGroup()
{
    qDeleteAll(m_items);
    m_items.clear();
}

// ---------------------------------------------------------------------------
// setRect
// ---------------------------------------------------------------------------

void ItemGroup::setRect(float x, float y, float w, float h)
{
    m_x = x;
    m_y = y;
    m_w = w;
    m_h = h;
}

// ---------------------------------------------------------------------------
// addItem / removeItem
// ---------------------------------------------------------------------------

void ItemGroup::addItem(MeterItem* item)
{
    if (!item) {
        return;
    }
    item->setParent(this);
    m_items.append(item);
}

void ItemGroup::removeItem(MeterItem* item)
{
    m_items.removeOne(item);
}

// ---------------------------------------------------------------------------
// serialize
// Format:
//   GROUP
//   name
//   x
//   y
//   w
//   h
//   itemCount
//   item1_serialized
//   item2_serialized
//   ...
// ---------------------------------------------------------------------------

QString ItemGroup::serialize() const
{
    QStringList lines;
    lines << QStringLiteral("GROUP");
    lines << m_name;
    lines << QString::number(static_cast<double>(m_x));
    lines << QString::number(static_cast<double>(m_y));
    lines << QString::number(static_cast<double>(m_w));
    lines << QString::number(static_cast<double>(m_h));
    lines << QString::number(m_items.size());
    for (const MeterItem* item : m_items) {
        lines << item->serialize();
    }
    return lines.join(QLatin1Char('\n'));
}

// ---------------------------------------------------------------------------
// deserialize
// ---------------------------------------------------------------------------

ItemGroup* ItemGroup::deserialize(const QString& data, QObject* parent)
{
    const QStringList lines = data.split(QLatin1Char('\n'));
    if (lines.size() < 7 || lines[0] != QLatin1String("GROUP")) {
        return nullptr;
    }

    const QString name = lines[1];
    bool ok = true;
    const float x = lines[2].toFloat(&ok); if (!ok) { return nullptr; }
    const float y = lines[3].toFloat(&ok); if (!ok) { return nullptr; }
    const float w = lines[4].toFloat(&ok); if (!ok) { return nullptr; }
    const float h = lines[5].toFloat(&ok); if (!ok) { return nullptr; }
    const int count = lines[6].toInt(&ok); if (!ok) { return nullptr; }

    if (lines.size() < 7 + count) {
        return nullptr;
    }

    ItemGroup* group = new ItemGroup(name, parent);
    group->setRect(x, y, w, h);

    for (int i = 0; i < count; ++i) {
        const QString& itemData = lines[7 + i];
        // Detect type from first pipe-delimited field
        const int pipeIdx = itemData.indexOf(QLatin1Char('|'));
        const QString typeTag = (pipeIdx >= 0) ? itemData.left(pipeIdx) : itemData;

        MeterItem* item = nullptr;
        if (typeTag == QLatin1String("BAR")) {
            BarItem* bar = new BarItem();
            if (bar->deserialize(itemData)) {
                item = bar;
            } else {
                delete bar;
            }
        } else if (typeTag == QLatin1String("SOLID")) {
            SolidColourItem* solid = new SolidColourItem();
            if (solid->deserialize(itemData)) {
                item = solid;
            } else {
                delete solid;
            }
        } else if (typeTag == QLatin1String("IMAGE")) {
            ImageItem* img = new ImageItem();
            if (img->deserialize(itemData)) {
                item = img;
            } else {
                delete img;
            }
        } else if (typeTag == QLatin1String("SCALE")) {
            ScaleItem* scale = new ScaleItem();
            if (scale->deserialize(itemData)) {
                item = scale;
            } else {
                delete scale;
            }
        } else if (typeTag == QLatin1String("TEXT")) {
            TextItem* text = new TextItem();
            if (text->deserialize(itemData)) {
                item = text;
            } else {
                delete text;
            }
        } else if (typeTag == QLatin1String("NEEDLE")) {
            NeedleItem* needle = new NeedleItem();
            if (needle->deserialize(itemData)) {
                item = needle;
            } else {
                delete needle;
            }
        } else if (typeTag == QLatin1String("SPACER")) {
            SpacerItem* spacer = new SpacerItem();
            if (spacer->deserialize(itemData)) {
                item = spacer;
            } else {
                delete spacer;
            }
        } else if (typeTag == QLatin1String("FADECOVER")) {
            FadeCoverItem* fadecover = new FadeCoverItem();
            if (fadecover->deserialize(itemData)) {
                item = fadecover;
            } else {
                delete fadecover;
            }
        } else if (typeTag == QLatin1String("LED")) {
            LEDItem* led = new LEDItem();
            if (led->deserialize(itemData)) {
                item = led;
            } else {
                delete led;
            }
        } else if (typeTag == QLatin1String("HISTORY")) {
            HistoryGraphItem* history = new HistoryGraphItem();
            if (history->deserialize(itemData)) {
                item = history;
            } else {
                delete history;
            }
        } else if (typeTag == QLatin1String("MAGICEYE")) {
            MagicEyeItem* magiceye = new MagicEyeItem();
            if (magiceye->deserialize(itemData)) {
                item = magiceye;
            } else {
                delete magiceye;
            }
        } else if (typeTag == QLatin1String("NEEDLESCALEPWR")) {
            NeedleScalePwrItem* needlescalepwr = new NeedleScalePwrItem();
            if (needlescalepwr->deserialize(itemData)) {
                item = needlescalepwr;
            } else {
                delete needlescalepwr;
            }
        } else if (typeTag == QLatin1String("SIGNALTEXT")) {
            SignalTextItem* signaltext = new SignalTextItem();
            if (signaltext->deserialize(itemData)) {
                item = signaltext;
            } else {
                delete signaltext;
            }
        } else if (typeTag == QLatin1String("DIAL")) {
            DialItem* dial = new DialItem();
            if (dial->deserialize(itemData)) {
                item = dial;
            } else {
                delete dial;
            }
        } else if (typeTag == QLatin1String("TEXTOVERLAY")) {
            TextOverlayItem* textoverlay = new TextOverlayItem();
            if (textoverlay->deserialize(itemData)) {
                item = textoverlay;
            } else {
                delete textoverlay;
            }
        } else if (typeTag == QLatin1String("WEBIMAGE")) {
            WebImageItem* webimage = new WebImageItem();
            if (webimage->deserialize(itemData)) {
                item = webimage;
            } else {
                delete webimage;
            }
        } else if (typeTag == QLatin1String("FILTERDISPLAY")) {
            FilterDisplayItem* filterdisplay = new FilterDisplayItem();
            if (filterdisplay->deserialize(itemData)) {
                item = filterdisplay;
            } else {
                delete filterdisplay;
            }
        } else if (typeTag == QLatin1String("ROTATOR")) {
            RotatorItem* rotator = new RotatorItem();
            if (rotator->deserialize(itemData)) {
                item = rotator;
            } else {
                delete rotator;
            }
        } else if (typeTag == QLatin1String("BANDBTNS")) {
            auto* item = new BandButtonItem(group);
            if (item->deserialize(itemData)) { group->addItem(item); } else { delete item; }
        } else if (typeTag == QLatin1String("MODEBTNS")) {
            auto* item = new ModeButtonItem(group);
            if (item->deserialize(itemData)) { group->addItem(item); } else { delete item; }
        } else if (typeTag == QLatin1String("FILTERBTNS")) {
            auto* item = new FilterButtonItem(group);
            if (item->deserialize(itemData)) { group->addItem(item); } else { delete item; }
        } else if (typeTag == QLatin1String("ANTENNABTNS")) {
            auto* item = new AntennaButtonItem(group);
            if (item->deserialize(itemData)) { group->addItem(item); } else { delete item; }
        } else if (typeTag == QLatin1String("TUNESTEPBTNS")) {
            auto* item = new TuneStepButtonItem(group);
            if (item->deserialize(itemData)) { group->addItem(item); } else { delete item; }
        } else if (typeTag == QLatin1String("OTHERBTNS")) {
            auto* item = new OtherButtonItem(group);
            if (item->deserialize(itemData)) { group->addItem(item); } else { delete item; }
        } else if (typeTag == QLatin1String("VOICERECPLAY")) {
            auto* item = new VoiceRecordPlayItem(group);
            if (item->deserialize(itemData)) { group->addItem(item); } else { delete item; }
        } else if (typeTag == QLatin1String("DISCORDBTNS")) {
            auto* item = new DiscordButtonItem(group);
            if (item->deserialize(itemData)) { group->addItem(item); } else { delete item; }
        } else if (typeTag == QLatin1String("VFO")) {
            auto* item = new VfoDisplayItem(group);
            if (item->deserialize(itemData)) { group->addItem(item); } else { delete item; }
        } else if (typeTag == QLatin1String("CLOCK")) {
            auto* item = new ClockItem(group);
            if (item->deserialize(itemData)) { group->addItem(item); } else { delete item; }
        } else if (typeTag == QLatin1String("CLICKBOX")) {
            auto* item = new ClickBoxItem(group);
            if (item->deserialize(itemData)) { group->addItem(item); } else { delete item; }
        } else if (typeTag == QLatin1String("DATAOUT")) {
            auto* item = new DataOutItem(group);
            if (item->deserialize(itemData)) { group->addItem(item); } else { delete item; }
        }

        if (item) {
            group->addItem(item);
        }
    }

    return group;
}

// ---------------------------------------------------------------------------
// createHBarPreset
// Factory: horizontal bar meter with scale + readout (AetherSDR styling).
// Layout:
//   top 20%  — label (left) + readout (right)
//   mid 28%  — bar (0.22 top, 0.28 height)
//   bottom 46% — scale (0.52 top, 0.46 height)
// Colors from AetherSDR: cyan bar (#00b4d8), red zone (#ff4444), dark bg (#0f0f1a).
// ---------------------------------------------------------------------------

ItemGroup* ItemGroup::createHBarPreset(int bindingId, double minVal, double maxVal,
                                        const QString& name, QObject* parent)
{
    ItemGroup* group = new ItemGroup(name, parent);

    // Background fill
    SolidColourItem* bg = new SolidColourItem();
    bg->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    bg->setColour(QColor(QStringLiteral("#0f0f1a")));
    bg->setZOrder(0);
    group->addItem(bg);

    // Label text (static, no binding)
    TextItem* label = new TextItem();
    label->setRect(0.02f, 0.0f, 0.5f, 0.2f);
    label->setLabel(name);
    label->setBindingId(-1);
    label->setTextColor(QColor(QStringLiteral("#8090a0")));
    label->setFontSize(10);
    label->setBold(false);
    label->setZOrder(10);
    group->addItem(label);

    // Readout text (dynamic, bound to bindingId)
    TextItem* readout = new TextItem();
    readout->setRect(0.5f, 0.0f, 0.48f, 0.2f);
    readout->setLabel(QString());
    readout->setBindingId(bindingId);
    readout->setTextColor(QColor(QStringLiteral("#c8d8e8")));
    readout->setFontSize(13);
    readout->setBold(true);
    readout->setSuffix(QStringLiteral(" dBm"));
    readout->setDecimals(1);
    readout->setZOrder(10);
    group->addItem(readout);

    // Bar meter
    BarItem* bar = new BarItem();
    bar->setRect(0.02f, 0.22f, 0.96f, 0.28f);
    bar->setOrientation(BarItem::Orientation::Horizontal);
    bar->setRange(minVal, maxVal);
    bar->setBindingId(bindingId);
    bar->setBarColor(QColor(QStringLiteral("#00b4d8")));
    bar->setBarRedColor(QColor(QStringLiteral("#ff4444")));
    bar->setRedThreshold(minVal + (maxVal - minVal) * 0.9);
    bar->setZOrder(5);
    group->addItem(bar);

    // Scale
    ScaleItem* scale = new ScaleItem();
    scale->setRect(0.02f, 0.52f, 0.96f, 0.46f);
    scale->setOrientation(ScaleItem::Orientation::Horizontal);
    scale->setRange(minVal, maxVal);
    scale->setMajorTicks(7);
    scale->setMinorTicks(5);
    scale->setTickColor(QColor(QStringLiteral("#c8d8e8")));
    scale->setLabelColor(QColor(QStringLiteral("#8090a0")));
    scale->setFontSize(9);
    scale->setZOrder(10);
    group->addItem(scale);

    return group;
}

// ---------------------------------------------------------------------------
// createCompactHBarPreset
// Compact single-line layout: label (left 20%), bar (center 50%), readout (right 28%).
// No scale ticks. From AetherSDR HGauge pattern (24px fixed height).
// ---------------------------------------------------------------------------

ItemGroup* ItemGroup::createCompactHBarPreset(int bindingId, double minVal, double maxVal,
                                               const QString& name, QObject* parent)
{
    ItemGroup* group = new ItemGroup(name, parent);

    // Background fill
    SolidColourItem* bg = new SolidColourItem();
    bg->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    bg->setColour(QColor(QStringLiteral("#0f0f1a")));
    bg->setZOrder(0);
    group->addItem(bg);

    // Label text (left 15%)
    TextItem* label = new TextItem();
    label->setRect(0.02f, 0.05f, 0.15f, 0.9f);
    label->setLabel(name);
    label->setBindingId(-1);
    label->setTextColor(QColor(QStringLiteral("#8090a0")));
    label->setFontSize(8);
    label->setBold(false);
    label->setZOrder(10);
    group->addItem(label);

    // Bar meter (center 45%)
    BarItem* bar = new BarItem();
    bar->setRect(0.17f, 0.2f, 0.45f, 0.6f);
    bar->setOrientation(BarItem::Orientation::Horizontal);
    bar->setRange(minVal, maxVal);
    bar->setBindingId(bindingId);
    bar->setBarColor(QColor(QStringLiteral("#00b4d8")));
    bar->setBarRedColor(QColor(QStringLiteral("#ff4444")));
    bar->setRedThreshold(minVal + (maxVal - minVal) * 0.9);
    bar->setZOrder(5);
    group->addItem(bar);

    // Readout text (right 36%)
    TextItem* readout = new TextItem();
    readout->setRect(0.62f, 0.05f, 0.36f, 0.9f);
    readout->setBindingId(bindingId);
    readout->setTextColor(QColor(QStringLiteral("#c8d8e8")));
    readout->setFontSize(8);
    readout->setBold(true);
    readout->setSuffix(QStringLiteral(" dBm"));
    readout->setDecimals(1);
    readout->setIdleText(QStringLiteral("\u2014 dBm"));
    readout->setMinValidValue(minVal);
    readout->setZOrder(10);
    group->addItem(readout);

    return group;
}

// ---------------------------------------------------------------------------
// installInto
// Transforms each item's normalized 0-1 rect into the target rect and
// transfers ownership to the given MeterWidget.
// ---------------------------------------------------------------------------

void ItemGroup::installInto(MeterWidget* widget, float gx, float gy, float gw, float gh)
{
    for (MeterItem* item : m_items) {
        item->setRect(
            gx + item->x() * gw,
            gy + item->y() * gh,
            item->itemWidth() * gw,
            item->itemHeight() * gh
        );
        item->setParent(widget);
        widget->addItem(item);
    }
    m_items.clear();
}

// ---------------------------------------------------------------------------
// createSMeterPreset
// From AetherSDR SMeterWidget — single NeedleItem handles all rendering.
// This is the default used by Container #0's fixed S-Meter header
// (MainWindow) and the Presets menu "S-Meter Only" entry. Phase 3G-9
// briefly rebuilt this as a Thetis-style bar composition
// (commit 4bba2c2) but that replaced the main signal meter against
// the design intent; the revert restores the AetherSDR needle as the
// default shape. The Thetis addSMeterBar port still lives in
// createSMeterBarPreset below for opt-in verification/use.
// ---------------------------------------------------------------------------
ItemGroup* ItemGroup::createSMeterPreset(int bindingId, const QString& name,
                                          QObject* parent)
{
    ItemGroup* group = new ItemGroup(name, parent);

    NeedleItem* needle = new NeedleItem();
    needle->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    needle->setBindingId(bindingId);
    needle->setSourceLabel(name);
    needle->setZOrder(5);
    group->addItem(needle);

    return group;
}

// ---------------------------------------------------------------------------
// createSMeterBarPreset — Thetis addSMeterBar composition (opt-in)
// ---------------------------------------------------------------------------
// From Thetis MeterManager.cs:21523-21616 addSMeterBar.
// Dark gray background + Line-style BarItem with 3-point non-linear
// calibration (S0 edge to S9+60) + ScaleItem showing the reading
// title and a two-tone GeneralScale. Accepts SignalPeak / SignalAvg
// / SignalMaxBin bindings — same composition, different data source.
//
// Not wired into any UI call site by default. Exists so the Phase
// 3G-9 calibrated bar S-meter port is preserved; add to a new
// container manually if you want to run/verify it.
ItemGroup* ItemGroup::createSMeterBarPreset(int bindingId, const QString& name,
                                             QObject* parent)
{
    ItemGroup* group = new ItemGroup(name, parent);

    // Dark gray backdrop (Thetis Color.FromArgb(32,32,32), MeterManager.cs:21571)
    // Fills the whole preset rect so title + bar + ticks all sit on it.
    SolidColourItem* bg = new SolidColourItem();
    bg->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    bg->setColour(QColor(32, 32, 32));
    bg->setZOrder(1);
    group->addItem(bg);

    // Line-style bar with Thetis calibration curve
    // (MeterManager.cs:21529-21555). The bar occupies the bottom ~55%
    // of the preset rect, leaving ~45% at the top for the ScaleItem
    // title strip.
    BarItem* bar = new BarItem();
    bar->setRect(0.02f, 0.45f, 0.96f, 0.50f);
    bar->setOrientation(BarItem::Orientation::Horizontal);
    bar->setBindingId(bindingId);
    bar->setRange(-140.0, 0.0);  // dBm range for value labels
    bar->setBarStyle(BarItem::BarStyle::Line);
    bar->setAttackRatio(0.8f);
    bar->setDecayRatio(0.2f);
    bar->setHistoryDurationMs(4000);
    bar->setShowHistory(true);
    bar->setHistoryColour(QColor(255, 0, 0, 128));   // Red(128)
    bar->setShowValue(true);
    bar->setShowMarker(true);
    bar->setPeakHoldMarkerColour(QColor(Qt::red));
    bar->setFontColour(QColor(Qt::yellow));
    bar->setBarColor(QColor(0x5f, 0x9e, 0xa0));      // CadetBlue
    // Thetis addSMeterBar non-linear calibration
    // (MeterManager.cs:21547-21549). -133 places S0 at the left edge
    // (S0 is actually -127 dBm but Thetis uses -133 for the bar
    // origin), -73 is S9 at mid-scale, -13 is S9+60 at 99% width.
    bar->addScaleCalibration(-133.0, 0.0f);
    bar->addScaleCalibration( -73.0, 0.5f);
    bar->addScaleCalibration( -13.0, 0.99f);
    bar->setZOrder(2);
    group->addItem(bar);

    // Scale with centered title and Thetis-style two-tone GeneralScale
    // baseline (MeterManager.cs:21557-21564 + dispatch at 31911-31916:
    //   generalScale(x,y,w,h, scale, 6, 3, -1, 60, 2, 20, ..., 0.5f, true, true)
    // lowLongTicks=6, highLongTicks=3, lowStart=-1, highEnd=60,
    // lowInc=2, highInc=20, centrePerc=0.5. The +/- trailing-plus
    // flags are cosmetic label decorations we don't port yet.
    ScaleItem* scale = new ScaleItem();
    scale->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    scale->setBindingId(bindingId);
    scale->setRange(-140.0, 0.0);
    scale->setShowType(true);
    scale->setTitleColour(QColor(Qt::red));
    scale->setScaleStyle(ScaleItem::ScaleStyle::GeneralScale);
    scale->setGeneralScaleParams(6, 3, -1, 60, 2, 20, 0.5f);
    scale->setZOrder(3);
    group->addItem(scale);

    Q_UNUSED(name);  // Thetis uses MeterType instead of a free label
    return group;
}

// ---------------------------------------------------------------------------
// createPowerSwrPreset
// From Thetis MeterManager.cs AddPWRBar (line 23862) + AddSWRBar (line 23990)
// Power: 0-120W, HighPoint at 100W (75%). DecayRatio = 0.1
// SWR: 1:1-5:1, HighPoint at 3:1 (75%). DecayRatio = 0.1
// ---------------------------------------------------------------------------

ItemGroup* ItemGroup::createPowerSwrPreset(const QString& name, QObject* parent)
{
    ItemGroup* group = new ItemGroup(name, parent);

    // Background
    SolidColourItem* bg = new SolidColourItem();
    bg->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    bg->setColour(QColor(0x0f, 0x0f, 0x1a));
    bg->setZOrder(0);
    group->addItem(bg);

    // --- Forward Power section (top half) ---

    TextItem* pwrLabel = new TextItem();
    pwrLabel->setRect(0.02f, 0.0f, 0.5f, 0.15f);
    pwrLabel->setLabel(QStringLiteral("Power"));
    pwrLabel->setBindingId(-1);
    pwrLabel->setTextColor(QColor(0x80, 0x90, 0xa0));
    pwrLabel->setFontSize(10);
    pwrLabel->setBold(false);
    pwrLabel->setZOrder(10);
    group->addItem(pwrLabel);

    TextItem* pwrReadout = new TextItem();
    pwrReadout->setRect(0.5f, 0.0f, 0.48f, 0.15f);
    pwrReadout->setBindingId(MeterBinding::TxPower);
    pwrReadout->setTextColor(QColor(0xc8, 0xd8, 0xe8));
    pwrReadout->setFontSize(13);
    pwrReadout->setBold(true);
    pwrReadout->setSuffix(QStringLiteral(" W"));
    pwrReadout->setDecimals(0);
    pwrReadout->setIdleText(QStringLiteral("\u2014 W"));
    pwrReadout->setMinValidValue(0.0);
    pwrReadout->setZOrder(10);
    group->addItem(pwrReadout);

    // From Thetis MeterManager.cs AddPWRBar: 0-120W, red at 100W
    // Range + redThreshold are reset per-SKU at runtime via
    // MeterWidget::rescalePowerMeters() (bench-reported #167 follow-up:
    // 0-120 W default made HL2's 5 W output a tiny sliver and ANAN-G2-1K's
    // 1000 W output saturate the bar).  The objectName tag is the
    // discriminator for the rescale lookup.
    BarItem* pwrBar = new BarItem();
    pwrBar->setObjectName(QStringLiteral("PowerBar"));
    pwrBar->setRect(0.02f, 0.17f, 0.96f, 0.22f);
    pwrBar->setOrientation(BarItem::Orientation::Horizontal);
    pwrBar->setRange(0.0, 120.0);
    pwrBar->setBindingId(MeterBinding::TxPower);
    pwrBar->setBarColor(QColor(0x00, 0xb4, 0xd8));
    pwrBar->setBarRedColor(QColor(0xff, 0x44, 0x44));
    pwrBar->setRedThreshold(100.0);   // From Thetis: HighPoint = 100W
    pwrBar->setAttackRatio(0.8f);     // From Thetis MeterManager.cs
    pwrBar->setDecayRatio(0.1f);      // From Thetis: PWR DecayRatio = 0.1
    pwrBar->setZOrder(5);
    group->addItem(pwrBar);

    ScaleItem* pwrScale = new ScaleItem();
    pwrScale->setObjectName(QStringLiteral("PowerScale"));
    pwrScale->setRect(0.02f, 0.40f, 0.96f, 0.12f);
    pwrScale->setOrientation(ScaleItem::Orientation::Horizontal);
    pwrScale->setRange(0.0, 120.0);
    pwrScale->setMajorTicks(7);       // 0, 20, 40, 60, 80, 100, 120
    pwrScale->setMinorTicks(3);
    pwrScale->setFontSize(8);
    pwrScale->setZOrder(10);
    group->addItem(pwrScale);

    // --- SWR section (bottom half) ---

    TextItem* swrLabel = new TextItem();
    swrLabel->setRect(0.02f, 0.55f, 0.5f, 0.12f);
    swrLabel->setLabel(QStringLiteral("SWR"));
    swrLabel->setBindingId(-1);
    swrLabel->setTextColor(QColor(0x80, 0x90, 0xa0));
    swrLabel->setFontSize(10);
    swrLabel->setBold(false);
    swrLabel->setZOrder(10);
    group->addItem(swrLabel);

    TextItem* swrReadout = new TextItem();
    swrReadout->setRect(0.5f, 0.55f, 0.48f, 0.12f);
    swrReadout->setBindingId(MeterBinding::TxSwr);
    swrReadout->setTextColor(QColor(0xc8, 0xd8, 0xe8));
    swrReadout->setFontSize(13);
    swrReadout->setBold(true);
    swrReadout->setSuffix(QStringLiteral(":1"));
    swrReadout->setDecimals(1);
    swrReadout->setIdleText(QStringLiteral("\u221E:1"));
    swrReadout->setMinValidValue(1.0);
    swrReadout->setZOrder(10);
    group->addItem(swrReadout);

    // From Thetis MeterManager.cs AddSWRBar: 1:1-5:1, red at 3:1
    BarItem* swrBar = new BarItem();
    swrBar->setRect(0.02f, 0.69f, 0.96f, 0.15f);
    swrBar->setOrientation(BarItem::Orientation::Horizontal);
    swrBar->setRange(1.0, 5.0);
    swrBar->setBindingId(MeterBinding::TxSwr);
    swrBar->setBarColor(QColor(0x00, 0xb4, 0xd8));
    swrBar->setBarRedColor(QColor(0xff, 0x44, 0x44));
    swrBar->setRedThreshold(3.0);     // From Thetis: HighPoint = SWR 3:1
    swrBar->setAttackRatio(0.8f);
    swrBar->setDecayRatio(0.1f);      // From Thetis: SWR DecayRatio = 0.1
    swrBar->setZOrder(5);
    group->addItem(swrBar);

    ScaleItem* swrScale = new ScaleItem();
    swrScale->setRect(0.02f, 0.86f, 0.96f, 0.12f);
    swrScale->setOrientation(ScaleItem::Orientation::Horizontal);
    swrScale->setRange(1.0, 5.0);
    swrScale->setMajorTicks(5);       // 1, 2, 3, 4, 5
    swrScale->setMinorTicks(1);
    swrScale->setFontSize(8);
    swrScale->setZOrder(10);
    group->addItem(swrScale);

    return group;
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Phase E — per-reading bar row port from Thetis Add*Bar factories.
// Shared builder + 16 thin wrappers. Each per-reading factory differs
// only in bindingId, 3-point calibration, and historyColour.
// ---------------------------------------------------------------------------

// From Thetis MeterManager.cs:23326-23411 (AddALCBar) — canonical
// 5-item bar row composition replicated across every Add*Bar factory:
//   clsSolidColour dark gray (32,32,32) z=1   — backdrop
//   clsBarItem PRIMARY z=2                    — peak bar, ShowHistory,
//       ShowValue, ShowMarker, MarkerColour Yellow, Line style,
//       3-point calibration, attack 0.8 / decay 0.1
//   clsScaleItem z=3 ShowType=true            — centered red title via
//       readingName(bindingId)
//
// NereusSDR collapses Thetis's dual-BarItem (separate _PK + _AV bars
// chained via PostDrawItem) into a single BarItem whose ShowPeakValue
// + m_peakValue tracking provides the same visual effect: one bar
// shows the live average, one marker holds the decaying peak.
ItemGroup* ItemGroup::buildBarRow(int bindingId,
                                  double lowVal, double midVal, double highVal,
                                  float midX, float highX,
                                  const QColor& historyColour,
                                  QObject* parent)
{
    ItemGroup* group = new ItemGroup(readingName(bindingId), parent);

    // z=1 dark gray backdrop (Thetis line 23390-23396 sc)
    SolidColourItem* bg = new SolidColourItem();
    bg->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    bg->setColour(QColor(32, 32, 32));
    bg->setZOrder(1);
    group->addItem(bg);

    // z=2 Line-style bar with ShowValue + ShowPeakValue + ShowHistory
    // + ShowMarker + PeakHoldMarker (Thetis line 23332-23354).
    //
    // Colours pinned from the live Thetis dialog at
    // Setup → Appearance → Meters/Gadgets ("ALC Compression" selected):
    //   Low          = white
    //   High         = red
    //   Indicator    = yellow   (marker)
    //   Peak Value   = red      (text and peak-hold marker)
    //   Show History = LemonChiffon(128) — caller-supplied
    //   Meter Title  = red      (set on the ScaleItem below)
    //   Current value text = white (same as Low colour)
    BarItem* bar = new BarItem();
    bar->setRect(0.02f, 0.45f, 0.96f, 0.50f);
    bar->setOrientation(BarItem::Orientation::Horizontal);
    bar->setBindingId(bindingId);
    bar->setRange(lowVal, highVal);
    bar->setBarStyle(BarItem::BarStyle::Line);
    bar->setAttackRatio(0.8f);
    bar->setDecayRatio(0.1f);
    bar->setHistoryDurationMs(2000);
    bar->setShowHistory(true);
    bar->setHistoryColour(historyColour);
    bar->setShowValue(true);
    bar->setShowPeakValue(true);
    bar->setShowMarker(true);
    bar->setMarkerColour(QColor(Qt::yellow));
    bar->setPeakHoldMarkerColour(QColor(Qt::red));
    bar->setPeakHoldDecayRatio(0.02f);
    bar->setFontColour(QColor(Qt::white));
    bar->setPeakFontColour(QColor(Qt::red));
    bar->setBarColor(QColor(Qt::white));
    bar->setBarRedColor(QColor(Qt::red));
    bar->setRedThreshold(midVal);
    // 3-point calibration — maps lowVal -> 0.0, midVal -> midX,
    // highVal -> highX. Matches Thetis per-reading ScaleCalibration
    // calls (AddALCBar line 23347-23349, AddMicBar 23025-23027, etc.).
    bar->addScaleCalibration(lowVal,  0.0f);
    bar->addScaleCalibration(midVal,  midX);
    bar->addScaleCalibration(highVal, highX);
    bar->setZOrder(2);
    group->addItem(bar);

    // z=3 Scale with ShowType centered title + linear tick layout
    // (Thetis line 23381-23388). Falls through to the NereusSDR
    // Linear scale renderer since each bar row uses a linear tick
    // layout — GeneralScale's two-tone is reserved for the S-Meter
    // preset where lowStart/highEnd are asymmetric.
    ScaleItem* scale = new ScaleItem();
    scale->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    scale->setBindingId(bindingId);
    scale->setRange(lowVal, highVal);
    scale->setShowType(true);
    scale->setTitleColour(QColor(Qt::red));
    scale->setMajorTicks(6);
    scale->setMinorTicks(4);
    scale->setZOrder(3);
    group->addItem(scale);

    return group;
}

// The 16 per-reading factories — each a one-liner call to buildBarRow
// with the reading's bindingId, min/mid/high calibration, and
// reference-sourced historyColour.

ItemGroup* ItemGroup::createAlcPreset(QObject* parent)
{
    // From Thetis AddALCBar:23347-23349 — cal (-30 -> 0, 0 -> 0.665, 12 -> 0.99)
    //        AddALCBar:23345      — historyColour LemonChiffon(128)
    return buildBarRow(MeterBinding::TxAlc,
                       -30.0, 0.0, 12.0, 0.665f, 0.99f,
                       QColor(255, 250, 205, 128),
                       parent);
}

ItemGroup* ItemGroup::createMicPreset(QObject* parent)
{
    // From Thetis AddMicBar:23025-23027 — cal (-30, 0@0.665, 12@0.99)
    //        AddMicBar:23023      — historyColour Red(128)
    return buildBarRow(MeterBinding::TxMic,
                       -30.0, 0.0, 12.0, 0.665f, 0.99f,
                       QColor(255, 0, 0, 128),
                       parent);
}

ItemGroup* ItemGroup::createCompPreset(QObject* parent)
{
    // From Thetis AddCompBar:23681+ — same calibration shape as ALC
    return buildBarRow(MeterBinding::TxComp,
                       -30.0, 0.0, 12.0, 0.665f, 0.99f,
                       QColor(255, 250, 205, 128),
                       parent);
}

ItemGroup* ItemGroup::createSignalBarPreset(QObject* parent)
{
    // Linear -140..0 range for direct dBm signal readout. The Thetis
    // calibrated 3-point S-meter (S0/S9/S9+60) lives in
    // createSMeterBarPreset (opt-in, not wired to any UI call site).
    return buildBarRow(MeterBinding::SignalPeak,
                       -140.0, -70.0, 0.0, 0.5f, 0.99f,
                       QColor(255, 0, 0, 128),
                       parent);
}

ItemGroup* ItemGroup::createAvgSignalBarPreset(QObject* parent)
{
    return buildBarRow(MeterBinding::SignalAvg,
                       -140.0, -70.0, 0.0, 0.5f, 0.99f,
                       QColor(255, 0, 0, 128),
                       parent);
}

ItemGroup* ItemGroup::createMaxBinBarPreset(QObject* parent)
{
    return buildBarRow(MeterBinding::SignalMaxBin,
                       -140.0, -70.0, 0.0, 0.5f, 0.99f,
                       QColor(255, 0, 0, 128),
                       parent);
}

ItemGroup* ItemGroup::createAdcBarPreset(QObject* parent)
{
    // From Thetis AddADCBar:21740+
    return buildBarRow(MeterBinding::AdcAvg,
                       -140.0, -70.0, 0.0, 0.5f, 0.99f,
                       QColor(100, 149, 237, 128),  // CornflowerBlue(128)
                       parent);
}

ItemGroup* ItemGroup::createAdcMaxMagPreset(QObject* parent)
{
    // From Thetis AddADCMaxMag:21638-21640 — cal (0 -> 0, 25000 -> 0.8333, 32768 -> 0.99)
    return buildBarRow(MeterBinding::AdcPeak,
                       0.0, 25000.0, 32768.0, 0.8333f, 0.99f,
                       QColor(100, 149, 237, 128),
                       parent);
}

ItemGroup* ItemGroup::createAgcBarPreset(QObject* parent)
{
    // Thetis renderScale dispatch AGC_AV: generalScale(.., -125, 125, 25, 25, ..)
    return buildBarRow(MeterBinding::AgcAvg,
                       -125.0, 0.0, 125.0, 0.5f, 0.99f,
                       QColor(255, 250, 205, 128),
                       parent);
}

ItemGroup* ItemGroup::createAgcGainBarPreset(QObject* parent)
{
    // Thetis renderScale AGC_GAIN: generalScale(.., -50, 125, 25, 25, ..)
    return buildBarRow(MeterBinding::AgcGain,
                       -50.0, 25.0, 125.0, 0.428f, 0.99f,
                       QColor(255, 250, 205, 128),
                       parent);
}

ItemGroup* ItemGroup::createPbsnrBarPreset(QObject* parent)
{
    return buildBarRow(MeterBinding::PbSnr,
                       0.0, 30.0, 60.0, 0.5f, 0.99f,
                       QColor(255, 250, 205, 128),
                       parent);
}

ItemGroup* ItemGroup::createEqBarPreset(QObject* parent)
{
    // From Thetis AddEQBar:23091+ — same calibration as ALC/Mic
    return buildBarRow(MeterBinding::TxEq,
                       -30.0, 0.0, 12.0, 0.665f, 0.99f,
                       QColor(255, 250, 205, 128),
                       parent);
}

ItemGroup* ItemGroup::createLevelerBarPreset(QObject* parent)
{
    // From Thetis AddLevelerBar:23179+
    return buildBarRow(MeterBinding::TxLeveler,
                       -30.0, 0.0, 12.0, 0.665f, 0.99f,
                       QColor(255, 250, 205, 128),
                       parent);
}

ItemGroup* ItemGroup::createLevelerGainBarPreset(QObject* parent)
{
    // From Thetis AddLevelerGainBar:23265+
    return buildBarRow(MeterBinding::TxLevelerGain,
                       0.0, 10.0, 30.0, 0.333f, 0.99f,
                       QColor(255, 250, 205, 128),
                       parent);
}

ItemGroup* ItemGroup::createAlcGainBarPreset(QObject* parent)
{
    // From Thetis AddALCGainBar:23412+
    return buildBarRow(MeterBinding::TxAlcGain,
                       0.0, 10.0, 30.0, 0.333f, 0.99f,
                       QColor(255, 250, 205, 128),
                       parent);
}

ItemGroup* ItemGroup::createAlcGroupBarPreset(QObject* parent)
{
    // From Thetis AddALCGroupBar:23473+
    return buildBarRow(MeterBinding::TxAlcGroup,
                       -30.0, 0.0, 25.0, 0.545f, 0.99f,
                       QColor(255, 250, 205, 128),
                       parent);
}

ItemGroup* ItemGroup::createCfcBarPreset(QObject* parent)
{
    // From Thetis AddCFCBar:23534+
    return buildBarRow(MeterBinding::TxCfc,
                       -30.0, 0.0, 12.0, 0.665f, 0.99f,
                       QColor(255, 250, 205, 128),
                       parent);
}

ItemGroup* ItemGroup::createCfcGainBarPreset(QObject* parent)
{
    // From Thetis AddCFCGainBar:23620+
    return buildBarRow(MeterBinding::TxCfcGain,
                       0.0, 10.0, 30.0, 0.333f, 0.99f,
                       QColor(255, 250, 205, 128),
                       parent);
}

ItemGroup* ItemGroup::createCustomBarPreset(int bindingId, double minVal, double maxVal,
                                             const QString& name, QObject* parent)
{
    return createHBarPreset(bindingId, minVal, maxVal, name, parent);
}

// ---------------------------------------------------------------------------
// Phase 3G-4 composite presets
// ---------------------------------------------------------------------------

// createAnanMMPreset
// From Thetis AddAnanMM() (MeterManager.cs:22461-22815)
// 7-needle composite meter: Signal, Volts, Amps, Power, SWR, Compression, ALC
// with NeedleScalePwrItem for power labels.
ItemGroup* ItemGroup::createAnanMMPreset(QObject* parent)
{
    ItemGroup* group = new ItemGroup(QStringLiteral("ANAN MM"), parent);

    // Helper: populate calibration map from initializer list
    // From Thetis MeterManager.cs calibration point arrays
    auto addCal = [](NeedleItem* ni, std::initializer_list<std::pair<float, QPointF>> points) {
        QMap<float, QPointF> cal;
        for (const auto& [val, pos] : points) {
            cal.insert(val, pos);
        }
        ni->setScaleCalibration(cal);
    };

    // Shared needle geometry — from Thetis MeterManager.cs:22487-22489
    const QPointF sharedOffset(0.004, 0.736);
    const QPointF sharedRadius(1.0, 0.58);

    // 1. Background image (z=1)
    ImageItem* bg = new ImageItem();
    bg->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    bg->setImagePath(QStringLiteral(":/meters/ananMM.png"));
    bg->setZOrder(1);
    group->addItem(bg);

    // 2. Signal needle — bindingId=SignalAvg, onlyWhenRx=true
    // From Thetis MeterManager.cs:22491-22510
    NeedleItem* signal = new NeedleItem();
    signal->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    signal->setBindingId(MeterBinding::SignalAvg);
    signal->setZOrder(10);
    signal->setNeedleColor(QColor(233, 51, 50));
    signal->setAttackRatio(0.8f);
    signal->setDecayRatio(0.2f);
    signal->setLengthFactor(1.65f);
    signal->setNeedleOffset(sharedOffset);
    signal->setRadiusRatio(sharedRadius);
    signal->setOnlyWhenRx(true);
    signal->setSourceLabel(QStringLiteral("Signal"));
    // 16-point calibration — from Thetis MeterManager.cs:22491-22506
    addCal(signal, {
        {-127.0f, QPointF(0.076, 0.31)},  {-121.0f, QPointF(0.131, 0.272)},
        {-115.0f, QPointF(0.189, 0.254)},  {-109.0f, QPointF(0.233, 0.211)},
        {-103.0f, QPointF(0.284, 0.207)},  { -97.0f, QPointF(0.326, 0.177)},
        { -91.0f, QPointF(0.374, 0.177)},  { -85.0f, QPointF(0.414, 0.151)},
        { -79.0f, QPointF(0.459, 0.168)},  { -73.0f, QPointF(0.501, 0.142)},
        { -63.0f, QPointF(0.564, 0.172)},  { -53.0f, QPointF(0.63, 0.164)},
        { -43.0f, QPointF(0.695, 0.203)},  { -33.0f, QPointF(0.769, 0.211)},
        { -23.0f, QPointF(0.838, 0.272)},  { -13.0f, QPointF(0.926, 0.31)}
    });
    group->addItem(signal);

    // 3. Volts needle — bindingId=HwVolts
    // From Thetis MeterManager.cs:22530-22545
    NeedleItem* volts = new NeedleItem();
    volts->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    volts->setBindingId(MeterBinding::HwVolts);
    volts->setZOrder(10);
    volts->setNeedleColor(Qt::black);
    volts->setAttackRatio(0.2f);
    volts->setDecayRatio(0.2f);
    volts->setLengthFactor(0.75f);
    volts->setNeedleOffset(sharedOffset);
    volts->setRadiusRatio(sharedRadius);
    volts->setSourceLabel(QStringLiteral("Volts"));
    // 3-point calibration — from Thetis MeterManager.cs:22535-22537
    addCal(volts, {
        {10.0f,  QPointF(0.559, 0.756)},
        {12.5f,  QPointF(0.605, 0.772)},
        {15.0f,  QPointF(0.665, 0.784)}
    });
    group->addItem(volts);

    // 4. Amps needle — bindingId=HwAmps, onlyWhenTx=true, displayGroup=4
    // From Thetis MeterManager.cs:22548-22570
    NeedleItem* amps = new NeedleItem();
    amps->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    amps->setBindingId(MeterBinding::HwAmps);
    amps->setZOrder(10);
    amps->setNeedleColor(Qt::black);
    amps->setLengthFactor(1.15f);
    amps->setNeedleOffset(sharedOffset);
    amps->setRadiusRatio(sharedRadius);
    amps->setOnlyWhenTx(true);
    amps->setDisplayGroup(4);
    amps->setSourceLabel(QStringLiteral("Amps"));
    // 11-point calibration — from Thetis MeterManager.cs:22555-22565
    addCal(amps, {
        { 0.0f, QPointF(0.199, 0.576)},  { 2.0f, QPointF(0.27,  0.54)},
        { 4.0f, QPointF(0.333, 0.516)},  { 6.0f, QPointF(0.393, 0.504)},
        { 8.0f, QPointF(0.448, 0.492)},  {10.0f, QPointF(0.499, 0.492)},
        {12.0f, QPointF(0.554, 0.488)},  {14.0f, QPointF(0.608, 0.5)},
        {16.0f, QPointF(0.667, 0.516)},  {18.0f, QPointF(0.728, 0.54)},
        {20.0f, QPointF(0.799, 0.576)}
    });
    group->addItem(amps);

    // 5. Power needle — bindingId=TxPower, onlyWhenTx=true, displayGroup=1
    // From Thetis MeterManager.cs:22573-22605
    NeedleItem* power = new NeedleItem();
    power->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    power->setBindingId(MeterBinding::TxPower);
    power->setZOrder(10);
    power->setNeedleColor(QColor(233, 51, 50));
    power->setAttackRatio(0.2f);
    power->setDecayRatio(0.1f);
    power->setLengthFactor(1.55f);
    power->setNeedleOffset(sharedOffset);
    power->setRadiusRatio(sharedRadius);
    power->setOnlyWhenTx(true);
    power->setDisplayGroup(1);
    power->setNormaliseTo100W(true);
    power->setHistoryEnabled(true);
    power->setHistoryDuration(4000);
    power->setHistoryColor(QColor(64, 233, 51, 50));
    power->setSourceLabel(QStringLiteral("Power"));
    // 10-point calibration — from Thetis MeterManager.cs:22580-22589
    addCal(power, {
        {  0.0f, QPointF(0.099, 0.352)},  {  5.0f, QPointF(0.164, 0.312)},
        { 10.0f, QPointF(0.224, 0.28)},   { 25.0f, QPointF(0.335, 0.236)},
        { 30.0f, QPointF(0.367, 0.228)},  { 40.0f, QPointF(0.436, 0.22)},
        { 50.0f, QPointF(0.499, 0.212)},  { 60.0f, QPointF(0.559, 0.216)},
        {100.0f, QPointF(0.751, 0.272)},  {150.0f, QPointF(0.899, 0.352)}
    });
    group->addItem(power);

    // 6. Power scale — NeedleScalePwrItem companion
    // From Thetis MeterManager.cs:22607-22620
    NeedleScalePwrItem* pwrScale = new NeedleScalePwrItem();
    pwrScale->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    pwrScale->setZOrder(11);
    pwrScale->setMarks(7);
    pwrScale->setFontFamily(QStringLiteral("Trebuchet MS"));
    pwrScale->setFontSize(22.0f);
    pwrScale->setFontBold(true);
    pwrScale->setLowColour(Qt::gray);
    pwrScale->setHighColour(Qt::red);
    pwrScale->setMaxPower(150.0f);
    // Pair the power scale labels with the power/SWR needle group so
    // they hide in RX and in TX groups other than 1. Thetis's nspi has
    // no filter of its own (MeterManager.cs:22647+), but the ANANMM
    // port previously rendered the pwr labels over the S-meter scale
    // in RX — this is the deliberate NereusSDR fix for that overlap.
    pwrScale->setOnlyWhenTx(true);
    pwrScale->setDisplayGroup(1);
    // Same calibration as power needle
    {
        QMap<float, QPointF> cal;
        cal.insert(0.0f,   QPointF(0.099, 0.352));
        cal.insert(5.0f,   QPointF(0.164, 0.312));
        cal.insert(10.0f,  QPointF(0.224, 0.28));
        cal.insert(25.0f,  QPointF(0.335, 0.236));
        cal.insert(30.0f,  QPointF(0.367, 0.228));
        cal.insert(40.0f,  QPointF(0.436, 0.22));
        cal.insert(50.0f,  QPointF(0.499, 0.212));
        cal.insert(60.0f,  QPointF(0.559, 0.216));
        cal.insert(100.0f, QPointF(0.751, 0.272));
        cal.insert(150.0f, QPointF(0.899, 0.352));
        pwrScale->setScaleCalibration(cal);
    }
    group->addItem(pwrScale);

    // 7. SWR needle — bindingId=TxSwr, onlyWhenTx=true, displayGroup=1
    // From Thetis MeterManager.cs:22622-22645
    NeedleItem* swr = new NeedleItem();
    swr->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    swr->setBindingId(MeterBinding::TxSwr);
    swr->setZOrder(10);
    swr->setNeedleColor(Qt::black);
    swr->setLengthFactor(1.36f);
    swr->setNeedleOffset(sharedOffset);
    swr->setRadiusRatio(sharedRadius);
    swr->setOnlyWhenTx(true);
    swr->setDisplayGroup(1);
    swr->setHistoryEnabled(true);
    swr->setHistoryDuration(4000);
    swr->setHistoryColor(QColor(64, 100, 149, 237));  // Cornflower blue at alpha 64
    swr->setSourceLabel(QStringLiteral("SWR"));
    // 6-point calibration — from Thetis MeterManager.cs:22630-22635
    addCal(swr, {
        { 1.0f, QPointF(0.152, 0.468)},  { 1.5f, QPointF(0.28,  0.404)},
        { 2.0f, QPointF(0.393, 0.372)},  { 2.5f, QPointF(0.448, 0.36)},
        { 3.0f, QPointF(0.499, 0.36)},   {10.0f, QPointF(0.847, 0.476)}
    });
    group->addItem(swr);

    // 8. Compression needle — bindingId=TxAlcGain, onlyWhenTx=true, displayGroup=2
    // From Thetis MeterManager.cs:22647-22670
    NeedleItem* comp = new NeedleItem();
    comp->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    comp->setBindingId(MeterBinding::TxAlcGain);
    comp->setZOrder(10);
    comp->setNeedleColor(Qt::black);
    comp->setLengthFactor(0.96f);
    comp->setNeedleOffset(sharedOffset);
    comp->setRadiusRatio(sharedRadius);
    comp->setOnlyWhenTx(true);
    comp->setDisplayGroup(2);
    comp->setSourceLabel(QStringLiteral("Compression"));
    // 7-point calibration — from Thetis MeterManager.cs:22655-22661
    addCal(comp, {
        { 0.0f, QPointF(0.249, 0.68)},   { 5.0f, QPointF(0.342, 0.64)},
        {10.0f, QPointF(0.425, 0.624)},  {15.0f, QPointF(0.499, 0.62)},
        {20.0f, QPointF(0.571, 0.628)},  {25.0f, QPointF(0.656, 0.64)},
        {30.0f, QPointF(0.751, 0.688)}
    });
    group->addItem(comp);

    // 9. ALC needle — bindingId=TxAlcGroup, onlyWhenTx=true, displayGroup=3
    // From Thetis MeterManager.cs:22672-22690
    NeedleItem* alc = new NeedleItem();
    alc->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    alc->setBindingId(MeterBinding::TxAlcGroup);
    alc->setZOrder(10);
    alc->setNeedleColor(Qt::black);
    alc->setLengthFactor(0.75f);
    alc->setNeedleOffset(sharedOffset);
    alc->setRadiusRatio(sharedRadius);
    alc->setOnlyWhenTx(true);
    alc->setDisplayGroup(3);
    alc->setSourceLabel(QStringLiteral("ALC"));
    // 3-point calibration — from Thetis MeterManager.cs:22680-22682
    addCal(alc, {
        {-30.0f, QPointF(0.295, 0.804)},
        {  0.0f, QPointF(0.332, 0.784)},
        { 25.0f, QPointF(0.499, 0.756)}
    });
    group->addItem(alc);

    // Shrink the whole composite into the Thetis-nominal 0..0.441
    // normalized y band. The background image + every needle + the
    // power scale were authored above at (0, 0, 1, 1); scale them
    // all uniformly in y so the composite reserves only the top
    // 44.1% of the container (matching Thetis
    // MeterManager.cs:22472 `ni.Size = new SizeF(1f, 0.441f)`).
    // Because the needles' calibration points are in RECT-local
    // 0..1 space, scaling the rect down naturally pulls the needle
    // tips onto the correctly-sized image without changing any
    // calibration value. Bar rows appended to the container below
    // this composite by ContainerSettingsDialog will stack starting
    // at y=0.441 automatically (MeterWidget::reflowStackedItems
    // picks up the composite's bottom as the stack bandTop).
    constexpr float kAnanMMHeight = 0.441f;
    for (MeterItem* mi : group->items()) {
        if (!mi) { continue; }
        mi->setRect(mi->x(),
                    mi->y() * kAnanMMHeight,
                    mi->itemWidth(),
                    mi->itemHeight() * kAnanMMHeight);
    }

    return group;
}

// createCrossNeedlePreset
// From Thetis AddCrossNeedle() (MeterManager.cs:22817-23002)
// Dual crossing needles: forward power (Clockwise) and reverse power (CounterClockwise)
// with background images and NeedleScalePwrItem companions.
ItemGroup* ItemGroup::createCrossNeedlePreset(QObject* parent)
{
    ItemGroup* group = new ItemGroup(QStringLiteral("Cross Needle"), parent);

    // Helper: populate calibration map from initializer list
    // From Thetis MeterManager.cs calibration point arrays
    auto addCal = [](NeedleItem* ni, std::initializer_list<std::pair<float, QPointF>> points) {
        QMap<float, QPointF> cal;
        for (const auto& [val, pos] : points) {
            cal.insert(val, pos);
        }
        ni->setScaleCalibration(cal);
    };
    auto addCalScale = [](NeedleScalePwrItem* ni, std::initializer_list<std::pair<float, QPointF>> points) {
        QMap<float, QPointF> cal;
        for (const auto& [val, pos] : points) {
            cal.insert(val, pos);
        }
        ni->setScaleCalibration(cal);
    };

    // 1. Background image (z=1)
    // From Thetis MeterManager.cs:22817-22820
    ImageItem* bg = new ImageItem();
    bg->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    bg->setImagePath(QStringLiteral(":/meters/cross-needle.png"));
    bg->setZOrder(1);
    group->addItem(bg);

    // 2. Bottom band image (z=5)
    // From Thetis MeterManager.cs:22821-22823
    ImageItem* band = new ImageItem();
    band->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    band->setImagePath(QStringLiteral(":/meters/cross-needle-bg.png"));
    band->setZOrder(5);
    group->addItem(band);

    // Forward power calibration — from Thetis MeterManager.cs:22823-22862
    const std::initializer_list<std::pair<float, QPointF>> fwdCal = {
        {  0.0f, QPointF(0.052, 0.732)},
        {  5.0f, QPointF(0.146, 0.528)},
        { 10.0f, QPointF(0.188, 0.434)},
        { 15.0f, QPointF(0.235, 0.387)},
        { 20.0f, QPointF(0.258, 0.338)},
        { 25.0f, QPointF(0.303, 0.313)},
        { 30.0f, QPointF(0.321, 0.272)},
        { 35.0f, QPointF(0.361, 0.257)},
        { 40.0f, QPointF(0.381, 0.223)},
        { 50.0f, QPointF(0.438, 0.181)},
        { 60.0f, QPointF(0.483, 0.155)},
        { 70.0f, QPointF(0.532, 0.13)},
        { 80.0f, QPointF(0.577, 0.111)},
        { 90.0f, QPointF(0.619, 0.098)},
        {100.0f, QPointF(0.662, 0.083)}
    };

    // 3. Forward power needle — bindingId=TxPower, Clockwise
    // From Thetis MeterManager.cs:22864-22888
    NeedleItem* fwdNeedle = new NeedleItem();
    fwdNeedle->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    fwdNeedle->setBindingId(MeterBinding::TxPower);
    fwdNeedle->setZOrder(10);
    fwdNeedle->setDirection(NeedleItem::NeedleDirection::Clockwise);
    fwdNeedle->setNeedleOffset(QPointF(0.322, 0.611));
    fwdNeedle->setLengthFactor(1.62f);
    fwdNeedle->setStrokeWidth(2.5f);
    fwdNeedle->setNeedleColor(Qt::black);
    fwdNeedle->setHistoryEnabled(true);
    fwdNeedle->setHistoryDuration(4000);
    fwdNeedle->setHistoryColor(QColor(255, 0, 0, 96));
    fwdNeedle->setAttackRatio(0.2f);
    fwdNeedle->setDecayRatio(0.1f);
    fwdNeedle->setNormaliseTo100W(true);
    fwdNeedle->setSourceLabel(QStringLiteral("Fwd Power"));
    addCal(fwdNeedle, fwdCal);
    group->addItem(fwdNeedle);

    // 4. Forward power scale — NeedleScalePwrItem companion
    // From Thetis MeterManager.cs:22890-22893
    NeedleScalePwrItem* fwdScale = new NeedleScalePwrItem();
    fwdScale->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    fwdScale->setZOrder(11);
    fwdScale->setMarks(8);
    fwdScale->setFontFamily(QStringLiteral("Trebuchet MS"));
    fwdScale->setFontSize(16.0f);
    fwdScale->setFontBold(true);
    fwdScale->setLowColour(Qt::gray);
    fwdScale->setHighColour(Qt::red);
    fwdScale->setDirection(NeedleScalePwrItem::Direction::Clockwise);
    fwdScale->setNeedleOffset(QPointF(0.318, 0.611));
    fwdScale->setLengthFactor(1.685f);
    addCalScale(fwdScale, fwdCal);
    group->addItem(fwdScale);

    // Reverse power calibration — from Thetis MeterManager.cs:22894-22937
    const std::initializer_list<std::pair<float, QPointF>> revCal = {
        { 0.00f, QPointF(0.948, 0.74)},
        { 0.25f, QPointF(0.913, 0.7)},
        { 0.50f, QPointF(0.899, 0.638)},
        { 0.75f, QPointF(0.875, 0.594)},
        { 1.00f, QPointF(0.854, 0.538)},
        { 2.00f, QPointF(0.814, 0.443)},
        { 3.00f, QPointF(0.769, 0.4)},
        { 4.00f, QPointF(0.744, 0.351)},
        { 5.00f, QPointF(0.702, 0.321)},
        { 6.00f, QPointF(0.682, 0.285)},
        { 7.00f, QPointF(0.646, 0.268)},
        { 8.00f, QPointF(0.626, 0.234)},
        { 9.00f, QPointF(0.596, 0.228)},
        {10.00f, QPointF(0.569, 0.196)},
        {12.00f, QPointF(0.524, 0.166)},
        {14.00f, QPointF(0.476, 0.14)},
        {16.00f, QPointF(0.431, 0.121)},
        {18.00f, QPointF(0.393, 0.109)},
        {20.00f, QPointF(0.349, 0.098)}
    };

    // 5. Reverse power needle — bindingId=TxReversePower, CounterClockwise
    // From Thetis MeterManager.cs:22938-22968
    NeedleItem* revNeedle = new NeedleItem();
    revNeedle->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    revNeedle->setBindingId(MeterBinding::TxReversePower);
    revNeedle->setZOrder(10);
    revNeedle->setDirection(NeedleItem::NeedleDirection::CounterClockwise);
    revNeedle->setNeedleOffset(QPointF(-0.322, 0.611));  // Negative X creates mirror
    revNeedle->setLengthFactor(1.62f);
    revNeedle->setStrokeWidth(2.5f);
    revNeedle->setNeedleColor(Qt::black);
    revNeedle->setHistoryEnabled(true);
    revNeedle->setHistoryColor(QColor(100, 149, 237, 96));  // Cornflower blue alpha
    revNeedle->setAttackRatio(0.2f);
    revNeedle->setDecayRatio(0.1f);
    revNeedle->setNormaliseTo100W(true);
    revNeedle->setSourceLabel(QStringLiteral("Rev Power"));
    addCal(revNeedle, revCal);
    group->addItem(revNeedle);

    // 6. Reverse power scale — NeedleScalePwrItem companion, CounterClockwise
    // From Thetis MeterManager.cs:22970-22974
    NeedleScalePwrItem* revScale = new NeedleScalePwrItem();
    revScale->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    revScale->setZOrder(11);
    revScale->setMarks(8);
    revScale->setFontFamily(QStringLiteral("Trebuchet MS"));
    revScale->setFontSize(16.0f);
    revScale->setFontBold(true);
    revScale->setLowColour(Qt::gray);
    revScale->setHighColour(Qt::red);
    revScale->setDirection(NeedleScalePwrItem::Direction::CounterClockwise);
    revScale->setNeedleOffset(QPointF(-0.322, 0.611));
    revScale->setLengthFactor(1.685f);
    addCalScale(revScale, revCal);
    group->addItem(revScale);

    return group;
}

// createMagicEyePreset
// Single MagicEyeItem fills the full group rect.
// From Thetis MeterManager.cs clsMagicEye usage.
ItemGroup* ItemGroup::createMagicEyePreset(int bindingId, QObject* parent)
{
    ItemGroup* group = new ItemGroup(QStringLiteral("Magic Eye"), parent);

    MagicEyeItem* eye = new MagicEyeItem();
    eye->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    eye->setBindingId(bindingId);
    eye->setZOrder(5);
    group->addItem(eye);

    return group;
}

// createSignalTextPreset
// Dark background + large SignalTextItem (S-units mode).
// From Thetis MeterManager.cs clsSignalText (line 20286+).
// Yellow text (#ffff00) from Thetis line 21708.
ItemGroup* ItemGroup::createSignalTextPreset(int bindingId, QObject* parent)
{
    ItemGroup* group = new ItemGroup(QStringLiteral("Signal Text"), parent);

    // Background
    SolidColourItem* bg = new SolidColourItem();
    bg->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    bg->setColour(QColor(0x0f, 0x0f, 0x1a));
    bg->setZOrder(0);
    group->addItem(bg);

    SignalTextItem* text = new SignalTextItem();
    text->setRect(0.02f, 0.05f, 0.96f, 0.9f);
    text->setBindingId(bindingId);
    text->setUnits(SignalTextItem::Units::SUnits);
    text->setFontSize(40.0f);
    text->setColour(QColor(0xff, 0xff, 0x00));  // Yellow (from Thetis line 21708)
    text->setZOrder(5);
    group->addItem(text);

    return group;
}

// createHistoryPreset
// Dark background + HistoryGraphItem (top 80%) + TextItem readout (bottom 20%).
// From Thetis MeterManager.cs clsHistoryItem (line 16149+).
ItemGroup* ItemGroup::createHistoryPreset(int bindingId, QObject* parent)
{
    ItemGroup* group = new ItemGroup(QStringLiteral("History"), parent);

    // Background
    SolidColourItem* bg = new SolidColourItem();
    bg->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    bg->setColour(QColor(0x0a, 0x0a, 0x18));
    bg->setZOrder(0);
    group->addItem(bg);

    // History graph (top 80%)
    HistoryGraphItem* hist = new HistoryGraphItem();
    hist->setRect(0.0f, 0.0f, 1.0f, 0.8f);
    hist->setBindingId(bindingId);
    hist->setCapacity(300);
    hist->setZOrder(5);
    group->addItem(hist);

    // Text readout (bottom 20%)
    TextItem* readout = new TextItem();
    readout->setRect(0.02f, 0.8f, 0.96f, 0.2f);
    readout->setBindingId(bindingId);
    readout->setTextColor(QColor(0xc8, 0xd8, 0xe8));
    readout->setFontSize(13);
    readout->setBold(true);
    readout->setSuffix(QStringLiteral(" dBm"));
    readout->setDecimals(1);
    readout->setZOrder(10);
    group->addItem(readout);

    return group;
}

// createSpacerPreset
// Invisible spacer that occupies the full group rect.
ItemGroup* ItemGroup::createSpacerPreset(QObject* parent)
{
    ItemGroup* group = new ItemGroup(QStringLiteral("Spacer"), parent);

    SpacerItem* spacer = new SpacerItem();
    spacer->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    spacer->setZOrder(0);
    group->addItem(spacer);

    return group;
}

// ============================================================================
// Phase 3G-5 Interactive Presets
// ============================================================================

ItemGroup* ItemGroup::createVfoDisplayPreset(QObject* parent)
{
    auto* group = new ItemGroup(QStringLiteral("VFO Display"), parent);
    auto* bg = new SolidColourItem(group);
    bg->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    bg->setColour(QColor(0x0f, 0x0f, 0x1a));
    bg->setZOrder(0);
    group->addItem(bg);
    auto* vfo = new VfoDisplayItem(group);
    vfo->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    vfo->setZOrder(1);
    group->addItem(vfo);
    return group;
}

ItemGroup* ItemGroup::createClockPreset(QObject* parent)
{
    // Clock preset is a single narrow strip at the top of its target
    // region (0..0.15 normalized). The previous version installed
    // both the background and the ClockItem at full container size
    // (0,0,1,1), which collided with auto-stacking heuristics: every
    // subsequent item the user added landed on top of the clock
    // instead of beside it. Now the bg + clock share the same narrow
    // strip and the auto-stacker treats them as a real stack member.
    auto* group = new ItemGroup(QStringLiteral("Clock"), parent);
    auto* bg = new SolidColourItem(group);
    bg->setRect(0.0f, 0.0f, 1.0f, 0.15f);
    bg->setColour(QColor(0x0f, 0x0f, 0x1a));
    bg->setZOrder(0);
    group->addItem(bg);
    auto* clock = new ClockItem(group);
    clock->setRect(0.0f, 0.0f, 1.0f, 0.15f);
    clock->setZOrder(1);
    group->addItem(clock);
    return group;
}

ItemGroup* ItemGroup::createContestPreset(QObject* parent)
{
    auto* group = new ItemGroup(QStringLiteral("Contest"), parent);
    auto* bg = new SolidColourItem(group);
    bg->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    bg->setColour(QColor(0x0f, 0x0f, 0x1a));
    bg->setZOrder(0);
    group->addItem(bg);
    auto* vfo = new VfoDisplayItem(group);
    vfo->setRect(0.0f, 0.0f, 1.0f, 0.3f);
    vfo->setZOrder(1);
    group->addItem(vfo);
    auto* bands = new BandButtonItem(group);
    bands->setRect(0.0f, 0.3f, 1.0f, 0.25f);
    bands->setZOrder(1);
    group->addItem(bands);
    auto* modes = new ModeButtonItem(group);
    modes->setRect(0.0f, 0.55f, 1.0f, 0.2f);
    modes->setZOrder(1);
    group->addItem(modes);
    auto* clock = new ClockItem(group);
    clock->setRect(0.0f, 0.75f, 1.0f, 0.25f);
    clock->setZOrder(1);
    group->addItem(clock);
    return group;
}

} // namespace NereusSDR
