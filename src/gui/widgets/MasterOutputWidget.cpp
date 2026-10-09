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
//   2026-09-23 - R-R3-23: save the picked output device before
//                announcing it (selectOutputDevice). J.J. Boyd
//                (KG4VCF), with AI-assisted implementation via
//                Anthropic Claude Code.
//   2026-10-06 - Radio speaker plan Task 6 (R-SPK-17, D1, D5): pc-on /
//                pc-muted icons through AppIcon replace the speaker emoji,
//                a "PC" word label (pcLabel) sits between the button and
//                the slider, and HeaderVolumeStyle exports the styles the
//                RADIO group shares. J.J. Boyd (KG4VCF), with AI-assisted
//                implementation via Anthropic Claude Code.
//   2026-10-06 - Radio speaker plan Task 6, JJ decision 2 (R-SPK-17, D1):
//                setStacked() switches the PC group to the stacked form
//                (thin row, small icon and readout, same handle) and
//                HeaderVolumeStyle::applyForm() sizes either group for
//                either form. J.J. Boyd (KG4VCF), with AI-assisted
//                implementation via Anthropic Claude Code.
//   2026-10-09 - Native audio plan Task 19 (R-AUD-23, R-AUD-03, R-AUD-08,
//                R-AUD-11, R-AUD-12, D20): the speakers menu of option A of
//                header-menu-mockup.html from the engine's device catalogue
//                (audioDeviceEntries), the tooltip from the speakers status,
//                "Sound setup…", and a pick saved as the Outputs card saves
//                it. J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                Anthropic Claude Code.
// =================================================================

#include "MasterOutputWidget.h"

#include "gui/setup/AudioDriverList.h"
#include "gui/styles/PopupMenuStyle.h"
#include "gui/widgets/AppIcon.h"

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"

#include <QAction>
#include <QChar>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QPoint>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QWidgetAction>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>

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

// The speaker button shows the app's own icons (R-SPK-19, D7), which look
// the same on every platform, in place of the two emoji glyphs it drew as
// text before.
static const char* kPcOnIcon    = "pc-on";
static const char* kPcMutedIcon = "pc-muted";

namespace {

constexpr char kSpeakersPrefix[] = "audio/Speakers";

// D10: the saved WASAPI option keys stay in the file, never rewritten (as
// the Outputs card keeps them).
constexpr std::array<const char*, 3> kRetiredWasapiKeys{"ExclusiveMode", "EventDriven",
                                                        "BypassMixer"};

// Each menu row carries its kind under this property, for the tests.
constexpr char kEntryKindProperty[] = "speakerMenuEntry";

// Option A of header-menu-mockup.html, on the house popup style
// (kPopupMenu): the heading, an interface's name over its pairs, the line
// under a missing choice, and the missing choice in amber.
const char* const kHeadingStyle =
    "QLabel { color: #6a8098; font-size: 9px; font-weight: 600; padding: 5px 14px 2px 8px; }";
const char* const kGroupStyle =
    "QLabel { color: #8090a0; font-size: 10px; padding: 4px 14px 1px 20px; }";
const char* const kNoteStyle =
    "QLabel { color: #8090a0; font-size: 10px; padding: 0 14px 4px 20px; }";
const char* const kMissingStyle =
    "QLabel { color: #e0a030; padding: 4px 20px 4px 7px; }";

// A pair is indented under its interface's name.
const QString kPairIndent = QStringLiteral("    ");

QString middleDot()
{
    return QLatin1Char(' ') + QChar(0x00B7) + QLatin1Char(' ');
}

// R-AUD-02's first native choice for this build, while the engine has no
// catalogue to say which it uses (as the Setup cards show it).
AudioEngineKind buildDefaultEngine()
{
#if defined(Q_OS_MAC)
    return AudioEngineKind::CoreAudio;
#elif defined(Q_OS_WIN)
    return AudioEngineKind::WindowsShared;
#else
    return AudioEngineKind::PipeWire;
#endif
}

QWidgetAction* addLabelRow(QMenu* menu, const QString& text, const char* style, const char* kind)
{
    auto* action = new QWidgetAction(menu);
    auto* label = new QLabel(text);
    label->setStyleSheet(QLatin1String(style));
    action->setDefaultWidget(label);   // the action owns the label
    action->setText(text);
    action->setEnabled(false);
    action->setProperty(kEntryKindProperty, QLatin1String(kind));
    menu->addAction(action);
    return action;
}

QString troubleWords(AudioRoleReason reason)
{
    return reason == AudioRoleReason::InUse ? QStringLiteral("is in use by another program")
                                            : QStringLiteral("is not connected");
}

bool troubled(const AudioRoleStatus& status)
{
    return status.state != AudioRoleState::Off
        && (status.reason == AudioRoleReason::NotConnected
            || status.reason == AudioRoleReason::InUse);
}

QString chosenNameOf(const AudioRoleStatus& status)
{
    return status.chosenName.isEmpty() ? status.chosen.deviceName : status.chosenName;
}

} // namespace

