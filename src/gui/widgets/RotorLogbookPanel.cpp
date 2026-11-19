// =================================================================
// src/gui/widgets/RotorLogbookPanel.cpp  (Longpath)
// =================================================================
//
// Longpath-original — see RotorLogbookPanel.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-11-17 — Jedes geloggte QSO geht an ALLE eingerichteten Dienste
//                 (QRZ, Wavelog, lokaler Logger) ueber QsoUploadQueue;
//                 die Statuszeile nennt den Dienst, der Vermerk gilt dem
//                 genauen QSO statt dem zuletzt geloggten. Martin
//                 Fischer, KI-gestuetzt ueber Anthropic Claude Code.
//   2026-10-21 — Eine Sonde bleibt offline: ein isolierter Lauf startet
//                 rotctld nicht von selbst. Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
//   2026-11-11 — Rotordialog: GS-232 am Netzserver oeffnet im Netzmodus
//                 mit Host/Port vorbelegt; host:port in der Schnittstelle
//                 gilt als Netzziel. Martin Fischer, KI-gestuetzt ueber
//                 Anthropic Claude.
//   2026-08-10 — workSpot() entry point for "Turn rotor to <call>" from
//                 the Spot Hub. Enter in the callsign field now only
//                 looks the station up (it used to also start the
//                 rotator — moving the mast is not what Enter means).
//                 The QRZ locator is shown on the station card instead
//                 of only being written silently into the DX field.
//                 AI-assisted via Anthropic Claude (Cowork), operator
//                 Martin Fischer.
//   2026-09-16 — First live run against a microHAM ARCO on the LAN
//                 (GS-232A over TCP), three defects found in one go:
//                 the setup dialog's Connect used the port combo's
//                 stale currentData() instead of the address on screen
//                 and then saved that wrong value back; a rotctld whose
//                 controller link died (the ARCO drops a silent session
//                 after ~20 s) is now restarted on the reply watchdog
//                 instead of being reconnected to forever; rotctld
//                 exiting on its own is now reported. AI-assisted via
//                 Anthropic Claude (Claude Code), operator Martin Fischer.
// =================================================================

#include "RotorLogbookPanel.h"
#include "core/RotorBereich.h"
#include "gui/widgets/RotorBandWidget.h"
#include "core/TransverterSetup.h"

#include <QLoggingCategory>
#include "gui/ScopedChildWidget.h"
#include "GlobeWidget.h"
#include "WorldTexture.h"
#include "RotorDialWidget.h"

#include <QContextMenuEvent>

#include "core/AdifLog.h"
#include "core/AppSettings.h"
#include "core/LogbookDatei.h"
#include "core/CtyDatParser.h"
#include "core/DxccColorProvider.h"
#include "core/DxccFlag.h"
#include "core/HamlibInstaller.h"
#include "core/Maidenhead.h"
#include "core/QrzClient.h"
#include "core/QrzLogbookUploader.h"
#include "core/QsoUploadQueue.h"
#include "core/Gs232Rotor.h"
#include "core/RotctldClient.h"
#include "core/RotctldProcess.h"
#include "core/RotorModels.h"
#include "core/SolarTimes.h"
#include "gui/LogbookWindow.h"
#include "core/sat/SatelliteService.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/StationPhoto.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QAction>
#include <QColorDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QCryptographicHash>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMargins>
#include <QResizeEvent>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QInputDialog>
#include <QMenu>
#include <QPointer>
#include <cmath>
#include <limits>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QClipboard>
#include <QGuiApplication>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <QTimer>
#include <QRegularExpression>
#include <QUrl>
#ifdef HAVE_SERIALPORT
#include <QSerialPortInfo>
#endif

#include <algorithm>

namespace Longpath {

namespace {



const QString kMyGridKey    = QStringLiteral("StationGridSquare");
const QString kWorldImgKey  = QStringLiteral("GlobeWorldImagePath");
const QString kShowPhotoKey = QStringLiteral("RotorLogShowQrzPhoto");
// Der Rotor hat bis zum 2026-10-11 nie ins Protokoll geschrieben: ob
// er verbunden war, liess sich nur am Statusschild im Fenster ablesen
// -- an dem Tag musste ich den Widget-Baum der laufenden Instanz ueber
// die Automatisierungsbruecke abfragen, um "Rotator connected" zu
// sehen. In einem Protokoll, das sonst jede Verbindung mitschreibt,
// fehlte genau die eine Zeile, die man beim Nachsehen braucht.
Q_LOGGING_CATEGORY(lcRotor, "longpath.rotor")

const QString kRotorHostKey = QStringLiteral("RotorRotctldHost");
const QString kRotorPortKey = QStringLiteral("RotorRotctldPort");
// Teachable presets + park (2026-08-11). %1 is the slot number 1..4.
// Empty Deg value = slot not taught yet. Client-authoritative, so
// AppSettings per the settings policy.
const QString kPresetLabelKey = QStringLiteral("RotorPreset%1Label");
const QString kPresetDegKey   = QStringLiteral("RotorPreset%1Deg");
const QString kParkDegKey     = QStringLiteral("RotorParkDeg");
const QString kRotorUseSerialKey = QStringLiteral("RotorUseLocalSerial");
const QString kRotorModelKey     = QStringLiteral("RotorHamlibModel");
const QString kRotorDeviceKey    = QStringLiteral("RotorSerialDevice");
const QString kRotorBaudKey      = QStringLiteral("RotorSerialBaud");
// Beim Start wieder verbinden, wenn der Betreiber zuletzt verbunden hatte
// (und nicht selbst getrennt). "True"/"False" (2026-09-26).
const QString kRotorConnectOnStartKey = QStringLiteral("RotorConnectOnStart");
// Mechanical end stop, degrees; negative = turns freely. Drives the
// dial's travel-path maths so the shown direction is the one the mast
// can actually take.
const QString kRotorEndStopKey   = QStringLiteral("RotorEndStopDeg");
const QString kAutoLookupKey = QStringLiteral("QrzAutoLookupWhileTyping");

// Long enough that typing a callsign is one request, short enough that
// the answer is there before the operator reaches for the log button.
constexpr int kAutoLookupDelayMs = 700;

// Below this, a partial callsign matches the pattern often enough to
// waste a lookup: "OE1" is a legal prefix and not a station.
constexpr int kMinAutoLookupChars = 4;

// NASA Blue Marble. Works of the US government are public domain, so
// this can be fetched and kept without anyone agreeing to anything —
// which is why it is offered as a download instead of being bundled:
// the file is larger than the rest of the application's assets put
// together, and the globe is one optional view.
//
// Each entry is a list of candidates tried in order. NASA reorganised
// its image sites and the deep paths do move; one dead link should be
// a pause of a second, not a feature that does not work. My first
// attempt at the large one 404'd because the Blue Marble Next
// Generation naming carries a band count — "200412.3x5400x2700", not
// "200412x5400x2700" — which is exactly the kind of detail a fallback
// list exists to absorb.
const QStringList& smallCandidates()
{
    static const QStringList l = {
        QStringLiteral("https://eoimages.gsfc.nasa.gov/images/imagerecords/"
                       "57000/57752/land_shallow_topo_2048.jpg"),
        QStringLiteral("https://eoimages.gsfc.nasa.gov/images/imagerecords/"
                       "57000/57752/land_ocean_ice_2048.jpg"),
    };
    return l;
}

const QStringList& largeCandidates()
{
    static const QStringList l = {
        QStringLiteral("https://eoimages.gsfc.nasa.gov/images/imagerecords/"
                       "73000/73909/world.topo.bathy.200412.3x5400x2700.jpg"),
        QStringLiteral("https://eoimages.gsfc.nasa.gov/images/imagerecords/"
                       "73000/73776/world.topo.bathy.200408.3x5400x2700.jpg"),
        QStringLiteral("https://eoimages.gsfc.nasa.gov/images/imagerecords/"
                       "74000/74117/world.200412.3x5400x2700.jpg"),
    };
    return l;
}

// The portrait cache used to be described here. It moved to
// StationPhoto::cacheDir() along with the rest of the fetching rules
// (2026-08-10) — the logbook detail pane needs the same directory, and
// two functions deciding where the cache lives is one too many.

// A row that can be hidden when the panel is dragged small.
//
// QLayout has no setVisible(), so every row that might be shed needs a
// widget of its own to hide. Zero margins so wrapping changes nothing
// about how the row looks while it is up. (2026-08-10)
QWidget* addShedRow(QVBoxLayout* col, QLayout* row, QWidget* parent)
{
    auto* w = new QWidget(parent);
    row->setContentsMargins(0, 0, 0, 0);
    w->setLayout(row);
    col->addWidget(w);
    return w;
}

QLabel* caption(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    QFont f = l->font();
    f.setPixelSize(9);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 1.1);
    l->setFont(f);
    l->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                         .arg(Style::kTextScale));
    return l;
}

QString compassPoint(double deg)
{
    static const char* kPoints[] = {"N", "NNE", "NE", "ENE", "E", "ESE",
                                    "SE", "SSE", "S", "SSW", "SW", "WSW",
                                    "W", "WNW", "NW", "NNW"};
    int idx = static_cast<int>(deg / 22.5 + 0.5) % 16;
    if (idx < 0) { idx += 16; }
    return QString::fromLatin1(kPoints[idx]);
}

} // namespace

RotorLogbookPanel::RotorLogbookPanel(RadioModel* radio, QrzClient* qrz,
                                     QrzLogbookUploader* uploader,
                                     QWidget* parent)
    : QWidget(parent), m_radio(radio), m_qrz(qrz), m_uploader(uploader)
{
    buildUi();
    wireQrz();
    applyLocators();
    refreshRecentList();

    // The sun keeps moving while you are in QSO. A minute is finer than
    // the model's own accuracy needs, but coarse enough to be free.
    auto* solarTick = new QTimer(this);
    solarTick->setInterval(60 * 1000);
    connect(solarTick, &QTimer::timeout,
            this, &RotorLogbookPanel::updateSolarLine);
    solarTick->start();
}

// ── Abbau ──────────────────────────────────────────────────────────────
//
// Diese zwei Zeilen sind die ERSTEN im Abbau, und das ist der ganze
// Punkt. Der Weg, den der Betreiber sechs Mal gesehen hat (Berichte vom
// 2026-10-10 16:05/16:06/16:10/16:13, 21:34 und 2026-10-11 11:08, alle
// SIGSEGV):
//
//   ~RotorLogbookPanel -> ~QWidget -> deleteChildren()
//     -> ~RotctldClient -> ~QAbstractSocket -> disconnectFromHost()
//     -> stateChanged(Disconnected) -> das Lambda aus ensureRotor()
//     -> setStatus() -> m_status->setText()   == toter Zeiger
//
// Die Kinder werden in der Reihenfolge geloescht, in der sie angelegt
// wurden: das Statusschild baut buildUi() zuerst, den Rotctld-Client
// legt ensureRotor() spaeter an. Wenn also der Socket im Abbau noch
// einmal meldet, ist das Schild schon weg -- und die QObject-Basis des
// Feldes lebt noch, weshalb die Verbindung noch traegt.
//
// Dieselbe Fehlerform steckte in diesem Programm schon in
// SpeConnection, MainWindow, PgxlConnection und TgxlConnection; behoben
// wird sie an der Ursache (Verbindung trennen), nicht mit einer
// Abbau-Fahne und Waechtern in jedem Zweig.
//
// m_rotorProc ist ein MITGLIED und wird noch vor den Kindern abgebaut;
// sein exited()-Rueckruf fasst dasselbe Schild an, also geht er mit.
RotorLogbookPanel::~RotorLogbookPanel()
{
    if (m_rotor) { m_rotor->disconnect(this); }
    m_rotorProc.disconnect(this);
}

QString RotorLogbookPanel::logbookPath()
{
    // Eine Stelle fuer alle: seit dem QSO-Eintrag vom Telefon (TCI
    // `log_qso:`) gibt es einen zweiten Anlass, in diese Datei zu
    // schreiben. Zwei Stellen, die dieselbe Datei anfassen, sind genau die
    // Lage, die am 2026-10-03 die Logzeilen zerschrieben hat.
    return LogbookDatei::pfad();
}

// ── UI ──────────────────────────────────────────────────────────────

