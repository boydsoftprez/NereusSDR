// tests/tst_layout_profiles.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
//
// ── Mehrere Ansichten ────────────────────────────────────────────────
//
// OE5SOS, 2026-08-15: „ein zweites Profil oder auch ein drittes
// anlegen, welches ich mir selbst wieder individuell gestalten kann" —
// „sodass man je nach Band oder je nach Modus andere Ansichten hat".
//
// Drei Arten, wie so etwas kaputtgeht, und alle drei kosten Arbeit des
// Betreibers, nicht Rechenzeit:
//
//   · Umschalten verwirft die Umgestaltung. Wer eine halbe Stunde sein
//     CW-Fenster gebaut hat und dann auf „SSB" klickt, hat sie
//     weggeklickt — ohne Rueckgaengig.
//
//   · Loeschen laesst das Fenster herrenlos zurueck. Kein aktives
//     Profil heisst: die naechste Aenderung landet nirgends.
//
//   · Die Bindung raet. Bei einem Fenster, in dem man sendet, ist eine
//     vorhersagbare Regel mehr wert als eine kluge.

#include <QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "gui/LayoutProfiles.h"

using namespace Longpath;

class TestLayoutProfiles : public QObject
{
    Q_OBJECT

private:
    // Ein Zustand, den der Test von aussen aendern kann — er steht fuer
    // „so sieht das Fenster gerade aus".
    QVariantMap m_live;

    void hook(LayoutProfiles& lp)
    {
        lp.setHooks([this]() { return m_live; },
                    [this](const QVariantMap& s) { m_live = s; });
    }

private slots:
    void init() { m_live.clear(); }

    void aFreshManagerHasNothing()
    {
        LayoutProfiles lp;
        QVERIFY(lp.names().isEmpty());
        QVERIFY(lp.current().isEmpty());
    }

    void creatingTakesASnapshotOfNow()
    {
        LayoutProfiles lp;
        hook(lp);
        m_live.insert(QStringLiteral("panel"), QStringLiteral("smeter"));

        QVERIFY(lp.create(QStringLiteral("SSB")));
        QCOMPARE(lp.names(), QStringList({QStringLiteral("SSB")}));
        QCOMPARE(lp.snapshot(QStringLiteral("SSB"))
                     .value(QStringLiteral("panel")).toString(),
                 QStringLiteral("smeter"));
        // Das erste wird auch das aktive — sonst schreibt
        // captureIntoCurrent() ins Leere.
        QCOMPARE(lp.current(), QStringLiteral("SSB"));
    }

    void namesMustBeUniqueAndNotEmpty()
    {
        LayoutProfiles lp;
        hook(lp);
        QVERIFY(lp.create(QStringLiteral("SSB")));
        QVERIFY(!lp.create(QStringLiteral("SSB")));
        QVERIFY(!lp.create(QStringLiteral("   ")));
        QVERIFY(!lp.create(QString{}));
        QCOMPARE(lp.names().size(), 1);
    }

    // ── Der teuerste Fehler ──────────────────────────────────────────
    void switchingKeepsWhatWasBuilt()
    {
        LayoutProfiles lp;
        hook(lp);

        // So laeuft es im Betrieb: SSB-Fenster bauen, Profil anlegen,
        // dann ein zweites anlegen (Kopie des ersten) und DAS umbauen.
        m_live.insert(QStringLiteral("v"), QStringLiteral("ssb-aufbau"));
        QVERIFY(lp.create(QStringLiteral("SSB")));

        QVERIFY(lp.create(QStringLiteral("CW")));
        QCOMPARE(lp.current(), QStringLiteral("CW"));

        // Jetzt umbauen — ohne zu speichern, so wie es im Betrieb
        // passiert: man schiebt Panels und klickt dann woanders hin.
        m_live.insert(QStringLiteral("v"), QStringLiteral("cw-umgebaut"));
        QVERIFY(lp.activate(QStringLiteral("SSB")));

        QCOMPARE(m_live.value(QStringLiteral("v")).toString(),
                 QStringLiteral("ssb-aufbau"));
        QVERIFY2(lp.snapshot(QStringLiteral("CW"))
                     .value(QStringLiteral("v")).toString()
                     == QStringLiteral("cw-umgebaut"),
                 "der Umbau ging beim Umschalten verloren");
    }

