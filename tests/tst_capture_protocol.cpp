// =================================================================
// tests/tst_capture_protocol.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Unit tests for the capture helper
// record protocol (R-R3-36): framing, bounded incremental reader, PCM and
// JSON message codecs, and every malformed-input rejection.
// =================================================================

#include <QtTest/QtTest>

#include "core/audio/CaptureProtocol.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>

#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

using namespace NereusSDR;
using namespace NereusSDR::CaptureProtocol;

namespace {

QByteArray payloadOf(const QByteArray& record)
{
    return record.mid(kHeaderBytes);
}

QByteArray rawHeader(const char magic[4], quint8 version, quint8 type, quint16 reserved,
                     quint32 payloadBytes)
{
    QByteArray h(kHeaderBytes, '\0');
    std::memcpy(h.data(), magic, 4);
    h[4] = static_cast<char>(version);
    h[5] = static_cast<char>(type);
    qToLittleEndian<quint16>(reserved, h.data() + 6);
    qToLittleEndian<quint32>(payloadBytes, h.data() + 8);
    return h;
}

QByteArray json(const QJsonObject& obj)
{
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QJsonObject objectOf(const QByteArray& record)
{
    return QJsonDocument::fromJson(payloadOf(record)).object();
}

Status readyStatus()
{
    Status s;
    s.generation = 7;
    s.state = HelperState::Ready;
    s.actualDevice = QStringLiteral("USB Audio CODEC");
    s.nativeRate = 44100;
    s.nativeChannels = 2;
    s.reason = FailReason::None;
    s.detail = QStringLiteral("opened");
    return s;
}

QByteArray pcmPayload(quint32 generation, quint32 frameCount, int actualFloats, float fill = 0.25f)
{
    QByteArray p(kPcmHeaderBytes, '\0');
    qToLittleEndian<quint32>(generation, p.data());
    qToLittleEndian<quint32>(frameCount, p.data() + 4);
    qToLittleEndian<quint64>(10, p.data() + 8);
    qToLittleEndian<quint64>(20, p.data() + 16);
    for (int i = 0; i < actualFloats; ++i) {
        char b[4];
        quint32 bits = 0;
        std::memcpy(&bits, &fill, 4);
        qToLittleEndian<quint32>(bits, b);
        p.append(b, 4);
    }
    return p;
}

} // namespace

class TstCaptureProtocol : public QObject {
    Q_OBJECT

private slots:
    // ── Constants and framing ──────────────────────────────────────────────

    void contractConstants()
    {
        QCOMPARE(int(kVersion), 1);
        QCOMPARE(kHeaderBytes, 12);
        QCOMPARE(kMaxJsonBytes, 4096);
        QCOMPARE(kPcmHeaderBytes, 24);
        QCOMPARE(kMaxPcmFrames, 4800);
        QCOMPARE(kHelperPcmFrames, 480);
        QCOMPARE(kSampleRate, 48000);
    }

    void headerLayoutIsLittleEndian()
    {
        const QByteArray rec = encodeRecord(RecordType::Status, QByteArray(300, 'x'));
        QCOMPARE(rec.size(), kHeaderBytes + 300);
        QCOMPARE(rec.left(4), QByteArray("NCAP"));
        QCOMPARE(quint8(rec[4]), quint8(1));
        QCOMPARE(quint8(rec[5]), quint8(2));
        QCOMPARE(quint8(rec[6]), quint8(0));
        QCOMPARE(quint8(rec[7]), quint8(0));
        QCOMPARE(quint8(rec[8]), quint8(300 & 0xFF));
        QCOMPARE(quint8(rec[9]), quint8(300 >> 8));
        QCOMPARE(quint8(rec[10]), quint8(0));
        QCOMPARE(quint8(rec[11]), quint8(0));
    }

