#pragma once

// =================================================================
// src/gui/meters/FilterDisplayItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Fix wave round 2 (I7): legacyLayout, the old look's hit
//                 geometry. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-26 - R-R3-49 (remote-window parity Task 16): the
//                 high-resolution curve can come from a Core's bins
//                 (setFilterResponseBins) in a remote window, resampled as
//                 the local channel's is. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-26 - Thetis filter display settings (vertical ratio, limits,
//                 fixed zoom, scales, font scale, sideband mode, frame
//                 interval, grey outside the passband, snap, autozoom,
//                 characteristic and its low limit) and the missing
//                 colours, appended to the record; new items take
//                 kFilterDisplayThetisDefaults. A saved waterfall palette
//                 now loads as itself whatever the frame interval.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Thetis filter display render and input (renderFilterDisplay,
//                 MouseUp, MouseWheel, the Low/High/Shift/AdjustNotch
//                 setters): new items paint the Thetis look with the MNF,
//                 +MNF, SNAP and FIT buttons and take edge drags, notches
//                 and zoom; saved records without the Thetis-look field
//                 paint as before. The item only asks (filterEdgesRequested
//                 and the notch and TNF signals); MainWindow applies the
//                 requests through FilterDisplayRequests, and an item it has
//                 not armed (a preview copy) asks nothing.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Final review fix wave (C2, I7): the display transmits
//                 only while its own slice is the transmit slice and MOX
//                 is on (SliceFilterState::transmitSlice); saved items keep
//                 their look and take the input. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "MeterItem.h"
#include "core/WdspTypes.h"
#include <QColor>
#include <QFont>
#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>
#include <vector>

class QMenu;

namespace NereusSDR {

class NotchModel;
class RadioModel;
class RxChannel;
class SliceModel;
class TransmitModel;

// From Thetis clsFilterItem (MeterManager.cs:16852+)
// Mini passband spectrum/waterfall display with filter edge markers.
class FilterDisplayItem : public MeterItem {
    Q_OBJECT

public:
    // From Thetis FIDisplayMode enum (MeterManager.cs:16865)
    enum class DisplayMode { Panadapter, Waterfall, Panafall, None };

    // From Thetis FIWaterfallPalette enum
    enum class WaterfallPalette { Enhanced, Spectran, BlackWhite, LinLog, LinRad, LinAuto, Custom };

    // From Thetis MeterManager.cs:43362 [v2.10.3.15] MiniSpec.PIXELS.
    static constexpr int kSpectrumPixels = 1024;

    // Button labels drawn on the display. From Thetis MeterManager.cs:36195-36198
    // [v2.10.3.15]. Qt does not trim, so the labels carry only their padding
    // spaces and not Thetis's trailing zero-width space:
    // the + "\u200B" is for a zero-width space to preserve layout, otherwise direct write trims
    // [original inline comment from MeterManager.cs:36194]
    static constexpr const char* kMnfLabel = " MNF ";
    static constexpr const char* kMnfPlusLabel = " +MNF ";
    static constexpr const char* kSnapLabel = " SNAP ";
    static constexpr const char* kFitLabel = " FIT ";

    // Ranges the Thetis Apply clamps to. From Thetis MeterManager.cs:27651-27705
    // [v2.10.3.15] (GetSetting min, max).
    static constexpr float kVerticalRatioMin = 0.15f;
    static constexpr float kVerticalRatioMax = 1.0f;
    static constexpr float kRxZoomMin = 1.0f;
    static constexpr float kRxZoomMax = 10.0f;
    static constexpr float kTxZoomMin = 1.0f;
    static constexpr float kTxZoomMax = 20.0f;
    static constexpr float kScaleMin = 0.0f;
    static constexpr float kScaleMax = 10.0f;
    static constexpr float kFontScaleMin = 0.01f;
    static constexpr float kFontScaleMax = 4.0f;
    static constexpr int   kFrameIntervalMin = 1;
    static constexpr int   kFrameIntervalMax = 1000;
    static constexpr float kCharacteristicLowMin = -300.0f;
    static constexpr float kCharacteristicLowMax = -50.0f;

