// tests/tst_command_bar.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
//
// ── Die Kopfleiste ───────────────────────────────────────────────────
//
// Ob sie huebsch ist, kann kein Test sagen. Drei Eigenschaften kann er,
// und an genau diesen scheitert so eine Leiste in der Praxis:
//
//   · Genau EINE Pille je Gruppe leuchtet. Zwei sind ein Widerspruch,
//     null heisst, dass der eingestellte Zustand nirgends steht.
//
//   · Sie folgt dem Modell, nicht dem Klick. Ein Knopf, der angeht,
//     egal was das Modell darunter macht, ist eine Luege mit
//     Rueckmeldung.
//
//   · Beim Wechsel der Empfangskette laesst sie die alte los. Sonst
//     haengt sie an beiden und meldet abwechselnd deren Zustand — ein
//     Fehler, der erst beim zweiten Pan auffaellt und dann wie ein
//     Wackelkontakt aussieht.

#include <QtTest>

#include <cmath>

#include "MenueFaenger.h"

#include "gui/widgets/CommandBar.h"
#include "core/AppSettings.h"
#include "models/FilterPresetStore.h"
#include "models/SliceModel.h"
#include "models/RadioModel.h"
#include "gui/widgets/DspParamPopup.h"
#include "gui/widgets/DspQuickPopups.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpression>
#include <QSlider>
#include <QTimer>
#include "FensterFrist.h"

using namespace Longpath;

class TestCommandBar : public QObject
{
    Q_OBJECT

private slots:
    // ════════════════════════════════════════════════════════════════
    // Die Leiste als Reihe lesbar machen (2026-10-07)
    //
    // Betreiber zum Bild der Leiste: „die leiste oben passt aber noch
    // immer nicht", und nach fuenf Entwuerfen: „suche das, welches am
    // besten harmonisch dazu passt". Gewaehlt wurden drei Dinge, die
    // alle dem folgen, was im Fenster schon gilt. Hier steht je eine
    // Pruefung dafuer.
    // ════════════════════════════════════════════════════════════════

