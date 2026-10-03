// =================================================================
// src/gui/setup/AudioAdvancedPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Audio → Advanced page.
// See AudioAdvancedPage.h for the full header.
//
// Sub-Phase 12 Task 12.4 (2026-04-20): Written by J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
//
// 2026-09-23: R-R3-10 / R-R3-23 by J.J. Boyd (KG4VCF), with AI-assisted
// implementation via Anthropic Claude Code. In a remote window, Reset
// removes only this computer's audio/* keys (never a key the Core holds,
// such as audio/DspRate and audio/DspBlockSize) and recreates no VAX
// outputs; local Reset is unchanged.
//
// 2026-09-23: R-R3-44 by J.J. Boyd (KG4VCF), with AI-assisted
// implementation via Anthropic Claude Code. Usable in a remote window: the
// engine comes from RadioModel::localAudioDevices() (the VAX groups are
// this computer's), the DSP group follows the Core's settings
// availability, and Send IQ to VAX is refused there with a plain reason.
//
// 2026-09-24: R-R3-49 by J.J. Boyd (KG4VCF), with AI-assisted
// implementation via Anthropic Claude Code. The DSP group, Send IQ to VAX,
// TX Monitor to VAX and Mute VAX during TX on other slice are hidden
// (UnbuiltFeatures) until they are applied; their saved values stay.
//
// 2026-09-24: R-R3-49 by J.J. Boyd (KG4VCF), with AI-assisted
// implementation via Anthropic Claude Code. The VAC feedback-loop tuning
// group is removed, with its editor and reader; saved
// audio/VacFeedback/<ch>/* values stay in the settings file.
// =================================================================

#include "AudioAdvancedPage.h"

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/LogCategories.h"
#include "core/settings/SettingsScope.h"
#include "core/audio/VirtualCableDetector.h"
#include "gui/UnbuiltFeatures.h"
#include "gui/VaxFirstRunDialog.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Style constants (matching AudioVaxPage / DeviceCard palette)
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

static const char* kNoteStyle =
    "QLabel { color: #607080; font-size: 11px; font-style: italic; }";

static const char* kAmberButtonStyle =
    "QPushButton {"
    "  background: #2a1a00;"
    "  border: 1px solid #b87300;"
    "  border-radius: 4px;"
    "  color: #e8a030;"
    "  font-weight: bold;"
    "  padding: 6px 16px;"
    "}"
    "QPushButton:hover { background: #3a2500; border-color: #e8a030; }"
    "QPushButton:pressed { background: #1a1000; }";

static const char* kComboStyle =
    "QComboBox {"
    "  background: #152535;"
    "  border: 1px solid #203040;"
    "  border-radius: 3px;"
    "  color: #c8d8e8;"
    "  padding: 2px 6px;"
    "}"
    "QComboBox::drop-down { border: none; }"
    "QComboBox QAbstractItemView { background: #152535; color: #c8d8e8; "
    "  selection-background-color: #1d3045; }";

} // namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

AudioAdvancedPage::AudioAdvancedPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Advanced"), model, parent)
    // R-R3-44: this computer's engine, live in a remote window too.
    , m_engine(model ? model->localAudioDevices() : nullptr)
{
    buildDspSection();
    buildFeatureFlagsSection();
    buildCablesSection();
    buildResetSection();
}

// ---------------------------------------------------------------------------
// Section 1 — DSP sample-rate + block-size
// ---------------------------------------------------------------------------

