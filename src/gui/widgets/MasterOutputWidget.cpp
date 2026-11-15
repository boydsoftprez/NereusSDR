// =================================================================
// src/gui/widgets/MasterOutputWidget.cpp  (Longpath)
// =================================================================
//
// Longpath-original widget. See MasterOutputWidget.h for the full
// attribution block. Styling references AetherSDR
// `src/gui/TitleBar.cpp:172-215`; the structure here is original to
// Longpath because this widget isolates JUST the master-output
// triad from AetherSDR's monolithic TitleBar.
//
// =================================================================
// Modification history (Longpath):
//   2026-04-20 — Written by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Anthropic Claude Code.
//                Phase 3O Sub-Phase 10 Task 10b.
// =================================================================

#include "MasterOutputWidget.h"
#include "core/LogCategories.h"
#include "gui/ScopedChildWidget.h"

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/audio/PortAudioBus.h"
#include "core/audio/SystemOutputMute.h"

#include <QAction>
#include <QActionGroup>
#include <QHBoxLayout>
#include <QFontMetrics>
#include <QLabel>
#include <QMenu>
#include <QPoint>
#include <QIcon>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include "gui/StyleConstants.h"

namespace Longpath {

// Speaker button style — matches AetherSDR TitleBar.cpp:175-177:
//   transparent background, no border, 14 px glyph font, flattens
//   the checked state by dimming opacity (mute visual cue).
static const char* kSpeakerBtnStyle =
    "QPushButton { background: transparent; border: none; font-size: 16px; padding: 0; }"
    "QPushButton:checked { opacity: 0.4; }";

// Regler und Wertchip: dieselben Bauteile wie ueberall (Glas & Tiefe,
// 2026-09-17) — versenkte Rinne, erhabener Griff mit Akzentrand,
// Zahl im Glaschip. Hier standen die AetherSDR-Toene woertlich
// (#1a2a3a-Rinne, blaue Kugel, blaue Fuellung, blaustichiger Chip):
// in der Kopfleiste der einzige Regler, der noch nach Qt aussah, und
// ein blauer Balken mehr in einer Zeile, die ohnehin zu viel Blau trug.

// Der Lautsprecher als Zeichnung, nicht als Farb-Emoji (🔊/🔇):
// das Emoji war in der Kopfleiste die einzige bunte Figur und sah bei
// jeder Schriftart anders aus. Ein Trapez, zwei Boegen — stumm: ein
// Strich quer durch. Grau wie die Beschriftung; gemalt gross, Qt
// rechnet auf jede Dichte herunter.
static QIcon speakerIcon(bool muted)
{
    constexpr int sz = 64;
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor ink(muted ? Style::kTextInactive : Style::kTextSecondary);
    p.setPen(Qt::NoPen);
    p.setBrush(ink);
    QPolygonF horn;
    horn << QPointF(10, 24) << QPointF(22, 24) << QPointF(36, 12)
         << QPointF(36, 52) << QPointF(22, 40) << QPointF(10, 40);
    p.drawPolygon(horn);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(ink, 4.0, Qt::SolidLine, Qt::RoundCap));
    if (muted) {
        p.drawLine(QPointF(42, 24), QPointF(56, 40));
        p.drawLine(QPointF(56, 24), QPointF(42, 40));
    } else {
        p.drawArc(QRectF(30, 20, 20, 24), -60 * 16, 120 * 16);
        p.drawArc(QRectF(26, 10, 36, 44), -60 * 16, 120 * 16);
    }
    p.end();
    return QIcon(pm);
}

