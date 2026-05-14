// no-port-check: NereusSDR-original test helper. Thetis cites in method
// comments below describe which Thetis handler each accessor models (for
// reader orientation), not ported logic — all accessor bodies are trivially
// NereusSDR-native array reads/writes with bounds checks.

// tests/TestMockRadioModel.h  (NereusSDR)
// NereusSDR-original test helper — no Thetis upstream port.
//
// Minimal in-process mock for TciProtocol tests. Inherits QObject so tests
// can pass `&mock` directly to TciProtocol (no reinterpret_cast hazard).
// Accessors marked Q_INVOKABLE so TciProtocol can call them via
// QMetaObject::invokeMethod(Qt::DirectConnection) without a layering
// violation (production code never #includes test headers). Extended per
// phase as new command families need additional accessors.
//
// Phase 1: 7 accessors (VFO hz, mode, MOX) + resetToBaseline().
// Phase 3: now inherits QObject; resolves Task 1.1 reinterpret_cast<QObject*>
//           workaround in tst_tci_matrix_runner.cpp.
// Phase 6: 11 accessors total — added setVfoLock/vfoLock/setLock/lock;
//           Q_INVOKABLE on setVfoHz/vfoHz/setVfoLock/vfoLock/setLock/lock
//           so TciProtocol dispatch uses invokeMethod (no header dependency).
// Phase 7: 14 accessors total — added setFilterBand/filterLow/filterHigh;
//           Q_INVOKABLE on setMode/mode/setFilterBand/filterLow/filterHigh
//           so TciProtocol modulation + rx_filter_band handlers dispatch correctly.
// Phase 8: 19 accessors total — added setSplit/split/setGlobalMute/globalMute/
//           setRxMute/rxMute; Q_INVOKABLE on all six so TciProtocol trx-family
//           handlers dispatch correctly. mox/setMox upgraded to Q_INVOKABLE.
// Phase 9: 37 accessors total — added 18 DSP-family accessors:
//           setRxNb/rxNb, setRxBin/rxBin, setRxApf/rxApf, setRxNf/rxNf,
//           setRxAnf/rxAnf, setRxNr/rxNr/rxNrIndex, setAgcMode/agcMode,
//           setAgcGain/agcGain, setSqlEnable/sqlEnable, setSqlLevel/sqlLevel,
//           setRitEnable/ritEnable, setRitOffset/ritOffset,
//           setXitEnable/xitEnable, setXitOffset/xitOffset,
//           setRxBalance/rxBalance.
// Phase 10: 43 accessors total — added 6 audio-stream / volume accessors:
//           setAudioSampleRate/audioSampleRate,
//           setAudioStreamSampleType/audioStreamSampleType,
//           setAudioStreamChannels/audioStreamChannels,
//           setAudioStreamSamples/audioStreamSamples,
//           setAfLinear/afLinear, setMonLinear/monLinear.
// Phase 11: 44 accessors total — added 1 IQ-stream accessor pair:
//           setIqSampleRate/iqSampleRate.
// Phase 13: 55 accessors total — added 11 bespoke-_ex accessors:
//           setRxEnable/rxEnable (per-slice bool, default true),
//           setRxCtun/rxCtun (per-slice bool, default false),
//           setTxProfile/txProfile (global string, default "Default"),
//           txProfilesList (returns QStringList{"Default"}),
//           setCalibration/calibrationMeter/calibrationDisplay/
//           calibrationXvtr/calibrationSixMeter/calibrationTxDisplay
//           (5 doubles per slice, default 0.0).
// Aim: ~60 accessors total by end of Phase 14. Each commit that adds
// accessors should note the addition in the commit message.

#pragma once

#include <QObject>
#include <QString>
#include <array>

namespace NereusSDR {

class TestMockRadioModel : public QObject {
    Q_OBJECT
public:
    explicit TestMockRadioModel(QObject* parent = nullptr) : QObject(parent) {}

    // Set the VFO frequency for a given slice (0-based) and channel (0=A,1=B).
    // Q_INVOKABLE: called via QMetaObject::invokeMethod from TciProtocol.
    Q_INVOKABLE void setVfoHz(int slice, int chan, qint64 hz)
    {
        if (slice >= 0 && slice < 2 && chan >= 0 && chan < 2) {
            m_vfoHz[slice][chan] = hz;
        }
    }

