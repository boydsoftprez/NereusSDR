// tests/tst_tci_dispatch_seam.cpp  (NereusSDR)
// NereusSDR-original — no Thetis upstream port in this file.
//
// Phase 3J-1 Task 3.1: TciProtocol dispatch seam.
// Verifies:
//   1. Unknown command -> empty response, 0 notifications (silent-error invariant
//      per design doc §4.1). The command still routes through one dispatch branch.
//   2. `vfo;` (no args) routes to query path (queryDispatchCount increments).
//   3. `vfo:0,0,14250000;` (with args) routes to set path (setDispatchCount increments).
//   4. Empty / semicolon-only input returns empty and increments no counter.
//   5. Trailing semicolon is stripped: `vfo;` and `vfo` route identically.
//
// Phase 3F Sub-Epic J Task 10 adds:
//   6. rx_volume init-burst lines resolve their OWN slice's AF gain through
//      the real dispatch entry point (TciProtocol::buildInitBurst() against
//      a real RadioModel, not the flat TestMockRadioModel -- the mock has no
//      real per-slice state to differentiate, so it cannot exercise this
//      bug). Two slices, two different gains, two different rx_volume
//      lines.
//   7. When a receiver index does not resolve to a live slice,
//      RadioModel::afGain(rx) falls back to the ACTIVE slice rather than a
//      hardcoded receiver 0 -- proven by removing the slice at id 0 so
//      nothing answers to that id anymore, then confirming rx_volume:0
//      still reports the surviving (and now active) slice's real gain
//      instead of the muted-sentinel a "fall back to sliceById(0)" bug
//      would produce.
//
// Tests 6-7 construct a real RadioModel (QTEST_MAIN, not QTEST_GUILESS_MAIN
// -- RadioModel's AudioEngine needs a full QApplication, the same
// requirement tst_tci_radio_model_shims.cpp and tst_audio_engine_antivox_mix
// already carry). Tests 1-5 above are unaffected: they still run against
// the lightweight TestMockRadioModel.

#include <QtTest>
#include <QSignalSpy>
#include <QWebSocket>
#include <algorithm>
#include <utility>
#include "core/TciProtocol.h"
#include "core/TciServer.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "TestMockRadioModel.h"

using namespace NereusSDR;

class TestTciDispatchSeam : public QObject {
    Q_OBJECT
private slots:
    void unknown_command_silent_error_invariant();
    void vfo_no_args_routes_to_query();
    void vfo_with_args_routes_to_set();
    void empty_command_returns_empty();
    void trailing_semicolon_stripped();
    void rx_volume_reports_per_slice_gain_via_dispatch();
    void rx_volume_falls_back_to_active_slice_when_unresolved();
    void existing_and_new_slice_wiring_use_same_stable_receiver_id();
};

void TestTciDispatchSeam::unknown_command_silent_error_invariant()
{
    TestMockRadioModel mock;
    TciProtocol p(&mock);
    const QString r = p.handleCommand(QStringLiteral("nonexistent_command_xyz;"));
    QCOMPARE(r, QString());
    QVERIFY(!p.hasPendingNotification());
    // Silent error invariant: even an unknown command DOES route through
    // handleSetCommand or handleQueryCommand (the case-default-empty path).
    // The "silence" is in the wire output, not the dispatch routing.
    QCOMPARE(p.setDispatchCount() + p.queryDispatchCount(), 1);
}

void TestTciDispatchSeam::vfo_no_args_routes_to_query()
{
    TestMockRadioModel mock;
    TciProtocol p(&mock);
    p.handleCommand(QStringLiteral("vfo;"));
    QCOMPARE(p.queryDispatchCount(), 1);
    QCOMPARE(p.setDispatchCount(), 0);
}

void TestTciDispatchSeam::vfo_with_args_routes_to_set()
{
    TestMockRadioModel mock;
    TciProtocol p(&mock);
    p.handleCommand(QStringLiteral("vfo:0,0,14250000;"));
    QCOMPARE(p.setDispatchCount(), 1);
    QCOMPARE(p.queryDispatchCount(), 0);
}

