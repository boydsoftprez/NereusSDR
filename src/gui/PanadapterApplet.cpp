// no-port-check: AetherSDR-derived Longpath file. Per-pan container
// (SpectrumWidget host, slice association) is adapted structurally from
// AetherSDR src/gui/PanadapterApplet.{h,cpp} [@0cd4559]. Registered in
// docs/attribution/aethersdr-reconciliation.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/PanadapterApplet.cpp  (Longpath)
// =================================================================
//
// Ported (structurally) from AetherSDR src/gui/PanadapterApplet.{h,cpp}
// [@0cd4559].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// See PanadapterApplet.h for full Modification history (Longpath).
// =================================================================

#include "gui/PanadapterApplet.h"
#include <QFileDialog>
#include <QImage>
#include <QMessageBox>
#include "gui/SpectrumWidget.h"
#include "gui/instruments/ReadingSource.h"
#include "gui/meters/MeterPoller.h"
#include "gui/widgets/SpectrumStatusOverlay.h"
#include "models/SliceModel.h"
#include "core/AppSettings.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>
#include <QPointer>
#include <QProgressBar>
#include <QResizeEvent>
#include <QVBoxLayout>
#include "gui/styles/ThemeQss.h"
#include "gui/StyleConstants.h"
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>

namespace Longpath {

PanadapterApplet::PanadapterApplet(const QString& panId, QWidget* parent)
    : QWidget(parent)
    , m_panId(panId)
    , m_spectrum(new SpectrumWidget(this))
{
    // ── Die gemerkte Anzeigequelle wiederherstellen ─────────────────
    //
    // Gegenstueck zum Speichern im Kontextmenue. Hier und nicht
    // spaeter: sonst zeigte der Panadapter beim Start kurz das Geraet,
    // bevor er auf den KiwiSDR umspringt — ein Flackern, das wie ein
    // Fehler aussieht.
    const QString persistKey = QStringLiteral("Pan_%1").arg(panId);
    if (AppSettings::instance()
            .value(persistKey + QStringLiteral("_KiwiDisplay"),
                   QStringLiteral("False")).toString()
        == QStringLiteral("True")) {
        m_spectrum->setKiwiDisplaySource(true);
    }
    // Schluessel ZULETZT: das Wiederherstellen oben soll nicht sofort
    // wieder speichern, was es gerade gelesen hat.
    m_spectrum->setPersistKey(persistKey);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Kopfleiste ────────────────────────────────────────────────────
    //
    // Die Vorlage setzt ueber den Panadapter eine Zeile mit Punkt, Namen und
    // laufender Mittenfrequenz. Die fehlte hier ganz: der Panadapter
    // war die einzige Flaeche ohne Kopf, obwohl jede Applet daneben
    // einen hat.
    //
    // ABSICHTLICH OHNE die Zoom- und Geschwindigkeitsregler, die die Vorlage
    // dort ebenfalls zeigt. Die Vorlage hat keine Overlay-Leiste; wir haben
    // eine, und sie traegt beide Regler bereits. Sie hier zu
    // wiederholen waere genau die Doppelung, die an diesem Tag zweimal
    // aufgeraeumt wurde.
    // Entwurf 4 vom 2026-08-19, auf Ansage des Betreibers („padapter
    // noch immer nicht in einem window"): DIESELBE Handschrift wie der
    // Kopf jedes Containers — gelber Griffstrich links, Name in
    // Versalien, rechts der Schalter. Vorher trug der Panadapter einen
    // eigenen Kopf mit eigenen Abstaenden und keinem Schalter; er sah
    // aus wie ein Fremdkoerper zwischen den Kacheln.
    auto* head = new QWidget(this);
    head->setFixedHeight(22);
    auto* headLay = new QHBoxLayout(head);
    headLay->setContentsMargins(5, 0, 3, 0);
    headLay->setSpacing(6);

    // Der gelbe Strich wie in der Vorlage und wie im Container-Kopf.
    m_grip = new QLabel(head);
    m_grip->setFixedWidth(3);
    m_grip->setStyleSheet(QStringLiteral(
        "background: %1; border-radius: 1px; margin: 4px 0;")
        .arg(QLatin1String(Style::kAmberText)));
    headLay->addWidget(m_grip);

    // Der gruene Punkt bleibt: er sagt, dass Daten fliessen. Ein leeres
    // Bild ohne ihn ist ein Fehler, mit ihm eine stille Frequenz.
    auto* dot = new QLabel(QString::fromUtf8("\xe2\x97\x8f"), head);
    dot->setStyleSheet(Style::themed(QStringLiteral(
        "QLabel { color: %1; background: transparent; }")
        .arg(Style::kGreenText)));
    headLay->addWidget(dot);

    m_titleLabel = new QLabel(QStringLiteral("PANADAPTER"), head);
    m_titleLabel->setFont(Style::capsFont(m_titleLabel->font(),
                                          Style::kFontCaption));
    // Ziffernbreite: die Mittenfrequenz laeuft beim Wischen mit, und
    // eine Zahl, die dabei die Breite wechselt, zappelt.
    QFont headFont = m_titleLabel->font();
    headFont.setStyleHint(QFont::Monospace);
    m_titleLabel->setFont(headFont);
    m_titleLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background: transparent; letter-spacing: 1px; }")
        .arg(QLatin1String(Style::kTextPrimary)));
    headLay->addWidget(m_titleLabel);
    headLay->addStretch(1);

    // Ablösen und zurueck, EIN Schalter. Bisher lag „Float this pan" nur
    // im Rechtsklick-Menue und der Rueckweg gar nirgends — man musste
    // das Fenster schliessen und wissen, dass genau das zurueckdockt.
    //
    // ALS ERKENNBARER KNOPF, nicht als blosses Zeichen. Erster Anlauf am
    // 2026-08-20 setzte hier ein randloses graues ↗ — direkt neben die
    // beiden lauten Marken CH 0 (blau) und TX (rot). Beim Selbsttest an
    // der laufenden Anwendung gesehen: es liest sich als Verzierung,
    // nicht als Bedienelement. Der Betreiber hat es dreimal nicht
    // gefunden und dreimal gesagt „padapter noch immer in keinem
    // einzelnen window" — obwohl der Weg dahinter funktionierte.
    //
    // Ein Knopf, den niemand als Knopf erkennt, ist kein Knopf.
    // Deshalb: eigener Grund, sichtbarer Rand, dieselbe Groesse wie im
    // Container-Kopf, und ein Trennstrich davor, damit er nicht zu den
    // Marken gehoert.
    {
        auto* sep = new QLabel(head);
        sep->setFixedWidth(1);
        sep->setStyleSheet(QStringLiteral("background: %1; margin: 5px 0;")
                               .arg(QLatin1String(Style::kBorder)));
        headLay->addWidget(sep);
    }

    // ── Der Zahnrad-Knopf ────────────────────────────────────────────
    //
    // Der Betreiber, 2026-08-20: „weiters fehlt auch die option button
    // um beim pandapter gleich etwas zu aendern wie zb bild, groesse
    // usw."
    //
    // Bild, Deckkraft und Grundfarbe lagen bisher nur unter
    // Setup -> Display -> Colors & Theme, also drei Klicks und ein
    // Dialog entfernt von der Flaeche, die man gerade ansieht. Der
    // Panadapter ist das Ding, an dem man am haeufigsten dreht; das
    // Naheliegende gehoert an seine eigene Kopfleiste.
    //
    // Nur das Haeufige steht im Menue. Der letzte Eintrag fuehrt in
    // den Setup-Dialog — ein Menue, das ALLES kann, waere wieder der
    // Dialog, nur schlechter.
    // \u2500\u2500 2D/3D ohne Umweg (2026-10-30) \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
    //
    // Betreiber: \u201eich m\u00f6chte dort auch viel schneller \u2014 als eigenen
    // Punkt \u2014 das Umstellen von 2D auf 3D haben."
    //
    // Bisher lag es im Ueberblendmenue des Panadapters: aufklappen,
    // suchen, waehlen. Es ist aber der Griff, den man beim Hinsehen
    // macht, nicht beim Einrichten \u2014 also gehoert er neben das
    // Zahnrad, als eigener Knopf, der gleich zeigt, was gerade laeuft.
    //
    // Die Beschriftung nennt den ZUSTAND, nicht das Ziel: steht dort
    // \u201e3D", ist 3D an. Ein Knopf, der sein Ziel nennt, liest sich beim
    // zweiten Hinsehen falsch herum.
    m_btn3d = new QPushButton(QStringLiteral("2D"), head);
    m_btn3d->setFixedSize(34, 24);
    m_btn3d->setCursor(Qt::PointingHandCursor);
    // Englisch, wie die ganze Oberflaeche \u2014 die deutsche Fassung
    // gehoert nach translations/longpath_de.ts, nicht hierher
    // (tst_oberflaeche_englisch haelt die Decke bei 9).
    m_btn3d->setToolTip(tr("Flat trace or 3D surface \u2014 switches the "
                           "panadapter between 2D and 3D"));
    m_btn3d->setObjectName(QStringLiteral("panHeader3dToggle"));
    headLay->addWidget(m_btn3d);

    m_btnOptions = new QPushButton(QStringLiteral("\u2699"), head);
    // \u2500\u2500 Das Zahnrad war ein Punkt (2026-10-30) \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
    //
    // Betreiber: \u201ebitte im Panadapter das Symbol f\u00fcr Option
    // vergr\u00f6ssern, dies ist nur ein kleiner Punkt."
    //
    // 22x18 bei 11 px Schriftgrad \u2014 das Zahnrad fiel darin auf wenige
    // Bildpunkte zusammen und war auf dem Retina-Schirm kaum als
    // Zahnrad zu erkennen. Jetzt 30x24 bei 17 px; es bleibt der
    // kleinste Knopf der Leiste, aber man sieht, was er ist.
    m_btnOptions->setFixedSize(30, 24);
    m_btnOptions->setCursor(Qt::PointingHandCursor);
    m_btnOptions->setToolTip(QStringLiteral(
        "Anzeige: Hintergrundbild, Deckkraft, Grundfarbe"));
    const QString kKopfKnopf = QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2;"
        "  border-radius: 4px; color: %3; font-size: %7px; padding: 0; }"
        "QPushButton:hover { background: %4; color: %5;"
        "  border-color: %6; }")
        .arg(QLatin1String(Style::kButtonBg),
             QLatin1String(Style::kBorder),
             QLatin1String(Style::kTextSecondary),
             QLatin1String(Style::kButtonHover),
             QLatin1String(Style::kTextPrimary),
             QLatin1String(Style::kAccent));
    m_btnOptions->setStyleSheet(kKopfKnopf.arg(17));
    m_btn3d->setStyleSheet(kKopfKnopf.arg(12));
    headLay->addWidget(m_btnOptions);
    connect(m_btnOptions, &QPushButton::clicked, this, [this]() {
        QMenu* m = buildDisplayMenu(window());   // am Fenster, siehe contextMenuEvent()
        m->exec(m_btnOptions->mapToGlobal(
            QPoint(0, m_btnOptions->height())));
        m->deleteLater();
    });

    // Den Knopf an die Flaeche haengen: er zeigt ihren Zustand und
    // stellt ihn um, und er folgt ihr auch dann, wenn jemand die Art
    // ueber das Ueberblendmenue wechselt.
    if (m_spectrum) {
        const auto zeige = [this]() {
            if (!m_btn3d || !m_spectrum) { return; }
            const bool raum = m_spectrum->spectrumRenderMode()
                              == SpectrumRenderMode::Mode3D;
            m_btn3d->setText(raum ? QStringLiteral("3D")
                                  : QStringLiteral("2D"));
        };
        zeige();
        connect(m_spectrum, &SpectrumWidget::spectrumRenderModeChanged,
                this, [zeige](SpectrumRenderMode) { zeige(); });
        connect(m_btn3d, &QPushButton::clicked, this, [this]() {
            if (!m_spectrum) { return; }
            const bool raum = m_spectrum->spectrumRenderMode()
                              == SpectrumRenderMode::Mode3D;
            m_spectrum->setSpectrumRenderMode(raum
                                                  ? SpectrumRenderMode::Mode2D
                                                  : SpectrumRenderMode::Mode3D);
        });
    }

    m_btnFloat = new QPushButton(QStringLiteral("\u2197"), head);
    m_btnFloat->setFixedSize(22, 18);
    m_btnFloat->setCursor(Qt::PointingHandCursor);
    m_btnFloat->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2;"
        "  border-radius: 4px; color: %3; font-size: 11px; padding: 0; }"
        "QPushButton:hover { background: %4; color: %5;"
        "  border-color: %6; }")
        .arg(QLatin1String(Style::kButtonBg),
             QLatin1String(Style::kBorder),
             QLatin1String(Style::kTextSecondary),
             QLatin1String(Style::kButtonHover),
             QLatin1String(Style::kTextPrimary),
             QLatin1String(Style::kAccent)));
    headLay->addWidget(m_btnFloat);
    connect(m_btnFloat, &QPushButton::clicked, this, [this]() {
        if (m_floating) { emit dockRequested(m_panId); }
        else            { emit floatRequested(m_panId); }
    });
    setFloatingIndicator(false);

    // Leichtes Rendering (2026-09-26): derselbe Kopf wie an jedem
    // anderen Panel (Verlauf + Lichtkante, Style::titleBarStyle), statt
    // flach. Nur fuer den Kopf selbst -- die alte Regel "QWidget { .. }"
    // gab jedem Kind im Kopf eine eigene Flaeche und eine Unterkante.
    head->setObjectName(QStringLiteral("panHead"));
    head->setAttribute(Qt::WA_StyledBackground, true);
    head->setStyleSheet(QStringLiteral("QWidget#panHead { %1 }")
                            .arg(Style::titleBarStyle()));
    layout->addWidget(head, 0);

    // ── Die Anzeigezeile (2026-11-06) ────────────────────────────────
    //
    // Unter dem Kopf, ueber dem Spektrum: Abstimmfrequenz links in Messing,
    // SWR rechts, gross wie auf dem gewaehlten Blatt. Skala und Schwelle
    // kommen aus ReadingSource (readingFor), nicht von hier: keine zweite
    // Skala. Verborgen, solange kein Haken gesetzt ist: dann bekommt das
    // Spektrum die ganze Hoehe, wie bisher.
    m_anzeigeZeile = new QWidget(this);
    m_anzeigeZeile->setObjectName(QStringLiteral("panReadoutRow"));
    m_anzeigeZeile->setAttribute(Qt::WA_StyledBackground, true);
    m_anzeigeZeile->setStyleSheet(QStringLiteral(
        "QWidget#panReadoutRow { background: %1; border-bottom: 1px solid %2; }")
        .arg(QLatin1String(Style::kPanelBg), QLatin1String(Style::kBorder)));
    m_anzeigeZeile->setFixedHeight(34);
    auto* zeile = new QHBoxLayout(m_anzeigeZeile);
    zeile->setContentsMargins(12, 0, 12, 0);
    zeile->setSpacing(8);
    m_anzeigeFrequenz = new QLabel(QString::fromUtf8("\xe2\x80\x94"), m_anzeigeZeile);
    m_anzeigeFrequenz->setObjectName(QStringLiteral("panReadoutFrequency"));
    m_anzeigeFrequenz->setFont(Style::monoFont(m_anzeigeFrequenz->font(), Style::kFontReading));
    m_anzeigeFrequenz->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background: transparent; }")
        .arg(QLatin1String(Style::kAmberText)));
    zeile->addWidget(m_anzeigeFrequenz);
    m_anzeigeEinheit = new QLabel(QStringLiteral("MHz"), m_anzeigeZeile);
    m_anzeigeEinheit->setFont(Style::capsFont(m_anzeigeEinheit->font(), Style::kFontCaption));
    m_anzeigeEinheit->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background: transparent; }")
        .arg(QLatin1String(Style::kAmberDim)));
    zeile->addWidget(m_anzeigeEinheit);
    zeile->addStretch(1);
    // ── Das S-Meter (2026-11-17) ─────────────────────────────────────
    // Zwischen Frequenz und SWR, gebaut wie das SWR-Stueck: Name, Wert,
    // Balken. Dazu klein die dBm -- die S-Stufe sagt „wie stark", die
    // Zahl sagt es genau, und im Instrument stehen auch beide.
    m_anzeigeSMeter = new QWidget(m_anzeigeZeile);
    {
        auto* sZeile = new QHBoxLayout(m_anzeigeSMeter);
        sZeile->setContentsMargins(0, 0, 0, 0);
        sZeile->setSpacing(8);
        auto* sName = new QLabel(QStringLiteral("S"), m_anzeigeSMeter);
        sName->setFont(Style::capsFont(sName->font(), Style::kFontCaption));
        sName->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; background: transparent; }")
            .arg(QLatin1String(Style::kAmberDim)));
        sZeile->addWidget(sName);
        m_anzeigeSWert = new QLabel(m_anzeigeSMeter);
        m_anzeigeSWert->setObjectName(QStringLiteral("panReadoutSMeter"));
        m_anzeigeSWert->setFont(Style::monoFont(m_anzeigeSWert->font(), Style::kFontReading));
        m_anzeigeSWert->setMinimumWidth(92);   // „S9+60": gleich breit, nichts zappelt
        sZeile->addWidget(m_anzeigeSWert);
        m_anzeigeSBalken = new QProgressBar(m_anzeigeSMeter);
        m_anzeigeSBalken->setRange(0, 1000);
        m_anzeigeSBalken->setTextVisible(false);
        m_anzeigeSBalken->setFixedSize(120, 8);
        sZeile->addWidget(m_anzeigeSBalken);
        m_anzeigeSDbm = new QLabel(m_anzeigeSMeter);
        m_anzeigeSDbm->setObjectName(QStringLiteral("panReadoutSMeterDbm"));
        m_anzeigeSDbm->setFont(Style::capsFont(m_anzeigeSDbm->font(), Style::kFontCaption));
        m_anzeigeSDbm->setMinimumWidth(64);
        m_anzeigeSDbm->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; background: transparent; }")
            .arg(QLatin1String(Style::kAmberDim)));
        sZeile->addWidget(m_anzeigeSDbm);
    }
    zeile->addWidget(m_anzeigeSMeter);
    // Etwas Luft zum SWR, wenn beide stehen.
    zeile->addSpacing(12);
    m_anzeigeSwr = new QWidget(m_anzeigeZeile);
    auto* swrZeile = new QHBoxLayout(m_anzeigeSwr);
    swrZeile->setContentsMargins(0, 0, 0, 0);
    swrZeile->setSpacing(8);
    auto* swrName = new QLabel(QStringLiteral("SWR"), m_anzeigeSwr);
    swrName->setFont(Style::capsFont(swrName->font(), Style::kFontCaption));
    swrName->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background: transparent; }")
        .arg(QLatin1String(Style::kAmberDim)));
    swrZeile->addWidget(swrName);
    m_anzeigeSwrWert = new QLabel(m_anzeigeSwr);
    m_anzeigeSwrWert->setObjectName(QStringLiteral("panReadoutSwr"));
    m_anzeigeSwrWert->setFont(Style::monoFont(m_anzeigeSwrWert->font(), Style::kFontReading));
    m_anzeigeSwrWert->setMinimumWidth(62);   // Zahl und Strich: gleich breit, nichts zappelt
    swrZeile->addWidget(m_anzeigeSwrWert);
    m_anzeigeSwrBalken = new QProgressBar(m_anzeigeSwr);
    m_anzeigeSwrBalken->setRange(0, 1000);
    m_anzeigeSwrBalken->setTextVisible(false);
    m_anzeigeSwrBalken->setFixedSize(120, 8);
    swrZeile->addWidget(m_anzeigeSwrBalken);
    m_anzeigeSwrWarnung = new QLabel(QStringLiteral("!"), m_anzeigeSwr);
    m_anzeigeSwrWarnung->setFixedWidth(10);
    m_anzeigeSwrWarnung->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background: transparent; font-weight: bold; }")
        .arg(QLatin1String(Style::kAmberText)));
    swrZeile->addWidget(m_anzeigeSwrWarnung);
    zeile->addWidget(m_anzeigeSwr);
    aktualisiereAnzeigeSwr();
    aktualisiereAnzeigeSMeter();
    m_anzeigeZeile->hide();
    layout->addWidget(m_anzeigeZeile, 0);

    layout->addWidget(m_spectrum, 1);

    // Die Mitte laeuft mit. Ohne Verbindung steht nur der Name da —
    // eine Frequenz ohne Radio waere eine Behauptung.
    auto zeigeMitte = [this](double centerHz) {
        m_titleLabel->setText(QString::fromUtf8("PANADAPTER \xc2\xb7 %1 MHz")
            .arg(centerHz / 1.0e6, 0, 'f', 6));
    };
    connect(m_spectrum, &SpectrumWidget::frequencyRangeChanged, this,
            [zeigeMitte](double centerHz, double) { zeigeMitte(centerHz); });

    // ── Die Mitte wandert auch auf dem ANDEREN Weg (2026-10-27) ──────
    //
    // Gemessen, nachdem die Flaeche jeden gesetzten Ausschnitt meldet:
    // `setFrequencyRange` ist nicht die einzige Stelle, die
    // `m_centerHz` bewegt. VIER weitere schreiben ihn direkt —
    // Zoomziehen auf den VFO, das Nachscrollen an den beiden
    // Bildraendern (Thetis console.cs:31371, W2PA) und
    // `recenterOnVfo`. Alle vier melden `centerChanged`, keine
    // `frequencyRangeChanged`.
    //
    // Die Kopfzeile hing nur am zweiten Signal. Darum stand am Geraet
    // „PANADAPTER · 14.105191 MHz" ueber einer Skala von 7,13 bis
    // 7,21: die Skala malt aus `m_centerHz`, der dem VFO gefolgt war,
    // die Kopfzeile aus dem letzten Wert, der durch
    // `setFrequencyRange` kam.
    //
    // Sie haengt jetzt an beiden. Nicht umgekehrt — aus den vier
    // Stellen zusaetzlich `frequencyRangeChanged` zu werfen, haette
    // alles mitgezogen, was daran haengt (Ablage, Weitbandrechnung),
    // und zwar bei jedem gescrollten Bildpunkt.
    connect(m_spectrum, &SpectrumWidget::centerChanged, this,
            [zeigeMitte](double centerHz) { zeigeMitte(centerHz); });

    // ── Kein Setzen beim Bauen (2026-10-27, verworfen) ───────────────
    //
    // Hier stand ein Griff, der die Kopfzeile gleich beim Bauen aus
    // der Flaeche fuellt. Er war eine VERMUTUNG ueber die Ursache von
    // „PANADAPTER · 14.105191 MHz" ueber einer Skala von 7,13 bis
    // 7,21 MHz — und er hat den Pruefstand umgeworfen, der sagt: ohne
    // Radio steht NUR der Name da. Zu Recht: beim Bauen steht in der
    // Flaeche die Vorgabe, nicht das Band des Betreibers.
    //
    // Die Ursache ist damit offen. Was feststeht: die Skala malt aus
    // `m_centerHz` (hzToX), die Kopfzeile liest denselben Wert ueber
    // `frequencyRangeChanged` — sie koennen nur auseinanderlaufen,
    // wenn eine Aenderung das Signal nicht erreicht. Die Flaeche
    // meldet jeden gesetzten Ausschnitt jetzt mit ihrem Namen ins
    // Protokoll; der naechste Lauf am Geraet zeigt, welcher fehlt.

    // Phase 3F Sub-Epic E Task 2: per-pan status overlay in top-right.
    // Positioned manually in resizeEvent so the spectrum host owns the full
    // applet area underneath. Parented to `this`, not m_spectrum, so it sits
    // above the SpectrumWidget's QRhi surface without becoming a child of it.
    m_statusOverlay = new SpectrumStatusOverlay(this);
    m_statusOverlay->raise();
    // Initial state for the KIWI pill (see its own header comment) --
    // m_spectrum may already have been switched to the KiwiSDR source by
    // the persisted-key restore above, before this overlay existed.
    m_statusOverlay->setKiwiActive(m_spectrum->kiwiDisplaySource());

    // Phase 3F: clicking anywhere in this pan makes it the active pan.
    //
    // `activated` was declared with the comment "emitted on any click within
    // applet" and had no emitter, so PanadapterStack::setActivePan was only
    // ever called for the first pan created. Every consumer of activePanId()
    // was therefore pinned to pan-0 for the session -- including "Add slice on
    // active pan", which is the ONLY route that puts a slice on a pan (the
    // per-pan +RX button is still disabled, NYI). A second pan could be
    // created but never given a slice, so it showed no VFO flag, no dial
    // frequency, and no FFT subscription.
    //
    // Ported from AetherSDR PanadapterApplet.cpp:629 [@6a142807]:
    //     if (ev->type() == QEvent::MouseButtonPress)
    //         emit activated(m_panId);
    // in eventFilter rather than mousePressEvent, because the spectrum host
    // and the overlay consume presses before they ever reach this widget.
    m_spectrum->installEventFilter(this);
    m_statusOverlay->installEventFilter(this);
    installEventFilter(this);

    connect(m_statusOverlay, &SpectrumStatusOverlay::txBadgeClicked, this,
            [this]() { emit txBadgeClicked(m_panId); });
    connect(m_statusOverlay, &SpectrumStatusOverlay::wideBadgeClicked, this,
            [this]() { emit wideBadgeClicked(m_panId); });
    connect(m_statusOverlay, &SpectrumStatusOverlay::chainTagClicked, this,
            [this](int chainIdx) { emit chainTagClicked(m_panId, chainIdx); });
    // Clicking the KIWI pill switches straight back to the device -- the
    // one-click "off" half of "kiwi sollte sich klar ein und ausschalten
    // lassen" (Betreiber, 2026-08-27). Turning it back on stays a
    // deliberate right-click menu action, not a click on an absent pill.
    connect(m_statusOverlay, &SpectrumStatusOverlay::kiwiBadgeClicked, this,
            &PanadapterApplet::zeigeDasGeraet);

    // Phase 3F Sub-Epic F Task 13: restore persisted extended-view
    // toggle (default true) and apply it to the embedded spectrum immediately.
    auto& s = AppSettings::instance();
    const QString stored = s.value(QStringLiteral("Pan_%1_ExtendedView").arg(m_panId),
                                   QStringLiteral("True")).toString();
    setExtendedViewEnabled(stored == QStringLiteral("True"));
}