    void activatingAppliesTheSnapshot()
    {
        LayoutProfiles lp;
        hook(lp);
        m_live.insert(QStringLiteral("v"), QStringLiteral("a"));
        QVERIFY(lp.create(QStringLiteral("A")));
        QVERIFY(lp.create(QStringLiteral("B")));   // Kopie, jetzt aktiv
        m_live.insert(QStringLiteral("v"), QStringLiteral("b"));

        QVERIFY(lp.activate(QStringLiteral("A")));
        QCOMPARE(m_live.value(QStringLiteral("v")).toString(),
                 QStringLiteral("a"));
    }

    // ── Warum create() mitwechselt ───────────────────────────────────
    //
    // Legte man ein Profil an, ohne hinzuwechseln, behauptete das alte
    // weiter, das Fenster gehoere ihm — und das naechste Umschalten
    // schriebe den Aufbau des neuen in das alte. Vier Tests fielen
    // darueber, bevor create() zu „Speichern unter" wurde.
    void creatingSwitchesAndLeavesTheOldOneIntact()
    {
        LayoutProfiles lp;
        hook(lp);
        m_live.insert(QStringLiteral("v"), QStringLiteral("a"));
        QVERIFY(lp.create(QStringLiteral("A")));
        QVERIFY(lp.create(QStringLiteral("B")));
        QCOMPARE(lp.current(), QStringLiteral("B"));

        m_live.insert(QStringLiteral("v"), QStringLiteral("b"));
        QVERIFY(lp.activate(QStringLiteral("A")));
        QVERIFY2(lp.snapshot(QStringLiteral("A"))
                     .value(QStringLiteral("v")).toString()
                     == QStringLiteral("a"),
                 "A wurde vom Umbau an B ueberschrieben");
    }

    void activatingWhatIsAlreadyActiveIsHarmless()
    {
        LayoutProfiles lp;
        hook(lp);
        QVERIFY(lp.create(QStringLiteral("A")));
        QVERIFY(lp.activate(QStringLiteral("A")));
        QCOMPARE(lp.current(), QStringLiteral("A"));
    }

    void activatingSomethingUnknownFails()
    {
        LayoutProfiles lp;
        hook(lp);
        QVERIFY(lp.create(QStringLiteral("A")));
        QVERIFY(!lp.activate(QStringLiteral("GibtsNicht")));
        QCOMPARE(lp.current(), QStringLiteral("A"));
    }

    // ── Das Plus legt LEER an ────────────────────────────────────────
    //
    // OE5SOS, 2026-08-15: „Wenn ich links ein neues Profil oeffne,
    // sollte dieses leer sein." Ein Plus, das eine Kopie anlegt, waere
    // ein Duplizieren-Knopf mit falscher Beschriftung — und Duplizieren
    // gibt es schon.
    void createWithTakesTheGivenStateNotTheCurrentOne()
    {
        LayoutProfiles lp;
        hook(lp);
        m_live.insert(QStringLiteral("v"), QStringLiteral("voll"));
        QVERIFY(lp.create(QStringLiteral("Standard")));

        QVariantMap leer;
        leer.insert(QStringLiteral("v"), QString{});
        QVERIFY(lp.createWith(QStringLiteral("Leer"), leer));

        QCOMPARE(lp.current(), QStringLiteral("Leer"));
        QVERIFY2(m_live.value(QStringLiteral("v")).toString().isEmpty(),
                 "der leere Zustand wurde nicht hergestellt");
        QVERIFY2(lp.snapshot(QStringLiteral("Standard"))
                     .value(QStringLiteral("v")).toString()
                     == QStringLiteral("voll"),
                 "das bisherige Profil hat seinen Aufbau verloren");
    }

    void createWithRefusesTheSameNamesAsCreate()
    {
        LayoutProfiles lp;
        hook(lp);
        QVERIFY(lp.create(QStringLiteral("A")));
        QVERIFY(!lp.createWith(QStringLiteral("A"), QVariantMap{}));
        QVERIFY(!lp.createWith(QStringLiteral("   "), QVariantMap{}));
        QCOMPARE(lp.names().size(), 1);
    }

