// =================================================================
// tests/tst_display_settings_golden.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure for the
// NereusSDR-original DisplaySettingsModel persistence migration (3D
// Stacked-Trace Spectrum Plan Task 23). See src/models/
// DisplaySettingsModel.h and src/gui/SpectrumWidget.cpp's
// loadSettings()/saveSettings() for the code this file pins.
//
// GOLDEN TEST: this file exists to catch a persistence FORMAT
// regression the moment SpectrumWidget::loadSettings()/saveSettings()
// stop reading and writing the fourteen per-pan display keys directly
// and start routing them through DisplaySettingsModel::load()/save()
// instead (Task 23). It is committed FIRST, alone, green against the
// pre-refactor code (Tasks 18-22's direct-read/direct-write shape), so
// the refactor commit that follows can be judged against it: if this
// file still passes UNCHANGED after the refactor, the on-disk format
// -- key names, per-pan "_<N>" suffixing, "True"/"False" booleans, and
// the exact numeric string every float round-trips to -- has not moved
// by a single byte.
//
// Every expected string below was read off a real saveSettings() run
// (a QCOMPARE mismatch's "Actual"/"Expected" output on the pre-refactor
// code), not computed by reasoning about QString::number()'s formatting
// rules -- see the task report for the read-don't-reason note this
// follows.
//
// Isolation: AppSettings::instance() is a process-wide in-memory
// singleton with no automatic load()/save() in this test process
// (TestSandboxInit.cpp also sandboxes QStandardPaths so nothing here
// can reach the developer's real ~/.config/NereusSDR/NereusSDR.settings).
// init()/cleanup() clear it before and after every slot, matching
// tst_dss_persistence.cpp and tst_display_settings_model.cpp, so no
// test's writes can leak into another's.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "gui/SpectrumWidget.h"
#include "models/DisplaySettingsModel.h"

using namespace NereusSDR;

