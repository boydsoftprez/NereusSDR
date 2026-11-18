// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/setup/DsrSetupPage.cpp  (NereusSDR)
// =================================================================
//
// See DsrSetupPage.h. Words and layout follow the approved mockup
// (docs/architecture/2026-11-15-recording-voice-keyer-dsr-design/
// dsr-setup/), with the spec's sentences where they differ.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code. Recording plan
//                 Task 26.
//   2026-11-17 - J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code: the disk drop-down is filled in
//                 place through DiskComboFiller instead of cleared on every
//                 Core update, which crashed under Qt accessibility (PR 808).
// =================================================================

#include "gui/setup/DsrSetupPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#include "core/recordings/RecordingNames.h"
#include "core/recordings/RecordingSettings.h"
#include "core/session/RecordingsFacade.h"
#include "gui/recordings/RecordingsWindow.h"
#include "gui/setup/DiskComboFiller.h"

namespace NereusSDR {

namespace {

constexpr const char* kNoteStyle = "color:#a6a6ab;";
constexpr const char* kLoopStyle =
    "QLabel { color:#dcdce1; background:#0f0f12; border:1px solid #38383e; border-radius:4px;"
    " padding:6px 8px; }";
constexpr const char* kWarnStyle =
    "QLabel { color:#ffd0d0; background:#1d0b0c; border:1px solid #a03030; border-radius:4px;"
    " padding:7px 9px; }";
constexpr const char* kDangerStyle =
    "QPushButton { color:#ffb4b4; background:#1b1b1f; border:1px solid #6b2c2e; border-radius:5px;"
    " padding:3px 10px; }"
    " QPushButton:hover { background:#26161a; }"
    " QPushButton:disabled { color:#5c5c62; border-color:#38383e; }";
constexpr const char* kSizeValueStyle = "QSpinBox { font-weight:600; }";

// The mockup's colours: kept, the write head, erased next, and the dots.
constexpr QRgb kBarBackground = 0xff040406;
constexpr QRgb kBarBorder = 0xff2c2c31;
constexpr QRgb kKeptColour = 0xff2f5f92;
constexpr QRgb kHeadColour = 0xffffffff;
constexpr QRgb kEraseColour = 0xffc02020;
constexpr const char* kOnDot = "#4caf6a";
constexpr const char* kOffDot = "#5c5c62";

constexpr double kBytesPerGb = 1.0e9;
constexpr qint64 kAlignBytes = 4096;   // a block starts on this (DsrRingFile::kAlignBytes)
constexpr int kSecondsPerHour = 3600;
constexpr int kMinutesPerHour = 60;
constexpr int kHoursPerDay = 24;
constexpr double kDaysFromHours = 48.0;   // past two days, say days
constexpr int kBarHeight = 10;
constexpr int kHeadWidth = 2;
constexpr int kEraseMinWidth = 4;
constexpr int kSpinWidth = 86;
constexpr int kFormatWidth = 200;
// The erased-next zone: the minute about to be written over.
constexpr int kEraseNextSeconds = 60;

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

QString gbText(qint64 bytes)
{
    return QStringLiteral("%1 GB").arg(static_cast<double>(std::max<qint64>(0, bytes)) / kBytesPerGb, 0, 'f', 1);
}

QString utcClock(qint64 utcMs)
{
    return QDateTime::fromMSecsSinceEpoch(utcMs, QTimeZone::utc()).toString(QStringLiteral("HH:mm"));
}

QString ellipsis()
{
    return QString(QChar(0x2026));
}

// The disk's line in the list.
QString diskText(const QJsonObject& disk)
{
    const bool own = disk.value(QStringLiteral("own")).toBool();
    const bool sd = disk.value(QStringLiteral("sd")).toBool();
    const QString name = disk.value(QStringLiteral("name")).toString();
    const QString root = disk.value(QStringLiteral("root")).toString();
    const QString free = RecordingsWindow::sizeText(disk.value(QStringLiteral("free")).toInteger());
    if (sd) {
        // A14: listed, never chosen.
        const QString what = own ? QStringLiteral("SD card")
                                 : QStringLiteral("%1, an SD card").arg(name.isEmpty() ? root : name);
        return QStringLiteral("%1 (%2 free): wears out, cannot be used").arg(what, free);
    }
    if (own) {
        return name.isEmpty() ? QStringLiteral("The Core's own disk (%1 free)").arg(free)
                              : QStringLiteral("The Core's own disk (%1, %2 free)").arg(name, free);
    }
    return QStringLiteral("%1 (%2 free)").arg(name.isEmpty() ? root : name, free);
}

QString warningHtml(bool onlySd)
{
    const QString head = onlySd ? QStringLiteral("This Core needs a proper storage device for the recorder")
                                : QStringLiteral("The recorder cannot use an SD card");
    const QString first = onlySd
        ? QStringLiteral("This Core runs from an SD card.")
        : QStringLiteral("The disk chosen for the recorder is an SD card.");
    const QString step = onlySd
        ? QStringLiteral("Connect a USB SSD or an NVMe drive to the Core, then choose it under Disk.")
        : QStringLiteral("Choose a USB SSD or an NVMe drive under Disk, or connect one to the Core.");
    return QStringLiteral("<b style=\"color:#ff8a8a;\">%1</b><br>%2 The Digital Spectrum Recorder writes nonstop, "
                          "day and night, and that would wear an SD card out in weeks and could take the Core "
                          "down with it. So the recorder will not write to it."
                          "<ul style=\"margin:4px 0 0 0;\"><li>%3</li>"
                          "<li>A USB thumb drive is not enough: it wears out the same way an SD card does.</li>"
                          "</ul>")
        .arg(head, first, step);
}

}  // namespace

// ---- DsrUsageBar

DsrUsageBar::DsrUsageBar(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("dsrUsageBar"));
    setFixedHeight(kBarHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_timer = new QTimer(this);
    m_timer->setInterval(kStepMs);
    connect(m_timer, &QTimer::timeout, this, [this] { advance(kStepMs); });
}

QSize DsrUsageBar::sizeHint() const
{
    return {200, kBarHeight};
}

void DsrUsageBar::setFigures(qint64 limitBytes, qint64 usedBytes, qint64 writePosition, qint64 bytesPerHour,
                             bool recording, bool full)
{
    m_limit = std::max<qint64>(0, limitBytes);
    m_used = std::clamp<qint64>(usedBytes, 0, m_limit);
    m_head = static_cast<double>(std::clamp<qint64>(writePosition, 0, m_limit));
    m_bytesPerHour = std::max<qint64>(0, bytesPerHour);
    m_recording = recording;
    m_full = full && recording;
    updateTimer();
    update();
}

bool DsrUsageBar::moving() const
{
    return m_recording && m_limit > 0 && m_bytesPerHour > 0;
}

void DsrUsageBar::updateTimer()
{
    if (moving() && !m_timer->isActive()) {
        m_timer->start();
    } else if (!moving() && m_timer->isActive()) {
        m_timer->stop();
    }
}

void DsrUsageBar::advance(int ms)
{
    if (!moving() || ms <= 0) {
        return;
    }
    const double bytes = static_cast<double>(m_bytesPerHour) * ms / (kSecondsPerHour * 1000.0);
    m_head += bytes;
    if (m_full) {
        // Round the loop: it goes on from the front.
        m_head = std::fmod(m_head, static_cast<double>(m_limit));
    } else {
        // Filling: the kept part grows with the head until the limit.
        m_head = std::min(m_head, static_cast<double>(m_limit));
        m_used = std::max(m_used, static_cast<qint64>(m_head));
    }
    update();
}

double DsrUsageBar::headFraction() const
{
    return m_limit > 0 ? m_head / static_cast<double>(m_limit) : 0.0;
}

double DsrUsageBar::keptFraction() const
{
    return m_limit > 0 ? static_cast<double>(m_used) / static_cast<double>(m_limit) : 0.0;
}

double DsrUsageBar::eraseFraction() const
{
    return m_full ? headFraction() : 0.0;
}

double DsrUsageBar::eraseWidthFraction() const
{
    if (!m_full || m_limit <= 0) {
        return 0.0;
    }
    const double next = static_cast<double>(m_bytesPerHour) * kEraseNextSeconds / kSecondsPerHour;
    return std::min(1.0, next / static_cast<double>(m_limit));
}

void DsrUsageBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    const QRectF outer = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QColor(kBarBorder));
    p.setBrush(QColor(kBarBackground));
    p.drawRoundedRect(outer, 3, 3);
    p.setRenderHint(QPainter::Antialiasing, false);
    const QRect inner = rect().adjusted(1, 1, -1, -1);
    if (m_limit <= 0 || inner.width() <= 0) {
        return;
    }
    p.setClipRect(inner);
    const double w = inner.width();
    p.fillRect(QRectF(inner.left(), inner.top(), w * keptFraction(), inner.height()), QColor(kKeptColour));
    if (m_full) {
        const double x = inner.left() + w * eraseFraction();
        const double ew = std::max<double>(kEraseMinWidth, w * eraseWidthFraction());
        p.fillRect(QRectF(x, inner.top(), ew, inner.height()), QColor(kEraseColour));
        // The zone wraps to the front when the head is at the end.
        if (x + ew > inner.right()) {
            p.fillRect(QRectF(inner.left(), inner.top(), x + ew - inner.right(), inner.height()),
                       QColor(kEraseColour));
        }
    }
    if (m_recording) {
        const double hx = inner.left() + w * headFraction() - kHeadWidth / 2.0;
        p.fillRect(QRectF(std::clamp<double>(hx, inner.left(), inner.right() - kHeadWidth), inner.top(),
                          kHeadWidth, inner.height()),
                   QColor(kHeadColour));
    }
}