    void encodeRecordEnforcesBounds()
    {
        QVERIFY(!encodeRecord(RecordType::Hello, QByteArray(kMaxJsonBytes, 'a')).isEmpty());
        QVERIFY(encodeRecord(RecordType::Hello, QByteArray(kMaxJsonBytes + 1, 'a')).isEmpty());
        QVERIFY(encodeRecord(RecordType::Configure, QByteArray(kMaxJsonBytes + 1, 'a')).isEmpty());
        const int pcmMax = kPcmHeaderBytes + kMaxPcmFrames * 4;
        QVERIFY(!encodeRecord(RecordType::Pcm, QByteArray(pcmMax, 'a')).isEmpty());
        QVERIFY(encodeRecord(RecordType::Pcm, QByteArray(pcmMax + 1, 'a')).isEmpty());
        QVERIFY(encodeRecord(static_cast<RecordType>(4), QByteArray("{}")).isEmpty());
    }

    // ── RecordReader ──────────────────────────────────────────────────────

    void readerSplitsAtEveryByteBoundary()
    {
        const QByteArray stream = encodeHello({1, 4242, QStringLiteral("abc")})
                                  + encodeOpen({5})
                                  + encodePcm(5, 0, 99, std::vector<float>(480, 0.5f).data(), 480)
                                  + encodeShutdown();
        RecordReader reader;
        std::vector<Record> out;
        for (qsizetype i = 0; i < stream.size(); ++i) {
            reader.append(stream.constData() + i, 1);
            while (auto r = reader.next()) {
                out.push_back(*r);
            }
        }
        QCOMPARE(reader.error(), RecordReader::Error::None);
        QCOMPARE(reader.bufferedBytes(), qsizetype(0));
        QCOMPARE(int(out.size()), 4);
        QCOMPARE(out[0].type, RecordType::Hello);
        QCOMPARE(out[1].type, RecordType::Open);
        QCOMPARE(out[2].type, RecordType::Pcm);
        QCOMPARE(out[3].type, RecordType::Shutdown);
        QCOMPARE(decodeHello(out[0].payload)->pid, qint64(4242));
        QCOMPARE(decodeCommand(out[1].payload)->generation, quint32(5));
        QCOMPARE(decodePcm(out[2].payload)->samples.size(), 480);
    }

    void readerSplitsAtEveryPosition()
    {
        const QByteArray stream = encodeOpen({3}) + encodeStop({3});
        for (qsizetype cut = 0; cut <= stream.size(); ++cut) {
            RecordReader reader;
            reader.append(stream.constData(), cut);
            reader.append(stream.constData() + cut, stream.size() - cut);
            auto a = reader.next();
            auto b = reader.next();
            QVERIFY(a && b);
            QCOMPARE(a->type, RecordType::Open);
            QCOMPARE(b->type, RecordType::Stop);
            QVERIFY(!reader.next());
        }
    }

    void readerHandlesSeveralRecordsInOneAppend()
    {
        QByteArray stream;
        for (quint32 g = 1; g <= 20; ++g) {
            stream += encodeOpen({g});
        }
        RecordReader reader;
        reader.append(stream.constData(), stream.size());
        QVERIFY(reader.bufferedBytes() <= kHeaderBytes + kPcmHeaderBytes + kMaxPcmFrames * 4);
        for (quint32 g = 1; g <= 20; ++g) {
            auto r = reader.next();
            QVERIFY(r);
            QCOMPARE(decodeCommand(r->payload)->generation, g);
        }
        QVERIFY(!reader.next());
    }

    void readerAcceptsEmptyPayload()
    {
        const QByteArray rec = encodeRecord(RecordType::Shutdown, QByteArray());
        RecordReader reader;
        reader.append(rec.constData(), rec.size());
        auto r = reader.next();
        QVERIFY(r);
        QVERIFY(r->payload.isEmpty());
    }

