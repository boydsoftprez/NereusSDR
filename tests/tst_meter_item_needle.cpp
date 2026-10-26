// =================================================================
// tests/tst_meter_item_needle.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - NEEDLE and NEEDLESCALEPWR building blocks: shadow, peak
//                 hold and its colour, the history arc, ignore history, the
//                 power limit, the saved record and the new-item defaults.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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

#include <QtTest/QtTest>
#include <QColor>
#include <QDoubleSpinBox>
#include <QImage>
#include <QPainter>

#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/meter_property_editors/NeedleScalePwrItemEditor.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"  // MeterBinding namespace
#include "gui/meters/NeedleScalePwrItem.h"

#include <memory>

using namespace NereusSDR;

namespace {

constexpr int kW = 400;
constexpr int kH = 200;

// NEEDLE records as serialize() wrote them before the needle options were
// appended: the built-in S-meter arc, and a calibrated face.
const QString kSavedArcNeedle = QStringLiteral(
    "NEEDLE|0|0|1|1|0|0|S-Meter|0|0|0"
    "|0"
    "|#ffffffff|0|0|1|1|1|0|0|100|0.3|0.3");
const QString kSavedCalibratedNeedle = QStringLiteral(
    "NEEDLE|0|0|1|1|100|0|Power|0|0|0"
    "|2|0:0.1:0.2|100:0.9:0.2"
    "|#ffffffff|0.5|0.9|1|1|1|0|1|100|0.3|0.3");

// 0 at the left, 100 at the right, along a row near the top; the pivot sits
// low in the middle.
QMap<float, QPointF> flatCalibration()
{
    QMap<float, QPointF> cal;
    cal.insert(0.0f, QPointF(0.1, 0.2));
    cal.insert(100.0f, QPointF(0.9, 0.2));
    return cal;
}

void makeCalibrated(NeedleItem& n)
{
    n.setRect(0.0f, 0.0f, 1.0f, 1.0f);
    n.setScaleCalibration(flatCalibration());
    n.setNeedleOffset(QPointF(0.5, 0.9));
}

void settle(NeedleItem& n, double v, int count = 60)
{
    for (int i = 0; i < count; ++i) { n.setValue(v); }
}

// Vertices (x, y, r, g, b, a) whose colour is c.
int countVerts(const QVector<float>& verts, const QColor& c)
{
    int n = 0;
    for (int i = 0; i + 5 < verts.size(); i += 6) {
        if (std::abs(verts[i + 2] - static_cast<float>(c.redF())) < 0.01f
            && std::abs(verts[i + 3] - static_cast<float>(c.greenF())) < 0.01f
            && std::abs(verts[i + 4] - static_cast<float>(c.blueF())) < 0.01f
            && std::abs(verts[i + 5] - static_cast<float>(c.alphaF())) < 0.01f) {
            ++n;
        }
    }
    return n;
}

QVector<float> vertsOf(NeedleItem& n)
{
    QVector<float> v;
    n.emitVertices(v, kW, kH);
    return v;
}

QImage paintOf(NeedleItem& n, const QColor& background = Qt::black)
{
    QImage img(kW, kH, QImage::Format_ARGB32_Premultiplied);
    img.fill(background);
    QPainter p(&img);
    n.paint(p, kW, kH);
    p.end();
    return img;
}

QPointF midpoint(const QPointF& a, const QPointF& b)
{
    return (a + b) / 2.0;
}

const QColor kShadow(0, 0, 0, 80);
const QColor kAmber(0xff, 0xaa, 0x00);

} // namespace

class TstMeterItemNeedle : public QObject {
    Q_OBJECT

private slots:
    void shadowOff_removesShadow()
    {
        NeedleItem n;
        makeCalibrated(n);
        settle(n, 50.0);
        QCOMPARE(countVerts(vertsOf(n), kShadow), 6);
        // On grey, so the translucent black shadow shows.
        const QImage withShadow = paintOf(n, Qt::gray);

        n.setShadow(false);
        QCOMPARE(countVerts(vertsOf(n), kShadow), 0);
        QVERIFY(paintOf(n, Qt::gray) != withShadow);
        // The needle itself still draws.
        QCOMPARE(countVerts(vertsOf(n), QColor(Qt::white)), 6);
    }

