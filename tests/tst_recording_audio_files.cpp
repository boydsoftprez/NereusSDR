// =================================================================
// tests/tst_recording_audio_files.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. Recording files on the Core:
// WAV and MP3 written by AudioFileWriter and read back by AudioFileReader
// (spec 6.1, V2, V10), a WAV from another tool, a disk that refuses a
// write, and the MP3 encoder's refusal reason.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-15 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtEndian>

#include <cmath>
#include <memory>
#include <vector>

#include "core/recordings/AudioFileReader.h"
#include "core/recordings/AudioFileWriter.h"

using namespace NereusSDR;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kRate = 24000;
constexpr int kSeconds = 10;
constexpr int kFrames = kRate * kSeconds;
constexpr double kToneHz = 1000.0;
constexpr float kAmplitude = 0.5f;  // -6 dBFS
constexpr int kChunkFrames = 480;

// A 1 kHz sine on the left (or only) channel; silence on the right.
std::vector<float> tone(int channels, int frames = kFrames, int rate = kRate)
{
    std::vector<float> pcm(size_t(frames) * size_t(channels), 0.0f);
    for (int i = 0; i < frames; ++i) {
        pcm[size_t(i) * size_t(channels)] =
            kAmplitude * float(std::sin(2.0 * kPi * kToneHz * double(i) / double(rate)));
    }
    return pcm;
}

bool writeAll(AudioFileWriter& writer, const std::vector<float>& pcm, int channels)
{
    const int frames = int(pcm.size() / size_t(channels));
    for (int at = 0; at < frames; at += kChunkFrames) {
        const int n = std::min(kChunkFrames, frames - at);
        if (!writer.write(pcm.data() + size_t(at) * size_t(channels), n)) {
            return false;
        }
    }
    return true;
}

std::vector<float> readAll(AudioFileReader& reader)
{
    std::vector<float> out;
    std::vector<float> buf(size_t(1024) * size_t(reader.channels()));
    while (true) {
        const int got = reader.read(buf.data(), 1024);
        if (got <= 0) {
            break;
        }
        out.insert(out.end(), buf.begin(), buf.begin() + qsizetype(got) * reader.channels());
    }
    return out;
}

float peak(const std::vector<float>& pcm, int channels, int channel)
{
    float p = 0.0f;
    for (size_t i = size_t(channel); i < pcm.size(); i += size_t(channels)) {
        p = std::max(p, std::abs(pcm[i]));
    }
    return p;
}

// The frequency, to 1 Hz, with the most energy between 900 and 1100 Hz
// (or around `nearHz`), over one second from the middle of the channel.
double peakFrequency(const std::vector<float>& pcm, int channels, int channel, int rate,
                     double nearHz = kToneHz)
{
    const size_t frames = pcm.size() / size_t(channels);
    const size_t start = frames / 2 - size_t(rate) / 2;
    double bestHz = 0.0;
    double best = -1.0;
    for (double hz = nearHz - 100.0; hz <= nearHz + 100.0; hz += 1.0) {
        const double w = 2.0 * kPi * hz / double(rate);
        double re = 0.0;
        double im = 0.0;
        for (int i = 0; i < rate; ++i) {
            const double x = pcm[(start + size_t(i)) * size_t(channels) + size_t(channel)];
            re += x * std::cos(w * i);
            im -= x * std::sin(w * i);
        }
        const double mag = re * re + im * im;
        if (mag > best) {
            best = mag;
            bestHz = hz;
        }
    }
    return bestHz;
}

// A file whose disk fills after `limit` bytes: writes past it are cut to
// what fits, then refused. Writes before the limit (the WAV header patch)
// still land.
class FullDiskFile : public QFile {
public:
    FullDiskFile(const QString& path, qint64 limit) : QFile(path), m_limit(limit) {}

protected:
    qint64 writeData(const char* data, qint64 len) override
    {
        const qint64 room = m_limit - pos();
        if (room <= 0) {
            setErrorString(QStringLiteral("No space left on device"));
            return -1;
        }
        return QFile::writeData(data, std::min(len, room));
    }

private:
    qint64 m_limit;
};

}  // namespace

