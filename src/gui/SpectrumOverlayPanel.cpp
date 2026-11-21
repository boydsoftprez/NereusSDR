// src/gui/SpectrumOverlayPanel.cpp
// Left overlay button strip ported from AetherSDR SpectrumOverlayMenu.
// 8 buttons, 4 flyout sub-panels, auto-close on outside click.
//
// Ported from AetherSDR src/gui/SpectrumOverlayMenu.cpp
// Adapted for Longpath OpenHPSDR/Thetis feature set.

// =================================================================
// src/gui/SpectrumOverlayPanel.cpp  (Longpath)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   Longpath is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (Longpath):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Ported from AetherSDR `src/gui/SpectrumOverlayMenu.{h,cpp}`
//                 (left button strip + 5 flyout panels).
//   2026-04-20 — Phase 3O Sub-Phase 9 Task 9.2c (issue #70 fold-in):
//                 added setRadioModel() so the previously-disabled VAX Ch
//                 combo on the left-edge overlay is now wired bidirectionally
//                 to the resolved pan slice's vaxChannel() with echo prevention. IQ Ch
//                 stays feature-flagged off (design spec §6.7/§11.3 —
//                 audio/SendIqToVax stored-but-not-active). J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

#include "SpectrumOverlayPanel.h"
#include "gui/ScopedChildWidget.h"

#include "StyleConstants.h"
#include "gui/styles/ThemeQss.h"    // Style::role — Kurvenfarbe aus dem Theme
#include "core/AntennaLabels.h"
#include "core/BoardCapabilities.h"
#include "core/SkuUiProfile.h"
#include "gui/AntennaPopupBuilder.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include "gui/SpectrumWidget.h"
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QColorDialog>
#include <QColor>
#include <QSignalBlocker>
#include <algorithm>
#include <utility>

namespace Longpath {

// ── Constants (from AetherSDR SpectrumOverlayMenu.cpp) ───────────────────────
// ── Maße aus docs/design/HAUSSTIL.md §Maße ──────────────────────────
//
//   Pille          Höhe 26–28 px, Radius 6, Polsterung 0 11 px
//   Knopfabstand    5 px innerhalb einer Gruppe
//   Gruppenabstand 22–26 px
//   Nie Radius 3.  Das ist der Qt-Standardwert und lässt alles nach
//                  Qt aussehen.
//
// Die Spalte stand vorher auf 68 × 22 mit Radius 2 und 2 px Abstand —
// eng, kantig und ohne jede Gliederung. Die Breite muss mit: bei
// Polsterung 0 11 px braucht „Display" in 11 px fett rund 70 px, und
// 68 hätte den Text beschnitten.
static constexpr int kBtnW     = 80;
static constexpr int kBtnH     = 27;

// Höhe einer Versalzeile samt ihrem Luftraum nach unten.
static constexpr int kHeadH    = 13;
// Abstand zwischen zwei Gruppen, gemessen über der Versalzeile.
static constexpr int kGroupGap = 24;
static constexpr int kBandBtnW = 48;
static constexpr int kBandBtnH = 26;
static constexpr int kPad      = 2;
static constexpr int kGap      = 5;   // innerhalb einer Gruppe

// Translucent overlay colours — intentionally NOT in StyleConstants.h because
// they are designed to alpha-blend over the changing spectrum background (the
// translucency is inherent to their purpose, not incidental). All opaque
// panel/button colours have been consolidated to Style:: canonical helpers.
// Per docs/architecture/ui-audit-polish-plan.md §A2 — "tightly-scoped local
// blocks for legitimate exceptions."
namespace OverlayColors {
    // Flyout panel backgrounds (verbatim from AetherSDR SpectrumOverlayMenu.cpp)
    constexpr auto kPanelStyle =
        "QWidget { background: rgba(15, 15, 26, 220); "
        "border: 1px solid #304050; border-radius: 6px; }"
        // Betreiber 2026-09-08 (Scheme-Combo im Display-Flyout): der
        // gerade ausgewaehlte Eintrag im aufgeklappten Dropdown war
        // praktisch unsichtbar. Ursache: die QWidget-Regel oben faerbt
        // auch die Zeilen der Popup-Liste ein (Qt kaskadiert Stylesheets
        // ueber die QObject-Elternschaft bis in QComboBox::view()
        // hinein), aber OHNE eigene QComboBox/QAbstractItemView-Regel
        // blieb Vordergrund/Auswahlfarbe dem Systemstandard ueberlassen
        // -- der klirrt gegen den erzwungenen dunklen Grund. Alle drei
        // Zustaende (normal/gehovert/ausgewaehlt) jetzt explizit, statt
        // nur den Grund zu setzen und den Rest zu raten.
        "QComboBox { background: #1a2a3a; color: #c8d8e8; "
        "border: 1px solid #304050; border-radius: 6px; padding: 2px 4px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #1a2a3a; color: #c8d8e8; "
        "selection-background-color: #4a7ba8; selection-color: #ffffff; "
        "outline: none; }"
        "QComboBox QAbstractItemView::item { color: #c8d8e8; }"
        "QComboBox QAbstractItemView::item:selected { "
        "background: #4a7ba8; color: #ffffff; }";

    // Label style for rows inside translucent flyout panels.
    // Transparent background is required here because the label must not
    // occlude the panel's own rgba layer. Color (#8aa8c0 = Style::kTitleText)
    // has a canonical equivalent but the full rule (transparent bg, no border,
    // 10 px bold) does not — keeping it co-located with the other overlay rules.
    constexpr auto kLabelStyle =
        "QLabel { background: transparent; border: none; "
        "color: #8aa8c0; font-size: 11px; font-weight: bold; }";

    // Menu strip buttons — fully transparent/semi-transparent fills that
    // blend with the spectrum below.
    // ── Genau ein Knopf je Gruppe leuchtet ──────────────────────────
    //
    // docs/design/HAUSSTIL.md Regel 2: „Aktiv gefüllt, inaktiv fast
    // unsichtbar." Vorher war es umgekehrt gewichtet — jeder Knopf trug
    // eine deckende Fläche (rgba(20,30,45,240)) und einen hellen Rahmen,
    // acht davon untereinander, und der aktive unterschied sich nur
    // durch ein kräftiges Blau. Acht gleich laute Flächen sagen nichts
    // darüber, welche gerade gilt.
    //
    // Jetzt: inaktiv fast nur Text, aktiv der Auswahl-Verlauf.
    // Radius 6, nie 3.
    // ── Ein Knopf LIEGT auf der Flaeche ─────────────────────────────
    //
    // Hier stand ein fast durchsichtiger Grund (8 % Weiss) mit
    // Beschriftung auf #8e8e93 -- gedacht als HAUSSTIL Regel 2,
    // "inaktiv fast unsichtbar". Zusammen mit dem neuen Panelgrund lagen
    // Knopf und Flaeche dann so nah beieinander, dass die Beschriftungen
    // verschwanden.
    //
    // Die Regel dahinter, gemeldet am 2026-08-16: NUR EINGABEFELDER SIND
    // MULDEN. Ein Knopf liegt auf der Flaeche und braucht einen eigenen
    // Grund und einen sichtbaren Rand. "Fast unsichtbar" heisst
    // zurueckhaltend gegenueber dem AKTIVEN Knopf, nicht unlesbar.
    const QString kMenuBtnNormal = QStringLiteral(
        "QPushButton { background: #1a1a1e; "
        "border: 1px solid #2c2c31; border-radius: 6px; "
        "padding: 0 11px; "
        "color: #c4c4c9; font-size: 11px; font-weight: bold; }"
        "QPushButton:hover { background: #2f3138; "
        "border: 1px solid %1; }")
        .arg(QString::fromLatin1(Style::kBlueBorder));

    // Auswahl: Füllung, Rahmen und Text wörtlich aus HAUSSTIL §Token.
    const QString kMenuBtnActive = QStringLiteral(
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
        "stop:0 %1, stop:1 %1); "
        "border: 1px solid %2; border-radius: 6px; "
        "padding: 0 11px; "
        "color: %3; font-size: 11px; font-weight: bold; }")
        .arg(QString::fromLatin1(Style::kBlueBg),
             QString::fromLatin1(Style::kBlueBorder),
             QString::fromLatin1(Style::kBlueText));

    const QString kMenuBtnDisabled = QStringLiteral(
        "QPushButton { background: transparent; "
        "border: 1px solid #232327; border-radius: 6px; "
        "padding: 0 11px; "
        "color: %1; font-size: 11px; font-weight: bold; }")
        .arg(QString::fromLatin1(Style::kBorderMuted));

    // ── Die Versalzeile über jeder Gruppe ───────────────────────────
    //
    // HAUSSTIL Regel 1: „Über jeder Knopfreihe eine Versalzeile: 8–9 px,
    // Laufweite .18em, Farbe Skala. Nie ein Knopf ohne Überschrift."
    //
    // Die Laufweite steht NICHT hier: Qt-Stylesheets kennen kein
    // letter-spacing, das wird still verworfen. Sie wird in
    // makeGroupHead() über QFont::setLetterSpacing gesetzt — 0,18 em bei
    // 9 px sind 1,62 px absolut.
    // Die Flaeche der Spalte selbst. Panel-Ton aus HAUSSTIL, leicht
    // durchscheinend, damit die Kurve dahinter noch zu ahnen ist —
    // aber deckend genug, dass sie eine Flaeche IST und nicht ein
    // Schleier. Radius 6, nie 3.
    constexpr auto kColumnStyle =
        "QWidget#spectrumOverlayPanel { background: rgba(12, 12, 14, 232); "
        "border: 1px solid #1f1f23; border-radius: 6px; }";

