// =================================================================
// src/gui/LogbookWindow.cpp  (Longpath)
// =================================================================
//
// Longpath-original — see LogbookWindow.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-09-21 — Sync QRZ: das QRZ-Logbuch abholen und wie einen Import
//                 zusammenfuehren; getroffene Kontakte gelten als bei QRZ
//                 vorhanden. Martin Fischer, AI-assisted via Anthropic Claude.
// =================================================================

#include "LogbookWindow.h"
#include "gui/ScopedChildWidget.h"
#include "core/LogbookStats.h"
#include "gui/widgets/LogbookStatsWidget.h"

#include "core/AdifLog.h"
#include "core/BeamHeading.h"
#include "core/QsoConfirmation.h"
#include "core/AppSettings.h"
#include "core/CallsignInfo.h"
#include "core/QrzLogbookFetcher.h"
#include "core/QrzLogbookUploader.h"
#include "core/QsoUploader.h"
#include "gui/QsoMapWindow.h"
#include "gui/widgets/FlowLayout.h"
#include "gui/FramelessResizer.h"
#include "gui/MacFloatingWindowBehavior.h"
#include "gui/StyleConstants.h"
#include "gui/WindowChrome.h"
#include "gui/WindowPlacement.h"
#include "gui/widgets/QsoDetailPane.h"
#include "gui/widgets/RotorBandWidget.h"
#include "core/Maidenhead.h"
#include "core/CtyDatParser.h"
#include "core/LogbuchBilder.h"
#include "core/LogbuchGesamtbilder.h"
#include "core/SolarFluxArchiv.h"
#include "core/SotaGipfel.h"
#include "core/SotaNachtrag.h"
#include "gui/SotaNachtragDialog.h"
#include "core/RotorBereich.h"
#include "core/RundumLauf.h"
#include "core/TransverterSetup.h"
#include "gui/widgets/LogbuchBilderWidgets.h"
#include "gui/widgets/LogbuchGesamtbilderWidgets.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QSplitter>

#include <QAccessible>
#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QScreen>
#include <QGuiApplication>
#include <QPointer>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QDateEdit>
#include <QDateTimeEdit>
#include <QCursor>
#include <QHash>
#include <QMenu>
#include <QMoveEvent>
#include <QResizeEvent>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTimer>
#include <QTextStream>
#include <QTimeZone>
#include <QButtonGroup>
#include <QGridLayout>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>

#include <algorithm>

namespace Longpath {

namespace {

enum Column {
    ColDate = 0, ColTime, ColCall, ColFreq, ColBand, ColMode,
    ColSent, ColRcvd, ColName, ColQth, ColCountry,
    ColGrid, ColDistance, ColQrz, ColConfirmed, ColComment, ColumnCount
};

QStringList headerLabels()
{
    return {QStringLiteral("Date"),  QStringLiteral("UTC"),
            QStringLiteral("Call"),  QStringLiteral("MHz"),
            QStringLiteral("Band"),
            QStringLiteral("Mode"),  QStringLiteral("Sent"),
            QStringLiteral("Rcvd"),  QStringLiteral("Name"),
            QStringLiteral("QTH"),   QStringLiteral("Country"),
            QStringLiteral("Grid"),  QStringLiteral("km"),
            QStringLiteral("QRZ"),   QStringLiteral("QSL"),
            QStringLiteral("Comment")};
}

// Newest first. Contacts with no timestamp sort last rather than being
// scattered through the list by a zero date.
bool newerFirst(const LogEntry& a, const LogEntry& b)
{
    if (a.timeOn.isValid() != b.timeOn.isValid()) {
        return a.timeOn.isValid();
    }
    return a.timeOn > b.timeOn;
}

} // namespace

LogbookWindow::LogbookWindow(const QString& adifPath, QWidget* parent)
    : QDialog(parent), m_path(adifPath)
{
    setWindowTitle(QStringLiteral("Logbook"));
    // Rahmenlos wie jedes andere Longpath-Fenster. Bis hierher war das
    // Logbuch das einzige mit dem Rahmen des Betriebssystems -- und damit
    // das einzige mit runden Ecken, waehrend alles andere eckig ist.
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    // ── Vor die schwebenden Fenster, nicht dahinter (2026-11-15) ─────
    //
    // Betreiber: „es muss immer das neu hinzugefuegte im vordergrund
    // sein!!!!"
    //
    // Dritter Fall derselben Sache. Das Antennenfenster bekam das am
    // 2026-09-01 („darf sich nicht hinter anderen Fenstern
    // verstecken"), der Setup-Dialog am 2026-09-17 („ich wollte auf
    // settings gehen, fenster oeffnet sich jedoch im hintergrund") --
    // das Logbuch nicht, und es ist das Fenster, das er am oeftesten
    // aufmacht.
    //
    // Der Grund ist jedes Mal derselbe: die schwebenden Fenster
    // (Panadapter, Rotor/Log, Applets) sind Qt::Tool, auf macOS ein
    // NSPanel auf einer HOEHEREN Fensterebene. Ein raise() hebt nur
    // innerhalb der eigenen Ebene und ist dagegen machtlos. Also
    // dieselbe Ebene und dasselbe Space-Verhalten wie sie.
    setWindowFlag(Qt::Tool, true);
    enableFullScreenAuxiliaryBehavior(this);
    // Not modal: you look things up in the log *while* working a
    // station, which is exactly when a modal dialog would be in the way.
    setModal(false);
    resize(1240, 620);
    // Read before the pane is built so the first selection already has
    // whatever QRZ said last time — otherwise the first contact clicked
    // after every start looks like a station nobody has ever heard of.
    m_callCache.load();
    buildUi();
    // Kein Knopf ist Standardknopf. In einem QDialog ist jeder QPushButton
    // "autoDefault": Return in einem Feld drueckt dann zusaetzlich den
    // ersten Knopf -- Return in der Suche oeffnete "Edit..." (gefunden
    // 2026-09-26 an einem haengenden Test: modaler Bearbeiten-Dialog).
    for (QPushButton* b : findChildren<QPushButton*>()) { b->setAutoDefault(false); b->setDefault(false); }
    // Ziehen an der Leiste, Groesse an den Kanten und am Griff unten
    // rechts -- dieselben drei Teile, die ToolWindow installiert. Der
    // obere Streifen gehoert dem Ziehen, sonst schnappt der Resizer den
    // Griff weg (AetherSDR #4266).
    FramelessResizer::install(this, 6, m_titleBar->height());
    attachResizeGrip(this);

    restoreHeaderState();
    restoreSplitState();
    restoreGeometryState();
    reload();

    // Eine Seite: Karte und Kennzahlen zeigen, wie beim letzten Mal —
    // beim ersten Mal beides an, das ist der Sinn der Sache.
    {
        // Entwurf C: die Arbeitsweise vom letzten Mal, beim ersten Mal „Log".
        const QString m = AppSettings::instance()
            .value(QStringLiteral("LogbookMode"), QStringLiteral("Log")).toString();
        setModus(m == QStringLiteral("Browse") ? ModusBrowse
                 : m == QStringLiteral("Statistics") ? ModusStats : ModusLog);
    }
}

void LogbookWindow::closeEvent(QCloseEvent* event)
{
    // Persist position/size on close so the next open restores it,
    // instead of Qt's default QDialog(parent) placement.
    saveGeometryState();
    QDialog::closeEvent(event);
}

void LogbookWindow::moveEvent(QMoveEvent* event)
{
    QDialog::moveEvent(event);
    saveGeometryState();
}

void LogbookWindow::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    if (m_settling) {
        m_settleTarget = m_settleTarget.boundedTo(event->size());
    }
    saveGeometryState();
}

void LogbookWindow::saveGeometryState()
{
    AppSettings::instance().setValue(
        QStringLiteral("LogbookGeometryState"), saveGeometry());
}

void LogbookWindow::restoreGeometryState()
{
    const QByteArray st = AppSettings::instance()
        .value(QStringLiteral("LogbookGeometryState")).toByteArray();
    // Empty on a first run, or a saved state from before this window
    // had a screen of its own to remember -- the resize(1240, 620)
    // above already set a sane default in that case.
    if (!st.isEmpty()) { m_geometryRestored = restoreGeometry(st); }
}

void LogbookWindow::setVisible(bool visible)
{
    // ── Die gespeicherte Groesse halten (2026-09-26) ─────────────────
    //
    // Beim ersten Zeigen kennen die umbrechenden Leisten (Knopfzeilen
    // der Karte, Werkzeugleisten) noch nicht ihre wirkliche Breite. Die
    // Kartenspalte steht fuer einen Durchgang schmal -- das kleine
    // Rotor-Radar belegt noch Platz, die Aufteilung ist noch nicht
    // angekommen --, die Knoepfe brauchen dort viele Zeilen, und das
    // Logbuch meldet fuer diesen Augenblick bis zu 931 px Mindesthoehe.
    // Qt zieht das Fenster darauf auf; einen Takt spaeter ist die
    // Mindesthoehe wieder 597, aber das Fenster schrumpft nie von
    // selbst zurueck. Jede gespeicherte Hoehe ging so verloren, und auf
    // dem Laptop schob macOS das Fenster dabei nach oben.
    //
    // Darum: was vor dem ersten Zeigen vorgesehen war, danach noch
    // einmal anwenden -- nie kleiner als der Inhalt dann braucht. Die
    // Lage nur, wenn sie wirklich wiederhergestellt wurde; sonst bleibt
    // Qts Platzierung neben dem Hauptfenster.
    //
    // Mehrmals, nicht einmal: im Test ist die Mindesthoehe nach 20 ms
    // eingeschwungen, im laufenden Programm (Karte, Karteikarte,
    // Kennzahlen) spaeter -- live nach 50 ms noch 776 statt 637.
    //
    // Nur zuruecknehmen, was aufgezogen wurde -- nie vergroessern
    // (2026-09-27): wer das Fenster in der ersten Sekunde kleiner zieht,
    // behaelt es so. Vorher stellte der naechste Takt die vorgesehene
    // Groesse wieder her (auf der CI: tst_logbook_toolbar_wraps).
    //
    // Ziel ist die kleinste Groesse, die das Fenster in dieser Zeit hatte
    // (m_settleTarget, gefuehrt in resizeEvent): zieht Qt NACH einer
    // Verkleinerung noch einmal auf, geht es auf die Verkleinerung
    // zurueck, nicht nur auf die vorgesehene Groesse (unter Last auf
    // cocoa: 900x650 gewollt, 900x698 aufgezogen, 2026-09-27).
    const bool first = visible && !m_shownOnce;
    const QRect intended = geometry();
    if (first) {
        m_settling = true;
        m_settleTarget = intended.size();
    }
    QDialog::setVisible(visible);
    if (!first) { return; }
    m_shownOnce = true;

    // ── Laufend statt an drei Punkten (2026-10-16) ───────────────────
    //
    // Hier standen drei feste Takte: 50, 300, 1000 ms. Jeder nahm
    // zurueck bis m_settleTarget.expandedTo(minimumSizeHint()) — und
    // war die Mindesthoehe beim LETZTEN noch nicht eingeschwungen, war
    // diese Grenze selbst der aufgezogene Wert. Dann unterblieb die
    // Korrektur, und danach lief kein Takt mehr. Im vollen Durchgang
    // vom 2026-10-16 stand das Fenster unter -j8-Last auf 780 statt
    // 700; allein gefahren war derselbe Lauf gruen.
    //
    // Der Kommentar oben sagt es selbst: „im laufenden Programm
    // (Karte, Karteikarte, Kennzahlen) spaeter — live nach 50 ms noch
    // 776 statt 637". Drei Zeitpunkte zu raten, an denen das vorbei
    // ist, geht auf einer besetzten Maschine schief.
    m_settleIntended     = intended;
    m_settleWithPosition = m_geometryRestored;
    m_settleLastHint     = QSize();
    m_settleUhr.start();
    if (!m_settleTimer) {
        m_settleTimer = new QTimer(this);
        connect(m_settleTimer, &QTimer::timeout,
                this, &LogbookWindow::onSettleTick);
    }
    m_settleTimer->start(kSettleTickMs);
}

void LogbookWindow::onSettleTick()
{
    if (!isVisible() || isMaximized() || isFullScreen()) {
        stopSettling();
        return;
    }

    const QSize hint = minimumSizeHint();
    const bool eingeschwungen = (hint == m_settleLastHint);
    m_settleLastHint = hint;

    // Nur zuruecknehmen, was aufgezogen wurde — nie vergroessern. Ziel
    // ist die kleinste Groesse, die das Fenster in dieser Zeit hatte
    // (m_settleTarget, gefuehrt in resizeEvent), nie kleiner als der
    // Inhalt jetzt braucht.
    const QSize bound = m_settleTarget.expandedTo(hint);
    const QSize jetzt = size();
    const QSize soll  = jetzt.boundedTo(bound);
    const bool nichtsZurueckzunehmen = (soll == jetzt);
    if (!nichtsZurueckzunehmen) {
        setGeometry(m_settleWithPosition
                        ? QRect(m_settleIntended.topLeft(), soll)
                        : QRect(pos(), soll));
    }

    const qint64 seit = m_settleUhr.elapsed();

    // Die erste Sekunde gehoert dem Betreiber: wer in ihr kleiner
    // zieht, behaelt es, und dafuer muss der Takt noch laufen. Erst
    // danach darf Ruhe einkehren — und auch dann nur, wenn die
    // Mindestgroesse wirklich steht und nichts mehr aufgezogen war.
    if (seit >= kSettleMinMs && eingeschwungen && nichtsZurueckzunehmen) {
        stopSettling();
        return;
    }
    // Und irgendwann ist Schluss, damit ein Fenster, dessen
    // Mindestgroesse nie zur Ruhe kommt, nicht ewig gefangen bleibt.
    if (seit >= kSettleMaxMs) {
        stopSettling();
    }
}

void LogbookWindow::stopSettling()
{
    if (m_settleTimer) { m_settleTimer->stop(); }
    m_settling = false;
}

void LogbookWindow::restoreSplitState()
{
    // Je Teiler ein Schluessel. Gibt es keinen (erster Start, oder ein
    // Zustand aus einem Bau mit anderen Spalten), die Breiten aus Entwurf C.
    const struct { QSplitter* teiler; const char* key; QList<int> vorgabe; } alle[] = {
        {m_logSplit,     "LogbookLogSplit",    {300, 860, 300}},
        {m_logMitte,     "LogbookLogMitte",    {520, 210}},
        {m_split,        "LogbookBrowseSplit", {240, 900, 320}},
        {m_browseRechts, "LogbookBrowseRight", {200, 560}},
    };
    for (const auto& a : alle) {
        if (!a.teiler) { continue; }
        const QByteArray st = AppSettings::instance()
            .value(QString::fromLatin1(a.key)).toByteArray();
        if (st.isEmpty() || !a.teiler->restoreState(st)) { a.teiler->setSizes(a.vorgabe); }
        // Jeder Teiler merkt sich selbst (restoreSplitState laeuft einmal,
        // aus dem Konstruktor).
        QSplitter* t = a.teiler;
        const QString key = QString::fromLatin1(a.key);
        connect(t, &QSplitter::splitterMoved, this, [t, key](int, int) {
            AppSettings::instance().setValue(key, t->saveState());
        });
    }
}

void LogbookWindow::setQrzClient(QrzClient* qrz)
{
    m_qrz = qrz;
    if (m_detail) { m_detail->setQrzClient(qrz); }
    if (m_map)    { m_map->setQrzClient(qrz); }
}

// ── UI ──────────────────────────────────────────────────────────────

