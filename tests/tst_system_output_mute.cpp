// =================================================================
// tests/tst_system_output_mute.cpp  (Longpath)
// =================================================================
//
// Der Stummschalter des BETRIEBSSYSTEMS -- nicht der von Longpath.
//
// Anlass (2026-11-14): „ton geht schon wieder nicht … und. das haeuft
// sich!" Zum zweiten Mal in drei Tagen war nicht Longpath stumm,
// sondern der Mac (`osascript -e 'get volume settings'` → `output
// muted:true`), waehrend hier alles offen stand. Behebung: beim Start
// loesen (wie #447 beim gespeicherten Hauptausgang), im Betrieb nur
// melden -- und zwar genau einmal je Flanke.
//
// Alle Faelle laufen gegen eine Attrappe statt gegen CoreAudio
// (SystemOutputMute::setBackendForTest), damit der Prüfstand den
// Ausgang des Rechners, auf dem er laeuft, nicht anfasst.
// =================================================================

#include <QtTest/QtTest>
#include <QPushButton>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/audio/SystemOutputMute.h"
#include "gui/widgets/MasterOutputWidget.h"

using namespace Longpath;

namespace {

// Zustand der Attrappe. Freie Funktionen, weil Backend rohe
// Funktionszeiger nimmt (kein std::function, damit der Weg auch in
// einem Echtzeitpfad brauchbar bleibt).
bool g_stumm = false;
int  g_schreibzaehler = 0;
bool g_schreibenGehtDurch = true;
bool g_unbekannt = false;

SystemOutputMute::State attrappeLesen()
{
    if (g_unbekannt) {
        return SystemOutputMute::Unknown;
    }
    return g_stumm ? SystemOutputMute::Muted : SystemOutputMute::NotMuted;
}

bool attrappeSchreiben(bool on)
{
    ++g_schreibzaehler;
    if (!g_schreibenGehtDurch) {
        return false;
    }
    g_stumm = on;
    return true;
}

void attrappeEinsetzen()
{
    g_stumm = false;
    g_schreibzaehler = 0;
    g_schreibenGehtDurch = true;
    g_unbekannt = false;
    SystemOutputMute::setBackendForTest({ &attrappeLesen, &attrappeSchreiben });
}

}  // namespace

class TstSystemOutputMute : public QObject {
    Q_OBJECT

private slots:

    void init()
    {
        attrappeEinsetzen();
        auto& s = AppSettings::instance();
        s.remove(QStringLiteral("audio/Master/Muted"));
        s.remove(QStringLiteral("audio/Master/Volume"));
    }

    void cleanupTestCase()
    {
        // Echten Weg zurueck, damit ein folgender Prüfstand im selben
        // Prozess nicht auf der Attrappe sitzt.
        SystemOutputMute::setBackendForTest({ nullptr, nullptr });
    }

    // ── 1. Die Attrappe trägt: lesen und schreiben gehen durch ────────
    void attrappeLiestUndSchreibt()
    {
        QCOMPARE(SystemOutputMute::isMuted(), SystemOutputMute::NotMuted);
        QVERIFY(SystemOutputMute::setMuted(true));
        QCOMPARE(SystemOutputMute::isMuted(), SystemOutputMute::Muted);
        QVERIFY(SystemOutputMute::setMuted(false));
        QCOMPARE(SystemOutputMute::isMuted(), SystemOutputMute::NotMuted);
    }

    // ── 2. Beim Start wird ein stummer Systemausgang geloest ──────────
    //
    // Das ist der Fall vom 2026-11-14: der Betreiber startet Longpath,
    // der Mac steht noch stumm von einem F10-Fehlgriff, und niemand
    // sagt es ihm.
    void startLoestDenStummenSystemausgang()
    {
        g_stumm = true;

        MasterOutputWidget w(nullptr);

        QCOMPARE(SystemOutputMute::isMuted(), SystemOutputMute::NotMuted);
        QCOMPARE(g_schreibzaehler, 1);
    }

    // ── 3. Gegenprobe: war der Systemausgang offen, wird nichts
    //       geschrieben ───────────────────────────────────────────────
    //
    // Ohne diesen Fall wuerde ein „schreibt immer false" genauso gruen
    // aussehen wie die Behebung.
    void offenerAusgangWirdNichtAngefasst()
    {
        g_stumm = false;

        MasterOutputWidget w(nullptr);

        QCOMPARE(g_schreibzaehler, 0);
        QCOMPARE(SystemOutputMute::isMuted(), SystemOutputMute::NotMuted);
    }

