// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original test tool, no Thetis logic.
//
// ── Entwurfsblaetter: alle Plaetze sehen (2026-11-13) ────────────────
//
// Betreiber, nachdem er `slots 3` eingestellt hatte und das Fenster
// „1 of 3" zeigte: „schön wäre es, wenn man dann auch alle 3 sieht,
// sprich 3 fenster und ein fenster durch 3 geteilt."
//
// Heute malt `Ft8PlaetzeFlaeche` eine Kachel je BELEGTEM Platz. Bei
// drei eingestellten und einem belegten steht eine Kachel da, und
// dass zwei Plaetze frei warten, sieht man nur an der Zahl in der
// Kopfzeile.
//
// Werkzeug, kein Pruefstand: ohne LONGPATH_GRAB_DIR ueberspringt es.
// Je Entwurf EIN Blatt (Martins Regel), darauf alle Betriebsfaelle:
// 1 von 3, 2 von 3, 3 von 3 belegt.
//
//   QT_SCALE_FACTOR=2 LONGPATH_GRAB_DIR=<ordner> \
//     ctest -R tst_ft8_plaetze_entwuerfe -V

#include <QtTest/QtTest>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QStandardPaths>

#include <memory>

#include "gui/StyleConstants.h"
#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/Ft8PlaetzeApplet.h"
#include "gui/applets/Ft8PlatzStand.h"
#include "gui/styles/AppTheme.h"
#include "models/RadioModel.h"
#include "FensterFrist.h"

using namespace Longpath;

namespace {

QColor farbe(const char* token) { return QColor(Style::hexRole(token)); }

// Ein Platz, so viel wie das Blatt davon braucht.
struct Platz {
    QString ruf;        // leer = frei
    QString land;
    QString stand;      // „sent -05", „waiting for his report", ...
    int     tonHz = 0;
    bool    sendet = false;
};

constexpr int kRandX   = 10;
constexpr int kKopfH   = 22;
// Die echten Masse aus Ft8PlaetzeApplet.h -- geschaetzte (erst 64)
// liessen Land und Stand uebereinander laufen, und ein Entwurf, der
// enger malt als das Gebaute, entscheidet die falsche Frage.
constexpr int kVollH   = 92;    // = Ft8PlaetzeFlaeche::kKachelH
constexpr int kLuftY   = 8;     // = Ft8PlaetzeFlaeche::kKachelLuft
constexpr int kSchmalH = 30;    // freie Kachel in Entwurf B

// Die Kopfzeile mit „on air" und „n of m" -- unveraendert aus dem
// echten Widget uebernommen, damit die Blaetter nicht an einer
// Stelle schmeicheln, die gar nicht zur Wahl steht.
int maleKopf(QPainter& p, int breite, int belegt, int gesamt, bool sendet)
{
    const int w = breite - 2 * kRandX;
    QFont klein = p.font();
    klein.setPointSizeF(9.0);
    p.setFont(klein);
    p.setPen(farbe(Style::kTextInactive));
    p.drawText(QRect(kRandX + w - 60, 4, 60, 14),
               Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("%1 of %2").arg(belegt).arg(gesamt));
    if (sendet) {
        p.setPen(Qt::NoPen);
        p.setBrush(farbe(Style::kTxKeyBorder));
        p.drawEllipse(QRectF(kRandX + w - 112, 7.5, 6, 6));
        p.setPen(farbe(Style::kTxKeyBorder));
        p.drawText(QRect(kRandX + w - 102, 4, 46, 14),
                   Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("on air"));
    }
    return kKopfH;
}

// Eine belegte Kachel, Aufbau wie heute.
void maleBelegt(QPainter& p, int breite, int y, int nummer, const Platz& z)
{
    const int w = breite - 2 * kRandX;
    const QRect r(kRandX, y, w, kVollH);
    p.setPen(farbe(Style::kBorderSubtle));
    p.setBrush(farbe(Style::kTileActiveBg));
    p.drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);

    QFont klein = p.font();
    klein.setPointSizeF(9.0);
    p.setFont(klein);
    p.setPen(farbe(Style::kTextInactive));
    p.drawText(QRect(r.left() + 8, r.top() + 10, 14, 14),
               Qt::AlignLeft | Qt::AlignVCenter, QString::number(nummer));

