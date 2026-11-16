// =================================================================
// tests/tst_dsr_save.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. Saving a stretch of the Digital
// Spectrum Recorder (spec 3 "Saving a stretch", 6.3, A13; recording plan
// Task 25). On the Core: a whole-pan save is a WholePan recording as long as
// the bracket and as big as the ring's bytes for it, kept outside the ring;
// a stretch partly gone saves what is kept and says so; nothing kept is
// refused; sound only is slice A's sound over the stretch in the Files
// format (a 1 kHz tone from a carrier kept at 7.100 MHz, played in USB at
// 7.099 MHz); Save sound only from a whole-pan recording; a saved stretch
// opened on the waterfall replays like rewind with the recorder off, and
// closes when slice A goes live. On the desktop, against a fake access:
// SAVE puts up a 10 minute yellow bracket whose ends and middle drag, the
// box shows times, length and sizes and sends the save; SAVE is disabled
// with the reason when the recorder is off; the Recordings window opens a
// whole-pan row on the pan, saves its sound only and shows the pan's range.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code. Recording plan Task 25.
// =================================================================

#include <QtTest/QtTest>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QToolButton>
#include <QtEndian>

#include "core/ConnectionState.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspTypes.h"
#include "core/recordings/RecordingService.h"
#include "core/recordings/RecordingSettings.h"
#include "core/recordings/RecordingStore.h"
#include "core/recordings/dsr/DsrRecorder.h"
#include "core/recordings/dsr/DsrReplay.h"
#include "fakes/FakeRecordingsAccess.h"
#include "gui/SpectrumWidget.h"
#include "gui/recordings/DsrHoursMap.h"
#include "gui/recordings/DsrRewindLink.h"
#include "gui/recordings/DsrSaveBox.h"
#include "gui/recordings/RecordingsWindow.h"
#include "models/RxDspWorker.h"

#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

using namespace NereusSDR;

namespace {

// ---- The Core.
const QDateTime kStartUtc(QDate(2026, 11, 16), QTime(14, 32, 0), QTimeZone::UTC);
constexpr int kRate = 192000;
constexpr int kChunkFrames = 4800;
constexpr int kInSize = bufferSizeForRate(kRate);
constexpr qint64 kKeptCentreHz = 7095000;
constexpr qint64 kCarrierHz = 7100000;
constexpr qint64 kPlayHz = 7099000;          // slice A, USB: the carrier is 1 kHz
constexpr float kCarrierAmplitude = 0.5f;
constexpr qint64 kNs = DsrRingFile::kNsPerSecond;
constexpr qint64 kNsPerMs = 1000000;
constexpr int kSaveWaitMs = 180000;

RecordingVolume volume(const QString& root, bool own)
{
    RecordingVolume v;
    v.rootPath = root;
    v.displayName = QFileInfo(root).fileName();
    v.freeBytes = 500LL * 1000000000LL;
    v.totalBytes = 1000LL * 1000000000LL;
    v.isCoreOwnDisk = own;
    v.isSdCard = false;
    return v;
}

// Slice A on receiver 0 at kPlayHz in USB, 100 to 3000 Hz; a DSP worker
// whose switch rewind sets.
class FakeRadio final : public DsrRewindHost {
public:
    qint64 frequency = kPlayHz;
    RxDspWorker* worker = nullptr;
    std::shared_ptr<DsrReplay> held;

    bool sliceOpen(int slice) const override { return slice == 0; }
    qint64 sliceFrequencyHz(int) const override { return frequency; }
    void setSliceFrequencyHz(int, qint64 hz) override { frequency = hz; }
    int sliceReceiver(int slice) const override { return slice == 0 ? 0 : -1; }
    qint64 receiverCentreHz(int receiver) const override { return receiver == 0 ? kKeptCentreHz : 0; }
    int receiverRateHz(int receiver) const override { return receiver == 0 ? kRate : 0; }
    bool sliceOnDiversity(int) const override { return false; }
    bool feedSlice(int slice, std::shared_ptr<DsrReplay> replay) override
    {
        if (worker == nullptr || slice != 0) {
            return replay == nullptr;
        }
        worker->setSliceReplay(0, replay.get());
        held = std::move(replay);
        return true;
    }
    bool sliceSound(int slice, DsrSoundSettings* out) const override
    {
        if (slice != 0 || out == nullptr) {
            return false;
        }
        out->frequencyHz = frequency;
        out->dspMode = static_cast<int>(DSPMode::USB);
        out->filterLowHz = 100.0;
        out->filterHighHz = 3000.0;
        return true;
    }
};

// 16-bit mono samples of a WAV file (its data chunk), and its rate.
std::vector<qint16> wavSamples(const QString& path, int* rate)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray all = f.readAll();
    if (all.size() < 44 || !all.startsWith("RIFF")) {
        return {};
    }
    *rate = int(qFromLittleEndian<quint32>(all.constData() + 24));
    qsizetype at = 12;
    while (at + 8 <= all.size()) {
        const QByteArray id = all.mid(at, 4);
        const qsizetype len = qsizetype(qFromLittleEndian<quint32>(all.constData() + at + 4));
        if (id == "data") {
            const qsizetype n = std::min(len, all.size() - at - 8) / 2;
            std::vector<qint16> out(static_cast<size_t>(n));
            for (qsizetype k = 0; k < n; ++k) {
                out[size_t(k)] = qFromLittleEndian<qint16>(all.constData() + at + 8 + 2 * k);
            }
            return out;
        }
        at += 8 + len;
    }
    return {};
}