class TestRecordingAudioFiles : public QObject {
    Q_OBJECT

private slots:
    void wavMonoRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("mono.wav"));
        QString error;
        auto writer = AudioFileWriter::open(path, {AudioFileFormat::Wav, kRate, 1, 0}, &error);
        QVERIFY2(writer, qPrintable(error));
        QVERIFY(writeAll(*writer, tone(1), 1));
        QVERIFY(writer->finish());
        QCOMPARE(writer->bytesWritten(), qint64(44 + kFrames * 2));
        writer.reset();

        QFile raw(path);
        QVERIFY(raw.open(QIODevice::ReadOnly));
        const QByteArray header = raw.read(44);
        QCOMPARE(raw.size(), qint64(44 + kFrames * 2));
        QCOMPARE(qFromLittleEndian<quint16>(header.constData() + 22), quint16(1));
        QCOMPARE(qFromLittleEndian<quint32>(header.constData() + 24), quint32(24000));
        QCOMPARE(qFromLittleEndian<quint16>(header.constData() + 34), quint16(16));
        QCOMPARE(qFromLittleEndian<quint32>(header.constData() + 40), quint32(kFrames * 2));

        auto reader = AudioFileReader::open(path, &error);
        QVERIFY2(reader, qPrintable(error));
        QCOMPARE(reader->sampleRateHz(), 24000);
        QCOMPARE(reader->channels(), 1);
        QCOMPARE(reader->lengthFrames(), qint64(240000));
        const std::vector<float> back = readAll(*reader);
        QCOMPARE(back.size(), size_t(240000));
        QVERIFY(std::abs(peak(back, 1, 0) - 0.5f) <= 0.01f);
        QCOMPARE(reader->read(std::vector<float>(16).data(), 16), 0);

        // Seeking lands on the sample written there.
        QVERIFY(reader->seekFrame(12345));
        float one = 0.0f;
        QCOMPARE(reader->read(&one, 1), 1);
        QVERIFY(std::abs(one - back[12345]) < 1e-6f);
    }

    void wavStereoKeepsTheSides()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("stereo.wav"));
        QString error;
        auto writer = AudioFileWriter::open(path, {AudioFileFormat::Wav, kRate, 2, 0}, &error);
        QVERIFY2(writer, qPrintable(error));
        QVERIFY(writeAll(*writer, tone(2), 2));
        QVERIFY(writer->finish());
        writer.reset();

        auto reader = AudioFileReader::open(path, &error);
        QVERIFY2(reader, qPrintable(error));
        QCOMPARE(reader->channels(), 2);
        QCOMPARE(reader->lengthFrames(), qint64(240000));
        const std::vector<float> back = readAll(*reader);
        QVERIFY(std::abs(peak(back, 2, 0) - 0.5f) <= 0.01f);
        QCOMPARE(peak(back, 2, 1), 0.0f);
    }

    void mp3RoundTrip_data()
    {
        QTest::addColumn<int>("channels");
        QTest::addColumn<int>("kbps");
        QTest::newRow("64 kbit/s mono") << 1 << 64;
        QTest::newRow("128 kbit/s stereo") << 2 << 128;
    }

    void mp3RoundTrip()
    {
        if (!AudioFileWriter::mp3Available()) {
            QSKIP("This build has no LAME; tst_recording_audio_files_no_lame covers it.");
        }
        QFETCH(int, channels);
        QFETCH(int, kbps);
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("tone.mp3"));
        QString error;
        auto writer =
            AudioFileWriter::open(path, {AudioFileFormat::Mp3, kRate, channels, kbps}, &error);
        QVERIFY2(writer, qPrintable(error));
        QVERIFY(writeAll(*writer, tone(channels), channels));
        QVERIFY(writer->finish());
        const qint64 bytes = writer->bytesWritten();
        writer.reset();

        const double expected = double(kbps) * 1000.0 / 8.0 * kSeconds;
        QCOMPARE(QFileInfo(path).size(), bytes);
        QVERIFY2(std::abs(double(bytes) - expected) <= 0.05 * expected,
                 qPrintable(QStringLiteral("%1 bytes, expected about %2").arg(bytes).arg(expected)));

        auto reader = AudioFileReader::open(path, &error);
        QVERIFY2(reader, qPrintable(error));
        QCOMPARE(reader->sampleRateHz(), kRate);
        QCOMPARE(reader->channels(), channels);
        QVERIFY2(std::abs(reader->lengthFrames() - qint64(kFrames)) <= 1152,
                 qPrintable(QString::number(reader->lengthFrames())));
        const std::vector<float> back = readAll(*reader);
        QVERIFY(std::abs(qint64(back.size() / size_t(channels)) - qint64(kFrames)) <= 1152);
        QVERIFY(std::abs(peakFrequency(back, channels, 0, kRate) - kToneHz) <= 10.0);
        QVERIFY(std::abs(peak(back, channels, 0) - 0.5f) <= 0.1f);
        if (channels == 2) {
            QVERIFY(peak(back, 2, 1) < 0.05f);
        }
        // Seek to a sample and read on from it.
        QVERIFY(reader->seekFrame(kRate));
        std::vector<float> some(size_t(256) * size_t(channels));
        QCOMPARE(reader->read(some.data(), 256), 256);
    }

    void mp3EncoderRefusalHasAReason()
    {
        if (!AudioFileWriter::mp3Available()) {
            QSKIP("This build has no LAME.");
        }
        QTemporaryDir dir;
        QString error;
        // MP3 cannot carry 23 kHz: refused with a reason before any file.
        const QString path = dir.filePath(QStringLiteral("odd.mp3"));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("^MP3 encoder fails")));
        QVERIFY(!AudioFileWriter::open(path, {AudioFileFormat::Mp3, 23000, 1, 64}, &error));
        QVERIFY(!error.isEmpty());
    }

    void wavFromAnotherToolReads()
    {
        // Written by macOS afconvert (LEI16@48000, one channel): 0.25 s of a
        // 440 Hz tone at 0.25, with an FLLR chunk between fmt and data.
        const QString path =
            QFINDTESTDATA("data/recordings/tone-440hz-48k-mono-16bit-afconvert.wav");
        QVERIFY(!path.isEmpty());
        QString error;
        auto reader = AudioFileReader::open(path, &error);
        QVERIFY2(reader, qPrintable(error));
        QCOMPARE(reader->sampleRateHz(), 48000);
        QCOMPARE(reader->channels(), 1);
        QCOMPARE(reader->lengthFrames(), qint64(12000));
        const std::vector<float> back = readAll(*reader);
        QCOMPARE(back.size(), size_t(12000));
        QVERIFY(std::abs(peak(back, 1, 0) - 0.25f) <= 0.01f);
    }

    void diskThatRefusesLeavesAReadableFile()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("full.wav"));
        // Room for the header, three whole chunks and 500 bytes more.
        const qint64 limit = 44 + 3 * kChunkFrames * 2 + 500;
        auto file = std::make_unique<FullDiskFile>(path, limit);
        QVERIFY(file->open(QIODevice::ReadWrite | QIODevice::Truncate));
        QString error;
        auto writer = AudioFileWriter::openDevice(std::move(file),
                                                  {AudioFileFormat::Wav, kRate, 1, 0}, &error);
        QVERIFY2(writer, qPrintable(error));
        const std::vector<float> pcm = tone(1, kChunkFrames * 6);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("^WAV write refused")));
        int accepted = 0;
        bool refused = false;
        for (int chunk = 0; chunk < 6; ++chunk) {
            if (writer->write(pcm.data() + size_t(chunk) * kChunkFrames, kChunkFrames)) {
                ++accepted;
            } else {
                refused = true;
                break;
            }
        }
        QVERIFY(refused);
        QCOMPARE(accepted, 3);
        QVERIFY(!writer->write(pcm.data(), kChunkFrames));  // stays refused
        QVERIFY(!writer->finish());
        writer.reset();

        auto reader = AudioFileReader::open(path, &error);
        QVERIFY2(reader, qPrintable(error));
        const qint64 kept = 3 * kChunkFrames + 500 / 2;
        QCOMPARE(reader->lengthFrames(), kept);
        const std::vector<float> back = readAll(*reader);
        QCOMPARE(back.size(), size_t(kept));
        for (qint64 i = 0; i < kept; ++i) {
            QVERIFY(std::abs(back[size_t(i)] - pcm[size_t(i)]) <= 1.0f / 16384.0f);
        }
    }

    void cutShortWavReadsToTheEnd()
    {
        // A recording whose header was never fixed (the Core stopped
        // mid-write): sizes zero, frames to the end of the file.
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("cut.wav"));
        {
            QString error;
            auto writer = AudioFileWriter::open(path, {AudioFileFormat::Wav, kRate, 1, 0}, &error);
            QVERIFY2(writer, qPrintable(error));
            QVERIFY(writeAll(*writer, tone(1, 4800), 1));
            QVERIFY(writer->finish());
        }
        QFile raw(path);
        QVERIFY(raw.open(QIODevice::ReadWrite));
        QVERIFY(raw.seek(40));
        const QByteArray zero(4, '\0');
        QCOMPARE(raw.write(zero), qint64(4));
        raw.close();
        QString error;
        auto reader = AudioFileReader::open(path, &error);
        QVERIFY2(reader, qPrintable(error));
        QCOMPARE(reader->lengthFrames(), qint64(4800));
    }

    void notAudioIsRefusedWithAReason()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("text.wav"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(4096, 'x'));
        f.close();
        QString error;
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("^cannot read")));
        QVERIFY(!AudioFileReader::open(path, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!AudioFileReader::open(dir.filePath(QStringLiteral("missing.wav")), &error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestRecordingAudioFiles)
#include "tst_recording_audio_files.moc"
