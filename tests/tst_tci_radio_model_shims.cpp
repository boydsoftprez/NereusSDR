// no-port-check: NereusSDR-original test for the Phase 3J-1 closeout
// Item 3 Q_INVOKABLE long-tail shims.  Each test invokes a shim via the
// same QMetaObject::invokeMethod path TciProtocol uses in production,
// then verifies the underlying model state (SliceModel Q_PROPERTY or
// RadioModel field) changed as expected.  The matrix test
// (tst_tci_matrix_runner) already exercises these against TestMockRadio
// Model; this file pins the production-side wiring.
//
// One test per category (14 categories) rather than one per shim --
// per the plan doc's verification policy at
// docs/architecture/2026-05-12-phase3j-1-loose-ends-plan.md Item 3.

#include <QtTest/QtTest>
#include "core/TciProtocol.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "core/WdspTypes.h"

using namespace NereusSDR;

class TestTciRadioModelShims : public QObject {
    Q_OBJECT

private:
    // Helper: build a RadioModel + one active slice, return both for tests
    // that need slice-level state checks.
    static SliceModel* setupOneSlice(RadioModel& model) {
        const int idx = model.addSlice();
        Q_UNUSED(idx);
        return model.activeSlice();
    }

private slots:
    void vfo_lock_roundtrips() {
        RadioModel m;
        SliceModel* slice = setupOneSlice(m);
        QVERIFY(slice);
        QMetaObject::invokeMethod(&m, "setVfoLock",
                                  Q_ARG(int, 0), Q_ARG(int, 0),
                                  Q_ARG(bool, true));
        QVERIFY(slice->locked());
        bool out = false;
        QMetaObject::invokeMethod(&m, "vfoLock",
                                  Q_RETURN_ARG(bool, out),
                                  Q_ARG(int, 0), Q_ARG(int, 0));
        QVERIFY(out);
    }

    void lock_alias_routes_to_same_state() {
        RadioModel m;
        SliceModel* slice = setupOneSlice(m);
        QVERIFY(slice);
        QMetaObject::invokeMethod(&m, "setLock",
                                  Q_ARG(int, 0), Q_ARG(bool, true));
        QVERIFY(slice->locked());
    }

    void global_mute_roundtrips() {
        RadioModel m;
        QMetaObject::invokeMethod(&m, "setGlobalMute", Q_ARG(bool, true));
        bool out = false;
        QMetaObject::invokeMethod(&m, "globalMute",
                                  Q_RETURN_ARG(bool, out));
        QVERIFY(out);
    }

    void rx_mute_routes_to_slice() {
        RadioModel m;
        SliceModel* slice = setupOneSlice(m);
        QVERIFY(slice);
        QMetaObject::invokeMethod(&m, "setRxMute",
                                  Q_ARG(int, 0), Q_ARG(bool, true));
        QVERIFY(slice->muted());
    }

    void set_filter_band_routes_both_cutoffs() {
        RadioModel m;
        SliceModel* slice = setupOneSlice(m);
        QVERIFY(slice);
        QMetaObject::invokeMethod(&m, "setFilterBand",
                                  Q_ARG(int, 0),
                                  Q_ARG(int, 200),
                                  Q_ARG(int, 2800));
        QCOMPARE(slice->filterLow(),  200);
        QCOMPARE(slice->filterHigh(), 2800);
    }

    void agc_mode_string_maps_to_enum() {
        RadioModel m;
        SliceModel* slice = setupOneSlice(m);
        QVERIFY(slice);
        QMetaObject::invokeMethod(&m, "setAgcMode",
                                  Q_ARG(int, 0),
                                  Q_ARG(QString, QStringLiteral("FAST")));
        QCOMPARE(slice->agcMode(), AGCMode::Fast);
        QString out;
        QMetaObject::invokeMethod(&m, "agcMode",
                                  Q_RETURN_ARG(QString, out),
                                  Q_ARG(int, 0));
        // Note: RadioModel::agcMode() returns the upper-case enum-style
        // token used internally; TciProtocol::tciAgcModeForWire() converts
        // to the Thetis wire token at emit time (see prod_agc_mode_wire_
        // token_thetis_faithful below).
        QCOMPARE(out, QStringLiteral("FAST"));
    }

