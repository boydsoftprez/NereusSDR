// tests/tst_pa_calibration_group.cpp  (NereusSDR)
//
// TDD tests for PaCalibrationGroup widget — Section 3.3 of the P1 full-
// parity epic. See docs/architecture/2026-05-02-p1-full-parity-plan.md §3.3.
//
// no-port-check: test file — Thetis cites in commentary only; behaviour
// is exercised through PaCalProfile + CalibrationController (already-cited
// owners). Source attribution: Thetis setup.cs:5404-5594 ud{10|100|200}PA{N}W
// spinbox blocks [v2.10.3.13 @501e3f51].
//
// Covers:
//   - default_construction_has_no_spinboxes: fresh widget before populate()
//     has zero spinboxes.
//   - populate_anan10_creates_10_labelled_spinboxes: assert count == 10,
//     labelTextForTest(1) == "1 W", labelTextForTest(10) == "10 W", visible.
//   - populate_anan100_uses_10W_intervals: labels "10 W" .. "100 W".
//   - populate_anan8000_uses_20W_intervals: labels "20 W" .. "200 W".
//   - populate_hermeslite_uses_anan10_intervals: HL2 maps to Anan10 per
//     mi0bot setup.cs:5463-5466 [v2.10.3.13-beta2 @c26a8a4]; labels "1 W" .. "10 W".
//   - populate_none_hides_group: setVisible(false) when boardClass == None.
//   - spinbox_change_writes_to_controller: setSpinValueForTest writes
//     m_calCtrl->paCalProfile().watts[idx].
//   - controller_paCalPointChanged_updates_spinbox: external setPaCalPoint
//     reflects in spinbox value.
//   - repopulate_with_new_class_rebuilds_spinboxes: populate Anan10 then
//     Anan100 — labels and ranges change accordingly.
//   - echo_loop_guard_no_recursion: setSpinValueForTest while
//     paCalPointChanged fires does not infinitely recurse — final value
//     is consistent.

#include "gui/setup/hardware/PaCalibrationGroup.h"
#include "core/CalibrationController.h"
#include "core/PaCalProfile.h"

#include <QtTest/QtTest>
#include <QSignalSpy>

using namespace NereusSDR;

class TstPaCalibrationGroup : public QObject {
    Q_OBJECT

private slots:
    void default_construction_has_no_spinboxes();
    void populate_anan10_creates_10_labelled_spinboxes();
    void populate_anan100_uses_10W_intervals();
    void populate_anan8000_uses_20W_intervals();
    void populate_hermeslite_uses_anan10_intervals();
    void populate_none_hides_group();
    void spinbox_change_writes_to_controller();
    void controller_paCalPointChanged_updates_spinbox();
    void repopulate_with_new_class_rebuilds_spinboxes();
    void echo_loop_guard_no_recursion();
};

// ---------------------------------------------------------------------------

void TstPaCalibrationGroup::default_construction_has_no_spinboxes()
{
    PaCalibrationGroup grp;
    QCOMPARE(grp.spinBoxCountForTest(), 0);
}

void TstPaCalibrationGroup::populate_anan10_creates_10_labelled_spinboxes()
{
    CalibrationController ctrl;
    PaCalibrationGroup grp;
    grp.populate(&ctrl, PaCalBoardClass::Anan10);

    QCOMPARE(grp.spinBoxCountForTest(), 10);
    QCOMPARE(grp.labelTextForTest(1), QString("1 W"));
    QCOMPARE(grp.labelTextForTest(10), QString("10 W"));
    QVERIFY(!grp.isHidden());
}

void TstPaCalibrationGroup::populate_anan100_uses_10W_intervals()
{
    CalibrationController ctrl;
    PaCalibrationGroup grp;
    grp.populate(&ctrl, PaCalBoardClass::Anan100);

    QCOMPARE(grp.spinBoxCountForTest(), 10);
    QCOMPARE(grp.labelTextForTest(1), QString("10 W"));
    QCOMPARE(grp.labelTextForTest(5), QString("50 W"));
    QCOMPARE(grp.labelTextForTest(10), QString("100 W"));
}

