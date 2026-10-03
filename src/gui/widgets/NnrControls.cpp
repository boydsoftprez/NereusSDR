// SPDX-License-Identifier: GPL-2.0-or-later
// NereusSDR-original WDSP 2.10 NNR controls.
// 2026-09-23: R-R3-40 runtime step-back notice and "Try again" action, by
// J.J. Boyd (KG4VCF), with Anthropic Claude Code assistance. Later the same
// day: one "try again" per model pick; the notice colour from
// StyleConstants.
// 2026-09-27: the model items, ranges, steps, places and units read from
// ControlRanges.h, the table the Core's catalogue sends (R-IOS-06,
// R-IOS-27); the values are unchanged. J.J. Boyd (KG4VCF), with Anthropic
// Claude Code assistance.

#include "NnrControls.h"

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/ControlRanges.h"
#include "core/WdspTypes.h"
#include "gui/OperatorReasonText.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>
#include <utility>

namespace NereusSDR {

namespace {

constexpr auto kAdvancedExpandedKey = "NnrControls/AdvancedExpanded";

constexpr auto kPanelStyle =
    "QWidget#NnrControls { background: #0f0f1a; }"
    "QGroupBox { border: 1px solid #304050; border-radius: 4px; margin-top: 8px;"
    " padding-top: 10px; color: #8aa8c0; font-weight: bold; }"
    "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
    "QLabel { color: #c8d8e8; font-size: 11px; }"
    "QComboBox, QDoubleSpinBox { background: #1a2a3a; color: #c8d8e8;"
    " border: 1px solid #304050; border-radius: 3px; padding: 2px 4px; }"
    "QPushButton, QToolButton { background: #1a2a3a; color: #c8d8e8;"
    " border: 1px solid #304050; border-radius: 3px; padding: 3px 8px; }"
    "QPushButton:hover, QToolButton:hover { background: #203040; border-color: #0090e0; }";

QDoubleSpinBox* makeDouble(const char* objectName, double minimum, double maximum,
                           double step, int decimals, const QString& suffix,
                           const QString& help, QWidget* parent)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setObjectName(QString::fromLatin1(objectName));
    spin->setRange(minimum, maximum);
    spin->setSingleStep(step);
    spin->setDecimals(decimals);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    spin->setToolTip(help);
    return spin;
}

// A spinbox drawn from its ControlRanges entry (NNR's values are in their
// properties' own units).
QDoubleSpinBox* makeDouble(const char* objectName, const ControlRanges::NrControl& control,
                           const QString& help, QWidget* parent)
{
    return makeDouble(objectName, control.min, control.max, control.step, control.decimals,
                      QString::fromUtf8(control.suffix), help, parent);
}

QString yesNo(bool value)
{
    return value ? QStringLiteral("yes") : QStringLiteral("no");
}

} // namespace

NnrControls::NnrControls(RadioModel* radio, SliceModel* slice,
                         Presentation presentation, QWidget* parent)
    : QWidget(parent), m_radio(radio)
{
    setObjectName(QStringLiteral("NnrControls"));
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(QString::fromLatin1(kPanelStyle));
    buildUi(presentation);

    if (m_radio) {
        connect(m_radio, &RadioModel::connectionStateChanged, this,
                [this](ConnectionState) { invalidateBinding(); });
        connect(m_radio, &RadioModel::stationLinkStateChanged, this,
                &NnrControls::invalidateBinding);
        connect(m_radio, &RadioModel::sliceRemoved, this, [this](int sliceId) {
            if (sliceId == m_sliceId) {
                invalidateBinding();
            }
        });
    }
    bindSlice(slice);
}

