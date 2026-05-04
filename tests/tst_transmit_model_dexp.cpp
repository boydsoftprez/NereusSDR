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
};

QTEST_APPLESS_MAIN(TstTransmitModelDexp)
#include "tst_transmit_model_dexp.moc"
