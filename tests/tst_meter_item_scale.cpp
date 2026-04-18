// =================================================================
// tests/tst_meter_item_scale.cpp  (NereusSDR)
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

#include <QtTest/QtTest>
#include <QImage>
#include <QPainter>

#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"

using namespace NereusSDR;

class TstMeterItemScale : public QObject
{
    Q_OBJECT

private:
    // Count pixels in `img` within `region` whose color is close to `wanted`
    // (ignoring alpha). Used to prove a title or marker actually rendered
    // somewhere.
    static int countMatching(const QImage& img, const QRect& region,
                             const QColor& wanted, int tol = 40)
    {
        int hits = 0;
        for (int y = region.top(); y <= region.bottom(); ++y) {
            for (int x = region.left(); x <= region.right(); ++x) {
                const QColor c = img.pixelColor(x, y);
                if (std::abs(c.red()   - wanted.red())   <= tol &&
                    std::abs(c.green() - wanted.green()) <= tol &&
                    std::abs(c.blue()  - wanted.blue())  <= tol) {
                    ++hits;
                }
            }
        }
        return hits;
    }

private slots:
    // ---- Phase B2: ShowType + centered title ----

    void showType_default_is_false()
    {
        ScaleItem s;
        QCOMPARE(s.showType(), false);
    }

    void showType_roundtrip()
    {
        ScaleItem s;
        s.setShowType(true);
        QCOMPARE(s.showType(), true);
    }

    void titleColour_defaults_and_roundtrip()
    {
        // Thetis ShowType draws in scale.FontColourType — the reference
        // screenshot shows these titles in red. NereusSDR stores that as
        // a settable QColor with a red default to match the screenshot.
        ScaleItem s;
        QCOMPARE(s.titleColour(), QColor(Qt::red));
        s.setTitleColour(QColor(Qt::green));
        QCOMPARE(s.titleColour(), QColor(Qt::green));
    }