void LogbookWindow::buildUi()
{
    setStyleSheet(QStringLiteral("QDialog { background: %1; }")
                      .arg(QString::fromLatin1(Style::kAppBg)));

    // ── Eigene Titelleiste, buendig am Rand ──────────────────────────
    //
    // Das aeussere Layout traegt KEINE Raender, damit die Leiste von Kante
    // zu Kante geht wie bei jedem anderen Longpath-Fenster. Die bisherigen
    // zehn Punkte Rand sitzen jetzt am Inhalt darunter -- `col` bleibt
    // dadurch genau das, was es vorher war, und alles Folgende
    // unveraendert.
    auto* aussen = new QVBoxLayout(this);
    aussen->setContentsMargins(0, 0, 0, 0);
    aussen->setSpacing(0);

    m_titleBar = new WindowTitleBar(tr("Logbook"), this);
    // Das Logbuch gehoert in kein Dock -- das × schliesst, und der
    // Andockwunsch (Doppelklick auf die Leiste) tut dasselbe, statt ins
    // Leere zu laufen.
    connect(m_titleBar, &WindowTitleBar::closeRequested, this, &QDialog::close);
    connect(m_titleBar, &WindowTitleBar::dockRequested, this, &QDialog::close);
    m_titleBar->setLockKey(QStringLiteral("Tool_Logbook"));
    aussen->addWidget(m_titleBar);

    auto* inhalt = new QWidget(this);
    aussen->addWidget(inhalt, 1);

    auto* col = new QVBoxLayout(inhalt);
    col->setContentsMargins(10, 10, 10, 10);
    col->setSpacing(8);

    // Search + Werkzeugleiste. Ein FlowLayout, kein QHBoxLayout: elf
    // Elemente nebeneinander machten das Fenster ueber 1000 px breit
    // und liessen es nicht schmaler ziehen — die Leiste bricht jetzt in
    // eine zweite Zeile um (Betreiber, 2026-09-21). Das Suchfeld nimmt,
    // was in seiner Zeile uebrig bleibt.
    // ── Drei Arbeitsweisen (2026-11-12, Entwurf C) ───────────────────
    //
    // Betreiber: „ich möchte dies überarbeiten und übersichtlicher
    // gestalten", dann „bitte doch C". Oben waehlt man, was man gerade
    // tut: „Log" ist der Platz zum Loggen (grosse Eingabe, Station, Weg,
    // was mit diesem Land noch fehlt, darunter die letzten QSOs),
    // „Browse" das Archiv (alle Filter links, die Liste ueber die ganze
    // Breite, rechts die Auswahl), „Statistics" die Kennzahlen ueber das
    // ganze Fenster. Datei- und Online-Arbeit liegt hinter dem Zahnrad in
    // der Titelleiste, wo jedes Longpath-Fenster seine Optionen hat.
    auto* top = new FlowLayout(nullptr, 6, 6);
    {
        auto* box = new QWidget(this);
        auto* h = new QHBoxLayout(box);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(2);
        m_modusGruppe = new QButtonGroup(this);
        m_modusGruppe->setExclusive(true);
        const struct { const char* text; const char* name; const char* tip; } modi[] = {
            {"Log", "logbookModeLog",
             "Log a contact: the station, the path, the rotor and what you still "
             "need with this country — and the last contacts below"},
            {"Browse", "logbookModeBrowse",
             "The whole log: search and filter on the left, the list in the "
             "middle, the selected contact on the right"},
            {"Statistics", "logbookModeStats",
             "Contacts per band, mode and month, countries and awards — for what "
             "the filters in Browse show"},
        };
        for (int k = 0; k < 3; ++k) {
            auto* b = new QPushButton(QString::fromUtf8(modi[k].text), box);
            b->setObjectName(QString::fromLatin1(modi[k].name));
            b->setToolTip(QString::fromUtf8(modi[k].tip));
            b->setCheckable(true);
            b->setFocusPolicy(Qt::NoFocus);
            b->setMinimumHeight(30);
            b->setStyleSheet(Style::buttonBaseStyle() + Style::blueCheckedStyle()
                             + QStringLiteral("QPushButton { padding: 4px 16px; font-size: 13px; }"));
            m_modusGruppe->addButton(b, k);
            h->addWidget(b);
        }
        top->addWidget(box);
    }
    m_modusHinweis = new QLabel(QString{}, this);
    m_modusHinweis->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; padding-left: 8px; }")
                                      .arg(QString::fromLatin1(Style::kTextScale)));
    top->addWidget(m_modusHinweis);
    top->addStretch();

    m_editBtn   = new QPushButton(QStringLiteral("Edit…"), this);
    m_deleteBtn = new QPushButton(QStringLiteral("Delete"), this);
    // Hochladen und Abholen stehen hinter dem Zahnrad. Die Knoepfe bleiben
    // als Zustandsanzeige: waehrend einer Uebertragung erscheinen sie mit
    // ihrem Fortschritt in der Leiste und verschwinden danach wieder.
    m_uploadBtn = new QPushButton(QStringLiteral("Upload…"), this);
    m_uploadBtn->setToolTip(
        QStringLiteral("Send the selected contacts to a logging service"));
    m_uploadBtn->setVisible(false);
    m_syncBtn = new QPushButton(QStringLiteral("Sync QRZ"), this);
    m_syncBtn->setToolTip(QStringLiteral(
        "Fetch your QRZ logbook and merge it into this log: contacts "
        "logged there are added, confirmations and QRZ log ids fill in, "
        "nothing you can see and edit here is overwritten. Needs the "
        "logbook key from Tools > QRZ."));
    m_syncBtn->setVisible(false);
    for (QPushButton* b : {m_uploadBtn, m_syncBtn}) {
        b->setStyleSheet(Style::buttonBaseStyle());
        top->addWidget(b);
    }
    // Bearbeiten und Loeschen gelten der Auswahl -- sie stehen in Browse.
    m_auswahlBox = new QWidget(this);
    {
        auto* h = new QHBoxLayout(m_auswahlBox);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(6);
        for (QPushButton* b : {m_editBtn, m_deleteBtn}) {
            b->setStyleSheet(Style::buttonBaseStyle());
            h->addWidget(b);
        }
    }
    top->addWidget(m_auswahlBox);
    col->addLayout(top);
    connect(m_modusGruppe, &QButtonGroup::idClicked, this, &LogbookWindow::setModus);
    connect(m_syncBtn, &QPushButton::clicked,
            this, &LogbookWindow::syncFromQrz);
    connect(m_uploadBtn, &QPushButton::clicked, this, [this]() {
        uploadMenuAt(m_uploadBtn->mapToGlobal(QPoint(0, m_uploadBtn->height())));
    });

    // Die Eingabe: nur im Modus „Log".
    m_eingabe = new QWidget(this);
    m_eingabe->setObjectName(QStringLiteral("logbookEntryArea"));
    {
        auto* ev = new QVBoxLayout(m_eingabe);
        ev->setContentsMargins(0, 0, 0, 0);
        ev->setSpacing(6);
        buildEntryRow(ev);
    }
    col->addWidget(m_eingabe);

    // ── Das Zahnrad: Datei und Online ────────────────────────────────
    //
    // Die Handlungen existieren als QAction von Anfang an (nicht erst beim
    // Aufklappen) -- so finden Pruefstaende und die Bedienhilfe sie, und
    // das Menue ist nur ihre Anordnung.
    auto aktion = [this](const QString& text, const QString& name, const QString& tip) {
        auto* a = new QAction(text, this);
        a->setObjectName(name);
        a->setToolTip(tip);
        return a;
    };
    m_importAction = aktion(QStringLiteral("Import ADIF…"), QStringLiteral("logbookImportAction"),
        QStringLiteral("Merge an ADIF file into this log, skipping contacts it already has"));
    m_exportAdifAction = aktion(QStringLiteral("Export ADIF…"), QStringLiteral("logbookExportAdifAction"),
        QStringLiteral("Export what the filters show as an ADIF file"));
    m_exportCsvAction = aktion(QStringLiteral("Export CSV…"), QStringLiteral("logbookExportCsvAction"),
        QStringLiteral("Export what the filters show as a CSV table"));
    m_cabrilloAction = aktion(QStringLiteral("Export Cabrillo…"), QStringLiteral("logbookCabrilloAction"),
        QStringLiteral("Export the filtered view as a Cabrillo 3.0 skeleton — check "
                       "the exchange column against the contest's rules before "
                       "submitting"));
    m_uploadAction = aktion(QStringLiteral("Upload to a logging service…"),
        QStringLiteral("logbookUploadAction"),
        QStringLiteral("Send the marked contacts — or everything not yet uploaded — "
                       "to LoTW, QRZ or another service set up under Tools"));
    m_syncAction = aktion(QStringLiteral("Sync with QRZ"), QStringLiteral("logbookSyncAction"),
        m_syncBtn->toolTip());
    m_sotaAction = aktion(QStringLiteral("SOTA…"), QStringLiteral("logbookSotaAction"),
        QStringLiteral(
        "Fill in your summit references (MY_SOTA_REF) from your SOTA activator "
        "log (sotadata.org.uk → Logs → Activator → Download complete log), so "
        "bearings, distances and path profiles start at the summit"));
    m_statsWindowAction = aktion(QStringLiteral("Statistics in their own window…"),
        QStringLiteral("logbookStatsWindowAction"),
        QStringLiteral("All statistics pages in a separate window"));
    connect(m_importAction, &QAction::triggered, this, &LogbookWindow::importAdif);
    connect(m_exportAdifAction, &QAction::triggered, this, &LogbookWindow::exportAdif);
    connect(m_exportCsvAction, &QAction::triggered, this, &LogbookWindow::exportCsv);
    connect(m_cabrilloAction, &QAction::triggered, this, &LogbookWindow::exportCabrillo);
    connect(m_uploadAction, &QAction::triggered, this, [this]() { uploadMenuAt(QCursor::pos()); });
    connect(m_syncAction, &QAction::triggered, this, &LogbookWindow::syncFromQrz);
    connect(m_sotaAction, &QAction::triggered, this, &LogbookWindow::sotaNachtragen);
    connect(m_statsWindowAction, &QAction::triggered, this, &LogbookWindow::showStatistics);
    m_titleBar->setOptionsVisible(true);
    connect(m_titleBar, &WindowTitleBar::optionsRequested, this, &LogbookWindow::showOptionsMenu);

    buildFilterBar(nullptr);

    // Table
    m_table = new QTableWidget(0, ColumnCount, this);
    // Mit Namen: seit Entwurf C liegt die Karte (mit ihrer Jagdliste, auch
    // eine QTableWidget) in derselben Seite.
    m_table->setObjectName(QStringLiteral("logbookTable"));
    m_table->setHorizontalHeaderLabels(headerLabels());
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setAlternatingRowColors(true);
    m_table->horizontalHeader()->setStretchLastSection(true);
    // Not QTableWidget's own sorting: it reorders the rows underneath
    // m_visible, and every row action here — edit, delete, upload, map —
    // maps a view row back to a log record through that index. Sorting
    // behind its back would silently act on the wrong contact. We sort
    // m_visible instead and rebuild.
    m_table->setSortingEnabled(false);
    m_table->horizontalHeader()->setSectionsClickable(true);
    m_table->horizontalHeader()->setSortIndicatorShown(true);
    m_table->horizontalHeader()->setSortIndicator(m_sortColumn, m_sortOrder);
    m_table->horizontalHeader()->setSectionsMovable(true);
    m_table->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    // Glas & Tiefe (2026-09-17): der zentrale Tabellenstil — versenkt,
    // Kopf als Versalzeile ohne Kaesten, Auswahl gedeckt. Schriften per
    // setFont, nicht im Stylesheet (Groesse kaskadiert sonst in den Kopf).
    m_table->setStyleSheet(Style::tableStyle());
    m_table->setFont([this] { QFont f = font(); f.setPixelSize(Style::kFontSmall); return f; }());
    m_table->horizontalHeader()->setFont(Style::capsFont(font(), 8));
    m_table->horizontalHeader()->setHighlightSections(false);

    // ── Table beside a detail pane (L1, 2026-08-10) ──────────────────
    //
    // The log keeps every ADIF field it does not model, and until now
    // kept them invisibly. A column each is not the answer — there is
    // no bound on how many a foreign logger writes — so they go in a
    // pane beside the table, which costs the table no width and shows
    // all of them for the row in hand.
    //
    // A splitter rather than a fixed width: on a laptop the pane is
    // most of the window, and an operator who wants the table back
    // should be able to drag it away rather than turn the feature off.
    m_detail = new QsoDetailPane;
    m_detail->setCache(&m_callCache);
    connect(m_detail, &QsoDetailPane::beamChanged, this, [this](double deg) {
        m_rotorTargetDeg = deg;
        applyRotorToMap();
    });
    // Die Rotorzeile folgt jeder Neuberechnung, nicht nur einer neuen
    // Richtung -- sonst bliebe sie bei JA1XY -> JA1XYZ stehen.
    connect(m_detail, &QsoDetailPane::peilungNeu, this, &LogbookWindow::aktualisiereRotorZeile);
    // Eigener Standort fuer Stationen, die nicht im Log stehen
    // (2026-11-10): der juengste MY_GRIDSQUARE, wie bei der Bedarfs-Matrix.
    m_detail->setHomeGrid([this]() { return meinFeld(); });
    // Das Detailpaneel in einem Rollbereich: wird das Fenster niedriger
    // als Foto, Kopf, Peilung und Knoepfe zusammen, rollt es, statt dass
    // die Abschnitte uebereinanderrutschen (gesehen beim Verkleinern
    // auf 420 × 500, 2026-09-21). Waagrecht rollt nichts — das Paneel
    // nimmt die Breite des Rollbereichs.
    m_detailScroll = new QScrollArea(this);
    m_detailScroll->setWidgetResizable(true);
    m_detailScroll->setFrameShape(QFrame::NoFrame);
    m_detailScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_detailScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_detailScroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
    m_detailScroll->setMinimumWidth(230);
    m_detailScroll->setWidget(m_detail);

    // ── Die drei Seiten (Entwurf C) ──────────────────────────────────
    //
    // Karte, Stationskarte und Liste gibt es je einmal; setModus() setzt
    // sie in die Seite, die gerade gilt. Zwei Karten waeren zwei Globen,
    // zwei Kachelspeicher und zwei 3D-Flaechen fuer dasselbe Bild.
    auto platz = [](QWidget* w) {
        auto* v = new QVBoxLayout(w);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(4);
        return v;
    };
    auto kopf = [this](const QString& t, QWidget* parent) {
        auto* l = new QLabel(t, parent);
        l->setFont(Style::capsFont(font(), 9));
        l->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                             .arg(QString::fromLatin1(Style::kTextScale)));
        return l;
    };
    m_seiten = new QStackedWidget(this);

    // Log: Station | Karte ueber den letzten QSOs | was noch fehlt
    m_logSplit = new QSplitter(Qt::Horizontal, m_seiten);
    m_logSplit->setChildrenCollapsible(false);
    {
        auto* links = new QWidget(m_logSplit);
        m_logKarteLay = platz(links);
        m_logMitte = new QSplitter(Qt::Vertical, m_logSplit);
        m_logMitte->setChildrenCollapsible(false);
        auto* karte = new QWidget(m_logMitte);
        m_logMapLay = platz(karte);
        auto* liste = new QWidget(m_logMitte);
        m_logListeLay = platz(liste);
        m_logListeLay->addWidget(kopf(QStringLiteral("LAST QSOS  ·  double-click to edit"), liste));
        auto* rechts = new QWidget(m_logSplit);
        m_logRechtsLay = platz(rechts);
        m_logRechtsLay->setSpacing(8);
        m_logRechtsLay->addWidget(kopf(QStringLiteral("IN YOUR LOG"), rechts));
        m_bedarfLeer = new QLabel(QStringLiteral(
            "Type a call: what you have with this station and its country — "
            "band by mode — shows here."), rechts);
        m_bedarfLeer->setWordWrap(true);
        m_bedarfLeer->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                                        .arg(QString::fromLatin1(Style::kTextScale)));
        m_logRechtsLay->addWidget(m_bedarfLeer);
        // Die Bedarfs-Matrix (2026-11-08) stand unter der Eingabe; jetzt
        // in ihrer eigenen Spalte daneben.
        m_logRechtsLay->addWidget(m_bedarf);
        m_logRechtsLay->addStretch(1);
        m_logSplit->setStretchFactor(0, 0);
        m_logSplit->setStretchFactor(1, 1);
        m_logSplit->setStretchFactor(2, 0);
    }
    m_seiten->addWidget(m_logSplit);

    // Browse: Filter | Liste | Karte ueber der Station
    m_split = new QSplitter(Qt::Horizontal, m_seiten);
    m_split->setChildrenCollapsible(false);
    {
        auto* filterRoll = new QScrollArea(m_split);
        filterRoll->setWidgetResizable(true);
        filterRoll->setFrameShape(QFrame::NoFrame);
        filterRoll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        filterRoll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
        filterRoll->setWidget(m_filterBar);
        // setWidget() schaltet das Fuellen ein; mit hellem Systemschema
        // stand die Spalte dann hellgrau im dunklen Fenster.
        m_filterBar->setAutoFillBackground(false);
        filterRoll->viewport()->setAutoFillBackground(false);
        filterRoll->setMinimumWidth(200);
        auto* liste = new QWidget(m_split);
        m_browseListeLay = platz(liste);
        m_browseRechts = new QSplitter(Qt::Vertical, m_split);
        m_browseRechts->setChildrenCollapsible(false);
        auto* karte = new QWidget(m_browseRechts);
        m_browseMapLay = platz(karte);
        auto* station = new QWidget(m_browseRechts);
        m_browseKarteLay = platz(station);
        m_split->setStretchFactor(0, 0);
        m_split->setStretchFactor(1, 1);
        m_split->setStretchFactor(2, 0);
    }
    m_seiten->addWidget(m_split);

    // Statistics: die Kennzahlen ueber das ganze Fenster
    auto* seiteStats = new QWidget(m_seiten);
    m_statsSeiteLay = platz(seiteStats);
    m_seiten->addWidget(seiteStats);

    // Die Teiler wie im Hauptfenster (Style::splitterStyle).
    for (QSplitter* t : {m_logSplit, m_logMitte, m_split, m_browseRechts}) {
        t->setStyleSheet(Style::splitterStyle());
    }
    col->addWidget(m_seiten, 1);
    // Liste und Stationskarte gleich in die Log-Seite (siehe ensureMapPanel).
    m_logListeLay->addWidget(m_table, 1);
    m_logKarteLay->addWidget(m_detailScroll);

    // The pane can turn the antenna, but it must not know there is one
    // — same reason the window does not. Pass it on and let whoever
    // owns a rotor decide.
    connect(m_detail, &QsoDetailPane::turnRotorRequested,
            this, &LogbookWindow::turnRotorRequested);
    // Ein Gipfel ist nachgeschlagen: Entfernungen und Peilungen neu.
    connect(&SotaGipfelVerzeichnis::instanz(), &SotaGipfelVerzeichnis::gipfelBekannt,
            this, [this]() {
        if (SotaGipfelVerzeichnis::instanz().korrigiereStrecken(m_all) > 0) {
            refreshTable();
            updateStats();
        }
    });
    connect(m_detail, &QsoDetailPane::editRequested,
            this, &LogbookWindow::editSelected);

    // ── Kennzahlenreihe ──────────────────────────────────────────────
    m_statsSection = new QWidget(this);
    {
        auto* v = new QVBoxLayout(m_statsSection);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(2);
        auto* head = new QHBoxLayout;
        head->setContentsMargins(0, 0, 0, 0);
        auto* cap = new QLabel(QStringLiteral("STATISTICS · what the filters show"), m_statsSection);
        cap->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 9px; letter-spacing: 1px; }")
                               .arg(QString::fromLatin1(Style::kTextScale)));
        head->addWidget(cap);
        head->addStretch(1);
        auto* popOut = new QPushButton(QStringLiteral("\u2197"), m_statsSection);
        popOut->setObjectName(QStringLiteral("statsPopOut"));
        popOut->setStyleSheet(Style::buttonBaseStyle());
        popOut->setToolTip(QStringLiteral("Open the statistics in their own window"));
        connect(popOut, &QPushButton::clicked, this, &LogbookWindow::showStatistics);
        // Die zwei weiteren Bilder (2026-11-08): direkt auf ihre Seite.
        for (const auto& [text, seite] : {std::pair<const char*, int>{"Band × hour", 1},
                                          std::pair<const char*, int>{"Directions", 2}}) {
            auto* k = new QPushButton(QString::fromUtf8(text), m_statsSection);
            k->setStyleSheet(Style::buttonBaseStyle());
            const int s = seite;
            connect(k, &QPushButton::clicked, this, [this, s]() { zeigeStatistikSeite(s); });
            head->addWidget(k);
        }
        head->addWidget(popOut);
        v->addLayout(head);
        m_statsRow = new LogbookStatsWidget(m_statsSection);
        // In der mittleren Spalte duerfen die Kacheln umbrechen.
        m_statsRow->setSingleRow(false);
        // Gedeckelt und rollbar: bricht die Reihe bei schmalem Fenster in
        // zwei oder drei Kachelzeilen um, darf sie der Tabelle nicht die
        // Hoehe nehmen.
        auto* scroll = new QScrollArea(m_statsSection);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setStyleSheet(QStringLiteral("QScrollArea { background: transparent; }"));
        scroll->setWidget(m_statsRow);
        scroll->setMinimumHeight(120);
        v->addWidget(scroll, 1);
    }
    m_statsSeiteLay->addWidget(m_statsSection, 1);

    m_stats = new QLabel(QString{}, this);
    m_stats->setWordWrap(true);
    m_stats->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; }")
        .arg(QString::fromLatin1(Style::kTextSecondary)));
    col->addWidget(m_stats);

    connect(m_table->horizontalHeader(), &QHeaderView::sectionClicked,
            this, [this](int column) {
        // Second click on the same column turns it round; a new column
        // starts ascending, except the date, where the useful first
        // answer is the most recent contact.
        if (column == m_sortColumn) {
            m_sortOrder = (m_sortOrder == Qt::AscendingOrder)
                              ? Qt::DescendingOrder : Qt::AscendingOrder;
        } else {
            m_sortColumn = column;
            m_sortOrder = (column == ColDate || column == ColTime)
                              ? Qt::DescendingOrder : Qt::AscendingOrder;
        }
        m_table->horizontalHeader()->setSortIndicator(m_sortColumn, m_sortOrder);
        applySort();
        refreshTable();
    });

    // Right-click the header to hide a column. Fourteen columns is more
    // than most operators want at once, and which ones matter differs
    // per person — contest, DX chasing and casual logging each care
    // about a different half.
    // Right-click a contact to point the beam at it.
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) { showRowMenu(pos); });

    connect(m_table->horizontalHeader(), &QHeaderView::customContextMenuRequested,
            this, [this](const QPoint& pos) {
        ScopedChildWidget<QMenu> menuOwner(this);   // stirbt nicht mit dem Rahmen, siehe ScopedChildWidget.h
        QMenu& menu = *menuOwner.get();
        const QStringList labels = headerLabels();
        for (int c = 0; c < ColumnCount; ++c) {
            QAction* a = menu.addAction(labels.at(c));
            a->setCheckable(true);
            a->setChecked(!m_table->isColumnHidden(c));
            connect(a, &QAction::toggled, this, [this, c](bool on) {
                m_table->setColumnHidden(c, !on);
                m_headerRestored = true;   // stop auto-fitting over it
                saveHeaderState();
            });
        }
        menu.exec(m_table->horizontalHeader()->mapToGlobal(pos));
        // Das Elternteil kann waehrend exec() gestorben sein — dann ist
        // auch das Menue weg und `this` eine Leiche. Siehe ScopedChildWidget.h.
        if (!menuOwner) { return; }
    });

    connect(m_table->horizontalHeader(), &QHeaderView::sectionResized,
            this, [this](int, int, int) {
        // Once a width has been chosen by hand, stop refitting on every
        // reload — a column that snaps back is worse than a narrow one.
        m_headerRestored = true;
        saveHeaderState();
    });

    // The pane follows the current row. currentCellChanged rather than
    // itemSelectionChanged: with several rows marked for an upload
    // there is no single contact to describe, and the one the operator
    // last touched is the one they are looking at.
    connect(m_table, &QTableWidget::currentCellChanged, this,
            [this](int row, int, int, int) {
        const int idx = sourceRow(row);
        if (idx < 0) { m_detail->clearEntry(); return; }
        m_detail->setEntry(m_all.at(idx));
    });
    // Markierte Zeilen auf der eingebetteten Karte, wie im Fenster.
    connect(m_table, &QTableWidget::itemSelectionChanged,
            this, &LogbookWindow::syncMapPanelSelection);


    // ── Tippen bleibt fluessig (Vorschlag 2026-09-27) ────────────────
    //
    // Gemessen mit 9271 QSOs: der erste Buchstabe kostete 243 ms, davon
    // rund 180 ms das Neuzeichnen der eingebetteten Karte und 34 ms die
    // Kennzahlen -- die Tabelle selbst 27 ms. Beide folgen dem Filter
    // jetzt erst, wenn 200 ms lang keine Taste kam; Tabelle und
    // Statuszeile sofort wie bisher.
    m_followUpTimer = new QTimer(this);
    m_followUpTimer->setSingleShot(true);
    m_followUpTimer->setInterval(200);
    connect(m_followUpTimer, &QTimer::timeout, this, [this]() {
        refreshStatsView();
        refreshMapPanel();
    });
    connect(m_search, &QLineEdit::textChanged, this, [this]() {
        applyFilter();
        applySort();
        refreshTable();
        m_deferFollowUp = true;
        updateStats();
        m_deferFollowUp = false;
    });

    // ── Return on a callsign means "tell me about this station" ──────
    //
    // Typing filters; Return is a decision. That distinction is what
    // makes it safe to spend a QRZ request here without asking: nobody
    // presses Return by scrolling past something.
    //
    // Three cases, and the third is the one that was missing. The call
    // is in the log — select the matching contact, which brings its
    // bearing, confirmations and ADIF fields with it. It is in the log
    // several times — the newest, because the sort already put it
    // first. It is not in the log at all — show what QRZ knows anyway,
    // because "who is this" is a fair question about a station you have
    // never worked, and an empty pane is a poor answer to it.
    connect(m_search, &QLineEdit::returnPressed, this, [this]() {
        showStationFor(m_search->text());
    });
    connect(m_editBtn,   &QPushButton::clicked, this, &LogbookWindow::editSelected);
    connect(m_deleteBtn, &QPushButton::clicked, this, &LogbookWindow::deleteSelected);
    connect(m_table, &QTableWidget::itemDoubleClicked,
            this, [this](QTableWidgetItem*) { editSelected(); });
}

namespace {

// ── Drei sichtbar, der Rest hinter „…" (2026-11-12) ──────────────────
//
// Betreiber: „die anzahl aller modi und baender sind gross und kosten
// platz". Hausstil, Regel 2: genau ein Knopf pro Gruppe leuchtet, drei
// sichtbar, nicht dreizehn. Die QComboBox dahinter bleibt die eine
// Wahrheit fuer den Filter; diese Knoepfe stellen sie nur. Steht dort
// ein Wert, der nicht unter den dreien ist, traegt „…" ihn und leuchtet.
class SchnellWahl : public QWidget {
public:
    SchnellWahl(QComboBox* box, QWidget* parent)
        : QWidget(parent), m_box(box)
    {
        m_lay = new QHBoxLayout(this);
        m_lay->setContentsMargins(0, 0, 0, 0);
        m_lay->setSpacing(2);
        QObject::connect(box, &QComboBox::currentTextChanged, this,
                         [this](const QString&) { aktualisiere(); });
        baue();
    }

    void setHaeufigste(const QStringList& top)
    {
        if (top == m_top && !m_knoepfe.isEmpty()) { aktualisiere(); return; }
        m_top = top;
        baue();
    }

private:
    QPushButton* knopf(const QString& text)
    {
        auto* b = new QPushButton(text, this);
        b->setCheckable(true);
        b->setFocusPolicy(Qt::NoFocus);
        b->setStyleSheet(Style::buttonBaseStyle() + Style::blueCheckedStyle());
        m_lay->addWidget(b);
        m_knoepfe.append(b);
        return b;
    }

