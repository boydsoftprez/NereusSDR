// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/setup/RecordingsSetupPage.cpp  (NereusSDR)
// =================================================================
//
// See RecordingsSetupPage.h. Words and layout follow the approved mockup
// (docs/architecture/2026-11-15-recording-voice-keyer-dsr-design/
// recording-settings/), with the spec's sentences where they differ.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 8.
// =================================================================

#include "gui/setup/RecordingsSetupPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#include "core/recordings/ContactRecorder.h"
#include "core/recordings/RecordingNames.h"
#include "core/recordings/RecordingSettings.h"
#include "gui/recordings/RecordingsWindow.h"

namespace NereusSDR {

namespace {

constexpr const char* kNoteStyle = "color:#a6a6ab;";
constexpr const char* kNameStyle =
    "QLabel { color:#dcdce1; background:#0f0f12; border:1px solid #38383e; border-radius:4px;"
    " padding:4px 7px; font-family:Menlo, 'SF Mono', monospace; }";
constexpr const char* kOkBoxStyle =
    "QLabel { color:#dcdce1; background:#0f0f12; border:1px solid #38383e; border-radius:4px;"
    " padding:6px 8px; }";
constexpr const char* kBarStyle =
    "QProgressBar { background:#040406; border:1px solid #2c2c31; border-radius:3px; }"
    " QProgressBar::chunk { background:#2f5f92; }";

// A WAV sample is 16-bit PCM (AudioFileWriter.cpp kWavBitsPerSample).
constexpr int kWavBytesPerSample = 2;
constexpr int kSecondsPerHour = 3600;
constexpr double kBytesPerMegabyte = 1000.0 * 1000.0;
constexpr int kBarSteps = 1000;
constexpr int kManyHours = 1000;
constexpr int kQuietSpinWidth = 76;   // "60 min" and its arrows, as the mockup's small box

// The name example's moment and station, as the spec's example name.
const QDateTime kExampleStartUtc(QDate(2026, 11, 15), QTime(14, 32), QTimeZone::utc());
constexpr qint64 kExampleFrequencyHz = 7236400;

// The five switches: the document's field and the word beside it.
struct OnAirSwitch {
    const char* field;
    const char* text;
};
constexpr OnAirSwitch kOnAirSwitches[] = {
    {"onAirUseTxEq", "EQ"},
    {"onAirUseCompressor", "Compressor"},
    {"onAirUseLeveler", "Leveler"},
    {"onAirUseCfc", "CFC"},
    {"onAirUsePhaseRotator", "Phase rotator"},
};

QLabel* noteLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    label->setStyleSheet(QString::fromLatin1(kNoteStyle));
    return label;
}

QLabel* textLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setTextFormat(Qt::PlainText);
    return label;
}

QString samePath(const QString& path)
{
    return QDir::fromNativeSeparators(path);
}

}  // namespace

RecordingsSetupPage::RecordingsSetupPage(RecordingsAccess* access, QWidget* parent)
    : SetupPage(QStringLiteral("Recordings"), parent)
{
    setObjectName(QStringLiteral("RecordingsSetupPage"));
    m_availability = noteLabel(QString(), this);
    m_availability->setObjectName(QStringLiteral("recordingsSetupAvailability"));
    addContent(m_availability);

    buildKeeps();
    buildStartStop();
    buildFiles();
    buildWhere();
    buildPlayingBack();

    setAccess(access);
}

int RecordingsSetupPage::megabytesPerHour(bool wav, bool sidesApart)
{
    const int channels = sidesApart ? 2 : 1;
    const double bytesPerSecond = wav
        ? static_cast<double>(ContactRecorder::kFileRateHz) * kWavBytesPerSample * channels
        : (sidesApart ? ContactRecorder::kStereoKbitPerSec : ContactRecorder::kMonoKbitPerSec) * 1000.0 / 8.0;
    return static_cast<int>(std::lround(bytesPerSecond * kSecondsPerHour / kBytesPerMegabyte));
}

