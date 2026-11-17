// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_ft8_warteschlange.cpp  (Longpath)
// =================================================================
//
// Die Schlange und die Karenz. Beide entstanden aus einer Messung am
// 2026-10-30, nicht aus einem Entwurf: waehrend eines laufenden QSOs
// riefen zwei Stationen fuenfmal vergeblich an, und die dritte wurde
// nach dem Logbucheintrag ein zweites Mal gearbeitet.
//
// Die Zeit kommt hier von Hand herein. Darum laeuft eine Karenz von
// zehn Minuten in zwei Millisekunden durch.
// =================================================================

#include <QtTest/QtTest>

#include "core/Ft8Warteschlange.h"

using namespace Longpath;

class TstFt8Warteschlange : public QObject
{
    Q_OBJECT

private slots:
    // ── Der gemessene Fall ────────────────────────────────────────
    //
    // 12:49:45 PD4RJ ruft, 12:50:15 wieder, 12:50:45 ein drittes Mal.
    // 9A6T ruft um 12:50:15 und 12:50:45. Danach ist das QSO mit
    // HB9ZHK fertig -- und PD4RJ muss der naechste sein, nicht HB9ZHK.
    void derGemesseneFall()
    {
        Ft8Warteschlange s;
        const qint64 t0 = 1000 * 1000;   // irgendein Nullpunkt

        QVERIFY(s.merkeAnruf(QStringLiteral("PD4RJ"), 12,
                             QStringLiteral("JO33"), t0));
        QVERIFY(s.merkeAnruf(QStringLiteral("9A6T"), 11,
                             QString(), t0 + 30000));
        // Die Wiederholungen bringen keinen neuen Eintrag ...
        QVERIFY(!s.merkeAnruf(QStringLiteral("PD4RJ"), 17,
                              QStringLiteral("JO33"), t0 + 30000));
        QVERIFY(!s.merkeAnruf(QStringLiteral("PD4RJ"), 13,
                              QStringLiteral("JO33"), t0 + 60000));
        QVERIFY(!s.merkeAnruf(QStringLiteral("9A6T"), 14,
                              QString(), t0 + 60000));
        QCOMPARE(s.wartende(t0 + 60000), 2);

        // Das QSO mit HB9ZHK wird fertig.
        s.merkeGearbeitet(QStringLiteral("HB9ZHK"), t0 + 58000);

        // Der naechste ist PD4RJ -- der ERSTE, nicht der lauteste
        // und nicht der hartnaeckigste.
        const Ft8Warteschlange::Wartend w = s.nimmNaechsten(t0 + 60000);
        QCOMPARE(w.partner, QStringLiteral("PD4RJ"));
        QCOMPARE(w.locator, QStringLiteral("JO33"));
        // ... aber sein Rapport ist der NEUESTE, nicht der erste.
        QCOMPARE(w.snr, 13);

        // Danach 9A6T, danach niemand.
        QCOMPARE(s.nimmNaechsten(t0 + 60000).partner,
                 QStringLiteral("9A6T"));
        QVERIFY(s.nimmNaechsten(t0 + 60000).partner.isEmpty());
    }

    // ── Die Karenz: HB9ZHK kommt nicht zweimal dran ────────────────
    void einGearbeitetesRufzeichenStelltSichNichtAn()
    {
        Ft8Warteschlange s;
        const qint64 t0 = 0;
        s.merkeGearbeitet(QStringLiteral("HB9ZHK"), t0);

        QVERIFY(s.inKarenz(QStringLiteral("HB9ZHK"), t0 + 1000));
        QVERIFY(!s.merkeAnruf(QStringLiteral("HB9ZHK"), 16,
                              QStringLiteral("JN47"), t0 + 15000));
        QCOMPARE(s.wartende(t0 + 15000), 0);

        // Gross-/Kleinschreibung darf die Karenz nicht aushebeln.
        QVERIFY(s.inKarenz(QStringLiteral("hb9zhk"), t0 + 1000));
        QVERIFY(!s.merkeAnruf(QStringLiteral("hb9zhk"), 16,
                              QString(), t0 + 15000));
    }

    // Die Gegenprobe zur Karenz: sie ist eine Frist, keine Sperre.
    void nachDerKarenzGehtEsWieder()
    {
        Ft8Warteschlange s;
        const qint64 t0 = 0;
        s.merkeGearbeitet(QStringLiteral("HB9ZHK"), t0);

        const qint64 spaeter = t0 + Ft8Warteschlange::kKarenzMs;
        QVERIFY(!s.inKarenz(QStringLiteral("HB9ZHK"), spaeter));
        QVERIFY(s.merkeAnruf(QStringLiteral("HB9ZHK"), 16,
                             QStringLiteral("JN47"), spaeter));
        QCOMPARE(s.nimmNaechsten(spaeter).partner,
                 QStringLiteral("HB9ZHK"));
    }

