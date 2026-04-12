#pragma once

#include <QWidget>
#include <QVector>
#include <QImage>
#include <QColor>
#include <QPoint>
#include <QMap>
#include <QTimer>

#include <utility>

// GPU spectrum: QRhiWidget base class for Metal/Vulkan/D3D12 rendering.
// CPU fallback: QWidget with QPainter.
// Note: NEREUS_GPU_SPECTRUM is set in CMakeLists.txt via target_compile_definitions.
#ifdef NEREUS_GPU_SPECTRUM
#include <QRhiWidget>
#include <rhi/qrhi.h>
using SpectrumBaseClass = QRhiWidget;
#else
using SpectrumBaseClass = QWidget;
#endif

namespace NereusSDR {

class SpectrumOverlayMenu;
class VfoWidget;

// Waterfall color scheme presets.
// Default matches AetherSDR/SmartSDR style.
// From Thetis enums.cs:68-79 (ColorScheme enum). Expanded 4 → 7 in
// Phase 3G-8 commit 5 (plan §7 W17 waterfall colour schemes expansion).
enum class WfColorScheme : int {
    Default = 0,    // AetherSDR: black → blue → cyan → green → yellow → red
    Enhanced,       // Thetis enhanced (9-band progression)
    Spectran,       // SPECTRAN
    BlackWhite,     // Grayscale
    LinLog,         // Linear in low, log in high — Thetis LinLog
    LinRad,         // Linradiance-style cool → hot
    Custom,         // User-defined custom stops (reads from AppSettings)
    Count
};

// Frequency label alignment for the bottom scale bar.
// From Thetis comboDisplayLabelAlign (setup.designer.cs:34635).
// Thetis exposes 5 options (Left/Center/Right/Auto/Off); NereusSDR
// Phase 3G-8 commit 5 expands the previous 2-mode implementation to
// match.
enum class FreqLabelAlign : int {
    Left = 0,
    Center,
    Right,
    Auto,   // centered when room, otherwise left
    Off,    // suppress frequency labels entirely
    Count
};

// Spectrum averaging mode. Ported from Thetis comboDispPanAveraging
// (setup.designer.cs:34835, target console.specRX.GetSpecRX(0).AverageMode).
// Thetis options: None / Recursive / Time Window / Log Recursive.
// NereusSDR names: None / Weighted / TimeWindow / Logarithmic — the
// previous single smoothing behavior (kSmoothAlpha * new + (1-a) * prev)
// corresponds to Weighted.
enum class AverageMode : int {
    None = 0,        // pass frame through unchanged
    Weighted,        // kSmoothAlpha exponential (current NereusSDR behavior)
    Logarithmic,     // log-domain exponential (matches Thetis Log Recursive)
    TimeWindow,      // approximated as slower exponential for now
    Count
};

// Gradient stop for waterfall color mapping.
struct WfGradientStop { float pos; int r, g, b; };

// Returns gradient stops for a given color scheme.
const WfGradientStop* wfSchemeStops(WfColorScheme scheme, int& count);

// CPU-rendered spectrum + waterfall display widget.
// Phase A: QPainter fallback (get something visible fast).
// Phase B: Switch to QRhiWidget for GPU rendering.
//
// Layout (top to bottom):
//   36px  - dBm scale (left strip)
//   ~40%  - spectrum trace (FFT line plot)
//   4px   - divider
//   ~60%  - waterfall (scrolling heat-map history)
//   20px  - frequency scale bar
//
// From gpu-waterfall.md lines 274-289
class SpectrumWidget : public SpectrumBaseClass {
    Q_OBJECT

public:
    explicit SpectrumWidget(QWidget* parent = nullptr);
    ~SpectrumWidget() override;

    QSize sizeHint() const override { return {800, 400}; }

    // ---- Frequency range ----
    void setFrequencyRange(double centerHz, double bandwidthHz);
    void setCenterFrequency(double centerHz);
    double centerFrequency() const { return m_centerHz; }
    double bandwidth() const { return m_bandwidthHz; }
    void setDdcCenterFrequency(double hz);
    double ddcCenterFrequency() const { return m_ddcCenterHz; }
    void setSampleRate(double hz);
    double sampleRate() const { return m_sampleRateHz; }

