// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_kombi_feld.cpp  (Longpath)
// =================================================================
//
// Betreiber, 2026-11-01: „in dieses leere widget sollte man generell
// unterschiedlich widget reinziehen können" -- zuerst am Beispiel
// RX und TX, dann allgemein.
//
// Der Kern ist, dasz ein aufgenommenes Panel DASSELBE Objekt bleibt:
// `RxApplet` und `TxApplet` haengen an einem halben Dutzend Dingen,
// die MainWindow ihnen von auszen reicht. Wer sie neu baut, baut die
// Verdrahtung neu -- und vergiszt eine.
// =================================================================

#include <QtTest/QtTest>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "gui/applets/KombiApplet.h"

using namespace Longpath;

namespace {

// Ein Panel, das nichts kann auszer sich melden. Genug: geprueft wird
// der Behaelter, nicht sein Inhalt.
class Probepanel : public AppletWidget
{
public:
    explicit Probepanel(const QString& id, const QString& titel)
        : AppletWidget(nullptr, nullptr), m_id(id), m_titel(titel) {}

    QString appletId() const override    { return m_id; }
    QString appletTitle() const override { return m_titel; }
    void    syncFromModel() override     { ++abgeglichen; }

    int abgeglichen{0};

private:
    QString m_id;
    QString m_titel;
};

}  // namespace

class TstKombiFeld : public QObject
{
    Q_OBJECT

private slots:
    // Leer ist leer -- und sagt es.
    void leerIstLeer()
    {
        KombiApplet k(nullptr);
        QVERIFY(k.istLeer());
        QVERIFY(k.seiten().isEmpty());
        QVERIFY(!k.hasExtendedSettings());
    }

    // ── Aufnehmen ist ein Elternwechsel, kein Neubau ────────────────
    //
    // Der wichtigste Fall ueberhaupt: dasselbe Objekt, kein zweites.
    void aufnehmenBehaeltDasselbeObjekt()
    {
        KombiApplet k(nullptr);
        auto* rx = new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX"));
        k.nimmAuf(rx);
        QCOMPARE(k.seiten().size(), 1);
        QCOMPARE(k.seiten().first(), static_cast<QWidget*>(rx));
        QVERIFY(!k.istLeer());
        // Und es haengt wirklich im Behaelter, nicht irgendwo.
        QVERIFY(rx->parentWidget() != nullptr);
    }

    // Zweimal dasselbe aufnehmen ist kein zweiter Reiter.
    void zweimalDasselbeIstEinmal()
    {
        KombiApplet k(nullptr);
        auto* rx = new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX"));
        k.nimmAuf(rx);
        k.nimmAuf(rx);
        QCOMPARE(k.seiten().size(), 1);
    }

