// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_pan_spot_sources.cpp  (Longpath)
// =================================================================
// Spots je Quelle direkt im Panadapter-Zahnrad.
//
// Der Betreiber, 2026-11-18: „rufzeichen aus dem cluster sollte man als
// option auch ein und ausblenden können". Den Schalter gab es — nur im
// Spot Hub, Reiter Display. Gesucht hat er ihn dort, wo alle Optionen
// eines Panels stehen: rechts oben hinter dem Zahnrad.
//
// Geprueft wird, dass Zahnrad und Spot Hub EIN Schalter sind:
//   * dieselben acht Quellen wie im Spot Hub;
//   * Haken aus -> SpotSourceVisible/<Quelle> = False, die Maske der
//     Flaeche blendet die Quelle aus, ein danach gebauter Spot Hub zeigt
//     den Haken leer und die Spot-Liste verbirgt die Quelle;
//   * ein schon offener Spot Hub zieht nach (reloadSourceVisibility);
//   * umgekehrt liest das Zahnrad, was im Spot Hub gesetzt wurde;
//   * Show all / Hide all;
//   * die eigenen Dekoder (Quelle "Longpath", FT8 und RTTY) haben einen
//     Haken -- am 19.11. fehlte er, und die FT8-Rufzeichen blieben stehen;
//   * ein ECHTER Mausklick (Press/Release an das offene Untermenue) stellt
//     um, nicht nur QAction::trigger().
//
// Modification history (Longpath):
//   2026-11-19 — Eigene Dekoder + echter Klick (Betreiber: „ich denke,
//                 dass funktioniert nicht").
//   2026-11-18 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================
#include <QtTest>
#include <QCheckBox>
#include <QMenu>
#include <QMouseEvent>
#include <QSignalSpy>

#include <memory>

#include "core/AppSettings.h"
#include "gui/PanadapterApplet.h"
#include "gui/SpectrumWidget.h"
#include "gui/SpotHubDialog.h"
#include "models/BandFilterProxy.h"
#include "models/RadioModel.h"
#include "FensterFrist.h"

using namespace Longpath;

namespace {

const QStringList kQuellen = {
    QStringLiteral("Cluster"),       QStringLiteral("RBN"),
    QStringLiteral("WSJT-X"),        QStringLiteral("SpotCollector"),
    QStringLiteral("POTA"),          QStringLiteral("FreeDV"),
    QStringLiteral("PSK"),           QStringLiteral("Roundtable"),
    QStringLiteral("Longpath"),   // eigene Dekoder (FT8, RTTY)
};

QString schluessel(const QString& quelle)
{
    return AppSettings::instance()
        .value(QStringLiteral("SpotSourceVisible/") + quelle).toString();
}

QMenu* spotsMenue(QMenu* zahnrad)
{
    return zahnrad->findChild<QMenu*>(QStringLiteral("panSpotSourcesMenu"));
}

QAction* haken(QMenu* spots, const QString& quelle)
{
    for (QAction* a : spots->actions()) {
        if (a->objectName() == QStringLiteral("panSpotSource_") + quelle) {
            return a;
        }
    }
    return nullptr;
}

QAction* eintrag(QMenu* spots, const QString& text)
{
    for (QAction* a : spots->actions()) {
        if (a->text() == text) { return a; }
    }
    return nullptr;
}

SpotHubDialog* baueHub(RadioModel& m)
{
    return new SpotHubDialog(m.dxCluster(), m.rbn(), m.wsjtx(),
                             m.spotCollector(), m.pota(), m.sota(),
                             m.freeDvReporter(), m.pskReporter(),
                             m.spotModel(), m.spotTableModel(),
                             m.dxccColorProvider(), nullptr);
}

} // namespace

class TestPanSpotSources : public QObject
{
    Q_OBJECT

    static void wegraeumen(QMenu* m)
    {
        m->hide();
        m->deleteLater();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
    }

    static void alleSichtbar()
    {
        auto& s = AppSettings::instance();
        for (const QString& q : kQuellen) {
            s.setValue(QStringLiteral("SpotSourceVisible/") + q,
                       QStringLiteral("True"));
        }
    }

private slots:
    void init() { alleSichtbar(); }
    void cleanup() { alleSichtbar(); }

    // Dieselben Quellen wie die Haken im Spot Hub — eine neue Quelle
    // darf nicht nur an einer der beiden Stellen auftauchen.
    void dieselbenQuellenWieImSpotHub()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        QMenu* m = applet.buildDisplayMenuForTesting();
        QMenu* spots = spotsMenue(m);
        QVERIFY2(spots, "im Zahnrad steht ein Untermenue Spots");
        QCOMPARE(spots->title(), QStringLiteral("Spots"));

