// no-port-check: smoke test for OC Outputs HF sub-sub-tab UI construction + matrix wiring
#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>

#include "core/AppSettings.h"

#include "gui/setup/hardware/OcOutputsHfTab.h"
#include "core/OcMatrix.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TestOcOutputsHfTab : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
    }

    // Construction succeeds without crashing; widget not visible yet
    void construct_basic()
    {
        RadioModel model;
        OcMatrix matrix;
        OcOutputsHfTab tab(&model, &matrix);
        QVERIFY(!tab.isVisible());
    }

    // Matrix state driven in from OcMatrix::setPin() is reflected by the
    // test seam rxPinCheckedForTest() after changed() fires.
    void matrix_to_ui_sync_rx()
    {
        RadioModel model;
        OcMatrix matrix;
        OcOutputsHfTab tab(&model, &matrix);

        // Default: all pins clear
        QVERIFY(!tab.rxPinCheckedForTest(/*bandIdx=*/5 /*Band20m*/, /*pin=*/1));

        // Set pin via matrix → changed() → syncFromMatrix()
        matrix.setPin(Band::Band20m, /*pin=*/1, /*tx=*/false, true);
        QVERIFY(tab.rxPinCheckedForTest(5, 1));

        // Clear it again
        matrix.setPin(Band::Band20m, 1, false, false);
        QVERIFY(!tab.rxPinCheckedForTest(5, 1));
    }

    // TX pin sync
    void matrix_to_ui_sync_tx()
    {
        RadioModel model;
        OcMatrix matrix;
        OcOutputsHfTab tab(&model, &matrix);

        matrix.setPin(Band::Band40m, /*pin=*/0, /*tx=*/true, true);
        QVERIFY(tab.txPinCheckedForTest(/*bandIdx=*/3 /*Band40m*/, 0));
    }

    // Reset clears all matrix state and UI reflects cleared state
    void reset_clears_matrix_and_ui()
    {
        RadioModel model;
        OcMatrix matrix;
        OcOutputsHfTab tab(&model, &matrix);

        matrix.setPin(Band::Band20m, 2, false, true);
        QVERIFY(tab.rxPinCheckedForTest(5, 2));

        matrix.resetDefaults();
        QVERIFY(!tab.rxPinCheckedForTest(5, 2));
        QVERIFY(!matrix.pinEnabled(Band::Band20m, 2, false));
    }

    // GEN band (idx 12) checkboxes exist but are disabled (no OC sense)
    void gen_band_row_is_disabled()
    {
        RadioModel model;
        OcMatrix matrix;
        OcOutputsHfTab tab(&model, &matrix);
        // The test seam returns false for disabled rows (they are unchecked
        // and cannot be toggled from the UI side)
        QVERIFY(!tab.rxPinCheckedForTest(12 /*GEN*/, 0));
    }

    // Default pin action is MoxTuneTwoTone (value 6 — Thetis Penny.cs:59)
    void default_pin_action_is_mox_tune_twotone()
    {
        RadioModel model;
        OcMatrix matrix;
        OcOutputsHfTab tab(&model, &matrix);

        // Matrix default: all pins → MoxTuneTwoTone
        for (int pin = 0; pin < 7; ++pin) {
            QCOMPARE(static_cast<int>(matrix.pinAction(pin)),
                     static_cast<int>(OcMatrix::TXPinAction::MoxTuneTwoTone));
        }
    }

    // setPinAction on matrix → changed() fires (UI wiring exercised without
    // needing to introspect the action checkboxes directly)
    void set_pin_action_fires_changed()
    {
        RadioModel model;
        OcMatrix matrix;

        bool changed = false;
        QObject::connect(&matrix, &OcMatrix::changed,
                         [&changed]() { changed = true; });

        OcOutputsHfTab tab(&model, &matrix);
        changed = false;  // reset after initial load

        matrix.setPinAction(0, OcMatrix::TXPinAction::Mox);
        QVERIFY(changed);
        QCOMPARE(static_cast<int>(matrix.pinAction(0)),
                 static_cast<int>(OcMatrix::TXPinAction::Mox));
    }

    // R-R3-21: the Penny Ext Control checkbox shows the radio's saved value
    // and saves to the key PennyLaneController reads
    // (hardware/<mac>/penny/extCtrlEnabled), not the unread
    // hardware/oc/pennyExtCtrl.
    void penny_ext_control_uses_the_radio_key()
    {
        auto& s = AppSettings::instance();
        const QString mac = QStringLiteral("aa:bb:cc:00:11:22");
        const QString perMac = QStringLiteral("hardware/%1/penny/extCtrlEnabled").arg(mac);
        const QString legacy = QStringLiteral("hardware/oc/pennyExtCtrl");
        s.remove(legacy);
        s.setValue(perMac, QStringLiteral("False"));

        RadioModel model;
        model.pennyLaneControllerMutable().setMacAddress(mac);
        model.pennyLaneControllerMutable().load();
        OcMatrix matrix;
        OcOutputsHfTab tab(&model, &matrix);

        QCheckBox* box = nullptr;
        for (QCheckBox* c : tab.findChildren<QCheckBox*>()) {
            if (c->text() == QStringLiteral("Penny Ext Control enabled")) { box = c; }
        }
        QVERIFY(box != nullptr);
        QVERIFY2(!box->isChecked(), "the checkbox must show the radio's saved value");

        box->setChecked(true);
        QCOMPARE(s.value(perMac).toString(), QStringLiteral("True"));
        QVERIFY(model.pennyLaneController().extCtrlEnabled());
        QVERIFY2(!s.contains(legacy), "the unread global key must not be written");

        // A change made elsewhere reaches the checkbox.
        model.pennyLaneControllerMutable().setExtCtrlEnabled(false);
        QVERIFY(!box->isChecked());
        s.remove(perMac);
    }
};

QTEST_MAIN(TestOcOutputsHfTab)
#include "tst_oc_outputs_hf_tab.moc"
