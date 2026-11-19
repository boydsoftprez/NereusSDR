// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_ft8_fenster_schmal.cpp  (Longpath)
// =================================================================
//
// Das Fenster „FT8 DECODER" laesst sich schmal ziehen, und nichts
// faellt rechts hinaus.
//
// ── Warum es diese Pruefung gibt ────────────────────────────────────
//
// Betreiber am 2026-11-18, mit Bildern seines Fensters bei rund 460
// bis 840 Punkt Breite: „hier finde ich nun keinen button mehr für
// CQ" -- „versteckt sich ganz rechts, somit ist es nicht sichtbar" --
// „man sollte hier zu sehen auch dieses fenster kleiner ziehen können,
// alle pillen usw. müssen natürlich dann auch verschoben werden".
//
// Die drei Reihen des Fensters -- oben die Verfahren mit Waterfall,
// TX Hz, Passes und Zustand; in der Mitte die Sicht-Pillen mit Zaehler
// und Clear; unten Manual, Textfeld, CQ, x30, slots, Free, Send, Stop
// -- waren je EINE Zeile. Eine Zeile hat eine Mindestbreite: die Summe
// ihrer Teile. Schmaler ging das Fenster zwar, aber der Rollbereich
// darunter rollte dann waagrecht, und was rechts stand, war weg. Rechts
// stehen CQ, Send und Stop: die Sendesteuerung.
//
// ── Was geprueft wird ───────────────────────────────────────────────
//
// Am ECHTEN Fenster (`AppletFloatingWindow`), nicht an einer Zahl aus
// der Anordnung: bei jeder Breite liegt jedes Bedienelement ganz im
// Sichtbereich, nichts ueberlappt, kein waagrechter Rollbalken. Der
// Lauf braucht nichts von dem, was dafuer gebaut wurde -- er findet
// die Knoepfe an ihrer Aufschrift -- und laesst sich darum auch gegen
// die Fassung davor bauen. Gegenprobe gefahren am 2026-11-18
// (Quelltext per Patch zurueck, nur dieser Lauf gebaut): 9 der 15
// Faelle rot, jeder mit „waagrechter Rollbalken (Inhalt 653 breit …)"
// oder „Stop liegt bei 595 …, der Sichtbereich ist 260 breit". Gruen
// bleiben dort nur die Faelle ab 700 Punkt Breite.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-18 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QDir>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyleFactory>
#include <QTimeZone>

#include <memory>

#include "core/AppSettings.h"
#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/Ft8Applet.h"
#include "gui/styles/AppTheme.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "FensterFrist.h"

using namespace Longpath;

namespace {

/// Das FT8-Fenster, wie es beim Betreiber schwebt.
struct Aufbau {
    RadioModel                            radio;
    Ft8Applet*                            panel{nullptr};
    std::unique_ptr<AppletFloatingWindow> fenster;
    QScrollArea*                          roll{nullptr};

    Aufbau()
    {
        panel = new Ft8Applet(&radio, nullptr);
        fenster = std::make_unique<AppletFloatingWindow>(
            panel, QStringLiteral("Ft8"), 0, nullptr);
        roll = fenster->findChild<QScrollArea*>();
    }

