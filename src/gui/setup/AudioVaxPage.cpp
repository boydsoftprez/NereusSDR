// =================================================================
// src/gui/setup/AudioVaxPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Audio → VAX page.
// See AudioVaxPage.h for the full header.
//
// Sub-Phase 12 Task 12.3 (2026-04-20): Written by J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// =================================================================

#include "AudioVaxPage.h"

#include "core/AppSettings.h"
#include "core/AudioDeviceConfig.h"
#include "core/AudioEngine.h"
#include "core/audio/VirtualCableDetector.h"
#include "models/RadioModel.h"

#include <QDesktopServices>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

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

} // namespace

// ---------------------------------------------------------------------------
// VaxChannelCard
// ---------------------------------------------------------------------------
VaxChannelCard::VaxChannelCard(int channel, AudioEngine* engine, QWidget* parent)
    : QGroupBox(QStringLiteral("VAX %1").arg(channel), parent)
    , m_channel(channel)
    , m_prefix(QStringLiteral("audio/Vax%1").arg(channel))
{
    setStyleSheet(QLatin1String(kGroupStyle));

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 16, 8, 8);
    outerLayout->setSpacing(6);

    // Inner DeviceCard (7-row form + enable checkbox in title).
    m_deviceCard = new DeviceCard(m_prefix, DeviceCard::Role::Output, engine,
                                  /*enableCheckbox=*/true, this);
    outerLayout->addWidget(m_deviceCard);

    // Amber "override — no consumer" badge (hidden by default; shown on
    // Mac/Linux when bound to non-native with consumerCount == 0).
    m_badgeLabel = new QLabel(QStringLiteral("override — no consumer"), this);
    m_badgeLabel->setStyleSheet(QLatin1String(kBadgeStyle));
    m_badgeLabel->setToolTip(QStringLiteral(
        "A non-native device is bound but no app is reading from it. "
        "If this is unintentional, pick the default platform-native "
        "HAL device."));
    m_badgeLabel->setVisible(false);
    m_badgeLabel->setWordWrap(false);
    outerLayout->addWidget(m_badgeLabel);

    // Auto-detect button (initially visible; hidden when a device is bound).
    m_autoDetectBtn = new QPushButton(QStringLiteral("Auto-detect\u2026"), this);
    m_autoDetectBtn->setStyleSheet(QLatin1String(kAutoDetectStyle));
    m_autoDetectBtn->setAutoDefault(false);
    m_autoDetectBtn->setDefault(false);
    outerLayout->addWidget(m_autoDetectBtn);

    // Wire inner card signals.
    connect(m_deviceCard, &DeviceCard::configChanged,
            this, &VaxChannelCard::onInnerConfigChanged);
    connect(m_deviceCard, &DeviceCard::enabledChanged,
            this, &VaxChannelCard::onInnerEnabledChanged);
    connect(m_autoDetectBtn, &QPushButton::clicked,
            this, &VaxChannelCard::onAutoDetectClicked);
}

void VaxChannelCard::loadFromSettings()
{
    m_deviceCard->loadFromSettings();
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
    return m_deviceCard->currentConfig().deviceName;
}

bool VaxChannelCard::isEnabled() const
{
    // enabledChanged fires from the DeviceCard's checkbox; the checked state
    // is reflected in loadFromSettings → AppSettings read. Read directly.
    const QString key = m_prefix + QStringLiteral("/Enabled");
    return AppSettings::instance().value(key, QStringLiteral("False")).toString()
        == QStringLiteral("True");
}

void VaxChannelCard::onInnerConfigChanged(AudioDeviceConfig cfg)
{
    updateBadge();
    emit configChanged(m_channel, cfg);
}

void VaxChannelCard::onInnerEnabledChanged(bool on)
{
    emit enabledChanged(m_channel, on);
}

