// no-port-check: NereusSDR-original regression test for the Phase 3J-1
// closeout that replaced the hardcoded init-burst placeholders with live
// RadioModel reads. Bench bug: an RF-Kit RF2K-S TCI client showed
// 14.250 MHz while the radio was actually on 40m, because
// buildInitialRadioStateLines() emitted `vfo:0,0,14250000;` regardless of
// the radio's tuned frequency.
//
// This suite seeds TestMockRadioModel via the Q_INVOKABLE setters that
// production-side TciProtocol invokes during a real client connect, then
// asserts the init burst reflects the seeded values. Add slots per field
// family as wiring lands.
//
// Companion to:
//   - tst_tci_init_burst_golden.cpp (byte-for-byte baseline parity)
//   - tst_tci_init_burst_smoke.cpp  (wrapper sanity)
//   - tst_tci_init_burst_typo_divergence.cpp (design doc §7 row 1 typo fix)

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocket>
#include "core/TciProtocol.h"
#include "core/TciServer.h"
#include "core/AppSettings.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "TestMockRadioModel.h"

using namespace NereusSDR;

class TestTciInitBurstLiveState : public QObject {
    Q_OBJECT
private:
    // Mirror the golden test's AppSettings pin so the unrelated wrapper
    // fields stay deterministic across this and the golden suite.
    static void pinAppSettingsToCaptureConditions();

private slots:
    // RED #1 -- vfoHz must flow into dds/vfo lines (bench bug repro).
    void vfo_hz_drives_dds_and_vfo_lines();
    // RED #2 -- mode(rx) must flow into modulation lines.
    void mode_drives_modulation_lines();
    // RED #3 -- filterLow/filterHigh per RX must flow into rx_filter_band.
    void filter_drives_rx_filter_band_lines();
    // RED #4 -- agcMode + agcGain per RX must flow into agc_mode + agc_gain.
    void agc_drives_agc_mode_and_gain_lines();
    // RED #5 -- mox + rit/xit + lock + globalMute drive their lines.
    void global_toggles_drive_their_lines();
    // RED #6 -- per-rx flags: sql, rxMute, nr, anf, apf, bin, nf, ctun, split.
    void per_rx_flags_drive_their_lines();
    // RED #7 -- balance + rxBalance per RX per channel.
    void balance_drives_rx_balance_lines();
    // RED #8 -- audio + iq stream config flow into their lines.
    void audio_iq_stream_config_drives_lines();
    // RED #9 -- afLinear + monLinear convert to volume/mon_volume dB lines.
    void volume_drives_db_lines();
    // RED #10 -- tx profile + tx profiles list.
    void tx_profile_drives_lines();
    // RED #11 -- per-RX calibration values F6 format.
    void calibration_drives_lines();
    // RED #12 -- rx_volume lines use audioGainToDb (LOG curve) not the
    // linear tciLinearToDbVolume used for the global volume line. Extended
    // by Phase 3F Sub-Epic J Task 10 to also prove rx_volume is per-slice
    // (afGain), decoupled from the radio-global afLinear it used to share.
    void rx_volume_uses_audio_gain_to_db_log_curve();
    // RED #13 -- tune flag flows into trx-side tune lines.
    void tune_drives_tune_lines();
    // RED #14 -- monEnabled flows into mon_enable line.
    void mon_enabled_drives_mon_enable_line();
    // RED #15 -- powerOn flows into start/stop line.
    void power_on_drives_start_stop_line();
    // RED #16 -- rx2Enabled flips bRX2Enabled gates throughout the burst.
    void rx2_enabled_flips_rx2_gated_lines();
    // RED #17 -- diglOffset / diguOffset flow into the offset lines.
    void digl_digu_offset_drives_offset_lines();
    // RED #18 -- CW macros default to Thetis null-controller defaults 30/0/30.
    void cw_macros_match_thetis_null_controller_defaults();

    // RED #19 -- distinct mock state must produce distinct init-burst output
    // (catches any future re-introduction of hardcoded placeholders).
    void distinct_state_yields_distinct_burst();

    // RED #20 (PR #279 review P3, 2026-05-22) -- enqueueLocalBroadcastVfo
    // must read rx2Enabled live (not hard-code false) so a tx_frequency_
    // thetis frame emitted after a VFO tune does not flip RX2 from
    // true (init) to false (live) between the two frames.
    void live_vfo_broadcast_reads_rx2_enabled_live();
    void existing_slice_after_deletion_gap_uses_stable_receiver_id();
};

void TestTciInitBurstLiveState::pinAppSettingsToCaptureConditions()
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("TciEmulateExpertSDR3Protocol"), QStringLiteral("False"));
    s.setValue(QStringLiteral("TciEmulateSunSDR2Pro"),         QStringLiteral("False"));
    s.setValue(QStringLiteral("TciCwluBecomesCw"),             QStringLiteral("False"));
    s.setValue(QStringLiteral("TciSendInitialFrequencyStateOnConnect"), QStringLiteral("True"));
    s.setValue(QStringLiteral("TciTxStreamBufferingMs"),       QStringLiteral("50"));
}

