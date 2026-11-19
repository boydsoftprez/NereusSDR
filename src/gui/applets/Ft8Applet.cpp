// =================================================================
// src/gui/applets/Ft8Applet.cpp  (Longpath)
// =================================================================
// Longpath-original. Siehe Ft8Applet.h.
// =================================================================
// Modification history (Longpath):
//   2026-10-15 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
//   2026-11-17 — Mehrere Plaetze: siehe den Eintrag in Ft8Applet.h.
//                 Martin Fischer (OE5SOS), KI-gestuetzt ueber Anthropic
//                 Claude.
// =================================================================

#include "Ft8Applet.h"
#include "core/TransverterSetup.h"

#include "core/AudioEngine.h"
#include "core/Ft8Stream.h"
#include "core/Ft8Ablauf.h"
#include "core/Ft8Tx.h"
#include "core/AdifLog.h"
#include "core/LogbookDatei.h"
#include "gui/widgets/Ft8Fensterbalken.h"
#include "models/Band.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QDateTime>
#include "core/AppSettings.h"
#include <QLoggingCategory>
#include "core/DigiFrequenzen.h"
#include "core/DxccColorProvider.h"
#include "core/DxccWorkedStatus.h"
#include "gui/widgets/Ft8Wasserfall.h"
#include "gui/widgets/Ft8DannListe.h"
#include "gui/widgets/Ft8PartnerZeile.h"
#include "gui/widgets/FlowLayout.h"
#include "core/CtyDatParser.h"
#include "core/Grauzone.h"
#include "core/Maidenhead.h"
#include "core/MoxController.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "core/audio/AudioTapRing.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"
#include "core/DxSpot.h"
#include "models/RxDecodeModel.h"
#include "models/SliceModel.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QSpinBox>
#include <QColor>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QMouseEvent>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

// Dieselbe Kategorie wie der Dekoderstrom („longpath.ft8"), eigener
// Zeiger: Ft8Stream.cpp meldet sie nicht ueber einen Kopf an, und
// zwei Objekte mit demselben Namen sind in Qt zulaessig.
//
// Warum es sie hier ueberhaupt braucht: der Sendeweg schrieb bis zum
// 2026-10-28 KEINE einzige Zeile. Er setzte nur eine Statuszeile, und
// die wird vom Takt („last period: N") binnen 200 ms ueberschrieben.
// Der Betreiber sah einen Send-Knopf, der nichts tut, und im
// Protokoll stand nichts — eine Stunde Suche fuer einen Fall, den
// eine Zeile beantwortet haette.
Q_LOGGING_CATEGORY(lcFt8Senden, "longpath.ft8", QtInfoMsg)

namespace Longpath {

namespace {

// Der Ring muss ein FT8-Fenster ueberbruecken koennen, wenn der
// Taktgeber einmal spaet kommt: 48 kHz, Stereo, eine Sekunde. Das ist
// das Doppelte des CW-Rings, weil hier in 15-Sekunden-Fenstern
// gerechnet wird und ein verlorenes Stueck nicht ein Zeichen kostet,
// sondern das ganze Fenster.
constexpr int kTapRingFloats = 96000;

// Alle 200 ms leeren. Der Ring haelt eine Sekunde; fuenf Mal
// Sicherheit ist genug und kostet nichts.
constexpr int kPumpIntervalMs = 200;

// So viele Zeilen bleiben stehen. Ein FT8-Fenster bringt auf 20 m
// leicht zwanzig Funde — 400 Zeilen sind also etwa die letzten zehn
// Minuten.
constexpr int kMaxLines = 400;

// Die Tonlage, auf der gesendet wird: Vorgabe und Grenzen. 1500 Hz ist
// die Mitte des Durchlassbereichs und das, was WSJT-X ab Werk nimmt --
// seit 2026-10-18 nur noch die VORGABE, nicht mehr die einzige Wahl
// (siehe m_sendeTonHz in Ft8Applet.h). Die Grenzen sind der nutzbare
// Durchlassbereich eines gewoehnlichen SSB-Filters; darueber hinaus
// kaeme der Ton gar nicht erst heraus.
constexpr int kSendeTonVorgabeHz = 1500;
constexpr int kSendeTonMinHz = 200;
constexpr int kSendeTonMaxHz = 2800;
constexpr auto kSendeTonSchluessel = "Ft8/SendeTonHz";

// ── Die drei Zeiten einer Sendung ─────────────────────────
//
// Begruendung und Rechnung: `Ft8Tx::sendeplan`.

/// Wie lange nach der Fenstergrenze auf den Dekoder gewartet wird,
/// bevor der Zug festgelegt und getastet wird.
///
/// Am Geraet gemessen (2026-10-30): das Fenster endete um :30,000,
/// der Dekoder meldete um :30,039 — 39 ms mit zwei Runden
/// Subtraktion. 250 ms lassen Luft fuer mehr Runden und einen
/// beschaeftigten Rechner, ohne dass etwas davon verlorengeht: die
/// Frist wird aus der fuehrenden Stille der Wellenform genommen,
/// nicht hinten angehaengt.
constexpr qint64 kDekoderFristMs = 250;

/// Wo das Signal nach der Fenstergrenze beginnen soll.
///
/// WSJT-X setzt es auf eine halbe Sekunde; das ist der Wert, gegen
/// den die ganze Betriebsart eingestellt ist. Von vorn abgespielt
/// laege es bei 1180 ms (ft8_lib zentriert) — dekodierbar, aber
/// 0,68 s neben der Norm.
constexpr qint64 kZielVersatzMs = 500;

/// Nachlauf, damit der letzte Ton wirklich draussen ist, bevor der
/// Sender abschaltet.
constexpr qint64 kSenderNachlaufMs = 300;

// ── Welche Sicht gewaehlt ist (2026-11-19) ──────────────────────────
//
// Betreiber, 2026-11-17: „weiters sieht man immer noch nicht, ob man
// auf die pille geklickt hat oder nicht". Die gewaehlte Pille war nur
// eine Mulde -- eine Spur dunkler als die anderen, auf dem Schirm kaum
// zu unterscheiden. Vier Entwurfsblaetter (A Messingstrich, B heller
// Rahmen, C helle Pille, D Messingpunkt); gewaehlt am 2026-11-19: A.
//
// Die Mulde bleibt, dazu ein schmaler Strich in Messing unter dem Wort.
// Ein eigenes Kind statt einer Stilblattregel: das Blatt der Pille
// kommt aus blueToggle() und wird an anderen Stellen weiterverwendet;
// ein `border-bottom` haette dort den Rahmen veraendert, nicht einen
// Strich hineingesetzt. Fuer die Maus durchlaessig, damit der Klick
// weiter die Pille trifft.
class MessingStrich : public QWidget {
public:
    explicit MessingStrich(QAbstractButton* pille)
        : QWidget(pille), m_pille(pille)
    {
        setObjectName(QStringLiteral("ft8SichtMessingStrich"));
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        pille->installEventFilter(this);
        QObject::connect(pille, &QAbstractButton::toggled, this,
                         [this](bool an) { setVisible(an); });
        folge();
        setVisible(pille->isChecked());
    }

protected:
    bool eventFilter(QObject* o, QEvent* e) override
    {
        if (o == m_pille && e->type() == QEvent::Resize) { folge(); }
        return false;
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(QString::fromLatin1(Style::kTxKeyBorder)));
        p.drawRoundedRect(QRectF(rect()), 1.0, 1.0);
    }

private:
    // Wie im Blatt: 6 Punkte vom Rand, 2 hoch, 2 ueber der Unterkante.
    void folge()
    {
        constexpr int kEinzug = 6;
        constexpr int kHoehe = 2;
        constexpr int kAbstand = 2;
        const int b = std::max(0, m_pille->width() - 2 * kEinzug);
        setGeometry(kEinzug, m_pille->height() - kAbstand - kHoehe,
                    b, kHoehe);
    }

    QAbstractButton* m_pille;
};

} // namespace

Ft8Applet::Ft8Applet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();

    m_stream = new Ft8Stream(this);
    connect(m_stream, &Ft8Stream::decoded, this, &Ft8Applet::onDecoded);

    // ── Was vor dem Hinhoeren schon feststeht (2026-11-13) ─────────
    //
    // Der Strom holt sich die Vorgabe je Fenster hier ab, statt dass
    // sie ihm zugeschoben wird: nur dieses Fenster weiss, mit WEM
    // gerade ein QSO laeuft, und das wechselt mitten im Betrieb.
    //
    // Die Rangfolge ist Absicht und entscheidet mit: der erste
    // Treffer gewinnt, also steht die staerkste Gewissheit vorn.
    // Laeuft ein QSO, sind beide Rufzeichen bekannt -- das ist die
    // Vorgabe, die den Schluss rettet (RR73, 73). Sonst bleibt „wer
    // immer mich ruft".
    m_stream->setVorwissenQuelle([this]() {
        QVector<Longpath::ApVorgabe> v;
        const QString meiner = eigenesRufzeichen();
        if (meiner.isEmpty()) { return v; }

        const QString partner = m_ablauf.laeuft() ? m_ablauf.partner()
                                                  : QString();
        if (!partner.isEmpty()) {
            auto a = Longpath::baueApVorgabe(
                Longpath::ApTyp::MeinRufUndSeiner, meiner, partner);
            if (a.brauchbar) { v.push_back(a); }
        }
        auto b = Longpath::baueApVorgabe(Longpath::ApTyp::MeinRuf, meiner);
        if (b.brauchbar) { v.push_back(b); }
        return v;
    });

    if (m_model) {
        // Dieselbe Sicherung wie beim CW-Dekoder: eine fallengelassene
        // Scheibe darf keinen alten sliceIndex im Abgriff hinterlassen,
        // sonst laeuft der naechste Empfaenger, der diesen Index
        // bekommt, hier herein.
        // Gereiht, nicht direkt: das Signal kommt mitten aus
        // MoxController::setMox() (hardwareFlipped -> flexTasten), und
        // `sendenGescheitert` tastet selbst wieder aus. Was im
        // Augenblick des Tastens verweigert wird, faengt
        // `sendeNachricht` ohnehin an Ort und Stelle; hier kommt an,
        // was WAEHREND der Sendung bricht.
        connect(m_model, &RadioModel::flexNichtGetastet,
                this, &Ft8Applet::sendenGescheitert,
                Qt::QueuedConnection);
        connect(m_model, &RadioModel::sliceRemoved, this, [this](int index) {
            if (m_slice && m_slice->sliceIndex() == index) {
                setSlice(nullptr);
            }
        });
    }

    m_tapRing = std::make_unique<AudioTapRing>(kTapRingFloats);

    m_pumpTimer = new QTimer(this);
    m_pumpTimer->setInterval(kPumpIntervalMs);
    connect(m_pumpTimer, &QTimer::timeout, this, [this]() {
        if (!m_tapRing || !m_stream) { return; }
        static thread_local std::vector<float> scratch;
        const int want = kTapRingFloats / 2;   // eine halbe Sekunde Stereo
        if (static_cast<int>(scratch.size()) < want) { scratch.resize(want); }
        const int got = m_tapRing->read(scratch.data(), want);
        if (got >= 2) {
            m_stream->feedAudio(scratch.data(), got / 2);
            // Derselbe Ton in den Streifen -- ein Abgriff, zwei
            // Verbraucher. Kein zweiter Ring, nichts zu halten.
            if (m_wasserfall) {
                m_wasserfall->speiseStereo(scratch.data(), got / 2, 48000);
            }
        } else {
            // Kein Ton: die Fenstergrenze trotzdem pruefen, sonst
            // friert die Anzeige auf dem vorletzten Stand ein und
            // sieht aus wie ein abgestuerzter Dekoder.
            m_stream->poll();
        }
        updateStatus();
    });
    m_pumpTimer->start();

    updateStatus();
}

Ft8Applet::~Ft8Applet()
{
    // Reihenfolge wie beim CW-Dekoder: erst der Taktgeber, dann der
    // Abgriff. Ft8Stream haengt als Kind an diesem Widget.
    if (m_pumpTimer) { m_pumpTimer->stop(); }
    if (m_model && m_model->audioEngine()) {
        m_model->audioEngine()->setFt8Tap(nullptr, -1);
    }
}

// ── Die Fundliste gibt zuerst nach (2026-11-18) ─────────────────────
//
// Die drei Reihen brechen um; damit haengt die Hoehe des Fensters an
// seiner Breite. Der Rollbereich des schwebenden Fensters
// (`AppletFloatingWindow`) fragt dafuer `heightForWidth()` und macht das
// Applet nie niedriger -- und Qt antwortet mit der Summe der
// WUNSCHhoehen. Die Fundliste wuenscht sich 192 Punkt und kommt mit 120
// aus: gemessen bei 260 Punkt Breite 703 gegen 631. In diesen 72 Punkt
// rollte das Fenster senkrecht, und unten, unter dem Rand, lag die
// Sendereihe mit `Stop`.
//
// Im Rollbereich darum die MINDESThoehe: gerollt wird erst, wenn die
// Liste nichts mehr abgeben kann. Angedockt in der Spalte bleibt es bei
// Qts Antwort -- dort rollt die ganze Spalte, und die Liste soll ihre
// Wunschhoehe behalten.
//
// Der zweite Griff ist derselbe wie im Slots-Fenster: der senkrechte
// Rollbalken nimmt dem Inhalt zehn Punkt Breite, die Reihen brechen
// frueher um, das Fenster wird hoeher -- und braucht den Balken erst
// recht. Passt alles, WENN der Balken weg waere, wird fuer diese Breite
// geantwortet; der Rollbereich nimmt ihn darauf selbst weg.
int Ft8Applet::heightForWidth(int breite) const
{
    QLayout* anordnung = layout();
    QWidget* sicht = parentWidget();
    auto* roll = sicht ? qobject_cast<QScrollArea*>(sicht->parentWidget())
                       : nullptr;
    if (!anordnung || !roll || roll->viewport() != sicht) {
        return AppletWidget::heightForWidth(breite);
    }
    const QSize frei = roll->maximumViewportSize();
    if (breite < frei.width()) {
        const int ohneBalken =
            anordnung->totalMinimumHeightForWidth(frei.width());
        if (ohneBalken <= frei.height()) { return ohneBalken; }
    }
    return anordnung->totalMinimumHeightForWidth(breite);
}

