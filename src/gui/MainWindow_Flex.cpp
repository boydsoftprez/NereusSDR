// =================================================================
// src/gui/MainWindow_Flex.cpp  (Longpath)
// =================================================================
//
// Flex-Treiber Stufe 2: Ton und Bild ins Fenster.
//
// Longpath-original — aber es haengt an zwei Naehten, die aus AetherSDR
// stammen (https://github.com/aethersdr/AetherSDR, GPLv3, primary
// author Jeremy [KK7GWY]), und deshalb steht das hier und nicht nur in
// der Herkunftstabelle:
//
//   * `AudioEngine::feedFlexAudioData` ist das dritte Mal desselben
//     Musters; die zwei davor (`feedKiwiSdrAudioData`,
//     `feedSunSdrAudioData`) sind aus AetherSDRs Tonweg portiert und
//     muenden wie dieser in `rxBlockReady`.
//   * `SpectrumWidget::updateKiwiSpectrumDbm` ist der Eingang fuer
//     fertige dBm-Bins einer fremden Quelle; seine Begruendung („keine
//     zweite Rohrleitung noetig") ist in SpectrumWidget.h an AetherSDR
//     zitiert.
//   * Das Sicherheitstor (`streamIndex() >= 0` nie fuettern) kommt aus
//     der SunSDR-Durchsicht, die ihrerseits AetherSDRs Aufbau folgt.
//
// Dieselbe Lage wie `src/gui/applets/StripGraphics.h`: eigener Code,
// der auf portiertem aufsitzt — also wird die Quelle benannt, nicht nur
// die eigene Urheberschaft.
//
// Der Zuschnitt folgt den zwei fremden Quellen, die es schon gibt —
// MainWindow_KiwiSdr.cpp (875 Zeilen) und MainWindow_SunSdr.cpp (849) —
// und ist deutlich kleiner als beide, weil ein Flex kein
// Profil-Verwaltungswerk braucht: es gibt genau eine Maschine, und ihre
// Stroeme tragen ihre Kennung im Kopf.
//
// Was hier passiert, ist die Naht aus
// docs/architecture/2026-10-18-flex-stufe1-design.md §5:
//
//   Ton  -> AudioEngine::feedFlexAudioData  -> rxBlockReady
//           (der EINE Punkt, an dem fertiger Ton hereinkommt; daneben
//            liegen feedKiwiSdrAudioData und feedSunSdrAudioData)
//   Bild -> SpectrumWidget::updateKiwiSpectrumDbm
//           (fertige dBm-Bins ueber einen eigenen Ausschnitt; KEINE
//            zweite Rohrleitung noetig — das begruendet SpectrumWidget.h
//            selbst bei :1405-1421)
//
// Die 34 WDSP-Stellen in RadioModel bleiben unberuehrt: eine
// Flex-Scheibe ist eine Scheibe OHNE RxChannel, und sie fragen alle
// nach einem.
//
// =================================================================
// Zum Namen `updateKiwiSpectrumDbm`
// =================================================================
//
// Der Eingang heisst `kiwi…`, weil er fuer den KiwiSDR gebaut wurde; er
// ist aber der allgemeine Eingang fuer FERTIGE dBm-Bins einer fremden
// Quelle, und die Begruendung in SpectrumWidget.h ist nicht
// kiwi-spezifisch. Hier wird er darum bewusst mitbenutzt statt
// hundert Zeilen daneben zu stellen. Das ist eine Namensschuld, kein
// Fehler — ein Umbenennen waere ein eigener Handgriff.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-18 — Original for Longpath by Martin Fischer, AI-assisted
//                 via Anthropic Claude (Cowork).
//   2026-11-11 — Der Sprachspeicher als Quelle der Sendepumpe (F1…F10
//                 an der Flex). KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "gui/MainWindow.h"
#include "gui/widgets/StatusToast.h"
#include "gui/applets/RxApplet.h"

#include "core/AudioEngine.h"
#include "core/Ft8Tx.h"
#include "core/MoxController.h"
#include "core/FlexVitaStream.h"
#include "core/strip/StripChain.h"
// Fuer den Anspruchs-Abgleich: Kiwi fuehrt seine Zuordnung in einer
// eigenen Tabelle, nicht ueber streamIndex().
#include "core/KiwiSdrManager.h"
#include "gui/meters/MeterPoller.h"
#include "gui/ConnectionPanel.h"
#include "core/ClarityController.h"
#include "core/AppSettings.h"
#include <QTimer>
#include <cmath>

#include "core/FlexFrequenzSpur.h"
#include "core/FlexFensterZug.h"
#include <algorithm>
#include "gui/SpectrumWidget.h"
#include "gui/widgets/CommandBar.h"
#include "core/audio/VoiceKeyerPlayer.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/BandDefaults.h"

#include <QLoggingCategory>