void RotorLogbookPanel::buildUi()
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral("background: %1;").arg(Style::kAppBg));

    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(10, 8, 10, 8);
    col->setSpacing(7);

    // Callsign
    auto* callRow = new QHBoxLayout;
    callRow->setSpacing(6);
    m_callEdit = new QLineEdit(this);
    m_callEdit->setPlaceholderText(QStringLiteral("callsign"));
    QFont cf = m_callEdit->font();
    cf.setPixelSize(16);
    cf.setBold(true);
    m_callEdit->setFont(cf);
    m_callEdit->setStyleSheet(Style::lineEditStyle());
    callRow->addWidget(m_callEdit, 1);

    m_lookupBtn = new QPushButton(QStringLiteral("QRZ"), this);
    m_lookupBtn->setStyleSheet(Style::buttonBaseStyle());
    callRow->addWidget(m_lookupBtn);

    // A second Log button, here in the callsign row. The one further
    // down is where it belongs when you are filling in reports and a
    // comment; this one is for the other case, which is most of them:
    // type a callsign, watch the name and locator arrive, click. Two
    // buttons for one action is usually a smell, but the distance
    // between the callsign field and the bottom of the panel is the
    // whole cost of logging a contact when you are working a run.
    auto* quickLogBtn = new QPushButton(QStringLiteral("LOG"), this);
    quickLogBtn->setStyleSheet(Style::buttonBaseStyle()
                               + Style::greenCheckedStyle());
    quickLogBtn->setToolTip(QStringLiteral(
        "Log this contact now, with whatever is filled in"));
    callRow->addWidget(quickLogBtn);
    connect(quickLogBtn, &QPushButton::clicked,
            this, &RotorLogbookPanel::onLogQso);

    // Betreiber (2026-09-02): Tab aus dem Rufzeichenfeld soll SOFORT auf
    // LOG fuehren, nicht erst ueber den QRZ-Knopf — und dort angekommen
    // soll Enter genuegen. setAutoDefault() allein reicht, weil dieses
    // Panel kein QDialog ist: Enter loest den Knopf nur aus, wenn ER
    // selbst den Fokus haelt, nicht irgendein anderes Feld im Panel —
    // das Enter-auf-QRZ-Verhalten des Rufzeichenfelds (weiter unten,
    // returnPressed) bleibt davon unberuehrt.
    quickLogBtn->setAutoDefault(true);
    setTabOrder(m_callEdit, quickLogBtn);

    // ── Das Zahnrad (2026-11-12, Entwurf R) ─────────────────────────
    //
    // Betreiber: „viele sachen werden auch angezeigt, nicht unwichtig
    // sind ... die wir auch in optionen einbinden können". Weltbild,
    // Rotor-Einrichtung, Ansicht (Rose/Kugel), der eigene Locator, die
    // QRZ-Fotos und das Logbuch stehen jetzt hier statt als Knoepfe und
    // Zeilen im Feld. Im Feld selbst, nicht im Kopf des Gehaeuses: das
    // Feld wohnt in vier verschiedenen Gehaeusen, und erreichbar heisst
    // in allen.
    m_optionenBtn = new QPushButton(QStringLiteral("⚙"), this);
    m_optionenBtn->setObjectName(QStringLiteral("rotorLogOptions"));
    m_optionenBtn->setStyleSheet(Style::buttonBaseStyle());
    m_optionenBtn->setFixedWidth(30);
    m_optionenBtn->setFocusPolicy(Qt::NoFocus);
    m_optionenBtn->setToolTip(QStringLiteral(
        "Options: rose or globe, world image, rotor setup, your own locator, "
        "QRZ photos, the logbook"));
    callRow->addWidget(m_optionenBtn);
    connect(m_optionenBtn, &QPushButton::clicked, this, &RotorLogbookPanel::zeigeOptionen);

    m_rowCall = addShedRow(col, callRow, this);

    // Station card: portrait on top, name line below. Was a side-by-side
    // row (portrait left, text right); the operator wanted the portrait
    // as large as the column allows instead, with the text underneath
    // and trimmed to what still fits at a glance (2026-09-01).
    auto* cardRow = new QVBoxLayout;
    cardRow->setSpacing(6);

    // The fetching rules — https only, no downgrade on redirect, a size
    // cap, a cache beside the log — used to live in this file. The
    // logbook detail pane needs the same ones, and two copies of a
    // security decision is one copy too many, so they moved into
    // StationPhoto and both callers use it. (2026-08-10)
    m_photo = new StationPhoto(this);
    // Betreiber 2026-09-01: "foto soll so gross wie moeglich sein" --
    // erst 78x78 (Quadrat, quetschte das Portraet), dann 160x120. Jetzt
    // so gross, wie die Spalte traegt, ohne die Reglerzeilen darunter zu
    // verdraengen (die Klasse selbst ist fuer 220x165 gebaut, siehe
    // StationPhoto::sizeHint()).
    m_photo->setFixedSize(300, 225);
    m_photo->setVisible(false);
    // A portrait is one network request per station, so it has to be
    // switchable — and a setting nobody can find is not a switch.
    m_photo->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_photo, &QWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) {
        ScopedChildWidget<QMenu> menuOwner(window());   // stirbt nicht mit dem Rahmen, siehe ScopedChildWidget.h
        const QPointer<RotorLogbookPanel> self(this);   // Menue am Fenster: am nativen Feld bekam es keinen Fensterbezug (2026-09-27)
        QMenu& menu = *menuOwner.get();
        QAction* off = menu.addAction(QStringLiteral("Don't show QRZ photos"));
        const QAction* chosen = menu.exec(m_photo->mapToGlobal(pos));
        // Das Elternteil kann waehrend exec() gestorben sein — dann ist
        // auch das Menue weg und `this` eine Leiche. Siehe ScopedChildWidget.h.
        if (!menuOwner || !self) { return; }
        if (chosen == off) {
            AppSettings::instance().setValue(kShowPhotoKey, false);
            m_photo->showPlaceholder(QString{});
            m_photo->setVisible(false);
            setStatus(QStringLiteral("QRZ photos off — re-enable in settings "
                                     "(%1)").arg(kShowPhotoKey));
        }
    });
    // The panel's frame is small and beside a callsign that already
    // says who this is, so it stays hidden until there is a picture
    // rather than explaining itself in 78 pixels.
    connect(m_photo, &StationPhoto::photoShown, this,
            [this]() { m_photo->setVisible(true); });
    cardRow->addWidget(m_photo, 0, Qt::AlignHCenter);

    // The flag lives on the same line as the station text rather than in
    // a label of its own. A separate row is one more thing that can end
    // up zero-height or laid out off-screen, and then a missing flag
    // looks like a missing lookup.
    //
    // Betreiber 2026-09-01: "Daten koennen minimiert werden. Name,
    // Entfernung, Land wuerde reichen" -- jetzt eine Zeile statt zwei,
    // siehe stationText(). Zentriert unter dem groesseren Foto statt
    // linksbuendig daneben.
    m_stationLine = new QLabel(QString{}, this);
    m_stationLine->setWordWrap(true);
    m_stationLine->setAlignment(Qt::AlignHCenter);
    m_stationLine->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }").arg(Style::kTextPrimary));
    cardRow->addWidget(m_stationLine);

    // Grey line at the far end. On the low bands this decides whether
    // the path is open at all, so it belongs next to the callsign, not
    // buried in a propagation window. Still kept up to date (see
    // updateSolarLine()) for whoever re-enables it — just not shown in
    // this trimmed-down card (2026-09-01).
    m_solarLine = new QLabel(QString{}, this);
    m_solarLine->setWordWrap(true);
    m_solarLine->setVisible(false);

    // Have I had this one before, and does it count. Same as
    // m_solarLine above: still maintained, not shown here.
    m_workedLine = new QLabel(QString{}, this);
    m_workedLine->setWordWrap(true);
    m_workedLine->setAlignment(Qt::AlignHCenter);
    m_workedLine->setVisible(false);
    // Seit Entwurf R (2026-11-12) wieder zu sehen: neu, neues Band, wie
    // oft schon gearbeitet -- die Frage, die man beim Loggen stellt.
    cardRow->addWidget(m_workedLine);

    m_rowCard = addShedRow(col, cardRow, this);

    // ── Ablesegitter (2026-11-12, Entwurf R) ─────────────────────────
    //
    // Kurzweg, Langweg, Entfernung und Locator als vier Zahlen in
    // schwarzem Glas (Hausstil: Ablesegitter, Zahlen in Monospace). Der
    // eigene Locator steht nicht mehr im Feld -- er aendert sich fast
    // nie und liegt hinter dem Zahnrad; das Feld bleibt als Speicher.
    m_myGrid = new QLineEdit(
        AppSettings::instance().value(kMyGridKey, QString{}).toString(), this);
    m_myGrid->setPlaceholderText(QStringLiteral("your locator"));
    m_myGrid->setVisible(false);
    m_dxGrid = new QLineEdit(this);
    m_dxGrid->setPlaceholderText(QStringLiteral("locator"));
    m_dxGrid->setToolTip(QStringLiteral(
        "Their locator — filled in from QRZ or the log; type one when neither knows"));
    auto* gridRow = new QHBoxLayout;
    gridRow->setSpacing(4);
    auto zelle = [this, gridRow](const QString& titel, QWidget* wert) {
        auto* box = new QWidget(this);
        auto* v = new QVBoxLayout(box);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(1);
        auto* t = new QLabel(titel, box);
        t->setFont(Style::capsFont(font(), 8));
        t->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(Style::kTextScale));
        v->addWidget(t);
        v->addWidget(wert);
        gridRow->addWidget(box, 1);
    };
    auto ablesung = [this]() {
        auto* l = new QLabel(QStringLiteral("—"), this);
        l->setStyleSheet(Style::insetValueStyle());
        l->setMinimumHeight(22);
        return l;
    };
    m_roKurz  = ablesung();
    m_roLang  = ablesung();
    m_roWeite = ablesung();
    zelle(QStringLiteral("SHORT"), m_roKurz);
    zelle(QStringLiteral("LONG"), m_roLang);
    zelle(QStringLiteral("DIST"), m_roWeite);
    zelle(QStringLiteral("LOC"), m_dxGrid);
    m_rowGrid = addShedRow(col, gridRow, this);

    // Dial and globe share one slot rather than stacking vertically: in a
    // dock this narrow, two 260 px squares would push the log off screen,
    // and they answer the same question in two ways.
    m_dial  = new RotorDialWidget(this);
    m_dial->setEndStop(AppSettings::instance()
                           .value(kRotorEndStopKey, -1.0).toDouble());
    m_globe = new GlobeWidget(this);

    auto* globePage = new QWidget(this);
    auto* globeCol  = new QVBoxLayout(globePage);
    globeCol->setContentsMargins(0, 0, 0, 0);
    globeCol->setSpacing(4);
    globeCol->addWidget(m_globe, 1);

    m_viewStack = new QStackedWidget(this);
    m_viewStack->addWidget(m_dial);
    m_viewStack->addWidget(globePage);

    col->addWidget(m_viewStack, 1);

    const QString saved =
        AppSettings::instance().value(kWorldImgKey, QString{}).toString();
    if (!saved.isEmpty()) { m_globe->loadTexture(saved); }

    // Betreiber 2026-09-02, Task 3: Landfarbe/Schema-Umschalter lebten
    // kurzzeitig im Rechtsklick-Menue der Kugel selbst -- zwei
    // Bedienwege fuer dieselbe Sache (wie das Weltbild aussieht). Jetzt
    // beides hier, im einen Knopf, der schon fuer "wie sieht die Kugel
    // aus" zustaendig ist.
    // Rotor buttons
    // ── Rotorband und Drehknoepfe (2026-11-12, Entwurf R) ──────────
    //
    // Das Band aus der Rotorzeile des Logbuchs (Entwurf C vom 11.11.):
    // Nadel = Stand, Dreieck = Station. Die Knoepfe sagen, wohin sie
    // drehen, und ein Weg hinter dem Berg heisst „blocked" (der
    // erlaubte Bereich, core/RotorBereich.h) -- vorher sah ein gesperrter
    // Knopf aus wie ein freier.
    auto* bandRow = new QVBoxLayout;
    bandRow->setSpacing(6);
    m_rotorBand = new RotorBandWidget(this);
    m_rotorBand->setObjectName(QStringLiteral("rotorLogBand"));
    bandRow->addWidget(m_rotorBand);
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(6);
    auto* rotateBtn = new QPushButton(QStringLiteral("Rotate"), this);
    m_rotateBtn = rotateBtn;
    m_lpBtn = new QPushButton(QStringLiteral("LP"), this);
    m_lpBtn->setObjectName(QStringLiteral("rotorLogLongPath"));
    m_lpBtn->setToolTip(QStringLiteral("Turn to the long path of the station"));
    auto* stopBtn   = new QPushButton(QStringLiteral("Stop"), this);
    for (QPushButton* b : {rotateBtn, m_lpBtn, stopBtn}) {
        b->setStyleSheet(Style::buttonBaseStyle());
        btnRow->addWidget(b, b == stopBtn ? 0 : 1);
    }
    connect(m_lpBtn, &QPushButton::clicked, this, [this]() {
        if (m_kurzGrad < 0.0) { return; }
        m_dial->setTargetBearing(std::fmod(m_kurzGrad + 180.0, 360.0));
        beginTurn();
        aktualisiereDrehknoepfe();
    });
    connect(m_dial, &RotorDialWidget::targetPicked, this,
            [this](double) { aktualisiereDrehknoepfe(); });

    // ── Die zwei Knopfreihen sind gefallen (2026-08-21) ─────────────
    //
    // „under der world image you see die N, NE, E ... das gehoert weg."
    //
    // Hier standen acht Himmelsrichtungen und darunter vier gelernte
    // Ziele mit Park und Gegenkurs. Beides steckt jetzt im Rechtsklick
    // auf das Feld (contextMenuEvent) — samt Lernen und Umbenennen.
    //
    // Nichts ist dabei verlorengegangen, und das war der Punkt: eine
    // gelernte Peilung, die man nicht mehr aufrufen kann, waere
    // schlimmer als keine. Gezielt wird weiter mit einem Klick auf die
    // Rose; der Vertrag bleibt, dass Zielen den Mast nicht bewegt.



    // Rose oder Kugel: hinter dem Zahnrad (Entwurf R). Eine Handlung,
    // kein versteckter Knopf -- ein Knopf ohne Platz im Layout kann nie
    // erscheinen (tst_reachability_audit).
    m_globeBtn = new QAction(QStringLiteral("Show the globe"), this);
    m_globeBtn->setObjectName(QStringLiteral("rotorLogGlobeAction"));
    m_globeBtn->setCheckable(true);
    connect(m_globeBtn, &QAction::toggled, this, [this](bool on) {
        m_viewStack->setCurrentIndex(on ? 1 : 0);
        // Idle spin only while the globe is the thing being looked at —
        // motion in a hidden page is wasted, and in a visible corner it
        // is a distraction while operating.
        m_globe->setAutoRotate(false);
        if (on) { updateGlobeFromLocators(); }
    });
    bandRow->addLayout(btnRow);
    m_rowBtn = addShedRow(col, bandRow, this);

    // Report + log
    auto* rstRow = new QHBoxLayout;
    rstRow->setSpacing(6);
    rstRow->addWidget(caption(QStringLiteral("SENT"), this));
    m_rstSent = new QLineEdit(QStringLiteral("59"), this);
    m_rstSent->setFixedWidth(46);
    m_rstSent->setAlignment(Qt::AlignCenter);
    rstRow->addWidget(m_rstSent);
    rstRow->addWidget(caption(QStringLiteral("RCVD"), this));
    m_rstRcvd = new QLineEdit(QStringLiteral("59"), this);
    m_rstRcvd->setFixedWidth(46);
    m_rstRcvd->setAlignment(Qt::AlignCenter);
    rstRow->addWidget(m_rstRcvd);
    m_comment = new QLineEdit(this);
    m_comment->setPlaceholderText(QStringLiteral("comment"));
    rstRow->addWidget(m_comment, 1);
    m_rowRst = addShedRow(col, rstRow, this);

    for (QLineEdit* e : {m_myGrid, m_dxGrid, m_rstSent, m_rstRcvd, m_comment}) {
        e->setStyleSheet(Style::lineEditStyle());
    }
    // Betreiber 2026-09-01: "die Daten darunter koennen kleiner sein" --
    // RST/Kommentar treten hinter die groesser gewordene QRZ-Kopfzeile
    // zurueck. Gleiche Optik wie lineEditStyle(), nur eine Stufe der
    // Schriftleiter kleiner; MY/DX-Locator bleiben bei den 13px der
    // Vorlage, sie sind kein Teil dieser Anfrage.
    const QString smallLineEdit = QString(Style::lineEditStyle())
        .replace(QStringLiteral("13px"), QStringLiteral("11px"));
    for (QLineEdit* e : {m_rstSent, m_rstRcvd, m_comment}) {
        e->setStyleSheet(smallLineEdit);
    }

    auto* logRow = new QHBoxLayout;
    logRow->setSpacing(6);
    auto* logBtn = new QPushButton(QStringLiteral("Log QSO"), this);
    // Der Knopf, um den es im Feld geht: in Bernstein, nicht in Gruen
    // (Gruen heisst in Longpath „bestaetigt").
    logBtn->setStyleSheet(Style::buttonBaseStyle()
                          + QStringLiteral("QPushButton { color: %1; border-color: %2; }")
                                .arg(Style::kAmberText, Style::kAmberBorder));
    logBtn->setMinimumHeight(30);
    logRow->addWidget(logBtn, 1);
    // „Logbook…" steht hinter dem Zahnrad (Entwurf R).
    m_rowLog = addShedRow(col, logRow, this);

    m_status = new QLabel(QString{}, this);
    m_status->setWordWrap(true);
    m_status->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }").arg(Style::kTextSecondary));
    col->addWidget(m_status);

    // Recent contacts — the log is a file, and a file you cannot see is
    // a file you do not trust. Newest first.
    // Betreiber 2026-09-01: "unten steht der MODE, diese Spalte sollte
    // kleiner sein, die Frequenz waere hier wichtiger." Freq dazu, Mode
    // nicht mehr die letzte (gestreckte) Spalte -- Call streckt jetzt,
    // Mode faellt mit den anderen content-fit-Spalten schmal aus.
    m_recent = new QTableWidget(0, 5, this);
    m_recent->setHorizontalHeaderLabels({QStringLiteral("UTC"),
                                         QStringLiteral("Freq"),
                                         QStringLiteral("Call"),
                                         QStringLiteral("Band"),
                                         QStringLiteral("Mode")});
    m_recent->verticalHeader()->setVisible(false);
    m_recent->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_recent->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_recent->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_recent->setMaximumHeight(130);
    // Glas & Tiefe (2026-09-17): die Tabelle versenkt, der Kopf eine
    // Versalzeile ohne Kaesten (Style::tableStyle). Schriften per
    // setFont, nicht im Stylesheet — die Zahlen (UTC, Freq) in
    // Monospace, Regel 4.
    m_recent->setStyleSheet(Style::tableStyle());
    // Tabelle in der Textschrift (11 px); Monospace bekommen nur die
    // Zeit- und Frequenzspalte, je Zelle beim Fuellen — die ganze
    // Tabelle in Monospace war auf 286 Punkten zu breit, das
    // Rufzeichen (Stretch-Spalte) schrumpfte auf "O…".
    m_recent->setFont([this] { QFont f = font(); f.setPixelSize(Style::kFontSmall); return f; }());
    // Kopf 8 px mit Laufweite: bei 9 px plus .18em Laufweite wurden
    // die vier festen Spalten so breit, dass dem Rufzeichen (Stretch)
    // auf 286 Punkten nur "O…" blieb — am Blatt gesehen.
    m_recent->horizontalHeader()->setFont(Style::capsFont(font(), 8));
    m_recent->horizontalHeader()->setHighlightSections(false);
    m_recent->horizontalHeader()->setMinimumSectionSize(24);
    m_recent->setShowGrid(false);
    col->addWidget(m_recent);

    // ── Shrinking down to the compass (2026-08-10) ───────────────────
    //
    // Everything above competes for the same column, and a panel with
    // ten rows in it has a minimum height of all ten. Dragged narrow in
    // a dock, the dial was the thing that got squeezed — which is
    // backwards, because the dial is the one part that is useful at a
    // glance from across the room.
    //
    // So the rows are shed from the bottom of this list upwards as the
    // panel loses height, and the dial keeps whatever is left. Order is
    // least useful first. The recent-contacts table goes early because
    // it is by far the tallest and the logbook window shows the same
    // thing properly. The status line goes late because it is one line
    // and it is where the panel says what went wrong. The rotate and
    // stop buttons go last of all, and even then the dial itself can
    // still be double-clicked to turn.
    // The taught presets outrank the cardinal row: cardinals are an
    // orientation aid, the taught ones are the operator's actual
    // targets — so the cardinal row sheds first.
    m_shedOrder = {m_recent, m_rowLog, m_rowRst, m_rowCard,
                   m_rowGrid, m_rowCall, m_status,
                   m_rowBtn};
    m_shedHeights.resize(m_shedOrder.size());
    m_column = col;

    // ── Wiring ───────────────────────────────────────────────────────
    connect(m_callEdit, &QLineEdit::textChanged,
            this, &RotorLogbookPanel::onCallsignEdited);
    // Enter looks the station up, full stop (2026-08-10). It used to
    // also start the rotator turning, which meant confirming a callsign
    // moved the mast — an action nobody expects from the Enter key. The
    // turn stays on the Rotate button and the dial's double-click.
    connect(m_callEdit, &QLineEdit::returnPressed,
            this, &RotorLogbookPanel::onLookupRequested);
    connect(m_lookupBtn, &QPushButton::clicked,
            this, &RotorLogbookPanel::onLookupRequested);
    connect(m_myGrid, &QLineEdit::textChanged,
            this, &RotorLogbookPanel::applyLocators);
    connect(m_dxGrid, &QLineEdit::textChanged, this, [this]() {
        // Only a human typing here counts as manual. Writing a QRZ
        // answer into the field must not make the field look defended
        // against the next QRZ answer.
        if (!m_adoptingGrid) { m_dxGridIsManual = true; }
        applyLocators();
    });

    // Automatic QRZ lookup while typing. Debounced rather than
    // per-keystroke: OE, OE1, OE1W, OE1WY would otherwise be four
    // requests for one station, and the replies can land out of order.
    m_lookupTimer = new QTimer(this);
    m_lookupTimer->setSingleShot(true);
    m_lookupTimer->setInterval(kAutoLookupDelayMs);
    connect(m_lookupTimer, &QTimer::timeout,
            this, &RotorLogbookPanel::onLookupRequested);
    connect(logBtn, &QPushButton::clicked,
            this, &RotorLogbookPanel::onLogQso);

    // Stand-in movement for when no rotator is connected, so the dial
    // is demonstrable and the Turning / OnTarget states stay reachable.
    // A real rotator takes this timer's place entirely.
    m_simTimer = new QTimer(this);
    m_simTimer->setInterval(40);
    connect(m_simTimer, &QTimer::timeout, this, [this]() {
        const double togo = m_dial->travelDegrees();
        if (qAbs(togo) < 1.0) {
            m_simTimer->stop();
            m_dial->setState(RotorDialWidget::State::OnTarget);
            return;
        }
        m_dial->setActualBearing(m_dial->actualBearing()
                                 + (togo > 0 ? 1.0 : -1.0));
    });

    connect(rotateBtn, &QPushButton::clicked,
            this, &RotorLogbookPanel::beginTurn);
    connect(m_dial, &RotorDialWidget::rotateRequested, this,
            [this](double) { beginTurn(); });
    connect(stopBtn, &QPushButton::clicked,
            this, &RotorLogbookPanel::haltTurn);

    // ── Den Rotor beim Start wieder verbinden (2026-09-26) ───────────
    //
    // Betreiber: "man muss auch den aktuellen Stand des Rotors sehen!!!"
    // Bisher verband der Rotor nur ueber Rotor... -> Connect; nach jedem
    // Neustart gab es keinen Stand, bis man das wieder tat. Hatte der
    // Betreiber zuletzt verbunden (und nicht selbst getrennt), verbindet
    // Longpath jetzt von selbst -- nur lesen, nie drehen. Ist der Rotor
    // nicht erreichbar (anderes Netz, ausgeschaltet), steht das in der
    // Statuszeile, und Logbuch und Radar zeigen "not connected".
    //
    // Eine Sonde bleibt offline (2026-10-21): ein isolierter Lauf
    // startet hier sonst `rotctld` als eigenen Prozess und verbindet
    // sich mit dem Rotor des Betreibers -- gemessen. Siehe
    // AppSettings::darfVonSelbstVerbinden(). Von Hand (Rotor... ->
    // Connect) geht es in der Sonde weiter.
    if (AppSettings::darfVonSelbstVerbinden()
        && AppSettings::instance().value(kRotorConnectOnStartKey, QStringLiteral("False"))
            .toString() == QStringLiteral("True")) {
        QTimer::singleShot(1500, this, [this]() {
            QString err;
            if (connectRotorFromSettings(&err)) {
                setStatus(QStringLiteral("Connecting to the rotator…"));
            } else {
                setStatus(QStringLiteral("Rotator: %1").arg(err), true);
            }
        });
    }
}

