#pragma once

// =================================================================
// src/gui/meters/MeterPoller.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-01  J.J. Boyd / KG4VCF. Resolve remote Max Bin by the slice
//                 hosted in this window. AI-assisted via OpenAI Codex.
//   2026-10-02 — Draft-only edits and inert cached previews by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-02 — Composite reading/replay/cadence contracts by J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-04-26 — Phase 3M-1a H.2: TX meter bindings on MOX engage/release.
//                 setTxChannel() + setInTx(bool) slot added.  TX poll
//                 reads TXA_OUT_PK / TXA_ALC_PK / TXA_ALC_AV / TXA_ALC_GAIN
//                 via GetTXAMeter() when in TX mode.
//                 Cite: Thetis dsp.cs:999-1050 [v2.10.3.13] CalculateTXMeter.
//   2026-08-06: Remote Daemon R2 Task 12: pollSliceSMeters() / setSliceChannels()
//                 / sliceSmeterUpdated / m_sliceChannels moved to the new
//                 core-side src/core/meters/SliceMeterPump.{h,cpp}; smeterUpdated
//                 removed with its only listener. J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-09-25: iPhone app plan Task 39 (D14, R-IOS-13): a remote window's
//               transmit meters: ALC and MIC from the Core's `txState`
//               (setRemoteTransmitState), the meters the Core does not send
//               shown disabled with the reason. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25 - R-R3-32 (remote-window parity Task 6):
//                 setPaReadingsModel. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-26 - R-R3-13 / R-R3-49 (remote-window parity Task 15):
//                 setRemoteMeterReadingsAvailable. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-26 - Trunk merge of remote transmit (R-R3-49, R-IOS-13): a
//                 connected Core below meterReadingsVersion 1 shows the
//                 five ADC and AGC meters disabled with the reason
//                 (remoteMeterReadingsNotSentText), as Task 39's transmit
//                 meters are. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-27 - R-R3-49 (remote-window parity Task 33): panMaxBinSource
//                 (Max Bin measured by each window from its own pan); the
//                 S-meter's Level and Compression TX modes from the handed-
//                 out MIC and COMP readings; setRemoteTxReadingsAvailable,
//                 the Core's COMP reading (txState's compressionDb). J.J.
//                 Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - A9 (iPhone app plan Task 39): the seven container stage
//                 meters from a Core at txReadingsVersion 3
//                 (setRemoteTxStageReadingsAvailable); a Core below it
//                 names the reason. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
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

#include "core/WdspTypes.h"
#include "MeterItem.h"
#include <QElapsedTimer>
#include <QJsonObject>
#include <QHash>

#include <functional>  // std::function for setRxOffsetSource (RXOffset port)

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVector>

namespace NereusSDR {

class MmioEndpoint;
class RxChannel;
class TxChannel;
class MeterWidget;
class RadioStatus;
class SMeterWidget;
class WdspEngine;
class RadioModel;
class SliceModel;
class TransmitState;
class SpectrumWidget;

// Binding IDs map to WDSP meter types (RxMeterType enum values)
namespace MeterBinding {
    // RX meters (0-49)
    constexpr int SignalPeak   = 0;    // RxMeterType::SignalPeak
    constexpr int SignalAvg    = 1;    // RxMeterType::SignalAvg
    constexpr int AdcPeak      = 2;    // RxMeterType::AdcPeak
    constexpr int AdcAvg       = 3;    // RxMeterType::AdcAvg
    constexpr int AgcGain      = 4;    // RxMeterType::AgcGain
    constexpr int AgcPeak      = 5;    // RxMeterType::AgcPeak
    constexpr int AgcAvg       = 6;    // RxMeterType::AgcAvg

    // RX meters — new (Phase 3G-4)
    constexpr int SignalMaxBin = 7;    // Spectral peak bin
    constexpr int PbSnr        = 8;    // Peak-to-baseline SNR

    // TX meters (100+). From Thetis MeterManager.cs Reading enum.
    // Stub values until TxChannel exists (Phase 3I-1).
    // PWR/SWR are hardware PA measurements, not WDSP meters.
    constexpr int TxPower        = 100;  // Forward power (hardware PA)
    constexpr int TxReversePower = 101;  // Reverse power (hardware PA)
    constexpr int TxSwr          = 102;  // SWR (computed fwd/rev ratio)
    constexpr int TxMic          = 103;  // TXA_MIC_AV
    constexpr int TxComp         = 104;  // TXA_COMP_AV