PanadapterApplet::~PanadapterApplet() = default;

void PanadapterApplet::zeigeDasGeraet()
{
    m_spectrum->setKiwiDisplaySource(false);
    m_statusOverlay->setKiwiActive(false);
}

void PanadapterApplet::addSlice(int sliceIndex)
{
    m_associatedSlices.insert(sliceIndex);
    if (m_activeSliceIndex == -1) {
        setActiveSliceIndex(sliceIndex);
    }
}

void PanadapterApplet::removeSlice(int sliceIndex)
{
    m_associatedSlices.remove(sliceIndex);
    if (m_activeSliceIndex == sliceIndex) {
        m_activeSliceIndex = m_associatedSlices.isEmpty() ? -1 : *m_associatedSlices.begin();
        emit activeSliceChanged(m_panId, m_activeSliceIndex);
    }
}

void PanadapterApplet::setActiveSliceIndex(int sliceIndex)
{
    if (m_activeSliceIndex == sliceIndex) { return; }
    m_activeSliceIndex = sliceIndex;
    // Bench-reported 2026-07-28 (Sub-Epic J): the newly-active slice's flag
    // must come to the front of THIS pan's stack -- see
    // SpectrumWidget::setFrontSliceIndex for why a plain raise() would not
    // hold. This is the single writer of m_activeSliceIndex from outside
    // (the addSlice seed path below also routes through here), so hanging
    // the z-order pin off it covers every caller, including
    // PanadapterStack::setActiveSliceOnHostingPan, without each one having
    // to remember to raise the flag itself.
    if (m_spectrum) {
        m_spectrum->setFrontSliceIndex(sliceIndex);
    }
    emit activeSliceChanged(m_panId, sliceIndex);
}

