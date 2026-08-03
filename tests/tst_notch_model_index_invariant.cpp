// =================================================================
// tests/tst_notch_model_index_invariant.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// TNF section 5.2: list position IS the WDSP notch index. Verified across
// add, edit and delete, including deleting from the middle, and verified
// that stable ids survive a mutation that shifts every later index down
// (the AetherSDR addition that retires Thetis's
// GetFirstNotchThatMatches selection-recovery dance).
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
//         section 5.2.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "models/NotchModel.h"

using namespace NereusSDR;

class TestNotchModelIndexInvariant : public QObject {
    Q_OBJECT

private slots:
    void add_appends_at_the_end()
    {
        NotchModel m;
        const int a = m.addNotch(14074000.0);
        const int b = m.addNotch(14075000.0);
        const int c = m.addNotch(14076000.0);

        QCOMPARE(m.notches().size(), 3);
        QCOMPARE(m.indexOfId(a), 0);
        QCOMPARE(m.indexOfId(b), 1);
        QCOMPARE(m.indexOfId(c), 2);
        QCOMPARE(m.notches().at(0).id, a);
        QCOMPARE(m.notches().at(2).id, c);
    }

    void add_out_of_frequency_order_still_appends_in_call_order()
    {
        // The list is index-ordered, not frequency-sorted: position must be
        // the WDSP index, and RXANBPAddNotch is an insert at position n.
        NotchModel m;
        const int a = m.addNotch(14076000.0);
        const int b = m.addNotch(14074000.0);
        QCOMPARE(m.indexOfId(a), 0);
        QCOMPARE(m.indexOfId(b), 1);
    }

    void delete_from_the_middle_shifts_later_indices_down()
    {
        NotchModel m;
        const int a = m.addNotch(14074000.0);
        const int b = m.addNotch(14075000.0);
        const int c = m.addNotch(14076000.0);

        QVERIFY(m.removeNotch(b));

        QCOMPARE(m.notches().size(), 2);
        QCOMPARE(m.indexOfId(a), 0);
        QCOMPARE(m.indexOfId(c), 1);
        QCOMPARE(m.indexOfId(b), -1);
        QCOMPARE(m.notchById(b), nullptr);
    }

    void delete_reports_the_former_index()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        const int b = m.addNotch(14075000.0);
        m.addNotch(14076000.0);

        QSignalSpy spy(&m, &NotchModel::notchRemoved);
        QVERIFY(m.removeNotch(b));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), b);
        QCOMPARE(spy.first().at(1).toInt(), 1);
    }

    void ids_survive_a_middle_delete()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        const int b = m.addNotch(14075000.0);
        const int c = m.addNotch(14076000.0);

        QVERIFY(m.removeNotch(b));

        // c kept its id even though its WDSP index moved from 2 to 1.
        QCOMPARE(m.notchById(c)->id, c);
        QCOMPARE(m.notchById(c)->centerHz, 14076000.0);
    }

    void edit_after_a_middle_delete_targets_the_shifted_index()
    {
        NotchModel m;
        const int a = m.addNotch(14074000.0);
        const int b = m.addNotch(14075000.0);
        const int c = m.addNotch(14076000.0);

        QVERIFY(m.removeNotch(b));
        QVERIFY(m.setCenter(c, 14077000.0));

        QCOMPARE(m.indexOfId(c), 1);
        QCOMPARE(m.notches().at(1).centerHz, 14077000.0);
        QCOMPARE(m.notches().at(0).centerHz, 14074000.0);
        QCOMPARE(m.indexOfId(a), 0);
    }

    void add_after_a_delete_appends_at_the_new_end()
    {
        NotchModel m;
        const int a = m.addNotch(14074000.0);
        const int b = m.addNotch(14075000.0);
        QVERIFY(m.removeNotch(a));

        const int c = m.addNotch(14076000.0);
        QCOMPARE(m.indexOfId(b), 0);
        QCOMPARE(m.indexOfId(c), 1);
        QVERIFY(c != a);
        QVERIFY(c != b);
    }

    void delete_rejects_unknown_id()
    {
        NotchModel m;
        m.addNotch(14074000.0);
        QSignalSpy spy(&m, &NotchModel::notchRemoved);
        QVERIFY(!m.removeNotch(9999));
        QCOMPARE(spy.count(), 0);
        QCOMPARE(m.notches().size(), 1);
    }

    void delete_rejected_while_admin_busy()
    {
        NotchModel m;
        const int a = m.addNotch(14074000.0);
        m.setAdminBusy(true);
        QVERIFY(!m.removeNotch(a));
        QCOMPARE(m.notches().size(), 1);
    }

    void indexOfId_returns_minus_one_on_empty_model()
    {
        NotchModel m;
        QCOMPARE(m.indexOfId(1), -1);
    }
};

QTEST_MAIN(TestNotchModelIndexInvariant)
#include "tst_notch_model_index_invariant.moc"