    // Get the VFO frequency for a given slice and channel.
    // Q_INVOKABLE: called via QMetaObject::invokeMethod from TciProtocol.
    Q_INVOKABLE qint64 vfoHz(int slice, int chan) const
    {
        if (slice >= 0 && slice < 2 && chan >= 0 && chan < 2) {
            return m_vfoHz[slice][chan];
        }
        return 0;
    }

    // Set the DSP mode string for a given slice (e.g. "USB", "LSB", "CW").
    // Q_INVOKABLE: called via QMetaObject::invokeMethod from TciProtocol modulation handler.
    Q_INVOKABLE void setMode(int slice, const QString& mode)
    {
        if (slice >= 0 && slice < 2) {
            m_mode[slice] = mode;
        }
    }

    // Get the DSP mode string for a given slice.
    // Q_INVOKABLE: called via QMetaObject::invokeMethod from TciProtocol modulation handler.
    Q_INVOKABLE QString mode(int slice) const
    {
        if (slice >= 0 && slice < 2) {
            return m_mode[slice];
        }
        return {};
    }

    // Set the filter band (low/high Hz) for a given slice.
    // From Thetis TCIServer.cs:4393-4399 [v2.10.3.13] — handleRxFilterBand set path.
    // Q_INVOKABLE: called via QMetaObject::invokeMethod from TciProtocol rx_filter_band handler.
    Q_INVOKABLE void setFilterBand(int slice, int lowHz, int highHz)
    {
        if (slice >= 0 && slice < 2) {
            m_filterLow[slice]  = lowHz;
            m_filterHigh[slice] = highHz;
        }
    }

    // Get the filter low cutoff Hz for a given slice.
    // From Thetis TCIServer.cs:4380-4384 [v2.10.3.13] — handleRxFilterBand query path.
    // Q_INVOKABLE: called via QMetaObject::invokeMethod from TciProtocol rx_filter_band handler.
    Q_INVOKABLE int filterLow(int slice) const
    {
        if (slice >= 0 && slice < 2) {
            return m_filterLow[slice];
        }
        return 0;
    }

    // Get the filter high cutoff Hz for a given slice.
    // From Thetis TCIServer.cs:4380-4384 [v2.10.3.13] — handleRxFilterBand query path.
    // Q_INVOKABLE: called via QMetaObject::invokeMethod from TciProtocol rx_filter_band handler.
    Q_INVOKABLE int filterHigh(int slice) const
    {
        if (slice >= 0 && slice < 2) {
            return m_filterHigh[slice];
        }
        return 0;
    }

    // MOX (TX active) state. Q_INVOKABLE: called via QMetaObject::invokeMethod
    // from TciProtocol trx handler (Phase 8).
    // From Thetis TCIServer.cs:3468 [v2.10.3.13] — handleTrxMessage rx + mox parse.
    Q_INVOKABLE void setMox(bool on) { m_mox = on; }
    Q_INVOKABLE bool mox() const { return m_mox; }

    // Split state per slice (0-based).
    // From Thetis TCIServer.cs:3091 [v2.10.3.13] — handleSplitEnableMessage.
    // Q_INVOKABLE: called via QMetaObject::invokeMethod from TciProtocol split_enable handler.
    Q_INVOKABLE void setSplit(int slice, bool on)
    {
        if (slice >= 0 && slice < 2) {
            m_split[slice] = on;
        }
    }

    Q_INVOKABLE bool split(int slice) const
    {
        if (slice >= 0 && slice < 2) {
            return m_split[slice];
        }
        return false;
    }

    // Global mute state (both MUT and MUT2 in Thetis).
    // From Thetis TCIServer.cs:4051 [v2.10.3.13] — handleMute set/query.
    // Named globalMute to avoid collision with future per-slice mute.
    // Q_INVOKABLE: called via QMetaObject::invokeMethod from TciProtocol mute handler.
    Q_INVOKABLE void setGlobalMute(bool on) { m_globalMute = on; }
    Q_INVOKABLE bool globalMute() const { return m_globalMute; }

    // Per-RX mute state (0-based slice index).
    // From Thetis TCIServer.cs:4069 [v2.10.3.13] — handleMuteRX set/query.
    // Q_INVOKABLE: called via QMetaObject::invokeMethod from TciProtocol rx_mute handler.
    Q_INVOKABLE void setRxMute(int slice, bool on)
    {
        if (slice >= 0 && slice < 2) {
            m_rxMute[slice] = on;
        }
    }

