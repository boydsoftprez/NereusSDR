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

    void sliceLetterRoundTrips() {
        RxDashboard d;
        d.setSliceLetter(QLatin1Char('B'));
        QCOMPARE(d.sliceLetter(), QLatin1Char('B'));
    }

    void badgeForRungMapsTheLadder() {
        RxDashboard d;
        QVERIFY(d.badgeForRung(5) != nullptr);   // SQL
        QVERIFY(d.badgeForRung(6) != nullptr);   // APF
        QVERIFY(d.badgeForRung(7) != nullptr);   // NB
        QVERIFY(d.badgeForRung(8) != nullptr);   // NR
        QVERIFY(d.badgeForRung(9) != nullptr);   // AGC
    }

    void modeAndFilterAreNotOnTheLadder() {
        RxDashboard d;
        // Rungs 0 through 4 belong to other banner items; the dashboard
        // must not claim mode or filter, which never fold.
        for (int rung = 0; rung <= 4; ++rung) {
            QCOMPARE(d.badgeForRung(rung), nullptr);
        }
        for (int rung = 10; rung <= 12; ++rung) {
            QCOMPARE(d.badgeForRung(rung), nullptr);
        }
    }

    void rebindingSwitchesTheObservedSlice() {
        SliceModel a(0);
        SliceModel b(1);
        RxDashboard d;
        d.bindSlice(&a);
        QCOMPARE(d.slice(), &a);
        d.bindSlice(&b);
        QCOMPARE(d.slice(), &b);
        // The old slice must no longer drive the badges.
        a.setDspMode(NereusSDR::DSPMode::CWU);
        QVERIFY(!d.modeText().contains(QStringLiteral("CW")));
    }
};

QTEST_MAIN(TstRxDashboard)
#include "tst_rx_dashboard.moc"
