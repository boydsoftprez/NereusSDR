// =================================================================
// src/gui/setup/CoreSpeakerCard.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Setup > Audio > Outputs' "Core
// speaker" card; see CoreSpeakerCard.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 22 (R-AUD-27, R-AUD-30, D24).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio fix wave (R-AUD-27, R-AUD-11): the trouble
//               note on the Core's default names no card; the volume
//               slider takes HeaderVolumeStyle::kPcSlider's own disabled
//               look. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
// =================================================================

#include "CoreSpeakerCard.h"

#include "core/audio/AudioDelayParts.h"
#include "core/audio/CoreSpeakerJson.h"
#include "core/session/RemoteDevicesState.h"
#include "gui/setup/AudioDriverList.h"
#include "gui/widgets/MasterOutputWidget.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include <array>
#include <optional>

namespace NereusSDR {

namespace {

constexpr int kSliderWidth = 160;
constexpr int kReadoutWidth = 26;
constexpr int kMaxVolume = 100;

// What the Core takes (Task 21: RadioModel's kCoreSpeaker* lists).
constexpr std::array<int, 7> kSampleRates{44100, 48000, 88200, 96000, 176400, 192000, 384000};
constexpr std::array<int, 6> kBufferFrames{64, 128, 256, 512, 1024, 2048};
// R-AUD-15: the Delay list ("Automatic" is DelayMs 0).
constexpr std::array<int, 7> kDelayChoicesMs{0, 2, 3, 5, 10, 20, 40};

// Device list items: Qt::UserRole is the card's id, kNameRole its name.
constexpr int kNameRole = Qt::UserRole + 1;

const QString kNoneId = QStringLiteral("(none)");

// The plain card title, as Headphones (core-speaker-mockup.html draws the
// Core speaker's legend in the default colour), greyed with the card.
const char* const kGroupStyle =
    "QGroupBox {"
    "  border: 1px solid #203040;"
    "  border-radius: 4px;"
    "  margin-top: 8px;"
    "  padding-top: 12px;"
    "  font-weight: bold;"
    "  color: #8aa8c0;"
    "}"
    "QGroupBox:disabled { color: #506070; }"
    "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }";

const char* const kRowLabel =
    "QLabel { color: #c8d8e8; font-size: 12px; }"
    "QLabel:disabled { color: #506070; }";
const char* const kNote = "QLabel { color: #8aa8c0; font-size: 11px; }";
const char* const kStateNote = "QLabel { color: #e0a030; font-size: 11px; }";
const char* const kReason = "QLabel { color: #8aa8c0; font-size: 11px; font-style: italic; }";
const char* const kDim =
    "QLabel { color: #607080; font-size: 11px; }"
    "QLabel:disabled { color: #405060; }";
const char* const kCheck =
    "QCheckBox { color: #c8d8e8; font-size: 12px; }"
    "QCheckBox:disabled { color: #506070; }";
const char* const kCombo =
    "QComboBox {"
    "  background: #152535;"
    "  border: 1px solid #203040;"
    "  border-radius: 3px;"
    "  color: #c8d8e8;"
    "  padding: 2px 6px;"
    "}"
    "QComboBox:disabled { color: #506070; }"
    "QComboBox::drop-down { border: none; }"
    "QComboBox QAbstractItemView { background: #152535; color: #c8d8e8; "
    "  selection-background-color: #00b4d8; }";
const char* const kToggle =
    "QToolButton { color: #8aa8c0; font-size: 11px; border: none; padding: 2px 0; }"
    "QToolButton:disabled { color: #405060; }";
const char* const kPillLive =
    "QLabel { background: #152530; border: 1px solid #203040; border-radius: 8px;"
    "  color: #80c8a0; font-size: 10px; padding: 2px 8px; }"
    "QLabel:disabled { color: #506070; }";
const char* const kPillIdle =
    "QLabel { background: #203040; border: 1px solid #304050; border-radius: 8px;"
    "  color: #c8d8e8; font-size: 10px; padding: 2px 8px; }"
    "QLabel:disabled { color: #506070; }";

QLabel* makeLabel(const QString& text, const char* style, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(QLatin1String(style));
    return label;
}

QLabel* makeNote(const QString& text, const char* objectName, const char* style, QWidget* parent)
{
    QLabel* note = makeLabel(text, style, parent);
    note->setObjectName(QLatin1String(objectName));
    note->setWordWrap(true);
    return note;
}

QComboBox* makeCombo(const char* objectName, QWidget* parent)
{
    auto* combo = new QComboBox(parent);
    combo->setObjectName(QLatin1String(objectName));
    combo->setStyleSheet(QLatin1String(kCombo));
    return combo;
}

void setTip(QWidget* widget, const QString& tip)
{
    if (widget->toolTip() != tip) {
        widget->setToolTip(tip);
    }
}

void selectData(QComboBox* combo, int value)
{
    const int index = combo->findData(value);
    if (index >= 0) {
        combo->setCurrentIndex(index);
    }
}

QString bufferMs(int frames, int sampleRate)
{
    if (frames <= 0 || sampleRate <= 0) {
        return QStringLiteral("-- ms");
    }
    const double ms = static_cast<double>(frames) / static_cast<double>(sampleRate) * 1000.0;
    return QStringLiteral("%1 ms").arg(ms, 0, 'f', 1);
}

// R-AUD-27 and settled call 35: the chosen card's trouble at the Core.
QString troubleNote(const CoreSpeakerState& state)
{
    const QString trouble = state.kind == CoreSpeakerStateKind::InUse
        ? CoreSpeakerCard::tr("is in use by another program at the Core")
        : CoreSpeakerCard::tr("is not connected at the Core");
    if (state.chosenName.isEmpty()) {
        // On "(the Core's default)" the Core names no chosen card and does
        // not name its default, so the note names neither.
        const QString defaultTrouble = state.kind == CoreSpeakerStateKind::InUse
            ? CoreSpeakerCard::tr("is in use by another program")
            : CoreSpeakerCard::tr("is not connected");
        if (state.playingName.isEmpty()) {
            return CoreSpeakerCard::tr("The Core's default sound card %1, so the Core is silent "
                                       "until it comes back.")
                .arg(defaultTrouble);
        }
        return CoreSpeakerCard::tr("The Core's default sound card %1. Playing on %2 until it "
                                   "comes back.")
            .arg(defaultTrouble, state.playingName);
    }
    if (state.playingName.isEmpty()) {
        return CoreSpeakerCard::tr("%1 %2, and the Core has no other sound card, so it is silent "
                                   "until it comes back.")
            .arg(state.chosenName, trouble);
    }
    return CoreSpeakerCard::tr("%1 %2. Playing on the Core's default, %3, until it comes back.")
        .arg(state.chosenName, trouble, state.playingName);
}

} // namespace

CoreSpeakerCard::CoreSpeakerCard(RadioModel* model, QWidget* parent)
    : QGroupBox(tr("Core speaker"), parent)
    , m_model(model)
    , m_stationReason(tr("Connect to the Core to change these."))
{
    setObjectName(QStringLiteral("coreSpeakerGroup"));
    setStyleSheet(QLatin1String(kGroupStyle));
    buildLayout();
    wireModel();
    sync();
}

// ── Layout ─────────────────────────────────────────────────────────────────
void CoreSpeakerCard::buildLayout()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 12, 8, 8);
    layout->setSpacing(6);

    m_where = makeNote(QString(), "coreSpeakerWhere", kRowLabel, this);
    m_where->setTextFormat(Qt::RichText);
    layout->addWidget(m_where);

    // Volume row
    auto* row = new QHBoxLayout;
    row->setSpacing(6);
    QLabel* volumeLabel = makeLabel(tr("Volume:"), kRowLabel, this);
    volumeLabel->setMinimumWidth(70);
    row->addWidget(volumeLabel);
    m_volume = new QSlider(Qt::Horizontal, this);
    m_volume->setObjectName(QStringLiteral("coreSpeakerVolume"));
    m_volume->setRange(0, kMaxVolume);
    m_volume->setFixedWidth(kSliderWidth);
    // The header's PC slider palette, greyed like the radio slider while
    // disabled.
    m_volume->setStyleSheet(QLatin1String(HeaderVolumeStyle::kPcSlider));
    m_volume->setAccessibleName(tr("Core speaker volume"));
    row->addWidget(m_volume);
    m_readout = new QLabel(this);
    m_readout->setObjectName(QStringLiteral("coreSpeakerVolumeReadout"));
    m_readout->setFixedWidth(kReadoutWidth);
    m_readout->setAlignment(Qt::AlignCenter);
    m_readout->setStyleSheet(QLatin1String(HeaderVolumeStyle::kReadout));
    row->addWidget(m_readout);
    m_mute = new QCheckBox(tr("Mute Core speaker"), this);
    m_mute->setObjectName(QStringLiteral("coreSpeakerMute"));
    m_mute->setStyleSheet(QLatin1String(kCheck));
    row->addWidget(m_mute);
    row->addStretch(1);
    layout->addLayout(row);

    // Device row
    auto* deviceRow = new QHBoxLayout;
    deviceRow->setSpacing(6);
    QLabel* deviceLabel = makeLabel(tr("Device:"), kRowLabel, this);
    deviceLabel->setMinimumWidth(70);
    deviceRow->addWidget(deviceLabel);
    m_device = makeCombo("coreSpeakerDevice", this);
    m_device->setMinimumWidth(300);
    m_device->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_device->setAccessibleName(tr("Core speaker device"));
    deviceRow->addWidget(m_device);
    deviceRow->addStretch(1);
    layout->addLayout(deviceRow);

    m_stateNote = makeNote(QString(), "coreSpeakerStateNote", kStateNote, this);
    m_stateNote->setVisible(false);
    layout->addWidget(m_stateNote);
    m_desktopNote = makeNote(QString(), "coreSpeakerDesktopNote", kNote, this);
    m_desktopNote->setVisible(false);
    layout->addWidget(m_desktopNote);

    m_note = makeNote(tr("This is the speaker at the Core, for listening where the Core sits. "
                         "Changes here reach every window and the phone. Each slice's AF level "
                         "and mute still apply."),
                      "coreSpeakerNote", kNote, this);
    layout->addWidget(m_note);

    m_reason = makeNote(QString(), "coreSpeakerReason", kReason, this);
    m_reason->setVisible(false);
    layout->addWidget(m_reason);

    // Device details, folded
    m_detailsToggle = new QToolButton(this);
    m_detailsToggle->setObjectName(QStringLiteral("coreSpeakerDetailsToggle"));
    m_detailsToggle->setText(tr("Device details"));
    m_detailsToggle->setCheckable(true);
    m_detailsToggle->setChecked(false);
    m_detailsToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_detailsToggle->setArrowType(Qt::RightArrow);
    m_detailsToggle->setAutoRaise(true);
    m_detailsToggle->setStyleSheet(QLatin1String(kToggle));
    layout->addWidget(m_detailsToggle);

    m_details = new QWidget(this);
    m_details->setObjectName(QStringLiteral("coreSpeakerDetails"));
    auto* form = new QFormLayout(m_details);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(4);
    form->setContentsMargins(12, 0, 0, 0);

    // R-AUD-01's Core line: one driver, greyed with why.
    const QString driverWhy =
        tr("The Core runs without a desktop, so it plays straight to the sound card.");
    m_driver = makeCombo("coreSpeakerDriver", m_details);
    m_driver->addItem(tr("ALSA, direct"));
    m_driver->setEnabled(false);
    m_driver->setToolTip(driverWhy);
    form->addRow(makeLabel(tr("Driver:"), kRowLabel, m_details), m_driver);
    // The note takes the field's full width; at its size hint a wrapped
    // label is squeezed and clipped by the form.
    QLabel* driverNote = makeNote(driverWhy, "coreSpeakerDriverNote", kNote, m_details);
    driverNote->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    form->addRow(makeLabel(QString(), kRowLabel, m_details), driverNote);

    m_sampleRate = makeCombo("coreSpeakerSampleRate", m_details);
    for (const int rate : kSampleRates) {
        m_sampleRate->addItem(tr("%1 Hz").arg(rate), rate);
    }
    form->addRow(makeLabel(tr("Sample rate:"), kRowLabel, m_details), m_sampleRate);

    m_channels = makeCombo("coreSpeakerChannels", m_details);
    m_channels->addItem(tr("2 (Stereo)"), 2);
    form->addRow(makeLabel(tr("Channels:"), kRowLabel, m_details), m_channels);

    {
        auto* bufferRow = new QHBoxLayout;
        bufferRow->setSpacing(6);
        m_bufferSize = makeCombo("coreSpeakerBufferSize", m_details);
        for (const int frames : kBufferFrames) {
            m_bufferSize->addItem(tr("%1 samples").arg(frames), frames);
        }
        m_bufferMs = makeLabel(QString(), kDim, m_details);
        m_bufferMs->setObjectName(QStringLiteral("coreSpeakerBufferMs"));
        m_bufferMs->setMinimumWidth(50);
        bufferRow->addWidget(m_bufferSize);
        bufferRow->addWidget(m_bufferMs);
        bufferRow->addStretch(1);
        form->addRow(makeLabel(tr("Buffer size:"), kRowLabel, m_details), bufferRow);
    }
    {
        auto* delayRow = new QHBoxLayout;
        delayRow->setSpacing(6);
        m_delay = makeCombo("coreSpeakerDelay", m_details);
        for (const int ms : kDelayChoicesMs) {
            m_delay->addItem(ms == 0 ? tr("Automatic") : tr("%1 ms").arg(ms), ms);
        }
        m_delayNow = makeLabel(QStringLiteral("Now -- ms"), kDim, m_details);
        m_delayNow->setObjectName(QStringLiteral("coreSpeakerDelayNow"));
        delayRow->addWidget(m_delay);
        delayRow->addWidget(m_delayNow);
        delayRow->addStretch(1);
        form->addRow(makeLabel(tr("Delay:"), kRowLabel, m_details), delayRow);
    }
    {
        auto* pillRow = new QHBoxLayout;
        pillRow->setSpacing(4);
        m_negotiated = makeLabel(tr("(not applied)"), kPillIdle, m_details);
        m_negotiated->setObjectName(QStringLiteral("coreSpeakerNegotiated"));
        pillRow->addWidget(m_negotiated);
        pillRow->addStretch(1);
        form->addRow(makeLabel(tr("Negotiated:"), kDim, m_details), pillRow);
    }
    layout->addWidget(m_details);
    m_details->setVisible(false);

    connect(m_detailsToggle, &QToolButton::toggled, this, [this](bool on) {
        m_detailsToggle->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
        m_details->setVisible(on);
    });

    // ── Widget -> model ──────────────────────────────────────────────────
    connect(m_volume, &QSlider::valueChanged, this, [this](int value) {
        if (m_updatingFromModel) {
            return;
        }
        m_readout->setText(QString::number(value));
        if (m_model) {
            m_model->setCoreSpeakerVolume(value);
        }
        // The model may refuse; show what it holds.
        sync();
    });
    connect(m_mute, &QCheckBox::toggled, this, [this](bool muted) {
        if (m_updatingFromModel) {
            return;
        }
        if (m_model) {
            m_model->setCoreSpeakerMuted(muted);
        }
        sync();
    });
    connect(m_device, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (m_updatingFromModel || index < 0) {
            return;
        }
        if (m_model) {
            m_model->setCoreSpeakerDevice(coreSpeakerDeviceToJson(
                m_device->itemData(index).toString(), m_device->itemData(index, kNameRole).toString()));
        }
        sync();
    });
    for (QComboBox* combo : {m_sampleRate, m_bufferSize, m_delay}) {
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
            if (m_updatingFromModel) {
                return;
            }
            writeDetails();
        });
    }
}

