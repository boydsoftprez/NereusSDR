#include <QtTest/QtTest>
#include "core/HpsdrModel.h"

using namespace NereusSDR;

class TestHpsdrEnums : public QObject {
    Q_OBJECT
private slots:
    void integerValuesMatchThetis() {
        // Source: mi0bot/Thetis@Hermes-Lite enums.cs:109
        QCOMPARE(static_cast<int>(HPSDRModel::FIRST),        -1);
        QCOMPARE(static_cast<int>(HPSDRModel::HPSDR),         0);
        QCOMPARE(static_cast<int>(HPSDRModel::HERMES),        1);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN10),        2);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN10E),       3);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN100),       4);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN100B),      5);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN100D),      6);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN200D),      7);
        QCOMPARE(static_cast<int>(HPSDRModel::ORIONMKII),     8);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN7000D),     9);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN8000D),    10);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN_G2),      11);
        QCOMPARE(static_cast<int>(HPSDRModel::ANAN_G2_1K),   12);
        QCOMPARE(static_cast<int>(HPSDRModel::ANVELINAPRO3), 13);
        QCOMPARE(static_cast<int>(HPSDRModel::HERMESLITE),   14);
        QCOMPARE(static_cast<int>(HPSDRModel::REDPITAYA),    15);
        QCOMPARE(static_cast<int>(HPSDRModel::LAST),         16);

        // Source: enums.cs:388 + network.h:446 — reserved gap at 7..9
        QCOMPARE(static_cast<int>(HPSDRHW::Atlas),        0);
        QCOMPARE(static_cast<int>(HPSDRHW::Hermes),       1);
        QCOMPARE(static_cast<int>(HPSDRHW::HermesII),     2);
        QCOMPARE(static_cast<int>(HPSDRHW::Angelia),      3);
        QCOMPARE(static_cast<int>(HPSDRHW::Orion),        4);
        QCOMPARE(static_cast<int>(HPSDRHW::OrionMKII),    5);
        QCOMPARE(static_cast<int>(HPSDRHW::HermesLite),   6);
        QCOMPARE(static_cast<int>(HPSDRHW::Saturn),      10);
        QCOMPARE(static_cast<int>(HPSDRHW::SaturnMKII),  11);
        QCOMPARE(static_cast<int>(HPSDRHW::Unknown),    999);
    }

    void boardForModelCoversEveryModel() {
        for (int i = 0; i < static_cast<int>(HPSDRModel::LAST); ++i) {
            auto m = static_cast<HPSDRModel>(i);
            auto hw = boardForModel(m);
            QVERIFY2(hw != HPSDRHW::Unknown,
                     qPrintable(QString("HPSDRModel %1 maps to Unknown").arg(i)));
        }
    }

    void displayNameNonNullForEveryModel() {
        for (int i = 0; i < static_cast<int>(HPSDRModel::LAST); ++i) {
            auto m = static_cast<HPSDRModel>(i);
            const char* name = displayName(m);
            QVERIFY(name != nullptr);
            QVERIFY(std::string(name).size() > 0);
        }
    }
};

QTEST_APPLESS_MAIN(TestHpsdrEnums)
#include "tst_hpsdr_enums.moc"