void AudioAdvancedPage::buildDspSection()
{
    auto* box = new QGroupBox(QStringLiteral("DSP"), this);
    box->setStyleSheet(QLatin1String(kGroupStyle));
    box->setObjectName(QStringLiteral("audioAdvancedDspGroup"));
    // R-R3-49: nothing applies the DSP rate or block size yet; the group is
    // hidden until something does.
    UnbuiltFeatures::hideUnlessBuilt(box, UnbuiltFeature::DspRate);
    auto* form = new QFormLayout(box);
    form->setSpacing(6);
    form->setContentsMargins(8, 16, 8, 8);

    m_dspRateCombo = new QComboBox(box);
    m_dspRateCombo->setStyleSheet(QLatin1String(kComboStyle));
    m_dspRateCombo->addItem(QStringLiteral("48 000 Hz"),  48000);
    m_dspRateCombo->addItem(QStringLiteral("96 000 Hz"),  96000);
    m_dspRateCombo->addItem(QStringLiteral("192 000 Hz"), 192000);
    installWheelFilter(m_dspRateCombo);
    form->addRow(QStringLiteral("DSP Sample Rate"), m_dspRateCombo);

    m_dspBlockCombo = new QComboBox(box);
    m_dspBlockCombo->setStyleSheet(QLatin1String(kComboStyle));
    m_dspBlockCombo->addItem(QStringLiteral("64"),   64);
    m_dspBlockCombo->addItem(QStringLiteral("128"),  128);
    m_dspBlockCombo->addItem(QStringLiteral("256"),  256);
    m_dspBlockCombo->addItem(QStringLiteral("512"),  512);
    m_dspBlockCombo->addItem(QStringLiteral("1024"), 1024);
    m_dspBlockCombo->addItem(QStringLiteral("2048"), 2048);
    installWheelFilter(m_dspBlockCombo);
    form->addRow(QStringLiteral("DSP Block Size"), m_dspBlockCombo);

    auto* noteLabel = new QLabel(
        QStringLiteral("Changes are queued and applied on next channel rebuild."),
        box);
    noteLabel->setStyleSheet(QLatin1String(kNoteStyle));
    form->addRow(QString(), noteLabel);

    loadDspSettings();

    connect(m_dspRateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
                if (idx < 0) { return; }
                const int rate = m_dspRateCombo->itemData(idx).toInt();
                if (m_engine) {
                    m_engine->setDspSampleRate(rate);
                } else {
                    AppSettings::instance().setValue(
                        QStringLiteral("audio/DspRate"), QString::number(rate));
                }
            });

    connect(m_dspBlockCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
                if (idx < 0) { return; }
                const int block = m_dspBlockCombo->itemData(idx).toInt();
                if (m_engine) {
                    m_engine->setDspBlockSize(block);
                } else {
                    AppSettings::instance().setValue(
                        QStringLiteral("audio/DspBlockSize"), QString::number(block));
                }
            });

    contentLayout()->addWidget(box);
}

void AudioAdvancedPage::loadDspSettings()
{
    auto& s = AppSettings::instance();
    const int rate =
        s.value(QStringLiteral("audio/DspRate"), 48000).toInt();
    const int block =
        s.value(QStringLiteral("audio/DspBlockSize"), 1024).toInt();

    const int rateIdx = m_dspRateCombo->findData(rate);
    if (rateIdx >= 0) { m_dspRateCombo->setCurrentIndex(rateIdx); }

    const int blockIdx = m_dspBlockCombo->findData(block);
    if (blockIdx >= 0) { m_dspBlockCombo->setCurrentIndex(blockIdx); }
}

QString AudioAdvancedPage::remoteSendIqReason()
{
    return tr("Sending the receiver's I/Q to VAX is not available while connected to a Core.");
}

void AudioAdvancedPage::setStationSettingsAvailable(bool available, const QString& reason)
{
    gateStationControls({m_dspRateCombo, m_dspBlockCombo}, available, reason);
}

// ---------------------------------------------------------------------------
// Section 2: Feature flags
// ---------------------------------------------------------------------------