MasterOutputWidget::MasterOutputWidget(AudioEngine* audio, QWidget* parent)
    : QWidget(parent)
    , m_audio(audio)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // ── Saved state ────────────────────────────────────────────────────────
    // Read BEFORE we wire widget→model signals so seeding the slider
    // and button does not re-emit volumeChanged/mutedChanged back
    // into the engine. The m_updatingFromModel guard covers this
    // belt-and-braces style, but keeping the order correct keeps the
    // signal-flow graph readable.
    auto& s = AppSettings::instance();
    const float savedVolume = s.value(QStringLiteral("audio/Master/Volume"),
                                      QStringLiteral("1.0")).toString().toFloat();
    const bool gespeichertStumm = s.value(QStringLiteral("audio/Master/Muted"),
                                          QStringLiteral("False")).toString()
                                  == QStringLiteral("True");
    const QString savedDevice = s.value(QStringLiteral("audio/Speakers/DeviceName"),
                                        QString()).toString();
    m_currentDeviceName = savedDevice;

    // ── Der Hauptausgang startet nie stumm (2026-11-13) ───────────────
    //
    // Am 2026-10-15 kostete „kein Ton" eine Stunde, weil hier `True`
    // stand und nichts es sagte. Die Entscheidung damals: „Der Knopf
    // bleibt wie er ist; nur steht der Zustand jetzt auch dort, wo man
    // bei Stille zuerst nachsieht." Das Protokoll kam, der Knopf blieb.
    //
    // Einen Monat spaeter ist dieselbe Meldung wieder da („wieder kein
    // ton", 2026-11-13), und diesmal mit Zahlen: in den fuenf
    // Protokollen seit dem 2026-11-12 abends steht die Warnung
    // fuenfmal. JEDER Start war stumm. Das Protokoll hat die Suche von
    // einer Stunde auf zwei Minuten verkuerzt -- aber gefunden hat es
    // der, der Protokolle liest, nicht der, der funken will.
    //
    // Also die andere Haelfte: ein gespeicherter Stummschalter wird
    // beim Start geloest. Niemand startet ein Funkprogramm, um nichts
    // zu hoeren; ein stummer Hauptausgang in der Einstellungsdatei ist
    // ein Versehen der letzten Sitzung, kein Wunsch. Waehrend des
    // Betriebs bleibt der Knopf unveraendert -- wer stumm schaltet,
    // will stumm sein, bis er es zuruecknimmt.
    //
    // Die Lautstaerke wird NICHT angefasst: ein Regler auf 0 ist
    // sichtbar, ein Stummschalter nicht, und genau das war das Problem.
    const bool savedMuted = false;
    if (gespeichertStumm) {
        s.setValue(QStringLiteral("audio/Master/Muted"), QStringLiteral("False"));
        qCWarning(lcAudio).noquote()
            << QStringLiteral("Hauptausgang war STUMM gespeichert "
                              "(audio/Master/Muted=True) — beim Start "
                              "geloest. Der Lautsprecherknopf in der "
                              "Hauptausgabe schaltet ihn wieder stumm.");
    }

    // ── Und derselbe Schalter eine Ebene hoeher (2026-11-14) ──────────
    //
    // „ton geht schon wieder nicht … und. das haeuft sich!" Zum zweiten
    // Mal (nach dem 2026-11-12) war nicht Longpath stumm, sondern der
    // MAC: `osascript -e 'get volume settings'` sagte `output
    // muted:true`, waehrend hier alles offen stand -- Hauptausgang
    // nicht stumm, AF 50, PortAudio sauber auf „Externe Kopfhoerer",
    // die Maschine schickte Ton mit audio_mute 0.
    //
    // Der Verdacht ist F10: am Mac die Stummtaste, und der F-Tasten-
    // Vorrang (FKeyPriority) gilt nur, solange Longpath VORNE ist. Im
    // Protokoll desselben Laufs schaltet der Modus in vier Minuten
    // sechsmal hin und her -- sechs Fenster, in denen ein F10 fuer den
    // Sprachspeicher am Mac landet statt im Programm.
    //
    // Die Begruendung von oben gilt unveraendert: niemand startet ein
    // Funkprogramm, um nichts zu hoeren. Also beim Start loesen.
    // Waehrend des Betriebs wird NICHT gestellt, nur gemeldet -- wer
    // dann stumm schaltet, kann es gewollt haben.
    if (SystemOutputMute::isMuted() == SystemOutputMute::Muted) {
        const bool geloest = SystemOutputMute::setMuted(false);
        // Zwei getrennte Rufe statt eines Fragezeichens: der
        // Englisch-Pruefstand neutralisiert Protokollzeilen nur, wenn
        // das Literal an einer Zeile haengt, die mit `qC` oder `<<`
        // beginnt. In der Ternaerform stand es hinter einem `?` -- und
        // damit zaehlten diese beiden Protokolltexte als deutsche
        // OBERFLAECHE (CI 14.11., tst_oberflaeche_englisch: 12 statt 9).
        if (geloest) {
            qCWarning(lcAudio).noquote()
                << QStringLiteral("Der Ausgang des Betriebssystems war "
                                  "STUMM — beim Start geloest. Am Mac ist "
                                  "das die F10-Taste, wenn Longpath nicht "
                                  "vorne ist.");
        } else {
            qCWarning(lcAudio).noquote()
                << QStringLiteral("Der Ausgang des Betriebssystems ist "
                                  "STUMM und liess sich nicht loesen — "
                                  "ohne ihn bleibt Longpath still.");
        }
    }

    // ── Speaker button ─────────────────────────────────────────────────────
    m_speakerBtn = new QPushButton(this);
    m_speakerBtn->setIcon(speakerIcon(savedMuted));
    m_speakerBtn->setIconSize(QSize(18, 18));
    m_speakerBtn->setObjectName(QStringLiteral("speakerBtn"));
    m_speakerBtn->setFixedSize(20, 20);
    m_speakerBtn->setCheckable(true);
    m_speakerBtn->setChecked(savedMuted);
    m_speakerBtn->setStyleSheet(QLatin1String(kSpeakerBtnStyle));
    m_speakerTipBase = QStringLiteral(
        "Click to mute/unmute master output — right-click for output devices");
    m_speakerBtn->setToolTip(m_speakerTipBase);
    m_speakerBtn->setAccessibleName(QStringLiteral("Master mute"));
    m_speakerBtn->setAccessibleDescription(QStringLiteral(
        "Mute or unmute master output; right-click for device picker"));
    m_speakerBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_speakerBtn);

    // ── Slider (100 px wide, 16 px tall per design spec §7.3) ──────────────
    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setObjectName(QStringLiteral("masterSlider"));
    m_slider->setRange(0, 100);
    m_slider->setFixedWidth(100);
    m_slider->setFixedHeight(16);
    m_slider->setStyleSheet(Style::sliderHStyle());
    m_slider->setAccessibleName(QStringLiteral("Master volume"));
    m_slider->setAccessibleDescription(
        QStringLiteral("Master output volume level, 0 to 100 percent"));
    // Seed slider from saved linear volume.
    const int pct = static_cast<int>(savedVolume * 100.0f + 0.5f);
    m_slider->setValue(pct);
    layout->addWidget(m_slider);

    // ── Inset percent readout ──────────────────────────────────────────────
    m_dbLabel = new QLabel(QString::number(pct), this);
    m_dbLabel->setObjectName(QStringLiteral("dbLabel"));
    // Breit genug fuer "100" in der Schrift des Glaschips (Menlo 11 px,
    // insetValueStyle: 4 px Polster + 1 px Rahmen je Seite). Die festen
    // 22 Punkte von frueher zeigten die Vorgabe 100 als "L0(" -- am
    // 2026-09-22 im 0.6.4-Paket gesehen, seit dem Glaschip vom 17.09.
    {
        QFont chip = m_dbLabel->font();
        Longpath::Style::setzeMonoFamilien(chip);
        chip.setPixelSize(11);
        m_dbLabel->setFixedWidth(QFontMetrics(chip).horizontalAdvance(QStringLiteral("100")) + 10);
    }
    m_dbLabel->setAlignment(Qt::AlignCenter);
    m_dbLabel->setStyleSheet(Style::insetValueStyle());
    layout->addWidget(m_dbLabel);

    // ── Seed the engine with the saved values BEFORE wiring widget→model ──
    // so the engine matches the UI state at startup (same contract as
    // the in-session echo path — engine and widget agree). The saved
    // speakers device is applied too so a restart honors the device the
    // user last picked from the right-click menu; without this the
    // engine defaults to the platform default output until the user
    // reselects.
    if (m_audio) {
        m_audio->setVolume(savedVolume);
        m_audio->setMasterMuted(savedMuted);
        if (!savedDevice.isEmpty()) {
            // Load the full persisted speakers config (sampleRate,
            // bufferSamples, exclusiveMode, etc.) and override only the
            // device name with the title-bar picker's value.  Without
            // loadFromSettings here, every restart-time seed reverted
            // the bus to AudioDeviceConfig{}'s struct defaults and
            // silently discarded everything the user had tuned via
            // Setup → Devices.
            AudioDeviceConfig cfg = AudioDeviceConfig::loadFromSettings(
                QStringLiteral("audio/Speakers"));
            cfg.deviceName = savedDevice;
            m_audio->setSpeakersConfig(cfg);
        }
    }

    // ── Widget → Model: slider drives engine volume + persists ─────────────
    connect(m_slider, &QSlider::valueChanged, this, [this](int value) {
        if (m_updatingFromModel) {
            return;
        }
        const float v = static_cast<float>(value) / 100.0f;
        m_dbLabel->setText(QString::number(value));
        if (m_audio) {
            m_audio->setVolume(v);
        }
        auto& ss = AppSettings::instance();
        ss.setValue(QStringLiteral("audio/Master/Volume"),
                    QString::number(v, 'f', 3));
        ss.save();
        emit volumeChanged(v);
    });

    // ── Widget → Model: speaker button drives engine mute + persists ───────
    connect(m_speakerBtn, &QPushButton::toggled, this, [this](bool muted) {
        if (m_updatingFromModel) {
            return;
        }
        m_speakerBtn->setIcon(speakerIcon(muted));
        if (m_audio) {
            m_audio->setMasterMuted(muted);
        }
        auto& ss = AppSettings::instance();
        ss.setValue(QStringLiteral("audio/Master/Muted"),
                    muted ? QStringLiteral("True") : QStringLiteral("False"));
        ss.save();
        emit mutedChanged(muted);
    });

    // ── Right-click → device picker ────────────────────────────────────────
    connect(m_speakerBtn, &QWidget::customContextMenuRequested,
            this, &MasterOutputWidget::onSpeakerContextMenu);

    // ── Model → Widget echo paths ──────────────────────────────────────────
    if (m_audio) {
        connect(m_audio, &AudioEngine::volumeChanged,
                this, &MasterOutputWidget::onAudioEngineVolumeChanged);
        connect(m_audio, &AudioEngine::masterMutedChanged,
                this, &MasterOutputWidget::onAudioEngineMasterMutedChanged);
        // Sub-Phase 12 Task 12.2: live-sync device-name with Setup →
        // Audio → Devices edits. When the Devices page rebuilds the
        // speakers bus, the negotiated config arrives here and updates
        // the picker anchor so the right-click checkmark stays correct.
        connect(m_audio, &AudioEngine::speakersConfigChanged,
                this, &MasterOutputWidget::onSpeakersConfigChanged);
    }

    // ── Waechter ueber den Stummschalter des Betriebssystems ──────────
    //
    // Nur Melden, nicht Stellen (Begruendung oben). Zwei Sekunden sind
    // fein genug, um die Ursache noch im Zusammenhang zu sehen, und die
    // Abfrage ist eine CoreAudio-Eigenschaft, kein Geraetezugriff.
    m_systemMuteSeen = (SystemOutputMute::isMuted() == SystemOutputMute::Muted);
    m_systemMuteWatch = new QTimer(this);
    m_systemMuteWatch->setInterval(2000);
    connect(m_systemMuteWatch, &QTimer::timeout,
            this, &MasterOutputWidget::pruefeSystemStumm);
    m_systemMuteWatch->start();

    setLayout(layout);
}

