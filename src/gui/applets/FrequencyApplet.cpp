// =================================================================
// src/gui/applets/FrequencyApplet.cpp  (Longpath)
// =================================================================
// Siehe FrequencyApplet.h.
// =================================================================

#include "gui/applets/FrequencyApplet.h"
#include "gui/ScopedChildWidget.h"

#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "core/AppSettings.h"
#include "core/CompanionRigController.h"
#include "core/RitXitAnzeige.h"
#include "gui/meters/MeterPoller.h"

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QMenu>
#include <QPointer>
#include <QVBoxLayout>

namespace Longpath {

FrequencyApplet::FrequencyApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    // ── Die Kachelreihe steht OBEN ───────────────────────────────────
    //
    // So wie auf der Vorlage des Betreibers: erst die Uebersicht ueber
    // alle Empfaenger, darunter gross der eine, den er gerade bedient.
    // Umgekehrt waere die grosse Zahl der Kopf und die Reihe eine
    // Fussnote — sie ist aber der Wegweiser.
    m_tiles = new VfoTileRow(model, this);
    lay->addWidget(m_tiles);
    connect(m_tiles, &VfoTileRow::kiwiToggleRequested,
            this, &FrequencyApplet::toggleKiwiDisplay);

    m_instrument = new FrequencyInstrument(this);
    lay->addWidget(m_instrument);

    // ── Die beiden Zusatzzeilen ──────────────────────────────────────
    //
    // Sie werden IMMER gebaut, aber nur auf Wunsch gezeigt. Der Grund
    // ist Erfahrung aus dieser Woche: ein Widget, das seine Kinder
    // erst beim Einschalten anlegt, muss dabei das Layout umbauen —
    // und genau daran hat der Panadapter beim Abloesen gezuckt. Ein
    // setVisible ist ein Zustand, kein Umbau.
    //
    // Es sind gewoehnliche BarInstrument, also gelten fuer sie
    // dieselben Formen wie fuer die eigenstaendigen Anzeigen: Segmente
    // und die Roehre lassen sich hier genauso einstellen.
    m_powerBar = new BarInstrument(this);
    m_powerBar->setCompact(true);
    m_powerBar->setPrimary(MeterBinding::TxPower);
    lay->addWidget(m_powerBar);

    m_swrBar = new BarInstrument(this);
    m_swrBar->setCompact(true);
    m_swrBar->setPrimary(MeterBinding::TxSwr);
    lay->addWidget(m_swrBar);

    // Betreiber 2026-08-30: "S-Meter bitte noch bei Frequenz
    // hinzufügen" -- dieselbe Quelle wie die eigenstaendige S-Meter-
    // Anzeige (m_signalInstrument in MainWindow), nur als knappe
    // Zusatzzeile hier.
    m_signalBar = new BarInstrument(this);
    m_signalBar->setCompact(true);
    m_signalBar->setPrimary(MeterBinding::SignalAvg);
    lay->addWidget(m_signalBar);

    applyRowVisibility();

    // Das Widget schreibt NICHT selbst. Es meldet die gewuenschte
    // Frequenz, und der Weg dorthin ist derselbe wie bei jeder anderen
    // Abstimmung — SliceModel::setFrequency, mit allem, was daran
    // haengt. Stellenweises Drehen fuehrt so wenig ueber eine
    // Bandkante wie das Rad auf der Flagge.
    connect(m_instrument, &FrequencyInstrument::frequencyEdited,
            this, [this](double hz) {
        if (m_slice) { m_slice->setFrequency(hz); }
    });

    // Die zweite Versatzquelle: das fremde Geraet. Sie haengt am
    // Modell, nicht an der Scheibe, darum hier und nicht in
    // bindSlice(). Verbindung und Zustand beide -- faellt der CAT-Weg
    // aus, muss die Zeile wieder die Scheibe zeigen, statt den letzten
    // Geraetewert stehen zu lassen.
    if (auto* rig = model ? model->companionRig() : nullptr) {
        connect(rig, &CompanionRigController::catZustandGeaendert,
                this, &FrequencyApplet::versatzNachziehen);
        connect(rig, &CompanionRigController::verbindungGeaendert,
                this, [this](bool) { versatzNachziehen(); });
    }
}