    const int tx = r.left() + 28;
    QFont gross = p.font();
    gross.setPointSizeF(16.0);
    p.setFont(gross);
    p.setPen(farbe(Style::kTextPrimary));
    p.drawText(QRect(tx, r.top() + 8, w - 130, 26),
               Qt::AlignLeft | Qt::AlignVCenter, z.ruf);

    klein.setPointSizeF(9.0);
    p.setFont(klein);
    p.setPen(farbe(Style::kTextScale));
    p.drawText(QRect(tx, r.top() + 38, w - 130, 16),
               Qt::AlignLeft | Qt::AlignVCenter, z.land);

    QFont mono(QStringLiteral("Menlo"));
    mono.setPointSizeF(9.5);
    p.setFont(mono);
    p.setPen(farbe(Style::kTextSecondary));
    p.drawText(QRect(tx, r.top() + 60, w - 36, 18),
               Qt::AlignLeft | Qt::AlignVCenter, z.stand);

    klein.setPointSizeF(10.0);
    p.setFont(klein);
    p.setPen(farbe(Style::kTextScale));
    p.drawText(QRect(r.right() - 108, r.top() + 10, 98, 18),
               Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("%1 Hz").arg(z.tonHz));
}

// ── Entwurf A: gleiche Felder ────────────────────────────────────────
// Das Fenster wird in so viele gleich hohe Kacheln geteilt, wie
// Plaetze eingestellt sind. Eine freie Kachel ist dieselbe Kachel,
// nur leer: gestrichelter Rand, Nummer, „free".
void entwurfA(QPainter& p, int breite, const QVector<Platz>& plaetze, int belegt)
{
    int y = maleKopf(p, breite, belegt, plaetze.size(), belegt > 0);
    for (int i = 0; i < plaetze.size(); ++i) {
        const Platz& z = plaetze.at(i);
        if (!z.ruf.isEmpty()) {
            maleBelegt(p, breite, y, i + 1, z);
        } else {
            const int w = breite - 2 * kRandX;
            const QRect r(kRandX, y, w, kVollH);
            QPen strich(farbe(Style::kBorderSubtle));
            strich.setStyle(Qt::DashLine);
            p.setPen(strich);
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);

            QFont klein = p.font();
            klein.setPointSizeF(9.0);
            p.setFont(klein);
            p.setPen(farbe(Style::kTextInactive));
            p.drawText(QRect(r.left() + 8, r.top() + 10, 14, 14),
                       Qt::AlignLeft | Qt::AlignVCenter, QString::number(i + 1));
            klein.setPointSizeF(11.0);
            p.setFont(klein);
            p.drawText(r, Qt::AlignCenter, QStringLiteral("free"));
        }
        y += kVollH + kLuftY;
    }
}

// ── Entwurf B: belegt gross, frei schmal ─────────────────────────────
// Alle Plaetze sind zu sehen, aber ein freier kostet nur einen
// Streifen. Das Fenster waechst mit dem Betrieb, nicht mit der
// Einstellung.
void entwurfB(QPainter& p, int breite, const QVector<Platz>& plaetze, int belegt)
{
    int y = maleKopf(p, breite, belegt, plaetze.size(), belegt > 0);
    for (int i = 0; i < plaetze.size(); ++i) {
        const Platz& z = plaetze.at(i);
        if (!z.ruf.isEmpty()) {
            maleBelegt(p, breite, y, i + 1, z);
            y += kVollH + kLuftY;
        } else {
            const int w = breite - 2 * kRandX;
            const QRect r(kRandX, y, w, kSchmalH);
            p.setPen(farbe(Style::kBorderSubtle));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
            QFont klein = p.font();
            klein.setPointSizeF(9.0);
            p.setFont(klein);
            p.setPen(farbe(Style::kTextInactive));
            p.drawText(QRect(r.left() + 8, r.top(), 14, kSchmalH),
                       Qt::AlignLeft | Qt::AlignVCenter, QString::number(i + 1));
            p.drawText(QRect(r.left() + 28, r.top(), w - 40, kSchmalH),
                       Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("free"));
            y += kSchmalH + kLuftY;
        }
    }
}

