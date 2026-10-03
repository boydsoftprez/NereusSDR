// no-port-check: NereusSDR-original test. One Protocol 2 socket drain's I/Q
// reaches the DSP worker as one post per stream (ReceiverManager's
// beginIqBatch / endIqBatch), with the same samples in the same order as
// the per-packet path.

#include <QtTest/QtTest>

#include <QSignalSpy>

#include "core/P2RadioConnection.h"
#include "core/ReceiverManager.h"
#include "fakes/P2FakeRadio.h"

#include <cstring>

using namespace NereusSDR;
using NereusSDR::Test::P2FakeRadio;

namespace {

bool bitIdentical(const QVector<float>& a, const QVector<float>& b)
{
    return a.size() == b.size()
        && (a.isEmpty()
            || std::memcmp(a.constData(), b.constData(),
                           size_t(a.size()) * sizeof(float)) == 0);
}

// Every emission of a (index, samples, ...) signal for `index`, joined in
// emission order.
QVector<float> joined(const QSignalSpy& spy, int index)
{
    QVector<float> out;
    for (const QList<QVariant>& args : spy) {
        if (args.at(0).toInt() == index) {
            out += args.at(1).value<QVector<float>>();
        }
    }
    return out;
}

QVector<float> packet(int samples, float base)
{
    QVector<float> iq;
    for (int i = 0; i < samples; ++i) {
        iq.append(base + float(i) * 1.0e-4f);
        iq.append(-base - float(i) * 1.0e-4f);
    }
    return iq;
}

} // namespace

class TstP2IqDrainBatching : public QObject {
    Q_OBJECT

private slots:
    // Packets that wait in the socket together are one drain: the DSP
    // worker gets one stamped post per stream for all of them, and the
    // main thread one frameReceived, with the samples exactly as the
    // per-packet signal carried them.
    void oneDrainIsOnePostPerStreamWithTheSameSamples()
    {
        ReceiverManager receivers;
        const int rx = receivers.createReceiver();
        QVERIFY(rx >= 0);
        receivers.setDdcMapping(rx, 2);
        receivers.activateReceiver(rx);

        P2FakeRadio fake;
        QVERIFY(fake.start());
        P2RadioConnection conn;
        conn.setPortBasesForTest(fake.outboundPortBase(), fake.inputRolePortBase());
        conn.init();
        // As RadioModel wires them.
        connect(&conn, &RadioConnection::iqDataReceived, &receivers,
                [&receivers](int ddc, const QVector<float>& samples) {
                    receivers.feedIqData(ddc, samples);
                }, Qt::DirectConnection);
        connect(&conn, &RadioConnection::iqBatchStarted, &receivers,
                &ReceiverManager::beginIqBatch, Qt::DirectConnection);
        connect(&conn, &RadioConnection::iqBatchFinished, &receivers,
                &ReceiverManager::endIqBatch, Qt::DirectConnection);

        QSignalSpy raw(&conn, &RadioConnection::iqDataReceived);
        QSignalSpy frames(&conn, &RadioConnection::frameReceived);
        QSignalSpy stamped(&receivers, &ReceiverManager::iqDataForReceiverStamped);
        QSignalSpy hardware(&receivers, &ReceiverManager::hardwareIqDataStamped);
        QSignalSpy perPacket(&receivers, &ReceiverManager::iqDataForReceiver);

        conn.connectToRadio(fake.radioInfo());
        QTRY_VERIFY_WITH_TIMEOUT(fake.hasClient(), 3000);
        // One packet first establishes the stream, so the connect watchdog
        // is cancelled before the timed part and cannot tear it down.
        fake.sendDdc(2, 0.001f, -0.001f);
        QTRY_VERIFY_WITH_TIMEOUT(conn.state() == ConnectionState::Connected, 3000);
        raw.clear();
        frames.clear();
        stamped.clear();
        hardware.clear();
        perPacket.clear();

        // Eight packets, each with its own values, sent before the event
        // loop runs, so they wait in the socket together.
        constexpr int kPackets = 8;
        for (int i = 0; i < kPackets; ++i) {
            fake.sendDdc(2, 0.01f * float(i + 1), -0.02f * float(i + 1));
        }
        QTRY_COMPARE_WITH_TIMEOUT(raw.count(), kPackets, 3000);
        QTRY_VERIFY_WITH_TIMEOUT(!stamped.isEmpty(), 3000);

        // The same samples in the same order, on both stamped routes.
        const QVector<float> expected = joined(raw, 2);
        QCOMPARE(expected.size(), kPackets * 238 * 2);
        QVERIFY(bitIdentical(joined(stamped, rx), expected));
        QVERIFY(bitIdentical(joined(hardware, 2), expected));
        // The direct per-packet tap is unchanged.
        QCOMPARE(perPacket.count(), kPackets);
        QVERIFY(bitIdentical(joined(perPacket, rx), expected));

        // One post per stream per drain, and one frameReceived per drain.
        QCOMPARE(stamped.count(), hardware.count());
        QCOMPARE(frames.count(), stamped.count());
        QVERIFY2(stamped.count() < kPackets,
                 qPrintable(QStringLiteral("%1 posts for %2 packets")
                                .arg(stamped.count()).arg(kPackets)));

        conn.disconnect();
        fake.stop();
    }