    // ---- Display range ----
    void setDbmRange(float minDbm, float maxDbm);
    float refLevel() const { return m_refLevel; }
    float dynamicRange() const { return m_dynamicRange; }

    // ---- Waterfall settings ----
    void setWfColorScheme(WfColorScheme scheme);
    WfColorScheme wfColorScheme() const { return m_wfColorScheme; }
    void setWfColorGain(int gain) { m_wfColorGain = gain; }
    int  wfColorGain() const { return m_wfColorGain; }
    void setWfBlackLevel(int level) { m_wfBlackLevel = level; }
    int  wfBlackLevel() const { return m_wfBlackLevel; }

    // ---- Spectrum renderer controls (Phase 3G-8 commit 3) ----

    // Averaging mode applied in updateSpectrum() before drawing.
    void setAverageMode(AverageMode m);
    AverageMode averageMode() const { return m_averageMode; }

    // Smoothing time constant for Weighted / Logarithmic / TimeWindow.
    // 0.0 = no smoothing, 1.0 = infinite smoothing. Default kSmoothAlpha.
    void setAverageAlpha(float alpha);
    float averageAlpha() const { return m_averageAlpha; }

    // Peak hold: track per-bin max, decay after delay.
    void setPeakHoldEnabled(bool on);
    bool peakHoldEnabled() const { return m_peakHoldEnabled; }
    void setPeakHoldDelayMs(int ms);
    int  peakHoldDelayMs() const { return m_peakHoldDelayMs; }

    // Trace fill (under-the-curve shaded region).
    void setPanFillEnabled(bool on);
    bool panFillEnabled() const { return m_panFill; }
    void setFillAlpha(float a);       // 0.0 .. 1.0
    float fillAlpha() const { return m_fillAlpha; }

    // Trace line width (QPainter pen width). GPU path uses line list
    // default width until commit 5 renderer additions.
    void setLineWidth(float w);
    float lineWidth() const { return m_lineWidth; }

    // Trace gradient: when enabled, the QPainter fill gradient ramps
    // from transparent at baseline to the fill color at the trace,
    // and the trace uses a vertical color gradient. GPU path keeps
    // its existing per-vertex heatmap coloring (wire-up in commit 5).
    void setGradientEnabled(bool on);
    bool gradientEnabled() const { return m_gradientEnabled; }

    // Display calibration offset added to every bin before it's mapped
    // to screen Y. Ported from Thetis Display.RX1DisplayCalOffset
    // (display.cs:1372). Range: -30 .. +30 dB.
    void setDbmCalOffset(float db);
    float dbmCalOffset() const { return m_dbmCalOffset; }

    // Trace colour (Phase 3G-8 commit 6 wiring). Single colour used for
    // both the line and the fill in the current renderer; plan §6 S11/S13
    // track splitting these if needed by future UI polish.
    void setFillColor(const QColor& c);
    QColor fillColor() const { return m_fillColor; }

    // ---- Waterfall renderer controls (Phase 3G-8 commit 4) ----

    void setWfHighThreshold(float dbm);
    float wfHighThreshold() const { return m_wfHighThreshold; }
    void setWfLowThreshold(float dbm);
    float wfLowThreshold() const { return m_wfLowThreshold; }
    void setWfAgcEnabled(bool on);
    bool wfAgcEnabled() const { return m_wfAgcEnabled; }
    void setWfReverseScroll(bool on);
    bool wfReverseScroll() const { return m_wfReverseScroll; }
    void setWfOpacity(int percent);          // 0..100
    int  wfOpacity() const { return m_wfOpacity; }
    void setWfUpdatePeriodMs(int ms);
    int  wfUpdatePeriodMs() const { return m_wfUpdatePeriodMs; }

    // Ported from setup.cs:7801 Display.WaterfallUseRX1SpectrumMinMax.
    void setWfUseSpectrumMinMax(bool on);
    bool wfUseSpectrumMinMax() const { return m_wfUseSpectrumMinMax; }