    // ── Der uebliche Weg zum zweiten Profil ──────────────────────────
    void duplicatingCopiesTheLayoutButNotTheBindings()
    {
        LayoutProfiles lp;
        hook(lp);
        m_live.insert(QStringLiteral("v"), QStringLiteral("aufbau"));
        QVERIFY(lp.create(QStringLiteral("CW")));
        lp.bindMode(QStringLiteral("CW"), DSPMode::CWU, true);

        QVERIFY(lp.duplicate(QStringLiteral("CW"), QStringLiteral("CW-Contest")));
        QCOMPARE(lp.snapshot(QStringLiteral("CW-Contest"))
                     .value(QStringLiteral("v")).toString(),
                 QStringLiteral("aufbau"));
        // Zwei Profile auf derselben Bindung waeren ein Konflikt, den
        // niemand angelegt hat.
        QVERIFY2(!lp.isBoundToMode(QStringLiteral("CW-Contest"), DSPMode::CWU),
                 "die Kopie hat die Bindung mitgenommen");
    }

    void duplicateNeedsAFreeName()
    {
        LayoutProfiles lp;
        hook(lp);
        QVERIFY(lp.create(QStringLiteral("A")));
        QVERIFY(lp.create(QStringLiteral("B")));
        QVERIFY(!lp.duplicate(QStringLiteral("A"), QStringLiteral("B")));
        QVERIFY(!lp.duplicate(QStringLiteral("GibtsNicht"),
                              QStringLiteral("C")));
    }

    void renamingKeepsTheOrderAndTheActiveOne()
    {
        LayoutProfiles lp;
        hook(lp);
        QVERIFY(lp.create(QStringLiteral("A")));
        QVERIFY(lp.create(QStringLiteral("B")));
        QVERIFY(lp.activate(QStringLiteral("A")));
        QVERIFY(lp.rename(QStringLiteral("A"), QStringLiteral("Alpha")));
        QCOMPARE(lp.names(), QStringList({QStringLiteral("Alpha"),
                                          QStringLiteral("B")}));
        QCOMPARE(lp.current(), QStringLiteral("Alpha"));
    }

    // ── Das herrenlose Fenster ───────────────────────────────────────
    void removingTheActiveOneMovesToAnother()
    {
        LayoutProfiles lp;
        hook(lp);
        m_live.insert(QStringLiteral("v"), QStringLiteral("a"));
        QVERIFY(lp.create(QStringLiteral("A")));
        QVERIFY(lp.create(QStringLiteral("B")));   // Kopie, jetzt aktiv
        m_live.insert(QStringLiteral("v"), QStringLiteral("b"));

        QVERIFY(lp.remove(QStringLiteral("B")));
        QCOMPARE(lp.current(), QStringLiteral("A"));
        QVERIFY2(m_live.value(QStringLiteral("v")).toString()
                     == QStringLiteral("a"),
                 "nach dem Loeschen wurde der Aufbau des neuen aktiven "
                 "Profils nicht hergestellt");
    }

    void removingTheLastOneLeavesNothingActive()
    {
        LayoutProfiles lp;
        hook(lp);
        QVERIFY(lp.create(QStringLiteral("A")));
        QVERIFY(lp.remove(QStringLiteral("A")));
        QVERIFY(lp.names().isEmpty());
        QVERIFY(lp.current().isEmpty());
    }

    // ── Bindung ──────────────────────────────────────────────────────
    void anUnboundProfileNeverClaimsABandChange()
    {
        LayoutProfiles lp;
        hook(lp);
        QVERIFY(lp.create(QStringLiteral("Standard")));
        QVERIFY2(lp.profileFor(Band::Band40m, DSPMode::LSB).isEmpty(),
                 "ein Profil ohne Bindung hat den Bandwechsel an sich "
                 "gerissen");
    }

    void bandAndModeTogether()
    {
        LayoutProfiles lp;
        hook(lp);
        QVERIFY(lp.create(QStringLiteral("CW-40")));
        lp.bindBand(QStringLiteral("CW-40"), Band::Band40m, true);
        lp.bindMode(QStringLiteral("CW-40"), DSPMode::CWU, true);

        QCOMPARE(lp.profileFor(Band::Band40m, DSPMode::CWU),
                 QStringLiteral("CW-40"));
        QVERIFY(lp.profileFor(Band::Band20m, DSPMode::CWU).isEmpty());
        QVERIFY(lp.profileFor(Band::Band40m, DSPMode::LSB).isEmpty());
    }