    /// Ein laufendes QSO und zwei, die dazwischenrufen -- die Szene
    /// vom 2026-10-30 (HB9ZHK im QSO, PD4RJ und 9A6T warten).
    void belebe()
    {
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        if (!slice) { return; }
        slice->setFrequency(14074000.0);
        panel->setSlice(slice);
        panel->ablaufForTestSchreibend().starteMitAnrufer(
            QStringLiteral("HB9ZHK"), QStringLiteral("OE5SOS"),
            QStringLiteral("JN67"), 18, QStringLiteral("JN47"));

        const QDateTime fenster =
            QDateTime::fromMSecsSinceEpoch(999999990000LL, QTimeZone::UTC);
        QVector<Ft8Decode> funde;
        const struct { const char* text; float snr; float hz; } zeilen[] = {
            { "CQ UN0GY MO52",        -3.0f, 1337.0f },
            { "OE5SOS PD4RJ JO33",    -9.0f, 1512.0f },
            { "CQ DX JS6TWW PL36",   -17.0f,  844.0f },
            { "DL1ABC R6FMM KN95",    -8.0f, 1790.0f },
            { "OE5SOS 9A6T JN75",     11.0f, 2233.0f },
            { "F5PZT OE3TVJ RR73",     4.0f,  611.0f } };
        for (const auto& z : zeilen) {
            Ft8Decode d;
            d.snrDb = z.snr; d.dtSeconds = 0.2f; d.freqHz = z.hz;
            d.score = 40;    d.text = QString::fromLatin1(z.text);
            funde.append(d);
        }
        QMetaObject::invokeMethod(
            panel, "onDecoded", Qt::DirectConnection,
            Q_ARG(QVector<Longpath::Ft8Decode>, funde),
            Q_ARG(QDateTime, fenster));
        panel->zeigeAblaufForTest();
    }

    /// Eine feste, kleine Zahl von Runden statt einer Schleife ohne
    /// Ende: eine Groessenaenderung laeuft ueber mehrere gepostete
    /// Ereignisse (Zeilenumbruch -> Hoehe -> Rollbalken -> Breite).
    static void beruhige()
    {
        for (int runde = 0; runde < 12; ++runde) {
            QCoreApplication::sendPostedEvents();
            QCoreApplication::processEvents();
        }
    }

    void zieheAuf(int breite, int hoehe)
    {
        fenster->resize(breite, hoehe);
        beruhige();
    }

    bool rolltWaagrecht() const
    {
        const QScrollBar* b = roll->horizontalScrollBar();
        return b->isVisible() && b->maximum() > b->minimum();
    }

    /// Ein Knopf an seiner Aufschrift -- so, wie der Betreiber ihn
    /// sucht.
    QPushButton* knopf(const QString& text) const
    {
        for (QPushButton* b : panel->findChildren<QPushButton*>()) {
            if (b->text() == text && b->isVisibleTo(panel)) { return b; }
        }
        return nullptr;
    }

    /// Wo ein Widget im Sichtbereich des Rollbereichs liegt.
    QRect imSichtbereich(const QWidget* w) const
    {
        return QRect(w->mapTo(roll->viewport(), QPoint(0, 0)), w->size());
    }