void PanadapterApplet::setCenterMhz(double mhz) { m_centerMhz = mhz; }
void PanadapterApplet::setBandwidthMhz(double bw) { m_bandwidthMhz = bw; }

// Phase 3F: any press inside this pan claims active-pan status. Watches the
// spectrum host and the overlay as well as the applet itself, because both sit
// on top of it and accept the press first.
//
// Ported from AetherSDR PanadapterApplet.cpp:628-630 [@6a142807]:
//     if (ev->type() == QEvent::MouseButtonPress)
//         emit activated(m_panId);
//     return QWidget::eventFilter(obj, ev);
//
// Observing, not intercepting: always falls through to the base implementation
// so the spectrum keeps its own click handling (tune, drag, zoom).
bool PanadapterApplet::eventFilter(QObject* obj, QEvent* ev)
{
    if (ev->type() == QEvent::MouseButtonPress) {
        emit activated(m_panId);
    }
    return QWidget::eventFilter(obj, ev);
}

void PanadapterApplet::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    repositionStatusOverlay();
}

QByteArrayList PanadapterApplet::statusOverlaySliceProperties()
{
    // Every SliceModel property updateStatusOverlay reads below. Keep the two
    // in step: a field painted with no entry here is a field that goes stale.
    //
    // streamIndex earns its place even though the overlay never paints it --
    // it is what the caller's chain resolution keys off, so a slice moving
    // between DDC streams changes the CH tag without touching any painted
    // property.
    //
    // sliceLetter is deliberately absent: the letter is derived from the
    // stable slice id, which cannot change for the life of the slice.
    return QByteArrayList{
        QByteArrayLiteral("frequency"),
        QByteArrayLiteral("dspMode"),
        QByteArrayLiteral("txSlice"),
        QByteArrayLiteral("diversityEnabled"),
        QByteArrayLiteral("psPaused"),
        QByteArrayLiteral("streamIndex"),
    };
}

