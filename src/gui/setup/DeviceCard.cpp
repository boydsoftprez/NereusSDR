// =================================================================
// src/gui/setup/DeviceCard.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Audio → Devices card widget.
// See DeviceCard.h for the full header.
//
// Sub-Phase 12 Task 12.2 (2026-04-20): Written by J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// 2026-09-22 (R-R3-36 fix wave): a configured device that is not present
// stays selected as "<name> (not available)", and a configured buffer
// size the list lacks is added, so an unrelated edit never rewrites them.
// The input card offers 4096 and 8192 samples like the TX Input page.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-10-04: reset Qt6.11 Cocoa's popup accessibility cache before
// replacing device rows or retained device/buffer entries.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-06: R-SPK-21, R-SPK-24, D14. Driver API, Sample rate, Bit depth,
// Channels, Buffer size, Options and Negotiated fold under "Device
// details", folded by default; the WASAPI options are greyed unless the
// card's driver API is WASAPI; pages can add rows around the Device row,
// grey the card until Enabled, and rescan its device list.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-10-09: native audio plan Task 16 (R-AUD-01, R-AUD-03, R-AUD-08 to
// R-AUD-11, R-AUD-14 to R-AUD-17, D10). One Driver list replaces the Driver
// API list and the three WASAPI checkboxes; the Device list comes from the
// engine's device catalogue and follows it live; a missing device stays as
// "<name> (not connected)"; the role's status notes, the engine notes and
// the Delay line. The saved ExclusiveMode, EventDriven and BypassMixer keys
// are kept as they are and no longer written.
// J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "DeviceCard.h"

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/audio/IAudioDeviceCatalog.h"
#include "gui/UnbuiltFeatures.h"
#include "gui/setup/AudioDriverList.h"

#include <QCheckBox>
#include <QAbstractItemView>
#include <QAccessible>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QStandardItemModel>

#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace NereusSDR {

namespace {

// Shared style constants — match SetupPage / STYLEGUIDE.md palette.
static const char* kGroupStyle =
    "QGroupBox {"
    "  border: 1px solid #203040;"
    "  border-radius: 4px;"
    "  margin-top: 8px;"
    "  padding-top: 12px;"
    "  font-weight: bold;"
    "  color: #8aa8c0;"
    "}"
    "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }";

static const char* kComboStyle =
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

static const char* kCheckStyle =
    "QCheckBox { color: #c8d8e8; spacing: 4px; }"
    "QCheckBox::indicator { width: 12px; height: 12px; border: 1px solid #203040;"
    "  border-radius: 2px; background: #0f0f1a; }"
    "QCheckBox::indicator:checked { background: #00b4d8; }"
    // R-SPK-21 / R-SPK-24: a disabled box reads as greyed (D10).
    "QCheckBox:disabled { color: #506070; }"
    "QCheckBox::indicator:disabled:checked { background: #405060; }";

static const char* kLabelStyle =
    "QLabel { color: #c8d8e8; font-size: 12px; }"
    "QLabel:disabled { color: #506070; }";

static const char* kDimLabelStyle = "QLabel { color: #607080; font-size: 11px; }";

// The "Device details" fold: a flat arrow and dim text, like the mockup's
// summary line (audio-setup.html).
static const char* kDetailsToggleStyle =
    "QToolButton { color: #8aa8c0; font-size: 11px; border: none; padding: 2px 0; }"
    "QToolButton:disabled { color: #405060; }";

// R-AUD-08 to R-AUD-11, R-AUD-14: a role's trouble or Bluetooth note, amber
// like the mockup's (asio-setup-mockup.html).
static const char* kStateNoteStyle = "QLabel { color: #e0a030; font-size: 11px; }";
static const char* kEngineNoteStyle = "QLabel { color: #8aa8c0; font-size: 11px; }";

// Pill style for the negotiated-format readout.
static const char* kPillStyleOk =
    "QLabel {"
    "  background: #152530;"
    "  border: 1px solid #203040;"
    "  border-radius: 8px;"
    "  color: #80c8a0;"
    "  font-size: 10px;"
    "  padding: 2px 8px;"
    "}";

static const char* kPillStyleError =
    "QLabel {"
    "  background: #301520;"
    "  border: 1px solid #603040;"
    "  border-radius: 8px;"
    "  color: #e05060;"
    "  font-size: 10px;"
    "  padding: 2px 8px;"
    "}";

static const char* kPillStyleApplying =
    "QLabel {"
    "  background: #203040;"
    "  border: 1px solid #304050;"
    "  border-radius: 8px;"
    "  color: #c8d8e8;"
    "  font-size: 10px;"
    "  padding: 2px 8px;"
    "}";

// Sample rates offered in the combo. Include 384k for future-proofing;
// hardware that can't support it will fail on open and show the red pill.
static const QStringList kSampleRates = {
    QStringLiteral("44100"),
    QStringLiteral("48000"),
    QStringLiteral("88200"),
    QStringLiteral("96000"),
    QStringLiteral("176400"),
    QStringLiteral("192000"),
    QStringLiteral("384000"),
};

static const QStringList kBitDepths = {
    QStringLiteral("16"),
    QStringLiteral("24"),
    QStringLiteral("32"),
};

static const QStringList kChannels = {
    QStringLiteral("1"),
    QStringLiteral("2"),
};

// Buffer sizes in samples. Derived ms shown next to the combo.
static const QList<int> kBufferSizes = { 64, 128, 256, 512, 1024, 2048 };
// The input card matches the TX Input page's buffer range.
static const QList<int> kInputBufferSizes = { 64, 128, 256, 512, 1024, 2048, 4096, 8192 };

// R-R3-36: an item the card adds to keep a configured value that the list
// lacks (a device that is not present, a buffer size not offered) carries
// this role, so the next load removes it before adding its own.
static constexpr int kKeptEntryRole = Qt::UserRole + 1;

// Driver list items: Qt::UserRole is the engine key (empty on the "Older
// drivers" heading), kHostApiRole the older driver's host API.
static constexpr int kHostApiRole = Qt::UserRole + 2;
// Device list items: Qt::UserRole is the device name (empty for
// "(platform default)"), these the saved identity.
static constexpr int kDeviceIdRole = Qt::UserRole + 3;
static constexpr int kFirstChannelRole = Qt::UserRole + 4;
static constexpr int kBluetoothRole = Qt::UserRole + 5;
static constexpr int kPairedRole = Qt::UserRole + 6;

// R-AUD-15: the Delay list ("Automatic" saves DelayMs 0).
static constexpr std::array<int, 7> kDelayChoicesMs{0, 2, 3, 5, 10, 20, 40};

// R-AUD-15: the delay readout's refresh, and the catalogue's pick-up.
static constexpr int kRefreshIntervalMs = 1000;

// D10: the saved WASAPI option keys stay in the file, never rewritten.
static constexpr std::array<const char*, 3> kRetiredWasapiKeys{"ExclusiveMode", "EventDriven",
                                                               "BypassMixer"};

static void setItemEnabled(QComboBox* combo, int index, bool enabled)
{
    auto* model = qobject_cast<QStandardItemModel*>(combo->model());
    if (model == nullptr) {
        return;
    }
    if (QStandardItem* item = model->item(index)) {
        item->setEnabled(enabled);
    }
}

static void resetPopupAccessibilityCache(QComboBox* combo)
{
#if defined(Q_OS_MAC)
    if (QGuiApplication::platformName() == QStringLiteral("cocoa")
        && qVersion() == QStringLiteral("6.11.0")) {
        // Qt6.11 Cocoa expires promoted popup cells with its old native
        // rows (qcocoaaccessibilityelement.mm:219-226,257-267,342-362),
        // but QAccessibleTable retains their IDs and dereferences them on
        // RowsRemoved/RowsInserted (itemviews.cpp:645-741). Reset only the
        // accessibility cache before clearing or replacing retained entries.
        // The reset below now uses the model to invalidate indexes first.
        // An accessibility-only reset deletes still-valid cells. Qt's Cocoa
        // destruction notification can then promote a native cell and delete
        // that interface reentrantly (Qt 6.11 qaccessiblecache.cpp:193-208).
        // Reset the actual model first so its persistent cell indexes are
        // invalid before the view sends its accessibility ModelReset.
        auto* model = qobject_cast<QStandardItemModel*>(combo->model());
        if (model == nullptr) { return; }
        QSignalBlocker blocker(combo);
        const int selected = combo->currentIndex();
        std::vector<std::unique_ptr<QStandardItem>> items;
        items.reserve(model->rowCount());
        for (int row = 0; row < model->rowCount(); ++row) {
            items.emplace_back(model->item(row)->clone());
        }
        model->clear();
        for (std::unique_ptr<QStandardItem>& item : items) {
            model->appendRow(item.release());
        }
        combo->setCurrentIndex(selected);
    }
#else
    Q_UNUSED(combo);
#endif
}

static void removeKeptEntries(QComboBox* combo)
{
    resetPopupAccessibilityCache(combo);
    for (int i = combo->count() - 1; i >= 0; --i) {
        if (combo->itemData(i, kKeptEntryRole).toBool()) {
            combo->removeItem(i);
        }
    }
}

// Compute derived milliseconds label from samples + sample rate.
static QString bufferMs(int samples, int sampleRate)
{
    if (sampleRate <= 0) {
        return QStringLiteral("? ms");
    }
    const double ms = static_cast<double>(samples) / static_cast<double>(sampleRate) * 1000.0;
    return QStringLiteral("%1 ms").arg(ms, 0, 'f', 1);
}


// A catalogue with nothing in it: a card without an engine (or before the
// engine has its catalogue) lists "(platform default)" and the saved
// device through the same functions.
class EmptyAudioDeviceCatalog final : public IAudioDeviceCatalog {
public:
    QList<AudioBackendId> backends() const override { return {}; }
    bool backendRunning(AudioBackendId) const override { return false; }
    QList<AudioDeviceInfo> devices(AudioBackendId, AudioDeviceDirection) const override
    {
        return {};
    }
    std::optional<AudioDeviceInfo> defaultDevice(AudioBackendId,
                                                 AudioDeviceDirection) const override
    {
        return std::nullopt;
    }
    void rescanOlderDrivers() override {}
};

const IAudioDeviceCatalog& emptyCatalogue()
{
    static EmptyAudioDeviceCatalog catalogue;
    return catalogue;
}

// R-AUD-02's first native choice for this build, shown on the Driver list
// of a card whose engine has no device catalogue to ask.
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

} // namespace