class TestDisplaySettingsGolden : public QObject
{
    Q_OBJECT

private slots:
    void init()    { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ============================================================
    // Task 23 Acceptance, verbatim: "It sets all fourteen values to
    // non-defaults through the widget's setters, calls saveSettings(),
    // and compares the stored string of every key (the fourteen exact
    // key names from the scout table, pan 0) with literal expected
    // strings; then it sets panIndex 2 with no own keys and proves
    // loadSettings() inherits pan 0's values; then it saves on pan 2
    // and proves the _2 suffixed keys hold pan 2's strings while pan
    // 0's are untouched."
    //
    // Catches: any key renamed, dropped, or moved off the per-pan
    // settingsKey()/settingsKeyFor() convention; a float formatted
    // differently (e.g. "-30" silently becoming "-30.0" or "-30.000000")
    // by whichever object ends up writing it; Dyn Range's derived-key
    // shape (DisplayGridMin = refLevel - dynamicRange, no key of its
    // own) breaking; a boolean written as "1"/"0" instead of the
    // project's "True"/"False"; pan-0-fallback inheritance dropped; or
    // pan 2's save clobbering pan 0's keys instead of writing its own
    // "_2" suffixed ones.
    // ============================================================
    void fourteenKeys_byteForByte_pan0ThenPan2InheritsThenOwnSave()
    {
        // ---- Phase 1: pan 0, all fourteen to non-defaults, byte for byte ----
        SpectrumWidget w0;
        w0.setPanIndex(0);

        w0.setWfColorScheme(WfColorScheme::Spectran);    // 2, default Default(0)
        w0.setWfColorGain(62);                           // default 45
        w0.setWfBlackLevel(77);                          // default 104
        w0.setRefLevel(-30.0f);                          // default -48.0
        w0.setDynamicRange(90.0f);                       // default 68 (-> GridMin -120.0)
        w0.setFillAlpha(0.35f);                          // default 0.70
        w0.setPanFillEnabled(false);                     // default true
        w0.setSpectrumFrac(0.62f);                       // default 0.40
        w0.setSpectrumRenderMode(                        // default Mode2D(0)
            static_cast<int>(SpectrumRenderMode::Mode3D));
        w0.setDssGain(55);                               // default 70
        w0.setDssRowSpan(65);                            // default 100
        w0.setDssAngle(82);                              // default 50
        w0.setDssRowDivider(7);                          // default 0 (Match)
        w0.setThreeDSliceDepth(true);                    // default false

        w0.saveSettings();

        auto& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("DisplayWfColorScheme")).toString(),
                 QStringLiteral("2"));
        QCOMPARE(s.value(QStringLiteral("DisplayWfColorGain")).toString(),
                 QStringLiteral("62"));
        QCOMPARE(s.value(QStringLiteral("DisplayWfBlackLevel")).toString(),
                 QStringLiteral("77"));
        QCOMPARE(s.value(QStringLiteral("DisplayGridMax")).toString(),
                 QStringLiteral("-30"));
        QCOMPARE(s.value(QStringLiteral("DisplayGridMin")).toString(),
                 QStringLiteral("-120"));
        QCOMPARE(s.value(QStringLiteral("DisplayFftFillAlpha")).toString(),
                 QStringLiteral("0.35"));
        QCOMPARE(s.value(QStringLiteral("DisplayPanFill")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(QStringLiteral("DisplaySpectrumFrac")).toString(),
                 QStringLiteral("0.62"));
        QCOMPARE(s.value(QStringLiteral("DisplaySpectrumRenderMode")).toString(),
                 QStringLiteral("1"));
        QCOMPARE(s.value(QStringLiteral("Display3DGain")).toString(),
                 QStringLiteral("55"));
        QCOMPARE(s.value(QStringLiteral("Display3DSpan")).toString(),
                 QStringLiteral("65"));
        QCOMPARE(s.value(QStringLiteral("Display3DAngle")).toString(),
                 QStringLiteral("82"));
        QCOMPARE(s.value(QStringLiteral("Display3DSpeed")).toString(),
                 QStringLiteral("7"));
        QCOMPARE(s.value(QStringLiteral("Display3DSliceShadow")).toString(),
                 QStringLiteral("True"));

        // ---- Phase 2: an untouched pan 2 inherits pan 0's values ----
        SpectrumWidget w2;
        w2.setPanIndex(2);
        w2.loadSettings();

        QCOMPARE(static_cast<int>(w2.wfColorScheme()), 2);
        QCOMPARE(w2.wfColorGain(), 62);
        QCOMPARE(w2.wfBlackLevel(), 77);
        QVERIFY(qFuzzyCompare(w2.refLevel() + 1.0f, -30.0f + 1.0f));
        QVERIFY(qFuzzyCompare(w2.dynamicRange() + 1.0f, 90.0f + 1.0f));
        QVERIFY(qFuzzyCompare(w2.fillAlpha() + 1.0f, 0.35f + 1.0f));
        QCOMPARE(w2.panFillEnabled(), false);
        QVERIFY(qFuzzyCompare(w2.spectrumFrac() + 1.0f, 0.62f + 1.0f));
        QCOMPARE(w2.spectrumRenderMode(), static_cast<int>(SpectrumRenderMode::Mode3D));
        QCOMPARE(w2.dssGain(), 55);
        QCOMPARE(w2.dssRowSpan(), 65);
        QCOMPARE(w2.dssAngle(), 82);
        QCOMPARE(w2.dssRowDivider(), 7);
        QCOMPARE(w2.threeDSliceDepth(), true);

        // ---- Phase 3: pan 2 saves its OWN, different, non-default values ----
        w2.setWfColorScheme(WfColorScheme::LinRad);      // 5
        w2.setWfColorGain(20);
        w2.setWfBlackLevel(15);
        w2.setRefLevel(-60.0f);
        w2.setDynamicRange(40.0f);                        // -> GridMin_2 -100.0
        w2.setFillAlpha(0.85f);
        w2.setPanFillEnabled(true);
        w2.setSpectrumFrac(0.18f);
        w2.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode2D));
        w2.setDssGain(88);
        w2.setDssRowSpan(30);
        w2.setDssAngle(19);
        w2.setDssRowDivider(3);
        w2.setThreeDSliceDepth(false);

        w2.saveSettings();

        QCOMPARE(s.value(QStringLiteral("DisplayWfColorScheme_2")).toString(),
                 QStringLiteral("5"));
        QCOMPARE(s.value(QStringLiteral("DisplayWfColorGain_2")).toString(),
                 QStringLiteral("20"));
        QCOMPARE(s.value(QStringLiteral("DisplayWfBlackLevel_2")).toString(),
                 QStringLiteral("15"));
        QCOMPARE(s.value(QStringLiteral("DisplayGridMax_2")).toString(),
                 QStringLiteral("-60"));
        QCOMPARE(s.value(QStringLiteral("DisplayGridMin_2")).toString(),
                 QStringLiteral("-100"));
        QCOMPARE(s.value(QStringLiteral("DisplayFftFillAlpha_2")).toString(),
                 QStringLiteral("0.85"));
        QCOMPARE(s.value(QStringLiteral("DisplayPanFill_2")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(s.value(QStringLiteral("DisplaySpectrumFrac_2")).toString(),
                 QStringLiteral("0.18"));
        QCOMPARE(s.value(QStringLiteral("DisplaySpectrumRenderMode_2")).toString(),
                 QStringLiteral("0"));
        QCOMPARE(s.value(QStringLiteral("Display3DGain_2")).toString(),
                 QStringLiteral("88"));
        QCOMPARE(s.value(QStringLiteral("Display3DSpan_2")).toString(),
                 QStringLiteral("30"));
        QCOMPARE(s.value(QStringLiteral("Display3DAngle_2")).toString(),
                 QStringLiteral("19"));
        QCOMPARE(s.value(QStringLiteral("Display3DSpeed_2")).toString(),
                 QStringLiteral("3"));
        QCOMPARE(s.value(QStringLiteral("Display3DSliceShadow_2")).toString(),
                 QStringLiteral("False"));

        // pan 0's own keys must be untouched by pan 2's save.
        QCOMPARE(s.value(QStringLiteral("DisplayWfColorScheme")).toString(),
                 QStringLiteral("2"));
        QCOMPARE(s.value(QStringLiteral("DisplayWfColorGain")).toString(),
                 QStringLiteral("62"));
        QCOMPARE(s.value(QStringLiteral("DisplayWfBlackLevel")).toString(),
                 QStringLiteral("77"));
        QCOMPARE(s.value(QStringLiteral("DisplayGridMax")).toString(),
                 QStringLiteral("-30"));
        QCOMPARE(s.value(QStringLiteral("DisplayGridMin")).toString(),
                 QStringLiteral("-120"));
        QCOMPARE(s.value(QStringLiteral("DisplayFftFillAlpha")).toString(),
                 QStringLiteral("0.35"));
        QCOMPARE(s.value(QStringLiteral("DisplayPanFill")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(s.value(QStringLiteral("DisplaySpectrumFrac")).toString(),
                 QStringLiteral("0.62"));
        QCOMPARE(s.value(QStringLiteral("DisplaySpectrumRenderMode")).toString(),
                 QStringLiteral("1"));
        QCOMPARE(s.value(QStringLiteral("Display3DGain")).toString(),
                 QStringLiteral("55"));
        QCOMPARE(s.value(QStringLiteral("Display3DSpan")).toString(),
                 QStringLiteral("65"));
        QCOMPARE(s.value(QStringLiteral("Display3DAngle")).toString(),
                 QStringLiteral("82"));
        QCOMPARE(s.value(QStringLiteral("Display3DSpeed")).toString(),
                 QStringLiteral("7"));
        QCOMPARE(s.value(QStringLiteral("Display3DSliceShadow")).toString(),
                 QStringLiteral("True"));
    }

    // ============================================================
    // Task 23 Acceptance, verbatim: "loadSettings() schedules no save
    // and performs no apply (displaySettingsApplyCountForTest() stays
    // 0), and the model holds the loaded values."
    //
    // Seeded with genuinely NON-default values -- unlike
    // tst_display_settings_binding.cpp's noApplyDuringLoad, which
    // starts from an empty AppSettings and so never actually exercises
    // a value CHANGE during load. With real seeded values,
    // DisplaySettingsModel::load()'s internal setters genuinely differ
    // from the model's just-constructed ship defaults; if this widget's
    // model were not signal-blocked while loading, those changes would,
    // through bindDisplaySettings()'s model-to-widget connections, fire
    // straight back into this widget's own appliers (setRefLevel(),
    // setWfColorGain(), ...), each of which calls update(),
    // scheduleSettingsSave() and increments m_displaySettingsApplyCount.
    //
    // Catches: loadSettings()'s model-load path routed through the
    // widget's OWN setters (directly, or indirectly via an unblocked
    // model-to-widget signal) instead of a direct member copy.
    // ============================================================
    void loadSettings_seedsTheModelWithoutApplyingOrScheduling()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("DisplayWfColorScheme"), QStringLiteral("2"));
        s.setValue(QStringLiteral("DisplayWfColorGain"), QStringLiteral("62"));
        s.setValue(QStringLiteral("DisplayWfBlackLevel"), QStringLiteral("77"));
        s.setValue(QStringLiteral("DisplayGridMax"), QStringLiteral("-30"));
        s.setValue(QStringLiteral("DisplayGridMin"), QStringLiteral("-120"));
        s.setValue(QStringLiteral("DisplayFftFillAlpha"), QStringLiteral("0.35"));
        s.setValue(QStringLiteral("DisplayPanFill"), QStringLiteral("False"));
        s.setValue(QStringLiteral("DisplaySpectrumFrac"), QStringLiteral("0.62"));
        s.setValue(QStringLiteral("DisplaySpectrumRenderMode"), QStringLiteral("1"));
        s.setValue(QStringLiteral("Display3DGain"), QStringLiteral("55"));
        s.setValue(QStringLiteral("Display3DSpan"), QStringLiteral("65"));
        s.setValue(QStringLiteral("Display3DAngle"), QStringLiteral("82"));
        s.setValue(QStringLiteral("Display3DSpeed"), QStringLiteral("7"));
        s.setValue(QStringLiteral("Display3DSliceShadow"), QStringLiteral("True"));

        SpectrumWidget w;
        w.setPanIndex(0);
        w.loadSettings();

        QCOMPARE(w.displaySettingsApplyCountForTest(), 0);

        DisplaySettingsModel* m = w.displaySettings();
        QCOMPARE(m->wfColorScheme(), 2);
        QCOMPARE(m->wfColorGain(), 62);
        QCOMPARE(m->wfBlackLevel(), 77);
        QVERIFY(qFuzzyCompare(m->refLevel() + 1.0f, -30.0f + 1.0f));
        QVERIFY(qFuzzyCompare(m->dynamicRange() + 1.0f, 90.0f + 1.0f));
        QVERIFY(qFuzzyCompare(m->fillAlpha() + 1.0f, 0.35f + 1.0f));
        QCOMPARE(m->panFill(), false);
        QVERIFY(qFuzzyCompare(m->spectrumFrac() + 1.0f, 0.62f + 1.0f));
        QCOMPARE(m->spectrumRenderMode(), 1);
        QCOMPARE(m->dssGain(), 55);
        QCOMPARE(m->dssRowSpan(), 65);
        QCOMPARE(m->dssAngle(), 82);
        QCOMPARE(m->dssRowDivider(), 7);
        QCOMPARE(m->threeDSliceDepth(), true);
    }
};

QTEST_MAIN(TestDisplaySettingsGolden)
#include "tst_display_settings_golden.moc"