    // From Thetis MeterManager.cs:43361 [v2.10.3.15]: MiniSpec.TX_BANDWIDTH,
    // the TX extent (ExtentHZ while MOX).
    static constexpr int kTxExtentHz = 20000;
    // From Thetis console.cs:13221 [v2.10.3.15]: _max_filter_width = 10000,
    // the RX extent (FilterMaxWidth) when no frame says otherwise.
    static constexpr int kRxExtentHz = 10000;
    // From Thetis console.cs:12788 [v2.10.3.15]: default_low_cut = 150, the
    // USB/LSB edge the top-line right click resets to.
    static constexpr int kDefaultLowCutHz = 150;
    // From Thetis MeterManager.cs:18966, 19016 [v2.10.3.15]:
    // float step = Common.ShiftKeyDown ? 1 : 10;
    static constexpr float kStepHz = 10.0f;
    static constexpr float kFineStepHz = 1.0f;
    // From Thetis MeterManager.cs:35481-35486 [v2.10.3.15]: a sideband-mode
    // display is 1.9 widths wide, so one sideband fills the item.
    static constexpr float kSidebandWidthFactor = 1.9f;
    // From Thetis MeterManager.cs:35555 [v2.10.3.15]: 18f * font_scale.
    static constexpr float kBaseFontSize = 18.0f;
    // From Thetis MeterManager.cs:35583-35585 [v2.10.3.15]: autozoom margins
    // and limits.
    static constexpr float kAutoZoomTwoSidebandMargin = 0.85f;
    static constexpr float kAutoZoomSidebandMargin = 0.92f;
    static constexpr float kAutoZoomMax = 20.0f;
    // From Thetis MeterManager.cs:35898, 35902, 35906 [v2.10.3.15]:
    // isMouseNearLine thresholds for the edges (6) and the top line (12).
    static constexpr float kEdgeGrabPixels = 6.0f;
    static constexpr float kTopGrabPixels = 12.0f;
    // From Thetis MeterManager.cs:36110 [v2.10.3.15]: the notch hit area
    // reaches 2 pixels past the notch width.
    static constexpr float kNotchGrabPixels = 2.0f;
    // Snap grid defaults. From Thetis setup.designer.cs:12405, 12414, 12423
    // [v2.10.3.15] (txtFilter_other/cw/sideband_frequencies); NereusSDR has
    // no Setup page for them yet.
    static const QVector<float>& snapFrequencies(DSPMode mode);

    explicit FilterDisplayItem(QObject* parent = nullptr);

    // ── Thetis look (renderFilterDisplay, MeterManager.cs:35466-36270) ─────
    // New items (applyNewItemDefaults) and records that saved it on paint
    // the Thetis filter display and take its input. A record saved before
    // the field existed loads with it off and paints as it always has.
    void setThetisLook(bool on) { m_thetisLook = on; }
    bool thetisLook() const { return m_thetisLook; }

    // What MainWindow tells the display about its slice each frame. Edges
    // are the slice's (RX) and TransmitModel's (TX, positive magnitudes as
    // Thetis's TXFilterLow/High) filter in Hz; notch centres are RF Hz.
    struct NotchMark {
        int    id{0};
        double centreHz{0.0};
        double widthHz{0.0};
        bool   active{true};
    };
    struct SliceFilterState {
        int     sliceIndex{-1};
        DSPMode mode{DSPMode::USB};
        double  vfoHz{0.0};
        int     rxLowHz{0};
        int     rxHighHz{0};
        int     txLowHz{0};
        int     txHighHz{0};
        int     cwPitchHz{600};
        int     diglOffsetHz{0};
        int     diguOffsetHz{0};
        double  minNotchWidthHz{0.0};
        bool    tnfActive{false};
        QString filterName;
        QString txProfileName;
        QVector<NotchMark> notches;
        // True while this slice is the transmit slice. The display shows
        // and edits the TX filter only while this is true and MOX is on
        // (Thetis m.MOX = rx == m.RX && newMox, MeterManager.cs:5264
        // [v2.10.3.15]); any other slice shows its RX filter.
        bool    transmitSlice{false};
    };
    void setSliceFilterState(const SliceFilterState& state) { m_state = state; }
    const SliceFilterState& sliceFilterState() const { return m_state; }
    void clearSliceFilterState() { m_state = SliceFilterState{}; }

    // Only MainWindow arms an item, when it wires the requests to the
    // models. A preview copy is never armed, so it asks for nothing.
    void setRequestsEnabled(bool on) { m_requestsEnabled = on; }
    bool requestsEnabled() const { return m_requestsEnabled; }

