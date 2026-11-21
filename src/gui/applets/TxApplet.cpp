// =================================================================
// src/gui/applets/TxApplet.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs — chkMOX_CheckedChanged2
//   (29311-29678 [v2.10.3.13]) and chkTUN_CheckedChanged (29978-30157
//   [v2.10.3.13]); original licence from Thetis source is included below.
//
// Layout from AetherSDR TxApplet.{h,cpp} (GPLv3, see AetherSDR attribution
// block below).
//
// =================================================================
// Modification history (Longpath):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Layout pattern from AetherSDR `src/gui/TxApplet.{h,cpp}`.
//                 Wiring deferred to Phase 3M.
//   2026-04-26 — Phase 3M-1a H.3: deep-wired TUNE/MOX/Tune-Power/RF-Power.
//                 Out-of-phase controls (2-Tone, PS-A) hidden.
//                 syncFromModel() implemented. setCurrentBand(Band) added.
//   2026-04-28 — Phase 3M-1b J.2: VOX toggle button added below Tune Power.
//                 Checkable, green border when active. Bidirectional with
//                 TransmitModel::voxEnabled. Right-click opens VoxSettingsPopup.
//                 (REMOVED 2026-05-03 in 3M-3a-iii Task 16 — see entry below.)
//   2026-04-28 — Phase 3M-1b J.3: MON toggle button + monitor volume slider
//                 added below VOX. Bidirectional with TransmitModel::monEnabled
//                 and monitorVolume (default 0.5f, Thetis audio.cs:417). Mic-source
//                 badge added above the gauges ("PC mic"/"Radio mic"), driven by
//                 TransmitModel::micSourceChanged. Phase J complete.
//   2026-04-28 — Phase 3M-1b (relocation): Mic Gain slider row (J.1) removed from
//                 TxApplet. Relocated to PhoneCwApplet (#5 slot) per JJ feedback.
//   2026-04-28 — Phase 3M-1b K.2: tooltipForMode(DSPMode) helper + onMoxModeChanged
//                 slot implemented. Wired to SliceModel::dspModeChanged via
//                 RadioModel active-slice accessor so the MOX button tooltip
//                 reflects the rejection reason for CW/AM/FM/etc. modes.
//                 Closes Phase K.
//   2026-04-30 — Phase 3M-3a-ii Batch 6 (Task F + A): PROC button enabled and
//                 bidirectionally wired to TransmitModel::cpdrOn.  CFC button
//                 added next to PROC, bidirectional with cfcEnabled, right-
//                 click opens modeless TxCfcDialog (single instance kept alive
//                 across opens — same pattern as TxEqDialog).
//                 requestOpenCfcDialog() public slot exposed so CfcSetupPage's
//                 [Configure CFC bands…] button routes to the same dialog
//                 instance via a MainWindow-side signal connection.
//   2026-05-02 — Plan 4 Cluster C (Task 4 / D2+D3+D9-status): TX BW spinbox
//                 row added below Profile combo — Lo/Hi QSpinBox pair
//                 bidirectional with TransmitModel::filterLow/filterHigh;
//                 orange status label shows filter description text.
//                 Wired via filterChanged(int,int) + dspModeChanged refresh.
//                 syncFromModel() extended to seed spinboxes + status label.
//   2026-05-04 — Phase 3M-3a-iii bench polish: VOX row relocated from
//                 PhoneCwApplet Phone tab (Control #10) back to TxApplet as
//                 a full row directly under TUNE/MOX (Option B - full row).
//                 VOX button + threshold slider + DexpPeakMeter strip + Hold
//                 slider all move as a unit, plus the 100 ms peak-meter
//                 poller (now TxApplet::pollVoxMeter) and the right-click
//                 → Setup → Transmit → DEXP/VOX signal handling.  DEXP row
//                 stays on PhoneCwApplet — only VOX moves.
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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

// =================================================================
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   Layout pattern from AetherSDR `src/gui/TxApplet.{h,cpp}`.
//   AetherSDR is licensed under the GNU General Public License v3.
//   Longpath is also GPLv3. Attribution follows GPLv3 §5 requirements.
// =================================================================

// TxApplet — TX control panel.
// Phase 3M-1a H.3: TUNE/MOX/Tune-Power/RF-Power deep-wired.
// Phase 3M-1b J.3: MON toggle + monitor volume slider + mic-source badge wired.
// Phase 3M-3a-iii bench polish (2026-05-04): VOX row relocated back from
//   PhoneCwApplet — full row [VOX btn][threshold + peak strip][Hold slider].
// Out-of-phase controls (2-Tone, PS-A) hidden.
//
// Control inventory:
//  0.  Mic-source badge  — read-only "PC mic"/"Radio mic"  [WIRED — 3M-1b J.3]
//  1.  Fwd Power gauge   — HGauge 0–120 W, redStart 100 W
//  2.  SWR gauge         — HGauge 1.0–3.0, redStart 2.5
//  3.  RF Power slider   + label + value  [WIRED — 3M-1a H.3]
//  4.  Tune Power slider + label + value  [WIRED — 3M-1a H.3]
//      (Mic Gain slider relocated to PhoneCwApplet — 2026-04-28 relocation)
//  4a. TUNE + MOX button row [WIRED — 3M-1a H.3]
//  4b. VOX row — [VOX btn][Threshold slider + DexpPeakMeter strip][-N dB]
//      [Hold slider 1..2000 ms][N ms]  [WIRED — 3M-3a-iii bench polish]
//      Bidirectional with voxEnabled / voxThresholdDb / voxHangTimeMs.
//      Right-click VOX → openSetupRequested("Transmit", "DEXP/VOX").
//      Relocated from PhoneCwApplet Phone tab Control #10.
//  4c. MON toggle button — checkable, blue:checked style  [WIRED — 3M-1b J.3]
//      Bidirectional with TransmitModel::monEnabled (default false).
//  4d. Monitor volume slider — 0..100 → monitorVolume 0.0..1.0  [WIRED — 3M-1b J.3]
//      Default 50 (model default 0.5f, Thetis audio.cs:417).
//  5.  MOX button        — checkable, red:checked style  [WIRED — 3M-1a H.3]
//  6.  TUNE button       — checkable, red:checked + "TUNING..." text  [WIRED — 3M-1a H.3]
//  7.  ATU button        — checkable (NYI — 3M-2/3M-3)
//  8.  MEM button        — checkable (NYI — 3M-2/3M-3)
//  9.  TX Profile combo  — (NYI — 3M-3)
// 10.  Tune mode combo   — (NYI — 3M-3)
// 11.  2-Tone test       — HIDDEN until Phase 3M-3
// 12.  PS-A toggle       — HIDDEN until Phase 3M-4
// 13.  DUP               — checkable (NYI — 3M-3)
// 14.  xPA indicator     — checkable (NYI — 3M-3)
// 15.  SWR protection LED — QLabel indicator (NYI — 3M-3)

#include "TxApplet.h"
#include "gui/styles/ThemeQss.h"
#include "TxEqDialog.h"
#include "TxCfcDialog.h"
#include "NyiOverlay.h"
#include "gui/HGauge.h"
#include "gui/MikroAusfallText.h"
#include "gui/StyleConstants.h"
#include "gui/ComboStyle.h"
#include "gui/widgets/DexpPeakMeter.h"
#include "core/AudioEngine.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "core/MicProfileManager.h"
#include "core/MoxController.h"
#include "core/PureSignal.h"
#include "core/RadioStatus.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <QComboBox>
#include <QContextMenuEvent>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QDateTime>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {
// Breite der Beschriftungsspalte im Messblock (Vorlauf, SWR, Leistung,
// Tune): eine Zahl fuer alle vier, damit die Rinnen buendig stehen.
constexpr int kTxLabelW = 62;
} // namespace

TxApplet::TxApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
    wireControls();
}

// ── Die Sendebandbreiten zum Durchklicken ────────────────────────────
//
// Betreiber, 2026-10-16: „nur das ich anklicken kann die
// unterschiedlichen bandbreiten durchklicken kann" — und „5k kannst du
// auch bauen".
//
// Die unteren Grenzen: 100 Hz fuer die schmalen (so steht es in den
// mitgelieferten Profilen), 50 Hz fuer 3.6k (die Breite aus Martins
// ESSB-Profil) und 40 Hz fuer die beiden breiten. Die 40 kommen von
// Zeus, dessen „eSSB Wide" auf lowCutHz 40 / highCutHz 5000 steht —
// das ist dort die breiteste Stufe ueberhaupt. 8k gibt es nirgends
// zum Abschreiben; der Feinblatt-Spinner laesst bis 10000 zu, und
// 40..8000 ist die naheliegende Fortsetzung der Reihe.
const TxApplet::TxBandbreite TxApplet::kTxBandbreiten[6] = {
    { "2.8k", 100, 2900 },
    { "3.0k", 100, 3100 },
    { "3.3k", 100, 3400 },
    { "3.6k",  50, 3650 },
    { "5.0k",  40, 5000 },
    { "8.0k",  40, 8000 },
};

int TxApplet::stufeFuer(int lowHz, int highHz)
{
    for (int i = 0; i < 6; ++i) {
        if (kTxBandbreiten[i].lowHz == lowHz
            && kTxBandbreiten[i].highHz == highHz) {
            return i;
        }
    }
    return -1;
}

// Steht eine Breite auf keiner Stufe (im Feinblatt von Hand gesetzt
// oder aus einem fremden Profil), geht der naechste Klick auf die
// naechstGROESSERE Stufe — und nicht zurueck auf die schmalste, was
// mitten im Betrieb die Bandbreite zusammenziehen wuerde.
int TxApplet::naechsteStufe(int lowHz, int highHz)
{
    const int genau = stufeFuer(lowHz, highHz);
    if (genau >= 0) { return (genau + 1) % 6; }

    const int breite = highHz - lowHz;
    for (int i = 0; i < 6; ++i) {
        if (kTxBandbreiten[i].highHz - kTxBandbreiten[i].lowHz > breite) {
            return i;
        }
    }
    return 0;   // breiter als alle Stufen: wieder von vorn
}

void TxApplet::setzeBandbreite(int index)
{
    if (!m_model || index < 0 || index >= 6) { return; }
    TransmitModel& tx = m_model->transmitModel();
    // BEIDE Grenzen setzen: nur die obere zu aendern liesse den
    // Hochpass der vorigen Stufe stehen, und 40 Hz unten klingen
    // anders als 100.
    tx.setFilterLow(kTxBandbreiten[index].lowHz);
    tx.setFilterHigh(kTxBandbreiten[index].highHz);
}

void TxApplet::zeigeBandbreitenMenue(const QPoint& pos)
{
    if (!m_model) { return; }
    const TransmitModel& tx = m_model->transmitModel();
    const int jetzt = stufeFuer(tx.filterLow(), tx.filterHigh());

    QMenu menue(this);
    for (int i = 0; i < 6; ++i) {
        QAction* a = menue.addAction(QStringLiteral("%1   %2–%3 Hz")
            .arg(QLatin1String(kTxBandbreiten[i].name))
            .arg(kTxBandbreiten[i].lowHz)
            .arg(kTxBandbreiten[i].highHz));
        a->setCheckable(true);
        a->setChecked(i == jetzt);
        connect(a, &QAction::triggered, this, [this, i]() { setzeBandbreite(i); });
    }
    menue.exec(pos);
}

