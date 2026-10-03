// =================================================================
// src/gui/MoxDisplayController.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. The one owner of what a window's pan
// does while its radio is keyed, for a window running its own DSP and for a
// remote window alike. It cites Thetis's display for the behaviour it
// follows; the widget work it orders already lives in SpectrumWidget and
// the transmit sources, and it translates no Thetis code.
//
// What it follows, Thetis v2.10.3.15:
//   - display.cs:1575-1597 (Display.MOX): the MOX edge swaps the cached
//     waterfall minimum and purges the buffers (here: setMoxOverlay's grid
//     and span swap, the waterfall AGC re-prime and the averaging cleared);
//   - display.cs:1782-1790 (SpectrumGridMaxMoxModified): the transmit grid
//     while keyed (setMoxOverlay loads it);
//   - display.cs:6420-6427: the waterfall's own transmit levels
//     (TXWFAmpMin / TXWFAmpMax, -70 and 30 at display.cs:1917-1937), the
//     transmit colour scheme and the transmit low colour while keyed (the
//     widget's m_moxOverlay branch);
//   - console.cs:24281-24338 (DisplayThread): the transmitting receiver's
//     display takes the transmit analyzer, never the receiver, unless
//     display duplex (DUP) is on: then it keeps the receiver;
//   - console.cs:22069-22150 (getLowHighForRXn): the display follows XIT
//     while transmitting (carrierChanged), except with DUP on;
//   - console.cs:15390-15395 (_display_duplex, false by default) and
//     console.cs:37555-37575 (chkRX2SR_CheckedChanged, "chkRX2SR is the
//     DUPlex button"): DUP; display.cs:514-521: a change while keyed
//     resets the peaks (the widget's setDisplayDuplex);
//   - display.cs:4619-4629 (isRxDuplex): DUP acts on the first receiver
//     only; here, the pan hosting the transmit slice.
//
// Modification history (NereusSDR):
//   2026-09-26 : Created for remote-window parity Task 29 (A11, R-R3-49,
//                 R-R3-12, verification row 16) by J.J. Boyd (KG4VCF): the
//                 rise and fall MainWindow's MOX lambda made, moved here so
//                 a remote window makes the same ones. AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-27 : Parity Task 31 (A11, R-R3-49): display duplex (DUP). With
//                 it on the rise puts only the overlay on the pan (red
//                 border, transmit grid and waterfall levels) and the pan
//                 keeps the receiver; a change while keyed swaps the view.
//                 In a remote window DUP needs the Core's txDisplayVersion
//                 3. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
// =================================================================

#pragma once

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

namespace NereusSDR {

class ITxDisplaySource;
class PanadapterStack;
class RadioModel;
class SliceModel;
class SpectrumWidget;
class StationClient;

class MoxDisplayController : public QObject {
    Q_OBJECT

public:
    MoxDisplayController(PanadapterStack* pans, RadioModel* model, QObject* parent);
    ~MoxDisplayController() override;

    /// Not owned. Set before the first key; a source set while keyed takes
    /// over at the next rise.
    void setSource(ITxDisplaySource* source);
    ITxDisplaySource* source() const { return m_source; }

    /// The rise and fall. On the rise, on the pan hosting slice
    /// `txSliceId` (never the active pan, never a fallback): the MOX
    /// overlay (red border, transmit grid, palette and waterfall levels),
    /// the receive rate and DDC centre saved, the view moved to the
    /// carrier, Clarity paused, the external transmit waterfall on, the
    /// waterfall AGC re-primed and the averaging cleared, then the source's
    /// transmit view. The fall undoes each on the pan recorded at the rise.
    void setKeyed(bool keyed, int txSliceId);
    bool isKeyed() const { return m_keyed; }

    /// The pan showing the transmit display; empty unless keyed with DUP
    /// off (keyed with DUP on the pan keeps its receive frames).
    QString transmitPanId() const { return m_transmitView ? m_panId : QString(); }
    /// The pan the rise took over (the overlay's pan), DUP on or off; empty
    /// unless keyed.
    QString keyedPanId() const { return m_panId; }

