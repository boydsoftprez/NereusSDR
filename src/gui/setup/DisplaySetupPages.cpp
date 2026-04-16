#include "DisplaySetupPages.h"
#include "SetupHelpers.h"
#include "gui/ColorSwatchButton.h"
#include "gui/SpectrumWidget.h"
#include "gui/StyleConstants.h"
#include "core/FFTEngine.h"
#include "core/ClarityController.h"
#include "core/AppSettings.h"
#include "models/Band.h"
#include "models/PanadapterModel.h"
#include "models/RadioModel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QThread>
#include <QPushButton>
#include <QMessageBox>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Helpers (file-local)
// ---------------------------------------------------------------------------

namespace {

// Apply the project-wide dark-theme stylesheet to a widget.
void applyDarkStyle(QWidget* w)
{
    w->setStyleSheet(QStringLiteral(
        "QGroupBox { color: #8090a0; font-size: 11px;"
        "  border: 1px solid #203040; border-radius: 4px;"
        "  margin-top: 8px; padding-top: 4px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
        "QLabel { color: #c8d8e8; }"
        "QComboBox { background: #1a2a3a; color: #c8d8e8; border: 1px solid #203040;"
        "  border-radius: 3px; padding: 2px 6px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #1a2a3a; color: #c8d8e8;"
        "  selection-background-color: #00b4d8; }"
        "QSlider::groove:horizontal { background: #1a2a3a; height: 4px; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #00b4d8; width: 12px; margin: -4px 0;"
        "  border-radius: 6px; }"
        "QSlider::sub-page:horizontal { background: #00b4d8; border-radius: 2px; }"
        "QSpinBox, QDoubleSpinBox { background: #1a2a3a; color: #c8d8e8;"
        "  border: 1px solid #203040; border-radius: 3px; padding: 1px 4px; }"
        "QSpinBox::up-button, QSpinBox::down-button,"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button"
        "  { background: #203040; border: none; }"
        "QCheckBox { color: #c8d8e8; }"
        "QCheckBox::indicator { width: 14px; height: 14px; background: #1a2a3a;"
        "  border: 1px solid #203040; border-radius: 2px; }"
        "QCheckBox::indicator:checked { background: #00b4d8; border-color: #00b4d8; }"
    ));
}

// Build a color swatch placeholder label (NYI — no color picker yet).
QLabel* makeColorSwatch(const QString& label, const QString& hexColor, QWidget* parent)
{
    auto* lbl = new QLabel(QStringLiteral("  %1  ").arg(label), parent);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; color: #c8d8e8; border: 1px solid #203040;"
        " border-radius: 3px; padding: 2px 6px; }").arg(hexColor));
    lbl->setFixedHeight(24);
    lbl->setEnabled(false);  // NYI
    lbl->setToolTip(QStringLiteral("Color picker — not yet implemented"));
    return lbl;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// SpectrumDefaultsPage
// ---------------------------------------------------------------------------

SpectrumDefaultsPage::SpectrumDefaultsPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Spectrum Defaults"), model, parent)
{
    buildUI();
    loadFromRenderer();
}

// Maps SpectrumDefaultsPage FPS slider (10-60) to FFTEngine output FPS.
void SpectrumDefaultsPage::pushFps(int fps)
{
    if (!model() || !model()->fftEngine()) { return; }
    model()->fftEngine()->setOutputFps(fps);
}

void SpectrumDefaultsPage::loadFromRenderer()
{
    if (!model()) { return; }
    auto* sw = model()->spectrumWidget();
    auto* fe = model()->fftEngine();
    if (!sw || !fe) { return; }

    QSignalBlocker b1(m_fftSizeCombo);
    QSignalBlocker b2(m_windowCombo);
    QSignalBlocker b3(m_fpSlider);
    QSignalBlocker b4(m_averagingCombo);
    QSignalBlocker b5(m_fillToggle);
    QSignalBlocker b6(m_fillAlphaSlider);
    QSignalBlocker b7(m_lineWidthSlider);
    QSignalBlocker b8(m_gradientToggle);
    QSignalBlocker b9(m_dataLineAlphaSlider);
    QSignalBlocker b10(m_calOffsetSpin);
    QSignalBlocker b11(m_peakHoldToggle);
    QSignalBlocker b12(m_peakHoldDelaySpin);
    QSignalBlocker b13(m_threadPriorityCombo);

    // FFT size — map actual FFT size to combo index.
    const int fs = fe->fftSize();
    const QString fsText = QString::number(fs);
    const int idx = m_fftSizeCombo->findText(fsText);
    if (idx >= 0) { m_fftSizeCombo->setCurrentIndex(idx); }

    // FFT window — enum → index (5 enum values but 4 UI options; map
    // BlackmanHarris4 → Blackman-Harris, Hanning → Hann, Hamming → Hamming,
    // Flat → Flat-Top. BlackmanHarris7 / Kaiser / None fall back to BH4).
    switch (fe->windowFunction()) {
        case WindowFunction::Hanning: m_windowCombo->setCurrentIndex(1); break;
        case WindowFunction::Hamming: m_windowCombo->setCurrentIndex(2); break;
        case WindowFunction::Flat:    m_windowCombo->setCurrentIndex(3); break;
        default:                      m_windowCombo->setCurrentIndex(0); break;
    }

    m_fpSlider->setValue(fe->outputFps());
    m_averagingCombo->setCurrentIndex(static_cast<int>(sw->averageMode()));
    m_fillToggle->setChecked(sw->panFillEnabled());
    m_fillAlphaSlider->setValue(static_cast<int>(sw->fillAlpha() * 100.0f));
    m_lineWidthSlider->setValue(qBound(1, static_cast<int>(sw->lineWidth()), 3));
    m_gradientToggle->setChecked(sw->gradientEnabled());
    m_dataLineAlphaSlider->setValue(sw->fillColor().alpha());
    m_calOffsetSpin->setValue(static_cast<double>(sw->dbmCalOffset()));
    m_peakHoldToggle->setChecked(sw->peakHoldEnabled());
    m_peakHoldDelaySpin->setValue(sw->peakHoldDelayMs());

    if (m_dataLineColorBtn) { m_dataLineColorBtn->setColor(sw->fillColor()); }
    if (m_dataFillColorBtn) { m_dataFillColorBtn->setColor(sw->fillColor()); }
}

