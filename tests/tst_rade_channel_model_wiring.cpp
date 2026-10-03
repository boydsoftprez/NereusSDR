// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - tst_rade_channel_model_wiring: tests for Phase 3R Task I5.
//
// I5 connects the standalone RadeChannel (I1-I4) into the rest of
// NereusSDR's slot graph:
//
//   RadeChannel::snrChanged(float)
//     -> RadioModel::onRadeSnrChanged(int sliceId, float snrDb)
//     -> SliceModel::setSnrDb(double)
//     -> RadioModel::radeSnrChanged(sliceId, snrDb) re-emit
//
//   RadeChannel::syncChanged(bool)
//     -> RadioModel::onRadeSyncChanged(int sliceId, bool synced)
//     -> RadioModel::radeSyncChanged(sliceId, synced) (only on transition)
//
//   RadeChannel::rxTextDecoded(QString callsign, QString grid)
//     -> RadioModel::onRadeTextDecoded(int sliceId, callsign, grid)
//     -> RxDecodeModel::addDecode({callsign, mode=RADE, source=rade_text, ...})
//
// The wiring helper RadioModel::wireRadeChannel(sliceId, channel, slice)
// adapts the RadeChannel's per-channel signals through captured-sliceId
// lambdas so the RadioModel slots can address the right slice. The
// channel signals do not carry the slice ID; we attach it at wire time.
//
// Tests use a small RadeChannel test subclass that exposes
// emitSnrChangedForTest / emitSyncChangedForTest / emitRxTextDecodedForTest
// helpers so we can drive the signal layer without a live RADE codec
// (which would require driving the I2/I3 RX/TX pipelines end-to-end).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3R Task I5. Initial test
//                 file. NereusSDR-native: no upstream port. The slot
//                 contract under test was established by the I5 plan
//                 spec + the existing I1 RadeChannel signal surface.
//                 AI tooling: Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  RADE status for the phone's VFO
//                 flag: the slice's radeSynced and radeFreqOffsetHz follow
//                 the channel, clear when the channel goes and when the
//                 slice leaves RADE, are read-only and outbound, and go
//                 only to a peer that declared radeStatus. AI tooling:
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/FreeDVRadeReporterBridge.h"
#include "core/RadeChannel.h"
#include "core/session/MirrorPolicy.h"
#include "gui/MainWindow.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/RxDecodeModel.h"
#include "models/SliceModel.h"

#include <QMetaProperty>

#include <cmath>
#include <memory>

using namespace NereusSDR;

namespace {

// Test-only RadeChannel subclass that exposes the three emission seams.
// The base class's signals are protected-by-Qt-default (any member can
// emit), but emission from outside the class hierarchy is undefined
// behaviour. The conventional fix is a friend test subclass that exposes
// thin emission wrappers.
class TestableRadeChannel : public RadeChannel {
    Q_OBJECT
public:
    using RadeChannel::RadeChannel;

    void emitSnrChangedForTest(float snrDb) {
        emit snrChanged(snrDb);
    }
    void emitSyncChangedForTest(bool synced) {
        emit syncChanged(synced);
    }
    void emitFreqOffsetChangedForTest(float hz) {
        emit freqOffsetChanged(hz);
    }
    void emitRxTextDecodedForTest(const QString& callsign,
                                  const QString& grid) {
        emit rxTextDecoded(callsign, grid);
    }
    // Phase 3R K4: TX modem output emission seam.
    void emitTxModemReadyForTest(const QByteArray& iq) {
        emit txModemReady(iq);
    }
    // Phase 3R K4: expose isSignalConnected (protected on QObject)
    // so the test can verify wireRadeChannel actually wired the
    // txModemReady signal.
    bool isTxModemReadyConnectedForTest() const {
        return isSignalConnected(
            QMetaMethod::fromSignal(&RadeChannel::txModemReady));
    }
};

}  // namespace

