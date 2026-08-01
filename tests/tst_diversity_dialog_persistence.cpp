// =================================================================
// tests/tst_diversity_dialog_persistence.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic G Task 2: per-band diversity persistence round-trip.
//
// SliceModel persists the new Phase 3F Sub-Epic G diversity controls
// (phase / gain / fine-null) under the existing per-slice-per-band
// schema (Slice<N>/Band<key>/Diversity<Field>) introduced in Phase
// 3G-10 Stage 2 — S2.P. We exercise saveToSettings(Band) on one
// SliceModel, then construct a fresh SliceModel and verify
// restoreFromSettings(Band) re-loads each value.
//
// TestSandboxInit redirects QStandardPaths to a sandbox dir at process
// start (tests/TestSandboxInit.cpp), so the AppSettings singleton
// writes to an isolated file and never pollutes the developer's real
// NereusSDR.settings. We still clear the Slice0/Band20m + Slice0/Band40m
// scope between tests so run order doesn't leak.
// =================================================================

#include <QtTest/QtTest>

#include "models/SliceModel.h"
#include "models/Band.h"
#include "core/AppSettings.h"

using namespace NereusSDR;

class TestDiversityDialogPersistence : public QObject {
    Q_OBJECT

private:
    // Clear Slice0/Band<key>/Diversity* + the legacy keys touched by
    // saveToSettings(band) so prior runs don't leak into this one.
    static void resetDiversityKeysFor(Band band) {
        auto& s = AppSettings::instance();
        const QString bp = QStringLiteral("Slice0/Band%1/")
                              .arg(NereusSDR::bandKeyName(band));
        for (const auto& field : {
                 QStringLiteral("DiversityPhaseDeg"),
                 QStringLiteral("DiversityGainDb"),
                 QStringLiteral("DiversityFineNullEnabled"),
             }) {
            s.remove(bp + field);
        }
    }

private slots:
    void init() {
        resetDiversityKeysFor(Band::Band20m);
        resetDiversityKeysFor(Band::Band40m);
    }

    void cleanup() {
        resetDiversityKeysFor(Band::Band20m);
        resetDiversityKeysFor(Band::Band40m);
    }

    // ── Round-trip 1: phase persists per-band ────────────────────────────────
    void diversity_phase_persists_per_band() {
        {
            SliceModel src;
            src.setDiversityPhaseDeg(123.45);
            src.saveToSettings(Band::Band20m);
        }

        SliceModel dst;
        dst.restoreFromSettings(Band::Band20m);
        QCOMPARE(dst.diversityPhaseDeg(), 123.45);
    }

    // ── Round-trip 2: gain persists per-band ─────────────────────────────────
    void diversity_gain_persists_per_band() {
        {
            SliceModel src;
            src.setDiversityGainDb(-6.0);
            src.saveToSettings(Band::Band40m);
        }

        SliceModel dst;
        dst.restoreFromSettings(Band::Band40m);
        QCOMPARE(dst.diversityGainDb(), -6.0);
    }

    // ── Round-trip 3: fine-null bool persists per-band ───────────────────────
    void diversity_fine_null_persists_per_band() {
        {
            SliceModel src;
            src.setDiversityFineNullEnabled(true);
            src.saveToSettings(Band::Band20m);
        }

        SliceModel dst;
        dst.restoreFromSettings(Band::Band20m);
        QCOMPARE(dst.diversityFineNullEnabled(), true);
    }

    // ── Per-band isolation: 20m and 40m hold independent values ──────────────
    void diversity_phase_and_gain_are_independent_across_bands() {
        {
            SliceModel src;
            src.setDiversityPhaseDeg(15.0);
            src.setDiversityGainDb(2.0);
            src.saveToSettings(Band::Band20m);

            src.setDiversityPhaseDeg(330.0);
            src.setDiversityGainDb(-12.0);
            src.saveToSettings(Band::Band40m);
        }

        SliceModel dst20;
        dst20.restoreFromSettings(Band::Band20m);
        QCOMPARE(dst20.diversityPhaseDeg(), 15.0);
        QCOMPARE(dst20.diversityGainDb(), 2.0);

        SliceModel dst40;
        dst40.restoreFromSettings(Band::Band40m);
        QCOMPARE(dst40.diversityPhaseDeg(), 330.0);
        QCOMPARE(dst40.diversityGainDb(), -12.0);
    }
};

QTEST_MAIN(TestDiversityDialogPersistence)
#include "tst_diversity_dialog_persistence.moc"