void SpectrumDefaultsPage::buildUI()
{
    applyDarkStyle(this);

    // Phase 3G-9b: Reset to Smooth Defaults button. Destructive — shows
    // a confirmation dialog before overwriting because it resets the
    // Spectrum / Waterfall display state.
    auto* resetBtn = new QPushButton(QStringLiteral("Reset to Smooth Defaults"), this);
    resetBtn->setToolTip(QStringLiteral(
        "Overwrite the Spectrum and Waterfall display settings with the "
        "NereusSDR smooth-default profile (Clarity Blue palette, "
        "log-recursive averaging, tight threshold gap, waterfall AGC on). "
        "Intended to recover the out-of-box look after experimentation. "
        "FFT size, frequency, band stack, and per-band grid slots are "
        "not affected."));
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        const auto rc = QMessageBox::question(
            this,
            QStringLiteral("Reset to Smooth Defaults"),
            QStringLiteral(
                "This will overwrite your current Spectrum and Waterfall "
                "display settings with the NereusSDR smooth-default profile.\n\n"
                "Your FFT size, frequency, band stack, and per-band grid "
                "slots are NOT affected.\n\n"
                "Continue?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (rc != QMessageBox::Yes) { return; }
        if (model()) {
            model()->applyClaritySmoothDefaults();
            // Reload this page so the controls immediately reflect the
            // new model state. Sibling pages (Waterfall Defaults, Grid &
            // Scales) will update on their next SetupDialog repaint
            // because their loadFromRenderer() reads from the model.
            loadFromRenderer();
        }
    });
    contentLayout()->addWidget(resetBtn);

    // Phase 3G-9c: Clarity adaptive display tuning master toggle.
    // When on, ClarityController drives Waterfall Low/High thresholds
    // from a percentile-based noise-floor estimate. When off, thresholds
    // stay at whatever value they were last set to (no AGC fallback).
    auto* clarityToggle = new QCheckBox(
        QStringLiteral("Enable Clarity (adaptive waterfall tuning)"), this);
    clarityToggle->setToolTip(QStringLiteral(
        "Clarity keeps the waterfall thresholds centered on the actual "
        "noise floor as band conditions and tuning change. Uses a 30th-"
        "percentile estimator with 3-second EWMA smoothing and a ±2 dB "
        "deadband. When off, thresholds are fixed at their last values."));
    if (auto* cc = model() ? model()->clarityController() : nullptr) {
        clarityToggle->setChecked(cc->isEnabled());
        connect(clarityToggle, &QCheckBox::toggled, this, [this](bool on) {
            if (auto* cc2 = model() ? model()->clarityController() : nullptr) {
                cc2->setEnabled(on);
                AppSettings::instance().setValue(
                    QStringLiteral("ClarityEnabled"),
                    on ? QStringLiteral("True") : QStringLiteral("False"));
            }
        });
    }
    contentLayout()->addWidget(clarityToggle);

    auto* sw = model() ? model()->spectrumWidget() : nullptr;
    auto* fe = model() ? model()->fftEngine() : nullptr;

    // --- Section: FFT ---
    auto* fftGroup = new QGroupBox(QStringLiteral("FFT"), this);
    auto* fftForm  = new QFormLayout(fftGroup);
    fftForm->setSpacing(6);

    m_fftSizeCombo = new QComboBox(fftGroup);
    m_fftSizeCombo->addItems({QStringLiteral("1024"), QStringLiteral("2048"),
                              QStringLiteral("4096"), QStringLiteral("8192"),
                              QStringLiteral("16384")});
    m_fftSizeCombo->setCurrentText(QStringLiteral("4096"));
    // Thetis: setup.designer.cs:35043 (tbDisplayFFTSize) — no upstream tooltip; rewritten
    // Thetis original: (none)
    m_fftSizeCombo->setToolTip(QStringLiteral("FFT size used for spectrum analysis. Larger = finer frequency resolution at higher CPU cost."));
    connect(m_fftSizeCombo, &QComboBox::currentTextChanged,
            this, [this](const QString& txt) {
        if (model() && model()->fftEngine()) {
            model()->fftEngine()->setFftSize(txt.toInt());
        }
    });
    fftForm->addRow(QStringLiteral("FFT Size:"), m_fftSizeCombo);

    m_windowCombo = new QComboBox(fftGroup);
    m_windowCombo->addItems({QStringLiteral("Blackman-Harris"), QStringLiteral("Hann"),
                             QStringLiteral("Hamming"),         QStringLiteral("Flat-Top")});
    // NereusSDR extension — no Thetis equivalent (Thetis hardcodes Blackman-Harris 4-term)
    m_windowCombo->setToolTip(QStringLiteral("FFT window function. Blackman-Harris offers best sidelobe rejection; Flat-Top is best for amplitude accuracy."));
    connect(m_windowCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        if (!model() || !model()->fftEngine()) { return; }
        WindowFunction wf = WindowFunction::BlackmanHarris4;
        switch (i) {
            case 0: wf = WindowFunction::BlackmanHarris4; break;
            case 1: wf = WindowFunction::Hanning; break;
            case 2: wf = WindowFunction::Hamming; break;
            case 3: wf = WindowFunction::Flat; break;
        }
        model()->fftEngine()->setWindowFunction(wf);
    });
    fftForm->addRow(QStringLiteral("Window:"), m_windowCombo);

    contentLayout()->addWidget(fftGroup);

    // --- Section: Rendering ---
    auto* renderGroup = new QGroupBox(QStringLiteral("Rendering"), this);
    auto* renderForm  = new QFormLayout(renderGroup);
    renderForm->setSpacing(6);

    {
        auto row = makeSliderRow(10, 60, 30, QStringLiteral(" fps"), renderGroup);
        m_fpSlider = row.slider;
        // Thetis: setup.designer.cs:33856 (udDisplayFPS) — Thetis original: "Frames Per Second (approximate)" (placeholder); rewritten
        m_fpSlider->setToolTip(QStringLiteral("Spectrum/waterfall redraw rate. Higher = smoother animation at cost of CPU."));
        row.spin->setToolTip(QStringLiteral("Spectrum/waterfall redraw rate. Higher = smoother animation at cost of CPU."));
        connect(m_fpSlider, &QSlider::valueChanged, this, [this](int v) { pushFps(v); });
        renderForm->addRow(QStringLiteral("FPS:"), row.container);
    }

    m_averagingCombo = new QComboBox(renderGroup);
    m_averagingCombo->addItems({QStringLiteral("None"), QStringLiteral("Weighted"),
                                QStringLiteral("Logarithmic"), QStringLiteral("Time Window")});
    // Thetis: setup.designer.cs:34835 (comboDispPanAveraging) — no upstream tooltip; rewritten
    // Thetis original: (none)
    m_averagingCombo->setToolTip(QStringLiteral("Panadapter spectrum averaging mode. Weighted and Time Window smooth the display; None shows raw FFT output."));
    connect(m_averagingCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setAverageMode(static_cast<AverageMode>(i));
        }
    });
    renderForm->addRow(QStringLiteral("Averaging:"), m_averagingCombo);

    m_averagingTimeSpin = new QSpinBox(renderGroup);
    m_averagingTimeSpin->setRange(10, 5000);
    m_averagingTimeSpin->setSingleStep(10);
    m_averagingTimeSpin->setSuffix(QStringLiteral(" ms"));
    m_averagingTimeSpin->setValue(100);
    // Thetis: setup.designer.cs:34902 (udDisplayAVGTime) — rewritten
    // Thetis original: "When averaging, use this number of buffers to calculate the average."
    // Rewritten because NereusSDR expresses this as a millisecond window that's
    // converted to a smoothing alpha, not a discrete buffer count.
    m_averagingTimeSpin->setToolTip(QStringLiteral("Duration of the averaging window in milliseconds. Longer = heavier smoothing, slower response to signal changes."));
    connect(m_averagingTimeSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int ms) {
        // Translate ms to an alpha between 0.05 (slow) and 0.95 (fast).
        const float a = qBound(0.05f, 1.0f - (ms / 5000.0f), 0.95f);
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setAverageAlpha(a);
        }
    });
    renderForm->addRow(QStringLiteral("Averaging Time:"), m_averagingTimeSpin);

    m_decimationSpin = new QSpinBox(renderGroup);
    m_decimationSpin->setRange(1, 32);
    m_decimationSpin->setValue(1);
    // Thetis: setup.designer.cs:33732 (udDisplayDecimation)
    m_decimationSpin->setToolTip(QStringLiteral("Display decimation. Higher the number, the lower the resolution."));
    // Scaffolded only — FFTEngine decimation hook deferred to a future phase.
    renderForm->addRow(QStringLiteral("Decimation:"), m_decimationSpin);

    m_fillToggle = new QCheckBox(QStringLiteral("Fill under trace"), renderGroup);
    // Thetis: setup.designer.cs:33749 (chkDisplayPanFill)
    m_fillToggle->setToolTip(QStringLiteral("Check to fill the panadapter display line below the data."));
    connect(m_fillToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setPanFillEnabled(on);
        }
    });
    renderForm->addRow(QString(), m_fillToggle);

    {
        auto row = makeSliderRow(0, 100, 70, QStringLiteral("%"), renderGroup);
        m_fillAlphaSlider = row.slider;
        // Thetis: setup.designer.cs:3215 (tbDataFillAlpha) — no upstream tooltip; rewritten
        // Thetis original: (none)
        m_fillAlphaSlider->setToolTip(QStringLiteral("Opacity of the fill area under the spectrum trace (0 = transparent, 100 = opaque)."));
        row.spin->setToolTip(QStringLiteral("Opacity of the fill area under the spectrum trace (0 = transparent, 100 = opaque)."));
        connect(m_fillAlphaSlider, &QSlider::valueChanged, this, [this](int v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setFillAlpha(v / 100.0f);
            }
        });
        renderForm->addRow(QStringLiteral("Fill Alpha:"), row.container);
    }

    {
        auto row = makeSliderRow(1, 3, 1, QStringLiteral(" px"), renderGroup);
        m_lineWidthSlider = row.slider;
        // Thetis: setup.designer.cs:3228 (udDisplayLineWidth) — no upstream tooltip; rewritten
        // Thetis original: (none)
        m_lineWidthSlider->setToolTip(QStringLiteral("Spectrum trace line width in pixels (1–3 px)."));
        row.spin->setToolTip(QStringLiteral("Spectrum trace line width in pixels (1–3 px)."));
        connect(m_lineWidthSlider, &QSlider::valueChanged, this, [this](int v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setLineWidth(static_cast<float>(v));
            }
        });
        renderForm->addRow(QStringLiteral("Line Width:"), row.container);
    }

    m_gradientToggle = new QCheckBox(QStringLiteral("Trace gradient"), renderGroup);
    // Thetis: setup.designer.cs:53918 (chkDataLineGradient) — rewritten (grammar fix)
    // Thetis original: "The data line is also uses the gradient if checked"
    m_gradientToggle->setToolTip(QStringLiteral("When checked, the spectrum trace line renders with the gradient colour applied."));
    connect(m_gradientToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setGradientEnabled(on);
        }
    });
    renderForm->addRow(QString(), m_gradientToggle);

    contentLayout()->addWidget(renderGroup);

    // --- Section: Colors ---
    auto* colorGroup = new QGroupBox(QStringLiteral("Trace Colors"), this);
    auto* colorForm  = new QFormLayout(colorGroup);
    colorForm->setSpacing(6);

    const QColor initLine = sw ? sw->fillColor() : QColor(0x00, 0xe5, 0xff);
    m_dataLineColorBtn = new ColorSwatchButton(initLine, colorGroup);
    // Thetis: setup.designer.cs:3234 (clrbtnDataLine) — no upstream tooltip; rewritten
    // Thetis original: (none)
    m_dataLineColorBtn->setToolTip(QStringLiteral("Click to choose the spectrum trace line colour."));
    connect(m_dataLineColorBtn, &ColorSwatchButton::colorChanged,
            this, [this](const QColor& c) {
        // SpectrumWidget currently uses one colour for both line and fill.
        // Plan §6 S11/S13 allow splitting once the renderer grows a
        // dedicated m_dataLineColor; for now both pickers set the shared
        // fill colour.
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setFillColor(c);
        }
    });
    colorForm->addRow(QStringLiteral("Data Line Color:"), m_dataLineColorBtn);

    {
        auto row = makeSliderRow(0, 255, 230, QString(), colorGroup);
        m_dataLineAlphaSlider = row.slider;
        // Thetis: setup.designer.cs:3214 (tbDataLineAlpha) — no upstream tooltip; rewritten
        // Thetis original: (none)
        m_dataLineAlphaSlider->setToolTip(QStringLiteral("Opacity of the spectrum trace line (0 = invisible, 255 = fully opaque)."));
        row.spin->setToolTip(QStringLiteral("Opacity of the spectrum trace line (0 = invisible, 255 = fully opaque)."));
        connect(m_dataLineAlphaSlider, &QSlider::valueChanged, this, [this](int a) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                QColor c = w->fillColor();
                c.setAlpha(a);
                w->setFillColor(c);
            }
        });
        colorForm->addRow(QStringLiteral("Line Alpha:"), row.container);
    }

    m_dataFillColorBtn = new ColorSwatchButton(initLine, colorGroup);
    // Thetis: setup.designer.cs:3217 (clrbtnDataFill) — no upstream tooltip; rewritten
    // Thetis original: (none)
    m_dataFillColorBtn->setToolTip(QStringLiteral("Click to choose the spectrum fill area colour (applied under the trace when Fill is enabled)."));
    connect(m_dataFillColorBtn, &ColorSwatchButton::colorChanged,
            this, [this](const QColor& c) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setFillColor(c);
        }
    });
    colorForm->addRow(QStringLiteral("Data Fill Color:"), m_dataFillColorBtn);

    contentLayout()->addWidget(colorGroup);

    // --- Section: Calibration ---
    auto* calGroup = new QGroupBox(QStringLiteral("Calibration & Peak Hold"), this);
    auto* calForm  = new QFormLayout(calGroup);
    calForm->setSpacing(6);

    m_calOffsetSpin = new QDoubleSpinBox(calGroup);
    m_calOffsetSpin->setRange(-30.0, 30.0);
    m_calOffsetSpin->setSingleStep(0.5);
    m_calOffsetSpin->setSuffix(QStringLiteral(" dBm"));
    m_calOffsetSpin->setValue(0.0);
    // Thetis: display.cs:1372 (Display.RX1DisplayCalOffset) — programmatic only, no designer tooltip; rewritten
    // Thetis original: (none — set programmatically via Display.RX1DisplayCalOffset)
    m_calOffsetSpin->setToolTip(QStringLiteral("dBm calibration offset applied to all displayed signal levels. Use to match a calibrated reference."));
    connect(m_calOffsetSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setDbmCalOffset(static_cast<float>(v));
        }
    });
    calForm->addRow(QStringLiteral("Cal Offset:"), m_calOffsetSpin);

    m_peakHoldToggle = new QCheckBox(QStringLiteral("Peak hold"), calGroup);
    // NereusSDR extension — no Thetis equivalent
    m_peakHoldToggle->setToolTip(QStringLiteral("When enabled, the highest signal level seen at each frequency bin is held on the display."));
    connect(m_peakHoldToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setPeakHoldEnabled(on);
        }
    });
    calForm->addRow(QString(), m_peakHoldToggle);

    m_peakHoldDelaySpin = new QSpinBox(calGroup);
    m_peakHoldDelaySpin->setRange(100, 10000);
    m_peakHoldDelaySpin->setSingleStep(100);
    m_peakHoldDelaySpin->setSuffix(QStringLiteral(" ms"));
    m_peakHoldDelaySpin->setValue(2000);
    // NereusSDR extension — no Thetis equivalent
    m_peakHoldDelaySpin->setToolTip(QStringLiteral("Time in milliseconds before a held peak begins to decay back toward the live trace."));
    connect(m_peakHoldDelaySpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setPeakHoldDelayMs(v);
        }
    });
    calForm->addRow(QStringLiteral("Peak Delay:"), m_peakHoldDelaySpin);

    contentLayout()->addWidget(calGroup);

    // --- Section: Thread ---
    auto* threadGroup = new QGroupBox(QStringLiteral("Thread"), this);
    auto* threadForm  = new QFormLayout(threadGroup);
    threadForm->setSpacing(6);

    m_threadPriorityCombo = new QComboBox(threadGroup);
    m_threadPriorityCombo->addItems({
        QStringLiteral("Lowest"), QStringLiteral("Below Normal"),
        QStringLiteral("Normal"), QStringLiteral("Above Normal"),
        QStringLiteral("Highest")
    });
    m_threadPriorityCombo->setCurrentIndex(3);  // Above Normal (Thetis default)
    // Thetis: setup.designer.cs:33165 (comboDisplayThreadPriority)
    m_threadPriorityCombo->setToolTip(QStringLiteral("Set the priority of the display thread"));
    connect(m_threadPriorityCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        // Map Thetis 5-item ThreadPriority → QThread::Priority per §13 Q3.
        const QThread::Priority pri =
            (i == 0) ? QThread::LowestPriority :
            (i == 1) ? QThread::LowPriority    :
            (i == 2) ? QThread::NormalPriority :
            (i == 3) ? QThread::HighPriority   :
                       QThread::HighestPriority;
        if (model() && model()->fftEngine()) {
            if (auto* t = model()->fftEngine()->thread()) {
                t->setPriority(pri);
            }
        }
    });
    threadForm->addRow(QStringLiteral("Display Thread Priority:"), m_threadPriorityCombo);

    contentLayout()->addWidget(threadGroup);
    contentLayout()->addStretch();

    Q_UNUSED(sw);
    Q_UNUSED(fe);
}

