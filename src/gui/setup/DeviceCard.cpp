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
// =================================================================

#include "DeviceCard.h"

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/audio/PortAudioBus.h"
#include "gui/UnbuiltFeatures.h"

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

#include <memory>
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

// PortAudio's name for its WASAPI host API.
// From PortAudio src/hostapi/wasapi/pa_win_wasapi.c:2352 [v19.7.0]
static constexpr const char* kWasapiHostApiName = "Windows WASAPI";

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
{
    setStyleSheet(QLatin1String(kGroupStyle));
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

// ---------------------------------------------------------------------------
// buildLayout: the Device row, then the folded "Device details" section
// ---------------------------------------------------------------------------
// R-SPK-21 / D14: the Device row stays in front; Driver API, Sample rate,
// Bit depth, Channels, Buffer size, Options and Negotiated fold under
// "Device details", folded by default. The driver API combo is created
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

    // ── Driver API (Device details) ──────────────────────────────────────
    m_driverApiCombo = new QComboBox(m_details);
    m_driverApiCombo->setStyleSheet(QLatin1String(kComboStyle));
    // Populate from PortAudio host APIs (requires Pa_Initialize done).
    const auto apis = PortAudioBus::hostApis();
    m_driverApiCombo->addItem(QStringLiteral("(PortAudio default)"),
                              QVariant::fromValue(-1));
    for (const auto& api : apis) {
        m_driverApiCombo->addItem(api.name, QVariant::fromValue(api.index));
    }

    // ── Device (always in front) ─────────────────────────────────────────
    auto* deviceForm = makeForm();
    m_deviceCombo = new QComboBox(m_body);
    m_deviceCombo->setStyleSheet(QLatin1String(kComboStyle));
    m_deviceCombo->setMinimumWidth(200);
    populateDeviceCombo();
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
    detailsForm->addRow(makeLabel(QStringLiteral("Driver API:")), m_driverApiCombo);

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

    // Options (WASAPI). R-SPK-24: live only when the card's driver API is
    // WASAPI, greyed with wasapiOnlyReason() otherwise (updateWasapiOptions).
    {
        auto* optRow = new QHBoxLayout;
        optRow->setSpacing(10);
        m_exclusiveChk   = new QCheckBox(QStringLiteral("Exclusive"));
        m_eventDrivenChk = new QCheckBox(QStringLiteral("Event-driven"));
        m_bypassMixerChk = new QCheckBox(QStringLiteral("Bypass mixer"));
        for (QCheckBox* chk : { m_exclusiveChk, m_eventDrivenChk, m_bypassMixerChk }) {
            chk->setStyleSheet(QLatin1String(kCheckStyle));
            chk->setToolTip(QStringLiteral("WASAPI only"));
            optRow->addWidget(chk);
        }
        optRow->addStretch();
        detailsForm->addRow(makeLabel(QStringLiteral("Options:")), optRow);
        m_wasapiNote = new QLabel(wasapiOnlyReason());
        m_wasapiNote->setObjectName(QStringLiteral("wasapiOnlyNote"));
        m_wasapiNote->setStyleSheet(QLatin1String(kDimLabelStyle));
        // One short line: a word-wrapped label in a form row is clipped to
        // one line's height.
        m_wasapiNote->setWordWrap(false);
        detailsForm->addRow(makeLabel(QString()), m_wasapiNote);
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

    bodyLayout->addWidget(m_details);
    m_details->setVisible(false);

    connect(m_detailsToggle, &QToolButton::toggled, this, [this](bool on) {
        m_detailsToggle->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
        m_details->setVisible(on);
    });
    connect(m_driverApiCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateWasapiOptions(); });
    updateWasapiOptions();

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
    connectCombo(m_driverApiCombo);
    connectCombo(m_sampleRateCombo);
    connectCombo(m_bitDepthCombo);
    connectCombo(m_channelsCombo);
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

    // Device combo fires populateDeviceCombo on driver-API change, then
    // also commits via the device-combo's own currentIndexChanged.
    connect(m_driverApiCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                populateDeviceCombo();
                // onAnyControlChanged is called via the device combo's
                // own signal after population — no double-commit here.
            });
    connectCombo(m_deviceCombo);

    auto connectCheck = [this](QCheckBox* chk) {
        if (!chk) { return; }
        connect(chk, &QCheckBox::toggled,
                this, &DeviceCard::onAnyControlChanged);
    };
    connectCheck(m_autoMatchSampleRate);
    connectCheck(m_exclusiveChk);
    connectCheck(m_eventDrivenChk);
    connectCheck(m_bypassMixerChk);
    if (m_monitorDuringTxChk) { connectCheck(m_monitorDuringTxChk); }
    if (m_toneCheckChk)       { connectCheck(m_toneCheckChk); }

    // Buffer-size or sample-rate change → update derived-ms label.
    connect(m_bufferSizeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateBufferMsLabel(); });
    connect(m_sampleRateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { updateBufferMsLabel(); });
}

