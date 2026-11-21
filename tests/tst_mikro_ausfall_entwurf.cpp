// no-port-check: Longpath-eigenes Entwurfswerkzeug, keine portierte Logik.
// =================================================================
// tests/tst_mikro_ausfall_entwurf.cpp  (Longpath)
// =================================================================
//
// Entwurfsblätter: wie zeigt Longpath, dass das PC-Mikrofon weg ist?
// Kein Prüfstand — ein Zeichenwerkzeug. Anlass: am 2026-11-20 um
// 13:00:24 verschwand das USB-Mikrofon vom Mac, Longpath hing am
// weg gewesenen Gerät, und das stand nur im Protokoll.
//
//   Entwurf A  Fußzeile im TX-Feld: „PC mic" wird zur Warnung
//   Entwurf B  ein eigener Platz „MIC" in der Sicherheitsgruppe der
//              unteren Leiste (neben PA / OVL / ON AIR)
//   Entwurf C  Hinweis erst beim Senden: Streifen über der Mic-Zeile
//
// Je Entwurf drei Betriebsfälle — Normalbetrieb, Mikrofon weg beim
// Empfangen, Mikrofon weg mit gedrücktem MOX. Ein Entwurf, der nur im
// Gutfall überzeugt, taugt nichts.
//
// A und C zeigen das ECHTE TxApplet, B die ECHTEN StatusBadge in der
// Breite der echten Plätze. Nur die Änderung selbst ist darübergelegt.
//
//   LONGPATH_GRAB_DIR=/pfad LONGPATH_ENTWURF=1 \
//       ./build/tests/tst_mikro_ausfall_entwurf
//
// 144 dpi, damit die Vorschau wirkliche Größe zeigt.
// =================================================================
// Modification history (Longpath):
//   2026-11-20 — Angelegt. Longpath, KI-gestützt via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QApplication>
#include <QDir>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QStyleFactory>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "gui/StyleConstants.h"
#include "gui/applets/TxApplet.h"
#include "gui/chrome/ChromeBarItems.h"
#include "gui/styles/AppTheme.h"
#include "gui/styles/Theme.h"
#include "gui/widgets/StatusBadge.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <atomic>
#include <memory>

using namespace Longpath;

namespace {

constexpr int kPanelW = 520;

const QColor kBlatt(18, 19, 21);

enum class Fall { Normal, Weg, WegMox };

const char* fallName(Fall f)
{
    switch (f) {
        case Fall::Normal: return "Normalbetrieb — Mikrofon liefert";
        case Fall::Weg:    return "Mikrofon ausgesteckt, Empfang";
        case Fall::WegMox: return "Mikrofon ausgesteckt, MOX gedrückt";
    }
    return "";
}

void zeige(QWidget* w)
{
    w->setAttribute(Qt::WA_DontShowOnScreen);
    w->show();
    QCoreApplication::processEvents();
}

QImage greife(QWidget* w)
{
    QImage img(w->size() * 2, QImage::Format_ARGB32);
    img.setDevicePixelRatio(2.0);
    img.fill(QColor(Style::kPanelBg));
    w->render(&img);
    return img;
}

/// Das echte TX-Feld in einem bestimmten Zustand.
TxApplet* feld(RadioModel* modell, bool mox)
{
    auto* a = new TxApplet(modell);
    a->setFixedWidth(kPanelW);
    for (QPushButton* b : a->findChildren<QPushButton*>()) {
        if (b->text() == QStringLiteral("MOX")) { b->setChecked(mox); }
        if (b->text() == QStringLiteral("LEV")) { b->setChecked(true); }
    }
    for (QLabel* l : a->findChildren<QLabel*>()) {
        if (l->accessibleName() == QStringLiteral("TX filter status")) {
            l->setText(QStringLiteral("100–2900 Hz · 2.8k BW"));
        }
    }
    a->adjustSize();
    return a;
}

QLabel* mikroAbzeichen(TxApplet* a)
{
    for (QLabel* l : a->findChildren<QLabel*>()) {
        if (l->accessibleName() == QStringLiteral("Mic source indicator")) {
            return l;
        }
    }
    return nullptr;
}

/// Entwurf A: die Fußzeile nennt den Ausfall dort, wo die Quelle steht.
void alsWarnung(QLabel* l)
{
    if (l == nullptr) { return; }
    l->setText(QStringLiteral("● PC mic — no signal"));
    l->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: %2px; font-weight: bold; }")
        .arg(QLatin1String(Style::kAmberText)).arg(Style::kFontCaption));
}