// ── Entwurf C: gleiche Felder MIT Tonstelle ──────────────────────────
// Wie A, aber jede Kachel traegt ihre Tonstelle -- auch die freie.
// Dann steht im Fenster, wo gesendet WIRD, nicht nur wo gesendet
// wurde: 2310, +60, +120 Hz, MSHVs Leiter.
void entwurfC(QPainter& p, int breite, const QVector<Platz>& plaetze, int belegt)
{
    int y = maleKopf(p, breite, belegt, plaetze.size(), belegt > 0);
    for (int i = 0; i < plaetze.size(); ++i) {
        const Platz& z = plaetze.at(i);
        if (!z.ruf.isEmpty()) {
            maleBelegt(p, breite, y, i + 1, z);
        } else {
            const int w = breite - 2 * kRandX;
            const QRect r(kRandX, y, w, kVollH);
            QPen strich(farbe(Style::kBorderSubtle));
            strich.setStyle(Qt::DashLine);
            p.setPen(strich);
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);

            QFont klein = p.font();
            klein.setPointSizeF(9.0);
            p.setFont(klein);
            p.setPen(farbe(Style::kTextInactive));
            p.drawText(QRect(r.left() + 8, r.top() + 10, 14, 14),
                       Qt::AlignLeft | Qt::AlignVCenter, QString::number(i + 1));

            klein.setPointSizeF(11.0);
            p.setFont(klein);
            p.setPen(farbe(Style::kTextInactive));
            p.drawText(QRect(r.left() + 28, r.top() + 6, w - 140, 24),
                       Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("free"));

            klein.setPointSizeF(10.0);
            p.setFont(klein);
            p.setPen(farbe(Style::kTextScale));
            p.drawText(QRect(r.right() - 110, r.top() + 8, 100, 18),
                       Qt::AlignRight | Qt::AlignVCenter,
                       QStringLiteral("%1 Hz").arg(z.tonHz));
        }
        y += kVollH + kLuftY;
    }
}

// Die drei Betriebsfaelle, immer mit drei eingestellten Plaetzen.
QVector<QVector<Platz>> faelle()
{
    const Platz frei1{ {}, {}, {}, 2310, false };
    const Platz frei2{ {}, {}, {}, 2370, false };
    const Platz frei3{ {}, {}, {}, 2430, false };

    QVector<Platz> einer{
        { QStringLiteral("R6FMM"), QStringLiteral("European Russia"),
          QStringLiteral("sent -05"), 2310, true },
        frei2, frei3 };

    QVector<Platz> zwei{
        { QStringLiteral("R6FMM"), QStringLiteral("European Russia"),
          QStringLiteral("sent -05"), 2310, true },
        { QStringLiteral("UN6LN"), QStringLiteral("Kazakhstan"),
          QStringLiteral("waiting for his report"), 2370, true },
        frei3 };

    QVector<Platz> drei{
        { QStringLiteral("R6FMM"), QStringLiteral("European Russia"),
          QStringLiteral("sent -05"), 2310, true },
        { QStringLiteral("UN6LN"), QStringLiteral("Kazakhstan"),
          QStringLiteral("waiting for his report"), 2370, true },
        { QStringLiteral("JS6TWW"), QStringLiteral("Japan"),
          QStringLiteral("sent RR73"), 2430, true } };

    Q_UNUSED(frei1);
    return { einer, zwei, drei };
}

int belegteVon(const QVector<Platz>& v)
{
    int n = 0;
    for (const Platz& z : v) { if (!z.ruf.isEmpty()) { ++n; } }
    return n;
}

} // namespace