// ---- DsrSetupPage

DsrSetupPage::DsrSetupPage(RecordingsAccess* access, QWidget* parent)
    : SetupPage(QStringLiteral("Digital Spectrum Recorder"), parent)
{
    setObjectName(QStringLiteral("DsrSetupPage"));
    m_availability = noteLabel(QString(), this);
    m_availability->setObjectName(QStringLiteral("dsrSetupAvailability"));
    addContent(m_availability);

    buildRecording();
    buildWhere();
    buildSaved();
    buildErase();

    m_confirm = [this](const QString& title, const QString& text) {
        // The question as the text, so it shows on a Mac (whose message
        // boxes show no title), and the details under it.
        QMessageBox box(QMessageBox::Warning, title, title, QMessageBox::NoButton, this);
        box.setInformativeText(text);
        QPushButton* erase = box.addButton(QStringLiteral("Erase"), QMessageBox::DestructiveRole);
        QPushButton* cancel = box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(cancel);
        box.setEscapeButton(cancel);
        box.exec();
        return box.clickedButton() == erase;
    };

    setAccess(access);
}

QString DsrSetupPage::durationText(double hours)
{
    if (!(hours > 0.0)) {
        return QStringLiteral("0 min");
    }
    if (hours >= kDaysFromHours) {
        return QStringLiteral("%1 days").arg(hours / kHoursPerDay, 0, 'f', 1);
    }
    const qint64 minutes = std::llround(hours * kMinutesPerHour);
    if (minutes < kMinutesPerHour) {
        return QStringLiteral("%1 min").arg(std::max<qint64>(1, minutes));
    }
    const qint64 h = minutes / kMinutesPerHour;
    const qint64 m = minutes % kMinutesPerHour;
    return m == 0 ? QStringLiteral("%1 h").arg(h) : QStringLiteral("%1 h %2 min").arg(h).arg(m);
}

