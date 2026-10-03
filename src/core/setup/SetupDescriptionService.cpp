// no-port-check: NereusSDR-original Setup description transport.
// 2026-09-29: setOnAirState, one revision per on-air edge. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-30: Hardware version 23, Calibration's Rx1 6m LNA row. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-30: Audio version 24 (radio codec lane): Line In Gain in 1.5 dB
// steps, the Saturn G2's Mic Tip-Ring row. J.J. Boyd (KG4VCF), AI-assisted
// via Anthropic Claude Code.
#include "core/setup/SetupDescriptionService.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/MirrorSchema.h"
#include "core/session/MirrorPolicy.h"
#include "models/StationTciModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"
#include "models/NotchModel.h"
#include "models/Band.h"
#include "core/accessories/AlexAntennaFacade.h"
#include "core/session/TransmitStateFacade.h"
#include "core/SkuUiProfile.h"
#include "core/PaCalProfile.h"
#include "core/RadioInfoFacts.h"
#include "core/ControlRanges.h"
#include "core/settings/SettingsScope.h"
#include "core/AlexSettingsKeys.h"
#include "core/SampleRateCatalog.h"
#include "core/codec/AlexFilterMap.h"
#include "core/setup/SetupDescriptionV15.h"
#include "core/dsp/DspAssetService.h"
#include "models/AccessoryDataModel.h"
#include "models/AccessorySettingsModel.h"
#include "models/AmplifierModel.h"
#include "models/RadioModel.h"
#include "models/RfKitModel.h"
#include "models/TunerModel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

#include <cmath>

static void initializeSetupResources()
{
    Q_INIT_RESOURCE(setup);
}

namespace NereusSDR {
namespace {

// Display and Appearance version 12 (R-IOS-18, R-IOS-27): the rest of
// Setup > Display after Spectrum Peaks, and Appearance's Reset Colors. Each
// described row is closed: the resource must carry exactly this object. The
// rows are phone-owned (binding.phone is the desktop's own key); buttons are
// phone actions whose semantics docs/architecture/2026-09-23-setup-
// description-v1.md defines, the same on the desktop, local or remote.
// Each control is its own raw string piece (MSVC limits one piece to 16 KB).
constexpr char kDisplayV12Controls[] =
    R"json([)json"
    R"json({"id":"display.spectrumDefaults.smoothDefaults","label":"Reset to Smooth Defaults","tooltip":"Overwrite this panadapter's spectrum and waterfall look with the NereusSDR smooth-default profile: Clarity Blue palette, log-recursive averaging with a 650 ms averaging time, a white trace without fill, waterfall AGC on and a 30 ms waterfall update period. FFT size, frequency, band stack, and per-band grid ranges are not affected.","kind":"button","binding":{"phone":"smoothDefaults"},"applies":"live","requiresDescriptionVersion":12,"confirm":"This will overwrite your current Spectrum and Waterfall display settings with the NereusSDR smooth-default profile.\n\nYour FFT size, frequency, band stack, and per-band grid ranges are NOT affected.\n\nContinue?"},)json"
    R"json({"id":"display.spectrumDefaults.clarity","label":"Enable Clarity (adaptive waterfall tuning)","tooltip":"Clarity keeps the waterfall thresholds centered on the actual noise floor as band conditions and tuning change. Uses a 30th-percentile estimator with 3-second EWMA smoothing and a \u00b12 dB deadband. When off, thresholds are fixed at their last values.","kind":"toggle","binding":{"phone":"ClarityEnabled"},"applies":"subscription","requiresDescriptionVersion":12,"default":true,"gate":{"capability":"displayExtrasVersion","min":1}},)json"
    R"json({"id":"display.spectrumDefaults.showCursorFreq","label":"Show cursor frequency","tooltip":"Display the frequency at the cursor position (always in MHz). Same toggle as the on-spectrum overlay-panel Cursor Freq button.","kind":"toggle","binding":{"phone":"DisplayShowCursorFreq"},"applies":"live","requiresDescriptionVersion":12,"default":true},)json"
    R"json({"id":"display.spectrumDefaults.showBinWidth","label":"Show bin width","tooltip":"Display the current FFT bin width (sample rate / FFT size) in the spectrum corner.","kind":"toggle","binding":{"phone":"DisplayShowBinWidth"},"applies":"live","requiresDescriptionVersion":12,"default":false},)json"
    R"json({"id":"display.spectrumDefaults.showNoiseFloor","label":"Show noise floor","tooltip":"Display the noise floor as a horizontal dashed line + dBm box+text.","kind":"toggle","binding":{"phone":"DisplayShowNoiseFloor"},"applies":"subscription","requiresDescriptionVersion":12,"default":false,"gate":{"capability":"displayExtrasVersion","min":1}},)json"
    R"json({"id":"display.spectrumDefaults.noiseFloorShift","label":"NF shift:","tooltip":"Operator-tunable offset added to the rendered NF level, from -12 to +12 dB.","kind":"decimal","binding":{"phone":"DisplayNoiseFloorShiftDb"},"applies":"subscription","requiresDescriptionVersion":12,"min":-12,"max":12,"step":0.5,"decimals":1,"unit":"dB","default":0,"gate":{"capability":"displayExtrasVersion","min":1}},)json"
    R"json({"id":"display.spectrumDefaults.noiseFloorLineWidth","label":"Line width:","tooltip":"Width of the horizontal NF dashed line.","kind":"decimal","binding":{"phone":"DisplayNoiseFloorLineWidth"},"applies":"live","requiresDescriptionVersion":12,"min":1,"max":5,"step":0.5,"decimals":1,"unit":"px","default":1,"gate":{"capability":"displayExtrasVersion","min":1}},)json"
    R"json({"id":"display.spectrumDefaults.noiseFloorColor","label":"Line:","tooltip":"Color for the NF line + 8x8 box.","kind":"colour","binding":{"phone":"DisplayNoiseFloorColor"},"applies":"live","requiresDescriptionVersion":12,"default":"#FF40FFFF","gate":{"capability":"displayExtrasVersion","min":1}},)json"
    R"json({"id":"display.spectrumDefaults.noiseFloorTextColor","label":"Text:","tooltip":"Color for the NF dBm label text.","kind":"colour","binding":{"phone":"DisplayNoiseFloorTextColor"},"applies":"live","requiresDescriptionVersion":12,"default":"#FFFF00FF","gate":{"capability":"displayExtrasVersion","min":1}},)json"
    R"json({"id":"display.spectrumDefaults.noiseFloorFastColor","label":"Fast-attack:","tooltip":"Color shown during fast-attack (band/freq/MOX change).","kind":"colour","binding":{"phone":"DisplayNoiseFloorFastColor"},"applies":"live","requiresDescriptionVersion":12,"default":"#C8C8C8FF","gate":{"capability":"displayExtrasVersion","min":4}},)json"
    R"json({"id":"display.spectrumDefaults.normalize","label":"Normalize trace","tooltip":"Normalize the spectrum trace to a 1 Hz reference bandwidth. Only active for Average, Sample, or RMS detector modes.","kind":"toggle","binding":{"phone":"DisplayDispNormalize"},"applies":"subscription","requiresDescriptionVersion":12,"enabledWhen":{"phone":"DisplaySpectrumDetector","oneOf":[2,3,4]},"default":false,"gate":{"capability":"displayExtrasVersion","min":1}},)json"
    R"json({"id":"display.spectrumDefaults.showPeakValue","label":"Show peak value overlay","tooltip":"Display the peak signal level and frequency as a text overlay in the spectrum corner.","kind":"toggle","binding":{"phone":"DisplayShowPeakValueOverlay"},"applies":"live","requiresDescriptionVersion":12,"default":false},)json"
    R"json({"id":"display.spectrumDefaults.peakValuePosition","label":"Peak value position:","tooltip":"Corner position for the peak value readout.","kind":"choice","binding":{"phone":"DisplayPeakValuePosition"},"applies":"live","requiresDescriptionVersion":12,"options":[{"value":0,"label":"Top Left"},{"value":1,"label":"Top Right"},{"value":2,"label":"Bottom Left"},{"value":3,"label":"Bottom Right"}],"default":1},)json"
    R"json({"id":"display.spectrumDefaults.peakTextDelay","label":"Peak value refresh:","tooltip":"Refresh interval for the peak value overlay in milliseconds.","kind":"integer","binding":{"phone":"DisplayPeakTextDelayMs"},"applies":"live","requiresDescriptionVersion":12,"min":50,"max":10000,"step":50,"unit":"ms","default":500},)json"
    R"json({"id":"display.spectrumDefaults.getMonitorHz","label":"Get Monitor Hz","tooltip":"Query the primary screen refresh rate and snap the FPS slider to the nearest valid value.","kind":"button","binding":{"phone":"getMonitorHz"},"applies":"live","requiresDescriptionVersion":12},)json"
    R"json({"id":"display.waterfallDefaults.highThreshold","label":"High Threshold:","tooltip":"Waterfall High Signal - Show High Color above this value (gradient in between).","kind":"slider","binding":{"phone":"DisplayWfHighLevel"},"applies":"subscription","requiresDescriptionVersion":12,"min":-200,"max":0,"step":1,"unit":"dBm","enabledWhen":{"phone":"DisplayWfUseSpectrumMinMax","oneOf":[false]},"default":-62,"gate":{"capability":"displayExtrasVersion","min":1}},)json"
    R"json({"id":"display.waterfallDefaults.lowThreshold","label":"Low Threshold:","tooltip":"Waterfall Low Signal - Show Low Color below this value (gradient in between).","kind":"slider","binding":{"phone":"DisplayWfLowLevel"},"applies":"subscription","requiresDescriptionVersion":12,"min":-200,"max":0,"step":1,"unit":"dBm","enabledWhen":{"phone":"DisplayWfUseSpectrumMinMax","oneOf":[false]},"default":-122,"gate":{"capability":"displayExtrasVersion","min":1}},)json"
    R"json({"id":"display.waterfallDefaults.agc","label":"AGC","tooltip":"Automatically calculates Low Level Threshold for Waterfall.","kind":"toggle","binding":{"phone":"DisplayWfAgc"},"applies":"subscription","requiresDescriptionVersion":12,"enabledWhen":{"phone":"DisplayWfUseSpectrumMinMax","oneOf":[false]},"default":true,"gate":{"capability":"displayExtrasVersion","min":1}},)json"
    R"json({"id":"display.waterfallDefaults.useSpectrumMinMax","label":"Use spectrum min/max","tooltip":"Spectrum Grid min/max used for low and high level","kind":"toggle","binding":{"phone":"DisplayWfUseSpectrumMinMax"},"applies":"subscription","requiresDescriptionVersion":12,"default":false,"gate":{"capability":"displayExtrasVersion","min":1}},)json"
    R"json({"id":"display.waterfallDefaults.copySpectrumMinMax","label":"Copy spectrum min/max \u2192 waterfall thresholds","tooltip":"Copies the current spectrum display dB max and dB min values into the waterfall High Threshold and Low Threshold above.","kind":"button","binding":{"phone":"copySpectrumMinMax"},"applies":"live","requiresDescriptionVersion":12},)json"
    R"json({"id":"display.waterfallDefaults.nfAgc","label":"Enable NF-AGC","tooltip":"When enabled, the waterfall low/high thresholds automatically track the estimated noise floor. The offset below sets how far below the noise floor the low threshold is placed.","kind":"toggle","binding":{"phone":"WaterfallNFAGCEnabled"},"applies":"subscription","requiresDescriptionVersion":12,"enabledWhen":{"phone":"DisplayWfUseSpectrumMinMax","oneOf":[false]},"default":false,"gate":{"capability":"displayExtrasVersion","min":1}},)json"
    R"json({"id":"display.waterfallDefaults.nfAgcOffset","label":"NF offset:","tooltip":"Offset applied above the estimated noise floor when computing the waterfall low threshold. Negative values place the low threshold below the noise floor (recommended).","kind":"integer","binding":{"phone":"WaterfallAGCOffsetDb"},"applies":"subscription","requiresDescriptionVersion":12,"min":-60,"max":60,"step":1,"unit":"dB","enabledWhen":{"phone":"DisplayWfUseSpectrumMinMax","oneOf":[false]},"default":0,"gate":{"capability":"displayExtrasVersion","min":1}},)json"
    R"json({"id":"display.waterfallDefaults.colorScheme","label":"Color Scheme:","tooltip":"Waterfall color palette. Each scheme maps signal level to a different color gradient from low (dark) to high (bright).","kind":"choice","binding":{"phone":"DisplayWfColorScheme"},"applies":"live","requiresDescriptionVersion":12,"options":[{"value":0,"label":"Default"},{"value":1,"label":"Enhanced"},{"value":2,"label":"Spectran"},{"value":3,"label":"BlackWhite"},{"value":4,"label":"LinLog"},{"value":5,"label":"LinRad"},{"value":6,"label":"Custom"},{"value":7,"label":"Clarity Blue"}],"default":0},)json"
    R"json({"id":"display.waterfallDefaults.historyDepth","label":"Depth:","tooltip":"Maximum amount of waterfall history kept for rewind. Effective rewind is capped at 16384 rows; slow the update period to extend depth at fast refresh rates.","kind":"choice","binding":{"phone":"DisplayWaterfallHistoryMs"},"applies":"live","requiresDescriptionVersion":12,"options":[{"value":60000,"label":"60 seconds"},{"value":300000,"label":"5 minutes"},{"value":900000,"label":"15 minutes"},{"value":1200000,"label":"20 minutes"}],"default":1200000},)json"
    R"json({"id":"display.waterfallDefaults.timestampPosition","label":"Timestamp Position:","tooltip":"Position of the time stamp drawn on each waterfall row. None disables timestamps; Left and Right place them at the respective edge.","kind":"choice","binding":{"phone":"DisplayWfTimestampPos"},"applies":"live","requiresDescriptionVersion":12,"options":[{"value":0,"label":"None"},{"value":1,"label":"Left"},{"value":2,"label":"Right"}],"default":0},)json"
    R"json({"id":"display.waterfallDefaults.timestampMode","label":"Timestamp Mode:","tooltip":"Time zone used for waterfall timestamps. UTC uses Coordinated Universal Time; Local uses the system clock time zone.","kind":"choice","binding":{"phone":"DisplayWfTimestampMode"},"applies":"live","requiresDescriptionVersion":12,"options":[{"value":0,"label":"UTC"},{"value":1,"label":"Local"}],"default":0},)json"
    R"json({"id":"display.gridScales.showGrid","label":"Show grid","tooltip":"Display the Major Grid on the Panadapter including the frequency numbers","kind":"toggle","binding":{"phone":"DisplayGridEnabled"},"applies":"live","requiresDescriptionVersion":12,"default":true},)json"
    R"json({"id":"display.gridScales.dbmScale","label":"Show dBm scale strip (right edge)","tooltip":"Show the reference-level scale on the right edge of the spectrum. Disable to give the spectrum trace the full widget width.","kind":"toggle","binding":{"phone":"DisplayDbmScaleVisible"},"applies":"live","requiresDescriptionVersion":12,"default":true},)json"
    R"json({"id":"display.gridScales.dbMax","label":"dB Max (per band):","tooltip":"Signal level at the top of the display in dB, for the current band. Each band keeps its own value.","kind":"integer","binding":{"phone":"DisplayGridMax"},"applies":"live","requiresDescriptionVersion":12,"min":-200,"max":0,"step":1,"unit":"dB","perBand":{"label":"dB Max (%1):"},"default":-40},)json"
    R"json({"id":"display.gridScales.dbMin","label":"dB Min (per band):","tooltip":"Signal level at the bottom of the display in dB, for the current band. Each band keeps its own value.","kind":"integer","binding":{"phone":"DisplayGridMin"},"applies":"live","requiresDescriptionVersion":12,"min":-200,"max":0,"step":1,"unit":"dB","perBand":{"label":"dB Min (%1):"},"default":-140},)json"
    R"json({"id":"display.gridScales.dbStep","label":"dB Step (global):","tooltip":"Horizontal grid step size in dB. Sets the spacing between dB grid lines across all bands (global, not per-band).","kind":"integer","binding":{"phone":"DisplayGridStep"},"applies":"live","requiresDescriptionVersion":12,"min":1,"max":40,"step":1,"unit":"dB","default":10},)json"
    R"json({"id":"display.gridScales.freqLabelAlign","label":"Freq Label Align:","tooltip":"Sets the alignment of the frequency labels on the grid callouts on the display.","kind":"choice","binding":{"phone":"DisplayFreqLabelAlign"},"applies":"live","requiresDescriptionVersion":12,"options":[{"value":0,"label":"Left"},{"value":1,"label":"Center"},{"value":2,"label":"Right"},{"value":3,"label":"Auto"},{"value":4,"label":"Off"}],"default":1},)json"
    R"json({"id":"display.gridScales.zeroLine","label":"Show zero line","tooltip":"Show a horizontal line at 0 dBm on the panadapter grid.","kind":"toggle","binding":{"phone":"DisplayShowZeroLine"},"applies":"live","requiresDescriptionVersion":12,"default":false},)json"
    R"json({"id":"display.gridScales.showFps","label":"Show FPS overlay","tooltip":"Show FPS reading in top left of spectrum area","kind":"toggle","binding":{"phone":"DisplayShowFps"},"applies":"live","requiresDescriptionVersion":12,"default":false},)json"
    R"json({"id":"display.gridScales.adjustGridMinToNoiseFloor","label":"Adjust grid min to track noise floor","tooltip":"When enabled, the lower grid boundary automatically follows the live noise floor estimate. The grid min is set to NF + offset.","kind":"toggle","binding":{"phone":"DisplayAdjustGridMinToNoiseFloor"},"applies":"live","requiresDescriptionVersion":12,"default":false,"gate":{"capability":"displayExtrasVersion","min":4}},)json"
    R"json({"id":"display.gridScales.noiseFloorOffset","label":"NF offset:","tooltip":"Offset added to the noise floor estimate to compute the grid min. Use a negative value to place the grid min below the noise floor.","kind":"integer","binding":{"phone":"DisplayNFOffsetGridFollow"},"applies":"live","requiresDescriptionVersion":12,"min":-60,"max":60,"step":1,"unit":"dB","enabledWhen":{"phone":"DisplayAdjustGridMinToNoiseFloor","oneOf":[true]},"default":0,"gate":{"capability":"displayExtrasVersion","min":4}},)json"
    R"json({"id":"display.gridScales.maintainGridRange","label":"Maintain grid range (move max with min)","tooltip":"When enabled, the grid max is also moved so the dB range stays constant as the grid min tracks the noise floor.","kind":"toggle","binding":{"phone":"DisplayMaintainNFAdjustDelta"},"applies":"live","requiresDescriptionVersion":12,"enabledWhen":{"phone":"DisplayAdjustGridMinToNoiseFloor","oneOf":[true]},"default":false,"gate":{"capability":"displayExtrasVersion","min":4}},)json"
    R"json({"id":"display.gridScales.copyWaterfallThresholds","label":"Copy waterfall thresholds \u2192 spectrum min/max","tooltip":"Copies the current waterfall High Threshold and Low Threshold into the spectrum dB max and dB min for the current band.","kind":"button","binding":{"phone":"copyWaterfallThresholds"},"applies":"live","requiresDescriptionVersion":12},)json"
    R"json({"id":"display.multimeter.showDecimal","label":"Show decimal point in readouts","tooltip":"Display a decimal digit in S-meter and dBm text readouts (e.g. S5.3 or -85.6 dBm).","kind":"toggle","binding":{"phone":"MultimeterShowDecimal"},"applies":"live","requiresDescriptionVersion":12,"default":true},)json"
    R"json({"id":"display.multimeter.unitMode","label":"Display units:","tooltip":"Sets the unit used for signal level readouts across all meter items. S = IARU S-scale (S1\u2013S9+dB), dBm = -130 to 0, uV = microvolts at 50\u03a9.","kind":"choice","binding":{"phone":"MultimeterUnitMode"},"applies":"live","requiresDescriptionVersion":12,"options":[{"value":0,"label":"S"},{"value":1,"label":"dBm"},{"value":2,"label":"uV"}],"default":1},)json"
    R"json({"id":"display.multimeter.historyDuration","label":"History duration:","tooltip":"Total time span shown in the signal history graph (1\u2013600 000 ms).","kind":"integer","binding":{"phone":"MultimeterSignalHistoryDurationMs"},"applies":"live","requiresDescriptionVersion":12,"min":1000,"max":600000,"step":1,"unit":"ms","default":60000},)json"
    R"json({"id":"display.txDisplay.wfLowLevel","label":"Low Level:","tooltip":"Waterfall Low Signal. Show Low Color below this value, with gradient in between.","kind":"integer","binding":{"phone":"DisplayTxWfLowLevel"},"applies":"live","requiresDescriptionVersion":12,"min":-200,"max":200,"step":5,"unit":"dBm","default":-70,"gate":{"capability":"txDisplayVersion","min":1}},)json"
    R"json({"id":"display.txDisplay.wfHighLevel","label":"High Level:","tooltip":"Waterfall High Signal. Show High Color above this value, with gradient in between.","kind":"integer","binding":{"phone":"DisplayTxWfHighLevel"},"applies":"live","requiresDescriptionVersion":12,"min":-200,"max":200,"step":5,"unit":"dBm","default":30,"gate":{"capability":"txDisplayVersion","min":1}},)json"
    R"json({"id":"display.txDisplay.wfPalette","label":"Palette:","tooltip":"Sets the color scheme for the TX waterfall.","kind":"choice","binding":{"phone":"DisplayTxWfPalette"},"applies":"live","requiresDescriptionVersion":12,"options":[{"value":1,"label":"Enhanced"},{"value":2,"label":"Spectran"},{"value":3,"label":"BlackWhite"},{"value":4,"label":"LinLog"},{"value":5,"label":"LinRad"},{"value":0,"label":"LinAuto"},{"value":6,"label":"Custom"}],"default":1,"gate":{"capability":"txDisplayVersion","min":1}},)json"
    R"json({"id":"display.txDisplay.wfLowColor","label":"Low Color:","tooltip":"Color used when the signal level is at or below the Low Level set above.","kind":"colour","binding":{"phone":"DisplayTxWfLowColor"},"applies":"live","requiresDescriptionVersion":12,"default":"#000000FF","gate":{"capability":"txDisplayVersion","min":1}},)json"
    R"json({"id":"display.threeD.reset","label":"Reset 3D to defaults","tooltip":"Restore Spectrum render mode, 3D Floor, 3D Gain, 3D Span, 3D Angle, 3D Speed and 3D Slice Shadow to their ship defaults (2D Waterfall / 6 dB / 70% / 100% / 50% / Match / off).","kind":"button","binding":{"phone":"reset3d"},"applies":"live","requiresDescriptionVersion":12,"confirm":"This will restore Spectrum render mode, 3D Floor, 3D Gain, 3D Span, 3D Angle, 3D Speed and 3D Slice Shadow to their ship defaults.\n\nContinue?"},)json"
    R"json({"id":"display.threeD.renderMode","label":"Spectrum:","tooltip":"2D: FFT trace + waterfall.\n3D: perspective stacked-trace spectrum stream.","kind":"choice","binding":{"phone":"DisplaySpectrumRenderMode"},"applies":"subscription","requiresDescriptionVersion":12,"options":[{"value":0,"label":"2D Waterfall"},{"value":1,"label":"3D Stacked Trace"}],"default":0,"gate":{"capability":"remoteMediaVersion","min":1}},)json"
    R"json({"id":"display.threeD.floor","label":"3D Floor:","tooltip":"How far below the noise floor the surface starts. Each band keeps its own value.","kind":"slider","binding":{"phone":"Display3DFloorDepth"},"applies":"live","requiresDescriptionVersion":12,"min":0,"max":24,"step":1,"unit":"dB","perBand":{"label":"3D Floor:"},"default":6},)json"
    R"json({"id":"display.threeD.gain","label":"3D Gain:","tooltip":"3D surface color gain: how far down the signal range the colormap reaches.\nHigher = color down toward the noise floor; lower = color only on the strongest signals.","kind":"slider","binding":{"phone":"Display3DGain"},"applies":"live","requiresDescriptionVersion":12,"min":0,"max":100,"step":1,"unit":"%","default":70},)json"
    R"json({"id":"display.threeD.span","label":"3D Span:","tooltip":"3D surface width: how far the nearest traces overhang the plot edges, using spectrum the radio sends from outside the panadapter.\nHigher = the empty wedges beside the surface close from the front; 0 = the classic narrowing trapezoid.","kind":"slider","binding":{"phone":"Display3DSpan"},"applies":"live","requiresDescriptionVersion":12,"min":0,"max":100,"step":1,"unit":"%","default":100},)json"
    R"json({"id":"display.threeD.angle","label":"3D Angle:","tooltip":"Viewing angle for the 3D surface: low looks along the traces edge-on, high looks down on them.\n50 is the classic fixed angle.","kind":"slider","binding":{"phone":"Display3DAngle"},"applies":"live","requiresDescriptionVersion":12,"min":0,"max":100,"step":1,"unit":"%","default":50},)json"
    R"json({"id":"display.threeD.sliceShadow","label":"3D Slice Shadow","tooltip":"Darken each slice's passband onto the 3D surface so it leans back\nwith the perspective, instead of drawing flat on top of it.","kind":"toggle","binding":{"phone":"Display3DSliceShadow"},"applies":"live","requiresDescriptionVersion":12,"default":false})json"
    R"json(])json";
constexpr char kAppearanceV12ResetColours[] =
    R"json({"id":"appearance.colorsTheme.resetColors","label":"Reset all colors to defaults","tooltip":"Reset all spectrum and waterfall colors to factory defaults. Other display settings (FPS, averaging, thresholds, etc.) are not affected.","kind":"button","binding":{"phone":"resetColors"},"applies":"live","requiresDescriptionVersion":12,"confirm":"Reset all spectrum and waterfall colors to factory defaults?\n\nCustom colors set here will be discarded. Other display settings are not affected."})json";

// PA and Hardware Config version 13 (R-R3-49, R-IOS-18, R-IOS-27): the
// rest of PA Values, the Watt Meter page, and Hardware Config's Radio Info,
// TX Display Cal and N2ADR switch. Each row is closed as the version 12 rows
// are. Rows marked rangeSource or carrying an empty value/copyText are the
// resource's form; loadCategory fills them for the Core's radio.
// The ten watt-meter points carry no offAir: Thetis gives the table no
// transmit rule. Its boxes have no ValueChanged handler and the forward-power
// reading reads them live with no MOX check:
// From Thetis setup.cs:5437-5452 [v2.10.3.15] PA10W (ud100PA10W, ud10PA1W,
// ud200PA20W).
// The table corrects the reading only, and the Core takes a point on the
// air and applies it once the radio is back on receive
// (RadioModel::flushRemoteHardwareApply).
constexpr char kPaV13Controls[] =
    R"json([)json"
    R"json({"id":"pa.wattMeter.calPoint1","label":"Point 1","tooltip":"","kind":"decimal","binding":{"radioSetting":"paCalibration/calPoint1"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":6},"requiresDescriptionVersion":13,"rangeSource":"board.paCalibration","unit":"W"},)json"
    R"json({"id":"pa.wattMeter.calPoint2","label":"Point 2","tooltip":"","kind":"decimal","binding":{"radioSetting":"paCalibration/calPoint2"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":6},"requiresDescriptionVersion":13,"rangeSource":"board.paCalibration","unit":"W"},)json"
    R"json({"id":"pa.wattMeter.calPoint3","label":"Point 3","tooltip":"","kind":"decimal","binding":{"radioSetting":"paCalibration/calPoint3"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":6},"requiresDescriptionVersion":13,"rangeSource":"board.paCalibration","unit":"W"},)json"
    R"json({"id":"pa.wattMeter.calPoint4","label":"Point 4","tooltip":"","kind":"decimal","binding":{"radioSetting":"paCalibration/calPoint4"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":6},"requiresDescriptionVersion":13,"rangeSource":"board.paCalibration","unit":"W"},)json"
    R"json({"id":"pa.wattMeter.calPoint5","label":"Point 5","tooltip":"","kind":"decimal","binding":{"radioSetting":"paCalibration/calPoint5"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":6},"requiresDescriptionVersion":13,"rangeSource":"board.paCalibration","unit":"W"},)json"
    R"json({"id":"pa.wattMeter.calPoint6","label":"Point 6","tooltip":"","kind":"decimal","binding":{"radioSetting":"paCalibration/calPoint6"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":6},"requiresDescriptionVersion":13,"rangeSource":"board.paCalibration","unit":"W"},)json"
    R"json({"id":"pa.wattMeter.calPoint7","label":"Point 7","tooltip":"","kind":"decimal","binding":{"radioSetting":"paCalibration/calPoint7"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":6},"requiresDescriptionVersion":13,"rangeSource":"board.paCalibration","unit":"W"},)json"
    R"json({"id":"pa.wattMeter.calPoint8","label":"Point 8","tooltip":"","kind":"decimal","binding":{"radioSetting":"paCalibration/calPoint8"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":6},"requiresDescriptionVersion":13,"rangeSource":"board.paCalibration","unit":"W"},)json"
    R"json({"id":"pa.wattMeter.calPoint9","label":"Point 9","tooltip":"","kind":"decimal","binding":{"radioSetting":"paCalibration/calPoint9"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":6},"requiresDescriptionVersion":13,"rangeSource":"board.paCalibration","unit":"W"},)json"
    R"json({"id":"pa.wattMeter.calPoint10","label":"Point 10","tooltip":"","kind":"decimal","binding":{"radioSetting":"paCalibration/calPoint10"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":6},"requiresDescriptionVersion":13,"rangeSource":"board.paCalibration","unit":"W"},)json"
    R"json({"id":"pa.wattMeter.showPaValues","label":"Show PA Values page","tooltip":"Toggle visibility of the PA Values page in Setup navigation.","kind":"toggle","binding":{"phone":"display/showPaValuesPage"},"applies":"live","requiresDescriptionVersion":13,"default":true},)json"
    R"json({"id":"pa.wattMeter.resetPaValues","label":"Reset PA Values","tooltip":"Reset peak/min tracking on the PA Values page.","kind":"button","binding":{"phone":"resetPaValues"},"applies":"live","requiresDescriptionVersion":13},)json"
    R"json({"id":"pa.values.paTemperature","label":"PA Temperature:","tooltip":"","kind":"readout","binding":{"telemetry":{"object":"radio","name":"paTemperatureCelsius"}},"applies":"live","gate":{"capability":"stationTelemetryVersion","min":4},"requiresDescriptionVersion":13,"decimals":1,"unit":"\u00b0C","temperatureUnit":"PaTempUnit"},)json"
    R"json({"id":"pa.values.adcOverload","label":"ADC Overload:","tooltip":"","kind":"readout","binding":{"adcOverload":{"object":"stepAtt"}},"applies":"live","gate":{"capability":"radioHardwareVersion","min":1},"requiresDescriptionVersion":13},)json"
    R"json({"id":"pa.values.resetPeakMin","label":"Reset Peak/Min","tooltip":"Reset running peak/min trackers to current values.","kind":"button","binding":{"phone":"resetPaValues"},"applies":"live","requiresDescriptionVersion":13})json"
    R"json(])json";