void Ft8Applet::buildUI()
{
    auto* root = new QVBoxLayout(this);

    // ── Das ⚙ rechts oben in der Kopfzeile ───────────────────────────
    //
    // Betreiber 2026-10-21: „sollt in der taksleiste im decoder eine
    // optionsmöglichkeit geben". Vorausgegangen war die Frage, WO man
    // Rufzeichen und Locator eintraegt -- die Antwort war bis dahin
    // „irgendwo im Setup, oder im Spot Hub", und das ist keine.
    //
    // Die Stelle ist seine Regel vom 2026-09-20: Optionen immer rechts
    // oben im Panelkopf.
    QWidget* leiste = appletTitleBar(QStringLiteral("FT8 DECODER"));
    if (auto* kopf = qobject_cast<QHBoxLayout*>(leiste->layout())) {
        // Der Knopf traegt "...", nicht ⚙ (2026-10-21).
        //
        // Hergeleitet, nicht geraten: eine Probe mit magenta Hintergrund
        // zeigte, dass der Knopf GEZEICHNET wird -- 176 Bildpunkte an
        // der richtigen Stelle -- nur sein Zeichen nicht. Weder ⚙
        // (U+2699) noch ⋯ (U+22EF) kommen an; die Schrift dieser Taste
        // kennt sie nicht, obwohl das ⋮⋮ des Griffs daneben (ein QLabel,
        // keine Taste) anstandslos erscheint.
        //
        // "..." ist ausserdem die Form, die in der Kommandoleiste schon
        // fuer "mehr davon" steht (Band, Mode, Filter). Die Stelle --
        // rechts oben im Panelkopf -- ist die Regel des Betreibers, das
        // Zeichen war es nie.
        auto* zahnrad = new QPushButton(QStringLiteral("..."), leiste);
        // 16x14 wie der geteilte Optionsknopf in GridCellWidget: die
        // Kopfzeile ist kTitleBarH = 16 Punkte hoch, ein 18 Punkte
        // hoher Knopf passt NICHT hinein und wurde am 2026-10-21
        // schlicht nicht gezeichnet.
        zahnrad->setFixedSize(26, 15);
        zahnrad->setCursor(Qt::PointingHandCursor);
        zahnrad->setToolTip(tr("Callsign and grid square"));
        zahnrad->setStyleSheet(QStringLiteral(
            // Ruhend in der Schrift der Kopfzeile, nicht in der des
            // Griffs: mit kTextScale (#7e7e85) auf diesem Grund war der
            // Knopf im Entwurfsblatt schlicht NICHT zu sehen -- er war
            // da, an der richtigen Stelle, sichtbar laut Qt, und doch
            // unsichtbar. Beim Zeigen das Blau des Hauses: anfassbar.
            "QPushButton { color: %1; background: transparent;"
            " border: none; font-size: 13px; font-weight: bold; }"
            "QPushButton:hover { color: %2; }")
            .arg(Style::kTitleText, Style::kAccent));
        connect(zahnrad, &QPushButton::clicked, this,
                &Ft8Applet::zeigeOptionen);
        kopf->addWidget(zahnrad);
    }
    root->addWidget(leiste);

    // ── Kopfzeile: Verfahren + Zustand ──────────────────────────────
    //
    // ── Die drei Reihen brechen um (2026-11-18) ──────────────────────
    //
    // Betreiber, mit Bildern seines Fensters bei 460 bis 840 Punkt
    // Breite: „hier finde ich nun keinen button mehr für CQ" --
    // „versteckt sich ganz rechts, somit ist es nicht sichtbar" -- „man
    // sollte hier zu sehen auch dieses fenster kleiner ziehen können,
    // alle pillen usw. müssen natürlich dann auch verschoben werden".
    //
    // Kopfreihe, Sicht-Pillen und Sendereihe waren je ein QHBoxLayout:
    // EINE Zeile, deren Mindestbreite die Summe ihrer Teile ist
    // (gemessen: 653 Punkt). Schmaler ging das Fenster zwar, aber dann
    // rollte der Rollbereich darunter waagrecht, und was rechts stand,
    // war weg -- ausgerechnet CQ, Send und Stop.
    //
    // Jetzt sind es drei FlowLayouts: dieselben Elemente in derselben
    // Reihenfolge, die in die naechste Zeile rutschen, wenn der Platz
    // ausgeht. Ist Platz, steht alles, wie es stand. Die Namen `head`,
    // `sichtReihe` und `sendRow` und ihre Aufrufe (addWidget,
    // addSpacing, addStretch, insertWidget) sind geblieben: wer eine
    // weitere Pille anhaengt, schreibt dieselbe Zeile wie bisher.
    auto* head = new FlowLayout(nullptr, 6, 6);
    // Beschriftung und Feld gehoeren zusammen: als EIN Stueck in der
    // Reihe, damit der Umbruch nie zwischen „TX Hz" und seine Zahl
    // faellt.
    const auto paar = [this](QWidget* schild, QWidget* feld) {
        auto* stueck = new QWidget(this);
        auto* h = new QHBoxLayout(stueck);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(6);
        h->addWidget(schild);
        h->addWidget(feld);
        return stueck;
    };

    m_ft8Btn = blueToggle(QStringLiteral("FT8"), 48);
    m_ft8Btn->setCheckable(true);
    m_ft8Btn->setChecked(true);
    m_ft8Btn->setToolTip(tr("15 second periods (FT8)"));
    connect(m_ft8Btn, &QPushButton::clicked, this, [this]() {
        emit digiGewuenscht(DigiFrequenzen::Digimodus::Ft8);
        setProtocol(Ft8Decoder::Protocol::Ft8);
    });
    head->addWidget(m_ft8Btn);

    m_ft4Btn = blueToggle(QStringLiteral("FT4"), 48);
    m_ft4Btn->setCheckable(true);
    m_ft4Btn->setToolTip(tr("7.5 second periods (FT4)"));
    connect(m_ft4Btn, &QPushButton::clicked, this, [this]() {
        emit digiGewuenscht(DigiFrequenzen::Digimodus::Ft4);
        setProtocol(Ft8Decoder::Protocol::Ft4);
    });
    head->addWidget(m_ft4Btn);

    // ── Hound: eine DX-Expedition rufen (2026-10-30) ──────────
    //
    // Im Fox-Betrieb laeuft ein QSO anders: den Schluss macht die
    // Expedition, und wer unter 1000 Hz sendet, wird gar nicht erst
    // beantwortet. Der Ablauf dafuer steht in `Ft8Ablauf::
    // starteAlsHound`; hier wird nur gesagt, dass er gelten soll.
    //
    // Ein Schalter, keine Erkennung: ob eine Station im Fox-Betrieb
    // laeuft, steht erst in ihrem 0.1-Satz -- und den hoert man erst,
    // wenn sie jemanden AUFRUFT. Bis dahin sieht ein Fox-CQ wie jedes
    // andere aus. Der Betreiber weiss es vorher, die Maschine nicht.
    m_houndBtn = blueToggle(tr("Hound"), 58);
    m_houndBtn->setToolTip(
        tr("Call a DXpedition running in Fox mode: the Fox closes the "
           "QSO, and a Hound transmitting below 1000 Hz is not "
           "answered — the transmit tone is raised if needed."));
    connect(m_houndBtn, &QPushButton::clicked, this, [this](bool an) {
        m_houndBetrieb = an;
        AppSettings::instance().setValue(
            QStringLiteral("Ft8/Hound"),
            an ? QStringLiteral("True") : QStringLiteral("False"));
        if (!m_tonFeld) { return; }
        if (an) {
            if (m_tonFeld->value() < Ft8Ablauf::kHoundMinTonHz) {
                // Lieber hochziehen als warnen: eine Warnung, die man
                // wegklickt, laesst einen stundenlang ins Leere rufen.
                m_tonVorHound = m_tonFeld->value();
                m_tonFeld->setValue(Ft8Ablauf::kHoundMinTonHz + 500);
                if (m_status) {
                    m_status->setText(tr("tone %1 → %2 Hz (Hounds below "
                                         "1000 are not answered)")
                                          .arg(m_tonVorHound)
                                          .arg(m_tonFeld->value()));
                }
            }
        } else if (m_tonVorHound > 0) {
            // ── Und zurueck (2026-10-30) ───────────────────
            //
            // Am Geraet gesehen: der Betreiber hat Hound ein- und
            // wieder ausgeschaltet, und seine 837 Hz waren weg -- 1500
            // blieben stehen. Eine Einstellung, die ich fuer eine
            // Betriebsart hochziehe, gebe ich zurueck, wenn die
            // Betriebsart endet. Sie gehoert ihm, nicht mir.
            const int zurueck = m_tonVorHound;
            m_tonVorHound = 0;
            m_tonFeld->setValue(zurueck);
            if (m_status) {
                m_status->setText(tr("tone back to %1 Hz").arg(zurueck));
            }
        }
    });
    m_houndBetrieb = AppSettings::instance()
                         .value(QStringLiteral("Ft8/Hound"),
                                QStringLiteral("False")).toString()
                     == QStringLiteral("True");
    m_houndBtn->setChecked(m_houndBetrieb);
    head->addWidget(m_houndBtn);

    // ── Contestbetrieb (2026-11-04) ─────────────────────────────────
    //
    // Im EU-VHF-Contest tritt an die Stelle des Rapports ein
    // Austausch: 59, laufende Nummer, sechsstelliger Feldkenner. Das
    // ist Nachrichtentyp 5 (siehe `Ft8Ablauf::Contest`).
    //
    // Ein Schalter, keine Erkennung -- genau wie beim Hound daneben:
    // ob gerade Contest ist, weiss der Betreiber, nicht die Maschine.
    //
    // Der Feldkenner kommt aus den STATIONSDATEN, nicht aus einem
    // zweiten Feld hier. Eine Angabe, eine Stelle; wer sie aendert,
    // aendert sie fuer alles. Hat sie nur vier Stellen, sagt der
    // Schalter das beim Einschalten und bleibt aus -- lieber
    // ablehnen als stillschweigend etwas anderes senden.
    m_contestBtn = blueToggle(tr("Contest"), 68);
    m_contestBtn->setToolTip(
        tr("EU VHF contest exchange: 59, running serial and your "
           "six-character locator instead of the signal report. Needs "
           "a six-character locator in the station data."));
    connect(m_contestBtn, &QPushButton::clicked, this, [this](bool an) {
        if (an && eigenerLocator().size() < 6) {
            m_contestBtn->setChecked(false);
            if (m_status) {
                m_status->setText(
                    tr("contest needs a six-character locator — "
                       "yours is \"%1\"").arg(eigenerLocator()));
            }
            return;
        }
        m_contestAn = an;
        AppSettings::instance().setValue(
            QStringLiteral("Ft8/Contest"),
            an ? QStringLiteral("True") : QStringLiteral("False"));
        uebernimmContest();
        zeigeContestStand();
    });
    m_contestAn = AppSettings::instance()
                      .value(QStringLiteral("Ft8/Contest"),
                             QStringLiteral("False")).toString()
                  == QStringLiteral("True");
    // Beim Hochfahren still abschalten, wenn der Feldkenner nicht
    // taugt: sonst stuende der Schalter an und saendte doch nichts.
    if (m_contestAn && eigenerLocator().size() < 6) { m_contestAn = false; }
    m_contestBtn->setChecked(m_contestAn);
    head->addWidget(m_contestBtn);

    head->addSpacing(8);

    // ── „CQ only" ───────────────────────────────────────────────────
    //
    // Betreiber 2026-10-18, nach dem ersten Blick in das Fenster:
    // „dachte das sind dann mehr optionen usw. wie ein richtiges ft8
    // programm". In WSJT-X ist das der haeufigste Griff ueberhaupt --
    // man sucht jemanden, den man rufen KANN, und das sind die
    // CQ-Zeilen.
    //
    // Nur die Ansicht wird gefiltert. Der Ablauf bekommt weiter jeden
    // Fund (siehe `inSicht`): sonst verschwaende ein
    // laufendes QSO, sobald der Haken sitzt.
    // ── Die zwei Haken sind Pillen geworden (2026-10-30) ──────
    //
    // „CQ only" und „New DXCC only" standen hier als zwei Haken in der
    // Kopfzeile, waehrend unten vier Pillen dasselbe taten. Zwei
    // Filtersysteme nebeneinander sind eines zu viel: zwei Haken
    // gleichzeitig ergaben die Schnittmenge, ohne dass das irgendwo
    // stand, und die Kopfzeile wurde so breit, dass das Panel sich bei
    // 460 Punkten nicht mehr zeichnen liesz (im gerenderten Blatt
    // gemessen: 706 statt 460).
    //
    // Beides steckt jetzt in den Pillen unter dem Kopf. Die Zugaenge
    // `nurCqButtonForTest` / `nurNeueLaenderButtonForTest` zeigen
    // dorthin — derselbe Zweck, dieselbe Pruefung.

    m_wasserfallBtn = blueToggle(tr("Waterfall"), 72);
    m_wasserfallBtn->setToolTip(tr("Show the waterfall above the list."));
    connect(m_wasserfallBtn, &QPushButton::clicked, this, [this](bool an) {
        m_wasserfallAn = an;
        if (m_wasserfall) { m_wasserfall->setVisible(an); }
        AppSettings::instance().setValue(
            QStringLiteral("Ft8/Wasserfall"),
            an ? QStringLiteral("True") : QStringLiteral("False"));
    });
    head->addWidget(m_wasserfallBtn);
    // Der Knopf zeigt, was gilt — die Wahl steht in AppSettings.

    head->addSpacing(8);

    // ── Die Sendestelle ─────────────────────────────────────────────
    auto* tonBez = new QLabel(tr("TX Hz"), this);
    tonBez->setFont(Style::monoFont(font(), Style::kFontSmall));
    tonBez->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary));

    m_sendeTonHz = AppSettings::instance()
                       .value(QLatin1String(kSendeTonSchluessel),
                              kSendeTonVorgabeHz)
                       .toInt();
    if (m_sendeTonHz < kSendeTonMinHz || m_sendeTonHz > kSendeTonMaxHz) {
        m_sendeTonHz = kSendeTonVorgabeHz;
    }
    m_tonFeld = new QSpinBox(this);
    m_tonFeld->setRange(kSendeTonMinHz, kSendeTonMaxHz);
    m_tonFeld->setSingleStep(10);
    m_tonFeld->setValue(m_sendeTonHz);
    m_tonFeld->setFont(Style::monoFont(font(), Style::kFontSmall));
    m_tonFeld->setToolTip(tr(
        "Where in the passband you transmit. Pick a clear spot — "
        "every call of yours goes out here."));
    connect(m_tonFeld, &QSpinBox::valueChanged, this, [this](int hz) {
        m_sendeTonHz = hz;
        AppSettings::instance().setValue(QLatin1String(kSendeTonSchluessel), hz);
        if (m_wasserfall) { m_wasserfall->setzeSendeTon(hz); }
    });
    head->addWidget(paar(tonBez, m_tonFeld));

    head->addSpacing(8);

    // ── Wie viele Runden der Dekoder ueber ein Fenster geht ──────────
    //
    // WSJT-X hat dasselbe als „Decode: Fast / Normal / Deep", JTDX
    // ebenso. Zwei ist die Vorgabe: nach der ersten Runde werden die
    // gefundenen Signale abgezogen, damit schwaechere darunter
    // auffindbar werden (core/Ft8Subtract.h, portiert aus MSHV).
    //
    // Der Knopf sitzt hier und nicht in einer Einstellseite, weil er
    // die Gegenprobe AN DER ANTENNE moeglich macht: was die zweite
    // Runde am Pruefstand leistet, ist belegt -- was sie auf dem Band
    // leistet, sieht man nur, wenn man dieselbe Minute einmal so und
    // einmal so hoeren kann, ohne das Programm neu zu starten.
    auto* rundenBez = new QLabel(tr("Passes"), this);
    rundenBez->setFont(Style::monoFont(font(), Style::kFontSmall));
    rundenBez->setStyleSheet(QStringLiteral("color: %1;")
                                 .arg(Style::kTextSecondary));

    m_rundenFeld = new QSpinBox(this);
    m_rundenFeld->setRange(1, 3);
    m_rundenFeld->setValue(AppSettings::instance()
                               .value(QStringLiteral("Ft8/Durchgaenge"),
                                      QStringLiteral("2"))
                               .toInt());
    m_rundenFeld->setFixedWidth(46);
    m_rundenFeld->setFont(Style::monoFont(font(), Style::kFontSmall));
    m_rundenFeld->setToolTip(tr(
        "How often the decoder goes over the same period. After each "
        "pass the signals it found are subtracted, so weaker ones "
        "underneath become readable.\n1 is one pass, no subtraction. "
        "More passes cost a little more time per period."));
    connect(m_rundenFeld, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [](int n) {
        AppSettings::instance().setValue(QStringLiteral("Ft8/Durchgaenge"),
                                         QString::number(n));
    });
    head->addWidget(paar(rundenBez, m_rundenFeld));

    head->addSpacing(8);

    m_status = new QLabel(QStringLiteral("—"), this);
    m_status->setFont(Style::monoFont(font(), Style::kFontSmall));
    m_status->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary));
    // Der Zustand nimmt, was in seiner Zeile uebrig ist, und zaehlt
    // beim Umbruch mit einer festen Breite -- NICHT mit seinem Text.
    // Der wechselt alle paar Sekunden („listening", „tone 1500 → 2000
    // Hz (Hounds below …)"), und mit ihm waere die Reihe umgebrochen
    // und wieder zurueck: die Fundliste darunter spraenge auf und ab.
    // Vorher machte ein langer Text die ganze Reihe breiter. Darum auch
    // kein addStretch() mehr dahinter: der Zustand IST der Rest.
    m_status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_status->setMinimumWidth(
        m_status->fontMetrics().horizontalAdvance(QString(14, QLatin1Char('0'))));
    head->addWidget(m_status);
    root->addLayout(head);

    // Der Fuellstand des laufenden Fensters. Ohne ihn sieht ein
    // Dekoder, der gerade sammelt, genauso aus wie einer, der nichts
    // bekommt — und bei fuenfzehn Sekunden Fenster ist das die
    // haeufigste Fehldeutung.
    // ── Der Streifen ───────────────────────────────────────────────
    //
    // Zwischen Kopfzeile und Fuellstand: erst sehen, wo frei ist, dann
    // die Stelle waehlen. Ein Klick darin setzt das Feld „TX Hz" und
    // umgekehrt -- eine Wahrheit, zwei Wege sie zu fassen.
    m_wasserfall = new Ft8Wasserfall(this);
    m_wasserfall->setzeSendeTon(m_sendeTonHz);
    connect(m_wasserfall, &Ft8Wasserfall::sendeTonGewaehlt, this,
            [this](int hz) {
                if (m_tonFeld) { m_tonFeld->setValue(hz); }
            });
    root->addWidget(m_wasserfall);

    // ── Der Wasserfall ist abschaltbar (Entwurf C, 2026-10-30) ───
    //
    // Er nahm ein Viertel der Hoehe und stand im gerenderten Blatt
    // leer da — er lebt erst, wenn Ton hereinkommt. Wer ihn braucht,
    // schaltet ihn ein; wer die Liste braucht, bekommt den Platz.
    // Die Wahl bleibt erhalten.
    m_wasserfallAn = AppSettings::instance()
                         .value(QStringLiteral("Ft8/Wasserfall"),
                                QStringLiteral("True")).toString()
                     == QStringLiteral("True");
    m_wasserfall->setVisible(m_wasserfallAn);
    if (m_wasserfallBtn) { m_wasserfallBtn->setChecked(m_wasserfallAn); }

    // ── Der Fuellstand sitzt jetzt in der Arbeitsleiste (30.10.) ──
    //
    // Hier stand ein vier Pixel hoher Fortschrittsbalken ueber der
    // Liste, der dasselbe zeigte wie der Fensterbalken unten -- nur
    // ohne die Marken fuer Signalanfang und -ende, und an einer
    // Stelle, an der niemand hinsieht. Am laufenden Geraet standen
    // beide uebereinander im Bild.
    root->addWidget(divider());

    // ── Die Liste ───────────────────────────────────────────────────
    m_list = new QPlainTextEdit(this);
    m_list->setReadOnly(true);
    // Doppelklick auf eine Zeile legt den Standardanruf ins
    // Eingabefeld (siehe antwortAuf). Am viewport, nicht am Widget:
    // die Mausereignisse einer Textflaeche kommen dort an.
    m_list->viewport()->installEventFilter(this);
    m_list->setToolTip(tr("Double-click a line to answer that station."));
    m_list->setFont(Style::monoFont(font(), Style::kFontSmall));
    m_list->setMinimumHeight(120);
    m_list->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_list->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: %1; color: %2; border: 1px solid %3; }")
        .arg(Style::kInsetBg, Style::kTextPrimary, Style::kInsetBorder));

    // ── „+ Queue" an der Zeile unter der Maus (2026-11-15) ─────────
    //
    // Wie bei ZeusSDR jede Fundzeile ihren Knopf „Queue" hat -- nur
    // erscheint er hier erst, wenn die Maus auf der Zeile steht. Zwanzig
    // Knoepfe je Fenster waeren zwanzig Dinge, die man nicht braucht,
    // bis man genau eines braucht. Am Viewport, nicht in einem Layout:
    // er schwebt ueber der Zeile, die gerade gemeint ist.
    m_list->viewport()->setMouseTracking(true);
    m_queuePille = new QPushButton(tr("+ Queue"), m_list->viewport());
    m_queuePille->setObjectName(QStringLiteral("ft8QueuePille"));
    m_queuePille->setCursor(Qt::PointingHandCursor);
    m_queuePille->setToolTip(tr("Call this station after the current QSO"));
    {
        QFont f = font();
        f.setPointSize(Style::kFontCaption);
        f.setBold(true);
        m_queuePille->setFont(f);
    }
    m_queuePille->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 7px; padding: 0px 7px; }"
        "QPushButton:hover { color: %4; border-color: %2; }")
        .arg(Style::kAmberBg, Style::kAmberText, Style::kAmberBorder,
             Style::kTextPrimary));
    m_queuePille->setFixedHeight(15);
    m_queuePille->hide();
    connect(m_queuePille, &QPushButton::clicked, this, [this] {
        const QString zeile = m_pilleZeile;
        versteckePille();
        const QString grund = stelleZeileAn(zeile);
        if (!grund.isEmpty() && m_status) { m_status->setText(grund); }
    });
    // Rollt die Liste oder kommt ein neues Fenster herein, steht die
    // Kapsel an der falschen Zeile. Die naechste Mausbewegung setzt sie
    // neu.
    connect(m_list->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this] { versteckePille(); });
    connect(m_list->document(), &QTextDocument::contentsChanged,
            this, [this] { versteckePille(); });

    // Zweite Spalte: „Rx Frequency" wie in WSJT-X. Links alles, rechts
    // nur, was auf der eigenen Stelle liegt oder an mich geht. In
    // einem vollen Band ist die linke Liste nach zwei Durchgaengen
    // voll -- die eine Zeile, auf die es ankommt, scrollt weg.

    // ── Eine Liste statt zweier (Entwurf C, 2026-10-30) ───────
    //
    // Bis dahin standen „Band Activity" und „Rx Frequency"
    // nebeneinander, wie in WSJT-X. Im Panel sind das zwei Spalten zu
    // je 214 Punkten, und dort passt die Zeile nicht hinein: im
    // gerenderten Blatt vom 2026-10-30 stand „Austr…" und „Fed." statt
    // des Landes, und rechts fehlte das Ende der Nachricht. Dazu zeigte
    // die rechte Spalte meistens dieselben Zeilen noch einmal.
    //
    // Jetzt eine Liste ueber die ganze Breite, und welche Zeilen zu
    // sehen sind, sagen vier Pillen. „Meine Frequenz" ist die alte
    // rechte Spalte — dieselbe Regel (eigene Tonlage ± 50 Hz, oder an
    // mich gerichtet), nur als Sicht statt als zweites Fenster.
    //
    // Betreiber am 2026-10-30, nach drei Entwurfsblaettern: „c bitte".
    // Bricht um wie die Kopfreihe (2026-11-18); 3 Punkt zwischen den
    // Pillen wie bisher.
    auto* sichtReihe = new FlowLayout(nullptr, 3, 4);
    // Zaehler und `Clear` bleiben am rechten Rand, auch wenn sie in
    // die zweite Zeile rutschen.
    sichtReihe->setStretchFollowsNext(true);
    const auto sichtKnopf = [this, sichtReihe](const QString& text,
                                               Sicht welche, int breite) {
        QPushButton* b = blueToggle(text, breite);
        b->setCheckable(true);
        new MessingStrich(b);   // gehoert der Pille
        connect(b, &QPushButton::clicked, this, [this, welche]() {
            m_sicht = welche;
            zeichneListe();
        });
        sichtReihe->addWidget(b);
        return b;
    };
    m_sichtAlles  = sichtKnopf(tr("All"),       Sicht::Alles, 46);
    m_sichtAnMich = sichtKnopf(tr("To me"),     Sicht::AnMich, 54);
    m_sichtCq     = sichtKnopf(tr("CQ"),        Sicht::Cq, 38);
    m_sichtNeuesLand = sichtKnopf(tr("New DXCC"), Sicht::NeuesLand, 72);
    // Betreiber am 2026-10-30: „somit sehe ich dann, was ich noch
    // benoetige." Derselbe Knopf wie „New DXCC", eine Stufe kleiner.
    m_sichtNeuesGrid = sichtKnopf(tr("New grid"), Sicht::NeuesGrid, 66);
    m_sichtFreq   = sichtKnopf(tr("My freq"),   Sicht::MeineFrequenz, 62);
    m_sichtAlles->setChecked(true);
    m_sichtAnMich->setToolTip(tr("Only messages addressed to you."));
    m_sichtCq->setToolTip(tr("Only CQ calls."));
    m_sichtNeuesLand->setToolTip(tr("Only countries that are not in the "
                                    "log yet."));
    m_sichtNeuesGrid->setToolTip(tr("Grid squares you still need. Blue "
                                    "in the list: not in the log at all. "
                                    "Dimmed blue: in the log, but not on "
                                    "this band."));

    // ── Das Logbuch einmal lesen (2026-10-30) ────────────────────
    //
    // Hier und nicht in `MainWindow`: die Markierung gehoert diesem
    // Fenster, und ein Fenster, das seine eigene Auskunft von einer
    // Verdrahtung abhaengig macht, ist genau das, was heute dreimal
    // stillschweigend ausgefallen ist.
    //
    // Einmal beim Bauen. Die Datei waechst um ein QSO je Minute;
    // sie bei jedem Fund neu zu lesen waere Aufwand ohne Gewinn --
    // frische QSOs kommen ueber `merke()` dazu (siehe
    // `meldeFertigesQso`).
    {
        const int gelesen = m_grids.liesAus(LogbookDatei::pfad());
        qCInfo(lcFt8Senden).noquote()
            << "Grossfelder aus dem Logbuch:" << m_grids.anzahl()
            << "verschiedene aus" << gelesen << "Eintraegen";

        // ── Dieselbe Datei, je Band gelesen (2026-11-12) ──────────
        //
        // Zweimal lesen, nicht einmal: `GridsGearbeitet` zieht nur die
        // Felder heraus und ist darum billig, das Mosaik braucht die
        // ganzen Eintraege (Band, Betriebsart, Bestaetigung). Beides
        // laeuft EINMAL beim Bauen des Fensters, nicht je Fund.
        //
        // `nurFt8` bleibt falsch: ein Feld, das in SSB im Logbuch
        // steht, ist gearbeitet. Wer es in FT8 noch sucht, sucht ein
        // Band-und-Modus-Haekchen -- das ist eine andere Frage, und
        // sie bekommt erst eine Bedienung, wenn der Betreiber sie
        // stellt.
        const QVector<LogEntry> log = AdifLog::read(LogbookDatei::pfad());
        m_mosaik.baue(log, false);
        qCInfo(lcFt8Senden).noquote()
            << "Mosaik je Band:" << m_mosaik.baender().size()
            << "Baender aus" << log.size() << "Eintraegen";
    }
    m_sichtFreq->setToolTip(tr("Decodes within 50 Hz of your transmit "
                               "tone, plus anything addressed to you — "
                               "what the second column used to show."));
    sichtReihe->addStretch(1);
    // Zaehler, Uhrwarnung und `Clear` als EIN Stueck: sie brechen
    // zusammen um und stehen zusammen rechts (2026-11-18).
    auto* rechts = new QWidget(this);
    auto* rechtsReihe = new QHBoxLayout(rechts);
    rechtsReihe->setContentsMargins(0, 0, 0, 0);
    rechtsReihe->setSpacing(3);
    sichtReihe->addWidget(rechts);
    m_fundZahl = new QLabel(QString(), this);
    m_fundZahl->setFont(Style::monoFont(font(), Style::kFontMicro));
    m_fundZahl->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                  .arg(Style::kTextInactive));
    rechtsReihe->addWidget(m_fundZahl);

    // ── Die Uhr, wenn sie falsch geht (2026-11-13) ──────────────
    //
    // Hier und nirgends sonst: wer sich fragt, warum nichts
    // hereinkommt, sieht auf die Fundzahl. Genau daneben gehoert die
    // einzige Erklaerung, die nicht am Band liegt. Messing, nicht
    // Rot -- das ist ein Hinweis, kein Alarm.
    m_uhrWarnung = new QLabel(QString(), this);
    m_uhrWarnung->setFont(Style::monoFont(font(), Style::kFontMicro));
    m_uhrWarnung->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                    .arg(Style::kAmberWarn));
    m_uhrWarnung->setVisible(false);
    rechtsReihe->addWidget(m_uhrWarnung);

    m_clearBtn = styledButton(tr("Clear"), 54);
    m_clearBtn->setCheckable(false);   // leert die Liste, rastet nicht ein
    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        // Auch den Speicher: sonst kaeme beim naechsten Pillendruck
        // alles zurueck, was gerade geloescht wurde.
        m_zeilen.clear();
        zeichneListe();
    });
    rechtsReihe->addWidget(m_clearBtn);
    root->addLayout(sichtReihe);

    // ── Die Spaltenkoepfe (2026-10-30) ──────────────────────────
    //
    // Betreiber: „die staerke des signals sollte auch in einer
    // eigenen spalten sein." Sie ist eine -- die zweite. Ohne
    // Ueberschrift war nur nicht zu sehen, welche der vier Zahlen
    // welche ist. WSJT-X schreibt sie ueber die Liste, und genau
    // dort hat er sie gesucht.
    m_spaltenkopf = new QLabel(spaltenkopf(), this);
    m_spaltenkopf->setFont(Style::monoFont(font(), Style::kFontSmall));
    m_spaltenkopf->setStyleSheet(
        QStringLiteral("QLabel { color: %1; padding: 0 0 2px 6px; }")
            .arg(QString::fromLatin1(Style::kTextInactive)));
    // Die Ueberschrift ist so breit wie alle Spalten zusammen. Als
    // gewoehnliche Beschriftung verlangte sie diese Breite auch vom
    // Fenster; sie darf rechts enden wie die Liste unter ihr
    // (2026-11-18).
    m_spaltenkopf->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    root->addWidget(m_spaltenkopf);

    root->addWidget(m_list, 1);

    // ── Fusszeile ───────────────────────────────────────────────────
    // ── Senden ──────────────────────────────────────────────────────
    //
    // Eine Zeile: was gesendet wird, und zwei Knoepfe. Kein
    // Folgeschalter, keine Automatik — siehe Ft8Applet.h.
    //
    // Bricht um wie die beiden Reihen oben (2026-11-18). Das Textfeld
    // dehnt sich in seiner Zeile; `CQ`, `Send` und `Stop` rutschen in
    // die naechste, statt rechts aus dem Fenster zu fallen.
    auto* sendRow = new FlowLayout(nullptr, 6, 6);
    m_textFeld = new QLineEdit(this);
    m_textFeld->setFont(Style::monoFont(font(), Style::kFontSmall));
    m_textFeld->setPlaceholderText(tr("CQ <call> <grid>"));
    m_textFeld->setStyleSheet(QStringLiteral(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3;"
        " padding: 2px 4px; }")
        .arg(Style::kInsetBg, Style::kTextPrimary, Style::kInsetBorder));
    sendRow->addWidget(m_textFeld);
    // (der „Manual"-Knopf kommt weiter unten dazu)

    // ── CQ rufen ────────────────────────────────────────────────────
    //
    // Ft8Ablauf kann seit dem QSO-Ablauf auch die ANRUFER-Rolle
    // (starteAlsAnrufer, dreizehn Prueffaelle) — nur rief sie
    // niemand auf. Antworten ging per Doppelklick, selbst CQ rufen
    // gar nicht. Dieser Knopf schliesst das, mit demselben Verhalten
    // wie der Doppelklick: er faengt den Ablauf an und laesst ihn
    // bis 73 laufen.
    // ── Die Arbeitsleiste (Entwurf C, 2026-10-30) ────────────
    //
    // Was beim automatischen Durchlauf zaehlt, steht unten, wo die
    // Hand ist: mit wem, welcher Zug, was danach kommt, wie weit das
    // Fenster. Vorher stand davon nichts im Panel — der Zustand war
    // nur aus der Statuszeile zu erraten, und der Fensterfortschritt
    // stand grau im Panelkopf, an der unauffaelligsten Stelle fuer das
    // einzige Ding, das sich dauernd bewegt.
    m_arbeit = new QWidget(this);
    // Ein Messingstrich links: dieselbe Rolle wie der Panelakzent im
    // Kopf — hier faengt etwas an, das gemessen wird.
    //
    // Radius 6, nicht 3. HAUSSTIL.md: „Nie Radius 3. Das ist der
    // Qt-Standardwert und laesst alles nach Qt aussehen." Der
    // Prueflauf tst_hausstil_radius faengt das -- und hat es bei
    // dieser Leiste auch getan (2026-10-30).
    m_arbeit->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border: 1px solid %2;"
        " border-left: 3px solid %3; border-radius: 6px; }")
        .arg(Style::kPanelBg, Style::kBorder, Style::kTxKeyBottom));
    auto* arbeit = new QGridLayout(m_arbeit);
    arbeit->setContentsMargins(10, 6, 10, 7);
    arbeit->setHorizontalSpacing(8);
    arbeit->setVerticalSpacing(4);

    const auto schild = [this](const QString& t, const char* farbe,
                               int groesse, bool fett = false) {
        auto* l = new QLabel(t, m_arbeit);
        QFont f = font();
        f.setPointSize(groesse);
        f.setBold(fett);
        l->setFont(f);
        l->setStyleSheet(QStringLiteral("QLabel { color: %1; border: none; }")
                             .arg(QString::fromLatin1(farbe)));
        return l;
    };

    auto* kopfReihe = new QHBoxLayout();
    kopfReihe->setSpacing(7);
    m_arbeitZustand = schild(tr("idle"), Style::kTextSecondary,
                             Style::kFontSmall);
    m_arbeitPartner = schild(QString(), Style::kTextPrimary,
                             Style::kFontSub, true);
    m_arbeitLand    = schild(QString(), Style::kTextInactive,
                             Style::kFontSmall);
    kopfReihe->addWidget(m_arbeitZustand);
    kopfReihe->addWidget(m_arbeitPartner);
    // Das Land nimmt den Rest der Zeile und gibt ihn zuerst her, wenn
    // es eng wird (2026-11-18) -- vorher stand dahinter ein Dehnraum,
    // und das Land verlangte seine volle Breite vom Fenster.
    m_arbeitLand->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    kopfReihe->addWidget(m_arbeitLand, 1);
    m_arbeitUhr = schild(QString(), Style::kTextInactive, Style::kFontSmall);
    m_arbeitUhr->setFont(Style::monoFont(font(), Style::kFontSmall));
    kopfReihe->addWidget(m_arbeitUhr);
    arbeit->addLayout(kopfReihe, 0, 0, 1, 2);

    // ── Wer er ist und wie es bei ihm aussieht (2026-11-18) ─────────
    //
    // Vorbild: die Rufzeichenkarte neben Zeus' FT8-Feld. Von drei
    // Entwurfsblaettern (Zeile, kleines Foto, Spalte wie Zeus) die
    // Zeile -- das grosse Foto steht schon in der Rotor/Log-Karte, die
    // dem Partner folgt. Siehe Ft8PartnerZeile.h.
    m_arbeitWer = new Ft8PartnerZeile(m_arbeit);
    arbeit->addWidget(m_arbeitWer, 1, 0, 1, 2);

    // Der naechste Zug im Klartext — dieselbe Schrift wie die Funde,
    // damit man ihn als FT8-Satz liest und nicht als Beschriftung.
    m_arbeitZug = new QLabel(QString(), m_arbeit);
    m_arbeitZug->setFont(Style::monoFont(font(), Style::kFontSmall));
    m_arbeitZug->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: %2; border: 1px solid %3;"
        " padding: 3px 6px; border-radius: 2px; }")
        .arg(Style::kInsetBg, Style::kTxKeyBorder, Style::kInsetBorder));
    arbeit->addWidget(m_arbeitZug, 2, 0);

    m_arbeitDanach = schild(QString(), Style::kTextSecondary,
                            Style::kFontCaption);
    m_arbeitDanach->setMinimumWidth(118);
    m_arbeitDanach->setWordWrap(true);
    arbeit->addWidget(m_arbeitDanach, 2, 1, 2, 1, Qt::AlignTop);

    m_arbeitBalken = new Ft8Fensterbalken(m_arbeit);
    m_arbeitBalken->setFixedHeight(10);
    arbeit->addWidget(m_arbeitBalken, 3, 0);
    arbeit->setColumnStretch(0, 1);

    // ── „THEN": wer nach diesem QSO drankommt (2026-11-15) ─────────
    //
    // Betreiber: „C bauen" -- von drei Entwurfsblaettern das, in dem
    // die Reihenfolge unter dem laufenden QSO steht. Der Blick bleibt
    // an einer Stelle: oben mit wem, darunter wer danach. Die
    // Entscheidungen (umreihen, streichen, vorziehen) trifft
    // `Ft8Warteschlange`; hier wird nur verbunden.
    m_dann = new Ft8DannListe(m_arbeit);
    arbeit->addWidget(m_dann, 4, 0, 1, 2);
    connect(m_dann, &Ft8DannListe::hoch, this, [this](const QString& wer) {
        if (m_schlange.ruecke(wer, -1, QDateTime::currentMSecsSinceEpoch())) {
            meldeStand();
        }
    });
    connect(m_dann, &Ft8DannListe::runter, this, [this](const QString& wer) {
        if (m_schlange.ruecke(wer, +1, QDateTime::currentMSecsSinceEpoch())) {
            meldeStand();
        }
    });
    connect(m_dann, &Ft8DannListe::weg, this, [this](const QString& wer) {
        m_schlange.weiseAb(wer, QDateTime::currentMSecsSinceEpoch());
        qCInfo(lcFt8Senden).noquote()
            << "Aus der Schlange gestrichen:" << wer;
        zeigeFundZahl();
        meldeStand();
    });
    connect(m_dann, &Ft8DannListe::gewaehlt,
            this, [this](const QString& wer) { dannGewaehlt(wer); });
    m_dannUhr = new QTimer(this);
    m_dannUhr->setInterval(1000);
    connect(m_dannUhr, &QTimer::timeout, this, [this] { zeigeDann(); });

    // Die Arbeitsleiste nimmt die Breite, die das Fenster hat, und
    // verlangt keine (2026-11-18): was in ihr steht -- der naechste
    // Zug, die Schlange -- wechselt mit jedem QSO, und daran darf nicht
    // haengen, ob `Stop` noch im Fenster liegt.
    m_arbeit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    root->addWidget(m_arbeit);

    // ── Tx1..Tx6 hinter einem Knopf ─────────────────────
    //
    // Sechs gleich aussehende Knoepfe in einer Reihe sind Handbetrieb,
    // und Handbetrieb ist beim automatischen Durchlauf die Ausnahme.
    // Sie legen den Satz weiterhin nur ins Feld — gesendet wird mit
    // „Send".
    m_handBtn = styledButton(tr("Manual"), 86);
    m_handBtn->setCheckable(false);
    m_handBtn->setToolTip(tr("The six standard messages Tx1…Tx6."));
    auto* handMenu = new QMenu(m_handBtn);
    for (int n = 1; n <= 6; ++n) {
        QAction* a = handMenu->addAction(QStringLiteral("Tx%1").arg(n));
        connect(a, &QAction::triggered, this, [this, n]() {
            const QString gegen = aktuellerPartner();
            const QString was = txKnopfNachricht(
                n, gegen, eigenesRufzeichen(), eigenerLocator(),
                m_letzterRapport.value(gegen, 0));
            if (was.isEmpty()) {
                if (m_status) {
                    m_status->setText(n == 6 ? tr("set your callsign first")
                                             : tr("no station selected"));
                }
                return;
            }
            if (m_textFeld) { m_textFeld->setText(was); }
        });
        m_txAktionen[n - 1] = a;
    }
    m_handBtn->setMenu(handMenu);
    sendRow->insertWidget(0, m_handBtn);

    // `blueToggle` statt `styledButton`: nur der erste haengt die
    // Blaufuellung des eingerasteten Knopfes an (Style::blueCheckedStyle).
    // Ohne sie aendert `setChecked` nichts Sichtbares — genau das meldete
    // der Betreiber am 2026-10-29: „der button sollte sich auch farblich
    // aendern, wenn er geklickt ist." Dieselbe Fuellung tragen FT8/FT4
    // darueber und die Bandwahl in der Kopfleiste.
    m_cqRufenBtn = blueToggle(tr("CQ"), 40);
    m_cqRufenBtn->setToolTip(tr(
        "Call CQ. It repeats by itself for as many periods as the "
        "box beside it says, and stops as soon as somebody answers.\n"
        "From there the QSO runs through to 73. Stop ends it."));
    connect(m_cqRufenBtn, &QPushButton::clicked, this, [this]() {
        const QString ruf = cqRuf(eigenesRufzeichen(), eigenerLocator());
        if (ruf.isEmpty()) {
            if (m_status) { m_status->setText(tr("set your callsign first")); }
            return;
        }
        if (m_textFeld) { m_textFeld->setText(ruf); }
        if (m_sendet) { return; }

        // Ein Knopf namens CQ ruft CQ.
        //
        // Betreiber am 2026-10-22: „der cq ruf funktioniert nicht."
        // Er tat bis dahin nur eines: den Text ins Feld schreiben.
        // Gesendet wurde nur, wenn zufaellig „Auto" an war -- sonst
        // musste man danach noch „Send" druecken. Das ist nicht, was
        // der Knopf verspricht.
        m_cqAutomatik = true;
        starteCqRuf();
    });
    sendRow->addWidget(m_cqRufenBtn);

    // Wie oft von selbst. Betreiber am 2026-10-22: „man soll auch
    // entscheiden koennen, wie oft er das automatisch probiert."
    //
    // Zehn ist die Vorgabe und keine erfundene Zahl: zehn Durchgaenge
    // sind zweieinhalb Minuten auf derselben Stelle -- lange genug,
    // dass jemand antwortet, kurz genug, dass ein vergessener Ruf
    // nicht den ganzen Abend laeuft.
    m_cqVersucheSpin = new QSpinBox(this);
    m_cqVersucheSpin->setRange(1, 30);
    m_cqVersucheSpin->setValue(AppSettings::instance()
                                   .value(QStringLiteral("Ft8/CqVersuche"),
                                          QStringLiteral("10"))
                                   .toInt());
    // ── Breit genug fuer zwei Stellen (2026-10-30) ───────────
    //
    // Hier stand `setFixedWidth(56)`. Am Geraet stand darin „x1" —
    // die zweite Stelle war weg, obwohl 16 eingestellt waren.
    // Betreiber: „neben dem CQ ist ein Fenster - anzahl der
    // wiederholungen. dies kann man nicht sehen!"
    //
    // Keine feste Zahl mehr: der Bereich geht bis 30, das Praefix ist
    // ein Zeichen, und wie breit das in der jeweiligen Schrift ist,
    // weiss die Schrift besser als ich. `sizeHint()` rechnet es aus
    // dem groeszten Wert samt Praefix und Pfeilen — eine feste Breite
    // ueberschreibt genau diese Rechnung.
    m_cqVersucheSpin->setPrefix(tr("x"));
    m_cqVersucheSpin->setMinimumWidth(m_cqVersucheSpin->sizeHint().width());
    m_cqVersucheSpin->setToolTip(tr(
        "How many periods a CQ call — or a call to one station — "
        "repeats by itself.\nIt stops earlier when somebody answers, "
        "or on Stop."));
    // Die Zahl gilt seit dem 2026-11-17 auch fuer den Anruf an EINE
    // Station (siehe `Ft8Ablauf::setzeAnrufWiederholungen`).
    m_ablauf.setzeAnrufWiederholungen(m_cqVersucheSpin->value());
    m_mehrfach.setzeAnrufWiederholungen(m_cqVersucheSpin->value());

    // ── Multi Answering: wie viele gleichzeitig (2026-10-30) ─────
    //
    // MSHVs Kernmerkmal. Die Technik lag seit #323/#333/#335 fertig
    // und geprueft im Baum und wurde von niemandem gerufen — hier ist
    // der Anschluss.
    //
    // Vorgabe 1 = aus. Die Sendeleistung wird GETEILT: zwei Plaetze
    // sind −6 dB je Station, drei −9,5, sechs −15,6 (MSHV
    // `gen_ft8.cpp:303`, und die Teilung ist kein Geschmack — der
    // Sender hat eine Hoechstamplitude). Bei 5 W und drei Plaetzen
    // bleiben 1,7 W je Station.
    m_plaetzeSpin = new QSpinBox(this);
    m_plaetzeSpin->setRange(1, Ft8Mehrfach::kMaxPlaetze);
    m_plaetzeSpin->setValue(AppSettings::instance()
                                .value(QStringLiteral("Ft8/Plaetze"),
                                       QStringLiteral("1"))
                                .toInt());
    m_plaetzeSpin->setPrefix(tr("slots "));
    m_plaetzeSpin->setMinimumWidth(m_plaetzeSpin->sizeHint().width());
    m_plaetzeSpin->setToolTip(tr(
        "Work several callers at once (MSHV\u2019s Multi Answering).\n"
        "The transmit power is SHARED: 2 slots are \u22126 dB each, "
        "3 are \u22129.5, 6 are \u221215.6.\n1 means off."));
    m_plaetze = qBound(1, m_plaetzeSpin->value(), Ft8Mehrfach::kMaxPlaetze);
    m_mehrfach.setPlaetze(m_plaetze);
    m_freieStelleBtn = styledButton(tr("Free"), 46);
    m_freieStelleBtn->setCheckable(false);
    m_freieStelleBtn->setToolTip(tr(
        "Move the transmit tone to a spot where nobody was heard.\n"
        "Longpath knows this from its own decodes \u2014 WSJT-X and MSHV "
        "do not."));
    connect(m_freieStelleBtn, &QPushButton::clicked, this, [this]() {
        sucheFreieStelle(false);
    });

    connect(m_plaetzeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v) {
        m_plaetze = v;
        m_mehrfach.setPlaetze(v);
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QString::number(v));
        if (m_status && v > 1) {
            // Die Zahl, die man wissen muss, bevor man sendet.
            const double dB = -10.0 * std::log10(double(v));
            m_status->setText(tr("%1 slots \u2014 %2 dB each")
                                  .arg(v).arg(dB, 0, 'f', 1));
        }
    });
    connect(m_cqVersucheSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int v) {
        AppSettings::instance().setValue(QStringLiteral("Ft8/CqVersuche"),
                                         QString::number(v));
        m_ablauf.setzeAnrufWiederholungen(v);
        m_mehrfach.setzeAnrufWiederholungen(v);
    });
    sendRow->addWidget(m_cqVersucheSpin);
    sendRow->addWidget(m_plaetzeSpin);
    sendRow->addWidget(m_freieStelleBtn);

    m_cqBtn = blueToggle(tr("Send"), 56);
    m_cqBtn->setToolTip(tr("Send this message once, in the next own "
                           "period. No auto-sequencing."));
    connect(m_cqBtn, &QPushButton::clicked, this, [this]() {
        sendeNachricht(m_textFeld ? m_textFeld->text() : QString());
    });
    sendRow->addWidget(m_cqBtn);

    m_stopBtn = styledButton(tr("Stop"), 48);
    m_stopBtn->setCheckable(false);    // haelt an, rastet nicht ein
    m_stopBtn->setToolTip(tr("Stop transmitting and drop the QSO "
                             "sequence."));
    m_stopBtn->setEnabled(false);
    // „Stop" heisst Stop. abbrechen() allein nimmt nur das laufende
    // Fenster zurueck — der Ablauf blieb stehen und waere beim
    // naechsten passenden Fund von selbst weitergelaufen. Seit der
    // Doppelklick den Ablauf immer startet, ist das der einzige Weg
    // hinaus, und er muss halten.
    connect(m_stopBtn, &QPushButton::clicked, this, [this]() {
        // Stop heiszt Stop: auch der Wiederanlauf nach einem QSO --
        // und auch die Plaetze.
        //
        // `m_mehrfach.abbrechen()` fehlte (Durchsicht 2026-10-30).
        // Ohne das liefen die Plaetze weiter: `fensterZuEnde` sah
        // `aktive() > 0`, sendete und kam gar nicht erst zu dem
        // angehaltenen Einzelablauf. Ein Stop, der nicht stoppt, ist
        // schlimmer als kein Stop -- man drueckt ihn und glaubt, es
        // sei vorbei.
        m_cqAutomatik = false;
        m_mehrfach.abbrechen();
        m_ablauf.abbrechen();
        m_cqVersucheOffen = 0;
        m_festerHinweis.clear();
        abbrechen();
        zeigeAblauf();
    });
    sendRow->addWidget(m_stopBtn);

    root->addLayout(sendRow);

    // ── Keine Fusszeile mehr (30.10.) ──────────────────
    //
    // Dort stand „last period: 23", waehrend rechts neben den Pillen
    // „23 decodes" steht -- dieselbe Zahl, zweimal, am laufenden
    // Geraet nebeneinander im Bild. „Clear" gehoert ohnehin neben die
    // Liste, die es leert.
}