    // Ported from setup.designer.cs:34428 comboDispWFAveraging / AverageModeWF.
    void setWfAverageMode(AverageMode m);
    AverageMode wfAverageMode() const { return m_wfAverageMode; }

    // Timestamp overlay (NereusSDR extensions W8/W9).
    enum class TimestampPosition : int { None = 0, Left, Right, Count };
    enum class TimestampMode     : int { UTC = 0, Local, Count };
    void setWfTimestampPosition(TimestampPosition p);
    TimestampPosition wfTimestampPosition() const { return m_wfTimestampPos; }
    void setWfTimestampMode(TimestampMode m);
    TimestampMode wfTimestampMode() const { return m_wfTimestampMode; }

    // Filter / zero-line overlays on the waterfall.
    // From setup.cs:1048-1052 Display.ShowRXFilterOnWaterfall / ShowTXFilterOnRXWaterfall
    // / ShowRXZeroLineOnWaterfall / ShowTXZeroLineOnWaterfall.
    void setShowRxFilterOnWaterfall(bool on);
    bool showRxFilterOnWaterfall() const { return m_showRxFilterOnWaterfall; }
    void setShowTxFilterOnRxWaterfall(bool on);
    bool showTxFilterOnRxWaterfall() const { return m_showTxFilterOnRxWaterfall; }
    void setShowRxZeroLineOnWaterfall(bool on);
    bool showRxZeroLineOnWaterfall() const { return m_showRxZeroLineOnWaterfall; }
    void setShowTxZeroLineOnWaterfall(bool on);
    bool showTxZeroLineOnWaterfall() const { return m_showTxZeroLineOnWaterfall; }

    // ---- Grid / scales renderer controls (Phase 3G-8 commit 5) ----

    void setGridEnabled(bool on);
    bool gridEnabled() const { return m_gridEnabled; }

    void setShowZeroLine(bool on);
    bool showZeroLine() const { return m_showZeroLine; }

    void setShowFps(bool on);
    bool showFps() const { return m_showFps; }

    void setFreqLabelAlign(FreqLabelAlign a);
    FreqLabelAlign freqLabelAlign() const { return m_freqLabelAlign; }

    // Configurable grid/text/zero-line/band-edge colours. Previously
    // hardcoded. Ported from Thetis setup.cs:1040-1044 Display.GridColor
    // / GridPenDark / HGridColor / GridTextColor / GridZeroColor and
    // display.cs:1941 BandEdgeColor.
    void setGridColor(const QColor& c);
    QColor gridColor() const { return m_gridColor; }
    void setGridFineColor(const QColor& c);
    QColor gridFineColor() const { return m_gridFineColor; }
    void setHGridColor(const QColor& c);
    QColor hGridColor() const { return m_hGridColor; }
    void setGridTextColor(const QColor& c);
    QColor gridTextColor() const { return m_gridTextColor; }
    void setZeroLineColor(const QColor& c);
    QColor zeroLineColor() const { return m_zeroLineColor; }
    void setBandEdgeColor(const QColor& c);
    QColor bandEdgeColor() const { return m_bandEdgeColor; }

    // ---- Per-pan settings persistence ----
    void setPanIndex(int idx) { m_panIndex = idx; }
    int  panIndex() const { return m_panIndex; }
    void loadSettings();
    void saveSettings();

    // ---- VFO / filter overlay ----
    void setVfoFrequency(double hz);
    void setFilterOffset(int lowHz, int highHz);  // updates filter passband overlay

    // ---- CTUN mode (SmartSDR-style independent pan) ----
    void setCtunEnabled(bool enabled);
    bool ctunEnabled() const { return m_ctunEnabled; }
    void recenterOnVfo();

    // ---- Tuning step ----
    void setStepSize(int hz) { m_stepHz = hz; }
    int  stepSize() const { return m_stepHz; }