// ---------------------------------------------------------------------------
// DeviceCard construction
// ---------------------------------------------------------------------------

DeviceCard::DeviceCard(const QString& prefix,
                       Role role,
                       bool enableCheckbox,
                       QWidget* parent)
    : QGroupBox(parent)
    , m_prefix(prefix)
    , m_role(role)
    , m_audioRole(roleForPrefix(prefix))
{
    setStyleSheet(QLatin1String(kGroupStyle));

    // R-AUD-15: the delay readout refreshes once a second while the card
    // follows an engine; the same tick picks up the engine's catalogue.
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(kRefreshIntervalMs);
    connect(m_refreshTimer, &QTimer::timeout, this, [this]() {
        attachCatalogue();
        refreshDelayNow();
    });

    buildLayout();

    // Enable checkbox (Headphones + VAX channels).  Inserted as the first
    // visible row of the card rather than using QGroupBox::setCheckable(true),
    // which clips the group title on platforms whose native checkable-title
    // indicator eats part of the title padding.  The checkbox persists via
    // audio/<prefix>/Enabled (separate from the 10-field AudioDeviceConfig
    // round-trip, since "enabled" is a bus-open decision, not a device param).
    if (enableCheckbox) {
        m_enableChk = new QCheckBox(QStringLiteral("Enabled"), this);
        m_enableChk->setStyleSheet(QLatin1String(kCheckStyle));
        auto* outer = qobject_cast<QVBoxLayout*>(layout());
        if (outer) {
            auto* headerRow = new QHBoxLayout;
            headerRow->setSpacing(4);
            headerRow->addWidget(m_enableChk);
            headerRow->addStretch(1);
            outer->insertLayout(0, headerRow);
        }
        connect(m_enableChk, &QCheckBox::toggled, this, [this](bool on) {
            if (m_suppressSignals) {
                return;
            }
            AppSettings::instance().setValue(
                m_prefix + QStringLiteral("/Enabled"),
                on ? QStringLiteral("True") : QStringLiteral("False"));
            AppSettings::instance().save();
            emit enabledChanged(on);
        });
        // R-SPK-21: a card greyed until Enabled follows the box, including
        // the loadFromSettings below (which suppresses only the signal).
        connect(m_enableChk, &QCheckBox::toggled, this, [this](bool) { updateBodyEnabled(); });
    }

    loadFromSettings();
}