void TxApplet::buildUI()
{
    // ══════════════════════════════════════════════════════════════════════
    // Umbau 2026-09-02 — Entwurf „Sendeleiste", Ton „Messing".
    //
    // Vorher stapelte dieses Feld VIERZEHN Zeilen in einer Spalte und
    // brauchte dafuer rund 420 Punkt Hoehe. Angedockt sind 154 da: MOX
    // und TUNE standen an der Unterkante, VOX und MON schon hinter dem
    // Rollbalken — ausgerechnet die Knoepfe, wegen derer man das Feld
    // aufmacht. Gleichzeitig lagen 520 Punkt Breite brach.
    //
    // Der Umbau raeumt in drei Baendern:
    //   1. Messblock links (Vorlauf und SWR mit ZAHL daneben, darunter
    //      die zwei Leistungsregler), Sendetasten rechts.
    //   2. eine Reihe gleich aussehender Zustandsschalter.
    //   3. eine Fusszeile: Mikrofonquelle, TX-Bandbreite, SWR-Schutz.
    //
    // Was seltener gebraucht wird — Profil, TX-Bandbreite, VOX-Schwelle
    // und -Haltezeit, Mithoerlautstaerke — steht im Feinblatt hinter
    // dem Zahnrad. Es sind DIESELBEN Bauteile, nur umgehaengt: die
    // Verdrahtung in wireControls() und jeder Test, der sie ueber
    // objectName findet, bleiben unberuehrt.
    //
    // Der Ton: bis heute trug MOX kTxRed — dieselbe Farbfamilie wie die
    // SWR-Warnung zwei Zeilen darueber. Jetzt traegt die Sendetaste
    // Messing (Style::txKeyStyle) und Rot bedeutet im Feld wieder genau
    // eine Sache: es stimmt etwas nicht.
    // ══════════════════════════════════════════════════════════════════════

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* body = new QWidget(this);
    // Durchsichtig (Glas & Tiefe, 2026-09-17): die Platte mit ihrem
    // Verlauf malt das Fenster bzw. die Zelle, nicht das Applet.
    body->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(6, 5, 6, 5);
    vbox->setSpacing(5);
    outer->addWidget(body);

    // Ein Zustandsschalter: gleiche Groesse, gleiche Bauform, gleicher
    // Einrast-Ton. Dass sie alle gleich aussehen, IST die Aussage —
    // vorher hatte dieses Feld vier verschiedene Einrast-Faerbungen
    // (VOX gruen, MON dunkelblau, 2-Tone rot, PS-A dunkelgruen).
    auto schalter = [this](const QString& text) {
        auto* b = new QPushButton(text, this);
        b->setCheckable(true);
        b->setFixedHeight(20);
        b->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        b->setStyleSheet(Style::buttonBaseStyle()
                         + QStringLiteral("QPushButton { padding: 1px 3px; }")
                         + Style::quietCheckedStyle());
        return b;
    };

    // ── Band 1: Messblock links, Sendetasten rechts ─────────────────────────
    auto* oben = new QHBoxLayout;
    oben->setSpacing(8);

    auto* mess = new QVBoxLayout;
    mess->setSpacing(2);

    // ── Vorlauf ── per-SKU-Skala ────────────────────────────────────────────
    // Ticks bei der Konstruktion wie beim 100-W-Geraet der Hermes-Klasse;
    // rescaleFwdGaugeForModel() zieht die Skala nach, sobald
    // RadioModel::currentRadioChanged feuert (verdrahtet in wireControls()).
    // Bench #167: ein HL2 mit 1-5 W auf einer 0-120-W-Skala war ein kaum
    // sichtbarer Span.
    auto* fwdGauge = new HGauge(this);
    // Englisch seit 2026-10-15 („englisch durchgehend"); die
// Breitenrechnung im Kommentar darunter galt dem laengeren
// Wortlaut „VORLAUF" und bleibt damit auf der sicheren Seite.
    fwdGauge->setTitle(tr("FWD"));
    // 62, nicht 52: die Versalzeile mit Laufweite (Glas & Tiefe,
    // 2026-09-17) braucht fuer "VORLAUF" rund 58 Punkte — bei 52 stand
    // "VORLAUI" auf dem Blatt. Dieselbe Breite wie die Reglerzeilen
    // darunter, damit Rinnen und Regler buendig beginnen.
    fwdGauge->setLabelWidth(kTxLabelW);
    fwdGauge->setReadout(true, 1, QStringLiteral("W"));
    fwdGauge->setAccessibleName(QStringLiteral("Forward power gauge"));
    m_fwdPowerGauge = fwdGauge;
    rescaleFwdGaugeForModel(HPSDRModel::FIRST);   // Platzhalter, bis das Geraet bekannt ist
    mess->addWidget(fwdGauge);

    // ── SWR ─────────────────────────────────────────────────────────────────
    // Die Vorwarnung ist neu: bis zum Umbau standen setYellowStart und
    // setRedStart BEIDE auf 2,5 — das gelbe Band war null breit, es gab
    // also keine Stufe zwischen „gut" und „zu viel". Jetzt faerbt sich
    // der Balken ab 2,0 und wird ab 2,5 zur Warnung.
    auto* swrGauge = new HGauge(this);
    swrGauge->setRange(1.0, 3.0);
    swrGauge->setYellowStart(2.0);
    swrGauge->setRedStart(2.5);
    swrGauge->setTitle(QStringLiteral("SWR"));
    swrGauge->setLabelWidth(kTxLabelW);
    swrGauge->setReadout(true, 1);
    swrGauge->setAccessibleName(QStringLiteral("SWR gauge"));
    m_swrGauge = swrGauge;
    mess->addWidget(swrGauge);

    // ── Leistung ────────────────────────────────────────────────────────────
    {
        m_rfPowerSlider = new QSlider(Qt::Horizontal, this);
        m_rfPowerSlider->setRange(0, 100);
        // Dieselbe Vorgabe wie im Modell. Hier stand 100 — ueberschrieben
        // wurde es gleich darauf ohnehin, aber bis dahin zeigte die
        // Bedienung volle Leistung an.
        m_rfPowerSlider->setValue(TransmitModel::kVorgabeLeistungProzent);
        m_rfPowerSlider->setAccessibleName(QStringLiteral("RF power"));
        m_rfPowerSlider->setObjectName(QStringLiteral("TxRfPowerSlider"));

        m_rfPowerValue = new QLabel(
            QString::number(TransmitModel::kVorgabeLeistungProzent), this);
        auto* row = sliderRow(tr("Power"), m_rfPowerSlider,
                              m_rfPowerValue, kTxLabelW);
        // Auf dem HL2 steht hier „-7.5 dB" statt einer nackten Zahl —
        // die 36 Punkt aus sliderRow() schneiden das ab.
        m_rfPowerValue->setFixedWidth(46);
        m_rfPowerSlider->setFixedHeight(16);
        mess->addLayout(row);
    }

    // ── Tune-Leistung ───────────────────────────────────────────────────────
    {
        m_tunePwrSlider = new QSlider(Qt::Horizontal, this);
        m_tunePwrSlider->setRange(0, kTuneSliderMaxWatts);
        m_tunePwrSlider->setValue(10);
        m_tunePwrSlider->setAccessibleName(QStringLiteral("Tune power"));
        // Der Hinweistext kommt aus rescalePowerSlidersForModel()
        // (Issue #175 Task 7) und wird bei jedem SKU-Wechsel neu gesetzt.

        m_tunePwrValue = new QLabel(QStringLiteral("10"), this);
        auto* row = sliderRow(QStringLiteral("Tune"), m_tunePwrSlider,
                              m_tunePwrValue, kTxLabelW);
        m_tunePwrValue->setFixedWidth(46);
        m_tunePwrSlider->setFixedHeight(16);
        mess->addLayout(row);
    }

    // ── Mikrofonaussteuerung, nur am Flex-Weg (2026-10-29) ──────────────────
    //
    // Der Flex-Sendeweg geht an keiner WDSP-Kette vorbei, sondern roh
    // von der Mikrofonbuchse an die Maschine — und hat deshalb als
    // einziger Sendeweg gar keine Verstaerkungsstufe. Am Geraet
    // gemessen: dasselbe Programm, dieselbe Einstellung, zwei
    // Mikrofone, +2,54 dBFS gegen −9,72 dBFS, 3,4 W gegen 73 mW.
    //
    // Darum steht der Regler hier und nirgends sonst: an den anderen
    // Geraeten macht die Kette das, und zwei Regler fuer dieselbe Sache
    // waeren einer zu viel. Er zeigt sich erst, wenn eine Flex
    // verbunden ist (siehe zeigeFlexMikRegler()).
    {
        m_flexMikSlider = new QSlider(Qt::Horizontal, this);
        m_flexMikSlider->setRange(-20, 30);
        m_flexMikSlider->setValue(0);
        m_flexMikSlider->setAccessibleName(QStringLiteral("Flex mic gain"));
        m_flexMikSlider->setObjectName(QStringLiteral("TxFlexMicGainSlider"));
        m_flexMikSlider->setToolTip(
            QStringLiteral("Microphone gain for the FlexRadio transmit path "
                           "(dB). The radio does the SSB processing itself; "
                           "this only sets how hot Longpath feeds it. Peaks "
                           "are limited at -1 dBFS."));

        m_flexMikValue = new QLabel(QStringLiteral("0 dB"), this);
        auto* row = sliderRow(QStringLiteral("Mic"), m_flexMikSlider,
                              m_flexMikValue, kTxLabelW);
        m_flexMikValue->setFixedWidth(46);
        m_flexMikSlider->setFixedHeight(16);
        mess->addLayout(row);

        m_flexMikRow = row;
        m_flexMikSlider->setVisible(false);
        m_flexMikValue->setVisible(false);

        connect(m_flexMikSlider, &QSlider::valueChanged, this, [this](int db) {
            m_flexMikValue->setText(QStringLiteral("%1 dB").arg(db));
            if (m_model) { m_model->setzeFlexMikGainDb(db); }
        });

        // Sichtbar wird der Regler, sobald eine Flex sich angemeldet
        // hat, und wieder unsichtbar beim Trennen. Ein Regler, der an
        // einem ANAN nichts tut, gehoert dort auch nicht hin.
        if (m_model) {
            auto zeige = [this](bool an) {
                if (!m_flexMikSlider) { return; }
                m_flexMikSlider->setVisible(an);
                m_flexMikValue->setVisible(an);
                if (an && m_model) {
                    const QSignalBlocker sperre(m_flexMikSlider);
                    m_flexMikSlider->setValue(m_model->flexMikGainDb());
                    m_flexMikValue->setText(
                        QStringLiteral("%1 dB").arg(m_model->flexMikGainDb()));
                }
            };
            connect(m_model, &RadioModel::flexGuiClientRegistered, this,
                    [zeige](const QString&) { zeige(true); });
            connect(m_model, &RadioModel::connectionStateChanged, this,
                    [zeige](ConnectionState lage) {
                if (lage == ConnectionState::Disconnected) { zeige(false); }
            });
        }
    }
    oben->addLayout(mess, 1);

    // ── Die Sendetasten ─────────────────────────────────────────────────────
    // MOX ist die groesste Flaeche im Feld und die einzige kraeftige.
    // TUNE ist dieselbe Art Handlung, eine Stufe leiser.
    {
        auto* tasten = new QVBoxLayout;
        tasten->setSpacing(4);

        m_moxBtn = new QPushButton(QStringLiteral("MOX"), this);
        m_moxBtn->setCheckable(true);
        m_moxBtn->setFixedSize(104, 58);
        m_moxBtn->setStyleSheet(Style::buttonBaseStyle()
            + QStringLiteral("QPushButton { font-size: %1px; }")
                  .arg(Style::kFontSub)
            + Style::txKeyStyle());
        m_moxBtn->setEnabled(true);
        m_moxBtn->setAccessibleName(QStringLiteral("MOX transmit"));
        m_moxBtn->setToolTip(QStringLiteral("Manual transmit (MOX)"));
        tasten->addWidget(m_moxBtn);

        m_tuneBtn = new QPushButton(QStringLiteral("TUNE"), this);
        m_tuneBtn->setCheckable(true);
        m_tuneBtn->setFixedSize(104, 24);
        m_tuneBtn->setStyleSheet(Style::buttonBaseStyle() + Style::txKeyStyle(true));
        m_tuneBtn->setEnabled(true);
        m_tuneBtn->setAccessibleName(QStringLiteral("Tune carrier"));
        m_tuneBtn->setToolTip(QStringLiteral("Enable TUNE carrier (single-tone CW)"));
        tasten->addWidget(m_tuneBtn);

        oben->addLayout(tasten);
    }
    vbox->addLayout(oben);

    // ── Band 2: die Zustandsschalter ────────────────────────────────────────
    {
        auto* reihe = new QHBoxLayout;
        reihe->setSpacing(3);

        m_voxBtn = schalter(QStringLiteral("VOX"));
        m_voxBtn->setAccessibleName(QStringLiteral("VOX voice-operated transmit"));
        m_voxBtn->setObjectName(QStringLiteral("TxVoxButton"));
        m_voxBtn->setToolTip(QStringLiteral(
            "VOX — voice-operated transmit.  Left-click to toggle.\n"
            "Right-click to open the DEXP/VOX setup page."));
        m_voxBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        reihe->addWidget(m_voxBtn, 1);

        m_monBtn = schalter(QStringLiteral("MON"));
        m_monBtn->setChecked(false);   // Plan §0 Zeile 9: laedt IMMER aus
        m_monBtn->setAccessibleName(QStringLiteral("Monitor enable"));
        m_monBtn->setToolTip(QStringLiteral(
            "Monitor: mix received audio into headphones during TX.\n"
            "Right-click for the monitor volume.\n"
            "Does NOT persist across restarts (safety)."));
        m_monBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        reihe->addWidget(m_monBtn, 1);

        m_levBtn = schalter(QStringLiteral("LEV"));
        m_levBtn->setObjectName(QStringLiteral("TxLevButton"));
        m_levBtn->setAccessibleName(QStringLiteral("TX leveler"));
        m_levBtn->setToolTip(QStringLiteral("TX leveler (ALC-style gain rider)"));
        reihe->addWidget(m_levBtn, 1);

        m_eqBtn = schalter(QStringLiteral("EQ"));
        m_eqBtn->setObjectName(QStringLiteral("TxEqButton"));
        m_eqBtn->setAccessibleName(QStringLiteral("TX equaliser"));
        m_eqBtn->setToolTip(QStringLiteral(
            "TX equaliser.  Left-click to toggle.  Right-click to configure."));
        m_eqBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        reihe->addWidget(m_eqBtn, 1);

        m_cfcBtn = schalter(QStringLiteral("CFC"));
        m_cfcBtn->setObjectName(QStringLiteral("TxCfcButton"));
        m_cfcBtn->setAccessibleName(QStringLiteral("TX continuous frequency compressor"));
        m_cfcBtn->setToolTip(QStringLiteral(
            "Continuous frequency compressor.  Left-click to toggle.  "
            "Right-click to configure."));
        m_cfcBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        reihe->addWidget(m_cfcBtn, 1);

        // 2-Tone bringt das Geraet AUF DIE LUFT — es gehoert damit zur
        // Sendetasten-Familie, nicht zu den stillen Zustandsschaltern.
        m_twoToneBtn = schalter(QStringLiteral("2-Tone"));
        m_twoToneBtn->setStyleSheet(Style::buttonBaseStyle()
            + QStringLiteral("QPushButton { padding: 1px 3px; }")
            + Style::txKeyStyle(true));
        m_twoToneBtn->setAccessibleName(QStringLiteral("2-tone test"));
        m_twoToneBtn->setToolTip(QStringLiteral(
            "Continuous or pulsed two-tone IMD test "
            "(configure in Setup → Test → Two-Tone)."));
        reihe->addWidget(m_twoToneBtn, 1);

        // PS-A: Sichtbarkeit haengt an setBoardCapabilities(); sichtbar
        // voreingestellt wegen des am Pruefstand beobachteten Wettlaufs
        // (die Faehigkeiten treffen manchmal erst nach dem Bau ein).
        m_psaBtn = schalter(QStringLiteral("PS-A"));
        m_psaBtn->setObjectName(QStringLiteral("TxAppletPsaBtn"));
        m_psaBtn->setAccessibleName(QStringLiteral("PS-A PureSignal"));
        m_psaBtn->setToolTip(QStringLiteral(
            "Toggle PureSignal auto-calibration. Right-click to open PureSignal..."));
        reihe->addWidget(m_psaBtn, 1);

        // Das Zahnrad oeffnet das Feinblatt. Kein Rastknopf: es ist eine
        // Handlung, kein Zustand.
        m_moreBtn = new QPushButton(QStringLiteral("⚙"), this);
        m_moreBtn->setFixedSize(26, 20);
        m_moreBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        m_moreBtn->setStyleSheet(Style::buttonBaseStyle()
            + QStringLiteral("QPushButton { padding: 1px 3px; }"));
        m_moreBtn->setAccessibleName(QStringLiteral("More TX settings"));
        m_moreBtn->setToolTip(tr("Fine tuning: TX profile, TX "
                                             "bandwidth, VOX threshold and "
                                             "monitor level."));
        reihe->addWidget(m_moreBtn);

        vbox->addLayout(reihe);
    }

    // ── Band 3: die Fusszeile ───────────────────────────────────────────────
    // Mikrofonquelle und Bandbreite sind ZUSTAND, kein Bedienelement —
    // sie hatten vorher je eine eigene Zeile mitten im Feld.
    {
        auto* fuss = new QHBoxLayout;
        fuss->setSpacing(6);

        m_micSourceBadge = new QLabel(QStringLiteral("PC mic"), this);
        m_micSourceBadge->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: %2px; }")
            .arg(QLatin1String(Style::kTextScale)).arg(Style::kFontCaption));
        m_micSourceBadge->setAccessibleName(QStringLiteral("Mic source indicator"));
        m_micSourceBadge->setToolTip(QStringLiteral(
            "Active microphone source: PC mic or Radio mic.\n"
            "Change via Setup → Transmit → Mic Source."));
        fuss->addWidget(m_micSourceBadge);

        auto trenner = [this] {
            auto* l = new QLabel(QStringLiteral("·"), this);
            l->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; font-size: %2px; }")
                .arg(QLatin1String(Style::kTextInactive)).arg(Style::kFontCaption));
            return l;
        };
        fuss->addWidget(trenner());

        // Das aktive TX-Profil. Der Waehler dafuer steht seit dem Umbau
        // im Feinblatt — sein NAME muss trotzdem sichtbar bleiben, sonst
        // arbeitet man mit einer Einstellung, die man nicht mehr sieht.
        m_profileEcho = new QLabel(QStringLiteral("Default"), this);
        m_profileEcho->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: %2px; }")
            .arg(QLatin1String(Style::kTextScale)).arg(Style::kFontCaption));
        m_profileEcho->setAccessibleName(QStringLiteral("Active TX profile"));
        fuss->addWidget(m_profileEcho);

        fuss->addWidget(trenner());

        // Zeigt z.B. „100-2900 Hz · 2.8k BW"; nachgezogen von
        // filterChanged + dspModeChanged. Seit dem 2026-10-16 ist das
        // kein blosser Zustand mehr, sondern ein Knopf: Linksklick
        // geht zur naechsten Stufe, Rechtsklick oeffnet die Liste.
        // Flach und in derselben Schrift wie vorher, damit die
        // Fussleiste nicht lauter wird.
        m_txFilterStatusBtn = new QPushButton(this);
        m_txFilterStatusBtn->setFlat(true);
        m_txFilterStatusBtn->setCursor(Qt::PointingHandCursor);
        m_txFilterStatusBtn->setStyleSheet(QStringLiteral(
            "QPushButton { color: %1; font-size: %2px; border: none; "
            "background: transparent; padding: 0px; text-align: left; } "
            "QPushButton:hover { text-decoration: underline; }")
            .arg(QLatin1String(Style::kTxFilterOverlayLabel)).arg(Style::kFontCaption));
        m_txFilterStatusBtn->setAccessibleName(QStringLiteral("TX filter status"));
        m_txFilterStatusBtn->setToolTip(tr(
            "Transmit bandwidth. Click for the next step, right-click "
            "for the list."));
        m_txFilterStatusBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_txFilterStatusBtn, &QPushButton::clicked, this, [this]() {
            if (!m_model) { return; }
            const TransmitModel& tx = m_model->transmitModel();
            setzeBandbreite(naechsteStufe(tx.filterLow(), tx.filterHigh()));
        });
        connect(m_txFilterStatusBtn, &QWidget::customContextMenuRequested,
                this, [this](const QPoint& p) {
            zeigeBandbreitenMenue(m_txFilterStatusBtn->mapToGlobal(p));
        });
        fuss->addWidget(m_txFilterStatusBtn);

        fuss->addStretch();

        m_swrProtLed = new QLabel(QStringLiteral("SWR Prot"), this);
        m_swrProtLed->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: %2px; font-weight: bold; }")
            .arg(QLatin1String(Style::kTextInactive)).arg(Style::kFontCaption));
        m_swrProtLed->setAccessibleName(QStringLiteral("SWR protection indicator"));
        fuss->addWidget(m_swrProtLed);

        vbox->addLayout(fuss);
    }

    vbox->addStretch();

    buildFinePopup();
}

