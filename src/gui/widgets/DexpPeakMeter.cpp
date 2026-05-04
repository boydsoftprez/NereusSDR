// =================================================================
// src/gui/widgets/DexpPeakMeter.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original Qt6 paint impl.  Thetis uses
// System.Drawing.Graphics; this is a fresh Qt6/QPainter
// implementation of the same visual recipe (LimeGreen peak fill +
// red threshold marker + above-threshold red overlay).  The Thetis
// excerpts below are commentary so the next maintainer can audit
// the visual fidelity, not ported code.
//
// NereusSDR-native paint impl mirroring Thetis picVOX_Paint /
// picNoiseGate_Paint visual behavior.  See header for full
// attribution + modification history.
//
// Reference Thetis paint behavior (verbatim, for the porter):
//
//   // From console.cs:28949-28960 [v2.10.3.13] — picVOX_Paint
//   unsafe private void picVOX_Paint(object sender, ...PaintEventArgs e)
//   {
//       double audio_peak = 0.0;
//       cmaster.GetDEXPPeakSignal(0, &audio_peak);
//       if (mic_boost) audio_peak /= Audio.VOXGain;
//       int peak_x = (int)(picVOX.Width
//           - 20.0 * Math.Log10(audio_peak) * picVOX.Width / ptbVOX.Minimum);
//       int vox_x  = picVOX.Width - ptbVOX.Value * picVOX.Width / ptbVOX.Minimum;
//       if (peak_x < 0) peak_x = vox_x = 0;
//       e.Graphics.FillRectangle(Brushes.LimeGreen, 0, 0, peak_x, picVOX.Height);
//       if (vox_x < peak_x)
//           e.Graphics.FillRectangle(Brushes.Red,
//               vox_x + 1, 0, peak_x - vox_x - 1, picVOX.Height);
//   }
//
//   // From console.cs:28972-28981 [v2.10.3.13] — picNoiseGate_Paint
//   private void picNoiseGate_Paint(object sender, ...PaintEventArgs e)
//   {
//       int signal_x = (int)((noise_gate_data + 160.0)
//           * (picNoiseGate.Width - 1) / 160.0);
//       int noise_x  = (int)(((float)ptbNoiseGate.Value + 160.0)
//           * (picNoiseGate.Width - 1) / 160.0);
//       if (!_mox) signal_x = noise_x = 0;
//       e.Graphics.FillRectangle(Brushes.LimeGreen, 0, 0, signal_x, picNoiseGate.Height);
//       if (noise_x < signal_x)
//           e.Graphics.FillRectangle(Brushes.Red,
//               noise_x + 1, 0, signal_x - noise_x - 1, picNoiseGate.Height);
//   }
//
// Both Thetis paints share the identical visual recipe:
//   1. Green peak fill from x=0 to signal_x.
//   2. If threshold_x < signal_x, red overlay from threshold_x+1
//      to signal_x (overwrites the green segment above-threshold).
// The only differences are domain mapping (dB vs linear) and the
// MOX gate.  NereusSDR delegates BOTH of those to the caller via
// the normalized 0..1 setters, so this widget is pure "draw a
// peak strip" with no radio-state coupling.
//
// =================================================================

#include "DexpPeakMeter.h"

#include <QPaintEvent>
#include <QPainter>

#include <algorithm>

namespace NereusSDR {

namespace {

// NereusSDR-original palette per plan §9.1.  Hex callouts kept in
// the trailing comment so a designer audit can grep for them.
constexpr QColor kBg            { 14,  17,  21};   // #0e1115 — strip background
constexpr QColor kBorder        { 24,  28,  32};   // #181c20 — 1 px frame
constexpr QColor kGreenPeak     { 76, 208,  72};   // #4cd048 — LimeGreen analogue
constexpr QColor kRedOverlay    {255,  58,  58};   // #ff3a3a — above-threshold
constexpr QColor kRedThreshLine {255,  58,  58};   // #ff3a3a — marker line

} // namespace

DexpPeakMeter::DexpPeakMeter(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(4);
    setMaximumHeight(8);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void DexpPeakMeter::setSignalLevel(double normalized01)
{
    const double clamped = std::clamp(normalized01, 0.0, 1.0);
    if (qFuzzyCompare(clamped + 1.0, m_signal01 + 1.0)) { return; }
    m_signal01 = clamped;
    update();
}

void DexpPeakMeter::setThresholdMarker(double normalized01)
{
    const double clamped = std::clamp(normalized01, 0.0, 1.0);
    if (qFuzzyCompare(clamped + 1.0, m_threshold01 + 1.0)) { return; }
    m_threshold01 = clamped;
    update();
}

void DexpPeakMeter::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int W = width();
    const int H = height();

    // Background + 1 px border (matches NereusSDR slider track styling).
    p.fillRect(rect(), kBg);
    p.setPen(kBorder);
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    const int innerLeft  = 1;
    const int innerRight = W - 1;
    const int innerW     = innerRight - innerLeft;
    const int innerTop   = 1;
    const int innerBot   = H - 1;

    if (innerW <= 0 || innerBot <= innerTop) { return; }

    const int signalX    = innerLeft + static_cast<int>(m_signal01    * innerW);
    const int thresholdX = innerLeft + static_cast<int>(m_threshold01 * innerW);

    // Step 1 — Green peak fill, mirrors picVOX_Paint:28957:
    //   FillRectangle(Brushes.LimeGreen, 0, 0, peak_x, picVOX.Height);
    if (signalX > innerLeft) {
        p.fillRect(QRect(innerLeft, innerTop,
                         signalX - innerLeft, innerBot - innerTop),
                   kGreenPeak);
    }

    // Step 2 — Red above-threshold overlay, mirrors picVOX_Paint:28958-28959:
    //   if (vox_x < peak_x)
    //       FillRectangle(Brushes.Red, vox_x + 1, 0,
    //                     peak_x - vox_x - 1, picVOX.Height);
    if (thresholdX < signalX) {
        p.fillRect(QRect(thresholdX + 1, innerTop,
                         signalX - thresholdX - 1, innerBot - innerTop),
                   kRedOverlay);
    }

    // Step 3 — Red threshold marker (1 px vertical line, full height
    // including the border so it's visible even when the threshold
    // sits right at the edge).  No direct Thetis equivalent — the
    // C# paint relies on the slider thumb landing right next to the
    // strip to imply the threshold position.  Adding an explicit line
    // is a NereusSDR-original UX nudge so the threshold position is
    // unambiguous when the strip is read in isolation.
    p.setPen(kRedThreshLine);
    p.drawLine(thresholdX, 0, thresholdX, H - 1);
}

} // namespace NereusSDR
