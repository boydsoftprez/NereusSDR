// =================================================================
// tests/tst_notch_model_guards.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// TNF section 5.4: NotchModel's ported guards. Add path in this file's
// first half, edit path in the second.
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
//         section 5.4 (guard table), section 1.2 (CW pitch correction
//         deliberately NOT ported).
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "models/NotchModel.h"

using namespace NereusSDR;

class TestNotchModelGuards : public QObject {
    Q_OBJECT

private slots:
    // Whole-Hz rounding on add (Thetis console.cs:40230)
    void add_rounds_centre_to_whole_hz()
    {
        NotchModel m;
        const int id = m.addNotch(14074123.4);
        QVERIFY(id > 0);
        QCOMPARE(m.notchById(id)->centerHz, 14074123.0);
    }

    void add_rounds_midpoint_to_even()
    {
        // C# Math.Round(double) is MidpointRounding.ToEven. Both midpoints
        // below therefore land on the even neighbour 14074124.
        NotchModel a;
        const int idA = a.addNotch(14074123.5);
        QVERIFY(idA > 0);
        QCOMPARE(a.notchById(idA)->centerHz, 14074124.0);

        NotchModel b;
        const int idB = b.addNotch(14074124.5);
        QVERIFY(idB > 0);
        QCOMPARE(b.notchById(idB)->centerHz, 14074124.0);
    }

