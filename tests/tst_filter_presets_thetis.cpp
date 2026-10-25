// no-port-check: NereusSDR-original test. The expected values are Thetis
//   InitFilterPresets data, cited per mode below; the ported logic and its
//   attribution live in src/models/SliceModel.cpp.
// =================================================================
// tests/tst_filter_presets_thetis.cpp  (NereusSDR)
// =================================================================
//
// Table-driven check of SliceModel::presetsForMode and
// SliceModel::defaultFilterForMode against Thetis InitFilterPresets
// (console.cs:5188-5582 [v2.10.3.15]). The expected edges are written out
// as plain numbers at the Thetis start-up values (cw_pitch 600,
// digu_click_tune_offset 1500, digl_click_tune_offset 2210), so a formula
// slip in the port cannot hide behind the same formula in the test.
//
// Also checks that the CW slots follow the CW pitch the way the Thetis
// CWPitch setter moves them (console.cs:18212-18266 [v2.10.3.15]): centered
// on the pitch, sliding at the image limit instead of crossing zero.
//
// Longpath PRs 139 and 140 (oe5sos/Longpath, d5eb90b4) found the same
// table drift; their values were used as a cross-check only.
//
// DIGU and DIGL are the exception to the F5 default: NereusSDR starts them
// on F1 3.0k (JJ's ruling, 2026-10-24, for FT8), where Thetis starts them
// on F5 1.0k. Saved presets the user edited stay as saved; slots the user
// never edited, and Reset, give the Thetis values.
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22 - Created by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Claude Code.
//   2026-10-24 - DIGU/DIGL default on F1 3.0k, and saved presets kept
//                as saved, by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "models/FilterPresetStore.h"
#include "models/SliceModel.h"

#include <utility>
#include <vector>

using namespace NereusSDR;

namespace {

using Edges = std::pair<int, int>;

struct ModeTable {
    DSPMode            mode;
    const char*        name;
    std::vector<Edges> edges;   // F1..F10
};

// Thetis InitFilterPresets at its start-up values, F1..F10 per mode.
std::vector<ModeTable> thetisTables()
{
    return {
        // From Thetis console.cs:5199-5240 [v2.10.3.15]
        {DSPMode::LSB, "LSB",
         {{-5100, -100}, {-4500, -100}, {-3900, -100}, {-3400, -100}, {-3000, -100},
          {-2800, -100}, {-2500, -100}, {-2200, -100}, {-1900, -100}, {-1100, -100}}},
        // From Thetis console.cs:5241-5282 [v2.10.3.15]
        {DSPMode::USB, "USB",
         {{100, 5100}, {100, 4500}, {100, 3900}, {100, 3400}, {100, 3000},
          {100, 2800}, {100, 2500}, {100, 2200}, {100, 1900}, {100, 1100}}},
        // From Thetis console.cs:5283-5324 [v2.10.3.15], digl_click_tune_offset 2210
        {DSPMode::DIGL, "DIGL",
         {{-3710, -710}, {-3460, -960}, {-3210, -1210}, {-2960, -1460}, {-2710, -1710},
          {-2610, -1810}, {-2510, -1910}, {-2360, -2060}, {-2285, -2135}, {-2248, -2172}}},
        // From Thetis console.cs:5325-5366 [v2.10.3.15], digu_click_tune_offset 1500
        {DSPMode::DIGU, "DIGU",
         {{0, 3000}, {250, 2750}, {500, 2500}, {750, 2250}, {1000, 2000},
          {1100, 1900}, {1200, 1800}, {1350, 1650}, {1425, 1575}, {1462, 1538}}},
        // From Thetis console.cs:5367-5408 [v2.10.3.15], cw_pitch 600
        {DSPMode::CWL, "CWL",
         {{-1100, -100}, {-1000, -200}, {-900, -300}, {-850, -350}, {-800, -400},
          {-725, -475}, {-675, -525}, {-650, -550}, {-625, -575}, {-613, -587}}},
        // From Thetis console.cs:5409-5450 [v2.10.3.15], cw_pitch 600
        {DSPMode::CWU, "CWU",
         {{100, 1100}, {200, 1000}, {300, 900}, {350, 850}, {400, 800},
          {475, 725}, {525, 675}, {550, 650}, {575, 625}, {587, 613}}},
        // From Thetis console.cs:5451-5492 [v2.10.3.15]
        {DSPMode::AM, "AM",
         {{-10000, 10000}, {-9000, 9000}, {-8000, 8000}, {-6000, 6000}, {-5000, 5000},
          {-4500, 4500}, {-4000, 4000}, {-3500, 3500}, {-3000, 3000}, {-2500, 2500}}},
        // From Thetis console.cs:5493-5534 [v2.10.3.15]
        {DSPMode::SAM, "SAM",
         {{-10000, 10000}, {-9000, 9000}, {-8000, 8000}, {-6000, 6000}, {-5000, 5000},
          {-4500, 4500}, {-4000, 4000}, {-3500, 3500}, {-3000, 3000}, {-2500, 2500}}},
        // From Thetis console.cs:5535-5576 [v2.10.3.15]
        {DSPMode::DSB, "DSB",
         {{-8000, 8000}, {-6000, 6000}, {-5000, 5000}, {-4000, 4000}, {-3300, 3300},
          {-2600, 2600}, {-2000, 2000}, {-1550, 1550}, {-1450, 1450}, {-1200, 1200}}},
    };
}

QString describe(const ModeTable& t, int slot, const Edges& got, const Edges& want)
{
    return QStringLiteral("%1 F%2: got %3..%4, Thetis %5..%6")
        .arg(QLatin1String(t.name)).arg(slot + 1)
        .arg(got.first).arg(got.second)
        .arg(want.first).arg(want.second);
}

} // namespace