void PanadapterApplet::setFrequencyReadoutShown(bool on)
{
    if (m_zeigeFrequenz == on) { return; }
    m_zeigeFrequenz = on;
    aktualisiereAnzeigeZeile();
}

void PanadapterApplet::setSwrReadoutShown(bool on)
{
    if (m_zeigeSwr == on) { return; }
    m_zeigeSwr = on;
    aktualisiereAnzeigeZeile();
}

void PanadapterApplet::setSMeterReadoutShown(bool on)
{
    if (m_zeigeSMeter == on) { return; }
    m_zeigeSMeter = on;
    aktualisiereAnzeigeZeile();
}

void PanadapterApplet::aktualisiereAnzeigeZeile()
{
    if (!m_anzeigeZeile) { return; }
    m_anzeigeFrequenz->setVisible(m_zeigeFrequenz);
    m_anzeigeEinheit->setVisible(m_zeigeFrequenz);
    m_anzeigeSwr->setVisible(m_zeigeSwr);
    if (m_anzeigeSMeter) { m_anzeigeSMeter->setVisible(m_zeigeSMeter); }
    m_anzeigeZeile->setVisible(m_zeigeFrequenz || m_zeigeSwr || m_zeigeSMeter);
    // Der Streifen haengt sonst in die Flaeche unter dem Kopf -- das waere
    // jetzt die Zeile. Siehe repositionStatusOverlay().
    repositionStatusOverlay();
}

