// =================================================================
// src/gui/setup/AudioVaxPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Audio → VAX page.
// See AudioVaxPage.h for the full header.
//
// Sub-Phase 12 Task 12.3 (2026-04-20): Written by J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
//
// Task 21 (2026-04-24): Rebuilt per spec §9.2. VAX is a virtual source
// exposed to the system — not a device the user picks. DeviceCard 7-row
// form removed from visible layout; replaced with PipeWire-era info rows.
// DeviceCard retained hidden for API compatibility.
//
// 2026-09-23 (R-R3-44): J.J. Boyd (KG4VCF), AI-assisted via Anthropic
// Claude Code. The engine comes from RadioModel::localAudioDevices(): the
// VAX channels are this computer's in a remote window as in a local one,
// so the page works there. The "Consumers:" row shows whether an app is
// reading each channel where the platform reports it.
//
// 2026-09-24 (R-R3-49, R-R3-21): J.J. Boyd (KG4VCF), AI-assisted via
// Anthropic Claude Code. The channel cards follow
// AudioEngine::vaxBusOpenChanged (open state and "On" switch), so the page
// matches a container's VAX toggle.
//
// 2026-09-24 (R-R3-43, R-R3-44, R-R3-21): J.J. Boyd (KG4VCF), AI-assisted
// via Anthropic Claude Code. In a remote window whose receiver streams are
// Opus, a plain note says the weakest digital-mode signals may not decode
// and that Lossless avoids it; it follows the quality choice and its
// fallback live (setReceiverAudioNote; with Lossless chosen but not running
// it says the connection cannot carry it right now instead).
//
// 2026-09-24 (R-R3-43, R-R3-44, R-R3-23): J.J. Boyd (KG4VCF), AI-assisted
// via Anthropic Claude Code. The note says "a few of the weakest" signals:
// receiver streams now run Opus at 48 kbit/s when compressed, and the
// wording holds for that and for an older Core's 24 kbit/s.
// =================================================================

#include "AudioVaxPage.h"
#include "gui/RemoteAudioStatus.h"

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/audio/VirtualCableDetector.h"
#include "models/RadioModel.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Style constants — same palette as DeviceCard / STYLEGUIDE.md
// ---------------------------------------------------------------------------
namespace {

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

static const char* kBadgeStyle =
    "QLabel {"
    "  background: #2a1a00;"
    "  border: 1px solid #b87300;"
    "  border-radius: 6px;"
    "  color: #e8a030;"
    "  font-size: 10px;"
    "  padding: 2px 6px;"
    "}";

static const char* kAutoDetectStyle =
    "QPushButton {"
    "  background: #152535;"
    "  border: 1px solid #203040;"
    "  border-radius: 3px;"
    "  color: #00b4d8;"
    "  font-size: 11px;"
    "  padding: 3px 8px;"
    "}"
    "QPushButton:hover { background: #1d3045; }";

// Binding-status banner — persistent one-line indicator kept hidden;
// queried by test probes (findStatusBanner) via Qt widget hierarchy.
[[maybe_unused]] static const char* kStatusNativeStyle =
    "QLabel {"
    "  background: #0f2a1a;"
    "  border: 1px solid #2a6a3a;"
    "  border-radius: 3px;"
    "  color: #00ff88;"
    "  font-size: 11px;"
    "  font-weight: bold;"
    "  padding: 3px 8px;"
    "}";

static const char* kStatusBYOStyle =
    "QLabel {"
    "  background: #0f2030;"
    "  border: 1px solid #2a5a8a;"
    "  border-radius: 3px;"
    "  color: #00b4d8;"
    "  font-size: 11px;"
    "  font-weight: bold;"
    "  padding: 3px 8px;"
    "}";

static const char* kStatusUnboundStyle =
    "QLabel {"
    "  background: #2a1a00;"
    "  border: 1px solid #b87300;"
    "  border-radius: 3px;"
    "  color: #e8a030;"
    "  font-size: 11px;"
    "  font-weight: bold;"
    "  padding: 3px 8px;"
    "}";

static const char* kSpecRowValueStyle =
    "QLabel { color: #c8d8e8; font-size: 11px; }";

static const char* kSpecRowPlaceholderStyle =
    "QLabel { color: #607080; font-size: 11px; font-style: italic; }";

static const char* kActionBtnStyle =
    "QPushButton {"
    "  background: #152535;"
    "  border: 1px solid #203040;"
    "  border-radius: 3px;"
    "  color: #8aa8c0;"
    "  font-size: 11px;"
    "  padding: 3px 10px;"
    "}"
    "QPushButton:hover { background: #1d3045; color: #c8d8e8; }";

static const char* kEnableChkStyle =
    "QCheckBox { color: #8aa8c0; font-size: 11px; font-weight: bold; }"
    "QCheckBox::indicator { width: 14px; height: 14px; }"
    "QCheckBox::indicator:checked { background: #00b4d8; border: 1px solid #00b4d8; border-radius: 2px; }";

// Label builder for the disabled "native (bound automatically)" info
// row shown at the top of the Auto-detect menu on Mac/Linux when the
// platform-native HAL/pipe bridge is live. Exposed via a static
// VaxChannelCard::nativeHalLabelForCable seam for unit coverage.
QString nativeHalLabelForCableImpl(const DetectedCable& cable)
{
    return QStringLiteral(
               "►  %1 · NereusSDR · native (bound automatically)")
        .arg(cable.deviceName);
}

// Default node description for a channel (spec §10).
QString defaultNodeDescription(int channel)
{
    return QStringLiteral("NereusSDR VAX %1").arg(channel);
}

// PipeWire node name for a channel — the string consumer apps use.
QString pipeWireNodeName(int channel)
{
    return QStringLiteral("nereussdr.vax-%1").arg(channel);
}

} // namespace