class TstFilterPresetsThetis : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void init()         { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // Every slot of every Thetis mode, at the Thetis start-up values.
    void presetTablesMatchThetis()
    {
        int mismatches = 0;
        for (const ModeTable& t : thetisTables()) {
            const QList<Edges> got = SliceModel::presetsForMode(t.mode);
            if (got.size() != static_cast<int>(t.edges.size())) {
                qWarning("%s has %lld slots, Thetis has %zu", t.name,
                         static_cast<long long>(got.size()), t.edges.size());
                ++mismatches;
                continue;
            }
            for (int i = 0; i < got.size(); ++i) {
                const Edges want = t.edges[static_cast<size_t>(i)];
                if (got[i] != want) {
                    qWarning("%s", qPrintable(describe(t, i, got[i], want)));
                    ++mismatches;
                }
            }
        }
        QCOMPARE(mismatches, 0);
    }

    // Thetis starts every mode on F5 (preset[m].LastFilter = Filter.F5);
    // defaultFilterForMode is that slot, except DIGU and DIGL (below).
    void defaultFilterIsThetisF5()
    {
        int mismatches = 0;
        for (const ModeTable& t : thetisTables()) {
            if (t.mode == DSPMode::DIGU || t.mode == DSPMode::DIGL) {
                continue;
            }
            const Edges got  = SliceModel::defaultFilterForMode(t.mode);
            const Edges want = t.edges[4];
            if (got != want) {
                qWarning("%s", qPrintable(describe(t, 4, got, want)));
                ++mismatches;
            }
        }
        QCOMPARE(mismatches, 0);
    }

    // NereusSDR's own default: DIGU and DIGL start on F1 3.0k for FT8, not
    // the Thetis F5 1.0k. DIGU is 0..3000, DIGL -3710..-710 at offset 2210.
    void digDefaultIsF1ThreeKilohertz()
    {
        QCOMPARE(SliceModel::defaultFilterForMode(DSPMode::DIGU), Edges(0, 3000));
        QCOMPARE(SliceModel::defaultFilterForMode(DSPMode::DIGL), Edges(-3710, -710));
        QCOMPARE(SliceModel::defaultFilterForMode(DSPMode::DIGU),
                 SliceModel::presetsForMode(DSPMode::DIGU).at(0));
        QCOMPARE(SliceModel::defaultFilterForMode(DSPMode::DIGL),
                 SliceModel::presetsForMode(DSPMode::DIGL).at(0));
    }