    // Thetis Low/High getters (MeterManager.cs:19151-19278 [v2.10.3.15]):
    // the displayed edges in Hz from the slice's centre.
    float displayLowHz() const;
    float displayHighHz() const;

    // The notch menu (Thetis ShowNotchPopup, console.cs:33541): width,
    // bypass and remove, as the panadapter's notch menu offers them.
    void buildNotchMenu(int id, QMenu& menu);

    // Geometry shared by paint and the mouse handlers, so hit areas match
    // what is drawn. From Thetis renderFilterDisplay (MeterManager.cs:35466-
    // 35868 [v2.10.3.15]).
    struct ThetisLayout {
        bool   valid{false};
        QRectF itemRect;        // original_x / original_w
        float  x{0}, y{0}, w{0}, h{0};
        int    sidebandSign{0};
        float  zoom{1.0f};
        int    extentHz{kRxExtentHz};
        int    hzSpan{2 * kRxExtentHz};
        float  pixelSpan{0};    // extent_h - extent_l
        QFont  font;
        float  textHeight{0};   // tsl.Height
        float  maxExtentTextWidth{0};
        float  zeroWidth{0};
        float  zeroHeight{0};
        float  lineWidth{2};
        float  slopePixels{0};
        float  zoomDiffPixels{0};
        float  topLineY{0};
        float  botLineY{0};
        float  extentL{0};
        float  extentH{0};
        float  spectrumHeight{0};
        float  topPos{0};
        float  botPos{0};
        float  centre{0};
        float  textY{0};
        QRectF mnfRect;
        QRectF mnfPlusRect;
        QRectF snapRect;
        QRectF fitRect;
        float  lowHz{0};
        float  highHz{0};
        float  filterL{0};
        float  filterH{0};
        QPointF lowBot, lowTop, highBot, highTop, topLeft, topRight;
    };
    // The hit layout: Thetis's, or for the old look (no m_thetisLook) the
    // geometry that look draws (legacyLayout).
    ThetisLayout thetisLayout(int widgetW, int widgetH) const;
    ThetisLayout legacyLayout(int widgetW, int widgetH) const;
    float hzToPixels(double hz, float pixelSpan, int hzSpan) const;
    double pixelsToHz(float pixels, float pixelSpan, int hzSpan) const;

    bool hitTest(const QPointF& pos, int widgetW, int widgetH) const override;
    bool handleMousePress(QMouseEvent* event, int widgetW, int widgetH) override;
    bool handleMouseRelease(QMouseEvent* event, int widgetW, int widgetH) override;
    bool handleMouseMove(QMouseEvent* event, int widgetW, int widgetH) override;
    bool handleWheel(QWheelEvent* event, int widgetW, int widgetH) override;

    void setDisplayMode(DisplayMode m) {
        if (m_displayMode == m) { return; }
        m_displayMode = m;
        emit displayModeChanged();
    }
    DisplayMode displayMode() const { return m_displayMode; }

    // Spectrum data feed (called externally, e.g., by FFTEngine)
    void setSpectrumData(const float* bins, int count);
    // A delivered analyzer frame owns its RF geometry and accepted count.
    // Waterfall history advances on a delivered row, never on a paint.
    void presentFrame(const QVector<float>& traceDbm,
                      const QVector<float>& waterfallDbm,
                      double centreHz, double spanHz, bool transmit,
                      bool waterfallAdvance);
    void clearFrame();
    bool frameAvailable() const { return m_frameAvailable; }
    int framePixels() const { return int(m_spectrumData.size()); }
    int tracePeakPixelForTest() const;
    double frameCentreHz() const { return m_frameCentreHz; }
    double frameSpanHz() const { return m_frameSpanHz; }
    bool frameTransmitting() const { return m_frameTransmitting; }
    const QImage& waterfallImageForTest() const { return m_waterfallImage; }
    int rxLowPixelForTest() const { return m_rxLow; }
    int rxHighPixelForTest() const { return m_rxHigh; }

    // Filter edges (in accepted-frame pixels)
    void setFilterEdgesRx(int low, int high) { m_rxLow = low; m_rxHigh = high; }
    void setFilterEdgesTx(int low, int high) { m_txLow = low; m_txHigh = high; }

