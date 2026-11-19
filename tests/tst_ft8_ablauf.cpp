// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_ft8_ablauf.cpp  (Longpath)
// =================================================================
// Der FT8-QSO-Ablauf, Zustand fuer Zustand.
//
// Ft8Ablauf sendet nicht, tastet nicht und kennt keine Uhr — sie sagt
// nur, was im naechsten Fenster zu senden waere. Genau darum laesst
// sich hier jeder Uebergang einzeln hinschreiben, ohne Fenster, ohne
// Funkgeraet und ohne zu warten.
//
// Geprueft werden beide Rollen vollstaendig durch, dazu die Faelle,
// in denen ein Ablauf NICHT weitergehen darf: ein Dritter, der
// dazwischenruft, eine Nachricht an jemand anderen, und das Aufgeben
// nach genug Wiederholungen.
//
// Modification history (Longpath):
//   2026-10-16 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>

#include "core/Ft8Ablauf.h"

using namespace Longpath;
using Z = Ft8Ablauf::Zustand;

class TstFt8Ablauf : public QObject
{
    Q_OBJECT

private slots:
    // ── Die ganze Strecke als Antworter ─────────────────────────────
    void alsAntworterDurch()
    {
        Ft8Ablauf a;
        a.starteAlsAntworter(QStringLiteral("DL1ABC"),
                             QStringLiteral("OE5SOS"),
                             QStringLiteral("JN67VV"), -12);

        QCOMPARE(a.zustand(), Z::RufeAn);
        QCOMPARE(a.naechsteNachricht(), QStringLiteral("DL1ABC OE5SOS JN67"));
        QVERIFY(a.laeuft());

        // Er gibt mir seinen Rapport.
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC -09"), -12));
        QCOMPARE(a.zustand(), Z::SendeRReport);
        QCOMPARE(a.empfangenerReport(), -9);
        QCOMPARE(a.naechsteNachricht(),
                 QStringLiteral("DL1ABC OE5SOS R-12"));

        // Er ist fertig.
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC RR73"), -12));
        QCOMPARE(a.zustand(), Z::Sende73);
        QCOMPARE(a.naechsteNachricht(), QStringLiteral("DL1ABC OE5SOS 73"));

        // Mein 73 ist hinaus — damit ist das QSO vollstaendig, auch
        // ohne dass er es noch einmal bestaetigt.
        a.fensterVerstrichen();
        QCOMPARE(a.zustand(), Z::Fertig);
        QVERIFY(!a.laeuft());
        QCOMPARE(a.gesendeterReport(), -12);
        QCOMPARE(a.empfangenerReport(), -9);
    }