    void readerRejections_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::addColumn<int>("error");
        QTest::newRow("bad magic") << rawHeader("NCAQ", 1, 2, 0, 2) + "{}"
                                   << int(RecordReader::Error::BadMagic);
        QTest::newRow("version 2") << rawHeader("NCAP", 2, 2, 0, 2) + "{}"
                                   << int(RecordReader::Error::BadVersion);
        QTest::newRow("reserved nonzero") << rawHeader("NCAP", 1, 2, 1, 2) + "{}"
                                          << int(RecordReader::Error::BadReserved);
        QTest::newRow("reserved high byte") << rawHeader("NCAP", 1, 2, 0x100, 2) + "{}"
                                            << int(RecordReader::Error::BadReserved);
        QTest::newRow("unknown type") << rawHeader("NCAP", 1, 4, 0, 2) + "{}"
                                      << int(RecordReader::Error::UnknownType);
        QTest::newRow("json 4097") << rawHeader("NCAP", 1, 2, 0, 4097)
                                   << int(RecordReader::Error::Oversize);
        QTest::newRow("configure 4097") << rawHeader("NCAP", 1, 16, 0, 4097)
                                        << int(RecordReader::Error::Oversize);
        QTest::newRow("pcm oversize")
            << rawHeader("NCAP", 1, 3, 0, kPcmHeaderBytes + kMaxPcmFrames * 4 + 1)
            << int(RecordReader::Error::Oversize);
        QTest::newRow("payload 0xFFFFFFFF") << rawHeader("NCAP", 1, 3, 0, 0xFFFFFFFFu)
                                            << int(RecordReader::Error::Oversize);
    }

    void readerRejections()
    {
        QFETCH(QByteArray, bytes);
        QFETCH(int, error);
        // A good record first, then the bad one, then another good one.
        const QByteArray good = encodeOpen({1});
        RecordReader reader;
        reader.append(good.constData(), good.size());
        QVERIFY(reader.next());
        reader.append(bytes.constData(), bytes.size());
        QCOMPARE(int(reader.error()), error);
        QVERIFY(!reader.next());
        reader.append(good.constData(), good.size());
        QVERIFY(!reader.next());
        QCOMPARE(int(reader.error()), error);  // sticky
        QVERIFY(reader.bufferedBytes() <= kHeaderBytes);
    }

    void readerBoundedOnGarbageStream()
    {
        const qsizetype bound = kHeaderBytes + kPcmHeaderBytes + kMaxPcmFrames * 4;
        RecordReader reader;
        const QByteArray junk(1000, 'Z');
        for (int i = 0; i < 1000; ++i) {
            reader.append(junk.constData(), junk.size());
            QVERIFY(reader.bufferedBytes() <= bound);
            QVERIFY(!reader.next());
        }
        QCOMPARE(reader.error(), RecordReader::Error::BadMagic);
    }

    void readerBoundedWhileLargestRecordArrives()
    {
        const qsizetype bound = kHeaderBytes + kPcmHeaderBytes + kMaxPcmFrames * 4;
        const QByteArray header = rawHeader("NCAP", 1, 3, 0,
                                            kPcmHeaderBytes + kMaxPcmFrames * 4);
        RecordReader reader;
        reader.append(header.constData(), header.size());
        const QByteArray chunk(777, '\0');
        // Stream far more than one record's worth; the reader must never
        // hold more than one bounded record.
        for (int i = 0; i < 200; ++i) {
            reader.append(chunk.constData(), chunk.size());
            QVERIFY2(reader.bufferedBytes() <= bound,
                     qPrintable(QString::number(reader.bufferedBytes())));
            while (reader.next()) {
            }
            if (reader.error() != RecordReader::Error::None) {
                break;
            }
        }
        // The zero bytes following the first record are not a valid magic.
        QCOMPARE(reader.error(), RecordReader::Error::BadMagic);
    }

    // ── PCM ───────────────────────────────────────────────────────────────

