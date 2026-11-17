// =================================================================
// tests/tst_ft8_anruferliste.cpp  (Longpath)
// =================================================================
//
// „THEN" in der QSO-Karte des FT8-Dekoders und „+ Queue" in der
// Fundliste (2026-11-15).
//
// Gefahren wird die BEDIENUNG, nicht die Schlange dahinter: echte
// Maus-Ereignisse an die Zeilen, die Knoepfe und den Viewport der
// Liste (QApplication::sendEvent -- QTest::mouseClick verpufft ohne
// Schirm). Die Regeln der Schlange selbst prueft tst_ft8_warteschlange.
//
//   * wer waehrend eines QSOs ruft, steht unter „THEN", in der
//     Reihenfolge, in der er drankommt
//   * ↑ ↓ ✕ an der Zeile aendern die Reihenfolge, die nach dem QSO
//     WIRKLICH gilt -- und ein ✕ bleibt beim naechsten Anruf stehen
//   * „+ Queue" erscheint nur an Zeilen, die sich anstellen lassen,
//     und eine CQ-Zeile wird so angerufen, wie ein Doppelklick sie
//     angerufen haette
//   * Doppelklick unter „THEN": waehrend eines QSOs „als naechster",
//     sonst sofort
//
// Mit LONGPATH_GRAB_DIR und LONGPATH_ENTWURF=1 legt `blaetter` Bilder
// des echten Panels ab (mit QT_SCALE_FACTOR=2 in wirklicher Groesse).
// =================================================================
// Modification history (Longpath):
//   2026-11-15 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest/QtTest>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QEnterEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyleFactory>
#include <QTextBlock>
#include <QTimeZone>
#include <QToolButton>

#include "core/AppSettings.h"
#include "core/Ft8Ablauf.h"
#include "core/Ft8Decoder.h"
#include "core/Ft8Warteschlange.h"
#include "gui/applets/Ft8Applet.h"
#include "gui/styles/AppTheme.h"
#include "gui/widgets/Ft8DannListe.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

Ft8Decode fund(const QString& text, float snr, float hz)
{
    Ft8Decode d;
    d.snrDb     = snr;
    d.dtSeconds = 0.2f;
    d.freqHz    = hz;
    d.score     = 40;
    d.text      = text;
    return d;
}

void periode(Ft8Applet& applet, const QVector<Ft8Decode>& funde, qint64 ms)
{
    QMetaObject::invokeMethod(
        &applet, "onDecoded", Qt::DirectConnection,
        Q_ARG(QVector<Longpath::Ft8Decode>, funde),
        Q_ARG(QDateTime, QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC)));
}

QPointF global(QWidget* w, const QPointF& p) { return w->mapToGlobal(p); }

