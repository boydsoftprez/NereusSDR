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
        QCOMPARE(out, QStringLiteral("FAST"));
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
        RadioModel m;
        QMetaObject::invokeMethod(&m, "setAfLinear", Q_ARG(int, 16384));
        int out = 0;
        QMetaObject::invokeMethod(&m, "afLinear",
                                  Q_RETURN_ARG(int, out));
        QCOMPARE(out, 16384);

        QMetaObject::invokeMethod(&m, "setMonLinear", Q_ARG(int, 8192));
        QMetaObject::invokeMethod(&m, "monLinear",
                                  Q_RETURN_ARG(int, out));
        QCOMPARE(out, 8192);
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