void PanadapterApplet::onReading(int bindingId, double value)
{
    // Auch verborgen fuettern: sonst stuende beim Einschalten erst einmal
    // nichts da (dieselbe Ueberlegung wie in FrequencyApplet::onReading).
    if (bindingId == MeterBinding::TxPower) {
        m_vorlaufW = value;
    } else if (bindingId == MeterBinding::TxSwr) {
        m_swrWert = value;
    } else if (bindingId == MeterBinding::SignalAvg) {
        // Derselbe Wert, den das S-Meter-Instrument zeigt (Signal Avg).
        m_signalDbm = value;
        m_signalGemessen = true;
        aktualisiereAnzeigeSMeter();
        return;
    } else {
        return;
    }
    aktualisiereAnzeigeSwr();
}

// Bevor der erste Wert da ist, steht ein Strich -- keine Zahl, die wie
// eine Messung aussaehe (dieselbe Regel wie beim SWR ohne Leistung).
// Text und Balken kommen aus der einen Skala in ReadingSource: „S7",
// „S9+20", bis S9 gleichmaessig, darueber gestaucht, und ueber der
// S9-Grenze von 2 m um 20 dB versetzt.
void PanadapterApplet::aktualisiereAnzeigeSMeter()
{
    if (!m_anzeigeSWert) { return; }
    const ReadingDescriptor* skala = readingFor(MeterBinding::SignalAvg);
    const bool gemessen = m_signalGemessen && skala != nullptr;
    const QString farbe = QLatin1String(gemessen ? Style::kAmberText : Style::kAmberDim);
    m_anzeigeSWert->setText(gemessen ? skala->text(m_signalDbm)
                                     : QString::fromUtf8("\xe2\x80\x94"));
    m_anzeigeSWert->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background: transparent; }").arg(farbe));
    m_anzeigeSDbm->setText(gemessen
                               ? QStringLiteral("%1 dBm").arg(qRound(m_signalDbm))
                               : QString());
    m_anzeigeSBalken->setValue(gemessen ? qRound(skala->valueFraction(m_signalDbm) * 1000.0) : 0);
    m_anzeigeSBalken->setStyleSheet(QStringLiteral(
        "QProgressBar { background: %1; border: 1px solid %2; border-radius: 4px; }"
        "QProgressBar::chunk { background: %3; border-radius: 2px; }")
        .arg(QLatin1String(Style::kButtonBg), QLatin1String(Style::kBorder), farbe));
}

QString PanadapterApplet::sMeterReadoutTextForTest() const
{
    return m_anzeigeSWert ? m_anzeigeSWert->text() : QString();
}

int PanadapterApplet::sMeterReadoutBarForTest() const
{
    return m_anzeigeSBalken ? m_anzeigeSBalken->value() : -1;
}

// Ohne Leistung gibt es kein SWR: im Empfang steht ein Strich statt einer
// Zahl, die wie eine Messung aussaehe. Alle Radios melden im Empfang 0 W
// Vorlauf (RadioStatus::setForwardPower), die Flex ebenso -- darum haengt das
// an der Leistung und nicht an einer eigenen MOX-Verdrahtung.
void PanadapterApplet::aktualisiereAnzeigeSwr()
{
    if (!m_anzeigeSwrWert) { return; }
    const ReadingDescriptor* skala = readingFor(MeterBinding::TxSwr);
    const bool gemessen = m_vorlaufW > 0.05 && skala != nullptr;
    const bool warnung = gemessen && skala->threshold && m_swrWert >= *skala->threshold;
    const QString farbe = QLatin1String(!gemessen ? Style::kAmberDim
                                        : warnung ? Style::kAmberText
                                                  : Style::kGreenText);
    m_anzeigeSwrWert->setText(gemessen ? QString::number(m_swrWert, 'f', skala->decimals)
                                       : QString::fromUtf8("\xe2\x80\x94"));
    m_anzeigeSwrWert->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background: transparent; }").arg(farbe));
    m_anzeigeSwrBalken->setValue(gemessen ? qRound(skala->fraction(m_swrWert) * 1000.0) : 0);
    m_anzeigeSwrBalken->setStyleSheet(QStringLiteral(
        "QProgressBar { background: %1; border: 1px solid %2; border-radius: 4px; }"
        "QProgressBar::chunk { background: %3; border-radius: 2px; }")
        .arg(QLatin1String(Style::kButtonBg), QLatin1String(Style::kBorder), farbe));
    m_anzeigeSwrWarnung->setText(warnung ? QStringLiteral("!") : QString());
}

void PanadapterApplet::zeigeAnzeigeFrequenz(double hz)
{
    if (!m_anzeigeFrequenz) { return; }
    // MHz.kHz.Hz, wie auf dem Blatt: 7.113.065
    const qint64 h = qRound64(hz);
    m_anzeigeFrequenz->setText(QStringLiteral("%1.%2.%3")
        .arg(h / 1000000)
        .arg((h / 1000) % 1000, 3, 10, QLatin1Char('0'))
        .arg(h % 1000, 3, 10, QLatin1Char('0')));
}

QString PanadapterApplet::swrReadoutTextForTest() const
{
    return m_anzeigeSwrWert ? m_anzeigeSwrWert->text() : QString();
}

QString PanadapterApplet::frequencyReadoutTextForTest() const
{
    return m_anzeigeFrequenz ? m_anzeigeFrequenz->text() : QString();
}

void PanadapterApplet::updateStatusOverlay(SliceModel* slice, int chainIndex)
{
    if (!m_statusOverlay || !slice) { return; }

    // Die Anzeigezeile haengt an DIESER Scheibe (der des Panadapters),
    // nicht an der global aktiven; dieselbe Regel wie der Streifen hier.
    QObject::disconnect(m_anzeigeVerbindung);
    m_anzeigeVerbindung = connect(slice, &SliceModel::frequencyChanged, this,
                                  [this](double hz) { zeigeAnzeigeFrequenz(hz); });
    zeigeAnzeigeFrequenz(slice->frequency());

    // SliceModel::sliceLetter() now derives from the stable slice id, so the
    // local workaround this used to carry is gone. (It reported 'A' for every
    // slice while the letter was a stored member no one ever set.)
    m_statusOverlay->setSliceLetter(slice->sliceLetter());
    // SliceModel::frequency() returns double Hz (default 14225000.0 = 14.225 MHz).
    // Cast directly; no MHz->Hz conversion.
    m_statusOverlay->setFrequencyHz(static_cast<qint64>(slice->frequency()));
    m_statusOverlay->setMode(SliceModel::modeName(slice->dspMode()));
    // An unbound slice (chainIndex < 0) is on no chain at all. Holding the
    // last tag is honest about "unknown"; writing 0 would assert it is on
    // chain 0, which is the failure mode this parameter exists to stop.
    if (chainIndex >= 0) {
        m_statusOverlay->setChainIndex(chainIndex);
    }
    m_statusOverlay->setTxBound(slice->isTxSlice());
    m_statusOverlay->setDiversityActive(slice->diversityEnabled());
    m_statusOverlay->setPsPaused(slice->psPaused());
    // TX / DIV / PS pills all change the strip's width; re-anchor.
    repositionStatusOverlay();
}

QChar PanadapterApplet::statusSliceLetter() const
{
    return m_statusOverlay ? m_statusOverlay->sliceLetter() : QChar();
}

qint64 PanadapterApplet::statusFrequencyHz() const
{
    return m_statusOverlay ? m_statusOverlay->frequencyHz() : 0;
}

