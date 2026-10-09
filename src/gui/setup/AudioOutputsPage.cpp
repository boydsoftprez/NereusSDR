// =================================================================
// src/gui/setup/AudioOutputsPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup > Audio > Outputs page.
// See AudioOutputsPage.h for the full header.
//
// 2026-10-06: Written for the radio speaker and Audio Setup plan, Task 9
// (R-SPK-21, R-SPK-22, R-SPK-24), by J.J. Boyd (KG4VCF), with AI-assisted
// implementation via Anthropic Claude Code. The Speakers and Headphones
// cards and their engine wiring moved here from the Devices page, keys
// unchanged.
// 2026-10-09: native audio plan Task 16 (R-AUD-01, R-AUD-03, R-AUD-06):
// the cards follow the engine's device catalogue; Rescan devices rescans
// the older drivers and says which lists update by themselves (greyed on
// the Mac); the Sound system line follows the cards' choices.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-10-09: native audio plan Task 22 (R-AUD-27, R-AUD-30, D24): the
// Core speaker card between Headphones and Radio speaker, in a window
// connected to a Core only. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
// Claude Code.
// =================================================================

#include "AudioOutputsPage.h"
#include "CoreSpeakerCard.h"
#include "DeviceCard.h"
#include "SoundSystemLine.h"

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/HpsdrModel.h"
#include "core/audio/IAudioDeviceCatalog.h"
#include "gui/StyleConstants.h"
#include "gui/setup/AudioDriverList.h"
#include "gui/widgets/AppIcon.h"
#include "gui/widgets/MasterOutputWidget.h"
#include "models/RadioModel.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

namespace NereusSDR {

namespace {

constexpr int kButtonPx = 20;
constexpr int kSliderWidth = 160;
constexpr int kReadoutWidth = 26;
// How often the page looks for the engine's device catalogue until it has
// one (the cards look as often, R-AUD-15's readout tick).
constexpr int kCataloguePickupMs = 1000;

// Amplifier choice ids: RadioModel::speakerAmplifierMode values.
constexpr int kAmpNormal = 0;
constexpr int kAmpOffOnTx = 1;
constexpr int kAmpAlwaysOff = 2;

const char* const kPcOnIcon       = "pc-on";
const char* const kPcMutedIcon    = "pc-muted";
const char* const kRadioOnIcon    = "radio-on";
const char* const kRadioMutedIcon = "radio-muted";
const char* const kRadioNoneIcon  = "radio-none";

// Title colours: cyan for this computer (the PC control), amber for the
// radio speaker (the RADIO control), as in the header.
const char* const kCyanTitle  = "QGroupBox { color: #00b4d8; }";
const char* const kAmberGroup =
    "QGroupBox {"
    "  border: 1px solid #203040;"
    "  border-radius: 4px;"
    "  margin-top: 8px;"
    "  padding-top: 12px;"
    "  font-weight: bold;"
    "  color: #e0a030;"
    "}"
    "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }";

const char* const kRowLabel = "QLabel { color: #c8d8e8; font-size: 12px; }";
const char* const kNote     = "QLabel { color: #8aa8c0; font-size: 11px; }";
const char* const kReason   = "QLabel { color: #e0a030; font-size: 11px; }";
// Disabled reads as greyed (D10: disabled, never hidden).
const char* const kCheck =
    "QCheckBox { color: #c8d8e8; font-size: 12px; }"
    "QCheckBox:disabled { color: #506070; }";
const char* const kRadio =
    "QRadioButton { color: #c8d8e8; font-size: 12px; }"
    "QRadioButton:disabled { color: #506070; }"
    "QRadioButton::indicator:disabled:checked { background: #405060; border-radius: 6px; }";

QLabel* makeNote(const QString& text, const char* objectName, QWidget* parent)
{
    auto* note = new QLabel(text, parent);
    note->setObjectName(QLatin1String(objectName));
    note->setWordWrap(true);
    note->setStyleSheet(QLatin1String(kNote));
    return note;
}

QLabel* makeRowLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(QLatin1String(kRowLabel));
    label->setMinimumWidth(70);
    return label;
}

QPushButton* makeIconButton(const char* objectName, QWidget* parent)
{
    auto* button = new QPushButton(parent);
    button->setObjectName(QLatin1String(objectName));
    button->setFixedSize(kButtonPx, kButtonPx);
    button->setCheckable(true);
    button->setStyleSheet(QLatin1String(HeaderVolumeStyle::kIconButton));
    return button;
}

void applyIcon(QPushButton* button, const QString& name, bool greyedArtwork)
{
    if (button->property(AppIcon::kIconProperty).toString() == name) {
        return;
    }
    AppIcon::apply(button, name, HeaderVolumeStyle::kIconPx);
    if (greyedArtwork) {
        // radio-none is already the greyed radio: keep Qt from greying it
        // again on the disabled button (as RadioSpeakerWidget does).
        QIcon icon = button->icon();
        for (const qreal ratio : {1.0, 2.0, button->devicePixelRatioF()}) {
            icon.addPixmap(AppIcon::pixmap(name, HeaderVolumeStyle::kIconPx, ratio),
                           QIcon::Disabled);
        }
        button->setIcon(icon);
    }
}

void setTipAndDescription(QWidget* widget, const QString& tip, const QString& description)
{
    if (widget->toolTip() != tip) {
        widget->setToolTip(tip);
    }
    if (widget->accessibleDescription() != description) {
        widget->setAccessibleDescription(description);
    }
}

} // namespace

