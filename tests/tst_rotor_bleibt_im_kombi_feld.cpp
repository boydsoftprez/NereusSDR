// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_rotor_bleibt_im_kombi_feld.cpp  (Longpath)
// =================================================================
//
// Betreiber, 2026-11-17, 20:09, mit Bild vom frisch gestarteten
// Programm:
//
//   „schon wieder erscheint hier der rotor mitten im bild"
//
// Auf dem Bild: rechts das Feld „Combined" mit den Pillen
// „Rotor / Log", „TX", „RX" -- und MITTEN ueber dem Panadapter ein
// eigenes Fenster „ROTOR / LOG". Ein Panel an zwei Orten.
//
// Sein Startprofil „Neu" fuehrt Rotor/Log im Kombi-Feld und nirgends
// sonst:
//
//     rotor:           { form: docked }
//     kombiInhalt:     [ RotorLog, Tx, Rx ]
//
// und das Protokoll dieses Starts (longpath-20261117-200836.log) sagt,
// dass das Profil ihn auch hineingelegt hat:
//
//     20:08:38.222 [ProfileApply:Step] 5/6 Rotor/Log
//     20:08:38.453 Kombi:  3 von 3 gemerkten Panels zurueckgeholt
//     20:08:38.453 [ProfileApply:Step] 6/6 fertig
//
// Danach kam nichts mehr zum Feld -- und genau das war es. Der Weg ins
// Feld fuehrt ueber detachRotorPanel() (das Panel wird fuer einen
// Augenblick zum Fenster), und das schreibt RotorFloating=True. Der
// Konstruktor las den Schluessel NACH dem Profil, hielt ihn fuer einen
// Wunsch und riss das Panel wieder heraus, in ein Fenster am Platz des
// Uebergabefensters (ToolWindowGeometry_RotorLog: 655,401 auf dem
// 1470 breiten Schirm -- die Bildmitte). Das Feld fuehrte die Seite
// weiter in seiner Liste: daher die Pille.
//
// Den einen Weg hat #473 am selben Abend geschlossen (detachRotorPanel
// laesst liegen, was in einem Feld liegt; der zweite Kombi-Durchgang
// beim Start ist wieder da) und prueft ihn in
// tst_tx_fenster_bleibt_im_ft8_profil. Dieser Pruefstand haelt fest,
// was dabei offen blieb -- dieselbe Luecke an den Nachbarstellen:
//
//  * Der Startblock kennt einen ZWEITEN Schluessel, RotorPanelBelow,
//    und der geht an detachRotorPanel() vorbei: setRotorPanelBelow(true)
//    haengt das Panel genauso aus dem Feld.
//  * Die Sichtbarkeits-Schleife nach dem Profil baut ein eingeschaltetes
//    Applet, das im Feld liegt, in die Spalte ein (addApplet) -- der
//    zweite Durchgang holt es zurueck, aber ans ENDE: die Reihenfolge
//    der Pillen aendert sich bei jedem Start.
//  * „Rotor zeigen" (Spot zum Rotor, Menue) machte das Dock auf --
//    leer, denn das Panel liegt im Feld. Ein zweites „Rotor / Log".
//  * Und das Feld selbst fuehrte eine Seite weiter, die ihm jemand per
//    Elternwechsel genommen hatte: daher die Pille auf seinem Bild.
//    Die Regel dazu prueft tst_kombi_feld am Feld allein.
//
// Drei Laeufe ueber dieselbe Datei (LONGPATH_PRUEF_VARIANTE):
//
//   (ohne)   sein ECHTER Profilsatz von diesem Tag (tests/data) und die
//            Schluessel, wie sie in seiner Einstellungsdatei standen.
//   unten    derselbe Satz, in „Neu" rotor.form = below und
//            RotorPanelBelow=True -- ein Profil, das im Feld aufgenommen
//            wurde, waehrend der globale Schluessel noch „unter dem
//            Panadapter" sagte (die Aufnahme liest ihn, wenn es kein
//            Rotor-Fenster gibt).
//   tx-an    derselbe Satz, in „Neu" TX eingeschaltet gefuehrt
//            (visible.Tx = true), wie es sein FT8-Profil tut.
//
// Getrennt, weil sich die zwei Widersprueche sonst verdecken: gemessen
// am 2026-11-18 mit beiden zugleich -- das zurueckgeholte TX baute die
// Reiter neu, und dabei haengte das Feld den Rotor zufaellig wieder ein.
//
// Die Fragen sind in beiden die des Bildes: gibt es ein Rotor/Log
// ausserhalb des Feldes? Liegt jedes Panel wirklich IN dem Feld, das
// es als Pille fuehrt? Und steht die Reihenfolge?
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QApplication>
#include <QDialog>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include "core/AppSettings.h"
#include "gui/LayoutProfiles.h"
#include "gui/MainWindow.h"
#include "gui/ToolWindow.h"
#include "gui/applets/AppletWidget.h"
#include "gui/applets/KombiApplet.h"
#include "gui/widgets/RotorLogbookPanel.h"
#include "FensterFrist.h"

