// tst_clarity_defaults.cpp
//
// Phase 3G-9b — unit smoke for the Clarity Blue palette registration
// and full-spectrum design invariants. Pure math over wfSchemeStops() —
// no QWidget event loop, no AppSettings interaction.
//
// Design philosophy: Clarity Blue is a full-spectrum rainbow palette
// (black → blue → cyan → green → yellow → red → magenta) with a deep-
// black noise-floor region at the bottom. The "blue look" users see
// comes from AGC + tight thresholds keeping most signals in the blue/
// cyan range; strong signals still progress cleanly to red/magenta.

#include <QtTest/QtTest>

#include "gui/SpectrumWidget.h"

using namespace Longpath;

class TestClarityDefaults : public QObject {
    Q_OBJECT
private slots:

    void clarityBlue_enumOrdinalIsSeven()
    {
        // ClarityBlue is the 8th scheme (index 7). Adding it before Count
        // is the only acceptable ordering — any shift breaks persisted
        // combo ordinals. This assertion locks that in.
        QCOMPARE(static_cast<int>(WfColorScheme::ClarityBlue), 7);
        // 2026-08-15: „Gedämpft" kam als Index 8 dazu — ANGEHÄNGT, was
        // genau das ist, was der Kommentar oben erlaubt. Der Wert dieses
        // Tests liegt in der Zeile darüber, nicht hier.
        //
        // Die vollständige Reihenfolge aller Schemata steht ab jetzt an
        // EINER Stelle, tst_waterfall_muted::theSchemeOrderIsFrozen.
        // Zwei Tests, die dieselbe Zahl festnageln, heißt: beim nächsten
        // Schema fällt wieder einer davon um, und man sucht in der
        // falschen Datei.
        QCOMPARE(static_cast<int>(WfColorScheme::Count), 13);  // +SlateBlue, Night, Ft8Amber, Teal (2026-11-21)
    }

    void clarityBlue_gradientIsMonotonicAndSpansZeroToOne()
    {
        int count = 0;
        const WfGradientStop* stops = wfSchemeStops(WfColorScheme::ClarityBlue, count);
        QVERIFY(stops != nullptr);
        QVERIFY(count >= 6);  // rainbow needs enough stops for smooth progression

        // First stop is exactly at 0.0, last at 1.0 — covers the full
        // normalized range.
        QCOMPARE(stops[0].pos, 0.00f);
        QCOMPARE(stops[count - 1].pos, 1.00f);

        // Positions are strictly monotonically increasing.
        for (int i = 1; i < count; ++i) {
            QVERIFY2(stops[i].pos > stops[i - 1].pos,
                     "Clarity Blue gradient stops must be monotonic");
        }
    }

    void clarityBlue_bottomIsDeepBlack()
    {
        int count = 0;
        const WfGradientStop* stops = wfSchemeStops(WfColorScheme::ClarityBlue, count);
        QVERIFY(count >= 2);

        // First stop must be pure/near-pure black — noise floor renders
        // as dark background, not navy. This is what separates Clarity
        // from Default/Enhanced, both of which start at dark blue.
        QVERIFY2(stops[0].r <= 10 && stops[0].g <= 10 && stops[0].b <= 10,
                 "Clarity Blue bottom stop must be pure/near-pure black");

        // Second stop must still be in the lower-third (<= 0.33) and
        // still very dark so the noise floor stays quiet.
        QVERIFY2(stops[1].pos <= 0.33f,
                 "Clarity Blue second stop must stay in the dark region "
                 "(<= 33% of range) to keep the noise floor quiet");
        const int brightness1 = stops[1].r + stops[1].g + stops[1].b;
        QVERIFY2(brightness1 <= 80,
                 "Clarity Blue second stop must be dark (r+g+b <= 80) so "
                 "noise floor renders as uniform near-black");
    }

    void clarityBlue_topIsVividRainbowPeak()
    {
        int count = 0;
        const WfGradientStop* stops = wfSchemeStops(WfColorScheme::ClarityBlue, count);
        QVERIFY(count >= 1);
        const WfGradientStop& top = stops[count - 1];

        // Strong signals should pop with a vivid peak colour — at least
        // one channel saturated, total brightness high. Rainbow palettes
        // reach red/orange/magenta at peak, NOT white (unlike a narrow-
        // band monochrome scheme).
        const int maxChannel = std::max({top.r, top.g, top.b});
        QVERIFY2(maxChannel >= 200,
                 "Clarity Blue top stop must have at least one channel "
                 "saturated (>= 200) so peak signals are vivid");

        const int totalBrightness = top.r + top.g + top.b;
        QVERIFY2(totalBrightness >= 300,
                 "Clarity Blue top stop total brightness must be >= 300");
    }

    void clarityBlue_hasRainbowProgression()
    {
        // The palette must reach at least one warm-colour stop (red or
        // orange — dominant R channel) somewhere in the upper half.
        // This is the defining full-spectrum invariant: if someone
        // replaces the stops with a blue-only ramp, this test fires.
        int count = 0;
        const WfGradientStop* stops = wfSchemeStops(WfColorScheme::ClarityBlue, count);

        bool foundWarmStop = false;
        for (int i = 0; i < count; ++i) {
            if (stops[i].pos < 0.5f) { continue; }
            // Warm = red channel dominant and at least 192 intensity.
            if (stops[i].r >= 192 && stops[i].r > stops[i].b) {
                foundWarmStop = true;
                break;
            }
        }
        QVERIFY2(foundWarmStop,
                 "Clarity Blue must include at least one warm (red-"
                 "dominant) stop in the upper half of the range — the "
                 "full-spectrum rainbow invariant");
    }
};

QTEST_MAIN(TestClarityDefaults)
#include "tst_clarity_defaults.moc"