// ── Das Feinblatt hinter dem Zahnrad ────────────────────────────────────────
//
// Alles, was aus dem Feld verschwunden ist, steht hier — und zwar als
// DIESELBEN Bauteile mit denselben objectNames. Wer sie ueber
// findChild() sucht (und das tun tst_tx_applet_vox_relocated,
// tst_tx_applet_profile_combo und tst_tx_applet_lev_eq_cfc), findet sie
// weiterhin; sie haengen nur an einem anderen Elternteil.
//
// Qt::Popup statt Dialog: das Blatt schliesst sich beim Klick daneben,
// so wie ein Menue. Es ist eine Feineinstellung, kein Arbeitsfenster.
void TxApplet::buildFinePopup()
{
    m_finePopup = new QWidget(this, Qt::Popup);
    m_finePopup->setObjectName(QStringLiteral("TxFinePopup"));
    m_finePopup->setStyleSheet(QStringLiteral(
        "#TxFinePopup { background: %1; border: 1px solid %2; border-radius: 8px; }"
        "#TxFinePopup QLabel { color: %3; font-size: %4px; background: transparent; }"
    ).arg(QLatin1String(Style::kPanelBg), QLatin1String(Style::kBorder),
          QLatin1String(Style::kTextSecondary)).arg(Style::kFontSmall));

    auto* grid = new QGridLayout(m_finePopup);
    grid->setContentsMargins(10, 8, 10, 8);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);

    auto beschriftung = [this](const QString& t) {
        auto* l = new QLabel(t, m_finePopup);
        l->setFixedWidth(64);
        return l;
    };

    int z = 0;

    // ── TX-Profil ───────────────────────────────────────────────────────────
    // Gefuellt aus dem MicProfileManager ueber setMicProfileManager().
    // Rechtsklick meldet txProfileMenuRequested (wie Thetis
    // comboTXProfile_MouseDown, console.cs:44519-44522 [v2.10.3.13]).
    grid->addWidget(beschriftung(tr("TX profile")), z, 0);
    m_profileCombo = new QComboBox(m_finePopup);
    m_profileCombo->addItem(QStringLiteral("Default"));
    m_profileCombo->setFixedHeight(22);
    applyComboStyle(m_profileCombo);
    m_profileCombo->setAccessibleName(QStringLiteral("TX profile"));
    m_profileCombo->setToolTip(QStringLiteral(
        "TX Profile — left-click to switch.  Right-click to edit "
        "(Setup → Audio → TX Profile)."));
    m_profileCombo->setContextMenuPolicy(Qt::CustomContextMenu);
    grid->addWidget(m_profileCombo, z, 1, 1, 3);
    ++z;

    // ── TX-Bandbreite ───────────────────────────────────────────────────────
    grid->addWidget(beschriftung(QStringLiteral("TX BW")), z, 0);
    m_txFilterLowSpin = new QSpinBox(m_finePopup);
    m_txFilterLowSpin->setRange(0, 5000);
    m_txFilterLowSpin->setSuffix(QStringLiteral(" Hz"));
    m_txFilterLowSpin->setStyleSheet(Style::spinBoxStyle());
    m_txFilterLowSpin->setMinimumWidth(78);
    m_txFilterLowSpin->setAccessibleName(QStringLiteral("TX filter low cutoff"));
    m_txFilterLowSpin->setToolTip(QStringLiteral(
        "TX bandpass filter lower cutoff (Hz).  0 Hz for voice SSB modes."));
    grid->addWidget(m_txFilterLowSpin, z, 1);

    m_txFilterHighSpin = new QSpinBox(m_finePopup);
    m_txFilterHighSpin->setRange(200, 10000);
    m_txFilterHighSpin->setSuffix(QStringLiteral(" Hz"));
    m_txFilterHighSpin->setStyleSheet(Style::spinBoxStyle());
    m_txFilterHighSpin->setMinimumWidth(78);
    m_txFilterHighSpin->setAccessibleName(QStringLiteral("TX filter high cutoff"));
    m_txFilterHighSpin->setToolTip(QStringLiteral(
        "TX bandpass filter upper cutoff (Hz).  2900 Hz for typical SSB voice."));
    grid->addWidget(m_txFilterHighSpin, z, 2, 1, 2);
    ++z;

    // ── VOX-Schwelle mit dem lebenden Pegelstreifen darunter ────────────────
    // Bereich -80..0 dB wie Thetis ptbVOX (console.Designer.cs:6018-6019
    // [v2.10.3.13]); Voreinstellung -20 wie ptbVOX.Value.
    grid->addWidget(beschriftung(tr("VOX threshold")), z, 0);
    auto* voxStack = new QWidget(m_finePopup);
    auto* voxStackVbox = new QVBoxLayout(voxStack);
    voxStackVbox->setContentsMargins(0, 0, 0, 0);
    voxStackVbox->setSpacing(1);

    m_voxSlider = new QSlider(Qt::Horizontal, voxStack);
    m_voxSlider->setRange(-80, 0);
    m_voxSlider->setValue(-20);
    m_voxSlider->setFixedHeight(14);
    m_voxSlider->setStyleSheet(Style::sliderHStyle());
    m_voxSlider->setAccessibleName(QStringLiteral("VOX threshold (dB)"));
    m_voxSlider->setObjectName(QStringLiteral("TxVoxThresholdSlider"));
    voxStackVbox->addWidget(m_voxSlider);

    m_voxPeakMeter = new DexpPeakMeter(voxStack);
    m_voxPeakMeter->setObjectName(QStringLiteral("TxVoxPeakMeter"));
    m_voxPeakMeter->setAccessibleName(QStringLiteral("VOX live mic peak"));
    voxStackVbox->addWidget(m_voxPeakMeter);
    grid->addWidget(voxStack, z, 1, 1, 2);

    m_voxLvlLabel = new QLabel(QStringLiteral("-20 dB"), m_finePopup);
    m_voxLvlLabel->setStyleSheet(Style::insetValueStyle());
    m_voxLvlLabel->setFixedWidth(46);
    m_voxLvlLabel->setAlignment(Qt::AlignCenter);
    grid->addWidget(m_voxLvlLabel, z, 3);
    ++z;

    // ── VOX-Haltezeit ───────────────────────────────────────────────────────
    // 1..2000 ms wie Thetis udDEXPHold (setup.designer.cs:45005-45013).
    grid->addWidget(beschriftung(QStringLiteral("VOX-Halten")), z, 0);
    m_voxDlySlider = new QSlider(Qt::Horizontal, m_finePopup);
    m_voxDlySlider->setRange(1, 2000);
    m_voxDlySlider->setValue(500);
    m_voxDlySlider->setFixedHeight(14);
    m_voxDlySlider->setStyleSheet(Style::sliderHStyle());
    m_voxDlySlider->setAccessibleName(QStringLiteral("VOX hold time (ms)"));
    m_voxDlySlider->setObjectName(QStringLiteral("TxVoxHoldSlider"));
    grid->addWidget(m_voxDlySlider, z, 1, 1, 2);

    m_voxDlyLabel = new QLabel(QStringLiteral("500 ms"), m_finePopup);
    m_voxDlyLabel->setStyleSheet(Style::insetValueStyle());
    m_voxDlyLabel->setFixedWidth(46);
    m_voxDlyLabel->setAlignment(Qt::AlignCenter);
    grid->addWidget(m_voxDlyLabel, z, 3);
    ++z;

    // ── Mithoerlautstaerke ──────────────────────────────────────────────────
    // 0..100 ganzzahlig -> monitorVolume 0,0..1,0. Voreinstellung 50
    // (Modellvorgabe 0.5f, Thetis audio.cs:417).
    grid->addWidget(beschriftung(tr("Monitor")), z, 0);
    m_monitorVolumeSlider = new QSlider(Qt::Horizontal, m_finePopup);
    m_monitorVolumeSlider->setRange(0, 100);
    m_monitorVolumeSlider->setValue(50);
    m_monitorVolumeSlider->setFixedHeight(14);
    m_monitorVolumeSlider->setStyleSheet(Style::sliderHStyle());
    m_monitorVolumeSlider->setAccessibleName(QStringLiteral("Monitor volume"));
    m_monitorVolumeSlider->setToolTip(QStringLiteral(
        "Monitor receive audio volume during TX (0–100 %)"));
    grid->addWidget(m_monitorVolumeSlider, z, 1, 1, 2);

    m_monitorVolumeValue = new QLabel(QStringLiteral("50"), m_finePopup);
    m_monitorVolumeValue->setStyleSheet(Style::insetValueStyle());
    m_monitorVolumeValue->setFixedWidth(46);
    m_monitorVolumeValue->setAlignment(Qt::AlignCenter);
    grid->addWidget(m_monitorVolumeValue, z, 3);

    m_finePopup->setFixedWidth(376);
    m_finePopup->adjustSize();

    connect(m_profileCombo, &QComboBox::currentTextChanged, this,
            [this](const QString& name) {
                if (m_profileEcho) { m_profileEcho->setText(name); }
            });

    connect(m_moreBtn, &QPushButton::clicked, this, &TxApplet::showFinePopup);
    // Rechtsklick auf MON fuehrt zur Mithoerlautstaerke — die stand
    // vorher als eigene Reglerzeile im Feld.
    connect(m_monBtn, &QPushButton::customContextMenuRequested,
            this, [this](const QPoint&) { showFinePopup(); });
}

void TxApplet::showFinePopup()
{
    if (!m_finePopup) { return; }
    m_finePopup->adjustSize();
    // Unter dem Zahnrad, aber rechtsbuendig mit ihm — sonst haengt das
    // Blatt bei angedocktem Feld ueber den rechten Fensterrand hinaus.
    const QPoint unten = m_moreBtn
        ? m_moreBtn->mapToGlobal(QPoint(m_moreBtn->width() - m_finePopup->width(),
                                        m_moreBtn->height() + 4))
        : mapToGlobal(QPoint(0, height()));
    m_finePopup->move(unten);
    m_finePopup->show();
}

