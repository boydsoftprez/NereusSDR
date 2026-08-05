// no-port-check: test fixture asserting HardwareProfile per-board init values
// against Thetis clsHardwareSpecific.cs:85-191 [v2.10.3.15] table.
// NereusSDR-original test structure; logic values sourced from Thetis.
#include <QtTest/QtTest>
#include "core/HardwareProfile.h"
#include "core/HpsdrModel.h"

using namespace NereusSDR;

// ---------------------------------------------------------------------------
// Data table sourced from Thetis clsHardwareSpecific.cs:85-191 [v2.10.3.15].
// Each row's values match the SetRxADC / SetMKIIBPF / SetADCSupply /
// LRAudioSwap / Hardware calls in the upstream switch statement.
// Upstream author tags preserved inline on the rows where they appear.
struct ExpectedInit {
    HPSDRModel model;
    int        adcCount;
    bool       mkiiBpf;
    int        adcSupplyVoltage;
    bool       lrAudioSwap;
    HPSDRHW    board;
};

// Rows sourced per-case from Thetis clsHardwareSpecific.cs [v2.10.3.15]:
//   :87  HERMES      :94  ANAN10      :101 ANAN10E     :108 ANAN100
//   :115 ANAN100B    :122 ANAN100D    :129 ANAN_G2E    :136 ANAN200D
//   :143 ORIONMKII   :150 ANAN7000D   :157 ANAN8000D   :164 ANAN_G2
//   :171 ANAN_G2_1K  :178 ANVELINAPRO3 :185 REDPITAYA
static const ExpectedInit kThetisInit[] = {
    // From Thetis clsHardwareSpecific.cs:87 [v2.10.3.15]
    {HPSDRModel::HERMES,       1, false, 33, true,  HPSDRHW::Hermes},
    // From Thetis clsHardwareSpecific.cs:94 [v2.10.3.15]
    {HPSDRModel::ANAN10,       1, false, 33, true,  HPSDRHW::Hermes},
    // From Thetis clsHardwareSpecific.cs:101 [v2.10.3.15]
    {HPSDRModel::ANAN10E,      1, false, 33, true,  HPSDRHW::HermesII},
    // From Thetis clsHardwareSpecific.cs:108 [v2.10.3.15]
    {HPSDRModel::ANAN100,      1, false, 33, true,  HPSDRHW::Hermes},
    // From Thetis clsHardwareSpecific.cs:115 [v2.10.3.15]
    {HPSDRModel::ANAN100B,     1, false, 33, true,  HPSDRHW::HermesII},
    // From Thetis clsHardwareSpecific.cs:122 [v2.10.3.15]
    {HPSDRModel::ANAN100D,     2, false, 33, false, HPSDRHW::Angelia},
    // From Thetis clsHardwareSpecific.cs:129 [v2.10.3.15] //N1GP G2E added
    {HPSDRModel::ANAN_G2E,     1, true,  33, false, HPSDRHW::HermesC10},
    // From Thetis clsHardwareSpecific.cs:136 [v2.10.3.15]
    {HPSDRModel::ANAN200D,     2, false, 50, false, HPSDRHW::Orion},
    // From Thetis clsHardwareSpecific.cs:143 [v2.10.3.15]
    {HPSDRModel::ORIONMKII,    2, true,  50, false, HPSDRHW::OrionMKII},
    // From Thetis clsHardwareSpecific.cs:150 [v2.10.3.15]
    {HPSDRModel::ANAN7000D,    2, true,  50, false, HPSDRHW::OrionMKII},
    // From Thetis clsHardwareSpecific.cs:157 [v2.10.3.15]
    {HPSDRModel::ANAN8000D,    2, true,  50, false, HPSDRHW::OrionMKII},
    // From Thetis clsHardwareSpecific.cs:164 [v2.10.3.15]
    {HPSDRModel::ANAN_G2,      2, true,  50, false, HPSDRHW::Saturn},
    // From Thetis clsHardwareSpecific.cs:171 [v2.10.3.15] //G8NJJ: likely to need further changes for PA
    {HPSDRModel::ANAN_G2_1K,   2, true,  50, false, HPSDRHW::Saturn},
    // From Thetis clsHardwareSpecific.cs:178 [v2.10.3.15]
    {HPSDRModel::ANVELINAPRO3, 2, true,  50, false, HPSDRHW::OrionMKII},
    // From Thetis clsHardwareSpecific.cs:185 [v2.10.3.15] //DH1KLM
    // NetworkIO.SetMKIIBPF(0); // DH1KLM: changed for compatibility reasons for OpenHPSDR compat. DIY PA/Filter boards
    {HPSDRModel::REDPITAYA,    2, false, 50, false, HPSDRHW::OrionMKII},
};
static constexpr int kThetisInitCount = static_cast<int>(sizeof(kThetisInit) / sizeof(kThetisInit[0]));