void Ft8Applet::setProtocol(Ft8Decoder::Protocol p)
{
    // ── Die gehoerten Tonlagen gelten nicht mehr (2026-10-30) ───
    //
    // Der Wechsel FT8 <-> FT4 zieht die Scheibe auf eine andere
    // Frequenz (7.074 -> 7.0475 MHz auf 40 m). Eine Tonlage ist aber
    // relativ zur Scheibe: was eben noch bei 1500 Hz lag, liegt jetzt
    // ganz woanders. Die Stellensuche wuerde sonst auf Zahlen
    // arbeiten, die nichts mehr bedeuten.
    m_toeneJePeriode.clear();

    // Ein echter Wechsel, nicht jedes Setzen: beim Aufbau und aus
    // `folgeDerSkala` kommt auch das Verfahren herein, das schon gilt.
    const bool wechsel = m_stream && m_stream->protocol() != p;

    if (m_stream) { m_stream->setProtocol(p); }
    const bool ft4 = (p == Ft8Decoder::Protocol::Ft4);
    if (m_ft8Btn) { m_ft8Btn->setChecked(!ft4); }
    if (m_ft4Btn) { m_ft4Btn->setChecked(ft4); }
    m_lastPeriodCount = -1;
    if (wechsel) {
        leereFuerNeuenOrt(ft4 ? QStringLiteral("FT4") : QStringLiteral("FT8"));
    }
    updateStatus();
}

// ── Neuer Ort, leere Liste (2026-11-17) ──────────────────────────────
//
// Betreiber: „Generell sollte bei einem Bandwechsel von FT4 auf FT8 …
// alle calls vom vorigen band gelöscht werden."
//
// Die Liste blieb ueber jeden Wechsel stehen. Nach 15 m -> 17 m standen
// die Stationen von 15 m weiter da, und nichts an ihnen sagte, dass
// sie hier nicht mehr zu hoeren sind: ein Doppelklick rief ins Leere.
// Dasselbe zwischen FT8 und FT4 -- die Skala steht dann woanders, und
// wer eben FT8 machte, hoert einen FT4-Anruf nicht.
//
// Mit der Liste geht die Schlange: wer wartet, hat auf dem ALTEN Band
// gerufen. Und ihre Karenz -- wer auf 15 m eben gearbeitet wurde, ist
// auf 17 m ein neues QSO und darf gleich wieder.
//
// Den laufenden Ablauf haelt beim BANDwechsel `skalaBewegt` an (seit
// dem 2026-11-18, siehe dort); diese Funktion leert nur.
void Ft8Applet::leereFuerNeuenOrt(const QString& grund)
{
    qCInfo(lcFt8Senden).noquote()
        << "Liste geleert:" << m_zeilen.size() << "Zeilen,"
        << m_schlange.wartende(QDateTime::currentMSecsSinceEpoch())
        << "wartend — jetzt" << grund;
    m_zeilen.clear();
    m_toeneJePeriode.clear();
    m_schlange.leeren();
    zeichneListe();
    zeigeDann();
}

void Ft8Applet::skalaBewegt(double skalaHz)
{
    // `bandFuerLog`, nicht selbst gerechnet: es beachtet den
    // Transverter. Ausserhalb jedes Bandes (leer) gilt das alte weiter
    // -- ein Dreh ueber die Bandkante ist noch kein neuer Ort.
    const QString band = bandFuerLog(skalaHz);
    if (!band.isEmpty() && band != m_listenBand) {
        const bool erstesBand = m_listenBand.isEmpty();
        m_listenBand = band;
        if (!erstesBand) {
            // ── Ein Bandwechsel haelt ALLES an (2026-11-18) ──────────
            //
            // Betreiber: „sobald man einen bandwechsel durchführt dann
            // nicht mehr automatisch weiter gesendet werden!!!!"
            //
            // #475 hatte hier ausdruecklich nur die Liste geleert und
            // den Ablauf laufen lassen („die Sache von Stop, nicht
            // eines Skalendrehs"). Das war falsch gedacht: am Geraet
            // wechselte er von 20 m auf 30 m, der Lauf sendete dort
            // weiter -- an Stationen, die auf 20 m waren, in eine
            // Antenne mit Stehwelle 2,96. Erst der Riegel hielt ihn auf.
            //
            // Wer das Band wechselt, hat das QSO verlassen. Gesendet
            // wird auf dem neuen Band erst wieder, wenn er es sagt.
            // Erst anhalten, dann leeren: so geht auch eine Sendung,
            // die gerade laeuft, sofort vom Sender.
            if (m_sendet || m_ablauf.laeuft() || m_mehrfach.aktive() > 0
                || m_cqAutomatik) {
                haltAllesAn(tr("stopped — band changed to %1").arg(band));
            }
            leereFuerNeuenOrt(band);
        }
    }
    folgeDerSkala(skalaHz);
}

void Ft8Applet::haltAllesAn(const QString& grund)
{
    qCInfo(lcFt8Senden).noquote() << "Angehalten:" << grund;
    m_cqAutomatik = false;
    m_mehrfach.abbrechen();
    m_ablauf.abbrechen();
    m_cqVersucheOffen = 0;
    abbrechen();
    zeigeAblauf();
    // NACH zeigeAblauf(), das selbst in die Statuszeile schreibt.
    m_festerHinweis = grund;
    if (m_status) { m_status->setText(m_festerHinweis); }
}

// ── Eben gearbeitet, also nicht mehr blau (2026-11-18) ───────────────
//
// Betreiber: „die soeben geführten qso … sehe ich im verlauf noch immer
// blau. diese sollten sofort farblich geändert werden."
//
// Zwei Luecken auf einmal:
//   * Mit Plaetzen (`meldeFertigePlaetze`) wurde das Feld NIE gemerkt
//     -- nur der einzelne Ablauf rief `m_grids.merke`. Wer mit
//     „slots 3" arbeitet, behielt jedes gearbeitete Feld blau bis zum
//     Neustart.
//   * Und auch dort galt das Merken nur fuer KUENFTIGE Zeilen: was
//     schon in der Liste stand, trug seine Fahne `neuesGrid` weiter.
//
// Ohne Locator im Ablauf (er hat auf einen Rapport geklickt, nicht auf
// ein CQ) steht das Feld oft trotzdem in der Liste -- in einer Zeile,
// die der Partner selbst gesendet hat.
void Ft8Applet::feldGearbeitet(const QString& partner, const QString& locator)
{
    QString feld = locator.trimmed().toUpper();
    if (feld.isEmpty() && !partner.isEmpty()) {
        for (int i = m_zeilen.size() - 1; i >= 0; --i) {
            const Fundzeile& z = m_zeilen.at(i);
            if (z.eigeneSendung) { continue; }
            if (senderCallsign(z.nachricht).compare(partner,
                                                    Qt::CaseInsensitive) != 0) {
                continue;
            }
            const QString l = locatorAusNachricht(z.nachricht);
            if (!l.isEmpty()) { feld = l; break; }
        }
    }
    if (feld.isEmpty()) { return; }
    m_grids.merke(feld);

    bool geaendert = false;
    for (Fundzeile& z : m_zeilen) {
        if (!z.neuesGrid) { continue; }
        const QString l = locatorAusNachricht(z.nachricht);
        if (!l.isEmpty() && !m_grids.istNeu(l)) {
            z.neuesGrid = false;
            geaendert = true;
        }
    }
    if (geaendert) { zeichneListe(); }
}

Ft8Decoder::Protocol Ft8Applet::protocol() const
{
    return m_stream ? m_stream->protocol() : Ft8Decoder::Protocol::Ft8;
}

void Ft8Applet::setSlice(SliceModel* slice)
{
    if (m_slice == slice) { return; }
    m_slice = slice;
    QObject::disconnect(m_skalaVerbindung);
    if (m_slice) {
        m_skalaVerbindung = connect(m_slice.data(), &SliceModel::frequencyChanged,
                                    this, &Ft8Applet::skalaBewegt);
        skalaBewegt(m_slice->frequency());
    }
    updateAudioTap();
    updateStatus();
}

// ── Der Dekoder folgt der Skala (2026-11-15) ─────────────────────────
//
// Betreiber nach dem Start um 11:28: „Versuch es noch einmal" -- seit
// zwei Minuten nach dem Start 0 Funde in jedem Fenster. Die Sitzung
// davor hatte er um 10:17 mit dem Knopf FT4 auf 14,080 MHz gestellt
// und bis zuletzt FT4 dekodiert (bis zu 14 Funde je Fenster). Beim
// Neustart behielt die Flex ihre 14,080 MHz („die Maschine gewinnt"),
// der Dekoder aber kam -- wie immer -- in FT8 hoch und suchte
// FT8-Signale auf der FT4-Anrufstelle. Gesagt hat das niemand.
//
// Die Knoepfe FT8/FT4 stellen seit dem 2026-10-21 die Skala. Hier die
// Gegenrichtung: steht die Skala AUF einer Anrufstelle (±500 Hz,
// dieselbe Tafel), dekodiert der Dekoder deren Verfahren. Daneben --
// irgendwo im Band -- bleibt es bei der Wahl des Betreibers; das
// Funkgeraet wird hier nie verstellt. Die Tafel gilt fuer oberes
// Seitenband, also nicht in DIGL. Und nicht mitten in einer Sendung:
// der Wechsel aendert die Fensterlaenge.
void Ft8Applet::folgeDerSkala(double skalaHz)
{
    if (!m_slice || m_slice->dspMode() == DSPMode::DIGL || m_sendet) {
        return;
    }
    const qint64 hz = static_cast<qint64>(std::llround(skalaHz));
    const bool ft4 = DigiFrequenzen::stehtSchonDort(
        hz, DigiFrequenzen::Digimodus::Ft4);
    const bool ft8 = DigiFrequenzen::stehtSchonDort(
        hz, DigiFrequenzen::Digimodus::Ft8);
    if (ft4 == ft8) { return; }   // keine Anrufstelle: Wahl bleibt
    const Ft8Decoder::Protocol soll = ft4 ? Ft8Decoder::Protocol::Ft4
                                          : Ft8Decoder::Protocol::Ft8;
    if (protocol() == soll) { return; }
    const QString name = ft4 ? QStringLiteral("FT4") : QStringLiteral("FT8");
    qCInfo(lcFt8Senden).noquote()
        << "Skala" << hz << "Hz steht auf der" << name
        << "-Anrufstelle — der Dekoder folgt";
    setProtocol(soll);
    if (m_status) {
        m_status->setText(tr("dial is on the %1 frequency — decoding %1")
                              .arg(name));
    }
}

void Ft8Applet::updateAudioTap()
{
    if (!m_model || !m_model->audioEngine() || !m_tapRing) { return; }
    if (m_slice) {
        m_model->audioEngine()->setFt8Tap(m_tapRing.get(), m_slice->sliceIndex());
    } else {
        m_model->audioEngine()->setFt8Tap(nullptr, -1);
    }
}

void Ft8Applet::syncFromModel()
{
    setSlice(m_model ? m_model->activeSlice() : nullptr);
}

// ── Senden ──────────────────────────────────────────────────────────
//
// Der ganze Ablauf an einer Stelle, damit man ihn lesen kann, ohne zu
// springen: vorbereiten, warten bis das eigene Fenster beginnt,
// Mikrofonweg umlegen, tasten, Fenster laufen lassen, alles
// zuruecknehmen.
//
// Was absichtlich fehlt: jede Automatik. Ein Druck = EIN Fenster.
// ── Ein Fenster ist hinaus ───────────────────────────────────────────
//
// Ohne laufenden Ablauf ist danach Schluss, wie bisher. Mit Ablauf
// zaehlt das Fenster als „gesendet, keine Antwort" — und der naechste
// Schritt folgt, sobald ein Fund ihn weiterbringt oder die
// Wiederholung faellig ist.
void Ft8Applet::fensterZuEnde()
{
    abbrechen();

    // ── Mehrere Plaetze: jeder bekommt sein Fenster ──────────
    //
    // Dieser Block fehlte, bis er bei der Durchsicht am 2026-10-30
    // auffiel: eine Ersetzung war still fehlgeschlagen, und damit
    // drehte Multi Answering seine Plaetze NIE weiter und schrieb NIE
    // ein QSO ins Logbuch. Gesendet haette es, aber kein Zug waere je
    // vorangekommen.
    //
    // `fensterVerstrichen` reicht es an alle laufenden Ablaeufe weiter
    // und raeumt danach auf; `nimmFertige` liefert jedes fertige QSO
    // GENAU EINMAL -- darum geht es von hier direkt ins Logbuch und
    // nicht ueber `meldeFertigesQso`, das fuer den einzelnen Ablauf
    // gedacht ist und mit `m_qsoGemeldet` gegen Doppelmeldung sichert.
    if (mehrfachAn()) {
        m_mehrfach.fensterVerstrichen();
        const int fertig = meldeFertigePlaetze();

        // ── Wer unter „THEN" wartet, bekommt den freien Platz ────────
        //
        // Mit Plaetzen landen Anrufer direkt bei `m_mehrfach`; unter
        // „THEN" steht dann nur, wen der Betreiber von Hand angestellt
        // hat. Abgearbeitet wird es nach derselben Regel wie beim
        // einzelnen Ablauf: von selbst nur im Lauf vom CQ-Knopf.
        if (m_cqAutomatik) {
            while (!m_mehrfach.istVoll() && beginneNaechstenWartenden()) {}
        }

        // ── Nach einem fertigen QSO zaehlt der CQ-Ruf von vorn ───────
        //
        // Der CQ-Ruf laeuft als eigener Platz mit und zaehlt dabei
        // seine Versuche herunter -- auch waehrend QSOs laufen. Beim
        // einzelnen Ablauf faengt er nach jedem fertigen QSO neu an
        // (`zeigeAblauf`, 2026-10-30: „Der Durchlauf muss automatisch
        // ablaufen, so schnell wie moeglich ohne mein klicken"). Mit
        // Plaetzen fehlte das: war der Zaehler waehrend eines QSOs
        // abgelaufen, blieb es nach dem RR73 still.
        if (fertig > 0 && m_cqAutomatik) {
            qCInfo(lcFt8Senden).noquote()
                << fertig << "QSO(s) auf den Plaetzen fertig — der"
                   " CQ-Ruf zaehlt von vorn";
            if (m_mehrfach.aktive() == 0) {
                starteCqRuf();
                return;
            }
            frischeCqRufAuf();
        }
        if (m_mehrfach.aktive() > 0) {
            // Den CQ-Ruf MITDREHEN, nicht einfrieren: er geht als
            // eigener Platz mit hinaus (siehe `sendeMehrfach`), also
            // muss auch sein Zaehler laufen. Sonst stuende er nach
            // zehn abgearbeiteten Anrufern immer noch bei Versuch 1
            // und riefe bis zum Abend.
            if (m_ablauf.laeuft()
                && m_ablauf.zustand() == Ft8Ablauf::Zustand::RufeCq) {
                if (m_cqVersucheOffen > 0) { --m_cqVersucheOffen; }
                m_ablauf.fensterVerstrichen();
                if (m_cqVersucheOffen == 0) { m_ablauf.abbrechen(); }
            }
            sendeMehrfach();
            return;
        }
        // Kein Platz mehr belegt: zurueck in den gewoehnlichen Weg,
        // damit ein laufender CQ-Ruf weitergeht.
    }
    if (!m_ablauf.laeuft()) {
        m_cqVersucheOffen = 0;
        zeigeAblauf();
        return;
    }

    // Ein laufender CQ-Ruf: noch einmal, solange Versuche offen sind.
    // Antwortet jemand, uebernimmt der Ablauf von selbst -- dann steht
    // er nicht mehr im Anruferzustand, und diese Stelle greift nicht.
    if (m_cqVersucheOffen > 0) {
        --m_cqVersucheOffen;
        if (m_cqVersucheOffen == 0
            && m_ablauf.zustand() == Ft8Ablauf::Zustand::RufeCq) {
            m_ablauf.abbrechen();
            if (m_status) {
                m_status->setText(tr("CQ finished — nobody answered"));
            }
            zeigeAblauf();
            return;
        }
    }
    m_ablauf.fensterVerstrichen();
    if (m_ablauf.laeuft()) {
        ablaufSchritt();
    } else {
        zeigeAblauf();
    }
}