    void paint_with_showType_false_does_not_draw_title()
    {
        QImage img(200, 80, QImage::Format_ARGB32);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            ScaleItem s;
            s.setRect(0.0f, 0.0f, 1.0f, 1.0f);
            s.setBindingId(MeterBinding::TxAlc);
            s.setRange(-30.0, 0.0);
            // showType defaults to false
            s.paint(p, img.width(), img.height());
        }
        // No red pixels anywhere — ticks are the default tick color
        // (#c8d8e8 — cool gray), not red.
        const int redHits = countMatching(img, img.rect(), QColor(Qt::red), 20);
        QCOMPARE(redHits, 0);
    }

    void paint_with_showType_true_draws_red_title_in_top_strip()
    {
        QImage img(200, 80, QImage::Format_ARGB32);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            ScaleItem s;
            s.setRect(0.0f, 0.0f, 1.0f, 1.0f);
            s.setBindingId(MeterBinding::TxAlc);
            s.setRange(-30.0, 0.0);
            s.setShowType(true);
            // Red is the default title colour — don't override it.
            s.paint(p, img.width(), img.height());
        }

        // Title should land in the top ~30% of the widget (before the
        // tick row which Thetis places at y + h*0.85).
        const QRect topBand(0, 0, img.width(), img.height() * 30 / 100);
        const int redInTop = countMatching(img, topBand, QColor(Qt::red), 60);
        QVERIFY2(redInTop > 0,
                 "ShowType should paint the ALC title in the top band "
                 "of the ScaleItem rect");

        // And no red in the bottom band (ticks are gray, not red).
        const QRect botBand(0, img.height() * 70 / 100,
                            img.width(), img.height() * 30 / 100);
        const int redInBot = countMatching(img, botBand, QColor(Qt::red), 20);
        QCOMPARE(redInBot, 0);
    }

    void paint_with_showType_true_uses_readingName_text()
    {
        // Sanity check: the ScaleItem title should come from
        // readingName(bindingId), not from a hardcoded string. Swap the
        // binding to SignalMaxBin and expect a much wider title ("Signal
        // Max FFT Bin" is long) — the red-pixel count should go UP.
        auto redHitsForBinding = [this](int binding) {
            QImage img(200, 80, QImage::Format_ARGB32);
            img.fill(Qt::black);
            {
                QPainter p(&img);
                ScaleItem s;
                s.setRect(0.0f, 0.0f, 1.0f, 1.0f);
                s.setBindingId(binding);
                s.setRange(-140.0, 0.0);
                s.setShowType(true);
                s.paint(p, img.width(), img.height());
            }
            return countMatching(img,
                                 QRect(0, 0, img.width(), img.height() * 30 / 100),
                                 QColor(Qt::red), 60);
        };
        const int alcHits = redHitsForBinding(MeterBinding::TxAlc);
        const int maxBinHits = redHitsForBinding(MeterBinding::SignalMaxBin);
        QVERIFY2(maxBinHits > alcHits,
                 "Signal Max FFT Bin should produce more red title pixels "
                 "than ALC (longer label -> wider glyphs)");
    }

    void serialize_roundtrip_with_showType()
    {
        ScaleItem s;
        s.setShowType(true);
        s.setTitleColour(QColor(0xff, 0x99, 0x11));
        s.setBindingId(MeterBinding::TxAlc);
        s.setRange(-30.0, 0.0);

        const QString serialized = s.serialize();

        ScaleItem s2;
        QVERIFY(s2.deserialize(serialized));
        QCOMPARE(s2.showType(), true);
        QCOMPARE(s2.titleColour(), QColor(0xff, 0x99, 0x11));
    }

    void deserialize_legacy_payload_defaults_showType_false()
    {
        // Pre-B2 ScaleItem serialized 15 fields (through m_fontSize). A
        // legacy payload must still load and produce showType=false.
        const QString legacy = QStringLiteral(
            "SCALE|0|0|1|1|-1|0|H|-30|0|6|4|#ffc8d8e8|#ff8090a0|10");
        ScaleItem s;
        QVERIFY(s.deserialize(legacy));
        QCOMPARE(s.showType(), false);
    }

    // ---- Phase B3: GeneralScale two-tone + baseline at y + h*0.85 ----

    void scaleStyle_defaults_to_Linear()
    {
        ScaleItem s;
        QCOMPARE(s.scaleStyle(), ScaleItem::ScaleStyle::Linear);
    }

    void scaleStyle_GeneralScale_roundtrip()
    {
        ScaleItem s;
        s.setScaleStyle(ScaleItem::ScaleStyle::GeneralScale);
        QCOMPARE(s.scaleStyle(), ScaleItem::ScaleStyle::GeneralScale);

        const QString serialized = s.serialize();
        ScaleItem s2;
        QVERIFY(s2.deserialize(serialized));
        QCOMPARE(s2.scaleStyle(), ScaleItem::ScaleStyle::GeneralScale);
    }

    void generalScale_low_high_colours_roundtrip()
    {
        ScaleItem s;
        s.setLowColour(QColor(Qt::green));
        s.setHighColour(QColor(Qt::blue));
        QCOMPARE(s.lowColour(), QColor(Qt::green));
        QCOMPARE(s.highColour(), QColor(Qt::blue));
    }

    void generalScale_params_roundtrip()
    {
        // From Thetis addSMeterBar's dispatch (MeterManager.cs:31915):
        //   (6, 3, -1, 60, 2, 20, ..., 0.5f, true, true)
        ScaleItem s;
        s.setGeneralScaleParams(6, 3, -1, 60, 2, 20, 0.5f);
        QCOMPARE(s.lowLongTicks(), 6);
        QCOMPARE(s.highLongTicks(), 3);
    }

    void generalScale_paints_two_tone_baseline()
    {
        // Paint a GeneralScale into a QImage and scan for a green baseline
        // in the left 50% and a blue baseline in the right 50% at
        // y = h * 0.85.
        QImage img(200, 100, QImage::Format_ARGB32);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            ScaleItem s;
            s.setRect(0.0f, 0.0f, 1.0f, 1.0f);
            s.setRange(-140.0, 0.0);
            s.setScaleStyle(ScaleItem::ScaleStyle::GeneralScale);
            s.setLowColour(QColor(Qt::green));
            s.setHighColour(QColor(Qt::blue));
            // centrePerc 0.5 → split at the horizontal midpoint
            s.setGeneralScaleParams(6, 3, -1, 60, 2, 20, 0.5f);
            s.paint(p, img.width(), img.height());
        }
        const int baselineY = static_cast<int>(img.height() * 0.85f);
        // Scan a 3-pixel-tall band around the baseline to tolerate
        // anti-aliasing and stroke width.
        const QRect leftBand(0, baselineY - 1,
                             img.width() / 2, 3);
        const QRect rightBand(img.width() / 2, baselineY - 1,
                              img.width() / 2, 3);
        const int greenInLeft  = countMatching(img, leftBand,  QColor(Qt::green), 60);
        const int blueInRight  = countMatching(img, rightBand, QColor(Qt::blue),  60);
        QVERIFY2(greenInLeft > 0,
                 "GeneralScale should draw low-colour baseline in left half");
        QVERIFY2(blueInRight > 0,
                 "GeneralScale should draw high-colour baseline in right half");
    }

    void generalScale_Linear_style_does_not_draw_horizontal_baseline()
    {
        // Sanity check the opt-in design — with style=Linear (the
        // NereusSDR default), no wide horizontal baseline appears at
        // y + h*0.85, just the existing evenly-spaced vertical ticks.
        QImage img(200, 100, QImage::Format_ARGB32);
        img.fill(Qt::black);
        {
            QPainter p(&img);
            ScaleItem s;
            s.setRect(0.0f, 0.0f, 1.0f, 1.0f);
            s.setRange(-140.0, 0.0);
            // Default style is Linear; no setScaleStyle() call.
            s.setLowColour(QColor(Qt::green));
            s.paint(p, img.width(), img.height());
        }
        const int baselineY = static_cast<int>(img.height() * 0.85f);
        const QRect leftBand(0, baselineY - 1, img.width() / 2, 3);
        const int greenInLeft = countMatching(img, leftBand, QColor(Qt::green), 60);
        QCOMPARE(greenInLeft, 0);
    }
};

QTEST_MAIN(TstMeterItemScale)
#include "tst_meter_item_scale.moc"