class TestRadeChannelModelWiring : public QObject {
    Q_OBJECT

private slots:
    void wireRadeChannelConnectsSnrToSlice();
    void wireRadeChannelEmitsRadioModelSync();
    void repeatedSyncEmitsOnlyOnceUntilChange();
    void rxTextDecodedAddsRxDecodeRow();
    void rxTextWithEmptyGridStoresCallsignOnly();
    void wiringWithNullChannelIsNoOp();
    // Phase 3R K4: TX modem output plumbing.
    void txModemReadyEmissionDoesNotCrash();
    // RADE status on the slice for the phone's VFO flag.
    void sliceRadeSyncedFollowsChannel();
    void sliceRadeSyncedClearsWhenChannelGoes();
    void newDecoderFirstLockReachesFlagAndReporter();
    void sliceRadeFreqOffsetFollowsChannel();
    void sliceRadeSyncedClearsOnLeavingRade();
    void radeStatusPropertiesAreReadOnlyAndGated();
};

void TestRadeChannelModelWiring::wireRadeChannelConnectsSnrToSlice()
{
    RadioModel model;
    const int sliceId = model.addSlice();
    QVERIFY(sliceId >= 0);
    SliceModel* slice = model.sliceById(sliceId);
    QVERIFY(slice != nullptr);

    TestableRadeChannel channel;
    model.wireRadeChannel(sliceId, &channel, slice);

    QSignalSpy snrSpy(&model, &RadioModel::radeSnrChanged);

    channel.emitSnrChangedForTest(5.0f);

    // Slice snrDb forwarded with cast-up to double; expect ~5.0.
    QVERIFY2(std::abs(slice->snrDb() - 5.0) < 1e-4,
             qPrintable(QString("slice->snrDb() = %1, expected ~5.0")
                            .arg(slice->snrDb())));

    // RadioModel::radeSnrChanged(sliceId, snrDb) fired once.
    QCOMPARE(snrSpy.count(), 1);
    QCOMPARE(snrSpy.at(0).at(0).toInt(), sliceId);
    QVERIFY2(std::abs(snrSpy.at(0).at(1).toFloat() - 5.0f) < 1e-4f,
             "radeSnrChanged carried wrong SNR value");
}

void TestRadeChannelModelWiring::wireRadeChannelEmitsRadioModelSync()
{
    RadioModel model;
    const int sliceId = model.addSlice();
    SliceModel* slice = model.sliceById(sliceId);
    QVERIFY(slice != nullptr);

    TestableRadeChannel channel;
    model.wireRadeChannel(sliceId, &channel, slice);

    QSignalSpy syncSpy(&model, &RadioModel::radeSyncChanged);

    QVERIFY(!model.radeSynced(sliceId));
    channel.emitSyncChangedForTest(true);

    QVERIFY(model.radeSynced(sliceId));
    QCOMPARE(syncSpy.count(), 1);
    QCOMPARE(syncSpy.at(0).at(0).toInt(), sliceId);
    QCOMPARE(syncSpy.at(0).at(1).toBool(), true);
}

void TestRadeChannelModelWiring::repeatedSyncEmitsOnlyOnceUntilChange()
{
    RadioModel model;
    const int sliceId = model.addSlice();
    SliceModel* slice = model.sliceById(sliceId);
    QVERIFY(slice != nullptr);

    TestableRadeChannel channel;
    model.wireRadeChannel(sliceId, &channel, slice);

    QSignalSpy syncSpy(&model, &RadioModel::radeSyncChanged);

    // Three sync=true emissions should collapse to a single transition.
    channel.emitSyncChangedForTest(true);
    channel.emitSyncChangedForTest(true);
    channel.emitSyncChangedForTest(true);

    QCOMPARE(syncSpy.count(), 1);
    QVERIFY(model.radeSynced(sliceId));

    // Flipping back fires once.
    channel.emitSyncChangedForTest(false);
    QCOMPARE(syncSpy.count(), 2);
    QVERIFY(!model.radeSynced(sliceId));
}