namespace HeaderVolumeStyle {

const char* const kIconButton =
    "QPushButton { background: transparent; border: none; padding: 0; }";

// The word label of layout A in header-layouts.html: 9 px, semi-bold, the
// readout's text colour; the mockup's disabled grey while disabled.
const char* const kWordLabel =
    "QLabel { color: #8aa8c0; font-size: 9px; font-weight: 600; }"
    "QLabel:disabled { color: #4a5a6a; }";

const char* const kPcSlider = kSliderStyle;

const char* const kRadioSlider =
    "QSlider::groove:horizontal { background: #1a2a3a; height: 4px; border-radius: 2px; }"
    "QSlider::handle:horizontal { background: #e0a030; width: 10px; margin: -3px 0; border-radius: 5px; }"
    "QSlider::sub-page:horizontal { background: #e0a030; border-radius: 2px; }"
    "QSlider::handle:horizontal:disabled { background: #4a5a6a; }"
    "QSlider::sub-page:horizontal:disabled { background: #1a2a3a; }";

const char* const kReadout =
    "QLabel {"
    "  font-size: 10px;"
    "  background: #0a0a18;"
    "  border: 1px solid #1e2e3e;"
    "  border-radius: 3px;"
    "  padding: 1px 2px;"
    "  color: #8aa8c0;"
    "}"
    "QLabel:disabled { color: #4a5a6a; }";

const char* const kReadoutStacked =
    "QLabel {"
    "  font-size: 9px;"
    "  background: #0a0a18;"
    "  border: 1px solid #1e2e3e;"
    "  border-radius: 3px;"
    "  padding: 0 2px;"
    "  color: #8aa8c0;"
    "}"
    "QLabel:disabled { color: #4a5a6a; }";

int applyForm(QPushButton* button, QLabel* word, QSlider* slider, QLabel* readout,
              bool stacked, int stackedLabelWidth, const char* readoutStyle)
{
    if (QLayout* row = button->parentWidget()->layout()) {
        row->setSpacing(stacked ? 3 : 4);
    }
    if (stacked) {
        button->setFixedSize(14, 14);
        slider->setFixedSize(84, 12);
        readout->setFixedSize(20, 13);
        readout->setStyleSheet(QLatin1String(kReadoutStacked));
        if (stackedLabelWidth > 0) {
            word->setFixedWidth(stackedLabelWidth);
            word->setVisible(true);
        } else {
            word->setVisible(false);
        }
        return kStackedIconPx;
    }
    button->setFixedSize(20, 20);
    slider->setFixedSize(100, 16);
    readout->setFixedWidth(22);
    readout->setMinimumHeight(0);
    readout->setMaximumHeight(QWIDGETSIZE_MAX);
    readout->setStyleSheet(QLatin1String(readoutStyle));
    word->setMinimumWidth(0);
    word->setMaximumWidth(QWIDGETSIZE_MAX);
    word->setVisible(true);
    return kIconPx;
}

} // namespace HeaderVolumeStyle

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

    // ── Speaker button ─────────────────────────────────────────────────────
    m_speakerBtn = new QPushButton(this);
    m_speakerBtn->setObjectName(QStringLiteral("speakerBtn"));
    m_speakerBtn->setFixedSize(20, 20);
    m_speakerBtn->setCheckable(true);
    m_speakerBtn->setChecked(savedMuted);
    m_speakerBtn->setStyleSheet(QLatin1String(kSpeakerBtnStyle));
    applySpeakerIcon(savedMuted);
    // R-AUD-23: the tooltip (refreshSpeakerToolTip) names the device playing.
    m_speakerBtn->setAccessibleName(QStringLiteral("Master mute"));
    m_speakerBtn->setAccessibleDescription(QStringLiteral(
        "Mute or unmute master output; right-click for speakers"));
    m_speakerBtn->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_speakerBtn);

    // ── "PC" word label (R-SPK-17, D1) ─────────────────────────────────────
    // Tells this computer's group apart from RADIO at a glance.
    m_pcLabel = new QLabel(QStringLiteral("PC"), this);
    m_pcLabel->setObjectName(QStringLiteral("pcLabel"));
    m_pcLabel->setStyleSheet(QLatin1String(HeaderVolumeStyle::kWordLabel));
    layout->addWidget(m_pcLabel);

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
        applySpeakerIcon(muted);
        if (m_audio) {
            m_audio->setMasterMuted(muted);
        }
        auto& ss = AppSettings::instance();
        ss.setValue(QStringLiteral("audio/Master/Muted"),
                    muted ? QStringLiteral("True") : QStringLiteral("False"));
        ss.save();
        emit mutedChanged(muted);
    });

    // ── Right-click → speakers menu ────────────────────────────────────────
    connect(m_speakerBtn, &QWidget::customContextMenuRequested,
            this, &MasterOutputWidget::onSpeakerContextMenu);

    // ── Model → Widget echo paths ──────────────────────────────────────────
    connectEngine();
    refreshSpeakerToolTip();

    setLayout(layout);
}