    // Notch positions (in pixels)
    void setNotchPositions(const std::vector<int>& positions) { m_notchPositions = positions; }

    // Colors
    void setDataLineColour(const QColor& c) { m_dataLineColour = c; }
    QColor dataLineColour() const { return m_dataLineColour; }
    void setDataFillColour(const QColor& c) { m_dataFillColour = c; }
    QColor dataFillColour() const { return m_dataFillColour; }
    void setEdgesColourRX(const QColor& c) { m_edgesColourRX = c; }
    QColor edgesColourRX() const { return m_edgesColourRX; }
    void setEdgesColourTX(const QColor& c) { m_edgesColourTX = c; }
    QColor edgesColourTX() const { return m_edgesColourTX; }
    void setNotchColour(const QColor& c) { m_notchColour = c; }
    QColor notchColour() const { return m_notchColour; }
    void setMeterBackColour(const QColor& c) { m_meterBackColour = c; }
    QColor meterBackColour() const { return m_meterBackColour; }
    void setTextColour(const QColor& c) { m_textColour = c; }
    QColor textColour() const { return m_textColour; }
    void setNumberHighlightColour(const QColor& c) { m_numberHighlightColour = c; }
    QColor numberHighlightColour() const { return m_numberHighlightColour; }
    void setEdgeHighlightColour(const QColor& c) { m_edgeHighlightColour = c; }
    QColor edgeHighlightColour() const { return m_edgeHighlightColour; }
    void setExtentsColour(const QColor& c) { m_extentsColour = c; }
    QColor extentsColour() const { return m_extentsColour; }
    void setNotchHighlightColour(const QColor& c) { m_notchHighlightColour = c; }
    QColor notchHighlightColour() const { return m_notchHighlightColour; }
    void setSnapLineColour(const QColor& c) { m_snapLineColour = c; }
    QColor snapLineColour() const { return m_snapLineColour; }
    void setSettingOnColour(const QColor& c) { m_settingOnColour = c; }
    QColor settingOnColour() const { return m_settingOnColour; }
    void setButtonHighlightColour(const QColor& c) { m_buttonHighlightColour = c; }
    QColor buttonHighlightColour() const { return m_buttonHighlightColour; }

    // Thetis filter display settings (MeterManager.cs:27651-27705
    // [v2.10.3.15]). Stored and saved per item; the editor enforces the
    // Thetis ranges above.
    // Vertical ratio is the Thetis Padding (filterdisplay_vertical_ratio).
    void setVerticalRatio(float r) { m_padding = r; }
    float verticalRatio() const { return m_padding; }
    void setShowFilterLimits(bool on) { m_showFilterLimits = on; }
    bool showFilterLimits() const { return m_showFilterLimits; }
    void setFixedRxZoom(bool on) { m_fixedRxZoom = on; }
    bool fixedRxZoom() const { return m_fixedRxZoom; }
    void setFixedTxZoom(bool on) { m_fixedTxZoom = on; }
    bool fixedTxZoom() const { return m_fixedTxZoom; }
    void setRxZoom(float z) { m_rxZoom = z; }
    float rxZoom() const { return m_rxZoom; }
    void setTxZoom(float z) { m_txZoom = z; }
    float txZoom() const { return m_txZoom; }
    void setSidebandsScale(float s) { m_sidebandsScale = s; }
    float sidebandsScale() const { return m_sidebandsScale; }
    void setCwScale(float s) { m_cwScale = s; }
    float cwScale() const { return m_cwScale; }
    void setOthersScale(float s) { m_othersScale = s; }
    float othersScale() const { return m_othersScale; }
    void setFontScale(float s) { m_fontScale = s; }
    float fontScale() const { return m_fontScale; }
    void setSidebandMode(bool on) { m_sidebandMode = on; }
    bool sidebandMode() const { return m_sidebandMode; }
    // From Thetis FrameInterval setter (MeterManager.cs:18688-18704
    // [v2.10.3.15]): a new interval restarts the frame count.
    void setWaterfallFrameInterval(int frames)
    {
        m_waterfallFrameInterval = frames;
        m_waterfallFrameCount = 0;
    }
    int waterfallFrameInterval() const { return m_waterfallFrameInterval; }
    // Thetis Greyscale (filterdisplay_use_grey): grey when not active.
    void setGreyWhenNotActive(bool on) { m_greyWhenNotActive = on; }
    bool greyWhenNotActive() const { return m_greyWhenNotActive; }
    void setSnapLines(bool on) { m_snapLines = on; }
    bool snapLines() const { return m_snapLines; }
    void setAutoZoom(bool on) { m_autoZoom = on; }
    bool autoZoom() const { return m_autoZoom; }
    void setShowCharacteristic(bool on) { m_showCharacteristic = on; }
    bool showCharacteristic() const { return m_showCharacteristic; }
    void setCharacteristicLow(float dbm) { m_characteristicLow = dbm; }
    float characteristicLow() const { return m_characteristicLow; }