    // ---- VFO flag widgets (AetherSDR pattern) ----
    VfoWidget* addVfoWidget(int sliceIndex);
    void removeVfoWidget(int sliceIndex);
    VfoWidget* vfoWidget(int sliceIndex) const;
    void updateVfoPositions();

public slots:
    // Feed a new FFT frame. binsDbm are dBm values, one per frequency bin.
    // Called from the main thread after FFTEngine delivers the frame.
    void updateSpectrum(int receiverId, const QVector<float>& binsDbm);

signals:
    // Emitted when user clicks on spectrum/waterfall to tune
    void frequencyClicked(double hz);
    // Emitted when user drags a filter edge
    void filterEdgeDragged(int lowHz, int highHz);
    // Emitted when pan center changes (drag, auto-scroll)
    void centerChanged(double centerHz);
    // Emitted when user scrolls to change bandwidth
    void bandwidthChangeRequested(double newBandwidthHz);
    // Emitted when CTUN mode changes
    void ctunEnabledChanged(bool enabled);

protected:
#ifdef NEREUS_GPU_SPECTRUM
    void initialize(QRhiCommandBuffer* cb) override;
    void render(QRhiCommandBuffer* cb) override;
    void releaseResources() override;
#endif
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;  // mouse overlay forwarding
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    // ---- Drawing helpers ----
    void drawGrid(QPainter& p, const QRect& specRect);
    void drawSpectrum(QPainter& p, const QRect& specRect);
    void drawWaterfall(QPainter& p, const QRect& wfRect);
    void drawFreqScale(QPainter& p, const QRect& r);
    void drawDbmScale(QPainter& p, const QRect& specRect);
    void drawVfoMarker(QPainter& p, const QRect& specRect, const QRect& wfRect);
    void drawCursorInfo(QPainter& p, const QRect& specRect);

    // ---- Coordinate helpers ----
    int    hzToX(double hz, const QRect& r) const;
    double xToHz(int x, const QRect& r) const;
    int    dbmToY(float dbm, const QRect& r) const;

    // Returns the first and last FFT bin indices visible in the current
    // display window (m_centerHz ± m_bandwidthHz/2), mapped against
    // the DDC center and sample rate. Clamped to [0, binCount-1].
    std::pair<int, int> visibleBinRange(int binCount) const;

    // ---- Waterfall helpers ----
    void   pushWaterfallRow(const QVector<float>& bins);
    QRgb   dbmToRgb(float dbm) const;

    // ---- FFT data ----
    QVector<float> m_bins;       // latest raw FFT frame (dBm)
    QVector<float> m_smoothed;   // exponential-smoothed for display

    // ---- Frequency range ----
    double m_centerHz{14225000.0};    // 14.225 MHz default
    double m_bandwidthHz{200000.0};   // 200 kHz default
    double m_ddcCenterHz{14225000.0};   // DDC hardware center frequency
    double m_sampleRateHz{768000.0};    // DDC sample rate

    // ---- Display range ----
    // From Thetis display.cs:1743-1754
    float  m_refLevel{-40.0f};        // top of display (dBm)
    float  m_dynamicRange{100.0f};    // range in dB (bottom = refLevel - dynamicRange)

    // ---- Waterfall ----
    QImage m_waterfall;               // ring buffer (Format_RGB32)
    int    m_wfWriteRow{0};

    // ---- Waterfall display controls ----
    // From AetherSDR SpectrumWidget defaults + Thetis display.cs:2522-2536
    WfColorScheme m_wfColorScheme{WfColorScheme::Default};
    int    m_wfColorGain{50};         // 0-100
    int    m_wfBlackLevel{15};        // 0-125
    // Waterfall uses its own dBm range (narrower than spectrum for better contrast)
    // From Thetis display.cs:2522 waterfall_high_threshold = -80.0F
    // From Thetis display.cs:2536 waterfall_low_threshold = -130.0F
    float  m_wfHighThreshold{-80.0f};
    float  m_wfLowThreshold{-130.0f};

    // ---- Smoothing constant ----
    // From AetherSDR SpectrumWidget.h:417 — SMOOTH_ALPHA = 0.35f
    static constexpr float kSmoothAlpha = 0.35f;