using namespace Longpath;

class TstRotorBleibtImKombiFeld : public QObject {
    Q_OBJECT

private:
    MainWindow*     m_mw{nullptr};
    LayoutProfiles* m_profile{nullptr};
    // Siehe Kopf: die Laeufe mit einem widerspruechlichen „Neu".
    const QString m_variante{qEnvironmentVariable("LONGPATH_PRUEF_VARIANTE")};
    const bool m_unten{m_variante == QStringLiteral("unten")};
    const bool m_txAn{m_variante == QStringLiteral("tx-an")};

    // Bis sich alles gesetzt hat: Raster (180 ms), Profilrahmen-
    // Nachlauf (bis 3000 ms), Kombi-Fuellung.
    static void warten() { QTest::qWait(3500); }

    // Das Rotor/Log-Panel, wo immer es gerade haengt. Es gibt genau
    // eines -- ein zweites waere ein zweites Logbuch.
    static QList<RotorLogbookPanel*> rotorPanels()
    {
        QList<RotorLogbookPanel*> alle;
        const QWidgetList widgets = QApplication::allWidgets();
        for (QWidget* w : widgets) {
            if (auto* p = qobject_cast<RotorLogbookPanel*>(w)) { alle << p; }
        }
        return alle;
    }

    // Jedes SICHTBARE Rotor/Log-Werkzeugfenster -- das, was auf seinem
    // Bild ueber dem Panadapter lag.
    static QList<ToolWindow*> rotorFenster()
    {
        QList<ToolWindow*> offen;
        const QWidgetList oben = QApplication::topLevelWidgets();
        for (QWidget* w : oben) {
            auto* t = qobject_cast<ToolWindow*>(w);
            if (t && t->id() == QStringLiteral("RotorLog") && t->isVisible()) {
                offen << t;
            }
        }
        return offen;
    }

    static QString rechteck(const QRect& r)
    {
        return QStringLiteral("%1,%2 %3x%4").arg(r.x()).arg(r.y())
                                             .arg(r.width()).arg(r.height());
    }

    // Wo liegt Rotor/Log gerade -- in Worten, fuer die Fehlermeldung.
    QString lage() const
    {
        QStringList teile;
        for (ToolWindow* t : rotorFenster()) {
            teile << QStringLiteral("eigenes Fenster bei %1 (Hauptfenster %2)")
                         .arg(rechteck(t->geometry()), rechteck(m_mw->geometry()));
        }
        for (int nr : {1, 2}) {
            KombiApplet* feld = m_mw->kombiFeldForTest(nr);
            if (!feld) { continue; }
            QStringList seiten;
            for (QWidget* w : feld->seiten()) {
                seiten << KombiApplet::nameVon(w)
                              + (feld->isAncestorOf(w) ? QString()
                                                       : QStringLiteral(" (NICHT im Feld)"));
            }
            teile << QStringLiteral("%1=[%2]").arg(feld->appletId(),
                                                   seiten.join(QStringLiteral(", ")));
        }
        return teile.join(QStringLiteral("; "));
    }

    // Die zwei Fragen des Bildes, an einer Stelle.
    void pruefeRotorNurImFeld(const QString& wann)
    {
        const QList<RotorLogbookPanel*> panels = rotorPanels();
        QVERIFY2(panels.size() == 1,
                 qPrintable(QStringLiteral("%1: es gibt %2 Rotor/Log-Panels statt einem")
                                .arg(wann).arg(panels.size())));
        RotorLogbookPanel* panel = panels.first();
        KombiApplet* feld = m_mw->kombiFeldForTest(1);
        QVERIFY(feld);

        // 1. Kein eigenes Rotor/Log-Fenster.
        QVERIFY2(rotorFenster().isEmpty(),
                 qPrintable(QStringLiteral("%1: Rotor/Log steht als eigenes Fenster im Bild, "
                                           "obwohl das Profil es im Kombi-Feld fuehrt: %2")
                                .arg(wann, lage())));
        QVERIFY2(m_mw->rotorWindowForTest() == nullptr,
                 qPrintable(QStringLiteral("%1: es gibt ein Rotor/Log-Fenster neben dem "
                                           "Kombi-Feld: %2").arg(wann, lage())));

        // 2. Das Panel liegt wirklich IM Feld -- nicht nur als Pille,
        //    und damit weder unter dem Panadapter noch im Dock.
        QVERIFY2(feld->seiten().contains(panel),
                 qPrintable(QStringLiteral("%1: das Kombi-Feld fuehrt Rotor/Log nicht: %2")
                                .arg(wann, lage())));
        QVERIFY2(feld->isAncestorOf(panel),
                 qPrintable(QStringLiteral("%1: die Pille steht da, das Panel liegt "
                                           "woanders: %2").arg(wann, lage())));
    }