/// Ein Blatt aus drei Zeilen: Fallname, darunter das Bild.
void blattSpeichern(const QString& dir, const QString& datei,
                    const QString& titel, const QList<QImage>& bilder,
                    const QString& fussnote)
{
    const int rand = 28;
    const int kopf = 54;
    const int fuss = 150;
    const int zeilenKopf = 22;
    int breite = 0;
    int hoehe = 0;
    for (const QImage& b : bilder) {
        const QSize s = b.deviceIndependentSize().toSize();
        breite = std::max(breite, s.width());
        hoehe += zeilenKopf + s.height() + 16;
    }
    breite = std::max(breite, 560);
    QImage blatt(QSize(breite + 2 * rand, kopf + hoehe + fuss) * 2,
                 QImage::Format_RGB32);
    blatt.setDevicePixelRatio(2.0);
    blatt.fill(kBlatt);
    QPainter p(&blatt);
    p.setRenderHint(QPainter::Antialiasing);
    QFont f = p.font();
    f.setPixelSize(15);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(196, 196, 201));
    p.drawText(QRect(rand, 14, breite, 26), Qt::AlignLeft | Qt::AlignVCenter, titel);

    int y = kopf;
    const Fall faelle[3] = {Fall::Normal, Fall::Weg, Fall::WegMox};
    for (int i = 0; i < bilder.size(); ++i) {
        f.setPixelSize(11);
        f.setBold(false);
        p.setFont(f);
        p.setPen(QColor(Style::kTextScale));
        p.drawText(QRect(rand, y, breite, zeilenKopf),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QString::fromUtf8(fallName(faelle[i])));
        y += zeilenKopf;
        p.drawImage(QPoint(rand, y), bilder.at(i));
        y += bilder.at(i).deviceIndependentSize().toSize().height() + 16;
    }
    f.setPixelSize(12);
    p.setFont(f);
    p.setPen(QColor(142, 142, 147));
    p.drawText(QRect(rand, y + 6, breite, fuss - 12),
               Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, fussnote);
    p.end();

    // 144 dpi: das Bild ist doppelt so gross wie die Punkte, die Vorschau
    // zeigt es damit in wirklicher Groesse.
    blatt.setDotsPerMeterX(static_cast<int>(144.0 / 0.0254));
    blatt.setDotsPerMeterY(static_cast<int>(144.0 / 0.0254));
    const QString pfad = QDir(dir).filePath(datei);
    QVERIFY2(blatt.save(pfad), qPrintable(pfad));
    qInfo().noquote() << "Blatt:" << pfad;
}

// ── Entwurf B: die Sicherheitsgruppe der unteren Leiste ─────────────
QImage sicherheitsgruppe(Fall fall)
{
    auto* leiste = new QWidget;
    leiste->setObjectName(QStringLiteral("leiste"));
    leiste->setFixedSize(kPanelW, 46);
    leiste->setStyleSheet(QStringLiteral(
        "QWidget#leiste { background: %1; border-top: 1px solid %2; }")
        .arg(QLatin1String(Style::kStatusBarBg), QLatin1String(Style::kBorder)));
    auto* row = new QHBoxLayout(leiste);
    row->setContentsMargins(8, 0, 8, 0);
    row->setSpacing(6);
    row->addStretch(1);

    // Wie MainWindow::dimSafetyBadge; ein aktives Abzeichen bekommt hier
    // gar keinen Effekt (offscreen zeichnet ein Effekt mit 1.0 nichts).
    auto dimmen = [](QWidget* w, bool aktiv) {
        if (aktiv) { return; }
        auto* fx = new QGraphicsOpacityEffect(w);
        fx->setOpacity(0.14);
        w->setGraphicsEffect(fx);
    };
    auto platz = [&](QWidget* badge, int breite) {
        auto* slot = new QWidget(leiste);
        slot->setFixedWidth(breite);
        auto* sl = new QHBoxLayout(slot);
        sl->setContentsMargins(0, 0, 0, 0);
        sl->addWidget(badge);
        row->addWidget(slot);
    };

    auto* pa = new StatusBadge;
    pa->setSvgIcon(QStringLiteral(":/icons/badge-check.svg"));
    pa->setLabel(QStringLiteral("PA"));
    pa->setVariant(StatusBadge::Variant::On);
    platz(pa, kSafetySlotWidthPx);

    auto* ovl = new StatusBadge;
    ovl->setLabel(QStringLiteral("ADC OVL"));
    ovl->setVariant(StatusBadge::Variant::Warn);
    dimmen(ovl, false);
    platz(ovl, kOverloadSlotWidthPx);

    const bool weg = fall != Fall::Normal;
    auto* mic = new StatusBadge;
    mic->setSvgIcon(QStringLiteral(":/icons/badge-dot.svg"));
    mic->setLabel(weg ? QStringLiteral("NO MIC") : QStringLiteral("MIC"));
    mic->setVariant(weg ? StatusBadge::Variant::Warn : StatusBadge::Variant::Off);
    dimmen(mic, weg);
    platz(mic, kMicSlotWidthPx);

    auto* tx = new StatusBadge;
    tx->setSvgIcon(QStringLiteral(":/icons/badge-dot.svg"));
    tx->setLabel(QStringLiteral("ON AIR"));
    tx->setVariant(fall == Fall::WegMox ? StatusBadge::Variant::Tx
                                        : StatusBadge::Variant::Off);
    platz(tx, kTxSlotWidthPx);

    zeige(leiste);
    const QImage img = greife(leiste);
    delete leiste;
    return img;
}

