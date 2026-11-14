// SPDX-License-Identifier: GPL-3.0-or-later
//
// Das Frequenz-Widget stimmt wirklich ab — mit dem Rad auf einer
// Stelle und mit der Tastatur.
//
// Der Betreiber am 2026-08-22: "für frequenz bitte auch ein eigenes
// widget bauen". Es GIBT eines (FrequencyApplet mit
// FrequencyInstrument), samt Ziffernrad und Eingabefeld — wie das
// grosse Frequenzfeld in der Vorlage ("CLICK TO TYPE · WHEEL ON A
// DIGIT").
//
// Heute war allerdings mehrfach etwas verdrahtet und trotzdem tot:
// die Zoomknoepfe sandten ins Leere, die Pfeiltasten kamen nie an,
// der Panadapter erfuhr nie von Frequenzaenderungen. Bevor ich dem
// Betreiber sage "das gibt es schon", wird es gemessen.

#include <QtTest>
#include <QLabel>
#include <QWheelEvent>

#include "gui/applets/FrequencyApplet.h"
#include "gui/instruments/FrequencyInstrument.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "FensterFrist.h"

using namespace Longpath;

class TstFrequencyWidgetTunes : public QObject
{
    Q_OBJECT

private slots:
    void theWheelOnADigitTunesThatDigit()
    {
        RadioModel model;
        if (model.slices().isEmpty()) { model.addSlice(); }
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }
        SliceModel& slice = *model.slices().first();
        slice.setFrequency(7'100'000.0);

