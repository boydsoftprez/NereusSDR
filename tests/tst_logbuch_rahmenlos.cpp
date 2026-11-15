// Das Logbuch traegt denselben Rahmen wie jedes andere Longpath-Fenster.
//
// Betreiber am 2026-10-06, mit Bildschirmfoto: „die fensterecken sind
// rund, alle anderen widget sind doch eckig?!?!"
//
// Er hatte recht, und der Grund war strukturell: das Logbuch war das
// EINZIGE Fenster, das noch den Rahmen des Betriebssystems trug. Rotor,
// abgeloeste Applets, der schwebende Panadapter und der Seitenbereich
// laufen seit dem 2026-08-20 rahmenlos mit `WindowChrome` -- und damit
// eckig. macOS rundet jedes Fenster mit Systemrahmen; dagegen hilft kein
// Stilblatt, nur der Verzicht auf den Rahmen.
//
// Dieser Stand haelt die drei Teile fest, die ein Longpath-Fenster
// ausmachen (dieselben, die `ToolWindow` installiert):
//   * rahmenlos,
//   * eine eigene Titelleiste,
//   * und das × schliesst wirklich.
//
// Offscreen, damit beim Pruefen kein Fenster aufgeht und niemand es fuer
// Longpath haelt (feedback-pruefprogramme-offscreen).

#include "gui/LogbookWindow.h"
#include "gui/WindowChrome.h"

#include <QtTest>
#include <QTemporaryDir>

using Longpath::LogbookWindow;
using Longpath::WindowTitleBar;

class TstLogbuchRahmenlos : public QObject
{
    Q_OBJECT

private slots:
    void keinSystemrahmenMehr();
    void eineEigeneTitelleiste();
    void dasKreuzSchliesst();
    void aufDerPalettenebene();

private:
    QTemporaryDir m_dir;
    QString pfad() { return m_dir.filePath(QStringLiteral("logbook.adi")); }
};

void TstLogbuchRahmenlos::keinSystemrahmenMehr()
{
    // DER Punkt: mit Systemrahmen rundet macOS die Ecken, und dagegen
    // hilft kein Stilblatt.
    LogbookWindow w(pfad());
    QVERIFY2(w.windowFlags().testFlag(Qt::FramelessWindowHint),
             "Das Logbuch traegt wieder den Rahmen des Betriebssystems -- "
             "damit ist es als einziges Fenster rund.");
}

// ── Und vor den schwebenden Fenstern, nicht dahinter (2026-11-15) ───
//
// Betreiber: „es muss immer das neu hinzugefuegte im vordergrund
// sein!!!!" Das Logbuch war ein gewoehnlicher QDialog und lag damit
// unter den Paletten (Qt::Tool = NSPanel auf hoeherer Ebene); raise()
// hebt nur innerhalb der eigenen Ebene. Dritter Fall derselben Sache
// nach Antennenfenster (2026-09-01) und Setup-Dialog (2026-09-17).
//
// Die Fensterebene selbst ist Cocoa-Sache und offscreen ein No-Op --
// die FLAGGE ist es nicht, und sie ist die Ursache.
void TstLogbuchRahmenlos::aufDerPalettenebene()
{
    LogbookWindow w(pfad());
    QVERIFY2(w.windowFlags().testFlag(Qt::Tool),
             "Das Logbuch ist wieder ein gewoehnliches Fenster -- damit "
             "geht es hinter den schwebenden Panels auf.");
    // Und der Rahmen bleibt weg: die zwei gehoeren zusammen.
    QVERIFY(w.windowFlags().testFlag(Qt::FramelessWindowHint));
}

void TstLogbuchRahmenlos::eineEigeneTitelleiste()
{
    // Rahmenlos allein genuegt nicht: ohne eigene Leiste liesse sich das
    // Fenster nicht mehr verschieben und nicht mehr schliessen.
    LogbookWindow w(pfad());
    const auto leisten = w.findChildren<WindowTitleBar*>();
    QCOMPARE(leisten.size(), 1);
    // Sie sitzt GANZ OBEN und buendig: ein eingerueckter Streifen sieht
    // aus wie ein Fehler, nicht wie ein Fensterkopf.
    w.resize(900, 500);
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w, 2000));
    const QPoint oben = leisten.first()->mapTo(&w, QPoint(0, 0));
    QCOMPARE(oben, QPoint(0, 0));
    QCOMPARE(leisten.first()->width(), w.width());
}

void TstLogbuchRahmenlos::dasKreuzSchliesst()
{
    // Das Logbuch gehoert in kein Dock. Ein × , das andockt statt zu
    // schliessen, liesse das Fenster offen stehen und niemand wuesste,
    // warum.
    LogbookWindow w(pfad());
    w.show();
    QVERIFY(QTest::qWaitForWindowExposed(&w, 2000));
    QVERIFY(w.isVisible());

    WindowTitleBar* leiste = w.findChild<WindowTitleBar*>();
    QVERIFY(leiste);
    emit leiste->closeRequested();
    QTRY_VERIFY_WITH_TIMEOUT(!w.isVisible(), 2000);
}

QTEST_MAIN(TstLogbuchRahmenlos)
#include "tst_logbuch_rahmenlos.moc"