constexpr char kHardwareV13Controls[] =
    R"json([)json"
    R"json({"id":"hardware.radioInfo.board","label":"Board:","tooltip":"","kind":"readout","binding":{"radioInfo":"board"},"applies":"live","requiresDescriptionVersion":13,"value":""},)json"
    R"json({"id":"hardware.radioInfo.protocol","label":"Protocol:","tooltip":"","kind":"readout","binding":{"radioInfo":"protocol"},"applies":"live","requiresDescriptionVersion":13,"value":""},)json"
    R"json({"id":"hardware.radioInfo.adcCount","label":"ADC count:","tooltip":"","kind":"readout","binding":{"radioInfo":"adcCount"},"applies":"live","requiresDescriptionVersion":13,"value":""},)json"
    R"json({"id":"hardware.radioInfo.maxRx","label":"Max RX:","tooltip":"","kind":"readout","binding":{"radioInfo":"maxRx"},"applies":"live","requiresDescriptionVersion":13,"value":""},)json"
    R"json({"id":"hardware.radioInfo.firmware","label":"Firmware:","tooltip":"","kind":"readout","binding":{"radioInfo":"firmware"},"applies":"live","requiresDescriptionVersion":13,"value":""},)json"
    R"json({"id":"hardware.radioInfo.mac","label":"MAC:","tooltip":"","kind":"readout","binding":{"radioInfo":"mac"},"applies":"live","requiresDescriptionVersion":13,"value":""},)json"
    R"json({"id":"hardware.radioInfo.ip","label":"IP address:","tooltip":"","kind":"readout","binding":{"radioInfo":"ip"},"applies":"live","requiresDescriptionVersion":13,"value":""},)json"
    R"json({"id":"hardware.radioInfo.sampleRate","label":"Sample rate (Hz):","tooltip":"","kind":"choice","binding":{"command":{"verb":"setRadioSampleRate","valueProperty":{"object":"slice:active","name":"sampleRateHz"},"arguments":{"rateHz":{"$controlValue":true}}}},"applies":"live","gate":{"capability":"radioHardwareVersion","min":9,"offAir":true},"requiresDescriptionVersion":13,"options":[]},)json"
    R"json({"id":"hardware.radioInfo.copySupportInfo","label":"Copy Support Info to Clipboard","tooltip":"Copies board identity and firmware version to the clipboard for bug reports.","kind":"button","binding":{"phone":"copySupportInfo"},"applies":"live","requiresDescriptionVersion":13,"copyText":""},)json"
    R"json({"id":"hardware.calibration.txDisplayOffset","label":"Offset:","tooltip":"TX display calibration offset in dB. Applied to TX spectrum display.","kind":"decimal","binding":{"radioSetting":"cal/txDisplayOffset"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":8},"requiresDescriptionVersion":13,"min":-100,"max":100,"step":0.1,"decimals":1,"unit":"dB","default":0},)json"
    R"json({"id":"hardware.hl2Io.n2adrFilter","label":"Enable N2ADR Filter board","tooltip":"","kind":"toggle","binding":{"radioSetting":"hl2IoBoard/n2adrFilter"},"valueEncoding":{"true":"True","false":"False"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":8},"requiresDescriptionVersion":13,"default":true})json"
    R"json(])json";

// Hardware version 16: HL2 I/O's Hermes Lite Options, the rows the
// desktop's HL2 Options tab shows, in its order, bound to the same per-radio
// keys (Hl2OptionsModel, hardware/<mac>/hl2/...). Ranges and defaults are
// Hl2OptionsModel's (mi0bot setup.designer.cs). The Core sends the radio TX
// latency, PTT hang, reset on disconnect, power supply sync and Band Volts;
// the three clock rows are stored only here, so they carry availability
// disabled with the desktop's own reason (version 18 opens them). Swap audio
// channels is open at every version from 16: the radio codec lane
// (2026-09-30) sends the HL2 its receive audio and honors the option, and
// the row's shape did not change, so no peer needs a new version to use it.
// Tooltip from mi0bot setup.designer.cs:11119 [@c26a8a4]. Closed as the
// version 13 rows are.
constexpr char kHardwareV16Controls[] =
    R"json([)json"
    R"json({"id":"hardware.hl2Io.txLatency","label":"TX buffer latency:","tooltip":"","kind":"integer","binding":{"radioSetting":"hl2/txLatencyMs"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":8,"transmit":true},"requiresDescriptionVersion":16,"min":0,"max":70,"step":1,"unit":"ms","default":20},)json"
    R"json({"id":"hardware.hl2Io.pttHang","label":"PTT hang:","tooltip":"","kind":"integer","binding":{"radioSetting":"hl2/pttHangMs"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":8,"transmit":true},"requiresDescriptionVersion":16,"min":0,"max":30,"step":1,"unit":"ms","default":12},)json"
    R"json({"id":"hardware.hl2Io.cl2Enable","label":"Enable CL2","tooltip":"","kind":"toggle","binding":{"radioSetting":"hl2/cl2Enable"},"valueEncoding":{"true":"True","false":"False"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":8},"requiresDescriptionVersion":16,"default":false,"availability":{"enabled":false,"reason":"NereusSDR does not change the radio's clock settings."}},)json"
    R"json({"id":"hardware.hl2Io.cl2Freq","label":"CL2 frequency","tooltip":"","kind":"integer","binding":{"radioSetting":"hl2/cl2FreqMHz"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":8},"requiresDescriptionVersion":16,"min":1,"max":200,"step":1,"unit":"MHz","default":116,"availability":{"enabled":false,"reason":"NereusSDR does not change the radio's clock settings."}},)json"
    R"json({"id":"hardware.hl2Io.ext10MHz","label":"External 10 MHz reference","tooltip":"","kind":"toggle","binding":{"radioSetting":"hl2/ext10MHz"},"valueEncoding":{"true":"True","false":"False"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":8},"requiresDescriptionVersion":16,"default":false,"availability":{"enabled":false,"reason":"NereusSDR does not change the radio's clock settings."}},)json"
    R"json({"id":"hardware.hl2Io.disconnectReset","label":"Reset on Ethernet disconnect","tooltip":"","kind":"toggle","binding":{"radioSetting":"hl2/disconnectReset"},"valueEncoding":{"true":"True","false":"False"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":8},"requiresDescriptionVersion":16,"default":false},)json"
    R"json({"id":"hardware.hl2Io.psSync","label":"Disable power supply sync","tooltip":"Stops the radio synchronizing its power supply clock.","kind":"toggle","binding":{"radioSetting":"hl2/psSync"},"valueEncoding":{"true":"True","false":"False"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":8},"requiresDescriptionVersion":16,"default":false},)json"
    R"json({"id":"hardware.hl2Io.bandVolts","label":"Band Volts (PWM out 0\u20133.3 V)","tooltip":"","kind":"toggle","binding":{"radioSetting":"hl2/bandVolts"},"valueEncoding":{"true":"True","false":"False"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":8},"requiresDescriptionVersion":16,"default":false},)json"
    R"json({"id":"hardware.hl2Io.swapAudioChannels","label":"Swap audio channels","tooltip":"Swap the audio channels sent to the HL2","kind":"toggle","binding":{"radioSetting":"hl2/swapAudioChannels"},"valueEncoding":{"true":"True","false":"False"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":8},"requiresDescriptionVersion":16,"default":false})json"
    R"json(])json";

// Hardware version 18: HL2 Options' Enable CL2, CL2 frequency and External
// 10 MHz, which the Core now sends to its radio (radioHardwareVersion 11).
// Tooltips are mi0bot's (setup.designer.cs:11158, 11174, 11187 [@c26a8a4]);
// the frequency row is enabled only while Enable CL2 is on, as mi0bot's
// ControlCl2 sets udCl2Freq.Enabled. The frequency is decimal to three
// places with a 0.1 step, as udCl2Freq (setup.designer.cs:11133-11163
// [@c26a8a4]). A peer below version 18 gets the version 16 rows, closed with
// the old reason, so it never writes a value its integer row could not hold.
// Closed as the version 13 rows are.
constexpr char kHardwareV18Controls[] =
    R"json([)json"
    R"json({"id":"hardware.hl2Io.cl2Enable","label":"Enable CL2","tooltip":"Enable frequency output on CL2","kind":"toggle","binding":{"radioSetting":"hl2/cl2Enable"},"valueEncoding":{"true":"True","false":"False"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":8},"requiresDescriptionVersion":18,"default":false},)json"
    R"json({"id":"hardware.hl2Io.cl2Freq","label":"CL2 frequency","tooltip":"Output frequency on CL2 output","kind":"decimal","binding":{"radioSetting":"hl2/cl2FreqMHz"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":8},"requiresDescriptionVersion":18,"min":1,"max":200,"step":0.1,"decimals":3,"unit":"MHz","enabledWhen":{"radioSetting":"hl2/cl2Enable","oneOf":[true]},"default":116},)json"
    R"json({"id":"hardware.hl2Io.ext10MHz","label":"External 10 MHz reference","tooltip":"Enable external 10 MHz input on CL1","kind":"toggle","binding":{"radioSetting":"hl2/ext10MHz"},"valueEncoding":{"true":"True","false":"False"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":8},"requiresDescriptionVersion":18,"default":false})json"
    R"json(])json";

// Hardware version 23: Calibration's Level Cal "Rx1 6m LNA" row, the
// desktop Calibration tab's spin box (CalibrationTab m_rx1LnaSpin), bound to
// the same per-radio key the Core already takes and applies to its receive
// calibration ("cal" reload, RadioModel::refreshRxMeterOffset). Range, step,
// places and default are the tab's:
// From Thetis setup.designer.cs:12089-12116 [v2.10.3.15] ud6mLNAGainOffset:
// 0..25 dB, step 1, one decimal, 13 dB.
// The Core refuses a value outside 0..25 (calibrationKeyValueRefusal).
// Closed as the version 13 rows are.
constexpr char kHardwareV23Controls[] =
    R"json([)json"
    R"json({"id":"hardware.calibration.rx1_6mLna","label":"Rx1 6m LNA:","tooltip":"","kind":"decimal","binding":{"radioSetting":"cal/rx1_6mLna"},"applies":"live","gate":{"capability":"radioHardwareVersion","min":1},"requiresDescriptionVersion":23,"min":0,"max":25,"step":1,"decimals":1,"unit":"dB","default":13})json"
    R"json(])json";

// Audio version 24 (radio codec lane): TX Input's Line In Gain moves in the
// 1.5 dB steps of Thetis's udLineInBoost with one decimal
// (From Thetis setup.designer.cs:47006-47034 [v2.10.3.15]: Increment 1.5,
// Minimum -34.5, Maximum 12, DecimalPlaces 1), the values the Core's
// TransmitModel::lineInBoost takes; and the Saturn G2 group's Mic Tip-Ring,
// the ORION panel's Tip / Ring that Thetis enables on the G2 and G2-1K
// (setup.cs:20292, 20343 [v2.10.3.15]), bound to transmit's micTipRing as
// the Orion group's row is. Closed as the version 23 row is.
constexpr char kAudioV24Controls[] =
    R"json([)json"
    R"json({"id":"audio.txInput.hermesLineInGain","label":"Line In Gain:","tooltip":"","kind":"decimal","binding":{"property":{"object":"transmit","name":"lineInBoost"}},"applies":"live","requiresDescriptionVersion":24,"gate":{"capability":"transmitSettingsVersion","min":3,"transmit":true},"min":-34.5,"max":12,"step":1.5,"decimals":1,"unit":"dB"},)json"
    R"json({"id":"audio.txInput.saturnMicTipRing","label":"Mic Tip-Ring (Tip is Mic)","tooltip":"","kind":"toggle","binding":{"property":{"object":"transmit","name":"micTipRing"}},"applies":"live","requiresDescriptionVersion":24,"gate":{"capability":"transmitSettingsVersion","min":3,"transmit":true}})json"
    R"json(])json";

// Transmit version 13 (R-R3-49): Power's PA Control group, "Disable HF PA",
// which the Core applies on and off the air (transmitSettingsVersion 11).
constexpr char kTransmitV13Controls[] =
    R"json([)json"
    R"json({"id":"transmit.power.DisableHfPa","label":"Disable HF PA","tooltip":"Disables HF PA.","kind":"toggle","binding":{"setting":"DisableHfPa"},"valueEncoding":{"true":"True","false":"False"},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":11,"transmit":true},"requiresDescriptionVersion":13,"default":false})json"
    R"json(])json";

// The page titles and row labels the desktop's Alex-1 Filters and Alex-2
// Filters tabs show, in the order of alexKeys::kPreselectorSlugs.
constexpr std::array<const char*, 6> kAlexHpfRowLabels = {
    "1.5 MHz HPF", "6.5 MHz HPF", "9.5 MHz HPF", "13 MHz HPF", "20 MHz HPF", "6m Bypass"};
constexpr std::array<const char*, 6> kAlexBpf1RowLabels = {
    "160m BPF", "80/60m BPF", "40/30m BPF", "20/17/15m BPF", "12/10m BPF", "6m BPF/LNA"};

QHash<QString, QJsonObject> controlsById(const char* json);

// Hardware version 13 (R-R3-46, R-R3-49): the Alex receive filter rows the
// Core applies (radioHardwareVersion 8), after the Alex-1 tab's five
// switches above them: each row's Bypass, Start and End on
// the Alex-1 high-pass and BPF1 banks and the Alex-2 bank, and the Alex-2
// master bypass. Defaults are Thetis's spinner values
// (codec::alex::AlexHpfEdges::thetisDefaults); the edge boxes are the
// desktop tab's (0 to 200 MHz, step 0.001, six decimals).
QString alexFilterRowsJson()
{
    const codec::alex::AlexHpfEdges defaults = codec::alex::AlexHpfEdges::thetisDefaults();
    const auto number = [](double value) { return QString::number(value, 'g', 10); };
    QStringList rows;
    const auto bank = [&rows, &number](const QString& page, const QString& bankId,
                                        const QString& prefix,
                                        const std::array<const char*, 6>& labels,
                                        const codec::alex::AlexHpfRows& edges) {
        for (size_t i = 0; i < edges.size(); ++i) {
            const QString slug = QString::fromLatin1(alexKeys::kPreselectorSlugs[i]);
            const QString base = QStringLiteral("%1.%2.%3").arg(page, bankId, slug);
            const QString key = QStringLiteral("%1/%2").arg(prefix, slug);
            const QString label = QString::fromLatin1(labels[i]);
            rows << QString::fromLatin1(
                R"j({"id":"%1.bypass","label":"%2 Bypass","tooltip":"","kind":"toggle","binding":{"radioSetting":"%3/enabled"},"valueEncoding":{"true":"True","false":"False"},"applies":"live","gate":{"capability":"radioHardwareVersion","min":8},"requiresDescriptionVersion":13,"default":false})j")
                .arg(base, label, key);
            const std::array<std::pair<const char*, double>, 2> leaves{{
                {"start", edges[i].startMhz}, {"end", edges[i].endMhz}}};
            for (const auto& [leaf, value] : leaves) {
                const QString leafName = QString::fromLatin1(leaf);
                const QString word = leafName == QLatin1String("start")
                    ? QStringLiteral("Start") : QStringLiteral("End");
                rows << QString::fromLatin1(
                    R"j({"id":"%1.%2","label":"%3 %4","tooltip":"","kind":"decimal","binding":{"radioSetting":"%5/%2"},"applies":"live","gate":{"capability":"radioHardwareVersion","min":8},"requiresDescriptionVersion":13,"min":0,"max":200,"step":0.001,"decimals":6,"unit":"MHz","default":%6})j")
                    .arg(base, leafName, label, word, key, number(value));
            }
        }
    };
    // The Alex-1 tab's five switches above its rows, in its order, which the
    // Core applies on a change (RadioModel::applyAlexHpfSwitchSettings, by
    // radioHardwareVersion 8). Defaults are the desktop's, from Thetis's
    // designer (chkDisableHPFonPSb and chkDisable6mLNAonTX checked). The
    // three the Core takes as transmit hardware (isTransmitHardwareKey) need
    // transmit permission, as a settings write of them does.
    struct MasterSwitch { const char* field; const char* label; bool transmit; bool on; };
    const std::array<MasterSwitch, 5> switches{{
        {"hpfBypass", "HPF Bypass (master)", false, false},
        {"hpfBypassOnTx", "HPF Bypass on TX", true, false},
        {"hpfBypassOnPs", "HPF Bypass on PureSignal feedback", true, true},
        {"disable6mLnaOnTx", "Disable 6m LNA on TX", true, true},
        {"disable6mLnaOnRx", "Disable 6m LNA on RX", false, false}}};
    // From Thetis setup.cs:29440-29449 [v2.10.3.15] (chkDisableHPFonPS_CheckedChanged):
    // the desktop asks before HPF Bypass on PureSignal feedback is cleared
    // (AntennaAlexAlex1Tab::imdWarningText, which the parity test holds this
    // text to). A peer asks with the same words before setting the toggle to
    // confirmWhen, and leaves it as it was on No.
    static constexpr const char* kImdWarningText =
        "Including the BPFs during a PureSignal transmission may "
        "produce passive Inter-Modulation Distortion in the "
        "inductors of the bandpass filters.\n\n"
        "You will NOT be able to observe this degraded performance "
        "on the panadapter because PS is correcting to the distorted "
        "feedback and the panadapter is \"seeing\" that same "
        "distorted feedback. It can only be observed with an "
        "external spectrum analyzer.\n\n"
        "Please ensure you understand the implications of including "
        "the BPFs when transmitting a PureSignal based signal. "
        "It is not recommended.";
    for (const MasterSwitch& master : switches) {
        if (qstrcmp(master.field, "hpfBypassOnPs") == 0) {
            QJsonObject row = QJsonDocument::fromJson(QString::fromLatin1(
                R"j({"id":"hardware.alex1Filters.%1","label":"%2","tooltip":"","kind":"toggle","binding":{"radioSetting":"alex/master/%1"},"valueEncoding":{"true":"True","false":"False"},"applies":"live","gate":{"capability":"radioHardwareVersion","min":8%3},"requiresDescriptionVersion":13,"default":%4})j")
                .arg(QString::fromLatin1(master.field), QString::fromLatin1(master.label),
                     master.transmit ? QStringLiteral(",\"transmit\":true") : QString(),
                     master.on ? QStringLiteral("true") : QStringLiteral("false"))
                .toUtf8()).object();
            row.insert(QStringLiteral("confirm"), QString::fromLatin1(kImdWarningText));
            row.insert(QStringLiteral("confirmWhen"), false);
            rows << QString::fromUtf8(QJsonDocument(row).toJson(QJsonDocument::Compact));
            continue;
        }
        rows << QString::fromLatin1(
            R"j({"id":"hardware.alex1Filters.%1","label":"%2","tooltip":"","kind":"toggle","binding":{"radioSetting":"alex/master/%1"},"valueEncoding":{"true":"True","false":"False"},"applies":"live","gate":{"capability":"radioHardwareVersion","min":8%3},"requiresDescriptionVersion":13,"default":%4})j")
            .arg(QString::fromLatin1(master.field), QString::fromLatin1(master.label),
                 master.transmit ? QStringLiteral(",\"transmit\":true") : QString(),
                 master.on ? QStringLiteral("true") : QStringLiteral("false"));
    }
    bank(QStringLiteral("hardware.alex1Filters"), QStringLiteral("hpf"),
         QString::fromLatin1(alexKeys::kAlex1HpfPrefix), kAlexHpfRowLabels, defaults.hpf);
    bank(QStringLiteral("hardware.alex1Filters"), QStringLiteral("bpf1"),
         QString::fromLatin1(alexKeys::kAlex1Bpf1Prefix), kAlexBpf1RowLabels, defaults.bpf1);
    rows << QString::fromLatin1(
        R"j({"id":"hardware.alex2Filters.bypass55MhzBpf","label":"ByPass / 55 MHz BPF (master)","tooltip":"","kind":"toggle","binding":{"radioSetting":"alex2/master/bypass55MhzBpf"},"valueEncoding":{"true":"True","false":"False"},"applies":"live","gate":{"capability":"radioHardwareVersion","min":8},"requiresDescriptionVersion":13,"default":false})j");
    bank(QStringLiteral("hardware.alex2Filters"), QStringLiteral("hpf"),
         QStringLiteral("alex2/hpf"), kAlexHpfRowLabels, defaults.alex2);
    return QLatin1Char('[') + rows.join(QLatin1Char(',')) + QLatin1Char(']');
}

