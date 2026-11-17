// tests/tst_schwebende_folgen_dem_hauptfenster.cpp  (Longpath)
//
// Betreiber am 2026-11-04: "sobald ich das _ oder das + usw. links oben
// verwenden muss sich das ganze!!!! Fenster bewegen! auch die
// freischwebenden im verhältnis!"
//
// Die abgeloesten Applets, der schwebende Panadapter, Rotor/Log, die
// Seitenflaeche und seit heute das Contest-Logbuch sind Qt::Tool-Fenster
// mit dem Hauptfenster als Elternteil. Bewegt sich das Hauptfenster
// (Zug am Titelbalken, das "+" links oben, Vollbild), blieben sie bisher
// stehen, wo sie waren. Jetzt folgen sie: ein Zug verschiebt sie um
// denselben Betrag, eine Groessenaenderung bildet ihre Lage im Verhaeltnis
// zum alten Rahmen auf den neuen ab -- und seit 2026-11-11 auch ihre
// Groesse. Betreiber: „ich hatte es bewusst kleiner gemacht, das
// hauptfenster. wenn ich das mache, müssen alle anderen fenster auch im
// verhältnis kleiner werden."
//
// Alte Fassung: die Faelle 1 und 2 fallen um (das Fenster steht noch).
// Fassung vom 2026-11-04: Fall 2 und 2b fallen um (die Groesse blieb).
//
// Bewusst nicht abgeraeumt (wie tst_dock_ablage_nimmt_fenster_mit).
//
// =================================================================
// Modification history (Longpath):
//   2026-11-04 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
//   2026-11-11 — Groesse im Verhaeltnis (Fall 2, neuer Fall 2b).
//                 Martin Fischer (OE5SOS), KI-gestuetzt mit Anthropic
//                 Claude Code.
//   2026-11-13 — Fall "klein und wieder gross" stellte seine Lage nie
//                 her (stehengebliebene Anteile aus dem Fall davor).
//                 Martin Fischer (OE5SOS), KI-gestuetzt mit Anthropic
//                 Claude Code.
//   2026-11-14 — Die Meldung eines roten Falles gibt jetzt die Maasse
//                 des Fensters, den Schirmplatz und die Grundlage des
//                 Mitziehens mit. Martin Fischer (OE5SOS), KI-gestuetzt
//                 mit Anthropic Claude Code.
//   2026-11-17 — Auf den Nachlauf warten statt fest zu schlafen: im
//                 Aufbau kam er unter Last zu spaet und verdarb die
//                 Grundlage fuer Fall 2. Martin Fischer (OE5SOS),
//                 KI-gestuetzt mit Anthropic Claude Code.
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>
#include <QPushButton>
#include <QScreen>
#include <QStringList>

#include "gui/MainWindow.h"
#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/GridCellWidget.h"
#include "gui/applets/AppletWidget.h"
#include "gui/WindowPlacement.h"
#include "FensterFrist.h"

using namespace Longpath;

class TestSchwebendeFolgenDemHauptfenster : public QObject
{
    Q_OBJECT

private:
    MainWindow* m_mw{nullptr};
    AppletFloatingWindow* m_abgeloest{nullptr};

    // Der Nachlauf des Hauptfensters (250 ms) plus Luft.
    static constexpr int kNachlaufMs = 700;
    // Nach dem Mitziehen rastet das schwebende Fenster auf Longpaths
    // Raster (16 px) ein -- bis zu einer halben Rasterweite Versatz sind
    // also das Raster, nicht ein Fehler (gemessen: (608,368) statt
    // (600,360)).
    static constexpr int kToleranzPx = kSnapGridPx / 2 + 2;