void AudioAdvancedPage::buildFeatureFlagsSection()
{
    auto* box = new QGroupBox(QStringLiteral("Feature Flags"), this);
    box->setStyleSheet(QLatin1String(kGroupStyle));
    box->setObjectName(QStringLiteral("audioAdvancedFeatureFlagsGroup"));
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(8, 16, 8, 8);
    layout->setSpacing(8);

    auto& s = AppSettings::instance();

    // SendIqToVax — Phase 3M deferred.
    {
        auto* row = new QHBoxLayout;
        m_sendIqToVaxCheck = new QCheckBox(QStringLiteral("Send IQ to VAX"), box);
        const bool on =
            s.value(QStringLiteral("audio/SendIqToVax"),
                    QStringLiteral("False")).toString() == QStringLiteral("True");
        m_sendIqToVaxCheck->setChecked(on);
        auto* note = new QLabel(
            QStringLiteral("(not available)"), box);
        note->setStyleSheet(QLatin1String(kNoteStyle));
        row->addWidget(m_sendIqToVaxCheck);
        row->addWidget(note);
        row->addStretch();
        layout->addLayout(row);

        connect(m_sendIqToVaxCheck, &QCheckBox::toggled,
                this, [](bool checked) {
                    AppSettings::instance().setValue(
                        QStringLiteral("audio/SendIqToVax"),
                        checked ? QStringLiteral("True") : QStringLiteral("False"));
                    if (checked) {
                        qCWarning(lcAudio)
                            << "SendIqToVax reserved for Phase 3M — no routing yet";
                    }
                });
    }

    // TxMonitorToVax — Phase 3M deferred.
    {
        auto* row = new QHBoxLayout;
        m_txMonitorToVaxCheck = new QCheckBox(QStringLiteral("TX Monitor to VAX"), box);
        const bool on =
            s.value(QStringLiteral("audio/TxMonitorToVax"),
                    QStringLiteral("False")).toString() == QStringLiteral("True");
        m_txMonitorToVaxCheck->setChecked(on);
        auto* note = new QLabel(
            QStringLiteral("(not available)"), box);
        note->setStyleSheet(QLatin1String(kNoteStyle));
        row->addWidget(m_txMonitorToVaxCheck);
        row->addWidget(note);
        row->addStretch();
        layout->addLayout(row);

        connect(m_txMonitorToVaxCheck, &QCheckBox::toggled,
                this, [](bool checked) {
                    AppSettings::instance().setValue(
                        QStringLiteral("audio/TxMonitorToVax"),
                        checked ? QStringLiteral("True") : QStringLiteral("False"));
                    if (checked) {
                        qCWarning(lcAudio)
                            << "TxMonitorToVax reserved for Phase 3M — no routing yet";
                    }
                });
    }

    // MuteVaxDuringTxOnOtherSlice — active; no note-inline.
    {
        m_muteVaxDuringTxOtherCheck =
            new QCheckBox(QStringLiteral("Mute VAX during TX on other slice"), box);
        const bool on =
            s.value(QStringLiteral("audio/MuteVaxDuringTxOnOtherSlice"),
                    QStringLiteral("False")).toString() == QStringLiteral("True");
        m_muteVaxDuringTxOtherCheck->setChecked(on);
        layout->addWidget(m_muteVaxDuringTxOtherCheck);

        connect(m_muteVaxDuringTxOtherCheck, &QCheckBox::toggled,
                this, [](bool checked) {
                    AppSettings::instance().setValue(
                        QStringLiteral("audio/MuteVaxDuringTxOnOtherSlice"),
                        checked ? QStringLiteral("True") : QStringLiteral("False"));
                    // TODO(sub-phase-12-vax-mute-live-apply): wire to
                    // cross-slice TX mute gate once Phase 3M TX path lands.
                    qCInfo(lcAudio)
                        << "MuteVaxDuringTxOnOtherSlice ="
                        << (checked ? "True" : "False")
                        << "(stored; live-apply deferred to Phase 3M TX path)";
                });
    }

    contentLayout()->addWidget(box);

    // R-R3-49: none of the three is applied yet; each is hidden until it is,
    // and the group goes with them while all three are.
    UnbuiltFeatures::hideRowUnlessBuilt(m_sendIqToVaxCheck, UnbuiltFeature::IqToVax);
    UnbuiltFeatures::hideRowUnlessBuilt(m_txMonitorToVaxCheck, UnbuiltFeature::IqToVax);
    UnbuiltFeatures::hideRowUnlessBuilt(m_muteVaxDuringTxOtherCheck,
                                        UnbuiltFeature::MuteVaxDuringTx);
    if (!UnbuiltFeatures::isBuilt(UnbuiltFeature::IqToVax)
        && !UnbuiltFeatures::isBuilt(UnbuiltFeature::MuteVaxDuringTx)) {
        box->setVisible(false);
    }
}