void NnrControls::buildUi(Presentation presentation)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto* quick = new QGroupBox(tr("Neural noise reduction"), this);
    auto* quickForm = new QFormLayout(quick);
    quickForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_model = new QComboBox(quick);
    m_model->setObjectName(QStringLiteral("nnrModelCombo"));
    for (const ControlRanges::NrChoiceItem& item : ControlRanges::kNnrModels) {
        m_model->addItem(tr(item.label), item.id);
    }
    m_model->setToolTip(tr("Select the neural model. The preference remains editable offline; "
                           "a connected radio may reject a model that is not installed."));
    quickForm->addRow(tr("Model"), m_model);

    // R-R3-40: shown only while the Core holds this receiver below the saved
    // choice because the computer could not keep up.
    m_limitRow = new QWidget(quick);
    m_limitRow->setObjectName(QStringLiteral("nnrLimitRow"));
    auto* limitLayout = new QHBoxLayout(m_limitRow);
    limitLayout->setContentsMargins(0, 0, 0, 0);
    m_limitNotice = new QLabel(m_limitRow);
    m_limitNotice->setObjectName(QStringLiteral("nnrLimitNotice"));
    m_limitNotice->setWordWrap(true);
    m_limitNotice->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                                     .arg(QLatin1String(Style::kAmberText)));
    m_tryAgain = new QPushButton(tr("Try again"), m_limitRow);
    m_tryAgain->setObjectName(QStringLiteral("nnrTryAgainButton"));
    m_tryAgain->setToolTip(tr("Run the saved noise reduction choice again."));
    limitLayout->addWidget(m_limitNotice, 1);
    limitLayout->addWidget(m_tryAgain);
    m_limitRow->setVisible(false);
    quickForm->addRow(m_limitRow);

    auto* suppression = new QWidget(quick);
    auto* suppressionRow = new QHBoxLayout(suppression);
    suppressionRow->setContentsMargins(0, 0, 0, 0);
    m_maskFloorSlider = new QSlider(Qt::Horizontal, suppression);
    m_maskFloorSlider->setObjectName(QStringLiteral("nnrMaskFloorSlider"));
    m_maskFloorSlider->setRange(
        qRound(ControlRanges::kNnrMaskFloor.min * ControlRanges::kNnrMaskFloorSliderPerDb),
        qRound(ControlRanges::kNnrMaskFloor.max * ControlRanges::kNnrMaskFloorSliderPerDb));
    m_maskFloorSlider->setSingleStep(1);
    m_maskFloor = makeDouble("nnrMaskFloorSpin", ControlRanges::kNnrMaskFloor,
                             tr("More-negative values permit stronger suppression. Higher values retain "
                                "more of the original signal and noise."),
                             suppression);
    suppressionRow->addWidget(m_maskFloorSlider, 1);
    suppressionRow->addWidget(m_maskFloor);
    quickForm->addRow(tr("Suppression"), suppression);
    root->addWidget(quick);

    if (presentation == Presentation::Compact) {
        m_advancedToggle = new QToolButton(this);
        m_advancedToggle->setObjectName(QStringLiteral("nnrAdvancedToggle"));
        m_advancedToggle->setText(tr("Advanced settings"));
        m_advancedToggle->setCheckable(true);
        m_advancedToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_advancedToggle->setArrowType(Qt::RightArrow);
        root->addWidget(m_advancedToggle);
    }

    auto* advancedGroup = new QGroupBox(tr("Advanced"), this);
    m_advanced = advancedGroup;
    auto* advanced = new QFormLayout(advancedGroup);
    advanced->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_position = new QComboBox(advancedGroup);
    m_position->setObjectName(QStringLiteral("nnrPositionCombo"));
    for (std::size_t i = 0; i < ControlRanges::kNnrPosition.optionCount; ++i) {
        const ControlRanges::NrChoiceItem& item = ControlRanges::kNnrPosition.options[i];
        m_position->addItem(tr(item.label), item.id);
    }
    m_position->setToolTip(tr("Choose whether NNR runs before or after automatic gain control."));
    advanced->addRow(tr("Position"), m_position);

    m_alpha = makeDouble(
        "nnrAlphaSpin", ControlRanges::kNnrAlpha,
        tr("Shapes deep-filter gains below the alpha knee. A value of 1 leaves "
           "those gains unchanged; values above 1 deepen them, while values "
           "below 1 lift them."),
        advancedGroup);
    m_alphaKnee = makeDouble("nnrAlphaKneeSpin", ControlRanges::kNnrAlphaKnee,
                             tr("Gain threshold below which Alpha reshapes the "
                                "deep-filter response."),
                             advancedGroup);
    m_tau = makeDouble("nnrTauSpin", ControlRanges::kNnrTau,
                       tr("Time constant for input-power normalization before "
                          "the neural network."),
                       advancedGroup);
    m_maxGain = makeDouble("nnrMaxGainSpin", ControlRanges::kNnrMaxGain,
                           tr("Maximum gain the neural stage may add."), advancedGroup);
    m_attack = makeDouble("nnrAttackSpin", ControlRanges::kNnrAttack,
                          tr("How quickly suppression engages. Zero uses the model response."), advancedGroup);
    m_release = makeDouble("nnrReleaseSpin", ControlRanges::kNnrRelease,
                           tr("How quickly suppression relaxes. Zero uses the model response."), advancedGroup);
    advanced->addRow(tr("Alpha"), m_alpha);
    advanced->addRow(tr("Alpha knee"), m_alphaKnee);
    advanced->addRow(tr("Noise time"), m_tau);
    advanced->addRow(tr("Maximum gain"), m_maxGain);
    advanced->addRow(tr("Attack"), m_attack);
    advanced->addRow(tr("Release"), m_release);
    root->addWidget(advancedGroup);

    if (m_advancedToggle) {
        advancedGroup->setObjectName(QStringLiteral("nnrAdvancedGroup"));
        const bool expanded =
            AppSettings::instance()
                .value(QString::fromLatin1(kAdvancedExpandedKey), QStringLiteral("False"))
                .toString()
            == QStringLiteral("True");
        m_advancedToggle->setChecked(expanded);
        m_advancedToggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        advancedGroup->setVisible(expanded);
        connect(m_advancedToggle, &QToolButton::toggled, this, [this](bool shown) {
            m_advanced->setVisible(shown);
            m_advancedToggle->setArrowType(shown ? Qt::DownArrow : Qt::RightArrow);
            AppSettings::instance().setValue(
                QString::fromLatin1(kAdvancedExpandedKey),
                shown ? QStringLiteral("True") : QStringLiteral("False"));
            window()->adjustSize();
        });
    }

    auto* diagnostics = new QGroupBox(tr("Diagnostics"), this);
    auto* diagnosticLayout = new QVBoxLayout(diagnostics);
    m_sessionBadge = new QLabel(tr("TEMPORARY: resets when this app reconnects to the Core"), diagnostics);
    m_sessionBadge->setObjectName(QStringLiteral("nnrSessionOnlyBadge"));
    m_sessionBadge->setStyleSheet(QStringLiteral(
        "QLabel { color: #ffd166; background: #302810; border: 1px solid #806820;"
        " border-radius: 3px; padding: 3px; font-size: 10px; font-weight: bold; }"));
    diagnosticLayout->addWidget(m_sessionBadge);

    auto* readbacks = new QFormLayout;
    m_runtime = new QLabel(diagnostics); m_runtime->setObjectName(QStringLiteral("nnrRuntimeReadback"));
    m_models = new QLabel(diagnostics); m_models->setObjectName(QStringLiteral("nnrModelsReadback"));
    m_rateLatency = new QLabel(diagnostics); m_rateLatency->setObjectName(QStringLiteral("nnrRateLatencyReadback"));
    m_source = new QLabel(diagnostics); m_source->setObjectName(QStringLiteral("nnrSourceReadback"));
    m_status = new QLabel(diagnostics); m_status->setObjectName(QStringLiteral("nnrStatusReadback"));
    m_status->setWordWrap(true);
    readbacks->addRow(tr("Runtime"), m_runtime);
    readbacks->addRow(tr("Models"), m_models);
    readbacks->addRow(tr("Rate / latency"), m_rateLatency);
    readbacks->addRow(tr("Source"), m_source);
    readbacks->addRow(tr("Status"), m_status);
    diagnosticLayout->addLayout(readbacks);

    auto* diagnosticActions = new QHBoxLayout;
    m_testMode = new QComboBox(diagnostics);
    m_testMode->setObjectName(QStringLiteral("nnrTestModeCombo"));
    m_testMode->addItem(tr("Network"), 0);
    m_testMode->addItem(tr("Identity"), 1);
    m_testMode->addItem(tr("Low-pass"), 2);
    m_testMode->setItemData(
        0, tr("Run the selected neural network and deep-filter head."), Qt::ToolTipRole);
    m_testMode->setItemData(
        1, tr("Diagnostic bypass: pass the spectrum through unchanged inside the NNR path."),
        Qt::ToolTipRole);
    m_testMode->setItemData(
        2, tr("Diagnostic filter: keep the lower half of the network bins and zero the upper half."),
        Qt::ToolTipRole);
    m_testMode->setToolTip(
        tr("Temporary NNR processing mode until this app reconnects to the Core."));
    m_outputMode = new QComboBox(diagnostics);
    m_outputMode->setObjectName(QStringLiteral("nnrOutputModeCombo"));
    m_outputMode->addItem(tr("Duplicate I/Q"), 0);
    m_outputMode->addItem(tr("Q zero"), 1);
    m_outputMode->setItemData(
        0, tr("Write the same real NNR result to both I and Q output components."),
        Qt::ToolTipRole);
    m_outputMode->setItemData(
        1, tr("Write the NNR result to I and write zero to Q."), Qt::ToolTipRole);
    m_outputMode->setToolTip(
        tr("Temporary output mapping until this app reconnects to the Core."));
    m_applyDiagnostic = new QPushButton(tr("Apply until reconnect"), diagnostics);
    m_applyDiagnostic->setObjectName(QStringLiteral("nnrApplyDiagnosticButton"));
    diagnosticActions->addWidget(m_testMode);
    diagnosticActions->addWidget(m_outputMode);
    diagnosticActions->addWidget(m_applyDiagnostic);
    diagnosticLayout->addLayout(diagnosticActions);
    root->addWidget(diagnostics);

    auto* actions = new QHBoxLayout;
    m_modelsButton = new QPushButton(tr("Models…"), this);
    m_modelsButton->setObjectName(QStringLiteral("nnrModelsButton"));
    m_reset = new QPushButton(tr("Reset tuning"), this);
    m_reset->setObjectName(QStringLiteral("nnrResetButton"));
    actions->addWidget(m_modelsButton);
    actions->addWidget(m_reset);
    if (presentation == Presentation::Compact) {
        m_moreButton = new QPushButton(tr("More settings…"), this);
        m_moreButton->setObjectName(QStringLiteral("nnrMoreSettingsButton"));
        actions->addWidget(m_moreButton);
    }
    root->addLayout(actions);

    connect(m_model, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_slice && index >= 0) {
            m_slice->setNnrModelSlot(m_model->itemData(index).toInt());
            // R-R3-40: a pick signals a changed index and then an
            // activation. This pick has asked already; the activation that
            // follows in the same interaction must not ask again (a remote
            // GUI would send "try again" twice).
            m_modelPickHandled = true;
            QMetaObject::invokeMethod(this, [this] { m_modelPickHandled = false; },
                                      Qt::QueuedConnection);
        }
        refresh();
    });
    // Choosing the model already shown changes no index; while a step-back
    // is in force it is still the operator asking for that model again.
    connect(m_model, &QComboBox::activated, this, [this](int index) {
        const bool handled = std::exchange(m_modelPickHandled, false);
        if (!handled && m_slice && index >= 0 && m_slice->nnrLimit() != 0) {
            m_slice->setNnrModelSlot(m_model->itemData(index).toInt());
        }
        refresh();
    });
    connect(m_tryAgain, &QPushButton::clicked, this, [this] {
        if (m_slice) {
            m_slice->requestNnrRetry();
        }
        refresh();
    });
    connect(m_maskFloor, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        if (m_slice) {
            m_slice->setNnrMaskFloorDb(value);
        }
        refresh();
    });
    connect(m_maskFloorSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_slice) {
            m_slice->setNnrMaskFloorDb(double(value) / ControlRanges::kNnrMaskFloorSliderPerDb);
        }
        refresh();
    });
    connect(m_position, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_slice && index >= 0) {
            m_slice->setNnrPosition(static_cast<NrPosition>(m_position->itemData(index).toInt()));
        }
        refresh();
    });
    const auto wireDouble = [this](QDoubleSpinBox* spin, auto setter) {
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this, setter](double value) {
            if (m_slice) {
                (m_slice.data()->*setter)(value);
            }
            refresh();
        });
    };
    wireDouble(m_alpha, &SliceModel::setNnrAlpha);
    wireDouble(m_alphaKnee, &SliceModel::setNnrAlphaKneeDb);
    wireDouble(m_tau, &SliceModel::setNnrTauSeconds);
    wireDouble(m_maxGain, &SliceModel::setNnrMaxGainDb);
    wireDouble(m_attack, &SliceModel::setNnrAttackMs);
    wireDouble(m_release, &SliceModel::setNnrReleaseMs);

    connect(m_applyDiagnostic, &QPushButton::clicked, this, [this] {
        if (!m_slice) {
            return;
        }
        m_slice->requestNnrDiagnostics(m_testMode->currentData().toInt(),
                                       m_outputMode->currentData().toInt());
        refresh();
    });
    connect(m_reset, &QPushButton::clicked, this, [this] {
        if (m_slice) {
            m_slice->resetNnrTuning();
        }
        refresh();
    });
    connect(m_modelsButton, &QPushButton::clicked, this, [this] {
        if (m_slice) {
            emit openModelsRequested(m_sliceId);
        }
    });
    if (m_moreButton) {
        connect(m_moreButton, &QPushButton::clicked, this, [this] {
            if (m_slice) {
                emit openMoreSettingsRequested(m_sliceId);
            }
        });
    }
}