void klick(QWidget* w)
{
    const QPointF p = QRectF(w->rect()).center();
    QMouseEvent druck(QEvent::MouseButtonPress, p, global(w, p),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(w, &druck);
    QMouseEvent los(QEvent::MouseButtonRelease, p, global(w, p),
                    Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(w, &los);
}

void doppelklick(QWidget* w)
{
    const QPointF p = QRectF(w->rect()).center();
    QMouseEvent druck(QEvent::MouseButtonPress, p, global(w, p),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(w, &druck);
    QMouseEvent los(QEvent::MouseButtonRelease, p, global(w, p),
                    Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(w, &los);
    QMouseEvent doppel(QEvent::MouseButtonDblClick, p, global(w, p),
                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(w, &doppel);
    QApplication::sendEvent(w, &los);
}

void ueber(QWidget* w)
{
    const QPointF p(5, 5);
    QEnterEvent e(p, p, global(w, p));
    QApplication::sendEvent(w, &e);
}

void weg(QWidget* w)
{
    QEvent e(QEvent::Leave);
    QApplication::sendEvent(w, &e);
}

// Die Maus auf die Fundzeile stellen, die `teil` enthaelt.
bool aufZeile(Ft8Applet& applet, const QString& teil)
{
    QPlainTextEdit* l = applet.listForTest();
    for (QTextBlock b = l->document()->firstBlock(); b.isValid(); b = b.next()) {
        if (!b.text().contains(teil)) { continue; }
        const QRect r = l->cursorRect(QTextCursor(b));
        const QPointF p(60, r.center().y());
        QMouseEvent m(QEvent::MouseMove, p, global(l->viewport(), p),
                      Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(l->viewport(), &m);
        return true;
    }
    return false;
}

QStringList reihe(const Ft8Applet& applet)
{
    QStringList r;
    for (const Ft8Warteschlange::Wartend& w :
         applet.schlangeForTest().gueltige(QDateTime::currentMSecsSinceEpoch())) {
        r << w.partner;
    }
    return r;
}

QStringList zeilen(Ft8DannListe* d)
{
    QStringList r;
    for (int i = 0; i < d->sichtbareZeilenForTest(); ++i) {
        r << d->rufInZeileForTest(i);
    }
    return r;
}

}  // namespace

class TstFt8Anruferliste : public QObject
{
    Q_OBJECT

    // Ein Fenster in der Vergangenheit, wie in tst_ft8_flaeche: die
    // Funde tragen es, die Schlange rechnet mit der echten Uhr.
    static constexpr qint64 kFenster = 999999990000LL;

    struct Stand {
        RadioModel radio;
        Ft8Applet  applet{&radio};
    };

    static void richte(Stand& s, int breite = 706, int hoehe = 700)
    {
        const int id = s.radio.addSlice();
        SliceModel* slice = s.radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14074000.0);
        s.applet.setSlice(slice);
        s.applet.resize(breite, hoehe);
        s.applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&s.applet));
    }

    // QSO mit HB9ZHK laeuft; vier rufen dazwischen, einer ruft CQ.
    static void belebe(Stand& s)
    {
        s.applet.ablaufForTestSchreibend().starteMitAnrufer(
            QStringLiteral("HB9ZHK"), QStringLiteral("OE5SOS"),
            QStringLiteral("JN67"), 18, QStringLiteral("JN47"));
        periode(s.applet, {
            fund(QStringLiteral("CQ DL1ABC JO31"), -8.0f, 1850.0f),
            fund(QStringLiteral("OE5SOS PD4RJ JO33"), 12.0f, 1338.0f),
            fund(QStringLiteral("OE5SOS 9A6T JN85"), 11.0f, 1428.0f),
            fund(QStringLiteral("OE5SOS DL0HAM JO53"), 9.0f, 812.0f),
            fund(QStringLiteral("OE5SOS SP9XYZ KO00"), -7.0f, 1906.0f),
        }, kFenster);
    }

private slots:
    void initTestCase()
    {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        applyDarkPalette(*qApp);
        applyAppBaselineQss(*qApp);
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("StationGridSquare"),
                                         QStringLiteral("JN67"));
    }

    void werRuftStehtUnterThen()
    {
        Stand s;
        richte(s);
        Ft8DannListe* d = s.applet.dannForTest();
        QVERIFY(d);
        QVERIFY2(d->isVisibleTo(&s.applet), "THEN steht nicht in der QSO-Karte");
        QCOMPARE(d->sichtbareZeilenForTest(), 0);
        QVERIFY(d->hinweisForTest()->text().contains(QStringLiteral("nobody waiting")));

        belebe(s);
        QCOMPARE(reihe(s.applet), (QStringList{QStringLiteral("PD4RJ"),
                                                QStringLiteral("9A6T"),
                                                QStringLiteral("DL0HAM"),
                                                QStringLiteral("SP9XYZ")}));
        // Drei stehen da, der vierte hinter „+1 more".
        QCOMPARE(zeilen(d), (QStringList{QStringLiteral("PD4RJ"),
                                         QStringLiteral("9A6T"),
                                         QStringLiteral("DL0HAM")}));
        QPushButton* mehr = d->mehrKnopfForTest();
        QVERIFY(mehr->isVisibleTo(&s.applet));
        QCOMPARE(mehr->text(), QStringLiteral("+1 more"));
        klick(mehr);
        QCOMPARE(d->sichtbareZeilenForTest(), 4);
        QCOMPARE(d->rufInZeileForTest(3), QStringLiteral("SP9XYZ"));
        klick(mehr);
        QCOMPARE(d->sichtbareZeilenForTest(), 3);
        // Die CQ-Zeile ruft niemanden an -- sie wartet nicht.
        QVERIFY(!reihe(s.applet).contains(QStringLiteral("DL1ABC")));
    }

    void hochRunterStreichenGiltNachDemQso()
    {
        Stand s;
        richte(s);
        belebe(s);
        Ft8DannListe* d = s.applet.dannForTest();

        // 9A6T eins nach oben: Maus auf die Zeile, dann ↑.
        QWidget* zeile2 = d->zeileForTest(1);
        QVERIFY(zeile2);
        ueber(zeile2);
        QToolButton* hoch = d->knopfForTest(1, 0);
        QVERIFY(hoch);
        QVERIFY2(hoch->isVisibleTo(&s.applet), "↑ erscheint nicht beim Ueberfahren");
        klick(hoch);
        QCOMPARE(zeilen(d).first(), QStringLiteral("9A6T"));
        QCOMPARE(reihe(s.applet).first(), QStringLiteral("9A6T"));
        weg(zeile2);
        QVERIFY(!d->knopfForTest(1, 0)->isVisibleTo(&s.applet));

        // DL0HAM streichen -- und wenn er gleich wieder ruft, bleibt er weg.
        QVERIFY(d->zeileForTest(2) && d->knopfForTest(2, 2));
        ueber(d->zeileForTest(2));
        klick(d->knopfForTest(2, 2));
        QVERIFY(!reihe(s.applet).contains(QStringLiteral("DL0HAM")));
        periode(s.applet, {fund(QStringLiteral("OE5SOS DL0HAM JO53"), 9.0f, 812.0f)},
                kFenster + 15000);
        QVERIFY2(!reihe(s.applet).contains(QStringLiteral("DL0HAM")),
                 "ein ✕ haelt nur bis zum naechsten Anruf");

        // Und nach dem QSO kommt der dran, der OBEN steht.
        QVERIFY(s.applet.rufeNaechstenWartendenForTest());
        QCOMPARE(s.applet.ablaufForTest().partner(), QStringLiteral("9A6T"));
        QCOMPARE(s.applet.ablaufForTest().zustand(),
                 Ft8Ablauf::Zustand::SendeReport);
        QCOMPARE(zeilen(d).first(), QStringLiteral("PD4RJ"));
    }

    void queueKapselNurWoSieEtwasTut()
    {
        Stand s;
        richte(s);
        belebe(s);
        periode(s.applet, {fund(QStringLiteral("OE5SOS HB9ZHK R-14"), 16.0f, 1712.0f)},
                kFenster + 15000);
        QPushButton* pille = s.applet.queuePilleForTest();
        QVERIFY(pille);

        // Der laufende Partner, ein schon Wartender: keine Kapsel.
        QVERIFY(aufZeile(s.applet, QStringLiteral("OE5SOS HB9ZHK")));
        QVERIFY(!pille->isVisible());
        QVERIFY(aufZeile(s.applet, QStringLiteral("OE5SOS PD4RJ")));
        QVERIFY(!pille->isVisible());

        // Eine CQ-Zeile: Kapsel, Klick, und DL1ABC wartet -- von Hand.
        QVERIFY(aufZeile(s.applet, QStringLiteral("CQ DL1ABC")));
        QVERIFY2(pille->isVisible(), "+ Queue erscheint nicht an der CQ-Zeile");
        QCOMPARE(pille->text(), QStringLiteral("+ Queue"));
        klick(pille);
        QVERIFY(!pille->isVisible());
        QCOMPARE(reihe(s.applet).last(), QStringLiteral("DL1ABC"));
        const auto alle = s.applet.schlangeForTest().gueltige(
            QDateTime::currentMSecsSinceEpoch());
        QVERIFY(alle.last().vonHand);
        QCOMPARE(alle.last().tonHz, 1850);

        // Jetzt steht er schon da: keine zweite Kapsel.
        QVERIFY(aufZeile(s.applet, QStringLiteral("CQ DL1ABC")));
        QVERIFY(!pille->isVisible());

        // Doppelklick unter THEN waehrend des QSOs: er kommt als
        // naechster, das QSO mit HB9ZHK laeuft weiter.
        Ft8DannListe* d = s.applet.dannForTest();
        klick(d->mehrKnopfForTest());
        int bei = -1;
        for (int i = 0; i < d->sichtbareZeilenForTest(); ++i) {
            if (d->rufInZeileForTest(i) == QStringLiteral("DL1ABC")) { bei = i; }
        }
        QVERIFY(bei > 0);
        doppelklick(d->zeileForTest(bei));
        QCOMPARE(s.applet.ablaufForTest().partner(), QStringLiteral("HB9ZHK"));
        QCOMPARE(reihe(s.applet).first(), QStringLiteral("DL1ABC"));

        // Er ist dran: angerufen wie per Doppelklick auf seine CQ-Zeile.
        QVERIFY(s.applet.rufeNaechstenWartendenForTest());
        QCOMPARE(s.applet.ablaufForTest().partner(), QStringLiteral("DL1ABC"));
        QCOMPARE(s.applet.ablaufForTest().zustand(), Ft8Ablauf::Zustand::RufeAn);
        QCOMPARE(s.applet.ablaufForTest().naechsteNachricht(),
                 QStringLiteral("DL1ABC OE5SOS JN67"));
    }

    void doppelklickOhneQsoRuftSofort()
    {
        Stand s;
        richte(s);
        // Der Ablauf ruht; PD4RJ ruft -- er wird gemerkt, nicht verloren.
        periode(s.applet, {fund(QStringLiteral("OE5SOS PD4RJ JO33"), 12.0f, 1338.0f)},
                kFenster);
        Ft8DannListe* d = s.applet.dannForTest();
        QCOMPARE(zeilen(d), QStringList{QStringLiteral("PD4RJ")});
        QVERIFY(d->zeileForTest(0));
        QVERIFY(d->hinweisForTest()->text().contains(QStringLiteral("double-click")));
        doppelklick(d->zeileForTest(0));
        QCOMPARE(s.applet.ablaufForTest().partner(), QStringLiteral("PD4RJ"));
        QCOMPARE(s.applet.ablaufForTest().zustand(),
                 Ft8Ablauf::Zustand::SendeReport);
        QCOMPARE(d->sichtbareZeilenForTest(), 0);
    }

    // ── Bilder des echten Panels ───────────────────────────────────
    void blaetter()
    {
        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (dir.isEmpty() || qEnvironmentVariable("LONGPATH_ENTWURF") != QStringLiteral("1")) {
            QSKIP("nur mit LONGPATH_GRAB_DIR und LONGPATH_ENTWURF=1");
        }
        QDir().mkpath(dir);
        Stand s;
        richte(s);
        const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();

        // Dasselbe Band wie auf den Entwurfsblaettern.
        s.applet.ablaufForTestSchreibend().starteMitAnrufer(
            QStringLiteral("HB9ZHK"), QStringLiteral("OE5SOS"),
            QStringLiteral("JN67"), 18, QStringLiteral("JN47"));
        periode(s.applet, {
            fund(QStringLiteral("CQ DL1ABC JO31"), -8.0f, 1850.0f),
            fund(QStringLiteral("CQ EA3IZF JN11"), -14.0f, 622.0f),
            fund(QStringLiteral("JA1XYZ W1AW RR73"), -3.0f, 1336.0f),
            fund(QStringLiteral("OE5SOS HB9ZHK JN47"), 18.0f, 1712.0f),
        }, kFenster);
        periode(s.applet, {
            fund(QStringLiteral("CQ DX ZS6CCY KG33"), -11.0f, 2140.0f),
            fund(QStringLiteral("R2DX UA9OBA R-17"), -19.0f, 944.0f),
            fund(QStringLiteral("OE5SOS HB9ZHK R-14"), 16.0f, 1712.0f),
        }, kFenster + 30000);
        periode(s.applet, {
            fund(QStringLiteral("CQ IK6CGO JN63"), 4.0f, 1203.0f),
            fund(QStringLiteral("CQ F4HZR IN88"), -16.0f, 1685.0f),
            fund(QStringLiteral("F6HUK EA3IZF 73"), -21.0f, 2159.0f),
            fund(QStringLiteral("OE5SOS PD4RJ JO33"), 12.0f, 1338.0f),
            fund(QStringLiteral("OE5SOS 9A6T JN85"), 11.0f, 1428.0f),
            fund(QStringLiteral("OE5SOS DL0HAM JO53"), 9.0f, 812.0f),
        }, kFenster + 60000);
        // Wie lange sie schon warten -- so, wie es im Betrieb aussaehe.
        Ft8Warteschlange& q = s.applet.schlangeForTestSchreibend();
        q.leeren();
        q.merkeAnruf(QStringLiteral("PD4RJ"), 12, QStringLiteral("JO33"), jetzt - 40000, 1338);
        q.merkeAnruf(QStringLiteral("9A6T"), 11, QStringLiteral("JN85"), jetzt - 70000, 1428);
        q.merkeAnruf(QStringLiteral("DL0HAM"), 9, QStringLiteral("JO53"), jetzt - 125000, 812);
        q.merkeAnruf(QStringLiteral("SP9XYZ"), -7, QStringLiteral("KO00"), jetzt - 270000, 1906);
        s.applet.zeigeDannForTest();
        Ft8DannListe* d = s.applet.dannForTest();
        QVERIFY(d->zeileForTest(1));
        ueber(d->zeileForTest(1));
        QVERIFY(aufZeile(s.applet, QStringLiteral("CQ IK6CGO")));
        QTest::qWait(50);
        QVERIFY(s.applet.grab().save(dir + QStringLiteral("/C-gebaut-voll.png")));

        // Aufgeklappt, blass am Ende.
        weg(d->zeileForTest(1));
        klick(d->mehrKnopfForTest());
        QTest::qWait(50);
        QVERIFY(s.applet.grab().save(dir + QStringLiteral("/C-gebaut-aufgeklappt.png")));
        klick(d->mehrKnopfForTest());

        // Nach dem RR73: PD4RJ ist dran.
        q.merkeGearbeitet(QStringLiteral("HB9ZHK"), jetzt);
        QVERIFY(s.applet.rufeNaechstenWartendenForTest());
        s.applet.zeigeDannForTest();
        QTest::qWait(50);
        QVERIFY(s.applet.grab().save(dir + QStringLiteral("/C-gebaut-danach.png")));

        // Niemand wartet.
        q.leeren();
        s.applet.zeigeDannForTest();
        QTest::qWait(50);
        QVERIFY(s.applet.grab().save(dir + QStringLiteral("/C-gebaut-leer.png")));
    }
};

QTEST_MAIN(TstFt8Anruferliste)
#include "tst_ft8_anruferliste.moc"