// ---------------------------------------------------------------------------
// WaterfallDefaultsPage
// ---------------------------------------------------------------------------

WaterfallDefaultsPage::WaterfallDefaultsPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Waterfall Defaults"), model, parent)
{
    buildUI();
    loadFromRenderer();
}

void WaterfallDefaultsPage::loadFromRenderer()
{
    auto* sw = model() ? model()->spectrumWidget() : nullptr;
    if (!sw) { return; }
    QSignalBlocker b1(m_highThresholdSlider);
    QSignalBlocker b2(m_lowThresholdSlider);
    QSignalBlocker b4(m_useSpectrumMinMaxToggle);
    QSignalBlocker b5(m_updatePeriodSlider);
    QSignalBlocker b6(m_reverseToggle);
    QSignalBlocker b7(m_opacitySlider);
    QSignalBlocker b8(m_colorSchemeCombo);
    QSignalBlocker b9(m_wfAveragingCombo);
    QSignalBlocker b10(m_showRxFilterToggle);
    QSignalBlocker b11(m_showTxFilterToggle);
    QSignalBlocker b12(m_showRxZeroLineToggle);
    QSignalBlocker b13(m_showTxZeroLineToggle);
    QSignalBlocker b14(m_timestampPosCombo);
    QSignalBlocker b15(m_timestampModeCombo);

    m_highThresholdSlider->setValue(static_cast<int>(sw->wfHighThreshold()));
    m_lowThresholdSlider->setValue(static_cast<int>(sw->wfLowThreshold()));
    m_useSpectrumMinMaxToggle->setChecked(sw->wfUseSpectrumMinMax());
    m_updatePeriodSlider->setValue(sw->wfUpdatePeriodMs());
    m_reverseToggle->setChecked(sw->wfReverseScroll());
    m_opacitySlider->setValue(sw->wfOpacity());
    m_colorSchemeCombo->setCurrentIndex(static_cast<int>(sw->wfColorScheme()));
    m_wfAveragingCombo->setCurrentIndex(static_cast<int>(sw->wfAverageMode()));
    m_showRxFilterToggle->setChecked(sw->showRxFilterOnWaterfall());
    m_showTxFilterToggle->setChecked(sw->showTxFilterOnRxWaterfall());
    m_showRxZeroLineToggle->setChecked(sw->showRxZeroLineOnWaterfall());
    m_showTxZeroLineToggle->setChecked(sw->showTxZeroLineOnWaterfall());
    m_timestampPosCombo->setCurrentIndex(static_cast<int>(sw->wfTimestampPosition()));
    m_timestampModeCombo->setCurrentIndex(static_cast<int>(sw->wfTimestampMode()));

    if (m_lowColorBtn) { m_lowColorBtn->setColor(QColor(Qt::black)); }
}