    // Presets the user edited and saved stay exactly as saved: nothing
    // migrates them to the Thetis values. A slot the user never edited
    // gives the Thetis value, and Reset on an edited slot gives it too.
    void savedPresetsStayAsSaved()
    {
        FilterPresetStore store;
        FilterPreset edited;
        edited.name = QStringLiteral("Mine");
        edited.low  = -300;
        edited.high = 900;
        store.setPreset(DSPMode::CWU, 0, edited);
        // DIGU F5 saved at the old NereusSDR default, 900..2100.
        FilterPreset oldDig;
        oldDig.name = QStringLiteral("F5");
        oldDig.low  = 900;
        oldDig.high = 2100;
        store.setPreset(DSPMode::DIGU, 4, oldDig);

        // A fresh store reads the saved values back unchanged.
        FilterPresetStore reloaded;
        const QList<FilterPreset> cwu = reloaded.presetsForMode(DSPMode::CWU);
        QCOMPARE(cwu.size(), 10);
        QCOMPARE(cwu[0].name, QStringLiteral("Mine"));
        QCOMPARE(cwu[0].low, -300);
        QCOMPARE(cwu[0].high, 900);
        const QList<FilterPreset> digu = reloaded.presetsForMode(DSPMode::DIGU);
        QCOMPARE(digu[4].low, 900);
        QCOMPARE(digu[4].high, 2100);

        // An unedited slot of the same mode: Thetis CWU F2 at pitch 600.
        QCOMPARE(cwu[1].low, 200);
        QCOMPARE(cwu[1].high, 1000);

        // Reset on the edited slot: Thetis CWU F1, 100..1100.
        reloaded.resetPreset(DSPMode::CWU, 0);
        const FilterPreset reset = reloaded.presetsForMode(DSPMode::CWU).at(0);
        QCOMPARE(reset.name, QStringLiteral("F1"));
        QCOMPARE(reset.low, 100);
        QCOMPARE(reset.high, 1100);
        QCOMPARE(FilterPresetStore::defaultPreset(DSPMode::CWU, 0).low, 100);
        QCOMPARE(FilterPresetStore::defaultPreset(DSPMode::CWU, 0).high, 1100);
        // The other saved slot is untouched by that Reset.
        QCOMPARE(reloaded.presetsForMode(DSPMode::DIGU).at(4).low, 900);
        QCOMPARE(reloaded.presetsForMode(DSPMode::DIGU).at(4).high, 2100);
    }

    // CW slots sit on the CW pitch: at 700 Hz CWU F1 is 200..1200 and CWL
    // F10 is -713..-687, as the Thetis CWPitch setter recenters them.
    void cwSlotsFollowThePitch()
    {
        AppSettings::instance().setValue(QStringLiteral("CWPitch"), 700);
        const QList<Edges> cwu = SliceModel::presetsForMode(DSPMode::CWU);
        const QList<Edges> cwl = SliceModel::presetsForMode(DSPMode::CWL);
        QCOMPARE(cwu.size(), 10);
        QCOMPARE(cwl.size(), 10);
        QCOMPARE(cwu[0], Edges(200, 1200));
        QCOMPARE(cwu[9], Edges(687, 713));
        QCOMPARE(cwl[0], Edges(-1200, -200));
        QCOMPARE(cwl[9], Edges(-713, -687));
        QCOMPARE(SliceModel::defaultFilterForMode(DSPMode::CWU), Edges(500, 900));
        QCOMPARE(SliceModel::defaultFilterForMode(DSPMode::CWL), Edges(-900, -500));
    }

    // Below half a slot's width the Thetis setter stops at the image limit
    // and slides the passband instead of crossing zero: at 300 Hz CWU F1
    // (1000 Hz wide) is 0..1000, CWL F1 is -1000..0; narrow slots stay
    // centered.
    void cwSlotsSlideAtTheImageLimit()
    {
        AppSettings::instance().setValue(QStringLiteral("CWPitch"), 300);
        const QList<Edges> cwu = SliceModel::presetsForMode(DSPMode::CWU);
        const QList<Edges> cwl = SliceModel::presetsForMode(DSPMode::CWL);
        QCOMPARE(cwu.size(), 10);
        QCOMPARE(cwl.size(), 10);
        QCOMPARE(cwu[0], Edges(0, 1000));
        QCOMPARE(cwu[1], Edges(0, 800));
        QCOMPARE(cwu[2], Edges(0, 600));
        QCOMPARE(cwu[3], Edges(50, 550));
        QCOMPARE(cwl[0], Edges(-1000, 0));
        QCOMPARE(cwl[1], Edges(-800, 0));
        QCOMPARE(cwl[2], Edges(-600, 0));
        QCOMPARE(cwl[3], Edges(-550, -50));
        for (const Edges& e : cwu) { QVERIFY(e.first >= 0); }
        for (const Edges& e : cwl) { QVERIFY(e.second <= 0); }
    }
};

QTEST_GUILESS_MAIN(TstFilterPresetsThetis)
#include "tst_filter_presets_thetis.moc"
