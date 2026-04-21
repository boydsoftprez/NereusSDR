// =================================================================
// src/gui/widgets/MasterOutputWidget.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original widget. See MasterOutputWidget.h for the full
// attribution block. Styling references AetherSDR
// `src/gui/TitleBar.cpp:172-215`; the structure here is original to
// NereusSDR because this widget isolates JUST the master-output
// triad from AetherSDR's monolithic TitleBar.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Written by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Anthropic Claude Code.
//                Phase 3O Sub-Phase 10 Task 10b.
// =================================================================

#include "MasterOutputWidget.h"

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/audio/PortAudioBus.h"

#include <QAction>
#include <QActionGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPoint>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>

namespace NereusSDR {

// Speaker button style — matches AetherSDR TitleBar.cpp:175-177:
//   transparent background, no border, 14 px glyph font, flattens
//   the checked state by dimming opacity (mute visual cue).
static const char* kSpeakerBtnStyle =
    "QPushButton { background: transparent; border: none; font-size: 14px; padding: 0; }"
    "QPushButton:checked { opacity: 0.4; }";

// Horizontal volume slider — copies AetherSDR TitleBar.cpp:195-198
// palette verbatim: #1a2a3a groove, #00b4d8 handle + sub-page fill.
static const char* kSliderStyle =
    "QSlider::groove:horizontal { background: #1a2a3a; height: 4px; border-radius: 2px; }"
    "QSlider::handle:horizontal { background: #00b4d8; width: 10px; margin: -3px 0; border-radius: 5px; }"
    "QSlider::sub-page:horizontal { background: #00b4d8; border-radius: 2px; }";

// Inset value readout — STYLEGUIDE.md line 137 "Inset Value Display".
static const char* kDbLabelStyle =
    "QLabel {"
    "  font-size: 10px;"
    "  background: #0a0a18;"
    "  border: 1px solid #1e2e3e;"
    "  border-radius: 3px;"
    "  padding: 1px 2px;"
    "  color: #8aa8c0;"
    "}";

// UTF-8 encodings of the two speaker glyphs — kept as raw escape
// bytes to match the AetherSDR source style (no <QChar> fuss).
static const char* kSpeakerOn  = "\xF0\x9F\x94\x8A";  // 🔊 U+1F50A
static const char* kSpeakerOff = "\xF0\x9F\x94\x87";  // 🔇 U+1F507

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
    const bool savedMuted = s.value(QStringLiteral("audio/Master/Muted"),
                                    QStringLiteral("False")).toString()
                            == QStringLiteral("True");
    const QString savedDevice = s.value(QStringLiteral("audio/Speakers/DeviceName"),
                                        QString()).toString();
    m_currentDeviceName = savedDevice;

    // ── Speaker button ─────────────────────────────────────────────────────
    m_speakerBtn = new QPushButton(
        QString::fromUtf8(savedMuted ? kSpeakerOff : kSpeakerOn), this);
    m_speakerBtn->setObjectName(QStringLiteral("speakerBtn"));
    m_speakerBtn->setFixedSize(20, 20);
    m_speakerBtn->setCheckable(true);
    m_speakerBtn->setChecked(savedMuted);
    m_speakerBtn->setStyleSheet(QLatin1String(kSpeakerBtnStyle));
    m_speakerBtn->setToolTip(QStringLiteral(
        "Click to mute/unmute master output — right-click for output devices"));
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
    m_slider->setStyleSheet(QLatin1String(kSliderStyle));
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
    m_dbLabel->setFixedWidth(22);
    m_dbLabel->setAlignment(Qt::AlignCenter);
    m_dbLabel->setStyleSheet(QLatin1String(kDbLabelStyle));
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
            AudioDeviceConfig cfg;
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
        m_speakerBtn->setText(QString::fromUtf8(muted ? kSpeakerOff : kSpeakerOn));
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

    setLayout(layout);
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
    QMenu menu(this);
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
        m_speakerBtn->setText(QString::fromUtf8(m ? kSpeakerOff : kSpeakerOn));
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

} // namespace NereusSDR