bool RotorLogbookPanel::connectRotorFromSettings(QString* err)
{
    ensureRotor();
    if (m_rotor->isConnected()) { return true; }
    AppSettings& s = AppSettings::instance();

    // ── Der eigene Treiber braucht kein rotctld (2026-10-21) ─────────────
    //
    // ensureRotor() baut den Treiber nach `Rotor/Backend`, diese Funktion
    // wusste davon aber nichts und startete rotctld trotzdem. Am Geraet
    // sah das so aus: die App meldete „connected — GS-232 ueber
    // 172.30.30.16:4001" UND hatte daneben einen rotctld-Prozess auf
    // denselben Netzserver gelegt. Zwei Leser auf einer Leitung, und bei
    // jedem Start ein weiterer Port -- das ist die Herkunft der drei
    // rotctld-Leichen, die am selben Tag gefunden wurden.
    //
    // Hier frueh aussteigen: `connectToRotor()` weiss ueber
    // setNetworkTarget/setPort schon, wohin es greifen muss.
    if (s.value(QStringLiteral("Rotor/Backend"),
                QStringLiteral("rotctld")).toString()
            == QStringLiteral("gs232")) {
        m_rotor->connectToRotor();
        return true;
    }

    // Dieselbe Voreinstellung wie der Dialog (lokaler rotctld).
    const bool local = s.value(kRotorUseSerialKey, true).toBool();
    if (local) {
        const int model = s.value(kRotorModelKey, 601).toInt();
        const QString device = s.value(kRotorDeviceKey, QString{}).toString().trimmed();
        const int baud = s.value(kRotorBaudKey, 9600).toInt();
        if (device.isEmpty()) {
            if (err) { *err = QStringLiteral("no rotator device set up (Rotor…)"); }
            return false;
        }
        QString e;
        if (!m_rotorProc.start(model, device, baud, 4533, &e)) {
            if (err) { *err = e; }
            return false;
        }
        const quint16 port = m_rotorProc.listenPort();
        if (m_rotctld) { m_rotctld->setTarget(QStringLiteral("127.0.0.1"), port); }
        s.setValue(kRotorHostKey, QStringLiteral("127.0.0.1"));
        s.setValue(kRotorPortKey, port);
    } else {
        const QString host = s.value(kRotorHostKey, QString{}).toString().trimmed();
        const quint16 port = static_cast<quint16>(s.value(kRotorPortKey, 4533).toUInt());
        if (host.isEmpty()) {
            if (err) { *err = QStringLiteral("no rotctld address set up (Rotor…)"); }
            return false;
        }
        if (m_rotctld) { m_rotctld->setTarget(host, port ? port : 4533); }
    }
    m_rotor->connectToRotor();
    return true;
}

// ── Shrinking down to the compass ───────────────────────────────────

void RotorLogbookPanel::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);
    updateCompactness();
}

void RotorLogbookPanel::updateCompactness()
{
    if (m_shedOrder.isEmpty() || !m_column) { return; }

    // Remember how tall each row is while it is up. A hidden widget's
    // sizeHint is still valid in Qt, but a row that has never been laid
    // out reports its hint before its children have theirs, so the
    // first honest measurement is cached and reused.
    for (int i = 0; i < m_shedOrder.size(); ++i) {
        QWidget* w = m_shedOrder.at(i);
        if (!w || !w->isVisible()) { continue; }
        const int h = w->sizeHint().height();
        if (h > 0) { m_shedHeights[i] = h; }
    }

    const QMargins m = m_column->contentsMargins();
    const int spacing = m_column->spacing();
    int avail = height() - m.top() - m.bottom();

    // What the dial insists on keeping. Below this there is no compass
    // left to protect and shedding another row buys nothing.
    const int dialFloor =
        m_viewStack ? m_viewStack->minimumSizeHint().height() : 96;

    int needed = dialFloor;
    for (int i = 0; i < m_shedOrder.size(); ++i) {
        if (m_shedHeights.at(i) > 0) {
            needed += m_shedHeights.at(i) + spacing;
        }
    }

    // Shed from the front of the list — least useful first — until what
    // is left fits. Purely a function of the height we were handed, so
    // it cannot oscillate: hiding a row does not change `avail`.
    int shed = 0;
    while (needed > avail && shed < m_shedOrder.size()) {
        if (m_shedHeights.at(shed) > 0) {
            needed -= m_shedHeights.at(shed) + spacing;
        }
        ++shed;
    }

    for (int i = 0; i < m_shedOrder.size(); ++i) {
        QWidget* w = m_shedOrder.at(i);
        if (!w) { continue; }
        const bool show = i >= shed;
        if (w->isVisible() != show) { w->setVisible(show); }
    }

    // With the buttons gone there is nothing left saying how to turn,
    // so the dial has to say it itself.
    if (m_dial) {
        m_dial->setHint(shed >= m_shedOrder.size()
            ? QStringLiteral("Click to aim, double-click to turn. Drag the "
                             "panel taller to get the controls back.")
            : QString{});
    }
}

// ── Rotator ─────────────────────────────────────────────────────────

// ── Teachable presets + park (2026-08-11) ───────────────────────────



bool RotorLogbookPanel::teachableBearing(double* outDeg) const
{
    if (m_dial && m_dial->hasTarget()) {
        *outDeg = m_dial->targetBearing();
        return true;
    }
    // No aim set: fall back to where the mast actually points — but
    // only a FRESH reading. Teaching from a stale needle stores a lie.
    if (m_rotor && m_rotor->isConnected() && m_rotor->hasFreshPosition()) {
        *outDeg = m_rotor->azimuth();
        return true;
    }
    return false;
}

// ── Zielen ohne Knopfreihen ──────────────────────────────────────────
//
// Ersetzt die beiden Reihen, die am 2026-08-21 gefallen sind: die
// Himmelsrichtungen und die gelernten Ziele samt Park und Gegenkurs.
//
// Warum ein Menue am FELD und nicht an der Rose: das Feld zeigt
// wahlweise Rose oder Globus. Haenge ich das Menue an die Rose, ist es
// im Globus-Bild unerreichbar — und genau dort stand der Betreiber, als
// er die Knoepfe wegwollte.
//
// Der Vertrag bleibt, was er war: Zielen bewegt den Mast NICHT. Erst
// Rotate dreht. Das galt fuer die Knoepfe und gilt hier weiter.
void RotorLogbookPanel::contextMenuEvent(QContextMenuEvent* ev)
{
    ScopedChildWidget<QMenu> menuOwner(window());   // stirbt nicht mit dem Rahmen, siehe ScopedChildWidget.h
    const QPointer<RotorLogbookPanel> self(this);   // Menue am Fenster: am nativen Feld bekam es keinen Fensterbezug (2026-09-27)
    QMenu& menu = *menuOwner.get();
    buildAimMenu(menu, ev->globalPos());
    menu.exec(ev->globalPos());
    // Das Elternteil kann waehrend exec() gestorben sein — dann ist
    // auch das Menue weg und `this` eine Leiche. Siehe ScopedChildWidget.h.
    if (!menuOwner || !self) { return; }
    ev->accept();
}

// Getrennt, damit eine Pruefung das Menue BAUEN kann, ohne es
// aufzuklappen. menu.exec() haelt die Runde an — eine Pruefung, die
// den Rechtsklick nachstellt, wuerde haengen. Genau davor warnt der
// Erreichbarkeits-Durchgang (tst_reachability_audit), und hier ist der
// Fall, auf den er zutrifft.
void RotorLogbookPanel::buildAimMenu(QMenu& menu, const QPoint& g)
{

    QMenu* points = menu.addMenu(tr("Peilen"));
    struct P { const char* label; double deg; };
    static constexpr P kPoints[] = {
        {"N", 0}, {"NE", 45}, {"E", 90}, {"SE", 135},
        {"S", 180}, {"SW", 225}, {"W", 270}, {"NW", 315},
    };
    for (const P& p : kPoints) {
        QAction* a = points->addAction(
            QStringLiteral("%1 · %2°")
                .arg(QString::fromLatin1(p.label))
                .arg(p.deg, 0, 'f', 0));
        const double deg = p.deg;
        connect(a, &QAction::triggered, this, [this, deg]() {
            if (m_dial) { m_dial->setTargetBearing(deg); }
        });
    }

    menu.addSeparator();

    AppSettings& st = AppSettings::instance();

    for (int i = 0; i < kUserPresetSlots; ++i) {
        const QString deg =
            st.value(kPresetDegKey.arg(i + 1), QString{}).toString();
        const QString label =
            st.value(kPresetLabelKey.arg(i + 1), QString{}).toString();

        QMenu* sub = menu.addMenu(
            label.isEmpty() ? tr("Target %1 (empty)").arg(i + 1)
                            : QStringLiteral("%1 · %2°")
                                  .arg(label, deg.isEmpty() ? QStringLiteral("—")
                                                            : deg));
        QAction* aim = sub->addAction(tr("Hierhin zielen"));
        aim->setEnabled(!deg.isEmpty());
        connect(aim, &QAction::triggered, this, [this, deg]() {
            if (m_dial && !deg.isEmpty()) {
                m_dial->setTargetBearing(deg.toDouble());
            }
        });
        QAction* teach = sub->addAction(tr("Teach, rename…"));
        connect(teach, &QAction::triggered, this, [this, i, g]() {
            userPresetMenu(i, g);
        });
    }

    menu.addSeparator();

    const QString park = st.value(kParkDegKey, QString{}).toString();
    QMenu* parkSub = menu.addMenu(
        park.isEmpty() ? tr("Park (not taught)")
                       : tr("Park · %1°").arg(park));
    QAction* parkAim = parkSub->addAction(tr("Park anzielen"));
    parkAim->setEnabled(!park.isEmpty());
    connect(parkAim, &QAction::triggered, this, [this, park]() {
        if (m_dial && !park.isEmpty()) {
            m_dial->setTargetBearing(park.toDouble());
        }
    });
    QAction* parkTeach = parkSub->addAction(tr("Park lernen…"));
    connect(parkTeach, &QAction::triggered, this, [this, g]() {
        parkMenu(g);
    });

    QAction* lp = menu.addAction(tr("Long path (±180°)"));
    lp->setEnabled(m_dial && m_dial->hasTarget());
    lp->setToolTip(tr(
        "Turns the BEARING to the long path. Aim first, then reverse "
         "course, then Rotate."));
    connect(lp, &QAction::triggered, this, [this]() {
        if (!m_dial || !m_dial->hasTarget()) { return; }
        m_dial->setTargetBearing(
            std::fmod(m_dial->targetBearing() + 180.0, 360.0));
    });

    menu.addSeparator();
    QAction* setup = menu.addAction(tr("Rotor settings…"));
    connect(setup, &QAction::triggered,
            this, &RotorLogbookPanel::openRotorSetupDialog);

}

// ── Die Ziel-Menues haengen nicht mehr an Knoepfen ──────────────────
//
// Am 2026-08-21 sind die zwei Knopfreihen im Rotor-Feld gefallen
// („under der world image you see die N, NE, E ... das gehoert weg").
// Die gespeicherten Ziele UND das Lernen daran duerfen dabei nicht
// verschwinden: eine gelernte Peilung, die man nicht mehr aufrufen
// kann, ist schlimmer als keine.
//
// Deshalb nehmen beide Menues jetzt eine Bildschirmposition statt
// einer Position IM KNOPF, und der Waechter auf den Knopf faellt weg.
// Aufgerufen werden sie aus dem Rechtsklick auf das Feld — das
// funktioniert auch ueber dem Globus, wo es nie eine Rose gab.
void RotorLogbookPanel::userPresetMenu(int slot, const QPoint& globalPos)
{
    if (slot < 0 || slot >= kUserPresetSlots) { return; }
    AppSettings& s = AppSettings::instance();
    const QString degKey   = kPresetDegKey.arg(slot + 1);
    const QString labelKey = kPresetLabelKey.arg(slot + 1);
    const bool taught = !s.value(degKey, QString{}).toString().isEmpty();

    ScopedChildWidget<QMenu> menuOwner(window());   // stirbt nicht mit dem Rahmen, siehe ScopedChildWidget.h
    const QPointer<RotorLogbookPanel> self(this);   // Menue am Fenster: am nativen Feld bekam es keinen Fensterbezug (2026-09-27)
    QMenu& menu = *menuOwner.get();
    double teach = 0.0;
    const bool canTeach = teachableBearing(&teach);
    QAction* save = menu.addAction(canTeach
        ? QStringLiteral("Store current aim here (%1°)")
              .arg(teach, 0, 'f', 0)
        : QStringLiteral("Store current aim here — aim something first"));
    save->setEnabled(canTeach);
    QAction* rename = taught
        ? menu.addAction(QStringLiteral("Rename…")) : nullptr;
    QAction* clear = taught
        ? menu.addAction(QStringLiteral("Clear")) : nullptr;

    QAction* chosen =
        menu.exec(globalPos);
        // Das Elternteil kann waehrend exec() gestorben sein — dann ist
        // auch das Menue weg und `this` eine Leiche. Siehe ScopedChildWidget.h.
        if (!menuOwner || !self) { return; }
    if (!chosen) { return; }

    if (chosen == save) {
        s.setValue(degKey, QString::number(teach, 'f', 1));
        // First teach without a name: ask for one right away — a row of
        // bare numbers is exactly the unhelpfulness this row replaces.
        bool ok = false;
        const QString name = QInputDialog::getText(
            this, QStringLiteral("Preset name"),
            QStringLiteral("Name for %1° (blank keeps the number):")
                .arg(teach, 0, 'f', 0),
            QLineEdit::Normal,
            s.value(labelKey, QString{}).toString(), &ok);
        if (ok) { s.setValue(labelKey, name.trimmed()); }
        s.save();
    } else if (rename && chosen == rename) {
        bool ok = false;
        const QString name = QInputDialog::getText(
            this, QStringLiteral("Preset name"),
            QStringLiteral("New name:"), QLineEdit::Normal,
            s.value(labelKey, QString{}).toString(), &ok);
        if (ok) {
            s.setValue(labelKey, name.trimmed());
            s.save();
        }
    } else if (clear && chosen == clear) {
        s.setValue(degKey, QString{});
        s.setValue(labelKey, QString{});
        s.save();
    }
}

void RotorLogbookPanel::parkMenu(const QPoint& globalPos)
{
    AppSettings& s = AppSettings::instance();
    const bool taught = !s.value(kParkDegKey, QString{}).toString().isEmpty();

    ScopedChildWidget<QMenu> menuOwner(window());   // stirbt nicht mit dem Rahmen, siehe ScopedChildWidget.h
    const QPointer<RotorLogbookPanel> self(this);   // Menue am Fenster: am nativen Feld bekam es keinen Fensterbezug (2026-09-27)
    QMenu& menu = *menuOwner.get();
    double teach = 0.0;
    const bool canTeach = teachableBearing(&teach);
    QAction* save = menu.addAction(canTeach
        ? QStringLiteral("Store current aim as park (%1°)")
              .arg(teach, 0, 'f', 0)
        : QStringLiteral("Store current aim as park — aim something "
                         "first"));
    save->setEnabled(canTeach);
    QAction* clear = taught
        ? menu.addAction(QStringLiteral("Clear park position")) : nullptr;

    QAction* chosen = menu.exec(globalPos);
    // Das Elternteil kann waehrend exec() gestorben sein — dann ist
    // auch das Menue weg und `this` eine Leiche. Siehe ScopedChildWidget.h.
    if (!menuOwner || !self) { return; }
    if (!chosen) { return; }
    if (chosen == save) {
        s.setValue(kParkDegKey, QString::number(teach, 'f', 1));
        s.save();
    } else if (clear && chosen == clear) {
        s.setValue(kParkDegKey, QString{});
        s.save();
    }
}

void RotorLogbookPanel::beginTurn()
{
    if (!m_dial->hasTarget()) { return; }
    const double bearing = m_dial->targetBearing();

    // Swing the globe round to the heading as the mast turns: the point
    // of that view is watching which way the path goes.
    m_globe->lookAlongBearing(bearing);
    m_dial->setState(RotorDialWidget::State::Turning);

    if (m_rotor && m_rotor->isConnected()) {
        m_dial->setSimulated(false);
        // Erlaubter Bereich (2026-11-08): eine Ablehnung soll nicht als
        // "dreht" stehen bleiben. Den Grund meldet moveTo() selbst.
        if (!m_rotor->zielErlaubt(bearing)) {
            m_dial->clearTarget();
            m_dial->setState(RotorDialWidget::State::Idle);
        }
        m_rotor->moveTo(bearing);
        return;
    }
    // No rotator. Say so every time rather than once at startup — the
    // needle moving is otherwise indistinguishable from the mast
    // moving, and that is a mistake an operator makes exactly once
    // before they stop trusting the display.
    //
    // The status line is not enough on its own: it scrolls away, and
    // what is left afterwards is a dial that looks like a reading. So
    // the needle itself goes dashed and dim and the rose is labelled
    // for as long as the reading is invented. (2026-08-10)
    m_dial->setSimulated(true);
    setStatus(QStringLiteral("No rotator connected — needle simulated"), true);
    m_simTimer->start();
}

void RotorLogbookPanel::haltTurn()
{
    m_simTimer->stop();
    if (m_rotor && m_rotor->isConnected()) { m_rotor->stop(); }
    m_dial->setState(RotorDialWidget::State::Targeted);
}

// Den Treiber wechseln, waehrend das Fenster offen ist.
//
// ensureRotor() legt den Rotor EINMAL an; die Wahl im Dialog kann ihn
// danach aendern. Statt das ganze Fenster neu zu bauen wird hier der
// alte abgebaut und ensureRotor() noch einmal gerufen -- die
// Verdrahtung entsteht dort an einer Stelle, nicht an zweien.
//
// Qt loest die Verbindungen des alten Objekts beim Loeschen selbst;
// was der Dialog an SEINE Anzeige gehaengt hat, haengt danach ins
// Leere und wird vom Rufer neu gesetzt.
void RotorLogbookPanel::rebuildRotorBackend()
{
    if (m_rotor) {
        m_rotor->disconnectFromRotor();
        if (m_radio) { m_radio->setRotor(nullptr); }
        m_rotor->deleteLater();
        m_rotor   = nullptr;
        m_rotctld = nullptr;
    }
    ensureRotor();
}

