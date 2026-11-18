// =================================================================
// tests/tst_menue_pfeile.cpp  (Longpath)
// =================================================================
//
// Longpath-original test.
//
// Betreiber, 2026-11-17: „die beiden kleinen pfeile oben in der
// taskleiste neben containers bitte in farbe machen. die sieht keiner."
//
// Die zwei Pfeile sind Qts eigene Taste fuer die Menues, die in der
// Leiste keinen Platz mehr haben. Geprueft wird am Bild: vorher so gut
// wie kein Orange, nachher ueber die halbe Taste. Mit LONGPATH_BILD_DIR
// schreibt der Lauf beide Zustaende als PNG.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QMenu>
#include <QMenuBar>
#include <QToolButton>

#include "gui/StyleConstants.h"
#include "gui/TitleBar.h"
#include "FensterFrist.h"

using namespace Longpath;

namespace {

int orangePunkte(const QImage& bild)
{
    int n = 0;
    for (int y = 0; y < bild.height(); ++y) {
        for (int x = 0; x < bild.width(); ++x) {
            const QColor c = bild.pixelColor(x, y);
            if (c.red() > 110 && c.red() > c.blue() + 50 && c.green() > c.blue() + 20
                && c.red() >= c.green()) {
                ++n;
            }
        }
    }
    return n;
}

void schreibeBild(const QImage& bild, const QString& name)
{
    const QByteArray ziel = qgetenv("LONGPATH_BILD_DIR");
    if (ziel.isEmpty()) { return; }
    QDir().mkpath(QString::fromLocal8Bit(ziel));
    bild.save(QString::fromLocal8Bit(ziel) + QLatin1Char('/') + name);
}

// Eine Menueleiste, die zu schmal ist fuer ihre Menues -- wie seine.
QMenuBar* schmaleLeiste(QWidget* eltern)
{
    auto* mb = new QMenuBar(eltern);
    mb->setNativeMenuBar(false);
    mb->setStyleSheet(QStringLiteral("QMenuBar { background: %1; color: %2; }")
                          .arg(QLatin1String(Style::kPanelBg),
                               QLatin1String(Style::kTextPrimary)));
    for (const char* name : {"File", "Radio", "View", "DSP", "Band", "Mode",
                             "Containers", "Tools", "Help"}) {
        mb->addMenu(QString::fromLatin1(name))->addAction(QStringLiteral("x"));
    }
    mb->setGeometry(0, 0, 330, 28);
    return mb;
}

} // namespace

class TstMenuePfeile : public QObject {
    Q_OBJECT

private slots:
    void diePfeileSindOrange()
    {
        QWidget fenster;
        fenster.setStyleSheet(QStringLiteral("background: %1;")
                                  .arg(QLatin1String(Style::kPanelBg)));
        fenster.resize(360, 40);
        QMenuBar* mb = schmaleLeiste(&fenster);
        fenster.show();
        QVERIFY(QTest::qWaitForWindowExposed(&fenster, kFensterFrist));

        auto* taste = mb->findChild<QToolButton*>(QStringLiteral("qt_menubar_ext_button"));
        QVERIFY2(taste, "Qt hat der Menueleiste keine Erweiterungstaste gegeben");
        // Voraussetzung mitpruefen: die Leiste IST zu schmal, die Taste
        // steht also wirklich da -- sonst prueft der Rest nichts.
        QTRY_VERIFY2_WITH_TIMEOUT(taste->isVisible(), "die Leiste ist nicht zu schmal — "
                                  "die Taste erscheint gar nicht", 3000);

        const QImage vorher = taste->grab().toImage();
        schreibeBild(mb->grab().toImage(), QStringLiteral("menue-pfeile-1-vorher.png"));
        const int flaeche = vorher.width() * vorher.height();
        QVERIFY2(orangePunkte(vorher) < flaeche / 10,
                 "die Taste ist schon vor dem Faerben orange");

        TitleBar::faerbeMenueErweiterung(mb);
        QCoreApplication::processEvents();
        const QImage nachher = taste->grab().toImage();
        schreibeBild(mb->grab().toImage(), QStringLiteral("menue-pfeile-2-nachher.png"));
        QVERIFY2(orangePunkte(nachher) > nachher.width() * nachher.height() / 2,
                 qPrintable(QStringLiteral("nur %1 von %2 Bildpunkten sind orange")
                                .arg(orangePunkte(nachher))
                                .arg(nachher.width() * nachher.height())));
        // Schon orange: keine Klickmarke darueber.
        QCOMPARE(taste->property("klickmarke").toBool(), false);
        QVERIFY(!taste->toolTip().isEmpty());
    }

    // Ein Stilwechsel gibt der Taste das Systemsymbol zurueck -- der
    // Waechter setzt unseres wieder. Zweimal rufen bleibt harmlos.
    void dasZeichenUeberlebtEinenStilwechsel()
    {
        QWidget fenster;
        fenster.resize(360, 40);
        QMenuBar* mb = schmaleLeiste(&fenster);
        fenster.show();
        QVERIFY(QTest::qWaitForWindowExposed(&fenster, kFensterFrist));
        auto* taste = mb->findChild<QToolButton*>(QStringLiteral("qt_menubar_ext_button"));
        QVERIFY(taste);
        TitleBar::faerbeMenueErweiterung(mb);
        TitleBar::faerbeMenueErweiterung(mb);
        const qint64 unseres = taste->icon().cacheKey();

        taste->setIcon(QIcon());                       // „das System" setzt seines
        QEvent wechsel(QEvent::StyleChange);
        QCoreApplication::sendEvent(taste, &wechsel);
        QVERIFY2(!taste->icon().isNull(), "nach dem Stilwechsel ist das Zeichen weg");
        Q_UNUSED(unseres);
    }

    void ohneLeisteGeschiehtNichts()
    {
        TitleBar::faerbeMenueErweiterung(nullptr);     // darf nicht fallen
    }
};

QTEST_MAIN(TstMenuePfeile)
#include "tst_menue_pfeile.moc"