namespace Longpath {

namespace {
Q_LOGGING_CATEGORY(lcFlexGui, "longpath.flex.gui", QtInfoMsg)

// Gemessen am 2026-10-18: 24 012,3 Hz ueber ein 24-Sekunden-Fenster,
// also 24 kHz auf 0,05 % genau. Als Konstante und nicht als Annahme im
// Wandler — eine falsche Rate hoert man sofort als falsche Tonhoehe.
constexpr int kFlexAudioRateHz = 24000;
}  // namespace

// Dasselbe Tor wie sunSdrControllableSlice(): die harte Regel aus der
// SunSDR-Sicherheitsdurchsicht gilt fuer jede fremde Quelle — eine
// Scheibe mit echter DDC-Bindung (`streamIndex() >= 0`, ein wirkliches,
// womoeglich sendefaehiges Geraet) wird aus diesem Weg NIE gefuettert
// oder gesteuert.
//
// Die `m_shuttingDown`-Wache ist nicht Schmuck: `m_radioModel` ist ein
// rohes RadioModel*, kein QPointer, wird beim Abbau also freigegeben und
// nicht auf nullptr gesetzt (derselbe ASAN-bestaetigte Heap-UAF wie bei
// SunSDR, docs/architecture/2026-08-29-sunsdr-tci-teardown-segfault-investigation.md).
SliceModel* MainWindow::flexControllableSlice() const
{
    if (m_shuttingDown) { return nullptr; }
    if (!m_radioModel || m_flexTargetSliceId < 0) { return nullptr; }
    SliceModel* slice = m_radioModel->sliceById(m_flexTargetSliceId);
    if (!slice || slice->streamIndex() >= 0) { return nullptr; }
    return slice;
}

void MainWindow::wireFlex()
{
    if (!m_radioModel) { return; }

    // ── Die Zielscheibe waehlen, sobald die Anmeldung durch ist ──────
    //
    // Erst dann gibt es ueberhaupt Stroeme. Gewaehlt wird nach derselben
    // Regel wie bei den zwei anderen fremden Quellen, und mit deren
    // Anspruechen: Kiwi und SunSDR fuehren ihre Zuordnung ebenfalls
    // nicht ueber `streamIndex()`, also muss sie hier mitgeprueft
    // werden — sonst traegt eine Scheibe zwei Erzeuger, und die Spur
    // bekommt zwei Faeden.
    connect(m_radioModel, &RadioModel::flexGuiClientRegistered, this,
            [this](const QString&) {
        if (m_shuttingDown || !m_radioModel) { return; }

        const auto istFremdBelegt = [this](const SliceModel* s) {
            if (!s) { return true; }
            if (m_sunSdrTargetSliceId == s->sliceIndex()) { return true; }
            if (m_kiwiSdrManager
                && !m_kiwiSdrManager->assignedProfileForSlice(s->sliceIndex()).isEmpty()) {
                return true;
            }
            return false;
        };

        SliceModel* ziel = m_radioModel->activeSlice();
        if (ziel && (ziel->streamIndex() >= 0 || istFremdBelegt(ziel))) {
            ziel = nullptr;
        }
        if (!ziel) {
            for (SliceModel* kandidat : m_radioModel->slices()) {
                if (kandidat && kandidat->streamIndex() < 0
                    && !istFremdBelegt(kandidat)) {
                    ziel = kandidat;
                    break;
                }
            }
        }
        if (!ziel) {
            // 2026-10-21 an Martins Station: ALLE Scheiben hingen an
            // einem echten Geraet (`streamIndex() >= 0`), also war keine
            // frei — und damit lief weder Ton NOCH Bild, denn beides
            // haengt an dieser Zuordnung (der Panadapter ueber
            // `setKiwiDisplaySource(true)` weiter unten). Die alte
            // Warnung behauptete „Bild laeuft, Ton nicht"; das war
            // falsch und hat eine Stunde Suche gekostet.
            //
            // Die Flex ist ein eigenes Geraet und wartet nicht darauf,
            // dass eine Scheibe frei wird: sie bekommt ihre eigene.
            // `suppressAutoStreamBinding` ist dabei Pflicht — ohne das
            // greift `bindSliceToStream()` zu, die Scheibe bekommt
            // einen echten `streamIndex()`, und die Sicherheitswache
            // liest das als „ein wirkliches Geraet hat uebernommen"
            // und schneidet genau den Weg ab, den wir gerade bauen.
            const int neu = m_radioModel->addSlice(QString(), true);
            ziel = (neu >= 0) ? m_radioModel->sliceById(neu) : nullptr;
            m_flexSliceWasCreated = (ziel != nullptr);
        }
        if (!ziel) {
            qCWarning(lcFlexGui) << "keine Scheibe fuer die Flex, und das "
                                    "Anlegen misslang — weder Ton noch Bild";
            return;
        }

        m_flexTargetSliceId = ziel->sliceIndex();

        // ── Und sie muss die AKTIVE Scheibe sein (2026-10-22) ────────
        //
        // `RadioModel::onBandButtonClicked` — der Weg jedes Band- und
        // Frequenzwechsels — beginnt mit `activeSlice()` und kehrt ohne
        // ein Wort um, wenn es keine gibt. Beim Flex legen wir die
        // Scheibe selbst an; aktiv wird sie dabei nicht. Folge am
        // Geraet am 2026-10-22: der Betreiber drehte einen Abend lang
        // an Band und Frequenz, und an die Maschine ging NULL Mal ein
        // `slice tune`. Sie stand die ganze Zeit auf 14.100.
        m_radioModel->setActiveSliceById(m_flexTargetSliceId);

        // Von innen nach aussen. Erst HIER verdrahtet, weil es die
        // Zielscheibe vorher nicht gibt.
        //
        // ── `Qt::UniqueConnection` geht nicht mit einer Lambda ───────
        //
        // Hier stand `Qt::UniqueConnection`, damit ein zweites Anmelden
        // nicht doppelt sendet. Qt unterstuetzt das aber NUR fuer
        // Zeiger auf Elementfunktionen; bei einer Lambda scheitert
        // `connect()` still und gibt eine leere Verbindung zurueck —
        // es wird GAR NICHTS verbunden.
        //
        // Folge am Geraet, 2026-10-22, ein ganzer Abend: der Betreiber
        // drehte an Band und Frequenz, die Anzeige folgte, und an die
        // Maschine ging in sechs Laeufen NULL Mal ein `slice tune`. Die
        // Flex stand die ganze Zeit auf 14.100 USB — dort ist nur
        // Rauschen, und genau das hat er gehoert. Gemessen:
        //   Klick im Panadapter "pan-0" 1.41747e+07 Scheibe 0
        //   gesperrt false
        // und danach kein Mucks mehr.
        //
        // Das Doppelte wird jetzt von Hand verhindert: die alte
        // Verbindung wird vor der neuen geloest.
        disconnect(m_flexAbstimmBand);
        m_flexAbstimmBand = connect(ziel, &SliceModel::frequencyChanged, this,
                [this](double hz) {
            if (m_shuttingDown || m_flexApplyingRemoteState) { return; }
            // Sagt, WARUM nicht abgestimmt wird. Ohne das sieht ein
            // nicht durchgereichter Drehknopf genauso aus wie einer,
            // der gar nicht erst ankommt (2026-10-22, eine Stunde).
            if (!flexControllableSlice()) {
                SliceModel* s = m_radioModel
                    ? m_radioModel->sliceById(m_flexTargetSliceId) : nullptr;
                qCWarning(lcFlexGui)
                    << "Abstimmen verworfen — Zielscheibe"
                    << m_flexTargetSliceId
                    << "vorhanden" << (s != nullptr)
                    << "streamIndex" << (s ? s->streamIndex() : -999);
                return;
            }
            m_radioModel->tuneFlexSlice(hz);
        });

        // ── Die Spur der Frequenz (2026-10-30) ──────────────────────
        //
        // Der offene Befund vom 2026-10-29: beim Verbinden steht die
        // Scheibe reproduzierbar neben der Maschine, und zwar jedes Mal um
        // dieselben 80 kHz.
        //
        //     Geraet 7.1790 MHz -> Scheibe las 7.2590 MHz
        //     Geraet 7.3086 MHz -> Scheibe las 7.3886 MHz
        //     Geraet 7.0740 MHz -> Scheibe las 7.1540 MHz   (30.10., 10:03)
        //
        // Durch Lesen ist es nicht zu klaeren, und zwar aus einem
        // belegbaren Grund: im Protokoll steht jede Meldung der MASCHINE
        // ("Scheibe 0 auf 7074000 Hz", FlexRadioConnection), aber keine
        // einzige Zeile darueber, wer die Scheibe im Programm verstellt.
        // Am 30.10. meldete die Maschine 7.074 und blieb dort — der
        // Versatz entstand also auf unserer Seite, zwischen zwei Zeilen,
        // die sechs Millisekunden auseinander liegen.
        //
        // Also wird gemessen statt vermutet. Fuenfzehn Sekunden nach dem
        // Verbinden bekommt jede Frequenzaenderung der Scheibe eine Zeile:
        // alt, neu, Unterschied, der Stand der Wache (gesetzt = der Wert
        // kam von der Maschine, offen = das Programm selbst hat
        // geschrieben), dazu was die Maschine in diesem Augenblick meldet
        // und wo das Fenster steht. Der erste Start nach dieser Aenderung
        // benennt den Schreiber.
        //
        // Danach ist still: die Frist laeuft ab, und ohne Aenderung sagt
        // die Spur nichts. Siehe auch
        // feedback-abwesenheit-ist-keine-aussage — ein Lauf ohne Zeile
        // heiszt "nichts hat geschoben", nicht "die Spur hat versagt".
        disconnect(m_flexFreqSpur);
        m_flexSpurBisMs = QDateTime::currentMSecsSinceEpoch() + 15000;
        m_flexSpurLetzteHz = ziel->frequency();
        m_flexFreqSpur = connect(ziel, &SliceModel::frequencyChanged, this,
                [this](double hz) {
            if (m_shuttingDown || !m_radioModel) { return; }
            if (QDateTime::currentMSecsSinceEpoch() > m_flexSpurBisMs) { return; }
            const double vorher = m_flexSpurLetzteHz;
            m_flexSpurLetzteHz = hz;
            if (qFuzzyCompare(vorher, hz)) { return; }
            const double geraet = m_radioModel->flexGeraeteFrequenzHz();
            double fenster = 0.0;
            if (SpectrumWidget* sw = m_radioModel->spectrumWidget()) {
                fenster = sw->centerFrequency();
            }
            const double abstand = (geraet > 0.0) ? (hz - geraet) : 0.0;
            // Laut werden, wenn das Programm selbst von der Maschine
            // weglaeuft: genau das ist der gesuchte Fall. Das Urteil
            // steht in FlexFrequenzSpur.h, weil es geprueft wird —
            // eine Spur, die bei jedem Handgriff schreit, ist Laerm.
            const bool verdaechtig = FlexFrequenzSpur::istVersatz(
                hz, geraet, m_flexApplyingRemoteState);
            // Die Worte gehoeren in die `<<`-Kette, nicht in eine
            // QString-Variable (2026-10-30). `tst_oberflaeche_englisch`
            // sucht in src/gui nach deutschem Text in tr() /
            // QStringLiteral() und neutralisiert dabei Zeilen, die mit
            // `qC` oder `<<` beginnen — also genau die Protokollzeilen.
            // Ein `QString satz = QStringLiteral("… von der Maschine")`
            // steht auszerhalb dieser Ausnahme und zaehlte als deutsche
            // Beschriftung: die Decke 9 ging auf 11, zwei Scherben rot.
            // Dieselbe Falle wie am Morgen desselben Tages bei der
            // Sprechblase des 2D/3D-Knopfes — nur ist das hier wirklich
            // ein Protokolltext, also wird er wie jeder andere in
            // dieser Datei geschrieben.
            const auto mhz = [](double v) {
                return QString::number(v / 1.0e6, 'f', 6);
            };
            const auto khz = [](double v) {
                return QString::number(v / 1.0e3, 'f', 3);
            };
            if (verdaechtig) {
                // Verdaechtig heiszt per Definition: die Wache war offen.
                qCWarning(lcFlexGui).noquote().nospace()
                    << "Flex-Spur: Scheibe " << m_flexTargetSliceId << "  "
                    << mhz(vorher) << " -> " << mhz(hz) << " MHz ("
                    << khz(hz - vorher) << " kHz), Wache offen (vom "
                    << "Programm), Maschine meldet " << mhz(geraet)
                    << " MHz, Fenster " << mhz(fenster)
                    << " MHz — DAS IST DER VERSATZ: " << khz(abstand)
                    << " kHz neben der Maschine";
            } else {
                qCInfo(lcFlexGui).noquote().nospace()
                    << "Flex-Spur: Scheibe " << m_flexTargetSliceId << "  "
                    << mhz(vorher) << " -> " << mhz(hz) << " MHz ("
                    << khz(hz - vorher) << " kHz), Wache "
                    << (m_flexApplyingRemoteState
                            ? "gesetzt (von der Maschine)"
                            : "offen (vom Programm)")
                    << ", Maschine meldet " << mhz(geraet)
                    << " MHz, Fenster " << mhz(fenster) << " MHz";
            }
        });

    // ── Betriebsart und Filterbreite, derselbe Weg ──────────────
        //
        // Bis zum 2026-10-22 ging nur die Frequenz hinaus — und auch
        // die nicht (siehe oben). Mit dem Abstimmen allein hoerte der
        // Betreiber Stationen, aber keine verstaendliche Sprache:
        // „als waere es eine andere modulation". Das war sie auch.
        // Longpath zeigte LSB, die Maschine stand auf USB, weil die
        // Betriebsart nur EINMAL beim Verbinden gesetzt wurde.
        //
        // Dieselbe Wache wie bei der Frequenz: ein Zustandsschub der
        // Maschine darf den Befehl nicht zurueckwerfen, und eine
        // Scheibe mit echter DDC-Bindung wird nie gefuettert.
        disconnect(m_flexArtBand);
        m_flexArtBand = connect(ziel, &SliceModel::dspModeChanged, this,
                [this](DSPMode art) {
            if (m_shuttingDown || m_flexApplyingRemoteState) { return; }
            if (!flexControllableSlice()) { return; }
            m_radioModel->setFlexSliceMode(art);
        });

        disconnect(m_flexFilterBand);
        m_flexFilterBand = connect(ziel, &SliceModel::filterChanged, this,
                [this](int tief, int hoch) {
            if (m_shuttingDown || m_flexApplyingRemoteState) { return; }
            if (!flexControllableSlice()) { return; }
            m_radioModel->setFlexSliceFilter(tief, hoch);
        });

        // ── Rauschminderung: der Knopf schaltet die MASCHINE ────────
        //
        // Der Betreiber am 2026-10-28: „ich kann zwar klicken, aber
        // kein unterschied zu hören." Flex-Ton kommt fertig
        // demoduliert in den Mischer; Longpaths NR/NB/ANF sitzen im
        // WDSP-Empfangskanal, den dieser Ton nie durchläuft. Also
        // gehen die drei Schalter dorthin, wo gerechnet wird.
        //
        // Longpath kennt vier Verfahren und einen Dreifachschalter für
        // den Austaster, die Maschine je einen Schalter. Übersetzt
        // wird auf AN/AUS: welches Verfahren im Gerät läuft, bestimmt
        // das Gerät — eine NR2-Marke an einem fremden Algorithmus wäre
        // eine Beschriftung, die nichts bedeutet.
        auto rauschHinaus = [this]() {
            if (m_shuttingDown || m_flexApplyingRemoteState) { return; }
            if (!flexControllableSlice()) { return; }
            SliceModel* s = m_radioModel
                ? m_radioModel->sliceById(m_flexTargetSliceId) : nullptr;
            if (!s) { return; }

            // ── Wer rechnet: die Maschine oder Longpath? ─────────────
            //
            // Betreiber am 2026-10-28: „aber jeder nr sollte anders
            // funktionieren! das ist bei anvelina ja schon!" Also
            // teilen sich die Steckplaetze auf:
            //
            //   NR1 / NR2 / NR4  die Maschine (anr, emnr und sbnr sind
            //                    WDSP-Einheiten und haben auf fertigem
            //                    Ton kein Gegenstueck)
            //   NR3              rnnoise in Longpath
            //   MNR              Apple Accelerate in Longpath
            //
            // Nie beide zugleich: zwei Minderungen hintereinander
            // klingen nach Eimer, und keiner der beiden Regler wuerde
            // dann noch das tun, was auf ihm steht.
            const NrSlot gewaehlt = s->activeNr();
            const bool eigenesVerfahren =
                (gewaehlt == NrSlot::NR3 || gewaehlt == NrSlot::MNR);

            if (AudioEngine* ae = m_radioModel->audioEngine()) {
                ae->setExternalAudioNr(s->sliceIndex(),
                                       eigenesVerfahren ? gewaehlt : NrSlot::Off,
                                       1.0f);
            }

            // ── Jedes Verfahren an SEINEN Minderer (2026-11-01) ──
            //
            // Bis heute ging fuer NR1, NR2 und NR4 dasselbe `nr=1`
            // hinaus -- dreimal derselbe Klang, obwohl die Maschine
            // fuenf Verfahren fuehrt. Jetzt trifft jeder Steckplatz
            // den seinen (`RadioModel::flexNrSchluessel`).
            //
            // Rauschsperre fuer Knacken (nb) und Auto-Notch (anf)
            // bleiben daneben, wie bisher.
            m_radioModel->sendeFlexNrVerfahren(
                eigenesVerfahren ? NrSlot::Off : gewaehlt);
            if (!eigenesVerfahren) {
                m_radioModel->sendeFlexNrStufe(gewaehlt, s->flexNrLevel());
            }
            m_radioModel->setFlexSliceNoiseNbAnf(
                s->nbMode() != NbMode::Off, s->anfEnabled());
            // Und die Staerke der Kerbe gleich mit: sie steht unter
            // einem eigenen Schluessel (anf_level) und war bis
            // 2026-11-04 nie gesetzt -- die Maschine blieb auf ihren
            // 50, egal was der Schnellgriff zeigte.
            if (s->anfEnabled()) {
                m_radioModel->sendeFlexAnfStufe(s->flexAnfLevel());
            }
        };
        disconnect(m_flexNrBand);
        m_flexNrBand = connect(ziel, &SliceModel::activeNrChanged, this,
                               [rauschHinaus](NrSlot) { rauschHinaus(); });
        disconnect(m_flexNbBand);
        m_flexNbBand = connect(ziel, &SliceModel::nbModeChanged, this,
                               [rauschHinaus](NbMode) { rauschHinaus(); });
        // ── Der Schnellgriff zeigt am Flex die Maschine ─────────────
        //
        // Dort stehen sonst WDSP-Werte, die nichts tun. Der Name
        // kommt aus derselben Zuordnung, die auch den Befehl baut --
        // zwei Quellen wuerden frueher oder spaeter auseinanderlaufen.
        if (m_commandBar) {
            m_commandBar->setMaschinenNr([](NrSlot slot) {
                return RadioModel::flexNrSchluessel(slot);
            });
        }

        disconnect(m_flexNrStufeBand);
        m_flexNrStufeBand = connect(ziel, &SliceModel::flexNrLevelChanged, this,
                                    [rauschHinaus](int) { rauschHinaus(); });
        disconnect(m_flexAnfBand);
        m_flexAnfBand = connect(ziel, &SliceModel::anfEnabledChanged, this,
                                [rauschHinaus](bool) { rauschHinaus(); });
        disconnect(m_flexAnfStufeBand);
        m_flexAnfStufeBand = connect(ziel, &SliceModel::flexAnfLevelChanged, this,
                                     [rauschHinaus](int) { rauschHinaus(); });

        // ── Die Rauschsperre gehoert in dieselbe Reihe (2026-10-31) ──
        //
        // Sie lag bisher bei `RadioModel::wireSliceSignals`, zusammen
        // mit dem ganzen uebrigen DSP-Bedienfeld der Scheibe. Diese
        // Funktion steigt aber in ihrer zweiten Zeile aus:
        //
        //     if (!slice || !m_connection) { return; }
        //
        // `m_connection` ist die OpenHPSDR-Verbindung -- am Flex immer
        // null. Am Geraet gemessen (Martins Protokoll, 2026-10-31):
        //
        //     23:03:40.740  SQL-Knopf an (Scheibe 0)
        //     23:03:41.945  SQL-Regler 17 (Scheibe 0)
        //     ... und danach keine einzige Zeile "Flex: Rauschsperre"
        //
        // Knopf und Regler erreichten das Modell. Das Modell erreichte
        // die Maschine nie, weil an dieser Scheibe gar nichts haengt.
        // Darum hier, wo der Flex-Weg alles andere auch selbst
        // verdrahtet -- Art, Flanken, Rauschminderung.
        auto sperreHinaus = [this, ziel]() {
            if (m_shuttingDown || !m_radioModel) { return; }
            // ── Auto-SQL (2026-11-08) ───────────────────────────────
            //
            // In AUTO stellt der Pegel der Scheibe die Stufe, nicht der
            // Regler (FlexAutoRauschsperre.h) -- dann geht hier nichts
            // hinaus, sonst ueberschriebe die Hand-Stufe die Automatik.
            const bool automatik = ziel->ssqlEnabled() && ziel->ssqlAuto();
            m_radioModel->setFlexAutoRauschsperre(automatik);
            if (automatik) { return; }
            m_radioModel->sendeFlexRauschsperre(ziel->ssqlEnabled(),
                                                ziel->ssqlThresh());
        };
        disconnect(m_flexSqlAnBand);
        m_flexSqlAnBand = connect(ziel, &SliceModel::ssqlEnabledChanged, this,
                                  [sperreHinaus](bool) { sperreHinaus(); });
        disconnect(m_flexSqlStufeBand);
        m_flexSqlStufeBand = connect(ziel, &SliceModel::ssqlThreshChanged, this,
                                     [sperreHinaus](double) { sperreHinaus(); });
        disconnect(m_flexSqlAutoBand);
        m_flexSqlAutoBand = connect(ziel, &SliceModel::ssqlAutoChanged, this,
                                    [sperreHinaus](bool) { sperreHinaus(); });
        // Der Knopf bietet AUTO an, solange eine Flex steht; der Regler
        // zeigt dann den gespeicherten Abstand.
        if (m_rxApplet) {
            m_rxApplet->setAutoSqlMarge(m_radioModel->flexAutoMarge());
            m_rxApplet->setAutoSqlVerfuegbar([this]() {
                return !m_shuttingDown && m_radioModel
                    && m_radioModel->istFlexVerbunden();
            });
            connect(m_rxApplet, &RxApplet::autoSqlMargeGeaendert,
                    m_radioModel, &RadioModel::setFlexAutoMarge,
                    Qt::UniqueConnection);
            connect(m_radioModel, &RadioModel::flexAutoRauschsperreGestellt,
                    m_rxApplet, &RxApplet::setAutoSqlStand,
                    Qt::UniqueConnection);
        }

        // ── Und die uebrigen Regler der Scheibe (2026-11-01) ────────
        //
        // Dieselbe Reihe, derselbe Grund: `wireSliceSignals` steigt
        // bei `!m_connection` aus (OpenHPSDR), also haengt am Flex
        // nichts davon. Jeder Schluessel ist am Geraet gelesen, nicht
        // geraten -- `FlexRadioConnection.h` sagt, wie.
        //
        // Was hier FEHLT und warum:
        //   AGC-Schwelle  unsere Zahl ist eine Schwelle in dBm, die
        //                 der Maschine laeuft 0..100. Ohne Quelle
        //                 fuer die Umrechnung wird nicht geraten.
        //   APF-Frequenz  die Maschine hat nur `apf_level`, wir nur
        //                 eine Frequenz. Kein gemeinsamer Nenner.
        //   SNB           unter allen 101 Schluesseln der Scheibe
        //                 gibt es keinen dafuer.
        disconnect(m_flexAgcBand);
        m_flexAgcBand = connect(ziel, &SliceModel::agcModeChanged, this,
                                [this](AGCMode m) {
            if (m_shuttingDown || !m_radioModel) { return; }
            m_radioModel->sendeFlexAgcModus(m);
        });
        disconnect(m_flexAgcSchwelleBand);
        m_flexAgcSchwelleBand = connect(ziel, &SliceModel::agcThresholdChanged,
                                        this, [this](int s) {
            if (m_shuttingDown || !m_radioModel) { return; }
            m_radioModel->sendeFlexAgcSchwelle(s);
        });
        disconnect(m_flexTonpegelBand);
        m_flexTonpegelBand = connect(ziel, &SliceModel::afGainChanged, this,
                                     [this](int g) {
            if (m_shuttingDown || !m_radioModel) { return; }
            m_radioModel->sendeFlexTonpegel(g);
        });
        disconnect(m_flexStummBand);
        m_flexStummBand = connect(ziel, &SliceModel::mutedChanged, this,
                                  [this](bool v) {
            if (m_shuttingDown || !m_radioModel) { return; }
            m_radioModel->sendeFlexStumm(v);
        });
        disconnect(m_flexBalanceBand);
        m_flexBalanceBand = connect(ziel, &SliceModel::audioPanChanged, this,
                                    [this](double p) {
            if (m_shuttingDown || !m_radioModel) { return; }
            m_radioModel->sendeFlexBalance(p);
        });
        // RIT und XIT haben je zwei Teile (an/aus und Hertz). Die
        // Maschine nimmt beide in EINEM Befehl, also schickt jeder
        // der beiden Regler den vollen Stand -- sonst stuende nach
        // einem Schalten die alte Zahl daneben.
        auto ritHinaus = [this, ziel]() {
            if (m_shuttingDown || !m_radioModel) { return; }
            m_radioModel->sendeFlexRit(ziel->ritEnabled(), ziel->ritHz());
        };
        disconnect(m_flexRitAnBand);
        m_flexRitAnBand = connect(ziel, &SliceModel::ritEnabledChanged, this,
                                  [ritHinaus](bool) { ritHinaus(); });
        disconnect(m_flexRitHzBand);
        m_flexRitHzBand = connect(ziel, &SliceModel::ritHzChanged, this,
                                  [ritHinaus](int) { ritHinaus(); });
        auto xitHinaus = [this, ziel]() {
            if (m_shuttingDown || !m_radioModel) { return; }
            m_radioModel->sendeFlexXit(ziel->xitEnabled(), ziel->xitHz());
        };
        disconnect(m_flexXitAnBand);
        m_flexXitAnBand = connect(ziel, &SliceModel::xitEnabledChanged, this,
                                  [xitHinaus](bool) { xitHinaus(); });
        disconnect(m_flexXitHzBand);
        m_flexXitHzBand = connect(ziel, &SliceModel::xitHzChanged, this,
                                  [xitHinaus](int) { xitHinaus(); });
        // ── APF nur in CW (2026-11-01, am Geraet gemessen) ──────────
        //
        // Im Protokoll des Betreibers:
        //
        //   WRN: Befehl 23 abgelehnt, Code 0x50000061:
        //        "slice set 0 apf=0"
        //
        // Die Scheibe war in Betrieb und auf LSB; andere Befehle an
        // dieselbe Scheibe gingen durch. Die Maschine nimmt `apf`
        // also nicht in jeder Betriebsart an -- bei ihr ist das
        // Spitzenfilter eine CW-Hilfe. Statt bei jedem Klick eine
        // Ablehnung zu ernten, geht es nur dort hinaus, wo sie es
        // annimmt; sonst sagt eine Zeile, warum nicht.
        //
        // (Dasz es GENAU an CW haengt, ist aus der Ablehnung
        // geschlossen, nicht aus einer Quelle. Am Geraet in CW noch
        // zu bestaetigen.)
        auto apfHinaus = [this, ziel]() {
            if (m_shuttingDown || !m_radioModel) { return; }
            const DSPMode art = ziel->dspMode();
            if (art != DSPMode::CWL && art != DSPMode::CWU) {
                qCInfo(lcFlexGui) << "Flex: APF nur in CW — die Maschine "
                                     "lehnt es sonst ab";
                return;
            }
            m_radioModel->sendeFlexApf(ziel->apfEnabled());
        };
        disconnect(m_flexApfBand);
        m_flexApfBand = connect(ziel, &SliceModel::apfEnabledChanged, this,
                                [apfHinaus](bool) { apfHinaus(); });

        // Und einmal gleich jetzt denselben Weg gehen, damit die
        // Maschine mit dem uebereinstimmt, was im Fenster steht --
        // und noch einmal, sobald sie ihre Scheibe gemeldet hat. Der
        // erste Griff faellt naemlich in die 143-ms-Luecke vom
        // 2026-10-31, in der es die Scheibennummer noch nicht gibt.
        auto reglerHinaus = [this, ziel, apfHinaus]() {
            if (m_shuttingDown || !m_radioModel) { return; }
            m_radioModel->sendeFlexAgcModus(ziel->agcMode());
            m_radioModel->sendeFlexAgcSchwelle(ziel->agcThreshold());
            m_radioModel->sendeFlexTonpegel(ziel->afGain());
            m_radioModel->sendeFlexStumm(ziel->muted());
            m_radioModel->sendeFlexBalance(ziel->audioPan());
            m_radioModel->sendeFlexRit(ziel->ritEnabled(), ziel->ritHz());
            m_radioModel->sendeFlexXit(ziel->xitEnabled(), ziel->xitHz());
            apfHinaus();
        };
        disconnect(m_flexReglerNachtrag);
        m_flexReglerNachtrag = connect(m_radioModel,
                                       &RadioModel::flexScheibennummerDa, this,
                                       [reglerHinaus]() { reglerHinaus(); });

        // Und einmal gleich jetzt, damit die Maschine mit dem
        // uebereinstimmt, was im Fenster steht — sonst gilt der
        // Unterschied bis zum ersten Drehen weiter.
        m_radioModel->setFlexSliceMode(ziel->dspMode());
        m_radioModel->setFlexSliceFilter(ziel->filterLow(), ziel->filterHigh());
        rauschHinaus();
        sperreHinaus();
        reglerHinaus();

        // ── Beim VERBINDEN gewinnt die MASCHINE ─────────────────────
        //
        // Zwei Dinge laufen hier gegeneinander: Longpath uebernimmt
        // zuerst Frequenz und Ausschnitt der Maschine, danach stellt
        // das Layoutprofil die gespeicherte Scheibe her — und die kann
        // auf einem ganz anderen Band stehen. Am Geraet gemessen am
        // 2026-10-23: die Maschine lieferte 7.065–7.265 MHz, das
        // Fenster stand nach dem Profil auf 14.150–14.300. KEINE
        // Ueberlappung, also hatte der Panadapter nichts zu zeichnen
        // (Bandmeldung: Rauschteppich −600 dBm, Hub 0 dB). Der
        // Betreiber: „oben alles schwarz".
        //
        // Der erste Versuch zog den PANADAPTER auf die Scheibe. Das
        // war verkehrt herum und sofort zu merken: der Betreiber hoerte
        // auf 80 m, und das Programm riss ihm die Maschine beim
        // Starten auf die 20 m aus dem gespeicherten Profil. Wer das
        // Geraet gerade von SmartSDR oder der Frontplatte aus benutzt,
        // soll davon nichts merken.
        //
        // Also andersherum: nach dem Profilaufbau wird die Frequenz
        // der MASCHINE in die Scheibe zurueckgeschrieben. Der
        // Panadapter stimmt dann ohnehin, denn er steht dort schon.
        // 1500 ms, weil der Profilaufbau in sechs Schritten laeuft und
        // beim Messen rund 600 ms brauchte.
        QTimer::singleShot(1500, this, [this]() {
            if (m_shuttingDown || !m_radioModel) { return; }
            SliceModel* s = flexControllableSlice();
            if (!s) { return; }
            const double geraet = m_radioModel->flexGeraeteFrequenzHz();
            if (!(geraet > 0.0)) { return; }
            if (qFuzzyCompare(geraet, s->frequency())) { return; }
            qCInfo(lcFlexGui).nospace()
                << "Flex: das Profil stellte " << (s->frequency() / 1.0e6)
                << " MHz her, die Maschine steht auf " << (geraet / 1.0e6)
                << " MHz — die Maschine gewinnt";
            // ── Aber nicht auszerhalb des Bandes (2026-10-29) ───────
            //
            // Betreiber, an diesem Abend zum sechsten Mal: „wieder
            // falsches band". Die Maschine stand auf 7.3086 MHz — in
            // Region 1 kein 40 m mehr, sondern Rundfunk. „Die Maschine
            // gewinnt" hielt ihn also zuverlaessig dort fest, wo er
            // nicht sein wollte, und die Flex meldete folgerichtig
            // `NOT_READY reason=OUT_OF_BAND`.
            //
            // Die Regel, die er genannt hat: „bandwechsel sollte immer
            // in das amateurfunkband springen, nicht daneben." Also
            // gewinnt die Maschine nur, solange sie auf einer
            // Amateurfrequenz steht. Steht sie daneben, zieht Longpath
            // sie auf das, was im Gedaechtnis des zuletzt benutzten
            // Bandes steht — und wenn auch das daneben liegt, auf die
            // Startfrequenz des Bandes.
            //
            // Von Hand darf er weiterhin hinfahren, wohin er will:
            // dieser Griff sitzt nur im Verbinden.
            const Band bandDerMaschine = bandFromFrequency(geraet);
            double tief = 0.0, hoch = 0.0;
            const bool aufAmateurband = bandRangeHz(bandDerMaschine, tief, hoch);
            double ziel = aufAmateurband
                              ? BandDefaults::frequenzImBand(bandDerMaschine, geraet)
                              : 0.0;

            if (!aufAmateurband) {
                const auto letztes =
                    SliceModel::loadLastBandFromSettings(s->sliceIndex());
                const Band zurueck = letztes.value_or(Band::Band40m);
                const QString schluessel =
                    QStringLiteral("Slice%1/Band%2/Frequency")
                        .arg(s->sliceIndex())
                        .arg(bandKeyName(zurueck));
                const double gemerkt =
                    AppSettings::instance().value(schluessel, 0.0).toDouble();
                ziel = BandDefaults::frequenzImBand(
                    zurueck,
                    gemerkt > 0.0 ? gemerkt
                                  : BandDefaults::seedFor(zurueck).frequencyHz);
                qCWarning(lcFlexGui).nospace()
                    << "Flex: die Maschine steht auf " << (geraet / 1.0e6)
                    << " MHz — kein Amateurband. Es wird auf "
                    << (ziel / 1.0e6) << " MHz gestellt ("
                    << bandKeyName(zurueck) << ", zuletzt benutzt)";
            }

            if (ziel > 0.0 && !qFuzzyCompare(ziel, geraet)) {
                // OHNE Wache: das soll als Befehl an die Maschine
                // hinausgehen, sie steht ja falsch.
                s->setFrequency(ziel);
            } else {
                // Die Wache setzen: sonst schickt die Scheibe die eben
                // uebernommene Frequenz als Befehl zurueck.
                m_flexApplyingRemoteState = true;
                s->setFrequency(geraet);
                m_flexApplyingRemoteState = false;
            }

            // ── Und das Seitenband zum neuen Band (2026-10-23) ───────
            //
            // Betreiber: „80 und 160 sowie 40 muessen immer automatisch
            // bei bandauswahl LSB sein". Dass die Maschine hier die
            // Frequenz durchsetzt, IST eine Bandauswahl — nur eine, die
            // nicht ueber den Bandknopf laeuft und damit auch nicht
            // ueber `SliceModel::restoreFromSettings`, wo die Regel
            // sonst greift. Am Geraet kam 40 m deshalb in USB: das
            // Profil stand auf 20 m, die Flex zog auf 7,148 MHz, und
            // das USB des 20-m-Platzes blieb stehen.
            //
            // NICHT in `SliceModel::setFrequency` allgemein: `setDspMode`
            // setzt dort die Filterbreite auf die Vorgabe der neuen
            // Betriebsart zurueck und hat mitten im Wiederherstellen
            // drei Pruefstaende der Ablage umgeworfen.
            //
            // Ohne Wache: diese Betriebsart SOLL zur Maschine hinaus.
            const DSPMode gerichtet =
                BandDefaults::korrigiereSeitenband(
                    bandFromFrequency(geraet), s->dspMode());
            if (gerichtet != s->dspMode()) {
                qCInfo(lcFlexGui).nospace()
                    << "Flex: " << bandLabel(bandFromFrequency(geraet))
                    << " gehoert auf " << (gerichtet == DSPMode::LSB
                                           ? "LSB" : "USB")
                    << " — Seitenband gerichtet";
                s->setDspMode(gerichtet);
            }
        });

    // ── Betriebsart und Filterbreite, derselbe Weg ──────────────
        //
        // Bis zum 2026-10-22 ging nur die Frequenz hinaus — und auch
        // die nicht (siehe oben). Mit dem Abstimmen allein hoerte der
        // Betreiber Stationen, aber keine verstaendliche Sprache:
        // „als waere es eine andere modulation". Das war sie auch.
        // Longpath zeigte LSB, die Maschine stand auf USB, weil die
        // Betriebsart nur EINMAL beim Verbinden gesetzt wurde.
        //
        // Dieselbe Wache wie bei der Frequenz: ein Zustandsschub der
        // Maschine darf den Befehl nicht zurueckwerfen, und eine
        // Scheibe mit echter DDC-Bindung wird nie gefuettert.
        disconnect(m_flexArtBand);
        m_flexArtBand = connect(ziel, &SliceModel::dspModeChanged, this,
                [this](DSPMode art) {
            if (m_shuttingDown || m_flexApplyingRemoteState) { return; }
            if (!flexControllableSlice()) { return; }
            m_radioModel->setFlexSliceMode(art);
        });

        disconnect(m_flexFilterBand);
        m_flexFilterBand = connect(ziel, &SliceModel::filterChanged, this,
                [this](int tief, int hoch) {
            if (m_shuttingDown || m_flexApplyingRemoteState) { return; }
            if (!flexControllableSlice()) { return; }
            m_radioModel->setFlexSliceFilter(tief, hoch);
        });

        // Und einmal gleich jetzt, damit die Maschine mit dem
        // uebereinstimmt, was im Fenster steht — sonst gilt der
        // Unterschied bis zum ersten Drehen weiter.
        m_radioModel->setFlexSliceMode(ziel->dspMode());
        m_radioModel->setFlexSliceFilter(ziel->filterLow(), ziel->filterHigh());

        // ── Panadapter und Scheibe muessen beim VERBINDEN zusammen ──
        //
        // Beim Verbinden laufen zwei Dinge gegeneinander: Longpath
        // uebernimmt zuerst den Ausschnitt der Maschine, danach stellt
        // das Layoutprofil die gespeicherte Scheibe wieder her — und
        // die kann auf einem ganz anderen Band stehen.
        //
        // Am Geraet gemessen am 2026-10-23: die Maschine lieferte
        // 7.065–7.265 MHz, das Fenster stand nach dem Profil auf
        // 14.150–14.300. KEINE Ueberlappung, also hatte der
        // Panadapter nichts zu zeichnen (Bandmeldung: Rauschteppich
        // −600 dBm, Hub 0 dB) und der Wasserfall war einfarbig. Der
        // Betreiber: „oben alles schwarz".
        //
        // Darum einmal nachziehen, NACHDEM das Profil durch ist. Die
        // Scheibe gewinnt: sie ist das, was der Betreiber sieht und
        // hoert. 1500 ms, weil der Profilaufbau in sechs Schritten
        // laeuft und beim Messen rund 600 ms brauchte.
        QTimer::singleShot(1500, this, [this]() {
            if (m_shuttingDown) { return; }
            SliceModel* s = flexControllableSlice();
            if (!s || !m_radioModel) { return; }
            // ── Die Maschine gibt die Frequenz vor (2026-10-29) ─────
            //
            // Hier stand `s->frequency()`. Am Geraet gemessen, zwei
            // Starts hintereinander:
            //
            //   Geraet 7.1790 MHz -> Scheibe las 7.2590 MHz
            //   Geraet 7.3086 MHz -> Scheibe las 7.3886 MHz
            //
            // Jedes Mal genau 80 kHz mehr, und jedes Mal drei
            // Millisekunden NACHDEM der Zwilling oben die Frequenz der
            // Maschine in die Scheibe zurueckgeschrieben hatte. Woher
            // die 80 kHz kommen, ist noch offen — der Nachzug schob
            // damit aber das Geraet aus dem Band, und der Betreiber
            // stand beim Start fuenfmal hintereinander auszerhalb.
            //
            // Also wird hier genommen, was die Maschine SELBST sagt.
            // Dieselbe Regel steht drei Zeilen hoeher: die Maschine
            // gewinnt. Nur die Scheibe zu fragen hiesz, dem Profil zu
            // glauben statt dem Geraet.
            const double geraet = m_radioModel->flexGeraeteFrequenzHz();
            const double hz = (geraet > 0.0) ? geraet : s->frequency();
            if (!(hz > 0.0)) { return; }
            if (geraet > 0.0 && !qFuzzyCompare(geraet, s->frequency())) {
                qCWarning(lcFlexGui).nospace()
                    << "Flex: Scheibe und Maschine gehen auseinander — "
                    << "Scheibe " << (s->frequency() / 1.0e6)
                    << " MHz, Maschine " << (geraet / 1.0e6)
                    << " MHz; es gilt die Maschine";
                // Und die Scheibe nachziehen, sonst zeigt die Bedienung
                // eine Frequenz an, auf der die Maschine nicht steht —
                // und schickt sie beim ersten Handgriff als Befehl
                // hinaus. Die Wache verhindert genau dieses Echo.
                m_flexApplyingRemoteState = true;
                s->setFrequency(geraet);
                m_flexApplyingRemoteState = false;
            }
            // Welche Scheibe, und was die Maschine selbst sagt: am
            // 2026-10-29 zog dieser Nachzug den Panadapter auf 7.259
            // MHz, obwohl die Maschine sechs Millisekunden vorher mit
            // 7.179 MHz gemeldet wurde („die Maschine gewinnt"). Ohne
            // die Kennung ist nicht zu sehen, ob hier eine ANDERE
            // Scheibe gelesen wird als die, die den Wert bekam.
            qCInfo(lcFlexGui).nospace()
                << "Flex: Panadapter auf die Scheibe gezogen — "
                << (hz / 1.0e6) << " MHz (Scheibe " << s->sliceIndex()
                << ", Geraet steht auf "
                << (m_radioModel->flexGeraeteFrequenzHz() / 1.0e6) << " MHz)";
            m_radioModel->zieheFlexPanadapterAuf(hz);
        });
        if (AudioEngine* ae = m_radioModel->audioEngine()) {
            // ── Den Ausgang anwerfen (2026-10-22) ────────────────────
            //
            // `AudioEngine::start()` oeffnet den Lautsprecherausgang und
            // laesst den Abfluss aus dem MasterMixer anlaufen. Im ganzen
            // Programm steht der Aufruf an genau EINER Stelle: im
            // WDSP-Init-Rueckruf nach dem Verbinden mit einem EIGENEN
            // Funkgeraet (RadioModel.cpp). Beim Flex gibt es den nicht —
            // die Maschine rechnet selbst —, also lief der Ausgang nie
            // an.
            //
            // Gemessen am 2026-10-22 an Martins FLEX-8400: der Ton kam
            // bis in die Mischung und weiter nicht. Sonde am Abfluss,
            // alle fuenf Sekunden:
            //   Scheibe 0, mixed 256, Spitze 0,07, Vol 0,82,
            //   Stumm false, Leerdrains 0, Bus FALSE
            // Alles richtig bis zum letzten Schritt, und dort kein
            // offener Ausgang. Der Betreiber hoerte eine Stunde lang
            // nichts, obwohl die Maschine 30 987 Tonpakete geschickt
            // hatte.
            //
            // SunSDR hat denselben Griff und dieselbe Begruendung
            // (MainWindow_SunSdr.cpp, 2026-08-24) — beim Flex ist er
            // schlicht vergessen worden. `start()` schuetzt sich gegen
            // doppeltes Anlaufen (`if (m_running) return;`), ist hier
            // also gefahrlos: laeuft der Ausgang schon durch ein eigenes
            // Geraet, geschieht nichts.
            ae->start();
            // Opportunistisch im Mix: ein Netzaussetzer zum Flex darf
            // nicht den ganzen Ton anhalten.
            ae->setFlexAudioSourceEnabled(m_flexTargetSliceId, true);
        }
        qCInfo(lcFlexGui) << "Flex-Ton auf Scheibe" << m_flexTargetSliceId;

        // ── Senden vorbereiten, NICHT tasten (2026-10-23, Stufe 2) ───
        //
        // Drei Dinge muessen stehen, bevor ein `xmit` ueberhaupt Sinn
        // hat, und keines davon sendet:
        //
        //   1. Die Maschine muss wissen, WELCHE Scheibe sendet —
        //      ohne `slice set <n> tx=1` weist sie `xmit` ab.
        //   2. Der Sendetonstrom (`stream create type=dax_tx`) muss
        //      angelegt sein; seine Kennung gehoert in jedes Tonpaket.
        //   3. Der Senderzweig muss auf DAX stehen. Sonst nimmt die
        //      Maschine das Mikrofon an ihrer eigenen Buchse und wirft
        //      jedes unserer Pakete weg — getastet wird, gesendet wird
        //      Stille (AetherSDR AudioEngine.cpp:8954 [@d58e2b8a]).
        //
        // Punkt 3 steht hier bewusst NICHT: `dax=1` nimmt dem
        // Betreiber das Mikrofon an der Maschine weg, und das gehoert
        // nicht ins Verbinden. Es wird erst beim Tasten gesetzt.
        m_radioModel->setzeFlexSendescheibe(
            QString::number(m_flexTargetSliceId));
        m_radioModel->legeFlexSendetonstromAn();
        // Der FT8-Sender gehoert hierher, nicht in den WDSP-Aufbau:
        // den gibt es beim Flex nicht. Still, bis jemand sendet.
        m_radioModel->stelleFt8SenderBereit();
        verdrahteFlexSenden();

        // ── Die Maschine folgt dem Blick des Betreibers (2026-10-27) ─
        //
        // Betreiber: „immer beim verschieben kommt es zu den
        // problemen". Und so entstehen sie: schiebt er das Bild, wandert
        // `m_centerHz` — der Wasserfall folgt mit (die Kachel wird ueber
        // die Frequenz abgebildet), die Bild-FFT aber deckt weiterhin
        // nur den Ausschnitt der MASCHINE. Je weiter er schiebt, desto
        // weniger ueberlappen die beiden, und am Ende steht im
        // Wasserfall etwas, wovon das Spektrum nichts weiss.
        //
        // Richtig ist, dass die Maschine dem Blick folgt: bei SmartSDR
        // IST der Panadapter das Fenster. Also seine Mitte hinausgeben.
        //
        // Entprellt: beim Ziehen kommt `centerChanged` je Bildpunkt,
        // und jeder Befehl waere ein Umlauf zur Maschine. 250 ms nach
        // dem letzten Ruck genuegt — der Bildstrom laeuft derweil
        // weiter, nur eben noch auf dem alten Ausschnitt.
        if (!m_flexPanNachzug) {
            m_flexPanNachzug = new QTimer(this);
            m_flexPanNachzug->setSingleShot(true);
            m_flexPanNachzug->setInterval(250);
            connect(m_flexPanNachzug, &QTimer::timeout, this, [this]() {
                if (m_shuttingDown || !m_radioModel) { return; }
                if (m_flexTargetSliceId < 0) { return; }
                SpectrumWidget* sw = m_radioModel->spectrumWidget();
                if (!sw) { return; }
                const double mitte = sw->centerFrequency();
                const double panMitte =
                    (m_flexSpanLowHz + m_flexSpanHighHz) * 0.5;
                // ── Das Totband muss KLEIN sein (2026-10-27) ─────
                //
                // Hier stand ein Zehntel der Breite, „eine
                // Verschiebung, die man ohnehin nicht sieht". Man
                // sieht sie doch — nur nicht als Verschiebung,
                // sondern als DEHNUNG.
                //
                // Gemessen: Fenster 7,0961 MHz, Panadapter der
                // Maschine 7,0788 — 17,3 kHz Unterschied, knapp unter
                // dem alten Totband von 20 kHz. Das Fenster ragt damit
                // oben ueber die Bild-FFT hinaus, `visibleBinRange`
                // klemmt den Bin-Ausschnitt am letzten Bin ab, und der
                // geklemmte Rest (182,7 statt 200 kHz) wird ueber die
                // GANZE Breite gezogen. Das ergibt einen Maszstab von
                // 200/182,7 = 1,095 mit dem Drehpunkt am linken Rand —
                // gemessen 1,072, linke Haelfte 42 Punkte Versatz,
                // rechte 83. Genau die Form, die der Betreiber als
                // „nicht auf allen frequenzen gleich" beschreibt, und
                // genau der Fall „immer beim verschieben".
                //
                // Ein Prozent reicht: 2 kHz Rest ergeben einen
                // Maszstab von 1,01, also am Bildrand ein Kilohertz.
                const double breite = m_flexSpanHighHz - m_flexSpanLowHz;
                if (!FlexFensterZug::nachzugZieht(mitte, panMitte, breite)) {
                    return;
                }
                qCInfo(lcFlexGui).nospace()
                    << "Flex: Panadapter folgt dem Bild — " << (mitte / 1.0e6)
                    << " MHz (war " << (panMitte / 1.0e6) << ")";
                m_radioModel->zieheFlexPanadapterAuf(mitte);
            });
        }
        disconnect(m_flexBlickBand);
        if (SpectrumWidget* sw = m_radioModel->spectrumWidget()) {
            m_flexBlickBand = connect(sw, &SpectrumWidget::centerChanged,
                                      this, [this, sw](double) {
                if (m_shuttingDown || m_flexTargetSliceId < 0) { return; }
                // ── Nur ein Zug wirft den Nachzug an (2026-10-30) ───
                //
                // Scrollt das Fenster nur dem VFO an den Rand nach, so
                // ist seine neue Mitte 80 kHz neben der Scheibe — und
                // der Nachzug zog den Panadapter der Maschine genau
                // dorthin, gegen die Zentrierung, die `RadioModel`
                // gerade erst gesendet hatte. Am Geraet gemessen, der
                // Wechsel auf FT8 um 16:06:04: die spaetere Quittung
                // gewann, das Signal stand wieder neben der Mitte.
                //
                // Stoppen genuegt nicht — der Nachzug hatte seinen
                // Befehl da schon gesendet. Er darf nicht anlaufen.
                if (!FlexFensterZug::nachzugWirdAngeworfen(
                        sw->mitteFolgtDemVfo())) {
                    return;
                }
                if (m_flexPanNachzug) { m_flexPanNachzug->start(); }
            });
        }

        // ── Die Maske geht von selbst zu (2026-10-22) ───────────────
        //
        // Der Selbstschluss haengt an `onConnectionStateChanged`, und
        // das kommt von der OpenHPSDR-Verbindung. Die Flex hat keine,
        // also blieb die Maske stehen, und hinter ihr lagen ALLE
        // schwebenden Fenster samt Panadapter. Der Betreiber:
        // „warum muss ich immer close klicken um auf das funkgeraet zu
        // kommen". Hier ist die Stelle, an der die Flex wirklich steht.
        QTimer::singleShot(1000, this, [this]() {
            if (m_shuttingDown) { return; }
            if (m_connectionPanel && m_connectionPanel->isVisible()
                && m_radioModel && m_radioModel->istIrgendwieVerbunden()) {
                qCInfo(lcFlexGui) << "Verbindungsmaske schliesst sich selbst";
                m_connectionPanel->accept();
            }
        });

        // ── Den Messwert-Umlauf anwerfen (2026-10-22) ────────────────
        //
        // Er wird sonst NUR aus `WdspEngine::initializedChanged`
        // gestartet, und auch dort nur, wenn `rxChannelForSlice(0)`
        // etwas liefert. Beim Flex gibt es diese Kette nicht — die
        // Maschine rechnet selbst —, also lief der Umlauf nie, und das
        // S-Meter blieb ohne Zeiger stehen, obwohl die Maschine den
        // Pegel dreissigmal in der Sekunde schickt.
        //
        // `start()` auf einem schon laufenden Zeitgeber ist folgenlos;
        // die fremde Quelle (MainWindow, setFremdeEmpfangsquelle) gibt
        // ohne Flex-Messwert `nullopt`, der WDSP-Weg bleibt also
        // unberuehrt.
        if (m_meterPoller) { m_meterPoller->start(); }
        if (m_radioModel->meterPoller() == nullptr && m_meterPoller) {
            m_radioModel->setMeterPoller(m_meterPoller);
        }

        // Der Panadapter zeigt ab jetzt die fremde Quelle; Bins vom
        // (nicht vorhandenen) Geraet werden damit verworfen.
        if (SpectrumWidget* sw = m_radioModel->spectrumWidget()) {
            sw->setKiwiDisplaySource(true);
        }
    });

    // ── Frequenz, beide Richtungen ──────────────────────────────────
    //
    // Bis zum 2026-10-22 ging hier gar nichts: `flexSliceFrequencyChanged`
    // wurde ausgesendet und von niemandem gehoert. Der VFO zeigte nie,
    // worauf die Maschine stand, und drehen liess sie sich nicht.
    //
    // Die Sicherheitsregel gilt auch hier, nicht nur fuer Ton und Bild:
    // gesteuert wird ausschliesslich ueber `flexControllableSlice()`,
    // also nie eine Scheibe mit echter DDC-Bindung
    // (`streamIndex() >= 0`). Der SunSDR-Durchgang vom 2026-08-24 wurde
    // genau deshalb nachgeschaerft — eine Fassung, die nur die Steuerung
    // absicherte, war zu wenig.
    connect(m_radioModel, &RadioModel::flexSliceFrequencyChanged, this,
            [this](int, double hz) {
        if (m_shuttingDown) { return; }
        SliceModel* slice = flexControllableSlice();
        if (!slice) { return; }
        // Von der Maschine nach innen: dabei die Wache setzen, sonst
        // schickt die Scheibe die eben empfangene Frequenz zurueck.
        m_flexApplyingRemoteState = true;
        slice->setFrequency(hz);
        m_flexApplyingRemoteState = false;
    });

    // ── Rückweg: die Maschine sagt, was wirklich an ist ─────────────
    //
    // Nötig, weil der Schalter auch an der Maschine selbst oder in
    // SmartSDR umgelegt werden kann — und weil beim Verbinden gilt,
    // was dort steht, nicht was im Fenster steht. Die Wache
    // `m_flexApplyingRemoteState` hält den Hinweg an, solange wir den
    // Zustand der Maschine übernehmen; sonst käme unser eigenes Echo
    // als neuer Befehl zurück.
    connect(m_radioModel, &RadioModel::flexSliceNoiseChanged, this,
            [this](bool nr, bool nb, bool anf) {
        if (m_shuttingDown) { return; }
        SliceModel* slice = flexControllableSlice();
        if (!slice) { return; }
        m_flexApplyingRemoteState = true;
        // AUS ist eindeutig, AN nicht: die Maschine nennt ihr
        // Verfahren nicht, und Longpath hat vier. Steht hier schon
        // eines, bleibt es stehen — nur der leere Fall bekommt eine
        // Marke, damit der Knopf überhaupt leuchtet. NR2 ist die, die
        // das RX-Feld auch sonst als Erstes anbietet.
        // Läuft ein Verfahren von Longpath (NR3, MNR), steht der
        // Schalter der Maschine mit Absicht auf 0 — ihr `nr=0` darf
        // die Wahl des Betreibers dann NICHT ausknipsen. Ohne diese
        // Ausnahme schaltete sich NR3 im selben Atemzug wieder ab, in
        // dem es eingeschaltet wurde: wir stellen die Maschine aus,
        // sie meldet aus, und die Meldung loeschte die Marke
        // (2026-10-28).
        const NrSlot steht = slice->activeNr();
        const bool eigenesVerfahren =
            (steht == NrSlot::NR3 || steht == NrSlot::MNR);
        if (!nr) {
            if (!eigenesVerfahren) { slice->setActiveNr(NrSlot::Off); }
        } else if (steht == NrSlot::Off) {
            slice->setActiveNr(NrSlot::NR2);
        }
        if (!nb) {
            slice->setNbMode(NbMode::Off);
        } else if (slice->nbMode() == NbMode::Off) {
            slice->setNbMode(NbMode::NB);
        }
        slice->setAnfEnabled(anf);
        m_flexApplyingRemoteState = false;
    });

    // ── Ton ─────────────────────────────────────────────────────────
    connect(m_radioModel, &RadioModel::flexAudioSamplesReady, this,
            [this](quint32, const QVector<float>& verschraenkt) {
        if (verschraenkt.isEmpty()) { return; }

        // Zwei Zahlen, die aus dem Protokoll bisher nicht zu holen
        // waren: kommt waehrend des Sendens ueberhaupt Ton, und wie
        // lange schweigt er nach dem Loesen?
        if (m_flexSelbstGetastet) {
            ++m_flexTonImSenden;
        } else if (m_flexLoesenMs > 0) {
            const qint64 luecke =
                QDateTime::currentMSecsSinceEpoch() - m_flexLoesenMs;
            m_flexLoesenMs = 0;
            qCInfo(lcFlexGui).nospace()
                << "Flex: Empfang nach dem Loesen wieder da — " << luecke
                << " ms" << (luecke > 2000 ? "  (Hoerperiode angeknabbert)"
                                           : "");
        }

        SliceModel* slice = flexControllableSlice();
        if (!slice) { return; }
        AudioEngine* ae = m_radioModel ? m_radioModel->audioEngine() : nullptr;
        if (!ae) { return; }
        // Zwei Werte je Rahmen (Stereo verschraenkt), gemessen.
        ae->feedFlexAudioData(slice->sliceIndex(), verschraenkt.constData(),
                              verschraenkt.size() / 2, kFlexAudioRateHz);
    });

    // ── Bild ────────────────────────────────────────────────────────
    //
    // Die FFT-Bins kommen schon in dBm (FlexVitaStream rechnet sie mit
    // dem Maszstab um, den RadioModel am Geraet gesetzt hat — die
    // Maschine meldet `y_pixels` nie zurueck). Der Ausschnitt steht
    // NICHT im FFT-Paket: nur die Wasserfall-Kachel traegt ihre
    // Frequenzen. Bis eine Zeile durch ist, gibt es also keinen
    // Ausschnitt, und dann wird nichts gezeichnet — lieber kein Bild
    // als ein Bild an der falschen Frequenz.
    // ── Der Ausschnitt kommt vom Panadapter der Maschine ────────────
    //
    // Bis zum 2026-10-22 kam er aus der WASSERFALLKACHEL, und die hat
    // ihre eigene Bin-Zahl und ihren eigenen Rand: am Geraet gemessen
    // 230,6 kHz, waehrend der Panadapter 200 kHz breit war. Ueber den
    // falschen Ausschnitt gespannt steht jedes Signal bis zu 15 kHz
    // neben seiner Frequenz.
    connect(m_radioModel, &RadioModel::flexAusschnittGeaendert, this,
            [this](double tiefHz, double hochHz, bool eigenerZug) {
        if (m_shuttingDown || !(hochHz > tiefHz)) { return; }
        m_flexSpanLowHz  = tiefHz;
        m_flexSpanHighHz = hochHz;

        // ── Unser eigener Zug schlaegt den Nachzug (2026-10-30) ───
        //
        // Steht hier ein Zug von uns, dann ist er die juengere
        // Entscheidung, und ein noch laufender Nachzug wuerde ihn
        // zurueckdrehen. Genau das stand im Protokoll: 41.632 auf
        // 7.074 gezogen, 41.890 wieder auf 7.154 zurueck. Die
        // Herleitung steht in core/FlexFensterZug.h.
        if (eigenerZug && m_flexPanNachzug) { m_flexPanNachzug->stop(); }

        // Das Fenster folgt dem Ausschnitt der Maschine — auch wenn er
        // sich spaeter aendert (Bandwechsel, Zoom am Geraet, oder weil
        // `tuneFlexSlice` den Panadapter nachgezogen hat).
        if (SpectrumWidget* sw = m_radioModel
                ? m_radioModel->spectrumWidget() : nullptr) {
            const double mitte  = (tiefHz + hochHz) * 0.5;
            const double breite = hochHz - tiefHz;

            // ── Den Zoom des Betreibers nicht wegwerfen (2026-10-23) ──
            //
            // Hier stand `setFrequencyRange(mitte, breite)` immer. Die
            // Maschine schickt ihren Ausschnitt aber auch dann neu,
            // wenn sich nur die MITTE geaendert hat — nach jedem
            // Abstimmen, weil `tuneFlexSlice` den Panadapter nachzieht.
            // Damit sprang das Fenster bei jedem Doppelklick im
            // Panadapter auf die volle Breite zurueck. Betreiber:
            // „verkleinert sich das display und ich muss wieder mit
            // plus vergroeszern".
            //
            // Also: Breite nur uebernehmen, wenn die Maschine WIRKLICH
            // eine andere schickt (oder beim ersten Mal). Sonst nur um
            // dieselbe Strecke mitwandern und die eigene Breite
            // behalten.
            // ── Fremder Zug oder eigener (2026-10-30) ──────────
            //
            // Bis heute hing das nur an der Breite, und damit blieb
            // das Fenster auch bei UNSEREM eigenen Zug stehen — der
            // Nachzug schob den Panadapter danach wieder auf das
            // Fenster zurueck, und der Betreiber stand beim Start
            // 80 kHz neben dem Band. Das Urteil liegt jetzt geprueft
            // in core/FlexFensterZug.h; dort steht auch die Messung.
            const double alteBreite = m_flexFensterBreiteHz;
            const bool folgt = FlexFensterZug::fensterFolgt(
                breite, alteBreite, m_flexAusschnittUebernommen,
                eigenerZug);
            const bool nimmtBreite = FlexFensterZug::fensterNimmtBreite(
                breite, alteBreite, m_flexAusschnittUebernommen,
                eigenerZug);
            if (folgt && nimmtBreite) {
                sw->setFrequencyRange(mitte, breite);
            } else if (folgt) {
                // Nur die Mitte: die Breite ist die des Betreibers.
                sw->setCenterFrequency(mitte);
            }
            // Sonst: NICHTS. Eine reine Mittenverschiebung der
            // Maschine kommt fast immer davon, dass der Betreiber
            // gerade selbst abgestimmt hat — ihm das Fenster
            // hinterherzuziehen hiesze, seinen Zoom wegzuwerfen UND
            // (ohne CTUN, wo die Fenstermitte die Frequenz IST) ihn
            // ein zweites Mal zu verstimmen. Im Protokoll stand das
            // als Kette von sieben `tuneFlexSlice` in 150 ms.
            m_flexFensterMitteHz  = mitte;
            m_flexFensterBreiteHz = breite;

            if (!m_flexAusschnittUebernommen) {
                m_flexAusschnittUebernommen = true;
                if (!sw->waterfallNFAGCEnabled()) {
                    sw->setWaterfallNFAGCEnabled(true);
                }
            }
            qCInfo(lcFlexGui).nospace()
                << "Flex-Bild: Ausschnitt der Maschine uebernommen — "
                << (breite / 1.0e3) << " kHz um " << (mitte / 1.0e6)
                << " MHz";
        }
    });

    // ── Der Wasserfall kommt von der Maschine (2026-10-23) ──────────
    //
    // Die FLEX-8400 fuehrt ZWEI Bildstroeme, und AetherSDR zeichnet
    // auch aus beiden (docs/protocols/flex-wasserfall-aethersdr.md):
    //   0x8003  Spektrum, Bildzeilen -> dBm ueber y_pixels
    //   0x8004  Wasserfall, qint16/128 -> INTENSITAET, mit eigenem
    //           Frequenzbereich und eigenem Takt (`LineDurationMS`)
    //
    // Longpath zog bisher nur den ersten und baute den Wasserfall aus
    // den FFT-Bins nach. Der Betreiber am 2026-10-22: „das rot ist
    // eigenartig gewesen, auch die zeitliche laenge, bis der
    // wasserfall anfaengt". Beides folgt aus fremdem Takt und fremden
    // Schwellen.
    //
    // `FlexVita::tileColourValue()` gab es in Longpath schon —
    // geschrieben, nie benutzt.
    connect(m_radioModel, &RadioModel::flexWaterfallRowReady, this,
            [this](quint32, const QVector<quint16>& roh,
                   double lowFreqMhz, double highFreqMhz, quint32) {
        if (m_shuttingDown || roh.isEmpty()) { return; }
        if (!(highFreqMhz > lowFreqMhz)) { return; }
        SpectrumWidget* sw = m_radioModel ? m_radioModel->spectrumWidget()
                                          : nullptr;
        if (!sw || !sw->kiwiDisplaySource()) { return; }

        QVector<float> intensitaet(roh.size());
        for (int i = 0; i < roh.size(); ++i) {
            intensitaet[i] = FlexVita::tileColourValue(roh[i]);
        }
        sw->zeigeFremdeWasserfallzeile(intensitaet, lowFreqMhz * 1.0e6,
                                       highFreqMhz * 1.0e6);

        // ── KEIN Nachziehen der Mitte (2026-10-23, zurueckgebaut) ────
        //
        // Hier stand ein Nachzug der Panadaptermitte auf die
        // Kachelmitte. Er hat den gemessenen Versatz von 7,4 kHz
        // scheinbar geheilt (0 bis 1 Punkt auf 15 m) — aber nur,
        // solange er AUCH das Fenster mitzog. Ohne das sprang der
        // Versatz auf +106 Punkte = +18,7 kHz, also schlechter als
        // vorher. Und das Fenster darf er nicht anfassen: ohne CTUN
        // ist die Fenstermitte die Empfangsfrequenz, und der Nachzug
        // hat den Betreiber nachweislich verstimmt (sieben
        // `tuneFlexSlice` in 150 ms).
        //
        // Damit ist die Annahme widerlegt, der Zustandsschub der
        // Maschine sei schuld: die Bild-FFT deckt den Ausschnitt des
        // ZUSTANDS, nicht den der Kachel. Was gemessen wurde, war
        // etwas anderes — und die Messung selbst ist nicht zuverlaessig
        // genug fuer so einen Schluss: dieselbe Lage ergab im Abstand
        // von 30 s einmal -8 und einmal +107 Punkte. Auf einem Band
        // mit vielen aehnlichen Signalen rastet die Kreuzkorrelation
        // auf dem falschen Nachbarn ein.
        //
        // Die Messung bleibt im Protokoll, aber sie meldet jetzt auch,
        // wie DEUTLICH ihr bestes Ergebnis ist; erst daran laesst sich
        // ein echter Versatz von einem Fehlgriff unterscheiden.
    });

    connect(m_radioModel, &RadioModel::flexSpectrumReady, this,
            [this](quint32, const QVector<float>& dbm) {
        if (m_shuttingDown || dbm.isEmpty()) { return; }

        // 2026-10-21: bis hierher war jeder Befund belegt — die Pakete
        // kommen an, die Rahmen entstehen, der Maszstab steht, die
        // Maschine hat `display pan set ... 1024 x 1000` mit Code 0
        // quittiert. Gezeichnet wurde trotzdem nichts, und WELCHES der
        // drei Tore unten zuschlaegt, war von auszen nicht zu sehen.
        // Also sagt es das Protokoll — einmal je Grund, nicht je
        // Rahmen (es kommen 25 in der Sekunde).
        const auto meldeEinmal = [this](FlexBildGrund grund, const char* text) {
            if (m_flexBildGrund == grund) { return; }
            m_flexBildGrund = grund;
            qCInfo(lcFlexGui) << "Flex-Bild:" << text;
        };

        if (m_flexSpanHighHz <= m_flexSpanLowHz) {
            meldeEinmal(FlexBildGrund::KeinAusschnitt,
                        "kein Ausschnitt — es kam noch keine Wasserfallzeile "
                        "mit Frequenzen, also waere jedes Bild an der "
                        "falschen Stelle");
            return;
        }
        SpectrumWidget* sw = m_radioModel ? m_radioModel->spectrumWidget() : nullptr;
        if (!sw) {
            meldeEinmal(FlexBildGrund::KeinFenster,
                        "kein Panadapter-Fenster da (RadioModel::spectrumWidget "
                        "ist leer) — es gibt nichts, worauf gezeichnet wird");
            return;
        }
        if (!sw->kiwiDisplaySource()) {
            meldeEinmal(FlexBildGrund::FremdquelleAus,
                        "das Fenster nimmt keine fremde Quelle an "
                        "(kiwiDisplaySource ist aus)");
            return;
        }
        if (m_flexBildGrund != FlexBildGrund::Zeichnet) {
            // Einmal, beim ersten gezeichneten Rahmen: WOHIN gemalt
            // wird. „Wird gezeichnet" und „ist zu sehen" sind zweierlei
            // — ein unsichtbares Fenster, eine Flaeche der Groesze null
            // oder ein Ausschnitt neben den Daten sehen alle gleich aus,
            // naemlich schwarz.
            const auto mhz = [](double hz) { return hz / 1.0e6; };
            qCInfo(lcFlexGui).nospace()
                << "Flex-Bild: " << dbm.size() << " Bins, Ausschnitt "
                << mhz(m_flexSpanLowHz) << "–" << mhz(m_flexSpanHighHz)
                << " MHz; Fenster zeigt " << mhz(sw->centerFrequency())
                << " MHz Mitte, " << mhz(sw->bandwidth()) << " MHz breit; "
                << "sichtbar=" << sw->isVisible()
                << " Groesze=" << sw->width() << "x" << sw->height();

            // Die Elternkette, wenn die Flaeche unsichtbar ist.
            //
            // `isVisible()` ist falsch, sobald IRGENDEIN Vorfahr
            // versteckt ist — die Flaeche selbst kann dabei tadellos
            // dastehen. Welche Ebene zu ist, sieht man nur, wenn man
            // die Kette hinauflaeuft; am 2026-10-21 hat das Raten
            // daran eine Stunde gekostet (memory
            // longpath-pruefstand-fensterbreite-xvfb sagt dasselbe:
            // Elternkette melden, nicht die Endzahl).
            if (!sw->isVisible()) {
                int ebene = 0;
                for (QWidget* w = sw; w; w = w->parentWidget(), ++ebene) {
                    qCInfo(lcFlexGui).nospace()
                        << "Flex-Bild: Elternkette " << ebene << ": "
                        << w->metaObject()->className()
                        << " \"" << w->objectName() << "\""
                        << " sichtbar=" << w->isVisible()
                        << " versteckt=" << w->isHidden()
                        << " Fenster=" << w->isWindow()
                        << " Lage=" << w->geometry();
                }
            }
        }
        meldeEinmal(FlexBildGrund::Zeichnet, "wird gezeichnet");

        // ── Clarity muss diese Bins auch sehen (2026-10-22) ──────────
        //
        // Clarity bekam seine Bins bis heute AUSSCHLIESZLICH aus
        // `primaryFftEngine()`, also aus dem eigenen Funkgeraet. Bei
        // der Flex gibt es die nicht — sie rechnet selbst. Clarity war
        // damit eingeschaltet (`ClarityEnabled=True` in Martins
        // Profil) und ohne Boden, und `composeWaterfallActiveThresholds`
        // liesz die Schwellen auf dem Stand stehen, den das letzte
        // eigene Geraet hinterlassen hatte.
        //
        // Folge am Geraet: der Wasserfall war durchgehend EINFARBIG
        // ROT — Rot ist bei Farbschema 0 die oberste Palettenstufe,
        // also der Anschlag. Dieselbe Fehlerklasse ist in
        // `composeWaterfallActiveThresholds` schon beschrieben
        // („an einer SunSDR2 QRP ohne Antenne … der Wasserfall wurde
        // ganz rot"), nur kam sie dort von einem FALSCHEN Boden und
        // hier von gar keinem.
        if (m_clarityController) {
            m_clarityController->feedBins(dbm);
        }

        // ── Beim Senden das Bandbild halten (2026-10-29) ─────────────
        //
        // Betreiber: „wenn ich sende ist alles blau, das sollte so
        // aber nicht sein."
        //
        // Er hat recht gesehen und die Maschine macht nichts falsch:
        // beim Tasten schlieszt die Flex ihren Empfaenger
        // (`full_duplex_enabled=0`, an der Maschine abgefragt), und was
        // dann noch im Bild steht, ist die eigene Aussendung und
        // sonst Rauschen. An einer Antenne waere es auch nicht anders
        // zu machen: das eigene Signal macht den Empfaenger taub.
        //
        // Also wird das letzte Empfangsbild festgehalten und die
        // Sendung darueber gelegt — je Bin der groeszere der beiden
        // Werte. Das Band bleibt stehen, wo es war, und das eigene
        // Signal waechst sichtbar hinein.
        //
        // Festgehalten wird nur, solange der Ausschnitt derselbe ist:
        // wer waehrend des Sendens die Scheibe verschiebt, bekaeme
        // sonst ein Bild von einer anderen Stelle des Bandes
        // untergelegt.
        const bool sendet = m_radioModel->flexSendetGerade()
                            || m_flexSelbstGetastet;
        const bool gleicherAusschnitt =
            m_flexBildFrostLowHz == m_flexSpanLowHz
            && m_flexBildFrostHighHz == m_flexSpanHighHz
            && m_flexBildFrost.size() == dbm.size();

        if (!sendet) {
            m_flexBildFrost        = dbm;
            m_flexBildFrostLowHz   = m_flexSpanLowHz;
            m_flexBildFrostHighHz  = m_flexSpanHighHz;
            sw->updateKiwiSpectrumDbm(dbm, m_flexSpanLowHz, m_flexSpanHighHz);
            return;
        }

        if (!gleicherAusschnitt || m_flexBildFrost.isEmpty()) {
            sw->updateKiwiSpectrumDbm(dbm, m_flexSpanLowHz, m_flexSpanHighHz);
            return;
        }

        QVector<float> gelegt = m_flexBildFrost;
        for (int i = 0; i < gelegt.size(); ++i) {
            gelegt[i] = std::max(gelegt[i], dbm[i]);
        }
        sw->updateKiwiSpectrumDbm(gelegt, m_flexSpanLowHz, m_flexSpanHighHz);
    });

    // ── Aufraeumen beim Trennen ─────────────────────────────────────
    connect(m_radioModel, &RadioModel::connectionStateChanged, this,
            [this](ConnectionState st) {
        if (st == ConnectionState::Connected || m_shuttingDown) { return; }
        if (m_flexTargetSliceId < 0) { return; }
        if (AudioEngine* ae = m_radioModel ? m_radioModel->audioEngine() : nullptr) {
            // Den Wandler mit wegwerfen: ein alter Filterzustand auf
            // einem neuen Empfaenger gibt beim ersten Block ein Knacken.
            ae->removeFlexAudioSource(m_flexTargetSliceId);
        }
        if (SpectrumWidget* sw = m_radioModel ? m_radioModel->spectrumWidget() : nullptr) {
            sw->setKiwiDisplaySource(false);
        }
        qCInfo(lcFlexGui) << "Flex-Ton von Scheibe" << m_flexTargetSliceId
                          << "geloest";
        // Nur die eigene wieder wegraeumen. Eine Scheibe, die der
        // Betreiber selbst hatte, bleibt stehen.
        if (m_flexSliceWasCreated && m_radioModel) {
            m_radioModel->removeSlice(m_flexTargetSliceId);
        }
        // Senden abbauen (Stufe 2). Die Baender bleiben haengen, ihre
        // Lambdas kehren aber ohne Zielscheibe sofort um; die Pumpe
        // muss ausdruecklich stehen, sonst zieht sie weiter Mikrofonton
        // und schickt ihn an eine Kennung, die es nicht mehr gibt.
        if (m_flexSendePumpe) { m_flexSendePumpe->stop(); }
        m_flexSelbstGetastet = false;

        m_flexSliceWasCreated = false;
        m_flexAusschnittUebernommen = false;
        m_flexTargetSliceId = -1;
        m_flexSpanLowHz = 0.0;
        m_flexSpanHighHz = 0.0;
    });
}

// ── Senden ueber die Flex (2026-10-23, Stufe 2) ──────────────────────
//
// Bis heute stand im Kopf von FlexRadioConnection.h: „Was Stufe 1
// trotzdem NICHT tut: senden." Hier haengt es jetzt zusammen — und
// zwar so, dass nichts davon von selbst tastet.
//
// Was anders ist als bei einem OpenHPSDR-Geraet: dort rechnet Longpath
// das Sendesignal (WDSP, TxWorkerThread, I/Q hinaus). Die Flex rechnet
// selbst. Es geht also KEIN fertiges Signal hinaus, sondern roher
// Mikrofonton, und die Maschine macht daraus SSB. Deshalb haengt hier
// auch nichts am TX-Kanal, sondern direkt an `AudioEngine::pullTxMic`.
// ── Was jetzt faellig ist ────────────────────────────────────────────
//
// Die ganze Behebung vom 2026-10-29 in vier Zeilen: nicht „ein Schlag,
// ein fester Block", sondern „so viel, wie die Uhr sagt". Siehe
// MainWindow.h bei `m_flexSendeUhr` fuer die Messung, die dahinter
// steht.
// ── Der Kanalzug im Flex-Sendeweg (2026-10-31) ───────────────────────
//
// Bis heute ging der Mikrofonton hier ROH an die Maschine. Der
// Kanalzug lief an genau einer Stelle — `TxWorkerThread.cpp`, dem
// eigenen Sendeweg, auf dem Longpath selbst rechnet. Die Flex rechnet
// selbst, also hat sie keinen TxWorkerThread, und damit war die ganze
// Kette ausser Betrieb, sobald am Flex gesendet wurde: Entrauschen,
// Tor, Entzerrer, De-Esser, Kompressor, Roehre, PUDU, Hall, Clipper,
// Begrenzer. Alles eingestellt, nichts davon in der Luft.
//
// Drei Regeln, alle aus dem Vorbild im TxWorkerThread uebernommen:
//
//   Nur Stimmen. `vonFt8` ist dort `datenSignal`: der Kanalzug ist
//   fuer eine Stimme gebaut, und auf einem fertig modulierten Signal
//   mit konstanter Huellkurve ist er Zerstoerung. Die Begruendung
//   steht ausfuehrlich bei TxWorkerThread.cpp:1099.
//
//   Hauptschalter zuerst. `isEnabled()` wird HIER gefragt und nicht
//   der Kette ueberlassen — sie kehrt zwar selbst sofort um, aber dann
//   stuende in `ketteLief` eine Unwahrheit.
//
//   Die Kette bleibt eine. Es ist dieselbe, die das Kanalzugfenster
//   bedient (`RadioModel::stripChain()`), keine zweite mit
//   nachgezogenen Werten: was der Betreiber am Regler sieht, ist was
//   hinausgeht. Zwei Sendewege gleichzeitig gibt es nicht — der
//   TxWorkerThread entsteht nur mit einem eigenen Funkgeraet
//   (RadioModel.cpp:10638), die Pumpe hier laeuft nur an der Flex.
MainWindow::FlexMikBlock MainWindow::flexMikrofonblock(
    float* proben, int anzahl, float faktor, StripChain* kette,
    float grenze)
{
    FlexMikBlock e{};
    if (proben == nullptr || anzahl <= 0) { return e; }

    // 1. Die Aussteuerung von Hand. 0 dB heiszt roh, wie bisher.
    if (faktor != 1.0f) {
        for (int i = 0; i < anzahl; ++i) { proben[i] *= faktor; }
    }
    auto spitze = [](const float* s, int n) {
        float p = 0.0f;
        for (int i = 0; i < n; ++i) { p = std::max(p, std::fabs(s[i])); }
        return p;
    };
    e.quelleSpitze = spitze(proben, anzahl);

    // 2. Der Kanalzug.
    if (kette != nullptr && kette->isEnabled()) {
        kette->processMono(proben, anzahl);
        e.ketteLief = true;
    }
    e.ausgangSpitze = e.ketteLief ? spitze(proben, anzahl) : e.quelleSpitze;

    // 3. Der Riegel. Steht auch dann, wenn die Kette aus ist.
    if (grenze > 0.0f) {
        for (int i = 0; i < anzahl; ++i) {
            proben[i] = std::clamp(proben[i], -grenze, grenze);
        }
    }
    return e;
}

qint64 MainWindow::flexFaelligeRahmen(qint64 nsecsVerstrichen,
                                      qint64 geliefert,
                                      qint64 rateHz,
                                      qint64 maxRahmen)
{
    if (nsecsVerstrichen < 0 || rateHz <= 0 || maxRahmen <= 0) { return 0; }
    const qint64 faellig = (nsecsVerstrichen * rateHz) / 1000000000LL;
    const qint64 offen   = faellig - geliefert;
    if (offen <= 0) { return 0; }
    return std::min(offen, maxRahmen);
}

void MainWindow::verdrahteFlexSenden()
{
    if (!m_radioModel) { return; }

    // Der Kanalzug steht schon (RadioModel legt ihn im Konstruktor an,
    // samt den Einstellungen des Betreibers). Hier wird er nur auf die
    // Rate dieses Weges gestellt und sein Zustand geleert — dieselbe
    // Zeile wie beim eigenen Funkgeraet (RadioModel.cpp:10651), aus
    // demselben Grund: die Pumpe soll auf einer Kette ohne Geschichte
    // anfangen. Hinein gehen 48 kHz; die Halbierung auf die 24 kHz der
    // Maschine kommt erst HINTER der Kette.
    if (StripChain* kette = m_radioModel->stripChain()) {
        kette->prepare(48000.0);
    }

    // ── Die Mikrofonpumpe ────────────────────────────────────────────
    //
    // 5 ms, wie die Pumpe des OpenHPSDR-Wegs (TxWorkerThread). Die
    // Maschine will Pakete zu 128 Rahmen, der Rest bleibt in
    // RadioModel liegen und geht beim naechsten Mal mit.
    //
    // ── Die Uhr gibt den Takt, nicht der Taktgeber (2026-10-29) ──────
    //
    // Hier standen starre 480 Rahmen je Schlag. Das war zweierlei
    // falsch: 480 sind zehn Millisekunden, nicht fuenf — und vor allem
    // kommt der Schlag auf einem malenden Hauptfaden nicht alle 5 ms.
    // Gemessen wurden 8,7 bis 34 ms, und damit ging der Ton zwischen
    // 0,27- und 1,15-fach schnell hinaus (siehe MainWindow.h bei
    // `m_flexSendeUhr`).
    //
    // Der Sendeton ist ein Echtzeitstrom. Also wird gerechnet, was
    // faellig ist, statt zu zaehlen, wie oft geweckt wurde.
    if (!m_flexSendePumpe) {
        m_flexSendePumpe = new QTimer(this);
        m_flexSendePumpe->setInterval(5);
        m_flexSendePumpe->setTimerType(Qt::PreciseTimer);
        connect(m_flexSendePumpe, &QTimer::timeout, this, [this]() {
            if (m_shuttingDown || !m_radioModel) { return; }
            AudioEngine* ae = m_radioModel->audioEngine();
            if (!ae) { return; }

            // ── Die Maschine nimmt 24 kHz (2026-10-29) ──────────────
            //
            // Hier stand 48000, weil die Quellen mit 48 kHz liefern
            // (RadioModel.cpp:2648, `Ft8TxSource(48000)`). Der
            // DAX-Sendestrom der Flex laeuft aber auf 24 kHz — und
            // eine Maschine, die zu viele Proben bekommt, wirft keine
            // weg: sie spielt sie ab, halb so schnell.
            //
            // Am Geraet gemessen, zwei Sendungen hintereinander, mit
            // einer zweiten Verbindung an der Verriegelung mitgelesen:
            //
            //   getastet 7,19 s -> 346.635 Proben -> Nachlauf 7,65 s
            //   getastet 3,83 s -> 182.624 Proben -> Nachlauf 3,98 s
            //
            //   346.635 / 24.000 = 14,4 s Spielzeit, 7,2 s davon
            //   waehrend des Tastens -> 7,2 s bleiben uebrig.
            //
            // Der Nachlauf ist also genau die Sendung noch einmal. Der
            // Betreiber hoerte das als „alles zeitverzoegert", sah es
            // als „ich loese MOX und es sendet noch weiter" und als
            // „driftet ab". Seine Stimme ging eine Oktave zu tief und
            // doppelt so lang hinaus.
            //
            // Hinein kommen weiter 48 kHz, hinaus gehen 24: je zwei
            // Proben werden gemittelt. Der Sendeweg ist auf 2,9 kHz
            // begrenzt, da ist nichts zu falten.
            constexpr qint64 kEingangHz = 48000;
            constexpr qint64 kRateHz    = 24000;   // was die Maschine nimmt
            static_assert(kEingangHz == 2 * kRateHz,
                          "die Mittelung unten halbiert genau");

            // Hoechstens 100 ms auf einen Schlag. Ein langer Haenger
            // soll aufgeholt werden, aber nicht als eine halbe Sekunde
            // Ton in einem Schwall: der Rueckstand bleibt im Zaehler
            // stehen und geht beim naechsten Schlag mit.
            constexpr qint64 kMaxRahmen = kRateHz / 10;

            if (!m_flexSendeUhr.isValid()) { return; }
            const qint64 offen = flexFaelligeRahmen(
                m_flexSendeUhr.nsecsElapsed(), m_flexSendeGeliefert,
                kRateHz, kMaxRahmen);
            if (offen <= 0) { return; }
            m_flexSendeRueckstand = std::max(
                m_flexSendeRueckstand,
                (m_flexSendeUhr.nsecsElapsed() * kRateHz) / 1000000000LL
                    - m_flexSendeGeliefert);
            // `offen` zaehlt in AUSGANGSrahmen (24 kHz). Geholt wird
            // das Doppelte, gemittelt wird unten.
            const int kRahmen = int(offen) * 2;
            m_flexMikMono.resize(kRahmen);

            // ── Wer gerade spricht (2026-10-28) ──────────────────────
            //
            // Bis heute holte diese Pumpe IMMER aus der
            // Mikrofonbuchse. Der FT8-Ton entsteht aber in
            // `Ft8TxSource` und lief beim eigenen Funkgeraet ueber den
            // Mikrofon-Router in den WDSP-Sendekanal — den die Flex
            // nicht hat. Er kam hier also nie an: Send gedrueckt,
            // Traeger an, Stille hinaus.
            //
            // AetherSDR hat dieselbe Klasse Fehler einmal von der
            // anderen Seite gehabt (AudioEngine.h:128-136 [@d58e2b8a]:
            // „the gate silently disabled the test tone as well, since
            // the tone is injected inside that callback") — ein Tor,
            // das fuer den Strom gedacht war, machte den erzeugten Ton
            // mit stumm. Daraus die Lehre, nicht der Code: die
            // erzeugte Quelle braucht einen eigenen Weg, der nicht an
            // der Mikrofonbuchse haengt.
            //
            // Reihenfolge: laeuft FT8, gewinnt FT8. Das Mikrofon ist
            // in dem Augenblick ohnehin nicht gemeint, und zwei
            // Quellen zu mischen hiesze, sich selbst zu stoeren.
            int geholt = 0;
            bool vonFt8 = false;
            //
            // Der Sprachspeicher (2026-11-11) ist eine dritte Quelle,
            // aber kein Datensignal: eine Ansage ist aufgenommene
            // Stimme und geht darum durch dieselbe Aufbereitung wie das
            // Mikrofon (vonFt8 bleibt false). Beim eigenen Geraet holt
            // ihn der Mikrofon-Router ab; den hat die Flex nicht, also
            // hier -- sonst ging F1 mit offenem Mikrofon auf Sendung.
            VoiceKeyerPlayer* const ansage = m_radioModel->voiceKeyerPlayer();
            if (Ft8TxSource* ft8 = m_radioModel->ft8TxSource();
                ft8 && ft8->laeuft()) {
                geholt = ft8->pullSamples(m_flexMikMono.data(), kRahmen);
                vonFt8 = true;
            } else if (ansage != nullptr
                       && ansage->state() == VoiceKeyerPlayer::State::Playing) {
                geholt = ansage->source()->pullSamples(m_flexMikMono.data(), kRahmen);
            } else {
                geholt = ae->pullTxMic(m_flexMikMono.data(), kRahmen);
                ++m_flexTxVomMik;
            }
            if (geholt <= 0) { return; }

            // ── Der erzeugte Ton braucht Luft nach oben (2026-10-28) ─
            //
            // Erste Sendung ueber die Flex, am Geraet gemessen:
            //
            //   MICPEAK 3.008 dBFS, MIC -0.727 dBFS, HWALC 0.000 dBFS
            //
            // Drei Dezibel UEBER Vollaussteuerung. Auf dem eigenen Weg
            // sitzt vor dem Sender die ganze WDSP-Kette mit ihrer
            // Verstaerkungsstufe; hier geht der Ton roh an die
            // Maschine, und `Ft8Decoder::encodeBlock` rechnet ihn auf
            // Vollaussteuerung. FT8 hat eine konstante Huellkurve —
            // Spitze und Effektivwert liegen beieinander —, deshalb
            // genuegt ein fester Faktor; die Klammer darunter ist der
            // Riegel, falls eine Quelle doch einmal lauter kommt.
            //
            // ── Nachgemessen am 2026-10-30: es waren 6 dB zu viel ──
            //
            // Gegen einen Ueberschuss von 3 dB standen hier 9 dB. Was
            // das am Band bedeutet, hat der Betreiber gesehen:
            //
            //   Regler 20  ->  FWDPWR 37,0 dBm =  5,0 W
            //   Regler 79  ->  FWDPWR 43,0 dBm = 19,9 W
            //
            // Jedes Mal ein Viertel der Reglerzahl. Betreiber dazu:
            // „10 watt sollen 10 watt sein."
            //
            // Die Zahl, die der Regler schickt, ist richtig --
            // `transmit set rfpower=79` geht woertlich so hinaus. Sie
            // verliert sich hier davor, denn `rfpower` sagt der
            // Maschine, was sie bei VOLLAUSSTEUERUNG liefern soll. Wer
            // sie mit −9 dB ansteuert, bekommt nicht, was auf dem
            // Regler steht.
            //
            // Die Messung von damals, Zeile fuer Zeile:
            //
            //   MICPEAK  3,008 dBFS   <- 3 dB zu hoch, das war der Anlass
            //   MIC     -0,727 dBFS   <- lag schon richtig
            //   HWALC    0,000 dBFS   <- die ALC regelte gar nicht
            //
            // Der Anlass war der Spitzenwert, nicht die Aussteuerung.
            // −3 dB raeumt ihn aus und laesst alles andere stehen.
            //
            // Die Klammer muss mitwandern. Bei 0,707 gegen eine Grenze
            // von 0,5 waere JEDE Probe geklemmt worden -- ein
            // abgeschnittener FT8-Ton ist kein leiserer Ton, sondern
            // ein breiter: die Ecken erzeugen Seitenbaender, die der
            // Nachbarkanal abbekommt. Sie steht jetzt knapp unter
            // Vollaussteuerung und faengt nur noch, wofuer sie gedacht
            // war: eine Quelle, die unerwartet lauter kommt.
            //
            // Gedaempft wird NUR der erzeugte Ton: das Mikrofon hat
            // seine eigene Aussteuerung und ist hier nicht gemeint.
            if (vonFt8) {
                constexpr float kFt8Pegel = 0.707f;   // −3,0 dB
                constexpr float kGrenze   = 0.95f;    // −0,4 dBFS
                // Die Falle, die beim Heben des Pegels fast
                // zugeschnappt waere, kann jetzt nicht mehr
                // zuschnappen: liegt der Pegel ueber der Klammer, wird
                // JEDE Probe geklemmt, und der Ton geht breit statt
                // leise hinaus. Das faellt beim Uebersetzen auf, nicht
                // erst am Band. Gegenprobe gefahren: mit 0,99 gegen
                // 0,5 bricht der Bau mit genau diesem Satz ab.
                static_assert(kFt8Pegel <= kGrenze,
                              "Der Sendepegel liegt ueber der Klammer — "
                              "jede Probe wuerde geklemmt und der Ton "
                              "ginge verzerrt und breit hinaus.");
                float spitze = 0.0f;
                for (int i = 0; i < geholt; ++i) {
                    float v = m_flexMikMono[i] * kFt8Pegel;
                    v = std::clamp(v, -kGrenze, kGrenze);
                    m_flexMikMono[i] = v;
                    spitze = std::max(spitze, std::abs(v));
                }
                m_flexTxSpitze = std::max(m_flexTxSpitze, spitze);
                // Einmal je Sendung in die Akte: beim naechsten Mal
                // wird gemessen, nicht geschaetzt.
                if (spitze > m_flexFt8Spitze) {
                    m_flexFt8Spitze = spitze;
                    qCInfo(lcFlexGui).nospace()
                        << "FT8-Sendepegel: Spitze " << spitze << " ("
                        << (20.0 * std::log10(std::max(spitze, 1.0e-6f)))
                        << " dBFS vor der Maschine)";
                }
            } else {
                // ── Das Messloch am Mikrofonweg (2026-10-29) ────────
                //
                // Die Spitze wurde bis heute NUR im FT8-Zweig
                // gerechnet. Am Mikrofonweg blieb `m_flexTxSpitze` auf
                // null, und die Zusammenfassung meldete nach jeder
                // Sprechprobe „Spitze -150 dBFS".
                //
                // Das hat mich am 2026-10-29 eine Stunde gekostet: ich
                // habe die nicht gemessene Null als Befund gelesen und
                // dem Betreiber gesagt, es ginge Stille hinaus —
                // waehrend die Maschine im selben Augenblick MICPEAK
                // +2,54 dBFS und FWDPWR 35,36 dBm (3,4 W) meldete. Der
                // Ton war da; nur diese Zeile sah ihn nicht.
                //
                // Dazu der Riegel: +2,54 dBFS sind zweieinhalb
                // Dezibel UEBER Vollaussteuerung. Auf dem eigenen Weg
                // sitzt vor dem Sender die ganze WDSP-Kette; hier geht
                // der Ton roh an die Maschine. −1 dBFS laesst normale
                // Sprache unberuehrt und nimmt nur die Spitzen, die
                // der Wandler der Flex ohnehin nicht mehr darstellen
                // kann.
                constexpr float kMikGrenze = 0.891f;   // −1,0 dBFS
                // Die Aussteuerung von Hand (siehe RadioModel.h,
                // flexMikGainDb()). 0 dB heiszt: roh hinaus, wie bisher.
                const int gainDb = m_radioModel->flexMikGainDb();
                const float faktor =
                    (gainDb == 0) ? 1.0f
                                  : std::pow(10.0f, float(gainDb) / 20.0f);
                // Aussteuerung, Kanalzug, Riegel — in dieser Reihenfolge
                // und seit dem 2026-10-31 mit der Kette darin. Warum
                // ausgelagert und warum diese Reihenfolge: siehe den Kopf
                // von flexMikrofonblock().
                const FlexMikBlock e = flexMikrofonblock(
                    m_flexMikMono.data(), geholt, faktor,
                    m_radioModel->stripChain(), kMikGrenze);
                m_flexTxSpitze = std::max(m_flexTxSpitze, e.ausgangSpitze);

                // Einmal je Sendung: lief die Kette mit? Ohne diese
                // Zeile ist einem Protokoll nicht anzusehen, ob der Ton
                // bearbeitet hinausging oder roh — und genau das war
                // der Befund, der diesen Umbau ausgeloest hat.
                if (e.ketteLief && !m_flexKetteGemeldet) {
                    m_flexKetteGemeldet = true;
                    qCInfo(lcFlexGui).nospace()
                        << "Flex-Senden: der Kanalzug laeuft mit.";
                }

                // Die Quelle. Gemessen VOR der Kette, damit die Zeile
                // weiter das meint, was sie sagt.
                if (e.quelleSpitze > kMikGrenze && !m_flexMikZuLaut) {
                    m_flexMikZuLaut = true;
                    qCWarning(lcFlexGui).nospace()
                        << "Flex-Senden: das Mikrofon steht zu hoch — "
                        << "Spitze " << (20.0 * std::log10(e.quelleSpitze))
                        << " dBFS vor der Maschine, begrenzt auf −1 dBFS. "
                        << "Die Aussteuerung gehoert an die Quelle, nicht "
                        << "hierher.";
                }
                // Die Kette. Eine andere Ursache und darum eine andere
                // Zeile: hier lag die Quelle richtig und erst der
                // Aufbereiter (Makeup, Roehre, PUDU) kam darueber.
                if (e.ketteLief && e.quelleSpitze <= kMikGrenze
                    && e.ausgangSpitze > kMikGrenze && !m_flexKetteZuLaut) {
                    m_flexKetteZuLaut = true;
                    qCWarning(lcFlexGui).nospace()
                        << "Flex-Senden: der Kanalzug kommt ueber den "
                        << "Riegel — Quelle "
                        << (20.0 * std::log10(std::max(e.quelleSpitze, 1.0e-6f)))
                        << " dBFS, danach "
                        << (20.0 * std::log10(e.ausgangSpitze))
                        << " dBFS, begrenzt auf −1 dBFS. Das ist der "
                        << "Aufbereiter, nicht das Mikrofon.";
                }
            }

            // ── 48 kHz hinein, 24 kHz hinaus ────────────────────────
            //
            // Je zwei Proben zu einer gemittelt — derselbe billige
            // Zweipunktfilter, den AetherSDR auf der anderen Seite
            // benutzt (AudioEngine.cpp:1774 [@d58e2b8a], 48->24 fuer
            // den CW-Abgriff). Der Sendeweg ist auf 2,9 kHz begrenzt,
            // da faltet nichts zurueck.
            //
            // Mono hinein, Stereo hinaus: die Maschine erwartet zwei
            // Kanaele je Rahmen (float32, Big-Endian im Paket).
            const int hinaus = geholt / 2;
            if (hinaus <= 0) { return; }
            m_flexMikStereo.resize(hinaus * 2);
            for (int i = 0; i < hinaus; ++i) {
                const float s = 0.5f * (m_flexMikMono[2 * i]
                                        + m_flexMikMono[2 * i + 1]);
                m_flexMikStereo[i * 2]     = s;
                m_flexMikStereo[i * 2 + 1] = s;
            }
            m_radioModel->sendeFlexMikrofonton(m_flexMikStereo.constData(),
                                               hinaus);
            m_flexTxProben      += hinaus;
            m_flexSendeGeliefert += hinaus;
        });
    }

    // ── MOX ──────────────────────────────────────────────────────────
    //
    // Reihenfolge: erst den Senderzweig auf DAX, dann tasten. Umgekehrt
    // sendet die erste Zehntelsekunde das Mikrofon an der Maschine —
    // oder Stille, wenn dort keines steckt.
    //
    // `dax=0` wird beim Loesen NICHT geschickt. Die Vorlage sagt
    // ausdruecklich warum (AetherSDR TciServer.cpp:3414 [@d58e2b8a]):
    // der Zustandsschub der Maschine kommt verzoegert, und ein
    // sofortiges Zuruecksetzen kreuzt ihn.
    // ── An WELCHER Leitung getastet wird (2026-10-28) ────────────────
    //
    // Hier stand `TransmitModel::moxChanged`. Das feuert aber nur,
    // solange es KEINEN MoxController gibt:
    //
    //   RadioModel::setMox(bool on) {
    //       if (m_moxController) { m_moxController->setMox(on); }
    //       else                 { m_transmitModel.setMox(on);  }
    //   }
    //
    // Mit Controller — also immer im Betrieb — lief das Tasten
    // vollstaendig an der Flex vorbei. Am 2026-10-28 um 21:14 stand
    // deshalb im Protokoll NULL mal „MOX gedrueckt", waehrend die
    // Bedienung MOX zeigte, ON AIR rot war und die Maschine
    // MICPEAK −120 dBFS und FWDPWR 0,000 dBm meldete: getastet nach
    // Meinung des Programms, in Wahrheit nie. PSK Reporter hatte in
    // 15 Minuten keine einzige Meldung.
    //
    // Das galt fuer JEDEN Weg, nicht nur FT8 — auch der MOX-Knopf
    // geht ueber RadioModel::setMox. Nur TUNE funktionierte, weil der
    // an `isTune` haengt, einer anderen Eigenschaft. Genau das stand
    // als „TUNE belegt, Sprechversuch offen" im Arbeitsstand: der
    // Sprechversuch waere ebenso stumm geblieben.
    //
    // `hardwareFlipped` ist die richtige Leitung — an ihr haengt auch
    // das eigene Funkgeraet (RadioModel.cpp:944) — und sie heiszt, was
    // sie tut: jetzt die Hardware umlegen.
    const auto tasten = [this](bool an) {
        qCInfo(lcFlexGui).nospace()
            << "MOX gedrueckt: " << (an ? "an" : "aus")
            << ", Zielscheibe " << m_flexTargetSliceId;
        if (m_shuttingDown || !m_radioModel) { return; }
        if (m_flexTargetSliceId < 0) {
            qCWarning(lcFlexGui) << "MOX verworfen: keine Flex-Zielscheibe";
            return;
        }

        // ── Nicht waehrend des Abstimmens tasten (2026-10-27) ────
        //
        // `transmit tune 1` laesst die Maschine ihren eigenen Traeger
        // machen und tastet sich dabei selbst. Kaeme jetzt noch
        // `xmit 1` samt Mikrofonpumpe dazu, lagen Traeger und Sprache
        // gleichzeitig am Sender. Wer TUNE drueckt, will den Traeger.
        if (an && m_radioModel->transmitModel().isTune()) {
            qCInfo(lcFlexGui) << "MOX waehrend TUNE ignoriert";
            return;
        }

        if (an) {
            m_radioModel->sendeFlexBefehl(
                FlexCommandWire::setDaxRouteCommand(true));
            m_flexTonImSenden = 0;
            m_flexLoesenMs    = 0;
            m_flexTxProben    = 0;
            m_flexTxVomMik    = 0;
            m_flexTxSpitze    = 0.0f;
            m_flexMikZuLaut   = false;
            m_flexKetteZuLaut   = false;
            m_flexKetteGemeldet = false;
            m_flexSendeGeliefert  = 0;
            m_flexSendeRueckstand = 0;
            m_flexSendeUhr.start();

            // ── Vor JEDEM Tasten die Leistung, die der Betreiber sieht ──
            //
            // Am Geraet (2026-11-17): am Regler stand 5, beim Anmelden
            // ging „senden 5 %" hinaus — und nach dem Wechsel von 17 m
            // auf 15 m masz die Maschine 35 W nach einer halben Sekunde
            // (12:30:30), am Vormittag auf demselben Band 72 W
            // (48,6 dBm, 11:07–11:09). Die Flex fuehrt ihre Leistung je
            // Band selbst und stellt beim Bandwechsel IHREN Wert
            // zurueck; unserer ging nur beim Anmelden und bei einer
            // Reglerbewegung hinaus.
            //
            // Ein Regler, der 5 zeigt, waehrend 72 W in eine Stehwelle
            // von 2,3 gehen, ist die gefaehrlichere Sorte Anzeige. Der
            // Befehl ist derselbe wie beim Anmelden und kostet nichts,
            // wenn die Maschine den Wert schon hat. Am Geraet noch
            // nicht gegengeprueft (Stand 17.11.).
            {
                const int prozent = qBound(
                    0, m_radioModel->transmitModel().power(), 100);
                m_radioModel->setzeFlexSendeleistung(prozent);
                qCInfo(lcFlexGui) << "Flex: Leistung vor dem Tasten"
                                  << prozent << "%";
            }
            m_flexSelbstGetastet = m_radioModel->flexTasten(true);
            if (m_flexSelbstGetastet && m_flexSendePumpe) {
                m_flexSendePumpe->start();
            } else {
                // Die Bedienung zeigt sonst „sendet", waehrend nichts
                // hinausgeht. Der Grund steht im Protokoll.
                //
                // 2026-11-10: hier stand transmitModel().setMox(false).
                // Mit MoxController ist der Riegel nur noch ein Spiegel
                // des Controllers (RadioModel-Konstruktor) -- das haette
                // nichts ausgetastet und waere beim naechsten Schritt des
                // Controllers wieder auf „sendet" gesprungen. Darum der
                // echte Weg, in die Warteschlange gelegt: wir stehen hier
                // mitten in hardwareFlipped, also mitten in setMox().
                QMetaObject::invokeMethod(m_radioModel, [rm = m_radioModel]() {
                    rm->setMox(false);
                }, Qt::QueuedConnection);
            }
        } else {
            if (m_flexSendePumpe) { m_flexSendePumpe->stop(); }
            const qint64 getastetMs = m_flexSendeUhr.isValid()
                                          ? m_flexSendeUhr.elapsed() : 0;
            m_flexSendeUhr.invalidate();
            m_radioModel->flexTasten(false);
            m_flexSelbstGetastet = false;
            // Ab hier laeuft die Uhr: die naechste Hoerperiode ist nur
            // dann zu gebrauchen, wenn der Empfang rechtzeitig
            // zurueckkommt.
            m_flexLoesenMs = QDateTime::currentMSecsSinceEpoch();
            qCInfo(lcFlexGui) << "Flex: waehrend des Sendens kamen"
                              << m_flexTonImSenden << "Tonpakete";

            // ── Was WIR hinausgeschickt haben ────────────────────────
            //
            // Die Gegenprobe zu den Messwerten der Maschine. Steht
            // hier eine volle Zahl und dort MICPEAK −150 dBFS, hat sie
            // nichts angenommen; steht hier null, haben wir nichts
            // geschickt. Am 2026-10-29 war das nicht zu entscheiden,
            // weil es diese Zeile noch nicht gab.
            //
            // 12,64 s FT8 bei 24 kHz sind rund 303.000 Proben.
            //
            // Dazu seit dem 2026-10-29 die GESCHWINDIGKEIT: der
            // Sendeton ist ein Echtzeitstrom, und eine Eins davor
            // heiszt, dass er so hinausging, wie er gemeint war.
            // Stand hier 0,27, kam beim Gegenueber ein auf das
            // Vierfache gedehntes Signal an — oder gar keines.
            // 24 Rahmen je Millisekunde, nicht 48: die Maschine nimmt
            // 24 kHz (siehe die Pumpe oben). Bis zum 2026-10-29 stand
            // hier 48, und das Tempo las sich als 1,0 — waehrend in
            // Wahrheit doppelt so viel hinausging, wie die Maschine
            // abspielen konnte.
            const double sollRahmen = std::max(1.0, double(getastetMs) * 24.0);
            qCInfo(lcFlexGui).nospace()
                << "Flex-Senden: " << m_flexTxProben
                << " Proben hinaus, Spitze "
                << (m_flexTxSpitze > 0.0f
                        ? 20.0 * std::log10(m_flexTxSpitze)
                        : -150.0)
                << " dBFS, " << m_flexTxVomMik
                << " Zuege am Mikrofon statt an FT8, Tempo "
                << (double(m_flexTxProben) / sollRahmen)
                << "-fach, groeszter Rueckstand "
                << (m_flexSendeRueckstand / 24) << " ms";

            // ── Eine stille Sendung ist ein Befund ───────────────────
            //
            // 2026-10-29, am Gerät: 664 Tonpakete hinaus, und die
            // Maschine machte 45 mW statt Leistung. Der offene Eingang
            // war „BoomAudio", ein virtuelles Gerät ohne Ton — der
            // Betreiber sah „es geht nicht" und suchte bei der Flex.
            //
            // Diese Prüfung trägt erst, seit die Spitze auch am
            // Mikrofonweg gemessen wird (siehe oben): vorher stand
            // hier bei JEDER Sprechprobe eine Null, und eine Warnung,
            // die immer kommt, ist keine.
            //
            // Ein echtes Mikrofon liefert immer Rauschen; exakt nichts
            // liefert nur ein Eingang, der nichts hört. Die Schwelle
            // liegt bei −120 dBFS, weit unter jedem Grundrauschen.
            if (m_flexTxProben > 0 && m_flexTxSpitze < 1.0e-6f) {
                qCWarning(lcFlexGui).noquote()
                    << QStringLiteral(
                           "Flex-Senden: der Sendeton war die ganze "
                           "Sendung über still — es ging ein Träger ohne "
                           "Modulation hinaus. Der offene Eingang steht "
                           "im Protokoll als \"input via …\".");
            }

            // Die Sendepause ist KEINE Taktabweichung. Ohne diese
            // Zeile liest der Driftausgleich den leergelaufenen
            // Ausgangspuffer als solche und taktet die folgende
            // Hoerperiode um 1700 ppm daneben — siehe AudioEngine.h,
            // m_rxDriftZuruecksetzen.
            if (AudioEngine* ae = m_radioModel->audioEngine()) {
                ae->rxDriftNeuAnfangen();
            }
        }
    };

    disconnect(m_flexMoxBand);
    if (MoxController* mox = m_radioModel->moxController()) {
        m_flexMoxBand = connect(mox, &MoxController::hardwareFlipped,
                                this, tasten);
    } else {
        // Ohne Controller ist der Riegel im TransmitModel die
        // Wahrheit — derselbe Rueckfall wie in RadioModel::setMox().
        m_flexMoxBand = connect(&m_radioModel->transmitModel(),
                                &TransmitModel::moxChanged, this, tasten);
    }

    // ── TUNE ─────────────────────────────────────────────────────────
    //
    // Der Abstimmtraeger der Maschine, nicht unserer: er taktet mit
    // `tunepower` und braucht weder Mikrofon noch DAX.
    disconnect(m_flexTuneBand);
    m_flexTuneBand = connect(&m_radioModel->transmitModel(),
                             &TransmitModel::tuneChanged, this,
                             [this](bool an) {
        qCInfo(lcFlexGui).nospace()
            << "TUNE gedrueckt: " << (an ? "an" : "aus")
            << ", Zielscheibe " << m_flexTargetSliceId
            << ", Abbau " << m_shuttingDown;
        if (m_shuttingDown || !m_radioModel) { return; }
        if (m_flexTargetSliceId < 0) {
            qCWarning(lcFlexGui) << "TUNE verworfen: keine Flex-Zielscheibe";
            return;
        }
        if (an && !m_radioModel->flexAbstimmen(true)) {
            m_radioModel->transmitModel().setTune(false);
        } else if (!an) {
            m_radioModel->flexAbstimmen(false);
        }
    });

    // ── Leistung ─────────────────────────────────────────────────────
    //
    // Die Maschine will Prozent, Longpath fuehrt Watt. Bei einer
    // FLEX-8400 (100 W) ist das dieselbe Zahl; bei einem anderen Geraet
    // waere es das nicht, und dann gehoert hier eine Umrechnung hin.
    disconnect(m_flexLeistungBand);
    m_flexLeistungBand = connect(&m_radioModel->transmitModel(),
                                 &TransmitModel::powerChanged, this,
                                 [this](int watt) {
        if (m_shuttingDown || !m_radioModel || m_flexTargetSliceId < 0) {
            return;
        }
        m_radioModel->setzeFlexSendeleistung(qBound(0, watt, 100));
    });

    disconnect(m_flexAbstimmleistungBand);
    m_flexAbstimmleistungBand =
        connect(&m_radioModel->transmitModel(),
                &TransmitModel::tunePowerChanged, this,
                [this](int watt) {
        if (m_shuttingDown || !m_radioModel || m_flexTargetSliceId < 0) {
            return;
        }
        // ── Der Regler, nicht der Regler-Wert (2026-10-31) ──────
        //
        // Mit eingeschaltetem Abstimmregler ist die Zahl am Schieber
        // das ZIEL IN WATT, nicht der Prozentwert fuer die Maschine.
        // Gemessen: 10 % ergaben 23 W — wer Watt meint, kann die
        // Prozente nicht einstellen, sondern musz das Ergebnis lesen.
        // Die Prozente setzt dann `flexAbstimmen()` beim Traegerstart
        // und regelt sie waehrenddessen nach unten.
        if (m_radioModel->flexAbstimmRegler().an()) {
            m_radioModel->flexAbstimmRegler().setzeZielWatt(watt);
        } else {
            m_radioModel->setzeFlexAbstimmleistung(qBound(0, watt, 100));
        }
    });

    // ── Einmal nachziehen, statt auf eine Reglerbewegung zu warten ───
    //
    // Die zwei Baender darueber haengen an `powerChanged` /
    // `tunePowerChanged` — sie feuern nur, wenn der Regler BEWEGT
    // wird. Beim Anmelden ging also nie eine Leistung an die Maschine,
    // und sie behielt, was ein anderer Client zuletzt gesetzt hatte.
    //
    // Am 2026-10-29 stand sie dadurch auf 1 %: der Betreiber sah
    // „Power 1" und „POWER 0 W" in einer Oberflaeche, die 10 W
    // anzeigte. Jede Erklaerung dafuer lag am falschen Ende (Ton,
    // Verriegelung, Pegel) — die Zahl war einfach nie hinausgegangen.
    //
    // Hier wird nichts entschieden, nur gesagt, was Longpath schon
    // fuehrt. Der Wert selbst kommt aus den Einstellungen des
    // Betreibers und bleibt seiner.
    {
        const TransmitModel& tm = m_radioModel->transmitModel();
        const int sendeWatt  = qBound(0, tm.power(), 100);
        // ── Die Abstimmleistung aus dem BANDWERT (2026-10-31) ───────
        //
        // `tm.tunePower()` ist der globale Wert. Bis heute schrieb der
        // Regler nur den Bandwert, der globale blieb also auf dem
        // Stand von irgendwann -- beim Betreiber auf 10, waehrend am
        // Regler 1 stand. Der Regler ist jetzt gepaart
        // (`TxApplet.cpp`), aber beim Anmelden ist noch offen, wer
        // zuerst dran war; darum hier die Quelle lesen, die der
        // Betreiber auch SIEHT: den Wert des Sendebandes.
        int stimmWatt = qBound(0, tm.tunePower(), 100);
        if (const SliceModel* const sl =
                m_radioModel->sliceById(m_flexTargetSliceId)) {
            stimmWatt = qBound(
                0, tm.tunePowerForBand(bandFromFrequency(sl->frequency())), 100);
        }
        m_radioModel->setzeFlexSendeleistung(sendeWatt);
        if (m_radioModel->flexAbstimmRegler().an()) {
            // Das Ziel in Watt; die Prozente findet der Regler selbst.
            m_radioModel->flexAbstimmRegler().setzeZielWatt(stimmWatt);
        } else {
            m_radioModel->setzeFlexAbstimmleistung(stimmWatt);
        }
        qCInfo(lcFlexGui).nospace()
            << "Flex: Leistung nachgezogen — senden " << sendeWatt
            << " %, abstimmen " << stimmWatt << " %";
    }

    // ── Was die Maschine selbst meldet ───────────────────────────────
    //
    // Sie sendet auch, wenn jemand die Taste am Mikrofon drueckt oder
    // der Fusschalter betaetigt wird. Dann haben WIR nicht getastet,
    // und die Mikrofonpumpe darf nicht laufen — sonst mischt sich
    // unser Ton in seinen.
    disconnect(m_flexSendetBand);
    m_flexSendetBand = connect(m_radioModel, &RadioModel::flexSendetGeaendert,
                               this, [this](bool sendet) {
        if (m_shuttingDown || !m_radioModel) { return; }
        if (!sendet && m_flexSendePumpe) { m_flexSendePumpe->stop(); }
        qCInfo(lcFlexGui).nospace()
            << "Flex " << (sendet ? "SENDET" : "empfaengt")
            << (m_flexSelbstGetastet ? " (von uns getastet)"
                                     : " (nicht von uns)");
    });

    // ── Die Sperre muss sagen, dass sie es war (2026-10-30) ──────
    //
    // `flexSwrGesperrt` gab es seit heute Nachmittag und es hatte
    // KEINEN Empfaenger: dreimal ausgesendet, niemand hoerte zu. Die
    // Sperre griff stumm, und eine stumme Sicherheitssperre ist von
    // einem Defekt nicht zu unterscheiden -- der Betreiber meldet
    // „sendet nicht", und der Abend geht mit Raten hin. Genau das
    // steht als Lehre schon ueber den vier Toren in `flexTasten`:
    // „Jedes einzeln melden: ‚es sendet nicht' ohne Grund hat am
    // 2026-10-22 einen Abend gekostet."
    //
    // `Error` ist die richtige Stufe, woertlich: „A protection or
    // interlock actively blocked the operator" (`StatusToast.h:43`).
    // Und das SWR-Rot bleibt kraeftig -- die Ausnahme vom Hausstil,
    // nicht sein Versaeumnis (CLAUDE.local.md).
    //
    // 8 s, weil 13,2 s FT8-Tastung und eine halbe Minute zwischen
    // zwei Durchgaengen liegen: der Hinweis soll noch stehen, wenn er
    // hinsieht, und weg sein, bevor der naechste kommt.
    disconnect(m_flexSwrBand);
    m_flexSwrBand = connect(m_radioModel, &RadioModel::flexSwrGesperrt,
                            this, [this](float swr, float grenze) {
        if (m_shuttingDown) { return; }
        showToast(tr("Not transmitting — SWR %1 is above the %2 limit. "
                     "Check antenna and feedline.")
                      .arg(swr, 0, 'f', 1)
                      .arg(grenze, 0, 'f', 1),
                  ToastSeverity::Error, 8000);
    });

    qCInfo(lcFlexGui) << "Flex-Senden verdrahtet — es wurde nichts getastet";
}

}  // namespace Longpath
