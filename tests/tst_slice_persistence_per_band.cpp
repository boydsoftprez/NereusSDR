// tst_slice_persistence_per_band.cpp
//
// Verifies per-slice-per-band DSP persistence (Phase 3G-10 Stage 2 — S2.P).
//
// Uses AppSettings(filePath) direct constructor so each test gets an isolated
// temporary store — no pollution of the developer's real settings file.
//
// Test 1: saveAndRestoreEachKey
//   Set non-default DSP values on 20m, save, create fresh SliceModel,
//   restore for 20m, verify all per-band and session keys round-trip.
//
// Test 2: bandChangeRestoresPreviouslyStoredValuesForNewBand
//   Store different AgcThreshold on 20m and 40m. Simulate a band change
//   (save 20m, restore 40m) and verify the 40m value is active.
//
// Test 3: sessionStateIsNotPerBand
//   Set muted=true, save for 20m. Restore for 40m (different band, no
//   40m session keys yet). Muted must still be true because session state
//   is band-agnostic.
//
// Test 4: migratesLegacyKeys
//   Write legacy VfoFrequency (14.1 MHz → 20m) + VfoDspMode + VfoAfGain.
//   Call migrateLegacyKeys(). Verify:
//     - Slice0/Band20m/DspMode exists with migrated value
//     - Slice0/AfGain exists with migrated value
//     - VfoFrequency is removed
//     - VfoDspMode is removed
//     - VfoAfGain is removed
//   Call migrateLegacyKeys() again — must be a no-op (no crash, no
//   duplication, old keys absent).

#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "models/SliceModel.h"
#include "models/Band.h"
#include "core/AppSettings.h"
#include "core/WdspTypes.h"

using namespace NereusSDR;

class TestSlicePersistencePerBand : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tempDir;

    // Build an AppSettings instance backed by a fresh temp file.
    // Each call returns a separate in-memory store; no load() is called
    // so the store starts empty.
    AppSettings makeSettings() const {
        return AppSettings(m_tempDir.path() + QStringLiteral("/NereusSDR.settings"));
    }

    // Replace the singleton file path for the duration of one test so that
    // SliceModel::saveToSettings/restoreFromSettings/migrateLegacyKeys all
    // write to the temp file rather than the real user config.
    //
    // Pattern: call AppSettings::instance().setValue("__test_redirect__", ...)
    // is not viable because the singleton caches its path. Instead we
    // write through the singleton (which TestSandboxInit already routes to
    // the QStandardPaths test sandbox) and reset between tests via remove().

    // Remove all Slice0/* and legacy Vfo* keys from the singleton.
    static void resetSingleton() {
        auto& s = AppSettings::instance();
        // Per-band keys for every band any test touches. lastBandRoundTrips*
        // walks the full HF set + GEN/WWV/XVTR, so we have to clean those
        // too or ctest run-order leaks pollute later tests.
        const QString bands[] = {
            QStringLiteral("160m"), QStringLiteral("80m"),  QStringLiteral("60m"),
            QStringLiteral("40m"),  QStringLiteral("30m"),  QStringLiteral("20m"),
            QStringLiteral("17m"),  QStringLiteral("15m"),  QStringLiteral("12m"),
            QStringLiteral("10m"),  QStringLiteral("6m"),
            QStringLiteral("GEN"),  QStringLiteral("WWV"),  QStringLiteral("XVTR"),
        };
        for (const QString& band : bands) {
            const QString bp = QStringLiteral("Slice0/Band") + band + QStringLiteral("/");
            for (const QString& field : {
                     QStringLiteral("AgcThreshold"), QStringLiteral("AgcHang"),
                     QStringLiteral("AgcSlope"),     QStringLiteral("AgcAttack"),
                     QStringLiteral("AgcDecay"),     QStringLiteral("FilterLow"),
                     QStringLiteral("FilterHigh"),   QStringLiteral("DspMode"),
                     QStringLiteral("AgcMode"),      QStringLiteral("StepHz"),
                     QStringLiteral("Frequency"),   QStringLiteral("NbMode") }) {
                s.remove(bp + field);
            }
        }
        // Session keys.
        const QString sp = QStringLiteral("Slice0/");
        for (const QString& field : {
                 QStringLiteral("Locked"),     QStringLiteral("Muted"),
                 QStringLiteral("RitEnabled"), QStringLiteral("RitHz"),
                 QStringLiteral("XitEnabled"), QStringLiteral("XitHz"),
                 QStringLiteral("AfGain"),     QStringLiteral("RfGain"),
                 QStringLiteral("RxAntenna"),  QStringLiteral("TxAntenna"),
                 QStringLiteral("LastBand") }) {
            s.remove(sp + field);
        }
        // Legacy keys.
        for (const QString& key : {
                 QStringLiteral("VfoFrequency"), QStringLiteral("VfoDspMode"),
                 QStringLiteral("VfoFilterLow"), QStringLiteral("VfoFilterHigh"),
                 QStringLiteral("VfoAgcMode"),   QStringLiteral("VfoStepHz"),
                 QStringLiteral("VfoAfGain"),    QStringLiteral("VfoRfGain"),
                 QStringLiteral("VfoRxAntenna"), QStringLiteral("VfoTxAntenna") }) {
            s.remove(key);
        }
    }