// ---------------------------------------------------------------------------
// Section 3: Detected cables + Rescan
// ---------------------------------------------------------------------------

void AudioAdvancedPage::buildCablesSection()
{
    auto* box = new QGroupBox(QStringLiteral("Detected Virtual Cables"), this);
    box->setStyleSheet(QLatin1String(kGroupStyle));
    auto* layout = new QHBoxLayout(box);
    layout->setContentsMargins(8, 16, 8, 8);
    layout->setSpacing(8);

    m_cablesLabel = new QLabel(QStringLiteral("Scanning…"), box);
    m_cablesLabel->setWordWrap(true);

    m_rescanButton = new QPushButton(QStringLiteral("Rescan"), box);
    m_rescanButton->setFixedWidth(80);

    layout->addWidget(m_cablesLabel, 1);
    layout->addWidget(m_rescanButton);

    // Initial scan.
    const QVector<DetectedCable> initial = VirtualCableDetector::scan();
    updateCablesLabel(initial);

    connect(m_rescanButton, &QPushButton::clicked,
            this, &AudioAdvancedPage::onRescan);

    contentLayout()->addWidget(box);
}

void AudioAdvancedPage::updateCablesLabel(const QVector<DetectedCable>& cables)
{
    if (cables.isEmpty()) {
        m_cablesLabel->setText(QStringLiteral("No virtual cables detected."));
        return;
    }
    QStringList names;
    for (const DetectedCable& c : cables) {
        names.append(c.deviceName +
                     (c.isInput ? QStringLiteral(" (input)")
                                : QStringLiteral(" (output)")));
    }
    m_cablesLabel->setText(
        QStringLiteral("%1 cable%2: %3")
            .arg(cables.size())
            .arg(cables.size() == 1 ? QString() : QStringLiteral("s"))
            .arg(names.join(QStringLiteral(", "))));
}

