// SPDX-License-Identifier: GPL-3.0-or-later
//
// Locator-Jagd mit FT8 -- ohne Funkgeraet, mit erfundenen Funden.
//
// Betreiber, nachts zum 11.11.: „besonders bei FT8 sehen, welche Locator
// ich noch nicht gearbeitet habe und welche gerade erreichbar sind, damit
// ich auf der Karte draufklicken kann und es probiere."
//
// Was hier gefahren wird:
//   * das Mosaik gegen ein kleines Logbuch (je Band, bestaetigt schlaegt
//     gearbeitet, „nur FT8"),
//   * die Live-Schicht: Einordnung, Auffrischen ohne Locator, Altern,
//     Zaehler,
//   * der Klick auf der Karte geht DENSELBEN Weg wie der Doppelklick in
//     der Fundliste -- dieselbe Zeile, derselbe Partner, derselbe Zug,
//   * und ohne FT8-Betrieb sagt er, warum nicht, statt nichts zu tun.
//
// Mit LONGPATH_GRAB_DIR (und LONGPATH_WELTBILD) legt der letzte Fall
// Bilder der beiden Karten mit einem erfundenen 20-m-Band ab.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSpinBox>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimeZone>

#include <algorithm>
#include <cmath>

#include "FensterFrist.h"
#include "core/AppSettings.h"
#include "core/Ft8Ablauf.h"
#include "core/Ft8Decoder.h"
#include "core/LocatorJagd.h"
#include "core/Maidenhead.h"
#include "core/MoxController.h"
#include "gui/Ft8KartenBruecke.h"
#include "gui/QsoMapWindow.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/LocatorJagdZeichen.h"
#include <QTableWidget>
#include "gui/applets/Ft8Applet.h"
#include "gui/widgets/AzimutalKarteWidget.h"
#include "gui/widgets/FlatMapWidget.h"
#include "gui/widgets/GibsTileLayer.h"
#include "gui/widgets/WorldTexture.h"
#include "models/LogEntry.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

LogEntry qso(const QString& call, const QString& grid, const QString& band,
             const QString& mode, bool lotw = false)
{
    LogEntry e;
    e.call = call;
    e.gridSquare = grid;
    e.band = band;
    e.mode = mode;
    e.timeOn = QDateTime(QDate(2026, 10, 1), QTime(12, 0), QTimeZone::UTC);
    if (lotw) { e.extras.append({QStringLiteral("LOTW_QSL_RCVD"), QStringLiteral("Y")}); }
    return e;
}

/// Ein kleines Logbuch, an dem jede Regel einmal haengt.
QVector<LogEntry> kleinesLog()
{
    QVector<LogEntry> log;
    log << qso(QStringLiteral("DL1ABC"), QStringLiteral("JO31"), QStringLiteral("20m"),
               QStringLiteral("FT8"), true);                       // 20 m bestaetigt
    log << qso(QStringLiteral("DL2XYZ"), QStringLiteral("JO31ab"), QStringLiteral("40m"),
               QStringLiteral("SSB"));                             // 40 m nur gearbeitet
    log << qso(QStringLiteral("OE3AAA"), QStringLiteral("JN78"), QStringLiteral("20m"),
               QStringLiteral("FT8"));                             // erst gearbeitet ...
    LogEntry karte = qso(QStringLiteral("OE3BBB"), QStringLiteral("JN78"), QStringLiteral("20m"),
                         QStringLiteral("FT8"));
    karte.extras.append({QStringLiteral("QSL_RCVD"), QStringLiteral("Y")});
    log << karte;                                                  // ... dann bestaetigt
    log << qso(QStringLiteral("G4ABC"), QStringLiteral("IO91"), QStringLiteral("20m"),
               QStringLiteral("SSB"));                             // 20 m, aber SSB
    log << qso(QStringLiteral("RR73X"), QStringLiteral("RR73"), QStringLiteral("20m"),
               QStringLiteral("FT8"));                             // kein Feld
    log << qso(QStringLiteral("NOGRID"), QString(), QStringLiteral("20m"),
               QStringLiteral("FT8"));                             // gar kein Locator
    return log;
}

Ft8Gehoert gehoert(const QString& call, const QString& loc, const QDateTime& t,
                   int snr = -10, bool neuesLand = false,
                   const QString& band = QStringLiteral("20m"))
{
    Ft8Gehoert g;
    g.rufzeichen = call;
    g.locator = loc;
    g.snrDb = snr;
    g.band = band;
    g.modus = QStringLiteral("FT8");
    g.zeit = t;
    g.nachricht = loc.isEmpty() ? QStringLiteral("OE5SOS %1 -12").arg(call)
                                : QStringLiteral("CQ %1 %2").arg(call, loc);
    g.zeile = QStringLiteral("%1  %2   0.2  1500  %3")
                  .arg(t.toString(QStringLiteral("HHmmss")))
                  .arg(snr, 3).arg(g.nachricht);
    g.neuesLand = neuesLand;
    return g;
}

Ft8Decode fund(const QString& text, float snr = -8.0f, float hz = 1500.0f)
{
    Ft8Decode d;
    d.snrDb = snr;
    d.dtSeconds = 0.2f;
    d.freqHz = hz;
    d.text = text;
    return d;
}

/// Ein Klick (Druck + Loslassen) per sendEvent: QTest-Mausklicks
/// verpuffen ohne Schirm.
void klick(QWidget* w, const QPointF& at)
{
    const QPointF g = w->mapToGlobal(at);
    QMouseEvent druck(QEvent::MouseButtonPress, at, g, Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &druck);
    QMouseEvent los(QEvent::MouseButtonRelease, at, g, Qt::LeftButton,
                    Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &los);
}