private slots:

    void init() {
        QVERIFY(m_tempDir.isValid());
        resetSingleton();
    }

    void cleanup() {
        resetSingleton();
    }

    // ── Test 1: round-trip all keys on one band ───────────────────────────────

    void saveAndRestoreEachKey() {
        // Writer slice.
        SliceModel writer;
        writer.setSliceIndex(0);

        // Set non-default per-band DSP values.
        writer.setAgcThreshold(-30);
        writer.setAgcHang(500);
        writer.setAgcSlope(3);
        writer.setAgcAttack(5);
        writer.setAgcDecay(300);
        writer.setFilter(-2800, -200);  // LSB
        writer.setDspMode(DSPMode::LSB);  // will reset filter; override below
        writer.setFilter(-2800, -200);    // re-apply after setDspMode
        writer.setAgcMode(AGCMode::Fast);
        writer.setStepHz(1000);

        // Set non-default session state.
        writer.setMuted(true);
        writer.setLocked(false);
        writer.setRitEnabled(true);
        writer.setRitHz(250);
        writer.setXitEnabled(false);
        writer.setXitHz(-100);
        writer.setAfGain(70);
        writer.setRfGain(60);
        writer.setRxAntenna(QStringLiteral("ANT2"));
        writer.setTxAntenna(QStringLiteral("ANT2"));

        writer.saveToSettings(Band::Band20m);

        // Reader slice — starts with all defaults.
        SliceModel reader;
        reader.setSliceIndex(0);
        reader.restoreFromSettings(Band::Band20m);

        // Verify per-band DSP.
        QCOMPARE(reader.agcThreshold(), -30);
        QCOMPARE(reader.agcHang(),      500);
        QCOMPARE(reader.agcSlope(),     3);
        QCOMPARE(reader.agcAttack(),    5);
        QCOMPARE(reader.agcDecay(),     300);
        QCOMPARE(reader.filterLow(),    -2800);
        QCOMPARE(reader.filterHigh(),   -200);
        QCOMPARE(reader.dspMode(),      DSPMode::LSB);
        QCOMPARE(reader.agcMode(),      AGCMode::Fast);
        QCOMPARE(reader.stepHz(),       1000);

        // Verify session state.
        QCOMPARE(reader.muted(),        true);
        QCOMPARE(reader.locked(),       false);
        QCOMPARE(reader.ritEnabled(),   true);
        QCOMPARE(reader.ritHz(),        250);
        QCOMPARE(reader.xitEnabled(),   false);
        QCOMPARE(reader.xitHz(),        -100);
        QCOMPARE(reader.afGain(),       70);
        QCOMPARE(reader.rfGain(),       60);
        QCOMPARE(reader.rxAntenna(),    QStringLiteral("ANT2"));
        QCOMPARE(reader.txAntenna(),    QStringLiteral("ANT2"));
    }

    // ── Test 2: per-band isolation ────────────────────────────────────────────

    void bandChangeRestoresPreviouslyStoredValuesForNewBand() {
        SliceModel slice;
        slice.setSliceIndex(0);

        // Store 20m state: AgcThreshold = -10.
        slice.setAgcThreshold(-10);
        slice.saveToSettings(Band::Band20m);

        // Store 40m state: AgcThreshold = -25.
        slice.setAgcThreshold(-25);
        slice.saveToSettings(Band::Band40m);

        // Simulate band change: restore 20m.
        slice.restoreFromSettings(Band::Band20m);
        QCOMPARE(slice.agcThreshold(), -10);

        // Simulate band change: restore 40m.
        slice.restoreFromSettings(Band::Band40m);
        QCOMPARE(slice.agcThreshold(), -25);
    }

    // ── Test 3: session state is band-agnostic ────────────────────────────────

    void sessionStateIsNotPerBand() {
        SliceModel slice;
        slice.setSliceIndex(0);

        // Set muted=true and save for 20m.
        slice.setMuted(true);
        slice.saveToSettings(Band::Band20m);

        // Now restore for 40m. No 40m session keys exist yet, so the
        // restoreFromSettings call should leave muted at its current value.
        // (It does NOT reset to default — it simply skips absent keys.)
        slice.restoreFromSettings(Band::Band40m);
        QCOMPARE(slice.muted(), true);

        // Explicitly save for 40m to write the session key under 40m band.
        // Then restore 40m again — should still read true.
        slice.saveToSettings(Band::Band40m);
        slice.setMuted(false);  // change in-memory
        slice.restoreFromSettings(Band::Band40m);
        // Session key is Slice0/Muted (band-agnostic), so restore reads "True".
        QCOMPARE(slice.muted(), true);
    }

    // ── Test 4: legacy key migration ─────────────────────────────────────────

    void migratesLegacyKeys() {
        auto& s = AppSettings::instance();

        // Write legacy flat keys (14.1 MHz → 20m band).
        s.setValue(QStringLiteral("VfoFrequency"),  14100000.0);
        s.setValue(QStringLiteral("VfoDspMode"),    static_cast<int>(DSPMode::LSB));
        s.setValue(QStringLiteral("VfoFilterLow"),  -3000);
        s.setValue(QStringLiteral("VfoFilterHigh"), -100);
        s.setValue(QStringLiteral("VfoAgcMode"),    static_cast<int>(AGCMode::Fast));
        s.setValue(QStringLiteral("VfoStepHz"),     500);
        s.setValue(QStringLiteral("VfoAfGain"),     65);
        s.setValue(QStringLiteral("VfoRfGain"),     75);
        s.setValue(QStringLiteral("VfoRxAntenna"),  QStringLiteral("ANT2"));
        s.setValue(QStringLiteral("VfoTxAntenna"),  QStringLiteral("ANT2"));

        // Migrate.
        SliceModel::migrateLegacyKeys();

        // Legacy keys must be gone.
        QVERIFY(!s.contains(QStringLiteral("VfoFrequency")));
        QVERIFY(!s.contains(QStringLiteral("VfoDspMode")));
        QVERIFY(!s.contains(QStringLiteral("VfoFilterLow")));
        QVERIFY(!s.contains(QStringLiteral("VfoFilterHigh")));
        QVERIFY(!s.contains(QStringLiteral("VfoAgcMode")));
        QVERIFY(!s.contains(QStringLiteral("VfoStepHz")));
        QVERIFY(!s.contains(QStringLiteral("VfoAfGain")));
        QVERIFY(!s.contains(QStringLiteral("VfoRfGain")));
        QVERIFY(!s.contains(QStringLiteral("VfoRxAntenna")));
        QVERIFY(!s.contains(QStringLiteral("VfoTxAntenna")));

        // New per-band keys must exist under 20m (bandFromFrequency(14.1 MHz) = Band20m).
        const QString bp20 = QStringLiteral("Slice0/Band20m/");
        QVERIFY(s.contains(bp20 + QStringLiteral("DspMode")));
        QCOMPARE(s.value(bp20 + QStringLiteral("DspMode")).toInt(),
                 static_cast<int>(DSPMode::LSB));
        QVERIFY(s.contains(bp20 + QStringLiteral("FilterLow")));
        QCOMPARE(s.value(bp20 + QStringLiteral("FilterLow")).toInt(), -3000);
        QVERIFY(s.contains(bp20 + QStringLiteral("FilterHigh")));
        QCOMPARE(s.value(bp20 + QStringLiteral("FilterHigh")).toInt(), -100);
        QVERIFY(s.contains(bp20 + QStringLiteral("AgcMode")));
        QCOMPARE(s.value(bp20 + QStringLiteral("AgcMode")).toInt(),
                 static_cast<int>(AGCMode::Fast));
        QVERIFY(s.contains(bp20 + QStringLiteral("StepHz")));
        QCOMPARE(s.value(bp20 + QStringLiteral("StepHz")).toInt(), 500);

        // Session keys must exist under Slice0/.
        const QString sp = QStringLiteral("Slice0/");
        QVERIFY(s.contains(sp + QStringLiteral("AfGain")));
        QCOMPARE(s.value(sp + QStringLiteral("AfGain")).toInt(), 65);
        QVERIFY(s.contains(sp + QStringLiteral("RfGain")));
        QCOMPARE(s.value(sp + QStringLiteral("RfGain")).toInt(), 75);
        QVERIFY(s.contains(sp + QStringLiteral("RxAntenna")));
        QCOMPARE(s.value(sp + QStringLiteral("RxAntenna")).toString(),
                 QStringLiteral("ANT2"));

        // Second call to migrateLegacyKeys() must be a no-op (no crash,
        // no double-migration, keys should still be present / absent).
        SliceModel::migrateLegacyKeys();

        // Same assertions — still hold after second call.
        QVERIFY(!s.contains(QStringLiteral("VfoFrequency")));
        QVERIFY(s.contains(bp20 + QStringLiteral("DspMode")));
        QCOMPARE(s.value(sp + QStringLiteral("AfGain")).toInt(), 65);
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    void missingKeyLeavesDefaultUnchanged() {
        // If no per-band key exists, restoreFromSettings must not change
        // the slice's current value (defaults stay intact).
        SliceModel slice;
        slice.setSliceIndex(0);

        int defaultThreshold = slice.agcThreshold();
        // No keys exist for 40m — restore must be a no-op.
        slice.restoreFromSettings(Band::Band40m);
        QCOMPARE(slice.agcThreshold(), defaultThreshold);
    }

    // ── LastBand: persist + reload last-used band marker ──────────────────────

    void lastBandIsPersistedOnSave() {
        // saveToSettings(band) must write Slice<N>/LastBand = bandKeyName(band)
        // every call. This is what RadioModel::loadSliceState reads on the
        // next launch to land on the user's last-used band, instead of
        // falling back to the panadapter's 14.225 MHz default → always 20m.
        SliceModel slice;
        slice.setSliceIndex(0);

        // No marker before any save — fresh install / pre-LastBand settings.
        QCOMPARE(SliceModel::loadLastBandFromSettings(0), std::nullopt);

        slice.saveToSettings(Band::Band40m);
        auto after40 = SliceModel::loadLastBandFromSettings(0);
        QVERIFY(after40.has_value());
        QCOMPARE(*after40, Band::Band40m);

        // Subsequent save on a different band must overwrite, not stack.
        slice.saveToSettings(Band::Band20m);
        auto after20 = SliceModel::loadLastBandFromSettings(0);
        QVERIFY(after20.has_value());
        QCOMPARE(*after20, Band::Band20m);
    }

    void lastBandRoundTripsAcrossEveryHfBand() {
        // Sanity check that bandKeyName/bandFromName round-trip through
        // AppSettings cleanly for the full HF band set the user can land on.
        // Prevents a future label rename from silently breaking persistence
        // for one specific band while leaving others working.
        const Band bands[] = {
            Band::Band160m, Band::Band80m,  Band::Band60m, Band::Band40m,
            Band::Band30m,  Band::Band20m,  Band::Band17m, Band::Band15m,
            Band::Band12m,  Band::Band10m,  Band::Band6m,
            Band::GEN,      Band::WWV,      Band::XVTR,
        };
        SliceModel slice;
        slice.setSliceIndex(0);

        for (Band b : bands) {
            slice.saveToSettings(b);
            auto loaded = SliceModel::loadLastBandFromSettings(0);
            QVERIFY2(loaded.has_value(),
                     qPrintable(QStringLiteral("LastBand missing after saving %1")
                                .arg(bandKeyName(b))));
            QCOMPARE(*loaded, b);
        }
    }

    void lastBandReturnsNulloptOnUnparseableValue() {
        // Belt-and-suspenders: if a corrupted settings file holds a non-band
        // string under Slice0/LastBand, the loader must NOT silently land on
        // Band::GEN — it should return nullopt so RadioModel falls back to
        // the panadapter / default-frequency path.
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Slice0/LastBand"), QStringLiteral("not-a-band"));
        QCOMPARE(SliceModel::loadLastBandFromSettings(0), std::nullopt);

        // GEN itself, however, is a valid persisted value.
        s.setValue(QStringLiteral("Slice0/LastBand"), QStringLiteral("GEN"));
        auto loaded = SliceModel::loadLastBandFromSettings(0);
        QVERIFY(loaded.has_value());
        QCOMPARE(*loaded, Band::GEN);
    }

    // ── Step persists across save/restore (gap-fill regression test) ─────────

    void stepHzPersistsAcrossSaveRestore() {
        // tst_slice_persistence_per_band already covers stepHz round-trip
        // alongside the other DSP keys, but call it out explicitly here as
        // a regression guard for the close-race fix: with the wire from
        // stepHzChanged → scheduleSettingsSave in place, the per-band
        // StepHz key must survive a save+restore on a single band.
        SliceModel writer;
        writer.setSliceIndex(0);
        writer.setStepHz(2500);  // non-default value
        writer.saveToSettings(Band::Band20m);

        SliceModel reader;
        reader.setSliceIndex(0);
        reader.restoreFromSettings(Band::Band20m);
        QCOMPARE(reader.stepHz(), 2500);
    }
};

QTEST_MAIN(TestSlicePersistencePerBand)
#include "tst_slice_persistence_per_band.moc"
