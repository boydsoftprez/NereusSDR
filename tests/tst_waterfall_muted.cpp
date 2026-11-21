// tests/tst_waterfall_muted.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
//
// ── Die groesste Farbflaeche im Programm ─────────────────────────────
//
// Nach dem Entblauen der Palette war der Wasserfall mit Abstand das
// Lauteste im Fenster: ein Regenbogen ueber vierzig Prozent der
// Flaeche, mit einem durchgehenden roten Band dort, wo das
// Grundrauschen oben an die Rampe stoesst. Am Bildschirm gesehen,
// 2026-08-15.
//
// Der Theme-Filter erreicht ihn nicht — er wird gemalt, nicht gestylt.
// Das Schema "Gedaempft" holt seine Farben deshalb selbst aus den
// Rollen.
//
// Zwei Dinge, die hier festgenagelt werden:
//
//   · Die Rampe beginnt im Hintergrund. Das ist der ganze Trick: das
//     Grundrauschen verschwindet, statt eingefaerbt zu werden.
//
//   · Sie folgt dem Theme. Ein Palettenwechsel, der die groesste
//     Farbflaeche des Programms stehen laesst, waere die auffaelligste
//     Luecke, die man bauen kann.
//
// ── Warum diese Datei so schlicht aussieht ───────────────────────────
//
// Die erste Fassung liess sich nicht binden: "Undefined symbols —
// vtable for TestWaterfallMuted". Ursache war eine LEERE .moc-Datei.
// AUTOMOC hatte Q_OBJECT gefunden und den Dateinamen richtig gesetzt
// (ParseCache.txt: "mmc:Q_OBJECT"), moc selbst gab aber nichts aus —
// eine von 661 .moc-Dateien im Testbaum, genau diese.
//
// mocs Parser ist kein vollstaendiger C++-Parser. Statt zu raten,
// welches Konstrukt ihn aus dem Tritt bringt, sind hier alle
// Verdaechtigen auf einmal weg: Helfer stehen als freie Funktionen VOR
// der Klasse, keine Roh-Zeichenketten, keine Initialisiererlisten in
// Schleifen. Die Klasse enthaelt nur noch Slots.
//
// Das ist ohnehin die bessere Form fuer eine Testdatei.

#include <QtTest>

#include "gui/SpectrumWidget.h"
#include "gui/StyleConstants.h"
#include "gui/styles/Theme.h"

#include <QColor>
#include <QFile>
#include <QIODevice>
#include <QString>
#include <QTemporaryDir>

using namespace Longpath;

namespace {

QColor colourAt(const WfGradientStop& s)
{
    return QColor(s.r, s.g, s.b);
}

/// Eine Theme-Datei mit genau diesen Farbzuweisungen. `body` ist der
/// Inhalt des colors-Objekts, ohne die aeusseren Klammern.
bool writeTheme(const QString& path, const QString& body)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { return false; }
    const QString doc = QStringLiteral("{ \"colors\": { ") + body
                      + QStringLiteral(" } }");
    f.write(doc.toUtf8());
    f.close();
    return true;
}

QString assign(const QString& role, const QString& hex)
{
    return QStringLiteral("\"") + role + QStringLiteral("\": \"")
         + hex + QStringLiteral("\"");
}

} // namespace

class TestWaterfallMuted : public QObject
{
    Q_OBJECT

private slots:
    void init()    { Style::Theme::instance().clear(); }
    void cleanup() { Style::Theme::instance().clear(); }

    // ── Der Grund, warum das Schema existiert ────────────────────────
    void theRampStartsAtTheBackgroundSoTheNoiseFloorDisappears()
    {
        int n = 0;
        const WfGradientStop* s = wfSchemeStops(WfColorScheme::Muted, n);
        QVERIFY(n >= 2);
        QCOMPARE(s[0].pos, 0.0f);
        QCOMPARE(colourAt(s[0]), QColor(Style::kAppBg));
    }