    // ── 4. Nimmt das System die Umstellung nicht an, bleibt es stumm
    //       -- und der Prüfstand sagt es, statt Erfolg zu melden ──────
    void abgelehnteUmstellungBleibtStumm()
    {
        g_stumm = true;
        g_schreibenGehtDurch = false;

        MasterOutputWidget w(nullptr);

        QCOMPARE(g_schreibzaehler, 1);
        QCOMPARE(SystemOutputMute::isMuted(), SystemOutputMute::Muted);
    }

    // ── 5. Im Betrieb: die Flanke wird genau EINMAL gemeldet ──────────
    //
    // Der Takt laeuft alle zwei Sekunden. Eine Meldung je Takt waere
    // eine Meldungsflut und haette denselben Wert wie keine.
    void flankeMeldetGenauEinmal()
    {
        MasterOutputWidget w(nullptr);
        QSignalSpy spy(&w, &MasterOutputWidget::systemOutputMutedDetected);

        // Noch offen: kein Takt meldet etwas.
        QVERIFY(QMetaObject::invokeMethod(&w, "pruefeSystemStumm"));
        QVERIFY(QMetaObject::invokeMethod(&w, "pruefeSystemStumm"));
        QCOMPARE(spy.count(), 0);

        // Jetzt kippt der Mac (F10).
        g_stumm = true;
        QVERIFY(QMetaObject::invokeMethod(&w, "pruefeSystemStumm"));
        QCOMPARE(spy.count(), 1);

        // Drei weitere Takte, immer noch stumm: keine zweite Meldung.
        QVERIFY(QMetaObject::invokeMethod(&w, "pruefeSystemStumm"));
        QVERIFY(QMetaObject::invokeMethod(&w, "pruefeSystemStumm"));
        QVERIFY(QMetaObject::invokeMethod(&w, "pruefeSystemStumm"));
        QCOMPARE(spy.count(), 1);

        // Wieder offen, dann erneut stumm: zweite Flanke, zweite Meldung.
        g_stumm = false;
        QVERIFY(QMetaObject::invokeMethod(&w, "pruefeSystemStumm"));
        QCOMPARE(spy.count(), 1);
        g_stumm = true;
        QVERIFY(QMetaObject::invokeMethod(&w, "pruefeSystemStumm"));
        QCOMPARE(spy.count(), 2);
    }

    // ── 6. Der Waechter stellt im Betrieb NICHT ──────────────────────
    //
    // „Wer waehrend des Betriebs stumm schaltet, kann es gewollt
    // haben" -- dieselbe Entscheidung wie bei #447 fuer den eigenen
    // Hauptausgang.
    void betriebStelltNicht()
    {
        MasterOutputWidget w(nullptr);
        const int vorher = g_schreibzaehler;

        g_stumm = true;
        QVERIFY(QMetaObject::invokeMethod(&w, "pruefeSystemStumm"));
        QVERIFY(QMetaObject::invokeMethod(&w, "pruefeSystemStumm"));

        QCOMPARE(g_schreibzaehler, vorher);
        QCOMPARE(SystemOutputMute::isMuted(), SystemOutputMute::Muted);
    }

    // ── 7. Unbekannt meldet nichts ───────────────────────────────────
    //
    // Linux und Windows haben keinen Weg dorthin, und eine
    // fehlgeschlagene Abfrage ist keine Aussage über den Zustand. Ein
    // Unknown darf deshalb weder eine Meldung ausloesen noch den
    // zuletzt gesehenen Zustand verwerfen.
    void unbekanntMeldetNichtsUndVergisstNichts()
    {
        MasterOutputWidget w(nullptr);
        QSignalSpy spy(&w, &MasterOutputWidget::systemOutputMutedDetected);

        g_unbekannt = true;
        QVERIFY(QMetaObject::invokeMethod(&w, "pruefeSystemStumm"));
        QVERIFY(QMetaObject::invokeMethod(&w, "pruefeSystemStumm"));
        QCOMPARE(spy.count(), 0);

        // Zurueck auf einen echten Wert, und zwar auf stumm: jetzt ist
        // es eine Flanke und wird gemeldet. Haette das Unknown den
        // gemerkten Zustand auf „stumm" gesetzt, blieb diese Meldung
        // aus.
        g_unbekannt = false;
        g_stumm = true;
        QVERIFY(QMetaObject::invokeMethod(&w, "pruefeSystemStumm"));
        QCOMPARE(spy.count(), 1);
    }

    // ── 8. Beim Start mit Unknown wird nichts geschrieben ────────────
    void startMitUnbekanntSchreibtNichts()
    {
        g_unbekannt = true;

        MasterOutputWidget w(nullptr);

        QCOMPARE(g_schreibzaehler, 0);
    }
};

QTEST_MAIN(TstSystemOutputMute)
#include "tst_system_output_mute.moc"