    void anEmptySideMeansDontCare()
    {
        // Nur Modus gebunden, kein Band: gilt auf jedem Band.
        LayoutProfiles lp;
        hook(lp);
        QVERIFY(lp.create(QStringLiteral("CW")));
        lp.bindMode(QStringLiteral("CW"), DSPMode::CWU, true);
        QCOMPARE(lp.profileFor(Band::Band40m, DSPMode::CWU),
                 QStringLiteral("CW"));
        QCOMPARE(lp.profileFor(Band::Band10m, DSPMode::CWU),
                 QStringLiteral("CW"));
    }

    void theFirstMatchWinsAndThatIsOnPurpose()
    {
        LayoutProfiles lp;
        hook(lp);
        QVERIFY(lp.create(QStringLiteral("Erst")));
        QVERIFY(lp.create(QStringLiteral("Dann")));
        lp.bindMode(QStringLiteral("Erst"), DSPMode::CWU, true);
        lp.bindMode(QStringLiteral("Dann"), DSPMode::CWU, true);
        lp.bindBand(QStringLiteral("Dann"), Band::Band40m, true);

        // „Dann" passt genauer. Es gewinnt trotzdem nicht — die Regel
        // ist die Reihenfolge, und die kann man im Kopf nachvollziehen.
        QCOMPARE(lp.profileFor(Band::Band40m, DSPMode::CWU),
                 QStringLiteral("Erst"));
    }

    void bindingCanBeTakenBack()
    {
        LayoutProfiles lp;
        hook(lp);
        QVERIFY(lp.create(QStringLiteral("CW")));
        lp.bindMode(QStringLiteral("CW"), DSPMode::CWU, true);
        QVERIFY(lp.isBoundToMode(QStringLiteral("CW"), DSPMode::CWU));
        lp.bindMode(QStringLiteral("CW"), DSPMode::CWU, false);
        QVERIFY(!lp.isBoundToMode(QStringLiteral("CW"), DSPMode::CWU));
        QVERIFY(lp.profileFor(Band::Band40m, DSPMode::CWU).isEmpty());
    }

    // ── Platte ───────────────────────────────────────────────────────
    void aRoundTripKeepsEverything()
    {
        {
            LayoutProfiles lp;
            hook(lp);
            m_live.insert(QStringLiteral("v"), QStringLiteral("ssb"));
            QVERIFY(lp.create(QStringLiteral("SSB")));
            QVERIFY(lp.create(QStringLiteral("CW")));
            m_live.insert(QStringLiteral("v"), QStringLiteral("cw"));
            lp.bindMode(QStringLiteral("CW"), DSPMode::CWU, true);
            lp.bindBand(QStringLiteral("CW"), Band::Band40m, true);
            lp.captureIntoCurrent();
            lp.save();
        }

        LayoutProfiles again;
        hook(again);
        again.load();
        QCOMPARE(again.names(), QStringList({QStringLiteral("SSB"),
                                             QStringLiteral("CW")}));
        QCOMPARE(again.current(), QStringLiteral("CW"));
        QCOMPARE(again.snapshot(QStringLiteral("SSB"))
                     .value(QStringLiteral("v")).toString(),
                 QStringLiteral("ssb"));
        QCOMPARE(again.snapshot(QStringLiteral("CW"))
                     .value(QStringLiteral("v")).toString(),
                 QStringLiteral("cw"));
        QVERIFY(again.isBoundToMode(QStringLiteral("CW"), DSPMode::CWU));
        QVERIFY(again.isBoundToBand(QStringLiteral("CW"), Band::Band40m));
    }