QString DsrSetupPage::receiversText(const QJsonArray& receivers)
{
    QList<int> rates;
    for (const QJsonValue& value : receivers) {
        const int khz = static_cast<int>(std::lround(value.toObject().value(QStringLiteral("rateHz")).toDouble() / 1000.0));
        if (khz > 0 && !rates.contains(khz)) {
            rates.append(khz);
        }
    }
    if (receivers.isEmpty() || rates.isEmpty()) {
        return {};
    }
    std::sort(rates.begin(), rates.end());
    QStringList words;
    for (const int khz : std::as_const(rates)) {
        words.append(QString::number(khz));
    }
    QString list = words.constLast();
    if (words.size() > 1) {
        list = QStringList(words.mid(0, words.size() - 1)).join(QStringLiteral(", ")) + QStringLiteral(" and ")
               + words.constLast();
    }
    const qsizetype n = receivers.size();
    return n == 1 ? QStringLiteral("1 receiver at %1 kHz").arg(list)
                  : QStringLiteral("%1 receivers at %2 kHz").arg(n).arg(list);
}

QString DsrSetupPage::eraseTitle()
{
    return QStringLiteral("Erase what the recorder has kept?");
}

QString DsrSetupPage::eraseText() const
{
    const qint64 used = m_dsr.value(QStringLiteral("usedBytes")).toInteger();
    return QStringLiteral("This erases %1 of kept waterfall and sound. Rewinding starts again from now. Saved "
                          "stretches stay. This cannot be undone.")
        .arg(gbText(used));
}

void DsrSetupPage::buildRecording()
{
    QGroupBox* group = addSection(QStringLiteral("Recording"));
    m_groups.append(group);
    auto* layout = static_cast<QVBoxLayout*>(group->layout());

    m_enabled = new QCheckBox(QStringLiteral("Keep the waterfall and sound so you can rewind"), group);
    m_enabled->setObjectName(QStringLiteral("dsrEnabled"));
    layout->addWidget(m_enabled);
    connect(m_enabled, &QCheckBox::toggled, this, [this](bool on) {
        send(QJsonObject{{QStringLiteral("dsrEnabled"), on}});
    });

    m_status = new QLabel(group);
    m_status->setObjectName(QStringLiteral("dsrStatus"));
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::RichText);
    m_status->setStyleSheet(QString::fromLatin1(kNoteStyle));
    layout->addWidget(m_status);
}