void RotorLogbookPanel::ensureRotor()
{
    if (m_rotor) { return; }

    // Welcher Weg zum Rotor? Vorgabe bleibt rotctld — wer nichts
    // einstellt, merkt von dieser Weiche nichts. `gs232` faehrt den
    // Yaesu GS-232A/B direkt an der seriellen Schnittstelle, ohne
    // Hamlib dazwischen (Gs232Rotor.h).
    {
        AppSettings& sb = AppSettings::instance();
        const QString weg = sb.value(QStringLiteral("Rotor/Backend"),
                                     QStringLiteral("rotctld"))
                                .toString().trimmed().toLower();
        if (weg == QStringLiteral("gs232")) {
            auto* gs232 = new Gs232Rotor(this);
            gs232->setPort(
                sb.value(QStringLiteral("Rotor/SerialPort"), QString{}).toString(),
                sb.value(QStringLiteral("Rotor/BaudRate"), 9600).toInt());
            // Ein Netzziel schlaegt die serielle Schnittstelle. An dieser
            // Station ist das der Normalfall und nicht die Ausnahme: der
            // GS-232 haengt auf einem seriellen Netzserver, nicht an
            // /dev/tty* (gemessen 2026-10-21, siehe Gs232Rotor.h).
            const QString host =
                sb.value(QStringLiteral("Rotor/Host"), QString{}).toString().trimmed();
            QString netzHost;
            quint16 netzPort = 0;
            if (!host.isEmpty()) {
                gs232->setNetworkTarget(
                    host,
                    static_cast<quint16>(
                        sb.value(QStringLiteral("Rotor/NetPort"), 4001).toInt()));
            } else if (Gs232Rotor::parseNetworkTarget(gs232->portName(),
                                                      &netzHost, &netzPort)) {
                // 2026-11-11: „172.30.30.16:4001" als Schnittstelle ist
                // ein Netzziel, keine Datei (siehe parseNetworkTarget).
                gs232->setNetworkTarget(netzHost, netzPort);
            }
            m_rotor   = gs232;
            m_rotctld = nullptr;
        }
    }

    if (!m_rotor) {
        m_rotctld = new RotctldClient(this);
        m_rotor   = m_rotctld;
    }
    // Beim Modell anmelden, damit der TCI-Server ihn erreicht, ohne in
    // dieses Fenster zu greifen (RadioModel::rotor()). Angelegt und
    // bedient wird er weiter hier; das Modell haelt nur einen QPointer.
    if (m_radio) { m_radio->setRotor(m_rotor); }

    AppSettings& s = AppSettings::instance();
    if (m_rotctld) {
        m_rotctld->setTarget(
            s.value(kRotorHostKey, QString{}).toString(),
            static_cast<quint16>(s.value(kRotorPortKey, 4533).toInt()));
    }

    // The rotator's own reading drives the second needle. Nothing else
    // may write it while a rotator is connected, or the dial would show
    // where the software thinks the mast is rather than where it is.
    connect(m_rotor, &RotorController::positionChanged, this,
            [this](double az) {
        // A real reading has arrived, so the needle stops being a
        // pretence — and the stand-in timer must stop too, or it would
        // keep nudging the needle away from what the mast reports.
        m_simTimer->stop();
        m_dial->setSimulated(false);
        m_dial->setActualBearing(az);
        if (m_rotorBand) { m_rotorBand->setStand(az); }
        // Das Logbuch zeigt ihn auch -- nur echte Ablesungen, nie die
        // Stellvertreter-Nadel (2026-09-26).
        m_lastRotorAz = az;
        if (m_logWindow) { m_logWindow->setRotorBearing(az); }
    });

    // Az/el rotators: the reported elevation appears on the dial. The
    // signal only fires when the value actually changes, so an
    // azimuth-only rotator never triggers the readout.
    if (m_rotctld) {
        connect(m_rotctld, &RotctldClient::elevationChanged, this,
                [this](double el) { m_dial->setElevation(el); });
    } else if (auto* seriell = qobject_cast<Gs232Rotor*>(m_rotor)) {
        // Der serielle Treiber meldet die Hoehe mit demselben Signalnamen,
        // aber aus seiner eigenen Klasse.
        connect(seriell, &Gs232Rotor::elevationChanged, this,
                [this](double el) { m_dial->setElevation(el); });
    }

    connect(m_rotor, &RotorController::stateChanged, this,
            [this](RotorController::State st) {
        switch (st) {
        case RotorController::State::Idle:
            m_dial->setState(m_dial->hasTarget()
                                 ? RotorDialWidget::State::OnTarget
                                 : RotorDialWidget::State::Idle);
            setStatus(QStringLiteral("Rotator connected — %1")
                          .arg(m_rotor->description()));
            break;
        case RotorController::State::Moving:
            m_dial->setState(RotorDialWidget::State::Turning);
            break;
        case RotorController::State::Connecting:
            setStatus(QStringLiteral("Connecting to %1…")
                          .arg(m_rotor->description()));
            break;
        case RotorController::State::Disconnected:
            m_lastRotorAz = std::numeric_limits<double>::quiet_NaN();
            if (m_logWindow) { m_logWindow->setRotorBearing(m_lastRotorAz); }
            setStatus(QStringLiteral("Rotator disconnected"), true);
            break;
        case RotorController::State::Error:
            break;   // errorOccurred carries the detail
        }
    });

    connect(m_rotor, &RotorController::errorOccurred, this,
            [this](const QString& msg) {
        setStatus(QStringLiteral("Rotator: %1").arg(msg), true);
        // Nach Rotate/LP in der Eingabezeile des Logbuchs steht es auch
        // dort -- wer dort gedrueckt hat, schaut dort hin (2026-11-11).
        if (m_logWindow) { m_logWindow->setRotorMeldung(msg); }
    });

    // 2026-09-16, found live against a microHAM ARCO on the LAN: the
    // ARCO closes a GS-232A TCP session after ~20 s without traffic.
    // rotctld opens that session the moment it starts, and if nothing
    // polls it in time (the first client connect was refused because
    // rotctld had not bound its port yet — see RotctldClient), the
    // session dies underneath rotctld, which then hangs on every
    // command for good. Reconnecting the client to a hung daemon just
    // repeats the watchdog cut every few seconds. The daemon is ours,
    // so bounce it; the client's own retry then finds the fresh one.
    if (m_rotctld) {
        connect(m_rotctld, &RotctldClient::replyTimedOut, this, [this]() {
        if (!m_rotorProc.isRunning()) { return; }
        QString err;
        if (!m_rotorProc.restart(&err)) {
            setStatus(QStringLiteral("Rotator: rotctld restart failed — %1")
                          .arg(err), true);
            return;
        }
        m_rotctld->setTarget(QStringLiteral("127.0.0.1"),
                             m_rotorProc.listenPort());
        setStatus(QStringLiteral("Rotator stopped answering — "
                                 "restarted rotctld"), true);
        });
    }

    // rotctld quitting by itself was never surfaced: Hamlib's reason
    // (bad model number, device not there, port taken) went to a
    // stderr nobody read, and the client kept retrying a port with
    // nothing behind it.
    connect(&m_rotorProc, &RotctldProcess::exited, this,
            [this](int code, const QString& stderrText) {
        m_rotor->disconnectFromRotor();
        // Nicht die letzte Zeile: Hamlib schliesst mit „IO error" ab, und
        // genau das stand am 2026-10-06 in Martins Protokoll -- waehrend
        // die Zeile darueber Adresse UND Ursache nannte
        // („connect to ...:4001 failed ... Operation timed out"). Wer
        // nichts erfaehrt, sucht beim Programm statt beim Kabel.
        const QString why = stderrText.isEmpty()
            ? QStringLiteral("exit code %1").arg(code)
            : RotctldProcess::grundAus(stderrText);
        setStatus(QStringLiteral("Rotator: rotctld exited — %1").arg(why),
                  true);
    });
}

void RotorLogbookPanel::showRotorSetup()
{
    openRotorSetupDialog();
}

// 2026-08-10: entry point for "Turn rotor to <call>" from the Spot Hub.
// Everything after setText is the panel's normal typing pipeline —
// onCallsignEdited resolves the prefix and sets the dial target when an
// own locator is present; a QRZ answer refines it later. If a bearing
// came out of that, the turn starts at once; if not (no own locator),
// the status line already says what is missing, which beats turning to
// a bearing that does not exist.
void RotorLogbookPanel::workSpot(const QString& call)
{
    const QString c = call.trimmed().toUpper();
    if (c.isEmpty()) { return; }
    m_callEdit->setText(c);
    if (m_dial->hasTarget()) { beginTurn(); }
}

void RotorLogbookPanel::takeSpot(const QString& call)
{
    const QString c = call.trimmed().toUpper();
    if (c.isEmpty()) { return; }

    // Das Setzen des Feldes zieht alles nach: onCallsignEdited holt Land
    // und Flagge aus cty.dat, stellt den Zeiger auf die Zielpeilung der
    // DXCC-Einheit und schreibt die Entfernung in die Statuszeile — ohne
    // Netz. Kommt die QRZ-Antwort, ersetzt der genaue Locator die
    // Schaetzung aus dem Prefix.
    m_callEdit->setText(c);
    m_callEdit->setFocus();

    // Das Logbuchfenster bekommt dasselbe Rufzeichen (2026-11-12,
    // Betreiber: „sollte auch im logbuch sofort das call übernehmen und
    // alle daten anzeigen"). Ist es nicht offen, steht es beim Oeffnen da.
    if (m_logWindow) { m_logWindow->uebernimmRufzeichen(c); }

    // Kein beginTurn(): siehe Begruendung am Kopf der Deklaration. Wer
    // drehen will, hat den Rotate-Knopf daneben und sieht vorher, wohin.
    if (!m_dial->hasTarget()) {
        setStatus(QStringLiteral("%1 — no bearing yet, waiting for a locator")
                      .arg(c));
    }
}

// 2026-11-18: dem FT8-Partner folgen. Begruendung der drei Unterschiede
// zu takeSpot() am Kopf der Deklaration.
void RotorLogbookPanel::folgeFt8Partner(const QString& call)
{
    QString c = call.trimmed().toUpper();
    // Spitze Klammern sagen „kam als Hash" -- im Rufzeichenfeld sind sie
    // nur im Weg (QRZ kennt sie nicht, die Pruefung lehnt sie ab).
    c.remove(QLatin1Char('<')).remove(QLatin1Char('>'));
    if (c.isEmpty()) { return; }

    const QString steht = m_callEdit->text().trimmed().toUpper();
    const bool frei = steht.isEmpty()
                      || (!m_ft8Partner.isEmpty() && steht == m_ft8Partner);
    if (!frei) { return; }          // getippt oder per Spot: bleibt
    m_ft8Partner = c;
    if (steht == c) { return; }

    // Was zum VORIGEN Partner gehoerte, geht mit ihm. onCallsignEdited
    // raeumt nur bei leerem Feld auf; ein Rufzeichen, das ein anderes
    // ersetzt, liesse sonst Foto und Locator des alten stehen -- und
    // fehlt der neue bei QRZ, stuende das fremde Gesicht neben ihm.
    // Ein von Hand getippter Locator bleibt (wie in adoptGridFromQrz).
    // Die Stationszeile ebenso: onCallsignEdited ueberschreibt sie nur,
    // wenn cty.dat das Prefix aufloest -- sonst stuende der Name des
    // vorigen Partners neben dem neuen (im Pruefstand gefunden).
    m_lastInfo = CallsignInfo{};
    m_flagEmoji.clear();
    m_stationLine->clear();
    m_photo->showPlaceholder(QString{});
    m_photo->setVisible(false);
    if (!m_dxGridIsManual) {
        m_adoptingGrid = true;
        m_dxGrid->clear();
        m_adoptingGrid = false;
    }

    // Derselbe Weg wie das Tippen: Land, Flagge, Peilung aus cty.dat,
    // dann QRZ mit Schalter, Entprellung und Cache. Kein setFocus(),
    // kein beginTurn().
    m_callEdit->setText(c);
}

