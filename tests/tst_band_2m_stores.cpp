// tst_band_2m_stores.cpp
//
// no-port-check: Test file references Thetis behavior in commentary only;
// no Thetis source is translated here.
//
// 2 m is its own band (JJ's ruling 2026-09-28): every per-band store that
// Thetis keeps a 2 m value in keeps one of its own, saved under the "2m"
// key (or the band's number, 27, where a store keys by number), and a 2 m
// value never lands on GEN's. Thetis sizes these by (int)Band.LAST, which
// includes B2M (console.cs:1793-1827 [v2.10.3.15]); its display grid has
// no 2 m slot and uses XVTR's, which the panadapter follows.
//
// The 2 m band is written as static_cast<Band>(27): the number is the
// contract.

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/StepAttenuatorController.h"
#include "core/TuneMemoryStore.h"
#include "core/accessories/AlexController.h"
#include "models/Band.h"
#include "models/PanadapterModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {
constexpr Band k2m = static_cast<Band>(27);
const QString kMac = QStringLiteral("00:1c:c0:a2:22:5e");
}

class TestBand2mStores : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void panadapter_grid_follows_xvtr()
    {
        // Thetis keeps no display grid of its own for 2 m: its per-band
        // switch sends B2M to the XVTR values (console.cs:9337-9339 and
        // 9474-9476 [v2.10.3.15]). So 2 m reads and writes XVTR's slot, and
        // no longer GEN's.
        {
            PanadapterModel pan;
            pan.setPerBandDbMax(Band::GEN, -30);
            pan.setPerBandDbMin(Band::GEN, -120);
            pan.setPerBandDbMax(k2m, -50);
            pan.setPerBandDbMin(k2m, -150);
        }
        PanadapterModel pan;
        QCOMPARE(pan.perBandGrid(Band::XVTR).dbMax, -50);
        QCOMPARE(pan.perBandGrid(Band::XVTR).dbMin, -150);
        QCOMPARE(pan.perBandGrid(k2m).dbMax, -50);
        QCOMPARE(pan.perBandGrid(Band::GEN).dbMax, -30);
        QCOMPARE(pan.perBandGrid(Band::GEN).dbMin, -120);
        pan.setCenterFrequency(144'300'000.0);
        QCOMPARE(pan.band(), k2m);
        QCOMPARE(pan.dBmCeiling(), -50);
        QCOMPARE(pan.dBmFloor(), -150);
    }

    void transmit_power_round_trip()
    {
        {
            TransmitModel t;
            t.loadFromSettings(kMac);
            t.setPowerForBand(Band::GEN, 20);
            t.setPowerForBand(k2m, 37);
            t.setMacAddress(kMac);
            t.setTunePowerForBand(Band::GEN, 11);
            t.setTunePowerForBand(k2m, 23);
            t.save();
        }
        TransmitModel t;
        t.loadFromSettings(kMac);
        t.setMacAddress(kMac);
        t.load();
        QCOMPARE(t.powerForBand(k2m), 37);
        QCOMPARE(t.powerForBand(Band::GEN), 20);
        QCOMPARE(t.tunePowerForBand(k2m), 23);
        QCOMPARE(t.tunePowerForBand(Band::GEN), 11);
    }

    void step_attenuator_round_trip()
    {
        {
            StepAttenuatorController ctrl;
            ctrl.setTickTimerEnabled(false);
            ctrl.loadSettings(kMac);
            ctrl.setTxAttenuationForBand(Band::GEN, 4);
            ctrl.setTxAttenuationForBand(k2m, 9);
            ctrl.setBand(k2m);
            ctrl.setAttenuation(12, 0);
            ctrl.setBand(Band::GEN);
            ctrl.setAttenuation(3, 0);
            ctrl.saveSettings(kMac);
        }
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.loadSettings(kMac);
        QCOMPARE(ctrl.txAttenuationForBand(k2m), 9);
        QCOMPARE(ctrl.txAttenuationForBand(Band::GEN), 4);
        ctrl.setBand(k2m);
        QCOMPARE(ctrl.attenuatorDb(), 12);
        ctrl.setBand(Band::GEN);
        QCOMPARE(ctrl.attenuatorDb(), 3);
    }

    void alex_antennas_round_trip()
    {
        {
            AlexController a;
            a.setMacAddress(kMac);
            a.setRxAnt(k2m, 3);
            a.setTxAnt(k2m, 2);
            a.save();
        }
        AlexController a;
        a.setMacAddress(kMac);
        a.load();
        QCOMPARE(a.rxAnt(k2m), 3);
        QCOMPARE(a.txAnt(k2m), 2);
        QCOMPARE(a.rxAnt(Band::GEN), 1);
        QCOMPARE(a.txAnt(Band::GEN), 1);
    }

    void slice_band_memory_round_trip()
    {
        // The slice's per-band memory is keyed by bandKeyName: 2 m's is
        // its own ("Band2m"), and what was saved for GEN stays GEN's.
        {
            SliceModel slice(0);
            slice.setDspMode(DSPMode::LSB);
            slice.setFrequency(7'100'000.0);
            slice.saveToSettings(Band::GEN);
            slice.setDspMode(DSPMode::FM);
            slice.setFrequency(146'520'000.0);
            slice.saveToSettings(k2m);
        }
        QVERIFY(AppSettings::instance().contains(QStringLiteral("Slice0/Band2m/DspMode")));
        QCOMPARE(SliceModel::loadLastBandFromSettings(0), std::optional<Band>(k2m));
        SliceModel slice(0);
        QVERIFY(slice.hasSettingsFor(k2m));
        slice.restoreFromSettings(k2m);
        QCOMPARE(slice.dspMode(), DSPMode::FM);
        QCOMPARE(slice.frequency(), 146'520'000.0);
        slice.restoreFromSettings(Band::GEN);
        QCOMPARE(slice.dspMode(), DSPMode::LSB);
    }

    void tgxl_tune_memory_round_trip()
    {
        {
            TuneMemoryStore store;
            store.store(TuneMemory{1, Band::GEN, 10, 20, 30, 1000});
            store.store(TuneMemory{1, k2m, 40, 50, 60, 2000});
        }
        TuneMemoryStore store;
        const auto twoMetre = store.recall(1, k2m);
        const auto gen = store.recall(1, Band::GEN);
        QVERIFY(twoMetre.has_value());
        QVERIFY(gen.has_value());
        QCOMPARE(twoMetre->c1, 40);
        QCOMPARE(gen->c1, 10);
    }
};

QTEST_MAIN(TestBand2mStores)
#include "tst_band_2m_stores.moc"
