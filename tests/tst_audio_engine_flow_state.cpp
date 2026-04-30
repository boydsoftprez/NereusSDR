// no-port-check: test-only — exercises AudioEngine::flowStateChanged signal.
// No Thetis logic is ported here; NereusSDR-original.
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/AudioEngine.h"

using namespace NereusSDR;

class TstAudioEngineFlowState : public QObject {
    Q_OBJECT

private slots:
    void initialStateIsDead() {
        AudioEngine engine;
        QCOMPARE(engine.flowState(), AudioEngine::FlowState::Dead);
    }

    void emitsHealthyOnSuccessfulFeed() {
        AudioEngine engine;
        QSignalSpy spy(&engine, &AudioEngine::flowStateChanged);
        engine.simulateSuccessfulFeed();
        QCOMPARE(engine.flowState(), AudioEngine::FlowState::Healthy);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).value<AudioEngine::FlowState>(),
                 AudioEngine::FlowState::Healthy);
    }

    void emitsUnderrunOnUnderrunWatchdog() {
        AudioEngine engine;
        engine.simulateSuccessfulFeed();
        QSignalSpy spy(&engine, &AudioEngine::flowStateChanged);
        engine.simulateUnderrun();
        QCOMPARE(engine.flowState(), AudioEngine::FlowState::Underrun);
        QCOMPARE(spy.count(), 1);
    }

    void emitsStalledAfterPersistentUnderrun() {
        AudioEngine engine;
        engine.simulateSuccessfulFeed();
        engine.simulateUnderrun();
        QSignalSpy spy(&engine, &AudioEngine::flowStateChanged);
        engine.simulatePersistentUnderrun();
        QCOMPARE(engine.flowState(), AudioEngine::FlowState::Stalled);
        QCOMPARE(spy.count(), 1);
    }

    void identicalStateDoesNotReEmit() {
        AudioEngine engine;
        QSignalSpy spy(&engine, &AudioEngine::flowStateChanged);
        engine.simulateSuccessfulFeed();
        engine.simulateSuccessfulFeed();
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TstAudioEngineFlowState)
#include "tst_audio_engine_flow_state.moc"