    // PR #279 review P2 (2026-05-22): pin the wire-token conversion that
    // bridges RadioModel's internal enum-style strings ("MED"/"FAST"/...)
    // to Thetis's lowercase TCI wire tokens ("normal"/"fast"/...).
    // Without this conversion the init burst emitted frames like
    // "agc_mode:0,MED;" instead of "agc_mode:0,normal;", which real
    // clients reject.  Mirrors Thetis agcModeToTciMode at TCIServer.cs:
    // 2260-2279 [v2.10.3.15].
    void prod_agc_mode_wire_token_thetis_faithful() {
        // Direct enum -> wire mapping.
        QCOMPARE(TciProtocol::tciAgcModeForWire(QStringLiteral("OFF")),
                 QStringLiteral("off"));
        QCOMPARE(TciProtocol::tciAgcModeForWire(QStringLiteral("LONG")),
                 QStringLiteral("long"));
        QCOMPARE(TciProtocol::tciAgcModeForWire(QStringLiteral("SLOW")),
                 QStringLiteral("slow"));
        QCOMPARE(TciProtocol::tciAgcModeForWire(QStringLiteral("FAST")),
                 QStringLiteral("fast"));
        QCOMPARE(TciProtocol::tciAgcModeForWire(QStringLiteral("CUSTOM")),
                 QStringLiteral("custom"));
        // MED -> normal special mapping (the bug reported in review P2).
        QCOMPARE(TciProtocol::tciAgcModeForWire(QStringLiteral("MED")),
                 QStringLiteral("normal"));

        // Synonyms that tciModeToAgcMode normalises (TCIServer.cs:2280-
        // 2303 [v2.10.3.15]) -- round-trip safety so a client that sent
        // "medium" or "fixed" sees the canonical token come back.
        QCOMPARE(TciProtocol::tciAgcModeForWire(QStringLiteral("MEDIUM")),
                 QStringLiteral("normal"));
        QCOMPARE(TciProtocol::tciAgcModeForWire(QStringLiteral("NORMAL")),
                 QStringLiteral("normal"));
        QCOMPARE(TciProtocol::tciAgcModeForWire(QStringLiteral("FIXD")),
                 QStringLiteral("off"));
        QCOMPARE(TciProtocol::tciAgcModeForWire(QStringLiteral("FIXED")),
                 QStringLiteral("off"));

        // Defensive default: unknown enum strings fall to "normal", same
        // as Thetis's default branch.  Catches any future enum addition
        // that forgets to extend the helper.
        QCOMPARE(TciProtocol::tciAgcModeForWire(QStringLiteral("BOGUS")),
                 QStringLiteral("normal"));
        QCOMPARE(TciProtocol::tciAgcModeForWire(QString()),
                 QStringLiteral("normal"));

        // Production-shaped end-to-end: SliceModel enum -> RadioModel
        // shim -> wire helper.  Locks in the integration that the init
        // burst exercises on real-client connect.
        RadioModel m;
        SliceModel* slice = setupOneSlice(m);
        QVERIFY(slice);
        slice->setAgcMode(AGCMode::Med);
        QString modeStr;
        QMetaObject::invokeMethod(&m, "agcMode",
                                  Q_RETURN_ARG(QString, modeStr),
                                  Q_ARG(int, 0));
        QCOMPARE(modeStr, QStringLiteral("MED"));
        QCOMPARE(TciProtocol::tciAgcModeForWire(modeStr),
                 QStringLiteral("normal"));
    }

    void agc_gain_routes_to_threshold() {
        RadioModel m;
        SliceModel* slice = setupOneSlice(m);
        QVERIFY(slice);
        QMetaObject::invokeMethod(&m, "setAgcGain",
                                  Q_ARG(int, 0), Q_ARG(int, 42));
        QCOMPARE(slice->agcThreshold(), 42);
    }

    void squelch_enable_and_level_route_to_slice() {
        RadioModel m;
        SliceModel* slice = setupOneSlice(m);
        QVERIFY(slice);
        QMetaObject::invokeMethod(&m, "setSqlEnable",
                                  Q_ARG(int, 0), Q_ARG(bool, true));
        QVERIFY(slice->ssqlEnabled());
        QMetaObject::invokeMethod(&m, "setSqlLevel",
                                  Q_ARG(int, 0), Q_ARG(int, -73));
        QCOMPARE(static_cast<int>(slice->ssqlThresh()), -73);
    }

