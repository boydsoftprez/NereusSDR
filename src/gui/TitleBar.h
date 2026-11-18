#pragma once

#include "core/RadioLinkKind.h"

// =================================================================
// src/gui/TitleBar.h  (Longpath)
// =================================================================
//
// Ported from AetherSDR source:
//   src/gui/TitleBar.h
//   src/gui/TitleBar.cpp
//
// AetherSDR is licensed under the GNU General Public License v3; see
// https://github.com/ten9876/AetherSDR for the contributor list and
// project-level LICENSE. Longpath is also GPLv3. AetherSDR source
// files carry no per-file GPL header; attribution is at project level
// per docs/attribution/HOW-TO-PORT.md rule 6.
//
// Upstream reference: AetherSDR v0.8.16 (2026-04).
//
// =================================================================
// Modification history (Longpath):
//   2026-04-20 — Ported/adapted in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Phase 3O Sub-Phase 10 Task 10c.
//                 Scoped-down port: master-output strip only. AetherSDR's
//                 heartbeat / multiFLEX / PC-audio / headphone / minimal-
//                 mode / feature-request widgets are intentionally omitted
//                 (deferred to separate Longpath phases — 3G-14 plans the
//                 💡 feature-request widget; headphone devices land in
//                 Sub-Phase 12 Setup → Audio → Devices; connection-state
//                 UI is already served elsewhere).
//                 Hosts `MasterOutputWidget` (Task 10b) on the right edge;
//                 `setMenuBar()` pattern copied from AetherSDR
//                 `src/gui/TitleBar.cpp:282-295` (re-style QMenuBar,
//                 `m_hbox->insertWidget(0, mb)`). Strip geometry and
//                 background (#0a0a14, 32 px, `border-bottom: 1px solid
//                 #203040`) follow AetherSDR `TitleBar.cpp:30-31`. App-
//                 name label colour and font follow AetherSDR
//                 `TitleBar.cpp:101-103` with the literal "AetherSDR"
//                 replaced by "Longpath".
//                 Design spec: docs/architecture/2026-04-19-vax-design.md
//                 §6.3 + §7.3.
//   2026-04-20 — Task 10d: consolidated the 💡 feature-request button
//                 (previously hosted by a separate `featureBar` QToolBar
//                 in MainWindow) into TitleBar's far-right slot, past the
//                 MasterOutputWidget. Emits `featureRequestClicked()`;
//                 MainWindow wires that to its existing
//                 showFeatureRequestDialog slot. Matches AetherSDR's
//                 pattern of the feature button as the rightmost element.
//   2026-04-27 — Phase 3Q-6: added ConnectionSegment — always-visible
//                 connection state widget (dot + radio name/IP + Mbps +
//                 activity LED). Inserted between menu bar and the
//                 centre stretch. Design §4.1.
//   2026-04-30 — Phase 3Q Sub-PR-4 D.1: rewrote ConnectionSegment per
//                 shell-chrome redesign spec §4.1. New form: state-encoding
//                 dot (color=state, pulse=activity), ▲ Mbps, RTT readout
//                 (clickable, color-thresholded), ▼ Mbps, ♪ audio pip.
//                 Removed setRadio()/name+IP API (STATION block carries
//                 radio identity, sub-PR-7). Removed activity LED (state dot
//                 encodes activity via pulse). Added signals: rttClicked(),
//                 audioPipClicked(), contextMenuRequested(QPoint).
//   2026-08-02 — Task A7 (bottom-banner cleanup): added a single-row UTC
//                 clock (`utcText()`) past the MasterOutputWidget. Replaces
//                 the two-row UTC+date/local clock deleted from the bottom
//                 banner in the same commit; the date/local row is dropped
//                 because local time already sits in the menu bar and the
//                 date is not an operator-facing fact worth the pixels.
//                 Longpath-original UI, not a Thetis/AetherSDR port.
// =================================================================

#include "core/AudioEngine.h"
#include "core/ConnectionState.h"

#include <QQueue>
#include <QTimer>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QMenuBar;
class QPushButton;

namespace Longpath {

class MasterOutputWidget;

// ConnectionSegment — always-visible connection-state indicator living
// in the TitleBar, between the menu bar and the centre "Longpath" label.
//
// Visual layout (left → right within the segment):
//   [state dot]  [▲ Mbps]  [RTT ms (color-coded, clickable)]  [▼ Mbps]  [|]  [♪ pip]
//   — or when Disconnected:
//   [state dot]  ["Disconnected — click to connect"]
//
// State dot color encodes connection state; pulse animation encodes
// radio/streaming activity. No separate activity LED.
//
// RTT readout: color-coded green/yellow/red (<50/<150/≥150 ms). Left-click
// opens NetworkDiagnosticsDialog. Disconnected-state click anywhere does
// the same.
//
// ♪ pip: audio pipeline health. Color: blue=Healthy, yellow=Underrun,
// red=Stalled, dim=Dead.
//
// Phase 3Q Sub-PR-4 D.1.
class ConnectionSegment : public QWidget {
    Q_OBJECT

public:
    explicit ConnectionSegment(QWidget* parent = nullptr);