std::optional<AudioRole> DeviceCard::roleForPrefix(const QString& prefix)
{
    static const std::array<std::pair<const char*, AudioRole>, 7> kPrefixes{{
        {"audio/Speakers", AudioRole::Speakers},
        {"audio/Headphones", AudioRole::Headphones},
        {"audio/TxInput", AudioRole::TxInput},
        {"audio/Vax1", AudioRole::Vax1},
        {"audio/Vax2", AudioRole::Vax2},
        {"audio/Vax3", AudioRole::Vax3},
        {"audio/Vax4", AudioRole::Vax4},
    }};
    for (const auto& [name, role] : kPrefixes) {
        if (prefix == QLatin1String(name)) {
            return role;
        }
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// buildLayout: the Device row, then the folded "Device details" section
// ---------------------------------------------------------------------------
// R-SPK-21 / D14: the Device row stays in front; Driver, Sample rate, Bit
// depth, Channels, Buffer size, Delay, Negotiated and the engine note fold
// under "Device details", folded by default. The Driver combo is created
// first, so it stays the card's first QComboBox child as before.
void DeviceCard::buildLayout()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 12, 8, 8);
    outer->setSpacing(4);

    // Rows a page adds above the Device row (Outputs' Volume row).
    m_aboveDeviceLayout = new QVBoxLayout;
    m_aboveDeviceLayout->setContentsMargins(0, 0, 0, 0);
    m_aboveDeviceLayout->setSpacing(4);
    outer->addLayout(m_aboveDeviceLayout);

    m_body = new QWidget(this);
    m_body->setObjectName(QStringLiteral("deviceCardBody"));
    auto* bodyLayout = new QVBoxLayout(m_body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(4);
    outer->addWidget(m_body);

    m_details = new QWidget(m_body);
    m_details->setObjectName(QStringLiteral("deviceDetails"));

    auto makeForm = []() {
        auto* form = new QFormLayout;
        form->setRowWrapPolicy(QFormLayout::DontWrapRows);
        form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        form->setHorizontalSpacing(8);
        form->setVerticalSpacing(4);
        return form;
    };
    auto makeLabel = [](const QString& text) -> QLabel* {
        auto* l = new QLabel(text);
        l->setStyleSheet(QLatin1String(kLabelStyle));
        return l;
    };

    // ── Driver (Device details), R-AUD-01 / D10 ──────────────────────────
    // Filled by populateDriverCombo() from the engine's device catalogue.
    m_driverApiCombo = new QComboBox(m_details);
    m_driverApiCombo->setObjectName(QStringLiteral("deviceDriverCombo"));
    m_driverApiCombo->setStyleSheet(QLatin1String(kComboStyle));

    // ── Device (always in front) ─────────────────────────────────────────
    auto* deviceForm = makeForm();
    m_deviceCombo = new QComboBox(m_body);
    m_deviceCombo->setStyleSheet(QLatin1String(kComboStyle));
    m_deviceCombo->setMinimumWidth(200);
    deviceForm->addRow(makeLabel(QStringLiteral("Device:")), m_deviceCombo);

    // ── TX-input extras (Input role only), in front ──────────────────────
    if (m_role == Role::Input) {
        m_monitorDuringTxChk = new QCheckBox(
            QStringLiteral("Monitor TX input during transmit"));
        m_monitorDuringTxChk->setStyleSheet(QLatin1String(kCheckStyle));
        m_monitorDuringTxChk->setToolTip(QStringLiteral(
            "Route microphone input through the monitor bus while transmitting"));

        m_toneCheckChk = new QCheckBox(
            QStringLiteral("Enable tone check (A-440 Hz burst on PTT)"));
        m_toneCheckChk->setStyleSheet(QLatin1String(kCheckStyle));
        m_toneCheckChk->setToolTip(QStringLiteral(
            "Inject a 440 Hz test tone to verify TX input routing on first PTT"));

        deviceForm->addRow(makeLabel(QString()), m_monitorDuringTxChk);
        deviceForm->addRow(makeLabel(QString()), m_toneCheckChk);
        UnbuiltFeatures::hideRowUnlessBuilt(m_monitorDuringTxChk,
                                           UnbuiltFeature::AudioMonitorTxInput, deviceForm);
        UnbuiltFeatures::hideRowUnlessBuilt(m_toneCheckChk,
                                           UnbuiltFeature::AudioToneCheck, deviceForm);
    }
    bodyLayout->addLayout(deviceForm);

    // R-AUD-08 to R-AUD-11, R-AUD-14: the role's state, right under the
    // Device row (empty, and so not shown, while the role plays as chosen).
    m_stateNote = new QLabel(m_body);
    m_stateNote->setObjectName(QStringLiteral("deviceStateNote"));
    m_stateNote->setStyleSheet(QLatin1String(kStateNoteStyle));
    m_stateNote->setWordWrap(true);
    m_stateNote->setVisible(false);
    bodyLayout->addWidget(m_stateNote);

    // Rows a page adds below the Device row (a note).
    m_belowDeviceLayout = new QVBoxLayout;
    m_belowDeviceLayout->setContentsMargins(0, 0, 0, 0);
    m_belowDeviceLayout->setSpacing(4);
    bodyLayout->addLayout(m_belowDeviceLayout);

    // ── "Device details" fold ────────────────────────────────────────────
    m_detailsToggle = new QToolButton(m_body);
    m_detailsToggle->setObjectName(QStringLiteral("deviceDetailsToggle"));
    m_detailsToggle->setText(QStringLiteral("Device details"));
    m_detailsToggle->setCheckable(true);
    m_detailsToggle->setChecked(false);
    m_detailsToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_detailsToggle->setArrowType(Qt::RightArrow);
    m_detailsToggle->setAutoRaise(true);
    m_detailsToggle->setStyleSheet(QLatin1String(kDetailsToggleStyle));
    bodyLayout->addWidget(m_detailsToggle);

    auto* detailsForm = makeForm();
    m_details->setLayout(detailsForm);
    detailsForm->setContentsMargins(12, 0, 0, 0);
    detailsForm->addRow(makeLabel(QStringLiteral("Driver:")), m_driverApiCombo);

    // Sample rate + Auto-match checkbox
    {
        auto* srRow = new QHBoxLayout;
        srRow->setSpacing(6);
        m_sampleRateCombo = new QComboBox;
        m_sampleRateCombo->setStyleSheet(QLatin1String(kComboStyle));
        for (const QString& r : kSampleRates) {
            m_sampleRateCombo->addItem(r + QStringLiteral(" Hz"), r.toInt());
        }
        m_autoMatchSampleRate = new QCheckBox(QStringLiteral("Auto-match"));
        m_autoMatchSampleRate->setStyleSheet(QLatin1String(kCheckStyle));
        m_autoMatchSampleRate->setToolTip(QStringLiteral(
            "Use the sample rate the device reports as its default"));
        srRow->addWidget(m_sampleRateCombo);
        srRow->addWidget(m_autoMatchSampleRate);
        srRow->addStretch();
        detailsForm->addRow(makeLabel(QStringLiteral("Sample rate:")), srRow);
        UnbuiltFeatures::hideUnlessBuilt(m_autoMatchSampleRate, UnbuiltFeature::AudioAutoMatch);
    }

    // Bit depth
    m_bitDepthCombo = new QComboBox;
    m_bitDepthCombo->setStyleSheet(QLatin1String(kComboStyle));
    for (const QString& d : kBitDepths) {
        m_bitDepthCombo->addItem(d + QStringLiteral(" bit"), d.toInt());
    }
    detailsForm->addRow(makeLabel(QStringLiteral("Bit depth:")), m_bitDepthCombo);
    UnbuiltFeatures::hideRowUnlessBuilt(m_bitDepthCombo, UnbuiltFeature::AudioBitDepth,
                                        detailsForm);

    // Channels
    m_channelsCombo = new QComboBox;
    m_channelsCombo->setStyleSheet(QLatin1String(kComboStyle));
    for (const QString& c : kChannels) {
        m_channelsCombo->addItem(c == QStringLiteral("1")
                                     ? QStringLiteral("1 (Mono)")
                                     : QStringLiteral("2 (Stereo)"),
                                 c.toInt());
    }
    detailsForm->addRow(makeLabel(QStringLiteral("Channels:")), m_channelsCombo);

    // Buffer size + derived-ms readout
    {
        auto* bufRow = new QHBoxLayout;
        bufRow->setSpacing(6);
        m_bufferSizeCombo = new QComboBox;
        m_bufferSizeCombo->setStyleSheet(QLatin1String(kComboStyle));
        for (int sz : (m_role == Role::Input ? kInputBufferSizes : kBufferSizes)) {
            m_bufferSizeCombo->addItem(QStringLiteral("%1 samples").arg(sz),
                                       QVariant::fromValue(sz));
        }
        m_bufferMsLabel = new QLabel;
        m_bufferMsLabel->setStyleSheet(QLatin1String(kDimLabelStyle));
        m_bufferMsLabel->setMinimumWidth(50);
        bufRow->addWidget(m_bufferSizeCombo);
        bufRow->addWidget(m_bufferMsLabel);
        bufRow->addStretch();
        detailsForm->addRow(makeLabel(QStringLiteral("Buffer size:")), bufRow);
    }

    // Delay (R-AUD-15): the clock-matched buffer's size, and the delay now.
    {
        auto* delayRow = new QHBoxLayout;
        delayRow->setSpacing(6);
        m_delayCombo = new QComboBox;
        m_delayCombo->setObjectName(QStringLiteral("deviceDelayCombo"));
        m_delayCombo->setStyleSheet(QLatin1String(kComboStyle));
        for (int ms : kDelayChoicesMs) {
            m_delayCombo->addItem(ms == 0 ? QStringLiteral("Automatic")
                                          : QStringLiteral("%1 ms").arg(ms),
                                  QVariant::fromValue(ms));
        }
        m_delayNow = new QLabel(QStringLiteral("Now -- ms"));
        m_delayNow->setObjectName(QStringLiteral("deviceDelayNow"));
        m_delayNow->setStyleSheet(QLatin1String(kDimLabelStyle));
        delayRow->addWidget(m_delayCombo);
        delayRow->addWidget(m_delayNow);
        delayRow->addStretch();
        detailsForm->addRow(makeLabel(QStringLiteral("Delay:")), delayRow);
    }

    // Negotiated-format pill
    {
        auto* pillRow = new QHBoxLayout;
        pillRow->setSpacing(4);
        m_negotiatedPill = new QLabel(QStringLiteral("(not applied)"));
        m_negotiatedPill->setStyleSheet(QLatin1String(kPillStyleApplying));
        pillRow->addWidget(m_negotiatedPill);
        pillRow->addStretch();
        auto* pillLbl = new QLabel(QStringLiteral("Negotiated:"));
        pillLbl->setStyleSheet(QLatin1String(kDimLabelStyle));
        detailsForm->addRow(pillLbl, pillRow);
    }

    // R-AUD-16: what the picked driver means. One short line: a
    // word-wrapped label in a form row is clipped to one line's height.
    m_engineNote = new QLabel;
    m_engineNote->setObjectName(QStringLiteral("engineNote"));
    m_engineNote->setStyleSheet(QLatin1String(kEngineNoteStyle));
    m_engineNote->setWordWrap(false);
    detailsForm->addRow(makeLabel(QString()), m_engineNote);

    bodyLayout->addWidget(m_details);
    m_details->setVisible(false);

    connect(m_detailsToggle, &QToolButton::toggled, this, [this](bool on) {
        m_detailsToggle->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
        m_details->setVisible(on);
    });

    // `outer` was already installed as this widget's layout by the
    // `new QVBoxLayout(this)` parent-ctor at the top of this function; a
    // second `setLayout(outer)` triggers the QGroupBox "already has a layout"
    // runtime warning (×7 on Settings open — fired 3 times from AudioDevicesPage
    // and 4 times from AudioVaxPage's VaxChannelCards before #272).

    // ── Wire all controls to the commit slot ────────────────────────────
    // Use event-filter on QComboBox / QCheckBox inside the card so wheel
    // events inside a scroll area don't leak into control-changes.
    auto connectCombo = [this](QComboBox* combo) {
        if (!combo) { return; }
        // Block wheel events on combos inside scroll areas.
        combo->installEventFilter(this);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &DeviceCard::onAnyControlChanged);
    };
    connectCombo(m_sampleRateCombo);
    connectCombo(m_bitDepthCombo);
    connectCombo(m_channelsCombo);
    connectCombo(m_delayCombo);
    // Buffer-size uses a 200 ms intra-control debounce (addendum §2.1).
    // Other combos fire immediately.
    if (m_bufferSizeCombo) {
        m_bufferSizeCombo->installEventFilter(this);
        m_bufferSizeDebounceTimer = new QTimer(this);
        m_bufferSizeDebounceTimer->setSingleShot(true);
        m_bufferSizeDebounceTimer->setInterval(200);
        connect(m_bufferSizeDebounceTimer, &QTimer::timeout,
                this, &DeviceCard::onAnyControlChanged);
        connect(m_bufferSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) {
                    if (m_suppressSignals) { return; }
                    m_bufferSizeDebounceTimer->start();
                });
    }

    // A Driver pick repopulates the Device list for that driver, then
    // commits once; a Device pick commits the picked device.
    m_driverApiCombo->installEventFilter(this);
    connect(m_driverApiCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { onDriverPicked(); });
    m_deviceCombo->installEventFilter(this);
    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { onDevicePicked(); });

    auto connectCheck = [this](QCheckBox* chk) {
        if (!chk) { return; }
        connect(chk, &QCheckBox::toggled,
                this, &DeviceCard::onAnyControlChanged);
    };
    connectCheck(m_autoMatchSampleRate);
    if (m_monitorDuringTxChk) { connectCheck(m_monitorDuringTxChk); }
    if (m_toneCheckChk)       { connectCheck(m_toneCheckChk); }

    // Buffer-size or sample-rate change → update derived-ms label.
    connect(m_bufferSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateBufferMsLabel(); });
    connect(m_sampleRateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateBufferMsLabel(); });
}