// ── Model wiring ───────────────────────────────────────────────────────────
void CoreSpeakerCard::wireModel()
{
    RadioModel* const m = m_model.data();
    if (m == nullptr) {
        return;
    }
    auto refresh = [this]() { sync(); };
    connect(m, &RadioModel::coreSpeakerVolumeChanged, this, refresh);
    connect(m, &RadioModel::coreSpeakerMutedChanged, this, refresh);
    connect(m, &RadioModel::coreSpeakerDeviceChanged, this, refresh);
    connect(m, &RadioModel::coreSpeakerDevicesChanged, this, refresh);
    connect(m, &RadioModel::coreSpeakerStateChanged, this, refresh);
    connect(m, &RadioModel::coreSpeakerDetailsChanged, this, refresh);
    // Task 21: no availability signal of its own; the link's state says when
    // the Core starts or stops offering the speaker.
    auto linkChanged = [this]() {
        followStationDevices();
        sync();
    };
    connect(m, &RadioModel::stationLinkStateChanged, this, linkChanged);
    connect(m, &RadioModel::connectionStateChanged, this, linkChanged);
    followStationDevices();
}

// The Core's name comes from the devices object the link fills.
void CoreSpeakerCard::followStationDevices()
{
    RemoteDevicesState* const devices = m_model ? m_model->stationDevices() : nullptr;
    if (devices == m_devices.data()) {
        return;
    }
    disconnect(m_devicesConnection);
    m_devices = devices;
    if (devices != nullptr) {
        m_devicesConnection = connect(devices, &RemoteDevicesState::coreInfoChanged, this,
                                      [this]() { syncWhere(); });
    }
}