    // New items only (ContainerContentRegistry::makeEntry): copies
    // kFilterDisplayThetisDefaults. Saved records keep their look.
    void applyNewItemDefaults() override;

    void setFillSpectrum(bool f) { m_fillSpectrum = f; }
    bool fillSpectrum() const { return m_fillSpectrum; }
    void setPadding(float p) { m_padding = p; }
    float padding() const { return m_padding; }
    void setWaterfallPalette(WaterfallPalette pal) { m_waterfallPalette = pal; }
    WaterfallPalette waterfallPalette() const { return m_waterfallPalette; }
    void setWaterfallLowColour(const QColor& color) { m_waterfallLowColour = color; }
    QColor waterfallLowColour() const { return m_waterfallLowColour; }
    void setCustomWaterfallGradient(const QVector<QColor>& rx,
                                    const QVector<QColor>& tx) {
        m_customRxGradient = rx;
        m_customTxGradient = tx;
    }

    // Spectrum range
    void setSpecGridRange(float minDb, float maxDb) { m_specMinDb = minDb; m_specMaxDb = maxDb; }
    float specMinDb() const { return m_specMinDb; }
    float specMaxDb() const { return m_specMaxDb; }

    // ── High-resolution filter characteristics (Task 4.4) ─────────────────────
    // When enabled, paintFilterEdges overlays the actual computed FIR magnitude
    // curve from RxChannel::filterResponseMagnitudes() in addition to the edge
    // markers.  The change takes effect on the next paint frame.
    void setHighResolution(bool h);
    bool highResolution() const { return m_highResolution; }