    // Betreiber 2026-09-27: „beim start von longpath muss immer das
    // standardprofil zu sehen sein" -- Profil 1, nicht das zuletzt
    // aktive. Und das zuletzt aktive bleibt dabei unangetastet.
    void theStartIsAlwaysProfileOne()
    {
        {
            LayoutProfiles lp;
            hook(lp);
            m_live.insert(QStringLiteral("v"), QStringLiteral("standard"));
            QVERIFY(lp.create(QStringLiteral("Neu")));
            QVERIFY(lp.create(QStringLiteral("S")));
            m_live.insert(QStringLiteral("v"), QStringLiteral("schlicht"));
            lp.captureIntoCurrent();
            lp.save();
        }

        LayoutProfiles again;
        hook(again);
        again.load();
        QCOMPARE(again.current(), QStringLiteral("S"));
        QSignalSpy changed(&again, &LayoutProfiles::currentChanged);
        m_live.insert(QStringLiteral("v"), QStringLiteral("leeres-fenster"));

        again.startWithFirst();

        QCOMPARE(again.current(), QStringLiteral("Neu"));
        QCOMPARE(changed.count(), 1);
        QCOMPARE(again.snapshot(QStringLiteral("S"))
                     .value(QStringLiteral("v")).toString(),
                 QStringLiteral("schlicht"));
        again.applyCurrent();
        QCOMPARE(m_live.value(QStringLiteral("v")).toString(),
                 QStringLiteral("standard"));

        // Schon Profil 1: nichts passiert. (applyCurrent() hat selbst
        // noch einmal currentChanged gemeldet -- ab hier zaehlen.)
        const int before = changed.count();
        again.startWithFirst();
        QCOMPARE(changed.count(), before);
    }

    // 2026-11-06, Betreiber: "Zuletzt aktives". Was beim Beenden aktiv
    // war, ist beim Start aktiv; fehlt es, gilt Profil 1.
    void theStartIsTheLastActiveOne()
    {
        {
            LayoutProfiles lp;
            hook(lp);
            m_live.insert(QStringLiteral("v"), QStringLiteral("standard"));
            QVERIFY(lp.create(QStringLiteral("Neu")));
            QVERIFY(lp.create(QStringLiteral("C")));
            m_live.insert(QStringLiteral("v"), QStringLiteral("contest"));
            lp.captureIntoCurrent();
            lp.save();
        }

        LayoutProfiles again;
        hook(again);
        again.load();
        QSignalSpy changed(&again, &LayoutProfiles::currentChanged);
        again.startWithLastActive();
        QCOMPARE(again.current(), QStringLiteral("C"));
        QCOMPARE(changed.count(), 0);
        again.applyCurrent();
        QCOMPARE(m_live.value(QStringLiteral("v")).toString(), QStringLiteral("contest"));

        // Das zuletzt aktive gibt es nicht mehr: Profil 1.
        QVERIFY(again.remove(QStringLiteral("C")));
        LayoutProfiles leer;
        hook(leer);
        leer.startWithLastActive();
        QVERIFY(leer.current().isEmpty());   // ohne Profile nichts
    }

    void abrokenFileLeavesTheProfilesAlone()
    {
        // Dieselbe Regel wie bei der Theme-Datei: eine beschaedigte
        // Datei darf nicht dazu fuehren, dass beim naechsten Speichern
        // die heile Fassung ueberschrieben wird.
        AppSettings::instance().setValue(LayoutProfiles::settingsKey(),
                                         QStringLiteral("{ kaputt"));
        LayoutProfiles lp;
        hook(lp);
        QVERIFY(lp.create(QStringLiteral("Behalten")));
        lp.load();
        QCOMPARE(lp.names(), QStringList({QStringLiteral("Behalten")}));
    }

    // ── Die Bindung an FT8/FT4 (2026-10-30) ───────────────────────
    //
    // `DSPMode` reicht nicht: FT8, FT4 und RTTY sind alle DIGU.
    // Betreiber: „und wenn bei ft8 klick automatisch ein eigenes
    // profil erscheint?"
    void einProfilLaesstSichAnFt8Binden()
    {
        LayoutProfiles lp;
        lp.create(QStringLiteral("FT8"));
        lp.create(QStringLiteral("SSB"));

        lp.bindDigi(QStringLiteral("FT8"),
                    DigiFrequenzen::Digimodus::Ft8, true);
        QVERIFY(lp.isBoundToDigi(QStringLiteral("FT8"),
                                 DigiFrequenzen::Digimodus::Ft8));
        QVERIFY(!lp.isBoundToDigi(QStringLiteral("FT8"),
                                  DigiFrequenzen::Digimodus::Ft4));

        // Mit FT8 gefragt: es passt.
        QCOMPARE(lp.profileFor(Band::Band40m, DSPMode::DIGU,
                               DigiFrequenzen::Digimodus::Ft8),
                 QStringLiteral("FT8"));

        // ── Der Kern: eine Digi-Bindung ist STRENG ────────────
        //
        // Ohne Digi-Angabe passt es NICHT. Sonst risse das
        // FT8-Profil jeden Wechsel nach DIGU an sich -- auch den
        // nach RTTY, das dieselbe Betriebsart ist und ein ganz
        // anderes Fenster braucht.
        QVERIFY2(lp.profileFor(Band::Band40m, DSPMode::DIGU).isEmpty(),
                 "ohne Digi-Angabe darf ein FT8-Profil nicht greifen");
        QVERIFY2(lp.profileFor(Band::Band40m, DSPMode::DIGU,
                               DigiFrequenzen::Digimodus::Ft4).isEmpty(),
                 "ein FT8-Profil darf nicht bei FT4 greifen");
    }

