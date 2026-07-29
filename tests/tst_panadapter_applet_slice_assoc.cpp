// =================================================================
// tests/tst_panadapter_applet_slice_assoc.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic D Task 1: PanadapterApplet skeleton + slice assoc.
// =================================================================
#include <QtTest/QtTest>
#include "core/AppSettings.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/MainWindow.h"
#include "gui/SpectrumWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestPanadapterAppletSliceAssoc : public QObject {
    Q_OBJECT
private slots:
    void init() { AppSettings::instance().clear(); }

    void applet_constructs_with_pan_id()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        QCOMPARE(applet.panId(), QStringLiteral("pan-0"));
    }

    void add_slice_makes_it_active_when_no_active_yet()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        QCOMPARE(applet.activeSliceIndex(), -1);
        applet.addSlice(2);
        QCOMPARE(applet.activeSliceIndex(), 2);
    }

    void add_second_slice_does_not_change_active()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        applet.addSlice(0);
        applet.addSlice(1);
        QCOMPARE(applet.activeSliceIndex(), 0);
        QCOMPARE(applet.associatedSlices().size(), 2);
    }

    void remove_active_slice_promotes_another()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        applet.addSlice(0);
        applet.addSlice(1);
        applet.removeSlice(0);
        QCOMPARE(applet.activeSliceIndex(), 1);
    }

    void remove_last_slice_sets_active_to_minus_1()
    {
        PanadapterApplet applet(QStringLiteral("pan-0"));
        applet.addSlice(0);
        applet.removeSlice(0);
        QCOMPARE(applet.activeSliceIndex(), -1);
    }

    void persisted_extended_false_is_applied_to_the_spectrum_widget()
    {
        AppSettings::instance().setValue(
            QStringLiteral("Pan_pan-1_ExtendedView"),
            QStringLiteral("False"));

        PanadapterApplet applet(QStringLiteral("pan-1"));
        QVERIFY(!applet.extendedViewEnabled());
        QVERIFY(!applet.spectrumWidget()->extendedMode());
    }

    void spectrum_settings_round_trip_in_separate_pan_namespaces()
    {
        auto& settings = AppSettings::instance();
        settings.setValue(QStringLiteral("DisplayBandwidth"), QStringLiteral("110000"));
        settings.setValue(QStringLiteral("DisplayBandwidth_1"), QStringLiteral("220000"));

        {
            PanadapterApplet pan0(QStringLiteral("pan-0"));
            PanadapterApplet pan1(QStringLiteral("pan-1"));
            MainWindow::configureSpectrumForPanForTest(pan0.spectrumWidget(),
                                                       pan0.panId());
            MainWindow::configureSpectrumForPanForTest(pan1.spectrumWidget(),
                                                       pan1.panId());
            QCOMPARE(pan0.spectrumWidget()->bandwidth(), 110000.0);
            QCOMPARE(pan1.spectrumWidget()->bandwidth(), 220000.0);

            pan0.spectrumWidget()->setFrequencyRange(14.2e6, 130000.0);
            pan1.spectrumWidget()->setFrequencyRange(7.1e6, 260000.0);
            pan0.spectrumWidget()->saveSettings();
            pan1.spectrumWidget()->saveSettings();
        }

        PanadapterApplet restored0(QStringLiteral("pan-0"));
        PanadapterApplet restored1(QStringLiteral("pan-1"));
        MainWindow::configureSpectrumForPanForTest(restored0.spectrumWidget(),
                                                   restored0.panId());
        MainWindow::configureSpectrumForPanForTest(restored1.spectrumWidget(),
                                                   restored1.panId());
        QCOMPARE(restored0.spectrumWidget()->bandwidth(), 130000.0);
        QCOMPARE(restored1.spectrumWidget()->bandwidth(), 260000.0);
    }

    void pan_one_wideband_toggle_changes_only_pan_ones_slice()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        SliceModel* first = model.sliceById(model.addSlice(QStringLiteral("pan-0")));
        SliceModel* second = model.sliceById(model.addSlice(QStringLiteral("pan-1")));
        QVERIFY(first);
        QVERIFY(second);

        PanadapterStack stack;
        PanadapterApplet* pan0 = stack.panadapter(QStringLiteral("pan-0"));
        PanadapterApplet* pan1 = stack.addPanadapter(QStringLiteral("pan-1"));
        QVERIFY(pan0);
        QVERIFY(pan1);
        pan0->addSlice(first->sliceIndex());
        pan1->addSlice(second->sliceIndex());

        MainWindow::wireWidebandExtensionForTest(
            pan1->spectrumWidget(), &model, &stack, pan1->panId());
        pan1->spectrumWidget()->setExtendedMode(false);
        pan1->spectrumWidget()->setExtendedMode(true);

        QVERIFY(second->widebandExtensionRequested());
        QVERIFY(!first->widebandExtensionRequested());
    }
};

QTEST_MAIN(TestPanadapterAppletSliceAssoc)
#include "tst_panadapter_applet_slice_assoc.moc"