    // ── Die ganze Strecke als Anrufer ───────────────────────────────
    void alsAnruferDurch()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"),
                           QStringLiteral("JN67VV"));

        QCOMPARE(a.zustand(), Z::RufeCq);
        QCOMPARE(a.naechsteNachricht(), QStringLiteral("CQ OE5SOS JN67"));
        QVERIFY(a.partner().isEmpty());

        // Jemand antwortet — der Rapport ist das, was ich von ihm messe.
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC JO31"), -07));
        QCOMPARE(a.partner(), QStringLiteral("DL1ABC"));
        QCOMPARE(a.zustand(), Z::SendeReport);
        QCOMPARE(a.naechsteNachricht(),
                 QStringLiteral("DL1ABC OE5SOS -07"));

        // Er bestaetigt und gibt seinen.
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC R-15"), -7));
        QCOMPARE(a.zustand(), Z::SendeRR73);
        QCOMPARE(a.empfangenerReport(), -15);
        QCOMPARE(a.naechsteNachricht(),
                 QStringLiteral("DL1ABC OE5SOS RR73"));

        a.fensterVerstrichen();
        QCOMPARE(a.zustand(), Z::Fertig);
    }

    // Ein positiver Rapport wird mit + und zwei Stellen geschrieben,
    // wie in WSJT-X.
    void einPositiverRapportStehtMitPlus()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC JO31"), 3));
        QCOMPARE(a.naechsteNachricht(), QStringLiteral("DL1ABC OE5SOS +03"));
    }

    // ── Sein Feldkenner, fuer das Logbuch ───────────────────────────
    void derLocatorDesPartnersWirdGemerkt()
    {
        // Als Anrufer: er steht in der Antwort auf mein CQ.
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        QVERIFY(a.partnerLocator().isEmpty());
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC JO31"), -7));
        QCOMPARE(a.partnerLocator(), QStringLiteral("JO31"));

        // Als Antworter: er kommt aus der angeklickten CQ-Zeile.
        Ft8Ablauf b;
        b.starteAlsAntworter(QStringLiteral("DL1ABC"),
                             QStringLiteral("OE5SOS"),
                             QStringLiteral("JN67"), -12,
                             QStringLiteral("JO31"));
        QCOMPARE(b.partnerLocator(), QStringLiteral("JO31"));
    }

    // Nicht jedes dritte Feld ist ein Feldkenner — „-12", „R-09",
    // „RR73" und „73" stehen an derselben Stelle. Ein geratener
    // Locator stuende sonst im Logbuch.
    void nurEchteFeldkennerZaehlen_data()
    {
        QTest::addColumn<QString>("feld");
        QTest::addColumn<bool>("gilt");
        QTest::newRow("JN67")   << QStringLiteral("JN67")   << true;
        QTest::newRow("JN67VV") << QStringLiteral("JN67VV") << true;
        QTest::newRow("jo31")   << QStringLiteral("jo31")   << true;
        QTest::newRow("-12")    << QStringLiteral("-12")    << false;
        QTest::newRow("R-09")   << QStringLiteral("R-09")   << false;
        QTest::newRow("RR73")   << QStringLiteral("RR73")   << false;
        QTest::newRow("73")     << QStringLiteral("73")     << false;
        QTest::newRow("Ruf")    << QStringLiteral("DL1ABC") << false;
        QTest::newRow("leer")   << QString()                << false;
    }

    void nurEchteFeldkennerZaehlen()
    {
        QFETCH(QString, feld);
        QFETCH(bool, gilt);
        QCOMPARE(Ft8Ablauf::wieLocator(feld), gilt);
    }

    // Ein Anrufer ohne Locator (es gibt CQ-Rufe ohne) darf keinen
    // erfinden.
    void ohneLocatorBleibtDasFeldLeer()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC"), -7));
        QVERIFY2(a.partnerLocator().isEmpty(),
                 "ohne Feldkenner darf keiner im Logbuch stehen");
    }

    // ── Was den Ablauf NICHT weiterbringen darf ─────────────────────

    void eineNachrichtAnJemandAnderenZaehltNicht()
    {
        Ft8Ablauf a;
        a.starteAlsAntworter(QStringLiteral("DL1ABC"),
                             QStringLiteral("OE5SOS"),
                             QStringLiteral("JN67"), -12);
        QVERIFY(!a.verarbeite(QStringLiteral("OE1XYZ DL1ABC -09"), -12));
        QCOMPARE(a.zustand(), Z::RufeAn);
    }

    // Laeuft das QSO schon mit DL1ABC, darf ein Dritter es nicht
    // uebernehmen — sonst wechselt der Ablauf mitten im Gespraech den
    // Partner.
    void einDritterUebernimmtDasQsoNicht()
    {
        Ft8Ablauf a;
        a.starteAlsAntworter(QStringLiteral("DL1ABC"),
                             QStringLiteral("OE5SOS"),
                             QStringLiteral("JN67"), -12);
        QVERIFY(!a.verarbeite(QStringLiteral("OE5SOS SP9XYZ -05"), -5));
        QCOMPARE(a.partner(), QStringLiteral("DL1ABC"));
        QCOMPARE(a.zustand(), Z::RufeAn);
    }

    // Auf ein CQ hin darf dagegen jeder der erste sein — aber nur der
    // erste.
    void beimCqGiltDerErsteAnrufer()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC JO31"), -7));
        QCOMPARE(a.partner(), QStringLiteral("DL1ABC"));
        QVERIFY(!a.verarbeite(QStringLiteral("OE5SOS SP9XYZ KO02"), -3));
        QCOMPARE(a.partner(), QStringLiteral("DL1ABC"));
    }

    // ── Aufgeben statt ewig rufen ───────────────────────────────────
    void ohneAntwortWirdAufgegeben()
    {
        Ft8Ablauf a;
        a.starteAlsAntworter(QStringLiteral("DL1ABC"),
                             QStringLiteral("OE5SOS"),
                             QStringLiteral("JN67"), -12);

        for (int i = 1; i < Ft8Ablauf::kMaxWiederholungen; ++i) {
            a.fensterVerstrichen();
            QVERIFY2(a.laeuft(), qPrintable(
                QStringLiteral("nach %1 Fenstern schon aufgegeben").arg(i)));
            QCOMPARE(a.wiederholungen(), i);
        }
        a.fensterVerstrichen();
        QCOMPARE(a.zustand(), Z::Abgebrochen);
        QVERIFY(!a.laeuft());
        QVERIFY2(a.naechsteNachricht().isEmpty(),
                 "ein abgebrochener Ablauf darf nichts mehr zu senden haben");
    }

    // Eine passende Antwort setzt den Zaehler zurueck — sonst gibt ein
    // langes QSO mitten im Gespraech auf.
    void einePassendeAntwortSetztDenZaehlerZurueck()
    {
        Ft8Ablauf a;
        a.starteAlsAntworter(QStringLiteral("DL1ABC"),
                             QStringLiteral("OE5SOS"),
                             QStringLiteral("JN67"), -12);
        a.fensterVerstrichen();
        a.fensterVerstrichen();
        QCOMPARE(a.wiederholungen(), 2);
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC -09"), -12));
        QCOMPARE(a.wiederholungen(), 0);
    }

    // ── Der CQ-Ruf laeuft so oft, wie der Betreiber sagt ────────────
    //
    // Gegenprobe zur alten Fassung: dort zaehlte `fensterVerstrichen`
    // starr bis `kMaxWiederholungen` (fuenf), auch wenn im Fenster
    // „x16" stand. Am Geraet gemessen (2026-10-29, 12:45:00 bis
    // 12:47:12): genau fuenf Sendungen, danach war Schluss.
    void derCqRufFolgtDerEingestelltenZahl()
    {
        Ft8Ablauf a;
        a.setzeCqWiederholungen(16);
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        QCOMPARE(a.zustand(), Z::RufeCq);

        // Die Fuenf, an der die alte Fassung abbrach, muss durchlaufen.
        for (int i = 1; i < 16; ++i) {
            a.fensterVerstrichen();
            QVERIFY2(a.laeuft(), qPrintable(
                QStringLiteral("nach %1 CQ-Fenstern schon aufgegeben "
                               "(eingestellt waren 16)").arg(i)));
        }
        a.fensterVerstrichen();
        QCOMPARE(a.zustand(), Z::Abgebrochen);
    }

    // Und das Gegenstueck: mitten im QSO bleibt es bei den fuenf
    // Zuegen, ganz gleich was fuer den Ruf eingestellt ist.
    void dieEingestellteZahlGiltNurFuerDenRuf()
    {
        Ft8Ablauf a;
        a.setzeCqWiederholungen(30);
        a.starteAlsAntworter(QStringLiteral("DL1ABC"),
                             QStringLiteral("OE5SOS"),
                             QStringLiteral("JN67"), -12);
        for (int i = 1; i < Ft8Ablauf::kMaxWiederholungen; ++i) {
            a.fensterVerstrichen();
            QVERIFY(a.laeuft());
        }
        a.fensterVerstrichen();
        QCOMPARE(a.zustand(), Z::Abgebrochen);
    }

    // Eine Null wuerde den Ruf im ersten Fenster abwuergen.
    void unsinnigeZahlenWerdenEingefangen()
    {
        Ft8Ablauf a;
        a.setzeCqWiederholungen(0);
        QCOMPARE(a.cqWiederholungen(), 1);
        a.setzeCqWiederholungen(-3);
        QCOMPARE(a.cqWiederholungen(), 1);
        a.setzeCqWiederholungen(999);
        QCOMPARE(a.cqWiederholungen(), 30);
    }

    void abbrechenHaeltSofortAn()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        QVERIFY(a.laeuft());
        a.abbrechen();
        QCOMPARE(a.zustand(), Z::Abgebrochen);
        QVERIFY(a.naechsteNachricht().isEmpty());
        // Und danach bringt ihn nichts mehr in Gang.
        QVERIFY(!a.verarbeite(QStringLiteral("OE5SOS DL1ABC JO31"), -7));
    }

    // Ohne eigenes Rufzeichen faengt gar nichts an — sonst stuenden
    // Nachrichten mit einer Luecke auf dem Band.
    void ohneRufzeichenFaengtNichtsAn()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QString(), QStringLiteral("JN67"));
        QCOMPARE(a.zustand(), Z::Ruht);
        QVERIFY(!a.laeuft());
        QVERIFY(a.naechsteNachricht().isEmpty());

        Ft8Ablauf b;
        b.starteAlsAntworter(QStringLiteral("DL1ABC"), QString(),
                             QStringLiteral("JN67"), -12);
        QCOMPARE(b.zustand(), Z::Ruht);
    }

    // Gibt er mir seinen Rapport schon bestaetigt (R-09), ist mein
    // eigener bei ihm angekommen — dann ist der Zwischenschritt
    // ueberfluessig.
    void einBestaetigterRapportSpartEinenSchritt()
    {
        Ft8Ablauf a;
        a.starteAlsAntworter(QStringLiteral("DL1ABC"),
                             QStringLiteral("OE5SOS"),
                             QStringLiteral("JN67"), -12);
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC R-09"), -12));
        QCOMPARE(a.zustand(), Z::SendeRR73);
        QCOMPARE(a.empfangenerReport(), -9);
    }

    // ── Ein UNbestaetigter Rapport schliesst nicht ab (2026-11-17) ──
    //
    // Ich habe ihm meinen Rapport gegeben und erwarte „R-09". Kommt
    // stattdessen „-15" ohne R, ist meiner nicht angekommen -- er haelt
    // mich fuer den Anrufer. Bis heute fuehrte JEDER Rapport hier zum
    // RR73, und ein Fenster spaeter stand ein QSO im Logbuch, das auf
    // seiner Seite keines war (am Geraet: JA2FRO, 17 m).
    void einUnbestaetigterRapportSchliesstNichtAb()
    {
        Ft8Ablauf a;
        a.starteMitAnrufer(QStringLiteral("JA2FRO"), QStringLiteral("OE5SOS"),
                           QStringLiteral("JN67"), -21,
                           QStringLiteral("PM84"));
        QCOMPARE(a.zustand(), Z::SendeReport);

        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS JA2FRO -15"), -24));
        QCOMPARE(a.zustand(), Z::SendeRReport);
        QCOMPARE(a.empfangenerReport(), -15);
        QCOMPARE(a.naechsteNachricht(), QStringLiteral("JA2FRO OE5SOS R-21"));

        // Ein Fenster spaeter ist es NICHT fertig.
        a.fensterVerstrichen();
        QVERIFY(a.laeuft());

        // Erst sein RR73 macht es zum QSO; ich schliesse mit 73.
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS JA2FRO RR73"), -24));
        QCOMPARE(a.zustand(), Z::Sende73);
        a.fensterVerstrichen();
        QCOMPARE(a.zustand(), Z::Fertig);
    }

    // Der bestaetigte bleibt, was er war: der kurze Weg zum RR73.
    void derBestaetigteRapportFuehrtWeiterZumRr73()
    {
        Ft8Ablauf a;
        a.starteMitAnrufer(QStringLiteral("DL1ABC"), QStringLiteral("OE5SOS"),
                           QStringLiteral("JN67"), -7, QString());
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC R-15"), -7));
        QCOMPARE(a.zustand(), Z::SendeRR73);
    }

    // ── Hound: eine DX-Expedition im Fox-Betrieb rufen ───────
    //
    // Der Ablauf Zug fuer Zug aus WSJT-X' eigener Vorschrift
    // (lib/DXped_Operations.txt [v2.7.0], Fox = KH1DX).

    void derHoundRuftDieExpedition()
    {
        Ft8Ablauf a;
        a.starteAlsHound(QStringLiteral("KH1DX"),
                         QStringLiteral("K1ABC"), QStringLiteral("FN42"));
        QCOMPARE(a.zustand(), Z::RufeAn);
        QVERIFY(a.istHound());
        QCOMPARE(a.naechsteNachricht(), QStringLiteral("KH1DX K1ABC FN42"));
    }

    void derFoxDarfGehashtAnkommen()
    {
        // „K1ABC <KH1DX> -13" ist derselbe Fox wie „K1ABC KH1DX -13".
        // Die spitzen Klammern heiszen nur, dass im Paket ein Hash
        // stand. Wer das nicht zusammenfuehrt, laesst die Expedition
        // genau in dem Augenblick fallen, in dem sie einen aufruft.
        Ft8Ablauf a;
        a.starteAlsHound(QStringLiteral("KH1DX"),
                         QStringLiteral("K1ABC"), QStringLiteral("FN42"));
        QVERIFY(a.verarbeite(QStringLiteral("K1ABC <KH1DX> -13"), -11));
        QCOMPARE(a.zustand(), Z::SendeRReport);
        QCOMPARE(a.empfangenerReport(), -13);
    }

    void derHoundGehtNieAufRR73()
    {
        // Den Schluss macht der Fox. Ein Hound, der RR73 sendet,
        // schickt einen Satz, auf den die Expedition nicht wartet --
        // und haelt dabei einen Platz im Pileup besetzt.
        Ft8Ablauf a;
        a.starteAlsHound(QStringLiteral("KH1DX"),
                         QStringLiteral("K1ABC"), QStringLiteral("FN42"));
        QVERIFY(a.verarbeite(QStringLiteral("K1ABC KH1DX -13"), -11));
        QCOMPARE(a.zustand(), Z::SendeRReport);
        QCOMPARE(a.naechsteNachricht(), QStringLiteral("KH1DX K1ABC R-11"));

        // GEGENPROBE: als gewoehnlicher Antworter biegt ein schon
        // bestaetigter Rapport sehr wohl auf RR73 ab. Ohne diesen Lauf
        // belegt der Fall darueber nicht, dass die Rolle etwas aendert.
        Ft8Ablauf b;
        b.starteAlsAntworter(QStringLiteral("DL1ABC"),
                             QStringLiteral("K1ABC"),
                             QStringLiteral("FN42"), -11);
        QVERIFY(b.verarbeite(QStringLiteral("K1ABC DL1ABC R-13"), -11));
        QCOMPARE(b.zustand(), Z::SendeRR73);
    }

    void derFoxSchliesztAbUndDasIstDasLogbuch()
    {
        Ft8Ablauf a;
        a.starteAlsHound(QStringLiteral("KH1DX"),
                         QStringLiteral("K1ABC"), QStringLiteral("FN42"));
        QVERIFY(a.verarbeite(QStringLiteral("K1ABC KH1DX -13"), -11));
        QCOMPARE(a.zustand(), Z::SendeRReport);

        // Sein Abschluss: mein Rufzeichen vorn, „RR73;" dahinter, und
        // danach steht schon die NAECHSTE Station.
        QVERIFY(a.verarbeite(
            QStringLiteral("K1ABC RR73; W9XYZ <KH1DX> -17"), -11));
        QCOMPARE(a.zustand(), Z::Fertig);
        QCOMPARE(a.partner(), QStringLiteral("KH1DX"));
        QCOMPARE(a.empfangenerReport(), -13);
    }

    void einFremderAbschlussGehtMichNichtsAn()
    {
        // Derselbe Satz, aber fuer W9XYZ. Er steht im Band und darf
        // mein QSO nicht beenden -- sonst logge ich ein QSO, das die
        // Expedition nie mit mir gefuehrt hat.
        Ft8Ablauf a;
        a.starteAlsHound(QStringLiteral("KH1DX"),
                         QStringLiteral("K1ABC"), QStringLiteral("FN42"));
        QVERIFY(a.verarbeite(QStringLiteral("K1ABC KH1DX -13"), -11));
        QVERIFY(!a.verarbeite(
            QStringLiteral("W9XYZ RR73; G4AAA <KH1DX> -12"), -11));
        QCOMPARE(a.zustand(), Z::SendeRReport);
    }

    void dieHoundGrenzeStehtBeiTausendHertz()
    {
        // From WSJT-X lib/DXped_Operations.txt:16-17 [v2.7.0] --
        // „Hounds call in 2nd sequence, 1000-5000 Hz above f0. Hounds
        // transmitting below f0+1000 Hz will not be answered."
        QCOMPARE(Ft8Ablauf::kHoundMinTonHz, 1000);
    }

    // ── Der Abschied am 2026-10-30 ────────────────────────────────
    //
    // Im Betrieb aufgefallen, nicht von einem roten Prueflauf: nach
    // dem fertigen QSO mit HB9ZHK kam sein „73", und der Anrufer nahm
    // es als neuen Anruf. Viermal derselbe Rapport ins Leere.
    void einDreiundsiebzigIstKeinAnrufAufMeinCq()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"),
                           QStringLiteral("JN67"));
        QCOMPARE(a.zustand(), Z::RufeCq);

        // Genau die Zeile aus dem Fenster, Spalte fuer Spalte.
        QVERIFY(!a.verarbeite(QStringLiteral("OE5SOS HB9ZHK 73"), 16));
        QCOMPARE(a.zustand(), Z::RufeCq);
        QVERIFY(a.partner().isEmpty());

        // Dasselbe fuer die beiden anderen Schlussworte.
        QVERIFY(!a.verarbeite(QStringLiteral("OE5SOS HB9ZHK RR73"), 16));
        QVERIFY(!a.verarbeite(QStringLiteral("OE5SOS HB9ZHK RRR"), 16));
        QCOMPARE(a.zustand(), Z::RufeCq);
        QVERIFY(a.partner().isEmpty());
    }

    // Die Gegenprobe: ein echter Anruf wird weiterhin genommen --
    // sonst waere das „Nein" oben kein Fortschritt, sondern Taubheit.
    void einEchterAnrufGehtWeiterhinDurch()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"),
                           QStringLiteral("JN67"));
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS PD4RJ JO33"), 12));
        QCOMPARE(a.zustand(), Z::SendeReport);
        QCOMPARE(a.partner(), QStringLiteral("PD4RJ"));
        QCOMPARE(a.partnerLocator(), QStringLiteral("JO33"));

        // Auch der Anruf ohne Locator (nicht-Standard-Rufzeichen).
        Ft8Ablauf b;
        b.starteAlsAnrufer(QStringLiteral("OE5SOS"),
                           QStringLiteral("JN67"));
        QVERIFY(b.verarbeite(QStringLiteral("OE5SOS 9A6T"), 11));
        QCOMPARE(b.zustand(), Z::SendeReport);
        QCOMPARE(b.partner(), QStringLiteral("9A6T"));
    }

    void ohneFoxRufzeichenFaengtNichtsAn()
    {
        Ft8Ablauf a;
        a.starteAlsHound(QString(), QStringLiteral("K1ABC"),
                         QStringLiteral("FN42"));
        QCOMPARE(a.zustand(), Z::Ruht);
        QVERIFY(!a.laeuft());
    }

    // ── Der Anruf an EINE Station bekommt die Zahl des Betreibers ────
    //
    // Am Geraet (2026-11-17): TL8GD fuenfmal gerufen, „gave up" —
    // Betreiber: „der call hängt glaube ich wieder". Die Fuenf galt
    // auch fuer den Anruf, auf den noch niemand geantwortet hat.
    void derAnrufLaeuftSoOftWieEingestellt()
    {
        Ft8Ablauf a;
        a.setzeAnrufWiederholungen(12);
        a.starteAlsAntworter(QStringLiteral("TL8GD"), QStringLiteral("OE5SOS"),
                             QStringLiteral("JN67"), -12);
        QCOMPARE(a.zustand(), Ft8Ablauf::Zustand::RufeAn);
        for (int i = 0; i < 11; ++i) {
            a.fensterVerstrichen();
            QVERIFY2(a.laeuft(), qPrintable(QStringLiteral(
                "gab nach %1 Anrufen auf, eingestellt sind 12").arg(i + 1)));
        }
        a.fensterVerstrichen();
        QCOMPARE(a.zustand(), Ft8Ablauf::Zustand::Abgebrochen);
    }

    // Ohne Einstellung bleibt es bei den fuenf von bisher.
    void ohneEinstellungBleibtDerAnrufBeiFuenf()
    {
        Ft8Ablauf a;
        a.starteAlsAntworter(QStringLiteral("TL8GD"), QStringLiteral("OE5SOS"),
                             QStringLiteral("JN67"), -12);
        for (int i = 0; i < Ft8Ablauf::kMaxWiederholungen - 1; ++i) {
            a.fensterVerstrichen();
            QVERIFY(a.laeuft());
        }
        a.fensterVerstrichen();
        QCOMPARE(a.zustand(), Ft8Ablauf::Zustand::Abgebrochen);
    }

    // Und MITTEN im QSO bleibt die Fuenf, auch wenn der Anruf dreissig
    // darf: ein Rapport wird nicht dreissigmal wiederholt.
    void mittenImQsoBleibtEsBeiFuenf()
    {
        Ft8Ablauf a;
        a.setzeAnrufWiederholungen(30);
        a.starteAlsAntworter(QStringLiteral("DL1ABC"), QStringLiteral("OE5SOS"),
                             QStringLiteral("JN67"), -12);
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC -07"), -7));
        QVERIFY(a.zustand() != Ft8Ablauf::Zustand::RufeAn);
        for (int i = 0; i < Ft8Ablauf::kMaxWiederholungen - 1; ++i) {
            a.fensterVerstrichen();
            QVERIFY(a.laeuft());
        }
        a.fensterVerstrichen();
        QCOMPARE(a.zustand(), Ft8Ablauf::Zustand::Abgebrochen);
    }
};

QTEST_APPLESS_MAIN(TstFt8Ablauf)
#include "tst_ft8_ablauf.moc"