// Den naechsten Zug des Ablaufs senden. Es ist derselbe Weg wie bei
// einem Druck auf „Send": Bandplan und Tasten inbegriffen.
// ── Einen CQ-Ruf anfangen ───────────────────────────────
//
// Eine Stelle, nicht zwei: der Knopf ruft sie, und nach einem
// fertigen QSO ruft `zeigeAblauf` sie wieder. Als es zwei Kopien gab,
// hat eine davon `setzeCqWiederholungen` vergessen -- und der Ruf
// brach nach fuenf statt nach den eingestellten sechzehn ab
// (2026-10-29).
// ── Wer gerufen hat, kommt dran ──────────────────────────────────────
//
// Beide Haelften der Warteschlange haengen hier. Sie gelten AUCH mit
// einem Platz -- `Ft8Mehrfach` hat eine eigene Schlange, aber die
// laeuft nur, wenn `mehrfachAn()` an ist.

QString Ft8Applet::anruferAus(const QString& nachricht) const
{
    const QString meiner = eigenesRufzeichen();
    if (meiner.isEmpty()) { return {}; }

    const QStringList f = nachricht.simplified().toUpper()
                              .split(QLatin1Char(' '), Qt::SkipEmptyParts);
    // An mich gerichtet heisst: mein Rufzeichen steht VORNE. Steht es
    // weiter hinten, bin ich darin nur erwaehnt (etwa im Schluss eines
    // Fox: „K1ABC RR73; W9XYZ <KH1DX> -17").
    if (f.size() < 2) { return {}; }
    // Die spitzen Klammern des Contest-Austauschs gehoeren nicht zum
    // Rufzeichen -- dieselbe Regel wie in `Ft8Ablauf::anMich`.
    QString anWen = f.at(0);
    anWen.remove(QLatin1Char('<')).remove(QLatin1Char('>'));
    if (anWen != meiner) { return {}; }

    QString anrufer = f.at(1);
    anrufer.remove(QLatin1Char('<')).remove(QLatin1Char('>'));
    if (anrufer.isEmpty() || anrufer == meiner) { return {}; }
    // Der Schluss eines Fox an mich („OE5SOS RR73; …") traegt an
    // zweiter Stelle kein Rufzeichen.
    if (anrufer.startsWith(QStringLiteral("RR73"))
        || Ft8Ablauf::istAbschied(anrufer)) {
        return {};
    }

    // Ein Abschied ist kein Anruf -- dieselbe Regel wie in
    // `Ft8Ablauf::verarbeite`, und darum auch dieselbe Funktion.
    if (f.size() >= 3 && Ft8Ablauf::istAbschied(f.at(2))) { return {}; }
    return anrufer;
}

bool Ft8Applet::nimmAnruferAufPlatz(const QString& nachricht, int snr)
{
    const QString anrufer = anruferAus(nachricht);
    if (anrufer.isEmpty() || m_mehrfach.kennt(anrufer)) { return false; }

    // Dieselben zwei Riegel wie in `Ft8Warteschlange::merkeAnruf`: wer
    // eben gearbeitet wurde, wird nicht gleich noch einmal gearbeitet,
    // und wen der Betreiber abgewiesen hat (✕ unter „THEN"), der
    // bleibt es fuer seine Frist.
    const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
    if (m_schlange.inKarenz(anrufer, jetzt)
        || m_schlange.abgewiesen(anrufer, jetzt)) {
        return false;
    }
    if (!m_mehrfach.nimmAnrufer(anrufer, snr,
                                locatorAusNachricht(nachricht))) {
        return false;
    }
    // Stand er von Hand unter „THEN", ist er jetzt dran.
    m_schlange.vergiss(anrufer);
    qCInfo(lcFt8Senden).noquote()
        << "Anruf auf einen Platz genommen:" << anrufer
        << "—" << m_mehrfach.aktive() << "belegt,"
        << m_mehrfach.wartende() << "wartend — aus:" << nachricht;
    return true;
}

int Ft8Applet::meldeFertigePlaetze()
{
    m_mehrfach.raeumeAuf();
    const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
    int zahl = 0;
    // `nimmFertige` liefert jedes fertige QSO GENAU EINMAL -- darum
    // geht es von hier direkt ins Logbuch und nicht ueber
    // `meldeFertigesQso`, das fuer den einzelnen Ablauf gedacht ist
    // und mit `m_qsoGemeldet` gegen Doppelmeldung sichert.
    for (const Ft8Ablauf& fertig : m_mehrfach.nimmFertige()) {
        emit qsoFertig(logEintrag(
            fertig, m_slice ? m_slice->frequency() : 0.0,
            m_slice && m_slice->dspMode() == DSPMode::DIGL,
            protocol(), eigenerLocator(), m_sendeTonHz));
        // In die Karenz -- derselbe zweite Riegel wie beim einzelnen
        // Ablauf (`zeigeAblauf`, 2026-10-30, HB9ZHK). Ohne ihn macht
        // sein naechster Satz an mich ihn wieder zum Anrufer.
        m_schlange.merkeGearbeitet(fertig.partner(), jetzt);
        feldGearbeitet(fertig.partner(), fertig.partnerLocator());
        qCInfo(lcFt8Senden).noquote()
            << "QSO fertig (Platz):" << fertig.partner();
        ++zahl;
    }
    return zahl;
}

void Ft8Applet::merkeAnrufFallsNoetig(const QString& nachricht, int snr,
                                      int tonHz)
{
    const QString anrufer = anruferAus(nachricht);
    if (anrufer.isEmpty()) { return; }

    // Mein aktueller Partner steht nicht in der Schlange: mit ihm
    // laeuft gerade ein QSO.
    if (!m_ablauf.partner().isEmpty() && anrufer == m_ablauf.partner()) {
        return;
    }
    if (mehrfachAn() && m_mehrfach.kennt(anrufer)) { return; }

    const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
    if (m_schlange.merkeAnruf(anrufer, snr,
                              locatorAusNachricht(nachricht), jetzt, tonHz)) {
        qCInfo(lcFt8Senden).noquote()
            << "Anruf gemerkt:" << anrufer << "— wartet als Nummer"
            << m_schlange.wartende(jetzt) << "— aus:" << nachricht;
        // Sofort sichtbar machen, nicht erst beim naechsten
        // Zustandswechsel: wer ruft, soll auf der Stelle im Kopf der
        // Liste stehen.
        zeigeFundZahl();
        meldeStand();
    }
}

// Den naechsten Wartenden ANFANGEN, ohne zu senden.
//
// Getrennt von `rufeNaechstenWartenden`, weil es einen zweiten
// Aufrufer gibt, der schon mitten in einem Sendefenster steht und den
// Zug selbst packt (siehe die Dekoderfrist in `sendeNachricht`). Der
// darf hier nicht `ablaufSchritt()` ausloesen -- das wuerde mitten im
// laufenden Fenster ein zweites anfangen.
bool Ft8Applet::beginneNaechstenWartenden()
{
    const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
    const Ft8Warteschlange::Wartend w = m_schlange.nimmNaechsten(jetzt);
    if (w.partner.isEmpty()) { return false; }

    m_qsoGemeldet = false;
    // Er hat MICH gerufen, also beginnt es mit dem Rapport.
    // ── Vor JEDEM QSO-Start den Contest-Stand nachziehen ────────
    //
    // Sonst traegt der Ablauf die Nummer, die beim Einschalten des
    // Schalters galt -- und nach dem ersten QSO, nach einem
    // Bandwechsel oder nach einem Neustart waere sie falsch. Die
    // vier Startstellen tun es alle; eine davon zu vergessen hiesse,
    // dass genau DIESER Weg eine alte Nummer sendet, und das faellt
    // erst beim Logabgleich auf.
    uebernimmContest();
    if (mehrfachAn()) {
        // Mit Plaetzen ist `m_ablauf` nur der CQ-Ruf -- ein QSO dort
        // bekaeme keinen einzigen Fund zu sehen (2026-11-17). Also auf
        // einen Platz, mit dem Zug, der zu ihm gehoert.
        m_mehrfach.setEigeneDaten(eigenesRufzeichen(), eigenerLocator());
        const bool genommen = w.vonHand
            ? m_mehrfach.nimmVonHand(w.partner, w.snr, w.locator)
            : m_mehrfach.nimmAnrufer(w.partner, w.snr, w.locator);
        qCInfo(lcFt8Senden).noquote()
            << "Aus der Schlange auf einen Platz:" << w.partner
            << (w.vonHand ? "(von Hand)" : "")
            << (genommen ? "" : "— war schon dabei") << "— noch"
            << m_schlange.wartende(jetzt) << "wartend";
        return genommen;
    }
    if (w.vonHand) {
        // Aus der Fundliste angestellt (2026-11-15, „+ Queue"): er hat
        // MICH nicht gerufen. Also derselbe Anfang wie ein Doppelklick
        // auf seine Zeile -- mein Ruf mit Locator, nicht ein Rapport an
        // jemanden, der nie gefragt hat. Nur ohne `m_cqAutomatik`
        // anzufassen: er ist Teil des Laufs, keine Einzelentscheidung.
        if (m_houndBetrieb) {
            m_ablauf.starteAlsHound(w.partner, eigenesRufzeichen(),
                                    eigenerLocator());
        } else {
            m_ablauf.starteAlsAntworter(w.partner, eigenesRufzeichen(),
                                        eigenerLocator(), w.snr, w.locator);
        }
    } else {
        m_ablauf.starteMitAnrufer(w.partner, eigenesRufzeichen(),
                                  eigenerLocator(), w.snr, w.locator);
    }
    qCInfo(lcFt8Senden).noquote()
        << "Aus der Schlange:" << w.partner
        << (w.vonHand ? "(von Hand)" : "") << "— noch"
        << m_schlange.wartende(jetzt) << "wartend";
    return true;
}

bool Ft8Applet::rufeNaechstenWartenden()
{
    if (m_sendet) { return false; }
    if (!beginneNaechstenWartenden()) { return false; }
    ablaufSchritt();
    return true;
}

// ── „THEN" zeichnen (2026-11-15) ─────────────────────────────────────
//
// Aus `meldeStand()` gerufen -- also bei jedem Zustandswechsel des
// Ablaufs und bei jedem neuen Anruf -- und jede Sekunde vom eigenen
// Takt, solange jemand wartet: sonst stuende das Alter still, und ein
// Anruf, der eben verfaellt, saehe frisch aus.
void Ft8Applet::zeigeDann()
{
    if (!m_dann) { return; }
    const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
    QVector<Ft8DannListe::Eintrag> liste;
    for (const Ft8Warteschlange::Wartend& w : m_schlange.gueltige(jetzt)) {
        Ft8DannListe::Eintrag e;
        e.partner = w.partner;
        e.land    = landFuer(w.partner);
        e.snr     = w.snr;
        e.locator = w.locator;
        e.tonHz   = w.tonHz;
        e.alterMs = jetzt - w.seitMs;
        e.vonHand = w.vonHand;
        liste.append(e);
    }
    m_dann->setzeEintraege(liste, m_cqAutomatik);
    if (m_dannUhr) {
        if (liste.isEmpty()) {
            m_dannUhr->stop();
        } else if (!m_dannUhr->isActive()) {
            m_dannUhr->start();
        }
    }
}

// Doppelklick auf eine Zeile unter „THEN". Laeuft ein QSO, kommt er
// als naechster dran -- mitten in ein QSO hineinzuspringen hiesse, den
// Partner im Stich zu lassen. Sonst sofort, auf demselben Weg wie
// nach einem fertigen QSO.
void Ft8Applet::dannGewaehlt(const QString& partner)
{
    const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
    const bool imQso = m_ablauf.laeuft() && !m_ablauf.partner().isEmpty();
    m_schlange.nachVorn(partner, jetzt);
    if (imQso) {
        if (m_status) {
            m_status->setText(tr("%1 is next after this QSO").arg(partner));
        }
        meldeStand();
        return;
    }
    // Wie ein Doppelklick in der Fundliste (siehe nimmFundAn): der
    // Ablauf wird auch waehrend einer Sendung gesetzt, und
    // `ablaufSchritt()` kehrt dann von selbst um.
    if (beginneNaechstenWartenden()) {
        ablaufSchritt();
    }
    zeigeFundZahl();
    meldeStand();
}

QString Ft8Applet::warumNichtAnstellen(const QString& zeile) const
{
    const QString meiner = eigenesRufzeichen();
    if (meiner.isEmpty()) { return tr("set your callsign first"); }
    if (antwortAuf(zeile, meiner, eigenerLocator()).isEmpty()) {
        return tr("no callsign to answer in this line");
    }
    const QString wer = senderCallsign(nachrichtAusZeile(zeile));
    if (wer.isEmpty() || wer.compare(meiner, Qt::CaseInsensitive) == 0) {
        return tr("no callsign to answer in this line");
    }
    if (m_ablauf.laeuft()
        && m_ablauf.partner().compare(wer, Qt::CaseInsensitive) == 0) {
        return tr("%1 is your QSO right now").arg(wer);
    }
    const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
    for (const Ft8Warteschlange::Wartend& w : m_schlange.gueltige(jetzt)) {
        if (w.partner.compare(wer, Qt::CaseInsensitive) == 0) {
            return tr("%1 is already waiting").arg(wer);
        }
    }
    if (m_schlange.inKarenz(wer, jetzt)) {
        return tr("%1 was worked a few minutes ago").arg(wer);
    }
    return {};
}

// „+ Queue": eine Fundzeile anstellen. Ruft er MICH, ist er ein
// Anrufer wie jeder andere (Rapport zuerst); sonst rufe ich ihn, wenn
// er drankommt.
QString Ft8Applet::stelleZeileAn(const QString& zeile)
{
    const QString grund = warumNichtAnstellen(zeile);
    if (!grund.isEmpty()) { return grund; }

    const QString nachricht = nachrichtAusZeile(zeile);
    const QString sauber    = zeile.simplified();
    const int snr   = sauber.section(QLatin1Char(' '), 1, 1).toInt();
    bool hzOk = false;
    const int tonHz = sauber.section(QLatin1Char(' '), 3, 3).toInt(&hzOk);
    const QString wer = senderCallsign(nachricht);
    const QStringList f = nachricht.toUpper()
                              .split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const bool ruftMich = f.size() >= 2 && f.first() == eigenesRufzeichen();

    const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
    if (!m_schlange.stelleAn(wer, snr, locatorAusNachricht(nachricht),
                             hzOk ? tonHz : -1, jetzt, !ruftMich)) {
        return tr("%1 could not be queued").arg(wer);
    }
    qCInfo(lcFt8Senden).noquote()
        << "Von Hand angestellt:" << wer << (ruftMich ? "(ruft mich)" : "")
        << "— aus:" << nachricht;
    if (m_status) {
        m_status->setText(tr("%1 queued").arg(wer));
    }
    zeigeFundZahl();
    meldeStand();
    return {};
}

// Die Kapsel an die Zeile unter der Maus haengen -- oder weg, wenn
// sich die Zeile nicht anstellen laesst. Dieselbe Pruefung, die der
// Klick spaeter noch einmal macht; hier nur, damit sie gar nicht erst
// an einer Zeile erscheint, bei der sie nichts taete.
void Ft8Applet::zeigePilleBei(const QPoint& pos)
{
    if (!m_list || !m_queuePille) { return; }
    const QTextCursor c = m_list->cursorForPosition(pos);
    const QTextBlock b = c.block();
    if (!b.isValid()) { versteckePille(); return; }
    // cursorForPosition liefert unter der letzten Zeile noch die letzte
    // -- die Maus steht dort aber auf keiner Zeile.
    const QRect r = m_list->cursorRect(QTextCursor(b));
    if (pos.y() < r.top() || pos.y() > r.bottom()) { versteckePille(); return; }
    if (b.blockNumber() == m_pilleBlock && m_queuePille->isVisible()) {
        return;
    }
    const QString zeile = b.text();
    if (!warumNichtAnstellen(zeile).isEmpty()) { versteckePille(); return; }

    m_pilleZeile = zeile;
    m_pilleBlock = b.blockNumber();
    m_queuePille->adjustSize();
    QWidget* vp = m_list->viewport();
    m_queuePille->move(vp->width() - m_queuePille->width() - 4,
                       r.center().y() - m_queuePille->height() / 2);
    m_queuePille->show();
    m_queuePille->raise();

    // Die Zeile selbst leicht anheben: man soll sehen, WELCHE gemeint
    // ist, bevor man klickt.
    QTextEdit::ExtraSelection sel;
    sel.cursor = QTextCursor(b);
    sel.format.setBackground(QColor(QString::fromLatin1(Style::kTileActiveBg)));
    sel.format.setProperty(QTextFormat::FullWidthSelection, true);
    m_list->setExtraSelections({sel});
}

void Ft8Applet::versteckePille()
{
    if (m_queuePille) { m_queuePille->hide(); }
    if (m_list && m_pilleBlock >= 0) { m_list->setExtraSelections({}); }
    m_pilleBlock = -1;
    m_pilleZeile.clear();
}

void Ft8Applet::starteCqRuf()
{
    beginneCqRuf();
    ablaufSchritt();
    zeigeKnopfzustand();
}

void Ft8Applet::beginneCqRuf()
{
    // ── Nicht in ein laufendes QSO hineinrufen (2026-10-30) ─────
    //
    // Nur beim ANFANG eines Rufs, und nur wenn die Stelle wirklich
    // belegt ist. Mitten im QSO zu wechseln waere schlimmer als
    // jede belegte Stelle: wer antwortet, sucht einen dort, wo er
    // zuletzt war.
    sucheFreieStelle(true);

    m_qsoGemeldet = false;
    uebernimmContest();   // siehe `starteMitAnrufer` oben
    m_ablauf.starteAlsAnrufer(eigenesRufzeichen(), eigenerLocator());
    m_cqVersucheOffen = m_cqVersucheSpin ? m_cqVersucheSpin->value() : 1;

    // Dieselbe Zahl auch dem Ablauf sagen. Ohne diese Zeile zaehlte
    // er fuer sich bis fuenf und brach ab, waehrend hier noch elf
    // Versuche offen standen (gemessen am 2026-10-29, 12:47:12).
    m_ablauf.setzeCqWiederholungen(m_cqVersucheOffen);
}

// Den CQ-Ruf von vorn zaehlen lassen, OHNE zu senden: nach einem
// fertigen QSO auf einem Platz. Laufen noch andere Plaetze, bleibt die
// Tonlage, wo sie ist -- mitten im QSO zu wechseln waere schlimmer als
// jede belegte Stelle (siehe `beginneCqRuf`).
void Ft8Applet::frischeCqRufAuf()
{
    if (m_mehrfach.aktive() == 0) {
        beginneCqRuf();
        return;
    }
    uebernimmContest();
    m_ablauf.starteAlsAnrufer(eigenesRufzeichen(), eigenerLocator());
    m_cqVersucheOffen = m_cqVersucheSpin ? m_cqVersucheSpin->value() : 1;
    m_ablauf.setzeCqWiederholungen(m_cqVersucheOffen);
}

void Ft8Applet::ablaufSchritt()
{
    // Mit mehreren Plaetzen entscheidet `Ft8Mehrfach`, was hinausgeht
    // -- der einzelne Ablauf ist dann nur noch einer von mehreren.
    if (mehrfachAn() && m_mehrfach.aktive() > 0) {
        sendeMehrfach();
        return;
    }
    const QString was = m_ablauf.naechsteNachricht();
    zeigeAblauf();
    if (was.isEmpty() || m_sendet) { return; }
    if (m_textFeld) { m_textFeld->setText(was); }
    sendeNachricht(was);
}

// ── Wo gerade jemand liegt ───────────────────────────────────────────
QVector<int> Ft8Applet::belegteToene() const
{
    QVector<int> alle;
    for (const QVector<int>& periode : m_toeneJePeriode) {
        alle += periode;
    }
    return alle;
}

int Ft8Applet::tonUntergrenze() const
{
    // Ein Hound unter 1000 Hz wird nicht beantwortet -- dann darf die
    // Stellensuche ihn auch nicht dorthin schicken.
    return m_houndBetrieb ? Ft8Ablauf::kHoundMinTonHz : kSendeTonMinHz;
}

// ── Eine freie Sendestelle suchen ────────────────────────────────────
//
// Weder WSJT-X noch MSHV tun das. Longpath weiss aus den eigenen
// Funden, WO Stationen senden -- das ist eine bessere Auskunft als
// Energie im Wasserfall, denn sie sagt „hier sendet jemand" und nicht
// „hier ist Rauschen".
bool Ft8Applet::sucheFreieStelle(bool nurWennBelegt)
{
    if (!m_tonFeld) { return false; }
    const QVector<int> belegt = belegteToene();
    const int von  = tonUntergrenze();
    // Die SUCHE geht nicht an den oberen Rand (2026-11-18): dort ist
    // es frei, weil dort kaum jemand hoert. Von Hand darf der
    // Betreiber weiter bis kSendeTonMaxHz stellen.
    const int bis  = std::min(kSendeTonMaxHz,
                              int(Ft8Mehrfach::kPlatzObergrenzeHz));
    const int alt  = m_tonFeld->value();

    if (belegt.isEmpty()) {
        // Nichts gehoert, nichts zu wissen. Abwesenheit ist keine
        // Aussage: wer hier verschoebe, zoege eine Station von einer
        // vielleicht guten Stelle weg, weil der Empfang gerade
        // schweigt.
        if (m_status && !nurWennBelegt) {
            m_status->setText(tr("nothing heard yet — tone unchanged"));
        }
        return false;
    }

    const int abstand = int(Ft8TxSource::kPlatzAbstandHz);
    if (nurWennBelegt && !stelleBelegt(belegt, alt, abstand)) {
        return false;
    }

    const int neu = freieTonlage(belegt, von, bis, abstand, alt);
    if (neu == alt) {
        if (m_status) {
            m_status->setText(stelleBelegt(belegt, alt, abstand)
                                  ? tr("no free slot — tone unchanged")
                                  : tr("%1 Hz is clear").arg(alt));
        }
        return false;
    }

    m_tonFeld->setValue(neu);
    qCInfo(lcFt8Senden).nospace()
        << "Freie Stelle gesucht: " << alt << " war belegt, jetzt "
        << neu << " Hz (" << belegt.size() << " Stationen gehoert)";
    if (m_status) {
        m_status->setText(tr("tone %1 \u2192 %2 Hz (was busy)")
                              .arg(alt).arg(neu));
    }
    return true;
}

// ── Ein Fenster, mehrere Stationen ─────────────────────
void Ft8Applet::sendeMehrfach()
{
    if (m_sendet) { return; }
    const QVector<Ft8TxSource::Platz> plaetze = mehrfachPlaetze();
    if (plaetze.isEmpty()) { return; }

    QStringList zeilen;
    for (const Ft8TxSource::Platz& p : plaetze) { zeilen << p.text; }
    const QString anzeige = zeilen.join(QStringLiteral(" | "));
    if (m_textFeld) { m_textFeld->setText(anzeige); }
    zeigeArbeit();
    sendeNachricht(anzeige, plaetze);
}

// ── Was JETZT auf allen Plaetzen hinausginge (2026-11-17) ────────────
//
// Aus `sendeMehrfach` herausgezogen, weil es zwei Fragesteller gibt:
// das Packen gleich nach einer Sendung, und die Nachpruefung kurz vor
// der naechsten (siehe `sendeNachricht`). Zwischen beiden liegen
// knapp 13 s und ein ganzes Empfangsfenster -- wer dort nur einmal
// fragt, sendet den Stand von vorhin.
QVector<Ft8TxSource::Platz> Ft8Applet::mehrfachPlaetze()
{
    m_mehrfach.setEigeneDaten(eigenesRufzeichen(), eigenerLocator());
    m_mehrfach.setGrundtonHz(float(m_sendeTonHz));
    QVector<Ft8TxSource::Platz> plaetze = m_mehrfach.naechsteNachrichten();

    // ── Der CQ-Ruf ist ein Platz, kein zweiter Ablauf ─────────
    //
    // Bei der Durchsicht am 2026-10-30 gefunden: mit Plaetzen liefen
    // ZWEI Zustandsmaschinen nebeneinander. Der CQ-Knopf startet
    // `m_ablauf` als Anrufer; antwortet dann jemand, nimmt ihn
    // `m_mehrfach` auf -- und von da an bediente `fensterZuEnde` nur
    // noch die Plaetze. Rufen UND Abarbeiten gleichzeitig ist aber
    // genau der Sinn der Sache; MSHV haelt dafuer einen Platz frei.
    //
    // Solange der Ruf laeuft und ein Platz frei ist, geht er als
    // eigener Platz mit hinaus. Ist alles belegt, schweigt das CQ --
    // wer sechs Stationen gleichzeitig abarbeitet, braucht keine
    // siebte.
    if (m_ablauf.laeuft()
        && m_ablauf.zustand() == Ft8Ablauf::Zustand::RufeCq
        && plaetze.size() < m_plaetze) {
        const QString cq = m_ablauf.naechsteNachricht();
        if (!cq.isEmpty()) {
            // Die erste Tonlage der Leiter, die KEIN Platz benutzt.
            //
            // Nicht `toene[plaetze.size()]`: `Ft8Mehrfach` vergibt
            // seine Toene nach dem Index im eigenen Feld, und das
            // enthaelt nur die aktiven Ablaeufe. Faellt einer von
            // ihnen mit leerer Nachricht heraus, stimmt die Zahl der
            // Eintraege nicht mehr mit den vergebenen Indizes
            // ueberein -- und das CQ laege auf derselben Tonlage wie
            // eine laufende Station. Zwei Nachrichten auf einem Ton
            // ueberlagern sich zu Matsch, und beide sind weg.
            // Dieselbe Leiter wie die Plaetze selbst (seit dem
            // 2026-11-18 mit Richtung, siehe `Ft8Mehrfach::tonleiter`)
            // -- sonst laege das CQ ueber dem Grundton, waehrend die
            // Plaetze darunter stapeln.
            const QVector<float> toene =
                m_mehrfach.tonleiter(Ft8Mehrfach::kMaxPlaetze);
            float frei = 0.0f;
            for (const float t : toene) {
                bool belegtVonPlatz = false;
                for (const Ft8TxSource::Platz& p : plaetze) {
                    if (qFuzzyCompare(p.toneHz + 1.0f, t + 1.0f)) {
                        belegtVonPlatz = true;
                        break;
                    }
                }
                if (!belegtVonPlatz) { frei = t; break; }
            }
            if (frei > 0.0f) {
                Ft8TxSource::Platz p;
                p.text   = cq;
                p.toneHz = frei;
                plaetze.push_back(p);
            }
        }
    }

    return plaetze;
}