// ── Phase 3M-1a H.3 wiring ──────────────────────────────────────────────────
//
// wireControls: called once after buildUI(). Attaches signal/slot connections
// between the four wired controls and the model layer.
//
// Pattern follows the Longpath "GUI↔Model sync, no feedback loops" rule:
//   - Use QSignalBlocker (or m_updatingFromModel) to prevent echo loops.
//   - Model setters emit signals → RadioConnection sends protocol commands.
//   - UI state changes → model setters → emit back to update other UI.
void TxApplet::wireControls()
{
    if (!m_model) {
        return;
    }

    TransmitModel& tx = m_model->transmitModel();
    MoxController* mox = m_model->moxController();

    // ── Forward-power gauge ← RadioStatus::powerChanged ─────────────────────
    // 3M-1a (2026-04-27): wire the previously-NYI fwd-power gauge to the
    // radio's reported forward power.  Pipeline:
    //   P2RadioConnection → paTelemetryUpdated(fwdRaw,...)
    //     → RadioModel handler scaleFwdPowerWatts() → m_radioStatus.setForwardPower(W)
    //     → RadioStatus::powerChanged(fwd, rev, swr)  ← we listen here
    //
    // Two-stage de-jitter (Phase 3I-1 will replace with the proper Thetis
    // peak-decay filter):
    //   1.  alpha=0.10 EMA on every powerChanged emit — smooths the per-
    //       sample noise without much lag (~10 sample time-constant).
    //   2.  10 Hz QTimer reads the smoothed state into the gauge — keeps
    //       the visible digits stable even when RadioStatus::powerChanged
    //       fires twice per hardware sample (documented in
    //       RadioModel.cpp:572).  Without this throttle the digit
    //       characters update too fast to read.
    connect(&m_model->radioStatus(), &RadioStatus::powerChanged,
            this, [this](double fwdW, double /*revW*/, double swr) {
        // Solange ein aeusserer Verstaerker arbeitet, meldet ER die
        // Leistung. Die Barfusszahl des Geraets ist dann seine
        // Ansteuerung, nicht die Ausgangsleistung — sie hier
        // durchzulassen ueberschriebe den Verstaerkerwert im Takt der
        // Geraetetelemetrie (~20 Hz gegen 1 Hz beim RF-Kit) und die
        // Anzeige spraenge zwischen 40 W und 800 W.
        //
        // Dieselbe Torschaltung wie bei der analogen Anzeige
        // (MainWindow Connect 2, KG4VCF-Befund 2026-05-25).
        if (m_model && m_model->isAnyExternalAmpInOperate()) { return; }

        // ── Nach dem Loesen kommen noch Werte von vorhin (2026-10-29) ──
        //
        // Betreiber: „dann klicke ich auf off mox und es sendet noch
        // weiter". Gemessen hat er recht gesehen und doch nicht:
        //
        //   20:43:16.454  MOX aus
        //   20:43:16.455  Flex: geloest
        //   20:43:16.456  Empfang nach dem Loesen wieder da — 1 ms
        //
        // Der Befehl ging in derselben Millisekunde hinaus und die
        // Maschine hoerte sofort auf. Ihr MESSWERTSTROM aber laeuft
        // hinterher: nach dem Loesen treffen noch Pakete ein, die die
        // Sendeleistung von vorhin tragen, und die schieben den Balken
        // wieder hoch. Am Schirm sieht das aus wie ein Sender, der
        // nicht aufhoert — bei einer Endstufe in der Leitung ist das
        // kein Schoenheitsfehler.
        //
        // Also anderthalb Sekunden lang nichts annehmen. Ein Tasten
        // von der Maschine aus (Taste am Mikrofon) kommt danach wieder
        // durch; es dauert nur kurz laenger, bis der Balken folgt.
        if (!m_inTx && m_fwdSperreBisMs > 0) {
            if (QDateTime::currentMSecsSinceEpoch() < m_fwdSperreBisMs) {
                m_fwdPowerSmoothedW = 0.0;
                if (m_swrGauge) { m_swrGauge->setValue(1.0); }
                return;
            }
            m_fwdSperreBisMs = 0;
        }

        // ── Waehrend des Sendens die SPITZE halten (2026-10-29) ─────
        //
        // Betreiber, an der FLEX-8400: „leider sieht man die werte
        // nicht bei fwd konstant". Zwei Gruende, die sich addieren:
        //
        //   * Die Flex schickt FWDPWR NICHT in jedem Messwertpaket
        //     (RadioModel.cpp, derselbe Befund vom 2026-10-27). Die
        //     Glaettung laeuft also zwischendurch auf den alten Wert
        //     zu und faellt ab, obwohl gesendet wird.
        //   * SSB hat keine konstante Huellkurve. Eine Glaettung ueber
        //     Sprache zeigt irgendetwas zwischen Null und Spitze — und
        //     nie die Zahl, die einen Funker interessiert.
        //
        // Ein Leistungsmesser am Sender zeigt darum PEP: die hoechste
        // Spitze, und die bleibt stehen. Hier ist es die Spitze SEIT
        // DEM TASTEN; beim Loesen faellt sie auf null zurueck (die
        // Flanke darunter macht das schon).
        if (m_inTx) {
            if (fwdW > m_fwdPeakW) { m_fwdPeakW = fwdW; }
            m_fwdPowerSmoothedW = m_fwdPeakW;
            if (m_swrGauge) { m_swrGauge->setValue(swr); }
            return;
        }

        constexpr double kAlpha = 0.30;
        m_fwdPowerSmoothedW = kAlpha * fwdW + (1.0 - kAlpha) * m_fwdPowerSmoothedW;
        if (m_swrGauge) {
            m_swrGauge->setValue(swr);
        }
    });

    // ── Die Leistung des Verstaerkers ────────────────────────────────
    //
    // Zwei Quellen, je Hersteller eine, mit derselben Torschaltung wie
    // die analoge Anzeige. PGXL meldet ueber ampMetersChanged, RF-Kit
    // ueber externalAmpFwdSwrUpdated.
    //
    // Die RF-Kit-Torschaltung fragt AUSDRUECKLICH nur nach RF-Kit und
    // nicht nach „irgendein Verstaerker": dieses Signal traegt
    // ausschliesslich RF-Kit-Telemetrie, und in der uebergreifenden
    // Form liess ein PGXL in OPERATE die 0 W eines RF2K-S im Standby
    // durch (Codex-Durchsicht, PR #291).
    if (m_model) {
        connect(m_model, &RadioModel::ampMetersChanged, this,
                [this](float fwd, float swr) {
            if (!m_model->hasAmplifier() || !m_model->ampOperate()) { return; }
            m_fwdPowerSmoothedW = fwd;
            if (m_swrGauge) { m_swrGauge->setValue(swr); }
        });
        connect(m_model, &RadioModel::externalAmpFwdSwrUpdated, this,
                [this](int fwd, float swr) {
            if (!m_model->isRfKitInOperate()) { return; }
            m_fwdPowerSmoothedW = static_cast<double>(fwd);
            if (m_swrGauge) { m_swrGauge->setValue(swr); }
        });
    }
    auto* fwdGaugeRefreshTimer = new QTimer(this);
    fwdGaugeRefreshTimer->setInterval(50);   // 20 Hz UI refresh
    connect(fwdGaugeRefreshTimer, &QTimer::timeout, this, [this]() {
        if (m_fwdPowerGauge) {
            m_fwdPowerGauge->setValue(m_fwdPowerSmoothedW);
        }
    });
    fwdGaugeRefreshTimer->start();

    // Bench-reported #167 follow-up: TxApplet's RF Pwr / SWR HGauges have
    // their own EMA smoothing (m_fwdPowerSmoothedW, alpha=0.30) separate
    // from the MeterPanel's BarItem stack.  A single zero from
    // RadioStatus::powerChanged after un-key only walks the smoothed
    // value down by 30% (e.g. 60 → 42), then no more updates flow so the
    // gauge stays stuck at ~42.  Snap the smoothed state to 0 on MOX
    // falling-edge so the gauge clears instantly — same idea as the
    // BarItem clearSmoothing path in MeterPoller::setInTx.  Subscribed
    // to MoxController::moxStateChanged (the authoritative TX boundary,
    // covers MOX un-key AND TUNE release) rather than orphan
    // TransmitModel::moxChanged signals.
    if (m_model && m_model->moxController()) {
        connect(m_model->moxController(), &MoxController::moxStateChanged,
                this, [this](bool active) {
            // Die Spitze gilt je Sendung: beim Tasten faengt sie bei
            // null an, sonst stuende beim naechsten Mal noch die von
            // vorhin da (siehe den Spitzenwerthalt oben).
            m_inTx = active;
            m_fwdPeakW = 0.0;
            if (active) {
                m_fwdSperreBisMs = 0;
                return;                  // rising-edge: smoothing takes over
            }
            // Siehe oben: der Messwertstrom laeuft dem Loesen nach.
            m_fwdSperreBisMs = QDateTime::currentMSecsSinceEpoch() + 1500;
            m_fwdPowerSmoothedW = 0.0;
            if (m_fwdPowerGauge) { m_fwdPowerGauge->setValue(0.0); }
            if (m_swrGauge)      { m_swrGauge->setValue(1.0); }
        });
    }

    // Per-SKU RF Pwr gauge rescale.  Bench-reported #167 follow-up: the
    // 0-120 W default scale made HL2 (5 W) and ANAN-G2-1K (1000 W) both
    // unreadable.  Subscribe to currentRadioChanged so the gauge ticks
    // and red-zone redraw whenever the active radio changes.
    //
    // Issue #175 Task 7: also rescale the RF Power and Tune Power sliders
    // (HL2 → 0..90/6 and 0..99/3 with -X.X dB labels via mi0bot formulae;
    //  every other SKU → canonical 0..100/1 with bare integer labels).
    connect(m_model, &RadioModel::currentRadioChanged, this,
            [this](const Longpath::RadioInfo&) {
        const auto model = m_model->hardwareProfile().model;
        rescaleFwdGaugeForModel(model);
        rescalePowerSlidersForModel(model);
    });
    // Apply the current model's scale at wireControls() time so cold-launch
    // (or reconnect-on-startup) lands a properly-sized gauge + sliders before
    // the first telemetry sample.
    {
        const auto model = m_model->hardwareProfile().model;
        rescaleFwdGaugeForModel(model);
        rescalePowerSlidersForModel(model);
    }

    // ── SWR Prot LED ← SwrProtectionController::highSwrChanged ─────────────
    // SwrProtectionController (Phase 3G-13) emits highSwrChanged(bool) when
    // the radio's SWR-protection state changes. Light the LED amber when
    // high-SWR protection is active; dim it when cleared.
    {
        auto updateSwrProtLed = [this](bool isHigh) {
            if (!m_swrProtLed) { return; }
            // Der Ton hatte hier keinen Namen und stand als nacktes
            // Literal; seit 2026-09-02 heisst er Style::kSwrProtActive.
            const QString color = isHigh ? QLatin1String(Style::kSwrProtActive)
                                         : QLatin1String(Style::kTextInactive);
            m_swrProtLed->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; font-size: %2px; font-weight: bold; }"
            ).arg(color).arg(Style::kFontCaption));
        };
        connect(&m_model->swrProt(),
                &safety::SwrProtectionController::highSwrChanged,
                this, updateSwrProtLed);
        // Initialise to current state.
        updateSwrProtLed(m_model->swrProt().highSwr());
    }

    // ── RF Power slider → TransmitModel::setPower(int) ──────────────────────
    // From Thetis chkMOX_CheckedChanged2 power flow [v2.10.3.13]:
    //   the RF Power slider maps 0–100 to TX drive level.
    //
    // Issue #175 Task 7: label text is now driven by updatePowerSliderLabels()
    // which formats the value in -X.X dB on HL2 and bare integer on every
    // other SKU.  The old direct setText(QString::number(val)) would have
    // bypassed the dB conversion on HL2.
    connect(m_rfPowerSlider, &QSlider::valueChanged, this, [this, &tx](int val) {
        updatePowerSliderLabels();
        if (m_updatingFromModel) { return; }
        tx.setPower(val);
        // Per-band write: matches Thetis ptbPWR_Scroll at console.cs:28642
        // [v2.10.3.13] (`power_by_band[(int)_tx_band] = ptbPWR.Value;`).
        // Without this, the per-band slot only updates indirectly via the
        // setPowerUsingTargetDbm txMode-0 side-effect (TransmitModel.cpp:825),
        // which is gated on connected radio + loaded PA profile + !TUNE.
        // Result: slider moves while disconnected (or before profiles load)
        // never persist across restart.  setPowerForBand auto-persists to
        // hardware/<mac>/powerByBand/<band> when m_persistMac is non-empty.
        //
        // Source the band from the active slice (the canonical TX band per
        // RadioModel.cpp:903-905), NOT m_currentBand.  m_currentBand tracks
        // UI state and is fed by both PanadapterModel::bandChanged AND
        // SliceModel::frequencyChanged, so it can drift to the panadapter
        // band on CTUN pans without slice retune — writing through it would
        // silently corrupt other bands' stored values.  txBand() falls back
        // to m_currentBand when the active slice is unavailable.
        tx.setPowerForBand(txBand(), val);
        // Symmetric to the tune-slider auto-switch above: touching the RF
        // Power slider restores the tune source to DriveSlider so the
        // setPowerUsingTargetDbm txMode 1 branch reads tx.power() during
        // TUNE.  Last-touched-slider-wins UX. From Thetis console.cs:46553
        // [v2.10.3.13] DrivePowerSource.DRIVE_SLIDER is the canonical default.
        tx.setTuneDrivePowerSource(DrivePowerSource::DriveSlider);
    });

    // Reverse: TransmitModel::powerChanged → slider
    connect(&tx, &TransmitModel::powerChanged, this, [this](int power) {
        QSignalBlocker b(m_rfPowerSlider);
        m_updatingFromModel = true;
        m_rfPowerSlider->setValue(power);
        updatePowerSliderLabels();   // Issue #175 Task 7: HL2 dB / non-HL2 int
        m_updatingFromModel = false;
    });

    // ── Tune Power slider → TransmitModel::setTunePowerForBand ──────────────
    // Per-band tune power, ported from Thetis console.cs:12094 [v2.10.3.13]:
    //   private int[] tunePower_by_band;
    // The current band is tracked by m_currentBand (updated by setCurrentBand).
    //
    // Issue #175 Task 7: label text routed through updatePowerSliderLabels()
    // for the HL2 (slider/3.0 - 33.0)/2.0 dB conversion.
    connect(m_tunePwrSlider, &QSlider::valueChanged, this, [this, &tx](int val) {
        updatePowerSliderLabels();
        if (m_updatingFromModel) { return; }
        // ── Auch global setzen (2026-10-31) ─────────────────────────
        //
        // Betreiber: „ich tune mit 1 watt und oben steht kurz ueber 23
        // watt" — und: „tunen will ich grundsaetzlich mit einem watt."
        //
        // Der Regler schrieb NUR den Bandwert. `tunePowerChanged`
        // feuerte damit nie, und wer daran haengt, erfuhr von der
        // Einstellung nichts: die Flex-Verdrahtung
        // (`MainWindow_Flex.cpp`) schickte weiter den alten globalen
        // Wert. Im Protokoll vom 2026-10-31, 14:52:27, steht es
        // woertlich — „abstimmen 10 %", waehrend am Regler 1 stand.
        //
        // Der Leistungsregler zehn Zeilen hoeher macht es seit jeher
        // als PAAR: `setPower` (global, meldet) UND `setPowerForBand`
        // (Speicher). Hier fehlte die erste Haelfte. Jetzt dieselbe
        // Hand.
        tx.setTunePower(val);
        tx.setTunePowerForBand(m_currentBand, val);
        // When the user touches the tune slider, switch the tune drive
        // source so TUNE actually reads from tunePowerForBand instead of
        // the regular drive slider (the default per Thetis console.cs:46553
        // [v2.10.3.13]).  Without this the tune slider is dead UI: its
        // value persists per-band but the math kernel
        // (TransmitModel::setPowerUsingTargetDbm txMode 1) only consults
        // tunePowerForBand when m_tuneDrivePowerSource == TuneSlider.
        // Longpath-spin: Thetis exposes _tuneDrivePowerSource via a
        // separate Setup combo; Longpath follows last-touched-slider-wins
        // UX so users don't need to know the enum exists.
        tx.setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
    });

    // Reverse: TransmitModel::tunePowerByBandChanged → slider (only for current band)
    connect(&tx, &TransmitModel::tunePowerByBandChanged,
            this, [this](Band band, int watts) {
        if (band != m_currentBand) { return; }
        QSignalBlocker b(m_tunePwrSlider);
        m_updatingFromModel = true;
        m_tunePwrSlider->setValue(watts);
        updatePowerSliderLabels();   // Issue #175 Task 7: HL2 dB / non-HL2 int
        m_updatingFromModel = false;
    });

    // ── TUNE button → RadioModel::setTune(bool) ──────────────────────────────
    // G.4 orchestrator: CW mode swap, tone setup, tune-power push.
    // From Thetis console.cs:29978-30157 [v2.10.3.13] chkTUN_CheckedChanged.
    // G8NJJ tell ARIES that tune is active  [original inline comment from console.cs:30153]
    // MW0LGE_22b setupTuneDriveSlider  [original inline comment from console.cs:30155]
    connect(m_tuneBtn, &QPushButton::toggled, this, [this](bool on) {
        if (m_updatingFromModel) { return; }
        if (!m_model) { return; }
        m_model->setTune(on);
        if (on) {
            m_tuneBtn->setText(QStringLiteral("TUNING..."));
        } else {
            m_tuneBtn->setText(QStringLiteral("TUNE"));
        }
    });

    // Reverse: tuneRefused → uncheck TUN button + clear text.
    // From Thetis console.cs:30076 [v2.10.3.13]: guard conditions before
    // chkTUN.Checked = true (connection + power-on checks).
    connect(m_model, &RadioModel::tuneRefused, this, [this](const QString& /*reason*/) {
        QSignalBlocker b(m_tuneBtn);
        m_updatingFromModel = true;
        m_tuneBtn->setChecked(false);
        m_tuneBtn->setText(QStringLiteral("TUNE"));
        m_updatingFromModel = false;
    });

    // ── MOX button → MoxController::setMox(bool) ────────────────────────────
    // B.5 setter: drives state machine through RX→TX or TX→RX transitions.
    // From Thetis console.cs:29311-29678 [v2.10.3.13] chkMOX_CheckedChanged2.
    // //[2.10.1.0]MW0LGE changed  [original inline comment from console.cs:29355]
    // //MW0LGE [2.9.0.7]  [original inline comment from console.cs:29400, 29561]
    // //[2.10.3.6]MW0LGE att_fixes  [original inline comment from console.cs:29567-29568, 29659]
    if (mox) {
        connect(m_moxBtn, &QPushButton::toggled, this, [this, mox](bool on) {
            if (m_updatingFromModel) { return; }
            mox->setMox(on);
        });

        // Reverse: MoxController::moxStateChanged → button checked state.
        // moxStateChanged fires at end of the timer walk (TX fully engaged
        // or fully released), not at setMox() entry — so the button reflects
        // the confirmed state, not the in-progress request.
        connect(mox, &MoxController::moxStateChanged,
                this, [this](bool on) {
            QSignalBlocker b(m_moxBtn);
            m_updatingFromModel = true;
            m_moxBtn->setChecked(on);
            m_updatingFromModel = false;
        });

        // TUNE button checked state driven by manualMoxChanged.
        // manualMoxChanged fires when setTune() sets/clears m_manualMox.
        connect(mox, &MoxController::manualMoxChanged,
                this, [this](bool isManual) {
            QSignalBlocker b(m_tuneBtn);
            m_updatingFromModel = true;
            m_tuneBtn->setChecked(isManual);
            m_tuneBtn->setText(isManual
                ? QStringLiteral("TUNING...")
                : QStringLiteral("TUNE"));
            m_updatingFromModel = false;
        });
    }

    // ── K.2: MOX button tooltip override ← SliceModel::dspModeChanged ─────────
    // Phase 3M-1b K.2: update MOX button tooltip when DSP mode changes.
    // For modes that are deferred to a later phase (CW → 3M-2, AM/FM → 3M-3)
    // the tooltip explains why MOX won't engage, matching the moxRejected reason.
    // 2026-09-13 code review fix: this used to connect once, here, to
    // whichever slice was active at construction time (there usually is
    // none yet -- TxApplet is built before any slice exists -- and even
    // when one did exist, switching the active slice afterward left this
    // wired to the old one). Moved into rebindActiveSlice(), called once
    // below and again by MainWindow on every RadioModel::activeSliceChanged.

    // ── 4b. VOX row wiring (3M-3a-iii bench polish 2026-05-04) ────────────────
    //
    // Bidirectional binds for [VOX] toggle + threshold/hold sliders, plus
    // right-click → openSetupRequested("Transmit", "DEXP/VOX") and a 100 ms
    // QTimer driving m_voxPeakMeter (TxApplet::pollVoxMeter).
    // Mirrors the wiring pattern from PhoneCwApplet (Phase 3M-3a-iii Task
    // 15) — relocated here as part of the 2026-05-04 bench polish.

    // ── VOX [ON] toggle ↔ TransmitModel::voxEnabled ──────────────────────────
    if (m_voxBtn) {
        {
            QSignalBlocker b(m_voxBtn);
            m_voxBtn->setChecked(tx.voxEnabled());
        }
        // UI → Model
        connect(m_voxBtn, &QPushButton::toggled, this, [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.setVoxEnabled(on);
        });
        // Model → UI
        connect(&tx, &TransmitModel::voxEnabledChanged, this, [this](bool on) {
            m_updatingFromModel = true;
            {
                QSignalBlocker b(m_voxBtn);
                m_voxBtn->setChecked(on);
            }
            m_updatingFromModel = false;
        });
    }

    // ── VOX threshold slider ↔ TransmitModel::voxThresholdDb ─────────────────
    // Range -80..0 dB from console.Designer.cs:6018-6019 [v2.10.3.13]
    // (already set in buildUI).  Default value -20 dB matches Thetis
    // ptbVOX.Value=-20 (console.Designer.cs:6024 [v2.10.3.13]).
    if (m_voxSlider) {
        {
            QSignalBlocker b(m_voxSlider);
            m_voxSlider->setValue(tx.voxThresholdDb());
        }
        if (m_voxLvlLabel) {
            m_voxLvlLabel->setText(QStringLiteral("%1 dB").arg(tx.voxThresholdDb()));
        }
        // UI → Model + label refresh
        connect(m_voxSlider, &QSlider::valueChanged, this, [this, &tx](int dB) {
            if (m_voxLvlLabel) {
                m_voxLvlLabel->setText(QStringLiteral("%1 dB").arg(dB));
            }
            if (m_updatingFromModel) { return; }
            tx.setVoxThresholdDb(dB);
        });
        // Model → UI
        connect(&tx, &TransmitModel::voxThresholdDbChanged, this, [this](int dB) {
            m_updatingFromModel = true;
            {
                QSignalBlocker b(m_voxSlider);
                m_voxSlider->setValue(dB);
            }
            if (m_voxLvlLabel) {
                m_voxLvlLabel->setText(QStringLiteral("%1 dB").arg(dB));
            }
            m_updatingFromModel = false;
        });
    }

    // ── VOX Hold slider ↔ TransmitModel::voxHangTimeMs ───────────────────────
    // Range 1..2000 ms from setup.designer.cs:45005-45013 [v2.10.3.13]
    // (already set in buildUI).  Default 500 ms matches udDEXPHold.Value
    // (setup.designer.cs:45020 [v2.10.3.13]).
    if (m_voxDlySlider) {
        {
            QSignalBlocker b(m_voxDlySlider);
            m_voxDlySlider->setValue(tx.voxHangTimeMs());
        }
        if (m_voxDlyLabel) {
            m_voxDlyLabel->setText(QStringLiteral("%1 ms").arg(tx.voxHangTimeMs()));
        }
        // UI → Model + label refresh
        connect(m_voxDlySlider, &QSlider::valueChanged, this, [this, &tx](int ms) {
            if (m_voxDlyLabel) {
                m_voxDlyLabel->setText(QStringLiteral("%1 ms").arg(ms));
            }
            if (m_updatingFromModel) { return; }
            tx.setVoxHangTimeMs(ms);
        });
        // Model → UI
        connect(&tx, &TransmitModel::voxHangTimeMsChanged, this, [this](int ms) {
            m_updatingFromModel = true;
            {
                QSignalBlocker b(m_voxDlySlider);
                m_voxDlySlider->setValue(ms);
            }
            if (m_voxDlyLabel) {
                m_voxDlyLabel->setText(QStringLiteral("%1 ms").arg(ms));
            }
            m_updatingFromModel = false;
        });
    }

    // ── Right-click on VOX → emit openSetupRequested ─────────────────────────
    // Mirrors PhoneCwApplet's DEXP-button right-click pattern.  MainWindow
    // listens and jumps the SetupDialog to the "DEXP/VOX" leaf page.
    if (m_voxBtn) {
        connect(m_voxBtn, &QPushButton::customContextMenuRequested, this,
                [this](const QPoint&) {
            emit openSetupRequested(QStringLiteral("Transmit"),
                                    QStringLiteral("DEXP/VOX"));
        });
    }

    // ── 100 ms timer driving m_voxPeakMeter ──────────────────────────────────
    // Cadence matches Thetis UpdateNoiseGate Task.Delay(100) at
    // console.cs:25347 [v2.10.3.13].  Stops automatically when the applet
    // is destroyed (parented to `this`).  Continuous (NOT MOX-gated) since
    // GetDEXPPeakSignal is the live DEXP detector envelope, not the
    // TX-pipeline meter.
    m_voxMeterTimer = new QTimer(this);
    m_voxMeterTimer->setInterval(100);
    connect(m_voxMeterTimer, &QTimer::timeout,
            this, &TxApplet::pollVoxMeter);
    m_voxMeterTimer->start();

    // ── MON toggle button ↔ TransmitModel::monEnabled ────────────────────────
    // Phase 3M-1b J.3.
    // UI → Model: toggled → setMonEnabled (with m_updatingFromModel guard).
    // Model → UI: monEnabledChanged → update checked state with QSignalBlocker.
    connect(m_monBtn, &QPushButton::toggled, this, [this, &tx](bool on) {
        if (m_updatingFromModel) { return; }
        tx.setMonEnabled(on);
    });

    connect(&tx, &TransmitModel::monEnabledChanged, this, [this](bool on) {
        QSignalBlocker b(m_monBtn);
        m_updatingFromModel = true;
        m_monBtn->setChecked(on);
        m_updatingFromModel = false;
    });

    // ── Monitor volume slider ↔ TransmitModel::monitorVolume ─────────────────
    // Phase 3M-1b J.3.
    // UI → Model: slider valueChanged(int) → setMonitorVolume(value / 100.0f).
    // Model → UI: monitorVolumeChanged(float) → slider = qRound(v * 100.0f).
    connect(m_monitorVolumeSlider, &QSlider::valueChanged,
            this, [this, &tx](int val) {
        if (m_updatingFromModel) { return; }
        m_monitorVolumeValue->setText(QString::number(val));
        tx.setMonitorVolume(static_cast<float>(val) / 100.0f);
    });

    connect(&tx, &TransmitModel::monitorVolumeChanged,
            this, [this](float volume) {
        QSignalBlocker b(m_monitorVolumeSlider);
        m_updatingFromModel = true;
        const int uiVal = qRound(volume * 100.0f);
        m_monitorVolumeSlider->setValue(uiVal);
        m_monitorVolumeValue->setText(QString::number(uiVal));
        m_updatingFromModel = false;
    });

    // ── Phase 3M-3a-i Batch 2 (Task F): LEV / EQ quick toggles ──────────────
    //
    // LEV button ↔ TransmitModel::txLevelerOn (bidirectional, echo-guarded).
    if (m_levBtn) {
        connect(m_levBtn, &QPushButton::toggled,
                this, [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.setTxLevelerOn(on);
        });
        connect(&tx, &TransmitModel::txLevelerOnChanged,
                this, [this](bool on) {
            QSignalBlocker b(m_levBtn);
            m_updatingFromModel = true;
            m_levBtn->setChecked(on);
            m_updatingFromModel = false;
        });
    }

    // EQ button ↔ TransmitModel::txEqEnabled (bidirectional, echo-guarded).
    // Right-click: placeholder slot for the TxEqDialog launch (Batch 3).
    if (m_eqBtn) {
        connect(m_eqBtn, &QPushButton::toggled,
                this, [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.setTxEqEnabled(on);
        });
        connect(&tx, &TransmitModel::txEqEnabledChanged,
                this, [this](bool on) {
            QSignalBlocker b(m_eqBtn);
            m_updatingFromModel = true;
            m_eqBtn->setChecked(on);
            m_updatingFromModel = false;
        });
        // Right-click → open TxEqDialog (Phase 3M-3a-i Batch 3 A.1).
        // Modeless singleton; show + raise + activateWindow so a
        // hidden-but-alive instance is brought forward.
        connect(m_eqBtn, &QPushButton::customContextMenuRequested,
                this, [this](const QPoint& /*pos*/) {
            if (!m_model) { return; }
            TxEqDialog* dlg = TxEqDialog::instance(m_model, this);
            dlg->show();
            dlg->raise();
            dlg->activateWindow();
        });
    }

    // ── Phase 3M-3a-ii Batch 6 (Task A): CFC button ↔ TransmitModel::cfcEnabled
    // Right-click opens the modeless TxCfcDialog (lazy-create singleton kept
    // alive for fast re-opens — same pattern as TxEqDialog).
    if (m_cfcBtn) {
        connect(m_cfcBtn, &QPushButton::toggled,
                this, [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.setCfcEnabled(on);
        });
        connect(&tx, &TransmitModel::cfcEnabledChanged,
                this, [this](bool on) {
            QSignalBlocker b(m_cfcBtn);
            m_updatingFromModel = true;
            m_cfcBtn->setChecked(on);
            m_updatingFromModel = false;
        });
        connect(m_cfcBtn, &QPushButton::customContextMenuRequested,
                this, [this](const QPoint& /*pos*/) {
            requestOpenCfcDialog();
        });
    }

    // ── Mic-source badge ← TransmitModel::micSourceChanged ───────────────────
    // Phase 3M-1b J.3. Read-only: updates badge text on signal, no user interaction.
    // "PC mic" for MicSource::Pc, "Radio mic" for MicSource::Radio, "VAX" for MicSource::Vax.
    connect(&tx, &TransmitModel::micSourceChanged,
            this, [this](MicSource) { aktualisiereMikroAbzeichen(); });
    // Ausfall des PC-Mikrofons (2026-11-20): die Tonmaschine meldet nur
    // Wechsel; der Text steht in aktualisiereMikroAbzeichen().
    if (AudioEngine* ae = m_model->audioEngine()) {
        connect(ae, &AudioEngine::pcMikrofonAusfallGeaendert,
                this, [this](bool) { aktualisiereMikroAbzeichen(); });
    }

    // ── Phase 3M-1c J.1 ─ TX Profile combo wiring ────────────────────────────
    // User-driven currentTextChanged → MicProfileManager::setActiveProfile.
    // Guarded with m_updatingFromModel so the rebuildProfileCombo() echo
    // doesn't bounce back into setActiveProfile.
    connect(m_profileCombo, &QComboBox::currentTextChanged,
            this, [this](const QString& name) {
        if (m_updatingFromModel) { return; }
        if (!m_micProfileMgr) { return; }
        if (name.isEmpty()) { return; }
        if (m_model) {
            m_micProfileMgr->setActiveProfile(name, &m_model->transmitModel());
        }
    });

    // Right-click on combo → emit txProfileMenuRequested (Thetis cite at the
    // signal declaration).
    connect(m_profileCombo, &QComboBox::customContextMenuRequested,
            this, [this](const QPoint& /*pos*/) {
        emit txProfileMenuRequested();
    });

    // ── Plan 4 Cluster C (Task 4 / D2+D3): TX BW spinbox wiring ─────────────
    //
    // UI → Model: spinbox valueChanged → setFilterLow / setFilterHigh.
    //             m_updatingFromModel guard prevents echo loops.
    // Model → UI: TransmitModel::filterChanged(int,int) → QSignalBlocker on
    //             both spinboxes, then setValue + refresh status label.
    if (m_txFilterLowSpin) {
        connect(m_txFilterLowSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int v) {
            if (m_updatingFromModel) { return; }
            m_model->transmitModel().setFilterLow(v);
        });
    }
    if (m_txFilterHighSpin) {
        connect(m_txFilterHighSpin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int v) {
            if (m_updatingFromModel) { return; }
            m_model->transmitModel().setFilterHigh(v);
        });
    }

    // Model → spinboxes + status label on filterChanged.
    connect(&tx, &TransmitModel::filterChanged,
            this, [this](int low, int high) {
        m_updatingFromModel = true;
        if (m_txFilterLowSpin) {
            QSignalBlocker bLo(m_txFilterLowSpin);
            m_txFilterLowSpin->setValue(low);
        }
        if (m_txFilterHighSpin) {
            QSignalBlocker bHi(m_txFilterHighSpin);
            m_txFilterHighSpin->setValue(high);
        }
        m_updatingFromModel = false;
        refreshTxFilterStatusLabel();
    });

    // Status label refresh on DSP mode change (symmetric ↔ asymmetric format)
    // is handled by rebindActiveSlice() (called once below, and again by
    // MainWindow on every RadioModel::activeSliceChanged) -- see its
    // 2026-09-13 code review comment on the header declaration.

    // ── Phase 3M-1c J.2 ─ 2-TONE button wiring ───────────────────────────────
    // toggled → TwoToneController::setActive.  Echo-guarded.
    connect(m_twoToneBtn, &QPushButton::toggled, this, [this](bool on) {
        if (m_updatingFromModel) { return; }
        if (!m_twoToneCtrl) { return; }
        m_twoToneCtrl->setActive(on);
    });

    // ── Phase 3M-4 Task 13: PS-A button wiring ───────────────────────────────
    // Source-first port of Thetis chkFWCATUBypass:
    //   - Left-click toggle drives PureSignal::setAutoCalEnabled (mirrors
    //     chkFWCATUBypass_Click, console.cs:36762 [v2.10.3.13]).
    //   - Right-click opens PsForm via openPureSignalDialogRequested (mirrors
    //     chkFWCATUBypass_MouseDown, console.cs:46149-46152 [v2.10.3.13]:
    //       if (IsRightButton(e)) linearityToolStripMenuItem_Click(null,
    //                                                              EventArgs.Empty);
    //     ).
    //
    // Coordinator is late-bound — wired via setPureSignal() when
    // RadioModel::pureSignalCoordinatorReady fires post-WDSP-init.  The
    // toggled lambda below null-guards on m_ps so pre-coordinator clicks
    // are safely no-op'd.
    if (m_psaBtn) {
        // Right-click → emit openPureSignalDialogRequested.  Wired
        // unconditionally so the seam exists even when no PureSignal
        // coordinator is bound (test harness verifies the signal emit).
        m_psaBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_psaBtn, &QWidget::customContextMenuRequested, this,
                [this](const QPoint&) {
            emit openPureSignalDialogRequested();
        });

        // Left-click toggle → PureSignal::setAutoCalEnabled (when bound).
        // Guarded on m_updatingFromModel to prevent echo loops when the
        // coordinator's autoCalEnabledChanged signal flips us back.
        connect(m_psaBtn, &QPushButton::toggled, this, [this](bool on) {
            if (m_updatingFromModel) { return; }
            if (m_ps) { m_ps->setAutoCalEnabled(on); }
        });

        // If a coordinator is already live (e.g. test injects via
        // RadioModel::pureSignal() returning non-null at construction),
        // wire it now.  Otherwise wait for the late-bind signal.
        if (m_model) {
            if (PureSignal* ps = m_model->pureSignal()) {
                setPureSignal(ps);
            }
            connect(m_model, &RadioModel::pureSignalCoordinatorReady, this,
                    &TxApplet::setPureSignal);
        }
    }

    // ── Initial sync from model ──────────────────────────────────────────────
    rebindActiveSlice();
    syncFromModel();
}