void CoreSpeakerCard::syncWhere()
{
    const QString name = m_devices ? m_devices->coreInfo().stationLabel : QString();
    m_where->setText(name.isEmpty()
        ? tr("A speaker or sound card plugged into the Core.")
        : tr("A speaker or sound card plugged into <b>%1</b>.").arg(name.toHtmlEscaped()));
}

QString CoreSpeakerCard::reason() const
{
    RadioModel* const m = m_model.data();
    if (m == nullptr) {
        return m_stationReason;
    }
    if (!m->coreSpeakerAvailable()) {
        return m->coreSpeakerUnavailableReason();
    }
    if (!m_stationAvailable) {
        return m_stationReason;
    }
    return QString();
}

void CoreSpeakerCard::setStationSettingsAvailable(bool available, const QString& reason)
{
    m_stationAvailable = available;
    m_stationReason = reason.isEmpty() ? tr("Connect to the Core to change these.") : reason;
    sync();
}

bool CoreSpeakerCard::detailsExpanded() const
{
    return m_detailsToggle->isChecked();
}

void CoreSpeakerCard::setDetailsExpanded(bool expanded)
{
    m_detailsToggle->setChecked(expanded);
}

// ── Model -> widgets ───────────────────────────────────────────────────────
void CoreSpeakerCard::sync()
{
    RadioModel* const m = m_model.data();
    syncWhere();

    const int volume = m ? m->coreSpeakerVolume() : 0;
    const bool muted = m && m->coreSpeakerMuted();
    m_updatingFromModel = true;
    {
        const QSignalBlocker volumeBlock(m_volume);
        const QSignalBlocker muteBlock(m_mute);
        m_volume->setValue(volume);
        m_mute->setChecked(muted);
    }
    m_updatingFromModel = false;
    m_readout->setText(QString::number(volume));

    fillDevices();
    syncDetails();

    // D10: disabled, never hidden, with the reason beside and on each control.
    const QString why = reason();
    const bool live = why.isEmpty();
    for (QWidget* w : {static_cast<QWidget*>(m_volume), static_cast<QWidget*>(m_readout),
                       static_cast<QWidget*>(m_mute), static_cast<QWidget*>(m_device),
                       static_cast<QWidget*>(m_sampleRate), static_cast<QWidget*>(m_channels),
                       static_cast<QWidget*>(m_bufferSize), static_cast<QWidget*>(m_bufferMs),
                       static_cast<QWidget*>(m_delay), static_cast<QWidget*>(m_delayNow),
                       static_cast<QWidget*>(m_negotiated)}) {
        w->setEnabled(live);
        setTip(w, why);
    }
    m_reason->setText(why);
    m_reason->setVisible(!live);

    // The Core's state, while it can be reached.
    const std::optional<CoreSpeakerState> state =
        m ? coreSpeakerStateFromJson(m->coreSpeakerState()) : std::nullopt;
    QString stateNote;
    QString desktopNote;
    if (live && state) {
        bool anyCard = false;
        if (const auto listed = coreSpeakerDevicesFromJson(m->coreSpeakerDevices())) {
            for (const CoreSpeakerCardInfo& card : *listed) {
                anyCard = anyCard || card.state == AudioDeviceState::Present;
            }
        }
        const std::optional<QPair<QString, QString>> chosen =
            coreSpeakerDeviceFromJson(m->coreSpeakerDevice());
        const bool noneChosen = state->kind == CoreSpeakerStateKind::WaitingForPick
            || (chosen && chosen->first == kNoneId);
        switch (state->kind) {
        case CoreSpeakerStateKind::NotConnected:
        case CoreSpeakerStateKind::InUse:
            stateNote = troubleNote(*state);
            break;
        case CoreSpeakerStateKind::NoCard:
            if (!anyCard) {
                stateNote = tr("No sound card is plugged into the Core. Plug in a USB sound card "
                               "or speaker and it shows up here by itself.");
            }
            break;
        case CoreSpeakerStateKind::Playing:
        case CoreSpeakerStateKind::WaitingForPick:
            break;
        }
        // R-AUD-30: a Core box that starts into a desktop.
        if (state->desktop) {
            if (noneChosen) {
                desktopNote = tr("The Core's computer runs a desktop, which uses its sound cards. "
                                 "Pick one here to play the Core speaker on it.");
            } else {
                const QString card = !state->playingName.isEmpty() ? state->playingName
                    : !state->chosenName.isEmpty()                 ? state->chosenName
                    : chosen                                       ? chosen->second
                                                                   : QString();
                if (!card.isEmpty()) {
                    desktopNote = tr("The desktop can't play through %1 while the Core has it.")
                                      .arg(card);
                }
            }
        }
    }
    m_stateNote->setText(stateNote);
    m_stateNote->setVisible(!stateNote.isEmpty());
    m_desktopNote->setText(desktopNote);
    m_desktopNote->setVisible(!desktopNote.isEmpty());
}