    const QString kGroupHeadStyle = QStringLiteral(
        "QLabel { color: %1; font-size: 9px; font-weight: bold; "
        "background: transparent; }")
        .arg(QString::fromLatin1(Style::kTextGroupHead));
} // namespace OverlayColors

// File-local helpers for opaque styles that diverge from the canonical
// Style:: helpers (per §A2 exception pattern — verified byte-by-byte):

// overlaySliderStyle — diverges from Style::sliderHStyle() on two counts:
//   - groove bg: #1a2a3a (Style::kButtonBg) vs Style::sliderHStyle() kGroove (#203040)
//   - handle bg: #c8d8e8 (Style::kTextPrimary, grey-white) vs kAccent (#00b4d8, cyan)
//   - handle margin: -4px 0 vs sliderHStyle()'s -3px 0
// These are deliberate — the overlay slider uses a low-contrast white handle
// that is legible against the translucent panel rather than the main spectrum
// accent colour.
static inline QString overlaySliderStyle()
{
    return QStringLiteral(
        "QSlider::groove:horizontal { background: %1; height: 4px; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: %2; width: 10px; margin: -4px 0; border-radius: 5px; }"
    ).arg(Style::kButtonBg, Style::kTextPrimary);
}

// overlayDisplayToggleStyle — diverges from Style::buttonBaseStyle() + Style::greenCheckedStyle():
//   - padding: 2px 6px vs buttonBaseStyle()'s 2px 4px (wider pill shape for display toggles)
// All other tokens match: bg kButtonBg, border kBorder, color kTextPrimary, 10px bold,
// border-radius 3px, hover kButtonAltHover, checked bg/text/border kGreen*.
static inline QString overlayDisplayToggleStyle()
{
    return Style::buttonBaseStyle().replace("padding: 2px 4px", "padding: 2px 6px")
         + Style::greenCheckedStyle();
}

// ── Wo die Versalzeilen stehen ──────────────────────────────────────
//
// Der Index ist die Stelle in m_menuBtns, VOR der die Zeile sitzt.
// Bewusst eine eigene Tabelle statt Beschriftungen zwischen die Knöpfe
// zu mischen: m_menuBtns wird an vier Stellen über feste Indizes
// angesprochen (BAND = 2, ANT = 3, Display = 4, VAX = 5, um die Flyouts
// zu setzen). Kämen Beschriftungen mit hinein, verschöbe sich jeder
// dieser Indizes — und zwar lautlos, denn ein Flyout an der falschen
// Stelle stürzt nicht ab, es geht nur woanders auf.
//
// Gruppierung nach Wirkung, entschieden vom Betreiber am 2026-08-15:
// was etwas anlegt, wohin gehört wird, was zu sehen ist, wie laut.
// Der Einklapp-Knopf steht ohne Überschrift ganz oben — er bedient das
// Panel selbst und nicht das Radio.
struct GroupHead { int beforeIndex; const char* title; };
static constexpr GroupHead kGroupHeads[] = {
    { 0, "NEU"     },   // +RX, +TNF
    { 2, "BAND"    },   // BAND, ANT
    { 4, "ANSICHT" },   // Display
    { 5, "PEGEL"   },   // VAX, ATT
};

// Helper: eine Versalzeile.
//
// Die Laufweite kommt hier her und nicht aus dem Stylesheet, weil
// Qt-Stylesheets kein letter-spacing kennen und es kommentarlos
// verwerfen. 0,18 em bei 9 px = 1,62 px.
static QLabel* makeGroupHead(const QString& text, QWidget* parent)
{
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet(OverlayColors::kGroupHeadStyle);
    QFont f = lbl->font();
    f.setPixelSize(9);
    f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.62);
    lbl->setFont(f);
    lbl->setFixedHeight(kHeadH);
    return lbl;
}

// Helper: create a standard menu button
static QPushButton* makeMenuBtn(const QString& text, QWidget* parent)
{
    auto* btn = new QPushButton(text, parent);
    btn->setFixedSize(kBtnW, kBtnH);
    btn->setStyleSheet(OverlayColors::kMenuBtnNormal);
    return btn;
}

// Helper: create a disabled NYI menu button
static QPushButton* makeDisabledBtn(const QString& text, QWidget* parent)
{
    auto* btn = new QPushButton(text, parent);
    btn->setFixedSize(kBtnW, kBtnH);
    btn->setStyleSheet(OverlayColors::kMenuBtnDisabled);
    btn->setEnabled(false);
    btn->setToolTip("Not yet implemented");
    return btn;
}

// ── Band table (from AetherSDR SpectrumOverlayMenu.cpp, reduced to HF + WWV) ─
struct BandEntry {
    const char* label;
    const char* name;
    double      freqHz;
    const char* mode;
};

// Frequencies in Hz (task spec: 1.8e6, 3.5e6, etc.)
static constexpr BandEntry kBands[] = {
    {"160", "160m",  1.8e6,    "LSB"},
    {"80",  "80m",   3.5e6,    "LSB"},
    {"60",  "60m",   5.3e6,    "USB"},
    {"40",  "40m",   7.0e6,    "LSB"},
    {"30",  "30m",  10.1e6,    "DIGU"},
    {"20",  "20m",  14.0e6,    "USB"},
    {"17",  "17m",  18.068e6,  "USB"},
    {"15",  "15m",  21.0e6,    "USB"},
    {"12",  "12m",  24.89e6,   "USB"},
    {"10",  "10m",  28.0e6,    "USB"},
    {"6",   "6m",   50.0e6,    "USB"},
    {"WWV", "WWV",  10.0e6,    "AM"},
};
static constexpr int kBandCount = static_cast<int>(sizeof(kBands) / sizeof(kBands[0]));

// ── Constructor ───────────────────────────────────────────────────────────────