/// Ein Doppelklick auf der Karte. Ein einzelner Klick darf NICHT
/// anrufen (Betreiber 2026-11-11: „doppelklick, sofern ich diesen
/// sender empfangen kann, und das ft8 geht los") -- also geht hier
/// erst ein voller Klick durch und dann das DoubleClick-Ereignis,
/// genau wie Qt es einem echten Doppelklick schickt.
void doppelklick(QWidget* w, const QPointF& at)
{
    klick(w, at);
    const QPointF g = w->mapToGlobal(at);
    QMouseEvent zwei(QEvent::MouseButtonDblClick, at, g, Qt::LeftButton,
                     Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &zwei);
    QMouseEvent los(QEvent::MouseButtonRelease, at, g, Qt::LeftButton,
                    Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(w, &los);
}

void doppelklickInDieListe(Ft8Applet& applet)
{
    auto* vp = applet.listForTest()->viewport();
    QMouseEvent ev(QEvent::MouseButtonDblClick, QPointF(10, 5),
                   vp->mapToGlobal(QPointF(10, 5)),
                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(vp, &ev);
}

void eigeneStation()
{
    AppSettings::instance().setValue(QStringLiteral("User/Callsign"), QStringLiteral("OE5SOS"));
    AppSettings::instance().setValue(QStringLiteral("StationGridSquare"), QStringLiteral("JN67VV"));
    AppSettings::instance().setValue(QStringLiteral("User/GridSquare"), QStringLiteral("JN67VV"));
}

} // namespace

class TstFt8Locatorjagd : public QObject
{
    Q_OBJECT

private slots:

    // ── Das Mosaik ──────────────────────────────────────────────────
    void mosaikJeBandAusEinemKleinenLog()
    {
        LocatorMosaik m;
        m.baue(kleinesLog(), false);

        QCOMPARE(m.stand(QStringLiteral("JO31"), QStringLiteral("20m")), FeldStand::Bestaetigt);
        // Sechs Stellen im Log zaehlen fuer das vierstellige Feld.
        QCOMPARE(m.stand(QStringLiteral("JO31"), QStringLiteral("40m")), FeldStand::Gearbeitet);
        // Gearbeitet und spaeter bestaetigt: bestaetigt gewinnt, in
        // beliebiger Reihenfolge.
        QCOMPARE(m.stand(QStringLiteral("JN78"), QStringLiteral("20m")), FeldStand::Bestaetigt);
        QCOMPARE(m.stand(QStringLiteral("IO91"), QStringLiteral("20m")), FeldStand::Gearbeitet);
        QCOMPARE(m.stand(QStringLiteral("KO02"), QStringLiteral("20m")), FeldStand::Nie);
        // Ueber alle Baender: das Beste je Feld.
        QCOMPARE(m.stand(QStringLiteral("JO31"), QString()), FeldStand::Bestaetigt);
        // „RR73" ist kein Feld, auch wenn es so aussieht.
        QCOMPARE(m.stand(QStringLiteral("RR73"), QStringLiteral("20m")), FeldStand::Nie);
        QCOMPARE(m.felder(QStringLiteral("20m")).size(), 3);   // JO31, JN78, IO91
        QCOMPARE(m.felder(QStringLiteral("40m")).size(), 1);
        QCOMPARE(m.felder(QString()).size(), 3);

        // Gearbeitet heisst: das Rufzeichen auf DIESEM Band.
        QVERIFY(m.gearbeitet(QStringLiteral("dl1abc"), QStringLiteral("20m")));
        QVERIFY(!m.gearbeitet(QStringLiteral("DL1ABC"), QStringLiteral("40m")));
        QVERIFY(m.gearbeitet(QStringLiteral("NOGRID"), QStringLiteral("20m")));
        QCOMPARE(m.baender(), (QStringList{QStringLiteral("40m"), QStringLiteral("20m")}));

        // Nur FT8: SSB faellt heraus -- auf 20 m ist IO91 dann noch frei,
        // und auf 40 m gibt es gar nichts.
        m.baue(kleinesLog(), true);
        QCOMPARE(m.stand(QStringLiteral("IO91"), QStringLiteral("20m")), FeldStand::Nie);
        QCOMPARE(m.stand(QStringLiteral("JO31"), QStringLiteral("40m")), FeldStand::Nie);
        QCOMPARE(m.stand(QStringLiteral("JO31"), QStringLiteral("20m")), FeldStand::Bestaetigt);
        QVERIFY(!m.gearbeitet(QStringLiteral("G4ABC"), QStringLiteral("20m")));
    }

    // ── Die Rangfolge ───────────────────────────────────────────────
    void rangfolgeLandVorFeldVorGearbeitet()
    {
        QCOMPARE(LiveSchicht::art(true, true, true), LiveArt::NeuesLand);
        QCOMPARE(LiveSchicht::art(false, true, true), LiveArt::NeuesFeld);
        QCOMPARE(LiveSchicht::art(false, false, true), LiveArt::Gearbeitet);
        QCOMPARE(LiveSchicht::art(false, false, false), LiveArt::Gewoehnlich);
    }

    // ── Die Live-Schicht: einordnen, auffrischen, altern, zaehlen ────
    void liveSchichtOrdnetEinFrischtAufUndAltert()
    {
        LocatorMosaik m;
        m.baue(kleinesLog(), false);
        LiveSchicht live;
        QCOMPARE(live.fensterSekunden(), 120);

        const QDateTime t0(QDate(2026, 11, 11), QTime(6, 0, 0), QTimeZone::UTC);
        live.nimm(gehoert(QStringLiteral("SP9XYZ"), QStringLiteral("KO02"), t0, -14));
        live.nimm(gehoert(QStringLiteral("DL1ABC"), QStringLiteral("JO31"), t0, -3));
        live.nimm(gehoert(QStringLiteral("F5NEW"), QStringLiteral("JN18"), t0, -18));
        live.nimm(gehoert(QStringLiteral("ZS6XX"), QStringLiteral("KG33"), t0, -20, true));
        live.nimm(gehoert(QStringLiteral("G0OLD"), QStringLiteral("IO91"), t0, -9));
        // Ohne Locator und nie mit einem gehoert: nicht zu verorten.
        live.nimm(gehoert(QStringLiteral("UA9AAA"), QString(), t0));
        // RR73 an der Stelle des Locators ist keiner.
        live.nimm(gehoert(QStringLiteral("UA9BBB"), QStringLiteral("RR73"), t0));
        QCOMPARE(live.anzahl(), 5);

        QVector<LivePunkt> p = live.punkte(t0.addSecs(10), m, QStringLiteral("JN67VV"));
        QCOMPARE(p.size(), 5);
        QHash<QString, LivePunkt> nach;
        for (const LivePunkt& x : p) { nach.insert(x.g.rufzeichen, x); }
        QCOMPARE(nach.value(QStringLiteral("SP9XYZ")).art, LiveArt::NeuesFeld);
        QCOMPARE(nach.value(QStringLiteral("DL1ABC")).art, LiveArt::Gearbeitet);
        QCOMPARE(nach.value(QStringLiteral("F5NEW")).art, LiveArt::NeuesFeld);
        QCOMPARE(nach.value(QStringLiteral("ZS6XX")).art, LiveArt::NeuesLand);
        QVERIFY(nach.value(QStringLiteral("ZS6XX")).feldNeu);
        QCOMPARE(nach.value(QStringLiteral("G0OLD")).art, LiveArt::Gewoehnlich);
        // Die wichtigsten zuletzt (obenauf gemalt).
        QCOMPARE(p.last().art, LiveArt::NeuesLand);

        // Entfernung und Richtung vom eigenen Locator: SP9 liegt
        // nordoestlich, ZS6 suedlich.
        const LivePunkt sp = nach.value(QStringLiteral("SP9XYZ"));
        QVERIFY2(sp.km > 600.0 && sp.km < 850.0, qPrintable(QString::number(sp.km)));
        QVERIFY2(sp.azimutGrad > 20.0 && sp.azimutGrad < 80.0,
                 qPrintable(QString::number(sp.azimutGrad)));
        const LivePunkt zs = nach.value(QStringLiteral("ZS6XX"));
        QVERIFY(zs.azimutGrad > 150.0 && zs.azimutGrad < 200.0);

        // Drei verschiedene neue Felder auf 20 m (KO02, JN18, KG33).
        QCOMPARE(LiveSchicht::neueFelder(p, QStringLiteral("20m")), 3);
        QCOMPARE(LiveSchicht::neueFelder(p, QStringLiteral("40m")), 0);
        QCOMPARE(LiveSchicht::zaehlerText(3, QStringLiteral("20m")),
                 QStringLiteral("3 new squares heard now on 20m"));
        QCOMPARE(LiveSchicht::zaehlerText(1, QStringLiteral("20m")),
                 QStringLiteral("1 new square heard now on 20m"));

        // Zwei Stationen im selben neuen Feld sind EIN Feld.
        live.nimm(gehoert(QStringLiteral("SP9QQQ"), QStringLiteral("KO02"), t0));
        QCOMPARE(LiveSchicht::neueFelder(live.punkte(t0, m, QStringLiteral("JN67VV")),
                                         QStringLiteral("20m")), 3);

        // Der Hinweis: Rufzeichen, Locator, Rapport, Entfernung,
        // Richtung, Alter -- und was die Station fuer mich ist.
        const QString h = LiveSchicht::hinweis(sp, t0.addSecs(40));
        QVERIFY2(h.contains(QStringLiteral("SP9XYZ")) && h.contains(QStringLiteral("KO02"))
                     && h.contains(QStringLiteral("-14 dB")) && h.contains(QStringLiteral(" km"))
                     && h.contains(QStringLiteral("°")) && h.contains(QStringLiteral("40 s ago"))
                     && h.contains(QStringLiteral("new square on 20m")),
                 qPrintable(h));

        // Eine Minute spaeter schickt SP9XYZ einen Rapport -- ohne
        // Locator. Er bleibt in seinem Feld, Zeit und Zeile sind neu.
        const Ft8Gehoert rapport = gehoert(QStringLiteral("SP9XYZ"), QString(), t0.addSecs(60), -11);
        live.nimm(rapport);
        QCOMPARE(live.finde(QStringLiteral("SP9XYZ"))->zeile, rapport.zeile);

        // Altern: 121 s nach t0 sind alle weg ausser SP9XYZ.
        live.altere(t0.addSecs(121));
        QCOMPARE(live.anzahl(), 1);
        p = live.punkte(t0.addSecs(121), m, QStringLiteral("JN67VV"));
        QCOMPARE(p.size(), 1);
        QCOMPARE(p.first().locator, QStringLiteral("KO02"));
        QCOMPARE(p.first().g.snrDb, -11);

        // Das Fenster ist einstellbar -- in Grenzen.
        live.setFensterSekunden(5);
        QCOMPARE(live.fensterSekunden(), LiveSchicht::kMinSekunden);
        live.setFensterSekunden(600);
        QCOMPARE(live.fensterSekunden(), 600);
    }

    // ── Der Klick auf der Karte = der Doppelklick in der Liste ──────
    void kartenKlickGehtDenselbenWegWieDerDoppelklick()
    {
        eigeneStation();
        RadioModel radio;
        radio.installFt8TxForTest();
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14074000.0);
        slice->setDspMode(DSPMode::DIGU);
        Ft8Applet applet(&radio);
        applet.setProtocol(Ft8Decoder::Protocol::Ft8);
        applet.setSlice(slice);
        QCOMPARE(applet.kartenSperre(), QString());

        // Dieselbe Verdrahtung wie im Hauptfenster.
        Ft8KartenBruecke::instanz().verbinde(&applet);

        QsoMapWindow karte;
        karte.resize(1100, 700);
        karte.imagery()->setNetworkEnabled(false);
        karte.setHomeGrid(QStringLiteral("JN67VV"));
        karte.setEntries(kleinesLog());
        karte.show();
        QVERIFY(QTest::qWaitForWindowExposed(&karte, kFensterFrist));

        QSignalSpy angenommen(&applet, &Ft8Applet::fundAngenommen);
        QSignalSpy gehoertSpy(&applet, &Ft8Applet::gehoert);

        // Ein Fund kommt herein, als waere er dekodiert worden.
        applet.fundForTest(fund(QStringLiteral("CQ SP9XYZ KO02"), -8.0f));
        QCOMPARE(gehoertSpy.count(), 1);
        const Ft8Gehoert g = gehoertSpy.first().first().value<Ft8Gehoert>();
        QCOMPARE(g.rufzeichen, QStringLiteral("SP9XYZ"));
        QCOMPARE(g.locator, QStringLiteral("KO02"));
        QCOMPARE(g.band, QStringLiteral("20m"));
        QCOMPARE(g.zeile.trimmed(), applet.listForTest()->toPlainText().trimmed()
                                        .section(QLatin1Char('\n'), -1).trimmed());

        // Er ist auf beiden Karten -- als neues Feld auf 20 m.
        FlatMapWidget* flach = karte.flatMapForTest();
        AzimutalKarteWidget* az = karte.azimutalForTest();
        QCOMPARE(flach->livePunkte().size(), 1);
        QCOMPARE(az->livePunkte().size(), 1);
        QCOMPARE(flach->livePunkte().first().art, LiveArt::NeuesFeld);
        QCOMPARE(karte.liveZaehlerForTest()->text(),
                 QStringLiteral("1 new square heard now on 20m"));
        QVERIFY(karte.liveZaehlerForTest()->isVisibleTo(&karte));

        // ── B: der DOPPELklick auf der flachen Karte ──
        //
        // Geaendert am 2026-11-11 auf Ansage des Betreibers: ein
        // einzelner Klick darf keine Taste druecken. Erst der zweite
        // ruft an -- derselbe Handgriff wie in der Fundliste.
        QSignalSpy klickSpy(flach, &FlatMapWidget::livePunktGeklickt);
        klick(flach, flach->livePunktBildpunkt(0));
        QVERIFY2(klickSpy.count() == 0,
                 "ein einzelner Klick darf NICHT anrufen");
        QCOMPARE(angenommen.count(), 0);
        doppelklick(flach, flach->livePunktBildpunkt(0));
        QCOMPARE(klickSpy.count(), 1);
        QCOMPARE(angenommen.count(), 1);
        const QString zeileKarte = angenommen.at(0).at(0).toString();
        const QString partnerKarte = angenommen.at(0).at(1).toString();
        QCOMPARE(partnerKarte, QStringLiteral("SP9XYZ"));
        QCOMPARE(applet.textFeldForTest()->text(), QStringLiteral("SP9XYZ OE5SOS JN67"));
        QVERIFY(applet.ablaufForTest().laeuft());
        const Ft8Ablauf::Zustand zustandKarte = applet.ablaufForTest().zustand();
        QCOMPARE(applet.ablaufForTest().partnerLocator(), QStringLiteral("KO02"));
        applet.abbrechen();
        applet.textFeldForTest()->clear();

        // ── A: der Doppelklick in der Liste, auf dieselbe Zeile ──
        doppelklickInDieListe(applet);
        QCOMPARE(angenommen.count(), 2);
        QCOMPARE(angenommen.at(1).at(0).toString(), zeileKarte);
        QCOMPARE(angenommen.at(1).at(1).toString(), partnerKarte);
        QCOMPARE(applet.textFeldForTest()->text(), QStringLiteral("SP9XYZ OE5SOS JN67"));
        QCOMPARE(applet.ablaufForTest().zustand(), zustandKarte);
        applet.abbrechen();

        // ── Und die Azimutalkarte, derselbe Weg ──
        QSignalSpy azKlick(az, &AzimutalKarteWidget::livePunktGeklickt);
        klick(az, az->livePunktBildpunkt(0));
        QCOMPARE(azKlick.count(), 1);
        QCOMPARE(angenommen.count(), 3);
        QCOMPARE(angenommen.at(2).at(0).toString(), zeileKarte);
        applet.abbrechen();

        Ft8KartenBruecke::instanz().verbinde(nullptr);
    }

    // ── Ohne FT8 sagt der Klick, warum nicht ────────────────────────
    void ohneFt8ErklaertDerKlickWarum()
    {
        eigeneStation();
        RadioModel radio;
        radio.installFt8TxForTest();
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14074000.0);
        slice->setDspMode(DSPMode::DIGU);
        Ft8Applet applet(&radio);
        applet.setSlice(slice);
        Ft8KartenBruecke::instanz().verbinde(&applet);

        QsoMapWindow karte;
        karte.resize(1100, 700);
        karte.imagery()->setNetworkEnabled(false);
        karte.setHomeGrid(QStringLiteral("JN67VV"));
        karte.setEntries(kleinesLog());

        applet.fundForTest(fund(QStringLiteral("CQ SP9XYZ KO02")));
        QCOMPARE(karte.flatMapForTest()->livePunkte().size(), 1);

        // Der Empfaenger geht auf USB: kein FT8-Betrieb mehr.
        slice->setDspMode(DSPMode::USB);
        QSignalSpy angenommen(&applet, &Ft8Applet::fundAngenommen);
        FlatMapWidget* flach = karte.flatMapForTest();
        klick(flach, flach->livePunktBildpunkt(0));
        QCOMPARE(angenommen.count(), 0);
        QVERIFY(!applet.ablaufForTest().laeuft());
        QVERIFY(!applet.sendetGerade());
        const QString grund = karte.rufeLive(QStringLiteral("SP9XYZ"));
        QVERIFY2(grund.contains(QStringLiteral("FT8 is not active")), qPrintable(grund));
        QVERIFY2(karte.infoTextForTest().contains(QStringLiteral("Cannot call SP9XYZ")),
                 qPrintable(karte.infoTextForTest()));
        QCOMPARE(angenommen.count(), 0);

        // Zurueck auf DIGU, aber ohne eigenes Rufzeichen.
        slice->setDspMode(DSPMode::DIGU);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"), QString());
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"), QString());
        QVERIFY2(karte.rufeLive(QStringLiteral("SP9XYZ")).contains(QStringLiteral("callsign")),
                 qPrintable(karte.rufeLive(QStringLiteral("SP9XYZ"))));
        QCOMPARE(angenommen.count(), 0);
        eigeneStation();

        // Ausserhalb des Bandes: der Bandplan sagt nein -- vorher, nicht
        // erst beim Tasten. Derselbe Waechter, den das Verbinden einhaengt.
        radio.installBandPlanMoxCheckForTest();
        QVERIFY2(applet.kartenSperre().isEmpty(), qPrintable(applet.kartenSperre()));
        slice->setFrequency(14500000.0);
        const QString bandplan = applet.kartenSperre();
        QVERIFY2(bandplan.contains(QStringLiteral("transmit not allowed")),
                 qPrintable(bandplan));
        QVERIFY(karte.rufeLive(QStringLiteral("SP9XYZ")).contains(QStringLiteral("transmit not allowed")));
        QCOMPARE(angenommen.count(), 0);
        slice->setFrequency(14074000.0);
        QVERIFY(applet.kartenSperre().isEmpty());

        // Die Tastensperre der SWR-Schutzschaltung ebenso.
        radio.moxController()->latchKeyBlock(QStringLiteral("SWR protection: open antenna"));
        QVERIFY2(applet.kartenSperre().contains(QStringLiteral("SWR protection")),
                 qPrintable(applet.kartenSperre()));
        radio.moxController()->releaseKeyBlock();
        QVERIFY(applet.kartenSperre().isEmpty());

        // Wer gerade nicht mehr gehoert wird, ist nicht anzurufen.
        QVERIFY(karte.rufeLive(QStringLiteral("K1ZZZ")).contains(QStringLiteral("no longer heard")));

        // Und ohne FT8-Fenster ueberhaupt.
        Ft8KartenBruecke::instanz().verbinde(nullptr);
        QVERIFY(karte.rufeLive(QStringLiteral("SP9XYZ")).contains(QStringLiteral("not running")));
        QCOMPARE(angenommen.count(), 0);
    }

    // ── Der eine Knopf: „Locator hunt" ─────────────────────────────
    //
    // Betreiber am 2026-11-11, nachdem er das Mosaik dreimal nicht
    // gefunden hatte: „es muss eine weltkarte sein, mit der ich sofort
    // sehe, was noch nicht erreicht ist und wo aktuell gesendet wird" --
    // und davor „wenn man dies nicht weiss...". Der Knopf muss die ganze
    // Buehne stellen, nicht einen Haken umlegen.
    void derJagdknopfStelltDieGanzeBuehne()
    {
        QsoMapWindow karte;
        karte.resize(1100, 700);
        karte.imagery()->setNetworkEnabled(false);
        karte.setHomeGrid(QStringLiteral("JN67VV"));
        karte.setEntries(kleinesLog());

        // So, wie es beim Betreiber stand: Pfade und Luftbild an,
        // Gitter aus.
        karte.pathsForTest()->setChecked(true);
        karte.imageryForTest()->setChecked(true);
        karte.gridForTest()->setChecked(false);
        karte.nurFt8ForTest()->setChecked(false);

        QVERIFY(karte.jagdButtonForTest() != nullptr);
        karte.jagdButtonForTest()->setChecked(true);

        // Alles auf einmal, ohne Vorwissen:
        QVERIFY2(karte.gridForTest()->isChecked(), "Mosaik muss an sein");
        QVERIFY2(karte.nurFt8ForTest()->isChecked(), "FT8 only muss an sein");
        QVERIFY2(!karte.pathsForTest()->isChecked(),
                 "9313 Pfade decken das Mosaik zu");
        QVERIFY2(!karte.imageryForTest()->isChecked(),
                 "das Satellitenfoto decke das Mosaik zu");
        QCOMPARE(karte.currentViewForTest(), 1);   // kViewFlat: die Weltkarte
        QVERIFY2(karte.flatMapForTest()->jagdModus(),
                 "das Mosaik muss decken, nicht tuschen");

        // Und wieder zurueck: der Knopf raeumt auf, er wirft nichts weg.
        karte.jagdButtonForTest()->setChecked(false);
        QVERIFY(karte.pathsForTest()->isChecked());
        QVERIFY(karte.imageryForTest()->isChecked());
        QVERIFY(!karte.gridForTest()->isChecked());
        QVERIFY(!karte.nurFt8ForTest()->isChecked());
        QVERIFY(!karte.flatMapForTest()->jagdModus());
    }

    // Der Knopf gehoert NICHT zu den Schaltern, die im 3D-Bild
    // zuruecktreten -- einer seiner Handgriffe ist ja, von dort
    // herauszuholen. Genau daran ist der Betreiber haengengeblieben:
    // der `Grid`-Haken war im 3D unsichtbar.
    void derJagdknopfBleibtAuchIm3dSichtbar()
    {
        QsoMapWindow karte;
        karte.imagery()->setNetworkEnabled(false);
        const QVector<QWidget*>& nurFlach = karte.flatOnlyControlsForTest();
        QVERIFY2(nurFlach.contains(karte.gridForTest()),
                 "der Grid-Haken verschwindet im 3D -- das ist der Befund");
        QVERIFY2(!nurFlach.contains(karte.jagdButtonForTest()),
                 "der Jagdknopf darf im 3D NICHT verschwinden");
    }

    // Im Jagdmodus deckt das Mosaik. Gemessen, nicht behauptet.
    void imJagdmodusDecktDasMosaik()
    {
        for (FeldStand f : {FeldStand::Bestaetigt, FeldStand::Gearbeitet}) {
            const int leise = LocatorJagdZeichen::feldFuellung(f, false).alpha();
            const int laut  = LocatorJagdZeichen::feldFuellung(f, true).alpha();
            QVERIFY2(laut > leise + 60,
                     qPrintable(QStringLiteral("leise=%1 laut=%2")
                                    .arg(leise).arg(laut)));
            QVERIFY(laut <= 255);
        }
        // Was nie gearbeitet wurde, bleibt in BEIDEN Staerken leer --
        // sonst waere die Luecke keine Luecke mehr.
        QCOMPARE(LocatorJagdZeichen::feldFuellung(FeldStand::Nie, true).alpha(), 0);
    }

    // Vollbild: nur im eigenen Fenster, und Escape fuehrt zurueck
    // statt die Karte zuzumachen (Betreiber 2026-11-11).
    void vollbildNurImEigenenFenster()
    {
        QsoMapWindow karte;
        karte.imagery()->setNetworkEnabled(false);
        QVERIFY(karte.vollbildButtonForTest() != nullptr);

        // Eingebettet in die Logbuchspalte: kein Vollbild, dafuer der
        // Pfeil, der ins eigene Fenster fuehrt.
        karte.setEmbedded(true);
        QVERIFY2(!karte.vollbildButtonForTest()->isVisibleTo(&karte),
                 "in der Spalte gibt es nichts zu vergroessern");

        karte.setEmbedded(false);
        QVERIFY(karte.vollbildButtonForTest()->isVisibleTo(&karte));
        QVERIFY(!karte.isFullScreen());
        karte.vollbildButtonForTest()->setChecked(true);
        QVERIFY2(karte.isFullScreen(), "der Knopf muss das Fenster fuellen");
        karte.vollbildButtonForTest()->setChecked(false);
        QVERIFY(!karte.isFullScreen());
    }

    // Die Jagdliste: nur was fehlt, in der richtigen Reihenfolge, und
    // die Peilungsspalte dreht statt anzurufen.
    void dieJagdlisteZeigtNurDieLuecken()
    {
        QsoMapWindow karte;
        karte.resize(1400, 800);
        karte.imagery()->setNetworkEnabled(false);
        karte.setHomeGrid(QStringLiteral("JN67VV"));
        karte.setEntries(kleinesLog());
        QDateTime uhr(QDate(2026, 11, 11), QTime(6, 0, 0), QTimeZone::UTC);
        karte.setUhrForTest([&uhr]() { return uhr; });
        karte.jagdButtonForTest()->setChecked(true);
        // Ohne „FT8 only": sonst zaehlt das SSB-QSO in JO31 nicht und
        // JO31 waere auf 40 m eine Luecke statt einer fehlenden QSL.
        karte.nurFt8ForTest()->setChecked(false);

        // JO31 ist auf 40 m gearbeitet, aber nicht bestaetigt (kleinesLog),
        // KO02 ist nie gearbeitet. Beide senden.
        karte.nimmFt8(gehoert(QStringLiteral("SP9XYZ"), QStringLiteral("KO02"), uhr,
                              -10, false, QStringLiteral("40m")));
        karte.nimmFt8(gehoert(QStringLiteral("DL1ABC"), QStringLiteral("JO31"), uhr,
                              -10, false, QStringLiteral("40m")));
        // JN78 ist auf 20 m bestaetigt -- wer von dort sendet, gehoert
        // auf gar keine Liste, deren Zweck die Luecke ist.
        karte.nimmFt8(gehoert(QStringLiteral("OE3RPU"), QStringLiteral("JN78"), uhr));

        QTableWidget* liste = karte.jagdListeForTest();
        QVERIFY(liste != nullptr);
        QStringList rufe;
        for (int r = 0; r < liste->rowCount(); ++r) {
            rufe << liste->item(r, 0)->data(Qt::UserRole).toString();
        }
        QVERIFY2(!rufe.contains(QStringLiteral("OE3RPU")),
                 qPrintable(QStringLiteral("bestaetigt gehoert nicht in die "
                                           "Liste: %1").arg(rufe.join(QLatin1Char(' ')))));
        QVERIFY(rufe.contains(QStringLiteral("SP9XYZ")));
        // Die fehlende Luecke steht VOR der fehlenden QSL.
        QVERIFY2(rufe.indexOf(QStringLiteral("SP9XYZ"))
                     < rufe.indexOf(QStringLiteral("DL1ABC")),
                 qPrintable(rufe.join(QLatin1Char(' '))));

        // Ein Doppelklick in die Peilungsspalte dreht, statt anzurufen.
        QSignalSpy dreh(&karte, &QsoMapWindow::rotorZielGewuenscht);
        const int zeile = rufe.indexOf(QStringLiteral("SP9XYZ"));
        QVERIFY(zeile >= 0);
        QTableWidgetItem* az = liste->item(zeile, 4);
        QVERIFY(az != nullptr);
        QVERIFY2(az->data(Qt::UserRole).isValid(),
                 "ohne Peilung kann die Spalte nichts drehen");
        emit liste->itemDoubleClicked(az);
        QCOMPARE(dreh.count(), 1);
        QCOMPARE(dreh.at(0).at(1).toString(), QStringLiteral("SP9XYZ"));

        // Der Jagdmodus legt `FT8 only` um und merkt es sich; wer ihn
        // angelassen stehen laesst, veraendert den naechsten Prueflauf.
        karte.jagdButtonForTest()->setChecked(false);
    }

    // Der Hinweis meldet nur, was NEU dazukommt -- nicht bei jedem
    // Auffrischen dieselben Stationen.
    void derHinweisMeldetNurNeues()
    {
        QsoMapWindow karte;
        karte.resize(1400, 800);
        karte.imagery()->setNetworkEnabled(false);
        karte.setHomeGrid(QStringLiteral("JN67VV"));
        karte.setEntries(kleinesLog());
        QDateTime uhr(QDate(2026, 11, 11), QTime(6, 0, 0), QTimeZone::UTC);
        karte.setUhrForTest([&uhr]() { return uhr; });
        karte.jagdButtonForTest()->setChecked(true);

        QSignalSpy alarm(&karte, &QsoMapWindow::jagdAlarm);
        karte.nimmFt8(gehoert(QStringLiteral("SP9XYZ"), QStringLiteral("KO02"), uhr));
        QCOMPARE(alarm.count(), 1);
        QCOMPARE(alarm.at(0).at(0).toString(), QStringLiteral("SP9XYZ"));

        // Dieselbe Station noch einmal: kein zweiter Hinweis.
        karte.nimmFt8(gehoert(QStringLiteral("SP9XYZ"), QStringLiteral("KO02"), uhr));
        QCOMPARE(alarm.count(), 1);

        // Eine andere, aus einem anderen fehlenden Feld: ja.
        karte.nimmFt8(gehoert(QStringLiteral("LZ1XX"), QStringLiteral("KN22"), uhr));
        QCOMPARE(alarm.count(), 2);

        // Eine Station aus einem BESTAETIGTEN Feld meldet gar nichts.
        karte.nimmFt8(gehoert(QStringLiteral("OE3RPU"), QStringLiteral("JN78"), uhr));
        QCOMPARE(alarm.count(), 2);

        karte.jagdButtonForTest()->setChecked(false);
    }

    // ── Die Karte: Mosaik am Grid, Zaehler, Altern ──────────────────
    void karteZeigtMosaikZaehlerUndAltert()
    {
        QsoMapWindow karte;
        karte.resize(1100, 700);
        karte.imagery()->setNetworkEnabled(false);
        karte.setHomeGrid(QStringLiteral("JN67VV"));
        karte.setEntries(kleinesLog());
        QDateTime uhr(QDate(2026, 11, 11), QTime(6, 0, 0), QTimeZone::UTC);
        karte.setUhrForTest([&uhr]() { return uhr; });

        // Vor dem ersten Fund: kein Zaehler, Mosaik ueber alle Baender.
        QVERIFY(!karte.liveZaehlerForTest()->isVisibleTo(&karte));
        QCOMPARE(karte.mosaikBand(), QString());

        karte.gridForTest()->setChecked(true);
        QVERIFY(karte.flatMapForTest()->showGrid());
        QVERIFY(karte.azimutalForTest()->showMosaik());
        QCOMPARE(karte.flatMapForTest()->mosaikAnzahl(), 3);

        karte.nimmFt8(gehoert(QStringLiteral("SP9XYZ"), QStringLiteral("KO02"), uhr));
        karte.nimmFt8(gehoert(QStringLiteral("F5NEW"), QStringLiteral("JN18"), uhr));
        karte.nimmFt8(gehoert(QStringLiteral("DL1ABC"), QStringLiteral("JO31"), uhr));
        // Das Mosaik folgt dem FT8-Band.
        QCOMPARE(karte.mosaikBand(), QStringLiteral("20m"));
        QVERIFY(karte.mosaikBandForTest()->currentText().contains(QStringLiteral("20m")));
        QCOMPARE(karte.flatMapForTest()->mosaikAnzahl(), 3);   // 20 m: JO31, JN78, IO91
        QCOMPARE(karte.liveZaehlerForTest()->text(),
                 QStringLiteral("2 new squares heard now on 20m"));
        QVERIFY(karte.liveZaehlerForTest()->isVisibleTo(&karte));

        // Nur FT8: IO91 (SSB) faellt aus dem Mosaik.
        karte.nurFt8ForTest()->setChecked(true);
        QCOMPARE(karte.flatMapForTest()->mosaikAnzahl(), 2);
        karte.nurFt8ForTest()->setChecked(false);

        // Ein festes Band gewaehlt: 40 m.
        const int i40 = karte.mosaikBandForTest()->findData(QStringLiteral("40m"));
        QVERIFY(i40 > 0);
        karte.mosaikBandForTest()->setCurrentIndex(i40);
        emit karte.mosaikBandForTest()->activated(i40);
        QCOMPARE(karte.mosaikBand(), QStringLiteral("40m"));
        QCOMPARE(karte.flatMapForTest()->mosaikAnzahl(), 1);
        karte.mosaikBandForTest()->setCurrentIndex(0);
        emit karte.mosaikBandForTest()->activated(0);

        // Altern: drei Minuten spaeter ist das Band leer.
        uhr = uhr.addSecs(180);
        karte.refreshLiveForTest();
        QCOMPARE(karte.flatMapForTest()->livePunkte().size(), 0);
        QCOMPARE(karte.liveZaehlerForTest()->text(),
                 QStringLiteral("0 new squares heard now on 20m"));

        // Das Fenster auf fuenf Minuten: dieselben Funde waeren noch da.
        karte.nimmFt8(gehoert(QStringLiteral("SP9XYZ"), QStringLiteral("KO02"), uhr.addSecs(-200)));
        QCOMPARE(karte.flatMapForTest()->livePunkte().size(), 0);
        karte.liveMinutenForTest()->setValue(5);
        QCOMPARE(karte.liveForTest().fensterSekunden(), 300);
        karte.nimmFt8(gehoert(QStringLiteral("SP9XYZ"), QStringLiteral("KO02"), uhr.addSecs(-200)));
        QCOMPARE(karte.flatMapForTest()->livePunkte().size(), 1);
        karte.liveMinutenForTest()->setValue(2);
    }

    // ── Bilder fuer den Betreiber ───────────────────────────────────
    void bilderMitEinemErfundenenBand()
    {
        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (grabDir.isEmpty()) { QSKIP("LONGPATH_GRAB_DIR nicht gesetzt"); }
        const QString welt = qEnvironmentVariable("LONGPATH_WELTBILD");
        if (!welt.isEmpty()) { QVERIFY(WorldTexture::setPath(welt)); }
        QDir().mkpath(grabDir);

        // Ein Logbuch, wie es nach einigen Jahren 20 m aussieht: Europa
        // dicht, Nordamerika ostlastig, der Rest duenn.
        QVector<LogEntry> log;
        QRandomGenerator z(20261111);
        struct Haufen { double lat, lon, sLat, sLon; int n; };
        const Haufen haufen[] = {
            {50.0, 10.0, 6.0, 10.0, 260}, {45.0, 25.0, 5.0, 8.0, 80},
            {55.0, 37.0, 4.0, 8.0, 40},   {40.0, -78.0, 4.0, 7.0, 90},
            {35.0, -100.0, 6.0, 15.0, 40}, {-25.0, -50.0, 6.0, 8.0, 20},
            {36.0, 138.0, 3.0, 4.0, 25},  {-33.0, 150.0, 4.0, 5.0, 12},
            {60.0, 90.0, 6.0, 25.0, 15},  {30.0, 31.0, 4.0, 4.0, 10},
        };
        int nr = 0;
        const auto normal = [&z]() {
            const double u1 = std::max(1e-12, z.generateDouble());
            return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * z.generateDouble());
        };
        for (const Haufen& h : haufen) {
            for (int i = 0; i < h.n; ++i) {
                const double lat = std::clamp(h.lat + normal() * h.sLat, -85.0, 85.0);
                const double lon = std::remainder(h.lon + normal() * h.sLon, 360.0);
                ++nr;
                LogEntry e = qso(QStringLiteral("LG%1").arg(nr),
                                 FlatMapWidget::gridSquare4(lat, lon),
                                 QStringLiteral("20m"), QStringLiteral("FT8"), (nr % 3) == 0);
                // Juengst, damit die Strahlen im Zeitraum der Karte liegen.
                e.timeOn = QDateTime::currentDateTimeUtc().addSecs(-60 * nr);
                log << e;
            }
        }

        // Die flache Karte zuerst -- die Ansicht wird beim Bau gelesen.
        AppSettings::instance().setValue(QStringLiteral("QsoMapView"), QStringLiteral("flat"));
        QsoMapWindow karte;
        karte.resize(1300, 820);
        karte.imagery()->setNetworkEnabled(false);
        karte.setHomeGrid(QStringLiteral("JN67VV"));
        karte.setEntries(log);
        const QDateTime jetzt = QDateTime::currentDateTimeUtc();
        karte.setUhrForTest([jetzt]() { return jetzt; });
        karte.show();
        QVERIFY(QTest::qWaitForWindowExposed(&karte, kFensterFrist));
        karte.gridForTest()->setChecked(true);
        // Ohne die Wege des Logbuchs: bei hunderten QSOs ueberdecken sie
        // das Mosaik. Der Haken bleibt beim Betreiber.
        for (QCheckBox* c : karte.findChildren<QCheckBox*>()) {
            if (c->text() == QStringLiteral("Paths")) { c->setChecked(false); }
        }

        // Ein 20-m-Band am Abend: was ein FT8-Fenster in zwei Minuten hoert.
        struct Station { const char* call; const char* loc; int snr; bool land; int vor; };
        const Station band[] = {
            {"SP9XYZ", "KO02", -14, false, 10}, {"F5NEW", "JN18", -18, false, 25},
            {"DL1ABC", "JO31", -3, false, 40},  {"G4XYZ", "IO91", -9, false, 55},
            {"EA8ZZ", "IL18", -16, false, 15},  {"4X1AB", "KM72", -19, false, 70},
            {"ZS6XX", "KG33", -20, true, 30},   {"PY2AA", "GG66", -12, false, 85},
            {"W1AW", "FN31", -21, false, 20},   {"VE3XX", "FN03", -17, false, 100},
            {"JA1ABC", "PM95", -22, false, 45}, {"UA9AAA", "MO06", -15, false, 60},
            {"OH2BB", "KP20", -6, false, 35},   {"LZ1XX", "KN22", -11, false, 5},
            {"YB0ZZ", "OI33", -23, true, 50},   {"HB9AA", "JN47", -7, false, 75},
            {"OK1XX", "JO70", -10, false, 95},  {"S51AA", "JN76", -4, false, 110},
        };
        for (const Station& s : band) {
            Ft8Gehoert g = gehoert(QString::fromLatin1(s.call), QString::fromLatin1(s.loc),
                                   jetzt.addSecs(-s.vor), s.snr, s.land);
            g.land = QString();
            karte.nimmFt8(g);
        }
        QTest::qWait(100);

        // Flache Karte.
        QCOMPARE(karte.currentViewForTest(), 1);
        QTest::qWait(100);
        QVERIFY(karte.flatMapForTest()->grab().save(grabDir + QStringLiteral("/ft8-jagd-flach.png")));
        // Bei voller Welt sind die Felder kleiner als ein Punkt -- dann
        // kein Rahmen.
        QCOMPARE(karte.flatMapForTest()->neueFelderGemaltForTest(), 0);

        // Dasselbe im Jagdmodus: EIN Knopf, und die Weltkarte zeigt, was
        // fehlt und wer gerade sendet (Betreiber 2026-11-11).
        karte.jagdButtonForTest()->setChecked(true);
        QTest::qWait(150);
        QVERIFY(karte.flatMapForTest()->jagdModus());
        QVERIFY(karte.flatMapForTest()->grab().save(
            grabDir + QStringLiteral("/ft8-jagd-weltkarte.png")));
        QVERIFY(karte.grab().save(
            grabDir + QStringLiteral("/ft8-jagd-weltkarte-fenster.png")));
        karte.jagdButtonForTest()->setChecked(false);
        QTest::qWait(100);

        // Hineingezoomt auf Mitteleuropa: jetzt bekommen die neuen Felder
        // ihren Bernsteingrund.
        karte.flatMapForTest()->flyTo(50.0, 16.0, 5.0, 0);
        QTest::qWait(100);
        QVERIFY(karte.flatMapForTest()->grab().save(grabDir + QStringLiteral("/ft8-jagd-europa.png")));
        QVERIFY(karte.flatMapForTest()->neueFelderGemaltForTest() > 0);

        // Azimutalkarte.
        karte.azimutalButtonForTest()->setChecked(true);
        QTest::qWait(200);
        QVERIFY(karte.azimutalForTest()->grab().save(grabDir + QStringLiteral("/ft8-jagd-azimutal.png")));

        // Das ganze Fenster, mit Zaehler und Leiste.
        QVERIFY(karte.grab().save(grabDir + QStringLiteral("/ft8-jagd-fenster.png")));

        // Die Azimutalkarte gross, mit denselben Daten.
        {
            AzimutalKarteWidget gross;
            gross.resize(1100, 1100);
            double hLat = 0, hLon = 0;
            calculateLatLonFromGridSquare(QStringLiteral("JN67VV"), hLat, hLon);
            gross.setHome(hLat, hLon, QStringLiteral("JN67VV"));
            gross.setMosaik(karte.mosaikForTest().felder(QStringLiteral("20m")));
            gross.setShowMosaik(true);
            gross.setLivePunkte(karte.azimutalForTest()->livePunkte());
            gross.show();
            QVERIFY(QTest::qWaitForWindowExposed(&gross, kFensterFrist));
            QVERIFY(!gross.mosaikForTest().isNull());
            QVERIFY(gross.grab().save(grabDir + QStringLiteral("/ft8-jagd-azimutal-gross.png")));
        }
        karte.azimutalButtonForTest()->setChecked(false);

        // ── Entwuerfe fuer die Jagdtafel (2026-11-11) ────────────────
        //
        // Vier Blaetter, aus demselben Logbuch und denselben gehoerten
        // Stationen -- Betreiber: „mache mir grafische vorschlaege".
        // Jedes Blatt beantwortet dieselbe Frage anders: „sofort sehen,
        // was noch nicht erreicht ist und wo gerade gesendet wird".
        karte.resize(1600, 900);
        karte.jagdButtonForTest()->setChecked(true);
        QTest::qWait(250);
        QVERIFY(karte.jagdSpalteForTest()->isVisible());
        QVERIFY(karte.jagdListeForTest()->rowCount() > 0);

        // A: vierstellige Quadrate (so, wie es heute malt) + Liste.
        karte.flatMapForTest()->setGrosseFelder(false);
        QTest::qWait(120);
        QVERIFY(karte.grab().save(grabDir + QStringLiteral("/jagdtafel-A-quadrate-liste.png")));

        // B: Grossfelder statt Quadrate -- auf der Weltkarte lesbar.
        karte.flatMapForTest()->setGrosseFelder(true);
        QTest::qWait(120);
        QVERIFY(karte.grab().save(grabDir + QStringLiteral("/jagdtafel-B-grossfelder-liste.png")));

        // C: umgekehrt gedacht -- gefaerbt wird, was FEHLT.
        karte.flatMapForTest()->setLueckenBetonen(true);
        QTest::qWait(120);
        QVERIFY(karte.grab().save(grabDir + QStringLiteral("/jagdtafel-C-luecken-liste.png")));

        // D: nur die Karte, ohne Spalte -- alles Platz fuers Bild.
        karte.flatMapForTest()->setLueckenBetonen(false);
        karte.jagdSpalteForTest()->setVisible(false);
        QTest::qWait(120);
        QVERIFY(karte.grab().save(grabDir + QStringLiteral("/jagdtafel-D-nur-karte.png")));
        karte.jagdSpalteForTest()->setVisible(true);

        karte.flatMapForTest()->setGrosseFelder(false);
        karte.jagdButtonForTest()->setChecked(false);
        AppSettings::instance().setValue(QStringLiteral("QsoMapView"), QString{});
    }

    // ── „New grid" zeigt nur, was ganz fehlt (2026-11-13) ─────────────
    //
    // Betreiber am Geraet: „hier sind einige grids, die sicher schon
    // hatte -- hier sollen nur jene stehen, die ich noch nicht erreicht
    // habe."
    //
    // Belegt an seinen eigenen Zahlen, auf 20 m: JN79 hat er auf 15 m,
    // 2 m und 40 m, KP30 auf 15 m und 6 m, KO26 auf vier Baendern --
    // alle drei standen im Filter als „neu" da. Der Filter fragte bis
    // heute beide Stufen ab (`neuesGrid || neuesGridBand`), weil „wer
    // die Jagd einschaltet, das Band voll sehen will" (2026-11-12).
    //
    // Die zweite Stufe ist nicht weg, nur nicht mehr in diesem Filter:
    // ihre FARBE bleibt und ist in „All" zu sehen. Dieser Lauf prueft
    // beides -- was der Filter zeigt und dass die Farbe bleibt.
    void derGridfilterZeigtNurGanzFehlende()
    {
        RadioModel radio;
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14074000.0);    // 20 m
        slice->setDspMode(DSPMode::DIGU);

        Ft8Applet applet(&radio);
        applet.setProtocol(Ft8Decoder::Protocol::Ft8);
        applet.setSlice(slice);

        // JN79 steht im Logbuch -- aber auf 15 m, nicht auf 20 m. Genau
        // Martins Fall.
        QVector<LogEntry> log;
        log << qso(QStringLiteral("OK1ABC"), QStringLiteral("JN79"),
                   QStringLiteral("15m"), QStringLiteral("FT8"));
        applet.mosaikForTest().baue(log, false);
        applet.gridsForTest().merke(QStringLiteral("JN79"));
        QVERIFY2(!applet.gridsForTest().leer(), "ohne Grundlage markiert nichts");
        QVERIFY2(applet.mosaikForTest().stand(QStringLiteral("JN79"),
                                              QStringLiteral("20m"))
                     == Longpath::FeldStand::Nie,
                 "die Ausgangslage stimmt nicht: JN79 gilt auf 20 m als gearbeitet");

        auto fund = [&applet](const QString& text, double hz) {
            Ft8Decode d;
            d.text   = text;
            d.snrDb  = -10.0;
            d.freqHz = hz;
            applet.fundForTest(d);
        };
        fund(QStringLiteral("CQ OK1JRD JN79"), 800.0);   // hat er -- nur nicht auf 20 m
        fund(QStringLiteral("CQ JS6TWW PL55"), 1200.0);  // hat er nie

        QVERIFY(applet.listForTest());
        const QString alles = applet.listForTest()->toPlainText();
        QVERIFY2(alles.contains(QStringLiteral("OK1JRD")), qPrintable(alles));
        QVERIFY2(alles.contains(QStringLiteral("JS6TWW")), qPrintable(alles));

        // ── EINE Farbe, EINE Bedeutung (2026-11-17) ───────────────
        //
        // Bis heute war auch die Bandstufe blau, nur dunkler. Am Geraet
        // auf 17 m: „jn75 habe ich schon gearbeitet, ist aber hellblau"
        // -- und dann: „ich will eine farbe haben, die mir zeigt, welche
        // locator ich noch nicht gearbeitet habe." Blau ist jetzt allein
        // das Feld, das ganz fehlt; was er auf einem anderen Band hat,
        // sieht aus wie jedes andere CQ.
        const auto farbeVon = [&applet](const QString& ruf) {
            QTextDocument* doc = applet.listForTest()->document();
            for (QTextBlock b = doc->begin(); b.isValid(); b = b.next()) {
                if (!b.text().contains(ruf)) { continue; }
                QTextCursor c(b);
                c.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
                return c.charFormat().foreground().color();
            }
            return QColor();
        };
        const QColor blau(QString::fromLatin1(Style::kAccent));
        QCOMPARE(farbeVon(QStringLiteral("JS6TWW")), blau);
        QCOMPARE(farbeVon(QStringLiteral("OK1JRD")),
                 QColor(QString::fromLatin1(Style::kAmberText)));
        // Und kein drittes, abgedunkeltes Blau irgendwo in der Liste.
        QVERIFY(farbeVon(QStringLiteral("OK1JRD")) != blau.darker(145));

        // Und jetzt die Jagd.
        QPushButton* jagd = applet.sichtNeuesGridButtonForTest();
        QVERIFY(jagd);
        jagd->setChecked(true);
        jagd->click();
        QCoreApplication::processEvents();

        const QString gefiltert = applet.listForTest()->toPlainText();
        QVERIFY2(gefiltert.contains(QStringLiteral("JS6TWW")),
                 qPrintable(QStringLiteral("das nie gearbeitete Feld fehlt: %1")
                                .arg(gefiltert)));
        QVERIFY2(!gefiltert.contains(QStringLiteral("OK1JRD")),
                 qPrintable(QStringLiteral("JN79 hat er laengst — es gehoert nicht "
                                           "in die Jagd: %1").arg(gefiltert)));
    }
};

QTEST_MAIN(TstFt8Locatorjagd)
#include "tst_ft8_locatorjagd.moc"