void WaterfallDefaultsPage::buildUI()
{
    applyDarkStyle(this);

    // --- Section: Levels ---
    auto* levGroup = new QGroupBox(QStringLiteral("Levels"), this);
    auto* levForm  = new QFormLayout(levGroup);
    levForm->setSpacing(6);

    {
        auto row = makeSliderRow(-200, 0, -40, QStringLiteral(" dBm"), levGroup);
        m_highThresholdSlider = row.slider;
        // Thetis: setup.designer.cs:34259 (udDisplayWaterfallHighLevel)
        m_highThresholdSlider->setToolTip(QStringLiteral("Waterfall High Signal - Show High Color above this value (gradient in between)."));
        row.spin->setToolTip(QStringLiteral("Waterfall High Signal - Show High Color above this value (gradient in between)."));
        connect(m_highThresholdSlider, &QSlider::valueChanged, this, [this](int v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setWfHighThreshold(static_cast<float>(v));
            }
        });
        levForm->addRow(QStringLiteral("High Threshold:"), row.container);
    }

    {
        auto row = makeSliderRow(-200, 0, -130, QStringLiteral(" dBm"), levGroup);
        m_lowThresholdSlider = row.slider;
        // Thetis: setup.designer.cs:34219 (udDisplayWaterfallLowLevel)
        m_lowThresholdSlider->setToolTip(QStringLiteral("Waterfall Low Signal - Show Low Color below this value (gradient in between)."));
        row.spin->setToolTip(QStringLiteral("Waterfall Low Signal - Show Low Color below this value (gradient in between)."));
        connect(m_lowThresholdSlider, &QSlider::valueChanged, this, [this](int v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setWfLowThreshold(static_cast<float>(v));
            }
        });
        levForm->addRow(QStringLiteral("Low Threshold:"), row.container);
    }

    // Phase 3G-9c: legacy Waterfall AGC checkbox replaced by inert label.
    // ClarityController supersedes the running-min/max AGC — its
    // percentile-based estimator is more robust against strong carriers.
    // Thetis origin: setup.designer.cs:34069 (chkRX1WaterfallAGC).
    m_agcDeprecationLabel = new QLabel(
        QStringLiteral("AGC — superseded by Clarity (see Spectrum Defaults)"),
        levGroup);
    m_agcDeprecationLabel->setEnabled(false);
    levForm->addRow(QString(), m_agcDeprecationLabel);

    m_useSpectrumMinMaxToggle = new QCheckBox(QStringLiteral("Use spectrum min/max"), levGroup);
    // Thetis: setup.designer.cs:34054 (chkWaterfallUseRX1SpectrumMinMax)
    m_useSpectrumMinMaxToggle->setToolTip(QStringLiteral("Spectrum Grid min/max used for low and high level"));
    connect(m_useSpectrumMinMaxToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setWfUseSpectrumMinMax(on);
        }
    });
    levForm->addRow(QString(), m_useSpectrumMinMaxToggle);

    m_lowColorBtn = new ColorSwatchButton(QColor(Qt::black), levGroup);
    // Thetis: setup.designer.cs:34176 (clrbtnWaterfallLow)
    m_lowColorBtn->setToolTip(QStringLiteral("The Color to use when the signal level is at or below the low level set above."));
    // Waterfall "low" colour is conceptually the 0.0 stop of the gradient —
    // exposed here for plan §4.2 W10 parity. SpectrumWidget currently uses
    // gradient tables from wfSchemeStops() so the user's custom value is
    // recorded in AppSettings for future Custom-scheme wiring, but it does
    // not rebuild the gradient on the fly yet.
    connect(m_lowColorBtn, &ColorSwatchButton::colorChanged,
            this, [](const QColor&) { /* stored via AppSettings on save */ });
    levForm->addRow(QStringLiteral("Low Color:"), m_lowColorBtn);

    contentLayout()->addWidget(levGroup);

    // --- Section: Display ---
    auto* dispGroup = new QGroupBox(QStringLiteral("Display"), this);
    auto* dispForm  = new QFormLayout(dispGroup);
    dispForm->setSpacing(6);

    {
        auto row = makeSliderRow(10, 500, 50, QStringLiteral(" ms"), dispGroup);
        m_updatePeriodSlider = row.slider;
        // Thetis: setup.designer.cs:34145 (udDisplayWaterfallUpdatePeriod)
        m_updatePeriodSlider->setToolTip(QStringLiteral("How often to update (scroll another pixel line) on the waterfall display.  Note that this is tamed by the FPS setting."));
        row.spin->setToolTip(QStringLiteral("How often to update (scroll another pixel line) on the waterfall display.  Note that this is tamed by the FPS setting."));
        connect(m_updatePeriodSlider, &QSlider::valueChanged, this, [this](int v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setWfUpdatePeriodMs(v);
            }
        });
        dispForm->addRow(QStringLiteral("Update Period:"), row.container);
    }

    m_reverseToggle = new QCheckBox(QStringLiteral("Reverse scroll"), dispGroup);
    // NereusSDR extension — no Thetis equivalent
    m_reverseToggle->setToolTip(QStringLiteral("Waterfall normally scrolls top to bottom (newest at top). When checked, scrolls bottom to top (newest at bottom)."));
    connect(m_reverseToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setWfReverseScroll(on);
        }
    });
    dispForm->addRow(QString(), m_reverseToggle);

    {
        auto row = makeSliderRow(0, 100, 100, QStringLiteral("%"), dispGroup);
        m_opacitySlider = row.slider;
        // Thetis: setup.designer.cs:2056 (tbRX1WaterfallOpacity) — rewritten
        // Thetis original: (none)
        m_opacitySlider->setToolTip(QStringLiteral("Waterfall opacity (0 = fully transparent, 100 = fully opaque). Blends the waterfall over the spectrum background."));
        row.spin->setToolTip(QStringLiteral("Waterfall opacity (0 = fully transparent, 100 = fully opaque). Blends the waterfall over the spectrum background."));
        connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int v) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                w->setWfOpacity(v);
            }
        });
        dispForm->addRow(QStringLiteral("Opacity:"), row.container);
    }

    m_colorSchemeCombo = new QComboBox(dispGroup);
    m_colorSchemeCombo->addItems({
        QStringLiteral("Default"),   QStringLiteral("Enhanced"),
        QStringLiteral("Spectran"),  QStringLiteral("BlackWhite"),
        QStringLiteral("LinLog"),    QStringLiteral("LinRad"),
        QStringLiteral("Custom"),
        QStringLiteral("Clarity Blue")   // Phase 3G-9b
    });
    // Thetis: setup.designer.cs:34110 (comboColorPalette) — rewritten
    // Thetis original: "Sets the color scheme"
    m_colorSchemeCombo->setToolTip(QStringLiteral("Waterfall colour palette. Each scheme maps signal level to a different colour gradient from low (dark) to high (bright)."));
    connect(m_colorSchemeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setWfColorScheme(static_cast<WfColorScheme>(
                qBound(0, i, static_cast<int>(WfColorScheme::Count) - 1)));
        }
    });
    dispForm->addRow(QStringLiteral("Color Scheme:"), m_colorSchemeCombo);

    m_wfAveragingCombo = new QComboBox(dispGroup);
    m_wfAveragingCombo->addItems({
        QStringLiteral("None"), QStringLiteral("Weighted"),
        QStringLiteral("Logarithmic"), QStringLiteral("Time Window")
    });
    // Thetis: setup.designer.cs:2083 (comboDispWFAveraging) — rewritten
    // Thetis original: (none)
    m_wfAveragingCombo->setToolTip(QStringLiteral("Waterfall averaging mode. Weighted and Time Window smooth rapid signal changes; None shows raw FFT output per row."));
    connect(m_wfAveragingCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setWfAverageMode(static_cast<AverageMode>(i));
        }
    });
    dispForm->addRow(QStringLiteral("WF Averaging:"), m_wfAveragingCombo);

    contentLayout()->addWidget(dispGroup);

    // --- Section: Overlays ---
    auto* ovGroup = new QGroupBox(QStringLiteral("Overlays"), this);
    auto* ovForm  = new QFormLayout(ovGroup);
    ovForm->setSpacing(6);

    m_showRxFilterToggle = new QCheckBox(QStringLiteral("Show RX filter on waterfall"), ovGroup);
    // Thetis: setup.designer.cs:3189 (chkShowRXFilterOnWaterfall) — rewritten
    // Thetis original: (none)
    m_showRxFilterToggle->setToolTip(QStringLiteral("Overlay the current RX passband filter boundaries on the waterfall display."));
    connect(m_showRxFilterToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setShowRxFilterOnWaterfall(on);
        }
    });
    ovForm->addRow(QString(), m_showRxFilterToggle);

    m_showTxFilterToggle = new QCheckBox(QStringLiteral("Show TX filter on RX waterfall"), ovGroup);
    // Thetis: setup.designer.cs:3187 (chkShowTXFilterOnRXWaterfall) — rewritten
    // Thetis original: (none)
    m_showTxFilterToggle->setToolTip(QStringLiteral("Overlay the TX passband filter boundaries on the RX waterfall display."));
    connect(m_showTxFilterToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setShowTxFilterOnRxWaterfall(on);
        }
    });
    ovForm->addRow(QString(), m_showTxFilterToggle);

    m_showRxZeroLineToggle = new QCheckBox(QStringLiteral("Show RX zero line on waterfall"), ovGroup);
    // Thetis: setup.designer.cs:3188 (chkShowRXZeroLineOnWaterfall) — rewritten
    // Thetis original: (none)
    m_showRxZeroLineToggle->setToolTip(QStringLiteral("Draw a line on the waterfall at the RX centre frequency (zero-beat reference)."));
    connect(m_showRxZeroLineToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setShowRxZeroLineOnWaterfall(on);
        }
    });
    ovForm->addRow(QString(), m_showRxZeroLineToggle);

    m_showTxZeroLineToggle = new QCheckBox(QStringLiteral("Show TX zero line on waterfall"), ovGroup);
    // Thetis: setup.designer.cs:3242 (chkShowTXZeroLineOnWaterfall) — rewritten
    // Thetis original: (none)
    m_showTxZeroLineToggle->setToolTip(QStringLiteral("Draw a line on the waterfall at the TX centre frequency (zero-beat reference)."));
    connect(m_showTxZeroLineToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setShowTxZeroLineOnWaterfall(on);
        }
    });
    ovForm->addRow(QString(), m_showTxZeroLineToggle);

    contentLayout()->addWidget(ovGroup);

    // --- Section: Time ---
    auto* timeGroup = new QGroupBox(QStringLiteral("Time"), this);
    auto* timeForm  = new QFormLayout(timeGroup);
    timeForm->setSpacing(6);

    m_timestampPosCombo = new QComboBox(timeGroup);
    m_timestampPosCombo->addItems({QStringLiteral("None"), QStringLiteral("Left"),
                                   QStringLiteral("Right")});
    // NereusSDR extension — no Thetis equivalent
    m_timestampPosCombo->setToolTip(QStringLiteral("Position of the time stamp drawn on each waterfall row. None disables timestamps; Left and Right place them at the respective edge."));
    connect(m_timestampPosCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setWfTimestampPosition(
                static_cast<SpectrumWidget::TimestampPosition>(
                    qBound(0, i, static_cast<int>(SpectrumWidget::TimestampPosition::Count) - 1)));
        }
    });
    timeForm->addRow(QStringLiteral("Timestamp Position:"), m_timestampPosCombo);

    m_timestampModeCombo = new QComboBox(timeGroup);
    m_timestampModeCombo->addItems({QStringLiteral("UTC"), QStringLiteral("Local")});
    // NereusSDR extension — no Thetis equivalent
    m_timestampModeCombo->setToolTip(QStringLiteral("Time zone used for waterfall timestamps. UTC uses Coordinated Universal Time; Local uses the system clock time zone."));
    connect(m_timestampModeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setWfTimestampMode(
                static_cast<SpectrumWidget::TimestampMode>(
                    qBound(0, i, static_cast<int>(SpectrumWidget::TimestampMode::Count) - 1)));
        }
    });
    timeForm->addRow(QStringLiteral("Timestamp Mode:"), m_timestampModeCombo);

    contentLayout()->addWidget(timeGroup);
    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// GridScalesPage