    void peakHoldOn_drawsPeakMarkerInItsColour()
    {
        const QColor blue(0, 0, 255);
        NeedleItem n;
        makeCalibrated(n);
        n.setHistoryDuration(30000);
        n.setPeakHold(true);
        n.setPeakHoldColour(blue);
        settle(n, 90.0);
        settle(n, 20.0, 30);
        QVERIFY(n.peakValue() > 85.0f);
        QVERIFY(n.smoothedValue() < 25.0f);

        // A peak needle (one line, two triangles) in the peak colour.
        QCOMPARE(countVerts(vertsOf(n), blue), 6);
        const QPointF mid = midpoint(n.needlePivot(kW, kH), n.needleTipAt(n.peakValue(), kW, kH));
        const QRgb px = paintOf(n).pixel(mid.toPoint());
        QVERIFY2(qBlue(px) > 150 && qRed(px) < 100 && qGreen(px) < 100,
                 qPrintable(QStringLiteral("peak pixel %1").arg(px, 8, 16)));

        n.setPeakHold(false);
        QCOMPARE(countVerts(vertsOf(n), blue), 0);
        QCOMPARE(qBlue(paintOf(n).pixel(mid.toPoint())), 0);
    }

    void peakHoldOn_arcMarkerTakesItsColour()
    {
        const QColor green(0, 200, 0);
        NeedleItem n;
        n.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        n.setHistoryDuration(30000);
        n.setPeakHold(true);
        n.setPeakHoldColour(green);
        settle(n, -50.0);
        settle(n, -110.0, 10);
        QCOMPARE(countVerts(vertsOf(n), green), 3);
        QCOMPARE(countVerts(vertsOf(n), kAmber), 0);
        n.setPeakHold(false);
        QCOMPARE(countVerts(vertsOf(n), green), 0);
    }

    void showHistory_drawsArcForTheDuration()
    {
        const QColor histColour(0, 255, 0, 128);
        NeedleItem n;
        makeCalibrated(n);
        n.setUpdateIntervalMs(100);
        n.setPeakHold(false);
        n.setHistoryColor(histColour);
        n.setHistoryDuration(500);
        settle(n, 90.0, 20);
        // 500 ms at 100 ms readings keeps five.
        QCOMPARE(n.historySize(), 5);
        n.setHistoryDuration(1000);
        settle(n, 90.0, 20);
        QCOMPARE(n.historySize(), 10);

        // Off: no arc.
        n.setHistoryDuration(30000);
        settle(n, 20.0, 30);
        QCOMPARE(countVerts(vertsOf(n), histColour), 0);

        n.setShowHistory(true);
        QVERIFY(n.showHistory());
        QVERIFY(countVerts(vertsOf(n), histColour) > 0);
        QVERIFY(n.minHistory() < 25.0f);
        QVERIFY(n.maxHistory() > 85.0f);

        // Inside the arc, half way along the needle at 55.
        const QPointF inside = midpoint(n.needlePivot(kW, kH), n.needleTipAt(55.0f, kW, kH));
        const QRgb px = paintOf(n).pixel(inside.toPoint());
        QVERIFY2(qGreen(px) > 60 && qRed(px) < 30 && qBlue(px) < 30,
                 qPrintable(QStringLiteral("history pixel %1").arg(px, 8, 16)));

        // A shorter duration narrows the arc to the recent readings.
        n.setHistoryDuration(500);
        n.setValue(20.0);
        QCOMPARE(n.historySize(), 5);
        QVERIFY(n.maxHistory() < 25.0f);
        QCOMPARE(qGreen(paintOf(n).pixel(inside.toPoint())), 0);
    }

    void ignoreHistory_dropsReadingsForTheTimeSet()
    {
        NeedleItem n;
        makeCalibrated(n);
        n.setUpdateIntervalMs(100);
        n.setHistoryDuration(30000);
        n.setIgnoreHistoryMs(2000);
        settle(n, 30.0, 5);
        QCOMPARE(n.historySize(), 5);

        n.clearHistory();
        QCOMPARE(n.historySize(), 0);
        QCOMPARE(n.ignoringReadings(), 20);
        const float before = n.smoothedValue();
        settle(n, 80.0, 20);
        // The needle moved; the history and the peak did not gather.
        QVERIFY(n.smoothedValue() > before + 10.0f);
        QCOMPARE(n.historySize(), 0);
        QCOMPARE(n.ignoringReadings(), 0);
        n.setValue(80.0);
        QCOMPARE(n.historySize(), 1);

        // Clamped to Thetis's 0..4000 ms.
        n.setIgnoreHistoryMs(9000);
        QCOMPARE(n.ignoreHistoryMs(), 4000);
        n.setIgnoreHistoryMs(-5);
        QCOMPARE(n.ignoreHistoryMs(), 0);
    }