// Seeds mock VFOs to 7.150 MHz (RX1) and 21.250 MHz (RX2), then asserts that
// the init burst's dds/vfo/tx_frequency lines carry the seeded frequencies
// rather than the old hardcoded 14250000/7100000 placeholders.
void TestTciInitBurstLiveState::vfo_hz_drives_dds_and_vfo_lines()
{
    pinAppSettingsToCaptureConditions();
    TestMockRadioModel mock;

    constexpr qint64 kRx1Hz = 7150000;   // 40m
    constexpr qint64 kRx2Hz = 21250000;  // 15m
    mock.setVfoHz(0, 0, kRx1Hz);
    mock.setVfoHz(0, 1, kRx1Hz);
    mock.setVfoHz(1, 0, kRx2Hz);
    mock.setVfoHz(1, 1, kRx2Hz);

    TciProtocol p(&mock);
    const QStringList burst = p.buildInitBurst();

    // dds: per-slice 0/1 carries the seeded VFO-A frequency.
    QVERIFY2(burst.contains(QStringLiteral("dds:0,7150000;")),
             "dds:0 should carry seeded RX1 VFO (40m)");
    QVERIFY2(burst.contains(QStringLiteral("dds:1,21250000;")),
             "dds:1 should carry seeded RX2 VFO (15m)");

    // vfo: per-slice per-channel mirrors VFO A on both channels (NereusSDR
    // collapses VFO B onto the same slice; vfoHz(rx, chan) returns the slice
    // frequency regardless of chan -- see RadioModel.h setVfoHz docs).
    QVERIFY2(burst.contains(QStringLiteral("vfo:0,0,7150000;")),
             "vfo:0,0 should carry seeded RX1 VFO");
    QVERIFY2(burst.contains(QStringLiteral("vfo:0,1,7150000;")),
             "vfo:0,1 should carry seeded RX1 VFO");
    QVERIFY2(burst.contains(QStringLiteral("vfo:1,0,21250000;")),
             "vfo:1,0 should carry seeded RX2 VFO");
    QVERIFY2(burst.contains(QStringLiteral("vfo:1,1,21250000;")),
             "vfo:1,1 should carry seeded RX2 VFO");

    // tx_frequency lines should reflect the TX VFO -- which, until the
    // VFOBTX split-TX state lands, mirrors RX1's VFO A.
    QVERIFY2(burst.contains(QStringLiteral("tx_frequency:7150000;")),
             "tx_frequency should mirror RX1 VFO until VFOBTX state lands");
    // tx_frequency_thetis carries the derived band label ("40m" lowercase).
    QVERIFY2(burst.contains(QStringLiteral("tx_frequency_thetis:7150000,40m,false,false;")),
             "tx_frequency_thetis band label should derive from txFreqHz");
}

void TestTciInitBurstLiveState::mode_drives_modulation_lines()
{
    pinAppSettingsToCaptureConditions();
    TestMockRadioModel mock;
    mock.setMode(0, QStringLiteral("LSB"));
    mock.setMode(1, QStringLiteral("CWU"));

    TciProtocol p(&mock);
    const QStringList burst = p.buildInitBurst();

    QVERIFY2(burst.contains(QStringLiteral("modulation:0,LSB;")),
             "modulation:0 should carry seeded LSB mode");
    QVERIFY2(burst.contains(QStringLiteral("modulation:1,CWU;")),
             "modulation:1 should carry seeded CWU mode");
}

void TestTciInitBurstLiveState::filter_drives_rx_filter_band_lines()
{
    pinAppSettingsToCaptureConditions();
    TestMockRadioModel mock;
    // RX1 CW narrow, RX2 wide SSB -- distinct values per slice catch any
    // accidental cross-wiring of slice indices.
    mock.setFilterBand(0, 400, 900);
    mock.setFilterBand(1, 100, 3500);

    TciProtocol p(&mock);
    const QStringList burst = p.buildInitBurst();

    QVERIFY2(burst.contains(QStringLiteral("rx_filter_band:0,400,900;")),
             "rx_filter_band:0 should carry seeded RX1 cutoffs");
    QVERIFY2(burst.contains(QStringLiteral("rx_filter_band:1,100,3500;")),
             "rx_filter_band:1 should carry seeded RX2 cutoffs");
}

