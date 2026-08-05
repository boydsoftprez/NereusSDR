// =================================================================
// tests/tst_notch_spatial_helpers.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// TNF section 5.3: the MNotchDB spatial helpers ported from Thetis
// radio.cs. notchesInBandwidth uses INCLUSIVE edge overlap
// (radio.cs:4286); the notchSurrounding pad applies only when FWidth <
// padWidth * 2 (radio.cs:4310), and the first match in list order wins.
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
//         section 5.3.
// =================================================================

#include <QtTest/QtTest>
#include "models/NotchModel.h"

using namespace NereusSDR;

class TestNotchSpatialHelpers : public QObject {
    Q_OBJECT

private slots:
    // notchNearFreq (radio.cs:4261-4272; strict `<` at :4267)
    void nearFreq_false_on_empty_model()
    {
        NotchModel m;
        QVERIFY(!m.notchNearFreq(14074000.0, 10));
    }

    void nearFreq_true_inside_the_window()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        QVERIFY(m.notchNearFreq(14074009.0, 10));
        QVERIFY(m.notchNearFreq(14073991.0, 10));
        QVERIFY(m.notchNearFreq(14074000.0, 10));
    }

    void nearFreq_false_exactly_at_the_window_edge()
    {
        // Strict `<`: |delta| == deltaHz is NOT "near".
        NotchModel m;
        m.addNotch(14074000.0);
        QVERIFY(!m.notchNearFreq(14074010.0, 10));
        QVERIFY(!m.notchNearFreq(14073990.0, 10));
    }

    // notchesInBandwidth (radio.cs:4276-4293; inclusive at :4286)
    void inBandwidth_returns_notch_fully_inside()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        const QList<Notch> l = m.notchesInBandwidth(14074000.0, -3000, 3000);
        QCOMPARE(l.size(), 1);
        QCOMPARE(l.first().id, id);
    }

    void inBandwidth_excludes_notch_outside()
    {
        NotchModel m;
        m.addNotch(14090000.0);
        QVERIFY(m.notchesInBandwidth(14074000.0, -3000, 3000).isEmpty());
    }

    void inBandwidth_includes_notch_whose_upper_edge_touches_the_low_bound()
    {
        // min = 14074000 - 3000 = 14071000. A 200 Hz notch centred at
        // 14070900 has its upper edge at exactly 14071000, and radio.cs:4286
        // tests `>= min`, so it IS included.
        NotchModel m;
        const int id = m.addNotch(14070900.0);
        const QList<Notch> l = m.notchesInBandwidth(14074000.0, -3000, 3000);
        QCOMPARE(l.size(), 1);
        QCOMPARE(l.first().id, id);
    }

    void inBandwidth_includes_notch_whose_lower_edge_touches_the_high_bound()
    {
        // max = 14074000 + 3000 = 14077000. A 200 Hz notch centred at
        // 14077100 has its lower edge at exactly 14077000 (`<= max`).
        NotchModel m;
        const int id = m.addNotch(14077100.0);
        const QList<Notch> l = m.notchesInBandwidth(14074000.0, -3000, 3000);
        QCOMPARE(l.size(), 1);
        QCOMPARE(l.first().id, id);
    }

    void inBandwidth_excludes_notch_one_hz_beyond_the_high_bound()
    {
        NotchModel m;
        m.addNotch(14077101.0);
        QVERIFY(m.notchesInBandwidth(14074000.0, -3000, 3000).isEmpty());
    }

    void inBandwidth_preserves_list_order()
    {
        NotchModel m;
        const int a = m.addNotch(14075000.0);
        const int b = m.addNotch(14073000.0);
        const QList<Notch> l = m.notchesInBandwidth(14074000.0, -3000, 3000);
        QCOMPARE(l.size(), 2);
        QCOMPARE(l.at(0).id, a);
        QCOMPARE(l.at(1).id, b);
    }

    // notchSurrounding (radio.cs:4297-4325)
    void surrounding_hits_inside_the_notch()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);   // 200 Hz wide
        const Notch* n = m.notchSurrounding(14074000.0, -3000, 3000, 14074050.0);
        QVERIFY(n != nullptr);
        QCOMPARE(n->id, id);
    }

    void surrounding_hits_exactly_on_the_notch_edges()
    {
        NotchModel m;
        m.addNotch(14074000.0);   // edges at 14073900 / 14074100
        QVERIFY(m.notchSurrounding(14074000.0, -3000, 3000, 14073900.0) != nullptr);
        QVERIFY(m.notchSurrounding(14074000.0, -3000, 3000, 14074100.0) != nullptr);
    }

    void surrounding_misses_just_outside_the_notch()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        QCOMPARE(m.notchSurrounding(14074000.0, -3000, 3000, 14074101.0), nullptr);
    }

    void surrounding_misses_when_the_notch_is_outside_the_bandwidth()
    {
        NotchModel m;
        m.addNotch(14090000.0);
        QCOMPARE(m.notchSurrounding(14074000.0, -3000, 3000, 14090000.0), nullptr);
    }

    void surrounding_pad_widens_a_notch_narrower_than_twice_the_pad()
    {
        // width 200 < padWidth 150 * 2 = 300, so the pad applies: edges
        // become 14073750 / 14074250.
        NotchModel m;
        m.addNotch(14074000.0);
        QCOMPARE(m.notchSurrounding(14074000.0, -3000, 3000, 14074200.0), nullptr);
        QVERIFY(m.notchSurrounding(14074000.0, -3000, 3000, 14074200.0, 150) != nullptr);
        QVERIFY(m.notchSurrounding(14074000.0, -3000, 3000, 14074250.0, 150) != nullptr);
        QCOMPARE(m.notchSurrounding(14074000.0, -3000, 3000, 14074251.0, 150), nullptr);
    }

    void surrounding_pad_does_not_apply_to_a_wide_notch()
    {
        // width 400 is NOT < padWidth 150 * 2 = 300, so radio.cs:4310 leaves
        // the edges alone: 14073800 / 14074200.
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QVERIFY(m.setWidth(id, 400.0));
        QVERIFY(m.notchSurrounding(14074000.0, -3000, 3000, 14074200.0, 150) != nullptr);
        QCOMPARE(m.notchSurrounding(14074000.0, -3000, 3000, 14074250.0, 150), nullptr);
    }

    void surrounding_returns_the_first_match_in_list_order()
    {
        // Two overlapping notches; the one added first must win.
        NotchModel m;
        const int a = m.addNotch(14074000.0);
        const int b = m.addNotch(14074100.0);
        QVERIFY(a != b);
        const Notch* n = m.notchSurrounding(14074000.0, -3000, 3000, 14074050.0);
        QVERIFY(n != nullptr);
        QCOMPARE(n->id, a);
    }

    void surrounding_returns_nullptr_on_empty_model()
    {
        NotchModel m;
        QCOMPARE(m.notchSurrounding(14074000.0, -3000, 3000, 14074000.0), nullptr);
    }
};

QTEST_MAIN(TestNotchSpatialHelpers)
#include "tst_notch_spatial_helpers.moc"
