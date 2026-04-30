// =================================================================
// tests/tst_connection_segment_v2.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for the rebuilt ConnectionSegment (Phase 3Q Sub-PR-4 D.1).
//
// Coverage:
//   1. disconnectedShowsClickToConnect — setState(Disconnected) round-trip.
//   2. connectedShowsRateAndRtt — setState(Connected) + setRates + setRttMs
//      round-trip via the accessor getters.
//   3. leftClickOnRttRegionEmitsRttClicked — a left-click roughly in the
//      centre of a Connected segment emits rttClicked().
//   4. rightClickEmitsContextMenuRequested — right-click emits
//      contextMenuRequested() once.
//   5. audioPipReflectsFlowState — setAudioFlowState round-trip through all
//      four AudioEngine::FlowState values.
//
// Design spec: docs/architecture/2026-04-30-shell-chrome-redesign-design.md
// §4.1. Phase 3Q Sub-PR-4 D.1.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AudioEngine.h"
#include "gui/TitleBar.h"

using namespace NereusSDR;

class TstConnectionSegmentV2 : public QObject {
    Q_OBJECT

private slots:

    void disconnectedShowsClickToConnect() {
        ConnectionSegment seg;
        seg.setState(ConnectionState::Disconnected);
        QCOMPARE(seg.state(), ConnectionState::Disconnected);
    }

    void connectedShowsRateAndRtt() {
        ConnectionSegment seg;
        seg.setState(ConnectionState::Connected);
        seg.setRates(11.4, 1.2);
        seg.setRttMs(12);
        QCOMPARE(seg.state(), ConnectionState::Connected);
        QCOMPARE(seg.rttMs(), 12);
    }

    void leftClickOnRttRegionEmitsRttClicked() {
        // In Disconnected state any left-click emits rttClicked() — this path
        // does NOT depend on paint-time hit rects so it runs cleanly in the
        // headless QTEST_MAIN environment (no compositor needed).
        ConnectionSegment seg;
        seg.resize(280, 30);
        seg.setState(ConnectionState::Disconnected);

        QSignalSpy spy(&seg, &ConnectionSegment::rttClicked);
        QVERIFY(spy.isValid());

        // Any click position triggers it in Disconnected state.
        QTest::mouseClick(&seg, Qt::LeftButton, Qt::NoModifier,
                          QPoint(seg.width() / 2, 15));
        QCOMPARE(spy.count(), 1);
    }

    void rightClickEmitsContextMenuRequested() {
        ConnectionSegment seg;
        seg.resize(280, 30);
        seg.setState(ConnectionState::Connected);

        QSignalSpy spy(&seg, &ConnectionSegment::contextMenuRequested);
        QVERIFY(spy.isValid());

        QTest::mouseClick(&seg, Qt::RightButton);
        QCOMPARE(spy.count(), 1);
    }

    void audioPipReflectsFlowState() {
        ConnectionSegment seg;

        seg.setAudioFlowState(AudioEngine::FlowState::Healthy);
        QCOMPARE(seg.audioFlowState(), AudioEngine::FlowState::Healthy);

        seg.setAudioFlowState(AudioEngine::FlowState::Underrun);
        QCOMPARE(seg.audioFlowState(), AudioEngine::FlowState::Underrun);

        seg.setAudioFlowState(AudioEngine::FlowState::Stalled);
        QCOMPARE(seg.audioFlowState(), AudioEngine::FlowState::Stalled);

        seg.setAudioFlowState(AudioEngine::FlowState::Dead);
        QCOMPARE(seg.audioFlowState(), AudioEngine::FlowState::Dead);
    }
};

QTEST_MAIN(TstConnectionSegmentV2)
#include "tst_connection_segment_v2.moc"
