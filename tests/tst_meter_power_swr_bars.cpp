// =================================================================
// tests/tst_meter_power_swr_bars.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Bars whose flavor name joins words take Thetis's caption
//                 (the reading name); a saved label is kept. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Standalone PWR, Reverse PWR and SWR bars: readings,
//                 calibration, power limit, Thetis defaults, and the
//                 meter.adcMax bar created in a container. J.J. Boyd
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


#include <QtTest/QtTest>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <memory>

#include "gui/containers/ContainerContentRegistry.h"
#include "gui/meters/MeterPoller.h"  // MeterBinding
#include "gui/meters/MeterWidget.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/presets/CrossNeedleItem.h"
#include "gui/meters/presets/PowerSwrPresetItem.h"
#include "gui/meters/presets/PresetGeometry.h"

using namespace NereusSDR;

namespace {
std::unique_ptr<BarPresetItem> newBar(const ContainerContentRegistry& registry, const QString& typeId)
{
    const ContentEntry entry = registry.makeEntry(typeId);
    std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
    auto* bar = qobject_cast<BarPresetItem*>(item.get());
    if (!bar) { return {}; }
    item.release();
    return std::unique_ptr<BarPresetItem>(bar);
}
// Settle the bar on one reading: attack 1 takes it at once.
void settle(BarPresetItem& bar, double value)
{
    QVERIFY(bar.applyConfiguration({{"attack", 1.0}}));
    bar.pushBindingValue(bar.bindingId(), value);
    bar.advanceMeter(0);
}
// The x of the marker's yellow column, halfway down the bar.
double paintedMarkerX(BarPresetItem& bar, int width, int height)
{
    bar.setRect(0, 0, 1, 1);
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    {
        QPainter painter(&image);
        bar.paint(painter, width, height);
    }
    const PresetGeometry g(QRect(0, 0, width, height));  // setRect(0, 0, 1, 1)
    const int row = int((g.top + g.baseline) / 2);
    double sum = 0;
    int count = 0;
    for (int x = 0; x < width; ++x) {
        const QColor c = image.pixelColor(x, row);
        if (c.red() > 200 && c.green() > 200 && c.blue() < 80) { sum += x; ++count; }
    }
    return count ? sum / count : -1;
}
double expectedX(int width, int height, double position)
{
    const PresetGeometry g(QRect(0, 0, width, height));  // setRect(0, 0, 1, 1)
    return g.left + g.width * position;
}
}