void DsrSetupPage::buildWhere()
{
    QGroupBox* group = addSection(QStringLiteral("Where it is kept"));
    m_groups.append(group);
    auto* layout = static_cast<QVBoxLayout*>(group->layout());

    m_sdWarning = new QLabel(group);
    m_sdWarning->setObjectName(QStringLiteral("dsrSdWarning"));
    m_sdWarning->setWordWrap(true);
    m_sdWarning->setTextFormat(Qt::RichText);
    m_sdWarning->setStyleSheet(QString::fromLatin1(kWarnStyle));
    m_sdWarning->setVisible(false);
    layout->addWidget(m_sdWarning);

    auto* diskRow = new QHBoxLayout;
    diskRow->addWidget(textLabel(QStringLiteral("Disk:"), group));
    m_disk = new QComboBox(group);
    m_disk->setObjectName(QStringLiteral("dsrDisk"));
    m_disk->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    diskRow->addWidget(m_disk, 1);
    m_diskFiller = new DiskComboFiller(m_disk, [this] { showDocument(); }, this);
    m_scan = new QPushButton(QStringLiteral("Scan again"), group);
    m_scan->setObjectName(QStringLiteral("dsrScanDisks"));
    diskRow->addWidget(m_scan);
    layout->addLayout(diskRow);
    connect(m_disk, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_updatingFromModel || index < 0) {
            return;
        }
        const QVariant root = m_disk->itemData(index);
        if (!root.isValid()) {
            return;
        }
        QJsonObject changed{{QStringLiteral("dsrDisk"), root.toString()}};
        // The mockup: a proper disk chosen after the SD card warning turns
        // the recorder on, since its switch could not be used before.
        if (m_sdBlocked && !m_dsr.value(QStringLiteral("enabled")).toBool(false)
            && !m_disk->itemData(index, Qt::UserRole + 1).toBool()) {
            changed.insert(QStringLiteral("dsrEnabled"), true);
        }
        send(changed);
    });
    connect(m_scan, &QPushButton::clicked, this, [this] {
        // Any settings change makes the Core read its disks again; this
        // one changes nothing else.
        send(QJsonObject{{QStringLiteral("dsrDisk"), m_dsr.value(QStringLiteral("disk")).toString()}});
    });

    auto* sizeRow = new QHBoxLayout;
    sizeRow->addWidget(textLabel(QStringLiteral("Size limit:"), group));
    m_sizeSlider = new QSlider(Qt::Horizontal, group);
    m_sizeSlider->setObjectName(QStringLiteral("dsrSizeSlider"));
    m_sizeSlider->setRange(RecordingSettings::kDsrSizeLimitGbMin, RecordingSettings::kDsrSizeLimitGbMax);
    sizeRow->addWidget(m_sizeSlider, 1);
    m_sizeSpin = new QSpinBox(group);
    m_sizeSpin->setObjectName(QStringLiteral("dsrSizeSpin"));
    m_sizeSpin->setRange(RecordingSettings::kDsrSizeLimitGbMin, RecordingSettings::kDsrSizeLimitGbMax);
    m_sizeSpin->setSuffix(QStringLiteral(" GB"));
    m_sizeSpin->setKeyboardTracking(false);
    m_sizeSpin->setFixedWidth(kSpinWidth);
    m_sizeSpin->setAlignment(Qt::AlignCenter);
    m_sizeSpin->setStyleSheet(QString::fromLatin1(kSizeValueStyle));
    sizeRow->addWidget(m_sizeSpin);
    layout->addLayout(sizeRow);
    connect(m_sizeSlider, &QSlider::valueChanged, this, [this](int gb) {
        {
            const QSignalBlocker blocker(m_sizeSpin);
            m_sizeSpin->setValue(gb);
        }
        // While dragging, only the hours follow; the Core gets it on release.
        showSizeDependent(gb);
        if (!m_sizeSlider->isSliderDown()) {
            commitSize(gb);
        }
    });
    connect(m_sizeSlider, &QSlider::sliderReleased, this, [this] { commitSize(m_sizeSlider->value()); });
    connect(m_sizeSpin, &QSpinBox::valueChanged, this, [this](int gb) {
        {
            const QSignalBlocker blocker(m_sizeSlider);
            m_sizeSlider->setValue(gb);
        }
        showSizeDependent(gb);
        commitSize(gb);
    });

    m_holds = new QLabel(group);
    m_holds->setObjectName(QStringLiteral("dsrHolds"));
    m_holds->setWordWrap(true);
    m_holds->setTextFormat(Qt::RichText);
    layout->addWidget(m_holds);

    m_loopNote = new QLabel(group);
    m_loopNote->setObjectName(QStringLiteral("dsrLoopNote"));
    m_loopNote->setWordWrap(true);
    m_loopNote->setTextFormat(Qt::RichText);
    m_loopNote->setStyleSheet(QString::fromLatin1(kLoopStyle));
    m_loopNote->setText(QStringLiteral(
        "<b style=\"color:#ffffff;\">It is always erasing.</b> The recorder never stops: once it reaches the size "
        "you set, every new minute it records erases the oldest minute it has. Whatever size you pick, anything "
        "older than that is gone for good. To keep something, save it as a stretch."));
    layout->addWidget(m_loopNote);

    m_usageBar = new DsrUsageBar(group);
    layout->addWidget(m_usageBar);

    m_usageKey = new QLabel(group);
    m_usageKey->setObjectName(QStringLiteral("dsrUsageKey"));
    m_usageKey->setTextFormat(Qt::RichText);
    m_usageKey->setStyleSheet(QStringLiteral("color:#a6a6ab; font-size:11px;"));
    const QString square = QString(QChar(0x25A0));
    m_usageKey->setText(QStringLiteral("<span style=\"color:#ffffff;\">%1</span> recording now &nbsp; "
                                       "<span style=\"color:#c02020;\">%1</span> erased next &nbsp; "
                                       "<span style=\"color:#2f5f92;\">%1</span> kept")
                            .arg(square));
    layout->addWidget(m_usageKey);

    m_usage = noteLabel(QString(), group);
    m_usage->setObjectName(QStringLiteral("dsrUsage"));
    layout->addWidget(m_usage);

    m_unusableNote = noteLabel(QString(), group);
    m_unusableNote->setObjectName(QStringLiteral("dsrUnusableNote"));
    layout->addWidget(m_unusableNote);
}