// ---------------------------------------------------------------------------

GridScalesPage::GridScalesPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Grid & Scales"), model, parent)
{
    buildUI();
    loadFromRenderer();
}

// Helper: return the first panadapter or nullptr.
static PanadapterModel* firstPan(RadioModel* m)
{
    if (!m) { return nullptr; }
    const auto pans = m->panadapters();
    return pans.isEmpty() ? nullptr : pans.first();
}

void GridScalesPage::applyBandSlot(PanadapterModel* pan)
{
    if (!pan || !m_dbMaxSpin || !m_dbMinSpin || !m_editingBandLabel) { return; }
    const Band b = pan->band();
    const BandGridSettings slot = pan->perBandGrid(b);
    QSignalBlocker bMax(m_dbMaxSpin);
    QSignalBlocker bMin(m_dbMinSpin);
    m_dbMaxSpin->setValue(slot.dbMax);
    m_dbMinSpin->setValue(slot.dbMin);
    m_editingBandLabel->setText(
        QStringLiteral("Editing band: %1").arg(bandLabel(b)));
}

void GridScalesPage::loadFromRenderer()
{
    auto* sw  = model() ? model()->spectrumWidget() : nullptr;
    auto* pan = firstPan(model());
    if (!sw || !pan) { return; }

    QSignalBlocker b1(m_gridToggle);
    QSignalBlocker b2(m_dbStepSpin);
    QSignalBlocker b3(m_freqLabelAlignCombo);
    QSignalBlocker b4(m_zeroLineToggle);
    QSignalBlocker b5(m_showFpsToggle);

    m_gridToggle->setChecked(sw->gridEnabled());
    m_dbStepSpin->setValue(pan->gridStep());
    m_freqLabelAlignCombo->setCurrentIndex(static_cast<int>(sw->freqLabelAlign()));
    m_zeroLineToggle->setChecked(sw->showZeroLine());
    m_showFpsToggle->setChecked(sw->showFps());

    if (m_gridColorBtn)     { m_gridColorBtn->setColor(sw->gridColor()); }
    if (m_gridFineColorBtn) { m_gridFineColorBtn->setColor(sw->gridFineColor()); }
    if (m_hGridColorBtn)    { m_hGridColorBtn->setColor(sw->hGridColor()); }
    if (m_gridTextColorBtn) { m_gridTextColorBtn->setColor(sw->gridTextColor()); }
    if (m_zeroLineColorBtn) { m_zeroLineColorBtn->setColor(sw->zeroLineColor()); }
    if (m_bandEdgeColorBtn) { m_bandEdgeColorBtn->setColor(sw->bandEdgeColor()); }

    applyBandSlot(pan);
}