AudioOutputsPage::AudioOutputsPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Outputs"), model, parent)
    // This computer's devices, as the Devices page does (R-R3-23); see
    // RadioModel::localAudioDevices().
    , m_engine(model ? model->localAudioDevices() : nullptr)
{
    m_soundSystem = new SoundSystemLine(m_engine, this);
    addContent(m_soundSystem);

    buildThisComputer();
    buildHeadphones();
    // D24: the Core speaker sits between Headphones and Radio speaker. A
    // window that runs the radio itself has no separate Core, so the card
    // is absent there (R-AUD-27: This computer is that speaker).
    if (model && model->role() == RadioModel::Role::Remote) {
        m_coreSpeakerCard = new CoreSpeakerCard(model, this);
        addContent(m_coreSpeakerCard);
    }
    buildRadioSpeaker();
    buildRescan();

    if (m_engine) {
        wireEngine();
    }
    updateRescanState();
    syncPcFromEngine();
    wireModel();
    syncRadioSpeaker();
}

// ── This computer ──────────────────────────────────────────────────────────
void AudioOutputsPage::buildThisComputer()
{
    m_speakersCard = new DeviceCard(QStringLiteral("audio/Speakers"),
                                    DeviceCard::Role::Output, false, this);
    m_speakersCard->setObjectName(QStringLiteral("thisComputerGroup"));
    m_speakersCard->setTitle(tr("This computer"));
    m_speakersCard->setStyleSheet(m_speakersCard->styleSheet()
                                  + QLatin1String(kCyanTitle));

    auto* row = new QWidget(m_speakersCard);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(6);
    rowLayout->addWidget(makeRowLabel(tr("Volume:"), row));

    m_pcButton = makeIconButton("pcMuteButton", row);
    m_pcButton->setAccessibleName(tr("Mute this computer"));
    rowLayout->addWidget(m_pcButton);

    m_pcSlider = new QSlider(Qt::Horizontal, row);
    m_pcSlider->setObjectName(QStringLiteral("pcVolume"));
    m_pcSlider->setRange(0, 100);
    m_pcSlider->setFixedWidth(kSliderWidth);
    m_pcSlider->setStyleSheet(QLatin1String(HeaderVolumeStyle::kPcSlider));
    m_pcSlider->setAccessibleName(tr("This computer volume"));
    rowLayout->addWidget(m_pcSlider);

    m_pcReadout = new QLabel(row);
    m_pcReadout->setObjectName(QStringLiteral("pcVolumeReadout"));
    m_pcReadout->setFixedWidth(kReadoutWidth);
    m_pcReadout->setAlignment(Qt::AlignCenter);
    m_pcReadout->setStyleSheet(QLatin1String(HeaderVolumeStyle::kReadout));
    rowLayout->addWidget(m_pcReadout);

    m_pcMute = new QCheckBox(tr("Mute"), row);
    m_pcMute->setObjectName(QStringLiteral("pcMute"));
    m_pcMute->setStyleSheet(QLatin1String(kCheck));
    rowLayout->addWidget(m_pcMute);
    rowLayout->addStretch(1);
    m_speakersCard->addAboveDevice(row);

    m_speakersCard->addBelowDevice(makeNote(
        tr("Same control as PC in the header. Headphones below are not affected by it."),
        "thisComputerNote", m_speakersCard));

    addContent(m_speakersCard);

    // ── Widget -> engine, saved as the header saves it ───────────────────
    auto setVolume = [this](int value) {
        if (m_updatingFromEngine) {
            return;
        }
        const float v = static_cast<float>(value) / 100.0f;
        m_pcReadout->setText(QString::number(value));
        if (m_engine) {
            m_engine->setVolume(v);
        }
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("audio/Master/Volume"), QString::number(v, 'f', 3));
        s.save();
    };
    auto setMuted = [this](bool muted) {
        if (m_updatingFromEngine) {
            return;
        }
        if (m_engine) {
            m_engine->setMasterMuted(muted);
        }
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("audio/Master/Muted"),
                   muted ? QStringLiteral("True") : QStringLiteral("False"));
        s.save();
        syncPcFromEngine();
    };
    connect(m_pcSlider, &QSlider::valueChanged, this, setVolume);
    connect(m_pcMute, &QCheckBox::toggled, this, setMuted);
    connect(m_pcButton, &QPushButton::toggled, this, setMuted);
}

