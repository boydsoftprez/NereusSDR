// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_link_conformance_regen.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan, Task 3 (R-IOS-01): writes the link's media vectors,
// tests/data/link/v1/media/*.bin with their *.expect.json, from the
// station's own encoders. Into NEREUS_LINK_REGEN_OUT (the v1 directory;
// files land in its media/ folder) when it is set, and otherwise into a
// temporary directory, so ctest runs it without touching the tree (the
// precedent is tst_link_surface_manifest_regen). Not a regression check;
// tst_link_conformance_media is.
//
//   cmake --build build --target tst_link_conformance_regen
//   NEREUS_LINK_REGEN_OUT=tests/data/link/v1 QT_QPA_PLATFORM=offscreen \
//       ./build/tests/tst_link_conformance_regen
//
// Every input is fixed, so a second run writes the same bytes. Read the
// resulting diff whole before committing it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 3 (R-IOS-01): media
//                                    vector writer. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 3 (R-IOS-01): NSDC and
//                                    Opus vectors, with "after" for the
//                                    stateful decoders. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Part A (R-R3-03, R-IOS-01):
//                                    three NSDC vectors both malformed and
//                                    refused. AI-assisted transformation
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 20 (R-IOS-27): the
//                                    NSDX display extras vectors.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-25: iPhone app Task 71 (R-IOS-02): lan-
//               announcement-2-devices; the trailing vector follows the count.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-28: iPhone app plan Task 25 (R-IOS-16): lan-announcement-2-radio
//               and -waiting; the trailing vector follows the radio state.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-26: transmit group fix wave M9: the "tx" channel keepalive.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 28 (R-R3-49, A11): the
//                                    transmit display's NSDC vector
//                                    (nsdc1-transmit). AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "core/dsp/Ps3Snapshot.h"
#include "core/session/Ps3DisplayCodec.h"
#include "core/safety/RemoteTxWatchdog.h"
#include "core/session/StationLanAnnouncement.h"
#include "core/session/DnsSdAdvertiser.h"
#include "core/session/media/DisplayCodec.h"
#include "core/session/media/DisplayExtras.h"
#include "core/session/media/OpusAudioCodec.h"

#include "LinkFixtures.h"

using namespace NereusSDR;
using namespace NereusSDR::Test;

class TstLinkConformanceRegen : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void writeLanAnnouncement();
    void writeLanAnnouncement2();
    void writeLanAnnouncement2Devices();
    void writeLanAnnouncement2Radio();
    void writeLanAnnouncement2Waiting();
    void writeLanAnnouncement2Trailing();
    void writeDnsSdTxt();
    void writeTxChannelKeepalive();
    void writePs3dFrame();
    void writeNsdcFrames();
    void writeNsdcTransmitFrame();
    void writeNsdxDatagrams();
    void writeOpusPackets();

private:
    bool write(const QString& name, const QByteArray& bytes, const QJsonObject& expectation);
    QTemporaryDir m_scratch;
    QString m_media;
};

void TstLinkConformanceRegen::initTestCase()
{
    const QByteArray requested = qgetenv("NEREUS_LINK_REGEN_OUT");
    const QString root =
        requested.isEmpty() ? m_scratch.path() : QString::fromLocal8Bit(requested);
    QVERIFY2(!root.isEmpty(), "no output directory");
    m_media = QDir(root).filePath(QStringLiteral("media"));
    QVERIFY(QDir().mkpath(m_media));
}