    // ── Auf den Nachlauf WARTEN, nicht schlafen (2026-11-17) ─────────
    //
    // Bis heute wartete jede Stelle fest `kNachlaufMs`. Im CI-Lauf
    // 43636263627 (Shard Linux 2/4) reichte das im Aufbau nicht: der
    // Nachlauf nach `hauptfensterStellen()` kam erst, NACHDEM
    // initTestCase das schwebende Fenster schon selbst verschoben hatte.
    // Er mass das verschobene Fenster dann gegen den alten, maximierten
    // Rahmen -- und legte die Grundlage x=0.1667 y=0.1162 h=0.4086,
    // mit der Fall 2 spaeter 285x269 statt 395x251 rechnete. Lokal
    // nachgestellt, indem das Warten im Aufbau entfaellt: dieselben
    // drei Anteile auf vier Stellen.
    //
    // Ein ruhender Nachlauf allein genuegt nicht: kommt das
    // Groessenereignis selbst zu spaet, ruht er, BEVOR ueberhaupt etwas
    // geschehen ist. Darum wartet das hier, bis das Hauptfenster die
    // verlangte Groesse (bis aufs Raster) angenommen hat, sein Nachlauf
    // ruht und die Grundlage des Mitziehens auf dem jetzigen Rahmen
    // steht. Die feste Zeit davor bleibt als Untergrenze: sie deckt den
    // Raster-Nachhall (180 ms) ab, der nach dem Nachlauf noch kommt.
    bool nachlaufFertig(const QSize& verlangt) const
    {
        const QRect f = m_mw->frameGeometry();
        if (verlangt.isValid()
            && (qAbs(f.width() - verlangt.width()) > kSnapGridPx
                || qAbs(f.height() - verlangt.height()) > kSnapGridPx)) {
            return false;
        }
        return m_mw->nachlaufRuhtForTest()
            && m_mw->hauptrahmenBasisForTest() == f;
    }

    void warteAufNachlauf(const QSize& verlangt = QSize())
    {
        QTest::qWait(kNachlaufMs);
        QTRY_VERIFY2_WITH_TIMEOUT(
            nachlaufFertig(verlangt),
            qPrintable(QStringLiteral(
                "Nachlauf des Hauptfensters kam nicht zur Ruhe: Rahmen "
                "%1x%2, verlangt %3x%4, Grundlage %5x%6")
                .arg(m_mw->frameGeometry().width())
                .arg(m_mw->frameGeometry().height())
                .arg(verlangt.width()).arg(verlangt.height())
                .arg(m_mw->hauptrahmenBasisForTest().width())
                .arg(m_mw->hauptrahmenBasisForTest().height())),
            5000);
    }

    AppletFloatingWindow* loeseEinApplet()
    {
        auto* panel = m_mw->findChild<AppletPanelWidget*>();
        if (!panel) { return nullptr; }
        QPushButton* detach = nullptr;
        for (GridCellWidget* c : panel->findChildren<GridCellWidget*>()) {
            if (!c->titleBar() || !c->titleBar()->isVisible()) { continue; }
            for (QPushButton* b : c->titleBar()->findChildren<QPushButton*>()) {
                if (b->text() == QStringLiteral("↗") && b->isVisible()) {
                    detach = b; break;
                }
            }
            if (detach) { break; }
        }
        if (!detach) { return nullptr; }
        const int vorher = m_mw->findChildren<AppletFloatingWindow*>().size();
        QTest::mouseClick(detach, Qt::LeftButton);
        QTest::qWait(200);
        const QList<AppletFloatingWindow*> wins =
            m_mw->findChildren<AppletFloatingWindow*>();
        if (wins.size() <= vorher) { return nullptr; }
        return wins.last();
    }

    // Das abgeloeste Fenster tief, aber SICHER innerhalb der
    // Arbeitsflaeche stellen (2026-11-13).
    //
    // Eine feste Zahl wie "520 Punkte unter der Oberkante" traegt nicht:
    // auf macOS wird dasselbe Fenster doppelt so hoch wie unter der
    // offscreen-Plattform hier (301 gegen 157 Punkte), und die Leisten
    // fressen dort 150 Punkte statt weniger. Das Fenster ragte dann in
    // die Statusleiste, der Schutzrahmen schob es waehrend der Messung
    // heraus -- und der Pruefstand zaehlte das als "zog mit"
    // (CI-Lauf 43523526068, Shard macOS 2/2: zwei Faelle, Versatz
    // (0,-192) und 224 Punkte).
    //
    // `reserveUnten` ist der Platz, den der Fall selbst noch verliert:
    // die Hoehe, um die er das Hauptfenster danach verkleinert, oder der
    // Weg, um den er es nach oben zieht.
    void tiefAberSicherStellen(int reserveUnten)
    {
        const QMargins leisten = m_mw->leistenRaenderForTest();
        const QRect arbeit = m_mw->frameGeometry().marginsRemoved(leisten);
        const int hoch = m_abgeloest->frameGeometry().height();
        const int hoechstesY =
            arbeit.bottom() + 1 - reserveUnten - kSnapGridPx - hoch;
        const int y = qMin(arbeit.y() + 520, hoechstesY);
        QVERIFY2(y >= arbeit.y(),
                 qPrintable(QStringLiteral(
                     "kein Platz: Arbeitsflaeche %1..%2, Fenster %3 hoch, "
                     "Reserve %4")
                     .arg(arbeit.y()).arg(arbeit.bottom()).arg(hoch)
                     .arg(reserveUnten)));
        {
            const EigeneBewegung wirSelbst;
            m_abgeloest->move(arbeit.x() + 300, y);
        }
        warteAufNachlauf();
    }