void MasterOutputWidget::connectEngine()
{
    if (!m_audio) {
        return;
    }
    connect(m_audio, &AudioEngine::volumeChanged,
            this, &MasterOutputWidget::onAudioEngineVolumeChanged);
    connect(m_audio, &AudioEngine::masterMutedChanged,
            this, &MasterOutputWidget::onAudioEngineMasterMutedChanged);
    // Sub-Phase 12 Task 12.2: live-sync with Setup → Audio → Devices edits.
    connect(m_audio, &AudioEngine::speakersConfigChanged,
            this, &MasterOutputWidget::onSpeakersConfigChanged);
    // R-AUD-12 / R-AUD-23: the speakers status names the device chosen and
    // the device playing; the tooltip follows it at once.
    connect(m_audio, &AudioEngine::roleStatusChanged, this,
            [this](AudioRole role, const AudioRoleStatus&) {
                if (role != AudioRole::Speakers) {
                    return;
                }
                attachCatalogue();
                refreshSpeakerToolTip();
            });
    attachCatalogue();
}

void MasterOutputWidget::setAudioEngine(AudioEngine* engine)
{
    if (m_audio == engine) {
        return;
    }
    if (m_audio) {
        disconnect(m_audio, nullptr, this, nullptr);
    }
    if (m_catalogue) {
        disconnect(m_catalogue, nullptr, this, nullptr);
    }
    m_catalogue = nullptr;
    m_audio = engine;
    connectEngine();
    refreshSpeakerToolTip();
}

