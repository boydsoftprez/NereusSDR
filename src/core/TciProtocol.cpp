// no-port-check: NereusSDR-original — parser + dispatch shells ported from
// Thetis TCIServer.cs:4900-5197 [v2.10.3.13]. Handler bodies are stubs;
// Phase 5+ adds individual cases via the matrix runner.

// src/core/TciProtocol.cpp  (NereusSDR)
// NereusSDR-original — TCI command protocol handler implementation.
//
// Parser ported from Thetis TCIServer.cs:4900-4924 [v2.10.3.13].
// Two-switch dispatch shape from Thetis TCIServer.cs:4924-5197 [v2.10.3.13].
//
// This file REPLACES the Phase 1 stub (commit 77d27b3).
//
// Modification history (NereusSDR):
//   2026-05-10 — Phase 3J-1 Task 3.1 by J.J. Boyd (KG4VCF);
//                AI-assisted transformation via Anthropic Claude Code.

#include "TciProtocol.h"
#include "AppSettings.h"
#include "LogCategories.h"
#include "TciVolume.h"
#include "TciVfoCoalescer.h"

namespace NereusSDR {

TciProtocol::TciProtocol(QObject* radio, QObject* parent)
    : QObject(parent)
    , m_radio(radio)
{
    (void)m_radio;  // dereferenced in Phase 6+ via QMetaObject::invokeMethod for setters.
}

// From Thetis TCIServer.cs:4900-4924 [v2.10.3.13] — text-frame parser.
// Strip trailing ';', split on first ':', case-insensitive command lookup.
// json_spot exception (parts[0]=="spot" AND msg contains "[json]{") routes
// to set path; the actual JSON brace-aware splitter at TCIServer.cs:4785-4900
// is a Phase 12 (spot stubs) concern.
QString TciProtocol::handleCommand(const QString& command)
{
    QString msg = command.trimmed();
    if (msg.endsWith(QLatin1Char(';'))) {
        msg.chop(1);
        msg = msg.trimmed();
    }
    if (msg.isEmpty()) {
        return {};
    }

    const int colonIdx = msg.indexOf(QLatin1Char(':'));
    QStringList parts;
    if (colonIdx >= 0) {
        parts << msg.left(colonIdx);
        parts << msg.mid(colonIdx + 1);
    } else {
        parts << msg;
    }

    const QString name = parts.at(0).toLower().trimmed();

    // From Thetis TCIServer.cs:4917 [v2.10.3.13] — json_spot detour.
    const bool jsonSpot = parts.size() >= 2 && name == QStringLiteral("spot")
                          && msg.toLower().indexOf(QStringLiteral("[json]{")) >= 0;

    if (parts.size() == 2 || jsonSpot) {
        const QStringList args = parts.at(1).split(QLatin1Char(','));
        return handleSetCommand(name, args);
    }
    return handleQueryCommand(name);
}

bool TciProtocol::hasPendingNotification() const
{
    return !m_pendingNotifications.isEmpty();
}

QString TciProtocol::takePendingNotification()
{
    if (m_pendingNotifications.isEmpty()) {
        return {};
    }
    return m_pendingNotifications.takeFirst();
}

// Phase 15: drain coalesced VFO updates into m_pendingNotifications.
// Called by TciServer from the 5ms drain timer (and by tst_tci_matrix_runner
// after each handleCommand for synchronous test-model compatibility).
// From Thetis TCIServer.cs:1722-1727 [v2.10.3.13] — outbound-coalesced map.
void TciProtocol::drainCoalescedNotifications()
{
    QStringList drained;
    m_vfoCoalescer.drainAll(&drained);
    for (const auto& frame : drained) {
        m_pendingNotifications.append(frame);
    }
}

// From Thetis TCIServer.cs:2512-2552 [v2.10.3.13] — sendInitialisationData
// emits 8 wrapper lines + buildInitialRadioState() + ready;.
QStringList TciProtocol::buildInitBurst() const
{
    QStringList lines;
    auto& s = AppSettings::instance();

    // From Thetis TCIServer.cs:2515-2520 [v2.10.3.13]
    //MW0LGE_22 emulate ee3 protocol
    //
    // Phase 3J-1 bench fix (2026-05-11): default to True (was False).  WSJT-X
    // and Hamlib's TCI driver gate TCI-audio mode on recognising the server
    // identifier — they enable TCI audio ONLY when the server advertises as
    // ExpertSDR3 / SunSDR2PRO.  An unknown identifier (e.g. "Thetis" alone,
    // or "NereusSDR") makes WSJT-X fall back to non-TCI audio: the radio
    // keys via the trx command but WSJT-X never streams TX_AUDIO_STREAM
    // frames, and it sends `trx:0,true;` (without the `,tci` suffix)
    // because it never entered TCI-audio mode.  Bench-verified symptom
    // exactly matched.  These keys also get wiped by the AppSettings
    // unknown-key purge on save, so the user's explicit "True" setting
    // doesn't survive a normal app exit — defaulting to True works around
    // both issues.
    const bool emulateEsdr3 = s.value(QStringLiteral("TciEmulateExpertSDR3Protocol"),
                                      QStringLiteral("True")).toString()
                              == QStringLiteral("True");
    const QString protocolName = emulateEsdr3
        ? QStringLiteral("ExpertSDR3")
        : QStringLiteral("Thetis");
    lines << QStringLiteral("protocol:%1,2.0;").arg(protocolName);

    // From Thetis TCIServer.cs:2523-2528 [v2.10.3.13]
    //MW0LGE_22 emulate sunsdr
    //
    // Phase 3J-1 bench fix (2026-05-11): default to True (see
    // TciEmulateExpertSDR3Protocol comment above for the full rationale).
    const bool emulateSunSdr = s.value(QStringLiteral("TciEmulateSunSDR2Pro"),
                                       QStringLiteral("True")).toString()
                               == QStringLiteral("True");
    // NereusSDR divergence: HardwareSpecific.Model has no NereusSDR equivalent;
    // hardcode "NereusSDR" until Phase 4 Task 4.2 wires RadioModel state.
    const QString deviceName = emulateSunSdr
        ? QStringLiteral("SunSDR2PRO")
        : QStringLiteral("NereusSDR");
    lines << QStringLiteral("device:%1;").arg(deviceName);

    // From Thetis TCIServer.cs:2529 [v2.10.3.13]
    lines << QStringLiteral("receive_only:false;");

    // From Thetis TCIServer.cs:2530 [v2.10.3.13] — locked at 2 per design doc §1.2;
    // Slice C/D are NereusSDR-internal and not exposed via TCI in Phase 3J-1.
    lines << QStringLiteral("trx_count:2;");

    // From Thetis TCIServer.cs:2531 [v2.10.3.13]
    lines << QStringLiteral("channels_count:2;");

    // From Thetis TCIServer.cs:2533 [v2.10.3.13] — sendVFOLimits(0, MaxFreq*1e6).
    // Phase 4 Task 4.1 hardcodes 64 MHz; Task 4.2 wires RadioModel state.
    lines << QStringLiteral("vfo_limits:0,64000000;");

    // From Thetis TCIServer.cs:2535-2536 [v2.10.3.13] — sendIFLimits(-halfSample, halfSample).
    // halfSample = SampleRateRX1 / 2. Phase 4 Task 4.1 hardcodes 96000 (192 kHz / 2);
    // Task 4.2 wires RadioModel state.
    lines << QStringLiteral("if_limits:-96000,96000;");

    // From Thetis TCIServer.cs:2538-2544 [v2.10.3.13]
    // MW0LGE_22b modulations are upper in sun, so replicate
    const bool cwluBecomesCw = s.value(QStringLiteral("TciCwluBecomesCw"),
                                       QStringLiteral("False")).toString()
                               == QStringLiteral("True");
    const QString cwSuffix = cwluBecomesCw
        ? QStringLiteral("cwl,cwu,cw")
        : QStringLiteral("cwl,cwu");
    const QString modList = QStringLiteral("am,sam,dsb,lsb,usb,nfm,fm,digl,digu,%1")
                                .arg(cwSuffix).toUpper();
    lines << QStringLiteral("modulations_list:%1;").arg(modList);

    // From Thetis TCIServer.cs:2546 [v2.10.3.13] — sendInitialRadioState body.
    // Phase 4 Task 4.2 fills the body (~75-87 wire lines per Sweep D).
    lines.append(buildInitialRadioStateLines());

    // From Thetis TCIServer.cs:2548 [v2.10.3.13]
    lines << QStringLiteral("ready;");

    return lines;
}

// From Thetis TCIServer.cs:2363-2510 [v2.10.3.13] — sendInitialRadioState body.
// Emits up to 97 wire frames (subset depending on bSend + bRX2Enabled flags).
// Phase 4 Task 4.2 ports the source order, 6 inline comments verbatim, and
// the typo-fix divergence (design doc §7 row 1). Value args are hardcoded
// placeholders; Phase 6+ wires RadioModel state via QMetaObject::invokeMethod.
//
// audioGainToDb(gain): if gain<=0 → -60.0; if gain>=1 → 0.0;
//   else 20*log10(gain). From Thetis TCIServer.cs:4553-4562 [v2.10.3.13].
// linearToDbVolume(volume): maps [0..100] linearly to [-60..0] dB, clamped.
//   From Thetis TCIServer.cs:4110-4120 [v2.10.3.13].
QStringList TciProtocol::buildInitialRadioStateLines() const
{
    QStringList lines;
    auto& s = AppSettings::instance();

    // From Thetis TCIServer.cs:2365 [v2.10.3.13]
    const bool bSend = s.value(QStringLiteral("TciSendInitialFrequencyStateOnConnect"),
                               QStringLiteral("True")).toString()
                       == QStringLiteral("True");

    // Phase 4 Task 4.2 placeholder — single-RX default (bRX2Enabled = false).
    // Phase 6+ reads RadioModel::rx2Enabled() via QMetaObject::invokeMethod.
    const bool bRX2Enabled = false;

    // Phase 4 Task 4.2 placeholder values.
    // Phase 6+ wires each to the corresponding RadioModel property.
    const qint64 rx1FreqHz  = 14250000;  // 14.250 MHz — 20m USB demo value
    const qint64 rx2FreqHz  = 7100000;   // 7.100 MHz  — 40m USB demo value
    const qint64 txFreqHz   = 14250000;  // 14.250 MHz — demo value
    const QString txBand    = QStringLiteral("20m");
    const QString modeUpper = QStringLiteral("USB");

    // Filter: 200/2900 Hz — common SSB filter placeholder.
    const int filterLow  = 200;
    const int filterHigh = 2900;

    // RX idle defaults — all off.
    const bool mox  = false;
    const bool tune = false;

    // NR off. Phase 6+ reads GetSelectedNR(1/2) via RadioModel.
    const int rx1nr = 0;
    const int rx2nr = 0;

    // RX volume: RX0Gain=100, RX1Gain=0, RX2Gain=100 (0-100 scale).
    // audioGainToDb(100/100) = 0.0; audioGainToDb(0/100) = -60.0.
    // From Thetis TCIServer.cs:4415-4417 variable naming conventions.
    const double rx1vol    = 0.0;   // audioGainToDb(100/100) = 0.0
    const double rx1Subvol = -60.0; // audioGainToDb(0/100) = -60.0
    const double rx2vol    = 0.0;   // audioGainToDb(100/100) = 0.0

    // Balance: GetBal(1/2, sub) = 0.0 → 40.0 - (0.0 * 0.8) = 40.0
    const double bal = 40.0;

    // AGC: MED → "normal". From Thetis TCIServer.cs:2206-2207 [v2.10.3.13].
    const QString agcMode = QStringLiteral("normal");
    const int agcGain = 40;  // Thetis default AGC threshold

    // CTUN off.
    const bool ctun = false;

    // TX profiles placeholder.
    const QStringList txProfiles = { QStringLiteral("Default") };
    const QString txProfile = QStringLiteral("Default");

    // Calibration: all 0.0 (F6) — defaults.
    const double calMeter   = 0.0;
    const double calDisplay = 0.0;
    const double calXvtr    = 0.0;
    const double cal6m      = 0.0;
    const double calTxDisp  = 0.0;

    // RIT / XIT defaults.
    const bool ritOn  = false;
    const bool xitOn  = false;
    const int ritVal  = 0;
    const int xitVal  = 0;

    // VFO locks.
    const bool vfoALock = false;
    const bool vfoBLock = false;

    // Squelch off.
    const bool sqlEn    = false;
    const int  sqlLevel = -150;

    // DIGL / DIGU click-tune offsets.
    const int diglOffset = 0;
    const int diguOffset = 0;

    // CW macro defaults.
    const int cwMacrosSpeed = 25;  // WPM
    const int cwMacrosDelay = 0;   // ms
    const int cwKeyerSpeed  = 25;  // WPM

    // Split off.
    const bool split = false;

    // IQ / audio stream defaults.
    const int iqSampleRate          = 192000;  // common HPSDR rate
    const int audioSampleRate       = 48000;   // default
    const QString audioSampleType   = QStringLiteral("float32");
    const int audioStreamChannels   = 2;       // stereo
    const int audioStreamSamples    = 2048;    // per design doc §10 default

    // Phase 3J-1 closeout Item 5 (2026-05-12): honor the
    // TciTxStreamBufferingMs AppSetting written by the Setup → Audio TCI
    // page.  Previously hardcoded to 50; the spin-box wrote a key the
    // server ignored, so the operator's chosen pre-buffer never reached
    // TCI clients.  Range [10..200] matches AudioTciPage's spin-box and
    // Thetis's `udTCIStreamAudioBufferingTime` constraint
    // (Setup.designer.cs:58610-58620 [v2.10.3.13]).  Default 50 ms
    // matches Thetis's m_txStreamAudioBufferingMs default
    // (TCIServer.cs:790 [v2.10.3.13]).
    int txStreamBufferingMs =
        s.value(QStringLiteral("TciTxStreamBufferingMs"), 50).toInt();
    if (txStreamBufferingMs < 10)  { txStreamBufferingMs = 10; }
    if (txStreamBufferingMs > 200) { txStreamBufferingMs = 200; }

    // Mute / volume / MON defaults.
    // linearToDbVolume(AF=100): ((100-0)/(100-0))*(0-(-60))+(-60) = 0.0 dB.
    const bool globalMute = false;
    const bool rx0Mute    = false;
    const bool rx1Mute    = false;
    const double volumeDb = 0.0;   // linearToDbVolume(AF=100)
    const bool monEnable  = false;
    const double monVolDb = 0.0;   // linearToDbVolume(TXAF=100)
    const bool powerOn    = true;

    // From Thetis TCIServer.cs:2368-2383 [v2.10.3.13] — bSend gate.
    if (bSend) {
        // From Thetis TCIServer.cs:2370-2379 [v2.10.3.13]
        lines << buildDdsLine(0, rx1FreqHz);
        lines << buildDdsLine(1, rx2FreqHz);
        lines << buildIfLine(0, 0, 0);
        lines << buildIfLine(0, 1, 0);
        // NereusSDR divergence (design doc §7 row 1): Thetis TCIServer.cs:2374-2375
        // [v2.10.3.13] calls sendIF(1,1) TWICE (copy-paste bug). We emit the intended
        // (1,0)+(1,1) cross-product instead, matching the sendVFO enumeration below.
        lines << buildIfLine(1, 0, 0);
        lines << buildIfLine(1, 1, 0);
        lines << buildVfoLine(0, 0, rx1FreqHz);
        lines << buildVfoLine(0, 1, rx1FreqHz);
        lines << buildVfoLine(1, 0, rx2FreqHz);
        lines << buildVfoLine(1, 1, rx2FreqHz);

        //bespoke
        lines << buildTxFrequencyLine(txFreqHz);
        lines << buildTxFrequencyThetisLine(txFreqHz, txBand, bRX2Enabled, false);
    }

    // From Thetis TCIServer.cs:2385-2389 [v2.10.3.13]
    lines << buildModulationLine(0, modeUpper);
    lines << buildModulationLine(1, modeUpper);
    lines << buildRxFilterBandLine(0, filterLow, filterHigh);
    lines << buildRxFilterBandLine(1, filterLow, filterHigh);

    // From Thetis TCIServer.cs:2391-2392 [v2.10.3.13]
    lines << buildRxEnableLine(0, !mox);
    lines << buildRxEnableLine(1, bRX2Enabled && !mox);

    // From Thetis TCIServer.cs:2394-2403 [v2.10.3.13]
    lines << buildRxNrEnableLine(0, rx1nr > 0);
    lines << buildRxNrEnableLine(1, rx2nr > 0);
    lines << buildRxNrEnableExLine(0, rx1nr > 0, rx1nr);
    lines << buildRxNrEnableExLine(1, rx2nr > 0, rx2nr);
    lines << buildRxNbEnableLine(0, false);
    lines << buildRxNbEnableLine(1, false);
    lines << buildRxBinEnableLine(0, false);
    lines << buildRxBinEnableLine(1, false);

    // From Thetis TCIServer.cs:2405-2413 [v2.10.3.13]
    lines << buildRxAnfEnableLine(0, false);
    lines << buildRxAnfEnableLine(1, false);
    // Gate on !IsSetupFormNull; Phase 4 Task 4.2 always emits (no Setup form yet).
    lines << buildRxApfEnableLine(0, false);
    lines << buildRxApfEnableLine(1, false);
    lines << buildRxNfEnableLine(0, false);
    lines << buildRxNfEnableLine(1, false);

    // From Thetis TCIServer.cs:2415-2430 [v2.10.3.13]
    lines << buildRxVolumeLine(0, 0, rx1vol);
    lines << buildRxVolumeLine(0, 1, rx1Subvol);
    lines << buildRxVolumeLine(1, 0, rx2vol);
    lines << buildRxVolumeLine(1, 1, rx2vol);
    lines << buildRxBalanceLine(0, 0, bal);
    lines << buildRxBalanceLine(0, 1, bal);
    lines << buildRxBalanceLine(1, 0, bal);
    lines << buildRxBalanceLine(1, 1, bal);

    // From Thetis TCIServer.cs:2427-2433 [v2.10.3.13]
    lines << buildAgcModeLine(0, agcMode);
    lines << buildAgcModeLine(1, agcMode);
    lines << buildAgcGainLine(0, agcGain);
    lines << buildAgcGainLine(1, agcGain);
    lines << buildRxCtunExLine(0, ctun);
    lines << buildRxCtunExLine(1, ctun);

    // From Thetis TCIServer.cs:2435-2439 [v2.10.3.13]
    lines << buildTxProfilesExLine(txProfiles);
    lines << buildTxProfileExLine(txProfile);
    lines << buildCalibrationExLine(0, calMeter, calDisplay, calXvtr, cal6m, calTxDisp);
    lines << buildCalibrationExLine(1, calMeter, calDisplay, calXvtr, cal6m, calTxDisp);

    //lock
    //TODO rx channel enable
    //rit/xit

    // From Thetis TCIServer.cs:2445-2452 [v2.10.3.13]
    lines << buildRitEnableLine(0, ritOn);
    lines << buildRitEnableLine(1, ritOn);
    lines << buildXitEnableLine(0, xitOn);
    lines << buildXitEnableLine(1, xitOn);
    lines << buildRitOffsetLine(0, ritVal);
    lines << buildRitOffsetLine(1, ritVal);
    lines << buildXitOffsetLine(0, xitVal);
    lines << buildXitOffsetLine(1, xitVal);

    // From Thetis TCIServer.cs:2453-2458 [v2.10.3.13]
    lines << buildLockLine(0, vfoALock);
    if (bRX2Enabled) {
        lines << buildLockLine(1, vfoBLock);
    }
    lines.append(buildAllVfoLocksLines(bRX2Enabled, vfoALock, vfoBLock));

    // From Thetis TCIServer.cs:2459-2464 [v2.10.3.13]
    lines << buildSqlEnableLine(0, sqlEn);
    lines << buildSqlEnableLine(1, sqlEn);
    lines << buildSqlLevelLine(0, sqlLevel);
    lines << buildSqlLevelLine(1, sqlLevel);
    lines << buildDiglOffsetLine(diglOffset);
    lines << buildDiguOffsetLine(diguOffset);

    // From Thetis TCIServer.cs:2465-2470 [v2.10.3.13] — gated on m_server != null
    // (always true in our port; Phase 6+ reads from server config object).
    lines << buildCwMacrosSpeedLine(cwMacrosSpeed);
    lines << buildCwMacrosDelayLine(cwMacrosDelay);
    lines << buildCwKeyerSpeedLine(cwKeyerSpeed);

    // From Thetis TCIServer.cs:2472-2476 [v2.10.3.13]
    lines << buildSplitEnableLine(0, split);
    lines << buildSplitEnableLine(1, bRX2Enabled && split);
    lines << buildTxEnableLine(0, !mox);
    lines << buildTxEnableLine(1, bRX2Enabled && !mox);

    // From Thetis TCIServer.cs:2478-2481 [v2.10.3.13]
    lines << buildRxChannelEnableLine(0, 0, true);
    lines << buildRxChannelEnableLine(0, 1, false);  // GetSubRX(1) placeholder
    lines << buildRxChannelEnableLine(1, 0, bRX2Enabled);
    // no sub rx on rx2
    lines << buildRxChannelEnableLine(1, 1, false);

    // From Thetis TCIServer.cs:2483-2487 [v2.10.3.13]
    lines << buildTrxLine(0, mox && !(false && bRX2Enabled));
    lines << buildTrxLine(1, mox && (false && bRX2Enabled));
    lines << buildTuneLine(0, tune && !(false && bRX2Enabled));
    lines << buildTuneLine(1, tune && (false && bRX2Enabled));

    // From Thetis TCIServer.cs:2489-2497 [v2.10.3.13]
    lines << buildIqStopLine(0);
    lines << buildIqStopLine(1);
    lines << buildIqSampleRateLine(iqSampleRate);
    lines << buildAudioSampleRateLine(audioSampleRate);
    lines << buildAudioStreamSampleTypeLine(audioSampleType);
    lines << buildAudioStreamChannelsLine(audioStreamChannels);
    lines << buildAudioStreamSamplesLine(audioStreamSamples);
    lines << buildTxStreamAudioBufferingLine(txStreamBufferingMs);

    // From Thetis TCIServer.cs:2499-2505 [v2.10.3.13]
    lines << buildMuteLine(globalMute);
    lines << buildRxMuteLine(0, rx0Mute);
    lines << buildRxMuteLine(1, rx1Mute);
    lines << buildVolumeLine(volumeDb);
    lines << buildMonEnableLine(monEnable);
    lines << buildMonVolumeLine(monVolDb);

    // From Thetis TCIServer.cs:2507 [v2.10.3.13]
    // MW0LGE_22b moved here to replicate sun
    lines << buildStartStopLine(powerOn);

    return lines;
}

// ── Freq / IF / VFO helpers ─────────────────────────────────────────────────

// From Thetis TCIServer.cs:2334-2348 [v2.10.3.13] — sendDDS format string.
QString TciProtocol::buildDdsLine(int rx, qint64 hz)
{
    // ddsFreq += GetDSPcwPitchShiftToZero(rx+1); //MW0LGE [2.9.0.7]
    //   [original inline comment from TCIServer.cs:2344 — cw pitch shift deferred to Phase 6+]
    return QStringLiteral("dds:%1,%2;").arg(rx).arg(hz);
}

// From Thetis TCIServer.cs:2096-2120 [v2.10.3.13] — sendIF format string.
// MW0LGE [2.9.0.7] note we invert with - (the cw pitch shift subtraction is
// part of sendIF; Phase 4 Task 4.2 hardcodes offsetHz = 0 for all IF lines).
QString TciProtocol::buildIfLine(int rx, int chan, qint64 offsetHz)
{
    return QStringLiteral("if:%1,%2,%3;").arg(rx).arg(chan).arg(offsetHz);
}

// From Thetis TCIServer.cs:2061-2095 [v2.10.3.13] — sendVFO format string.
QString TciProtocol::buildVfoLine(int rx, int chan, qint64 hz)
{
    return QStringLiteral("vfo:%1,%2,%3;").arg(rx).arg(chan).arg(hz);
}

// From Thetis TCIServer.cs:2246-2259 [v2.10.3.13] — sendTXFrequencyChanged.
// Emits two frames: tx_frequency (lower-cased) and the bespoke tx_frequency_thetis.
// bespoke TCI command for anan to make life easier determining active TX frequency
// format is : tx_frequency_thetis:3700000,b80m,false,false;
// arg1 freq (long) / arg2 band b80m, b40m etc / arg3 rx2 enabled / arg4 tx on vfoB
QString TciProtocol::buildTxFrequencyLine(qint64 hz)
{
    return QStringLiteral("tx_frequency:%1;").arg(hz);
}

QString TciProtocol::buildTxFrequencyThetisLine(qint64 hz, const QString& band,
                                                 bool rx2en, bool txvfob)
{
    return QStringLiteral("tx_frequency_thetis:%1,%2,%3,%4;")
        .arg(hz)
        .arg(band)
        .arg(rx2en ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(txvfob ? QStringLiteral("true") : QStringLiteral("false"));
}

// ── Mode / filter helpers ───────────────────────────────────────────────────

// From Thetis TCIServer.cs:2136-2157 [v2.10.3.13] — sendMode format string.
// MW0LGE_22b mods are uppercase on the sun, replicate
QString TciProtocol::buildModulationLine(int rx, const QString& modeUpper)
{
    return QStringLiteral("modulation:%1,%2;").arg(rx).arg(modeUpper);
}

// From Thetis TCIServer.cs:2349-2353 [v2.10.3.13] — sendFilterBand.
// Adjacent sendDDS at TCIServer.cs:2344 carries: //MW0LGE [2.9.0.7]
QString TciProtocol::buildRxFilterBandLine(int rx, int lowHz, int highHz)
{
    return QStringLiteral("rx_filter_band:%1,%2,%3;").arg(rx).arg(lowHz).arg(highHz);
}

// ── RX-enable / NR / NB / BIN / ANF / APF / NF ────────────────────────────

// From Thetis TCIServer.cs:2279-2283 [v2.10.3.13] — sendRXEnable.
QString TciProtocol::buildRxEnableLine(int rx, bool en)
{
    return QStringLiteral("rx_enable:%1,%2;").arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:4472-4480 [v2.10.3.13] — sendNrEnable (non-extended form).
QString TciProtocol::buildRxNrEnableLine(int rx, bool en)
{
    return QStringLiteral("rx_nr_enable:%1,%2;").arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:4472-4480 [v2.10.3.13] — sendNrEnable (extended form).
QString TciProtocol::buildRxNrEnableExLine(int rx, bool en, int nrIndex)
{
    return QStringLiteral("rx_nr_enable_ex:%1,%2,%3;")
        .arg(rx)
        .arg(en ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(nrIndex);
}

// From Thetis TCIServer.cs:1901-1905 [v2.10.3.13] — sendRxNbEnable.
QString TciProtocol::buildRxNbEnableLine(int rx, bool en)
{
    return QStringLiteral("rx_nb_enable:%1,%2;").arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:1906-1910 [v2.10.3.13] — sendRxBinEnable.
QString TciProtocol::buildRxBinEnableLine(int rx, bool en)
{
    return QStringLiteral("rx_bin_enable:%1,%2;").arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:4482-4487 [v2.10.3.13] — sendAnfEnable.
QString TciProtocol::buildRxAnfEnableLine(int rx, bool en)
{
    return QStringLiteral("rx_anf_enable:%1,%2;").arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:1911-1915 [v2.10.3.13] — sendRxApfEnable.
QString TciProtocol::buildRxApfEnableLine(int rx, bool en)
{
    return QStringLiteral("rx_apf_enable:%1,%2;").arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:1916-1920 [v2.10.3.13] — sendRxNfEnable.
QString TciProtocol::buildRxNfEnableLine(int rx, bool en)
{
    return QStringLiteral("rx_nf_enable:%1,%2;").arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
}

// ── Volume / balance helpers ────────────────────────────────────────────────

// From Thetis TCIServer.cs:4563-4568 [v2.10.3.13] — sendRxVolume (F2 C-locale).
QString TciProtocol::buildRxVolumeLine(int rx, int chan, double db)
{
    return QStringLiteral("rx_volume:%1,%2,%3;")
        .arg(rx)
        .arg(chan)
        .arg(QString::number(db, 'f', 2));
}

// From Thetis TCIServer.cs:2187-2191 [v2.10.3.13] — sendRxBalance (F2 C-locale).
QString TciProtocol::buildRxBalanceLine(int rx, int chan, double bal)
{
    return QStringLiteral("rx_balance:%1,%2,%3;")
        .arg(rx)
        .arg(chan)
        .arg(QString::number(bal, 'f', 2));
}

// ── AGC helpers ─────────────────────────────────────────────────────────────

// From Thetis TCIServer.cs:2236-2240 [v2.10.3.13] — sendAgcMode.
QString TciProtocol::buildAgcModeLine(int rx, const QString& mode)
{
    return QStringLiteral("agc_mode:%1,%2;").arg(rx).arg(mode);
}

// From Thetis TCIServer.cs:2241-2245 [v2.10.3.13] — sendAgcGain.
QString TciProtocol::buildAgcGainLine(int rx, int gain)
{
    return QStringLiteral("agc_gain:%1,%2;").arg(rx).arg(gain);
}

// ── CTUN / TX profile / calibration ────────────────────────────────────────

// From Thetis TCIServer.cs:4690-4694 [v2.10.3.13] — sendCTUN (rx_ctun_ex suffix).
QString TciProtocol::buildRxCtunExLine(int rx, bool en)
{
    return QStringLiteral("rx_ctun_ex:%1,%2;").arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:4721-4731 [v2.10.3.13] — sendTXProfiles.
// Profiles are comma-separated; sendTXProfiles returns early if IsSetupFormNull.
// Phase 4 Task 4.2 always emits with the placeholder list.
QString TciProtocol::buildTxProfilesExLine(const QStringList& names)
{
    return QStringLiteral("tx_profiles_ex:%1;").arg(names.join(QLatin1Char(',')));
}

// From Thetis TCIServer.cs:4715-4720 [v2.10.3.13] — sendTXProfile.
QString TciProtocol::buildTxProfileExLine(const QString& active)
{
    return QStringLiteral("tx_profile_ex:%1;").arg(active);
}

// From Thetis TCIServer.cs:4766-4775 [v2.10.3.13] — sendCalibration (F6 C-locale).
QString TciProtocol::buildCalibrationExLine(int rx, double meter, double display,
                                             double xvtr, double sixMeter,
                                             double txDisplay)
{
    return QStringLiteral("calibration_ex:%1,%2,%3,%4,%5,%6;")
        .arg(rx)
        .arg(QString::number(meter, 'f', 6))
        .arg(QString::number(display, 'f', 6))
        .arg(QString::number(xvtr, 'f', 6))
        .arg(QString::number(sixMeter, 'f', 6))
        .arg(QString::number(txDisplay, 'f', 6));
}

// ── RIT / XIT helpers ───────────────────────────────────────────────────────

// From Thetis TCIServer.cs:1881-1885 [v2.10.3.13] — sendRITEnable.
QString TciProtocol::buildRitEnableLine(int rx, bool en)
{
    return QStringLiteral("rit_enable:%1,%2;").arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:1886-1890 [v2.10.3.13] — sendXITEnable.
QString TciProtocol::buildXitEnableLine(int rx, bool en)
{
    return QStringLiteral("xit_enable:%1,%2;").arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:1891-1895 [v2.10.3.13] — sendRITOffset.
QString TciProtocol::buildRitOffsetLine(int rx, int hz)
{
    return QStringLiteral("rit_offset:%1,%2;").arg(rx).arg(hz);
}

// From Thetis TCIServer.cs:1896-1900 [v2.10.3.13] — sendXITOffset.
QString TciProtocol::buildXitOffsetLine(int rx, int hz)
{
    return QStringLiteral("xit_offset:%1,%2;").arg(rx).arg(hz);
}

// ── Lock helpers ────────────────────────────────────────────────────────────

// From Thetis TCIServer.cs:1921-1925 [v2.10.3.13] — sendLock.
QString TciProtocol::buildLockLine(int rx, bool locked)
{
    return QStringLiteral("lock:%1,%2;").arg(rx).arg(locked ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:2032-2049 [v2.10.3.13] — sendAllVFOLocks logic.
// Uses sendVFOLock (TCIServer.cs:1926-1930) for the individual vfo_lock lines.
// When rx2Enabled: emits vfo_lock:0,0; vfo_lock:1,0; vfo_lock:1,1 (3 lines).
// When !rx2Enabled: emits vfo_lock:0,0; vfo_lock:0,1 (2 lines).
QStringList TciProtocol::buildAllVfoLocksLines(bool rx2Enabled,
                                                bool vfoALock, bool vfoBLock)
{
    QStringList out;
    const QString trueStr  = QStringLiteral("true");
    const QString falseStr = QStringLiteral("false");
    if (rx2Enabled) {
        // tryGetVFOLockState(0,0) → VFOALock
        out << QStringLiteral("vfo_lock:0,0,%1;").arg(vfoALock ? trueStr : falseStr);
        // tryGetVFOLockState(1,0) → VFOBLock
        out << QStringLiteral("vfo_lock:1,0,%1;").arg(vfoBLock ? trueStr : falseStr);
        // tryGetVFOLockState(1,1) → VFOBLock
        out << QStringLiteral("vfo_lock:1,1,%1;").arg(vfoBLock ? trueStr : falseStr);
    } else {
        // tryGetVFOLockState(0,0) → VFOALock
        out << QStringLiteral("vfo_lock:0,0,%1;").arg(vfoALock ? trueStr : falseStr);
        // tryGetVFOLockState(0,1) → VFOBLock
        out << QStringLiteral("vfo_lock:0,1,%1;").arg(vfoBLock ? trueStr : falseStr);
    }
    return out;
}

// ── SQL / DIGL / DIGU helpers ────────────────────────────────────────────────

// From Thetis TCIServer.cs:1931-1935 [v2.10.3.13] — sendSqlEnable.
QString TciProtocol::buildSqlEnableLine(int rx, bool en)
{
    return QStringLiteral("sql_enable:%1,%2;").arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:1936-1940 [v2.10.3.13] — sendSqlLevel.
QString TciProtocol::buildSqlLevelLine(int rx, int level)
{
    return QStringLiteral("sql_level:%1,%2;").arg(rx).arg(level);
}

// From Thetis TCIServer.cs:2051-2055 [v2.10.3.13] — sendDiglOffset.
QString TciProtocol::buildDiglOffsetLine(int hz)
{
    return QStringLiteral("digl_offset:%1;").arg(hz);
}

// From Thetis TCIServer.cs:2056-2060 [v2.10.3.13] — sendDiguOffset.
QString TciProtocol::buildDiguOffsetLine(int hz)
{
    return QStringLiteral("digu_offset:%1;").arg(hz);
}

// ── CW macro helpers ─────────────────────────────────────────────────────────

// From Thetis TCIServer.cs:1941-1944 [v2.10.3.13] — sendCwMacrosSpeed.
QString TciProtocol::buildCwMacrosSpeedLine(int wpm)
{
    return QStringLiteral("cw_macros_speed:%1;").arg(wpm);
}

// From Thetis TCIServer.cs:1945-1948 [v2.10.3.13] — sendCwMacrosDelay.
QString TciProtocol::buildCwMacrosDelayLine(int ms)
{
    return QStringLiteral("cw_macros_delay:%1;").arg(ms);
}

// From Thetis TCIServer.cs:1949-1952 [v2.10.3.13] — sendCwKeyerSpeed.
QString TciProtocol::buildCwKeyerSpeedLine(int wpm)
{
    return QStringLiteral("cw_keyer_speed:%1;").arg(wpm);
}

// ── Split / TX-enable helpers ─────────────────────────────────────────────────

// From Thetis TCIServer.cs:1876-1880 [v2.10.3.13] — sendSplit.
QString TciProtocol::buildSplitEnableLine(int rx, bool en)
{
    return QStringLiteral("split_enable:%1,%2;").arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:2284-2288 [v2.10.3.13] — sendTXEnable.
QString TciProtocol::buildTxEnableLine(int rx, bool en)
{
    return QStringLiteral("tx_enable:%1,%2;").arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
}

// ── RX channel enable ─────────────────────────────────────────────────────────

// From Thetis TCIServer.cs:5887-5890 [v2.10.3.13] — sendRxChannelEnable.
QString TciProtocol::buildRxChannelEnableLine(int rx, int chan, bool en)
{
    return QStringLiteral("rx_channel_enable:%1,%2,%3;")
        .arg(rx)
        .arg(chan)
        .arg(en ? QStringLiteral("true") : QStringLiteral("false"));
}

// ── MOX / TUNE helpers ────────────────────────────────────────────────────────

// From Thetis TCIServer.cs:2121-2131 [v2.10.3.13] — sendMOX (no ",tci" suffix in
// init burst; signalTCI=false always for initial state lines).
// Adjacent sendIF at TCIServer.cs:2116 carries: //MW0LGE [2.9.0.7] note we invert with -
QString TciProtocol::buildTrxLine(int rx, bool mox)
{
    return QStringLiteral("trx:%1,%2;").arg(rx).arg(mox ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:2274-2278 [v2.10.3.13] — sendTune.
QString TciProtocol::buildTuneLine(int rx, bool tuning)
{
    return QStringLiteral("tune:%1,%2;").arg(rx).arg(tuning ? QStringLiteral("true") : QStringLiteral("false"));
}

// ── IQ helpers ────────────────────────────────────────────────────────────────

// From Thetis TCIServer.cs:5814-5817 [v2.10.3.13] — sendIQStartStop (iq_stop form).
// Init burst always sends iq_stop (enable=false per Thetis sendIQStartStop(0,false)).
QString TciProtocol::buildIqStopLine(int rx)
{
    return QStringLiteral("iq_stop:%1;").arg(rx);
}

// From Thetis TCIServer.cs:5364-5370 [v2.10.3.13] — sendIQSampleRate.
QString TciProtocol::buildIqSampleRateLine(int sr)
{
    return QStringLiteral("iq_samplerate:%1;").arg(sr);
}

// ── Audio-stream helpers ──────────────────────────────────────────────────────

// From Thetis TCIServer.cs:5372-5375 [v2.10.3.13] — sendAudioSampleRate.
QString TciProtocol::buildAudioSampleRateLine(int sr)
{
    return QStringLiteral("audio_samplerate:%1;").arg(sr);
}

// From Thetis TCIServer.cs:5377-5380 [v2.10.3.13] — sendAudioStreamSampleType.
QString TciProtocol::buildAudioStreamSampleTypeLine(const QString& typeLower)
{
    return QStringLiteral("audio_stream_sample_type:%1;").arg(typeLower);
}

// From Thetis TCIServer.cs:5382-5385 [v2.10.3.13] — sendAudioStreamChannels.
QString TciProtocol::buildAudioStreamChannelsLine(int n)
{
    return QStringLiteral("audio_stream_channels:%1;").arg(n);
}

// From Thetis TCIServer.cs:5387-5390 [v2.10.3.13] — sendAudioStreamSamples.
QString TciProtocol::buildAudioStreamSamplesLine(int n)
{
    return QStringLiteral("audio_stream_samples:%1;").arg(n);
}

// From Thetis TCIServer.cs:5392-5395 [v2.10.3.13] — sendTxStreamAudioBuffering.
QString TciProtocol::buildTxStreamAudioBufferingLine(int ms)
{
    return QStringLiteral("tx_stream_audio_buffering:%1;").arg(ms);
}

// ── Mute / volume / MON helpers ───────────────────────────────────────────────

// From Thetis TCIServer.cs:2158-2162 [v2.10.3.13] — sendMute.
QString TciProtocol::buildMuteLine(bool muted)
{
    return QStringLiteral("mute:%1;").arg(muted ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:2163-2167 [v2.10.3.13] — sendMuteRX.
QString TciProtocol::buildRxMuteLine(int rx, bool muted)
{
    return QStringLiteral("rx_mute:%1,%2;").arg(rx).arg(muted ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:2173-2179 [v2.10.3.13] — sendVolume.
// Clamped -60..0 dB; F1 C-locale. Thetis returns early if out of range;
// Phase 4 Task 4.2 hardcodes 0.0 dB which is within range.
QString TciProtocol::buildVolumeLine(double db)
{
    if (db < -60.0) { db = -60.0; }
    if (db >   0.0) { db =   0.0; }
    return QStringLiteral("volume:%1;").arg(QString::number(db, 'f', 1));
}

// From Thetis TCIServer.cs:2168-2172 [v2.10.3.13] — sendMONEnable.
QString TciProtocol::buildMonEnableLine(bool en)
{
    return QStringLiteral("mon_enable:%1;").arg(en ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:2180-2186 [v2.10.3.13] — sendMONVolume.
// Clamped -60..0 dB; F1 C-locale.
QString TciProtocol::buildMonVolumeLine(double db)
{
    if (db < -60.0) { db = -60.0; }
    if (db >   0.0) { db =   0.0; }
    return QStringLiteral("mon_volume:%1;").arg(QString::number(db, 'f', 1));
}

// ── Start / stop ──────────────────────────────────────────────────────────────

// From Thetis TCIServer.cs:1868-1875 / 2354-2360 [v2.10.3.13] — sendStartStop.
QString TciProtocol::buildStartStopLine(bool powerOn)
{
    return powerOn ? QStringLiteral("start;") : QStringLiteral("stop;");
}

// NereusSDR architectural divergence — Slice A/B/C/D maps to trx:N wire format.
// Identity through Phase 3; same mapping in subsequent phases (design doc §1.2).
int TciProtocol::sliceToTrx(int slice) { return slice; }
int TciProtocol::trxToSlice(int trx) { return trx; }

void TciProtocol::resetDispatchCounters()
{
    m_setDispatchCount = 0;
    m_queryDispatchCount = 0;
}

// From Thetis TCIServer.cs:4924-5128 [v2.10.3.13] — 60-case set-command switch.
// Phase 6 adds vfo, vfo_lock, lock cases; remaining cases land Phase 7+.
QString TciProtocol::handleSetCommand(const QString& name, const QStringList& args)
{
    ++m_setDispatchCount;

    // From Thetis TCIServer.cs:4929-4931 [v2.10.3.13] — vfo case (set switch).
    // handleVFOMessage at TCIServer.cs:3724 [v2.10.3.13] dispatches by args.Length:
    //   args.Length == 3 → set; args.Length == 2 → query (unicast response).
    if (name == QStringLiteral("vfo")) {
        return handleVfoCommand(args);
    }

    // From Thetis TCIServer.cs:4962-4964 [v2.10.3.13] — lock case (set switch).
    // 2-arg form drops channel per sendLock at TCIServer.cs:1921-1925 [v2.10.3.13].
    if (name == QStringLiteral("lock")) {
        return handleLockCommand(args);
    }

    // From Thetis TCIServer.cs:4965-4967 [v2.10.3.13] — vfo_lock case (set switch).
    if (name == QStringLiteral("vfo_lock")) {
        return handleVfoLockCommand(args);
    }

    // Phase 7: mode/filter family.
    if (name == QStringLiteral("modulation"))     { return handleModulationCommand(args); }
    if (name == QStringLiteral("rx_filter_band")) { return handleRxFilterBandCommand(args); }

    // Phase 8: TRX family.
    // From Thetis TCIServer.cs:4932 [v2.10.3.13] — trx case in set switch.
    if (name == QStringLiteral("trx"))          { return handleTrxCommand(args); }
    // From Thetis TCIServer.cs:4935 [v2.10.3.13] — split_enable case in set switch.
    if (name == QStringLiteral("split_enable")) { return handleSplitEnableCommand(args); }
    // From Thetis TCIServer.cs:5061 [v2.10.3.13] — mute case in set switch.
    if (name == QStringLiteral("mute"))         { return handleMuteSetCommand(args); }
    // From Thetis TCIServer.cs:5067 [v2.10.3.13] — rx_mute case in set switch.
    if (name == QStringLiteral("rx_mute"))      { return handleRxMuteCommand(args); }

    // Phase 9: DSP family.
    // From Thetis TCIServer.cs:4950 [v2.10.3.13] — rx_nb_enable case in set switch.
    if (name == QStringLiteral("rx_nb_enable"))    { return handleRxNbEnableCommand(args); }
    // From Thetis TCIServer.cs:4953 [v2.10.3.13] — rx_bin_enable case in set switch.
    if (name == QStringLiteral("rx_bin_enable"))   { return handleRxBinEnableCommand(args); }
    // From Thetis TCIServer.cs:4956 [v2.10.3.13] — rx_apf_enable case in set switch.
    if (name == QStringLiteral("rx_apf_enable"))   { return handleRxApfEnableCommand(args); }
    // From Thetis TCIServer.cs:4959 [v2.10.3.13] — rx_nf_enable case in set switch.
    if (name == QStringLiteral("rx_nf_enable"))    { return handleRxNfEnableCommand(args); }
    // From Thetis TCIServer.cs:5103 [v2.10.3.13] — rx_anf_enable case in set switch.
    if (name == QStringLiteral("rx_anf_enable"))   { return handleRxAnfEnableCommand(args); }
    // From Thetis TCIServer.cs:5097 [v2.10.3.13] — rx_nr_enable case in set switch.
    if (name == QStringLiteral("rx_nr_enable"))    { return handleRxNrEnableCommand(args); }
    // From Thetis TCIServer.cs:5100 [v2.10.3.13] — rx_nr_enable_ex case in set switch.
    if (name == QStringLiteral("rx_nr_enable_ex")) { return handleRxNrEnableExCommand(args); }
    // From Thetis TCIServer.cs:5112 [v2.10.3.13] — agc_mode case in set switch.
    if (name == QStringLiteral("agc_mode"))        { return handleAgcModeCommand(args); }
    // From Thetis TCIServer.cs:5115 [v2.10.3.13] — agc_gain case in set switch.
    if (name == QStringLiteral("agc_gain"))        { return handleAgcGainCommand(args); }
    // From Thetis TCIServer.cs:4968 [v2.10.3.13] — sql_enable case in set switch.
    if (name == QStringLiteral("sql_enable"))      { return handleSqlEnableCommand(args); }
    // From Thetis TCIServer.cs:4971 [v2.10.3.13] — sql_level case in set switch.
    if (name == QStringLiteral("sql_level"))       { return handleSqlLevelCommand(args); }
    // From Thetis TCIServer.cs:4938 [v2.10.3.13] — rit_enable case in set switch.
    if (name == QStringLiteral("rit_enable"))      { return handleRitEnableCommand(args); }
    // From Thetis TCIServer.cs:4944 [v2.10.3.13] — rit_offset case in set switch.
    if (name == QStringLiteral("rit_offset"))      { return handleRitOffsetCommand(args); }
    // From Thetis TCIServer.cs:4941 [v2.10.3.13] — xit_enable case in set switch.
    if (name == QStringLiteral("xit_enable"))      { return handleXitEnableCommand(args); }
    // From Thetis TCIServer.cs:4947 [v2.10.3.13] — xit_offset case in set switch.
    if (name == QStringLiteral("xit_offset"))      { return handleXitOffsetCommand(args); }
    // From Thetis TCIServer.cs:5109 [v2.10.3.13] — rx_balance case in set switch.
    if (name == QStringLiteral("rx_balance"))      { return handleRxBalanceCommand(args); }

    // Phase 11: IQ stream family.
    // From Thetis TCIServer.cs:5016 [v2.10.3.13] — iq_samplerate case in set switch.
    if (name == QStringLiteral("iq_samplerate"))             { return handleIqSampleRateCommand(args); }
    // From Thetis TCIServer.cs:5022 [v2.10.3.13] — iq_start case (STUB Phase 11).
    if (name == QStringLiteral("iq_start"))                  { return handleIqStartStopCommand(args, true); }
    // From Thetis TCIServer.cs:5025 [v2.10.3.13] — iq_stop case (STUB Phase 11).
    if (name == QStringLiteral("iq_stop"))                   { return handleIqStartStopCommand(args, false); }

    // Phase 10: audio stream family.
    // From Thetis TCIServer.cs:5019 [v2.10.3.13] — audio_samplerate case in set switch.
    if (name == QStringLiteral("audio_samplerate"))          { return handleAudioSampleRateCommand(args); }
    // From Thetis TCIServer.cs:5034 [v2.10.3.13] — audio_stream_sample_type case.
    if (name == QStringLiteral("audio_stream_sample_type"))  { return handleAudioStreamSampleTypeCommand(args); }
    // From Thetis TCIServer.cs:5037 [v2.10.3.13] — audio_stream_channels case.
    if (name == QStringLiteral("audio_stream_channels"))     { return handleAudioStreamChannelsCommand(args); }
    // From Thetis TCIServer.cs:5040 [v2.10.3.13] — audio_stream_samples case.
    if (name == QStringLiteral("audio_stream_samples"))      { return handleAudioStreamSamplesCommand(args); }
    // From Thetis TCIServer.cs:5028 [v2.10.3.13] — audio_start case (STUB Phase 10).
    if (name == QStringLiteral("audio_start"))               { return handleAudioStartCommand(args, true); }
    // From Thetis TCIServer.cs:5031 [v2.10.3.13] — audio_stop case (STUB Phase 10).
    if (name == QStringLiteral("audio_stop"))                { return handleAudioStartCommand(args, false); }
    // From Thetis TCIServer.cs:5064 [v2.10.3.13] — volume case in set switch.
    if (name == QStringLiteral("volume"))                    { return handleVolumeCommand(args); }
    // From Thetis TCIServer.cs:5070 [v2.10.3.13] — mon_volume case in set switch.
    if (name == QStringLiteral("mon_volume"))                { return handleMonVolumeCommand(args); }

    // Phase 13: bespoke _ex commands.
    // From Thetis TCIServer.cs:5010 [v2.10.3.13] — rx_enable case in set switch.
    if (name == QStringLiteral("rx_enable"))     { return handleRxEnableCommand(args); }
    // From Thetis TCIServer.cs:5118 [v2.10.3.13] — rx_ctun_ex case in set switch.
    if (name == QStringLiteral("rx_ctun_ex"))    { return handleRxCtunExCommand(args); }
    // From Thetis TCIServer.cs:5121 [v2.10.3.13] — tx_profile_ex case in set switch.
    if (name == QStringLiteral("tx_profile_ex")) { return handleTxProfileExSetCommand(args); }
    // From Thetis TCIServer.cs:5124 [v2.10.3.13] — calibration_ex case in set switch.
    if (name == QStringLiteral("calibration_ex")) { return handleCalibrationExCommand(args); }

    // Phase 12: Spot + CW stubs.
    // From Thetis TCIServer.cs:5049 [v2.10.3.13] — spot case in set switch.
    if (name == QStringLiteral("spot"))                   { return handleSpotCommand(args); }
    // From Thetis TCIServer.cs:5052 [v2.10.3.13] — spot_delete case in set switch.
    if (name == QStringLiteral("spot_delete"))            { return handleSpotDeleteCommand(args); }
    // From Thetis TCIServer.cs:5082 [v2.10.3.13] — spot_simulate_click case in set switch.
    if (name == QStringLiteral("spot_simulate_click"))    { return handleSpotSimulateClickCommand(args); }
    // From Thetis TCIServer.cs:4989 [v2.10.3.13] — cw_macros_speed_up case in set switch.
    if (name == QStringLiteral("cw_macros_speed_up"))     { return handleCwMacrosSpeedUpCommand(args); }
    // From Thetis TCIServer.cs:4992 [v2.10.3.13] — cw_macros_speed_down case in set switch.
    if (name == QStringLiteral("cw_macros_speed_down"))   { return handleCwMacrosSpeedDownCommand(args); }
    // From Thetis TCIServer.cs:5001 [v2.10.3.13] — cw_msg case in set switch.
    if (name == QStringLiteral("cw_msg"))                 { return handleCwMsgCommand(args); }

    return {};
}

// From Thetis TCIServer.cs:5134-5197 [v2.10.3.13] — 21-case query-command switch.
// Phase 6: vfo, lock, vfo_lock have no 1-arg query cases in Thetis query switch;
// they self-dispatch (set vs. query) by args.size() inside handleSetCommand.
// Phase 8: adds the first 1-arg query case (mute).
QString TciProtocol::handleQueryCommand(const QString& name)
{
    ++m_queryDispatchCount;

    // Phase 8 — first 1-arg query case.
    // From Thetis TCIServer.cs:5145 [v2.10.3.13] — case "mute" in 1-arg query switch.
    if (name == QStringLiteral("mute")) {
        return handleMuteQueryCommand();
    }

    // Phase 11: IQ stream query cases.
    // From Thetis TCIServer.cs:5175 [v2.10.3.13] — iq_samplerate query case.
    if (name == QStringLiteral("iq_samplerate")) {
        return handleIqSampleRateQueryCommand();
    }

    // Phase 10: audio stream / volume query cases.
    // From Thetis TCIServer.cs:5178 [v2.10.3.13] — audio_samplerate query case.
    if (name == QStringLiteral("audio_samplerate")) {
        return handleAudioSampleRateQueryCommand();
    }
    // From Thetis TCIServer.cs:5148 [v2.10.3.13] — volume query case.
    if (name == QStringLiteral("volume")) {
        return handleVolumeQueryCommand();
    }
    // From Thetis TCIServer.cs:5154 [v2.10.3.13] — mon_volume query case.
    if (name == QStringLiteral("mon_volume")) {
        return handleMonVolumeQueryCommand();
    }

    // Phase 12: Spot + CW stubs (query side).
    // From Thetis TCIServer.cs:5184 [v2.10.3.13] — spot_clear case in 1-arg query switch.
    if (name == QStringLiteral("spot_clear")) {
        return handleSpotClearCommand();
    }

    // Phase 13: bespoke _ex query cases.
    // From Thetis TCIServer.cs:5187 [v2.10.3.13] — tx_profiles_ex case in 1-arg query switch.
    if (name == QStringLiteral("tx_profiles_ex")) {
        return handleTxProfilesExQueryCommand();
    }
    // From Thetis TCIServer.cs:5193 [v2.10.3.13] — tx_profile_ex case in 1-arg query switch.
    // Thetis passes tmpArgs=string[0] to handleTXProfile (zero-length → query path).
    if (name == QStringLiteral("tx_profile_ex")) {
        return handleTxProfileExQueryCommand();
    }
    // From Thetis TCIServer.cs:5190 [v2.10.3.13] — shutdown_ex case in 1-arg query switch.
    if (name == QStringLiteral("shutdown_ex")) {
        return handleShutdownExCommand();
    }

    return {};
}

// ── VFO family handlers (Phase 6) ───────────────────────────────────────────

// From Thetis TCIServer.cs:3724-3833 [v2.10.3.13] — handleVFOMessage.
// 3-arg path: set VFO frequency; enqueue broadcast notification.
// 2-arg path: query VFO frequency; return as direct response (Phase 6 stub —
//   Thetis routes through VFOChange → sendTextFrame broadcast; Phase 14 adds
//   priority-queue coalescing; for Phase 6 we return the value directly).
// UseRX1VFOaForRX2VFOa quirk (TCIServer.cs:3732 [v2.10.3.13]) deferred to
// Phase 6+ refinement (compat placeholder row notes this).
QString TciProtocol::handleVfoCommand(const QStringList& args)
{
    if (args.size() < 2) {
        return {};
    }
    bool ok1 = false;
    bool ok2 = false;
    const int rx = args.at(0).trimmed().toInt(&ok1);
    const int chan = args.at(1).trimmed().toInt(&ok2);
    if (!ok1 || !ok2) {
        return {};
    }

    if (args.size() >= 3) {
        // 3-arg set path.
        // From Thetis TCIServer.cs:3746-3793 [v2.10.3.13] — set VFOAFreq/VFOBFreq.
        bool ok3 = false;
        const qint64 hz = args.at(2).trimmed().toLongLong(&ok3);
        if (!ok3) {
            return {};
        }
        // Write to mock via QMetaObject::invokeMethod (DirectConnection — test thread).
        // Production RadioModel exposes setVfoHz as Q_INVOKABLE too (Phase 17+).
        QMetaObject::invokeMethod(m_radio, "setVfoHz",
                                  Qt::DirectConnection,
                                  Q_ARG(int, rx),
                                  Q_ARG(int, chan),
                                  Q_ARG(qint64, hz));
        // Phase 15: route through coalescer (Layer 3 of Thetis 3-layer throttle
        // at TCIServer.cs:1722-1727 [v2.10.3.13]) instead of direct enqueue.
        // Rapid VFO bursts within a 5ms drain tick collapse to 1 frame per key.
        // From Thetis sendVFO at TCIServer.cs:2061-2093 [v2.10.3.13] — format string.
        const QString vfoKey   = QStringLiteral("vfo:%1,%2").arg(rx).arg(chan);
        const QString vfoFrame = QStringLiteral("vfo:%1,%2,%3;").arg(rx).arg(chan).arg(hz);
        m_vfoCoalescer.update(vfoKey, vfoFrame);
        return {};
    }

    // 2-arg query path.
    // From Thetis TCIServer.cs:3793-3833 [v2.10.3.13] — reads VFOAFreq/VFOBFreq.
    qint64 hz = 0;
    QMetaObject::invokeMethod(m_radio, "vfoHz",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(qint64, hz),
                              Q_ARG(int, rx),
                              Q_ARG(int, chan));
    // From Thetis sendVFO at TCIServer.cs:2093 [v2.10.3.13] — format string.
    return QStringLiteral("vfo:%1,%2,%3;").arg(rx).arg(chan).arg(hz);
}

// From Thetis TCIServer.cs:3284-3302 [v2.10.3.13] — handleVFOLock.
// 3-arg path: set vfo_lock state; enqueue broadcast notification.
// 2-arg path: query vfo_lock state; return as direct response.
QString TciProtocol::handleVfoLockCommand(const QStringList& args)
{
    if (args.size() < 2) {
        return {};
    }
    bool ok1 = false;
    bool ok2 = false;
    const int rx = args.at(0).trimmed().toInt(&ok1);
    const int chan = args.at(1).trimmed().toInt(&ok2);
    if (!ok1 || !ok2) {
        return {};
    }

    if (args.size() >= 3) {
        // 3-arg set path.
        // From Thetis TCIServer.cs:3298 [v2.10.3.13] — trySetVFOLockState.
        const QString boolStr = args.at(2).trimmed().toLower();
        if (boolStr != QStringLiteral("true") && boolStr != QStringLiteral("false")) {
            return {};
        }
        const bool locked = (boolStr == QStringLiteral("true"));
        QMetaObject::invokeMethod(m_radio, "setVfoLock",
                                  Qt::DirectConnection,
                                  Q_ARG(int, rx),
                                  Q_ARG(int, chan),
                                  Q_ARG(bool, locked));
        // From Thetis sendVFOLock at TCIServer.cs:1926-1930 [v2.10.3.13] — format.
        m_pendingNotifications << QStringLiteral("vfo_lock:%1,%2,%3;")
                                      .arg(rx).arg(chan)
                                      .arg(locked ? QStringLiteral("true") : QStringLiteral("false"));
        return {};
    }

    // 2-arg query path.
    // From Thetis TCIServer.cs:3292-3293 [v2.10.3.13] — tryGetVFOLockState + sendVFOLock.
    bool locked = false;
    QMetaObject::invokeMethod(m_radio, "vfoLock",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(bool, locked),
                              Q_ARG(int, rx),
                              Q_ARG(int, chan));
    // From Thetis sendVFOLock at TCIServer.cs:1926-1930 [v2.10.3.13] — format.
    return QStringLiteral("vfo_lock:%1,%2,%3;")
        .arg(rx).arg(chan)
        .arg(locked ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:3265-3283 [v2.10.3.13] — handleLock.
// 2-arg path: set lock state; enqueue broadcast notification.
// 1-arg path: query lock state; return as direct response.
// Note: drops channel arg — sendLock at TCIServer.cs:1921-1925 [v2.10.3.13]
// emits lock:rx,bool; (no channel).
QString TciProtocol::handleLockCommand(const QStringList& args)
{
    if (args.size() < 1) {
        return {};
    }
    bool ok1 = false;
    const int rx = args.at(0).trimmed().toInt(&ok1);
    if (!ok1) {
        return {};
    }

    if (args.size() >= 2) {
        // 2-arg set path.
        // From Thetis TCIServer.cs:3277-3282 [v2.10.3.13] — VFOALock/VFOBLock write.
        const QString boolStr = args.at(1).trimmed().toLower();
        if (boolStr != QStringLiteral("true") && boolStr != QStringLiteral("false")) {
            return {};
        }
        const bool locked = (boolStr == QStringLiteral("true"));
        QMetaObject::invokeMethod(m_radio, "setLock",
                                  Qt::DirectConnection,
                                  Q_ARG(int, rx),
                                  Q_ARG(bool, locked));
        // From Thetis sendLock at TCIServer.cs:1921-1925 [v2.10.3.13] — format.
        m_pendingNotifications << QStringLiteral("lock:%1,%2;")
                                      .arg(rx)
                                      .arg(locked ? QStringLiteral("true") : QStringLiteral("false"));
        return {};
    }

    // 1-arg query path.
    // From Thetis TCIServer.cs:3271-3273 [v2.10.3.13] — sendLock(rx, VFOALock/VFOBLock).
    bool locked = false;
    QMetaObject::invokeMethod(m_radio, "lock",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(bool, locked),
                              Q_ARG(int, rx));
    // From Thetis sendLock at TCIServer.cs:1921-1925 [v2.10.3.13] — format.
    return QStringLiteral("lock:%1,%2;")
        .arg(rx)
        .arg(locked ? QStringLiteral("true") : QStringLiteral("false"));
}

// ── Mode / filter family handlers (Phase 7) ─────────────────────────────────

// From Thetis TCIServer.cs:4926 [v2.10.3.13] — modulation case in set switch.
// handleModulationMessage at TCIServer.cs:3837-3940 [v2.10.3.13] dispatches by args.size():
//   args.size() == 2 → set (parse mode string, set DSPMode + enqueue notification)
//   args.size() == 1 → query (return current mode uppercase)
// CWLUbecomesCW transform at TCIServer.cs:2148-2153 [v2.10.3.13] is DEFERRED
//   (send-side, Phase 11/12 follow-up — wiring requires touching buildModulationLine).
// CWbecomesCWUabove10mhz transform at TCIServer.cs:3868-3895 [v2.10.3.13] is DEFERRED
//   (needs VFOATX/VFOBTX accessors on TestMockRadioModel — future phase).
QString TciProtocol::handleModulationCommand(const QStringList& args)
{
    if (args.size() < 1) {
        return {};
    }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok) {
        return {};
    }

    if (args.size() == 1) {
        // Query path — return current mode uppercase.
        // From Thetis TCIServer.cs:3933-3940 [v2.10.3.13] — query dispatch.
        QString currentMode;
        QMetaObject::invokeMethod(m_radio, "mode", Qt::DirectConnection,
                                  Q_RETURN_ARG(QString, currentMode),
                                  Q_ARG(int, rx));
        return QStringLiteral("modulation:%1,%2;").arg(rx).arg(currentMode.toUpper());
    }

    if (args.size() >= 2) {
        // Set path — canonicalize mode string.
        // From Thetis TCIServer.cs:3847-3911 [v2.10.3.13] — switch on args[1].ToLower().
        //change if needed [2.10.3.6]MW0LGE fixes #365  [original inline comment from TCIServer.cs:3869]
        // Note: nfm and fm are aliases for DSPMode.FM (TCIServer.cs:3865-3866).
        // Unknown modes map to DSPMode.FIRST, which is treated as a no-op
        // (TCIServer.cs:3909-3913 [v2.10.3.13] — silent drop).
        const QString modeIn = args.at(1).trimmed().toLower();
        QString modeOut;
        if      (modeIn == QStringLiteral("lsb"))  { modeOut = QStringLiteral("LSB"); }
        else if (modeIn == QStringLiteral("usb"))  { modeOut = QStringLiteral("USB"); }
        else if (modeIn == QStringLiteral("dsb"))  { modeOut = QStringLiteral("DSB"); }
        else if (modeIn == QStringLiteral("am"))   { modeOut = QStringLiteral("AM"); }
        else if (modeIn == QStringLiteral("sam"))  { modeOut = QStringLiteral("SAM"); }
        else if (modeIn == QStringLiteral("nfm") || modeIn == QStringLiteral("fm")) {
            // nfm and fm both alias to DSPMode.FM per TCIServer.cs:3865-3866 [v2.10.3.13].
            modeOut = QStringLiteral("FM");
        }
        else if (modeIn == QStringLiteral("cw")) {
            // CWbecomesCWUabove10mhz transform at TCIServer.cs:3868-3895 [v2.10.3.13] is
            // DEFERRED — `cw` → CWL until VFOATX/VFOBTX state arrives.
            modeOut = QStringLiteral("CWL");
        }
        else if (modeIn == QStringLiteral("cwl"))  { modeOut = QStringLiteral("CWL"); }
        else if (modeIn == QStringLiteral("cwu"))  { modeOut = QStringLiteral("CWU"); }
        else if (modeIn == QStringLiteral("digl")) { modeOut = QStringLiteral("DIGL"); }
        else if (modeIn == QStringLiteral("digu")) { modeOut = QStringLiteral("DIGU"); }
        else {
            // Unknown mode — silent drop per Thetis DSPMode.FIRST default
            // at TCIServer.cs:3909-3913 [v2.10.3.13].
            return {};
        }

        QMetaObject::invokeMethod(m_radio, "setMode", Qt::DirectConnection,
                                  Q_ARG(int, rx), Q_ARG(QString, modeOut));
        // MW0LGE_22b mods are uppcase on the sun, replicate
        // From Thetis TCIServer.cs:2155 [v2.10.3.13] — sendMode format string.
        m_pendingNotifications << QStringLiteral("modulation:%1,%2;").arg(rx).arg(modeOut);
        return {};
    }

    return {};
}

// From Thetis TCIServer.cs:5085 [v2.10.3.13] — rx_filter_band case in set switch.
// handleRxFilterBand at TCIServer.cs:4366-4413 [v2.10.3.13] dispatches by args.size():
//   args.size() == 3 → set (rx, low, high — all int)
//     calls UpdateRX1Filters/UpdateRX2Filters at TCIServer.cs:4393-4399 [v2.10.3.13].
//   args.size() == 1 → query (rx)
//     reads RX1FilterLow/High or RX2FilterLow/High at TCIServer.cs:4380-4384 [v2.10.3.13].
QString TciProtocol::handleRxFilterBandCommand(const QStringList& args)
{
    if (args.size() < 1) {
        return {};
    }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok) {
        return {};
    }

    if (args.size() == 1) {
        // Query path.
        // From Thetis TCIServer.cs:4380-4384 [v2.10.3.13] — RX1FilterLow/High read.
        int low = 0, high = 0;
        QMetaObject::invokeMethod(m_radio, "filterLow", Qt::DirectConnection,
                                  Q_RETURN_ARG(int, low), Q_ARG(int, rx));
        QMetaObject::invokeMethod(m_radio, "filterHigh", Qt::DirectConnection,
                                  Q_RETURN_ARG(int, high), Q_ARG(int, rx));
        return QStringLiteral("rx_filter_band:%1,%2,%3;").arg(rx).arg(low).arg(high);
    }

    if (args.size() >= 3) {
        // Set path.
        // From Thetis TCIServer.cs:4393-4399 [v2.10.3.13] — UpdateRX1Filters/UpdateRX2Filters.
        bool ok2 = false, ok3 = false;
        const int low  = args.at(1).trimmed().toInt(&ok2);
        const int high = args.at(2).trimmed().toInt(&ok3);
        if (!ok2 || !ok3) {
            return {};
        }
        QMetaObject::invokeMethod(m_radio, "setFilterBand", Qt::DirectConnection,
                                  Q_ARG(int, rx), Q_ARG(int, low), Q_ARG(int, high));
        // From Thetis sendFilterBand at TCIServer.cs:2349-2353 [v2.10.3.13] — format string.
        m_pendingNotifications << QStringLiteral("rx_filter_band:%1,%2,%3;")
                                      .arg(rx).arg(low).arg(high);
        return {};
    }

    return {};
}

// ── TRX family handlers (Phase 8) ─────────────────────────────────────────────

// From Thetis TCIServer.cs:3459-3559 [v2.10.3.13] — handleTrxMessage.
// Phase 8 SIMPLIFIED scope: rx + mox parse only. Broadcasts trx:rx,bool;.
// DEFERRED to Phase 17 (TX audio): the m_txUsesTCIAudio / m_tciPttActive
// mutex acquire/release + shouldIgnoreTrxForCurrentCwBreakIn + VFOATX/VFOBTX
// gating + RX2Enabled gating. Phase 17 must restore the full handler body.
//
// 1-arg query path (TCIServer.cs:3555-3558 [v2.10.3.13]):
//   sendMOX(rx, console.ThreadSafeTCIAccessor.MOX, m_txUsesTCIAudio);
//   Phase 8: emits trx:rx,bool; without ",tci" suffix (m_txUsesTCIAudio = false always).
QString TciProtocol::handleTrxCommand(const QStringList& args)
{
    if (args.size() < 1) {
        return {};
    }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok) {
        return {};
    }

    if (args.size() >= 2) {
        // Set path — parse mox bool.
        // From Thetis TCIServer.cs:3468-3470 [v2.10.3.13] — bMox parse.
        const QString boolStr = args.at(1).trimmed().toLower();
        if (boolStr != QStringLiteral("true") && boolStr != QStringLiteral("false")) {
            return {};
        }
        const bool mox = (boolStr == QStringLiteral("true"));
        // Phase 8: store via setMox. In Phase 17 this becomes TCIPTT + VFOATX/VFOBTX logic.
        QMetaObject::invokeMethod(m_radio, "setMox",
                                  Qt::DirectConnection,
                                  Q_ARG(bool, mox));

        // ── Phase 3J-1 bench fix (2026-05-10): emit MoxChange-style broadcast
        //    sequence so WSJT-X recognises that TX has actually engaged.
        //
        // Bench discovery: WSJT-X sends trx:0,true,tci;, the server acquires
        // the TX mutex, and the radio keys — but WSJT-X then sits silent for
        // 11 seconds (the full FT8 TX window) without sending a single
        // TX_AUDIO_STREAM binary frame.  Diagnostic capture showed our only
        // outbound text was the immediate trx echo from this set-path
        // handler, with the ",tci" suffix.  WSJT-X ignores that.
        //
        // Source-first audit of Thetis TCIServer.cs:3459-3559 [v2.10.3.13]:
        // handleTrxMessage does NOT broadcast any notification itself.  It
        // sets m_txUsesTCIAudio + m_tciPttActive + TCIPTT properties and
        // RETURNS.  The console PTT loop (console.cs:25461-25465
        // [v2.10.3.13]) polls _tci_ptt, sets PTTMode.TCI + chkMOX.Checked
        // = true.  When MOX actually changes, the console fires its
        // MoxChange delegate (TCIServer.cs:1410-1438 [v2.10.3.13]) which
        // sends `tx_enable:other_rx,false;` then `sendMOX(0, true)` →
        // `trx:0,true;` (no ",tci" suffix — sendMOX defaults
        // signalTCI=false).  WSJT-X waits for THAT broadcast (no suffix)
        // before streaming audio.
        //
        // The proper Thetis-faithful fix is to wire MoxController::moxChanged
        // → TciServer broadcast hook.  For Phase 3J-1 bench-unblock we
        // emit the MoxChange-style frames synchronously here — the trx
        // command's setMox call propagates to MOX-engage via the same
        // path Thetis uses, so the relative ordering of broadcast vs
        // actual MOX engage is close enough for WSJT-X.
        //
        // From Thetis TCIServer.cs:1414-1437 [v2.10.3.13] — MoxChange:
        //   if (newMox) {
        //       if (rx == 1) {
        //           if (RX2Enabled) sendTXEnable(1, false);  // disable RX2 TX
        //       } else {
        //           sendTXEnable(0, false);                  // disable RX1 TX
        //       }
        //   } else {
        //       /* symmetric release — sendTXEnable back to true */
        //   }
        //   sendMOX(rx - 1, newMox);  // signalTCI default false → no suffix
        //
        // Single-RX scope: RX2Enabled=false, so the rx==1 branch (TX on RX1
        // in 1-indexed Thetis === rx=0 in our 0-indexed) emits NO
        // tx_enable line.  Only the trx broadcast (no suffix) is needed
        // for WSJT-X.  For multi-RX in Phase 3F we'll add the
        // tx_enable:other,false; branch.
        if (rx == 0) {
            // Single-RX TX path: only the trx broadcast (no tx_enable in
            // this MoxChange branch because RX2 is not enabled in
            // Phase 3J-1 scope).
            m_pendingNotifications << QStringLiteral("trx:%1,%2;")
                                          .arg(rx)
                                          .arg(mox ? QStringLiteral("true") : QStringLiteral("false"));
        } else {
            // rx==1 (TXing RX2): also disable RX1's tx_enable per
            // TCIServer.cs:1421-1422 [v2.10.3.13] (`sendTXEnable(0, false)`
            // when not rx==1).
            m_pendingNotifications << QStringLiteral("tx_enable:0,%1;")
                                          .arg(mox ? QStringLiteral("false") : QStringLiteral("true"));
            m_pendingNotifications << QStringLiteral("trx:%1,%2;")
                                          .arg(rx)
                                          .arg(mox ? QStringLiteral("true") : QStringLiteral("false"));
        }
        return {};
    }

    // 1-arg query path.
    // From Thetis TCIServer.cs:3555-3558 [v2.10.3.13] — sendMOX(rx, MOX, m_txUsesTCIAudio).
    // Phase 8: m_txUsesTCIAudio is always false → no ",tci" suffix on the response.
    bool mox = false;
    QMetaObject::invokeMethod(m_radio, "mox",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(bool, mox));
    // From Thetis sendMOX at TCIServer.cs:2121-2131 [v2.10.3.13] — format string.
    return QStringLiteral("trx:%1,%2;")
        .arg(rx)
        .arg(mox ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:3091-3127 [v2.10.3.13] — handleSplitEnableMessage.
// 2-arg path (set): parse rx + bool → write VFOSplit → broadcast split_enable:rx,bool;.
// 1-arg path (query): read VFOSplit → return split_enable:rx,bool; as direct response.
// SplitFromCATorTCIcancelsQSPLIT at TCIServer.cs:3107 [v2.10.3.13] deferred (no
// quickSplit state on mock; Phase 17 wires when QuickSplitEnabled arrives).
//
// From Thetis sendSplit at TCIServer.cs:1876-1880 [v2.10.3.13] — format:
//   "split_enable:" + rx + "," + bSplit.ToString().ToLower() + ";".
QString TciProtocol::handleSplitEnableCommand(const QStringList& args)
{
    if (args.size() < 1) {
        return {};
    }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok) {
        return {};
    }

    if (args.size() >= 2) {
        // Set path — parse split bool.
        // From Thetis TCIServer.cs:3098-3121 [v2.10.3.13] — VFOSplit write.
        const QString boolStr = args.at(1).trimmed().toLower();
        if (boolStr != QStringLiteral("true") && boolStr != QStringLiteral("false")) {
            return {};
        }
        const bool split = (boolStr == QStringLiteral("true"));
        QMetaObject::invokeMethod(m_radio, "setSplit",
                                  Qt::DirectConnection,
                                  Q_ARG(int, rx),
                                  Q_ARG(bool, split));
        // From Thetis sendSplit at TCIServer.cs:1878 [v2.10.3.13] — broadcast format.
        m_pendingNotifications << QStringLiteral("split_enable:%1,%2;")
                                      .arg(rx)
                                      .arg(split ? QStringLiteral("true") : QStringLiteral("false"));
        return {};
    }

    // 1-arg query path.
    // From Thetis TCIServer.cs:3123-3127 [v2.10.3.13] — sendSplit(rx, VFOSplit).
    bool split = false;
    QMetaObject::invokeMethod(m_radio, "split",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(bool, split),
                              Q_ARG(int, rx));
    // From Thetis sendSplit at TCIServer.cs:1878 [v2.10.3.13] — format string.
    return QStringLiteral("split_enable:%1,%2;")
        .arg(rx)
        .arg(split ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:4051-4067 [v2.10.3.13] — handleMute (set path, hasArgs=true).
// 1-arg form (args.size() == 1): parse bool → set MUT + MUT2 (global mute).
// Called from set switch (TCIServer.cs:5061-5064 [v2.10.3.13]).
//
// From Thetis sendMute at TCIServer.cs:2158-2162 [v2.10.3.13] — format:
//   "mute:" + mute.ToString().ToLower() + ";".
QString TciProtocol::handleMuteSetCommand(const QStringList& args)
{
    if (args.size() < 1) {
        return {};
    }
    const QString boolStr = args.at(0).trimmed().toLower();
    if (boolStr != QStringLiteral("true") && boolStr != QStringLiteral("false")) {
        return {};
    }
    const bool muted = (boolStr == QStringLiteral("true"));
    // From Thetis TCIServer.cs:4059-4060 [v2.10.3.13] — MUT = mute; MUT2 = mute.
    QMetaObject::invokeMethod(m_radio, "setGlobalMute",
                              Qt::DirectConnection,
                              Q_ARG(bool, muted));
    // From Thetis sendMute at TCIServer.cs:2160 [v2.10.3.13] — broadcast format.
    m_pendingNotifications << QStringLiteral("mute:%1;")
                                  .arg(muted ? QStringLiteral("true") : QStringLiteral("false"));
    return {};
}

// From Thetis TCIServer.cs:5145 [v2.10.3.13] — mute case in 1-arg query switch.
// handleMute(null, false) → queries MUT || MUT2 → sendMute(...).
// Phase 8: globalMute() models MUT || MUT2 as a single flag per Thetis.
//
// From Thetis sendMute at TCIServer.cs:2160 [v2.10.3.13] — format:
//   "mute:" + mute.ToString().ToLower() + ";".
QString TciProtocol::handleMuteQueryCommand()
{
    bool muted = false;
    // From Thetis TCIServer.cs:4064 [v2.10.3.13] — MUT || MUT2 read.
    QMetaObject::invokeMethod(m_radio, "globalMute",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(bool, muted));
    // From Thetis sendMute at TCIServer.cs:2160 [v2.10.3.13] — format string.
    return QStringLiteral("mute:%1;")
        .arg(muted ? QStringLiteral("true") : QStringLiteral("false"));
}

// From Thetis TCIServer.cs:4069-4090 [v2.10.3.13] — handleMuteRX.
// 2-arg path (set): parse rx + bool → write MUT (rx==0) or MUT2 (rx==1).
// 1-arg path (query): read MUT or MUT2 → return rx_mute:rx,bool; as direct response.
// Called from set switch (TCIServer.cs:5067-5070 [v2.10.3.13]).
//
// From Thetis sendMuteRX at TCIServer.cs:2163-2167 [v2.10.3.13] — format:
//   "rx_mute:" + rx + "," + mute.ToString().ToLower() + ";".
QString TciProtocol::handleRxMuteCommand(const QStringList& args)
{
    if (args.size() < 1) {
        return {};
    }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok) {
        return {};
    }

    if (args.size() >= 2) {
        // Set path.
        // From Thetis TCIServer.cs:4078-4083 [v2.10.3.13] — MUT / MUT2 write.
        const QString boolStr = args.at(1).trimmed().toLower();
        if (boolStr != QStringLiteral("true") && boolStr != QStringLiteral("false")) {
            return {};
        }
        const bool muted = (boolStr == QStringLiteral("true"));
        QMetaObject::invokeMethod(m_radio, "setRxMute",
                                  Qt::DirectConnection,
                                  Q_ARG(int, rx),
                                  Q_ARG(bool, muted));
        // From Thetis sendMuteRX at TCIServer.cs:2165 [v2.10.3.13] — broadcast format.
        m_pendingNotifications << QStringLiteral("rx_mute:%1,%2;")
                                      .arg(rx)
                                      .arg(muted ? QStringLiteral("true") : QStringLiteral("false"));
        return {};
    }

    // 1-arg query path.
    // From Thetis TCIServer.cs:4085-4088 [v2.10.3.13] — sendMuteRX(rx, MUT / MUT2).
    bool muted = false;
    QMetaObject::invokeMethod(m_radio, "rxMute",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(bool, muted),
                              Q_ARG(int, rx));
    // From Thetis sendMuteRX at TCIServer.cs:2165 [v2.10.3.13] — format string.
    return QStringLiteral("rx_mute:%1,%2;")
        .arg(rx)
        .arg(muted ? QStringLiteral("true") : QStringLiteral("false"));
}

// ── DSP family handlers (Phase 9) ─────────────────────────────────────────────

// From Thetis TCIServer.cs:4950 [v2.10.3.13] — rx_nb_enable case in set switch.
// handleRxNbEnable at TCIServer.cs:3192-3207 [v2.10.3.13]:
//   1-arg = query → sendRxNbEnable; 2-arg = set → SetSelectedNB(rx+1, en?1:0).
// sendRxNbEnable format at TCIServer.cs:1901-1905 [v2.10.3.13]:
//   "rx_nb_enable:" + rx + "," + enabled.ToLower() + ";"
QString TciProtocol::handleRxNbEnableCommand(const QStringList& args)
{
    if (args.size() < 1) { return {}; }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok || rx < 0 || rx > 1) { return {}; }

    if (args.size() == 1) {
        // Query path.
        bool en = false;
        QMetaObject::invokeMethod(m_radio, "rxNb", Qt::DirectConnection,
                                  Q_RETURN_ARG(bool, en), Q_ARG(int, rx));
        return QStringLiteral("rx_nb_enable:%1,%2;")
            .arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
    }

    if (args.size() >= 2) {
        // Set path.
        const bool en = args.at(1).trimmed().toLower() == QStringLiteral("true");
        QMetaObject::invokeMethod(m_radio, "setRxNb", Qt::DirectConnection,
                                  Q_ARG(int, rx), Q_ARG(bool, en));
        m_pendingNotifications << QStringLiteral("rx_nb_enable:%1,%2;")
            .arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
        return {};
    }

    return {};
}

// From Thetis TCIServer.cs:4953 [v2.10.3.13] — rx_bin_enable case in set switch.
// handleRxBinEnable at TCIServer.cs:3208-3223 [v2.10.3.13]:
//   1-arg = query → sendRxBinEnable; 2-arg = set → SetBin(rx+1, en).
// sendRxBinEnable format at TCIServer.cs:1906-1910 [v2.10.3.13]:
//   "rx_bin_enable:" + rx + "," + enabled.ToLower() + ";"
QString TciProtocol::handleRxBinEnableCommand(const QStringList& args)
{
    if (args.size() < 1) { return {}; }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok || rx < 0 || rx > 1) { return {}; }

    if (args.size() == 1) {
        bool en = false;
        QMetaObject::invokeMethod(m_radio, "rxBin", Qt::DirectConnection,
                                  Q_RETURN_ARG(bool, en), Q_ARG(int, rx));
        return QStringLiteral("rx_bin_enable:%1,%2;")
            .arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
    }

    if (args.size() >= 2) {
        const bool en = args.at(1).trimmed().toLower() == QStringLiteral("true");
        QMetaObject::invokeMethod(m_radio, "setRxBin", Qt::DirectConnection,
                                  Q_ARG(int, rx), Q_ARG(bool, en));
        m_pendingNotifications << QStringLiteral("rx_bin_enable:%1,%2;")
            .arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
        return {};
    }

    return {};
}

// From Thetis TCIServer.cs:4956 [v2.10.3.13] — rx_apf_enable case in set switch.
// handleRxApfEnable at TCIServer.cs:3224-3247 [v2.10.3.13]:
//   1-arg = query → sendRxApfEnable; 2-arg = set → SetupForm.RX1/RX2APFEnable.
//   Thetis gates on !IsSetupFormNull (TCIServer.cs:3229 [v2.10.3.13]) — DEFERRED to Phase 20.
// sendRxApfEnable format at TCIServer.cs:1911-1915 [v2.10.3.13]:
//   "rx_apf_enable:" + rx + "," + enabled.ToLower() + ";"
QString TciProtocol::handleRxApfEnableCommand(const QStringList& args)
{
    if (args.size() < 1) { return {}; }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok || rx < 0 || rx > 1) { return {}; }

    if (args.size() == 1) {
        bool en = false;
        QMetaObject::invokeMethod(m_radio, "rxApf", Qt::DirectConnection,
                                  Q_RETURN_ARG(bool, en), Q_ARG(int, rx));
        return QStringLiteral("rx_apf_enable:%1,%2;")
            .arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
    }

    if (args.size() >= 2) {
        const bool en = args.at(1).trimmed().toLower() == QStringLiteral("true");
        QMetaObject::invokeMethod(m_radio, "setRxApf", Qt::DirectConnection,
                                  Q_ARG(int, rx), Q_ARG(bool, en));
        m_pendingNotifications << QStringLiteral("rx_apf_enable:%1,%2;")
            .arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
        return {};
    }

    return {};
}

// From Thetis TCIServer.cs:4959 [v2.10.3.13] — rx_nf_enable case in set switch.
// handleRxNfEnable at TCIServer.cs:3249-3264 [v2.10.3.13]:
//   1-arg = query → sendRxNfEnable(rx, GetMNF(rx+1)); 2-arg = set → TNFActive = en.
// sendRxNfEnable format at TCIServer.cs:1916-1920 [v2.10.3.13]:
//   "rx_nf_enable:" + rx + "," + enabled.ToLower() + ";"
QString TciProtocol::handleRxNfEnableCommand(const QStringList& args)
{
    if (args.size() < 1) { return {}; }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok || rx < 0 || rx > 1) { return {}; }

    if (args.size() == 1) {
        bool en = false;
        QMetaObject::invokeMethod(m_radio, "rxNf", Qt::DirectConnection,
                                  Q_RETURN_ARG(bool, en), Q_ARG(int, rx));
        return QStringLiteral("rx_nf_enable:%1,%2;")
            .arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
    }

    if (args.size() >= 2) {
        const bool en = args.at(1).trimmed().toLower() == QStringLiteral("true");
        QMetaObject::invokeMethod(m_radio, "setRxNf", Qt::DirectConnection,
                                  Q_ARG(int, rx), Q_ARG(bool, en));
        m_pendingNotifications << QStringLiteral("rx_nf_enable:%1,%2;")
            .arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
        return {};
    }

    return {};
}

// From Thetis TCIServer.cs:5103 [v2.10.3.13] — rx_anf_enable case in set switch.
// handleAnfEnable at TCIServer.cs:4521-4541 [v2.10.3.13]:
//   1-arg = query → sendAnfEnable(rx, GetANF(rx+1)); 2-arg = set → SetANF(rx+1, en).
// sendAnfEnable format at TCIServer.cs:4482-4487 [v2.10.3.13]:
//   "rx_anf_enable:" + rx + "," + enabled.ToLower() + ";"
QString TciProtocol::handleRxAnfEnableCommand(const QStringList& args)
{
    if (args.size() < 1) { return {}; }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok || rx < 0 || rx > 1) { return {}; }

    if (args.size() == 1) {
        bool en = false;
        QMetaObject::invokeMethod(m_radio, "rxAnf", Qt::DirectConnection,
                                  Q_RETURN_ARG(bool, en), Q_ARG(int, rx));
        return QStringLiteral("rx_anf_enable:%1,%2;")
            .arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
    }

    if (args.size() >= 2) {
        const bool en = args.at(1).trimmed().toLower() == QStringLiteral("true");
        QMetaObject::invokeMethod(m_radio, "setRxAnf", Qt::DirectConnection,
                                  Q_ARG(int, rx), Q_ARG(bool, en));
        m_pendingNotifications << QStringLiteral("rx_anf_enable:%1,%2;")
            .arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
        return {};
    }

    return {};
}

// From Thetis TCIServer.cs:5097 [v2.10.3.13] — rx_nr_enable case in set switch.
// handleNrEnable(args, false) at TCIServer.cs:4488-4519 [v2.10.3.13].
// Basic (non-extended) form:
//   1-arg = query → sendNrEnable(rx, en, false, nr) with nr from GetSelectedNR(rx+1).
//   2-arg = set (rx, bool) → SelectNR(rx+1, false, en?1:0).
//   nr range checked 1..4; set defaults nr=1 (TCIServer.cs:4494 [v2.10.3.13]).
// sendNrEnable non-extended format at TCIServer.cs:4472-4480 [v2.10.3.13]:
//   "rx_nr_enable:" + rx + "," + enabled.ToLower() + ";"
QString TciProtocol::handleRxNrEnableCommand(const QStringList& args)
{
    if (args.size() < 1) { return {}; }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok || rx < 0 || rx > 1) { return {}; }

    if (args.size() == 1) {
        // Query path: also emit the _ex form per Thetis (both forms sent on query).
        // TCIServer.cs:4501-4502 [v2.10.3.13]: sendNrEnable(rx, en, false, nr); sendNrEnable(rx, en, true, nr);
        // Phase 9 returns the basic form as the direct response; ex-form added to notifications.
        bool en = false;
        int nrIndex = 1;
        QMetaObject::invokeMethod(m_radio, "rxNr", Qt::DirectConnection,
                                  Q_RETURN_ARG(bool, en), Q_ARG(int, rx));
        QMetaObject::invokeMethod(m_radio, "rxNrIndex", Qt::DirectConnection,
                                  Q_RETURN_ARG(int, nrIndex), Q_ARG(int, rx));
        const QString enStr = en ? QStringLiteral("true") : QStringLiteral("false");
        // Also enqueue the extended form (Thetis emits both on query).
        m_pendingNotifications << QStringLiteral("rx_nr_enable_ex:%1,%2,%3;")
            .arg(rx).arg(enStr).arg(nrIndex);
        return QStringLiteral("rx_nr_enable:%1,%2;").arg(rx).arg(enStr);
    }

    if (args.size() >= 2) {
        const bool en = args.at(1).trimmed().toLower() == QStringLiteral("true");
        // Default nr=1 for basic form (TCIServer.cs:4494 [v2.10.3.13]).
        QMetaObject::invokeMethod(m_radio, "setRxNr", Qt::DirectConnection,
                                  Q_ARG(int, rx), Q_ARG(bool, en), Q_ARG(int, 1));
        m_pendingNotifications << QStringLiteral("rx_nr_enable:%1,%2;")
            .arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
        return {};
    }

    return {};
}

// From Thetis TCIServer.cs:5100 [v2.10.3.13] — rx_nr_enable_ex case in set switch.
// handleNrEnable(args, true) at TCIServer.cs:4488-4519 [v2.10.3.13].
// Extended form:
//   1-arg = query → same dual-emit as basic but returns extended form.
//   3-arg = set (rx, bool, nr_index) → SelectNR(rx+1, false, en?nr:0); nr in [1..4].
// sendNrEnable extended format at TCIServer.cs:4472-4480 [v2.10.3.13]:
//   "rx_nr_enable_ex:" + rx + "," + enabled.ToLower() + "," + nr + ";"
QString TciProtocol::handleRxNrEnableExCommand(const QStringList& args)
{
    if (args.size() < 1) { return {}; }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok || rx < 0 || rx > 1) { return {}; }

    if (args.size() == 1) {
        bool en = false;
        int nrIndex = 1;
        QMetaObject::invokeMethod(m_radio, "rxNr", Qt::DirectConnection,
                                  Q_RETURN_ARG(bool, en), Q_ARG(int, rx));
        QMetaObject::invokeMethod(m_radio, "rxNrIndex", Qt::DirectConnection,
                                  Q_RETURN_ARG(int, nrIndex), Q_ARG(int, rx));
        const QString enStr = en ? QStringLiteral("true") : QStringLiteral("false");
        // Also enqueue the basic form (Thetis emits both on query).
        m_pendingNotifications << QStringLiteral("rx_nr_enable:%1,%2;")
            .arg(rx).arg(enStr);
        return QStringLiteral("rx_nr_enable_ex:%1,%2,%3;").arg(rx).arg(enStr).arg(nrIndex);
    }

    if (args.size() >= 3) {
        const bool en = args.at(1).trimmed().toLower() == QStringLiteral("true");
        bool ok2 = false;
        int nrIndex = args.at(2).trimmed().toInt(&ok2);
        if (!ok2) { return {}; }
        // nr range check: 1..4 per TCIServer.cs:4509 [v2.10.3.13].
        if (nrIndex < 1 || nrIndex > 4) { return {}; }
        QMetaObject::invokeMethod(m_radio, "setRxNr", Qt::DirectConnection,
                                  Q_ARG(int, rx), Q_ARG(bool, en), Q_ARG(int, nrIndex));
        m_pendingNotifications << QStringLiteral("rx_nr_enable_ex:%1,%2,%3;")
            .arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(nrIndex);
        return {};
    }

    return {};
}

// From Thetis TCIServer.cs:5112 [v2.10.3.13] — agc_mode case in set switch.
// handleAgcMode at TCIServer.cs:4658-4671 [v2.10.3.13]:
//   1-arg = query → sendAgcMode(rx, GetAGCMode(rx+1)).
//   2-arg = set → SetAGCMode(rx+1, tciModeToAgcMode(args[1])).
// agcModeToTciMode / tciModeToAgcMode at TCIServer.cs:2192-2235 [v2.10.3.13]:
//   FIXD→"off", LONG→"long", SLOW→"slow", FAST→"fast", CUSTOM→"custom", MED→"normal".
//   Input aliases: "off"/"fixd"/"fixed"→"off"; "normal"/"med"/"medium"→"normal".
//   Unknown input → "normal" (AGCMode.MED default).
// sendAgcMode format at TCIServer.cs:2236-2240 [v2.10.3.13]:
//   "agc_mode:" + rx + "," + agcModeToTciMode(mode) + ";"
QString TciProtocol::handleAgcModeCommand(const QStringList& args)
{
    if (args.size() < 1) { return {}; }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok || rx < 0 || rx > 1) { return {}; }

    if (args.size() == 1) {
        QString mode;
        QMetaObject::invokeMethod(m_radio, "agcMode", Qt::DirectConnection,
                                  Q_RETURN_ARG(QString, mode), Q_ARG(int, rx));
        return QStringLiteral("agc_mode:%1,%2;").arg(rx).arg(mode);
    }

    if (args.size() >= 2) {
        // tciModeToAgcMode canonicalization per TCIServer.cs:2212-2234 [v2.10.3.13].
        const QString modeIn = args.at(1).trimmed().toLower();
        QString modeCanon;
        if      (modeIn == QStringLiteral("off")    ||
                 modeIn == QStringLiteral("fixd")   ||
                 modeIn == QStringLiteral("fixed"))     { modeCanon = QStringLiteral("off"); }
        else if (modeIn == QStringLiteral("long"))      { modeCanon = QStringLiteral("long"); }
        else if (modeIn == QStringLiteral("slow"))      { modeCanon = QStringLiteral("slow"); }
        else if (modeIn == QStringLiteral("fast"))      { modeCanon = QStringLiteral("fast"); }
        else if (modeIn == QStringLiteral("custom"))    { modeCanon = QStringLiteral("custom"); }
        else if (modeIn == QStringLiteral("normal") ||
                 modeIn == QStringLiteral("med")    ||
                 modeIn == QStringLiteral("medium"))    { modeCanon = QStringLiteral("normal"); }
        else {
            // Unknown input defaults to AGCMode.MED → "normal"
            // per TCIServer.cs:2232-2234 [v2.10.3.13] default: return AGCMode.MED.
            modeCanon = QStringLiteral("normal");
        }
        QMetaObject::invokeMethod(m_radio, "setAgcMode", Qt::DirectConnection,
                                  Q_ARG(int, rx), Q_ARG(QString, modeCanon));
        m_pendingNotifications << QStringLiteral("agc_mode:%1,%2;").arg(rx).arg(modeCanon);
        return {};
    }

    return {};
}

// From Thetis TCIServer.cs:5115 [v2.10.3.13] — agc_gain case in set switch.
// handleAgcGain at TCIServer.cs:4673-4689 [v2.10.3.13]:
//   1-arg = query → sendAgcGain(rx, GetAgcT(rx+1)).
//   2-arg = set → gain clamped to [-20, 120] → SetAgcT(rx+1, gain).
// sendAgcGain format at TCIServer.cs:2241-2245 [v2.10.3.13]:
//   "agc_gain:" + rx + "," + gain + ";"
QString TciProtocol::handleAgcGainCommand(const QStringList& args)
{
    if (args.size() < 1) { return {}; }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok || rx < 0 || rx > 1) { return {}; }

    if (args.size() == 1) {
        int gain = 0;
        QMetaObject::invokeMethod(m_radio, "agcGain", Qt::DirectConnection,
                                  Q_RETURN_ARG(int, gain), Q_ARG(int, rx));
        return QStringLiteral("agc_gain:%1,%2;").arg(rx).arg(gain);
    }

    if (args.size() >= 2) {
        bool ok2 = false;
        int gain = args.at(1).trimmed().toInt(&ok2);
        if (!ok2) { return {}; }
        // Clamp per TCIServer.cs:4686 [v2.10.3.13]: Math.Max(-20, Math.Min(120, gain)).
        gain = std::max(-20, std::min(120, gain));
        QMetaObject::invokeMethod(m_radio, "setAgcGain", Qt::DirectConnection,
                                  Q_ARG(int, rx), Q_ARG(int, gain));
        m_pendingNotifications << QStringLiteral("agc_gain:%1,%2;").arg(rx).arg(gain);
        return {};
    }

    return {};
}

// From Thetis TCIServer.cs:4968 [v2.10.3.13] — sql_enable case in set switch.
// handleSqlEnable at TCIServer.cs:3301-3316 [v2.10.3.13]:
//   1-arg = query → sendSqlEnable(rx, GetSqlMode(rx+1) != SquelchState.OFF).
//   2-arg = set → SetSqlMode(rx+1, en ? SQL : OFF).
// sendSqlEnable format at TCIServer.cs:1931-1935 [v2.10.3.13]:
//   "sql_enable:" + rx + "," + enabled.ToLower() + ";"
QString TciProtocol::handleSqlEnableCommand(const QStringList& args)
{
    if (args.size() < 1) { return {}; }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok || rx < 0 || rx > 1) { return {}; }

    if (args.size() == 1) {
        bool en = false;
        QMetaObject::invokeMethod(m_radio, "sqlEnable", Qt::DirectConnection,
                                  Q_RETURN_ARG(bool, en), Q_ARG(int, rx));
        return QStringLiteral("sql_enable:%1,%2;")
            .arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
    }

    if (args.size() >= 2) {
        const bool en = args.at(1).trimmed().toLower() == QStringLiteral("true");
        QMetaObject::invokeMethod(m_radio, "setSqlEnable", Qt::DirectConnection,
                                  Q_ARG(int, rx), Q_ARG(bool, en));
        m_pendingNotifications << QStringLiteral("sql_enable:%1,%2;")
            .arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
        return {};
    }

    return {};
}

// From Thetis TCIServer.cs:4971 [v2.10.3.13] — sql_level case in set switch.
// handleSqlLevel at TCIServer.cs:3317-3333 [v2.10.3.13]:
//   1-arg = query → sendSqlLevel(rx, GetSql(rx+1)).
//   2-arg = set → level clamped to [-140, 0] → SetSql(rx+1, level).
// sendSqlLevel format at TCIServer.cs:1936-1940 [v2.10.3.13]:
//   "sql_level:" + rx + "," + level + ";"
QString TciProtocol::handleSqlLevelCommand(const QStringList& args)
{
    if (args.size() < 1) { return {}; }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok || rx < 0 || rx > 1) { return {}; }

    if (args.size() == 1) {
        int level = 0;
        QMetaObject::invokeMethod(m_radio, "sqlLevel", Qt::DirectConnection,
                                  Q_RETURN_ARG(int, level), Q_ARG(int, rx));
        return QStringLiteral("sql_level:%1,%2;").arg(rx).arg(level);
    }

    if (args.size() >= 2) {
        bool ok2 = false;
        int level = args.at(1).trimmed().toInt(&ok2);
        if (!ok2) { return {}; }
        // Clamp per TCIServer.cs:3330 [v2.10.3.13]: Math.Max(-140, Math.Min(0, level)).
        level = std::max(-140, std::min(0, level));
        QMetaObject::invokeMethod(m_radio, "setSqlLevel", Qt::DirectConnection,
                                  Q_ARG(int, rx), Q_ARG(int, level));
        m_pendingNotifications << QStringLiteral("sql_level:%1,%2;").arg(rx).arg(level);
        return {};
    }

    return {};
}

// From Thetis TCIServer.cs:4938 [v2.10.3.13] — rit_enable case in set switch.
// handleRITEnableMessage at TCIServer.cs:3128-3143 [v2.10.3.13]:
//   1-arg = query → sendRITEnable(rx, RITOn).
//   2-arg = set → RITOn = en (gated rx==0||rx==1, effectively global).
// sendRITEnable format at TCIServer.cs:1881-1885 [v2.10.3.13]:
//   "rit_enable:" + rx + "," + enabled.ToLower() + ";"
// NOTE: RIT is GLOBAL in Thetis; rx arg is cosmetic per TCIServer.cs:3136 [v2.10.3.13].
QString TciProtocol::handleRitEnableCommand(const QStringList& args)
{
    if (args.size() < 1) { return {}; }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok) { return {}; }
    // Per TCIServer.cs:3136 [v2.10.3.13]: if (rx == 0 || rx == 1) — gate on valid rx.
    if (rx < 0 || rx > 1) { return {}; }

    if (args.size() == 1) {
        bool en = false;
        QMetaObject::invokeMethod(m_radio, "ritEnable", Qt::DirectConnection,
                                  Q_RETURN_ARG(bool, en));
        return QStringLiteral("rit_enable:%1,%2;")
            .arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
    }

    if (args.size() >= 2) {
        const bool en = args.at(1).trimmed().toLower() == QStringLiteral("true");
        QMetaObject::invokeMethod(m_radio, "setRitEnable", Qt::DirectConnection,
                                  Q_ARG(bool, en));
        m_pendingNotifications << QStringLiteral("rit_enable:%1,%2;")
            .arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
        return {};
    }

    return {};
}

// From Thetis TCIServer.cs:4944 [v2.10.3.13] — rit_offset case in set switch.
// handleRITOffsetMessage at TCIServer.cs:3160-3175 [v2.10.3.13]:
//   1-arg = query → sendRITOffset(rx, RITValue).
//   2-arg = set → RITValue = offset (global per TCIServer.cs:3169 [v2.10.3.13]).
// sendRITOffset format at TCIServer.cs:1891-1895 [v2.10.3.13]:
//   "rit_offset:" + rx + "," + offset + ";"
QString TciProtocol::handleRitOffsetCommand(const QStringList& args)
{
    if (args.size() < 1) { return {}; }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok || rx < 0 || rx > 1) { return {}; }

    if (args.size() == 1) {
        int offset = 0;
        QMetaObject::invokeMethod(m_radio, "ritOffset", Qt::DirectConnection,
                                  Q_RETURN_ARG(int, offset));
        return QStringLiteral("rit_offset:%1,%2;").arg(rx).arg(offset);
    }

    if (args.size() >= 2) {
        bool ok2 = false;
        const int offset = args.at(1).trimmed().toInt(&ok2);
        if (!ok2) { return {}; }
        QMetaObject::invokeMethod(m_radio, "setRitOffset", Qt::DirectConnection,
                                  Q_ARG(int, offset));
        m_pendingNotifications << QStringLiteral("rit_offset:%1,%2;").arg(rx).arg(offset);
        return {};
    }

    return {};
}

// From Thetis TCIServer.cs:4941 [v2.10.3.13] — xit_enable case in set switch.
// handleXITEnableMessage at TCIServer.cs:3144-3159 [v2.10.3.13]:
//   1-arg = query → sendXITEnable(rx, XITOn).
//   2-arg = set → XITOn = en (global, same rx-gating as RIT).
// sendXITEnable format at TCIServer.cs:1886-1890 [v2.10.3.13]:
//   "xit_enable:" + rx + "," + enabled.ToLower() + ";"
QString TciProtocol::handleXitEnableCommand(const QStringList& args)
{
    if (args.size() < 1) { return {}; }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok || rx < 0 || rx > 1) { return {}; }

    if (args.size() == 1) {
        bool en = false;
        QMetaObject::invokeMethod(m_radio, "xitEnable", Qt::DirectConnection,
                                  Q_RETURN_ARG(bool, en));
        return QStringLiteral("xit_enable:%1,%2;")
            .arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
    }

    if (args.size() >= 2) {
        const bool en = args.at(1).trimmed().toLower() == QStringLiteral("true");
        QMetaObject::invokeMethod(m_radio, "setXitEnable", Qt::DirectConnection,
                                  Q_ARG(bool, en));
        m_pendingNotifications << QStringLiteral("xit_enable:%1,%2;")
            .arg(rx).arg(en ? QStringLiteral("true") : QStringLiteral("false"));
        return {};
    }

    return {};
}

// From Thetis TCIServer.cs:4947 [v2.10.3.13] — xit_offset case in set switch.
// handleXITOffsetMessage at TCIServer.cs:3176-3191 [v2.10.3.13]:
//   1-arg = query → sendXITOffset(rx, XITValue).
//   2-arg = set → XITValue = offset (global per TCIServer.cs:3185 [v2.10.3.13]).
// sendXITOffset format at TCIServer.cs:1896-1900 [v2.10.3.13]:
//   "xit_offset:" + rx + "," + offset + ";"
QString TciProtocol::handleXitOffsetCommand(const QStringList& args)
{
    if (args.size() < 1) { return {}; }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok || rx < 0 || rx > 1) { return {}; }

    if (args.size() == 1) {
        int offset = 0;
        QMetaObject::invokeMethod(m_radio, "xitOffset", Qt::DirectConnection,
                                  Q_RETURN_ARG(int, offset));
        return QStringLiteral("xit_offset:%1,%2;").arg(rx).arg(offset);
    }

    if (args.size() >= 2) {
        bool ok2 = false;
        const int offset = args.at(1).trimmed().toInt(&ok2);
        if (!ok2) { return {}; }
        QMetaObject::invokeMethod(m_radio, "setXitOffset", Qt::DirectConnection,
                                  Q_ARG(int, offset));
        m_pendingNotifications << QStringLiteral("xit_offset:%1,%2;").arg(rx).arg(offset);
        return {};
    }

    return {};
}

// From Thetis TCIServer.cs:5109 [v2.10.3.13] — rx_balance case in set switch.
// handleRxBalance at TCIServer.cs:4631-4656 [v2.10.3.13]:
//   2-arg = query (rx, chan) → sendRxBalance(rx, chan, 40.0-(pan*0.8)).
//   3-arg = set (rx, chan, balance) → balance clamped [-40,40]; pan = round((40-bal)/0.8).
// sendRxBalance format at TCIServer.cs:2187-2191 [v2.10.3.13]:
//   "rx_balance:" + rx + "," + chan + "," + balance.ToString("F2", CultureInfo.InvariantCulture) + ";"
// NOTE: Thetis pan-slider calibration (40-x*0.8 transform at TCIServer.cs:4644-4652
//       [v2.10.3.13]) NOT replicated in Phase 9 stub — NereusSDR stores F2 dB directly.
//       Clamp to [-40.0, 40.0] per TCIServer.cs:4650 [v2.10.3.13] IS replicated.
//       Full Thetis pan math deferred to Phase 20 (SetupForm / mixer integration).
QString TciProtocol::handleRxBalanceCommand(const QStringList& args)
{
    if (args.size() < 2) { return {}; }
    bool ok1 = false, ok2 = false;
    const int rx   = args.at(0).trimmed().toInt(&ok1);
    const int chan = args.at(1).trimmed().toInt(&ok2);
    if (!ok1 || !ok2 || rx < 0 || rx > 1 || chan < 0 || chan > 1) { return {}; }

    if (args.size() == 2) {
        // Query path.
        double bal = 0.0;
        QMetaObject::invokeMethod(m_radio, "rxBalance", Qt::DirectConnection,
                                  Q_RETURN_ARG(double, bal),
                                  Q_ARG(int, rx), Q_ARG(int, chan));
        return QStringLiteral("rx_balance:%1,%2,%3;")
            .arg(rx).arg(chan)
            .arg(QString::number(bal, 'f', 2));
    }

    if (args.size() >= 3) {
        // Set path. Parse as double; clamp to [-40.0, 40.0] per TCIServer.cs:4650.
        bool ok3 = false;
        double bal = args.at(2).trimmed().toDouble(&ok3);
        if (!ok3) { return {}; }
        bal = std::max(-40.0, std::min(40.0, bal));
        QMetaObject::invokeMethod(m_radio, "setRxBalance", Qt::DirectConnection,
                                  Q_ARG(int, rx), Q_ARG(int, chan), Q_ARG(double, bal));
        m_pendingNotifications << QStringLiteral("rx_balance:%1,%2,%3;")
            .arg(rx).arg(chan)
            .arg(QString::number(bal, 'f', 2));
        return {};
    }

    return {};
}

// ── Audio stream family handlers (Phase 10) ────────────────────────────────

// From Thetis TCIServer.cs:5740-5795 [v2.10.3.13] — handleAudioSampleRate.
// 1-arg set. Accepts 8000/12000/24000/48000; ignores other values (echoes
// current rate either way). Phase 10 simplified: default-samples adjustment
// (m_audioStreamSamplesExplicitlySet path) deferred to Phase 16.
// Notification format: audio_samplerate:<int>; (sendAudioSampleRate).
QString TciProtocol::handleAudioSampleRateCommand(const QStringList& args)
{
    if (args.size() != 1) { return {}; }
    bool ok = false;
    const int sr = args.at(0).trimmed().toInt(&ok);
    if (!ok) { return {}; }

    // From Thetis TCIServer.cs:5750 [v2.10.3.13] — valid-rate gate.
    const bool valid = (sr == 8000 || sr == 12000 || sr == 24000 || sr == 48000);
    if (valid) {
        QMetaObject::invokeMethod(m_radio, "setAudioSampleRate",
                                  Qt::DirectConnection,
                                  Q_ARG(int, sr));
    }

    int current = 48000;
    QMetaObject::invokeMethod(m_radio, "audioSampleRate",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(int, current));
    // From Thetis TCIServer.cs:5792 [v2.10.3.13] — sendAudioSampleRate(audioSampleRate).
    m_pendingNotifications << buildAudioSampleRateLine(current);
    return {};
}

// From Thetis TCIServer.cs:5178-5179 [v2.10.3.13] — audio_samplerate query case.
// sendAudioSampleRate(m_audioSampleRate).
QString TciProtocol::handleAudioSampleRateQueryCommand()
{
    int sr = 48000;
    QMetaObject::invokeMethod(m_radio, "audioSampleRate",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(int, sr));
    return buildAudioSampleRateLine(sr);
}

// From Thetis TCIServer.cs:5908-5933 [v2.10.3.13] — handleAudioStreamSampleType.
// 1-arg set. Valid values: "int16", "int24", "int32", "float32".
// Unknown value defaults to "float32" (Thetis: case "float32": default:).
// Notification format: audio_stream_sample_type:<string>;
QString TciProtocol::handleAudioStreamSampleTypeCommand(const QStringList& args)
{
    if (args.size() != 1) { return {}; }
    const QString raw = args.at(0).trimmed().toLower();

    QString canonical;
    if (raw == QStringLiteral("int16")) {
        canonical = QStringLiteral("int16");
    } else if (raw == QStringLiteral("int24")) {
        canonical = QStringLiteral("int24");
    } else if (raw == QStringLiteral("int32")) {
        canonical = QStringLiteral("int32");
    } else {
        // From Thetis TCIServer.cs:5924-5927 [v2.10.3.13] — float32 default.
        canonical = QStringLiteral("float32");
    }
    QMetaObject::invokeMethod(m_radio, "setAudioStreamSampleType",
                              Qt::DirectConnection,
                              Q_ARG(QString, canonical));
    m_pendingNotifications << buildAudioStreamSampleTypeLine(canonical);
    return {};
}

// From Thetis TCIServer.cs:5935-5949 [v2.10.3.13] — handleAudioStreamChannels.
// 1-arg set. Accepts 1 (mono) or 2 (stereo); ignores other values but echoes current.
// Notification format: audio_stream_channels:<int>;
QString TciProtocol::handleAudioStreamChannelsCommand(const QStringList& args)
{
    if (args.size() != 1) { return {}; }
    bool ok = false;
    const int n = args.at(0).trimmed().toInt(&ok);
    if (!ok) { return {}; }

    // From Thetis TCIServer.cs:5941-5943 [v2.10.3.13] — channels == 1 || channels == 2 gate.
    if (n == 1 || n == 2) {
        QMetaObject::invokeMethod(m_radio, "setAudioStreamChannels",
                                  Qt::DirectConnection,
                                  Q_ARG(int, n));
    }
    int current = 2;
    QMetaObject::invokeMethod(m_radio, "audioStreamChannels",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(int, current));
    m_pendingNotifications << buildAudioStreamChannelsLine(current);
    return {};
}

// From Thetis TCIServer.cs:5951-5983 [v2.10.3.13] — handleAudioStreamSamples.
// 1-arg set. Range [100..2048]. Values outside range: ignored, current echoed.
// Notification format: audio_stream_samples:<int>;
QString TciProtocol::handleAudioStreamSamplesCommand(const QStringList& args)
{
    if (args.size() != 1) { return {}; }
    bool ok = false;
    const int n = args.at(0).trimmed().toInt(&ok);
    if (!ok) { return {}; }

    // From Thetis TCIServer.cs:5957 [v2.10.3.13] — samples >= 100 && samples <= 2048 gate.
    if (n >= 100 && n <= 2048) {
        QMetaObject::invokeMethod(m_radio, "setAudioStreamSamples",
                                  Qt::DirectConnection,
                                  Q_ARG(int, n));
    }
    int current = 2048;
    QMetaObject::invokeMethod(m_radio, "audioStreamSamples",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(int, current));
    m_pendingNotifications << buildAudioStreamSamplesLine(current);
    return {};
}

// From Thetis TCIServer.cs:5891-5906 [v2.10.3.13] — handleAudioStart.
// STUBBED in Phase 10: per-client audio stream subscription tracking
// (m_audioStreamEnabled Set<int>) deferred to Phase 16. Returns empty;
// enqueues no notification. The stub accepts the required 1-arg form and
// validates the receiver int silently — unknown commands still produce nothing.
QString TciProtocol::handleAudioStartCommand(const QStringList& args, bool enable)
{
    // Phase 16 will wire per-client m_audioStreamEnabled.Add/Remove(receiver)
    // and call m_server->RefreshStreamRunState(). For now, accept valid args
    // and return empty so the silent-error invariant still holds for bad inputs.
    if (args.size() != 1) { return {}; }
    bool ok = false;
    (void)args.at(0).trimmed().toInt(&ok);
    // Intentionally no notification — subscription model deferred to Phase 16.
    (void)enable;
    return {};
}

// From Thetis TCIServer.cs:4150-4161 [v2.10.3.13] — handleVolume (set path).
// 1-arg set (double dB). Converts via tciDbToLinearVolume and stores AF.
// sendVolume guards: -60 <= db <= 0 before emitting.
// Notification format: volume:<F1 dB>; (sendVolume at TCIServer.cs:2173-2179).
QString TciProtocol::handleVolumeCommand(const QStringList& args)
{
    if (args.size() != 1) { return {}; }
    bool ok = false;
    // From Thetis TCIServer.cs:4154 [v2.10.3.13] — double.TryParse.
    const double db = args.at(0).trimmed().toDouble(&ok);
    if (!ok) { return {}; }
    // From Thetis TCIServer.cs:4155 [v2.10.3.13] — AF = dbToLinearVolume(dBLevel).
    const int linear = tciDbToLinearVolume(db);
    QMetaObject::invokeMethod(m_radio, "setAfLinear",
                              Qt::DirectConnection,
                              Q_ARG(int, linear));
    // From Thetis TCIServer.cs:2173-2179 [v2.10.3.13] — sendVolume guards on -60..0.
    // Use linearToDbVolume round-trip to produce the canonical value.
    m_pendingNotifications << buildVolumeLine(tciLinearToDbVolume(linear));
    return {};
}

// From Thetis TCIServer.cs:5148-5149 [v2.10.3.13] — volume query case.
// sendVolume(linearToDbVolume(AF)) → direct unicast response.
QString TciProtocol::handleVolumeQueryCommand()
{
    int linear = 50;
    QMetaObject::invokeMethod(m_radio, "afLinear",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(int, linear));
    return buildVolumeLine(tciLinearToDbVolume(linear));
}

// From Thetis TCIServer.cs:4133-4148 [v2.10.3.13] — handleMONVolume (set path).
// 1-arg set (double dB). Converts via tciDbToLinearVolume and stores TXAF.
// sendMONVolume guards: -60 <= db <= 0 before emitting.
// Notification format: mon_volume:<F1 dB>; (sendMONVolume at TCIServer.cs:2180-2186).
QString TciProtocol::handleMonVolumeCommand(const QStringList& args)
{
    if (args.size() != 1) { return {}; }
    bool ok = false;
    // From Thetis TCIServer.cs:4138 [v2.10.3.13] — double.TryParse.
    const double db = args.at(0).trimmed().toDouble(&ok);
    if (!ok) { return {}; }
    // From Thetis TCIServer.cs:4141 [v2.10.3.13] — TXAF = dbToLinearVolume(dBLevel).
    const int linear = tciDbToLinearVolume(db);
    QMetaObject::invokeMethod(m_radio, "setMonLinear",
                              Qt::DirectConnection,
                              Q_ARG(int, linear));
    // From Thetis TCIServer.cs:2180-2186 [v2.10.3.13] — sendMONVolume guards on -60..0.
    m_pendingNotifications << buildMonVolumeLine(tciLinearToDbVolume(linear));
    return {};
}

// From Thetis TCIServer.cs:5154-5155 [v2.10.3.13] — mon_volume query case.
// sendMONVolume(linearToDbVolume(TXAF)) → direct unicast response.
QString TciProtocol::handleMonVolumeQueryCommand()
{
    int linear = 50;
    QMetaObject::invokeMethod(m_radio, "monLinear",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(int, linear));
    return buildMonVolumeLine(tciLinearToDbVolume(linear));
}

// ── IQ stream family handlers (Phase 11) ──────────────────────────────────

// From Thetis TCIServer.cs:5705-5722 [v2.10.3.13] — handleIQSampleRate.
// 1-arg set (int). Thetis does NOT clamp to specific rates — it accepts any
// int, stores it, and echoes it back unchanged. The hardware-rate coupling
// that would apply 48000/96000/192000/384000 constraints is commented out at
// TCIServer.cs:5712-5719 [v2.10.3.13]:
//     //just echo out that we have changed to keep client happy, we dont change
//     //Thetis H/W sample rate for now
// Phase 11 faithful: parse int, store if valid, always echo current value.
// Notification format: iq_samplerate:<int>; (sendIQSampleRate at TCIServer.cs:5364).
QString TciProtocol::handleIqSampleRateCommand(const QStringList& args)
{
    if (args.size() != 1) { return {}; }
    bool ok = false;
    // From Thetis TCIServer.cs:5708 [v2.10.3.13] — int.TryParse(args[0], out int sampleRate).
    const int sr = args.at(0).trimmed().toInt(&ok);
    if (ok) {
        // From Thetis TCIServer.cs:5710 [v2.10.3.13] — no rate gate; any int stored.
        QMetaObject::invokeMethod(m_radio, "setIqSampleRate",
                                  Qt::DirectConnection,
                                  Q_ARG(int, sr));
    }
    int current = 192000;
    QMetaObject::invokeMethod(m_radio, "iqSampleRate",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(int, current));
    // From Thetis TCIServer.cs:5720 [v2.10.3.13] — sendIQSampleRate(sampleRate).
    m_pendingNotifications << buildIqSampleRateLine(current);
    return {};
}

// From Thetis TCIServer.cs:5175-5176 [v2.10.3.13] — iq_samplerate query case.
// sendIQSampleRate(getPublishedIQSampleRate()) → direct unicast response.
// getPublishedIQSampleRate at TCIServer.cs:5925-5938 [v2.10.3.13] returns the
// max hardware sample rate across active receivers; Phase 11 returns the stored
// value directly (hardware query path deferred to Phase 18).
QString TciProtocol::handleIqSampleRateQueryCommand()
{
    int sr = 192000;
    QMetaObject::invokeMethod(m_radio, "iqSampleRate",
                              Qt::DirectConnection,
                              Q_RETURN_ARG(int, sr));
    return buildIqSampleRateLine(sr);
}

// From Thetis TCIServer.cs:5022-5026 [v2.10.3.13] — iq_start/iq_stop cases
// dispatch through handleIQStart at TCIServer.cs:5797-5813 [v2.10.3.13].
//
// Phase 11 STUB: validate args parses an int receiver and return empty.
// DEFERRED to Phase 18 (IQ binary stream pipeline): per-client subscription
// tracking via m_iqStreamEnabled HashSet (Thetis TCIServer.cs:766).
// AlwaysStreamIQ flag effect (Thetis TCIServer.cs:5401) lands at the same
// time. Phase 11's placeholder rows for compat_always_stream_iq_on are
// updated in this commit's matrix.csv to reflect Phase 18 deferral.
//
// When Phase 18 wires this:
//   lock (m_objStreamLock) {
//     if (enable) m_iqStreamEnabled.Add(receiver);
//     else         m_iqStreamEnabled.Remove(receiver);
//   }
//   sendIQStartStop(receiver, enable);
//   m_server?.RefreshStreamRunState();
QString TciProtocol::handleIqStartStopCommand(const QStringList& args, bool enable)
{
    // Validate args contains a parseable int receiver; silently ignore bad inputs.
    if (args.size() != 1) { return {}; }
    bool ok = false;
    (void)args.at(0).trimmed().toInt(&ok);
    // Intentionally no notification — per-client subscription model deferred to Phase 18.
    (void)enable;
    return {};
}

// ── Phase 13: Bespoke _ex command handlers ─────────────────────────────────

// Porting from Thetis TCIServer.cs:4413-4450 [v2.10.3.13] — handleRXEnable.
// Original C# logic:
//   1-arg path: if rx==0 → sendRXEnable(rx, !MOX); rx==1 → sendRXEnable(rx, RX2Enabled && !MOX).
//   2-arg path: if rx==0 → always on (no-op); if rx==1 → RX2Enabled = enable.
// NereusSDR simplification: MOX-gating on query deferred to Phase 17;
//   stored enable state returned directly. rx0 always stays true on set.
// sendRXEnable at TCIServer.cs:2279-2283 [v2.10.3.13]: "rx_enable:rx,bool;"
QString TciProtocol::handleRxEnableCommand(const QStringList& args)
{
    if (args.size() < 1) { return {}; }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok || rx < 0 || rx > 1) { return {}; }

    if (args.size() == 1) {
        // Query path.
        // From Thetis TCIServer.cs:4438-4445 [v2.10.3.13] — sendRXEnable per rx.
        bool en = true;
        QMetaObject::invokeMethod(m_radio, "rxEnable", Qt::DirectConnection,
                                  Q_RETURN_ARG(bool, en), Q_ARG(int, rx));
        return buildRxEnableLine(rx, en);
    }

    if (args.size() >= 2) {
        // Set path.
        // From Thetis TCIServer.cs:4422-4433 [v2.10.3.13] — set RX2Enabled (rx==1 only).
        // rx==0 is always enabled in Thetis; Phase 13 stores it but never forces off.
        const bool en = args.at(1).trimmed().toLower() == QStringLiteral("true");
        QMetaObject::invokeMethod(m_radio, "setRxEnable", Qt::DirectConnection,
                                  Q_ARG(int, rx), Q_ARG(bool, en));
        m_pendingNotifications << buildRxEnableLine(rx, en);
        return {};
    }

    return {};
}

// Porting from Thetis TCIServer.cs:4696-4710 [v2.10.3.13] — handleCTUN.
// Original C# logic:
//   args.Length < 1 || > 2 → return.
//   args.Length == 1 → get: enable = GetCTUN(rx+1); sendCTUN(rx, enable).
//   args.Length == 2 → set: SetCTUN(rx+1, enable).
// sendCTUN at TCIServer.cs:4690-4694 [v2.10.3.13]: "rx_ctun_ex:rx,bool;"
QString TciProtocol::handleRxCtunExCommand(const QStringList& args)
{
    if (args.size() < 1 || args.size() > 2) { return {}; }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok || rx < 0 || rx > 1) { return {}; }

    if (args.size() == 1) {
        // Query path.
        bool en = false;
        QMetaObject::invokeMethod(m_radio, "rxCtun", Qt::DirectConnection,
                                  Q_RETURN_ARG(bool, en), Q_ARG(int, rx));
        return buildRxCtunExLine(rx, en);
    }

    // Set path.
    const bool en = args.at(1).trimmed().toLower() == QStringLiteral("true");
    QMetaObject::invokeMethod(m_radio, "setRxCtun", Qt::DirectConnection,
                              Q_ARG(int, rx), Q_ARG(bool, en));
    m_pendingNotifications << buildRxCtunExLine(rx, en);
    return {};
}

// Porting from Thetis TCIServer.cs:4732-4748 [v2.10.3.13] — handleTXProfile (set path).
// Original C# logic: args.Length > 1 → return; args.Length == 1 → SafeTXProfileSet(args[0]).
// Set path in switch at TCIServer.cs:5121 [v2.10.3.13] — args has 1 element (the name).
// sendTXProfile at TCIServer.cs:4715-4720 [v2.10.3.13]: "tx_profile_ex:name;"
QString TciProtocol::handleTxProfileExSetCommand(const QStringList& args)
{
    if (args.size() != 1) { return {}; }
    const QString name = args.at(0).trimmed();
    if (name.isEmpty()) { return {}; }
    QMetaObject::invokeMethod(m_radio, "setTxProfile", Qt::DirectConnection,
                              Q_ARG(QString, name));
    m_pendingNotifications << buildTxProfileExLine(name);
    return {};
}

// Porting from Thetis TCIServer.cs:4732-4748 [v2.10.3.13] — handleTXProfile (query path).
// Original C# logic: args.Length == 0 → get TXProfile; sendTXProfile(prof).
// Query path in switch at TCIServer.cs:5193 [v2.10.3.13]: tmpArgs=string[0] (empty array).
// sendTXProfile at TCIServer.cs:4715-4720 [v2.10.3.13]: "tx_profile_ex:name;"
QString TciProtocol::handleTxProfileExQueryCommand()
{
    QString prof;
    QMetaObject::invokeMethod(m_radio, "txProfile", Qt::DirectConnection,
                              Q_RETURN_ARG(QString, prof));
    return buildTxProfileExLine(prof);
}

// Porting from Thetis TCIServer.cs:4748-4752 [v2.10.3.13] — handleTXProfiles.
// Original C# logic: calls sendTXProfiles().
// sendTXProfiles at TCIServer.cs:4721-4731 [v2.10.3.13]: returns early if IsSetupFormNull;
//   else builds CSV of profile strings and emits "tx_profiles_ex:name1,name2,...;"
// NereusSDR: queries mock for profile list (stub returns {"Default"}).
// Query path in switch at TCIServer.cs:5187 [v2.10.3.13].
QString TciProtocol::handleTxProfilesExQueryCommand()
{
    QStringList profiles;
    QMetaObject::invokeMethod(m_radio, "txProfilesList", Qt::DirectConnection,
                              Q_RETURN_ARG(QStringList, profiles));
    if (profiles.isEmpty()) {
        profiles << QStringLiteral("Default");
    }
    return buildTxProfilesExLine(profiles);
}

// Porting from Thetis TCIServer.cs:4776-4782 [v2.10.3.13] — handleCalibration.
// Original C# logic:
//   args.Length != 1 → return (ignores 0-arg and 2+-arg inputs).
//   args.Length == 1 → parse rx, call CalibrationChanged(rx).
// CalibrationChanged at TCIServer.cs:1152-1170 [v2.10.3.13]:
//   queries 5 floats from radio (meter_offset, display_offset, xvtr_gain_offset,
//   offset_6m, tx_display_offset) and emits sendCalibration.
// sendCalibration at TCIServer.cs:4766-4775 [v2.10.3.13]:
//   "calibration_ex:rx,meter,display,xvtr,six_meter,tx_display;" (F6 C-locale each).
// Note: this is NOT a 6-arg client write; client sends rx only and the server
//   pushes back the radio's calibration values. The plan doc was incorrect.
// Set switch case at TCIServer.cs:5124 [v2.10.3.13].
QString TciProtocol::handleCalibrationExCommand(const QStringList& args)
{
    if (args.size() != 1) { return {}; }
    bool ok = false;
    const int rx = args.at(0).trimmed().toInt(&ok);
    if (!ok || rx < 0 || rx > 1) { return {}; }

    // From Thetis TCIServer.cs:1152-1170 [v2.10.3.13] — CalibrationChanged queries each value.
    double meter = 0.0;
    double display = 0.0;
    double xvtr = 0.0;
    double sixMeter = 0.0;
    double txDisplay = 0.0;
    QMetaObject::invokeMethod(m_radio, "calibrationMeter",     Qt::DirectConnection,
                              Q_RETURN_ARG(double, meter),    Q_ARG(int, rx));
    QMetaObject::invokeMethod(m_radio, "calibrationDisplay",   Qt::DirectConnection,
                              Q_RETURN_ARG(double, display),  Q_ARG(int, rx));
    QMetaObject::invokeMethod(m_radio, "calibrationXvtr",      Qt::DirectConnection,
                              Q_RETURN_ARG(double, xvtr),     Q_ARG(int, rx));
    QMetaObject::invokeMethod(m_radio, "calibrationSixMeter",  Qt::DirectConnection,
                              Q_RETURN_ARG(double, sixMeter), Q_ARG(int, rx));
    QMetaObject::invokeMethod(m_radio, "calibrationTxDisplay", Qt::DirectConnection,
                              Q_RETURN_ARG(double, txDisplay),Q_ARG(int, rx));
    m_pendingNotifications << buildCalibrationExLine(rx, meter, display, xvtr, sixMeter, txDisplay);
    return {};
}

// From Thetis TCIServer.cs:5190 [v2.10.3.13] — shutdown_ex case in 1-arg query switch.
// handleShutdown at TCIServer.cs:4752-4765 [v2.10.3.13]: BeginInvoke → console.Close().
// STUB: logs warning + returns empty. Actual shutdown wiring is a maintainer-policy
//   decision; Phase 24+ may implement it. TCI clients must not be able to close NereusSDR.
QString TciProtocol::handleShutdownExCommand()
{
    qCWarning(lcTci) << "TCI client requested shutdown_ex; ignored — Phase 13 stub (maintainer-policy decision)";
    return {};
}

// ── Phase 12: Spot + CW command stubs ──────────────────────────────────────
// All stubs log at lcTci info level and return empty. Real handlers land
// post-Phase 3J-1: spot_* in Phase 3J-2 (Spots epic); cw_macros_speed_up/down +
// cw_msg in Phase 3M-2 (CW TX epic).

// From Thetis TCIServer.cs:5049 [v2.10.3.13] — spot case.
// Phase 12 STUB: log + return empty. Real handler lands in Phase 3J-2.
QString TciProtocol::handleSpotCommand(const QStringList& args)
{
    qCInfo(lcTci) << "TCI stub: spot received with" << args.size() << "args (Phase 12 stub)";
    return {};
}

// From Thetis TCIServer.cs:5052 [v2.10.3.13] — spot_delete case.
// Phase 12 STUB: log + return empty. Real handler lands in Phase 3J-2.
QString TciProtocol::handleSpotDeleteCommand(const QStringList& args)
{
    qCInfo(lcTci) << "TCI stub: spot_delete received with" << args.size() << "args (Phase 12 stub)";
    return {};
}

// From Thetis TCIServer.cs:5082 [v2.10.3.13] — spot_simulate_click case.
// Phase 12 STUB: log + return empty. Real handler lands in Phase 3J-2.
QString TciProtocol::handleSpotSimulateClickCommand(const QStringList& args)
{
    qCInfo(lcTci) << "TCI stub: spot_simulate_click received with" << args.size() << "args (Phase 12 stub)";
    return {};
}

// From Thetis TCIServer.cs:5184 [v2.10.3.13] — spot_clear case in 1-arg query switch.
// Phase 12 STUB: log + return empty. Real handler lands in Phase 3J-2.
QString TciProtocol::handleSpotClearCommand()
{
    qCInfo(lcTci) << "TCI stub: spot_clear received (Phase 12 stub)";
    return {};
}

// From Thetis TCIServer.cs:4989 [v2.10.3.13] — cw_macros_speed_up case.
// Phase 12 STUB: log + return empty. Real handler lands in Phase 3M-2.
QString TciProtocol::handleCwMacrosSpeedUpCommand(const QStringList& args)
{
    qCInfo(lcTci) << "TCI stub: cw_macros_speed_up received with" << args.size() << "args (Phase 12 stub)";
    return {};
}

// From Thetis TCIServer.cs:4992 [v2.10.3.13] — cw_macros_speed_down case.
// Phase 12 STUB: log + return empty. Real handler lands in Phase 3M-2.
QString TciProtocol::handleCwMacrosSpeedDownCommand(const QStringList& args)
{
    qCInfo(lcTci) << "TCI stub: cw_macros_speed_down received with" << args.size() << "args (Phase 12 stub)";
    return {};
}

// From Thetis TCIServer.cs:5001 [v2.10.3.13] — cw_msg case.
// Phase 12 STUB: log + return empty. Real handler lands in Phase 3M-2.
QString TciProtocol::handleCwMsgCommand(const QStringList& args)
{
    qCInfo(lcTci) << "TCI stub: cw_msg received with" << args.size() << "args (Phase 12 stub)";
    return {};
}

} // namespace NereusSDR