// Hardware version 17 (R-R3-46, R-R3-49): the Alex-1 tab's low-pass rows
// and 6m/ByPass on RX, which the Core applies as Thetis's setAlexLPF
// selects (radioHardwareVersion 10). The rows are the transmit low-pass
// table (isTransmitHardwareKey), so a write needs transmit permission; the
// Core takes them on the air and keeps them for the next selection, as
// Thetis's spinner handlers do (setup.cs:15888-15994 [v2.10.3.15]). The
// bypass acts on receive only. Defaults are Thetis's spinner values
// (codec::alex::AlexLpfEdges::thetisDefaults); each edge's min / max is its
// spinner's Minimum / Maximum (codec::alex::kAlexLpfEdgeLimits,
// setup.designer.cs [v2.10.3.15]), which the Core also enforces on a write
// (StationServer::alexLpfKeyValueRefusal); the labels and edge boxes are the
// desktop tab's (AntennaAlexAlex1Tab).
QString alexLpfRowsJson()
{
    const codec::alex::AlexLpfEdges defaults = codec::alex::AlexLpfEdges::thetisDefaults();
    const auto number = [](double value) { return QString::number(value, 'g', 10); };
    static constexpr std::array<const char*, codec::alex::kAlexLpfRowCount> kLabels = {
        "160m", "80m", "60/40m", "30/20m", "17/15m", "12/10m", "6m"};
    QStringList rows;
    for (int i = 0; i < codec::alex::kAlexLpfRowCount; ++i) {
        const QString slug = QString::fromLatin1(codec::alex::kAlexLpfRowSlugs[i]);
        const QString label = QString::fromLatin1(kLabels[i]);
        const codec::alex::AlexLpfEdgeLimits& lim = codec::alex::kAlexLpfEdgeLimits[i];
        struct Leaf {
            const char* name;
            double value;
            double min;
            double max;
        };
        const std::array<Leaf, 2> leaves{{
            {"start", defaults.rows[i].startMhz, lim.startMin, lim.startMax},
            {"end", defaults.rows[i].endMhz, lim.endMin, lim.endMax}}};
        for (const Leaf& leafRow : leaves) {
            const char* leaf = leafRow.name;
            const double value = leafRow.value;
            const QString leafName = QString::fromLatin1(leaf);
            const QString word = leafName == QLatin1String("start")
                ? QStringLiteral("Start") : QStringLiteral("End");
            rows << QString::fromLatin1(
                R"j({"id":"hardware.alex1Filters.lpf.%1.%2","label":"%3 LPF %4","tooltip":"","kind":"decimal","binding":{"radioSetting":"alex/lpf/%1/%2"},"applies":"live","gate":{"capability":"radioHardwareVersion","min":10,"transmit":true},"requiresDescriptionVersion":17,"min":%6,"max":%7,"step":0.001,"decimals":6,"unit":"MHz","default":%5})j")
                .arg(slug, leafName, label, word, number(value),
                     number(leafRow.min), number(leafRow.max));
        }
    }
    // From Thetis setup.designer.cs:23484-23495 [v2.10.3.15] (chkLPFBypass):
    // the desktop checkbox's text and tooltip ("reguardless" spelled right).
    rows << QString::fromLatin1(
        R"j({"id":"hardware.alex1Filters.lpfBypass","label":"6m/ByPass on RX","tooltip":"Selects the 6m LPF during receive regardless of frequency.","kind":"toggle","binding":{"radioSetting":"alex/master/lpfBypass"},"valueEncoding":{"true":"True","false":"False"},"applies":"live","gate":{"capability":"radioHardwareVersion","min":10},"requiresDescriptionVersion":17,"default":false})j");
    return QLatin1Char('[') + rows.join(QLatin1Char(',')) + QLatin1Char(']');
}

QHash<QString, QJsonObject> controlsById(const char* json)
{
    QHash<QString, QJsonObject> controls;
    const QJsonArray rows = QJsonDocument::fromJson(QByteArray(json)).array();
    for (const QJsonValue& row : rows) {
        const QJsonObject control = row.toObject();
        controls.insert(control.value(QStringLiteral("id")).toString(), control);
    }
    return controls;
}

// PA version 14 (R-R3-49, R-IOS-18): PA Gain's profile choice, its four
// buttons and the per-band table, bound to the Core's `paProfiles` object
// and the paProfile verbs (paProfileVersion 1). Closed as the version 12
// rows are.
constexpr char kPaV14Controls[] =
    R"json([)json"
    R"json({"id":"pa.gain.profile","label":"PA profile","tooltip":"Active PA gain profile. Its per-band PA gain keeps the drive right for this radio's power amplifier, so a high-gain amplifier is not overdriven.","kind":"choice","binding":{"paProfile":"active"},"applies":"live","gate":{"capability":"paProfileVersion","min":1,"offAir":true},"requiresDescriptionVersion":14},)json"
    R"json({"id":"pa.gain.new","label":"New","tooltip":"Create a new empty profile seeded from the connected radio's factory PA gain row.","kind":"button","binding":{"paProfile":"new"},"applies":"live","gate":{"capability":"paProfileVersion","min":1,"offAir":true},"requiresDescriptionVersion":14,"prompt":{"title":"New PA Profile","label":"Profile name:","default":""}},)json"
    R"json({"id":"pa.gain.copy","label":"Copy","tooltip":"Duplicate the active profile under a new name.","kind":"button","binding":{"paProfile":"copy"},"applies":"live","gate":{"capability":"paProfileVersion","min":1,"offAir":true},"requiresDescriptionVersion":14,"prompt":{"title":"Copy PA Profile","label":"New profile name:","default":"%1 (copy)"}},)json"
    R"json({"id":"pa.gain.delete","label":"Delete","tooltip":"Delete the active profile.  The last remaining profile cannot be deleted.","kind":"button","binding":{"paProfile":"delete"},"applies":"live","gate":{"capability":"paProfileVersion","min":1,"offAir":true},"requiresDescriptionVersion":14,"confirm":"Delete profile \"%1\"?"},)json"
    R"json({"id":"pa.gain.reset","label":"Reset Defaults","tooltip":"Re-seed the active profile from the canonical factory PA gain row for its model.  Drive-step adjusts and max-power columns are cleared.","kind":"button","binding":{"paProfile":"reset"},"applies":"live","gate":{"capability":"paProfileVersion","min":1,"offAir":true},"requiresDescriptionVersion":14,"confirm":"Reset the active profile to factory defaults?"},)json"
    R"json({"id":"pa.gain.table","label":"PA Gain by Band (dB)","tooltip":"","kind":"table","binding":{"paProfileGrid":{"object":"paProfiles"}},"applies":"live","gate":{"capability":"paProfileVersion","min":1,"offAir":true},"requiresDescriptionVersion":14,"rows":[{"band":0,"label":"160m"},{"band":1,"label":"80m"},{"band":2,"label":"60m"},{"band":3,"label":"40m"},{"band":4,"label":"30m"},{"band":5,"label":"20m"},{"band":6,"label":"17m"},{"band":7,"label":"15m"},{"band":8,"label":"12m"},{"band":9,"label":"10m"},{"band":10,"label":"6m"},{"band":11,"label":"GEN"},{"band":12,"label":"WWV"},{"band":13,"label":"XVTR"}],"columns":[{"id":"gain","label":"Gain (dB)","field":"gain","kind":"decimal","min":38.8,"max":100,"step":0.1,"decimals":1,"tooltip":"PA gain for %1 in dB. The Core subtracts it from the power you ask for to set the drive."},{"id":"adjust1","label":"10%","field":"adjust","driveStep":0,"kind":"decimal","min":-10,"max":10,"step":0.1,"decimals":1,"tooltip":"Per-step adjust at 10% drive for %1."},{"id":"adjust2","label":"20%","field":"adjust","driveStep":1,"kind":"decimal","min":-10,"max":10,"step":0.1,"decimals":1,"tooltip":"Per-step adjust at 20% drive for %1."},{"id":"adjust3","label":"30%","field":"adjust","driveStep":2,"kind":"decimal","min":-10,"max":10,"step":0.1,"decimals":1,"tooltip":"Per-step adjust at 30% drive for %1."},{"id":"adjust4","label":"40%","field":"adjust","driveStep":3,"kind":"decimal","min":-10,"max":10,"step":0.1,"decimals":1,"tooltip":"Per-step adjust at 40% drive for %1."},{"id":"adjust5","label":"50%","field":"adjust","driveStep":4,"kind":"decimal","min":-10,"max":10,"step":0.1,"decimals":1,"tooltip":"Per-step adjust at 50% drive for %1."},{"id":"adjust6","label":"60%","field":"adjust","driveStep":5,"kind":"decimal","min":-10,"max":10,"step":0.1,"decimals":1,"tooltip":"Per-step adjust at 60% drive for %1."},{"id":"adjust7","label":"70%","field":"adjust","driveStep":6,"kind":"decimal","min":-10,"max":10,"step":0.1,"decimals":1,"tooltip":"Per-step adjust at 70% drive for %1."},{"id":"adjust8","label":"80%","field":"adjust","driveStep":7,"kind":"decimal","min":-10,"max":10,"step":0.1,"decimals":1,"tooltip":"Per-step adjust at 80% drive for %1."},{"id":"adjust9","label":"90%","field":"adjust","driveStep":8,"kind":"decimal","min":-10,"max":10,"step":0.1,"decimals":1,"tooltip":"Per-step adjust at 90% drive for %1."},{"id":"maxPower","label":"Max W","field":"maxPower","kind":"decimal","min":0,"max":1500,"step":0.1,"decimals":1,"tooltip":"Per-band max-power ceiling in watts for %1."},{"id":"useMax","label":"Use Max","field":"useMax","kind":"toggle","tooltip":"Apply the per-band max-power ceiling on %1."}]})json"
    R"json(])json";

// DSP version 19 (R-R3-49): CFC's band editor, bound to transmit's
// cfcProfile (transmitSettingsVersion 15) and applied with cfc.setProfile.
// The ranges are CfcProfile's (CfcProfile.h: each from Thetis
// frmCFCConfig.Designer.cs [v2.10.3.15]); steps and decimals are the
// dialog's spin boxes (nudCFC_f step 1; nudCFC_c and nudCFC_gain 0.1, one
// decimal; nudCFC_cq and nudCFC_q 0.01, two decimals). Closed as the
// version 16 rows are. No off-air rule: Thetis's frmCFCConfig applies a
// change on the air (frmCFCConfig.cs:333-392 [v2.10.3.15] has no MOX check),
// and this Core takes it on the air (transmitSettingsVersion 13).
constexpr char kDspV19Controls[] =
    R"json([)json"
    R"json({"id":"dsp.cfc.bands","label":"Configure CFC bands\u2026","tooltip":"Open the per-band CFC editor: 5, 10 or 18 bands of compression and post-EQ.","kind":"table","binding":{"cfcProfile":{"object":"transmit","name":"cfcProfile","command":"cfc.setProfile"}},"applies":"live","gate":{"capability":"transmitSettingsVersion","min":15},"requiresDescriptionVersion":19,"bandCounts":[5,10,18],"minSpanHz":1000,"fields":[{"id":"minHz","label":"Low","kind":"decimal","min":0,"max":20000,"step":1,"decimals":0,"unit":" Hz"},{"id":"maxHz","label":"High","kind":"decimal","min":0,"max":20000,"step":1,"decimals":0,"unit":" Hz"},{"id":"parametric","label":"Use Q Factors","kind":"toggle"},{"id":"precompDb","label":"Pre-Comp","kind":"decimal","min":0,"max":16,"step":0.1,"decimals":1,"unit":" dB"},{"id":"postEqGainDb","label":"Post-EQ","kind":"decimal","min":-24,"max":24,"step":0.1,"decimals":1,"unit":" dB"}],"columns":[{"id":"frequencyHz","label":"Freq","kind":"decimal","min":0,"max":20000,"step":1,"decimals":0,"unit":" Hz"},{"id":"compressionDb","label":"Comp","kind":"decimal","min":0,"max":16,"step":0.1,"decimals":1,"unit":" dB"},{"id":"compressionQ","label":"Comp Q","kind":"decimal","min":0.2,"max":20,"step":0.01,"decimals":2},{"id":"postEqGainDb","label":"Gain","kind":"decimal","min":-24,"max":24,"step":0.1,"decimals":1,"unit":" dB"},{"id":"postEqQ","label":"EQ Q","kind":"decimal","min":0.2,"max":20,"step":0.01,"decimals":2}]})json"
    R"json(])json";

const QHash<QString, QJsonObject>& dspV19Controls()
{
    static const QHash<QString, QJsonObject> table = controlsById(kDspV19Controls);
    return table;
}

const QHash<QString, QJsonObject>& paV14Controls()
{
    static const QHash<QString, QJsonObject> table = controlsById(kPaV14Controls);
    return table;
}

const QHash<QString, QJsonObject>& paV13Controls()
{
    static const QHash<QString, QJsonObject> table = controlsById(kPaV13Controls);
    return table;
}

const QHash<QString, QJsonObject>& hardwareV13Controls()
{
    static const QHash<QString, QJsonObject> table = [] {
        QHash<QString, QJsonObject> controls = controlsById(kHardwareV13Controls);
        const QByteArray alex = alexFilterRowsJson().toUtf8();
        controls.insert(controlsById(alex.constData()));
        const QByteArray lpf = alexLpfRowsJson().toUtf8();
        controls.insert(controlsById(lpf.constData()));
        return controls;
    }();
    return table;
}

const QHash<QString, QJsonObject>& hardwareV16Controls()
{
    static const QHash<QString, QJsonObject> table = controlsById(kHardwareV16Controls);
    return table;
}

const QHash<QString, QJsonObject>& hardwareV18Controls()
{
    static const QHash<QString, QJsonObject> table = controlsById(kHardwareV18Controls);
    return table;
}

const QHash<QString, QJsonObject>& hardwareV23Controls()
{
    static const QHash<QString, QJsonObject> table = controlsById(kHardwareV23Controls);
    return table;
}

const QHash<QString, QJsonObject>& audioV24Controls()
{
    static const QHash<QString, QJsonObject> table = controlsById(kAudioV24Controls);
    return table;
}

const QHash<QString, QJsonObject>& transmitV13Controls()
{
    static const QHash<QString, QJsonObject> table = controlsById(kTransmitV13Controls);
    return table;
}

const QHash<QString, QJsonObject>& displayV12Controls()
{
    static const QHash<QString, QJsonObject> table = [] {
        QHash<QString, QJsonObject> controls;
        const QJsonArray rows = QJsonDocument::fromJson(QByteArray(kDisplayV12Controls)).array();
        for (const QJsonValue& row : rows) {
            const QJsonObject control = row.toObject();
            controls.insert(control.value(QStringLiteral("id")).toString(), control);
        }
        return controls;
    }();
    return table;
}


QJsonObject expectedAntennaRowsTable(bool tx, HPSDRModel model)
{
    const SkuUiProfile sku = skuUiProfileFor(model);
    QJsonArray columns;
    for (int antenna = 1; antenna <= 3; ++antenna) {
        const QString id = (tx ? QStringLiteral("tx%1") : QStringLiteral("rx%1"))
                               .arg(antenna);
        columns.append(QJsonObject{{QStringLiteral("id"), id},
                                   {QStringLiteral("field"), tx ? QStringLiteral("tx")
                                                               : QStringLiteral("rx")},
                                   {QStringLiteral("antenna"), antenna},
                                   {QStringLiteral("label"), tx ? QStringLiteral("Ant %1").arg(antenna)
                                                                : QString::number(antenna)}});
    }
    if (!tx) {
        for (int antenna = 1; antenna <= 3; ++antenna) {
            columns.append(QJsonObject{{QStringLiteral("id"),
                                        QStringLiteral("rxOnly%1").arg(antenna)},
                                       {QStringLiteral("field"), QStringLiteral("rxOnly")},
                                       {QStringLiteral("antenna"), antenna},
                                       {QStringLiteral("label"), sku.rxOnlyLabels[antenna - 1]}});
        }
    }
    QJsonArray rows;
    // One row per entry of the antenna lists, in their order: 160m .. XVTR,
    // then 2 m (R-IOS-26). A row carries its band's number (2 m is 27).
    for (int slot = 0; slot < AlexAntennaFacade::kBandCount; ++slot) {
        const int band = static_cast<int>(bandFromPerBandStateSlot(slot));
        const QString label = bandLabel(static_cast<Band>(band));
        QJsonArray cells;
        for (const QJsonValue& rawColumn : columns) {
            const QJsonObject column = rawColumn.toObject();
            const int antenna = column.value(QStringLiteral("antenna")).toInt();
            const QString field = column.value(QStringLiteral("field")).toString();
            const QString tooltip = field == QLatin1String("tx")
                ? QStringLiteral("TX Ant %1 for %2").arg(antenna).arg(label)
                : field == QLatin1String("rx")
                ? QStringLiteral("RX1 Ant %1 for %2").arg(antenna).arg(label)
                : QStringLiteral("RX-only %1 for %2")
                      .arg(sku.rxOnlyLabels[antenna - 1], label);
            cells.append(QJsonObject{{QStringLiteral("column"), column.value(QStringLiteral("id"))},
                                     {QStringLiteral("tooltip"), tooltip}});
        }
        rows.append(QJsonObject{{QStringLiteral("band"), band},
                                {QStringLiteral("label"), label},
                                {QStringLiteral("cells"), cells}});
    }
    QJsonObject gate{{QStringLiteral("capability"), QStringLiteral("radioAntennaRowsVersion")},
                     {QStringLiteral("min"), 1}};
    if (tx) { gate.insert(QStringLiteral("offAir"), true); }
    QJsonObject control{{QStringLiteral("id"), tx ? QStringLiteral("hardware.antenna.txRows")
                                                   : QStringLiteral("hardware.antenna.rxRows")},
                        {QStringLiteral("label"), tx ? QStringLiteral("TX Antenna per Band")
                                                      : QStringLiteral("RX1 / RX2 Antenna per Band")},
                        {QStringLiteral("tooltip"), QString()},
                        {QStringLiteral("kind"), QStringLiteral("table")},
                        {QStringLiteral("binding"), QJsonObject{{QStringLiteral("antennaRows"),
                            QJsonObject{{QStringLiteral("object"), QStringLiteral("alexAntennas")},
                                        {QStringLiteral("mode"), tx ? QStringLiteral("tx")
                                                                     : QStringLiteral("rx")}}}}},
                        {QStringLiteral("applies"), QStringLiteral("live")},
                        {QStringLiteral("requiresDescriptionVersion"), 6},
                        {QStringLiteral("gate"), gate},
                        {QStringLiteral("rows"), rows},
                        {QStringLiteral("columns"), columns}};
    if (!tx) {
        control.insert(QStringLiteral("columnGroups"), QJsonArray{
            QJsonObject{{QStringLiteral("label"), QStringLiteral("RX1")},
                        {QStringLiteral("columns"), QJsonArray{QStringLiteral("rx1"),
                                                                QStringLiteral("rx2"),
                                                                QStringLiteral("rx3")}}},
            QJsonObject{{QStringLiteral("label"), QStringLiteral("RX-only")},
                        {QStringLiteral("columns"), QJsonArray{QStringLiteral("rxOnly1"),
                                                                QStringLiteral("rxOnly2"),
                                                                QStringLiteral("rxOnly3")}}}});
    }
    return control;
}

QJsonObject expectedSettingsHygienePanel()
{
    return QJsonObject{
        {QStringLiteral("id"), QStringLiteral("diagnostics.settingsValidation.health")},
        {QStringLiteral("label"), QStringLiteral("Validation Issues")},
        {QStringLiteral("tooltip"), QString()},
        {QStringLiteral("kind"), QStringLiteral("settingsHygiene")},
        {QStringLiteral("requiresDescriptionVersion"), 3},
        {QStringLiteral("binding"), QJsonObject{{QStringLiteral("settingsHygiene"),
            QJsonObject{{QStringLiteral("version"), 1}}}}},
        {QStringLiteral("applies"), QStringLiteral("live")},
        {QStringLiteral("gate"), QJsonObject{
            {QStringLiteral("capability"), QStringLiteral("settingsHygieneVersion")},
            {QStringLiteral("min"), 1}}},
        {QStringLiteral("actions"), QJsonArray{
            QJsonObject{{QStringLiteral("id"), QStringLiteral("validate")},
                        {QStringLiteral("label"), QStringLiteral("Re-validate")}},
            // G-38: Repair Invalid Settings (station.repairSettings),
            // with Forget's gates, on a Core at settingsHygieneVersion 2.
            QJsonObject{{QStringLiteral("id"), QStringLiteral("repair")},
                        {QStringLiteral("label"), QStringLiteral("Repair Invalid Settings")},
                        {QStringLiteral("paired"), true},
                        {QStringLiteral("offAir"), true},
                        {QStringLiteral("gate"), QJsonObject{
                            {QStringLiteral("capability"), QStringLiteral("settingsHygieneVersion")},
                            {QStringLiteral("min"), 2}}},
                        {QStringLiteral("reason"), QStringLiteral(
                            "Repair invalid settings is not available on this Core. "
                            "Updating the Core may help.")},
                        {QStringLiteral("confirmation"), QJsonObject{
                            {QStringLiteral("title"), QStringLiteral("Repair Settings")},
                            {QStringLiteral("message"), QStringLiteral(
                                "Repair the settings that are invalid for this radio?")},
                            {QStringLiteral("default"), QStringLiteral("cancel")}}}},
            QJsonObject{{QStringLiteral("id"), QStringLiteral("forget")},
                        {QStringLiteral("label"), QStringLiteral("Forget This Radio")},
                        {QStringLiteral("paired"), true},
                        {QStringLiteral("offAir"), true},
                        {QStringLiteral("confirmation"), QJsonObject{
                            {QStringLiteral("title"), QStringLiteral("Forget Radio")},
                            {QStringLiteral("message"), QStringLiteral(
                                "Forget all settings for this radio?")},
                            {QStringLiteral("default"), QStringLiteral("cancel")}}}}}}};
}

bool validDiagnosticsEnvelope(const QJsonObject& root)
{
    const QJsonArray pages = root.value(QStringLiteral("pages")).toArray();
    // Version 15 adds Radio Status and Connection Quality before Settings
    // Validation (the desktop's order); their rows are version 15 rows.
    const bool v15 = root.value(QStringLiteral("version"))
        == QJsonValue(SetupDescriptionV15::kVersion);
    if (root.size() != 3
        || (root.value(QStringLiteral("version")) != QJsonValue(3) && !v15)
        || root.value(QStringLiteral("category")) != QJsonValue(QJsonObject{
            {QStringLiteral("id"), QStringLiteral("diagnostics")},
            {QStringLiteral("title"), QStringLiteral("Diagnostics")},
            {QStringLiteral("where"), QStringLiteral("station")},
            {QStringLiteral("coverage"), QStringLiteral("partial")}})
        || pages.isEmpty() || (!v15 && pages.size() != 1)) {
        return false;
    }
    int validation = -1;
    for (int p = 0; p < pages.size(); ++p) {
        if (pages.at(p).toObject().value(QStringLiteral("id"))
            == QJsonValue(QStringLiteral("diagnostics.settingsValidation"))) {
            if (validation >= 0) { return false; }
            validation = p;
            continue;
        }
        for (const QJsonValue& section : pages.at(p).toObject().value(QStringLiteral("sections")).toArray()) {
            for (const QJsonValue& control : section.toObject().value(QStringLiteral("controls")).toArray()) {
                if (control.toObject().value(QStringLiteral("requiresDescriptionVersion"))
                    != QJsonValue(SetupDescriptionV15::kVersion)) {
                    return false;
                }
            }
        }
    }
    if (validation < 0) { return false; }
    const QJsonObject page = pages.at(validation).toObject();
    const QJsonArray sections = page.value(QStringLiteral("sections")).toArray();
    if (page.size() != 5
        || page.value(QStringLiteral("id")) != QJsonValue(QStringLiteral("diagnostics.settingsValidation"))
        || page.value(QStringLiteral("title")) != QJsonValue(QStringLiteral("Settings Validation"))
        || page.value(QStringLiteral("where")) != QJsonValue(QStringLiteral("station"))
        || page.value(QStringLiteral("coverage")) != QJsonValue(QStringLiteral("partial"))
        || sections.size() != 1) {
        return false;
    }
    const QJsonObject section = sections.first().toObject();
    const QJsonArray controls = section.value(QStringLiteral("controls")).toArray();
    return section.size() == 2
        && section.value(QStringLiteral("title")) == QJsonValue(QStringLiteral("Validation Issues"))
        && controls.size() == 1
        && SetupDescription::validateSettingsHygienePanel(controls.first().toObject());
}

bool validAppearanceEnvelope(const QJsonObject& root)
{
    const QJsonArray pages = root.value(QStringLiteral("pages")).toArray();
    if (root.size() != 3 || root.value(QStringLiteral("version")) != QJsonValue(12)
        || root.value(QStringLiteral("category")) != QJsonValue(QJsonObject{
            {QStringLiteral("id"), QStringLiteral("appearance")},
            {QStringLiteral("title"), QStringLiteral("Appearance")},
            {QStringLiteral("where"), QStringLiteral("phone")},
            {QStringLiteral("coverage"), QStringLiteral("partial")}})
        || pages.size() != 2) { return false; }
    const QJsonObject page = pages.first().toObject();
    const QJsonArray sections = page.value(QStringLiteral("sections")).toArray();
    if (page.size() != 5 || page.value(QStringLiteral("id")) != QJsonValue(QStringLiteral("appearance.colorsTheme"))
        || page.value(QStringLiteral("title")) != QJsonValue(QStringLiteral("Colors & Theme"))
        || page.value(QStringLiteral("where")) != QJsonValue(QStringLiteral("phone"))
        || page.value(QStringLiteral("coverage")) != QJsonValue(QStringLiteral("partial"))
        || sections.size() != 2) { return false; }
    const QJsonObject section = sections.first().toObject();
    if (section.size() != 2
        || section.value(QStringLiteral("title")) != QJsonValue(QStringLiteral("Spectrum"))
        || section.value(QStringLiteral("controls")).toArray().size() != 10) { return false; }
    // Version 12: the Reset section holds only Reset all colors.
    const QJsonObject reset = sections.at(1).toObject();
    const QJsonArray resetControls = reset.value(QStringLiteral("controls")).toArray();
    if (reset.size() != 2
        || reset.value(QStringLiteral("title")) != QJsonValue(QStringLiteral("Reset"))
        || resetControls.size() != 1
        || !SetupDescription::validateAppearanceResetColours(resetControls.first().toObject())) {
        return false;
    }
    const QJsonObject meterPage = pages.at(1).toObject();
    const QJsonArray meterSections = meterPage.value(QStringLiteral("sections")).toArray();
    if (meterPage.size() != 5
        || meterPage.value(QStringLiteral("id")) != QJsonValue(QStringLiteral("appearance.meterStyles"))
        || meterPage.value(QStringLiteral("title")) != QJsonValue(QStringLiteral("Meter Styles"))
        || meterPage.value(QStringLiteral("where")) != QJsonValue(QStringLiteral("phone"))
        || meterPage.value(QStringLiteral("coverage")) != QJsonValue(QStringLiteral("partial"))
        || meterSections.size() != 1) { return false; }
    const QJsonObject meterSection = meterSections.first().toObject();
    return meterSection.size() == 2
        && meterSection.value(QStringLiteral("title")) == QJsonValue(QStringLiteral("S-Meter"))
        && meterSection.value(QStringLiteral("controls")).toArray().size() == 3;
}