    void baue()
    {
        for (QPushButton* b : std::as_const(m_knoepfe)) {
            m_lay->removeWidget(b);
            b->deleteLater();
        }
        m_knoepfe.clear();
        QStringList zeigen{QStringLiteral("any")};
        zeigen << m_top;
        for (const QString& t : std::as_const(zeigen)) {
            QPushButton* b = knopf(t);
            b->setProperty("wert", t);
            QObject::connect(b, &QPushButton::clicked, this, [this, t]() {
                const int i = m_box->findText(t);
                m_box->setCurrentIndex(i < 0 ? 0 : i);
                aktualisiere();
            });
        }
        m_mehr = knopf(QStringLiteral("…"));
        m_mehr->setToolTip(QStringLiteral("Every value in the log"));
        QObject::connect(m_mehr, &QPushButton::clicked, this, [this]() {
            ScopedChildWidget<QMenu> menuOwner(this);
            QMenu& menu = *menuOwner.get();
            for (int i = 1; i < m_box->count(); ++i) {
                QAction* a = menu.addAction(m_box->itemText(i));
                a->setCheckable(true);
                a->setChecked(i == m_box->currentIndex());
                a->setData(i);
            }
            QAction* gewaehlt = menu.exec(m_mehr->mapToGlobal(QPoint(0, m_mehr->height())));
            if (!menuOwner) { return; }
            if (gewaehlt) { m_box->setCurrentIndex(gewaehlt->data().toInt()); }
            aktualisiere();
        });
        aktualisiere();
    }

    void aktualisiere()
    {
        const QString jetzt = m_box->currentIndex() > 0 ? m_box->currentText()
                                                        : QStringLiteral("any");
        bool gezeigt = false;
        for (QPushButton* b : std::as_const(m_knoepfe)) {
            if (b == m_mehr) { continue; }
            const bool an = b->property("wert").toString() == jetzt;
            b->setChecked(an);
            gezeigt = gezeigt || an;
        }
        if (m_mehr) {
            m_mehr->setChecked(!gezeigt);
            m_mehr->setText(gezeigt ? QStringLiteral("…") : jetzt + QStringLiteral(" ▾"));
        }
    }

    QComboBox*           m_box{nullptr};
    QHBoxLayout*         m_lay{nullptr};
    QStringList          m_top;
    QList<QPushButton*>  m_knoepfe;
    QPushButton*         m_mehr{nullptr};
};

// Die drei haeufigsten Werte, fuer SchnellWahl.
QStringList haeufigste(const QStringList& alle, int n)
{
    QHash<QString, int> zahl;
    for (const QString& v : alle) { ++zahl[v]; }
    QStringList werte = zahl.keys();
    std::sort(werte.begin(), werte.end(), [&zahl](const QString& a, const QString& b) {
        return zahl.value(a) != zahl.value(b) ? zahl.value(a) > zahl.value(b) : a < b;
    });
    return werte.mid(0, n);
}

} // namespace

void LogbookWindow::buildFilterBar(QVBoxLayout* col)
{
    // ── Die Filterspalte in „Browse" (Entwurf C, 2026-11-12) ─────────
    //
    // Alle Filter links untereinander, jede Gruppe mit Ueberschrift (Haus-
    // stil, Regel 1); gewaehlt ist eine Mulde. Oben die Suche, darunter die
    // aktiven Filter als Kapseln -- ein Klick nimmt einen weg.
    m_filterBar = new QWidget(this);
    m_filterBar->setObjectName(QStringLiteral("logbookFilterBar"));
    auto* spalte = new QVBoxLayout(m_filterBar);
    spalte->setContentsMargins(0, 0, 8, 0);
    spalte->setSpacing(6);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(
        QStringLiteral("Search call, name, QTH, country, grid or comment"));
    m_search->setClearButtonEnabled(true);
    m_search->setStyleSheet(Style::lineEditStyle());
    m_search->setMinimumWidth(200);
    spalte->addWidget(m_search);

    m_filterChips = new QWidget(this);
    m_filterChips->setObjectName(QStringLiteral("logbookFilterChips"));
    new FlowLayout(m_filterChips, 4, 4);
    m_filterChips->layout()->setContentsMargins(0, 0, 0, 0);
    m_filterChips->setVisible(false);
    spalte->addWidget(m_filterChips);

    auto caption = [this](const QString& t) {
        auto* l = new QLabel(t, this);
        l->setFont(Style::capsFont(font(), 9));
        l->setStyleSheet(QStringLiteral("QLabel { color: %1; padding-top: 8px; }")
                             .arg(QString::fromLatin1(Style::kTextScale)));
        return l;
    };
    // Ueberschrift, darunter das Feld (oder zwei nebeneinander).
    auto pair = [this, &spalte](QLabel* label, QWidget* field, QWidget* second = nullptr) {
        spalte->addWidget(label);
        if (!second) { spalte->addWidget(field); return; }
        auto* box = new QWidget(this);
        auto* h = new QHBoxLayout(box);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(6);
        h->addWidget(field, 1);
        h->addWidget(second);
        spalte->addWidget(box);
    };

    // Band und Betriebsart: drei Knoepfe und „…" statt einer Klappliste.
    // Die Klappliste bleibt als Speicher dahinter, unsichtbar.
    m_bandBox = new QComboBox(this);
    m_bandBox->setVisible(false);
    m_bandWahl = new SchnellWahl(m_bandBox, this);
    m_bandWahl->setObjectName(QStringLiteral("logbookBandChoice"));
    pair(caption(QStringLiteral("BAND")), m_bandWahl);

    m_modeBox = new QComboBox(this);
    m_modeBox->setVisible(false);
    m_modeWahl = new SchnellWahl(m_modeBox, this);
    m_modeWahl->setObjectName(QStringLiteral("logbookModeChoice"));
    pair(caption(QStringLiteral("MODE")), m_modeWahl);

    m_gridEdit = new QLineEdit(this);
    m_gridEdit->setPlaceholderText(QStringLiteral("JN, JN67, JN67VV"));
    m_gridEdit->setToolTip(QStringLiteral(
        "Matches from the start, so two characters find a whole field"));
    m_gridEdit->setFixedWidth(80);

    m_countryEdit = new QLineEdit(this);
    m_countryEdit->setPlaceholderText(QStringLiteral("part of the name"));
    pair(caption(QStringLiteral("COUNTRY · GRID")), m_countryEdit, m_gridEdit);

    m_activationBox = new QComboBox(this);
    m_activationBox->setMinimumWidth(100);
    m_activationBox->setStyleSheet(Style::formFieldStyle());
    m_activationBox->setToolTip(QStringLiteral(
        "SOTA summit or POTA park this contact was logged from.\n\n"
        "Pick one to see just that activation — Export ADIF exports "
        "what is filtered, so this is also how one activation leaves "
        "as its own file."));
    pair(caption(QStringLiteral("ACTIVATION")), m_activationBox);

    // Off by default. A live date range would hide contacts the moment
    // the window opened, and an empty log reads as an empty log.
    // ── Only what is still outstanding ───────────────────────────────
    //
    // The question an operator actually asks of a logbook is not "which
    // contacts are confirmed" but "which are not yet" — that is the list
    // you chase, and the one you send to LoTW again.
    m_unconfirmedOnly = new QCheckBox(QStringLiteral("Unconfirmed only"),
                                      this);
    m_unconfirmedOnly->setStyleSheet(
        QStringLiteral("QCheckBox { color: %1; }")
            .arg(QString::fromLatin1(Style::kTextPrimary)));
    m_unconfirmedOnly->setToolTip(QStringLiteral(
        "Hide contacts confirmed by LoTW, a card or eQSL.\n\nA contact "
        "with a QSL merely REQUESTED still counts as unconfirmed, "
        "because it is."));
    spalte->addWidget(caption(QStringLiteral("ONLY")));
    spalte->addWidget(m_unconfirmedOnly);

    // ── Nur Grauzonen-QSOs (2026-11-10) ──────────────────────────────
    m_greyLineOnly = new QCheckBox(QStringLiteral("Grey line"), this);
    m_greyLineOnly->setObjectName(QStringLiteral("logbookGreyLineOnly"));
    m_greyLineOnly->setStyleSheet(
        QStringLiteral("QCheckBox { color: %1; }")
            .arg(QString::fromLatin1(Style::kTextPrimary)));
    m_greyLineOnly->setToolTip(QStringLiteral(
        "Only contacts with the sun between −6° and +6° at one end or "
        "both, at the time of the contact.\n\nPositions come from the "
        "locators; without one, their end is the country centre from "
        "cty.dat and your end is the newest MY_GRIDSQUARE in the log."));
    spalte->addWidget(m_greyLineOnly);

    m_useDates = new QCheckBox(QStringLiteral("Dates"), this);
    m_useDates->setStyleSheet(QStringLiteral("QCheckBox { color: %1; }")
                                  .arg(QString::fromLatin1(Style::kTextPrimary)));

    m_fromDate = new QDateEdit(QDate::currentDate().addYears(-1), this);
    m_fromDate->setCalendarPopup(true);
    m_fromDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_fromDate->setEnabled(false);
    m_fromDate->setFixedWidth(118);

    m_toDate = new QDateEdit(QDate::currentDate(), this);
    m_toDate->setCalendarPopup(true);
    m_toDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    m_toDate->setEnabled(false);
    m_toDate->setFixedWidth(118);
    // Haken und beide Daten als ein Element: ein Datumsbereich, der
    // zerfaellt, ist keiner mehr. In der schmalen Spalte die Daten unter
    // dem Haken nebeneinander.
    {
        spalte->addWidget(caption(QStringLiteral("DATES")));
        auto* box = new QWidget(this);
        auto* g = new QGridLayout(box);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(6);
        g->addWidget(m_useDates, 0, 0, 1, 2);
        m_fromDate->setFixedWidth(104);
        m_toDate->setFixedWidth(104);
        g->addWidget(m_fromDate, 1, 0);
        g->addWidget(m_toDate, 1, 1);
        spalte->addWidget(box);
    }

    m_clearBtn = new QPushButton(QStringLiteral("Clear"), this);
    m_clearBtn->setStyleSheet(Style::buttonBaseStyle());
    m_clearBtn->setToolTip(QStringLiteral("Empty every filter box"));
    spalte->addSpacing(10);
    spalte->addWidget(m_clearBtn);
    spalte->addStretch(1);

    for (QLineEdit* e : {m_gridEdit, m_countryEdit}) {
        e->setClearButtonEnabled(true);
        e->setStyleSheet(Style::lineEditStyle());
    }
    Q_UNUSED(col);   // die Spalte setzt buildUi() in die Seite „Browse"

    auto reapply = [this]() {
        applyFilter();
        applySort();
        refreshTable();
        updateStats();
    };
    connect(m_bandBox, &QComboBox::currentTextChanged, this, reapply);
    connect(m_modeBox, &QComboBox::currentTextChanged, this, reapply);
    connect(m_activationBox, &QComboBox::currentTextChanged, this, reapply);
    connect(m_gridEdit, &QLineEdit::textChanged, this, reapply);
    connect(m_countryEdit, &QLineEdit::textChanged, this, reapply);
    connect(m_fromDate, &QDateEdit::dateChanged, this, reapply);
    connect(m_toDate, &QDateEdit::dateChanged, this, reapply);
    connect(m_unconfirmedOnly, &QCheckBox::toggled, this,
            [reapply](bool) { reapply(); });
    connect(m_greyLineOnly, &QCheckBox::toggled, this,
            [reapply](bool) { reapply(); });
    connect(m_useDates, &QCheckBox::toggled, this, [this, reapply](bool on) {
        m_fromDate->setEnabled(on);
        m_toDate->setEnabled(on);
        reapply();
    });

    connect(m_clearBtn, &QPushButton::clicked, this, [this, reapply]() {
        // Block each control rather than reapplying six times on the
        // way to an empty filter.
        QSignalBlocker b1(m_search), b2(m_bandBox), b3(m_modeBox);
        QSignalBlocker b4(m_gridEdit), b5(m_countryEdit), b6(m_useDates);
        QSignalBlocker b7(m_activationBox);
        // „Unconfirmed only" kam nach Clear dazu und blieb stehen: die
        // Tabelle blieb gefiltert, obwohl Clear gedrueckt war (Nachttest
        // 2026-09-26).
        QSignalBlocker b8(m_unconfirmedOnly);
        m_unconfirmedOnly->setChecked(false);
        QSignalBlocker b9(m_greyLineOnly);
        m_greyLineOnly->setChecked(false);
        m_search->clear();
        m_bandBox->setCurrentIndex(0);
        m_modeBox->setCurrentIndex(0);
        m_activationBox->setCurrentIndex(0);
        m_gridEdit->clear();
        m_countryEdit->clear();
        m_useDates->setChecked(false);
        m_fromDate->setEnabled(false);
        m_toDate->setEnabled(false);
        reapply();
    });
}

void LogbookWindow::uploadMenuAt(const QPoint& globalPos)
{
    if (m_uploaders.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Upload"),
            QStringLiteral("No logging service is set up yet.\n\n"
                           "Add one under Tools."));
        return;
    }
    // Decide what is being sent before asking where. Marked rows
    // if there are any; otherwise everything not yet uploaded —
    // "what still needs to go" is the question after a session, and
    // making the operator select it by hand every time is the
    // reason people stop bothering.
    QList<int> rows = selectedSourceRows();
    const bool fromSelection = !rows.isEmpty();
    if (!fromSelection) { rows = outstandingRows(); }

    if (rows.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Upload"),
            QStringLiteral("Everything is already uploaded."));
        return;
    }

    ScopedChildWidget<QMenu> menuOwner(this);   // stirbt nicht mit dem Rahmen, siehe ScopedChildWidget.h
    QMenu& menu = *menuOwner.get();
    auto* header = menu.addAction(fromSelection
        ? QStringLiteral("Send %1 marked contacts to…").arg(rows.size())
        : QStringLiteral("Send %1 not yet uploaded to…").arg(rows.size()));
    header->setEnabled(false);
    menu.addSeparator();

    QHash<QAction*, QsoUploader*> map;
    for (QsoUploader* u : m_uploaders) {
        QAction* a = menu.addAction(u->serviceName());
        // Listed but disabled rather than hidden: an operator who
        // set one up last week and sees it missing assumes the
        // feature is broken, not that a credential went astray.
        a->setEnabled(u->isConfigured());
        if (!u->isConfigured()) {
            a->setText(u->serviceName()
                       + QStringLiteral("  (not configured)"));
        }
        map.insert(a, u);
    }
    QAction* chosen = menu.exec(globalPos);
    // Das Elternteil kann waehrend exec() gestorben sein — dann ist
    // auch das Menue weg und `this` eine Leiche. Siehe ScopedChildWidget.h.
    if (!menuOwner) { return; }
    if (chosen && map.contains(chosen)) {
        uploadEntries(rows, map.value(chosen));
    }
}

void LogbookWindow::showOptionsMenu()
{
    ScopedChildWidget<QMenu> menuOwner(this);   // stirbt nicht mit dem Rahmen, siehe ScopedChildWidget.h
    QMenu& menu = *menuOwner.get();
    menu.setToolTipsVisible(true);
    menu.addSection(QStringLiteral("File"));
    menu.addAction(m_importAction);
    menu.addAction(m_exportAdifAction);
    menu.addAction(m_exportCsvAction);
    menu.addAction(m_cabrilloAction);
    menu.addSection(QStringLiteral("Online"));
    // Laeuft gerade eine Uebertragung, ist der Eintrag aus -- wie vorher
    // der Knopf.
    m_uploadAction->setEnabled(m_uploadBtn == nullptr || m_uploadBtn->isEnabled());
    m_syncAction->setEnabled(m_syncBtn == nullptr || m_syncBtn->isEnabled());
    menu.addAction(m_uploadAction);
    menu.addAction(m_syncAction);
    menu.addAction(m_sotaAction);
    menu.addSection(QStringLiteral("Window"));
    menu.addAction(m_statsWindowAction);
    // Unter dem Zahnrad aufklappen, nicht irgendwo: der Knopf ist das
    // einzige Kind der Titelleiste, das „⚙" heisst.
    QPoint at = QCursor::pos();
    for (QPushButton* b : m_titleBar->findChildren<QPushButton*>()) {
        if (b->text() == QStringLiteral("⚙")) {
            at = b->mapToGlobal(QPoint(b->width() - menu.sizeHint().width(), b->height() + 2));
        }
    }
    menu.exec(at);
    if (!menuOwner) { return; }
}

void LogbookWindow::updateFilterChips()
{
    if (!m_filterChips || !m_bandBox) { return; }
    QLayout* lay = m_filterChips->layout();
    if (!lay) { return; }
    while (QLayoutItem* it = lay->takeAt(0)) {
        std::unique_ptr<QLayoutItem> weg(it);
        if (QWidget* w = it->widget()) { w->deleteLater(); }
    }

    // Was die Liste gerade kuerzt -- jedes als Kapsel, ein Klick nimmt es
    // weg. Die Suche selbst steht im Feld und braucht keine.
    struct Kapsel { QString text; std::function<void()> weg; };
    QVector<Kapsel> kapseln;
    if (m_bandBox->currentIndex() > 0) {
        kapseln.push_back({m_bandBox->currentText(), [this]() { m_bandBox->setCurrentIndex(0); }});
    }
    if (m_modeBox->currentIndex() > 0) {
        kapseln.push_back({m_modeBox->currentText(), [this]() { m_modeBox->setCurrentIndex(0); }});
    }
    if (m_activationBox->currentIndex() > 0) {
        kapseln.push_back({m_activationBox->currentText(),
                           [this]() { m_activationBox->setCurrentIndex(0); }});
    }
    if (!m_gridEdit->text().trimmed().isEmpty()) {
        kapseln.push_back({QStringLiteral("grid ") + m_gridEdit->text().trimmed().toUpper(),
                           [this]() { m_gridEdit->clear(); }});
    }
    if (!m_countryEdit->text().trimmed().isEmpty()) {
        kapseln.push_back({m_countryEdit->text().trimmed(), [this]() { m_countryEdit->clear(); }});
    }
    if (m_unconfirmedOnly->isChecked()) {
        kapseln.push_back({QStringLiteral("unconfirmed"),
                           [this]() { m_unconfirmedOnly->setChecked(false); }});
    }
    if (m_greyLineOnly->isChecked()) {
        kapseln.push_back({QStringLiteral("grey line"),
                           [this]() { m_greyLineOnly->setChecked(false); }});
    }
    if (m_useDates->isChecked()) {
        kapseln.push_back({m_fromDate->date().toString(QStringLiteral("yyyy-MM-dd"))
                               + QStringLiteral(" – ")
                               + m_toDate->date().toString(QStringLiteral("yyyy-MM-dd")),
                           [this]() { m_useDates->setChecked(false); }});
    }

    for (const Kapsel& k : std::as_const(kapseln)) {
        auto* b = new QPushButton(k.text + QStringLiteral("  ✕"), m_filterChips);
        b->setFocusPolicy(Qt::NoFocus);
        b->setToolTip(QStringLiteral("Remove this filter"));
        b->setStyleSheet(Style::buttonBaseStyle()
                         + QStringLiteral("QPushButton { border-radius: 10px; padding: 1px 9px; }"));
        const std::function<void()> weg = k.weg;
        connect(b, &QPushButton::clicked, this, [weg]() { weg(); });
        lay->addWidget(b);
    }
    m_filterChips->setVisible(!kapseln.isEmpty());
}

void LogbookWindow::refreshFilterChoices()
{
    // Only what the log actually contains. A list of every band in
    // existence is mostly a set of ways to get no results.
    auto fill = [](QComboBox* box, QStringList values) {
        const QString keep = box->currentText();
        values.removeDuplicates();
        std::sort(values.begin(), values.end());

        QSignalBlocker block(box);
        box->clear();
        box->addItem(QStringLiteral("any"));
        box->addItems(values);

        const int at = box->findText(keep);
        box->setCurrentIndex(at >= 0 ? at : 0);
    };

    QStringList bands;
    QStringList modes;
    QStringList modeZahl;
    QStringList activations;
    for (const LogEntry& e : m_all) {
        if (!e.band.trimmed().isEmpty())    { bands << e.band.trimmed(); }
        if (!e.mode.trimmed().isEmpty())    { modes << e.mode.trimmed(); }
        if (!e.submode.trimmed().isEmpty()) { modes << e.submode.trimmed(); }
        // Fuer die drei Schnellknoepfe zaehlt die Betriebsart, wie man sie
        // nennt: FT4 statt MFSK, aber SSB statt USB/LSB.
        const QString sub = e.submode.trimmed().toUpper();
        modeZahl << ((sub.isEmpty() || sub == QStringLiteral("USB") || sub == QStringLiteral("LSB"))
                         ? e.mode.trimmed() : e.submode.trimmed());
        if (!e.mySotaRef.trimmed().isEmpty()) {
            activations << e.mySotaRef.trimmed();
        }
        if (!e.myPotaRef.trimmed().isEmpty()) {
            activations << e.myPotaRef.trimmed();
        }
    }
    // Die drei haeufigsten vor dem Fuellen zaehlen -- fill() macht die
    // Listen eindeutig.
    const QStringList bandTop = haeufigste(bands, 3);
    const QStringList modeTop = haeufigste(modeZahl, 3);
    fill(m_bandBox, bands);
    fill(m_modeBox, modes);
    fill(m_activationBox, activations);
    if (m_bandWahl) { static_cast<SchnellWahl*>(m_bandWahl)->setHaeufigste(bandTop); }
    if (m_modeWahl) { static_cast<SchnellWahl*>(m_modeWahl)->setHaeufigste(modeTop); }
}