QString PanadapterApplet::statusMode() const
{
    return m_statusOverlay ? m_statusOverlay->mode() : QString();
}

int PanadapterApplet::statusChainIndex() const
{
    return m_statusOverlay ? m_statusOverlay->chainIndex() : 0;
}

// Phase 3F: WIDE pill forwarder. Kept separate from updateStatusOverlay
// because the two have different triggers and different sources: the slice
// fields refresh when the active slice changes, whereas the bypass state is
// a property of the chain feeding this pan and changes on band crossings,
// slice add/remove, wideband toggles and Filter Policy edits -- none of
// which need be the active slice, or any slice on this pan at all.
// Right-align the status strip, clear of the dBm scale strip.
//
// Must re-run whenever a pill lights or goes dark, not only on resize.
// SpectrumStatusOverlay::paintEvent grows minimumWidth() to fit the pills it
// just drew, and setGeometry clamps a narrower request up to that minimum --
// expanding the widget RIGHTWARD from its fixed x. So a strip positioned while
// only "CH 0" was showing crept back under the dBm range arrows the moment the
// TX or WIDE pill appeared. Re-anchoring from the current hint puts the right
// edge back where it belongs.
void PanadapterApplet::repositionStatusOverlay()
{
    if (!m_statusOverlay) { return; }
    const QSize hint = m_statusOverlay->sizeHint();
    // Clear of the dBm scale strip: its range up/down arrows sit at the top of
    // that column, and the status strip accepts mouse events for its own
    // badges, so any overlap makes the arrows both hard to read and hard to
    // hit.
    int reserved = m_spectrum ? m_spectrum->reservedRightEdgeWidth() : 0;

    // ── Und frei von den Knoepfen der Kopfleiste (2026-10-30) ───────
    //
    // Der Streifen liegt in derselben Zeile wie Zahnrad und
    // Schnellumschalter. Solange das Zahnrad 22 Punkt breit war, ging
    // es sich gerade aus; mit dem groeszeren Zahnrad und dem neuen
    // 2D/3D-Knopf schob sich die KIWI-Kachel darueber — am Geraet
    // gesehen: der Umschalter lugte nur noch oben heraus.
    //
    // Also wird auch deren Platz freigehalten. Der linke Rand des
    // ersten Knopfes ist die Grenze; was rechts davon liegt, gehoert
    // der Kopfleiste.
    if (m_btn3d && m_btn3d->isVisible()) {
        const int links = m_btn3d->mapTo(this, QPoint(0, 0)).x();
        reserved = std::max(reserved, std::max(0, width() - links + 6));
    }

    // Mit der Anzeigezeile bleibt der Streifen IM Kopf: bei y = 8 hinge er
    // sonst ueber die Zeile, genau in die SWR-Anzeige.
    const bool zeile = m_anzeigeZeile && !m_anzeigeZeile->isHidden();
    m_statusOverlay->setGeometry(width() - hint.width() - 8 - reserved, zeile ? 0 : 8,
                                 hint.width(), hint.height());
}

void PanadapterApplet::setWideBpf(bool wide, const QString& reason)
{
    if (!m_statusOverlay) { return; }
    m_statusOverlay->setWideBpf(wide, reason);
    // The WIDE pill changes the strip's width; re-anchor its right edge.
    repositionStatusOverlay();
}

bool PanadapterApplet::wideBpf() const
{
    return m_statusOverlay && m_statusOverlay->wideBpf();
}

QString PanadapterApplet::wideReason() const
{
    return m_statusOverlay ? m_statusOverlay->wideReason() : QString();
}

// Phase 3F Sub-Epic F Task 13: operator-toggleable Extended view.
// Default is true so the SpectrumWidget zoom auto-derive (Task 7-10)
// gets to decide extendedMode based on bandwidth vs DDC sample rate.
// This toggle is permission, not the actual state: at normal zoom the
// wideband stream stays off even when permission is enabled.
void PanadapterApplet::setExtendedViewEnabled(bool on)
{
    const bool changed = (m_extendedViewEnabled != on);
    m_extendedViewEnabled = on;
    if (changed) {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Pan_%1_ExtendedView").arg(m_panId),
                   on ? QStringLiteral("True") : QStringLiteral("False"));
    }
    if (m_spectrum) {
        m_spectrum->setExtendedViewAllowed(on);
    }
}

// Phase 3F Sub-Epic F Task 13: right-click context menu, originally a single
// checkable Extended view entry. Pops at the global cursor position.
void PanadapterApplet::contextMenuEvent(QContextMenuEvent* event)
{
    // Am Fenster, nicht am Applet (2026-09-30): das Applet ist nativ (es
    // traegt den Panadapter), und ein Menue daran bekam von Qt keinen
    // Fensterbezug („... must be a top level window.").
    m_menuSpektrumPos = QPoint(-1, -1);
    if (SpectrumWidget* sw = spectrumWidget()) {
        const QPoint p = sw->mapFrom(this, event->pos());
        if (sw->rect().contains(p)) { m_menuSpektrumPos = p; }
    }
    QMenu* menu = buildContextMenu(window());
    // Das Fenster kann waehrend exec() sterben; dann hat Qt das Menue als
    // sein Kind schon geholt und der Zeiger hier ist tot.
    QPointer<QMenu> stillAlive(menu);
    menu->exec(event->globalPos());
    if (stillAlive) {
        stillAlive->deleteLater();
    }
}

// Task B5 (bottom-banner + pan-menu epic, design doc s8.5): "Add slice on
// this pan" / "Float this pan" used to live on the +PAN button's dropdown
// (Phase 3F Sub-Epic D Task 10, retired in Task B4) and resolved through
// m_panStack->activePanId() -- "active pan" meaning whichever pan happened
// to be active when the operator reached a button sitting nowhere near any
// pan. A control drawn ON a pan targets THAT pan, so both move here and
// carry this applet's own panId(). Neither action exists on AetherSDR's pan
// menu (design doc s8.5), so this is Longpath-original, not a further port.
//
// Split out of contextMenuEvent so a test can build the menu, find +
// trigger() these two actions by their operator-facing text, and assert the
// resulting signal without exec()-ing a real nested event loop -- mirrors
// AmpApplet::buildContextMenu() / buildContextMenuForTesting().
QMenu* PanadapterApplet::buildContextMenu(QObject* parent)
{
    auto* menu = new QMenu(qobject_cast<QWidget*>(parent));

    menu->addAction(tr("Add slice on this pan"), this, [this]() {
        emit addSliceRequested(panId());
    });
    QAction* floatAct = menu->addAction(tr("Float this pan"));
    floatAct->setToolTip(floatLockReason());
    connect(floatAct, &QAction::triggered, this, [this]() {
        emit floatRequested(panId());
    });
    menu->addSeparator();

    // ── Einmessen an einem Traeger (2026-11-08) ─────────────────────
    //
    // Auf 2 m stellt es den LO-Fehler des Transverters, auf KW an einem
    // OpenHPSDR-Geraet den Korrekturfaktor (Thetis Freq Cal) -- an der
    // Stelle des Rechtsklicks, nicht an einer festen Eichfrequenz. Das
    // Ergebnis kommt ueber SpectrumWidget::traegerGemessen ins
    // Hauptfenster, das den Dialog zeigt.
    if (spectrumWidget()) {
        QAction* eich = menu->addAction(tr("Calibrate frequency on this carrier…"));
        eich->setToolTip(tr("Measures the strongest carrier within ±2.5 kHz of "
                            "this point (ten spectra averaged) and corrects the "
                            "transverter LO error or the radio's reference."));
        const QPoint p = m_menuSpektrumPos;
        eich->setEnabled(p.x() >= 0);
        connect(eich, &QAction::triggered, this, [this, p]() {
            if (SpectrumWidget* w = spectrumWidget()) { w->starteTraegerMessungBei(p); }
        });
        menu->addSeparator();
    }

    // ── Anzeigequelle: Geraet oder KiwiSDR (Stufe 6, 2026-08-23) ────
    //
    // Der Haken steht hier und nicht im KiwiSDR-Applet, weil er eine
    // Eigenschaft DIESES Panadapters ist: bei mehreren Panadaptern
    // darf jeder eine andere Quelle zeigen. Ein Schalter im
    // Kiwi-Applet muesste erst fragen, welchen er meint.
    //
    // Er ist bewusst nicht ausgegraut, wenn kein Kiwi verbunden ist:
    // wer ihn umlegt und ein leeres Bild bekommt, hat eine Antwort
    // ("da kommt nichts"); wer einen ausgegrauten Punkt sieht, hat
    // keine.
    if (SpectrumWidget* sw = spectrumWidget()) {
        QAction* kiwiAct = menu->addAction(tr("Display from the KiwiSDR"));
        kiwiAct->setCheckable(true);
        kiwiAct->setChecked(sw->kiwiDisplaySource());
        kiwiAct->setToolTip(
            tr("Shows the waterfall of the KiwiSDR assigned to this slice "
                "instead of the radio's."));
        connect(kiwiAct, &QAction::triggered, this, [this](bool on) {
            if (SpectrumWidget* w = spectrumWidget()) {
                // Kein Speichern hier: das erledigt SpectrumWidget
                // selbst ueber seinen Schluessel. Stuende es an diesem
                // Bedienweg, vergaesse es die KIWI-Kachel im
                // Frequenz-Widget — der Fehler, den
                // tst_settings_are_remembered am 2026-08-23 gefunden
                // hat.
                w->setKiwiDisplaySource(on);
                if (m_statusOverlay) { m_statusOverlay->setKiwiActive(on); }
            }
        });
        menu->addSeparator();
    }

    QAction* extAct = menu->addAction(tr("Extended view (wideband wings)"));
    extAct->setCheckable(true);
    extAct->setChecked(m_extendedViewEnabled);
    connect(extAct, &QAction::toggled, this, &PanadapterApplet::setExtendedViewEnabled);

    return menu;
}