void TestTciInitBurstLiveState::agc_drives_agc_mode_and_gain_lines()
{
    pinAppSettingsToCaptureConditions();
    TestMockRadioModel mock;
    mock.setAgcMode(0, QStringLiteral("fast"));
    mock.setAgcMode(1, QStringLiteral("long"));
    mock.setAgcGain(0, 65);
    mock.setAgcGain(1, 80);

    TciProtocol p(&mock);
    const QStringList burst = p.buildInitBurst();

    QVERIFY2(burst.contains(QStringLiteral("agc_mode:0,fast;")),
             "agc_mode:0 should carry seeded fast mode");
    QVERIFY2(burst.contains(QStringLiteral("agc_mode:1,long;")),
             "agc_mode:1 should carry seeded long mode");
    QVERIFY2(burst.contains(QStringLiteral("agc_gain:0,65;")),
             "agc_gain:0 should carry seeded threshold");
    QVERIFY2(burst.contains(QStringLiteral("agc_gain:1,80;")),
             "agc_gain:1 should carry seeded threshold");
}

void TestTciInitBurstLiveState::global_toggles_drive_their_lines()
{
    pinAppSettingsToCaptureConditions();
    TestMockRadioModel mock;
    mock.setMox(true);
    mock.setRitEnable(true);
    mock.setRitOffset(123);
    mock.setXitEnable(true);
    mock.setXitOffset(-456);
    mock.setLock(0, true);
    mock.setGlobalMute(true);

    TciProtocol p(&mock);
    const QStringList burst = p.buildInitBurst();

    // MOX gates rx_enable / tx_enable / trx.  Thetis convention
    // (TCIServer.cs:2515-2516 + 2618-2619 [v2.10.3.15]):
    //   sendRXEnable(0, !MOX)   -- "RX is ready" iff not transmitting
    //   sendTXEnable(0, !MOX)   -- "TX ready to be activated" iff not already
    //                              transmitting (counter-intuitive but source-
    //                              faithful: 'enable' means 'can be commanded',
    //                              not 'currently keyed').
    //   sendMOX(0, MOX && ...)  -- trx:0 mirrors actual MOX state.
    QVERIFY2(burst.contains(QStringLiteral("rx_enable:0,false;")),
             "MOX on must mark RX-not-ready (rx_enable:0,false)");
    QVERIFY2(burst.contains(QStringLiteral("tx_enable:0,false;")),
             "MOX on must mark TX-not-ready (tx_enable:0,false; "
             "'enable' = ready-to-be-commanded per Thetis convention)");
    QVERIFY2(burst.contains(QStringLiteral("trx:0,true;")),
             "MOX on must set trx:0,true (actual TX state)");

    QVERIFY2(burst.contains(QStringLiteral("rit_enable:0,true;")),
             "RIT-on must flow into rit_enable:0,true");
    QVERIFY2(burst.contains(QStringLiteral("rit_offset:0,123;")),
             "RIT offset must flow into rit_offset:0,123");
    QVERIFY2(burst.contains(QStringLiteral("xit_enable:0,true;")),
             "XIT-on must flow into xit_enable:0,true");
    QVERIFY2(burst.contains(QStringLiteral("xit_offset:0,-456;")),
             "XIT offset must flow into xit_offset:0,-456");

    QVERIFY2(burst.contains(QStringLiteral("lock:0,true;")),
             "VFOALock on must flow into lock:0,true");
    QVERIFY2(burst.contains(QStringLiteral("mute:true;")),
             "Global mute on must flow into mute:true");
}

void TestTciInitBurstLiveState::per_rx_flags_drive_their_lines()
{
    pinAppSettingsToCaptureConditions();
    TestMockRadioModel mock;
    // Distinct on/off per slice catches accidental cross-wiring.
    mock.setSqlEnable(0, true);
    mock.setSqlEnable(1, false);
    mock.setSqlLevel(0, -90);
    mock.setSqlLevel(1, -123);
    mock.setRxMute(0, true);
    mock.setRxMute(1, false);
    mock.setRxNr(0, true, 3);    // NR3
    mock.setRxNr(1, false, 0);
    mock.setRxNb(0, true);
    mock.setRxNb(1, false);
    mock.setRxAnf(0, true);
    mock.setRxAnf(1, false);
    mock.setRxApf(0, true);
    mock.setRxApf(1, false);
    mock.setRxBin(0, true);
    mock.setRxBin(1, false);
    mock.setRxNf(0, true);
    mock.setRxNf(1, false);
    mock.setRxCtun(0, true);
    mock.setRxCtun(1, false);
    mock.setSplit(0, true);
    mock.setSplit(1, false);

    TciProtocol p(&mock);
    const QStringList burst = p.buildInitBurst();

    QVERIFY(burst.contains(QStringLiteral("sql_enable:0,true;")));
    QVERIFY(burst.contains(QStringLiteral("sql_enable:1,false;")));
    QVERIFY(burst.contains(QStringLiteral("sql_level:0,-90;")));
    QVERIFY(burst.contains(QStringLiteral("sql_level:1,-123;")));

    QVERIFY(burst.contains(QStringLiteral("rx_mute:0,true;")));
    QVERIFY(burst.contains(QStringLiteral("rx_mute:1,false;")));

    QVERIFY(burst.contains(QStringLiteral("rx_nr_enable:0,true;")));
    QVERIFY(burst.contains(QStringLiteral("rx_nr_enable:1,false;")));
    QVERIFY(burst.contains(QStringLiteral("rx_nr_enable_ex:0,true,3;")));
    QVERIFY(burst.contains(QStringLiteral("rx_nr_enable_ex:1,false,0;")));

    QVERIFY(burst.contains(QStringLiteral("rx_nb_enable:0,true;")));
    QVERIFY(burst.contains(QStringLiteral("rx_nb_enable:1,false;")));
    QVERIFY(burst.contains(QStringLiteral("rx_anf_enable:0,true;")));
    QVERIFY(burst.contains(QStringLiteral("rx_anf_enable:1,false;")));
    QVERIFY(burst.contains(QStringLiteral("rx_apf_enable:0,true;")));
    QVERIFY(burst.contains(QStringLiteral("rx_apf_enable:1,false;")));
    QVERIFY(burst.contains(QStringLiteral("rx_bin_enable:0,true;")));
    QVERIFY(burst.contains(QStringLiteral("rx_bin_enable:1,false;")));
    QVERIFY(burst.contains(QStringLiteral("rx_nf_enable:0,true;")));
    QVERIFY(burst.contains(QStringLiteral("rx_nf_enable:1,false;")));

    QVERIFY(burst.contains(QStringLiteral("rx_ctun_ex:0,true;")));
    QVERIFY(burst.contains(QStringLiteral("rx_ctun_ex:1,false;")));

    QVERIFY(burst.contains(QStringLiteral("split_enable:0,true;")));
    QVERIFY(burst.contains(QStringLiteral("split_enable:1,false;")));
}