void TestRadeChannelModelWiring::rxTextDecodedAddsRxDecodeRow()
{
    RadioModel model;
    const int sliceId = model.addSlice();
    SliceModel* slice = model.sliceById(sliceId);
    QVERIFY(slice != nullptr);
    slice->setFrequency(14225000.0);  // 14.225 MHz

    RxDecodeModel* decodes = model.rxDecodeModel();
    QVERIFY(decodes != nullptr);
    QCOMPARE(decodes->decodes().size(), 0);

    TestableRadeChannel channel;
    model.wireRadeChannel(sliceId, &channel, slice);

    channel.emitRxTextDecodedForTest(QStringLiteral("KG4VCF"),
                                     QStringLiteral("EM85"));

    QCOMPARE(decodes->decodes().size(), 1);
    const RxDecode& row = decodes->decodes().at(0);
    QCOMPARE(row.callsign, QStringLiteral("KG4VCF"));
    QCOMPARE(row.mode,     QStringLiteral("RADE"));
    QCOMPARE(row.source,   QStringLiteral("rade_text"));
    QCOMPARE(row.payload,  QStringLiteral("KG4VCF EM85"));
    QVERIFY2(std::abs(row.freqMhz - 14.225) < 1e-6,
             qPrintable(QString("row.freqMhz = %1, expected ~14.225")
                            .arg(row.freqMhz)));
    QVERIFY(row.utcTime.isValid());
}

void TestRadeChannelModelWiring::rxTextWithEmptyGridStoresCallsignOnly()
{
    RadioModel model;
    const int sliceId = model.addSlice();
    SliceModel* slice = model.sliceById(sliceId);
    QVERIFY(slice != nullptr);

    RxDecodeModel* decodes = model.rxDecodeModel();
    QVERIFY(decodes != nullptr);

    TestableRadeChannel channel;
    model.wireRadeChannel(sliceId, &channel, slice);

    channel.emitRxTextDecodedForTest(QStringLiteral("KG4VCF"),
                                     QString());

    QCOMPARE(decodes->decodes().size(), 1);
    const RxDecode& row = decodes->decodes().at(0);
    QCOMPARE(row.callsign, QStringLiteral("KG4VCF"));
    QCOMPARE(row.payload,  QStringLiteral("KG4VCF"));
}

void TestRadeChannelModelWiring::wiringWithNullChannelIsNoOp()
{
    RadioModel model;
    const int sliceId = model.addSlice();
    SliceModel* slice = model.sliceById(sliceId);
    QVERIFY(slice != nullptr);

    // Null channel: must not crash, must not emit anything.
    model.wireRadeChannel(sliceId, nullptr, slice);

    // Null slice with a valid channel: must not crash either. Slot bodies
    // look the slice up via sliceById(sliceId) at signal time, so a null
    // slice pointer at wire time is harmless as long as the helper does
    // not dereference it.
    TestableRadeChannel channel;
    model.wireRadeChannel(sliceId, &channel, nullptr);
}

// Phase 3R K4: txModemReady emission must reach the RadioModel hook
// without crashing AND the wire helper must actually establish the
// connect (verified via Qt's signal-receiver count via a transient
// blocker).  The hook currently logs once and Q_UNUSEDs the bytes
// (full I/Q TX routing is K-bench scope); this test pins both the
// "doesn't crash" contract and the "connect exists" contract so a
// future bench fill-in can build on a stable signal graph.
void TestRadeChannelModelWiring::txModemReadyEmissionDoesNotCrash()
{
    RadioModel model;
    const int sliceId = model.addSlice();
    SliceModel* slice = model.sliceById(sliceId);
    QVERIFY(slice != nullptr);

    TestableRadeChannel channel;

    // Snapshot the channel's connection count before wire.  Qt's
    // isSignalConnected() returns true once any slot listens.
    // Before wireRadeChannel, nothing is connected; after, at least
    // the K4 lambda is.  Both checks go through a thin test-only
    // friend accessor on TestableRadeChannel because
    // QObject::isSignalConnected is protected.
    QVERIFY2(!channel.isTxModemReadyConnectedForTest(),
             "txModemReady should have no listeners before wireRadeChannel");

    model.wireRadeChannel(sliceId, &channel, slice);

    QVERIFY2(channel.isTxModemReadyConnectedForTest(),
             "wireRadeChannel must connect a listener to txModemReady (K4)");

    // Emit a 480-frame stereo Int16 payload (480 * 4 = 1920 bytes;
    // matches the 24 kHz stereo Int16 shape documented at
    // RadeChannel.h:250-252 [Phase 3R I1]).
    QByteArray iq(1920, '\0');
    channel.emitTxModemReadyForTest(iq);

    // Empty payload: hook must still no-op cleanly.
    channel.emitTxModemReadyForTest(QByteArray());

    // If we reached this line without crashing, the connect plumbing
    // is in place.  The full I/Q route to RadioConnection::sendTxIq
    // lands at K-bench time.
    QVERIFY(true);
}