// ── Ein fertiges QSO ins Logbuch ─────────────────────────────────────
//
// Nicht selbst schreiben: dasselbe Signal wie bei einem QSO aus
// WSJT-X, und MainWindow haengt es an RotorLogbookPanel::
// logExternalQso. Dort sitzt die Doppelt-Regel, das Anhaengen an die
// Datei und das Hochladen — alles einmal, nicht zweimal.
LogEntry Ft8Applet::logEintrag(const Ft8Ablauf& ablauf,
                               double wahlHz,
                               bool unteresSeitenband,
                               Ft8Decoder::Protocol p,
                               const QString& meinLocator,
                               int sendeTonHz)
{
    LogEntry e;
    e.call   = ablauf.partner();
    e.timeOn = QDateTime::currentDateTimeUtc();

    // Dieselbe Rechnung wie fuer die Funde: die Wahlfrequenz der
    // Scheibe plus die Tonlage, und das Vorzeichen haengt am
    // Seitenband. Gesendet wird auf der gewaehlten Tonlage.
    const double hz = wahlHz + (unteresSeitenband ? -double(sendeTonHz)
                                                  : double(sendeTonHz));
    e.freqMHz = hz / 1.0e6;
    e.band    = bandLabel(bandFromFrequency(hz));

    // ── Transverter (2026-10-28) ─────────────────────────────
    //
    // Steht einer davor, traegt der Satz die Frequenz der ANTENNE
    // und deren ADIF-Bandnamen. Ohne Transverter gibt
    // logbuchFrequenz() false zurueck und die zwei Zeilen darueber
    // bleiben, wie sie waren.
    TransverterSetup::logbuchFrequenz(std::llround(hz), &e.freqMHz, &e.band);

    // ADIF kennt FT8 und FT4 als eigene Betriebsarten — kein SUBMODE.
    e.mode = p == Ft8Decoder::Protocol::Ft4
                 ? QStringLiteral("FT4") : QStringLiteral("FT8");

    // In FT8 ist der Rapport die Signalstaerke in dB, nicht RST. So
    // schreibt WSJT-X ihn auch: „-12", „+03".
    const auto alsText = [](int r) {
        return r == Ft8Ablauf::kKeinReport
                   ? QString()
                   : QStringLiteral("%1%2")
                         .arg(r < 0 ? QLatin1Char('-') : QLatin1Char('+'))
                         .arg(qAbs(r), 2, 10, QLatin1Char('0'));
    };
    e.rstSent = alsText(ablauf.gesendeterReport());
    e.rstRcvd = alsText(ablauf.empfangenerReport());

    e.gridSquare   = ablauf.partnerLocator();   // leer, wenn nie einer kam
    e.myGridSquare = meinLocator;
    return e;
}

void Ft8Applet::fundForTest(const Ft8Decode& d)
{
    onDecoded({d}, QDateTime::currentDateTimeUtc());
}

void Ft8Applet::periodeForTest(const QVector<Ft8Decode>& funde)
{
    onDecoded(funde, QDateTime::currentDateTimeUtc());
}

void Ft8Applet::meldeFertigesQso()
{
    if (m_qsoGemeldet || m_ablauf.partner().isEmpty()) { return; }
    m_qsoGemeldet = true;
    // Das Feld ist ab jetzt gearbeitet -- ohne auf das naechste
    // Lesen der Datei zu warten. Sonst bliebe die Station, die eben
    // im Logbuch landete, bis zum Neustart blau.
    feldGearbeitet(m_ablauf.partner(), m_ablauf.partnerLocator());
    emit qsoFertig(logEintrag(m_ablauf,
                              m_slice ? m_slice->frequency() : 0.0,
                              m_slice && m_slice->dspMode() == DSPMode::DIGL,
                              protocol(), eigenerLocator(),
                              m_sendeTonHz));

    // ── Die laufende Nummer hochzaehlen (2026-11-04) ────────────────
    //
    // NACH dem Logeintrag, nicht davor: der Eintrag traegt die
    // Nummer, die GEGEBEN wurde. Wer vorher hochzaehlt, schreibt jede
    // Nummer um eins zu hoch ins Log, und beim Abgleich stimmt kein
    // einziges QSO.
    //
    // Und nur im Contestbetrieb -- sonst liefe die Folge auch an
    // einem gewoehnlichen Abend weiter und faenge beim naechsten
    // Contest irgendwo an.
    if (m_contestAn) {
        const int naechste = naechsteContestNummer() + 1;
        AppSettings::instance().setValue(
            contestNummerSchluessel(),
            QString::number(naechste > 4095 ? 4095 : naechste));
        uebernimmContest();
        zeigeContestStand();
    }
}

void Ft8Applet::zeigeAblauf()
{
    zeigeKnopfzustand();
    zeigeArbeit();
    if (!m_status) { return; }
    switch (m_ablauf.zustand()) {
        case Ft8Ablauf::Zustand::Fertig: {
            const QString wer = m_ablauf.partner();
            m_status->setText(tr("QSO with %1 complete").arg(wer));
            meldeFertigesQso();

            // ── Und weiter, ohne Klick ─────────────────────
            //
            // Wer den CQ-Knopf gedrueckt hat, will rufen -- nicht ein
            // QSO fuehren und dann warten. Bis zum 2026-10-30 war hier
            // Schluss: nach dem RR73 blieb es still, bis der Betreiber
            // wieder drueckte. Betreiber: „Der Durchlauf muss
            // automatisch ablaufen, so schnell wie moeglich ohne mein
            // klicken bei einem QSO."
            //
            // Nur wenn der Ruf vom Knopf kam. Ein Doppelklick in die
            // Liste ist eine Einzelentscheidung: danach wird nicht von
            // selbst CQ gerufen.
            // In die Karenz, damit er nicht gleich noch einmal
            // gearbeitet wird -- der zweite Riegel neben
            // `Ft8Ablauf::istAbschied` (2026-10-30, HB9ZHK).
            m_schlange.merkeGearbeitet(wer, QDateTime::currentMSecsSinceEpoch());

            if (m_cqAutomatik && !m_sendet) {
                // ── Erst die Wartenden, dann wieder CQ ────────
                //
                // Wer mich waehrend des QSOs gerufen hat, steht in
                // der Schlange. Ihn anzurufen ist immer besser als
                // ein neues CQ: er hat schon gezeigt, dass er mich
                // hoert, und er wartet. Am 2026-10-30 riefen PD4RJ
                // und 9A6T zusammen fuenfmal und bekamen nie eine
                // Antwort, weil es diese Zeilen nicht gab.
                if (rufeNaechstenWartenden()) { break; }
                qCInfo(lcFt8Senden).noquote()
                    << "QSO mit" << wer << "fertig — rufe weiter CQ";
                starteCqRuf();
            }
            break;
        }
        case Ft8Ablauf::Zustand::Abgebrochen:
            m_status->setText(m_ablauf.partner().isEmpty()
                ? tr("sequence stopped")
                : tr("gave up calling %1").arg(m_ablauf.partner()));
            break;
        default:
            break;      // die laufende Anzeige macht updateStatus()
    }
}

// ── Auf eine gehoerte Station antworten ──────────────────────────────
//
// Ein Doppelklick in die Fundliste legt den Standardanruf ins
// Eingabefeld: „<sein Ruf> <mein Ruf> <mein Locator>". Gesendet wird
// dadurch nichts — der Druck auf den Knopf bleibt beim Betreiber.
//
// Die Regel steht hier als statische Funktion, nicht im
// Mausereignis: so laesst sie sich ohne Fenster und ohne Maus
// pruefen, und der Doppelklick ist nur der Ausloeser.
// ── Der Locator der Gegenstation, falls die Nachricht einen hat ──────
//
// „CQ HA1BF JN86" hat einen, „OE5SOS DL1ABC -12" nicht — dort waere
// das letzte Wort der Rapport. Bisher ging das letzte Wort
// ungeprueft weiter und stand als Feld im Logbuch; mit der
// Landspalte stand dort sogar „Hungary".
//
// Geprueft wird die Form eines Maidenhead-Feldes: zwei Buchstaben,
// zwei Ziffern. Mehr braucht eine FT8-Nachricht vom Standardtyp
// nicht, und weniger reicht nicht aus, um es von „R-09" oder „+02"
// zu unterscheiden.
QString Ft8Applet::locatorAusNachricht(const QString& nachricht)
{
    const QString letztes =
        nachricht.simplified().section(QLatin1Char(' '), -1).toUpper();
    if (letztes.size() != 4) { return {}; }
    if (!letztes.at(0).isLetter() || !letztes.at(1).isLetter()) { return {}; }
    if (!letztes.at(2).isDigit() || !letztes.at(3).isDigit()) { return {}; }
    return letztes;
}

// ── Die Nachricht aus einer Anzeigezeile ─────────────────────────────
//
// Die Spalten stehen in zeilentext(): vier Zahlenfelder (mit
// FUEHRENDEN Leerzeichen auf feste Breite gebracht), dann die
// Nachricht, dann — durch ZWEI Leerzeichen getrennt — das Land.
//
// Das Land kam mit #311 dazu, und seitdem log `section(' ', 4)` nach
// einem simplified(): es lieferte „CQ HA1BF JN86 Hungary" statt „CQ
// HA1BF JN86", und `section(' ', -1)` gab „Hungary" als Locator aus.
// Innerhalb einer FT8-Nachricht stehen nie zwei Leerzeichen
// hintereinander, also trennt das erste Doppelte sauber; ohne
// Landspalte gibt es keines und die Zeile bleibt, wie sie ist.
QString Ft8Applet::nachrichtAusZeile(const QString& zeile)
{
    int feld = 0;
    int i = 0;
    const int n = zeile.size();
    while (i < n && feld < 4) {
        while (i < n && zeile.at(i).isSpace())  { ++i; }
        while (i < n && !zeile.at(i).isSpace()) { ++i; }
        ++feld;
    }
    while (i < n && zeile.at(i).isSpace()) { ++i; }
    if (i >= n) { return {}; }

    QString rest = zeile.mid(i);
    const int trenner = rest.indexOf(QStringLiteral("  "));
    if (trenner > 0) { rest = rest.left(trenner); }
    return rest.trimmed();
}

QString Ft8Applet::antwortAuf(const QString& zeile,
                              const QString& meinRuf,
                              const QString& meinLocator)
{
    if (meinRuf.trimmed().isEmpty()) { return {}; }

    const QString nachricht = nachrichtAusZeile(zeile);
    if (nachricht.isEmpty()) { return {}; }

    const QString seinRuf = senderCallsign(nachricht);
    if (seinRuf.isEmpty()) { return {}; }

    // Sich selbst anzurufen ergibt keinen Sinn — und waere der
    // haeufigste Fehlgriff, weil das eigene Rufzeichen in jeder
    // Antwort auf einen selbst vorkommt.
    if (seinRuf.compare(meinRuf.trimmed(), Qt::CaseInsensitive) == 0) {
        return {};
    }

    // Der Locator ist das vierstellige Feld (JN67VV -> JN67): mehr
    // passt nicht in eine FT8-Nachricht vom Standardtyp.
    const QString locator = meinLocator.trimmed().left(4).toUpper();
    if (locator.isEmpty()) {
        return QStringLiteral("%1 %2").arg(seinRuf, meinRuf.trimmed().toUpper());
    }
    return QStringLiteral("%1 %2 %3")
        .arg(seinRuf, meinRuf.trimmed().toUpper(), locator);
}

// Der eigene CQ-Ruf. Derselbe Aufbau wie eine Antwort, nur ohne
// Gegenstation: „CQ OE5SOS JN67". Ohne Locator bleibt „CQ OE5SOS" —
// das ist zulaessig und kommt auf dem Band vor.
QString Ft8Applet::cqRuf(const QString& meinRuf, const QString& meinLocator)
{
    const QString ruf = meinRuf.trimmed().toUpper();
    if (ruf.isEmpty()) { return {}; }
    const QString loc = meinLocator.trimmed().left(4).toUpper();
    return loc.isEmpty() ? QStringLiteral("CQ %1").arg(ruf)
                         : QStringLiteral("CQ %1 %2").arg(ruf, loc);
}

// `User/Callsign` ist der kanonische Schluessel — SpotHubDialog
// schreibt ihn („Writes canonical keys: User/Callsign,
// User/GridSquare"). `StationCallsign` wird dagegen von NIEMANDEM
// geschrieben und nur gelesen (SotaClient, MainWindow_KiwiSdr,
// QsoMapWindow); er steht hier als Rueckfall, damit eine von Hand
// gesetzte Fassung nicht ins Leere laeuft.
// ── Tx1..Tx6, wie in WSJT-X ──────────────────────────────────────────
//
// Betreiber 2026-10-18: „dachte das sind dann mehr optionen usw. wie
// ein richtiges ft8 programm." Die sechs Standardsaetze sind der
// haeufigste Griff im QSO -- bisher musste man sie tippen.
//
// Gebaut werden sie NICHT hier, sondern von Ft8Ablauf::nachrichtFuer():
// derselbe Code, den die Automatik benutzt. Hier steht nur, welcher
// Knopf welchen Zustand meint.
QString Ft8Applet::txKnopfNachricht(int nummer, const QString& partner,
                                    const QString& meinRuf,
                                    const QString& meinLocator,
                                    int reportDb)
{
    using Z = Ft8Ablauf::Zustand;
    if (meinRuf.isEmpty()) { return {}; }
    // Tx1 bis Tx5 sprechen jemanden an. Ohne Gegenueber gibt es nichts
    // zu senden -- und einen Platzhalter ins Feld zu legen waere
    // schlimmer, denn der ginge auf Knopfdruck hinaus.
    if (nummer >= 1 && nummer <= 5 && partner.isEmpty()) { return {}; }

    switch (nummer) {
        case 1: return Ft8Ablauf::nachrichtFuer(Z::RufeAn, partner, meinRuf,
                                                meinLocator, reportDb);
        case 2: return Ft8Ablauf::nachrichtFuer(Z::SendeReport, partner,
                                                meinRuf, meinLocator, reportDb);
        case 3: return Ft8Ablauf::nachrichtFuer(Z::SendeRReport, partner,
                                                meinRuf, meinLocator, reportDb);
        case 4: return Ft8Ablauf::nachrichtFuer(Z::SendeRR73, partner, meinRuf,
                                                meinLocator, reportDb);
        case 5: return Ft8Ablauf::nachrichtFuer(Z::Sende73, partner, meinRuf,
                                                meinLocator, reportDb);
        case 6: return Ft8Ablauf::nachrichtFuer(Z::RufeCq, partner, meinRuf,
                                                meinLocator, reportDb);
        default: return {};
    }
}

// Das Gegenueber: der laufende Ablauf, sonst das erste Wort im
// Textfeld -- in jedem Standardsatz steht dort das Rufzeichen der
// Gegenstation, und ein Doppelklick auf eine Zeile hat es genau
// dorthin gelegt. Kein zusaetzliches Feld, keine zweite Wahrheit.
QString Ft8Applet::aktuellerPartner() const
{
    if (m_ablauf.laeuft() && !m_ablauf.partner().isEmpty()) {
        return m_ablauf.partner();
    }
    if (!m_textFeld) { return {}; }
    const QString erstes =
        m_textFeld->text().trimmed().section(QLatin1Char(' '), 0, 0).toUpper();
    return erstes == QLatin1String("CQ") ? QString{} : erstes;
}

// ── Das Blatt hinter dem ⚙ ──────────────────────────────────────────
//
// Zwei Felder, mehr nicht: ohne Rufzeichen kann der Dekoder kein CQ
// bauen, ohne Locator keinen Rapport mit Feld.
//
// Geschrieben werden BEIDE Schluesselpaare. `User/Callsign` ist der
// kanonische (den liest dieser Dekoder, den schreibt der Spot Hub),
// `StationCallsign` lesen vier andere Stellen -- QSO-Karte, KiwiSDR,
// der FlexRadio-Rundruf und das Logbuch. Wer hier eintraegt, soll
// nicht hinterher feststellen, dass die Haelfte des Programms sein
// Rufzeichen nicht kennt. Genau das war am 2026-10-21 der Fall:
// `User/Callsign` stand auf OE5SOS, das Setup-Feld war leer.
void Ft8Applet::zeigeOptionen()
{
    auto& s = AppSettings::instance();

    QDialog blatt(this);
    blatt.setWindowTitle(tr("FT8 Decoder"));
    auto* form = new QFormLayout(&blatt);

    auto* ruf = new QLineEdit(eigenesRufzeichen(), &blatt);
    ruf->setPlaceholderText(QStringLiteral("OE5SOS"));
    form->addRow(tr("Callsign"), ruf);

    auto* loc = new QLineEdit(eigenerLocator(), &blatt);
    loc->setPlaceholderText(QStringLiteral("JN67VV"));
    loc->setToolTip(tr("Four or six characters. The decoder sends the "
                       "first four in CQ calls."));
    form->addRow(tr("Grid square"), loc);

    auto* knoepfe = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &blatt);
    form->addRow(knoepfe);
    connect(knoepfe, &QDialogButtonBox::accepted, &blatt, &QDialog::accept);
    connect(knoepfe, &QDialogButtonBox::rejected, &blatt, &QDialog::reject);

    if (blatt.exec() != QDialog::Accepted) { return; }

    const QString neuerRuf = ruf->text().trimmed().toUpper();
    const QString neuerLoc = loc->text().trimmed().toUpper();
    s.setValue(QStringLiteral("User/Callsign"), neuerRuf);
    s.setValue(QStringLiteral("StationCallsign"), neuerRuf);
    s.setValue(QStringLiteral("User/GridSquare"), neuerLoc);
    s.setValue(QStringLiteral("StationGridSquare"), neuerLoc);
}

// Beide lesen ueber die gemeinsame Rueckfallkette der Station
// (AppSettings::stationCallsign / stationGridSquare). Hier stand bis
// zum 2026-10-28 eine eigene, kuerzere Kette — und die kannte nur die
// zwei kanonischen Schluessel, also genau die beiden, die in Martins
// Einstellungen leer waren. Siehe AppSettings.cpp, dort steht der
// Hergang.
QString Ft8Applet::eigenesRufzeichen()
{
    return AppSettings::stationCallsign().toUpper();
}

QString Ft8Applet::contestNummerSchluessel() const
{
    // Je Band eine eigene Folge -- IARU R1: „a serial number
    // commencing with 001 for the first contact on each band".
    // Dieselbe Vorgabe fuehrt das Contestprogramm
    // (`ContestDefinition::serialScope()` = „band").
    const double hz = m_slice ? m_slice->frequency() : 0.0;
    return QStringLiteral("Ft8/ContestNummer/%1")
        .arg(Longpath::bandKeyName(Longpath::bandFromFrequency(hz)));
}

int Ft8Applet::naechsteContestNummer() const
{
    const int n = AppSettings::instance()
                      .value(contestNummerSchluessel(),
                             QStringLiteral("1")).toInt();
    // Wertebereich wie in `Ft8Ablauf::Contest::brauchbar()`: unter 1
    // oder ueber 4095 ist keine gueltige Nummer, und eine kaputte
    // Einstellung darf den Betrieb nicht anhalten.
    return (n < 1 || n > 4095) ? 1 : n;
}

void Ft8Applet::uebernimmContest()
{
    Ft8Ablauf::Contest c;
    c.an           = m_contestAn;
    c.nummer       = naechsteContestNummer();
    c.meinLocator6 = eigenerLocator();
    m_ablauf.setzeContest(c);
}

void Ft8Applet::zeigeContestStand()
{
    if (!m_status) { return; }
    if (!m_contestAn) {
        m_status->setText(tr("contest off"));
        return;
    }
    m_status->setText(tr("contest: next serial %1 on %2")
                          .arg(naechsteContestNummer(), 3, 10,
                               QLatin1Char('0'))
                          .arg(Longpath::bandLabel(
                              Longpath::bandFromFrequency(
                                  m_slice ? m_slice->frequency() : 0.0))));
}

QString Ft8Applet::eigenerLocator()
{
    return AppSettings::stationGridSquare().toUpper();
}

bool Ft8Applet::eventFilter(QObject* obj, QEvent* ev)
{
    if (m_list && obj == m_list->viewport()) {
        if (ev->type() == QEvent::MouseMove) {
            zeigePilleBei(static_cast<QMouseEvent*>(ev)->pos());
        } else if (ev->type() == QEvent::Leave) {
            versteckePille();
        }
    }
    if (m_list && obj == m_list->viewport()
        && ev->type() == QEvent::MouseButtonDblClick) {
        auto* me = static_cast<QMouseEvent*>(ev);
        QTextCursor c = m_list->cursorForPosition(me->pos());
        c.select(QTextCursor::LineUnderCursor);
        // Der Doppelklick ist nur der Ausloeser; was er tut, steht in
        // nimmFundAn() -- dort, wo auch der Klick auf der Karte ankommt.
        nimmFundAn(c.selectedText());
        return true;
    }
    return AppletWidget::eventFilter(obj, ev);
}

// ── Einen Fund annehmen (2026-11-11) ─────────────────────────────────
//
// Wortgleich der Rumpf, der bis heute im Doppelklick stand -- nur
// herausgezogen, damit der Klick auf der QSO-Karte DENSELBEN Weg geht
// (Betreiber: „damit ich auf der Karte draufklicken kann und es
// probiere"). Neu sind nur die Rueckgabe (warum nicht) und das Signal
// fundAngenommen() fuer den Pruefstand.
QString Ft8Applet::nimmFundAn(const QString& zeile)
{
    const QString antwort = antwortAuf(zeile,
                                       eigenesRufzeichen(),
                                       eigenerLocator());
    if (antwort.isEmpty()) {
        if (eigenesRufzeichen().isEmpty()) {
            if (m_status) { m_status->setText(tr("set your callsign first")); }
            return tr("set your callsign first");
        }
        return tr("no callsign to answer in this line");
    }
    if (!m_textFeld) { return tr("the FT8 window is not built"); }
    {
        m_textFeld->setText(antwort);
        m_textFeld->setFocus();

        // Ein Doppelklick nimmt das QSO an — und ab hier laeuft es
        // bis 73 von selbst. Betreiber am 2026-10-22: „wichtig ist
        // auch, dass nach der annahme des qso das qso automatisch
        // anlaeuft bis 73."
        //
        // Davor stand hier der Schalter „Auto", und er stand aus:
        // der Doppelklick legte die Antwort nur ins Feld, gesendet
        // wurde nichts. Derselbe Fehlgriff wie beim CQ-Knopf
        // (#315). Angehalten wird mit „Stop".
        //
        // ── Auch WAEHREND einer Sendung (2026-10-30) ─────
        //
        // Hier stand `if (!m_sendet)`, und das verschluckte jeden
        // Doppelklick, der in eine laufende Sendung fiel.
        // Betreiber: „doppelklick loest kein automatisches qso
        // aus."
        //
        // Bei FT8 ist das die Haelfte der Zeit: 13,2 s von 15
        // wird getastet, und wer in der Liste etwas sieht,
        // klickt genau dann. Der Druck war weg, ohne dass
        // irgendwo etwas stand.
        //
        // Der Ablauf wird jetzt IMMER gesetzt. Geht gerade etwas
        // hinaus, laeuft es aus -- und der neue Zug uebernimmt
        // beim naechsten Fenster. Dafuer gibt es die
        // Zug-in-der-Frist-Pruefung seit dem 2026-10-28: sie
        // fragt nach der Dekoderfrist noch einmal, was wirklich
        // zu senden ist. `ablaufSchritt()` kehrt bei laufender
        // Sendung von selbst um, es geht also nichts doppelt
        // hinaus.
        {
            const QString nachricht = nachrichtAusZeile(zeile);
            const QString sauber = zeile.simplified();
            const int snr = sauber.section(QLatin1Char(' '), 1, 1).toInt();

            // ── Mit Plaetzen: auf einen Platz (2026-11-17) ───────────
            //
            // Hier ging der Doppelklick IMMER an `m_ablauf`. Mit
            // Plaetzen ist der aber nur der CQ-Ruf: der Dekodier-Eingang
            // reicht dort jeden Fund an `m_mehrfach` und keinen an
            // `m_ablauf`. Das so angefangene QSO sah also nie eine
            // Antwort -- und der laufende CQ-Ruf war obendrein weg,
            // weil `starteAlsAntworter` ihn ueberschrieb.
            //
            // Am Geraet (2026-11-17): Doppelklick auf
            //   081645  OE5SOS JA2FRO PM84
            // sendete sechsmal „JA2FRO OE5SOS JN67", in der Karte stand
            // „waiting for his report", und sein Rapport (zweimal -15)
            // kam dort nie an.
            //
            // Jetzt: er bekommt einen Platz. Ruft er MICH (wie oben),
            // mit dem Rapport; sonst mit meinem Ruf und Locator. Der
            // CQ-Ruf laeuft daneben weiter -- rufen UND abarbeiten ist
            // der Sinn der Plaetze. Darum bleibt auch `m_cqAutomatik`,
            // wie es ist.
            if (mehrfachAn()) {
                const QString wer = senderCallsign(nachricht);
                m_mehrfach.setEigeneDaten(eigenesRufzeichen(),
                                          eigenerLocator());
                if (m_mehrfach.kennt(wer)) {
                    if (m_status) {
                        m_status->setText(
                            tr("%1 already has a slot").arg(wer));
                    }
                    return tr("%1 already has a slot").arg(wer);
                }
                const bool ruftMich = !anruferAus(nachricht).isEmpty();
                const QString loc = locatorAusNachricht(nachricht);
                const bool genommen = ruftMich
                    ? m_mehrfach.nimmAnrufer(wer, snr, loc)
                    : m_mehrfach.nimmVonHand(wer, snr, loc);
                if (!genommen) {
                    return tr("no callsign to answer in this line");
                }
                m_schlange.vergiss(wer);
                qCInfo(lcFt8Senden).noquote()
                    << "Doppelklick: auf einen Platz genommen:" << wer
                    << (ruftMich ? "(ruft mich — Rapport)"
                                 : "(mein Ruf mit Locator)");
                // Im Feld steht, was WIRKLICH hinausgeht -- oben wurde
                // die Antwort des einfachen Wegs eingesetzt, und die
                // waere fuer einen Anrufer die falsche Zeile.
                {
                    QStringList zeilen;
                    for (const Ft8TxSource::Platz& pl : mehrfachPlaetze()) {
                        zeilen << pl.text;
                    }
                    if (!zeilen.isEmpty()) {
                        m_textFeld->setText(
                            zeilen.join(QStringLiteral(" | ")));
                    }
                }
                ablaufSchritt();
                zeigeFundZahl();
                meldeStand();
                emit fundAngenommen(zeile, wer);
                return {};
            }

            m_qsoGemeldet = false;
            // Eine Einzelentscheidung: danach nicht von selbst
            // weiter CQ rufen.
            m_cqAutomatik = false;
            if (m_houndBetrieb) {
                // Im Fox-Betrieb ruft man die Expedition; den
                // Schluss macht sie. Siehe `starteAlsHound`.
                uebernimmContest();   // siehe oben
                m_ablauf.starteAlsHound(senderCallsign(nachricht),
                                        eigenesRufzeichen(),
                                        eigenerLocator());
                ablaufSchritt();
                emit fundAngenommen(zeile, m_ablauf.partner());
                return {};
            }
            uebernimmContest();   // siehe oben
            m_ablauf.starteAlsAntworter(senderCallsign(nachricht),
                                        eigenesRufzeichen(),
                                        eigenerLocator(), snr,
                                        locatorAusNachricht(nachricht));
            ablaufSchritt();
            emit fundAngenommen(zeile, m_ablauf.partner());
        }
    }
    return {};
}

bool Ft8Applet::istFt8Modus(DSPMode modus)
{
    return modus == DSPMode::DIGU || modus == DSPMode::DIGL;
}