// ---------------------------------------------------------------------------
// Device details, page rows, Enabled greying, rescan
// ---------------------------------------------------------------------------
bool DeviceCard::detailsExpanded() const
{
    return m_detailsToggle != nullptr && m_detailsToggle->isChecked();
}

void DeviceCard::setDetailsExpanded(bool expanded)
{
    if (m_detailsToggle != nullptr) {
        m_detailsToggle->setChecked(expanded);
    }
}

void DeviceCard::addAboveDevice(QWidget* widget)
{
    if (widget != nullptr) {
        m_aboveDeviceLayout->addWidget(widget);
    }
}

void DeviceCard::addBelowDevice(QWidget* widget)
{
    if (widget != nullptr) {
        m_belowDeviceLayout->addWidget(widget);
    }
}

void DeviceCard::setGreyedUntilEnabled(bool greyed)
{
    m_greyedUntilEnabled = greyed;
    updateBodyEnabled();
}

void DeviceCard::updateBodyEnabled()
{
    if (m_body == nullptr) {
        return;
    }
    m_body->setEnabled(!m_greyedUntilEnabled || isCheckboxEnabled());
}

void DeviceCard::rescanDevices()
{
    // Keeps the selection: a configured device that has gone stays as
    // "<name> (not connected)".
    const bool suppressed = m_suppressSignals;
    m_suppressSignals = true;
    populateDriverCombo();
    populateDeviceCombo();
    m_suppressSignals = suppressed;
    refreshStatus();
}