    // Die Bindung ueberlebt Speichern und Laden -- sonst waere sie
    // beim naechsten Start weg, und das faellt erst auf, wenn man
    // schon umgeschaltet hat.
    void dieDigiBindungUeberlebtDenNeustart()
    {
        {
            LayoutProfiles lp;
            lp.create(QStringLiteral("FT4-Fenster"));
            lp.bindDigi(QStringLiteral("FT4-Fenster"),
                        DigiFrequenzen::Digimodus::Ft4, true);
            lp.save();
        }
        LayoutProfiles wieder;
        wieder.load();
        QVERIFY(wieder.isBoundToDigi(QStringLiteral("FT4-Fenster"),
                                     DigiFrequenzen::Digimodus::Ft4));
        QCOMPARE(wieder.profileFor(Band::Band40m, DSPMode::DIGU,
                                   DigiFrequenzen::Digimodus::Ft4),
                 QStringLiteral("FT4-Fenster"));
    }

    // ── Die Bindung ueberlebt eine fremde Fassung (2026-10-30) ───
    //
    // Der Profilsatz wird als EIN JSON geschrieben: wer ihn
    // speichert, schreibt alle Felder, die er kennt -- und nur die.
    // Eine Fassung ohne `digis` loescht es beim ersten Speichern,
    // stillschweigend.
    //
    // Am 2026-10-30 genau so passiert: der Auto-Install hatte dem
    // Betreiber `main` untergeschoben, main kannte das Feld nicht,
    // und danach stand in allen Profilen `digis=None`. Er klickte
    // zweimal FT8 -- „es oeffnet nur den decoder, nicht das profil".
    void dieDigiBindungUeberlebtEinenFremdenSpeichervorgang()
    {
        {
            LayoutProfiles lp;
            lp.create(QStringLiteral("FT8"));
            lp.bindDigi(QStringLiteral("FT8"),
                        DigiFrequenzen::Digimodus::Ft8, true);
            lp.save();
        }

        // Jetzt schreibt eine FREMDE Fassung den Satz zurueck --
        // nachgestellt, indem „digis" aus dem JSON entfernt wird.
        auto& s = AppSettings::instance();
        const QString schluessel = QStringLiteral("LayoutProfiles");
        QString roh = s.value(schluessel, QString()).toString();
        QVERIFY(!roh.isEmpty());
        QVERIFY2(roh.contains(QStringLiteral("digis")),
                 "ohne digis im Satz prueft dieser Lauf nichts");
        roh.replace(QStringLiteral("\"digis\":[0]"),
                    QStringLiteral("\"digis\":[]"));
        s.setValue(schluessel, roh);

        // Und trotzdem ist sie da.
        LayoutProfiles wieder;
        wieder.load();
        QVERIFY2(wieder.isBoundToDigi(QStringLiteral("FT8"),
                                      DigiFrequenzen::Digimodus::Ft8),
                 "die Bindung hat den fremden Speichervorgang nicht "
                 "ueberlebt");
    }

    // Ein Profil ohne jede Bindung bleibt, was es war: eines, das
    // nie von selbst greift.
    void ohneBindungGreiftNichts()
    {
        LayoutProfiles lp;
        lp.create(QStringLiteral("Standard"));
        QVERIFY(lp.profileFor(Band::Band40m, DSPMode::DIGU,
                              DigiFrequenzen::Digimodus::Ft8).isEmpty());
    }

