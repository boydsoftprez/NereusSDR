// =================================================================
// tests/tst_rf2ks_applet_context_menu.cpp  (NereusSDR-native)
// =================================================================
//   2026-05-24  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
// =================================================================
#include <QtTest/QtTest>
#include <QMenu>
#include "gui/applets/Rf2ksApplet.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class Rf2ksAppletContextMenuTest : public QObject {
    Q_OBJECT
private slots:
    void menuHasFourItems();
    void openAdvancedEmitsNavigation();
    void disconnectEmitsConnectionToggle();
    void copyDiagnosticsEmitsSignal();
};

// Plan adaptation: added a.setConnectedState(true) so action[2] reads
// "Disconnect" (the connected branch) rather than "Reconnect" (the
// disconnected branch). The disconnectEmitsConnectionToggle test also
// sets connected=true for the same reason.  The plan text was
// self-inconsistent on this point; option (b) from the task notes
// was chosen.
void Rf2ksAppletContextMenuTest::menuHasFourItems() {
    RadioModel m;
    Rf2ksApplet a(&m);
    a.setConnectedState(true);
    QMenu* menu = a.buildContextMenuForTesting();
    const auto actions = menu->actions();
    QCOMPARE(actions.size(), 5);   // 4 items + 1 separator
    QCOMPARE(actions[0]->text(), QString("Open RF-Kit Advanced..."));
    QVERIFY(actions[1]->isSeparator());
    QCOMPARE(actions[2]->text(), QString("Disconnect"));
    QCOMPARE(actions[3]->text(), QString("Reconnect"));
    QCOMPARE(actions[4]->text(), QString("Copy diagnostics to clipboard"));
    delete menu;
}

void Rf2ksAppletContextMenuTest::openAdvancedEmitsNavigation() {
    RadioModel m;
    Rf2ksApplet a(&m);
    QSignalSpy spy(&a, &Rf2ksApplet::navigationRequested);
    QMenu* menu = a.buildContextMenuForTesting();
    menu->actions()[0]->trigger();
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(), QString("rfKit"));
    delete menu;
}

void Rf2ksAppletContextMenuTest::disconnectEmitsConnectionToggle() {
    RadioModel m;
    Rf2ksApplet a(&m);
    a.setConnectedState(true);
    QSignalSpy spy(&a, &Rf2ksApplet::connectionToggleRequested);
    QMenu* menu = a.buildContextMenuForTesting();
    menu->actions()[2]->trigger();   // Disconnect
    QCOMPARE(spy.count(), 1);
    delete menu;
}

void Rf2ksAppletContextMenuTest::copyDiagnosticsEmitsSignal() {
    RadioModel m;
    Rf2ksApplet a(&m);
    QSignalSpy spy(&a, &Rf2ksApplet::diagnosticsCopyRequested);
    QMenu* menu = a.buildContextMenuForTesting();
    menu->actions()[4]->trigger();
    QCOMPARE(spy.count(), 1);
    delete menu;
}

QTEST_MAIN(Rf2ksAppletContextMenuTest)
#include "tst_rf2ks_applet_context_menu.moc"
