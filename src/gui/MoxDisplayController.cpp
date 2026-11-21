// =================================================================
// src/gui/MoxDisplayController.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. See MoxDisplayController.h.
//
// Modification history (NereusSDR):
//   2026-10-13 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//                 Mirror current local/remote IMD presentation gates to
//                 existing/new pans and disconnect replaced coordinators.
//   2026-10-13 — J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//                 Restore current XIT when releasing a transmit view, so
//                 Tune's temporary NCO offset cannot survive into SSB.
//   2026-10-05 — J.J. Boyd (KG4VCF). Independent per-pan Clarity ownership.
//                 AI-assisted via OpenAI Codex.
//   2026-09-26 : Created for remote-window parity Task 29 (A11, R-R3-49,
//                 R-R3-12, verification row 16) by J.J. Boyd (KG4VCF). The
//                 rise and fall, and their comments, moved here from
//                 MainWindow's MoxController::moxStateChanged lambda (PR
//                 #212 follow-up and the 3M-5 revive, 2026-05-07 to
//                 2026-08-09) so a remote window makes the same ones.
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27 : Parity Task 31 (A11, R-R3-49): display duplex (DUP): the
//                 rise and fall split into the overlay and the transmit
//                 view, the view left out with DUP on and swapped when DUP
//                 changes while keyed. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-28 : Thetis's peak resets on every MOX edge and on the radio
//                 connecting reach every pan of the window, local or
//                 remote. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-04 : Resolve a remote slice's actual window host when its Core
//                 pan key does not name a window pan, and complete a rise
//                 once the initial TX slice snapshot follows the keyed state.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-10 : Display pill plan Task 13: both edges reach every pan
//                 (keyEveryPan) before the rise and the fall. The rise no
//                 longer takes Clarity off the transmitting pan and neither
//                 edge re-primes the waterfall AGC: each pan keeps its level
//                 through the key. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

#include "gui/MoxDisplayController.h"

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/MoxController.h"
#include "core/PureSignal.h"
#include "core/session/PureSignalSessionFacade.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/TransmitStateFacade.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/SpectrumWidget.h"
#include "gui/TxDisplaySource.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QtGlobal>

#include <cmath>