        FrequencyApplet applet(&model);
        applet.bindSlice(&slice);
        applet.resize(360, 90);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, kFensterFrist));
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        auto* inst = applet.findChild<FrequencyInstrument*>();
        QVERIFY2(inst, "Kein Frequenz-Instrument im Applet");

        // Die Ziffernschilder liegen als QLabel im Instrument. Gesucht
        // wird das, auf dem eine Ziffer steht — das Rad darauf soll
        // GENAU dessen Stelle bewegen.
        QList<QLabel*> digits;
        for (QLabel* l : inst->findChildren<QLabel*>()) {
            const QString t = l->text();
            if (t.size() == 1 && t.at(0).isDigit()) { digits.append(l); }
        }
        QVERIFY2(!digits.isEmpty(), "Keine Ziffern gefunden");

        const double before = slice.frequency();
        QLabel* last = digits.last();          // kleinste Stelle
        QWheelEvent up(QPointF(last->width() / 2.0, last->height() / 2.0),
                       last->mapToGlobal(QPoint(1, 1)),
                       QPoint(0, 0), QPoint(0, 120),
                       Qt::NoButton, Qt::NoModifier,
                       Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(last, &up);
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        QVERIFY2(!qFuzzyCompare(slice.frequency(), before),
                 qPrintable(QStringLiteral(
                     "Das Rad auf der Ziffer bewegt nichts: %1 -> %2")
                     .arg(before).arg(slice.frequency())));
        // Genau EINE Stelle, nicht irgendetwas: der Sprung muss klein
        // sein.
        const double step = qAbs(slice.frequency() - before);
        QVERIFY2(step <= 1000.0,
                 qPrintable(QStringLiteral(
                     "Ein Radschritt auf der kleinsten Stelle springt "
                     "um %1 Hz").arg(step)));
    }

    void theWidgetFollowsTheModel()
    {
        // Die andere Richtung, und heute die haeufigere Fehlerquelle:
        // aendert sich die Frequenz anderswo (Klick im Panadapter,
        // Pfeiltaste, CAT), muss das Widget mitgehen.
        RadioModel model;
        if (model.slices().isEmpty()) { model.addSlice(); }
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }
        SliceModel& slice = *model.slices().first();
        slice.setFrequency(7'100'000.0);

        FrequencyApplet applet(&model);
        applet.bindSlice(&slice);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, kFensterFrist));
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        auto* inst = applet.findChild<FrequencyInstrument*>();
        QVERIFY(inst);

        slice.setFrequency(14'225'000.0);
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }

        QString shown;
        for (QLabel* l : inst->findChildren<QLabel*>()) {
            const QString t = l->text();
            if (t.size() == 1 && (t.at(0).isDigit() || t == QStringLiteral("."))) {
                shown += t;
            }
        }
        QVERIFY2(shown.contains(QStringLiteral("14")),
                 qPrintable(QStringLiteral(
                     "Das Widget zeigt %1, das Modell steht auf "
                     "14.225.000").arg(shown)));
    }

    // ── Der Versatz muss den WEG bis zur Zeile gehen (2026-11-14) ──
    //
    // Die Zeile selbst ist in tst_frequency_instrument genagelt, die
    // Quellenwahl ebenso. Was dort NICHT gefahren wird, ist das
    // Stueck dazwischen: `FrequencyApplet::versatzNachziehen()` und
    // die vier Signale, an denen es haengt. Genau dort war die Luecke
    // -- gestellt werden RIT/XIT seit S2.8 im RX-Applet, und
    // SliceModel::effectiveRxFrequency rechnet sie still mit, aber
    // bei der Frequenz stand nichts davon.
    //
    // Die zweite Quelle (das fremde Geraet) laesst sich hier nicht
    // fahren -- dafuer braeuchte es einen laufenden CAT-Weg. Sie ist
    // ueber `ritXitFuerDieZeile()` abgedeckt, mit fuenf Faellen.
    void einVersatzAnDerScheibeErreichtDieVfoZeile()
    {
        RadioModel model;
        if (model.slices().isEmpty()) { model.addSlice(); }
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }
        SliceModel& slice = *model.slices().first();
        slice.setFrequency(7'164'781.0);

        FrequencyApplet applet(&model);
        applet.bindSlice(&slice);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet, kFensterFrist));
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        auto* inst = applet.findChild<FrequencyInstrument*>();
        QVERIFY(inst);
        QVERIFY2(!inst->vfoRowText().contains(QStringLiteral("RIT")),
                 "ausgeschaltet darf nichts dastehen");

        // Erst der Wert, dann der Schalter -- ein Wert allein aendert
        // nichts, genau wie im Empfang.
        slice.setRitHz(-1200);
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        QVERIFY2(!inst->vfoRowText().contains(QStringLiteral("RIT")),
                 "ein Wert ohne Schalter darf nicht erscheinen");

        slice.setRitEnabled(true);
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        QVERIFY2(inst->vfoRowText().contains(QStringLiteral("RIT \u22121200")),
                 qPrintable(QStringLiteral(
                     "die Zeile zeigt \"%1\" -- der Versatz der Scheibe "
                     "kommt nicht an").arg(inst->vfoRowText())));

        // Und zurueck: ausgeschaltet verschwindet er wieder, sonst
        // behauptet die Zeile einen Versatz, den es nicht mehr gibt.
        slice.setRitEnabled(false);
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        QVERIFY(!inst->vfoRowText().contains(QStringLiteral("RIT")));
    }
    // ── Die Optionen muessen ERREICHBAR sein (2026-10-31) ────────
    //
    // Der Betreiber: „frequency widget bitte mit option ausstatten,
    // was alles genau angezeigt werden soll. entweder nur frequenz,
    // oder mit swr, oder mit leistung usw."
    //
    // Es gab sie alle schon — fuenf einzelne Schalter, seit dem
    // 2026-08-23 und dem 2026-08-30. Nur: `hasExtendedSettings()`
    // stand auf dem Vorgabewert false, und `GridCellWidget.cpp:235`
    // zeichnet das Zahnrad im Panelkopf GENAU DANN, wenn das wahr
    // ist. Also kein Knopf; die Schalter lagen ausschliesslich hinter
    // der rechten Maustaste, und wer das nicht weiss, findet nichts.
    //
    // Derselbe Fehler wie schon dreimal am 2026-10-30: gebaut, aber
    // unerreichbar. Dieser Lauf haelt die Erreichbarkeit fest, nicht
    // das Vorhandensein — `showsSwr()` und die anderen gab es ja die
    // ganze Zeit.
    //
    // KEIN `openExtendedSettings()` hier: `QMenu::exec()` dreht auf
    // macOS eine Cocoa-Schleife, in der weder Taktgeber noch
    // gepostete Ereignisse ankommen (2026-10-15 nachgewiesen,
    // `tst_real_menus_hang_on_the_window`). Geprueft wird der
    // Schalter, der den Knopf erscheinen laesst, und dasz beide
    // Zugaenge dasselbe Menue bauen.
    void dieOptionenHaengenAmZahnradNichtNurAnDerRechtenMaustaste()
    {
        RadioModel model;
        FrequencyApplet applet(&model);
        QVERIFY2(applet.hasExtendedSettings(),
                 "ohne dies zeichnet GridCellWidget kein Zahnrad — die "
                 "Anzeigeschalter waeren nur per rechter Maustaste zu "
                 "finden");

        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }
        QFile f(QString::fromLocal8Bit(root)
                + QStringLiteral("/src/gui/applets/FrequencyApplet.cpp"));
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));
        const QString src = QString::fromUtf8(f.readAll());

        // Eine Quelle fuer beide Wege: sonst driften sie auseinander,
        // und einer der beiden zeigt irgendwann weniger Schalter.
        for (const QString& kopf : {QStringLiteral("void FrequencyApplet::openExtendedSettings("),
                                    QStringLiteral("void FrequencyApplet::contextMenuEvent(")}) {
            const int a = src.indexOf(kopf);
            QVERIFY2(a >= 0, qPrintable(kopf));
            const int e = src.indexOf(QStringLiteral("\n}\n"), a);
            QVERIFY(e > a);
            QVERIFY2(src.mid(a, e - a).contains(QStringLiteral("baueOptionsMenue")),
                     qPrintable(kopf + QStringLiteral(" baut sein Menue selbst")));
        }

        // Und die fuenf Schalter stehen wirklich darin.
        const int b = src.indexOf(QStringLiteral("QMenu* FrequencyApplet::baueOptionsMenue"));
        QVERIFY(b > 0);
        const int be = src.indexOf(QStringLiteral("\n}\n"), b);
        const QString menue = src.mid(b, be - b);
        for (const QString& schalter : {QStringLiteral("setShowTiles"),
                                        QStringLiteral("setShowVfo"),
                                        QStringLiteral("setShowPower"),
                                        QStringLiteral("setShowSwr"),
                                        QStringLiteral("setShowSignal")}) {
            QVERIFY2(menue.contains(schalter), qPrintable(schalter));
        }
    }
    // ── Auch im SCHWEBENDEN Fenster (2026-10-31) ────────────────
    //
    // Betreiber, mit Bildschirmfoto: „bei der frequenz noch immer
    // keine optionen". `hasExtendedSettings()` stand da schon auf
    // true -- aber das Zahnrad zeichnete nur `GridCellWidget`, also
    // die Applet-Spalte. Bei ihm steht das Frequenz-Panel als
    // schwebendes Fenster, und dessen Titelleiste kannte gar keinen
    // solchen Knopf.
    //
    // Die Lehre, zum zweiten Mal am selben Tag: „erreichbar" heiszt
    // nicht „in EINEM Weg erreichbar". Es gibt zwei Gehaeuse fuer
    // dasselbe Applet, und beide muessen den Knopf zeichnen.
    void dasZahnradGibtEsAuchImSchwebendenFenster()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }

        QFile ch(QString::fromLocal8Bit(root)
                 + QStringLiteral("/src/gui/WindowChrome.h"));
        QVERIFY2(ch.open(QIODevice::ReadOnly), qPrintable(ch.fileName()));
        const QString chrome = QString::fromUtf8(ch.readAll());
        QVERIFY2(chrome.contains(QStringLiteral("setOptionsVisible")),
                 "die Titelleiste kennt keinen Zahnrad-Knopf");
        QVERIFY2(chrome.contains(QStringLiteral("optionsRequested")),
                 "der Knopf meldet nichts");

        QFile fw(QString::fromLocal8Bit(root)
                 + QStringLiteral("/src/gui/applets/AppletFloatingWindow.cpp"));
        QVERIFY2(fw.open(QIODevice::ReadOnly), qPrintable(fw.fileName()));
        const QString win = QString::fromUtf8(fw.readAll());
        QVERIFY2(win.contains(QStringLiteral("hasExtendedSettings()")),
                 "das schwebende Fenster fragt nicht, ob es Optionen gibt");
        QVERIFY2(win.contains(QStringLiteral("openExtendedSettings()")),
                 "der Knopf im schwebenden Fenster oeffnet nichts");
    }
};

QTEST_MAIN(TstFrequencyWidgetTunes)
#include "tst_frequency_widget_tunes.moc"