QString loadCategory(const QString& id, const BoardCapabilities& caps, HPSDRModel model,
                     const RadioInfo& info)
{
    QFile resource(QStringLiteral(":/setup/%1.json").arg(id));
    if (!resource.open(QIODevice::ReadOnly)) {
        return {};
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(resource.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }
    QJsonObject root = document.object();
    if ((root.value(QStringLiteral("version")).toInt() != 1
         && root.value(QStringLiteral("version")).toInt() != 2
         && !(id == QLatin1String("diagnostics")
              && root.value(QStringLiteral("version")) == QJsonValue(3))
         && !(id == QLatin1String("display")
              && root.value(QStringLiteral("version")) == QJsonValue(12))
         && !(id == QLatin1String("appearance")
              && root.value(QStringLiteral("version")) == QJsonValue(12))
         && !(id == QLatin1String("pa")
              && root.value(QStringLiteral("version")) == QJsonValue(14))
         && !(id == QLatin1String("dsp")
              && root.value(QStringLiteral("version")) == QJsonValue(19))
         && !(id == QLatin1String("transmit")
              && root.value(QStringLiteral("version")) == QJsonValue(13))
         // Version 23: Calibration's Rx1 6m LNA row.
         && !(id == QLatin1String("hardware")
              && root.value(QStringLiteral("version")) == QJsonValue(23))
         // Version 24: TX Input's Line In Gain steps and Saturn Mic Tip-Ring.
         && !(id == QLatin1String("audio")
              && root.value(QStringLiteral("version")) == QJsonValue(24))
         // Version 21: CAT & Network's TCI Forget row greys out with Duplicate.
         && !(id == QLatin1String("catNetwork")
              && root.value(QStringLiteral("version")) == QJsonValue(21))
         && !(SetupDescriptionV15::isCategory(id)
              && root.value(QStringLiteral("version"))
                  == QJsonValue(SetupDescriptionV15::kVersion)))
        || root.value(QStringLiteral("category")).toObject()
               .value(QStringLiteral("id")).toString() != id
        || root.value(QStringLiteral("pages")).toArray().isEmpty()
        || (id == QLatin1String("diagnostics") && !validDiagnosticsEnvelope(root))
        || (id == QLatin1String("appearance") && !validAppearanceEnvelope(root))) {
        return {};
    }
    QSet<QString> ids;
    int antennaTables = 0;
    for (const QJsonValue& rawPage : root.value(QStringLiteral("pages")).toArray()) {
        const QJsonObject page = rawPage.toObject();
        const QString pageId = page.value(QStringLiteral("id")).toString();
        if (pageId.isEmpty() || ids.contains(pageId)
            || page.value(QStringLiteral("title")).toString().isEmpty()
                    || page.value(QStringLiteral("sections")).toArray().isEmpty()) {
            return {};
        }
        ids.insert(pageId);
        for (const QJsonValue& rawSection : page.value(QStringLiteral("sections")).toArray()) {
            const QJsonObject section = rawSection.toObject();
            if (section.value(QStringLiteral("title")).toString().isEmpty()
                || section.value(QStringLiteral("controls")).toArray().isEmpty()) {
                return {};
            }
            if (!SetupDescriptionV15::validateSection(
                    id, section.value(QStringLiteral("controls")).toArray())) {
                return {};
            }
            for (const QJsonValue& rawControl : section.value(QStringLiteral("controls")).toArray()) {
                const QJsonObject control = rawControl.toObject();
                const QString controlId = control.value(QStringLiteral("id")).toString();
                // Version 15 rows are checked against the Core's own sources
                // (SetupDescriptionV15::validateControl, run by
                // validateSection above), not the older category rules.
                if (control.value(QStringLiteral("requiresDescriptionVersion"))
                    == QJsonValue(SetupDescriptionV15::kVersion)) {
                    if (controlId.isEmpty() || ids.contains(controlId)
                        || !SetupDescriptionV15::isCategory(id)
                        || (root.value(QStringLiteral("version"))
                                != QJsonValue(SetupDescriptionV15::kVersion)
                            && !(id == QLatin1String("dsp")
                                 && root.value(QStringLiteral("version")) == QJsonValue(19))
                            && !(id == QLatin1String("catNetwork")
                                 && root.value(QStringLiteral("version")) == QJsonValue(21))
                            && !(id == QLatin1String("audio")
                                 && root.value(QStringLiteral("version")) == QJsonValue(24)))) {
                        return {};
                    }
                    ids.insert(controlId);
                    continue;
                }
                // Version 24: TX Input's two rows, accepted only as the exact
                // closed rows (validateAudioV24Control).
                if (id == QLatin1String("audio")
                    && control.value(QStringLiteral("requiresDescriptionVersion"))
                        == QJsonValue(24)) {
                    if (controlId.isEmpty() || ids.contains(controlId)
                        || root.value(QStringLiteral("version")) != QJsonValue(24)
                        || !SetupDescription::validateAudioV24Control(control)) {
                        return {};
                    }
                    ids.insert(controlId);
                    continue;
                }
                // Version 19: DSP > CFC's band editor, accepted only as the
                // exact closed row (validateDspV19Control).
                if (id == QLatin1String("dsp")
                    && control.value(QStringLiteral("requiresDescriptionVersion"))
                        == QJsonValue(19)) {
                    if (controlId.isEmpty() || ids.contains(controlId)
                        || root.value(QStringLiteral("version")) != QJsonValue(19)
                        || !SetupDescription::validateDspV19Control(control)) {
                        return {};
                    }
                    ids.insert(controlId);
                    continue;
                }
                if (controlId.isEmpty() || ids.contains(controlId)
                    || control.value(QStringLiteral("label")).toString().isEmpty()
                    || !control.value(QStringLiteral("binding")).isObject()
                    || (id != QLatin1String("display")
                        && ((control.contains(QStringLiteral("options"))
                             && !(id == QLatin1String("appearance")
                                  && SetupDescription::validateAppearanceMeterStyleBinding(control))
                             && !(id == QLatin1String("hardware")
                                  && SetupDescription::validateHardwareV13Control(control)))
                            || (control.contains(QStringLiteral("enabledWhen"))
                                && !(id == QLatin1String("hardware")
                                     && SetupDescription::validateHardwareV18Control(control))
                                && !(id == QLatin1String("catNetwork")
                                     && root.value(QStringLiteral("version")) == QJsonValue(21)
                                     && SetupDescription::validateCatNetworkV21EnabledWhen(control)))))
                    || (control.contains(QStringLiteral("requiresDescriptionVersion"))
                        && (root.value(QStringLiteral("version")).toInt()
                                < control.value(QStringLiteral("requiresDescriptionVersion")).toInt()
                            || (control.value(QStringLiteral("requiresDescriptionVersion"))
                                    != QJsonValue(2)
                                && control.value(QStringLiteral("requiresDescriptionVersion"))
                                    != QJsonValue(3)
                                && !(id == QLatin1String("display")
                                     && control.value(QStringLiteral("requiresDescriptionVersion"))
                                         == QJsonValue(4))
                                && !(id == QLatin1String("pa")
                                     && control.value(QStringLiteral("requiresDescriptionVersion"))
                                         == QJsonValue(5))
                                && !(id == QLatin1String("hardware")
                                     && control.value(QStringLiteral("requiresDescriptionVersion"))
                                         == QJsonValue(6))
                                && !(id == QLatin1String("appearance")
                                     && control.value(QStringLiteral("requiresDescriptionVersion"))
                                         == QJsonValue(7))
                                && !(id == QLatin1String("display")
                                     && control.value(QStringLiteral("requiresDescriptionVersion"))
                                         == QJsonValue(8))
                                && !(id == QLatin1String("display")
                                     && control.value(QStringLiteral("requiresDescriptionVersion"))
                                         == QJsonValue(9))
                                && !(id == QLatin1String("display")
                                     && control.value(QStringLiteral("requiresDescriptionVersion"))
                                         == QJsonValue(10))
                                && !(id == QLatin1String("display")
                                     && control.value(QStringLiteral("requiresDescriptionVersion"))
                                         == QJsonValue(11))
                                && !((id == QLatin1String("display")
                                      || id == QLatin1String("appearance"))
                                     && control.value(QStringLiteral("requiresDescriptionVersion"))
                                         == QJsonValue(12))
                                && !((id == QLatin1String("pa")
                                      || id == QLatin1String("hardware")
                                      || id == QLatin1String("transmit"))
                                     && control.value(QStringLiteral("requiresDescriptionVersion"))
                                         == QJsonValue(13))
                                && !(id == QLatin1String("pa")
                                     && control.value(QStringLiteral("requiresDescriptionVersion"))
                                         == QJsonValue(14))
                                && !(id == QLatin1String("hardware")
                                     && (control.value(QStringLiteral("requiresDescriptionVersion"))
                                             == QJsonValue(16)
                                         || control.value(QStringLiteral("requiresDescriptionVersion"))
                                             == QJsonValue(17)
                                         || control.value(QStringLiteral("requiresDescriptionVersion"))
                                             == QJsonValue(18)
                                         || control.value(QStringLiteral("requiresDescriptionVersion"))
                                             == QJsonValue(23))))))
                    || (control.value(QStringLiteral("kind")) == QJsonValue(QStringLiteral("table"))
                        && !((id == QLatin1String("dsp")
                              && SetupDescription::validateTnfTable(control))
                             || (id == QLatin1String("hardware")
                                 && SetupDescription::validateAntennaRowsTable(control))
                             || (id == QLatin1String("pa")
                                 && SetupDescription::validatePaV14Control(control))))
                    || (control.value(QStringLiteral("kind")) != QJsonValue(QStringLiteral("table"))
                        && (control.value(QStringLiteral("binding")).toObject().contains(QStringLiteral("table"))
                            || control.value(QStringLiteral("binding")).toObject()
                                   .contains(QStringLiteral("antennaRows"))))
                    || !SetupDescription::validateSettingToggleEncoding(control)
                    || (id == QLatin1String("dsp")
                        && !control.value(QStringLiteral("binding")).toObject()
                                .contains(QStringLiteral("property"))
                        && !control.value(QStringLiteral("binding")).toObject()
                                .contains(QStringLiteral("command"))
                        && !control.value(QStringLiteral("binding")).toObject()
                                .contains(QStringLiteral("table"))
                        && !control.value(QStringLiteral("binding")).toObject()
                                .contains(QStringLiteral("setting")))
                    || (id == QLatin1String("dsp")
                        && control.value(QStringLiteral("binding")).toObject()
                               .contains(QStringLiteral("setting"))
                        && !SetupDescription::validateDspSettingBinding(control))
                    || (id == QLatin1String("display")
                        && !SetupDescription::validateDisplaySettingBinding(control)
                        && !SetupDescription::validateDisplayPhoneBinding(control))
                    || (id == QLatin1String("dsp")
                        && control.value(QStringLiteral("binding")).toObject()
                               .contains(QStringLiteral("property"))
                        && !SetupDescription::validateActiveSlicePropertyBinding(control))
                    || (id == QLatin1String("transmit")
                        && !SetupDescription::validateTransmitPropertyBinding(control)
                        && !SetupDescription::validateTransmitSettingBinding(control)
                        && !SetupDescription::validateTransmitV13Control(control))
                    || (id == QLatin1String("hardware")
                        && !SetupDescription::validateHardwarePropertyBinding(control)
                        && !SetupDescription::validateAntennaRowsTable(control)
                        && !SetupDescription::validateHardwareV13Control(control)
                        && !SetupDescription::validateHardwareV16Control(control)
                        && !SetupDescription::validateHardwareV18Control(control)
                        && !SetupDescription::validateHardwareV23Control(control))
                    || (id == QLatin1String("pa")
                        && !SetupDescription::validatePaReadoutBinding(control)
                        && !SetupDescription::validatePaDriveReadoutBinding(control)
                        && !SetupDescription::validatePaTelemetryReadoutBinding(control)
                        && !SetupDescription::validatePaBypassBinding(control)
                        && !SetupDescription::validatePaV13Control(control)
                        && !SetupDescription::validatePaV14Control(control))
                    || (id == QLatin1String("audio")
                        && !SetupDescription::validateAudioPropertyBinding(control))
                    || (id == QLatin1String("appearance")
                        && !SetupDescription::validateAppearanceColourBinding(control)
                        && !SetupDescription::validateAppearanceMeterStyleBinding(control)
                        && !SetupDescription::validateAppearanceResetColours(control))
                    || (id == QLatin1String("diagnostics")
                        && !SetupDescription::validateSettingsHygienePanel(control))
                    || (control.value(QStringLiteral("binding")).toObject().contains(QStringLiteral("command"))
                        && !SetupDescription::validateCommandBinding(control))) {
                    return {};
                }
                if (id == QLatin1String("hardware")
                    && control.value(QStringLiteral("kind")) == QJsonValue(QStringLiteral("table"))) {
                    ++antennaTables;
                }
                ids.insert(controlId);
            }
        }
    }
    if (id == QLatin1String("hardware") && antennaTables != 2) { return {}; }
    // The Antenna / ALEX page is built only when the connected board has
    // ALEX filters, and HL2 I/O only on the HL2's I/O board (HardwarePage's
    // own tab gates). A board change must retire their old controls
    // altogether. Radio Info and Calibration are on every board.
    if (id == QLatin1String("hardware")) {
        QJsonArray pages = root.value(QStringLiteral("pages")).toArray();
        for (int p = 0; p < pages.size(); ++p) {
            const QJsonValue pageId = pages.at(p).toObject().value(QStringLiteral("id"));
            if ((pageId == QJsonValue(QStringLiteral("hardware.antennaAlex")) && !caps.hasAlexFilters)
                || (pageId == QJsonValue(QStringLiteral("hardware.alex1Filters"))
                    && !caps.hasAlexFilters)
                || (pageId == QJsonValue(QStringLiteral("hardware.alex2Filters"))
                    && !(caps.hasAlexFilters && caps.hasAlex2))
                || (pageId == QJsonValue(QStringLiteral("hardware.hl2Io")) && !caps.hasIoBoardHl2)) {
                pages.removeAt(p--);
                continue;
            }
            // Version 13: the Alex-1 Filters tab shows either the Alex HPF
            // Bands or the Saturn BPF1 Bands: the bank the Core programs for
            // the board (codec::alex::usesBpf1Preselector, the selector
            // computeRxPreselector uses), as AntennaAlexTab::populate does.
            // The five HPF / 6 m LNA switches go with whichever section is
            // shown, first.
            // From Thetis console.cs:6827-6837 [v2.10.3.15] (setAlex1HPF):
            //   if ((HardwareSpecific.Hardware == HPSDRHW.OrionMKII) || (HardwareSpecific.Hardware == HPSDRHW.Saturn)
            //      || (HardwareSpecific.Hardware == HPSDRHW.HermesC10))  //N1GP G2E added (HermesC10) //DK1HLM
            //   { setBPF1ForOrionIISaturn(freq); } else { setAlexHPF(freq); }
            // From Thetis setup.cs:6336-6360 [v2.10.3.15] (the panel list by model):
            //   HardwareSpecific.Model != HPSDRModel.ANAN_G2E && //N1GP G2E added
            //   HardwareSpecific.Model != HPSDRModel.REDPITAYA)//DH1KLM
            //   { panelBPFControl.Visible = false; panelAlex1HPFControl.Visible = true; ... }
            // From Thetis setup.cs:20208-20220 [v2.10.3.15] (7000D; the other
            //   BPF-panel cases match): panelAlex1HPFControl.Visible = false;
            //   panelBPFControl.Visible = true; switches moved to panelBPFControl.
            // The two agree for every model but the plain ORIONMKII, which is
            // on the OrionMKII board: Thetis programs BPF1 for it but shows
            // the HPF panel. The page describes the rows that take effect.
            if (pageId == QJsonValue(QStringLiteral("hardware.alex1Filters"))) {
                const bool bpfPanel = codec::alex::usesBpf1Preselector(caps.board);
                const QString hpfTitle = QStringLiteral("Alex HPF Bands");
                const QString bpf1Title = QStringLiteral("Saturn BPF1 Bands");
                QJsonObject page = pages.at(p).toObject();
                QJsonArray sections = page.value(QStringLiteral("sections")).toArray();
                QJsonArray switchRows;
                for (int s = 0; s < sections.size(); ++s) {
                    QJsonObject section = sections.at(s).toObject();
                    const QString title = section.value(QStringLiteral("title")).toString();
                    if (bpfPanel && title == hpfTitle) {
                        const QJsonArray rows = section.value(QStringLiteral("controls")).toArray();
                        for (const QJsonValue& row : rows) {
                            const QString id = row.toObject().value(QStringLiteral("id")).toString();
                            if (!id.startsWith(QStringLiteral("hardware.alex1Filters.hpf."))) {
                                switchRows.append(row);
                            }
                        }
                        sections.removeAt(s--);
                    } else if (!bpfPanel && title == bpf1Title) {
                        sections.removeAt(s--);
                    }
                }
                if (bpfPanel) {
                    for (int s = 0; s < sections.size(); ++s) {
                        QJsonObject section = sections.at(s).toObject();
                        if (section.value(QStringLiteral("title")).toString() != bpf1Title) {
                            continue;
                        }
                        QJsonArray rows = switchRows;
                        for (const QJsonValue& row : section.value(QStringLiteral("controls")).toArray()) {
                            rows.append(row);
                        }
                        section.insert(QStringLiteral("controls"), rows);
                        sections[s] = section;
                    }
                }
                page.insert(QStringLiteral("sections"), sections);
                pages[p] = page;
            }
        }
        if (pages.isEmpty()) { return {}; }
        root.insert(QStringLiteral("pages"), pages);
    }
    if (id == QLatin1String("pa") && (!caps.hasPaProfile || caps.isRxOnlySku)) {
        return {};
    }
    // PA Gain's bypass box is the ANAN-G2E's only; version 14's profile
    // rows are on every radio with a PA. A page left with no rows goes.
    if (id == QLatin1String("pa") && !caps.showsBypassPaSettingsUi) {
        QJsonArray pages = root.value(QStringLiteral("pages")).toArray();
        for (int p = 0; p < pages.size(); ++p) {
            QJsonObject page = pages.at(p).toObject();
            if (page.value(QStringLiteral("id")) != QJsonValue(QStringLiteral("pa.gain"))) {
                continue;
            }
            QJsonArray sections = page.value(QStringLiteral("sections")).toArray();
            for (int s = 0; s < sections.size(); ++s) {
                QJsonObject section = sections.at(s).toObject();
                QJsonArray controls = section.value(QStringLiteral("controls")).toArray();
                for (int k = 0; k < controls.size(); ++k) {
                    if (controls.at(k).toObject().value(QStringLiteral("id"))
                        == QJsonValue(QStringLiteral("pa.gain.bypassPaSettings"))) {
                        controls.removeAt(k--);
                    }
                }
                if (controls.isEmpty()) { sections.removeAt(s--); continue; }
                section.insert(QStringLiteral("controls"), controls);
                sections[s] = section;
            }
            if (sections.isEmpty()) {
                pages.removeAt(p--);
                continue;
            }
            page.insert(QStringLiteral("sections"), sections);
            pages[p] = page;
        }
        root.insert(QStringLiteral("pages"), pages);
    }
    if (id == QLatin1String("pa")) {
        QJsonArray pages = root.value(QStringLiteral("pages")).toArray();
        for (int p = 0; p < pages.size(); ++p) {
            QJsonObject page = pages.at(p).toObject();
            if (page.value(QStringLiteral("id")) != QJsonValue(QStringLiteral("pa.values"))) {
                continue;
            }
            QJsonArray sections = page.value(QStringLiteral("sections")).toArray();
            for (int s = 0; s < sections.size(); ++s) {
                QJsonObject section = sections.at(s).toObject();
                QJsonArray controls = section.value(QStringLiteral("controls")).toArray();
                for (int c = 0; c < controls.size(); ++c) {
                    const QString controlId = controls.at(c).toObject().value(QStringLiteral("id")).toString();
                    if ((controlId == QLatin1String("pa.values.paCurrent") && !caps.hasPaAmpsTelemetry)
                        || (controlId == QLatin1String("pa.values.dcVoltage") && !caps.hasPaVoltsTelemetry)) {
                        controls.removeAt(c--);
                    }
                }
                if (controls.isEmpty()) { sections.removeAt(s--); }
                else { section.insert(QStringLiteral("controls"), controls); sections[s] = section; }
            }
            page.insert(QStringLiteral("sections"), sections);
            pages[p] = page;
        }
        root.insert(QStringLiteral("pages"), pages);
    }
    if (id == QLatin1String("hardware")) {
        const SkuUiProfile sku = skuUiProfileFor(model);
        QJsonArray pages = root.value(QStringLiteral("pages")).toArray();
        for (int p = 0; p < pages.size(); ++p) {
            QJsonObject page = pages.at(p).toObject();
            // The SKU labels are Antenna / ALEX's; the version 13 pages
            // have none.
            if (page.value(QStringLiteral("id")) != QJsonValue(QStringLiteral("hardware.antennaAlex"))) {
                continue;
            }
            QJsonArray sections = page.value(QStringLiteral("sections")).toArray();
            for (int s = 0; s < sections.size(); ++s) {
                QJsonObject section = sections.at(s).toObject();
                QJsonArray controls = section.value(QStringLiteral("controls")).toArray();
                for (int c = 0; c < controls.size(); ++c) {
                    QJsonObject control = controls.at(c).toObject();
                    if (control.value(QStringLiteral("kind")) == QJsonValue(QStringLiteral("table"))) {
                        const bool tx = control.value(QStringLiteral("id"))
                            == QJsonValue(QStringLiteral("hardware.antenna.txRows"));
                        control = expectedAntennaRowsTable(tx, model);
                        if (!SetupDescription::validateAntennaRowsTable(control, model)) {
                            return {};
                        }
                        controls[c] = control;
                        continue;
                    }
                    const QString name = control.value(QStringLiteral("binding")).toObject()
                        .value(QStringLiteral("property")).toObject()
                        .value(QStringLiteral("name")).toString();
                    const bool visible = name == QLatin1String("rxOutOnTx")
                        ? sku.hasRxOutOnTx : name == QLatin1String("ext1OutOnTx")
                        ? sku.hasExt1OutOnTx : name == QLatin1String("ext2OutOnTx")
                        ? sku.hasExt2OutOnTx : name == QLatin1String("rxOutOverride")
                        ? sku.hasRxBypassUi : true;
                    if (!visible) {
                        controls.removeAt(c--);
                        continue;
                    }
                    if (name == QLatin1String("ext1OutOnTx")) {
                        control.insert(QStringLiteral("label"), sku.ext1OutOnTxLabel);
                    } else if (name == QLatin1String("ext2OutOnTx")) {
                        control.insert(QStringLiteral("label"), sku.ext2OutOnTxLabel);
                        control.insert(QStringLiteral("tooltip"), sku.ext2OutOnTxTooltip);
                    }
                    if (!SetupDescription::validateHardwarePropertyBinding(control, model)) {
                        return {};
                    }
                    controls[c] = control;
                }
                section.insert(QStringLiteral("controls"), controls);
                sections[s] = section;
            }
            page.insert(QStringLiteral("sections"), sections);
            pages[p] = page;
        }
        root.insert(QStringLiteral("pages"), pages);
    }
    // Version 13: Radio Info's values and its support text, as the
    // desktop's tab shows them for this Core's radio (radioInfoFacts).
    if (id == QLatin1String("hardware")) {
        const RadioInfoFacts facts = radioInfoFacts(info, caps, model);
        QJsonArray pages = root.value(QStringLiteral("pages")).toArray();
        for (int p = 0; p < pages.size(); ++p) {
            QJsonObject page = pages.at(p).toObject();
            if (page.value(QStringLiteral("id")) != QJsonValue(QStringLiteral("hardware.radioInfo"))) {
                continue;
            }
            QJsonArray sections = page.value(QStringLiteral("sections")).toArray();
            for (int s = 0; s < sections.size(); ++s) {
                QJsonObject section = sections.at(s).toObject();
                QJsonArray controls = section.value(QStringLiteral("controls")).toArray();
                for (int c = 0; c < controls.size(); ++c) {
                    QJsonObject control = controls.at(c).toObject();
                    const QString field = control.value(QStringLiteral("binding")).toObject()
                        .value(QStringLiteral("radioInfo")).toString();
                    if (control.contains(QStringLiteral("copyText"))) {
                        control.insert(QStringLiteral("copyText"), facts.supportText());
                    } else if (control.value(QStringLiteral("id"))
                               == QJsonValue(QStringLiteral("hardware.radioInfo.sampleRate"))) {
                        // The rates the desktop's box lists for this radio
                        // (allowedSampleRates, as RadioInfoTab::populate).
                        QJsonArray options;
                        for (int rate : allowedSampleRates(info.protocol, caps, model)) {
                            options.append(QJsonObject{{QStringLiteral("value"), rate},
                                                       {QStringLiteral("label"),
                                                        QString::number(rate)}});
                        }
                        control.insert(QStringLiteral("options"), options);
                        if (options.isEmpty()) {
                            control.insert(QStringLiteral("availability"), QJsonObject{
                                {QStringLiteral("enabled"), false},
                                {QStringLiteral("reason"), QStringLiteral(
                                    "The radio is not connected, so its sample rate cannot change.")}});
                        }
                    } else if (!field.isEmpty()) {
                        control.insert(QStringLiteral("value"),
                            field == QLatin1String("board") ? facts.board
                            : field == QLatin1String("protocol") ? facts.protocol
                            : field == QLatin1String("adcCount") ? facts.adcCount
                            : field == QLatin1String("maxRx") ? facts.maxRx
                            : field == QLatin1String("firmware") ? facts.firmware
                            : field == QLatin1String("mac") ? facts.mac : facts.ip);
                    }
                    controls[c] = control;
                }
                section.insert(QStringLiteral("controls"), controls);
                sections[s] = section;
            }
            page.insert(QStringLiteral("sections"), sections);
            pages[p] = page;
        }
        root.insert(QStringLiteral("pages"), pages);
    }
    // Version 13: the Watt Meter's ten points are the ones the desktop
    // builds for this radio's board class (PaCalibrationGroup): each
    // labelled with its factory value, which is also its default, and held
    // to Thetis's range for that box (paCalPointSpec). A board with no
    // class (or no model yet) has no points.
    if (id == QLatin1String("pa")) {
        const PaCalBoardClass boardClass = paCalBoardClassFor(model);
        const PaCalProfile defaults = PaCalProfile::defaults(boardClass);
        QJsonArray pages = root.value(QStringLiteral("pages")).toArray();
        for (int p = 0; p < pages.size(); ++p) {
            QJsonObject page = pages.at(p).toObject();
            if (page.value(QStringLiteral("id")) != QJsonValue(QStringLiteral("pa.wattMeter"))) {
                continue;
            }
            QJsonArray sections = page.value(QStringLiteral("sections")).toArray();
            for (int s = 0; s < sections.size(); ++s) {
                QJsonObject section = sections.at(s).toObject();
                QJsonArray controls = section.value(QStringLiteral("controls")).toArray();
                for (int c = 0; c < controls.size(); ++c) {
                    QJsonObject control = controls.at(c).toObject();
                    if (!control.contains(QStringLiteral("rangeSource"))) { continue; }
                    const int point = control.value(QStringLiteral("binding")).toObject()
                        .value(QStringLiteral("radioSetting")).toString()
                        .mid(QStringLiteral("paCalibration/calPoint").size()).toInt();
                    const PaCalPointSpec spec = paCalPointSpec(boardClass, point);
                    if (boardClass == PaCalBoardClass::None || spec.maximum <= 0.0) {
                        controls.removeAt(c--);
                        continue;
                    }
                    const double factory = static_cast<double>(
                        defaults.watts[static_cast<std::size_t>(point)]);
                    control.remove(QStringLiteral("rangeSource"));
                    control.insert(QStringLiteral("label"), QStringLiteral("%1 W")
                        .arg(QString::number(factory, 'g', 4)));
                    control.insert(QStringLiteral("min"), 0);
                    control.insert(QStringLiteral("max"), spec.maximum);
                    control.insert(QStringLiteral("step"), spec.step);
                    control.insert(QStringLiteral("decimals"), spec.decimals);
                    control.insert(QStringLiteral("default"), factory);
                    control.insert(QStringLiteral("boardClass"), static_cast<int>(boardClass));
                    controls[c] = control;
                }
                if (controls.isEmpty()) { sections.removeAt(s--); continue; }
                section.insert(QStringLiteral("controls"), controls);
                sections[s] = section;
            }
            page.insert(QStringLiteral("sections"), sections);
            pages[p] = page;
        }
        root.insert(QStringLiteral("pages"), pages);
    }
    // Hardware version 17: 6m/ByPass on RX stays on a radio that has no
    // such bypass, disabled with the desktop's reason
    // (AntennaAlexAlex1Tab::applyLpfGates, codec::alex::lpfBypassAvailable).
    if (id == QLatin1String("hardware") && !codec::alex::lpfBypassAvailable(model)) {
        QJsonArray pages = root.value(QStringLiteral("pages")).toArray();
        for (int p = 0; p < pages.size(); ++p) {
            QJsonObject page = pages.at(p).toObject();
            QJsonArray sections = page.value(QStringLiteral("sections")).toArray();
            for (int s = 0; s < sections.size(); ++s) {
                QJsonObject section = sections.at(s).toObject();
                QJsonArray controls = section.value(QStringLiteral("controls")).toArray();
                for (int c = 0; c < controls.size(); ++c) {
                    QJsonObject control = controls.at(c).toObject();
                    if (control.value(QStringLiteral("id"))
                        != QJsonValue(QStringLiteral("hardware.alex1Filters.lpfBypass"))) {
                        continue;
                    }
                    control.insert(QStringLiteral("availability"), QJsonObject{
                        {QStringLiteral("enabled"), false},
                        {QStringLiteral("reason"), RadioModel::lpfBypassUnavailableReason()}});
                    controls[c] = control;
                }
                section.insert(QStringLiteral("controls"), controls);
                sections[s] = section;
            }
            page.insert(QStringLiteral("sections"), sections);
            pages[p] = page;
        }
        root.insert(QStringLiteral("pages"), pages);
    }
    // Transmit version 13: "Disable HF PA" stays on a radio that has no such
    // switch, disabled with the desktop's reason (PowerPage::applyHfPaGate,
    // RadioModel::hfPaSwitchAvailable).
    if (id == QLatin1String("transmit") && !RadioModel::hfPaSwitchAvailable(model)) {
        QJsonArray pages = root.value(QStringLiteral("pages")).toArray();
        for (int p = 0; p < pages.size(); ++p) {
            QJsonObject page = pages.at(p).toObject();
            QJsonArray sections = page.value(QStringLiteral("sections")).toArray();
            for (int s = 0; s < sections.size(); ++s) {
                QJsonObject section = sections.at(s).toObject();
                QJsonArray controls = section.value(QStringLiteral("controls")).toArray();
                for (int c = 0; c < controls.size(); ++c) {
                    QJsonObject control = controls.at(c).toObject();
                    if (control.value(QStringLiteral("id"))
                        != QJsonValue(QStringLiteral("transmit.power.DisableHfPa"))) {
                        continue;
                    }
                    control.insert(QStringLiteral("availability"), QJsonObject{
                        {QStringLiteral("enabled"), false},
                        {QStringLiteral("reason"), RadioModel::hfPaSwitchUnavailableReason()}});
                    controls[c] = control;
                }
                section.insert(QStringLiteral("controls"), controls);
                sections[s] = section;
            }
            page.insert(QStringLiteral("sections"), sections);
            pages[p] = page;
        }
        root.insert(QStringLiteral("pages"), pages);
    }
    // General > Options is the only Task-43 control with a board-dependent
    // range. The desktop reads this same BoardCapabilities row.
    if (id == QLatin1String("general")) {
        QJsonArray pages = root.value(QStringLiteral("pages")).toArray();
        for (int p = 0; p < pages.size(); ++p) {
            QJsonObject page = pages.at(p).toObject();
            QJsonArray sections = page.value(QStringLiteral("sections")).toArray();
            for (int s = 0; s < sections.size(); ++s) {
                QJsonObject section = sections.at(s).toObject();
                QJsonArray controls = section.value(QStringLiteral("controls")).toArray();
                bool removedSecondAdc = false;
                for (int c = 0; c < controls.size(); ++c) {
                    QJsonObject control = controls.at(c).toObject();
                    const QString boardGate = control.value(QStringLiteral("gate"))
                        .toObject().value(QStringLiteral("board")).toString();
                    if (boardGate == QLatin1String("attenuator.present")
                        && !caps.attenuator.present) {
                        controls.removeAt(c--);
                        continue;
                    }
                    // R-R3-46 / R-R3-11: RX2 Attenuation, the other receive
                    // ADC's own attenuator, only on a radio with a second
                    // receive ADC (the desktop row is disabled there).
                    if (boardGate == QLatin1String("attenuator.secondAdc")
                        && !(caps.attenuator.present && caps.adcCount >= 2)) {
                        controls.removeAt(c--);
                        removedSecondAdc = true;
                        continue;
                    }
                    if (control.value(QStringLiteral("rangeSource")).toString()
                        == QLatin1String("board.attenuator")) {
                        control.insert(QStringLiteral("min"), caps.attenuator.minDb);
                        control.insert(QStringLiteral("max"), caps.attenuator.maxDb);
                        control.insert(QStringLiteral("step"), caps.attenuator.stepDb);
                        control.remove(QStringLiteral("rangeSource"));
                        controls[c] = control;
                    }
                }
                // A section left empty by the second-ADC gate (RX2 Auto
                // Attenuate on a one-ADC radio) goes too.
                if (removedSecondAdc && controls.isEmpty()) {
                    sections.removeAt(s--);
                    continue;
                }
                section.insert(QStringLiteral("controls"), controls);
                sections[s] = section;
            }
            page.insert(QStringLiteral("sections"), sections);
            pages[p] = page;
        }
        root.insert(QStringLiteral("pages"), pages);
    }
    // Version 15: rows and sections that depend on the Core's radio.
    if (SetupDescriptionV15::isCategory(id)) {
        QJsonArray pages = root.value(QStringLiteral("pages")).toArray();
        for (int p = 0; p < pages.size(); ++p) {
            QJsonObject page = pages.at(p).toObject();
            QJsonArray sections = page.value(QStringLiteral("sections")).toArray();
            for (int s = 0; s < sections.size(); ++s) {
                QJsonObject section = sections.at(s).toObject();
                if (!SetupDescriptionV15::keepSectionForRadio(&section, caps)) {
                    sections.removeAt(s--);
                    continue;
                }
                QJsonArray controls = section.value(QStringLiteral("controls")).toArray();
                for (int c = 0; c < controls.size(); ++c) {
                    QJsonObject control = controls.at(c).toObject();
                    if (control.value(QStringLiteral("requiresDescriptionVersion"))
                        != QJsonValue(SetupDescriptionV15::kVersion)) {
                        continue;
                    }
                    if (!SetupDescriptionV15::projectForRadio(&control, caps, model)) {
                        controls.removeAt(c--);
                        continue;
                    }
                    controls[c] = control;
                }
                if (controls.isEmpty()) {
                    sections.removeAt(s--);
                    continue;
                }
                section.insert(QStringLiteral("controls"), controls);
                sections[s] = section;
            }
            if (sections.isEmpty()) {
                pages.removeAt(p--);
                continue;
            }
            page.insert(QStringLiteral("sections"), sections);
            pages[p] = page;
        }
        root.insert(QStringLiteral("pages"), pages);
    }
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

} // namespace

bool SetupDescription::validateSettingToggleEncoding(const QJsonObject& control)
{
    const QJsonObject binding = control.value(QStringLiteral("binding")).toObject();
    const bool toggle = control.value(QStringLiteral("kind")) == QJsonValue(QStringLiteral("toggle"));
    // Version 13: a radioSetting names a key under the connected radio's
    // hardware/<MAC>/, a Station key, stored the same way.
    const bool radioSetting = binding.contains(QStringLiteral("radioSetting"));
    const bool settingToggle = toggle && (binding.contains(QStringLiteral("setting")) || radioSetting);
    if (!settingToggle) {
        return !control.contains(QStringLiteral("valueEncoding"));
    }
    // SettingsProxyServer broadcasts the stored QString. These keys' readers
    // compare capitalized strings, so the wire value must use that spelling.
    const QJsonValue key = binding.value(radioSetting ? QStringLiteral("radioSetting")
                                                      : QStringLiteral("setting"));
    const QJsonValue encoding = control.value(QStringLiteral("valueEncoding"));
    if (binding.size() != 1 || !key.isString() || key.toString().isEmpty()
        || classifySettingsKey(radioSetting ? QStringLiteral("hardware/mac/") + key.toString()
                                            : key.toString()) != SettingsScope::Station
        || !encoding.isObject()) {
        return false;
    }
    const QJsonObject values = encoding.toObject();
    return values.size() == 2
        && values.value(QStringLiteral("true")) == QJsonValue(QStringLiteral("True"))
        && values.value(QStringLiteral("false")) == QJsonValue(QStringLiteral("False"));
}

bool SetupDescription::validateDisplaySettingBinding(const QJsonObject& control)
{
    struct Spec { const char* id; const char* key; const char* kind; bool tx; int version; };
    static constexpr Spec specs[] = {
        {"display.spectrumDefaults.fftSize", "DisplayFftSize", "slider", false, 4},
        {"display.spectrumDefaults.window", "DisplayFftWindow", "choice", false, 1},
        {"display.spectrumDefaults.hzPerBinTarget", "DisplayHzPerBinTarget", "decimal", false, 1},
        {"display.spectrumDefaults.fps", "DisplaySpectrumFps", "slider", false, 1},
        {"display.multimeter.pollingDelay", "MultimeterDelayMs", "integer", false, 1},
        {"display.txDisplay.fftSize", "DisplayTxFftSize", "slider", true, 4},
        {"display.txDisplay.window", "DisplayTxWindowType", "choice", true, 1},
        {"display.txDisplay.panDetector", "DisplayTxPanDetector", "choice", true, 1},
        {"display.txDisplay.panAveraging", "DisplayTxPanAveraging", "choice", true, 1},
        {"display.txDisplay.panAvTime", "DisplayTxPanAvTimeMs", "integer", true, 1},
        {"display.txDisplay.panNormalize", "DisplayTxPanNormalize", "toggle", true, 4},
        {"display.txDisplay.wfDetector", "DisplayTxWfDetector", "choice", true, 1},
        {"display.txDisplay.wfAveraging", "DisplayTxWfAveraging", "choice", true, 1},
        {"display.txDisplay.wfAvTime", "DisplayTxWfAvTimeMs", "integer", true, 1},
    };
    const QString id = control.value(QStringLiteral("id")).toString();
    const QJsonObject binding = control.value(QStringLiteral("binding")).toObject();
    const QJsonObject gate = control.value(QStringLiteral("gate")).toObject();
    for (const Spec& spec : specs) {
        if (id != QLatin1String(spec.id)) { continue; }
        if (binding.size() != 1 || binding.value(QStringLiteral("setting")) != QJsonValue(QLatin1String(spec.key))
            || classifySettingsKey(QString::fromLatin1(spec.key)) != SettingsScope::Station
            || control.value(QStringLiteral("kind")) != QJsonValue(QLatin1String(spec.kind))
            || control.value(QStringLiteral("applies"))
                != QJsonValue(spec.tx || id == QLatin1String("display.multimeter.pollingDelay")
                                  ? QStringLiteral("live") : QStringLiteral("subscription"))
            || (spec.tx ? gate != QJsonObject{{QStringLiteral("capability"), QStringLiteral("txDisplayVersion")},
                                              {QStringLiteral("min"), 2}} : !gate.isEmpty())
            || (spec.version == 4 ? control.value(QStringLiteral("requiresDescriptionVersion")) != QJsonValue(4)
                                  : control.contains(QStringLiteral("requiresDescriptionVersion")))) {
            return false;
        }
        if (id == QLatin1String("display.spectrumDefaults.fftSize")
            || id == QLatin1String("display.txDisplay.fftSize")) {
            const QJsonArray options = control.value(QStringLiteral("options")).toArray();
            if (options.size() != 7 || control.contains(QStringLiteral("min"))
                || control.contains(QStringLiteral("max")) || control.contains(QStringLiteral("step"))
                || control.contains(QStringLiteral("enabledWhen"))
                || control.value(QStringLiteral("default"))
                    != QJsonValue(spec.tx ? 32768 : 4096)) { return false; }
            for (int i = 0; i < 7; ++i) {
                const int value = 4096 << i;
                if (options.at(i) != QJsonValue(QJsonObject{
                        {QStringLiteral("value"), value},
                        {QStringLiteral("label"), QString::number(value)}})) { return false; }
            }
            return true;
        }
        if (id == QLatin1String("display.txDisplay.panNormalize")) {
            return !control.contains(QStringLiteral("options"))
                && control.value(QStringLiteral("default")) == QJsonValue(false)
                && control.value(QStringLiteral("enabledWhen")) == QJsonValue(QJsonObject{
                    {QStringLiteral("setting"), QStringLiteral("DisplayTxPanDetector")},
                    {QStringLiteral("oneOf"), QJsonArray{QStringLiteral("2"), QStringLiteral("3"), QStringLiteral("4")}}})
                && validateSettingToggleEncoding(control);
        }
        return !control.contains(QStringLiteral("options"))
            && !control.contains(QStringLiteral("enabledWhen"));
    }
    return false;
}

bool SetupDescription::validateDisplayPhoneBinding(const QJsonObject& control)
{
    // Version 12: the exact closed row, or nothing.
    const auto v12 = displayV12Controls().constFind(
        control.value(QStringLiteral("id")).toString());
    if (v12 != displayV12Controls().constEnd()) {
        return control == *v12;
    }
    struct RendererSpec {
        const char* id;
        const char* key;
        const char* label;
        const char* tooltip;
        const char* kind;
        const char* applies;
        int defaultValue;
        int minimum;
        int maximum;
        int step;
        const char* unit;
    };
    static constexpr RendererSpec rendererSpecs[] = {
        {"display.spectrumDefaults.panFill", "DisplayPanFill", "Fill under trace",
         "Check to fill the panadapter display line below the data.", "toggle", "live", 1, 0, 0, 0, ""},
        {"display.spectrumDefaults.fillAlpha", "DisplayFftFillAlpha", "Fill Alpha:",
         "Opacity of the fill area under the spectrum trace (0 = transparent, 100 = opaque).",
         "slider", "live", 70, 0, 100, 1, "%"},
        {"display.spectrumDefaults.gradient", "DisplayGradientEnabled", "Trace gradient",
         "When checked, the spectrum trace line renders with the gradient color applied.",
         "toggle", "live", 0, 0, 0, 0, ""},
        {"display.spectrumDefaults.peakHold", "DisplayPeakHoldEnabled", "Peak hold",
         "When enabled, the highest signal level seen at each frequency bin is held on the display.",
         "toggle", "live", 0, 0, 0, 0, ""},
        {"display.spectrumDefaults.peakDelay", "DisplayPeakHoldResetMs", "Peak Delay:",
         "Time in milliseconds before a held peak begins to decay back toward the live trace.",
         "integer", "live", 2000, 100, 10000, 100, "ms"},
        {"display.waterfallDefaults.updatePeriod", "DisplayWfUpdatePeriodMs", "Update Period:",
         "How often to update (scroll another pixel line) on the waterfall display.  Note that this is tamed by the FPS setting.",
         "slider", "subscription", 30, 10, 500, 1, "ms"},
        {"display.waterfallDefaults.stopOnTx", "WaterfallStopOnTx", "Stop on TX",
         "Pause the waterfall while transmitting. Resumes automatically when TX ends.",
         "toggle", "live", 0, 0, 0, 0, ""},
        {"display.waterfallDefaults.opacity", "DisplayWfOpacity", "Opacity:",
         "Waterfall opacity (0 = fully transparent, 100 = fully opaque). Blends the waterfall over the spectrum background.",
         "slider", "live", 100, 0, 100, 1, "%"},
        {"display.waterfallDefaults.showRxFilter", "DisplayShowRxFilterOnWaterfall",
         "Show RX filter on waterfall",
         "Overlay the current RX passband filter boundaries on the waterfall display.",
         "toggle", "live", 0, 0, 0, 0, ""},
        {"display.waterfallDefaults.showTxFilter", "DisplayShowTxFilterOnRxWaterfall",
         "Show TX filter on RX waterfall",
         "Overlay the TX passband filter boundaries on the RX waterfall display.",
         "toggle", "live", 1, 0, 0, 0, ""},
        {"display.waterfallDefaults.showRxZeroLine", "DisplayShowRxZeroLine",
         "Show RX zero line on waterfall",
         "Draw a line on the waterfall at the RX center frequency (zero-beat reference).",
         "toggle", "live", 0, 0, 0, 0, ""},
        {"display.waterfallDefaults.showTxZeroLine", "DisplayShowTxZeroLine",
         "Show TX zero line on waterfall",
         "Draw a line on the waterfall at the TX center frequency (zero-beat reference).",
         "toggle", "live", 0, 0, 0, 0, ""},
    };
    for (const RendererSpec& spec : rendererSpecs) {
        if (control.value(QStringLiteral("id")) != QJsonValue(QLatin1String(spec.id))) {
            continue;
        }
        const bool toggle = QLatin1String(spec.kind) == QLatin1String("toggle");
        const int requiredVersion = QLatin1String(spec.id).startsWith(
            QLatin1String("display.waterfallDefaults.show")) ? 10 : 9;
        if (control.size() != (toggle ? 8 : 12)
            || control.value(QStringLiteral("binding")) != QJsonValue(QJsonObject{
                   {QStringLiteral("phone"), QLatin1String(spec.key)}})
            || control.value(QStringLiteral("label")) != QJsonValue(QLatin1String(spec.label))
            || control.value(QStringLiteral("tooltip")) != QJsonValue(QLatin1String(spec.tooltip))
            || control.value(QStringLiteral("kind")) != QJsonValue(QLatin1String(spec.kind))
            || control.value(QStringLiteral("applies")) != QJsonValue(QLatin1String(spec.applies))
            || control.value(QStringLiteral("requiresDescriptionVersion")) != QJsonValue(requiredVersion)
            || control.value(QStringLiteral("default"))
                != (toggle ? QJsonValue(spec.defaultValue != 0) : QJsonValue(spec.defaultValue))) {
            return false;
        }
        return toggle || (control.value(QStringLiteral("min")) == QJsonValue(spec.minimum)
            && control.value(QStringLiteral("max")) == QJsonValue(spec.maximum)
            && control.value(QStringLiteral("step")) == QJsonValue(spec.step)
            && control.value(QStringLiteral("unit")) == QJsonValue(QLatin1String(spec.unit)));
    }
    // Version 11: Spectrum Peaks, in the native page's order. The desktop
    // draws the peak hold trace and the blobs from the frames it already has
    // (SpectrumWidget::updateReducedSpectrumOverlays); the phone asks the
    // Core for the same computation through its display extras
    // (peakBlobs / activePeakHold), so every row needs displayExtrasVersion 1;
    // the peak hold's Hold duration and Update during TX need 3, the Core
    // that honours holdMs and activePeakHold.onTx.
    struct PeaksSpec {
        const char* id;
        const char* key;
        const char* label;
        const char* tooltip;
        const char* kind;
        const char* applies;
        int defaultValue;
        const char* defaultColour;
        int minimum;
        int maximum;
        int step;
        const char* unit;
        int extrasVersion;
    };
    static constexpr PeaksSpec peaksSpecs[] = {
        {"display.spectrumPeaks.activePeakHold", "DisplayActivePeakHoldEnabled",
         "Enable per-bin peak trace with decay",
         "Display a secondary trace of the highest recent level at each frequency bin. "
         "Each bin holds its peak for the hold duration after it was last raised, then falls "
         "at the drop rate.",
         "toggle", "subscription", 0, "", 0, 0, 0, "", 1},
        {"display.spectrumPeaks.activePeakHoldTime", "DisplayActivePeakHoldDurationMs",
         "Hold duration:",
         "How long (ms) a peak bin is held at its maximum before starting to decay.",
         "integer", "subscription", 2000, "", 100, 60000, 100, "ms", 3},
        {"display.spectrumPeaks.activePeakHoldDropRate", "DisplayActivePeakHoldDropDbPerSec",
         "Drop rate:", "Rate at which a held peak falls once its hold duration has passed.",
         "integer", "subscription", 6, "", 1, 60, 1, "dB/s", 1},
        {"display.spectrumPeaks.activePeakHoldFill", "DisplayActivePeakHoldFill",
         "Fill area between peak trace and current trace",
         "Shade the region between the live spectrum and the peak-hold trace.",
         "toggle", "live", 0, "", 0, 0, 0, "", 1},
        {"display.spectrumPeaks.activePeakHoldOnTx", "DisplayActivePeakHoldOnTx",
         "Update during TX",
         "Keep the peak trace running while this panadapter transmits. "
         "When off, the trace is hidden and paused until transmit ends.",
         "toggle", "subscription", 0, "", 0, 0, 0, "", 3},
        {"display.spectrumPeaks.activePeakHoldColor", "DisplayActivePeakHoldColor",
         "Trace color:",
         "Color of the dashed Active Peak Hold trace. Set this to a hue different from the "
         "live data-line color so the peak trace stays visible (e.g. after Reset to Smooth "
         "Defaults paints the live trace white).",
         "colour", "live", 0, "#FFD700FF", 0, 0, 0, "", 1},
        {"display.spectrumPeaks.peakBlobs", "DisplayPeakBlobsEnabled",
         "Show top-N peak markers",
         "Display small circle markers at the top-N highest signal peaks in the spectrum.",
         "toggle", "subscription", 0, "", 0, 0, 0, "", 1},
        {"display.spectrumPeaks.peakBlobCount", "DisplayPeakBlobsCount",
         "Number of peaks:", "Number of peak markers to display (1 to 20).",
         "integer", "subscription", 3, "", 1, 20, 1, "", 1},
        {"display.spectrumPeaks.peakBlobInsideFilter", "DisplayPeakBlobsInsideFilterOnly",
         "Only show peaks inside the RX filter passband",
         "Restrict peak blobs to frequencies within the current RX filter passband.",
         "toggle", "subscription", 0, "", 0, 0, 0, "", 1},
        {"display.spectrumPeaks.peakBlobHold", "DisplayPeakBlobsHoldEnabled",
         "Hold peaks before decay",
         "Keep each blob at its peak position for the hold duration before falling.",
         "toggle", "subscription", 0, "", 0, 0, 0, "", 1},
        {"display.spectrumPeaks.peakBlobHoldTime", "DisplayPeakBlobsHoldMs",
         "Hold duration:", "How long (ms) a blob is held at its peak before falling.",
         "integer", "subscription", 500, "", 100, 60000, 100, "ms", 1},
        {"display.spectrumPeaks.peakBlobHoldDrop", "DisplayPeakBlobsHoldDrop",
         "Decay after hold (off = hard cut)",
         "When on, blobs decay at the fall rate after the hold. "
         "When off, blobs disappear instantly after the hold duration.",
         "toggle", "subscription", 0, "", 0, 0, 0, "", 1},
        {"display.spectrumPeaks.peakBlobFallRate", "DisplayPeakBlobsFallDbPerSec",
         "Fall rate:", "Rate at which blobs fall after the hold duration.",
         "integer", "subscription", 6, "", 1, 60, 1, "dB/s", 1},
        {"display.spectrumPeaks.peakBlobColor", "DisplayPeakBlobColor",
         "Blob color:", "Color of the peak blob circles.",
         "colour", "live", 0, "#FF4500FF", 0, 0, 0, "", 1},
        {"display.spectrumPeaks.peakBlobTextColor", "DisplayPeakBlobTextColor",
         "Text color:", "Color of the dBm readout text on each peak blob.",
         "colour", "live", 0, "#7FFF00FF", 0, 0, 0, "", 1},
    };
    for (const PeaksSpec& spec : peaksSpecs) {
        if (control.value(QStringLiteral("id")) != QJsonValue(QLatin1String(spec.id))) {
            continue;
        }
        const QLatin1String kind(spec.kind);
        const bool integer = kind == QLatin1String("integer");
        const bool hasUnit = spec.unit[0] != '\0';
        const QJsonValue expectedDefault = kind == QLatin1String("toggle")
            ? QJsonValue(spec.defaultValue != 0)
            : kind == QLatin1String("colour") ? QJsonValue(QLatin1String(spec.defaultColour))
                                              : QJsonValue(spec.defaultValue);
        if (control.size() != (integer ? (hasUnit ? 13 : 12) : 9)
            || control.value(QStringLiteral("binding")) != QJsonValue(QJsonObject{
                   {QStringLiteral("phone"), QLatin1String(spec.key)}})
            || control.value(QStringLiteral("label")) != QJsonValue(QLatin1String(spec.label))
            || control.value(QStringLiteral("tooltip")) != QJsonValue(QLatin1String(spec.tooltip))
            || control.value(QStringLiteral("kind")) != QJsonValue(kind)
            || control.value(QStringLiteral("applies")) != QJsonValue(QLatin1String(spec.applies))
            || control.value(QStringLiteral("requiresDescriptionVersion")) != QJsonValue(11)
            || control.value(QStringLiteral("gate")) != QJsonValue(QJsonObject{
                   {QStringLiteral("capability"), QStringLiteral("displayExtrasVersion")},
                   {QStringLiteral("min"), spec.extrasVersion}})
            || control.value(QStringLiteral("default")) != expectedDefault) {
            return false;
        }
        return !integer || (control.value(QStringLiteral("min")) == QJsonValue(spec.minimum)
            && control.value(QStringLiteral("max")) == QJsonValue(spec.maximum)
            && control.value(QStringLiteral("step")) == QJsonValue(spec.step)
            && (!hasUnit || control.value(QStringLiteral("unit")) == QJsonValue(QLatin1String(spec.unit))));
    }
    struct Spec {
        const char* id;
        const char* phone;
        const char* kind;
        const char* capability;
        int capabilityVersion;
        int defaultValue;
        int minimum;
        int maximum;
        int step;
        const char* unit;
        QStringList choices;
    };
    static const Spec specs[] = {
        {"display.spectrumDefaults.detector", "DisplaySpectrumDetector", "choice",
         "remoteMediaVersion", 1, ControlRanges::kDisplaySpectrumDetectorDefault, 0, 0, 0, "",
         {"Peak", "Rosenfell", "Average", "Sample", "RMS"}},
        {"display.spectrumDefaults.averaging", "DisplaySpectrumAveraging", "choice",
         "displayExtrasVersion", 1, ControlRanges::kDisplaySpectrumAveragingDefault, 0, 0, 0, "",
         {"None", "Recursive", "Time Window", "Log Recursive"}},
        {"display.spectrumDefaults.averageTime", "DisplaySpectrumAverageTimeMs", "integer",
         "displayExtrasVersion", 1, ControlRanges::kDisplaySpectrumAvgTimeDefaultMs,
         ControlRanges::kDisplayAvgTimeMinMs, ControlRanges::kDisplayAvgTimeMaxMs,
         ControlRanges::kDisplayAvgTimeStepMs, "ms", {}},
        {"display.spectrumDefaults.decimation", "decimation", "integer",
         "spectrumGrantVersion", 2, ControlRanges::kDisplayDecimationDefault,
         ControlRanges::kDisplayDecimationMin, ControlRanges::kDisplayDecimationMax,
         ControlRanges::kDisplayDecimationStep, "", {}},
        {"display.waterfallDefaults.detector", "DisplayWaterfallDetector", "choice",
         "remoteMediaVersion", 1, ControlRanges::kDisplayWaterfallDetectorDefault, 0, 0, 0, "",
         {"Peak", "Rosenfell", "Average", "Sample"}},
        {"display.waterfallDefaults.averaging", "DisplayWaterfallAveraging", "choice",
         "displayExtrasVersion", 1, ControlRanges::kDisplayWaterfallAveragingDefault, 0, 0, 0, "",
         {"None", "Recursive", "Time Window", "Log Recursive"}},
        {"display.waterfallDefaults.averageTime", "DisplayWaterfallAverageTimeMs", "integer",
         "displayExtrasVersion", 1, ControlRanges::kDisplayWaterfallAvgTimeDefaultMs,
         ControlRanges::kDisplayAvgTimeMinMs, ControlRanges::kDisplayAvgTimeMaxMs,
         ControlRanges::kDisplayAvgTimeStepMs, "ms", {}}
    };
    const QJsonObject binding = control.value(QStringLiteral("binding")).toObject();
    for (const Spec& spec : specs) {
        if (control.value(QStringLiteral("id")) != QJsonValue(QLatin1String(spec.id))) {
            continue;
        }
        if (binding != QJsonObject{{QStringLiteral("phone"), QLatin1String(spec.phone)}}
            || control.value(QStringLiteral("kind")) != QJsonValue(QLatin1String(spec.kind))
            || control.value(QStringLiteral("applies")) != QJsonValue(QStringLiteral("subscription"))
            || control.value(QStringLiteral("requiresDescriptionVersion")) != QJsonValue(8)
            || control.value(QStringLiteral("gate")) != QJsonValue(QJsonObject{
                   {QStringLiteral("capability"), QLatin1String(spec.capability)},
                   {QStringLiteral("min"), spec.capabilityVersion}})
            || control.value(QStringLiteral("default")) != QJsonValue(spec.defaultValue)
            || !control.value(QStringLiteral("label")).isString()
            || control.value(QStringLiteral("label")).toString().isEmpty()
            || !control.value(QStringLiteral("tooltip")).isString()) {
            return false;
        }
        if (!spec.choices.isEmpty()) {
            const QJsonArray options = control.value(QStringLiteral("options")).toArray();
            if (control.size() != 10 || options.size() != spec.choices.size()) { return false; }
            for (int i = 0; i < options.size(); ++i) {
                if (options.at(i) != QJsonValue(QJsonObject{
                        {QStringLiteral("value"), i},
                        {QStringLiteral("label"), spec.choices.at(i)}})) { return false; }
            }
            return true;
        }
        const bool hasUnit = spec.unit[0] != '\0';
        return control.size() == (hasUnit ? 13 : 12)
            && control.value(QStringLiteral("min")) == QJsonValue(spec.minimum)
            && control.value(QStringLiteral("max")) == QJsonValue(spec.maximum)
            && control.value(QStringLiteral("step")) == QJsonValue(spec.step)
            && (!hasUnit || control.value(QStringLiteral("unit")) == QJsonValue(QLatin1String(spec.unit)));
    }
    return false;
}

bool SetupDescription::validateAppearanceColourBinding(const QJsonObject& control)
{
    struct Swatch { const char* id; const char* phoneKey; const char* defaultRgba; };
    static constexpr Swatch swatches[] = {
        {"traceFillColor", "DisplayFillColor", "#00E5FFFF"},
        {"gridColor", "DisplayGridColor", "#FFFFFF28"},
        {"gridFineColor", "DisplayGridFineColor", "#FFFFFF14"},
        {"hGridColor", "DisplayHGridColor", "#FFFFFF28"},
        {"gridTextColor", "DisplayGridTextColor", "#FFFF00FF"},
        {"bandEdgeColor", "DisplayBandEdgeColor", "#FF0000FF"},
        {"rxZeroLineColor", "DisplayRxZeroLineColor", "#FF0000FF"},
        {"txZeroLineColor", "DisplayTxZeroLineColor", "#FFB800FF"},
        {"rxFilterColor", "DisplayRxFilterColor", "#00B4D850"},
        {"txFilterColor", "DisplayTxFilterColor", "#FF783C2E"},
    };
    const QString id = control.value(QStringLiteral("id")).toString();
    const QJsonObject binding = control.value(QStringLiteral("binding")).toObject();
    const QString defaultColour = control.value(QStringLiteral("default")).toString();
    if (control.size() != 7 || control.value(QStringLiteral("kind")) != QJsonValue(QStringLiteral("colour"))
        || control.value(QStringLiteral("applies")) != QJsonValue(QStringLiteral("live"))
        || control.value(QStringLiteral("label")).toString().isEmpty()
        || !control.value(QStringLiteral("tooltip")).isString()
        || binding.size() != 1 || !binding.value(QStringLiteral("phone")).isString()
        || defaultColour.size() != 9 || defaultColour.front() != QLatin1Char('#')) { return false; }
    for (const QChar digit : defaultColour.sliced(1)) {
        if (!digit.isDigit() && (digit < QLatin1Char('A') || digit > QLatin1Char('F'))) {
            return false;
        }
    }
    for (const Swatch& swatch : swatches) {
        if (id == QStringLiteral("appearance.colorsTheme.") + QLatin1String(swatch.id)) {
            return binding.value(QStringLiteral("phone")) == QJsonValue(QLatin1String(swatch.phoneKey))
                && defaultColour == QLatin1String(swatch.defaultRgba);
        }
    }
    return false;
}

bool SetupDescription::validateAppearanceResetColours(const QJsonObject& control)
{
    static const QJsonObject expected =
        QJsonDocument::fromJson(QByteArray(kAppearanceV12ResetColours)).object();
    return !expected.isEmpty() && control == expected;
}

bool SetupDescription::validateAppearanceMeterStyleBinding(const QJsonObject& control)
{
    struct StyleControl {
        const char* id;
        const char* phoneKey;
        const char* label;
        const char* tooltip;
        const char* kind;
        int defaultChoice;
        QStringList options;
    };
    static const StyleControl styles[] = {
        {"face", "SMeter_FaceStyle", "Face:", "The S-meter's face", "choice", 0,
         {"Aged Cream", "VU Amber", "Collins White", "Blackface", "Carbon", "Ice", "Classic (flat)"}},
        {"peakHold", "PeakHoldEnabled", "Peak hold", "Hold the S-meter's peak reading", "toggle", 0, {}},
        {"peakDecay", "PeakDecayRate", "Decay Rate:", "How fast the held peak falls back", "choice", 1,
         {"Fast (20 dB/s)", "Medium (10 dB/s)", "Slow (5 dB/s)"}},
    };
    const QJsonObject binding = control.value(QStringLiteral("binding")).toObject();
    if (binding.size() != 1
        || control.value(QStringLiteral("applies")) != QJsonValue(QStringLiteral("live"))
        || control.value(QStringLiteral("requiresDescriptionVersion")) != QJsonValue(7)) {
        return false;
    }
    for (const StyleControl& style : styles) {
        if (control.value(QStringLiteral("id"))
            != QJsonValue(QStringLiteral("appearance.meterStyles.") + QLatin1String(style.id))) {
            continue;
        }
        if (control.value(QStringLiteral("label")) != QJsonValue(QLatin1String(style.label))
            || control.value(QStringLiteral("tooltip")) != QJsonValue(QLatin1String(style.tooltip))
            || control.value(QStringLiteral("kind")) != QJsonValue(QLatin1String(style.kind))
            || binding.value(QStringLiteral("phone")) != QJsonValue(QLatin1String(style.phoneKey))) {
            return false;
        }
        if (style.options.isEmpty()) {
            return control.size() == 8 && !control.contains(QStringLiteral("options"))
                && control.value(QStringLiteral("default")) == QJsonValue(true);
        }
        const QJsonArray options = control.value(QStringLiteral("options")).toArray();
        if (control.size() != 9 || options.size() != style.options.size()
            || control.value(QStringLiteral("default")) != QJsonValue(style.defaultChoice)) {
            return false;
        }
        int index = 0;
        for (const QString& label : style.options) {
            const QJsonObject option = options.at(index).toObject();
            if (option.size() != 2 || option.value(QStringLiteral("value")) != QJsonValue(index)
                || option.value(QStringLiteral("label")) != QJsonValue(label)) {
                return false;
            }
            ++index;
        }
        return true;
    }
    return false;
}

bool SetupDescription::validateActiveSlicePropertyBinding(const QJsonObject& control)
{
    const QJsonObject binding = control.value(QStringLiteral("binding")).toObject();
    if (binding.size() != 1 || !binding.value(QStringLiteral("property")).isObject()) {
        return false;
    }
    const QJsonObject ref = binding.value(QStringLiteral("property")).toObject();
    if (ref.size() != 2) {
        return false;
    }
    const QString object = ref.value(QStringLiteral("object")).toString();
    const QMetaObject* meta = object == QLatin1String("slice:active")
        ? &SliceModel::staticMetaObject
        : object == QLatin1String("transmit")
          ? &TransmitModel::staticMetaObject
          : object == QLatin1String("notches")
            ? &NotchModel::staticMetaObject : nullptr;
    const QByteArray policyClass = object == QLatin1String("slice:active")
        ? QByteArrayLiteral("SliceModel")
        : object == QLatin1String("transmit")
          ? QByteArrayLiteral("TransmitModel") : QByteArrayLiteral("NotchModel");
    if (!meta) {
        return false;
    }
    const QByteArray name = ref.value(QStringLiteral("name")).toString().toUtf8();
    const MirrorProperty* property = MirrorSchema::forMetaObject(meta).byName(name);
    if (!property) {
        return false;
    }
    if (object == QLatin1String("transmit")) {
        // DSP's transmit processing rows (TX Leveler, TX ALC, Phase Rotator,
        // CFC, CESSB) carry no off-air rule: this Core takes transmit
        // settings on the air (transmitSettingsVersion 13 or later) from a
        // session permitted to change them, as the desktop does.
        const QJsonObject gate = control.value(QStringLiteral("gate")).toObject();
        if (gate.contains(QStringLiteral("offAir"))
            || gate.value(QStringLiteral("capability")) != QJsonValue(QStringLiteral("transmitSettingsVersion"))
            || gate.value(QStringLiteral("min")).toInt() < 4) {
            return false;
        }
    }
    const QString kind = control.value(QStringLiteral("kind")).toString();
    const QJsonObject gate = control.value(QStringLiteral("gate")).toObject();
    if (kind == QLatin1String("readout")) {
        return object == QLatin1String("slice:active")
            && name == QByteArrayLiteral("minNotchWidthHz")
            && gate.value(QStringLiteral("capability")) == QJsonValue(QStringLiteral("dspInfoVersion"))
            && gate.value(QStringLiteral("min")).toInt() >= 1
            && property->kind == MirrorWireKind::Float64
            && MirrorPolicy::hasExplicitEntry(policyClass, name)
            && MirrorPolicy::directionFor(policyClass, name) == MirrorDirection::Outbound;
    }
    if (!property->isWritable || !MirrorPolicy::inboundAllowed(policyClass, name)) {
        return false;
    }
    if (object == QLatin1String("notches")
        && (name != QByteArrayLiteral("autoIncrease")
            || gate.value(QStringLiteral("capability")) != QJsonValue(QStringLiteral("notchControlVersion"))
            || gate.value(QStringLiteral("min")).toInt() < 1)) {
        return false;
    }
    const MirrorWireKind expected = kind == QLatin1String("toggle") ? MirrorWireKind::Bool
        : kind == QLatin1String("decimal") ? MirrorWireKind::Float64
        : kind == QLatin1String("choice") ? MirrorWireKind::Enum
        : kind == QLatin1String("integer") || kind == QLatin1String("slider")
          ? MirrorWireKind::Int64 : MirrorWireKind::Unsupported;
    return property->kind == expected
        || (kind == QLatin1String("choice") && property->kind == MirrorWireKind::Int64);
}

bool SetupDescription::validateTransmitPropertyBinding(const QJsonObject& control)
{
    const QJsonObject binding = control.value(QStringLiteral("binding")).toObject();
    if (binding.size() != 1 || !binding.value(QStringLiteral("property")).isObject()) {
        return false;
    }
    const QJsonObject ref = binding.value(QStringLiteral("property")).toObject();
    if (ref.size() != 2 || ref.value(QStringLiteral("object")) != QJsonValue(QStringLiteral("transmit"))) {
        return false;
    }
    const QByteArray name = ref.value(QStringLiteral("name")).toString().toUtf8();
    static const QSet<QByteArray> kDexpSettings{
        "dexpEnabled", "dexpAttackTimeMs", "voxHangTimeMs", "dexpReleaseTimeMs",
        "voxThresholdDb", "dexpExpansionRatioDb", "dexpHysteresisRatioDb",
        "dexpDetectorTauMs", "dexpLookAheadEnabled", "dexpLookAheadMs",
        "dexpSideChannelFilterEnabled", "dexpLowCutHz", "dexpHighCutHz",
        "antiVoxRun", "antiVoxGainDb", "antiVoxTauMs"};
    const QJsonObject gate = control.value(QStringLiteral("gate")).toObject();
    if ((name != QByteArrayLiteral("power") && !kDexpSettings.contains(name))
        || gate.value(QStringLiteral("transmit")) != QJsonValue(true)
        || gate.value(QStringLiteral("capability")) != QJsonValue(QStringLiteral("transmitSettingsVersion"))
        || gate.value(QStringLiteral("min")).toInt() < 5
        // The Core takes these on the air (transmitSettingsVersion 13) and
        // Thetis disables none of them during MOX (setup.cs:5132-5161
        // [v2.10.3.15]), so they carry no offAir rule.
        || gate.contains(QStringLiteral("offAir"))) {
        return false;
    }
    const MirrorProperty* property = MirrorSchema::forMetaObject(&TransmitModel::staticMetaObject).byName(name);
    if (!property || !property->isWritable
        || !MirrorPolicy::inboundAllowed(QByteArrayLiteral("TransmitModel"), name)) {
        return false;
    }
    const QString kind = control.value(QStringLiteral("kind")).toString();
    const MirrorWireKind expected = kind == QLatin1String("toggle") ? MirrorWireKind::Bool
        : kind == QLatin1String("integer") ? MirrorWireKind::Int64
        : kind == QLatin1String("slider") ? MirrorWireKind::Int64
        : kind == QLatin1String("decimal") ? MirrorWireKind::Float64
        : MirrorWireKind::Unsupported;
    return expected != MirrorWireKind::Unsupported && property->kind == expected;
}

bool SetupDescription::validateHardwarePropertyBinding(const QJsonObject& control, HPSDRModel model)
{
    // Closed scalar set: source binding, safety gate, and projected SKU text
    // must agree before a description can be published.
    const QJsonObject binding = control.value(QStringLiteral("binding")).toObject();
    if (binding.size() != 1 || !binding.value(QStringLiteral("property")).isObject()
        || control.size() != 7
        || control.value(QStringLiteral("kind")) != QJsonValue(QStringLiteral("toggle"))
        || control.value(QStringLiteral("applies")) != QJsonValue(QStringLiteral("live"))) {
        return false;
    }
    const QJsonObject ref = binding.value(QStringLiteral("property")).toObject();
    if (ref.size() != 2
        || ref.value(QStringLiteral("object")) != QJsonValue(QStringLiteral("alexAntennas"))) {
        return false;
    }
    const QByteArray name = ref.value(QStringLiteral("name")).toString().toUtf8();
    const bool receive = name == QByteArrayLiteral("useTxAntennaForRx");
    const bool block2 = name == QByteArrayLiteral("blockTxAnt2");
    const bool block3 = name == QByteArrayLiteral("blockTxAnt3");
    const bool rxOut = name == QByteArrayLiteral("rxOutOnTx");
    const bool ext1 = name == QByteArrayLiteral("ext1OutOnTx");
    const bool ext2 = name == QByteArrayLiteral("ext2OutOnTx");
    const bool override = name == QByteArrayLiteral("rxOutOverride");
    if (!receive && !block2 && !block3 && !rxOut && !ext1 && !ext2 && !override) {
        return false;
    }
    const SkuUiProfile sku = skuUiProfileFor(model);
    if (model != HPSDRModel::FIRST
        && ((rxOut && !sku.hasRxOutOnTx) || (ext1 && !sku.hasExt1OutOnTx)
            || (ext2 && !sku.hasExt2OutOnTx) || (override && !sku.hasRxBypassUi))) {
        return false;
    }
    const QString id = QStringLiteral("hardware.antennaAlex.") + QString::fromUtf8(name);
    const QString label = receive ? QStringLiteral("Use TX antenna for RX")
        : block2 ? QStringLiteral("Block TX on Ant 2")
        : block3 ? QStringLiteral("Block TX on Ant 3")
        : rxOut ? QStringLiteral("RX Bypass on TX")
        : ext1 ? sku.ext1OutOnTxLabel
        : ext2 ? sku.ext2OutOnTxLabel
               : QStringLiteral("Disable RX Bypass relay");
    const QString tooltip = receive
        ? QStringLiteral("Use the TX antenna for RX instead of the RX antenna.")
        : block2
          ? QStringLiteral("Prevents transmit assignments to Antenna Port 2. Use when Ant 2 is wired for receive only.")
        : block3 ? QStringLiteral("Prevents transmit assignments to Antenna Port 3. Use when Ant 3 is wired for receive only.")
        : rxOut ? QStringLiteral("Enable RX Bypass Out relay on transmit.")
        : ext1 ? QStringLiteral("Route Ext 1 to receive path during transmit.")
        : ext2 ? sku.ext2OutOnTxTooltip
               // The Thetis control for this relay is chkDisableRXOut.
               : QStringLiteral("Disable the RX Bypass Out relay.");
    if (control.value(QStringLiteral("id")) != QJsonValue(id)
        || control.value(QStringLiteral("label")) != QJsonValue(label)
        || control.value(QStringLiteral("tooltip")) != QJsonValue(tooltip)) {
        return false;
    }
    // hasAlexFilters is projected by this service, not repeated as a client
    // gate: that BoardCapabilities flag is absent from the station catalogue.
    QJsonObject expectedGate{{QStringLiteral("capability"), QStringLiteral("radioHardwareVersion")},
                             {QStringLiteral("min"), receive ? 2 : rxOut ? 5 : 6}};
    if (!receive) {
        expectedGate.insert(QStringLiteral("offAir"), true);
    }
    if (control.value(QStringLiteral("gate")).toObject() != expectedGate) {
        return false;
    }
    const MirrorProperty* property = MirrorSchema::forMetaObject(
        &AlexAntennaFacade::staticMetaObject).byName(name);
    return property && property->isWritable && property->kind == MirrorWireKind::Bool
        && MirrorPolicy::inboundAllowed(QByteArrayLiteral("AlexAntennaFacade"), name);
}

bool SetupDescription::validatePaReadoutBinding(const QJsonObject& control)
{
    const QJsonObject binding = control.value(QStringLiteral("binding")).toObject();
    const QJsonObject ref = binding.value(QStringLiteral("property")).toObject();
    const QJsonObject gate = control.value(QStringLiteral("gate")).toObject();
    if (control.size() != 9 || binding.size() != 1 || ref.size() != 2
        || ref.value(QStringLiteral("object")) != QJsonValue(QStringLiteral("txState"))
        || control.value(QStringLiteral("kind")) != QJsonValue(QStringLiteral("readout"))
        || control.value(QStringLiteral("applies")) != QJsonValue(QStringLiteral("live"))) {
        return false;
    }
    const QByteArray name = ref.value(QStringLiteral("name")).toString().toUtf8();
    const bool derivedPower = name == QByteArrayLiteral("forwardRawPowerWatts");
    const bool volts = name == QByteArrayLiteral("forwardAdcVolts")
        || name == QByteArrayLiteral("reflectedAdcVolts");
    const bool derived = derivedPower || volts;
    if (gate != QJsonObject{{QStringLiteral("capability"), QStringLiteral("txReadingsVersion")},
                            {QStringLiteral("min"), derived ? 2 : 1}}) {
        return false;
    }
    const bool power = name == QByteArrayLiteral("forwardPowerWatts")
        || name == QByteArrayLiteral("reflectedPowerWatts");
    const bool swr = name == QByteArrayLiteral("swr");
    const bool raw = name == QByteArrayLiteral("forwardAdcRaw")
        || name == QByteArrayLiteral("reflectedAdcRaw");
    if (!power && !swr && !raw && !derived) { return false; }
    const QString id = name == QByteArrayLiteral("forwardPowerWatts")
        ? QStringLiteral("pa.values.forwardCalibrated")
        : derivedPower ? QStringLiteral("pa.values.forwardRawPower")
        : name == QByteArrayLiteral("reflectedPowerWatts")
          ? QStringLiteral("pa.values.reflectedPower")
        : swr ? QStringLiteral("pa.values.swr")
        : name == QByteArrayLiteral("forwardAdcVolts") ? QStringLiteral("pa.values.forwardVoltage")
        : name == QByteArrayLiteral("reflectedAdcVolts") ? QStringLiteral("pa.values.reflectedVoltage")
        : name == QByteArrayLiteral("forwardAdcRaw")
          ? QStringLiteral("pa.values.forwardAdc") : QStringLiteral("pa.values.reflectedAdc");
    const QString label = name == QByteArrayLiteral("forwardPowerWatts")
        ? QStringLiteral("Forward (calibrated):")
        : derivedPower ? QStringLiteral("Forward (raw):")
        : name == QByteArrayLiteral("reflectedPowerWatts") ? QStringLiteral("Reflected:")
        : swr ? QStringLiteral("SWR:")
        : name == QByteArrayLiteral("forwardAdcVolts") ? QStringLiteral("FWD Voltage:")
        : name == QByteArrayLiteral("reflectedAdcVolts") ? QStringLiteral("REV Voltage:")
        : name == QByteArrayLiteral("forwardAdcRaw") ? QStringLiteral("FWD ADC:")
                                                       : QStringLiteral("REV ADC:");
    const QJsonValue decimals = control.value(QStringLiteral("decimals"));
    if (!decimals.isDouble() || std::floor(decimals.toDouble()) != decimals.toDouble()
        || decimals.toInt(-1) != (raw ? 0 : 2)
        || control.value(QStringLiteral("id")) != QJsonValue(id)
        || control.value(QStringLiteral("label")) != QJsonValue(label)
        || control.value(QStringLiteral("tooltip")) != QJsonValue(QString())
        || control.value(QStringLiteral("unit")) != QJsonValue(
               power || derivedPower ? QStringLiteral("W") : volts ? QStringLiteral("V") : QString())) {
        return false;
    }
    const MirrorProperty* property = MirrorSchema::forMetaObject(
        &TransmitState::staticMetaObject).byName(name);
    return property && !property->isWritable
        && property->kind == (raw ? MirrorWireKind::Int64 : MirrorWireKind::Float64)
        && MirrorPolicy::hasExplicitEntry(QByteArrayLiteral("TransmitState"), name)
        && MirrorPolicy::directionFor(QByteArrayLiteral("TransmitState"), name)
            == MirrorDirection::Outbound;
}

bool SetupDescription::validatePaDriveReadoutBinding(const QJsonObject& control)
{
    const QJsonObject binding = control.value(QStringLiteral("binding")).toObject();
    const QJsonObject ref = binding.value(QStringLiteral("property")).toObject();
    if (control.size() != 9 || binding.size() != 1 || ref.size() != 2
        || ref != QJsonObject{{QStringLiteral("object"), QStringLiteral("transmit")},
                              {QStringLiteral("name"), QStringLiteral("power")}}
        || control.value(QStringLiteral("id")) != QJsonValue(QStringLiteral("pa.values.drive"))
        || control.value(QStringLiteral("label")) != QJsonValue(QStringLiteral("Drive:"))
        || control.value(QStringLiteral("tooltip")) != QJsonValue(QString())
        || control.value(QStringLiteral("kind")) != QJsonValue(QStringLiteral("readout"))
        || control.value(QStringLiteral("applies")) != QJsonValue(QStringLiteral("live"))
        || control.value(QStringLiteral("gate")).toObject()
            != QJsonObject{{QStringLiteral("capability"), QStringLiteral("transmitSettingsVersion")},
                           {QStringLiteral("min"), 1}}
        || control.value(QStringLiteral("decimals")) != QJsonValue(0)
        || control.value(QStringLiteral("unit")) != QJsonValue(QStringLiteral("W"))) {
        return false;
    }
    const QByteArray name = QByteArrayLiteral("power");
    const MirrorProperty* property = MirrorSchema::forMetaObject(
        &TransmitModel::staticMetaObject).byName(name);
    return property && property->isWritable && property->kind == MirrorWireKind::Int64
        && MirrorPolicy::hasExplicitEntry(QByteArrayLiteral("TransmitModel"), name)
        && MirrorPolicy::directionFor(QByteArrayLiteral("TransmitModel"), name)
            == MirrorDirection::Bidirectional;
}

bool SetupDescription::validatePaTelemetryReadoutBinding(const QJsonObject& control)
{
    const QJsonObject binding = control.value(QStringLiteral("binding")).toObject();
    const QJsonObject ref = binding.value(QStringLiteral("telemetry")).toObject();
    if (control.size() != 10 || binding.size() != 1 || ref.size() != 2
        || ref.value(QStringLiteral("object")) != QJsonValue(QStringLiteral("radio"))
        || control.value(QStringLiteral("kind")) != QJsonValue(QStringLiteral("readout"))
        || control.value(QStringLiteral("applies")) != QJsonValue(QStringLiteral("live"))
        || control.value(QStringLiteral("tooltip")) != QJsonValue(QString())
        || control.value(QStringLiteral("requiresDescriptionVersion")) != QJsonValue(5)
        || control.value(QStringLiteral("gate")).toObject()
            != QJsonObject{{QStringLiteral("capability"), QStringLiteral("stationTelemetryVersion")},
                           {QStringLiteral("min"), 4}}) {
        return false;
    }
    const QString name = ref.value(QStringLiteral("name")).toString();
    if (name == QLatin1String("paCurrentAmps")) {
        return control.value(QStringLiteral("id")) == QJsonValue(QStringLiteral("pa.values.paCurrent"))
            && control.value(QStringLiteral("label")) == QJsonValue(QStringLiteral("PA Current:"))
            && control.value(QStringLiteral("decimals")) == QJsonValue(2)
            && control.value(QStringLiteral("unit")) == QJsonValue(QStringLiteral("A"));
    }
    if (name == QLatin1String("supplyVolts")) {
        return control.value(QStringLiteral("id")) == QJsonValue(QStringLiteral("pa.values.dcVoltage"))
            && control.value(QStringLiteral("label")) == QJsonValue(QStringLiteral("DC Voltage:"))
            && control.value(QStringLiteral("decimals")) == QJsonValue(1)
            && control.value(QStringLiteral("unit")) == QJsonValue(QStringLiteral("V"));
    }
    return false;
}

bool SetupDescription::validatePaBypassBinding(const QJsonObject& control)
{
    const QJsonObject binding = control.value(QStringLiteral("binding")).toObject();
    const QJsonObject ref = binding.value(QStringLiteral("property")).toObject();
    if (control.size() != 7 || binding.size() != 1 || ref.size() != 2
        || ref.value(QStringLiteral("object")) != QJsonValue(QStringLiteral("transmit"))
        || ref.value(QStringLiteral("name")) != QJsonValue(QStringLiteral("paSettingsBypass"))
        || control.value(QStringLiteral("id")) != QJsonValue(QStringLiteral("pa.gain.bypassPaSettings"))
        || control.value(QStringLiteral("label")) != QJsonValue(QStringLiteral("Bypass ANAN PA Settings"))
        || control.value(QStringLiteral("tooltip")) != QJsonValue(QStringLiteral(
            "Bypass the board-specific PA calibration table (BP PA). "
            "When checked, the generic Hermes gain row is used instead "
            "of the ANAN-G2E factory row. Useful when PA gain for this "
            "radio is not calibrated."))
        || control.value(QStringLiteral("kind")) != QJsonValue(QStringLiteral("toggle"))
        || control.value(QStringLiteral("applies")) != QJsonValue(QStringLiteral("live"))
        || control.value(QStringLiteral("gate")).toObject() != QJsonObject{
            {QStringLiteral("capability"), QStringLiteral("transmitSettingsVersion")},
            {QStringLiteral("min"), 6}}) {
        return false;
    }
    const QByteArray name = QByteArrayLiteral("paSettingsBypass");
    const MirrorProperty* property = MirrorSchema::forMetaObject(
        &TransmitModel::staticMetaObject).byName(name);
    return property && property->isWritable && property->kind == MirrorWireKind::Bool
        && MirrorPolicy::hasExplicitEntry(QByteArrayLiteral("TransmitModel"), name)
        && MirrorPolicy::inboundAllowed(QByteArrayLiteral("TransmitModel"), name);
}

bool SetupDescription::validatePaV13Control(const QJsonObject& control)
{
    const auto row = paV13Controls().constFind(control.value(QStringLiteral("id")).toString());
    return row != paV13Controls().constEnd() && control == *row;
}

bool SetupDescription::validatePaV14Control(const QJsonObject& control)
{
    const auto row = paV14Controls().constFind(control.value(QStringLiteral("id")).toString());
    return row != paV14Controls().constEnd() && control == *row;
}

bool SetupDescription::validateTransmitV13Control(const QJsonObject& control)
{
    const auto row = transmitV13Controls().constFind(control.value(QStringLiteral("id")).toString());
    return row != transmitV13Controls().constEnd() && control == *row;
}

bool SetupDescription::validateHardwareV13Control(const QJsonObject& control)
{
    const auto row = hardwareV13Controls().constFind(
        control.value(QStringLiteral("id")).toString());
    return row != hardwareV13Controls().constEnd() && control == *row;
}

bool SetupDescription::validateCatNetworkV21EnabledWhen(const QJsonObject& control)
{
    // Forget acts only while Duplicate is on, as the desktop greys it out.
    return control.value(QStringLiteral("id"))
            == QJsonValue(QStringLiteral("catNetwork.tciServer.core.forgetRx2VfoBOnDisconnect"))
        && control.value(QStringLiteral("enabledWhen")) == QJsonValue(QJsonObject{
            {QStringLiteral("property"), QJsonObject{
                {QStringLiteral("object"), QStringLiteral("stationTci")},
                {QStringLiteral("name"), QStringLiteral("copyRx2VfobToVfoa")}}},
            {QStringLiteral("oneOf"), QJsonArray{true}}});
}

bool SetupDescription::validateDspV19Control(const QJsonObject& control)
{
    const auto row = dspV19Controls().constFind(
        control.value(QStringLiteral("id")).toString());
    return row != dspV19Controls().constEnd() && control == *row;
}

bool SetupDescription::validateHardwareV16Control(const QJsonObject& control)
{
    const auto row = hardwareV16Controls().constFind(
        control.value(QStringLiteral("id")).toString());
    return row != hardwareV16Controls().constEnd() && control == *row;
}

bool SetupDescription::validateHardwareV18Control(const QJsonObject& control)
{
    const auto row = hardwareV18Controls().constFind(
        control.value(QStringLiteral("id")).toString());
    return row != hardwareV18Controls().constEnd() && control == *row;
}

bool SetupDescription::validateAudioV24Control(const QJsonObject& control)
{
    const auto row = audioV24Controls().constFind(
        control.value(QStringLiteral("id")).toString());
    return row != audioV24Controls().constEnd() && control == *row;
}

bool SetupDescription::validateHardwareV23Control(const QJsonObject& control)
{
    const auto row = hardwareV23Controls().constFind(
        control.value(QStringLiteral("id")).toString());
    return row != hardwareV23Controls().constEnd() && control == *row;
}

bool SetupDescription::validateTransmitSettingBinding(const QJsonObject& control)
{
    const QJsonObject binding = control.value(QStringLiteral("binding")).toObject();
    if (binding.size() != 1 || !binding.value(QStringLiteral("setting")).isString()) {
        return false;
    }
    const QString key = binding.value(QStringLiteral("setting")).toString();
    const QJsonObject gate = control.value(QStringLiteral("gate")).toObject();
    if (classifySettingsKey(key) != SettingsScope::Station
        || gate.value(QStringLiteral("transmit")) != QJsonValue(true)
        || gate.value(QStringLiteral("capability")) != QJsonValue(QStringLiteral("transmitSettingsVersion"))
        || gate.value(QStringLiteral("min")).toInt() < 5
        || gate.contains(QStringLiteral("offAir"))) {
        return false;
    }
    if (key == QLatin1String("SwrProtectionLimit")) {
        return control.value(QStringLiteral("kind")) == QJsonValue(QStringLiteral("decimal"))
            && control.value(QStringLiteral("min")) == QJsonValue(1.0)
            && control.value(QStringLiteral("max")) == QJsonValue(5.0)
            && control.value(QStringLiteral("step")) == QJsonValue(0.1);
    }
    if (key == QLatin1String("TunePowerSwrIgnore")) {
        return control.value(QStringLiteral("kind")) == QJsonValue(QStringLiteral("integer"))
            && control.value(QStringLiteral("min")) == QJsonValue(5)
            && control.value(QStringLiteral("max")) == QJsonValue(50)
            && control.value(QStringLiteral("step")) == QJsonValue(1);
    }
    static const QSet<QString> kToggles{
        QStringLiteral("SwrProtectionEnabled"), QStringLiteral("SwrTuneProtectionEnabled"),
        QStringLiteral("WindBackPowerSwr"), QStringLiteral("TxInhibitMonitorEnabled"),
        QStringLiteral("TxInhibitMonitorReversed")};
    return kToggles.contains(key) && validateSettingToggleEncoding(control);
}

bool SetupDescription::validateAudioPropertyBinding(const QJsonObject& control)
{
    const QJsonObject binding = control.value(QStringLiteral("binding")).toObject();
    if (binding.size() != 1 || !binding.value(QStringLiteral("property")).isObject()) {
        return false;
    }
    const QJsonObject ref = binding.value(QStringLiteral("property")).toObject();
    if (ref.size() != 2 || ref.value(QStringLiteral("object")) != QJsonValue(QStringLiteral("transmit"))) {
        return false;
    }
    const QByteArray name = ref.value(QStringLiteral("name")).toString().toUtf8();
    const int version = name == QByteArrayLiteral("filterLow")
        || name == QByteArrayLiteral("filterHigh") ? 1
        : name == QByteArrayLiteral("amCarrierLevel") ? 2 : 0;
    const QJsonObject gate = control.value(QStringLiteral("gate")).toObject();
    if (version == 0 || control.value(QStringLiteral("kind")) != QJsonValue(QStringLiteral("integer"))
        || gate.value(QStringLiteral("transmit")) != QJsonValue(true)
        || gate.value(QStringLiteral("capability")) != QJsonValue(QStringLiteral("transmitSettingsVersion"))
        || gate.value(QStringLiteral("min")) != QJsonValue(version)
        || gate.contains(QStringLiteral("offAir"))) {
        return false;
    }
    const MirrorProperty* property = MirrorSchema::forMetaObject(&TransmitModel::staticMetaObject).byName(name);
    return property && property->isWritable && property->kind == MirrorWireKind::Int64
        && MirrorPolicy::inboundAllowed(QByteArrayLiteral("TransmitModel"), name);
}

bool SetupDescription::validateDspSettingBinding(const QJsonObject& control)
{
    const QJsonObject binding = control.value(QStringLiteral("binding")).toObject();
    if (binding.size() != 1 || !binding.value(QStringLiteral("setting")).isString()) {
        return false;
    }
    const QString key = binding.value(QStringLiteral("setting")).toString();
    if (classifySettingsKey(key) != SettingsScope::Station) {
        return false;
    }
    const QString kind = control.value(QStringLiteral("kind")).toString();
    if (key == QLatin1String("DspOptionsCacheImpulse")
        || key == QLatin1String("DspOptionsCacheImpulseSaveRestore")) {
        return kind == QLatin1String("toggle")
            && validateSettingToggleEncoding(control);
    }
    if (kind != QLatin1String("choice")) {
        return false;
    }
    for (const QString& family : {QStringLiteral("BufferSize"), QStringLiteral("FilterSize"),
                                  QStringLiteral("FilterType")}) {
        for (const QString& mode : {QStringLiteral("Phone"), QStringLiteral("Fm"),
                                    QStringLiteral("Cw"), QStringLiteral("Dig")}) {
            for (const QString& side : {QStringLiteral("Rx"), QStringLiteral("Tx")}) {
                if (mode == QLatin1String("Cw") && side == QLatin1String("Tx")) {
                    continue;
                }
                if (key == QStringLiteral("DspOptions") + family + mode + side) {
                    const QJsonArray choices = control.value(QStringLiteral("choices")).toArray();
                    const QStringList expected = family == QLatin1String("BufferSize")
                        ? QStringList{QStringLiteral("64"), QStringLiteral("128"),
                                      QStringLiteral("256"), QStringLiteral("512"),
                                      QStringLiteral("1024")}
                        : family == QLatin1String("FilterSize")
                          ? QStringList{QStringLiteral("1024"), QStringLiteral("2048"),
                                        QStringLiteral("4096"), QStringLiteral("8192"),
                                        QStringLiteral("16384")}
                          : QStringList{QStringLiteral("Linear Phase"),
                                        QStringLiteral("Low Latency")};
                    const QJsonObject gate = control.value(QStringLiteral("gate")).toObject();
                    if (choices.size() != expected.size()
                        // Only the TX buffer sizes stay off-air: Thetis
                        // greys grpDSPBufferSize during MOX (setup.cs:5159
                        // [v2.10.3.15]); the TX filter size and type are not.
                        || (side == QLatin1String("Tx") && family == QLatin1String("BufferSize")
                            ? gate.value(QStringLiteral("offAir")) != QJsonValue(true)
                            : gate.contains(QStringLiteral("offAir")))
                        || (side == QLatin1String("Tx")
                            && (gate.value(QStringLiteral("capability")) != QJsonValue(QStringLiteral("transmitSettingsVersion"))
                                || gate.value(QStringLiteral("min")).toInt() < 1))) {
                        return false;
                    }
                    for (int i = 0; i < expected.size(); ++i) {
                        if (choices.at(i).toString() != expected.at(i)) {
                            return false;
                        }
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

bool SetupDescription::validateCommandBinding(const QJsonObject& control, QString* error)
{
    const auto fail = [error](const QString& why) {
        if (error) {
            *error = why;
        }
        return false;
    };
    const QString kind = control.value(QStringLiteral("kind")).toString();
    if (control.value(QStringLiteral("id")) == QJsonValue(QStringLiteral("dsp.tnf.add"))) {
        const QJsonObject expected{{QStringLiteral("id"), QStringLiteral("dsp.tnf.add")},
            {QStringLiteral("label"), QStringLiteral("Add")},
            {QStringLiteral("tooltip"), QStringLiteral("Add a notch")},
            {QStringLiteral("kind"), QStringLiteral("button")},
            {QStringLiteral("requiresDescriptionVersion"), 2},
            {QStringLiteral("binding"), QJsonObject{{QStringLiteral("command"), QJsonObject{
                {QStringLiteral("verb"), QStringLiteral("notch.addAtSlice")},
                {QStringLiteral("arguments"), QJsonObject{{QStringLiteral("sliceId"),
                    QJsonObject{{QStringLiteral("$selectedOwnedSliceId"), true}}}}}}}}},
            {QStringLiteral("applies"), QStringLiteral("live")},
            {QStringLiteral("gate"), QJsonObject{{QStringLiteral("capability"),
                QStringLiteral("notchControlVersion")}, {QStringLiteral("min"), 2}}}};
        if (control != expected) {
            return fail(QStringLiteral("TNF Add must use the exact selected owned slice contract"));
        }
    }
    // Version 15: a choice whose choices come from the Core's names sends
    // the chosen name.
    const bool v15 = control.value(QStringLiteral("requiresDescriptionVersion"))
        == QJsonValue(SetupDescriptionV15::kVersion);
    const MirrorWireKind controlKind = kind == QLatin1String("toggle") ? MirrorWireKind::Bool
        : kind == QLatin1String("choice") && (control.contains(QStringLiteral("choicesFrom"))
              || control.value(QStringLiteral("options")).toArray().first().toObject()
                     .value(QStringLiteral("value")).isString())
          ? MirrorWireKind::Utf8
        : kind == QLatin1String("integer") || kind == QLatin1String("slider")
          || kind == QLatin1String("choice") ? MirrorWireKind::Int64
        : kind == QLatin1String("decimal") ? MirrorWireKind::Float64
        : kind == QLatin1String("text") || kind == QLatin1String("colour")
          ? MirrorWireKind::Utf8 : MirrorWireKind::Unsupported;
    const QJsonObject binding = control.value(QStringLiteral("binding")).toObject();
    if (binding.size() != 1 || !binding.value(QStringLiteral("command")).isObject()) {
        return fail(QStringLiteral("a command control needs exactly one command binding"));
    }
    const QJsonObject command = binding.value(QStringLiteral("command")).toObject();
    const QByteArray verb = command.value(QStringLiteral("verb")).toString().toUtf8();
    const CommandVerbSpec* spec = nullptr;
    for (const CommandVerbSpec& candidate : SessionCommandDispatcher::verbSpecs()) {
        if (candidate.verb == verb) {
            spec = &candidate;
            break;
        }
    }
    if (!spec) {
        return fail(QStringLiteral("unknown command verb"));
    }
    const QJsonObject gate = control.value(QStringLiteral("gate")).toObject();
    if (!spec->capability.isEmpty()
        && (gate.value(QStringLiteral("capability")).toString().toUtf8() != spec->capability
            || gate.value(QStringLiteral("min")).toInt() < spec->capabilityVersion)) {
        return fail(QStringLiteral("command capability gate is missing or too low"));
    }
    const auto propertyKind = [](const QJsonValue& raw) {
        if (!raw.isObject()) {
            return MirrorWireKind::Unsupported;
        }
        const QJsonObject ref = raw.toObject();
        if (ref.size() != 2 || ref.value(QStringLiteral("object")).toString().isEmpty()
            || ref.value(QStringLiteral("name")).toString().isEmpty()) {
            return MirrorWireKind::Unsupported;
        }
        // Every static object named here has an allowlisted mirror schema.
        // Future categories extend this map alongside their object bindings.
        const QString object = ref.value(QStringLiteral("object")).toString();
        const QMetaObject* meta = object == QLatin1String("stationTci")
            ? &StationTciModel::staticMetaObject
            : object == QLatin1String("slice:active")
              ? &SliceModel::staticMetaObject
            : object == QLatin1String("transmit")
              ? &TransmitModel::staticMetaObject
            : object == QLatin1String("dspAssets")
              ? &DspAssetService::staticMetaObject
            : object == QLatin1String("radio")
              ? &RadioModel::staticMetaObject
            : object == QLatin1String("amplifier")
              ? &AmplifierModel::staticMetaObject
            : object == QLatin1String("tuner")
              ? &TunerModel::staticMetaObject
            : object == QLatin1String("rfkit")
              ? &RfKitModel::staticMetaObject
            : object == QLatin1String("accessoryData")
              ? &AccessoryDataModel::staticMetaObject
            : object == QLatin1String("accessorySettings")
              ? &AccessorySettingsModel::staticMetaObject : nullptr;
        if (!meta) {
            return MirrorWireKind::Unsupported;
        }
        const MirrorProperty* property = MirrorSchema::forMetaObject(meta).byName(
            ref.value(QStringLiteral("name")).toString().toUtf8());
        return property ? property->kind : MirrorWireKind::Unsupported;
    };
    const MirrorWireKind valueKind = command.contains(QStringLiteral("valueProperty"))
        ? propertyKind(command.value(QStringLiteral("valueProperty"))) : controlKind;
    if (command.contains(QStringLiteral("valueProperty")) && valueKind != controlKind
        && !(v15 && valueKind == MirrorWireKind::Enum && controlKind == MirrorWireKind::Int64)) {
        return fail(QStringLiteral("command valueProperty is missing or has the wrong type"));
    }
    if (!command.value(QStringLiteral("arguments")).isObject()) {
        return fail(QStringLiteral("command arguments are missing"));
    }
    const QJsonObject args = command.value(QStringLiteral("arguments")).toObject();
    int required = 0;
    for (const CommandArgumentSpec& expected : spec->arguments) {
        const QString name = QString::fromUtf8(expected.name);
        if (!expected.optional) {
            ++required;
        }
        if (!args.contains(name)) {
            if (expected.optional) { continue; }
            return fail(QStringLiteral("required command argument is missing: %1").arg(name));
        }
        const QJsonValue value = args.value(name);
        MirrorWireKind supplied = MirrorWireKind::Unsupported;
        if (value.isObject()) {
            const QJsonObject source = value.toObject();
            if (source.size() != 1) {
                return fail(QStringLiteral("command argument source must have one marker"));
            }
            if (source.contains(QStringLiteral("$controlValue"))) {
                if (source.value(QStringLiteral("$controlValue")) != QJsonValue(true)) {
                    return fail(QStringLiteral("$controlValue must be true"));
                }
                supplied = controlKind;
            } else if (source.contains(QStringLiteral("$selectedOwnedSliceId"))) {
                // Version 15: any verb's sliceId may come from the selected
                // owned slice, with the same checks as TNF Add.
                const bool tnfAdd = control.value(QStringLiteral("id"))
                        == QJsonValue(QStringLiteral("dsp.tnf.add"))
                    && control.value(QStringLiteral("requiresDescriptionVersion")) == QJsonValue(2)
                    && verb == "notch.addAtSlice";
                if (!(tnfAdd || v15) || name != QLatin1String("sliceId")
                    || source.value(QStringLiteral("$selectedOwnedSliceId")) != QJsonValue(true)) {
                    return fail(QStringLiteral("invalid selected owned slice source"));
                }
                supplied = MirrorWireKind::Int64;
            } else if (source.contains(QStringLiteral("$prompt"))) {
                // Version 15: the text the button's `prompt` asked for.
                if (!v15 || kind != QLatin1String("button")
                    || !control.value(QStringLiteral("prompt")).isObject()
                    || source.value(QStringLiteral("$prompt")) != QJsonValue(true)) {
                    return fail(QStringLiteral("invalid prompt source"));
                }
                supplied = MirrorWireKind::Utf8;
            } else if (source.contains(QStringLiteral("$control"))) {
                // Version 15: a staged row of the same section; its kind is
                // checked with the section (SetupDescriptionV15::validateSection).
                if (!v15 || !source.value(QStringLiteral("$control")).isString()) {
                    return fail(QStringLiteral("invalid staged control source"));
                }
                supplied = expected.kind;
            } else if (source.contains(QStringLiteral("$property"))) {
                supplied = propertyKind(source.value(QStringLiteral("$property")));
                // Version 15: a mirrored enum's number fills an integer argument.
                if (v15 && supplied == MirrorWireKind::Enum
                    && expected.kind == MirrorWireKind::Int64) {
                    supplied = MirrorWireKind::Int64;
                }
            } else {
                return fail(QStringLiteral("unknown command argument source"));
            }
        } else if (value.isBool()) {
            supplied = MirrorWireKind::Bool;
        } else if (value.isString()) {
            supplied = MirrorWireKind::Utf8;
        } else if (value.isDouble()) {
            supplied = std::floor(value.toDouble()) == value.toDouble()
                ? MirrorWireKind::Int64 : MirrorWireKind::Float64;
        }
        if (supplied != expected.kind) {
            return fail(QStringLiteral("command argument has wrong type: %1").arg(name));
        }
    }
    if (args.size() < required || args.size() > spec->arguments.size()) {
        return fail(QStringLiteral("command has extra or missing arguments"));
    }
    for (auto it = args.constBegin(); it != args.constEnd(); ++it) {
        bool known = false;
        for (const CommandArgumentSpec& expected : spec->arguments) {
            known |= it.key().toUtf8() == expected.name;
        }
        if (!known) {
            return fail(QStringLiteral("command has an unknown argument"));
        }
    }
    if (error) {
        error->clear();
    }
    return true;
}

bool SetupDescription::validateSettingsHygienePanel(const QJsonObject& control)
{
    // This panel is a fixed description of the existing two hygiene verbs,
    // not a command binding or a new generic result/argument language.
    return control == expectedSettingsHygienePanel();
}

bool SetupDescription::validateAntennaRowsTable(const QJsonObject& control, HPSDRModel model)
{
    const QString id = control.value(QStringLiteral("id")).toString();
    const bool tx = id == QLatin1String("hardware.antenna.txRows");
    if (!tx && id != QLatin1String("hardware.antenna.rxRows")) { return false; }
    if (control != expectedAntennaRowsTable(tx, model)) { return false; }
    // The closed table maps the existing 14-value CSV mirrors to only the
    // already admitted per-band verbs. No table-supplied command is parsed.
    const MirrorSchema& schema = MirrorSchema::forMetaObject(
        &AlexAntennaFacade::staticMetaObject);
    const auto hasSource = [&schema](const QByteArray& name, MirrorWireKind kind) {
        const MirrorProperty* property = schema.byName(name);
        return property && property->kind == kind
            && MirrorPolicy::hasExplicitEntry(QByteArrayLiteral("AlexAntennaFacade"), name);
    };
    return hasSource("txAntennas", MirrorWireKind::Utf8)
        && hasSource("rxAntennas", MirrorWireKind::Utf8)
        && hasSource("rxOnlyAntennas", MirrorWireKind::Utf8)
        && hasSource("blockTxAnt2", MirrorWireKind::Bool)
        && hasSource("blockTxAnt3", MirrorWireKind::Bool);
}

bool SetupDescription::validateTnfTable(const QJsonObject& control, QString* error)
{
    const auto fail = [error](const QString& why) {
        if (error) { *error = why; }
        return false;
    };
    const QJsonObject expectedBinding{{QStringLiteral("table"), QJsonObject{
        {QStringLiteral("valueProperty"), QJsonObject{{QStringLiteral("object"), QStringLiteral("notches")},
                                                     {QStringLiteral("name"), QStringLiteral("listJson")}}},
        {QStringLiteral("revisionProperty"), QJsonObject{{QStringLiteral("object"), QStringLiteral("notches")},
                                                        {QStringLiteral("name"), QStringLiteral("revision")}}},
        {QStringLiteral("format"), QStringLiteral("json-array")},
        {QStringLiteral("rowKey"), QStringLiteral("id")},
        {QStringLiteral("maxRows"), NotchModel::kMaxNotches}}}};
    if (control.value(QStringLiteral("id")) != QJsonValue(QStringLiteral("dsp.tnf.list"))
        || control.value(QStringLiteral("kind")) != QJsonValue(QStringLiteral("table"))
        || control.size() != 10
        || control.value(QStringLiteral("label"))
            != QJsonValue(QStringLiteral("Tunable Notch Filter"))
        || control.value(QStringLiteral("tooltip")) != QJsonValue(QStringLiteral(""))
        || control.value(QStringLiteral("requiresDescriptionVersion")) != QJsonValue(2)
        || control.value(QStringLiteral("applies")) != QJsonValue(QStringLiteral("live"))
        || control.value(QStringLiteral("gate")).toObject()
            != QJsonObject{{QStringLiteral("capability"), QStringLiteral("notchControlVersion")},
                           {QStringLiteral("min"), 1}}
        || control.value(QStringLiteral("binding")).toObject() != expectedBinding) {
        return fail(QStringLiteral("TNF table source, version or gate is invalid"));
    }
    const MirrorSchema& schema = MirrorSchema::forMetaObject(&NotchModel::staticMetaObject);
    const MirrorProperty* list = schema.byName("listJson");
    const MirrorProperty* revision = schema.byName("revision");
    if (!list || list->kind != MirrorWireKind::Utf8
        || !revision || revision->kind != MirrorWireKind::Int64
        || MirrorPolicy::directionFor("NotchModel", "listJson") != MirrorDirection::Outbound
        || MirrorPolicy::directionFor("NotchModel", "revision") != MirrorDirection::Outbound) {
        return fail(QStringLiteral("TNF table source is not an outbound mirrored list"));
    }
    const QJsonArray columns = control.value(QStringLiteral("columns")).toArray();
    if (columns.size() != 4) { return fail(QStringLiteral("TNF table needs four columns")); }
    const QJsonArray expectedColumns{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("dsp.tnf.list.centreHz")},
                    {QStringLiteral("field"), QStringLiteral("centreHz")},
                    {QStringLiteral("label"), QStringLiteral("Center Frequency (Hz)")},
                    {QStringLiteral("tooltip"), QStringLiteral("Center frequency of the notch")},
                    {QStringLiteral("kind"), QStringLiteral("decimal")},
                    {QStringLiteral("min"), NotchModel::kMinNotchCentreHz},
                    {QStringLiteral("max"), NotchModel::kMaxNotchCentreHz},
                    {QStringLiteral("step"), 1}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("dsp.tnf.list.widthHz")},
                    {QStringLiteral("field"), QStringLiteral("widthHz")},
                    {QStringLiteral("label"), QStringLiteral("Width (Hz)")},
                    {QStringLiteral("tooltip"), QStringLiteral("Bandwdith of the notch")},
                    {QStringLiteral("kind"), QStringLiteral("decimal")},
                    {QStringLiteral("min"), 0}, {QStringLiteral("max"), NotchModel::kMaxNotchWidthHz},
                    {QStringLiteral("step"), 1}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("dsp.tnf.list.active")},
                    {QStringLiteral("field"), QStringLiteral("active")},
                    {QStringLiteral("label"), QStringLiteral("Active")},
                    {QStringLiteral("tooltip"), QStringLiteral("Checked if the notch is active")},
                    {QStringLiteral("kind"), QStringLiteral("toggle")}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("dsp.tnf.list.delete")},
                    {QStringLiteral("rowAction"), QStringLiteral("delete")},
                    {QStringLiteral("label"), QStringLiteral("Delete")},
                    {QStringLiteral("tooltip"), QStringLiteral("Delete the current notch index")},
                    {QStringLiteral("kind"), QStringLiteral("button")}}};
    if (columns != expectedColumns) { return fail(QStringLiteral("TNF table column schema is invalid")); }
    const QJsonArray actions = control.value(QStringLiteral("rowActions")).toArray();
    if (actions.size() != 3) { return fail(QStringLiteral("TNF table needs three row actions")); }
    const auto source = [](const QString& marker, const QString& field) {
        return QJsonObject{{marker, field}};
    };
    const QJsonArray expectedActions{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("move")},
                    {QStringLiteral("command"), QJsonObject{
                        {QStringLiteral("verb"), QStringLiteral("notch.move")},
                        {QStringLiteral("arguments"), QJsonObject{
                            {QStringLiteral("id"), source(QStringLiteral("$row"), QStringLiteral("id"))},
                            {QStringLiteral("centreHz"), source(QStringLiteral("$edit"), QStringLiteral("centreHz"))},
                            {QStringLiteral("widthHz"), source(QStringLiteral("$edit"), QStringLiteral("widthHz"))}}}}}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("active")},
                    {QStringLiteral("command"), QJsonObject{
                        {QStringLiteral("verb"), QStringLiteral("notch.setActive")},
                        {QStringLiteral("arguments"), QJsonObject{
                            {QStringLiteral("id"), source(QStringLiteral("$row"), QStringLiteral("id"))},
                            {QStringLiteral("active"), source(QStringLiteral("$edit"), QStringLiteral("active"))}}}}}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("delete")},
                    {QStringLiteral("command"), QJsonObject{
                        {QStringLiteral("verb"), QStringLiteral("notch.delete")},
                        {QStringLiteral("arguments"), QJsonObject{
                            {QStringLiteral("id"), source(QStringLiteral("$row"), QStringLiteral("id"))}}}}}}};
    if (actions != expectedActions) { return fail(QStringLiteral("TNF row action schema is invalid")); }
    const QList<CommandVerbSpec>& specs = SessionCommandDispatcher::verbSpecs();
    const auto hasVerb = [&specs](const QByteArray& verb,
                                  const QList<QByteArray>& names,
                                  const QList<MirrorWireKind>& kinds) {
        for (const CommandVerbSpec& spec : specs) {
            if (spec.verb != verb || spec.capability != "notchControlVersion"
                || spec.capabilityVersion != 1 || spec.arguments.size() != kinds.size()) {
                continue;
            }
            bool matched = true;
            for (int i = 0; i < kinds.size(); ++i) {
                matched &= !spec.arguments.at(i).optional
                    && spec.arguments.at(i).name == names.at(i)
                    && spec.arguments.at(i).kind == kinds.at(i);
            }
            if (matched) { return true; }
        }
        return false;
    };
    if (!hasVerb("notch.move", {"id", "centreHz", "widthHz"},
                 {MirrorWireKind::Int64, MirrorWireKind::Float64,
                                 MirrorWireKind::Float64})
        || !hasVerb("notch.setActive", {"id", "active"},
                    {MirrorWireKind::Int64, MirrorWireKind::Bool})
        || !hasVerb("notch.delete", {"id"}, {MirrorWireKind::Int64})) {
        return fail(QStringLiteral("TNF row verbs no longer match typed Core commands"));
    }
    if (error) { error->clear(); }
    return true;
}