void DsrSetupPage::buildSaved()
{
    QGroupBox* group = addSection(QStringLiteral("Saved stretches"));
    m_groups.append(group);
    auto* layout = static_cast<QVBoxLayout*>(group->layout());
    layout->addWidget(textLabel(
        QStringLiteral("Stretches you save are kept apart from this limit and are never overwritten."), group));

    auto* savedRow = new QHBoxLayout;
    m_saved = noteLabel(QString(), group);
    m_saved->setObjectName(QStringLiteral("dsrSaved"));
    savedRow->addWidget(m_saved, 1);
    m_recordings = new QPushButton(QStringLiteral("Show recordings"), group);
    m_recordings->setObjectName(QStringLiteral("dsrShowRecordings"));
    savedRow->addWidget(m_recordings);
    layout->addLayout(savedRow);
    connect(m_recordings, &QPushButton::clicked, this, &DsrSetupPage::recordingsWindowRequested);

    auto* formatRow = new QHBoxLayout;
    formatRow->addWidget(textLabel(QStringLiteral("Save sound only as:"), group));
    m_format = new QComboBox(group);
    m_format->setObjectName(QStringLiteral("dsrSoundFormat"));
    m_format->addItem(QStringLiteral("MP3"), QStringLiteral("mp3"));
    m_format->addItem(QStringLiteral("WAV"), QStringLiteral("wav"));
    m_format->setFixedWidth(kFormatWidth);
    formatRow->addWidget(m_format);
    formatRow->addStretch(1);
    layout->addLayout(formatRow);
    connect(m_format, &QComboBox::currentIndexChanged, this, [this](int) {
        send(QJsonObject{{QStringLiteral("format"), m_format->currentData().toString()}});
    });
}

void DsrSetupPage::buildErase()
{
    auto* row = new QHBoxLayout;
    m_eraseNote = noteLabel(QString(), this);
    m_eraseNote->setObjectName(QStringLiteral("dsrEraseNote"));
    m_eraseNote->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(m_eraseNote, 1);
    m_erase = new QPushButton(QStringLiteral("Erase what is kept now") + ellipsis(), this);
    m_erase->setObjectName(QStringLiteral("dsrEraseAll"));
    m_erase->setStyleSheet(QString::fromLatin1(kDangerStyle));
    row->addWidget(m_erase);
    addContent(row);
    connect(m_erase, &QPushButton::clicked, this, &DsrSetupPage::askToErase);

    m_eraseNoteTimer = new QTimer(this);
    m_eraseNoteTimer->setSingleShot(true);
    m_eraseNoteTimer->setInterval(kErasedNoteMs);
    connect(m_eraseNoteTimer, &QTimer::timeout, m_eraseNote, &QLabel::clear);
}

void DsrSetupPage::setAccess(RecordingsAccess* access)
{
    for (const QMetaObject::Connection& c : std::as_const(m_accessConnections)) {
        disconnect(c);
    }
    m_accessConnections.clear();
    m_access = access;
    m_indexRevision = -1;
    m_wholePanReader = 0;
    m_soundReader = 0;
    if (access != nullptr) {
        m_wholePanReader = access->newListReader();
        m_soundReader = access->newListReader();
        m_accessConnections.append(
            connect(access, &RecordingsAccess::documentChanged, this, &DsrSetupPage::showDocument));
        m_accessConnections.append(
            connect(access, &RecordingsAccess::stateChanged, this, &DsrSetupPage::showDocument));
        m_accessConnections.append(connect(access, &RecordingsAccess::listChanged, this, [this](int reader) {
            if (reader == m_wholePanReader || reader == m_soundReader) {
                showSaved();
            }
        }));
    }
    showDocument();
}

