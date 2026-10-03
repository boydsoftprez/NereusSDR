// tst_band_2m.cpp
//
// no-port-check: Test file references Thetis behavior in commentary only;
// no Thetis source is translated here. Thetis cites are in Band.cpp and
// BandDefaults.cpp.
//
// 2 m is its own band (JJ's ruling 2026-09-28), as Thetis has it: B2M in
// enums.cs, 144.0 to 148.0 MHz in every region's table of
// clsBandStackManager.cs, labelled "2m". NereusSDR appends it to its Band
// enum as number 27, after the SWL bands, so no existing band's number
// (persisted per-band arrays, the link's band numbers) moves.
//
// The 2 m band is written as static_cast<Band>(27) here on purpose: the
// number itself is the contract the link and the stores rely on.

#include <QtTest/QtTest>

#include "core/WdspTypes.h"
#include "models/Band.h"
#include "models/BandDefaults.h"

using namespace NereusSDR;

namespace {
constexpr Band k2m = static_cast<Band>(27);
}

class TestBand2m : public QObject {
    Q_OBJECT

private slots:
    void existing_band_numbers_do_not_move()
    {
        QCOMPARE(static_cast<int>(Band::Band160m), 0);
        QCOMPARE(static_cast<int>(Band::Band6m), 10);
        QCOMPARE(static_cast<int>(Band::GEN), 11);
        QCOMPARE(static_cast<int>(Band::WWV), 12);
        QCOMPARE(static_cast<int>(Band::XVTR), 13);
        QCOMPARE(static_cast<int>(Band::Band120m), 14);
        QCOMPARE(static_cast<int>(Band::Band11m), 26);
        QCOMPARE(static_cast<int>(Band::Count), 28);
    }

    void lookup_at_the_edges()
    {
        // Thetis clsBandStackManager.cs: BandFrequencyData(144.0, 148.0,
        // Band.B2M, ...), inclusive at both ends (freq >= low && freq <= high).
        QCOMPARE(bandFromFrequency(143'999'999.0), Band::GEN);
        QCOMPARE(bandFromFrequency(144'000'000.0), k2m);
        QCOMPARE(bandFromFrequency(146'520'000.0), k2m);
        QCOMPARE(bandFromFrequency(148'000'000.0), k2m);
        QCOMPARE(bandFromFrequency(148'000'001.0), Band::GEN);
        // Its neighbours are unchanged.
        QCOMPARE(bandFromFrequency(54'000'000.0), Band::Band6m);
        QCOMPARE(bandFromFrequency(54'000'001.0), Band::GEN);
        QCOMPARE(bandFromFrequency(100'000'000.0), Band::GEN);
        QCOMPARE(bandFromFrequency(222'000'000.0), Band::GEN);
    }

    void label_key_and_name()
    {
        QCOMPARE(bandLabel(k2m), QStringLiteral("2m"));
        QCOMPARE(bandKeyName(k2m), QStringLiteral("2m"));
        QCOMPARE(bandFromName(QStringLiteral("2m")), k2m);
        QCOMPARE(bandFromName(QStringLiteral("2")), k2m);
        // GEN keeps its own key, so settings saved under it stay GEN's.
        QCOMPARE(bandKeyName(Band::GEN), QStringLiteral("GEN"));
    }

    void ui_index_follows_xvtr()
    {
        // The band button grid keeps indices 0..13 (saved visibility bits
        // and active-band indices stay valid) and adds 2 m at 14.
        QCOMPARE(uiIndexFromBand(k2m), 14);
        QCOMPARE(bandFromUiIndex(14), k2m);
        QCOMPARE(bandFromUiIndex(15), Band::GEN);
        QCOMPARE(uiIndexFromBand(Band::XVTR), 13);
        QCOMPARE(bandFromUiIndex(13), Band::XVTR);
        // An SWL band has no button.
        QCOMPARE(uiIndexFromBand(Band::Band120m), -1);
        QCOMPARE(uiIndexFromBand(Band::Band11m), -1);
    }

    void per_band_state_slots()
    {
        QCOMPARE(kPerBandStateCount, 15);
        for (int i = 0; i < 14; ++i) {
            QCOMPARE(perBandStateSlot(static_cast<Band>(i)), i);
            QCOMPARE(bandFromPerBandStateSlot(i), static_cast<Band>(i));
        }
        QCOMPARE(perBandStateSlot(k2m), 14);
        QCOMPARE(bandFromPerBandStateSlot(14), k2m);
        QCOMPARE(perBandStateSlot(Band::Band120m), -1);
        QCOMPARE(perBandStateSlot(Band::Count), -1);
        QVERIFY(hasPerBandState(k2m));
        QVERIFY(!hasPerBandState(Band::Band49m));
        QCOMPARE(k2m, Band::Band2m);
    }

    void band_stack_seed()
    {
        // Thetis clsBandStackManager.cs:2160 [v2.10.3.15]: the first voice
        // entry of the Region 2 "2M" stack, 144.200 MHz USB.
        const BandSeed s = BandDefaults::seedFor(k2m);
        QVERIFY(s.valid);
        QCOMPARE(s.band, k2m);
        QCOMPARE(s.frequencyHz, 144'200'000.0);
        QCOMPARE(s.mode, DSPMode::USB);
        QCOMPARE(bandFromFrequency(s.frequencyHz), k2m);
    }
};

QTEST_APPLESS_MAIN(TestBand2m)
#include "tst_band_2m.moc"