        QStringList imMenue;
        for (QAction* a : spots->actions()) {
            if (a->objectName().startsWith(QStringLiteral("panSpotSource_"))) {
                QVERIFY(a->isCheckable());
                QVERIFY2(a->isChecked(), "ab Werk ist jede Quelle sichtbar");
                imMenue << a->objectName().mid(14);
            }
        }

        RadioModel model;
        std::unique_ptr<SpotHubDialog> hub(baueHub(model));
        QStringList imHub;
        for (QCheckBox* cb : hub->findChildren<QCheckBox*>()) {
            if (cb->objectName().startsWith(QStringLiteral("displaySourceShow_"))) {
                imHub << cb->objectName().mid(18);
            }
        }
        imMenue.sort();
        imHub.sort();
        QCOMPARE(imMenue, imHub);
        QCOMPARE(imMenue.size(), 9);
        delete m;
    }

    // Haken aus: Schluessel False, Maske aus, Spot Hub und Spot-Liste
    // lesen dasselbe. Haken an: alles wieder da.
    void hakenAusBlendetDieQuelleUeberall()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        SpectrumWidget* flaeche = applet.spectrumWidget();
        QVERIFY(flaeche);
        flaeche->loadSpotDisplaySettings();
        QVERIFY(flaeche->isSpotSourceVisible(QStringLiteral("Cluster")));
        QSignalSpy gemeldet(&applet, &PanadapterApplet::spotSourcesChanged);

        QMenu* m = applet.buildDisplayMenuForTesting();
        QAction* cluster = haken(spotsMenue(m), QStringLiteral("Cluster"));
        QVERIFY(cluster && cluster->isChecked());
        cluster->trigger();

        QCOMPARE(schluessel(QStringLiteral("Cluster")), QStringLiteral("False"));
        QVERIFY2(!flaeche->isSpotSourceVisible(QStringLiteral("Cluster")),
                 "die Maske der Flaeche blendet Cluster aus");
        QVERIFY2(flaeche->isSpotSourceVisible(QStringLiteral("RBN")),
                 "die anderen Quellen bleiben");
        QCOMPARE(gemeldet.count(), 1);

        {
            RadioModel model;
            std::unique_ptr<SpotHubDialog> hub(baueHub(model));
            auto* cb = hub->findChild<QCheckBox*>(
                QStringLiteral("displaySourceShow_Cluster"));
            QVERIFY(cb);
            QVERIFY2(!cb->isChecked(), "der Spot Hub zeigt denselben Stand");
            auto* liste = hub->findChild<BandFilterProxy*>();
            QVERIFY(liste);
            QVERIFY2(!liste->isSourceVisible(QStringLiteral("Cluster")),
                     "und die Spot-Liste verbirgt Cluster");
        }

        cluster->trigger();
        QCOMPARE(schluessel(QStringLiteral("Cluster")), QStringLiteral("True"));
        QVERIFY(flaeche->isSpotSourceVisible(QStringLiteral("Cluster")));
        QCOMPARE(gemeldet.count(), 2);
        delete m;
    }

    // Ein schon gebauter Spot Hub zieht nach — Haken und Spot-Liste.
    void offenerSpotHubZiehtNach()
    {
        RadioModel model;
        std::unique_ptr<SpotHubDialog> hub(baueHub(model));
        auto* cb = hub->findChild<QCheckBox*>(QStringLiteral("displaySourceShow_RBN"));
        auto* liste = hub->findChild<BandFilterProxy*>();
        QVERIFY(cb && cb->isChecked() && liste);

        PanadapterApplet applet(QStringLiteral("A"));
        // Was MainWindow::onPanSpotSourcesChanged tut.
        connect(&applet, &PanadapterApplet::spotSourcesChanged,
                hub.get(), &SpotHubDialog::reloadSourceVisibility);
        QMenu* m = applet.buildDisplayMenuForTesting();
        haken(spotsMenue(m), QStringLiteral("RBN"))->trigger();

        QVERIFY2(!cb->isChecked(), "der offene Spot Hub zeigt den Haken leer");
        QVERIFY(!liste->isSourceVisible(QStringLiteral("RBN")));
        QCOMPARE(schluessel(QStringLiteral("RBN")), QStringLiteral("False"));
        delete m;

        // Umgekehrt: Haken im Spot Hub setzen, das Zahnrad liest ihn.
        cb->setChecked(true);
        QCOMPARE(schluessel(QStringLiteral("RBN")), QStringLiteral("True"));
        QVERIFY(liste->isSourceVisible(QStringLiteral("RBN")));
        QMenu* m2 = applet.buildDisplayMenuForTesting();
        QVERIFY(haken(spotsMenue(m2), QStringLiteral("RBN"))->isChecked());
        delete m2;

        cb->setChecked(false);
        QMenu* m3 = applet.buildDisplayMenuForTesting();
        QVERIFY2(!haken(spotsMenue(m3), QStringLiteral("RBN"))->isChecked(),
                 "das Zahnrad liest, was im Spot Hub abgeschaltet wurde");
        delete m3;
    }

    // Der Klick, den der Betreiber macht: Untermenue offen, Maus auf den
    // Eintrag, druecken, loslassen. QTest::mouseClick verpufft im
    // Pruefstand (kein echter Schirm), darum QMouseEvent + sendEvent.
    void echterKlickStelltUm()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        SpectrumWidget* flaeche = applet.spectrumWidget();
        flaeche->loadSpotDisplaySettings();
        QMenu* m = applet.buildDisplayMenuForTesting();
        QMenu* spots = spotsMenue(m);
        QVERIFY(spots);
        spots->popup(QPoint(200, 200));
        QVERIFY(QTest::qWaitForWindowExposed(spots, kFensterFrist));

        const auto klick = [&spots](QAction* a) {
            QVERIFY(a && a->isEnabled() && a->isVisible());
            const QPoint mitte = spots->actionGeometry(a).center();
            QVERIFY2(!spots->actionGeometry(a).isEmpty(), "der Eintrag hat eine Flaeche");
            const QPointF lokal(mitte);
            const QPointF global(spots->mapToGlobal(mitte));
            QMouseEvent bewegt(QEvent::MouseMove, lokal, global,
                               Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(spots, &bewegt);
            QMouseEvent drueckt(QEvent::MouseButtonPress, lokal, global,
                                Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(spots, &drueckt);
            QMouseEvent laesst(QEvent::MouseButtonRelease, lokal, global,
                               Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(spots, &laesst);
            QCoreApplication::processEvents();
        };

        klick(haken(spots, QStringLiteral("Longpath")));
        QCOMPARE(schluessel(QStringLiteral("Longpath")), QStringLiteral("False"));
        QVERIFY2(!flaeche->isSpotSourceVisible(QStringLiteral("Longpath")),
                 "ein Klick auf den eigenen Dekoder blendet die FT8-Rufzeichen aus");
        QVERIFY(flaeche->isSpotSourceVisible(QStringLiteral("Cluster")));

        // Das Menue schliesst nach dem Klick; der naechste Klick geht in
        // ein frisch gebautes, wie beim Betreiber. Abraeumen ueber die
        // Ereignisschleife: Qt arbeitet nach dem Klick noch am Schliessen.
        wegraeumen(m);
        m = applet.buildDisplayMenuForTesting();
        spots = spotsMenue(m);
        QVERIFY(!haken(spots, QStringLiteral("Longpath"))->isChecked());
        spots->popup(QPoint(200, 200));
        QVERIFY(QTest::qWaitForWindowExposed(spots, kFensterFrist));
        klick(haken(spots, QStringLiteral("Cluster")));
        QCOMPARE(schluessel(QStringLiteral("Cluster")), QStringLiteral("False"));
        QVERIFY(!flaeche->isSpotSourceVisible(QStringLiteral("Cluster")));
        wegraeumen(m);
    }

    void showAllUndHideAll()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        SpectrumWidget* flaeche = applet.spectrumWidget();
        QMenu* m = applet.buildDisplayMenuForTesting();
        QMenu* spots = spotsMenue(m);
        QAction* aus = eintrag(spots, QStringLiteral("Hide all"));
        QAction* an = eintrag(spots, QStringLiteral("Show all"));
        QVERIFY(aus && an);

        aus->trigger();
        for (const QString& q : kQuellen) {
            QCOMPARE(schluessel(q), QStringLiteral("False"));
            QVERIFY(!flaeche->isSpotSourceVisible(q));
        }
        an->trigger();
        for (const QString& q : kQuellen) {
            QCOMPARE(schluessel(q), QStringLiteral("True"));
            QVERIFY(flaeche->isSpotSourceVisible(q));
        }
        delete m;
    }
};

QTEST_MAIN(TestPanSpotSources)
#include "tst_pan_spot_sources.moc"