void TstPaCalibrationGroup::populate_anan8000_uses_20W_intervals()
{
    CalibrationController ctrl;
    PaCalibrationGroup grp;
    grp.populate(&ctrl, PaCalBoardClass::Anan8000);

    QCOMPARE(grp.spinBoxCountForTest(), 10);
    QCOMPARE(grp.labelTextForTest(1), QString("20 W"));
    QCOMPARE(grp.labelTextForTest(10), QString("200 W"));
}

void TstPaCalibrationGroup::populate_hermeslite_uses_anan10_intervals()
{
    // mi0bot setup.cs:5463-5466 [v2.10.3.13-beta2 @c26a8a4] — HL2 grouped
    // with ANAN10/ANAN10E for PA cal: same ud10PA1W..ud10PA10W spinbox
    // set, same 1 W intervals, same 10 W max. So when the widget is
    // populated for HL2 it must render the Anan10 way: labels "1 W" through
    // "10 W". Earlier NereusSDR placeholder used a separate HermesLite
    // class (0.5 W intervals / 5 W max); dropped 2026-05-02 after upstream
    // verification.
    CalibrationController ctrl;
    PaCalibrationGroup grp;
    grp.populate(&ctrl, PaCalBoardClass::Anan10);

    QCOMPARE(grp.spinBoxCountForTest(), 10);
    QCOMPARE(grp.labelTextForTest(1), QString("1 W"));
    QCOMPARE(grp.labelTextForTest(10), QString("10 W"));
}

void TstPaCalibrationGroup::populate_none_hides_group()
{
    CalibrationController ctrl;
    PaCalibrationGroup grp;
    grp.populate(&ctrl, PaCalBoardClass::None);

    QCOMPARE(grp.spinBoxCountForTest(), 0);
    QVERIFY(grp.isHidden());
}

void TstPaCalibrationGroup::spinbox_change_writes_to_controller()
{
    CalibrationController ctrl;
    ctrl.setPaCalProfile(PaCalProfile::defaults(PaCalBoardClass::Anan100));

    PaCalibrationGroup grp;
    grp.populate(&ctrl, PaCalBoardClass::Anan100);

    grp.setSpinValueForTest(5, 47.0);

    QCOMPARE(ctrl.paCalProfile().watts[5], 47.0f);
}

void TstPaCalibrationGroup::controller_paCalPointChanged_updates_spinbox()
{
    CalibrationController ctrl;
    ctrl.setPaCalProfile(PaCalProfile::defaults(PaCalBoardClass::Anan100));

    PaCalibrationGroup grp;
    grp.populate(&ctrl, PaCalBoardClass::Anan100);

    ctrl.setPaCalPoint(3, 33.0f);

    QCOMPARE(grp.spinValueForTest(3), 33.0);
}

void TstPaCalibrationGroup::repopulate_with_new_class_rebuilds_spinboxes()
{
    CalibrationController ctrl;
    PaCalibrationGroup grp;

    grp.populate(&ctrl, PaCalBoardClass::Anan10);
    QCOMPARE(grp.labelTextForTest(10), QString("10 W"));

    grp.populate(&ctrl, PaCalBoardClass::Anan100);
    QCOMPARE(grp.spinBoxCountForTest(), 10);
    QCOMPARE(grp.labelTextForTest(10), QString("100 W"));
}

void TstPaCalibrationGroup::echo_loop_guard_no_recursion()
{
    CalibrationController ctrl;
    ctrl.setPaCalProfile(PaCalProfile::defaults(PaCalBoardClass::Anan100));

    PaCalibrationGroup grp;
    grp.populate(&ctrl, PaCalBoardClass::Anan100);

    // Drive spin → controller → spin echo loop. If the guard is missing
    // we'd recurse infinitely; if present, the loop terminates and the
    // final value is consistent on both ends.
    grp.setSpinValueForTest(2, 22.0);

    QCOMPARE(grp.spinValueForTest(2), 22.0);
    QCOMPARE(ctrl.paCalProfile().watts[2], 22.0f);
}

QTEST_MAIN(TstPaCalibrationGroup)
#include "tst_pa_calibration_group.moc"
