// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: Longpath-eigener Prueflauf. Die eine MSHV-Nennung ist
// eine Herkunftsangabe im Kommentar, kein uebersetzter Quelltext.
//
// ── Mehrere QSOs gleichzeitig: MSHVs Multi Answering, Stufe 2 ──────────
//
// Stufe 1 (PR #323) legt mehrere Nachrichten in EIN Sendefenster.
// Stufe 2 ist die Buchhaltung davor: wer steht auf welchem Platz, wie
// kommt er darauf, wann geht er herunter, und wer rueckt nach.
//
// Das Modell stammt aus MSHV `src/HvTxW/hvmultianswermodw.h` [2.76.7,
// LZ2HV]: zwei Listen (`LsQueue` wartend, `LsNow` laufend) und
// `#define MAXSL 6` als Obergrenze.
//
// Der Hauptfall faehrt DREI vollstaendige QSOs nebeneinander durch --
// derselbe Gedanke wie beim RTTY-Rundlauf und beim Zweiton: ein
// Koordinator laesst sich nicht daran pruefen, dass er Zaehler
// hochzaehlt, sondern daran, dass am Ende drei fertige QSOs dastehen
// und jedes genau einmal.
//
// Gegenprobe (2026-10-24): mit nur EINEM Platz (`setPlaetze(1)`) ist
// `dreiGleichzeitigDurch` rot -- die beiden anderen warten noch, statt
// fertig zu sein. Genau das war der Stand vor Stufe 2.

#include <QtTest>

#include "core/Ft8Mehrfach.h"

using namespace Longpath;
using Z = Ft8Ablauf::Zustand;

namespace {
// QStringLiteral braucht ein echtes Literal; darum hier Funktionen
// statt constexpr-Zeiger.
QString ich()     { return QStringLiteral("OE5SOS"); }
QString locator() { return QStringLiteral("JN67VV"); }
}

class TstFt8Mehrfach : public QObject
{
    Q_OBJECT

private:
    static Ft8Mehrfach frisch(int plaetze)
    {
        Ft8Mehrfach m;
        m.setEigeneDaten(ich(), locator());
        m.setPlaetze(plaetze);
        return m;
    }

private slots:

    // ── Die Obergrenze steht in MSHV, nicht in meinem Kopf ───────────
    void dieObergrenzeIstSechs()
    {
        QCOMPARE(Ft8Mehrfach::kMaxPlaetze, 6);

        Ft8Mehrfach m = frisch(3);
        m.setPlaetze(99);
        QCOMPARE(m.plaetze(), 6);     // geklemmt, nicht abgelehnt
        m.setPlaetze(0);
        QCOMPARE(m.plaetze(), 1);
    }