    // Das Hauptfenster in eine bekannte, NICHT maximierte Lage bringen
    // und warten, bis Raster und Nachlauf durch sind.
    // ── Was eine rote Meldung sonst nicht hergibt (2026-11-14) ──────
    //
    // Im CI-Lauf 43619257941 (Shard Linux 2/4) fiel Fall 2 einmal mit
    // 285x269 statt 395x251 (vorher 269x189) und war im Neulauf gruen.
    // Schmaeler UND hoeher als das Verhaeltnis: ein noch ausstehender
    // Nachlauf erklaert das nicht, der geht in eine Richtung. In Frage
    // kamen `expandedTo(minimumSize)`, die Schirmbegrenzung und eine aus
    // dem Fall davor wiederverwendete Grundlage (MainWindow::
    // schwebendeUmrechnen) -- aus den Protokollzahlen allein nicht zu
    // entscheiden. Also gibt die Meldung jetzt die Zeugen mit, damit der
    // naechste rote Lauf die Frage selbst beantwortet.
    QString zustandDesFensters() const
    {
        if (!m_abgeloest) { return QStringLiteral(" [kein Fenster]"); }
        auto masz = [](const QString& was, const QSize& s) {
            return QStringLiteral("%1 %2x%3").arg(was).arg(s.width()).arg(s.height());
        };
        QStringList teile;
        teile << QStringLiteral("Applet \"%1\"").arg(m_abgeloest->appletId())
              << masz(QStringLiteral("minimumSize"), m_abgeloest->minimumSize())
              << masz(QStringLiteral("minimumSizeHint"), m_abgeloest->minimumSizeHint())
              << masz(QStringLiteral("sizeHint"), m_abgeloest->sizeHint())
              << masz(QStringLiteral("maximumSize"), m_abgeloest->maximumSize());
        const QRect rahmen = m_abgeloest->frameGeometry();
        teile << QStringLiteral("Rahmen %1,%2 %3x%4")
                     .arg(rahmen.x()).arg(rahmen.y())
                     .arg(rahmen.width()).arg(rahmen.height());
        if (const QScreen* scr = m_abgeloest->screen()) {
            const QRect platz = scr->availableGeometry();
            teile << QStringLiteral("Schirmplatz %1,%2 %3x%4")
                         .arg(platz.x()).arg(platz.y())
                         .arg(platz.width()).arg(platz.height());
        }
        const QRect bezug = m_mw->hauptrahmenBasisForTest();
        teile << QStringLiteral("Bezugsrahmen %1,%2 %3x%4")
                     .arg(bezug.x()).arg(bezug.y())
                     .arg(bezug.width()).arg(bezug.height())
              << QStringLiteral("Grundlage: %1")
                     .arg(m_mw->mitziehBasisAlsTextForTest(m_abgeloest));
        return QStringLiteral(" [%1]").arg(teile.join(QStringLiteral("; ")));
    }

    void hauptfensterStellen(const QRect& r)
    {
        if (m_mw->isMaximized() || m_mw->isFullScreen()) { m_mw->showNormal(); }
        m_mw->setGeometry(r);
        warteAufNachlauf(r.size());
    }

private slots:

    void initTestCase()
    {
        m_mw = new MainWindow();
        m_mw->resize(1280, 800);
        m_mw->show();
        QVERIFY2(QTest::qWaitForWindowExposed(m_mw, kFensterFrist),
                 "das Hauptfenster muss ueberhaupt erscheinen");
        QTest::qWait(300);

        m_abgeloest = loeseEinApplet();
        QVERIFY2(m_abgeloest, "ein Applet muss sich abloesen lassen");
        QTRY_VERIFY_WITH_TIMEOUT(m_abgeloest->isVisible(), 3000);

        // Der Schirm der offscreen-Plattform misst von sich aus 800x800,
        // das Hauptfenster braucht mindestens 800x600: da stoesst jedes
        // schwebende Fenster an den Rand oder an die Bedienleisten, die
        // der Schutzrahmen freihaelt, und misst Falsches. Darum bekommt
        // DIESER Pruefstand in tests/CMakeLists.txt einen 1920x1200-Schirm
        // (tests/offscreen-grosser-schirm.json); hier wird das geprueft,
        // damit ein zu kleiner Schirm als solcher gemeldet wird und nicht
        // als "Fenster folgt nicht".
        const QRect schirm = m_mw->screen()->availableGeometry();
        QVERIFY2(schirm.width() >= 1600 && schirm.height() >= 1000,
                 qPrintable(QStringLiteral("Pruefschirm zu klein: %1x%2")
                                .arg(schirm.width()).arg(schirm.height())));
        hauptfensterStellen(QRect(64, 64, 1024, 640));
        // Das abgeloeste Fenster mitten INS Hauptfenster legen (weg von
        // den Bedienleisten), damit das Verhaeltnis eindeutig ist. Eigene
        // Bewegung: kein Ziehen.
        {
            const EigeneBewegung wirSelbst;
            m_abgeloest->move(m_mw->frameGeometry().topLeft() + QPoint(256, 160));
        }
        warteAufNachlauf();
        QVERIFY(m_mw->schwebendeMitziehenForTest() == 0);
    }

    // ── Fall 1: ein Zug am Hauptfenster nimmt das Fenster mit ────────────
    void zugVerschiebtDasSchwebendeFensterMit()
    {
        const QRect hauptVorher = m_mw->frameGeometry();
        const QPoint vorher = m_abgeloest->frameGeometry().topLeft();

        m_mw->move(m_mw->pos() + QPoint(160, 96));
        warteAufNachlauf();

        const QPoint hauptDelta =
            m_mw->frameGeometry().topLeft() - hauptVorher.topLeft();
        QVERIFY2(hauptDelta != QPoint(0, 0), "das Hauptfenster muss sich bewegt haben");
        const QPoint delta = m_abgeloest->frameGeometry().topLeft() - vorher;
        QVERIFY2(qAbs(delta.x() - hauptDelta.x()) <= kToleranzPx
                     && qAbs(delta.y() - hauptDelta.y()) <= kToleranzPx,
                 qPrintable(QStringLiteral(
                     "Das schwebende Fenster ist um (%1,%2) gewandert, das "
                     "Hauptfenster um (%3,%4) -- es muss mit")
                     .arg(delta.x()).arg(delta.y())
                     .arg(hauptDelta.x()).arg(hauptDelta.y())));
    }