    Q_INVOKABLE bool rxMute(int slice) const
    {
        if (slice >= 0 && slice < 2) {
            return m_rxMute[slice];
        }
        return false;
    }

    // VFO lock state for a given slice (0-based) and channel (0=A,1=B).
    // From Thetis TCIServer.cs:3284-3302 [v2.10.3.13] — handleVFOLock args dispatch.
    // Q_INVOKABLE: called via QMetaObject::invokeMethod from TciProtocol.
    Q_INVOKABLE void setVfoLock(int slice, int chan, bool locked)
    {
        if (slice >= 0 && slice < 2 && chan >= 0 && chan < 2) {
            m_vfoLock[slice][chan] = locked;
        }
    }

    Q_INVOKABLE bool vfoLock(int slice, int chan) const
    {
        if (slice >= 0 && slice < 2 && chan >= 0 && chan < 2) {
            return m_vfoLock[slice][chan];
        }
        return false;
    }

    // Main VFO lock (the "lock:" command's 2-arg form; drops channel arg per Thetis).
    // From Thetis TCIServer.cs:3265-3283 [v2.10.3.13] — handleLock args dispatch.
    // rx=0 → VFOALock, rx=1 → VFOBLock. Q_INVOKABLE: QMetaObject::invokeMethod.
    Q_INVOKABLE void setLock(int slice, bool locked)
    {
        if (slice >= 0 && slice < 2) {
            m_lock[slice] = locked;
        }
    }

    Q_INVOKABLE bool lock(int slice) const
    {
        if (slice >= 0 && slice < 2) {
            return m_lock[slice];
        }
        return false;
    }

    // ── Phase 9: DSP family accessors ─────────────────────────────────────────

    // NB (Noise Blanker) enable per slice.
    // From Thetis TCIServer.cs:3192-3207 [v2.10.3.13] — handleRxNbEnable.
    Q_INVOKABLE void setRxNb(int slice, bool on)
    {
        if (slice >= 0 && slice < 2) { m_rxNb[slice] = on; }
    }
    Q_INVOKABLE bool rxNb(int slice) const
    {
        return (slice >= 0 && slice < 2) ? m_rxNb[slice] : false;
    }

    // BIN (Binaural) enable per slice.
    // From Thetis TCIServer.cs:3208-3223 [v2.10.3.13] — handleRxBinEnable.
    Q_INVOKABLE void setRxBin(int slice, bool on)
    {
        if (slice >= 0 && slice < 2) { m_rxBin[slice] = on; }
    }
    Q_INVOKABLE bool rxBin(int slice) const
    {
        return (slice >= 0 && slice < 2) ? m_rxBin[slice] : false;
    }

    // APF (Audio Peak Filter) enable per slice.
    // From Thetis TCIServer.cs:3224-3247 [v2.10.3.13] — handleRxApfEnable.
    Q_INVOKABLE void setRxApf(int slice, bool on)
    {
        if (slice >= 0 && slice < 2) { m_rxApf[slice] = on; }
    }
    Q_INVOKABLE bool rxApf(int slice) const
    {
        return (slice >= 0 && slice < 2) ? m_rxApf[slice] : false;
    }

    // NF (Notch Filter / TNF) enable per slice.
    // From Thetis TCIServer.cs:3249-3264 [v2.10.3.13] — handleRxNfEnable.
    Q_INVOKABLE void setRxNf(int slice, bool on)
    {
        if (slice >= 0 && slice < 2) { m_rxNf[slice] = on; }
    }
    Q_INVOKABLE bool rxNf(int slice) const
    {
        return (slice >= 0 && slice < 2) ? m_rxNf[slice] : false;
    }

    // ANF (Automatic Notch Filter) enable per slice.
    // From Thetis TCIServer.cs:4521-4541 [v2.10.3.13] — handleAnfEnable.
    Q_INVOKABLE void setRxAnf(int slice, bool on)
    {
        if (slice >= 0 && slice < 2) { m_rxAnf[slice] = on; }
    }
    Q_INVOKABLE bool rxAnf(int slice) const
    {
        return (slice >= 0 && slice < 2) ? m_rxAnf[slice] : false;
    }

