// =================================================================
// src/gui/setup/AudioAdvancedPage.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original Setup → Audio → Advanced page.
// See AudioAdvancedPage.h for the full header.
//
// Sub-Phase 12 Task 12.4 (2026-04-20): Written by J.J. Boyd (KG4VCF),
// AI-assisted via Anthropic Claude Code.
// =================================================================

#include "AudioAdvancedPage.h"

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/LogCategories.h"
#include "core/audio/VirtualCableDetector.h"
#include "gui/VaxFirstRunDialog.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
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
    , m_engine(model ? model->audioEngine() : nullptr)
{
    buildDspSection();
    buildVacFeedbackSection();
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

// ---------------------------------------------------------------------------
// Section 2 — VAC feedback-loop tuning
// ---------------------------------------------------------------------------

void AudioAdvancedPage::buildVacFeedbackSection()
{
    auto* box = new QGroupBox(QStringLiteral("VAC Feedback-Loop Tuning"), this);
    box->setStyleSheet(QLatin1String(kGroupStyle));
    auto* outerLayout = new QVBoxLayout(box);
    outerLayout->setContentsMargins(8, 16, 8, 8);
    outerLayout->setSpacing(6);

    // Target-channel selector row.
    auto* targetRow = new QHBoxLayout;
    auto* targetLabel = new QLabel(QStringLiteral("Target VAX Channel"), box);
    targetLabel->setMinimumWidth(140);
    m_vacTargetCombo = new QComboBox(box);
    m_vacTargetCombo->setStyleSheet(QLatin1String(kComboStyle));
    for (int ch = 1; ch <= 4; ++ch) {
        m_vacTargetCombo->addItem(QStringLiteral("VAX %1").arg(ch), ch);
    }
    installWheelFilter(m_vacTargetCombo);
    targetRow->addWidget(targetLabel);
    targetRow->addWidget(m_vacTargetCombo);
    targetRow->addStretch();
    outerLayout->addLayout(targetRow);

    // Parameter spinboxes.
    auto* form = new QFormLayout;
    form->setSpacing(6);

    m_vacGainSpin = new QDoubleSpinBox(box);
    m_vacGainSpin->setRange(0.0, 4.0);
    m_vacGainSpin->setSingleStep(0.05);
    m_vacGainSpin->setDecimals(3);
    m_vacGainSpin->setValue(1.0);
    form->addRow(QStringLiteral("Gain"), m_vacGainSpin);

    m_vacSlewSpin = new QSpinBox(box);
    m_vacSlewSpin->setRange(1, 100);
    m_vacSlewSpin->setSuffix(QStringLiteral(" ms"));
    m_vacSlewSpin->setValue(5);
    form->addRow(QStringLiteral("Slew Time"), m_vacSlewSpin);

    m_vacPropRingSpin = new QSpinBox(box);
    m_vacPropRingSpin->setRange(1, 16);
    m_vacPropRingSpin->setValue(2);
    form->addRow(QStringLiteral("Prop Ring"), m_vacPropRingSpin);

    m_vacFfRingSpin = new QSpinBox(box);
    m_vacFfRingSpin->setRange(1, 16);
    m_vacFfRingSpin->setValue(2);
    form->addRow(QStringLiteral("FF Ring"), m_vacFfRingSpin);

    auto* deferNote = new QLabel(
        QStringLiteral("Live-apply deferred to Phase 3M IVAC port."), box);
    deferNote->setStyleSheet(QLatin1String(kNoteStyle));
    form->addRow(QString(), deferNote);

    outerLayout->addLayout(form);

    // Load initial values for channel 1.
    loadVacFeedbackSettings(1);

    // Channel-change: load settings for the selected channel.
    connect(m_vacTargetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
                if (idx < 0) { return; }
                m_currentVacChannel = m_vacTargetCombo->itemData(idx).toInt();
                loadVacFeedbackSettings(m_currentVacChannel);
            });

    // Spinbox edits: immediately persist + call engine.
    auto persist = [this]() {
        if (m_vacLoading) { return; }
        AudioEngine::VacFeedbackParams p;
        p.gain       = static_cast<float>(m_vacGainSpin->value());
        p.slewTimeMs = m_vacSlewSpin->value();
        p.propRing   = m_vacPropRingSpin->value();
        p.ffRing     = m_vacFfRingSpin->value();
        if (m_engine) {
            m_engine->setVacFeedbackParams(m_currentVacChannel, p);
        } else {
            // Engine not wired yet — persist directly.
            const QString prefix =
                QStringLiteral("audio/VacFeedback/%1").arg(m_currentVacChannel);
            auto& s = AppSettings::instance();
            s.setValue(prefix + QStringLiteral("/Gain"),
                       QString::number(static_cast<double>(p.gain), 'f', 4));
            s.setValue(prefix + QStringLiteral("/SlewTimeMs"),
                       QString::number(p.slewTimeMs));
            s.setValue(prefix + QStringLiteral("/PropRing"),
                       QString::number(p.propRing));
            s.setValue(prefix + QStringLiteral("/FfRing"),
                       QString::number(p.ffRing));
        }
    };

    connect(m_vacGainSpin,     QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [persist](double) { persist(); });
    connect(m_vacSlewSpin,     QOverload<int>::of(&QSpinBox::valueChanged),
            this, [persist](int)    { persist(); });
    connect(m_vacPropRingSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [persist](int)    { persist(); });
    connect(m_vacFfRingSpin,   QOverload<int>::of(&QSpinBox::valueChanged),
            this, [persist](int)    { persist(); });

    contentLayout()->addWidget(box);
}

void AudioAdvancedPage::loadVacFeedbackSettings(int channel)
{
    m_vacLoading = true;
    const QString prefix =
        QStringLiteral("audio/VacFeedback/%1").arg(channel);
    auto& s = AppSettings::instance();

    m_vacGainSpin->setValue(
        s.value(prefix + QStringLiteral("/Gain"), 1.0).toDouble());
    m_vacSlewSpin->setValue(
        s.value(prefix + QStringLiteral("/SlewTimeMs"), 5).toInt());
    m_vacPropRingSpin->setValue(
        s.value(prefix + QStringLiteral("/PropRing"), 2).toInt());
    m_vacFfRingSpin->setValue(
        s.value(prefix + QStringLiteral("/FfRing"), 2).toInt());
    m_vacLoading = false;
}

// ---------------------------------------------------------------------------
// Section 3 — Feature flags
// ---------------------------------------------------------------------------

void AudioAdvancedPage::buildFeatureFlagsSection()
{
    auto* box = new QGroupBox(QStringLiteral("Feature Flags"), this);
    box->setStyleSheet(QLatin1String(kGroupStyle));
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
            QStringLiteral("(reserved for Phase 3M — no routing yet)"), box);
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
            QStringLiteral("(reserved for Phase 3M — no routing yet)"), box);
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
}

// ---------------------------------------------------------------------------
// Section 4 — Detected cables + Rescan
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
// Section 5 — Reset all audio to defaults
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
    QMessageBox dlg(this);
    dlg.setWindowTitle(QStringLiteral("Reset all audio to defaults?"));
    dlg.setText(QStringLiteral("Reset all audio to defaults?"));
    dlg.setInformativeText(
        QStringLiteral(
            "This will clear:\n"
            "\u2022 All device bindings (Speakers / Headphones / TX Input / VAX 1\u20134)\n"
            "\u2022 DSP sample rate and block size\n"
            "\u2022 VAC feedback-loop tuning\n"
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

    if (m_engine) {
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
    loadVacFeedbackSettings(m_currentVacChannel);

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