namespace {

// Version 20 (R-R3-49, JJ's ruling: follow Thetis). Thetis locks the PA
// profile controls while MOX is on (PAProfileEnableControls, setup.cs
// 23479-23496 [v2.10.3.15]) and keeps only the transmitting band's cells
// live (setAdjustingBand, setup.cs 23839-23852 [v2.10.3.15]). The Core
// enforces that on its writes; the description publishes the same state
// per control and per table row with the availability object the other
// rows use.
constexpr char kPaHolderMayEdit[] = "holderMayEdit";

template <typename Fn>
QJsonObject mapCategoryControls(QJsonObject category, Fn&& fn)
{
    QJsonArray pages = category.value(QStringLiteral("pages")).toArray();
    for (int p = 0; p < pages.size(); ++p) {
        QJsonObject page = pages.at(p).toObject();
        QJsonArray sections = page.value(QStringLiteral("sections")).toArray();
        for (int s = 0; s < sections.size(); ++s) {
            QJsonObject section = sections.at(s).toObject();
            QJsonArray controls = section.value(QStringLiteral("controls")).toArray();
            for (int c = 0; c < controls.size(); ++c) {
                controls[c] = fn(controls.at(c).toObject());
            }
            section.insert(QStringLiteral("controls"), controls);
            sections[s] = section;
        }
        page.insert(QStringLiteral("sections"), sections);
        pages[p] = page;
    }
    category.insert(QStringLiteral("pages"), pages);
    return category;
}

QJsonObject lockedAvailability(const QString& reason)
{
    return QJsonObject{{QStringLiteral("enabled"), false},
                       {QStringLiteral("reason"), reason}};
}

// The Core's PA description in its version 20 shape: the table's gate no
// longer closes it off the air as a whole; on the air each row says why it
// is locked, and the transmitting band's row is marked for the holder.
QString paWithOnAirState(const QString& description, bool onAir, int transmittingBand)
{
    if (description.isEmpty()) { return description; }
    const QJsonDocument document = QJsonDocument::fromJson(description.toUtf8());
    if (!document.isObject()) { return description; }
    const QJsonObject fitted = mapCategoryControls(document.object(),
        [onAir, transmittingBand](QJsonObject control) {
            const QString id = control.value(QStringLiteral("id")).toString();
            if (!paV14Controls().contains(id)) { return control; }
            if (id != QLatin1String("pa.gain.table")) {
                if (onAir) {
                    control.insert(QStringLiteral("availability"),
                                   lockedAvailability(RadioModel::paOnAirLockedReason()));
                }
                return control;
            }
            QJsonObject gate = control.value(QStringLiteral("gate")).toObject();
            gate.remove(QStringLiteral("offAir"));
            control.insert(QStringLiteral("gate"), gate);
            if (!onAir) { return control; }
            QJsonArray rows = control.value(QStringLiteral("rows")).toArray();
            for (int i = 0; i < rows.size(); ++i) {
                QJsonObject row = rows.at(i).toObject();
                if (transmittingBand >= 0
                    && row.value(QStringLiteral("band")).toInt(-1) == transmittingBand) {
                    row.insert(QStringLiteral("availability"),
                               lockedAvailability(RadioModel::paHolderOnlyReason()));
                    row.insert(QLatin1String(kPaHolderMayEdit), true);
                } else {
                    row.insert(QStringLiteral("availability"),
                               lockedAvailability(RadioModel::paOnAirLockedReason()));
                }
                rows[i] = row;
            }
            control.insert(QStringLiteral("rows"), rows);
            return control;
        });
    return QString::fromUtf8(QJsonDocument(fitted).toJson(QJsonDocument::Compact));
}

// Version 22: DSP > Options' RX buffer size rows lock on the air. The TCI
// lane holds version 21; this branch's description skips it.
constexpr int kDspOnAirLockVersion = 22;

bool isRxBufferSizeControl(const QJsonObject& control)
{
    const QString id = control.value(QStringLiteral("id")).toString();
    return id.startsWith(QLatin1String("dsp.options."))
        && RadioModel::isRxDspBufferSizeKey(id.mid(int(qstrlen("dsp.options."))));
}

// The Core's DSP description in its version 22 shape: on the air the four
// RX buffer size rows say why they are locked. Thetis greys the whole
// Buffer Size (IQcomp) group, RX combos included, while MOX is on:
// From Thetis setup.cs:5159 [v2.10.3.15] grpDSPBufferSize.Enabled = !mox;
// The TX rows keep their offAir gate. The lock is published rather than
// gated on offAir because offAir needs txState, which a receive-only
// peer (no remoteTx) never gets.
QString dspWithOnAirState(const QString& description, bool onAir)
{
    if (description.isEmpty() || !onAir) { return description; }
    const QJsonDocument document = QJsonDocument::fromJson(description.toUtf8());
    if (!document.isObject()) { return description; }
    const QJsonObject fitted = mapCategoryControls(document.object(), [](QJsonObject control) {
        if (isRxBufferSizeControl(control)) {
            control.insert(QStringLiteral("availability"),
                           lockedAvailability(RadioModel::dspBufferOnAirLockedReason()));
        }
        return control;
    });
    return QString::fromUtf8(QJsonDocument(fitted).toJson(QJsonDocument::Compact));
}

} // namespace