    // Die Regel dahinter, fuer jede Seite jedes Feldes: was ein Feld als
    // Seite fuehrt, liegt auch darin.
    void pruefeJedeSeiteLiegtInIhremFeld(const QString& wann)
    {
        for (int nr : {1, 2}) {
            KombiApplet* feld = m_mw->kombiFeldForTest(nr);
            if (!feld) { continue; }
            for (QWidget* w : feld->seiten()) {
                QVERIFY2(feld->isAncestorOf(w),
                         qPrintable(QStringLiteral("%1: %2 fuehrt „%3\" als Seite, das Panel "
                                                   "liegt aber woanders: %4")
                                        .arg(wann, feld->appletId(),
                                             KombiApplet::nameVon(w), lage())));
            }
        }
    }

private slots:
    void initTestCase()
    {
        QFile f(QStringLiteral(LONGPATH_TEST_DATA_DIR)
                + QStringLiteral("/layoutprofile_ft8_tx_doppelt_2026-11-17.json"));
        QVERIFY2(f.open(QIODevice::ReadOnly), "Profilsatz fehlt in tests/data");
        QByteArray json = f.readAll();
        QJsonObject wurzel = QJsonDocument::fromJson(json).object();
        QVERIFY(!wurzel.isEmpty());

        // ── Voraussetzung mitpruefen ─────────────────────────────────
        // Das Startprofil MUSS Rotor/Log im Kombi-Feld fuehren und als
        // „docked" -- sonst prueft dieser Lauf nichts und ist trotzdem
        // gruen.
        bool voraussetzung = false;
        const QJsonArray profile = wurzel.value(QStringLiteral("profiles")).toArray();
        QVERIFY(!profile.isEmpty());
        {
            const QJsonObject erstes = profile.first().toObject();
            QCOMPARE(erstes.value(QStringLiteral("name")).toString(), QStringLiteral("Neu"));
            const QJsonObject st = erstes.value(QStringLiteral("state")).toObject();
            QStringList inhalt;
            for (const QJsonValue& k : st.value(QStringLiteral("kombiInhalt")).toArray()) {
                inhalt << k.toString();
            }
            voraussetzung =
                inhalt == QStringList{QStringLiteral("RotorLog"), QStringLiteral("Tx"),
                                      QStringLiteral("Rx")}
                && st.value(QStringLiteral("rotor")).toObject()
                           .value(QStringLiteral("form")).toString() == QStringLiteral("docked")
                && st.value(QStringLiteral("floatingApplets")).toObject()
                       .contains(QStringLiteral("Kombi"));
        }
        QVERIFY2(voraussetzung,
                 "Voraussetzung: das Startprofil „Neu\" fuehrt kombiInhalt = "
                 "[RotorLog, Tx, Rx], rotor.form = docked und das Kombi-Feld als "
                 "schwebendes Fenster");

        QVERIFY2(m_variante.isEmpty() || m_unten || m_txAn,
                 "unbekannte LONGPATH_PRUEF_VARIANTE");
        if (m_unten || m_txAn) {
            // „Neu" an EINER Stelle veraendert -- siehe Kopf. Die
            // Voraussetzung oben galt der unveraenderten Vorlage.
            QJsonArray alle = wurzel.value(QStringLiteral("profiles")).toArray();
            QJsonObject erstes = alle.first().toObject();
            QJsonObject st = erstes.value(QStringLiteral("state")).toObject();
            if (m_unten) {
                st.insert(QStringLiteral("rotor"),
                          QJsonObject{{QStringLiteral("form"), QStringLiteral("below")}});
            }
            if (m_txAn) {
                QJsonObject sichtbar = st.value(QStringLiteral("visible")).toObject();
                QVERIFY2(!sichtbar.value(QStringLiteral("Tx")).toBool(),
                         "Voraussetzung: die Vorlage fuehrt TX in „Neu\" ausgeschaltet");
                sichtbar.insert(QStringLiteral("Tx"), true);
                st.insert(QStringLiteral("visible"), sichtbar);
            }
            erstes.insert(QStringLiteral("state"), st);
            alle.replace(0, erstes);
            wurzel.insert(QStringLiteral("profiles"), alle);
            json = QJsonDocument(wurzel).toJson(QJsonDocument::Compact);
        }

        AppSettings& s = AppSettings::instance();
        s.setValue(LayoutProfiles::settingsKey(), QString::fromUtf8(json));
        // Die Schluessel, wie sie am 2026-11-17 um 20:09 in seiner
        // Einstellungsdatei standen.
        s.setValue(QStringLiteral("Kombi/Inhalt"), QStringLiteral("RotorLog,Tx,Rx"));
        s.setValue(QStringLiteral("Kombi/Seite"), QStringLiteral("2"));
        s.setValue(QStringLiteral("RotorPanelBelow"),
                   m_unten ? QStringLiteral("True") : QStringLiteral("False"));
        s.setValue(QStringLiteral("RotorFloating"), QStringLiteral("True"));
        s.setValue(QStringLiteral("AppletWinRotorLogVisible"), QStringLiteral("True"));
        // Der Platz des Uebergabefensters: 655,401 bei 100x30, Schirm
        // 1470 breit. Dorthin ging das Fenster auf seinem Bild auf.
        s.setValue(QStringLiteral("ToolWindowGeometry_RotorLog"),
                   QByteArray::fromBase64(
                       "AdnQywADAAAAAAKPAAABkQAAAvIAAAGuAAACjwAAAZEAAALyAAABrgAAAAAAAAAA"
                       "Bb4AAAKPAAABkQAAAvIAAAGu"));

        // Das Contestprogramm fragt im leeren Pruefordner nach seinen
        // Einstellungen (modaler Dialog) -- jeden gleich wieder zumachen.
        auto* zu = new QTimer(this);
        connect(zu, &QTimer::timeout, this, []() {
            if (auto* d = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
                d->reject();
            }
        });
        zu->start(100);

        m_mw = new MainWindow();
        m_mw->show();
        QVERIFY2(QTest::qWaitForWindowExposed(m_mw, kFensterFrist),
                 "das Hauptfenster muss ueberhaupt erscheinen");
        warten();

        m_profile = m_mw->findChild<LayoutProfiles*>();
        QVERIFY(m_profile);
        QCOMPARE(m_profile->current(), QStringLiteral("Neu"));
        QVERIFY(m_profile->exists(QStringLiteral("FT8")));
    }

