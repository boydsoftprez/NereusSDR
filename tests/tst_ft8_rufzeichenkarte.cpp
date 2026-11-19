// =================================================================
// tests/tst_ft8_rufzeichenkarte.cpp  (Longpath)
// =================================================================
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Die Rotor/Log-Karte folgt dem laufenden FT8-Partner (2026-11-18),
// nach dem Vorbild der Rufzeichenkarte neben Zeus' FT8-Feld.
//
// Zwei Haelften, und beide werden gebraucht:
//
//   * Das FT8-Fenster meldet `partnerGewechselt` auf JEDEM Weg, auf dem
//     ein Partner dazukommt -- Doppelklick, Antwort auf mein CQ, der
//     naechste aus der Schlange --, und nur bei echtem Wechsel, nicht
//     bei jedem Zug. Vorher gab es nur `fundAngenommen`, und das kam
//     bei Doppelklick und Karte, nie bei einem Antworter.
//   * Die Karte uebernimmt ihn, OHNE dem FT8-Feld den Fokus zu nehmen,
//     ohne den Rotor zu drehen, und nie ueber ein Rufzeichen, das der
//     Betreiber getippt oder per Spot geholt hat. QRZ laeuft ueber den
//     vorhandenen Weg (Cache zuerst), und Foto/Locator des vorigen
//     Partners bleiben nicht neben dem neuen stehen.
//
// Gefahren mit echten Ereignissen (QApplication::sendEvent):
// QTest::mouseClick verpufft ohne Schirm.
//
// Mit LONGPATH_GRAB_DIR legt `bild` ein Bild des echten Felds ab (mit
// QT_SCALE_FACTOR=2 in wirklicher Groesse, 144 dpi). LONGPATH_GRAB_FOTO
// nimmt ein Foto von der Platte statt des gemalten.
// =================================================================
// Modification history (Longpath):
//   2026-11-18 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest/QtTest>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStyleFactory>
#include <QTextBlock>
#include <QTimeZone>
#include <QTimer>

#include "FensterFrist.h"
#include "core/AppSettings.h"
#include "core/CallsignInfo.h"
#include "core/Ft8Ablauf.h"
#include "core/Ft8Decoder.h"
#include "core/QrzClient.h"
#include "gui/applets/Ft8Applet.h"
#include "gui/styles/AppTheme.h"
#include "gui/widgets/Ft8PartnerZeile.h"
#include "gui/widgets/RotorDialWidget.h"
#include "gui/widgets/RotorLogbookPanel.h"
#include "gui/widgets/StationPhoto.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

// Ein Fenster in der Vergangenheit, wie in tst_ft8_anruferliste.
constexpr qint64 kFenster = 999999990000LL;

Ft8Decode fund(const QString& text, float snr, float hz)
{
    Ft8Decode d;
    d.snrDb     = snr;
    d.dtSeconds = 0.2f;
    d.freqHz    = hz;
    d.score     = 40;
    d.text      = text;
    return d;
}

void periode(Ft8Applet& applet, const QVector<Ft8Decode>& funde, qint64 ms)
{
    QMetaObject::invokeMethod(
        &applet, "onDecoded", Qt::DirectConnection,
        Q_ARG(QVector<Longpath::Ft8Decode>, funde),
        Q_ARG(QDateTime, QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC)));
}

QPointF global(QWidget* w, const QPointF& p) { return w->mapToGlobal(p); }

