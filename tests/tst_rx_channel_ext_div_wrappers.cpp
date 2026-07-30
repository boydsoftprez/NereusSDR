// =================================================================
// tests/tst_rx_channel_ext_div_wrappers.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original lifecycle test infrastructure.
//
// WDSP external diversity is not an RXA-channel property. It lives in the
// separate two-slot pdiv[] table in div.c and therefore belongs to the
// engine that owns WDSP process-wide state. This target keeps its historical
// name to avoid unrelated CMake churn while testing the WdspEngine contract.
// =================================================================

#include <QtTest/QtTest>

#include "core/WdspEngine.h"

using namespace NereusSDR;

class TestWdspEngineExternalDiversity : public QObject {
    Q_OBJECT

    static inline QStringList* s_calls = nullptr;

    static void recordCreate(int id, int run, int nr, int size)
    {
        s_calls->append(QStringLiteral("create:%1:%2:%3:%4")
                            .arg(id).arg(run).arg(nr).arg(size));
    }

    static void recordDestroy(int id)
    {
        s_calls->append(QStringLiteral("destroy:%1").arg(id));
    }

    static void recordProcess(int id, int samples, double**, double* output)
    {
        s_calls->append(QStringLiteral("process:%1:%2").arg(id).arg(samples));
        if (output && samples > 0) {
            output[0] = 123.0;
        }
    }

    static void recordRun(int id, int run)
    {
        s_calls->append(QStringLiteral("run:%1:%2").arg(id).arg(run));
    }

    static void recordNr(int id, int nr)
    {
        s_calls->append(QStringLiteral("nr:%1:%2").arg(id).arg(nr));
    }

    static void recordOutput(int id, int output)
    {
        s_calls->append(QStringLiteral("output:%1:%2").arg(id).arg(output));
    }

    static void recordRotate(int id, int nr, double*, double*)
    {
        s_calls->append(QStringLiteral("rotate:%1:%2").arg(id).arg(nr));
    }

    static WdspEngine::ExternalDiversityApiForTest recordingApi()
    {
        return {
            &recordCreate,
            &recordDestroy,
            &recordProcess,
            &recordRun,
            &recordNr,
            &recordOutput,
            &recordRotate,
        };
    }

private slots:
    void configure_before_create_is_a_noop()
    {
        QStringList calls;
        s_calls = &calls;
        WdspEngine engine;
        engine.setExternalDiversityApiForTest(recordingApi());

        double iRotate[2] = {1.0, 1.0};
        double qRotate[2] = {0.0, 0.0};
        double input0[8]{};
        double input1[8]{};
        double* inputs[2] = {input0, input1};
        double output[8]{};

        engine.configureExternalDiversity(0, 2, iRotate, qRotate, 2);
        engine.setExternalDiversityRunning(0, true);
        QVERIFY(!engine.processExternalDiversity(0, 4, inputs, output));
        engine.destroyExternalDiversity(0);

        QVERIFY(calls.isEmpty());
    }

    void create_configure_run_process_stop_destroy_are_ordered()
    {
        QStringList calls;
        s_calls = &calls;
        WdspEngine engine;
        engine.setExternalDiversityApiForTest(recordingApi());

        double iRotate[2] = {1.0, 0.5};
        double qRotate[2] = {0.0, 0.25};
        double input0[8]{};
        double input1[8]{};
        double* inputs[2] = {input0, input1};
        double output[8]{};

        QVERIFY(engine.createExternalDiversity(0, 2, 4));
        engine.configureExternalDiversity(0, 2, iRotate, qRotate, 2);
        engine.setExternalDiversityRunning(0, true);
        QVERIFY(engine.processExternalDiversity(0, 4, inputs, output));
        QCOMPARE(output[0], 123.0);
        engine.setExternalDiversityRunning(0, false);
        engine.destroyExternalDiversity(0);

        QCOMPARE(calls, QStringList({
            QStringLiteral("create:0:0:2:4"),
            QStringLiteral("nr:0:2"),
            QStringLiteral("output:0:2"),
            QStringLiteral("rotate:0:2"),
            QStringLiteral("run:0:1"),
            QStringLiteral("process:0:4"),
            QStringLiteral("run:0:0"),
            QStringLiteral("destroy:0"),
        }));
    }

    void process_requires_created_and_running_state()
    {
        QStringList calls;
        s_calls = &calls;
        WdspEngine engine;
        engine.setExternalDiversityApiForTest(recordingApi());

        double input0[4]{};
        double input1[4]{};
        double* inputs[2] = {input0, input1};
        double output[4]{};

        QVERIFY(!engine.processExternalDiversity(1, 2, inputs, output));
        QVERIFY(engine.createExternalDiversity(1, 2, 2));
        QVERIFY(!engine.processExternalDiversity(1, 2, inputs, output));
        engine.setExternalDiversityRunning(1, true);
        QVERIFY(engine.processExternalDiversity(1, 2, inputs, output));
        engine.setExternalDiversityRunning(1, false);
        QVERIFY(!engine.processExternalDiversity(1, 2, inputs, output));
        engine.destroyExternalDiversity(1);

        QCOMPARE(calls.count(QStringLiteral("process:1:2")), 1);
    }

    void repeated_disable_shutdown_and_destructor_destroy_once()
    {
        QStringList calls;
        s_calls = &calls;
        {
            WdspEngine engine;
            engine.setExternalDiversityApiForTest(recordingApi());
            QVERIFY(engine.createExternalDiversity(0, 2, 4));
            engine.setExternalDiversityRunning(0, true);
            engine.setExternalDiversityRunning(0, false);
            engine.setExternalDiversityRunning(0, false);
            engine.destroyExternalDiversity(0);
            engine.destroyExternalDiversity(0);
            engine.shutdown();
        }

        QCOMPARE(calls.count(QStringLiteral("run:0:0")), 1);
        QCOMPARE(calls.count(QStringLiteral("destroy:0")), 1);
        QVERIFY(calls.indexOf(QStringLiteral("run:0:0"))
                < calls.indexOf(QStringLiteral("destroy:0")));
    }

    void destructor_stops_then_destroys_a_live_slot()
    {
        QStringList calls;
        s_calls = &calls;
        {
            WdspEngine engine;
            engine.setExternalDiversityApiForTest(recordingApi());
            QVERIFY(engine.createExternalDiversity(1, 2, 4));
            engine.setExternalDiversityRunning(1, true);
        }

        QCOMPARE(calls, QStringList({
            QStringLiteral("create:1:0:2:4"),
            QStringLiteral("run:1:1"),
            QStringLiteral("run:1:0"),
            QStringLiteral("destroy:1"),
        }));
    }

    void ids_outside_the_two_wdsp_slots_are_rejected()
    {
        QStringList calls;
        s_calls = &calls;
        WdspEngine engine;
        engine.setExternalDiversityApiForTest(recordingApi());

        QVERIFY(!engine.createExternalDiversity(-1, 2, 4));
        QVERIFY(!engine.createExternalDiversity(2, 2, 4));
        engine.setExternalDiversityRunning(-1, true);
        engine.setExternalDiversityRunning(2, true);
        engine.destroyExternalDiversity(-1);
        engine.destroyExternalDiversity(2);

        QVERIFY(calls.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestWdspEngineExternalDiversity)
#include "tst_rx_channel_ext_div_wrappers.moc"