void GridScalesPage::buildUI()
{
    applyDarkStyle(this);

    // --- Section: Grid ---
    auto* gridGroup = new QGroupBox(QStringLiteral("Grid"), this);
    auto* gridForm  = new QFormLayout(gridGroup);
    gridForm->setSpacing(6);

    m_gridToggle = new QCheckBox(QStringLiteral("Show grid"), gridGroup);
    m_gridToggle->setChecked(true);
    // Thetis: setup.designer.cs:52824 (chkGridControl)
    m_gridToggle->setToolTip(QStringLiteral("Display the Major Grid on the Panadapter including the frequency numbers"));
    connect(m_gridToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setGridEnabled(on);
        }
    });
    gridForm->addRow(QString(), m_gridToggle);

    m_editingBandLabel = new QLabel(QStringLiteral("Editing band: —"), gridGroup);
    m_editingBandLabel->setStyleSheet(QStringLiteral("QLabel { color: #00b4d8; font-weight: bold; }"));
    gridForm->addRow(QString(), m_editingBandLabel);

    m_dbMaxSpin = new QSpinBox(gridGroup);
    m_dbMaxSpin->setRange(-200, 0);
    m_dbMaxSpin->setValue(-40);
    m_dbMaxSpin->setSuffix(QStringLiteral(" dB"));
    // Thetis: setup.designer.cs:34745 (udDisplayGridMax) — rewritten
    // Thetis original: "Signal level at top of display in dB."
    m_dbMaxSpin->setToolTip(QStringLiteral("Signal level at the top of the display in dB. Edits the current band's grid slot — see the band indicator above."));
    connect(m_dbMaxSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (auto* pan = firstPan(model())) {
            pan->setPerBandDbMax(pan->band(), v);
        }
    });
    gridForm->addRow(QStringLiteral("dB Max (per band):"), m_dbMaxSpin);

    m_dbMinSpin = new QSpinBox(gridGroup);
    m_dbMinSpin->setRange(-200, 0);
    m_dbMinSpin->setValue(-140);
    m_dbMinSpin->setSuffix(QStringLiteral(" dB"));
    // Thetis: setup.designer.cs:34714 (udDisplayGridMin) — rewritten
    // Thetis original: "Signal Level at bottom of display in dB."
    m_dbMinSpin->setToolTip(QStringLiteral("Signal level at the bottom of the display in dB. Edits the current band's grid slot — see the band indicator above."));
    connect(m_dbMinSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (auto* pan = firstPan(model())) {
            pan->setPerBandDbMin(pan->band(), v);
        }
    });
    gridForm->addRow(QStringLiteral("dB Min (per band):"), m_dbMinSpin);

    m_dbStepSpin = new QSpinBox(gridGroup);
    m_dbStepSpin->setRange(1, 40);
    m_dbStepSpin->setValue(10);
    m_dbStepSpin->setSuffix(QStringLiteral(" dB"));
    // Thetis: setup.designer.cs:34683 (udDisplayGridStep) — rewritten
    // Thetis original: "Horizontal Grid Step Size in dB."
    m_dbStepSpin->setToolTip(QStringLiteral("Horizontal grid step size in dB. Sets the spacing between dB grid lines across all bands (global, not per-band)."));
    connect(m_dbStepSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (auto* pan = firstPan(model())) {
            pan->setGridStep(v);
        }
    });
    gridForm->addRow(QStringLiteral("dB Step (global):"), m_dbStepSpin);

    contentLayout()->addWidget(gridGroup);

    // Connect to PanadapterModel::bandChanged so the editing-band label
    // and the dbMax/dbMin spinboxes refresh when the user tunes across a
    // band boundary (or clicks a band button).
    if (auto* pan = firstPan(model())) {
        connect(pan, &PanadapterModel::bandChanged,
                this, [this, pan](Band) { applyBandSlot(pan); });
    }

    // --- Section: Labels ---
    auto* lblGroup = new QGroupBox(QStringLiteral("Labels"), this);
    auto* lblForm  = new QFormLayout(lblGroup);
    lblForm->setSpacing(6);

    m_freqLabelAlignCombo = new QComboBox(lblGroup);
    m_freqLabelAlignCombo->addItems({
        QStringLiteral("Left"),   QStringLiteral("Center"),
        QStringLiteral("Right"),  QStringLiteral("Auto"),
        QStringLiteral("Off")
    });
    m_freqLabelAlignCombo->setCurrentIndex(1);
    // Thetis: setup.designer.cs:34649 (comboDisplayLabelAlign) — rewritten
    // Thetis original: "Sets the alignement of the grid callouts on the display."
    m_freqLabelAlignCombo->setToolTip(QStringLiteral("Sets the alignment of the frequency labels on the grid callouts on the display."));
    connect(m_freqLabelAlignCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int i) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setFreqLabelAlign(static_cast<FreqLabelAlign>(i));
        }
    });
    lblForm->addRow(QStringLiteral("Freq Label Align:"), m_freqLabelAlignCombo);

    m_zeroLineToggle = new QCheckBox(QStringLiteral("Show zero line"), lblGroup);
    // Thetis: setup.designer.cs:3221 (chkShowZeroLine) — rewritten
    // Thetis original: (none)
    m_zeroLineToggle->setToolTip(QStringLiteral("Show a horizontal line at 0 dBm on the panadapter grid."));
    connect(m_zeroLineToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setShowZeroLine(on);
        }
    });
    lblForm->addRow(QString(), m_zeroLineToggle);

    m_showFpsToggle = new QCheckBox(QStringLiteral("Show FPS overlay"), lblGroup);
    // Thetis: setup.designer.cs:33177 (chkShowFPS)
    m_showFpsToggle->setToolTip(QStringLiteral("Show FPS reading in top left of spectrum area"));
    connect(m_showFpsToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
            w->setShowFps(on);
        }
    });
    lblForm->addRow(QString(), m_showFpsToggle);

    contentLayout()->addWidget(lblGroup);

    // --- Section: Colors ---
    auto* colGroup = new QGroupBox(QStringLiteral("Colors"), this);
    auto* colForm  = new QFormLayout(colGroup);
    colForm->setSpacing(6);

    auto makeBtn = [this](QWidget* parent, const QColor& init,
                          void (SpectrumWidget::*setter)(const QColor&)) {
        auto* btn = new ColorSwatchButton(init, parent);
        connect(btn, &ColorSwatchButton::colorChanged,
                this, [this, setter](const QColor& c) {
            if (auto* w = model() ? model()->spectrumWidget() : nullptr) {
                (w->*setter)(c);
            }
        });
        return btn;
    };

    m_gridColorBtn     = makeBtn(colGroup, QColor(255, 255, 255, 40), &SpectrumWidget::setGridColor);
    // Thetis: setup.designer.cs:3202 (clrbtnGrid) — rewritten
    // Thetis original: (none)
    m_gridColorBtn->setToolTip(QStringLiteral("Colour of the major vertical grid lines on the panadapter."));
    colForm->addRow(QStringLiteral("Grid Color:"), m_gridColorBtn);

    m_gridFineColorBtn = makeBtn(colGroup, QColor(255, 255, 255, 20), &SpectrumWidget::setGridFineColor);
    // Thetis: setup.designer.cs:3198 (clrbtnGridFine) — rewritten
    // Thetis original: (none)
    m_gridFineColorBtn->setToolTip(QStringLiteral("Colour of the minor (fine) grid lines between major grid lines on the panadapter."));
    colForm->addRow(QStringLiteral("Grid Fine Color:"), m_gridFineColorBtn);

    m_hGridColorBtn    = makeBtn(colGroup, QColor(255, 255, 255, 40), &SpectrumWidget::setHGridColor);
    // Thetis: setup.designer.cs:3193 (clrbtnHGridColor) — rewritten
    // Thetis original: (none)
    m_hGridColorBtn->setToolTip(QStringLiteral("Colour of the horizontal dB grid lines on the panadapter."));
    colForm->addRow(QStringLiteral("H-Grid Color:"), m_hGridColorBtn);

    m_gridTextColorBtn = makeBtn(colGroup, QColor(255, 255, 0), &SpectrumWidget::setGridTextColor);
    // Thetis: setup.designer.cs:3206 (clrbtnText) — rewritten
    // Thetis original: (none)
    m_gridTextColorBtn->setToolTip(QStringLiteral("Colour of the frequency and dB labels drawn on the panadapter grid."));
    colForm->addRow(QStringLiteral("Text Color:"), m_gridTextColorBtn);

    m_zeroLineColorBtn = makeBtn(colGroup, QColor(255, 0, 0), &SpectrumWidget::setZeroLineColor);
    // Thetis: setup.designer.cs:3204 (clrbtnZeroLine) — rewritten
    // Thetis original: (none)
    m_zeroLineColorBtn->setToolTip(QStringLiteral("Colour of the zero line (0 dBm marker) drawn on the panadapter when Show zero line is checked."));
    colForm->addRow(QStringLiteral("Zero Line Color:"), m_zeroLineColorBtn);

    m_bandEdgeColorBtn = makeBtn(colGroup, QColor(255, 0, 0), &SpectrumWidget::setBandEdgeColor);
    // Thetis: setup.designer.cs:3232 (clrbtnBandEdge) — rewritten
    // Thetis original: (none)
    m_bandEdgeColorBtn->setToolTip(QStringLiteral("Colour of the band edge markers drawn at the amateur band boundaries on the panadapter."));
    colForm->addRow(QStringLiteral("Band Edge Color:"), m_bandEdgeColorBtn);

    contentLayout()->addWidget(colGroup);
    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// Rx2DisplayPage