void TestTciInitBurstLiveState::balance_drives_rx_balance_lines()
{
    pinAppSettingsToCaptureConditions();
    TestMockRadioModel mock;
    mock.setRxBalance(0, 0, 10.50);
    mock.setRxBalance(0, 1, 25.75);
    mock.setRxBalance(1, 0, -15.25);
    mock.setRxBalance(1, 1, 33.00);

    TciProtocol p(&mock);
    const QStringList burst = p.buildInitBurst();

    QVERIFY(burst.contains(QStringLiteral("rx_balance:0,0,10.50;")));
    QVERIFY(burst.contains(QStringLiteral("rx_balance:0,1,25.75;")));
    QVERIFY(burst.contains(QStringLiteral("rx_balance:1,0,-15.25;")));
    QVERIFY(burst.contains(QStringLiteral("rx_balance:1,1,33.00;")));
}

void TestTciInitBurstLiveState::audio_iq_stream_config_drives_lines()
{
    pinAppSettingsToCaptureConditions();
    TestMockRadioModel mock;
    mock.setIqSampleRate(384000);
    mock.setAudioSampleRate(24000);
    mock.setAudioStreamSampleType(QStringLiteral("int24"));
    mock.setAudioStreamChannels(1);
    mock.setAudioStreamSamples(1024);

    TciProtocol p(&mock);
    const QStringList burst = p.buildInitBurst();

    QVERIFY(burst.contains(QStringLiteral("iq_samplerate:384000;")));
    QVERIFY(burst.contains(QStringLiteral("audio_samplerate:24000;")));
    QVERIFY(burst.contains(QStringLiteral("audio_stream_sample_type:int24;")));
    QVERIFY(burst.contains(QStringLiteral("audio_stream_channels:1;")));
    QVERIFY(burst.contains(QStringLiteral("audio_stream_samples:1024;")));
}

void TestTciInitBurstLiveState::volume_drives_db_lines()
{
    pinAppSettingsToCaptureConditions();
    TestMockRadioModel mock;
    // afLinear=50 maps via tciLinearToDbVolume to -30.0 dB
    // (linear 50/100 * (0 - -60) + -60 = -30).  monLinear=25 maps to -45.0 dB.
    mock.setAfLinear(50);
    mock.setMonLinear(25);

    TciProtocol p(&mock);
    const QStringList burst = p.buildInitBurst();

    // sendVolume format: "volume:%1;" with F1 conversion via Qt arg (no
    // trailing .0 required) -- existing buildVolumeLine controls format.
    bool sawVolume = false;
    bool sawMonVolume = false;
    for (const auto& line : burst) {
        if (line.startsWith(QStringLiteral("volume:")) && !line.startsWith(QStringLiteral("mon_volume:"))) {
            // Accept -30 / -30.0 forms; the exact format helper choice is
            // not part of this regression -- just assert the magnitude.
            QVERIFY2(line.contains(QStringLiteral("-30")),
                     qPrintable(QStringLiteral("volume line should be near -30 dB, got: %1").arg(line)));
            sawVolume = true;
        }
        if (line.startsWith(QStringLiteral("mon_volume:"))) {
            QVERIFY2(line.contains(QStringLiteral("-45")),
                     qPrintable(QStringLiteral("mon_volume line should be near -45 dB, got: %1").arg(line)));
            sawMonVolume = true;
        }
    }
    QVERIFY2(sawVolume, "burst must include a volume: line");
    QVERIFY2(sawMonVolume, "burst must include a mon_volume: line");
}

