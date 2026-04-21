#include <QtTest/QtTest>
#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"

using namespace NereusSDR;

class TestBoardCapabilities : public QObject {
    Q_OBJECT
private slots:

    void forBoardReturnsMatchingEntry_data() {
        QTest::addColumn<int>("hw");
        QTest::newRow("Atlas")      << int(HPSDRHW::Atlas);
        QTest::newRow("Hermes")     << int(HPSDRHW::Hermes);
        QTest::newRow("HermesII")   << int(HPSDRHW::HermesII);
        QTest::newRow("Angelia")    << int(HPSDRHW::Angelia);
        QTest::newRow("Orion")      << int(HPSDRHW::Orion);
        QTest::newRow("OrionMKII")  << int(HPSDRHW::OrionMKII);
        QTest::newRow("HermesLite") << int(HPSDRHW::HermesLite);
        QTest::newRow("Saturn")     << int(HPSDRHW::Saturn);
        QTest::newRow("SaturnMKII") << int(HPSDRHW::SaturnMKII);
    }
    void forBoardReturnsMatchingEntry() {
        QFETCH(int, hw);
        const auto& caps = BoardCapsTable::forBoard(static_cast<HPSDRHW>(hw));
        QCOMPARE(static_cast<int>(caps.board), hw);
    }

    void forModelMatchesBoardForModel() {
        for (int i = 0; i < int(HPSDRModel::LAST); ++i) {
            auto m = static_cast<HPSDRModel>(i);
            const auto& caps = BoardCapsTable::forModel(m);
            QCOMPARE(caps.board, boardForModel(m));
        }
    }

    void sampleRatesAreSaneForEveryBoard() {
        for (const auto& caps : BoardCapsTable::all()) {
            QVERIFY(caps.sampleRates[0] == 48000);
            int prev = 0;
            for (int sr : caps.sampleRates) {
                if (sr == 0) { break; }
                QVERIFY(sr > prev);
                QVERIFY(sr <= caps.maxSampleRate);
                prev = sr;
            }
        }
    }

    void attenuatorAbsentImpliesZeroRange() {
        for (const auto& caps : BoardCapsTable::all()) {
            if (!caps.attenuator.present) {
                QCOMPARE(caps.attenuator.minDb, 0);
                QCOMPARE(caps.attenuator.maxDb, 0);
            } else {
                QVERIFY(caps.attenuator.minDb <= caps.attenuator.maxDb);
                QVERIFY(caps.attenuator.stepDb > 0);
            }
        }
    }

    void diversityRequiresTwoAdcs() {
        for (const auto& caps : BoardCapsTable::all()) {
            if (caps.hasDiversityReceiver) {
                QCOMPARE(caps.adcCount, 2);
            }
        }
    }

    void alexTxRoutingImpliesAlexFilters() {
        for (const auto& caps : BoardCapsTable::all()) {
            if (caps.hasAlexTxRouting) {
                QVERIFY(caps.hasAlexFilters);
            }
        }
    }

    void displayNameAndCitationNonNull() {
        for (const auto& caps : BoardCapsTable::all()) {
            QVERIFY(caps.displayName != nullptr);
            QVERIFY(caps.sourceCitation != nullptr);
            QVERIFY(std::string(caps.displayName).size() > 0);
            QVERIFY(std::string(caps.sourceCitation).size() > 0);
        }
    }

    void firmwareMinIsLessOrEqualKnownGood() {
        for (const auto& caps : BoardCapsTable::all()) {
            QVERIFY(caps.minFirmwareVersion <= caps.knownGoodFirmware);
        }
    }

    void hermesLiteHasHl2Extras() {
        const auto& caps = BoardCapsTable::forBoard(HPSDRHW::HermesLite);
        QVERIFY(caps.hasBandwidthMonitor);
        QVERIFY(caps.hasIoBoardHl2);
        QVERIFY(!caps.hasAlexFilters);
        QCOMPARE(caps.ocOutputCount, 0);
        // HL2 maxDb bumped to 63 (6-bit range) — Phase 3P-A Task 11
        QCOMPARE(caps.attenuator.maxDb, 63);
    }

    // Phase 3P-A Task 11: Attenuator encoding parameters —
    // mask + enableBit + moxBranchesAtt per board.
    // From spec §6.3.2 (HL2 vs Standard) and §6.3.3 (RedPitaya gate).
    void hermes_attenuator_5bit_ramdor_encoding() {
        const auto& caps = BoardCapsTable::forBoard(HPSDRHW::Hermes);
        QCOMPARE(int(caps.attenuator.mask),       0x1F);
        QCOMPARE(int(caps.attenuator.enableBit),  0x20);
        QVERIFY(!caps.attenuator.moxBranchesAtt);
        QCOMPARE(caps.attenuator.maxDb, 31);
    }

    void hl2_attenuator_6bit_mi0bot_encoding() {
        const auto& caps = BoardCapsTable::forBoard(HPSDRHW::HermesLite);
        QCOMPARE(int(caps.attenuator.mask),       0x3F);
        QCOMPARE(int(caps.attenuator.enableBit),  0x40);
        QVERIFY(caps.attenuator.moxBranchesAtt);
        QCOMPARE(caps.attenuator.maxDb, 63);
    }

    void orionmkii_attenuator_5bit_no_mox_branch() {
        const auto& caps = BoardCapsTable::forBoard(HPSDRHW::OrionMKII);
        QCOMPARE(int(caps.attenuator.mask),       0x1F);
        QCOMPARE(int(caps.attenuator.enableBit),  0x20);
        QVERIFY(!caps.attenuator.moxBranchesAtt);
    }

    void angeliaHasDiversityAndPureSignal() {
        const auto& caps = BoardCapsTable::forBoard(HPSDRHW::Angelia);
        QVERIFY(caps.hasDiversityReceiver);
        QVERIFY(caps.hasPureSignal);
        QCOMPARE(caps.adcCount, 2);
    }

    // Phase 3P-B Task 6: P2-specific capability fields
    void saturn_has_p2_fields_default() {
        const auto& caps = BoardCapsTable::forBoard(HPSDRHW::Saturn);
        QVERIFY(!caps.p2PreampPerAdc);  // Saturn is single-ADC at the wire layer
        QVERIFY(caps.p2SaturnBpf1Edges.isEmpty());  // user populates via Setup
    }

    void orionmkii_has_p2_per_adc_preamp() {
        const auto& caps = BoardCapsTable::forBoard(HPSDRHW::OrionMKII);
        QVERIFY(caps.p2PreampPerAdc);  // OrionMKII family supports per-ADC preamp
    }

    void hl2_no_p2_fields() {
        // HL2 is P1-only — p2PreampPerAdc default false, no Saturn BPF1
        const auto& caps = BoardCapsTable::forBoard(HPSDRHW::HermesLite);
        QVERIFY(!caps.p2PreampPerAdc);
    }
};

QTEST_APPLESS_MAIN(TestBoardCapabilities)
#include "tst_board_capabilities.moc"