bool TstLinkConformanceRegen::write(const QString& name, const QByteArray& bytes,
                                    const QJsonObject& expectation)
{
    QFile bin(QDir(m_media).filePath(name + QStringLiteral(".bin")));
    QFile expect(QDir(m_media).filePath(name + QStringLiteral(".expect.json")));
    if (!bin.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || !expect.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const QByteArray json = QJsonDocument(expectation).toJson(QJsonDocument::Indented);
    const bool ok = bin.write(bytes) == bytes.size() && expect.write(json) == json.size();
    if (ok && !qEnvironmentVariableIsEmpty("NEREUS_LINK_REGEN_OUT")) {
        qInfo().noquote() << "wrote" << bin.fileName() << "and" << expect.fileName();
    }
    return ok;
}

void TstLinkConformanceRegen::writeLanAnnouncement()
{
    const StationLanAnnouncement value = LinkMediaVectors::lanAnnouncement();
    QString error;
    const QByteArray bytes = encodeStationLanAnnouncement(value, &error);
    QVERIFY2(!bytes.isEmpty(), qPrintable(error));
    QVERIFY(write(QStringLiteral("lan-announcement"), bytes,
                  QJsonObject{{QStringLiteral("codec"), QStringLiteral("nrsc1")},
                              {QStringLiteral("expect"), LinkMediaVectors::toJson(value)}}));
}

void TstLinkConformanceRegen::writeLanAnnouncement2()
{
    const StationLanAnnouncement value = LinkMediaVectors::lanAnnouncement2();
    QString error;
    const QByteArray bytes = encodeStationLanAnnouncement(value, &error);
    QVERIFY2(!bytes.isEmpty(), qPrintable(error));
    QVERIFY(write(QStringLiteral("lan-announcement-2"), bytes,
                  QJsonObject{{QStringLiteral("codec"), QStringLiteral("nrsc1")},
                              {QStringLiteral("expect"), LinkMediaVectors::toJson(value)}}));
}

// iPhone app Task 71 (ruling 10.4): the same Core with the device count
// appended after Pairing.
void TstLinkConformanceRegen::writeLanAnnouncement2Devices()
{
    const StationLanAnnouncement value = LinkMediaVectors::lanAnnouncement2Devices();
    QString error;
    const QByteArray bytes = encodeStationLanAnnouncement(value, &error);
    QVERIFY2(!bytes.isEmpty(), qPrintable(error));
    QVERIFY(write(QStringLiteral("lan-announcement-2-devices"), bytes,
                  QJsonObject{{QStringLiteral("codec"), QStringLiteral("nrsc1")},
                              {QStringLiteral("expect"), LinkMediaVectors::toJson(value)}}));
}

// iPhone app plan Task 25 (R-IOS-16): the counted Core with its radio
// state appended, and the same Core waiting for a radio.
void TstLinkConformanceRegen::writeLanAnnouncement2Radio()
{
    const StationLanAnnouncement value = LinkMediaVectors::lanAnnouncement2Radio();
    QString error;
    const QByteArray bytes = encodeStationLanAnnouncement(value, &error);
    QVERIFY2(!bytes.isEmpty(), qPrintable(error));
    QVERIFY(write(QStringLiteral("lan-announcement-2-radio"), bytes,
                  QJsonObject{{QStringLiteral("codec"), QStringLiteral("nrsc1")},
                              {QStringLiteral("expect"), LinkMediaVectors::toJson(value)}}));
}

void TstLinkConformanceRegen::writeLanAnnouncement2Waiting()
{
    const StationLanAnnouncement value = LinkMediaVectors::lanAnnouncement2Waiting();
    QString error;
    const QByteArray bytes = encodeStationLanAnnouncement(value, &error);
    QVERIFY2(!bytes.isEmpty(), qPrintable(error));
    QVERIFY(write(QStringLiteral("lan-announcement-2-waiting"), bytes,
                  QJsonObject{{QStringLiteral("codec"), QStringLiteral("nrsc1")},
                              {QStringLiteral("expect"), LinkMediaVectors::toJson(value)}}));
}

// Schema 2 extends by appending: the same Core with bytes after its known
// fields (the radio state the last of them), which a reader ignores (link
// document section 14.1).
void TstLinkConformanceRegen::writeLanAnnouncement2Trailing()
{
    const StationLanAnnouncement value = LinkMediaVectors::lanAnnouncement2Radio();
    QString error;
    const QByteArray known = encodeStationLanAnnouncement(value, &error);
    QVERIFY2(!known.isEmpty(), qPrintable(error));
    const QByteArray trailing = LinkMediaVectors::lanAnnouncementTrailingBytes();
    QJsonObject expect = LinkMediaVectors::toJson(value);
    expect.insert(QStringLiteral("ignoredTrailingBytes"), int(trailing.size()));
    QVERIFY(write(QStringLiteral("lan-announcement-2-trailing"), known + trailing,
                  QJsonObject{{QStringLiteral("codec"), QStringLiteral("nrsc1")},
                              {QStringLiteral("expect"), expect}}));
}

// Fix wave M9: the "tx" data channel's keepalive, 13 bytes (link section
// 18.7, remote media control "The \"tx\" data channel").
void TstLinkConformanceRegen::writeTxChannelKeepalive()
{
    const quint64 sequence = 4097;
    const quint32 epoch = 4294967295U;
    const QByteArray bytes = RemoteTxWatchdog::channelKeepalive(sequence, epoch);
    QCOMPARE(bytes.size(), 13);
    QVERIFY(write(QStringLiteral("tx-keepalive"), bytes,
                  QJsonObject{{QStringLiteral("codec"), QStringLiteral("tx-keepalive")},
                              {QStringLiteral("expect"),
                               QJsonObject{{QStringLiteral("sequence"), qint64(sequence)},
                                           {QStringLiteral("epoch"), qint64(epoch)}}}}));
}

void TstLinkConformanceRegen::writeDnsSdTxt()
{
    const DnsSdRecord record = LinkMediaVectors::dnsSdRecord();
    QString error;
    const QByteArray bytes = encodeDnsSdTxtRecord(record, &error);
    QVERIFY2(!bytes.isEmpty(), qPrintable(error));
    QVERIFY(write(QStringLiteral("dnssd-txt"), bytes,
                  QJsonObject{{QStringLiteral("codec"), QStringLiteral("dnssd-txt")},
                              {QStringLiteral("expect"), LinkMediaVectors::toJson(record)}}));
}

void TstLinkConformanceRegen::writePs3dFrame()
{
    const Ps3Snapshot snapshot = LinkMediaVectors::ps3Snapshot();
    QString error;
    const QList<QByteArray> chunks = Ps3DisplayCodec::encode(snapshot, &error);
    QVERIFY2(chunks.size() == 1, qPrintable(error));
    QJsonObject expect = LinkMediaVectors::toJson(snapshot);
    expect.insert(QStringLiteral("tolerance"), QJsonObject{{QStringLiteral("absolute"), 0}});
    QVERIFY(write(QStringLiteral("ps3d-frame"), chunks.first(),
                  QJsonObject{{QStringLiteral("codec"), QStringLiteral("ps3d")},
                              {QStringLiteral("expect"), expect}}));
}

void TstLinkConformanceRegen::writeNsdcFrames()
{
    // One encoder sends frames 1 to 4; frame 3 is lost on the way, so the
    // sender is asked for a keyframe and sends frame 4 as one. Each
    // expectation is what the station's decoder makes of the packet after
    // the packets its "after" names.
    const QList<DisplayCodecFrame> frames = LinkMediaVectors::nsdcFrames();
    QCOMPARE(frames.size(), 4);
    DisplayCodecEncoder encoder;
    const QByteArray full = encoder.encode(frames.at(0));
    const QByteArray delta = encoder.encode(frames.at(1));
    const QByteArray lost = encoder.encode(frames.at(2));
    const QByteArray keyframe = encoder.encode(frames.at(3), /*requestKeyframe=*/true);
    QVERIFY(!full.isEmpty() && !delta.isEmpty() && !lost.isEmpty() && !keyframe.isEmpty());

    const auto expectation = [](const QList<QByteArray>& before, const QByteArray& packet,
                                const QStringList& after) {
        DisplayCodecDecoder decoder;
        for (const QByteArray& earlier : before) {
            decoder.decode(earlier);
        }
        const DisplayCodecDecodeResult result = decoder.decode(packet);
        QJsonObject expect = LinkMediaVectors::toJson(result);
        expect.insert(QStringLiteral("keyframe"), (packet.at(5) & 0x01) != 0);
        if (!after.isEmpty()) {
            expect.insert(QStringLiteral("after"), QJsonArray::fromStringList(after));
        }
        if (result.disposition == DisplayCodecDisposition::Accepted) {
            expect.insert(QStringLiteral("tolerance"),
                          QJsonObject{{QStringLiteral("dbm"), 0.01}});
        }
        return QJsonObject{{QStringLiteral("codec"), QStringLiteral("nsdc1")},
                           {QStringLiteral("expect"), expect}};
    };
    const QStringList afterFull{QStringLiteral("media-nsdc1-full")};
    QVERIFY(write(QStringLiteral("nsdc1-full"), full, expectation({}, full, {})));
    QVERIFY(write(QStringLiteral("nsdc1-delta"), delta, expectation({full}, delta, afterFull)));
    QVERIFY(write(QStringLiteral("nsdc1-delta-after-loss"), lost,
                  expectation({full}, lost, afterFull)));
    QVERIFY(write(QStringLiteral("nsdc1-keyframe-after-loss"), keyframe,
                  expectation({full}, keyframe, afterFull)));

    // Datagrams both malformed and refused: the structure is checked first
    // (display codec document, "State and recovery"), so each rejects as
    // malformed whatever the decoder's state would have refused it for.
    QVERIFY(write(QStringLiteral("nsdc1-malformed-delta"),
                  LinkMediaVectors::nsdcBadPlaneDelta(delta),
                  expectation({}, LinkMediaVectors::nsdcBadPlaneDelta(delta), {})));
    QVERIFY(write(QStringLiteral("nsdc1-malformed-stale-delta"),
                  LinkMediaVectors::nsdcStaleTruncatedDelta(delta),
                  expectation({full}, LinkMediaVectors::nsdcStaleTruncatedDelta(delta),
                              afterFull)));
    QVERIFY(write(QStringLiteral("nsdc1-malformed-keyframe"),
                  LinkMediaVectors::nsdcBadBlockCountKeyframe(keyframe),
                  expectation({full, lost}, LinkMediaVectors::nsdcBadBlockCountKeyframe(keyframe),
                              {QStringLiteral("media-nsdc1-full"),
                               QStringLiteral("media-nsdc1-delta-after-loss")})));
}

void TstLinkConformanceRegen::writeNsdcTransmitFrame()
{
    // Parity Task 28: the first frame of a transmit context, the station
    // encoder's keyframe, as DaemonMediaController sends it.
    DisplayCodecEncoder encoder;
    const QByteArray packet = encoder.encode(LinkMediaVectors::nsdcTransmitFrame(),
                                             /*requestKeyframe=*/true);
    QVERIFY(!packet.isEmpty());
    DisplayCodecDecoder decoder;
    const DisplayCodecDecodeResult result = decoder.decode(packet);
    QJsonObject expect = LinkMediaVectors::toJson(result);
    expect.insert(QStringLiteral("keyframe"), (packet.at(5) & 0x01) != 0);
    expect.insert(QStringLiteral("tolerance"), QJsonObject{{QStringLiteral("dbm"), 0.01}});
    QVERIFY(write(QStringLiteral("nsdc1-transmit"), packet,
                  QJsonObject{{QStringLiteral("codec"), QStringLiteral("nsdc1")},
                              {QStringLiteral("expect"), expect}}));
}

void TstLinkConformanceRegen::writeNsdxDatagrams()
{
    // The display extras datagram (display extras v1) is stateless: each
    // vector decodes on its own against the context its expectation names.
    const DisplayCodecContext context = LinkMediaVectors::nsdxContext();
    const QByteArray full = encodeDisplayExtras(LinkMediaVectors::nsdxFrame(), context);
    const QByteArray floor = encodeDisplayExtras(LinkMediaVectors::nsdxNoiseFloorFrame(), context);
    QVERIFY(!full.isEmpty() && !floor.isEmpty());
    DisplayCodecContext newer = context;
    newer.contextGeneration = 2;

    const auto expectation = [](const QByteArray& bytes, const DisplayCodecContext& against) {
        const DisplayExtrasDecodeResult result = decodeDisplayExtras(bytes, against);
        QJsonObject expect = LinkMediaVectors::toJson(result);
        expect.insert(QStringLiteral("context"), LinkMediaVectors::toJson(against));
        if (result.accepted) {
            expect.insert(QStringLiteral("tolerance"),
                          QJsonObject{{QStringLiteral("dbm"), 0.01}});
        }
        return QJsonObject{{QStringLiteral("codec"), QStringLiteral("nsdx1")},
                           {QStringLiteral("expect"), expect}};
    };
    QVERIFY(write(QStringLiteral("nsdx1-full"), full, expectation(full, context)));
    QVERIFY(write(QStringLiteral("nsdx1-noise-floor"), floor, expectation(floor, context)));
    const QByteArray state =
        encodeDisplayExtras(LinkMediaVectors::nsdxNoiseFloorStateFrame(), context);
    QVERIFY(!state.isEmpty());
    QVERIFY(write(QStringLiteral("nsdx1-noise-floor-state"), state, expectation(state, context)));
    QVERIFY(write(QStringLiteral("nsdx1-other-generation"), full, expectation(full, newer)));
    const QByteArray unknown = LinkMediaVectors::nsdxUnknownSection(full);
    QVERIFY(write(QStringLiteral("nsdx1-unknown-section"), unknown,
                  expectation(unknown, context)));
    const QByteArray truncated = full.chopped(1);
    QVERIFY(write(QStringLiteral("nsdx1-truncated"), truncated,
                  expectation(truncated, context)));
}

void TstLinkConformanceRegen::writeOpusPackets()
{
    // Four consecutive packets from one encoder at the station's settings
    // (OpusAudioCodecConfig: 48 kHz stereo, 1920 samples, 48000 bit/s,
    // fullband, R-R3-21), each as the RTP packet the station sends. The reference
    // PCM is the station's decoder's output decoding them in order.
    OpusAudioEncoder encoder;
    QVERIFY(encoder.isReady());
    OpusAudioDecoder decoder;
    QVERIFY(decoder.isReady());
    QStringList before;
    for (int i = 0; i < LinkMediaVectors::kOpusPackets; ++i) {
        const OpusRtpEncodeResult encoded = encoder.encode(
            LinkMediaVectors::opusInput(i),
            static_cast<quint16>(LinkMediaVectors::kOpusFirstSequence + i),
            LinkMediaVectors::kOpusFirstTimestamp
                + static_cast<quint32>(i * OpusAudioCodecConfig::kFrameSamples),
            LinkMediaVectors::kOpusSsrc);
        QCOMPARE(encoded.status, OpusAudioCodecStatus::Accepted);
        const OpusRtpDecodeResult decoded =
            decoder.decodeRtp(encoded.packet, LinkMediaVectors::kOpusSsrc);
        QCOMPARE(decoded.status, OpusAudioCodecStatus::Accepted);
        QJsonObject expect = LinkMediaVectors::toJson(decoded);
        expect.insert(QStringLiteral("ssrc"), static_cast<qint64>(LinkMediaVectors::kOpusSsrc));
        expect.insert(QStringLiteral("tolerance"), QJsonObject{{QStringLiteral("minSnrDb"), 60}});
        if (!before.isEmpty()) {
            expect.insert(QStringLiteral("after"), QJsonArray::fromStringList(before));
        }
        const QString name = QStringLiteral("opus-%1").arg(i + 1);
        QVERIFY(write(name, encoded.packet,
                      QJsonObject{{QStringLiteral("codec"), QStringLiteral("opus")},
                                  {QStringLiteral("expect"), expect}}));
        before.append(QStringLiteral("media-") + name);
    }
}

QTEST_MAIN(TstLinkConformanceRegen)
#include "tst_link_conformance_regen.moc"
