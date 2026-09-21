// =================================================================
// tests/tst_display_settings_model.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure for the
// NereusSDR-original DisplaySettingsModel (3D Stacked-Trace Spectrum
// Plan Task 17). See src/models/DisplaySettingsModel.h for the design
// note this file pins.
//
// Isolation: AppSettings::instance() is a process-wide in-memory
// singleton with no automatic load()/save() in this test process
// (TestSandboxInit.cpp also sandboxes QStandardPaths so nothing here
// can reach the developer's real ~/.config/NereusSDR/NereusSDR.settings).
// init()/cleanup() clear it before and after every slot, matching the
// established pattern in tst_dss_persistence.cpp, so no test's writes
// can leak into another's fresh-model round trip.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "models/DisplaySettingsModel.h"

using namespace NereusSDR;

class TestDisplaySettingsModel : public QObject
{
    Q_OBJECT

private slots:
    void init()    { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ============================================================
    // Round trip: fresh instance, so a load() that silently does
    // nothing (leaving fresh-object ship defaults) cannot pass.
    // ============================================================

    // Catches: any of the thirteen per-pan fields missing from load()
    // or save(); a wrong key name; a swapped default. Two DISTINCT
    // instances (w writes, r reads) so the read can only succeed if
    // load() genuinely pulled the value back out of AppSettings -- a
    // fresh r starts from ship defaults, not from w's in-memory state.
    void allThirteenPerPanFields_roundTripThroughSaveAndLoad()
    {
        DisplaySettingsModel w;
        w.setPanIndex(0);
        w.setWfColorScheme(6);       // LinRad
        w.setWfColorGain(81);
        w.setWfBlackLevel(37);
        w.setRefLevel(-12.5f);
        w.setDynamicRange(96.0f);
        w.setFillAlpha(0.23f);
        w.setPanFill(false);
        w.setSpectrumFrac(0.61f);
        w.setSpectrumRenderMode(1);  // Mode3D
        w.setDssGain(19);
        w.setDssRowSpan(64);
        w.setDssAngle(88);
        w.setThreeDSliceDepth(true);
        w.save();

        DisplaySettingsModel r;
        r.setPanIndex(0);
        r.load();
        QCOMPARE(r.wfColorScheme(), 6);
        QCOMPARE(r.wfColorGain(), 81);
        QCOMPARE(r.wfBlackLevel(), 37);
        QVERIFY(qFuzzyCompare(r.refLevel() + 1.0f, -12.5f + 1.0f));
        QVERIFY(qFuzzyCompare(r.dynamicRange(), 96.0f));
        QVERIFY(qFuzzyCompare(r.fillAlpha() + 1.0f, 0.23f + 1.0f));
        QCOMPARE(r.panFill(), false);
        QVERIFY(qFuzzyCompare(r.spectrumFrac(), 0.61f));
        QCOMPARE(r.spectrumRenderMode(), 1);
        QCOMPARE(r.dssGain(), 19);
        QCOMPARE(r.dssRowSpan(), 64);
        QCOMPARE(r.dssAngle(), 88);
        QCOMPARE(r.threeDSliceDepth(), true);
    }

    // Catches: save() never actually reaching AppSettings (in-memory
    // only) -- distinct from the test above because it inspects the
    // raw store directly rather than round-tripping through a second
    // object's getters, so a setter/getter pair that agreed with each
    // other but never touched AppSettings would still be caught.
    void save_actuallyWritesAppSettings_notJustTheInMemoryCache()
    {
        DisplaySettingsModel w;
        w.setPanIndex(0);
        w.setDssGain(55);
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("Display3DGain")));
        w.save();
        QVERIFY(AppSettings::instance().contains(QStringLiteral("Display3DGain")));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Display3DGain")).toInt(), 55);
    }

    // Catches: a static/shared field instead of a genuine per-instance
    // member -- two different model instances set to two different
    // values must read back independently. Task 18 flattened this field
    // from Band-keyed AppSettings storage onto a plain in-memory member,
    // the same per-instance shape the other thirteen fields already have
    // (see the class-header comment).
    void dssFloorDepth_isPerInstanceNotShared()
    {
        DisplaySettingsModel a;
        DisplaySettingsModel b;
        a.setDssFloorDepth(4);
        b.setDssFloorDepth(18);
        QCOMPARE(a.dssFloorDepth(), 4);
        QCOMPARE(b.dssFloorDepth(), 18);
    }

    // Catches: a fresh instance returning something other than the
    // documented ship default (6 -- matches SpectrumWidget's
    // m_dssFloorDepth in-class initializer and PanadapterModel's
    // BandGridSettings::dss3DFloorDepth default).
    void dssFloorDepth_defaultsToShipDefaultOfSix()
    {
        DisplaySettingsModel m;
        QCOMPARE(m.dssFloorDepth(), 6);
    }

    // ============================================================
    // Booleans persist as the literal strings "True"/"False".
    // Reads the RAW stored string directly -- round-tripping through
    // the bool getter alone cannot distinguish "wrote the wrong
    // string, read back false" from "wrote nothing, defaulted false".
    // ============================================================

    // Catches: booleans written via QString::number(bool) ("1"/"0") or
    // native lowercase ("true"/"false") instead of the project's
    // mandated "True"/"False" strings (CLAUDE.md persistence rule).
    void booleansPersistAsTrueFalseStrings()
    {
        DisplaySettingsModel w;
        w.setPanIndex(0);
        w.setPanFill(true);
        w.setThreeDSliceDepth(true);
        w.save();
        QCOMPARE(AppSettings::instance().value(QStringLiteral("DisplayPanFill")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Display3DSliceShadow")).toString(),
                 QStringLiteral("True"));
    }

    // Catches: an implementation that only ever writes "True" regardless
    // of the actual value (the previous test alone cannot tell "always
    // writes True" apart from "writes correctly", since both pass when
    // the field happens to be true).
    void booleansPersistFalseAsTheLiteralFalseString()
    {
        DisplaySettingsModel w;
        w.setPanIndex(0);
        w.setPanFill(false);         // ship default is true -- a real change
        w.setThreeDSliceDepth(false); // ship default already false; still assert the string
        w.save();
        QCOMPARE(AppSettings::instance().value(QStringLiteral("DisplayPanFill")).toString(),
                 QStringLiteral("False"));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Display3DSliceShadow")).toString(),
                 QStringLiteral("False"));
    }

    // ============================================================
    // Signal-once: exactly one emission per real change, never zero,
    // never a cascade. Mirrors tst_dss_overlay_menu.cpp's
    // movingASlider_emitsItsSignal shape (one QSignalSpy per signal,
    // one edit each, assert count 1).
    // ============================================================

    // Catches: a setter that forgot to emit (spy count 0) or that
    // somehow emits twice for one change (e.g. a copy-paste bug that
    // calls the signal both directly and via a second code path).
    void everyChangedSignal_firesExactlyOnce()
    {
        DisplaySettingsModel m;

        QSignalSpy schemeSpy(&m, &DisplaySettingsModel::wfColorSchemeChanged);
        QSignalSpy gainSpy(&m, &DisplaySettingsModel::wfColorGainChanged);
        QSignalSpy blackSpy(&m, &DisplaySettingsModel::wfBlackLevelChanged);
        QSignalSpy refSpy(&m, &DisplaySettingsModel::refLevelChanged);
        QSignalSpy dynSpy(&m, &DisplaySettingsModel::dynamicRangeChanged);
        QSignalSpy alphaSpy(&m, &DisplaySettingsModel::fillAlphaChanged);
        QSignalSpy fillSpy(&m, &DisplaySettingsModel::panFillChanged);
        QSignalSpy fracSpy(&m, &DisplaySettingsModel::spectrumFracChanged);
        QSignalSpy modeSpy(&m, &DisplaySettingsModel::spectrumRenderModeChanged);
        QSignalSpy dssGainSpy(&m, &DisplaySettingsModel::dssGainChanged);
        QSignalSpy spanSpy(&m, &DisplaySettingsModel::dssRowSpanChanged);
        QSignalSpy angleSpy(&m, &DisplaySettingsModel::dssAngleChanged);
        QSignalSpy shadowSpy(&m, &DisplaySettingsModel::threeDSliceDepthChanged);
        QSignalSpy floorSpy(&m, &DisplaySettingsModel::dssFloorDepthChanged);

        m.setWfColorScheme(2);
        m.setWfColorGain(80);
        m.setWfBlackLevel(20);
        m.setRefLevel(-30.0f);
        m.setDynamicRange(90.0f);
        m.setFillAlpha(0.5f);
        m.setPanFill(false);
        m.setSpectrumFrac(0.5f);
        m.setSpectrumRenderMode(1);
        m.setDssGain(10);
        m.setDssRowSpan(10);
        m.setDssAngle(10);
        m.setThreeDSliceDepth(true);
        m.setDssFloorDepth(12);

        QCOMPARE(schemeSpy.count(), 1);
        QCOMPARE(gainSpy.count(), 1);
        QCOMPARE(blackSpy.count(), 1);
        QCOMPARE(refSpy.count(), 1);
        QCOMPARE(dynSpy.count(), 1);
        QCOMPARE(alphaSpy.count(), 1);
        QCOMPARE(fillSpy.count(), 1);
        QCOMPARE(fracSpy.count(), 1);
        QCOMPARE(modeSpy.count(), 1);
        QCOMPARE(dssGainSpy.count(), 1);
        QCOMPARE(spanSpy.count(), 1);
        QCOMPARE(angleSpy.count(), 1);
        QCOMPARE(shadowSpy.count(), 1);
        QCOMPARE(floorSpy.count(), 1);

        // Payload sanity, not just the count -- catches a signal that
        // fires the right number of times but with a stale/wrong value.
        QCOMPARE(gainSpy.at(0).at(0).toInt(), 80);
        QCOMPARE(floorSpy.at(0).at(0).toInt(), 12);
    }

    // ============================================================
    // The echo guard: setting a value to what it already holds emits
    // NOTHING. This is what stops two surfaces bound to the same model
    // field from bouncing an edit back and forth.
    // ============================================================

    // Catches: a missing (or backwards) equality guard -- a setter that
    // unconditionally assigns-and-emits even when nothing changed.
    void settingTheCurrentValueAgain_emitsNothing()
    {
        DisplaySettingsModel m;
        m.setWfColorGain(80);
        m.setRefLevel(-30.0f);
        m.setPanFill(false);
        m.setDssAngle(77);
        m.setDssFloorDepth(9);

        QSignalSpy gainSpy(&m, &DisplaySettingsModel::wfColorGainChanged);
        QSignalSpy refSpy(&m, &DisplaySettingsModel::refLevelChanged);
        QSignalSpy fillSpy(&m, &DisplaySettingsModel::panFillChanged);
        QSignalSpy angleSpy(&m, &DisplaySettingsModel::dssAngleChanged);
        QSignalSpy floorSpy(&m, &DisplaySettingsModel::dssFloorDepthChanged);

        m.setWfColorGain(80);          // same int
        m.setRefLevel(-30.0f);         // same float
        m.setPanFill(false);           // same bool
        m.setDssAngle(77);             // same int
        m.setDssFloorDepth(9);         // same int

        QCOMPARE(gainSpy.count(), 0);
        QCOMPARE(refSpy.count(), 0);
        QCOMPARE(fillSpy.count(), 0);
        QCOMPARE(angleSpy.count(), 0);
        QCOMPARE(floorSpy.count(), 0);

        // And the getters genuinely hold the value, not some earlier
        // default the no-op path silently fell back to.
        QCOMPARE(m.wfColorGain(), 80);
        QVERIFY(qFuzzyCompare(m.refLevel() + 1.0f, -30.0f + 1.0f));
        QCOMPARE(m.panFill(), false);
        QCOMPARE(m.dssAngle(), 77);
        QCOMPARE(m.dssFloorDepth(), 9);
    }

    // A value that CLAMPS to the current stored value must also emit
    // nothing -- e.g. already at the ceiling and asked to go higher
    // still. Catches a guard that compares the RAW incoming argument
    // instead of the clamped result.
    void aClampedNoOpEdit_emitsNothing()
    {
        DisplaySettingsModel m;
        m.setDssGain(100); // ceiling
        QSignalSpy spy(&m, &DisplaySettingsModel::dssGainChanged);
        m.setDssGain(500); // clamps to 100, same as current
        QCOMPARE(spy.count(), 0);
        QCOMPARE(m.dssGain(), 100);
    }

    // ============================================================
    // Clamping holds at both ends of every ranged control.
    // ============================================================

    // Catches: a missing clamp, a swapped min/max, or an off-by-one
    // boundary on any of the eleven ranged fields.
    void clampingHoldsAtBothEndsOfEveryRangedControl()
    {
        DisplaySettingsModel m;

        m.setWfColorScheme(-5);
        QCOMPARE(m.wfColorScheme(), 0);
        m.setWfColorScheme(999);
        QCOMPARE(m.wfColorScheme(), 7);

        m.setWfColorGain(-5);
        QCOMPARE(m.wfColorGain(), 0);
        m.setWfColorGain(999);
        QCOMPARE(m.wfColorGain(), 100);

        m.setWfBlackLevel(-5);
        QCOMPARE(m.wfBlackLevel(), 0);
        m.setWfBlackLevel(999);
        QCOMPARE(m.wfBlackLevel(), 125);

        m.setRefLevel(-9999.0f);
        QVERIFY(qFuzzyCompare(m.refLevel() + 1000.0f, -180.0f + 1000.0f));
        m.setRefLevel(9999.0f);
        QVERIFY(qFuzzyCompare(m.refLevel() + 1000.0f, 80.0f + 1000.0f));

        m.setDynamicRange(-9999.0f);
        QVERIFY(qFuzzyCompare(m.dynamicRange(), 10.0f));
        m.setDynamicRange(9999.0f);
        QVERIFY(qFuzzyCompare(m.dynamicRange(), 200.0f));

        m.setFillAlpha(-9.0f);
        QVERIFY(qFuzzyCompare(m.fillAlpha() + 1.0f, 0.0f + 1.0f));
        m.setFillAlpha(9.0f);
        QVERIFY(qFuzzyCompare(m.fillAlpha() + 1.0f, 1.0f + 1.0f));

        m.setSpectrumFrac(-9.0f);
        QVERIFY(qFuzzyCompare(m.spectrumFrac(), 0.10f));
        m.setSpectrumFrac(9.0f);
        QVERIFY(qFuzzyCompare(m.spectrumFrac(), 0.90f));

        m.setSpectrumRenderMode(-5);
        QCOMPARE(m.spectrumRenderMode(), 0);
        m.setSpectrumRenderMode(999);
        QCOMPARE(m.spectrumRenderMode(), 1);

        m.setDssFloorDepth(-5);
        QCOMPARE(m.dssFloorDepth(), 0);
        m.setDssFloorDepth(999);
        QCOMPARE(m.dssFloorDepth(), 24);

        m.setDssGain(-5);
        QCOMPARE(m.dssGain(), 0);
        m.setDssGain(999);
        QCOMPARE(m.dssGain(), 100);

        m.setDssRowSpan(-5);
        QCOMPARE(m.dssRowSpan(), 0);
        m.setDssRowSpan(999);
        QCOMPARE(m.dssRowSpan(), 100);

        m.setDssAngle(-5);
        QCOMPARE(m.dssAngle(), 0);
        m.setDssAngle(999);
        QCOMPARE(m.dssAngle(), 100);
    }

    // ============================================================
    // 3D Floor is untouched by load()/save(); the other thirteen
    // fields key per pan and round-trip through both.
    // ============================================================

    // Catches: load()/save() reaching into dssFloorDepth despite the
    // file header's explicit "load() and save() do not touch 3D Floor" --
    // either save() writing SOME AppSettings key for it under any name
    // (there must be none: as of Task 18 this field's persistence lives
    // entirely on PanadapterModel, not here), or load() overwriting an
    // in-memory value a caller already set.
    void dssFloorDepth_untouchedByLoadAndSave()
    {
        DisplaySettingsModel w;
        w.setPanIndex(0);
        w.setDssFloorDepth(19);
        w.setDssGain(80); // proves save() writes keys at all
        w.save();
        QVERIFY(AppSettings::instance().contains(QStringLiteral("Display3DGain")));
        const QStringList keys = AppSettings::instance().allKeys();
        for (const QString& key : keys) {
            QVERIFY(!key.startsWith(QStringLiteral("Display3DFloorDepth")));
        }

        DisplaySettingsModel r;
        r.setPanIndex(0);
        r.setDssFloorDepth(3);
        r.load();
        QCOMPARE(r.dssFloorDepth(), 3); // load() must not have touched it
    }

    // Catches: the per-pan key suffix convention drifting from
    // SpectrumWidget's own settingsKey(base, panIndex) -- pan 0 gets
    // the bare key, pan 1 gets "_1". Uses a field with no pan-0
    // fallback ambiguity (both keys are written by the SAME save()
    // call from two DIFFERENTLY-scoped model instances).
    void perPanFields_usePanIndexSuffixConvention()
    {
        DisplaySettingsModel pan0;
        pan0.setPanIndex(0);
        pan0.setWfColorGain(11);
        pan0.save();

        DisplaySettingsModel pan1;
        pan1.setPanIndex(1);
        pan1.setWfColorGain(22);
        pan1.save();

        QCOMPARE(AppSettings::instance().value(QStringLiteral("DisplayWfColorGain")).toInt(), 11);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("DisplayWfColorGain_1")).toInt(), 22);
    }

    // Catches: the pan-0-fallback-inheritance rule (2026-07-30 bench
    // fix, replicated from SpectrumWidget::loadSettings()) being
    // dropped -- an untouched pan must read pan 0's value, not the
    // hardcoded ship default, until it is given a value of its own.
    void anUntouchedPan_inheritsPanZerosValue()
    {
        DisplaySettingsModel pan0;
        pan0.setPanIndex(0);
        pan0.setWfBlackLevel(77);
        pan0.save(); // pan 1 never saves anything of its own

        DisplaySettingsModel pan1;
        pan1.setPanIndex(1);
        pan1.load();
        QCOMPARE(pan1.wfBlackLevel(), 77);
    }
};

QTEST_MAIN(TestDisplaySettingsModel)
#include "tst_display_settings_model.moc"