// 2026-09-13 code review fix: re-binds the MOX-tooltip (K.2) and TX-filter-
// status-label connections to whichever slice is now RadioModel's active
// slice, and immediately resyncs both from it. Previously these connected
// once, in wireControls(), to whichever slice happened to be active at
// TxApplet construction time -- which in practice was none at all (TxApplet
// is built in MainWindow::buildUI(), before any slice exists), so neither
// ever fired for the life of the app. Call this from wireControls() (once,
// at construction) and from MainWindow on every RadioModel::activeSliceChanged.
void TxApplet::rebindActiveSlice()
{
    if (m_activeSliceModeConn) {
        QObject::disconnect(m_activeSliceModeConn);
        m_activeSliceModeConn = {};
    }
    if (m_activeSliceFilterConn) {
        QObject::disconnect(m_activeSliceFilterConn);
        m_activeSliceFilterConn = {};
    }
    if (!m_model) { return; }
    SliceModel* slice = m_model->activeSlice();
    if (!slice) { return; }

    m_activeSliceModeConn = connect(slice, &SliceModel::dspModeChanged,
                                    this, &TxApplet::onMoxModeChanged);
    onMoxModeChanged(slice->dspMode());

    m_activeSliceFilterConn = connect(slice, &SliceModel::dspModeChanged,
                                       this, [this](DSPMode) {
        refreshTxFilterStatusLabel();
    });
    refreshTxFilterStatusLabel();
}