    // NR (Noise Reduction) enable + nr_index per slice.
    // From Thetis TCIServer.cs:4488-4519 [v2.10.3.13] — handleNrEnable.
    // nr_index 1..4 (0 = off per Thetis SelectNR(rx+1, false, 0)).
    Q_INVOKABLE void setRxNr(int slice, bool on, int nrIndex)
    {
        if (slice >= 0 && slice < 2) {
            m_rxNr[slice]      = on;
            m_rxNrIndex[slice] = nrIndex;
        }
    }
    Q_INVOKABLE bool rxNr(int slice) const
    {
        return (slice >= 0 && slice < 2) ? m_rxNr[slice] : false;
    }
    Q_INVOKABLE int rxNrIndex(int slice) const
    {
        return (slice >= 0 && slice < 2) ? m_rxNrIndex[slice] : 1;
    }

    // AGC mode string per slice. Canonical values: "off","long","slow","fast","custom","normal".
    // From Thetis TCIServer.cs:4658-4671 [v2.10.3.13] — handleAgcMode.
    Q_INVOKABLE void setAgcMode(int slice, const QString& mode)
    {
        if (slice >= 0 && slice < 2) { m_agcMode[slice] = mode; }
    }
    Q_INVOKABLE QString agcMode(int slice) const
    {
        return (slice >= 0 && slice < 2) ? m_agcMode[slice] : QStringLiteral("normal");
    }

    // AGC gain (dB) per slice. Range [-20, 120] per TCIServer.cs:4686 [v2.10.3.13].
    // From Thetis TCIServer.cs:4673-4689 [v2.10.3.13] — handleAgcGain.
    Q_INVOKABLE void setAgcGain(int slice, int gain)
    {
        if (slice >= 0 && slice < 2) { m_agcGain[slice] = gain; }
    }
    Q_INVOKABLE int agcGain(int slice) const
    {
        return (slice >= 0 && slice < 2) ? m_agcGain[slice] : 0;
    }

    // Squelch enable per slice.
    // From Thetis TCIServer.cs:3301-3316 [v2.10.3.13] — handleSqlEnable.
    Q_INVOKABLE void setSqlEnable(int slice, bool on)
    {
        if (slice >= 0 && slice < 2) { m_sqlEnable[slice] = on; }
    }
    Q_INVOKABLE bool sqlEnable(int slice) const
    {
        return (slice >= 0 && slice < 2) ? m_sqlEnable[slice] : false;
    }

    // Squelch level (dBm) per slice. Range [-140, 0] per TCIServer.cs:3330 [v2.10.3.13].
    // From Thetis TCIServer.cs:3317-3333 [v2.10.3.13] — handleSqlLevel.
    Q_INVOKABLE void setSqlLevel(int slice, int level)
    {
        if (slice >= 0 && slice < 2) { m_sqlLevel[slice] = level; }
    }
    Q_INVOKABLE int sqlLevel(int slice) const
    {
        return (slice >= 0 && slice < 2) ? m_sqlLevel[slice] : 0;
    }

    // RIT (Receive Incremental Tuning) — GLOBAL, not per-slice.
    // From Thetis TCIServer.cs:3128-3143 [v2.10.3.13] — handleRITEnableMessage.
    Q_INVOKABLE void setRitEnable(bool on) { m_ritEnable = on; }
    Q_INVOKABLE bool ritEnable() const { return m_ritEnable; }

    // RIT offset in Hz — GLOBAL.
    // From Thetis TCIServer.cs:3160-3175 [v2.10.3.13] — handleRITOffsetMessage.
    Q_INVOKABLE void setRitOffset(int hz) { m_ritOffset = hz; }
    Q_INVOKABLE int ritOffset() const { return m_ritOffset; }

    // XIT (Transmit Incremental Tuning) — GLOBAL, not per-slice.
    // From Thetis TCIServer.cs:3144-3159 [v2.10.3.13] — handleXITEnableMessage.
    Q_INVOKABLE void setXitEnable(bool on) { m_xitEnable = on; }
    Q_INVOKABLE bool xitEnable() const { return m_xitEnable; }

    // XIT offset in Hz — GLOBAL.
    // From Thetis TCIServer.cs:3176-3191 [v2.10.3.13] — handleXITOffsetMessage.
    Q_INVOKABLE void setXitOffset(int hz) { m_xitOffset = hz; }
    Q_INVOKABLE int xitOffset() const { return m_xitOffset; }