    // Bind the RxChannel whose filterResponseMagnitudes() provides the FIR curve.
    // Non-owning — caller is responsible for clearing before the channel is destroyed.
    void bindRxChannel(RxChannel* channel);
    // R-R3-49 (parity Task 16): a remote window has no channel; it sets the
    // Core's filter response bins (magnitudes, DC to Nyquist) here, and the
    // curve is resampled from them as RxChannel::filterResponseMagnitudes
    // resamples its own. Used only while no channel is bound. Empty: none.
    void setFilterResponseBins(const QVector<double>& binMagnitudes)
    { m_filterResponseBins = binMagnitudes; }
    void setRfFilterResponse(const QVector<double>& magnitudesDb,
                             double startHz, double stepHz, double referenceHz);
    void setRfMarkers(double rxLowHz, double rxHighHz, double txLowHz,
                      double txHighHz, const QVector<double>& notchCentresHz);
    const QVector<double>& filterResponseBinsForTest() const { return m_filterResponseBins; }

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paint(QPainter& p, int widgetW, int widgetH) override;
    QString serialize() const override;
    bool deserialize(const QString& data) override;

signals:
    void displayModeChanged();
    // Requests MainWindow applies (FilterDisplayRequests). tx: the edges
    // are TransmitModel's FilterLow/FilterHigh (positive magnitudes), not
    // the slice's. Changing the TX filter never keys the radio.
    void filterEdgesRequested(int slice, int lowHz, int highHz, bool tx);
    // Thetis Shift (MeterManager.cs:19378-19412 [v2.10.3.15]): the whole
    // passband moves, constrained with filterShift (SliceModel::shiftFilter).
    void filterShiftRequested(int slice, int lowHz, int highHz);
    void notchAddRequested(int slice, double hz);
    void notchChangeRequested(int id, double hz, double widthHz);
    void notchActiveRequested(int id, bool active);
    void notchRemoveRequested(int id);
    void tnfToggleRequested();
    void tnfAddRequested(int slice);
    // Right click on a notch: MainWindow pops a menu it owns, filled by
    // buildNotchMenu(id, menu).
    void notchMenuRequested(int id, const QPoint& globalPos);

private:
    // Thetis look.
    void paintThetis(QPainter& p, int widgetW, int widgetH);
    void paintThetisSpectrum(QPainter& p, const ThetisLayout& l);
    void paintThetisWaterfall(QPainter& p, const ThetisLayout& l);
    void paintThetisCharacteristic(QPainter& p, const ThetisLayout& l,
                                   const QColor& lineColour);
    QSizeF plotText(QPainter& p, const ThetisLayout& l, const QString& text,
                    float x, float y, const QColor& colour, int alpha,
                    bool alignRight, bool alignCentre, bool fillBackground,
                    const QColor& back) const;
    QVector<float> greyscale(const ThetisLayout& l) const;
    // Hover analysis from renderFilterDisplay, for the stored mouse point.
    struct Hover {
        bool onButton{false};
        bool low{false};
        bool high{false};
        bool top{false};
        int  notchId{-1};
        bool mnf{false};
        bool mnfPlus{false};
        bool snap{false};
        bool fit{false};
        bool adjustLow{false};
        bool adjustHigh{false};
        bool adjustShift{false};
    };
    Hover hoverAt(const ThetisLayout& l, const QPointF& mouse) const;
    bool interactive() const;
    // The TX state the display shows and edits: MOX on this display's
    // slice only. An item with no slice (a preview copy) follows MOX.
    bool txActive() const;
    bool filterEnabled() const;   // Thetis RXEnabled / TXEnabled
    bool canAdjust() const;       // Thetis CanAdjust
    bool twoSidebands() const;    // console.CurrentDSPhasTwoSidebands
    int  sidebandModeSign() const;
    int  modeSign() const;
    bool isCw() const;
    int  extentHz() const;
    // Thetis setters: they emit the request instead of calling the console.
    void requestLow(float value, Qt::KeyboardModifiers mods, bool ignoreSnap);
    void requestHigh(float value, Qt::KeyboardModifiers mods, bool ignoreSnap);
    void requestShift(float shiftHz, Qt::KeyboardModifiers mods);
    void requestNotchMove(float deltaHz, Qt::KeyboardModifiers mods);
    void resetFilterPosition();
    int  findNearestVGrid(int hz) const;
    const NotchMark* notchById(int id) const;
    void clearSelection();

    bool m_thetisLook{false};
    bool m_requestsEnabled{false};
    SliceFilterState m_state;
    // Mouse state (Thetis MouseMovePoint, MouseButtonDown, the selections).
    QPointF m_mousePoint{-1.0e9, -1.0e9};
    bool m_mouseInside{false};
    bool m_mouseDown{false};
    Qt::MouseButton m_mouseButton{Qt::NoButton};
    bool m_lowSelected{false};
    bool m_highSelected{false};
    bool m_topSelected{false};
    int  m_selectedNotchId{-1};
    double m_notchStartHz{-1.0};
    float m_startShiftX{0.0f};
    float m_startLow{0.0f};
    float m_startHigh{0.0f};
    bool m_pressHandled{false};

    void paintSpectrum(QPainter& p, const QRect& rect);
    void paintWaterfall(QPainter& p, const QRect& rect);
    void paintFilterEdges(QPainter& p, const QRect& rect);
    // High-res FIR magnitude curve overlay (Task 4.4, design Section 4D).
    // Called from paintFilterEdges() when m_highResolution, with a channel
    // or (parity Task 16) the Core's bins.
    void paintHighResolutionFilterCurve(QPainter& p, const QRect& rect);
    void paintNotches(QPainter& p, const QRect& rect);
    QColor dbToWaterfallColor(float db) const;

    DisplayMode m_displayMode{DisplayMode::Panafall};
    WaterfallPalette m_waterfallPalette{WaterfallPalette::Enhanced};

    // Spectrum data in the accepted RF window.
    std::vector<float> m_spectrumData;
    bool m_frameAvailable{false};
    bool m_frameTransmitting{false};
    double m_frameCentreHz{0.0};
    double m_frameSpanHz{0.0};