// The Device list: "(none)" on a desktop box (R-AUD-30, settled call 12),
// "(the Core's default)", then the Core's cards as it lists them.  Rebuilt
// on each change, so a card plugged in at the Core appears in the open list.
void CoreSpeakerCard::fillDevices()
{
    RadioModel* const m = m_model.data();
    const std::optional<CoreSpeakerState> state =
        m ? coreSpeakerStateFromJson(m->coreSpeakerState()) : std::nullopt;
    const std::optional<QPair<QString, QString>> chosen =
        m ? coreSpeakerDeviceFromJson(m->coreSpeakerDevice()) : std::nullopt;
    const std::optional<QList<CoreSpeakerCardInfo>> cards =
        m ? coreSpeakerDevicesFromJson(m->coreSpeakerDevices()) : std::nullopt;
    const bool desktop = state && state->desktop;

    struct Entry {
        QString id;
        QString name;
        QString label;
    };
    QList<Entry> entries;
    if (desktop || (chosen && chosen->first == kNoneId)) {
        entries.append({kNoneId, QString(), tr("(none)")});
    }
    entries.append({QString(), QString(), tr("(the Core's default)")});
    if (cards) {
        for (const CoreSpeakerCardInfo& card : *cards) {
            QString label = card.name;
            if (card.state == AudioDeviceState::NotConnected) {
                label = tr("%1 (not connected)").arg(card.name);
            } else if (card.state == AudioDeviceState::InUse) {
                label = tr("%1 (in use by another program)").arg(card.name);
            }
            entries.append({card.id, card.name, label});
        }
    }
    // The Core's pick before its list has come: keep it in view.
    QString selectedId;
    if (chosen) {
        selectedId = chosen->first;
    } else if (desktop) {
        selectedId = kNoneId;
    }
    bool listed = false;
    for (const Entry& e : entries) {
        listed = listed || e.id == selectedId;
    }
    if (!listed && chosen) {
        entries.append({chosen->first, chosen->second,
                        chosen->second.isEmpty() ? chosen->first : chosen->second});
    }

    bool same = m_device->count() == entries.size();
    for (int i = 0; same && i < entries.size(); ++i) {
        same = m_device->itemData(i).toString() == entries.at(i).id
            && m_device->itemData(i, kNameRole).toString() == entries.at(i).name
            && m_device->itemText(i) == entries.at(i).label;
    }
    m_updatingFromModel = true;
    {
        const QSignalBlocker block(m_device);
        if (!same) {
            m_device->clear();
            for (const Entry& e : entries) {
                m_device->addItem(e.label, e.id);
                m_device->setItemData(m_device->count() - 1, e.name, kNameRole);
            }
        }
        for (int i = 0; i < m_device->count(); ++i) {
            if (m_device->itemData(i).toString() == selectedId) {
                m_device->setCurrentIndex(i);
                break;
            }
        }
    }
    m_updatingFromModel = false;
}