    // ── Ein alter Anruf wird nicht mehr beantwortet ────────────────
    //
    // Wer vor fuenf Minuten rief, sitzt laengst woanders. Ihn
    // anzurufen heisst, zwanzig Sekunden in eine Leere zu senden.
    void einAbgelaufenerAnrufFaelltHeraus()
    {
        Ft8Warteschlange s;
        const qint64 t0 = 0;
        s.merkeAnruf(QStringLiteral("PD4RJ"), 12, QString(), t0);
        s.merkeAnruf(QStringLiteral("9A6T"), 11, QString(),
                     t0 + Ft8Warteschlange::kHaltbarkeitMs - 1000);

        const qint64 spaet = t0 + Ft8Warteschlange::kHaltbarkeitMs;
        QCOMPARE(s.wartende(spaet), 1);
        QCOMPARE(s.nimmNaechsten(spaet).partner, QStringLiteral("9A6T"));
    }

    void einLeeresRufzeichenKommtNichtHinein()
    {
        Ft8Warteschlange s;
        QVERIFY(!s.merkeAnruf(QString(), 10, QString(), 0));
        QCOMPARE(s.wartende(0), 0);
        // Und es legt auch keine Karenz an.
        s.merkeGearbeitet(QString(), 0);
        QVERIFY(!s.inKarenz(QString(), 0));
    }

    void wirdJemandZumPartnerFaelltErAusDerSchlange()
    {
        Ft8Warteschlange s;
        s.merkeAnruf(QStringLiteral("PD4RJ"), 12, QString(), 0);
        s.merkeAnruf(QStringLiteral("9A6T"), 11, QString(), 0);
        s.vergiss(QStringLiteral("pd4rj"));
        QCOMPARE(s.wartende(0), 1);
        QCOMPARE(s.nimmNaechsten(0).partner, QStringLiteral("9A6T"));
    }

    // Die Obergrenze: der AELTESTE faellt, nicht der neueste.
    void dieSchlangeHatEinEnde()
    {
        Ft8Warteschlange s;
        for (int i = 0; i < Ft8Warteschlange::kMaxWartende + 3; ++i) {
            s.merkeAnruf(QStringLiteral("DL%1AA").arg(i), 10,
                         QString(), 0);
        }
        QCOMPARE(s.wartende(0), Ft8Warteschlange::kMaxWartende);
        // Der erste Ueberlebende ist der vierte Anrufer.
        QCOMPARE(s.nimmNaechsten(0).partner, QStringLiteral("DL3AA"));
    }

    // ── Die Hand an der Schlange (2026-11-15) ─────────────────────
    //
    // „THEN" in der QSO-Karte laesst umreihen, streichen und aus der
    // Fundliste anstellen. Die Regeln dafuer stehen hier, nicht in der
    // Anzeige.
    void umreihenUndNachVorn()
    {
        Ft8Warteschlange s;
        const qint64 t0 = 1000 * 1000;
        QVERIFY(s.merkeAnruf(QStringLiteral("PD4RJ"), 12, QStringLiteral("JO33"), t0, 1338));
        QVERIFY(s.merkeAnruf(QStringLiteral("9A6T"), 11, QString(), t0, 1428));
        QVERIFY(s.merkeAnruf(QStringLiteral("DL0HAM"), 9, QStringLiteral("JO53"), t0, 812));

        QVERIFY(s.ruecke(QStringLiteral("9A6T"), -1, t0));
        QCOMPARE(s.gueltige(t0).at(0).partner, QStringLiteral("9A6T"));
        QCOMPARE(s.gueltige(t0).at(1).partner, QStringLiteral("PD4RJ"));
        // Der erste kann nicht weiter nach vorn, der letzte nicht weiter
        // nach hinten -- und beides aendert nichts.
        QVERIFY(!s.ruecke(QStringLiteral("9A6T"), -1, t0));
        QVERIFY(!s.ruecke(QStringLiteral("DL0HAM"), +1, t0));
        QVERIFY(s.nachVorn(QStringLiteral("dl0ham"), t0));   // Gross/klein egal
        QCOMPARE(s.nimmNaechsten(t0).partner, QStringLiteral("DL0HAM"));
        QCOMPARE(s.nimmNaechsten(t0).partner, QStringLiteral("9A6T"));
    }

    // Ein Abgelaufener steht unsichtbar dazwischen: „nach oben" muss
    // trotzdem einen SICHTBAREN Platz gewinnen.
    void rueckenUeberspringtAbgelaufene()
    {
        Ft8Warteschlange s;
        const qint64 t0 = 1000 * 1000;
        QVERIFY(s.merkeAnruf(QStringLiteral("ALT1"), 1, QString(), t0));
        QVERIFY(s.merkeAnruf(QStringLiteral("PD4RJ"), 12, QString(),
                             t0 + Ft8Warteschlange::kHaltbarkeitMs - 1000));
        QVERIFY(s.merkeAnruf(QStringLiteral("9A6T"), 11, QString(),
                             t0 + Ft8Warteschlange::kHaltbarkeitMs - 500));
        const qint64 jetzt = t0 + Ft8Warteschlange::kHaltbarkeitMs + 10;
        QCOMPARE(s.gueltige(jetzt).size(), 2);
        QVERIFY(s.ruecke(QStringLiteral("9A6T"), -1, jetzt));
        QCOMPARE(s.gueltige(jetzt).at(0).partner, QStringLiteral("9A6T"));
    }