QString VaxChannelCard::nativeHalLabelForCable(const DetectedCable& cable)
{
    return nativeHalLabelForCableImpl(cable);
}

// ---------------------------------------------------------------------------
// VaxChannelCard
// ---------------------------------------------------------------------------
VaxChannelCard::VaxChannelCard(int channel, QWidget* parent)
    : QGroupBox(QStringLiteral("VAX %1").arg(channel), parent)
    , m_channel(channel)
    , m_prefix(QStringLiteral("audio/Vax%1").arg(channel))
{
    setStyleSheet(QLatin1String(kGroupStyle));

    // ── Legacy hidden widgets created FIRST so findChild<QPushButton*>()
    //    in test probes finds m_autoDetectBtn (not m_renameBtn). Qt's
    //    findChild traversal is DFS in child-creation order, so earliest
    //    parented wins.
    m_autoDetectBtn = new QPushButton(QStringLiteral("Auto-detect…"), this);
    m_autoDetectBtn->setStyleSheet(QLatin1String(kAutoDetectStyle));
    m_autoDetectBtn->setAutoDefault(false);
    m_autoDetectBtn->setDefault(false);
    m_autoDetectBtn->setVisible(false);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(false);
    m_statusLabel->setTextFormat(Qt::PlainText);
    m_statusLabel->setVisible(false);

    m_badgeLabel = new QLabel(QStringLiteral("No program is using this device"), this);
    m_badgeLabel->setStyleSheet(QLatin1String(kBadgeStyle));
    m_badgeLabel->setVisible(false);

    m_deviceCard = new DeviceCard(m_prefix, DeviceCard::Role::Output,
                                  /*enableCheckbox=*/true, this);
    m_deviceCard->setVisible(false);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 16, 8, 8);
    outerLayout->setSpacing(6);

    // ── Spec §9.2 visible widgets ─────────────────────────────────────────

    // Enable / On toggle row (spec §9.2: "On" toggle in title area).
    {
        auto* enableRow = new QHBoxLayout;
        enableRow->setSpacing(6);
        m_enableChk = new QCheckBox(tr("On"), this);
        m_enableChk->setStyleSheet(QLatin1String(kEnableChkStyle));
        m_enableChk->setChecked(false);  // loadFromSettings() will set real value
        m_enableChk->setToolTip(tr("Enable this VAX channel. When on, NereusSDR "
                                   "exposes the channel as a PipeWire source that "
                                   "consumer apps (WSJT-X, FLDIGI, etc.) can "
                                   "select as an audio input device."));
        enableRow->addWidget(m_enableChk);
        enableRow->addStretch(1);
        outerLayout->addLayout(enableRow);

        connect(m_enableChk, &QCheckBox::toggled, this, [this](bool on) {
            // Persist the per-channel enable state (spec §10).
            AppSettings::instance().setValue(
                m_prefix + QStringLiteral("/Enabled"),
                on ? QStringLiteral("True") : QStringLiteral("False"));
            AppSettings::instance().save();
            // Sync the hidden DeviceCard checkbox to keep API compat.
            if (m_deviceCard) {
                QCheckBox* inner = m_deviceCard->findChild<QCheckBox*>();
                if (inner) {
                    QSignalBlocker blk(inner);
                    inner->setChecked(on);
                }
            }
            emit enabledChanged(m_channel, on);
        });
    }

    // "Exposed to system as:" row.
    {
        auto* form = new QFormLayout;
        form->setSpacing(4);
        form->setContentsMargins(0, 0, 0, 0);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto* exposedLbl = new QLabel(tr("Exposed to system as:"), this);
        exposedLbl->setStyleSheet(
            QStringLiteral("QLabel { color: #607080; font-size: 11px; }"));

        m_nodeDescLabel = new QLabel(
            defaultNodeDescription(m_channel), this);
        m_nodeDescLabel->setStyleSheet(QLatin1String(kSpecRowValueStyle));
        m_nodeDescLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        form->addRow(exposedLbl, m_nodeDescLabel);

        // "Format:" static row.
        auto* formatLbl = new QLabel(tr("Format:"), this);
        formatLbl->setStyleSheet(
            QStringLiteral("QLabel { color: #607080; font-size: 11px; }"));
        m_formatLabel = new QLabel(
            QStringLiteral("48000 Hz · Stereo · Float32"), this);
        m_formatLabel->setStyleSheet(QLatin1String(kSpecRowValueStyle));
        form->addRow(formatLbl, m_formatLabel);

        // "Consumers:" placeholder row.
        auto* consumersLbl = new QLabel(tr("Consumers:"), this);
        consumersLbl->setStyleSheet(
            QStringLiteral("QLabel { color: #607080; font-size: 11px; }"));
        // R-R3-44: whether an app is reading this channel, where the
        // platform reports it (AudioVaxPage::refreshReaders).
        m_consumerLabel = new QLabel(this);
        m_consumerLabel->setObjectName(QStringLiteral("vaxConsumerLabel"));
        m_consumerLabel->setStyleSheet(QLatin1String(kSpecRowPlaceholderStyle));
        m_consumerLabel->setToolTip(tr("Whether an app such as WSJT-X has this "
                                       "VAX channel open."));
        setReaderState(std::nullopt);
        form->addRow(consumersLbl, m_consumerLabel);

        // "Level:" HGauge row.
        auto* levelLbl = new QLabel(tr("Level:"), this);
        levelLbl->setStyleSheet(
            QStringLiteral("QLabel { color: #607080; font-size: 11px; }"));
        m_levelGauge = new HGauge(this);
        m_levelGauge->setRange(-60.0, 0.0);
        m_levelGauge->setYellowStart(-12.0);
        m_levelGauge->setRedStart(-3.0);
        m_levelGauge->setValue(-60.0);  // quiet until AudioVaxPage polls it
        m_levelGauge->setObjectName(QStringLiteral("vaxLevelGauge"));
        // R-R3-21: AudioVaxPage feeds it from AudioEngine::vaxRxLevel.
        m_levelGauge->setToolTip(tr("Audio level of this VAX channel"));
        form->addRow(levelLbl, m_levelGauge);

        outerLayout->addLayout(form);
    }

    // Action buttons row: "Rename…" + "Copy node name".
    {
        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(6);

        m_renameBtn = new QPushButton(tr("Rename…"), this);
        m_renameBtn->setStyleSheet(QLatin1String(kActionBtnStyle));
        m_renameBtn->setAutoDefault(false);
        m_renameBtn->setDefault(false);
        m_renameBtn->setToolTip(tr("Change the display name this channel is "
                                   "advertised as to consumer applications."));
        btnRow->addWidget(m_renameBtn);

        m_copyNodeBtn = new QPushButton(tr("Copy node name"), this);
        m_copyNodeBtn->setStyleSheet(QLatin1String(kActionBtnStyle));
        m_copyNodeBtn->setAutoDefault(false);
        m_copyNodeBtn->setDefault(false);
        m_copyNodeBtn->setToolTip(
            tr("Copy the PipeWire node.name (%1) to clipboard. "
               "Use this string in pw-link or consumer app config.")
                .arg(pipeWireNodeName(m_channel)));
        btnRow->addWidget(m_copyNodeBtn);

        btnRow->addStretch(1);
        outerLayout->addLayout(btnRow);
    }

    // Wire inner DeviceCard signals (kept for configChanged propagation).
    connect(m_deviceCard, &DeviceCard::configChanged,
            this, &VaxChannelCard::onInnerConfigChanged);
    connect(m_deviceCard, &DeviceCard::enabledChanged,
            this, &VaxChannelCard::onInnerEnabledChanged);
    connect(m_autoDetectBtn, &QPushButton::clicked,
            this, &VaxChannelCard::onAutoDetectClicked);

    // Wire spec §9.2 button signals.
    connect(m_renameBtn,   &QPushButton::clicked,
            this, &VaxChannelCard::onRenameClicked);
    connect(m_copyNodeBtn, &QPushButton::clicked,
            this, &VaxChannelCard::onCopyNodeNameClicked);

    // Populate the hidden status banner and node description label.
    updateBadge();
    updateNodeDescLabel();
}

