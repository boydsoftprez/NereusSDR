// no-port-check: test fixture asserting BoardCapabilities per-board fields against Thetis source rules
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
        QTest::newRow("HermesLite")       << int(HPSDRHW::HermesLite);
        QTest::newRow("HermesLiteRxOnly") << int(HPSDRHW::HermesLiteRxOnly);
        QTest::newRow("Saturn")           << int(HPSDRHW::Saturn);
        QTest::newRow("SaturnMKII")       << int(HPSDRHW::SaturnMKII);
        QTest::newRow("Andromeda")        << int(HPSDRHW::Andromeda);
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
        // HL2 user-facing range: signed -28..+31 dB (issue #175 follow-up —
        // capped at +31 per maintainer approval; mi0bot's +32 widening at
        // console.cs:11043 [v2.10.3.13-beta2] is an off-by-one upstream bug).
        // Wire conversion `wire = 31 - userDb` lives in P1CodecHl2.
        QCOMPARE(caps.attenuator.minDb, -28);
        QCOMPARE(caps.attenuator.maxDb, 31);
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
        // Signed user-facing range -28..+31 (issue #175 follow-up — capped
        // at +31 per maintainer approval).  The 6-bit wire field expresses
        // the span via `wire = 31 - userDb` clamped through the 0x3F mask.
        QCOMPARE(caps.attenuator.minDb, -28);
        QCOMPARE(caps.attenuator.maxDb, 31);
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

    // -----------------------------------------------------------------------
    // Phase 3P-F Task 2: accessory board capability gating
    // Source: setup.cs:19834-20310 RadioModelChanged() per-model if-ladder [@501e3f5]
    //         setup.cs:6338 AddHPSDRPages() for tpPennyCtrl / tpAlexControl visibility
    // Upstream inline attribution preserved verbatim:
    //   setup.cs:19855  if (initializing) return; // forceallevents will call this  // [2.10.1.0] MW0LGE renabled
    //   setup.cs:19904  case HPSDRModel.ANAN_G1: //N1GP G1 added
    //   setup.cs:20202  case HPSDRModel.ANAN_G2:                 // added G8NJJ
    //   setup.cs:20253  case HPSDRModel.ANAN_G2_1K:              // added G8NJJ
    // -----------------------------------------------------------------------

    // Only HPSDRModel.HERMES enables chkApolloPresent; all ANAN family boards
    // set chkApolloPresent.Enabled=false + Checked=false.
    void hermes_has_apollo() {
        const auto& caps = BoardCapsTable::forBoard(HPSDRHW::Hermes);
        QVERIFY(caps.hasApollo);
        QVERIFY(caps.hasAlex);
        QVERIFY(caps.hasPennyLane);
    }

    // HL2 has no Alex slot, no Penny/OC ext-ctrl, no Apollo port.
    void hl2_no_apollo_no_alex_no_penny() {
        const auto& caps = BoardCapsTable::forBoard(HPSDRHW::HermesLite);
        QVERIFY(!caps.hasApollo);
        QVERIFY(!caps.hasAlex);
        QVERIFY(!caps.hasPennyLane);
    }

    // ANAN-100D (Angelia): Apollo disabled (setup.cs:20009), Alex enabled (setup.cs:20007),
    // PennyLane/OC Control tab present (setup.cs:6364).
    void angelia_no_apollo_has_alex_has_penny() {
        const auto& caps = BoardCapsTable::forBoard(HPSDRHW::Angelia);
        QVERIFY(!caps.hasApollo);
        QVERIFY(caps.hasAlex);
        QVERIFY(caps.hasPennyLane);
    }

    // ANAN-G2 (Saturn): Apollo disabled (setup.cs:20203), Alex enabled (setup.cs:20201),
    // PennyLane/OC Control tab present for all HPSDR boards (setup.cs:6364).
    void saturn_no_apollo_has_alex_has_penny() {
        const auto& caps = BoardCapsTable::forBoard(HPSDRHW::Saturn);
        QVERIFY(!caps.hasApollo);
        QVERIFY(caps.hasAlex);
        QVERIFY(caps.hasPennyLane);
    }

    // -----------------------------------------------------------------------
    // Phase 3M-0 Task 1: isRxOnlySku + canDriveGanymede SKU capability flags
    //
    // isRxOnlySku: NereusSDR-original flag — Thetis treats RX-only purely as
    //   a user toggle (chkGeneralRXOnly, console.cs:15283-15307 [v2.10.3.13]);
    //   we add this so SKUs without TX drivers are hard-blocked regardless of
    //   user settings.
    // canDriveGanymede: Andromeda console family flag — captures the PA-trip
    //   capability per Andromeda.cs:914-920 [v2.10.3.13].
    // -----------------------------------------------------------------------

    // Hermes Lite 2 RX-only kits ship without a TX driver — isRxOnlySku must
    // be true for HermesLiteRxOnly.
    void rxOnlySku_HermesLiteRxOnlyVariants_returnsTrue() {
        const auto& caps = BoardCapsTable::forBoard(HPSDRHW::HermesLiteRxOnly);
        QVERIFY(caps.isRxOnlySku);
    }

    // Standard HL2 (TX-capable) must NOT be flagged as RX-only.
    void rxOnlySku_StandardHl2_returnsFalse() {
        const auto& caps = BoardCapsTable::forBoard(HPSDRHW::HermesLite);
        QVERIFY(!caps.isRxOnlySku);
    }

    // ANAN-G2 (Saturn) is a full TX-capable board — not RX-only.
    void rxOnlySku_AnanG2_returnsFalse() {
        const auto& caps = BoardCapsTable::forBoard(HPSDRHW::Saturn);
        QVERIFY(!caps.isRxOnlySku);
    }

    // Andromeda console family can drive the Ganymede 500W PA.
    void canDriveGanymede_AndromedaConsoleFamily_returnsTrue() {
        const auto& caps = BoardCapsTable::forBoard(HPSDRHW::Andromeda);
        QVERIFY(caps.canDriveGanymede);
    }

    // Standard ANAN-G2 (Saturn) does not drive the Ganymede PA.
    void canDriveGanymede_StandardAnan_returnsFalse() {
        const auto& caps = BoardCapsTable::forBoard(HPSDRHW::Saturn);
        QVERIFY(!caps.canDriveGanymede);
    }
};

QTEST_APPLESS_MAIN(TestBoardCapabilities)
#include "tst_board_capabilities.moc"