    // ── Fall 2: eine Groessenaenderung haelt das Verhaeltnis ─────────────
    void groessenaenderungHaeltDasVerhaeltnis()
    {
        // ── Erst die Grundlage vergessen (2026-11-14) ───────────────
        //
        // Dieser Fall rechnet sein Soll aus der Groesse, die das Fenster
        // JETZT hat. Longpath rechnet aber aus `m_mitziehBasis` -- den
        // Anteilen, die beim letzten Mitziehen galten, und die bleiben
        // absichtlich stehen, solange das Fenster dort steht, wo wir es
        // hingesetzt haben (2026-11-13, fuer „klein und wieder gross").
        // Stammt diese Grundlage aus einem verdorbenen Durchlauf, sind
        // das zwei verschiedene Rechnungen: CI-Lauf 43636263627, Shard
        // Linux 2/4, gemessen 285x269 statt 395x251 -- mit Anteilen
        // x=0.1667 y=0.1162 w=0.1984 h=0.4086 statt der 0.2634/0.3881,
        // die zu 269x189 auf 1021x487 gehoeren. Verdorben hatte sie der
        // zu spaete Nachlauf im Aufbau (siehe warteAufNachlauf()); das
        // Warten dort ist die Behebung, dieses Vergessen haelt den Fall
        // davon unabhaengig, was die Faelle davor hinterlassen.
        //
        // Die Wiederverwendung selbst prueft Fall 5
        // (`kleinUndWiederGrossGibtDieAlteLage`). HIER soll frisch
        // gemessen werden, sonst misst der Fall nicht, was er behauptet.
        m_mw->vergissMitziehBasisForTest();

        // Seit 2026-11-12 im Verhaeltnis der ARBEITSFLAECHE (ohne die
        // Leisten oben und unten, die nicht mitwachsen).
        const QMargins leisten = m_mw->leistenRaenderForTest();
        const QRect alt = m_mw->frameGeometry();
        const QRect altA = alt.marginsRemoved(leisten);
        const QRect f = m_abgeloest->frameGeometry();
        const double fx = double(f.x() - altA.x()) / altA.width();
        const double fy = double(f.y() - altA.y()) / altA.height();
        const QSize innen = m_abgeloest->size();

        m_mw->resize(alt.width() + 480, alt.height() + 160);
        warteAufNachlauf(QSize(alt.width() + 480, alt.height() + 160));

        const QRect neu = m_mw->frameGeometry();
        const QRect neuA = neu.marginsRemoved(leisten);
        QVERIFY2(neu.size() != alt.size(), "das Hauptfenster muss gewachsen sein");
        const QRect g = m_abgeloest->frameGeometry();
        const int sollX = neuA.x() + qRound(fx * neuA.width());
        const int sollY = neuA.y() + qRound(fy * neuA.height());
        QVERIFY2(qAbs(g.x() - sollX) <= kToleranzPx
                     && qAbs(g.y() - sollY) <= kToleranzPx,
                 qPrintable(QStringLiteral(
                     "Lage (%1,%2), erwartet im Verhaeltnis (%3,%4)%5")
                     .arg(g.x()).arg(g.y()).arg(sollX).arg(sollY)
                     .arg(zustandDesFensters())));
        // Die Groesse waechst im selben Verhaeltnis (2026-11-11). Was
        // danach noch an ihr ruehrt, ist Longpaths Raster, das alle vier
        // Kanten des Rahmens einrasten laesst -- eine Rasterweite, wie
        // bei einem Zug von Hand auch.
        const QSize soll(qRound(innen.width() * double(neuA.width()) / altA.width()),
                         qRound(innen.height() * double(neuA.height()) / altA.height()));
        const QSize ist = m_abgeloest->size();
        QVERIFY2(qAbs(ist.width() - soll.width()) <= kSnapGridPx
                     && qAbs(ist.height() - soll.height()) <= kSnapGridPx,
                 qPrintable(QStringLiteral("Groesse %1x%2, erwartet %3x%4 (vorher %5x%6)%7")
                                .arg(ist.width()).arg(ist.height())
                                .arg(soll.width()).arg(soll.height())
                                .arg(innen.width()).arg(innen.height())
                                .arg(zustandDesFensters())));
    }

    // ── Fall 2b: verkleinert der Betreiber das Hauptfenster, werden die
    //    schwebenden Fenster im Verhaeltnis kleiner (2026-11-11) ───────────
    void verkleinernMachtSieKleiner()
    {
        // Wie in Fall 2: frisch messen, sonst rechnet Longpath mit den
        // Anteilen von dort und der Fall misst etwas anderes.
        m_mw->vergissMitziehBasisForTest();

        const QMargins leisten = m_mw->leistenRaenderForTest();
        const QRect alt = m_mw->frameGeometry();
        const QRect altA = alt.marginsRemoved(leisten);
        const QSize innen = m_abgeloest->size();

        m_mw->resize(alt.width() - 360, alt.height() - 120);
        warteAufNachlauf(QSize(alt.width() - 360, alt.height() - 120));

        const QRect neu = m_mw->frameGeometry();
        const QRect neuA = neu.marginsRemoved(leisten);
        QVERIFY2(neu.width() < alt.width(), "das Hauptfenster muss kleiner geworden sein");
        const QSize ist = m_abgeloest->size();
        const QSize soll = QSize(qRound(innen.width() * double(neuA.width()) / altA.width()),
                                 qRound(innen.height() * double(neuA.height()) / altA.height()))
                               .expandedTo(m_abgeloest->minimumSize());
        QVERIFY2(ist.width() < innen.width() || ist.width() == m_abgeloest->minimumWidth(),
                 qPrintable(QStringLiteral("Breite %1, vorher %2 -- nicht kleiner geworden")
                                .arg(ist.width()).arg(innen.width())));
        QVERIFY2(qAbs(ist.width() - soll.width()) <= kSnapGridPx
                     && qAbs(ist.height() - soll.height()) <= kSnapGridPx,
                 qPrintable(QStringLiteral("Groesse %1x%2, erwartet %3x%4 (vorher %5x%6)%7")
                                .arg(ist.width()).arg(ist.height())
                                .arg(soll.width()).arg(soll.height())
                                .arg(innen.width()).arg(innen.height())
                                .arg(zustandDesFensters())));
    }