    void pcmRoundTripMaximum()
    {
        std::vector<float> samples(kMaxPcmFrames);
        for (int i = 0; i < kMaxPcmFrames; ++i) {
            samples[i] = std::sin(float(i) * 0.01f) * 0.9f - 1e-7f * float(i);
        }
        const quint64 pos = 0x0123456789ABCDEFull;
        const quint64 ns = 0xFEDCBA9876543210ull;
        const QByteArray rec = encodePcm(0xFFFFFFFFu, pos, ns, samples.data(), kMaxPcmFrames);
        QCOMPARE(rec.size(), kHeaderBytes + kPcmHeaderBytes + kMaxPcmFrames * 4);
        RecordReader reader;
        reader.append(rec.constData(), rec.size());
        auto r = reader.next();
        QVERIFY(r);
        QCOMPARE(r->type, RecordType::Pcm);
        auto block = decodePcm(r->payload);
        QVERIFY(block);
        QCOMPARE(block->generation, 0xFFFFFFFFu);
        QCOMPARE(block->framePosition, pos);
        QCOMPARE(block->sentMonotonicNs, ns);
        QCOMPARE(block->samples.size(), kMaxPcmFrames);
        for (int i = 0; i < kMaxPcmFrames; ++i) {
            QCOMPARE(block->samples[i], samples[i]);  // bit-exact float
        }
    }

    void pcmRoundTripSingleAndHelperSize()
    {
        const float one = -0.5f;
        auto b1 = decodePcm(payloadOf(encodePcm(1, 0, 0, &one, 1)));
        QVERIFY(b1);
        QCOMPARE(b1->samples.size(), 1);
        QCOMPARE(b1->samples[0], -0.5f);
        std::vector<float> ten(kHelperPcmFrames, 0.125f);
        auto b2 = decodePcm(payloadOf(encodePcm(2, 480, 1, ten.data(), kHelperPcmFrames)));
        QVERIFY(b2);
        QCOMPARE(b2->framePosition, quint64(480));
        QCOMPARE(b2->samples.size(), kHelperPcmFrames);
    }

    void pcmPayloadLayout()
    {
        const float s = 1.0f;
        const QByteArray p = payloadOf(encodePcm(0x01020304u, 5, 6, &s, 1));
        QCOMPARE(p.size(), kPcmHeaderBytes + 4);
        QCOMPARE(qFromLittleEndian<quint32>(p.constData()), 0x01020304u);
        QCOMPARE(qFromLittleEndian<quint32>(p.constData() + 4), 1u);
        QCOMPARE(qFromLittleEndian<quint64>(p.constData() + 8), quint64(5));
        QCOMPARE(qFromLittleEndian<quint64>(p.constData() + 16), quint64(6));
        QCOMPARE(qFromLittleEndian<quint32>(p.constData() + 24), 0x3F800000u);
    }

    void pcmEncodeRejections()
    {
        std::vector<float> s(kMaxPcmFrames + 1, 0.0f);
        QVERIFY(encodePcm(1, 0, 0, s.data(), 0).isEmpty());
        QVERIFY(encodePcm(1, 0, 0, s.data(), kMaxPcmFrames + 1).isEmpty());
        QVERIFY(encodePcm(0, 0, 0, s.data(), 1).isEmpty());
        QVERIFY(encodePcm(1, 0, 0, nullptr, 1).isEmpty());
        s[3] = std::numeric_limits<float>::quiet_NaN();
        QVERIFY(encodePcm(1, 0, 0, s.data(), 10).isEmpty());
        s[3] = std::numeric_limits<float>::infinity();
        QVERIFY(encodePcm(1, 0, 0, s.data(), 10).isEmpty());
    }

    void pcmDecodeRejections()
    {
        QVERIFY(decodePcm(pcmPayload(1, 1, 1)));
        QVERIFY(!decodePcm(pcmPayload(1, 0, 0)));                  // frameCount 0
        QVERIFY(!decodePcm(pcmPayload(1, 4801, 4801)));            // frameCount 4801
        QVERIFY(!decodePcm(pcmPayload(1, 10, 9)));                 // short
        QVERIFY(!decodePcm(pcmPayload(1, 10, 11)));                // long
        QVERIFY(!decodePcm(pcmPayload(1, 10, 10) + "x"));          // trailing byte
        QVERIFY(!decodePcm(pcmPayload(0, 1, 1)));                  // generation 0
        QVERIFY(!decodePcm(QByteArray(kPcmHeaderBytes - 1, '\0'))); // truncated header
        QVERIFY(!decodePcm(pcmPayload(1, 4, 4, std::numeric_limits<float>::quiet_NaN())));
        QVERIFY(!decodePcm(pcmPayload(1, 4, 4, std::numeric_limits<float>::infinity())));
        QVERIFY(!decodePcm(pcmPayload(1, 4, 4, -std::numeric_limits<float>::infinity())));
    }