    // Width defaults (Thetis console.cs:40268-40269)
    void add_default_width_is_200hz()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QCOMPARE(m.notchById(id)->widthHz, NotchModel::kDefaultNotchWidthHz);
        QCOMPARE(NotchModel::kDefaultNotchWidthHz, 200.0);
    }

    void add_narrow_width_is_100hz()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0, NotchModel::kNarrowNotchWidthHz);
        QCOMPARE(m.notchById(id)->widthHz, 100.0);
        QCOMPARE(NotchModel::kNarrowNotchWidthHz, 100.0);
    }

    void add_marks_new_notch_active()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QVERIFY(m.notchById(id)->active);
    }

    void add_emits_notchAdded_with_the_new_id()
    {
        NotchModel m;
        QSignalSpy spy(&m, &NotchModel::notchAdded);
        const int id = m.addNotch(14074000.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), id);
    }

    // 10 Hz dedupe, exact boundary (Thetis console.cs:40260 via
    // radio.cs:4267: strict `<`, so exactly 10 Hz away is allowed)
    void dedupe_window_is_ten_hz()
    {
        QCOMPARE(NotchModel::kNotchDedupeWindowHz, 10);
    }

    void add_rejects_notch_within_ten_hz()
    {
        NotchModel m;
        QVERIFY(m.addNotch(14074000.0) > 0);
        QCOMPARE(m.addNotch(14074009.0), -1);
        QCOMPARE(m.addNotch(14073991.0), -1);
        QCOMPARE(m.notches().size(), 1);
    }

    void add_allows_notch_exactly_ten_hz_away()
    {
        NotchModel m;
        QVERIFY(m.addNotch(14074000.0) > 0);
        QVERIFY(m.addNotch(14074010.0) > 0);
        QCOMPARE(m.notches().size(), 2);

        NotchModel below;
        QVERIFY(below.addNotch(14074000.0) > 0);
        QVERIFY(below.addNotch(14073990.0) > 0);
        QCOMPARE(below.notches().size(), 2);
    }

    void add_rejection_emits_reason()
    {
        NotchModel m;
        QVERIFY(m.addNotch(14074000.0) > 0);
        QSignalSpy spy(&m, &NotchModel::notchAddRejected);
        QCOMPARE(m.addNotch(14074002.0), -1);
        QCOMPARE(spy.count(), 1);
        QVERIFY(!spy.first().first().toString().isEmpty());
    }

    // Centre constrained to the radio tuning range
    // (Thetis console.cs:40257; bounds per design section 5.4)
    void add_rejects_centre_below_minimum()
    {
        NotchModel m;
        QCOMPARE(m.addNotch(NotchModel::kMinNotchCentreHz - 1.0), -1);
        QVERIFY(m.notches().isEmpty());
    }

    void add_accepts_centre_exactly_at_minimum()
    {
        NotchModel m;
        QVERIFY(m.addNotch(NotchModel::kMinNotchCentreHz) > 0);
    }

    void add_rejects_centre_above_maximum()
    {
        NotchModel m;
        QCOMPARE(m.addNotch(NotchModel::kMaxNotchCentreHz + 1.0), -1);
        QVERIFY(m.notches().isEmpty());
    }

    void add_accepts_centre_exactly_at_maximum()
    {
        NotchModel m;
        QVERIFY(m.addNotch(NotchModel::kMaxNotchCentreHz) > 0);
    }

    void constrain_bounds_match_the_vfo_clamp()
    {
        QCOMPARE(NotchModel::kMinNotchCentreHz, 100000.0);
        QCOMPARE(NotchModel::kMaxNotchCentreHz, 61440000.0);
    }

    // NotchAdminBusy (Thetis console.cs:40224)
    void add_rejected_while_admin_busy()
    {
        NotchModel m;
        m.setAdminBusy(true);
        QVERIFY(m.adminBusy());
        QCOMPARE(m.addNotch(14074000.0), -1);
        QVERIFY(m.notches().isEmpty());
    }

    void add_resumes_when_admin_busy_clears()
    {
        NotchModel m;
        m.setAdminBusy(true);
        QCOMPARE(m.addNotch(14074000.0), -1);
        m.setAdminBusy(false);
        QVERIFY(m.addNotch(14074000.0) > 0);
    }

    // Design section 1.2: the Thetis CW-pitch correction is deliberately
    // NOT ported. NotchModel takes no mode and no rx argument, so the CWU
    // and CWL calls are literally the same call and both store F exactly.
    // A ported GetDSPcwPitchShiftToZero would show up here as F +/- pitch.
    void cw_upper_notch_stores_at_exact_frequency()
    {
        NotchModel m;
        const double f = 7025000.0;
        const int id = m.addNotch(f);
        QCOMPARE(m.notchById(id)->centerHz, f);
    }

    void cw_lower_notch_stores_at_exact_frequency()
    {
        NotchModel m;
        const double f = 7025000.0;
        const int id = m.addNotch(f);
        QCOMPARE(m.notchById(id)->centerHz, f);
    }

    // ids are stable and monotonic (AetherSDR TnfEntry::id addition)
    void ids_are_monotonic_and_distinct()
    {
        NotchModel m;
        const int a = m.addNotch(14074000.0);
        const int b = m.addNotch(14075000.0);
        const int c = m.addNotch(14076000.0);
        QVERIFY(a < b);
        QVERIFY(b < c);
        QCOMPARE(m.notchById(a)->id, a);
        QCOMPARE(m.notchById(c)->id, c);
    }

    void notchById_returns_nullptr_for_unknown_id()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        QCOMPARE(m.notchById(9999), nullptr);
    }

    // setCenter: constrain, admin-busy, whole-Hz rounding
    // (Thetis console.cs:40077, :40079, :40081)
    void setCenter_rounds_to_whole_hz()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QVERIFY(m.setCenter(id, 14074500.4));
        QCOMPARE(m.notchById(id)->centerHz, 14074500.0);
    }

    void setCenter_rejects_below_minimum()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QVERIFY(!m.setCenter(id, NotchModel::kMinNotchCentreHz - 1.0));
        QCOMPARE(m.notchById(id)->centerHz, 14074000.0);
    }

    void setCenter_rejects_above_maximum()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QVERIFY(!m.setCenter(id, NotchModel::kMaxNotchCentreHz + 1.0));
        QCOMPARE(m.notchById(id)->centerHz, 14074000.0);
    }

    void setCenter_rejected_while_admin_busy()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        m.setAdminBusy(true);
        QSignalSpy spy(&m, &NotchModel::notchChanged);
        QVERIFY(!m.setCenter(id, 14075000.0));
        QCOMPARE(spy.count(), 0);
        QCOMPARE(m.notchById(id)->centerHz, 14074000.0);
    }

    void setCenter_emits_notchChanged_once_on_change()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QSignalSpy spy(&m, &NotchModel::notchChanged);
        QVERIFY(m.setCenter(id, 14075000.0));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), id);
    }

    void setCenter_is_silent_when_value_unchanged()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QSignalSpy spy(&m, &NotchModel::notchChanged);
        // Upstream returns true whenever the index resolves, and only fires
        // its change handler when the value actually moved
        // (console.cs:40109-40113 [v2.10.3.15]).
        QVERIFY(m.setCenter(id, 14074000.0));
        QCOMPARE(spy.count(), 0);
    }

    void setCenter_rejects_unknown_id()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        QVERIFY(!m.setCenter(9999, 14075000.0));
    }

    // setWidth: wheel-resize clamps (Thetis console.cs:33312-33318)
    void wheel_steps_match_thetis_notch_mouse_wheel()
    {
        // console.cs:33305-33310: Shift held adds the raw detent count,
        // no modifier multiplies it by 10.
        QCOMPARE(NotchModel::kWheelWidthStepHz, 10.0);
        QCOMPARE(NotchModel::kWheelWidthStepFineHz, 1.0);
    }

    void setWidth_clamps_to_max_filter_width()
    {
        NotchModel m;
        const int id = m.addNotch(1000000.0);
        QVERIFY(m.setWidth(id, 20000.0));
        QCOMPARE(m.notchById(id)->widthHz, NotchModel::kMaxNotchWidthHz);
        QCOMPARE(NotchModel::kMaxNotchWidthHz, 10000.0);
    }

    void setWidth_clamps_negative_to_zero()
    {
        NotchModel m;
        const int id = m.addNotch(1000000.0);
        QVERIFY(m.setWidth(id, -50.0));
        QCOMPARE(m.notchById(id)->widthHz, 0.0);
    }

    void setWidth_rejects_when_upper_edge_leaves_the_range()
    {
        // Thetis rejects outright rather than clamping the width down:
        // "check to see if outside frequency limits" (console.cs:33315-33318).
        NotchModel m;
        const int id = m.addNotch(NotchModel::kMaxNotchCentreHz - 1000.0);
        QVERIFY(!m.setWidth(id, 5000.0));
        QCOMPARE(m.notchById(id)->widthHz, NotchModel::kDefaultNotchWidthHz);
    }

    void setWidth_accepts_when_upper_edge_stays_inside_the_range()
    {
        NotchModel m;
        const int id = m.addNotch(NotchModel::kMaxNotchCentreHz - 5000.0);
        QVERIFY(m.setWidth(id, 8000.0));
        QCOMPARE(m.notchById(id)->widthHz, 8000.0);
    }

    void setWidth_rejected_while_admin_busy()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        m.setAdminBusy(true);
        QVERIFY(!m.setWidth(id, 400.0));
        QCOMPARE(m.notchById(id)->widthHz, NotchModel::kDefaultNotchWidthHz);
    }

    void setWidth_rejects_unknown_id()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        QVERIFY(!m.setWidth(9999, 400.0));
    }

    void setWidth_emits_notchChanged_once_on_change()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QSignalSpy spy(&m, &NotchModel::notchChanged);
        QVERIFY(m.setWidth(id, 400.0));
        QCOMPARE(spy.count(), 1);
    }

    // setActive (Thetis console.cs:40123-40156)
    void setActive_toggles_and_emits()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QSignalSpy spy(&m, &NotchModel::notchChanged);
        QVERIFY(m.setActive(id, false));
        QVERIFY(!m.notchById(id)->active);
        QCOMPARE(spy.count(), 1);
    }

    void setActive_is_silent_when_value_unchanged()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        QSignalSpy spy(&m, &NotchModel::notchChanged);
        QVERIFY(m.setActive(id, true));
        QCOMPARE(spy.count(), 0);
    }

    void setActive_rejected_while_admin_busy()
    {
        NotchModel m;
        const int id = m.addNotch(14074000.0);
        m.setAdminBusy(true);
        QVERIFY(!m.setActive(id, false));
        QVERIFY(m.notchById(id)->active);
    }
};

QTEST_MAIN(TestNotchModelGuards)
#include "tst_notch_model_guards.moc"