LogFilter LogbookWindow::currentFilter() const
{
    LogFilter f;
    f.text    = m_search->text();
    f.grid    = m_gridEdit->text();
    f.country = m_countryEdit->text();

    // Index 0 is "any" and is not a value to match against.
    if (m_bandBox->currentIndex() > 0) { f.band = m_bandBox->currentText(); }
    if (m_modeBox->currentIndex() > 0) { f.mode = m_modeBox->currentText(); }
    if (m_activationBox->currentIndex() > 0) {
        f.activation = m_activationBox->currentText();
    }

    f.unconfirmedOnly = m_unconfirmedOnly->isChecked();
    f.greyLineOnly = m_greyLineOnly->isChecked();
    if (f.greyLineOnly) {
        f.greyLineCty  = m_cty;
        f.greyLineHome = meinFeld();
    }
    f.useDates = m_useDates->isChecked();
    if (f.useDates) {
        f.from = m_fromDate->date();
        f.to   = m_toDate->date();
    }
    return f;
}

// ── Data ────────────────────────────────────────────────────────────

void LogbookWindow::reload()
{
    QString err;
    m_all = AdifLog::read(m_path, &err);
    if (!err.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Logbook"),
            QStringLiteral("Couldn't read the log:\n%1").arg(err));
    }
    // Vom Gipfel (2026-11-11): Entfernung und Peilung der QSOs mit
    // MY_SOTA_REF / SOTA_REF aus der Lage des Gipfels, nicht aus der
    // Locatormitte. Unbekannte Gipfel werden einmal nachgeschlagen.
    SotaGipfelVerzeichnis::instanz().anfragenFuer(m_all);
    SotaGipfelVerzeichnis::instanz().korrigiereStrecken(m_all);
    std::stable_sort(m_all.begin(), m_all.end(), newerFirst);
    refreshFilterChoices();
    applyFilter();
    applySort();
    refreshTable();
    updateStats();
}

void LogbookWindow::applyFilter()
{
    const LogFilter f = currentFilter();
    m_visible.clear();
    m_visible.reserve(m_all.size());

    for (int i = 0; i < m_all.size(); ++i) {
        if (f.matches(m_all.at(i))) { m_visible.append(i); }
    }
    updateFilterChips();
}

void LogbookWindow::saveHeaderState()
{
    AppSettings::instance().setValue(
        QStringLiteral("LogbookHeaderState"),
        m_table->horizontalHeader()->saveState());
}

void LogbookWindow::restoreHeaderState()
{
    const QByteArray st = AppSettings::instance()
        .value(QStringLiteral("LogbookHeaderState")).toByteArray();
    if (st.isEmpty()) {
        // First run with the detail pane. Five columns say exactly what
        // the pane says, and with both on screen the table needs
        // horizontal scrolling to show the ones that are its own job —
        // date, callsign, band, mode, confirmed.
        //
        // Only when nothing was saved. An operator who has already
        // arranged their columns has said what they want, and quietly
        // hiding five of them because a new pane arrived would be the
        // program overruling them.
        for (int c : {ColName, ColQth, ColCountry, ColGrid, ColDistance}) {
            m_table->setColumnHidden(c, true);
        }
        return;
    }
    // A saved state from a build with fewer columns restores the old
    // count and leaves the new ones invisible with no way to find them.
    // Better to start over than to hide a column the operator never
    // hid.
    QHeaderView* h = m_table->horizontalHeader();
    if (!h->restoreState(st)) { return; }
    if (h->count() != ColumnCount) { return; }
    m_headerRestored = true;
    h->setSortIndicator(m_sortColumn, m_sortOrder);
}

void LogbookWindow::applySort()
{
    const int column = m_sortColumn;
    const bool asc = m_sortOrder == Qt::AscendingOrder;
    const QVector<LogEntry>& all = m_all;

    auto text = [&all, column](int i) -> QString {
        const LogEntry& e = all.at(i);
        switch (column) {
        case ColCall:    return e.call;
        case ColMode:    return e.submode.isEmpty() ? e.mode : e.submode;
        case ColSent:    return e.rstSent;
        case ColRcvd:    return e.rstRcvd;
        case ColName:    return e.name;
        case ColQth:     return e.qth;
        case ColCountry: return e.country;
        case ColGrid:    return e.gridSquare;
        case ColComment: return e.comment;
        default:         return {};
        }
    };

    std::stable_sort(m_visible.begin(), m_visible.end(),
                     [&](int lhs, int rhs) {
        const LogEntry& a = all.at(lhs);
        const LogEntry& b = all.at(rhs);
        bool less = false;
        switch (column) {
        case ColDate:
        case ColTime:
            // Undated contacts sort together at the end whichever way
            // the column is pointing, rather than jumping between the
            // top and the bottom as it is toggled.
            if (a.timeOn.isValid() != b.timeOn.isValid()) {
                return a.timeOn.isValid() == asc ? false : true;
            }
            less = a.timeOn < b.timeOn;
            break;
        case ColFreq:
            less = a.freqMHz < b.freqMHz;
            break;
        case ColBand:
            // By frequency, not by name. Sorted as text, "160m" comes
            // before "40m" and the band column becomes nonsense.
            less = AdifLog::bandSortKeyMHz(a.band)
                 < AdifLog::bandSortKeyMHz(b.band);
            break;
        case ColDistance:
            less = a.distanceKm < b.distanceKm;
            break;
        case ColQrz:
            // Not uploaded first when ascending: the useful question is
            // "what is still outstanding", not "what is done".
            less = a.uploadedToQrz < b.uploadedToQrz;
            break;
        case ColConfirmed:
            // Same reasoning: unconfirmed first, because that is the
            // list you act on.
            less = QsoConfirmation::isConfirmed(a)
                 < QsoConfirmation::isConfirmed(b);
            break;
        default:
            less = text(lhs).compare(text(rhs), Qt::CaseInsensitive) < 0;
            break;
        }
        return asc ? less : !less;
    });
}

void LogbookWindow::showRowMenu(const QPoint& pos)
{
    const int view = m_table->rowAt(pos.y());
    const int idx  = sourceRow(view);
    if (idx < 0) { return; }
    const LogEntry& e = m_all.at(idx);

    ScopedChildWidget<QMenu> menuOwner(this);   // stirbt nicht mit dem Rahmen, siehe ScopedChildWidget.h
    QMenu& menu = *menuOwner.get();

    // The bearing is only meaningful when both locators are known —
    // ours and theirs. Without it the entry has a bearing field holding
    // zero, and offering to turn the antenna due north because a grid
    // square is missing would be worse than offering nothing.
    const bool haveBearing = e.distanceKm > 0.0
                          && !e.gridSquare.trimmed().isEmpty()
                          && !e.myGridSquare.trimmed().isEmpty();

    if (haveBearing) {
        const double sp = BeamHeading::wrap360(e.bearingDeg);
        const double lp = BeamHeading::longPath(sp);
        QAction* a1 = menu.addAction(
            QStringLiteral("Turn rotor to %1 — short path %2°")
                .arg(e.call).arg(sp, 0, 'f', 0));
        QAction* a2 = menu.addAction(
            QStringLiteral("Turn rotor to %1 — long path %2°")
                .arg(e.call).arg(lp, 0, 'f', 0));
        a2->setToolTip(QStringLiteral(
            "The other way round the world. On the low bands and at grey "
            "line this is often the stronger signal."));
        connect(a1, &QAction::triggered, this,
                [this, sp, call = e.call]() {
            emit turnRotorRequested(sp, call);
        });
        connect(a2, &QAction::triggered, this,
                [this, lp, call = e.call]() {
            emit turnRotorRequested(lp, call);
        });
        menu.addSeparator();
    } else {
        QAction* none = menu.addAction(
            QStringLiteral("No bearing — needs both locators"));
        none->setEnabled(false);
        menu.addSeparator();
    }

    QAction* edit = menu.addAction(QStringLiteral("Edit…"));
    connect(edit, &QAction::triggered, this, &LogbookWindow::editSelected);

    menu.exec(m_table->viewport()->mapToGlobal(pos));
    // Das Elternteil kann waehrend exec() gestorben sein — dann ist
    // auch das Menue weg und `this` eine Leiche. Siehe ScopedChildWidget.h.
    if (!menuOwner) { return; }
}

void LogbookWindow::refreshTable()
{
    m_table->setRowCount(m_visible.size());

    // Bug fix 2026-09-07: this loop calls setItem()/setForeground()/
    // setFont()/setToolTip() up to ColumnCount times PER ROW, and each
    // one fires a dataChanged signal from the table's model. Qt's macOS
    // accessibility bridge does not coalesce those -- confirmed against
    // Qt's own source (qtbase src/widgets/accessible/itemviews.cpp,
    // src/plugins/platforms/cocoa/qcocoaaccessibilityelement.mm): every
    // dataChanged re-walks and reallocates the WHOLE table's
    // NSAccessibilityElement row/column arrays via
    // -[QMacAccessibilityElement populateTableArray:count:], which
    // always allocates fresh elements rather than reusing cached ones.
    // With any accessibility observer attached (VoiceOver, remote
    // control software, or a11y-based automation), that is O(rows x
    // cols) work PER CELL -- O((rows x cols)^2) total for one refresh.
    // Confirmed live: opening this window while an observer was attached
    // grew process RSS from under 1 GB to 2-5+ GB within seconds (up to
    // 164 million live QMacAccessibilityElement instances in one heap
    // snapshot), eventually forcing macOS to freeze or kill the app.
    //
    // setRowCount() above stays unblocked (the view still needs its
    // rowsInserted/rowsRemoved to keep geometry/scrollbars correct); only
    // the per-cell dataChanged flood from the loop below is suppressed,
    // and replaced with a single manual repaint + a single accessibility
    // DataChanged event covering the whole table after the loop. This
    // turns O(rows x cols) accessibility notifications into O(1) without
    // disabling accessibility for real VoiceOver users.
    {
    const QSignalBlocker modelBlocker(m_table->model());
    for (int row = 0; row < m_visible.size(); ++row) {
        const LogEntry& e = m_all.at(m_visible.at(row));
        const QDateTime u = e.timeOn.toUTC();

        auto put = [this, row](int c, const QString& s) {
            m_table->setItem(row, c, new QTableWidgetItem(s));
        };
        put(ColDate, u.isValid() ? u.toString(QStringLiteral("yyyy-MM-dd"))
                                 : QString{});
        put(ColTime, u.isValid() ? u.toString(QStringLiteral("hh:mm"))
                                 : QString{});
        put(ColCall, e.call);
        put(ColFreq, e.freqMHz > 0.0
                ? QString::number(e.freqMHz, 'f', 3) : QString{});
        put(ColBand, e.band);
        // Band coloured by band (2026-08-10): a mixed log is scanned by
        // this column, and colour reads faster than text.
        if (QTableWidgetItem* bi = m_table->item(row, ColBand)) {
            static const QHash<QString, QColor> kBandCol = {
                {QStringLiteral("160m"), QColor(Style::kTxRed)},
                {QStringLiteral("80m"),  QColor(Style::kTxRed)},
                {QStringLiteral("60m"),  QColor(Style::kAmberText)},
                {QStringLiteral("40m"),  QColor(Style::kAmberText)},
                {QStringLiteral("30m"),  QColor(Style::kAmberText)},
                {QStringLiteral("20m"),  QColor(Style::kGreenText)},
                {QStringLiteral("17m"),  QColor(Style::kDspToggleText)},
                {QStringLiteral("15m"),  QColor(Style::kBlueText)},
                {QStringLiteral("12m"),  QColor(Style::kAccent)},
                {QStringLiteral("10m"),  QColor(Style::kBlueHover)},
                {QStringLiteral("6m"),   QColor(Style::kTxRed)},
                {QStringLiteral("2m"),   QColor(Style::kTxRed)},
            };
            const QColor c = kBandCol.value(e.band.trimmed().toLower());
            if (c.isValid()) {
                bi->setForeground(c);
                QFont bf = bi->font();
                bf.setBold(true);
                bi->setFont(bf);
            }
        }
        put(ColMode, e.submode.isEmpty() ? e.mode : e.submode);
        put(ColSent, e.rstSent);
        put(ColRcvd, e.rstRcvd);
        put(ColName, e.name);
        put(ColQth, e.qth);
        put(ColCountry, e.country);
        put(ColGrid, e.gridSquare);
        put(ColDistance, e.distanceKm > 0.0
                ? QString::number(e.distanceKm, 'f', 0) : QString{});
        // A tick, not the word "yes": the column is scanned, not read.
        put(ColQrz, e.uploadedToQrz ? QStringLiteral("\u2713") : QString{});

        // ── Confirmations, at last visible ───────────────────────────
        //
        // These fields have survived an import since the ADIF
        // round-trip was fixed, and until now nothing showed them. A
        // confirmation you cannot see is not much better than one that
        // was deleted, which is what used to happen to it.
        //
        // Blank when nothing is confirmed, deliberately: an unconfirmed
        // contact should be quiet rather than compete with the
        // confirmed ones for attention.
        put(ColConfirmed, QsoConfirmation::badge(e));
        if (QTableWidgetItem* it = m_table->item(row, ColConfirmed)) {
            it->setToolTip(QsoConfirmation::describe(e));
            if (QsoConfirmation::isConfirmed(e)) {
                it->setForeground(QColor(0x00, 0xff, 0x88));
            }
        }
        put(ColComment, e.comment);
    }
    } // modelBlocker scope

    // Single manual repaint (paintEvent reads current model data directly,
    // so this is correct even though dataChanged was suppressed above) and
    // a single accessibility notification for the whole table, replacing
    // the O(rows x cols) per-cell ones that were just blocked.
    m_table->viewport()->update();
    {
        QAccessibleTableModelChangeEvent tableEvent(
            m_table, QAccessibleTableModelChangeEvent::DataChanged);
        tableEvent.setFirstRow(0);
        tableEvent.setLastRow(qMax(0, m_table->rowCount() - 1));
        tableEvent.setFirstColumn(0);
        tableEvent.setLastColumn(ColumnCount - 1);
        QAccessible::updateAccessibility(&tableEvent);
    }

    if (!m_headerRestored) { m_table->resizeColumnsToContents(); }

    // ── Something has to be selected for the pane to have a subject ──
    //
    // setRowCount() drops the current cell, so after every filter
    // keystroke and every reload there was no current row and the
    // detail pane read "Select a contact" — including the moment the
    // window opens, which is when an operator first looks at it and
    // concludes the pane is broken.
    //
    // The first row is the right default because the table is sorted
    // newest-first: the contact you want is nearly always the last one
    // you made. (2026-08-10)
    // Das eigene Feld fuer QSOs ohne MY_GRIDSQUARE (Grauzone), bevor
    // das Paneel ein QSO bekommt.
    if (m_detail) { m_detail->setMeinFeldErsatz(meinFeld()); }
    if (m_table->rowCount() > 0) {
        if (m_table->currentRow() < 0) {
            m_table->setCurrentCell(0, ColCall);
        } else {
            // The row count changed underneath the current row and the
            // signal does not fire for that, so the pane would keep
            // describing whichever contact used to be at this index.
            const int idx = sourceRow(m_table->currentRow());
            if (idx >= 0) { m_detail->setEntry(m_all.at(idx)); }
        }
    } else {
        m_detail->clearEntry();
    }
}

void LogbookWindow::updateStats()
{
    // The Kennzahlen dialog follows the same view as this status line.
    if (m_deferFollowUp && m_followUpTimer) {
        m_followUpTimer->start();
    } else {
        if (m_followUpTimer) { m_followUpTimer->stop(); }
        refreshStatsView();
        refreshMapPanel();
    }

    QSet<QString> calls;
    QSet<QString> bands;
    QSet<QString> modes;
    double longest = 0.0;
    QString longestCall;

    for (int i : m_visible) {
        const LogEntry& e = m_all.at(i);
        calls.insert(e.call.toUpper());
        if (!e.band.isEmpty()) { bands.insert(e.band.toLower()); }
        if (!e.mode.isEmpty()) { modes.insert(e.mode.toUpper()); }
        if (e.distanceKm > longest) {
            longest = e.distanceKm;
            longestCall = e.call;
        }
    }

    QStringList bits;
    bits << QStringLiteral("%1 of %2 contacts")
                .arg(m_visible.size()).arg(m_all.size());
    bits << QStringLiteral("%1 unique calls").arg(calls.size());
    if (!bands.isEmpty()) {
        bits << QStringLiteral("%1 bands").arg(bands.size());
    }
    if (!modes.isEmpty()) {
        bits << QStringLiteral("%1 modes").arg(modes.size());
    }
    if (longest > 0.0) {
        bits << QStringLiteral("furthest %1 km (%2)")
                    .arg(longest, 0, 'f', 0).arg(longestCall);
    }
    // What still has to go out, always visible rather than only on
    // opening the Upload menu. A prompt on every open would be nagging;
    // a number that is there when you look is not.
    const int outstanding = outstandingRows().size();
    if (outstanding > 0 && !m_uploaders.isEmpty()) {
        bits << QStringLiteral("%1 not uploaded").arg(outstanding);
    }

    m_stats->setText(bits.join(QStringLiteral("  ·  ")));
}

int LogbookWindow::sourceRow(int viewRow) const
{
    if (viewRow < 0 || viewRow >= m_visible.size()) { return -1; }
    return m_visible.at(viewRow);
}

QList<int> LogbookWindow::selectedSourceRows() const
{
    // Collect distinct rows first: selectedItems() yields one entry per
    // cell, so a three-row selection arrives as thirty-odd items.
    QSet<int> rows;
    for (QTableWidgetItem* it : m_table->selectedItems()) {
        rows.insert(it->row());
    }
    QList<int> viewRows(rows.begin(), rows.end());
    std::sort(viewRows.begin(), viewRows.end());

    QList<int> out;
    for (int r : viewRows) {
        const int idx = sourceRow(r);
        if (idx >= 0) { out.append(idx); }
    }
    return out;
}

void LogbookWindow::setUploaders(const QVector<QsoUploader*>& uploaders)
{
    m_uploaders = uploaders;
    for (QsoUploader* u : m_uploaders) {
        if (!u) { continue; }
        // Uploads from the panel use the same objects, so filter by
        // whether a batch of ours is outstanding. Without that, logging
        // a contact live would pop a summary box from this window.
        //
        // ── Echte Slot-Methode, kein Lambda ─────────────────────────
        //
        // Qt::UniqueConnection greift laut Dokumentation NUR bei Zeigern
        // auf Elementfunktionen und schweigend NICHT bei Lambdas. Hier
        // stand ein Lambda MIT UniqueConnection -- die Fahne tat also
        // nichts, und jeder Aufruf von setUploaders haengte eine weitere
        // Verbindung an. Zwei Aufrufe, zwei Meldungsfenster je Upload.
        connect(u, &QsoUploader::uploadFinished,
                this, &LogbookWindow::onUploadFinished,
                Qt::UniqueConnection);
    }
}

void LogbookWindow::onUploadFinished(const QString& call, bool ok,
                                     bool duplicate, const QString& message)
{
            if (m_pending <= 0) { return; }
            --m_pending;
            if (ok) {
                ++m_okCount;
                if (duplicate) { ++m_dupCount; }
                // A duplicate counts as uploaded: the service has it.
                // Leaving it unmarked would offer it again on every
                // future run, for ever.
                for (int idx : m_uploadBatch) {
                    if (idx >= 0 && idx < m_all.size()
                        && !m_all[idx].uploadedToQrz
                        && Callsigns::normalized(m_all.at(idx).call)
                               == Callsigns::normalized(call)) {
                        m_all[idx].uploadedToQrz = true;
                        break;
                    }
                }
            } else {
                m_failures << QStringLiteral("%1: %2").arg(call, message);
            }
            if (m_pending > 0) { return; }

            // One summary at the end, not a box per contact.
            QString text = QStringLiteral("%1 accepted").arg(m_okCount);
            if (m_dupCount > 0) {
                text += QStringLiteral(", of which %1 already present")
                            .arg(m_dupCount);
            }
            if (!m_failures.isEmpty()) {
                text += QStringLiteral("\n\n%1 failed:\n%2")
                            .arg(m_failures.size())
                            .arg(m_failures.mid(0, 10)
                                     .join(QLatin1Char('\n')));
                if (m_failures.size() > 10) {
                    text += QStringLiteral("\n…and %1 more")
                                .arg(m_failures.size() - 10);
                }
            }
            // Write the marks once for the whole batch, not once per
            // contact: a five-hundred-contact upload would otherwise
            // rewrite the log five hundred times.
            if (m_okCount > 0) { saveAll(); }

            const bool anyFailed = !m_failures.isEmpty();
            m_okCount = 0;
            m_dupCount = 0;
            m_failures.clear();
            m_uploadBatch.clear();
            m_uploadBtn->setEnabled(true);
            m_uploadBtn->setVisible(false);   // Fortschritt vorbei: zurueck hinters Zahnrad

            if (anyFailed) {
                QMessageBox::warning(this, QStringLiteral("Upload"), text);
            } else {
                QMessageBox::information(this, QStringLiteral("Upload"), text);
            }
}

QList<int> LogbookWindow::outstandingRows() const
{
    QList<int> out;
    for (int i = 0; i < m_all.size(); ++i) {
        if (!m_all.at(i).uploadedToQrz) { out.append(i); }
    }
    return out;
}