    void rit_xit_route_to_active_slice() {
        RadioModel m;
        SliceModel* slice = setupOneSlice(m);
        QVERIFY(slice);
        QMetaObject::invokeMethod(&m, "setRitEnable", Q_ARG(bool, true));
        QMetaObject::invokeMethod(&m, "setRitOffset", Q_ARG(int, 150));
        QVERIFY(slice->ritEnabled());
        QCOMPARE(slice->ritHz(), 150);

        QMetaObject::invokeMethod(&m, "setXitEnable", Q_ARG(bool, true));
        QMetaObject::invokeMethod(&m, "setXitOffset", Q_ARG(int, -200));
        QVERIFY(slice->xitEnabled());
        QCOMPARE(slice->xitHz(), -200);
    }

    void rx_balance_routes_to_audio_pan() {
        RadioModel m;
        SliceModel* slice = setupOneSlice(m);
        QVERIFY(slice);
        QMetaObject::invokeMethod(&m, "setRxBalance",
                                  Q_ARG(int, 0), Q_ARG(int, 0),
                                  Q_ARG(double, 0.25));
        QCOMPARE(slice->audioPan(), 0.25);
    }

    void rx_nb_toggles_nb_mode() {
        RadioModel m;
        SliceModel* slice = setupOneSlice(m);
        QVERIFY(slice);
        QMetaObject::invokeMethod(&m, "setRxNb",
                                  Q_ARG(int, 0), Q_ARG(bool, true));
        QVERIFY(slice->nbMode() != NbMode::Off);
        bool out = false;
        QMetaObject::invokeMethod(&m, "rxNb",
                                  Q_RETURN_ARG(bool, out),
                                  Q_ARG(int, 0));
        QVERIFY(out);
    }

    void rx_nr_selects_slot() {
        RadioModel m;
        SliceModel* slice = setupOneSlice(m);
        QVERIFY(slice);
        // nrIndex=1 maps to NR2 (EMNR) per the shim's switch table.
        QMetaObject::invokeMethod(&m, "setRxNr",
                                  Q_ARG(int, 0), Q_ARG(bool, true),
                                  Q_ARG(int, 1));
        QCOMPARE(slice->activeNr(), NrSlot::NR2);
        int idx = -1;
        QMetaObject::invokeMethod(&m, "rxNrIndex",
                                  Q_RETURN_ARG(int, idx),
                                  Q_ARG(int, 0));
        QCOMPARE(idx, 1);
    }

    void af_linear_and_mon_linear_roundtrip() {
        // Thetis TCI volume range is 0..100 linear, NOT 0..32767. From
        // TCIServer.cs:4273-4294 [v2.10.3.15] linearToDbVolume /
        // dbToLinearVolume:
        //   double dbMin = -60f;  double dbMax = 0;
        //   double linearMax = 100f;  double linearMin = 0;
        // The TciProtocol parser (src/core/TciProtocol.cpp:2769-2789)
        // converts the wire-format dB value to 0..100 linear via
        // tciDbToLinearVolume BEFORE calling setAfLinear, so values
        // arriving at this shim are always in 0..100.
        //
        // Earlier versions of this test (commit c78ac725) sent 16384 / 8192
        // because the shim was a pure pass-through cache. Commit 9ae5f135
        // rewired the shim through AudioEngine::setVolume (which expects
        // 0..1.0 float), and the shim correctly clamps inputs to 0..100 to
        // match Thetis. The test is now updated to the Thetis-faithful
        // range and also verifies the clamp for out-of-range inputs.
        RadioModel m;
        int out = 0;

        // In-range round-trip (50 = -30 dB equivalent, mid-slider).
        QMetaObject::invokeMethod(&m, "setAfLinear", Q_ARG(int, 50));
        QMetaObject::invokeMethod(&m, "afLinear",
                                  Q_RETURN_ARG(int, out));
        QCOMPARE(out, 50);

        // Boundary round-trip (full volume).
        QMetaObject::invokeMethod(&m, "setAfLinear", Q_ARG(int, 100));
        QMetaObject::invokeMethod(&m, "afLinear",
                                  Q_RETURN_ARG(int, out));
        QCOMPARE(out, 100);

        // Out-of-range positive clamps to 100 (matches Thetis
        // dbToLinearVolume Math.Min(linearMax, ...)).
        QMetaObject::invokeMethod(&m, "setAfLinear", Q_ARG(int, 16384));
        QMetaObject::invokeMethod(&m, "afLinear",
                                  Q_RETURN_ARG(int, out));
        QCOMPARE(out, 100);

        // Out-of-range negative clamps to 0 (matches Thetis
        // dbToLinearVolume Math.Max(linearMin, ...)).
        QMetaObject::invokeMethod(&m, "setAfLinear", Q_ARG(int, -50));
        QMetaObject::invokeMethod(&m, "afLinear",
                                  Q_RETURN_ARG(int, out));
        QCOMPARE(out, 0);

        // MON volume: same range + clamp semantics (TXAF mirrors AF).
        QMetaObject::invokeMethod(&m, "setMonLinear", Q_ARG(int, 75));
        QMetaObject::invokeMethod(&m, "monLinear",
                                  Q_RETURN_ARG(int, out));
        QCOMPARE(out, 75);

        QMetaObject::invokeMethod(&m, "setMonLinear", Q_ARG(int, 8192));
        QMetaObject::invokeMethod(&m, "monLinear",
                                  Q_RETURN_ARG(int, out));
        QCOMPARE(out, 100);
    }

