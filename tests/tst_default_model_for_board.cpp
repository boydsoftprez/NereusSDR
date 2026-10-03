// no-port-check: test fixture; cite comments reference upstream
// setup.Designer.cs / setup.cs / clsHardwareSpecific.cs as documentation of
// the auto-default behaviour under test.
//
// Issue #202 deep-fix regression coverage: defaultModelForBoard() must NOT
// auto-pick HPSDRModel::ORIONMKII for an OrionMKII boardtype.  The bare
// ORIONMKII enum is excluded from Thetis's user-facing comboRadioModel.Items
// list (setup.Designer.cs:8515-8528 [v2.10.3.13]) and from
// StringModelToEnum (clsHardwareSpecific.cs:316-351 [v2.10.3.13]).  It
// shares the Hermes 41.x dB PA-gain row at clsHardwareSpecific.cs:471-486
// [v2.10.3.13]; productized OrionMKII boards (ANAN-7000DLE / 8000DLE /
// AnvelinaPro3 / RedPitaya) all have ~50 dB PA gain.  Auto-defaulting
// to ORIONMKII over-drove those PAs — exactly w3ub's "max regardless of
// slider" report on a 7000DLE Mk II.

#include <QtTest/QtTest>
#include "core/HardwareProfile.h"
#include "core/HpsdrModel.h"
#include "core/BoardCapabilities.h"

using NereusSDR::HPSDRHW;
using NereusSDR::HPSDRModel;
using NereusSDR::defaultModelForBoard;
namespace BoardCapsTable = NereusSDR::BoardCapsTable;
using NereusSDR::HardwareProfile;

class TestDefaultModelForBoard : public QObject {
    Q_OBJECT
private slots:

    // ── Atlas / Hermes / HermesII / Angelia / Orion / Saturn / SaturnMKII ──
    // These boardtypes have either a single or unambiguous default; the
    // auto-pick lands on the lowest-enum-value matching HPSDRModel.
    void atlas_returns_HPSDR() {
        QCOMPARE(defaultModelForBoard(HPSDRHW::Atlas), HPSDRModel::HPSDR);
    }
    void hermes_returns_HERMES() {
        QCOMPARE(defaultModelForBoard(HPSDRHW::Hermes), HPSDRModel::HERMES);
    }
    void hermesII_returns_ANAN10E() {
        // ANAN10E (idx 3) precedes ANAN100B (idx 5) in enum order.
        QCOMPARE(defaultModelForBoard(HPSDRHW::HermesII), HPSDRModel::ANAN10E);
    }
    void angelia_returns_ANAN100D() {
        QCOMPARE(defaultModelForBoard(HPSDRHW::Angelia), HPSDRModel::ANAN100D);
    }
    void orion_returns_ANAN200D() {
        QCOMPARE(defaultModelForBoard(HPSDRHW::Orion), HPSDRModel::ANAN200D);
    }
    void hermesLite_returns_HERMESLITE() {
        QCOMPARE(defaultModelForBoard(HPSDRHW::HermesLite),
                 HPSDRModel::HERMESLITE);
    }
    void saturn_returns_ANAN_G2() {
        QCOMPARE(defaultModelForBoard(HPSDRHW::Saturn), HPSDRModel::ANAN_G2);
    }
    void saturnMKII_returns_ANAN_G2_via_specialcase() {
        // SaturnMKII has no dedicated HPSDRModel — special-cased to ANAN_G2.
        QCOMPARE(defaultModelForBoard(HPSDRHW::SaturnMKII),
                 HPSDRModel::ANAN_G2);
    }

    // ── #202 root cause: OrionMKII must NOT return ORIONMKII ────────────────
    //
    // The bare ORIONMKII enum is intentionally skipped in the auto-pick
    // walk (HardwareProfile.cpp::defaultModelForBoard).  Iteration falls
    // through to ANAN7000D (idx 9, lowest matching enum after ORIONMKII
    // is skipped).  Both ANAN7000D and ANAN8000D map to HPSDRHW::OrionMKII
    // via boardForModel, but ANAN7000D is enum-first.
    //
    // Cite (the exclusion rationale):
    //   - Thetis comboRadioModel.Items at setup.Designer.cs:8515-8528
    //     [v2.10.3.13] omits "ORIONMKII".
    //   - StringModelToEnum at clsHardwareSpecific.cs:316-351 [v2.10.3.13]
    //     has no string entry for "ORIONMKII".
    //   - database.cs:10350 [v2.10.3.13] notes: "not implemented in
    //     comboRadioModel list items".
    //   - DefaultPAGainsForBands(ORIONMKII) at clsHardwareSpecific.cs:
    //     471-486 [v2.10.3.13] shares the Hermes 41.x dB row, which is
    //     ~9 dB low for productized OrionMKII PAs (kAnan7000dRow ~50 dB).
    void orionMKII_returns_ANAN7000D_not_ORIONMKII() {
        const auto picked = defaultModelForBoard(HPSDRHW::OrionMKII);
        QCOMPARE(picked, HPSDRModel::ANAN7000D);
        // Pin: must NOT be ORIONMKII.  Regression guard for #202.
        QVERIFY(picked != HPSDRModel::ORIONMKII);
    }