void MasterOutputWidget::pruefeSystemStumm()
{
    const SystemOutputMute::State zustand = SystemOutputMute::isMuted();
    if (zustand == SystemOutputMute::Unknown) {
        // Kein Weg dorthin (Linux/Windows) oder die Abfrage schlug fehl.
        // Abwesenheit ist keine Aussage: der zuletzt gesehene Zustand
        // bleibt stehen, damit kein Phantomwechsel gemeldet wird.
        return;
    }

    const bool stumm = (zustand == SystemOutputMute::Muted);
    if (stumm == m_systemMuteSeen) {
        return;
    }
    m_systemMuteSeen = stumm;

    if (stumm) {
        qCWarning(lcAudio).noquote()
            << QStringLiteral("Der Ausgang des Betriebssystems ist gerade "
                              "STUMM geworden — Longpath selbst ist offen. "
                              "Am Mac ist das die F10-Taste, wenn Longpath "
                              "nicht vorne ist.");
        // Englisch, weil es im FENSTER steht: der Sprechblasentext ist
        // Oberflaeche, keine Protokollzeile (Betreiber: „englisch
        // durchgehend, aber deutsch als option").
        m_speakerBtn->setToolTip(
            tr("The system output is muted — Longpath itself is open.\n")
            + m_speakerTipBase);
        emit systemOutputMutedDetected();
    } else {
        qCInfo(lcAudio).noquote()
            << QStringLiteral("Der Ausgang des Betriebssystems ist wieder "
                              "offen.");
        m_speakerBtn->setToolTip(m_speakerTipBase);
    }
}