void CoreSpeakerCard::syncDetails()
{
    RadioModel* const m = m_model.data();
    const std::optional<CoreSpeakerDetails> details =
        m ? coreSpeakerDetailsFromJson(m->coreSpeakerDetails()) : std::nullopt;
    const std::optional<CoreSpeakerState> state =
        m ? coreSpeakerStateFromJson(m->coreSpeakerState()) : std::nullopt;
    m_updatingFromModel = true;
    {
        const QSignalBlocker rateBlock(m_sampleRate);
        const QSignalBlocker bufferBlock(m_bufferSize);
        const QSignalBlocker delayBlock(m_delay);
        if (details) {
            selectData(m_sampleRate, details->sampleRate);
            selectData(m_bufferSize, details->bufferFrames);
            selectData(m_delay, details->delayMs);
        }
    }
    m_updatingFromModel = false;

    const int frames = m_bufferSize->currentData().toInt();
    const int rate = m_sampleRate->currentData().toInt();
    m_bufferMs->setText(bufferMs(frames, rate));

    // Task 16's "Now" line, over the Core's measured total.
    AudioDelayParts parts;
    parts.matcherFillMs = details ? details->delayNowMs : -1.0;
    const QString playing = state ? state->playingName : QString();
    m_delayNow->setText(audioDelayLine(AudioRole::Speakers, parts, playing));

    const QString negotiated = details ? details->negotiated : QString();
    m_negotiated->setText(negotiated.isEmpty() ? tr("(not applied)") : negotiated);
    m_negotiated->setStyleSheet(QLatin1String(negotiated.isEmpty() ? kPillIdle : kPillLive));
}

// The window's three details fields; the Core keeps the rest as it sent them.
void CoreSpeakerCard::writeDetails()
{
    if (!m_model) {
        return;
    }
    CoreSpeakerDetails details =
        coreSpeakerDetailsFromJson(m_model->coreSpeakerDetails()).value_or(CoreSpeakerDetails{});
    details.sampleRate = m_sampleRate->currentData().toInt();
    details.bufferFrames = m_bufferSize->currentData().toInt();
    details.delayMs = m_delay->currentData().toInt();
    m_model->setCoreSpeakerDetails(coreSpeakerDetailsToJson(details));
    sync();
}

} // namespace NereusSDR