namespace NereusSDR {

namespace {

// Nothing exists outside the transmit siphon's baseband: the TX DSP rate,
// 96 kHz (WdspEngine::kTxDspSampleRate = cmaster.c:182 [v2.10.3.13]),
// either side of the carrier. TxAnalyzer::clampViewToBaseband and the
// Core's TxDisplayFeed hold the analyzer to the same edges.
constexpr double kHalfBasebandHz = 48000.0;

} // namespace

MoxDisplayController::MoxDisplayController(PanadapterStack* pans, RadioModel* model,
                                           QObject* parent)
    : QObject(parent)
    , m_pans(pans)
    , m_model(model)
{
    m_showIMDMeasurements = AppSettings::instance().value(
        QStringLiteral("puresignal/showTwoToneMeasurements"), false).toBool();
    if (pans != nullptr) {
        connect(pans, &PanadapterStack::countChanged, this,
                &MoxDisplayController::applyMeasurementGates);
    }
    // Parity Task 31: a remote window's DUP follows the Core's
    // txDisplayVersion (3 and above).
    if (model != nullptr) {
        connect(model, &RadioModel::pureSignalCoordinatorReady, this,
                &MoxDisplayController::bindMeasurementCoordinator);
        if (PureSignalSessionFacade* facade = model->pureSignalFacade()) {
            connect(facade, &PureSignalSessionFacade::statusChanged, this,
                    &MoxDisplayController::refreshTestingIMD);
        }
        if (model->pureSignal() != nullptr) {
            bindMeasurementCoordinator(model->pureSignal());
        }
        connect(model, &RadioModel::stationTxDisplayVersionChanged, this,
                &MoxDisplayController::applyDisplayDuplex);
        // The radio coming on resets every receiver's peaks. From Thetis
        // display.cs:818-823 [v2.10.3.15]:
        //   private static void OnPowerChangeHander(bool oldPower, bool newPower)
        //   {
        //       if (newPower)
        //       {
        //            PurgeBuffers();
        // (PurgeBuffers, display.cs:11630-11639 [v2.10.3.15], clears both
        // receivers' buffers; clearBuffers, 2835-2837, starts with
        // resetPeaksAndNoise.)
        // A local radio and a Core's radio both report it here.
        connect(model, &RadioModel::connectionStateChanged, this,
                [this](ConnectionState state) {
                    if (state == ConnectionState::Connected) {
                        resetPeaksOnEveryPan();
                    }
                });
    }
    refreshTestingIMD();
}

void MoxDisplayController::bindMeasurementCoordinator(PureSignal* coordinator)
{
    if (m_measurementCoordinator == coordinator) {
        return;
    }
    QObject::disconnect(m_measurementsConnection);
    m_measurementCoordinator = coordinator;
    if (coordinator != nullptr) {
        m_measurementsConnection = connect(coordinator, &PureSignal::show2ToneMeasurementsChanged,
                                           this, &MoxDisplayController::setShowIMDMeasurements);
    }
    // PsForm owns this window-local saved preference. The legacy coordinator
    // getter is an unpersisted mirror, so its startup false cannot reset a
    // saved true (and a saved false also wins). Use the snapshot only when
    // there is no saved preference; keep future explicit legacy signals.
    const QString key = QStringLiteral("puresignal/showTwoToneMeasurements");
    AppSettings& settings = AppSettings::instance();
    setShowIMDMeasurements(settings.contains(key) ? settings.value(key).toBool()
        : coordinator != nullptr && coordinator->show2ToneMeasurements());
}

void MoxDisplayController::setShowIMDMeasurements(bool on)
{
    m_showIMDMeasurements = on;
    applyMeasurementGates();
}

void MoxDisplayController::refreshTestingIMD()
{
    PureSignalSessionFacade* facade = m_model ? m_model->pureSignalFacade() : nullptr;
    m_testingIMD = facade != nullptr && facade->twoToneOn();
    applyMeasurementGates();
}

void MoxDisplayController::applyMeasurementGates()
{
    if (m_pans.isNull()) {
        return;
    }
    for (PanadapterApplet* applet : m_pans->allApplets()) {
        if (SpectrumWidget* spectrum = applet != nullptr ? applet->spectrumWidget() : nullptr) {
            spectrum->setTestingIMD(m_testingIMD);
            spectrum->setShowIMDMeasurements(m_showIMDMeasurements);
        }
    }
}

void MoxDisplayController::resetPeaksOnEveryPan()
{
    if (m_pans.isNull()) {
        return;
    }
    for (PanadapterApplet* applet : m_pans->allApplets()) {
        if (SpectrumWidget* sw = applet != nullptr ? applet->spectrumWidget() : nullptr) {
            sw->resetPeaks();
        }
    }
}

void MoxDisplayController::setPanBandProvider(std::function<int(const QString& panId)> provider)
{
    m_panBand = std::move(provider);
}

void MoxDisplayController::keyEveryPan(bool keyed)
{
    if (m_pans.isNull()) {
        return;
    }
    // NereusSDR, based on Thetis display.cs:7651 [v2.10.3.15]: Thetis has
    // one waterfall minimum for each receiver and stops following it while
    // that receiver's display shows the transmitter. Here every pan of the
    // window has its own follower and the receiver hears the transmitter on
    // all of them, so every pan's level is frozen for the key, not only the
    // transmitting pan's.
    //
    // NereusSDR, based on Thetis display.cs:5912 and 7640-7651
    // [v2.10.3.15]: after the un-key each pan holds its level for its own
    // hold length (SpectrumWidget::moxHoldLengthMs: twice its FFT's fill
    // time, a second at least) and takes no reading from the frames in it.
    // Thetis delays its waterfall AGC for one fill time and lets the
    // noise-floor fast attack ride out the rest; here the hold and the fast
    // attack are one length, because the purged FFT input is still
    // refilling for that long.
    for (PanadapterApplet* applet : m_pans->allApplets()) {
        SpectrumWidget* sw = applet != nullptr ? applet->spectrumWidget() : nullptr;
        if (sw == nullptr) {
            continue;
        }
        const QString panId = applet->panId();
        const int band = m_panBand ? m_panBand(panId) : kNoBandKey;
        sw->setDisplayKeyed(keyed, sw->moxHoldLengthMs(), band);
        note(QStringLiteral("setDisplayKeyed(%1) %2")
                 .arg(keyed ? QStringLiteral("true") : QStringLiteral("false"), panId));
    }
}

QString MoxDisplayController::displayDuplexUnavailableReason(const RadioModel* model)
{
    if (model != nullptr && model->role() == RadioModel::Role::Remote
        && model->stationTxDisplayVersion() < 3) {
        return QStringLiteral("This Core does not show the receiver while transmitting "
                              "for this app. Updating the Core may help.");
    }
    return {};
}

bool MoxDisplayController::savedDisplayDuplex()
{
    return AppSettings::instance()
               .value(QStringLiteral("DisplayDuplex"), QStringLiteral("False"))
               .toString() == QStringLiteral("True");
}

void MoxDisplayController::saveDisplayDuplex(bool on)
{
    AppSettings::instance().setValue(QStringLiteral("DisplayDuplex"),
                                     on ? QStringLiteral("True") : QStringLiteral("False"));
}

bool MoxDisplayController::displayDuplexAvailable() const
{
    return displayDuplexUnavailableReason(m_model.data()).isEmpty();
}

void MoxDisplayController::setDisplayDuplex(bool on)
{
    m_wantDuplex = on;
    applyDisplayDuplex();
}

void MoxDisplayController::applyDisplayDuplex()
{
    const bool duplex = displayDuplex();
    if (duplex == m_appliedDuplex) {
        return;
    }
    m_appliedDuplex = duplex;
    // First, so a remote window's subscriptions carry the new `duplex`
    // before the view below asks the Core again.
    emit displayDuplexChanged(duplex);
    SpectrumWidget* sw = m_pan.data();
    if (!m_keyed || sw == nullptr) {
        return;
    }
    // Changed while keyed (Thetis Display.DisplayDuplex, display.cs:514-521
    // [v2.10.3.15], "just incase dup is changed whilst tx'ing"): the widget
    // resets its peaks and swaps the span; the transmit view starts or
    // stops here, so the pan changes over within one frame period.
    sw->setDisplayDuplex(duplex);
    note(QStringLiteral("setDisplayDuplex"));
    if (duplex && m_transmitView) {
        endTransmitView(sw);
    } else if (!duplex && !m_transmitView) {
        beginTransmitView(sw);
    }
}

MoxDisplayController::~MoxDisplayController()
{
    QObject::disconnect(m_viewConnection);
}

void MoxDisplayController::setSource(ITxDisplaySource* source)
{
    if (m_source != nullptr) {
        m_source->setCarrierSink({});
        m_source->setCallLog(nullptr);
    }
    m_source = source;
    if (m_source != nullptr) {
        m_source->setCarrierSink([this](double carrierHz) { carrierChanged(carrierHz); });
        m_source->setCallLog(m_recording ? &m_callLog : nullptr);
    }
}

void MoxDisplayController::setCallRecording(bool on)
{
    m_recording = on;
    if (m_source != nullptr) {
        m_source->setCallLog(on ? &m_callLog : nullptr);
    }
}

void MoxDisplayController::note(const QString& call)
{
    if (m_recording) {
        m_callLog.append(call);
    }
}

void MoxDisplayController::setKeyed(bool keyed, int txSliceId)
{
    if (keyed == m_keyed) {
        // The Core mirrors TX state before the slice objects. Joining an
        // already-keyed station can therefore try the rise with no slice
        // yet. Complete it once the snapshot is ready, without moving a pan
        // already taken over (even one whose widget was destroyed during
        // this key).
        if (keyed && m_panId.isEmpty()) {
            rise(txSliceId);
            applyHighSwr();
            emit keyedOwnerCommitted(m_panId);
        }
        return;
    }
    m_keyed = keyed;
    const QPointer<MoxDisplayController> self(this);
    // First on an edge, ahead of every pan's own work: the window purges
    // its FFT input on this signal.
    //
    // NereusSDR, based on Thetis console.cs:24243-24254 [v2.10.3.15]:
    // Thetis purges only when _wdsp_mox_transition_buffer_clear is set
    // (console.cs:24132, false by default). The window always purges, so a
    // long FFT never draws transmit samples into the receive picture.
    note(keyed ? QStringLiteral("keyedChanged(true)") : QStringLiteral("keyedChanged(false)"));
    emit keyedChanged(keyed);
    if (!self) { return; }
    // A MOX edge resets every receiver's peaks, not only the transmitting
    // one's. From Thetis display.cs:1582-1593 [v2.10.3.15] (Display.MOX):
    //   if (value != _old_mox)
    //   {
    //       ...
    //       PurgeBuffers();
    // and console.cs:24243-24254 [v2.10.3.15] (the display thread; the
    // block above it carries //MW0LGE_21g):
    //   if (bLocalMox != bOldLocalMox)
    //   {
    //       // if the mox state is different, reset the analyzer to remove
    //       // possibilty of tx data being in the rx buffers, and vice versa
    //       ...
    //       Display.PurgeBuffers();
    resetPeaksOnEveryPan();
    keyEveryPan(keyed);
    if (keyed) {
        rise(txSliceId);
    } else {
        fall();
    }
    applyHighSwr();
    emit keyedOwnerCommitted(m_panId);
}

// TX audio check (TX/N): the same transmit panadapter/waterfall presentation
// as a real MOX, but the radio is not keyed. Thetis did this with
// Display.TXAudioCheckMode (display.cs), which the display thread treated as
// a local MOX for the TX analyzer while console.MOX stayed false.
void MoxDisplayController::setTxAudioCheckDisplay(bool on, int txSliceId)
{
    if (on == m_txAudioCheckDisplay) {
        return;
    }
    m_txAudioCheckDisplay = on;
    note(on ? QStringLiteral("txAudioCheckDisplay(true)")
            : QStringLiteral("txAudioCheckDisplay(false)"));
    resetPeaksOnEveryPan();
    keyEveryPan(on);
    if (on) {
        rise(txSliceId);
    } else {
        fall();
    }
    applyHighSwr();
}

// ── PR #212 follow-up bench fix (J.J. KG4VCF, 2026-05-07) ────────
// MOX-aware panadapter source switch: FFTEngine (RX-DDC FFTW3) ↔
// TxAnalyzer (WDSP TX siphon, pre-IQC).
//
// From Thetis console.cs:24399-24462 [v2.10.3.13] DisplayThread2.
// Author tags carried verbatim from inside that range, per
// CLAUDE.md 'Inline comment preservation'. They sit on sibling
// switch arms of the same DisplayThread2 dispatch rather than on
// the GetPixels lines quoted below, so they are grouped here:
//   // MW0LGE                                    [console.cs:24429]
//   //[2.10.3.4]MW0LGE not used anymore since scope was coded in cmaster.cs
//                                                [console.cs:24450]
//   // MW0LGE would be null if audio not running (ie not connected?)
//                                                [console.cs:24465]
//   if (bLocalMox && !_display_duplex)
//       SpecHPSDRDLL.GetPixels(cmaster.inid(1, 0), 0, ptr, ref flag);
//   else
//       SpecHPSDRDLL.GetPixels(0, 0, ptr, ref flag);
//
// We achieve the same effect by switching the pan's source on the MOX edge
// rather than runtime branching in a polling loop: on the rise the pan
// hosting the transmitting slice takes its trace and waterfall from the
// transmit source (this computer's analyzer through TxDisplayFeed, or the
// Core's transmit frames in a remote window) and its receive frames are
// held; the fall reverses it. Task 29 moved this here from MainWindow's
// MoxController::moxStateChanged lambda so both windows make the same
// calls in the same order.
void MoxDisplayController::rise(int txSliceId)
{
    // Resolve the target pan HERE, on the MOX edge, rather than capturing a
    // widget when the controller was made. "Whichever pan is active" is the
    // wrong answer: the TX picture belongs on the pan hosting the
    // transmitting slice.
    //
    // Deliberately NOT MainWindow::spectrumForSlice(): that helper falls
    // back to activeSpectrumWidget() when the pan does not resolve, which
    // would paint the transmit trace onto an unrelated pan. A Core's slice
    // can have an empty key, or name a pan absent from this window; its
    // actual hosted-slice association is evidence of where to show it.
    SliceModel* txSlice = m_model ? m_model->sliceById(txSliceId) : nullptr;
    if (txSlice == nullptr || m_pans.isNull()) {
        return;
    }
    QString panId = txSlice->panKey();
    SpectrumWidget* sw = m_pans->spectrum(panId);
    if (sw == nullptr) {
        PanadapterApplet* host = nullptr;
        for (PanadapterApplet* applet : m_pans->allApplets()) {
            if (applet == nullptr || !applet->associatedSlices().contains(txSliceId)) {
                continue;
            }
            // An ambiguous host must not pick whichever QHash entry wins.
            if (host != nullptr) {
                return;
            }
            host = applet;
        }
        if (host != nullptr) {
            panId = host->panId();
            sw = host->spectrumWidget();
        }
    }
    if (sw == nullptr) {
        return;
    }

    // Remember which pan was taken over, so un-key restores THIS one even
    // if the TX binding moved mid-transmission.
    //
    // It also suppresses RX frames for this pan while TX owns it: a local
    // window's dispatchFftFrameToPans skips transmitPanId(), and a remote
    // window's source holds the pan's receive frames. On a 1-ADC HERMES
    // class radio this is a no-op during PureSignal transmit -- the radio
    // has stopped streaming, so no RX frame arrives to suppress. It matters
    // on the ORION class, where RX1 keeps its DDC right through transmit
    // (Thetis console.cs:8265-8272 [v2.10.3.15], DDCEnable = DDC0 + DDC2
    // with Rate[2] = rx1_rate). There the receiver really is still
    // delivering, and without this the RX and TX traces would alternate
    // frame by frame on the same pan. A remote window on a Core that sends
    // no transmit display is the same case: the receiver hearing its own
    // transmitter is a full-width band the waterfall must never take.
    m_panId = panId;
    m_pan = sw;
    m_slice = txSlice;

    // Parity Task 31: DUP acts on this pan (Thetis isRxDuplex,
    // display.cs:4619-4629 [v2.10.3.15]: the first receiver only). Set
    // before the overlay, which keeps the receive span when it is on.
    sw->setDisplayDuplex(displayDuplex());

    // Red border / TX palette / TX grid / TX waterfall levels all key off
    // this flag, on the pan that is actually transmitting. The transmit
    // grid is SpectrumWidget's own business: setMoxOverlay(true) parks the
    // receive pair and loads the transmit one, mirroring Thetis's
    // SpectrumGridMaxMoxModified (display.cs:1782-1804 [v2.10.3.15]). It
    // reads localMox only, so DUP on keeps the transmit grid and waterfall
    // levels too.
    sw->setMoxOverlay(true);
    note(QStringLiteral("setMoxOverlay(true)"));

    // The TRANSMITTING slice, not the active one, and its TRANSMIT
    // frequency, not its dial frequency (see beginTransmitView).
    m_carrierHz = static_cast<double>(m_model->txFrequencyForSlice(txSlice));

    // Parity Task 31: with DUP on the pan keeps the receiver (Thetis's
    // DisplayThread, console.cs:24281-24338 [v2.10.3.15],
    // `if (bLocalMox && !_display_duplex)` takes the transmit analyzer
    // only with DUP off): the overlay only.
    if (!displayDuplex()) {
        beginTransmitView(sw);
    }
}

void MoxDisplayController::beginTransmitView(SpectrumWidget* sw)
{
    m_transmitView = true;

    // Save just the two settings the TX path actually needs to flip:
    //   sampleRate  -- RX is at the wire DDC rate (768k Saturn / 192k HL2);
    //                  TX is the transmit window (WdspEngine::
    //                  kTxDspSampleRate = cmaster.c:182 [v2.10.3.13]).
    //   ddcCenterHz -- RX DDC may be off-VFO under CTUN; TX FFT is centered
    //                  on the carrier.
    // The receive centre and span come back through setMoxOverlay(false)'s
    // swap, same as the grid.
    m_savedSampleRate = sw->sampleRate();
    m_savedDdcHz = sw->ddcCenterFrequency();
    note(QStringLiteral("save sampleRate and ddcCenterFrequency"));

    // The TRANSMITTING slice, not the active one, and its TRANSMIT
    // frequency, not its dial frequency. Centring on activeSlice puts the
    // trace at the receive-focused slice's frequency whenever TX has been
    // handed elsewhere; centring on slice->frequency() puts it at the RX
    // VFO whenever XIT is on, while the radio transmits at the shifted
    // carrier. txFrequencyForSlice owns that arithmetic (dial + XIT when
    // enabled), so the display and the transmitter cannot disagree about
    // where the carrier is. Both found by Codex on PR #317.
    if (!m_slice.isNull() && !m_model.isNull()) {
        m_carrierHz = static_cast<double>(m_model->txFrequencyForSlice(m_slice.data()));
    }

    // ── The transmit window ──────────────────────────────────────
    //
    // From Thetis display.cs:1284-1295 + :4585-4590
    // [v2.10.3.15]:
    //     private static int tx_display_low  = -4000;
    //     private static int tx_display_high =  4000;
    //     if (local_mox) {
    //         if (!displayduplex) {
    //             Low  = tx_display_low;
    //             High = tx_display_high;
    //         }
    // The panadapter shows a fixed +/-4 kHz around the carrier while
    // transmitting the first time (TxDisplayFeed::kDefaultLowHz / HighHz,
    // and SpectrumWidget's transmit span seed); a zoom made while keyed is
    // remembered by setMoxOverlay for the next key.
    //
    // Centre on the carrier, but keep whatever SPAN setMoxOverlay just
    // loaded from the transmit store. Forcing the +/-4 kHz seed here is
    // what made a zoom made during one transmission vanish on the next
    // (bench 2026-08-05): the seed is a first-run default, not something to
    // re-impose every key-up.
    sw->setDisplayWindowPreservingHistory(m_carrierHz, sw->bandwidth());
    note(QStringLiteral("setDisplayWindowPreservingHistory(carrier)"));

    // Clarity stays on the pan through the key (#359). The transmit
    // waterfall does not use its level: with the MOX overlay on, the pan
    // draws with its own transmit levels (setMoxOverlay(true) above, Thetis
    // TXWFAmpMin / TXWFAmpMax, display.cs:6420-6427 [v2.10.3.15]), so the
    // receive level Clarity found is neither shown nor lost. It is kept as
    // it was, PanClarityRegistry takes no reading while keyed or in the
    // hold after it, and the first receive row after the un-key is drawn
    // at the level the pan had before the key. Taking Clarity off here, as
    // this did until 2026-10-10, left the pan to find its level again from
    // nothing after every transmission.

    // 3M-5d: the waterfall plane is driven by the transmit source's
    // waterfall (pixout=1, DetTypeWF + AverageModeWF applied inside WDSP)
    // rather than by the spectrum trace path's avenger. Tell SpectrumWidget
    // to skip its internal pushWaterfallRow.
    sw->setTxExternalWaterfall(true);
    note(QStringLiteral("setTxExternalWaterfall(true)"));

    // The waterfall AGC is not re-primed here: keyEveryPan saved this
    // pan's receive level for its band and froze the follower, and the
    // transmit rows draw with the transmit levels, never the follower's.
    // From Thetis display.cs:1585 [v2.10.3.15] (Display.MOX):
    //   _RX1waterfallPreviousMinValue = getWaterfallCachedPreviousMinOrFloor(1, rx1WillBeTx);
    // 3M-5b polish (KG4VCF, 2026-05-10): clear the spectrum + waterfall
    // avenger accumulators on MOX rise so the trace doesn't fade from
    // pre-MOX RX state into TX state via LogRecursive smoothing (~3 s time
    // constant), which would otherwise show a brief mic-like blend overlaid
    // on the TUNE tone for the first few frames after key.
    sw->clearAvengers();
    note(QStringLiteral("clearAvengers"));

    // The transmit source takes the pan: its bins' centre and span, and its
    // trace and waterfall.
    if (m_source != nullptr) {
        m_source->beginTransmitView(sw, m_carrierHz);
    }

    // Keep the transmit view equal to whatever window is on screen, for as
    // long as transmit owns this pan.
    //
    // Thetis gets this for free: initAnalyzer derives span and window from
    // one zoom state, so they cannot drift apart. Ours are separate, and a
    // zoom made mid-transmission left the analyzer clipped to the span
    // chosen at key-down while the pan showed something wider -- 8 kHz of
    // bins stretched across 55 kHz of axis, with the trace wherever that
    // stretch put it. Bench 2026-08-05.
    //
    // Context object is `sw`, so this dies with the pan; the fall
    // disconnects it explicitly as well, because a pan that survives must
    // stop re-clipping once receive owns it again.
    m_viewConnection = connect(sw, &SpectrumWidget::txViewWindowChanged, sw,
                               [this](double centreHz, double bandwidthHz) {
        onViewWindowChanged(centreHz, bandwidthHz);
    });
    note(QStringLiteral("connect txViewWindowChanged"));
}

void MoxDisplayController::fall()
{
    // Restore the pan we actually took over, the one recorded on the rise
    // rather than re-resolved from the transmit slice. If the TX binding
    // moved to another slice while keyed, re-resolving would restore the
    // WRONG pan and strand the real one showing TX forever.
    //
    // A null pan here means it was destroyed mid-transmission (a layout
    // change while keyed); there is nothing to restore, and the per-pan
    // state died with the widget. Still clear the id so the router is not
    // left refusing RX frames for a pan id that no longer exists, and still
    // let the source go.
    SpectrumWidget* sw = m_pan.data();
    const bool hadPan = !m_panId.isEmpty();
    const bool hadTransmitView = m_transmitView;
    // A Tune context reports the compensated NCO and temporarily shifts
    // the TX filter offset. Restore the slice's current XIT before releasing
    // its reference; an ordinary rekey can report the rise's seeded carrier
    // and return from carrierChanged without refreshing this widget state.
    if (sw != nullptr && !m_slice.isNull()) {
        sw->setTxVfoOffsetHz(m_slice->xitEnabled() ? m_slice->xitHz() : 0);
    }
    m_panId.clear();
    m_pan.clear();
    m_slice.clear();
    if (sw == nullptr) {
        QObject::disconnect(m_viewConnection);
        m_transmitView = false;
        if (hadPan && hadTransmitView && m_source != nullptr) {
            m_source->endTransmitView(nullptr);
        }
        return;
    }

    sw->setMoxOverlay(false);
    note(QStringLiteral("setMoxOverlay(false)"));
    // Parity Task 31: with DUP on there was no transmit view to undo.
    if (hadTransmitView) {
        endTransmitView(sw);
    }
}

void MoxDisplayController::endTransmitView(SpectrumWidget* sw)
{
    m_transmitView = false;

    // 3M-5d: re-enable the internal pushWaterfallRow path before the
    // transmit source lets go, so the first RX frame post-unkey drives the
    // waterfall.
    sw->setTxExternalWaterfall(false);
    note(QStringLiteral("setTxExternalWaterfall(false)"));
    if (m_source != nullptr) {
        m_source->endTransmitView(sw);
    }

    // Restore the RX-side bin-frequency mapping (sample rate + DDC centre).
    if (m_savedSampleRate > 0.0) {
        sw->setSampleRate(m_savedSampleRate);
        sw->setDdcCenterFrequency(m_savedDdcHz);
        note(QStringLiteral("restore sampleRate and ddcCenterFrequency"));
    }
    // Leave TX context behind, so visibleBinRange goes back to the RX rate
    // and DDC centre. Its useTx test requires a non-zero centre, so zeroing
    // that is what disarms it.
    QObject::disconnect(m_viewConnection);
    note(QStringLiteral("disconnect txViewWindowChanged"));
    sw->setTxCenterFrequency(0.0);
    note(QStringLiteral("setTxCenterFrequency(0)"));
    // No AGC re-prime on the un-key either: keyEveryPan put back the level
    // this pan had on its band before the key (Thetis display.cs:1585
    // [v2.10.3.15]), so the first receive row is drawn where the last one
    // before the key was.
    // Symmetric avenger clear on un-key so the trace doesn't fade from TX
    // tone state into RX state.
    sw->clearAvengers();
    note(QStringLiteral("clearAvengers"));
}

void MoxDisplayController::onViewWindowChanged(double centreHz, double bandwidthHz)
{
    SpectrumWidget* sw = m_pan.data();
    if (!m_keyed || sw == nullptr || bandwidthHz <= 0.0) {
        return;
    }

    // Clamp the VIEW, not only the analyzer's span.
    //
    // Clamping the analyzer alone left the widget showing a window the
    // analyzer cannot fill: updateSpectrumFromTxPixels then resampled the
    // clipped bins across the whole axis, stretching and mislabelling them.
    // Found by Codex on PR #317. Pulling the window back inside the
    // baseband is the honest answer: the transmit display genuinely cannot
    // show more than 96 kHz, so the operator is stopped at the edge rather
    // than shown a stretch.
    //
    // Re-entrant by exactly one pass. The push below re-emits this signal
    // synchronously with the clamped values, that pass finds nothing left
    // to clamp and hands the view to the source, and applyViewWindow emits
    // nothing when nothing moved.
    const double viewBw = qMin(bandwidthHz, 2.0 * kHalfBasebandHz);
    double lo = centreHz - viewBw / 2.0 - m_carrierHz;
    double hi = lo + viewBw;
    if (lo < -kHalfBasebandHz) {
        lo = -kHalfBasebandHz;
        hi = lo + viewBw;
    }
    if (hi > kHalfBasebandHz) {
        hi = kHalfBasebandHz;
        lo = hi - viewBw;
    }
    const double clampedCentre = m_carrierHz + (lo + hi) / 2.0;
    if (!qFuzzyCompare(clampedCentre, centreHz) || !qFuzzyCompare(viewBw, bandwidthHz)) {
        sw->setDisplayWindowPreservingHistory(clampedCentre, viewBw);
        return;
    }
    if (m_source != nullptr) {
        m_source->requestView(centreHz, bandwidthHz, sw->width());
    }
}

void MoxDisplayController::carrierChanged(double carrierHz)
{
    SpectrumWidget* sw = m_pan.data();
    if (!m_keyed || sw == nullptr || carrierHz <= 0.0) {
        return;
    }
    if (qFuzzyCompare(1.0 + carrierHz, 1.0 + m_carrierHz)) {
        return;
    }
    // Parity Task 31: XIT does not move the display in display duplex
    // (console.cs:22144 [v2.10.3.15], "only when not in display duplex
    // mode"); the transmit view takes the carrier when it starts.
    if (!m_transmitView) {
        m_carrierHz = carrierHz;
        return;
    }
    // Row 16: Thetis's display follows XIT while transmitting
    // (console.cs:22069-22150 [v2.10.3.15], getLowHighForRXn, "xit, only
    // when txing"). The transmit filter overlay centres on the pan's VFO
    // plus the transmit slice's XIT: the carrier's offset from the slice's
    // dial (in a remote window the Core's carrier can land before the
    // mirrored XIT value does).
    if (!m_slice.isNull()) {
        sw->setTxVfoOffsetHz(static_cast<int>(std::lround(carrierHz - m_slice->frequency())));
    }
    const double shift = carrierHz - m_carrierHz;
    m_carrierHz = carrierHz;
    // The view keeps its place relative to the carrier, as the transmit
    // source holds it; the move re-asks the source through
    // txViewWindowChanged.
    sw->setDisplayWindowPreservingHistory(sw->centerFrequency() + shift, sw->bandwidth());
}

void MoxDisplayController::setHighSwr(bool highSwr, bool windBackLatched)
{
    m_highSwr = highSwr;
    m_windBackLatched = windBackLatched;
    applyHighSwr();
}

void MoxDisplayController::applyHighSwr()
{
    // The transmitting pan while keyed; after the fall the pan that last
    // transmitted keeps the state until it clears, as a local window's
    // pan keeps it until SwrProtectionController clears it.
    SpectrumWidget* target = m_pan.isNull() ? m_swrPan.data() : m_pan.data();
    if (!m_swrPan.isNull() && m_swrPan.data() != target) {
        m_swrPan->setHighSwrOverlay(false, false);
    }
    m_swrPan = target;
    if (target != nullptr) {
        target->setHighSwrOverlay(m_highSwr, m_highSwr && m_windBackLatched);
    }
}

void MoxDisplayController::followLocalRadio()
{
    MoxController* mox = m_model ? m_model->moxController() : nullptr;
    if (mox == nullptr) {
        return;
    }
    // Every key, whatever keyed it (a MOX click, a hardware PTT, TUNE, VOX,
    // a remote device): MoxController::moxStateChanged is the one edge.
    // Queued, as the lambda it replaces was, so the rise runs after the
    // feed has started the analyzer on the same edge.
    connect(mox, &MoxController::moxStateChanged, this, [this](bool isTx) {
        SliceModel* txSlice = m_model ? m_model->txBoundSlice() : nullptr;
        setKeyed(isTx, txSlice != nullptr ? txSlice->sliceIndex() : -1);
    }, Qt::QueuedConnection);
}

void MoxDisplayController::followStation(StationClient* client)
{
    if (client == nullptr || m_model.isNull()) {
        return;
    }
    TransmitState* state = client->transmitState();
    const QPointer<StationClient> guard(client);
    auto follow = [this, guard, state]() {
        if (guard.isNull() || m_model.isNull()) {
            return;
        }
        if (state != nullptr && guard->capabilities().txStateVersion >= 1) {
            setKeyed(state->keyed(), state->txSliceId());
            return;
        }
        // A Core that sends no txState: its mirrored radio.transmitting and
        // the slice whose txSlice is true.
        int txSliceId = -1;
        for (SliceModel* slice : m_model->slices()) {
            if (slice != nullptr && slice->isTxSlice()) {
                txSliceId = slice->sliceIndex();
                break;
            }
        }
        setKeyed(m_model->isTransmitting(), txSliceId);
    };
    if (state != nullptr) {
        connect(state, &TransmitState::stateChanged, this, follow);
        connect(state, &TransmitState::swrChanged, this, [this, state]() {
            setHighSwr(state->highSwr(), state->swrWindBackLatched());
        });
    }
    connect(m_model, &RadioModel::transmittingChanged, this, follow);
    // txState precedes the slice mirrors in the initial snapshot. By this
    // point their IDs, pan keys and window placements can resolve the rise.
    connect(client, &StationClient::stateSnapshotApplied, this, follow);
}

} // namespace NereusSDR