// ---------------------------------------------------------------------------

Rx2DisplayPage::Rx2DisplayPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("RX2 Display"), model, parent)
{
    buildUI();
}

void Rx2DisplayPage::buildUI()
{
    applyDarkStyle(this);

    // --- Section: RX2 Spectrum ---
    auto* specGroup = new QGroupBox(QStringLiteral("RX2 Spectrum"), this);
    auto* specForm  = new QFormLayout(specGroup);
    specForm->setSpacing(6);

    m_dbMaxSpin = new QSpinBox(specGroup);
    m_dbMaxSpin->setRange(-200, 0);
    m_dbMaxSpin->setValue(-20);
    m_dbMaxSpin->setSuffix(QStringLiteral(" dB"));
    m_dbMaxSpin->setEnabled(false);  // NYI
    m_dbMaxSpin->setToolTip(QStringLiteral("RX2 spectrum dB max — not yet implemented"));
    specForm->addRow(QStringLiteral("dB Max:"), m_dbMaxSpin);

    m_dbMinSpin = new QSpinBox(specGroup);
    m_dbMinSpin->setRange(-200, 0);
    m_dbMinSpin->setValue(-160);
    m_dbMinSpin->setSuffix(QStringLiteral(" dB"));
    m_dbMinSpin->setEnabled(false);  // NYI
    m_dbMinSpin->setToolTip(QStringLiteral("RX2 spectrum dB min — not yet implemented"));
    specForm->addRow(QStringLiteral("dB Min:"), m_dbMinSpin);

    m_colorSchemeCombo = new QComboBox(specGroup);
    m_colorSchemeCombo->addItems({QStringLiteral("Enhanced"), QStringLiteral("Grayscale"),
                                  QStringLiteral("Spectrum")});
    m_colorSchemeCombo->setEnabled(false);  // NYI
    m_colorSchemeCombo->setToolTip(QStringLiteral("RX2 waterfall color scheme — not yet implemented"));
    specForm->addRow(QStringLiteral("Color Scheme:"), m_colorSchemeCombo);

    contentLayout()->addWidget(specGroup);

    // --- Section: RX2 Waterfall ---
    auto* wfGroup = new QGroupBox(QStringLiteral("RX2 Waterfall"), this);
    auto* wfForm  = new QFormLayout(wfGroup);
    wfForm->setSpacing(6);

    m_highThresholdSlider = new QSlider(Qt::Horizontal, wfGroup);
    m_highThresholdSlider->setRange(-200, 0);
    m_highThresholdSlider->setValue(-40);
    m_highThresholdSlider->setEnabled(false);  // NYI
    m_highThresholdSlider->setToolTip(QStringLiteral("RX2 waterfall high threshold — not yet implemented"));
    wfForm->addRow(QStringLiteral("High Threshold:"), m_highThresholdSlider);

    m_lowThresholdSlider = new QSlider(Qt::Horizontal, wfGroup);
    m_lowThresholdSlider->setRange(-200, 0);
    m_lowThresholdSlider->setValue(-130);
    m_lowThresholdSlider->setEnabled(false);  // NYI
    m_lowThresholdSlider->setToolTip(QStringLiteral("RX2 waterfall low threshold — not yet implemented"));
    wfForm->addRow(QStringLiteral("Low Threshold:"), m_lowThresholdSlider);

    contentLayout()->addWidget(wfGroup);
    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// TxDisplayPage
// ---------------------------------------------------------------------------

TxDisplayPage::TxDisplayPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("TX Display"), model, parent)
{
    buildUI();
}

