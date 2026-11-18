// =================================================================
// tests/tst_cal_offset_bestellt_kein_speichern.cpp  (Longpath)
// =================================================================
//
// Longpath-original test.
//
// Betreiber, 2026-11-18: „das beenden dauert immer sehr lange und macht
// mich nachdenklich warum." Bei der Suche fiel ein Bericht von macOS
// selbst auf (Longpath_2026-11-18-125207…diag, „disk writes"): 2147 MB
// in 4521 Sekunden, der schwerste Stapel ein Einmal-Taktgeber, der die
// Einstellungsdatei schreibt.
//
// Danach am Geraet gemessen, zehn Minuten FT8 an der Flex, jede
// Neuschreibung von Longpath.settings mit den geaenderten Schluesseln:
//
//   29 Neuschreibungen der ganzen Datei (2,9 MB, 13 740 Eintraege)
//   26 davon mit genau EINER Aenderung: DisplayCalOffset
//   Zeiten: 13:15:01, :06, :15, :21, :30, :36, :46, :51 … -- zweimal je
//   15-Sekunden-Durchgang, beim Tasten und beim Loesen.
//
// Der Weg: Tasten/Loesen stellt den Messversatz um ->
// RadioModel::rxMeterOffsetChanged -> MainWindow schiebt ihn in
// SpectrumWidget::setDbmCalOffset() -> und der bestellte jedes Mal
// scheduleSettingsSave() -> AppSettings::save() ueber die ganze Datei.
//
// Der Versatz vom Geraet ist gerechnet und kommt bei jedem Start neu.
// Gespeichert wird nur, was der Bediener selbst einstellt.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-18 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>

#include <QFile>
#include <QStandardPaths>

#include "core/AppSettings.h"
#include "gui/SpectrumWidget.h"

using namespace Longpath;

class TstCalOffsetBestelltKeinSpeichern : public QObject {
    Q_OBJECT

private:
    static QString gespeichert()
    {
        return AppSettings::instance()
            .value(QStringLiteral("DisplayCalOffset"), QStringLiteral("(leer)"))
            .toString();
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    // Der Wert vom Geraet: er gilt sofort, aber niemand bestellt das
    // Speichern. Nach der Frist des Einmal-Taktgebers (500 ms) steht in
    // den Einstellungen noch, was vorher dort stand.
    void derWertVomGeraetWirdNichtGespeichert()
    {
        SpectrumWidget w;
        AppSettings::instance().setValue(QStringLiteral("DisplayCalOffset"),
                                         QStringLiteral("0"));
        const QString vorher = gespeichert();

        w.setDbmCalOffset(-11.5f, /*merken=*/false);
        QCOMPARE(w.dbmCalOffset(), -11.5f);        // er wirkt
        QTest::qWait(900);
        QVERIFY2(gespeichert() == vorher,
                 qPrintable(QStringLiteral(
                     "der Versatz vom Geraet wurde gespeichert: vorher %1, "
                     "jetzt %2 -- das ist die Neuschreibung je Sendedurchgang")
                     .arg(vorher, gespeichert())));

        // Und noch einmal, wie beim Loesen: wieder ein anderer Wert,
        // wieder kein Speichern.
        w.setDbmCalOffset(-3.0f, /*merken=*/false);
        QTest::qWait(900);
        QCOMPARE(gespeichert(), vorher);
    }

    // Die Gegenseite, damit der Fall oben nicht bloss gruen ist, weil
    // gar nichts mehr gespeichert wird: was der Bediener einstellt
    // (Setup > Display, der Aufruf ohne zweites Argument), steht nach
    // der Frist in den Einstellungen.
    void wasDerBedienerEinstelltWirdGespeichert()
    {
        SpectrumWidget w;
        AppSettings::instance().setValue(QStringLiteral("DisplayCalOffset"),
                                         QStringLiteral("0"));
        w.setDbmCalOffset(4.5f);
        QTest::qWait(900);
        bool ok = false;
        const float wert = gespeichert().toFloat(&ok);
        QVERIFY2(ok && qFuzzyCompare(wert, 4.5f),
                 qPrintable(QStringLiteral("erwartet 4.5, gespeichert: %1")
                                .arg(gespeichert())));
    }

    // Die Verdrahtung: der eine Aufrufer, der den Wert vom Geraet
    // durchreicht, sagt „nicht merken". Am Quelltext geprueft -- der
    // echte Weg braucht ein Hauptfenster mit verbundenem Geraet.
    void dasHauptfensterReichtIhnOhneMerkenDurch()
    {
        // ctest setzt LONGPATH_SOURCE_DIR; ohne sie (Binaerdatei von Hand
        // gestartet) waere der Fall still gruen -- darum laut.
        const QString wurzel = qEnvironmentVariable("LONGPATH_SOURCE_DIR");
        QVERIFY2(!wurzel.isEmpty(),
                 "LONGPATH_SOURCE_DIR fehlt -- ueber ctest starten");
        QFile f(wurzel + QStringLiteral("/src/gui/MainWindow.cpp"));
        QVERIFY2(f.open(QIODevice::ReadOnly), "MainWindow.cpp nicht lesbar");
        const QString quelle = QString::fromUtf8(f.readAll());
        const int wo = quelle.indexOf(QStringLiteral("auto pushSpectrumCal"));
        QVERIFY2(wo >= 0, "pushSpectrumCal nicht gefunden -- Anker anpassen");
        const QString stueck = quelle.mid(wo, 500);
        QVERIFY2(stueck.contains(QStringLiteral("setDbmCalOffset(")),
                 "pushSpectrumCal ruft setDbmCalOffset nicht mehr");
        QVERIFY2(stueck.contains(QStringLiteral("/*merken=*/false")),
                 "der Versatz vom Geraet wird wieder MIT Speichern "
                 "durchgereicht -- das schreibt die Einstellungsdatei bei "
                 "jedem Sendedurchgang zweimal neu");
    }
};

QTEST_MAIN(TstCalOffsetBestelltKeinSpeichern)
#include "tst_cal_offset_bestellt_kein_speichern.moc"