// ── Headphones ─────────────────────────────────────────────────────────────
void AudioOutputsPage::buildHeadphones()
{
    m_headphonesCard = new DeviceCard(QStringLiteral("audio/Headphones"),
                                      DeviceCard::Role::Output, true, this);
    m_headphonesCard->setObjectName(QStringLiteral("headphonesGroup"));
    m_headphonesCard->setTitle(tr("Headphones"));
    m_headphonesCard->setGreyedUntilEnabled(true);
    m_headphonesCard->addAboveDevice(makeNote(
        tr("The PC volume does not change the headphones."),
        "headphonesNote", m_headphonesCard));
    addContent(m_headphonesCard);
}

// ── Radio speaker ──────────────────────────────────────────────────────────
void AudioOutputsPage::buildRadioSpeaker()
{
    auto* group = new QGroupBox(tr("Radio speaker"), this);
    group->setObjectName(QStringLiteral("radioSpeakerGroup"));
    group->setStyleSheet(QLatin1String(kAmberGroup));
    m_radioGroup = group;
    auto* layout = new QVBoxLayout(group);
    layout->setContentsMargins(8, 12, 8, 8);
    layout->setSpacing(6);

    m_radioStatus = new QLabel(group);
    m_radioStatus->setObjectName(QStringLiteral("radioSpeakerStatusLine"));
    m_radioStatus->setWordWrap(true);
    m_radioStatus->setStyleSheet(QLatin1String(kRowLabel));
    layout->addWidget(m_radioStatus);

    // Volume row
    auto* row = new QHBoxLayout;
    row->setSpacing(6);
    row->addWidget(makeRowLabel(tr("Volume:"), group));
    m_radioButton = makeIconButton("radioSpeakerMuteButton", group);
    m_radioButton->setAccessibleName(tr("Mute radio speaker"));
    row->addWidget(m_radioButton);

    m_radioSlider = new QSlider(Qt::Horizontal, group);
    m_radioSlider->setObjectName(QStringLiteral("radioSpeakerVolume"));
    m_radioSlider->setProperty("nereusSetupId", "audio.outputs.radioSpeakerVolume");
    m_radioSlider->setRange(0, 100);
    m_radioSlider->setFixedWidth(kSliderWidth);
    m_radioSlider->setStyleSheet(QLatin1String(HeaderVolumeStyle::kRadioSlider));
    m_radioSlider->setAccessibleName(tr("Radio speaker volume"));
    row->addWidget(m_radioSlider);

    m_radioReadout = new QLabel(group);
    m_radioReadout->setObjectName(QStringLiteral("radioSpeakerReadout"));
    m_radioReadout->setFixedWidth(kReadoutWidth);
    m_radioReadout->setAlignment(Qt::AlignCenter);
    m_radioReadout->setStyleSheet(QLatin1String(HeaderVolumeStyle::kReadout));
    row->addWidget(m_radioReadout);

    m_radioMute = new QCheckBox(tr("Mute radio speaker"), group);
    m_radioMute->setObjectName(QStringLiteral("radioSpeakerMute"));
    m_radioMute->setProperty("nereusSetupId", "audio.outputs.radioSpeakerMuted");
    m_radioMute->setStyleSheet(QLatin1String(kCheck));
    row->addWidget(m_radioMute);
    row->addStretch(1);
    layout->addLayout(row);

    const bool remote = model() && model()->role() == RadioModel::Role::Remote;
    m_radioNote = makeNote(remote
        ? tr("This is the speaker at the Core. Changes here reach every window and "
             "the phone. Each slice's AF level and mute still apply.")
        : tr("Same control as RADIO in the header. Plays the receiving slices; each "
             "slice's AF level and mute still apply."),
        "radioSpeakerNote", group);
    layout->addWidget(m_radioNote);

    // Speaker amplifier (R-SPK-08 to R-SPK-10)
    m_ampChoice = new QWidget(group);
    m_ampChoice->setObjectName(QStringLiteral("speakerAmplifierChoice"));
    m_ampChoice->setProperty("nereusSetupId", "audio.outputs.speakerAmplifierMode");
    auto* ampRow = new QHBoxLayout(m_ampChoice);
    ampRow->setContentsMargins(0, 0, 0, 0);
    ampRow->setSpacing(10);
    ampRow->addWidget(makeRowLabel(tr("Speaker amplifier:"), m_ampChoice));
    m_ampButtons = new QButtonGroup(m_ampChoice);
    const struct {
        int id;
        const char* objectName;
        QString text;
    } choices[] = {
        {kAmpNormal, "ampNormal", tr("Normal")},
        {kAmpOffOnTx, "ampOffOnTx", tr("Off while transmitting")},
        {kAmpAlwaysOff, "ampAlwaysOff", tr("Always off")},
    };
    for (const auto& choice : choices) {
        auto* button = new QRadioButton(choice.text, m_ampChoice);
        button->setObjectName(QLatin1String(choice.objectName));
        button->setStyleSheet(QLatin1String(kRadio));
        m_ampButtons->addButton(button, choice.id);
        ampRow->addWidget(button);
    }
    ampRow->addStretch(1);
    layout->addWidget(m_ampChoice);

    // R-SPK-10: the explanation names the speaker jacks only until the
    // bench says whether amplifier off also silences headphones and line
    // out (V-HW-2).
    layout->addWidget(makeNote(
        tr("The radio's built-in amplifier for its speaker jacks. Muting the radio "
           "speaker also switches it off. Off while transmitting keeps it on for CW "
           "and Tune, so you still hear the sidetone. Switching it can make a pop."),
        "speakerAmplifierExplanation", group));

    m_ampReason = new QLabel(group);
    m_ampReason->setObjectName(QStringLiteral("speakerAmplifierReason"));
    m_ampReason->setWordWrap(true);
    m_ampReason->setStyleSheet(QLatin1String(kReason));
    layout->addWidget(m_ampReason);

    m_ampStatus = new QLabel(group);
    m_ampStatus->setObjectName(QStringLiteral("speakerAmplifierStatus"));
    m_ampStatus->setWordWrap(true);
    m_ampStatus->setStyleSheet(QLatin1String(kReason));
    layout->addWidget(m_ampStatus);

    addContent(group);

    // ── Widget -> model ──────────────────────────────────────────────────
    connect(m_radioSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_updatingFromModel) {
            return;
        }
        if (model()) {
            model()->setRadioSpeakerVolume(value);
        }
        // The model may clamp or refuse; show what it holds.
        syncRadioSpeaker();
    });
    auto setMuted = [this](bool muted) {
        if (m_updatingFromModel) {
            return;
        }
        if (model()) {
            model()->setRadioSpeakerMuted(muted);
        }
        syncRadioSpeaker();
    };
    connect(m_radioMute, &QCheckBox::toggled, this, setMuted);
    connect(m_radioButton, &QPushButton::toggled, this, setMuted);
    connect(m_ampButtons, &QButtonGroup::idToggled, this, [this](int id, bool checked) {
        if (m_updatingFromModel || !checked) {
            return;
        }
        if (model()) {
            model()->setSpeakerAmplifierMode(id);
        }
        syncRadioSpeaker();
    });
}