    /// Parity Task 31 (A11): display duplex (DUP), the window's setting
    /// DisplayDuplex. displayDuplex() is what the pan does: the setting,
    /// unless DUP is unavailable (a remote window whose Core is below
    /// txDisplayVersion 3), when the pan behaves as DUP off. Changed while
    /// keyed, the transmitting pan swaps between the transmit view and the
    /// receiver's at once.
    void setDisplayDuplex(bool on);
    bool displayDuplex() const { return m_wantDuplex && displayDuplexAvailable(); }
    bool displayDuplexAvailable() const;
    /// Why DUP is unavailable here, in the operator's words; empty when it
    /// is available. A remote window on a Core below txDisplayVersion 3.
    static QString displayDuplexUnavailableReason(const RadioModel* model);
    /// The window's setting DisplayDuplex ("True" / "False", "False" by
    /// default as Thetis), in this computer's AppSettings: DUP is this
    /// window's own, never the Core's.
    static bool savedDisplayDuplex();
    static void saveDisplayDuplex(bool on);

    /// Row 16: the transmit carrier moved while keyed (XIT, a retune). The
    /// view, the transmit filter overlay and the trace move with it.
    void carrierChanged(double carrierHz);
    double carrierHz() const { return m_carrierHz; }

    /// The high-SWR border (with fold-back when latched) on the
    /// transmitting pan, as RadioModel drives it on a local window.
    void setHighSwr(bool highSwr, bool windBackLatched);

    /// A window running its own DSP: follows MoxController::moxStateChanged
    /// with RadioModel::txBoundSlice().
    void followLocalRadio();
    /// A remote window: follows the Core's `txState` (keyed, txSliceId,
    /// highSwr, swrWindBackLatched) or, on a Core that sends none, the
    /// mirrored radio.transmitting and the slice whose txSlice is true.
    void followStation(StationClient* client);

    /// Test seam: record each widget call of the rise and the fall, in
    /// order (the source's too), into callLog().
    void setCallRecording(bool on);
    QStringList callLog() const { return m_callLog; }
    void clearCallLog() { m_callLog.clear(); }

signals:
    /// displayDuplex() changed (the setting, or its availability).
    void displayDuplexChanged(bool on);

private:
    // Every pan's peaks reset (SpectrumWidget::resetPeaks): Thetis's
    // PurgeBuffers on a MOX edge and on the radio coming on.
    void resetPeaksOnEveryPan();
    void rise(int txSliceId);
    void fall();
    // The transmit view's half of the rise and the fall, on the pan the
    // rise took over: DUP off at the rise, or DUP turned off while keyed;
    // and back.
    void beginTransmitView(SpectrumWidget* sw);
    void endTransmitView(SpectrumWidget* sw);
    void applyDisplayDuplex();
    void onViewWindowChanged(double centreHz, double bandwidthHz);
    void applyHighSwr();
    void note(const QString& call);

    QPointer<PanadapterStack> m_pans;
    QPointer<RadioModel> m_model;
    ITxDisplaySource* m_source{nullptr};

    bool m_keyed{false};
    // Parity Task 31: the window's DUP setting; what the pan did last
    // (displayDuplex() when it was applied); whether the transmit view runs.
    bool m_wantDuplex{false};
    bool m_appliedDuplex{false};
    bool m_transmitView{false};
    QString m_panId;
    QPointer<SpectrumWidget> m_pan;
    QPointer<SliceModel> m_slice;
    double m_carrierHz{0.0};
    // The receive-side bin mapping, saved on the rise: the receive rate is
    // the wire DDC rate while the transmit display's is its own window, and
    // the DDC may sit off the VFO under CTUN.
    double m_savedSampleRate{0.0};
    double m_savedDdcHz{0.0};
    QMetaObject::Connection m_viewConnection;

    bool m_highSwr{false};
    bool m_windBackLatched{false};
    QPointer<SpectrumWidget> m_swrPan;

    bool m_recording{false};
    QStringList m_callLog;
};

} // namespace NereusSDR
