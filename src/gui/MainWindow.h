#pragma once

// =================================================================
// src/gui/MainWindow.h  (Longpath)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (Longpath):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Signal-routing hub, double-height status-bar layout, and
//                 TitleBar feature-request dialog ported from AetherSDR
//                 (ten9876/AetherSDR, GPLv3) src/gui/MainWindow.{h,cpp} and
//                 src/gui/TitleBar.{h,cpp}. AetherSDR has no per-file
//                 headers; project-level citation per docs/attribution/
//                 HOW-TO-PORT.md rule 6.
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include <memory>
#include <QMainWindow>
#include <QLabel>
#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QPointer>

#include "core/WdspTypes.h"
#include "core/DigiFrequenzen.h"
#include "gui/applets/PanelAutomatik.h"

#include <QElapsedTimer>
#include <QTimer>
#include <QHash>
#include <QMap>
#include <QVector>

class QHBoxLayout;
class QProgressDialog;
class QThread;
class QSplitter;
class QMenu;
class QAudioSource;
class QIODevice;

namespace Longpath {

/// Defined in gui/widgets/StatusToast.h. Forward-declared with its fixed
/// underlying type so this header keeps to Qt includes only.
enum class ToastSeverity : int;

class RadioModel;
/// Defined in core/strip/StripChain.h; der Sendeton-Kanalzug sitzt seit
/// dem 2026-10-31 auch im Flex-Weg und wird hier nur als Zeiger gehalten.
class StripChain;
/// Defined in gui/WindowPlacement.h; nur als Zeiger gehalten
/// (m_aufbauBewegung), damit dieser Kopf keinen weiteren Kopf zieht.
class EigeneBewegung;
class SideAreaWindow;
class ConnectionPanel;
class SupportDialog;
class WdspEngine;
class FFTEngine;
class SpectrumWidget;
class SliceModel;
// Phase 3F Sub-Epic D: forward declarations for the multi-pan layout
// manager. Member m_panStack is introduced (nullptr) in Task 10/11 so the
// +PAN affordance (a dropdown at the time; a drawn icon opening
// PanLayoutDialog since Task B4) and per-chain status indicators can
// guard against not-yet-wired state; Task 12 instantiates m_panStack and
// migrates m_spectrumWidget references.
class PanadapterStack;
class ClarityController;
class ContainerManager;
class MeterWidget;
class MeterPoller;
class TitleBar;
class VaxFirstRunDialog;
class PsForm;
// Phase 3J-2 H1: Tools menu modeless singletons.
class SpotHubDialog;
class AntennaWindow;
class StripWindow;
class ClientPuduMonitor;
class FreeDVReporterDialog;

class RxDashboard;
class StationBlock;
class ChromeBarController;
class SystemTile;
class StatusBadge;
class AdcOverloadBadge;
class OverflowChip;
class PsaIndicatorWidget;
class AppletVisibilityController;
class AppletWidget;
class QrzClient;
class RotorLogbookPanel;
class QrzLogbookUploader;
class CloudlogUploader;
class AdifNetworkUploader;
class QsoUploader;

// Phase 23: TCI server + applets forward declarations (all inside Longpath
// namespace — TciServer only exists when HAVE_WEBSOCKETS is defined but we
// forward-declare unconditionally; m_tciServer is nullptr in non-WebSocket builds).
class TciServer;
class TciApplet;
class ClientChainApplet;
// 2026-08-24: TciClient — Longpath als TCI-CLIENT auf ExpertSDR2/SunSDR2 QRP
// (siehe docs/TCI-SunSDR-gemessen.md). Unabhaengig vom TciServer oben, der
// Longpath in der Gegenrichtung als TCI-Server spielen laesst.
class TciClient;
// Phase 3J-1 closeout Item 2 (2026-05-12): TciLogWindow viewer.
class TciLogWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT

    // Die Tastenkuerzel (2026-11-10) gehen an dieselben Stellen wie die
    // Bedienelemente und brauchen dafuer m_radioModel, m_shuttingDown
    // und die zwei Wege darunter (stimmeVomBediener, panoramaZoomen).
    friend class MainWindowHotKeyTargets;

public:

    /// Die Scheibenflaggen ein- oder ausschalten (View > Slice
    /// flags). Aus ist der Zustand von August bis heute; an ist der
    /// dritte Entwurf vom 2026-10-15. Oeffentlich fuer den Pruefstand.
    void setSliceFlagsEnabled(bool on);
    bool sliceFlagsEnabled() const { return m_sliceFlagsOn; }

    /// Das Feldbetrieb-Bild herstellen: ein eigenes Profil „Field",
    /// darin nur die Felder, die am Berg zaehlen. Der Pruefstand geht
    /// ueber den Menueeintrag — dann ist mitgeprueft, dass man
    /// hinkommt.
    void applyFieldLayout();

    /// Die Kennungen, die im Feldbetrieb SICHTBAR bleiben. Alles
    /// andere tritt zurueck. Statisch, damit der Pruefstand dieselbe
    /// Liste liest und nicht eine zweite fuehrt.
    static QStringList fieldLayoutKeepIds();

    /// Panorama im Betrieb ueber CAT (TS-590): +-6 kHz um den Traeger,
    /// denn ueber USB kommt hoechstens ein Seitenband (~5 kHz) an.
    static constexpr double kCatRigBildHz = 12000.0;

    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    /// Die FFT-Groesse fuer ein Geraet: sein eigener Eintrag
    /// (hardware/<mac>/display/fftSize), sonst DisplayFftSize, sonst
    /// `fallback`. Nur gueltige Reglerwerte (4096 .. 262144).
    static int fftSizeForRadio(const QString& mac, int fallback);

    // ── Phase 3M-0 Task 14 test accessors ────────────────────────────────
    // TX Inhibit no longer has a label of its own. It paints onto the TX
    // badge (prohibition symbol) and raises a toast; see setTxInhibited().
    /// True while an external TX Inhibit is asserted.
    bool isTxInhibited() const noexcept { return m_txInhibited; }

    // Returns the PA status badge. Variant is On (green) or Tx (red) per
    // RadioModel::paTripped(). Wiring to RadioModel lands in Task 17.
    // Non-null after construction.
    StatusBadge* paStatusBadge() const noexcept { return m_paStatusBadge; }

    /// Phase 3F Sub-Epic D Task 12: backward-compat accessor that returns
    /// the SpectrumWidget owned by the currently-active pan (via
    /// m_panStack->panadapter(activePanId())->spectrumWidget()).
    /// Returns nullptr during early init before m_panStack is constructed,
    /// or if the active pan has no widget. Long-term migration target:
    /// callers should thread through per-pan PanadapterApplet rather than
    /// reaching for the active pan.
    SpectrumWidget* activeSpectrumWidget() const;

    /// The SpectrumWidget that hosts slice `s`'s panadapter, resolved from
    /// the slice's panKey(). Falls back to the active pan's widget when the
    /// slice has no pan key or the pan no longer exists. Phase 3F multi-pan
    /// flag routing hub; mirrors AetherSDR MainWindow::spectrumForSlice
    /// (MainWindow.cpp:14856 [@6a142807]).
    SpectrumWidget* spectrumForSlice(SliceModel* s) const;
    // Herausgezogen aus rebuildFftRouting() (2026-08-24) -- welcher Pan
    // zeigt diese Scheibe tatsaechlich. Siehe Umsetzung in MainWindow.cpp
    // fuer die Rueckfall-Reihenfolge (panKey, dann Scheiben-Zugehoerigkeit,
    // dann der aktive Pan).
    QString panIdForSlice(SliceModel* slice) const;

    /// The pan-id list a layout template implies. Sole owner of the
    /// template-to-pan-count table, which previously had three copies.
    /// Public (moved from private slots: in Task B1) so the pan-count
    /// table has a direct unit test instead of only being exercised
    /// indirectly through applyPanLayout, which needs a constructed
    /// MainWindow the test harness cannot build.
    static QStringList panIdsForLayout(const QString& layoutId);

    // Narrow composition seams used by deletion-gap regressions. Runtime
    // call sites use these same helpers so stable-ID lookup cannot diverge
    // between the test and the UI signal path.
    // ── Pruefzugaenge fuer den KiwiSDR ───────────────────────────────
    //
    // addKiwiSdrReceiver ist der Weg, den der Menuepunkt geht. Eine
    // Pruefung, die ihn nicht erreicht, koennte nur die API testen —
    // und genau das haette in dieser Sitzung schon dreimal nichts
    // gefunden (Zoomknoepfe ohne Empfaenger, Frequenz erreichte den
    // Panadapter nicht, Kiwi vollstaendig gebaut und unerreichbar).
    void addKiwiSdrReceiverForTest(const QString& name,
                                   const QString& endpoint)
    { addKiwiSdrReceiver(name, endpoint); }
    class KiwiSdrManager* kiwiSdrManagerForTest() const
    { return m_kiwiSdrManager; }
    RadioModel* radioModelForTest() const { return m_radioModel; }
    class InstrumentApplet* signalInstrumentForTest() const;
    class ContestFenster* contestFensterForTest() const { return m_contestFenster; }
    QByteArray layoutprofilAlsJsonForTest(const QString& name) { return layoutprofilAlsJson(name); }
    void holeContestInsKombiFeldForTest(const QString& id, bool zweites = false)
    { holeContestInsKombiFeld(zweites ? m_kombiApplet2 : m_kombiApplet, id); }
    void detachAppletForTest(class AppletWidget* applet);
    class KombiApplet* kombiFeldForTest(int nr) const { return nr == 2 ? m_kombiApplet2 : m_kombiApplet; }
    QStringList kombiListeForTest(class KombiApplet* feld);
    bool waehleKombiEintragForTest(class KombiApplet* feld, const QString& text);
    bool waehleReiterEintragForTest(class KombiApplet* feld, QWidget* panel,
                                    const QString& text);
    int sperreAlleFensterForTest(bool an) { return sperreAlleFenster(an); }

    /// Wie viele Rahmen des Sendetons jetzt faellig sind.
    ///
    /// Herausgezogen, damit die Rechnung einen Prueffall haben kann:
    /// sie entscheidet, ob der Ton in Echtzeit hinausgeht oder gedehnt
    /// (siehe `m_flexSendeUhr`). `maxRahmen` deckelt den einzelnen
    /// Schwall; der Rest bleibt faellig und geht beim naechsten Schlag
    /// mit, sodass ein Haenger aufgeholt und nicht verschluckt wird.
    static qint64 flexFaelligeRahmen(qint64 nsecsVerstrichen,
                                     qint64 geliefert,
                                     qint64 rateHz,
                                     qint64 maxRahmen);

    /// Was dieser Block noch erlebt, bevor er zur Flex geht.
    ///
    /// Drei Schritte in fester Reihenfolge: die Aussteuerung von Hand,
    /// dann der Kanalzug, dann der Riegel. Herausgezogen aus demselben
    /// Grund wie `flexFaelligeRahmen` — ein Prueffall kann sie ohne
    /// Fenster, ohne Geraet und ohne Soundkarte fahren.
    ///
    /// Die Reihenfolge ist nicht beliebig: die Schwellen des
    /// Kompressors und des Tors sind ABSOLUT (dBFS), also muss die
    /// Aussteuerung vor der Kette liegen, sonst arbeitet sie jedes Mal
    /// an einem anderen Pegel. Der Riegel liegt hinter der Kette, weil
    /// er die letzte Sicherung vor dem Wandler ist — auch dann, wenn
    /// die Kette aus ist.
    struct FlexMikBlock {
        /// Spitze NACH der Aussteuerung und VOR der Kette. Das ist die
        /// Zahl, die etwas ueber die QUELLE sagt — die Warnung „das
        /// Mikrofon steht zu hoch" haengt an ihr und nicht an dem, was
        /// ein Kompressor daraus gemacht hat.
        float quelleSpitze{0.0f};
        /// Spitze am Ausgang, vor dem Riegel. Das ist, was hinausgeht.
        float ausgangSpitze{0.0f};
        /// Ob die Kette wirklich gelaufen ist (an, vorhanden, Stimme).
        bool  ketteLief{false};
    };
    static FlexMikBlock flexMikrofonblock(float* proben, int anzahl,
                                          float faktor,
                                          StripChain* kette,
                                          float grenze);
    /// Die Kommandoleiste, damit ein Lauf ihre Knoepfe wirklich
    /// druecken kann statt die Wirkung nachzubauen.
    class CommandBar* commandBarForTest() const { return m_commandBar; }
    /// Die Fensterverwaltung, fuer den Pruefstand. Nur lesen und
    /// schalten -- der Besitz bleibt beim Fenster.
    class AppletVisibilityController* appletVisibilityForTest() const
    { return m_appletVis; }
    /// Das FT8/FT4-Fenster, fuer den Pruefstand. KEIN eigenes
    /// `public:`/`private:` um diesen Block legen -- der Abschnitt ist
    /// schon oeffentlich, und ein Paar darum kippte am 2026-10-21
    /// alles dahinter auf privat (sieben rote CI-Laeufe).
    class Ft8Applet* ft8AppletForTest() const { return m_ft8Applet; }
    class Ft8PlaetzeApplet* ft8PlaetzeAppletForTest() const
    { return m_ft8PlaetzeApplet; }
    /// Ein Durchgang der Plaetze-Automatik, ohne das Prueflauf-Tor
    /// und mit der Zeit von Hand. Der Anschluss (Entscheidung ->
    /// Fenster -> Rueckschlag) war bis zum 2026-10-30 ueberhaupt nicht
    /// fahrbar, weil `isTestModeEnabled()` am Anfang stand und die
    /// ganze Automatik im Lauf abschaltete. SECHS Ursachen sind
    /// nacheinander durch genau diese Luecke gefallen.
    void ft8SlotsAutomatikSchrittForTest(bool hatInhalt, qint64 jetztMs)
    { ft8SlotsAutomatikSchritt(hatInhalt, 0, 0, jetztMs); }
    /// Ob das Plaetze-Panel ein eigenes Fenster hat -- die Groesse,
    /// die der Betreiber meint, wenn er „kein Fenster" sagt.
    bool ft8SlotsHatFensterForTest() const;
    /// Das Fenster selbst, um seine Hoehe zu messen.
    class QWidget* ft8SlotsFensterForTest() const;
    /// Ob die Automatik das Panel fuer Eigentum des Betreibers haelt.
    bool ft8SlotsGehoertDemBetreiberForTest() const
    { return m_ft8SlotsAutomatik.gehoertDemBetreiber(); }
    /// Die Automatik von vorn. Noetig, weil die Prueffaelle EINE
    /// Einstellungsdatei teilen: nach dem ersten Fall steht
    /// `AppletFt8SlotsVisible=True`, und `setVisible(id, true)` ist
    /// dann ein Nichts, das kein Signal aussendet.
    void ft8SlotsAutomatikZuruecksetzenForTest()
    { m_ft8SlotsAutomatik.zuruecksetzen(); }


    // ── Der Schutzrahmen (2026-10-14) ────────────────────────────────
    //
    // Auf Ansage des Betreibers: „es sollte vielleicht einen grossen
    // Rahmen geben, ueber den man nichts ziehen kann und keine Fenster
    // anordnen kann." Siehe die lange Begruendung bei
    // `ausSperrstreifenSchieben()` in WindowPlacement.h.

    /// Die Streifen des Hauptfensters, auf denen kein schwebendes
    /// Fenster liegen darf, in GLOBALEN Koordinaten.
    ///
    /// Das sind die beiden Bedienleisten: die Kommandoleiste oben (Band,
    /// Mode, Filter, Step, NR, Rate, Profil) und die Statuszeile unten
    /// (MOX, VOX, TUNE, PS, CAT, TCI, CPU, PA, ON AIR). Links und rechts
    /// gibt es keinen Rahmen: dort steht keine Bedienung, und ein
    /// seitliches Ausweichen wuerde eine Anordnung zerreissen, die der
    /// Betreiber nebeneinander gestellt hat.
    ///
    /// Leer, solange das Fenster nicht sichtbar ist -- dann sind die
    /// Leisten noch nicht an ihrem Platz, und ein Rahmen aus Nullen
    /// schoebe jedes Fenster an den oberen Rand.
    /// Die Lage eines schwebenden Fensters ins Profil schreiben --
    /// ABSOLUT (wie bisher) UND relativ zum Hauptfenster.
    ///
    /// Der Grund (2026-10-14): bis heute standen nur absolute
    /// Bildschirmkoordinaten im Profil. Eine Anordnung, die im Vollbild
    /// entstanden ist, passt damit in keiner anderen Fenstergroesse --
    /// das Hauptfenster rutscht um die Titelleiste nach unten, die
    /// schwebenden Fenster bleiben stehen, und sie liegen auf der
    /// Kommandoleiste. Genau der Fall, den der Betreiber mit zwei
    /// Bildschirmfotos gemeldet hat.
    ///
    /// Die absoluten Werte bleiben stehen: ein Profil, das eine aeltere
    /// Fassung liest, soll weiter funktionieren, und fuer ein Fenster
    /// auf einem ZWEITEN Schirm ist die absolute Lage die richtige.
    void merkeLage(QVariantMap& ziel, const QRect& g) const;

    /// Das Gegenstueck: die heutige Lage aus einem Profileintrag.
    /// Traegt er `dx`/`dy`, wird daraus und aus der JETZIGEN Lage des
    /// Hauptfensters gerechnet; sonst gelten die alten absoluten Werte.
    QRect lageAusProfil(const QVariantMap& quelle) const;

    QList<QRect> schutzrahmen() const;

    /// Alle schwebenden Fenster (Applets, Panadapter, Rotor/Log) aus dem
    /// Schutzrahmen holen. Tut nichts an Fenstern, die ihn ohnehin nicht
    /// beruehren -- auch nicht an solchen auf einem zweiten Schirm.
    /// Gibt zurueck, wie viele verschoben wurden (fuer Prueffaelle).
    int schwebendeAusDemSchutzrahmenHolen();

    /// Das Geraet auf FT8 oder FT4 herrichten -- in einem Griff.
    ///
    /// Betriebsart DIGU, Skala auf die Anrufstelle des Bandes, Filter
    /// weit genug fuer den ganzen Durchlass, Dekoderfenster auf. Was
    /// schon stimmt, wird nicht angefasst; steht die Skala bereits auf
    /// einer Anrufstelle, bleibt sie stehen (wer von Hand danebensteht,
    /// meint es so).
    ///
    /// Auch fuer den Pruefstand da: so laesst sich der Griff ohne
    /// Mausklick in die Leiste fahren. Der Abschnitt hier ist schon
    /// oeffentlich -- ein eigenes `public:`/`private:` darum herum
    /// kippte am 2026-10-21 alles DAHINTER auf privat und brach sieben
    /// CI-Laeufe (`appletIsRegisteredForTest` war ploetzlich privat).
    void aufDigiStellen(Longpath::DigiFrequenzen::Digimodus modus);

    /// Band umschalten -- und den Digi-Betrieb dabei nicht verlieren.
    ///
    /// Betreiber am 2026-10-31: „habe von 40 auf 20 meter gewechselt
    /// und der decoder ist verschwunden!" Das war kein Fehler im
    /// Dekoder. `RadioModel::onBandButtonClicked` holt fuer ein schon
    /// besuchtes Band Frequenz UND Betriebsart aus dem Bandgedaechtnis
    /// zurueck -- auf 20 m lagen dort 14.294700 MHz in USB, von einem
    /// Sprechabend. Damit fiel die Betriebsart aus DIGU heraus, und das
    /// Verfuegbarkeitstor in `rebindRttyRadeAvailability` (DIGU/DIGL,
    /// sonst nichts) nahm das Dekoderfenster weg. Richtig, nur nicht
    /// gemeint: wer mitten im FT8-Betrieb das Band wechselt, will FT8
    /// auf dem neuen Band, nicht den letzten Sprechkanal.
    ///
    /// Also: lief Digi, wird nach dem Bandwechsel `aufDigiStellen()`
    /// nachgezogen -- Anrufstelle des NEUEN Bandes, DIGU, Filter,
    /// Fenster. Lief es nicht, aendert sich nichts.
    ///
    /// Jeder Weg zum Bandwechsel geht hier durch (Leiste, Panel,
    /// Behaelterknoepfe, Antennenfenster). Die Seitenband-Regel vom
    /// 2026-10-23 musste an drei Stellen einzeln nachgetragen werden,
    /// weil es diesen einen Weg noch nicht gab.
    void bandUmschalten(Longpath::Band band);