void TestTciInitBurstLiveState::tx_profile_drives_lines()
{
    pinAppSettingsToCaptureConditions();
    TestMockRadioModel mock;
    mock.setTxProfile(QStringLiteral("Default"));  // mock list always = {Default}

    TciProtocol p(&mock);
    const QStringList burst = p.buildInitBurst();

    QVERIFY(burst.contains(QStringLiteral("tx_profiles_ex:Default;")));
    QVERIFY(burst.contains(QStringLiteral("tx_profile_ex:Default;")));
}

void TestTciInitBurstLiveState::calibration_drives_lines()
{
    pinAppSettingsToCaptureConditions();
    TestMockRadioModel mock;
    // F6 format: ".000123" requires 6 digits after the decimal point.
    mock.setCalibration(0, 0.123456, 0.234567, 0.345678, 0.456789, 0.567890);
    mock.setCalibration(1, 1.0, 2.0, 3.0, 4.0, 5.0);

    TciProtocol p(&mock);
    const QStringList burst = p.buildInitBurst();

    QVERIFY(burst.contains(QStringLiteral(
        "calibration_ex:0,0.123456,0.234567,0.345678,0.456789,0.567890;")));
    QVERIFY(burst.contains(QStringLiteral(
        "calibration_ex:1,1.000000,2.000000,3.000000,4.000000,5.000000;")));
}

void TestTciInitBurstLiveState::rx_volume_uses_audio_gain_to_db_log_curve()
{
    pinAppSettingsToCaptureConditions();
    TestMockRadioModel mock;
    // Phase 3F Sub-Epic J Task 10: rx_volume now reads PER-SLICE AF gain
    // (RadioModel::afGain(rx), backed by SliceModel::afGain on production)
    // rather than the radio-global afLinear master volume this test used to
    // seed exclusively.  Seed the two independently, with DIFFERENT values,
    // to prove both the LOG-curve conversion AND that the two are decoupled
    // (afLinear no longer drives rx_volume -- that was the bug: every
    // rx_volume slot used to echo afLinear, so receiver 1 read receiver 0's
    // value).
    //   afGain(0)=50  -> tciAudioGainToDb(50/100) = 20*log10(0.5)  ~= -6.02 dB
    //   afGain(1)=25  -> tciAudioGainToDb(25/100) = 20*log10(0.25) ~= -12.04 dB
    //   afLinear=50   -> tciLinearToDbVolume(50)  = -30.0 dB (LINEAR curve,
    //                    global "volume:" line only)
    // Thetis convention:
    //   rx_volume:* uses audioGainToDb (TCIServer.cs:4778-4787 [v2.10.3.15])
    //   volume:     uses linearToDbVolume (TCIServer.cs:4110-4120 [v2.10.3.13])
    // The format is "F2" so we expect "-6.02" / "-12.04".
    mock.setAfLinear(50);
    mock.setRxAfGain(0, 50);
    mock.setRxAfGain(1, 25);

    TciProtocol p(&mock);
    const QStringList burst = p.buildInitBurst();

    QVERIFY2(burst.contains(QStringLiteral("rx_volume:0,0,-6.02;")),
             "rx_volume:0 should use audioGainToDb LOG curve (-6.02 dB for "
             "afGain=50), not linearToDbVolume (-30.0 dB).");
    QVERIFY2(burst.contains(QStringLiteral("rx_volume:1,0,-12.04;")),
             "rx_volume:1 must reflect receiver 1's OWN afGain (-12.04 dB "
             "for afGain=25), not receiver 0's afGain or the radio-global "
             "afLinear -- this is the bug Task 10 fixed.");
    // The global volume: line keeps the linear curve and stays sourced from
    // afLinear, unaffected by the per-slice rx_volume fix.
    QVERIFY2(burst.contains(QStringLiteral("volume:-30.0;")),
             "volume: should use linearToDbVolume LINEAR curve (-30.0 dB "
             "for afLinear=50), separate from rx_volume's log curve.");
}

void TestTciInitBurstLiveState::tune_drives_tune_lines()
{
    pinAppSettingsToCaptureConditions();
    TestMockRadioModel mock;
    mock.setTune(true);
    // Thetis sendTune at TCIServer.cs:2636-2637 [v2.10.3.15]:
    //   sendTune(0, TUN && !(VFOBTX && bRX2Enabled));
    //   sendTune(1, TUN && (VFOBTX && bRX2Enabled));
    // With bRX2Enabled=false (mock default), the VFOBTX clause is false either
    // way, so the result is just (TUN, false): tune:0,true; tune:1,false;
    TciProtocol p(&mock);
    const QStringList burst = p.buildInitBurst();

    QVERIFY2(burst.contains(QStringLiteral("tune:0,true;")),
             "TUN on must set tune:0,true;");
    QVERIFY2(burst.contains(QStringLiteral("tune:1,false;")),
             "tune:1 must be false when bRX2Enabled is false (Thetis "
             "VFOBTX && bRX2Enabled gate cited at TCIServer.cs:2637)");
}