    void iq_sample_rate_roundtrips() {
        RadioModel m;
        QMetaObject::invokeMethod(&m, "setIqSampleRate", Q_ARG(int, 192000));
        int out = 0;
        QMetaObject::invokeMethod(&m, "iqSampleRate",
                                  Q_RETURN_ARG(int, out));
        QCOMPARE(out, 192000);
    }

    void audio_stream_config_caches_last_value() {
        RadioModel m;
        QMetaObject::invokeMethod(&m, "setAudioSampleRate", Q_ARG(int, 24000));
        QMetaObject::invokeMethod(&m, "setAudioStreamSamples", Q_ARG(int, 1024));
        QMetaObject::invokeMethod(&m, "setAudioStreamChannels", Q_ARG(int, 1));
        int sr = 0, samples = 0, channels = 0;
        QMetaObject::invokeMethod(&m, "audioSampleRate",
                                  Q_RETURN_ARG(int, sr));
        QMetaObject::invokeMethod(&m, "audioStreamSamples",
                                  Q_RETURN_ARG(int, samples));
        QMetaObject::invokeMethod(&m, "audioStreamChannels",
                                  Q_RETURN_ARG(int, channels));
        QCOMPARE(sr, 24000);
        QCOMPARE(samples, 1024);
        QCOMPARE(channels, 1);
    }

    // PR #279 review P1 (2026-05-22): real-client connect was receiving
    // muted volume / iq_samplerate:0 / non-canonical sample type because
    // the prod RadioModel m_tciAfLinear / m_tciMonLinear / m_tciIqSampleRate
    // / m_tciAudioStreamSampleType defaults didn't match the mock golden
    // capture (0/0/0/"Float32" vs 50/50/192000/"float32").  CI passed on
    // the mock but the bench bug only showed up on real clients.  This
    // test exercises the production RadioModel pre-connect to lock in
    // Thetis-faithful defaults.
    void prod_init_burst_defaults_match_thetis_wire_expectations() {
        // Fresh RadioModel, no connection, no slice setup.  This is the
        // exact state TciProtocol::buildInitialRadioStateLines sees the
        // first time a real client connects before any user state has
        // been touched.
        RadioModel m;

        // AF volume range: 0..100 (Thetis TCIServer.cs:4273-4294
        // [v2.10.3.15] linearMin/linearMax). AudioEngine m_masterVolume
        // defaults to 0.5f -> 50 linear, so afLinear() returns 50 in
        // practice.  Asserting > 0 catches any regression that drops
        // the AudioEngine fallback and returns the m_tciAfLinear stub.
        int afLin = -1;
        QMetaObject::invokeMethod(&m, "afLinear", Qt::DirectConnection,
                                  Q_RETURN_ARG(int, afLin));
        QVERIFY2(afLin > 0 && afLin <= 100,
            qPrintable(QString("afLinear=%1, expected (0,100]; "
                "0 emits muted volume:-60.0; on first connect")
                .arg(afLin)));

        // MON volume range identical (Thetis TXAF mirrors AF).
        // TransmitModel m_monitorVolume defaults to 0.5f -> 50 linear.
        int monLin = -1;
        QMetaObject::invokeMethod(&m, "monLinear", Qt::DirectConnection,
                                  Q_RETURN_ARG(int, monLin));
        QVERIFY2(monLin > 0 && monLin <= 100,
            qPrintable(QString("monLinear=%1, expected (0,100]; "
                "0 emits muted mon_volume:-60.0; on first connect")
                .arg(monLin)));

        // audioStreamSampleType wire format is lowercase per Thetis
        // (TCIServer.cs audio_stream sample-type tokens float32/int16/
        // int24/int32 are all lower-case in golden captures).  The
        // mock-driven init-burst tests pass an already-normalized
        // lowercase string; this assertion guarantees prod RadioModel
        // ships the same casing so real clients get the canonical
        // token on first connect.
        QString sampleType;
        QMetaObject::invokeMethod(&m, "audioStreamSampleType",
                                  Qt::DirectConnection,
                                  Q_RETURN_ARG(QString, sampleType));
        QVERIFY2(!sampleType.isEmpty(),
            "audioStreamSampleType empty on fresh RadioModel");
        QCOMPARE(sampleType, sampleType.toLower());

        // iqSampleRate: prefers the live connection rate, falls back to
        // m_tciIqSampleRate.  Pre-connect (no connection wired) it
        // returns the m_tciIqSampleRate stub.  192000 is the canonical
        // HPSDR P2 default (matches SampleRateCatalog::kDefaultSampleRate
        // and the mock golden capture).  0 would emit iq_samplerate:0;
        // which real clients reject.
        int iqRate = -1;
        QMetaObject::invokeMethod(&m, "iqSampleRate", Qt::DirectConnection,
                                  Q_RETURN_ARG(int, iqRate));
        QVERIFY2(iqRate > 0,
            qPrintable(QString("iqSampleRate=%1, expected > 0; "
                "0 emits iq_samplerate:0; which real TCI clients reject")
                .arg(iqRate)));
    }