// ── Entwurf C: Streifen über der Mic-Zeile, nur beim Senden ─────────
QImage mitStreifen(TxApplet* a)
{
    QImage img = greife(a);
    // Auf die Zeile "Mic" ueber der Knopfreihe: dort steht ohnehin das
    // Mikrofon, und die Sendeanzeigen bleiben frei.
    QPushButton* vox = nullptr;
    QPushButton* tune = nullptr;
    for (QPushButton* b : a->findChildren<QPushButton*>()) {
        if (b->text() == QStringLiteral("VOX")) { vox = b; }
        if (b->text() == QStringLiteral("TUNE")) { tune = b; }
    }
    if (vox == nullptr || tune == nullptr) { return img; }
    const QPoint reihe = vox->mapTo(a, QPoint(0, 0));
    const int rechts = tune->mapTo(a, QPoint(0, 0)).x() - 8;
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF streifen(6, reihe.y() - 25, rechts - 6, 21);
    p.setPen(QPen(QColor(Style::kAmberBorder), 1.0));
    p.setBrush(QColor(Style::kBadgeWarnBg));
    p.drawRoundedRect(streifen, 6, 6);
    QFont f = p.font();
    f.setPixelSize(11);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(Style::kAmberText));
    p.drawText(streifen.adjusted(10, 0, -10, 0), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("No microphone signal — transmitting silence"));
    f.setBold(false);
    p.setFont(f);
    p.setPen(QColor(Style::kTextScale));
    p.drawText(streifen.adjusted(10, 0, -10, 0), Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("PC mic"));
    return img;
}

/// Aufnahmebus mit gestelltem "letzter Rueckruf" fuer das gebaute Blatt.
class GestellterBus : public IAudioBus {
public:
    explicit GestellterBus(std::shared_ptr<std::atomic<qint64>> ns) : m_ns(std::move(ns)) {}
    bool open(const AudioFormat&) override { return true; }
    void close() override {}
    bool isOpen() const override { return true; }
    qint64 push(const char*, qint64 b) override { return b; }
    qint64 pull(char*, qint64) override { return 0; }
    float rxLevel() const override { return 0.0F; }
    float txLevel() const override { return 0.0F; }
    QString backendName() const override { return QStringLiteral("GestellterBus"); }
    AudioFormat negotiatedFormat() const override { return {}; }
    qint64 nsSinceLastCallback() const override { return m_ns->load(); }
private:
    std::shared_ptr<std::atomic<qint64>> m_ns;
};

} // namespace