    void dieFrequenzWirdGemerktUndAufgefrischt()
    {
        Ft8Warteschlange s;
        const qint64 t0 = 1000 * 1000;
        QVERIFY(s.merkeAnruf(QStringLiteral("PD4RJ"), 12, QString(), t0, 1338));
        QCOMPARE(s.gueltige(t0).at(0).tonHz, 1338);
        QVERIFY(!s.merkeAnruf(QStringLiteral("PD4RJ"), 13, QString(), t0 + 15000, 1340));
        QCOMPARE(s.gueltige(t0 + 15000).at(0).tonHz, 1340);
        // Ohne Frequenz (-1) bleibt die alte stehen.
        QVERIFY(!s.merkeAnruf(QStringLiteral("PD4RJ"), 14, QString(), t0 + 30000));
        QCOMPARE(s.gueltige(t0 + 30000).at(0).tonHz, 1340);
    }

    // ✕ heisst: diesmal nicht. Ruft er gleich wieder, stellt er sich
    // nicht von selbst an -- sonst stuende er nach einem Fenster wieder
    // da. Von Hand geht es trotzdem.
    void gestrichenBleibtGestrichen()
    {
        Ft8Warteschlange s;
        const qint64 t0 = 1000 * 1000;
        QVERIFY(s.merkeAnruf(QStringLiteral("DL0HAM"), 9, QString(), t0));
        s.weiseAb(QStringLiteral("DL0HAM"), t0 + 1000);
        QCOMPARE(s.wartende(t0 + 1000), 0);
        QVERIFY(!s.merkeAnruf(QStringLiteral("DL0HAM"), 9, QString(), t0 + 16000));
        QVERIFY(s.abgewiesen(QStringLiteral("dl0ham"), t0 + 16000));
        // Nach der Abweisung gilt ein Anruf wieder.
        const qint64 spaeter = t0 + 1000 + Ft8Warteschlange::kAbweisungMs;
        QVERIFY(!s.abgewiesen(QStringLiteral("DL0HAM"), spaeter));
        QVERIFY(s.merkeAnruf(QStringLiteral("DL0HAM"), 9, QString(), spaeter));
        // Und von Hand sofort, auch waehrend der Abweisung.
        s.weiseAb(QStringLiteral("DL0HAM"), spaeter);
        QVERIFY(s.stelleAn(QStringLiteral("DL0HAM"), 9, QString(), 812, spaeter + 1));
        QVERIFY(!s.abgewiesen(QStringLiteral("DL0HAM"), spaeter + 1));
    }

    // Von Hand angestellt heisst: ICH rufe IHN. Ruft er dann doch
    // selbst, beginnt es wie bei jedem Anrufer mit dem Rapport.
    void vonHandAngestellt()
    {
        Ft8Warteschlange s;
        const qint64 t0 = 1000 * 1000;
        QVERIFY(s.stelleAn(QStringLiteral("DL1ABC"), -8, QStringLiteral("JO31"), 1850, t0));
        QVERIFY(s.gueltige(t0).at(0).vonHand);
        QCOMPARE(s.gueltige(t0).at(0).tonHz, 1850);
        QVERIFY(!s.stelleAn(QStringLiteral("DL1ABC"), -8, QString(), 1850, t0));  // schon da
        QVERIFY(!s.merkeAnruf(QStringLiteral("DL1ABC"), -3, QString(), t0 + 15000));
        QVERIFY(!s.gueltige(t0 + 15000).at(0).vonHand);
        // Gerade gearbeitet: auch von Hand nicht -- ein Doppel-QSO
        // bliebe eines.
        s.merkeGearbeitet(QStringLiteral("HB9ZHK"), t0);
        QVERIFY(!s.stelleAn(QStringLiteral("HB9ZHK"), 18, QString(), 1712, t0 + 1000));
        // Eine Zeile, die MICH ruft, stellt sich als Anrufer an.
        QVERIFY(s.stelleAn(QStringLiteral("PD4RJ"), 12, QString(), 1338, t0, false));
        QVERIFY(!s.gueltige(t0).at(1).vonHand);
    }

    void leerenRaeumtBeideListen()
    {
        Ft8Warteschlange s;
        s.merkeAnruf(QStringLiteral("PD4RJ"), 12, QString(), 0);
        s.merkeGearbeitet(QStringLiteral("HB9ZHK"), 0);
        s.leeren();
        QCOMPARE(s.wartende(0), 0);
        QVERIFY(!s.inKarenz(QStringLiteral("HB9ZHK"), 0));
    }
};

QTEST_APPLESS_MAIN(TstFt8Warteschlange)
#include "tst_ft8_warteschlange.moc"
