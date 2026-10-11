// =================================================================
// tests/tst_capture_protocol.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Unit tests for the capture helper
// record protocol (R-R3-36): framing, bounded incremental reader, PCM and
// JSON message codecs, and every malformed-input rejection.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 13 (R-AUD-17, R-AUD-18): version 3
//               cases (AttachRing, RingAttached, the Configure identity
//               keys, device-in-use, the Status latency and buffer).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 15 (R-AUD-19, R-AUD-20, R-AUD-21):
//               version 4, the ASIO records and their rejections.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/audio/CaptureProtocol.h"

#include <QJsonArray>
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
    s.latencyUs = 2'500;
    s.bufferFrames = 256;
    return s;
}

AsioDriverCaps motuCaps()
{
    AsioDriverCaps caps;
    caps.name = QStringLiteral("MOTU M Series");
    caps.inputChannels = 4;
    caps.outputChannels = 4;
    caps.sampleType = AsioSampleType::Int24Lsb;
    caps.minBufferFrames = 32;
    caps.maxBufferFrames = 2048;
    caps.preferredBufferFrames = 256;
    caps.granularity = -1;
    caps.sampleRates = {44100.0, 48000.0, 96000.0};
    caps.currentRate = 48000.0;
    caps.inputLatencyFrames = 300;
    caps.outputLatencyFrames = 412;
    return caps;
}