class TstMikroAusfallEntwurf : public QObject
{
    Q_OBJECT
private slots:
    void blaetter()
    {
        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (dir.isEmpty() || qEnvironmentVariable("LONGPATH_ENTWURF") != QStringLiteral("1")) {
            QSKIP("Zeichenwerkzeug: LONGPATH_GRAB_DIR und LONGPATH_ENTWURF=1 setzen.");
        }
        QDir().mkpath(dir);

        // Dieselben Zeilen wie main.cpp: ohne sie rendert Qt im hellen
        // Systemstil, und das Blatt zeigt nicht, was der Betreiber hat.
        qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        Longpath::applyDarkPalette(*qApp);
        Longpath::applyAppBaselineQss(*qApp);
        qApp->installEventFilter(new Longpath::Style::ThemeFilter(qApp));
        Longpath::Style::Theme::instance().applyStoredChoice();

        RadioModel modell;

        // A — Fußzeile
        {
            QList<QImage> b;
            for (Fall fall : {Fall::Normal, Fall::Weg, Fall::WegMox}) {
                TxApplet* a = feld(&modell, fall == Fall::WegMox);
                if (fall != Fall::Normal) { alsWarnung(mikroAbzeichen(a)); }
                zeige(a);
                b << greife(a);
                delete a;
            }
            blattSpeichern(dir, QStringLiteral("mikro-ausfall-A-fusszeile.png"),
                QStringLiteral("Entwurf A — Fußzeile im TX-Feld"), b,
                QStringLiteral(
                    "„PC mic“ unten links wird zur Warnung, in Bernstein (die Warnfarbe des Hauses, kein Rot). "
                    "Steht dort, wo die Mikrofonquelle ohnehin genannt wird; der Tooltip nennt das Gerät "
                    "(„Externes Mikrofon stopped delivering audio at 13:00:24“). Erscheint binnen etwa "
                    "1,5 s, unabhängig davon, ob gesendet wird. Nachteil: klein — und nur sichtbar, wenn das "
                    "TX-Feld im Blick ist."));
        }

        // Gebaut — das TX-Feld durch den ECHTEN Code in den Ausfall
        // gebracht: gestellter Bus, Waechter getaktet, kein Uebermalen.
        {
            RadioModel echt;
            AudioEngine* ae = echt.audioEngine();
            auto ns = std::make_shared<std::atomic<qint64>>(qint64(2) * 1000000);
            ae->setTxInputBusForTest(std::make_unique<GestellterBus>(ns));
            ae->setTonLaeuftFuerTest(true);
            echt.transmitModel().setMicSource(MicSource::Pc);
            ae->onMicSourceChanged(true);
            QList<QImage> b;
            qint64 t = 0;
            for (Fall fall : {Fall::Normal, Fall::Weg, Fall::WegMox}) {
                ns->store((fall == Fall::Normal ? 2 : 6000) * qint64(1000000));
                for (int i = 0; i < 4; ++i, t += 250) { ae->pruefePcMikrofon(t); }
                TxApplet* a = feld(&echt, fall == Fall::WegMox);
                a->syncFromModel();
                if (QPushButton* m = [a]() -> QPushButton* {
                        for (QPushButton* x : a->findChildren<QPushButton*>()) {
                            if (x->text() == QStringLiteral("MOX")) { return x; }
                        }
                        return nullptr; }()) {
                    m->setChecked(fall == Fall::WegMox);
                }
                zeige(a);
                b << greife(a);
                delete a;
            }
            blattSpeichern(dir, QStringLiteral("mikro-ausfall-A-gebaut.png"),
                QStringLiteral("Gebaut — TX-Feld, Ausfall über den echten Code"), b,
                QStringLiteral(
                    "Kein Übermalen: ein gestellter Aufnahmebus, dessen letzter Rückruf 6 s her ist, "
                    "MikrofonWaechter getaktet, TxApplet::aktualisiereMikroAbzeichen setzt den Text. "
                    "Der Tooltip lautet: „The system default input stopped delivering audio at …“ "
                    "(bzw. der Gerätename aus Setup → Audio → TX Input)."));
        }

        // B — Platz in der unteren Leiste
        {
            QList<QImage> b;
            for (Fall fall : {Fall::Normal, Fall::Weg, Fall::WegMox}) {
                b << sicherheitsgruppe(fall);
            }
            blattSpeichern(dir, QStringLiteral("mikro-ausfall-B-fussleiste.png"),
                QStringLiteral("Entwurf B — eigener Platz „MIC“ in der unteren Leiste"), b,
                QStringLiteral(
                    "Ein vierter fester Platz in der Sicherheitsgruppe (PA · ADC OVL · MIC · ON AIR), echte "
                    "Abzeichen. Im Normalbetrieb matt wie OVL, beim Ausfall „NO MIC“ in Bernstein. Die Leiste "
                    "ist immer zu sehen, egal wo das TX-Feld liegt; nichts verschiebt sich, weil der Platz "
                    "reserviert ist. Klick öffnet Setup → Audio → TX Input. Nur bei Quelle „PC mic“ — bei "
                    "Mikrofon an der Maschine oder DAX bleibt der Platz matt."));
        }

        // C — Streifen beim Senden
        {
            QList<QImage> b;
            for (Fall fall : {Fall::Normal, Fall::Weg, Fall::WegMox}) {
                TxApplet* a = feld(&modell, fall == Fall::WegMox);
                zeige(a);
                b << (fall == Fall::WegMox ? mitStreifen(a) : greife(a));
                delete a;
            }
            blattSpeichern(dir, QStringLiteral("mikro-ausfall-C-beim-senden.png"),
                QStringLiteral("Entwurf C — Hinweis erst beim Drücken von MOX"), b,
                QStringLiteral(
                    "Beim Empfang sieht man nichts; erst wenn MOX in einer Sprachbetriebsart gedrückt wird, "
                    "legt sich ein Streifen über die Mic-Zeile. Gesendet wird trotzdem (keine Sperre — ein "
                    "Fußtaster, der plötzlich nicht mehr tastet, wäre die schlimmere Überraschung). TUNE "
                    "braucht kein Mikrofon und bleibt ohne Hinweis. Nachteil: man erfährt es erst, wenn "
                    "man schon auf Sendung ist."));
        }
    }
};

QTEST_MAIN(TstMikroAusfallEntwurf)
#include "tst_mikro_ausfall_entwurf.moc"