// Status label refresh helper (shared by TransmitModel::filterChanged and
// the active slice's dspModeChanged via rebindActiveSlice()).
void TxApplet::refreshTxFilterStatusLabel()
{
    if (!m_txFilterStatusBtn || !m_model) { return; }
    SliceModel* slice = m_model->activeSlice();
    const DSPMode mode = slice ? slice->dspMode() : DSPMode::USB;
    m_txFilterStatusBtn->setText(
        m_model->transmitModel().filterDisplayText(mode));
}

// ── Fusszeile: Mikrofonquelle, und der Ausfall des PC-Mikrofons ────────
//
// 2026-11-20, 13:00:24: das USB-Mikrofon verschwand vom Mac, Longpath
// hing am weg gewesenen Geraet, und das stand nur im Protokoll. Der
// Betreiber hat Entwurf A + B gewaehlt: hier, wo die Quelle ohnehin
// genannt wird, wird "PC mic" zur Warnung -- in Bernstein, der
// Warnfarbe des Hauses, nicht in Rot (Rot heisst im TX-Feld nur SWR).
// Das Gegenstueck ist der Platz "MIC" in der unteren Leiste
// (MainWindow::aktualisiereMikroPlatz).
void TxApplet::aktualisiereMikroAbzeichen()
{
    if (m_micSourceBadge == nullptr || m_model == nullptr) {
        return;
    }
    const MicSource quelle = m_model->transmitModel().micSource();
    const AudioEngine* ae = m_model->audioEngine();
    const bool ausgefallen = quelle == MicSource::Pc
        && ae != nullptr && ae->pcMikrofonAusgefallen();

    if (ausgefallen) {
        m_micSourceBadge->setText(QStringLiteral("\u25CF PC mic \u2014 no signal"));
        m_micSourceBadge->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: %2px; font-weight: bold; }")
            .arg(QLatin1String(Style::kAmberText)).arg(Style::kFontCaption));
        m_micSourceBadge->setToolTip(MikroAusfall::tooltip(
            ae->pcMikrofonName(), ae->pcMikrofonAusgefallenUm()));
        return;
    }

    QString text;
    switch (quelle) {
        case MicSource::Radio: text = QStringLiteral("Radio mic"); break;
        case MicSource::Vax:   text = QStringLiteral("VAX");       break;
        case MicSource::Pc:
        default:               text = QStringLiteral("PC mic");    break;
    }
    m_micSourceBadge->setText(text);
    m_micSourceBadge->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: %2px; }")
        .arg(QLatin1String(Style::kTextScale)).arg(Style::kFontCaption));
    m_micSourceBadge->setToolTip(QStringLiteral(
        "Active microphone source: PC mic or Radio mic.\n"
        "Change via Setup \u2192 Transmit \u2192 Mic Source."));
}