void RecordingsSetupPage::buildKeeps()
{
    QGroupBox* group = addSection(QStringLiteral("What the Record button keeps"));
    m_groups.append(group);
    auto* layout = group->layout();
    layout->addWidget(textLabel(
        QStringLiteral("The round Record button beside the slice flag records the contact on that slice."), group));

    m_hear = new QCheckBox(QStringLiteral("What you hear on the slice"), group);
    m_hear->setObjectName(QStringLiteral("recordingsKeepHear"));
    m_hear->setChecked(true);
    m_hear->setEnabled(false);
    m_hear->setToolTip(QStringLiteral("The Record button always keeps what you hear on the slice."));
    layout->addWidget(m_hear);

    m_myVoice = new QCheckBox(QStringLiteral("Your own voice when you transmit"), group);
    m_myVoice->setObjectName(QStringLiteral("recordingsKeepMyVoice"));
    m_myVoice->setChecked(true);
    layout->addWidget(m_myVoice);
    connect(m_myVoice, &QCheckBox::toggled, this, [this](bool on) {
        showDependentText();
        send(QStringLiteral("keepMyVoice"), on);
    });

    auto* sidesRow = new QHBoxLayout;
    sidesRow->setContentsMargins(21, 0, 0, 0);
    sidesRow->addWidget(textLabel(QStringLiteral("Put the two sides:"), group));
    m_sides = new QComboBox(group);
    m_sides->setObjectName(QStringLiteral("recordingsSides"));
    m_sides->addItem(QStringLiteral("Together, one after the other"), false);
    m_sides->addItem(QStringLiteral("Apart: them on the left, you on the right"), true);
    sidesRow->addWidget(m_sides);
    sidesRow->addStretch(1);
    static_cast<QVBoxLayout*>(layout)->addLayout(sidesRow);
    connect(m_sides, &QComboBox::currentIndexChanged, this, [this](int) {
        showDependentText();
        send(QStringLiteral("sidesApart"), m_sides->currentData().toBool());
    });
}

void RecordingsSetupPage::buildStartStop()
{
    QGroupBox* group = addSection(QStringLiteral("Starting and stopping"));
    m_groups.append(group);
    auto* layout = static_cast<QVBoxLayout*>(group->layout());

    m_autoStart = new QCheckBox(QStringLiteral("Start recording by itself when I transmit"), group);
    m_autoStart->setObjectName(QStringLiteral("recordingsAutoStart"));
    layout->addWidget(m_autoStart);
    connect(m_autoStart, &QCheckBox::toggled, this, [this](bool on) {
        showDependentText();
        send(QStringLiteral("autoStart"), on);
    });

    auto* stopRow = new QHBoxLayout;
    m_autoStop = new QCheckBox(QStringLiteral("Stop by itself after"), group);
    m_autoStop->setObjectName(QStringLiteral("recordingsAutoStop"));
    m_autoStop->setChecked(true);
    stopRow->addWidget(m_autoStop);
    m_quiet = new QSpinBox(group);
    m_quiet->setObjectName(QStringLiteral("recordingsQuietMinutes"));
    m_quiet->setRange(RecordingSettings::kQuietMinutesMin, RecordingSettings::kQuietMinutesMax);
    m_quiet->setValue(2);
    m_quiet->setSuffix(QStringLiteral(" min"));
    m_quiet->setKeyboardTracking(false);
    m_quiet->setFixedWidth(kQuietSpinWidth);
    stopRow->addWidget(m_quiet);
    QLabel* quietWords = noteLabel(QStringLiteral("with nothing heard and no transmit"), group);
    quietWords->setWordWrap(false);
    stopRow->addWidget(quietWords);
    stopRow->addStretch(1);
    layout->addLayout(stopRow);
    connect(m_autoStop, &QCheckBox::toggled, this, [this](bool on) {
        showDependentText();
        send(QStringLiteral("autoStop"), on);
    });
    connect(m_quiet, &QSpinBox::valueChanged, this, [this](int minutes) {
        showDependentText();
        send(QStringLiteral("quietMinutes"), minutes);
    });

    m_startStopSummary = noteLabel(QString(), group);
    m_startStopSummary->setObjectName(QStringLiteral("recordingsStartStopSummary"));
    layout->addWidget(m_startStopSummary);
}

void RecordingsSetupPage::buildFiles()
{
    QGroupBox* group = addSection(QStringLiteral("Files"));
    m_groups.append(group);
    auto* layout = static_cast<QVBoxLayout*>(group->layout());

    auto* formatRow = new QHBoxLayout;
    formatRow->addWidget(textLabel(QStringLiteral("Save as:"), group));
    m_format = new QComboBox(group);
    m_format->setObjectName(QStringLiteral("recordingsFormat"));
    m_format->addItem(QStringLiteral("MP3"), QStringLiteral("mp3"));
    m_format->addItem(QStringLiteral("WAV"), QStringLiteral("wav"));
    formatRow->addWidget(m_format);
    m_formatSize = noteLabel(QString(), group);
    m_formatSize->setObjectName(QStringLiteral("recordingsFormatSize"));
    formatRow->addWidget(m_formatSize, 1);
    layout->addLayout(formatRow);
    connect(m_format, &QComboBox::currentIndexChanged, this, [this](int) {
        showDependentText();
        send(QStringLiteral("format"), m_format->currentData().toString());
    });

    layout->addWidget(noteLabel(
        QStringLiteral("Also used for \"Save sound only\" from the Digital Spectrum Recorder."), group));
    layout->addWidget(textLabel(QStringLiteral("Each one is named for when and where it was made, with the call of "
                                               "the ham you worked when the log knows it:"),
                                group));
    m_nameExample = new QLabel(group);
    m_nameExample->setObjectName(QStringLiteral("recordingsNameExample"));
    m_nameExample->setTextFormat(Qt::PlainText);
    m_nameExample->setStyleSheet(QString::fromLatin1(kNameStyle));
    layout->addWidget(m_nameExample);
    layout->addWidget(noteLabel(QStringLiteral("You can rename any recording in the Recordings window."), group));
}

