// no-port-check: Longpath-original test, no Thetis logic.
//
// ── Farbschemata des Panadapters (2026-11-21) ────────────────────────────
//
// Betreiber zu den Blaettern werkzeug/entwuerfe-pan-ft8-look-2026-11-21:
// „s2,s4,s5 - alles als option!", vorher „vor allem in der bandwidth",
// dann „na dann nehme dazu" fuer S1/S3. Fuenf Schemata (FT8 Bernstein,
// Messing gedaempft, Tuerkis-Gruen, Graublau, Nacht) neben Standard, im
// Zahnrad des Panadapters. Ein Schema bestimmt Durchlass und Wasserfall;
// die Spur bleibt Messwert, nur Nacht dimmt sie.
//
// Dieser Pruefstand nagelt fest:
//   1. Vorgabe Standard, die Wahl ueberlebt den Neustart;
//   2. der Wasserfall nimmt die Palette des Schemas, die eigene Palette
//      bleibt gespeichert und gilt bei Standard wieder;
//   3. Graublau und Nacht haben ihre Farben, Nacht wird nie hell;
//   4. der Durchlass: Standard flach in der eigenen Farbe, ein Schema als
//      Verlauf (unten kraeftiger als oben) in seiner Farbe;
//   5. die Spur: nur Nacht dimmt;
//   6. die sechs Eintraege stehen im Zahnrad und wirken.
//
// Gegenprobe gefahren (2026-11-21): effectiveWfColorScheme() und
// fuelleDurchlass() per Patch auf das alte Verhalten zurueckgesetzt ->
// die Faelle 2, 4 fallen; Patch zurueckgenommen -> gruen.

#include <QtTest/QtTest>
#include <QApplication>
#include <QImage>
#include <QMenu>
#include <QPainter>

#include "core/AppSettings.h"
#include "gui/PanadapterApplet.h"
#include "gui/SpectrumWidget.h"

using namespace Longpath;

namespace {

QImage durchlassMalen(const SpectrumWidget& w)
{
    QImage img(100, 200, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    w.paintPassbandFillForTest(p, QRectF(0, 0, 100, 200));
    return img;
}

int helligkeit(QRgb c) { return qRed(c) + qGreen(c) + qBlue(c); }

}  // namespace

class TestPanadapterFarbschema : public QObject {
    Q_OBJECT
private slots:
    void init() { AppSettings::instance().clear(); }

    // 1 ──────────────────────────────────────────────────────────────────
    void vorgabeStandardUndWahlBleibt()
    {
        {
            SpectrumWidget w;
            w.loadSettings();
            QCOMPARE(w.colourScheme(), PanColourScheme::Standard);
            w.setColourScheme(PanColourScheme::Night);
            w.saveSettings();
        }
        SpectrumWidget w2;
        w2.loadSettings();
        QCOMPARE(w2.colourScheme(), PanColourScheme::Night);
    }

    // 2 ──────────────────────────────────────────────────────────────────
    void wasserfallNimmtDieSchemaPalette()
    {
        SpectrumWidget w;
        w.setWfColorScheme(WfColorScheme::Default);   // Martins Regenbogen
        QCOMPARE(w.effectiveWfColorScheme(), WfColorScheme::Default);

        w.setColourScheme(PanColourScheme::Brass);
        QCOMPARE(w.effectiveWfColorScheme(), WfColorScheme::Muted);
        w.setColourScheme(PanColourScheme::SlateBlue);
        QCOMPARE(w.effectiveWfColorScheme(), WfColorScheme::SlateBlue);
        w.setColourScheme(PanColourScheme::Night);
        QCOMPARE(w.effectiveWfColorScheme(), WfColorScheme::Night);
        w.setColourScheme(PanColourScheme::Ft8Amber);
        QCOMPARE(w.effectiveWfColorScheme(), WfColorScheme::Ft8Amber);
        w.setColourScheme(PanColourScheme::Teal);
        QCOMPARE(w.effectiveWfColorScheme(), WfColorScheme::Teal);

        // Die eigene Palette wurde nicht ueberschrieben ...
        QCOMPARE(w.wfColorScheme(), WfColorScheme::Default);
        // ... und gilt bei Standard wieder.
        w.setColourScheme(PanColourScheme::Standard);
        QCOMPARE(w.effectiveWfColorScheme(), WfColorScheme::Default);
    }

