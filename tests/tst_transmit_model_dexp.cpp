// no-port-check: NereusSDR-original unit-test file.  The Thetis references
// below are cite comments documenting which upstream lines each assertion
// verifies; no Thetis logic is ported in this test file.
// =================================================================
// tests/tst_transmit_model_dexp.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel DEXP properties (11 total, Phase 3M-3a-iii
// Tasks 7-10).  Properties grouped by sub-task:
//
//   Task 7  envelope:    dexpEnabled, dexpDetectorTauMs,
//                        dexpAttackTimeMs, dexpReleaseTimeMs
//   Task 8  gate-ratio:  dexpExpansionRatioDb, dexpHysteresisRatioDb
//   Task 9  look-ahead:  dexpLookAheadEnabled, dexpLookAheadMs
//   Task 10 SCF filter:  dexpLowCutHz, dexpHighCutHz,
//                        dexpSideChannelFilterEnabled
//
// Each property is verified for default value (matching Thetis verbatim),
// round-trip setter+getter, signal emission, idempotent guard, range clamp,
// and persistence-across-instances.
//
// Source references (cited per assertion; logic ported in TransmitModel.cpp):
//   setup.Designer.cs:44788     [v2.10.3.13]  udDEXPLookAhead.Value=60
//   setup.Designer.cs:44808     [v2.10.3.13]  chkDEXPLookAheadEnable.Checked=true
//   setup.Designer.cs:44869-73  [v2.10.3.13]  udDEXPHysteresisRatio.Value=2.0
//   setup.Designer.cs:44900-04  [v2.10.3.13]  udDEXPExpansionRatio.Value=10
//   setup.Designer.cs:44990     [v2.10.3.13]  udDEXPRelease.Value=100
//   setup.Designer.cs:45050     [v2.10.3.13]  udDEXPAttack.Value=2
//   setup.Designer.cs:45093     [v2.10.3.13]  udDEXPDetTau.Value=20
//   setup.Designer.cs:45140-51  [v2.10.3.13]  chkDEXPEnable (no Checked= -> false)
//   setup.Designer.cs:45210     [v2.10.3.13]  udSCFHighCut.Value=1500
//   setup.Designer.cs:45240     [v2.10.3.13]  udSCFLowCut.Value=500
//   setup.Designer.cs:45250     [v2.10.3.13]  chkSCFEnable.Checked=true
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/AppSettings.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {
const QString kMac = QStringLiteral("dexp:test:mac");
}

class TstTransmitModelDexp : public QObject {
    Q_OBJECT
private slots:

    void initTestCase() {
        AppSettings::instance().clear();
    }
    void init() {
        // Clear before each test so persistence assertions are isolated.
        AppSettings::instance().clear();
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // TASK 7 — DEXP envelope properties (4 props)
    //   dexpEnabled, dexpDetectorTauMs, dexpAttackTimeMs, dexpReleaseTimeMs
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    // ── Defaults match Thetis ──────────────────────────────────────

    void default_dexpEnabled_isFalse() {
        // setup.Designer.cs:45140-45151 [v2.10.3.13]: chkDEXPEnable has no
        // explicit Checked= setter, so the WinForms CheckBox default is false.
        TransmitModel t;
        QCOMPARE(t.dexpEnabled(), false);
    }

    void default_dexpDetectorTauMs_is20() {
        // setup.Designer.cs:45093 [v2.10.3.13] - udDEXPDetTau.Value=20.
        TransmitModel t;
        QCOMPARE(t.dexpDetectorTauMs(), 20.0);
    }

    void default_dexpAttackTimeMs_is2() {
        // setup.Designer.cs:45050 [v2.10.3.13] - udDEXPAttack.Value=2.
        TransmitModel t;
        QCOMPARE(t.dexpAttackTimeMs(), 2.0);
    }

    void default_dexpReleaseTimeMs_is100() {
        // setup.Designer.cs:44990 [v2.10.3.13] - udDEXPRelease.Value=100.
        TransmitModel t;
        QCOMPARE(t.dexpReleaseTimeMs(), 100.0);
    }

    // ── Round-trip setters ─────────────────────────────────────────

    void setDexpEnabled_true_roundTrip() {
        TransmitModel t;
        t.setDexpEnabled(true);
        QCOMPARE(t.dexpEnabled(), true);
    }

    void setDexpDetectorTauMs_roundTrip() {
        TransmitModel t;
        t.setDexpDetectorTauMs(50.0);
        QCOMPARE(t.dexpDetectorTauMs(), 50.0);
    }

    void setDexpAttackTimeMs_roundTrip() {
        TransmitModel t;
        t.setDexpAttackTimeMs(15.0);
        QCOMPARE(t.dexpAttackTimeMs(), 15.0);
    }

    void setDexpReleaseTimeMs_roundTrip() {
        TransmitModel t;
        t.setDexpReleaseTimeMs(250.0);
        QCOMPARE(t.dexpReleaseTimeMs(), 250.0);
    }

    // ── Signal emission ────────────────────────────────────────────

    void setDexpEnabled_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpEnabledChanged);
        t.setDexpEnabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), true);
    }

    void setDexpDetectorTauMs_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpDetectorTauMsChanged);
        t.setDexpDetectorTauMs(45.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toDouble(), 45.0);
    }

    void setDexpAttackTimeMs_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpAttackTimeMsChanged);
        t.setDexpAttackTimeMs(10.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toDouble(), 10.0);
    }

    void setDexpReleaseTimeMs_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpReleaseTimeMsChanged);
        t.setDexpReleaseTimeMs(300.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toDouble(), 300.0);
    }

    // ── Idempotent guards (no signal on same value) ────────────────

    void idempotent_dexpEnabled_default_noSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpEnabledChanged);
        t.setDexpEnabled(false);  // default = false
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_dexpDetectorTauMs_default_noSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpDetectorTauMsChanged);
        t.setDexpDetectorTauMs(20.0);  // default = 20
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_dexpAttackTimeMs_default_noSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpAttackTimeMsChanged);
        t.setDexpAttackTimeMs(2.0);  // default = 2
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_dexpReleaseTimeMs_default_noSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpReleaseTimeMsChanged);
        t.setDexpReleaseTimeMs(100.0);  // default = 100
        QCOMPARE(spy.count(), 0);
    }

    // ── Range clamps ───────────────────────────────────────────────
    // udDEXPDetTau:  1..100  (setup.Designer.cs:45078-45087 [v2.10.3.13])
    // udDEXPAttack:  2..100  (setup.Designer.cs:45035-45044 [v2.10.3.13])
    // udDEXPRelease: 2..1000 (setup.Designer.cs:44975-44984 [v2.10.3.13])

    void dexpDetectorTauMs_clampBelowMin() {
        TransmitModel t;
        t.setDexpDetectorTauMs(0.5);
        QCOMPARE(t.dexpDetectorTauMs(), TransmitModel::kDexpDetectorTauMsMin);
    }

    void dexpDetectorTauMs_clampAboveMax() {
        TransmitModel t;
        t.setDexpDetectorTauMs(150.0);
        QCOMPARE(t.dexpDetectorTauMs(), TransmitModel::kDexpDetectorTauMsMax);
    }

    void dexpAttackTimeMs_clampBelowMin() {
        TransmitModel t;
        t.setDexpAttackTimeMs(0.1);
        QCOMPARE(t.dexpAttackTimeMs(), TransmitModel::kDexpAttackTimeMsMin);
    }

    void dexpAttackTimeMs_clampAboveMax() {
        TransmitModel t;
        t.setDexpAttackTimeMs(500.0);
        QCOMPARE(t.dexpAttackTimeMs(), TransmitModel::kDexpAttackTimeMsMax);
    }

    void dexpReleaseTimeMs_clampBelowMin() {
        TransmitModel t;
        t.setDexpReleaseTimeMs(0.0);
        QCOMPARE(t.dexpReleaseTimeMs(), TransmitModel::kDexpReleaseTimeMsMin);
    }

    void dexpReleaseTimeMs_clampAboveMax() {
        TransmitModel t;
        t.setDexpReleaseTimeMs(9999.0);
        QCOMPARE(t.dexpReleaseTimeMs(), TransmitModel::kDexpReleaseTimeMsMax);
    }

    // ── Persistence across instances ──────────────────────────────
    // dexpEnabled IS persisted (no PTT-safety carve-out, unlike voxEnabled).

    void dexpEnabled_persistsAcrossInstances() {
        {
            TransmitModel t;
            t.loadFromSettings(kMac);
            t.setDexpEnabled(true);  // flip from default false
        }
        TransmitModel t2;
        t2.loadFromSettings(kMac);
        QCOMPARE(t2.dexpEnabled(), true);
    }

    void dexpDetectorTauMs_persistsAcrossInstances() {
        {
            TransmitModel t;
            t.loadFromSettings(kMac);
            t.setDexpDetectorTauMs(45.0);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMac);
        QCOMPARE(t2.dexpDetectorTauMs(), 45.0);
    }

    void dexpAttackTimeMs_persistsAcrossInstances() {
        {
            TransmitModel t;
            t.loadFromSettings(kMac);
            t.setDexpAttackTimeMs(8.5);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMac);
        QCOMPARE(t2.dexpAttackTimeMs(), 8.5);
    }

    void dexpReleaseTimeMs_persistsAcrossInstances() {
        {
            TransmitModel t;
            t.loadFromSettings(kMac);
            t.setDexpReleaseTimeMs(250.0);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMac);
        QCOMPARE(t2.dexpReleaseTimeMs(), 250.0);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // TASK 8 — DEXP gate-ratio properties (2 props)
    //   dexpExpansionRatioDb, dexpHysteresisRatioDb
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    // ── Defaults match Thetis ──────────────────────────────────────

    void default_dexpExpansionRatioDb_is10() {
        // setup.Designer.cs:44900-44904 [v2.10.3.13] - udDEXPExpansionRatio.Value=10.
        TransmitModel t;
        QCOMPARE(t.dexpExpansionRatioDb(), 10.0);
    }

    void default_dexpHysteresisRatioDb_is2() {
        // setup.Designer.cs:44869-44873 [v2.10.3.13] -
        //   udDEXPHysteresisRatio.Value=20 with DecimalPlaces=1, scale=65536
        //   -> displayed as 2.0.
        TransmitModel t;
        QCOMPARE(t.dexpHysteresisRatioDb(), 2.0);
    }

    // ── Round-trip + signal emission ──────────────────────────────

    void setDexpExpansionRatioDb_roundTrip_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpExpansionRatioDbChanged);
        t.setDexpExpansionRatioDb(15.0);
        QCOMPARE(t.dexpExpansionRatioDb(), 15.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toDouble(), 15.0);
    }

    void setDexpHysteresisRatioDb_roundTrip_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpHysteresisRatioDbChanged);
        t.setDexpHysteresisRatioDb(4.5);
        QCOMPARE(t.dexpHysteresisRatioDb(), 4.5);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toDouble(), 4.5);
    }

    // ── Idempotent guards ──────────────────────────────────────────

    void idempotent_dexpExpansionRatioDb_default_noSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpExpansionRatioDbChanged);
        t.setDexpExpansionRatioDb(10.0);  // default
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_dexpHysteresisRatioDb_default_noSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpHysteresisRatioDbChanged);
        t.setDexpHysteresisRatioDb(2.0);  // default
        QCOMPARE(spy.count(), 0);
    }

    // ── Range clamps ───────────────────────────────────────────────
    // udDEXPExpansionRatio:  0..30  (setup.Designer.cs:44885-44894 [v2.10.3.13])
    // udDEXPHysteresisRatio: 0..10  (setup.Designer.cs:44854-44863 [v2.10.3.13])

    void dexpExpansionRatioDb_clampBelowMin() {
        TransmitModel t;
        t.setDexpExpansionRatioDb(-5.0);
        QCOMPARE(t.dexpExpansionRatioDb(), TransmitModel::kDexpExpansionRatioDbMin);
    }

    void dexpExpansionRatioDb_clampAboveMax() {
        TransmitModel t;
        t.setDexpExpansionRatioDb(99.0);
        QCOMPARE(t.dexpExpansionRatioDb(), TransmitModel::kDexpExpansionRatioDbMax);
    }

    void dexpHysteresisRatioDb_clampBelowMin() {
        TransmitModel t;
        t.setDexpHysteresisRatioDb(-1.0);
        QCOMPARE(t.dexpHysteresisRatioDb(), TransmitModel::kDexpHysteresisRatioDbMin);
    }

    void dexpHysteresisRatioDb_clampAboveMax() {
        TransmitModel t;
        t.setDexpHysteresisRatioDb(50.0);
        QCOMPARE(t.dexpHysteresisRatioDb(), TransmitModel::kDexpHysteresisRatioDbMax);
    }

    // ── Persistence across instances ──────────────────────────────

    void dexpExpansionRatioDb_persistsAcrossInstances() {
        {
            TransmitModel t;
            t.loadFromSettings(kMac);
            t.setDexpExpansionRatioDb(20.0);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMac);
        QCOMPARE(t2.dexpExpansionRatioDb(), 20.0);
    }

    void dexpHysteresisRatioDb_persistsAcrossInstances() {
        {
            TransmitModel t;
            t.loadFromSettings(kMac);
            t.setDexpHysteresisRatioDb(5.5);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMac);
        QCOMPARE(t2.dexpHysteresisRatioDb(), 5.5);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // TASK 9 — DEXP look-ahead properties (2 props)
    //   dexpLookAheadEnabled, dexpLookAheadMs
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    // ── Defaults match Thetis ──────────────────────────────────────

    void default_dexpLookAheadEnabled_isTrue() {
        // setup.Designer.cs:44808 [v2.10.3.13] - chkDEXPLookAheadEnable.Checked=true.
        // This is the ONLY DEXP boolean defaulting true.
        TransmitModel t;
        QCOMPARE(t.dexpLookAheadEnabled(), true);
    }

    void default_dexpLookAheadMs_is60() {
        // setup.Designer.cs:44788 [v2.10.3.13] - udDEXPLookAhead.Value=60.
        TransmitModel t;
        QCOMPARE(t.dexpLookAheadMs(), 60.0);
    }

    // ── Round-trip + signal emission ──────────────────────────────

    void setDexpLookAheadEnabled_roundTrip_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpLookAheadEnabledChanged);
        t.setDexpLookAheadEnabled(false);  // flip from default true
        QCOMPARE(t.dexpLookAheadEnabled(), false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), false);
    }

    void setDexpLookAheadMs_roundTrip_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpLookAheadMsChanged);
        t.setDexpLookAheadMs(120.0);
        QCOMPARE(t.dexpLookAheadMs(), 120.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toDouble(), 120.0);
    }

    // ── Idempotent guards ──────────────────────────────────────────

    void idempotent_dexpLookAheadEnabled_default_noSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpLookAheadEnabledChanged);
        t.setDexpLookAheadEnabled(true);  // default
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_dexpLookAheadMs_default_noSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpLookAheadMsChanged);
        t.setDexpLookAheadMs(60.0);  // default
        QCOMPARE(spy.count(), 0);
    }

    // ── Range clamps ───────────────────────────────────────────────
    // udDEXPLookAhead: 10..999 (setup.Designer.cs:44773-44782 [v2.10.3.13])

    void dexpLookAheadMs_clampBelowMin() {
        TransmitModel t;
        t.setDexpLookAheadMs(1.0);
        QCOMPARE(t.dexpLookAheadMs(), TransmitModel::kDexpLookAheadMsMin);
    }

    void dexpLookAheadMs_clampAboveMax() {
        TransmitModel t;
        t.setDexpLookAheadMs(5000.0);
        QCOMPARE(t.dexpLookAheadMs(), TransmitModel::kDexpLookAheadMsMax);
    }

    // ── Persistence across instances ──────────────────────────────

    void dexpLookAheadEnabled_persistsAcrossInstances() {
        {
            TransmitModel t;
            t.loadFromSettings(kMac);
            t.setDexpLookAheadEnabled(false);  // flip from default true
        }
        TransmitModel t2;
        t2.loadFromSettings(kMac);
        QCOMPARE(t2.dexpLookAheadEnabled(), false);
    }

    void dexpLookAheadMs_persistsAcrossInstances() {
        {
            TransmitModel t;
            t.loadFromSettings(kMac);
            t.setDexpLookAheadMs(150.0);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMac);
        QCOMPARE(t2.dexpLookAheadMs(), 150.0);
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // TASK 10 — DEXP side-channel filter properties (3 props)
    //   dexpLowCutHz, dexpHighCutHz, dexpSideChannelFilterEnabled
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

    // ── Defaults match Thetis ──────────────────────────────────────

    void default_dexpLowCutHz_is500() {
        // setup.Designer.cs:45240 [v2.10.3.13] - udSCFLowCut.Value=500.
        TransmitModel t;
        QCOMPARE(t.dexpLowCutHz(), 500.0);
    }

    void default_dexpHighCutHz_is1500() {
        // setup.Designer.cs:45210 [v2.10.3.13] - udSCFHighCut.Value=1500.
        TransmitModel t;
        QCOMPARE(t.dexpHighCutHz(), 1500.0);
    }

    void default_dexpSideChannelFilterEnabled_isTrue() {
        // setup.Designer.cs:45250 [v2.10.3.13] - chkSCFEnable.Checked=true.
        TransmitModel t;
        QCOMPARE(t.dexpSideChannelFilterEnabled(), true);
    }

    // ── Round-trip + signal emission ──────────────────────────────

    void setDexpLowCutHz_roundTrip_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpLowCutHzChanged);
        t.setDexpLowCutHz(300.0);
        QCOMPARE(t.dexpLowCutHz(), 300.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toDouble(), 300.0);
    }

    void setDexpHighCutHz_roundTrip_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpHighCutHzChanged);
        t.setDexpHighCutHz(2500.0);
        QCOMPARE(t.dexpHighCutHz(), 2500.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toDouble(), 2500.0);
    }

    void setDexpSideChannelFilterEnabled_roundTrip_emitsSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpSideChannelFilterEnabledChanged);
        t.setDexpSideChannelFilterEnabled(false);  // flip from default true
        QCOMPARE(t.dexpSideChannelFilterEnabled(), false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toBool(), false);
    }

    // ── Idempotent guards ──────────────────────────────────────────

    void idempotent_dexpLowCutHz_default_noSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpLowCutHzChanged);
        t.setDexpLowCutHz(500.0);  // default
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_dexpHighCutHz_default_noSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpHighCutHzChanged);
        t.setDexpHighCutHz(1500.0);  // default
        QCOMPARE(spy.count(), 0);
    }

    void idempotent_dexpSideChannelFilterEnabled_default_noSignal() {
        TransmitModel t;
        QSignalSpy spy(&t, &TransmitModel::dexpSideChannelFilterEnabledChanged);
        t.setDexpSideChannelFilterEnabled(true);  // default
        QCOMPARE(spy.count(), 0);
    }

    // ── Range clamps ───────────────────────────────────────────────
    // udSCFLowCut + udSCFHighCut both: 100..10000 Hz
    // (setup.Designer.cs:45195-45234 [v2.10.3.13])

    void dexpLowCutHz_clampBelowMin() {
        TransmitModel t;
        t.setDexpLowCutHz(50.0);
        QCOMPARE(t.dexpLowCutHz(), TransmitModel::kDexpFilterCutHzMin);
    }

    void dexpLowCutHz_clampAboveMax() {
        TransmitModel t;
        t.setDexpLowCutHz(20000.0);
        QCOMPARE(t.dexpLowCutHz(), TransmitModel::kDexpFilterCutHzMax);
    }

    void dexpHighCutHz_clampBelowMin() {
        TransmitModel t;
        t.setDexpHighCutHz(50.0);
        QCOMPARE(t.dexpHighCutHz(), TransmitModel::kDexpFilterCutHzMin);
    }

    void dexpHighCutHz_clampAboveMax() {
        TransmitModel t;
        t.setDexpHighCutHz(20000.0);
        QCOMPARE(t.dexpHighCutHz(), TransmitModel::kDexpFilterCutHzMax);
    }

    // ── Persistence across instances ──────────────────────────────

    void dexpLowCutHz_persistsAcrossInstances() {
        {
            TransmitModel t;
            t.loadFromSettings(kMac);
            t.setDexpLowCutHz(400.0);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMac);
        QCOMPARE(t2.dexpLowCutHz(), 400.0);
    }

    void dexpHighCutHz_persistsAcrossInstances() {
        {
            TransmitModel t;
            t.loadFromSettings(kMac);
            t.setDexpHighCutHz(2200.0);
        }
        TransmitModel t2;
        t2.loadFromSettings(kMac);
        QCOMPARE(t2.dexpHighCutHz(), 2200.0);
    }

    void dexpSideChannelFilterEnabled_persistsAcrossInstances() {
        {
            TransmitModel t;
            t.loadFromSettings(kMac);
            t.setDexpSideChannelFilterEnabled(false);  // flip from default true
        }
        TransmitModel t2;
        t2.loadFromSettings(kMac);
        QCOMPARE(t2.dexpSideChannelFilterEnabled(), false);
    }
};

QTEST_APPLESS_MAIN(TstTransmitModelDexp)
#include "tst_transmit_model_dexp.moc"