    void powerLimit50_normalises25WToTheHalfwayPoint()
    {
        NeedleItem n;
        makeCalibrated(n);
        n.setBindingId(MeterBinding::TxPower);
        n.setNormaliseTo100W(true);
        n.setPowerLimit(50.0);
        QCOMPARE(n.powerLimit(), 50.0);
        QCOMPARE(n.scaleValue(25.0f), 50.0f);

        NeedleItem plain;
        makeCalibrated(plain);
        plain.setBindingId(MeterBinding::TxPower);
        QCOMPARE(n.needleTipAt(25.0f, kW, kH), plain.needleTipAt(50.0f, kW, kH));
        // The 50% point of the 0..100 face: x = 0.5.
        QCOMPARE(n.needleTipAt(25.0f, kW, kH).x(), kW * 0.5);

        // A limit over 100 W lets the needle travel past 100 W.
        n.setPowerLimit(200.0);
        settle(n, 150.0);
        QVERIFY(n.smoothedValue() > 140.0f);
        QCOMPARE(n.scaleValue(150.0f), 75.0f);

        // Normalise off: the limit does not apply.
        n.setNormaliseTo100W(false);
        QCOMPARE(n.scaleValue(25.0f), 25.0f);

        // Thetis's 0.1 .. 5000 W; below 0.1 recovers to 100 W.
        n.setPowerLimit(0.1);
        QCOMPARE(n.powerLimit(), static_cast<double>(0.1f));
        n.setPowerLimit(0.0);
        QCOMPARE(n.powerLimit(), 100.0);
        n.setPowerLimit(9000.0);
        QCOMPARE(n.powerLimit(), 5000.0);
    }

    void vhfS9Rule_keepsWorkingOnASignalFace()
    {
        NeedleItem n;
        makeCalibrated(n);
        n.setBindingId(MeterBinding::SignalPeak);
        n.setNormaliseTo100W(true);
        n.setPowerLimit(50.0);
        // A signal face takes the S9 rule, not the power limit.
        QCOMPARE(n.scaleValue(-93.0f), -93.0f);
        n.setAboveS9Frequency(true);
        QCOMPARE(n.scaleValue(-93.0f), -73.0f);
    }

    void needleScalePwr_acceptsAPowerLimitOfPoint1()
    {
        NeedleScalePwrItem s;
        s.setMaxPower(0.1f);
        QCOMPARE(s.maxPower(), 0.1f);
        NeedleScalePwrItem loaded;
        QVERIFY(loaded.deserialize(s.serialize()));
        QCOMPARE(loaded.maxPower(), 0.1f);
        s.setMaxPower(0.0f);
        QCOMPARE(s.maxPower(), 100.0f);

        // The editor row goes down to 0.1 W.
        NeedleScalePwrItem item;
        item.setMaxPower(0.1f);
        NeedleScalePwrItemEditor editor;
        editor.setItem(&item);
        bool found = false;
        for (QDoubleSpinBox* spin : editor.findChildren<QDoubleSpinBox*>()) {
            if (qFuzzyCompare(spin->minimum(), 0.1) && qFuzzyCompare(spin->maximum(), 5000.0)) {
                QCOMPARE(spin->value(), 0.1);
                found = true;
            }
        }
        QVERIFY(found);
    }

    void oldRecord_paintsTheSame()
    {
        // The built-in arc: shadow, white needle and the amber peak triangle.
        NeedleItem arc;
        QVERIFY(arc.deserialize(kSavedArcNeedle));
        QVERIFY(arc.shadow());
        QVERIFY(arc.peakHold());
        QCOMPARE(arc.historyEnabled(), false);
        QCOMPARE(arc.ignoreHistoryMs(), 0);
        settle(arc, -50.0);
        settle(arc, -110.0, 3);
        const QVector<float> arcVerts = vertsOf(arc);
        QCOMPARE(arcVerts.size(), 15 * 6);
        QCOMPARE(countVerts(arcVerts, kShadow), 6);
        QCOMPARE(countVerts(arcVerts, QColor(Qt::white)), 6);
        QCOMPARE(countVerts(arcVerts, kAmber), 3);

        // A calibrated face: shadow and needle, no peak marker, no history.
        NeedleItem cal;
        QVERIFY(cal.deserialize(kSavedCalibratedNeedle));
        QVERIFY(cal.shadow());
        QCOMPARE(cal.peakHold(), false);
        QCOMPARE(cal.powerLimit(), 100.0);
        settle(cal, 80.0);
        settle(cal, 20.0, 5);
        const QVector<float> calVerts = vertsOf(cal);
        QCOMPARE(calVerts.size(), 12 * 6);
        QCOMPARE(countVerts(calVerts, kShadow), 6);
        // Normalise on at 100 W: the tip sits where the value says.
        QCOMPARE(cal.scaleValue(40.0f), 40.0f);

        // Saved again, it keeps its own marker.
        NeedleItem again;
        QVERIFY(again.deserialize(cal.serialize()));
        QCOMPARE(again.peakHold(), false);
        QVERIFY(again.deserialize(arc.serialize()));
        QVERIFY(again.peakHold());
        QCOMPARE(again.peakHoldColour(), kAmber);
    }