IAudioDeviceCatalog* MasterOutputWidget::attachCatalogue()
{
    if (m_catalogue) {
        return m_catalogue;
    }
    if (!m_audio) {
        return nullptr;
    }
    // Null until the engine's device layer has a catalogue to show.
    IAudioDeviceCatalog* catalogue = m_audio->catalogue();
    if (catalogue == nullptr) {
        return nullptr;
    }
    m_catalogue = catalogue;
    // R-AUD-03: the menu is built from the catalogue each time it opens, so
    // a device added or removed is in the next menu with no Rescan; the
    // tooltip follows the default and the list here.
    connect(catalogue, &IAudioDeviceCatalog::devicesChanged, this,
            [this]() { refreshSpeakerToolTip(); });
    connect(catalogue, &IAudioDeviceCatalog::defaultChanged, this,
            [this](AudioDeviceDirection direction) {
                if (direction == AudioDeviceDirection::Output) {
                    refreshSpeakerToolTip();
                }
            });
    return catalogue;
}

void MasterOutputWidget::setCurrentOutputDevice(const QString& name)
{
    // Sync-from-elsewhere path only — do NOT emit outputDeviceChanged.
    // The caller (Setup → Audio → Devices or the picker's own action
    // handler) is responsible for emitting that signal exactly once
    // per user action. The menu reads the choice each time it opens.
    Q_UNUSED(name);
    refreshSpeakerToolTip();
}

AudioRoleStatus MasterOutputWidget::speakersStatus() const
{
    return m_audio ? m_audio->roleStatus(AudioRole::Speakers) : AudioRoleStatus{};
}

AudioDeviceConfig MasterOutputWidget::speakersChoice() const
{
    const AudioDeviceConfig saved =
        AudioDeviceConfig::loadFromSettings(QLatin1String(kSpeakersPrefix));
    const AudioRoleStatus status = speakersStatus();
    if (status.state == AudioRoleState::Off) {
        return saved;
    }
    // The device the speakers are set to, not the default a missing one
    // falls back to (speakersConfigChanged names that one).
    AudioDeviceConfig chosen = status.chosen;
    if (!chosen.engine) {
        chosen.engine = saved.engine;
    }
    return chosen;
}

AudioEngineKind MasterOutputWidget::choiceEngine(const AudioDeviceConfig& choice) const
{
    if (choice.engine) {
        return *choice.engine;
    }
    if (m_audio && m_catalogue) {
        return m_audio->defaultEngine();
    }
    return buildDefaultEngine();
}

void MasterOutputWidget::refreshSpeakerToolTip()
{
    QString tip = QStringLiteral("PC volume. Click to mute, right-click for speakers.");
    const AudioRoleStatus status = speakersStatus();
    const QString name = chosenNameOf(status);
    if (troubled(status) && !name.isEmpty() && status.state == AudioRoleState::PlayingOnDefault
        && !status.playingName.isEmpty()) {
        tip += QStringLiteral(" %1 %2; playing on %3 meanwhile.")
                   .arg(name, troubleWords(status.reason), status.playingName);
    } else if (troubled(status) && !name.isEmpty() && status.state == AudioRoleState::Silent) {
        tip += QStringLiteral(" %1 %2.").arg(name, troubleWords(status.reason));
    } else if ((status.state == AudioRoleState::Playing
                || status.state == AudioRoleState::PlayingOnDefault)
               && !status.playingName.isEmpty()) {
        tip += QStringLiteral(" Playing on %1.").arg(status.playingName);
    }
    if (m_speakerBtn->toolTip() != tip) {
        m_speakerBtn->setToolTip(tip);
    }
}

QMenu* MasterOutputWidget::buildSpeakerMenuForTest()
{
    return buildSpeakerMenu();
}