QString SetupDescription::fitCategoryForVersion(const QString& description, int version,
                                                bool antennaRowsAvailable, bool holdsTransmit)
{
    if (version < 1 || description.isEmpty()) { return {}; }
    const QJsonDocument document = QJsonDocument::fromJson(description.toUtf8());
    if (!document.isObject()) { return {}; }
    QJsonObject category = document.object();
    const QString categoryId = category.value(QStringLiteral("category")).toObject()
        .value(QStringLiteral("id")).toString();
    QJsonArray pages;
    for (const QJsonValue& rawPage : category.value(QStringLiteral("pages")).toArray()) {
        QJsonObject page = rawPage.toObject();
        QJsonArray sections;
        for (const QJsonValue& rawSection : page.value(QStringLiteral("sections")).toArray()) {
            QJsonObject section = rawSection.toObject();
            QJsonArray controls;
            for (const QJsonValue& rawControl : section.value(QStringLiteral("controls")).toArray()) {
                QJsonObject control = rawControl.toObject();
                // Hardware 18 opened HL2 Options' clock rows: a peer below 18
                // keeps the version 16 rows it was built for, closed.
                if (categoryId == QLatin1String("hardware") && version < 18
                    && control.value(QStringLiteral("requiresDescriptionVersion")) == QJsonValue(18)) {
                    const auto older = hardwareV16Controls().constFind(
                        control.value(QStringLiteral("id")).toString());
                    if (older != hardwareV16Controls().constEnd()) {
                        control = *older;
                    }
                }
                // Audio 24 moved Line In Gain to 1.5 dB steps: a peer below
                // 24 keeps the version 15 row it was built for (whole
                // decibels from -34).
                if (categoryId == QLatin1String("audio") && version < 24
                    && control.value(QStringLiteral("id"))
                        == QJsonValue(QStringLiteral("audio.txInput.hermesLineInGain"))
                    && control.value(QStringLiteral("requiresDescriptionVersion"))
                        == QJsonValue(24)) {
                    control.insert(QStringLiteral("requiresDescriptionVersion"),
                                   SetupDescriptionV15::kVersion);
                    control.insert(QStringLiteral("min"), -34);
                    control.insert(QStringLiteral("step"), 1);
                    control.remove(QStringLiteral("decimals"));
                }
                // CAT & Network 21 greys TCI's Forget row out while Duplicate
                // is off: a peer below 21 keeps the row, always enabled.
                if (categoryId == QLatin1String("catNetwork") && version < 21) {
                    control.remove(QStringLiteral("enabledWhen"));
                }
                if (control.value(QStringLiteral("requiresDescriptionVersion")).toInt(1) <= version
                    && (antennaRowsAvailable || !control.value(QStringLiteral("binding"))
                            .toObject().contains(QStringLiteral("antennaRows")))) {
                    if (version < 4 && (categoryId == QLatin1String("display")
                                        || categoryId == QLatin1String("appearance"))) {
                        control.remove(QStringLiteral("default"));
                        if (control.value(QStringLiteral("kind")) == QJsonValue(QStringLiteral("decimal"))) {
                            control.remove(QStringLiteral("decimals"));
                        }
                    }
                    controls.append(control);
                }
            }
            if (!controls.isEmpty()) {
                section.insert(QStringLiteral("controls"), controls);
                sections.append(section);
            }
        }
        if (!sections.isEmpty()) {
            page.insert(QStringLiteral("sections"), sections);
            // Version 15: a page's coverage as a version 15 peer sees it
            // (an empty string: the page is complete).
            if (page.contains(QStringLiteral("coverageV15"))) {
                if (version >= SetupDescriptionV15::kVersion) {
                    const QString coverage = page.value(QStringLiteral("coverageV15")).toString();
                    if (coverage.isEmpty()) {
                        page.remove(QStringLiteral("coverage"));
                    } else {
                        page.insert(QStringLiteral("coverage"), coverage);
                    }
                }
                page.remove(QStringLiteral("coverageV15"));
            }
            // Version 19: likewise, a page's coverage to a version 19 peer.
            if (page.contains(QStringLiteral("coverageV19"))) {
                if (version >= 19) {
                    const QString coverage = page.value(QStringLiteral("coverageV19")).toString();
                    if (coverage.isEmpty()) {
                        page.remove(QStringLiteral("coverage"));
                    } else {
                        page.insert(QStringLiteral("coverage"), coverage);
                    }
                }
                page.remove(QStringLiteral("coverageV19"));
            }
            pages.append(page);
        }
    }
    if (pages.isEmpty()) { return {}; }
    category.insert(QStringLiteral("pages"), pages);
    if (category.contains(QStringLiteral("coverageV15"))) {
        if (version >= SetupDescriptionV15::kVersion) {
            category.insert(QStringLiteral("coverage"),
                            category.value(QStringLiteral("coverageV15")).toString());
        }
        category.remove(QStringLiteral("coverageV15"));
    }
    if (category.contains(QStringLiteral("coverageV19"))) {
        if (version >= 19) {
            category.insert(QStringLiteral("coverage"),
                            category.value(QStringLiteral("coverageV19")).toString());
        }
        category.remove(QStringLiteral("coverageV19"));
    }
    if (categoryId == QLatin1String("pa")) {
        category = mapCategoryControls(category, [version, holdsTransmit](QJsonObject control) {
            const QString id = control.value(QStringLiteral("id")).toString();
            const auto closed = paV14Controls().constFind(id);
            if (closed == paV14Controls().constEnd()) { return control; }
            // Before version 20 a peer keeps the closed version 14 rows.
            if (version < 20) { return *closed; }
            if (id != QLatin1String("pa.gain.table")) { return control; }
            QJsonArray rows = control.value(QStringLiteral("rows")).toArray();
            for (int i = 0; i < rows.size(); ++i) {
                QJsonObject row = rows.at(i).toObject();
                if (row.contains(QLatin1String(kPaHolderMayEdit))) {
                    row.remove(QLatin1String(kPaHolderMayEdit));
                    // The transmit holder edits its band live, as Thetis does.
                    if (holdsTransmit) { row.remove(QStringLiteral("availability")); }
                }
                rows[i] = row;
            }
            control.insert(QStringLiteral("rows"), rows);
            return control;
        });
    }
    // Before version 22 a peer keeps DSP's RX buffer size rows unlocked,
    // as it always read them.
    if (categoryId == QLatin1String("dsp") && version < kDspOnAirLockVersion) {
        category = mapCategoryControls(category, [](QJsonObject control) {
            if (isRxBufferSizeControl(control)) {
                control.remove(QStringLiteral("availability"));
            }
            return control;
        });
    }
    // DSP changed at 15, 19 (CFC's band editor) and 22 (the RX buffer
    // sizes' on-the-air lock): 15 to 18 see 15, 19 to 21 see 19.
    // CAT & Network changed at 15 and 21 (TCI Forget's enabledWhen): 15 to
    // 20 see 15.
    const int ceiling = categoryId == QLatin1String("dsp") && version >= kDspOnAirLockVersion
            ? kDspOnAirLockVersion
        : categoryId == QLatin1String("dsp") && version >= 19 ? 19
        : categoryId == QLatin1String("catNetwork") && version >= 21 ? 21
        // Audio changed at 15 and 24 (Line In Gain's steps, Saturn Mic
        // Tip-Ring): 15 to 23 see 15.
        : categoryId == QLatin1String("audio") && version >= 24 ? 24
        : SetupDescriptionV15::isCategory(categoryId)
            && version >= SetupDescriptionV15::kVersion ? SetupDescriptionV15::kVersion
        // Hardware changed at 16 (HL2 Options), 17 (the Alex-1 low-pass
        // rows), 18 (HL2 Options' clock rows) and 23 (Calibration's Rx1 6m
        // LNA row): 18 to 22 see 18.
        : categoryId == QLatin1String("hardware") ? (version >= 23 ? 23 : 18)
        : categoryId == QLatin1String("transmit") ? 13
        // PA changed at 20 (PA Gain's on-the-air lock per row): 14 to 19
        // see 14.
        : categoryId == QLatin1String("pa") ? (version >= 20 ? 20 : 14)
        : categoryId == QLatin1String("appearance") ? 12
        : categoryId == QLatin1String("display") ? 12 : 3;
    // Appearance changed at 4, 7 and 12: versions 7-11 all see version 7.
    category.insert(QStringLiteral("version"),
                    categoryId == QLatin1String("appearance") && version < 7
                        ? qMin(version, 4)
                        : categoryId == QLatin1String("appearance") && version < 12
                            ? 7
                        : categoryId == QLatin1String("display") && version < 8
                            ? qMin(version, 4)
                        // Hardware changed at 6, 13, 16, 17, 18 and 23, PA at 5 and 13.
                        : categoryId == QLatin1String("hardware") && version < 13
                            ? qMin(version, 6)
                        : categoryId == QLatin1String("hardware") && version < 16
                            ? 13
                        : categoryId == QLatin1String("pa") && version < 13
                            ? qMin(version, 5)
                        // Transmit changed at 13 (Disable HF PA).
                        : categoryId == QLatin1String("transmit") && version < 13
                            ? qMin(version, 3) : qMin(version, ceiling));
    if (categoryId == QLatin1String("transmit") && version < 13) {
        // An older peer keeps the page's coverage it was built for.
        QJsonArray fittedPages = category.value(QStringLiteral("pages")).toArray();
        for (int i = 0; i < fittedPages.size(); ++i) {
            QJsonObject page = fittedPages.at(i).toObject();
            if (page.value(QStringLiteral("id")) == QJsonValue(QStringLiteral("transmit.power"))) {
                page.insert(QStringLiteral("coverage"), QStringLiteral(
                    "partial: ATT on TX uses the stepAtt facade; Tune fixed drive has "
                    "SKU-dependent display conversion; TX TUN Meter and Disable HF PA have "
                    "no Core apply binding"));
                fittedPages[i] = page;
            }
        }
        category.insert(QStringLiteral("pages"), fittedPages);
    }
    // PA changed at 5, 13, 14 and 20; hardware at 6, 13, 16, 17, 18 and 23;
    // transmit at 13; DSP, Transmit, Audio, Diagnostics and CAT & Network at
    // 15; DSP at 19; CAT & Network at 21; Audio at 24.
    if (version >= 2 && version < SetupDescriptionV15::kVersion
        && category.value(QStringLiteral("category")).toObject()
            .value(QStringLiteral("id")) == QJsonValue(QStringLiteral("dsp"))) {
        category.insert(QStringLiteral("coverage"), QStringLiteral(
            "partial: Filter Presets and other listed page controls remain incomplete"));
        QJsonArray fittedPages = category.value(QStringLiteral("pages")).toArray();
        for (int i = 0; i < fittedPages.size(); ++i) {
            QJsonObject page = fittedPages.at(i).toObject();
            if (page.value(QStringLiteral("id")) == QJsonValue(QStringLiteral("dsp.tnf"))) {
                page.insert(QStringLiteral("coverage"), QStringLiteral(
                    "partial: Visual Notch is a phone-local display preference"));
                fittedPages[i] = page;
                break;
            }
        }
        category.insert(QStringLiteral("pages"), fittedPages);
    }
    return QString::fromUtf8(QJsonDocument(category).toJson(QJsonDocument::Compact));
}