    // R-R3-21: the floor Thetis puts on the Compression reading.
    // From Thetis console.cs:46979 [v2.10.3.15]:
    //   updateMetersReading(Reading.COMP, (float)Math.Max(-30.0f, -WDSP.CalculateTXMeter(1, WDSP.MeterType.COMP)), 0);
    // CalculateTXMeter already returns -(float)val (dsp.cs:1056 [v2.10.3.15]),
    // so the two negations cancel: the reading is max(-30, TXA_COMP_AV).
    constexpr double kTxCompFloorDb = -30.0;
    constexpr int TxAlc          = 105;  // TXA_ALC_AV

    // TX meters — new (Phase 3G-4)
    constexpr int TxEq           = 106;  // From Thetis MeterManager.cs EQ reading
    constexpr int TxLeveler      = 107;  // TXA_LEVELER_AV
    constexpr int TxLevelerGain  = 108;  // TXA_LEVELER_GAIN
    constexpr int TxAlcGain      = 109;  // TXA_ALC_GAIN
    constexpr int TxAlcGroup     = 110;  // TXA_ALC_GROUP
    constexpr int TxCfc          = 111;  // TXA_CFC_AV
    constexpr int TxCfcGain      = 112;  // TXA_CFC_GAIN

    // GUI-only additions: independent stage peaks from existing TxChannel cache.
    constexpr int TxMicPeak      = 113;
    constexpr int TxAlcPeak      = 114;
    constexpr int TxCompPeak     = 115;
    constexpr int TxEqPeak       = 116;
    constexpr int TxLevelerPeak  = 117;
    constexpr int TxCfcPeak      = 118;

    // Hardware readings (200+)
    constexpr int HwVolts        = 200;  // PA supply voltage
    constexpr int HwAmps         = 201;  // PA supply current
    constexpr int HwTemperature  = 202;  // PA temperature

    // Rotator readings (300+)
    constexpr int RotatorAz      = 300;  // Azimuth (0-360)
    constexpr int RotatorEle     = 301;  // Elevation (0-90)
}

class MeterPoller : public QObject {
    Q_OBJECT

public:
    explicit MeterPoller(QObject* parent = nullptr);
    ~MeterPoller() override;

    void setRxChannel(RxChannel* channel);

    // NereusSDR (R-R3-13): whether the local radio link is up. On a local
    // LinkLost the RX channels stay alive but their meters stop updating
    // (an inactive channel reads -140 dBm), so the channel pointer alone
    // cannot say whether a reading exists. While false, poll() feeds the
    // no-reading sentinel to the RX bindings and the analog S-meter, as it
    // does with no channel. MainWindow drives it from connectionStateChanged
    // (true only in Connected). Default true; remote windows ignore it.
    void setLocalRxReadingAvailable(bool available);
    bool localRxReadingAvailable() const { return m_localRxReadingAvailable; }

    // ── SMeterWidget feed (Task 41, Phase 3P-II) ──────────────────────────
    //
    // setSMeter: register the analog SMeterWidget (AppletPanelWidget header).
    // pollSMeter() reads the WDSP source selected by m_sMeter->rxMode() and
    // calls m_sMeter->setLevel(dbm) on each poll tick.
    // setWdspEngine: provides getMaxBinDbm() for RxMode::MaxBin.
    // Both are non-owning; call with nullptr to detach.
    void setSMeter(SMeterWidget* widget);
    void setWdspEngine(WdspEngine* engine);

    // Remote GUI readings are already calibrated by Core. The display
    // callback supplies Max Bin from the current decoded passband; no
    // local WDSP channel or calibration participates in this path.
    void setRemoteRadioModel(RadioModel* model,
                             std::function<bool()> snapshotReady,
                             std::function<double(const SliceModel*)> maxBinSource = {});