    // ── Fall 3: eine Profilanwendung (eigene Bewegung) zieht NICHT ───────
    //
    // Mit einem Versatz NEBEN dem Raster (-93,-41): das Raster rueckt das
    // Hauptfenster danach um wenige Pixel nach, als eigene Bewegung im
    // laufenden Nachlauf -- genau der Weg, auf dem beim Aufbau am
    // 2026-11-04 fuenf Fenster gezogen wurden (Geraetelog 12:09:43,
    // "wieder irgendeine zusammensetzung der panels"). Ohne fremde
    // Bewegung davor darf das nichts ziehen.
    // 2026-11-12, Betreiber mit Bild: nach dem Verkleinern lag Combined
    // ueber den Profilknoepfen. Ein Fenster direkt unter der
    // Befehlsleiste bleibt beim Verkleinern unter ihr.
    void verkleinernSchiebtNichtsInDieLeiste()
    {
        const QMargins leisten = m_mw->leistenRaenderForTest();
        QVERIFY2(leisten.top() > 0, "ohne Leiste oben prueft der Fall nichts");
        const QRect alt = m_mw->frameGeometry();
        m_abgeloest->move(alt.x() + 300, alt.y() + leisten.top() + 4);
        warteAufNachlauf();
        m_mw->schwebendeMitziehenForTest();   // Basis = jetzt

        m_mw->resize(alt.width() - 200, alt.height() - 160);
        warteAufNachlauf();

        const QRect neu = m_mw->frameGeometry();
        QVERIFY2(neu.height() < alt.height(), "das Hauptfenster muss niedriger geworden sein");
        const int unterDerLeiste = neu.y() + leisten.top();
        const QRect g = m_abgeloest->frameGeometry();
        qInfo() << "Leisten" << leisten << "Hauptfenster" << neu << "Fenster" << g;
        QVERIFY2(g.y() >= unterDerLeiste - kToleranzPx,
                 qPrintable(QStringLiteral("Fenster oben bei %1, Leiste endet bei %2")
                                .arg(g.y()).arg(unterDerLeiste)));
    }

    // 2026-11-12, Betreiber: „nicht alle windows werden automatisch
    // angepasst". Jeder Zug rechnete vom LETZTEN Stand weiter: hing ein
    // Fenster beim Verkleinern an seiner Untergrenze, kam es beim
    // Vergroessern nicht mehr auf seine alte Groesse zurueck. Jetzt gilt
    // je Fenster eine feste Grundlage (Anteile der Arbeitsflaeche).
    void kleinUndWiederGrossGibtDieAlteLage()
    {
        const QRect alt = m_mw->frameGeometry();
        const QSize hauptInnen = m_mw->size();

        // ── Die Grundlage frisch erzwingen (2026-11-13) ──────────────
        //
        // schwebendeMitziehenForTest() setzt nur den Hauptrahmen als
        // Basis. Die ANTEILE je Fenster (m_mitziehBasis) bleiben dagegen
        // stehen, solange das Fenster noch dort steht, wo das Mitziehen
        // es hingesetzt hat -- so ist es gewollt (MainWindow::
        // schwebendeUmrechnen: "solange das Fenster noch dort steht").
        // Dieser Fall rechnete deshalb mit den Anteilen aus dem Fall
        // davor weiter, und die stammten aus einem Stand, in dem das
        // Fenster im Verhaeltnis BREITER war: auf macOS kamen 269 Punkte
        // heraus, wo 260 die eben gesetzte Untergrenze war (CI-Lauf
        // 43497937016, Shard 2/2). Erst auf Linux, dann auf macOS -- der
        // Fall war nie hergestellt, nicht die Behebung war falsch.
        // Ein Zug von Hand (ohne EigeneBewegung, mehr als eine
        // Rasterweite) macht die Anteile ungueltig, dann messen sie
        // beim naechsten Umrechnen frisch.
        // (Das Stellen ist eine eigene Bewegung, zieht also nichts --
        // aber das Fenster steht danach nicht mehr dort, wo das
        // Mitziehen es hingesetzt hat, und genau daran haengt die
        // Frische der Anteile.)
        tiefAberSicherStellen(100);

        const QRect vorher = m_abgeloest->frameGeometry();
        const QSize innenVorher = m_abgeloest->size();
        const int minAlt = m_abgeloest->minimumWidth();
        // Die Untergrenze erzwingen: kleiner als jetzt darf es nicht.
        m_abgeloest->setMinimumWidth(innenVorher.width());
        m_mw->schwebendeMitziehenForTest();   // Basis = jetzt

        m_mw->resize(hauptInnen.width() - 400, hauptInnen.height() - 100);
        warteAufNachlauf();
        // Vor dem Vergleich sagen, was gemessen wurde: ein Fehlschlag in
        // der CI soll die Zahlen zeigen, nicht nur zwei Breiten.
        qInfo() << "Hauptfenster" << hauptInnen << "->" << m_mw->size()
                << "Untergrenze" << innenVorher.width()
                << "Fenster" << m_abgeloest->size();
        QVERIFY2(m_mw->width() < hauptInnen.width(),
                 "das Hauptfenster muss schmaler geworden sein, sonst "
                 "haengt das Fenster nie an seiner Untergrenze");
        QCOMPARE(m_abgeloest->width(), innenVorher.width());   // hing an der Grenze

        m_mw->resize(hauptInnen);
        warteAufNachlauf();
        m_abgeloest->setMinimumWidth(minAlt);

        const QRect nachher = m_abgeloest->frameGeometry();
        qInfo() << "Hauptfenster" << alt << "->" << m_mw->frameGeometry()
                << "Fenster vorher" << vorher << "nachher" << nachher;
        QVERIFY2(qAbs(m_mw->width() - hauptInnen.width()) <= kSnapGridPx
                     && qAbs(m_mw->height() - hauptInnen.height()) <= kSnapGridPx,
                 "das Hauptfenster muss wieder seine alte Groesse haben");
        QVERIFY2(qAbs(nachher.width() - vorher.width()) <= kSnapGridPx
                     && qAbs(nachher.height() - vorher.height()) <= kSnapGridPx
                     && qAbs(nachher.x() - vorher.x()) <= kToleranzPx
                     && qAbs(nachher.y() - vorher.y()) <= kToleranzPx,
                 qPrintable(QStringLiteral("vorher %1,%2 %3x%4 -- nachher %5,%6 %7x%8")
                     .arg(vorher.x()).arg(vorher.y()).arg(vorher.width()).arg(vorher.height())
                     .arg(nachher.x()).arg(nachher.y()).arg(nachher.width()).arg(nachher.height())));
    }