    void newOptions_roundTrip()
    {
        NeedleItem a;
        makeCalibrated(a);
        a.setShadow(false);
        a.setPeakHold(true);
        a.setPeakHoldColour(QColor(0x12, 0x34, 0x56));
        a.setShowHistory(true);
        a.setHistoryDuration(750);
        a.setHistoryColor(QColor(10, 20, 30, 40));
        a.setIgnoreHistoryMs(1500);
        a.setNormaliseTo100W(true);
        a.setPowerLimit(0.1);

        NeedleItem b;
        QVERIFY(b.deserialize(a.serialize()));
        QCOMPARE(b.shadow(), false);
        QCOMPARE(b.peakHold(), true);
        QCOMPARE(b.peakHoldColour(), QColor(0x12, 0x34, 0x56));
        QCOMPARE(b.showHistory(), true);
        QCOMPARE(b.historyDuration(), 750);
        QCOMPARE(b.historyColor(), QColor(10, 20, 30, 40));
        QCOMPARE(b.ignoreHistoryMs(), 1500);
        QCOMPARE(b.strokeWidth(), a.strokeWidth());
        QCOMPARE(b.normaliseTo100W(), true);
        QCOMPARE(b.powerLimit(), static_cast<double>(0.1f));
        QCOMPARE(b.serialize(), a.serialize());

        // Peak hold off on an arc stays off.
        NeedleItem c;
        c.setPeakHold(false);
        NeedleItem d;
        QVERIFY(d.deserialize(c.serialize()));
        QCOMPARE(d.peakHold(), false);
    }

    void makeEntryNeedle_takesThetisDefaults()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("NEEDLE"));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* n = qobject_cast<NeedleItem*>(item.get());
        QVERIFY(n != nullptr);
        QCOMPARE(n->shadow(), true);
        QCOMPARE(n->peakHold(), false);
        QCOMPARE(n->peakHoldColour(), QColor(Qt::red));
        QCOMPARE(n->showHistory(), false);
        QCOMPARE(n->historyDuration(), 500);
        QCOMPARE(n->ignoreHistoryMs(), 2000);
        QCOMPARE(n->needleColor(), QColor(Qt::white));
        QCOMPARE(n->strokeWidth(), 3.0f);
        QCOMPARE(n->attackRatio(), 0.8f);
        QCOMPARE(n->decayRatio(), 0.2f);
        QCOMPARE(n->historyColor(), QColor(255, 255, 255, 128));
        QCOMPARE(n->updateIntervalMs(), 100);
        QCOMPARE(kNeedleThetisDefaults.historyDurationMs, 500);
        QCOMPARE(kNeedleThetisDefaults.ignoreHistoryMs, 2000);
    }

    void makeEntryNeedleScalePwr_takesThetisDefaults()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("NEEDLESCALEPWR"));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* s = qobject_cast<NeedleScalePwrItem*>(item.get());
        QVERIFY(s != nullptr);
        QCOMPARE(s->lowColour(), QColor(Qt::white));
        QCOMPARE(s->highColour(), QColor(Qt::red));
        QCOMPARE(s->fontFamily(), QStringLiteral("Trebuchet MS"));
        QCOMPARE(s->fontBold(), false);
        QCOMPARE(s->fontSize(), 20.0f);
        QCOMPARE(s->showMarkers(), true);
        QCOMPARE(s->darkMode(), false);
        QCOMPARE(s->lengthFactor(), 1.0f);
        QCOMPARE(s->direction(), NeedleScalePwrItem::Direction::Clockwise);
        QCOMPARE(s->maxPower(), 100.0f);
        QCOMPARE(s->updateIntervalMs(), 100);
        QCOMPARE(QColor::fromRgb(kNeedleScalePwrThetisDefaults.lowColour), QColor(Qt::white));
    }

    void plainItems_keepTheLegacyLook()
    {
        // Items built in code (presets) keep NereusSDR's constructor values;
        // only makeEntry applies the Thetis defaults.
        NeedleItem n;
        QCOMPARE(n.shadow(), true);
        QCOMPARE(n.peakHold(), true);
        QCOMPARE(n.peakHoldColour(), kAmber);
        QCOMPARE(n.historyDuration(), 4000);
        QCOMPARE(n.ignoreHistoryMs(), 0);
        QCOMPARE(n.powerLimit(), 100.0);
        makeCalibrated(n);
        QCOMPARE(n.peakHold(), false);

        NeedleScalePwrItem s;
        QCOMPARE(s.maxPower(), 150.0f);
        QCOMPARE(s.fontBold(), true);
    }
};

QTEST_MAIN(TstMeterItemNeedle)
#include "tst_meter_item_needle.moc"