// ── Rescan devices ─────────────────────────────────────────────────────────
void AudioOutputsPage::buildRescan()
{
    auto* row = new QHBoxLayout;
    row->setSpacing(8);
    m_rescanButton = new QPushButton(tr("Rescan devices"), this);
    m_rescanButton->setObjectName(QStringLiteral("rescanDevices"));
    // R-AUD-06: greyed on the Mac, so it must draw greyed (the shared
    // disabled rules; the enabled look is unchanged).
    m_rescanButton->setStyleSheet(Style::darkPageDisabledRules());
    row->addWidget(m_rescanButton);
    m_rescanResult = new QLabel(this);
    m_rescanResult->setObjectName(QStringLiteral("rescanDevicesResult"));
    m_rescanResult->setStyleSheet(QLatin1String(kNote));
    row->addWidget(m_rescanResult, 1);
    addContent(row);
    connect(m_rescanButton, &QPushButton::clicked, this, &AudioOutputsPage::rescan);
}

void AudioOutputsPage::rescan()
{
    // R-AUD-06: only the older drivers need a rescan; the native engines'
    // lists update by themselves.  The engine closes and reopens only the
    // roles on older drivers, and the cards follow the new lists.
    if (m_engine) {
        m_engine->rescanOlderDrivers();
    }
    m_speakersCard->rescanDevices();
    m_headphonesCard->rescanDevices();
    updateRescanState();
}