    void setState(ConnectionState s);
    void setRates(double rxMbps, double txMbps);
    void setRttMs(int ms);

    /// Paketverlust im I/Q-Strom (Prozent des jeweils letzten
    /// 5-Sekunden-Fensters). Negativ = unbekannt, dann wird nichts
    /// gezeigt.
    // ── Woran haengt das Geraet? (2026-08-23) ───────────────────────
    //
    // Steht neben der Verlustanzeige, weil es genau deren Frage
    // beantwortet. Der Betreiber sah 0,17 % bis 2,87 % Verlust; die
    // Messung ergab, dass der Kern KEIN Paket wegen vollen Puffers
    // verworfen hat (netstat: "0 dropped due to full socket buffers")
    // und der Weg zum Geraet ueber WLAN laeuft.
    //
    // Ohne diesen Hinweis sucht man den Fehler im Programm — so, wie
    // ich ihn eine Stunde lang gesucht habe.
    void setRadioLinkKind(Longpath::RadioLinkKind kind);

    void setPacketLoss(double lossPercent);
    void setAudioFlowState(AudioEngine::FlowState s);

    ConnectionState          state() const noexcept { return m_state; }
    int                      rttMs() const noexcept { return m_rttMs; }
    AudioEngine::FlowState   audioFlowState() const noexcept { return m_audioFlow; }

    // Smoothed RTT — 2nd-smallest of the rolling N-sample window
    // (min-filter approximation of true network round-trip time). See
    // smoothedRttMs() impl for the rationale. Returns the latest sample
    // if only one sample has been seen, or -1 if no samples at all.
    int                      smoothedRttMs() const noexcept;

    // Hit-test rect accessors — populated after the first paintEvent.
    // Exposed publicly so callers can verify click regions (e.g. in tests).
    QRect rttRect() const;
    QRect audioPipRect() const;

public slots:
    /// Empfaengt RadioConnection::iqPacketLoss.
    ///
    /// Eigener Slot statt Lambda, und das ist kein Geschmack: mit einem
    /// Lambda ist Qt::UniqueConnection WIRKUNGSLOS — Qt braucht dafuer
    /// einen Zeiger auf eine Member-Funktion. Am 2026-08-22 kam die
    /// Verbindung deshalb gar nicht zustande, die Anzeige blieb leer,
    /// und beim Betreiber ruckte der Ton weiter unerklaert.
    void onIqPacketLoss(double lossPercent, quint32 lost, quint32 received);

    // Throttled activity tick — nudges a repaint so the pulse looks "live".
    // The pulse timer in the ctor already drives the animation; this slot
    // exists for future per-frame visual cues.
    void frameTick();

signals:
    void rttClicked();
    void audioPipClicked();
    void contextMenuRequested(const QPoint& globalPos);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QColor stateDotColor() const;
    QColor rttColor(int rttMs) const;
    QColor audioPipColor(AudioEngine::FlowState s) const;

    ConnectionState         m_state{ConnectionState::Disconnected};
    double                  m_rxMbps{0.0};
    double                  m_txMbps{0.0};
    int                     m_rttMs{-1};
    double                  m_lossPct{-1.0};
    Longpath::RadioLinkKind m_linkKind{Longpath::RadioLinkKind::Unknown};
    /// Der schlimmste Wert der letzten Minute. Ein Verlust von 0,8 %
    /// verschwindet nach fuenf Sekunden wieder — und genau der ist es,
    /// der das Rucken macht. Die Anzeige haelt ihn fest, sonst schaut
    /// der Bediener immer in dem Moment hin, in dem es gerade gut geht.
    double                  m_lossWorstPct{-1.0};
    qint64                  m_lossWorstMs{0};
    AudioEngine::FlowState  m_audioFlow{AudioEngine::FlowState::Dead};
    QTimer                  m_pulseTimer;
    bool                    m_pulseOn{false};

    // RTT smoothing — rolling window of the last N samples. setRttMs()
    // pushes; the painter and color-threshold both consume a min-filter
    // of the window via smoothedRttMs() (2nd-smallest). The min-filter
    // approximates true round-trip time despite the radio's periodic
    // status-packet cadence adding bracket noise to each raw sample;
    // a window of 10 gives us ~10 s of history at the 1 Hz sample rate,
    // long enough to capture a "lucky" sample where the cadence noise
    // happened to be near zero.
    static constexpr int kRttSmoothingWindow = 10;
    QQueue<int>           m_rttSamples;