void VaxChannelCard::loadFromSettings()
{
    // Load the DeviceCard's 10 fields + hidden enable checkbox.
    m_deviceCard->loadFromSettings();

    syncEnabledFromSettings();

    // Refresh node description label from persisted NodeDescription key.
    updateNodeDescLabel();
    updateBadge();
}

void VaxChannelCard::updateNegotiatedPill(const AudioDeviceConfig& negotiated,
                                          const QString& errorString)
{
    m_deviceCard->updateNegotiatedPill(negotiated, errorString);
    updateBadge();
}

QString VaxChannelCard::currentDeviceName() const
{
    // Primary source: live combo selection in the hidden DeviceCard.
    const QString fromCard = m_deviceCard->currentConfig().deviceName;
    if (!fromCard.isEmpty()) {
        return fromCard;
    }
    return AppSettings::instance()
               .value(m_prefix + QStringLiteral("/DeviceName"), QString())
               .toString();
}

void VaxChannelCard::syncEnabledFromSettings()
{
    // Sync visible enable toggle from AppSettings (same key the hidden
    // DeviceCard uses: audio/VaxN/Enabled).
    if (m_enableChk) {
        const bool on = AppSettings::instance()
                            .value(m_prefix + QStringLiteral("/Enabled"),
                                   QStringLiteral("False"))
                            .toString() == QStringLiteral("True");
        QSignalBlocker blk(m_enableChk);
        m_enableChk->setChecked(on);
    }
}