// The strongest tone from 100 Hz to half the rate in 10 Hz steps (Goertzel), after
// `skipSeconds`, its level and the whole signal's.
double toneHz(const std::vector<qint16>& s, int rate, double skipSeconds, double* rms, double* toneShare)
{
    const size_t from = size_t(skipSeconds * rate);
    const double n = double(s.size() - from);
    double energy = 0.0;
    for (size_t k = from; k < s.size(); ++k) {
        energy += double(s[k]) * double(s[k]);
    }
    *rms = std::sqrt(energy / n) / 32768.0;
    double best = 0.0;
    double bestPower = -1.0;
    for (int hz = 100; hz < rate / 2; hz += 10) {
        const double w = 2.0 * std::numbers::pi * hz / rate;
        const double c = 2.0 * std::cos(w);
        double s1 = 0.0;
        double s2 = 0.0;
        for (size_t k = from; k < s.size(); ++k) {
            const double s0 = double(s[k]) + c * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        const double power = s1 * s1 + s2 * s2 - c * s1 * s2;
        if (power > bestPower) {
            bestPower = power;
            best = hz;
        }
    }
    // A pure tone's power over the window is (amplitude * n / 2)^2, and its
    // energy amplitude^2 * n / 2: their ratio is n / 2.
    *toneShare = energy > 0.0 ? bestPower / (energy * n / 2.0) : 0.0;
    return best;
}

// ---- The desktop.
const qint64 kNowMs = QDateTime(QDate(2026, 11, 14), QTime(14, 32, 10), QTimeZone::utc()).toMSecsSinceEpoch();
constexpr int kRowMs = 50;
constexpr qint64 kOwnHistoryMs = 20000;
constexpr int kOwnRows = int(kOwnHistoryMs / kRowMs);
constexpr qint64 kKeptMs = 3LL * 3600LL * 1000LL;
constexpr int kBins = 600;
// 46 MB a minute kept, as the mockup has it.
constexpr double kPanBytesPerMs = 46.0e6 / 60000.0;

QJsonObject dsrDocument(bool enabled, const QString& format = QStringLiteral("mp3"),
                        const QJsonValue& stretch = QJsonValue::Null)
{
    QJsonObject doc = FakeRecordingsAccess::document({}, 1);
    doc.insert(QStringLiteral("dsr"), QJsonObject{{QStringLiteral("enabled"), enabled},
                                                  {QStringLiteral("recording"), enabled},
                                                  {QStringLiteral("reason"), QString()},
                                                  {QStringLiteral("oldestUtcMs"), kNowMs - kKeptMs},
                                                  {QStringLiteral("newestUtcMs"), kNowMs},
                                                  {QStringLiteral("usedBytes"), kPanBytesPerMs * double(kKeptMs)},
                                                  {QStringLiteral("rewound"), QJsonArray()},
                                                  {QStringLiteral("stretch"), stretch}});
    QJsonObject settings = doc.value(QStringLiteral("settings")).toObject();
    settings.insert(QStringLiteral("format"), format);
    doc.insert(QStringLiteral("settings"), settings);
    return doc;
}

void mouse(QWidget* w, QEvent::Type type, const QPoint& pos, Qt::MouseButtons buttons)
{
    QMouseEvent ev(type, QPointF(pos), w->mapToGlobal(QPointF(pos)), Qt::LeftButton, buttons, Qt::NoModifier);
    QApplication::sendEvent(w, &ev);
}

void drag(QWidget* w, const QPoint& from, const QPoint& to)
{
    mouse(w, QEvent::MouseButtonPress, from, Qt::LeftButton);
    mouse(w, QEvent::MouseMove, to, Qt::LeftButton);
    mouse(w, QEvent::MouseButtonRelease, to, Qt::NoButton);
}

QList<FakeRecordingsAccess::Call> callsOf(const FakeRecordingsAccess& fake, const QString& verb)
{
    QList<FakeRecordingsAccess::Call> out;
    for (const auto& c : fake.calls) {
        if (c.verb == verb) {
            out.append(c);
        }
    }
    return out;
}

QString hm(qint64 utcMs)
{
    return QDateTime::fromMSecsSinceEpoch(utcMs, QTimeZone::utc()).toString(QStringLiteral("HH:mm"));
}

}  // namespace

