// tests/tst_rx_dashboard.cpp
//
// Phase 3Q Sub-PR-5 (E.1) — RxDashboard widget tests.
//
// 4 tests: unbound construction, bind, rebind, active-only badge visibility.

#include <QtTest/QtTest>
#include <QSignalSpy>

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
        // Task A8 fix round 1: RxDashboard no longer setVisible()'s these
        // pills itself (ChromeBarController owns visibility once
        // registered); it reports DSP-active state via
        // badgeAvailabilityChanged instead. isVisible() is not a
        // meaningful check here any more -- and was already a weak one
        // even before this change, since an unshown, unparented top-level
        // RxDashboard reports isVisible()==false for EVERY child
        // regardless of intent, mode/filter/AGC included, not just the
        // 4 that are meant to start off.
        RxDashboard d;
        QSignalSpy spy(&d, &RxDashboard::badgeAvailabilityChanged);
        SliceModel slice;
        d.bindSlice(&slice);
        // Default SliceModel state: NR=Off, NB=Off, APF=off, ssql=off
        // → each of the 4 active-only rungs reports available=false
        // during the bind-time seed pass. AGC has no off state.
        QHash<int, bool> lastByRung;
        for (const QList<QVariant>& call : spy) {
            lastByRung[call.at(0).toInt()] = call.at(1).toBool();
        }
        QVERIFY(lastByRung.contains(5));  // SQL
        QVERIFY(lastByRung.contains(6));  // APF
        QVERIFY(lastByRung.contains(7));  // NB
        QVERIFY(lastByRung.contains(8));  // NR
        QCOMPARE(lastByRung.value(5), false);
        QCOMPARE(lastByRung.value(6), false);
        QCOMPARE(lastByRung.value(7), false);
        QCOMPARE(lastByRung.value(8), false);
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