    // ── Hello ─────────────────────────────────────────────────────────────

    void helloRoundTrip()
    {
        const QByteArray rec = encodeHello({1, 98765, QStringLiteral("0.5.2 abc1234")});
        QVERIFY(!rec.isEmpty());
        QCOMPARE(quint8(rec[5]), quint8(RecordType::Hello));
        auto h = decodeHello(payloadOf(rec));
        QVERIFY(h);
        QCOMPARE(h->protocol, 1);
        QCOMPARE(h->pid, qint64(98765));
        QCOMPARE(h->build, QStringLiteral("0.5.2 abc1234"));
    }

    void helloRejections()
    {
        const QJsonObject good{{"protocol", 1}, {"pid", 5}, {"build", "x"}};
        QVERIFY(decodeHello(json(good)));
        QJsonObject missing = good;
        missing.remove("build");
        QVERIFY(!decodeHello(json(missing)));
        QJsonObject extra = good;
        extra.insert("extra", 1);
        QVERIFY(!decodeHello(json(extra)));
        QJsonObject longBuild = good;
        longBuild["build"] = QString(513, 'b');
        QVERIFY(!decodeHello(json(longBuild)));
        QJsonObject okBuild = good;
        okBuild["build"] = QString(512, 'b');
        QVERIFY(decodeHello(json(okBuild)));
        QJsonObject strPid = good;
        strPid["pid"] = "5";
        QVERIFY(!decodeHello(json(strPid)));
        QJsonObject fracProtocol = good;
        fracProtocol["protocol"] = 1.5;
        QVERIFY(!decodeHello(json(fracProtocol)));
        QVERIFY(!decodeHello(QByteArray("not json")));
        QVERIFY(!decodeHello(QByteArray("[1,2]")));
        QVERIFY(encodeHello({1, 5, QString(513, 'b')}).isEmpty());
    }

    // ── Configure ─────────────────────────────────────────────────────────

    void configureRoundTripFullFieldSet()
    {
        Configure c;
        c.generation = 4294967295u;
        c.device.deviceName = QStringLiteral("MacBook Pro Microphone");
        c.device.sampleRate = 44100;
        c.device.channels = 1;
        c.device.bufferSamples = 512;
        c.device.exclusiveMode = true;
        c.device.hostApiIndex = -1;
        c.device.driverApi = QStringLiteral("Core Audio");
        c.device.bitDepth = 24;
        c.device.eventDriven = true;
        c.device.bypassMixer = true;
        c.device.manualLatencyMs = 17;
        const QByteArray rec = encodeConfigure(c);
        QCOMPARE(quint8(rec[5]), quint8(RecordType::Configure));
        const QJsonObject obj = objectOf(rec);
        QCOMPARE(obj.size(), 12);
        for (const char* key : {"generation", "deviceName", "sampleRate", "channels",
                                "bufferSamples", "exclusiveMode", "hostApiIndex", "driverApi",
                                "bitDepth", "eventDriven", "bypassMixer", "manualLatencyMs"}) {
            QVERIFY2(obj.contains(QLatin1String(key)), key);
        }
        auto d = decodeConfigure(payloadOf(rec));
        QVERIFY(d);
        QCOMPARE(d->generation, c.generation);
        QCOMPARE(d->device.deviceName, c.device.deviceName);
        QCOMPARE(d->device.sampleRate, c.device.sampleRate);
        QCOMPARE(d->device.channels, c.device.channels);
        QCOMPARE(d->device.bufferSamples, c.device.bufferSamples);
        QCOMPARE(d->device.exclusiveMode, c.device.exclusiveMode);
        QCOMPARE(d->device.hostApiIndex, c.device.hostApiIndex);
        QCOMPARE(d->device.driverApi, c.device.driverApi);
        QCOMPARE(d->device.bitDepth, c.device.bitDepth);
        QCOMPARE(d->device.eventDriven, c.device.eventDriven);
        QCOMPARE(d->device.bypassMixer, c.device.bypassMixer);
        QCOMPARE(d->device.manualLatencyMs, c.device.manualLatencyMs);
    }