void klick(QWidget* w)
{
    const QPointF p = QRectF(w->rect()).center();
    QMouseEvent druck(QEvent::MouseButtonPress, p, global(w, p),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(w, &druck);
    QMouseEvent los(QEvent::MouseButtonRelease, p, global(w, p),
                    Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(w, &los);
}

// Doppelklick auf die Fundzeile, die `teil` enthaelt -- an ihrer echten
// Stelle im Viewport, nicht blind auf die erste Zeile.
bool doppelklickAufZeile(Ft8Applet& applet, const QString& teil)
{
    QPlainTextEdit* l = applet.listForTest();
    for (QTextBlock b = l->document()->firstBlock(); b.isValid(); b = b.next()) {
        if (!b.text().contains(teil)) { continue; }
        const QRect r = l->cursorRect(QTextCursor(b));
        const QPointF p(60, r.center().y());
        QWidget* vp = l->viewport();
        QMouseEvent druck(QEvent::MouseButtonPress, p, global(vp, p),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &druck);
        QMouseEvent los(QEvent::MouseButtonRelease, p, global(vp, p),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &los);
        QMouseEvent doppel(QEvent::MouseButtonDblClick, p, global(vp, p),
                           Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(vp, &doppel);
        QApplication::sendEvent(vp, &los);
        return true;
    }
    return false;
}

// Tippen wie an der Tastatur: je Zeichen ein Druck und ein Loslassen.
void tippe(QWidget* w, const QString& text)
{
    for (const QChar ch : text) {
        const int taste = ch.toUpper().unicode();
        QKeyEvent druck(QEvent::KeyPress, taste, Qt::NoModifier, QString(ch));
        QApplication::sendEvent(w, &druck);
        QKeyEvent los(QEvent::KeyRelease, taste, Qt::NoModifier, QString(ch));
        QApplication::sendEvent(w, &los);
    }
}

// Alles im Feld markiert und mit der Ruecktaste weg. Markiert wird
// direkt: Cmd+A heiszt je System anders, die Ruecktaste nicht.
void leere(QLineEdit* w)
{
    w->selectAll();
    QKeyEvent weg(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
    QApplication::sendEvent(w, &weg);
}

QLineEdit* feld(QWidget& w, const QString& platzhalter)
{
    for (QLineEdit* e : w.findChildren<QLineEdit*>()) {
        if (e->placeholderText() == platzhalter) { return e; }
    }
    return nullptr;
}

bool zeigtText(QWidget& w, const QString& teil)
{
    for (QLabel* l : w.findChildren<QLabel*>()) {
        if (l->isVisibleTo(&w) && l->text().contains(teil)) { return true; }
    }
    return false;
}

StationPhoto* foto(QWidget& w) { return w.findChild<StationPhoto*>(); }

// Ein Portraet in den Fotocache legen, als waere es frueher geladen
// worden. Gemalt, kein echtes Gesicht -- ausser LONGPATH_GRAB_FOTO
// zeigt auf eine Datei.
void legeFotoAb(const QString& url)
{
    const QString ziel = StationPhoto::cachePath(url);
    QFile::remove(ziel);
    const QString vonPlatte = qEnvironmentVariable("LONGPATH_GRAB_FOTO");
    if (!vonPlatte.isEmpty() && QFile::copy(vonPlatte, ziel)) { return; }

    QImage bild(480, 320, QImage::Format_RGB32);
    QPainter p(&bild);
    QLinearGradient himmel(0, 0, 0, 320);
    himmel.setColorAt(0.0, QColor(0x6f, 0x8f, 0xb5));
    himmel.setColorAt(0.7, QColor(0xc9, 0xd6, 0xe0));
    himmel.setColorAt(1.0, QColor(0x5d, 0x6b, 0x4a));
    p.fillRect(bild.rect(), himmel);
    p.fillRect(QRect(0, 250, 480, 70), QColor(0x55, 0x63, 0x42));
    p.setPen(QPen(QColor(0x33, 0x33, 0x38), 3));
    p.drawLine(300, 250, 300, 70);                    // Mast
    p.drawLine(250, 80, 350, 80);                     // Beam
    p.drawLine(260, 95, 340, 95);
    p.fillRect(QRect(110, 190, 120, 60), QColor(0xb8, 0x9a, 0x6a));   // Haus
    p.end();
    QVERIFY(bild.save(ziel, "PNG"));
}

CallsignInfo antwort(const QString& ruf, const QString& vor, const QString& nach,
                     const QString& ort, const QString& land,
                     const QString& grid, const QString& bildUrl)
{
    CallsignInfo i;
    i.call         = ruf;
    i.firstName    = vor;
    i.lastName     = nach;
    i.city         = ort;
    i.country      = land;
    i.grid         = grid;
    i.licenseClass = QStringLiteral("A");
    i.imageUrl     = bildUrl;
    i.fetchedUtc   = QDateTime::currentSecsSinceEpoch();
    return i;
}

// Die beiden Wege wie in MainWindow: der Partner geht zur Karte, die
// Auskunft der Karte zurueck ins FT8-Feld.
void verbinde(Ft8Applet* applet, RotorLogbookPanel* p)
{
    QObject::connect(applet, &Ft8Applet::partnerGewechselt, p,
                     [p](const QString& wer) {
        if (!wer.isEmpty()) { p->folgeFt8Partner(wer); }
    });
    QObject::connect(p, &RotorLogbookPanel::auskunft, applet,
                     [applet](const RufzeichenAuskunft& a) {
        applet->setPartnerAuskunft(a);
    });
}

QString zeile(Ft8Applet& applet)
{
    return applet.partnerZeileForTest()->sichtbareStueckeForTest()
        .join(QStringLiteral(" | "));
}

}  // namespace

class TstFt8Rufzeichenkarte : public QObject
{
    Q_OBJECT

    struct Stand {
        RadioModel radio;
        Ft8Applet  applet{&radio};
    };

    static void richte(RadioModel& radio, Ft8Applet& applet)
    {
        const int id = radio.addSlice();
        SliceModel* slice = radio.sliceById(id);
        QVERIFY(slice);
        slice->setFrequency(14074000.0);
        applet.setSlice(slice);
        applet.resize(706, 700);
    }
    static void richte(Stand& s) { richte(s.radio, s.applet); }

private slots:
    void initTestCase()
    {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        applyDarkPalette(*qApp);
        applyAppBaselineQss(*qApp);
        AppSettings::instance().setValue(QStringLiteral("StationCallsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("StationGridSquare"),
                                         QStringLiteral("JN67"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67"));
        AppSettings::instance().setValue(QStringLiteral("Ft8/Plaetze"),
                                         QStringLiteral("1"));
    }

    // ── Das FT8-Fenster ──────────────────────────────────────────────

    // Doppelklick auf eine CQ-Zeile: gemeldet, einmal -- und nicht noch
    // einmal, wenn er mit dem Rapport antwortet.
    void doppelklickMeldetDenPartnerEinmal()
    {
        Stand s;
        richte(s);
        s.applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&s.applet, kFensterFrist));
        QSignalSpy partner(&s.applet, &Ft8Applet::partnerGewechselt);

        periode(s.applet, {fund(QStringLiteral("CQ DL1ABC JO31"), -8.0f, 1850.0f)},
                kFenster);
        QCOMPARE(partner.count(), 0);       // gehoert ist nicht gerufen

        QVERIFY(doppelklickAufZeile(s.applet, QStringLiteral("CQ DL1ABC")));
        QCOMPARE(s.applet.ablaufForTest().partner(), QStringLiteral("DL1ABC"));
        QCOMPARE(partner.count(), 1);
        QCOMPARE(partner.at(0).at(0).toString(), QStringLiteral("DL1ABC"));

        // Er antwortet mit Rapport: neuer Zug, derselbe Partner.
        periode(s.applet, {fund(QStringLiteral("OE5SOS DL1ABC -10"), -8.0f, 1850.0f)},
                kFenster + 15000);
        QVERIFY(s.applet.ablaufForTest().zustand()
                != Ft8Ablauf::Zustand::RufeAn);
        QCOMPARE(partner.count(), 1);
    }

    // Jemand antwortet auf mein CQ. Diesen Weg kannte `fundAngenommen`
    // nie -- und er ist im Lauf der haeufigste.
    void antwortAufMeinCqMeldetDenPartner()
    {
        Stand s;
        richte(s);
        s.radio.installFt8TxForTest();
        s.applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&s.applet, kFensterFrist));
        QSignalSpy partner(&s.applet, &Ft8Applet::partnerGewechselt);
        QSignalSpy angenommen(&s.applet, &Ft8Applet::fundAngenommen);

        QPushButton* cq = s.applet.cqRufenButtonForTest();
        QVERIFY(cq && cq->isEnabled());
        klick(cq);
        QCOMPARE(s.applet.ablaufForTest().zustand(), Ft8Ablauf::Zustand::RufeCq);
        QCOMPARE(partner.count(), 0);       // CQ hat keinen Partner
        s.applet.fensterZuEndeForTest();

        periode(s.applet, {fund(QStringLiteral("OE5SOS SP9XYZ KO00"), -7.0f, 1906.0f)},
                kFenster);
        QCOMPARE(s.applet.ablaufForTest().partner(), QStringLiteral("SP9XYZ"));
        QCOMPARE(partner.count(), 1);
        QCOMPARE(partner.at(0).at(0).toString(), QStringLiteral("SP9XYZ"));
        QCOMPARE(angenommen.count(), 0);    // genau die alte Luecke
    }

    // Der naechste aus der Schlange, wie nach einem fertigen QSO.
    void naechsterAusDerSchlangeMeldetDenPartner()
    {
        Stand s;
        richte(s);
        s.applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&s.applet, kFensterFrist));
        QSignalSpy partner(&s.applet, &Ft8Applet::partnerGewechselt);

        s.applet.ablaufForTestSchreibend().starteMitAnrufer(
            QStringLiteral("HB9ZHK"), QStringLiteral("OE5SOS"),
            QStringLiteral("JN67"), 18, QStringLiteral("JN47"));
        periode(s.applet, {
            fund(QStringLiteral("OE5SOS PD4RJ JO33"), 12.0f, 1338.0f),
            fund(QStringLiteral("OE5SOS 9A6T JN85"), 11.0f, 1428.0f),
        }, kFenster);
        QVERIFY(partner.count() >= 1);
        QCOMPARE(partner.last().at(0).toString(), QStringLiteral("HB9ZHK"));
        const int vorher = partner.count();

        QVERIFY(s.applet.rufeNaechstenWartendenForTest());
        QCOMPARE(s.applet.ablaufForTest().partner(), QStringLiteral("PD4RJ"));
        QCOMPARE(partner.count(), vorher + 1);
        QCOMPARE(partner.last().at(0).toString(), QStringLiteral("PD4RJ"));
    }

    // ── Die Karte ────────────────────────────────────────────────────

    // Der Fokus bleibt im FT8-Feld, und der Rotor dreht nicht.
    void folgtOhneFokusUndOhneDrehen()
    {
        QWidget fenster;
        auto* reihe = new QHBoxLayout(&fenster);
        auto* ft8Feld = new QLineEdit(&fenster);
        auto* p = new RotorLogbookPanel(nullptr, nullptr, nullptr, &fenster);
        reihe->addWidget(ft8Feld);
        reihe->addWidget(p);
        fenster.resize(700, 733);
        fenster.show();
        QVERIFY(QTest::qWaitForWindowExposed(&fenster, kFensterFrist));
        fenster.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&fenster, kFensterFrist));

        // Ein Ziel auf dem Kompass, von Hand: eigener und fremder Locator.
        QLineEdit* mein = feld(*p, QStringLiteral("your locator"));
        QLineEdit* sein = feld(*p, QStringLiteral("locator"));
        QVERIFY(mein && sein);
        mein->setText(QStringLiteral("JN67"));
        tippe(sein, QStringLiteral("JO31"));
        QVERIFY(p->dial()->hasTarget());

        ft8Feld->setFocus();
        QTRY_VERIFY(ft8Feld->hasFocus());

        p->folgeFt8Partner(QStringLiteral("dl1abc"));
        QLineEdit* ruf = feld(*p, QStringLiteral("callsign"));
        QVERIFY(ruf);
        QCOMPARE(ruf->text(), QStringLiteral("DL1ABC"));
        QVERIFY2(ft8Feld->hasFocus(), "die Karte hat dem FT8-Feld den Fokus genommen");
        QCOMPARE(QApplication::focusWidget(), ft8Feld);
        QVERIFY2(p->dial()->state() != RotorDialWidget::State::Turning,
                 "der Rotor dreht -- der Partner soll nur ZEIGEN");
        // Von Hand getippt: der Locator bleibt, und mit ihm das Ziel.
        QCOMPARE(sein->text(), QStringLiteral("JO31"));
        QVERIFY(p->dial()->hasTarget());
    }

    // Nur ein leeres Feld oder der VORIGE FT8-Partner wird ersetzt.
    void getipptesUndGespottetesBleiben()
    {
        RotorLogbookPanel p(nullptr, nullptr, nullptr);
        p.resize(317, 733);
        p.show();
        QVERIFY(QTest::qWaitForWindowExposed(&p, kFensterFrist));
        QLineEdit* ruf = feld(p, QStringLiteral("callsign"));
        QVERIFY(ruf);

        p.folgeFt8Partner(QStringLiteral("DL1ABC"));
        QCOMPARE(ruf->text(), QStringLiteral("DL1ABC"));
        p.folgeFt8Partner(QStringLiteral("PD4RJ"));          // der vorige: weiter
        QCOMPARE(ruf->text(), QStringLiteral("PD4RJ"));
        p.folgeFt8Partner(QString());                         // CQ: bleibt stehen
        QCOMPARE(ruf->text(), QStringLiteral("PD4RJ"));
        p.folgeFt8Partner(QStringLiteral("<HB9ZHK>"));        // Hash-Klammern weg
        QCOMPARE(ruf->text(), QStringLiteral("HB9ZHK"));

        // Der Betreiber tippt selbst.
        leere(ruf);
        QCOMPARE(ruf->text(), QString());
        tippe(ruf, QStringLiteral("oe1xyz"));
        QCOMPARE(ruf->text(), QStringLiteral("OE1XYZ"));
        p.folgeFt8Partner(QStringLiteral("SP9XYZ"));
        QVERIFY2(ruf->text() == QStringLiteral("OE1XYZ"),
                 "ein getipptes Rufzeichen wurde ueberschrieben");

        // Und auch dann nicht, wenn er zufaellig vorher FT8-Partner war.
        p.folgeFt8Partner(QStringLiteral("9A6T"));
        QCOMPARE(ruf->text(), QStringLiteral("OE1XYZ"));

        // Ein Spot aus dem Panadapter zaehlt wie getippt.
        leere(ruf);
        p.folgeFt8Partner(QStringLiteral("9A6T"));
        QCOMPARE(ruf->text(), QStringLiteral("9A6T"));
        p.takeSpot(QStringLiteral("K1ABC"));
        p.folgeFt8Partner(QStringLiteral("DL0HAM"));
        QCOMPARE(ruf->text(), QStringLiteral("K1ABC"));

        // Leer gemacht: der naechste Partner steht wieder da.
        leere(ruf);
        p.folgeFt8Partner(QStringLiteral("DL0HAM"));
        QCOMPARE(ruf->text(), QStringLiteral("DL0HAM"));
    }

    // QRZ ueber den vorhandenen Weg: Cache sofort, sonst die Entprellung.
    // Und was zum vorigen Partner gehoerte, geht mit ihm.
    void qrzUeberDenVorhandenenWeg()
    {
        QrzClient qrz;
        qrz.setCredentials(QStringLiteral("pruefstand"), QStringLiteral("pruefstand"));
        RotorLogbookPanel p(nullptr, &qrz, nullptr);
        p.resize(317, 733);
        p.show();
        QVERIFY(QTest::qWaitForWindowExposed(&p, kFensterFrist));
        QLineEdit* mein = feld(p, QStringLiteral("your locator"));
        QLineEdit* sein = feld(p, QStringLiteral("locator"));
        QVERIFY(mein && sein);
        mein->setText(QStringLiteral("JN67"));

        const QString bildUrl = QStringLiteral("https://example.invalid/pruefstand-dl1abc.png");
        legeFotoAb(bildUrl);
        p.merkeQrzAntwortForTest(antwort(QStringLiteral("DL1ABC"),
            QStringLiteral("Hans"), QStringLiteral("Muster"), QStringLiteral("Bonn"),
            QStringLiteral("Germany"), QStringLiteral("JO30RR"), bildUrl));

        p.folgeFt8Partner(QStringLiteral("DL1ABC"));
        QVERIFY2(zeigtText(p, QStringLiteral("Hans Muster")),
                 "die QRZ-Antwort aus dem Cache steht nicht in der Karte");
        QVERIFY(!p.lookupTimerForTest()->isActive());     // Cache: keine Anfrage
        QCOMPARE(sein->text(), QStringLiteral("JO30RR"));
        StationPhoto* f = foto(p);
        QVERIFY(f);
        QTRY_VERIFY(f->hasPhoto() && f->isVisibleTo(&p));

        // Der naechste ist nicht im Cache: die Abfrage wird eingeplant
        // (und hier angehalten -- kein Netz im Pruefstand).
        p.folgeFt8Partner(QStringLiteral("SP9XYZ"));
        QTimer* uhr = p.lookupTimerForTest();
        const bool eingeplant = uhr->isActive();
        uhr->stop();
        QVERIFY2(eingeplant, "fuer den neuen Partner wird QRZ nicht gefragt");
        QVERIFY2(!f->isVisibleTo(&p),
                 "das Foto des vorigen Partners steht neben dem neuen");
        QVERIFY2(!zeigtText(p, QStringLiteral("Hans Muster")),
                 "der Name des vorigen Partners steht neben dem neuen");
        QVERIFY2(sein->text().isEmpty(),
                 "der Locator des vorigen Partners steht neben dem neuen");

        // Zurueck zu einem bekannten: wieder alles da, wieder ohne Netz.
        leere(feld(p, QStringLiteral("callsign")));
        uhr->stop();
        p.folgeFt8Partner(QStringLiteral("DL1ABC"));
        QVERIFY(zeigtText(p, QStringLiteral("Hans Muster")));
        QVERIFY(!uhr->isActive());
    }

    // Die Kette wie in MainWindow: Antwort auf mein CQ -> Karte, und der
    // Fokus bleibt im Textfeld des FT8-Fensters.
    void ketteVomFt8FensterZurKarte()
    {
        // Das Radio VOR dem Rahmen: der Rahmen geht zuerst und nimmt das
        // FT8-Fenster mit, das am Radio haengt.
        RadioModel radio;
        radio.installFt8TxForTest();
        QWidget fenster;
        auto* applet = new Ft8Applet(&radio, &fenster);
        richte(radio, *applet);
        auto* reihe = new QHBoxLayout(&fenster);
        auto* p = new RotorLogbookPanel(nullptr, nullptr, nullptr, &fenster);
        reihe->addWidget(applet);
        reihe->addWidget(p);
        QObject::connect(applet, &Ft8Applet::partnerGewechselt, p,
                         [p](const QString& wer) {
            if (!wer.isEmpty()) { p->folgeFt8Partner(wer); }
        });
        fenster.resize(1030, 733);
        fenster.show();
        QVERIFY(QTest::qWaitForWindowExposed(&fenster, kFensterFrist));
        fenster.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&fenster, kFensterFrist));

        klick(applet->cqRufenButtonForTest());
        applet->fensterZuEndeForTest();
        QLineEdit* text = applet->textFeldForTest();
        QVERIFY(text);
        text->setFocus();
        QTRY_VERIFY(text->hasFocus());

        periode(*applet, {fund(QStringLiteral("OE5SOS SP9XYZ KO00"), -7.0f, 1906.0f)},
                kFenster);
        QCOMPARE(feld(*p, QStringLiteral("callsign"))->text(), QStringLiteral("SP9XYZ"));
        QVERIFY2(text->hasFocus(), "das FT8-Textfeld hat den Fokus verloren");
    }

    // ── Die Zeile unter „QSO with" (Entwurf A) ────────────────────────

    // Ohne Netz: Locator aus seiner Nachricht, Weg vom eigenen Feld,
    // Licht bei ihm. Ohne Partner bleibt die Zeile leer, aber stehen.
    void zeileOhneNetz()
    {
        Stand s;
        richte(s);
        s.applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&s.applet, kFensterFrist));
        Ft8PartnerZeile* z = s.applet.partnerZeileForTest();
        QVERIFY(z);
        QVERIFY2(z->isVisibleTo(&s.applet), "die Zeile steht nicht in der QSO-Karte");
        QVERIFY(z->height() > 0);
        QVERIFY(z->sichtbareStueckeForTest().isEmpty());
        const int hoeheLeer = z->height();

        periode(s.applet, {fund(QStringLiteral("CQ DL1ABC JO31"), -8.0f, 1850.0f)},
                kFenster);
        QVERIFY(doppelklickAufZeile(s.applet, QStringLiteral("CQ DL1ABC")));
        const QStringList st = z->sichtbareStueckeForTest();
        QVERIFY2(st.size() == 2, qPrintable(st.join(QStringLiteral(" | "))));
        QVERIFY2(st.at(0).startsWith(QStringLiteral("JO31 · ")), qPrintable(st.at(0)));
        QVERIFY(st.at(0).contains(QStringLiteral(" km · ")));
        QVERIFY(st.at(0).endsWith(QStringLiteral("°")));
        QVERIFY2(st.at(1).endsWith(QStringLiteral(" there")), qPrintable(st.at(1)));
        QCOMPARE(z->height(), hoeheLeer);     // die Liste springt nicht
    }

    // Die Kette wie in MainWindow: Cache, Abfrage laeuft, Antwort aus
    // dem Netz, nicht gefunden -- alles ueber die Karte, nie selbst.
    void zeileMitQrzAusDerKarte()
    {
        RadioModel radio;
        QrzClient qrz;
        qrz.setCredentials(QStringLiteral("pruefstand"), QStringLiteral("pruefstand"));
        QWidget fenster;
        auto* applet = new Ft8Applet(&radio, &fenster);
        richte(radio, *applet);
        auto* reihe = new QHBoxLayout(&fenster);
        auto* p = new RotorLogbookPanel(nullptr, &qrz, nullptr, &fenster);
        reihe->addWidget(applet);
        reihe->addWidget(p);
        verbinde(applet, p);
        fenster.resize(1030, 733);
        fenster.show();
        QVERIFY(QTest::qWaitForWindowExposed(&fenster, kFensterFrist));

        p->merkeQrzAntwortForTest(antwort(QStringLiteral("DL1ABC"),
            QStringLiteral("Hans"), QStringLiteral("Muster"), QStringLiteral("Bonn"),
            QStringLiteral("Germany"), QStringLiteral("JO30RR"), QString()));

        // Aus dem Cache: sofort, ohne Anfrage.
        periode(*applet, {
            fund(QStringLiteral("CQ DL1ABC JO31"), -8.0f, 1850.0f),
            fund(QStringLiteral("CQ SP9XYZ KO00"), -7.0f, 1906.0f),
        }, kFenster);
        QVERIFY(doppelklickAufZeile(*applet, QStringLiteral("CQ DL1ABC")));
        // QRZ sagt JO30RR, gesendet hat er JO31: er ist woanders, das
        // Gesendete gilt.
        QVERIFY2(zeile(*applet).startsWith(QStringLiteral("Hans Muster | Bonn | class A | JO31 · ")),
                 qPrintable(zeile(*applet)));
        QVERIFY(!p->lookupTimerForTest()->isActive());

        // Der naechste ist nicht im Cache: „looking up", und die Karte
        // hat die Abfrage eingeplant (hier angehalten, kein Netz).
        QVERIFY(doppelklickAufZeile(*applet, QStringLiteral("CQ SP9XYZ")));
        QCOMPARE(applet->ablaufForTest().partner(), QStringLiteral("SP9XYZ"));
        QTimer* uhr = p->lookupTimerForTest();
        QVERIFY(uhr->isActive());
        uhr->stop();
        QVERIFY2(zeile(*applet).startsWith(QStringLiteral("looking up SP9XYZ… | KO00 · ")),
                 qPrintable(zeile(*applet)));
        QVERIFY2(!zeile(*applet).contains(QStringLiteral("Hans Muster")),
                 "der Name des vorigen Partners steht neben dem neuen");

        // Die Antwort kommt -- ueber den QRZ-Weg der Karte.
        emit qrz.lookupSucceeded(QStringLiteral("SP9XYZ"), antwort(QStringLiteral("SP9XYZ"),
            QStringLiteral("Jan"), QStringLiteral("Kowalski"), QStringLiteral("Tarnów"),
            QStringLiteral("Poland"), QStringLiteral("KO00LA"), QString()));
        QVERIFY2(zeile(*applet).startsWith(QStringLiteral("Jan Kowalski | Tarnów | class A | KO00LA · ")),
                 qPrintable(zeile(*applet)));

        // Ein dritter, den QRZ nicht kennt.
        periode(*applet, {fund(QStringLiteral("CQ PD4RJ JO33"), 12.0f, 1338.0f)},
                kFenster + 15000);
        QVERIFY(doppelklickAufZeile(*applet, QStringLiteral("CQ PD4RJ")));
        p->lookupTimerForTest()->stop();
        emit qrz.lookupFailed(QStringLiteral("PD4RJ"), QrzClient::Error::NotFound, QString());
        QVERIFY2(zeile(*applet).endsWith(QStringLiteral(" | not in QRZ")),
                 qPrintable(zeile(*applet)));
    }

    // Was der Betreiber in die Karte tippt, gehoert nicht in die Zeile.
    void fremdeAuskunftZaehltNicht()
    {
        RadioModel radio;
        QrzClient qrz;
        qrz.setCredentials(QStringLiteral("pruefstand"), QStringLiteral("pruefstand"));
        QWidget fenster;
        auto* applet = new Ft8Applet(&radio, &fenster);
        richte(radio, *applet);
        auto* reihe = new QHBoxLayout(&fenster);
        auto* p = new RotorLogbookPanel(nullptr, &qrz, nullptr, &fenster);
        reihe->addWidget(applet);
        reihe->addWidget(p);
        verbinde(applet, p);
        fenster.resize(1030, 733);
        fenster.show();
        QVERIFY(QTest::qWaitForWindowExposed(&fenster, kFensterFrist));
        p->merkeQrzAntwortForTest(antwort(QStringLiteral("OE1XYZ"),
            QStringLiteral("Franz"), QStringLiteral("Fremd"), QStringLiteral("Wien"),
            QStringLiteral("Austria"), QStringLiteral("JN88EF"), QString()));

        periode(*applet, {fund(QStringLiteral("CQ DL1ABC JO31"), -8.0f, 1850.0f)},
                kFenster);
        QVERIFY(doppelklickAufZeile(*applet, QStringLiteral("CQ DL1ABC")));
        p->lookupTimerForTest()->stop();

        // Er tippt selbst ein anderes Rufzeichen: die Karte zeigt OE1XYZ.
        QLineEdit* ruf = feld(*p, QStringLiteral("callsign"));
        leere(ruf);
        tippe(ruf, QStringLiteral("OE1XYZ"));
        QVERIFY(zeigtText(*p, QStringLiteral("Franz Fremd")));
        QVERIFY2(!zeile(*applet).contains(QStringLiteral("Franz Fremd")),
                 "die Zeile zeigt ein getipptes Rufzeichen statt des Partners");
        QVERIFY(zeile(*applet).startsWith(QStringLiteral("looking up DL1ABC…")));
    }

    // Ohne QRZ-Konto: der Hinweis, wo man es eintraegt.
    void ohneKontoDerHinweis()
    {
        RadioModel radio;
        QWidget fenster;
        auto* applet = new Ft8Applet(&radio, &fenster);
        richte(radio, *applet);
        auto* reihe = new QHBoxLayout(&fenster);
        auto* p = new RotorLogbookPanel(nullptr, nullptr, nullptr, &fenster);
        reihe->addWidget(applet);
        reihe->addWidget(p);
        verbinde(applet, p);
        fenster.resize(1030, 733);
        fenster.show();
        QVERIFY(QTest::qWaitForWindowExposed(&fenster, kFensterFrist));

        periode(*applet, {fund(QStringLiteral("CQ DL1ABC JO31"), -8.0f, 1850.0f)},
                kFenster);
        QVERIFY(doppelklickAufZeile(*applet, QStringLiteral("CQ DL1ABC")));
        QVERIFY2(zeile(*applet).endsWith(QStringLiteral(" | name and photo: QRZ account in Tools")),
                 qPrintable(zeile(*applet)));
    }

    // Wenn der Platz nicht reicht: erst Hinweis, Ort, Klasse, Weg --
    // Name und Licht bleiben am laengsten.
    void schmalBleibenNameUndLicht()
    {
        Ft8PartnerZeile z;
        Ft8PartnerZeile::Inhalt in;
        in.rufzeichen = QStringLiteral("SP9XYZ");
        in.name    = QStringLiteral("Jan Kowalski");
        in.ort     = QStringLiteral("Tarnów");
        in.klasse  = QStringLiteral("A");
        in.locator = QStringLiteral("KO00");
        in.km      = 647;
        in.grad    = 61;
        in.licht   = Grauzone::Licht::Nacht;
        in.qrz     = RufzeichenAuskunft::Stand::Da;
        z.setzeInhalt(in);
        z.resize(2000, 20);
        QCOMPARE(z.sichtbareStueckeForTest(), (QStringList{
            QStringLiteral("Jan Kowalski"), QStringLiteral("Tarnów"),
            QStringLiteral("class A"), QStringLiteral("KO00 · 647 km · 61°"),
            QStringLiteral("night there")}));
        const int voll = z.sizeHint().width();

        z.resize(voll - 10, 20);
        QVERIFY2(!z.sichtbareStueckeForTest().contains(QStringLiteral("Tarnów")),
                 "zuerst geht der Ort");
        QVERIFY(z.sichtbareStueckeForTest().contains(QStringLiteral("class A")));

        z.resize(140, 20);
        const QStringList knapp = z.sichtbareStueckeForTest();
        QVERIFY2(knapp.contains(QStringLiteral("Jan Kowalski")), qPrintable(knapp.join(QLatin1Char('|'))));
        QVERIFY2(knapp.contains(QStringLiteral("night there"))
                     || knapp.size() == 1,
                 qPrintable(knapp.join(QLatin1Char('|'))));
        QVERIFY(!knapp.contains(QStringLiteral("KO00 · 647 km · 61°")));
    }

    // Ein Bild des echten Felds -- nur mit LONGPATH_GRAB_DIR.
    void bild()
    {
        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (dir.isEmpty()) { QSKIP("ohne LONGPATH_GRAB_DIR kein Bild"); }
        QDir().mkpath(dir);

        RadioModel radio;
        radio.installFt8TxForTest();
        QrzClient qrz;
        qrz.setCredentials(QStringLiteral("pruefstand"), QStringLiteral("pruefstand"));

        QWidget fenster;
        fenster.setAutoFillBackground(true);
        auto* applet = new Ft8Applet(&radio, &fenster);
        richte(radio, *applet);
        auto* reihe = new QHBoxLayout(&fenster);
        reihe->setContentsMargins(8, 8, 8, 8);
        reihe->setSpacing(8);
        auto* p = new RotorLogbookPanel(nullptr, &qrz, nullptr, &fenster);
        p->setFixedSize(317, 733);
        applet->setFixedSize(706, 733);
        reihe->addWidget(applet);
        reihe->addWidget(p);
        verbinde(applet, p);
        fenster.show();
        QVERIFY(QTest::qWaitForWindowExposed(&fenster, kFensterFrist));
        if (QLineEdit* mein = feld(*p, QStringLiteral("your locator"))) {
            mein->setText(QStringLiteral("JN67"));
        }

        const QString bildUrl = QStringLiteral("https://example.invalid/pruefstand-sp9xyz.jpg");
        legeFotoAb(bildUrl);
        p->merkeQrzAntwortForTest(antwort(QStringLiteral("SP9XYZ"),
            QStringLiteral("Jan"), QStringLiteral("Kowalski"), QStringLiteral("Tarnów"),
            QStringLiteral("Poland"), QStringLiteral("KO00LA"), bildUrl));

        klick(applet->cqRufenButtonForTest());
        applet->fensterZuEndeForTest();
        periode(*applet, {
            fund(QStringLiteral("CQ DL1ABC JO31"), -8.0f, 1850.0f),
            fund(QStringLiteral("OE5SOS SP9XYZ KO00"), -7.0f, 1906.0f),
            fund(QStringLiteral("CQ PD4RJ JO33"), 12.0f, 1338.0f),
        }, kFenster);
        QCOMPARE(feld(*p, QStringLiteral("callsign"))->text(), QStringLiteral("SP9XYZ"));
        QTRY_VERIFY(foto(*p)->hasPhoto());
        QTest::qWait(300);

        const auto sichere = [&dir](QWidget* w, const QString& name) {
            QImage img = w->grab().toImage();
            img.setDotsPerMeterX(5669);     // 144 dpi
            img.setDotsPerMeterY(5669);
            QVERIFY(img.save(dir + QLatin1Char('/') + name));
        };
        QVERIFY(zeile(*applet).startsWith(QStringLiteral("Jan Kowalski")));
        sichere(&fenster, QStringLiteral("rufzeichenkarte-heute.png"));

        // Die QSO-Karte allein, in den drei Faellen der Entwurfsblaetter.
        QWidget* karte = applet->partnerZeileForTest()->parentWidget();
        sichere(karte, QStringLiteral("zeile-volle-daten.png"));
        RufzeichenAuskunft a;
        a.rufzeichen = QStringLiteral("SP9XYZ");
        a.stand = RufzeichenAuskunft::Stand::Laeuft;
        applet->setPartnerAuskunft(a);
        sichere(karte, QStringLiteral("zeile-abfrage-laeuft.png"));
        a.stand = RufzeichenAuskunft::Stand::KeinKonto;
        applet->setPartnerAuskunft(a);
        sichere(karte, QStringLiteral("zeile-kein-qrz-konto.png"));
    }
};

QTEST_MAIN(TstFt8Rufzeichenkarte)
#include "tst_ft8_rufzeichenkarte.moc"