void FrequencyApplet::bindSlice(SliceModel* slice)
{
    if (m_slice == slice) { return; }
    if (m_slice) { disconnect(m_slice, nullptr, this, nullptr); }
    m_slice = slice;
    if (!m_slice) { return; }

    connect(slice, &SliceModel::frequencyChanged, this, [this](double hz) {
        m_instrument->setFrequency(hz);
    });
    // RIT/XIT: zwei Quellen, ein Schild. Gestellt werden sie im
    // RX-Applet (eigener Empfaenger) oder im CAT-Applet bzw. am Geraet
    // selbst (TS-590) -- hier werden sie nur sichtbar, damit ein
    // eingeschalteter Versatz auch dem auffaellt, der keines der beiden
    // Applets offen hat. Welche Quelle gilt, entscheidet
    // ritXitFuerDieZeile() (core/RitXitAnzeige.h).
    connect(slice, &SliceModel::ritEnabledChanged, this,
            &FrequencyApplet::versatzNachziehen);
    connect(slice, &SliceModel::ritHzChanged,      this,
            &FrequencyApplet::versatzNachziehen);
    connect(slice, &SliceModel::xitEnabledChanged, this,
            &FrequencyApplet::versatzNachziehen);
    connect(slice, &SliceModel::xitHzChanged,      this,
            &FrequencyApplet::versatzNachziehen);

    m_instrument->setFrequency(slice->frequency());
    versatzNachziehen();
    syncFromModel();
}

void FrequencyApplet::syncFromModel()
{
    if (!m_model || !m_slice) { return; }
    // Die zweite Scheibe fuer die VFO-Zeile. Gibt es keine, steht dort
    // dieselbe Zahl zweimal — das ist ehrlicher als eine erfundene B.
    SliceModel* other = nullptr;
    for (SliceModel* s : m_model->slices()) {
        if (s && s != m_slice) { other = s; break; }
    }
    m_instrument->setOtherFrequency(other ? other->frequency()
                                          : m_slice->frequency());
    m_instrument->setActiveIsThis(true);
    versatzNachziehen();
}

void FrequencyApplet::versatzNachziehen()
{
    if (!m_slice) { return; }
    auto* rig = m_model ? m_model->companionRig() : nullptr;
    // Ein eingestellter, aber toter CAT-Weg darf die Scheibe nicht
    // verdraengen -- darum catBetrieb() UND verbunden().
    const bool catLaeuft = rig != nullptr && rig->catBetrieb() && rig->verbunden();
    const Ts590Cat::Zustand leer{};
    const RitXitAnzeige a = ritXitFuerDieZeile(
        catLaeuft, rig ? rig->catZustand() : leer,
        m_slice->ritEnabled(), m_slice->ritHz(),
        m_slice->xitEnabled(), m_slice->xitHz());
    m_instrument->setRitXit(a.ritAn, a.ritHz, a.xitAn, a.xitHz);
}



void FrequencyApplet::setShowTiles(bool on)
{
    if (m_showTiles == on) { return; }
    m_showTiles = on;
    applyRowVisibility();
    saveState();
}

void FrequencyApplet::applyRowVisibility()
{
    if (m_tiles) { m_tiles->setVisible(m_showTiles); }
    if (m_powerBar)  { m_powerBar->setVisible(m_showPower); }
    if (m_swrBar)    { m_swrBar->setVisible(m_showSwr); }
    if (m_signalBar) { m_signalBar->setVisible(m_showSignal); }
    if (m_instrument) { m_instrument->setVfoRowVisible(m_showVfo); }
}

void FrequencyApplet::setShowVfo(bool on)
{
    if (m_showVfo == on) { return; }
    m_showVfo = on;
    applyRowVisibility();
    saveState();
}

void FrequencyApplet::setShowPower(bool on)
{
    if (m_showPower == on) { return; }
    m_showPower = on;
    applyRowVisibility();
    saveState();
}

void FrequencyApplet::setShowSwr(bool on)
{
    if (m_showSwr == on) { return; }
    m_showSwr = on;
    applyRowVisibility();
    saveState();
}

void FrequencyApplet::setShowSignal(bool on)
{
    if (m_showSignal == on) { return; }
    m_showSignal = on;
    applyRowVisibility();
    saveState();
}

