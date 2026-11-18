// =================================================================
// tests/tst_kurvenpuffer_nur_wenn_gezeichnet.cpp  (Longpath)
// =================================================================
//
// Longpath-original test.
//
// Betreiber, 2026-11-18: „das beenden dauert immer sehr lange und macht
// mich nachdenklich warum."
//
// Am haengenden Prozess aufgenommen (`sample`, das Beenden dauerte
// 64 s nach 74 Minuten FT8 an der Flex):
//
//   MainWindow::closeEvent -> PanadapterStack::shutDownFloating
//     -> SpectrumWidget::prepareForShutdown -> releaseResources
//       -> ~QMetalBuffer -> QMetalBuffer::destroy()   4051 von 4065 Proben
//   Physical footprint 23,0 GB, Spitze 42,3 GB (Rechner: 16 GB)
//
// Metal schreibt eine Aenderung an einem dynamischen Puffer erst, wenn
// er in einem Zeichenbefehl gebunden wird; bis dahin haengt sie mit
// einer Kopie der Daten in einer Liste. Sein Panadapter steht im
// 3D-Modus (DisplaySpectrumRenderMode = 1): dort wird die 2D-Kurve gar
// nicht gezeichnet -- Kurve, Fuellung und Peak-Hold wurden trotzdem in
// JEDEM Bild neu in ihre Puffer geschoben.
//
// ── Was hier geprueft wird und was nicht ────────────────────────────
//
// Die Entscheidung „welcher Puffer wird in diesem Bild gefuellt" ist
// eine reine Funktion und laesst sich vollstaendig pruefen. DASS die
// drei Aufrufe wirklich hinter ihr stehen, wird am Quelltext geprueft:
// der GPU-Weg selbst hat unter der Offscreen-Plattform kein Backend
// (siehe tst_spectrum_trace_visible, das dort ueberspringt). Der
// Speicher am echten Geraet ist damit NICHT gemessen -- das geht nur an
// der laufenden App.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-18 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>

#include <QFile>
#include <QRegularExpression>

#include "gui/SpectrumWidget.h"

using namespace Longpath;

class TstKurvenpufferNurWennGezeichnet : public QObject {
    Q_OBJECT

private slots:
    // Sein Fall: 3D-Modus, Fuellung an, Peak-Hold an. Der Durchgang
    // zeichnet die 2D-Kurve nicht -- also wird KEINER der drei gefuellt.
    void imDreiDModusWirdNichtsGefuellt()
    {
        const SpectrumWidget::KurvenPuffer p = SpectrumWidget::kurvenPufferFuellen(
            SpectrumRenderMode::Mode3D, /*pipelinesDa=*/true,
            /*fuellungAn=*/true, /*peakHoldBereit=*/true);
        QVERIFY2(!p.linie,    "3D: die Kurve wird gefuellt, aber nie gezeichnet");
        QVERIFY2(!p.fuellung, "3D: die Fuellung wird gefuellt, aber nie gezeichnet");
        QVERIFY2(!p.peakHold, "3D: Peak-Hold wird gefuellt, aber nie gezeichnet");
    }

    // 2D: die Kurve immer, die beiden anderen nur, wenn sie an sind.
    void imZweiDModusNurWasAnIst()
    {
        auto f = [](bool fuellung, bool peak) {
            return SpectrumWidget::kurvenPufferFuellen(
                SpectrumRenderMode::Mode2D, true, fuellung, peak);
        };
        QVERIFY(f(false, false).linie);
        QVERIFY(!f(false, false).fuellung);
        QVERIFY(!f(false, false).peakHold);

        QVERIFY(f(true, false).fuellung);
        QVERIFY(!f(true, false).peakHold);

        QVERIFY(!f(false, true).fuellung);
        QVERIFY(f(false, true).peakHold);

        QVERIFY(f(true, true).linie);
        QVERIFY(f(true, true).fuellung);
        QVERIFY(f(true, true).peakHold);
    }

    // Ohne Pipelines zeichnet der Durchgang gar nichts.
    void ohnePipelinesNichts()
    {
        const SpectrumWidget::KurvenPuffer p = SpectrumWidget::kurvenPufferFuellen(
            SpectrumRenderMode::Mode2D, /*pipelinesDa=*/false, true, true);
        QVERIFY(!p.linie);
        QVERIFY(!p.fuellung);
        QVERIFY(!p.peakHold);
    }

    // Die Verdrahtung: jeder der drei Aufrufe steht hinter seiner
    // Entscheidung. Ein vierter Puffer, der ohne Tor gefuellt wird,
    // faellt hier ebenfalls auf.
    void jederAufrufStehtHinterSeinerEntscheidung()
    {
        const QString wurzel = qEnvironmentVariable("LONGPATH_SOURCE_DIR");
        QVERIFY2(!wurzel.isEmpty(),
                 "LONGPATH_SOURCE_DIR fehlt -- ueber ctest starten");
        QFile f(wurzel + QStringLiteral("/src/gui/SpectrumWidget.cpp"));
        QVERIFY2(f.open(QIODevice::ReadOnly), "SpectrumWidget.cpp nicht lesbar");
        const QString quelle = QString::fromUtf8(f.readAll());

        const QHash<QString, QString> tor{
            {QStringLiteral("m_fftLineVbo"), QStringLiteral("if (puffer.linie)")},
            {QStringLiteral("m_fftFillVbo"), QStringLiteral("if (puffer.fuellung)")},
            {QStringLiteral("m_fftPeakVbo"), QStringLiteral("if (puffer.peakHold")},
        };
        const QRegularExpression aufruf(
            QStringLiteral("updateDynamicBuffer\\((m_fft\\w+Vbo)"));
        int gefunden = 0;
        auto it = aufruf.globalMatch(quelle);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            ++gefunden;
            const QString name = m.captured(1);
            QVERIFY2(tor.contains(name),
                     qPrintable(QStringLiteral(
                         "ein weiterer Kurvenpuffer wird gefuellt (%1) -- er "
                         "braucht dieselbe Entscheidung").arg(name)));
            // Das Tor steht in den Zeilen davor (der Peak-Hold-Block ist
            // lang: seine Ecken werden zwischen Tor und Aufruf gerechnet).
            const int ab = qMax(0, m.capturedStart() - 6000);
            const QString davor = quelle.mid(ab, m.capturedStart() - ab);
            QVERIFY2(davor.contains(tor.value(name)),
                     qPrintable(QStringLiteral(
                         "%1 wird ohne '%2' gefuellt -- im 3D-Modus staut "
                         "sich das ueber die Sitzung")
                         .arg(name, tor.value(name))));
        }
        QCOMPARE(gefunden, 3);
    }
};

QTEST_MAIN(TstKurvenpufferNurWennGezeichnet)
#include "tst_kurvenpuffer_nur_wenn_gezeichnet.moc"