    // ── Der Startsprung schiebt nichts mehr (2026-11-12) ─────────────
    //
    // Betreiber: „ich habe alles gespeichert und wieder ist alles
    // verschoben". Im Protokoll jedes Starts: das Profil stellt die
    // Fenster an ihre gespeicherten Punkte, das Hauptfenster kommt aber
    // 58 Punkte niedriger herauf (ueber dem Dock), springt spaeter auf
    // die alte Hoehe -- und alle Fenster ziehen um 6,7 % mit. Gespeichert
    // wurde dann DAS, und der naechste Start schob weiter.
    //
    // Hier: Lage zum grossen Rahmen gespeichert, Hauptfenster kommt klein
    // herauf (eigene Bewegung, wie das Maximieren beim Aufbau), springt
    // auf gross (fremd). Am Ende muss das Fenster genau dort stehen, wo
    // es gespeichert war.
    void startsprungGibtDieGespeicherteLage()
    {
        // Platz nach unten: das Hauptfenster darf nicht unter 600 hoch,
        // und die 58 Punkte muessen wirklich ankommen.
        m_mw->resize(1300, 900);
        warteAufNachlauf();
        QVERIFY(m_mw->height() >= 900 - kSnapGridPx);
        // Tief im Fenster, wie das S-Meter des Betreibers: dort wirkt der
        // Sprung am staerksten (rund 30 Punkte). Der Fall verliert
        // danach 58 Punkte Hoehe -- so viel Reserve muss bleiben.
        tiefAberSicherStellen(58);
        const QRect gross = m_mw->frameGeometry();
        const QSize innenGross = m_mw->size();
        const QRect gespeichert = m_abgeloest->frameGeometry();
        m_mw->schwebendeMitziehenForTest();   // Basis = jetzt

        // „Start": kleiner herauf, als eigene Bewegung; das Profil hat
        // das Fenster an seinen gespeicherten Punkt gestellt.
        {
            const EigeneBewegung wirSelbst;
            m_mw->resize(innenGross.width(), innenGross.height() - 58);
            QCoreApplication::processEvents();
        }
        QVERIFY(m_mw->height() <= innenGross.height() - 58 + kSnapGridPx);
        warteAufNachlauf();
        QCOMPARE(m_abgeloest->frameGeometry().topLeft(), gespeichert.topLeft());
        // Ein frischer Start kennt keine Grundlage vom letzten Mal.
        m_mw->vergissMitziehBasisForTest();

        // Das Profil kennt seinen Rahmen: umrechnen.
        m_mw->setProfilRahmenForTest(gross);
        QVERIFY(m_mw->schwebendeNachProfilrahmenForTest());

        // Und der Sprung auf die alte Hoehe, von aussen.
        m_mw->resize(innenGross);
        warteAufNachlauf();

        const QRect nachher = m_abgeloest->frameGeometry();
        qInfo() << "gespeichert" << gespeichert << "nachher" << nachher
                << "Hauptfenster" << gross << "->" << m_mw->frameGeometry();
        QVERIFY2(qAbs(nachher.x() - gespeichert.x()) <= kToleranzPx
                     && qAbs(nachher.y() - gespeichert.y()) <= kToleranzPx
                     && qAbs(nachher.width() - gespeichert.width()) <= 2
                     && qAbs(nachher.height() - gespeichert.height()) <= 2,
                 qPrintable(QStringLiteral("gespeichert %1,%2 %3x%4 -- nachher %5,%6 %7x%8")
                     .arg(gespeichert.x()).arg(gespeichert.y())
                     .arg(gespeichert.width()).arg(gespeichert.height())
                     .arg(nachher.x()).arg(nachher.y())
                     .arg(nachher.width()).arg(nachher.height())));
    }