    // Stored during paintEvent so mousePressEvent can hit-test without
    // re-computing geometry. Mutable because they are written inside const-
    // eligible paint logic; paintEvent itself is not const in Qt.
    mutable int m_lastRttX1{0};
    mutable int m_lastRttX2{0};
    mutable int m_lastPipX1{0};
    mutable int m_lastPipX2{0};
};

// TitleBar — thin 32 px host strip at the top of the main window.
//
// Layout (left → right):
//   [menuBar, inserted at position 0 via setMenuBar()]
//   [ConnectionSegment — state dot + radio info + activity LED]
//   [stretch]
//   [Longpath app-name label]
//   [stretch]
//   [MasterOutputWidget — speaker button + master slider + readout]
//   [6 px spacing]
//   [💡 feature-request button]
//
// macOS caveat: embedding the QMenuBar inside a custom widget prevents Qt
// from promoting it to the native global menu bar at the top of the
// screen. The menus render in-window instead. This is the explicit design
// choice per user-approved Sub-Phase 10 decision (option D); the
// MasterOutputWidget master-volume controls MUST be visible at all times
// alongside the menus, and the native global bar cannot host them.
class TitleBar : public QWidget {
    Q_OBJECT

public:
    // Der Stand gegenüber der neuesten veröffentlichten Fassung,
    // neben der Version. Begründung an der Stelle, an der das Feld
    // gebaut wird (TitleBar.cpp). Heisst bewusst anders als
    // ConnectionSegment::UpdateState -- zwei gleichnamige
    // Aufzaehlungen in einer Datei sind eine Falle.
    enum class AktualisierungsStand { Unbekannt, Aktuell, Neuer };
    // `grund` landet im Tooltip -- bei „not checked" steht dort,
    // warum die Abfrage nicht durchkam. Leer laesst den Tooltip weg.
    void setUpdateState(const QString& text, AktualisierungsStand art,
                        const QString& grund = QString());

    // Platz links, damit die drei Fensterknoepfe von macOS nicht auf
    // der Menuezeile liegen. Siehe gui/MacVollflaeche.h.
    void setLinkerRand(int punkte);

    explicit TitleBar(AudioEngine* audio, QWidget* parent = nullptr);

    // Re-style `mb` and insert it at position 0 of the hbox layout.
    // Pattern from AetherSDR TitleBar.cpp:282. Caller must ensure `mb`
    // is fully populated with actions BEFORE invoking this — otherwise
    // the menu bar's size policy negotiation happens on an empty widget.
    void setMenuBar(QMenuBar* mb);

    /// Die „»"-Taste der Menueleiste in das Orange des Plus setzen.
    ///
    /// Betreiber, 2026-11-17: „die beiden kleinen pfeile oben in der
    /// taskleiste neben containers bitte in farbe machen. die sieht
    /// keiner." Die zwei Pfeile sind Qts eigene Taste fuer die Menues,
    /// die in der Leiste keinen Platz mehr haben (bei ihm alles hinter
    /// „Containers": Tools, Help …) -- ein graues Zeichen auf dunklem
    /// Grund, das wie ein Trenner aussieht und doch der einzige Weg zu
    /// diesen Menues ist.
    ///
    /// Eigene, oeffentliche Funktion, damit ein Pruefstand sie an einer
    /// blossen QMenuBar fahren kann. Findet sie keine solche Taste,
    /// geschieht nichts.
    static void faerbeMenueErweiterung(QMenuBar* mb);

    // Non-owning accessor so MainWindow can wire the device-picker
    // signal into AudioEngine::setSpeakersConfig.
    MasterOutputWidget* masterOutput() const { return m_master; }

    // Non-owning accessor so MainWindow can wire connection-state signals
    // and the activity LED. Phase 3Q-6.
    ConnectionSegment* connectionSegment() const { return m_connectionSegment; }

    /// Single-row UTC, "23:59:59 UTC". Time lives where every OS puts it,
    /// which also frees the banner's bottom-right corner for alarms alone.
    QString utcText() const;

signals:
    // Emitted when the 💡 feature-request button is clicked. MainWindow
    // connects this to its showFeatureRequestDialog slot.
    void featureRequestClicked();

private:
    QHBoxLayout*        m_hbox{nullptr};
    QMenuBar*           m_menuBar{nullptr};
    ConnectionSegment*  m_connectionSegment{nullptr};
    MasterOutputWidget* m_master{nullptr};
    QPushButton*        m_featureBtn{nullptr};
    // Der Stand der Aktualisierung, neben der Version.
    QLabel*             m_updateState{nullptr};
    QLabel*             m_utcLabel{nullptr};
    QTimer*             m_utcTimer{nullptr};
};

} // namespace Longpath