bool VaxChannelCard::isChannelEnabled() const
{
    // Visible checkbox is the primary source; hidden DeviceCard mirrors it.
    if (m_enableChk) {
        return m_enableChk->isChecked();
    }
    return m_deviceCard->isCheckboxEnabled();
}

void VaxChannelCard::applyAutoDetectBinding(const QString& deviceName)
{
    // 1) Build config: take whatever the card's combos currently say for all
    //    other fields, then overwrite the device name with the picked cable.
    AudioDeviceConfig cfg = m_deviceCard->currentConfig();
    cfg.deviceName = deviceName;

    // 2) Persist all 10 fields to AppSettings under audio/Vax<N>/.
    cfg.saveToSettings(m_prefix);
    AppSettings::instance().save();

    // 3) Refresh the DeviceCard combos from the now-persisted settings so the
    //    device combo, negotiated pill, and badge all reflect the new state
    //    without requiring a manual reload.
    m_deviceCard->loadFromSettings();

    // 4) Emit to engine (original configChanged path).
    emit configChanged(m_channel, cfg);

    // 5) Update badge + node description label.
    updateBadge();
    updateNodeDescLabel();
}

void VaxChannelCard::clearBinding()
{
    // 1) Build an empty config (all 10 fields reset to defaults).
    const AudioDeviceConfig empty;

    // 2) Persist the empty config, wiping all 10 AppSettings fields.
    empty.saveToSettings(m_prefix);
    AppSettings::instance().save();

    // 3) Refresh the DeviceCard UI from the now-empty settings.
    m_deviceCard->loadFromSettings();

    // 4) Tear down the engine bus: empty deviceName tells AudioEngine to close.
    emit configChanged(m_channel, empty);
    emit enabledChanged(m_channel, false);

    // 5) Update badge + node description label.
    updateBadge();
    updateNodeDescLabel();
}

void VaxChannelCard::onInnerConfigChanged(AudioDeviceConfig cfg)
{
    updateBadge();
    emit configChanged(m_channel, cfg);
}

void VaxChannelCard::onInnerEnabledChanged(bool on)
{
    // Sync visible enable toggle when the hidden DeviceCard drives a change.
    if (m_enableChk) {
        QSignalBlocker blk(m_enableChk);
        m_enableChk->setChecked(on);
    }
    updateBadge();
    emit enabledChanged(m_channel, on);
}

void VaxChannelCard::setReaderState(std::optional<bool> reading)
{
    if (!m_consumerLabel) {
        return;
    }
    if (!reading) {
        m_consumerLabel->setText(tr("Not reported on this computer"));
    } else if (*reading) {
        m_consumerLabel->setText(tr("An app is reading this channel"));
    } else {
        m_consumerLabel->setText(tr("No app is reading this channel"));
    }
}

QString VaxChannelCard::readerText() const
{
    return m_consumerLabel ? m_consumerLabel->text() : QString();
}

void VaxChannelCard::setBusOpen(bool open)
{
    if (m_busOpen == open) {
        return;
    }
    m_busOpen = open;
    updateBadge();
}

void VaxChannelCard::updateNodeDescLabel()
{
    if (!m_nodeDescLabel) {
        return;
    }
    // Read persisted node description (spec §10: Audio/VaxN/NodeDescription).
    const QString desc = AppSettings::instance()
        .value(m_prefix + QStringLiteral("/NodeDescription"),
               defaultNodeDescription(m_channel))
        .toString();
    m_nodeDescLabel->setText(desc);
}

void VaxChannelCard::onRenameClicked()
{
    // Current description for the dialog pre-fill.
    const QString current = AppSettings::instance()
        .value(m_prefix + QStringLiteral("/NodeDescription"),
               defaultNodeDescription(m_channel))
        .toString();

    bool ok = false;
    const QString newDesc = QInputDialog::getText(
        this,
        tr("Rename VAX %1").arg(m_channel),
        tr("Display name (advertised to consumer apps):"),
        QLineEdit::Normal,
        current,
        &ok);

    if (!ok || newDesc.trimmed().isEmpty()) {
        return;
    }

    // Persist via Audio/VaxN/NodeDescription (spec §10).
    AppSettings::instance().setValue(
        m_prefix + QStringLiteral("/NodeDescription"),
        newDesc.trimmed());
    AppSettings::instance().save();

    // Refresh the visible label.
    updateNodeDescLabel();
}

void VaxChannelCard::onCopyNodeNameClicked()
{
    // Copies nereussdr.vax-N to clipboard (PipeWire node.name convention).
    QApplication::clipboard()->setText(pipeWireNodeName(m_channel));
}

