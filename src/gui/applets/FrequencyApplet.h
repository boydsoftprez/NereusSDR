#pragma once

// =================================================================
// src/gui/applets/FrequencyApplet.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Die Fassung um FrequencyInstrument — dieselbe Rolle, die
// InstrumentApplet fuer Zeiger und Balken spielt. Getrennt davon, weil
// die Frequenz keine Messgroesse aus ReadingSource ist, sondern der
// Zustand einer Scheibe: sie hat keine Skala, keine Schwelle und keine
// Spitze, dafuer eine Bedienung.
//
// Der Umschalter der drei Varianten gehoert in den Panelkopf (Punkt 5
// der abgesprochenen Reihenfolge) und ist hier nicht gebaut.
// setForm() ist die Stelle, an der er andocken wird.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
//   2026-10-31 — Die Anzeigeschalter auch ins Zahnrad des Panelkopfs.
// =================================================================

#include "gui/applets/AppletWidget.h"
#include "gui/instruments/BarInstrument.h"
#include "gui/instruments/FrequencyInstrument.h"
#include "gui/widgets/VfoTileRow.h"

class QMenu;

namespace Longpath {

class SliceModel;

class FrequencyApplet : public AppletWidget {
    Q_OBJECT

public:
    explicit FrequencyApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId() const override    { return QStringLiteral("Frequency"); }
    QString appletTitle() const override { return tr("Frequency"); }
    void    syncFromModel() override;

    void setForm(FrequencyInstrument::Form f) { m_instrument->setForm(f); }
    FrequencyInstrument* instrument() const { return m_instrument; }

    /// Die Scheibe, deren Frequenz gezeigt und gestellt wird.
    void bindSlice(SliceModel* slice);

    // ── Zusatzbalken unter der Frequenz ──────────────────────────────
    //
    // Der Betreiber am 2026-08-23: "optional sollte man auch beim
    // widget frequenz den unteren balken als stehwelle auswählen
    // können, und auch als swr dazu, sprich frequenz widget als option
    // mit stehwelle und auch swr zusätzlich."
    //
    // Der Gedanke dahinter ist Platz: wer am Notebook sitzt, will
    // Frequenz, Stehwelle und SWR sehen, ohne dafuer drei Applets
    // uebereinanderzustapeln — jedes mit eigener Titelleiste und
    // eigenem Rand. Ein Widget mit zwei angehaengten Zeilen kostet
    // deutlich weniger Hoehe als drei Widgets.
    //
    // Beide Zeilen sind EINZELN schaltbar, nicht als Paar: wer nur das
    // SWR will, soll nicht die Leistung mitnehmen muessen.
    /// Die Kachelreihe ueber der Ziffernanzeige, nach der Vorlage des
    /// Betreibers vom 2026-08-23 (Bildschirmfoto der Vorlage).
    void setShowTiles(bool on);
    bool showsTiles() const { return m_showTiles; }

    /// Zustand der KIWI-Kachel. Setzt MainWindow, weil nur es den
    /// Panadapter kennt.
    void setKiwiState(bool on) { if (m_tiles) { m_tiles->setKiwiOn(on); } }

    void setShowPower(bool on);
    void setShowSwr(bool on);
    bool showsPower() const { return m_showPower; }
    bool showsSwr() const   { return m_showSwr; }

    /// Betreiber 2026-08-30: "S-Meter bitte noch bei Frequenz
    /// hinzufügen" -- dieselbe Zusatzzeile wie Leistung/SWR, nur mit
    /// derselben Quelle wie die eigenstaendige S-Meter-Anzeige
    /// (MeterBinding::SignalAvg).
    void setShowSignal(bool on);
    bool showsSignal() const { return m_showSignal; }

    /// Betreiber 2026-08-30: "A und B sollte man im Frequenzfeld auch
    /// anhacken können" -- dieselbe Idee wie showsPower()/showsSwr(),
    /// nur fuer FrequencyInstrument::m_vfoRow (die "A ... B ..."-Zeile).
    void setShowVfo(bool on);
    bool showsVfo() const { return m_showVfo; }

    /// Messwerte durchreichen — dieselbe Rolle wie
    /// InstrumentApplet::onReading, damit MainWindow beide gleich
    /// bedienen kann.
    void onReading(int bindingId, double value);

    void saveState() const;
    void restoreState();

    // ── Die Optionen gehoeren ins Zahnrad (2026-10-31) ───────────────
    //
    // Betreiber: „frequency widget bitte mit option ausstatten, was
    // alles genau angezeigt werden soll. entweder nur frequenz, oder
    // mit swr, oder mit leistung usw."
    //
    // Fuenf Schalter gab es dafuer schon (Kacheln, A/B-Zeile,
    // Leistung, SWR, S-Meter), seit dem 2026-08-23 und dem
    // 2026-08-30 — aber NUR hinter der rechten Maustaste. Martins
    // Regel vom 2026-09-20 lautet: Optionen stehen rechts oben im
    // Panelkopf, hinter dem Zahnrad. `GridCellWidget` zeigt dieses
    // Zahnrad genau dann, wenn `hasExtendedSettings()` wahr ist
    // (`GridCellWidget.cpp:235`) — hier stand der Vorgabewert false,
    // und damit war der Knopf nie da. Wer nicht weiss, dass man
    // rechts klicken muss, findet nichts; er hat es nicht gefunden.
    //
    // Darum dasselbe Menue an BEIDEN Stellen, aus einer Quelle:
    // `baueOptionsMenue()`. Die rechte Maustaste bleibt, sie stoert
    // niemanden.
    bool hasExtendedSettings() const override { return true; }
    void openExtendedSettings() override;

protected:
    void contextMenuEvent(QContextMenuEvent* ev) override;

signals:
    /// Weitergereicht an MainWindow, das als einziges weiss, welcher
    /// Panadapter zu dieser Scheibe gehoert.
    void kiwiDisplayToggleRequested(SliceModel* slice);

private slots:
    /// Die KIWI-Kachel wurde gedrueckt. Das Applet kennt seine Scheibe
    /// und ueber sie den Panadapter — die Kachelreihe kennt beides
    /// nicht und soll es auch nicht.
    void toggleKiwiDisplay();

private:
    void applyRowVisibility();
    /// Holt den Versatz aus der gueltigen Quelle (Scheibe oder Geraet)
    /// und schreibt ihn an die VFO-Zeile. Siehe core/RitXitAnzeige.h.
    void versatzNachziehen();
    /// Das Menue der Anzeigeschalter. Eine Quelle fuer das Zahnrad im
    /// Panelkopf und fuer die rechte Maustaste.
    QMenu* baueOptionsMenue(QWidget* parent);

    VfoTileRow*          m_tiles{nullptr};
    FrequencyInstrument* m_instrument{nullptr};
    BarInstrument*       m_powerBar{nullptr};
    BarInstrument*       m_swrBar{nullptr};
    BarInstrument*       m_signalBar{nullptr};
    bool                 m_showTiles{true};
    bool                 m_showPower{false};
    bool                 m_showSwr{false};
    bool                 m_showSignal{false};
    bool                 m_showVfo{true};
    QPointer<SliceModel> m_slice;
};

} // namespace Longpath