    /// Alles, was man anfassen oder ablesen kann: Knoepfe, Drehfelder,
    /// Eingabefelder und die Beschriftungen der Reihen. NICHT, was in
    /// einem Drehfeld oder in der Fundliste steckt (deren Innenleben
    /// gehoert ihnen), und nicht die Fundliste selbst -- sie darf
    /// waagrecht abschneiden.
    QList<QWidget*> bedienelemente() const
    {
        QList<QWidget*> liste;
        for (QWidget* w : panel->findChildren<QWidget*>()) {
            if (!w->isVisibleTo(panel)) { continue; }
            const bool element =
                qobject_cast<QAbstractButton*>(w)
                || qobject_cast<QAbstractSpinBox*>(w)
                || qobject_cast<QLineEdit*>(w)
                || qobject_cast<QLabel*>(w);
            if (!element) { continue; }
            bool innen = false;
            for (QWidget* p = w->parentWidget(); p && p != panel;
                 p = p->parentWidget()) {
                if (qobject_cast<QAbstractSpinBox*>(p)
                    || qobject_cast<QPlainTextEdit*>(p)) {
                    innen = true;
                    break;
                }
            }
            if (innen) { continue; }
            // Eine leere Beschriftung nimmt keinen Platz und sagt
            // nichts (die Uhrwarnung, solange die Uhr stimmt).
            if (auto* l = qobject_cast<QLabel*>(w)) {
                if (l->text().isEmpty() && l->pixmap().isNull()) { continue; }
            }
            liste.append(w);
        }
        return liste;
    }
};

QString nenne(const QWidget* w)
{
    QString text;
    if (auto* b = qobject_cast<const QAbstractButton*>(w)) { text = b->text(); }
    else if (auto* l = qobject_cast<const QLabel*>(w)) { text = l->text().left(24); }
    else if (auto* s = qobject_cast<const QAbstractSpinBox*>(w)) { text = s->text(); }
    else if (auto* e = qobject_cast<const QLineEdit*>(w)) { text = e->placeholderText(); }
    return QStringLiteral("%1 „%2\"")
        .arg(QString::fromLatin1(w->metaObject()->className()), text);
}

/// Die Anordnung, in der ein Widget steckt -- auch wenn sie eine
/// Unter-Anordnung ist und nicht die seines Elternwidgets.
QLayout* anordnungMit(QLayout* wurzel, QWidget* gesucht)
{
    if (!wurzel) { return nullptr; }
    for (int i = 0; i < wurzel->count(); ++i) {
        QLayoutItem* it = wurzel->itemAt(i);
        if (it->widget() == gesucht) { return wurzel; }
        if (QLayout* unter = it->layout()) {
            if (QLayout* fund = anordnungMit(unter, gesucht)) { return fund; }
        }
        if (QWidget* w = it->widget()) {
            if (QLayout* fund = anordnungMit(w->layout(), gesucht)) {
                return fund;
            }
        }
    }
    return nullptr;
}

/// Liegt alles im Fenster? Gibt die Liste dessen zurueck, was es NICHT
/// tut -- leer heisst gut.
QStringList verstoesse(const Aufbau& a)
{
    QStringList aus;
    const QRect sicht(QPoint(0, 0), a.roll->viewport()->size());

    if (a.rolltWaagrecht()) {
        aus << QStringLiteral("waagrechter Rollbalken (Inhalt %1 breit, "
                              "Sichtbereich %2)")
                   .arg(a.panel->width()).arg(sicht.width());
    }
    const QList<QWidget*> alle = a.bedienelemente();
    QList<QRect> lagen;
    for (QWidget* w : alle) {
        const QRect r = a.imSichtbereich(w);
        lagen.append(r);
        if (r.left() < 0 || r.right() >= sicht.width()) {
            aus << QStringLiteral("%1 ragt hinaus: x %2..%3 bei Breite %4")
                       .arg(nenne(w)).arg(r.left()).arg(r.right())
                       .arg(sicht.width());
        }
    }
    for (int i = 0; i < alle.size(); ++i) {
        for (int j = i + 1; j < alle.size(); ++j) {
            // Eltern und Kind duerfen einander enthalten.
            if (alle.at(i)->isAncestorOf(alle.at(j))
                || alle.at(j)->isAncestorOf(alle.at(i))) {
                continue;
            }
            if (lagen.at(i).intersects(lagen.at(j))) {
                aus << QStringLiteral("%1 und %2 liegen uebereinander")
                           .arg(nenne(alle.at(i)), nenne(alle.at(j)));
            }
        }
    }
    return aus;
}

}  // namespace