// The note beside Rescan devices, shown from the start, and the button
// greyed with its reason where there is nothing to rescan (the Mac).
void AudioOutputsPage::updateRescanState()
{
    const IAudioDeviceCatalog* catalogue = m_engine ? m_engine->catalogue() : nullptr;
    QString note;
    bool nothingToDo = false;
    if (catalogue != nullptr) {
        note = rescanNote(*catalogue);
        nothingToDo = rescanHasNothingToDo(*catalogue);
    } else {
#if defined(Q_OS_MAC)
        note = tr("Core Audio lists update by themselves, so there is nothing to rescan.");
        nothingToDo = true;
#elif defined(Q_OS_WIN)
        note = tr("Only the older drivers need this. Windows audio and ASIO lists update "
                  "by themselves.");
#else
        note = tr("Only the older drivers need this.");
#endif
    }
    m_rescanResult->setText(note);
    m_rescanButton->setEnabled(!nothingToDo);
    m_rescanButton->setToolTip(nothingToDo ? note : QString());
    m_soundSystem->refresh();
}

// ── Engine wiring (moved from the Devices page) ────────────────────────────
void AudioOutputsPage::wireEngine()
{
    // R-AUD-01, R-AUD-03, R-AUD-08: the cards list the engine's devices and
    // show their roles' states.
    m_speakersCard->setAudioEngine(m_engine);
    m_headphonesCard->setAudioEngine(m_engine);

    // The Sound system line names the older drivers the cards use.
    for (DeviceCard* card : {m_speakersCard, m_headphonesCard}) {
        connect(card, &DeviceCard::configChanged, this,
                [this](const AudioDeviceConfig&) { m_soundSystem->refresh(); });
        connect(card, &DeviceCard::enabledChanged, this,
                [this](bool) { m_soundSystem->refresh(); });
    }

    // The engine builds its device catalogue on first use: the note and
    // the line take it up once it is there.
    m_cataloguePickup = new QTimer(this);
    m_cataloguePickup->setInterval(kCataloguePickupMs);
    connect(m_cataloguePickup, &QTimer::timeout, this, [this]() {
        if (m_engine->catalogue() != nullptr) {
            m_cataloguePickup->stop();
            updateRescanState();
        }
    });
    if (m_engine->catalogue() == nullptr) {
        m_cataloguePickup->start();
    }

    // Speakers card -> engine
    connect(m_speakersCard, &DeviceCard::configChanged,
            this, [this](const AudioDeviceConfig& cfg) {
                if (m_updatingFromEngine) { return; }
                m_engine->setSpeakersConfig(cfg);
            });
    // Engine -> Speakers pill (QSignalBlocker prevents echo).
    connect(m_engine, &AudioEngine::speakersConfigChanged,
            this, [this](const AudioDeviceConfig& cfg) {
                m_updatingFromEngine = true;
                QSignalBlocker blocker(m_speakersCard);
                m_speakersCard->updateNegotiatedPill(cfg);
                m_updatingFromEngine = false;
            });

    // Headphones card -> engine
    connect(m_headphonesCard, &DeviceCard::configChanged,
            this, [this](const AudioDeviceConfig& cfg) {
                if (m_updatingFromEngine) { return; }
                m_engine->setHeadphonesConfig(cfg);
            });
    // R-R3-45: Enabled opens the headphones output on the card's device,
    // or closes it. The card has already saved audio/Headphones/Enabled.
    connect(m_headphonesCard, &DeviceCard::enabledChanged,
            this, [this](bool on) {
                if (m_updatingFromEngine) { return; }
                if (on) {
                    m_engine->setHeadphonesEnabled(false);
                    m_engine->setHeadphonesConfig(m_headphonesCard->currentConfig());
                }
                m_engine->setHeadphonesEnabled(on);
            });
    connect(m_engine, &AudioEngine::headphonesConfigChanged,
            this, [this](const AudioDeviceConfig& cfg) {
                m_updatingFromEngine = true;
                QSignalBlocker blocker(m_headphonesCard);
                m_headphonesCard->updateNegotiatedPill(cfg);
                m_updatingFromEngine = false;
            });

    // PC volume and mute from any source (the header, the phone's echo).
    connect(m_engine, &AudioEngine::volumeChanged,
            this, [this](float) { syncPcFromEngine(); });
    connect(m_engine, &AudioEngine::masterMutedChanged,
            this, [this](bool) { syncPcFromEngine(); });
}

