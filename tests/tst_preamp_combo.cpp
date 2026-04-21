// no-port-check: test fixture asserts BoardCapabilities::preampItemsForBoard()
// matches Thetis SetComboPreampForHPSDR console.cs:40755-40825 [@501e3f5]
// per board, and that RxApplet.preampComboItemCountForTest() reflects those
// per-board counts at construction time. Phase 3P-C Step 2 + Step 3.

#include <QtTest/QtTest>
#include <QApplication>

#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "core/StepAttenuatorController.h"
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

    // HL2 is not in Thetis SetComboPreampForHPSDR (postdates the switch).
    // Per spec §8 and mi0bot HL2 LNA design [@c26a8a4]: anan100d 4-step set.
    // Phase 3P-C Step 2: corrected from 1-item "On only" to 4-item anan100d.
    void hl2_four_items()
    {
        auto items = BoardCapsTable::preampItemsForBoard(HPSDRHW::HermesLite, /*alexPresent=*/false);
        QCOMPARE(int(items.size()), 4);
        QCOMPARE(QLatin1String(items[0].label), QLatin1String("0dB"));
        QCOMPARE(QLatin1String(items[1].label), QLatin1String("-10dB"));
        QCOMPARE(QLatin1String(items[2].label), QLatin1String("-20dB"));
        QCOMPARE(QLatin1String(items[3].label), QLatin1String("-30dB"));
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
        QCOMPARE(items[1].modeInt, static_cast<int>(PreampMode::Minus10)); // "-10dB"
        QCOMPARE(items[2].modeInt, static_cast<int>(PreampMode::Minus20)); // "-20dB"
        QCOMPARE(items[3].modeInt, static_cast<int>(PreampMode::Minus30)); // "-30dB"
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
};

QTEST_MAIN(TestPreampCombo)
#include "tst_preamp_combo.moc"