    // Waterfall: rolling image, shift down 1 row per frame
    QImage m_waterfallImage;
    int m_waterfallFrameCount{0};
    int m_waterfallFrameInterval{4}; // update every Nth frame

    // Filter edges (pixel positions 0-511)
    int m_rxLow{-1};
    int m_rxHigh{-1};
    int m_txLow{-1};  // -1 = don't show
    int m_txHigh{-1};

    // Notch positions
    std::vector<int> m_notchPositions;

    // Colors (from Thetis clsFilterItem defaults MeterManager.cs:17012+)
    QColor m_dataLineColour{0x32, 0xcd, 0x32};       // LimeGreen
    QColor m_dataFillColour{0x32, 0xcd, 0x32, 0x40}; // LimeGreen alpha
    QColor m_edgesColourRX{0xff, 0xff, 0x00};         // Yellow
    QColor m_edgesColourTX{0xff, 0x00, 0x00};         // Red
    QColor m_notchColour{0xff, 0xa5, 0x00};           // Orange
    QColor m_meterBackColour{0x00, 0x00, 0x00};       // Black
    QColor m_textColour{0xff, 0xff, 0xff};             // White
    // From Thetis clsFilterItem ctor (MeterManager.cs:18337-18348
    // [v2.10.3.15]).
    QColor m_numberHighlightColour{0x8b, 0x00, 0x00};  // DarkRed
    QColor m_edgeHighlightColour{0xff, 0xff, 0xff};    // White
    QColor m_extentsColour{0x80, 0x80, 0x80};          // Gray
    QColor m_notchHighlightColour{0x32, 0xcd, 0x32};   // LimeGreen
    QColor m_snapLineColour{0x80, 0x80, 0x80};         // Gray
    QColor m_settingOnColour{0x64, 0x95, 0xed};        // CornflowerBlue
    QColor m_buttonHighlightColour{0x80, 0x80, 0x80};  // Gray
    // From Thetis clsFilterItem ctor (MeterManager.cs:18310-18322,
    // 18350-18353 [v2.10.3.15]).
    bool   m_showFilterLimits{true};
    bool   m_fixedRxZoom{false};
    bool   m_fixedTxZoom{false};
    float  m_rxZoom{1.0f};
    float  m_txZoom{1.0f};
    float  m_sidebandsScale{0.0f};
    float  m_cwScale{0.0f};
    float  m_othersScale{0.0f};
    float  m_fontScale{1.0f};
    bool   m_sidebandMode{false};
    bool   m_greyWhenNotActive{true};
    bool   m_snapLines{false};
    bool   m_autoZoom{false};
    bool   m_showCharacteristic{false};
    float  m_characteristicLow{-250.0f};
    bool   m_fillSpectrum{true};
    float  m_padding{0.02f};
    float  m_specMinDb{-140.0f};
    float  m_specMaxDb{-40.0f};
    float  m_autoWaterfallMinDb{-140.0f};
    float  m_autoWaterfallMaxDb{-40.0f};
    QColor m_waterfallLowColour{Qt::black};
    QVector<QColor> m_customRxGradient;
    QVector<QColor> m_customTxGradient;