// The desktop flag shows the filled sync dot while the decoder reports
// sync and the hollow one when it loses it (VfoWidget::setRadeSynced);
// the slice carries the same decoder state for a remote flag.
void TestRadeChannelModelWiring::sliceRadeSyncedFollowsChannel()
{
    RadioModel model;
    const int sliceId = model.addSlice();
    SliceModel* slice = model.sliceById(sliceId);
    QVERIFY(slice != nullptr);
    QCOMPARE(slice->radeSynced(), false);

    TestableRadeChannel channel;
    model.wireRadeChannel(sliceId, &channel, slice);
    QSignalSpy spy(slice, &SliceModel::radeSyncedChanged);

    channel.emitSyncChangedForTest(true);
    QCOMPARE(slice->radeSynced(), true);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), true);

    channel.emitSyncChangedForTest(true);
    QCOMPARE(spy.count(), 1);

    channel.emitSyncChangedForTest(false);
    QCOMPARE(slice->radeSynced(), false);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toBool(), false);
}

// A new channel starts unsynced (RadeChannel::m_synced false) and reports
// its own first lock, whatever the previous channel last said.
void TestRadeChannelModelWiring::sliceRadeSyncedClearsWhenChannelGoes()
{
    RadioModel model;
    const int sliceId = model.addSlice();
    SliceModel* slice = model.sliceById(sliceId);
    QVERIFY(slice != nullptr);

    auto first = std::make_unique<TestableRadeChannel>();
    model.wireRadeChannel(sliceId, first.get(), slice);
    first->emitSyncChangedForTest(true);
    QCOMPARE(slice->radeSynced(), true);

    first.reset();
    QCOMPARE(slice->radeSynced(), false);

    TestableRadeChannel second;
    model.wireRadeChannel(sliceId, &second, slice);
    second.emitSyncChangedForTest(true);
    QCOMPARE(slice->radeSynced(), true);
}

// A decoder that closes while locked must not leave the slice recorded
// as synced: the next decoder on the slice starts unsynced
// (RadeChannel::m_synced false) and its first lock has to reach the VFO
// flag and the FreeDV reporter, not be dropped as a repeat.
void TestRadeChannelModelWiring::newDecoderFirstLockReachesFlagAndReporter()
{
    RadioModel model;
    const int sliceId = model.addSlice();
    SliceModel* slice = model.sliceById(sliceId);
    QVERIFY(slice != nullptr);
    FreeDVRadeReporterBridge* reporter = model.radeReporterBridgeForTest();
    QVERIFY(reporter != nullptr);

    VfoWidget flag;
    flag.setSlice(slice);
    flag.setRadeActive(true);
    MainWindow::wireRadeFlagForTest(&model, &flag, sliceId);
    QSignalSpy syncSpy(&model, &RadioModel::radeSyncChanged);

    auto first = std::make_unique<TestableRadeChannel>();
    model.wireRadeChannel(sliceId, first.get(), slice);
    first->emitSyncChangedForTest(true);
    first->emitSnrChangedForTest(7.0f);
    QCOMPARE(syncSpy.count(), 1);
    QCOMPARE(reporter->syncedForTest(), true);
    QVERIFY(flag.snrLabelForTest()->text().contains(QStringLiteral("●")));

    first.reset();
    QCOMPARE(syncSpy.count(), 2);
    QCOMPARE(syncSpy.at(1).at(0).toInt(), sliceId);
    QCOMPARE(syncSpy.at(1).at(1).toBool(), false);
    QCOMPARE(model.radeSynced(sliceId), false);
    QCOMPARE(reporter->syncedForTest(), false);
    QVERIFY(flag.snrLabelForTest()->text().contains(QStringLiteral("○")));

    TestableRadeChannel second;
    model.wireRadeChannel(sliceId, &second, slice);
    second.emitSyncChangedForTest(true);
    second.emitSnrChangedForTest(3.0f);
    QCOMPARE(syncSpy.count(), 3);
    QCOMPARE(syncSpy.at(2).at(1).toBool(), true);
    QCOMPARE(model.radeSynced(sliceId), true);
    QCOMPARE(reporter->syncedForTest(), true);
    QVERIFY(flag.snrLabelForTest()->text().contains(QStringLiteral("●")));
    QVERIFY(flag.snrLabelForTest()->text().contains(QStringLiteral("3dB")));
}