class TestDsrSave : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    std::atomic<qint64> m_clockMs{0};
    double m_carrierPhase = 0.0;
    qint64 m_wfClockMs = kNowMs;

    QString ssdRoot() const
    {
        return m_dir.filePath(QStringLiteral("ssd-") + QString::fromLatin1(QTest::currentTestFunction()));
    }
    QString recordingsRoot() const
    {
        return m_dir.filePath(QStringLiteral("rec-") + QString::fromLatin1(QTest::currentTestFunction()));
    }

    // The recorder on a USB SSD at 1 GB, running; recordings saved as WAV.
    FakeRadio* prepare(RecordingService& svc)
    {
        QDir().mkpath(ssdRoot());
        m_clockMs = kStartUtc.toMSecsSinceEpoch();
        m_carrierPhase = 0.0;
        svc.setClockForTest([this] { return m_clockMs.load(); });
        svc.setDsrVolumesForTest([this] {
            return QList<RecordingVolume>{volume(QDir::rootPath(), true), volume(ssdRoot(), false)};
        });
        RecordingSettings s = svc.settings();
        s.disk = recordingsRoot();
        QDir().mkpath(s.disk);
        s.format = AudioFileFormat::Wav;
        s.dsrEnabled = true;
        s.dsrDisk = ssdRoot();
        s.dsrSizeLimitGb = 1;
        svc.setSettings(s);
        auto radio = std::make_unique<FakeRadio>();
        FakeRadio* const raw = radio.get();
        svc.setDsrRewindHostForTest(std::move(radio));
        svc.setRadioStreaming(true);
        svc.dsrRecorder()->setStream(0, kKeptCentreHz, kRate);
        return raw;
    }

    void keepCarrier(RecordingService& svc, int seconds)
    {
        const double step = 2.0 * std::numbers::pi * double(kCarrierHz - kKeptCentreHz) / double(kRate);
        std::vector<float> chunk(static_cast<size_t>(kChunkFrames) * 2);
        for (int sec = 0; sec < seconds; ++sec) {
            for (int c = 0; c < kRate / kChunkFrames; ++c) {
                for (int k = 0; k < kChunkFrames; ++k) {
                    chunk[size_t(2 * k)] = kCarrierAmplitude * float(std::cos(m_carrierPhase));
                    // As the radio sends I/Q (and WDSP's shift, which moves
                    // up by freq - centre, expects): a signal above the
                    // centre turns the other way.
                    chunk[size_t(2 * k + 1)] = -kCarrierAmplitude * float(std::sin(m_carrierPhase));
                    m_carrierPhase = std::fmod(m_carrierPhase + step, 2.0 * std::numbers::pi);
                }
                svc.dsrRecorder()->consumeIq(0, chunk.data(), kChunkFrames);
            }
            m_clockMs += 1000;
            svc.pumpForTest();
        }
    }

    // Saves and waits for the store to list it; its id.
    QString saveAndWait(RecordingService& svc, qint64 from, qint64 to, bool wholePan, QString* note)
    {
        QSignalSpy saved(&svc, &RecordingService::stretchSaved);
        QSignalSpy failed(&svc, &RecordingService::stretchSaveFailed);
        QString why;
        if (!svc.saveStretch(from, to, wholePan, note, &why)) {
            qWarning() << "refused:" << why;
            return {};
        }
        // Long enough for WDSP's first channel in a test run to plan its
        // FFTs without the wisdom file a real Core has.
        if (!QTest::qWaitFor([&] { return saved.count() + failed.count() > 0; }, kSaveWaitMs) || saved.isEmpty()) {
            return {};
        }
        return saved.first().first().toString();
    }

    // A shown pan, rewound 10 rows into its own history, on a fake access
    // with the recorder in use.
    void prepareDesktop(SpectrumWidget& w, DsrRewindLink& link, FakeRecordingsAccess& fake, bool enabled = true)
    {
        m_wfClockMs = kNowMs - kOwnHistoryMs;
        w.setWaterfallClockForTest([this] { return m_wfClockMs; });
        w.setConnectionState(ConnectionState::Connected);
        w.setWaterfallTickerPausedForTest(true);
        w.setWfUpdatePeriodMs(kRowMs);
        w.setWaterfallHistoryMs(kOwnHistoryMs);
        w.setFrequencyRange(7185000.0, 192000.0);
        w.resize(1000, 600);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.pushWaterfallRowForTest(QVector<float>(kBins, -130.0f));
        QTRY_VERIFY(!w.waterfallHistoryResizePendingForTest());
        for (int i = 1; i < kOwnRows; ++i) {
            m_wfClockMs += kRowMs;
            w.pushWaterfallRowForTest(QVector<float>(kBins, -130.0f));
        }
        fake.version = 4;
        fake.setDocument(dsrDocument(enabled));
        link.setAccess(&fake);
        w.setWaterfallHistoryOffsetForTest(10);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.info=false\nnereus.*.debug=false"));
    }

    // ── The Core ────────────────────────────────────────────────────────

    // Whole pan: a WholePan recording as long as the bracket (within 1 s)
    // and as big as the ring's bytes for it, in Recordings, outside the
    // ring; it reads back as the kept I/Q, the pan's range on its entry.
    void aWholePanSaveIsTheStretch()
    {
        RecordingService svc(nullptr);
        prepare(svc);
        keepCarrier(svc, 20);
        const DsrRingFile& ring = svc.dsrRecorder()->ring();
        const qint64 from = ring.oldestUtcNs(0) + 5 * kNs;
        const qint64 to = from + 10 * kNs;
        const DsrStretchExtent extent = DsrStretch::extent(ring, from, to);
        QVERIFY(extent.kept());
        QString note;
        const QString id = saveAndWait(svc, from, to, true, &note);
        QVERIFY(!id.isEmpty());
        QVERIFY2(note.startsWith(QStringLiteral("Saving ")) && note.endsWith(QStringLiteral("whole pan.")),
                 qPrintable(note));
        QCOMPARE(svc.savesInProgress(), 0);
        const std::optional<RecordingEntry> e = svc.store()->find(id);
        QVERIFY(e.has_value());
        QCOMPARE(e->kind, RecordingKind::WholePan);
        QVERIFY2(std::llabs(e->lengthMs - 10000) <= 1000, qPrintable(QString::number(e->lengthMs)));
        QCOMPARE(e->sizeBytes, extent.bytes);
        QCOMPARE(e->centreHz, kKeptCentreHz);
        QCOMPARE(e->spanHz, qint64(kRate));
        QCOMPARE(e->frequencyHz, kPlayHz);
        const QString path = QDir(svc.store()->folder()).filePath(e->fileName);
        QCOMPARE(QFileInfo(path).size(), extent.bytes);
        QVERIFY(!QFileInfo(path).absoluteFilePath().startsWith(QFileInfo(ssdRoot()).absoluteFilePath()));
        // It reads back as the kept I/Q.
        DsrRingFile saved;
        QString error;
        QVERIFY2(saved.open(path, DsrStretchCopy::kNeverWrapBytes, &error), qPrintable(error));
        std::vector<qint16> a(2048);
        std::vector<qint16> b(2048);
        const qint64 at = from + 2 * kNs;
        QCOMPARE(saved.read(0, at, a.data(), 1024, nullptr), 1024);
        QCOMPARE(ring.read(0, at, b.data(), 1024, nullptr), 1024);
        QVERIFY(a == b);
        // The ring keeps going; the saved file does not change.
        keepCarrier(svc, 3);
        QCOMPARE(QFileInfo(path).size(), extent.bytes);
    }

    // A stretch partly older than the ring keeps saves only what is kept,
    // and says so; nothing kept is refused.
    void aStretchPartlyGoneSavesWhatIsKept()
    {
        RecordingService svc(nullptr);
        prepare(svc);
        keepCarrier(svc, 8);
        const DsrRingFile& ring = svc.dsrRecorder()->ring();
        const qint64 oldest = ring.oldestUtcNs(0);
        QString note;
        const QString id = saveAndWait(svc, oldest - 60 * kNs, oldest + 5 * kNs, true, &note);
        QVERIFY(!id.isEmpty());
        QVERIFY2(note.startsWith(QStringLiteral("Only ")), qPrintable(note));
        const std::optional<RecordingEntry> e = svc.store()->find(id);
        QVERIFY(e.has_value());
        QVERIFY2(std::llabs(e->lengthMs - 5000) <= 1000, qPrintable(QString::number(e->lengthMs)));

        QString why;
        QVERIFY(!svc.saveStretch(oldest - 600 * kNs, oldest - 300 * kNs, true, &note, &why));
        QCOMPARE(why, QString::fromLatin1(RecordingService::kSaveNothingKeptReason));
    }

    // Sound only: slice A's sound over the stretch, in the Files format
    // (WAV here), the right length, the past carrier a 1 kHz tone.
    void soundOnlyIsSliceASound()
    {
#ifndef HAVE_WDSP
        QSKIP("Built without WDSP.");
#else
        RecordingService svc(nullptr);
        prepare(svc);
        keepCarrier(svc, 8);
        const DsrRingFile& ring = svc.dsrRecorder()->ring();
        const qint64 from = ring.oldestUtcNs(0) + kNs;
        const qint64 to = from + 5 * kNs;
        QString note;
        const QString id = saveAndWait(svc, from, to, false, &note);
        QVERIFY(!id.isEmpty());
        QVERIFY2(note.endsWith(QStringLiteral("sound only as WAV.")), qPrintable(note));
        const std::optional<RecordingEntry> e = svc.store()->find(id);
        QVERIFY(e.has_value());
        QCOMPARE(e->kind, RecordingKind::Sound);
        QCOMPARE(e->sliceId, 0);
        QCOMPARE(e->frequencyHz, kPlayHz);
        QVERIFY(e->fileName.endsWith(QStringLiteral(".wav")));
        QVERIFY2(std::llabs(e->lengthMs - 5000) <= 1000, qPrintable(QString::number(e->lengthMs)));
        int rate = 0;
        const std::vector<qint16> s = wavSamples(QDir(svc.store()->folder()).filePath(e->fileName), &rate);
        QVERIFY(!s.empty());
        double rms = 0.0;
        double share = 0.0;
        const double hz = toneHz(s, rate, 2.0, &rms, &share);
        QVERIFY2(rms > 1e-3, qPrintable(QStringLiteral("rms %1").arg(rms)));
        // Most of the sound is that one tone.
        QVERIFY2(std::abs(hz - 1000.0) <= 10.0 && share > 0.5,
                 qPrintable(QStringLiteral("tone %1 Hz, %2 of the sound").arg(hz).arg(share)));
#endif
    }

    // Save sound only from a whole-pan recording makes a Sound recording of
    // its length; asked of a sound recording it is refused.
    void saveSoundOnlyFromAWholePan()
    {
#ifndef HAVE_WDSP
        QSKIP("Built without WDSP.");
#else
        RecordingService svc(nullptr);
        prepare(svc);
        keepCarrier(svc, 8);
        const DsrRingFile& ring = svc.dsrRecorder()->ring();
        const qint64 from = ring.oldestUtcNs(0) + kNs;
        const QString pan = saveAndWait(svc, from, from + 4 * kNs, true, nullptr);
        QVERIFY(!pan.isEmpty());
        QSignalSpy saved(&svc, &RecordingService::stretchSaved);
        QString note;
        QString why;
        QVERIFY2(svc.saveSoundOnly(pan, &note, &why), qPrintable(why));
        QVERIFY2(note.startsWith(QStringLiteral("Saving the sound of 7.099.000")), qPrintable(note));
        QTRY_COMPARE_WITH_TIMEOUT(saved.count(), 1, kSaveWaitMs);
        const std::optional<RecordingEntry> e = svc.store()->find(saved.first().first().toString());
        QVERIFY(e.has_value());
        QCOMPARE(e->kind, RecordingKind::Sound);
        QVERIFY2(std::llabs(e->lengthMs - 4000) <= 1000, qPrintable(QString::number(e->lengthMs)));
        QVERIFY(!svc.saveSoundOnly(e->id, &note, &why));
        QCOMPARE(why, QString::fromLatin1(RecordingService::kSaveNotWholePanReason));
        QVERIFY(!svc.saveSoundOnly(QStringLiteral("missing"), &note, &why));
        QCOMPARE(why, QString::fromLatin1(RecordingService::kPlayGoneReason));
#endif
    }

    // A saved stretch opened on the waterfall: slice A plays it like rewind
    // from its start, with the recorder off; the document names it; LIVE
    // closes it.
    void anOpenedStretchReplaysWithTheRecorderOff()
    {
        RecordingService svc(nullptr);
        FakeRadio* radio = prepare(svc);
        RxDspWorker worker;
        worker.setBufferSizes(kInSize, 64);
        worker.setStreamSlices(0, {0});
        radio->worker = &worker;
        keepCarrier(svc, 8);
        const DsrRingFile& ring = svc.dsrRecorder()->ring();
        const qint64 from = ring.oldestUtcNs(0) + kNs;
        const QString id = saveAndWait(svc, from, from + 4 * kNs, true, nullptr);
        QVERIFY(!id.isEmpty());

        RecordingSettings s = svc.settings();
        s.dsrEnabled = false;
        svc.setSettings(s);
        svc.pumpForTest();
        QString why;
        QVERIFY2(svc.openStretch(id, &why), qPrintable(why));
        QCOMPARE(svc.openStretchId(), id);
        const DsrStatus st = svc.dsrStatus();
        QCOMPARE(st.stretchId, id);
        QVERIFY(std::llabs(st.stretchFromUtcNs - from) < kNs);
        const QList<DsrRewoundSlice> rewound = svc.rewound();
        QCOMPARE(rewound.size(), 1);
        QCOMPARE(rewound.first().slice, 0);
        QCOMPARE(radio->frequency, kPlayHz);
        svc.pumpForTest();
        QVERIFY(worker.sliceReplay(0) != nullptr);
        const std::shared_ptr<DsrReplay> replay = svc.replayForTest(0);
        QVERIFY(replay);
        QVERIFY(replay->primed());

        QVERIFY(!svc.openStretch(QStringLiteral("missing"), &why));
        QCOMPARE(why, QString::fromLatin1(RecordingService::kPlayGoneReason));

        QVERIFY2(svc.goLive(0, &why), qPrintable(why));
        QVERIFY(svc.openStretchId().isEmpty());
        QVERIFY(svc.dsrStatus().stretchId.isEmpty());
        radio->worker = nullptr;
    }

    // ── The desktop ─────────────────────────────────────────────────────

    // SAVE above the map puts up a 10 minute bracket ending at most a second
    // ago, and the box beside it; the ends and the middle drag; Save sends
    // the bracket; the Core's note shows; SAVE again takes it down.
    void saveShowsTheBracketAndTheBox()
    {
        SpectrumWidget w;
        FakeRecordingsAccess fake;
        DsrRewindLink link(&w, [] { return 0; });
        prepareDesktop(w, link, fake);
        if (QTest::currentTestFailed()) {
            return;
        }
        auto* map = qobject_cast<DsrHoursMap*>(w.dsrHoursMapForTest());
        auto* save = qobject_cast<QPushButton*>(w.dsrSaveButtonForTest());
        QVERIFY(map != nullptr && save != nullptr);
        QVERIFY(save->isVisible());
        QVERIFY(save->isEnabled());
        QCOMPARE(save->text(), QStringLiteral("SAVE"));
        QCOMPARE(save->geometry(), QRect(map->x(), w.waterfallRectForTest().top() + 2, DsrHoursMap::kWidth, 18));
        QCOMPARE(save->toolTip(), QStringLiteral("Save a stretch to Recordings before it is overwritten"));

        save->click();
        QVERIFY(map->hasBracket());
        const qint64 now = m_wfClockMs;
        const qint64 to = std::min(now - 1000, w.waterfallTopUtcMs() + 600000);
        QCOMPARE(map->bracketToUtcMs(), to);
        QCOMPARE(map->bracketFromUtcMs(), to - 600000);
        auto* box = qobject_cast<DsrSaveBox*>(w.dsrSaveBoxForTest());
        QVERIFY(box != nullptr && box->isVisible());
        QCOMPARE(box->x() + box->width() + DsrSaveBox::kGapPx, map->x());
        QCOMPARE(box->findChild<QLabel*>(QStringLiteral("DsrSaveHeader"))->text(),
                 QStringLiteral("Save %1 to %2 UTC").arg(hm(to - 600000), hm(to)));
        QCOMPARE(box->findChild<QLabel*>(QStringLiteral("DsrSaveDetail"))->text(),
                 QStringLiteral("10 min · whole pan about 460 MB, sound only about 5 MB"));
        QCOMPARE(box->findChild<QPushButton*>(QStringLiteral("DsrSaveWholePan"))->text(),
                 QStringLiteral("Save stretch (whole pan)"));
        QCOMPARE(box->findChild<QPushButton*>(QStringLiteral("DsrSaveSound"))->text(),
                 QStringLiteral("Save sound only (MP3)"));

        // The bottom edge (its start) drags older, the top edge is held
        // under now, and the middle moves it whole.
        QRect r = map->bracketRect();
        drag(map, QPoint(map->width() / 2, r.bottom() - 2), QPoint(map->width() / 2, r.bottom() + 40));
        QVERIFY(map->bracketFromUtcMs() < to - 600000);
        QCOMPARE(map->bracketToUtcMs(), to);
        const qint64 len = map->bracketToUtcMs() - map->bracketFromUtcMs();
        r = map->bracketRect();
        drag(map, QPoint(map->width() / 2, r.center().y()), QPoint(map->width() / 2, r.center().y() + 30));
        QVERIFY(map->bracketToUtcMs() < to);
        QCOMPARE(map->bracketToUtcMs() - map->bracketFromUtcMs(), len);
        r = map->bracketRect();
        drag(map, QPoint(map->width() / 2, r.top() + 2), QPoint(map->width() / 2, r.top() - 400));
        QCOMPARE(map->bracketToUtcMs(), now - 1000);
        QCOMPARE(box->fromUtcMs(), map->bracketFromUtcMs());
        QCOMPARE(box->toUtcMs(), map->bracketToUtcMs());
        // Never shorter than 30 seconds.
        r = map->bracketRect();
        drag(map, QPoint(map->width() / 2, r.bottom() - 2), QPoint(map->width() / 2, r.top() - 50));
        QCOMPARE(map->bracketToUtcMs() - map->bracketFromUtcMs(), DsrHoursMap::kMinBracketMs);

        // Save: the bracket goes to the Core, the bracket and box come down.
        const qint64 savedFrom = map->bracketFromUtcMs();
        const qint64 savedTo = map->bracketToUtcMs();
        box->findChild<QPushButton*>(QStringLiteral("DsrSaveWholePan"))->click();
        const auto saves = callsOf(fake, QStringLiteral("saveStretch"));
        QCOMPARE(saves.size(), 1);
        QCOMPARE(saves.first().sliceId, 1);
        QCOMPARE(saves.first().fromUtcNs, savedFrom * kNsPerMs);
        QCOMPARE(saves.first().utcNs, savedTo * kNsPerMs);
        QVERIFY(!map->hasBracket());
        QVERIFY(!box->isVisible());
        emit fake.stretchSaving(QStringLiteral("Saving 14:00 to 14:10 UTC to Recordings, whole pan."));
        QCOMPARE(w.dsrNoteForTest(), QStringLiteral("Saving 14:00 to 14:10 UTC to Recordings, whole pan."));

        // Sound only as WAV per the Files setting; SAVE again cancels.
        fake.setDocument(dsrDocument(true, QStringLiteral("wav")));
        save->click();
        QCOMPARE(box->findChild<QPushButton*>(QStringLiteral("DsrSaveSound"))->text(),
                 QStringLiteral("Save sound only (WAV)"));
        QVERIFY(box->findChild<QLabel*>(QStringLiteral("DsrSaveDetail"))->text().endsWith(
            QStringLiteral("sound only about 29 MB")));
        box->findChild<QPushButton*>(QStringLiteral("DsrSaveSound"))->click();
        QCOMPARE(callsOf(fake, QStringLiteral("saveStretch")).last().sliceId, 0);
        save->click();
        QVERIFY(map->hasBracket());
        save->click();
        QVERIFY(!map->hasBracket());
        QVERIFY(!box->isVisible());
        QCOMPARE(callsOf(fake, QStringLiteral("saveStretch")).size(), 2);

        // A refusal shows; going live takes the bracket down.
        save->click();
        box->findChild<QPushButton*>(QStringLiteral("DsrSaveWholePan"))->click();
        emit fake.refused(QStringLiteral("None of that stretch is still kept."));
        QCOMPARE(w.dsrNoteForTest(), QStringLiteral("None of that stretch is still kept."));
        save->click();
        QVERIFY(map->hasBracket());
        w.returnWaterfallToLive();
        QVERIFY(!map->hasBracket());
        QVERIFY(!save->isVisible());
    }

    // The recorder off (or an older Core): SAVE stays, disabled with the
    // reason; a saved stretch that is open makes it usable again.
    void saveIsDisabledWithTheReasonWhenTheRecorderIsOff()
    {
        SpectrumWidget w;
        FakeRecordingsAccess fake;
        DsrRewindLink link(&w, [] { return 0; });
        prepareDesktop(w, link, fake, false);
        if (QTest::currentTestFailed()) {
            return;
        }
        auto* save = qobject_cast<QPushButton*>(w.dsrSaveButtonForTest());
        QVERIFY(save != nullptr);
        QVERIFY(save->isVisible());
        QVERIFY(!save->isEnabled());
        QCOMPARE(save->toolTip(), DsrRewindLink::recorderOffReason());
        w.toggleDsrSaveBracket();
        QVERIFY(!qobject_cast<DsrHoursMap*>(w.dsrHoursMapForTest())->hasBracket());

        const qint64 stretchFromMs = kNowMs - 2 * kKeptMs;
        fake.setDocument(dsrDocument(false, QStringLiteral("mp3"),
                                     QJsonObject{{QStringLiteral("id"), QStringLiteral("w1")},
                                                 {QStringLiteral("fromUtcNs"), stretchFromMs * kNsPerMs},
                                                 {QStringLiteral("toUtcNs"), (stretchFromMs + 600000) * kNsPerMs}}));
        QVERIFY(w.dsrRewindAvailable());
        QVERIFY(save->isEnabled());
        QVERIFY(qobject_cast<DsrHoursMap*>(w.dsrHoursMapForTest())->isEnabled());

        fake.version = 3;
        fake.setDocument(dsrDocument(true));
        QVERIFY(!save->isEnabled());
        QCOMPARE(save->toolTip(), RecordingsAccess::needsNewerCoreReason());
    }

    // The Recordings window: a whole-pan row shows the pan's range, opens
    // on the pan (row play, Play), saves its sound only, and says what the
    // Core said.
    void theWindowOpensAndSavesSoundFromAWholePan()
    {
        RecordingEntry pan;
        pan.id = QStringLiteral("w1");
        pan.kind = RecordingKind::WholePan;
        pan.sliceId = -1;
        pan.startUtc = QDateTime::fromMSecsSinceEpoch(kNowMs - 3600000, QTimeZone::utc());
        pan.lengthMs = 600000;
        pan.frequencyHz = 7185000;
        pan.centreHz = 7185000;
        pan.spanHz = 192000;
        pan.mode = QStringLiteral("LSB");
        pan.name = QStringLiteral("pan");
        pan.fileName = QStringLiteral("pan.ndsr");
        pan.sizeBytes = 460000000;
        RecordingEntry sound = pan;
        sound.id = QStringLiteral("s1");
        sound.kind = RecordingKind::Sound;
        sound.sliceId = 0;
        sound.spanHz = 0;
        sound.fileName = QStringLiteral("s.mp3");
        FakeRecordingsAccess fake;
        fake.version = 4;
        fake.entries = {pan, sound};
        fake.setDocument(FakeRecordingsAccess::document({}, 1, QStringLiteral("/Recordings")));
        RecordingsWindow window(&fake);
        QTRY_COMPARE(window.tableForTest()->rowCount(), 2);
        QCOMPARE(window.tableForTest()->item(0, RecordingsWindow::ColFrequency)->text(),
                 QStringLiteral("7.089 to 7.281"));
        QCOMPARE(window.tableForTest()->item(1, RecordingsWindow::ColFrequency)->text(),
                 QStringLiteral("7.185.000"));

        auto* rowPlay = window.tableForTest()->cellWidget(0, RecordingsWindow::ColPlay)->findChild<QToolButton*>();
        QVERIFY(rowPlay != nullptr && rowPlay->isEnabled());
        QCOMPARE(rowPlay->toolTip(), RecordingsWindow::openOnPanTip());
        fake.calls.clear();
        rowPlay->click();
        QCOMPARE(fake.verbs(), QStringList{QStringLiteral("openStretch")});
        QCOMPARE(fake.calls.last().id, QStringLiteral("w1"));

        window.setSelectedIds({QStringLiteral("w1")});
        QVERIFY(window.playButtonForTest()->isEnabled());
        QCOMPARE(window.playButtonForTest()->toolTip(), RecordingsWindow::openOnPanTip());
        QVERIFY(window.soundOnlyButtonForTest()->isEnabled());
        fake.calls.clear();
        window.playButtonForTest()->click();
        window.soundOnlyButtonForTest()->click();
        QCOMPARE(fake.verbs(), (QStringList{QStringLiteral("openStretch"), QStringLiteral("saveSoundOnly")}));
        QCOMPARE(fake.calls.last().id, QStringLiteral("w1"));
        emit fake.stretchSaving(QStringLiteral("Saving the sound of 7.185.000 from that stretch to Recordings as MP3."));
        QCOMPARE(window.statusForTest()->text(),
                 QStringLiteral("Saving the sound of 7.185.000 from that stretch to Recordings as MP3."));

        // A sound recording has no sound to save; an older Core says so.
        window.setSelectedIds({QStringLiteral("s1")});
        QVERIFY(!window.soundOnlyButtonForTest()->isEnabled());
        fake.version = 3;
        fake.setDocument(FakeRecordingsAccess::document({}, 2, QStringLiteral("/Recordings")));
        window.setSelectedIds({QStringLiteral("w1")});
        QVERIFY(!window.soundOnlyButtonForTest()->isEnabled());
        QCOMPARE(window.soundOnlyButtonForTest()->toolTip(), RecordingsAccess::needsNewerCoreReason());
    }
};

QTEST_MAIN(TestDsrSave)
#include "tst_dsr_save.moc"