    // High-resolution filter characteristics (Task 4.4)
    bool      m_highResolution{false};
    RxChannel* m_rxChannel{nullptr};  // non-owning
    QVector<double> m_filterResponseBins;  // parity Task 16: the Core's
    QVector<double> m_rfFilterResponseDb;
    double m_rfFilterStartHz{0.0};
    double m_rfFilterStepHz{0.0};
    double m_rfFilterReferenceHz{0.0};
};

// New FILTERDISPLAY items (makeEntry) take the values Thetis applies to a
// filter display. From Thetis MeterManager.cs:27651-27705 [v2.10.3.15]
// (ApplySettingsForMeterGroup, FILTER_DISPLAY: the GetSetting defaults);
// colours in Apply order, each named by its field in the clsFilterItem
// constructor (MeterManager.cs:18316-18350 [v2.10.3.15]).
struct FilterDisplayItemThetisDefaults {
    float verticalRatio;
    bool  showFilterLimits;
    bool  fixedRxZoom;
    bool  fixedTxZoom;
    float rxZoom;
    float txZoom;
    float sidebandsScale;
    float cwScale;
    float othersScale;
    FilterDisplayItem::DisplayMode displayMode;
    float fontScale;
    QRgb  dataLineColour;
    QRgb  waterfallLowColour;
    QRgb  textColour;
    QRgb  numberHighlightColour;
    QRgb  edgesColourRx;
    QRgb  edgesColourTx;
    QRgb  edgeHighlightColour;
    QRgb  meterBackColour;
    QRgb  notchColour;
    QRgb  notchHighlightColour;
    QRgb  extentsColour;
    bool  sidebandMode;
    int   frameInterval;
    bool  greyWhenNotActive;
    bool  snapLines;
    bool  autoZoom;
    QRgb  snapLineColour;
    QRgb  settingOnColour;
    QRgb  buttonHighlightColour;
    bool  showCharacteristic;
    float characteristicLow;
};
inline constexpr FilterDisplayItemThetisDefaults kFilterDisplayThetisDefaults{
    0.2f,                                   // filterdisplay_vertical_ratio 0.2f
    true,                                   // filterdisplay_show_filter_limits true
    false,                                  // filterdisplay_show_fixed_rx_zoom false
    false,                                  // filterdisplay_show_fixed_tx_zoom false
    1.0f,                                   // filterdisplay_rx_zoom 1f
    1.0f,                                   // filterdisplay_tx_zoom 1f
    0.0f,                                   // filterdisplay_sidebands_scale 0f
    0.0f,                                   // filterdisplay_cw_scale 0f
    0.0f,                                   // filterdisplay_others_scale 0f
    FilterDisplayItem::DisplayMode::Panafall, // filterdisplay_others_displaymode PANAFALL
    1.0f,                                   // filterdisplay_font_scale 1f
    qRgb(0x32, 0xcd, 0x32),                 // _dataline_colour = LimeGreen
    qRgb(0x00, 0x00, 0x00),                 // _waterfall_low_colour = Black
    qRgb(0xff, 0xff, 0xff),                 // _text_colour = White
    qRgb(0x8b, 0x00, 0x00),                 // _number_highlight_colour = DarkRed
    qRgb(0xff, 0xff, 0x00),                 // _edges_colour_rx = Yellow
    qRgb(0xff, 0x00, 0x00),                 // _edges_colour_tx = Red
    qRgb(0xff, 0xff, 0xff),                 // _edge_highlight_colour = White
    qRgb(0x00, 0x00, 0x00),                 // _meterback_colour = Black
    qRgb(0xff, 0x45, 0x00),                 // _notch_colour = OrangeRed (Apply)
    qRgb(0x32, 0xcd, 0x32),                 // _notchhighligh_colour = LimeGreen
    qRgb(0x80, 0x80, 0x80),                 // _extents_colour = Gray
    false,                                  // filterdisplay_sideband_mode false
    4,                                      // filterdisplay_waterfall_frameupdate 4
    true,                                   // filterdisplay_use_grey true
    false,                                  // filterdisplay_snap_lines false
    false,                                  // filterdisplay_autozoom false
    qRgb(0x80, 0x80, 0x80),                 // _snapline_colour = Gray
    qRgb(0x64, 0x95, 0xed),                 // _setting_on_colour = CornflowerBlue
    qRgb(0x80, 0x80, 0x80),                 // _button_highlight_colour = Gray
    false,                                  // filterdisplay_characteristic false
    -250.0f,                                // filterdisplay_characteristic_low -250f
};

// How MainWindow applies a filter display's requests. Kept here so the
// routing (RX edges to the slice, TX edges to TransmitModel, no notch
// added while transmitting) is testable without a window.
namespace FilterDisplayRequests {
// tx: TransmitModel::setFilterLow/setFilterHigh; else SliceModel::setFilter.
void applyEdges(SliceModel* slice, TransmitModel* transmit, int lowHz,
                int highHz, bool tx);
void applyShift(SliceModel* slice, int lowHz, int highHz);
void applyNotchChange(NotchModel* notches, int id, double hz, double widthHz);
void applyTnfToggle(NotchModel* notches);
// Refused (false) while the radio transmits, as Thetis refuses
// (MeterManager.cs:18949, 18997 [v2.10.3.15]).
bool applyNotchAdd(RadioModel* radio, SliceModel* slice, double hz);
bool applyTnfAdd(RadioModel* radio, SliceModel* slice);
} // namespace FilterDisplayRequests

} // namespace NereusSDR