SetupDescription::SetupDescription(QObject* parent) : QObject(parent)
{
    initializeSetupResources();
    rebuild();
}

QJsonObject SetupDescription::category(const QString& id) const
{
    const QString* value = nullptr;
    if (id == QLatin1String("general")) { value = &m_general; }
    else if (id == QLatin1String("hardware")) { value = &m_hardware; }
    else if (id == QLatin1String("audio")) { value = &m_audio; }
    else if (id == QLatin1String("dsp")) { value = &m_dsp; }
    else if (id == QLatin1String("display")) { value = &m_display; }
    else if (id == QLatin1String("transmit")) { value = &m_transmit; }
    else if (id == QLatin1String("appearance")) { value = &m_appearance; }
    else if (id == QLatin1String("catNetwork")) { value = &m_catNetwork; }
    else if (id == QLatin1String("test")) { value = &m_test; }
    else if (id == QLatin1String("diagnostics")) { value = &m_diagnostics; }
    else if (id == QLatin1String("pa")) { value = &m_pa; }
    return value == nullptr || value->isEmpty() ? QJsonObject{}
        : QJsonDocument::fromJson(value->toUtf8()).object();
}

void SetupDescription::setBoardCapabilities(const BoardCapabilities& caps)
{
    m_caps = caps;
    m_model = HPSDRModel::FIRST;
    m_radioInfo = RadioInfo{};
    rebuild();
}