// The desktop flag appends the decoder's offset after the SNR
// (VfoWidget::setRadeFreqOffset); the slice carries the same value in Hz,
// sign included.
void TestRadeChannelModelWiring::sliceRadeFreqOffsetFollowsChannel()
{
    RadioModel model;
    const int sliceId = model.addSlice();
    SliceModel* slice = model.sliceById(sliceId);
    QVERIFY(slice != nullptr);
    QCOMPARE(slice->radeFreqOffsetHz(), 0.0);

    TestableRadeChannel channel;
    model.wireRadeChannel(sliceId, &channel, slice);
    QSignalSpy spy(slice, &SliceModel::radeFreqOffsetHzChanged);

    channel.emitFreqOffsetChangedForTest(38.5f);
    QCOMPARE(slice->radeFreqOffsetHz(), 38.5);
    QCOMPARE(spy.count(), 1);

    channel.emitFreqOffsetChangedForTest(38.5f);
    QCOMPARE(spy.count(), 1);

    channel.emitFreqOffsetChangedForTest(-12.25f);
    QCOMPARE(slice->radeFreqOffsetHz(), -12.25);
    QCOMPARE(spy.count(), 2);
}

// The desktop flag drops its RADE state when the slice leaves RADE
// (VfoWidget::setRadeActive(false)); the RADE channel is destroyed on
// the same change.
void TestRadeChannelModelWiring::sliceRadeSyncedClearsOnLeavingRade()
{
    SliceModel slice(0);
    slice.setDspMode(DSPMode::RADE_U);
    slice.setRadeSynced(true);
    QCOMPARE(slice.radeSynced(), true);

    slice.setDspMode(DSPMode::RADE_L);
    QCOMPARE(slice.radeSynced(), false);

    slice.setRadeSynced(true);
    slice.setDspMode(DSPMode::USB);
    QCOMPARE(slice.radeSynced(), false);
}

void TestRadeChannelModelWiring::radeStatusPropertiesAreReadOnlyAndGated()
{
    const QMetaObject& meta = SliceModel::staticMetaObject;
    for (const char* name : {"radeSynced", "radeFreqOffsetHz"}) {
        const int index = meta.indexOfProperty(name);
        QVERIFY2(index >= 0, name);
        const QMetaProperty property = meta.property(index);
        QVERIFY2(!property.isWritable(), name);
        QVERIFY2(property.hasNotifySignal(), name);
        QCOMPARE(MirrorPolicy::directionFor(QByteArrayLiteral("SliceModel"), name),
                 MirrorDirection::Outbound);
        QVERIFY2(MirrorPolicy::hasExplicitEntry(QByteArrayLiteral("SliceModel"), name), name);
        const MirrorPolicy::FeatureGate* gate =
            MirrorPolicy::featureGateFor(QByteArrayLiteral("SliceModel"), name);
        QVERIFY2(gate != nullptr, name);
        QCOMPARE(QByteArray(gate->feature), QByteArrayLiteral("radeStatus"));
    }
    // Declared last, after diversityPattern, so every earlier property
    // keeps its wire ordinal.
    QVERIFY(meta.indexOfProperty("radeSynced") > meta.indexOfProperty("diversityPattern"));
    QVERIFY(meta.indexOfProperty("radeFreqOffsetHz") > meta.indexOfProperty("radeSynced"));
}

QTEST_MAIN(TestRadeChannelModelWiring)
#include "tst_rade_channel_model_wiring.moc"
