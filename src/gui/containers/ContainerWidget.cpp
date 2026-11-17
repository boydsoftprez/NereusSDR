// =================================================================
// src/gui/containers/ContainerWidget.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/ucMeter.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (Longpath):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-11-17 — Greifpunkt und Titel: Bewegen-Zeiger ohne Farbprofil
//                 (gui/ZeigerOhneFarbraum.h; Qt 6.11.1 stuerzte beim Bau
//                 des eigenen ab). Martin Fischer, KI-gestuetzt ueber
//                 Anthropic Claude.
// =================================================================

/*  ucMeter.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2025 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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

#include "ContainerWidget.h"
#include "gui/WindowPlacement.h"

#include <cmath>
#include "gui/ScopedChildWidget.h"

#include "gui/StyleConstants.h"
#include "gui/ZeigerOhneFarbraum.h"
#include "gui/styles/PopupMenuStyle.h"
#include "gui/styles/ThemeQss.h"
#include "FloatingContainer.h"
#include "core/BoardCapabilities.h"
#include "core/LogCategories.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/meters/FilterButtonItem.h"
#include "gui/meters/AntennaButtonItem.h"
#include "gui/meters/TuneStepButtonItem.h"
#include "gui/meters/OtherButtonItem.h"
#include "gui/meters/VoiceRecordPlayItem.h"
#include "gui/meters/DiscordButtonItem.h"
#include "gui/meters/VfoDisplayItem.h"

#include <QUuid>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <QTabBar>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QApplication>
#include <QMap>
#include <QStringList>
#include <algorithm>

namespace Longpath {

ContainerWidget::ContainerWidget(QWidget* parent)
    : QWidget(parent)
{
    m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    setMinimumSize(kMinContainerWidth, kMinContainerHeight);
    setMouseTracking(true);
    buildUI();
    updateLockButton();
    updateTitleBar();
    updateTitle();
    setupBorder();
    storeLocation();

    // Default placeholder content — replaced by setContent() in later phases
    auto* placeholder = new QLabel(QStringLiteral("Container"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet(Style::themed(QStringLiteral(
        "color: #405060; font-size: 16px; background: transparent;")));
    setContent(placeholder);

    qCDebug(lcContainer) << "Container created:" << m_id;
}

ContainerWidget::~ContainerWidget()
{
    qCDebug(lcContainer) << "Container destroyed:" << m_id;
}

void ContainerWidget::buildUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Ein Kopf fuer alle Fenster ───────────────────────────────────
    //
    // Entwurf 1 vom 2026-08-19, vom Betreiber freigegeben: Greifpunkt
    // links, Name in Versalien, rechts die Schalter — Schloss, Ablösen,
    // Einstellungen. Wer ein Fenster bewegen will, soll nie zweimal
    // suchen muessen.
    //
    // Die Farben kommen aus Style:: statt als Hexwerte im Quelltext zu
    // stehen. themed() hat die alten Werte zwar richtig uebersetzt, aber
    // nur, weil jeder einzelne in der Theme-Tabelle steht; ein neu
    // hingeschriebener Hexwert faellt still durch. Die Konstante kann
    // das nicht.
    //
    // Hoehe aus kTitleBarHeight (22): genug zum Greifen, wenig genug,
    // dass zehn Fenster nicht nur aus Koepfen bestehen.
    m_titleBar = new QWidget(this);
    m_titleBar->setFixedHeight(kTitleBarHeight);
    // ── Angedockt bleibt die Leiste STEHEN ──────────────────────────
    //
    // Zwei Befunde des Betreibers am 2026-08-23, und beide gehen auf
    // dieselbe Sache zurueck:
    //
    //   "das öffnen und zittern im rx1 panel"  — die Leiste lag in der
    //   Anordnung und schob beim Aufklappen alles nach unten.
    //   "das anklicken für das ablösen ist schwierig, das zeichen
    //   verschwindet sobald man auf die leiste klickt" — nachdem ich
    //   sie zur Ueberlagerung gemacht hatte, DECKTE sie den
    //   Trennen-Pfeil des Applets zu.
    //
    // Ich hatte also ein Problem gegen ein anderes getauscht. Die
    // Vorlage loest es anders und einfacher: bei OpenHPSDR ist die
    // Titelleiste eines angedockten Feldes IMMER da ("⠿ BANDWIDTH
    // FILTER ... 🔒 ✕"). Dann schiebt nichts, dann deckt nichts, und
    // der Griff zum Verschieben ist jederzeit greifbar.
    //
    // Der Preis sind 22 Punkte Hoehe, dauerhaft. Das ist der richtige
    // Preis: ein Bedienelement, das man nur durch Herumfahren findet,
    // ist keines.
    //
    // Geloest (Overlay/Floating) bleibt es beim bisherigen Verhalten —
    // dort ist die Leiste ohnehin der einzige Griff.
    m_titleBar->setVisible(isPanelDocked());
    // Derselbe Verlauf wie jede andere Kopfleiste (WindowChrome,
    // GridCellWidget). Hier stand eine flache Flaeche aus
    // kTitleGradBot — der unterste Ton DESSELBEN Verlaufs, also
    // dieselbe Familie, aber ohne den Verlauf. Nebeneinander sah eine
    // Leiste plastisch aus und die andere flach.
    m_titleBar->setStyleSheet(QStringLiteral("%1")
                                  .arg(Style::titleBarStyle()));

    auto* barLayout = new QHBoxLayout(m_titleBar);
    barLayout->setContentsMargins(5, 0, 0, 0);
    barLayout->setSpacing(3);

    // Greifpunkt. Er bewegt nichts, was die Titelleiste nicht auch
    // bewegt — er SAGT nur, dass sich hier etwas ziehen laesst. Genau
    // das fehlte: das Ziehen gab es, den Hinweis darauf nicht.
    //
    // Als GELBER STRICH, nicht als sechs graue Punkte: der Betreiber am
    // 2026-08-19 zum ersten Entwurf — „die 6 punkte vor dem titel
    // koennten farbe benoetigen oder wie bei [der Vorlage] der gelbe strich".
    // Die Vorlage setzt denselben Strich, und ein Grauton auf grauem
    // Grund war ohnehin genau die Zurueckhaltung, die den Hinweis
    // unsichtbar gemacht hat.
    //
    // Ein QLabel ohne Text, das nur seinen Grund zeigt — ein Zeichen
    // waere in jeder Schrift anders breit, ein Grund ist ueberall
    // gleich.
    m_grip = new QLabel(m_titleBar);
    m_grip->setFixedWidth(3);
    m_grip->setStyleSheet(QStringLiteral("background: %1;")
                              .arg(QLatin1String(Style::kAmberText)));
    m_grip->setCursor(bewegenZeiger());
    m_grip->setToolTip(QStringLiteral("Drag to move this container"));
    barLayout->addWidget(m_grip);

    // Title label — "RX1" + notes (Thetis ucMeter.cs:625-639)
    m_titleLabel = new QLabel(QStringLiteral("RX1"), m_titleBar);
    m_titleLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 11px; font-weight: bold; background: transparent;"
        " letter-spacing: 1px;").arg(QLatin1String(Style::kTextPrimary)));
    m_titleLabel->setCursor(bewegenZeiger());
    barLayout->addWidget(m_titleLabel, 1);

    const QString btnStyle = QStringLiteral(
        "QPushButton { background: transparent; border: none; color: %1;"
        "  font-size: 11px; padding: 2px 4px; }"
        "QPushButton:hover { background: %2; color: %3; }")
        .arg(QLatin1String(Style::kTextSecondary),
             QLatin1String(Style::kButtonHover),
             QLatin1String(Style::kTextPrimary));

    // Axis lock button (overlay-docked only)
    m_btnAxis = new QPushButton(QStringLiteral("\u2196"), m_titleBar);
    m_btnAxis->setFixedSize(22, 22);
    m_btnAxis->setToolTip(QStringLiteral("Axis lock (click to cycle, right-click reverse)"));
    m_btnAxis->setStyleSheet(btnStyle);
    barLayout->addWidget(m_btnAxis);

    // Pin-on-top button (floating only)
    m_btnPin = new QPushButton(QStringLiteral("\U0001F4CC"), m_titleBar);
    m_btnPin->setFixedSize(22, 22);
    m_btnPin->setToolTip(QStringLiteral("Pin on top"));
    m_btnPin->setStyleSheet(btnStyle);
    m_btnPin->setVisible(false);
    barLayout->addWidget(m_btnPin);

    // Schloss. Es stand bisher nur im Rechtsklick-Menue — ein
    // festgestelltes Fenster sah aus wie ein loses, und man ruettelt
    // daran, bis man merkt, warum nichts geht. Sichtbarer Zustand ist
    // der halbe Zweck eines Schlosses.
    m_btnLock = new QPushButton(m_titleBar);
    m_btnLock->setFixedSize(22, 22);
    m_btnLock->setStyleSheet(btnStyle);
    barLayout->addWidget(m_btnLock);

    // Float/Dock toggle
    m_btnFloat = new QPushButton(QStringLiteral("\u2197"), m_titleBar);
    m_btnFloat->setFixedSize(22, 22);
    m_btnFloat->setToolTip(QStringLiteral("Float / Dock"));
    m_btnFloat->setStyleSheet(btnStyle);
    barLayout->addWidget(m_btnFloat);

    // Settings gear
    m_btnSettings = new QPushButton(QStringLiteral("\u2699"), m_titleBar);
    m_btnSettings->setFixedSize(22, 22);
    m_btnSettings->setToolTip(QStringLiteral("Container settings"));
    m_btnSettings->setStyleSheet(btnStyle);
    barLayout->addWidget(m_btnSettings);

    // ── Und ein x (2026-11-01) ──────────────────────────────────────
    //
    // Betreiber, zweimal: "wieso kann ich das RX 1 panel nicht
    // schließen" und "kann das panel wieder nicht schließen". Zu
    // Recht -- diese Leiste hatte Pin, Schloss, Andocken und Zahnrad,
    // aber kein x. Jedes andere Panel im Programm laeszt sich
    // schliessen; dieses nicht, und niemand sucht den Weg dazu im
    // Menue.
    //
    // Zurueck kommt es ueber Containers im Menue -- dasselbe, was
    // auch fuer ein geschlossenes Applet gilt.
    m_btnClose = new QPushButton(QStringLiteral("\u2715"), m_titleBar);
    m_btnClose->setFixedSize(22, 22);
    m_btnClose->setToolTip(QStringLiteral("Hide this container "
                                          "(Containers menu brings it back)"));
    m_btnClose->setStyleSheet(btnStyle);
    barLayout->addWidget(m_btnClose);
    connect(m_btnClose, &QPushButton::clicked, this, [this]() {
        if (QWidget* fenster = window()) {
            if (fenster != this && fenster->isWindow()
                && !qobject_cast<QMainWindow*>(fenster)) {
                fenster->hide();
                return;
            }
        }
        hide();
    });

    // ── Die Titelleiste liegt UEBER dem Inhalt, nicht darin ─────────
    //
    // Hier stand mainLayout->addWidget(m_titleBar). Sie erscheint aber
    // beim Ueberfahren (mouseMoveEvent, Thetis-Vorbild) — und wer in
    // einer Anordnung liegt, schiebt beim Erscheinen alles darunter
    // nach unten.
    //
    // Der Betreiber am 2026-08-23: "das öffnen und zittern im rx1
    // panel ist noch nicht behoben ... ist nur beim ersten mal trennen
    // des filter." Genau so: der Zeiger faehrt zum Trennen-Pfeil, der
    // ganz oben sitzt, die Leiste klappt auf, und die ganze Spalte
    // rutscht um ihre Hoehe nach unten. Beim ERSTEN Mal faellt es auf,
    // danach ist die Leiste schon da.
    //
    // Nachgestellt am laufenden Programm: vor dem Klick keine Leiste,
    // danach "RX1 Main Panel" und alles zwoelf Punkte tiefer.
    //
    // Angedockt: in der Anordnung (steht immer, verdeckt nichts).
    // Geloest: Ueberlagerung (klappt auf, nimmt keinen Platz).
    if (isPanelDocked()) {
        mainLayout->addWidget(m_titleBar);
    } else {
        m_titleBar->raise();
    }

    // Content holder — layout slot for setContent()
    m_contentHolder = new QWidget(this);
    m_contentHolder->setMouseTracking(true);
    m_contentHolder->setStyleSheet(QStringLiteral("background: %1;")
                                       .arg(QLatin1String(Style::kAppBg)));
    new QVBoxLayout(m_contentHolder);
    m_contentHolder->layout()->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_contentHolder, 1);

    // Resize grip (bottom-right, hidden until hover)
    m_resizeGrip = new QWidget(this);
    m_resizeGrip->setFixedSize(12, 12);
    m_resizeGrip->setCursor(Qt::SizeFDiagCursor);
    m_resizeGrip->setStyleSheet(QStringLiteral(
        "background: %1; border-radius: 6px;")
        .arg(QLatin1String(Style::kTextInactive)));
    m_resizeGrip->setVisible(false);

    // Wire button signals
    connect(m_btnFloat, &QPushButton::clicked, this, [this]() {
        if (isFloating()) {
            emit dockRequested();
        } else {
            emit floatRequested();
        }
    });

    connect(m_btnAxis, &QPushButton::clicked, this, [this]() {
        cycleAxisLock(QApplication::keyboardModifiers() & Qt::ShiftModifier);
    });

    connect(m_btnPin, &QPushButton::clicked, this, [this]() {
        setPinOnTop(!m_pinOnTop);
    });

    connect(m_btnSettings, &QPushButton::clicked, this, [this]() {
        emit settingsRequested();
    });

    connect(m_btnLock, &QPushButton::clicked, this, [this]() {
        setLocked(!m_locked);
    });

    // Event filters for title bar drag + resize grip + hover detection
    m_titleBar->installEventFilter(this);
    m_titleLabel->installEventFilter(this);
    m_grip->installEventFilter(this);
    m_resizeGrip->installEventFilter(this);
    m_contentHolder->installEventFilter(this);
}

void ContainerWidget::setContent(QWidget* widget)
{
    QLayout* layout = m_contentHolder->layout();
    if (m_content) {
        layout->removeWidget(m_content);
        // Detach SYNCHRONOUSLY before deleteLater. When the content is a
        // MeterWidget (WA_NativeWindow QRhiWidget), leaving it under this
        // container's HWND through an upcoming reparent blocks the fresh
        // MeterWidget's D3D11 swapchain with E_ACCESSDENIED on Windows.
        m_content->hide();
        m_content->setParent(nullptr);
        m_content->deleteLater();
    }
    m_content = widget;
    if (m_content) {
        m_content->setParent(m_contentHolder);
        m_content->setMouseTracking(true);
        m_content->installEventFilter(this);
        // Enable mouse tracking + event filter on all descendants so hover
        // detection works even when mouse is over child widgets (scroll areas, applets).
        for (QWidget* child : m_content->findChildren<QWidget*>()) {
            child->setMouseTracking(true);
            child->installEventFilter(this);
        }
        layout->addWidget(m_content);
    }
    refreshMinimumWidth();
    emit contentChanged(m_content);
}

void ContainerWidget::refreshMinimumWidth()
{
    int w = kMinContainerWidth;
    if (m_content) {
        w = qMax(w, m_content->minimumSizeHint().width());
    }
    if (w != minimumWidth()) {
        setMinimumWidth(w);
    }
}

bool ContainerWidget::event(QEvent* event)
{
    if (event->type() == QEvent::LayoutRequest) {
        refreshMinimumWidth();
    }
    return QWidget::event(event);
}

// ── Reiter ───────────────────────────────────────────────────────────
//
// Begruendung steht am Kopf der Deklaration. Der Stapel wird ERST beim
// zweiten Inhalt gebaut: eine Reiterleiste ueber einem einzigen Reiter
// ist eine Zeile Hoehe fuer nichts.

void ContainerWidget::addTab(QWidget* widget, const QString& title)
{
    if (!widget || !m_contentHolder) { return; }
    auto* holderLayout = qobject_cast<QVBoxLayout*>(m_contentHolder->layout());
    if (!holderLayout) { return; }

    if (!m_stack) {
        m_stack = new QStackedWidget(m_contentHolder);

        m_tabBar = new QTabBar(m_contentHolder);
        m_tabBar->setDrawBase(false);
        m_tabBar->setExpanding(false);
        m_tabBar->setTabsClosable(false);
        m_tabBar->setStyleSheet(QStringLiteral(
            "QTabBar { background: %1; }"
            "QTabBar::tab { background: transparent; color: %2;"
            "  padding: 2px 10px; font-size: 11px; font-weight: bold;"
            "  border: none; border-bottom: 2px solid transparent; }"
            "QTabBar::tab:selected { color: %3; border-bottom-color: %4; }"
            "QTabBar::tab:hover { color: %3; }")
            .arg(QLatin1String(Style::kTitleGradBot),
                 QLatin1String(Style::kTextScale),
                 QLatin1String(Style::kTextPrimary),
                 QLatin1String(Style::kAmberText)));
        m_tabBar->hide();   // erscheint erst ab zwei

        connect(m_tabBar, &QTabBar::currentChanged, this, [this](int i) {
            if (m_stack) { m_stack->setCurrentIndex(i); }
        });

        // Ein Reiter kann wieder eigenstaendig werden. Ueber das
        // Kontextmenue der Leiste, nicht ueber ein ✕ am Reiter: ein ✕
        // dort liest sich als „schliessen", und geschlossen wird hier
        // nichts.
        m_tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_tabBar, &QTabBar::customContextMenuRequested, this,
                [this](const QPoint& p) {
            const int i = m_tabBar->tabAt(p);
            if (i < 0) { return; }
            ScopedChildWidget<QMenu> menuOwner(window());   // stirbt nicht mit dem Rahmen, siehe ScopedChildWidget.h
            const QPointer<ContainerWidget> self(this);   // Menue am Fenster: ein nativer Vorfahr gab ihm keinen Fensterbezug (2026-09-30)
            QMenu& menu = *menuOwner.get();
            menu.setStyleSheet(QString::fromLatin1(kPopupMenu));
            QAction* out = menu.addAction(
                tr("%1 as its own window")
                    .arg(m_tabBar->tabText(i)));
            const QAction* chosen = menu.exec(m_tabBar->mapToGlobal(p));
            // Das Elternteil kann waehrend exec() gestorben sein — dann ist
            // auch das Menue weg und `this` eine Leiche. Siehe ScopedChildWidget.h.
            if (!menuOwner || !self) { return; }
            if (chosen == out) {
                emit tabDetachRequested(m_id, i);
            }
        });

        // Was schon drin war, wird der erste Reiter.
        if (m_content) {
            holderLayout->removeWidget(m_content);
            const QString first = m_notes.isEmpty()
                ? QStringLiteral("RX%1").arg(m_rxSource)
                : m_notes.section(QLatin1Char('\n'), 0, 0);
            m_stack->addWidget(m_content);
            m_tabBar->addTab(first);
        }

        holderLayout->addWidget(m_tabBar);
        holderLayout->addWidget(m_stack, 1);
    }

    widget->setParent(m_stack);
    m_stack->addWidget(widget);
    m_tabBar->addTab(title);
    m_tabBar->setVisible(m_tabBar->count() > 1);
    m_tabBar->setCurrentIndex(m_tabBar->count() - 1);

    // m_content zeigt weiter auf den ERSTEN Inhalt. Wer die Kachel
    // fragt, was sie enthaelt, soll nicht je nach angeklicktem Reiter
    // eine andere Antwort bekommen.
}

QWidget* ContainerWidget::takeTab(int index)
{
    if (!m_stack || !m_tabBar) { return nullptr; }
    if (index < 0 || index >= m_stack->count()) { return nullptr; }

    QWidget* w = m_stack->widget(index);
    if (!w) { return nullptr; }

    // Reihenfolge: erst aus dem Stapel, dann der Reiter. Andersherum
    // zeigt die Leiste kurz auf einen Inhalt, den es nicht mehr gibt.
    m_stack->removeWidget(w);
    w->setParent(nullptr);
    m_tabBar->removeTab(index);
    m_tabBar->setVisible(m_tabBar->count() > 1);

    if (index == 0) {
        // Der erste Inhalt ist gegangen — m_content muss nachziehen,
        // sonst zeigt es auf ein Widget, das woanders lebt.
        m_content = m_stack->count() > 0 ? m_stack->widget(0) : nullptr;
    }
    return w;
}

int ContainerWidget::tabCount() const
{
    return m_tabBar ? m_tabBar->count() : (m_content ? 1 : 0);
}

QString ContainerWidget::tabTitle(int index) const
{
    if (!m_tabBar || index < 0 || index >= m_tabBar->count()) { return {}; }
    return m_tabBar->tabText(index);
}

int ContainerWidget::currentTab() const
{
    return m_tabBar ? m_tabBar->currentIndex() : 0;
}

void ContainerWidget::setCurrentTab(int index)
{
    if (m_tabBar && index >= 0 && index < m_tabBar->count()) {
        m_tabBar->setCurrentIndex(index);
    }
}

void ContainerWidget::updateTitleBar()
{
    // Thetis ucMeter.cs:605-624 — adapted for 3 dock modes
    if (isFloating()) {
        m_btnFloat->setText(QStringLiteral("\u2199"));
        m_btnFloat->setToolTip(QStringLiteral("Dock"));
        m_btnAxis->setVisible(false);
        m_btnPin->setVisible(true);
    } else if (isPanelDocked()) {
        m_btnFloat->setText(QStringLiteral("\u2197"));
        m_btnFloat->setToolTip(QStringLiteral("Float"));
        m_btnAxis->setVisible(false);
        m_btnPin->setVisible(false);
    } else {
        m_btnFloat->setText(QStringLiteral("\u2197"));
        m_btnFloat->setToolTip(QStringLiteral("Float"));
        m_btnAxis->setVisible(true);
        m_btnPin->setVisible(false);
    }
}

void ContainerWidget::updateTitle()
{
    // Thetis ucMeter.cs:625-639
    QString prefix = QStringLiteral("RX");
    QString firstLine = m_notes.section(QLatin1Char('\n'), 0, 0);
    QString title = prefix + QString::number(m_rxSource);
    if (!firstLine.isEmpty()) {
        title += QStringLiteral(" ") + firstLine;
    }
    m_titleLabel->setText(title);
}

void ContainerWidget::setupBorder()
{
    // Thetis ucMeter.cs:640-643, extended with highlight outline
    // (Phase 3G-6). When m_highlighted the accent outline overrides the
    // normal border regardless of m_border, so the user always sees
    // which container the settings dialog is editing.
    if (m_highlighted) {
        setStyleSheet(QStringLiteral("ContainerWidget { border: 2px solid %1; }")
                          .arg(QLatin1String(Style::kAccent)));
    } else if (m_border) {
        setStyleSheet(QStringLiteral("ContainerWidget { border: 1px solid %1; }")
                          .arg(QLatin1String(Style::kBorderSubtle)));
    } else {
        setStyleSheet(QString());
    }
}

// --- Property setters ---

void ContainerWidget::setId(const QString& id) { m_id = id; m_id.remove(QLatin1Char('|')); }
void ContainerWidget::setRxSource(int rx) { m_rxSource = rx; updateTitle(); }

void ContainerWidget::setDockMode(DockMode mode)
{
    bool changed = (m_dockMode != mode);
    m_dockMode = mode;

    // Enforce minimum width in all dock modes so the applets are always
    // usable -- and never narrower than the content itself needs.
    refreshMinimumWidth();

    // ── Der Griff ────────────────────────────────────────────────────
    //
    // AUSSERHALB der Aenderungsbedingung, und das ist der Kern: beim
    // Wiederherstellen aus den Einstellungen steht der Modus schon auf
    // OVERLAY, `changed` ist also false — und der Container laege ohne
    // Griff da. Genau so war es im laufenden Programm am 2026-08-19.
    //
    // Zwei Dinge, die sich leicht verwechseln lassen:
    //   m_titleBarVisible       — DARF die Leiste erscheinen (Vorgabe true)
    //   m_titleBar->isVisible() — ZEIGT sie gerade
    // Im Splitter blendet die Ueberfahr-Automatik sie ein und aus.
    // Geloest muss sie STEHEN: die Applets darunter schlucken jede
    // Mausbewegung, das Ueberfahren kommt also nie an, und ohne Leiste
    // ist „frei bewegen" ein leeres Versprechen.
    if (m_titleBar && m_titleBarVisible) {
        m_titleBar->setVisible(mode != DockMode::PanelDocked);
        if (mode != DockMode::PanelDocked) { m_titleBar->raise(); }
    }

    if (changed) {
        updateTitleBar();
        emit dockModeChanged(mode);
    }
}

void ContainerWidget::setAxisLock(AxisLock lock)
{
    m_axisLock = lock;

    // Update axis button icon — from Thetis ucMeter.cs:936-968
    static const QChar arrows[] = {
        QChar(0x2190),  // LEFT
        QChar(0x2196),  // TOPLEFT
        QChar(0x2191),  // TOP
        QChar(0x2197),  // TOPRIGHT
        QChar(0x2192),  // RIGHT
        QChar(0x2198),  // BOTTOMRIGHT
        QChar(0x2193),  // BOTTOM
        QChar(0x2199),  // BOTTOMLEFT
    };
    int idx = static_cast<int>(lock);
    if (idx >= 0 && idx < 8) {
        m_btnAxis->setText(QString(arrows[idx]));
    }
}

void ContainerWidget::cycleAxisLock(bool reverse)
{
    // From Thetis ucMeter.cs:912-935 [v2.10.3.13]
    int n = static_cast<int>(m_axisLock);
    if (reverse) {
        n--;
    } else {
        n++;
    }
    if (n > static_cast<int>(AxisLock::BottomLeft)) {
        n = static_cast<int>(AxisLock::Left);
    }
    if (n < static_cast<int>(AxisLock::Left)) {
        n = static_cast<int>(AxisLock::BottomLeft);
    }
    setAxisLock(static_cast<AxisLock>(n));
    emit dockedMoved();
}

void ContainerWidget::setPinOnTop(bool pin)
{
    // From Thetis ucMeter.cs:974-978 [v2.10.3.13]
    m_pinOnTop = pin;
    m_btnPin->setText(pin ? QStringLiteral("\U0001F4CD") : QStringLiteral("\U0001F4CC"));
    setTopMost();
}

void ContainerWidget::setTopMost()
{
    // From Thetis ucMeter.cs:980-993 [v2.10.3.13]
    if (isFloating() && parentWidget()) {
        FloatingContainer* fc = qobject_cast<FloatingContainer*>(parentWidget());
        if (fc) {
            bool wasVisible = fc->isVisible();
            if (m_pinOnTop) {
                fc->setWindowFlags(fc->windowFlags() | Qt::WindowStaysOnTopHint);
            } else {
                fc->setWindowFlags(fc->windowFlags() & ~Qt::WindowStaysOnTopHint);
            }
            if (wasVisible) {
                fc->show();
            }
        }
    }
}

void ContainerWidget::setBorder(bool border) { m_border = border; setupBorder(); }
void ContainerWidget::setLocked(bool locked)
{
    if (m_locked == locked) { return; }
    m_locked = locked;
    updateLockButton();
    emit lockedChanged(m_locked);
}

// Zu heisst bernstein und geschlossenes Schloss, offen heisst grau.
// Beides zugleich — Farbe UND Zeichen — damit der Zustand auch dann
// traegt, wenn jemand die Farben umstellt.
void ContainerWidget::updateLockButton()
{
    if (!m_btnLock) { return; }

    m_btnLock->setText(m_locked ? QString::fromUtf8("\U0001F512")
                                : QString::fromUtf8("\U0001F513"));
    m_btnLock->setToolTip(m_locked
        ? QStringLiteral("Locked — click to release position and size")
        : QStringLiteral("Lock position and size"));

    if (m_locked) {
        m_btnLock->setStyleSheet(QStringLiteral(
            "QPushButton { background: transparent; border: none; color: %1;"
            "  font-size: 11px; padding: 2px 4px; }"
            "QPushButton:hover { background: %2; }")
            .arg(QLatin1String(Style::kAmberText),
                 QLatin1String(Style::kButtonHover)));
    } else {
        m_btnLock->setStyleSheet(QStringLiteral(
            "QPushButton { background: transparent; border: none; color: %1;"
            "  font-size: 11px; padding: 2px 4px; }"
            "QPushButton:hover { background: %2; color: %3; }")
            .arg(QLatin1String(Style::kTextSecondary),
                 QLatin1String(Style::kButtonHover),
                 QLatin1String(Style::kTextPrimary)));
    }

    // Der Greifpunkt sagt mit, ob sich etwas ziehen laesst: ein Pfeil,
    // der einen Umzug verspricht, den das Schloss verbietet, ist eine
    // Luege.
    if (m_grip) {
        m_grip->setStyleSheet(QStringLiteral(
            "background: %1; border-radius: 1px; margin: 4px 0;")
            .arg(QLatin1String(m_locked ? Style::kTextInactive
                                        : Style::kAmberText)));
        m_grip->setCursor(m_locked ? QCursor(Qt::ArrowCursor) : bewegenZeiger());
    }
}

// ── Kontextmenue: bewegen und feststellen ────────────────────────────
//
// Auf Ansage des Betreibers (2026-08-19): „jeder Container muss sich
// ueberall hinbewegen koennen … es sollte ein click and drop verwendet
// werden, anschliessend mit einem Schloss um es zu fixieren."
//
// Beides GAB es schon — drei Andock-Arten, freies Ziehen im
// ueberlagernden und schwebenden Zustand (updateDrag), und ein
// m_locked, das Ziehen UND Groessenaendern sperrt. Was fehlte, war der
// Weg dorthin: kein Menue, kein Knopf, nichts. Der Modus liess sich nur
// im Quelltext setzen.
//
// Darum hier ein Rechtsklick-Menue statt neuer Mechanik. Die
// Andock-Arten in ihren eigenen Worten:
//
//   Frei bewegen   — ueberlagernd: absolute Lage ueber dem Hauptbereich,
//                    Titelleiste ziehen, Ecke groessenaendern
//   Eigenes Fenster— schwebend, auch ausserhalb des Hauptfensters
//   Am Rand        — im Splitter, also EINE Achse; das ist der Zustand,
//                    ueber den sich der Betreiber beschwert hat
void ContainerWidget::contextMenuEvent(QContextMenuEvent* event)
{
    ScopedChildWidget<QMenu> menuOwner(window());   // stirbt nicht mit dem Rahmen, siehe ScopedChildWidget.h
    const QPointer<ContainerWidget> self(this);   // Menue am Fenster: ein nativer Vorfahr gab ihm keinen Fensterbezug (2026-09-30)
    QMenu& menu = *menuOwner.get();
    menu.setStyleSheet(QString::fromLatin1(kPopupMenu));

    QAction* move  = menu.addAction(QStringLiteral("Move freely"));
    move->setCheckable(true);
    move->setChecked(isOverlayDocked());
    move->setToolTip(QStringLiteral(
        "Place this container anywhere over the main area — drag its "
        "title bar to move, the corner to resize."));

    QAction* window = menu.addAction(QStringLiteral("Own window"));
    window->setCheckable(true);
    window->setChecked(isFloating());

    QAction* edge = menu.addAction(QStringLiteral("Dock at the edge"));
    edge->setCheckable(true);
    edge->setChecked(isPanelDocked());
    edge->setToolTip(QStringLiteral(
        "Back into the side panel. There it shares one axis with the "
        "panadapter and moves only with the splitter handle."));

    menu.addSeparator();

    QAction* lock = menu.addAction(QStringLiteral("Lock in place"));
    lock->setCheckable(true);
    lock->setChecked(m_locked);
    lock->setToolTip(QStringLiteral(
        "Freeze position and size. Nothing moves this container until "
        "the lock comes off — the way to keep a layout you like."));

    const QAction* chosen = menu.exec(event->globalPos());
    // Das Elternteil kann waehrend exec() gestorben sein — dann ist
    // auch das Menue weg und `this` eine Leiche. Siehe ScopedChildWidget.h.
    if (!menuOwner || !self) { return; }
    if (chosen == move && !isOverlayDocked())        { emit overlayRequested(); }
    else if (chosen == window && !isFloating())      { emit floatRequested(); }
    else if (chosen == edge && !isPanelDocked())     { emit dockRequested(); }
    else if (chosen == lock)                         { setLocked(!m_locked); }
    event->accept();
}
void ContainerWidget::setContainerEnabled(bool enabled) { m_enabled = enabled; }
void ContainerWidget::setShowOnRx(bool show) { m_showOnRx = show; }
void ContainerWidget::setShowOnTx(bool show) { m_showOnTx = show; }
void ContainerWidget::setHiddenByMacro(bool hidden) { m_hiddenByMacro = hidden; }
void ContainerWidget::setContainerMinimises(bool minimises) { m_containerMinimises = minimises; }
void ContainerWidget::setContainerHidesWhenRxNotUsed(bool hides) { m_containerHidesWhenRxNotUsed = hides; }

void ContainerWidget::setNotes(const QString& notes)
{
    QString scrubbed = notes;
    scrubbed.remove(QLatin1Char('|'));
    if (m_notes == scrubbed) { return; }
    m_notes = scrubbed;
    updateTitle();
    emit notesChanged(m_notes);
}

void ContainerWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // Die Leiste sitzt oben und ueber dem Inhalt — siehe die
    // Begruendung im Aufbau.
    if (m_titleBar && !isPanelDocked()) {
        m_titleBar->setGeometry(0, 0, width(), kTitleBarHeight);
        m_titleBar->raise();
    }
}

void ContainerWidget::setNoControls(bool noControls) { m_noControls = noControls; }
void ContainerWidget::setAutoHeight(bool autoHeight) { m_autoHeight = autoHeight; }

void ContainerWidget::setTitleBarVisible(bool visible)
{
    if (m_titleBarVisible == visible) { return; }
    m_titleBarVisible = visible;
    if (m_titleBar) {
        m_titleBar->setVisible(visible);
    }
    emit titleBarVisibilityChanged(visible);
    update();
}

void ContainerWidget::setMinimised(bool minimised)
{
    if (m_minimised == minimised) { return; }
    m_minimised = minimised;
    // Collapse the content area so only the title bar shows. The title
    // bar stays visible regardless of m_titleBarVisible while minimised
    // so the user retains a handle to un-minimise. FloatingContainer
    // (commit 10) picks up minimisedChanged() to resize its window.
    if (m_contentHolder) {
        m_contentHolder->setVisible(!minimised);
    }
    if (minimised && m_titleBar) {
        m_titleBar->setVisible(true);
    } else if (m_titleBar) {
        m_titleBar->setVisible(m_titleBarVisible);
    }
    emit minimisedChanged(minimised);
    update();
}

void ContainerWidget::setHighlighted(bool highlighted)
{
    if (m_highlighted == highlighted) { return; }
    m_highlighted = highlighted;
    setupBorder();
    update();
}
void ContainerWidget::setDockedLocation(const QPoint& loc) { m_dockedLocation = loc; }
void ContainerWidget::setDockedSize(const QSize& size) { m_dockedSize = size; }
void ContainerWidget::setDelta(const QPoint& delta) { m_delta = delta; }

void ContainerWidget::storeLocation()
{
    // From Thetis ucMeter.cs:567-572 [v2.10.3.13]
    m_dockedLocation = pos();
    m_dockedSize = size();
}

void ContainerWidget::restoreLocation()
{
    // From Thetis ucMeter.cs:574-593 [v2.10.3.13]
    bool moved = false;
    if (m_dockedLocation != pos()) {
        move(m_dockedLocation);
        moved = true;
    }
    if (m_dockedSize != size()) {
        resize(m_dockedSize);
        moved = true;
    }
    if (moved) {
        update();
    }
}

int ContainerWidget::roundToNearestTen(int value)
{
    return ((value + 5) / 10) * 10;
}

// ── Raster statt Strg-Zehnerschritte (2026-09-26) ─────────────────────
//
// Thetis (ucMeter.cs) rundet beim Ziehen/Groessern nur mit gedrueckter
// Strg-Taste auf 10. Der Betreiber will "alle widget in einer linie":
// Container rasten jetzt immer auf dasselbe Raster wie die Fenster
// (WindowPlacement kSnapGridPx), Strg gibt die freie Lage -- also genau
// umgekehrt. Bewusste Abweichung von Thetis, UX-Wunsch des Betreibers.
bool ContainerWidget::gridSnapActive()
{
    return !(QApplication::keyboardModifiers() & Qt::ControlModifier);
}

int ContainerWidget::roundToGrid(int value)
{
    const int g = kSnapGridPx;
    return static_cast<int>(std::lround(value / static_cast<double>(g))) * g;
}

// --- Hover show/hide ---

void ContainerWidget::mouseMoveEvent(QMouseEvent* event)
{
    // From Thetis ucMeter.cs:1198-1229 [v2.10.3.13]
    // Upstream inline attribution preserved verbatim (ucMeter.cs:1200):
    //   bool no_controls = _no_controls && !Common.ShiftKeyDown; //[2.10.3.6]MW0LGE no title or resize grabber, override by holding shift
    bool noControls = m_noControls && !(QApplication::keyboardModifiers() & Qt::ShiftModifier);

    if (!m_dragging && !noControls) {
        int y = static_cast<int>(event->position().y());
        if (y < kTitleHoverZone && !m_titleBar->isVisible()) {
            m_titleBar->setVisible(true);
            m_titleBar->raise();
        } else if (y >= kTitleBarHeight && m_titleBar->isVisible() && !m_dragging
                   && isPanelDocked()) {
            // Nur im Splitter wieder ausblenden. Geloest ist die Leiste
            // der EINZIGE Griff — sie wegzublenden, sobald der Zeiger
            // zwei Zentimeter tiefer steht, macht das Bewegen unmoeglich.
            // Genau daran ist der Selbsttest am 2026-08-19 gescheitert.
            m_titleBar->setVisible(false);
        }
    }

    // Resize grip: only for overlay-docked and floating, not panel-docked
    if (!m_resizing && !noControls && !isPanelDocked()) {
        bool inGripRegion = event->position().x() > (width() - 16)
                         && event->position().y() > (height() - 16);
        if (inGripRegion && !m_resizeGrip->isVisible()) {
            m_resizeGrip->move(width() - 12, height() - 12);
            m_resizeGrip->setVisible(true);
            m_resizeGrip->raise();
        } else if (!inGripRegion && m_resizeGrip->isVisible() && !m_resizing) {
            m_resizeGrip->setVisible(false);
        }
    }

    if (m_dragging) {
        updateDrag(event->globalPosition().toPoint());
    }
    if (m_resizing) {
        updateResize(event->globalPosition().toPoint());
    }

    QWidget::mouseMoveEvent(event);
}

void ContainerWidget::leaveEvent(QEvent* event)
{
    if (!m_dragging && !m_resizing) {
        // Siehe mouseMoveEvent: das Ausblenden gehoert in den Splitter.
        // Ein geloester Container behaelt seinen Griff, auch wenn die
        // Maus ihn verlaesst — sonst ist er beim naechsten Hinfahren
        // wieder unfassbar.
        if (isPanelDocked()) {
            m_titleBar->setVisible(false);
        }
        m_resizeGrip->setVisible(false);
    }
    QWidget::leaveEvent(event);
}

// --- Event filter for title bar drag + resize grip ---

bool ContainerWidget::eventFilter(QObject* watched, QEvent* event)
{
    // Title bar drag
    if ((watched == m_titleBar || watched == m_titleLabel
         || watched == m_grip) && !m_locked) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && !isPanelDocked()) {
                beginDrag(me->globalPosition().toPoint());
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_dragging) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            updateDrag(me->globalPosition().toPoint());
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_dragging) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            const QPoint wo = me->globalPosition().toPoint();
            endDrag();
            // Wo der Container gelandet ist -- fuer das Kombi-Feld
            // (2026-11-01). Hier wird nichts entschieden, nur
            // gemeldet: was dort liegt, weisz das Hauptfenster.
            emit zugEndeBei(wo);
            return true;
        }
    }

    // Content area hover — detect mouse in title region when title bar is hidden
    // (hidden title bar collapses in layout, so content fills top area)
    bool isContentArea = (watched == m_contentHolder || watched == m_content);
    if (!isContentArea && m_content) {
        QWidget* w = qobject_cast<QWidget*>(watched);
        if (w && w->isAncestorOf(m_content) == false && m_content->isAncestorOf(w)) {
            isContentArea = true;
        }
    }
    if (isContentArea && event->type() == QEvent::MouseMove && !m_locked) {
        QMouseEvent* me = static_cast<QMouseEvent*>(event);
        bool noControls = m_noControls && !(QApplication::keyboardModifiers() & Qt::ShiftModifier);
        if (!noControls) {
            QWidget* w = static_cast<QWidget*>(watched);
            int y = w->mapTo(this, me->position().toPoint()).y();
            if (y < kTitleHoverZone && !m_titleBar->isVisible()) {
                m_titleBar->setVisible(true);
                m_titleBar->raise();
            } else if (y >= kTitleBarHeight && m_titleBar->isVisible() && !m_dragging
                       && isPanelDocked()) {
                // Dieselbe Regel wie in mouseMoveEvent — hier fuer
                // Mausbewegungen, die in einem KIND ankommen. Der
                // Applet-Inhalt deckt den Container vollstaendig ab,
                // also laeuft praktisch jede Bewegung durch diesen Zweig.
                m_titleBar->setVisible(false);
            }
        }
    }

    // Resize grip (not for panel-docked)
    if (watched == m_resizeGrip && !m_locked && !isPanelDocked()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                beginResize(me->globalPosition().toPoint());
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_resizing) {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            updateResize(me->globalPosition().toPoint());
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_resizing) {
            endResize();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

// --- Drag logic ---

void ContainerWidget::beginDrag(const QPoint& globalPos)
{
    // From Thetis ucMeter.cs:281-294 [v2.10.3.13]
    m_dragging = true;
    if (isFloating()) {
        m_dragStartPos = globalPos - parentWidget()->pos();
    } else {
        raise();
        m_dragStartPos = globalPos - pos();
    }
}

void ContainerWidget::updateDrag(const QPoint& globalPos)
{
    if (!m_dragging) {
        return;
    }

    if (isFloating()) {
        // From Thetis ucMeter.cs:319-345 [v2.10.3.13]
        QPoint newPos = globalPos - m_dragStartPos;
        if (gridSnapActive()) {
            newPos.setX(roundToGrid(newPos.x()));
            newPos.setY(roundToGrid(newPos.y()));
        }
        if (parentWidget() && parentWidget()->pos() != newPos) {
            parentWidget()->move(newPos);
        }
    } else {
        // From Thetis ucMeter.cs:346-374 [v2.10.3.13] — overlay-docked, clamped
        QPoint newPos = globalPos - m_dragStartPos;
        if (gridSnapActive()) {
            newPos.setX(roundToGrid(newPos.x()));
            newPos.setY(roundToGrid(newPos.y()));
        }
        if (parentWidget()) {
            int maxX = parentWidget()->width() - width();
            int maxY = parentWidget()->height() - height();
            newPos.setX(std::clamp(newPos.x(), 0, std::max(0, maxX)));
            newPos.setY(std::clamp(newPos.y(), 0, std::max(0, maxY)));
        }
        if (pos() != newPos) {
            move(newPos);
            update();
        }
    }
}

void ContainerWidget::endDrag()
{
    m_dragging = false;
    m_dragStartPos = QPoint();
    if (isOverlayDocked()) {
        m_dockedLocation = pos();
        emit dockedMoved();
    }
}

// --- Resize logic ---

void ContainerWidget::beginResize(const QPoint& globalPos)
{
    // From Thetis ucMeter.cs:400-407 [v2.10.3.13]
    m_resizeStartGlobal = globalPos;
    m_resizeStartSize = isFloating() && parentWidget() ? parentWidget()->size() : size();
    m_resizing = true;
    raise();
}

void ContainerWidget::updateResize(const QPoint& globalPos)
{
    if (!m_resizing) {
        return;
    }

    // From Thetis ucMeter.cs:489-518 [v2.10.3.13]
    int dX = globalPos.x() - m_resizeStartGlobal.x();
    int dY = globalPos.y() - m_resizeStartGlobal.y();

    int newW = m_resizeStartSize.width() + dX;
    int newH = m_resizeStartSize.height() + dY;

    if (gridSnapActive()) {
        newW = roundToGrid(newW);
        newH = roundToGrid(newH);
    }

    doResize(newW, newH);
}

void ContainerWidget::endResize()
{
    m_resizing = false;
    m_resizeStartGlobal = QPoint();
    if (isOverlayDocked()) {
        m_dockedSize = size();
    }
}

void ContainerWidget::doResize(int w, int h)
{
    // From Thetis ucMeter.cs:520-549 [v2.10.3.13]
    w = std::max(w, kMinContainerWidth);
    h = std::max(h, kMinContainerHeight);

    if (isFloating()) {
        if (parentWidget()) {
            parentWidget()->resize(w, h);
        }
    } else {
        if (parentWidget()) {
            if (x() + w > parentWidget()->width()) {
                w = parentWidget()->width() - x();
            }
            if (y() + h > parentWidget()->height()) {
                h = parentWidget()->height() - y();
            }
        }
        QSize newSize(w, h);
        if (newSize != size()) {
            resize(newSize);
            update();
        }
    }
}

// --- Interactive item signal forwarding ---

void ContainerWidget::wireInteractiveItem(MeterItem* item)
{
    if (auto* band = qobject_cast<BandButtonItem*>(item)) {
        connect(band, &BandButtonItem::bandClicked,
                this, &ContainerWidget::bandClicked);
        connect(band, &BandButtonItem::bandStackRequested,
                this, &ContainerWidget::bandStackRequested);
    } else if (auto* mode = qobject_cast<ModeButtonItem*>(item)) {
        connect(mode, &ModeButtonItem::modeClicked,
                this, &ContainerWidget::modeClicked);
    } else if (auto* filter = qobject_cast<FilterButtonItem*>(item)) {
        connect(filter, &FilterButtonItem::filterClicked,
                this, &ContainerWidget::filterClicked);
        connect(filter, &FilterButtonItem::filterContextRequested,
                this, &ContainerWidget::filterContextRequested);
    } else if (auto* ant = qobject_cast<AntennaButtonItem*>(item)) {
        connect(ant, &AntennaButtonItem::antennaSelected,
                this, &ContainerWidget::antennaSelected);
        // Phase 3P-I-a T17 — late-added antenna items inherit the
        // container's current hasAlex state (set by MainWindow on
        // connect / currentRadioChanged via setBoardCapabilities).
        ant->setHasAlex(m_hasAlex);
    } else if (auto* step = qobject_cast<TuneStepButtonItem*>(item)) {
        connect(step, &TuneStepButtonItem::tuneStepSelected,
                this, &ContainerWidget::tuneStepSelected);
    } else if (auto* other = qobject_cast<OtherButtonItem*>(item)) {
        connect(other, &OtherButtonItem::otherButtonClicked,
                this, &ContainerWidget::otherButtonClicked);
        connect(other, &OtherButtonItem::macroTriggered,
                this, &ContainerWidget::macroTriggered);
    } else if (auto* voice = qobject_cast<VoiceRecordPlayItem*>(item)) {
        connect(voice, &VoiceRecordPlayItem::voiceAction,
                this, &ContainerWidget::voiceAction);
    } else if (auto* discord = qobject_cast<DiscordButtonItem*>(item)) {
        connect(discord, &DiscordButtonItem::discordAction,
                this, &ContainerWidget::discordAction);
    } else if (auto* vfo = qobject_cast<VfoDisplayItem*>(item)) {
        connect(vfo, &VfoDisplayItem::frequencyChangeRequested,
                this, &ContainerWidget::frequencyChangeRequested);
        connect(vfo, &VfoDisplayItem::bandStackRequested,
                this, &ContainerWidget::bandStackRequested);
        connect(vfo, &VfoDisplayItem::filterContextRequested,
                this, &ContainerWidget::filterContextRequested);
    }
}

// --- Serialization ---

QString ContainerWidget::axisLockToString(AxisLock lock)
{
    static const char* names[] = {
        "LEFT", "TOPLEFT", "TOP", "TOPRIGHT",
        "RIGHT", "BOTTOMRIGHT", "BOTTOM", "BOTTOMLEFT"
    };
    int idx = static_cast<int>(lock);
    return (idx >= 0 && idx < 8) ? QString::fromLatin1(names[idx]) : QStringLiteral("TOPLEFT");
}

AxisLock ContainerWidget::axisLockFromString(const QString& str)
{
    static const QMap<QString, AxisLock> map = {
        {QStringLiteral("LEFT"), AxisLock::Left},
        {QStringLiteral("TOPLEFT"), AxisLock::TopLeft},
        {QStringLiteral("TOP"), AxisLock::Top},
        {QStringLiteral("TOPRIGHT"), AxisLock::TopRight},
        {QStringLiteral("RIGHT"), AxisLock::Right},
        {QStringLiteral("BOTTOMRIGHT"), AxisLock::BottomRight},
        {QStringLiteral("BOTTOM"), AxisLock::Bottom},
        {QStringLiteral("BOTTOMLEFT"), AxisLock::BottomLeft},
    };
    return map.value(str.toUpper(), AxisLock::TopLeft);
}

QString ContainerWidget::dockModeToString(DockMode mode)
{
    switch (mode) {
    case DockMode::PanelDocked:   return QStringLiteral("PANEL");
    case DockMode::OverlayDocked: return QStringLiteral("OVERLAY");
    case DockMode::Floating:      return QStringLiteral("FLOATING");
    }
    return QStringLiteral("OVERLAY");
}

DockMode ContainerWidget::dockModeFromString(const QString& str)
{
    if (str == QStringLiteral("PANEL")) {
        return DockMode::PanelDocked;
    }
    if (str == QStringLiteral("FLOATING")) {
        return DockMode::Floating;
    }
    return DockMode::OverlayDocked;
}

QString ContainerWidget::serialize() const
{
    // Based on Thetis ucMeter.cs:1012-1038, extended with DockMode (field 23)
    QStringList p;
    p << m_id;                                                              // 0
    p << QString::number(m_rxSource);                                       // 1
    p << QString::number(m_dockedLocation.x());                             // 2
    p << QString::number(m_dockedLocation.y());                             // 3
    p << QString::number(m_dockedSize.width());                             // 4
    p << QString::number(m_dockedSize.height());                            // 5
    p << (isFloating() ? QStringLiteral("true") : QStringLiteral("false")); // 6 (Thetis compat)
    p << QString::number(m_delta.x());                                      // 7
    p << QString::number(m_delta.y());                                      // 8
    p << axisLockToString(m_axisLock);                                      // 9
    p << (m_pinOnTop ? QStringLiteral("true") : QStringLiteral("false"));   // 10
    p << (m_border ? QStringLiteral("true") : QStringLiteral("false"));     // 11
    p << m_backgroundColor.name(QColor::HexArgb);                           // 12
    p << (m_noControls ? QStringLiteral("true") : QStringLiteral("false")); // 13
    p << (m_enabled ? QStringLiteral("true") : QStringLiteral("false"));    // 14
    p << m_notes;                                                           // 15
    p << (m_containerMinimises ? QStringLiteral("true") : QStringLiteral("false")); // 16
    p << (m_autoHeight ? QStringLiteral("true") : QStringLiteral("false")); // 17
    p << (m_showOnRx ? QStringLiteral("true") : QStringLiteral("false"));   // 18
    p << (m_showOnTx ? QStringLiteral("true") : QStringLiteral("false"));   // 19
    p << (m_locked ? QStringLiteral("true") : QStringLiteral("false"));     // 20
    p << (m_containerHidesWhenRxNotUsed ? QStringLiteral("true") : QStringLiteral("false")); // 21
    p << (m_hiddenByMacro ? QStringLiteral("true") : QStringLiteral("false")); // 22
    p << dockModeToString(m_dockMode);                                      // 23 (NereusSDR extension)
    p << (m_titleBarVisible ? QStringLiteral("true") : QStringLiteral("false")); // 24 (Phase 3G-6)
    return p.join(QLatin1Char('|'));
}

bool ContainerWidget::deserialize(const QString& data)
{
    // Based on Thetis ucMeter.cs:1039-1160, backward-compatible
    if (data.isEmpty()) {
        return false;
    }

    QStringList p = data.split(QLatin1Char('|'));
    if (p.size() < 13) {
        qCWarning(lcContainer) << "deserialize: too few fields:" << p.size();
        return false;
    }

    if (p[0].isEmpty()) {
        return false;
    }
    setId(p[0]);

    bool ok = false;
    int rx = p[1].toInt(&ok);
    if (!ok) { return false; }
    setRxSource(rx);

    int x = p[2].toInt(&ok); if (!ok) { return false; }
    int y = p[3].toInt(&ok); if (!ok) { return false; }
    int w = p[4].toInt(&ok); if (!ok) { return false; }
    int h = p[5].toInt(&ok); if (!ok) { return false; }
    setDockedLocation(QPoint(x, y));
    setDockedSize(QSize(w, h));

    // Field 6: Thetis floating bool — used as fallback if field 23 absent
    bool thetisFloating = (p[6].toLower() == QStringLiteral("true"));

    x = p[7].toInt(&ok); if (!ok) { return false; }
    y = p[8].toInt(&ok); if (!ok) { return false; }
    setDelta(QPoint(x, y));

    setAxisLock(axisLockFromString(p[9]));
    setPinOnTop(p[10].toLower() == QStringLiteral("true"));
    setBorder(p[11].toLower() == QStringLiteral("true"));

    QColor bgColor(p[12]);
    // Das alte Blau (#0f0f1a, bis 2026-11-04 die Vorgabe) steht in jedem
    // gespeicherten Profil; es ist keine Wahl des Betreibers, sondern
    // die damalige Vorgabe -- und wird zur heutigen.
    if (bgColor.isValid() && bgColor.rgb() == qRgb(0x0f, 0x0f, 0x1a)) {
        bgColor = QColor(QLatin1String(Style::kAppBg));
    }
    if (bgColor.isValid()) {
        m_backgroundColor = bgColor;
        m_contentHolder->setStyleSheet(
            QStringLiteral("background: %1;").arg(bgColor.name(QColor::HexArgb)));
    }

    if (p.size() > 13) { setNoControls(p[13].toLower() == QStringLiteral("true")); }
    if (p.size() > 14) { setContainerEnabled(p[14].toLower() == QStringLiteral("true")); }
    if (p.size() > 15) { setNotes(p[15]); }
    if (p.size() > 16) { setContainerMinimises(p[16].toLower() == QStringLiteral("true")); }
    if (p.size() > 17) { setAutoHeight(p[17].toLower() == QStringLiteral("true")); }
    if (p.size() > 19) {
        setShowOnRx(p[18].toLower() == QStringLiteral("true"));
        setShowOnTx(p[19].toLower() == QStringLiteral("true"));
    }
    if (p.size() > 20) { setLocked(p[20].toLower() == QStringLiteral("true")); }
    if (p.size() > 21) { setContainerHidesWhenRxNotUsed(p[21].toLower() == QStringLiteral("true")); }
    if (p.size() > 22) { setHiddenByMacro(p[22].toLower() == QStringLiteral("true")); }

    // Field 23: Longpath DockMode extension
    if (p.size() > 23) {
        setDockMode(dockModeFromString(p[23]));
    } else {
        setDockMode(thetisFloating ? DockMode::Floating : DockMode::OverlayDocked);
    }

    // Field 24: Phase 3G-6 title-bar visibility toggle
    if (p.size() > 24) {
        setTitleBarVisible(p[24].toLower() == QStringLiteral("true"));
    }

    qCDebug(lcContainer) << "Deserialized:" << m_id << "rx:" << m_rxSource
                          << "mode:" << dockModeToString(m_dockMode)
                          << "pos:" << m_dockedLocation << "size:" << m_dockedSize;
    return true;
}

// Phase 3P-I-a T17 — propagate hasAlex to every AntennaButtonItem
// nested inside this container's content tree. Walks all MeterWidgets
// reachable from m_content via findChildren (handles nested applet
// panels that host a header MeterWidget as well as flat content).
void ContainerWidget::setBoardCapabilities(const BoardCapabilities& caps)
{
    m_hasAlex = caps.hasAlex;
    if (!m_content) { return; }

    // The content may be the MeterWidget directly, or an applet panel
    // hosting one as its header. findChildren catches both.
    const QList<MeterWidget*> meters =
        m_content->findChildren<MeterWidget*>();
    QList<MeterWidget*> scan = meters;
    if (auto* asMeter = qobject_cast<MeterWidget*>(m_content)) {
        scan.prepend(asMeter);
    }
    for (MeterWidget* mw : scan) {
        for (MeterItem* item : mw->items()) {
            if (auto* ant = qobject_cast<AntennaButtonItem*>(item)) {
                ant->setHasAlex(m_hasAlex);
            }
        }
    }
}

} // namespace Longpath