// ---------------------------------------------------------------------------
// Device details, page rows, Enabled greying, rescan, WASAPI options
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
    // Keeps the selection: populateDeviceCombo re-selects the current name
    // (a configured device that has gone stays as "(not available)").
    populateDeviceCombo();
}

int DeviceCard::deviceCount() const
{
    int count = 0;
    for (int i = 0; i < m_deviceCombo->count(); ++i) {
        if (!m_deviceCombo->itemData(i).toString().isEmpty()
            && !m_deviceCombo->itemData(i, kKeptEntryRole).toBool()) {
            ++count;
        }
    }
    return count;
}

bool DeviceCard::isWasapiDriverName(const QString& driverApi)
{
    return driverApi == QLatin1String(kWasapiHostApiName);
}

QString DeviceCard::wasapiOnlyReason()
{
    return QStringLiteral("These three work only with WASAPI on Windows.");
}

bool DeviceCard::wasapiOptionsAvailable() const
{
    return m_driverApiCombo != nullptr && m_driverApiCombo->currentIndex() > 0
        && isWasapiDriverName(m_driverApiCombo->currentText());
}

void DeviceCard::updateWasapiOptions()
{
    const bool live = wasapiOptionsAvailable();
    const QString tip = live ? QStringLiteral("WASAPI only") : wasapiOnlyReason();
    for (QCheckBox* chk : { m_exclusiveChk, m_eventDrivenChk, m_bypassMixerChk }) {
        if (chk == nullptr) {
            continue;
        }
        chk->setEnabled(live);
        chk->setToolTip(tip);
    }
    if (m_wasapiNote != nullptr) {
        m_wasapiNote->setVisible(!live);
    }
}

// ---------------------------------------------------------------------------
// populateDeviceCombo
// ---------------------------------------------------------------------------
void DeviceCard::populateDeviceCombo()
{
    QSignalBlocker blocker(m_deviceCombo);
    const QString prevName = m_deviceCombo->currentData().toString();

    resetPopupAccessibilityCache(m_deviceCombo);
    m_deviceCombo->clear();
    m_deviceCombo->addItem(QStringLiteral("(platform default)"), QString());

    const int apiIdx = m_driverApiCombo
        ? m_driverApiCombo->currentData().toInt()
        : -1;

    QVector<PortAudioBus::DeviceInfo> devices;
    if (apiIdx < 0) {
        // All APIs.
        const auto apis = PortAudioBus::hostApis();
        for (const auto& api : apis) {
            if (m_role == Role::Output) {
                const auto devs = PortAudioBus::outputDevicesFor(api.index);
                devices += devs;
            } else {
                const auto devs = PortAudioBus::inputDevicesFor(api.index);
                devices += devs;
            }
        }
    } else {
        if (m_role == Role::Output) {
            devices = PortAudioBus::outputDevicesFor(apiIdx);
        } else {
            devices = PortAudioBus::inputDevicesFor(apiIdx);
        }
    }

    for (int i = 0; i < devices.size(); ++i) {
        m_deviceCombo->addItem(devices[i].name,
                               QVariant::fromValue(devices[i].name));
    }
    selectDeviceName(prevName);
}