void TestTciInitBurstLiveState::mon_enabled_drives_mon_enable_line()
{
    pinAppSettingsToCaptureConditions();

    {
        TestMockRadioModel mock;
        mock.setMonEnabled(false);
        TciProtocol p(&mock);
        QVERIFY(p.buildInitBurst().contains(QStringLiteral("mon_enable:false;")));
    }
    {
        TestMockRadioModel mock;
        mock.setMonEnabled(true);
        TciProtocol p(&mock);
        QVERIFY(p.buildInitBurst().contains(QStringLiteral("mon_enable:true;")));
    }
}

void TestTciInitBurstLiveState::power_on_drives_start_stop_line()
{
    pinAppSettingsToCaptureConditions();

    // Thetis sendStartStop at TCIServer.cs:2449-2455 [v2.10.3.15]:
    //   if (bPower) sendStart(); else sendStop();
    // sendStart emits "start;", sendStop emits "stop;".

    {
        TestMockRadioModel mock;
        mock.setPowerOn(true);
        TciProtocol p(&mock);
        const QStringList burst = p.buildInitBurst();
        QVERIFY2(burst.contains(QStringLiteral("start;")),
                 "PowerOn=true must emit 'start;' frame");
        QVERIFY2(!burst.contains(QStringLiteral("stop;")),
                 "PowerOn=true must NOT emit 'stop;' frame");
    }
    {
        TestMockRadioModel mock;
        mock.setPowerOn(false);
        TciProtocol p(&mock);
        const QStringList burst = p.buildInitBurst();
        QVERIFY2(burst.contains(QStringLiteral("stop;")),
                 "PowerOn=false must emit 'stop;' frame");
        QVERIFY2(!burst.contains(QStringLiteral("start;")),
                 "PowerOn=false must NOT emit 'start;' frame");
    }
}

void TestTciInitBurstLiveState::rx2_enabled_flips_rx2_gated_lines()
{
    pinAppSettingsToCaptureConditions();

    {
        // bRX2Enabled = false (mock default) -- multiple RX2-gated lines collapse
        // to their RX2-off form.
        TestMockRadioModel mock;
        mock.setRx2Enabled(false);
        TciProtocol p(&mock);
        const QStringList burst = p.buildInitBurst();
        QVERIFY2(burst.contains(QStringLiteral("rx_enable:1,false;")),
                 "rx_enable:1 must be false when RX2 disabled");
        QVERIFY2(burst.contains(QStringLiteral("tx_enable:1,false;")),
                 "tx_enable:1 must be false when RX2 disabled");
        QVERIFY2(burst.contains(QStringLiteral("rx_channel_enable:1,0,false;")),
                 "rx_channel_enable:1,0 must be false when RX2 disabled");
    }
    {
        TestMockRadioModel mock;
        mock.setRx2Enabled(true);
        // MOX off by default so the !MOX side wins.
        TciProtocol p(&mock);
        const QStringList burst = p.buildInitBurst();
        QVERIFY2(burst.contains(QStringLiteral("rx_enable:1,true;")),
                 "rx_enable:1 must be true when RX2 enabled && !MOX");
        QVERIFY2(burst.contains(QStringLiteral("tx_enable:1,true;")),
                 "tx_enable:1 must be true when RX2 enabled && !MOX");
        QVERIFY2(burst.contains(QStringLiteral("rx_channel_enable:1,0,true;")),
                 "rx_channel_enable:1,0 must be true when RX2 enabled");
        // lock:1 line appears only when bRX2Enabled (Thetis TCIServer.cs:2596-2599).
        QVERIFY2(burst.contains(QStringLiteral("lock:1,false;")),
                 "lock:1 must be emitted when RX2 enabled");
    }
}

void TestTciInitBurstLiveState::digl_digu_offset_drives_offset_lines()
{
    pinAppSettingsToCaptureConditions();
    TestMockRadioModel mock;
    mock.setDiglOffset(2100);  // close to Thetis radio-global default 2210
    mock.setDiguOffset(1400);  // close to Thetis radio-global default 1500

    TciProtocol p(&mock);
    const QStringList burst = p.buildInitBurst();

    QVERIFY2(burst.contains(QStringLiteral("digl_offset:2100;")),
             "DIGL offset must flow from RadioModel::diglOffset()");
    QVERIFY2(burst.contains(QStringLiteral("digu_offset:1400;")),
             "DIGU offset must flow from RadioModel::diguOffset()");
}