void VaxChannelCard::updateBadge()
{
    // Hidden auto-detect button: show only when no device is bound (name empty).
    // This logic is preserved for the hidden backing DeviceCard but the button
    // itself remains hidden in the spec §9.2 layout.
    const QString deviceName = currentDeviceName();
    const bool hasDevice = !deviceName.isEmpty();
    m_autoDetectBtn->setVisible(false);  // always hidden in spec §9.2 layout

    // Hidden binding-status banner — state machine kept for test compat.
    // Tests call findStatusBanner() which searches this label by text.
    if (m_statusLabel) {
        const bool enabled = isChannelEnabled();
        if (!enabled) {
            m_statusLabel->setStyleSheet(QLatin1String(kStatusUnboundStyle));
            m_statusLabel->setText(QStringLiteral(
                "⚠  Disabled. Enable it to route audio"));
            m_statusLabel->setToolTip(QStringLiteral(
                "The VAX channel's Enabled checkbox is off. Check it to "
                "open the audio bus and route receiver audio through this "
                "slot."));
        } else {
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
            const QString nativeName =
                QStringLiteral("NereusSDR VAX %1").arg(m_channel);
            if (hasDevice) {
                // BYO override path.
                if (!m_busOpen) {
                    m_statusLabel->setStyleSheet(
                        QLatin1String(kStatusUnboundStyle));
                    m_statusLabel->setText(
                        QStringLiteral("⚠  Bus failed to open: %1")
                            .arg(deviceName));
                    m_statusLabel->setToolTip(QStringLiteral(
                        "NereusSDR tried to open the selected 3rd-party "
                        "virtual cable but the PortAudio stream failed. "
                        "The device may be busy, unplugged, or the "
                        "Driver API / sample-rate combo may be "
                        "unsupported. Clear the Device picker to fall "
                        "back to the native HAL/pipe bridge."));
                } else {
                    m_statusLabel->setStyleSheet(
                        QLatin1String(kStatusBYOStyle));
                    m_statusLabel->setText(
                        QStringLiteral("✓  Bound: %1").arg(deviceName));
                    m_statusLabel->setToolTip(QStringLiteral(
                        "VAX channel is routed through the selected "
                        "3rd-party virtual cable (BYO override). Clear "
                        "the Device picker to fall back to the native "
                        "HAL/pipe bridge."));
                }
            } else {
                // Native HAL / PipeWire path.
                if (!m_busOpen) {
                    m_statusLabel->setStyleSheet(
                        QLatin1String(kStatusUnboundStyle));
#  if defined(Q_OS_MAC)
                    m_statusLabel->setText(QStringLiteral(
                        "⚠  Native HAL unavailable. Reinstall "
                        "NereusSDR"));
                    m_statusLabel->setToolTip(QStringLiteral(
                        "NereusSDR could not open the bundled CoreAudio "
                        "HAL plugin's shared-memory bridge. Reinstall "
                        "NereusSDR, or unblock NereusSDRVAX.driver in "
                        "System Settings → Privacy & Security."));
#  else
                    m_statusLabel->setText(QStringLiteral(
                        "⚠  Native PipeWire bridge unavailable"));
                    m_statusLabel->setToolTip(QStringLiteral(
                        "NereusSDR could not create a PipeWire "
                        "pipe-source for this VAX slot. Check that "
                        "PipeWire is running and that pactl is "
                        "available on PATH."));
#  endif
                } else {
                    m_statusLabel->setStyleSheet(
                        QLatin1String(kStatusNativeStyle));
#  if defined(Q_OS_MAC)
                    m_statusLabel->setText(
                        QStringLiteral(
                            "✓  Bound: Native HAL · %1")
                            .arg(nativeName));
                    m_statusLabel->setToolTip(QStringLiteral(
                        "VAX channel is routed through the bundled "
                        "CoreAudio HAL plugin. Consumer apps (WSJT-X, "
                        "FLDIGI, etc.) can select \"%1\" as their "
                        "audio input device.")
                            .arg(nativeName));
#  else
                    m_statusLabel->setText(
                        QStringLiteral(
                            "✓  Bound: Native (PipeWire) · %1")
                            .arg(nativeName));
                    m_statusLabel->setToolTip(QStringLiteral(
                        "VAX channel is routed through a native "
                        "PipeWire pipe source. Consumer apps can select "
                        "\"%1\" as their audio input device.")
                            .arg(nativeName));
#  endif
                }
            }
#else  // Q_OS_WIN
            if (hasDevice) {
                if (!m_busOpen) {
                    m_statusLabel->setStyleSheet(
                        QLatin1String(kStatusUnboundStyle));
                    m_statusLabel->setText(
                        QStringLiteral("⚠  Bus failed to open: %1")
                            .arg(deviceName));
                    m_statusLabel->setToolTip(QStringLiteral(
                        "NereusSDR tried to open the selected virtual "
                        "cable but the PortAudio stream failed. The "
                        "device may be busy, unplugged, or the Driver "
                        "API / sample-rate combo may be unsupported."));
                } else {
                    m_statusLabel->setStyleSheet(
                        QLatin1String(kStatusBYOStyle));
                    m_statusLabel->setText(
                        QStringLiteral("✓  Bound: %1").arg(deviceName));
                    m_statusLabel->setToolTip(QStringLiteral(
                        "VAX channel is routed through the selected "
                        "virtual cable."));
                }
            } else {
                m_statusLabel->setStyleSheet(
                    QLatin1String(kStatusUnboundStyle));
                m_statusLabel->setText(QStringLiteral(
                    "⚠  Not bound. Pick a virtual cable"));
                m_statusLabel->setToolTip(QStringLiteral(
                    "Windows has no built-in virtual audio cable. "
                    "Install VB-CABLE, Voicemeeter, or VAC and pick it "
                    "in the Device dropdown to enable this VAX channel."));
            }
#endif
        }
    }

#if defined(Q_OS_WIN)
    // Windows: gate enable checkbox until a BYO device is picked.
    // On Mac/Linux the PipeWire bridge is automatic (no gate needed).
    if (m_enableChk) {
        m_enableChk->setEnabled(hasDevice);
        if (!hasDevice) {
            m_enableChk->setToolTip(tr("Pick a device first"));
        }
    }
    m_deviceCard->setEnableAllowed(hasDevice);
#endif

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    // Amber badge logic — hidden widget, state kept for API compat.
    if (hasDevice) {
        const bool isNative = deviceName.contains(QStringLiteral("NereusSDR"),
                                                   Qt::CaseInsensitive);
        const bool badgeOn = !isNative
            && (VirtualCableDetector::consumerCount(deviceName) == 0);
        m_badgeLabel->setVisible(badgeOn);
    } else {
        m_badgeLabel->setVisible(false);
    }
#else
    m_badgeLabel->setVisible(false);
#endif
}