int DeviceCard::deviceCount() const
{
    int count = 0;
    for (int i = 1; i < m_deviceCombo->count(); ++i) {
        if (!m_deviceCombo->itemData(i, kKeptEntryRole).toBool()
            && m_deviceCombo->itemData(i, kDeviceIdRole).toString()
                   != QLatin1String(kAudioDeviceNone)) {
            ++count;
        }
    }
    return count;
}

// ---------------------------------------------------------------------------
// The engine, its catalogue and its role's status
// ---------------------------------------------------------------------------
void DeviceCard::setAudioEngine(AudioEngine* engine)
{
    if (m_engine == engine) {
        return;
    }
    if (m_engine) {
        disconnect(m_engine, nullptr, this, nullptr);
    }
    if (m_catalogue) {
        disconnect(m_catalogue, nullptr, this, nullptr);
    }
    m_catalogue = nullptr;
    m_engine = engine;
    m_status = AudioRoleStatus{};
    if (!m_engine) {
        m_refreshTimer->stop();
        refreshStatus();
        return;
    }
    // R-AUD-12: the card shows the same status the header's PC tooltip
    // reads, so a default change updates both at once.
    connect(m_engine, &AudioEngine::roleStatusChanged, this,
            [this](AudioRole role, const AudioRoleStatus& status) {
                if (!m_audioRole || role != *m_audioRole) {
                    return;
                }
                m_status = status;
                attachCatalogue();
                refreshStatus();
            });
    if (m_audioRole) {
        m_status = m_engine->roleStatus(*m_audioRole);
    }
    m_refreshTimer->start();
    attachCatalogue();
    refreshStatus();
}

void DeviceCard::attachCatalogue()
{
    if (m_catalogue || !m_engine) {
        return;
    }
    IAudioDeviceCatalog* catalogue = m_engine->catalogue();
    if (catalogue == nullptr) {
        return;
    }
    m_catalogue = catalogue;
    // R-AUD-05: the engine migrated the saved keys as it built the
    // catalogue, so the card takes the saved choice again.  Every edit
    // saves at once, so the file holds nothing the card has not shown.
    takeSavedChoice(AudioDeviceConfig::loadFromSettings(m_prefix));
    // R-AUD-03: a device added or removed shows at once, the selection kept.
    connect(catalogue, &IAudioDeviceCatalog::devicesChanged, this,
            [this]() { rescanDevices(); });
    connect(catalogue, &IAudioDeviceCatalog::defaultChanged, this,
            [this](AudioDeviceDirection) { refreshStatus(); });
    rescanDevices();
    updateEngineNote();
}

void DeviceCard::takeSavedChoice(const AudioDeviceConfig& saved)
{
    m_loaded = saved;
    m_driverEngine = saved.engine;
    m_driverHostApi =
        saved.engine == AudioEngineKind::PortAudio ? saved.driverApi : QString();
    m_selection = Selection{saved.deviceId, saved.deviceName, std::max(1, saved.firstChannel)};
}

AudioEngineKind DeviceCard::selectedEngine() const
{
    if (m_driverEngine) {
        return *m_driverEngine;
    }
    if (m_engine && m_catalogue) {
        return m_engine->defaultEngine();
    }
    return buildDefaultEngine();
}

void DeviceCard::refreshStatus()
{
    QString note;
    if (m_engine && m_audioRole) {
        note = audioRoleNote(*m_audioRole, m_status);
        // R-AUD-14: a Bluetooth mic picked by name says what it costs.
        const int idx = m_deviceCombo->currentIndex();
        if (note.isEmpty() && *m_audioRole == AudioRole::TxInput && idx > 0
            && m_deviceCombo->itemData(idx, kBluetoothRole).toBool()
            && !m_deviceCombo->itemData(idx, kKeptEntryRole).toBool()) {
            note = bluetoothMicNote(m_deviceCombo->itemData(idx).toString());
        }
    }
    if (m_stateNote->text() != note) {
        m_stateNote->setText(note);
    }
    m_stateNote->setVisible(!note.isEmpty());
    refreshDelayNow();
    renderPill();
}

void DeviceCard::refreshDelayNow()
{
    QString text = audioDelayLine(m_audioRole.value_or(AudioRole::Speakers), AudioDelayParts{},
                                  QString());
    if (m_engine && m_audioRole
        && (m_status.state == AudioRoleState::Playing
            || m_status.state == AudioRoleState::PlayingOnDefault)) {
        // The device actually playing, the system default included.
        text = audioDelayLine(*m_audioRole, m_engine->delayParts(*m_audioRole),
                              m_status.playingName);
    }
    if (m_delayNow->text() != text) {
        m_delayNow->setText(text);
    }
}

void DeviceCard::updateEngineNote()
{
    QString note;
    switch (selectedEngine()) {
    case AudioEngineKind::WindowsExclusive:
        note = QStringLiteral("Other apps cannot play through this device while NereusSDR has it.");
        break;
    case AudioEngineKind::PortAudio:
        note = QStringLiteral("An older driver: more delay, and its list updates only with "
                              "Rescan devices.");
        break;
    case AudioEngineKind::CoreAudio:
    case AudioEngineKind::WindowsShared:
    case AudioEngineKind::Asio:
    case AudioEngineKind::PipeWire:
    case AudioEngineKind::PulseAudio:
    case AudioEngineKind::AlsaDirect:
        break;
    }
    m_engineNote->setText(note);
    m_engineNote->setVisible(!note.isEmpty());
}