SpectrumOverlayPanel::SpectrumOverlayPanel(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    // Nativ, sonst unsichtbar — dieselbe Begruendung wie beim
    // Zoomstreifen in buildZoomButtons(): der Panadapter ist ein
    // QRhiWidget mit WA_NativeWindow, und ein natives NSView zeichnet
    // auf macOS ueber allen nicht-nativen Geschwistern
    // (SpectrumWidget.cpp:551). Die Bedienflaeche liegt in genau diesem
    // Panadapter.
    setAttribute(Qt::WA_NativeWindow);

    // ── Die Spalte braucht eine eigene Flaeche ───────────────────────
    //
    // Hier stand WA_NoSystemBackground: das Panel malte nichts, und der
    // geschlossene Streifen, den man sah, waren in Wahrheit acht zu 94 %
    // deckende Knoepfe nebeneinander. kPanelStyle lag nur auf den
    // Flyouts, nie auf der Spalte selbst.
    //
    // Solange die Knoepfe deckten, fiel das nicht auf. Als sie nach
    // HAUSSTIL Regel 2 ("inaktiv fast unsichtbar") duenn wurden, ging
    // die Flaeche mit ihnen weg und die Knoepfe schwebten ueber der
    // Kurve. Die Regel setzt ein Panel voraus; das hier hatte keins.
    //
    // Objektname + QWidget#-Selektor, damit die Flaeche nicht auf jedes
    // Kind durchschlaegt — ein blankes background: im Stylesheet eines
    // Elternwidgets faerbt sonst auch die Knoepfe.
    setObjectName(QStringLiteral("spectrumOverlayPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(OverlayColors::kColumnStyle);
    setAutoFillBackground(false);

    // Button 1: Collapse toggle
    m_collapseBtn = new QPushButton(this);
    m_collapseBtn->setFixedSize(kBtnW, kBtnH);
    m_collapseBtn->setStyleSheet(Style::themed(
        "QPushButton { background: rgba(20, 30, 45, 240); "
        "border: 1px solid rgba(255, 255, 255, 40); border-radius: 6px; "
        "color: #c8d8e8; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: rgba(0, 112, 192, 180); "
        "border: 1px solid #4a7ba8; }"));
    connect(m_collapseBtn, &QPushButton::clicked, this, &SpectrumOverlayPanel::toggle);

    // Button 2: +RX (NYI Phase 3F)
    {
        // Adds a slice on THIS pan -- the one this strip is drawn on -- not on
        // whichever pan happens to be active. A control rendered on a pan acts
        // on that pan; anything else makes the operator track hidden state to
        // predict what a visible button will do.
        auto* btn = makeMenuBtn("+RX", this);
        btn->setToolTip("Add an RX slice on this panadapter");
        connect(btn, &QPushButton::clicked, this, [this]() {
            emit addRxClicked(m_panId);
        });
        m_menuBtns.append(btn);
    }

    // Button 3: +TNF
    {
        // From AetherSDR src/gui/SpectrumOverlayMenu.cpp:293 [@c6481cbf] --
        // the "+TNF" entry in the strip's button table. Upstream's signal is
        // arg-less; ours carries the pan id, matching the +RX shape at
        // SpectrumOverlayMenu.cpp:315 [@c6481cbf] so a strip drawn on pan-2
        // never adds a notch on pan-0.
        auto* btn = makeMenuBtn("+TNF", this);
        btn->setObjectName(QStringLiteral("tnfAddButton"));
        btn->setToolTip("Add a notch filter at this panadapter's VFO");
        connect(btn, &QPushButton::clicked, this, [this]() {
            emit addTnfClicked(m_panId);
        });
        m_menuBtns.append(btn);  // index 1
    }

    // Button 4: BAND — flyout
    {
        auto* btn = makeMenuBtn("BAND", this);
        btn->setToolTip("Open band selector");
        connect(btn, &QPushButton::clicked, this, &SpectrumOverlayPanel::toggleBandFlyout);
        m_menuBtns.append(btn);  // index 2
    }

    // Button 5: ANT — flyout
    {
        auto* btn = makeMenuBtn("ANT", this);
        btn->setToolTip("Open antenna and RF gain controls");
        connect(btn, &QPushButton::clicked, this, &SpectrumOverlayPanel::toggleAntFlyout);
        m_menuBtns.append(btn);  // index 3
    }

    // DSP flyout removed — controls duplicated the VFO flag's DSP grid.
    // Use the top menu bar's DSP menu (full parity) or the VFO flag.

    // Button 6: Display — flyout
    {
        auto* btn = makeMenuBtn("Display", this);
        btn->setToolTip("Open display settings");
        connect(btn, &QPushButton::clicked, this, &SpectrumOverlayPanel::toggleDisplayFlyout);
        m_menuBtns.append(btn);  // index 4
    }

    // Button 7: VAX — flyout (NYI Phase 3-VAX)
    {
        auto* btn = makeMenuBtn("VAX", this);
        btn->setToolTip("Open VAX audio routing (NYI Phase 3-VAX)");
        connect(btn, &QPushButton::clicked, this, &SpectrumOverlayPanel::toggleVaxFlyout);
        m_menuBtns.append(btn);  // index 5
    }

    // Button 8: ATT (NYI)
    {
        auto* btn = makeDisabledBtn("ATT", this);
        btn->setToolTip("Attenuator control (NYI)");
        m_menuBtns.append(btn);  // index 6
    }

    // The strip used to end in a disabled "MNF" twin of the +TNF button
    // above. It is gone rather than shipped beside a live control that does
    // the same job.

    buildBandFlyout();
    buildAntFlyout();
    buildDisplayFlyout();
    buildVaxFlyout();
    buildZoomButtons();

    // Install event filter for auto-close on outside click and parent resize tracking
    qApp->installEventFilter(this);
    if (parentWidget()) {
        parentWidget()->installEventFilter(this);
    }

    updateLayout();
}

// ── eventFilter — auto-close flyout on outside click ─────────────────────────

bool SpectrumOverlayPanel::eventFilter(QObject* obj, QEvent* event)
{
    // Wieviel Platz die Spalte hat, haengt an der Hoehe des
    // Kurvenbereichs -- die aendert sich mit jedem Fenstergriff.
    // updateLayout() lief bisher nur beim Bau und beim Ein-/Ausklappen,
    // also blieb eine einmal getroffene Ueberlauf-Entscheidung fuer die
    // ganze Sitzung stehen.
    if (obj == parentWidget() && event->type() == QEvent::Resize) {
        updateLayout();
    }

    // Auto-close flyout on outside click (from AetherSDR)
    if (event->type() == QEvent::MouseButtonPress && m_activeFlyout) {
        auto* me = static_cast<QMouseEvent*>(event);
        QPoint gp = me->globalPosition().toPoint();
        bool inFlyout = m_activeFlyout->rect().contains(
                            m_activeFlyout->mapFromGlobal(gp));
        bool inButton = m_activeButton &&
                        m_activeButton->rect().contains(
                            m_activeButton->mapFromGlobal(gp));
        bool inPanel  = rect().contains(mapFromGlobal(gp));
        if (!inFlyout && !inButton && !inPanel) {
            hideFlyout();
        }
    }

    // Reposition zoom buttons when parent (spectrum widget) resizes
    if (obj == parentWidget() && event->type() == QEvent::Resize) {
        repositionZoomButtons();
    }

    return QWidget::eventFilter(obj, event);
}

// ── raiseAll ──────────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::raiseAll()
{
    raise();
    for (QWidget* fw : {m_bandFlyout, m_antFlyout,
                        m_displayFlyout, m_vaxFlyout}) {
        if (fw) { fw->raise(); }
    }
}

// ── hideFlyout — hide active flyout and reset button style ───────────────────

void SpectrumOverlayPanel::hideFlyout()
{
    if (m_activeFlyout) {
        m_activeFlyout->hide();
        m_activeFlyout = nullptr;
    }
    if (m_activeButton) {
        m_activeButton->setStyleSheet(OverlayColors::kMenuBtnNormal);
        m_activeButton = nullptr;
    }
}

// ── updateLayout ─────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::updateLayout()
{
    m_collapseBtn->setText(m_expanded ? QStringLiteral("\u25c0")   // ◀
                                      : QStringLiteral("\u25b6")); // ▶
    m_collapseBtn->move(kPad, kPad);

    // Die Beschriftungen entstehen beim ersten Durchlauf und werden
    // danach nur noch bewegt — sie hängen an keiner Zustandsänderung.
    if (m_groupHeads.isEmpty()) {
        for (const GroupHead& g : kGroupHeads) {
            m_groupHeads.append(
                makeGroupHead(QString::fromLatin1(g.title), this));
        }
    }

    // ── Wie viele Gruppen passen ueberhaupt hinein ───────────────────
    //
    // Die Spalte wuchs mit den Pillen und den Versalzeilen von 194 auf
    // 383 px. Auf dem Geraet lief sie damit unten aus dem Spektrum
    // heraus: VAX lag ueber dem Bandbalken, ATT ueber der Frequenzskala.
    //
    // Hier bekommt das "..." aus HAUSSTIL Regel 2 seine Aufgabe, und
    // zwar eine echte: nicht "drei statt dreizehn" als Geschmacksfrage,
    // sondern weil der Platz endet. Was nicht mehr hineinpasst, wandert
    // hinter das Auslassungszeichen statt ueber die Skala.
    //
    // Gefahrlos, weil die Flyouts am PANEL haengen (x() + width()) und
    // nicht an ihrem Knopf — ein ausgeblendeter Knopf laesst sein
    // Flyout weiter an derselben Stelle aufgehen.
    // Nur der Kurvenbereich, nicht die ganze Flaeche des Widgets: unter
    // dem Spektrum liegen Wasserfall, Bandbalken und Frequenzskala, und
    // ueber denen hat die Spalte nichts zu suchen — das war der Befund.
    const auto* sw = qobject_cast<const SpectrumWidget*>(parentWidget());
    int avail = sw ? sw->spectrumAreaHeight() - y() - kPad * 2
                   : (parentWidget()
                          ? parentWidget()->height() - y() - kPad * 2
                          : 100000);

    // updateLayout() laeuft auch beim Bau, und da hat das Elternwidget
    // seine Groesse noch nicht. Ein spectrumAreaHeight() von 0 haette
    // sonst JEDE Gruppe hinter das "..." geschoben -- und weil die
    // Entscheidung ohne die Groessenaenderung unten nie wiederholt
    // wurde, waere sie fuer die ganze Sitzung stehen geblieben.
    //
    // Zu frueh gefragt heisst nicht "kein Platz", sondern "noch nicht
    // bekannt". In dem Fall alles zeigen; der Resize korrigiert.
    if (avail < kBtnH * 2) { avail = 100000; }

    int visibleBtnCount = m_menuBtns.size();
    {
        int probe = kPad + kBtnH + kGroupGap;
        int g = 0;
        for (int i = 0; i < m_menuBtns.size(); ++i) {
            if (g < static_cast<int>(std::size(kGroupHeads))
                && kGroupHeads[g].beforeIndex == i) {
                if (i > 0) { probe += kGroupGap - kGap; }
                probe += kHeadH;
                ++g;
            }
            probe += kBtnH + kGap;
            // probe steht jetzt unter dem gerade gesetzten Knopf. Passt
            // darunter noch das "..." selbst? Wenn nicht, ist hier
            // Schluss. Bewusst mitgerechnet: ein Auslassungszeichen, das
            // seinerseits nicht mehr hineinpasst, waere die Anzeige, die
            // ihre eigene Unvollstaendigkeit verschweigt.
            if (probe + kBtnH + kPad > avail) {
                // Auf Gruppengrenze zurueckgehen: eine halbe Gruppe
                // anzuzeigen waere schlimmer als eine ganze zu
                // verstecken — die Versalzeile stuende dann ueber
                // nichts.
                visibleBtnCount = (g > 0) ? kGroupHeads[g - 1].beforeIndex : 0;
                break;
            }
        }
    }
    const bool overflow = visibleBtnCount < m_menuBtns.size();

    int y = kPad + kBtnH + kGroupGap;
    int headIdx = 0;
    for (int i = 0; i < m_menuBtns.size(); ++i) {
        // Die Versalzeile MUSS vor der Sichtbarkeitspruefung behandelt
        // werden. Stand sie dahinter, uebersprang der continue-Zweig sie
        // mit: Ueberschriften ausgeblendeter Gruppen wurden weder
        // versteckt noch bewegt, blieben also auf ihrer Erstposition
        // (0, 0) liegen und wurden mit dem Panel sichtbar -- alle vier
        // uebereinander, oben links, als unlesbares Gemisch. Genau so
        // gemeldet am 2026-08-15.
        const bool startsGroup =
            headIdx < static_cast<int>(std::size(kGroupHeads))
            && kGroupHeads[headIdx].beforeIndex == i;
        const bool groupShown = (i < visibleBtnCount);

        if (startsGroup && !groupShown) {
            m_groupHeads[headIdx]->setVisible(false);
            ++headIdx;
        }
        if (!groupShown) {
            m_menuBtns[i]->setVisible(false);
            continue;
        }

        if (startsGroup) {
            QLabel* head = m_groupHeads[headIdx];
            head->setVisible(m_expanded);
            if (m_expanded) {
                // Der Gruppenabstand steht ÜBER der Zeile, nicht
                // zwischen Zeile und erstem Knopf — sonst schwebt die
                // Überschrift in der Mitte und gehört optisch zu der
                // Gruppe darüber.
                if (i > 0) { y += kGroupGap - kGap; }
                head->move(kPad + 2, y);
                head->setFixedWidth(kBtnW - 2);
                y += kHeadH;
            }
            ++headIdx;
        }

        QPushButton* btn = m_menuBtns[i];
        btn->setVisible(m_expanded);
        if (m_expanded) {
            btn->move(kPad, y);
            y += kBtnH + kGap;
        }
    }

    // ── Das Auslassungszeichen ───────────────────────────────────────
    //
    // Entsteht erst, wenn es gebraucht wird, und traegt die Gruppen, die
    // nicht mehr hineinpassten — beschriftet mit ihrem Gruppennamen,
    // damit man sieht, WAS dahinter liegt und nicht nur DASS etwas
    // dahinter liegt.
    if (overflow && m_expanded) {
        if (!m_moreBtn) {
            m_moreBtn = new QPushButton(QStringLiteral("…"), this);
            m_moreBtn->setFixedSize(kBtnW, kBtnH);
            m_moreBtn->setStyleSheet(OverlayColors::kMenuBtnNormal);
            m_moreBtn->setToolTip(
                tr("More groups — this is where the room ends"));
            connect(m_moreBtn, &QPushButton::clicked, this, [this]() {
                ScopedChildWidget<QMenu> menuOwner(window());   // stirbt nicht mit dem Rahmen, siehe ScopedChildWidget.h
                const QPointer<SpectrumOverlayPanel> self(this);   // Menue am Fenster: ein nativer Vorfahr gab ihm keinen Fensterbezug (2026-09-30)
                QMenu& menu = *menuOwner.get();
                int g = 0;
                for (int i = 0; i < m_menuBtns.size(); ++i) {
                    while (g < static_cast<int>(std::size(kGroupHeads))
                           && kGroupHeads[g].beforeIndex == i) {
                        if (!m_menuBtns[i]->isVisible()) {
                            menu.addSection(
                                QString::fromLatin1(kGroupHeads[g].title));
                        }
                        ++g;
                    }
                    if (m_menuBtns[i]->isVisible()) { continue; }
                    QPushButton* btn = m_menuBtns[i];
                    QAction* a = menu.addAction(btn->text());
                    a->setEnabled(btn->isEnabled());
                    // click() statt des Signals direkt: der Knopf traegt
                    // seine Verdrahtung schon, und ein zweiter Weg
                    // dorthin waere ein zweiter Ort zum Vergessen.
                    connect(a, &QAction::triggered, btn, [btn]() {
                        btn->click();
                    });
                }
                menu.exec(mapToGlobal(m_moreBtn->geometry().bottomLeft()));
                // Das Elternteil kann waehrend exec() gestorben sein — dann ist
                // auch das Menue weg und `this` eine Leiche. Siehe ScopedChildWidget.h.
                if (!menuOwner || !self) { return; }
            });
        }
        m_moreBtn->setVisible(true);
        m_moreBtn->move(kPad, y);
        y += kBtnH + kGap;
    } else if (m_moreBtn) {
        m_moreBtn->setVisible(false);
    }

    const int totalH = m_expanded ? (y - kGap + kPad)
                                  : (kPad + kBtnH + kPad);
    setFixedSize(kPad + kBtnW + kPad, totalH);
}

// ── toggle ────────────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::toggle()
{
    m_expanded = !m_expanded;
    if (!m_expanded) {
        hideFlyout();
    }
    updateLayout();
    emit collapsed(!m_expanded);
}

void SpectrumOverlayPanel::setExpandedState(bool expanded)
{
    if (m_expanded == expanded) { return; }
    m_expanded = expanded;
    if (!m_expanded) {
        hideFlyout();
    }
    updateLayout();
}

// ── Band flyout ───────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::buildBandFlyout()
{
    m_bandFlyout = new QWidget(parentWidget());
    m_bandFlyout->setStyleSheet(OverlayColors::kPanelStyle);
    m_bandFlyout->hide();

    auto* grid = new QGridLayout(m_bandFlyout);
    grid->setContentsMargins(2, 2, 2, 2);
    grid->setSpacing(2);

    const QString bandBtnStyle =
        "QPushButton { background: rgba(30, 40, 55, 220); "
        "border: 1px solid #304050; border-radius: 6px; "
        "color: #c8d8e8; font-size: 11px; font-weight: bold; }"
        "QPushButton:hover { background: rgba(0, 112, 192, 180); "
        "border: 1px solid #4a7ba8; }"
        // Welches Band gerade gehoert wird -- von einer AetherSDR-
        // Sichtung angestossen (2026-09-06). Entwurf A vom Betreiber
        // gewaehlt: wortgleich der :checked-Stil, den der WNB-Knopf
        // im selben Panel schon nutzt (Zeile ~892).
        "QPushButton:checked { background: #4a7ba8; color: #ffffff; "
        "border: 1px solid #4a7ba8; }";

    // 4-column grid layout:
    // Row 0: 160, 80, 60, 40
    // Row 1: 30, 20, 17, 15
    // Row 2: 12, 10, 6, WWV
    static constexpr int kCols = 4;
    m_bandButtons.clear();
    for (int i = 0; i < kBandCount; ++i) {
        int row = i / kCols;
        int col = i % kCols;

        auto* btn = new QPushButton(QString::fromLatin1(kBands[i].label), m_bandFlyout);
        btn->setFixedSize(kBandBtnW, kBandBtnH);
        btn->setStyleSheet(bandBtnStyle);
        btn->setCheckable(true);

        QString bandName = QString::fromLatin1(kBands[i].name);
        double  freqHz   = kBands[i].freqHz;
        QString mode     = QString::fromLatin1(kBands[i].mode);

        connect(btn, &QPushButton::clicked, this, [this, bandName, freqHz, mode]() {
            hideFlyout();
            emit bandSelected(bandName, freqHz, mode);
        });
        grid->addWidget(btn, row, col);
        m_bandButtons.insert(bandName, btn);
    }

    m_bandFlyout->adjustSize();
}

void SpectrumOverlayPanel::setActiveBandHighlight(const QString& bandKeyName)
{
    for (auto it = m_bandButtons.constBegin(); it != m_bandButtons.constEnd(); ++it) {
        it.value()->setChecked(it.key() == bandKeyName);
    }
}

void SpectrumOverlayPanel::toggleBandFlyout()
{
    // Button index 2 (BAND) in m_menuBtns
    QPushButton* bandBtn = m_menuBtns[2];

    if (m_activeFlyout == m_bandFlyout) {
        hideFlyout();
        return;
    }
    hideFlyout();

    // Flyout: top-aligned with panel (from AetherSDR toggleBandPanel)
    m_bandFlyout->move(x() + width(), y());
    m_bandFlyout->raise();
    m_bandFlyout->show();
    m_activeFlyout = m_bandFlyout;
    m_activeButton = bandBtn;
    bandBtn->setStyleSheet(OverlayColors::kMenuBtnActive);
}

// ── ANT flyout ────────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::buildAntFlyout()
{
    m_antFlyout = new QWidget(parentWidget());
    m_antFlyout->setStyleSheet(OverlayColors::kPanelStyle);
    m_antFlyout->hide();

    auto* vbox = new QVBoxLayout(m_antFlyout);
    vbox->setContentsMargins(6, 6, 6, 6);
    vbox->setSpacing(4);

    constexpr int kLabelW = 52;

    // Phase 3P-I-a T18 — RX/TX antenna rows. Prior to this phase the
    // combos existed but had no currentTextChanged connect — they were
    // zombie controls. Now wired through the pan's resolved slice,
    // and both rows are wrapped in a QWidget so setBoardCapabilities
    // can hide them as a unit on HL2/Atlas.
    {
        m_rxAntRow = new QWidget;
        auto* row = new QHBoxLayout(m_rxAntRow);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);
        auto* lbl = new QLabel("RX Ant:");
        lbl->setStyleSheet(OverlayColors::kLabelStyle);
        lbl->setFixedWidth(kLabelW);
        row->addWidget(lbl);
        m_rxAntCmb = new QComboBox;
        m_rxAntCmb->setObjectName(QStringLiteral("m_rxAntCmb"));
        // Seed with default labels; setBoardCapabilities replaces once
        // a radio is connected.
        m_rxAntCmb->addItems(QStringList{"ANT1", "ANT2", "ANT3"});
        m_rxAntCmb->setFixedHeight(kBtnH);
        row->addWidget(m_rxAntCmb, 1);
        vbox->addWidget(m_rxAntRow);

        // Widget → Model: user picks an antenna → resolved slice setRxAntenna.
        // T12's RadioModel connect then routes to AlexController and
        // T9's applyAlexAntennaForBand reaches the wire. Echo from the
        // model side is guarded by m_updatingFromModel (shared with VAX
        // — same flag, different combo, doesn't matter because the
        // echoes arrive serially on the GUI thread).
        connect(m_rxAntCmb, &QComboBox::currentTextChanged, this,
                [this](const QString& ant) {
            if (m_updatingFromModel || !m_radioModel || ant.isEmpty()) {
                return;
            }
            if (SliceModel* s = resolvedSlice()) {
                s->setRxAntenna(ant);
            }
        });
    }

    {
        m_txAntRow = new QWidget;
        auto* row = new QHBoxLayout(m_txAntRow);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(4);
        auto* lbl = new QLabel("TX Ant:");
        lbl->setStyleSheet(OverlayColors::kLabelStyle);
        lbl->setFixedWidth(kLabelW);
        row->addWidget(lbl);
        m_txAntCmb = new QComboBox;
        m_txAntCmb->setObjectName(QStringLiteral("m_txAntCmb"));
        m_txAntCmb->addItems(QStringList{"ANT1", "ANT2", "ANT3"});
        m_txAntCmb->setFixedHeight(kBtnH);
        row->addWidget(m_txAntCmb, 1);
        vbox->addWidget(m_txAntRow);

        connect(m_txAntCmb, &QComboBox::currentTextChanged, this,
                [this](const QString& ant) {
            if (m_updatingFromModel || !m_radioModel || ant.isEmpty()) {
                return;
            }
            if (SliceModel* s = resolvedSlice()) {
                s->setTxAntenna(ant);
            }
        });
    }

    // RF Gain slider (-8 to +32 dB) — from AetherSDR buildAntPanel
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);
        auto* lbl = new QLabel("RF Gain:");
        lbl->setStyleSheet(OverlayColors::kLabelStyle);
        lbl->setFixedWidth(kLabelW);
        row->addWidget(lbl);

        m_rfGainSlider = new QSlider(Qt::Horizontal);
        m_rfGainSlider->setRange(-8, 32);
        m_rfGainSlider->setValue(0);
        m_rfGainSlider->setSingleStep(8);
        m_rfGainSlider->setPageStep(8);
        m_rfGainSlider->setTickInterval(8);
        m_rfGainSlider->setTickPosition(QSlider::TicksBelow);
        m_rfGainSlider->setStyleSheet(overlaySliderStyle());
        m_rfGainSlider->setToolTip("RF Gain: −8 to +32 dB (8 dB steps)");
        row->addWidget(m_rfGainSlider, 1);

        m_rfGainLabel = new QLabel("0 dB");
        m_rfGainLabel->setStyleSheet(OverlayColors::kLabelStyle);
        m_rfGainLabel->setFixedWidth(36);
        m_rfGainLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(m_rfGainLabel);
        vbox->addLayout(row);

        connect(m_rfGainSlider, &QSlider::valueChanged, this, [this](int v) {
            // Snap to nearest 8 dB step (from AetherSDR buildAntPanel)
            constexpr int kStep = 8;
            int snapped = qRound(static_cast<double>(v) / kStep) * kStep;
            if (snapped != v) {
                QSignalBlocker sb(m_rfGainSlider);
                m_rfGainSlider->setValue(snapped);
            }
            m_rfGainLabel->setText(QString("%1 dB").arg(snapped));
        });
    }

    // WNB toggle
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);
        m_wnbBtn = new QPushButton("WNB");
        m_wnbBtn->setCheckable(true);
        m_wnbBtn->setFixedSize(48, 22);
        m_wnbBtn->setStyleSheet(Style::themed(
            "QPushButton { background: #1a2a3a; border: 1px solid #304050; "
            "border-radius: 6px; color: #c8d8e8; font-size: 11px; font-weight: bold; }"
            "QPushButton:checked { background: #4a7ba8; color: #ffffff; "
            "border: 1px solid #4a7ba8; }"
            "QPushButton:hover { border: 1px solid #4a7ba8; }"));
        m_wnbBtn->setToolTip("Wideband noise blanker — suppresses impulse noise across panadapter bandwidth");
        row->addWidget(m_wnbBtn);
        row->addStretch();
        vbox->addLayout(row);
    }

    m_antFlyout->setFixedWidth(180);
    m_antFlyout->adjustSize();
}