void MasterOutputWidget::setCurrentOutputDevice(const QString& name)
{
    // Sync-from-elsewhere path only — do NOT emit outputDeviceChanged.
    // The caller (Setup → Audio → Devices or the picker's own action
    // handler) is responsible for emitting that signal exactly once
    // per user action.
    m_currentDeviceName = name;
}

void MasterOutputWidget::onSpeakerContextMenu(const QPoint& pos)
{
    ScopedChildWidget<QMenu> menuOwner(window());   // stirbt nicht mit dem Rahmen, siehe ScopedChildWidget.h
    const QPointer<MasterOutputWidget> self(this);   // Menue am Fenster: ein nativer Vorfahr gab ihm keinen Fensterbezug (2026-09-30)
    QMenu& menu = *menuOwner.get();
    menu.setTitle(QStringLiteral("Output device"));

    auto* group = new QActionGroup(&menu);
    group->setExclusive(true);

    // Enumerate host APIs + their output devices. Each API becomes a
    // section header followed by its devices. The design spec allows
    // either a flat list or per-API submenus; flat-with-section-headers
    // reads best at this widget's size.
    const auto apis = PortAudioBus::hostApis();
    bool anyDevice = false;
    for (const auto& api : apis) {
        const auto devices = PortAudioBus::outputDevicesFor(api.index);
        if (devices.isEmpty()) {
            continue;
        }
        QAction* header = menu.addSection(api.name);
        Q_UNUSED(header);
        for (const auto& dev : devices) {
            QAction* act = menu.addAction(dev.name);
            act->setCheckable(true);
            act->setChecked(dev.name == m_currentDeviceName);
            group->addAction(act);
            const QString devName = dev.name;
            connect(act, &QAction::triggered, this, [this, devName]() {
                if (devName == m_currentDeviceName) {
                    return;
                }
                m_currentDeviceName = devName;
                emit outputDeviceChanged(devName);
                auto& ss = AppSettings::instance();
                ss.setValue(QStringLiteral("audio/Speakers/DeviceName"), devName);
                ss.save();
            });
            anyDevice = true;
        }
    }

    if (!anyDevice) {
        QAction* none = menu.addAction(QStringLiteral("No output devices"));
        none->setEnabled(false);
    }

    menu.exec(m_speakerBtn->mapToGlobal(pos));
    // Das Elternteil kann waehrend exec() gestorben sein — dann ist
    // auch das Menue weg und `this` eine Leiche. Siehe ScopedChildWidget.h.
    if (!menuOwner || !self) { return; }
}