class TestMeterPowerSwrBars : public QObject {
    Q_OBJECT
private slots:
    void typeIdsAndTitles()
    {
        ContainerContentRegistry registry;
        const QHash<QString, QString> titles{{"meter.pwr", "Power bar"}, {"meter.reversePwr", "Reverse power bar"}, {"meter.swr", "SWR bar"}};
        for (auto it = titles.begin(); it != titles.end(); ++it) {
            bool found = false;
            for (const auto& d : registry.descriptors()) {
                if (d.typeId == it.key()) { found = true; QCOMPARE(d.title, it.value()); QVERIFY(d.available); }
            }
            QVERIFY2(found, qPrintable(it.key()));
            auto bar = newBar(registry, it.key());
            QVERIFY2(bar, qPrintable(it.key()));
            QCOMPARE(bar->typeId(), it.key());
        }
    }
    // Each bar reads what the matching composite channel reads.
    void bindsTheCompositeReadings()
    {
        ContainerContentRegistry registry;
        PowerSwrPresetItem power;
        const QJsonArray channels = power.configuration()["channels"].toArray();
        QCOMPARE(channels.size(), 2);
        auto pwr = newBar(registry, "meter.pwr");
        auto swr = newBar(registry, "meter.swr");
        auto reverse = newBar(registry, "meter.reversePwr");
        QCOMPARE(pwr->bindingId(), power.bindingId());
        QCOMPARE(pwr->bindingId(), int(MeterBinding::TxPower));
        QCOMPARE(swr->bindingId(), channels[1].toObject()["bindingId"].toInt());
        QCOMPARE(swr->bindingId(), int(MeterBinding::TxSwr));
        // Power/SWR has no reverse channel; the Cross needle's reflected one is it.
        CrossNeedleItem cross;
        QVERIFY(cross.readingBindings().contains(MeterBinding::TxReversePower));
        QCOMPARE(reverse->bindingId(), int(MeterBinding::TxReversePower));
        QCOMPARE(pwr->readingBindings(), (QSet<int>{MeterBinding::TxPower}));
        QCOMPARE(swr->readingBindings(), (QSet<int>{MeterBinding::TxSwr}));
        QCOMPARE(reverse->readingBindings(), (QSet<int>{MeterBinding::TxReversePower}));
        pwr->pushBindingValue(MeterBinding::TxPower, 40);
        pwr->advanceMeter(0);
        QVERIFY(pwr->hasPrimaryReading());
    }
    void pwrLimit100Reads50WAtItsCalibrationPoint()
    {
        ContainerContentRegistry registry;
        auto pwr = newBar(registry, "meter.pwr");
        QVERIFY(pwr->applyConfiguration({{"powerLimitW", 100.0}}));
        QCOMPARE(pwr->effectivePowerLimit(), 100.0);
        QCOMPARE(pwr->calibratedPosition(50), 0.5625);
        for (const auto& [watts, position] : std::initializer_list<std::pair<double, double>>{{0, 0}, {5, .1875}, {10, .375}, {100, .75}, {120, .99}, {500, .99}}) {
            QCOMPARE(pwr->calibratedPosition(watts), position);
        }
        QCOMPARE(pwr->calibratedPosition(75), 0.5625 + (0.75 - 0.5625) * 0.5);
        settle(*pwr, 50);
        QCOMPARE(pwr->primaryValue(), 50.0);
        QVERIFY(qAbs(paintedMarkerX(*pwr, 520, 144) - expectedX(520, 144, .5625)) <= 2.0);
        // A 200 W limit puts 100 W where 50 W was.
        QVERIFY(pwr->applyConfiguration({{"powerLimitW", 200.0}}));
        QCOMPARE(pwr->calibratedPosition(100), 0.5625);
        QCOMPARE(pwr->configuration()["maxValue"].toDouble(), 240.0);
        QCOMPARE(pwr->configuration()["redThreshold"].toDouble(), 200.0);
    }
    // Power limit 0 follows the radio's rating (MaxPower recovery).
    void zeroLimitFollowsTheRadioRating()
    {
        ContainerContentRegistry registry;
        auto pwr = newBar(registry, "meter.reversePwr");
        QCOMPARE(pwr->configuration()["powerLimitW"].toDouble(), 0.0);
        QCOMPARE(pwr->effectivePowerLimit(), kDefaultBarPowerLimitW);
        pwr->setPowerScale(500);
        QCOMPARE(pwr->effectivePowerLimit(), 500.0);
        QCOMPARE(pwr->calibratedPosition(250), 0.5625);
        QVERIFY(pwr->applyConfiguration({{"powerLimitW", 10.0}}));
        QCOMPARE(pwr->calibratedPosition(5), 0.5625);
        pwr->setPowerScale(1000);
        QCOMPARE(pwr->effectivePowerLimit(), 10.0);
        // The limit survives a save and reload.
        BarPresetItem loaded;
        QVERIFY(loaded.deserialize(pwr->serialize()));
        QCOMPARE(loaded.effectivePowerLimit(), 10.0);
        // Outside the Setup range is refused and changes nothing.
        const QString before = pwr->serialize();
        QVERIFY(!pwr->applyConfiguration({{"powerLimitW", -5.0}}));
        QVERIFY(!pwr->applyConfiguration({{"powerLimitW", 6000.0}}));
        QVERIFY(!pwr->applyConfiguration({{"powerLimitW", QStringLiteral("100")}}));
        QCOMPARE(pwr->serialize(), before);
    }
    void swr2ReadsAtItsCalibrationPoint()
    {
        ContainerContentRegistry registry;
        auto swr = newBar(registry, "meter.swr");
        QCOMPARE(swr->calibratedPosition(2.0), 0.5);
        for (const auto& [ratio, position] : std::initializer_list<std::pair<double, double>>{{1, 0}, {1.5, .25}, {3, .75}, {5, .99}, {9, .99}, {.5, 0}}) {
            QCOMPARE(swr->calibratedPosition(ratio), position);
        }
        // SWR ignores the power limit.
        swr->setPowerScale(500);
        QCOMPARE(swr->calibratedPosition(2.0), 0.5);
        QVERIFY(!swr->configuration().contains("powerLimitW"));
        settle(*swr, 2.0);
        QVERIFY(qAbs(paintedMarkerX(*swr, 520, 144) - expectedX(520, 144, .5)) <= 2.0);
    }
    void newPlacementsTakeThetisValues()
    {
        ContainerContentRegistry registry;
        for (const QString& typeId : {QStringLiteral("meter.pwr"), QStringLiteral("meter.reversePwr"), QStringLiteral("meter.swr")}) {
            const QJsonObject p = registry.makeEntry(typeId).config["properties"].toObject();
            QCOMPARE(p["attack"].toDouble(), 0.8);
            QCOMPARE(p["decay"].toDouble(), 0.1);
            QCOMPARE(p["historyMs"].toInt(), 2000);
            QCOMPARE(p["showHistory"].toBool(), true);
            QCOMPARE(p["barColor"].toString(), QStringLiteral("#ffffff00"));
            QCOMPARE(p["style"].toString(), QStringLiteral("Line"));
            QCOMPARE(p["backdropColor"].toString(), QStringLiteral("#ff202020"));
            QCOMPARE(p["segLowColor"].toString(), QStringLiteral("#ffff0000"));
            QCOMPARE(p["segHighColor"].toString(), QStringLiteral("#ffffa500"));
            QCOMPARE(p["showTitle"].toBool(), true);
            QCOMPARE(p["historyColor"].toString(), typeId == "meter.swr" ? QStringLiteral("#80ffa500") : QStringLiteral("#80ff0000"));
        }
        auto pwr = newBar(registry, "meter.pwr");
        // High point: calibration index 4 (100 % of the limit) and index 3 (3:1).
        QCOMPARE(pwr->calibratedPosition(pwr->configuration()["redThreshold"].toDouble()), 0.75);
        QCOMPARE(pwr->configuration()["units"].toString(), QStringLiteral("W"));
        QCOMPARE(pwr->configuration()["label"].toString(), QStringLiteral("Power"));
        auto swr = newBar(registry, "meter.swr");
        QCOMPARE(swr->configuration()["redThreshold"].toDouble(), 3.0);
        QCOMPARE(swr->configuration()["units"].toString(), QStringLiteral(":1"));
        QCOMPARE(newBar(registry, "meter.reversePwr")->configuration()["label"].toString(), QStringLiteral("Reverse Power"));
        // The styles and segment colours apply to these bars too.
        QVERIFY(swr->applyConfiguration({{"style", "Segments"}, {"segLowColor", "#ff00ff00"}}));
        QCOMPARE(swr->configuration()["style"].toString(), QStringLiteral("Segments"));
    }
    // A new bar whose flavor name joins words takes the caption Thetis draws
    // over it, its reading's name (From Thetis MeterManager.cs:2269-2313,
    // 33413-33420 [v2.10.3.15]); a saved layout keeps its stored label.
    void joinedFlavorsTakeThetisCaptions()
    {
        const QList<QPair<QString, QString>> captions{
            {"SignalAvg", "Signal Average"}, {"SignalMaxBin", "Signal Max FFT Bin"},
            {"CfcGain", "CFC Compression"}, {"LevelerGain", "Leveler Gain"},
            {"AlcGain", "ALC Compression"}, {"AlcGroup", "ALC Group"},
            {"AgcGain", "AGC Gain"}, {"AdcMax", "ADC Max Magnitude"},
            {"PbSnr", "Estimated PBSNR"}, {"ReversePwr", "Reverse Power"}};
        for (const auto& [flavor, caption] : captions) {
            BarPresetItem bar;
            QVERIFY2(bar.configureVariant(flavor), qPrintable(flavor));
            QCOMPARE(bar.configuration()["label"].toString(), caption);
        }
        // Single-word flavors keep their upper-case caption.
        BarPresetItem comp;
        QVERIFY(comp.configureVariant("Comp"));
        QCOMPARE(comp.configuration()["label"].toString(), QStringLiteral("COMP"));
        // A layout saved before keeps the label it stored.
        BarPresetItem saved;
        QVERIFY(saved.configureVariant("SignalMaxBin"));
        QJsonObject record = saved.configuration();
        record["label"] = QStringLiteral("SIGNALMAXBIN");
        BarPresetItem loaded;
        QVERIFY(loaded.deserialize(QString::fromUtf8(QJsonDocument(record).toJson(QJsonDocument::Compact))));
        QCOMPARE(loaded.configuration()["label"].toString(), QStringLiteral("SIGNALMAXBIN"));
    }
    void existingRecordsAreUnchanged()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry("meter.powerSwr");
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        QVERIFY(qobject_cast<PowerSwrPresetItem*>(item.get()));
        const ContentEntry saved = registry.captureMeterItem(*item, entry);
        QCOMPARE(saved.config["legacyRecord"], entry.config["legacyRecord"]);
        QCOMPARE(saved.config["properties"], entry.config["properties"]);
        // Other bars carry no power limit.
        QVERIFY(!registry.makeEntry("meter.comp").config["properties"].toObject().contains("powerLimitW"));
        BarPresetItem comp;
        QVERIFY(comp.configureVariant("Comp"));
        QVERIFY(!comp.configuration().contains("powerLimitW"));
    }
    // In a container, meter.adcMax is its bar with the one reason, takes no
    // reading and reads no channel.
    void adcMaxInAContainerVeilsWithOneReason()
    {
        const QString reason = QStringLiteral("ADC magnitude is not available");
        ContainerContentRegistry registry;
        bool found = false;
        for (const auto& d : registry.descriptors()) {
            if (d.typeId == "meter.adcMax") { found = true; QVERIFY(d.available); QVERIFY(d.unavailableReason.isEmpty()); }
        }
        QVERIFY(found);
        const ContentEntry entry = registry.makeEntry("meter.adcMax");
        MeterWidget meter;
        meter.resize(300, 80);
        MeterItem* item = registry.createMeterItem(entry, &meter);
        auto* bar = qobject_cast<BarPresetItem*>(item);
        QVERIFY(bar);
        bar->setRect(0, 0, 1, 1);
        meter.addItem(bar);
        QCOMPARE(meter.unavailableReasonAt(QPointF(150, 40)), reason);
        QCOMPARE(bar->readingBindings(), (QSet<int>{-1}));
        for (int binding : {-1, int(MeterBinding::AdcPeak), int(MeterBinding::AdcAvg)}) { bar->pushBindingValue(binding, 20000); }
        bar->setValue(20000);
        meter.updateMeterValue(MeterBinding::AdcPeak, -10);
        bar->advanceMeter(0);
        QVERIFY(!bar->hasPrimaryReading());
        QVERIFY(!bar->hasAverageReading());
    }
};

QTEST_MAIN(TestMeterPowerSwrBars)
#include "tst_meter_power_swr_bars.moc"