void RotorLogbookPanel::openRotorSetupDialog()
{
    ensureRotor();

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Rotator"));

    auto* col = new QVBoxLayout(&dlg);
    col->setContentsMargins(14, 14, 14, 14);
    col->setSpacing(10);

    AppSettings& s = AppSettings::instance();

    // Two ways in. Most operators want the first and should not have to
    // know the second exists; anyone already running rotctld their own
    // way, or with the controller on another machine, needs the second
    // and would be blocked without it.
    // Zwei Fragen nacheinander statt einer Liste mit allem drin: erst
    // WER faehrt, dann WIE er angeschlossen ist. Entscheidung des
    // Betreibers am Entwurfsblatt (2026-10-21, Fassung B von drei) --
    // so steht immer nur da, was gilt, statt eines dauerhaft grauen
    // Feldes daneben.
    auto* driverBox = new QComboBox(&dlg);
    driverBox->addItem(QStringLiteral("Hamlib (rotctld) — every rotator Hamlib knows"));
    driverBox->addItem(QStringLiteral("GS-232 direct — Yaesu GS-232A/B, no Hamlib"));
    driverBox->setCurrentIndex(
        s.value(QStringLiteral("Rotor/Backend"), QStringLiteral("rotctld"))
            .toString().trimmed().toLower() == QStringLiteral("gs232") ? 1 : 0);
    col->addWidget(driverBox);

    // ── Wie er angeschlossen ist, beim GS-232 aus dessen eigenen
    // Schluesseln (2026-11-11) ──────────────────────────────────────
    //
    // `RotorUseLocalSerial` gehoert zum rotctld-Weg. Martins ARCO laeuft
    // seit 2026-10-21 ueber den direkten GS-232-Treiber am Netzserver
    // (Rotor/Host = 172.30.30.16), der alte Schluessel stand aber noch
    // auf „lokal". Der Dialog zeigte darum „Serial port on this
    // computer", und „Connect" loeschte Rotor/Host und schrieb die
    // Adresse als Schnittstelle -- danach „No such file or directory".
    // Beim GS-232 sagt Rotor/Host (oder ein host:port in der
    // Schnittstelle), ob er am Netz haengt.
    const bool gs232Gespeichert =
        s.value(QStringLiteral("Rotor/Backend"), QStringLiteral("rotctld"))
            .toString().trimmed().toLower() == QStringLiteral("gs232");
    QString gs232Host =
        s.value(QStringLiteral("Rotor/Host"), QString{}).toString().trimmed();
    quint16 gs232Port = static_cast<quint16>(
        s.value(QStringLiteral("Rotor/NetPort"), 4001).toInt());
    if (gs232Gespeichert && gs232Host.isEmpty()) {
        Gs232Rotor::parseNetworkTarget(
            s.value(QStringLiteral("Rotor/SerialPort"), QString{}).toString(),
            &gs232Host, &gs232Port);
    }
    const bool gs232AmNetz = gs232Gespeichert && !gs232Host.isEmpty();

    auto* modeBox = new QComboBox(&dlg);
    modeBox->setObjectName(QStringLiteral("rotorDialogMode"));
    modeBox->addItem(QStringLiteral("Controller on this computer"));
    modeBox->addItem(QStringLiteral("rotctld already running (network)"));
    modeBox->setCurrentIndex(
        gs232Gespeichert ? (gs232AmNetz ? 1 : 0)
                         : (s.value(kRotorUseSerialKey, true).toBool() ? 0 : 1));
    col->addWidget(modeBox);

    // Mechanical end stop (2026-08-10). The dial computes its travel
    // path around this, so the sector it draws is the way the mast will
    // actually turn instead of the way a free compass would.
    auto* stopRow = new QHBoxLayout;
    auto* stopLabel = new QLabel(QStringLiteral("End stop"), &dlg);
    stopLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }")
        .arg(QString::fromLatin1(Style::kTextSecondary)));
    stopRow->addWidget(stopLabel);
    auto* stopCombo = new QComboBox(&dlg);
    stopCombo->addItem(QStringLiteral("none — turns freely"), -1.0);
    stopCombo->addItem(QStringLiteral("North (0°)"), 0.0);
    stopCombo->addItem(QStringLiteral("East (90°)"), 90.0);
    stopCombo->addItem(QStringLiteral("South (180°)"), 180.0);
    stopCombo->addItem(QStringLiteral("West (270°)"), 270.0);
    {
        const double savedStop =
            s.value(kRotorEndStopKey, -1.0).toDouble();
        int at = 0;
        for (int i = 0; i < stopCombo->count(); ++i) {
            if (qFuzzyCompare(stopCombo->itemData(i).toDouble() + 1.0,
                              savedStop + 1.0)) { at = i; break; }
        }
        stopCombo->setCurrentIndex(at);
    }
    connect(stopCombo, &QComboBox::currentIndexChanged, &dlg,
            [this, stopCombo](int) {
        const double deg = stopCombo->currentData().toDouble();
        AppSettings::instance().setValue(kRotorEndStopKey, deg);
        m_dial->setEndStop(deg);
    });
    stopRow->addWidget(stopCombo, 1);
    col->addLayout(stopRow);

    // ── Erlaubter Bereich (2026-11-08) ───────────────────────────────
    //
    // Betreiber: „zu hause ... soll der rotor nicht 360 drehen". Gilt fuer
    // jeden Drehbefehl aus Longpath (RotorController::pruefeZiel) und
    // rechnet mit dem Anschlag darueber: geprueft wird der WEG, nicht nur
    // das Ziel (core/RotorBereich.h).
    {
        auto* limitRow = new QHBoxLayout;
        auto* limitOn = new QCheckBox(QStringLiteral("Limit rotation to"), &dlg);
        limitOn->setObjectName(QStringLiteral("rotorLimitOn"));
        auto* vonSpin = new QSpinBox(&dlg);
        auto* bisSpin = new QSpinBox(&dlg);
        for (QSpinBox* sp : {vonSpin, bisSpin}) {
            sp->setRange(0, 359);
            sp->setWrapping(true);
            sp->setSuffix(QStringLiteral("°"));
            sp->setStyleSheet(Style::doubleSpinBoxStyle());
        }
        vonSpin->setObjectName(QStringLiteral("rotorLimitFrom"));
        bisSpin->setObjectName(QStringLiteral("rotorLimitTo"));
        auto* bisLabel = new QLabel(QStringLiteral("to"), &dlg);
        auto* uhrLabel = new QLabel(QStringLiteral("clockwise"), &dlg);
        limitRow->addWidget(limitOn);
        limitRow->addWidget(vonSpin);
        limitRow->addWidget(bisLabel);
        limitRow->addWidget(bisSpin);
        limitRow->addWidget(uhrLabel);
        limitRow->addStretch(1);
        col->addLayout(limitRow);
        auto* limitNote = new QLabel(&dlg);
        limitNote->setObjectName(QStringLiteral("rotorLimitNote"));
        limitNote->setWordWrap(true);
        limitNote->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
        col->addWidget(limitNote);

        const RotorBereich b = RotorBereich::load();
        limitOn->setChecked(b.an);
        vonSpin->setValue(qRound(b.vonGrad) % 360);
        bisSpin->setValue(qRound(b.bisGrad) % 360);

        auto zeige = [limitOn, vonSpin, bisSpin, limitNote]() {
            const RotorBereich r = RotorBereich::load();
            vonSpin->setEnabled(limitOn->isChecked());
            bisSpin->setEnabled(limitOn->isChecked());
            QString t = QStringLiteral(
                "Applies to every turn Longpath commands, including the way "
                "there: %1.").arg(r.beschreibung());
            double anfang = 0.0, spanne = 0.0;
            if (r.an && r.anschlagGrad >= 0.0 && r.reichweite(r.vonGrad, &anfang, &spanne)
                && spanne + 1.0 < std::fmod(r.bisGrad - r.vonGrad + 360.0, 360.0)) {
                t += QStringLiteral(" The end stop lies inside this range, so the "
                                    "rotator cannot pass from one side of it to the "
                                    "other.");
            }
            limitNote->setText(t);
        };
        auto speichere = [limitOn, vonSpin, bisSpin, zeige]() {
            RotorBereich r = RotorBereich::load();
            r.an = limitOn->isChecked();
            r.vonGrad = vonSpin->value();
            r.bisGrad = bisSpin->value();
            r.save();
            zeige();
        };
        connect(limitOn, &QCheckBox::toggled, &dlg, speichere);
        connect(vonSpin, &QSpinBox::valueChanged, &dlg, speichere);
        connect(bisSpin, &QSpinBox::valueChanged, &dlg, speichere);
        // Der Anschlag steht eine Zeile darueber und aendert die Rechnung.
        connect(stopCombo, &QComboBox::currentIndexChanged, &dlg, zeige);
        zeige();
    }

    // ── Local: model + port + baud ───────────────────────────────────
    auto* localBox = new QWidget(&dlg);
    auto* localForm = new QFormLayout(localBox);
    localForm->setContentsMargins(0, 0, 0, 0);

    auto* modelCombo = new QComboBox(localBox);
    const QVector<RotorModel> models = commonRotorModels();
    for (const RotorModel& m : models) {
        modelCombo->addItem(QStringLiteral("%1  (%2)").arg(m.name)
                                .arg(m.hamlibId), m.hamlibId);
    }
    // Anything Hamlib knows that is not in the short list.
    modelCombo->addItem(QStringLiteral("Other — enter a Hamlib number"), -1);
    const int savedModel = s.value(kRotorModelKey, 601).toInt();
    const int at = modelCombo->findData(savedModel);
    modelCombo->setCurrentIndex(at >= 0 ? at : 0);
    auto* modelLabel = new QLabel(QStringLiteral("Controller"), localBox);
    localForm->addRow(modelLabel, modelCombo);

    auto* otherEdit = new QLineEdit(QString::number(savedModel), localBox);
    otherEdit->setPlaceholderText(QStringLiteral("rotctl --list shows them all"));
    otherEdit->setVisible(at < 0);
    localForm->addRow(QStringLiteral("Model number"), otherEdit);

    auto* noteLabel = new QLabel(localBox);
    noteLabel->setWordWrap(true);
    noteLabel->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                                 .arg(QString::fromLatin1(Style::kTextSecondary)));
    localForm->addRow(QString{}, noteLabel);

    auto* portCombo = new QComboBox(localBox);
    portCombo->setEditable(true);   // a port that is not plugged in yet
    localForm->addRow(QStringLiteral("Serial port"), portCombo);

    auto* baudCombo = new QComboBox(localBox);
    for (int b : commonRotorBauds()) { baudCombo->addItem(QString::number(b), b); }
    const int savedBaud = s.value(kRotorBaudKey, 9600).toInt();
    const int bat = baudCombo->findData(savedBaud);
    baudCombo->setCurrentIndex(bat >= 0 ? bat : baudCombo->findData(9600));
    localForm->addRow(QStringLiteral("Speed"), baudCombo);
    col->addWidget(localBox);

    // ── Hamlib itself ────────────────────────────────────────────────
    //
    // Everything above is useless without rotctld, and rotctld comes
    // from Hamlib, which is not part of this program. Telling the
    // operator to open a terminal is where the feature stopped being
    // used, so the install happens here.
    //
    // Both queries below shell out to rotctl, so they are read once and
    // kept. Running them from refreshUi() would spawn two processes
    // every time the operator touched a combo box.
    auto* hamlibBox = new QWidget(&dlg);
    auto* hamlibCol = new QVBoxLayout(hamlibBox);
    hamlibCol->setContentsMargins(0, 0, 0, 0);
    hamlibCol->setSpacing(6);

    auto* hamlibLabel = new QLabel(hamlibBox);
    hamlibLabel->setWordWrap(true);
    hamlibLabel->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                                   .arg(QString::fromLatin1(Style::kTextSecondary)));
    hamlibCol->addWidget(hamlibLabel);

    auto* hamlibBtns = new QHBoxLayout;
    auto* installBtn = new QPushButton(hamlibBox);
    installBtn->setStyleSheet(Style::buttonBaseStyle());
    hamlibBtns->addWidget(installBtn);
    auto* copyCmdBtn = new QPushButton(QStringLiteral("Copy command"), hamlibBox);
    copyCmdBtn->setStyleSheet(Style::buttonBaseStyle());
    hamlibBtns->addWidget(copyCmdBtn);
    hamlibBtns->addStretch(1);
    hamlibCol->addLayout(hamlibBtns);

    // Homebrew takes minutes and says a great deal while it works. A
    // dialog that sits silent for that long looks hung, and an operator
    // who cannot see the output has nothing to paste into a question.
    auto* installLog = new QPlainTextEdit(hamlibBox);
    installLog->setReadOnly(true);
    installLog->setMaximumHeight(150);
    installLog->setVisible(false);
    installLog->setStyleSheet(
        QStringLiteral("QPlainTextEdit { background: %1; color: %2; "
                       "font-family: " LP_MONO_QSS "; font-size: 11px; "
                       "border: 1px solid %3; }")
            .arg(QString::fromLatin1(Style::kInsetBg),
                 QString::fromLatin1(Style::kTextSecondary),
                 QString::fromLatin1(Style::kInsetBorder)));
    hamlibCol->addWidget(installLog);
    col->addWidget(hamlibBox);

    // No parent: this lives on the stack beside the dialog, and giving
    // it a parent that outlives nothing would only invite a double
    // delete the day the declarations are reordered.
    HamlibInstaller installer;
    QString hamlibVersion;
    QVector<HamlibRotorEntry> hamlibKnows;
    auto rescanHamlib = [&]() {
        hamlibVersion = HamlibInstaller::installedVersion();
        // Only worth asking when there is something to ask. Each of
        // these runs rotctl, and the answer is kept rather than fetched
        // again on every keystroke.
        hamlibKnows = hamlibVersion.isEmpty()
            ? QVector<HamlibRotorEntry>{} : HamlibInstaller::supportedModels();
    };
    rescanHamlib();

    auto refreshPorts = [portCombo, &s]() {
        const QString keep = portCombo->currentText();
        portCombo->clear();
#ifdef HAVE_SERIALPORT
        for (const QSerialPortInfo& info : QSerialPortInfo::availablePorts()) {
            // Name the device beside the path. "/dev/tty.usbserial-1410"
            // tells nobody which box it is; "FT232R USB UART" does.
            const QString desc = info.description().isEmpty()
                ? info.manufacturer() : info.description();
            portCombo->addItem(desc.isEmpty()
                ? info.systemLocation()
                : QStringLiteral("%1  —  %2").arg(info.systemLocation(), desc),
                info.systemLocation());
        }
#endif
        if (portCombo->count() == 0) {
            portCombo->addItem(QStringLiteral("no serial ports found"),
                               QString{});
        }
        const QString saved = keep.isEmpty()
            ? s.value(kRotorDeviceKey, QString{}).toString() : keep;
        if (!saved.isEmpty()) {
            const int i = portCombo->findData(saved);
            if (i >= 0) { portCombo->setCurrentIndex(i); }
            else { portCombo->setEditText(saved); }
        }
    };
    refreshPorts();

    // ── Network: host + port ─────────────────────────────────────────
    auto* netBox = new QWidget(&dlg);
    auto* netForm = new QFormLayout(netBox);
    netForm->setContentsMargins(0, 0, 0, 0);
    // Beim GS-232 am Netzserver stehen hier dessen Adresse und Port
    // (2026-11-11) -- vorher kamen beide aus rotctld, das es auf diesem
    // Weg nicht gibt, und die Felder standen leer bzw. auf 4533.
    auto* hostEdit = new QLineEdit(
        gs232AmNetz ? gs232Host : (m_rotctld ? m_rotctld->host() : QString{}), netBox);
    hostEdit->setObjectName(QStringLiteral("rotorDialogHost"));
    hostEdit->setPlaceholderText(
        QStringLiteral("address running rotctld, e.g. 192.168.1.20"));
    netForm->addRow(QStringLiteral("Host"), hostEdit);
    auto* portEdit = new QLineEdit(
        QString::number(gs232AmNetz ? gs232Port
                                    : (m_rotctld ? m_rotctld->port() : 4533)),
        netBox);
    portEdit->setObjectName(QStringLiteral("rotorDialogPort"));
    portEdit->setPlaceholderText(QStringLiteral("4533"));
    netForm->addRow(QStringLiteral("Port"), portEdit);
    col->addWidget(netBox);

    auto* status = new QLabel(&dlg);
    status->setWordWrap(true);
    status->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                              .arg(QString::fromLatin1(Style::kTextSecondary)));
    col->addWidget(status);

    auto* row = new QHBoxLayout;
    auto* connectBtn = new QPushButton(&dlg);
    connectBtn->setStyleSheet(Style::buttonBaseStyle());
    row->addWidget(connectBtn);
    row->addStretch(1);
    auto* closeBtn = new QPushButton(QStringLiteral("Close"), &dlg);
    closeBtn->setStyleSheet(Style::buttonBaseStyle());
    row->addWidget(closeBtn);
    col->addLayout(row);

    auto currentModel = [modelCombo, otherEdit]() {
        const int id = modelCombo->currentData().toInt();
        return id > 0 ? id : otherEdit->text().trimmed().toInt();
    };

    auto refreshUi = [&]() {
        const bool gs232 = driverBox->currentIndex() == 1;
        const bool local = modeBox->currentIndex() == 0;

        // Die zweite Zeile heisst je nach Treiber etwas anderes: bei
        // Hamlib geht es darum, WO rotctld laeuft, beim direkten Weg
        // darum, WORAN der Controller haengt.
        {
            QSignalBlocker b(modeBox);
            modeBox->setItemText(0, gs232
                ? QStringLiteral("Serial port on this computer")
                : QStringLiteral("Controller on this computer"));
            modeBox->setItemText(1, gs232
                ? QStringLiteral("Serial server on the network (ser2net, Moxa …)")
                : QStringLiteral("rotctld already running (network)"));
        }
        hostEdit->setPlaceholderText(gs232
            ? QStringLiteral("address of the serial server, e.g. 192.168.1.20")
            : QStringLiteral("address running rotctld, e.g. 192.168.1.20"));
        portEdit->setPlaceholderText(gs232 ? QStringLiteral("4001")
                                           : QStringLiteral("4533"));

        localBox->setVisible(local);
        netBox->setVisible(!local);
        // Modellwahl, Hamlib-Kasten und der Hinweis dazu gehoeren allein
        // zum rotctld-Weg: der direkte Treiber kennt nur GS-232A/B.
        modelLabel->setVisible(!gs232);
        modelCombo->setVisible(!gs232);
        otherEdit->setVisible(!gs232 && local
                              && modelCombo->currentData().toInt() < 0);

        const int id = currentModel();
        QString note;
        for (const RotorModel& m : models) {
            if (m.hamlibId == id) { note = m.note; break; }
        }
        noteLabel->setText(note);
        noteLabel->setVisible(!note.isEmpty());

        // ── What to say about Hamlib ─────────────────────────────────
        const bool installed = !hamlibVersion.isEmpty();
        const bool installing = installer.isRunning();
        hamlibBox->setVisible(local && !gs232);
        noteLabel->setVisible(noteLabel->isVisible() && !gs232);

        if (!installed) {
            hamlibLabel->setText(QStringLiteral(
                "Hamlib isn't installed. It provides rotctld, which is "
                "the piece that talks to the controller — without it "
                "nothing below can connect."));
            installBtn->setVisible(HamlibInstaller::canInstallAutomatically());
            installBtn->setEnabled(!installing);
            installBtn->setText(installing
                ? QStringLiteral("Installing…")
                : QStringLiteral("Install Hamlib and connect"));
            copyCmdBtn->setVisible(true);
            copyCmdBtn->setText(
                HamlibInstaller::canInstallAutomatically()
                    ? QStringLiteral("Copy command")
                    : QStringLiteral("Copy instructions"));
            if (!HamlibInstaller::canInstallAutomatically()) {
                hamlibLabel->setText(hamlibLabel->text()
                    + QStringLiteral("\n\n")
                    + HamlibInstaller::manualInstructions());
            }
        } else {
            QString line = QStringLiteral("Hamlib %1 · %2")
                               .arg(hamlibVersion,
                                    RotctldProcess::findBinary());

            // The number matters more than the name. Hamlib gains
            // drivers between releases, so a controller in the picker
            // can be missing from the copy that is actually installed —
            // and rotctld's way of saying so is to exit, which reads as
            // a cable fault rather than as an out-of-date Hamlib.
            if (!hamlibKnows.isEmpty() && id > 0) {
                bool has = false;
                for (const HamlibRotorEntry& e : hamlibKnows) {
                    if (e.model == id) { has = true; break; }
                }
                if (!has) {
                    line += QStringLiteral(
                        "\n⚠ This Hamlib doesn't have model %1. Update "
                        "Hamlib, or pick one of the emulations listed "
                        "under the controller.").arg(id);
                }
            }
            hamlibLabel->setText(line);
            installBtn->setVisible(false);
            copyCmdBtn->setVisible(false);
        }

        connectBtn->setText(m_rotor->isConnected()
            ? QStringLiteral("Disconnect") : QStringLiteral("Connect"));
        // Offering Connect with no rotctld to run only produces a
        // puzzling failure; the install button above is the thing to
        // press.
        // Der direkte Weg braucht kein Hamlib -- der Knopf darf dort
        // immer.
        connectBtn->setEnabled(gs232 || !local || installed);
        status->setText(m_rotor->isConnected()
            ? QStringLiteral("Connected — %1").arg(m_rotor->description())
            : QStringLiteral("Not connected"));
    };
    refreshUi();

    connect(modeBox, &QComboBox::currentIndexChanged, &dlg,
            [&](int) { refreshUi(); });
    connect(driverBox, &QComboBox::currentIndexChanged, &dlg,
            [&](int) { refreshUi(); });
    connect(modelCombo, &QComboBox::currentIndexChanged, &dlg,
            [&](int) { refreshUi(); });

    auto reportConn = connect(m_rotor, &RotorController::errorOccurred,
                              &dlg, [status](const QString& m) {
        status->setText(m);
    });
    auto reportState = connect(m_rotor, &RotorController::stateChanged,
                               &dlg, [&](auto) { refreshUi(); });

    connect(connectBtn, &QPushButton::clicked, &dlg, [&]() {
        if (m_rotor->isConnected()) {
            m_rotor->disconnectFromRotor();
            m_rotorProc.stop();
            // Selbst getrennt: beim naechsten Start nicht wieder verbinden.
            AppSettings::instance().setValue(kRotorConnectOnStartKey,
                                              QStringLiteral("False"));
            refreshUi();
            return;
        }
        // Verbunden wird auf Wunsch: beim naechsten Start wieder.
        AppSettings::instance().setValue(kRotorConnectOnStartKey,
                                          QStringLiteral("True"));

        const bool local = modeBox->currentIndex() == 0;
        s.setValue(kRotorUseSerialKey, local);

        // ── Der direkte GS-232-Weg ───────────────────────────────────
        //
        // Kein rotctld, kein Hamlib: Longpath spricht den Controller
        // selbst an, seriell oder ueber einen seriellen Netzserver
        // (Gs232Rotor.h). Der Treiber steht beim Oeffnen des Fensters
        // fest, also wird er hier gewechselt, wenn die Wahl eine andere
        // ist als das, was gerade laeuft.
        if (driverBox->currentIndex() == 1) {
            s.setValue(QStringLiteral("Rotor/Backend"), QStringLiteral("gs232"));
            QString netzHost;
            quint16 netzPort = 0;
            if (local && Gs232Rotor::parseNetworkTarget(
                             portCombo->currentText(), &netzHost, &netzPort)) {
                // „172.30.30.16:4001" im Schnittstellenfeld: ein Netzziel,
                // keine Datei (2026-11-11). Als solches ablegen.
                s.setValue(kRotorUseSerialKey, false);
                s.setValue(QStringLiteral("Rotor/Host"), netzHost);
                s.setValue(QStringLiteral("Rotor/NetPort"), netzPort);
            } else if (local) {
                s.setValue(QStringLiteral("Rotor/SerialPort"),
                           portCombo->currentText().trimmed());
                s.setValue(QStringLiteral("Rotor/BaudRate"),
                           baudCombo->currentText().trimmed());
                s.setValue(QStringLiteral("Rotor/Host"), QString{});
            } else {
                s.setValue(QStringLiteral("Rotor/Host"), hostEdit->text().trimmed());
                s.setValue(QStringLiteral("Rotor/NetPort"),
                           portEdit->text().trimmed().isEmpty()
                               ? QStringLiteral("4001")
                               : portEdit->text().trimmed());
            }
            m_rotorProc.stop();          // der alte Weg braucht ihn nicht mehr
            rebuildRotorBackend();
            m_rotor->connectToRotor();
            refreshUi();
            return;
        }
        s.setValue(QStringLiteral("Rotor/Backend"), QStringLiteral("rotctld"));
        rebuildRotorBackend();

        if (local) {
            const int model = currentModel();
            // 2026-09-16: currentData() survives a setEditText() call
            // untouched — refreshPorts() pre-fills the line edit with a
            // saved network address (e.g. an ARCO's "host:port") via
            // setEditText(), but currentIndex() (and so currentData())
            // stays wherever it was left, usually index 0's real serial
            // port. Trusting currentData() whenever it happens to be
            // valid meant Connect silently used that stale real port
            // instead of the address on screen — and then SAVED it back
            // over the operator's setting, so the correct value kept
            // reappearing lost. currentData() is only trustworthy when
            // the visible text still matches the selected item's own
            // text, i.e. the operator picked it from the dropdown rather
            // than typing or having it pre-filled.
            const int idx = portCombo->currentIndex();
            const bool selectionStillShown = idx >= 0
                && portCombo->itemText(idx) == portCombo->currentText();
            const QString device = (selectionStillShown
                && portCombo->currentData().isValid()
                && !portCombo->currentData().toString().isEmpty())
                    ? portCombo->currentData().toString()
                    : portCombo->currentText().trimmed();
            const int baud = baudCombo->currentData().toInt();

            s.setValue(kRotorModelKey, model);
            s.setValue(kRotorDeviceKey, device);
            s.setValue(kRotorBaudKey, baud);

            QString err;
            if (!m_rotorProc.start(model, device, baud, 4533, &err)) {
                status->setText(err);
                return;
            }
            // The port actually bound — 4533 unless something else
            // had it (see RotctldProcess::start).
            const quint16 port = m_rotorProc.listenPort();
            if (m_rotctld) { m_rotctld->setTarget(QStringLiteral("127.0.0.1"), port); }
            s.setValue(kRotorHostKey, QStringLiteral("127.0.0.1"));
            s.setValue(kRotorPortKey, port);
        } else {
            const QString host = hostEdit->text().trimmed();
            const quint16 port =
                static_cast<quint16>(portEdit->text().trimmed().toUInt());
            if (m_rotctld) { m_rotctld->setTarget(host, port ? port : 4533); }
            s.setValue(kRotorHostKey, host);
            s.setValue(kRotorPortKey, port ? port : 4533);
        }
        m_rotor->connectToRotor();
    });
    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    // ── Installing Hamlib, then connecting ───────────────────────────

    connect(copyCmdBtn, &QPushButton::clicked, &dlg, [&]() {
        const QStringList cmd = HamlibInstaller::installCommand();
        QGuiApplication::clipboard()->setText(
            cmd.isEmpty() ? HamlibInstaller::manualInstructions()
                          : cmd.join(QLatin1Char(' ')));
        status->setText(QStringLiteral("Copied. Paste it into a terminal, "
                                       "then press Connect."));
    });

    connect(&installer, &HamlibInstaller::output, &dlg,
            [installLog](const QString& line) {
        installLog->appendPlainText(line);
    });

    connect(&installer, &HamlibInstaller::finished, &dlg,
            [&](bool ok, const QString& message) {
        rescanHamlib();
        status->setText(message);
        refreshUi();
        // "Install and connect" means both. Stopping at "installed" and
        // making the operator find the next button is the small gap the
        // whole exercise was meant to close.
        if (ok && !m_rotor->isConnected()) { connectBtn->click(); }
    });

    connect(installBtn, &QPushButton::clicked, &dlg, [&]() {
        installLog->clear();
        installLog->setVisible(true);
        status->setText(QStringLiteral(
            "Installing Hamlib. This takes a few minutes the first time "
            "— Homebrew's output is below."));
        refreshUi();

        QString err;
        if (!installer.install(&err)) {
            status->setText(err);
            refreshUi();
        }
    });

    dlg.exec();
    disconnect(reportConn);
    disconnect(reportState);
}

