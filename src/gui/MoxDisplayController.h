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
//     and span swap, every pan's waterfall AGC level saved for its band on
//     the key and restored on the un-key, and the averaging cleared);
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
//   2026-10-13  J.J. Boyd / KG4VCF  Export the controller's Qt signal
//                 addresses from the Windows GUI DLL. AI-assisted via
//                 OpenAI Codex.
//   2026-10-13 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//                 Central IMD gate mirrors; unchanged TX source ownership.
//   2026-10-05 — J.J. Boyd (KG4VCF). Independent per-pan Clarity ownership.
//                 AI-assisted via OpenAI Codex.
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
//   2026-10-10 : Display pill plan Task 13: both edges reach every pan of
//                 the window (SpectrumWidget::setDisplayKeyed) before the
//                 rise and the fall; the rise no longer takes Clarity off
//                 the pan or re-primes the waterfall AGC, and the fall no
//                 longer re-primes it. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

#pragma once

#include "gui/NereusGuiExport.h"

#include <functional>

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

namespace NereusSDR {

class ITxDisplaySource;
class PanadapterStack;
class PureSignal;
class RadioModel;
class SliceModel;
class SpectrumWidget;
class StationClient;

class NEREUS_GUI_EXPORT MoxDisplayController : public QObject {
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
    /// carrier, the external transmit waterfall on and the averaging
    /// cleared, then the source's transmit view. The fall undoes each on
    /// the pan recorded at the rise.
    ///
    /// Before either, on an edge: keyedChanged, then every pan of the
    /// window is told (SpectrumWidget::setDisplayKeyed, with the pan's own
    /// hold length and its band from setPanBandProvider). Every pan keeps
    /// its Clarity level and its waterfall AGC level through the key.
    void setKeyed(bool keyed, int txSliceId);
    bool isKeyed() const { return m_keyed; }

    /// TX audio check (TX/N): show the transmit panadapter/waterfall while
    /// the radio stays unkeyed. The RF/frequency view is the same as a real
    /// MOX; the transmit state and meters are not.
    void setTxAudioCheckDisplay(bool on, int txSliceId);
    bool isTxAudioCheckDisplay() const { return m_txAudioCheckDisplay; }

    /// Window-local presentation preference, seeded from the existing saved
    /// Show Two-tone Measurements key and current/late coordinator state.
    /// Two-tone truth comes from the local/remote PureSignal session facade.
    /// Mirror both gates onto all current/new pans; only the keyed host has
    /// the MOX/DUP gates that make the measurement visible.
    void setShowIMDMeasurements(bool on);
    bool showIMDMeasurements() const { return m_showIMDMeasurements; }
    bool testingIMD() const { return m_testingIMD; }

    /// The band a pan is on, as the int SpectrumWidget::setDisplayKeyed
    /// keeps its waterfall AGC levels under. With none set, or for a pan
    /// the provider does not know, every band is kNoBandKey.
    static constexpr int kNoBandKey = -1;
    void setPanBandProvider(std::function<int(const QString& panId)> provider);

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
    /// order (the source's too), into callLog(). An edge records
    /// "keyedChanged(true|false)" and one "setDisplayKeyed(true|false)
    /// <panId>" for each pan ahead of them.
    void setCallRecording(bool on);
    QStringList callLog() const { return m_callLog; }
    void clearCallLog() { m_callLog.clear(); }

signals:
    void keyedChanged(bool keyed);
    // GUI ownership is committed after rise/fall, including a late slice snapshot.
    void keyedOwnerCommitted(const QString& panId);
    /// displayDuplex() changed (the setting, or its availability).
    void displayDuplexChanged(bool on);

private:
    // Every pan's peaks reset (SpectrumWidget::resetPeaks): Thetis's
    // PurgeBuffers on a MOX edge and on the radio coming on.
    void resetPeaksOnEveryPan();
    // An edge on every pan (SpectrumWidget::setDisplayKeyed).
    void keyEveryPan(bool keyed);
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
    void bindMeasurementCoordinator(PureSignal* coordinator);
    void applyMeasurementGates();
    void refreshTestingIMD();

    QPointer<PanadapterStack> m_pans;
    QPointer<RadioModel> m_model;
    ITxDisplaySource* m_source{nullptr};
    std::function<int(const QString&)> m_panBand;

    bool m_keyed{false};
    bool m_txAudioCheckDisplay{false};
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
    QPointer<PureSignal> m_measurementCoordinator;
    QMetaObject::Connection m_measurementsConnection;
    bool m_testingIMD{false};
    bool m_showIMDMeasurements{false};

    bool m_highSwr{false};
    bool m_windBackLatched{false};
    QPointer<SpectrumWidget> m_swrPan;

    bool m_recording{false};
    QStringList m_callLog;
};

} // namespace NereusSDR