QMenu* MasterOutputWidget::buildSpeakerMenu()
{
    auto* menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("speakerMenu"));
    menu->setStyleSheet(QString::fromLatin1(kPopupMenu));
    menu->setToolTipsVisible(true);

    const AudioRoleStatus status = speakersStatus();
    const AudioDeviceConfig choice = speakersChoice();
    IAudioDeviceCatalog* catalogue = attachCatalogue();
    const AudioEngineKind engine = choiceEngine(choice);
    const bool older = engine == AudioEngineKind::PortAudio;
    const QString hostApi = older ? choice.driverApi : QString();

    // D20: "Speakers · <driver>", the speakers' own driver only.
    QString driver = audioEngineLabel(engine);
    if (older && !hostApi.isEmpty()) {
        driver = olderDriverDisplayName(hostApi);
    }
    QWidgetAction* heading =
        addLabelRow(menu, QStringLiteral("Speakers") + middleDot() + driver, kHeadingStyle,
                    "heading");
    if (auto* label = qobject_cast<QLabel*>(heading->defaultWidget())) {
        QFont font = label->font();
        font.setCapitalization(QFont::AllUppercase);
        label->setFont(font);
    }

    auto addSoundSetup = [this, menu]() {
        menu->addSeparator();
        QAction* setup = menu->addAction(QStringLiteral("Sound setup") + QChar(0x2026));
        setup->setProperty(kEntryKindProperty, QStringLiteral("setup"));
        connect(setup, &QAction::triggered, this, [this]() { emit soundSetupRequested(); });
    };

    if (catalogue == nullptr) {
        // Disabled, never hidden: the choice shows, greyed with the reason.
        const QString reason = QStringLiteral("The device lists are not ready.");
        auto addChoiceRow = [menu, &reason](const QString& text, bool checked) {
            QAction* action = menu->addAction(text);
            action->setCheckable(true);
            action->setChecked(checked);
            action->setEnabled(false);
            action->setToolTip(reason);
            action->setProperty(kEntryKindProperty, QStringLiteral("device"));
        };
        addChoiceRow(QStringLiteral("(platform default)"), choice.isPlatformDefault());
        if (choice.isNone()) {
            addChoiceRow(QString::fromLatin1(kAudioDeviceNone), true);
        } else if (!choice.isPlatformDefault()) {
            QString name = choice.deviceName.isEmpty() ? choice.deviceId : choice.deviceName;
            if (choice.firstChannel > 1) {
                name += middleDot()
                    + audioPairLabel(AudioDeviceDirection::Output,
                                     AudioChannelPair{choice.firstChannel, 2});
            }
            addChoiceRow(name, true);
        }
        addLabelRow(menu, reason, kNoteStyle, "notReady");
        addSoundSetup();
        return menu;
    }

    const AudioBackendId backend = audioBackendFor(engine);
    QList<AudioDeviceInfo> devices;
    for (const AudioDeviceInfo& info : catalogue->devices(backend, AudioDeviceDirection::Output)) {
        if (older && !hostApi.isEmpty() && info.hostApi != hostApi) {
            continue;
        }
        devices.append(info);
    }
    auto deviceWithId = [&devices](const QString& id) -> std::optional<AudioDeviceInfo> {
        for (const AudioDeviceInfo& info : devices) {
            if (info.id == id) {
                return info;
            }
        }
        return std::nullopt;
    };

    QList<AudioDeviceEntry> entries = audioDeviceEntries(*catalogue, engine, hostApi,
                                                         AudioDeviceDirection::Output, choice);
    // The saved choice the list does not have comes last, as "<name> (not
    // connected)"; the menu puts it on top (D20).
    std::optional<AudioDeviceEntry> missing;
    if (entries.size() > 1) {
        const AudioDeviceEntry& last = entries.constLast();
        const std::optional<AudioDeviceInfo> info = deviceWithId(last.deviceId);
        const bool listedAsIs = info && info->channelCount <= 2;
        if (last.state == AudioDeviceState::NotConnected && last.group.isEmpty() && !listedAsIs) {
            missing = last;
            entries.removeLast();
        }
    }

    // R-AUD-08 / R-AUD-11: a missing or busy choice, ticked, in amber, with
    // where the speakers play meanwhile.
    QString topLabel;
    if (missing) {
        topLabel = missing->label;
    } else if (troubled(status) && !chosenNameOf(status).isEmpty()) {
        topLabel = chosenNameOf(status)
            + (status.reason == AudioRoleReason::InUse
                   ? QStringLiteral(" (in use by another program)")
                   : QStringLiteral(" (not connected)"));
    }
    if (!topLabel.isEmpty()) {
        QWidgetAction* top = addLabelRow(menu, topLabel, kMissingStyle, "missing");
        if (auto* label = qobject_cast<QLabel*>(top->defaultWidget())) {
            label->setText(QString(QChar(0x2713)) + QStringLiteral("  ") + topLabel);
        }
        top->setCheckable(true);
        top->setChecked(true);
        QString playingOn;
        if (status.state == AudioRoleState::PlayingOnDefault) {
            playingOn = status.playingName;
        } else if (status.state == AudioRoleState::Off) {
            if (const std::optional<AudioDeviceInfo> def =
                    catalogue->defaultDevice(backend, AudioDeviceDirection::Output)) {
                playingOn = def->name;
            }
        }
        if (!playingOn.isEmpty()) {
            addLabelRow(menu, QStringLiteral("Playing on %1 until it comes back.").arg(playingOn),
                        kNoteStyle, "missingNote");
        }
        menu->addSeparator();
    }

    // The driver's devices: "(platform default)" first, pairs indented under
    // their interface's name, the choice ticked.
    bool ticked = !topLabel.isEmpty();
    bool anyDevice = false;
    QString lastGroup;
    for (int i = 0; i < entries.size(); ++i) {
        const AudioDeviceEntry& e = entries.at(i);
        const bool platformDefault = i == 0;
        const bool none = e.deviceId == QLatin1String(kAudioDeviceNone);
        if (!e.group.isEmpty() && e.group != lastGroup) {
            addLabelRow(menu, e.group, kGroupStyle, "group");
        }
        lastGroup = e.group;

        QString text = e.label;
        if (!e.group.isEmpty()) {
            const QString prefix = e.group + middleDot();
            if (text.startsWith(prefix)) {
                text = text.mid(prefix.size());
            }
            text = kPairIndent + text;
        }
        QAction* action = menu->addAction(text);
        action->setCheckable(true);
        action->setProperty(kEntryKindProperty, QStringLiteral("device"));

        QString name;
        if (none) {
            name = QString::fromLatin1(kAudioDeviceNone);
        } else if (!platformDefault) {
            const std::optional<AudioDeviceInfo> info = deviceWithId(e.deviceId);
            name = info ? info->name : e.label;
            anyDevice = true;
        }

        bool isChoice = false;
        if (platformDefault) {
            isChoice = choice.isPlatformDefault();
        } else if (none) {
            isChoice = choice.isNone();
        } else {
            const bool sameDevice = choice.deviceId.isEmpty()
                ? (!choice.deviceName.isEmpty() && name == choice.deviceName)
                : e.deviceId == choice.deviceId;
            const bool samePair =
                e.group.isEmpty() || e.pair.firstChannel == std::max(1, choice.firstChannel);
            isChoice = sameDevice && samePair;
        }
        if (isChoice && !ticked) {
            action->setChecked(true);
            ticked = true;
        }

        SpeakerPick pick;
        pick.engine = engine;
        pick.hostApi = hostApi;
        pick.deviceId = platformDefault ? QString() : e.deviceId;
        pick.deviceName = name;
        pick.firstChannel = std::max(1, e.pair.firstChannel);
        connect(action, &QAction::triggered, this, [this, pick]() { selectOutputDevice(pick); });
    }

    // Disabled, never hidden: ASIO with no device says so.
    if (engine == AudioEngineKind::Asio && !anyDevice && topLabel.isEmpty()) {
        QAction* noDevices = menu->addAction(QStringLiteral("No ASIO devices present"));
        noDevices->setEnabled(false);
        noDevices->setProperty(kEntryKindProperty, QStringLiteral("noDevices"));
    }

    addSoundSetup();
    return menu;
}