class TestHardwareProfile : public QObject {
    Q_OBJECT
private slots:
    void initValuesMatchThetis_data() {
        QTest::addColumn<int>("modelIdx");
        for (int i = 0; i < kThetisInitCount; ++i) {
            // Use the displayName string as the row tag
            const char* tag = nullptr;
            switch (kThetisInit[i].model) {
                case HPSDRModel::HERMES:       tag = "HERMES"; break;
                case HPSDRModel::ANAN10:       tag = "ANAN10"; break;
                case HPSDRModel::ANAN10E:      tag = "ANAN10E"; break;
                case HPSDRModel::ANAN100:      tag = "ANAN100"; break;
                case HPSDRModel::ANAN100B:     tag = "ANAN100B"; break;
                case HPSDRModel::ANAN100D:     tag = "ANAN100D"; break;
                case HPSDRModel::ANAN_G2E:     tag = "ANAN_G2E"; break;
                case HPSDRModel::ANAN200D:     tag = "ANAN200D"; break;
                case HPSDRModel::ORIONMKII:    tag = "ORIONMKII"; break;
                case HPSDRModel::ANAN7000D:    tag = "ANAN7000D"; break;
                case HPSDRModel::ANAN8000D:    tag = "ANAN8000D"; break;
                case HPSDRModel::ANAN_G2:      tag = "ANAN_G2"; break;
                case HPSDRModel::ANAN_G2_1K:   tag = "ANAN_G2_1K"; break;
                case HPSDRModel::ANVELINAPRO3: tag = "ANVELINAPRO3"; break;
                case HPSDRModel::REDPITAYA:    tag = "REDPITAYA"; break;
                default:                       tag = "unknown"; break;
            }
            QTest::newRow(tag) << i;
        }
    }

    void initValuesMatchThetis() {
        QFETCH(int, modelIdx);
        const ExpectedInit& expected = kThetisInit[modelIdx];
        HardwareProfile profile = profileForModel(expected.model);
        QCOMPARE(profile.adcCount,         expected.adcCount);
        QCOMPARE(profile.mkiiBpf,          expected.mkiiBpf);
        QCOMPARE(profile.adcSupplyVoltage, expected.adcSupplyVoltage);
        QCOMPARE(profile.lrAudioSwap,      expected.lrAudioSwap);
        QCOMPARE(profile.effectiveBoard,   expected.board);
    }

    void g2e_effectiveBoardIsHermesC10() {
        // From Thetis clsHardwareSpecific.cs:129-135 [v2.10.3.15] //N1GP G2E added
        HardwareProfile p = profileForModel(HPSDRModel::ANAN_G2E);
        QCOMPARE(p.effectiveBoard, HPSDRHW::HermesC10);
        QCOMPARE(p.adcCount, 1);
        QVERIFY(p.mkiiBpf);         // SetMKIIBPF(1) — G2E uses the OrionMKII BPF board
        QCOMPARE(p.adcSupplyVoltage, 33);
        QVERIFY(!p.lrAudioSwap);    // LRAudioSwap(0) — modern board, no swap
    }
};

QTEST_APPLESS_MAIN(TestHardwareProfile)
#include "tst_hardware_profile.moc"