void LogbookWindow::uploadEntries(const QList<int>& sourceRows,
                                  QsoUploader* target)
{
    if (!target || sourceRows.isEmpty()) { return; }
    if (m_pending > 0) { return; }   // a batch is already running

    // Confirm the count. Sending 400 contacts because a stray Ctrl-A
    // selected the whole log is not recoverable at the far end.
    if (sourceRows.size() > 1) {
        if (QMessageBox::question(this, QStringLiteral("Upload"),
                QStringLiteral("Send %1 contacts to %2?")
                    .arg(sourceRows.size()).arg(target->serviceName()),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes) {
            return;
        }
    }

    m_pending     = sourceRows.size();
    m_okCount     = 0;
    m_dupCount    = 0;
    m_uploadBatch = sourceRows;
    m_failures.clear();
    m_uploadBtn->setEnabled(false);
    m_uploadBtn->setVisible(true);   // der Fortschritt steht in der Leiste

    for (int idx : sourceRows) { target->upload(m_all.at(idx)); }
}

// ── Saving and correcting ───────────────────────────────────────────
//
// These three went missing in the one-click-logging commit: an edit
// script replaced a region and took the tail of the file with it.
// Nothing complained until the linker did, because a definition that
// is simply absent compiles perfectly well.

bool LogbookWindow::saveAll()
{
    // The table shows newest first, but the file stays chronological.
    // Writing m_all in display order would silently reverse the log on
    // the first correction, and every other reader of the file — the
    // dock's recent list, another logger's import — assumes oldest
    // first.
    QVector<LogEntry> chronological = m_all;
    std::stable_sort(chronological.begin(), chronological.end(),
                     [](const LogEntry& a, const LogEntry& b) {
        if (a.timeOn.isValid() != b.timeOn.isValid()) {
            return b.timeOn.isValid();
        }
        return a.timeOn < b.timeOn;
    });

    QString err;
    if (AdifLog::write(m_path, chronological, &err)) {
        emit logChanged();
        return true;
    }
    QMessageBox::critical(this, QStringLiteral("Logbook"),
        QStringLiteral("Couldn't save the log:\n%1\n\n"
                       "Your previous log file is untouched.").arg(err));
    return false;
}

namespace {

// Derselbe Kontakt nach einem modalen Dialog wiederfinden. Waehrend
// "Edit" oder die Loesch-Rueckfrage offen stehen, laeuft die
// Ereignisschleife weiter -- ein QSO aus WSJT-X liest das Logbuch neu
// ein, und m_all ist danach neu sortiert (neuestes zuerst): der alte
// Index zeigt auf den Nachbarn. Wiedererkannt am ganzen Datensatz,
// nicht an isSameQso(), das eine Zeittoleranz hat und zwei Kontakte
// derselben Station kurz hintereinander verwechseln wuerde.
int indexOfRecord(const QVector<LogEntry>& all, const QString& record,
                  int hint, const QSet<int>& taken = {})
{
    if (hint >= 0 && hint < all.size() && !taken.contains(hint)
        && all.at(hint).toAdifRecord() == record) {
        return hint;
    }
    for (int i = 0; i < all.size(); ++i) {
        if (!taken.contains(i) && all.at(i).toAdifRecord() == record) { return i; }
    }
    return -1;
}

} // namespace

void LogbookWindow::editSelected()
{
    const int view = m_table->currentRow();
    const int idx  = sourceRow(view);
    if (idx < 0) { return; }

    LogEntry e = m_all.at(idx);
    const QString original = e.toAdifRecord();

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Edit %1").arg(e.call));
    auto* form = new QFormLayout(&dlg);

    auto* call    = new QLineEdit(e.call, &dlg);
    auto* when    = new QDateTimeEdit(e.timeOn.toUTC(), &dlg);
    when->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    when->setCalendarPopup(true);
    // The stored time is UTC and stays UTC. A widget that silently
    // showed local time would rewrite every edited contact by the
    // offset, and nothing would look wrong until someone compared logs.
    //
    // QDateTimeEdit::setTimeZone() is Qt 6.7+ (the ubuntu-24.04-arm
    // release runner's system package is Qt 6.4.2); pre-6.7 there is no
    // way to lock the edit's zone explicitly, so this falls back to the
    // UTC-constructed QDateTime passed to the constructor above, which
    // is the best available approximation on that Qt version.
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    when->setTimeZone(QTimeZone::UTC);
#endif

    auto* band    = new QLineEdit(e.band, &dlg);
    auto* mode    = new QLineEdit(e.mode, &dlg);
    auto* submode = new QLineEdit(e.submode, &dlg);
    auto* sent    = new QLineEdit(e.rstSent, &dlg);
    auto* rcvd    = new QLineEdit(e.rstRcvd, &dlg);
    auto* grid    = new QLineEdit(e.gridSquare, &dlg);
    auto* myGrid  = new QLineEdit(e.myGridSquare, &dlg);

    // One field for either scheme rather than four: SOTA references
    // always carry a '/' (W2/WE-003, G/LD-003), POTA park references
    // never do (US-0005, OE-1234) — that alone is enough to sort a
    // typed reference into the right ADIF field on save (see below).
    // Whichever of the pair is set shows here; both are cleared before
    // the save re-populates one of them, so switching schemes on an
    // existing contact does not leave the old one behind.
    auto* myActivation = new QLineEdit(
        e.mySotaRef.isEmpty() ? e.myPotaRef : e.mySotaRef, &dlg);
    myActivation->setPlaceholderText(
        QStringLiteral("summit or park you're activating, e.g. OE/OO-001 or OE-1234"));
    auto* theirActivation = new QLineEdit(
        e.sotaRef.isEmpty() ? e.potaRef : e.sotaRef, &dlg);
    theirActivation->setPlaceholderText(
        QStringLiteral("their summit/park, if they're activating too"));

    auto* name    = new QLineEdit(e.name, &dlg);
    auto* qth     = new QLineEdit(e.qth, &dlg);
    auto* country = new QLineEdit(e.country, &dlg);
    auto* comment = new QLineEdit(e.comment, &dlg);

    form->addRow(QStringLiteral("Call"),        call);
    form->addRow(QStringLiteral("UTC"),         when);
    form->addRow(QStringLiteral("Band"),        band);
    form->addRow(QStringLiteral("Mode"),        mode);
    form->addRow(QStringLiteral("Submode"),     submode);
    form->addRow(QStringLiteral("RST sent"),    sent);
    form->addRow(QStringLiteral("RST rcvd"),    rcvd);
    form->addRow(QStringLiteral("Their grid"),  grid);
    form->addRow(QStringLiteral("My grid"),     myGrid);
    form->addRow(QStringLiteral("My activation (SOTA/POTA)"), myActivation);
    form->addRow(QStringLiteral("Their activation"), theirActivation);
    form->addRow(QStringLiteral("Name"),        name);
    form->addRow(QStringLiteral("QTH"),         qth);
    form->addRow(QStringLiteral("Country"),     country);
    form->addRow(QStringLiteral("Comment"),     comment);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Save
                                     | QDialogButtonBox::Cancel, &dlg);
    form->addRow(box);
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    // Was der Dialog beim Oeffnen zeigte. Nur ein Feld, dessen Text sich
    // davon unterscheidet, wird uebernommen und dabei in Form gebracht --
    // ein unveraendertes Feld bleibt, wie es in der Datei steht.
    // Nachttest 2026-09-26: 1495 Kontakte im Betreiber-Log tragen den
    // Locator in der ueblichen Schreibweise "PL05sa"; wer nur den
    // Kommentar aenderte, bekam "PL05SA" mitgeschrieben. Ebenso das
    // Datum: ein Kontakt ohne gueltige Zeit bekaeme sonst still, was das
    // Feld ohne Wert anzeigt.
    const QDateTime shownWhen = when->dateTime();
    QHash<const QLineEdit*, QString> shown;
    for (const QLineEdit* le : dlg.findChildren<QLineEdit*>()) { shown.insert(le, le->text()); }
    const auto edited = [&shown](const QLineEdit* le) { return le->text() != shown.value(le); };

    // ── Bei SOTA der Locator des Gipfels (Betreiber 2026-11-10) ──────
    // Steht in "My activation" ein Gipfel, kommt sein Locator sichtbar in
    // "My grid" (ebenso fuer die Gegenseite) -- erst "Save" schreibt ihn.
    // Ein noch unbekannter Gipfel wird nachgeschlagen; die Antwort fuellt
    // das Feld nach, solange der Dialog offen ist.
    const auto gipfelLocator = [myActivation, myGrid, theirActivation, grid]() {
        const SotaGipfelVerzeichnis& v = SotaGipfelVerzeichnis::instanz();
        const QString mein = v.gipfelLocator(myActivation->text());
        if (!mein.isEmpty() && !myGrid->text().trimmed().toUpper().startsWith(mein)) {
            myGrid->setText(mein);
        }
        const QString ihr = v.gipfelLocator(theirActivation->text());
        if (!ihr.isEmpty() && !grid->text().trimmed().toUpper().startsWith(ihr)) {
            grid->setText(ihr);
        }
    };
    const auto nachschlagen = [gipfelLocator](const QString& text) {
        SotaGipfelVerzeichnis::instanz().anfragen(text);
        gipfelLocator();
    };
    connect(myActivation, &QLineEdit::textChanged, &dlg, nachschlagen);
    connect(theirActivation, &QLineEdit::textChanged, &dlg, nachschlagen);
    connect(&SotaGipfelVerzeichnis::instanz(), &SotaGipfelVerzeichnis::gipfelBekannt,
            &dlg, gipfelLocator);
    nachschlagen(myActivation->text());
    nachschlagen(theirActivation->text());

    if (dlg.exec() != QDialog::Accepted) { return; }
    // Waehrend des Dialogs neu eingelesen? Dann steht der Kontakt
    // anderswo -- oder gar nicht mehr.
    const int at = indexOfRecord(m_all, original, idx);
    if (at < 0) { return; }

    if (call->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Logbook"),
            QStringLiteral("A contact needs a callsign."));
        return;
    }

    if (edited(call))    { e.call         = call->text().trimmed().toUpper(); }
    if (when->dateTime() != shownWhen) { e.timeOn = when->dateTime(); }
    if (edited(band))    { e.band         = band->text().trimmed(); }
    if (edited(mode))    { e.mode         = mode->text().trimmed().toUpper(); }
    if (edited(submode)) { e.submode      = submode->text().trimmed().toUpper(); }
    if (edited(sent))    { e.rstSent      = sent->text().trimmed(); }
    if (edited(rcvd))    { e.rstRcvd      = rcvd->text().trimmed(); }
    if (edited(grid))    { e.gridSquare   = grid->text().trimmed().toUpper(); }
    if (edited(myGrid))  { e.myGridSquare = myGrid->text().trimmed().toUpper(); }

    if (edited(myActivation)) {
        e.mySotaRef.clear();
        e.myPotaRef.clear();
        if (const QString mine = myActivation->text().trimmed().toUpper();
            !mine.isEmpty()) {
            if (mine.contains(QLatin1Char('/'))) { e.mySotaRef = mine; }
            else                                 { e.myPotaRef = mine; }
        }
    }
    if (edited(theirActivation)) {
        e.sotaRef.clear();
        e.potaRef.clear();
        if (const QString theirs = theirActivation->text().trimmed().toUpper();
            !theirs.isEmpty()) {
            if (theirs.contains(QLatin1Char('/'))) { e.sotaRef = theirs; }
            else                                   { e.potaRef = theirs; }
        }
    }

    if (edited(name))    { e.name         = name->text().trimmed(); }
    if (edited(qth))     { e.qth          = qth->text().trimmed(); }
    if (edited(country)) { e.country      = country->text().trimmed(); }
    if (edited(comment)) { e.comment      = comment->text().trimmed(); }

    m_all[at] = e;
    if (saveAll()) { reload(); }
}

void LogbookWindow::deleteSelected()
{
    // Collect source indices first: deleting by view row while the view
    // is being rebuilt underneath is how you remove the wrong contact.
    QList<int> victims = selectedSourceRows();
    if (victims.isEmpty()) { return; }
    QList<QPair<QString, int>> records;
    for (int idx : victims) { records.append({m_all.at(idx).toAdifRecord(), idx}); }

    const QString question = victims.size() == 1
        ? QStringLiteral("Delete the contact with %1?")
              .arg(m_all.at(victims.first()).call)
        : QStringLiteral("Delete %1 contacts?").arg(victims.size());

    if (QMessageBox::question(this, QStringLiteral("Logbook"), question,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }

    // Nach der Rueckfrage wiederfinden (siehe indexOfRecord): geloescht
    // wird, was gefragt wurde, nicht was inzwischen an der Stelle steht.
    QSet<int> found;
    for (const auto& [record, hint] : records) {
        const int at = indexOfRecord(m_all, record, hint, found);
        if (at >= 0) { found.insert(at); }
    }
    victims = QList<int>(found.begin(), found.end());
    std::sort(victims.begin(), victims.end(), std::greater<int>());
    for (int idx : victims) { m_all.removeAt(idx); }
    if (saveAll()) { reload(); }
}

// ── Map ─────────────────────────────────────────────────────────────

void LogbookWindow::setPositionFallback(PositionFallback fn)
{
    m_fallback = std::move(fn);
    if (m_detail) { m_detail->setPositionFallback(m_fallback); }
    if (m_map) { m_map->setPositionFallback(m_fallback); }
}

void LogbookWindow::setSatellites(SatelliteService* svc)
{
    m_satellites = svc;
    if (m_map) { m_map->setSatellites(svc); }
}

void LogbookWindow::openMap()
{
    if (!m_map) {
        m_map = new QsoMapWindow(this);
        m_map->setQrzClient(m_qrz);
        m_map->setSatellites(m_satellites);
        // Eine Station, die der Betreiber im Logbuch nachschlaegt, landet
        // auf der offenen Karte — der Flug ist der Sinn der Karte.
        connect(m_detail, &QsoDetailPane::stationLocated, m_map,
                [this](const QString& call, double lat, double lon, const QString& caption) {
            if (m_map && m_map->isVisible()) { m_map->flyToStation(call, lat, lon, caption); }
        });
        connect(m_map, &QsoMapWindow::rotorZielGewuenscht, this,
                [this](double grad, const QString& call) {
            emit turnRotorRequested(grad, call);
        });
    }
    prepareMap(m_map);
    m_map->setEntries(m_all);

    // Rows picked out in the table go to the map as a selection. With
    // none picked, the map falls back to its date range — opening it
    // with nothing selected should still show something.
    QVector<LogEntry> marked;
    for (int idx : selectedSourceRows()) { marked.append(m_all.at(idx)); }
    m_map->setSelection(marked);

    m_map->show();
    m_map->raise();
    m_map->activateWindow();

    // ── Und zwar SICHTBAR (2026-11-13) ──────────────────────────────
    //
    // Betreiber: „neben dem 3d ist ein pfeil, der funktioniert auch
    // nicht." Er funktionierte: das Fenster ging auf, arbeitete (eine
    // Azimutalkarte mit markiertem Kontakt) -- und lag deckungsgleich
    // hinter dem Logbuch. Gemessen an der laufenden App: Karte
    // 1470x923 an (0,33), Logbuch 1468x790 an (1,109). Wer das
    // Logbuch weiter vor sich sieht, schliesst auf „tut nichts", und
    // das zu Recht.
    //
    // `QsoMapGeometry` ist die Lage, die der Betreiber dem Fenster
    // selbst gegeben hat -- die wird nicht ueberschrieben. Nur wenn
    // sie es unter dem Logbuch verschwinden laesst, sucht es sich
    // einen freien Platz. Dieselbe Hilfe und dasselbe Muster wie beim
    // CAT-Feld (MainWindow::zeigeCatRigFeld, 2026-11-07, aus dem
    // gleichen Anlass: „wieder ein window, welches ich nicht oeffnen
    // kann"), einschliesslich `EigeneBewegung` -- ein Zug, den das
    // Programm selbst macht, ist kein Ziehen des Betreibers.
    //
    // Einen Umlauf spaeter, weil show() die endgueltige Rahmenlage
    // erst danach kennt.
    QPointer<QsoMapWindow> karte = m_map;
    QTimer::singleShot(0, this, [this, karte]() {
        if (!karte || !karte->isVisible()) { return; }
        const QRect kartenRahmen = karte->frameGeometry();
        // Verdeckt dieses Logbuch sie so weit, dass man sie uebersieht?
        //
        // Die Zahl stammt aus dem gemeldeten Fall, nicht aus dem Gefuehl:
        // Karte 1470x923 an (0,33), Logbuch 1468x790 an (1,109) -- das
        // Logbuch deckt 0,86 der Karte. Eine Schwelle von 0,90 (der erste
        // Versuch) haette genau diesen Fall durchgelassen; aufgefallen an
        // der Gegenprobe, nicht am Lauf. Bei 0,86 sieht der Betreiber
        // zwei schmale Streifen am Rand, in derselben dunklen Farbe wie
        // alles andere -- das ist kein Sehen.
        //
        // Drei Viertel: darunter steht ein gutes Stueck Karte frei, und
        // wer das Fenster gerade geholt hat, erkennt es daran.
        constexpr double kVerdecktGenug = 0.75;
        const double verdeckt =
            Longpath::coveredFraction(kartenRahmen, { frameGeometry() });
        if (verdeckt < kVerdecktGenug) { return; }

        QScreen* schirm = karte->screen() ? karte->screen() : QGuiApplication::primaryScreen();
        if (!schirm) { return; }
        const QRect platz = Longpath::freierPlatz(
            kartenRahmen.size(), schirm->availableGeometry(), { frameGeometry() });
        if (!platz.isEmpty()) {
            const Longpath::EigeneBewegung eigene;
            karte->move(platz.topLeft());
            karte->raise();
            return;
        }

        // Kein freier Platz -- und das ist der haeufige Fall, nicht der
        // seltene: die Karte des Betreibers misst 1470x923 auf einem
        // Schirm von 1470x956. Ein bildschirmfuellendes Fenster kann
        // nirgends ausweichen.
        //
        // Dann tritt das Logbuch zurueck. Nicht „die Karte nach vorne" --
        // das war der erste Versuch, und er genuegt nicht: `show()` +
        // `raise()` stehen oben schon und haben genau dieses Verdecken
        // nicht verhindert. Umgekehrt ist es ausserdem das richtige
        // Verhaeltnis: der Betreiber hat eben auf den Pfeil gedrueckt,
        // also will er die Karte sehen, nicht das Logbuch.
        //
        // Das Logbuch bleibt offen und an seinem Platz; es liegt nur
        // darunter, und ein Klick holt es jederzeit zurueck.
        lower();
        karte->raise();
        karte->activateWindow();
    });
}

// ── Import ──────────────────────────────────────────────────────────

QString LogbookWindow::makeBackup(QString* error) const
{
    if (!QFile::exists(m_path)) { return {}; }   // nothing to lose yet

    const QString stamp = QDateTime::currentDateTimeUtc()
                              .toString(QStringLiteral("yyyyMMdd-hhmmss"));
    const QString dest = m_path + QStringLiteral(".") + stamp
                       + QStringLiteral(".bak");
    if (!QFile::copy(m_path, dest)) {
        if (error) {
            *error = QStringLiteral("couldn't write %1").arg(dest);
        }
        return {};
    }
    return dest;
}

void LogbookWindow::importAdif()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import ADIF"), QString{},
        QStringLiteral("ADIF (*.adi *.adif);;All files (*)"));
    if (path.isEmpty()) { return; }
    importAdifFile(path);
}

// ── Der Dateidialog und das Einlesen sind zwei Dinge ────────────────
//
// Sie standen in einer Funktion, und damit war der ganze Weg —
// lesen, zusammenfuehren, sichern, in die Tabelle stellen — nur mit
// einem Klick eines Menschen erreichbar. Kein Test kam daran vorbei,
// weil getOpenFileName stehenbleibt und auf eine Maus wartet.
//
// Der Betreiber am 2026-08-21: „logbuch leer". Ob daran die Knoepfe,
// das Einlesen oder das Zusammenfuehren schuld war, liess sich nicht
// nachsehen, sondern nur raten. Genau der Zustand, aus dem in dieser
// Sitzung schon mehrere Fehlschluesse gekommen sind.
//
// Jetzt nimmt importAdifFile() einen Pfad. Der Dialog sucht ihn aus,
// ein Test reicht ihn hin. Derselbe Weg, beide Male.
void LogbookWindow::importAdifFile(const QString& path)
{
    QString err;
    const QVector<LogEntry> incoming = AdifLog::read(path, &err);
    if (!err.isEmpty()) {
        tellOperator(QStringLiteral("Couldn't read that file:\n%1").arg(err));
        return;
    }
    if (incoming.isEmpty()) {
        // Distinguish "empty" from "not ADIF": a file the parser found
        // no records in is usually the wrong file, not an empty log.
        tellOperator(
            QStringLiteral("No contacts found in that file.\n\n"
                           "ADIF records are marked with <EOR>; if this "
                           "is a Cabrillo or CSV export it needs "
                           "converting first."));
        return;
    }

    importEntries(incoming, QStringLiteral("that file"), /*fromQrz*/ false);
}