    /// Die Tastenkuerzel (2026-11-10): der Verteiler, fuer Pruefstaende.
    class HotKeyDispatcher* hotKeyDispatcher() const { return m_hotKeyDispatcher.get(); }
    /// Am Mac: F1…F12 als F-Tasten, solange Longpath vorne ist (2026-11-11).
    class FKeyPriority* fKeyPriority() const { return m_fKeyPriority.get(); }

    /// Bandstapel (2026-11-11, Entwurf D1): die Marken aller
    /// Panadapter neu setzen -- nach jeder Aenderung am Stapel, einem
    /// Wechsel der Scheibe oder dem Schalter im Zahnrad des Applets.
    void bandstapelMarkenNachziehen();
    /// Dasselbe fuer einen Panadapter: die Eintraege mit der Stelle der
    /// Scheibe, die er zeigt (sliceForPan). Leer, wenn der Schalter aus
    /// ist oder kein Geraet verbunden.
    void bandstapelMarkenFuer(class SpectrumWidget* sw, const QString& panId);
    /// Das an Band/Modus/Digimodus gebundene Layoutprofil aktivieren,
    /// falls eines passt. Siehe `LayoutProfiles::profileFor`.
    void profilNachstellen(
        std::optional<Longpath::DigiFrequenzen::Digimodus> digi
            = std::nullopt);

    /// Der Weg zurueck: die Betriebsart stellen, an die das AKTIVE
    /// Profil gebunden ist (2026-11-15).
    ///
    /// Betreiber: „weiters muss auch im ft8 profil - F - immer
    /// automatisch DIGI mode sein ... derzeit muss ich immer im profil
    /// auf FT8 zb klicken."
    ///
    /// Nur bei einer EINDEUTIGEN Bindung, und nur, wenn eine Scheibe da
    /// ist. Ohne Bindung bleibt die Betriebsart, wo sie ist -- „Neu",
    /// „LOGBUCH" und „CONTEST" fassen sie also nie an. Siehe
    /// `LayoutProfiles::eindeutigeBetriebsart`.
    void profilBetriebsartStellen();

    /// Welcher Digimodus zuletzt gemeint war. Der DIGU/DIGL-Knopf in
    /// der Leiste traegt keinen -- er richtet mit diesem her.
    Longpath::DigiFrequenzen::Digimodus m_letzterDigiModus{
        Longpath::DigiFrequenzen::Digimodus::Ft8};

    /// Dasselbe mit Nachlauf: waehrend eines Groessenzugs feuert
    /// resizeEvent bei jedem Pixel, und Fenster, die dabei im Takt
    /// springen, sind unbedienbar. 250 ms nach der letzten Aenderung
    /// einmal.
    void schutzrahmenNachziehen();

    /// Die schwebenden Fenster folgen dem Hauptfenster (2026-11-04).
    ///
    /// Betreiber: "sobald ich das _ oder das + usw. links oben verwenden
    /// muss sich das ganze!!!! Fenster bewegen! auch die freischwebenden
    /// im verhältnis!" Ein Zug am Titelbalken, das "+" (Zoom), Vollbild:
    /// das Hauptfenster bekommt einen neuen Rahmen, und jedes schwebende
    /// Fenster (dieselbe Aufzaehlung wie beim Schutzrahmen: Qt::Tool-
    /// Kinder des Hauptfensters, keine Dialoge) wird von der alten Lage
    /// IM VERHAELTNIS zum alten Rahmen auf den neuen abgebildet. Seit
    /// 2026-11-11 auch die Groesse („wenn ich das mache, müssen alle
    /// anderen fenster auch im verhältnis kleiner werden"); ein reiner
    /// Zug laesst sie, wie sie ist. Laeuft im Nachlauf des Schutzrahmens
    /// (250 ms), VOR dem Herausholen aus den Sperrstreifen.
    ///
    /// `m_hauptrahmenBasis` ist der Rahmen, zu dem die Lagen zuletzt
    /// passten. Eine EIGENE Bewegung (Profil anwenden, Aufbau, Raster)
    /// setzt nur die Basis neu und zieht nichts: dort setzt das Profil
    /// die schwebenden Fenster ohnehin selbst.
    int schwebendeMitziehen();
    /// Abstand der Arbeitsflaeche zum Rahmen: oben Titel-, Menue- und
    /// Befehlsleiste, unten die Statusleiste (2026-11-12). Diese Leisten
    /// wachsen nicht mit; umgerechnet wird nur die Flaeche dazwischen.
    QMargins leistenRaender() const;
    /// Je schwebendem Fenster seine Lage und Groesse als Anteil der
    /// Arbeitsflaeche (2026-11-12). Gilt, solange das Fenster dort steht,
    /// wo das Mitziehen oder der Schutzrahmen es zuletzt hingesetzt hat;
    /// steht es woanders (eigener Zug, Profil, Anordnen), wird neu
    /// gemessen. So kommen nach Klein- und wieder Grossziehen alle
    /// Fenster genau zurueck, auch wenn eines unterwegs an seiner
    /// Untergrenze hing oder aus einer Leiste geschoben wurde.
    struct MitziehBasis { double x{0}, y{0}, w{0}, h{0}; QRect gesetzt; };
    QHash<const QWidget*, MitziehBasis> m_mitziehBasis;
    QRect m_hauptrahmenBasis;
    // Die Rechnung selbst: Lagen und Groessen von `alt` auf `neu`
    // (beide Hauptfenster-Rahmen). basisFrisch: die Anteile neu messen,
    // auch wenn eine Grundlage da waere.
    int schwebendeUmrechnen(const QRect& alt, const QRect& neu, bool basisFrisch);

    // ── Die Lage gehoert zu ihrem Hauptfenster (2026-11-12) ──────────
    // Betreiber: „ich habe alles gespeichert und wieder ist alles
    // verschoben" -- „für was speichere ich die ab wenn sie immer anders
    // erscheinen". Ein Profil nimmt seit heute den Hauptfenster-Rahmen
    // mit auf. Beim Herstellen werden die schwebenden Fenster von DIESEM
    // Rahmen auf den heutigen umgerechnet, und die Anteile bleiben die
    // Grundlage fuer jedes weitere Mitziehen. Vorher galten die
    // gespeicherten Punkte absolut: das Hauptfenster kam 865 hoch herauf
    // (ueber dem Dock), sprang spaeter auf 923, alle Fenster zogen um
    // 6,7 % mit -- und genau das wurde beim Beenden gespeichert. Jeder
    // Start schob weiter.
    QRect m_profilRahmen;
    bool  m_profilRahmenOffen{false};
    /// Rechnet um, sobald Hauptfenster und schwebende Fenster stehen.
    /// true, wenn es getan ist (dann ist nichts mehr offen).
    bool schwebendeNachProfilrahmen();
    /// Der Rahmen beim letzten Lage-/Groessenereignis -- um den kleinen
    /// Nachhall des Rasters von einer echten eigenen Bewegung zu trennen.
    QRect m_letzterHauptrahmen;
    /// Im laufenden Nachlauf war eine FREMDE Bewegung (Zug, "+",
    /// Vollbild). Nur dann wird gezogen; und nur dann ist ein kleiner
    /// eigener Sprung danach der Nachhall des Rasters.
    bool m_fremdeBewegungAnhaengig{false};


    /// Den Schutzrahmen sichtbar machen, solange ein schwebendes
    /// Fenster gezogen wird (2026-10-14). Blendet sich 600 ms nach der
    /// letzten Bewegung von selbst wieder aus.
    void zeigeSchutzrahmenKurz();

    /// Nur fuer Prueffaelle: liegt der Hinweis gerade sichtbar ueber den
    /// Leisten?
    bool schutzrahmenHinweisSichtbarFuerTest() const;

    // ── Pruefzugaenge fuer „hinter die Connect-Maske" ────────────────
    //
    // Die Regel ist nur an einem laufenden MainWindow pruefbar (die
    // Maske selbst entscheidet mit), aber ein Pruefstand kann sich kein
    // echtes Rotor/Log-Fenster ins Layout stellen, ohne ein Profil
    // anzufassen. Also ein Stellvertreter: irgendein Fenster in die
    // Liste legen und danach fragen, ob es schon zurueck ist.
    int schwebendeMitziehenForTest() { return schwebendeMitziehen(); }
    void vergissMitziehBasisForTest() { m_mitziehBasis.clear(); }   // wie ein frischer Start
    // Die Grundlage, mit der das Mitziehen gerechnet HAT -- als Text fuer
    // eine Fehlermeldung. Ohne sie bleibt ein roter Pruefstand ein Raten:
    // eine wiederverwendete Grundlage aus dem Fall davor und eine frisch
    // gemessene fuehren zu verschiedenen Groessen, und beides sieht im
    // Protokoll gleich aus (CI-Lauf 43619257941, Shard Linux 2/4).
    // Der Rahmen, GEGEN den das naechste Mitziehen rechnet. Er wandert
    // bei jedem Schritt des Hauptfensters mit -- auch dann, wenn der
    // Durchlauf danach nichts umrechnet. Passt er nicht zu den Groessen
    // der schwebenden Fenster, rechnet die naechste frische Messung
    // gegen einen Rahmen, den sie nie gesehen haben.
    QRect hauptrahmenBasisForTest() const { return m_hauptrahmenBasis; }

    QString mitziehBasisAlsTextForTest(const QWidget* w) const
    {
        const auto bi = m_mitziehBasis.constFind(w);
        if (bi == m_mitziehBasis.constEnd()) { return QStringLiteral("none"); }
        return QStringLiteral("ratios x=%1 y=%2 w=%3 h=%4, set %5,%6 %7x%8")
            .arg(bi->x, 0, 'f', 4).arg(bi->y, 0, 'f', 4)
            .arg(bi->w, 0, 'f', 4).arg(bi->h, 0, 'f', 4)
            .arg(bi->gesetzt.x()).arg(bi->gesetzt.y())
            .arg(bi->gesetzt.width()).arg(bi->gesetzt.height());
    }
    void setProfilRahmenForTest(const QRect& r) { m_profilRahmen = r; m_profilRahmenOffen = r.isValid(); }
    bool schwebendeNachProfilrahmenForTest() { return schwebendeNachProfilrahmen(); }
    QMargins leistenRaenderForTest() const { return leistenRaender(); }
    void rememberFloatingWindowBehindConnectMaskForTest(QWidget* w)
    {
        if (!w) { return; }
        w->hide();
        m_floatingContainersHiddenPreConnect.append(w);
    }
    int floatingWindowsWaitingForConnectMaskForTest() const
    { return int(m_floatingContainersHiddenPreConnect.size()); }
    int schwebendeFensterImDockForTest() const
    { return int(m_schwebendeFensterImDock.size()); }
    void restoreFloatingWindowsForTest()
    { restoreFloatingWindowsHiddenBehindConnectMask(); }

    /// Pruefzugang (2026-10-17): steht das Fenster im RAHMENLOSEN
    /// Vollbild? Seit heute ist die Antwort beim Start `false` --
    /// formatfuellend wird ueber showMaximized() erreicht, damit die
    /// drei Knoepfe links oben bleiben. Siehe tst_fenster_hat_rahmen.
    bool rahmenlosesVollbildFuerTest() const { return m_borderlessFullSize; }

    // ── Pruefzugaenge fuer den Seitenbereich (tst_side_area_integration) ──
    void setSideAreaEnabledForTest(bool on) { setSideAreaEnabled(on); }
    bool addToSideAreaForTest(const QString& id, bool activate)
    { return addToSideArea(id, activate); }
    void removeFromSideAreaToOwnWindowForTest(const QString& id)
    { removeFromSideArea(id, SideAreaExit::OwnWindow); }
    class SideAreaWindow* sideAreaForTest() const { return sideArea(); }
    class AppletWidget* appletForKeyForTest(const QString& key) const
    { return appletForKey(key); }
    class ToolWindow* rotorWindowForTest() const { return m_rotorWindow; }
    QString sideAreaIdForWindowForTest(QWidget* w) const
    { return sideAreaIdForWindow(w); }
    QString sideAreaKeyForForTest(const QString& id) const
    { return sideAreaKeyFor(id); }

    // ── Pruefzugaenge fuer den SunSDR ────────────────────────────────
    //
    // connectSunSdr ist der Weg, den der Menuepunkt geht — derselbe
    // Grund wie bei addKiwiSdrReceiverForTest oben: eine Pruefung, die
    // nur die TciClient-API anspricht, koennte "gebaut und unerreichbar"
    // nicht aufdecken.
    void connectSunSdrForTest(const QString& endpoint) { connectSunSdr(endpoint); }
    void disconnectSunSdrForTest() { disconnectSunSdr(); }
    class TciClient* sunSdrClientForTest() const { return m_sunSdrClient; }
    int sunSdrTargetSliceForTest() const { return m_sunSdrTargetSliceId; }
    // Der reservierte Pseudo-Stream-Index lebt als anonymes-Namespace-
    // Konstante in MainWindow_SunSdr.cpp (siehe dort, "Rechercheergebnis
    // 2026-08-24") -- diese Funktion ist der einzige Weg fuer eine
    // Pruefung, denselben Wert zu bekommen, ohne ihn ein zweites Mal
    // abzuschreiben (und damit stillschweigend auseinanderlaufen zu
    // koennen).
    static int sunSdrPseudoStreamIndexForTest();

    /// Ist dieses Applet ueber den Auswaehler erreichbar?
    ///
    /// Fuer tst_every_applet_is_reachable. Gebaut und nicht erreichbar
    /// ist so gut wie nicht gebaut, und genau das ist am 2026-08-23
    /// dreimal passiert.
    /// Ob ein Panel laut Bedienerwunsch sichtbar ist. Gebraucht fuer
    /// die Pruefung, dass das Hinzufuegen eines KiwiSDR sein Panel
    /// aufmacht -- vorher war das Ergebnis des Hinzufuegens unsichtbar.
    bool appletVisibleForTest(const QString& id) const;
    void setAppletVisibleForTest(const QString& id, bool visible);

    bool appletIsRegisteredForTest(const class AppletWidget* a) const
    {
        for (auto it = m_appletsById.cbegin(); it != m_appletsById.cend(); ++it) {
            if (it.value() == a) { return true; }
        }
        return false;
    }

    static SliceModel* sliceForAddedIdForTest(RadioModel* model, int sliceId);
    static void applyAntennaChangeForTest(RadioModel* model, int sliceId,
                                          const QString& antennaName);
    static void configureSpectrumForPanForTest(SpectrumWidget* spectrum,
                                                const QString& panId);
    static void wireWidebandExtensionForTest(SpectrumWidget* spectrum,
                                             RadioModel* model,
                                             PanadapterStack* stack,
                                             const QString& panId);
    static void fanWidebandBinsForTest(PanadapterStack* stack, int adcIndex,
                                       const QVector<float>& bins);

    // ── TNF operator controls (design sections 7, 7.5 and 10.2) ───────────
    // Public statics rather than file-local helpers: MainWindow needs a full
    // RadioModel (WDSP, audio, network) to construct, which no unit-test
    // executable can afford, so this is the only shape in which the
    // indicator's and the notice's pure behaviours can be tested. All three
    // are called from production code below.

    /// Status-bar TNF light. Struck through in every off state, and
    /// escalated to the amber warning colour once notches exist and are
    /// being bypassed: under maintainer decision D-a the master enable ships
    /// OFF, so the operator's first notch does nothing until they turn it
    /// on, and that state has to be unmistakable rather than a subtle tint.
    static QString tnfIndicatorStyleSheet(bool globalEnabled, int notchCount);

    /// Status-bar TNF light tooltip: how many notches exist, whether they
    /// are doing anything, and that the click toggles them.
    static QString tnfIndicatorTooltip(int notchCount, bool globalEnabled);

    /// The DSP > TNF accelerator. Public and static so the collision test can
    /// read it without an instance; design section 10.2 fixes it in code
    /// because Longpath has no shortcut-assignment subsystem to register
    /// with.
    static QKeySequence tnfToggleShortcut();

    /// Operator notice for a rejected notch add. Pure so the wording can be
    /// pinned without standing MainWindow up.
    static QString tnfAddRejectedNotice(const QString& reason);

    /// Ist dieses Fenster OHNE die gespeicherte Anordnung hochgefahren,
    /// weil zuvor zwei Starts hintereinander steckenblieben? Siehe
    /// core/StartWatch.h.
    bool startedSafely() const { return m_startedSafely; }
    /// Wieviele Starts davor nicht durchkamen (0, wenn normal
    /// hochgefahren).
    int  failedStartCount() const { return m_failedStartCount; }

public slots:
    // ── Phase 3M-0 Task 14 helper slots ──────────────────────────────────
    // Update PA status badge state. Wired by Task 17 to
    // RadioModel::paTrippedChanged.
    void setPaTripped(bool tripped);

    // Update TX Inhibit label visibility. Wired by Task 17 to
    // TxInhibitMonitor::txInhibitedChanged.
    void setTxInhibited(bool inhibited);

    // Task 3.6: live-apply CPU meter update rate from GeneralOptionsPage spinbox.
    // hz is clamped to [1, 30]. Restarts m_cpuTimer with the new interval.
    void setCpuTimerIntervalHz(int hz);