    // R-R3-49 (parity Task 33): Max Bin stays a measurement each window
    // makes from its own display (the controller's ruling of 2026-09-27):
    // the peak of the slice's passband on the slice's own pan, read from
    // the displayed trace after the detector and averaging, exactly as the
    // local window's reading (SpectrumWidget::peakDbmInSlicePassband, fed
    // by MainWindow's spectrumFrameRendered hook). `spectrumFor` finds a
    // pan's spectrum by its key; no pan, or a passband off it, reads the
    // no-reading value (-400).
    static std::function<double(const SliceModel*)> panMaxBinSource(
        std::function<SpectrumWidget*(const QString& panKey)> spectrumFor);
    // 2026-10-02 KG4VCF, Codex: retain key-based callers while allowing a
    // remote window to resolve an empty Core pan key by its actual slice host.
    static std::function<double(const SliceModel*)> panMaxBinSourceForSlice(
        std::function<SpectrumWidget*(const SliceModel* slice)> spectrumFor);

    // R-R3-13 / R-R3-49 (remote-window parity Task 15): whether the Core
    // sends its ADC and AGC readings on its slices (meterReadingsVersion 1).
    // While true, a remote window's AdcPeak, AdcAvg, AgcGain, AgcPeak and
    // AgcAvg bindings read the active slice's adcPeakDbfs, adcAverageDbfs,
    // agcGainDb, agcPeakDb and agcAverageDb; unset or false, they get the
    // no-reading sentinel (shown "--"), never a frozen value. While the
    // window is connected (the snapshot is ready) and this is false, the
    // five bindings are also shown disabled with
    // remoteMeterReadingsNotSentText (MeterWidget::setBindingUnavailable).
    void setRemoteMeterReadingsAvailable(std::function<bool()> available);
    /// The five ADC and AGC bindings the Core sends at meterReadingsVersion 1.
    static const QList<int>& remoteMeterReadingBindings();
    /// Why: a connected Core that does not send these readings.
    static QString remoteMeterReadingsNotSentText();

    // iPhone app plan Task 39 (D14, R-IOS-13): a remote window's transmit
    // meters come from the Core's `txState` (StationClient::transmitState).
    // While transmitting, the ALC and MIC bindings get its alcDb and
    // micLevelDb; power, reflected power and SWR reach the meters through
    // this window's RadioStatus, which MainWindow feeds from the same
    // object. `unavailableText` returns why the window has no transmit
    // meters from its Core right now (not connected, or a Core that does
    // not send them), or empty when it has: then every transmit binding
    // the object does not carry (remoteTxBindingsNotSent) is shown disabled
    // with remoteTxMeterNotSentText until the Core sends them too
    // (setRemoteTxStageReadingsAvailable); otherwise every transmit
    // binding is, with that text. Remote role only.
    void setRemoteTransmitState(TransmitState* state,
                                std::function<QString()> unavailableText);
    /// The transmit bindings a Core below txReadingsVersion 3 does not
    /// carry in `txState`: the seven container stage meters (A9).
    static const QList<int>& remoteTxBindingsNotSent();
    // R-R3-49 (parity Task 33 follow-up): whether the Core sends its
    // transmit readings (txReadingsVersion 1), which carry the COMP reading
    // (`txState`'s compressionDb). While false the TxComp binding and the
    // S-meter's Compression mode show TransmitState::txReadingNotSentText.
    void setRemoteTxReadingsAvailable(std::function<bool()> available);
    /// Why: the Core sends transmit state but not this meter.
    static QString remoteTxMeterNotSentText();
    // A9 (iPhone app plan Task 39): whether the Core sends the seven stage
    // readings (txReadingsVersion 3: `txState`'s eqDb .. alcGroupDb). While
    // true the remoteTxBindingsNotSent bindings get them while transmitting,
    // as a local window's get its own transmit channel's; while false they
    // show remoteTxMeterNotSentText.
    void setRemoteTxStageReadingsAvailable(std::function<bool()> available);
    // R-R3-32 (remote-window parity Task 6): the model whose
    // paReadings() feed the HwVolts, HwAmps and HwTemperature bindings on
    // every poll, in a local window (this radio) and a remote one (the
    // Core's), with the no-reading sentinel when a reading is absent.
    void setPaReadingsModel(RadioModel* model);