void LogbookWindow::importEntries(const QVector<LogEntry>& incoming,
                                  const QString& source, bool fromQrz)
{
    AdifLog::MergeResult r = AdifLog::merge(m_all, incoming);

    // Was aus dem QRZ-Logbuch kommt, IST im QRZ-Logbuch — der Upload
    // braucht es nicht mehr zu schicken. uploadedToQrz ist kein Feld,
    // das der Betreiber sieht und pflegt, also darf es hier gesetzt
    // werden, auch an Kontakten, die er schon hatte.
    int markedQrz = 0;
    if (fromQrz) {
        QHash<QString, QVector<const LogEntry*>> byCall;
        for (const LogEntry& in : incoming) {
            byCall[in.call.trimmed().toUpper()].append(&in);
        }
        for (LogEntry& e : r.merged) {
            if (e.uploadedToQrz) { continue; }
            const auto it = byCall.constFind(e.call.trimmed().toUpper());
            if (it == byCall.constEnd()) { continue; }
            for (const LogEntry* in : it.value()) {
                if (AdifLog::isSameQso(e, *in)) { e.uploadedToQrz = true; ++markedQrz; break; }
            }
        }
    }

    // "Nothing new" is not the same as "nothing to do". A confirmation
    // report from LoTW or eQSL is a file of contacts you already have,
    // and the whole point of importing it is the fields it carries that
    // your copies do not. Bailing out on added == 0 threw those away
    // and told the operator everything was fine.
    if (r.added == 0 && r.enriched == 0 && markedQrz == 0) {
        tellOperator(
            QStringLiteral("All %1 contacts in %2 are already in "
                           "your log, and none of them carried anything "
                           "your copies were missing. Nothing to do.")
                .arg(incoming.size()).arg(source));
        return;
    }

    QString question =
        QStringLiteral("%1 contacts in %4.\n\n"
                       "%2 are new and will be added.\n"
                       "%3 are already in your log.\n")
            .arg(incoming.size()).arg(r.added).arg(r.skipped).arg(source);
    if (markedQrz > 0) {
        question += QStringLiteral(
            "\n%1 of yours will be marked as present in your QRZ logbook, "
            "so Upload will not send them again.\n").arg(markedQrz);
    }
    if (r.enriched > 0) {
        question += QStringLiteral(
            "\nOf those, %1 carry fields your copies do not have — "
            "confirmations, awards data or another logger's notes. Those "
            "fields will be filled in. Nothing you can see and edit in "
            "this window is overwritten.\n")
            .arg(r.enriched);
    }
    question += QStringLiteral(
        "\nYour current log is copied to a dated backup first. "
        "Go ahead?");

    if (!askOperator(question)) { return; }

    // Back up before touching anything. This is the one operation here
    // that rewrites many records at once, and unlike an edit or a delete
    // it cannot be put right from the table afterwards.
    QString backupErr;
    const QString backup = makeBackup(&backupErr);
    if (!backupErr.isEmpty()) {
        if (!askOperator(
                QStringLiteral("Couldn't make a backup first: %1\n\n"
                               "Import anyway?").arg(backupErr))) {
            return;
        }
    }

    m_all = r.merged;
    if (!saveAll()) {
        // saveAll already explained itself and left the old file intact.
        reload();
        return;
    }
    reload();

    QString done = QStringLiteral("Added %1 contacts, skipped %2 already "
                                  "present.").arg(r.added).arg(r.skipped);
    if (r.enriched > 0) {
        done += QStringLiteral("\nFilled in missing fields on %1 of them.")
                    .arg(r.enriched);
    }
    if (markedQrz > 0) {
        done += QStringLiteral("\nMarked %1 as present in your QRZ logbook.")
                    .arg(markedQrz);
    }
    if (!backup.isEmpty()) {
        done += QStringLiteral("\n\nBackup: %1").arg(backup);
    }
    tellOperator(done);
}

// ── Wer die Rueckfragen stellt ──────────────────────────────────────
//
// Voreingestellt ein Fenster, wie bisher. Ein Test setzt stattdessen
// eine Funktion ein, die „ja" sagt und die Meldungen mitschreibt.
//
// Der Grund ist nicht Bequemlichkeit: ohne diese Stelle ist der
// Import-Weg fuer einen Test unerreichbar, weil QMessageBox auf eine
// Maus wartet. Ein Weg, den kein Test gehen kann, ist ein Weg, ueber
// den man nur Vermutungen hat — und davon hatte diese Sitzung genug.
bool LogbookWindow::askOperator(const QString& question)
{
    if (m_ask) { return m_ask(question); }
    return QMessageBox::question(this, QStringLiteral("Import"), question,
               QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
           == QMessageBox::Yes;
}

void LogbookWindow::tellOperator(const QString& message)
{
    if (m_tell) { m_tell(message); return; }
    QMessageBox::information(this, QStringLiteral("Import"), message);
}

// ── Export ──────────────────────────────────────────────────────────

void LogbookWindow::exportAdif()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export ADIF"),
        QStringLiteral("longpath-log.adi"),
        QStringLiteral("ADIF (*.adi *.adif)"));
    if (path.isEmpty()) { return; }

    // Exports what is on screen, not the whole file. Filtering to one
    // band or one callsign and exporting that is a normal thing to want,
    // and the count is shown so nobody exports 12 records thinking they
    // got 1200.
    QVector<LogEntry> subset;
    subset.reserve(m_visible.size());
    for (int i : m_visible) { subset.append(m_all.at(i)); }

    QString err;
    if (!AdifLog::write(path, subset, &err)) {
        QMessageBox::warning(this, QStringLiteral("Logbook"),
            QStringLiteral("Export failed:\n%1").arg(err));
        return;
    }
    QMessageBox::information(this, QStringLiteral("Logbook"),
        QStringLiteral("Exported %1 contacts.").arg(subset.size()));
}

void LogbookWindow::exportCsv()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export CSV"),
        QStringLiteral("longpath-log.csv"),
        QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) { return; }

    QVector<LogEntry> subset;
    subset.reserve(m_visible.size());
    for (int i : m_visible) { subset.append(m_all.at(i)); }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Logbook"),
            QStringLiteral("Export failed:\n%1").arg(f.errorString()));
        return;
    }
    QTextStream out(&f);
    out << AdifLog::toCsv(subset);
    out.flush();
    QMessageBox::information(this, QStringLiteral("Logbook"),
        QStringLiteral("Exported %1 contacts.").arg(subset.size()));
}

// ── Cabrillo (2026-08-10) ───────────────────────────────────────────

void LogbookWindow::exportCabrillo()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export Cabrillo"),
        QStringLiteral("longpath-log.cbr"),
        QStringLiteral("Cabrillo (*.cbr *.log)"));
    if (path.isEmpty()) { return; }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Logbook"),
            QStringLiteral("Export failed:\n%1").arg(f.errorString()));
        return;
    }

    const QString myCall = AppSettings::instance()
        .value(QStringLiteral("User/Callsign"), QString{})
        .toString().trimmed().toUpper();

    // Mode → Cabrillo's two letters. Everything digital is DG except
    // RTTY, which contests treat as its own thing.
    auto cbrMode = [](const LogEntry& e) {
        const QString m = e.mode.trimmed().toUpper();
        if (m == QLatin1String("CW"))  { return QStringLiteral("CW"); }
        if (m == QLatin1String("SSB") || m == QLatin1String("USB")
            || m == QLatin1String("LSB") || m == QLatin1String("AM")
            || m == QLatin1String("FM")) { return QStringLiteral("PH"); }
        if (m == QLatin1String("RTTY")) { return QStringLiteral("RY"); }
        return QStringLiteral("DG");
    };
    // Frequency in kHz; when only the band is known, its lower edge —
    // Cabrillo robots accept a band-representative figure.
    auto cbrFreq = [](const LogEntry& e) {
        if (e.freqMHz > 0.0) {
            return QString::number(qRound(e.freqMHz * 1000.0));
        }
        static const QHash<QString, int> kEdge = {
            {QStringLiteral("160m"), 1800}, {QStringLiteral("80m"), 3500},
            {QStringLiteral("60m"), 5330},  {QStringLiteral("40m"), 7000},
            {QStringLiteral("30m"), 10100}, {QStringLiteral("20m"), 14000},
            {QStringLiteral("17m"), 18068}, {QStringLiteral("15m"), 21000},
            {QStringLiteral("12m"), 24890}, {QStringLiteral("10m"), 28000},
            {QStringLiteral("6m"), 50000},  {QStringLiteral("2m"), 144000},
        };
        return QString::number(
            kEdge.value(e.band.trimmed().toLower(), 0));
    };

    QTextStream out(&f);
    out << "START-OF-LOG: 3.0\n"
        << "CREATED-BY: Longpath\n"
        << "CALLSIGN: " << (myCall.isEmpty()
                                ? QStringLiteral("NOCALL") : myCall)
        << "\n";

    int written = 0;
    for (int i : m_visible) {
        const LogEntry& e = m_all.at(i);
        const QDateTime u = e.timeOn.toUTC();
        if (!u.isValid() || e.call.trimmed().isEmpty()) { continue; }

        // Exchange: the grid where one is known, a placeholder where
        // not. Contests differ; the dialog below says to check this.
        const QString sentX = QStringLiteral("---");
        const QString rcvdX = e.gridSquare.trimmed().isEmpty()
            ? QStringLiteral("---")
            : e.gridSquare.trimmed().toUpper().left(4);

        out << QStringLiteral("QSO: %1 %2 %3 %4 %5 %6 %7 %8 %9 %10\n")
            .arg(cbrFreq(e), 5)
            .arg(cbrMode(e))
            .arg(u.toString(QStringLiteral("yyyy-MM-dd")))
            .arg(u.toString(QStringLiteral("hhmm")))
            .arg(myCall.isEmpty() ? QStringLiteral("NOCALL") : myCall, -10)
            .arg(e.rstSent.trimmed().isEmpty()
                     ? QStringLiteral("59") : e.rstSent.trimmed(), 3)
            .arg(sentX, 6)
            .arg(e.call.trimmed().toUpper(), -10)
            .arg(e.rstRcvd.trimmed().isEmpty()
                     ? QStringLiteral("59") : e.rstRcvd.trimmed(), 3)
            .arg(rcvdX, 6);
        ++written;
    }
    out << "END-OF-LOG:\n";
    out.flush();

    QMessageBox::information(this, QStringLiteral("Logbook"),
        QStringLiteral("Wrote %1 QSO lines.\n\nCabrillo exchanges differ "
                       "by contest — check the exchange columns against "
                       "the rules before submitting.").arg(written));
}

// ── Statistics (2026-08-10) ─────────────────────────────────────────

void LogbookWindow::showStatistics()
{
    // The Kennzahlen view (2026-09-18, benchmark item 4): six tiles over
    // the filtered view -- same rule as every export here: filter to one
    // year or one band and the numbers answer for exactly that. One
    // modeless dialog, reused, refreshed whenever the log or the filter
    // changes while it is open.
    if (!m_statsDialog) {
        m_statsDialog = new QDialog(this);
        m_statsDialog->setWindowTitle(QStringLiteral("Logbook statistics"));
        m_statsDialog->setModal(false);
        m_statsDialog->resize(900, 560);
        m_statsDialog->setStyleSheet(QStringLiteral("QDialog { background: %1; }")
                                         .arg(QLatin1String(Style::kAppBg)));
        auto* lay = new QVBoxLayout(m_statsDialog);
        lay->setContentsMargins(0, 0, 0, 0);

        // Drei Seiten (2026-11-08): die Kacheln, Band x Stunde, Richtungen;
        // drei weitere (2026-11-10): Sonnenzyklus, Kontinent-Uhr, DXCC-Wand.
        // Alle zeigen, was die Filter des Logbuchs zeigen.
        auto* reiter = new QHBoxLayout;
        reiter->setContentsMargins(10, 8, 10, 0);
        reiter->setSpacing(6);
        const QStringList namen = {QStringLiteral("Overview"), QStringLiteral("Band × hour"),
                                   QStringLiteral("Directions"), QStringLiteral("Solar cycle"),
                                   QStringLiteral("Continent clock"), QStringLiteral("DXCC wall")};
        m_statsSeitenKnoepfe.clear();
        for (int i = 0; i < namen.size(); ++i) {
            auto* k = new QPushButton(namen[i], m_statsDialog);
            k->setCheckable(true);
            k->setAutoExclusive(true);
            k->setStyleSheet(Style::buttonBaseStyle() + Style::blueCheckedStyle());
            // toggled, nicht clicked (Live-Befund 2026-11-11, wie beim 3D-
            // Knopf): die Bedienhilfe (AXPress, VoiceOver, Automatisierung)
            // drueckt einen ankreuzbaren Knopf ueber toggle(), und das sendet
            // KEIN clicked -- der Reiter rastete ein, die Seite blieb stehen.
            // zeigeStatistikSeite() stellt die Seite vor setChecked() um;
            // dessen toggled kommt hier mit currentIndex() == i an und ruft
            // nicht ein zweites Mal.
            connect(k, &QPushButton::toggled, this, [this, i](bool an) {
                if (an && m_statsSeiten && m_statsSeiten->currentIndex() != i) {
                    zeigeStatistikSeite(i);
                }
            });
            reiter->addWidget(k);
            m_statsSeitenKnoepfe << k;
        }
        reiter->addStretch(1);
        lay->addLayout(reiter);
        m_statsSeiten = new QStackedWidget(m_statsDialog);

        auto* scroll = new QScrollArea(m_statsSeiten);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setStyleSheet(QStringLiteral("QScrollArea { background: %1; border: none; }")
                                  .arg(QLatin1String(Style::kAppBg)));
        m_statsView = new LogbookStatsWidget(scroll);
        scroll->setWidget(m_statsView);
        m_statsSeiten->addWidget(scroll);

        auto* seiteB = new QWidget(m_statsSeiten);
        {
            auto* v = new QVBoxLayout(seiteB);
            v->setContentsMargins(10, 8, 10, 8);
            auto* zeile = new QHBoxLayout;
            auto* l = new QLabel(QStringLiteral("Contacts with"), seiteB);
            l->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                                 .arg(QLatin1String(Style::kTextSecondary)));
            zeile->addWidget(l);
            m_bandStundeFilter = new QComboBox(seiteB);
            m_bandStundeFilter->setObjectName(QStringLiteral("bandStundeFilter"));
            m_bandStundeFilter->addItem(QStringLiteral("all continents"), QString());
            m_bandStundeFilter->addItem(QStringLiteral("outside Europe"), QStringLiteral("!EU"));
            for (const char* k : {"EU", "NA", "SA", "AF", "AS", "OC"}) {
                m_bandStundeFilter->addItem(QString::fromLatin1(k), QString::fromLatin1(k));
            }
            connect(m_bandStundeFilter, &QComboBox::currentIndexChanged, this,
                    [this](int) { refreshStatsView(); });
            zeile->addWidget(m_bandStundeFilter);
            zeile->addStretch(1);
            v->addLayout(zeile);
            m_bandStunde = new BandStundeWidget(seiteB);
            v->addWidget(m_bandStunde);
            v->addStretch(1);
        }
        m_statsSeiten->addWidget(seiteB);

        m_rose = new RichtungsRoseWidget(m_statsSeiten);
        m_statsSeiten->addWidget(m_rose);

        m_sonne = new SonnenzyklusWidget(m_statsSeiten);
        m_statsSeiten->addWidget(m_sonne);
        m_uhr = new KontinentUhrWidget(m_statsSeiten);
        m_statsSeiten->addWidget(m_uhr);
        m_wand = new DxccWandWidget(m_statsSeiten);
        m_statsSeiten->addWidget(m_wand);
        m_flux = new SolarFluxArchiv(QString(), this);
        // Kommen neue Flux-Werte, solange die Seite offen ist: nachziehen.
        connect(m_flux, &SolarFluxArchiv::aktualisiert, this, [this]() {
            if (m_statsSeiten && m_statsSeiten->currentWidget() == m_sonne) { refreshStatsView(); }
        });
        lay->addWidget(m_statsSeiten, 1);
        m_statsSeitenKnoepfe.first()->setChecked(true);
        auto* closeBtn = new QPushButton(QStringLiteral("Close"), m_statsDialog);
        closeBtn->setStyleSheet(Style::buttonBaseStyle());
        connect(closeBtn, &QPushButton::clicked, m_statsDialog, &QDialog::hide);
        auto* foot = new QHBoxLayout;
        foot->setContentsMargins(10, 4, 10, 10);
        foot->addStretch(1);
        foot->addWidget(closeBtn);
        lay->addLayout(foot);
    }
    m_statsDialog->show();
    m_statsDialog->raise();
    m_statsDialog->activateWindow();
    refreshStatsView();   // after show(): it only works on a visible dialog
}

QSize LogbookWindow::gesamtbildGroesse(const QWidget* dialog)
{
    QSize z(1240, 820);
    if (const QScreen* sc = dialog ? dialog->screen() : nullptr) {
        z = z.boundedTo(sc->availableGeometry().size() - QSize(40, 60));
    }
    return z;
}

void LogbookWindow::zeigeStatistikSeite(int seite)
{
    showStatistics();
    if (!m_statsSeiten) { return; }
    seite = std::clamp(seite, 0, m_statsSeiten->count() - 1);
    m_statsSeiten->setCurrentIndex(seite);
    if (seite < m_statsSeitenKnoepfe.size()) { m_statsSeitenKnoepfe[seite]->setChecked(true); }
    // Die drei Gesamtbilder brauchen mehr Flaeche als die Kacheln: das
    // Fenster waechst (nie schrumpft es), soweit der Bildschirm reicht.
    if (m_statsDialog && seite >= 3) {
        const QSize ziel = m_statsDialog->size().expandedTo(gesamtbildGroesse(m_statsDialog));
        if (ziel != m_statsDialog->size()) { m_statsDialog->resize(ziel); }
    }
    refreshStatsView();
}

void LogbookWindow::refreshStatsView()
{
    const bool dialogWants = m_statsView && m_statsDialog && m_statsDialog->isVisible();
    // isVisibleTo(this): vor dem ersten show() ist nichts „visible", die
    // Reihe soll aber schon beim Aufgehen gefuellt sein.
    const bool rowWants    = m_statsRow && m_statsSection && m_statsSection->isVisibleTo(this);
    if (!dialogWants && !rowWants) { return; }
    QVector<LogEntry> shown;
    shown.reserve(m_visible.size());
    for (int i : m_visible) { shown.push_back(m_all.at(i)); }
    const LogbookStats stats = LogbookStats::compute(shown, m_cty,
                                                     QDateTime::currentDateTimeUtc(),
                                                     meinFeld());
    if (dialogWants) { m_statsView->setStats(stats); }
    if (rowWants)    { m_statsRow->setStats(stats); }

    // Die zwei weiteren Seiten -- nur die gezeigte rechnet.
    if (dialogWants && m_statsSeiten) {
        const int seite = m_statsSeiten->currentIndex();
        if (seite == 1 && m_bandStunde) {
            const QString k = m_bandStundeFilter ? m_bandStundeFilter->currentData().toString()
                                                 : QString();
            const CtyDatParser* cty = m_cty;
            auto nimm = [cty, k](const LogEntry& e) {
                if (k.isEmpty() || !cty) { return true; }
                const DxccEntity* d = cty->entityByPrefix(cty->resolvePrimaryPrefix(e.call));
                if (!d) { return false; }
                return k == QLatin1String("!EU") ? d->continent != QLatin1String("EU")
                                                 : d->continent == k;
            };
            m_bandStunde->setDaten(LogbuchBilder::bandStunde(shown, nimm),
                                   m_bandStundeFilter ? m_bandStundeFilter->currentText()
                                                      : QStringLiteral("all"),
                                   meinFeld());
        }
        if (seite == 2 && m_rose) {
            m_rose->setRose(LogbuchBilder::rose(shown, m_cty, meinFeld()),
                            QStringLiteral("this view"));
            m_rose->setRotor(m_rotorDeg, m_rotorBeamDeg, RotorBereich::load());
        }
        if (seite == 3 && m_sonne && m_flux) {
            m_flux->ladeCache();
            // Pruefstaende holen nie aus dem Netz; die Regel, WANN geholt
            // wird, steht in SolarFluxArchiv::brauchtAbruf.
            if (!QStandardPaths::isTestModeEnabled()) {
                m_flux->holeWennNoetig(QDateTime::currentDateTimeUtc());
            }
            m_sonne->setDaten(LogbuchBilder::sonnenzyklus(shown, m_flux->werte(),
                                                          QDateTime::currentDateTimeUtc().date()),
                              m_flux->stand(), m_flux->laeuft());
        }
        if (seite == 4 && m_uhr) {
            m_uhr->setDaten(LogbuchBilder::kontinentUhr(shown, m_cty));
        }
        if (seite == 5 && m_wand) {
            m_wand->setDaten(LogbuchBilder::dxccWand(shown, m_cty));
        }
    }
}

// ── Eine Seite: Karte und Kennzahlen im Fenster ──────────────────────

void LogbookWindow::prepareMap(QsoMapWindow* map)
{
    map->setQrzClient(m_qrz);
    map->setSatellites(m_satellites);
    map->setPositionFallback(m_fallback);
    // Home comes from whichever contact recorded it most recently. The
    // panel knows the operator's locator, but the map is opened from
    // here — and the log carries the same answer, with the advantage of
    // being right for an imported log made from a different station.
    QString home;
    for (const LogEntry& e : m_all) {
        // Ein QSO vom Gipfel sagt nicht, wo zu Hause ist -- seit es den
        // Locator des Gipfels traegt (2026-11-10), waere sonst nach jeder
        // Aktivierung der Gipfel das Zuhause.
        if (!e.mySotaRef.trimmed().isEmpty()) { continue; }
        if (!e.myGridSquare.trimmed().isEmpty()) {
            home = e.myGridSquare;
            break;   // m_all is newest first
        }
    }
    map->setHomeGrid(home);
    // Das Mosaik der Locator-Jagd fragt das GANZE Logbuch, auch wenn die
    // eingebettete Karte nur die gefilterten Zeilen zeigt (2026-11-11).
    map->setMosaikLog(m_all);
}

QPushButton* LogbookWindow::mapToggleForTest() const
{
    return m_modusGruppe ? qobject_cast<QPushButton*>(m_modusGruppe->button(ModusLog)) : nullptr;
}

QPushButton* LogbookWindow::statsToggleForTest() const
{
    return m_modusGruppe ? qobject_cast<QPushButton*>(m_modusGruppe->button(ModusStats)) : nullptr;
}

QPushButton* LogbookWindow::threeDToggleForTest() const
{
    return m_mapPanel ? m_mapPanel->threeDButtonForTest() : nullptr;
}