void DsrSetupPage::send(const QJsonObject& changed)
{
    if (m_updatingFromModel || !m_access || !m_ready) {
        return;
    }
    m_access->setSettings(changed);
}

void DsrSetupPage::commitSize(int gb)
{
    if (m_updatingFromModel || gb == m_dsr.value(QStringLiteral("sizeLimitGb")).toInt(-1)) {
        return;
    }
    send(QJsonObject{{QStringLiteral("dsrSizeLimitGb"), gb}});
}

void DsrSetupPage::askToErase()
{
    if (!m_access || !m_ready) {
        return;
    }
    // The confirm is the safeguard: nothing is erased without it.
    if (!m_confirm || !m_confirm(eraseTitle(), eraseText())) {
        return;
    }
    m_access->eraseDsr();
    m_eraseNote->setText(m_dsr.value(QStringLiteral("recording")).toBool(false)
                             ? QStringLiteral("Erased. The recorder keeps going from now.")
                             : QStringLiteral("Erased."));
    m_eraseNoteTimer->start();
}

void DsrSetupPage::showDocument()
{
    const QJsonObject document = m_access ? m_access->document() : QJsonObject();
    const QJsonObject settings = document.value(QStringLiteral("settings")).toObject();
    const QJsonArray disks = settings.value(QStringLiteral("disks")).toArray();
    m_dsr = document.value(QStringLiteral("dsr")).toObject();

    QString reason = m_access ? m_access->unavailableReason(RecordingsAccess::kDsrVersion)
                              : RecordingsAccess::notConnectedReason();
    const bool notConnected = !m_access || m_access->state() == RecordingsAccess::State::NotConnected;
    m_ready = reason.isEmpty() && !m_dsr.isEmpty();

    // The recorder's disk, and whether it is an SD card (A14).
    const QString chosen = samePath(m_dsr.value(QStringLiteral("disk")).toString());
    bool chosenSd = false;
    bool anyProper = false;
    for (const QJsonValue& value : disks) {
        const QJsonObject disk = value.toObject();
        const bool sd = disk.value(QStringLiteral("sd")).toBool();
        anyProper = anyProper || !sd;
        const bool own = disk.value(QStringLiteral("own")).toBool();
        if (chosen.isEmpty() ? own : samePath(disk.value(QStringLiteral("root")).toString()) == chosen) {
            chosenSd = sd;
        }
    }
    const bool onlySd = !disks.isEmpty() && !anyProper;
    m_sdBlocked = m_ready && chosenSd;
    const bool usable = m_ready && !m_sdBlocked;
    const bool enabled = m_dsr.value(QStringLiteral("enabled")).toBool(false);
    const bool recording = m_dsr.value(QStringLiteral("recording")).toBool(false);
    const QString offReason = m_dsr.value(QStringLiteral("reason")).toString();
    const qint64 limitBytes = m_dsr.value(QStringLiteral("sizeLimitBytes")).toInteger();
    const qint64 used = m_dsr.value(QStringLiteral("usedBytes")).toInteger();
    const qint64 writePosition = m_dsr.value(QStringLiteral("writePosition")).toInteger();
    const qint64 bytesPerHour = m_dsr.value(QStringLiteral("bytesPerHour")).toInteger();
    const int sizeGb = m_dsr.value(QStringLiteral("sizeLimitGb")).toInt(RecordingSettings{}.dsrSizeLimitGb);

    m_updatingFromModel = true;
    {
        const QSignalBlocker b1(m_enabled);
        const QSignalBlocker b2(m_disk);
        const QSignalBlocker b3(m_sizeSlider);
        const QSignalBlocker b4(m_sizeSpin);
        const QSignalBlocker b5(m_format);

        // The effective value: what the Core holds, unticked where it
        // cannot record at all.
        m_enabled->setChecked(usable && enabled);

        // The disks, filled in place: this runs on every update from the
        // Core (about once a second while it records), so the list is only
        // rebuilt when its disks change, and never while it is open.
        QList<DiskComboItem> items;
        int chosenIndex = -1;
        for (const QJsonValue& value : disks) {
            const QJsonObject disk = value.toObject();
            const bool own = disk.value(QStringLiteral("own")).toBool();
            const bool sd = disk.value(QStringLiteral("sd")).toBool();
            const QString root = own ? QString() : disk.value(QStringLiteral("root")).toString();
            items.append(
                DiskComboItem{diskText(disk), root, sd, disk.value(QStringLiteral("free")).toInteger(), !sd});
            if (chosen.isEmpty() ? own : samePath(root) == chosen) {
                chosenIndex = static_cast<int>(items.size()) - 1;
            }
        }
        if (chosenIndex < 0 && !chosen.isEmpty()) {
            // The chosen disk is not mounted on the Core now: show it, so
            // the page does not claim another disk is in use.
            items.append(DiskComboItem{
                QStringLiteral("%1 (not on the Core now)").arg(QDir::toNativeSeparators(chosen)),
                QDir::toNativeSeparators(chosen), QVariant(), QVariant(), true});
            chosenIndex = static_cast<int>(items.size()) - 1;
        }
        if (onlySd) {
            items.append(DiskComboItem{QStringLiteral("No USB disk found"), QVariant(), QVariant(), QVariant(), false});
        }
        m_diskFiller->fill(items, std::max(chosenIndex, items.isEmpty() ? -1 : 0));

        // The slider offers what the disk has room for; the box the same.
        const int diskIndex = m_disk->currentIndex();
        const qint64 free = diskIndex >= 0 && m_disk->itemData(diskIndex, Qt::UserRole + 2).isValid()
            ? m_disk->itemData(diskIndex, Qt::UserRole + 2).toLongLong()
            : 0;
        const int roomGb = static_cast<int>(
            std::floor(static_cast<double>(free + used) * kRoomShare / kBytesPerGb));
        const int maxGb = std::clamp(std::max(roomGb, sizeGb), RecordingSettings::kDsrSizeLimitGbMin,
                                     RecordingSettings::kDsrSizeLimitGbMax);
        m_sizeSlider->setRange(RecordingSettings::kDsrSizeLimitGbMin, maxGb);
        m_sizeSpin->setRange(RecordingSettings::kDsrSizeLimitGbMin, maxGb);
        m_sizeSlider->setValue(sizeGb);
        m_sizeSpin->setValue(sizeGb);

        m_format->setCurrentIndex(settings.value(QStringLiteral("format")).toString() == QLatin1String("wav") ? 1
                                                                                                                : 0);
    }
    m_updatingFromModel = false;

    // ---- Availability (V9) and what can be used.
    if (reason.isEmpty() && !m_ready) {
        reason = notConnected ? RecordingsAccess::notConnectedReason() : QStringLiteral("Waiting for the Core.");
    }
    m_availability->setText(reason);
    m_availability->setVisible(!reason.isEmpty());
    for (QGroupBox* group : std::as_const(m_groups)) {
        group->setEnabled(m_ready);
        group->setToolTip(reason);
    }
    const QString sdReason = onlySd
        ? QStringLiteral("Connect a USB SSD or NVMe drive and choose it under Disk to turn the recorder on.")
        : QStringLiteral("Choose a USB SSD or NVMe drive under Disk to turn the recorder on.");
    const QString switchReason = !m_ready ? reason : m_sdBlocked ? sdReason : QString();
    for (QWidget* w : std::initializer_list<QWidget*>{m_enabled, m_sizeSlider, m_sizeSpin}) {
        w->setEnabled(usable);
        w->setToolTip(switchReason);
        w->setAccessibleDescription(switchReason);
    }
    const std::pair<QWidget*, QString> others[] = {
        {m_disk, QString()},
        {m_scan, QStringLiteral("Look again for disks plugged into the Core.")},
        {m_recordings, QStringLiteral("Open the Recordings window.")},
        {m_format, QStringLiteral("The same as Files on the Recordings page, so contact recordings use it too.")}};
    for (const auto& [w, tip] : others) {
        w->setEnabled(m_ready);
        w->setToolTip(m_ready ? tip : reason);
        w->setAccessibleDescription(m_ready ? QString() : reason);
    }

    // ---- The SD card warning (A14).
    m_sdWarning->setText(warningHtml(onlySd));
    m_sdWarning->setVisible(m_sdBlocked);

    // ---- Status.
    QString words;
    bool dotOn = false;
    if (!m_ready) {
        words = notConnected ? QStringLiteral("Not known: no Core connected.") : QStringLiteral("Not known.");
    } else if (m_sdBlocked) {
        words = onlySd ? QStringLiteral("Off: this Core has no disk that can take it.")
                       : QStringLiteral("Off: the recorder's disk is an SD card.");
    } else if (!enabled) {
        words = QStringLiteral("Off. Nothing is being kept.");
    } else if (recording) {
        dotOn = true;
        const QString who = receiversText(m_dsr.value(QStringLiteral("receivers")).toArray());
        words = who.isEmpty() ? QStringLiteral("Recording now, including your own transmit audio.")
                              : QStringLiteral("Recording now: %1, including your own transmit audio.").arg(who);
    } else if (!offReason.isEmpty()) {
        words = QStringLiteral("Off. %1").arg(offReason);
    } else {
        words = QStringLiteral("On. It records while the radio is connected.");
    }
    m_status->setText(QStringLiteral("<span style=\"color:%1;\">%2</span>&nbsp; %3")
                          .arg(QString::fromLatin1(dotOn ? kOnDot : kOffDot), QString(QChar(0x25CF)),
                               words.toHtmlEscaped()));

    // ---- The loop, the bar and what is in use.
    const bool full = recording
        && ((writePosition + kAlignBytes < used)
            || (bytesPerHour > 0 && limitBytes - used < bytesPerHour / kSecondsPerHour));
    m_usageBar->setFigures(limitBytes, used, writePosition, bytesPerHour, usable && recording, usable && full);
    for (QWidget* w : std::initializer_list<QWidget*>{m_holds, m_loopNote, m_usageBar, m_usage}) {
        w->setVisible(usable);
    }
    m_usageKey->setVisible(usable && full);
    m_unusableNote->setVisible(!usable);
    m_unusableNote->setText(!m_ready ? QStringLiteral("Connect to the Core to change these.") : sdReason);

    const qint64 diskFree = m_disk->currentIndex() >= 0
        ? m_disk->itemData(m_disk->currentIndex(), Qt::UserRole + 2).toLongLong()
        : 0;
    QString usage;
    if (used <= 0) {
        usage = QStringLiteral("In use: nothing.");
    } else if (full) {
        const qint64 oldest = m_dsr.value(QStringLiteral("oldestUtcMs")).toInteger();
        const qint64 newest = m_dsr.value(QStringLiteral("newestUtcMs")).toInteger();
        usage = QStringLiteral("Full, erasing the oldest as it records. Kept now: %1 to %2 UTC.")
                    .arg(utcClock(oldest), utcClock(newest));
    } else if (recording && bytesPerHour > 0) {
        usage = QStringLiteral("Filling: %1 of %2 GB. Erasing starts when it is full, in about %3.")
                    .arg(gbText(used))
                    .arg(sizeGb)
                    .arg(durationText(static_cast<double>(limitBytes - used) / static_cast<double>(bytesPerHour)));
    } else {
        usage = QStringLiteral("In use: %1 of %2 GB.").arg(gbText(used)).arg(sizeGb);
    }
    if (diskFree > 0) {
        usage += QStringLiteral(" %1 free on this disk.").arg(RecordingsWindow::sizeText(diskFree));
    }
    m_usage->setText(usage);
    showSizeDependent(sizeGb);

    // ---- Erase.
    const QString eraseReason = !m_ready ? reason : used <= 0 ? QStringLiteral("Nothing is kept now.") : QString();
    m_erase->setEnabled(eraseReason.isEmpty());
    m_erase->setToolTip(eraseReason.isEmpty() ? QStringLiteral("Asks first. It cannot be undone.") : eraseReason);
    m_erase->setAccessibleDescription(eraseReason);

    // ---- Saved stretches: asked again when the list changes.
    const qint64 revision =
        document.value(QStringLiteral("index")).toObject().value(QStringLiteral("revision")).toInteger(-1);
    if (m_ready && revision != m_indexRevision) {
        m_indexRevision = revision;
        requestSaved();
    } else if (!m_ready) {
        m_indexRevision = -1;
    }
    showSaved();
}