void RecordingsSetupPage::buildWhere()
{
    QGroupBox* group = addSection(QStringLiteral("Where they are kept"));
    m_groups.append(group);
    auto* layout = static_cast<QVBoxLayout*>(group->layout());

    auto* diskRow = new QHBoxLayout;
    diskRow->addWidget(textLabel(QStringLiteral("Disk:"), group));
    m_disk = new QComboBox(group);
    m_disk->setObjectName(QStringLiteral("recordingsDisk"));
    m_disk->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    diskRow->addWidget(m_disk, 1);
    layout->addLayout(diskRow);
    connect(m_disk, &QComboBox::currentIndexChanged, this, [this](int) {
        showDependentText();
        send(QStringLiteral("disk"), m_disk->currentData().toString());
    });

    m_sdNote = new QLabel(group);
    m_sdNote->setObjectName(QStringLiteral("recordingsSdCardNote"));
    m_sdNote->setWordWrap(true);
    m_sdNote->setTextFormat(Qt::RichText);
    m_sdNote->setStyleSheet(QString::fromLatin1(kOkBoxStyle));
    m_sdNote->setText(QStringLiteral("<b>The SD card is fine for these.</b> Recordings are only written while one "
                                     "is running, so they do not wear the card the way the Digital Spectrum "
                                     "Recorder would."));
    m_sdNote->setVisible(false);
    layout->addWidget(m_sdNote);

    m_usageBar = new QProgressBar(group);
    m_usageBar->setObjectName(QStringLiteral("recordingsUsageBar"));
    m_usageBar->setRange(0, kBarSteps);
    m_usageBar->setTextVisible(false);
    m_usageBar->setFixedHeight(10);
    m_usageBar->setStyleSheet(QString::fromLatin1(kBarStyle));
    layout->addWidget(m_usageBar);

    auto* usageRow = new QHBoxLayout;
    m_usage = noteLabel(QString(), group);
    m_usage->setObjectName(QStringLiteral("recordingsUsage"));
    usageRow->addWidget(m_usage, 1);
    m_recordings = new QPushButton(QStringLiteral("Recordings ›"), group);
    m_recordings->setObjectName(QStringLiteral("recordingsOpenWindow"));
    m_recordings->setToolTip(QStringLiteral("Open the Recordings window."));
    usageRow->addWidget(m_recordings, 0, Qt::AlignTop);
    layout->addLayout(usageRow);
    connect(m_recordings, &QPushButton::clicked, this, &RecordingsSetupPage::recordingsWindowRequested);

    layout->addWidget(noteLabel(QStringLiteral("Recordings are never erased by themselves. Only you delete them."),
                                group));
}

void RecordingsSetupPage::buildPlayingBack()
{
    QGroupBox* group = addSection(QStringLiteral("Playing back"));
    m_groups.append(group);
    auto* layout = static_cast<QVBoxLayout*>(group->layout());

    layout->addWidget(textLabel(QStringLiteral("Play comes out of your own speaker or headphones, on the computer or "
                                               "phone you press it on."),
                                group));
    m_onAirSentence = textLabel(QStringLiteral("Keying while a recording plays sends it on the air."), group);
    m_onAirSentence->setObjectName(QStringLiteral("recordingsOnAirSentence"));
    layout->addWidget(m_onAirSentence);
    layout->addWidget(textLabel(QStringLiteral("Use while it goes out:"), group));

    auto* row = new QHBoxLayout;
    for (const OnAirSwitch& sw : kOnAirSwitches) {
        auto* box = new QCheckBox(QString::fromLatin1(sw.text), group);
        box->setObjectName(QStringLiteral("recordingsOnAir_") + QString::fromLatin1(sw.field));
        box->setProperty("recordingsField", QString::fromLatin1(sw.field));
        row->addWidget(box);
        m_onAir.append(box);
        connect(box, &QCheckBox::toggled, this, [this, box](bool on) {
            send(box->property("recordingsField").toString(), on);
        });
    }
    row->addStretch(1);
    layout->addLayout(row);

    m_onAirNote = noteLabel(QString(), group);
    m_onAirNote->setObjectName(QStringLiteral("recordingsOnAirNote"));
    layout->addWidget(m_onAirNote);
}

