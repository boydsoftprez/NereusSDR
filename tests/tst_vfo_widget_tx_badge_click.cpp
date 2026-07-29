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
#include "core/AppSettings.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestVfoWidgetTxBadgeClick : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

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

    void context_menu_resolves_slice_c_by_stable_id_after_b_is_removed()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        model.addSlice();
        const int b = model.addSlice();
        const int c = model.addSlice();
        model.removeSlice(b);
        QCOMPARE(model.slices().at(1)->sliceIndex(), c);

        VfoWidget widget;
        widget.setSliceIndex(c);
        widget.setRadioModel(&model);

        QCOMPARE(widget.contextMenuSliceForTest(), model.sliceById(c));
    }

    void tx_badge_handoff_selects_slice_c_by_stable_id_after_b_is_removed()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        model.addSlice();
        const int b = model.addSlice();
        const int c = model.addSlice();
        model.removeSlice(b);

        VfoWidget widget;
        widget.setSliceIndex(c);
        QObject::connect(&widget, &VfoWidget::txHandoffRequested,
                         &model, [&model](int sliceId) {
            model.requestTxHandoffToSlice(sliceId);
        });

        widget.simulateTxBadgeClick();
        QCOMPARE(model.txBoundSlice(), model.sliceById(c));
    }
};

QTEST_MAIN(TestVfoWidgetTxBadgeClick)
#include "tst_vfo_widget_tx_badge_click.moc"