// ---------------------------------------------------------------------------
// populateDriverCombo: the Driver list (R-AUD-01, D10)
// ---------------------------------------------------------------------------
void DeviceCard::populateDriverCombo()
{
    QList<AudioDriverEntry> entries = audioDriverEntries(
        m_catalogue ? static_cast<const IAudioDeviceCatalog&>(*m_catalogue) : emptyCatalogue(),
        false);
    const AudioEngineKind engine = selectedEngine();
    const bool older = engine == AudioEngineKind::PortAudio;
    auto isChoice = [&](const AudioDriverEntry& e) {
        return e.engine && *e.engine == engine && (!older || e.hostApi == m_driverHostApi);
    };
    bool listed = false;
    for (const AudioDriverEntry& e : entries) {
        listed = listed || isChoice(e);
    }
    if (!listed) {
        // The card's own choice always shows: a saved driver this list does
        // not offer, or the build's default while the lists are not ready.
        AudioDriverEntry own;
        own.engine = engine;
        own.hostApi = older ? m_driverHostApi : QString();
        own.label = older && !m_driverHostApi.isEmpty() ? olderDriverDisplayName(m_driverHostApi)
                                                        : audioEngineLabel(engine);
        if (!m_catalogue) {
            own.enabled = false;
            own.disabledReason = QStringLiteral("The device lists are not ready.");
        }
        entries.append(own);
    }

    QSignalBlocker blocker(m_driverApiCombo);
    resetPopupAccessibilityCache(m_driverApiCombo);
    m_driverApiCombo->clear();
    int current = -1;
    int choices = 0;
    for (int i = 0; i < entries.size(); ++i) {
        const AudioDriverEntry& e = entries.at(i);
        m_driverApiCombo->addItem(e.label);
        m_driverApiCombo->setItemData(i, e.engine ? audioEngineKey(*e.engine) : QString());
        m_driverApiCombo->setItemData(i, e.hostApi, kHostApiRole);
        if (!e.enabled) {
            setItemEnabled(m_driverApiCombo, i, false);
            if (!e.disabledReason.isEmpty()) {
                m_driverApiCombo->setItemData(i, e.disabledReason, Qt::ToolTipRole);
            }
        } else if (e.engine) {
            ++choices;
        }
        if (current < 0 && isChoice(e)) {
            current = i;
        }
    }
    m_driverApiCombo->setCurrentIndex(current);
    // Disabled, never hidden: a list with nothing else to pick is greyed
    // with its reason (the Mac, the Core, lists not ready).
    const bool currentEnabled = current >= 0 && entries.at(current).enabled;
    const bool pickable = choices > 1 || (choices == 1 && !currentEnabled);
    m_driverApiCombo->setEnabled(pickable);
    const QString reason = current >= 0 ? entries.at(current).disabledReason : QString();
    m_driverApiCombo->setToolTip(currentEnabled ? QString() : reason);

    // R-AUD-15 / R-AUD-05: DelayMs is saved with the engine's keys, which a
    // choice the engine has not migrated cannot write. Until the lists are
    // ready the Delay is greyed with that reason, never a change that is lost.
    if (m_delayCombo) {
        const bool delaySaves = m_catalogue || m_loaded.engine.has_value();
        m_delayCombo->setEnabled(delaySaves);
        m_delayCombo->setToolTip(delaySaves ? QString()
                                            : QStringLiteral("The device lists are not ready."));
    }
}

void DeviceCard::onDriverPicked()
{
    if (m_suppressSignals) {
        return;
    }
    const int idx = m_driverApiCombo->currentIndex();
    const std::optional<AudioEngineKind> engine =
        audioEngineFromKey(m_driverApiCombo->itemData(idx).toString());
    if (!engine) {
        return;   // the "Older drivers" heading is never picked
    }
    m_driverEngine = engine;
    m_driverHostApi = m_driverApiCombo->itemData(idx, kHostApiRole).toString();
    m_driverApiCombo->setToolTip(QString());

    // R-R3-36: the device stays the one chosen, by name, never a
    // substitute: the new driver's device of that name when it lists one,
    // else "<name> (not connected)". A device's id belongs to its driver.
    if (m_selection.deviceId != QLatin1String(kAudioDeviceNone)) {
        m_selection.deviceId.clear();
    }
    {
        const bool suppressed = m_suppressSignals;
        m_suppressSignals = true;
        populateDeviceCombo();
        m_suppressSignals = suppressed;
    }
    updateEngineNote();
    refreshStatus();
    onAnyControlChanged();
}

// ---------------------------------------------------------------------------
// populateDeviceCombo: the Device list for the card's driver (R-AUD-03)
// ---------------------------------------------------------------------------
QString DeviceCard::deviceNameForId(const QString& deviceId) const
{
    if (!m_catalogue || deviceId.isEmpty()) {
        return {};
    }
    const AudioDeviceDirection direction =
        m_role == Role::Output ? AudioDeviceDirection::Output : AudioDeviceDirection::Input;
    for (const AudioDeviceInfo& info :
         m_catalogue->devices(audioBackendFor(selectedEngine()), direction)) {
        if (info.id == deviceId) {
            return info.name;
        }
    }
    return {};
}

void DeviceCard::populateDeviceCombo()
{
    const AudioEngineKind engine = selectedEngine();
    AudioDeviceConfig saved;
    saved.engine = engine;
    saved.driverApi = engine == AudioEngineKind::PortAudio ? m_driverHostApi : QString();
    saved.deviceId = m_selection.deviceId;
    saved.deviceName = m_selection.deviceName;
    saved.firstChannel = m_selection.firstChannel;
    const QList<AudioDeviceEntry> entries = audioDeviceEntries(
        m_catalogue ? static_cast<const IAudioDeviceCatalog&>(*m_catalogue) : emptyCatalogue(),
        engine, saved.driverApi,
        m_role == Role::Output ? AudioDeviceDirection::Output : AudioDeviceDirection::Input, saved);

    QSignalBlocker blocker(m_deviceCombo);
    resetPopupAccessibilityCache(m_deviceCombo);
    m_deviceCombo->clear();
    for (int i = 0; i < entries.size(); ++i) {
        const AudioDeviceEntry& e = entries.at(i);
        QString name;
        bool kept = false;
        if (i == 0) {
            name.clear();   // "(platform default)"
        } else if (e.deviceId == QLatin1String(kAudioDeviceNone)) {
            name = QString::fromLatin1(kAudioDeviceNone);
        } else {
            name = deviceNameForId(e.deviceId);
            if (name.isEmpty()) {
                // The saved device, missing: it keeps its saved name.
                name = m_selection.deviceName.isEmpty() ? e.deviceId : m_selection.deviceName;
                kept = true;
            }
        }
        m_deviceCombo->addItem(e.label, QVariant::fromValue(name));
        m_deviceCombo->setItemData(i, e.deviceId, kDeviceIdRole);
        m_deviceCombo->setItemData(i, e.pair.firstChannel, kFirstChannelRole);
        m_deviceCombo->setItemData(i, e.bluetooth, kBluetoothRole);
        m_deviceCombo->setItemData(i, !e.group.isEmpty(), kPairedRole);
        if (kept) {
            m_deviceCombo->setItemData(i, true, kKeptEntryRole);
        }
    }
    selectDevice();
}