AsioOpen motuOpen()
{
    AsioOpen open;
    open.serial = 9;
    open.driver = QStringLiteral("MOTU M Series");
    open.bufferFrames = 512;
    open.rate = 96000.0;
    AsioOpenUse speakers;
    speakers.role = AudioRole::Speakers;
    speakers.pair = AudioChannelPair{3, 2};
    speakers.direction = AudioDeviceDirection::Output;
    speakers.memory = QStringLiteral("/nrsc-1-0000001am");
    speakers.wake = QStringLiteral("/nrsc-1-0000001aw");
    speakers.bytes = 33'024;
    AsioOpenUse vax;
    vax.pair = AudioChannelPair{1, 1};
    vax.direction = AudioDeviceDirection::Input;
    vax.memory = QStringLiteral("Local\\nrsc-1-0000001bm");
    vax.wake = QStringLiteral("Local\\nrsc-1-0000001bw");
    vax.bytes = 4096;
    open.uses = {speakers, vax};
    return open;
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
        QCOMPARE(int(kVersion), 4);
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
        QCOMPARE(quint8(rec[4]), quint8(4));
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
        QVERIFY(encodeRecord(static_cast<RecordType>(8), QByteArray("{}")).isEmpty());
        QVERIFY(encodeRecord(static_cast<RecordType>(25), QByteArray("{}")).isEmpty());
        QVERIFY(!encodeRecord(RecordType::AsioOpen, QByteArray(kMaxAsioJsonBytes, 'a')).isEmpty());
        QVERIFY(encodeRecord(RecordType::AsioOpen, QByteArray(kMaxAsioJsonBytes + 1, 'a')).isEmpty());
        QVERIFY(!encodeRecord(RecordType::AttachRing, QByteArray(kMaxJsonBytes, 'a')).isEmpty());
        QVERIFY(encodeRecord(RecordType::AttachRing, QByteArray(kMaxJsonBytes + 1, 'a')).isEmpty());
        QVERIFY(encodeRecord(RecordType::RingAttached, QByteArray(kMaxJsonBytes + 1, 'a')).isEmpty());
        QVERIFY(!encodeRecord(RecordType::ProbeHit, QByteArray(kMaxJsonBytes, 'a')).isEmpty());
        QVERIFY(encodeRecord(RecordType::ProbeHit, QByteArray(kMaxJsonBytes + 1, 'a')).isEmpty());
        QVERIFY(encodeRecord(RecordType::ProbeEnable, QByteArray(kMaxJsonBytes + 1, 'a')).isEmpty());
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
        QTest::newRow("bad magic") << rawHeader("NCAQ", 4, 2, 0, 2) + "{}"
                                   << int(RecordReader::Error::BadMagic);
        QTest::newRow("version 1") << rawHeader("NCAP", 1, 2, 0, 2) + "{}"
                                   << int(RecordReader::Error::BadVersion);
        QTest::newRow("version 2") << rawHeader("NCAP", 2, 2, 0, 2) + "{}"
                                   << int(RecordReader::Error::BadVersion);
        QTest::newRow("version 3") << rawHeader("NCAP", 3, 2, 0, 2) + "{}"
                                   << int(RecordReader::Error::BadVersion);
        QTest::newRow("version 5") << rawHeader("NCAP", 5, 2, 0, 2) + "{}"
                                   << int(RecordReader::Error::BadVersion);
        QTest::newRow("reserved nonzero") << rawHeader("NCAP", 4, 2, 1, 2) + "{}"
                                          << int(RecordReader::Error::BadReserved);
        QTest::newRow("reserved high byte") << rawHeader("NCAP", 4, 2, 0x100, 2) + "{}"
                                            << int(RecordReader::Error::BadReserved);
        QTest::newRow("unknown type") << rawHeader("NCAP", 4, 8, 0, 2) + "{}"
                                      << int(RecordReader::Error::UnknownType);
        QTest::newRow("unknown type 25") << rawHeader("NCAP", 4, 25, 0, 2) + "{}"
                                         << int(RecordReader::Error::UnknownType);
        QTest::newRow("probe hit 4097") << rawHeader("NCAP", 4, 4, 0, 4097)
                                        << int(RecordReader::Error::Oversize);
        QTest::newRow("attach ring 4097") << rawHeader("NCAP", 4, 21, 0, 4097)
                                          << int(RecordReader::Error::Oversize);
        QTest::newRow("ring attached 4097") << rawHeader("NCAP", 4, 5, 0, 4097)
                                            << int(RecordReader::Error::Oversize);
        QTest::newRow("json 4097") << rawHeader("NCAP", 4, 2, 0, 4097)
                                   << int(RecordReader::Error::Oversize);
        QTest::newRow("configure 4097") << rawHeader("NCAP", 4, 16, 0, 4097)
                                        << int(RecordReader::Error::Oversize);
        QTest::newRow("asio caps 16385") << rawHeader("NCAP", 4, 6, 0, 16385)
                                         << int(RecordReader::Error::Oversize);
        QTest::newRow("asio state 16385") << rawHeader("NCAP", 4, 7, 0, 16385)
                                          << int(RecordReader::Error::Oversize);
        QTest::newRow("asio describe 16385") << rawHeader("NCAP", 4, 22, 0, 16385)
                                             << int(RecordReader::Error::Oversize);
        QTest::newRow("asio open 16385") << rawHeader("NCAP", 4, 23, 0, 16385)
                                         << int(RecordReader::Error::Oversize);
        QTest::newRow("asio panel 16385") << rawHeader("NCAP", 4, 24, 0, 16385)
                                          << int(RecordReader::Error::Oversize);
        QTest::newRow("pcm oversize")
            << rawHeader("NCAP", 4, 3, 0, kPcmHeaderBytes + kMaxPcmFrames * 4 + 1)
            << int(RecordReader::Error::Oversize);
        QTest::newRow("payload 0xFFFFFFFF") << rawHeader("NCAP", 4, 3, 0, 0xFFFFFFFFu)
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
        const QByteArray header = rawHeader("NCAP", kVersion, 3, 0,
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
        c.device.engine = AudioEngineKind::CoreAudio;
        c.device.deviceId = QStringLiteral("AppleUSBAudioEngine:Generic:USB:1");
        c.device.firstChannel = 3;
        c.device.micChannel = MicChannelPick::Right;
        c.device.delayMs = 5;
        const QByteArray rec = encodeConfigure(c);
        QCOMPARE(quint8(rec[5]), quint8(RecordType::Configure));
        const QJsonObject obj = objectOf(rec);
        QCOMPARE(obj.size(), 17);
        for (const char* key : {"generation", "deviceName", "sampleRate", "channels",
                                "bufferSamples", "exclusiveMode", "hostApiIndex", "driverApi",
                                "bitDepth", "eventDriven", "bypassMixer", "manualLatencyMs",
                                "engine", "deviceId", "firstChannel", "micChannel",
                                "delayMs"}) {
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
        QCOMPARE(obj.value("engine").toString(), QStringLiteral("CoreAudio"));
        QCOMPARE(obj.value("micChannel").toString(), QStringLiteral("Right"));
        QCOMPARE(d->device.engine, c.device.engine);
        QCOMPARE(d->device.deviceId, c.device.deviceId);
        QCOMPARE(d->device.firstChannel, 3);
        QCOMPARE(d->device.micChannel, MicChannelPick::Right);
        QCOMPARE(d->device.delayMs, 5);
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
        // No Engine saved: the engine key is empty and decodes to none.
        QCOMPARE(objectOf(encodeConfigure(c)).value("engine").toString(), QString());
        QVERIFY(!d->device.engine.has_value());
        QVERIFY(d->device.deviceId.isEmpty());
        QCOMPARE(d->device.firstChannel, 1);
        QCOMPARE(d->device.micChannel, MicChannelPick::Left);
        QCOMPARE(d->device.delayMs, 0);
    }

    void configureIdentityRejections()
    {
        Configure c;
        c.generation = 9;
        c.device.engine = AudioEngineKind::WindowsShared;
        const QJsonObject good = objectOf(encodeConfigure(c));
        QVERIFY(decodeConfigure(json(good)));
        auto rejects = [&](const char* key, const QJsonValue& value) {
            QJsonObject o = good;
            o[QLatin1String(key)] = value;
            return !decodeConfigure(json(o));
        };
        QVERIFY(rejects("engine", "Wasapi"));
        QVERIFY(rejects("engine", "coreaudio"));
        QVERIFY(rejects("engine", 1));
        QVERIFY(!rejects("engine", "AlsaDirect"));
        QVERIFY(!rejects("engine", "ASIO"));
        QVERIFY(rejects("micChannel", "left"));
        QVERIFY(rejects("micChannel", ""));
        QVERIFY(!rejects("micChannel", "Both"));
        QVERIFY(rejects("firstChannel", 0));
        QVERIFY(rejects("firstChannel", 33));
        QVERIFY(!rejects("firstChannel", 32));
        QVERIFY(rejects("delayMs", 4));
        QVERIFY(rejects("delayMs", -1));
        QVERIFY(rejects("delayMs", 41));
        for (const int ms : {0, 2, 3, 5, 10, 20, 40}) {
            QVERIFY(!rejects("delayMs", ms));
        }
        QVERIFY(rejects("deviceId", QString(513, 'd')));
        QVERIFY(rejects("deviceId", 5));
        for (const char* key : {"engine", "deviceId", "firstChannel", "micChannel", "delayMs"}) {
            QJsonObject missing = good;
            missing.remove(QLatin1String(key));
            QVERIFY2(!decodeConfigure(json(missing)), key);
        }
        c.device.delayMs = 7;
        QVERIFY(encodeConfigure(c).isEmpty());
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
            {FailReason::Internal, "internal"},
            {FailReason::DeviceInUse, "device-in-use"}};
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
                QCOMPARE(d->latencyUs, s.latencyUs);
                QCOMPARE(d->bufferFrames, s.bufferFrames);
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
        QVERIFY(rejects("latencyUs", -1));
        QVERIFY(rejects("latencyUs", 10'000'001));
        QVERIFY(rejects("latencyUs", 1.5));
        QVERIFY(!rejects("latencyUs", 0));
        QVERIFY(!rejects("latencyUs", 10'000'000));
        QVERIFY(rejects("bufferFrames", -1));
        QVERIFY(rejects("bufferFrames", 65'537));
        QVERIFY(!rejects("bufferFrames", 0));
        QVERIFY(!rejects("bufferFrames", 65'536));
        for (const char* key : {"latencyUs", "bufferFrames"}) {
            QJsonObject missingKey = good;
            missingKey.remove(QLatin1String(key));
            QVERIFY2(!decodeStatus(json(missingKey)), key);
        }

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

    // ── Probe records (V-HW-8) ────────────────────────────────────────────

    void probeRecordTypes()
    {
        QCOMPARE(int(RecordType::ProbeHit), 4);
        QCOMPARE(int(RecordType::ProbeEnable), 20);
    }

    void probeHitRoundTrip_data()
    {
        QTest::addColumn<qint64>("captureNs");
        QTest::newRow("-1") << qint64(-1);
        QTest::newRow("0") << qint64(0);
        QTest::newRow("max") << qint64(9223372036854775807LL);
        QTest::newRow("min") << std::numeric_limits<qint64>::min();
        QTest::newRow("steady") << qint64(1234567890123456789LL);
    }

    void probeHitRoundTrip()
    {
        QFETCH(qint64, captureNs);
        const QByteArray rec = encodeProbeHit(captureNs);
        QVERIFY(!rec.isEmpty());
        QCOMPARE(quint8(rec[4]), quint8(kVersion));
        QCOMPARE(quint8(rec[5]), quint8(RecordType::ProbeHit));
        // The value is a decimal string, so the 64-bit value is exact.
        const QJsonObject obj = objectOf(rec);
        QCOMPARE(obj.size(), 1);
        QVERIFY(obj.value("captureNs").isString());
        QCOMPARE(obj.value("captureNs").toString(), QString::number(captureNs));
        const auto decoded = decodeProbeHit(payloadOf(rec));
        QVERIFY(decoded);
        QCOMPARE(qint64(*decoded), captureNs);
    }

    void probeHitExactMaximumText()
    {
        const auto decoded = decodeProbeHit(QByteArray(R"({"captureNs":"9223372036854775807"})"));
        QVERIFY(decoded);
        QCOMPARE(qint64(*decoded), qint64(9223372036854775807LL));
        QVERIFY(payloadOf(encodeProbeHit(9223372036854775807LL))
                == QByteArray(R"({"captureNs":"9223372036854775807"})"));
    }

    void probeHitRejections()
    {
        QVERIFY(decodeProbeHit(QByteArray(R"({"captureNs":"12"})")));
        QVERIFY(!decodeProbeHit(QByteArray(R"({"captureNs":12})")));
        QVERIFY(!decodeProbeHit(QByteArray(R"({"captureNs":"1x"})")));
        QVERIFY(!decodeProbeHit(QByteArray(R"({"captureNs":"12","extra":1})")));
        QVERIFY(!decodeProbeHit(QByteArray(R"({})")));
        QVERIFY(!decodeProbeHit(QByteArray(R"({"captureMs":"12"})")));
        QVERIFY(!decodeProbeHit(QByteArray(R"({"captureNs":""})")));
        QVERIFY(!decodeProbeHit(QByteArray(R"({"captureNs":"+12"})")));
        QVERIFY(!decodeProbeHit(QByteArray(R"({"captureNs":"012"})")));
        QVERIFY(!decodeProbeHit(QByteArray(R"({"captureNs":" 12"})")));
        QVERIFY(!decodeProbeHit(QByteArray(R"({"captureNs":"9223372036854775808"})")));
        QVERIFY(!decodeProbeHit(QByteArray(R"({"captureNs":true})")));
        QVERIFY(!decodeProbeHit(QByteArray("not json")));
        QVERIFY(!decodeProbeHit(QByteArray(kMaxJsonBytes + 1, ' ')));
    }

    void probeEnableRoundTrip()
    {
        for (const bool enabled : {true, false}) {
            const QByteArray rec = encodeProbeEnable(enabled);
            QVERIFY(!rec.isEmpty());
            QCOMPARE(quint8(rec[5]), quint8(RecordType::ProbeEnable));
            QCOMPARE(payloadOf(rec), enabled ? QByteArray(R"({"enabled":true})")
                                             : QByteArray(R"({"enabled":false})"));
            const auto decoded = decodeProbeEnable(payloadOf(rec));
            QVERIFY(decoded);
            QCOMPARE(*decoded, enabled);
        }
    }

    void probeEnableRejections()
    {
        QVERIFY(!decodeProbeEnable(QByteArray(R"({"enabled":1})")));
        QVERIFY(!decodeProbeEnable(QByteArray(R"({"enabled":"true"})")));
        QVERIFY(!decodeProbeEnable(QByteArray(R"({"enabled":true,"extra":1})")));
        QVERIFY(!decodeProbeEnable(QByteArray(R"({})")));
        QVERIFY(!decodeProbeEnable(QByteArray("[true]")));
    }

    void readerAcceptsProbeRecords()
    {
        const QByteArray stream = encodeProbeHit(-1) + encodeProbeEnable(true)
                                  + encodeProbeEnable(false) + encodeProbeHit(42);
        RecordReader reader;
        reader.append(stream.constData(), stream.size());
        QCOMPARE(reader.error(), RecordReader::Error::None);
        const auto a = reader.next();
        const auto b = reader.next();
        const auto c = reader.next();
        const auto d = reader.next();
        QVERIFY(a && b && c && d);
        QCOMPARE(a->type, RecordType::ProbeHit);
        QCOMPARE(b->type, RecordType::ProbeEnable);
        QCOMPARE(c->type, RecordType::ProbeEnable);
        QCOMPARE(d->type, RecordType::ProbeHit);
        QCOMPARE(qint64(*decodeProbeHit(a->payload)), qint64(-1));
        QCOMPARE(*decodeProbeEnable(b->payload), true);
        QCOMPARE(*decodeProbeEnable(c->payload), false);
        QCOMPARE(qint64(*decodeProbeHit(d->payload)), qint64(42));
        QVERIFY(!reader.next());
    }

    // ── Shared-memory hand-off (version 3, R-AUD-17) ──────────────────────

    void ringRecordTypes()
    {
        QCOMPARE(int(RecordType::RingAttached), 5);
        QCOMPARE(int(RecordType::AttachRing), 21);
    }

    void attachRingRoundTrip()
    {
        AttachRing a;
        a.generation = 4294967295u;
        a.memory = QStringLiteral("/nrsc-4194303-deadbeefm");
        a.wake = QStringLiteral("/nrsc-4194303-deadbeefw");
        a.bytes = 33'024;
        a.inRate = 44'100;
        const QByteArray rec = encodeAttachRing(a);
        QVERIFY(!rec.isEmpty());
        QCOMPARE(quint8(rec[4]), quint8(kVersion));
        QCOMPARE(quint8(rec[5]), quint8(RecordType::AttachRing));
        const QJsonObject obj = objectOf(rec);
        QCOMPARE(obj.size(), 5);
        for (const char* key : {"generation", "memory", "wake", "bytes", "inRate"}) {
            QVERIFY2(obj.contains(QLatin1String(key)), key);
        }
        const auto d = decodeAttachRing(payloadOf(rec));
        QVERIFY(d);
        QCOMPARE(d->generation, a.generation);
        QCOMPARE(d->memory, a.memory);
        QCOMPARE(d->wake, a.wake);
        QCOMPARE(d->bytes, a.bytes);
        QCOMPARE(d->inRate, a.inRate);
        // The Windows names carry a backslash.
        a.memory = QStringLiteral("Local\\nrsc-1-0000001am");
        a.wake = QStringLiteral("Local\\nrsc-1-0000001aw");
        QCOMPARE(decodeAttachRing(payloadOf(encodeAttachRing(a)))->memory, a.memory);
    }

    void attachRingRejections()
    {
        AttachRing a;
        a.generation = 3;
        a.memory = QStringLiteral("/m");
        a.wake = QStringLiteral("/w");
        a.bytes = 4096;
        a.inRate = 48000;
        const QJsonObject good = objectOf(encodeAttachRing(a));
        QVERIFY(decodeAttachRing(json(good)));
        auto rejects = [&](const char* key, const QJsonValue& value) {
            QJsonObject o = good;
            o[QLatin1String(key)] = value;
            return !decodeAttachRing(json(o));
        };
        QVERIFY(rejects("generation", 0));
        QVERIFY(rejects("memory", ""));
        QVERIFY(rejects("wake", ""));
        QVERIFY(rejects("wake", "/m"));          // the same name twice
        QVERIFY(rejects("memory", QString(513, 'm')));
        QVERIFY(rejects("bytes", 0));
        QVERIFY(rejects("bytes", double(kMaxRingBytes + 1)));
        QVERIFY(!rejects("bytes", double(kMaxRingBytes)));
        QVERIFY(rejects("bytes", "4096"));
        QVERIFY(rejects("inRate", 7999));
        QVERIFY(rejects("inRate", 384001));
        QVERIFY(!rejects("inRate", 8000));
        QVERIFY(!rejects("inRate", 384000));
        QJsonObject extra = good;
        extra.insert("pid", 1);
        QVERIFY(!decodeAttachRing(json(extra)));
        QJsonObject missing = good;
        missing.remove("inRate");
        QVERIFY(!decodeAttachRing(json(missing)));
        a.generation = 0;
        QVERIFY(encodeAttachRing(a).isEmpty());
    }

    void ringAttachedRoundTrip()
    {
        const QByteArray rec = encodeRingAttached({77});
        QCOMPARE(quint8(rec[5]), quint8(RecordType::RingAttached));
        QCOMPARE(payloadOf(rec), QByteArray(R"({"generation":77})"));
        QCOMPARE(decodeCommand(payloadOf(rec))->generation, 77u);
        QVERIFY(encodeRingAttached({0}).isEmpty());
        RecordReader reader;
        const QByteArray stream = encodeAttachRing({5, "/a", "/b", 64, 48000})
                                  + encodeRingAttached({5});
        reader.append(stream.constData(), stream.size());
        QCOMPARE(reader.next()->type, RecordType::AttachRing);
        QCOMPARE(reader.next()->type, RecordType::RingAttached);
        QCOMPARE(reader.error(), RecordReader::Error::None);
    }

    // ── ASIO (version 4, R-AUD-19 to R-AUD-21) ────────────────────────────

    void asioRecordTypes()
    {
        QCOMPARE(int(RecordType::AsioCaps), 6);
        QCOMPARE(int(RecordType::AsioState), 7);
        QCOMPARE(int(RecordType::AsioDescribe), 22);
        QCOMPARE(int(RecordType::AsioOpen), 23);
        QCOMPARE(int(RecordType::AsioControlPanel), 24);
        QCOMPARE(kMaxAsioJsonBytes, 16384);
    }

    void asioRoleKeys()
    {
        const AudioRole roles[] = {AudioRole::Speakers, AudioRole::Headphones, AudioRole::TxInput,
                                   AudioRole::Vax1, AudioRole::Vax2, AudioRole::Vax3,
                                   AudioRole::Vax4};
        for (const AudioRole role : roles) {
            QCOMPARE(asioRoleFromKey(asioRoleKey(role)), std::optional<AudioRole>(role));
        }
        QCOMPARE(asioRoleKey(AudioRole::Vax1), QStringLiteral("Vax1"));
        QVERIFY(!asioRoleFromKey(QStringLiteral("Vax5")).has_value());
    }

    void asioDescribeRoundTrip()
    {
        const QByteArray rec = encodeAsioDescribe({QStringLiteral("MOTU M Series")});
        QCOMPARE(quint8(rec[4]), quint8(kVersion));
        QCOMPARE(quint8(rec[5]), quint8(RecordType::AsioDescribe));
        QCOMPARE(payloadOf(rec), QByteArray(R"({"driver":"MOTU M Series"})"));
        QCOMPARE(decodeAsioDescribe(payloadOf(rec))->driver, QStringLiteral("MOTU M Series"));
        QCOMPARE(decodeAsioDescribe(R"({"driver":""})")->driver, QString());
        QVERIFY(!decodeAsioDescribe(R"({"driver":1})"));
        QVERIFY(!decodeAsioDescribe(R"({"driver":"a","pid":1})"));
        QVERIFY(!decodeAsioDescribe("{}"));
    }

    void asioCapsRoundTrip()
    {
        AsioCapsRecord caps;
        caps.drivers = {QStringLiteral("Focusrite USB ASIO"), QStringLiteral("MOTU M Series")};
        caps.driver = QStringLiteral("MOTU M Series");
        caps.caps = motuCaps();
        caps.inUse = false;
        const QByteArray rec = encodeAsioCaps(caps);
        QVERIFY(!rec.isEmpty());
        QCOMPARE(quint8(rec[5]), quint8(RecordType::AsioCaps));
        const auto d = decodeAsioCaps(payloadOf(rec));
        QVERIFY(d);
        QCOMPARE(d->drivers, caps.drivers);
        QCOMPARE(d->driver, caps.driver);
        QVERIFY(d->caps.has_value());
        QVERIFY(*d->caps == *caps.caps);
        QCOMPARE(d->inUse, false);

        // The list only, and a held driver: no caps.
        AsioCapsRecord list;
        list.drivers = caps.drivers;
        QCOMPARE(decodeAsioCaps(payloadOf(encodeAsioCaps(list)))->drivers, caps.drivers);
        AsioCapsRecord held;
        held.drivers = caps.drivers;
        held.driver = QStringLiteral("Focusrite USB ASIO");
        held.inUse = true;
        const auto heldBack = decodeAsioCaps(payloadOf(encodeAsioCaps(held)));
        QVERIFY(heldBack && heldBack->inUse && !heldBack->caps);
    }

    void asioCapsRejections()
    {
        AsioCapsRecord caps;
        caps.drivers = {QStringLiteral("MOTU M Series")};
        caps.driver = QStringLiteral("MOTU M Series");
        caps.caps = motuCaps();
        const QJsonObject good = objectOf(encodeAsioCaps(caps));
        QVERIFY(decodeAsioCaps(json(good)));
        auto rejectsCaps = [&](const char* key, const QJsonValue& value) {
            QJsonObject o = good;
            QJsonObject c = o.value(QStringLiteral("caps")).toObject();
            c[QLatin1String(key)] = value;
            o[QStringLiteral("caps")] = c;
            return !decodeAsioCaps(json(o));
        };
        QVERIFY(rejectsCaps("name", "Another driver"));        // not the described driver
        QVERIFY(rejectsCaps("outputs", -1));
        QVERIFY(rejectsCaps("outputs", kMaxAsioChannels + 1));
        QVERIFY(rejectsCaps("sampleType", "Int32MSB"));
        QVERIFY(!rejectsCaps("sampleType", "Unsupported"));
        QVERIFY(rejectsCaps("granularity", -2));
        QVERIFY(rejectsCaps("rates", QJsonArray{48000.0, 22050.0}));
        QVERIFY(rejectsCaps("rates", QJsonArray{44100.0, 48000.0, 88200.0, 96000.0, 176400.0,
                                                192000.0, 48000.0}));
        QVERIFY(rejectsCaps("currentRate", -1.0));
        QVERIFY(rejectsCaps("inputLatency", -1));
        QJsonObject extra = good;
        extra.insert("pid", 1);
        QVERIFY(!decodeAsioCaps(json(extra)));
        QJsonObject badDrivers = good;
        badDrivers["drivers"] = QJsonArray{QString()};
        QVERIFY(!decodeAsioCaps(json(badDrivers)));
        QJsonArray tooMany;
        for (int i = 0; i <= kMaxAsioDrivers; ++i) {
            tooMany.append(QStringLiteral("Driver %1").arg(i));
        }
        badDrivers["drivers"] = tooMany;
        QVERIFY(!decodeAsioCaps(json(badDrivers)));
    }

    void asioOpenRoundTrip()
    {
        const AsioOpen open = motuOpen();
        const QByteArray rec = encodeAsioOpen(open);
        QVERIFY(!rec.isEmpty());
        QCOMPARE(quint8(rec[5]), quint8(RecordType::AsioOpen));
        const auto d = decodeAsioOpen(payloadOf(rec));
        QVERIFY(d);
        QCOMPARE(d->serial, open.serial);
        QCOMPARE(d->driver, open.driver);
        QCOMPARE(d->bufferFrames, 512);
        QCOMPARE(d->rate, 96000.0);
        QCOMPARE(d->uses.size(), 2);
        QCOMPARE(d->uses[0].role, std::optional<AudioRole>(AudioRole::Speakers));
        QCOMPARE(d->uses[0].pair, (AudioChannelPair{3, 2}));
        QCOMPARE(d->uses[0].direction, AudioDeviceDirection::Output);
        QCOMPARE(d->uses[0].memory, open.uses[0].memory);
        QCOMPARE(d->uses[0].wake, open.uses[0].wake);
        QCOMPARE(d->uses[0].bytes, open.uses[0].bytes);
        QVERIFY(!d->uses[1].role.has_value());
        QCOMPARE(d->uses[1].pair, (AudioChannelPair{1, 1}));
        QCOMPARE(d->uses[1].direction, AudioDeviceDirection::Input);

        // No uses closes the session; no driver is needed then.
        AsioOpen close;
        close.serial = 10;
        const auto closed = decodeAsioOpen(payloadOf(encodeAsioOpen(close)));
        QVERIFY(closed && closed->uses.isEmpty() && closed->driver.isEmpty());
    }

    void asioOpenRejections()
    {
        const QJsonObject good = objectOf(encodeAsioOpen(motuOpen()));
        QVERIFY(decodeAsioOpen(json(good)));
        auto rejects = [&](const char* key, const QJsonValue& value) {
            QJsonObject o = good;
            o[QLatin1String(key)] = value;
            return !decodeAsioOpen(json(o));
        };
        QVERIFY(rejects("serial", 0));
        QVERIFY(rejects("driver", ""));                          // uses need a driver
        QVERIFY(rejects("rate", 22050.0));
        QVERIFY(!rejects("rate", 0.0));                          // the driver's own
        QVERIFY(rejects("bufferFrames", -1));
        QVERIFY(rejects("bufferFrames", kMaxBufferFrames + 1));
        auto rejectsUse = [&](const char* key, const QJsonValue& value) {
            QJsonObject o = good;
            QJsonArray uses = o.value(QStringLiteral("uses")).toArray();
            QJsonObject u = uses.at(0).toObject();
            u[QLatin1String(key)] = value;
            uses[0] = u;
            o[QStringLiteral("uses")] = uses;
            return !decodeAsioOpen(json(o));
        };
        QVERIFY(rejectsUse("role", "Vax5"));
        QVERIFY(!rejectsUse("role", ""));
        QVERIFY(rejectsUse("first", 0));
        QVERIFY(rejectsUse("count", 3));
        QVERIFY(rejectsUse("count", 0));
        QVERIFY(rejectsUse("direction", "both"));
        QVERIFY(rejectsUse("memory", ""));
        QVERIFY(rejectsUse("wake", good.value("uses").toArray().at(0).toObject().value("memory")));
        QVERIFY(rejectsUse("bytes", 0));
        QVERIFY(rejectsUse("bytes", double(kMaxRingBytes + 1)));
        QJsonObject tooMany = good;
        QJsonArray uses;
        for (int i = 0; i <= kMaxAsioUses; ++i) {
            uses.append(good.value("uses").toArray().at(0));
        }
        tooMany["uses"] = uses;
        QVERIFY(!decodeAsioOpen(json(tooMany)));
    }

    void asioStateRoundTripEveryState()
    {
        const AsioStateKind kinds[] = {AsioStateKind::Running, AsioStateKind::Restarted,
                                       AsioStateKind::InUse, AsioStateKind::Failed,
                                       AsioStateKind::Closed};
        for (const AsioStateKind kind : kinds) {
            AsioState s;
            s.serial = 9;
            s.state = kind;
            s.driver = QStringLiteral("MOTU M Series");
            s.detail = QStringLiteral("detail");
            s.bufferFrames = 512;
            s.rate = 96000.0;
            s.inputLatencyFrames = 300;
            s.outputLatencyFrames = 412;
            const QByteArray rec = encodeAsioState(s);
            QVERIFY(!rec.isEmpty());
            QCOMPARE(quint8(rec[5]), quint8(RecordType::AsioState));
            const auto d = decodeAsioState(payloadOf(rec));
            QVERIFY(d);
            QVERIFY(d->state == kind);
            QCOMPARE(d->serial, s.serial);
            QCOMPARE(d->driver, s.driver);
            QCOMPARE(d->detail, s.detail);
            QCOMPARE(d->bufferFrames, 512);
            QCOMPARE(d->rate, 96000.0);
            QCOMPARE(d->inputLatencyFrames, 300);
            QCOMPARE(d->outputLatencyFrames, 412);
        }
        QCOMPARE(objectOf(encodeAsioState(AsioState{})).value("state").toString(),
                 QStringLiteral("closed"));
    }

    void asioStateRejections()
    {
        AsioState s;
        s.serial = 9;
        s.state = AsioStateKind::Running;
        s.driver = QStringLiteral("MOTU M Series");
        s.bufferFrames = 256;
        s.rate = 48000.0;
        const QJsonObject good = objectOf(encodeAsioState(s));
        QVERIFY(decodeAsioState(json(good)));
        auto rejects = [&](const char* key, const QJsonValue& value) {
            QJsonObject o = good;
            o[QLatin1String(key)] = value;
            return !decodeAsioState(json(o));
        };
        QVERIFY(rejects("state", "stopped"));
        QVERIFY(rejects("bufferFrames", 0));                    // running says what it runs at
        QVERIFY(rejects("rate", 0.0));
        QVERIFY(rejects("rate", 22050.0));
        QVERIFY(rejects("serial", -1));
        QVERIFY(rejects("inputLatency", -1));
        QJsonObject failed = good;
        failed["state"] = "failed";
        failed["bufferFrames"] = 0;
        failed["rate"] = 0.0;
        QVERIFY(decodeAsioState(json(failed)));                 // a failure needs neither
        QJsonObject extra = good;
        extra.insert("pid", 1);
        QVERIFY(!decodeAsioState(json(extra)));
        s.bufferFrames = 0;
        QVERIFY(encodeAsioState(s).isEmpty());
    }

    void asioControlPanelAndReader()
    {
        const QByteArray panel = encodeAsioControlPanel();
        QCOMPARE(quint8(panel[5]), quint8(RecordType::AsioControlPanel));
        QCOMPARE(payloadOf(panel), QByteArray("{}"));
        // The largest open fits its bound, and the reader takes every
        // ASIO record.
        AsioOpen open = motuOpen();
        open.uses[0].memory = QString(200, QLatin1Char('m'));
        open.uses[0].wake = QString(200, QLatin1Char('w'));
        while (open.uses.size() < kMaxAsioUses) {
            open.uses.append(open.uses.at(0));
        }
        const QByteArray big = encodeAsioOpen(open);
        QVERIFY(!big.isEmpty());
        QVERIFY(big.size() - kHeaderBytes > kMaxJsonBytes);
        AsioCapsRecord caps;
        caps.drivers = {QStringLiteral("MOTU M Series")};
        const QByteArray stream = encodeAsioDescribe({QString()}) + encodeAsioCaps(caps) + big
                                  + encodeAsioState(AsioState{}) + panel;
        RecordReader reader;
        reader.append(stream.constData(), stream.size());
        QCOMPARE(reader.next()->type, RecordType::AsioDescribe);
        QCOMPARE(reader.next()->type, RecordType::AsioCaps);
        QCOMPARE(reader.next()->type, RecordType::AsioOpen);
        QCOMPARE(reader.next()->type, RecordType::AsioState);
        QCOMPARE(reader.next()->type, RecordType::AsioControlPanel);
        QCOMPARE(reader.error(), RecordReader::Error::None);
        // Above the ASIO bound, a record is refused.
        const QByteArray tooBig = rawHeader("NCAP", kVersion, quint8(RecordType::AsioOpen), 0,
                                            quint32(kMaxAsioJsonBytes + 1));
        RecordReader refusing;
        refusing.append(tooBig.constData(), tooBig.size());
        QVERIFY(refusing.error() != RecordReader::Error::None);
    }

    void readerRejectsAVersion2Record()
    {
        // The window and its helper ship together; a version 2 peer is
        // refused whatever its type.
        RecordReader reader;
        const QByteArray bytes = rawHeader("NCAP", 2, 2, 0, 2) + "{}";
        reader.append(bytes.constData(), bytes.size());
        QCOMPARE(reader.error(), RecordReader::Error::BadVersion);
    }

    void readerRejectsAVersion1ProbeRecord()
    {
        // A version 1 peer never sends these; a version 1 header is
        // rejected whatever its type.
        RecordReader reader;
        const QByteArray bytes = rawHeader("NCAP", 1, 4, 0, 2) + "{}";
        reader.append(bytes.constData(), bytes.size());
        QCOMPARE(reader.error(), RecordReader::Error::BadVersion);
    }
};

QTEST_GUILESS_MAIN(TstCaptureProtocol)
#include "tst_capture_protocol.moc"