void AudioOutputsPage::syncPcFromEngine()
{
    const float v = m_engine ? m_engine->volume() : 0.0f;
    const bool muted = m_engine && m_engine->masterMuted();
    const int pct = static_cast<int>(std::lround(v * 100.0f));
    m_updatingFromEngine = true;
    {
        const QSignalBlocker sliderBlock(m_pcSlider);
        const QSignalBlocker muteBlock(m_pcMute);
        const QSignalBlocker buttonBlock(m_pcButton);
        m_pcSlider->setValue(pct);
        m_pcMute->setChecked(muted);
        m_pcButton->setChecked(muted);
    }
    m_updatingFromEngine = false;
    m_pcReadout->setText(QString::number(pct));
    applyIcon(m_pcButton, QLatin1String(muted ? kPcMutedIcon : kPcOnIcon), false);
}

// ── Model wiring ───────────────────────────────────────────────────────────
void AudioOutputsPage::wireModel()
{
    RadioModel* const m = model();
    if (!m) {
        return;
    }
    auto sync = [this]() { syncRadioSpeaker(); };
    connect(m, &RadioModel::radioSpeakerVolumeChanged, this, sync);
    connect(m, &RadioModel::radioSpeakerMutedChanged, this, sync);
    connect(m, &RadioModel::speakerAmplifierModeChanged, this, sync);
    connect(m, &RadioModel::radioSpeakerAvailabilityChanged, this, sync);
    connect(m, &RadioModel::speakerAmplifierAvailableChanged, this, sync);
    connect(m, &RadioModel::speakerAmplifierStatusChanged, this, sync);
    // The reasons and the board can change with the connection while the
    // reports stay the same.
    connect(m, &RadioModel::connectionStateChanged, this, sync);
    // The Core link alone can change the reason (an older Core signing in
    // with no radio).
    connect(m, &RadioModel::stationLinkStateChanged, this, sync);
    connect(m, &RadioModel::currentRadioChanged, this, sync);
}

QString AudioOutputsPage::speakerAmplifierToolTip()
{
    return tr("The radio's built-in amplifier for its speaker jacks. Muting the radio "
              "speaker also switches it off. Off while transmitting keeps it on for CW "
              "and Tune, so you still hear the sidetone. Switching it can make a pop. "
              "Greyed out when the radio has no switchable speaker amplifier.");
}

QString AudioOutputsPage::radioSpeakerStatusText() const
{
    return m_radioStatus->text();
}