class TstFt8PlaetzeEntwuerfe : public QObject { Q_OBJECT
private slots:
    void blaetter()
    {
        const QString ziel = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (ziel.isEmpty()) {
            QSKIP("Werkzeug: LONGPATH_GRAB_DIR setzen, dann malt es die Blaetter.");
        }
        QDir().mkpath(ziel);

        // Das echte Fenster misst 397 x 269; die Blaetter zeigen drei
        // Betriebsfaelle nebeneinander in genau dieser Breite.
        constexpr int kBreite = 372;
        constexpr int kLuft   = 18;
        constexpr int kHoehe  = 3 * (kVollH + kLuftY) + kKopfH + 10;

        struct Blatt {
            const char* name;
            const char* titel;
            void (*malen)(QPainter&, int, const QVector<Platz>&, int);
        };
        const QVector<Blatt> blaetter{
            { "A-gleiche-felder", "A — gleiche Felder: jeder Platz eine Kachel, "
              "frei gestrichelt", entwurfA },
            { "B-belegt-gross-frei-schmal", "B — belegt gross, frei als Streifen: "
              "das Fenster waechst mit dem Betrieb", entwurfB },
            { "C-mit-tonstelle", "C — wie A, aber jede Kachel traegt ihre "
              "Tonstelle (2310 / +60 / +120 Hz)", entwurfC },
        };

        const QVector<QVector<Platz>> f = faelle();

        for (const Blatt& b : blaetter) {
            const int breiteGes = 3 * kBreite + 4 * kLuft;
            QImage bild(breiteGes, kHoehe + 54, QImage::Format_ARGB32_Premultiplied);
            bild.setDotsPerMeterX(int(144 / 0.0254));   // 144 dpi: wirkliche Groesse
            bild.setDotsPerMeterY(int(144 / 0.0254));
            bild.fill(farbe(Style::kAppBg));

            QPainter p(&bild);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setRenderHint(QPainter::TextAntialiasing, true);

            QFont titel = p.font();
            titel.setPointSizeF(11.0);
            titel.setBold(true);
            p.setFont(titel);
            p.setPen(farbe(Style::kTextPrimary));
            p.drawText(QRect(kLuft, 8, breiteGes - 2 * kLuft, 20),
                       Qt::AlignLeft | Qt::AlignVCenter, QString::fromUtf8(b.titel));

            for (int i = 0; i < f.size(); ++i) {
                const int x = kLuft + i * (kBreite + kLuft);
                p.save();
                p.translate(x, 36);
                // Der Fenstergrund, wie ihn das Applet selbst malt.
                p.fillRect(QRect(0, 0, kBreite, kHoehe), farbe(Style::kPanelBg));
                p.setPen(farbe(Style::kBorderSubtle));
                p.setBrush(Qt::NoBrush);
                p.drawRect(QRect(0, 0, kBreite - 1, kHoehe - 1));
                b.malen(p, kBreite, f.at(i), belegteVon(f.at(i)));
                p.restore();

                QFont klein = p.font();
                klein.setPointSizeF(9.0);
                klein.setBold(false);
                p.setFont(klein);
                p.setPen(farbe(Style::kTextInactive));
                p.drawText(QRect(x, 36 + kHoehe + 2, kBreite, 14),
                           Qt::AlignHCenter | Qt::AlignVCenter,
                           QStringLiteral("%1 von 3 belegt").arg(belegteVon(f.at(i))));
            }
            p.end();

            const QString pfad = ziel + QLatin1Char('/')
                + QStringLiteral("ft8-plaetze-%1.png").arg(QLatin1String(b.name));
            QVERIFY2(bild.save(pfad), qPrintable(pfad));
            qInfo().noquote() << "Blatt:" << pfad;
        }
    }

    // ── Das GEBAUTE, nicht das Gemalte (2026-11-13) ──────────────────
    //
    // Martins Regel: „Nach der Wahl mit dem ECHTEN Widget nachrendern
    // -- freihand nachgemalte Entwuerfe sehen im eingebauten Zustand
    // anders aus." Dieses Blatt nimmt `Ft8PlaetzeFlaeche` selbst und
    // faehrt dieselben drei Betriebsfaelle durch.
    void nachgerendert()
    {
        const QString ziel = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (ziel.isEmpty()) { QSKIP("Werkzeug: LONGPATH_GRAB_DIR setzen."); }
        QDir().mkpath(ziel);

        constexpr int kBreite = 372;
        constexpr int kLuft   = 18;

        auto stand = [](int belegt) {
            Ft8PlaetzeStand s;
            s.plaetzeGesamt = 3;
            s.sendet = belegt > 0;
            const struct { const char* ruf; const char* land; int ton; }
                quellen[] = {
                    { "R6FMM",  "European Russia", 2310 },
                    { "UN6LN",  "Kazakhstan",      2370 },
                    { "JS6TWW", "Japan",           2430 } };
            for (int i = 0; i < belegt; ++i) {
                Ft8PlatzStand z;
                z.partner = QString::fromLatin1(quellen[i].ruf);
                z.land    = QString::fromLatin1(quellen[i].land);
                z.tonHz   = quellen[i].ton;
                s.plaetze.append(z);
            }
            return s;
        };

        // Die hoechste der drei Lagen gibt die Blatthoehe.
        Ft8PlaetzeFlaeche mass;
        mass.setzeStand(stand(3));
        const int kHoehe = mass.noetigeHoehe();

        const int breiteGes = 3 * kBreite + 4 * kLuft;
        QImage bild(breiteGes, kHoehe + 54, QImage::Format_ARGB32_Premultiplied);
        bild.setDotsPerMeterX(int(144 / 0.0254));
        bild.setDotsPerMeterY(int(144 / 0.0254));
        bild.fill(farbe(Style::kAppBg));

        QPainter p(&bild);
        QFont titel = p.font();
        titel.setPointSizeF(11.0);
        titel.setBold(true);
        p.setFont(titel);
        p.setPen(farbe(Style::kTextPrimary));
        p.drawText(QRect(kLuft, 8, breiteGes - 2 * kLuft, 20),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("Entwurf B, eingebaut — das echte Panel"));
        p.end();

        for (int i = 0; i < 3; ++i) {
            Ft8PlaetzeFlaeche f;
            f.setzeStand(stand(i + 1));
            f.resize(kBreite, kHoehe);
            const QPixmap bild2 = f.grab();
            QPainter q(&bild);
            q.drawPixmap(kLuft + i * (kBreite + kLuft), 36, bild2);
            QFont klein = q.font();
            klein.setPointSizeF(9.0);
            q.setFont(klein);
            q.setPen(farbe(Style::kTextInactive));
            q.drawText(QRect(kLuft + i * (kBreite + kLuft), 36 + kHoehe + 2,
                             kBreite, 14),
                       Qt::AlignHCenter | Qt::AlignVCenter,
                       QStringLiteral("%1 von 3 belegt").arg(i + 1));
        }

        const QString pfad = ziel + QStringLiteral("/ft8-plaetze-B-eingebaut.png");
        QVERIFY2(bild.save(pfad), qPrintable(pfad));
        qInfo().noquote() << "Blatt:" << pfad;
    }

