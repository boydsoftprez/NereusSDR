// =================================================================
// tests/tst_board_capabilities_phase3f.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic A Task 1: verify BoardCapabilities gains
// maxSlices and widebandAdcs fields with correct per-SKU values per
// docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §2.
// =================================================================

#include <QtTest/QtTest>
#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"

using namespace NereusSDR;

class TestBoardCapabilitiesPhase3F : public QObject {
    Q_OBJECT
private slots:
    void struct_has_max_slices_field()
    {
        BoardCapabilities caps{};
        caps.maxSlices = 5;
        QCOMPARE(caps.maxSlices, 5);
    }

    void struct_has_wideband_adcs_field()
    {
        BoardCapabilities caps{};
        caps.widebandAdcs = 2;
        QCOMPARE(caps.widebandAdcs, 2);
    }
};

QTEST_MAIN(TestBoardCapabilitiesPhase3F)
#include "tst_board_capabilities_phase3f.moc"