class TstFt8FensterSchmal : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Wie main.cpp. Der Stil redet hier mit: er bestimmt, wie breit
        // ein Knopf und ein Drehfeld sind und wie viel Luft zwischen
        // ihnen liegt.
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        applyDarkPalette(*qApp);
        applyAppBaselineQss(*qApp);

        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("StationGridSquare"),
                                         QStringLiteral("JN67"));
        // Vier Plaetze und dreissig Rufe, wie auf seinen Bildern: so
        // sind die Drehfelder am breitesten („slots 4", „x30").
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QStringLiteral("4"));
        AppSettings::instance().setValue(QStringLiteral("Ft8/CqVersuche"),
                                         QStringLiteral("30"));
    }

    // ── Die vier Breiten aus dem Auftrag ──────────────────────────
    void beiJederBreiteLiegtAllesImFenster_data()
    {
        QTest::addColumn<int>("breite");
        for (int b : {420, 520, 700, 1000}) {
            QTest::newRow(qPrintable(QString::number(b))) << b;
        }
    }
    void beiJederBreiteLiegtAllesImFenster()
    {
        QFETCH(int, breite);
        Aufbau a;
        a.fenster->resize(breite, 640);
        a.fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(a.fenster.get(), kFensterFrist));
        Aufbau::beruhige();
        QCOMPARE(a.fenster->width(), breite);

        const QStringList v = verstoesse(a);
        QVERIFY2(v.isEmpty(), qPrintable(v.join(QStringLiteral("\n   "))));
    }

    // ── Die Sendesteuerung bleibt IMMER da ────────────────────────
    //
    // CQ, Send, Stop -- und Stop zuallererst: wer sendet und den Knopf
    // zum Anhalten nicht findet, hat ein Problem, das groesser ist als
    // ein Schoenheitsfehler. Von der schmalsten Breite, die das Fenster
    // zulaesst, bis weit ueber die groesste aus seinen Bildern.
    void cqSendUndStopSindBeiJederBreiteZuSehen()
    {
        // 760 hoch: bei 260 Punkt Breite brechen die drei Reihen in
        // zehn Zeilen um, und die brauchen ihre Hoehe. Dass das Fenster
        // dann nicht frueher rollt als noetig, prueft der naechste Fall.
        constexpr int kHoehe = 760;
        Aufbau a;
        a.fenster->resize(1100, kHoehe);
        a.fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(a.fenster.get(), kFensterFrist));
        Aufbau::beruhige();

        const int schmal = qMax(a.fenster->minimumWidth(), 260);
        for (int breite = schmal; breite <= 1100; breite += 13) {
            a.zieheAuf(breite, kHoehe);
            const QRect sicht(QPoint(0, 0), a.roll->viewport()->size());
            for (const QString& name : {QStringLiteral("Stop"),
                                        QStringLiteral("Send"),
                                        QStringLiteral("CQ"),
                                        QStringLiteral("Manual"),
                                        QStringLiteral("Clear"),
                                        QStringLiteral("FT8")}) {
                // „CQ" gibt es zweimal: die Sicht-Pille und den Ruf.
                // Beide muessen da sein.
                int gefunden = 0;
                for (QPushButton* b : a.panel->findChildren<QPushButton*>()) {
                    if (b->text() != name || !b->isVisibleTo(a.panel)) {
                        continue;
                    }
                    ++gefunden;
                    const QRect r = a.imSichtbereich(b);
                    QVERIFY2(sicht.contains(r),
                             qPrintable(QStringLiteral(
                                 "Breite %1: „%2\" liegt bei %3,%4 %5x%6, "
                                 "der Sichtbereich ist %7x%8")
                                 .arg(breite).arg(name)
                                 .arg(r.left()).arg(r.top())
                                 .arg(r.width()).arg(r.height())
                                 .arg(sicht.width()).arg(sicht.height())));
                }
                QVERIFY2(gefunden >= 1,
                         qPrintable(QStringLiteral("Breite %1: kein Knopf "
                                                   "„%2\"").arg(breite).arg(name)));
            }
            QVERIFY2(!a.rolltWaagrecht(),
                     qPrintable(QStringLiteral("Breite %1: waagrechter "
                                               "Rollbalken").arg(breite)));
            // „Clear" steht am rechten Rand -- auch wenn es mit dem
            // Zaehler in die zweite Zeile gerutscht ist.
            const QRect clear = a.imSichtbereich(a.knopf(QStringLiteral("Clear")));
            QVERIFY2(clear.right() > sicht.width() - 30,
                     qPrintable(QStringLiteral("Breite %1: „Clear\" endet bei "
                                               "%2 von %3")
                                    .arg(breite).arg(clear.right())
                                    .arg(sicht.width())));
        }
    }

    // ── Die Fundliste gibt zuerst nach ────────────────────────────
    //
    // Brechen die Reihen um, wird das Fenster hoeher. Bevor es deshalb
    // senkrecht rollt -- und die Sendereihe unter den Rand rutscht --,
    // gibt die Fundliste Platz ab: sie wuenscht sich 192 Punkt und
    // kommt mit 120 aus. Qt rechnete im Rollbereich mit der
    // Wunschhoehe; in den 72 Punkt dazwischen lag `Stop` unter dem
    // Fenster, obwohl ueber ihm noch Luft war.
    void dieFundlisteGibtPlatzAbBevorStopVerschwindet()
    {
        Aufbau a;
        a.fenster->resize(420, 900);
        a.fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(a.fenster.get(), kFensterFrist));
        Aufbau::beruhige();

        // So hoch, dass alles gerade hineinpasst, wenn die Liste auf
        // ihre Mindesthoehe geht -- und zehn Punkt dazu.
        const int rand   = a.fenster->height() - a.roll->viewport()->height();
        const int knapp  = a.panel->layout()->totalMinimumHeightForWidth(
                               a.roll->viewport()->width());
        const int wunsch = a.panel->layout()->totalHeightForWidth(
                               a.roll->viewport()->width());
        QVERIFY2(wunsch > knapp + 20,
                 qPrintable(QStringLiteral("Wunsch %1, knapp %2 -- so prueft "
                                           "dieser Fall nichts")
                                .arg(wunsch).arg(knapp)));
        a.zieheAuf(420, knapp + rand + 10);

        const QScrollBar* senkrecht = a.roll->verticalScrollBar();
        QVERIFY2(!(senkrecht->isVisible()
                   && senkrecht->maximum() > senkrecht->minimum()),
                 qPrintable(QStringLiteral("es rollt senkrecht, obwohl die "
                                           "Liste noch %1 Punkt abgeben "
                                           "koennte").arg(wunsch - knapp)));
        const QRect sicht(QPoint(0, 0), a.roll->viewport()->size());
        for (const QString& name : {QStringLiteral("Stop"),
                                    QStringLiteral("Send")}) {
            QPushButton* b = a.knopf(name);
            QVERIFY(b);
            QVERIFY2(sicht.contains(a.imSichtbereich(b)), qPrintable(name));
        }
        const QStringList v = verstoesse(a);
        QVERIFY2(v.isEmpty(), qPrintable(v.join(QStringLiteral("\n   "))));
    }

    // ── Mit laufendem QSO und Wartenden ───────────────────────────
    //
    // Die Arbeitsleiste zeigt dann den naechsten Zug und darunter die
    // Schlange -- Zeilen mit festen Spalten, zusammen 346 Punkt breit.
    // So breit musste das Fenster sein, sobald jemand wartete: genau
    // dann, wenn man `Stop` am ehesten braucht.
    void mitWartendenBleibtAllesImFenster_data()
    {
        QTest::addColumn<int>("breite");
        for (int b : {340, 420, 520, 1000}) {
            QTest::newRow(qPrintable(QString::number(b))) << b;
        }
    }
    void mitWartendenBleibtAllesImFenster()
    {
        QFETCH(int, breite);
        Aufbau a;
        a.belebe();
        a.fenster->resize(breite, 800);
        a.fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(a.fenster.get(), kFensterFrist));
        Aufbau::beruhige();

        // Die Schlange steht wirklich da -- sonst prueft der Fall nichts.
        int rufe = 0;
        for (QLabel* l : a.panel->findChildren<QLabel*>()) {
            if (l->isVisibleTo(a.panel)
                && (l->text() == QStringLiteral("PD4RJ")
                    || l->text() == QStringLiteral("9A6T"))) {
                ++rufe;
            }
        }
        QVERIFY2(rufe >= 2, "die beiden Wartenden stehen nicht in der Leiste");

        const QStringList v = verstoesse(a);
        QVERIFY2(v.isEmpty(), qPrintable(v.join(QStringLiteral("\n   "))));
        const QRect sicht(QPoint(0, 0), a.roll->viewport()->size());
        QVERIFY(sicht.contains(a.imSichtbereich(a.knopf(QStringLiteral("Stop")))));
    }

    // ── Auf dem Weg von breit nach schmal und zurueck ─────────────
    //
    // Beim Ziehen am Rand: bei jeder Breite alles im Fenster, nichts
    // uebereinander. Und der Rueckweg endet, wo der Hinweg begann --
    // das Fenster bleibt nicht in der umgebrochenen Form haengen.
    void beimZiehenBleibtAllesImFenster()
    {
        Aufbau a;
        a.fenster->resize(1000, 640);
        a.fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(a.fenster.get(), kFensterFrist));
        Aufbau::beruhige();

        QPushButton* stop = a.knopf(QStringLiteral("Stop"));
        QVERIFY(stop);
        const QRect stopBreit = a.imSichtbereich(stop);

        for (int breite = 1000; breite >= 400; breite -= 29) {
            a.zieheAuf(breite, 640);
            const QStringList v = verstoesse(a);
            QVERIFY2(v.isEmpty(),
                     qPrintable(QStringLiteral("Breite %1:\n   ").arg(breite)
                                + v.join(QStringLiteral("\n   "))));
        }
        a.zieheAuf(1000, 640);
        QCOMPARE(a.imSichtbereich(stop), stopBreit);
    }

    // ── Breit bleibt es, wie es war ───────────────────────────────
    //
    // Der Auftrag ist die Anordnung im SCHMALEN Fenster. Ist Platz,
    // steht jede der drei Reihen in einer Zeile, in derselben
    // Reihenfolge wie bisher.
    void imBreitenFensterStehenDieReihenInEinerZeile()
    {
        Aufbau a;
        a.fenster->resize(1000, 640);
        a.fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(a.fenster.get(), kFensterFrist));
        Aufbau::beruhige();

        auto mitte = [&a](const QString& text) {
            QPushButton* b = a.knopf(text);
            return b ? a.imSichtbereich(b).center() : QPoint(-1, -1);
        };
        auto pruefeZeile = [&](const QStringList& namen) {
            QPoint davor(-1, -1);
            for (const QString& n : namen) {
                const QPoint p = mitte(n);
                QVERIFY2(p.x() >= 0, qPrintable(n));
                if (davor.x() >= 0) {
                    QVERIFY2(qAbs(p.y() - davor.y()) <= 2,
                             qPrintable(QStringLiteral("„%1\" steht nicht in "
                                                       "der Zeile davor").arg(n)));
                    QVERIFY2(p.x() > davor.x(),
                             qPrintable(QStringLiteral("„%1\" steht nicht "
                                                       "rechts davon").arg(n)));
                }
                davor = p;
            }
        };
        pruefeZeile({QStringLiteral("FT8"), QStringLiteral("FT4"),
                     QStringLiteral("Hound"), QStringLiteral("Contest"),
                     QStringLiteral("Waterfall")});
        if (QTest::currentTestFailed()) { return; }
        pruefeZeile({QStringLiteral("All"), QStringLiteral("To me"),
                     QStringLiteral("New DXCC"), QStringLiteral("New grid"),
                     QStringLiteral("My freq"), QStringLiteral("Clear")});
        if (QTest::currentTestFailed()) { return; }
        pruefeZeile({QStringLiteral("Manual"), QStringLiteral("Free"),
                     QStringLiteral("Send"), QStringLiteral("Stop")});
        if (QTest::currentTestFailed()) { return; }

        // „Clear" und „Stop" stehen am rechten Rand, nicht irgendwo.
        const int rand = a.roll->viewport()->width();
        QVERIFY(a.imSichtbereich(a.knopf(QStringLiteral("Clear"))).right()
                > rand - 30);
        QVERIFY(a.imSichtbereich(a.knopf(QStringLiteral("Stop"))).right()
                > rand - 30);
        // Und das Textfeld nimmt, was in seiner Zeile uebrig ist.
        QLineEdit* feld = nullptr;
        for (QLineEdit* e : a.panel->findChildren<QLineEdit*>()) {
            if (!e->placeholderText().isEmpty()) { feld = e; }
        }
        QVERIFY(feld);
        QVERIFY2(feld->width() > 300,
                 qPrintable(QString::number(feld->width())));
    }

    // ── Eine weitere Pille passt dazu ─────────────────────────────
    //
    // Auf dem Zweig `feat/ft2` kommt neben FT8 und FT4 ein drittes
    // Verfahren. Es wird an dieselbe Reihe gehaengt -- und die muss
    // das aushalten, ohne dass wieder etwas hinausfaellt.
    void eineWeiterePilleInDerKopfreiheBringtNichtsZumKippen()
    {
        Aufbau a;
        QPushButton* ft4 = a.knopf(QStringLiteral("FT4"));
        QVERIFY(ft4);
        QLayout* reihe = anordnungMit(a.panel->layout(), ft4);
        QVERIFY(reihe);
        auto* ft2 = new QPushButton(QStringLiteral("FT2"), a.panel);
        ft2->setFixedSize(48, 22);
        reihe->addWidget(ft2);

        a.fenster->resize(420, 640);
        a.fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(a.fenster.get(), kFensterFrist));
        Aufbau::beruhige();

        const QStringList v = verstoesse(a);
        QVERIFY2(v.isEmpty(), qPrintable(v.join(QStringLiteral("\n   "))));
        QVERIFY(QRect(QPoint(0, 0), a.roll->viewport()->size())
                    .contains(a.imSichtbereich(ft2)));
    }

    // ── Blaetter, wenn eines verlangt wird ────────────────────────
    //
    // Je Breite EIN Bild vom echten Fenster. Braucht LONGPATH_GRAB_DIR
    // **und** LONGPATH_ENTWURF=1 (die CI setzt das erste fuer ihre
    // eigenen Abgriffe). Mit QT_SCALE_FACTOR=2 und QT_FONT_DPI=72
    // entstehen sie in wirklicher Groesse: offscreen rechnet
    // Punktschriften sonst mit rund 100 dpi, das Programm auf dem Mac
    // mit 72.
    void blaetter()
    {
        const QString ziel = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (ziel.isEmpty() || qEnvironmentVariable("LONGPATH_ENTWURF")
                                  != QLatin1String("1")) {
            QSKIP("Werkzeug: braucht LONGPATH_GRAB_DIR und LONGPATH_ENTWURF=1.");
        }
        QDir().mkpath(ziel);

        for (int breite : {340, 420, 520, 700, 840, 1000}) {
            Aufbau a;
            a.belebe();
            // Hoch genug, dass auch die umgebrochene Fassung ganz
            // hineinpasst -- das Bild soll die Reihen zeigen, nicht den
            // Rollbalken.
            const int hoehe = breite < 600 ? 700 : 600;
            a.fenster->resize(breite, hoehe);
            a.fenster->show();
            QVERIFY(QTest::qWaitForWindowExposed(a.fenster.get(), kFensterFrist));
            Aufbau::beruhige();

            QImage bild = a.fenster->grab().toImage();
            const int dpm = int(72.0 * bild.devicePixelRatio() / 0.0254);
            bild.setDotsPerMeterX(dpm);
            bild.setDotsPerMeterY(dpm);
            const QString pfad = ziel
                + QStringLiteral("/ft8-decoder-%1x%2.png").arg(breite).arg(hoehe);
            QVERIFY2(bild.save(pfad), qPrintable(pfad));
            qInfo().noquote() << "Blatt:" << pfad << "| Fenster"
                              << a.fenster->size() << "| Inhalt"
                              << a.panel->size() << "| Verstoesse"
                              << verstoesse(a).size();
        }
    }
};

QTEST_MAIN(TstFt8FensterSchmal)
#include "tst_ft8_fenster_schmal.moc"