void NnrControls::bindSlice(SliceModel* slice)
{
    for (const auto& connection : std::as_const(m_sliceConnections)) {
        disconnect(connection);
    }
    m_sliceConnections.clear();
    m_slice = slice;
    m_sliceId = slice ? slice->sliceIndex() : -1;
    setInteractive(slice != nullptr);
    if (slice) {
        connectSlice();
    }
    refresh();
}

void NnrControls::connectSlice()
{
    auto refreshConnection = [this](auto signal) {
        m_sliceConnections.push_back(connect(m_slice.data(), signal, this,
                                             [this] { refresh(); }));
    };
    refreshConnection(&SliceModel::nnrConfigurationChanged);
    refreshConnection(&SliceModel::nnrDiagnosticsChanged);
    refreshConnection(&SliceModel::nnrLastErrorChanged);
    refreshConnection(&SliceModel::nnrLimitChanged);
    m_sliceConnections.push_back(connect(m_slice.data(), &SliceModel::nnrEditRejected,
                                         this, [this](const QString&) { refresh(); }));
    m_sliceConnections.push_back(connect(m_slice.data(), &QObject::destroyed,
                                         this, &NnrControls::invalidateBinding));
}

void NnrControls::setInteractive(bool enabled)
{
    const QVector<QWidget*> widgets{
        m_model, m_maskFloor, m_maskFloorSlider, m_position, m_alpha, m_alphaKnee,
        m_tau, m_maxGain, m_attack, m_release, m_testMode, m_outputMode,
        m_reset, m_modelsButton, m_moreButton, m_tryAgain
    };
    for (QWidget* widget : widgets) {
        if (widget) {
            widget->setEnabled(enabled);
        }
    }
    if (!enabled && m_applyDiagnostic) {
        m_applyDiagnostic->setEnabled(false);
    }
}