    // ── Der Name bleibt, der Inhalt wechselt ────────────────────────
    //
    // Betreiber, 2026-11-01: „das RX/TX (combined) panel sollte
    // generell combined heißen, sofern man hier dann alle widget
    // reinziehen kann!" Ein Name, der mit dem Inhalt wandert, haette
    // das Feld jedes Mal zu etwas anderem gemacht.
    void derNameStehtNurBeimLeerenFeld()
    {
        KombiApplet k(nullptr);
        // Leer: der Name musz dastehen, sonst weisz niemand, was das
        // Feld ist.
        QCOMPARE(k.appletTitle(), QStringLiteral("Combined"));
        k.nimmAuf(new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX")));
        k.nimmAuf(new Probepanel(QStringLiteral("Tx"), QStringLiteral("TX")));
        // Voll: kein ausgeschriebener Name mehr. Betreiber,
        // 2026-11-01: „also Combined muss ja nicht ausgeschrieben
        // sein" -- das Wort sagt nichts, was die Pillen nicht zeigen,
        // und nimmt ihnen in einer schmalen Spalte den Platz.
        QVERIFY2(k.appletTitle().isEmpty(),
                 "der ausgeschriebene Name nimmt den Pillen den Platz");
    }

    // Umschalten zeigt die andere Seite, und die Wahl bleibt in den
    // Grenzen.
    void umschaltenBleibtInDenGrenzen()
    {
        KombiApplet k(nullptr);
        k.nimmAuf(new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX")));
        k.nimmAuf(new Probepanel(QStringLiteral("Tx"), QStringLiteral("TX")));
        k.zeigeSeite(1);
        QCOMPARE(k.seite(), 1);
        k.zeigeSeite(0);
        QCOMPARE(k.seite(), 0);
        k.zeigeSeite(7);
        QCOMPARE(k.seite(), 1);
        k.zeigeSeite(-3);
        QCOMPARE(k.seite(), 0);
    }

    // ── Herausloesen gibt es unversehrt zurueck ─────────────────────
    //
    // Ohne Elternteil, aber am Leben: der Aufrufer legt es in ein
    // eigenes Feld. Wer es hier abbaute, haette alle Verbindungen
    // mitgenommen.
    void herausloesenGibtDasObjektZurueck()
    {
        KombiApplet k(nullptr);
        auto* rx = new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX"));
        auto* tx = new Probepanel(QStringLiteral("Tx"), QStringLiteral("TX"));
        k.nimmAuf(rx);
        k.nimmAuf(tx);

        QSignalSpy spion(&k, &KombiApplet::herausgeloest);
        k.gibFrei(rx);
        QCOMPARE(spion.count(), 1);
        QCOMPARE(spion.first().first().value<QWidget*>(),
                 static_cast<QWidget*>(rx));
        QCOMPARE(k.seiten().size(), 1);
        QCOMPARE(k.seiten().first(), static_cast<QWidget*>(tx));
        QVERIFY2(rx->parentWidget() == nullptr,
                 "herausgeloest heiszt ohne Elternteil -- sonst haengt es "
                 "noch im Behaelter und bekommt nie ein eigenes Feld");
        delete rx;
    }

    // Alle hergeben leert das Feld und gibt die Reihenfolge zurueck.
    void alleHergebenLeertDasFeld()
    {
        KombiApplet k(nullptr);
        auto* rx = new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX"));
        auto* tx = new Probepanel(QStringLiteral("Tx"), QStringLiteral("TX"));
        k.nimmAuf(rx);
        k.nimmAuf(tx);
        const QList<QWidget*> raus = k.gibAlleFrei();
        QCOMPARE(raus.size(), 2);
        QCOMPARE(raus.at(0), static_cast<QWidget*>(rx));
        QCOMPARE(raus.at(1), static_cast<QWidget*>(tx));
        QVERIFY(k.istLeer());
        delete rx;
        delete tx;
    }

    // Der Abgleich geht an ALLE, nicht nur an die sichtbare Seite:
    // wer umschaltet, soll keinen veralteten Stand vorfinden.
    void derAbgleichGehtAnAlleSeiten()
    {
        KombiApplet k(nullptr);
        auto* rx = new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX"));
        auto* tx = new Probepanel(QStringLiteral("Tx"), QStringLiteral("TX"));
        k.nimmAuf(rx);
        k.nimmAuf(tx);
        k.zeigeSeite(0);
        k.syncFromModel();
        QCOMPARE(rx->abgeglichen, 1);
        QVERIFY2(tx->abgeglichen == 1,
                 "die unsichtbare Seite wurde nicht abgeglichen — beim "
                 "Umschalten stuende dort ein alter Stand");
    }

    // ── Ueber die Fenstergrenze hinweg ──────────────────────────────
    //
    // Betreiber, 2026-11-01: „kann das rx panel nicht reinziehen".
    // Sein RX sitzt in der SPALTE (Fenster „RX1 Main Panel"), das
    // Kombi-Feld schwebt DANEBEN als eigenes Fenster. Beide
    // Ablegewege suchten vorher nur in ihrem eigenen Gehaeuse und
    // fanden ueber die Fenstergrenze nichts.
    //
    // `unter()` kennt keine Gehaeuse: es vergleicht Rechtecke auf dem
    // Schirm. Dieser Fall stellt zwei getrennte Fenster auf und
    // fragt nach Stellen in beiden.
    void unterFindetUeberFenstergrenzen()
    {
        QWidget fensterA;
        fensterA.setGeometry(40, 40, 200, 120);
        auto* a = new KombiApplet(nullptr, &fensterA);
        a->setGeometry(0, 0, 200, 120);
        fensterA.show();

        QWidget fensterB;
        fensterB.setGeometry(400, 300, 200, 120);
        auto* b = new KombiApplet(nullptr, &fensterB);
        b->setGeometry(0, 0, 200, 120);
        fensterB.show();
        QTest::qWait(50);

        QCOMPARE(KombiApplet::unter(a->mapToGlobal(QPoint(10, 10))), a);
        QCOMPARE(KombiApplet::unter(b->mapToGlobal(QPoint(10, 10))), b);
        QVERIFY2(KombiApplet::unter(QPoint(-5000, -5000)) == nullptr,
                 "irgendwo im Nirgendwo darf kein Feld liegen");
    }

    // Und beide Ablegewege muessen DIESE Liste fragen -- zwei Suchen
    // an zwei Stellen laufen frueher oder spaeter auseinander.
    void beideWegeFragenDieselbeListe()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }
        const auto lies = [&](const QString& rel) {
            QFile f(QString::fromLocal8Bit(root) + rel);
            return f.open(QIODevice::ReadOnly)
                       ? QString::fromUtf8(f.readAll()) : QString();
        };
        for (const QString& datei : {QStringLiteral("/src/gui/applets/AppletPanelWidget.cpp"),
                                     QStringLiteral("/src/gui/MainWindow.cpp")}) {
            const QString src = lies(datei);
            // Mit Klammer. Ohne sie fand dieser Waechter am
            // 2026-11-01 seinen eigenen KOMMENTAR („`KombiApplet::unter`
            // kennt keine Gehaeuse") und blieb gruen, obwohl der Ruf
            // entfernt war. Ein Waechter, der nicht faellt, waecht
            // nicht.
            QVERIFY2(src.contains(QStringLiteral("KombiApplet::unter(")),
                     qPrintable(datei + QStringLiteral(
                         " sucht das Kombi-Feld wieder selbst — dann findet "
                         "es nichts, was in einem anderen Fenster liegt")));
        }
    }