void MasterOutputWidget::onAudioEngineVolumeChanged(float v)
{
    m_updatingFromModel = true;
    const int pct = static_cast<int>(v * 100.0f + 0.5f);
    m_slider->setValue(pct);
    m_dbLabel->setText(QString::number(pct));
    m_updatingFromModel = false;
}

void MasterOutputWidget::onAudioEngineMasterMutedChanged(bool m)
{
    m_updatingFromModel = true;
    // QSignalBlocker prevents the button's toggled() from re-entering
    // the widget→model lambda while we mirror the engine state into
    // the UI. The text update still has to happen manually because
    // the toggled() handler (which normally sets the glyph) is blocked.
    {
        QSignalBlocker blocker(m_speakerBtn);
        m_speakerBtn->setChecked(m);
        m_speakerBtn->setIcon(speakerIcon(m));
    }
    m_updatingFromModel = false;
}

void MasterOutputWidget::onSpeakersConfigChanged(const AudioDeviceConfig& cfg)
{
    // Sub-Phase 12 Task 12.2: keep the right-click picker anchor in sync
    // with whatever device the engine is actually using. This is a
    // sync-from-engine path — do NOT emit outputDeviceChanged (that signal
    // is user-action only per the widget contract in the header comment).
    m_currentDeviceName = cfg.deviceName;
}

} // namespace Longpath