// Das Menue hinter dem Zahnrad. Ausgelagert, damit eine Pruefung es
// bauen und seine Eintraege ueber ihren Text ausloesen kann, ohne eine
// verschachtelte Ereignisschleife zu starten — dasselbe Muster wie
// buildContextMenu().
QMenu* PanadapterApplet::buildDisplayMenu(QObject* parent)
{
    auto* menu = new QMenu(qobject_cast<QWidget*>(parent));

    // Die Anzeigezeile, zuoberst: das ist, was man hier am ehesten sucht.
    QAction* zeileFrequenz = menu->addAction(tr("Frequency readout"));
    zeileFrequenz->setCheckable(true);
    zeileFrequenz->setChecked(m_zeigeFrequenz);
    connect(zeileFrequenz, &QAction::toggled, this,
            [this](bool an) { setFrequencyReadoutShown(an); });
    QAction* zeileSwr = menu->addAction(tr("SWR readout"));
    zeileSwr->setCheckable(true);
    zeileSwr->setChecked(m_zeigeSwr);
    connect(zeileSwr, &QAction::toggled, this,
            [this](bool an) { setSwrReadoutShown(an); });
    QAction* zeileSMeter = menu->addAction(tr("S-meter readout"));
    zeileSMeter->setCheckable(true);
    zeileSMeter->setChecked(m_zeigeSMeter);
    connect(zeileSMeter, &QAction::toggled, this,
            [this](bool an) { setSMeterReadoutShown(an); });
    menu->addSeparator();

    // Durchlass und Bandbreite (2026-11-08): die vier Entwuerfe, die der
    // Betreiber gewaehlt hat. Alle an ab Werk; hier je einzeln aus.
    if (m_spectrum) {
        QAction* breite = menu->addAction(tr("Bandwidth label"));
        breite->setCheckable(true);
        breite->setChecked(m_spectrum->passbandLabelShown());
        connect(breite, &QAction::toggled, m_spectrum,
                &SpectrumWidget::setPassbandLabelShown);
        QAction* klammern = menu->addAction(tr("Signal brackets (width, SNR)"));
        klammern->setCheckable(true);
        klammern->setChecked(m_spectrum->signalBracketsShown());
        connect(klammern, &QAction::toggled, m_spectrum,
                &SpectrumWidget::setSignalBracketsShown);
        QAction* luecken = menu->addAction(tr("Gap finder (CQ)"));
        luecken->setCheckable(true);
        luecken->setChecked(m_spectrum->gapFinderShown());
        connect(luecken, &QAction::toggled, m_spectrum,
                &SpectrumWidget::setGapFinderShown);
        QAction* sStufen = menu->addAction(tr("S-units on the scale"));
        sStufen->setCheckable(true);
        sStufen->setChecked(m_spectrum->sUnitsScaleShown());
        sStufen->setToolTip(tr("S1…S9, +10…+60 instead of dBm. Below 30 MHz "
                               "S9 = −73 dBm, above (2 m) −93 dBm, judged at "
                               "the antenna frequency."));
        connect(sStufen, &QAction::toggled, m_spectrum,
                &SpectrumWidget::setSUnitsScaleShown);
        QAction* lupe = menu->addAction(tr("Passband magnifier"));
        lupe->setCheckable(true);
        lupe->setChecked(m_spectrum->passbandMagnifier() != 0);
        lupe->setToolTip(tr("Shows ±10 kHz around the VFO with the passband, "
                            "at every zoom level."));
        connect(lupe, &QAction::toggled, m_spectrum,
                [this](bool an) { m_spectrum->setPassbandMagnifier(an ? 1 : 0); });
        menu->addSeparator();
    }

    // ── Spots je Quelle (2026-11-18) ─────────────────────────────────
    //
    // Betreiber: „rufzeichen aus dem cluster sollte man als option auch
    // ein und ausblenden können". Den Schalter gab es — aber nur im
    // Spot Hub, Reiter Display. Hier derselbe Schluessel
    // (SpotSourceVisible/<Quelle>) und derselbe Weg: schreiben, die
    // Maske aus den Einstellungen nachladen, und spotSourcesChanged()
    // sagt MainWindow, dass der Spot Hub (Haken + Spot-Liste) nachzieht.
    // Kein zweiter Speicher, keine zweite Maske.
    addSpotSourceMenu(menu);
    menu->addSeparator();

    menu->addAction(tr("Choose background image…"), this, [this]() {
        // ── Der Dialog gehoert dem FENSTER, nicht dem Applet ─────
        //
        // Mit `this` als Elternteil haengt er an einem Widget, das in
        // einem rahmenlosen Qt::Tool-Fenster stecken kann. Ein modaler
        // Dialog geht dann leicht dahinter auf, und der Betreiber sieht
        // nichts passieren — was er am 2026-08-20 als
        // „hintergrundbild funktioniert auch nicht" gemeldet hat.
        //
        // Nachgemessen ist der Rest der Kette in Ordnung: ein direkt
        // gesetztes Bild landet auf der Flaeche (66854 Magenta-Punkte
        // im GPU-Bild). Es fehlte also nicht das Malen, sondern der Weg
        // zum Dateinamen.
        QWidget* dlgParent = window() ? window() : this;
        const QString f = QFileDialog::getOpenFileName(
            dlgParent, tr("Hintergrundbild"), QString(),
            tr("Bilder (*.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff)"));
        if (f.isEmpty()) { return; }

        // Ladbar? Sonst sagt es das, statt still nichts zu tun.
        // SpectrumWidget schreibt zwar eine Warnung ins Protokoll, aber
        // ein Protokoll liest niemand, der gerade ein Foto ausgesucht
        // hat.
        if (QImage(f).isNull()) {
            QMessageBox::warning(
                dlgParent, tr("Hintergrundbild"),
                tr("This file cannot be read as an image:\n%1")
                    .arg(f));
            return;
        }
        emit backgroundImageRequested(f);
        // Ein Bild waehlen und nichts sehen ist kein Ergebnis.
        //
        // Die Vorgabe fuer „Fuellfarbe sichtbar" ist 80 — davon bleiben
        // vom Bild 20 %, auf einem dunklen Panadapter praktisch nichts.
        // Wer gerade ein Foto ausgesucht hat, will es sehen; steht die
        // Fuellfarbe noch auf einem Wert, der das Bild verschluckt,
        // wird sie einmalig auf die Haelfte gesetzt. Wer danach anders
        // will, findet den Wert im selben Menue.
        if (m_bgOpacityPct >= 70) {
            m_bgOpacityPct = 50;
            emit backgroundOpacityRequested(m_bgOpacityPct);
        }
    });
    menu->addAction(tr("Remove background image"), this, [this]() {
        emit backgroundImageRequested(QString());
    });

    // ── „Bild sichtbar", nicht „Deckkraft" ───────────────────────────
    //
    // Der Wert, den SpectrumWidget haelt, ist die Sichtbarkeit der
    // FUELLFARBE: 100 = nur Fuellfarbe, 0 = nur Bild (so steht es auch
    // am Regler unter Setup -> Display). Hier stand „Deckkraft" mit
    // demselben Zahlenwert — also genau verkehrt herum.
    //
    // Der Betreiber, 2026-08-20: „fotos sehe ich nicht im hintergrund
    // beim pandapter." Bei der Vorgabe 80 blieben vom Bild 20 %, und
    // wer im Menue 100 waehlte, um es kraeftiger zu machen, bekam gar
    // keins mehr.
    //
    // Das Menue spricht jetzt vom BILD und rechnet selbst um.
    auto* opa = menu->addMenu(tr("Image opacity"));
    for (int pct : {0, 25, 50, 75, 100}) {
        QAction* a = opa->addAction(QStringLiteral("%1 %").arg(pct));
        a->setCheckable(true);
        a->setChecked(pct == 100 - m_bgOpacityPct);
        connect(a, &QAction::triggered, this, [this, pct]() {
            m_bgOpacityPct = 100 - pct;          // gespeichert wird die Fuellfarbe
            emit backgroundOpacityRequested(m_bgOpacityPct);
        });
    }

    menu->addAction(tr("Grundfarbe…"), this, [this]() {
        emit backgroundColourRequested();
    });

    // Der Weg zurueck. Es gab ihn nicht: eine einmal gewaehlte
    // Grundfarbe stand in den Einstellungen und gewann von da an gegen
    // den eingebauten Wert UND gegen jedes Thema. Beim Betreiber war
    // das deckendes Schwarz, und der blaugraue Standard kam nie mehr
    // durch (2026-08-21: „bitte setze ihn als standard").
    QAction* reset = menu->addAction(tr("Reset base colour"));
    reset->setToolTip(tr(
        "Forgets the base colour you picked. The panadapter then "
         "follows the theme again — blue-grey in the dark theme."));
    connect(reset, &QAction::triggered, this, [this]() {
        emit backgroundColourResetRequested();
    });

    menu->addSeparator();

    menu->addAction(tr("All display settings…"), this, [this]() {
        emit displaySetupRequested();
    });
    return menu;
}