    // ── Ein Panel hat ZWEI Gehaeuse ─────────────────────────────────
    //
    // Betreiber, 2026-11-01: „nimmt es nicht auf". Der erste Versuch
    // hing nur am Ziehen in der SPALTE -- seine Panels schweben aber
    // alle. Dieselbe Lehre wie beim Zahnrad am 2026-10-31: wer nur
    // ein Gehaeuse bedient, baut etwas, das der Betreiber nicht
    // erreicht.
    //
    // Darum beide Wege festhalten: die Spalte fragt beim Loslassen,
    // worueber gezogen wurde; das schwebende Fenster meldet, wo es
    // abgelegt wurde.
    void beideGehaeuseNehmenAuf()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }
        const auto lies = [&](const QString& rel) {
            QFile f(QString::fromLocal8Bit(root) + rel);
            return f.open(QIODevice::ReadOnly)
                       ? QString::fromUtf8(f.readAll()) : QString();
        };

        const QString spalte = lies(QStringLiteral(
            "/src/gui/applets/AppletPanelWidget.cpp"));
        QVERIFY2(spalte.contains(QStringLiteral("nimmKombiAuf")),
                 "die Spalte fragt beim Loslassen nicht mehr, ob dort ein "
                 "Kombi-Feld liegt");

        const QString fenster = lies(QStringLiteral("/src/gui/MainWindow.cpp"));
        QVERIFY2(fenster.contains(QStringLiteral("AppletFloatingWindow::abgelegtBei")),
                 "ein SCHWEBENDES Panel kann nicht mehr abgelegt werden — und "
                 "genau die schweben beim Betreiber alle");
        QVERIFY2(fenster.contains(QStringLiteral("legeAufKombiFeld")),
                 "das Ablegen fuehrt ins Leere");

        // Und der Name eines Werkzeugfensters muss beim Hineinlegen
        // MITGEHEN: er haengt am Fenster, und das Fenster verschwindet.
        const int a = fenster.indexOf(QStringLiteral("ToolWindow::abgelegtBei"));
        QVERIFY2(a > 0, "ein Werkzeugfenster kann nicht abgelegt werden");
        const int e = fenster.indexOf(QStringLiteral("    });"), a);
        QVERIFY2(e > a, "der Ablegeweg sieht anders aus als erwartet");
        QVERIFY2(fenster.mid(a, e - a).contains(QStringLiteral("setWindowTitle")),
                 "der Name bleibt am verschwindenden Fenster haengen — auf "
                 "dem Reiter steht dann „Panel“");
    }

    // ── Rechtsklick fragt nach der Liste ────────────────────────────
    //
    // Betreiber, 2026-11-01: „oder man öffnet das leere combined
    // fenster, rechte moustaste und dann sieht man alles, was in das
    // fenster kommen kann und klickt es an."
    //
    // Das Feld selbst kennt die Liste nicht -- es fragt nur. Wer sie
    // kennt (das Hauptfenster), zeigt sie.
    void rechtsklickFragtNachDerListe()
    {
        KombiApplet k(nullptr);
        k.resize(200, 120);
        k.show();
        QSignalSpy spion(&k, &KombiApplet::listeGewuenscht);
        emit k.customContextMenuRequested(QPoint(10, 10));
        QCOMPARE(spion.count(), 1);

        // ── UEBERALL, nicht nur auf dem Feldkoerper ──────────────────
        //
        // Betreiber, 2026-11-01, mit Bild („rechte maustaste
        // funktioniert nicht") und dann klar: „muss immer und überall
        // funktionieren in im combined". Rollbereich, Stapel und
        // Hinweis beantworten einen Rechtsklick sonst mit einem leeren
        // Menue und schlucken ihn.
        int gefragt = 0;
        for (QWidget* teil : k.findChildren<QWidget*>()) {
            if (teil->contextMenuPolicy() == Qt::CustomContextMenu) {
                ++gefragt;
            }
        }
        QVERIFY2(gefragt >= 4,
                 qPrintable(QStringLiteral(
                     "nur %1 Stuecke im Feld fragen nach der Liste — "
                     "ein Rechtsklick landet dann irgendwo im Nichts")
                         .arg(gefragt)));

        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { return; }
        QFile f(QString::fromLocal8Bit(root)
                + QStringLiteral("/src/gui/MainWindow.cpp"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString src = QString::fromUtf8(f.readAll());
        QVERIFY2(src.contains(QStringLiteral("KombiApplet::listeGewuenscht")),
                 "niemand zeigt die Liste — der Rechtsklick fuehrt ins Leere");
        QVERIFY2(src.contains(QStringLiteral("holeInsKombiFeld")),
                 "die Liste legt nichts hinein");
    }

    // ── Der Fall, der beim Betreiber schiefging ─────────────────────
    //
    // „genau mitten im fenster" -- und es kam das Menue des
    // Containers. Ein Rechtsklick geht an das Widget UNTER dem
    // Zeiger; reicht das ihn weiter, laeuft er die Elternkette hoch.
    //
    // Dieser Fall schickt den Klick genau dorthin: auf eine SEITE im
    // Feld, nicht auf das Feld selbst. Er darf nicht nach oben
    // durchlaufen.
    //
    // EHRLICH DAZU: dieser Lauf bleibt auch OHNE das Fangnetz gruen
    // (Gegenprobe gefahren, 2026-11-01). Er haelt also den normalen
    // Weg fest -- der schon vorher ging --, nicht den Fehler des
    // Betreibers. Was bei ihm den Klick nach oben durchlieszt, ist
    // damit NICHT gefunden; das Netz faengt es nur ab. Wer hier
    // weitersucht: der Unterschied musz in der wirklichen Seite
    // liegen (Rotorpanel, Bandbreitenfilter), nicht im Behaelter.
    void rechtsklickAufEinerSeiteKommtAn()
    {
        auto* rahmen = new QWidget;        // steht fuer den Container
        rahmen->setGeometry(0, 0, 300, 200);
        auto* k = new KombiApplet(nullptr, rahmen);
        k->setGeometry(0, 0, 300, 200);
        auto* seite = new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX"));
        k->nimmAuf(seite);
        rahmen->show();
        QTest::qWait(50);

        QSignalSpy spion(k, &KombiApplet::listeGewuenscht);
        QContextMenuEvent ev(QContextMenuEvent::Mouse, QPoint(20, 20),
                             seite->mapToGlobal(QPoint(20, 20)));
        QVERIFY2(QApplication::sendEvent(seite, &ev),
                 "der Rechtsklick wurde nicht beantwortet — er laeuft dann "
                 "die Elternkette hoch bis zum Container, und DESSEN Menue "
                 "erscheint (Betreiber, 2026-11-01)");
        QCOMPARE(spion.count(), 1);
        delete rahmen;
    }

    // ── Eine Pille klappt zu und zeigt ihre Seite ───────────────────
    //
    // Betreiber, 2026-11-01, mit Bild: „die pillen funktionieren
    // nicht, kann nicht umschalten". Das Feld war AUFGEKLAPPT -- dann
    // ist der Stapel gar nicht zu sehen, und ein Klick auf eine Pille
    // aendert nur etwas Unsichtbares.
    void einePilleKlapptZu()
    {
        KombiApplet k(nullptr);
        k.nimmAuf(new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX")));
        k.nimmAuf(new Probepanel(QStringLiteral("Tx"), QStringLiteral("TX")));
        k.setAufgeklappt(true);
        QVERIFY(k.aufgeklappt());
        k.zeigeSeite(1);
        QVERIFY2(!k.aufgeklappt(),
                 "die Pille laeszt das Feld aufgeklappt — dann sieht man "
                 "die gewaehlte Seite nicht und haelt sie fuer kaputt");
        QCOMPARE(k.seite(), 1);
    }

    // ── Der Inhalt gehoert ins PROFIL ───────────────────────────────
    //
    // Betreiber, 2026-11-01, mit Bild: „so war mein profil nicht
    // abgespeichert" -- und auf Nachfrage: „ich habe es definitiv am
    // desktop unter 01. abgespeichert".
    //
    // Er hatte recht. Das Feld merkte sich seinen Inhalt in EINER
    // globalen Einstellung, nicht im Profil: nach einem
    // Profilwechsel war er falsch, nach einem Neustart gar nicht da.
    // ── Pille und Seite bleiben zusammen, auch nach Umsortierung ────
    //
    // Betreiber 2026-11-04: "RX/TX -- entweder RX oder TX (Pille
    // schaltet nicht um)": Pille RX hell, Inhalt TX. Nachgestellt: ein
    // drittes Panel kommt im AUFGEKLAPPTEN Zustand herein -- dann steht
    // es im Stapel vorn, in der Liste hinten. Alte Fassung: zeigeSeite(0)
    // zeigt das dritte statt RX, die Pille sagt RX.
    void pilleUndSeiteBleibenZusammen()
    {
        KombiApplet k(nullptr);
        auto* rx = new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX"));
        auto* tx = new Probepanel(QStringLiteral("Tx"), QStringLiteral("TX"));
        k.nimmAuf(rx);
        k.nimmAuf(tx);
        k.setAufgeklappt(true);
        auto* dritte = new Probepanel(QStringLiteral("Log"), QStringLiteral("LOG"));
        k.nimmAuf(dritte);
        k.setAufgeklappt(false);

        k.zeigeSeite(0);
        QCOMPARE(k.seite(), 0);
        QCOMPARE(k.aktuelleSeiteForTest(), static_cast<QWidget*>(rx));
        k.zeigeSeite(1);
        QCOMPARE(k.seite(), 1);
        QCOMPARE(k.aktuelleSeiteForTest(), static_cast<QWidget*>(tx));
        k.zeigeSeite(2);
        QCOMPARE(k.seite(), 2);
        QCOMPARE(k.aktuelleSeiteForTest(), static_cast<QWidget*>(dritte));
    }

    // ── Ein Eintrag ohne Inhalt ist kein Grund abzulehnen (2026-11-17) ─
    //
    // Gemessen mit den Einstellungen des Betreibers: jemand nahm das
    // Rotor-Panel per Elternwechsel aus dem Feld, ohne gibFrei() zu
    // rufen. Die Liste fuehrte es weiter, nimmAuf() sagte „ist schon
    // da", und das Panel blieb ohne Elternteil und unsichtbar liegen --
    // der Rotor war ganz weg.
    void einHerausgerissenesPanelWirdWiederAufgenommen()
    {
        KombiApplet k(nullptr);
        auto* rotor = new Probepanel(QStringLiteral("RotorLog"), QStringLiteral("Rotor / Log"));
        k.nimmAuf(rotor);
        QCOMPARE(k.seiten().size(), 1);
        QVERIFY(k.isAncestorOf(rotor));

        // Herausgerissen, am Feld vorbei.
        rotor->setParent(nullptr);
        QVERIFY(!k.isAncestorOf(rotor));

        // Wieder aufnehmen: es muss wirklich wieder DARIN liegen, und
        // genau einmal in der Liste stehen.
        k.nimmAuf(rotor);
        QVERIFY2(k.isAncestorOf(rotor),
                 "das Feld hat ein Panel abgelehnt, das es nur noch in der "
                 "Liste fuehrte -- es liegt jetzt nirgends");
        QCOMPARE(k.seiten().size(), 1);

        // Und der gewoehnliche Fall bleibt: zweimal aufnehmen aendert nichts.
        k.nimmAuf(rotor);
        QCOMPARE(k.seiten().size(), 1);
    }

    void derInhaltStehtImProfil()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }
        QFile f(QString::fromLocal8Bit(root)
                + QStringLiteral("/src/gui/MainWindow.cpp"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString src = QString::fromUtf8(f.readAll());

        QVERIFY2(src.contains(QStringLiteral("insert(QStringLiteral(\"kombiInhalt\")")),
                 "das Profil sichert den Inhalt des Feldes nicht — dann ist "
                 "er nach jedem Wechsel falsch");
        // Seit 2026-11-17 fuellt eine kleine Hilfsfunktion beide Felder
        // (sie braucht den Schluessel zweimal: ob das Profil ihn TRAEGT
        // und was darin steht). Der Anker ist darum ihr Aufruf, nicht
        // mehr das nackte value().
        QVERIFY2(src.contains(QStringLiteral(
                     "fuelle(m_kombiApplet,  QStringLiteral(\"kombiInhalt\"))")),
                 "das Profil stellt den Inhalt nicht wieder her");
        // Und was das Profil als eigenes Fenster fuehrt, kommt nicht ins
        // Feld -- sonst verschwindet es dort (Betreiber 2026-11-17: „Das
        // TX Fenster verschwindet immer bei FT8"). Das Verhalten selbst
        // prueft tst_tx_fenster_bleibt_im_ft8_profil am echten Fenster;
        // hier nur, dass die Regel an dieser Stelle haengt.
        QVERIFY2(src.contains(QStringLiteral("kombiInhaltOhneEigeneFenster(")),
                 "die Kombi-Fuellung fragt nicht mehr, was das Profil als "
                 "eigenes Fenster fuehrt");
        // Und die Reihenfolge: erst die Panels dorthin, wo das Profil
        // sie haben will, DANN das Feld. Umgekehrt nimmt Schritt 3 sie
        // ihm wieder weg.
        const int schritt1 = src.indexOf(QStringLiteral("1/6 Vollbild"));
        const int schritt3 = src.indexOf(QStringLiteral("3/6 Applet-Andocken"));
        const int leeren   = src.indexOf(QStringLiteral("gibAlleFrei()"));
        const int fuellen  = src.indexOf(QStringLiteral("fuelle(m_kombiApplet,"));
        const int fertig   = src.indexOf(QStringLiteral("6/6 fertig"));
        QVERIFY2(schritt1 > 0 && schritt3 > schritt1 && fertig > schritt3,
                 "der Anwenden-Weg sieht anders aus als erwartet");
        // LEEREN vor Schritt 1: sonst landen die Panels des Feldes in
        // der Spalte, obwohl das Profil sie schwebend haben will --
        // Schritt 3 entscheidet das, und der ist dann vorbei.
        // Betreiber, 2026-11-01: „aber das profil sieht nicht so aus,
        // wie gespeichert".
        QVERIFY2(leeren > 0 && leeren < schritt1,
                 "das Kombi-Feld wird zu spaet geleert — seine Panels "
                 "landen dann in der Spalte statt dort, wo das Profil sie "
                 "haben will");
        // FUELLEN nach Schritt 3: vorher naehme es ihm Schritt 3 wieder weg.
        QVERIFY2(fuellen > schritt3 && fuellen < fertig,
                 "das Kombi-Feld wird vor dem Andocken gefuellt — dann ist "
                 "es gleich wieder leer");
    }

    // ── Jedes Widget, nicht nur ein Applet ──────────────────────────
    //
    // Betreiber, 2026-11-01: „ich will da auch den rotor und andere
    // sache reinziehen können" -- und der Rotor ist kein Applet,
    // sondern der Inhalt eines Werkzeugfensters. Ein Behaelter, der
    // nach der Art fragt, kann ihn nicht aufnehmen.
    // 2026-11-06: zwei Felder duerfen sich ihren Stand nicht teilen -- sonst
    // las das zweite beim Start, was das erste zuletzt enthielt.
    void jedesFeldMerktSichSeinenStandSelbst()
    {
        KombiApplet a(nullptr);
        KombiApplet b(nullptr, nullptr, QStringLiteral("Kombi2"));
        QCOMPARE(a.appletId(), QStringLiteral("Kombi"));
        QCOMPARE(b.appletId(), QStringLiteral("Kombi2"));
        auto* w = new QWidget;
        w->setObjectName(QStringLiteral("NurImZweiten"));
        b.nimmAuf(w);
        const QString erstes = AppSettings::instance().value(QStringLiteral("Kombi/Inhalt"), QString()).toString();
        const QString zweites = AppSettings::instance().value(QStringLiteral("Kombi2/Inhalt"), QString()).toString();
        QVERIFY2(zweites.contains(QStringLiteral("NurImZweiten")), qPrintable(zweites));
        QVERIFY2(!erstes.contains(QStringLiteral("NurImZweiten")), qPrintable(erstes));
        b.gibFrei(w);
        delete w;
    }

    // ── Der Griff gehoert den eigenen Reitern (2026-11-06) ──────────────
    //
    // Der Filter haengt an der Anwendung. Ohne die Frage nach dem eigenen
    // Reiter griff ein Druck auf IRGENDEINEN Knopf die erste Seite.
    void einFremderKnopfGreiftNichts()
    {
        KombiApplet k(nullptr);
        auto* seite = new QWidget;
        seite->setWindowTitle(QStringLiteral("Seite"));
        k.nimmAuf(seite);
        QPushButton fremd(QStringLiteral("fremd"));
        const QPoint start(5, 5);
        const QPoint gStart = fremd.mapToGlobal(start);
        const QPoint weit = gStart + QPoint(300, 300);
        QMouseEvent druck(QEvent::MouseButtonPress, QPointF(start), QPointF(gStart),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&fremd, &druck);
        QMouseEvent zug(QEvent::MouseMove, QPointF(start), QPointF(weit),
                        Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&fremd, &zug);
        QMouseEvent los(QEvent::MouseButtonRelease, QPointF(start), QPointF(weit),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&fremd, &los);
        QVERIFY2(k.seiten().contains(seite), "ein fremder Knopf riss die Seite heraus");
    }

    // Der Rechtsklick erreicht den Reiter; das Netz des Feldes fing ihn ab.
    // Und aus dem Feld gezogen wird beim Loslassen, nicht beim ersten Weg.
    void derReiterHatSeinMenueUndDerZugEndetBeimLoslassen()
    {
        KombiApplet k(nullptr);
        k.resize(300, 200);
        k.show();
        auto* seite = new QWidget;
        seite->setWindowTitle(QStringLiteral("Seite"));
        k.nimmAuf(seite);
        QPushButton* reiter = nullptr;
        for (QPushButton* b : k.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("Seite")) { reiter = b; }
        }
        QVERIFY(reiter);

        QSignalSpy menue(&k, &KombiApplet::reiterMenueGewuenscht);
        QSignalSpy liste(&k, &KombiApplet::listeGewuenscht);
        QContextMenuEvent rechts(QContextMenuEvent::Mouse, QPoint(3, 3),
                                 reiter->mapToGlobal(QPoint(3, 3)));
        QApplication::sendEvent(reiter, &rechts);
        QCOMPARE(menue.size(), 1);
        QCOMPARE(liste.size(), 0);
        QCOMPARE(menue.first().at(0).value<QWidget*>(), seite);

        QSignalSpy raus(&k, &KombiApplet::herausgezogen);
        const QPoint start(3, 3);
        const QPoint gStart = reiter->mapToGlobal(start);
        const QPoint draussen = k.mapToGlobal(QPoint(k.width() + 200, 0));
        QMouseEvent druck(QEvent::MouseButtonPress, QPointF(start), QPointF(gStart),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(reiter, &druck);
        QMouseEvent zug(QEvent::MouseMove, QPointF(start), QPointF(draussen),
                        Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(reiter, &zug);
        QCOMPARE(raus.size(), 0);
        QVERIFY2(k.seiten().contains(seite), "waehrend des Zuges bleibt es im Feld");
        QMouseEvent los(QEvent::MouseButtonRelease, QPointF(start), QPointF(draussen),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(reiter, &los);
        QCOMPARE(raus.size(), 1);
        QCOMPARE(raus.first().at(1).toPoint(), draussen);
        QVERIFY(!k.seiten().contains(seite));
        delete seite;
    }

    void auchEinFremdesWidget()
    {
        KombiApplet k(nullptr);
        auto* fremd = new QWidget;
        fremd->setWindowTitle(QStringLiteral("Rotor / Log"));
        k.nimmAuf(fremd);
        QCOMPARE(k.seiten().size(), 1);
        // Der Name kommt vom Fenstertitel -- ohne ihn stand auf dem
        // Reiter „Panel" (Betreiber, 2026-11-01, mit Bild).
        QCOMPARE(KombiApplet::nameVon(fremd), QStringLiteral("Rotor / Log"));
        QCOMPARE(k.seiten().first(), fremd);
        QVERIFY(!k.istLeer());
        k.gibFrei(fremd);
        QVERIFY(k.istLeer());
        QVERIFY2(fremd->parentWidget() == nullptr,
                 "auch ein fremdes Widget kommt ohne Elternteil zurueck");
        delete fremd;
    }

    // Und der Abgleich darf daran nicht scheitern: ein fremdes
    // Widget hat kein syncFromModel.
    void derAbgleichStolpertNichtUeberFremde()
    {
        KombiApplet k(nullptr);
        auto* rx = new Probepanel(QStringLiteral("Rx"), QStringLiteral("RX"));
        auto* fremd = new QWidget;
        k.nimmAuf(rx);
        k.nimmAuf(fremd);
        k.syncFromModel();          // darf nicht stuerzen
        QCOMPARE(rx->abgeglichen, 1);
    }
};

QTEST_MAIN(TstKombiFeld)
#include "tst_kombi_feld.moc"