    // RX balance (F2 dB) per (slice, channel).
    // From Thetis TCIServer.cs:4631-4656 [v2.10.3.13] — handleRxBalance.
    // Phase 9: stored directly as F2 dB; Thetis pan-slider transform deferred.
    Q_INVOKABLE void setRxBalance(int slice, int chan, double balance)
    {
        if (slice >= 0 && slice < 2 && chan >= 0 && chan < 2) {
            m_rxBalance[slice][chan] = balance;
        }
    }
    Q_INVOKABLE double rxBalance(int slice, int chan) const
    {
        if (slice >= 0 && slice < 2 && chan >= 0 && chan < 2) {
            return m_rxBalance[slice][chan];
        }
        return 0.0;
    }

    // ── Phase 10: Audio stream config accessors ───────────────────────────────

    // Audio sample rate (Hz). Range: typically 8000/12000/24000/48000.
    // From Thetis TCIServer.cs:5740 [v2.10.3.13] — handleAudioSampleRate.
    // Q_INVOKABLE: called via QMetaObject::invokeMethod from TciProtocol.
    Q_INVOKABLE void setAudioSampleRate(int sr) { m_audioSampleRate = sr; }
    Q_INVOKABLE int audioSampleRate() const { return m_audioSampleRate; }

    // Audio stream sample type: "float32" | "int16" | "int24" | "int32".
    // From Thetis TCIServer.cs:5908 [v2.10.3.13] — handleAudioStreamSampleType.
    // Q_INVOKABLE: called via QMetaObject::invokeMethod from TciProtocol.
    Q_INVOKABLE void setAudioStreamSampleType(const QString& t) { m_audioStreamSampleType = t; }
    Q_INVOKABLE QString audioStreamSampleType() const { return m_audioStreamSampleType; }

    // Audio stream channel count: 1 (mono) or 2 (stereo).
    // From Thetis TCIServer.cs:5935 [v2.10.3.13] — handleAudioStreamChannels.
    // Q_INVOKABLE: called via QMetaObject::invokeMethod from TciProtocol.
    Q_INVOKABLE void setAudioStreamChannels(int n) { m_audioStreamChannels = n; }
    Q_INVOKABLE int audioStreamChannels() const { return m_audioStreamChannels; }

    // Audio stream block size in samples. Range [100..2048].
    // From Thetis TCIServer.cs:5951 [v2.10.3.13] — handleAudioStreamSamples.
    // Q_INVOKABLE: called via QMetaObject::invokeMethod from TciProtocol.
    Q_INVOKABLE void setAudioStreamSamples(int n) { m_audioStreamSamples = n; }
    Q_INVOKABLE int audioStreamSamples() const { return m_audioStreamSamples; }

    // AF (RX audio) gain stored as linear int [0..100].
    // Wire boundary converts via tciLinearToDbVolume / tciDbToLinearVolume
    // (Thetis TCIServer.cs:4110-4132 [v2.10.3.13]).
    // From Thetis TCIServer.cs:4150-4160 [v2.10.3.13] — handleVolume.
    // Q_INVOKABLE: called via QMetaObject::invokeMethod from TciProtocol.
    Q_INVOKABLE void setAfLinear(int v) { m_afLinear = v; }
    Q_INVOKABLE int afLinear() const { return m_afLinear; }

    // MON (TX monitor) volume stored as linear int [0..100].
    // From Thetis TCIServer.cs:4133-4148 [v2.10.3.13] — handleMONVolume.
    // Q_INVOKABLE: called via QMetaObject::invokeMethod from TciProtocol.
    Q_INVOKABLE void setMonLinear(int v) { m_monLinear = v; }
    Q_INVOKABLE int monLinear() const { return m_monLinear; }

    // ── Phase 11: IQ stream config accessors ──────────────────────────────────

    // IQ sample rate (Hz). Default 192000 (common HPSDR rate; Thetis does not
    // clamp — see handleIQSampleRate at TCIServer.cs:5705-5722 [v2.10.3.13]).
    // Q_INVOKABLE: called via QMetaObject::invokeMethod from TciProtocol.
    Q_INVOKABLE void setIqSampleRate(int sr) { m_iqSampleRate = sr; }
    Q_INVOKABLE int iqSampleRate() const { return m_iqSampleRate; }

    // ── Phase 13: bespoke _ex command accessors ───────────────────────────────