void TxDisplayPage::buildUI()
{
    applyDarkStyle(this);

    // --- Section: TX Spectrum ---
    auto* specGroup = new QGroupBox(QStringLiteral("TX Spectrum"), this);
    auto* specForm  = new QFormLayout(specGroup);
    specForm->setSpacing(6);

    m_bgColorLabel = makeColorSwatch(QStringLiteral("Background Color"), QStringLiteral("#0a0a14"), specGroup);
    specForm->addRow(QStringLiteral("Background:"), m_bgColorLabel);

    m_gridColorLabel = makeColorSwatch(QStringLiteral("Grid Color"), QStringLiteral("#203040"), specGroup);
    specForm->addRow(QStringLiteral("Grid Color:"), m_gridColorLabel);

    m_lineWidthSlider = new QSlider(Qt::Horizontal, specGroup);
    m_lineWidthSlider->setRange(1, 3);
    m_lineWidthSlider->setValue(1);
    m_lineWidthSlider->setEnabled(false);  // NYI
    m_lineWidthSlider->setToolTip(QStringLiteral("TX trace line width (1–3 px) — not yet implemented"));
    specForm->addRow(QStringLiteral("Line Width:"), m_lineWidthSlider);

    m_calOffsetSpin = new QDoubleSpinBox(specGroup);
    m_calOffsetSpin->setRange(-30.0, 30.0);
    m_calOffsetSpin->setSingleStep(0.5);
    m_calOffsetSpin->setSuffix(QStringLiteral(" dBm"));
    m_calOffsetSpin->setValue(0.0);
    m_calOffsetSpin->setEnabled(false);  // NYI
    m_calOffsetSpin->setToolTip(QStringLiteral("TX calibration offset — not yet implemented"));
    specForm->addRow(QStringLiteral("Cal Offset:"), m_calOffsetSpin);

    contentLayout()->addWidget(specGroup);
    contentLayout()->addStretch();
}

} // namespace NereusSDR