    /// A — zwischen den Gruppen steht ein Strich. Am BILD gemessen,
    /// nicht an der Absicht: der Strich wird gezeichnet, nicht gebaut,
    /// also kann kein Bauteil ihn bezeugen.
    void zwischenDenGruppenStehtEinStrich()
    {
        CommandBar bar;
        bar.resize(1400, 70);
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar, kFensterFrist));
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        const QImage bild = bar.grab().toImage();
        auto* row = bar.findChild<QHBoxLayout*>();
        QVERIFY(row);

        QVector<QRect> kaesten;
        for (int i = 0; i < row->count(); ++i) {
            QLayoutItem* it = row->itemAt(i);
            if (it && it->layout() && it->layout()->geometry().isValid()) {
                kaesten.append(it->layout()->geometry());
            }
        }
        QVERIFY2(kaesten.size() >= 2, "weniger als zwei Gruppen in der Leiste");

        // ── Bildpunkte sind nicht Layoutpunkte (2026-10-15) ─────────
        //
        // `grab()` liefert auf einem Retina-Schirm ein Bild mit der
        // doppelten Kantenlaenge (hier gemessen: Leiste 1400x70,
        // Bild 2800x152), die Kaesten aus dem Layout zaehlen aber in
        // Layoutpunkten. Ohne Umrechnung sucht dieser Lauf an den
        // falschen Stellen: er fand 2 von 5 Strichen und fiel auf
        // JEDEM Mac um, waehrend er in der CI (1x-Schirm) gruen war.
        const qreal dpr = bild.devicePixelRatio();
        const auto zuBild = [dpr](int v) {
            return static_cast<int>(std::lround(v * dpr));
        };

        const QRgb grund = bild.pixel(2, 2);
        int striche = 0;
        int luecken = 0;
        for (int i = 0; i + 1 < kaesten.size(); ++i) {
            // Nur die Luecke absuchen, nicht die Gruppen selbst.
            const int von  = zuBild(kaesten.at(i).right() + 2);
            const int bis  = zuBild(kaesten.at(i + 1).left() - 2);
            if (von > bis) { continue; }        // kein Platz, kein Strich
            ++luecken;
            const int mitteY = zuBild(kaesten.at(i).center().y());
            bool gefunden = false;
            for (int x = von; x <= bis && x < bild.width(); ++x) {
                if (x < 0) { continue; }
                if (bild.pixel(x, mitteY) != grund) { gefunden = true; break; }
            }
            if (gefunden) { ++striche; }
        }
        QVERIFY2(luecken >= 2, "weniger als zwei Luecken zwischen den Gruppen");
        QCOMPARE(striche, luecken);
    }

    /// B — hinter jeder Gruppe steht eine Dehnung, solange noch etwas
    /// dahinter haengt. Nur so verteilt Qt den Platz gleichmaessig,
    /// statt alles links zusammenzuschieben und rechts 168 Pixel tot
    /// stehen zu lassen.
    void dieGruppenVerteilenSichUeberDieBreite()
    {
        CommandBar bar;
        auto* plus = new QPushButton(QStringLiteral("+"), &bar);
        bar.addTrailing(plus);
        auto* row = bar.findChild<QHBoxLayout*>();
        QVERIFY(row);

        int kaesten = 0, dehnungen = 0;
        bool letzterKastenHatDehnung = false;
        for (int i = 0; i < row->count(); ++i) {
            QLayoutItem* it = row->itemAt(i);
            if (!it) { continue; }
            if (it->layout()) {
                ++kaesten;
                QLayoutItem* next = (i + 1 < row->count()) ? row->itemAt(i + 1) : nullptr;
                letzterKastenHatDehnung = next && next->spacerItem();
                QVERIFY2(letzterKastenHatDehnung,
                         "hinter jeder Gruppe muss eine Dehnung stehen");
            } else if (it->spacerItem()) { ++dehnungen; }
        }
        QVERIFY2(kaesten >= 6, "zu wenige Gruppen gefunden");
        // Eine je Gruppe -- auch hinter der letzten, denn dahinter
        // haengt das Plus und soll an den rechten Rand.
        QCOMPARE(dehnungen, kaesten);
        QVERIFY2(letzterKastenHatDehnung,
                 "auch hinter der letzten Gruppe muss eine stehen, sonst "
                 "klebt das Plus an der Rate statt am rechten Rand");
    }

    /// D — ohne Funkgeraet traegt eine leere Gruppe den leisen Stil,
    /// eine volle nicht. FILTER zeigt ohne Geraet nur „—", BAND immer
    /// seine Baender -- an dem Paar laesst es sich entscheiden.
    void ohneFunkgeraetTretenLeereGruppenZurueck()
    {
        CommandBar bar;

        // Die Pillen einer Gruppe ueber ihre Beschriftung finden.
        auto pillenMit = [&bar](const QStringList& texte) {
            QVector<QPushButton*> gefunden;
            for (QPushButton* b : bar.findChildren<QPushButton*>()) {
                if (texte.contains(b->text())) { gefunden.append(b); }
            }
            return gefunden;
        };

        const QVector<QPushButton*> baender =
            pillenMit({QStringLiteral("40m"), QStringLiteral("20m")});
        QVERIFY2(!baender.isEmpty(), "keine Band-Pillen gefunden");
        for (QPushButton* b : baender) {
            QCOMPARE(b->styleSheet(), CommandBar::pillStyle());
        }

        const QVector<QPushButton*> leere = pillenMit({QStringLiteral("—")});
        QVERIFY2(!leere.isEmpty(), "keine leeren Pillen gefunden");
        for (QPushButton* b : leere) {
            QVERIFY2(b->styleSheet() == CommandBar::pillStyleGedaempft(),
                     "eine Gruppe ohne Inhalt muss den leisen Stil tragen");
        }

        // Und die beiden Stile duerfen sich nicht gleichen, sonst
        // belegt die Pruefung oben nichts.
        QVERIFY(CommandBar::pillStyle() != CommandBar::pillStyleGedaempft());
    }

    void noPillIsNarrowerThanItsText()
    {
        // Der Betreiber sah am 2026-08-23 live "I0 Hz" statt "10 Hz"
        // und "00 H" statt "100 Hz": mit der neuen BAND-Gruppe fehlte
        // der Platz, und die Anordnung quetschte die Pillen unter ihre
        // eigene Schrift.
        //
        // Gemessen wird an der Schriftmetrik, nicht am Augenschein.
        CommandBar bar;
        bar.resize(700, 60);          // absichtlich eng
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar, kFensterFrist));
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        for (QPushButton* b : bar.findChildren<QPushButton*>()) {
            if (b->text().isEmpty()) { continue; }
            const int need = b->fontMetrics().horizontalAdvance(b->text());
            QVERIFY2(b->width() >= need,
                     qPrintable(QStringLiteral(
                         "Pille \"%1\" ist %2 Punkte breit, die Schrift "
                         "braucht %3 — sie wird abgeschnitten")
                         .arg(b->text()).arg(b->width()).arg(need)));
        }
    }

    void theGroupsAreNamedAndInOrder()
    {
        // Filter und NR kamen am 2026-08-21 dazu, auf Ansage mit
        // Vorlage: „bitte folgende sachen in die leiste oben!
        // bandbreite ... NR1-NR4 eben so."
        //
        // Filter steht neben Modus, weil beide dasselbe Signal
        // beschreiben; NR ans Ende, weil sie eine Behandlung ist und
        // keine Eigenschaft.
        // BAND kam am 2026-08-22 dazu, auf Ansage des Betreibers
        // ("bandwechsel sollte auch mit buttons möglich sein, am
        // besten in der leiste oben") und ganz vorne, weil das Band
        // die groebste Wahl ist: erst das Band, dann Betriebsart,
        // Filter, Schrittweite. Die Vorlage ordnet es genauso.
        CommandBar bar;
        const QStringList g = bar.groups();
        // RATE kam am 2026-09-24 dazu, "neben NR usw.": keine Wahl, eine
        // Anzeige -- darum ganz ans Ende.
        // DIGI kam am 2026-10-21 dazu, auf Ansage des Betreibers vor
        // der Antenne („ich will nicht die freq suchen", „quasi wie
        // jtdx") -- und zwar NEBEN Mode, nicht darin: die Leiste zeigt
        // je Gruppe genau drei Pillen, und FT8/FT4 sind ohnehin keine
        // Betriebsarten des Geraets, sondern eine Anrufstelle samt
        // Dekoder ueber DIGU.
        QCOMPARE(g.size(), 7);
        QCOMPARE(g.at(0), QStringLiteral("Band"));
        QCOMPARE(g.at(1), QStringLiteral("Mode"));
        QCOMPARE(g.at(2), QStringLiteral("Digi"));
        QCOMPARE(g.at(3), QStringLiteral("Filter"));
        QCOMPARE(g.at(4), QStringLiteral("Step"));
        QCOMPARE(g.at(5), QStringLiteral("NR"));
        QCOMPARE(g.at(6), QStringLiteral("Rate"));
    }

    // ── DIGI leuchtet nur, wenn es wirklich laeuft ───────────────────
    void digiLeuchtetNurAufDerAnrufstelle()
    {
        // Ein Knopf, der schon bei DIGU angeht, luegt: in DIGU sitzt
        // man auch auf PSK oder Olivia. Angehen darf er, wenn beides
        // stimmt -- Datenmodus UND Anrufstelle.
        SliceModel slice;
        CommandBar bar;
        bar.attach(&slice);

        slice.setDspMode(DSPMode::USB);
        slice.setFrequency(14'074'000.0);
        QVERIFY2(bar.activePill(QStringLiteral("Digi")).isEmpty(),
                 "leuchtet in SSB, nur weil die Frequenz passt");

        slice.setDspMode(DSPMode::DIGU);
        slice.setFrequency(14'200'000.0);
        QVERIFY2(bar.activePill(QStringLiteral("Digi")).isEmpty(),
                 "leuchtet in DIGU, obwohl weit weg von jeder Stelle");

        slice.setFrequency(14'074'000.0);
        QCOMPARE(bar.activePill(QStringLiteral("Digi")),
                 QStringLiteral("FT8"));

        // Gleiches Band, andere Stelle -- und die Leiste sagt es.
        slice.setFrequency(14'080'000.0);
        QCOMPARE(bar.activePill(QStringLiteral("Digi")),
                 QStringLiteral("FT4"));
    }

    // ── Laeuft ein Dekoder, hat ER Vorrang (2026-10-30) ──────────
    //
    // Der Betreiber stand am 2026-10-30 auf 7.1275 MHz -- keine
    // Anrufstelle, also beide Pillen grau -- waehrend der Dekoder
    // durchgehend FT4 fuhr. Er sagte dazu „war aber auf ft8" und
    // konnte es nicht besser wissen: die einzige Stelle, an der FT4
    // stand, war der Knopf IM FT8-Fenster.
    void derLaufendeDekoderSchlaegtDieFrequenz()
    {
        SliceModel slice;
        CommandBar bar;
        bar.attach(&slice);
        slice.setDspMode(DSPMode::DIGU);

        // Genau sein Fall: weit weg von jeder Anrufstelle.
        slice.setFrequency(7'127'500.0);
        QVERIFY2(bar.activePill(QStringLiteral("Digi")).isEmpty(),
                 "ohne Dekoder sagt sie weiterhin nur die Stelle");

        bar.setzeDigiDekoder(true, DigiFrequenzen::Digimodus::Ft4);
        QCOMPARE(bar.activePill(QStringLiteral("Digi")),
                 QStringLiteral("FT4"));

        // Und sie folgt ihm beim Umschalten, ohne dass sich die
        // Frequenz ruehrt.
        bar.setzeDigiDekoder(true, DigiFrequenzen::Digimodus::Ft8);
        QCOMPARE(bar.activePill(QStringLiteral("Digi")),
                 QStringLiteral("FT8"));
    }

    // Die Gegenprobe zur Gegenprobe: in SSB bleibt sie dunkel, auch
    // wenn ein Dekoder gemeldet ist. Sonst leuchtete sie waehrend
    // eines Sprechdurchgangs weiter.
    void ausserhalbDerDatenmodiBleibtSieDunkel()
    {
        SliceModel slice;
        CommandBar bar;
        bar.attach(&slice);
        bar.setzeDigiDekoder(true, DigiFrequenzen::Digimodus::Ft8);

        slice.setDspMode(DSPMode::USB);
        slice.setFrequency(14'074'000.0);
        QVERIFY2(bar.activePill(QStringLiteral("Digi")).isEmpty(),
                 "leuchtet in SSB, obwohl ein Dekoder gemeldet ist");

        slice.setDspMode(DSPMode::DIGU);
        QCOMPARE(bar.activePill(QStringLiteral("Digi")),
                 QStringLiteral("FT8"));
    }

    // ── RATE ─────────────────────────────────────────────────────────
    // Am 2026-09-24 lief der QRP-Kanal auf 192 kHz, waehrend 48 kHz
    // ankamen. Die Anzeige zeigt die Kanalrate und wird bernsteinfarben,
    // wenn die gemessene Rate zwei Sekunden in Folge deutlich abweicht.
    void theRateShowsTheChannelAndStaysQuietWhenItMatches()
    {
        CommandBar bar;
        QCOMPARE(bar.rateReadoutText(), QStringLiteral("\u2014"));
        bar.setRateReadout(48000, 48000);
        bar.setRateReadout(48000, 47100);   // UDP gebuendelt: -2 %
        bar.setRateReadout(48000, 49300);
        QCOMPARE(bar.rateReadoutText(), QStringLiteral("48 kHz"));
        QVERIFY(!bar.rateReadoutWarns());
    }

    void aStarvedChannelWarnsAfterTwoSeconds()
    {
        CommandBar bar;
        bar.setRateReadout(192000, 48000);
        QVERIFY2(!bar.rateReadoutWarns(), "die erste Sekunde ist angebrochen");
        bar.setRateReadout(192000, 48000);
        QVERIFY(bar.rateReadoutWarns());
        QCOMPARE(bar.rateReadoutText(), QStringLiteral("192 kHz"));

        // Stimmt es wieder, geht die Warnung sofort weg.
        bar.setRateReadout(48000, 48000);
        QVERIFY(!bar.rateReadoutWarns());
    }

    void disconnectingClearsTheReadout()
    {
        CommandBar bar;
        bar.setRateReadout(192000, 384000);
        bar.setRateReadout(192000, 384000);
        QVERIFY(bar.rateReadoutWarns());
        bar.setRateReadout(0, 0);
        QVERIFY(!bar.rateReadoutWarns());
        QCOMPARE(bar.rateReadoutText(), QStringLiteral("\u2014"));
    }

    void threeVisibleEntriesNotThirteen()
    {
        // Der ganze Punkt der Leiste gegenueber der Filterwand im
        // RX-Panel: drei vorne, der Rest im Menue.
        CommandBar bar;
        QCOMPARE(bar.pillsIn(QStringLiteral("Mode")).size(),
                 CommandBar::kVisiblePerGroup);
        QCOMPARE(bar.pillsIn(QStringLiteral("Step")).size(),
                 CommandBar::kVisiblePerGroup);
        QCOMPARE(bar.pillsIn(QStringLiteral("Filter")).size(),
                 CommandBar::kVisiblePerGroup);
        QCOMPARE(bar.pillsIn(QStringLiteral("NR")).size(),
                 CommandBar::kVisiblePerGroup);
    }

    void withoutASliceNothingIsLit()
    {
        // Eine Leiste ohne Empfangskette darf keinen Zustand behaupten.
        CommandBar bar;
        QVERIFY(bar.activePill(QStringLiteral("Mode")).isEmpty());
        QVERIFY(bar.activePill(QStringLiteral("Step")).isEmpty());
    }

    void attachingShowsWhatTheModelSays()
    {
        SliceModel slice;
        slice.setDspMode(DSPMode::USB);
        slice.setStepHz(100);

        CommandBar bar;
        bar.attach(&slice);
        QCOMPARE(bar.activePill(QStringLiteral("Mode")),
                 QStringLiteral("USB"));
        QCOMPARE(bar.activePill(QStringLiteral("Step")),
                 QStringLiteral("100 Hz"));
    }

    void aClickReachesTheModel()
    {
        SliceModel slice;
        slice.setDspMode(DSPMode::USB);
        CommandBar bar;
        bar.attach(&slice);

        QVERIFY(bar.clickPill(QStringLiteral("Mode"), QStringLiteral("LSB")));
        QCOMPARE(slice.dspMode(), DSPMode::LSB);
    }

    // ── Der Weg zurueck, und der ist der wichtigere ──────────────────
    void aChangeMadeElsewhereMovesTheBar()
    {
        // Der Modus laesst sich auch ueber das Menue, per CAT und per
        // Bandwechsel aendern. Eine Leiste, die nur ihren eigenen Klick
        // kennt, steht dann falsch da.
        SliceModel slice;
        slice.setDspMode(DSPMode::LSB);
        CommandBar bar;
        bar.attach(&slice);
        QCOMPARE(bar.activePill(QStringLiteral("Mode")),
                 QStringLiteral("LSB"));

        slice.setDspMode(DSPMode::USB);
        QCOMPARE(bar.activePill(QStringLiteral("Mode")),
                 QStringLiteral("USB"));
    }

    void exactlyOnePillIsLitPerGroup()
    {
        SliceModel slice;
        slice.setDspMode(DSPMode::CWL);
        slice.setStepHz(1000);
        CommandBar bar;
        bar.attach(&slice);

        // NR ist ausgenommen, und das ist kein Schoenheitsfehler:
        // Modus, Schrittweite und Filter haben IMMER einen Wert, die
        // Rauschminderung hat einen Aus-Zustand (NrSlot::Off). Zu
        // verlangen, dass dort immer eine Pille leuchtet, hiesse zu
        // verlangen, dass immer eine Rauschminderung laeuft.
        //
        // Der Test hat beim Einbau von NR am 2026-08-21 genau darauf
        // hingewiesen — und vorher schon einen echten Entwurfsfehler
        // bei den Filterpillen aufgedeckt.
        // RATE ist ebenfalls ausgenommen: sie ist eine Anzeige, keine
        // Wahl, und hat gar keine Pillen (2026-09-24).
        for (const QString& g : bar.groups()) {
            // DIGI ist aus demselben Grund ausgenommen wie NR: es
            // gibt einen Aus-Zustand. Wer SSB faehrt, macht weder FT8
            // noch FT4 — zu verlangen, dass dort immer eine Pille
            // leuchtet, hiesse zu verlangen, dass immer ein
            // Datenmodus laeuft (2026-10-21).
            if (g == QStringLiteral("NR") || g == QStringLiteral("Rate")
                || g == QStringLiteral("Digi")) { continue; }
            int lit = 0;
            for (const QString& p : bar.pillsIn(g)) {
                if (bar.activePill(g) == p) { ++lit; }
            }
            QVERIFY2(lit == 1,
                     qPrintable(QStringLiteral("Gruppe %1: %2 Pillen an")
                                    .arg(g).arg(lit)));
        }
    }

    // ── Ein Modus, der nicht zu den ersten dreien gehoert ────────────
    void aModeOutsideTheFirstThreeTakesTheLastSlot()
    {
        // Sonst zeigt die Leiste drei Pillen, von denen keine an ist,
        // und der eingestellte Modus steht nirgends auf dem Schirm.
        SliceModel slice;
        slice.setDspMode(DSPMode::AM);      // Platz 5 der Gesamtliste
        CommandBar bar;
        bar.attach(&slice);

        QCOMPARE(bar.activePill(QStringLiteral("Mode")),
                 QStringLiteral("AM"));
        QVERIFY2(bar.pillsIn(QStringLiteral("Mode"))
                     .contains(QStringLiteral("AM")),
                 "AM ist eingeschaltet, steht aber auf keiner Pille");
        // Und es bleiben drei. Die Leiste waechst nicht.
        QCOMPARE(bar.pillsIn(QStringLiteral("Mode")).size(),
                 CommandBar::kVisiblePerGroup);
    }

    // ── Der Wackelkontakt ────────────────────────────────────────────
    void switchingSlicesLetsGoOfTheOldOne()
    {
        SliceModel a;
        SliceModel b;
        a.setDspMode(DSPMode::LSB);
        b.setDspMode(DSPMode::USB);

        CommandBar bar;
        bar.attach(&a);
        bar.attach(&b);
        QCOMPARE(bar.activePill(QStringLiteral("Mode")),
                 QStringLiteral("USB"));

        // Die alte Kette darf die Leiste nicht mehr bewegen.
        a.setDspMode(DSPMode::CWL);
        QCOMPARE(bar.activePill(QStringLiteral("Mode")),
                 QStringLiteral("USB"));

        // Die neue schon.
        b.setDspMode(DSPMode::CWU);
        QCOMPARE(bar.activePill(QStringLiteral("Mode")),
                 QStringLiteral("CWU"));
    }

    // ── Der Fund vom 2026-09-07 ──────────────────────────────────────
    //
    // Live gegen ein angeschlossenes ANAN 10E, nach einer laengeren
    // Sitzung mit zwischenzeitlich aktivem KiwiSDR-Profil: die
    // MODE-Gruppe zeigte LSB und DIGL gleichzeitig als aktiv, obwohl
    // SliceModel::dspMode() der echten Scheibe durchgehend DIGL blieb.
    //
    // Ursache: die angehaengte Scheibe war verschwunden, ohne dass ein
    // erneutes attach() das nachgezogen hatte. Ein Klick auf LSB
    // schaltete daraufhin nur Qt's EIGENEN Checked-Haken des Knopfes um
    // (Vorgabeverhalten eines checkable QPushButton ohne
    // QButtonGroup-Exklusivitaet) -- pushModeToModel() und
    // pullFromModel() liefen beide durch ihre "kein Modell"-Wachen und
    // liessen den alten Zustand (DIGL) stehen, statt LSB abzulehnen.
    //
    // Dieser Test simuliert das Verschwinden direkt (delete statt eines
    // erneuten attach()), unabhaengig davon, WAS im Zusammenspiel von
    // RadioModel/KiwiSDR die Bindung im Betrieb verloren hat -- die
    // Leiste selbst darf so einen Zustand nicht zeigen.
    void danglingSliceLeavesNothingLitInsteadOfTwoPillsLit()
    {
        auto* slice = new SliceModel();
        slice->setDspMode(DSPMode::DIGL);

        CommandBar bar;
        bar.attach(slice);
        QCOMPARE(bar.activePill(QStringLiteral("Mode")),
                 QStringLiteral("DIGL"));

        delete slice;   // verschwindet, OHNE dass jemand attach() erneut ruft

        QVERIFY(bar.clickPill(QStringLiteral("Mode"), QStringLiteral("LSB")));

        int lit = 0;
        const QStringList modePills = bar.pillsIn(QStringLiteral("Mode"));
        for (QPushButton* b : bar.findChildren<QPushButton*>()) {
            if (modePills.contains(b->text()) && b->isChecked()) { ++lit; }
        }
        QVERIFY2(lit == 0,
                 qPrintable(QStringLiteral(
                     "Ohne Modell darf keine Pille leuchten -- %1 tun es")
                     .arg(lit)));
    }

    void detachingIsAllowedAndQuiet()
    {
        SliceModel slice;
        slice.setDspMode(DSPMode::USB);
        CommandBar bar;
        bar.attach(&slice);
        bar.attach(nullptr);
        // Kein Absturz, und die alte Kette bewegt nichts mehr.
        slice.setDspMode(DSPMode::LSB);
        QCOMPARE(bar.activePill(QStringLiteral("Mode")),
                 QStringLiteral("USB"));
    }

    /// Die drei vorderen Breiten sind 2,9 / 3,2 / 3,8 — und eine
    /// Filterpille setzt wirklich den Filter.
    ///
    /// Sie sind KEINE Presets, sondern Breiten aus den Einstellungen
    /// („CommandBarFilterFront"). Der Betreiber hat die Auswahl am
    /// 2026-08-21 innerhalb weniger Minuten zweimal geaendert; waere
    /// sie fest verdrahtet, waere jede Meinungsaenderung eine
    /// Uebersetzung.
    void theThreeFrontWidthsAreTheOnesAsked()
    {
        SliceModel slice;
        slice.setDspMode(DSPMode::LSB);

        CommandBar bar;
        bar.attach(&slice);

        const QStringList labels = bar.pillsIn(QStringLiteral("Filter"));
        QCOMPARE(labels.size(), 3);
        QCOMPARE(labels.at(0), QStringLiteral("2.9k"));
        QCOMPARE(labels.at(1), QStringLiteral("3.2k"));
        QCOMPARE(labels.at(2), QStringLiteral("3.8k"));
    }

    /// Ein Klick setzt die Breite und laesst die MITTE stehen.
    ///
    /// Sonst spraenge ein LSB-Filter auf die andere Seite des
    /// Traegers, sobald man eine Breite waehlt — aus -2900…0 wuerde
    /// -1600…+1600.
    void aFilterPillKeepsTheCentre()
    {
        SliceModel slice;
        slice.setDspMode(DSPMode::LSB);
        slice.setFilter(-2900, 0);

        CommandBar bar;
        bar.attach(&slice);

        const double centreBefore =
            (slice.filterLow() + slice.filterHigh()) / 2.0;

        QVERIFY(bar.clickPill(QStringLiteral("Filter"),
                              QStringLiteral("3.8k")));

        QCOMPARE(qAbs(slice.filterHigh() - slice.filterLow()), 3800);
        const double centreAfter =
            (slice.filterLow() + slice.filterHigh()) / 2.0;
        QVERIFY2(qAbs(centreAfter - centreBefore) <= 1.0,
                 qPrintable(QStringLiteral(
                     "Die Mitte ist gewandert: %1 -> %2")
                     .arg(centreBefore).arg(centreAfter)));
    }

    /// Beim Moduswechsel werden die Pillen NEU BESCHRIFTET.
    ///
    /// Feste Beschriftungen haetten in CW die Breiten von SSB
    /// angeboten — ein Knopf, der 2.9k sagt und 500 Hz schaltet.
    void theFilterPillsFollowTheMode()
    {
        SliceModel slice;
        CommandBar bar;
        bar.attach(&slice);

        slice.setDspMode(DSPMode::LSB);
        const QStringList ssb = bar.pillsIn(QStringLiteral("Filter"));
        slice.setDspMode(DSPMode::CWL);
        const QStringList cw = bar.pillsIn(QStringLiteral("Filter"));

        QVERIFY2(ssb != cw,
                 qPrintable(QStringLiteral(
                     "SSB und CW zeigen dieselben Breiten: %1")
                     .arg(ssb.join(QLatin1Char('|')))));
    }

    /// Das „…" der Filtergruppe zeigt die Vorgaben mit ihren NAMEN —
    /// ohne Speicher die Thetis-Namen, mit Speicher auch das, was der
    /// Betreiber unter Setup › Filter Presets selbst eingetragen hat.
    ///
    /// Bis 2026-09-28 las das Menue SliceModel::presetsForMode direkt:
    /// eigene Vorgaben kamen nie an, und die Beschriftung war aus den
    /// Kanten gerechnet — CW F10 (±13 Hz) hiesse „26", Thetis sagt „25".
    void theFilterOverflowOffersNamedAndOwnPresets()
    {
        AppSettings::instance().remove(QStringLiteral("CWPitch"));
        SliceModel slice;
        slice.setDspMode(DSPMode::CWU);
        CommandBar bar;
        bar.attach(&slice);

        auto entries = bar.filterMenuEntries();
        QCOMPARE(entries.size(), 10);
        QCOMPARE(entries.at(0).name, QStringLiteral("1.0k"));
        QCOMPARE(entries.at(0).low, 100);
        QCOMPARE(entries.at(0).high, 1100);
        QCOMPARE(entries.at(9).name, QStringLiteral("25"));
        QCOMPARE(entries.at(9).low, 587);
        QCOMPARE(entries.at(9).high, 613);

        FilterPresetStore store;
        store.setPreset(DSPMode::CWU, 0, FilterPreset{QStringLiteral("Mine"), 400, 800});
        bar.setFilterPresetStore(&store);
        entries = bar.filterMenuEntries();
        QCOMPARE(entries.at(0).name, QStringLiteral("Mine"));
        QCOMPARE(entries.at(0).low, 400);
        QCOMPARE(entries.at(0).high, 800);
        QCOMPARE(entries.at(1).name, QStringLiteral("800"));

        // Folgt dem Modus der Scheibe.
        slice.setDspMode(DSPMode::USB);
        entries = bar.filterMenuEntries();
        QCOMPARE(entries.at(3).name, QStringLiteral("3.3k"));
        QCOMPARE(entries.at(3).high, 3400);

        store.resetAll();
    }

    /// NR schaltet ein — und ein zweiter Klick wieder aus.
    void anNrPillTogglesTheModel()
    {
        SliceModel slice;
        CommandBar bar;
        bar.attach(&slice);

        QCOMPARE(slice.activeNr(), NrSlot::Off);

        QVERIFY(bar.clickPill(QStringLiteral("NR"), QStringLiteral("NR1")));
        QCOMPARE(slice.activeNr(), NrSlot::NR1);
        QCOMPARE(bar.activePill(QStringLiteral("NR")),
                 QStringLiteral("NR1"));

        // Derselbe Knopf noch einmal: das ist der einzige Weg zu
        // „keine", ohne einen achten Knopf „AUS".
        QVERIFY(bar.clickPill(QStringLiteral("NR"), QStringLiteral("NR1")));
        QCOMPARE(slice.activeNr(), NrSlot::Off);
        QVERIFY(bar.activePill(QStringLiteral("NR")).isEmpty());
    }

    /// Laeuft eine Rauschminderung, die nicht unter den ersten dreien
    /// ist, rueckt sie an die letzte sichtbare Stelle.
    ///
    /// Der erste Entwurf hatte hier eine EIGENE Regel: die Marke am
    /// „…" statt Nachruecken. Der bestehende Test
    /// exactlyOnePillIsLitPerGroup hat sie sofort verworfen — zu
    /// Recht, zwei Regeln fuer dieselbe Leiste sind eine zu viel.
    void anNrOutsideTheFirstThreeTakesTheLastSlot()
    {
        SliceModel slice;
        CommandBar bar;
        bar.attach(&slice);

        // NNR, nicht DFNR: seit dem 2026-09-17 stehen nur noch die
        // Minderungen in der Leiste, die dieser Bau auch enthaelt, und
        // DFNR ist ohne DeepFilterNet-Bibliothek keine davon. NNR (WDSP
        // 2.10) ist immer dabei und steht ebenfalls hinter den ersten
        // drei.
        slice.setActiveNr(NrSlot::NNR);

        QCOMPARE(bar.activePill(QStringLiteral("NR")),
                 QStringLiteral("NNR"));
        QCOMPARE(bar.pillsIn(QStringLiteral("NR")).last(),
                 QStringLiteral("NNR"));

        // Und der nachgerueckte Knopf schaltet auch wirklich NNR —
        // nicht das, was vorher an seiner Stelle stand. Die Nutzlast
        // haengt am Knopf, nicht an seiner Stelle in der Liste.
        slice.setActiveNr(NrSlot::Off);
        QVERIFY(bar.clickPill(QStringLiteral("NR"),
                              QStringLiteral("NNR")));
        QCOMPARE(slice.activeNr(), NrSlot::NNR);
    }

    /// Was dieser Bau nicht enthaelt, steht nicht in der Leiste: ohne
    /// HAVE_DFNR kein DFNR, ohne HAVE_BNR kein BNR (2026-09-17, "bitte
    /// kontrollieren ... auch die anderen optionen im offenen menü").
    void onlyBuiltInNrBackendsAreOffered()
    {
        SliceModel slice;
        CommandBar bar;
        bar.attach(&slice);
        const QStringList all = bar.allNrLabels();
#ifndef HAVE_DFNR
        QVERIFY2(!all.contains(QStringLiteral("DFNR")),
                 "DFNR steht in der Leiste, obwohl es in diesem Bau nichts tut");
#endif
#ifndef HAVE_BNR
        QVERIFY2(!all.contains(QStringLiteral("BNR")),
                 "BNR steht in der Leiste, obwohl es in diesem Bau nichts tut");
#endif
        QVERIFY(all.contains(QStringLiteral("NNR")));
        QVERIFY(all.contains(QStringLiteral("NR4")));
    }

    /// An der FlexRadio zeigt die Rauschminderungs-Leiste nur, was die
    /// Maschine fuehrt oder am Flex-Ton wirkt -- und die Maschine nennt
    /// es bei ihrem Namen (2026-11-20, Betreiber: „bei flexnutzung sollte
    /// natuerlich auch nur das angezeigt werden, was wir koennen").
    ///
    /// Die Attrappe ist setNrFlexModus(true); die Regeln sind die echten
    /// Funktionen aus RadioModel. Ohne Flex (Anvelina, kein Funkgeraet)
    /// muss alles genau so stehen wie vor der Aenderung.
    void anDerFlexNurDieErlaubtenPillenMitFlexNamen()
    {
        SliceModel slice;
        CommandBar bar;
        bar.setNrFlexRegeln(
            [](NrSlot s) { return RadioModel::flexNrAngeboten(s); },
            [](NrSlot s) { return RadioModel::flexNrAnzeige(s); });
        bar.attach(&slice);

        // Ohne Flex: wie bisher, mit Longpath-Namen.
        const QStringList ohneFlex = bar.allNrLabels();
        QCOMPARE(bar.pillsIn(QStringLiteral("NR")),
                 (QStringList{QStringLiteral("NR1"), QStringLiteral("NR2"),
                              QStringLiteral("NR3")}));
        QVERIFY(ohneFlex.contains(QStringLiteral("NNR")));
        QVERIFY(ohneFlex.contains(QStringLiteral("NR4")));

        bar.setNrFlexModus(true);

        // Vorne die drei Pillen der Maschine, dahinter das „…": NRL, NR,
        // NR3 vorne; NRS (NR4) und MNR (nur mit HAVE_MNR, also auf dem Mac)
        // im „…". DFNR, BNR, NNR fehlen ganz.
        QCOMPARE(bar.pillsIn(QStringLiteral("NR")),
                 (QStringList{QStringLiteral("NRL"), QStringLiteral("NR"),
                              QStringLiteral("NR3")}));
        QStringList erwartet{QStringLiteral("NRL"), QStringLiteral("NR"),
                             QStringLiteral("NR3"), QStringLiteral("NRS")};
        if (ohneFlex.contains(QStringLiteral("MNR"))) {
            erwartet << QStringLiteral("MNR");
        }
        QCOMPARE(bar.allNrLabels(), erwartet);
        for (const char* weg : {"NR1", "NR2", "NR4", "DFNR", "BNR", "NNR"}) {
            QVERIFY2(!bar.allNrLabels().contains(QString::fromLatin1(weg)),
                     weg);
        }

        // Die Namen sind nur Namen: der Knopf „NRL" schaltet NR1 (lms_nr),
        // der Knopf „NR" schaltet NR2 (nr), und die Leuchte folgt dem Modell.
        QVERIFY(bar.clickPill(QStringLiteral("NR"), QStringLiteral("NRL")));
        QCOMPARE(slice.activeNr(), NrSlot::NR1);
        QCOMPARE(bar.activePill(QStringLiteral("NR")),
                 QStringLiteral("NRL"));
        QVERIFY(bar.clickPill(QStringLiteral("NR"), QStringLiteral("NRL")));

        slice.setActiveNr(NrSlot::NR2);
        QCOMPARE(bar.activePill(QStringLiteral("NR")),
                 QStringLiteral("NR"));

        // Die Zuordnung der Namen zu den Verfahren, ohne Oberflaeche:
        QCOMPARE(RadioModel::flexNrAnzeige(NrSlot::NR1), QStringLiteral("NRL"));
        QCOMPARE(RadioModel::flexNrAnzeige(NrSlot::NR2), QStringLiteral("NR"));
        QCOMPARE(RadioModel::flexNrAnzeige(NrSlot::NR4), QStringLiteral("NRS"));
        // NR3 und MNR laufen bei uns: ihr Name bleibt.
        QVERIFY(RadioModel::flexNrAnzeige(NrSlot::NR3).isEmpty());
        QVERIFY(RadioModel::flexNrAnzeige(NrSlot::MNR).isEmpty());

        // Zurueck zu Anvelina: alles wie vorher.
        bar.setNrFlexModus(false);
        QCOMPARE(bar.allNrLabels(), ohneFlex);
        QCOMPARE(bar.pillsIn(QStringLiteral("NR")),
                 (QStringList{QStringLiteral("NR1"), QStringLiteral("NR2"),
                              QStringLiteral("NR3")}));
    }

    /// Rechtsklick auf eine NR-Pille oeffnet ihre Schnellregler.
    ///
    /// Umgezogen aus tst_rx_applet_inherited am 2026-08-21, zusammen
    /// mit der Faehigkeit selbst. Die Begruendung von dort gilt
    /// unveraendert: der Rechtsklick oeffnet nicht die
    /// Einstellungsseite, sondern die drei bis fuenf Regler GENAU
    /// DIESER Minderung; die Einstellungsseite ist darin nur der
    /// Verweis ganz unten. Ohne diesen Weg waeren die Regler nur noch
    /// ueber das Setup-Menue erreichbar.
    void rightClickOnAnNrPillOpensItsQuickControls()
    {
        SliceModel slice;
        CommandBar bar;
        bar.attach(&slice);

        QPushButton* nr1 = nullptr;
        for (QPushButton* b : bar.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("NR1")) { nr1 = b; break; }
        }
        QVERIFY2(nr1, "Keine NR1-Pille in der Leiste");

        emit nr1->customContextMenuRequested(QPoint(2, 2));

        DspParamPopup* popup = nullptr;
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (auto* p = qobject_cast<DspParamPopup*>(w)) { popup = p; }
        }
        if (!popup) { popup = bar.findChild<DspParamPopup*>(); }

        QVERIFY2(popup,
                 "Rechtsklick oeffnet keinen Schnellregler — dann sind "
                 "die NR-Parameter nur noch ueber das Setup-Menue "
                 "erreichbar");
        QVERIFY2(!popup->findChildren<QSlider*>().isEmpty(),
                 "der Schnellregler hat keine Regler");
        popup->close();
    }

    /// Und aus dem Schnellregler heraus fuehrt weiter ein Weg auf die
    /// volle Einstellungsseite. Ebenfalls umgezogen aus
    /// tst_rx_applet_inherited (2026-08-21), Mechanismus unveraendert.
    void theQuickControlsStillLinkToTheFullSetupPage()
    {
        SliceModel slice;
        CommandBar bar;
        bar.attach(&slice);

        QSignalSpy spy(&bar, &CommandBar::openNrSetupRequested);

        QPushButton* nr3 = nullptr;
        for (QPushButton* b : bar.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("NR3")) { nr3 = b; break; }
        }
        QVERIFY(nr3);
        emit nr3->customContextMenuRequested(QPoint(2, 2));

        DspParamPopup* popup = bar.findChild<DspParamPopup*>();
        for (QWidget* w : QApplication::topLevelWidgets()) {
            if (auto* p = qobject_cast<DspParamPopup*>(w)) { popup = p; }
        }
        QVERIFY(popup);

        QPushButton* more = nullptr;
        for (QPushButton* b : popup->findChildren<QPushButton*>()) {
            if (b && b->text().contains(QStringLiteral("More"))) { more = b; }
        }
        QVERIFY2(more, "kein Verweis auf die volle Einstellungsseite");
        more->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<NrSlot>(), NrSlot::NR3);
    }
    /// Jedes „…"-Menue haengt am Fenster, nicht an der Leiste.
    ///
    /// Befund 2026-09-29 (Martins Protokoll): „QWidgetWindow(...,
    /// name="QWidgetClassWindow") must be a top level window." — dreimal,
    /// jedesmal kurz vor einem Bandwechsel auf die gespeicherte Frequenz
    /// des Bandes. Die Leiste liegt neben dem nativen Panadapter; je nach
    /// Aufbau hat sie selbst ein natives Fenster oder ihr (namenloses)
    /// Elternteil. Ein QMenu an so einem Feld bekommt von Qt keinen
    /// Fensterbezug — QWindow::setTransientParent() lehnt ab. Beide
    /// Aufbauten werden geprueft. Die Farbe bleibt: am echten Hauptfenster
    /// verglichen, 0 abweichende Pixel.
    void theOverflowMenusHangOnTheWindow_data()
    {
        QTest::addColumn<bool>("barItselfNative");
        QTest::newRow("Leiste nativ") << true;
        QTest::newRow("Leiste im nativen Feld") << false;
    }

    void theOverflowMenusHangOnTheWindow()
    {
        QFETCH(bool, barItselfNative);
        QTest::failOnWarning(QRegularExpression(QStringLiteral("must be a top level window")));

        SliceModel slice;
        slice.setDspMode(DSPMode::USB);
        QWidget top;
        top.resize(1200, 120);
        auto* field = new QWidget(&top);
        field->setGeometry(0, 0, 1200, 120);
        field->setAttribute(Qt::WA_DontCreateNativeAncestors);
        field->setAttribute(Qt::WA_NativeWindow);
        auto* bar = new CommandBar(field);
        if (barItselfNative) {
            bar->setAttribute(Qt::WA_NativeWindow);
        }
        bar->setGeometry(0, 0, 1200, 120);
        bar->attach(&slice);
        top.show();
        QVERIFY(QTest::qWaitForWindowExposed(&top, kFensterFrist));
        QCOMPARE(bar->windowHandle() != nullptr, barItselfNative);

        int opened = 0;
        for (QPushButton* more : bar->findChildren<QPushButton*>()) {
            if (more->text() != QStringLiteral("…")) { continue; }
            // Frueher ein eigener QTimer auf activePopupWidget(): der
            // las unter Last den Stand von vorher, und dieser Lauf fiel
            // nur im -j6-Durchgang um. Siehe tests/MenueFaenger.h.
            const auto fund = Test::oeffneMenue([&]() { more->click(); });
            QVERIFY2(fund.seen, "kein Ueberlaufmenue erschienen");
            QCOMPARE(fund.parent, static_cast<QWidget*>(&top));
            ++opened;
        }
        QCOMPARE(opened, 5);   // Modus, Band, Filter, Schritt, NR
    }
};

QTEST_MAIN(TestCommandBar)
#include "tst_command_bar.moc"