void RotorLogbookPanel::updateSolarLine()
{
    if (!m_hasDxPos) {
        m_solarLine->clear();
        return;
    }

    const SolarInfo s =
        solarInfo(QDateTime::currentDateTimeUtc(), m_dxLat, m_dxLon);

    QString text;
    if (s.alwaysUp) {
        text = QStringLiteral("DX: midnight sun");
    } else if (s.alwaysDown) {
        text = QStringLiteral("DX: polar night");
    } else {
        text = QStringLiteral("DX: SR %1z · SS %2z")
                   .arg(s.riseUtc.toString(QStringLiteral("HH:mm")),
                        s.setUtc.toString(QStringLiteral("HH:mm")));
    }
    // Elevation with an explicit sign: "-9" reads as night at a glance
    // in a way that "9 degrees below the horizon" does not.
    text += QStringLiteral(" · sun %1%2°")
                .arg(s.elevationDeg >= 0 ? QStringLiteral("+")
                                         : QStringLiteral("-"))
                .arg(std::abs(s.elevationDeg), 0, 'f', 0);
    if (s.greyline) { text += QStringLiteral(" · GREY LINE"); }

    m_solarLine->setText(text);
    m_solarLine->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }")
        .arg(QString::fromLatin1(s.greyline ? Style::kAmberText
                                            : Style::kTextSecondary)));
}

void RotorLogbookPanel::chooseWorldImage()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Choose an equirectangular world image"),
        AppSettings::instance().value(kWorldImgKey, QString{}).toString(),
        QStringLiteral("Images (*.jpg *.jpeg *.png *.tif *.tiff *.webp)"));
    if (path.isEmpty()) { return; }

    if (!m_globe->loadTexture(path)) {
        setStatus(QStringLiteral("Couldn't read that image"), true);
        return;
    }
    AppSettings::instance().setValue(kWorldImgKey, path);
    setStatus(QStringLiteral("World image loaded"));
}

void RotorLogbookPanel::downloadWorldImage(const QStringList& candidates,
                                           int index, const QString& label)
{
    if (index < 0 || index >= candidates.size()) { return; }
    const QString url = candidates.at(index);

    if (!m_net) { m_net = new QNetworkAccessManager(this); }

    const QString dest =
        AppSettings::dataDir()
        + QStringLiteral("/world.jpg");

    setStatus(QStringLiteral("Downloading world image (%1)…").arg(label));

    QNetworkRequest req{QUrl(url)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_net->get(req);

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, label](qint64 got, qint64 total) {
        if (total <= 0) { return; }
        setStatus(QStringLiteral("Downloading world image (%1) — %2%")
                      .arg(label).arg(got * 100 / total));
    });

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, dest, candidates, index, label]() {
        reply->deleteLater();

        auto tryNext = [this, candidates, index, label](const QString& why) {
            if (index + 1 < candidates.size()) {
                downloadWorldImage(candidates, index + 1, label);
                return;
            }
            // Out of candidates. Name the last address: if NASA has moved
            // the file for good, the operator can find its replacement and
            // use "Choose a file…". An unexplained failure would just look
            // like a feature that does not work.
            setStatus(QStringLiteral("Download failed: %1 — you can still "
                                     "load an image manually (%2)")
                          .arg(why, candidates.last()), true);
        };

        if (reply->error() != QNetworkReply::NoError) {
            tryNext(reply->errorString());
            return;
        }

        const QByteArray bytes = reply->readAll();
        QFile out(dest);
        if (!out.open(QIODevice::WriteOnly)) {
            setStatus(QStringLiteral("Couldn't save the image: %1")
                          .arg(out.errorString()), true);
            return;
        }
        out.write(bytes);
        out.close();

        // Load before remembering the path: a truncated download, or an
        // error page served with status 200, must not become the
        // remembered setting.
        if (!m_globe->loadTexture(dest)) {
            tryNext(QStringLiteral("not a readable image"));
            return;
        }
        AppSettings::instance().setValue(kWorldImgKey, dest);
        setStatus(QStringLiteral("World image loaded (%1 KB)")
                      .arg(bytes.size() / 1024));
    });
}

void RotorLogbookPanel::onCallsignEdited(const QString& raw)
{
    const QString call = raw.trimmed().toUpper();
    if (call != raw) {
        QSignalBlocker block(m_callEdit);
        m_callEdit->setText(call);
    }
    updateWorkedLine(call);
    scheduleAutoLookup(call);

    if (call.isEmpty()) {
        m_flagEmoji.clear();
        m_stationLine->clear();
        m_solarLine->clear();
        m_workedLine->clear();
        m_workedLine->setVisible(false);
        m_photo->showPlaceholder(QString{});
        m_photo->setVisible(false);
        m_hasDxPos = false;
        m_lastInfo = CallsignInfo{};
        return;
    }

    // Country estimate from cty.dat — no network, so it keeps up with
    // typing. QRZ replaces it on demand.
    if (!m_radio) { return; }
    DxccColorProvider* dxcc = m_radio->dxccColorProvider();
    if (!dxcc) { return; }

    const CtyDatParser& cty = dxcc->ctyDat();
    const QString prefix = cty.resolvePrimaryPrefix(call);

    // The flag comes from the prefix alone, so it appears as the callsign
    // is typed — no locator and no lookup needed.
    updateFlagFor(call);

    const DxccEntity* ent = prefix.isEmpty() ? nullptr
                                             : cty.entityByPrefix(prefix);
    if (!ent || !ent->hasLatLon) {
        // Even with no coordinates the entity name is worth showing, and
        // it proves the prefix resolved — which is what a missing flag
        // would otherwise leave ambiguous.
        setStationLine(ent ? ent->name : QString{});
        return;
    }

    // The entity centre is enough for the grey line even with no
    // locator of our own — sunrise over Japan does not depend on where
    // the listener is standing.
    m_dxLat = ent->latitude; m_dxLon = ent->longitude; m_hasDxPos = true;
    m_globe->setTarget(ent->latitude, ent->longitude);
    updateSolarLine();

    const QString mine = m_myGrid->text().trimmed().toUpper();
    if (!isValidGridSquare(mine)) { return; }

    const QString entGrid = gridSquareFromLatLon(ent->latitude, ent->longitude);
    m_dial->setTargetBearing(calculateBearingInDegrees(mine, entGrid));
    setStationLine(QStringLiteral("%1 · %2 km (from prefix)")
        .arg(ent->name)
        .arg(calculateDistanceKm(mine, entGrid), 0, 'f', 0));
}

// ── Station card ────────────────────────────────────────────────────

void RotorLogbookPanel::currentBandMode(QString& band, QString& mode) const
{
    band.clear();
    mode.clear();
    SliceModel* s = m_radio ? m_radio->activeSlice() : nullptr;
    if (!s) { return; }

    band = bandLabel(bandFromFrequency(s->frequency()));

    // Same collapsing as buildEntry: ADIF has no LSB/USB mode, and the
    // worked-before answer has to be about the mode the contact will be
    // logged as, not the one the radio calls it.
    const QString m = SliceModel::modeName(s->dspMode());
    if (m == QLatin1String("LSB") || m == QLatin1String("USB")) {
        mode = QStringLiteral("SSB");
    } else if (m == QLatin1String("CWL") || m == QLatin1String("CWU")) {
        mode = QStringLiteral("CW");
    } else {
        mode = m;
    }
}

void RotorLogbookPanel::updateWorkedLine(const QString& call)
{
    if (call.trimmed().isEmpty()) {
        m_workedLine->clear();
        m_workedLine->setVisible(false);
        return;
    }

    WorkedBefore::PrefixResolver resolver;
    if (m_radio) {
        if (DxccColorProvider* dxcc = m_radio->dxccColorProvider()) {
            resolver = [dxcc](const QString& c) {
                return dxcc->ctyDat().resolvePrimaryPrefix(c);
            };
        }
    }

    QString band, mode;
    currentBandMode(band, mode);
    const WorkedSummary s = m_worked.lookup(call, band, mode, resolver);

    QStringList bits;
    QString colour = QString::fromLatin1(Style::kTextSecondary);

    if (s.knownEntity && s.newEntity) {
        bits << QStringLiteral("NEW DXCC");
        colour = QString::fromLatin1(Style::kGreenText);
    } else if (s.knownEntity && (s.newBand || s.newMode)) {
        QStringList what;
        if (s.newBand) { what << QStringLiteral("band"); }
        if (s.newMode) { what << QStringLiteral("mode"); }
        bits << QStringLiteral("new %1 for %2")
                    .arg(what.join(QStringLiteral(" and ")), s.entity);
        colour = QString::fromLatin1(Style::kAmberText);
    }

    if (s.timesWorked > 0) {
        QString had = s.timesWorked == 1
            ? QStringLiteral("worked once")
            : QStringLiteral("worked %1×").arg(s.timesWorked);
        if (s.lastWorked.isValid()) {
            had += QStringLiteral(", last %1")
                       .arg(s.lastWorked.toString(QStringLiteral("yyyy-MM-dd")));
        }
        bits << had;
    } else if (bits.isEmpty()) {
        bits << QStringLiteral("not in your log");
    }

    m_workedLine->setText(bits.join(QStringLiteral(" · ")));
    m_workedLine->setVisible(!bits.isEmpty());
    m_workedLine->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }").arg(colour));
}

void RotorLogbookPanel::updateFlagFor(const QString& call)
{
    // Flag from the DXCC prefix rather than from QRZ's free-text country
    // field: the prefix is what cty.dat resolves deterministically, while
    // "country" arrives spelled a dozen different ways.
    m_flagEmoji.clear();
    if (!m_radio) { return; }
    DxccColorProvider* dxcc = m_radio->dxccColorProvider();
    if (!dxcc) { return; }
    m_flagEmoji = dxccFlagEmoji(dxcc->ctyDat().resolvePrimaryPrefix(call));
}

// 2026-08-10, operator wish: "more data after Enter" — everything the
// lookup answer carries that an operator in QSO actually uses. Trimmed
// back down 2026-09-01 to the three facts that still earn their place
// once the portrait grew large enough to want the room; see the note
// inside the function.
QString RotorLogbookPanel::stationText(const CallsignInfo& info) const
{
    // Betreiber 2026-09-01: "Daten koennen minimiert werden. Name,
    // Entfernung, Land wuerde reichen." Stadt/Bundesland/Locator/County/
    // Lizenzklasse/QSL-Wege/Peilung fallen aus dieser Zeile -- die
    // Peilung steht ohnehin schon als Zeiger auf dem Kompass, das
    // brauchte hier keine zweite Zahl.
    QStringList bits;
    if (!info.displayName().isEmpty()) { bits << info.displayName(); }
    if (!info.country.trimmed().isEmpty()) { bits << info.country.trimmed(); }

    const QString mine = m_myGrid->text().trimmed().toUpper();
    if (isValidGridSquare(mine) && isValidGridSquare(info.grid)) {
        bits << QStringLiteral("%1 km")
                    .arg(calculateDistanceKm(mine, info.grid), 0, 'f', 0);
    }
    return bits.join(QStringLiteral(" · "));
}

void RotorLogbookPanel::setStationLine(const QString& text)
{
    if (m_flagEmoji.isEmpty()) {
        m_stationLine->setText(text);
    } else if (text.isEmpty()) {
        m_stationLine->setText(m_flagEmoji);
    } else {
        m_stationLine->setText(m_flagEmoji + QLatin1Char(' ') + text);
    }
}

void RotorLogbookPanel::showStationVisuals(const CallsignInfo& info)
{
    if (AppSettings::instance().value(kShowPhotoKey, true).toBool()
        && !info.imageUrl.isEmpty()) {
        // StationPhoto owns the rules now — https only, no downgrade on
        // redirect, a size cap, the cache. It shows itself once a
        // picture actually decodes; see the photoShown connection in
        // buildUi. (2026-08-10)
        m_photo->setUrl(info.imageUrl);
    } else {
        m_photo->showPlaceholder(QString{});
        m_photo->setVisible(false);
    }
}

void RotorLogbookPanel::onLookupRequested()
{
    const QString call = Callsigns::normalized(m_callEdit->text());
    if (call.isEmpty()) { return; }
    if (!Callsigns::isLikelyCallsign(call)) {
        setStatus(QStringLiteral("%1 doesn't look like a callsign").arg(call),
                  true);
        return;
    }

    // Already answered once this session. A contest is three hundred
    // callsigns and some of them repeat; a metered subscription should
    // not pay twice for the same answer.
    if (m_qrzCache.contains(call)) {
        const CallsignInfo cached = m_qrzCache.value(call);
        m_lastInfo = cached;
        adoptGridFromQrz(cached);
        updateFlagFor(cached.call);
        setStationLine(stationText(cached));
        showStationVisuals(cached);
        meldeAuskunft(call, RufzeichenAuskunft::Stand::Da, cached);
        return;
    }

    if (!m_qrz || !m_qrz->hasCredentials()) {
        setStatus(QStringLiteral("Add your QRZ account in Tools first"), true);
        meldeAuskunft(call, RufzeichenAuskunft::Stand::KeinKonto);
        return;
    }
    setStatus(QStringLiteral("Looking up %1…").arg(call));
    meldeAuskunft(call, RufzeichenAuskunft::Stand::Laeuft);
    m_qrz->lookup(call);
}

void RotorLogbookPanel::scheduleAutoLookup(const QString& call)
{
    if (!m_lookupTimer) { return; }
    m_lookupTimer->stop();

    if (!AppSettings::instance().value(kAutoLookupKey, true).toBool()) {
        meldeAuskunft(call, RufzeichenAuskunft::Stand::Aus);
        return;
    }
    if (call.size() < kMinAutoLookupChars) { return; }
    if (!Callsigns::isLikelyCallsign(call)) { return; }
    if (!m_qrz || !m_qrz->hasCredentials()) {
        meldeAuskunft(call, RufzeichenAuskunft::Stand::KeinKonto);
        return;
    }

    // A cached answer needs no waiting.
    if (m_qrzCache.contains(call)) { onLookupRequested(); return; }

    m_lookupTimer->start();
    meldeAuskunft(call, RufzeichenAuskunft::Stand::Laeuft);
}

void RotorLogbookPanel::meldeAuskunft(const QString& call,
                                      RufzeichenAuskunft::Stand stand,
                                      const CallsignInfo& info)
{
    const QString norm = Callsigns::normalized(call);
    if (norm.isEmpty()) { return; }
    RufzeichenAuskunft a;
    a.rufzeichen = norm;
    a.stand      = stand;
    a.info       = info;
    emit auskunft(a);
}

void RotorLogbookPanel::adoptGridFromQrz(const CallsignInfo& info)
{
    if (!isValidGridSquare(info.grid)) { return; }
    // Never over a locator a person typed. QRZ is often right and
    // sometimes years out of date; the operator in front of the radio
    // has just been told the real one.
    if (m_dxGridIsManual && !m_dxGrid->text().trimmed().isEmpty()) { return; }

    m_adoptingGrid = true;
    m_dxGrid->setText(info.grid.trimmed().toUpper());
    m_adoptingGrid = false;
    applyLocators();
}

