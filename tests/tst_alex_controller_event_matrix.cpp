// =================================================================
// tests/tst_alex_controller_event_matrix.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic B Task 17: AlexController event trigger matrix
// coverage per docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §10.
//
// Three matrix rows under test:
//   1. Slice retune to a different band triggers recompute (bpfStateChanged emitted).
//   2. Slice retune within same band does NOT emit (idempotency guard).
//   3. Wideband active overrides operator ForceBand mode.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/accessories/AlexController.h"

using namespace NereusSDR;

class TestAlexControllerEventMatrix : public QObject {
    Q_OBJECT
private slots:
    void slice_freq_change_to_different_band_triggers_recompute()
    {
        AlexController alex;
        std::array<Band, 5> initialSlices {Band::Band20m, Band::Count, Band::Count, Band::Count, Band::Count};
        alex.notifySlicesOnAdc(0, initialSlices);

        QSignalSpy spy(&alex, &AlexController::bpfStateChanged);

        // Slice A retunes from 20m to 40m: new band, effective state must change.
        std::array<Band, 5> updated {Band::Band40m, Band::Count, Band::Count, Band::Count, Band::Count};
        alex.notifySlicesOnAdc(0, updated);

        QVERIFY(spy.count() >= 1);
        QCOMPARE(alex.adcState(0).currentBpfBand, Band::Band40m);
    }

    void slice_freq_change_within_same_band_does_not_emit()
    {
        AlexController alex;
        // Prime state with a single 20m slice so effective = Filtered/Band20m.
        std::array<Band, 5> slices {Band::Band20m, Band::Count, Band::Count, Band::Count, Band::Count};
        alex.notifySlicesOnAdc(0, slices);

        // Second notify with identical slice list. recomputeBpf sees no change in
        // effective or reasonText and must NOT emit bpfStateChanged.
        QSignalSpy spy(&alex, &AlexController::bpfStateChanged);
        alex.notifySlicesOnAdc(0, slices);

        QCOMPARE(spy.count(), 0);
    }

    void wideband_active_overrides_operator_force_filter()
    {
        AlexController alex;
        // Operator sets ForceBand; wideband then activates on the same ADC.
        alex.setBpfMode(0, AlexController::BpfMode::ForceBand);
        alex.setWidebandActive(0, true);

        // Wideband priority must override ForceBand.
        QCOMPARE(alex.adcState(0).effective, AlexController::BpfEffective::WidebandLocked);
    }
};

QTEST_MAIN(TestAlexControllerEventMatrix)
#include "tst_alex_controller_event_matrix.moc"
