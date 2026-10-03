// no-port-check: NereusSDR-original media framing test.
#include <QtTest>

#include "core/session/media/RemoteIqCodec.h"
#include "core/session/media/RemoteIqIngress.h"

#include <cmath>
#include <limits>

using namespace NereusSDR;

class TstRemoteIqCodec : public QObject {
    Q_OBJECT
private slots:
    void roundTrip()
    {
        const QVector<float> samples{1.0f, -2.0f, 0.25f, 3.5f};
        const auto encoded = RemoteIqCodec::encode(1, 7, 23, samples);
        QVERIFY(encoded);
        QCOMPARE(encoded->size(), 40);
        QCOMPARE(encoded->left(4), QByteArray("NSIQ", 4));
        QCOMPARE(quint8(encoded->at(4)), quint8(1));
        QCOMPARE(quint8(encoded->at(5)), quint8(0));
        QCOMPARE(encoded->mid(6, 2), QByteArray(2, '\0'));
        const auto decoded = RemoteIqCodec::decode(*encoded);
        QVERIFY(decoded);
        QCOMPARE(decoded->sliceId, quint32(1));
        QCOMPARE(decoded->generation, quint32(7));
        QCOMPARE(decoded->sequence, quint32(23));
        QCOMPARE(decoded->samples, samples);
    }

    void boundsAndMalformed()
    {
        const QVector<float> maximum(2 * RemoteIqCodec::kMaxPairs, 0.5f);
        const auto encoded = RemoteIqCodec::encode(0, 1, 0, maximum);
        QVERIFY(encoded);
        QCOMPARE(encoded->size(), RemoteIqCodec::kMaxMessageBytes);
        QVERIFY(RemoteIqCodec::decode(*encoded));
        QVERIFY(!RemoteIqCodec::encode(0, 1, 0, {}));
        QVERIFY(!RemoteIqCodec::encode(0, 1, 0, QVector<float>(2 * (RemoteIqCodec::kMaxPairs + 1))));
        QVERIFY(!RemoteIqCodec::encode(0, 0, 0, {1.0f, 2.0f}));
        QVERIFY(!RemoteIqCodec::encode(0, 1, 0, {1.0f}));
        QVERIFY(!RemoteIqCodec::encode(0, 1, 0, {1.0f, std::numeric_limits<float>::infinity()}));
        QByteArray malformed = *encoded;
        malformed[5] = '\1';
        QVERIFY(!RemoteIqCodec::decode(malformed));
        malformed = *encoded;
        malformed[6] = '\1';
        QVERIFY(!RemoteIqCodec::decode(malformed));
        malformed = *encoded;
        malformed[20] = '\0';
        malformed[21] = '\0';
        malformed[22] = '\0';
        malformed[23] = '\0';
        QVERIFY(!RemoteIqCodec::decode(malformed));
        malformed = *encoded;
        malformed.chop(1);
        QVERIFY(!RemoteIqCodec::decode(malformed));
    }

    void ingressBoundAndRetirement()
    {
        RemoteIqIngress ingress;
        const QVector<float> samples(2 * RemoteIqIngress::kPairsPerFrame, 0.25f);
        for (unsigned i = 0; i < RemoteIqIngress::kFramesCapacity; ++i) {
            QVERIFY(ingress.push(samples));
        }
        QVERIFY(!ingress.push(samples));
        QVERIFY(ingress.failed.load());
        for (unsigned i = 0; i < RemoteIqIngress::kFramesCapacity; ++i) {
            const auto popped = ingress.pop();
            QVERIFY(popped);
            QCOMPARE(*popped, samples);
        }
        QVERIFY(!ingress.pop());
        QVERIFY(!ingress.push(samples));
        RemoteIqIngress retired;
        retired.stopped.store(true);
        QVERIFY(!retired.push(samples));
        QVERIFY(!retired.pop());
        QCOMPARE(RemoteIqIngress::kStorageBytes, 131072u);
    }

    void variableProducerChunksBecomeExactFrames()
    {
        RemoteIqIngress ingress;
        QVector<float> first(2 * 1026, 0.25f);
        QVector<float> second(2 * 1022, 0.75f);
        QVERIFY(ingress.push(first));
        const auto frame0 = ingress.pop();
        QVERIFY(frame0);
        QCOMPARE(frame0->size(), 2048);
        QCOMPARE(frame0->constFirst(), 0.25f);
        QVERIFY(!ingress.pop());
        QVERIFY(ingress.push(second));
        const auto frame1 = ingress.pop();
        QVERIFY(frame1);
        QCOMPARE(frame1->size(), 2048);
        QCOMPARE(frame1->at(0), 0.25f);
        QCOMPARE(frame1->at(3), 0.25f);
        QCOMPARE(frame1->at(4), 0.75f);
        QVERIFY(!ingress.pop());
    }
};

QTEST_APPLESS_MAIN(TstRemoteIqCodec)
#include "tst_remote_iq_codec.moc"