    // Task 3.6: live-apply ANAN-8000DLE volts/amps visibility preference.
    // Called when the "Show volts/amps in title bar" checkbox changes.
    // Only has visible effect when the connected radio is an ANAN-8000D
    // (the SystemTile PA row is already auto-hidden for non-MKII boards).
    void setVoltsAmpsVisible(bool visible);

protected:
    void closeEvent(QCloseEvent* event) override;
    /// Ablage ins Dock (Minimieren) nimmt die schwebenden Fenster mit,
    /// das Zurueckholen bringt sie wieder -- siehe die Definition.
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void moveEvent(QMoveEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    /// Phase 3F: repaint every pan's status overlay from the slice that pan
    /// is showing (its own activeSliceIndex), with the ADC chain resolved
    /// through RadioModel::sliceChainIndex so the CH tag agrees with the WIDE
    /// pill beside it.
    ///
    /// A slot rather than a plain method because the per-slice triggers are
    /// connected through QMetaMethod, driven by
    /// PanadapterApplet::statusOverlaySliceProperties, so that adding an
    /// overlay field never means remembering to add a connect here.
    ///
    /// Every pan is refreshed on every pass, matching refreshPanWideBadges.
    /// Cheap: single-digit pans, and each overlay setter drops a no-op write
    /// before it repaints, so a VFO detent repaints exactly the one pan whose
    /// frequency actually moved.
    void refreshPanStatusOverlays();

    /// Phase 3F: connect the status-overlay badge clicks on EVERY pan.
    ///
    /// Armed from PanadapterStack::countChanged, the same hook
    /// wirePanStatusOverlayTriggers uses, so pans created after startup by a
    /// layout switch or an Add Panadapter action are wired too -- which is
    /// every pan except pan-0. Before this, MainWindow connected the three
    /// signals for the single applet it could name at construction, so on a
    /// multi-pan layout the badges painted correctly everywhere (00ab9522,
    /// 0896b4f3) and responded nowhere else.
    ///
    /// Idempotent, because re-arming on every countChanged would otherwise
    /// stack duplicate connections and open one dialog per layout switch the
    /// operator had ever made. The handlers below are SLOTS, not lambdas,
    /// specifically so Qt::UniqueConnection actually dedups.
    ///
    /// Qt6 does NOT quietly drop the flag on a functor target — it warns
    /// ("unique connections require a pointer to member function of a
    /// QObject subclass") and returns an invalid Connection, so the
    /// connect never happens at all. A lambda here would therefore not
    /// stack duplicates; it would be dead. Both outcomes are wrong, and
    /// the second is the one that hides.
    void wirePanBadgeHandlers();

    /// Give every panadapter its own control strip (+RX / BAND / ANT /
    /// Display). Idempotent and re-armed from PanadapterStack::countChanged,
    /// so pans created later get one by construction. Each strip carries its
    /// own panId and its controls act on that pan.
    void ensureOverlayPanels();

    /// Push the live slice-id list to MeterPoller so every flag's S-meter bar
    /// is fed. Must be called on slice add/remove, not just on pan-count
    /// change: a slice added to an existing pan moves no pan count.
    void refreshMeterPollerSlices();

    /// TNF: push the global notch list at EVERY pan (design section 8.1).
    ///
    /// Under D1 the notch list is global, so each pan gets the same vector
    /// and converts it into its own pixel space. Deliberately not the spot
    /// overlay's activeSpectrumWidget()-only shape, which would leave every
    /// secondary pan blank.
    ///
    /// The only Hz-to-MHz conversion site in the TNF stack: NotchModel
    /// stores absolute RF Hz, NotchMarker::freqMhz is MHz, and everything
    /// else (the five interaction signals, setNotchMinWidthHz, the dent
    /// maths) stays in Hz.
    void refreshPanNotchMarkers();

    /// TNF: push the visual-notch (trace dent) toggle at EVERY pan
    /// (design section 8.3).
    ///
    /// Same shape as refreshPanNotchMarkers above, and for the same reason:
    /// the toggle is one global NotchModel flag but each pan owns its own
    /// SpectrumWidget, so it has to reach pans created after startup too.
    /// Armed from PanadapterStack::countChanged and from
    /// NotchModel::visualEnabledChanged.
    void refreshPanVisualNotch();

    /// TNF: push WDSP's minimum notch width at EVERY pan
    /// (design sections 7.2 and 8.3).
    ///
    /// SpectrumWidget cannot pull this: it varies with the filter's
    /// coefficient count and the channel's DSP rate, and neither is visible
    /// from the GUI layer. Without the push every pan keeps the 100 Hz
    /// construction default forever, which silently mis-sizes both the
    /// edge-drag clamp and the dent span the moment the operator changes nc
    /// on the DSP Options page or the radio's sample rate.
    ///
    /// Resolved per pan through that pan's OWN activeSliceIndex, matching
    /// refreshPanStatusOverlays, because a multi-pan layout can host slices
    /// on channels with different rates. Re-arms the per-channel
    /// RxChannel::minNotchWidthChanged follow on every pass
    /// (Qt::UniqueConnection), since the channel a pan resolves to changes
    /// with the slice set.
    void refreshPanNotchMinWidth();

    /// TNF: connect the five per-pan notch interaction signals on EVERY pan.
    ///
    /// Armed from PanadapterStack::countChanged, the same hook
    /// wirePanBadgeHandlers uses, and for the same reason: a pan created by
    /// a layout switch would otherwise never be wired. Not folded into
    /// wireSpectrumForPan, which skips pan-0 by design.
    ///
    /// The handlers below are SLOTS, not lambdas, so Qt::UniqueConnection is
    /// actually honoured on the re-arm.
    void wirePanNotchHandlers();

    /// TNF inbound handlers. Each mutates the single global NotchModel; the
    /// frequencies arrive already resolved in the emitting pan's own
    /// frequency mapping, so nothing here consults an active pan.

    /// SpectrumWidget::notchCreateRequested carries no pan id — the widget
    /// does not know its own — so this recovers it from sender() and hands
    /// off to onNotchCreateRequested below.
    ///
    /// A slot rather than a pan-id-binding lambda for a harder reason than
    /// the sibling handlers: Qt6 does not merely ignore Qt::UniqueConnection
    /// on a functor target, it refuses the connect outright and returns an
    /// invalid Connection. As a lambda this signal was not connected at all.
    void onNotchCreateFromPan(double freqHz, bool narrow);
    // Einmessen an einem Traeger (2026-11-08): Ergebnis der Messung eines
    // Panadapters -> Dialog (TraegerEichDialog).
    void zeigeTraegerEichung(bool gefunden, double geraetHz, double anzeigeHz,
                             float snrDb);
    // Rundum-Lauf (2026-11-08): Tools -> Rotor sweep.
    void oeffneRundumLauf();
    void onNotchCreateRequested(const QString& panId, double freqHz, bool narrow);
    void onNotchMoveRequested(int id, double newFreqHz);
    void onNotchWidthRequested(int id, double widthHz);
    void onNotchActiveRequested(int id, bool active);
    void onNotchRemoveRequested(int id);
    void onNotchRemoveAll();
    /// Alle Notch-Filter aus der Fussleiste erreichbar machen.
    void showNotchBarMenu(const QPoint& globalPos);
    /// Einstellungsseite einer Rauschminderung oeffnen (RX-Feld und
    /// obere Leiste gehen denselben Weg).
    void openNrSetupPage(Longpath::NrSlot slot);

    /// TNF: the +TNF overlay button on pan `panId`. A distinct slot from
    /// onNotchCreateRequested above, which takes an already-resolved
    /// frequency from a panadapter click; this one has to compose the centre
    /// itself from the pan's own slice (design section 7.5).
    ///
    /// A slot, not a lambda, for the same reason the five handlers above
    /// are: ensureOverlayPanels re-runs on every
    /// PanadapterStack::countChanged, and Qt::UniqueConnection only works
    /// on a pointer-to-member target. On a lambda Qt6 warns and refuses
    /// the connect outright — the handler would never fire.
    void onAddTnfClicked(const QString& panId);

    /// Ein Loch im IQ-Strom: jeder Panadapter verwirft sein angefangenes
    /// FFT-Fenster. Ein Slot, kein Lambda (2026-09-27): wireRtt verbindet
    /// ihn bei jedem Verbindungsversuch neu, mit Qt::UniqueConnection --
    /// und an einem Lambda verweigerte Qt6 genau diesen connect ("unique
    /// connections require a pointer to member function", stand bei
    /// jedem Verbinden im Protokoll). Das Verwerfen geschah also nie.
    void onIqSequenceGap(int hwReceiverIndex);

    /// TNF: surface a rejected add. Without this a +TNF press inside the
    /// 10 Hz dedupe window is silently ignored and the button reads as dead.
    void onNotchAddRejected(const QString& reason);

    /// TNF: repaint the status-bar light from NotchModel. Driven by every
    /// signal that can change either half of what it shows.
    void refreshTnfIndicator();

    /// Wire one panadapter's spot, connection and MaxBin hooks. Every one of
    /// these used to be connected once to pan-0's widget, leaving other pans
    /// inert. The four controls that TARGET A SLICE live in
    /// wireSpectrumSliceControls below, because pan-0 needs those and does not
    /// come through here.
    void wireSpectrumForPan(class SpectrumWidget* sw, const QString& panId);
    // Befund 4 (2026-10-22): die Zustaende des Zweiton-Felds je Pan statt
    // einmalig an activeSpectrumWidget(). Siehe die Erklaerung in der .cpp.
    void wireImdOverlayForPan(class SpectrumWidget* sw);
    void wireImdOverlayForAllPans();
    // Die sieben Anzeigeregler der Knopfleiste, je Pan an das eigene
    // Spektrum statt einmalig an activeSpectrumWidget() (2026-10-22).
    void wireDisplayControlsForPan(class SpectrumOverlayPanel* panel,
                                   class SpectrumWidget* sw);

    /// Wire the four spectrum controls that act on a slice: click-to-tune,
    /// filter-edge drag, pan drag, CTUN toggle.
    ///
    /// Split out of wireSpectrumForPan for the bench defect of 2026-07-28,
    /// where click-to-tune retuned Slice A however many times the operator
    /// selected another flag. ensureOverlayPanels deliberately skips
    /// wireSpectrumForPan for pan-0 (its spot / connection / MaxBin hooks are
    /// wired elsewhere and would double), so pan-0 was left running an older
    /// copy of these four in wireSliceToSpectrum whose lambdas captured
    /// RadioModel::activeSlice() by value at connect time. That pointer is
    /// Slice A and never moved, so on the one pan almost every operator uses,
    /// none of the four ever consulted the pan's active slice at all.
    ///
    /// Called for EVERY pan, pan-0 included, and the sole home for these four
    /// signals: verify-no-captured-slice-spectrum-wiring.py fails the build if
    /// any of them is connected to a sender other than this function's `sw`.
    /// Each handler resolves its target through sliceForPan(panId) at signal
    /// time rather than capturing it, which is what makes it follow the
    /// operator's selection.
    void wireSpectrumSliceControls(class SpectrumWidget* sw,
                                   const QString& panId);

    /// Begleitgeraet: Marke in jeden Panadapter, Fehler in die
    /// Statuszeile, und -- wenn eingeschaltet -- die Dauerverbindung
    /// aufnehmen. Einmal aus dem Ctor, nachdem die Oberflaeche steht.
    void verdrahteBegleitgeraet();

    /// Push the live connection state into every pan's spectrum widget.
    /// SpectrumWidget::mousePressEvent returns early when it believes the
    /// radio is disconnected, so a widget that never receives this is inert to
    /// every mouse press.
    void pushConnectionStateToPans();

    /// The slice a pan hosts -- its own active slice if it has one, else the
    /// first slice associated with it. nullptr when the pan has no slices.
    SliceModel* sliceForPan(const QString& panId) const;

    /// Phase 3F: WIDE pill / CH tag both open FilterPolicyDialog on the chain
    /// feeding the CLICKED pan, and the TX pill asks the arbiter to hand the
    /// transmitter to that pan's active slice.
    ///
    /// Each takes the pan id rather than assuming the active pan: an operator
    /// looking at a WIDE pill on pan 1 is asking about pan 1's chain whether
    /// or not pan 1 is the pan with focus.
    void onPanWideBadgeClicked(const QString& panId);
    void onPanChainTagClicked(const QString& panId, int chainIdx);
    void onPanTxBadgeClicked(const QString& panId);

    /// Task B5: PanadapterApplet::addSliceRequested / floatRequested both
    /// carry the emitting applet's own panId(), so these forward straight to
    /// RadioModel::addSliceOnPan / PanadapterStack::floatPanadapter with no
    /// activePanId() lookup -- the same "acts on the pan that was clicked"
    /// shape as the three handlers above. Named slots rather than lambdas:
    /// wirePanBadgeHandlers() re-runs on every countChanged, and
    /// Qt::UniqueConnection only works on a pointer-to-member target: on a
    /// lambda Qt6 warns and refuses the connect (see the comment on the
    /// `activated` connect in wirePanBadgeHandlers()). Either the dedup
    /// fails or the handler never runs — a named slot avoids the question.
    void onPanAddSliceRequested(const QString& panId);
    void onPanFloatRequested(const QString& panId);
    void onPanDockRequested(const QString& panId);

    /// Wuensche aus dem Zahnrad in der Panadapter-Kopfleiste. Das
    /// Applet aendert nichts selbst — es kennt den Renderer nicht.
    void onPanBackgroundImage(const QString& path);
    void onPanBackgroundOpacity(int percent);
    void onPanBackgroundColour();
    void onPanBackgroundColourReset();
    void onPanDisplaySetup();

    /// Windrose im Spektrum ein-/ausblenden und ihr Bild nachfuehren.
    /// Gemalt wird sie vom Rotorzeiger selbst — EIN Zifferblatt im
    /// Programm, nicht zwei, die auseinanderlaufen koennen.
    /// Startet oder stoppt den Taktgeber je nachdem, ob
    /// ueberhaupt eine Einblendung zu sehen ist.

    void onConnectionStateChanged();
    void showConnectionPanel();
    void showSupportDialog();
    void showAudioDiagnoseDialog();
    /// Der PipeWire-Erststart unter Linux (2026-10-22 herausgeloest).
    void showLinuxAudioFirstRunDialog();
    void showFeatureRequestDialog();
    void showFeatureRequestDialogImpl();
    // Phase 3M-4 Task 8: open the modeless PureSignal dialog (Tools menu).
    // Lazy-constructs on first invocation; subsequent calls show + raise the
    // existing instance so geometry persists across opens.
    void openPureSignalDialog();
    // Phase 3J-2 H1: open the modeless Spot Hub / FreeDV Reporter dialogs
    // (Tools menu). Same lazy-construction pattern as openPureSignalDialog;
    // both dialogs are single-instance for the lifetime of MainWindow.
    void openSpotHub();
    /// Tools > Contest Logbook: das Contestprogramm im eigenen Fenster,
    /// mit Longpaths rigctld-Server als CAT-Quelle.
    void openContestFenster();
    /// Ein Panel des Contestprogramms als eigenes Fenster holen (2026-11-04,
    /// Betreiber: "jedes panel ... eigens hinzuzufuegen"). Laedt das
    /// Logbuch bei Bedarf verborgen. Siehe ContestFenster.h.
    void contestPanelAlsFenster(const QString& id);
    /// Alle Schloesser auf einmal (2026-11-04): schwebende Fenster,
    /// Instrumenten-Container, Panels des Contestprogramms. Liefert, wie
    /// viele umgestellt wurden. Containers > Lock every window in place.
    int sperreAlleFenster(bool an);
    // Frequency memories (Thetis MemoryForm + the front-panel quick memory):
    // Tools > Memories..., Tools > Memory Quick Save / Quick Restore.
    void openMemories();
    void openFreeDVReporter();
    // File > Settings Backups... -- the backups half of Thetis's Database
    // Manager (SettingsBackupDialog). Same lazy, single-instance pattern.
    void openSettingsBackups();
    /// Task B4 (bottom-banner + pan-menu epic): +PAN icon click handler.
    /// Also the View > Pan Layout… (Ctrl+L) menu action's target. Gated on
    /// m_radioModel->isConnected(); opens PanLayoutDialog sized to
    /// qMin(BoardCapabilities::maxSlices, BoardCapabilities::userDdcCount)
    /// -- opening a new pan always claims its own DDC, so that ceiling
    /// (not the raw slice count) is what bounds how many pans a board can
    /// actually fill -- and, on accept, applies the selected layout via
    /// applyPanLayout(). Replaces the Phase 3F Sub-Epic D Task 10
    /// showPanMenu() context menu -- its add-slice-on-active-pan and
    /// float-active-pan actions move to each pan's own right-click menu in
    /// Task B5, since both routed through activePanId() and a control
    /// drawn on a pan should target that pan.
    void showPanLayoutDialog();

    /// Apply a pan layout template and reconcile the slices against it.
    ///
    /// Codex review round 3, PR #293. There were three places that applied a
    /// layout: session restore, the View menu, and the +PAN affordance
    /// (a dropdown at the time; a drawn icon since Task B4). Each had its
    /// own copy of the pan-count table, the id list and an add-only slice
    /// loop. Round 2's fix for slices orphaned by a shrinking layout went
    /// into the View-menu copy only, so the defect stayed live through
    /// +PAN, which is the one operators actually use.
    ///
    /// One function now owns the whole sequence, so a later fix cannot land
    /// in one path and miss two. The part that carries real logic,
    /// RadioModel::rehomeSlicesToPans, is tested there; MainWindow is not
    /// constructible in the harness, so what is left here is plumbing.
    void applyPanLayout(const QString& layoutId);

    /// Give every pan that has no slice one, so it gets a VFO flag, an RX
    /// applet entry and a stream. Called from applyPanLayout and again at
    /// connect, because the startup layout restore cannot do it: no radio,
    /// no stream pool. See the definition for the bench defect where a
    /// persisted multi-pan layout came back with a permanently dead pane.
    void populateEmptyPans();

    // Phase 3M-4 bench-fix: gate m_psaIndicator visibility on
    // caps.hasPureSignal && PureSignal::isAutoCalEnabled.  Called from
    // PureSignal::autoCalEnabledChanged + RadioModel::pureSignalCoordinator-
    // Ready + onConnectionStateChanged.
    void updatePsaIndicatorVisibility();
    // Phase 3Q Sub-PR-4 D.2: right-click context menu on the TitleBar
    // ConnectionSegment. Items: Disconnect / Connect-to-other / Diagnostics /
    // Copy IP / Copy MAC. "Reconnect" omitted — no RadioModel::reconnect() API.
    void showSegmentContextMenu(const QPoint& globalPos);
    // Phase 3Q Sub-PR-7 G.1: right-click context menu on the StationBlock.
    // Items: Disconnect / Edit radio… / Forget radio.
    void showStationContextMenu(const QPoint& globalPos);
    // Phase 23: update m_tciIndicator bottom label + tooltip for the 4 states
    // (Off / On / On·N / On·N ▸TX).  Connected to TciServer signals.
    void updateTciIndicator();
    // Phase 23: open Setup dialog at "TCI Server" page.  Wired to
    // tciAction triggered + m_tciIndicator click + TciApplet::setupRequested.
    void openTciSetupPage();

    // Phase 3P-II Phase 4 Task 90: generic navigation entry point for applet
    // right-click menus. Maps a pageKey string to a SetupDialog tree label:
    //   "pgxlAdvanced"  -> "PGXL Advanced"
    //   "tgxlAdvanced"  -> "TGXL Advanced"
    //   "pgxlInterlock" -> "PGXL Interlock"
    //   "peripherals"   -> "Peripherals"
    // Unknown keys are logged and ignored (current page unchanged).
    void openSetup(const QString& pageKey);

    // Phase 3P-II Phase 4 Task 97: soft-alert toast when peak forward power
    // exceeds the PGXL cap.  Connected to RadioModel::ampMetersChanged.
    // De-bounced: only one toast per exceedance event (re-arms when fwd drops
    // back below cap).  Uses QStatusBar::showMessage (5-second duration).
    // Guards: PGXL_PowerCapEnabled == "True" and fwd > PGXL_PowerCapW.
    void onAmpMetersForPowerCap(float fwd, float swr);

    // Phase 3P-II review fix C2: show TX interlock warning/denial on the
    // status bar so bench rows 28/29/31 are visible to the operator.
    // Connected to TxInterlockPolicy::warned / denied in buildUI().
    void onTxInterlockWarning(const QString& reason);
    void onTxInterlockDenial(const QString& reason);

    /// Phase 3F Sub-Epic I Task 8: fan one stream's FFT frame out to every
    /// pan subscribed to it. Connected to every pooled FFTEngine's
    /// fftReadyLinear; the streamIndex argument is the engine's receiver id.
    void dispatchFftFrameToPans(int streamIndex,
                                const QVector<float>& binsLinear,
                                double windowEnb,
                                double dbmOffset);

private:
    void buildUI();
    void buildMenuBar();
    // Antenna rotator dial (Tools > Rotor...). Modeless singleton,
    // lazy-constructed so it costs nothing until opened.
    void openRotorDial();
    // The logbook window, reachable from the menu without the dock
    // having to be visible. Both go through the one panel, so there is
    // never a second window over the same file.
    void openLogbookWindow();
    // Start: das offene Logbuch vor Panadapter und Rotor/Log (2026-09-26).
    void raiseLogbookIfOpen();
    // Start mit offenem Logbuch: nach dem Verbinden-Fenster nach vorne.
    bool m_startedSafely{false};
    int  m_failedStartCount{0};
    bool m_raiseLogbookAfterConnectMask{false};
    /// Verbunden, solange die Maske stand: ob das CAT-Feld verdeckt ist,
    /// laesst sich erst sagen, wenn die Fenster wieder da sind.
    bool m_catRigFeldNachDerMaske{false};
    void openRotorSetup();
    void openVoiceCheck();
    void wirePuduMonitor();
    void finishPuduTake();
    // Der rohe Mitschnitt, durch Kette und Sendekanal gerechnet -- bei
    // jedem Abspielen neu, damit ein Wechsel der Bandbreite an
    // derselben Aufnahme hoerbar wird.
    std::vector<float> puduTakeProcessed() const;
    void reseatPuduTake();
    void savePuduTake();
    // Open the antenna window: a measured sweep turned into a length of
    // wire. Reads a file, touches nothing else.
    void openAntennaWindow();
    void openChannelStrip();
    class RotorLogbookPanel* ensureRotorPanel();

    /// Rotor/Log anzeigen, gleich in welcher der drei Formen (Dock,
    /// unten, eigenes Fenster) es gerade lebt. Review-Fund 2026-08-28:
    /// mehrere Aufrufer (Spot-Rechtsklick, Doppelklick, "Take Spot")
    /// zeigten bisher immer m_rotorDock direkt -- seit RotorFloating
    /// standardmaessig "True" ist, ist das dann eine leere, versteckte
    /// Huelle, waehrend das echte Panel unsichtbar in m_rotorWindow
    /// steckt. Derselbe Vorrang wie in der connectionStateChanged-
    /// Bindung: m_rotorWindow zuerst.
    void raiseRotorPanel();

    /// Rotor/Log in ein eigenes Fenster — mit Leiste, Schloss und
    /// Anfasser wie jedes andere Feld. Bis 2026-08-20 lag das Panel
    /// nackt im Splitter: keine Marke, kein ↗, kein Schloss. Es war
    /// das letzte Feld ohne.
    // Q_INVOKABLE, damit eine Pruefung sie ueber den Namen erreicht:
    // sie haengen an Menue und Kopfleiste, nicht an einer oeffentlichen
    // Schnittstelle, und ein Test soll den ECHTEN Weg gehen duerfen,
    // ohne dass die Klasse dafuer aufgemacht wird.
    Q_INVOKABLE void detachRotorPanel();
    Q_INVOKABLE void dockRotorPanel();

    // Applet-Leiste neben (false) oder unter (true) den Panadapter legen.
    // Wechselt die Richtung des Hauptsplitters, damit der Griff zwischen
    // beiden die HOEHE des Panadapters verstellt statt seiner Breite.
    // Persistiert unter „AppletPanelBelow". Begruendung und Zielbild
    // stehen in MainWindow.cpp beim Menueeintrag.
    void setAppletPanelBelow(bool below);

    // Rotor/Log unter den Panadapter legen statt an den Rand andocken.
    // Gibt der unteren Flaeche des aeusseren Splitters Inhalt — ohne
    // Inhalt waere der senkrechte Griff da, aber sinnlos.
    // Q_INVOKABLE aus demselben Grund wie detachRotorPanel:
    // eine Pruefung soll den echten Weg gehen duerfen.
    Q_INVOKABLE void setRotorPanelBelow(bool below);

    // Betreiber 2026-09-02: "Vertikal ist da noch eine Linie inkl.
    // blauer Punkt" ueber der CAT-Anzeige, auch wenn m_belowPane leer
    // und verborgen ist. QSplitter versteckt seinen Griff NICHT von
    // selbst, nur weil ein Kind hide() bekommt — genau die Falle, vor
    // der der Kommentar oben bei setRotorPanelBelow() schon warnte.
    // Nach jedem show()/hide() von m_belowPane aufrufen.
    void syncOuterSplitterHandle();
    // QRZ XML client, created on first use. Username from AppSettings,
    // password from the platform credential store.
    void ensureQrzClient();
    void openQrzCredentialsDialog();
    // Logbook: one appended ADIF file, plus the QRZ logbook uploader
    // (a different service and credential from the XML lookup).
    void ensureQrzUploader();
    // Satelliten in Sicht: ein Dienst fuer Stempel (Logbuch-Panel) und
    // Kartenschicht (Logbuchfenster). Beim ersten Bedarf angelegt; er
    // liest die TLE-Datei und holt bei Bedarf einmal am Tag von CelesTrak.
    void ensureSatellites();
    // Cloudlog/Wavelog and the local-logger socket. Separate from the
    // QRZ pair because they are separate services with separate
    // credentials, and one "logging settings" blob would invite mixing
    // them up the way the QRZ password and API key already do.
    void ensureExtraUploaders();
    void openLoggingServicesDialog();
    // Every configured destination, for the logbook window's Upload
    // menu. Ownership stays here.
    QVector<QsoUploader*> qsoUploaders();
    void buildStatusBar();
    void applyDarkTheme();
    void openConnectionPanelOnLaunch();
    /// Was im Kombi-Feld lag, beim Start wieder hineinlegen.
    void holeKombiInhaltZurueck(class KombiApplet* feld, const QStringList& vorgabe = {});
    void wireSliceToSpectrum();

    /// Re-bind RttyDecoderApplet and the RADE/RttyDecoder availability gate
    /// to `slice` (nullptr included). Both used to be wired ONCE, inside
    /// wireSliceToSpectrum(), to whichever slice existed at slice-0-added
    /// time -- unlike RxApplet and CommandBar, which already re-bind on
    /// every RadioModel::activeSliceChanged. Bench-found 2026-09-07 against
    /// a real ANAN 10e, same root cause CommandBar had (db0c50cf, same
    /// day): after the active slice's IDENTITY changes (a band click that
    /// restores a per-band mode, a slice removed and a new one taking its
    /// index), RxApplet's own Mark/Shift line correctly followed the real
    /// mode, but RttyDecoderApplet stayed visible with the OLD slice's
    /// values -- it never got the equivalent re-bind. Called from
    /// wireSliceToSpectrum() (first slice) and from the same
    /// activeSliceChanged handler that already re-binds CommandBar.
    void rebindRttyRadeAvailability(class SliceModel* slice);

    /// Re-bind TunerApplet::setBand to `slice`'s bandChanged (nullptr
    /// included) and immediately seed it with that slice's current band.
    /// Same fix shape as rebindRttyRadeAvailability() just above, for the
    /// same underlying bug class: wireSliceToSpectrum() used to wire this
    /// ONCE, to whichever slice existed at slice-0-added time, so
    /// TunerApplet's Save/Recall/Clear context-menu actions kept reading/
    /// writing the wrong (antenna, band) slot after the active slice
    /// changed identity. Called from wireSliceToSpectrum() (first slice)
    /// and from the same activeSliceChanged handler that re-binds
    /// CommandBar and RttyDecoderApplet.
    void rebindTunerAppletBand(class SliceModel* slice);

    /// Stream 0's engine. Back-compat accessor for call sites that still
    /// address "the" FFT engine (display settings, Max Bin, auto-zoom).
    FFTEngine* primaryFftEngine() const { return m_fftEngines.value(0, nullptr); }

    /// FFT-Groesse des Panadapters je Geraet: die gespeicherte dieses
    /// Geraets, sonst die globale (DisplayFftSize). Beim Verbinden.
    void applyPerRadioFftSize(const QString& mac);

    /// Build and register one FFTEngine for `streamIndex`, configured as
    /// the old single-engine path was, moved onto the shared FFT thread.
    /// Returns the existing engine if one is already registered.
    ///
    /// Called on demand rather than once per pool slot: engines subscribe
    /// to the shared RadioModel::rawIqDataForStream and filter by index, so
    /// an engine for a stream that is never claimed would still take (and
    /// discard) a queued event for every packet of every OTHER stream.
    /// Building one only when the allocator actually claims the DDC keeps
    /// that cost at zero for the single-stream case. Sizing off
    /// streamPoolSize() at construction would not work anyway: the pool is
    /// unsized until RadioModel::configureStreamPool runs at connect.
    FFTEngine* createFftEngineForStream(int streamIndex);

    /// Haengt die dBm-Bins dieser Engine als TCI-Empfaenger `rx` an
    /// RadioModel::meldeSpektrum — den geraeteunabhaengigen Weg, an dem der
    /// TCI-Spektrumstrom seit 2026-11-03 haengt (vorher an Engine 0 allein,
    /// und die rechnet bei Flex und SunSDR nichts).
    void verbindeSpektrumMeldung(FFTEngine* engine, int rx);

    /// Push the stream's cached DDC centre + sample rate onto one pan's
    /// SpectrumWidget so visibleBinRange maps its bins against the right
    /// window. No-op for a stream we have never seen a centre for.
    void applyStreamWindowToPan(const QString& panId, int streamIndex);

    /// Re-derive the entire pan-to-stream topology from the current slice
    /// set. Cheap (one pass) and called on slice add / remove / migration
    /// / pan change. Chosen over incremental edits because a pan can host
    /// several slices, so no single change maps onto one subscription.
    void rebuildFftRouting();

    /// Phase 3F: light the WIDE pill on every pan fed by a bypassed RX
    /// preselector chain, and clear it on the rest. One
    /// RadioModel::panBypassState query per pan; the decision (and the
    /// operator-facing reason) lives there, not here.
    void refreshPanWideBadges();

    /// Phase 3F: arm the status-overlay triggers that are not per-slice --
    /// each pan's own activeSliceChanged. Idempotent (Qt::UniqueConnection),
    /// so it can be re-run whenever the pan set changes; a layout switch
    /// destroys and rebuilds applets, and a pan created after startup would
    /// otherwise never be wired.
    void wirePanStatusOverlayTriggers();

    /// Phase 3F: the ADC chain feeding `panId`, or -1 when it resolves to
    /// none.
    ///
    /// Deliberately the SAME resolution refreshPanStatusOverlays paints with
    /// -- the pan's own activeSliceIndex through
    /// RadioModel::sliceChainIndex -- so the dialog a badge opens is on the
    /// chain the CH tag beside it is showing. The shipped pan-0 handler used
    /// slices.first()->chainIndex() instead, which was wrong twice over: the
    /// first slice rather than the clicked pan's, and through a SliceModel
    /// property with no production writer. Both errors return 0, so every
    /// click on every pan opened chain 0.
    int panChainIndex(const QString& panId) const;

    /// Phase 3F: connect one slice's overlay triggers. Drives the connects
    /// off PanadapterApplet::statusOverlaySliceProperties through the
    /// metaobject rather than naming signals here, so the trigger set has a
    /// single definition and cannot silently fall behind the fields
    /// updateStatusOverlay paints.
    /// Die Profilschiene an ihre Dialoge hängen (Anlegen, Umbenennen,
    /// Duplizieren, Löschen) und den Stand beim Beenden sichern.
    void wireProfileRail();

    /// Die Profilschiene an ihren Platz setzen -- waagrecht in die
    /// Kommandoleiste oder senkrecht an den linken Rand. Baut sie dabei
    /// neu auf (die Richtung steht im Aufbau des Bauteils) und haengt
    /// die Verdrahtung wieder an.
    void setzeProfilschiene(Qt::Orientation richtung);
    /// Was in den Einstellungen steht; Vorgabe ist die Leiste.
    Qt::Orientation profilschienenRichtungAusEinstellung() const;

    /// Das schwebende „+" unten rechts an seinen Platz setzen. Es hat
    /// keine Anordnung, die es mitzieht, also von Hand bei jeder
    /// Groessenaenderung.
    void positionAddWidgetButton();

    // ── Fensterteile, die keine Applets sind ─────────────────────────
    //
    // Sie stehen im Auswähler wie die Applets, hängen aber nicht in
    // m_appletsById — es gibt kein AppletWidget zu ihnen. Deshalb zwei
    // feste Kennungen und eine eigene Anwendung.
    static constexpr auto kChromeOverlayId = "ChromeSpectrumButtons";
    static constexpr auto kChromeStatusId  = "ChromeStatusBar";

    /// Ein solches Fensterteil ein- oder ausblenden. Tut nichts, wenn
    /// die Kennung keines der beiden ist.
    void applyChromeVisibility(const QString& id, bool visible);

    /// Der Zustand, den ein frisch angelegtes Profil bekommt: alles
    /// aus, was im Auswähler steht. Panadapter und Splitterstellung
    /// bleiben.
    QVariantMap blankLayoutState() const;

    // ── Applets als eigene Fenster ───────────────────────────────────
    //
    // Entscheidung des Betreibers (2026-08-16): das PROFIL besitzt die
    // Geometrie, das Fenster meldet sie nur. Beide Wege hinaus — der
    // Zug über die Schwelle und der Menüpunkt — enden in
    // detachApplet(); das Schliessen des Fensters führt zurück.

    // ── EINE Kennung, nicht zwei ─────────────────────────────────────
    //
    // Bis 2026-08-18 gab es im Baum zwei Namen für dasselbe Applet:
    //
    //   Panelkennung   „Rx" — m_appletsById, AppletVisibilityController,
    //                  der Auswähler, das Profilfeld „visible"
    //   Eigenkennung   „rx" — AppletWidget::appletId(), das Profilfeld
    //                  „order", „floatingApplets", AppletStackOrder
    //
    // Sie stimmen bei vier von neunzehn Applets überein und laufen sonst
    // auseinander: rx/Rx, TX/Tx, PHCW/PhoneCw, vax/Vax, amp/Amp,
    // tuner/Tuner, tci/Tci, pure_signal/PureSignal, RADE/Rade,
    // tci_clients/ClientChain. Jede Stelle, an der ein Schlüssel der
    // einen Sorte in einer Karte der anderen nachgeschlagen wurde, gab
    // still nullptr zurück — und damit den Fehler vom 2026-08-18: „Als
    // Fenster ablösen" nahm das RX-Panel aus der Spalte, fand die
    // Sichtbarkeit unter „rx" nicht und zeigte das Fenster nie.
    //
    // Ab jetzt ist die PANELKENNUNG der Schlüssel — überall. Sie steht
    // im Auswähler, im Menü und im Profil und ist die einzige, die ein
    // Mensch je zu Gesicht bekommt. appletId() bleibt, wo es hingehört:
    // als Eigenname des Widgets.

    /// Die Panelkennung eines Applets — der Schlüssel, unter dem es im
    /// Auswähler, im Profil und in m_floatingApplets steht.
    QString panelIdFor(const AppletWidget* applet) const;

    /// Das Applet zu einer Kennung, gleich welcher Sorte. Nimmt auch
    /// die Eigenkennung an, damit ein Profil von vor dem 2026-08-18
    /// weiter gelesen wird.
    AppletWidget* appletForKey(const QString& key) const;

    /// Eine Kennung beliebiger Sorte auf die Panelkennung bringen.
    QString canonicalAppletKey(const QString& key) const;

    /// Ein Applet aus der Spalte in ein eigenes Fenster heben.
    /// `dockIndex` ist die Stelle, an die es beim Andocken zurückkehrt.
    /// `rect` und `screenKey` kommen aus dem Profil, wenn es dazu etwas
    /// sagt; sonst leer, und dann platziert ensureOnVisibleScreen().
    /// Ein Applet aus dem Stapel nehmen und als frei bewegliche Kachel
    /// auf die Flaeche legen. Ab dann gilt fuer es alles, was fuer
    /// Container gilt: Ziehen an der Titelleiste, Groesse an der Ecke,
    /// Schloss, Rechtsklick-Menue.
    void moveAppletToCanvas(AppletWidget* applet);

    /// Die Lagen aller Kacheln in die Einstellungen schreiben.
    /// Das Menue am Zahnrad einer Kachel: welche anderen freien
    /// Fenster koennen als Reiter hierher.
    void showTileTabMenu(const QString& tileId);

    /// Einen Reiter wieder als eigene Kachel herausloesen.
    void detachTabToOwnTile(const QString& containerId, int index);

    void saveCanvasLayout();

    /// Und beim Start zurueckholen. Nach der Sichtbarkeit aufrufen.
    void restoreCanvasLayout();

    /// Jedes sichtbare Applet auf die Flaeche legen. Der Weg zu „alles
    /// x-beliebig verschiebbar" in einem Schritt.
    void moveAllAppletsToCanvas();

    /// Die Kachel aufloesen und das Applet in den Stapel zurueckgeben.
    void returnAppletFromCanvas(const QString& id);

    /// Freie Flaeche an oder aus. Steht sie an, kommt JEDES neu
    /// eingeschaltete Fenster als Kachel mit eigener Lage.
    void setFreeCanvasMode(bool on);
    bool freeCanvasMode() const { return m_freeCanvasMode; }

    /// Das Plaetze-Panel wirklich sichtbar machen -- notfalls als
    /// eigenes Fenster, wenn die Appletspalte zu ist. Siehe dort.
    void zeigeFt8SlotsFenster();

    /// Ein Durchgang der Plaetze-Automatik: entscheiden und ausfuehren.
    /// Ohne das Prueflauf-Tor -- das sitzt beim Aufrufer.
    void ft8SlotsAutomatikSchritt(bool hatInhalt, int qsos, int wartende,
                                  qint64 jetztMs);

    void detachApplet(AppletWidget* applet, int dockIndex,
                      const QRect& rect = QRect(),
                      const QString& screenKey = QString());

    /// Ein vom Bediener eben eingeschaltetes Applet nach vorne holen:
    /// sein Fenster heben, und wenn es in der Spalte unter schwebenden
    /// Fenstern laege (oder dort gar nicht zu sehen ist), als eigenes
    /// Fenster obenauf ablösen. Nur für Einschalten per Menü/Plus —
    /// nicht beim Start oder Profilwechsel, die Lagen gehören dem Profil.
    void bringAppletToFront(const QString& id);
    /// Ein Mausdruck irgendwo im Programm. Wahr heisst: geschluckt.
    bool klickHoltNachVorne(QWidget* ziel);
    /// Das CAT-Feld nach dem Verbinden zeigen. Geht es dabei neu als eigenes
    /// Fenster auf, kommt es auf einen freien Platz, wenn es einen gibt.
    void zeigeCatRigFeld();
    /// Ist das Applet in der Spalte des Hauptfensters für den Bediener
    /// verdeckt (von schwebenden Fenstern, weggerollt, Fenster zu)?
    /// `countScrolledAway`: auch weggerollt zaehlt als verdeckt.
    bool appletHiddenInColumn(QWidget* applet, bool countScrolledAway = true) const;

    // ── Seitenbereich (MainWindow_SideArea.cpp) ──────────────────────
    //
    // Betreiber 2026-09-25, Variante 2: ein schwebender Bereich mit
    // Symbolleiste am rechten Rand, eine Seite je Fenster (Rotor/Log oder
    // ein Applet). Eine Seite darin gilt als sichtbar.
    enum class SideAreaExit {
        OwnWindow,  ///< als eigenes Fenster an der Stelle des Bereichs
        Home,       ///< zurück in Spalte bzw. Dock (ausgeschaltet)
        Silent      ///< nur zurück, für die Profilanwendung
    };
    bool sideAreaHas(const QString& id) const;
    class SideAreaWindow* sideArea() const;
    /// Legt Rotor/Log ("WinRotorLog") oder ein Applet (Panelkennung) als
    /// Seite in den Bereich; nimmt es dazu aus seiner bisherigen Heimat.
    bool addToSideArea(const QString& id, bool activate);
    void removeFromSideArea(const QString& id, SideAreaExit exit);
    /// Menü „Seitenbereich rechts": an legt Rotor/Log und die sichtbaren
    /// Spalten-Applets hinein; aus macht jede Seite zum eigenen Fenster.
    void setSideAreaEnabled(bool on);

    /// Zurück in die Spalte, an die gemerkte Stelle. Räumt das Fenster
    /// ab. Tut nichts, wenn das Applet nicht abgelöst ist.
    void dockAppletBack(const QString& appletId);
    /// Ein schwebendes Panel auf einem Kombi-Feld ablegen (2026-11-01).
    void legeAufKombiFeld(const QString& appletId, const QPoint& globalPos);
    /// Was kein Applet ist, bekommt sein eigenes Werkzeugfenster
    /// zurueck -- siehe das Kombi-Feld (2026-11-01).
    void gibFremdesPanelZurueck(QWidget* panel, const QRect& dort);
    /// Rechtsklick auf ein Kombi-Feld: zeigt, was hineinkann.
    void zeigeKombiListe(class KombiApplet* feld, const QPoint& globalPos);
    void fuelleKombiListe(class QMenu& menue, class KombiApplet* feld);
    void fuelleReiterMenue(class QMenu& menue, class KombiApplet* feld, QWidget* panel);
    void uebergibKombiPanel(class KombiApplet* ziel, QWidget* panel);
    void entferneAusKombiFeld(class KombiApplet* feld, QWidget* panel);
    void holeContestInsKombiFeld(class KombiApplet* feld, const QString& id);
    QByteArray layoutprofilAlsJson(const QString& name);
    void exportiereLayoutprofil(const QString& name);
    void holeInsKombiFeld(class KombiApplet* feld, AppletWidget* applet,
                          const QString& appletId);

    /// Sichtbarkeit anwenden — auf die Spalte ODER auf das Fenster, je
    /// nachdem, wo das Applet gerade steht. Ohne diese Weiche liefe ein
    /// Ausblenden für ein abgelöstes Applet ins Leere: sein Platzhalter
    /// in der Spalte existiert nicht mehr, und setAppletVisible fände
    /// keine Hülle.
    void applyAppletVisibility(const QString& id, bool effective);

    /// Alle fuenf Schwebe-Mechanismen (Container, abgeloeste Applets,
    /// Antenne, Panadapter, Rotor/Log) verstecken und in
    /// m_floatingContainersHiddenPreConnect vormerken -- die eine
    /// gemeinsame Liste fuer "hinter die Connect-Maske". Zurueck kommt
    /// alles beim naechsten Connected ODER beim Schliessen der Maske
    /// ohne Verbindung.
    void hideFloatingWindowsBehindConnectMask();

    /// Die gemeinsame Aufzaehlung dahinter: versteckt alles, was gerade
    /// sichtbar schwebt, und merkt es in `liste` vor. Zweiter Abnehmer
    /// neben der Maske ist die Ablage ins Dock (2026-11-03).
    void versteckeSchwebendeFenster(QList<QPointer<QWidget>>& liste);
    void legeSchwebendeFensterInsDock();
    void holeSchwebendeFensterAusDemDock();

    /// Die Gegenseite: zeigt wieder, was fuer die Maske weggeraeumt wurde
    /// -- aber erst, wenn die Maske wirklich weg ist. Solange sie steht,
    /// bleibt die Liste liegen (siehe die Begruendung an der Definition:
    /// die schwebenden Fenster sind Qt::Tool und stehen auf macOS auf
    /// einer hoeheren Ebene als die Maske, ein raise() auf sie hilft
    /// nicht). Mehrfach aufrufbar; bei leerer Liste ein No-Op.
    void restoreFloatingWindowsHiddenBehindConnectMask();

    /// Ersatz fuer showFullScreen() -- siehe m_borderlessFullSize. Randlos
    /// und auf die volle Bildschirmflaeche gesetzt, KEIN eigener macOS-
    /// Space. Idempotent: ruft nur um, was noch nicht stimmt.
    void enterBorderlessFullSize();

    /// Kehrseite von enterBorderlessFullSize() -- normaler Fensterrahmen
    /// zurueck, fuer ein Profil, das ausdruecklich NICHT die volle
    /// Flaeche will. Idempotent wie das Gegenstueck.
    void exitBorderlessFullSize();

    /// Ein Eintrag der Widget-Auswahl, der ein eigenes FENSTER meint
    /// (Logbuch, Rotor, Kanalzug, Antenne …). Der Auswaehler verwaltet
    /// Absichten; dass daraus ein Fenster wird statt einer Huelle in
    /// der Spalte, entscheidet sich hier und nur hier.
    void applyWindowVisibility(const QString& id, bool on);

    /// Ein Fenster, das gerade aufgegangen ist, dorthin holen, wo
    /// gearbeitet wird -- und davor.
    ///
    /// Betreiber 2026-11-15, mit drei Schirmen: „wenn ich das logbuch
    /// aufmache mit dem plus sehe ich es nur am macbook, nicht aber auf
    /// den beiden bildschirmen! es muss immer das neu hinzugefuegte im
    /// vordergrund sein!!!!"
    ///
    /// Zwei Dinge, eine Stelle: `aufDenArbeitsschirm()` holt es auf den
    /// Schirm des Hauptfensters, falls es auf einem anderen lag (und
    /// laesst es stehen, wenn es schon dort liegt), danach raise() und
    /// activateWindow(). Gedacht fuer die Fenster, die auf Zuruf
    /// aufgehen -- Logbuch, Kanalzug, Antenne, Spot-Zentrale, Rotor/Log
    /// --, NICHT fuer die schwebenden Applets: deren Schirm steht im
    /// Profil, und den darf diese Regel nicht uebergehen.
    void holeAufDenArbeitsschirm(QWidget* fenster);

    /// Der Betreiber hat ein solches Fenster selbst geschlossen (roter
    /// Knopf): der Haken in der Auswahl geht aus und das Profil merkt es,
    /// sonst reisst der naechste Start es wieder auf. Das Schliessen beim
    /// Beenden zaehlt nicht -- was da offen war, soll wieder aufgehen.
    void noteWindowClosedByOperator(const QString& id);

    /// Bildschirmkennung für die Geometrie im Profil.
    /// QScreen::serialNumber() zuerst — nur sie hält zwei baugleiche
    /// Monitore auseinander; name() tut das nicht. Wo die Plattform
    /// keine liefert, name() als Rückfall, und wo auch der fehlt, leer:
    /// dann entscheidet allein das Rechteck, und ensureOnVisibleScreen
    /// fängt den Fall „Monitor weg" ohnehin ab.
    static QString screenKeyFor(const QWidget* w);

    void wireSliceStatusOverlayTriggers(SliceModel* slice);


    /// Phase 3F Sub-Epic D Task 16: clean disconnect-before-removal for pans
    /// (AetherSDR issue #242 pattern - avoids lambda crashes during teardown).
    /// Caller should immediately follow with m_panStack->removePanadapter(panId).
    void disconnectPanadapter(const QString& panId);

    // Issue #206 — persist the main window's position, size, and
    // maximized/fullscreen state across launches. Stored in AppSettings
    // under MainWindowGeometry / MainWindowState (base64 of Qt's native
    // QByteArray blobs). restoreMainWindowGeometry() returns true if a
    // valid saved geometry was applied, false otherwise (first launch
    // or corrupted blob); buildUI() uses the return to decide whether
    // to keep the 1280×800 default. Multi-screen safety clamp: if the
    // restored geometry sits entirely outside every connected screen
    // (monitor disconnected since last save), fall back to centering
    // on the primary screen at the default size.
    void saveMainWindowGeometry();
    bool restoreMainWindowGeometry();

    // Task A8 fix round 1 shipped reapplyHardwarePresenceGates(), a second
    // pass ANDing a live hardware query on top of m_chromeBar's fold
    // decision after every relayout(). Fix round 2 removed it: it only
    // ran from resize/tick call sites, so a signal that fired BETWEEN
    // relayout() calls (plug in a TGXL while folded past rung 2) bypassed
    // it entirely. ChromeBarController::setItemAvailable (see its own doc
    // comment) replaces it -- the presence/DSP-active facts are now
    // reported straight from the signal that changes them
    // (TunerModel::presenceChanged, the rxFilterChainCount capability
    // gate, updatePsaIndicatorVisibility, RxDashboard::badgeAvailabilityChanged),
    // each followed by a relayout() call at that same call site, so there
    // is no window where the fact and the controller's decision disagree.

    // CPU usage helpers — return instantaneous percent since the last call.
    // First call after a toggle returns 0 (delta-state reset). The timer
    // applies Thetis-style smoothing on top. Both helpers branch internally
    // on Q_OS_MAC / Q_OS_LINUX / Q_OS_WIN; declared on every platform so
    // the timer wiring in buildStatusBar() doesn't need a platform guard.
    //   process: getrusage(RUSAGE_SELF) on POSIX, GetProcessTimes on Windows
    //   system : host_processor_info on macOS, /proc/stat on Linux,
    //            GetSystemTimes on Windows
    double readProcessCpuPercent();
    double readSystemCpuPercent();
    // Right-click menu on m_systemTile — System / App radio choice.
    void onCpuMenuRequested(const QPoint& localPos);

    // Phase 3M-3a-ii Batch 6 (Task 3): one-shot wiring helper called from
    // every SetupDialog construction site.  Connects the dialog's
    // cfcDialogRequested signal to TxApplet::requestOpenCfcDialog so the
    // [Configure CFC bands…] button on Setup → DSP → CFC reuses the same
    // modeless dialog instance owned by the TxApplet.
    void wireSetupDialog(class SetupDialog* dialog);

    // Help > Check for Updates... und die stille Startpruefung
    // (UpdateDialog.h). Betreiber 2026-09-21: "ein Klick, Installation
    // automatisch".
    void openUpdateDialog();
    void scheduleStartupUpdateCheck();

    // KiwiSDR — siehe MainWindow_KiwiSdr.cpp.
    void wireKiwiSdr();
    void refreshKiwiSdrAppletReceivers();
    void addKiwiSdrReceiver(const QString& name, const QString& endpoint);
    void syncKiwiSdrTransmitMute();
    // Stufe 3b (2026-09-21): der Kiwi folgt der zugeordneten Scheibe --
    // Frequenz, Betriebsart, Filter, Panadapter. Verdrahtet je Zuordnung
    // (nicht je Scheibe wie bei Aether, wo jede Scheibe einen Kiwi haben
    // kann); geloest, sobald die Zuordnung faellt.
    void rewireKiwiSdrTrackingForSlice(int sliceId, const QString& profileId);
    void updateKiwiSdrTrackingForSlice(class SliceModel* slice);
    // Sicherheitsschranke (2026-08-24, uebertragen aus der SunSDR-
    // Durchsicht -- siehe docs/architecture/2026-08-24-sunsdr-tci-
    // client-design.md): die EINE Stelle, die "gibt es diese Scheibe
    // und darf sie gerade von einem Kiwi gefuettert werden" beantwortet.
    // Liefert nullptr, wenn die Scheibe nicht existiert ODER eine echte
    // DDC-Bindung hat (streamIndex() >= 0 -- ein ECHTES, moeglicherweise
    // sendefaehiges Funkgeraet). Jeder Verbraucher (Ton, Panadapter) geht
    // darueber, statt sliceById() von Hand zu wiederholen.
    class SliceModel* kiwiControllableSlice(int sliceId) const;
    // Gibt EINE Kiwi-Scheiben-Zuordnung vollstaendig frei -- Ton UND die
    // Manager-Zuordnung selbst -- wenn die Scheibe geloescht wird oder
    // spaeter von einem echten Funkgeraet uebernommen wird. Anders als
    // SunSDR (ein Geraet, eine Zielscheibe) kann ein KiwiSDR mehrere
    // Scheiben gleichzeitig speisen; darum nimmt diese Funktion die
    // konkrete sliceId/profileId statt eines einzigen gespeicherten
    // Ziels.
    void releaseKiwiSdrSlice(int sliceId, const QString& profileId,
                              const QString& reason);
    void setAsrEnabled(bool on);
    // Neue Sprache aus dem ⚙ der Mitschrift: laeuft sie, mit der neuen
    // Sprache neu aufsetzen (MainWindow_Asr.cpp, 2026-11-11).
    void uebernehmeAsrSprache();

    // SunSDR — siehe MainWindow_SunSdr.cpp.
    void wireSunSdr();
    void connectSunSdr(const QString& endpoint);
    void disconnectSunSdr();
    // Setzt m_streamWindows fuer den reservierten SunSDR-Pseudostrom und
    // schiebt es zum Pan durch (applyStreamWindowToPan) -- aufgerufen,
    // wann immer TciClient eine neue Rate oder Mittenfrequenz meldet.
    void applySunSdrStreamWindow();
    // Traegt die Router-Zuordnung erneut ein. Selbstheilend gegen
    // rebuildFftRouting(), das bei jedem Aufruf JEDEN Pan loescht und nur
    // aus echten, gebundenen Scheiben neu aufbaut (siehe Umsetzung).
    void reassertSunSdrRouterMapping();

    // Phase 3O Sub-Phase 11 Task 11b — first-launch / startup rescan
    // hook. Scheduled via QTimer::singleShot(0, ...) from the
    // constructor so it runs after the event loop starts and the UI
    // is fully built. Diffs detected cables against the persisted
    // audio/LastDetectedCables fingerprint and pops the
    // VaxFirstRunDialog in the appropriate scenario.
    void checkVaxFirstRun();

    RadioModel* m_radioModel{nullptr};
    ConnectionPanel* m_connectionPanel{nullptr};
    SupportDialog* m_supportDialog{nullptr};

    // Review-Fund 2026-08-28: seit der P2-Stillewaechter unbegrenzt
    // wiederholt (Betreiberwunsch, kein Versuchslimit), durchlaeuft eine
    // anhaltende Funkstille LinkLost->Connecting->LinkLost... immer wieder
    // -- ohne diese Wachen haette jeder Durchlauf den Verbinden-Dialog
    // erneut nach vorn geholt (Fokusraub) und/oder einen weiteren
    // 14-Sekunden-Hinweis gestapelt. Zwei getrennte Flaggen, weil beide
    // Ereignisse auf unterschiedlichen Signalen mit unterschiedlichem
    // Timing haengen (siehe onConnectionStateChanged vs. der
    // connectAttemptFailed-Anschluss) -- eine gemeinsame Flagge haette
    // beim ersten Ausfall eines der beiden unterdrueckt, je nachdem
    // welches Signal zuerst kam. Einmal je Ausfall reicht fuer jedes;
    // beide zurueck auf false, sobald Connected wieder eintrifft.
    bool m_connectionPanelAutoOpenedThisEpisode{false};
    bool m_connectFailedToastShownThisEpisode{false};

    // Phase 3M-4 Task 8: PsForm modeless dialog (Tools > PureSignal...).
    // Lazy-constructed on first openPureSignalDialog() call; lives for the
    // lifetime of MainWindow.  Hidden on close, never destroyed.
    PsForm* m_psForm{nullptr};
    QAction* m_actPureSignal{nullptr};

    // Phase 3J-2 H1: Tools > Spot Hub... and Tools > FreeDV Reporter...
    // modeless singleton dialogs. Lazy-constructed on first
    // openSpotHub() / openFreeDVReporter() call; lives for the lifetime
    // of MainWindow. QPointer guards against the QDialog being deleted
    // out from under MainWindow (Qt::WA_DeleteOnClose is left at the
    // default false in the dialogs themselves so close-then-reopen
    // preserves geometry / table state). Both members are accessed by
    // the H1 test seam below.
    QPointer<SpotHubDialog>        m_spotHubDialog;
    QPointer<class MemoryDialog>   m_memoryDialog;
    // m_voiceCheckDialog is gone (2026-08-11): the voice check is an
    // embedded tab of StripWindow now; openVoiceCheck() routes there.
    QPointer<StripWindow>          m_stripWindow;

    // The AetherSDR record-then-listen monitor, structure copied 1:1
    // (2026-08-11): MainWindow owns it, the strip window hosts its two
    // buttons, muteRxRequested gates live RX, recordingStopped
    // auto-plays. Created alongside the strip window; the feed tap is
    // (re)wired per recording because the TX worker is rebuilt on every
    // connect.
    ClientPuduMonitor*      m_finalMonitor{nullptr};
    QMetaObject::Connection m_puduFeedTap;   // retired feed path; kept for teardown safety
    // Device-paced capture for the monitor (2026-08-11): QAudioSource
    // on the default input, int16 mono 48 kHz, accumulated on the UI
    // thread. The radio never paces this — see wirePuduMonitor.
    QAudioSource* m_puduCapture{nullptr};
    QIODevice*    m_puduCaptureIo{nullptr};
    QByteArray    m_puduRawTake;
    QPointer<FreeDVReporterDialog> m_freeDVReporterDialog;
    QPointer<class SettingsBackupDialog> m_settingsBackupDialog;
    // The "Backup on shut-down" copy (Thetis DBMan.Shutdown()) is taken
    // right after the final AppSettings::save(); two shutdown paths save
    // (closeEvent and aboutToQuit), one copy is enough.
    void takeShutdownBackupIfWanted();
    bool m_shutdownBackupDone{false};

    // Status bar widgets (double-height AetherSDR design, 46px)
    //
    // Design §4.1: the left-section model+firmware pair (formerly
    // m_radioModelLabel / m_radioFwLabel, plus the m_connStatusLabel alias
    // to the model label) is retired. Both had no click affordance and sat
    // in the banner's unprotected left section, so their width changes were
    // what shoved neighbours. Radio identity now renders once, on
    // StationBlock's second row via setHardwareLine() (Task A4), driven
    // from onConnectionStateChanged().
    StationBlock* m_stationBlock{nullptr};    // Sub-PR-7 G.1: radio-name anchor
    QLabel* m_tnfLabel{nullptr};
    /// Die vier Sendeschalter der unteren Leiste (2026-08-18).
    class TxSwitch* m_txMoxSwitch{nullptr};
    class TxSwitch* m_txVoxSwitch{nullptr};
    class TxSwitch* m_txTuneSwitch{nullptr};
    class TxSwitch* m_txPsSwitch{nullptr};

    // Wisdom generation dialog (shown on first run)
    QProgressDialog* m_wisdomDialog{nullptr};

    // Spectrum display
    //
    // Phase 3F Sub-Epic D Task 12: the single m_spectrumWidget has been
    // removed and replaced by m_panStack (PanadapterStack), which owns
    // 1..N PanadapterApplet instances, each containing its own
    // SpectrumWidget. Existing call sites that still need a single
    // SpectrumWidget* go through activeSpectrumWidget() below, which
    // resolves to m_panStack->panadapter(activePanId())->spectrumWidget().
    // The accessor returns nullptr during early init before m_panStack
    // is constructed, so callers must null-guard.
    PanadapterStack*    m_panStack{nullptr};

    // Task B4: +PAN status-bar icon (AetherSDR MainWindow.cpp:4368-4396
    // [@c6481cb]). Dimmed + retooltipped by updateAddPanButtonState(),
    // called from buildStatusBar() at construction and again on every
    // connectionStateChanged so the affordance reads unavailable before
    // the click rather than no-opping after it (design §8.2).
    QLabel*  m_addPanButton{nullptr};
    void     updateAddPanButtonState();

    // Phase 3F Sub-Epic I Task 8: one FFTEngine per DDC stream, keyed by
    // stream index. Before this there was a single FFTEngine(0) wired at
    // construction to activeSpectrumWidget(), which resolves to pan 0
    // permanently, so no secondary pan ever received a frame.
    //
    // One engine per STREAM, not per slice: the panadapter belongs to the
    // DDC (ChannelMaster `_rcvr.run_pan`, cmaster.h:79 [v2.10.3.15]), so
    // slices sharing a DDC share its spectrum and appear as separate flags
    // on it.
    //
    // All engines share m_fftThread. If a 5-stream 1536 kHz bench shows
    // the thread saturating, splitting to one thread per engine is a
    // follow-up needing maintainer sign-off (thread architecture).
    QMap<int, FFTEngine*> m_fftEngines;

    /// One NoiseFloorTracker per stream, fed by that stream's FFT engine.
    /// Auto AGC-T needs the noise floor of the band a slice is actually on;
    /// a single tracker fed from stream 0 would mis-set every other slice.
    QMap<int, class NoiseFloorTracker*> m_streamNoiseFloors;
    QThread*            m_fftThread{nullptr};

    /// Last centre + sample rate RadioModel published for each stream, kept
    /// so a pan that subscribes AFTER the stream was centred still learns
    /// where its bins sit. RadioModel::bindSliceToStream emits
    /// streamCentreChanged BEFORE SliceModel::streamIndex is updated and
    /// before sliceAdded (plan discovery item 7), so at emit time the router
    /// does not yet know which pan shows the stream and the direct push in
    /// the streamCentreChanged handler reaches nobody. rebuildFftRouting
    /// replays the cached value onto each pan as it (re)subscribes.
    /// Without it SpectrumWidget::visibleBinRange maps a second pan's bins
    /// against its ctor-default 14.225 MHz / 768 kHz window.
    struct StreamWindow {
        double centreHz{0.0};
        int    sampleRateHz{0};
    };
    QHash<int, StreamWindow> m_streamWindows;
    ClarityController*  m_clarityController{nullptr};
    class StepAttenuatorController* m_stepAttController{nullptr};
    /// Phase 3F Sub-Epic D Task 11: CH 1 stacked-indicator widget in the
    /// bottom status bar. Shown only on 2-ADC SKUs. Registered with
    /// m_chromeBar at rung 4 (design §6) so it folds under width pressure;
    /// its rxFilterChainCount>=2 capability gate is reported via
    /// ChromeBarController::setItemAvailable from the currentRadioChanged
    /// handler, not a direct setVisible call.
    QWidget*            m_chain1IndicatorWidget{nullptr};
    /// CH 0's stacked-indicator widget, captured the same way as CH 1 so
    /// it can be registered with m_chromeBar (chain0 shares rung 4).
    /// Always shown; single-ADC and multi-ADC SKUs alike have a chain 0.
    QWidget*            m_chain0IndicatorWidget{nullptr};

    // Right-side strip wrapper widget — the inner QWidget hosting the
    // QHBoxLayout that buildStatusBar() populates. Stored as a member so
    // resizeEvent can read its available width for m_chromeBar->relayout().
    QWidget* m_chromeBarWidget{nullptr};

    // Single layout authority for the banner (design §5). Replaces both
    // the old right-strip drop-priority ladder (30 px deadband
    // hysteresis) and RxDashboard's internal 3-stage ladder. One rung
    // table, one relayout() call per resize, no re-measure mid-decision.
    // Item-to-rung wiring lives in registerChromeBarItems() (ChromeBarItems.h)
    // rather than inline here, so it is testable without constructing
    // MainWindow — see tests/tst_chrome_bar_items.cpp.
    ChromeBarController* m_chromeBar{nullptr};
    // Merged PA telemetry + CPU tile (design §4.3). Replaces the old
    // m_paStackWidget / m_paVoltLabel / m_paTempLabel / m_cpuMetric quartet.
    SystemTile* m_systemTile{nullptr};
    QLabel*     m_systemTileSep{nullptr};
    // Rung-10 group: band-stack dots + TNF/CWX/DVK/FDX, wrapped in one
    // widget so the ladder folds them together instead of dribbling them
    // out one label at a time (design §6, "last resort").
    QWidget*    m_placeholderGroup{nullptr};
    // Band-stack dots. Head of the bar positionally, ahead of +PAN, but
    // registered at rung 10 so they fold with the other stubs.
    QWidget*    m_bandStackLabel{nullptr};
    QLabel*     m_placeholderSep{nullptr};

    // Right-side strip items — captured so they can be registered with
    // m_chromeBar. Each non-separator widget has a paired separator
    // pointer so the pair hides + shows together (no dangling "··" runs).
    QWidget* m_catIndicator{nullptr};
    QLabel*  m_catSep{nullptr};
    QWidget* m_tciIndicator{nullptr};
    QLabel*  m_tciSep{nullptr};

    // OverflowChip — "…" pill that surfaces drop-list contents via its
    // hover tooltip. Hidden when the drop list is empty. Now driven by
    // m_chromeBar's foldStateChanged signal rather than a direct call
    // from the old right-strip drop-priority pass.
    OverflowChip* m_overflowChip{nullptr};

    // (Earlier revisions had a "voltage stack" wrapper holding PSU above
    //  PA. The PSU widget was source-first audited against Thetis 2026-04-30
    //  and removed — Thetis never displays AIN6/supply_volts. The PA volt
    //  label below is the sole supply indicator; it lives directly in the
    //  hbox now with no wrapper.)
    // ADC overload alarm: "ADCx / OVERLOAD" badge living in its own
    // reserved slot inside m_safetyGroup (design §4.5), between the PA
    // and TX slots. Dimmed when no ADC is in overload; setVariant()
    // flips between Warn (yellow) / Tx (red) per Thetis severity rules
    // (ucInfoBar.cs:928 [@501e3f5]).
    AdcOverloadBadge* m_adcOvlBadge{nullptr};
    // 2-second auto-hide timer for the ADC-overload alarm. Mirrors
    // Thetis ucInfoBar._warningTimer: restarts on each overload event,
    // dims the badge when elapsed — independent of the level-decay
    // state tracked in StepAttenuatorController. Source:
    // ucInfoBar.cs:927-932 [@501e3f5]
    QTimer* m_adcOvlHideTimer{nullptr};

    // Re-entrancy guard: prevents centerChanged from firing a second
    // forceHardwareFrequency while frequencyChanged is already retuning the DDC
    bool m_handlingBandJump{false};

    // Set true at the top of closeEvent (and aboutToQuit). Gates the
    // "auto-open ConnectionPanel on Disconnect" slot — without this,
    // closeEvent's disconnectFromRadio fires connectionStateChanged →
    // ConnectionPanel ctor → startDiscovery, which clears the discovery
    // stop flag and runs a fresh ~5 s NIC walk on the main thread mid-
    // close. Symptom: ⌘Q beach-balls for the full SafeDefault scan time.
    // ── Das sichtbare Raster (2026-10-31) ────────────────────────────
    //
    // Betreiber: „man sollte den raster auch ein und ausblenden
    // können." Ein Blatt ueber dem Hauptfenster, das die Linien
    // zeichnet, an denen die schwebenden Fenster einrasten. Siehe
    // `RasterOverlay`. Wird erst beim ersten Einschalten angelegt.
    class RasterOverlay* m_rasterOverlay{nullptr};
    QAction*             m_rasterAction{nullptr};
    void setRasterVisible(bool on);

    bool m_shuttingDown{false};

    // Betreiber 2026-09-01: echtes macOS-Vollbild (showFullScreen(), ein
    // eigener "Space") entschieden gegen ein randloses, maximiertes
    // Fenster ausgetauscht -- schwebende Werkzeugfenster (Panadapter,
    // S-Meter, Bandwidth Filter, Rotor/Log...) blieben beim Wechsel in
    // den eigenen Vollbild-Space zuverlaessig auf dem normalen
    // Schreibtisch-Space zurueck (NSWindowCollectionBehaviorFullScreenAuxiliary
    // nimmt ein bereits bestehendes Fenster nicht zuverlaessig mit --
    // dokumentierte Cocoa-Eigenheit, kein Longpath-Bug). Ein randloses
    // Fenster, das einfach die volle Bildschirmflaeche einnimmt, hat
    // gar keinen eigenen Space -- schwebende Kinder bleiben immer auf
    // demselben Space wie das Hauptfenster. isFullScreen() gilt dafuer
    // nicht mehr (Qt::WindowFullScreen wird nie gesetzt); dieses Flag
    // ist die neue Quelle der Wahrheit fuers Sichern/Wiederherstellen.
    bool m_borderlessFullSize{false};
    /// Erst wenn der Aufbau durch ist, zaehlt eine Fensterbewegung als
    /// ZIEHEN. Davor platziert das Programm selbst: Profilgeometrien,
    /// showMaximized(), der Wechsel des Rahmens. Ohne dieses Flag stand
    /// der Schutzrahmen-Hinweis schon beim Start da (2026-10-17).
    bool m_fensterAufbauFertig{false};
    /// Und solange der Aufbau laeuft, ist jede Fensterbewegung eine
    /// EIGENE: dieselbe Marke, die das Raster und das Herausholen aus
    /// dem Sperrstreifen setzen (`EigeneBewegung` in WindowPlacement.h).
    /// m_fensterAufbauFertig darueber schweigt nur den Schutzrahmen-
    /// Hinweis; der Mindestabstand sitzt in WindowPlacement, das
    /// MainWindow nicht kennt und nicht kennen soll -- es liest diese
    /// Marke. Gehalten vom Konstruktor bis zu derselben Stelle, an der
    /// das Flag gesetzt wird; stirbt mit dem Fenster, falls es die nie
    /// erreicht (Prueffaelle bauen mehrere nacheinander).
    std::unique_ptr<EigeneBewegung> m_aufbauBewegung;

    // View > Vollbild (2026-09-08, Betreiber: "es sollte auch im
    // gernellen fenster die möglichkeit geben, immer auf full screen
    // zu schalten"). enterBorderlessFullSize()/exitBorderlessFullSize()
    // existierten schon -- bisher nur ueber das Layoutprofil erreichbar,
    // ohne eigenen Menuepunkt oder Tastenkuerzel. checked wird von
    // beiden Methoden nachgezogen, damit ein Profilwechsel den Haken
    // nicht aus dem Takt bringt.
    QAction* m_fullScreenAction{nullptr};

    /// Der EINE Schalter fuer den schwebenden Panadapter (2026-10-15,
    /// Betreiber: „das sollte nur eine option sein, die man ein und
    /// ausblenden kann"). Ankreuzbar und beidseitig: an loest ab, aus
    /// dockt an. Vorher war es „Float active pan…" -- eine Handlung in
    /// EINE Richtung, ohne Haken, also ohne Weg zurueck und ohne
    /// Auskunft darueber, ob gerade etwas schwebt.
    QAction* m_floatPanAction{nullptr};

    /// Je Scheibe eine Flagge auf dem Panadapter, nach Scheibenkennung.
    /// Leer, solange der Schalter aus ist.
    QHash<int, class SliceFlag*> m_sliceFlags;
    bool     m_sliceFlagsOn{false};
    QAction* m_sliceFlagAction{nullptr};

    // Container infrastructure (Phase 3G-1)
    ContainerManager* m_containerManager{nullptr};
    QSplitter* m_mainSplitter{nullptr};
    // Aeusserer, SENKRECHTER Splitter um m_mainSplitter herum: erst
    // dadurch laesst sich der Panadapter auch in der Hoehe ziehen.
    // Begruendung der Schachtelung in MainWindow.cpp bei der Anlage.
    QSplitter* m_outerSplitter{nullptr};
    QWidget*   m_belowPane{nullptr};   // Flaeche unter dem Panadapter
    // Das Panel selbst, unabhaengig davon, WO es gerade haengt (Dock
    // oder untere Flaeche). ensureRotorPanel() liest diesen Zeiger.
    // QPointer (2026-09-27): beim Beenden geht das Panel mit seinem
    // Fenster/Dock zu Bruch, und ein noch eingereihtes
    // raiseLogbookIfOpen() griff danach auf den freigegebenen Speicher
    // zu (SIGSEGV in tst_window_close_stays_closed).
    QPointer<RotorLogbookPanel> m_rotorPanel;
    int m_hDelta{0};
    int m_vDelta{0};

    void createDefaultContainers();

    // Phase 3G-6 block 6: dynamic "Edit Container ▸" submenu,
    // populated from ContainerManager::allContainers() and rebuilt
    // whenever a container is added, removed, or retitled. Addresses
    // the block 4 review observation that there was no way to see
    // or manage already-created containers from the menu bar.
    QMenu* m_editContainerMenu{nullptr};
    void rebuildEditContainerSubmenu();
    void resetDefaultLayout();

    // Meter system (Phase 3G-2)
    MeterWidget* m_meterWidget{nullptr};
    MeterPoller* m_meterPoller{nullptr};
    void populateDefaultMeter();

    // Menu DSP actions
    // NR / NB submenus use exclusive QActionGroups; SNB / APF / BIN are
    // single toggle actions that mirror SliceModel state.
    QActionGroup* m_nrGroup   = nullptr;
    QActionGroup* m_nbGroup   = nullptr;
    QAction*      m_snbAction = nullptr;
    QAction*      m_apfAction = nullptr;
    QAction*      m_binAction = nullptr;
    /// DSP > TNF. Checkable; two-way bound to NotchModel::globalEnabled, so
    /// the status-bar light and this item never disagree.
    QAction*      m_tnfAction = nullptr;

    // Mode menu actions (14 modes: 12 Thetis + Longpath-native
    // RADE-U / RADE-L from Phase 3R L3; mutual exclusion via
    // QActionGroup).
    QAction*      m_modeActions[14]  = {};
    QActionGroup* m_modeActionGroup  = nullptr;

    // AGC menu action group (Task 12)
    QActionGroup* m_agcGroup = nullptr;

    // Dark theme checkable action (Task 12)

    // Radio menu state-aware actions (3Q-9; trimmed in 3Q polish — Discover Now
    // dropped because Manage Radios already exposes a ↻ Scan button).
    QAction* m_actConnect      = nullptr;
    QAction* m_actDisconnect   = nullptr;
    QAction* m_actManageRadios = nullptr;
    QAction* m_actProtocolInfo = nullptr;

    // Status bar members (Task 13 / sub-PR-8 restyle; merged into
    // SystemTile per design §4.3 in the bottom-banner cleanup).
    //
    // PA telemetry + CPU now share one two-row tile (m_systemTile) instead
    // of a separate PA stack (PA-V over PA-T) plus a standalone CPU
    // MetricLabel. Row one is PA voltage (MKII-class boards — Saturn / G2 /
    // 8000D / 7000DLE / OrionMkII / Anvelina Pro 3; Thetis-faithful via
    // convertToVolts) and/or PA temperature (HL2 today; future
    // PureSignal-feedback boards may surface a real temp source via
    // Phase 3M-4) — both share row one when a board publishes both rather
    // than evicting CPU. Row two is always CPU. The PA row is also
    // click-to-toggle °C / °F via PaTempUnitNotifier when it carries a
    // temperature reading (SystemTile::paTempClicked). Source-of-truth
    // value lives in RadioStatus::paTemperatureCelsius (always °C);
    // display formatting happens at paint time via PaTempUnitNotifier::format.
    QTimer*      m_cpuTimer{nullptr};

    // CPU usage source — System (whole machine) or App (this process).
    // Thetis equivalent: m_bShowSystemCPUUsage (console.cs:20668), default
    // true. Right-click on m_systemTile pops a menu with the two choices,
    // matching Thetis's toolStripDropDownButton_CPU. Persisted as
    // AppSettings "CpuShowSystem" ("True"/"False"). Smoothed reading is
    // updated via 0.8 * prev + 0.2 * new (matches Thetis console.cs:26224).
    bool   m_cpuShowSystem{true};
    double m_cpuSmoothedPct{0.0};
    // Process-CPU delta state (getrusage). Reset on toggle so the next
    // reading starts fresh rather than reporting accumulated cross-mode delta.
    qint64 m_cpuProcPrevWallUs{0};
    qint64 m_cpuProcPrevUserUs{0};
    qint64 m_cpuProcPrevSysUs{0};
    // System-CPU delta state (host_processor_info). Same reset rule as above.
    quint64 m_cpuSysPrevTotal{0};
    quint64 m_cpuSysPrevIdle{0};
    QVector<int> m_splitterSizesBeforeHide;  // saved splitter sizes for ☰ toggle

    // Bench-fix 2026-05-19: debounce token for within-band PGXL frequency push.
    // frequencyChanged fires on every tune-wheel click; we coalesce into one
    // setBand call per 200 ms burst. The token (timestamp in ms) lets the
    // QTimer::singleShot callback drop stale invocations.
    qint64 m_pgxlBandPushTokenMs{0};

    // Status bar safety indicators (Phase 3M-0 Task 14 / sub-PR-8 restyle;
    // reserved safety slots added per design §4.5).
    // TX Inhibit: no widget. Formerly an "INH" pill, dimmed when
    //   TxInhibitMonitor::inhibited() asserts (wired Task 17).
    // m_paStatusBadge:  PA OK (green check) / PA FAULT (red check) StatusBadge.
    // m_txStatusBadge:  TX indicator, solid red when MOX engaged.
    // All four safety badges (TX Inhibit, PA, ADC overload, TX) live in
    // m_safetyGroup's fixed-width slots so an alarm never shifts geometry.
    // TX Inhibit has no widget of its own; it paints onto m_txStatusBadge.
    // m_txInhibited guards the MOX handler from repainting over an active
    // interlock, and the toast is held so it can be dismissed the instant
    // inhibit clears rather than aging out.
    bool                     m_txInhibited{false};
    QPointer<class StatusToast> m_txInhibitToast;
    StatusBadge* m_paStatusBadge{nullptr};
    StatusBadge* m_txStatusBadge{nullptr};
    // Reserved safety slot group. Registered at rung 0 (never folds).
    QWidget* m_safetyGroup{nullptr};
    // Inactive slots dim rather than collapse, so geometry never moves
    // (design §4.5). Shared by buildStatusBar()'s construction-time state
    // and setTxInhibited() -- both toggle a safety-group badge's active
    // state and must agree on how "inactive" is represented.
    static void dimSafetyBadge(QWidget* w, bool active);

    // Phase 3Q Sub-PR-6 (F.1): RxDashboard — always-visible glance surface
    // for the ACTIVE slice's RX state. Replaces the Phase 3Q-7
    // m_statusConnInfo / m_statusLiveDot strip (those fields now live in
    // the segment tooltip / NetworkDiagnosticsDialog).
    // Task A5 (2026-08-02 bottom-banner cleanup): rebound on every
    // RadioModel::sliceAdded / activeSliceChanged so it follows whichever
    // slice is active, not a fixed slice(0) -- see the rebindDashboard
    // lambda in buildStatusBar().
    RxDashboard* m_rxDashboard{nullptr};

    // Phase 3M-4 Task 10: PSA bottom-banner indicator pair (FB + PS labels).
    // Inserted between m_rxDashboard and m_stationBlock per design doc §4 #5
    // (option B).  Visibility gated on caps.hasPureSignal in
    // onConnectionStateChanged().  Wired to PureSignal coordinator + MOX
    // controller from inside the widget (auto-wired via RadioModel).
    PsaIndicatorWidget* m_psaIndicator{nullptr};

    // Die VFO-Flagge ist am 2026-08-18 geloescht (Zielbild Punkt 1).
    // Hier standen m_vfoWidget und m_vfoWidgetsBySlice; die Notiz in
    // MainWindow.cpp an der Stelle von createSliceFlag nennt, was
    // wohin umgezogen ist.

    // Applets (Phase 3-UI)
    class AmpApplet*   m_ampApplet{nullptr};
    bool               m_ampAppletWired{false};  // guards one-time AmpApplet signal connects
    class KombiApplet* m_kombiApplet{nullptr};
    // Das zweite Feld (2026-11-06). Alle Stellen, die "das Feld" meinen,
    // gehen ueber kombiFelder() -- eine Kopie der Liste vergaesse eines.
    class KombiApplet* m_kombiApplet2{nullptr};
    QList<class KombiApplet*> kombiFelder() const;
    void verdrahteKombiFeld(class KombiApplet* feld);
    class RxApplet* m_rxApplet{nullptr};
    // Phase 3M-3a-ii Batch 6: cached so SetupDialog instances can route
    // CfcSetupPage's [Configure CFC bands…] button to the same modeless
    // TxCfcDialog instance owned by TxApplet (m_cfcDialog).
    class TxApplet* m_txApplet{nullptr};
    class PhoneCwApplet* m_phoneCwApplet{nullptr};
    // Phase 3R L2 — RADE-mode applet, visible only when the active slice
    // is in DSPMode::RADE_U or DSPMode::RADE_L.  Sits alongside
    // PhoneCwApplet in the panel stack and is shown/hidden in the same
    // dspModeChanged lambda.
    class RadeApplet* m_radeApplet{nullptr};
    class RttyDecoderApplet* m_rttyDecoderApplet{nullptr};
    class CwDecoderApplet*   m_cwDecoderApplet{nullptr};
    class Ft8Applet*         m_ft8Applet{nullptr};
    class Ft8PlaetzeApplet*  m_ft8PlaetzeApplet{nullptr};
    /// Das Plaetze-Panel geht von selbst auf, wenn etwas laeuft, und
    /// nach einer Nachlaufzeit wieder zu. Betreiber am 2026-10-30:
    /// „schoen waere, wenn hier die fenster eigens aufgehen und
    /// danach wieder zu gehen." Siehe `PanelAutomatik`.
    PanelAutomatik           m_ft8SlotsAutomatik;
    /// Gesetzt, solange die Automatik SELBST schaltet. Ohne diesen
    /// Merker hielt `visibilityChanged` jedes eigene Aufmachen fuer
    /// einen Griff des Betreibers und schaltete die Automatik im
    /// ersten Durchgang ab -- siehe `MainWindow.cpp` am Anschluss.
    bool                     m_ft8SlotsAutomatikSchaltet{false};
    /// Gesetzt, solange eine ANORDNUNG hergestellt wird (Profil beim
    /// Start, Profilwechsel, der Sprung auf FT8). Eine Anordnung ist
    /// kein Griff des Betreibers -- siehe den Hook in `buildUI`.
    bool                     m_sichtbarkeitOhneBetreiber{false};
    // Torn down and rebuilt on every rebindRttyRadeAvailability() call --
    // same QMetaObject::Connection-list idiom CommandBar::attach() uses,
    // so the dspModeChanged listener below never accumulates one dangling
    // connection per past slice.
    QList<QMetaObject::Connection> m_rttyRadeLinks;
    /// Der CAT-Server (2026-10-11). Vier Kanaele, nur auf 127.0.0.1.
    class RigctldServer* m_rigctldServer{nullptr};  // Phase 3K: Hamlib-Bruecke
    /// Das Contestprogramm als Fenster in Longpath (2026-11-04), nur mit
    /// LONGPATH_CONTESTPROGRAMM gebaut; sonst bleibt der Zeiger leer.
    class ContestFenster* m_contestFenster{nullptr};
    // Die Contest-Anordnung aus dem Profil, solange sie nicht hergestellt
    // werden konnte (Logbuch laeuft eigenstaendig o. ae.): die Erfassung
    // schreibt sie dann unveraendert zurueck, statt sie zu verlieren.
    QVariantMap m_contestAnordnungGemerkt;
    QTimer*     m_contestAnordnungTimer{nullptr};
    void stelleContestAnordnungHer(const QVariantMap& s);
    /// Bruecke anlegen und Longpaths rigctld fuer die Sitzung starten
    /// (nur lesend). Gemeinsam fuer das Logbuch und die Panels einzeln.
    void bereiteContestFensterVor();
    /// Das Untermenue Tools > Contest logbook panels beim Aufklappen
    /// fuellen: je Panel ein Haken "als eigenes Fenster".
    void fuelleContestPanelMenu(class QMenu* menu);
    class CatServer*    m_catServer{nullptr};
    class CatPtyServer* m_catPtyServer{nullptr};
    class EqApplet* m_eqApplet{nullptr};
    class VaxApplet* m_vaxApplet{nullptr};

    // Applets — Tasks 7-10 (NYI shells, hidden until Task 15 Container wiring)
    class DigitalApplet*    m_digitalApplet{nullptr};
    class PureSignalApplet* m_pureSignalApplet{nullptr};
    class DiversityApplet*  m_diversityApplet{nullptr};
    class CwxApplet*        m_cwxApplet{nullptr};
    class DvkApplet*        m_dvkApplet{nullptr};

    // ── Tastenkuerzel (2026-11-10) ──────────────────────────────────
    //
    // Die Belegung liegt in RadioModel (hotKeyBindings). Hier die drei
    // Teile, die das Fenster braucht. Reihenfolge ist Absicht: als
    // Mitglieder werden sie in umgekehrter Reihenfolge abgebaut, also
    // erst die Weiche (sie haengt als Ereignisfilter an der ganzen
    // Anwendung), dann der Verteiler, zuletzt die Ziele -- und alle drei
    // VOR den Kindern des Fensters, zu denen RadioModel gehoert.
    void richteTastenkuerzelEin();
    // Abstimmen wie ein Klick ins Panorama: Sperre meldet sich,
    // Begleitgeraet folgt.
    void stimmeVomBediener(class SliceModel* s, double hz);
    // Die Lupe im Panorama: Faktor < 1 vergroessert.
    void panoramaZoomen(double factor);
    std::unique_ptr<class MainWindowHotKeyTargets> m_hotKeyTargets;
    std::unique_ptr<class HotKeyDispatcher>        m_hotKeyDispatcher;
    std::unique_ptr<class KeyboardRouter>          m_keyboardRouter;
    std::unique_ptr<class FKeyPriority>            m_fKeyPriority;
    class QsoRecorderApplet* m_qsoRecorderApplet{nullptr};
    class RecordingApplet*   m_recordingApplet{nullptr};

    // ── KiwiSDR (Stufe 4, 2026-08-23) ──────────────────────────────
    //
    // Der Manager lebt am MainWindow, damit er die Anwendung ueberdauert
    // und nicht am Applet haengt, das der Betreiber jederzeit
    // wegschalten kann. Die Bruecke zwischen beiden steht in
    // MainWindow_KiwiSdr.cpp; was dort NOCH NICHT steht, ist am Kopf
    // jener Datei aufgezaehlt.
    class KiwiSdrManager* m_kiwiSdrManager{nullptr};
    // Je zugeordneter Scheibe die vier Nachfuehr-Verbindungen (Stufe 3b).
    QHash<int, QVector<QMetaObject::Connection>> m_kiwiSdrTrackingConnections;
    QPointer<class UpdateDialog> m_updateDialog;

    // ── SunSDR (TCI-Client, 2026-08-24) ──────────────────────────────
    //
    // Bewusst KEIN Manager wie bei KiwiSDR: der Betreiber hat EIN
    // SunSDR2 QRP, fest angeschlossen — eine Profilverwaltung fuer
    // mehrere Geraete waere hier Ueberbau (siehe CLAUDE.md, "Feature
    // scope"). m_sunSdrClient lebt trotzdem am MainWindow, aus
    // demselben Grund wie m_kiwiSdrManager: er soll die Anwendung
    // ueberdauern, nicht an einem Dialog haengen, der jederzeit
    // geschlossen werden kann.
    //
    // m_sunSdrTargetSliceId ist die Scheibe, deren Spur der SunSDR-Ton
    // speist — festgelegt beim Verbinden (derselbe Rueckfall wie bei
    // KiwiSDR: aktive Scheibe, sonst die erste, sonst eine anlegen,
    // siehe connectSunSdr), NICHT bei jedem Rahmen neu ermittelt: die
    // aktive Scheibe koennte sich waehrenddessen aendern, der SunSDR
    // bleibt aber an der Scheibe, mit der er verbunden wurde.
    class TciClient* m_sunSdrClient{nullptr};
    int m_sunSdrTargetSliceId{-1};

    // Flex-Treiber Stufe 2: die Scheibe, die den Flex-Ton traegt. Genau
    // wie m_sunSdrTargetSliceId ist das eine Zuordnung, die ueber
    // streamIndex() NICHT sichtbar ist — sie muss darum in die
    // Anspruchspruefungen der beiden anderen fremden Quellen hinein,
    // sonst bindet eine von ihnen dieselbe Scheibe ein zweites Mal.
    int m_flexTargetSliceId{-1};
    /// Wahr, wenn die Scheibe fuer die Flex von uns angelegt wurde —
    /// dann raeumen wir sie beim Trennen auch wieder weg.
    bool m_flexSliceWasCreated{false};
    /// Echo-Wachposten fuer die Flex, dasselbe Muster wie
    /// `m_sunSdrApplyingRemoteState`: waehrend ein Zustand VON der
    /// Maschine in die Scheibe geschrieben wird, darf die Scheibe
    /// nicht zurueckfunken. Sonst antwortet jeder Zustandsschub mit
    /// einem `slice tune`, der den naechsten Schub ausloest.
    bool m_flexApplyingRemoteState{false};
    /// Die Verbindung Scheibe -> Flex (Abstimmen nach aussen).
    /// Von Hand gehalten, weil `Qt::UniqueConnection` bei einer Lambda
    /// nicht greift und dort still GAR NICHTS verbindet.
    QMetaObject::Connection m_flexAbstimmBand;
    // Senden ueber die Flex (Stufe 2).
    QMetaObject::Connection m_flexMoxBand;
    QMetaObject::Connection m_flexTuneBand;
    QMetaObject::Connection m_flexLeistungBand;
    QMetaObject::Connection m_flexAbstimmleistungBand;
    QMetaObject::Connection m_flexSendetBand;
    /// Die SWR-Sperre meldet sich am Schirm. Ohne diesen Band
    /// griff sie stumm -- siehe MainWindow_Flex.cpp.
    QMetaObject::Connection m_flexSwrBand;
    /// Der Blick des Betreibers zieht den Panadapter der Maschine nach.
    QMetaObject::Connection m_flexBlickBand;
    QTimer* m_flexPanNachzug{nullptr};
    /// Die Spur: WER schiebt die Scheibe im Verbindungsfenster?
    /// Siehe MainWindow_Flex.cpp, "Die Spur der Frequenz".
    QMetaObject::Connection m_flexFreqSpur;
    qint64  m_flexSpurBisMs{0};
    double  m_flexSpurLetzteHz{0.0};
    /// Dasselbe fuer Betriebsart und Filterflanken.
    QMetaObject::Connection m_flexArtBand;
    QMetaObject::Connection m_flexFilterBand;
    /// Rauschminderung, Störaustaster, Kerbfilter → Maschine
    /// (2026-10-28). Drei Bänder, ein Befehl: siehe
    /// RadioModel::setFlexSliceNoise.
    QMetaObject::Connection m_flexNrBand;
    QMetaObject::Connection m_flexNrStufeBand;
    QMetaObject::Connection m_flexNbBand;
    QMetaObject::Connection m_flexAnfBand;
    QMetaObject::Connection m_flexAnfStufeBand;   ///< anf_level (Flex)
    /// Die Rauschsperre. Eigene Baender, weil `wireSliceSignals` am
    /// Flex ueberhaupt nicht laeuft (sie steigt bei `!m_connection`
    /// aus, und das ist die OpenHPSDR-Verbindung) -- gemessen am
    /// 2026-10-31: Knopf und Regler erreichten das Modell, das Modell
    /// nie die Maschine.
    QMetaObject::Connection m_flexSqlAnBand;
    QMetaObject::Connection m_flexSqlStufeBand;
    QMetaObject::Connection m_flexSqlAutoBand;
    /// Die uebrigen Regler der Scheibe -- aus demselben Grund eigene
    /// Baender: am Flex laeuft `wireSliceSignals` nicht.
    QMetaObject::Connection m_flexAgcBand;
    QMetaObject::Connection m_flexAgcSchwelleBand;
    QMetaObject::Connection m_flexTonpegelBand;
    QMetaObject::Connection m_flexStummBand;
    QMetaObject::Connection m_flexBalanceBand;
    QMetaObject::Connection m_flexRitAnBand;
    QMetaObject::Connection m_flexRitHzBand;
    QMetaObject::Connection m_flexXitAnBand;
    QMetaObject::Connection m_flexXitHzBand;
    QMetaObject::Connection m_flexApfBand;
    /// Einmal nachziehen, sobald die Maschine ihre Scheibe meldet.
    QMetaObject::Connection m_flexReglerNachtrag;
    /// Einmal je Verbindung: die Fensterbreite auf den Ausschnitt der
    /// Flex stellen. Danach gehoert der Zoom wieder dem Betreiber.
    bool m_flexAusschnittUebernommen{false};
    /// Der zuletzt von der Maschine gemeldete Ausschnitt. Dient dazu,
    /// eine blosze Mittenverschiebung von einer echten Breitenaenderung
    /// zu unterscheiden — sonst wirft jedes Abstimmen den Zoom des
    /// Betreibers weg (2026-10-23).
    double m_flexFensterMitteHz{0.0};
    double m_flexFensterBreiteHz{0.0};

    // ── Senden ueber die Flex (2026-10-23, Stufe 2) ──────────────────
    /// Die Mikrofonpumpe. Laeuft NUR, solange getastet ist.
    QTimer* m_flexSendePumpe{nullptr};
    /// Mono aus `AudioEngine::pullTxMic`, bevor es verdoppelt wird.
    QVector<float> m_flexMikMono;
    // Hoechste Spitze, die der erzeugte FT8-Ton bisher an die Flex
    // gegeben hat. Nur fuer die eine Protokollzeile je Sendung.
    float m_flexFt8Spitze{0.0f};

    // ── Wie lange der Empfang nach dem Loesen schweigt (2026-10-28) ──
    //
    // Am 2026-10-28 verlor der FT8-Dekoder ganze Hoerperioden: drei
    // Fenster hintereinander „0 Funde", dann wieder 17. Gegengerechnet
    // mit dem Wasserfall lag jede leere Hoerperiode in einem Zeitraum
    // ohne eine einzige Zeile. Was fehlte, war die Zahl: wie lange
    // genau vom Loesen bis zum ersten Empfangssample. Raten hilft hier
    // nicht — 15 Sekunden Hoerperiode entscheiden ueber alles oder
    // nichts.
    qint64 m_flexLoesenMs{0};     ///< Zeitpunkt des letzten „MOX aus"
    int    m_flexTonImSenden{0};  ///< Tonpakete WAEHREND des Sendens

    // ── Was JE SENDUNG wirklich hinausging (2026-10-29) ──────────────
    //
    // `m_flexFt8Spitze` darueber ist ein Hoechststand ueber die ganze
    // Laufzeit und meldet sich nur, wenn er steigt — nach der ersten
    // Sendung also nie wieder. Am 2026-10-29 fehlte genau deshalb die
    // Antwort auf die eine Frage, an der alles hing: hat Longpath
    // nichts geschickt, oder hat die Maschine nichts angenommen?
    // (Gemessen war nur ihre Seite: MICPEAK −150 dBFS im vierten
    // Durchgang gegen −5,98 dBFS im fuenften.) Diese drei werden je
    // Sendung zurueckgesetzt und je Sendung gemeldet.
    int    m_flexTxProben{0};     ///< Proben an die Maschine, diese Sendung
    int    m_flexTxVomMik{0};     ///< Zuege, die am Mikrofon statt an FT8 hingen
    float  m_flexTxSpitze{0.0f};  ///< Spitze dieser Sendung, nach der Daempfung
    /// Einmal je Sendung melden, wenn das Mikrofon ueber −1 dBFS kam.
    /// Ohne das stuende die Zeile bei jedem Pumpentakt im Protokoll.
    bool   m_flexMikZuLaut{false};
    /// Dasselbe, aber fuer den Fall, dass erst der KANALZUG ueber den
    /// Riegel kommt. Eine eigene Flagge, weil es eine andere Ursache
    /// ist: die Quelle lag dann richtig, und wer die beiden in einer
    /// Flagge zusammenfasst, verschluckt die zweite Meldung.
    bool   m_flexKetteZuLaut{false};
    /// Einmal je Sendung ins Protokoll, dass die Kette mitlief —
    /// sonst ist am Protokoll nicht zu sehen, ob der Ton bearbeitet
    /// hinausging oder roh.
    bool   m_flexKetteGemeldet{false};

    // ── Die Pumpe braucht eine Uhr (2026-10-29) ──────────────────────
    //
    // Der Sendeton ist ein Echtzeitstrom: was die Pumpe an die Maschine
    // gibt, spielt die Maschine ab. Die Pumpe hatte aber keine
    // Zeitreferenz — sie schob bei JEDEM Taktschlag starr 480 Rahmen
    // hinaus, ganz gleich, wann der Schlag kam. Auf einem Hauptfaden,
    // der nebenher Wasserfall, Spektrum und Kachel malt, kommt er
    // nicht alle 5 ms.
    //
    // Am Geraet gemessen (Protokoll 2026-10-29, 13:05 bis 13:07, vier
    // Sendungen von je 12,94 s). Hinausgehen muessten 621.120 Rahmen:
    //
    //     180.000 Rahmen = 13,9 kHz = 0,29-fache Geschwindigkeit
    //     332.640 Rahmen = 25,7 kHz = 0,54-fache
    //     165.600 Rahmen = 12,8 kHz = 0,27-fache
    //     716.160 Rahmen = 55,3 kHz = 1,15-fache
    //
    // Der Ton ging also mal auf ein Viertel gedehnt, mal zu schnell
    // hinaus, und vom 15-s-Fenster kam oft nur das erste Drittel an.
    // Dasselbe erklaert den spaeten Anfang: bis die Pumpe durch die
    // fuehrende Stille der Wellenform war, vergingen 3,6 s statt 0,5 —
    // „FT8-Sendepegel" meldete den ersten Ton um 13:05:33,655, getastet
    // war um 13:05:30,033. Betreiber: „leistung kommt zeitversetzt."
    //
    // Jetzt zaehlt nicht mehr der Taktschlag, sondern die Uhr: je
    // Schlag geht genau so viel hinaus, wie seit dem Tasten faellig
    // geworden ist. Ein spaeter Schlag holt auf, ein frueher wartet.
    QElapsedTimer m_flexSendeUhr;      ///< laeuft ab dem Tasten
    qint64 m_flexSendeGeliefert{0};    ///< Rahmen, die schon hinaus sind
    qint64 m_flexSendeRueckstand{0};   ///< groeszter Rueckstand, Rahmen
    /// Dasselbe als Stereo, wie die Maschine es erwartet.
    QVector<float> m_flexMikStereo;
    /// Haben wir selbst getastet? Trennt unser `xmit` von der Taste am
    /// Mikrofon der Maschine, die genauso in der Verriegelung ankommt.
    bool m_flexSelbstGetastet{false};
    /// Warum das Flex-Bild gerade nicht gezeichnet wird — damit die
    /// Meldung einmal je Grund kommt und nicht 25 Mal je Sekunde.
    enum class FlexBildGrund { Unbekannt, KeinAusschnitt, KeinFenster,
                               FremdquelleAus, Zeichnet };
    FlexBildGrund m_flexBildGrund{FlexBildGrund::Unbekannt};
    // Der Frequenzausschnitt der Flex-Anzeige, in Hz. Er steht NICHT im
    // FFT-Paket — nur die Wasserfall-Kachel traegt ihre Frequenzen
    // (gemessen). Solange er unbekannt ist, wird nichts gezeichnet:
    // lieber kein Bild als ein Bild an der falschen Frequenz.
    double m_flexSpanLowHz{0.0};
    /// Das letzte Empfangsbild, das beim Senden stehenbleibt, samt
    /// dem Ausschnitt, zu dem es gehoert. Siehe MainWindow_Flex.cpp,
    /// „Beim Senden das Bandbild halten".
    QVector<float> m_flexBildFrost;
    double m_flexBildFrostLowHz{0.0};
    double m_flexBildFrostHighHz{0.0};
    double m_flexSpanHighHz{0.0};

    // ── SunSDR-Bild (2026-08-24) ──────────────────────────────────────
    //
    // Letzte an den FFTEngine gemeldete I/Q-Rate, um QMetaObject::
    // invokeMethod nicht auf jedem einzelnen Rahmen erneut aufzurufen,
    // wenn sich nichts geaendert hat -- nur bei einer echten Umstellung
    // (siehe TciClient.h: TCI erlaubt iq_samplerate jederzeit zu aendern).
    int m_sunSdrLastAppliedIqRateHz{0};
    // Letzte von TciClient::ddcCenterChanged gemeldete Mittenfrequenz
    // ("dds:", siehe TciClient.h). 0 = noch keine Zeile gesehen; dann
    // faellt applySunSdrStreamWindow() auf die Scheibenfrequenz zurueck.
    qint64 m_sunSdrDdcCenterHz{0};

    // ── SunSDR-Steuerung (2026-08-24) ─────────────────────────────────
    //
    // Zwei-Wege: eingehende vfo:/modulation:-Zeilen uebernehmen die
    // Scheibenfrequenz/-betriebsart (applyRemoteSunSdrFrequency/
    // -Modulation), eine Bedieneraenderung an der Scheibe sendet
    // umgekehrt vfo:/modulation: an ExpertSDR2 (wireSunSdrOutboundControl,
    // pro Verbindungsaufbau neu gesetzt, siehe connectSunSdr).
    //
    // m_sunSdrApplyingRemoteState ist der Echo-Wachposten: waehrend eine
    // eingehende Zeile die Scheibe schreibt, muss die Ausgangsseite still
    // bleiben, sonst schickt Longpath denselben Wert postwendend zurueck
    // an das Geraet, von dem er gerade kam (dasselbe Bool-Wachposten-Muster
    // wie RadioModel::m_rollingBackFrequency; siehe RadioModel.cpp).
    bool m_sunSdrApplyingRemoteState{false};
    // Verbindungen zur AKTUELLEN Ziel-Scheibe -- werden bei jedem
    // connectSunSdr()-Aufruf zuerst getrennt und neu gesetzt, weil sich
    // die Ziel-Scheibe zwischen zwei "Verbinden…"-Klicks aendern kann
    // (derselbe Rueckfall wie bei m_sunSdrTargetSliceId). Ohne das
    // wuerde die Ausgangsseite nach einem zweiten Verbindungsaufbau auf
    // eine moeglicherweise nicht mehr existierende oder falsche Scheibe
    // zeigen.
    QMetaObject::Connection m_sunSdrFreqOutConn;
    QMetaObject::Connection m_sunSdrModeOutConn;
    // Meldet, wenn die Ziel-Scheibe NACH dem Verbinden noch eine echte
    // DDC-Bindung bekommt (bindUnboundSlices() beim naechsten Verbinden
    // eines echten Funkgeraets) -- ohne das blieben Ton-/Steuerungs-
    // Verdrahtung auf einer inzwischen echten Scheibe stehen (gegen-
    // geprueft 2026-08-24, siehe releaseSunSdrSlice).
    QMetaObject::Connection m_sunSdrStreamIndexWatchConn;
    void applyRemoteSunSdrFrequency(qint64 hz);
    void applyRemoteSunSdrModulation(const QString& mode);
    void wireSunSdrOutboundControl(class SliceModel* slice);
    // Der EINZIGE Weg, an die aktuelle SunSDR-Zielscheibe zu kommen --
    // liefert nullptr, wenn es keine gibt ODER sie inzwischen eine echte
    // DDC-Bindung hat. Jeder Verbraucher (Ton, Panadapter, Steuerung)
    // muss darueber gehen, nicht m_sunSdrTargetSliceId+sliceById() von
    // Hand wiederholen -- genau das Auseinanderlaufen kostete vorher die
    // Sicherheitsschranke an mehreren, aber nicht allen Verbrauchern.
    class SliceModel* sunSdrControllableSlice() const;
    // Dasselbe Tor fuer den Flex-Weg: nie eine Scheibe mit echter
    // DDC-Bindung (streamIndex() >= 0) fuettern oder steuern.
    class SliceModel* flexControllableSlice() const;
    void wireFlex();

    /// Das Senden ueber die Flex verdrahten (Stufe 2). Tastet nichts —
    /// es haengt nur MOX, TUNE und die Leistungsregler an die Maschine
    /// und richtet die Mikrofonpumpe ein.
    void verdrahteFlexSenden();
    // Gibt die aktuelle Zielscheibe vollstaendig frei -- Ton, Panadapter-
    // Zuordnung, Ausgangssteuerung -- und setzt m_sunSdrTargetSliceId auf
    // -1 zurueck. Aufgerufen, wenn die Scheibe geloescht wird oder eine
    // echte DDC-Bindung bekommt; der Betreiber muss danach erneut
    // "Verbinden…" waehlen, um eine neue Zielscheibe zu bekommen (keine
    // stille Automatik).
    void releaseSunSdrSlice(const QString& reason);

    class KiwiSdrApplet*  m_kiwiSdrApplet{nullptr};
    class KiwiWaterfallPanel* m_kiwiWaterfallPanel{nullptr};

    // ── Spracherkennung (2026-08-23) ────────────────────────────────
    //
    // Der Betreiber hat den OERTLICHEN Weg gewaehlt: ein
    // Whisper-Dienst auf seinem eigenen Rechner, der Ton verlaesst die
    // Maschine nicht. Was hier liegt, ist die Verbindung dorthin —
    // Abgriff, Dienst, Anzeige.
    //
    // Der Abgriffring gehoert dem MainWindow und nicht dem Dienst: die
    // Tonmaschine schreibt hinein, und ein Ring, der unter dem
    // Tonfaden verschwindet, weil jemand den Dienst wechselt, waere
    // ein Absturz mit Ansage.
    std::unique_ptr<class AudioTapRing> m_asrTapRing;
    class AsrService*     m_asrService{nullptr};
    class AsrApplet*      m_asrApplet{nullptr};
    class CatRigApplet*   m_catRigApplet{nullptr};
    int                   m_asrTapSlice{-1};
    class BandwidthFilterApplet* m_bwFilterApplet{nullptr};
    class CatApplet*        m_catApplet{nullptr};
    class TunerApplet*      m_tunerApplet{nullptr};
    // Code review, 2026-09-13: rebindTunerAppletBand() re-makes this on
    // every RadioModel::activeSliceChanged so TunerApplet's Save/Recall/
    // Clear actions always address the truly active slice's band -- see
    // rebindTunerAppletBand()'s own comment for the bug this replaces
    // (wireSliceToSpectrum() used to wire this once, to slice 0, forever).
    QMetaObject::Connection m_tunerAppletBandConn;

    // Phase 3P-III Task 14: RF-Kit RF2K-S applet.
    class Rf2ksApplet*      m_rfKitApplet{nullptr};
    class SpeApplet*        m_speApplet{nullptr};
    class Kpa500Applet*    m_kpa500Applet{nullptr};
    class AcomApplet*      m_acomApplet{nullptr};

    // Phase 23: TCI server + applets.
    // m_tciServer is nullptr in non-WebSocket builds (HAVE_WEBSOCKETS not defined).
    TciServer*         m_tciServer{nullptr};
    TciApplet*         m_tciApplet{nullptr};
    ClientChainApplet* m_clientChainApplet{nullptr};

    // Phase 3J-1 closeout Item 2 (2026-05-12): TCI message log viewer.
    // Lazy-constructed on the first "Show Log..." click from the Setup
    // dialog; persistent thereafter for the lifetime of MainWindow so the
    // window survives Setup close/reopen.  Connected to
    // TciServer::messageLogged via Qt::QueuedConnection so emit-side never
    // blocks the server.  Nullptr until first show.
    TciLogWindow* m_tciLogWindow{nullptr};
    void showTciLogWindow();

    // Bottom label of the TCI indicator tile — captured from makeIndicator()
    // so updateTciIndicator() can change color + text without a findChild scan.
    QLabel* m_tciIndicatorBotLabel{nullptr};

    // Live connection-count cache for updateTciIndicator().  Updated from
    // clientConnected / clientDisconnected signals.
    int  m_tciClientCount{0};
    bool m_tciServerRunning{false};
    bool m_tciHasTxClient{false};

    /// Live notification toasts, newest last. Bench report 2026-07-30:
    /// QStatusBar::showMessage hides every non-permanent widget for the
    /// life of the message, and the whole bottom bar is one such widget
    /// (see buildStatusBar), so a TUNE with PureSignal active blanked the
    /// CH pill, PS indicator, radio name, CAT/TCI state, PA/TX badges and
    /// the clock for six seconds. Notices moved off the bar to here.
    ///
    /// QPointer because each toast deletes itself on close, by timer or
    /// by click, without telling us first.
    QList<QPointer<class StatusToast>> m_toasts;



    /// Show a transient notice without disturbing the bottom bar.
    /// Repeats of a message already on screen restart that toast's
    /// countdown instead of stacking a duplicate beneath it.
    ///
    /// Returns the toast so a caller whose condition can end early can
    /// dismiss it rather than leaving a stale notice up for its full
    /// timeout. Hold it by QPointer: it deletes itself on close.
    StatusToast* showToast(const QString& message,
                           ToastSeverity severity,
                           int timeoutMs);

    /// Die gemerkte Palette gilt erst beim naechsten Start -- fragen,
    /// ob jetzt neu gestartet werden soll. Unter
    /// QStandardPaths::isTestModeEnabled() kehrt die Methode sofort
    /// zurueck: ein Pruefstand darf an keinem modalen Fenster haengen
    /// bleiben (dieselbe Regel wie showAudioDiagnoseDialog()).
    /// Den Haken des Panadapter-Schalters auf den wirklichen Zustand
    /// setzen, ohne dabei sein eigenes toggled() auszuloesen.
    void syncFloatPanAction();

    /// Flaggen anlegen, abraeumen und an ihre Traeger legen. Drei
    /// Schritte, drei Stellen — rebuildSliceFlags() beim Kommen und
    /// Gehen einer Scheibe, refreshSliceFlags() bei Frequenz, Mode und
    /// Groessenaenderung.
    void rebuildSliceFlags();
    void refreshSliceFlags();

    void offerPaletteRestart(const QString& paletteName);

    /// Beendet dieses Programm und laesst es wieder oeffnen. Nutzt den
    /// abgekoppelten Helfer der Aktualisierung, damit derselbe Weg
    /// (und dieselben Argumente, „--profile" eingeschlossen) gilt.
    void restartNow();

    /// The suspended-streams notice, kept so it can be taken down the
    /// moment the streams come back instead of aging out.
    QPointer<class StatusToast> m_suspendToast;

    /// Re-stack live toasts bottom-right, newest nearest the bar.
    /// Called on show, on close, and on move/resize.
    void restackToasts();

    // Spectrum overlay panel
    /// Pan-0's strip. Kept as a stable target for the display-settings and
    /// clarity wiring, which is still global rather than per-pan.
    class SpectrumOverlayPanel* m_overlayPanel{nullptr};

    /// One control strip per pan, keyed by pan id. QPointer because the widget
    /// is parented to its pan's SpectrumWidget and dies with it when a layout
    /// switch retires the pan.
    QHash<QString, QPointer<class SpectrumOverlayPanel>> m_overlayPanels;

    // Applet panel — scrollable content widget inside Container #0
    class AppletPanelWidget* m_appletPanel{nullptr};

    // Applet visibility controller (Longpath-original) — backs the
    // Containers > Applets top menu and the panel banner ☰ menu.
    // Constructed in the layout-build path after the panel is wired.
    // Rotor + logbook dock (Tools > Rotor...). Lazy; owned by `this`.
    QDockWidget*     m_rotorDock{nullptr};
    // Ob das Profil den Rotor/Log-Dock sichtbar haben will -- gesetzt
    // beim Laden, aber erst mit der ersten Verbindung angewandt (siehe
    // die connectionStateChanged-Bindung neben der Profil-Wiederherstellung):
    // vor dem Verbinden ist der Kompass ohne Ziel bedeutungslos.
    bool             m_rotorDockWantedVisible{true};
    // Betreiber 2026-08-28: dieselbe Vorher-verstecken-Regel gilt auch fuer
    // jedes andere schwebende Meter/Applet-Fenster (S-Meter, Stehwelle,
    // Mitschrift, ...) -- restoreState() zeigt sie sonst unconditional beim
    // Start, egal ob eine Verbindung steht. Nur die Fenster gemerkt, die
    // deswegen tatsaechlich versteckt wurden, damit die erste Verbindung
    // genau die wieder zeigt und nichts, was ohnehin schon zu war.
    QList<QPointer<QWidget>> m_floatingContainersHiddenPreConnect;
    /// Was beim Minimieren des Hauptfensters mit ins Dock ging und beim
    /// Zurueckholen wieder gezeigt wird (2026-11-03).
    QList<QPointer<QWidget>> m_schwebendeFensterImDock;
    /// Der Rotor/Log-Einmalhaken (Konstruktor) hat bei der ersten
    /// Verbindung dieser Sitzung zugeschlagen und moechte m_rotorWindow
    /// zeigen -- ausgefuehrt wird der Wunsch in
    /// restoreFloatingWindowsHiddenBehindConnectMask(), sobald die
    /// Connect-Maske aus dem Weg ist.
    bool m_showRotorAfterConnectMask{false};
    /// Fuehrt die Windrose im Spektrum nach, solange sie zu
    class WindowTitleBar* m_rotorHeader{nullptr};
    class ToolWindow*     m_rotorWindow{nullptr};
    // Seitenbereich (MainWindow_SideArea.cpp). Die Stelle in der Spalte,
    // an die ein Applet beim Herausnehmen zurückkehrt.
    QPointer<class SideAreaWindow> m_sideArea;
    QHash<QString, int> m_sideAreaDockIndex;
    class SideAreaWindow* ensureSideArea();
    void dissolveSideArea(SideAreaExit exit);
    void showSideAreaAddMenu(const QPoint& globalPos);
    void onSideAreaCollapsed(bool collapsed, int delta);
    /// Schwebende Panadapter, deren rechter Rand an `edgeX` grenzt und die
    /// `area` senkrecht überlappen, um `dx` breiter (+) oder schmaler (−).
    void resizeNeighbourPans(int edgeX, const QRect& area, int dx);
    void applySideAreaState(const QVariantMap& s);
    QString sideAreaTitleFor(const QString& id) const;
    /// Seitenkennung eines schwebenden Fensters (leer: kann keine Seite
    /// werden, etwa ein Panadapter).
    QString sideAreaIdForWindow(QWidget* w) const;
    /// Kennung aus dem Auswaehler → Seitenkennung (leer: keine Seite).
    QString sideAreaKeyFor(const QString& id) const;
    /// Profil jetzt aufnehmen und ablegen (nicht beim Herunterfahren).
    void saveLayoutNow();
    QrzClient*           m_qrzClient{nullptr};
    // One window, reused. Kept so a second measurement lands in the
    // same place as the first rather than beside it.
    AntennaWindow*       m_antennaWindow{nullptr};
    QrzLogbookUploader*  m_qrzUploader{nullptr};
    class SatelliteService* m_satellites{nullptr};
    CloudlogUploader*    m_cloudlogUploader{nullptr};
    AdifNetworkUploader* m_localLogUploader{nullptr};

    // Kopfleiste im Zuschnitt der Vorlage und das Plus an ihrem rechten Ende.
    // Das Plus entsteht erst, wenn m_appletVis alle Kategorien kennt —
    // deshalb zwei Zeiger und nicht einer.
    class CommandBar* m_commandBar{nullptr};
    /// Nachlauf fuer den Schutzrahmen (2026-10-14).
    QTimer* m_schutzrahmenTimer{nullptr};
    /// Der sichtbare Hinweis auf die gesperrten Streifen.
    class SchutzrahmenHinweis* m_rahmenHinweis{nullptr};
    QTimer* m_rahmenHinweisTimer{nullptr};
    class AddWidgetButton* m_addWidget{nullptr};
    class ProfileRail* m_profileRail{nullptr};
    /// Die Reihe ganz links im Mittelteil -- dort haengt die Schiene,
    /// wenn sie senkrecht steht.
    ///
    /// OHNE das Schluesselwort "class" davor: eine Deklaration der Form
    /// "class QHBoxLayout* x" legt im Klassenbereich einen NEUEN,
    /// unvollstaendigen Typ dieses Namens an und verdeckt den echten --
    /// der ganze Rest der Datei bekam dann "incomplete type".
    QHBoxLayout* m_profileRailRow{nullptr};
    QWidget*     m_profileRailHome{nullptr};
    class AddWidgetButton* m_addWidgetBtn{nullptr};
    void richtePlusEin(class AddWidgetButton* plus);

    /// Welche Applets als freie Kachel auf der Flaeche liegen:
    /// Panelkennung -> Container-Kennung.
    QHash<QString, QString> m_canvasApplets;

    /// Kommen neue Fenster als freie Kachel? Gemerkt unter
    /// „FreeCanvasMode".
    bool m_freeCanvasMode{false};
    class QAction* m_freeCanvasAction{nullptr};
    class LayoutProfiles* m_layoutProfiles{nullptr};

    /// Die beiden Zeiger-/Balkeninstrumente (2026-08-17). Sie hängen an
    /// MeterPoller::readingUpdated, also am selben Umlauf wie die
    /// Meter-Items.
    ///
    /// QPointer, nicht roh (2026-11-08): closeEvent loescht die
    /// schwebenden Applet-Fenster samt Applet, und bis quit() laeuft die
    /// Ereignisschleife weiter -- ein Meter-Takt danach traf das
    /// geloeschte S-Meter (Absturzbericht 12:35, NeedleInstrument::
    /// onReading mit this = 0, 57 ms nach closeEvent).
    QPointer<class FrequencyApplet>  m_frequencyApplet;
    // Bandstapel (2026-11-11, Entwurf A): das Applet unter der Frequenz.
    QPointer<class BandStackApplet>  m_bandStackApplet;
    QPointer<class InstrumentApplet> m_swrInstrument;
    QPointer<class InstrumentApplet> m_signalInstrument;
    QPointer<class RundumDialog>     m_rundumDialog;   // Rundum-Lauf (2026-11-08)
    // Betreiber 2026-09-03: "leider funktioniert die Frequenzanzeige
    // nicht im S-Meter" -- die urspruengliche Verdrahtung (buildUI())
    // lief EINMAL gegen die beim Start aktive Scheibe; ein frisches
    // MainWindow hat aber null Scheiben, bevor ein Funkgeraet verbindet
    // (siehe MainWindow_SunSdr.cpp-Kommentar am selben Fund), also lief
    // sie damals ins Leere und niemand versuchte es je wieder. Diese
    // Verbindung wird jetzt bei jedem RadioModel::activeSliceChanged neu
    // gezogen -- muss vor dem naechsten Ziehen wieder geloest werden,
    // sonst haengt am Ende eine tote Scheibe noch mit dran.
    QMetaObject::Connection m_instrumentFreqConn;

    // Codereview 2026-09-03 (gefunden, nicht gemeldet): dieselbe Krankheit
    // wie bei m_instrumentFreqConn oben, diesmal bei der Zoom-Leiste unter
    // dem Panadapter. Die urspruengliche Fassung verdrahtete
    // SpectrumWidget::frequencyRangeChanged EINMAL gegen die beim Bau
    // aktive Flaeche (activeSpectrumWidget()) -- bei mehreren Flaechen
    // (2v/2h/2x2-Anordnung) folgte die Leiste danach dauerhaft nur noch
    // dieser einen Flaeche, auch nachdem eine andere aktiv wurde, und
    // konnte nach einem Anordnungswechsel sogar an einer inzwischen
    // entfernten Flaeche haengen bleiben (Qt trennt dann automatisch,
    // die Leiste folgt danach ueberhaupt keiner Flaeche mehr). Wird jetzt
    // bei jedem PanadapterStack::activePanChanged neu gezogen.

    AppletVisibilityController* m_appletVis{nullptr};
    QHash<QString, AppletWidget*> m_appletsById;

    /// Die abgelösten Applets, nach Kennung. Ein Eintrag hier heisst:
    /// dieses Applet steht NICHT in der Spalte. Die Fenster gehören
    /// diesem Fenster (Qt-Elternschaft), damit sie beim Beenden
    /// mitgehen.
    QHash<QString, class AppletFloatingWindow*> m_floatingApplets;
    QHash<QString, QAction*> m_topMenuAppletActions;
    /// Containers > Contest logbook: je Panel des Contestprogramms ein
    /// Haken "als eigenes Fenster" (2026-11-04), siehe MainWindow.cpp.
    QHash<QString, QAction*> m_containersContestActions;

    // Phase 3O Sub-Phase 10 Task 10c: host strip for the menu bar +
    // MasterOutputWidget. Owned by QMainWindow via setMenuWidget().
    TitleBar* m_titleBar{nullptr};

    // Phase 3P-II Task 21: TGXL status bar chip.
    // Shown when TunerModel::presenceChanged fires true; hidden otherwise.
    // Text is "TGXL" / "TGXL OPER" / "TGXL BYPS" / "TGXL SBY".
    QLabel* m_tgxlChip{nullptr};

    // Phase 3P-II Phase 4 Task 97: de-bounce flag for power cap soft-alert
    // toast.  Set true when the toast fires; reset false when fwd drops back
    // below the cap so a new exceedance event re-arms.
    bool m_powerCapToastShown{false};
};

} // namespace Longpath