QString Ft8Applet::kartenSperre() const
{
    // Dieselbe Regel, nach der MainWindow das Fenster zeigt: ohne
    // Datenmodus ist es kein FT8-Betrieb, egal was die Karte zeigt.
    if (!m_slice || !istFt8Modus(m_slice->dspMode())) {
        return tr("FT8 is not active — switch the receiver to DIGU "
                  "(FT8 button) first");
    }
    if (eigenesRufzeichen().isEmpty()) {
        return tr("set your callsign first");
    }
    // Dieselbe Bedingung, an der sendeNachricht() abbricht.
    if (!m_model || !m_model->ft8TxSource()) {
        return tr("no transmit path — connect a radio first");
    }
    // Was setMox(true) jetzt sagen wuerde: Tastensperre und Bandplan.
    // Nur eine fruehe Erklaerung -- getastet wird spaeter ueber
    // setMox(true), und dort gilt die Pruefung noch einmal.
    if (MoxController* mox = m_model->moxController()) {
        const QString grund = mox->txRefusalReason();
        if (!grund.isEmpty()) {
            return tr("transmit not allowed: %1").arg(grund);
        }
    }
    return {};
}

QString Ft8Applet::rufeVonDerKarte(const QString& zeile)
{
    const QString sperre = kartenSperre();
    if (!sperre.isEmpty()) {
        qCInfo(lcFt8Senden).noquote()
            << "Anruf von der Karte abgelehnt:" << sperre;
        return sperre;
    }
    qCInfo(lcFt8Senden).noquote()
        << "Anruf von der Karte:" << nachrichtAusZeile(zeile);
    return nimmFundAn(zeile);
}

QString Ft8Applet::bandFuerLog(double hz)
{
    // Wie in logEintrag(): erst das Band der Frequenz, dann -- mit
    // Transverter -- das der Antenne.
    double mhz = hz / 1.0e6;
    QString band = bandLabel(bandFromFrequency(hz));
    TransverterSetup::logbuchFrequenz(std::llround(hz), &mhz, &band);
    return band;
}

void Ft8Applet::sendeNachricht(const QString& text)
{
    sendeNachricht(text, {});
}

void Ft8Applet::sendeNachricht(const QString& text,
                               const QVector<Ft8TxSource::Platz>& plaetze)
{
    if (m_sendet) {
        qCInfo(lcFt8Senden) << "Senden abgelehnt: es laeuft schon eine Sendung";
        return;
    }
    const QString sauber = text.trimmed().toUpper();
    if (sauber.isEmpty()) {
        qCInfo(lcFt8Senden) << "Senden abgelehnt: das Textfeld ist leer"
                            << "(Rufzeichen gesetzt:"
                            << !eigenesRufzeichen().isEmpty() << ")";
        if (m_status) { m_status->setText(tr("nothing to send")); }
        return;
    }

    // ── Der Mikrofon-Router ist NICHT mehr Bedingung (2026-10-28) ────
    //
    // Hier stand `|| !m_model->micRouter()`. Der Router gehoert zum
    // eigenen WDSP-Sendekanal; die Flex hat keinen, sie rechnet
    // selbst. Also brach jeder Sendeversuch an der Flex hier ab —
    // stumm, weil die Statuszeile sofort ueberschrieben wird.
    //
    // Gebraucht wird nur die Tonquelle. Wer den Ton abholt, ist eine
    // Frage des Funkgeraets: beim eigenen der Router, beim Flex die
    // Sendepumpe in MainWindow_Flex.cpp.
    if (!m_model || !m_model->ft8TxSource()) {
        qCWarning(lcFt8Senden)
            << "Senden abgelehnt: keine Tonquelle (Modell"
            << (m_model != nullptr) << ", Ft8TxSource"
            << (m_model && m_model->ft8TxSource() != nullptr) << ")";
        if (m_status) { m_status->setText(tr("no transmit path")); }
        return;
    }

    Ft8TxSource* quelle = m_model->ft8TxSource();

    // ── Ein Fenster, mehrere Stationen (Multi Answering) ───────
    //
    // Mit mehreren Plaetzen geht nicht EIN Satz hinaus, sondern die
    // Summe mehrerer -- jeder auf seiner eigenen Tonlage. Der Rest
    // des Sendewegs (Sendeplan, Taktgeber, Tasten) ist identisch:
    // darum steht hier nur das Packen anders, nicht der ganze Weg
    // noch einmal.
    if (!plaetze.isEmpty()) {
        if (!quelle->vorbereitenMehrere(plaetze, protocol())) {
            qCWarning(lcFt8Senden)
                << "Senden abgelehnt: ft8_lib kann die" << plaetze.size()
                << "Plaetze nicht packen";
            if (m_status) { m_status->setText(tr("cannot encode slots")); }
            return;
        }
        QStringList was;
        for (const Ft8TxSource::Platz& p : plaetze) {
            was << QStringLiteral("%1@%2").arg(p.text)
                       .arg(int(p.toneHz));
        }
        qCInfo(lcFt8Senden).noquote()
            << "Senden vorbereitet (" << plaetze.size() << "Plaetze):"
            << was.join(QStringLiteral(" | "));
    } else {
    if (!quelle->vorbereiten(sauber, float(m_sendeTonHz), protocol())) {
        qCWarning(lcFt8Senden).noquote()
            << "Senden abgelehnt: ft8_lib kann \"" + sauber + "\" nicht packen";
        if (m_status) {
            m_status->setText(tr("cannot encode “%1”").arg(sauber));
        }
        return;
    }
    qCInfo(lcFt8Senden).noquote()
        << "Senden vorbereitet:" << sauber << "auf" << m_sendeTonHz << "Hz";
    }

    // Wann faengt mein naechstes Fenster an? Die Gegenperiode zu der,
    // in der gehoert wird — wer in derselben sendet, hoert sich selbst
    // und niemand sonst.
    const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
    const Ft8Tx::Sendeplan plan =
        Ft8Tx::sendeplan(protocol(), kDekoderFristMs,
                         kZielVersatzMs, kSenderNachlaufMs);
    const qint64 wartet = Ft8Tx::bisZumNaechsten(jetzt, protocol(),
                                                 m_periode)
                          + plan.tastenNachMs;
    m_sendet = true;
    // Ein neuer Versuch: der Grund des letzten Scheiterns ist damit
    // ueberholt.
    m_festerHinweis.clear();
    if (m_cqBtn)   { m_cqBtn->setEnabled(false); }
    if (m_stopBtn) { m_stopBtn->setEnabled(true); }
    zeigeKnopfzustand();
    if (m_status) {
        m_status->setText(tr("sending in %1 s")
                              .arg(wartet / 1000.0, 0, 'f', 1));
    }

    if (!m_sendeTimer) {
        m_sendeTimer = new QTimer(this);
        m_sendeTimer->setSingleShot(true);
        // ── Genau, nicht grob (2026-10-29) ───────────────────────────
        //
        // Qt gibt jedem Taktgeber ab 20 ms von sich aus
        // `Qt::CoarseTimer` und erlaubt sich dabei, bis zu FUENF
        // PROZENT des Intervalls zu frueh zu wecken — damit mehrere
        // Taktgeber gemeinsam aufwachen koennen. Bei FT8 wird hier auf
        // die naechste Fenstergrenze gewartet, und das sind bis zu
        // 17,8 s. Fuenf Prozent davon sind 0,89 s.
        //
        // Am Geraet gemessen (Protokoll 2026-10-29, 07:02 bis 07:04,
        // fuenf Sendungen): MOX ging bei :29,21 an statt bei :30,00 —
        // 0,779 / 0,787 / 0,786 / 0,786 / 0,786 s zu frueh, jedes Mal.
        // Die Rechnung in `Ft8Tx::bisZumNaechsten` war dabei richtig;
        // der Taktgeber hat sie verworfen. Betreiber: „die empfangs
        // und sendezeit stimmt nicht."
        //
        // Die Systemuhr war es nicht: `sntp time.apple.com` meldete
        // im selben Augenblick +37 ms.
        m_sendeTimer->setTimerType(Qt::PreciseTimer);
    }
    m_sendeTimer->disconnect();
    QString gesendet = sauber;
    const bool ausAblauf = m_ablauf.laeuft();
    // Was gepackt wurde, als Plaetze -- auch der einzelne Satz, damit
    // die Nachpruefung unten Gleiches mit Gleichem vergleicht.
    QVector<Ft8TxSource::Platz> gepackt = plaetze;
    const bool ausPlaetzen = !plaetze.isEmpty();
    if (gepackt.isEmpty()) {
        Ft8TxSource::Platz einer;
        einer.text   = sauber;
        einer.toneHz = float(m_sendeTonHz);
        gepackt.push_back(einer);
    }
    connect(m_sendeTimer, &QTimer::timeout, this,
            [this, gesendet, ausAblauf, ausPlaetzen, gepackt, plan]() mutable {
        if (!m_sendet || !m_model) { return; }
        Ft8TxSource* q = m_model->ft8TxSource();
        if (!q) {
            qCWarning(lcFt8Senden) << "Tonquelle zwischendurch verschwunden";
            abbrechen();
            return;
        }

        // ── Den Zug nachsehen, bevor er hinausgeht ─────────────
        //
        // Die Nachricht wurde beim Ende der VORIGEN Sendung gepackt,
        // also knapp 13 s vor diesem Augenblick. In der Zwischenzeit
        // hat die Gegenstation geantwortet und der Dekoder ist fertig
        // geworden — nur kam das bisher zu spaet: `ablaufSchritt()`
        // kehrt bei laufender Sendung um, und der neue Zug musste ein
        // ganzes Fenster warten. Jeder Zug kostete 30 s statt 15.
        //
        // Darum wird hier, nach der Dekoderfrist, nochmal gefragt.
        // Nur bei einer Sendung aus dem Ablauf: wer Tx1…Tx6 oder
        // „Send" drueckt, will genau das senden, was dort steht.
        // ── Und wenn das QSO in der Frist ZU ENDE ging (2026-10-31) ─
        //
        // Die Nachpruefung darunter fragt seit jeher nur, ob sich der
        // ZUG geaendert hat. Den Fall „es gibt keinen Zug mehr“ kannte
        // sie nicht: `laeuft()` ist bei `Fertig` und `Abgebrochen`
        // falsch, also fiel sie ganz aus -- und die vor 13 s gepackte
        // Nachricht ging trotzdem hinaus.
        //
        // Am Geraet gemessen (2026-10-31, Betreiber-Mitschnitt):
        //   054100  Tx  R5AV OE5SOS +15
        //   054115      OE5SOS R5AV RR73     <- er ist fertig
        //   054130  Tx  R5AV OE5SOS +15      <- trotzdem nochmal
        //   054200  Tx  UA3IHJ OE5SOS +15    <- der Wartende erst jetzt
        //
        // UA3IHJ hatte um 054045 gerufen. Der Zug um 054130 war nicht
        // nur fuer nichts -- er hat dem Wartenden sein Fenster
        // weggenommen, und 75 s nach seinem Ruf ist er laengst
        // woanders. Danach ruft der Ablauf ihn fuenfmal ins Leere.
        //
        // Also: nicht senden -- und wenn jemand wartet, bekommt er
        // GENAU DIESES Fenster. Gepackt wird unten, mit demselben
        // Weg, der auch den gewechselten Zug packt.
        //
        // Nur im einfachen Betrieb: mit mehreren Plaetzen fuehrt
        // `Ft8Mehrfach` die Ablaeufe, und `m_ablauf` ist dort der
        // CQ-Ruf, nicht das QSO.
        if (ausAblauf && !m_ablauf.laeuft() && !mehrfachAn()) {
            if (m_ablauf.zustand() == Ft8Ablauf::Zustand::Fertig) {
                // Erst ins Logbuch, dann den Platz raeumen. `zeigeAblauf`
                // tut das sonst; hier kommt der naechste Ablauf dazwischen.
                meldeFertigesQso();
            }
            if (!beginneNaechstenWartenden()) {
                qCInfo(lcFt8Senden).noquote()
                    << "Zug verworfen: das QSO ging in der Dekoderfrist"
                       " zu Ende, und niemand wartet";
                abbrechen();
                zeigeAblauf();
                return;
            }
            qCInfo(lcFt8Senden).noquote()
                << "QSO in der Frist beendet — der Wartende bekommt"
                   " dieses Fenster statt des naechsten";
        }

        // ── Mit mehreren Plaetzen: die PLAETZE nachsehen (2026-11-17) ──
        //
        // Die Nachpruefung darunter kennt nur `m_ablauf`. Mit Plaetzen
        // ist der aber bloss der CQ-Ruf -- und sein Satz ist nie gleich
        // der Anzeigezeile „A | B | CQ". Also schlug sie bei JEDER
        // Sendung an und packte den einzelnen Satz ueber das fertige
        // Paket: die Antworten an die Anrufer gingen nie hinaus.
        //
        // Am Geraet gemessen (2026-11-17, 17 m, 3 Plaetze):
        //   09:15:13  Senden vorbereitet (2 Plaetze):
        //             US2YW OE5SOS -15@2430 | CQ OE5SOS JN67@2490
        //   09:15:30  Zug in der Frist gewechselt:
        //             US2YW OE5SOS -15 | CQ OE5SOS JN67 -> CQ OE5SOS JN67
        // So bei allen 27 Sendungen dieser Viertelstunde. US2YW, RA5CW
        // und 9A2KS riefen zusammen elfmal und bekamen nie einen
        // Rapport; die Plaetze hielten ihre Zuege fuer gesendet und
        // schrieben zwei QSOs ins Logbuch, die es nie gab. Betreiber:
        // „die automatischen antworten meines cq call funktionieren
        // irgendwie nicht."
        //
        // Darum hier dieselbe Frage wie beim einzelnen Ablauf, nur an
        // der richtigen Stelle gestellt: was ginge JETZT auf den
        // Plaetzen hinaus? Das faengt drei Faelle auf einmal:
        //   * ein Platz ist in der Frist einen Zug weitergekommen,
        //   * jemand hat in der Frist zum ersten Mal gerufen (auch
        //     wenn nur das CQ gepackt war) -- er bekommt DIESES
        //     Fenster und nicht erst das uebernaechste,
        //   * ein QSO ist in der Frist zu Ende gegangen.
        // Nur bei einer Sendung aus dem Lauf: wer „Send" drueckt, will
        // genau das senden, was dort steht.
        const bool mehrfachLauf =
            (ausAblauf || ausPlaetzen) && mehrfachAn()
            && (ausPlaetzen || m_mehrfach.aktive() > 0);
        if (mehrfachLauf) {
            // Erst aufraeumen: was in der Frist fertig wurde, gibt
            // seinen Platz frei, und aus der Schlange rueckt nach.
            if (meldeFertigePlaetze() > 0 && m_cqAutomatik) {
                // Ein QSO ist in der Frist fertig geworden, und der
                // Lauf kam vom CQ-Knopf: der Ruf zaehlt von vorn und
                // geht -- wenn ein Platz frei ist -- gleich in diesem
                // Fenster wieder mit hinaus, nicht erst nach einer
                // Pause.
                frischeCqRufAuf();
            }
            const QVector<Ft8TxSource::Platz> neu = mehrfachPlaetze();
            if (neu.isEmpty()) {
                qCInfo(lcFt8Senden).noquote()
                    << "Zug verworfen: alle Plaetze sind in der"
                       " Dekoderfrist fertig geworden, und es laeuft"
                       " kein CQ";
                abbrechen();
                zeigeAblauf();
                return;
            }
            bool gleich = neu.size() == gepackt.size();
            for (int i = 0; gleich && i < neu.size(); ++i) {
                gleich = neu.at(i).text == gepackt.at(i).text
                         && qFuzzyCompare(neu.at(i).toneHz + 1.0f,
                                          gepackt.at(i).toneHz + 1.0f);
            }
            if (!gleich) {
                if (q->vorbereitenMehrere(neu, protocol())) {
                    QStringList alt;
                    for (const Ft8TxSource::Platz& p : gepackt) {
                        alt << p.text;
                    }
                    QStringList jetzt;
                    QStringList mitTon;
                    for (const Ft8TxSource::Platz& p : neu) {
                        jetzt  << p.text;
                        mitTon << QStringLiteral("%1@%2").arg(p.text)
                                      .arg(int(p.toneHz));
                    }
                    qCInfo(lcFt8Senden).noquote()
                        << "Plaetze in der Frist neu gepackt:"
                        << alt.join(QStringLiteral(" | ")) << "->"
                        << mitTon.join(QStringLiteral(" | "));
                    gepackt  = neu;
                    gesendet = jetzt.join(QStringLiteral(" | "));
                    if (m_textFeld) { m_textFeld->setText(gesendet); }
                    zeigeArbeit();
                } else {
                    qCWarning(lcFt8Senden).noquote()
                        << "Die neuen Plaetze liessen sich nicht packen"
                           " — es geht das alte Paket hinaus";
                }
            }
        } else if (ausAblauf && m_ablauf.laeuft()) {
            const QString neu = m_ablauf.naechsteNachricht();
            if (!neu.isEmpty() && neu != gesendet) {
                if (q->vorbereiten(neu, float(m_sendeTonHz), protocol())) {
                    qCInfo(lcFt8Senden).noquote()
                        << "Zug in der Frist gewechselt:" << gesendet
                        << "->" << neu;
                    gesendet = neu;
                    if (m_textFeld) { m_textFeld->setText(neu); }
                } else {
                    qCWarning(lcFt8Senden).noquote()
                        << "Neuer Zug \"" + neu + "\" liess sich nicht "
                           "packen — es geht der alte hinaus";
                }
            }
        }

        // Erst den Weg legen, dann tasten: andersherum ginge der
        // Sender mit dem Mikrofon auf, nicht mit dem Ton. Ohne Router
        // (Flex) entfaellt das Umlegen — dort holt die Sendepumpe die
        // Proben direkt aus der Quelle.
        if (auto* router = m_model->micRouter()) {
            m_quelleVorher = static_cast<int>(router->activeSource());
            router->setActiveSource(MicSource::Ft8);
        }
        // Ab der uebersprungenen Stille: so bleibt der Zeitpunkt des
        // ersten Tons derselbe, obwohl wir spaeter tasten.
        q->starten(plan.probenAbMs);

        // ── Wenn der Bandplan nein sagt, nicht weitermachen ──────────
        //
        // MoxController::setMox(true) darf abgelehnt werden: falsche
        // Betriebsart, Frequenz ausserhalb des Bandes, Cross-Band-TX.
        // Die Ablehnung kommt als moxRejected(reason) und sonst gar
        // nicht — setMox() gibt nichts zurueck.
        //
        // Ohne diese Verbindung lief der Ton danach volle 12,6 s
        // weiter, der Sendeweg blieb auf FT8 (also das Mikrofon
        // abgeklemmt), und in der Zeile stand „transmitting", waehrend
        // nichts hinausging. Jetzt bricht der Versuch sofort ab und
        // sagt, woran es lag.
        bool abgelehnt = false;
        QString geraetSagtNein;
        if (auto* mox = m_model->moxController()) {
            QMetaObject::Connection c = connect(
                mox, &MoxController::moxRejected, this,
                [this, &abgelehnt](const QString& grund) {
                    abgelehnt = true;
                    if (m_status) {
                        m_status->setText(tr("not transmitting: %1").arg(grund));
                    }
                });
            // Und was das GERAET verweigert (Stehwelle, Verriegelung,
            // keine Sendescheibe): das kommt nicht als moxRejected,
            // sondern aus `flexTasten()` — im selben Aufruf. Nur
            // merken; gehandelt wird, wenn setMox() zurueck ist.
            QMetaObject::Connection g = connect(
                m_model, &RadioModel::flexNichtGetastet, this,
                [&geraetSagtNein](const QString& grund) {
                    geraetSagtNein = grund;
                },
                Qt::DirectConnection);
            mox->setMox(true);
            disconnect(c);
            disconnect(g);
        }
        if (!geraetSagtNein.isEmpty()) {
            // Keine „Tx"-Zeile, kein „transmitting": es ging nichts
            // hinaus.
            sendenGescheitert(geraetSagtNein);
            return;
        }
        if (abgelehnt) {
            // abbrechen() setzt den Sendeweg zurueck und macht die
            // Knoepfe wieder frei; die Zeile oben bleibt stehen, weil
            // updateStatus() sie erst beim naechsten Takt ersetzt.
            const QString grund = m_status ? m_status->text() : QString();
            abbrechen();
            if (m_status && !grund.isEmpty()) { m_status->setText(grund); }
            return;
        }
        if (m_status) { m_status->setText(tr("transmitting")); }
        zeigeEigeneSendung(gesendet);

        // Nur so lange tasten, wie das SIGNAL dauert -- nicht das
        // ganze Fenster.
        //
        // Gemessen am 2026-10-22 am Geraet: der Sender blieb 14,9 s an
        // (`mox=true` 06:17:30,295 bis 06:17:45,170), obwohl ein
        // FT8-Signal 12,64 s lang ist. Betreiber: „hat zu lange
        // gedauert." Die zwei Sekunden danach gehen nicht nur
        // verloren -- in ihnen laeuft der Empfang nicht, und bei FT8
        // beginnt das naechste Fenster unmittelbar. Wer zu spaet
        // zurueckkommt, hoert den Anfang der Antwort nicht.
        //
        // Der Zuschlag von 300 ms bleibt: er sorgt dafuer, dass der
        // letzte Ton wirklich draussen ist, bevor der Sender
        // abschaltet.
        // Auch hier genau: ein Grobtimer duerfte bis zu 0,65 s zu
        // frueh abschalten und damit den Schluss des Signals
        // abschneiden (siehe `setTimerType` oben).
        QTimer* schluss = new QTimer(this);
        schluss->setSingleShot(true);
        schluss->setTimerType(Qt::PreciseTimer);
        connect(schluss, &QTimer::timeout, this, [this, schluss]() {
            schluss->deleteLater();
            fensterZuEnde();
        });
        // ── So lange, wie das Signal WIRKLICH braucht ─────────
        //
        // Hier stand `signalMs + 300`, gerechnet ab der
        // Fenstergrenze. Das Signal beginnt aber erst nach der
        // fuehrenden Stille der Wellenform. Am Geraet gemessen
        // (2026-10-30, 07:09:30,003 getastet, erster Ton 07:09:31,185,
        // geloest 07:09:42,945): die letzten 880 ms fehlten, also 5,5
        // von 79 Symbolen. `sendeplan()` rechnet die Dauer jetzt vom
        // Zielversatz aus — der Nachlauf von 300 ms bleibt, damit der
        // letzte Ton wirklich draussen ist.
        schluss->start(static_cast<int>(plan.tastdauerMs));
    });
    m_sendeTimer->start(static_cast<int>(wartet));
}

// ── Das Geraet hat nicht getastet ────────────────────────────────────
//
// Am Geraet gemessen (2026-11-17, Flex, 15 m, FT8):
//   12:30:30.254  MOX an, getastet
//   12:30:30.772  SENDUNG GEBROCHEN — Stehwelle 2.52 ueber der Grenze 2.5
//   12:31:00.475  MOX an — nicht getastet, Stehwelle 2.53 ueber 2.50
//   12:31:30.258  MOX an — nicht getastet …
//   12:32:00.458  MOX an — nicht getastet …
// Der Riegel tat, was er soll. Das Fenster aber schrieb zu jeder dieser
// Zeiten eine „Tx"-Zeile, zeigte „transmitting" und liess den Ablauf
// weiterzaehlen („calling T77RN — waiting for his report"), als waere
// der Anruf draussen. Betreiber: „da steht zwar, dass gesendet wird,
// aber es geht keine Leistung raus!"
//
// `moxRejected` weiter unten faengt nur, was der MoxController selbst
// ablehnt (Bandplan, Betriebsart). Was das GERAET verweigert, kam hier
// nie an: `flexTasten()` sagt es nur seinem Aufrufer im Hauptfenster.
//
// Jetzt haelt der Lauf an, wie bei Stop — ein Ablauf, der Zuege fuer
// gesendet haelt, die nie hinausgingen, gibt irgendwann auf oder,
// schlimmer, schreibt ein QSO ins Log, das es nicht gab. Und der Grund
// bleibt stehen, bis der Betreiber wieder sendet.
void Ft8Applet::sendenGescheitert(const QString& grund)
{
    // Nur die eigene Sendung: wer am Mikrofon spricht oder abstimmt,
    // waehrend hier nichts laeuft, geht dieses Fenster nichts an.
    if (!m_sendet) { return; }

    qCWarning(lcFt8Senden).noquote()
        << "Nicht gesendet:" << grund << "— der Lauf haelt an";

    m_cqAutomatik = false;
    m_mehrfach.abbrechen();
    m_ablauf.abbrechen();
    m_cqVersucheOffen = 0;
    abbrechen();
    zeigeAblauf();
    // NACH zeigeAblauf(): das schreibt selbst in die Statuszeile, und
    // der Grund stuende sonst keinen Augenblick dort.
    m_festerHinweis = tr("not transmitting: %1").arg(grund);
    if (m_status) { m_status->setText(m_festerHinweis); }
}

void Ft8Applet::abbrechen()
{
    if (m_sendeTimer) { m_sendeTimer->stop(); }

    // Erst den Sender aus, dann den Weg zurueck — in dieser
    // Reihenfolge ist zu keinem Zeitpunkt das Mikrofon auf Sendung.
    if (m_model && m_model->moxController()) {
        m_model->moxController()->setMox(false);
    }
    if (m_model && m_model->ft8TxSource()) {
        m_model->ft8TxSource()->abbrechen();
    }
    if (m_model && m_model->micRouter() && m_quelleVorher >= 0) {
        m_model->micRouter()->setActiveSource(
            static_cast<MicSource>(m_quelleVorher));
    }
    m_quelleVorher = -1;
    m_sendet = false;
    if (m_cqBtn)   { m_cqBtn->setEnabled(true); }
    if (m_stopBtn) { m_stopBtn->setEnabled(false); }
    zeigeKnopfzustand();
    updateStatus();
}

// ── Was die Knoepfe zeigen ───────────────────────────────────────────
//
// Betreiber am 2026-10-29: „der button sollte sich auch farblich
// aendern, wenn er geklickt ist."
//
// Nicht „geklickt" ist hier das Richtige, sondern „laeuft". Ein
// einrastbarer Knopf kippt beim Klick von selbst um und bliebe danach
// stehen, auch wenn der Ruf laengst aufgegeben hat — er zeigte dann,
// was zuletzt gedrueckt wurde, statt was gerade geschieht. Also wird
// der Zustand hier aus dem Ablauf gelesen und gesetzt.
//
// Blau ist im Hausstil das Anfassbare und das Aktive; die Knoepfe
// nehmen dieselbe Fuellung wie FT8/FT4 oder die Bandwahl darueber.
void Ft8Applet::zeigeKnopfzustand()
{
    const bool ruftCq = m_ablauf.laeuft();
    if (m_cqRufenBtn) { m_cqRufenBtn->setChecked(ruftCq); }
    // „Send" leuchtet nur beim Einzelschuss: waehrend eines Ablaufs
    // ist er ohnehin gesperrt, und zwei leuchtende Knoepfe waeren eine
    // Frage statt einer Antwort.
    if (m_cqBtn) { m_cqBtn->setChecked(m_sendet && !ruftCq); }
}