    // ── Die Anordnung nach der Groesse (2026-11-18) ──────────────────
    //
    // Betreiber: „FT8 SLOTS sollte man auch nebeneinander anordnen
    // können und soll sich automatsich an die Größe des Fenster
    // anpassen!"
    //
    // Je Zustand EIN Bild, und zwar vom echten Fenster
    // (`AppletFloatingWindow` mit Titelleiste und Rollbereich) in der
    // Groesse, die im Dateinamen steht -- nicht von der Flaeche
    // allein. Ob drei Kacheln nebeneinander passen, haengt am
    // Rollbalken und an der Titelleiste mit, und beides hat die
    // Flaeche nicht.
    //
    // Braucht LONGPATH_GRAB_DIR **und** LONGPATH_ENTWURF=1 (die CI
    // setzt das erste fuer ihre eigenen Abgriffe). Mit
    // QT_SCALE_FACTOR=2 entstehen die Bilder in 144 dpi, also in
    // wirklicher Groesse.
    void anordnungNachGroesse()
    {
        const QString ziel = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (ziel.isEmpty() || qEnvironmentVariable("LONGPATH_ENTWURF")
                                  != QLatin1String("1")) {
            QSKIP("Werkzeug: braucht LONGPATH_GRAB_DIR und LONGPATH_ENTWURF=1.");
        }
        QDir().mkpath(ziel);
        applyDarkPalette(*qApp);
        applyAppBaselineQss(*qApp);

        // Was auf den Kacheln steht, wie an einem Morgen auf 20 m.
        auto stand = [](int belegt, int gesamt, int wartende = 0) {
            static const struct {
                const char* ruf; const char* land; int ton;
                const char* zug; int stufe; int gesendet; int empfangen;
            } quellen[] = {
                { "R6FMM",  "European Russia",      2710, "Tx3",  3,  -5,  -11 },
                { "UN6LN",  "Kazakhstan",           2770, "Tx2",  2, -14,  -99 },
                { "JS6TWW", "Japan",                2830, "Tx1",  1, -99,  -99 },
                { "HB9ZHK", "Switzerland",          2890, "RR73", 4,   3,   -2 },
                { "DL1ABC", "Fed. Rep. of Germany", 2950, "Tx2",  2,  -8,  -99 },
                { "9A6T",   "Croatia",              3010, "Tx1",  1, -99,  -99 } };
            Ft8PlaetzeStand s;
            s.plaetzeGesamt = gesamt;
            s.sendet = belegt > 0;
            for (int i = 0; i < belegt; ++i) {
                Ft8PlatzStand z;
                z.partner   = QString::fromLatin1(quellen[i].ruf);
                z.land      = QString::fromLatin1(quellen[i].land);
                z.tonHz     = quellen[i].ton;
                z.zug       = QString::fromLatin1(quellen[i].zug);
                z.stufe     = quellen[i].stufe;
                z.gesendet  = quellen[i].gesendet;
                z.empfangen = quellen[i].empfangen;
                s.plaetze.append(z);
            }
            static const struct { const char* ruf; const char* land; int snr; }
                schlange[] = { { "PD4RJ", "Netherlands", -9 },
                               { "F5PZT", "France",      4 },
                               { "OE3TVJ", "Austria",   -17 } };
            for (int i = 0; i < wartende; ++i) {
                s.wartende.append(Ft8WartenderStand{
                    QString::fromLatin1(schlange[i].ruf),
                    QString::fromLatin1(schlange[i].land), schlange[i].snr});
            }
            return s;
        };

        struct Zustand {
            const char*     name;
            int             breite;
            int             hoehe;
            Ft8PlaetzeStand stand;
        };
        const QVector<Zustand> zustaende{
            { "a-seine-groesse-2-von-3",          365, 340, stand(2, 3) },
            { "b-breit-niedrig-3-von-3",          760, 180, stand(3, 3) },
            { "c-schmal-hoch-3-von-3",            240, 520, stand(3, 3) },
            { "d-sechs-plaetze-4-belegt",         560, 330, stand(4, 6) },
            { "e-leer",                           365, 340, Ft8PlaetzeStand{} },
            { "f-leer-schmal",                    200, 340, Ft8PlaetzeStand{} },
            { "g-seine-groesse-3-von-3",          365, 340, stand(3, 3) },
            { "h-etwas-breiter-3-von-3",          460, 300, stand(3, 3) },
            { "i-breit-niedrig-2-von-3",          760, 180, stand(2, 3) },
            { "j-seine-groesse-2-von-6-dicht",    365, 340, stand(2, 6) },
            { "k-breit-mit-wartenden",            760, 260, stand(2, 3, 3) },
            // Sein Bild vom 2026-11-18: rund 1040 x 340, „1 of 4", eine
            // Kachel ueber die ganze Breite, drei „free" darunter.
            { "l-sein-bild-1-von-4",             1040, 340, stand(1, 4) },
            { "m-sein-bild-3-von-4",             1040, 340, stand(3, 4) },
            // Zu klein fuer drei untereinander: ein Rollbalken, keine
            // abgeschnittene Kachel.
            { "n-zu-klein-3-von-3-rollt",         365, 280, stand(3, 3) },
        };

        for (const Zustand& z : zustaende) {
            RadioModel radio;
            auto* panel = new Ft8PlaetzeApplet(&radio, nullptr);
            panel->zeigeStand(z.stand);
            AppletFloatingWindow fenster(panel, QStringLiteral("Ft8Slots"),
                                         0, nullptr);
            fenster.resize(z.breite, z.hoehe);
            fenster.show();
            QVERIFY(QTest::qWaitForWindowExposed(&fenster, kFensterFrist));
            // Eine feste, kleine Zahl von Runden: Mindesthoehe ->
            // Rollbereich -> Rollbalken -> Breite -> Mindesthoehe.
            for (int runde = 0; runde < 12; ++runde) {
                QCoreApplication::sendPostedEvents();
                QCoreApplication::processEvents();
            }
            QCOMPARE(fenster.size(), QSize(z.breite, z.hoehe));

            QImage bild = fenster.grab().toImage();
            // 72 dpi je Punkt mal Geraetefaktor: mit QT_SCALE_FACTOR=2
            // sind das 144 dpi, und das Bild oeffnet sich in
            // wirklicher Groesse.
            const int dpm = int(72.0 * bild.devicePixelRatio() / 0.0254);
            bild.setDotsPerMeterX(dpm);
            bild.setDotsPerMeterY(dpm);
            const QString pfad = ziel + QStringLiteral("/ft8-slots-%1-%2x%3.png")
                .arg(QLatin1String(z.name)).arg(z.breite).arg(z.hoehe);
            QVERIFY2(bild.save(pfad), qPrintable(pfad));

            const Ft8PlaetzeAnordnung a = panel->flaecheForTest()->anordnung();
            qInfo().noquote()
                << "Blatt:" << pfad
                << "| Spalten" << a.spalten
                << "| Kacheln" << a.kacheln.size()
                << "| freie" << a.freie.size()
                << "| Kachel mind." << panel->flaecheForTest()->kachelMindestbreite()
                << "| Flaeche" << panel->flaecheForTest()->size();
        }
    }
};

QTEST_MAIN(TstFt8PlaetzeEntwuerfe)
#include "tst_ft8_plaetze_entwuerfe.moc"
