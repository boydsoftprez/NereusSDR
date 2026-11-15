// =================================================================
// tests/tst_auxiliary_window_leveler.cpp  (Longpath)
// =================================================================
//
// Longpath-original test. AuxiliaryWindowLeveler (Betreiber 2026-09-21:
// "wenn ich ein Channel Strip oder etwas anderes aufmache, ist es im
// Hintergrund"): welche Fenster der App-weite Filter anfasst und welche
// nicht. Die eigentliche Fensterebene ist Cocoa-Sache und unter der
// Offscreen-Plattform ein No-Op -- hier zaehlt die Auswahl.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QDialog>
#include <QMessageBox>
#include <QMenu>

#include "gui/AuxiliaryWindowLeveler.h"
#include "FensterFrist.h"

using namespace Longpath;

class TstAuxiliaryWindowLeveler : public QObject {
    Q_OBJECT

private slots:
    void dialogsAndSecondaryWindowsAreLifted()
    {
        AuxiliaryWindowLeveler leveler;
        qApp->installEventFilter(&leveler);

        QWidget main;
        main.show();
        QVERIFY(QTest::qWaitForWindowExposed(&main, kFensterFrist));
        // Das Hauptfenster selbst hat kein Elternteil -- nicht angefasst.
        QVERIFY(!AuxiliaryWindowLeveler::isAuxiliaryWindow(&main));
        QVERIFY(!leveler.trackedForTest().contains(&main));

        // Ein modeless QDialog aus dem Hauptfenster heraus: der Fall
        // Channel Strip / Logbuch / Setup.
        QDialog dialog(&main);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog, kFensterFrist));
        QVERIFY(AuxiliaryWindowLeveler::isAuxiliaryWindow(&dialog));
        QVERIFY(leveler.trackedForTest().contains(&dialog));

        // Ein gewoehnliches Qt::Window mit Elternteil ebenso.
        QWidget window(&main, Qt::Window);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window, kFensterFrist));
        QVERIFY(leveler.trackedForTest().contains(&window));

        // Ein QMessageBox mit Elternteil auch -- der "eingefrorene"
        // Fall, wenn die Meldung hinter den Paletten steht.
        QMessageBox box(&main);
        // Nicht nativ: auf macOS mit Qt 6.11 wird ein nativer QMessageBox
        // beim show() zu [NSAlert runModal] -- eine modale Schleife, die
        // erst endet, wenn jemand klickt. Hier klickt niemand; der Test
        // hing 300 s (2026-09-24, mit `sample` belegt). Das Programm
        // zeigt seine nicht blockierenden Meldungen ebenfalls nicht nativ.
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        box.setOption(QMessageBox::Option::DontUseNativeDialog);
#endif
        box.show();
        QVERIFY(QTest::qWaitForWindowExposed(&box, kFensterFrist));
        QVERIFY(leveler.trackedForTest().contains(&box));

        qApp->removeEventFilter(&leveler);
    }

    // ── Die Paletten gehoeren jetzt DAZU (2026-11-15) ───────────────
    //
    // Hier stand „palettesPopupsAndStayOnTopAreLeftAlone", und die
    // Paletten standen darin bei den Ausgenommenen: sie regelten das
    // Verlassen der App selbst, mit `hidesOnDeactivate` -- sie
    // versteckten sich.
    //
    // Betreiber 2026-11-15: „und immer, wenn ich ein anderes fenster
    // ausserhalb von longpath klicke, wird longpath schwarz." Genau
    // dieses Verstecken. Jetzt stuft der Filter auch sie herunter,
    // statt sie verschwinden zu lassen; `hidesOnDeactivate` ist auf
    // NO. Die uebrigen Ausnahmen (Popups, „bleibt oben", Kinder)
    // gelten unveraendert -- darum stehen sie weiter in demselben
    // Fall.
    void palettenWerdenGestuftPopupsNicht()
    {
        AuxiliaryWindowLeveler leveler;
        qApp->installEventFilter(&leveler);

        QWidget main;
        main.show();
        QVERIFY(QTest::qWaitForWindowExposed(&main, kFensterFrist));

        // Eine Palette (Qt::Tool) ist kein „Nebenfenster" im alten
        // Sinn -- aber der Filter nimmt sie jetzt in die Hand.
        QWidget tool(&main, Qt::Tool);
        tool.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tool, kFensterFrist));
        QVERIFY(!AuxiliaryWindowLeveler::isAuxiliaryWindow(&tool));
        QVERIFY(AuxiliaryWindowLeveler::istPalette(&tool));
        QVERIFY(AuxiliaryWindowLeveler::wirdGestuft(&tool));
        QVERIFY(leveler.trackedForTest().contains(&tool));

        // Eine Palette OHNE Elternteil ist keine: sie gehoert zu
        // keinem Fenster, aus dem heraus sie aufgegangen waere.
        QWidget frei(nullptr, Qt::Tool);
        QVERIFY(!AuxiliaryWindowLeveler::istPalette(&frei));

        // Menues sind Popups.
        QMenu menu(&main);
        menu.addAction(QStringLiteral("x"));
        menu.popup(QPoint(10, 10));
        QVERIFY(!AuxiliaryWindowLeveler::wirdGestuft(&menu));
        QVERIFY(!leveler.trackedForTest().contains(&menu));
        menu.close();

        // "Bleibt oben" liegt bei Qt ohnehin ueber den Paletten.
        QDialog onTop(&main, Qt::WindowStaysOnTopHint);
        onTop.show();
        QVERIFY(!AuxiliaryWindowLeveler::wirdGestuft(&onTop));
        QVERIFY(!leveler.trackedForTest().contains(&onTop));

        // Auch eine Palette, die „bleibt oben" sagt, bleibt oben.
        QWidget toolOben(&main, Qt::Tool | Qt::WindowStaysOnTopHint);
        QVERIFY(!AuxiliaryWindowLeveler::istPalette(&toolOben));

        // Ein Kind-Widget im Fenster ist kein Fenster.
        QWidget child(&main);
        child.show();
        QVERIFY(!AuxiliaryWindowLeveler::wirdGestuft(&child));

        qApp->removeEventFilter(&leveler);
    }

    void aDestroyedWindowLeavesTheSet()
    {
        AuxiliaryWindowLeveler leveler;
        qApp->installEventFilter(&leveler);
        QWidget main;
        main.show();
        QVERIFY(QTest::qWaitForWindowExposed(&main, kFensterFrist));
        auto* dialog = new QDialog(&main);
        dialog->show();
        QVERIFY(QTest::qWaitForWindowExposed(dialog, kFensterFrist));
        QVERIFY(leveler.trackedForTest().contains(dialog));
        delete dialog;
        QVERIFY(leveler.trackedForTest().isEmpty());
        qApp->removeEventFilter(&leveler);
    }

    // Die Reihenfolge, zuletzt benutztes Fenster zuerst (2026-09-26): beim
    // Zurueckkommen in die App stellt macOS alle Paletten vor das Logbuch,
    // das der Betreiber vor sich hatte; der Leveler stellt diese Liste
    // wieder her. Aktivieren, Anklicken (auch eines Kindes) und Zeigen
    // zaehlen, Aufklapper nicht.
    void theRecentOrderFollowsWhatTheOperatorTouches()
    {
        AuxiliaryWindowLeveler leveler;
        qApp->installEventFilter(&leveler);
        QWidget main;
        main.show();
        QVERIFY(QTest::qWaitForWindowExposed(&main, kFensterFrist));
        auto* logbook = new QDialog(&main);
        auto* child = new QWidget(logbook);
        logbook->show();
        QVERIFY(QTest::qWaitForWindowExposed(logbook, kFensterFrist));
        auto* palette = new QWidget(&main, Qt::Tool);
        palette->show();
        QVERIFY(QTest::qWaitForWindowExposed(palette, kFensterFrist));
        // Zuletzt gezeigt: die Palette vorne.
        QCOMPARE(leveler.recentOrderForTest().value(0), palette);

        // Klick in ein Kind des Logbuchs: das Logbuch nach vorne.
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(1, 1), QPointF(1, 1),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(child, &press);
        QCOMPARE(leveler.recentOrderForTest().value(0), static_cast<QWidget*>(logbook));
        QCOMPARE(leveler.recentOrderForTest().value(1), palette);

        // Aktivierung der Palette: sie wieder vorne, jedes Fenster nur einmal.
        QEvent act(QEvent::WindowActivate);
        QCoreApplication::sendEvent(palette, &act);
        QCOMPARE(leveler.recentOrderForTest().value(0), palette);
        QCOMPARE(leveler.recentOrderForTest().count(palette), 1);

        // Ein Aufklapper (Qt::Popup, wie Menues und Auswahllisten) ist
        // keine Lage, die sich jemand merkt.
        QWidget popup(&main, Qt::Popup);
        popup.resize(40, 40);
        popup.show();
        QVERIFY(QTest::qWaitForWindowExposed(&popup, kFensterFrist));
        QVERIFY(!leveler.recentOrderForTest().contains(&popup));
        QCOMPARE(leveler.recentOrderForTest().value(0), palette);
        popup.hide();

        delete palette;
        delete logbook;
        QVERIFY(!leveler.recentOrderForTest().contains(palette));
        qApp->removeEventFilter(&leveler);
    }
};

QTEST_MAIN(TstAuxiliaryWindowLeveler)
#include "tst_auxiliary_window_leveler.moc"
