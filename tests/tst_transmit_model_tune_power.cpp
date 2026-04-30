// no-port-check: NereusSDR-original unit-test file.  The "console.cs"
// references below are cite comments documenting which Thetis lines each
// assertion verifies; no Thetis logic is ported in this test file.
// =================================================================
// tests/tst_transmit_model_tune_power.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel::tunePowerByBand[14] per-MAC persistence.
//
// Phase 3M-1a G.3.
//
// Source references (cited for traceability; logic ported in TransmitModel.cpp):
//   console.cs:12094 [v2.10.3.13]  — declaration of tunePower_by_band int[]
//   console.cs:1819-1820 [v2.10.3.13] — initialisation to 50W per band
//   console.cs:3087-3091 [v2.10.3.13] — save (pipe-delimited in Thetis;
//     NereusSDR uses per-band scalar keys)
//   console.cs:4904-4910 [v2.10.3.13] — restore
// =================================================================

#include <QtTest/QtTest>
#include "models/TransmitModel.h"
#include "models/Band.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TstTransmitModelTunePower : public QObject {
    Q_OBJECT
private slots:

    void init() {
        // Clear AppSettings before each test for isolation.
        AppSettings::instance().clear();
    }

    // ── Default: all bands return 50W ────────────────────────────────────

    void defaultAllBands50W() {
        TransmitModel t;
        // Per-band TX power covers HF amateur + GEN/WWV/XVTR (HL2 SWL
        // bands inherit ham values, so iterate to Band::SwlFirst only).
        for (int i = 0; i < static_cast<int>(Band::SwlFirst); ++i) {
            QCOMPARE(t.tunePowerForBand(static_cast<Band>(i)), 50);
        }
    }

    // ── Basic set/get ────────────────────────────────────────────────────

    void setAndGet() {
        TransmitModel t;
        t.setTunePowerForBand(Band::Band20m, 75);
        QCOMPARE(t.tunePowerForBand(Band::Band20m), 75);
    }

    void otherBandsUnaffected() {
        TransmitModel t;
        t.setTunePowerForBand(Band::Band20m, 75);
        // Every other band should still be 50W.
        // Per-band TX power covers HF amateur + GEN/WWV/XVTR (HL2 SWL
        // bands inherit ham values, so iterate to Band::SwlFirst only).
        for (int i = 0; i < static_cast<int>(Band::SwlFirst); ++i) {
            const Band b = static_cast<Band>(i);
            if (b != Band::Band20m) {
                QCOMPARE(t.tunePowerForBand(b), 50);
            }
        }
    }

    // ── Clamping ─────────────────────────────────────────────────────────

    void clampAbove100() {
        TransmitModel t;
        t.setTunePowerForBand(Band::Band20m, 200);
        QCOMPARE(t.tunePowerForBand(Band::Band20m), 100);
    }

    void clampBelowZero() {
        TransmitModel t;
        t.setTunePowerForBand(Band::Band20m, -5);
        QCOMPARE(t.tunePowerForBand(Band::Band20m), 0);
    }

    void clampAtBoundaries() {
        TransmitModel t;
        t.setTunePowerForBand(Band::Band80m, 0);
        QCOMPARE(t.tunePowerForBand(Band::Band80m), 0);
        t.setTunePowerForBand(Band::Band80m, 100);
        QCOMPARE(t.tunePowerForBand(Band::Band80m), 100);
    }

    // ── Out-of-range Band → 50W safe fallback ────────────────────────────

    void outOfRangeBandReturnsFallback() {
        TransmitModel t;
        // Cast a deliberately out-of-range index.
        const Band outOfRange = static_cast<Band>(999);
        QCOMPARE(t.tunePowerForBand(outOfRange), 50);
    }

    void outOfRangeBandSetIsNoOp() {
        TransmitModel t;
        const Band outOfRange = static_cast<Band>(999);
        t.setTunePowerForBand(outOfRange, 99);  // must not crash
        // All valid bands still at default.
        QCOMPARE(t.tunePowerForBand(Band::Band20m), 50);
    }

    // ── Signal emission ──────────────────────────────────────────────────

    void signalEmittedOnChange() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::tunePowerByBandChanged);
        t.setTunePowerForBand(Band::Band20m, 75);
        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).value<Band>(), Band::Band20m);
        QCOMPARE(args.at(1).toInt(), 75);
    }

    void noSignalOnIdempotentSet() {
        TransmitModel t;
        t.setTunePowerForBand(Band::Band20m, 75);
        QSignalSpy spy(&t, &TransmitModel::tunePowerByBandChanged);
        t.setTunePowerForBand(Band::Band20m, 75);  // same value → no emit
        QCOMPARE(spy.count(), 0);
    }

    void signalCarriesClampedValue() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::tunePowerByBandChanged);
        t.setTunePowerForBand(Band::Band160m, 999);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(1).toInt(), 100);  // clamped to 100
    }

    // ── Persistence round-trip ───────────────────────────────────────────

    void persistenceRoundTrip() {
        const QString mac = QStringLiteral("aa:bb:cc:11:22:33");

        {
            TransmitModel t;
            t.setMacAddress(mac);
            t.setTunePowerForBand(Band::Band20m, 75);
            t.setTunePowerForBand(Band::Band40m, 30);
            t.setTunePowerForBand(Band::Band6m,  10);
            t.save();
        }

        // New model — should restore the saved values.
        TransmitModel t2;
        t2.setMacAddress(mac);
        t2.load();
        QCOMPARE(t2.tunePowerForBand(Band::Band20m), 75);
        QCOMPARE(t2.tunePowerForBand(Band::Band40m), 30);
        QCOMPARE(t2.tunePowerForBand(Band::Band6m),  10);
        // Bands we didn't set should still be 50W default.
        QCOMPARE(t2.tunePowerForBand(Band::Band80m), 50);
    }

    void persistenceRoundTripAllBands() {
        const QString mac = QStringLiteral("de:ad:be:ef:00:01");

        {
            TransmitModel t;
            t.setMacAddress(mac);
            // Per-band TX power covers HF amateur + GEN/WWV/XVTR (HL2 SWL
        // bands inherit ham values, so iterate to Band::SwlFirst only).
        for (int i = 0; i < static_cast<int>(Band::SwlFirst); ++i) {
                t.setTunePowerForBand(static_cast<Band>(i), i * 5 % 101);
            }
            t.save();
        }

        TransmitModel t2;
        t2.setMacAddress(mac);
        t2.load();
        // Per-band TX power covers HF amateur + GEN/WWV/XVTR (HL2 SWL
        // bands inherit ham values, so iterate to Band::SwlFirst only).
        for (int i = 0; i < static_cast<int>(Band::SwlFirst); ++i) {
            QCOMPARE(t2.tunePowerForBand(static_cast<Band>(i)),
                     i * 5 % 101);
        }
    }

    // ── Empty MAC → load/save are no-ops ────────────────────────────────

    void emptyMacLoadIsNoOp() {
        // Pre-seed AppSettings with a known value; load() with empty MAC
        // must not touch it.
        AppSettings::instance().setValue(
            QStringLiteral("hardware//tunePowerByBand/5"), QStringLiteral("99"));

        TransmitModel t;  // no setMacAddress → m_mac is empty
        t.load();         // must be a no-op

        // Band::Band20m is index 5; should still be 50W default (not 99).
        QCOMPARE(t.tunePowerForBand(Band::Band20m), 50);
    }

    void emptyMacSaveIsNoOp() {
        TransmitModel t;
        t.setTunePowerForBand(Band::Band20m, 77);
        // No setMacAddress — save() must not write anything under
        // "hardware//tunePowerByBand/...".
        t.save();
        // Verify the key was not created.
        const QVariant v = AppSettings::instance().value(
            QStringLiteral("hardware//tunePowerByBand/5"));
        QVERIFY(!v.isValid());
    }

    // ── Load clamps out-of-range persisted values ────────────────────────

    void loadClampsOutOfRangeValue() {
        const QString mac = QStringLiteral("ff:ee:dd:cc:bb:aa");
        AppSettings::instance().setValue(
            QStringLiteral("hardware/%1/tunePowerByBand/5").arg(mac),
            QStringLiteral("200"));

        TransmitModel t;
        t.setMacAddress(mac);
        t.load();
        QCOMPARE(t.tunePowerForBand(Band::Band20m), 100);
    }

    void loadClampsNegativeValue() {
        const QString mac = QStringLiteral("11:22:33:44:55:66");
        AppSettings::instance().setValue(
            QStringLiteral("hardware/%1/tunePowerByBand/5").arg(mac),
            QStringLiteral("-10"));

        TransmitModel t;
        t.setMacAddress(mac);
        t.load();
        QCOMPARE(t.tunePowerForBand(Band::Band20m), 0);
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelTunePower)
#include "tst_transmit_model_tune_power.moc"
