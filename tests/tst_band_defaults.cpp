// tst_band_defaults.cpp
//
// no-port-check: Test file references Thetis behavior in commentary only;
// no Thetis source is translated here. Thetis cites are in BandDefaults.cpp.
//
// Seed-table integrity tests for BandDefaults (#118).
// Frequencies sourced verbatim from Thetis Region 2 seeds:
//   clsBandStackManager.cs:2099-2176 [v2.10.3.13 @ 501e3f51]

#include <QtTest/QtTest>

#include "core/WdspTypes.h"
#include "models/Band.h"
#include "models/BandDefaults.h"

using namespace NereusSDR;

class TestBandDefaults : public QObject {
    Q_OBJECT

private slots:
    void every_ham_band_has_valid_seed() {
        for (Band b : { Band::Band160m, Band::Band80m, Band::Band60m,
                        Band::Band40m,  Band::Band30m, Band::Band20m,
                        Band::Band17m,  Band::Band15m, Band::Band12m,
                        Band::Band10m,  Band::Band6m,  Band::WWV,
                        Band::GEN }) {
            BandSeed s = BandDefaults::seedFor(b);
            QVERIFY2(s.valid, qPrintable(QStringLiteral("Band %1 must have a valid seed")
                                          .arg(bandLabel(b))));
            QCOMPARE(s.band, b);
            QVERIFY(s.frequencyHz > 0.0);
        }
    }

    void xvtr_seed_is_invalid() {
        BandSeed s = BandDefaults::seedFor(Band::XVTR);
        QVERIFY(!s.valid);
    }

    void seed_freq_lands_inside_its_own_band() {
        // Skips GEN (fallback band — any-freq maps to it) and WWV
        // (detected at discrete centers ± window, so the 10 MHz seed
        // lands in WWV, not a ham band).
        for (Band b : { Band::Band160m, Band::Band80m, Band::Band60m,
                        Band::Band40m,  Band::Band30m, Band::Band20m,
                        Band::Band17m,  Band::Band15m, Band::Band12m,
                        Band::Band10m,  Band::Band6m }) {
            BandSeed s = BandDefaults::seedFor(b);
            Band derived = bandFromFrequency(s.frequencyHz);
            QVERIFY2(derived == b,
                     qPrintable(QStringLiteral("Seed for %1 (%2 Hz) derives to %3")
                                  .arg(bandLabel(b))
                                  .arg(s.frequencyHz)
                                  .arg(bandLabel(derived))));
        }
    }

    void wwv_seed_maps_to_wwv() {
        BandSeed s = BandDefaults::seedFor(Band::WWV);
        QCOMPARE(bandFromFrequency(s.frequencyHz), Band::WWV);
    }

    void seeds_match_thetis_region2_values() {
        // Exact values from clsBandStackManager.cs:2104-2168 (first voice
        // entry per band, Thetis v2.10.3.13).
        struct Expected { Band band; double hz; DSPMode mode; };
        const Expected table[] = {
            { Band::Band160m, 1840000.0,  DSPMode::LSB },   // :2104
            { Band::Band80m,  3650000.0,  DSPMode::LSB },   // :2109
            { Band::Band60m,  5354000.0,  DSPMode::USB },   // :2114
            { Band::Band40m,  7152000.0,  DSPMode::LSB },   // :2120
            { Band::Band30m,  10107000.0, DSPMode::CWU },   // :2123 (CW only)
            { Band::Band20m,  14155000.0, DSPMode::USB },   // :2130
            { Band::Band17m,  18125000.0, DSPMode::USB },   // :2135
            { Band::Band15m,  21205000.0, DSPMode::USB },   // :2140
            { Band::Band12m,  24931000.0, DSPMode::USB },   // :2145
            { Band::Band10m,  28305000.0, DSPMode::USB },   // :2150
            { Band::Band6m,   50125000.0, DSPMode::USB },   // :2155
            { Band::WWV,      10000000.0, DSPMode::SAM },   // :2165
            { Band::GEN,      13845000.0, DSPMode::SAM },   // :2168
        };
        for (const Expected& e : table) {
            BandSeed s = BandDefaults::seedFor(e.band);
            QCOMPARE(s.frequencyHz, e.hz);
            QCOMPARE(s.mode, e.mode);
        }
    }

    // Container item parity Task 35 fix round 1 (2026-10-26): the SWL
    // bands, the band grid's GEN group, seed from the first entry of each
    // band in Thetis AddBandStackSWL (clsBandStackManager.cs:2246-2303
    // [v2.10.3.15]), all SAM, and land in their own band.
    void swl_seeds_match_thetis_and_land_in_their_band() {
        struct Expected { Band band; double hz; };
        const Expected table[] = {
            { Band::Band120m,  2400000.0 },   // :2246
            { Band::Band90m,   3300000.0 },   // :2250
            { Band::Band61m,   4700000.0 },   // :2254
            { Band::Band49m,   5600000.0 },   // :2258
            { Band::Band41m,   7310000.0 },   // :2266
            { Band::Band31m,   9100000.0 },   // :2271
            { Band::Band25m,  11700000.0 },   // :2279
            { Band::Band22m,  13600000.0 },   // :2283
            { Band::Band19m,  15200000.0 },   // :2287
            { Band::Band16m,  17500000.0 },   // :2291
            { Band::Band14m,  18900000.0 },   // :2295
            { Band::Band13m,  21500000.0 },   // :2299
            { Band::Band11m,  25700000.0 },   // :2303
        };
        for (const Expected& e : table) {
            const BandSeed s = BandDefaults::seedFor(e.band);
            QVERIFY(s.valid);
            QCOMPARE(s.band, e.band);
            QCOMPARE(s.frequencyHz, e.hz);
            QCOMPARE(s.mode, DSPMode::SAM);
            QCOMPARE(BandDefaults::bandGridBandFromFrequency(s.frequencyHz), e.band);
            // bandFromFrequency itself never names an SWL band.
            QCOMPARE(bandFromFrequency(s.frequencyHz), Band::GEN);
        }
        // Ham bands and WWV come first; outside every range is GEN.
        QCOMPARE(BandDefaults::bandGridBandFromFrequency(14100000.0), Band::Band20m);
        QCOMPARE(BandDefaults::bandGridBandFromFrequency(10000000.0), Band::WWV);
        QCOMPARE(BandDefaults::bandGridBandFromFrequency(1000000.0), Band::GEN);
        QCOMPARE(BandDefaults::bandGridBandFromFrequency(30000000.0), Band::GEN);
    }
};

QTEST_MAIN(TestBandDefaults)
#include "tst_band_defaults.moc"