void DsrSetupPage::showSizeDependent(int gb)
{
    const qint64 bytesPerHour = m_dsr.value(QStringLiteral("bytesPerHour")).toInteger();
    const QString who = receiversText(m_dsr.value(QStringLiteral("receivers")).toArray());
    if (bytesPerHour <= 0 || who.isEmpty()) {
        m_holds->setText(QStringLiteral("How long it holds is shown while the radio is connected."));
        return;
    }
    const double hours = static_cast<double>(gb) * kBytesPerGb / static_cast<double>(bytesPerHour);
    m_holds->setText(QStringLiteral("Holds about <b>%1</b> with %2.").arg(durationText(hours), who.toHtmlEscaped()));
}

void DsrSetupPage::requestSaved()
{
    if (!m_access) {
        return;
    }
    for (const auto& [reader, kind] : {std::pair{m_wholePanReader, RecordingKind::WholePan},
                                       std::pair{m_soundReader, RecordingKind::Sound}}) {
        RecordingsListQuery query;
        query.count = RecordingsFacade::kListPageMax;
        query.kind = kind;
        m_access->requestList(reader, query);
    }
}

void DsrSetupPage::showSaved()
{
    if (!m_ready || !m_access) {
        m_saved->setText(QStringLiteral("Not known."));
        return;
    }
    int total = 0;
    qint64 bytes = 0;
    bool every = true;
    for (const int reader : {m_wholePanReader, m_soundReader}) {
        const RecordingsListPage page = m_access->listed(reader);
        total += page.total;
        every = every && page.entries.size() >= page.total;
        for (const RecordingEntry& e : page.entries) {
            bytes += e.sizeBytes;
        }
    }
    if (total == 0) {
        m_saved->setText(QStringLiteral("None saved."));
    } else if (every) {
        m_saved->setText(QStringLiteral("%1 saved, %2.").arg(total).arg(RecordingsWindow::sizeText(bytes)));
    } else {
        m_saved->setText(QStringLiteral("%1 saved.").arg(total));
    }
}

}  // namespace NereusSDR
