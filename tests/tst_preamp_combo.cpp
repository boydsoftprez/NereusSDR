// no-port-check: test fixture asserts BoardCapabilities::preampItemsForBoard()
// matches Thetis SetComboPreampForHPSDR console.cs:40755-40825 [@501e3f5]
// per board, and that RxApplet.preampComboItemCountForTest() reflects those
// per-board counts at construction time. Phase 3P-C Step 2 + Step 3.
// R-R3-46 / R-R3-21 (2026-09-23): in a remote window the combo shows and
// writes the Core's preamp mode (the `stepAtt` object).

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QStackedWidget>

#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "core/StepAttenuatorController.h"
#include "core/StepAttenuatorFacade.h"
#include "gui/applets/RxApplet.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestPreampCombo : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()
    {
        if (!qApp) {
            static int   argc = 0;
            static char* argv = nullptr;
            new QApplication(argc, &argv);
        }
    }

    // ─── BoardCapabilities::preampItemsForBoard() per-board assertions ──────

    // From Thetis console.cs:40756-40760 [@501e3f5] — HPSDR (Atlas) no ALEX:
    // comboPreamp.Items.AddRange(on_off_preamp_settings)  → 2 items only.
    void hpsdr_no_alex_two_items()
    {
        auto items = BoardCapsTable::preampItemsForBoard(HPSDRHW::Atlas, /*alexPresent=*/false);
        QCOMPARE(int(items.size()), 2);
        QCOMPARE(QLatin1String(items[0].label), QLatin1String("0dB"));
        QCOMPARE(QLatin1String(items[1].label), QLatin1String("-20dB"));
        QCOMPARE(items[0].modeInt, 1);  // PreampMode::On
        QCOMPARE(items[1].modeInt, 0);  // PreampMode::Off
    }

    // From Thetis console.cs:40760-40763 [@501e3f5] — HPSDR (Atlas) with ALEX:
    // on_off + alex_preamp → 2 + 5 = 7 items.
    void hpsdr_with_alex_seven_items()
    {
        auto items = BoardCapsTable::preampItemsForBoard(HPSDRHW::Atlas, /*alexPresent=*/true);
        QCOMPARE(int(items.size()), 7);
        QCOMPARE(QLatin1String(items[0].label), QLatin1String("0dB"));
        QCOMPARE(QLatin1String(items[1].label), QLatin1String("-20dB"));
        // ALEX extension (lowercase "db" per Thetis source comment "not a very nice implementation")
        QCOMPARE(QLatin1String(items[2].label), QLatin1String("-10db"));
        QCOMPARE(QLatin1String(items[6].label), QLatin1String("-50db"));
        QCOMPARE(items[6].modeInt, 6);  // PreampMode::Minus50
    }

    // From Thetis console.cs:40764-40767 [@501e3f5] — HERMES no ALEX:
    // comboPreamp.Items.AddRange(anan100d_preamp_settings) → 4 items.
    void hermes_no_alex_four_items()
    {
        auto items = BoardCapsTable::preampItemsForBoard(HPSDRHW::Hermes, /*alexPresent=*/false);
        QCOMPARE(int(items.size()), 4);
        QCOMPARE(QLatin1String(items[0].label), QLatin1String("0dB"));
        QCOMPARE(QLatin1String(items[1].label), QLatin1String("-10dB"));
        QCOMPARE(QLatin1String(items[2].label), QLatin1String("-20dB"));
        QCOMPARE(QLatin1String(items[3].label), QLatin1String("-30dB"));
    }

    // From Thetis console.cs:40764-40767 [@501e3f5] — HERMES with ALEX (ANAN-100):
    // on_off + alex → 7 items.
    void hermes_with_alex_seven_items()
    {
        auto items = BoardCapsTable::preampItemsForBoard(HPSDRHW::Hermes, /*alexPresent=*/true);
        QCOMPARE(int(items.size()), 7);
    }

    // From Thetis console.cs:40777-40780 [@501e3f5] — ANAN-7000D/8000D/OrionMKII/G2/G2-1K:
    // comboPreamp.Items.AddRange(anan100d_preamp_settings) → 4 items, always.
    // Maps to HPSDRHW::OrionMKII and HPSDRHW::Saturn.
    void orionmkii_four_items()
    {
        auto items = BoardCapsTable::preampItemsForBoard(HPSDRHW::OrionMKII, /*alexPresent=*/false);
        QCOMPARE(int(items.size()), 4);
    }

    void saturn_four_items()
    {
        auto items = BoardCapsTable::preampItemsForBoard(HPSDRHW::Saturn, /*alexPresent=*/false);
        QCOMPARE(int(items.size()), 4);
    }

    // HL2 shares HERMES's branch in mi0bot SetComboPreampForHPSDR
    // (mi0bot console.cs:41709-41718 [v2.10.3.13-beta2], "MI0BOT: HL2"):
    // without Alex, the anan100d 4-step set.
    void hl2_four_items()
    {
        auto items = BoardCapsTable::preampItemsForBoard(HPSDRHW::HermesLite, /*alexPresent=*/false);
        QCOMPARE(int(items.size()), 4);
        QCOMPARE(QLatin1String(items[0].label), QLatin1String("0dB"));
        QCOMPARE(QLatin1String(items[1].label), QLatin1String("-10dB"));
        QCOMPARE(QLatin1String(items[2].label), QLatin1String("-20dB"));
        QCOMPARE(QLatin1String(items[3].label), QLatin1String("-30dB"));
    }

    // HL2 with Alex: on/off plus the five Alex items, as HERMES.
    void hl2_with_alex_seven_items()
    {
        auto hl2 = BoardCapsTable::preampItemsForBoard(HPSDRHW::HermesLite, /*alexPresent=*/true);
        auto hermes = BoardCapsTable::preampItemsForBoard(HPSDRHW::Hermes, /*alexPresent=*/true);
        QCOMPARE(int(hl2.size()), 7);
        QCOMPARE(int(hl2.size()), int(hermes.size()));
        for (std::size_t i = 0; i < hl2.size(); ++i) {
            QCOMPARE(QLatin1String(hl2[i].label), QLatin1String(hermes[i].label));
            QCOMPARE(hl2[i].modeInt, hermes[i].modeInt);
        }
    }

    // Angelia (ANAN-100D) no ALEX → 4 items; with ALEX → 7 items.
    // From Thetis console.cs:40771-40776 [@501e3f5].
    void angelia_no_alex_four_items()
    {
        auto items = BoardCapsTable::preampItemsForBoard(HPSDRHW::Angelia, /*alexPresent=*/false);
        QCOMPARE(int(items.size()), 4);
    }

    void angelia_with_alex_seven_items()
    {
        auto items = BoardCapsTable::preampItemsForBoard(HPSDRHW::Angelia, /*alexPresent=*/true);
        QCOMPARE(int(items.size()), 7);
    }

    // HermesC10 (ANAN-G2E) → anan100d_preamp_settings (4 items), always.
    // From Thetis console.cs:40871-40879 [v2.10.3.15] //N1GP G2E added:
    //   case HPSDRModel.ANAN7000D:
    //   case HPSDRModel.ANAN8000D:
    //   case HPSDRModel.ORIONMKII:
    //   case HPSDRModel.ANAN_G2E: //N1GP G2E added
    //   case HPSDRModel.ANAN_G2:
    //   case HPSDRModel.ANAN_G2_1K:
    //   case HPSDRModel.ANVELINAPRO3:
    //       // case HPSDRModel.REDPITAYA: // DH1KLM: removed for compatibility reasons
    //       comboPreamp.Items.AddRange(anan100d_preamp_settings);
    // NereusSDR dispatches by HPSDRHW; HermesC10 is the board for ANAN_G2E.
    void hermes_c10_anan_g2e_four_items()
    {
        auto items = BoardCapsTable::preampItemsForBoard(HPSDRHW::HermesC10, /*alexPresent=*/false);
        QCOMPARE(int(items.size()), 4);
        QCOMPARE(QLatin1String(items[0].label), QLatin1String("0dB"));
        QCOMPARE(QLatin1String(items[1].label), QLatin1String("-10dB"));
        QCOMPARE(QLatin1String(items[2].label), QLatin1String("-20dB"));
        QCOMPARE(QLatin1String(items[3].label), QLatin1String("-30dB"));
    }

    // HermesC10 with alexPresent=true → still 4 items (ANAN_G2E always
    // uses anan100d; the alexPresent flag is irrelevant for this board family).
    void hermes_c10_anan_g2e_alex_still_four_items()
    {
        auto items = BoardCapsTable::preampItemsForBoard(HPSDRHW::HermesC10, /*alexPresent=*/true);
        QCOMPARE(int(items.size()), 4);
    }

    // ─── PreampMode enum has 7 distinct values ───────────────────────────────
    // From Thetis enums.cs:246 [@501e3f5] — PreampMode enum.
    void preamp_mode_enum_seven_values()
    {
        // Verify the 7 modes exist and have distinct integer values 0-6.
        QCOMPARE(static_cast<int>(PreampMode::Off),     0);
        QCOMPARE(static_cast<int>(PreampMode::On),      1);
        QCOMPARE(static_cast<int>(PreampMode::Minus10), 2);
        QCOMPARE(static_cast<int>(PreampMode::Minus20), 3);
        QCOMPARE(static_cast<int>(PreampMode::Minus30), 4);
        QCOMPARE(static_cast<int>(PreampMode::Minus40), 5);
        QCOMPARE(static_cast<int>(PreampMode::Minus50), 6);
    }

    // ─── modeInt in preampItems matches PreampMode values ───────────────────
    void anan100d_mode_ints_correct()
    {
        auto items = BoardCapsTable::preampItemsForBoard(HPSDRHW::Hermes, /*alexPresent=*/false);
        QCOMPARE(items[0].modeInt, static_cast<int>(PreampMode::On));      // "0dB"
        // SA step attenuator modes (console.cs:28401-28420 [v2.10.3.15]).
        QCOMPARE(items[1].modeInt, static_cast<int>(PreampMode::SaMinus10)); // "-10dB"
        QCOMPARE(items[2].modeInt, static_cast<int>(PreampMode::SaMinus20)); // "-20dB"
        QCOMPARE(items[3].modeInt, static_cast<int>(PreampMode::SaMinus30)); // "-30dB"
    }

    void alex_mode_ints_correct()
    {
        auto items = BoardCapsTable::preampItemsForBoard(HPSDRHW::Atlas, /*alexPresent=*/true);
        QCOMPARE(items[0].modeInt, static_cast<int>(PreampMode::On));      // "0dB"
        QCOMPARE(items[1].modeInt, static_cast<int>(PreampMode::Off));     // "-20dB"
        QCOMPARE(items[2].modeInt, static_cast<int>(PreampMode::Minus10)); // "-10db"
        QCOMPARE(items[6].modeInt, static_cast<int>(PreampMode::Minus50)); // "-50db"
    }

    // ─── RxApplet preamp combo populates per board at construction ───────────
    // Phase 3P-C Step 3: verifies populatePreampCombo() is called at init,
    // not hardcoded. Uses preampComboItemCountForTest() accessor.

    void rxapplet_hl2_combo_has_four_items()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        RxApplet applet(nullptr, &model);
        QCOMPARE(applet.preampComboItemCountForTest(), 4);
    }

    void rxapplet_hermes_with_alex_combo_has_seven_items()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Hermes);
        RxApplet applet(nullptr, &model);
        // Hermes board caps has hasAlexFilters=true (ANAN-100 uses Hermes board
        // and ships with Alex). So the combo gets 7 items (on_off+alex).
        QCOMPARE(applet.preampComboItemCountForTest(), 7);
    }

    void rxapplet_orionmkii_combo_has_four_items()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::OrionMKII);
        RxApplet applet(nullptr, &model);
        QCOMPARE(applet.preampComboItemCountForTest(), 4);
    }

    void rxapplet_remote_combo_shows_and_writes_the_cores_preamp()
    {
        RadioModel remote(RadioModel::Role::Remote);
        remote.setBoardForTest(HPSDRHW::Hermes);
        RxApplet applet(nullptr, &remote);
        auto* stack = applet.findChild<QStackedWidget*>(QStringLiteral("RxAttenuatorStack"));
        QVERIFY(stack != nullptr);
        auto* combo = stack->findChild<QComboBox*>();
        QVERIFY(combo != nullptr);
        QCOMPARE(combo->count(), 7);
        StepAttenuatorFacade* stepAtt = remote.stepAttFacade();
        stepAtt->setWindowAvailability(true, QString());
        QVERIFY(combo->isEnabled());

        // The Core's mode is shown...
        stepAtt->setPreampMode(combo->itemData(3).toInt());
        QCOMPARE(combo->currentIndex(), 3);
        // ...and the window's choice is written to the Core's object.
        combo->setCurrentIndex(5);
        QCOMPARE(stepAtt->preampMode(), combo->itemData(5).toInt());
    }

    // R-R3-46: a remote window is built before the Core's radio is known.
    // When the Core's board arrives (MainWindow hands currentRadioChanged
    // to setBoardCapabilities) and it is a dual-ADC board, the RX1 preamp
    // toggle is built then, follows the Core's `stepAtt` object and writes
    // to it; a single-ADC board hides it again.
    void rxapplet_remote_rx1_preamp_toggle_arrives_with_the_cores_board()
    {
        RadioModel remote(RadioModel::Role::Remote);
        RxApplet applet(nullptr, &remote);
        QVERIFY(!remote.boardCapabilities().p2PreampPerAdc);
        QVERIFY(applet.findChild<QCheckBox*>(QStringLiteral("RxRx1PreampToggle")) == nullptr);

        remote.setBoardForTest(HPSDRHW::OrionMKII);
        QVERIFY(remote.boardCapabilities().p2PreampPerAdc);
        applet.setBoardCapabilities(remote.boardCapabilities());
        auto* toggle = applet.findChild<QCheckBox*>(QStringLiteral("RxRx1PreampToggle"));
        QVERIFY(toggle != nullptr);
        QVERIFY(!toggle->isHidden());
        // Until the Core takes the window's edits it is disabled with the
        // object's plain reason, like the rest of the row.
        StepAttenuatorFacade* stepAtt = remote.stepAttFacade();
        QVERIFY(!toggle->isEnabled());
        QVERIFY(!toggle->toolTip().isEmpty());

        stepAtt->setWindowAvailability(true, QString());
        QVERIFY(toggle->isEnabled());
        stepAtt->setRx1Preamp(true);
        QVERIFY(toggle->isChecked());
        toggle->click();
        QVERIFY(!stepAtt->rx1Preamp());

        // A second board message builds nothing new.
        applet.setBoardCapabilities(remote.boardCapabilities());
        QCOMPARE(applet.findChildren<QCheckBox*>(QStringLiteral("RxRx1PreampToggle")).size(), 1);

        // A single-ADC board hides it.
        remote.setBoardForTest(HPSDRHW::Hermes);
        applet.setBoardCapabilities(remote.boardCapabilities());
        QVERIFY(toggle->isHidden());
    }
};

QTEST_MAIN(TestPreampCombo)
#include "tst_preamp_combo.moc"