    // ── TX meter bindings (H.2, Phase 3M-1a) ─────────────────────────────
    //
    // setTxChannel: register the TX channel for TX-meter polling.
    // Call with nullptr to detach (e.g. on radio disconnect).
    // Non-owning; RadioModel/WdspEngine own the object.
    //
    // setInTx: switch the poll set between RX and TX meters.
    // Ported from Thetis dsp.cs:999-1050 [v2.10.3.13] CalculateTXMeter:
    //   case MeterType.TXA_OUT_PK:   val = GetTXAMeter(channel, TXA_OUT_PK);
    //   case MeterType.TXA_ALC_PK:   val = GetTXAMeter(channel, TXA_ALC_PK);
    //   case MeterType.TXA_ALC_AV:   val = GetTXAMeter(channel, TXA_ALC_AV);
    //   case MeterType.TXA_ALC_GAIN: val = GetTXAMeter(channel, TXA_ALC_GAIN);
    // When isTx=true the poll() path skips the RX meter loop and calls
    // GetTXAMeter for the four TX binding IDs (TxAlc, TxAlcGain, TxPower stub).
    // When isTx=false the poll() path resumes normal RX meter polling.
    void setTxChannel(TxChannel* channel);

    void addTarget(MeterWidget* widget);
    void removeTarget(MeterWidget* widget);
    // Registers at most one target. Empty context is the legacy active RX;
    // otherwise the JSON source identity is resolved only by the GUI adapter.
    // A changed context clears old input/history before same-context replay.
    void setTargetContext(MeterWidget* widget, const QJsonObject& context);
    void setSMeterContext(const QJsonObject& context) { m_sMeterContext = context; }
    SMeterWidget* smeterForTest() const;
    // One cached-source lookup per context/binding per existing timer frame.
    // No new channel/subscription/WDSP read belongs in this callback. Return
    // kNoMeterReadingDbm for absent slices; setter changes invalidate RX replay.
    void setRxReadingSource(std::function<double(const QJsonObject&, int)> source);
    // Current GUI window identity; explicit foreign sessions cannot consume
    // window-global TX/PA/hardware caches, independent of RX slice availability.
    void setSessionIdSource(std::function<QString()> source);
    // Seeds state, availability and current samples without registering or
    // polling the widget. Read-only previews then listen to readingUpdated,
    // frameAdvanced, and bindingAvailabilityChanged.
    void replayReadings(MeterWidget* widget, const QJsonObject& context) const;
    void copyCachedReadings(MeterWidget* widget, const QJsonObject& context) const;
    bool inTx() const { return m_inTx; }
    QString bindingUnavailableReason(int binding) const { return m_availability.value(binding); }
    MeterItem::BindingSupport bindingSupport(int binding) const
    { return m_bindingSupport.value(binding, MeterItem::BindingSupport::Unknown); }
    // Explicit shared presentation settings for hosts/previews. Legacy hosts
    // that set these on MeterWidget are also sampled at replacement.
    // Shared settings are retained even with no live target. Task6 hosts
    // should use these setters when changing global units or PA rating.
    void setUnitMode(MeterItem::MeterUnit unit);
    void rescalePowerMeters(int watts);
    static const QList<int>& remoteTxPeakBindingsNotSent();

    // ── Polling interval (Task 3.1, MultimeterPage wire-up) ──────────────────
    // setIntervalMs / intervalMs: new preferred interface used by MultimeterPage.
    // Corresponds to Thetis udDisplayMeterDelay (display.cs) which sets the
    // UpdateInterval property that drives the meter polling timer.
    // Default 100ms (10 fps) from Thetis MeterManager.cs [v2.10.3.13].
    // setInterval / interval kept for internal callers (backward-compat).
    void setIntervalMs(int ms);
    int  intervalMs() const;

    // ── Averaging window (Task 3.1, forward-looking for Task 3.2) ────────────
    // setAverageWindow: controls how many poll samples are averaged before
    // dispatch.  Currently stored; full averaging dispatch lands in Task 3.2.
    // Corresponds to Thetis udDisplayMeterAvg (display.cs) [v2.10.3.13].
    // Default 1 (no averaging).
    void setAverageWindow(int n);
    int  averageWindow() const;

    void setInterval(int ms);
    int interval() const;

