// =================================================================
// tests/tst_alex_controller_per_adc_bpf.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic B Task 11+: AlexController per-ADC BPF state machine
// per docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §4.
// =================================================================

#include <QtTest/QtTest>
#include "core/accessories/AlexController.h"

using namespace NereusSDR;

class TestAlexControllerPerAdcBpf : public QObject {
    Q_OBJECT
private slots:
    void alex_adc_state_struct_exists()
    {
        AlexController::AlexAdcState s{};
        s.mode = AlexController::BpfMode::Auto;
        s.effective = AlexController::BpfEffective::Filtered;
        s.currentBpfBand = Band::Band20m;
        QCOMPARE(s.mode, AlexController::BpfMode::Auto);
    }
};

QTEST_MAIN(TestAlexControllerPerAdcBpf)
#include "tst_alex_controller_per_adc_bpf.moc"