// ---------------------------------------------------------------------------
// selectDeviceName — select a configured device, never a substitute
// ---------------------------------------------------------------------------
// R-R3-36: a named device that is not present is kept as
// "<name> (not available)" with the name as its data, so the card shows it
// and currentConfig() saves the same name back. Falling to
// "(platform default)" would silently switch the device on the next edit.
void DeviceCard::selectDeviceName(const QString& name)
{
    removeKeptEntries(m_deviceCombo);
    int idx = 0;
    if (!name.isEmpty()) {
        idx = m_deviceCombo->findData(QVariant::fromValue(name));
        if (idx < 0) {
            m_deviceCombo->addItem(
                QStringLiteral("%1 (not available)").arg(name),
                QVariant::fromValue(name));
            idx = m_deviceCombo->count() - 1;
            m_deviceCombo->setItemData(idx, true, kKeptEntryRole);
        }
    }
    m_deviceCombo->setCurrentIndex(idx);
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
    AudioDeviceConfig cfg;

    // deviceName: empty string from "(platform default)" entry maps to
    // AudioDeviceConfig empty deviceName → makeBus treats as platform default.
    cfg.deviceName = m_deviceCombo->currentData().toString();

    cfg.driverApi = (m_driverApiCombo && m_driverApiCombo->currentIndex() > 0)
        ? m_driverApiCombo->currentText()
        : QString();

    cfg.hostApiIndex = m_driverApiCombo
        ? m_driverApiCombo->currentData().toInt()
        : -1;

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

    cfg.exclusiveMode = m_exclusiveChk   && m_exclusiveChk->isChecked();
    cfg.eventDriven   = m_eventDrivenChk && m_eventDrivenChk->isChecked();
    cfg.bypassMixer   = m_bypassMixerChk && m_bypassMixerChk->isChecked();

    cfg.manualLatencyMs = 0;  // Not exposed in this card; reserved for Advanced page.

    return cfg;
}

// ---------------------------------------------------------------------------
// updateNegotiatedPill
// ---------------------------------------------------------------------------
void DeviceCard::updateNegotiatedPill(const AudioDeviceConfig& negotiated,
                                      const QString& errorString)
{
    if (!m_negotiatedPill) {
        return;
    }

    if (!errorString.isEmpty()) {
        // Red pill — driver rejected the config.
        m_negotiatedPill->setStyleSheet(QLatin1String(kPillStyleError));
        m_negotiatedPill->setText(QStringLiteral("Error: ") + errorString);
        return;
    }

    // Green pill — show negotiated format.
    m_negotiatedPill->setStyleSheet(QLatin1String(kPillStyleOk));
    const QString name = negotiated.deviceName.isEmpty()
        ? QStringLiteral("(default)")
        : negotiated.deviceName;
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

    m_suppressSignals = true;

    // Device name. A configured device that is not present stays selected.
    {
        QSignalBlocker blocker(m_deviceCombo);
        selectDeviceName(cfg.deviceName);
    }

    // Driver API — look up by display text (api.name is the item text, set in
    // buildLayout).  Empty driverApi falls through to index 0 ("(PortAudio
    // default)").  findText returns -1 on no match; guard keeps found == 0.
    if (m_driverApiCombo) {
        int found = 0;
        if (!cfg.driverApi.isEmpty()) {
            const int byName = m_driverApiCombo->findText(cfg.driverApi);
            if (byName >= 0) {
                found = byName;
            }
        }
        m_driverApiCombo->setCurrentIndex(found);
    }

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

    // WASAPI options.
    if (m_exclusiveChk)   { m_exclusiveChk->setChecked(cfg.exclusiveMode); }
    if (m_eventDrivenChk) { m_eventDrivenChk->setChecked(cfg.eventDriven); }
    if (m_bypassMixerChk) { m_bypassMixerChk->setChecked(cfg.bypassMixer); }

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

    // Persist to AppSettings immediately.
    cfg.saveToSettings(m_prefix);
    AppSettings::instance().save();

    // Show "APPLYING" pill while the engine is rebuilding the bus.
    if (m_negotiatedPill) {
        m_negotiatedPill->setStyleSheet(QLatin1String(kPillStyleApplying));
        m_negotiatedPill->setText(QStringLiteral("APPLYING…"));
    }

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