void TxApplet::syncFromModel()
{
    if (!m_model) { return; }

    TransmitModel& tx = m_model->transmitModel();
    MoxController* mox = m_model->moxController();

    m_updatingFromModel = true;

    // RF Power
    {
        QSignalBlocker b(m_rfPowerSlider);
        m_rfPowerSlider->setValue(tx.power());
    }

    // Tune Power for current band
    {
        QSignalBlocker b(m_tunePwrSlider);
        const int tunePwr = tx.tunePowerForBand(m_currentBand);
        m_tunePwrSlider->setValue(tunePwr);
    }

    // Issue #175 Task 7: refresh both labels through the central
    // formatter (HL2 dB / non-HL2 int).  Replaces the two inline
    // QString::number() calls that bypassed HL2 dB conversion above.
    updatePowerSliderLabels();

    // VOX [ON] toggle + Threshold slider + Hold slider
    // (3M-3a-iii bench polish 2026-05-04 — relocated from PhoneCwApplet).
    // Bidirectional sync to TransmitModel::voxEnabled / voxThresholdDb /
    // voxHangTimeMs.  Note: voxEnabled is NEVER persisted (safety: VOX
    // always starts OFF), but we still pull whatever the current model
    // state is so any other UI surface (e.g. Setup → DEXP/VOX page) stays
    // in agreement.
    if (m_voxBtn) {
        QSignalBlocker bv(m_voxBtn);
        m_voxBtn->setChecked(tx.voxEnabled());
    }
    if (m_voxSlider) {
        {
            QSignalBlocker b(m_voxSlider);
            m_voxSlider->setValue(tx.voxThresholdDb());
        }
        if (m_voxLvlLabel) {
            m_voxLvlLabel->setText(QStringLiteral("%1 dB").arg(tx.voxThresholdDb()));
        }
    }
    if (m_voxDlySlider) {
        {
            QSignalBlocker b(m_voxDlySlider);
            m_voxDlySlider->setValue(tx.voxHangTimeMs());
        }
        if (m_voxDlyLabel) {
            m_voxDlyLabel->setText(QStringLiteral("%1 ms").arg(tx.voxHangTimeMs()));
        }
    }

    // MON button state (J.3 Phase 3M-1b)
    // monEnabled intentionally loads as OFF — plan §0 row 9 safety rule.
    if (m_monBtn) {
        QSignalBlocker bm(m_monBtn);
        m_monBtn->setChecked(tx.monEnabled());
    }

    // Monitor volume slider (J.3 Phase 3M-1b)
    // Sync slider position from model; default 0.5f → slider 50.
    if (m_monitorVolumeSlider) {
        QSignalBlocker bvol(m_monitorVolumeSlider);
        const int uiVal = qRound(tx.monitorVolume() * 100.0f);
        m_monitorVolumeSlider->setValue(uiVal);
        m_monitorVolumeValue->setText(QString::number(uiVal));
    }

    // LEV / EQ / CFC button state (3M-3a-i Batch 2 Task F + 3M-3a-ii
    // Batch 6 Task A — CFC button added; PROC moved to PhoneCwApplet
    // 3M-3a-ii post-bench cleanup).
    if (m_levBtn) {
        QSignalBlocker b(m_levBtn);
        m_levBtn->setChecked(tx.txLevelerOn());
    }
    if (m_eqBtn) {
        QSignalBlocker b(m_eqBtn);
        m_eqBtn->setChecked(tx.txEqEnabled());
    }
    if (m_cfcBtn) {
        QSignalBlocker b(m_cfcBtn);
        m_cfcBtn->setChecked(tx.cfcEnabled());
    }

    // TX BW spinboxes + status label (Plan 4 Cluster C Task 4 / D2+D3+D9)
    if (m_txFilterLowSpin) {
        QSignalBlocker bLo(m_txFilterLowSpin);
        m_txFilterLowSpin->setValue(tx.filterLow());
    }
    if (m_txFilterHighSpin) {
        QSignalBlocker bHi(m_txFilterHighSpin);
        m_txFilterHighSpin->setValue(tx.filterHigh());
    }
    if (m_txFilterStatusBtn) {
        SliceModel* slice = m_model->activeSlice();
        const DSPMode mode = slice ? slice->dspMode() : DSPMode::USB;
        m_txFilterStatusBtn->setText(tx.filterDisplayText(mode));
    }

    // Mic-source badge (J.3 Phase 3M-1b; extended to 3-way in Phase 3M-VAX-toggle)
    aktualisiereMikroAbzeichen();

    // MOX / TUNE button state
    if (mox) {
        QSignalBlocker bm(m_moxBtn);
        m_moxBtn->setChecked(mox->isMox());

        QSignalBlocker bt(m_tuneBtn);
        const bool isManual = mox->isManualMox();
        m_tuneBtn->setChecked(isManual);
        m_tuneBtn->setText(isManual ? QStringLiteral("TUNING...") : QStringLiteral("TUNE"));
    }

    m_updatingFromModel = false;
}

// (Phase 3M-1b J.2 showVoxSettingsPopup removed in 3M-3a-iii Task 16 —
//  the wired VOX surface lives on PhoneCwApplet now, and the per-parameter
//  popup gave way to right-click → Setup → Transmit → DEXP/VOX.)

void TxApplet::setPowerScale(int maxWatts, bool hasAmplifier)
{
    if (!m_fwdPowerGauge) { return; }
    m_ampScaleActive = hasAmplifier;

    if (!hasAmplifier) {
        // Zurueck auf die Skala des Geraets — und zwar auf die
        // PER-SKU-Skala, nicht auf eine feste Barfusszahl. Ein HL2 mit
        // 5 W darf nach dem Abschalten des Verstaerkers nicht auf
        // 0-200 W stehen; er stuende dann in der ersten Vierzigstel
        // der Anzeige.
        rescaleFwdGaugeForModel(m_fwdGaugeModel);
        return;
    }

    // Code review, 2026-09-13 (changelog comparison with another
    // client -- their 2.0.16 fixed "TX stage meters kept the stock 120W scale" with a
    // lower-power amp configured): maxWatts used to be silently dropped
    // here (Q_UNUSED), so ANY amplifier snapped this gauge to the fixed
    // 2kW scale below regardless of its actual rating. That was
    // harmless in practice because both amps this class currently wires
    // up (PGXL, RF-Kit/RF2K-S) genuinely ARE ~2kW-class -- but the next
    // lower-power amp integration would have silently inherited the
    // wrong scale instead of getting its own. A caller that knows the
    // real rating now gets a scale proportional to it (same shape as
    // rescaleFwdGaugeForModel's per-SKU scale below); maxWatts<=0 keeps
    // today's exact fixed 2kW behaviour for PGXL/RF-Kit call sites,
    // which still legitimately pass 0.
    if (maxWatts > 0) {
        const double red = static_cast<double>(maxWatts);
        const double top = red * 1.2;
        m_fwdPowerGauge->setRange(0.0, top);
        m_fwdPowerGauge->setRedStart(red);
        m_fwdPowerGauge->setYellowStart(red);
        auto fmt = [maxWatts](double v) {
            return (maxWatts <= 10) ? QString::number(v, 'f', 1)
                                    : QString::number(qRound(v));
        };
        m_fwdPowerGauge->setTickLabels({
            fmt(0.0), fmt(red / 3.0), fmt(2.0 * red / 3.0), fmt(red), fmt(top),
        });
        return;
    }

    // Wortgleich mit TunerApplet::setPowerScale — dieselben Zahlen fuer
    // dieselbe Sache, damit die Anzeigen nicht auseinanderlaufen.
    // Ursprung: AetherSDR src/gui/TunerApplet.cpp:setPowerScale [@0cd4559].
    m_fwdPowerGauge->setRange(0.0, 2000.0);
    m_fwdPowerGauge->setYellowStart(1500.0);
    m_fwdPowerGauge->setRedStart(1500.0);
    m_fwdPowerGauge->setTickLabels({
        QStringLiteral("0"),    QStringLiteral("500"),
        QStringLiteral("1000"), QStringLiteral("1500"),
        QStringLiteral("2000"),
    });
}

void TxApplet::rescaleFwdGaugeForModel(HPSDRModel model)
{
    if (!m_fwdPowerGauge) { return; }
    // Die Verstaerkerskala hat Vorrang. Ohne diese Zeile setzte ein
    // Bandwechsel bei laufendem PGXL die Anzeige zurueck auf die
    // Barfussdecke des Geraets — und 800 W stuenden dann weit jenseits
    // des Anschlags.
    // Das Geraet merken, auch wenn die Skala gerade nicht gesetzt
    // wird: setPowerScale(false) stellt danach GENAU dieses wieder her.
    m_fwdGaugeModel = model;
    if (m_ampScaleActive) { return; }

    // Per-SKU PA ceiling from HpsdrModel.h paMaxWattsFor().  Bench-reported
    // #167 follow-up: 0-120 W default scale made HL2 (5 W max) and
    // ANAN-G2-1K (1000 W max) both show meaningless bar widths.
    const int maxW   = paMaxWattsFor(model);
    const double red = static_cast<double>(maxW);
    const double top = red * 1.2;   // 20% headroom past the red zone

    m_fwdPowerGauge->setRange(0.0, top);
    m_fwdPowerGauge->setRedStart(red);
    m_fwdPowerGauge->setYellowStart(red);   // no distinct yellow zone

    // Pick five ticks proportional to the new range: 0 / 1/3 / 2/3 / red / top.
    // For HL2 (red=5): 0 / 1.7 / 3.3 / 5 / 6.  For ANAN-100 (red=100):
    // 0 / 33 / 67 / 100 / 120.  For ANAN-G2-1K (red=1000): 0 / 333 / 667 / 1000 / 1200.
    auto fmt = [maxW](double v) {
        return (maxW <= 10) ? QString::number(v, 'f', 1)   // sub-watt resolution for QRP
                            : QString::number(qRound(v));
    };
    m_fwdPowerGauge->setTickLabels({
        fmt(0.0),
        fmt(red / 3.0),
        fmt(2.0 * red / 3.0),
        fmt(red),
        fmt(top),
    });
}

// ── Issue #175 Task 7: per-SKU power-slider rescale + dB labels ─────────────
//
// From mi0bot-Thetis console.cs:2098-2108 [v2.10.3.13-beta2]
//   if (HPSDRHW.HermesLite == Audio.LastRadioHardware ||
//       HPSDRModel.HERMESLITE == HardwareSpecific.Model)     // MI0BOT: Need an early indication of hardware type due to HL2 rx attenuator can be negative
//   {
//       ptbPWR.Maximum = 90;        // MI0BOT: Changes for HL2 only having a 16 step output attenuator
//       ptbPWR.Value = 0;
//       ptbPWR.LargeChange = 6;
//       ptbPWR.SmallChange = 6;
//       ptbTune.Maximum = 99;
//       ptbTune.Value = 0;
//       ptbTune.LargeChange = 3;
//       ptbTune.SmallChange = 3;
//       ...
//   }
//
// HL2: RF Power slider 0..90 step 6 (16-step output attenuator,
//      0.5 dB/step); Tune slider 0..99 step 3 (33 sub-steps).
// Other SKUs: canonical Thetis 0..100 step 1.
//
// MI0BOT: Changes for HL2 only having a 16 step output attenuator
// [original inline comment from mi0bot console.cs:2098]
void TxApplet::rescalePowerSlidersForModel(HPSDRModel model)
{
    if (!m_rfPowerSlider || !m_tunePwrSlider) { return; }

    // Cache for updatePowerSliderLabels() so the formatter sees the same
    // SKU the slider was rescaled for, even when called outside the live
    // currentRadioChanged path (unit tests, headless harness).
    m_powerSliderModel = model;

    const QSignalBlocker rfBlock(m_rfPowerSlider);
    const QSignalBlocker tunBlock(m_tunePwrSlider);

    m_rfPowerSlider->setRange(0, rfPowerSliderMaxFor(model));
    m_rfPowerSlider->setSingleStep(rfPowerSliderStepFor(model));
    m_rfPowerSlider->setPageStep(rfPowerSliderStepFor(model));

    m_tunePwrSlider->setRange(0, tuneSliderMaxFor(model));
    m_tunePwrSlider->setSingleStep(tuneSliderStepFor(model));
    m_tunePwrSlider->setPageStep(tuneSliderStepFor(model));

    // Tooltip - Thetis-faithful on every SKU.  Replaces the previous
    // "RF output power (0-100 W)" / "Tune carrier power for current band
    // (0-100 W)" wording, which contradicted Thetis semantics on every
    // SKU (the slider is a relative drive level, not watts).
    //
    // From Thetis console.resx + mi0bot-Thetis console.resx [v2.10.3.13-beta2]
    //   <data ...><value>Transmit Drive - This is a relative value and is
    //   not meant to imply watts, unless the PA profile is configured
    //   with MAX watts @ 100%</value></data>
    m_rfPowerSlider->setToolTip(QStringLiteral(
        "Transmit Drive - relative value, not watts unless the PA profile "
        "is configured with MAX watts @ 100%"));
    m_tunePwrSlider->setToolTip(QStringLiteral(
        "Tune and/or 2Tone Drive - relative value, not watts unless the "
        "PA profile is configured with MAX watts @ 100%"));

    updatePowerSliderLabels();
}

// From mi0bot-Thetis console.cs:29245-29274 [v2.10.3.13-beta2]
//   if (HardwareSpecific.Model == HPSDRModel.HERMESLITE)       // MI0BOT: HL2 has only 15 output power levels
//   {
//       ...
//       lblPWR.Text = "Drive:  " + ((Math.Round(drv / 6.0) / 2) - 7.5).ToString() + "dB";
//   }
//
// HL2 RF Power label formula:
//   dB = (round(drv/6.0)/2) - 7.5     // 0.5 dB steps in [-7.5, 0]
// HL2 Tune slider label formula (slider 0..99 -> dB -16.5..0):
//   dB = (slider/3 - 33) / 2          // inverse of the persistence formula
//
// MI0BOT: HL2 has only 15 output power levels
// [original inline comment from mi0bot console.cs:29245]
void TxApplet::updatePowerSliderLabels()
{
    if (!m_rfPowerSlider || !m_tunePwrSlider
        || !m_rfPowerValue || !m_tunePwrValue) { return; }

    // Use the cached SKU set by rescalePowerSlidersForModel() so the
    // label format always matches the slider's effective range (HL2
    // 0..90 step 6 / 0..99 step 3, all others 0..100 step 1).
    const HPSDRModel model = m_powerSliderModel;
    const int        rfVal  = m_rfPowerSlider->value();
    const int        tunVal = m_tunePwrSlider->value();

    if (model == HPSDRModel::HERMESLITE) {
        const float rfDb  = (std::round(rfVal  / 6.0f) / 2.0f) - 7.5f;
        const float tunDb = (tunVal / 3.0f - 33.0f) / 2.0f;
        m_rfPowerValue->setText(QString::number(rfDb,  'f', 1));
        m_tunePwrValue->setText(QString::number(tunDb, 'f', 1));
    } else {
        m_rfPowerValue->setText(QString::number(rfVal));
        m_tunePwrValue->setText(QString::number(tunVal));
    }
}

// Canonical TX band — derived from the active slice's frequency (which
// is what RadioModel.cpp:903-905 uses to compose the TX wire byte).
// Falls back to m_currentBand when:
//   - m_model is null (early bootstrap — TxApplet exists before
//     RadioModel pointer wired up; see TxApplet ctor parent),
//   - activeSlice() is null (no slice yet — first launch before
//     addSlice fires, or post-disconnect cleanup state).
// In both fallback cases m_currentBand is the best information we have
// and is what the pre-fix code already used.
Band TxApplet::txBand() const
{
    if (!m_model) { return m_currentBand; }
    SliceModel* slice = m_model->activeSlice();
    if (!slice) { return m_currentBand; }
    return bandFromFrequency(slice->frequency());
}