    void configureDefaultDeviceRoundTrip()
    {
        Configure c;
        c.generation = 1;
        auto d = decodeConfigure(payloadOf(encodeConfigure(c)));
        QVERIFY(d);
        QVERIFY(d->device.deviceName.isEmpty());  // empty = system default
        QCOMPARE(d->device.sampleRate, 48000);
        QCOMPARE(d->device.bufferSamples, 128);
    }

    void configureRejections()
    {
        Configure c;
        c.generation = 9;
        const QJsonObject good = objectOf(encodeConfigure(c));
        QVERIFY(decodeConfigure(json(good)));

        QJsonObject missing = good;
        missing.remove("manualLatencyMs");
        QVERIFY(!decodeConfigure(json(missing)));
        QJsonObject extra = good;
        extra.insert("volume", 1);
        QVERIFY(!decodeConfigure(json(extra)));
        QJsonObject gen0 = good;
        gen0["generation"] = 0;
        QVERIFY(!decodeConfigure(json(gen0)));
        QJsonObject genBig = good;
        genBig["generation"] = 4294967296.0;
        QVERIFY(!decodeConfigure(json(genBig)));
        QJsonObject longName = good;
        longName["deviceName"] = QString(513, 'n');
        QVERIFY(!decodeConfigure(json(longName)));
        QJsonObject longApi = good;
        longApi["driverApi"] = QString(513, 'n');
        QVERIFY(!decodeConfigure(json(longApi)));
        QJsonObject boolAsInt = good;
        boolAsInt["exclusiveMode"] = 1;
        QVERIFY(!decodeConfigure(json(boolAsInt)));
        QJsonObject intAsBool = good;
        intAsBool["sampleRate"] = true;
        QVERIFY(!decodeConfigure(json(intAsBool)));

        c.generation = 0;
        QVERIFY(encodeConfigure(c).isEmpty());
    }

    // ── Open / Stop / Shutdown ────────────────────────────────────────────

    void commandRoundTrip()
    {
        const QByteArray open = encodeOpen({12});
        const QByteArray stop = encodeStop({4294967295u});
        QCOMPARE(quint8(open[5]), quint8(RecordType::Open));
        QCOMPARE(quint8(stop[5]), quint8(RecordType::Stop));
        QCOMPARE(decodeCommand(payloadOf(open))->generation, quint32(12));
        QCOMPARE(decodeCommand(payloadOf(stop))->generation, 4294967295u);
        const QByteArray shut = encodeShutdown();
        QCOMPARE(quint8(shut[5]), quint8(RecordType::Shutdown));
        QCOMPARE(payloadOf(shut), QByteArray("{}"));
    }

    void commandRejections()
    {
        QVERIFY(!decodeCommand(json({{"generation", 0}})));
        QVERIFY(!decodeCommand(json({{"generation", 4294967296.0}})));
        QVERIFY(!decodeCommand(json({{"generation", -1}})));
        QVERIFY(!decodeCommand(json({{"generation", 1.5}})));
        QVERIFY(!decodeCommand(json({{"generation", "1"}})));
        QVERIFY(!decodeCommand(json({})));
        QVERIFY(!decodeCommand(json({{"generation", 1}, {"x", 1}})));
        QVERIFY(!decodeCommand(QByteArray(kMaxJsonBytes + 1, ' ')));
        QVERIFY(encodeOpen({0}).isEmpty());
        QVERIFY(encodeStop({0}).isEmpty());
    }

    // ── Status ────────────────────────────────────────────────────────────