void NnrControls::refresh()
{
    if (!m_slice) {
        m_runtime->setText(tr("closed"));
        m_models->setText(QStringLiteral("—"));
        m_rateLatency->setText(QStringLiteral("—"));
        m_source->setText(QStringLiteral("—"));
        m_status->setText(tr("Reopen NNR controls to see this receiver again."));
        m_limitRow->setVisible(false);
        return;
    }

    const QString limitText = m_slice->nnrLimitText();
    m_limitNotice->setText(limitText);
    m_limitNotice->setToolTip(limitText);
    m_limitRow->setVisible(!limitText.isEmpty());

    const auto setComboData = [](QComboBox* combo, int value) {
        QSignalBlocker blocker(combo);
        combo->setCurrentIndex(combo->findData(value));
    };
    const auto setDouble = [](QDoubleSpinBox* spin, double value) {
        QSignalBlocker blocker(spin);
        spin->setValue(value);
    };
    setComboData(m_model, m_slice->nnrModelSlot());
    setDouble(m_maskFloor, m_slice->nnrMaskFloorDb());
    {
        QSignalBlocker blocker(m_maskFloorSlider);
        m_maskFloorSlider->setValue(
            qRound(m_slice->nnrMaskFloorDb() * ControlRanges::kNnrMaskFloorSliderPerDb));
    }
    setComboData(m_position, static_cast<int>(m_slice->nnrPosition()));
    setDouble(m_alpha, m_slice->nnrAlpha());
    setDouble(m_alphaKnee, m_slice->nnrAlphaKneeDb());
    setDouble(m_tau, m_slice->nnrTauSeconds());
    setDouble(m_maxGain, m_slice->nnrMaxGainDb());
    setDouble(m_attack, m_slice->nnrAttackMs());
    setDouble(m_release, m_slice->nnrReleaseMs());

    const int selected = m_slice->nnrModelSlot();
    const bool selectedAvailable = selected == 0 ? m_slice->nnrStandardAvailable()
                                                  : m_slice->nnrPremiumAvailable();
    m_model->setToolTip(!m_slice->nnrAvailable() || selectedAvailable
        ? tr("This preference remains editable while offline.")
        : tr("The selected model is not available in the live runtime; the runtime may reject it."));

    m_runtime->setText(tr("available %1 · ready %2 · running %3")
                           .arg(yesNo(m_slice->nnrAvailable()), yesNo(m_slice->nnrReady()),
                                yesNo(m_slice->nnrRunning())));
    m_models->setText(tr("standard %1 · premium %2 · actual %3")
                          .arg(yesNo(m_slice->nnrStandardAvailable()),
                               yesNo(m_slice->nnrPremiumAvailable()))
                          .arg(m_slice->nnrActualModelSlot()));
    m_rateLatency->setText(tr("NNR processing %1 Hz / model %2 Hz (%3) · %4 samples · %5 ms")
                               .arg(m_slice->nnrDspRateHz())
                               .arg(m_slice->nnrNetworkRateHz())
                               .arg(m_slice->nnrRateSupported() ? tr("supported") : tr("unsupported"))
                               .arg(m_slice->nnrDelaySamples())
                               .arg(m_slice->nnrLatencyMs(), 0, 'f', 2));
    m_source->setText(m_slice->nnrModelSource().isEmpty() ? QStringLiteral("—")
                                                          : m_slice->nnrModelSource());
    const QString profiling = m_slice->nnrProfilingAvailable()
        ? tr("Profiling available. ")
        : tr("Profiling unavailable in this build. ");
    // A refusal or status from the Core is shown in user words; the raw
    // text is logged (R-R3-21).
    const QString detail = !m_slice->nnrLastError().isEmpty()
        ? m_slice->nnrLastError() : m_slice->nnrStatus();
    m_status->setText(profiling
        + (detail.isEmpty() ? detail : OperatorReasonText::forDisplay(detail)));
    setComboData(m_testMode, m_slice->nnrTestMode());
    setComboData(m_outputMode, m_slice->nnrOutputMode());
    m_applyDiagnostic->setEnabled(m_slice->nnrReady());
}

void NnrControls::invalidateBinding()
{
    if (!m_slice) {
        return;
    }
    for (const auto& connection : std::as_const(m_sliceConnections)) {
        disconnect(connection);
    }
    m_sliceConnections.clear();
    m_slice.clear();
    setInteractive(false);
    refresh();
    emit bindingInvalidated();
}

} // namespace NereusSDR