void LogbookWindow::ensureMapPanel()
{
    if (m_mapPanel) { return; }
    m_mapPanel = new QsoMapWindow(this);
    m_mapPanel->setEmbedded(true);
    // Gleich in eine Seite: ein Kind ohne Layout laege sonst frei ueber
    // dem Fenster (gesehen beim Oeffnen direkt in „Statistics").
    m_logMapLay->addWidget(m_mapPanel, 1);
    // Der Rotor steht in der Stationskarte -- das kleine Radar links der
    // Karte zeigte dasselbe ein zweites Mal.
    m_mapPanel->setRotorRadarShown(false);
    applyRotorToMap();
    connect(m_mapPanel, &QsoMapWindow::popOutRequested, this, &LogbookWindow::openMap);
    // Die Jagdliste darf die Antenne drehen -- das Kartenfenster
    // kennt keinen Rotor, das Logbuch schon (2026-11-11).
    connect(m_mapPanel, &QsoMapWindow::rotorZielGewuenscht, this,
            [this](double grad, const QString& call) {
        emit turnRotorRequested(grad, call);
    });
    connect(m_detail, &QsoDetailPane::stationLocated, m_mapPanel,
            [this](const QString& call, double lat, double lon, const QString& caption) {
        if (m_mapPanel && m_mapPanel->isVisibleTo(this)) {
            m_mapPanel->flyToStation(call, lat, lon, caption);
        }
    });
    connect(m_mapPanel, &QsoMapWindow::dreiDSichtbar, this, &LogbookWindow::spiegle3d);
    // 3D steht seit Entwurf C in der Kartenleiste; es fliegt zur Auswahl.
    m_mapPanel->setAuswahlRuf([this]() {
        const QList<int> rows = selectedSourceRows();
        return rows.isEmpty() ? QString() : m_all.at(rows.first()).call;
    });
}

void LogbookWindow::setModus(int modus)
{
    modus = std::clamp(modus, 0, 2);
    m_modus = modus;
    ensureMapPanel();
    // Karte, Station und Liste in die Seite, die jetzt gilt.
    if (modus == ModusLog) {
        m_logKarteLay->addWidget(m_detailScroll);
        m_logMapLay->addWidget(m_mapPanel, 1);
        m_logListeLay->addWidget(m_table, 1);
        m_mapPanel->setKompakt(false);
        // Das kleine Rotor-Radar neben der Karte (Betreiber 2026-11-10:
        // „sehen, wohin der rotor zeigt und wo das call steht").
        m_mapPanel->setRotorRadarShown(true);
    } else if (modus == ModusBrowse) {
        m_browseKarteLay->addWidget(m_detailScroll);
        m_browseMapLay->addWidget(m_mapPanel, 1);
        m_browseListeLay->addWidget(m_table, 1);
        // Klein ueber der Station: nur das Bild, die Leiste waere dort
        // drei Zeilen hoch.
        m_mapPanel->setKompakt(true);
        m_mapPanel->setRotorRadarShown(false);
    }
    if (modus != ModusStats) {
        // Umgehaengt heisst versteckt, bis Qt sie wieder zeigt -- gleich
        // zeigen, damit die Pruefungen danach stimmen.
        for (QWidget* w : {static_cast<QWidget*>(m_detailScroll), static_cast<QWidget*>(m_mapPanel),
                           static_cast<QWidget*>(m_table)}) {
            if (w) { w->show(); }
        }
    }
    m_seiten->setCurrentIndex(modus);
    if (m_eingabe)    { m_eingabe->setVisible(modus == ModusLog); }
    if (m_auswahlBox) { m_auswahlBox->setVisible(modus == ModusBrowse); }
    if (QAbstractButton* b = m_modusGruppe ? m_modusGruppe->button(modus) : nullptr) {
        if (!b->isChecked()) { b->setChecked(true); }
    }
    static const char* const hinweis[] = {
        "enter a call — station, path and what is still needed",
        "search, filter and edit the whole log",
        "what the filters in Browse show",
    };
    if (m_modusHinweis) { m_modusHinweis->setText(QString::fromUtf8(hinweis[modus])); }
    static const char* const namen[] = {"Log", "Browse", "Statistics"};
    AppSettings::instance().setValue(QStringLiteral("LogbookMode"), QString::fromLatin1(namen[modus]));
    if (modus == ModusStats) {
        refreshStatsView();
    } else {
        refreshMapPanel();
    }
    spiegle3d();
}

void LogbookWindow::setMapPanelShown(bool on)
{
    // Seit Entwurf C gehoert die Karte zu „Log" und „Browse"; wer sie
    // will, bekommt sie dort.
    if (on && m_modus == ModusStats) { setModus(ModusLog); }
    ensureMapPanel();
    m_mapPanel->setVisible(on);
    if (on) { refreshMapPanel(); }
    spiegle3d();
}

void LogbookWindow::set3dShown(bool on)
{
    if (on) {
        setMapPanelShown(true);
        QString call;
        const QList<int> rows = selectedSourceRows();
        if (!rows.isEmpty()) { call = m_all.at(rows.first()).call; }
        if (m_mapPanel) { m_mapPanel->zeige3d(call); }
    } else if (m_mapPanel) {
        m_mapPanel->verlasse3d();
    }
    spiegle3d();
}

void LogbookWindow::spiegle3d()
{
    // Seit Entwurf C zeigt der 3D-Knopf der Karte selbst, ob das 3D-Bild
    // steht (QsoMapWindow::update3dButton); hier ist nichts mehr zu spiegeln.
}

void LogbookWindow::setStatsRowShown(bool on)
{
    if (on) { setModus(ModusStats); }
    else if (m_modus == ModusStats) { setModus(ModusLog); }
}

void LogbookWindow::refreshMapPanel()
{
    if (!m_mapPanel || !m_mapPanel->isVisibleTo(this)) { return; }
    prepareMap(m_mapPanel);
    QVector<LogEntry> shown;
    shown.reserve(m_visible.size());
    for (int i : m_visible) { shown.push_back(m_all.at(i)); }
    m_mapPanel->setEntries(shown);
    syncMapPanelSelection();
}

void LogbookWindow::syncMapPanelSelection()
{
    if (!m_mapPanel || !m_mapPanel->isVisibleTo(this)) { return; }
    QVector<LogEntry> marked;
    for (int idx : selectedSourceRows()) { marked.append(m_all.at(idx)); }
    m_mapPanel->setSelection(marked);
}


// ── QRZ-Logbuch abholen ──────────────────────────────────────────────

void LogbookWindow::setQrzLogbookUploader(QrzLogbookUploader* uploader)
{
    m_qrzUploader = uploader;
}

void LogbookWindow::syncFromQrz()
{
    const QString key = m_qrzUploader ? m_qrzUploader->apiKey() : QString();
    if (key.trimmed().isEmpty()) {
        tellOperator(QStringLiteral(
            "No QRZ logbook key yet.\n\nAdd it under Tools > QRZ — it is the "
            "logbook access key from your QRZ logbook's settings page, not "
            "your QRZ password."));
        return;
    }
    if (!m_qrzFetcher) {
        m_qrzFetcher = new QrzLogbookFetcher(this);
        connect(m_qrzFetcher, &QrzLogbookFetcher::progress, this,
                [this](int records, int page) {
            if (m_syncBtn) {
                m_syncBtn->setText(QStringLiteral("Syncing… %1 (page %2)")
                                       .arg(records).arg(page));
            }
        });
        connect(m_qrzFetcher, &QrzLogbookFetcher::finished, this,
                [this](bool ok, const QString& adif, int records, const QString& error) {
            if (m_syncBtn) {
                m_syncBtn->setEnabled(true);
                m_syncBtn->setText(QStringLiteral("Sync QRZ"));
                m_syncBtn->setVisible(false);
            }
            if (!ok) {
                tellOperator(QStringLiteral("Couldn't fetch your QRZ logbook:\n%1")
                                 .arg(error));
                return;
            }
            if (records == 0 || adif.trimmed().isEmpty()) {
                tellOperator(QStringLiteral("Your QRZ logbook is empty — nothing to merge."));
                return;
            }
            mergeFetchedEntries(adif);
        });
    }
    if (m_qrzFetcher->isBusy()) { return; }
    m_qrzFetcher->setApiKey(key);
    if (m_syncBtn) {
        m_syncBtn->setVisible(true);   // der Fortschritt steht in der Leiste
        m_syncBtn->setEnabled(false);
        m_syncBtn->setText(QStringLiteral("Syncing…"));
    }
    m_qrzFetcher->fetchAll();
}

void LogbookWindow::mergeFetchedEntries(const QString& adif)
{
    QVector<LogEntry> incoming = AdifLog::parse(adif);
    if (incoming.isEmpty()) {
        tellOperator(QStringLiteral(
            "QRZ sent data, but no contacts could be read from it."));
        return;
    }
    // Aus dem QRZ-Logbuch kommt, was dort steht — die Markierung
    // „hochgeladen" gilt fuer jeden dieser Datensaetze.
    for (LogEntry& e : incoming) { e.uploadedToQrz = true; }
    importEntries(incoming, QStringLiteral("your QRZ logbook"), /*fromQrz*/ true);
}


// ── Eingabezeile "NEW QSO" (2026-09-26) ─────────────────────────────────
//
// Betreiber: "ich sehe hier nirgendwo, wo ich das QSO loggen koennte, die
// Frequenz und die Betriebsart geht auch ab. Ich will dieses auch zum
// Loggen verwenden." Gewaehlt: Blatt A -- eine Zeile ueber der Suche.
// Das Rufzeichen filtert zugleich das Log darunter (war er schon da?)
// und zeigt die Station in der Karteikarte. Enter loggt, Esc leert.

namespace {

// 14215800 -> "14.215.800", wie die Frequenz am Funkgeraet steht.
QString dottedHz(double hz)
{
    const qint64 v = qRound64(hz);
    const qint64 mhz = v / 1000000;
    const qint64 khz = (v / 1000) % 1000;
    const qint64 rest = v % 1000;
    return QStringLiteral("%1.%2.%3").arg(mhz)
        .arg(khz, 3, 10, QLatin1Char('0'))
        .arg(rest, 3, 10, QLatin1Char('0'));
}

QString chipStyle()
{
    return QStringLiteral(
        "QLabel { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 6px; padding: 2px 8px; font-size: 11px; font-weight: bold; }")
        .arg(QLatin1String(Style::kGreenBg), QLatin1String(Style::kGreenText),
             QLatin1String(Style::kGreenBorder));
}

} // namespace

void LogbookWindow::buildEntryRow(QVBoxLayout* col)
{
    auto* row = new FlowLayout(nullptr, 6, 6);

    auto* caption = new QLabel(QStringLiteral("NEW QSO"), this);
    caption->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: %2px; letter-spacing: 2px; }")
        .arg(QLatin1String(Style::kTextScale)).arg(Style::kFontCaption));
    row->addWidget(caption);

    m_entryCall = new QLineEdit(this);
    m_entryCall->setPlaceholderText(QStringLiteral("callsign"));
    m_entryCall->setStyleSheet(Style::lineEditStyle());
    // Gross wie bei N1MM (Entwurf C): das Rufzeichen ist die Zeile.
    m_entryCall->setFont(Style::monoFont(m_entryCall->font(), 22, QFont::Bold));
    m_entryCall->setFixedWidth(240);
    m_entryCall->setMinimumHeight(36);
    // Fest, nicht "Expanding" (Vorgabe von QLineEdit): sonst gibt das
    // FlowLayout dem Feld Platz, den es nicht nehmen kann -- eine Luecke.
    m_entryCall->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_entryCall->setToolTip(QStringLiteral(
        "The station you are working. Filters the log below as you type, "
        "so you see at once whether and when you worked them before."));
    row->addWidget(m_entryCall);

    // ── ROTOR (2026-11-10) ───────────────────────────────────────────
    //
    // Betreiber: „wenn man im logbuch das call eingibt und auf ROTOR
    // klickt, sollte man sehen, wohin der rotor zeigt und wo das call
    // steht. mit rotate sollte sich dann der rotor auch dorthin drehen."
    // ROTOR oeffnet die Karte mit dem Rotor-Radar (Keil = Stellung,
    // Marke = Station) und zeigt daneben Peilung, Entfernung und woher
    // die Lage stammt; Rotate / LP drehen. Der erlaubte Bereich
    // (RotorBereich) gilt -- ein Weg hinaus wird gesagt, nicht versucht.
    m_entryRotorBtn = new QPushButton(QStringLiteral("Rotor"), this);
    m_entryRotorBtn->setAutoDefault(false);
    m_entryRotorBtn->setStyleSheet(Style::buttonBaseStyle());
    m_entryRotorBtn->setToolTip(QStringLiteral(
        "Show where the rotor points and where this station is, "
        "then turn to it"));
    row->addWidget(m_entryRotorBtn);

    // ── Die Rotorzeile: eine eigene Zeile UNTER der Eingabe (2026-11-11) ──
    //
    // Betreiber: „weiters sollte alles sehr übersichtlich gestaltet
    // werden" -- gewaehlt Entwurf C (Blatt C-band.png). Die QSO-Zeile
    // bleibt, wie sie ohne Rotor war; darunter, nur nach ROTOR, feste
    // Plaetze: Band (Nadel = Rotor, Dreieck = Station), Peilung gross,
    // Entfernung, Herkunft, darunter der Zustand, dann Rotate / LP.
    m_rotorZeileW = new QWidget(this);
    m_rotorZeileW->setVisible(false);
    auto* rz = new QHBoxLayout(m_rotorZeileW);
    rz->setContentsMargins(0, 0, 0, 0);
    rz->setSpacing(6);
    auto* rotorKopf = new QLabel(QStringLiteral("ROTOR"), m_rotorZeileW);
    rotorKopf->setStyleSheet(caption->styleSheet());
    // So breit wie „NEW QSO": das Band steht genau unter dem Rufzeichen.
    rotorKopf->setFixedWidth(caption->sizeHint().width());
    rz->addWidget(rotorKopf);
    m_rotorBand = new RotorBandWidget(m_rotorZeileW);
    rz->addWidget(m_rotorBand);
    rz->addSpacing(4);
    auto* texte = new QVBoxLayout;
    texte->setSpacing(1);
    auto* oben = new QHBoxLayout;
    oben->setSpacing(10);
    m_entryGrad = new QLabel(m_rotorZeileW);
    m_entryGrad->setFont(Style::monoFont(m_entryGrad->font(), 14, QFont::Bold));
    m_entryGrad->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                   .arg(QLatin1String(Style::kTextPrimary)));
    oben->addWidget(m_entryGrad);
    m_entryKm = new QLabel(m_rotorZeileW);
    m_entryKm->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                                 .arg(QLatin1String(Style::kTextSecondary)));
    oben->addWidget(m_entryKm);
    m_entryPeilung = new QLabel(m_rotorZeileW);
    oben->addWidget(m_entryPeilung);
    oben->addStretch(1);
    texte->addLayout(oben);
    m_entryDreh = new QLabel(m_rotorZeileW);
    texte->addWidget(m_entryDreh);
    rz->addLayout(texte);
    rz->addSpacing(4);
    m_entryRotate = new QPushButton(QStringLiteral("Rotate"), m_rotorZeileW);
    m_entryRotate->setAutoDefault(false);
    m_entryRotate->setStyleSheet(Style::buttonBaseStyle());
    rz->addWidget(m_entryRotate);
    m_entryLp = new QPushButton(QStringLiteral("LP"), m_rotorZeileW);
    m_entryLp->setAutoDefault(false);
    m_entryLp->setStyleSheet(Style::buttonBaseStyle());
    rz->addWidget(m_entryLp);
    rz->addStretch(1);
    m_stillstandMs = RundumLauf::kStillstandMs;
    m_drehWache = new QTimer(this);
    m_drehWache->setInterval(1000);
    connect(m_drehWache, &QTimer::timeout, this, &LogbookWindow::aktualisiereRotorZeile);

    m_entryLive = new QLabel(QStringLiteral("●"), this);
    row->addWidget(m_entryLive);
    m_entryFreq = new QLabel(QStringLiteral("—"), this);
    m_entryFreq->setFont(Style::monoFont(m_entryFreq->font(), 14));
    m_entryFreq->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                   .arg(QLatin1String(Style::kTextPrimary)));
    m_entryFreq->setToolTip(QStringLiteral(
        "Frequency and mode are taken from the radio when you log"));
    row->addWidget(m_entryFreq);
    m_entryMode = new QLabel(this);
    m_entryMode->setStyleSheet(chipStyle());
    row->addWidget(m_entryMode);
    m_entryBand = new QLabel(this);
    m_entryBand->setStyleSheet(chipStyle());
    row->addWidget(m_entryBand);
    m_entrySource = new QLabel(QStringLiteral("from radio"), this);
    m_entrySource->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                                     .arg(QLatin1String(Style::kTextScale)));
    row->addWidget(m_entrySource);

    auto small = [this](const QString& text) {
        auto* l = new QLabel(text, this);
        l->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                             .arg(QLatin1String(Style::kTextScale)));
        return l;
    };
    row->addWidget(small(QStringLiteral("RST S")));
    m_entryRstS = new QLineEdit(QStringLiteral("59"), this);
    m_entryRstS->setStyleSheet(Style::lineEditStyle());
    m_entryRstS->setFixedWidth(52);
    m_entryRstS->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    row->addWidget(m_entryRstS);
    row->addWidget(small(QStringLiteral("R")));
    m_entryRstR = new QLineEdit(QStringLiteral("59"), this);
    m_entryRstR->setStyleSheet(Style::lineEditStyle());
    m_entryRstR->setFixedWidth(52);
    m_entryRstR->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    row->addWidget(m_entryRstR);

    m_entryComment = new QLineEdit(this);
    m_entryComment->setPlaceholderText(QStringLiteral("comment"));
    m_entryComment->setStyleSheet(Style::lineEditStyle());
    m_entryComment->setMinimumWidth(180);
    m_entryComment->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    row->addWidget(m_entryComment);

    m_entryLogBtn = new QPushButton(QStringLiteral("Log QSO"), this);
    m_entryLogBtn->setAutoDefault(false);
    // Die Hauptsache der Zeile: in der Auswahlfarbe, wie ein gedrueckter
    // Schalter -- dieselbe Flaeche wie "Map"/"Stats" im gedrueckten Zustand.
    m_entryLogBtn->setStyleSheet(Style::buttonBaseStyle()
        + QString(Style::blueCheckedStyle()).replace(
              QStringLiteral("QPushButton:checked"), QStringLiteral("QPushButton")));
    row->addWidget(m_entryLogBtn);

    m_entryHint = new QLabel(QStringLiteral("Enter logs · Esc clears"), this);
    m_entryHint->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                                   .arg(QLatin1String(Style::kTextScale)));
    row->addWidget(m_entryHint);

    col->addLayout(row);
    col->addWidget(m_rotorZeileW);

    // Bedarfs-Matrix (2026-11-08): sobald ein Rufzeichen dasteht, was das
    // Land auf jedem Band und in jeder Betriebsart schon hat -- und ob der
    // Platz, auf dem das Funkgeraet gerade steht, neu ist. Entprellt wie
    // die Karteikarte, aber kuerzer: es ist eine Rechnung, keine Anfrage.
    m_bedarf = new BedarfsMatrixWidget(this);
    m_bedarf->setVisible(false); if (m_bedarfLeer) { m_bedarfLeer->setVisible(true); }
    col->addWidget(m_bedarf);
    m_bedarfTimer = new QTimer(this);
    m_bedarfTimer->setSingleShot(true);
    m_bedarfTimer->setInterval(250);
    connect(m_bedarfTimer, &QTimer::timeout, this, &LogbookWindow::aktualisiereBedarf);

    for (QLineEdit* e : {m_entryCall, m_entryRstS, m_entryRstR, m_entryComment}) {
        e->installEventFilter(this);
    }
    connect(m_entryLogBtn, &QPushButton::clicked, this, &LogbookWindow::requestLog);
    connect(m_entryRotorBtn, &QPushButton::clicked, this, &LogbookWindow::zeigeRotorZeile);
    // Gedreht wird nur zu dem Rufzeichen, fuer das ROTOR gedrueckt wurde
    // -- und nur, solange die Karte genau diese Station zeigt.
    const auto peilungFuerRotor = [this](double& sp) {
        double km = 0.0;
        return m_rotorZeile && m_detail
            && m_detail->gezeigtesRufzeichen() == m_rotorRuf
            && m_detail->peilung(sp, km);
    };
    connect(m_entryRotate, &QPushButton::clicked, this, [this, peilungFuerRotor]() {
        double sp = 0.0;
        if (!peilungFuerRotor(sp)) { return; }
        drehe(sp);
    });
    connect(m_entryLp, &QPushButton::clicked, this, [this, peilungFuerRotor]() {
        double sp = 0.0;
        if (!peilungFuerRotor(sp)) { return; }
        drehe(BeamHeading::longPath(sp));
    });
    // Leeres oder anderes Rufzeichen: die Rotorzeile geht weg (2026-11-11).
    // textChanged, nicht textEdited -- auch ein Wert, der per
    // Bedienungshilfe oder aus dem Programm gesetzt wird, darf kein
    // „Rotate" zu einem alten Ziel stehen lassen.
    connect(m_entryCall, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (!m_rotorZeile) { return; }
        if (Callsigns::normalized(text) == m_rotorRuf) { return; }
        m_rotorZeile = false;
        m_rotorRuf.clear();
        drehZeileLeeren();
        aktualisiereRotorZeile();
    });

    // Tippen filtert das Log (Doppelt-Blick) und zeigt die Station nach
    // einer kurzen Pause in der Karteikarte -- entprellt wie im
    // Rotor/Log-Feld, damit OE, OE5, OE5V nicht drei Anfragen werden.
    m_entryLookup = new QTimer(this);
    m_entryLookup->setSingleShot(true);
    m_entryLookup->setInterval(700);
    connect(m_entryLookup, &QTimer::timeout, this, [this]() {
        const QString call = Callsigns::normalized(m_entryCall->text());
        if (call.size() >= 3 && Callsigns::isLikelyCallsign(call)) { showStationFor(call); }
    });
    connect(m_entryCall, &QLineEdit::textEdited, this, [this](const QString& text) {
        const QString up = text.toUpper();
        if (up != text) {
            const int pos = m_entryCall->cursorPosition();
            m_entryCall->setText(up);
            m_entryCall->setCursorPosition(pos);
        }
        if (m_search) { m_search->setText(Callsigns::normalized(up)); }
        m_entryLookup->start();
        m_bedarfTimer->start();
        m_entryHint->setText(QStringLiteral("Enter logs · Esc clears"));
        m_entryHint->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                                       .arg(QLatin1String(Style::kTextScale)));
    });

    refreshRadioReadout();
}