void MasterOutputWidget::onSpeakerContextMenu(const QPoint& pos)
{
    std::unique_ptr<QMenu> menu(buildSpeakerMenu());
    menu->exec(m_speakerBtn->mapToGlobal(pos));
}

void MasterOutputWidget::selectOutputDevice(const SpeakerPick& pick)
{
    const AudioDeviceConfig current = speakersChoice();
    const bool sameEngine = choiceEngine(current) == pick.engine
        && (pick.engine != AudioEngineKind::PortAudio || current.driverApi == pick.hostApi);
    const bool sameDevice = pick.deviceId.isEmpty()
        ? (current.deviceId.isEmpty() && current.deviceName == pick.deviceName)
        : current.deviceId == pick.deviceId;
    if (sameEngine && sameDevice && std::max(1, current.firstChannel) == pick.firstChannel) {
        return;   // the choice already
    }

    // R-AUD-04: saved as the Outputs card saves a pick (DeviceCard's
    // currentConfig and onAnyControlChanged): the rest of the speakers'
    // settings kept; Engine, DeviceId, DeviceName and FirstChannel set.
    AudioDeviceConfig cfg = AudioDeviceConfig::loadFromSettings(QLatin1String(kSpeakersPrefix));
    cfg.engine = pick.engine;
    cfg.driverApi = pick.engine == AudioEngineKind::PortAudio ? pick.hostApi : QString();
    cfg.hostApiIndex = -1;
    cfg.deviceId = pick.deviceId;
    cfg.deviceName = pick.deviceName;
    cfg.firstChannel = pick.firstChannel;

    // R-R3-23: saved before the announcement; see the header.
    auto& ss = AppSettings::instance();
    const QString prefix = QLatin1String(kSpeakersPrefix);
    std::array<std::optional<QString>, kRetiredWasapiKeys.size()> retired;
    for (std::size_t i = 0; i < kRetiredWasapiKeys.size(); ++i) {
        const QString key = prefix + QLatin1Char('/') + QLatin1String(kRetiredWasapiKeys[i]);
        if (ss.contains(key)) {
            retired[i] = ss.value(key).toString();
        }
    }
    cfg.saveToSettings(prefix);
    for (std::size_t i = 0; i < kRetiredWasapiKeys.size(); ++i) {
        const QString key = prefix + QLatin1Char('/') + QLatin1String(kRetiredWasapiKeys[i]);
        if (retired[i]) {
            ss.setValue(key, *retired[i]);
        } else {
            ss.remove(key);
        }
    }
    ss.save();
    emit outputDeviceChanged(pick.deviceName);
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
    // the UI. The icon update still has to happen manually because
    // the toggled() handler (which normally sets the icon) is blocked.
    {
        QSignalBlocker blocker(m_speakerBtn);
        m_speakerBtn->setChecked(m);
        applySpeakerIcon(m);
    }
    m_updatingFromModel = false;
}

void MasterOutputWidget::applySpeakerIcon(bool muted)
{
    AppIcon::apply(m_speakerBtn,
                   QLatin1String(muted ? kPcMutedIcon : kPcOnIcon),
                   m_iconPx);
}

void MasterOutputWidget::setStacked(bool stacked, int stackedLabelWidth)
{
    m_stacked = stacked;
    m_iconPx = HeaderVolumeStyle::applyForm(m_speakerBtn, m_pcLabel, m_slider, m_dbLabel,
                                            stacked, stackedLabelWidth, kDbLabelStyle);
    applySpeakerIcon(m_speakerBtn->isChecked());
}

void MasterOutputWidget::onSpeakersConfigChanged(const AudioDeviceConfig& cfg)
{
    // Sub-Phase 12 Task 12.2: a sync-from-engine path, so do NOT emit
    // outputDeviceChanged (that signal is user-action only per the widget
    // contract in the header comment). The config names the device that
    // opened, the default a missing choice falls back to included, so the
    // menu's tick comes from the speakers status instead.
    Q_UNUSED(cfg);
    refreshSpeakerToolTip();
}

} // namespace NereusSDR
