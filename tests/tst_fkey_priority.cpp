// =================================================================
// tests/tst_fkey_priority.cpp  (Longpath)
// =================================================================
//
// F1…F12 als F-Tasten, solange Longpath vorne ist (FKeyPriority) --
// gegen eine Attrappe des Systemmodus. Der echte Tastaturmodus des
// Rechners wird hier NIE angefasst.
//
// Geprueft:
//   * umgestellt nur, wenn gewuenscht UND vorne; zurueck, sobald eins fehlt
//   * steht das System schon auf F-Tasten, bleibt es unberuehrt -- auch beim
//     Zurueckstellen
//   * unbekannter Modus (kein Mac): nichts geschrieben
//   * macOS nimmt nicht an: einmal gemeldet, nicht umgestellt, keine Marke
//   * Marke nur waehrend der Umstellung; nach einem Absturz stellt der
//     naechste Start zurueck
//   * der Destruktor stellt zurueck
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>

#include "gui/hotkeys/FKeyPriority.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

using namespace Longpath;

namespace {

// Der Systemmodus als Zahl, dazu ein Zaehler der Schreibzugriffe.
struct System {
    int mode{FKeyPriority::kModeSpecial};
    int writes{0};
    bool refuse{false};

    FKeyPriority::Backend backend()
    {
        return FKeyPriority::Backend{
            [this]() { return mode; },
            [this](int m) {
                ++writes;
                if (refuse) {
                    return false;
                }
                mode = m;
                return true;
            }};
    }
};

} // namespace

class TstFKeyPriority : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString marke() const { return m_dir.filePath(QStringLiteral("fkeys-switched.mark")); }