    // A held batch goes out early once it reaches kIqBatchMaxSamples, and
    // outside a batch every packet is posted at once, as before.
    void heldBatchIsBoundedAndUnbatchedFeedIsPerPacket()
    {
        ReceiverManager receivers;
        const int rx = receivers.createReceiver();
        receivers.setDdcMapping(rx, 3);
        receivers.activateReceiver(rx);
        QSignalSpy stamped(&receivers, &ReceiverManager::iqDataForReceiverStamped);

        // Unbatched: one post per packet.
        QVector<float> expected;
        for (int i = 0; i < 3; ++i) {
            const QVector<float> iq = packet(238, 0.1f * float(i + 1));
            expected += iq;
            receivers.feedIqData(3, iq);
        }
        QCOMPARE(stamped.count(), 3);
        QVERIFY(bitIdentical(joined(stamped, rx), expected));

        // Batched: 20 packets of 238 samples. The held batch goes out when it
        // reaches 2048 samples (after 9 packets, then 18) and the rest at the
        // end of the drain.
        stamped.clear();
        expected.clear();
        receivers.beginIqBatch();
        for (int i = 0; i < 20; ++i) {
            const QVector<float> iq = packet(238, 0.2f + 0.01f * float(i));
            expected += iq;
            receivers.feedIqData(3, iq);
            if (i == 7) {
                QCOMPARE(stamped.count(), 0);
            }
        }
        QCOMPARE(stamped.count(), 2);
        receivers.endIqBatch();
        QCOMPARE(stamped.count(), 3);
        QCOMPARE(stamped.at(0).at(1).value<QVector<float>>().size(), 9 * 238 * 2);
        QCOMPARE(stamped.at(1).at(1).value<QVector<float>>().size(), 9 * 238 * 2);
        QCOMPARE(stamped.at(2).at(1).value<QVector<float>>().size(), 2 * 238 * 2);
        QVERIFY(bitIdentical(joined(stamped, rx), expected));

        // Closed again: per packet.
        stamped.clear();
        receivers.feedIqData(3, packet(238, 0.9f));
        QCOMPARE(stamped.count(), 1);
    }

    // A reset (a disconnect) in the middle of a drain drops the held
    // samples of the receivers it removes and ends the batch: nothing held
    // is posted afterwards, and the next feed on the same thread is per
    // packet again.
    void resetDropsTheHeldBatchAndEndsBatching()
    {
        ReceiverManager receivers;
        const int rx = receivers.createReceiver();
        receivers.setDdcMapping(rx, 3);
        receivers.activateReceiver(rx);
        QSignalSpy stamped(&receivers, &ReceiverManager::iqDataForReceiverStamped);
        QSignalSpy hardware(&receivers, &ReceiverManager::hardwareIqDataStamped);

        receivers.beginIqBatch();
        receivers.feedIqData(3, packet(238, 0.3f));
        receivers.feedIqData(3, packet(238, 0.4f));
        QCOMPARE(stamped.count(), 0);
        QCOMPARE(hardware.count(), 0);

        receivers.reset();
        receivers.endIqBatch();  // the drain's own end, after the reset
        QCOMPARE(stamped.count(), 0);
        QCOMPARE(hardware.count(), 0);

        // Batching ended with the reset: a new receiver's packet on this
        // thread goes out at once, with none of the dropped samples.
        const int again = receivers.createReceiver();
        receivers.setDdcMapping(again, 3);
        receivers.activateReceiver(again);
        receivers.beginIqBatch();
        receivers.reset();
        const int third = receivers.createReceiver();
        receivers.setDdcMapping(third, 3);
        receivers.activateReceiver(third);
        const QVector<float> fresh = packet(238, 0.5f);
        receivers.feedIqData(3, fresh);
        QCOMPARE(stamped.count(), 1);
        QCOMPARE(hardware.count(), 1);
        QVERIFY(bitIdentical(stamped.at(0).at(1).value<QVector<float>>(), fresh));
        QVERIFY(bitIdentical(hardware.at(0).at(1).value<QVector<float>>(), fresh));
    }
};

QTEST_MAIN(TstP2IqDrainBatching)
#include "tst_p2_iq_drain_batching.moc"