    void start();
    void stop();

    /// Wire PA telemetry signals from RadioStatus into the meter targets
    /// for MeterBinding::TxPower / TxReversePower / TxSwr.
    /// Call once during integration; call with nullptr to detach cleanly.
    /// Safe to call again with a new pointer — the previous connection is
    /// disconnected automatically before re-connecting.
    /// Cite: Thetis console.cs PollPAPWR loop [v2.10.3.13] (RadioStatus
    /// aggregates forward/reflected/swr from that loop via powerChanged).
    void setRadioStatus(RadioStatus* status);

    // ── RX meter calibration offset (Thetis-faithful port) ───────────────
    //
    // Source for the per-poll cumulative offset (preamp + cal) applied to
    // SignalPeak / SignalAvg / MaxBin readings before display.
    //
    // The callable is invoked once per poll tick and must be lightweight
    // (RadioModel::rxMeterOffsetDb() is a const lookup over
    // m_hardwareProfile + StepAttenuatorController + AppSettings, no
    // mutex, no I/O).  Returning 0.0 disables the offset cleanly.
    //
    // Thetis call sites:
    //   console.cs:46821  float offset = RXOffset(1);
    //   console.cs:46824  ... = CalculateRXMeter(...) + offset;       // S_PK
    //   console.cs:46828  ... = CalculateRXMeter(...) + offset;       // S_AV
    //   console.cs:46881  ... = GetDetectMaxBin(0)    + offset;       // MaxBin
    //
    // Pass nullptr to detach (e.g. on RadioModel teardown).
    void setRxOffsetSource(std::function<double()> source);

signals:
    void frameAdvanced(qint64 monotonicMs);
    void bindingAvailabilityChanged(int bindingId, const QString& reason);
    void bindingSupportChanged(int bindingId, MeterItem::BindingSupport support);
    void readingUpdated(const QJsonObject& context, int bindingId, double value);
    // MMIO identities never share the radio-binding cache/feed.
    void mmioReadingUpdated(const QUuid& guid, const QString& variable, double value, const QString& reason);
    void remoteSliceLevelUpdated(int sliceId, double dbm);
    /// R-R3-21: each transmit reading pollTxMeters() hands the meters
    /// (bindingId is a MeterBinding Tx* id), for controls outside a meter
    /// container: the Phone/CW applet's compression gauge.
    void txMeterReading(int bindingId, double value);

public:
    /// R-R3-21: the Compression reading Thetis shows for a raw TXA_COMP_AV
    /// value: max(-30, raw). With PROC off WDSP returns -400 (meter.c
    /// xmeter), which reads -30; a non-finite value also reads -30.
    static double compressionReading(double rawTxaCompAv);

    /// D14, R-R3-49: the value a transmit meter binding (MeterBinding Tx*,
    /// WDSP-read ones) shows, worked from WDSP readings exactly as Thetis
    /// works it (thetisTxReading in WdspTypes.h: CalculateTXMeter, then the
    /// MOX reading step). `readRaw` returns one GetTXAMeter reading
    /// (TxChannel::txMeter). -400 for a binding no WDSP meter feeds.
    static double txReadingForBinding(int bindingId,
                                      const std::function<double(TxMeterType)>& readRaw);

public slots:
    // Switch between RX and TX meter polling.
    // Connected to MoxController::moxStateChanged(bool) by MainWindow (H.2).
    // From Thetis dsp.cs:995-1050 [v2.10.3.13] CalculateTXMeter (TX branch)
    // vs CalculateRXMeter (RX branch). Switches happen at MOX engage/release,
    // not mid-poll, matching Thetis's integer-tick dispatch via UpdateTimer.
    void setInTx(bool isTx);

private slots:
    void poll();

private slots:

private:
    // ── TX poll helper ────────────────────────────────────────────────────────
    // Reads the 4 WDSP TX meters gated for 3M-1a and pushes them to all
    // registered MeterWidget targets.
    // Porting from Thetis dsp.cs:999-1050 [v2.10.3.13] CalculateTXMeter.
    void pollTxMeters();
    // pollTxMeters()'s hand-out of one reading, already worked
    // (txReadingForBinding), to the meters and txMeterReading.
    void handOutTxReading(int bindingId, double value);

#ifdef NEREUS_BUILD_TESTS
public:
    void setMmioEndpointSourceForTest(std::function<MmioEndpoint*(const QUuid&)> source) { m_mmioEndpointLookup = std::move(source); }
    void setMonotonicSourceForTest(std::function<qint64()> source) { m_monotonicSource = std::move(source); }
    int targetCountForTest() const { return m_targets.size(); }
    // Test seam: what pollTxMeters() does when every WDSP meter reads
    // `rawValue`.
    void handOutTxReadingForTest(int bindingId, double rawValue)
    {
        handOutTxReading(bindingId, txReadingForBinding(
            bindingId, [rawValue](TxMeterType) { return rawValue; }));
    }
private:
#endif

