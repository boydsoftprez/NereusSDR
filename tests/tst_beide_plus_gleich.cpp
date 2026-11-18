// =================================================================
// tests/tst_beide_plus_gleich.cpp  (Longpath)
// =================================================================
//
// Beide Plus-Knoepfe bieten dasselbe an.
//
// Es gibt zwei: oben in der Befehlsleiste und unten rechts ueber der
// Arbeitsflaeche. Am 2026-11-12 hatte nur der untere den neuen
// FT8-Einschalter (und die Contest-Panels fehlten oben schon seit dem
// 2026-11-04). Martin oeffnete den oberen und sah „FT8 Decoder OFF" mit
// dem alten Text. Seine Worte: „beim plus oben oder unten muss alles
// natürlich erreichbar sein", „sonst findet das nie wer".
//
// Geprueft am echten Hauptfenster: es gibt zwei Plus-Knoepfe, ihre
// Auswaehler zeigen dieselben Karten, und in beiden laesst sich der
// FT8-Dekoder anklicken, obwohl ohne Funkgeraet keine Datenbetriebsart
// steht.
//
// ── Seit dem 2026-11-18 gibt es nur noch EINES ──────────────────────
//
// Betreiber: „Das gelbe Plus rechts unten bitte komplett löschen,
// rechts oben alleine reicht!" Seit das Plus oben eine orange-gelbe
// Taste ist (2026-11-12), stand dieselbe Taste zweimal im Bild.
//
// Der Lauf heisst weiter wie die Datei -- die Lehre dahinter gilt
// unveraendert („alles muss erreichbar sein, sonst findet das nie
// wer") --, prueft aber jetzt: es gibt GENAU ein Plus, es sitzt in der
// Befehlsleiste (die nie ausgeblendet wird), und es kann alles, was
// vorher beide konnten.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-12 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QApplication>
#include <QDialog>
#include <QTimer>
#include <cstdio>
#include <cstdlib>

#include "gui/MainWindow.h"
#include "gui/widgets/WidgetPicker.h"
#include "FensterFrist.h"

using namespace Longpath;

class TstBeidePlusGleich : public QObject {
    Q_OBJECT

private:
    MainWindow* m_mw{nullptr};

private slots:
    void initTestCase()
    {
        // Ein modaler Dialog (Contestprogramm im leeren Pruefordner)
        // haette hier niemanden, der antwortet.
        auto* zu = new QTimer(this);
        connect(zu, &QTimer::timeout, this, []() {
            if (auto* d = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
                d->reject();
            }
        });
        zu->start(100);

        m_mw = new MainWindow();
        m_mw->resize(1280, 800);
        m_mw->show();
        QVERIFY2(QTest::qWaitForWindowExposed(m_mw, kFensterFrist),
                 "das Hauptfenster muss ueberhaupt erscheinen");
        QTest::qWait(300);
    }

    void esGibtGenauEinPlusUndEsKannAlles()
    {
        const QList<AddWidgetButton*> plus = m_mw->findChildren<AddWidgetButton*>();
        QVERIFY2(plus.size() == 1,
                 qPrintable(QStringLiteral("erwartet genau ein Plus, gefunden: %1 -- "
                                           "das untere sollte weg sein")
                                .arg(plus.size())));
        AddWidgetButton* p = plus.first();

        // Es sitzt in der Befehlsleiste, nicht mehr lose ueber der
        // Arbeitsflaeche -- und es ist zu sehen.
        QVERIFY2(p->parentWidget() != m_mw->centralWidget(),
                 "das Plus haengt wieder lose ueber der Arbeitsflaeche");
        QVERIFY2(p->isVisible(), "das eine Plus ist nicht zu sehen");

        // Und es kann alles: der FT8-Dekoder ist da und anklickbar,
        // obwohl ohne Funkgeraet keine Datenbetriebsart steht.
        p->openPicker();
        WidgetPicker* w = p->picker();
        QVERIFY(w);
        w->setCategory(WidgetPicker::allCategory());
        const QStringList e = w->entries();
        QVERIFY2(e.contains(QStringLiteral("Ft8Decoder")),
                 "der FT8-Dekoder fehlt im Auswaehler");
        QVERIFY2(w->isEnabled(QStringLiteral("Ft8Decoder")),
                 "im Auswaehler ist der FT8-Dekoder grau -- der Einschalter fehlt");
        w->close();
    }
};

// Kein QTEST_MAIN: das Abraeumen eines echten Hauptfensters stuerzt ab,
// und jede Absturzmeldung landet auf Martins Schirm. Siehe
// tst_profil_loeschen_laesst_andere_stehen.
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    TstBeidePlusGleich t;
    QTEST_SET_MAIN_SOURCE_PATH
    const int rc = QTest::qExec(&t, argc, argv);
    std::fflush(nullptr);
    std::_Exit(rc);
}

#include "tst_beide_plus_gleich.moc"