bool LogbookWindow::eventFilter(QObject* watched, QEvent* event)
{
    const bool entryField = watched == m_entryCall || watched == m_entryRstS
                         || watched == m_entryRstR || watched == m_entryComment;
    // Erst fragt Qt, ob ein Tastenkuerzel die Taste will. Das Hauptfenster
    // hat eines auf Esc (Vollbild verlassen) -- im echten Programm kam Esc
    // deshalb nie im Feld an (Live-Test 2026-09-26; QTest schickt die
    // Taste direkt und sah es nicht). Die Felder melden Esc und Enter als
    // ihre eigenen, dann geht die Taste als KeyPress hierher.
    if (entryField && event->type() == QEvent::ShortcutOverride) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape || ke->key() == Qt::Key_Return
            || ke->key() == Qt::Key_Enter) {
            event->accept();
            return true;
        }
    }
    if (event->type() == QEvent::KeyPress && entryField) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            requestLog();
            return true;        // nicht an den Dialog: kein Standardknopf
        }
        if (ke->key() == Qt::Key_Escape) {
            clearEntry();
            return true;        // nicht an den Dialog: Esc schliesst sonst das Logbuch
        }
    }
    return QDialog::eventFilter(watched, event);
}

void LogbookWindow::setRadio(RadioModel* radio)
{
    if (m_radio) { disconnect(m_radio, nullptr, this, nullptr); }
    m_radio = radio;
    if (m_radio) {
        connect(m_radio, &RadioModel::activeSliceChanged,
                this, [this](int) { rewireSlice(); });
        connect(m_radio, &RadioModel::connectionStateChanged,
                this, [this](ConnectionState) { refreshRadioReadout(); });
    }
    rewireSlice();
}

void LogbookWindow::rewireSlice()
{
    disconnect(m_sliceFreqConn);
    disconnect(m_sliceModeConn);
    if (SliceModel* s = m_radio ? m_radio->activeSlice() : nullptr) {
        m_sliceFreqConn = connect(s, &SliceModel::frequencyChanged,
                                  this, [this](double) { refreshRadioReadout(); });
        m_sliceModeConn = connect(s, &SliceModel::dspModeChanged,
                                  this, [this](DSPMode) { refreshRadioReadout(); });
    }
    refreshRadioReadout();
}

void LogbookWindow::refreshRadioReadout()
{
    if (!m_entryFreq) { return; }
    SliceModel* s = m_radio ? m_radio->activeSlice() : nullptr;
    const bool live = s && m_radio->connectionState() == ConnectionState::Connected;
    m_entryLive->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
        .arg(QString::fromLatin1(live ? Style::kLiveGreen : Style::kTextScale)));
    if (!s) {
        m_entryFreq->setText(QStringLiteral("—"));
        m_entryMode->setVisible(false);
        m_entryBand->setVisible(false);
        // Ein Funkgeraet gibt es, nur noch keine Verbindung (und damit
        // keinen Empfaenger) -- "no radio" waere falsch.
        m_entrySource->setText(m_radio ? QStringLiteral("radio not connected")
                                       : QStringLiteral("no radio"));
        return;
    }
    // Auf der Antenne, nicht am Geraet (2026-11-08): am Transverter stand
    // hier 28.300.000 und "10m" fuer ein QSO auf 144,300.
    const double antenne = static_cast<double>(TransverterSetup::displayHz(
        static_cast<qint64>(s->frequency())));
    m_entryFreq->setText(dottedHz(antenne));
    m_entryMode->setText(SliceModel::modeName(s->dspMode()));
    m_entryMode->setVisible(true);
    const QString band = bandLabel(bandFromFrequency(antenne));
    m_entryBand->setText(band);
    m_entryBand->setVisible(!band.isEmpty());
    m_entrySource->setText(live ? QStringLiteral("from radio")
                                : QStringLiteral("radio not connected"));
    if (m_bedarf && m_bedarf->isVisible()) { aktualisiereBedarf(); }
}

void LogbookWindow::setCtyDat(const CtyDatParser* cty)
{
    m_cty = cty;
    if (m_detail) { m_detail->setCty(cty); }
}

QString LogbookWindow::meinFeld() const
{
    for (const LogEntry& e : m_all) {                 // m_all: neueste zuerst
        if (!e.mySotaRef.trimmed().isEmpty()) { continue; }   // Gipfel, nicht zu Hause
        if (isValidGridSquare(e.myGridSquare.trimmed())) { return e.myGridSquare.trimmed(); }
    }
    return AppSettings::instance().value(QStringLiteral("StationGridSquare")).toString().trimmed();
}

void LogbookWindow::aktualisiereBedarf()
{
    if (!m_bedarf || !m_entryCall) { return; }
    const QString call = Callsigns::normalized(m_entryCall->text());
    if (call.size() < 3 || !Callsigns::isLikelyCallsign(call) || !m_cty) {
        m_bedarf->setVisible(false); if (m_bedarfLeer) { m_bedarfLeer->setVisible(true); }
        return;
    }
    const bool funk = m_entryMode && m_entryMode->isVisible();
    const QString band = (funk && m_entryBand->isVisible()) ? m_entryBand->text() : QString();
    const QString mode = funk ? m_entryMode->text() : QString();
    m_bedarf->setBedarf(LogbuchBilder::bedarf(m_all, call, band, mode, m_cty, meinFeld()));
    m_bedarf->setVisible(true);
    if (m_bedarfLeer) { m_bedarfLeer->setVisible(false); }
}

void LogbookWindow::requestLog()
{
    LogEntry e;
    e.call    = Callsigns::normalized(m_entryCall->text());
    if (e.call.isEmpty()) {
        reportLogged(false, QStringLiteral("Enter a callsign to log"));
        m_entryCall->setFocus();
        return;
    }
    e.rstSent = m_entryRstS->text().trimmed();
    e.rstRcvd = m_entryRstR->text().trimmed();
    e.comment = m_entryComment->text().trimmed();
    // Was QRZ zu genau diesem Rufzeichen weiss -- nicht das Blatt einer
    // anderen Station, die gerade in der Karteikarte steht.
    if (m_callCache.contains(e.call)) {
        const CallsignInfo info = m_callCache.get(e.call);
        e.name    = info.displayName();
        e.qth     = info.city;
        e.country = info.country;
        if (isValidGridSquare(info.grid.trimmed())) {
            e.gridSquare = info.grid.trimmed().toUpper();
        } else if (info.hasLatLon) {
            e.gridSquare = gridSquareFromLatLon(info.latitude, info.longitude);
        }
    }
    emit logQsoRequested(e);
}

void LogbookWindow::reportLogged(bool ok, const QString& message)
{
    m_entryHint->setText(message);
    m_entryHint->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
        .arg(QString::fromLatin1(ok ? Style::kGreenText : Style::kAmberWarn)));
    if (!ok) { return; }
    const QString call = Callsigns::normalized(m_entryCall->text());
    // Rufzeichen und Kommentar gehen, die Rapporte bleiben: die naechste
    // Station wird meist mit denselben gearbeitet (wie im Rotor/Log-Feld).
    m_entryCall->clear();
    m_entryComment->clear();
    if (m_search) { m_search->clear(); }
    reload();
    emit logChanged();
    m_entryCall->setFocus();
    Q_UNUSED(call);
}

void LogbookWindow::zeigeRotorZeile()
{
    const QString call = Callsigns::normalized(m_entryCall->text());
    if (call.size() < 3 || !Callsigns::isLikelyCallsign(call)) {
        m_entryCall->setFocus();
        return;
    }
    m_rotorZeile = true;
    m_rotorRuf = call;
    // Nicht auf die Entprellung warten: wer ROTOR drueckt, will es jetzt.
    m_entryLookup->stop();
    if (!m_detail || m_detail->gezeigtesRufzeichen() != call) { showStationFor(call); }
    setMapPanelShown(true);
    aktualisiereRotorZeile();
}

void LogbookWindow::aktualisiereRotorZeile()
{
    if (!m_rotorZeileW) { return; }
    m_rotorZeileW->setVisible(m_rotorZeile);
    if (!m_rotorZeile) { return; }

    const QString call = m_rotorRuf;
    double sp = 0.0, km = 0.0;
    const bool hat = m_detail && m_detail->gezeigtesRufzeichen() == call
                  && m_detail->peilung(sp, km);
    m_rotorBand->setStation(hat ? sp : -1.0);
    aktualisiereDrehZeile();
    if (!hat) {
        m_entryGrad->setText(QStringLiteral("—"));
        m_entryKm->clear();
        m_entryPeilung->setText(QStringLiteral("%1 — no position yet").arg(call));
        m_entryPeilung->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                                          .arg(QLatin1String(Style::kTextScale)));
        m_entryRotate->setText(QStringLiteral("Rotate"));
        m_entryLp->setText(QStringLiteral("LP"));
        m_entryRotate->setEnabled(false);
        m_entryLp->setEnabled(false);
        return;
    }
    const double lp = BeamHeading::longPath(sp);
    m_entryGrad->setText(QStringLiteral("%1°").arg(sp, 0, 'f', 0));
    m_entryKm->setText(QStringLiteral("%1 km").arg(km, 0, 'f', 0));
    m_entryPeilung->setText(m_detail->lageText());
    m_entryPeilung->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }").arg(
        QLatin1String(m_detail->lageGrob() ? Style::kAmberWarn : Style::kTextScale)));

    // Der erlaubte Bereich gilt fuer jeden Drehbefehl (RotorLogbookPanel
    // verweigert ihn dort) -- hier wird es gesagt, bevor jemand drueckt:
    // auf dem Knopf selbst, weil ein gesperrter Knopf im Hausstil sonst
    // aussieht wie ein freier.
    const RotorBereich bereich = RotorBereich::load();
    const auto geht = [&](double ziel) {
        return bereich.erlaubt(ziel)
            && (m_rotorDeg < 0.0 || bereich.wegErlaubt(m_rotorDeg, ziel));
    };
    const bool spGeht = geht(sp), lpGeht = geht(lp);
    m_entryRotate->setEnabled(spGeht);
    m_entryLp->setEnabled(lpGeht);
    const QString gesperrt = QStringLiteral(" — blocked");
    m_entryRotate->setText(QStringLiteral("Rotate %1°").arg(sp, 0, 'f', 0)
                           + (spGeht ? QString() : gesperrt));
    m_entryLp->setText(QStringLiteral("LP %1°").arg(lp, 0, 'f', 0)
                       + (lpGeht ? QString() : gesperrt));
    const QString draussen = QStringLiteral("Outside the allowed rotation range (%1)")
                                 .arg(bereich.beschreibung());
    m_entryRotate->setToolTip(spGeht ? QStringLiteral("Turn the rotor to the short path")
                                     : draussen);
    m_entryLp->setToolTip(lpGeht ? QStringLiteral(
        "Turn the rotor the other way round the world. On the low bands and "
        "at grey line this is often the stronger signal.") : draussen);
}

void LogbookWindow::drehe(double ziel)
{
    m_drehZiel = BeamHeading::wrap360(ziel);
    m_drehMeldung.clear();
    m_drehUhr.restart();
    // Synchron: lehnt der Rotor ab, kommt setRotorMeldung() noch in
    // diesem Aufruf an -- und steht dann gleich in der Zeile.
    emit turnRotorRequested(m_drehZiel, m_rotorRuf);
    m_drehWache->start();
    aktualisiereRotorZeile();
}

void LogbookWindow::drehZeileLeeren()
{
    m_drehZiel = -1.0;
    m_drehMeldung.clear();
    if (m_drehWache) { m_drehWache->stop(); }
}

void LogbookWindow::setRotorMeldung(const QString& text)
{
    // Nur zu einem Drehbefehl aus der Eingabezeile; was der Rotor sonst
    // meldet, steht im Rotorfenster.
    if (m_drehZiel < 0.0) { return; }
    m_drehMeldung = text.trimmed();
    aktualisiereRotorZeile();
}

void LogbookWindow::aktualisiereDrehZeile()
{
    // ── Dreht er wirklich? ── 2026-11-11 ─────────────────────────────
    //
    // Gezeigt wird nur, was der Rotor selbst meldet (setRotorBearing --
    // echte Ablesungen, nie die Stellvertreter-Nadel). Am Ziel heisst
    // innerhalb RundumLauf::kAngekommenGrad, „steht" heisst so lange
    // ohne neue Position wie dort (kStillstandMs) -- Rotorfenster,
    // Rundumlauf und Logbuch urteilen gleich. Farben wie am Zifferblatt:
    // in Bewegung Bernstein, am Ziel Gruen.
    const bool rotorDa = m_rotorDeg >= 0.0;
    const auto grad = [](double d) { return QString::number(qRound(BeamHeading::wrap360(d)) % 360); };
    QString text;
    const char* farbe = Style::kTextScale;
    bool wachen = false;
    if (m_drehZiel < 0.0) {
        text = rotorDa ? QStringLiteral("rotor at %1°").arg(grad(m_rotorDeg))
                       : QStringLiteral("rotor not connected");
    } else if (!m_drehMeldung.isEmpty()) {
        text = QStringLiteral("rotor: %1").arg(m_drehMeldung);
        farbe = Style::kAmberWarn;
    } else if (!rotorDa) {
        text = QStringLiteral("no rotor connected — the antenna does not turn");
        farbe = Style::kAmberWarn;
    } else {
        double weg = std::fmod(std::abs(m_rotorDeg - m_drehZiel), 360.0);
        if (weg > 180.0) { weg = 360.0 - weg; }
        if (weg <= RundumLauf::kAngekommenGrad) {
            text = QStringLiteral("rotor at %1° — on target").arg(grad(m_rotorDeg));
            farbe = Style::kGreenText;
        } else if (m_drehUhr.isValid() && m_drehUhr.elapsed() > m_stillstandMs) {
            text = QStringLiteral("rotor not moving — at %1°, target %2°")
                       .arg(grad(m_rotorDeg), grad(m_drehZiel));
            farbe = Style::kAmberWarn;
        } else {
            text = QStringLiteral("turning to %1°  ·  now %2°")
                       .arg(grad(m_drehZiel), grad(m_rotorDeg));
            farbe = Style::kAmberText;
            wachen = true;
        }
    }
    m_entryDreh->setText(text);
    m_entryDreh->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                                   .arg(QLatin1String(farbe)));
    // Das Band: Nadel nur aus echten Ablesungen; gruen am Ziel, sonst in
    // der Farbe der Ablesung am Zifferblatt (Bernstein).
    m_rotorBand->setStand(rotorDa ? m_rotorDeg : -1.0);
    m_rotorBand->setZiel(m_drehZiel >= 0.0);
    m_rotorBand->setFarbe(farbe == Style::kGreenText ? Style::kGreenText : Style::kAmberText);
    if (!wachen && m_drehWache) { m_drehWache->stop(); }
}

void LogbookWindow::clearEntry()
{
    m_rotorZeile = false;
    m_rotorRuf.clear();
    drehZeileLeeren();
    aktualisiereRotorZeile();
    m_entryLookup->stop();
    if (m_bedarfTimer) { m_bedarfTimer->stop(); }
    if (m_bedarf) { m_bedarf->setVisible(false); if (m_bedarfLeer) { m_bedarfLeer->setVisible(true); } }
    m_entryCall->clear();
    m_entryComment->clear();
    if (m_search) { m_search->clear(); }
    m_entryCall->setFocus();
}

void LogbookWindow::uebernimmRufzeichen(const QString& rufzeichen)
{
    // Betreiber 2026-11-12: „ein doppelklick im panadapter sollte auch
    // im logbuch sofort das call übernehmen und alle daten anzeigen."
    // Bis heute ging der Doppelklick nur ins Rotor/Log-Feld; dieses
    // Fenster erfuhr nichts davon. Derselbe Weg wie das Tippen (siehe
    // textEdited der Eingabezeile), aber ohne die 700 ms Entprellung:
    // ein Doppelklick ist kein halbes Rufzeichen.
    const QString up = rufzeichen.trimmed().toUpper();
    if (up.isEmpty() || !m_entryCall) { return; }
    m_entryCall->setText(up);
    if (m_search) { m_search->setText(Callsigns::normalized(up)); }
    if (m_entryLookup) { m_entryLookup->stop(); }
    if (m_entryHint) {
        m_entryHint->setText(QStringLiteral("Enter logs · Esc clears"));
        m_entryHint->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                                       .arg(QLatin1String(Style::kTextScale)));
    }
    showStationFor(up);
    if (m_bedarfTimer) { m_bedarfTimer->stop(); }
    aktualisiereBedarf();
}

void LogbookWindow::showStationFor(const QString& raw)
{
    const QString call = Callsigns::normalized(raw);
    if (call.isEmpty()) { return; }

    for (int row = 0; row < m_visible.size(); ++row) {
        const int idx = sourceRow(row);
        if (idx < 0) { continue; }
        if (Callsigns::normalized(m_all.at(idx).call) != call) {
            continue;
        }
        m_table->setCurrentCell(row, ColCall);
        m_detail->setEntry(m_all.at(idx));
        m_detail->lookUpNow();
        return;
    }

    if (!Callsigns::isLikelyCallsign(call)) { return; }
    m_detail->showCallsign(call);
}


// ── Rotor im Logbuch (2026-09-26) ───────────────────────────────────────

void LogbookWindow::setRotorBearing(double deg)
{
    const double vorher = m_rotorDeg;
    m_rotorDeg = std::isnan(deg) ? -1.0 : deg;
    // Eine neue Position heisst: er bewegt sich (Stillstandswache).
    if (m_rotorDeg >= 0.0 && std::abs(m_rotorDeg - vorher) > 0.05) { m_drehUhr.restart(); }
    if (m_detail) { m_detail->setRotorBearing(deg); }
    applyRotorToMap();
    aktualisiereRotorZeile();
}

void LogbookWindow::setRotorBeamWidth(double deg)
{
    m_rotorBeamDeg = deg;
    applyRotorToMap();
}

void LogbookWindow::applyRotorToMap()
{
    if (!m_mapPanel) { return; }
    m_mapPanel->setRotorBeamWidth(m_rotorBeamDeg);
    m_mapPanel->setRotorHeading(m_rotorDeg);
    m_mapPanel->setRotorTarget(m_rotorTargetDeg);
}

// ── MY_SOTA_REF aus dem SOTAdata-Log (2026-11-11) ───────────────────

void LogbookWindow::sotaNachtragen()
{
    const QString pfad = QFileDialog::getOpenFileName(
        this, QStringLiteral("SOTA activator log"), QString{},
        QStringLiteral("CSV (*.csv *.txt);;All files (*)"));
    if (pfad.isEmpty()) { return; }

    // Der Dialog arbeitet auf einer Kopie. Waehrend er offen ist, kann ein
    // QSO aus WSJT-X das Log neu einlesen -- die Vorschlaege werden danach
    // am ganzen Datensatz wiedergefunden, nicht am alten Index.
    const QVector<LogEntry> stand = m_all;
    ScopedChildWidget<SotaNachtragDialog> besitzer(stand, this);
    SotaNachtragDialog& dlg = *besitzer.get();
    dlg.ladeCsv(pfad);
    if (dlg.exec() != QDialog::Accepted) { return; }

    QVector<SotaNachtrag::Vorschlag> gewaehlt = dlg.gewaehlt();
    QSet<int> vergeben;
    for (SotaNachtrag::Vorschlag& v : gewaehlt) {
        v.index = indexOfRecord(m_all, stand.at(v.index).toAdifRecord(), v.index, vergeben);
        if (v.index >= 0) { vergeben.insert(v.index); }
    }
    gewaehlt.erase(std::remove_if(gewaehlt.begin(), gewaehlt.end(),
                                  [](const SotaNachtrag::Vorschlag& v) { return v.index < 0; }),
                   gewaehlt.end());

    QString sicherung;
    const int vorher = gewaehlt.size();
    if (sotaVorschlaegeAnwenden(gewaehlt, &sicherung)) {
        tellOperator(QStringLiteral("Summit references added to %1 contact%2. "
                                    "The log before this change: %3")
                         .arg(vorher).arg(vorher == 1 ? QString() : QStringLiteral("s"),
                              QDir::toNativeSeparators(sicherung)));
    }
}

bool LogbookWindow::sotaVorschlaegeAnwenden(const QVector<SotaNachtrag::Vorschlag>& gewaehlt,
                                            QString* sicherung)
{
    if (gewaehlt.isEmpty()) { return false; }
    QVector<LogEntry> neu = m_all;
    if (SotaNachtrag::anwenden(neu, gewaehlt) == 0) { return false; }
    // Bei SOTA gilt der Locator des Gipfels (Betreiber 2026-11-10); der
    // Dialog zeigt die Aenderung in der Spalte "Your grid".
    for (const SotaNachtrag::Vorschlag& v : gewaehlt) {
        if (v.index >= 0 && v.index < neu.size()) {
            SotaGipfelVerzeichnis::instanz().wendeGipfelLocatorAn(neu[v.index]);
        }
    }

    // Erst sichern, dann schreiben -- und ohne Sicherung gar nicht.
    QString ziel;
    if (QFile::exists(m_path)) {
        ziel = m_path + QStringLiteral(".before-sota-")
               + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss"));
        if (!QFile::copy(m_path, ziel)) {
            QMessageBox::critical(this, QStringLiteral("Logbook"),
                QStringLiteral("Couldn't back up the log to\n%1\n\nNothing was changed.")
                    .arg(QDir::toNativeSeparators(ziel)));
            return false;
        }
    }
    const QVector<LogEntry> alt = m_all;
    m_all = neu;
    if (!saveAll()) {
        m_all = alt;
        return false;
    }
    if (sicherung) { *sicherung = ziel; }
    reload();
    return true;
}

} // namespace Longpath