void TestTciDispatchSeam::empty_command_returns_empty()
{
    TestMockRadioModel mock;
    TciProtocol p(&mock);
    QCOMPARE(p.handleCommand(QString()), QString());
    QCOMPARE(p.handleCommand(QStringLiteral(";")), QString());
    QCOMPARE(p.setDispatchCount(), 0);
    QCOMPARE(p.queryDispatchCount(), 0);
}

void TestTciDispatchSeam::trailing_semicolon_stripped()
{
    TestMockRadioModel mock;
    TciProtocol p(&mock);
    // Both forms should route identically (trailing ';' is stripped per Thetis).
    p.handleCommand(QStringLiteral("vfo;"));
    p.resetDispatchCounters();
    p.handleCommand(QStringLiteral("vfo"));
    QCOMPARE(p.queryDispatchCount(), 1);
}

// Bench bug: TCI rx_volume returned one global afLinear value for every
// slot, so a client asking receiver 1's volume got receiver 0's. The
// model-level fact that SliceModel::afGain()/setAfGain() round-trip per
// slice is not itself a useful regression guard here -- it was already true
// before this fix, since the bug lived entirely in TciProtocol's read path
// (buildInitialRadioStateLines read the radio-global afLinear instead of
// each receiver's own slice). So this test goes straight to the real
// dispatch entry point -- TciProtocol::buildInitBurst() against a real
// RadioModel -- and checks the actual wire lines it produces for two
// different receivers.
void TestTciDispatchSeam::rx_volume_reports_per_slice_gain_via_dispatch()
{
    RadioModel radio;
    radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
    const int a = radio.addSlice(QStringLiteral("pan-0"));
    const int b = radio.addSlice(QStringLiteral("pan-0"));
    QCOMPARE(a, 0);
    QCOMPARE(b, 1);
    radio.sliceById(a)->setAfGain(30);
    radio.sliceById(b)->setAfGain(70);

    TciProtocol p(&radio);
    const QStringList burst = p.buildInitBurst();

    // 30/100 -> 20*log10(0.30) = -10.46 dB; 70/100 -> 20*log10(0.70) = -3.10 dB
    // (tciAudioGainToDb, F2 format -- see TciProtocol.cpp's rx_volume block).
    QVERIFY2(burst.contains(QStringLiteral("rx_volume:0,0,-10.46;")),
             "rx_volume:0 must reflect receiver 0's OWN slice gain (30 -> -10.46 dB)");
    QVERIFY2(burst.contains(QStringLiteral("rx_volume:0,1,-10.46;")),
             "rx_volume:0's second channel mirrors the same slice (no sub-rx model)");
    QVERIFY2(burst.contains(QStringLiteral("rx_volume:1,0,-3.10;")),
             "rx_volume:1 must reflect receiver 1's OWN slice gain (70 -> -3.10 dB), "
             "not receiver 0's (this is the bug: before the fix both read -10.46)");
    QVERIFY2(burst.contains(QStringLiteral("rx_volume:1,1,-3.10;")),
             "rx_volume:1's second channel mirrors the same slice (no sub-rx model)");
}

// The mapping decision (RadioModel.cpp's afGain(int rx)) falls back to the
// ACTIVE slice when sliceById(rx) resolves to nothing, rather than a
// hardcoded receiver 0 or a muted 0 default. Proven here by removing the
// slice at id 0 entirely (leaving nothing to alias onto even if the code
// DID hard-code "0"), then confirming rx_volume:0 still reports the
// surviving slice's real, distinctive gain -- a "return 0 when unresolved"
// or "fall back to sliceById(0)" implementation would instead emit the
// muted sentinel -60.00 here, since neither receiver 0 nor a slice at id 0
// exists anymore.
void TestTciDispatchSeam::rx_volume_falls_back_to_active_slice_when_unresolved()
{
    RadioModel radio;
    radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
    const int a = radio.addSlice(QStringLiteral("pan-0"));  // id 0, active by default
    const int b = radio.addSlice(QStringLiteral("pan-0"));  // id 1
    QCOMPARE(a, 0);
    QCOMPARE(b, 1);
    radio.sliceById(b)->setAfGain(65);

    // Remove id 0. Only slice b (id 1) survives, and RadioModel's removal
    // path reassigns m_activeSlice to the sole survivor automatically.
    radio.removeSlice(a);
    QCOMPARE(radio.slices().size(), 1);
    QCOMPARE(radio.sliceById(a), static_cast<SliceModel*>(nullptr));
    QVERIFY(radio.activeSlice() != nullptr);
    QCOMPARE(radio.activeSlice()->sliceIndex(), b);

    TciProtocol p(&radio);
    const QStringList burst = p.buildInitBurst();

    // 65/100 -> 20*log10(0.65) = -3.74 dB.
    QVERIFY2(burst.contains(QStringLiteral("rx_volume:0,0,-3.74;")),
             "receiver 0 has no slice anymore; afGain(0) must fall back to "
             "the active slice (id 1, gain 65 -> -3.74 dB), not a hardcoded "
             "receiver 0 or a muted -60.00 default");
    QVERIFY2(burst.contains(QStringLiteral("rx_volume:1,0,-3.74;")),
             "receiver 1 resolves its own slice directly (same value here, "
             "for a different reason: direct resolution, not fallback)");
}

