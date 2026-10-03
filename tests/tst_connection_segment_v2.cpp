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
//
// Modification history (NereusSDR):
//   2026-10-01  J.J. Boyd / KG4VCF  The radio-offline audio group is checked
//                                    for its exact wording, "Radio offline".
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QColor>
#include <QPixmap>
#include <QLabel>
#include <QMenuBar>
#include <QPushButton>
#include <QSignalSpy>

#include "core/AudioEngine.h"
#include "gui/TitleBar.h"
#include "core/session/NetworkPathSnapshot.h"

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

    void remoteTelemetryRendersWithStatusAndRemainsClickable() {
        ConnectionSegment seg;
        seg.setRemoteStatusText(QStringLiteral("Core connected"));
        const int statusWidth = seg.sizeHint().width();

        seg.setRemoteTelemetryText(
            QStringLiteral("Radio ↓12.4 ↑0.8 Mbps  ·  Core 18 ms  ·  Audio playing"));
        QVERIFY(seg.sizeHint().width() > statusWidth);
        QCOMPARE(seg.remoteTelemetryText(),
                 QStringLiteral("Radio ↓12.4 ↑0.8 Mbps  ·  Core 18 ms  ·  Audio playing"));
        QCOMPARE(seg.accessibleName(),
                 QStringLiteral("Core connected  ·  Radio ↓12.4 ↑0.8 Mbps  ·  Core 18 ms  ·  Audio playing"));

        seg.resize(seg.sizeHint());
        QPixmap rendered(seg.size());
        rendered.fill(Qt::magenta);
        seg.render(&rendered);
        QVERIFY(!rendered.isNull());
        QCOMPARE(rendered.toImage().pixelColor(0, seg.height() / 2), QColor("#0f1420"));

        QSignalSpy clicked(&seg, &ConnectionSegment::rttClicked);
        QTest::mouseClick(&seg, Qt::LeftButton, Qt::NoModifier,
                          QPoint(seg.width() / 2, seg.height() / 2));
        QCOMPARE(clicked.count(), 1);
    }

    void routePresentationUsesObservedEndpointMeaning() {
        NetworkPathSnapshot path;
        path.kind = NetworkPathSnapshot::Kind::Direct;
        path.carrier = NetworkPathSnapshot::Carrier::Ice;
        path.endpoints = NetworkPathSnapshot::Endpoints::IceCandidates;
        path.localAddress = QStringLiteral("2001:db8::1");
        path.localPort = 1234;
        path.remoteAddress = QStringLiteral("192.0.2.8");
        path.remotePort = 4321;
        path.localCandidateType = QStringLiteral("host");
        path.remoteCandidateType = QStringLiteral("srflx");
        const QString direct = ConnectionSegment::routeText(path);
        QVERIFY(direct.contains(QStringLiteral("Direct")));
        QVERIFY(direct.contains(QStringLiteral("ICE candidates")));
        QVERIFY(direct.contains(QStringLiteral("[2001:db8::1]:1234")));
        QVERIFY(direct.contains(QStringLiteral("192.0.2.8:4321")));
        QVERIFY(direct.contains(QStringLiteral("host")));

        path.kind = NetworkPathSnapshot::Kind::Relayed;
        path.carrier = NetworkPathSnapshot::Carrier::WebRelay;
        path.endpoints = NetworkPathSnapshot::Endpoints::Socket;
        path.localPort = 0;
        path.remoteAddress.clear();
        const QString relay = ConnectionSegment::routeText(path);
        QVERIFY(relay.contains(QStringLiteral("Via relay")));
        QVERIFY(relay.contains(QStringLiteral("socket")));
        QVERIFY(relay.contains(QStringLiteral("unavailable")));
        QVERIFY(!relay.contains(QStringLiteral("127.0.0.1")));
        path.mediaRidesControl = true;
        path.localAddress = QStringLiteral("192.0.2.3");
        path.localPort = 5000;
        path.remoteAddress = QStringLiteral("2001:db8::4");
        path.remotePort = 443;
        const QString tunnel = ConnectionSegment::routeText(path);
        QVERIFY(tunnel.contains(QStringLiteral("Uses the control connection")));
        QVERIFY(tunnel.contains(QStringLiteral("[2001:db8::4]:443")));
        QVERIFY(tunnel.contains(QStringLiteral("192.0.2.3:5000")));
        QCOMPARE(ConnectionSegment::routeText(std::nullopt), QStringLiteral("Path unavailable"));
    }

    void remotePopupUpdatesAndClears() {
        ConnectionSegment seg;
        seg.setState(ConnectionState::Connected);
        seg.setRemoteStatusText(QStringLiteral("Core connected"));
        seg.setRemoteMetrics({QStringLiteral("Traffic ↓1 ↑0 kbps"),
                              QStringLiteral("Audio 96 kbps"),
                              QStringLiteral("Radio ↓12 ↑1 Mbps"),
                              QStringLiteral("RTT 0 ms")});
        QVERIFY(seg.remotePresentationText().contains(QStringLiteral("RTT 0 ms")));
        NetworkPathSnapshot path;
        path.kind = NetworkPathSnapshot::Kind::Direct;
        path.carrier = NetworkPathSnapshot::Carrier::WebSocket;
        path.endpoints = NetworkPathSnapshot::Endpoints::Socket;
        path.remoteAddress = QStringLiteral("192.0.2.1");
        path.remotePort = 443;
        seg.setRemotePaths(path, std::nullopt);
        seg.showRoutePopup();
        QVERIFY(seg.routePopup()->isVisible());
        QVERIFY(seg.routePopup()->findChild<QLabel*>(QStringLiteral("controlsRoute"))->text()
                    .contains(QStringLiteral("192.0.2.1:443")));
        path.remoteAddress = QStringLiteral("2001:db8::7");
        path.remotePort = 8443;
        seg.setRemotePaths(path, std::nullopt);
        const QString replaced = seg.routePopup()->findChild<QLabel*>(QStringLiteral("controlsRoute"))->text();
        QVERIFY(replaced.contains(QStringLiteral("[2001:db8::7]:8443")));
        QVERIFY(!replaced.contains(QStringLiteral("192.0.2.1:443")));
        seg.setRemotePaths(std::nullopt, std::nullopt);
        QVERIFY(seg.routePopup()->findChild<QLabel*>(QStringLiteral("controlsRoute"))->text()
                    .contains(QStringLiteral("unavailable")));
        seg.setState(ConnectionState::Disconnected);
        QVERIFY(!seg.routePopup()->isVisible());
    }

    void desktopHeaderKeepsFourGroupsInOneRow() {
        AudioEngine engine;
        TitleBar bar(&engine);
        auto* menu = new QMenuBar(&bar);
        menu->addMenu(QStringLiteral("Radio"));
        menu->addMenu(QStringLiteral("Setup"));
        menu->addMenu(QStringLiteral("Help"));
        bar.setMenuBar(menu);
        bar.resize(1440, 32);
        auto* seg = bar.connectionSegment();
        seg->setState(ConnectionState::Connected);
        seg->setRemoteStatusText(QStringLiteral("Core connected"));
        seg->setRemoteMetrics({QStringLiteral("Traffic ↓12.4 ↑0.8 Mbps"),
                               QStringLiteral("Audio 96.0 kbps (playing)"),
                               QStringLiteral("Radio ↓12.4 ↑0.8 Mbps"),
                               QStringLiteral("Core RTT 18 ms")});
        bar.show();
        QApplication::processEvents();
        QFontMetrics metrics(QFont(QStringLiteral("SF Mono"), 10, QFont::DemiBold));
        QVERIFY2(seg->width() >= metrics.horizontalAdvance(seg->remotePresentationText()) + 34,
                 qPrintable(QStringLiteral("segment %1, text %2")
                                .arg(seg->width()).arg(metrics.horizontalAdvance(seg->remotePresentationText()))));
        QCOMPARE(seg->height(), 30);
        QPixmap image(bar.size());
        bar.render(&image);
        QVERIFY(!image.isNull());
        seg->resize(230, 30);
        const QString narrowText = seg->remoteTextForWidth(230 - 34);
        QVERIFY(narrowText.contains(QStringLiteral("Traffic")));
        QVERIFY(!narrowText.contains(QStringLiteral("Radio")));
        QVERIFY(metrics.horizontalAdvance(narrowText) <= 230 - 34);
        QPixmap narrow(seg->size());
        seg->render(&narrow);
        QVERIFY(!narrow.isNull());
    }

    void keyboardOpensAndDismissesPopup() {
        ConnectionSegment seg;
        seg.setState(ConnectionState::Connected);
        seg.setRemoteStatusText(QStringLiteral("Core connected"));
        seg.show();
        QTest::keyClick(&seg, Qt::Key_Return);
        QVERIFY(seg.routePopup()->isVisible());
        QTest::keyClick(seg.routePopup(), Qt::Key_Escape);
        QTRY_VERIFY(!seg.routePopup()->isVisible());
        QTest::keyClick(&seg, Qt::Key_Space);
        QVERIFY(seg.routePopup()->isVisible());
        QSignalSpy diagnostics(&seg, &ConnectionSegment::diagnosticsRequested);
        auto* button = seg.routePopup()->findChild<QPushButton*>(QStringLiteral("networkDiagnosticsButton"));
        QVERIFY(button);
        button->click();
        QCOMPARE(diagnostics.count(), 1);
    }

    void audioMetricKeepsPersistentStatesDistinct() {
        using State = RemoteAudioStatus::State;
        QCOMPARE(ConnectionSegment::audioMetricText(0.0, State::MutedHere),
                 QStringLiteral("Audio muted"));
        QCOMPARE(ConnectionSegment::audioMetricText(std::nullopt, State::CoreCouldNotStart),
                 QStringLiteral("Audio unavailable"));
        QCOMPARE(ConnectionSegment::audioMetricText(std::nullopt, State::PlaybackProblem),
                 QStringLiteral("Audio unavailable"));
        QCOMPARE(ConnectionSegment::audioMetricText(std::nullopt, State::RadioOffline),
                 QStringLiteral("Radio offline"));
    }
};

QTEST_MAIN(TstConnectionSegmentV2)
#include "tst_connection_segment_v2.moc"