    // 1. Sein Bild: frisch gestartet, Profil „Neu", kein Funkgeraet.
    //    Rotor/Log liegt im Kombi-Feld -- und nur dort.
    void nachDemStartLiegtRotorLogNurImFeld()
    {
        pruefeRotorNurImFeld(QStringLiteral("nach dem Start"));
    }

    // 2. TX und RX liegen ebenfalls im Feld, wie das Profil es will --
    //    und was ein Feld als Seite fuehrt, liegt auch darin. Dieselbe
    //    Regel fuer jedes Panel, nicht nur fuer den Rotor.
    void txUndRxLiegenAuchImFeld()
    {
        KombiApplet* feld = m_mw->kombiFeldForTest(1);
        QVERIFY(feld);
        QStringList namen;
        for (QWidget* w : feld->seiten()) {
            if (auto* a = qobject_cast<AppletWidget*>(w)) {
                namen << a->appletId();
            } else {
                namen << w->objectName();
            }
        }
        QCOMPARE(namen, (QStringList{QStringLiteral("RotorLog"), QStringLiteral("Tx"),
                                     QStringLiteral("Rx")}));
        pruefeJedeSeiteLiegtInIhremFeld(QStringLiteral("nach dem Start"));
    }

    // 3. Die naechste Aufnahme des Profils sagt dasselbe wie vorher:
    //    Rotor/Log im Feld, in derselben Reihenfolge, und die Form wie
    //    sie war -- kein „floating" mit einer Lage in der Bildmitte, das
    //    sich sonst im Profil festschriebe.
    void dieAufnahmeSchreibtKeinRotorFensterFest()
    {
        m_profile->captureIntoCurrent();
        const QVariantMap neu = m_profile->snapshot(QStringLiteral("Neu"));
        QCOMPARE(neu.value(QStringLiteral("kombiInhalt")).toStringList(),
                 (QStringList{QStringLiteral("RotorLog"), QStringLiteral("Tx"),
                              QStringLiteral("Rx")}));
        // „below" im Lauf „unten": die Form ist dort der globale
        // Schluessel, und den laesst niemand an. Entscheidend ist, dass
        // es nie „floating" wird.
        QCOMPARE(neu.value(QStringLiteral("rotor")).toMap()
                     .value(QStringLiteral("form")).toString(),
                 m_unten ? QStringLiteral("below") : QStringLiteral("docked"));
    }