// ---------------------------------------------------------------------------
// selectDevice: select the configured device, never a substitute
// ---------------------------------------------------------------------------
// R-R3-36 / R-AUD-08: a named device that is not present is kept as
// "<name> (not connected)", so the card shows it and currentConfig() saves
// the same identity back. Falling to "(platform default)" would silently
// switch the device on the next edit.
void DeviceCard::selectDevice()
{
    int idx = 0;
    if (!m_selection.deviceId.isEmpty() || !m_selection.deviceName.isEmpty()) {
        auto pairMatches = [this](int i) {
            return !m_deviceCombo->itemData(i, kPairedRole).toBool()
                || m_deviceCombo->itemData(i, kFirstChannelRole).toInt()
                       == m_selection.firstChannel;
        };
        int byName = -1;
        for (int i = 1; i < m_deviceCombo->count() && idx == 0; ++i) {
            if (!pairMatches(i)) {
                continue;
            }
            const QString id = m_deviceCombo->itemData(i, kDeviceIdRole).toString();
            if (!m_selection.deviceId.isEmpty() && id == m_selection.deviceId) {
                idx = i;
            } else if (byName < 0 && !m_selection.deviceName.isEmpty()
                       && m_deviceCombo->itemData(i).toString() == m_selection.deviceName) {
                byName = i;
            }
        }
        if (idx == 0 && byName > 0) {
            // Found by name: the listed device's id is the one saved from
            // now on, as the stream supervisor saves it.
            idx = byName;
            if (m_catalogue && !m_deviceCombo->itemData(idx, kKeptEntryRole).toBool()) {
                m_selection.deviceId = m_deviceCombo->itemData(idx, kDeviceIdRole).toString();
            }
        }
    }
    m_deviceCombo->setCurrentIndex(idx);
}

void DeviceCard::onDevicePicked()
{
    if (m_suppressSignals) {
        return;
    }
    const int idx = m_deviceCombo->currentIndex();
    if (idx < 0) {
        return;
    }
    m_selection.deviceId = m_deviceCombo->itemData(idx, kDeviceIdRole).toString();
    m_selection.deviceName = m_deviceCombo->itemData(idx).toString();
    m_selection.firstChannel =
        std::max(1, m_deviceCombo->itemData(idx, kFirstChannelRole).toInt());
    refreshStatus();
    onAnyControlChanged();
}

// ---------------------------------------------------------------------------
// updateBufferMsLabel — recompute the derived milliseconds readout
// ---------------------------------------------------------------------------
void DeviceCard::updateBufferMsLabel()
{
    if (!m_bufferMsLabel) {
        return;
    }
    const int sz = m_bufferSizeCombo ? m_bufferSizeCombo->currentData().toInt() : 256;
    const int sr = m_sampleRateCombo ? m_sampleRateCombo->currentData().toInt() : 48000;
    m_bufferMsLabel->setText(bufferMs(sz, sr));
}

// ---------------------------------------------------------------------------
// currentConfig
//
// Note: autoMatchSampleRate, monitorDuringTx, and toneCheck are session-only
// UI helpers by design — not part of AudioDeviceConfig's 10 persisted fields
// and not round-tripped through loadFromSettings/saveToSettings.  If future
// requirements change, extend AudioDeviceConfig.
// ---------------------------------------------------------------------------
AudioDeviceConfig DeviceCard::currentConfig() const
{
    // Fields the card does not edit (the mic's channel, the retired WASAPI
    // options) carry through from the saved config.
    AudioDeviceConfig cfg = m_loaded;

    // deviceName: empty string from "(platform default)" entry maps to
    // AudioDeviceConfig empty deviceName → makeBus treats as platform default.
    cfg.deviceName = m_selection.deviceName;
    cfg.deviceId = m_selection.deviceId;
    cfg.firstChannel = m_selection.firstChannel;

    // R-AUD-04: a pick from the engine's lists saves the engine with it
    // (Engine, DeviceId, DeviceName, FirstChannel). Without the lists the
    // saved engine stays as it was: the engine has not migrated the keys
    // (R-AUD-05), and a save must never mark them migrated.
    if (m_catalogue) {
        cfg.engine = selectedEngine();
        // As AudioEngine::openRole sets them: the host API on older
        // drivers, empty on the native engines; the index is left to it.
        cfg.driverApi = *cfg.engine == AudioEngineKind::PortAudio ? m_driverHostApi : QString();
        cfg.hostApiIndex = -1;
    }

    // Auto-match is a UI preference (session-only) meant to signal "use the
    // device's own default sample rate".  That resolution isn't wired yet —
    // the engine does NOT treat sampleRate=0 as "device default" and would
    // pass it straight to makeBus → stream-open fails at 0 Hz.  Until the
    // PortAudio device-enumeration is threaded through this card, always
    // emit the combo's current rate so the bus always opens cleanly; the
    // auto-match checkbox state remains session-only UI.
    // TODO(sub-phase-12-automatch-resolve): when device-default-rate
    // lookup lands, set cfg.sampleRate to the device default here when
    // auto-match is checked.
    cfg.sampleRate = m_sampleRateCombo
        ? m_sampleRateCombo->currentData().toInt()
        : 48000;

    cfg.bitDepth      = m_bitDepthCombo  ? m_bitDepthCombo->currentData().toInt()  : 32;
    cfg.channels      = m_channelsCombo  ? m_channelsCombo->currentData().toInt()  : 2;
    cfg.bufferSamples = m_bufferSizeCombo? m_bufferSizeCombo->currentData().toInt(): 256;
    cfg.delayMs       = m_delayCombo     ? m_delayCombo->currentData().toInt()     : 0;

    cfg.manualLatencyMs = 0;  // Not exposed in this card; reserved for Advanced page.

    return cfg;
}

// ---------------------------------------------------------------------------
// updateNegotiatedPill
// ---------------------------------------------------------------------------
void DeviceCard::updateNegotiatedPill(const AudioDeviceConfig& negotiated,
                                      const QString& errorString)
{
    m_applying = false;
    m_negotiated = negotiated;
    m_negotiatedError = errorString;
    renderPill();
}

void DeviceCard::renderPill()
{
    if (!m_negotiatedPill) {
        return;
    }
    if (m_applying) {
        // Show "APPLYING" pill while the engine is rebuilding the bus.
        m_negotiatedPill->setStyleSheet(QLatin1String(kPillStyleApplying));
        m_negotiatedPill->setText(QStringLiteral("APPLYING…"));
        return;
    }
    if (!m_negotiatedError.isEmpty()) {
        // Red pill — driver rejected the config.
        m_negotiatedPill->setStyleSheet(QLatin1String(kPillStyleError));
        m_negotiatedPill->setText(QStringLiteral("Error: ") + m_negotiatedError);
        return;
    }
    if (!m_negotiated) {
        m_negotiatedPill->setStyleSheet(QLatin1String(kPillStyleApplying));
        m_negotiatedPill->setText(QStringLiteral("(not applied)"));
        return;
    }

    // Green pill: show negotiated format.  With an engine, the name is
    // the device the role plays on now, from the role's status.
    const AudioDeviceConfig& negotiated = *m_negotiated;
    m_negotiatedPill->setStyleSheet(QLatin1String(kPillStyleOk));
    QString name = negotiated.deviceName.isEmpty()
        ? QStringLiteral("(default)")
        : negotiated.deviceName;
    if (m_engine && !m_status.playingName.isEmpty()
        && (m_status.state == AudioRoleState::Playing
            || m_status.state == AudioRoleState::PlayingOnDefault)) {
        name = m_status.playingName;
    }
    const QString sr = negotiated.sampleRate == 0
        ? QStringLiteral("auto")
        : QStringLiteral("%1 Hz").arg(negotiated.sampleRate);
    m_negotiatedPill->setText(
        QStringLiteral("%1 · %2 · %3 ch · %4 samples")
            .arg(name)
            .arg(sr)
            .arg(negotiated.channels)
            .arg(negotiated.bufferSamples));
}