void AudioAdvancedPage::onRescan()
{
    const QVector<DetectedCable> current = VirtualCableDetector::scan();
    updateCablesLabel(current);

    auto& s = AppSettings::instance();
    const QString lastCsv =
        s.value(QStringLiteral("audio/LastDetectedCables"), QString()).toString();
    const QVector<DetectedCable> newCables =
        VirtualCableDetector::diffNewCables(current, lastCsv);

    // Update the stored fingerprint.
    s.setValue(QStringLiteral("audio/LastDetectedCables"),
               VirtualCableDetector::fingerprintCsv(current));
    s.save();

    if (!newCables.isEmpty()) {
        auto* dlg = new VaxFirstRunDialog(
            FirstRunScenario::RescanNewCables, newCables, this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->exec();
    }
}

// ---------------------------------------------------------------------------
// Section 4: Reset all audio to defaults
// ---------------------------------------------------------------------------

void AudioAdvancedPage::buildResetSection()
{
    auto* box = new QGroupBox(QStringLiteral("Reset"), this);
    box->setStyleSheet(QLatin1String(kGroupStyle));
    auto* layout = new QVBoxLayout(box);
    layout->setContentsMargins(8, 16, 8, 8);
    layout->setSpacing(6);

    m_resetButton = new QPushButton(
        QStringLiteral("Reset all audio to defaults…"), box);
    m_resetButton->setStyleSheet(QLatin1String(kAmberButtonStyle));

    layout->addWidget(m_resetButton, 0, Qt::AlignLeft);

    connect(m_resetButton, &QPushButton::clicked,
            this, &AudioAdvancedPage::onResetClicked);

    contentLayout()->addWidget(box);
}

void AudioAdvancedPage::onResetClicked()
{
    // Addendum §2.5 — verbatim confirm modal copy.
    // R-R3-21 (2026-09-24): "device bindings" reads "device choices".
    // R-R3-21 (2026-09-24): "DSP sample rate" reads "Audio processing rate".
    QMessageBox dlg(this);
    dlg.setWindowTitle(QStringLiteral("Reset all audio to defaults?"));
    dlg.setText(QStringLiteral("Reset all audio to defaults?"));
    dlg.setInformativeText(
        QStringLiteral(
            "This will clear:\n"
            "\u2022 All device choices (Speakers / Headphones / TX Input / VAX 1\u20134)\n"
            "\u2022 Audio processing rate and block size\n"
            "\u2022 Feature flags\n"
            "\n"
            "Your per-slice VAX channel assignments will be kept. "
            "The first-run setup will re-appear on next launch."));
    dlg.setIcon(QMessageBox::Warning);

    QPushButton* cancelBtn =
        dlg.addButton(tr("Cancel"), QMessageBox::RejectRole);
    QPushButton* resetBtn =
        dlg.addButton(tr("Reset all audio"), QMessageBox::DestructiveRole);
    resetBtn->setStyleSheet(QLatin1String(kAmberButtonStyle));
    dlg.setDefaultButton(cancelBtn);

    dlg.exec();

    if (dlg.clickedButton() != resetBtn) {
        return;
    }

    if (model() && !model()->ownsLocalDsp()) {
        // The engine belongs to this computer even in a remote window.
        // Rebuild all of its outputs, but leave the Core's DSP settings alone.
        if (m_engine) {
            m_engine->resetAudioSettings(true);
        }
    } else if (m_engine) {
        m_engine->resetAudioSettings();
    } else {
        // Engine not wired — do a direct settings clear (test or early-init path).
        // Delete all audio/* keys; slice/<N>/VaxChannel and tx/OwnerSlot are
        // implicitly safe because they live under different namespaces.
        auto& s = AppSettings::instance();
        const QStringList keys = s.allKeys();
        for (const QString& key : keys) {
            if (key.startsWith(QStringLiteral("audio/"))) {
                s.remove(key);
            }
        }
    }

    // Reload UI from (now-cleared) settings.
    // Block widget signals during the reload so combo currentIndexChanged and
    // checkbox toggled handlers don't fire and re-persist the defaults we just
    // cleared (avoids spurious "change queued" log spam and write-back cascade).
    {
        QSignalBlocker ba(m_dspRateCombo);
        QSignalBlocker bb(m_dspBlockCombo);
        loadDspSettings();
    }

    auto& s = AppSettings::instance();
    {
        QSignalBlocker bc(m_sendIqToVaxCheck);
        QSignalBlocker bd(m_txMonitorToVaxCheck);
        QSignalBlocker be(m_muteVaxDuringTxOtherCheck);
        const bool sendIq =
            s.value(QStringLiteral("audio/SendIqToVax"),
                    QStringLiteral("False")).toString() == QStringLiteral("True");
        m_sendIqToVaxCheck->setChecked(sendIq);
        const bool txMon =
            s.value(QStringLiteral("audio/TxMonitorToVax"),
                    QStringLiteral("False")).toString() == QStringLiteral("True");
        m_txMonitorToVaxCheck->setChecked(txMon);
        const bool muteVax =
            s.value(QStringLiteral("audio/MuteVaxDuringTxOnOtherSlice"),
                    QStringLiteral("False")).toString() == QStringLiteral("True");
        m_muteVaxDuringTxOtherCheck->setChecked(muteVax);
    }

    // Refresh cable readout.
    updateCablesLabel(VirtualCableDetector::scan());
}

// ---------------------------------------------------------------------------
// Event filter — block wheel scroll on un-focused combos in scroll area
// ---------------------------------------------------------------------------

void AudioAdvancedPage::installWheelFilter(QComboBox* combo)
{
    combo->installEventFilter(this);
}

bool AudioAdvancedPage::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::Wheel) {
        auto* combo = qobject_cast<QComboBox*>(obj);
        if (combo && !combo->hasFocus()) {
            event->ignore();
            return true;
        }
    }
    return SetupPage::eventFilter(obj, event);
}

} // namespace NereusSDR