void SetupDescription::setRadioContext(const BoardCapabilities& caps, HPSDRModel model)
{
    setRadioContext(caps, model, RadioInfo{});
}

void SetupDescription::setRadioContext(const BoardCapabilities& caps, HPSDRModel model,
                                       const RadioInfo& info)
{
    m_caps = caps;
    m_model = model;
    m_radioInfo = info;
    rebuild();
}

void SetupDescription::rebuild()
{
    bool changed = false;
    const auto update = [this, &changed](const QString& id, QString& target) {
        const QString description = loadCategory(id, m_caps, m_model, m_radioInfo);
        if (description != target) {
            target = description;
            changed = true;
        }
    };
    update(QStringLiteral("general"), m_general);
    update(QStringLiteral("hardware"), m_hardware);
    update(QStringLiteral("audio"), m_audio);
    bool dspChanged = false;
    const QString dsp = dspWithOnAirState(loadCategory(QStringLiteral("dsp"), m_caps, m_model,
                                                       m_radioInfo),
                                          m_dspOnAir);
    if (dsp != m_dsp) {
        m_dsp = dsp;
        changed = true;
        dspChanged = true;
    }
    update(QStringLiteral("display"), m_display);
    update(QStringLiteral("transmit"), m_transmit);
    update(QStringLiteral("appearance"), m_appearance);
    update(QStringLiteral("catNetwork"), m_catNetwork);
    update(QStringLiteral("test"), m_test);
    update(QStringLiteral("diagnostics"), m_diagnostics);
    const QString pa = paWithOnAirState(loadCategory(QStringLiteral("pa"), m_caps, m_model,
                                                     m_radioInfo),
                                        m_paOnAir, m_paTransmittingBand);
    if (pa != m_pa) {
        m_pa = pa;
        changed = true;
    }
    if (changed) {
        ++m_revision;
        emit descriptionsChanged();
        if (dspChanged) { emit dspDescriptionChanged(); }
        emit paDescriptionChanged();
    }
}

bool SetupDescription::applyDspOnAir(bool onAir)
{
    if (onAir == m_dspOnAir) { return false; }
    m_dspOnAir = onAir;
    const QString dsp = dspWithOnAirState(loadCategory(QStringLiteral("dsp"), m_caps, m_model,
                                                       m_radioInfo),
                                          m_dspOnAir);
    if (dsp == m_dsp) { return false; }
    m_dsp = dsp;
    return true;
}

bool SetupDescription::applyPaOnAir(bool onAir, int transmittingBand)
{
    const int band = onAir ? transmittingBand : -1;
    if (onAir == m_paOnAir && band == m_paTransmittingBand) { return false; }
    m_paOnAir = onAir;
    m_paTransmittingBand = band;
    const QString pa = paWithOnAirState(loadCategory(QStringLiteral("pa"), m_caps, m_model,
                                                     m_radioInfo),
                                        m_paOnAir, m_paTransmittingBand);
    if (pa == m_pa) { return false; }
    m_pa = pa;
    return true;
}

void SetupDescription::setOnAirState(bool onAir, int transmittingBand)
{
    const bool paChanged = applyPaOnAir(onAir, transmittingBand);
    const bool dspChanged = applyDspOnAir(onAir);
    if (!paChanged && !dspChanged) { return; }
    // One edge, one revision: PA and DSP are sent again together, and PA
    // carries the revision (its notify), so a peer sees one new revision.
    ++m_revision;
    if (dspChanged) { emit dspDescriptionChanged(); }
    emit paDescriptionChanged();
}

void SetupDescription::setPaOnAirState(bool onAir, int transmittingBand)
{
    if (!applyPaOnAir(onAir, transmittingBand)) { return; }
    ++m_revision;
    emit paDescriptionChanged();
}

void SetupDescription::noteTransmitHolderChanged()
{
    // Off the air nothing depends on the holder.
    if (!m_paOnAir) { return; }
    ++m_revision;
    emit paDescriptionChanged();
}

} // namespace NereusSDR