    void stub_dsp_toggles_roundtrip() {
        RadioModel m;
        setupOneSlice(m);
        // setRxBin / setRxApf / setRxNf / setRxEnable / setRxCtun / setRxAnf
        // all share the same per-slice atomic-stub backing storage.
        for (const QByteArray name :
             {"setRxBin", "setRxApf", "setRxNf",
              "setRxEnable", "setRxCtun"})
        {
            QMetaObject::invokeMethod(&m, name.constData(),
                                      Q_ARG(int, 0), Q_ARG(bool, true));
        }
        const QByteArray getters[] = {
            "rxBin", "rxApf", "rxNf", "rxEnable", "rxCtun"
        };
        for (const QByteArray& g : getters) {
            bool out = false;
            QMetaObject::invokeMethod(&m, g.constData(),
                                      Q_RETURN_ARG(bool, out),
                                      Q_ARG(int, 0));
            QVERIFY2(out, g.constData());
        }
    }

    void calibration_getters_return_zero() {
        RadioModel m;
        setupOneSlice(m);
        // No CalibrationModel exists yet; all five getters return 0.0.
        for (const QByteArray name :
             {"calibrationMeter", "calibrationDisplay",
              "calibrationXvtr", "calibrationSixMeter",
              "calibrationTxDisplay"})
        {
            double out = 1.0;  // poison value to verify it gets overwritten
            QMetaObject::invokeMethod(&m, name.constData(),
                                      Q_RETURN_ARG(double, out),
                                      Q_ARG(int, 0));
            QCOMPARE(out, 0.0);
        }
    }

    void out_of_range_slice_silently_no_ops() {
        RadioModel m;
        // No slice added yet; every shim should silently no-op or return
        // a sensible default.  Crash-test only -- no assertion needed
        // beyond "test process didn't segfault".
        QMetaObject::invokeMethod(&m, "setRxMute",
                                  Q_ARG(int, 99), Q_ARG(bool, true));
        QMetaObject::invokeMethod(&m, "setAgcGain",
                                  Q_ARG(int, 99), Q_ARG(int, 42));
        QMetaObject::invokeMethod(&m, "setFilterBand",
                                  Q_ARG(int, -1), Q_ARG(int, 100),
                                  Q_ARG(int, 2900));
        bool b = false;
        int  i = 99;
        QMetaObject::invokeMethod(&m, "rxMute",
                                  Q_RETURN_ARG(bool, b), Q_ARG(int, 99));
        QCOMPARE(b, false);
        QMetaObject::invokeMethod(&m, "agcGain",
                                  Q_RETURN_ARG(int, i), Q_ARG(int, 99));
        QCOMPARE(i, 0);
    }
};

QTEST_MAIN(TestTciRadioModelShims)
#include "tst_tci_radio_model_shims.moc"