    // ---- Layout constants ----
    // From gpu-waterfall.md:590-593
    float  m_spectrumFrac{0.40f};     // 40% spectrum, 60% waterfall
    static constexpr int kFreqScaleH = 28;  // Taller for easier grab target
    static constexpr int kDividerH = 4;
    static constexpr int kDbmStripW = 36;

    // ---- Spectrum fill ----
    // From AetherSDR defaults
    QColor m_fillColor{0x00, 0xe5, 0xff};  // cyan
    float  m_fillAlpha{0.70f};
    bool   m_panFill{true};

    // ---- Phase 3G-8 commit 3: spectrum renderer state ----

    AverageMode m_averageMode{AverageMode::Weighted};
    float       m_averageAlpha{kSmoothAlpha};  // 0..1 exp-smoothing factor

    bool        m_peakHoldEnabled{false};
    int         m_peakHoldDelayMs{2000};
    QVector<float> m_peakHoldBins;
    QTimer*     m_peakHoldDecayTimer{nullptr};

    float       m_lineWidth{1.5f};
    bool        m_gradientEnabled{false};

    // Ported from Thetis Display.RX1DisplayCalOffset (display.cs:1372).
    float       m_dbmCalOffset{0.0f};

    // ---- Phase 3G-8 commit 4: waterfall renderer state ----

    bool  m_wfAgcEnabled{false};
    bool  m_wfReverseScroll{false};
    int   m_wfOpacity{100};           // 0..100
    int   m_wfUpdatePeriodMs{50};     // NereusSDR default per §10 divergence
    bool  m_wfUseSpectrumMinMax{false};
    AverageMode m_wfAverageMode{AverageMode::None};
    QVector<float> m_wfSmoothedBins;  // for wf averaging mode

    TimestampPosition m_wfTimestampPos{TimestampPosition::None};
    TimestampMode     m_wfTimestampMode{TimestampMode::UTC};

    bool  m_showRxFilterOnWaterfall{false};
    bool  m_showTxFilterOnRxWaterfall{false};
    bool  m_showRxZeroLineOnWaterfall{false};
    bool  m_showTxZeroLineOnWaterfall{false};

    // AGC rolling envelope (tracked across waterfall rows).
    float m_wfAgcRunMin{0.0f};
    float m_wfAgcRunMax{0.0f};
    bool  m_wfAgcPrimed{false};

    // Rate-limit waterfall pushes per m_wfUpdatePeriodMs.
    qint64 m_wfLastPushMs{0};

    // ---- Phase 3G-8 commit 5: grid / scales renderer state ----

    bool  m_gridEnabled{true};
    bool  m_showZeroLine{false};
    bool  m_showFps{false};
    FreqLabelAlign m_freqLabelAlign{FreqLabelAlign::Center};

    QColor m_gridColor{255, 255, 255, 40};       // vertical freq grid
    QColor m_gridFineColor{255, 255, 255, 20};   // 1/5 step fine grid
    QColor m_hGridColor{255, 255, 255, 40};      // horizontal dBm grid
    QColor m_gridTextColor{255, 255, 0};         // yellow text default
    QColor m_zeroLineColor{255, 0, 0};           // red default (Thetis)
    QColor m_bandEdgeColor{255, 0, 0};           // red default (Thetis)

    // FPS overlay tracking
    int    m_fpsFrameCount{0};
    qint64 m_fpsLastUpdateMs{0};
    float  m_fpsDisplayValue{0.0f};

    // ---- VFO / filter overlay ----
    double m_vfoHz{0.0};
    int    m_filterLowHz{-2850};    // LSB default — from Thetis
    int    m_filterHighHz{-150};
    int    m_stepHz{100};           // tuning step size

    int    m_panIndex{0};            // for per-pan settings keys

    // ---- VFO flag widgets ----
    QMap<int, VfoWidget*> m_vfoWidgets;

    // ---- CTUN mode ----
    bool   m_ctunEnabled{true};  // true = SmartSDR-style (pan independent of VFO)
    enum class VfoOffScreen { None, Left, Right };
    VfoOffScreen m_vfoOffScreen{VfoOffScreen::None};
    void drawOffScreenIndicator(QPainter& p, const QRect& specRect, const QRect& wfRect);

