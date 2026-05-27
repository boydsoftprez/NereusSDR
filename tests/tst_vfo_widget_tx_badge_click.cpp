// =================================================================
// tests/tst_vfo_widget_tx_badge_click.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic C Task 9: VfoWidget TX badge click emits
// txHandoffRequested(sliceIndex), forwarded by MainWindow to
// RadioModel::txSliceArbiter()->requestHandoff().
// =================================================================
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "gui/widgets/VfoWidget.h"

using namespace NereusSDR;

class TestVfoWidgetTxBadgeClick : public QObject {
    Q_OBJECT
private slots:
    void tx_badge_click_emits_handoff_request_with_slice_index()
    {
        VfoWidget widget;
        widget.setSliceIndex(1);  // this widget represents Slice B

        QSignalSpy spy(&widget, &VfoWidget::txHandoffRequested);

        // Simulate badge click (test-only seam — see VfoWidget::simulateTxBadgeClick)
        widget.simulateTxBadgeClick();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), 1);  // sliceIndex=1
    }
};

QTEST_MAIN(TestVfoWidgetTxBadgeClick)
#include "tst_vfo_widget_tx_badge_click.moc"