    // RX enable per slice (default true — rx0 is always enabled in Thetis).
    // From Thetis TCIServer.cs:4413-4450 [v2.10.3.13] — handleRXEnable.
    // rx_enable set: rx==0 always on; rx==1 sets RX2Enabled.
    // rx_enable query: rx==0 → !MOX; rx==1 → RX2Enabled && !MOX.
    // NereusSDR simplification: MOX-gating deferred to Phase 17; stored directly.
    Q_INVOKABLE void setRxEnable(int slice, bool on)
    {
        if (slice >= 0 && slice < 2) { m_rxEnable[slice] = on; }
    }
    Q_INVOKABLE bool rxEnable(int slice) const
    {
        return (slice >= 0 && slice < 2) ? m_rxEnable[slice] : true;
    }

    // CTUN (Center Tune) enable per slice.
    // From Thetis TCIServer.cs:4696-4710 [v2.10.3.13] — handleCTUN.
    // rx_ctun_ex set: 2-arg (rx, bool) — SetCTUN(rx+1, enable).
    // rx_ctun_ex query: 1-arg (rx) → sendCTUN(rx, GetCTUN(rx+1)).
    // sendCTUN at TCIServer.cs:4690-4694 [v2.10.3.13]: "rx_ctun_ex:rx,bool;"
    Q_INVOKABLE void setRxCtun(int slice, bool on)
    {
        if (slice >= 0 && slice < 2) { m_rxCtun[slice] = on; }
    }
    Q_INVOKABLE bool rxCtun(int slice) const
    {
        return (slice >= 0 && slice < 2) ? m_rxCtun[slice] : false;
    }

    // TX profile name (global, not per-slice).
    // From Thetis TCIServer.cs:4732-4748 [v2.10.3.13] — handleTXProfile.
    // tx_profile_ex set (2-arg path via set switch): profile name in args[0].
    // tx_profile_ex query (1-arg path via query switch): returns current name.
    // sendTXProfile at TCIServer.cs:4715-4720 [v2.10.3.13]: "tx_profile_ex:name;"
    Q_INVOKABLE void setTxProfile(const QString& name) { m_txProfile = name; }
    Q_INVOKABLE QString txProfile() const { return m_txProfile; }

    // TX profiles list — available profile names.
    // From Thetis TCIServer.cs:4748-4752 [v2.10.3.13] — handleTXProfiles.
    // tx_profiles_ex query (1-arg path): returns CSV list of profiles.
    // sendTXProfiles at TCIServer.cs:4721-4731 [v2.10.3.13]: comma-joined.
    // NereusSDR stub: always returns {"Default"} (MicProfileManager integration deferred).
    Q_INVOKABLE QStringList txProfilesList() const
    {
        return QStringList{QStringLiteral("Default")};
    }

    // Calibration values per slice (5 doubles: meter, display, xvtr, sixMeter, txDisplay).
    // From Thetis TCIServer.cs:1152-1170 [v2.10.3.13] — CalibrationChanged.
    // calibration_ex set (1-arg path): rx only — queries calibration from radio and emits.
    // sendCalibration at TCIServer.cs:4766-4775 [v2.10.3.13]: F6 C-locale per value.
    // NereusSDR: setCalibration stores all 5 doubles for that slice; CalibrationChanged
    //   path in handler reads them back and emits via buildCalibrationExLine.
    Q_INVOKABLE void setCalibration(int slice, double meter, double display,
                                    double xvtr, double sixMeter, double txDisplay)
    {
        if (slice >= 0 && slice < 2) {
            m_calibration[slice][0] = meter;
            m_calibration[slice][1] = display;
            m_calibration[slice][2] = xvtr;
            m_calibration[slice][3] = sixMeter;
            m_calibration[slice][4] = txDisplay;
        }
    }
    Q_INVOKABLE double calibrationMeter(int slice) const
    {
        return (slice >= 0 && slice < 2) ? m_calibration[slice][0] : 0.0;
    }
    Q_INVOKABLE double calibrationDisplay(int slice) const
    {
        return (slice >= 0 && slice < 2) ? m_calibration[slice][1] : 0.0;
    }
    Q_INVOKABLE double calibrationXvtr(int slice) const
    {
        return (slice >= 0 && slice < 2) ? m_calibration[slice][2] : 0.0;
    }
    Q_INVOKABLE double calibrationSixMeter(int slice) const
    {
        return (slice >= 0 && slice < 2) ? m_calibration[slice][3] : 0.0;
    }
    Q_INVOKABLE double calibrationTxDisplay(int slice) const
    {
        return (slice >= 0 && slice < 2) ? m_calibration[slice][4] : 0.0;
    }