void VaxChannelCard::onAutoDetectClicked()
{
    // Gather current assignments (all 4 channels) to detect already-assigned
    // cables and report "→ VAX N" in the menu.
    QMap<QString, int> assignedDeviceToChannel;
    for (int ch = 1; ch <= 4; ++ch) {
        const QString key = QStringLiteral("audio/Vax%1/DeviceName").arg(ch);
        const QString dev = AppSettings::instance()
                                .value(key, QString()).toString();
        if (!dev.isEmpty()) {
            assignedDeviceToChannel[dev] = ch;
        }
    }

    // Scan virtual cables (or use injected test vector if the seam is active).
#ifdef NEREUS_BUILD_TESTS
    const QVector<DetectedCable> cables =
        m_useTestCables ? m_testCables : VirtualCableDetector::scan();
#else
    const QVector<DetectedCable> cables = VirtualCableDetector::scan();
#endif

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background: #0f0f1a; color: #c8d8e8; border: 1px solid #203040; }"
        "QMenu::item:selected { background: #203040; }"
        "QMenu::item:disabled { color: #506070; }");

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    int nativeHalCount = 0;
    for (const DetectedCable& cable : cables) {
        if (cable.product != VirtualCableProduct::NereusSdrVax) {
            continue;
        }
        QAction* act = menu.addAction(nativeHalLabelForCable(cable));
        act->setEnabled(false);
        ++nativeHalCount;
    }
    if (nativeHalCount > 0) {
        menu.addSeparator();
    }
#endif

    if (cables.isEmpty()) {
        QAction* noCablesAct = menu.addAction(
            QStringLiteral("No virtual cables detected"));
        noCablesAct->setEnabled(false);

        menu.addSeparator();

        QAction* installAct = menu.addAction(
            QStringLiteral("Install virtual cables…"));
        connect(installAct, &QAction::triggered, this, []() {
            QDesktopServices::openUrl(
                QUrl(VirtualCableDetector::installUrl(
                    VirtualCableProduct::VbCable)));
        });
    } else {
        bool hasAny = false;
        for (const DetectedCable& cable : cables) {
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
            if (cable.product == VirtualCableProduct::NereusSdrVax) {
                continue;
            }
#endif
            if (cable.isInput) {
                continue;
            }
            hasAny = true;

            const int alreadyAssignedToChannel =
                assignedDeviceToChannel.value(cable.deviceName, 0);
            const bool alreadyAssigned = (alreadyAssignedToChannel > 0)
                && (alreadyAssignedToChannel != m_channel);

            const QString vendor =
                VirtualCableDetector::vendorDisplayName(cable.product);
            QString label = QStringLiteral("►  ") + cable.deviceName;
            if (!vendor.isEmpty()) {
                label += QStringLiteral(" · ") + vendor;
            }
            if (alreadyAssigned) {
                label += QStringLiteral("  → VAX %1")
                             .arg(alreadyAssignedToChannel);
            }

            QAction* act = menu.addAction(label);

            if (alreadyAssigned) {
                const int srcChannel    = alreadyAssignedToChannel;
                const QString devName   = cable.deviceName;
                connect(act, &QAction::triggered, this,
                        [this, devName, srcChannel]() {
                    QMessageBox confirm(this);
                    confirm.setWindowTitle(QStringLiteral("Reassign cable?"));
                    confirm.setText(
                        QStringLiteral("Reassign %1 from VAX %2 to VAX %3?"
                                       " VAX %2 will become unassigned.")
                            .arg(devName)
                            .arg(srcChannel)
                            .arg(m_channel));
                    confirm.setStandardButtons(
                        QMessageBox::Ok | QMessageBox::Cancel);
                    confirm.setDefaultButton(QMessageBox::Cancel);
                    if (confirm.exec() != QMessageBox::Ok) {
                        return;
                    }

                    AudioVaxPage* page = nullptr;
                    for (QObject* p = parent(); p; p = p->parent()) {
                        if (auto* vp = qobject_cast<AudioVaxPage*>(p)) {
                            page = vp;
                            break;
                        }
                    }
                    if (page) {
                        VaxChannelCard* srcCard = page->channelCard(srcChannel);
                        if (srcCard) {
                            srcCard->clearBinding();
                        }
                    }

                    applyAutoDetectBinding(devName);
                });
            } else {
                const QString devName = cable.deviceName;
                connect(act, &QAction::triggered, this,
                        [this, devName]() {
                    applyAutoDetectBinding(devName);
                });
            }
        }

        if (!hasAny) {
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
            if (nativeHalCount > 0) {
                QAction* hintAct = menu.addAction(
                    QStringLiteral(
                        "No 3rd-party cables available (BYO override optional)"));
                hintAct->setEnabled(false);
            } else {
                QAction* noCablesAct = menu.addAction(
                    QStringLiteral("No output-side virtual cables detected"));
                noCablesAct->setEnabled(false);
            }
#else
            QAction* noCablesAct = menu.addAction(
                QStringLiteral("No output-side virtual cables detected"));
            noCablesAct->setEnabled(false);
#endif
        }
    }

    menu.exec(m_autoDetectBtn->mapToGlobal(
        QPoint(0, m_autoDetectBtn->height())));
}