void AudioOutputsPage::syncRadioSpeaker()
{
    RadioModel* const m = model();
    const int availability = m ? m->radioSpeakerAvailability()
                               : RadioModel::kRadioSpeakerNoRadio;
    const bool available = availability != RadioModel::kRadioSpeakerNoRadio;
    const bool remote = m && m->role() == RadioModel::Role::Remote;
    const int volume = m ? m->radioSpeakerVolume() : 0;
    const bool muted = m && m->radioSpeakerMuted();
    const bool ampAvailable = m && m->speakerAmplifierAvailable();
    const int ampMode = m ? m->speakerAmplifierMode() : kAmpNormal;

    // Status line: what the radio has.
    QString status;
    if (!available) {
        status = m ? m->radioSpeakerUnavailableReason() : tr("No radio connected");
    } else if (availability == RadioModel::kRadioSpeakerNeedsAddOn) {
        status = tr("Headphone output on the Hermes Lite 2. It needs the audio add-on "
                    "board; the radio cannot report whether it has one.");
    } else {
        const HPSDRModel hpsdr = m->hardwareProfile().model;
        // A board not known yet is "the radio". A remote window's profile
        // holds a default model until the Core's capabilities name its
        // radio, so there the board counts only once they have.
        const bool boardKnown = m->connectionState() == ConnectionState::Connected
            && hpsdr != HPSDRModel::FIRST && hpsdr != HPSDRModel::LAST
            && (!remote || m->currentRadioInfo().boardType != HPSDRHW::Unknown);
        const QString board = boardKnown ? QString::fromLatin1(displayName(hpsdr))
                                         : tr("radio");
        status = remote ? tr("Speaker output on the %1 at the Core.").arg(board)
                        : tr("Speaker output on the %1.").arg(board);
    }
    m_radioStatus->setText(status);

    m_updatingFromModel = true;
    {
        const QSignalBlocker sliderBlock(m_radioSlider);
        const QSignalBlocker muteBlock(m_radioMute);
        const QSignalBlocker buttonBlock(m_radioButton);
        const QSignalBlocker ampBlock(m_ampButtons);
        m_radioSlider->setValue(volume);
        m_radioMute->setChecked(muted);
        m_radioButton->setChecked(muted);
        if (QAbstractButton* button = m_ampButtons->button(ampMode)) {
            button->setChecked(true);
        }
    }
    m_updatingFromModel = false;

    // D10: disabled, never hidden. The Core's settings unavailable in a
    // remote window (R-R3-21) disables these as well, with that reason.
    const bool speakerLive = available && m_stationAvailable;
    const bool ampLive = ampAvailable && m_stationAvailable;
    m_radioSlider->setEnabled(speakerLive);
    m_radioMute->setEnabled(speakerLive);
    m_radioButton->setEnabled(speakerLive);
    m_radioReadout->setEnabled(speakerLive);
    m_radioReadout->setText(available ? QString::number(volume) : QStringLiteral("--"));
    m_ampChoice->setEnabled(ampLive);
    applyIcon(m_radioButton,
              QLatin1String(!available ? kRadioNoneIcon
                            : muted    ? kRadioMutedIcon
                                       : kRadioOnIcon),
              !available);

    const QString speakerTip = m ? m->radioSpeakerToolTip() : tr("No radio connected");
    const QString ampReason = ampAvailable
        ? QString()
        : (m ? m->speakerAmplifierUnavailableReason() : tr("No radio connected"));
    if (!m_stationAvailable) {
        for (QWidget* w : {static_cast<QWidget*>(m_radioSlider),
                           static_cast<QWidget*>(m_radioMute),
                           static_cast<QWidget*>(m_radioButton),
                           m_ampChoice}) {
            setTipAndDescription(w, m_stationReason, m_stationReason);
        }
    } else {
        setTipAndDescription(m_radioSlider, speakerTip, tr("Radio speaker volume, 0 to 100"));
        setTipAndDescription(m_radioMute, speakerTip, QString());
        setTipAndDescription(m_radioButton, speakerTip, QString());
        // R-SPK-10 / D9: the described tooltip (audio.json) while it can
        // be switched; the reason while it is greyed.
        const QString ampTip = ampReason.isEmpty() ? speakerAmplifierToolTip() : ampReason;
        setTipAndDescription(m_ampChoice, ampTip, ampTip);
    }

    m_ampReason->setText(ampReason);
    m_ampReason->setVisible(!ampReason.isEmpty());
    const QString ampStatus = m ? m->speakerAmplifierStatus() : QString();
    m_ampStatus->setText(ampStatus);
    m_ampStatus->setVisible(!ampStatus.isEmpty());
}

// R-R3-21: only the Radio speaker controls write the Core's settings; this
// computer's volume, devices and the rescan stay live.
void AudioOutputsPage::setStationSettingsAvailable(bool available, const QString& reason)
{
    m_stationAvailable = available;
    m_stationReason = reason.isEmpty() ? tr("Connect to the Core to change these.") : reason;
    if (m_coreSpeakerCard) {
        m_coreSpeakerCard->setStationSettingsAvailable(available, reason);
    }
    syncRadioSpeaker();
}

} // namespace NereusSDR
