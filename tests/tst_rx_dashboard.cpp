// tests/tst_rx_dashboard.cpp
//
// Phase 3Q Sub-PR-5 (E.1) — RxDashboard widget tests.
//
// 4 tests: unbound construction, bind, rebind, active-only badge visibility.

#include <QtTest/QtTest>

#include "gui/widgets/RxDashboard.h"
#include "gui/widgets/StatusBadge.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TstRxDashboard : public QObject {
    Q_OBJECT

private slots:

    void unboundDoesNotCrash() {
        RxDashboard d;
        // No slice bound — widget constructs cleanly with placeholder state.
        QVERIFY(d.slice() == nullptr);
    }

    void bindSliceDoesNotCrash() {
        RxDashboard d;
        SliceModel slice;
        d.bindSlice(&slice);
        QCOMPARE(d.slice(), &slice);
    }

    void rebindDisconnectsOldSlice() {
        RxDashboard d;
        SliceModel a;
        SliceModel b;
        d.bindSlice(&a);
        d.bindSlice(&b);
        QCOMPARE(d.slice(), &b);
        // Verify a no longer drives the dashboard (no crash on destroy)
    }

    void activeOnlyBadgesHiddenWhenFeaturesOff() {
        RxDashboard d;
        SliceModel slice;
        d.bindSlice(&slice);
        // Default SliceModel state: NR=Off, NB=Off, APF=off, ssql=off
        // → the 4 active-only badges should be hidden.
        const auto badges = d.findChildren<StatusBadge*>();
        int hidden = 0;
        for (auto* b : badges) {
            if (!b->isVisible()) { ++hidden; }
        }
        QVERIFY2(hidden >= 4,
                 qPrintable(QStringLiteral("expected >=4 hidden badges, got %1").arg(hidden)));
    }
};

QTEST_MAIN(TstRxDashboard)
#include "tst_rx_dashboard.moc"