void Ft8Applet::updateStatus()
{
    if (!m_status || !m_stream) { return; }

    const int imFenster = m_stream->bufferedSamples();
    const double sekunden = static_cast<double>(imFenster)
                            / Ft8Decoder::kSampleRate;
    const double voll = m_stream->slotMs() / 1000.0;

    // ── Die Uhr steht unten, nicht oben (2026-10-30) ────────
    //
    // Hier stand jede Sekunde „0.0 / 15.0 s" — und ueberschrieb
    // damit jede Meldung, die der Ablauf gerade hinterlassen hatte.
    // Seit Entwurf C traegt die Arbeitsleiste die Uhr; oben bleibt
    // stehen, WAS gerade los ist. Ohne laufenden Ablauf (und damit
    // ohne Arbeitsleiste) bleibt es bei der Uhr: eine leere Zeile
    // waere schlechter als eine, die wenigstens den Takt zeigt.
    // Die Uhr steht in der Arbeitsleiste, und nur dort. Am laufenden
    // Geraet stand sie zweimal im Bild: oben im Panelkopf und unten.
    // Oben bleibt, WAS zuletzt los war -- im Leerlauf, wie viele Funde
    // die letzte Periode brachte.
    if (!m_ablauf.laeuft() && !m_sendet) {
        m_status->setText(m_festerHinweis.isEmpty() ? tr("listening")
                                                    : m_festerHinweis);
    }

    // Die Arbeitsleiste laeuft mit derselben Uhr: sie zeigt denselben
    // Stand, nur dort, wo man hinsieht.
    if (m_arbeitBalken) {
        m_arbeitBalken->setzeStand(voll > 0.0 ? sekunden / voll : 0.0,
                                   m_sendet);
    }
    if (m_arbeitUhr) {
        m_arbeitUhr->setText(QStringLiteral("%1 / %2 s")
                                 .arg(sekunden, 0, 'f', 1)
                                 .arg(voll, 0, 'f', 1));
    }
}

// ── Die Arbeitsleiste nachziehen ─────────────────────────────────────
//
// Sie sagt dasselbe wie die Statuszeile, aber als Satz, den man im
// Vorbeigehen liest: mit wem, welcher Zug, was danach kommt. Gerufen
// aus `zeigeAblauf()` -- also bei jedem Zustandswechsel des Ablaufs.
void Ft8Applet::zeigeArbeit()
{
    // Den Stand ZUERST melden, nicht am Ende (2026-10-30).
    //
    // Der Ausstieg darunter gilt der Arbeitsleiste: gibt es sie
    // nicht, ist nichts zu beschriften. Das Plaetze-Panel haengt
    // aber nicht an der Leiste, sondern am Ablauf -- und bekam
    // dadurch nichts, sobald die Leiste fehlte.
    //
    // Gesucht habe ich das an vier Stellen (Merker, Qt-Sichtbarkeit,
    // Fensterliste, Spaltenzustand), und es lag an einem `return`
    // zwei Zeilen weiter oben.
    meldeStand();

    if (!m_arbeit) { return; }

    using Z = Ft8Ablauf::Zustand;
    const Z z        = m_ablauf.zustand();
    const QString wer = m_ablauf.partner();
    const bool laeuft = m_ablauf.laeuft();

    QString zustand;
    QString danach;
    switch (z) {
    case Z::Ruht:
        zustand = tr("idle");
        danach  = tr("press CQ, or double-click a line");
        break;
    case Z::RufeCq:
        zustand = m_cqVersucheOffen > 0
                      ? tr("calling CQ · %1 left").arg(m_cqVersucheOffen)
                      : tr("calling CQ");
        danach  = tr("waiting for an answer");
        break;
    case Z::RufeAn:
        zustand = tr("calling");
        danach  = tr("waiting for his report");
        break;
    case Z::SendeReport:
        zustand = tr("QSO with");
        danach  = tr("then his confirmation");
        break;
    case Z::SendeRReport:
        zustand = tr("QSO with");
        danach  = tr("then his RR73");
        break;
    case Z::SendeRR73:
    case Z::Sende73:
        zustand = tr("QSO with");
        danach  = m_cqAutomatik ? tr("then the log, then CQ again")
                                : tr("then into the log");
        break;
    case Z::Fertig:
        zustand = tr("complete");
        danach  = m_cqAutomatik ? tr("logged · calling CQ again")
                                : tr("logged");
        break;
    case Z::Abgebrochen:
        zustand = wer.isEmpty() ? tr("stopped") : tr("gave up");
        danach  = QString();
        break;
    }

    if (m_arbeitZustand) { m_arbeitZustand->setText(zustand); }
    if (m_arbeitPartner) { m_arbeitPartner->setText(wer); }
    if (m_arbeitLand) {
        m_arbeitLand->setText(wer.isEmpty() ? QString() : landFuer(wer));
    }
    zeigeWer(wer);
    if (m_arbeitDanach) { m_arbeitDanach->setText(danach); }
    if (m_arbeitZug) {
        // Was wirklich hinausgeht: bei laufender Sendung steht es
        // schon im Feld, sonst fragt der Ablauf.
        const QString zug = laeuft ? m_ablauf.naechsteNachricht()
                                   : QString();
        m_arbeitZug->setText(zug.isEmpty() ? tr("—") : zug);
    }

}

// ── Die Zeile unter „QSO with" (2026-11-18) ──────────────────────────
//
// Alles ohne Netz wird hier gerechnet: der Locator, den er in DIESEM
// QSO gesendet hat (sonst der von QRZ), Entfernung und Richtung vom
// eigenen Feld, und das Licht bei ihm -- mit Grauzone::fuer, derselben
// Rechnung wie in der Detailkarte des Logbuchs, damit beide dieselbe
// Grauzone zeigen. Name, Ort und Klasse kommen aus der Auskunft der
// Rotor/Log-Karte (setPartnerAuskunft).
void Ft8Applet::zeigeWer(const QString& wer)
{
    if (!m_arbeitWer) { return; }
    Ft8PartnerZeile::Inhalt in;
    if (wer.isEmpty()) {
        m_arbeitWer->setzeInhalt(in);
        return;
    }
    in.rufzeichen = wer;

    const bool seine = !m_auskunft.rufzeichen.isEmpty()
        && Callsigns::normalized(m_auskunft.rufzeichen)
               == Callsigns::normalized(wer);
    const bool da = seine
        && m_auskunft.stand == RufzeichenAuskunft::Stand::Da;
    if (seine) { in.qrz = m_auskunft.stand; }
    if (da) {
        in.name   = m_auskunft.info.displayName();
        in.ort    = m_auskunft.info.city.trimmed();
        in.klasse = m_auskunft.info.licenseClass.trimmed();
    }

    // Was er JETZT sendet, gilt vor dem, was bei QRZ steht: wer
    // portabel ist, hat bei QRZ noch den Heimatort. Liegt der QRZ-
    // Locator aber im gesendeten Grossfeld, ist er nur feiner -- dann
    // gilt er, sonst stuenden hier 672 km und in der Rotor/Log-Karte
    // daneben 647 km fuer dieselbe Station (im Bild gesehen).
    QString gesendet = m_ablauf.partnerLocator6();
    if (!isValidGridSquare(gesendet)) { gesendet = m_ablauf.partnerLocator(); }
    const QString ausQrz = da ? m_auskunft.info.grid.trimmed().toUpper() : QString();
    QString loc = gesendet;
    if (!isValidGridSquare(loc)) {
        loc = ausQrz;
    } else if (isValidGridSquare(ausQrz) && ausQrz.size() > loc.size()
               && ausQrz.startsWith(loc.left(4), Qt::CaseInsensitive)) {
        loc = ausQrz;
    }
    const QString mein = eigenerLocator();
    if (isValidGridSquare(loc)) {
        in.locator = loc.toUpper();
        if (isValidGridSquare(mein)) {
            in.km   = static_cast<int>(std::lround(calculateDistanceKm(mein, loc)));
            in.grad = static_cast<int>(std::lround(
                          calculateBearingInDegrees(mein, loc))) % 360;
        }
    }

    // Das Licht bei ihm: aus dem Locator, sonst aus der Landesmitte.
    LogEntry e;
    e.call         = wer;
    e.gridSquare   = in.locator;
    e.myGridSquare = mein;
    e.timeOn       = QDateTime::currentDateTimeUtc();
    const CtyDatParser* cty = (m_model && m_model->dxccColorProvider())
        ? &m_model->dxccColorProvider()->ctyDat() : nullptr;
    in.licht = Grauzone::fuer(e, mein, cty).dort.licht;

    m_arbeitWer->setzeInhalt(in);
}

void Ft8Applet::setPartnerAuskunft(const RufzeichenAuskunft& a)
{
    const QString wer = m_ablauf.partner();
    if (wer.isEmpty() || a.rufzeichen.isEmpty()
        || Callsigns::normalized(a.rufzeichen) != Callsigns::normalized(wer)) {
        return;
    }
    m_auskunft = a;
    zeigeWer(wer);
}

// ── Der Stand fuer das Plaetze-Panel ─────────────────────────────────
//
// Betreiber am 2026-10-30: „ich will sehen, sobald ein qso oder
// mehrere vielleicht seitlich im eigenen fenster zu sehen sind."
//
// Gebaut wird er HIER, nicht dort: `Ft8Mehrfach` und
// `Ft8Warteschlange` gehoeren diesem Applet, und zwei Fenster, die
// beide hineingreifen, haetten jedes seine eigene Meinung darueber,
// welcher Zug gerade dran ist.

// Der Zug, wie das Fenster ihn nennt -- „Tx2", „RR73", „CQ". NICHT
// `Ft8Ablauf::zustandName()`: das sind die technischen Namen
// („SendeRReport"), gut fuer Prueflaeufe und falsch auf einer
// Kachel, die 48 Punkte breit ist.
QString Ft8Applet::zugKurz(Ft8Ablauf::Zustand z)
{
    using Z = Ft8Ablauf::Zustand;
    switch (z) {
        case Z::RufeCq:       return QStringLiteral("CQ");
        case Z::RufeAn:       return QStringLiteral("Tx1");
        case Z::SendeReport:  return QStringLiteral("Tx2");
        case Z::SendeRReport: return QStringLiteral("Tx3");
        case Z::SendeRR73:    return QStringLiteral("RR73");
        case Z::Sende73:      return QStringLiteral("73");
        case Z::Fertig:       return tr("done");
        case Z::Ruht:
        case Z::Abgebrochen:  return QString();
    }
    return QString();
}

int Ft8Applet::stufeFuer(Ft8Ablauf::Zustand z)
{
    using Z = Ft8Ablauf::Zustand;
    switch (z) {
        case Z::Ruht:         return 0;
        case Z::RufeCq:       return 0;
        case Z::RufeAn:       return 1;
        case Z::SendeReport:  return 2;
        case Z::SendeRReport: return 3;
        case Z::SendeRR73:    return 4;
        case Z::Sende73:      return 5;
        case Z::Fertig:       return 5;
        case Z::Abgebrochen:  return 0;
    }
    return 0;
}

Ft8PlaetzeStand Ft8Applet::baueStand() const
{
    Ft8PlaetzeStand st;
    st.plaetzeGesamt = m_plaetze;
    st.sendet        = m_sendet;

    auto ausAblauf = [this](const Ft8Ablauf& a, int tonHz) {
        Ft8PlatzStand z;
        z.partner   = a.partner();
        z.land      = z.partner.isEmpty() ? QString() : landFuer(z.partner);
        z.tonHz     = tonHz;
        z.zug       = zugKurz(a.zustand());
        z.gesendet  = a.gesendeterReport();
        z.empfangen = a.empfangenerReport();
        z.stufe     = stufeFuer(a.zustand());
        return z;
    };

    if (mehrfachAn()) {
        for (const Ft8Mehrfach::Belegung& b : m_mehrfach.belegung()) {
            Ft8PlatzStand z;
            z.partner = b.partner;
            z.land    = z.partner.isEmpty() ? QString() : landFuer(z.partner);
            z.tonHz   = int(b.toneHz);
            z.stufe   = stufeFuer(b.zustand);
            z.zug     = zugKurz(b.zustand);
            st.plaetze.append(z);
        }
    }
    // Der CQ-Ruf ist ein Platz, kein zweiter Ablauf -- genau wie beim
    // Senden (siehe `sendeMehrfach`). Darum steht er in derselben
    // Liste und nicht daneben.
    if (m_ablauf.laeuft()) {
        st.plaetze.append(ausAblauf(m_ablauf, m_sendeTonHz));
    }

    const qint64 jetzt = QDateTime::currentMSecsSinceEpoch();
    for (const Ft8Warteschlange::Wartend& w : m_schlange.alle()) {
        if ((jetzt - w.seitMs) >= Ft8Warteschlange::kHaltbarkeitMs) {
            continue;
        }
        Ft8WartenderStand ws;
        ws.partner = w.partner;
        ws.land    = landFuer(w.partner);
        ws.snr     = w.snr;
        st.wartende.append(ws);
    }
    return st;
}

void Ft8Applet::meldeStand()
{
    // Dieselbe Stelle fuer beide Ansichten der Schlange: das
    // Plaetze-Panel bekommt den Stand, die QSO-Karte zeichnet „THEN".
    zeigeDann();
    emit plaetzeGeaendert(baueStand());

    // Der Partner fuer die Rotor/Log-Karte (2026-11-18). Hier und nicht
    // an den Stellen, an denen er gesetzt wird: das sind fuenf (Doppel-
    // klick, Karte, Antwort auf mein CQ, Schlange, Hound), und jede
    // endet ohnehin in diesem Aufruf. Mit Plaetzen fuehrt `m_ablauf`
    // nur den CQ-Ruf -- dort bleibt es vorerst still.
    const QString partner = mehrfachAn() ? QString() : m_ablauf.partner();
    if (partner != m_gemeldeterPartner) {
        m_gemeldeterPartner = partner;
        emit partnerGewechselt(partner);
    }
}

// Der Absender einer FT8-Nachricht.
//
// Zwei Formen reichen fuer alles, was im Durchlassbereich steht:
//   „CQ OE5SOS JN78"       — Anruf, der Absender steht hinter CQ
//   „CQ DX OE5SOS JN78"    — mit Zusatz; dann eins weiter
//   „DL1ABC OE5SOS -12"    — Antwort: erst der Angerufene, dann der
//                             Absender
// In beiden Faellen ist der Absender das ERSTE Feld, das wie ein
// Rufzeichen aussieht und nicht an erster Stelle steht — ausser bei
// CQ, wo das erste Feld der Anruf selbst ist.
// Ein Rufzeichen aus spitzen Klammern schaelen.
//
// ── Warum es die braucht (2026-11-04) ───────────────────────────────
//
// Im EU-VHF-Contest (Nachrichtentyp 5) stehen BEIDE Rufzeichen nur als
// Hash im Paket, und ft8_lib schreibt sie — wie WSJT-X — in spitzen
// Klammern heraus: `<OE1ABC> <OE5SOS> 590003 IO91NP`. Ohne das
// Abstreifen kostete das an zwei Stellen gleichzeitig:
//
//   * `anMich` verglich `<OE5SOS>` mit `OE5SOS` und sagte nie ja —
//     im Contest haette der Betreiber nicht gesehen, dass er gerufen
//     wird, obwohl die Zeile vor ihm stand.
//   * `senderCallsign` lehnte die geklammerten Felder ab und nahm
//     stattdessen `IO91NP` (Buchstaben, Ziffern, endet auf Buchstabe
//     — die Pruefung passt leider). Der Locator wurde damit fuer das
//     Rufzeichen gehalten, und die DXCC-Einfaerbung schlug auf ein
//     Land, das niemand gerufen hatte.
//
// Die Klammern selbst sind kein Fehler: sie SAGEN etwas, naemlich
// „gehasht, aus der Tabelle aufgeloest". Darum werden sie nur fuer
// den Vergleich abgestreift, nicht aus der angezeigten Zeile.
static QString ohneSpitzeKlammern(const QString& feld)
{
    if (feld.size() >= 3
        && feld.startsWith(QLatin1Char('<'))
        && feld.endsWith(QLatin1Char('>'))) {
        return feld.mid(1, feld.size() - 2);
    }
    return feld;
}

QString Ft8Applet::senderCallsign(const QString& message)
{
    const QStringList felder =
        message.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (felder.isEmpty()) { return {}; }

    // Wie ein Rufzeichen: drei bis elf Zeichen, mindestens eine
    // Ziffer UND ein Buchstabe, und die letzte Stelle KEINE Ziffer.
    // Die letzte Bedingung ist die wichtige: ohne sie gilt auch der
    // Feldkenner (JN78, FN20) als Rufzeichen, und bei „CQ DX OE5SOS
    // JN78" zaehlt nur, welches Feld zuerst passt. „DX", „-12",
    // „RR73" und „73" fallen ohnehin heraus.
    const auto wieRufzeichen = [](const QString& f) {
        if (f.size() < 3 || f.size() > 11) { return false; }
        bool ziffer = false;
        bool buchstabe = false;
        for (const QChar c : f) {
            if (c.isDigit()) { ziffer = true; }
            else if (c.isLetter()) { buchstabe = true; }
            else if (c != QLatin1Char('/')) { return false; }
        }
        if (!ziffer || !buchstabe) { return false; }
        if (f.back().isDigit()) { return false; }

        // ── Und der SECHSstellige Feldkenner auch nicht (2026-11-04)
        //
        // Die Regel „letzte Stelle keine Ziffer" faengt JN78, aber
        // nicht IO91NP: Buchstaben, Ziffern, endet auf einen
        // Buchstaben — die Pruefung darueber passt. Aufgefallen am
        // EU-VHF-Contest, wo `<...> <...> 590003 IO91NP` als
        // Rufzeichen „IO91NP" lieferte und die DXCC-Einfaerbung auf
        // ein Land schlug, das niemand gerufen hatte. Der Fehler war
        // aber schon vorher da — jede Nachricht mit sechsstelligem
        // Locator und ohne lesbares Rufzeichen traf ihn.
        //
        // Das Muster ist Buchstabe Buchstabe Ziffer Ziffer Buchstabe
        // Buchstabe. Ehrlich gesagt: ein Sonderrufzeichen wie
        // „OE25AB" sieht genauso aus und faellt hier mit heraus. Das
        // ist der kleinere Schaden — es kommt selten vor, und wer es
        // fuehrt, steht in derselben Nachricht meist noch an einer
        // zweiten Stelle.
        if (f.size() == 6
            && f.at(0).isLetter() && f.at(1).isLetter()
            && f.at(2).isDigit()  && f.at(3).isDigit()
            && f.at(4).isLetter() && f.at(5).isLetter()) {
            return false;
        }
        return true;
    };

    // Erst ab Feld 1: an Stelle 0 steht der Anruf („CQ") oder der
    // ANGERUFENE, nie der Absender.
    for (int i = 1; i < felder.size(); ++i) {
        // Erst schaelen: `<OE5SOS>` ist ein Rufzeichen, nur gehasht
        // uebertragen. Siehe `ohneSpitzeKlammern` oben.
        const QString kern = ohneSpitzeKlammern(felder.at(i));
        if (wieRufzeichen(kern)) { return kern; }
    }
    return {};
}

void Ft8Applet::onDecoded(const QVector<Longpath::Ft8Decode>& funde,
                          const QDateTime& periodStart)
{
    m_lastPeriodCount = funde.size();

    // ── Die Uhr mitlesen (2026-11-13) ───────────────────────────
    //
    // Jeder Fund traegt seinen Zeitversatz im Fenster. Einzeln sagt
    // er nichts ueber die EIGENE Uhr -- die Gegenstation kann
    // ebensogut danebenliegen. Ueber Funde und Perioden hinweg
    // bleibt nur der gemeinsame Anteil stehen, und der ist der
    // eigene. `setzeFt4` zuerst: ein Protokollwechsel leert, was
    // zum alten Fenster gehoerte.
    m_uhrversatz.setzeFt4(protocol() == Ft8Decoder::Protocol::Ft4);
    for (const Ft8Decode& d : funde) {
        m_uhrversatz.merke(double(d.dtSeconds));
    }
    m_uhrversatz.periodeFertig();
    zeigeUhrversatz();

    updateStatus();
    zeigeFundZahl();
    if (funde.isEmpty()) { return; }

    const QString uhrzeit = periodStart.toUTC().toString(QStringLiteral("HHmmss"));
    const QString modus = protocol() == Ft8Decoder::Protocol::Ft4
        ? QStringLiteral("FT4") : QStringLiteral("FT8");
    // Die Wahlfrequenz der Scheibe liegt in Hz; der Fund steht als
    // Tonlage IM Durchlassbereich. Erst zusammen ergeben sie die
    // Frequenz, unter der das Rufzeichen im Logbuch steht.
    //
    // Und das Vorzeichen haengt am Seitenband: in DIGU (so wird FT8
    // ueblicherweise gehoert) liegt der Ton OBERHALB der Wahlfrequenz,
    // in DIGL darunter. Ohne diese Unterscheidung stuende ein
    // DIGL-Fund um bis zu 3 kHz daneben.
    const double wahlHz = m_slice ? m_slice->frequency() : 0.0;
    const bool unteresSeitenband =
        m_slice && m_slice->dspMode() == DSPMode::DIGL;

    for (const Ft8Decode& d : funde) {
        // Spalten wie in WSJT-X „Band Activity", dahinter das Land.
        const QString zeile = zeilentext(
            uhrzeit,
            static_cast<int>(std::lround(d.snrDb)),
            d.dtSeconds,
            static_cast<int>(std::lround(d.freqHz)),
            d.text,
            landFuer(senderCallsign(d.text)));
        zeigeZeile(zeile, d.text,
                   static_cast<int>(std::lround(d.freqHz)));

        // Der Fund geht an EINE Stelle: RadioModel verteilt ihn weiter
        // (Panadapter-Ueberlagerung ueber SpotModel, der Sammler
        // RxDecodeModel, und PSK Reporter, wenn er laeuft) — genau wie
        // beim WSJT-X-Weg. Vorher schrieb dieses Applet selbst ins
        // RxDecodeModel und erreichte die Ueberlagerung nie
        // (2026-10-15).
        // Bringt der Fund den Ablauf weiter, ist der naechste Zug
        // faellig — das ist der einzige Weg, auf dem er vorankommt.
        const int snr = static_cast<int>(std::lround(d.snrDb));

        // ── Multi Answering (2026-10-30 angeschlossen) ─────────
        //
        // Mit mehreren Plaetzen fuehrt `Ft8Mehrfach` die Ablaeufe, und
        // jeder gehoerte Satz geht an ALLE -- jeder prueft selbst, ob
        // er ihn angeht. Ruft mich jemand, der noch nicht dabei ist,
        // bekommt er einen freien Platz oder stellt sich an.
        if (mehrfachAn()) {
            m_mehrfach.setEigeneDaten(eigenesRufzeichen(), eigenerLocator());
            bool bewegt = m_mehrfach.verarbeite(d.text, snr) > 0;
            // ── Wer als Anrufer zaehlt (2026-11-17) ──────────────────
            //
            // Hier stand: mein Rufzeichen kommt IRGENDWO im Satz vor.
            // Damit bekam auch der einen neuen Platz, der sich eben
            // verabschiedet hatte -- sein „73" nach dem fertigen QSO
            // machte ihn zum Anrufer, und er wurde gleich noch einmal
            // gearbeitet (am Geraet: EY7AD dreimal in zwei Minuten).
            // Der einfache Weg hatte die richtige Regel laengst; jetzt
            // gilt sie fuer beide.
            if (!bewegt) { bewegt = nimmAnruferAufPlatz(d.text, snr); }
            if (bewegt) { ablaufSchritt(); }
        } else if (m_ablauf.laeuft() && m_ablauf.verarbeite(d.text, snr)) {
            ablaufSchritt();
        } else {
            // Er ruft mich, aber gerade geht es nicht: ein QSO laeuft,
            // oder der Ablauf ruht. Dann merken statt fallen lassen.
            merkeAnrufFallsNoetig(d.text, snr,
                                  static_cast<int>(std::lround(d.freqHz)));
        }

        const QString ruf = senderCallsign(d.text);
        if (!ruf.isEmpty()) {
            m_letzterRapport.insert(ruf,
                                    static_cast<int>(std::lround(d.snrDb)));
        }
        if (m_model && !ruf.isEmpty()) {
            DxSpot spot;
            spot.dxCall   = ruf;
            spot.freqMhz  = (wahlHz + (unteresSeitenband ? -d.freqHz
                                                          : d.freqHz))
                            / 1.0e6;
            spot.snr      = static_cast<int>(std::lround(d.snrDb));
            // Der Modus steht im Kommentar: SpotTableModel::extractMode
            // liest ihn dort heraus, so wie bei den WSJT-X-Funden.
            spot.comment  = QStringLiteral("%1  %2").arg(modus, d.text);
            // Nicht „WSJT-X": das ist eine andere Quelle und wuerde die
            // beiden in der Liste vermischen.
            spot.source   = QStringLiteral("Longpath");
            spot.spotterCall = QStringLiteral("Longpath");
            spot.utcTime  = periodStart.toUTC().time();
            m_model->onNativeFt8Decode(spot);
        }

        // ── An die QSO-Karte (2026-11-11) ─────────────────────
        //
        // Jede Station mit Rufzeichen, auch ohne Locator: wer vorhin
        // mit Feld gehoert wurde und jetzt einen Rapport schickt, ist
        // noch da -- die Karte frischt ihn auf. Die ZEILE geht mit,
        // weil ein Klick dort genau diese Zeile beantwortet.
        if (!ruf.isEmpty()) {
            const double hz = wahlHz + (unteresSeitenband ? -double(d.freqHz)
                                                          : double(d.freqHz));
            Ft8Gehoert g;
            g.rufzeichen = ruf;
            g.locator    = locatorAusNachricht(d.text);
            g.snrDb      = snr;
            g.dtSek      = d.dtSeconds;
            g.tonHz      = static_cast<int>(std::lround(d.freqHz));
            g.freqMHz    = hz / 1.0e6;
            g.band       = bandFuerLog(hz);
            g.modus      = modus;
            g.zeit       = periodStart.toUTC();
            g.nachricht  = d.text;
            g.zeile      = zeile;
            g.land       = landFuer(ruf);
            g.neuesLand  = istNeuesLand(ruf);
            emit gehoert(g);
        }
    }

    if (m_lineCount > kMaxLines) {
        const int zuviel = m_lineCount - kMaxLines;
        auto cursor = m_list->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, zuviel);
        cursor.removeSelectedText();
        m_lineCount = kMaxLines;
    }

    m_list->moveCursor(QTextCursor::End);
    m_list->ensureCursorVisible();

    // Wo in dieser Periode jemand lag -- fuer die Stellensuche.
    QVector<int> dieserPeriode;
    dieserPeriode.reserve(funde.size());
    for (const Ft8Decode& d : funde) {
        dieserPeriode.append(static_cast<int>(std::lround(d.freqHz)));
    }
    m_toeneJePeriode.append(dieserPeriode);
    while (m_toeneJePeriode.size() > kPeriodenImGedaechtnis) {
        m_toeneJePeriode.removeFirst();
    }
}