void TxApplet::setCurrentBand(Band band)
{
    // No same-band early-return: the bootstrap call from
    // MainWindow.cpp:1578 (`txApplet->setCurrentBand(pan0->band())`) fires
    // with band == m_currentBand-default (Band20m) when the panadapter
    // also opens on 20m, and we still need that call to push the loaded
    // per-band slider values into the UI on first paint.  Re-running the
    // sync on identical input is idempotent — QSlider::setValue same-value
    // is a no-op, and TransmitModel::setPower / setTunePowerForBand have
    // their own same-value early-returns — so the cost is negligible and
    // the win is correct first-paint behaviour after loadFromSettings.
    m_currentBand = band;

    if (!m_model) { return; }

    auto& tx = m_model->transmitModel();

    // Update the Tune Power slider to reflect the per-band stored value.
    {
        const int tunePwr = tx.tunePowerForBand(band);
        QSignalBlocker b(m_tunePwrSlider);
        m_updatingFromModel = true;
        m_tunePwrSlider->setValue(tunePwr);
        // Issue #175 Task 7: HL2 dB / non-HL2 integer — replaces the
        // bare m_tunePwrValue->setText(QString::number(tunePwr)) so HL2
        // operators see the dB-attenuator label rather than a raw 0..99
        // slider integer.
        updatePowerSliderLabels();
        m_updatingFromModel = false;
    }

    // Update the RF Power slider to reflect the per-band stored value —
    // ONLY when the band passed in is the canonical TX band (i.e. the
    // active slice's band).  Matches Thetis TXBand setter at
    // console.cs:17513 [v2.10.3.13] (`PWR = power_by_band[(int)value];`),
    // where `_tx_band` is single-source-of-truth for TX state.
    //
    // Why the gate: setCurrentBand is wired in MainWindow to BOTH
    // PanadapterModel::bandChanged and SliceModel::frequencyChanged, so it
    // can fire from a CTUN pan that does NOT change the slice.  Recalling
    // the panadapter band's RF power into the live slider would (a) jump
    // the displayed value off the actual TX band, and (b) leak that
    // wrong value back into the active slice's band slot via the
    // setPowerUsingTargetDbm txMode-0 side-effect on the next powerChanged
    // emission — silently corrupting per-band storage.
    //
    // Routed through setPower so the existing reverse-binding lambda
    // (TxApplet.cpp:905) paints the slider; setPower's same-value
    // early-return makes the no-band-change call free.
    if (band == txBand()) {
        tx.setPower(tx.powerForBand(band));
        // Dieselbe Hand fuer die Abstimmleistung (2026-10-31): ohne
        // sie trug ein Bandwechsel den neuen Bandwert nicht nach
        // auszen, und das Geraet stimmte mit dem Wert des vorigen
        // Bandes ab.
        tx.setTunePower(tx.tunePowerForBand(band));
    }
}

// ── Phase 3M-1b K.2: tooltipForMode ──────────────────────────────────────────
//
// Returns a MOX button tooltip string matching the active DSP mode.
// For modes deferred to a later phase, the tooltip explains why MOX won't
// engage, matching the reason string emitted by moxRejected (K.2) and the
// rejection reason from BandPlanGuard::checkMoxAllowed (K.1).
//
// Mode categories:
//   Allowed (LSB/USB/DIGL/DIGU): normal "Manual transmit (MOX)" tooltip.
//   CW (CWL/CWU):                CW TX deferred to Phase 3M-2.
//   Audio (AM/SAM/DSB/FM/DRM):   AM/FM TX deferred to Phase 3M-3 (audio modes).
//   SPEC:                        Never a TX mode.
//
// This helper is static so TxApplet tests can call it directly without
// constructing a full TxApplet instance.
// ---------------------------------------------------------------------------
// static
QString TxApplet::tooltipForMode(DSPMode mode)
{
    switch (mode) {
    case DSPMode::LSB:
    case DSPMode::USB:
    case DSPMode::DIGL:
    case DSPMode::DIGU:
        return QStringLiteral("Manual transmit (MOX)");

    case DSPMode::CWL:
    case DSPMode::CWU:
        return QStringLiteral("CW TX coming in Phase 3M-2");

    case DSPMode::AM:
    case DSPMode::SAM:
    case DSPMode::DSB:
    case DSPMode::FM:
    case DSPMode::DRM:
        return QStringLiteral("AM/FM TX coming in Phase 3M-3 (audio modes)");

    case DSPMode::SPEC:
    default:
        return QStringLiteral("Mode not supported for TX");
    }
}

// ---------------------------------------------------------------------------
// onMoxModeChanged — update MOX button tooltip when DSP mode changes.
//
// Wired to SliceModel::dspModeChanged (via RadioModel active-slice accessor)
// in wireControls(). Calls tooltipForMode(mode) to get the appropriate
// tooltip text and installs it on m_moxBtn. If the mode is an allowed SSB
// mode the tooltip reverts to the normal "Manual transmit (MOX)".
// ---------------------------------------------------------------------------
void TxApplet::onMoxModeChanged(DSPMode mode)
{
    if (m_moxBtn) {
        m_moxBtn->setToolTip(tooltipForMode(mode));
    }
}

// ── pollVoxMeter — Phase 3M-3a-iii bench polish 2026-05-04 ─────────────────
// 100 ms tick that drives m_voxPeakMeter on the TX right pane.
//
//   VOX peak (linear amplitude from TxChannel::getDexpPeakSignal()):
//     • 20 * log10(linear) → dB.
//     • Map -80..0 dB → 0..1 normalized (range matches Thetis ptbVOX scale
//       per console.Designer.cs:6018-6019 [v2.10.3.13]).
//     • Threshold marker pulled from TransmitModel::voxThresholdDb() and
//       mapped through the same -80..0 → 0..1 transform.
//
// Continuous (NOT MOX-gated) since GetDEXPPeakSignal is the live DEXP
// detector envelope, not the TX-pipeline meter.  Same rationale as the
// PhoneCwApplet::pollDexpMeters comment — see that file for the full
// narrative.  Relocated from PhoneCwApplet as part of the 2026-05-04 bench
// polish (VOX row moved to TxApplet under TUNE/MOX).
void TxApplet::pollVoxMeter()
{
    if (!m_model) { return; }
    TxChannel* ch = m_model->txChannel();
    if (!ch || !m_voxPeakMeter) { return; }

    // VOX peak: linear amplitude → dB → 0..1 over -80..0 range.
    const double linearPeak = ch->getDexpPeakSignal();
    const double voxPeakDb  = (linearPeak > 0.0)
        ? 20.0 * std::log10(linearPeak)
        : -80.0;
    const double voxPeak01  = std::clamp((voxPeakDb + 80.0) / 80.0, 0.0, 1.0);
    m_voxPeakMeter->setSignalLevel(voxPeak01);

    // VOX threshold marker: voxThresholdDb is in -80..0 dB range.
    const int thDb = m_model->transmitModel().voxThresholdDb();
    m_voxPeakMeter->setThresholdMarker(
        std::clamp((thDb + 80.0) / 80.0, 0.0, 1.0));
}

// ---------------------------------------------------------------------------
// Phase 3M-1c J.1 — setMicProfileManager
//
// Inject the per-MAC MicProfileManager.  Wires:
//   - manager.profileListChanged → rebuildProfileCombo (set membership change)
//   - manager.activeProfileChanged → combo selection update (programmatic)
// ---------------------------------------------------------------------------
void TxApplet::setMicProfileManager(MicProfileManager* mgr)
{
    if (m_micProfileMgr == mgr) { return; }

    if (m_micProfileMgr) {
        disconnect(m_micProfileMgr, nullptr, this, nullptr);
    }

    m_micProfileMgr = mgr;

    if (m_micProfileMgr) {
        // List changes → rebuild combo entries.
        connect(m_micProfileMgr, &MicProfileManager::profileListChanged,
                this, &TxApplet::rebuildProfileCombo);
        // Active changes → select the named entry without triggering a
        // setActiveProfile callback (m_updatingFromModel guards that).
        connect(m_micProfileMgr, &MicProfileManager::activeProfileChanged,
                this, [this](const QString& name) {
            if (!m_profileCombo) { return; }
            QSignalBlocker blk(m_profileCombo);
            m_updatingFromModel = true;
            const int idx = m_profileCombo->findText(name);
            if (idx >= 0) {
                m_profileCombo->setCurrentIndex(idx);
            }
            m_updatingFromModel = false;
        });
    }

    rebuildProfileCombo();
}

// ---------------------------------------------------------------------------
// Phase 3M-1c J.2 — setTwoToneController
//
// Inject the TwoToneController.  Wires the controller's
// twoToneActiveChanged signal so the button visually mirrors the
// authoritative state (covers the BandPlanGuard rejection clean-up
// from Phase I.5).
// ---------------------------------------------------------------------------
void TxApplet::setTwoToneController(TwoToneController* controller)
{
    if (m_twoToneCtrl == controller) { return; }

    if (m_twoToneCtrl) {
        disconnect(m_twoToneCtrl, nullptr, this, nullptr);
    }

    m_twoToneCtrl = controller;

    if (m_twoToneCtrl) {
        connect(m_twoToneCtrl, &TwoToneController::twoToneActiveChanged,
                this, [this](bool active) {
            if (!m_twoToneBtn) { return; }
            QSignalBlocker blk(m_twoToneBtn);
            m_updatingFromModel = true;
            m_twoToneBtn->setChecked(active);
            m_updatingFromModel = false;
        });

        // Sync initial state.
        QSignalBlocker blk(m_twoToneBtn);
        m_updatingFromModel = true;
        m_twoToneBtn->setChecked(m_twoToneCtrl->isActive());
        m_updatingFromModel = false;
    }
}

// ---------------------------------------------------------------------------
// Phase 3M-3a-ii Batch 6 (Task A) — requestOpenCfcDialog
//
// Open (or raise) the modeless TxCfcDialog.  Lazy-creates the dialog on first
// call so the construction cost is only paid when the user actually opens
// CFC settings.  The dialog is parented to this applet's top-level window so
// it floats freely; modal flag is forced false in TxCfcDialog's ctor.  We
// don't deleteLater() the dialog on close — keep it alive across opens for
// fast re-show, mirroring the TxEqDialog singleton pattern.
//
// Public-slot entry point so external surfaces can reuse the same instance:
//   - [CFC] button right-click → customContextMenuRequested → this slot.
//   - CfcSetupPage's [Configure CFC bands…] button → MainWindow connects its
//     openCfcDialogRequested signal to this slot.
//   - Future Tools menu item → connects to this slot.
// ---------------------------------------------------------------------------
void TxApplet::requestOpenCfcDialog()
{
    if (!m_model) { return; }

    if (!m_cfcDialog) {
        QWidget* host = window();
        m_cfcDialog = new TxCfcDialog(
            &m_model->transmitModel(),
            m_model->txChannel(),
            host ? host : static_cast<QWidget*>(this));
    } else {
        // Connection may have come up since the dialog was created.
        // Refresh the TxChannel pointer so the bar chart timer can poll WDSP.
        m_cfcDialog->setTxChannel(m_model->txChannel());
    }
    m_cfcDialog->show();
    m_cfcDialog->raise();
    m_cfcDialog->activateWindow();
}

// ---------------------------------------------------------------------------
// Phase 3M-4 Task 13 — setBoardCapabilities
//
// Hide / show the [PS-A] button based on caps.hasPureSignal.  Called by
// MainWindow on RadioModel::currentRadioChanged (and once on startup).
// Mirrors the RxApplet::setBoardCapabilities pattern.  No-op when m_psaBtn
// is null (e.g. during early construction or after teardown).
// ---------------------------------------------------------------------------
void TxApplet::setBoardCapabilities(const Longpath::BoardCapabilities& caps)
{
    if (!m_psaBtn) { return; }
    m_psaBtn->setVisible(caps.hasPureSignal);
}

// ---------------------------------------------------------------------------
// Phase 3M-4 Task 13 — setPureSignal (late-bound coordinator)
//
// Re-arm the [PS-A] toggle's bidirectional binding when the PureSignal
// coordinator becomes available (post-WDSP-init).  Disconnects the prior
// coordinator's autoCalEnabledChanged echo before rewiring; the toggled
// lambda in wireControls() reads m_ps live so it picks up the new pointer
// without rewiring.
//
// Pass nullptr to clear bindings on teardown (RadioModel emits this at
// disconnect via pureSignalCoordinatorReady(nullptr)).
// ---------------------------------------------------------------------------
void TxApplet::setPureSignal(Longpath::PureSignal* coordinator)
{
    if (m_ps == coordinator) { return; }

    if (m_ps) {
        // Drop the prior coordinator's signal subscriptions targeting us.
        disconnect(m_ps, nullptr, this, nullptr);
    }
    m_ps = coordinator;

    if (!m_psaBtn) { return; }

    if (m_ps) {
        connect(m_ps, &Longpath::PureSignal::autoCalEnabledChanged, this,
                [this](bool on) {
            if (!m_psaBtn) { return; }
            QSignalBlocker blk(m_psaBtn);
            m_updatingFromModel = true;
            m_psaBtn->setChecked(on);
            m_updatingFromModel = false;
        });
        // Initial sync from coordinator state.
        QSignalBlocker blk(m_psaBtn);
        m_updatingFromModel = true;
        m_psaBtn->setChecked(m_ps->isAutoCalEnabled());
        m_updatingFromModel = false;
    } else {
        // Coordinator gone — reset toggle state (safe default).
        QSignalBlocker blk(m_psaBtn);
        m_updatingFromModel = true;
        m_psaBtn->setChecked(false);
        m_updatingFromModel = false;
    }
}

// ---------------------------------------------------------------------------
// Phase 3M-1c J.1 — rebuildProfileCombo
//
// Rebuild combo entries from m_micProfileMgr->profileNames().  Preserves
// the active-profile selection where possible; otherwise the manager's
// activeProfileName() is selected.  No-op when manager is null.
// ---------------------------------------------------------------------------
void TxApplet::rebuildProfileCombo()
{
    if (!m_profileCombo) { return; }
    if (!m_micProfileMgr) {
        // No manager → leave the placeholder "Default" item alone.
        return;
    }

    QSignalBlocker blk(m_profileCombo);
    m_updatingFromModel = true;

    const QStringList names = m_micProfileMgr->profileNames();
    const QString active = m_micProfileMgr->activeProfileName();

    m_profileCombo->clear();
    m_profileCombo->addItems(names);

    const int idx = m_profileCombo->findText(active);
    if (idx >= 0) {
        m_profileCombo->setCurrentIndex(idx);
    }

    m_updatingFromModel = false;
}

} // namespace Longpath