void TestTciInitBurstLiveState::cw_macros_match_thetis_null_controller_defaults()
{
    pinAppSettingsToCaptureConditions();
    TestMockRadioModel mock;
    // No CwController on NereusSDR yet (3M-2).  Thetis's TCIServer
    // GetCwMacrosSpeed / GetCwMacrosDelay / GetCwKeyerSpeed
    // (TCIServer.cs:6950-6973 [v2.10.3.15]) return 30 / 0 / 30 when
    // m_cwController is null.  Our port must match that defaults set
    // verbatim (NOT the old 25/0/25 placeholder).
    TciProtocol p(&mock);
    const QStringList burst = p.buildInitBurst();

    QVERIFY2(burst.contains(QStringLiteral("cw_macros_speed:30;")),
             "cw_macros_speed default must be Thetis null-controller 30 WPM");
    QVERIFY2(burst.contains(QStringLiteral("cw_macros_delay:0;")),
             "cw_macros_delay default must be Thetis null-controller 0 ms");
    QVERIFY2(burst.contains(QStringLiteral("cw_keyer_speed:30;")),
             "cw_keyer_speed default must be Thetis null-controller 30 WPM");
}

// Regression catch: builds two distinct mock states and asserts the resulting
// init bursts differ in their dds/vfo/modulation/agc lines.  If anyone re-
// hardcodes a placeholder, this will fail even when per-family tests pass
// against a single value (e.g. agc=normal/40 both before and after).
void TestTciInitBurstLiveState::distinct_state_yields_distinct_burst()
{
    pinAppSettingsToCaptureConditions();

    auto captureBurst = [](qint64 vfo, const QString& mode, int agcGain) {
        TestMockRadioModel mock;
        mock.setVfoHz(0, 0, vfo);
        mock.setMode(0, mode);
        mock.setAgcGain(0, agcGain);
        TciProtocol p(&mock);
        return p.buildInitBurst();
    };

    const QStringList burstA = captureBurst(7150000, QStringLiteral("LSB"), 40);
    const QStringList burstB = captureBurst(21250000, QStringLiteral("CWU"), 80);

    QVERIFY2(burstA != burstB,
             "Distinct RadioModel state must produce distinct init bursts "
             "(regression catch for re-hardcoded placeholders).");

    // Spot-check the per-field differences so a future failure points at
    // the right family.
    QVERIFY(burstA.contains(QStringLiteral("dds:0,7150000;")));
    QVERIFY(burstB.contains(QStringLiteral("dds:0,21250000;")));
    QVERIFY(burstA.contains(QStringLiteral("modulation:0,LSB;")));
    QVERIFY(burstB.contains(QStringLiteral("modulation:0,CWU;")));
    QVERIFY(burstA.contains(QStringLiteral("agc_gain:0,40;")));
    QVERIFY(burstB.contains(QStringLiteral("agc_gain:0,80;")));
}

// PR #279 review P3 (2026-05-22): enqueueLocalBroadcastVfo previously
// hard-coded ,false,false in the tx_frequency_thetis live frame for
// every VFO move.  Init burst read the real rx2Enabled, so after the
// first VFO tune, real TCI clients saw RX2 flip from true (init) to
// false (live) inside the same connection.  Fix in 9ae5f135: read
// rx2Enabled via QMetaObject::invokeMethod, same shim the init burst
// uses.  This test pins the live broadcast path.
void TestTciInitBurstLiveState::live_vfo_broadcast_reads_rx2_enabled_live()
{
    pinAppSettingsToCaptureConditions();

    // RX2 enabled: live VFO frame must carry rx2en=true.
    {
        TestMockRadioModel mock;
        mock.setRx2Enabled(true);
        TciProtocol p(&mock);

        // Trigger the live broadcast path (rotary tune simulated).
        p.enqueueLocalBroadcastVfo(/*rxIndex=*/0, /*hz=*/14250000LL, /*isTxBound=*/true);

        // Drain the coalescer into the pending-notifications queue so
        // we can inspect emitted frames.
        p.drainCoalescedNotifications();

        QStringList frames;
        while (p.hasPendingNotification()) {
            frames << p.takePendingNotification();
        }

        // tx_frequency_thetis format from sendTXFrequencyChanged
        // (TCIServer.cs:2249-2254 [v2.10.3.15]):
        //   tx_frequency_thetis:<hz>,<band>,<rx2en>,<txvfob>;
        // Expect rx2en=true for this case.  VFOBTX stays false until
        // 3F multi-pan lands the full TXFreq logic.
        bool found = false;
        for (const QString& f : frames) {
            if (f.startsWith(QStringLiteral("tx_frequency_thetis:"))) {
                QVERIFY2(f.contains(QStringLiteral(",true,false;")),
                    qPrintable(QString("rx2_enabled=true must produce "
                        "rx2en=true in live frame; got: %1").arg(f)));
                found = true;
                break;
            }
        }
        QVERIFY2(found, "no tx_frequency_thetis frame in drained queue");
    }

    // RX2 disabled: live frame must carry rx2en=false (matches init).
    {
        TestMockRadioModel mock;
        mock.setRx2Enabled(false);
        TciProtocol p(&mock);
        p.enqueueLocalBroadcastVfo(/*rxIndex=*/0, /*hz=*/7100000LL, /*isTxBound=*/true);
        p.drainCoalescedNotifications();

        QStringList frames;
        while (p.hasPendingNotification()) {
            frames << p.takePendingNotification();
        }

        bool found = false;
        for (const QString& f : frames) {
            if (f.startsWith(QStringLiteral("tx_frequency_thetis:"))) {
                QVERIFY2(f.contains(QStringLiteral(",false,false;")),
                    qPrintable(QString("rx2_enabled=false must produce "
                        "rx2en=false in live frame; got: %1").arg(f)));
                found = true;
                break;
            }
        }
        QVERIFY2(found, "no tx_frequency_thetis frame in drained queue");
    }
}