// ── Eine Zeile in die Liste, gefiltert und gefaerbt ─────────────────
//
// Drei Sorten, nach Dringlichkeit:
//
//   1. Die Nachricht enthaelt das EIGENE Rufzeichen -- jemand ruft
//      dich. Gruen und fett; das ist die einzige Zeile, die man nicht
//      uebersehen darf.
//   2. Die Nachricht faengt mit „CQ" an -- jemand ist rufbar. Bernstein.
//   3. Alles andere -- ein fremdes QSO. Die gewoehnliche Textfarbe.
//
// Eingefaerbt wird ueber einen Cursor mit Zeichenformat, nicht ueber
// appendHtml(): die Liste steht in fester Schrittweite, und HTML
// faltet mehrfache Leerzeichen zusammen -- die Spalten (Zeit, dB, DT,
// Hz) stuenden danach nicht mehr untereinander.
Ft8Applet::Zeilenart Ft8Applet::letzteZeilenartForTest() const
{
    if (m_zeilen.isEmpty()) { return Zeilenart::Gewoehnlich; }
    const Fundzeile& z = m_zeilen.last();
    return zeilenart(z.anMich, z.neuesLand, z.neuesGrid, z.neuesGridBand,
                     z.istCq);
}

Ft8Applet::Zeilenart Ft8Applet::zeilenart(bool anMich, bool neuesLand,
                                         bool neuesGrid,
                                         bool neuesGridBand, bool istCq)
{
    if (anMich)    { return Zeilenart::AnMich; }
    if (neuesLand) { return Zeilenart::NeuesLand; }
    // Zwischen Land und CQ: ein fehlendes Land ist mehr wert als ein
    // fehlendes Feld, und beides mehr als ein beliebiges CQ.
    if (neuesGrid) { return Zeilenart::NeuesGrid; }
    // Darunter die schwaechere Stufe (2026-11-12): das Feld ist im
    // Logbuch, aber nicht auf diesem Band. Sie steht VOR dem CQ --
    // ein CQ ist nur ein Angebot, ein fehlendes Band ist ein Grund.
    if (neuesGridBand) { return Zeilenart::NeuesGridBand; }
    if (istCq)     { return Zeilenart::Cq; }
    return Zeilenart::Gewoehnlich;
}

// Siehe Ft8Applet.h.
QString Ft8Applet::zeilentext(const QString& uhrzeit, int snrDb,
                              double dtSekunden, int hz,
                              const QString& nachricht,
                              const QString& land)
{
    // Breite der Nachrichtenspalte. 18 ist gemessen, nicht geraten:
    // die laengste uebliche FT8-Nachricht ist ein Ruf mit Praefix und
    // Suffix samt Feld („ON6LEO F5OYA/P JN13" = 19), die allermeisten
    // bleiben darunter. Wer laenger ist, schiebt die Landspalte --
    // das faellt auf und schneidet nichts ab.
    constexpr int kNachrichtBreite = 19;

    const QString spalten = QStringLiteral("%1 %2 %3 %4  %5")
                                .arg(uhrzeit)
                                .arg(snrDb, 4)
                                .arg(dtSekunden, 5, 'f', 1)
                                .arg(hz, 5)
                                .arg(nachricht);
    if (land.isEmpty()) { return spalten; }

    // Nur auffuellen, wenn die Nachricht kuerzer ist -- `leftJustified`
    // mit `truncate=false` laesst Laengeres stehen.
    const QString breit = nachricht.leftJustified(kNachrichtBreite, QLatin1Char(' '));
    return QStringLiteral("%1 %2 %3 %4  %5  %6")
        .arg(uhrzeit)
        .arg(snrDb, 4)
        .arg(dtSekunden, 5, 'f', 1)
        .arg(hz, 5)
        .arg(breit, land);
}

// ── Die Spaltenkoepfe (2026-10-30) ───────────────────────────────────
//
// Betreiber: „die staerke des signals sollte auch in einer eigenen
// spalten sein." Sie IST eine -- die zweite. Ohne Ueberschrift ist
// nur nicht zu sehen, welche der vier Zahlen welche ist:
//
//     191845    8    1.0   2406   LZ1ZF SP4DIS KO13   Poland
//
// WSJT-X schreibt sie ueber die Liste („UTC  dB  DT  Freq  Message"),
// und genau dort hat er sie gesucht. Gebaut aus DENSELBEN Breiten wie
// `zeilentext` -- zwei Stellen mit eigenen Zahlen waeren beim ersten
// Spaltenwechsel auseinandergelaufen.
QString Ft8Applet::spaltenkopf()
{
    // `zeilentext` baut "%1 %2 %3 %4  %5": Uhrzeit, dann vier Stellen
    // fuer dB, fuenf fuer DT, fuenf fuer Hz.
    return QStringLiteral("%1 %2 %3 %4  %5")
        .arg(QStringLiteral("UTC   "))
        .arg(tr("dB"), 4)
        .arg(tr("DT"), 5)
        .arg(tr("Freq"), 5)
        .arg(tr("Message"));
}

// Bis zum 2026-11-11 stand diese Abfrage nur in zeigeZeile(). Jetzt
// fragt auch die Meldung an die Karte -- dieselbe Antwort, darum EINE
// Stelle.
bool Ft8Applet::istNeuesLand(const QString& senderRuf) const
{
    if (senderRuf.isEmpty() || !m_model || !m_model->dxccColorProvider()) {
        return false;
    }
    auto* dxcc = m_model->dxccColorProvider();
    if (!dxcc->isEnabled()) { return false; }
    const double mhz = (m_slice ? m_slice->frequency() : 0.0) / 1.0e6;
    const QString modus = protocol() == Ft8Decoder::Protocol::Ft4
                              ? QStringLiteral("FT4")
                              : QStringLiteral("FT8");
    return dxcc->statusForSpot(senderRuf, mhz, modus) == DxccStatus::NewDxcc;
}

QString Ft8Applet::landFuer(const QString& rufzeichen) const
{
    if (rufzeichen.isEmpty() || !m_model) { return {}; }
    auto* dxcc = m_model->dxccColorProvider();
    if (!dxcc) { return {}; }
    // Absichtlich NICHT an `isEnabled()` gebunden: der Schalter gehoert
    // der Faerbung. Den Namen des Landes zu lesen, ist keine Farbe --
    // wer die Farben abschaltet, will deswegen nicht wissen, wen er
    // hoert, und umgekehrt.
    const QString praefix = dxcc->ctyDat().resolvePrimaryPrefix(rufzeichen);
    if (praefix.isEmpty()) { return {}; }
    const DxccEntity* e = dxcc->ctyDat().entityByPrefix(praefix);
    return e ? e->name : QString();
}

// ── Was ICH gesendet habe, in die rechte Spalte ──────────────────────
//
// Betreiber am 2026-10-22: „sehe ich auch rechts, wenn ein qso zu
// stande kommt." Die Antworten an ihn standen dort schon -- jede
// Zeile mit dem eigenen Rufzeichen geht nach rechts, egal auf welcher
// Tonlage. Was fehlte, war die eigene Haelfte des Gespraechs: ein QSO
// laeuft jetzt von selbst ab, und ohne die eigenen Zuege steht rechts
// nur, was der andere sagt.
//
// Wie in WSJT-X: in die rechte Spalte, nicht in die linke -- links
// steht, was auf dem BAND los ist, und das eigene Signal gehoert
// nicht dazu. Die Spalten sind dieselben, nur steht „Tx" an der
// Stelle des Rapports und die Laufzeit bleibt leer; beides hat bei
// einer eigenen Sendung keinen Wert.
//
// Die Farbe ist das Messing der Sendetaste (kTxKeyBorder), nicht
// Rot: „kein kraeftiges Rot als Zustandsfarbe" gilt hier wie dort.
void Ft8Applet::zeigeEigeneSendung(const QString& nachricht)
{
    if (nachricht.trimmed().isEmpty()) { return; }

    Fundzeile z;
    z.text = QStringLiteral("%1 %2 %3 %4  %5")
                 .arg(QDateTime::currentDateTimeUtc()
                          .toString(QStringLiteral("hhmmss")))
                 .arg(tr("Tx"), 4)
                 .arg(QString(), 5)
                 .arg(m_sendeTonHz, 5)
                 .arg(nachricht.trimmed());
    z.nachricht     = nachricht.trimmed();
    z.tonHz         = m_sendeTonHz;
    z.eigeneSendung = true;
    merkeZeile(z);
}

// ── Eine Zeile merken und, wenn sie in die Sicht gehoert, zeigen ────
void Ft8Applet::merkeZeile(const Fundzeile& z)
{
    m_zeilen.append(z);
    if (m_zeilen.size() > kMaxZeilen) {
        m_zeilen.remove(0, m_zeilen.size() - kMaxZeilen);
        // Beim Abschneiden muss die ganze Liste neu, sonst stuenden
        // Zeilen darin, die es im Speicher nicht mehr gibt.
        zeichneListe();
        return;
    }
    if (inSicht(z)) {
        haengeAn(z);
        ++m_lineCount;
        if (m_list) {
            m_list->moveCursor(QTextCursor::End);
            m_list->ensureCursorVisible();
        }
    }
    zeigeFundZahl();
}

bool Ft8Applet::inSicht(const Fundzeile& z) const
{
    // Die eigene Sendung steht ueberall: sie IST der Zusammenhang,
    // in dem die Antwort gelesen wird.
    if (z.eigeneSendung) { return true; }
    switch (m_sicht) {
    case Sicht::Alles:  break;
    case Sicht::AnMich: return z.anMich;
    case Sicht::Cq:     return z.istCq || z.anMich;
    case Sicht::NeuesLand: return z.neuesLand || z.anMich;
    // Nur ganz fehlende Felder (2026-11-13).
    //
    // Der Filter zeigte bis heute beide Stufen: ganz fehlende Felder UND
    // solche, die nur auf diesem Band noch fehlen. Begruendung damals:
    // „wer die Jagd einschaltet, will das Band voll sehen."
    //
    // Betreiber am Geraet: „hier sind einige grids, die sicher schon
    // hatte -- hier sollen nur jene stehen, die ich noch nicht erreicht
    // habe." An seinen Zahlen nachgerechnet, auf 20 m: JN79 (hat er auf
    // 15 m, 2 m, 40 m), KP30 (15 m, 6 m), KO26 (10 m, 15 m, 6 m, 80 m)
    // -- alle drei standen als „neu" da, und keines davon ist neu.
    //
    // Die zweite Stufe ist damit nicht weg, nur nicht mehr in diesem
    // Filter: ihre Farbe bleibt und ist in „All" zu sehen. Eine Jagd
    // nach fehlenden Feldern ist eine Liste dessen, was fehlt.
    case Sicht::NeuesGrid:
        return z.neuesGrid || z.anMich;
    case Sicht::MeineFrequenz:
        // Dieselbe Regel wie frueher die rechte Spalte: das Fenster
        // ist die Signalbreite von FT8/FT4 (50 Hz), kein gegriffener
        // Wert. Schmaler, und ein Fund am Rand des eigenen Signals
        // fehlt; breiter, und es fuellt sich mit Nachbarn.
        return z.anMich || std::abs(z.tonHz - m_sendeTonHz) <= kRxFensterHz;
    }
    // „Alles" heiszt alles. Bis zum 2026-10-30 lagen hier noch zwei
    // Haken aus der Kopfzeile darueber, und zwei Filtersysteme
    // nebeneinander ergaben Faelle, die niemand im Kopf behaelt.
    //
    // Die Regel, die bleibt: eine Zeile, die an MICH geht, kommt
    // durch jede Sicht. Wer gerufen wird und es nicht sieht, weil ein
    // Filter sitzt, verliert ein QSO -- das ist kein Filter mehr,
    // sondern ein Fehler. Darum steht `|| z.anMich` in jedem Fall
    // oben.
    return true;
}

void Ft8Applet::haengeAn(const Fundzeile& z)
{
    if (!m_list) { return; }
    QTextCharFormat f;
    if (z.eigeneSendung) {
        // Die Farbe ist das Messing der Sendetaste, nicht Rot: „kein
        // kraeftiges Rot als Zustandsfarbe" gilt hier wie dort.
        f.setForeground(QColor(QString::fromLatin1(Style::kTxKeyBorder)));
        f.setFontWeight(QFont::Bold);
    } else {
        const Zeilenart art = zeilenart(z.anMich, z.neuesLand,
                                        z.neuesGrid, z.neuesGridBand,
                                        z.istCq);
        if (art == Zeilenart::AnMich) {
            f.setForeground(QColor(QString::fromLatin1(Style::kGreenText)));
            f.setFontWeight(QFont::Bold);
        } else if (art == Zeilenart::NeuesLand) {
            // Fett wie „an mich": ein neues Land uebersieht man sonst
            // in zwanzig Zeilen je Fenster.
            f.setForeground(z.landFarbe.isValid()
                                ? z.landFarbe
                                : QColor(QString::fromLatin1(
                                      Style::kAmberText)));
            f.setFontWeight(QFont::Bold);
        } else if (art == Zeilenart::NeuesGrid) {
            // ── Blau fuer ein Grossfeld, das fehlt (2026-10-30) ──
            //
            // Betreiber: „interessant waere zu sehen, welche grids ich
            // noch nicht habe und diese dann zb blau zu markieren" --
            // „somit sehe ich dann, was ich noch benoetige."
            //
            // Die Farbe ist seine Wahl. Im Hausstil ist Blau sonst
            // „anfassbar" und Warm „gemessen"; hier traegt es eine
            // Auskunft. Das ist eine bewusste Ausnahme und keine
            // Nachlaessigkeit: er hat sie genannt, und sie trennt die
            // Felder sauber vom Bernstein der Laender und CQs.
            //
            // NICHT fett: ein neues Land ist mehr wert als ein neues
            // Feld, und wenn beides fett waere, hoben sich die
            // Stufen gegenseitig auf.
            f.setForeground(QColor(QString::fromLatin1(Style::kAccent)));
        } else if (art == Zeilenart::NeuesGridBand) {
            // ── Keine eigene Farbe mehr (2026-11-17) ───────────────
            //
            // Hier stand dasselbe Blau, um ein Drittel dunkler: „das
            // Feld fehlt nur auf DIESEM Band". Am Geraet, 17 m:
            // 9A7BB JN75 war blau, und der Betreiber hatte JN75 auf
            // 15 m und 40 m im Log. „jn75 habe ich schon gearbeitet,
            // ist aber hellblau" -- und auf die Erklaerung: „ich will
            // eine farbe haben, die mir zeigt, welche locator ich noch
            // nicht gearbeitet habe."
            //
            // Zwei Blaus, die man nur nebeneinander unterscheiden
            // kann, sind eine Farbe mit zwei Bedeutungen. Blau heisst
            // jetzt genau eines: dieses Feld fehlt ganz. Die Bandstufe
            // bleibt gerechnet (`Zeilenart::NeuesGridBand`, die Karte
            // im Logbuch faerbt je Band), nur die Liste zeigt sie nicht
            // mehr -- die Zeile sieht aus wie jede andere ihrer Art.
            f.setForeground(QColor(QString::fromLatin1(
                z.istCq ? Style::kAmberText : Style::kTextPrimary)));
        } else if (art == Zeilenart::Cq) {
            f.setForeground(QColor(QString::fromLatin1(Style::kAmberText)));
        } else {
            f.setForeground(QColor(QString::fromLatin1(Style::kTextPrimary)));
        }
    }
    QTextCursor c(m_list->document());
    c.movePosition(QTextCursor::End);
    if (!m_list->document()->isEmpty()) { c.insertBlock(); }
    c.insertText(z.text, f);
    // Ans Ende der Zeile zu schreiben schiebt die Ansicht mit -- dann
    // steht die Uhrzeit links ausserhalb des Fensters, und man sieht
    // die Spalte, die man am wenigsten braucht. Gesehen im
    // gerenderten Blatt am 2026-10-21.
}

// Die ganze Liste aus dem Speicher. Laeuft bei jedem Pillendruck --
// vierhundert Zeilen sind dafuer schnell genug, und alles andere
// hiesse, zwei Wahrheiten zu fuehren.
void Ft8Applet::zeichneListe()
{
    if (m_sichtAlles)  { m_sichtAlles->setChecked(m_sicht == Sicht::Alles); }
    if (m_sichtAnMich) { m_sichtAnMich->setChecked(m_sicht == Sicht::AnMich); }
    if (m_sichtCq)     { m_sichtCq->setChecked(m_sicht == Sicht::Cq); }
    if (m_sichtNeuesLand) { m_sichtNeuesLand->setChecked(
                                m_sicht == Sicht::NeuesLand); }
    if (m_sichtFreq)   { m_sichtFreq->setChecked(m_sicht
                                                 == Sicht::MeineFrequenz); }
    if (!m_list) { return; }
    m_list->clear();
    m_lineCount = 0;
    for (const Fundzeile& z : m_zeilen) {
        if (!inSicht(z)) { continue; }
        haengeAn(z);
        ++m_lineCount;
    }
    m_list->moveCursor(QTextCursor::End);
    m_list->ensureCursorVisible();
    zeigeFundZahl();
}

// ── Die Uhr, wenn sie falsch geht ────────────────────────────────────
//
// Sie stellt nichts. Sie sagt nur, was sie gemessen hat, und was das
// bedeutet -- in dieser Reihenfolge, damit ein Betreiber, der seine
// Uhr fuer richtig haelt, die Zahl pruefen kann statt dem Urteil
// glauben zu muessen.
void Ft8Applet::zeigeUhrversatz()
{
    if (!m_uhrWarnung) { return; }

    const auto lage = m_uhrversatz.lage();
    if (lage == Ft8Uhrversatz::Lage::Unbekannt
        || lage == Ft8Uhrversatz::Lage::Ok) {
        // Im Normalfall steht hier NICHTS. Eine Zeile, die immer da
        // ist, liest nach drei Tagen niemand mehr -- und genau dann
        // wuerde sie gebraucht.
        m_uhrWarnung->setVisible(false);
        m_uhrWarnung->clear();
        return;
    }

    const double v = m_uhrversatz.versatzSek();
    const QString zahl = QStringLiteral("%1%2 s")
                             .arg(v >= 0.0 ? QStringLiteral("+")
                                           : QStringLiteral("\u2212"))
                             .arg(std::abs(v), 0, 'f', 1);
    m_uhrWarnung->setText(
        lage == Ft8Uhrversatz::Lage::Schlecht
            ? tr("clock %1 \u2014 decodes are being lost").arg(zahl)
            : tr("clock %1").arg(zahl));
    m_uhrWarnung->setToolTip(
        tr("Median time offset of the last %1 periods: %2.\n\n"
           "The decoder only searches a narrow window around zero "
           "(%3 s for %4). A clock that is off does not kill reception "
           "at once \u2014 it takes the weak decodes first, then whole "
           "periods. From the inside that looks like a dead band.\n\n"
           "This is your computer's clock, not the radio's. Longpath "
           "does not change it.")
            .arg(m_uhrversatz.periodenGezaehlt())
            .arg(zahl)
            .arg(m_uhrversatz.istFt4()
                     ? QStringLiteral("\u22120.48 \u2026 +0.91")
                     : QStringLiteral("\u22121.60 \u2026 +3.04"))
            .arg(m_uhrversatz.istFt4() ? QStringLiteral("FT4")
                                       : QStringLiteral("FT8")));
    m_uhrWarnung->setVisible(true);
}

void Ft8Applet::zeigeFundZahl()
{
    if (!m_fundZahl) { return; }
    // Zwei Zahlen, und sie sind NICHT dasselbe: wie viel in der Liste
    // steht, und was die letzte Periode gebracht hat. Beim Aufraeumen
    // am 2026-10-30 habe ich sie erst fuer eine Dopplung gehalten --
    // sie standen im Bild des Betreibers nur zufaellig gleich, weil
    // die Liste frisch war. Jetzt stehen sie nebeneinander, statt an
    // zwei Enden des Panels.
    const QString liste = m_sicht == Sicht::Alles
                              ? tr("%1 decodes").arg(m_zeilen.size())
                              : tr("%1 of %2").arg(m_lineCount)
                                    .arg(m_zeilen.size());
    QString text = m_lastPeriodCount < 0
                       ? liste
                       : QStringLiteral("%1 \u00b7 %2")
                             .arg(liste,
                                  tr("%1 last").arg(m_lastPeriodCount));

    // ── Wer wartet, muss sichtbar sein ──────────────────────
    //
    // Am 2026-10-30 im Betrieb aufgefallen, eine Minute nachdem die
    // Warteschlange zum ersten Mal lief:
    //
    //   16:09:00  Anruf gemerkt: F5PZT  — wartet als Nummer 1
    //   16:09:00  Anruf gemerkt: OE3TVJ — wartet als Nummer 2
    //
    // Beide standen im Protokoll und nirgends sonst. Das Plaetze-Panel
    // zeigt sie, aber das ist ab Werk aus -- wer es nicht geoeffnet
    // hat, sieht nicht, dass zwei Stationen auf ihn warten. Eine
    // Warteschlange, die niemand sieht, ist keine.
    // ── Wie viele QSOs laufen (2026-10-30) ──────────────────
    //
    // Betreiber: „so sehe ich nicht, ob und wie viele qso gleich
    // ablaufen und welches wie weit sie sind."
    //
    // Das Plaetze-Panel zeigt beides ausfuehrlich, ist aber nicht
    // immer offen. Die ZAHL gehoert darum auch hierher -- sie
    // beantwortet „laeuft ueberhaupt was" ohne ein zweites Fenster.
    // Wie weit jedes ist, bleibt dem Panel: fuenf Striche je QSO
    // passen in keine Kopfzeile.
    const int laufend = mehrfachAn()
                            ? m_mehrfach.aktive()
                            : (m_ablauf.laeuft()
                               && !m_ablauf.partner().isEmpty() ? 1 : 0);
    if (laufend > 0) {
        text += QStringLiteral(" \u00b7 ")
                + (laufend == 1 ? tr("1 QSO") : tr("%1 QSOs").arg(laufend));
    }

    const int warten =
        m_schlange.wartende(QDateTime::currentMSecsSinceEpoch());
    if (warten > 0) {
        text += QStringLiteral(" \u00b7 ") + tr("%1 waiting").arg(warten);
    }
    m_fundZahl->setText(text);
}

void Ft8Applet::zeigeZeile(const QString& zeile, const QString& nachricht,
                           int tonHz)
{
    Fundzeile z;
    z.text      = zeile;
    z.nachricht = nachricht;
    z.tonHz     = tonHz;

    const QString ruf = eigenesRufzeichen();
    // Wortgrenzen, nicht contains(): „OE5SOS" steckt auch in
    // „OE5SOSX", und das ist eine andere Station.
    // Geschaelt vergleichen: im Contest steht das eigene Rufzeichen
    // als `<OE5SOS>` da (siehe `ohneSpitzeKlammern`).
    z.anMich = false;
    if (!ruf.isEmpty()) {
        const QStringList felder = nachricht.toUpper().split(
            QLatin1Char(' '), Qt::SkipEmptyParts);
        for (const QString& f : felder) {
            if (ohneSpitzeKlammern(f) == ruf) { z.anMich = true; break; }
        }
    }
    z.istCq = nachricht.trimmed().toUpper().startsWith(
        QStringLiteral("CQ"));

    // Ein Land, das noch nie im Logbuch stand? Der Aufloeser steht
    // schon im Modell -- derselbe, der die Funde im Panadapter
    // einfaerbt. Also DIESELBE Palette, keine zweite erfunden.
    //
    // Steht hier und nicht in `inSicht`: die Abfrage kostet, und mit
    // dem Zeilenspeicher wuerde sie sonst bei jedem Pillendruck fuer
    // vierhundert Zeilen neu laufen.
    const QString senderRuf = senderCallsign(nachricht);
    z.neuesLand = istNeuesLand(senderRuf);
    if (z.neuesLand) {
        const double mhz = (m_slice ? m_slice->frequency() : 0.0) / 1.0e6;
        const QString modus = protocol() == Ft8Decoder::Protocol::Ft4
                                  ? QStringLiteral("FT4")
                                  : QStringLiteral("FT8");
        z.landFarbe = m_model->dxccColorProvider()
                          ->colorForSpot(senderRuf, mhz, modus);
    }

    // ── Fehlt mir dieses Grossfeld? (2026-10-30) ─────────────
    //
    // Der Locator steht bei FT8 in der Nachricht selbst
    // („CQ G4MHJ IO83"); nachschlagen muss ihn niemand. Steht keiner
    // drin -- bei jedem Rapport und jedem RR73 --, ist die Zeile
    // nicht neu, sondern stumm: `GridsGearbeitet::istNeu` gibt dann
    // false, und das ist Absicht.
    //
    // `leer()` schuetzt vor der schlimmsten Form: ohne gelesenes
    // Logbuch waere JEDES Feld neu, und die Markierung sagte nichts
    // mehr. Dann lieber keine.
    const QString locator = locatorAusNachricht(nachricht);
    if (!m_grids.leer() && !locator.isEmpty()) {
        z.neuesGrid = m_grids.istNeu(locator);

        // ── Und die zweite Stufe: neu AUF DIESEM BAND (2026-11-12) ──
        //
        // Nur wenn das Feld nicht ohnehin ganz fehlt -- sonst stuenden
        // beide Fahnen an derselben Zeile und die schwaechere
        // entwertete die staerkere.
        //
        // `baender()` leer heisst: kein Logbuch gelesen. Dann gilt
        // dasselbe wie oben bei `leer()` -- lieber keine Markierung
        // als eine, die jede Zeile trifft.
        if (!z.neuesGrid && !m_mosaik.baender().isEmpty()) {
            const QString feld = Longpath::LocatorMosaik::feld(locator);
            // `bandFuerLog`, nicht selbst gerechnet: es ist DIE Stelle,
            // die auch `Ft8Gehoert::band` und den Logbucheintrag fuellt,
            // und sie beachtet den Transverter. Mit einer eigenen
            // Rechnung stuende am 2-m-Konverter das ZF-Band da, und das
            // Feld waere gegen die falsche Spalte geprueft.
            const QString band = bandFuerLog(m_slice ? m_slice->frequency()
                                                     : 0.0);
            z.neuesGridBand = !feld.isEmpty() && !band.isEmpty()
                && m_mosaik.stand(feld, band) == Longpath::FeldStand::Nie;
        }
    }

    // Gemerkt wird JEDE Zeile, gezeigt nur die, die in die Sicht
    // gehoert. Bis zum 2026-10-30 wurde hier gefiltert und verworfen
    // -- wer „CQ only" einschaltete, sah das Band unveraendert, bis
    // die naechste Periode durch war, und beim Ausschalten kam das
    // Verworfene nie zurueck.
    merkeZeile(z);
}

} // namespace Longpath