    // ── SMeterWidget poll helper (Task 41, Phase 3P-II) ──────────────────────
    // Branches on m_sMeter->rxMode() to read the correct WDSP source and
    // calls m_sMeter->setLevel(dbm).  Called from poll() when m_inTx=false.
    // Selector mapping (from Thetis Console/console.cs:954-957 [@501e3f5]):
    //   SMeter / SMeterPeak  -> GetRXAMeter(ch, RXA_S_PK)  (enum 0)
    //   SignalAverage        -> GetRXAMeter(ch, RXA_S_AV)  (enum 1)
    //   MaxBin               -> GetDetectMaxBin(disp=0)
    void pollSMeter();
    void pollAdaptedSMeter();
    QJsonObject m_sMeterContext;
    void pollRemoteRxMeters();
    // Task 39: the ALC and MIC readings from the Core's transmit state.
    void pollRemoteTxMeters();
    // Task 39: marks each target's transmit bindings the Core cannot feed.
    void refreshRemoteTxAvailability(bool force = false);
    QString remoteTransmitUnavailableText() const;
    // Trunk merge (R-R3-49): marks the five ADC and AGC bindings on a
    // connected Core that does not send them.
    void refreshRemoteMeterReadingsAvailability(bool force = false);

    // m_avgWindow: averaging window size set by MultimeterPage (Task 3.1).
    // Task 3.2 will use this value in dispatch; stored here for round-trip.
    // From Thetis udDisplayMeterAvg (display.cs) [v2.10.3.13].
    int    m_avgWindow{1};

    struct MmioReading { double value; QString reason; MeterItem::BindingSupport support; };
    QHash<QString, MmioReading> m_mmioReadings;
    MmioReading mmioReading(const MeterItem* item) const;
    void replayMmioReading(MeterWidget* widget, MeterItem* item) const;
#ifdef NEREUS_BUILD_TESTS
    std::function<MmioEndpoint*(const QUuid&)> m_mmioEndpointLookup;
#endif
    void publishContextReading(const QJsonObject& context, int binding, double value);
    void publishGlobalReading(int binding, double value);
    void publishAvailability(int binding, const QString& reason);
    void publishSupport(int binding, MeterItem::BindingSupport support);
    void refreshBindingSupport();
    void refreshGlobalSession();
    bool acceptsGlobalReading(const QJsonObject& context) const;
    QString globalAvailability(const QJsonObject& context, int binding) const;
    void invalidateReadings(bool rx, bool tx, bool hardware);
    void pollContextReadings();
    void invalidateTxAudioReadings();
    void rememberPresentation(const MeterWidget* widget);
    QHash<MeterWidget*, QJsonObject> m_targetContexts;
    QHash<QByteArray, QHash<int, double>> m_contextReadings;
    QHash<QByteArray, QJsonObject> m_knownContexts;
    QHash<int, double> m_globalReadings;
    QHash<int, QString> m_availability;
    QHash<int, MeterItem::BindingSupport> m_bindingSupport;
    QString m_supportIdentity;
    QVector<QMetaObject::Connection> m_supportConnections;
    std::function<double(const QJsonObject&, int)> m_rxReadingSource;
    std::function<QString()> m_sessionIdSource;
    QString m_cachedSessionId;
    QElapsedTimer m_clock;
    std::function<qint64()> m_monotonicSource;
    MeterItem::MeterUnit m_unitMode{MeterItem::MeterUnit::dBm};
    int m_powerScale{0};
    QMetaObject::Connection m_rxDestroyed, m_txDestroyed, m_paDestroyed, m_statusDestroyed;
    QVector<QMetaObject::Connection> m_remoteConnections;
    QTimer m_timer;
    QPointer<RxChannel> m_rxChannel;
    bool m_localRxReadingAvailable{true};