    // 4. Ein Spot „zum Rotor" (raiseRotorPanel) zeigt die Seite im Feld
    //    -- er oeffnet weder ein Fenster noch das leere Dock daneben.
    void rotorZeigenBleibtImFeld()
    {
        KombiApplet* feld = m_mw->kombiFeldForTest(1);
        QVERIFY(feld);
        feld->zeigeSeite(2);                       // RX, wie auf seinem Bild
        m_mw->raiseRotorPanelForTest();
        QTest::qWait(300);
        pruefeRotorNurImFeld(QStringLiteral("nach „Rotor zeigen\""));
        const QList<RotorLogbookPanel*> panels = rotorPanels();
        QCOMPARE(panels.size(), 1);
        QCOMPARE(feld->aktuelleSeiteForTest(), static_cast<QWidget*>(panels.first()));
        if (auto* dock = m_mw->findChild<QWidget*>(QStringLiteral("rotorLogDock"))) {
            QVERIFY2(!dock->isVisible(),
                     "das leere Rotor/Log-Dock ging auf, obwohl das Panel im "
                     "Kombi-Feld liegt");
        }
    }

    // 5. Weg und zurueck: nach FT8 (dort fuehrt das Profil Rotor/Log
    //    ebenfalls im Feld) und wieder nach „Neu". Kein Rotor/Log-
    //    Fenster, in keinem der beiden.
    void profilwechselUndZurueck()
    {
        QVERIFY(m_profile->activate(QStringLiteral("FT8")));
        warten();
        pruefeRotorNurImFeld(QStringLiteral("in FT8"));
        pruefeJedeSeiteLiegtInIhremFeld(QStringLiteral("in FT8"));

        QVERIFY(m_profile->activate(QStringLiteral("Neu")));
        warten();
        pruefeRotorNurImFeld(QStringLiteral("zurueck in „Neu\""));
        pruefeJedeSeiteLiegtInIhremFeld(QStringLiteral("zurueck in „Neu\""));
    }

    // 6. Ein Profil, das Rotor/Log NICHT im Feld fuehrt (LOGBUCH:
    //    kombiInhalt leer, rotor.form = docked, WinRotorLog aus): dann
    //    liegt es dort, wo das Profil es hinlegt -- im Hauptfenster,
    //    nicht herrenlos als eigenes Fenster.
    void ohneFeldLiegtEsWoDasProfilEsHinlegt()
    {
        QVERIFY(m_profile->exists(QStringLiteral("LOGBUCH")));
        QVERIFY(m_profile->activate(QStringLiteral("LOGBUCH")));
        warten();
        QVERIFY2(rotorFenster().isEmpty(),
                 qPrintable(QStringLiteral("in LOGBUCH steht ein Rotor/Log-Fenster im "
                                           "Bild: ") + lage()));
        const QList<RotorLogbookPanel*> panels = rotorPanels();
        QCOMPARE(panels.size(), 1);
        RotorLogbookPanel* panel = panels.first();
        for (int nr : {1, 2}) {
            KombiApplet* feld = m_mw->kombiFeldForTest(nr);
            QVERIFY2(!feld || !feld->seiten().contains(panel),
                     qPrintable(QStringLiteral("LOGBUCH fuehrt Rotor/Log nicht im "
                                               "Feld: ") + lage()));
        }
        QVERIFY2(panel->window() == m_mw || !panel->isVisible(),
                 qPrintable(QStringLiteral("Rotor/Log liegt in LOGBUCH weder im "
                                           "Hauptfenster noch ist es verborgen: ") + lage()));
    }

    // Kein cleanupTestCase(): das Hauptfenster bleibt bis zum Prozessende
    // stehen, wie in tst_tx_fenster_bleibt_im_ft8_profil -- sein Abbau
    // ist ein eigenes Thema und nicht das, was hier geprueft wird.
};

QTEST_MAIN(TstRotorBleibtImKombiFeld)
#include "tst_rotor_bleibt_im_kombi_feld.moc"