// ── Logging ─────────────────────────────────────────────────────────

LogEntry RotorLogbookPanel::buildEntry() const
{
    LogEntry e = draftFromRadio(m_callEdit->text());
    e.gridSquare   = m_dxGrid->text().trimmed().toUpper();
    e.rstSent      = m_rstSent->text().trimmed();
    e.rstRcvd      = m_rstRcvd->text().trimmed();
    e.comment      = m_comment->text().trimmed();

    // QRZ detail, but only when it belongs to THIS callsign — a card
    // left over from the previous station would put the wrong operator
    // in the log.
    if (m_lastInfo.isValid()
        && Callsigns::normalized(m_lastInfo.call) == e.call) {
        e.name    = m_lastInfo.displayName();
        e.qth     = m_lastInfo.city;
        e.country = m_lastInfo.country;
    }

    if (isValidGridSquare(e.myGridSquare) && isValidGridSquare(e.gridSquare)) {
        e.distanceKm = calculateDistanceKm(e.myGridSquare, e.gridSquare);
        e.bearingDeg = calculateBearingInDegrees(e.myGridSquare, e.gridSquare);
    }
    stampSatellites(e);
    return e;
}

LogEntry RotorLogbookPanel::draftFromRadio(const QString& call) const
{
    LogEntry e;
    e.call         = Callsigns::normalized(call);
    e.timeOn       = QDateTime::currentDateTimeUtc();
    e.myGridSquare = m_myGrid->text().trimmed().toUpper();

    if (SliceModel* s = m_radio ? m_radio->activeSlice() : nullptr) {
        e.freqMHz = s->frequency() / 1e6;
        e.band    = bandLabel(bandFromFrequency(s->frequency()));

        // ── Transverter (2026-10-28) ─────────────────────────────
        //
        // Steht einer davor, traegt der Satz die Frequenz der ANTENNE
        // und deren ADIF-Bandnamen. Ohne Transverter gibt
        // logbuchFrequenz() false zurueck und die zwei Zeilen darueber
        // bleiben, wie sie waren.
        TransverterSetup::logbuchFrequenz(std::llround(s->frequency()),
                                          &e.freqMHz, &e.band);

        // ADIF has no LSB/USB mode — those are submodes of SSB, and a
        // record with MODE=LSB is rejected or silently rewritten.
        const QString m = SliceModel::modeName(s->dspMode());
        if (m == QLatin1String("LSB") || m == QLatin1String("USB")) {
            e.mode = QStringLiteral("SSB");
            e.submode = m;
        } else if (m == QLatin1String("CWL") || m == QLatin1String("CWU")) {
            e.mode = QStringLiteral("CW");
        } else {
            e.mode = m;
        }
    }
    return e;
}

// Welche Satelliten standen zum QSO-Zeitpunkt ueber dem eigenen Horizont?
// Beim Loggen gerechnet, mit den Bahndaten von jetzt — spaeter liesse
// sich das nicht mehr nachholen, ein TLE taugt nur Tage. Nichts in Sicht
// oder kein Standort: kein Feld.
void RotorLogbookPanel::stampSatellites(LogEntry& e) const
{
    if (!m_satellites || !m_satellites->hasCatalog()) { return; }
    for (const auto& kv : e.extras) {
        if (kv.first == SatelliteService::stampField()) { return; }   // schon gestempelt
    }
    const QString text = m_satellites->stamp(e.timeOn.isValid() ? e.timeOn
                                                                 : QDateTime::currentDateTimeUtc(),
                                             e.myGridSquare);
    if (!text.isEmpty()) {
        e.extras.append(qMakePair(SatelliteService::stampField(), text));
    }
}

void RotorLogbookPanel::setSatellites(SatelliteService* svc)
{
    m_satellites = svc;
    if (m_logWindow) { m_logWindow->setSatellites(svc); }
}

bool RotorLogbookPanel::appendToLogFile(const LogEntry& entry, QString* error)
{
    // Der Rumpf ist nach core/LogbookDatei umgezogen -- siehe logbookPath().
    return LogbookDatei::anhaengen(entry, error);
}

bool RotorLogbookPanel::commitEntry(LogEntry e, QWidget* askParent, QString* message)
{
    QString dummy;
    QString& msg = message ? *message : dummy;
    if (!e.isValid()) {
        msg = QStringLiteral("Enter a callsign to log");
        return false;
    }
    if (e.distanceKm <= 0.0
        && isValidGridSquare(e.myGridSquare) && isValidGridSquare(e.gridSquare)) {
        e.distanceKm = calculateDistanceKm(e.myGridSquare, e.gridSquare);
        e.bearingDeg = calculateBearingInDegrees(e.myGridSquare, e.gridSquare);
    }
    stampSatellites(e);

    // Ask, do not refuse. Working the same station twice on one band
    // and mode inside a couple of minutes is unusual but legitimate —
    // a botched exchange repeated, a contest dupe logged deliberately.
    // The same rule the importer uses, so one idea of "the same QSO"
    // rather than two that disagree.
    if (m_worked.wouldDuplicate(e)) {
        if (QMessageBox::question(askParent ? askParent : this, QStringLiteral("Duplicate"),
                QStringLiteral("You already have %1 on %2 at about this "
                               "time.\n\nLog it again?")
                    .arg(e.call,
                         e.band.isEmpty() ? QStringLiteral("this band")
                                          : e.band),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes) {
            msg = QStringLiteral("Not logged — %1 is already in the log").arg(e.call);
            return false;
        }
    }

    QString err;
    if (!appendToLogFile(e, &err)) {
        msg = QStringLiteral("Couldn't write the log: %1").arg(err);
        return false;
    }

    // Confirm the local write before the upload, so a failing upload
    // never reads as a lost contact.
    msg = QStringLiteral("Logged %1").arg(e.call);
    if (!e.band.isEmpty()) { msg += QStringLiteral(" on %1").arg(e.band); }
    // Every service that is set up, not QRZ alone (2026-11-17). The
    // queue answers on a later turn of the event loop, so this line is
    // on screen first and the result replaces it.
    if (m_uploadQueue && m_uploadQueue->submit(e) > 0) {
        msg += QStringLiteral(" · uploading…");
    }

    refreshRecentList();
    emit qsoLogged(e);
    return true;
}

void RotorLogbookPanel::logFromLogbook(const LogEntry& partial)
{
    // Frequenz, Band, Betriebsart, Zeit und eigener Locator kommen von
    // hier -- dieselben wie beim "Log QSO" dieses Felds. Aus dem Logbuch
    // nur, was dort eingegeben oder nachgeschlagen wurde.
    LogEntry e = draftFromRadio(partial.call);
    e.rstSent    = partial.rstSent;
    e.rstRcvd    = partial.rstRcvd;
    e.comment    = partial.comment;
    e.name       = partial.name;
    e.qth        = partial.qth;
    e.country    = partial.country;
    e.gridSquare = partial.gridSquare;

    QString msg;
    const bool ok = commitEntry(e, m_logWindow ? static_cast<QWidget*>(m_logWindow) : this, &msg);
    setStatus(msg, !ok);
    if (m_logWindow) { m_logWindow->reportLogged(ok, msg); }
}

void RotorLogbookPanel::onLogQso()
{
    QString msg;
    const LogEntry e = buildEntry();
    const bool ok = commitEntry(e, this, &msg);
    setStatus(msg, !ok);
    if (!ok) { return; }
    if (m_logWindow) { m_logWindow->reload(); }

    // Clear only what belongs to the contact just made. The locators
    // and the reports stay — the next station is usually worked with
    // the same defaults, and retyping "59" every time is friction.
    // Clear the DX locator too, and forget that it was hand-entered.
    // Carrying the last station's grid into the next contact is how a
    // whole run of contest QSOs ends up logged from one square.
    m_adoptingGrid = true;
    m_dxGrid->clear();
    m_adoptingGrid = false;
    m_dxGridIsManual = false;

    m_callEdit->clear();
    m_comment->clear();
    m_stationLine->clear();
    m_flagEmoji.clear();
    m_lastInfo = CallsignInfo{};
    m_callEdit->setFocus();
}

// 2026-08-10: WSJT-X (or any program speaking its UDP protocol) logged
// a contact. Same file, same duplicate rule, same uploader as manual
// logging — the only differences are that nothing here asks questions
// (an FT8 run must not stop for a dialog) and the status line says
// where the contact came from.
void RotorLogbookPanel::logExternalQso(const LogEntry& entry)
{
    if (!entry.isValid()) { return; }

    LogEntry e = entry;
    if (e.myGridSquare.trimmed().isEmpty()) {
        e.myGridSquare = m_myGrid->text().trimmed().toUpper();
    }
    if (isValidGridSquare(e.myGridSquare)
        && isValidGridSquare(e.gridSquare)) {
        e.distanceKm = calculateDistanceKm(e.myGridSquare, e.gridSquare);
        e.bearingDeg =
            calculateBearingInDegrees(e.myGridSquare, e.gridSquare);
    }
    stampSatellites(e);

    // Silently skip duplicates instead of asking: WSJT-X re-sends its
    // message when its own log is edited, and a dialog popping up mid
    // FT8 sequence would cost the next transmit period.
    if (m_worked.wouldDuplicate(e)) {
        setStatus(QStringLiteral("%1 already logged — WSJT-X duplicate "
                                 "skipped").arg(e.call));
        return;
    }

    QString err;
    if (!appendToLogFile(e, &err)) {
        setStatus(QStringLiteral("Couldn't write WSJT-X contact: %1")
                      .arg(err), true);
        return;
    }

    // Ein offenes Logbuch liest mit. Sonst haelt es den alten Stand,
    // und seine naechste Korrektur (saveAll schreibt die ganze Datei)
    // loeschte dieses QSO wieder (Nachttest 2026-09-26).
    if (m_logWindow) { m_logWindow->reload(); }

    QString msg = QStringLiteral("Logged %1 from WSJT-X").arg(e.call);
    if (!e.band.isEmpty()) { msg += QStringLiteral(" on %1").arg(e.band); }
    // The built-in FT8/FT4 arrives here too (Ft8Applet::qsoFertig), so
    // this is the line that sends a digital contact out — to every
    // service that is set up, see commitEntry().
    if (m_uploadQueue && m_uploadQueue->submit(e) > 0) {
        msg += QStringLiteral(" · uploading…");
    }
    setStatus(msg);
    refreshRecentList();
    emit qsoLogged(e);
}

// What markUploaded records, and what it does not (2026-11-17):
//
// The log file has ONE upload note per contact, APP_LONGPATH_QRZUP, and
// it means what it says — QRZ has it. It is the tick in the logbook's
// QRZ column. There is no such note per service, and none is added
// here: what a service still lacks is on QsoUploadQueue's waiting list,
// which is the only record that needs to exist while an upload is
// outstanding. So this is called for QRZ's answers only.
bool RotorLogbookPanel::markUploadedInFile(const LogEntry& sent)
{
    if (!sent.isValid()) { return false; }

    // This exact contact, not every past QSO with the same station.
    // Marking by callsign alone would claim a station's whole history
    // had been uploaded because one of its contacts was.
    QVector<LogEntry> all = AdifLog::read(logbookPath());
    bool changed = false;
    for (LogEntry& e : all) {
        if (!e.uploadedToQrz && AdifLog::isSameQso(e, sent)) {
            e.uploadedToQrz = true;
            changed = true;
            break;
        }
    }
    if (!changed) { return false; }

    QString err;
    if (!AdifLog::write(logbookPath(), all, &err)) {
        // Not worth interrupting for: the contact is logged and it is
        // uploaded. Only the note saying so failed, and the worst
        // consequence is that it gets offered for upload again, where
        // QRZ will call it a duplicate.
        qWarning("Couldn't record the QRZ upload: %s", qPrintable(err));
        return false;
    }
    return true;
}

void RotorLogbookPanel::markUploaded(const LogEntry& sent)
{
    if (markUploadedInFile(sent) && m_logWindow) {
        // Wie beim WSJT-X-QSO: sonst schriebe die naechste Korrektur im
        // Logbuch den Vermerk wieder zurueck.
        m_logWindow->reload();
    }
}

void RotorLogbookPanel::setUploadQueue(QsoUploadQueue* queue)
{
    if (m_uploadQueue == queue) { return; }
    if (m_uploadQueue) { disconnect(m_uploadQueue, nullptr, this, nullptr); }
    m_uploadQueue = queue;
    if (!queue) { return; }

    connect(queue, &QsoUploadQueue::uploaded, this,
            [this](const QString& serviceId, const LogEntry& entry, bool) {
        // Record that it got through. Without this the answer is
        // forgotten at the next restart, and the only safe thing left
        // is to send everything again.
        if (serviceId == QLatin1String("qrz")) { markUploaded(entry); }
    });
    connect(queue, &QsoUploadQueue::qsoFinished, this,
            [this](const LogEntry& entry,
                   const QVector<QsoUploadQueue::Outcome>& outcomes) {
        bool anyFailed = false;
        for (const QsoUploadQueue::Outcome& o : outcomes) {
            if (!o.ok) { anyFailed = true; }
        }
        // Names the service: with two of them "upload failed" no longer
        // says which.
        setStatus(QsoUploadQueue::statusLine(entry.call, outcomes), anyFailed);
    });
    connect(queue, &QsoUploadQueue::backlogSent, this,
            [this](const QString& service, int sent, int stillWaiting,
                   const QString& lastError) {
        const auto contacts = [](int n) {
            return n == 1 ? QStringLiteral("1 earlier contact")
                          : QStringLiteral("%1 earlier contacts").arg(n);
        };
        if (sent > 0 && stillWaiting == 0) {
            setStatus(QStringLiteral("%1: %2 uploaded — nothing left waiting")
                          .arg(service, contacts(sent)));
        } else if (sent > 0) {
            setStatus(QStringLiteral("%1: %2 uploaded, %3 still waiting")
                          .arg(service, contacts(sent)).arg(stillWaiting), true);
        } else if (stillWaiting > 0) {
            setStatus(QStringLiteral("%1: %2 still waiting to be uploaded — %3")
                          .arg(service, contacts(stillWaiting),
                               lastError.isEmpty() ? QStringLiteral("will retry")
                                                   : lastError),
                      true);
        }
    });
}

void RotorLogbookPanel::refreshRecentList()
{
    m_recent->setRowCount(0);

    // Read the file back rather than remembering what was written: the
    // list then shows the file's truth, including contacts from earlier
    // sessions, and a write that silently failed would be visible.
    //
    // Shares AdifLog with the logbook window, so the dock and the window
    // can never disagree about what a record says.
    QVector<LogEntry> all = AdifLog::read(logbookPath());

    // Rebuild the worked-before index from the same read. Doing it here
    // rather than on a timer means the answer beside a callsign is
    // never older than the last contact written — including one
    // written by the logbook window or pulled in by an import.
    {
        WorkedBefore::PrefixResolver resolver;
        if (m_radio) {
            if (DxccColorProvider* dxcc = m_radio->dxccColorProvider()) {
                resolver = [dxcc](const QString& c) {
                    return dxcc->ctyDat().resolvePrimaryPrefix(c);
                };
            }
        }
        m_worked.rebuild(all, resolver);
    }

    // Sort rather than trusting file order. The file is written oldest
    // first, but an imported log need not be, and relying on position
    // would put the twelve oldest contacts here without any error.
    std::stable_sort(all.begin(), all.end(),
                     [](const LogEntry& a, const LogEntry& b) {
        if (a.timeOn.isValid() != b.timeOn.isValid()) {
            return a.timeOn.isValid();
        }
        return a.timeOn > b.timeOn;
    });

    const int shown = std::min<int>(all.size(), 12);
    m_recent->setRowCount(shown);
    for (int i = 0; i < shown; ++i) {
        const LogEntry& e = all.at(i);   // already newest first
        const QDateTime u = e.timeOn.toUTC();
        const QFont mono = Style::monoFont(m_recent->font(), Style::kFontSmall);
        auto* utc = new QTableWidgetItem(
            u.isValid() ? u.toString(QStringLiteral("hh:mm")) : QString{});
        utc->setFont(mono);   // Zahlen sind Monospace (Regel 4)
        m_recent->setItem(i, 0, utc);
        // HAUSSTIL.md Regel 7: "Unbekannt ist ein Strich, keine Null" --
        // 0.000 sieht wie eine echte Frequenz aus, ist aber nur der
        // Sentinel fuer "kein Funkgeraet beim Loggen verbunden".
        auto* freq = new QTableWidgetItem(
            e.freqMHz > 0.0 ? QString::number(e.freqMHz, 'f', 3)
                            : QStringLiteral("—"));
        freq->setFont(mono);
        m_recent->setItem(i, 1, freq);
        m_recent->setItem(i, 2, new QTableWidgetItem(e.call));
        m_recent->setItem(i, 3, new QTableWidgetItem(e.band));
        m_recent->setItem(i, 4, new QTableWidgetItem(
            e.submode.isEmpty() ? e.mode : e.submode));
    }
    m_recent->resizeColumnsToContents();
}

void RotorLogbookPanel::setUploadTargets(const QVector<QsoUploader*>& targets)
{
    m_uploadTargets = targets;
    if (m_logWindow) { m_logWindow->setUploaders(m_uploadTargets); }
}

void RotorLogbookPanel::showLogbook()
{
    openLogbookWindow();
}

QWidget* RotorLogbookPanel::logbuchFenster() const
{
    return m_logWindow.data();
}

void RotorLogbookPanel::openLogbookWindow()
{
    // One window, reused. Opening a second copy of the same file in two
    // windows is how one of them ends up writing over the other's edit.
    if (!m_logWindow) {
        m_logWindow = new LogbookWindow(logbookPath(), this);
        m_logWindow->setUploaders(m_uploadTargets);
        // The same QRZ client the panel uses, so the detail pane can
        // put a name and a portrait against a selected contact. Shared
        // rather than a second client: one session key, one queue, and
        // one place the credentials live.
        m_logWindow->setQrzClient(m_qrz);
        m_logWindow->setSatellites(m_satellites);
        // Derselbe Logbuch-Schluessel fuer den Weg zurueck (Sync QRZ).
        m_logWindow->setQrzLogbookUploader(m_uploader);
        connect(m_logWindow, &LogbookWindow::logChanged,
                this, &RotorLogbookPanel::refreshRecentList);

        // ── Point the beam at a logged contact ───────────────────────
        //
        // The logbook knows the bearing to every contact and this panel
        // owns the rotor; until now nobody had joined them, so working
        // a station again meant reading a number off one half of the
        // screen and typing it into the other.
        //
        // The dial follows too, not only the rotor: leaving it showing
        // the old heading while the antenna turns is the kind of small
        // lie that gets believed at three in the morning.
        //
        // ── One path, not two (2026-08-10) ───────────────────────────
        //
        // This used to refuse with a modal box when no rotator was
        // connected, while the panel's own Rotate button, one control
        // away, cheerfully turned the needle and said so in the status
        // line. Same request, two different programs.
        //
        // The box was the wrong half of that pair. It interrupted —
        // during a contact, which is when you look a station up — and
        // it withheld the part that needs no rotator at all: showing
        // where the station is. Setting the target and swinging the
        // globe is display, not motion.
        //
        // So the target goes on the dial and beginTurn() decides the
        // rest, exactly as it does for the button. Whatever it does, it
        // now does the same thing from both places.
        // Die Eingabezeile oben im Logbuch loggt ueber dieses Feld: eine
        // Datei, eine Doppelt-Regel, ein Hochladen (2026-09-26).
        m_logWindow->setRadio(m_radio);
        m_logWindow->setRotorBeamWidth(m_dial ? m_dial->beamWidth() : 40.0);
        m_logWindow->setRotorBearing(m_lastRotorAz);
        connect(m_logWindow, &LogbookWindow::logQsoRequested,
                this, &RotorLogbookPanel::logFromLogbook);
        // Schliessen melden, damit der Haken „Logbuch" in der Auswahl
        // mitgeht -- sonst ging das Logbuch bei jedem Start wieder auf
        // (Betreiber 2026-09-27).
        connect(m_logWindow, &QDialog::finished,
                this, &RotorLogbookPanel::logbookClosed);

        connect(m_logWindow, &LogbookWindow::turnRotorRequested, this,
                [this](double bearing, const QString& call) {
            Q_UNUSED(call);
            if (!m_dial) { return; }
            m_dial->setTargetBearing(bearing);
            beginTurn();   // same call workSpot() and the button make
        });

        // Hand cty.dat down to the map. Contacts logged before a QRZ
        // lookup filled in a locator — and most contacts in a log
        // imported from another program — have no GRIDSQUARE at all,
        // and without this the map draws nothing for them.
        if (m_radio) {
            if (DxccColorProvider* dxcc = m_radio->dxccColorProvider()) {
                // The Kennzahlen need the same table: entity per contact
                // for the country list and the DXCC / continent / zone counts.
                m_logWindow->setCtyDat(&dxcc->ctyDat());
                m_logWindow->setPositionFallback(
                    [dxcc](const QString& call, double& lat, double& lon) {
                        const QString prefix =
                            dxcc->ctyDat().resolvePrimaryPrefix(call);
                        if (prefix.isEmpty()) { return false; }
                        const DxccEntity* ent = dxcc->ctyDat()
                                                    .entityByPrefix(prefix);
                        if (!ent || !ent->hasLatLon) { return false; }
                        lat = ent->latitude;
                        lon = ent->longitude;
                        return true;
                    });
            }
        }
    }
    m_logWindow->reload();
    m_logWindow->show();
    m_logWindow->raise();
    m_logWindow->activateWindow();
}

void RotorLogbookPanel::hideLogbook()
{
    if (m_logWindow) { m_logWindow->hide(); }
}

void RotorLogbookPanel::raiseLogbookIfOpen()
{
    if (!m_logWindow || !m_logWindow->isVisible()) { return; }
    m_logWindow->raise();
    m_logWindow->activateWindow();
}

void RotorLogbookPanel::setStatus(const QString& text, bool warn)
{
    // Nur echte Wechsel, sonst schreibt die wiederholte Meldung das
    // Protokoll voll (der Zustand wird bei jedem Abfragedurchgang
    // gesetzt, nicht nur wenn er sich aendert).
    if (text != m_letzterStatus) {
        m_letzterStatus = text;
        if (warn) { qCWarning(lcRotor).noquote() << "Rotor:" << text; }
        else      { qCInfo(lcRotor).noquote()    << "Rotor:" << text; }
    }
    m_status->setText(text);
    m_status->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }"
    ).arg(QString::fromLatin1(warn ? Style::kAmberText
                                   : Style::kTextSecondary)));
}

