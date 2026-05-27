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

    // Phase 3F Task 2: per-SKU maxSlices population tests.
    // Values from docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §2.

    void hl2_max_slices_is_1()
    {
        const auto caps = capabilitiesFor(HPSDRModel::HERMESLITE);
        QCOMPARE(caps.maxSlices, 1);
    }

    void metis_max_slices_is_3()
    {
        const auto caps = capabilitiesFor(HPSDRModel::HPSDR);
        QCOMPARE(caps.maxSlices, 3);
    }

    void hermes_max_slices_is_4()
    {
        const auto caps = capabilitiesFor(HPSDRModel::HERMES);
        QCOMPARE(caps.maxSlices, 4);
    }

    void hermesII_max_slices_is_2()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN10E);
        QCOMPARE(caps.maxSlices, 2);
    }

    void angelia_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN100D);
        QCOMPARE(caps.maxSlices, 5);
    }

    void orion_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN200D);
        QCOMPARE(caps.maxSlices, 5);
    }

    void orionMkII_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ORIONMKII);
        QCOMPARE(caps.maxSlices, 5);
    }

    void anan_g2_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN_G2);
        QCOMPARE(caps.maxSlices, 5);
    }

    void anan_g2e_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN_G2E);
        QCOMPARE(caps.maxSlices, 5);
    }

    void anvelinapro3_max_slices_is_5()
    {
        // AnvelinaPro3 maps to kOrionMKII (P2, dual-ADC, 7 DDCs).
        // Design doc §2 table: AnvelinaPro3 row = 5 slices (same as OrionMkII).
        const auto caps = capabilitiesFor(HPSDRModel::ANVELINAPRO3);
        QCOMPARE(caps.maxSlices, 5);
    }

    void anan_7000d_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN7000D);
        QCOMPARE(caps.maxSlices, 5);
    }

    void anan_8000d_max_slices_is_5()
    {
        const auto caps = capabilitiesFor(HPSDRModel::ANAN8000D);
        QCOMPARE(caps.maxSlices, 5);
    }

private:
    static BoardCapabilities capabilitiesFor(HPSDRModel m)
    {
        // forModel returns a const reference; copy it for comparison.
        return BoardCapsTable::forModel(m);
    }
};

QTEST_MAIN(TestBoardCapabilitiesPhase3F)
#include "tst_board_capabilities_phase3f.moc"