    // Non-owning TX channel pointer (H.2).  Valid only while WdspEngine has
    // opened the TX channel (after createTxChannel()).  Guarded in poll().
    // QPointer auto-clears when TxChannel is destroyed — matches m_rxChannel
    // pattern (3M-1a review fixup: prevents dangling-pointer dereference if
    // WdspEngine destroys the channel without calling setTxChannel(nullptr)).
    QPointer<TxChannel> m_txChannel;
    // m_inTx: true while MOX is active; flipped by setInTx(bool).
    // From Thetis dsp.cs:995-1050 [v2.10.3.13] TX/RX meter dispatch.
    bool m_inTx{false};
    // QPointer auto-clears to nullptr when the MeterWidget is destroyed.
    // ContainerManager's float/dock swap replaces MeterWidgets mid-session;
    // without QPointer, poll() would dereference the deleted old widgets.
    QVector<QPointer<MeterWidget>> m_targets;

    // PA telemetry wiring (Part B — Phase 3M-0 Task 7).
    // m_radioStatus is a non-owning raw pointer (RadioModel owns the object).
    // m_powerConn holds the single connection to RadioStatus::powerChanged;
    // disconnected on re-set or when status is nullptr.
    QPointer<RadioStatus>   m_radioStatus;
    QMetaObject::Connection m_powerConn;

    // SMeterWidget + WdspEngine (Task 41, Phase 3P-II).
    // Both are non-owning raw pointers.  QPointer for SMeterWidget matches
    // the m_rxChannel / m_txChannel safety pattern above.
    QPointer<SMeterWidget>  m_sMeter;
    WdspEngine*             m_wdspEngine{nullptr};

    // RX meter cal offset (Thetis-faithful port).  Set via
    // setRxOffsetSource(); empty callable yields 0.0 dB (no offset).
    // Queried independently by poll()'s own SignalPeak/SignalAvg loop and
    // by pollSMeter()'s analog-widget read (smeterUpdated, the signal this
    // comment used to also name, was removed in Remote Daemon R2 Task 12
    // fix round 1 -- see MeterPoller.cpp's modification history).
    // See setRxOffsetSource() doc for Thetis console.cs:46821 cite.
    std::function<double()> m_rxOffsetSource;
    bool m_remoteRole{false};
    QPointer<RadioModel> m_remoteModel;
    QPointer<RadioModel> m_paReadingsModel;
    void pollHardwareTelemetry();
    std::function<bool()> m_remoteSnapshotReady;
    std::function<double(const SliceModel*)> m_remoteMaxBinSource;
    std::function<bool()> m_remoteMeterReadingsAvailable;   // parity Task 15
    // What the targets were last told about the five ADC and AGC bindings.
    bool m_remoteReadingsAvailabilityShown{false};
    QString m_remoteReadingsUnavailableShown;
    // Task 39: the Core's transmit state and whether it sends it.
    QPointer<TransmitState> m_remoteTransmitState;
    // Parity Task 33: the S-meter's Level and Compression TX modes, the
    // last MIC and COMP readings handed out (setMicMeters takes both).
    float m_sMeterMicDb{-50.0f};
    float m_sMeterCompDb{0.0f};
    std::function<QString()> m_remoteTransmitUnavailable;
    // What the targets were last told (refreshRemoteTxAvailability).
    bool m_remoteTxAvailabilityShown{false};
    QString m_remoteTxUnavailableShown;
    // Parity Task 33 follow-up: the Core sends the COMP reading.
    std::function<bool()> m_remoteTxReadingsAvailable;
    bool m_remoteTxReadingsShown{false};
    bool remoteTxReadingsAvailable() const;
    std::function<bool()> m_remoteTxStageReadingsAvailable;
    bool m_remoteTxStageReadingsShown{false};
    bool remoteTxStageReadingsAvailable() const;
};

} // namespace NereusSDR