void VaxChannelCard::updateBadge()
{
    // Auto-detect button: show when no device is bound (name empty).
    const bool hasDevice = !currentDeviceName().isEmpty();
    m_autoDetectBtn->setVisible(!hasDevice);

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    // Amber badge: show only when bound to non-native and consumerCount == 0.
    // The native device name contains "NereusSDR" (reserved for the NereusSDR
    // HAL plugin). If the device is native, badge is always suppressed.
    if (hasDevice) {
        const QString devName = currentDeviceName();
        const bool isNative = devName.contains(QStringLiteral("NereusSDR"),
                                                Qt::CaseInsensitive);
        const bool badgeOn = !isNative
            && (VirtualCableDetector::consumerCount(devName) == 0);
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

    if (cables.isEmpty()) {
        // No cables case.
        QAction* noCablesAct = menu.addAction(
            QStringLiteral("No virtual cables detected"));
        noCablesAct->setEnabled(false);

        menu.addSeparator();

        QAction* installAct = menu.addAction(
            QStringLiteral("Install virtual cables\u2026"));
        connect(installAct, &QAction::triggered, this, []() {
            // Open generic VB-Audio landing page as default.
            QDesktopServices::openUrl(
                QUrl(VirtualCableDetector::installUrl(
                    VirtualCableProduct::VbCable)));
        });
    } else {
        // Build one entry per detected cable (output/render side only —
        // those are the cables that a consumer app reads from).
        bool hasAny = false;
        for (const DetectedCable& cable : cables) {
            if (cable.isInput) {
                continue; // skip capture side
            }
            hasAny = true;

            const int alreadyAssignedToChannel =
                assignedDeviceToChannel.value(cable.deviceName, 0);
            const bool alreadyAssigned = (alreadyAssignedToChannel > 0)
                && (alreadyAssignedToChannel != m_channel);

            QString label = QStringLiteral("\u25ba  ") + cable.deviceName;
            if (alreadyAssigned) {
                label += QStringLiteral("  \u2192 VAX %1")
                             .arg(alreadyAssignedToChannel);
            }

            QAction* act = menu.addAction(label);

            if (alreadyAssigned) {
                const int srcChannel    = alreadyAssignedToChannel;
                const int dstChannel    = m_channel;
                const QString devName   = cable.deviceName;
                connect(act, &QAction::triggered, this,
                        [this, devName, srcChannel, dstChannel]() {
                    // Reassign confirm modal.
                    QMessageBox confirm(this);
                    confirm.setWindowTitle(QStringLiteral("Reassign cable?"));
                    confirm.setText(
                        QStringLiteral("Reassign %1 from VAX %2 to VAX %3?"
                                       " VAX %2 will become unassigned.")
                            .arg(devName)
                            .arg(srcChannel)
                            .arg(dstChannel));
                    confirm.setStandardButtons(
                        QMessageBox::Ok | QMessageBox::Cancel);
                    confirm.setDefaultButton(QMessageBox::Cancel);
                    if (confirm.exec() != QMessageBox::Ok) {
                        return;
                    }
                    // Clear source channel binding.
                    const QString srcKey =
                        QStringLiteral("audio/Vax%1/DeviceName").arg(srcChannel);
                    AppSettings::instance().setValue(srcKey, QString());

                    // Apply to this channel via engine.
                    AudioDeviceConfig cfg = m_deviceCard->currentConfig();
                    cfg.deviceName = devName;
                    emit configChanged(dstChannel, cfg);
                });
            } else {
                const QString devName = cable.deviceName;
                connect(act, &QAction::triggered, this,
                        [this, devName]() {
                    AudioDeviceConfig cfg = m_deviceCard->currentConfig();
                    cfg.deviceName = devName;
                    emit configChanged(m_channel, cfg);
                });
            }
        }

        if (!hasAny) {
            // Only input-side cables found; surface a message.
            QAction* noCablesAct = menu.addAction(
                QStringLiteral("No output-side virtual cables detected"));
            noCablesAct->setEnabled(false);
        }
    }

    // Show menu anchored to the button (addendum §2.3: inline QMenu::exec
    // at button position). Qt handles ↑/↓ / Enter / Esc natively.
    menu.exec(m_autoDetectBtn->mapToGlobal(
        QPoint(0, m_autoDetectBtn->height())));
}

// ---------------------------------------------------------------------------
// AudioVaxPage
// ---------------------------------------------------------------------------
AudioVaxPage::AudioVaxPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("VAX"), model, parent)
    , m_engine(model ? model->audioEngine() : nullptr)
{
    buildPage();
    wirePillFeedback();
}

void AudioVaxPage::buildPage()
{
    // Scrollable content area so the page fits on small screens.
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(
        "QScrollArea { background: transparent; }"
        "QScrollBar:vertical { background: #0f0f1a; width: 8px; }"
        "QScrollBar::handle:vertical { background: #203040; border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }");

    auto* container = new QWidget;
    container->setStyleSheet(QStringLiteral("background: transparent;"));
    auto* vLayout = new QVBoxLayout(container);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(8);

    // Section header.
    auto* headerLabel = new QLabel(
        QStringLiteral("Virtual Audio eXchange — channel bindings"), container);
    headerLabel->setStyleSheet(
        QStringLiteral("QLabel { color: #8aa8c0; font-size: 12px; }"));
    vLayout->addWidget(headerLabel);

    // Four VAX channel cards (1–4).
    m_channelCards.reserve(4);
    for (int ch = 1; ch <= 4; ++ch) {
        auto* card = new VaxChannelCard(ch, m_engine, container);
        card->loadFromSettings();
        m_channelCards.append(card);
        vLayout->addWidget(card);

        // Wire configChanged to AudioEngine.
        if (m_engine) {
            connect(card, &VaxChannelCard::configChanged, this,
                    [this](int channel, AudioDeviceConfig cfg) {
                m_engine->setVaxConfig(channel, cfg);
            });
            connect(card, &VaxChannelCard::enabledChanged, this,
                    [this](int channel, bool on) {
                m_engine->setVaxEnabled(channel, on);
            });
        }
    }

    // TX row — informational; actual TX → VAX routing lands in Phase 3M.
    auto* txGroup = new QGroupBox(QStringLiteral("TX Monitor"), container);
    txGroup->setStyleSheet(QLatin1String(kGroupStyle));
    auto* txLayout = new QVBoxLayout(txGroup);
    auto* txLabel = new QLabel(
        QStringLiteral("TX → VAX routing is configured in the Transmit section. "
                       "Phase 3M (SendIqToVax / TxMonitorToVax) will add "
                       "per-band override here."),
        txGroup);
    txLabel->setStyleSheet(QStringLiteral("QLabel { color: #607080; font-size: 11px; }"));
    txLabel->setWordWrap(true);
    txLayout->addWidget(txLabel);
    vLayout->addWidget(txGroup);

    vLayout->addStretch(1);

    scrollArea->setWidget(container);

    // Insert the scroll area into this SetupPage's contentLayout.
    contentLayout()->addWidget(scrollArea);
}

void AudioVaxPage::wirePillFeedback()
{
    if (!m_engine) {
        return;
    }

    // Connect AudioEngine::vaxConfigChanged back to each card's pill.
    connect(m_engine, &AudioEngine::vaxConfigChanged, this,
            [this](int channel, AudioDeviceConfig cfg) {
        const int idx = channel - 1;
        if (idx >= 0 && idx < m_channelCards.size()) {
            m_channelCards[idx]->updateNegotiatedPill(cfg);
        }
    });
}

} // namespace NereusSDR