    // ── Der Hauptfall ────────────────────────────────────────────────
    void dreiGleichzeitigDurch()
    {
        Ft8Mehrfach m = frisch(3);

        QVERIFY(m.nimmAnrufer(QStringLiteral("DL1ABC"), -12));
        QVERIFY(m.nimmAnrufer(QStringLiteral("G0XYZ"),  -7));
        QVERIFY(m.nimmAnrufer(QStringLiteral("F5QRP"),  -18));
        QCOMPARE(m.aktive(), 3);
        QCOMPARE(m.wartende(), 0);

        // Erstes Fenster: drei Anrufe, auf drei Toenen mit 60 Hz
        // Abstand (MSHV gen_ft8.cpp:262).
        //
        // ── Der RAPPORT, nicht mein Locator (2026-10-30) ──────
        //
        // Hier stand bis heute „DL1ABC OE5SOS JN67", also Tx1. Das
        // war falsch, und der Lauf hat es festgehalten statt
        // gefangen: wer MICH gerufen hat, wartet auf einen Rapport,
        // nicht auf meinen Locator. `nimmAnrufer` nahm
        // `starteAlsAntworter` (Zustand RufeAn = Tx1) und begruendete
        // das im Kommentar damit, es sei „der Einstieg bei Tx2" --
        // war es nie.
        //
        // Jeder Durchgang kostete dadurch einen Zug mehr, also 30 s.
        // Siehe `Ft8Ablauf::starteMitAnrufer`.
        auto raus = m.naechsteNachrichten();
        QCOMPARE(raus.size(), 3);
        QCOMPARE(raus[0].text, QStringLiteral("DL1ABC OE5SOS -12"));
        QCOMPARE(raus[1].text, QStringLiteral("G0XYZ OE5SOS -07"));
        QCOMPARE(raus[2].text, QStringLiteral("F5QRP OE5SOS -18"));
        QCOMPARE(raus[1].toneHz - raus[0].toneHz, 60.0f);
        QCOMPARE(raus[2].toneHz - raus[1].toneHz, 60.0f);

        // Alle drei bestaetigen und geben ihren Rapport -- durcheinander,
        // wie auf dem Band. Jeder Ablauf nimmt sich das Seine.
        //
        // ── Mit „R" davor (2026-11-17) ───────────────────────────────
        //
        // Hier standen bis heute UNbestaetigte Rapporte („-05"), und
        // der Lauf hielt fest, dass darauf RR73 folgt: „ich habe ihm
        // meinen Rapport gegeben und seinen bekommen -- der Austausch
        // ist vollstaendig". Das stimmt nur, wenn meiner ANGEKOMMEN
        // ist, und genau das sagt erst das R. Ohne R hat er meinen Satz
        // nicht gelesen; ein RR73 darauf schliesst ein QSO ab, das auf
        // seiner Seite keines ist. Am Geraet stand so JA2FRO im
        // Logbuch (2026-11-17, 17 m). Der Fall ohne R steht jetzt in
        // tst_ft8_ablauf (`einUnbestaetigterRapportSchliesstNichtAb`).
        QCOMPARE(m.verarbeite(QStringLiteral("OE5SOS G0XYZ R-05"),  -7), 1);
        QCOMPARE(m.verarbeite(QStringLiteral("OE5SOS F5QRP R-20"), -18), 1);
        QCOMPARE(m.verarbeite(QStringLiteral("OE5SOS DL1ABC R-09"), -12), 1);

        // ── Und danach RR73, nicht R-12 (2026-10-30) ─────────
        //
        // Folgt aus dem Einstieg bei Tx2. Ich habe IHM meinen
        // Rapport gegeben und SEINEN bestaetigt bekommen -- der
        // Austausch ist vollstaendig. Ein bestaetigter Rapport (R-12)
        // waere der Zug des ANTWORTERS, und das bin ich hier nicht:
        // ich habe CQ gerufen.
        //
        // Der alte Lauf erwartete R-12, weil `nimmAnrufer` bei Tx1
        // einstieg und der Ablauf dadurch eine Rolle zu tief lag.
        raus = m.naechsteNachrichten();
        QCOMPARE(raus.size(), 3);
        QCOMPARE(raus[0].text, QStringLiteral("DL1ABC OE5SOS RR73"));

        // Alle drei schliessen ab.
        QCOMPARE(m.verarbeite(QStringLiteral("OE5SOS DL1ABC RR73"), -12), 1);
        QCOMPARE(m.verarbeite(QStringLiteral("OE5SOS G0XYZ RR73"),   -7), 1);
        QCOMPARE(m.verarbeite(QStringLiteral("OE5SOS F5QRP RR73"),  -18), 1);

        // ── Danach ist nichts mehr zu senden (2026-10-30) ────
        //
        // Auch das folgt aus dem Einstieg bei Tx2. Als ANRUFER
        // schliesse ich mit RR73 ab; sein RR73 ist die Bestaetigung,
        // und damit ist der Durchgang vorbei. Ein 73 von mir waere
        // der Zug des ANTWORTERS (Tx5).
        //
        // Der alte Lauf erwartete hier drei Saetze -- und hielt damit
        // fest, dass jedes QSO einen Durchgang laenger dauert als
        // noetig.
        QCOMPARE(m.naechsteNachrichten().size(), 0);
        m.fensterVerstrichen();

        const QVector<Ft8Ablauf> fertig = m.nimmFertige();
        QCOMPARE(fertig.size(), 3);
        QStringList partner;
        for (const Ft8Ablauf& a : fertig) {
            QCOMPARE(a.zustand(), Z::Fertig);
            partner << a.partner();
        }
        partner.sort();
        QCOMPARE(partner, QStringList({QStringLiteral("DL1ABC"),
                                       QStringLiteral("F5QRP"),
                                       QStringLiteral("G0XYZ")}));

        // Genau EINMAL. Beim zweiten Griff ist nichts mehr da.
        QCOMPARE(m.nimmFertige().size(), 0);
        QCOMPARE(m.aktive(), 0);
    }

