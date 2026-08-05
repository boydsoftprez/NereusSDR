// =================================================================
// tests/tst_phone_applet_vax_toggle.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test file. No Thetis port at this layer.
//
// Verifies the PhoneCwApplet VAX button:
//   click_setsVax              - left-click toggles MicSource to Vax
//   secondClick_restoresPrevious - second click reverts to previous source
//   modelChange_syncsButton    - external micSource change updates checked state
//   rightClick_emitsSetupReq   - right-click emits openSetupRequested("Audio", "TX Input")
//   nyiMark_absent             - the NyiOverlay mark removed
// =================================================================
//
// Modification history (NereusSDR):
//   2026-05-10 - Original test for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude
//                 Code.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>
#include <QPushButton>
#include <QSignalSpy>
#include <QApplication>

#include "gui/applets/PhoneCwApplet.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {
QPushButton* findVaxButton(PhoneCwApplet* applet)
{
    // Locate by accessibleName set in PhoneCwApplet::buildUI.
    const auto buttons = applet->findChildren<QPushButton*>();
    for (auto* b : buttons) {
        if (b->accessibleName() == QStringLiteral("VAX digital audio")) {
            return b;
        }
    }
    return nullptr;
}
}

class TstPhoneAppletVaxToggle : public QObject {
    Q_OBJECT

private slots:

    void click_setsVax()
    {
        RadioModel model;
        PhoneCwApplet applet(&model);
        auto* btn = findVaxButton(&applet);
        QVERIFY(btn != nullptr);

        QVERIFY(!btn->isChecked());
        btn->click();
        QCOMPARE(model.transmitModel().micSource(), MicSource::Vax);
        QVERIFY(btn->isChecked());
    }

    void secondClick_restoresPrevious()
    {
        RadioModel model;
        model.transmitModel().setMicSource(MicSource::Radio);

        PhoneCwApplet applet(&model);
        auto* btn = findVaxButton(&applet);
        QVERIFY(btn != nullptr);

        btn->click();  // -> Vax
        QCOMPARE(model.transmitModel().micSource(), MicSource::Vax);

        btn->click();  // -> Radio (previous)
        QCOMPARE(model.transmitModel().micSource(), MicSource::Radio);
        QVERIFY(!btn->isChecked());
    }

    void modelChange_syncsButton()
    {
        RadioModel model;
        PhoneCwApplet applet(&model);
        auto* btn = findVaxButton(&applet);
        QVERIFY(btn != nullptr);

        QVERIFY(!btn->isChecked());
        model.transmitModel().setMicSource(MicSource::Vax);
        QVERIFY(btn->isChecked());
        model.transmitModel().setMicSource(MicSource::Pc);
        QVERIFY(!btn->isChecked());
    }

    void rightClick_emitsSetupReq()
    {
        RadioModel model;
        PhoneCwApplet applet(&model);
        auto* btn = findVaxButton(&applet);
        QVERIFY(btn != nullptr);

        QSignalSpy spy(&applet, &PhoneCwApplet::openSetupRequested);

        // Trigger the customContextMenuRequested handler directly.
        emit btn->customContextMenuRequested(QPoint(0, 0));

        QCOMPARE(spy.count(), 1);
        const auto args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("Audio"));
        QCOMPARE(args.at(1).toString(), QStringLiteral("TX Input"));
    }

    void nyiMark_absent()
    {
        RadioModel model;
        PhoneCwApplet applet(&model);
        auto* btn = findVaxButton(&applet);
        QVERIFY(btn != nullptr);
        // NyiOverlay sets the property "nyiOverlay" on marked widgets;
        // the absence of the overlay is sufficient verification.
        QVERIFY(btn->property("nyiOverlay").isNull()
                || !btn->property("nyiOverlay").toBool());
    }
};

QTEST_MAIN(TstPhoneAppletVaxToggle)
#include "tst_phone_applet_vax_toggle.moc"