    // ── Zwei Punkte sind kein anderes Hauptfenster (2026-11-13) ──────────
    //
    // Betreiber: „oft nervt es, dass ich zum 20 mal ... die fenster zum
    // beispiel verschoben haben". Sein Protokoll vom 12.11., 21:50:
    // `Profilrahmen 1470x867 -> 1470x865 : 7 schwebende Fenster
    // umgerechnet`, danach 9 aus den Leisten geschoben -- bei JEDEM Start,
    // weil das maximierte Hauptfenster mal 865, mal (nach dem Raster) 867
    // hoch ist. Ein Unterschied unterhalb des Rasters ist kein anderes
    // Hauptfenster: die Fenster bleiben, wo das Profil sie hingestellt hat.
    void rasterUnterschiedRechnetNichtsUm()
    {
        m_mw->resize(1300, 900);
        warteAufNachlauf();
        tiefAberSicherStellen(0);
        const QRect jetzt = m_mw->frameGeometry();
        const QRect vorher = m_abgeloest->frameGeometry();
        const QSize innenVorher = m_abgeloest->size();
        m_mw->vergissMitziehBasisForTest();   // ein frischer Start

        // Gespeichert mit 2 Punkten mehr Hoehe -- 867 gegen 865.
        m_mw->setProfilRahmenForTest(QRect(jetzt.x(), jetzt.y(),
                                           jetzt.width(), jetzt.height() + 2));
        QVERIFY(m_mw->schwebendeNachProfilrahmenForTest());
        // Sofort, vor jedem Raster: nichts angefasst.
        QCOMPARE(m_abgeloest->size(), innenVorher);
        QCOMPARE(m_abgeloest->frameGeometry(), vorher);
        warteAufNachlauf();
        QCOMPARE(m_abgeloest->frameGeometry(), vorher);
    }

    void eigeneBewegungZiehtNicht()
    {
        // Eigene Ausgangslage: das Hauptfenster zieht gleich um 41
        // Punkte nach oben, seine Statusleiste kommt dem stehenden
        // Fenster also um 41 Punkte entgegen.
        tiefAberSicherStellen(41);
        const QPoint vorher = m_abgeloest->frameGeometry().topLeft();
        {
            const EigeneBewegung wirSelbst;
            m_mw->move(m_mw->pos() + QPoint(-93, -41));
            QCoreApplication::processEvents();
        }
        warteAufNachlauf();
        // Das Raster darf es um weniger als eine Rasterweite ruecken,
        // das Mitziehen haette es um rund (93,41) bewegt.
        const QPoint delta = m_abgeloest->frameGeometry().topLeft() - vorher;
        QVERIFY2(qAbs(delta.x()) < kSnapGridPx && qAbs(delta.y()) < kSnapGridPx,
                 qPrintable(QStringLiteral("eigene Bewegung zog mit: (%1,%2)")
                                .arg(delta.x()).arg(delta.y())));
    }
};

QTEST_MAIN(TestSchwebendeFolgenDemHauptfenster)
#include "tst_schwebende_folgen_dem_hauptfenster.moc"