    // ── Die Warteschlange ────────────────────────────────────────────
    void ueberzaehligeWartenUndRueckenNach()
    {
        Ft8Mehrfach m = frisch(2);

        QVERIFY(m.nimmAnrufer(QStringLiteral("DL1ABC"), -12));
        QVERIFY(m.nimmAnrufer(QStringLiteral("G0XYZ"),  -7));
        QVERIFY(m.nimmAnrufer(QStringLiteral("F5QRP"), -18));   // zu viel

        QCOMPARE(m.aktive(), 2);
        QCOMPARE(m.wartende(), 1);
        QVERIFY(m.istVoll());

        // Der Wartende belegt KEINEN Ton, solange er wartet -- sonst
        // saesse er im Fenster, ohne dass jemand mit ihm spricht.
        QCOMPARE(m.naechsteNachrichten().size(), 2);

        // Einer wird fertig.
        m.verarbeite(QStringLiteral("OE5SOS DL1ABC -09"), -12);
        m.verarbeite(QStringLiteral("OE5SOS DL1ABC RR73"), -12);
        m.fensterVerstrichen();     // 73 hinaus -> fertig -> Platz frei

        QCOMPARE(m.nimmFertige().size(), 1);
        QCOMPARE(m.wartende(), 0);
        QCOMPARE(m.aktive(), 2);        // F5QRP ist nachgerueckt

        bool f5Dabei = false;
        for (const auto& b : m.belegung()) {
            if (b.partner == QStringLiteral("F5QRP")) { f5Dabei = true; }
        }
        QVERIFY2(f5Dabei, "F5QRP ist nicht nachgerueckt");
    }

    // ── Dasselbe Rufzeichen nicht zweimal ────────────────────────────
    //
    // Ohne diese Sperre antwortete man demselben im selben Fenster auf
    // zwei Toenen und zaehlte am Ende zwei QSOs.
    void dasselbeRufzeichenNurEinmal()
    {
        Ft8Mehrfach m = frisch(3);
        QVERIFY(m.nimmAnrufer(QStringLiteral("DL1ABC"), -12));
        QVERIFY(!m.nimmAnrufer(QStringLiteral("DL1ABC"), -14));
        QVERIFY(!m.nimmAnrufer(QStringLiteral("dl1abc"), -14));  // Schreibweise egal
        QCOMPARE(m.aktive(), 1);

        // Auch in der Warteschlange gilt es.
        Ft8Mehrfach eng = frisch(1);
        QVERIFY(eng.nimmAnrufer(QStringLiteral("DL1ABC"), -12));
        QVERIFY(eng.nimmAnrufer(QStringLiteral("G0XYZ"), -7));
        QVERIFY(!eng.nimmAnrufer(QStringLiteral("G0XYZ"), -9));
        QCOMPARE(eng.wartende(), 1);
    }

    // ── Ein Platz, der aufgibt, blockiert nicht ──────────────────────
    void abgebrochenerPlatzWirdFrei()
    {
        Ft8Mehrfach m = frisch(1);
        QVERIFY(m.nimmAnrufer(QStringLiteral("DL1ABC"), -12));
        QVERIFY(m.nimmAnrufer(QStringLiteral("G0XYZ"), -7));
        QCOMPARE(m.wartende(), 1);

        // DL1ABC antwortet nie. Nach kMaxWiederholungen gibt der
        // Ablauf auf, und der Platz muss frei werden.
        for (int i = 0; i <= Ft8Ablauf::kMaxWiederholungen + 1; ++i) {
            m.fensterVerstrichen();
        }

        QCOMPARE(m.wartende(), 0);
        QCOMPARE(m.aktive(), 1);
        QCOMPARE(m.belegung().first().partner, QStringLiteral("G0XYZ"));
        // Ein aufgegebener Ablauf ist KEIN fertiges QSO.
        QCOMPARE(m.nimmFertige().size(), 0);
    }

    // ── Ohne eigenes Rufzeichen geht gar nichts ──────────────────────
    void ohneEigenesRufzeichenKeinPlatz()
    {
        Ft8Mehrfach m;
        m.setPlaetze(3);
        QVERIFY(!m.nimmAnrufer(QStringLiteral("DL1ABC"), -12));
        QCOMPARE(m.aktive(), 0);
    }
};

QTEST_MAIN(TstFt8Mehrfach)
#include "tst_ft8_mehrfach.moc"