// ── QRZ wiring and locator maths ────────────────────────────────────
//
// Lost the same way as the logbook's editing slots: a scripted edit
// dropped the tail of a region. Restored verbatim from ea7b31c2.

void RotorLogbookPanel::wireQrz()
{
    if (!m_qrz) {
        m_lookupBtn->setEnabled(false);
        m_lookupBtn->setToolTip(
            QStringLiteral("Add your QRZ account in Tools to enable lookups"));
        return;
    }

    connect(m_qrz, &QrzClient::lookupSucceeded, this,
            [this](const QString& call, const CallsignInfo& info) {
        // A queued reply for a callsign the operator has already typed
        // past must not overwrite the card.
        // Remember it whatever the operator has typed since — the next
        // time this callsign comes round, the answer is already here.
        m_qrzCache.insert(call, info);
        meldeAuskunft(call, RufzeichenAuskunft::Stand::Da, info);

        if (Callsigns::normalized(m_callEdit->text()) != call) { return; }
        m_lastInfo = info;

        adoptGridFromQrz(info);
        updateFlagFor(info.call);
        setStationLine(stationText(info));
        showStationVisuals(info);
        setStatus(QString{});

        // Show the answer as a picture too (2026-08-10, operator wish):
        // a successful lookup brings the globe up and swings it along
        // the path to the station. The dial is one click away on the
        // same button.
        if (m_dial->hasTarget()) {
            if (m_globeBtn && !m_globeBtn->isChecked()) {
                m_globeBtn->setChecked(true);   // switches the stack
            }
            m_globe->lookAlongBearing(m_dial->targetBearing());
        }
    });

    connect(m_qrz, &QrzClient::lookupFailed, this,
            [this](const QString& call, QrzClient::Error err,
                   const QString& msg) {
        meldeAuskunft(call, err == QrzClient::Error::NotFound
                                ? RufzeichenAuskunft::Stand::NichtGefunden
                                : RufzeichenAuskunft::Stand::Fehler);
        if (Callsigns::normalized(m_callEdit->text()) != call) { return; }
        m_lastInfo = CallsignInfo{};
        switch (err) {
        case QrzClient::Error::NotFound:
            setStatus(QStringLiteral("%1 isn't in QRZ — country estimate kept")
                          .arg(call), true);
            break;
        case QrzClient::Error::AuthFailed:
            setStatus(QStringLiteral("QRZ rejected the login"), true);
            break;
        case QrzClient::Error::Network:
            setStatus(QStringLiteral("Couldn't reach QRZ — estimate kept"), true);
            break;
        case QrzClient::Error::Provider:
            setStatus(msg.isEmpty() ? QStringLiteral("QRZ returned no data")
                                    : msg, true);
            break;
        }
    });

    // The upload answers no longer arrive here: see setUploadQueue().
}

void RotorLogbookPanel::applyLocators()
{
    const QString mine = m_myGrid->text().trimmed().toUpper();
    const QString dx   = m_dxGrid->text().trimmed().toUpper();
    AppSettings::instance().setValue(kMyGridKey, mine);

    const QString needsInput = Style::lineEditStyle()
        + QStringLiteral("QLineEdit { border: 1px solid %1; }")
              .arg(Style::kAmberBorder);
    const bool myOk = isValidGridSquare(mine);
    m_myGrid->setStyleSheet(myOk ? Style::lineEditStyle()
                                 : needsInput);
    if (!myOk) {
        setStatus(mine.isEmpty()
            ? QStringLiteral("Enter your own locator to get bearings")
            : QStringLiteral("%1 is not a valid locator").arg(mine), true);
        return;
    }
    if (!isValidGridSquare(dx)) {
        m_kurzGrad = -1.0;
        aktualisiereAblesung(-1.0, -1.0);
        return;
    }

    const double km  = calculateDistanceKm(mine, dx);
    const double deg = calculateBearingInDegrees(mine, dx);
    m_dial->setTargetBearing(deg);
    m_kurzGrad = deg;
    aktualisiereAblesung(deg, km);
    updateGlobeFromLocators();
    updateSolarLine();
    setStatus(QStringLiteral("%1 km · %2° %3")
                  .arg(km, 0, 'f', 0).arg(deg, 0, 'f', 0)
                  .arg(compassPoint(deg)));
}

void RotorLogbookPanel::updateGlobeFromLocators()
{
    if (!m_globe) { return; }
    const QString mine = m_myGrid->text().trimmed().toUpper();
    const QString dx   = m_dxGrid->text().trimmed().toUpper();

    if (isValidGridSquare(mine)) {
        double lat = 0.0, lon = 0.0;
        calculateLatLonFromGridSquare(mine, lat, lon);
        m_globe->setHome(lat, lon);
    }
    if (isValidGridSquare(dx)) {
        double lat = 0.0, lon = 0.0;
        calculateLatLonFromGridSquare(dx, lat, lon);
        m_globe->setTarget(lat, lon);
        m_dxLat = lat; m_dxLon = lon; m_hasDxPos = true;
    } else {
        m_globe->clearTarget();
    }
    // The sun moves; a terminator computed once at startup would be
    // visibly wrong by evening.
    m_globe->useCurrentSubsolarPoint();
}

void RotorLogbookPanel::zeigeWeltbildMenue(const QPoint& at)
{
    ScopedChildWidget<QMenu> menuOwner(window());   // stirbt nicht mit dem Rahmen, siehe ScopedChildWidget.h
    const QPointer<RotorLogbookPanel> self(this);   // Menue am Fenster: am nativen Feld bekam es keinen Fensterbezug (2026-09-27)
    QMenu& menu = *menuOwner.get();
    QAction* small = menu.addAction(
        QStringLiteral("Download from NASA — 2048 × 1024 (about 1 MB)"));
    QAction* large = menu.addAction(
        QStringLiteral("Download from NASA — 5400 × 2700 (about 7 MB)"));
    menu.addSeparator();
    QAction* pick = menu.addAction(QStringLiteral("Choose a file…"));

    menu.addSeparator();
    QAction* schematic = nullptr;
    QAction* landColour = nullptr;
    QAction* styleNatural = nullptr;
    QAction* styleMuted = nullptr;
    QAction* styleNight = nullptr;
    QAction* styleCrisp = nullptr;
    if (m_globe->hasTexture()) {
        schematic = menu.addAction(QStringLiteral("Use schematic globe"));

        // Betreiber 2026-09-02: drei Tonwertkurven gezeigt, alle drei
        // gefielen -- also waehlbar statt entschieden. Nur sichtbar,
        // wenn ein echtes Foto geladen ist; der schematische Globus
        // hat sein eigenes, unabhaengiges Glimmen.
        QMenu* styleMenu = menu.addMenu(QStringLiteral("Photo style"));
        const WorldTexture::Style cur = WorldTexture::style();
        styleNatural = styleMenu->addAction(QStringLiteral("Natural"));
        styleNatural->setCheckable(true);
        styleNatural->setChecked(cur == WorldTexture::Style::Natural);
        styleMuted = styleMenu->addAction(QStringLiteral("Muted"));
        styleMuted->setCheckable(true);
        styleMuted->setChecked(cur == WorldTexture::Style::Muted);
        styleNight = styleMenu->addAction(QStringLiteral("Night wash"));
        styleNight->setCheckable(true);
        styleNight->setChecked(cur == WorldTexture::Style::NightWash);
        styleCrisp = styleMenu->addAction(QStringLiteral("Crisp"));
        styleCrisp->setCheckable(true);
        styleCrisp->setChecked(cur == WorldTexture::Style::Crisp);
    } else {
        landColour = menu.addAction(QStringLiteral("Land colour…"));
    }

    QAction* chosen = menu.exec(at);
    // Das Elternteil kann waehrend exec() gestorben sein — dann ist
    // auch das Menue weg und `this` eine Leiche. Siehe ScopedChildWidget.h.
    if (!menuOwner || !self) { return; }
    if (chosen == small) {
        downloadWorldImage(smallCandidates(), 0,
                           QStringLiteral("2048 × 1024"));
    } else if (chosen == large) {
        downloadWorldImage(largeCandidates(), 0,
                           QStringLiteral("5400 × 2700"));
    } else if (chosen == pick) {
        chooseWorldImage();
    } else if (chosen == schematic) {
        m_globe->clearTexture();
        setStatus(QStringLiteral("Schematic globe"));
    } else if (chosen == landColour) {
        const QColor picked = QColorDialog::getColor(
            m_globe->landColour(), this, tr("Land colour"), QColorDialog::DontUseNativeDialog);
        if (picked.isValid()) { m_globe->setLandColour(picked); }
    } else if (chosen == styleNatural) {
        WorldTexture::setStyle(WorldTexture::Style::Natural);
    } else if (chosen == styleMuted) {
        WorldTexture::setStyle(WorldTexture::Style::Muted);
    } else if (chosen == styleNight) {
        WorldTexture::setStyle(WorldTexture::Style::NightWash);
    } else if (chosen == styleCrisp) {
        WorldTexture::setStyle(WorldTexture::Style::Crisp);
    }
    }


// ── Entwurf R (2026-11-12) ──────────────────────────────────────────

void RotorLogbookPanel::aktualisiereAblesung(double kurzGrad, double km)
{
    if (!m_roKurz) { return; }
    if (kurzGrad < 0.0) {
        m_roKurz->setText(QStringLiteral("—"));
        m_roLang->setText(QStringLiteral("—"));
        m_roWeite->setText(QStringLiteral("—"));
        if (m_rotorBand) { m_rotorBand->setStation(-1.0); }
    } else {
        const double lang = std::fmod(kurzGrad + 180.0, 360.0);
        m_roKurz->setText(QStringLiteral("%1°").arg(kurzGrad, 0, 'f', 0));
        m_roLang->setText(QStringLiteral("%1°").arg(lang, 0, 'f', 0));
        m_roWeite->setText(QStringLiteral("%1 km").arg(km, 0, 'f', 0));
        if (m_rotorBand) { m_rotorBand->setStation(kurzGrad); }
    }
    aktualisiereDrehknoepfe();
}

void RotorLogbookPanel::aktualisiereDrehknoepfe()
{
    if (!m_rotateBtn || !m_lpBtn) { return; }
    const RotorBereich bereich = RotorBereich::load();
    if (m_dial->hasTarget()) {
        const double ziel = m_dial->targetBearing();
        const bool frei = bereich.erlaubt(ziel);
        m_rotateBtn->setText(frei ? QStringLiteral("Rotate %1°").arg(ziel, 0, 'f', 0)
                                  : QStringLiteral("%1° — blocked").arg(ziel, 0, 'f', 0));
        m_rotateBtn->setEnabled(frei);
        m_rotateBtn->setToolTip(frei ? QStringLiteral("Turn the antenna to %1°").arg(ziel, 0, 'f', 0)
                                     : QStringLiteral("%1° is outside the rotor's allowed range "
                                                      "(Rotor setup)").arg(ziel, 0, 'f', 0));
    } else {
        m_rotateBtn->setText(QStringLiteral("Rotate"));
        m_rotateBtn->setEnabled(true);
    }
    if (m_kurzGrad >= 0.0) {
        const double lang = std::fmod(m_kurzGrad + 180.0, 360.0);
        const bool frei = bereich.erlaubt(lang);
        m_lpBtn->setText(frei ? QStringLiteral("LP %1°").arg(lang, 0, 'f', 0)
                              : QStringLiteral("LP %1° — blocked").arg(lang, 0, 'f', 0));
        m_lpBtn->setEnabled(frei);
    } else {
        m_lpBtn->setText(QStringLiteral("LP"));
        m_lpBtn->setEnabled(false);
    }
}

void RotorLogbookPanel::zeigeOptionen()
{
    ScopedChildWidget<QMenu> menuOwner(window());   // stirbt nicht mit dem Rahmen, siehe ScopedChildWidget.h
    const QPointer<RotorLogbookPanel> self(this);
    QMenu& menu = *menuOwner.get();
    menu.addSection(QStringLiteral("Rotor / Log"));
    QAction* rose = menu.addAction(QStringLiteral("Show the rose"));
    QAction* kugel = menu.addAction(QStringLiteral("Show the globe"));
    rose->setCheckable(true);
    kugel->setCheckable(true);
    const bool globus = m_globeBtn && m_globeBtn->isChecked();
    rose->setChecked(!globus);
    kugel->setChecked(globus);
    QAction* welt = menu.addAction(QStringLiteral("World image…"));
    menu.addSeparator();
    QAction* rotor = menu.addAction(QStringLiteral("Rotor setup…"));
    const QString mein = m_myGrid->text().trimmed().toUpper();
    QAction* locator = menu.addAction(mein.isEmpty()
        ? QStringLiteral("Your locator…")
        : QStringLiteral("Your locator: %1…").arg(mein));
    QAction* foto = menu.addAction(QStringLiteral("Show QRZ photos"));
    foto->setCheckable(true);
    foto->setChecked(AppSettings::instance().value(kShowPhotoKey, true).toBool());
    menu.addSeparator();
    QAction* buch = menu.addAction(QStringLiteral("Open logbook…"));

    const QPoint at = m_optionenBtn->mapToGlobal(
        QPoint(m_optionenBtn->width() - menu.sizeHint().width(), m_optionenBtn->height() + 2));
    QAction* gewaehlt = menu.exec(at);
    if (!menuOwner || !self) { return; }
    if (gewaehlt == rose) {
        m_globeBtn->setChecked(false);
    } else if (gewaehlt == kugel) {
        m_globeBtn->setChecked(true);
    } else if (gewaehlt == welt) {
        if (!m_globeBtn->isChecked()) { m_globeBtn->setChecked(true); }
        zeigeWeltbildMenue(at);
    } else if (gewaehlt == rotor) {
        openRotorSetupDialog();
    } else if (gewaehlt == locator) {
        bool ok = false;
        const QString neu = QInputDialog::getText(this, QStringLiteral("Your locator"),
            QStringLiteral("Your own Maidenhead locator (4 or 6 characters):"),
            QLineEdit::Normal, mein, &ok).trimmed().toUpper();
        if (ok && !neu.isEmpty()) { m_myGrid->setText(neu); }
    } else if (gewaehlt == foto) {
        AppSettings::instance().setValue(kShowPhotoKey, foto->isChecked());
        if (!foto->isChecked()) {
            m_photo->showPlaceholder(QString{});
            m_photo->setVisible(false);
        }
        setStatus(foto->isChecked() ? QStringLiteral("QRZ photos on — from the next lookup")
                                    : QStringLiteral("QRZ photos off"));
    } else if (gewaehlt == buch) {
        openLogbookWindow();
    }
}

} // namespace Longpath