    // ── Oben steht „laut", nicht „Achtung" ───────────────────────────
    //
    // Dieser Test hiess theTopIsTheDangerColour und nagelte fest, dass
    // das obere Ende die Gefahrenfarbe traegt. Am Bildschirm gesehen
    // (2026-08-15, 20 m) war das Ergebnis: jeder kraeftige Traeger
    // bekam einen rosa Streifen. Ein starkes Signal ist aber keine
    // Gefahr.
    //
    // Es ist derselbe Fehler, der beim S-Meter schon ausgebaut wurde:
    // dort war der halbe Bogen rot und kodierte damit nichts. Rot heisst
    // in diesem Programm „Achtung". Wenn es zusaetzlich „oberes
    // Skalenende" heisst, faellt es dort nicht mehr auf, wo es zaehlt —
    // an der Bandkante und beim Stehwellenverhaeltnis.
    //
    // Der Test prueft jetzt das Gegenteil: die Gefahrenfarbe kommt in
    // der Rampe NICHT vor.
    void theTopIsBrightNotDangerous()
    {
        int n = 0;
        const WfGradientStop* s = wfSchemeStops(WfColorScheme::Muted, n);
        QCOMPARE(s[n - 1].pos, 1.0f);
        QCOMPARE(colourAt(s[n - 1]), QColor(Style::kInstrumentFace));

        const QColor danger(Style::kRedBorder);
        for (int i = 0; i < n; ++i) {
            QVERIFY2(colourAt(s[i]) != danger,
                     "die Gefahrenfarbe steht wieder in der Wasserfallrampe");
        }
    }

    // ── Wo das Schwarz aufhoert ──────────────────────────────────────
    //
    // Zwei Fassungen daneben, beide am Bildschirm gesehen:
    //
    //   0,16 — der Rauschteppich landete im hellen Grau, das ganze
    //          Fenster wirkte neblig.
    //   0,45 — Traeger, die im Spektrum darueber deutlich herausragten,
    //          verschwanden im Wasserfall.
    //
    // Die Regel dahinter, und der Grund fuer diesen Test: das Rauschen
    // soll verschwinden, das schwaechste ERKENNBARE Signal soll gerade
    // sichtbar werden. Das Fenster dafuer ist schmal, und ohne
    // Untergrenze rutscht die naechste Aenderung wieder in den Nebel.
    void theDarkEndCoversRoughlyTheLowerThird()
    {
        int n = 0;
        const WfGradientStop* s = wfSchemeStops(WfColorScheme::Muted, n);
        QVERIFY(n >= 3);
        QVERIFY2(s[1].pos >= 0.22f,
                 qPrintable(QStringLiteral("das Schwarz endet schon bei %1 — "
                                           "der Rauschteppich wird eingefaerbt")
                                .arg(s[1].pos)));
        QVERIFY2(s[1].pos <= 0.38f,
                 qPrintable(QStringLiteral("das Schwarz reicht bis %1 — "
                                           "schwache Signale verschwinden")
                                .arg(s[1].pos)));
    }

    void theStopsRiseAndStayInRange()
    {
        int n = 0;
        const WfGradientStop* s = wfSchemeStops(WfColorScheme::Muted, n);
        for (int i = 0; i < n; ++i) {
            QVERIFY(s[i].pos >= 0.0f && s[i].pos <= 1.0f);
            QVERIFY(s[i].r >= 0 && s[i].r <= 255);
            QVERIFY(s[i].g >= 0 && s[i].g <= 255);
            QVERIFY(s[i].b >= 0 && s[i].b <= 255);
            if (i > 0) {
                QVERIFY2(s[i].pos > s[i - 1].pos,
                         "die Stops laufen nicht aufsteigend");
            }
        }
    }

    // ── Und der eigentliche Punkt ────────────────────────────────────
    void theRampFollowsTheThemeFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        int n = 0;
        const QColor before = colourAt(wfSchemeStops(WfColorScheme::Muted, n)[0]);

        const QString path = dir.filePath(QStringLiteral("w.json"));
        QVERIFY(writeTheme(path,
                           assign(QStringLiteral("app-bg"),
                                  QStringLiteral("#123456"))
                           + QStringLiteral(", ")
                           + assign(QStringLiteral("instrument-face"),
                                    QStringLiteral("#654321"))));
        QString err;
        QVERIFY2(Style::Theme::instance().loadFile(path, &err), qPrintable(err));