void TestTciDispatchSeam::
    existing_and_new_slice_wiring_use_same_stable_receiver_id()
{
    const auto captureVfoFrames =
        [this](TciServer& server, SliceModel* slice, QStringList* captured) {
            QVERIFY(server.start(0));
            QWebSocket client;
            QStringList frames;
            connect(&client, &QWebSocket::textMessageReceived,
                    this, [&frames](const QString& frame) {
                        frames.append(frame);
                    });
            QSignalSpy connectedSpy(&client, &QWebSocket::connected);
            client.open(
                QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
            QVERIFY(connectedSpy.wait(2000));
            QTRY_VERIFY(frames.contains(QStringLiteral("ready;")));

            frames.clear();
            slice->setFrequency(18123456.0);
            QTRY_VERIFY(std::any_of(
                frames.cbegin(), frames.cend(), [](const QString& frame) {
                    return frame.startsWith(QStringLiteral("vfo:"));
                }));
            for (const QString& frame : std::as_const(frames)) {
                if (frame.startsWith(QStringLiteral("vfo:"))
                    || frame.startsWith(QStringLiteral("dds:"))) {
                    captured->append(frame);
                }
            }
            client.close();
            server.stop();
        };

    // Existing-slice path: A/B/C exist first, B is removed, then TCI hooks
    // A(position 0/id 0) and C(position 1/id 2).
    RadioModel existingRadio;
    existingRadio.configureStreamPool(5, 5, 192000);
    existingRadio.addSlice(QStringLiteral("pan-0"));
    const int existingB = existingRadio.addSlice(QStringLiteral("pan-0"));
    const int existingC = existingRadio.addSlice(QStringLiteral("pan-0"));
    existingRadio.removeSlice(existingB);
    SliceModel* const existingSliceC = existingRadio.sliceById(existingC);
    QVERIFY(existingSliceC != nullptr);
    TciServer existingServer(&existingRadio);
    QStringList existingFrames;
    captureVfoFrames(existingServer, existingSliceC, &existingFrames);

    // New-slice path: TCI is already attached when C(id 2) is added, so the
    // RadioModel::sliceAdded stable id drives wiring.
    RadioModel addedRadio;
    addedRadio.configureStreamPool(5, 5, 192000);
    addedRadio.addSlice(QStringLiteral("pan-0"));
    addedRadio.addSlice(QStringLiteral("pan-0"));
    TciServer addedServer(&addedRadio);
    const int addedC = addedRadio.addSlice(QStringLiteral("pan-0"));
    QCOMPARE(addedC, 2);
    SliceModel* const addedSliceC = addedRadio.sliceById(addedC);
    QVERIFY(addedSliceC != nullptr);
    QStringList addedFrames;
    captureVfoFrames(addedServer, addedSliceC, &addedFrames);

    QCOMPARE(existingFrames, addedFrames);
    QVERIFY(existingFrames.contains(QStringLiteral("vfo:2,0,18123456;")));
    QVERIFY(existingFrames.contains(QStringLiteral("vfo:2,1,18123456;")));
    QVERIFY(existingFrames.contains(QStringLiteral("dds:2,18123456;")));
}

QTEST_MAIN(TestTciDispatchSeam)
#include "tst_tci_dispatch_seam.moc"