void TestTciInitBurstLiveState::
    existing_slice_after_deletion_gap_uses_stable_receiver_id()
{
    pinAppSettingsToCaptureConditions();
    RadioModel radio;
    radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5, 192000);
    const int a = radio.addSlice(QStringLiteral("pan-0"));
    const int b = radio.addSlice(QStringLiteral("pan-0"));
    const int c = radio.addSlice(QStringLiteral("pan-0"));
    QCOMPARE(a, 0);
    QCOMPARE(b, 1);
    QCOMPARE(c, 2);
    SliceModel* const sliceB = radio.sliceById(b);
    SliceModel* const sliceC = radio.sliceById(c);
    QVERIFY(sliceB != nullptr);
    QVERIFY(sliceC != nullptr);

    // Delete the FIRST slice, not the middle one. Codex review round 6,
    // PR #293 changed which gap this case can use.
    //
    // The property under test is unchanged: a surviving slice broadcasts its
    // stable id, never its current list position. Removing A leaves B at
    // list position 0 while B's id stays 1, which demonstrates exactly that
    // and does it inside the two receivers this server advertises.
    //
    // It used to remove B and assert on C, at id 2. That id is outside
    // trx_count:2, so the server was asserting it would send a client frames
    // for a receiver the client had not allocated and could never address
    // back: RadioModel resolves inbound `vfo:N` straight through
    // sliceById(N) (the mapping decision documented in TciProtocol.cpp), so
    // a client that was told about two receivers has no way to reply about a
    // third. Outbound was the only place that id leaked, and it now stops at
    // the advertised count.
    radio.removeSlice(a);
    QCOMPARE(radio.slices().size(), 2);
    QCOMPARE(radio.slices().at(0), sliceB);
    QCOMPARE(sliceB->sliceIndex(), 1);

    // Attach TCI only after the deletion gap exists, so B takes the
    // hookSliceBroadcasts() existing-slice path rather than sliceAdded().
    TciServer server(&radio);
    QVERIFY(server.start(0));
    QWebSocket client;
    QStringList frames;
    connect(&client, &QWebSocket::textMessageReceived,
            this, [&frames](const QString& frame) { frames.append(frame); });
    QSignalSpy connectedSpy(&client, &QWebSocket::connected);
    client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
    QVERIFY(connectedSpy.wait(2000));
    QTRY_VERIFY(frames.contains(QStringLiteral("ready;")));

    // Once the initial state burst establishes the client, B's first
    // one-shot announcement and its subsequent coalesced VFO update must both
    // carry B's stable id (1), never its current list position (0).
    frames.clear();
    sliceB->setDspMode(DSPMode::CWU);
    sliceB->setFrequency(18123456.0);
    QTRY_VERIFY(frames.contains(QStringLiteral("modulation:1,CWU;")));
    QTRY_VERIFY(frames.contains(QStringLiteral("vfo:1,0,18123456;")));
    QVERIFY2(!frames.contains(QStringLiteral("modulation:0,CWU;")),
             "the surviving slice must not adopt the departed slice's id");
    QVERIFY2(!frames.contains(QStringLiteral("vfo:0,0,18123456;")),
             "nor its list position");

    // And the slice past the advertised count stays off the wire entirely,
    // however live it is locally. Design doc §1.2: Slice C/D exist
    // internally and are not exposed via TCI.
    frames.clear();
    sliceC->setDspMode(DSPMode::CWL);
    sliceC->setFrequency(21050000.0);
    QTest::qWait(250);
    for (const QString& f : std::as_const(frames)) {
        QVERIFY2(!f.startsWith(QStringLiteral("modulation:2,")),
                 qPrintable(QStringLiteral("slice id 2 is past trx_count:2 "
                     "and must not be broadcast: %1").arg(f)));
        QVERIFY2(!f.startsWith(QStringLiteral("vfo:2,")),
                 qPrintable(QStringLiteral("slice id 2 is past trx_count:2 "
                     "and must not be broadcast: %1").arg(f)));
    }

    client.close();
    server.stop();
}

QTEST_MAIN(TestTciInitBurstLiveState)
#include "tst_tci_init_burst_live_state.moc"
