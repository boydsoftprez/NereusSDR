// tests/tst_dss_persistence.cpp
//
// no-port-check: NereusSDR-original persistence scheme (3D Stacked-Trace
// Spectrum Plan Task 14). Upstream AetherSDR groups its 3D settings in a
// single JSON object; NereusSDR uses flat AppSettings keys per project
// convention, so there is no upstream logic to port here.
//
// The six 3D controls split across two owners:
//
//   - Five are per PANADAPTER, stored on SpectrumWidget through the
//     existing settingsKey(base, panIndex) helper: spectrumRenderMode,
//     dssGain, dssRowSpan, dssAngle, threeDSliceDepth.
//   - 3D Floor (dssFloorDepth) is per BAND instead, stored on
//     PanadapterModel, keyed exactly like the existing per-band grid keys
//     built in PanadapterModel.cpp (Display3DFloorDepth_<bandKeyName>, no
//     pan index), because the floor is anchored to the measured noise
//     floor, which is strongly a per-band property, not a per-panadapter
//     one. See design doc
//     docs/architecture/2026-08-08-3d-stacked-trace-spectrum-design.md §6.
//
// Isolation: AppSettings::instance() is a process-wide in-memory singleton
// with no automatic load()/save() in this test process (TestSandboxInit.cpp
// also sandboxes QStandardPaths so nothing here can reach the developer's
// real ~/.config/NereusSDR/NereusSDR.settings). init()/cleanup() clear it
// before and after every slot so no test's writes can leak into another's
// fresh PanadapterModel/SpectrumWidget construction.
//
// Fix-forward (coordinator review): storage alone does not make 3D Floor
// recall on band change operator-visible -- something has to push the
// stored value into the live SpectrumWidget. That connection is
// MainWindow::wireDss3DFloorRecallForTest, a static composition seam
// (same shape as the pre-existing wireWidebandExtensionForTest etc. in
// MainWindow.h) that MainWindow's constructor calls verbatim, so the test
// below exercises the exact connect() call production code makes, not a
// parallel test-only copy of it.

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "gui/MainWindow.h"
#include "gui/SpectrumWidget.h"
#include "models/PanadapterModel.h"

using namespace NereusSDR;