// Die Quellen in der Reihenfolge und mit den Namen des Spot Hub
// (SpotHubDialog::buildDisplayTab). tst_pan_spot_sources vergleicht
// beide Listen, damit eine neue Quelle nicht nur an einer Stelle steht.
namespace {
struct SpotQuelle { const char* quelle; const char* name; };
constexpr SpotQuelle kSpotQuellen[] = {
    {"Cluster",       "DX Cluster"},
    {"RBN",           "RBN"},
    {"WSJT-X",        "WSJT-X"},
    {"SpotCollector", "SpotCollector"},
    {"POTA",          "POTA"},
    {"FreeDV",        "FreeDV"},
    {"PSK",           "PSK Reporter"},
    {"Roundtable",    "Roundtable"},
    // Die eigenen Dekoder (Ft8Applet, RttyDecoderApplet) stempeln
    // "Longpath". Ohne diesen Haken blieben die FT8-Rufzeichen im Bild,
    // was immer man sonst abschaltet (2026-11-19).
    {"Longpath",      "Own decoder (FT8, RTTY)"},
};
QString spotQuellenSchluessel(const char* quelle)
{
    return QStringLiteral("SpotSourceVisible/") + QLatin1String(quelle);
}
} // namespace

void PanadapterApplet::setSpotSourcesVisible(const QStringList& quellen, bool an)
{
    // Wie `save` im Spot Hub: setzen, sichern — dann EIN Nachladen.
    auto& s = AppSettings::instance();
    for (const QString& q : quellen) {
        s.setValue(QStringLiteral("SpotSourceVisible/") + q,
                   an ? QStringLiteral("True") : QStringLiteral("False"));
    }
    s.save();
    if (m_spectrum) { m_spectrum->loadSpotDisplaySettings(); }
    emit spotSourcesChanged();
}

void PanadapterApplet::addSpotSourceMenu(QMenu* menu)
{
    auto* spots = menu->addMenu(tr("Spots"));
    spots->setObjectName(QStringLiteral("panSpotSourcesMenu"));
    const auto& s = AppSettings::instance();
    QStringList alle;
    for (const SpotQuelle& q : kSpotQuellen) {
        const QString quelle = QLatin1String(q.quelle);
        alle << quelle;
        QAction* a = spots->addAction(QLatin1String(q.name));
        a->setObjectName(QStringLiteral("panSpotSource_") + quelle);
        a->setCheckable(true);
        a->setChecked(s.value(spotQuellenSchluessel(q.quelle),
                              QStringLiteral("True")).toString()
                      == QStringLiteral("True"));
        connect(a, &QAction::toggled, this, [this, quelle](bool an) {
            setSpotSourcesVisible({quelle}, an);
        });
    }
    spots->addSeparator();
    spots->addAction(tr("Show all"), this, [this, alle]() {
        setSpotSourcesVisible(alle, true);
    });
    spots->addAction(tr("Hide all"), this, [this, alle]() {
        setSpotSourcesVisible(alle, false);
    });
}

// ↗ heisst „ablösen", ↙ heisst „zurueck in die Anordnung".
// ── Die Abloese-Sperre ist Geschichte (2026-08-21 bis 2026-08-22) ────
//
// Einen Tag lang war das Abloesen gesperrt: es stuerzte beim Beenden
// ab (SIGSEGV in QRhiWidgetPrivate::ensureRhi, upstream #2495). Die
// Ursache waren VIER bei der Portierung verlorene Schutzmethoden aus
// AetherSDR (prepareForTopLevelChange / resetGpuResources /
// prepareForShutdown / applyNativeWindowIsolationPolicy). Seit dem
// Rueckport ueberlebt der frühere Absturz-Repro (tst_real_pan_float_
// state beendet mit abgeloestem Panadapter) — die Sperre fiel.
//
// floatLockReason() bleibt als Huelle stehen, weil Pruefungen und
// aeltere Anordnungen den Namen kennen; sie beschreibt jetzt den
// Normalzustand.
QString PanadapterApplet::floatLockReason()
{
    return tr("Detach into its own window — freely movable and resizable. The "
               "arrow in that window brings it back.");
}

void PanadapterApplet::setFloatingIndicator(bool floating)
{
    m_floating = floating;
    if (!m_btnFloat) { return; }
    m_btnFloat->setText(floating ? QStringLiteral("\u2199")
                                 : QStringLiteral("\u2197"));

    m_btnFloat->setEnabled(true);
    m_btnFloat->setToolTip(floating
        ? QStringLiteral("Back into the layout")
        : QStringLiteral("Detach into its own window"));
}

} // namespace Longpath