    // ── Der Weg zurueck: Profil -> Betriebsart (2026-11-15) ──────
    //
    // Betreiber: „weiters muss auch im ft8 profil - F - immer
    // automatisch DIGI mode sein ... derzeit muss ich immer im profil
    // auf FT8 zb klicken."
    //
    // Dieselbe Bindung, andersherum gelesen. „Eindeutig" ist die
    // ganze Regel: eine Bindung ist eine Ansage, keine ist keine,
    // zwei sind keine Entscheidung.
    void dasProfilNenntGenauEINEBetriebsart()
    {
        LayoutProfiles lp;
        lp.create(QStringLiteral("CW"));
        lp.bindMode(QStringLiteral("CW"), DSPMode::CWU, true);

        const auto eine = lp.eindeutigeBetriebsart(QStringLiteral("CW"));
        QVERIFY(eine.has_value());
        QCOMPARE(*eine, DSPMode::CWU);

        // Eine zweite Bindung macht daraus eine Frage, keine Ansage.
        lp.bindMode(QStringLiteral("CW"), DSPMode::LSB, true);
        QVERIFY(!lp.eindeutigeBetriebsart(QStringLiteral("CW")).has_value());

        // Zurueckgenommen: wieder eindeutig.
        lp.bindMode(QStringLiteral("CW"), DSPMode::LSB, false);
        QCOMPARE(*lp.eindeutigeBetriebsart(QStringLiteral("CW")),
                 DSPMode::CWU);
    }

    void ohneBindungNenntDasProfilKeineBetriebsart()
    {
        LayoutProfiles lp;
        lp.create(QStringLiteral("Neu"));
        QVERIFY(!lp.eindeutigeBetriebsart(QStringLiteral("Neu")).has_value());
        QVERIFY(!lp.eindeutigerDigimodus(QStringLiteral("Neu")).has_value());

        // Und ein Name, den es nicht gibt, nennt auch nichts -- statt
        // zu raten oder umzufallen.
        QVERIFY(!lp.eindeutigeBetriebsart(QStringLiteral("gibtsnicht"))
                     .has_value());
        QVERIFY(!lp.eindeutigerDigimodus(QStringLiteral("gibtsnicht"))
                     .has_value());
    }

    // Genau der Fall des Betreibers: sein Profil „FT8" traegt
    // `digis:[0]` (Einstellungsdatei vom 15.11., am 2026-10-30 von ihm
    // selbst angelegt).
    void dasFt8ProfilNenntFt8()
    {
        LayoutProfiles lp;
        lp.create(QStringLiteral("FT8"));
        lp.bindDigi(QStringLiteral("FT8"),
                    DigiFrequenzen::Digimodus::Ft8, true);

        const auto d = lp.eindeutigerDigimodus(QStringLiteral("FT8"));
        QVERIFY(d.has_value());
        QCOMPARE(*d, DigiFrequenzen::Digimodus::Ft8);

        // FT8 UND FT4 gebunden: welches denn? Keines.
        lp.bindDigi(QStringLiteral("FT8"),
                    DigiFrequenzen::Digimodus::Ft4, true);
        QVERIFY(!lp.eindeutigerDigimodus(QStringLiteral("FT8")).has_value());
    }

    // Die Ansage muss den Neustart ueberleben, sonst stellt das Profil
    // die Betriebsart nur in der Sitzung, in der man die Bindung
    // gesetzt hat.
    void dieAnsageUeberlebtDenNeustart()
    {
        {
            LayoutProfiles lp;
            lp.create(QStringLiteral("FT8"));
            lp.bindDigi(QStringLiteral("FT8"),
                        DigiFrequenzen::Digimodus::Ft8, true);
            lp.save();
        }
        LayoutProfiles wieder;
        wieder.load();
        const auto d = wieder.eindeutigerDigimodus(QStringLiteral("FT8"));
        QVERIFY2(d.has_value(),
                 "nach dem Neustart nennt das Profil keine Betriebsart mehr");
        QCOMPARE(*d, DigiFrequenzen::Digimodus::Ft8);
    }
};

QTEST_GUILESS_MAIN(TestLayoutProfiles)
#include "tst_layout_profiles.moc"