class TestDssPersistence : public QObject
{
    Q_OBJECT

private slots:
    void init()    { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ============================================================
    // SpectrumWidget: five per-panadapter keys
    // ============================================================

    // Catches: loadSettings()/saveSettings() not touching the five new
    // fields at all (round trip would silently keep fresh-object ship
    // defaults instead of the written values); wrong key names; the
    // per-pan settingsKey() plumbing dropped for these five.
    //
    // Uses two DISTINCT SpectrumWidget instances (w writes, r reads) so
    // the assertion can only pass if loadSettings() genuinely read
    // AppSettings: a fresh r starts from ship defaults, not from w's
    // in-memory state.
    void fivePerPanKeys_roundTrip()
    {
        SpectrumWidget w;
        w.setPanIndex(0);
        w.setSpectrumRenderMode(static_cast<int>(SpectrumRenderMode::Mode3D));
        w.setDssGain(33);
        w.setDssRowSpan(44);
        w.setDssAngle(66);
        w.setThreeDSliceDepth(true);
        w.saveSettings();

        SpectrumWidget r;
        r.setPanIndex(0);
        r.loadSettings();
        QCOMPARE(r.spectrumRenderMode(),
                 static_cast<int>(SpectrumRenderMode::Mode3D));
        QCOMPARE(r.dssGain(),     33);
        QCOMPARE(r.dssRowSpan(),  44);
        QCOMPARE(r.dssAngle(),    66);
        QCOMPARE(r.threeDSliceDepth(), true);
    }

    // Catches: booleans written via QString::number()/"1"/"0" instead of
    // the project's mandated "True"/"False" strings (CLAUDE.md persistence
    // rule). Reads the RAW stored string directly rather than round
    // tripping through the bool getter, which would mask a "1" vs "True"
    // mismatch (toString() == "True" is false for "1" too, so a getter
    // round trip alone can't distinguish "wrote wrong string, read back
    // false" from "wrote nothing, defaulted false" -- reading the raw
    // value can).
    //
    // Pan index 0 maps to the BARE key (no "_0" / "_pan0" suffix) per
    // settingsKey()'s own convention (see the "if (panIndex == 0) return
    // base;" branch) -- confirmed against every existing per-pan key
    // in this file (e.g. tst_pan_display_settings_inherit.cpp's
    // "DisplayGridMax" for pan 0 vs "DisplayGridMax_1" for pan 1).
    void booleansPersistAsTrueFalseStrings()
    {
        SpectrumWidget w;
        w.setPanIndex(0);
        w.setThreeDSliceDepth(true);
        w.saveSettings();
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("Display3DSliceShadow"))
                     .toString(),
                 QStringLiteral("True"));
    }

    // Catches: dssFloorDepth accidentally folded into SpectrumWidget's
    // per-pan save list (the design this task explicitly rejects: 3D
    // Floor is per band on PanadapterModel, not per pan on SpectrumWidget).
    // Proves the negative by first proving saveSettings() really does
    // write keys at all (Display3DGain, asserted present); an absence
    // is only meaningful once presence is shown to be reachable.
    void dssFloorDepthIsNotPersistedBySpectrumWidget()
    {
        SpectrumWidget w;
        w.setPanIndex(0);
        w.setDssFloorDepth(15);
        w.setDssGain(80);
        w.saveSettings();

        QVERIFY(AppSettings::instance().contains(QStringLiteral("Display3DGain")));
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("Display3DFloorDepth")));
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("Display3DFloorDepth_0")));
    }

    // ============================================================
    // PanadapterModel: one per-band key
    // ============================================================

    // Catches: storage that is not really per band (e.g. a single scalar
    // ignoring the Band argument) -- two different bands set to two
    // different values must read back independently.
    void floorDepth_isRecalledPerBand()
    {
        PanadapterModel m;
        m.setDss3DFloorDepthForBand(Band::Band80m, 4);
        m.setDss3DFloorDepthForBand(Band::Band10m, 18);
        QCOMPARE(m.dss3DFloorDepthForBand(Band::Band80m),  4);
        QCOMPARE(m.dss3DFloorDepthForBand(Band::Band10m), 18);
    }

    // Catches: wrong key prefix/suffix (e.g. missing the Display3D prefix,
    // or accidentally inventing a pan-scoped variant of this key, which
    // the design doc explicitly rejects -- see the class-header comment
    // on PanadapterModel::setDss3DFloorDepthForBand).
    void floorDepthKey_followsTheGridKeyConvention()
    {
        PanadapterModel m;
        m.setDss3DFloorDepthForBand(Band::Band20m, 9);
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("Display3DFloorDepth_")
                            + bandKeyName(Band::Band20m)).toInt(),
                 9);
    }

    // Catches: wrong upstream default (design doc §6: ship default is 6,
    // matching SpectrumWidget's m_dssFloorDepth ship default) -- a fresh
    // model, band never touched.
    void unsetBand_returnsTheUpstreamDefault()
    {
        PanadapterModel m;
        QCOMPARE(m.dss3DFloorDepthForBand(Band::Band6m), 6);
    }

    // Catches: setBand()/setCenterFrequency() corrupting or mis-keying the
    // per-band store across a real band transition (as opposed to the
    // direct same-object set/get in floorDepth_isRecalledPerBand above,
    // which never calls setBand() at all). Sets the value while the model
    // is still on its default band (20m, from the default 14.225 MHz
    // center), then drives a genuine band change via setCenterFrequency()
    // before reading back through the now-current band.
    void bandChange_pushesTheStoredDepth()
    {
        PanadapterModel m;
        QCOMPARE(m.band(), Band::Band20m);  // precondition: not yet 40m
        m.setDss3DFloorDepthForBand(Band::Band40m, 15);
        m.setCenterFrequency(7100000.0);    // 7.1 MHz -> 40m
        QCOMPARE(m.band(), Band::Band40m);
        QCOMPARE(m.dss3DFloorDepthForBand(m.band()), 15);
    }

    // Catches: setDss3DFloorDepthForBand only updating the in-memory
    // QHash without ever calling AppSettings::setValue (i.e. "persists"
    // in name only) -- the write happens on an instance that goes out of
    // scope and is destroyed, so the read can only succeed if a SECOND,
    // freshly constructed instance's constructor genuinely loaded the
    // value back out of AppSettings via loadPerBandGridFromSettings().
    void floorDepthPersistsAcrossFreshConstruction()
    {
        {
            PanadapterModel m;
            m.setDss3DFloorDepthForBand(Band::Band17m, 21);
        }
        PanadapterModel m2;
        QCOMPARE(m2.dss3DFloorDepthForBand(Band::Band17m), 21);
        // A neighboring, untouched band must still read the default:
        // proves the load loop didn't smear one band's value onto others.
        QCOMPARE(m2.dss3DFloorDepthForBand(Band::Band15m), 6);
    }

    // Catches: dss3DFloorDepth folded into saveBandGridToSettings()'s
    // unconditional dbMax/dbMin bundle. If it were, every 2D-only user
    // who ever adjusts their per-band grid range (a common, everyday
    // action wholly unrelated to 3D) would get a Display3DFloorDepth_
    // key written for that band despite never having opened 3D mode or
    // touched the 3D Floor control. setPerBandDbMax/setPerBandDbMin are
    // the ONLY entry points exercised here; setDss3DFloorDepthForBand is
    // deliberately never called.
    void gridOnlyChange_doesNotWriteDssFloorKey()
    {
        PanadapterModel m;
        m.setPerBandDbMax(Band::Band30m, -35);
        m.setPerBandDbMin(Band::Band30m, -130);

        QVERIFY(AppSettings::instance().contains(
            QStringLiteral("DisplayGridMax_") + bandKeyName(Band::Band30m)));
        QVERIFY(!AppSettings::instance().contains(
            QStringLiteral("Display3DFloorDepth_") + bandKeyName(Band::Band30m)));
    }

    // ============================================================
    // End-to-end recall: PanadapterModel -> live SpectrumWidget
    // ============================================================

    // Catches: the recall connection never wired at all, wired to the
    // wrong signal, or wired but reading the wrong band. Unlike
    // floorDepth_isRecalledPerBand (direct set-then-get on the model,
    // never touches a SpectrumWidget) this drives the actual production
    // seam, MainWindow::wireDss3DFloorRecallForTest, and asserts on the
    // WIDGET's own dssFloorDepth() getter -- the thing an operator
    // actually sees on the panadapter -- not the model's storage.
    //
    // Both bands are set up front, before wiring, so the first assertion
    // also exercises the seam's initial push (band 80m is already current
    // when wireDss3DFloorRecallForTest runs), not only its bandChanged
    // handler.
    void bandChangeRecallsIntoTheLiveWidget_endToEnd()
    {
        PanadapterModel pan;
        SpectrumWidget sw;

        pan.setDss3DFloorDepthForBand(Band::Band80m, 4);
        pan.setDss3DFloorDepthForBand(Band::Band10m, 18);

        pan.setCenterFrequency(3700000.0);  // 3.7 MHz -> 80m
        QCOMPARE(pan.band(), Band::Band80m);

        MainWindow::wireDss3DFloorRecallForTest(&pan, &sw);
        QCOMPARE(sw.dssFloorDepth(), 4);  // initial push landed on wiring

        pan.setCenterFrequency(28400000.0);  // 28.4 MHz -> 10m
        QCOMPARE(pan.band(), Band::Band10m);
        QCOMPARE(sw.dssFloorDepth(), 18);  // live widget followed the change

        pan.setCenterFrequency(3700000.0);  // back to 80m
        QCOMPARE(pan.band(), Band::Band80m);
        QCOMPARE(sw.dssFloorDepth(), 4);  // and back again
    }
};

QTEST_MAIN(TestDssPersistence)
#include "tst_dss_persistence.moc"