// ---------------------------------------------------------------------------
// loadFromSettings — seed controls from persisted state
// ---------------------------------------------------------------------------
void DeviceCard::loadFromSettings()
{
    const AudioDeviceConfig cfg =
        AudioDeviceConfig::loadFromSettings(m_prefix);
    m_loaded = cfg;

    m_suppressSignals = true;

    // Driver and device: the saved choice. A choice saved before engines
    // were named shows on the default engine until the engine migrates it
    // (R-AUD-05), when it builds its catalogue. A configured device that
    // is not present stays selected.
    takeSavedChoice(cfg);
    populateDriverCombo();
    populateDeviceCombo();

    // Sample rate.
    if (m_sampleRateCombo) {
        const int idx = m_sampleRateCombo->findData(
            QVariant::fromValue(cfg.sampleRate));
        m_sampleRateCombo->setCurrentIndex(idx >= 0 ? idx : 1); // default 48000
    }

    // Bit depth.
    if (m_bitDepthCombo) {
        const int idx = m_bitDepthCombo->findData(
            QVariant::fromValue(cfg.bitDepth));
        m_bitDepthCombo->setCurrentIndex(idx >= 0 ? idx : 2); // default 32-bit
    }

    // Channels.
    if (m_channelsCombo) {
        const int idx = m_channelsCombo->findData(
            QVariant::fromValue(cfg.channels));
        m_channelsCombo->setCurrentIndex(idx >= 0 ? idx : 1); // default stereo
    }

    // Buffer size.
    if (m_bufferSizeCombo) {
        {
            QSignalBlocker blocker(m_bufferSizeCombo);
            removeKeptEntries(m_bufferSizeCombo);
        }
        int idx = m_bufferSizeCombo->findData(
            QVariant::fromValue(cfg.bufferSamples));
        if (idx < 0 && cfg.bufferSamples > 0) {
            // R-R3-36: keep a configured size the list lacks, in order,
            // rather than falling to 256 and saving that on the next edit.
            int insertAt = 0;
            while (insertAt < m_bufferSizeCombo->count()
                   && m_bufferSizeCombo->itemData(insertAt).toInt() < cfg.bufferSamples) {
                ++insertAt;
            }
            m_bufferSizeCombo->insertItem(
                insertAt, QStringLiteral("%1 samples").arg(cfg.bufferSamples),
                QVariant::fromValue(cfg.bufferSamples));
            m_bufferSizeCombo->setItemData(insertAt, true, kKeptEntryRole);
            idx = insertAt;
        }
        m_bufferSizeCombo->setCurrentIndex(idx >= 0 ? idx : 2); // default 256
        // Update the derived-ms label.
        if (m_bufferMsLabel) {
            const int sr = m_sampleRateCombo
                ? m_sampleRateCombo->currentData().toInt()
                : 48000;
            m_bufferMsLabel->setText(bufferMs(cfg.bufferSamples, sr));
        }
    }

    // Delay (R-AUD-15). A saved value the list lacks is kept, in order, as
    // the buffer size is.
    if (m_delayCombo) {
        {
            QSignalBlocker blocker(m_delayCombo);
            removeKeptEntries(m_delayCombo);
        }
        int idx = m_delayCombo->findData(QVariant::fromValue(cfg.delayMs));
        if (idx < 0 && cfg.delayMs > 0) {
            int insertAt = 1;
            while (insertAt < m_delayCombo->count()
                   && m_delayCombo->itemData(insertAt).toInt() < cfg.delayMs) {
                ++insertAt;
            }
            m_delayCombo->insertItem(insertAt, QStringLiteral("%1 ms").arg(cfg.delayMs),
                                     QVariant::fromValue(cfg.delayMs));
            m_delayCombo->setItemData(insertAt, true, kKeptEntryRole);
            idx = insertAt;
        }
        m_delayCombo->setCurrentIndex(idx >= 0 ? idx : 0);   // default Automatic
    }

    // Enable checkbox (Headphones + VAX channels) — restored from
    // audio/<prefix>/Enabled.  Default is unchecked on fresh install so the
    // bus only opens after explicit user consent.
    if (m_enableChk) {
        const bool on = AppSettings::instance()
                            .value(m_prefix + QStringLiteral("/Enabled"),
                                   QStringLiteral("False"))
                            .toString() == QStringLiteral("True");
        m_enableChk->setChecked(on);
    }

    updateEngineNote();
    refreshStatus();
    m_suppressSignals = false;
}

// ---------------------------------------------------------------------------
// onAnyControlChanged — commit on every control edit
// ---------------------------------------------------------------------------
void DeviceCard::onAnyControlChanged()
{
    if (m_suppressSignals) {
        return;
    }

    const AudioDeviceConfig cfg = currentConfig();

    // Persist to AppSettings immediately.  D10: the retired WASAPI option
    // keys stay in the file as they were (readable for migration) and are
    // never written again.
    auto& settings = AppSettings::instance();
    std::array<std::optional<QString>, kRetiredWasapiKeys.size()> retired;
    for (std::size_t i = 0; i < kRetiredWasapiKeys.size(); ++i) {
        const QString key = m_prefix + QLatin1Char('/') + QLatin1String(kRetiredWasapiKeys[i]);
        if (settings.contains(key)) {
            retired[i] = settings.value(key).toString();
        }
    }
    cfg.saveToSettings(m_prefix);
    for (std::size_t i = 0; i < kRetiredWasapiKeys.size(); ++i) {
        const QString key = m_prefix + QLatin1Char('/') + QLatin1String(kRetiredWasapiKeys[i]);
        if (retired[i]) {
            settings.setValue(key, *retired[i]);
        } else {
            settings.remove(key);
        }
    }
    settings.save();
    m_loaded = cfg;

    m_applying = true;
    renderPill();

    emit configChanged(cfg);
}

// ---------------------------------------------------------------------------
// eventFilter — block wheel events on combo boxes in scroll areas
// per CLAUDE.md: "Event-filter QSlider/QComboBox in scroll areas to block
// wheel leak."
// ---------------------------------------------------------------------------
bool DeviceCard::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::Wheel) {
        auto* combo = qobject_cast<QComboBox*>(obj);
        if (combo && !combo->hasFocus()) {
            event->ignore();
            return true;
        }
    }
    return QGroupBox::eventFilter(obj, event);
}

} // namespace NereusSDR