void FrequencyApplet::onReading(int bindingId, double value)
{
    // Alle drei bekommen jeden Wert; BarInstrument sortiert selbst aus,
    // ob die Kennung seine ist. Auch die unsichtbare Zeile wird
    // gefuettert — sonst zeigte sie beim Einschalten erst einmal
    // nichts, und der Betreiber haette einen halben Meterzyklus lang
    // ein leeres Feld.
    if (m_powerBar)  { m_powerBar->onReading(bindingId, value); }
    if (m_swrBar)    { m_swrBar->onReading(bindingId, value); }
    if (m_signalBar) { m_signalBar->onReading(bindingId, value); }
}

namespace {
QString tilesKey()  { return QStringLiteral("FrequencyApplet_ShowTiles"); }
QString powerKey()  { return QStringLiteral("FrequencyApplet_ShowPower"); }
QString swrKey()    { return QStringLiteral("FrequencyApplet_ShowSwr"); }
QString signalKey() { return QStringLiteral("FrequencyApplet_ShowSignal"); }
QString vfoKey()    { return QStringLiteral("FrequencyApplet_ShowVfo"); }
} // namespace

void FrequencyApplet::saveState() const
{
    auto& st = AppSettings::instance();
    st.setValue(tilesKey(), m_showTiles ? QStringLiteral("True")
                                        : QStringLiteral("False"));
    st.setValue(powerKey(), m_showPower ? QStringLiteral("True")
                                        : QStringLiteral("False"));
    st.setValue(swrKey(), m_showSwr ? QStringLiteral("True")
                                    : QStringLiteral("False"));
    st.setValue(signalKey(), m_showSignal ? QStringLiteral("True")
                                          : QStringLiteral("False"));
    st.setValue(vfoKey(), m_showVfo ? QStringLiteral("True")
                                    : QStringLiteral("False"));
}

void FrequencyApplet::restoreState()
{
    auto& st = AppSettings::instance();
    m_showTiles = st.value(tilesKey(), QStringLiteral("True")).toString()
                  == QStringLiteral("True");
    m_showPower = st.value(powerKey(), QStringLiteral("False")).toString()
                  == QStringLiteral("True");
    m_showSwr = st.value(swrKey(), QStringLiteral("False")).toString()
                == QStringLiteral("True");
    m_showSignal = st.value(signalKey(), QStringLiteral("False")).toString()
                   == QStringLiteral("True");
    m_showVfo = st.value(vfoKey(), QStringLiteral("True")).toString()
                == QStringLiteral("True");
    applyRowVisibility();
}


void FrequencyApplet::toggleKiwiDisplay()
{
    // Der Weg zum Panadapter fuehrt ueber die Scheibe. Er ist bewusst
    // NICHT abgekuerzt: nimmt man den aktiven Panadapter, schaltet die
    // Kachel bei mehreren Empfaengern den falschen um — und zwar
    // stillschweigend, denn beide sehen gleich aus.
    if (!m_slice) { return; }
    auto* mw = qobject_cast<QWidget*>(window());
    Q_UNUSED(mw);

    // Das Applet kennt den Panadapter nicht selbst. Statt hier eine
    // Abhaengigkeit auf MainWindow aufzubauen, wird das Ereignis nach
    // aussen gemeldet — MainWindow verdrahtet es dort, wo auch die
    // uebrige Panadapter-Zuordnung liegt.
    emit kiwiDisplayToggleRequested(m_slice.data());
}