void SpectrumOverlayPanel::toggleAntFlyout()
{
    // Button index 3 (ANT)
    QPushButton* antBtn = m_menuBtns[3];

    if (m_activeFlyout == m_antFlyout) {
        hideFlyout();
        return;
    }
    hideFlyout();

    // Center flyout vertically on the ANT button (from AetherSDR toggleAntPanel)
    int antBtnCenterY = antBtn->y() + antBtn->height() / 2;
    int panelY = y() + antBtnCenterY - m_antFlyout->sizeHint().height() / 2;
    m_antFlyout->move(x() + width(), std::max(0, panelY));
    m_antFlyout->raise();
    m_antFlyout->show();
    m_activeFlyout = m_antFlyout;
    m_activeButton = antBtn;
    antBtn->setStyleSheet(OverlayColors::kMenuBtnActive);
}

// ── Display flyout ────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::buildDisplayFlyout()
{
    m_displayFlyout = new QWidget(parentWidget());
    m_displayFlyout->setStyleSheet(OverlayColors::kPanelStyle);
    m_displayFlyout->hide();

    auto* grid = new QGridLayout(m_displayFlyout);
    grid->setContentsMargins(8, 6, 8, 6);
    grid->setSpacing(4);
    grid->setColumnStretch(2, 1);

    const QString labelStyle =
        "QLabel { color: #8090a0; font-size: 11px; border: none; }";
    const QString valStyle =
        "QLabel { color: #c8d8e8; font-size: 11px; border: none; min-width: 24px; }";
    const QString sliderStyle =
        "QSlider { border: none; }"
        "QSlider::groove:horizontal { height: 4px; background: #203040; border-radius: 2px; }"
        "QSlider::handle:horizontal { width: 10px; height: 10px; margin: -3px 0;"
        " background: #4a7ba8; border-radius: 5px; }";

    // grid columns: 0=label, 1=button (optional), 2=slider, 3=value
    grid->setColumnMinimumWidth(1, 22);

    int row = 0;

    // Color Scheme combo
    {
        auto* lbl = new QLabel("Scheme:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0, 1, 2);

        m_colorSchemeCmb = new QComboBox;
        m_colorSchemeCmb->setObjectName(QStringLiteral("colorSchemeCmb"));
        m_colorSchemeCmb->setFixedHeight(18);
        // War {"Classic","Phosphor","Sunrise","Inverted"} -- vier erfundene
        // Namen, die keinem WfColorScheme-Wert entsprachen; das aktuell
        // geladene Schema (z.B. ClarityBlue) war darueber gar nicht
        // waehlbar. Betreiber 2026-09-05: alle Schemata aufnehmen. Liste
        // wortgleich aus DisplaySetupPages.cpp uebernommen (dort seit
        // Phase 3G-8/3G-9b die kanonische, Reihenfolge-verbindliche
        // Quelle -- der Index IST der gespeicherte Wert, siehe dortiger
        // Kommentar). Anhaengen, nie einfuegen.
        m_colorSchemeCmb->addItems({
            QStringLiteral("Default"),   QStringLiteral("Enhanced"),
            QStringLiteral("Spectran"),  QStringLiteral("BlackWhite"),
            QStringLiteral("LinLog"),    QStringLiteral("LinRad"),
            QStringLiteral("Custom"),
            QStringLiteral("Clarity Blue"),
            tr("Muted"),
            tr("Slate blue"),
            tr("Night"),
            tr("FT8 amber"),
            tr("Teal green")
        });
        grid->addWidget(m_colorSchemeCmb, row, 2, 1, 2);
        connect(m_colorSchemeCmb, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &SpectrumOverlayPanel::colorSchemeChanged);
        ++row;
    }

    // Spectrum: 2D / 3D combo — perspective stacked-trace view, ported
    // from AetherSDR's "3DSS" (see DssRenderer.h). Only the trace pipeline
    // changes; waterfall and every other overlay stay as-is either way.
    {
        auto* lbl = new QLabel("Spectrum:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0, 1, 2);

        m_renderModeCmb = new QComboBox;
        m_renderModeCmb->setObjectName(QStringLiteral("spectrumRenderModeCombo"));
        m_renderModeCmb->setFixedHeight(18);
        m_renderModeCmb->addItems({"2D Waterfall", "3D Stacked Trace"});
        m_renderModeCmb->setToolTip(
            "2D: FFT trace + waterfall.\n"
            "3D: perspective stacked-trace spectrum stream.");
        grid->addWidget(m_renderModeCmb, row, 2, 1, 2);
        connect(m_renderModeCmb, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &SpectrumOverlayPanel::spectrumRenderModeChanged);
        ++row;
    }

    // WF Gain slider
    {
        auto* lbl = new QLabel("WF Gain:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0);

        m_wfGainSlider = new QSlider(Qt::Horizontal);
        m_wfGainSlider->setObjectName(QStringLiteral("wfGainSlider"));
        m_wfGainSlider->setRange(0, 100);
        m_wfGainSlider->setValue(50);
        m_wfGainSlider->setStyleSheet(sliderStyle);
        m_wfGainSlider->setToolTip("Waterfall color gain. Higher values brighten weak signals.");
        grid->addWidget(m_wfGainSlider, row, 2);

        m_wfGainLabel = new QLabel("50");
        m_wfGainLabel->setStyleSheet(valStyle);
        m_wfGainLabel->setFixedWidth(28);
        m_wfGainLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(m_wfGainLabel, row, 3);

        connect(m_wfGainSlider, &QSlider::valueChanged, this, [this](int v) {
            m_wfGainLabel->setText(QString::number(v));
            emit wfColorGainChanged(v);
        });
        ++row;
    }

    // Black Level slider
    {
        auto* lbl = new QLabel("Black Lvl:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0);

        m_wfBlackSlider = new QSlider(Qt::Horizontal);
        m_wfBlackSlider->setObjectName(QStringLiteral("wfBlackSlider"));
        // SpectrumWidget::m_wfBlackLevel reicht 0..125 (Vorgabe 104); der
        // Regler deckelte bislang bei 100 und haette einen gespeicherten
        // Wert darueber beim ersten Anfassen still gekappt. Betreiber
        // 2026-09-05: Regler auf den Widget-Bereich erweitern.
        m_wfBlackSlider->setRange(0, 125);
        m_wfBlackSlider->setValue(15);
        m_wfBlackSlider->setStyleSheet(sliderStyle);
        m_wfBlackSlider->setToolTip("Waterfall black level. Increase to darken the noise floor.");
        grid->addWidget(m_wfBlackSlider, row, 2);

        m_wfBlackLabel = new QLabel("15");
        m_wfBlackLabel->setStyleSheet(valStyle);
        m_wfBlackLabel->setFixedWidth(28);
        m_wfBlackLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(m_wfBlackLabel, row, 3);

        connect(m_wfBlackSlider, &QSlider::valueChanged, this, [this](int v) {
            m_wfBlackLabel->setText(QString::number(v));
            emit wfBlackLevelChanged(v);
        });
        ++row;
    }

    // Fill Alpha slider
    {
        auto* lbl = new QLabel("Fill Alpha:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0);

        m_fillAlphaSlider = new QSlider(Qt::Horizontal);
        m_fillAlphaSlider->setRange(0, 100);
        m_fillAlphaSlider->setValue(70);
        m_fillAlphaSlider->setStyleSheet(sliderStyle);
        m_fillAlphaSlider->setToolTip("Opacity of the spectrum fill area below the trace.");
        grid->addWidget(m_fillAlphaSlider, row, 2);

        m_fillAlphaLabel = new QLabel("70");
        m_fillAlphaLabel->setStyleSheet(valStyle);
        m_fillAlphaLabel->setFixedWidth(28);
        m_fillAlphaLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(m_fillAlphaLabel, row, 3);

        connect(m_fillAlphaSlider, &QSlider::valueChanged, this, [this](int v) {
            m_fillAlphaLabel->setText(QString::number(v));
            // B8 fix-up: emit so MainWindow can forward to SpectrumWidget::setFillAlpha.
            // int 0..100 → float 0.0..1.0.
            emit fillAlphaChanged(static_cast<float>(v) / 100.0f);
        });
        ++row;
    }

    // Fill Color button
    {
        auto* lbl = new QLabel("Fill Color:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0);

        m_fillColorBtn = new QPushButton;
        m_fillColorBtn->setFixedSize(18, 18);
        // Das Farbfeld zeigt, was tatsächlich gemalt wird — es hing
        // fest auf Zyan und log, sobald das Theme die Kurve änderte.
        m_fillColorBtn->setStyleSheet(Style::themed(
            QStringLiteral("QPushButton { background: %1;"
                           " border: 1px solid #556070;"
                           " border-radius: 6px; }")
                .arg(Style::role("trace", Style::kSpectrumTrace))));
        m_fillColorBtn->setToolTip("Choose spectrum fill color");
        grid->addWidget(m_fillColorBtn, row, 1);

        connect(m_fillColorBtn, &QPushButton::clicked, this, [this]() {
            QColor c = QColorDialog::getColor(QColor(Style::role("trace", Style::kSpectrumTrace)), this, "FFT Fill Color",
                                              QColorDialog::DontUseNativeDialog);
            if (c.isValid()) {
                m_fillColorBtn->setStyleSheet(Style::themed(
                    QString("QPushButton { background: %1; border: 1px solid #556070;"
                            " border-radius: 6px; }").arg(c.name())));
                emit fillColorChanged(c);  // B8 Task 22
            }
        });
        ++row;
    }

    // Show Grid toggle
    {
        auto* lbl = new QLabel("Grid Lines:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0, 1, 2);

        m_showGridBtn = new QPushButton("On");
        m_showGridBtn->setCheckable(true);
        m_showGridBtn->setChecked(true);
        m_showGridBtn->setFixedSize(36, 18);
        m_showGridBtn->setStyleSheet(overlayDisplayToggleStyle());
        m_showGridBtn->setToolTip("Show or hide frequency and dB grid lines");
        grid->addWidget(m_showGridBtn, row, 2, 1, 2);
        connect(m_showGridBtn, &QPushButton::toggled, this, [this](bool on) {
            m_showGridBtn->setText(on ? "On" : "Off");
        });
        ++row;
    }

    // Cursor Freq toggle
    {
        auto* lbl = new QLabel("Cursor Freq:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0, 1, 2);

        m_cursorFreqBtn = new QPushButton("Off");
        m_cursorFreqBtn->setCheckable(true);
        m_cursorFreqBtn->setChecked(true);   // default on — matches SpectrumWidget default
        m_cursorFreqBtn->setText("On");
        m_cursorFreqBtn->setFixedSize(36, 18);
        m_cursorFreqBtn->setStyleSheet(overlayDisplayToggleStyle());
        m_cursorFreqBtn->setToolTip("Show frequency at mouse cursor position");
        grid->addWidget(m_cursorFreqBtn, row, 3, Qt::AlignRight);
        connect(m_cursorFreqBtn, &QPushButton::toggled, this, [this](bool on) {
            m_cursorFreqBtn->setText(on ? "On" : "Off");
            emit cursorFreqVisibleChanged(on);  // B8 Task 21
        });
        ++row;
    }

    // Phase 3G-9c: Clarity adaptive tuning — status badge + Re-tune button
    // NOTE: Heat Map / Noise Floor / Weighted Avg toggles were removed (B8
    // Task 23). They were pure label-update theatre with no signal/model state.
    // Noise floor estimation uses NoiseFloorEstimator + ClarityController (the
    // canonical path). Heat Map gradient control is in Setup → Display → Gradient.
    // Weighted Average is configurable in Setup → Display → Average Mode.
    {
        auto* lbl = new QLabel("Clarity:");
        lbl->setStyleSheet(labelStyle);
        grid->addWidget(lbl, row, 0);

        m_clarityBadge = new QLabel("C");
        m_clarityBadge->setFixedSize(18, 18);
        m_clarityBadge->setAlignment(Qt::AlignCenter);
        m_clarityBadge->setToolTip("Clarity status: green = active, amber = paused");
        m_clarityBadge->hide();
        grid->addWidget(m_clarityBadge, row, 1);

        m_clarityRetuneBtn = new QPushButton("Re-tune");
        m_clarityRetuneBtn->setFixedSize(52, 18);
        m_clarityRetuneBtn->setStyleSheet(Style::themed(
            "QPushButton { background: #204060; color: #c8d8e8; border: 1px solid #304050;"
            "  border-radius: 6px; font-size: 11px; padding: 0; }"
            "QPushButton:hover { background: #204060; }"
            "QPushButton:pressed { background: #161e27; }"));
        m_clarityRetuneBtn->setToolTip("Force Clarity to re-estimate the noise floor now");
        grid->addWidget(m_clarityRetuneBtn, row, 2, 1, 2, Qt::AlignRight);
        connect(m_clarityRetuneBtn, &QPushButton::clicked,
                this, &SpectrumOverlayPanel::clarityRetuneRequested);
        ++row;
    }

    // Separator
    {
        auto* sep = new QFrame;
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet(Style::themed("QFrame { color: #304050; border: none; }"));
        sep->setFixedHeight(2);
        grid->addWidget(sep, row, 0, 1, 4);
        ++row;
    }

    // "More Display Options →" footer link — B8 Task 24: wired to Setup → Display.
    {
        auto* moreLbl = new QLabel("<a href=\"#more\" style=\"color: #4a7ba8; "
                                   "text-decoration: none; font-size: 11px;\">"
                                   "More Display Options &#x2192;</a>");
        moreLbl->setTextFormat(Qt::RichText);
        moreLbl->setTextInteractionFlags(Qt::TextBrowserInteraction);
        moreLbl->setToolTip("Open full display settings in Setup → Display");
        grid->addWidget(moreLbl, row, 0, 1, 4);
        // linkActivated fires when the anchor is clicked; ignore the href value
        // and always navigate to the Display page.
        connect(moreLbl, &QLabel::linkActivated, this, [this](const QString&) {
            emit openSetupRequested(QStringLiteral("Display"));
        });
        ++row;
    }

    m_displayFlyout->adjustSize();
}

void SpectrumOverlayPanel::toggleDisplayFlyout()
{
    // Button index 4 (Display)
    QPushButton* dispBtn = m_menuBtns[4];

    if (m_activeFlyout == m_displayFlyout) {
        hideFlyout();
        return;
    }
    hideFlyout();

    // adjustSize then position (from AetherSDR toggleDisplayPanel)
    m_displayFlyout->adjustSize();
    m_displayFlyout->move(x() + width(), y());
    m_displayFlyout->raise();
    m_displayFlyout->show();
    m_activeFlyout = m_displayFlyout;
    m_activeButton = dispBtn;
    dispBtn->setStyleSheet(OverlayColors::kMenuBtnActive);
}

// ── VAX flyout ────────────────────────────────────────────────────────────────

void SpectrumOverlayPanel::buildVaxFlyout()
{
    m_vaxFlyout = new QWidget(parentWidget());
    m_vaxFlyout->setStyleSheet(OverlayColors::kPanelStyle);
    m_vaxFlyout->hide();

    auto* vb = new QVBoxLayout(m_vaxFlyout);
    vb->setContentsMargins(6, 6, 6, 6);
    vb->setSpacing(4);

    // VAX Ch combo (from AetherSDR buildDaxPanel)
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);
        auto* lbl = new QLabel("VAX Ch");
        lbl->setStyleSheet(OverlayColors::kLabelStyle);
        row->addWidget(lbl);
        m_vaxCmb = new QComboBox;
        m_vaxCmb->setObjectName(QStringLiteral("vaxCombo"));
        m_vaxCmb->addItems({"Off", "1", "2", "3", "4"});
        // Disabled until setRadioModel() resolves a slice; retains the
        // pre-3O tooltip in that transient state.
        m_vaxCmb->setEnabled(false);
        m_vaxCmb->setToolTip("VAX channel (not yet bound to a radio model)");
        row->addWidget(m_vaxCmb, 1);
        vb->addLayout(row);

        // Widget → Model: user picks a channel → resolved slice setVaxChannel.
        // Combo index 0 = "Off" = vaxChannel 0; indices 1..4 = VAX 1..4
        // (1:1 mapping, matches the header ordering above).
        connect(m_vaxCmb, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) {
            if (m_updatingFromModel || !m_radioModel) {
                return;
            }
            SliceModel* s = resolvedSlice();
            if (s) {
                s->setVaxChannel(idx);
            }
        });
    }

    // IQ Ch combo — reserved for a future phase (design spec §11.3).
    // audio/SendIqToVax is stored-but-not-active per spec §6.7; the combo
    // stays disabled until a consumer of the I/Q-to-VAX path exists.
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);
        auto* lbl = new QLabel("IQ Ch");
        lbl->setStyleSheet(OverlayColors::kLabelStyle);
        row->addWidget(lbl);
        m_vaxIqCmb = new QComboBox;
        m_vaxIqCmb->setObjectName(QStringLiteral("vaxIqCombo"));
        m_vaxIqCmb->addItems({"None", "1", "2", "3", "4"});
        m_vaxIqCmb->setEnabled(false);
        m_vaxIqCmb->setToolTip("IQ-stream to VAX \u2014 reserved for future phase (design spec \u00a711.3)");
        row->addWidget(m_vaxIqCmb, 1);
        vb->addLayout(row);
    }

    m_vaxFlyout->setFixedWidth(140);
    m_vaxFlyout->adjustSize();
}

// Bind VAX and antenna controls to the slice currently owned by this pan.
//
// Widget → Model: the QComboBox::currentIndexChanged lambda installed in
// buildVaxFlyout() calls the resolved slice's setVaxChannel() (and gates on
// m_updatingFromModel to suppress the echo).
// Model → Widget: this function stores the vaxChannelChanged connection in
// m_vaxChannelConn so a rebind can cleanly disconnect before reconnecting
// to a new slice. QObject auto-disconnect handles the model-destroyed case
// for free; a live-to-live rebind needs the explicit disconnect we do here.
void SpectrumOverlayPanel::setRadioModel(RadioModel* model)
{
    if (m_radioModel == model) {
        return;
    }

    // Drop any prior Model→Widget subscription. Safe when the stored
    // connection is default-constructed (QObject::disconnect() no-ops on
    // an invalid Connection handle).
    if (m_vaxChannelConn) {
        QObject::disconnect(m_vaxChannelConn);
        m_vaxChannelConn = {};
    }
    if (m_rxAntConn) {
        QObject::disconnect(m_rxAntConn);
        m_rxAntConn = {};
    }
    if (m_txAntConn) {
        QObject::disconnect(m_txAntConn);
        m_txAntConn = {};
    }

    if (m_radioModel) {
        QObject::disconnect(m_radioModel, nullptr, this, nullptr);
    }
    m_radioModel = model;

    if (!m_vaxCmb) {
        return;  // defensive: flyout builder has not run yet
    }

    if (!m_radioModel) {
        // Unbound — revert to the pre-3O disabled state.
        m_vaxCmb->setEnabled(false);
        m_vaxCmb->setToolTip("VAX channel (not yet bound to a radio model)");
        if (m_rxAntCmb) { m_rxAntCmb->setEnabled(false); }
        if (m_txAntCmb) { m_txAntCmb->setEnabled(false); }
        return;
    }

    // Any slice topology change can change this pan's resolved slice.
    const auto rebind = [this](int) {
        bindToPanSlice();
    };
    connect(m_radioModel, &RadioModel::sliceAdded,   this, rebind);
    connect(m_radioModel, &RadioModel::sliceRemoved, this, rebind);

    bindToPanSlice();
}

void SpectrumOverlayPanel::setSliceResolver(SliceResolver resolver)
{
    m_sliceResolver = std::move(resolver);
    if (m_radioModel) {
        bindToPanSlice();
    }
}

SliceModel* SpectrumOverlayPanel::resolvedSlice() const
{
    if (!m_radioModel) {
        return nullptr;
    }
    return m_sliceResolver ? m_sliceResolver() : m_radioModel->sliceById(0);
}

void SpectrumOverlayPanel::bindToPanSlice()
{
    if (!m_vaxCmb || !m_radioModel) {
        return;
    }

    // Drop any prior Model→Widget subscription. QObject auto-disconnect
    // already handles the destroyed-slice case, but a live slice that is
    // no longer index 0 (after a sliceRemoved shuffle) needs the explicit
    // disconnect.
    if (m_vaxChannelConn) {
        QObject::disconnect(m_vaxChannelConn);
        m_vaxChannelConn = {};
    }
    // Phase 3P-I-a T18 — antenna Model→Widget subscriptions. Same
    // rebind-on-shuffle pattern as VAX above.
    if (m_rxAntConn) { QObject::disconnect(m_rxAntConn); m_rxAntConn = {}; }
    if (m_txAntConn) { QObject::disconnect(m_txAntConn); m_txAntConn = {}; }
    // Band-flyout highlight (2026-09-06). Same rebind-on-shuffle pattern.
    if (m_bandChangedConn) { QObject::disconnect(m_bandChangedConn); m_bandChangedConn = {}; }

    SliceModel* s = resolvedSlice();
    if (s) {
        // Seed the combo with the current model value before wiring up
        // the listener, using the flag pattern so no spurious setVaxChannel
        // is issued back to the slice.
        m_updatingFromModel = true;
        m_vaxCmb->setCurrentIndex(s->vaxChannel());
        m_updatingFromModel = false;

        // Welches Band im Band-Flyout gehoert wird -- von einer
        // AetherSDR-Sichtung angestossen (2026-09-06, "highlight active
        // band"). Ursprünglich an PanadapterModel::bandChanged gebunden
        // versucht -- die Klasse wird aber nirgends instanziiert
        // (RadioModel::addPanadapter() hat keinen einzigen Aufrufer,
        // siehe Nachtrag in der Speicher-Notiz), also nie ausgeloest.
        // SliceModel ist die tatsaechlich lebendige Quelle (wie der
        // Bandklick oben in MainWindow::ensureOverlayPanels schon zeigt).
        setActiveBandHighlight(bandKeyName(bandFromFrequency(s->frequency())));
        m_bandChangedConn = connect(s, &SliceModel::bandChanged,
                                    this, [this](Longpath::Band newBand) {
            setActiveBandHighlight(bandKeyName(newBand));
        });

        m_vaxChannelConn = connect(s, &SliceModel::vaxChannelChanged,
                                   this, [this](int ch) {
            if (!m_vaxCmb) {
                return;
            }
            m_updatingFromModel = true;
            m_vaxCmb->setCurrentIndex(ch);
            m_updatingFromModel = false;
        });

        m_vaxCmb->setEnabled(true);
        m_vaxCmb->setToolTip("Route this slice's RX audio to a VAX channel");
        if (m_rxAntCmb) { m_rxAntCmb->setEnabled(true); }
        if (m_txAntCmb) { m_txAntCmb->setEnabled(true); }

        // Phase 3P-I-a T18 — seed + subscribe antenna combos.
        // SliceModel::rxAntennaChanged also fires from T13's reverse
        // sync (AlexController → slice), so the combo label stays
        // coherent with the per-band state after a band change.
        if (m_rxAntCmb) {
            m_updatingFromModel = true;
            const int idx = m_rxAntCmb->findText(s->rxAntenna());
            if (idx >= 0) { m_rxAntCmb->setCurrentIndex(idx); }
            m_updatingFromModel = false;

            m_rxAntConn = connect(s, &SliceModel::rxAntennaChanged,
                                  this, [this](const QString& ant) {
                if (!m_rxAntCmb) { return; }
                m_updatingFromModel = true;
                const int i = m_rxAntCmb->findText(ant);
                if (i >= 0) { m_rxAntCmb->setCurrentIndex(i); }
                m_updatingFromModel = false;
            });
        }
        if (m_txAntCmb) {
            m_updatingFromModel = true;
            const int idx = m_txAntCmb->findText(s->txAntenna());
            if (idx >= 0) { m_txAntCmb->setCurrentIndex(idx); }
            m_updatingFromModel = false;

            m_txAntConn = connect(s, &SliceModel::txAntennaChanged,
                                  this, [this](const QString& ant) {
                if (!m_txAntCmb) { return; }
                m_updatingFromModel = true;
                const int i = m_txAntCmb->findText(ant);
                if (i >= 0) { m_txAntCmb->setCurrentIndex(i); }
                m_updatingFromModel = false;
            });
        }
    } else {
        // No slice for this pan — typically the pre-connectToRadio() state, or a
        // transient window during slice teardown. The sliceAdded listener
        // installed by setRadioModel() will call us again when a slice
        // comes (back) online.
        m_updatingFromModel = true;
        m_vaxCmb->setCurrentIndex(0);
        m_updatingFromModel = false;
        m_vaxCmb->setEnabled(false);
        m_vaxCmb->setToolTip("VAX channel (waiting for this pan's slice)");
        if (m_rxAntCmb) { m_rxAntCmb->setEnabled(false); }
        if (m_txAntCmb) { m_txAntCmb->setEnabled(false); }
        setActiveBandHighlight(QString());
    }
}

// Phase 3P-I-a T18 — repopulate RX/TX antenna combos from BoardCapabilities.
// Called by MainWindow on connect and on currentRadioChanged. Empty port
// list (HL2/Atlas) hides both rows entirely. After repopulating we reseed
// the current value from the resolved slice so persisted selections survive
// a reconnect.
void SpectrumOverlayPanel::setBoardCapabilities(const BoardCapabilities& caps)
{
    if (!m_rxAntCmb || !m_txAntCmb) { return; }

    // B3: use AntennaPopupBuilder::labels() for the capability-gated list.
    // Derive SkuUiProfile from RadioModel (already accessible) so RX-only
    // labels (EXT1/EXT2/XVTR/BYPS/RX1/RX2) appear when rxOnlyAntennaCount > 0.
    // Falls back to antennaLabels(caps) (ANT1-3 only) when no model is set.
    const SkuUiProfile sku = m_radioModel
        ? skuUiProfileFor(m_radioModel->hardwareProfile().model)
        : SkuUiProfile{};

    const QStringList rxLabels = AntennaPopupBuilder::labels(caps, sku,
        AntennaPopupBuilder::Mode::RX);
    const QStringList txLabels = AntennaPopupBuilder::labels(caps, sku,
        AntennaPopupBuilder::Mode::TX);
    const bool show = !rxLabels.isEmpty();

    // Suppress widget→model echo while we clear + refill the combos.
    // Clearing a combo emits currentTextChanged("") and addItems()
    // emits again; without the guard each setVisible-false path would
    // stomp the resolved slice's antenna setting to empty.
    m_updatingFromModel = true;
    m_rxAntCmb->clear();
    m_txAntCmb->clear();
    m_rxAntCmb->addItems(rxLabels);
    m_txAntCmb->addItems(txLabels);
    m_updatingFromModel = false;

    if (m_rxAntRow) { m_rxAntRow->setVisible(show); }
    if (m_txAntRow) { m_txAntRow->setVisible(show); }

    // Reseed from the resolved slice so the combo label matches persisted state.
    if (show && m_radioModel) {
        if (SliceModel* s = resolvedSlice()) {
            m_updatingFromModel = true;
            const int rxIdx = m_rxAntCmb->findText(s->rxAntenna());
            if (rxIdx >= 0) { m_rxAntCmb->setCurrentIndex(rxIdx); }
            const int txIdx = m_txAntCmb->findText(s->txAntenna());
            if (txIdx >= 0) { m_txAntCmb->setCurrentIndex(txIdx); }
            m_updatingFromModel = false;
        }
    }
}

void SpectrumOverlayPanel::toggleVaxFlyout()
{
    // Button index 5 (VAX)
    QPushButton* vaxBtn = m_menuBtns[5];

    if (m_activeFlyout == m_vaxFlyout) {
        hideFlyout();
        return;
    }
    hideFlyout();

    // Center vertically on VAX button (from AetherSDR toggleDaxPanel)
    int btnCenterY = vaxBtn->y() + vaxBtn->height() / 2;
    int panelY = y() + btnCenterY - m_vaxFlyout->sizeHint().height() / 2;
    m_vaxFlyout->move(x() + width(), std::max(0, panelY));
    m_vaxFlyout->raise();
    m_vaxFlyout->show();
    m_activeFlyout = m_vaxFlyout;
    m_activeButton = vaxBtn;
    vaxBtn->setStyleSheet(OverlayColors::kMenuBtnActive);
}

void SpectrumOverlayPanel::wheelEvent(QWheelEvent* event) { event->accept(); }
void SpectrumOverlayPanel::mousePressEvent(QMouseEvent* event) { event->accept(); }
void SpectrumOverlayPanel::mouseReleaseEvent(QMouseEvent* event) { event->accept(); }

// ── Waterfall zoom buttons ─────────────────────────────────────────────────────
// Four small buttons [S][B][-][+] docked at bottom-left of the spectrum widget.
// [S] = Segment zoom (fit visible slice passband), [B] = Band zoom (fit whole band),
// [-] = zoom out, [+] = zoom in.
// From AetherSDR SpectrumOverlayMenu.cpp zoom controls section.

void SpectrumOverlayPanel::buildZoomButtons()
{
    // Parent: same spectrum widget as the panel (parentWidget())
    m_zoomStrip = new QWidget(parentWidget());
    m_zoomStrip->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_zoomStrip->setAttribute(Qt::WA_NoSystemBackground, true);
    // NICHT nativ — und das ist seit dem 2026-08-22 wieder moeglich.
    //
    // Vorgeschichte: am 2026-08-20 war der Streifen unsichtbar (der
    // Panadapter ist ein QRhiWidget mit WA_NativeWindow, und ein
    // natives NSView zeichnet ueber allen nicht-nativen Geschwistern).
    // Der Notbehelf war, den Streifen ebenfalls nativ zu machen —
    // native Geschwister sortieren sich untereinander, raise() greift
    // wieder, sichtbar war er.
    //
    // Nur: KLICKBAR war er nicht mehr. Gemessen am 2026-08-22, weil der
    // Betreiber fragte, ob Plus und Minus ueberhaupt etwas tun: der
    // Knopf leuchtete beim Ueberfahren NICHT, und im Protokoll kam kein
    // einziger Druck an. Sichtbar und taub — die schlechteste aller
    // Kombinationen, weil nichts darauf hinweist.
    //
    // Weg damit. Moeglich wurde das durch den Rueckport von
    // applyNativeWindowIsolationPolicy() am selben Tag: seither steht
    // WA_DontCreateNativeAncestors immer gepaart mit WA_NativeWindow
    // (AetherSDR #4339), die Vorfahren werden nicht mehr
    // mit-nativiert, und ein gewoehnliches Kind ist wieder sichtbar.
    //
    // AetherSDR macht es genauso: seine Zoomknoepfe sind schlichte
    // Kinder des SpectrumWidget (SpectrumWidget.cpp:2125-2154
    // [@0cd4559]), ohne eigenen nativen Streifen.
    //
    // WER DAS WIEDER EINSCHALTET, macht die Knoepfe erneut taub.

    static constexpr int kZBtnW = 24;
    static constexpr int kZBtnH = 20;
    static constexpr int kZGap  = 2;

    // "padding: 0; margin: 0; min-width: 0;" wie im Vorbild (AetherSDR
    // SpectrumWidget.cpp:2162-2168 [@d58e2b8a]) — beim Uebernehmen
    // verloren gegangen. Seit dem app-weiten Grundstil vom 2026-09-18
    // ("Glas & Tiefe", Style::kButtonStyle) erbt jeder QPushButton ohne
    // eigene Angabe "padding: 4px 12px"; bei 24 px Breite blieb fuer das
    // Zeichen nichts uebrig, und der Betreiber sah vier leere Kaestchen
    // (2026-09-25). Pruefung: tst_zoom_buttons_legible.
    const QString zBtnStyle =
        "QPushButton { background: rgba(20, 30, 45, 200); "
        "border: 1px solid rgba(255, 255, 255, 40); border-radius: 6px; "
        "color: #c8d8e8; font-size: 11px; font-weight: bold; "
        "padding: 0; margin: 0; min-width: 0; }"
        "QPushButton:hover { background: rgba(0, 112, 192, 180); "
        "border: 1px solid #4a7ba8; }"
        "QPushButton:pressed { background: rgba(0, 144, 224, 200); }";

    auto makeZBtn = [&](const QString& text) -> QPushButton* {
        auto* btn = new QPushButton(text, m_zoomStrip);
        btn->setFixedSize(kZBtnW, kZBtnH);
        btn->setStyleSheet(zBtnStyle);
        return btn;
    };

    // Namen wie im Original (AetherSDR SpectrumWidget.cpp:2151-2154
    // [@0cd4559]) — damit Pruefungen und Barrierefreiheit den richtigen
    // Knopf finden. Es gibt mehrere "+" im Fenster; ohne Namen greift
    // eine Pruefung zuverlaessig daneben (erlebt am 2026-08-22).
    m_zoomSegBtn  = makeZBtn(QStringLiteral("S"));
    m_zoomBandBtn = makeZBtn(QStringLiteral("B"));
    // Echtes Minuszeichen U+2212 wie im Vorbild (AetherSDR
    // SpectrumWidget.cpp:2201 [@d58e2b8a]); der Bindestrich ist bei
    // 11 px nur 4x4 Pixel gross und neben dem "+" kaum zu sehen.
    m_zoomOutBtn  = makeZBtn(QStringLiteral("−"));
    m_zoomInBtn   = makeZBtn(QStringLiteral("+"));

    m_zoomSegBtn->setToolTip(QStringLiteral("Segment zoom — fit visible slice passband"));
    m_zoomSegBtn->setObjectName(QStringLiteral("panZoomSegBtn"));
    m_zoomBandBtn->setObjectName(QStringLiteral("panZoomBandBtn"));
    m_zoomOutBtn->setObjectName(QStringLiteral("panZoomOutBtn"));
    m_zoomInBtn->setObjectName(QStringLiteral("panZoomInBtn"));
    m_zoomSegBtn->setAccessibleName(QStringLiteral("Zoom to segment"));
    m_zoomBandBtn->setAccessibleName(QStringLiteral("Zoom to band"));
    m_zoomOutBtn->setAccessibleName(QStringLiteral("Zoom out"));
    m_zoomInBtn->setAccessibleName(QStringLiteral("Zoom in"));

    m_zoomBandBtn->setToolTip(QStringLiteral("Band zoom — fit entire amateur band"));
    m_zoomOutBtn->setToolTip(QStringLiteral("Zoom out — increase visible bandwidth"));
    m_zoomInBtn->setToolTip(QStringLiteral("Zoom in — decrease visible bandwidth"));

    connect(m_zoomSegBtn,  &QPushButton::clicked, this, &SpectrumOverlayPanel::zoomSegment);
    connect(m_zoomBandBtn, &QPushButton::clicked, this, &SpectrumOverlayPanel::zoomBand);
    connect(m_zoomOutBtn,  &QPushButton::clicked, this, &SpectrumOverlayPanel::zoomOut);
    connect(m_zoomInBtn,   &QPushButton::clicked, this, &SpectrumOverlayPanel::zoomIn);

    // Lay out horizontally inside the strip
    int xOff = kZGap;
    for (QPushButton* btn : {m_zoomSegBtn, m_zoomBandBtn, m_zoomOutBtn, m_zoomInBtn}) {
        btn->move(xOff, kZGap);
        xOff += kZBtnW + kZGap;
    }
    m_zoomStrip->setFixedSize(xOff, kZBtnH + 2 * kZGap);
    m_zoomStrip->show();
    m_zoomStrip->raise();

    repositionZoomButtons();
}

void SpectrumOverlayPanel::repositionZoomButtons()
{
    if (!m_zoomStrip || !parentWidget()) { return; }
    QWidget* spectrum = parentWidget();
    // Place at bottom-left of the waterfall area, above the frequency scale
    // (approximate: leave 32px from bottom for frequency scale bar)
    static constexpr int kFreqScaleH = 32;
    static constexpr int kMargin     = 4;
    int x = kMargin;
    int y = spectrum->height() - kFreqScaleH - m_zoomStrip->height() - kMargin;
    y = std::max(kMargin, y);
    m_zoomStrip->move(x, y);
    m_zoomStrip->raise();
}

// ── Clarity status badge (Phase 3G-9c) ───────────────────────────────────────

void SpectrumOverlayPanel::setClarityStatus(bool active, bool paused)
{
    if (!m_clarityBadge) { return; }
    if (!active && !paused) {
        m_clarityBadge->hide();
        return;
    }
    const QString color = paused
        ? QStringLiteral("#c2924f")   // amber
        : QString::fromLatin1(Style::kDspToggleBorder);  // green
    m_clarityBadge->setStyleSheet(Style::themed(
        QStringLiteral("QLabel { background: %1; color: #08080a; "
                        "border-radius: 6px; font-size: 11px; "
                        "font-weight: bold; }").arg(color)));
    m_clarityBadge->show();
}

void SpectrumOverlayPanel::setSpectrumRenderModeIndex(int renderModeIndex)
{
    if (!m_renderModeCmb) { return; }
    if (renderModeIndex < 0 || renderModeIndex >= m_renderModeCmb->count()) { return; }
    m_renderModeCmb->setCurrentIndex(renderModeIndex);
}

void SpectrumOverlayPanel::setWfGainValue(int gain)
{
    if (!m_wfGainSlider) { return; }
    m_wfGainSlider->setValue(gain);
}

void SpectrumOverlayPanel::setWfBlackLevelValue(int level)
{
    if (!m_wfBlackSlider) { return; }
    m_wfBlackSlider->setValue(level);
}

void SpectrumOverlayPanel::setColorSchemeIndex(int index)
{
    if (!m_colorSchemeCmb) { return; }
    if (index < 0 || index >= m_colorSchemeCmb->count()) { return; }
    m_colorSchemeCmb->setCurrentIndex(index);
}

} // namespace Longpath