    // ── Plan Task 15: the HL2 receive-only kit ─────────────────────────────
    //
    // mi0bot-Thetis models no separate receive-only HL2: its HPSDRHW enum has
    // one HL2 value, HermesLite = 6 (enums.cs:396 [v2.10.3.13-beta2]), and
    // every HL2 path keys on HPSDRModel.HERMESLITE. Receive-only is the
    // operator's RXOnly toggle (console.cs:15374-15395). So the kit resolves
    // to HERMESLITE, not HERMES, and keeps its own row, whose isRxOnlySku is
    // the NereusSDR hard block on transmit.
    void hermesLiteRxOnly_returns_HERMESLITE() {
        QCOMPARE(defaultModelForBoard(HPSDRHW::HermesLiteRxOnly),
                 HPSDRModel::HERMESLITE);
    }

    void hermesLiteRxOnly_profile_keeps_the_kit_row() {
        const HardwareProfile p = NereusSDR::profileForRadio(
            HPSDRHW::HermesLiteRxOnly, defaultModelForBoard(HPSDRHW::HermesLiteRxOnly));
        QCOMPARE(p.model, HPSDRModel::HERMESLITE);
        QCOMPARE(p.effectiveBoard, HPSDRHW::HermesLiteRxOnly);
        QVERIFY(p.caps != nullptr);
        QCOMPARE(p.caps, &BoardCapsTable::forBoard(HPSDRHW::HermesLiteRxOnly));
        QVERIFY(p.caps->isRxOnlySku);
        // The rest of the HL2 model init applies unchanged
        // (clsHardwareSpecific.cs HERMESLITE branch).
        const HardwareProfile hl2 = NereusSDR::profileForModel(HPSDRModel::HERMESLITE);
        QCOMPARE(p.adcCount, hl2.adcCount);
        QCOMPARE(p.mkiiBpf, hl2.mkiiBpf);
        QCOMPARE(p.adcSupplyVoltage, hl2.adcSupplyVoltage);
        QCOMPARE(p.lrAudioSwap, hl2.lrAudioSwap);
    }

    // A standard HL2 and every other board are unchanged by the kit rule.
    void profileForRadio_is_profileForModel_off_the_kit() {
        for (int i = int(HPSDRModel::FIRST) + 1; i < int(HPSDRModel::LAST); ++i) {
            const auto m = static_cast<HPSDRModel>(i);
            const HardwareProfile a = NereusSDR::profileForRadio(NereusSDR::boardForModel(m), m);
            const HardwareProfile b = NereusSDR::profileForModel(m);
            QCOMPARE(a.model, b.model);
            QCOMPARE(a.effectiveBoard, b.effectiveBoard);
            QCOMPARE(a.caps, b.caps);
        }
    }

    // A remote window on a Core running the kit gets the same profile.
    void hermesLiteRxOnly_station_profile_matches_local() {
        for (HPSDRModel reported : {HPSDRModel::HERMESLITE, HPSDRModel::FIRST}) {
            const HardwareProfile p =
                NereusSDR::profileForStation(HPSDRHW::HermesLiteRxOnly, reported);
            QCOMPARE(p.model, HPSDRModel::HERMESLITE);
            QCOMPARE(p.effectiveBoard, HPSDRHW::HermesLiteRxOnly);
            QCOMPARE(p.caps, &BoardCapsTable::forBoard(HPSDRHW::HermesLiteRxOnly));
        }
    }

    // The connection panel's model list for the kit offers the HL2.
    void hermesLiteRxOnly_compatible_models_is_the_HL2() {
        QCOMPARE(NereusSDR::compatibleModels(HPSDRHW::HermesLiteRxOnly),
                 QList<HPSDRModel>{HPSDRModel::HERMESLITE});
    }
};

QTEST_MAIN(TestDefaultModelForBoard)
#include "tst_default_model_for_board.moc"