    // ---- Mouse tracking overlay (QRhiWidget macOS workaround) ----
    QWidget* m_mouseOverlay{nullptr};

    // ---- Overlay menu ----
    SpectrumOverlayMenu* m_overlayMenu{nullptr};

    // ---- Coalesced settings save ----
    void scheduleSettingsSave();
    bool m_settingsSaveScheduled{false};

    // ---- Mouse state ----
    bool   m_draggingDbm{false};
    int    m_dragStartY{0};
    float  m_dragStartRef{0.0f};
    QPoint m_mousePos;              // for cursor frequency display
    bool   m_mouseInWidget{false};

    // Filter edge drag — from AetherSDR SpectrumWidget.h:429-432
    enum class FilterEdge { None, Low, High };
    FilterEdge m_draggingFilter{FilterEdge::None};
    int  m_filterDragStartX{0};     // pixel X at grab time
    int  m_filterDragStartHz{0};    // filter edge Hz at grab time

    // Passband center drag (slide-to-tune) — AetherSDR:434
    bool m_draggingVfo{false};

    // Divider drag (spectrum/waterfall split) — AetherSDR:419
    bool m_draggingDivider{false};

    // Pan drag (waterfall/spectrum drag to change center) — AetherSDR:425-427
    bool   m_draggingPan{false};
    int    m_panDragStartX{0};
    double m_panDragStartCenter{0.0};

    // Bandwidth drag (frequency scale bar) — AetherSDR:421-423
    bool   m_draggingBandwidth{false};
    int    m_bwDragStartX{0};
    double m_bwDragStartBw{0.0};

    // Filter edge grab zone — from AetherSDR line 1087: GRAB = 5
    static constexpr int kFilterGrab = 5;

#ifdef NEREUS_GPU_SPECTRUM
    bool m_rhiInitialized{false};

    // GPU pipeline init helpers
    void initWaterfallPipeline();
    void initOverlayPipeline();
    void initSpectrumPipeline();
    void renderGpuFrame(QRhiCommandBuffer* cb);

    // ---- Waterfall GPU resources ----
    QRhiGraphicsPipeline*       m_wfPipeline{nullptr};
    QRhiShaderResourceBindings* m_wfSrb{nullptr};
    QRhiBuffer*                 m_wfVbo{nullptr};
    QRhiBuffer*                 m_wfUbo{nullptr};
    QRhiTexture*                m_wfGpuTex{nullptr};
    QRhiSampler*                m_wfSampler{nullptr};
    int  m_wfGpuTexW{0};
    int  m_wfGpuTexH{0};
    bool m_wfTexFullUpload{true};
    int  m_wfLastUploadedRow{-1};

    // ---- Overlay GPU resources ----
    QRhiGraphicsPipeline*       m_ovPipeline{nullptr};
    QRhiShaderResourceBindings* m_ovSrb{nullptr};
    QRhiBuffer*                 m_ovVbo{nullptr};
    QRhiTexture*                m_ovGpuTex{nullptr};
    QRhiSampler*                m_ovSampler{nullptr};
    QImage m_overlayStatic;
    bool   m_overlayStaticDirty{true};
    bool   m_overlayNeedsUpload{true};

    // ---- FFT spectrum GPU resources ----
    QRhiGraphicsPipeline*       m_fftLinePipeline{nullptr};
    QRhiGraphicsPipeline*       m_fftFillPipeline{nullptr};
    QRhiShaderResourceBindings* m_fftSrb{nullptr};
    QRhiBuffer*                 m_fftLineVbo{nullptr};
    QRhiBuffer*                 m_fftFillVbo{nullptr};
    // From AetherSDR: kMaxFftBins = 8192, kFftVertStride = 6
    static constexpr int kMaxFftBins = 65536;
    static constexpr int kFftVertStride = 6;  // x, y, r, g, b, a
    int m_visibleBinCount{0};  // bins rendered this frame (for draw call count)

    void markOverlayDirty() {
        m_overlayStaticDirty = true;
        update();
    }
#endif
};

} // namespace NereusSDR