// ---------------------------------------------------------------------------
// AudioVaxPage
// ---------------------------------------------------------------------------
AudioVaxPage::AudioVaxPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("VAX"), model, parent)
    // R-R3-44: this computer's VAX outputs, live in a remote window too.
    , m_engine(model ? model->localAudioDevices() : nullptr)
{
    buildPage();
    wirePillFeedback();
    m_levelTimer = new QTimer(this);
    m_levelTimer->setInterval(50);  // 20 Hz, as VaxApplet polls
    connect(m_levelTimer, &QTimer::timeout, this, &AudioVaxPage::pollLevels);
    refreshReaders();
    auto* readerTimer = new QTimer(this);
    readerTimer->setInterval(1000);
    connect(readerTimer, &QTimer::timeout, this, &AudioVaxPage::refreshReaders);
    readerTimer->start();
}

void AudioVaxPage::setReceiverAudioNote(RemoteReceiverAudioNote note)
{
    if (!m_compressedNote) {
        return;
    }
    switch (note) {
    case RemoteReceiverAudioNote::None:
        break;
    case RemoteReceiverAudioNote::OpusChosen:
        m_compressedNote->setText(QStringLiteral(
            "Receiver audio from the Core is compressed (Opus), so a few of the weakest "
            "digital-mode signals may not decode. Set Audio quality to Lossless "
            "in Core connection if your network can carry it."));
        break;
    case RemoteReceiverAudioNote::LosslessUnavailable:
        // The operator already chose Lossless; pointing them at it again
        // would be wrong. Say the connection cannot carry it right now.
        m_compressedNote->setText(QStringLiteral(
            "Receiver audio from the Core is compressed (Opus): Lossless is chosen, "
            "but this connection cannot carry it right now. A few of the weakest "
            "digital-mode signals may not decode."));
        break;
    }
    m_compressedNote->setVisible(note != RemoteReceiverAudioNote::None);
}

bool AudioVaxPage::compressedAudioNoteShown() const
{
    return m_compressedNote && !m_compressedNote->isHidden();
}

QString AudioVaxPage::compressedAudioNoteText() const
{
    return m_compressedNote ? m_compressedNote->text() : QString();
}

void AudioVaxPage::refreshReaders()
{
    for (int i = 0; i < m_channelCards.size(); ++i) {
        m_channelCards[i]->setReaderState(
            m_engine ? m_engine->vaxOutputHasReader(i + 1) : std::nullopt);
    }
}

void AudioVaxPage::showEvent(QShowEvent* event)
{
    SetupPage::showEvent(event);
    pollLevels();
    m_levelTimer->start();
}

void AudioVaxPage::hideEvent(QHideEvent* event)
{
    m_levelTimer->stop();
    SetupPage::hideEvent(event);
}

void AudioVaxPage::pollLevels()
{
    if (!m_engine) { return; }
    for (VaxChannelCard* card : std::as_const(m_channelCards)) {
        card->setLevel(m_engine->vaxRxLevel(card->channelIndex()));
    }
}

void VaxChannelCard::setLevel(float linear)
{
    if (!m_levelGauge) { return; }
    // Linear 0..1 to dBFS, floored at the gauge floor, -60 dB.
    double dB = -60.0;
    if (std::isfinite(linear) && linear > 0.001f) {
        dB = 20.0 * std::log10(static_cast<double>(linear));
    }
    m_levelGauge->setValue(std::clamp(dB, -60.0, 0.0));
}