    // 3 ──────────────────────────────────────────────────────────────────
    void graublauUndNachtHabenIhreFarben()
    {
        const QColor tief;   // nur fuer Enhanced belangt
        // Das Rauschen verschwindet im Schwarz.
        QVERIFY(helligkeit(SpectrumWidget::waterfallColorForIntensityF(
                    0.2f, WfColorScheme::SlateBlue, tief)) < 30);
        QVERIFY(helligkeit(SpectrumWidget::waterfallColorForIntensityF(
                    0.2f, WfColorScheme::Night, tief)) < 30);
        // Oben: Graublau hellgrau, Nacht dunkles Bernstein.
        QCOMPARE(SpectrumWidget::waterfallColorForIntensityF(
                     1.0f, WfColorScheme::SlateBlue, tief),
                 qRgb(0xdd, 0xe4, 0xec));
        const QRgb nachtOben = SpectrumWidget::waterfallColorForIntensityF(
            1.0f, WfColorScheme::Night, tief);
        QCOMPARE(nachtOben, qRgb(0x7a, 0x55, 0x30));
        QVERIFY2(qRed(nachtOben) < 0x80 && qGreen(nachtOben) < 0x80,
                 "Nacht wird nie heller als dunkles Bernstein");
        // FT8 Bernstein endet wie der Streifen im FT8-Fenster (255,170,70),
        // Tuerkis-Gruen in Mint.
        QCOMPARE(SpectrumWidget::waterfallColorForIntensityF(
                     1.0f, WfColorScheme::Ft8Amber, tief),
                 qRgb(0xff, 0xaa, 0x46));
        QCOMPARE(SpectrumWidget::waterfallColorForIntensityF(
                     1.0f, WfColorScheme::Teal, tief),
                 qRgb(0xe0, 0xf4, 0xec));
        QVERIFY(helligkeit(SpectrumWidget::waterfallColorForIntensityF(
                    0.2f, WfColorScheme::Ft8Amber, tief)) < 30);
    }

    // 4 ──────────────────────────────────────────────────────────────────
    void durchlassStandardFlachSchemaAlsVerlauf()
    {
        SpectrumWidget w;
        const QImage flach = durchlassMalen(w);
        QCOMPARE(flach.pixel(50, 10), flach.pixel(50, 190));
        QVERIFY(qAlpha(flach.pixel(50, 100)) > 0);

        w.setColourScheme(PanColourScheme::SlateBlue);
        const QImage verlauf = durchlassMalen(w);
        const QRgb oben = verlauf.pixel(50, 5);
        const QRgb unten = verlauf.pixel(50, 195);
        QVERIFY2(qAlpha(unten) > qAlpha(oben) + 20,
                 "unten kraeftiger als oben");
        // Schieferblau: Blau vor Rot.
        QVERIFY(qBlue(unten) > qRed(unten));
        // Und nicht die Farbe von heute (Tuerkis #00b4d8).
        QVERIFY(verlauf.pixel(50, 100) != flach.pixel(50, 100));

        w.setColourScheme(PanColourScheme::Brass);
        const QRgb messing = durchlassMalen(w).pixel(50, 195);
        QVERIFY2(qRed(messing) > qBlue(messing), "Messing ist warm");

        w.setColourScheme(PanColourScheme::Ft8Amber);
        const QRgb ft8 = durchlassMalen(w).pixel(50, 195);
        QVERIFY2(qRed(ft8) > qGreen(ft8) && qGreen(ft8) > qBlue(ft8),
                 "FT8 Bernstein ist orange");
        QVERIFY2(qAlpha(ft8) > qAlpha(messing), "FT8 kraeftiger als Messing");
        w.setColourScheme(PanColourScheme::Teal);
        const QRgb teal = durchlassMalen(w).pixel(50, 195);
        QVERIFY2(qGreen(teal) > qRed(teal), "Tuerkis-Gruen ist gruen");
    }

    // 5 ──────────────────────────────────────────────────────────────────
    void nurNachtDimmtDieSpur()
    {
        SpectrumWidget w;
        const QColor heute = w.traceColourForPaint();
        w.setColourScheme(PanColourScheme::Brass);
        QCOMPARE(w.traceColourForPaint(), heute);
        w.setColourScheme(PanColourScheme::SlateBlue);
        QCOMPARE(w.traceColourForPaint(), heute);
        w.setColourScheme(PanColourScheme::Night);
        const QColor nacht = w.traceColourForPaint();
        QVERIFY(nacht.valueF() < heute.valueF() * 0.7);
        QCOMPARE(nacht.hue(), heute.hue());   // gedimmt, nicht umgefaerbt
    }

    // 6 ──────────────────────────────────────────────────────────────────
    void dieSchemataStehenImZahnrad()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        SpectrumWidget* sw = applet.spectrumWidget();
        QVERIFY(sw);
        QMenu* m = applet.buildDisplayMenuForTesting();
        QVERIFY(m);
        QMenu* schemata = nullptr;
        for (QAction* a : m->actions()) {
            if (a->menu() && a->text() == QStringLiteral("Colour scheme")) {
                schemata = a->menu();
            }
        }
        QVERIFY2(schemata, "Untermenue „Colour scheme“ im Zahnrad");
        QStringList namen;
        QAction* nacht = nullptr;
        for (QAction* a : schemata->actions()) {
            namen << a->text();
            if (a->text() == QStringLiteral("Night")) { nacht = a; }
        }
        QCOMPARE(namen, (QStringList{QStringLiteral("Standard"),
                                     QStringLiteral("FT8 amber"),
                                     QStringLiteral("Brass (muted)"),
                                     QStringLiteral("Teal green"),
                                     QStringLiteral("Slate blue"),
                                     QStringLiteral("Night")}));
        QVERIFY(schemata->actions().first()->isChecked());   // Standard ab Werk
        QVERIFY(nacht);
        nacht->trigger();
        QCOMPARE(sw->colourScheme(), PanColourScheme::Night);
        delete m;
    }
};

QTEST_MAIN(TestPanadapterFarbschema)
#include "tst_panadapter_farbschema.moc"