QMenu* FrequencyApplet::baueOptionsMenue(QWidget* parent)
{
    // Eine Quelle fuer beide Zugaenge — das Zahnrad im Panelkopf
    // (Martins Regel vom 2026-09-20) und die rechte Maustaste. Vorher
    // stand dieses Menue NUR hinter der rechten Maustaste, und
    // `hasExtendedSettings()` war der Vorgabewert false: der Knopf im
    // Kopf wurde also nie gezeichnet (`GridCellWidget.cpp:235`).
    auto* menu = new QMenu(parent);


    // Haken, keine Punkte: die beiden Zeilen sind unabhaengig. Ein
    // Auswahlkranz aus "keine / Stehwelle / SWR / beide" waere
    // dieselbe Information in vier Zeilen statt zwei und liesse sich
    // schlechter erweitern.
    QAction* tiles = menu->addAction(tr("Show receiver tiles"));
    tiles->setCheckable(true);
    tiles->setChecked(m_showTiles);
    connect(tiles, &QAction::triggered, this,
            [this](bool on) { setShowTiles(on); });

    // Betreiber 2026-08-30: "A und B sollte man im Frequenzfeld auch
    // anhacken können" -- dieselbe Ein-/Ausblenden-Idee wie die beiden
    // Zusatzzeilen unten, nur fuer die VFO-Zeile ("A ... B ...").
    QAction* vfo = menu->addAction(tr("Show A/B row"));
    vfo->setCheckable(true);
    vfo->setChecked(m_showVfo);
    connect(vfo, &QAction::triggered, this,
            [this](bool on) { setShowVfo(on); });
    menu->addSeparator();

    // War "Stehwelle anzeigen" -- falsch beschriftet, obwohl an
    // setShowPower() haengend. "Stehwelle" und "SWR" sind dasselbe
    // Wort (derselbe Befund traf am 2026-08-23 schon das inzwischen
    // entfernte eigene SWR/Leistung-Applet); mit BEIDEN Zeilen im
    // selben Menue las sich das wie zwei
    // SWR-Schalter, von denen einer in Wahrheit die Leistung meinte.
    // Betreiber 2026-08-30 stiess genau darauf ("brauche die Zeile
    // darunter nicht, soll als Option sein" -- die Option gab es
    // schon, nur unter dem falschen Namen versteckt).
    QAction* pwr = menu->addAction(tr("Show power"));
    pwr->setCheckable(true);
    pwr->setChecked(m_showPower);
    connect(pwr, &QAction::triggered, this,
            [this](bool on) { setShowPower(on); });

    // Betreiber 2026-08-30: "Stehwelle bitte auch noch als Option" --
    // "SWR" und "Stehwelle" sind dasselbe Wort (siehe Begruendung oben
    // bei "Show power"); die Beschriftung traegt seither beide,
    // damit niemand mehr danach sucht, ohne es zu finden.
    QAction* swr = menu->addAction(tr("Show SWR"));
    swr->setCheckable(true);
    swr->setChecked(m_showSwr);
    connect(swr, &QAction::triggered, this,
            [this](bool on) { setShowSwr(on); });

    // Betreiber 2026-08-30: "S-Meter bitte noch bei Frequenz
    // hinzufügen" -- dieselbe Zusatzzeile wie Leistung/SWR.
    QAction* sig = menu->addAction(tr("Show S-meter"));
    sig->setCheckable(true);
    sig->setChecked(m_showSignal);
    connect(sig, &QAction::triggered, this,
            [this](bool on) { setShowSignal(on); });

    // Betreiber 2026-08-30: "die Zusatzzeilen bitte weg" -- Segmente
    // und Roehre (3D) galten fuer die alte Mulden-Zeichnung; die
    // knappe Form (BarInstrument::setCompact) zeichnet seit heute eine
    // Haarlinie ohne Mulde und liest m_segmented/m_tube gar nicht mehr.
    // Ein Menuepunkt fuer etwas, das nichts mehr aendert, ist eine
    // Falle -- weg statt weiter anzubieten.
    return menu;
}

void FrequencyApplet::openExtendedSettings()
{
    // Dieselbe Hand wie InstrumentApplet::openExtendedSettings: am
    // Fenster haengen (ein nativer Vorfahr gab dem Menue sonst keinen
    // Fensterbezug, 2026-09-30), und ueberleben, falls das Fenster
    // waehrend exec() stirbt.
    QMenu* menu = baueOptionsMenue(window());
    QPointer<QMenu> lebtNoch(menu);
    menu->exec(mapToGlobal(rect().center()));
    if (lebtNoch) { lebtNoch->deleteLater(); }
}

void FrequencyApplet::contextMenuEvent(QContextMenuEvent* ev)
{
    QMenu* menu = baueOptionsMenue(window());   // am Fenster, siehe oben
    QPointer<QMenu> lebtNoch(menu);
    const QPointer<FrequencyApplet> self(this);
    menu->exec(ev->globalPos());
    if (lebtNoch) { lebtNoch->deleteLater(); }
    // Das Elternteil kann waehrend exec() gestorben sein — dann ist
    // `this` eine Leiche. Siehe ScopedChildWidget.h.
    if (!self) { return; }
    ev->accept();
}

} // namespace Longpath