private slots:

    void init()
    {
        QFile::remove(marke());
    }

    void nurGewuenschtUndVorne()
    {
        System sys;
        FKeyPriority fk(sys.backend(), marke());

        fk.setActive(true);
        QCOMPARE(sys.mode, FKeyPriority::kModeSpecial);   // nicht gewuenscht
        fk.setWanted(true);
        QCOMPARE(sys.mode, FKeyPriority::kModeFKeys);
        QVERIFY(fk.engaged());
        QVERIFY(QFile::exists(marke()));

        // Ein anderes Programm vorne: zurueck.
        fk.setActive(false);
        QCOMPARE(sys.mode, FKeyPriority::kModeSpecial);
        QVERIFY(!fk.engaged());
        QVERIFY(!QFile::exists(marke()));

        // Wieder vorne, dann abgeschaltet.
        fk.setActive(true);
        QCOMPARE(sys.mode, FKeyPriority::kModeFKeys);
        fk.setWanted(false);
        QCOMPARE(sys.mode, FKeyPriority::kModeSpecial);

        // Doppelte Meldungen schreiben nicht doppelt.
        const int vorher = sys.writes;
        fk.setActive(true);
        fk.setActive(true);
        QCOMPARE(sys.writes, vorher);
    }

    // ── Durchhalten, solange Longpath laeuft (2026-11-14) ────────────
    //
    // Betreiber: „Ja, durchgehend". Gegenstueck zu nurGewuenschtUndVorne
    // darueber: dort loest ein Fokuswechsel die Umstellung, hier nicht.
    // Genau diese Luecke hat den Mac zweimal in drei Tagen stumm
    // gemacht -- F10 ist am Mac die Stummtaste.
    void durchhaltenUeberlebtDenFokuswechsel()
    {
        System sys;
        FKeyPriority fk(sys.backend(), marke());

        fk.setWanted(true);
        fk.setActive(true);
        QCOMPARE(sys.mode, FKeyPriority::kModeFKeys);

        // Ohne Durchhalten: ein anderes Programm vorne loest es.
        fk.setActive(false);
        QCOMPARE(sys.mode, FKeyPriority::kModeSpecial);

        // Mit Durchhalten: der Modus steht, OBWOHL Longpath hinten ist.
        fk.setHoldWhileRunning(true);
        QCOMPARE(sys.mode, FKeyPriority::kModeFKeys);
        QVERIFY(fk.engaged());
        QVERIFY(QFile::exists(marke()));

        // Und er bleibt stehen, so oft der Fokus auch wechselt.
        fk.setActive(true);
        fk.setActive(false);
        fk.setActive(true);
        fk.setActive(false);
        QCOMPARE(sys.mode, FKeyPriority::kModeFKeys);
    }

    // Durchhalten hebt den Wunsch NICHT auf: wer die Tastenkuerzel oder
    // „F-Tasten zuerst" abschaltet, bekommt seine Mac-Tasten zurueck,
    // egal wie der Schalter steht.
    void durchhaltenSchlaegtDenWunschNicht()
    {
        System sys;
        FKeyPriority fk(sys.backend(), marke());

        fk.setHoldWhileRunning(true);
        QCOMPARE(sys.mode, FKeyPriority::kModeSpecial);   // noch nicht gewuenscht

        fk.setWanted(true);
        QCOMPARE(sys.mode, FKeyPriority::kModeFKeys);     // auch ohne setActive

        fk.setWanted(false);
        QCOMPARE(sys.mode, FKeyPriority::kModeSpecial);
        QVERIFY(!fk.engaged());
        QVERIFY(!QFile::exists(marke()));
    }

    // Und beim Beenden geht es trotz Durchhalten zurueck -- sonst
    // behielte der Rechner die Umstellung nach jedem Lauf.
    void durchhaltenGibtBeimBeendenZurueck()
    {
        System sys;
        {
            FKeyPriority fk(sys.backend(), marke());
            fk.setHoldWhileRunning(true);
            fk.setWanted(true);
            QCOMPARE(sys.mode, FKeyPriority::kModeFKeys);
            fk.release();
            QCOMPARE(sys.mode, FKeyPriority::kModeSpecial);

            // release() ist kein Abschalten: ein Fokuswechsel stellt
            // wieder her, weil der Wunsch steht.
            fk.setActive(true);
            QCOMPARE(sys.mode, FKeyPriority::kModeFKeys);
        }
        // Destruktor.
        QCOMPARE(sys.mode, FKeyPriority::kModeSpecial);
        QVERIFY(!QFile::exists(marke()));
    }

    void systemSchonAufFTastenBleibtUnberuehrt()
    {
        System sys;
        sys.mode = FKeyPriority::kModeFKeys;
        {
            FKeyPriority fk(sys.backend(), marke());
            fk.setWanted(true);
            fk.setActive(true);
            QVERIFY(!fk.engaged());
            fk.setActive(false);
            fk.release();
        }
        QCOMPARE(sys.writes, 0);
        QCOMPARE(sys.mode, FKeyPriority::kModeFKeys);
        QVERIFY(!QFile::exists(marke()));
    }

    void unbekannterModusSchreibtNichts()
    {
        int writes = 0;
        FKeyPriority fk(FKeyPriority::Backend{[]() { return -1; },
                                              [&writes](int) { ++writes; return true; }},
                        marke());
        fk.setWanted(true);
        fk.setActive(true);
        QVERIFY(!fk.engaged());
        QCOMPARE(writes, 0);

        // Ganz ohne Weg (Linux, Windows, Pruefstand) ebenso.
        FKeyPriority leer(FKeyPriority::Backend{}, marke());
        leer.setWanted(true);
        leer.setActive(true);
        QVERIFY(!leer.engaged());
    }

    void abgelehntWirdEinmalGemeldet()
    {
        System sys;
        sys.refuse = true;
        FKeyPriority fk(sys.backend(), marke());
        QSignalSpy spy(&fk, &FKeyPriority::failed);
        fk.setWanted(true);
        fk.setActive(true);
        QVERIFY(!fk.engaged());
        QCOMPARE(spy.count(), 1);
        QVERIFY(!QFile::exists(marke()));

        fk.setActive(false);
        fk.setActive(true);
        QCOMPARE(spy.count(), 1);   // nicht bei jedem Fensterwechsel
        QCOMPARE(sys.mode, FKeyPriority::kModeSpecial);
    }

    void markeHebtAbsturzAuf()
    {
        System sys;
        // Absturz mitten in der Umstellung: Marke liegt, System steht auf
        // F-Tasten, kein Destruktor lief.
        {
            QFile f(marke());
            QVERIFY(f.open(QIODevice::WriteOnly));
        }
        sys.mode = FKeyPriority::kModeFKeys;

        FKeyPriority fk(sys.backend(), marke());
        QCOMPARE(sys.mode, FKeyPriority::kModeSpecial);
        QVERIFY(!QFile::exists(marke()));
        QVERIFY(!fk.engaged());
    }

    void markeOhneUmstellungLaesstDenModus()
    {
        // Marke liegt, das System steht aber schon wieder auf
        // Sonderfunktionen: nichts zu schreiben.
        System sys;
        {
            QFile f(marke());
            QVERIFY(f.open(QIODevice::WriteOnly));
        }
        FKeyPriority fk(sys.backend(), marke());
        QCOMPARE(sys.writes, 0);
        QVERIFY(!QFile::exists(marke()));
    }

    void destruktorStelltZurueck()
    {
        System sys;
        {
            FKeyPriority fk(sys.backend(), marke());
            fk.setWanted(true);
            fk.setActive(true);
            QCOMPARE(sys.mode, FKeyPriority::kModeFKeys);
        }
        QCOMPARE(sys.mode, FKeyPriority::kModeSpecial);
        QVERIFY(!QFile::exists(marke()));
    }

    void attrappeNachtraeglichWirktSofort()
    {
        // So setzt der Fensterpruefstand die Attrappe ein: Longpath ist
        // laengst vorne und gewuenscht, der leere Weg hat nichts getan.
        FKeyPriority fk(FKeyPriority::Backend{}, QString());
        fk.setWanted(true);
        fk.setActive(true);
        System sys;
        fk.setBackendForTest(sys.backend());
        QCOMPARE(sys.mode, FKeyPriority::kModeFKeys);
        QVERIFY(fk.engaged());
    }
};

QTEST_GUILESS_MAIN(TstFKeyPriority)
#include "tst_fkey_priority.moc"