    void statusRoundTripEveryStateAndReason()
    {
        const QList<QPair<HelperState, QString>> states = {
            {HelperState::Permission, "permission"}, {HelperState::Opening, "opening"},
            {HelperState::Ready, "ready"},           {HelperState::Failed, "failed"},
            {HelperState::Stopped, "stopped"}};
        const QList<QPair<FailReason, QString>> reasons = {
            {FailReason::None, "none"},
            {FailReason::PermissionDenied, "permission-denied"},
            {FailReason::DeviceNotFound, "device-not-found"},
            {FailReason::OpenFailed, "open-failed"},
            {FailReason::StartFailed, "start-failed"},
            {FailReason::InputLost, "input-lost"},
            {FailReason::Internal, "internal"}};
        for (const auto& st : states) {
            for (const auto& rs : reasons) {
                Status s = readyStatus();
                s.state = st.first;
                s.reason = rs.first;
                const QByteArray rec = encodeStatus(s);
                QVERIFY(!rec.isEmpty());
                QCOMPARE(quint8(rec[5]), quint8(RecordType::Status));
                const QJsonObject obj = objectOf(rec);
                QCOMPARE(obj.value("state").toString(), st.second);
                QCOMPARE(obj.value("reason").toString(), rs.second);
                auto d = decodeStatus(payloadOf(rec));
                QVERIFY(d);
                QCOMPARE(d->generation, s.generation);
                QCOMPARE(d->state, s.state);
                QCOMPARE(d->actualDevice, s.actualDevice);
                QCOMPARE(d->nativeRate, s.nativeRate);
                QCOMPARE(d->nativeChannels, s.nativeChannels);
                QCOMPARE(d->reason, s.reason);
                QCOMPARE(d->detail, s.detail);
            }
        }
    }

    void statusUnknownFormatBeforeReady()
    {
        Status s;
        s.generation = 2;
        s.state = HelperState::Failed;
        s.reason = FailReason::DeviceNotFound;
        QVERIFY(decodeStatus(payloadOf(encodeStatus(s))));
        s.state = HelperState::Ready;
        s.reason = FailReason::None;
        QVERIFY(encodeStatus(s).isEmpty());
    }

    void statusRejections()
    {
        const QJsonObject good = objectOf(encodeStatus(readyStatus()));
        QVERIFY(decodeStatus(json(good)));

        auto rejects = [&](const char* key, const QJsonValue& value) {
            QJsonObject o = good;
            o[QLatin1String(key)] = value;
            return !decodeStatus(json(o));
        };
        QVERIFY(rejects("generation", 0));
        QVERIFY(rejects("generation", 4294967296.0));
        QVERIFY(rejects("state", "idle"));
        QVERIFY(rejects("state", "Ready"));
        QVERIFY(rejects("reason", "timeout"));
        QVERIFY(rejects("actualDevice", QString(513, 'a')));
        QVERIFY(rejects("detail", QString(513, 'a')));
        QVERIFY(rejects("nativeRate", 7999));
        QVERIFY(rejects("nativeRate", 384001));
        QVERIFY(rejects("nativeRate", 0));          // Ready with unknown rate
        QVERIFY(rejects("nativeChannels", 0));
        QVERIFY(rejects("nativeChannels", 33));
        QVERIFY(!rejects("nativeRate", 8000));
        QVERIFY(!rejects("nativeRate", 384000));
        QVERIFY(!rejects("nativeChannels", 1));
        QVERIFY(!rejects("nativeChannels", 32));
        QVERIFY(!rejects("detail", QString(512, 'a')));

        QJsonObject missing = good;
        missing.remove("detail");
        QVERIFY(!decodeStatus(json(missing)));
        QJsonObject extra = good;
        extra.insert("pid", 1);
        QVERIFY(!decodeStatus(json(extra)));

        Status bad = readyStatus();
        bad.state = static_cast<HelperState>(99);
        QVERIFY(encodeStatus(bad).isEmpty());
        bad = readyStatus();
        bad.nativeRate = 500000;
        QVERIFY(encodeStatus(bad).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstCaptureProtocol)
#include "tst_capture_protocol.moc"