    // Reset all state to baseline (zeros/empty). Called before each matrix row.
    void resetToBaseline()
    {
        m_vfoHz = {};
        m_mode = {};
        m_mox = false;
        m_vfoLock = {};
        m_lock = {};
        // Phase 7: common SSB filter defaults (200 Hz low, 2900 Hz high).
        m_filterLow  = { 200, 200 };
        m_filterHigh = { 2900, 2900 };
        // Phase 8: TRX family state resets.
        m_split = {};
        m_globalMute = false;
        m_rxMute = {};
        // Phase 9: DSP family state resets.
        m_rxNb = {};
        m_rxBin = {};
        m_rxApf = {};
        m_rxNf = {};
        m_rxAnf = {};
        m_rxNr = {};
        m_rxNrIndex = { 1, 1 };
        m_agcMode = { QStringLiteral("normal"), QStringLiteral("normal") };
        m_agcGain = {};
        m_sqlEnable = {};
        m_sqlLevel = {};
        m_ritEnable = false;
        m_ritOffset = 0;
        m_xitEnable = false;
        m_xitOffset = 0;
        m_rxBalance = {};
        // Phase 10: audio stream / volume state resets.
        m_audioSampleRate = 48000;
        m_audioStreamSampleType = QStringLiteral("float32");
        m_audioStreamChannels = 2;
        m_audioStreamSamples = 2048;
        m_afLinear  = 50;
        m_monLinear = 50;
        // Phase 11: IQ stream state resets.
        m_iqSampleRate = 192000;
        // Phase 13: bespoke _ex state resets.
        m_rxEnable   = { true, true };  // rx0 always on per Thetis default
        m_rxCtun     = {};
        m_txProfile  = QStringLiteral("Default");
        m_calibration = {};  // all 5 doubles per slice reset to 0.0
    }

private:
    // 2 slices x 2 channels is sufficient for Phase 1; expand in later phases.
    std::array<std::array<qint64, 2>, 2> m_vfoHz{};
    std::array<QString, 2> m_mode{};
    bool m_mox{false};
    // Phase 6: VFO lock state (vfo_lock command) and main lock (lock command).
    std::array<std::array<bool, 2>, 2> m_vfoLock{};
    std::array<bool, 2> m_lock{};
    // Phase 7: filter band state (rx_filter_band command).
    std::array<int, 2> m_filterLow{200, 200};
    std::array<int, 2> m_filterHigh{2900, 2900};
    // Phase 8: TRX family state.
    std::array<bool, 2> m_split{};
    bool m_globalMute{false};
    std::array<bool, 2> m_rxMute{};
    // Phase 9: DSP family state.
    std::array<bool, 2> m_rxNb{};
    std::array<bool, 2> m_rxBin{};
    std::array<bool, 2> m_rxApf{};
    std::array<bool, 2> m_rxNf{};
    std::array<bool, 2> m_rxAnf{};
    std::array<bool, 2> m_rxNr{};
    std::array<int, 2>  m_rxNrIndex{1, 1};
    std::array<QString, 2> m_agcMode{QStringLiteral("normal"), QStringLiteral("normal")};
    std::array<int, 2> m_agcGain{};
    std::array<bool, 2> m_sqlEnable{};
    std::array<int, 2> m_sqlLevel{};
    bool m_ritEnable{false};
    int  m_ritOffset{0};
    bool m_xitEnable{false};
    int  m_xitOffset{0};
    std::array<std::array<double, 2>, 2> m_rxBalance{};
    // Phase 10: audio stream / volume state.
    int     m_audioSampleRate{48000};
    QString m_audioStreamSampleType{QStringLiteral("float32")};
    int     m_audioStreamChannels{2};
    int     m_audioStreamSamples{2048};
    int     m_afLinear{50};
    int     m_monLinear{50};
    // Phase 11: IQ stream state.
    int     m_iqSampleRate{192000};
    // Phase 13: bespoke _ex state.
    std::array<bool, 2>    m_rxEnable{true, true};   // default ON
    std::array<bool, 2>    m_rxCtun{};
    QString                m_txProfile{QStringLiteral("Default")};
    std::array<std::array<double, 5>, 2> m_calibration{};  // [slice][meter,display,xvtr,6m,txdisp]
};

} // namespace NereusSDR