        const WfGradientStop* s = wfSchemeStops(WfColorScheme::Muted, n);
        QCOMPARE(colourAt(s[0]),     QColor(QStringLiteral("#123456")));
        QCOMPARE(colourAt(s[n - 1]), QColor(QStringLiteral("#654321")));
        QVERIFY(colourAt(s[0]) != before);
    }

    void theCacheDoesNotOutliveTheTheme()
    {
        // Die Stops werden zwischengespeichert, weil sie beim Zeichnen
        // jeder Zeile gebraucht werden. Wenn der Cache die
        // Theme-Generation nicht beachtet, aendert die erste geladene
        // Datei den Wasserfall — und keine danach.
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString a = dir.filePath(QStringLiteral("a.json"));
        const QString b = dir.filePath(QStringLiteral("b.json"));
        QVERIFY(writeTheme(a, assign(QStringLiteral("app-bg"),
                                     QStringLiteral("#111111"))));
        QVERIFY(writeTheme(b, assign(QStringLiteral("app-bg"),
                                     QStringLiteral("#222222"))));

        int n = 0;
        QVERIFY(Style::Theme::instance().loadFile(a));
        QCOMPARE(colourAt(wfSchemeStops(WfColorScheme::Muted, n)[0]),
                 QColor(QStringLiteral("#111111")));
        QVERIFY(Style::Theme::instance().loadFile(b));
        QCOMPARE(colourAt(wfSchemeStops(WfColorScheme::Muted, n)[0]),
                 QColor(QStringLiteral("#222222")));
    }

    // ── Der gespeicherte Wert ist ein Index ──────────────────────────
    //
    // WfColorScheme wird als int in die Einstellungen geschrieben, und
    // die Auswahlliste in DisplaySetupPages ist nach demselben Index
    // aufgebaut. Ein neues Schema in der MITTE des enum verschiebt bei
    // jedem Anwender still das eingestellte Schema — aus "Clarity Blue"
    // wird "Custom", ohne dass jemand etwas angefasst haette.
    void theSchemeOrderIsFrozen()
    {
        QCOMPARE(static_cast<int>(WfColorScheme::Default),     0);
        QCOMPARE(static_cast<int>(WfColorScheme::Enhanced),    1);
        QCOMPARE(static_cast<int>(WfColorScheme::Spectran),    2);
        QCOMPARE(static_cast<int>(WfColorScheme::BlackWhite),  3);
        QCOMPARE(static_cast<int>(WfColorScheme::LinLog),      4);
        QCOMPARE(static_cast<int>(WfColorScheme::LinRad),      5);
        QCOMPARE(static_cast<int>(WfColorScheme::Custom),      6);
        QCOMPARE(static_cast<int>(WfColorScheme::ClarityBlue), 7);
        QCOMPARE(static_cast<int>(WfColorScheme::Muted),       8);
        // 2026-11-21: Farbschemata des Panadapters, angehaengt.
        QCOMPARE(static_cast<int>(WfColorScheme::SlateBlue),   9);
        QCOMPARE(static_cast<int>(WfColorScheme::Night),      10);
        QCOMPARE(static_cast<int>(WfColorScheme::Ft8Amber),   11);
        QCOMPARE(static_cast<int>(WfColorScheme::Teal),       12);
        QCOMPARE(static_cast<int>(WfColorScheme::Count),      13);
    }

    void theOtherSchemesAreUntouched()
    {
        // "Gedaempft" ist ein Angebot, keine Enteignung. Wer den
        // Regenbogen will, bekommt ihn unveraendert.
        int n = 0;
        const WfGradientStop* d = wfSchemeStops(WfColorScheme::Default, n);
        QCOMPARE(n, 7);
        QCOMPARE(colourAt(d[0]), QColor(0, 0, 0));
        QCOMPARE(colourAt(d[n - 1]), QColor(255, 0, 0));
    }
};

QTEST_MAIN(TestWaterfallMuted)
#include "tst_waterfall_muted.moc"