void RecordingsSetupPage::setAccess(RecordingsAccess* access)
{
    for (const QMetaObject::Connection& c : std::as_const(m_accessConnections)) {
        disconnect(c);
    }
    m_accessConnections.clear();
    m_access = access;
    if (access != nullptr) {
        m_accessConnections.append(
            connect(access, &RecordingsAccess::documentChanged, this, &RecordingsSetupPage::showDocument));
        m_accessConnections.append(
            connect(access, &RecordingsAccess::stateChanged, this, &RecordingsSetupPage::showDocument));
    }
    showDocument();
}

void RecordingsSetupPage::send(const QString& field, const QJsonValue& value)
{
    if (m_updatingFromModel || !m_access) {
        return;
    }
    m_access->setSettings(QJsonObject{{field, value}});
}

void RecordingsSetupPage::showDocument()
{
    const QJsonObject document = m_access ? m_access->document() : QJsonObject();
    const QJsonObject settings = document.value(QStringLiteral("settings")).toObject();
    m_updatingFromModel = true;
    {
        const QSignalBlocker b1(m_myVoice);
        const QSignalBlocker b2(m_sides);
        const QSignalBlocker b3(m_autoStart);
        const QSignalBlocker b4(m_autoStop);
        const QSignalBlocker b5(m_quiet);
        const QSignalBlocker b6(m_format);
        const QSignalBlocker b7(m_disk);

        // The spec's defaults until the Core says otherwise.
        m_myVoice->setChecked(settings.value(QStringLiteral("keepMyVoice")).toBool(true));
        m_sides->setCurrentIndex(settings.value(QStringLiteral("sidesApart")).toBool(false) ? 1 : 0);
        m_autoStart->setChecked(settings.value(QStringLiteral("autoStart")).toBool(false));
        m_autoStop->setChecked(settings.value(QStringLiteral("autoStop")).toBool(true));
        m_quiet->setValue(settings.value(QStringLiteral("quietMinutes")).toInt(2));
        m_format->setCurrentIndex(settings.value(QStringLiteral("format")).toString() == QLatin1String("wav") ? 1 : 0);

        m_disk->clear();
        const QString chosen = samePath(settings.value(QStringLiteral("disk")).toString());
        int chosenIndex = -1;
        for (const QJsonValue& value : settings.value(QStringLiteral("disks")).toArray()) {
            const QJsonObject disk = value.toObject();
            const bool own = disk.value(QStringLiteral("own")).toBool();
            const QString name = disk.value(QStringLiteral("name")).toString();
            const QString free = RecordingsWindow::sizeText(disk.value(QStringLiteral("free")).toInteger());
            const QString root = own ? QString() : disk.value(QStringLiteral("root")).toString();
            QString text;
            if (own) {
                text = name.isEmpty() ? QStringLiteral("The Core's own disk (%1 free)").arg(free)
                                      : QStringLiteral("The Core's own disk (%1, %2 free)").arg(name, free);
            } else {
                text = QStringLiteral("%1 (%2 free)").arg(name.isEmpty() ? root : name, free);
            }
            m_disk->addItem(text, root);
            const int index = m_disk->count() - 1;
            m_disk->setItemData(index, disk.value(QStringLiteral("sd")).toBool(), Qt::UserRole + 1);
            m_disk->setItemData(index, disk.value(QStringLiteral("free")).toInteger(), Qt::UserRole + 2);
            if (samePath(root) == chosen) {
                chosenIndex = index;
            }
        }
        if (chosenIndex < 0 && !chosen.isEmpty()) {
            // The chosen disk is not mounted on the Core now: show it, so
            // the page does not claim another disk is in use.
            m_disk->addItem(QStringLiteral("%1 (not on the Core now)").arg(QDir::toNativeSeparators(chosen)),
                            QDir::toNativeSeparators(chosen));
            chosenIndex = m_disk->count() - 1;
        }
        m_disk->setCurrentIndex(std::max(chosenIndex, m_disk->count() > 0 ? 0 : -1));

        for (QCheckBox* box : std::as_const(m_onAir)) {
            const QSignalBlocker blocker(box);
            box->setChecked(settings.value(box->property("recordingsField").toString()).toBool(false));
        }
    }
    m_updatingFromModel = false;
    showAvailability();
    showDependentText();
}