double VaxChannelCard::levelDbForTest() const
{
    return m_levelGauge ? m_levelGauge->value() : -60.0;
}

void AudioVaxPage::buildPage()
{
    // SetupPage base already wraps contentLayout() in a QScrollArea with a
    // trailing addStretch(1) — insert widgets before that stretch so the
    // content stacks from the top of the viewport.
    auto insertBeforeStretch = [this](QWidget* w) {
        const int stretchIndex = contentLayout()->count() - 1;
        contentLayout()->insertWidget(stretchIndex, w);
    };

    // Section header.
    auto* headerLabel = new QLabel(
        QStringLiteral("Virtual Audio eXchange: PipeWire sources"), this);
    headerLabel->setStyleSheet(
        QStringLiteral("QLabel { color: #8aa8c0; font-size: 12px; }"));
    insertBeforeStretch(headerLabel);

    // Sub-header describing the new Phase 3O model.
    auto* subHeader = new QLabel(
        QStringLiteral(
            "Each VAX channel is exposed to the system as a PipeWire virtual "
            "source (node). Consumer applications (WSJT-X, FLDIGI, etc.) "
            "select it as an audio input device; no virtual cable needed."),
        this);
    subHeader->setStyleSheet(
        QStringLiteral("QLabel { color: #607080; font-size: 11px; }"));
    subHeader->setWordWrap(true);
    insertBeforeStretch(subHeader);

    // R-R3-43 / R-R3-44: in a remote window whose receiver streams are Opus,
    // say what that costs digital modes and what avoids it. Receiver streams
    // run Opus at 48 kbit/s when compressed: in the confirming FT8 run it
    // decoded 175 files in all, 173 of the 177 the untouched audio decoded
    // plus 2 it missed (24 kbit/s, an older Core's rate: 164 in all, 162 of
    // the 177 plus 2); lossless decoded exactly the untouched audio's 177.
    // "A few of the weakest" holds for both rates
    // (docs/architecture/2026-09-20-remote-daemon-r3-verification/
    // digital-modes-over-opus.md, "Confirming run"). Hidden until MainWindow
    // says so.
    m_compressedNote = new QLabel(
        QStringLiteral(
            "Receiver audio from the Core is compressed (Opus), so a few of the weakest "
            "digital-mode signals may not decode. Set Audio quality to Lossless "
            "in Core connection if your network can carry it."),
        this);
    m_compressedNote->setObjectName(QStringLiteral("vaxCompressedAudioNote"));
    m_compressedNote->setStyleSheet(
        QStringLiteral("QLabel { color: #607080; font-size: 11px; }"));
    m_compressedNote->setWordWrap(true);
    m_compressedNote->setVisible(false);
    insertBeforeStretch(m_compressedNote);

    // Four VAX channel cards (1–4).
    m_channelCards.reserve(4);
    for (int ch = 1; ch <= 4; ++ch) {
        auto* card = new VaxChannelCard(ch, this);
        card->loadFromSettings();
        m_channelCards.append(card);
        insertBeforeStretch(card);

        // Wire configChanged to AudioEngine.
        if (m_engine) {
            card->setBusOpen(m_engine->isVaxBusOpen(ch));

            connect(card, &VaxChannelCard::configChanged, this,
                    [this](int channel, AudioDeviceConfig cfg) {
                m_engine->setVaxConfig(channel, cfg);
                if (auto* c = channelCard(channel)) {
                    c->setBusOpen(m_engine->isVaxBusOpen(channel));
                }
            });
            connect(card, &VaxChannelCard::enabledChanged, this,
                    [this](int channel, bool on) {
                m_engine->setVaxEnabled(channel, on);
                if (auto* c = channelCard(channel)) {
                    c->setBusOpen(m_engine->isVaxBusOpen(channel));
                }
            });
        }
    }

    // R-R3-49 fix wave I2: the informational "TX Monitor" group is gone. Its
    // only text promised a later per-band override and pointed at Send IQ to
    // VAX and TX Monitor to VAX, which are hidden until built (iq-to-vax).
}

void AudioVaxPage::wirePillFeedback()
{
    if (!m_engine) {
        return;
    }

    // R-R3-21: the open state and the "On" switch follow every change to a
    // VAX output, including a container's VAX toggle and a remote window
    // opening its outputs.
    connect(m_engine, &AudioEngine::vaxBusOpenChanged, this, [this](int channel) {
        if (auto* card = channelCard(channel)) {
            card->setBusOpen(m_engine->isVaxBusOpen(channel));
            card->syncEnabledFromSettings();
        }
    });

    connect(m_engine, &AudioEngine::vaxConfigChanged, this,
            [this](int channel, AudioDeviceConfig cfg) {
        const int idx = channel - 1;
        if (idx >= 0 && idx < m_channelCards.size()) {
            m_channelCards[idx]->updateNegotiatedPill(cfg);
            m_channelCards[idx]->setBusOpen(
                m_engine->isVaxBusOpen(channel));
        }
    });
}

} // namespace NereusSDR
