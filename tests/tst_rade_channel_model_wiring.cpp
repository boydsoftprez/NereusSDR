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
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/RadeChannel.h"
#include "models/RadioModel.h"
#include "models/RxDecodeModel.h"
#include "models/SliceModel.h"

#include <cmath>

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

QTEST_GUILESS_MAIN(TestRadeChannelModelWiring)
#include "tst_rade_channel_model_wiring.moc"