void RecordingsSetupPage::showAvailability()
{
    const QString reason = m_access ? m_access->unavailableReason() : RecordingsAccess::notConnectedReason();
    const bool usable = reason.isEmpty();
    m_availability->setText(reason);
    m_availability->setVisible(!usable);
    for (QGroupBox* group : std::as_const(m_groups)) {
        group->setEnabled(usable);
        group->setToolTip(reason);
    }

    const QString onAirReason =
        m_access ? m_access->unavailableReason(kOnAirVersion) : RecordingsAccess::notConnectedReason();
    for (QCheckBox* box : std::as_const(m_onAir)) {
        box->setEnabled(onAirReason.isEmpty());
        box->setToolTip(onAirReason);
        box->setAccessibleDescription(onAirReason);
    }
    m_onAirNote->setText(onAirReason);
    m_onAirNote->setVisible(usable && !onAirReason.isEmpty());
}

void RecordingsSetupPage::showDependentText()
{
    const bool wav = m_format->currentData().toString() == QLatin1String("wav");
    const bool apart = m_myVoice->isChecked() && m_sides->currentData().toBool();
    const int mbPerHour = megabytesPerHour(wav, apart);

    m_sides->setEnabled(m_myVoice->isChecked());
    m_sides->setToolTip(m_myVoice->isChecked()
                            ? QString()
                            : QStringLiteral("Only what you hear is kept while your own voice is off."));
    m_quiet->setEnabled(m_autoStop->isChecked());
    m_quiet->setToolTip(m_autoStop->isChecked()
                            ? QString()
                            : QStringLiteral("Turn on \"Stop by itself\" to choose how long."));

    const QString starts = m_autoStart->isChecked()
        ? QStringLiteral("Every contact you answer or call is recorded without pressing Record.")
        : QStringLiteral("Recording starts only when you press Record.");
    const QString stops = m_autoStop->isChecked()
        ? QStringLiteral("A recording left running ends %1 min after the band goes quiet.").arg(m_quiet->value())
        : QStringLiteral("A recording runs until you press Record again.");
    m_startStopSummary->setText(starts + QLatin1Char(' ') + stops);

    m_formatSize->setText(wav ? QStringLiteral("Full quality, bigger. About %1 MB an hour.").arg(mbPerHour)
                              : QStringLiteral("Small, plays anywhere. About %1 MB an hour.").arg(mbPerHour));
    m_nameExample->setText(recordingBaseName(kExampleStartUtc, kExampleFrequencyHz, QStringLiteral("LSB"),
                                             QStringLiteral("K4ABC"))
                           + (wav ? QStringLiteral(".wav") : QStringLiteral(".mp3")));

    const int disk = m_disk->currentIndex();
    m_sdNote->setVisible(disk >= 0 && m_disk->itemData(disk, Qt::UserRole + 1).toBool());

    const QJsonObject document = m_access ? m_access->document() : QJsonObject();
    const QJsonObject index = document.value(QStringLiteral("index")).toObject();
    if (index.isEmpty()) {
        m_usage->setText(QStringLiteral("Not known."));
        m_usageBar->setValue(0);
        return;
    }
    const qint64 count = index.value(QStringLiteral("count")).toInteger();
    const qint64 used = index.value(QStringLiteral("usedBytes")).toInteger();
    const qint64 free = disk >= 0 && m_disk->itemData(disk, Qt::UserRole + 2).isValid()
        ? m_disk->itemData(disk, Qt::UserRole + 2).toLongLong()
        : index.value(QStringLiteral("freeBytes")).toInteger();
    const QString recordings = count == 1 ? QStringLiteral("1 recording")
                                          : QStringLiteral("%1 recordings").arg(count);
    const double hours = static_cast<double>(free) / (mbPerHour * kBytesPerMegabyte);
    const QString room = hours > kManyHours
        ? QStringLiteral("room for well over a thousand hours more.")
        : QStringLiteral("room for about %1 hours more.").arg(static_cast<qint64>(std::floor(hours)));
    m_usage->setText(QStringLiteral("%1, %2. %3 free on this disk, %4")
                         .arg(recordings, RecordingsWindow::sizeText(used), RecordingsWindow::sizeText(free), room));
    const qint64 whole = used + free;
    m_usageBar->setValue(whole > 0 ? static_cast<int>(std::max<qint64>(
                                         used > 0 ? 1 : 0, used * kBarSteps / whole))
                                   : 0);
}

}  // namespace NereusSDR
