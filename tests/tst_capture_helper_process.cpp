// =================================================================
// tests/tst_capture_helper_process.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Process-level tests of the
// nereus-audio-capture helper (R-R3-36): the real helper re-executed from
// this binary with --capture-helper, and the scripted fake re-executed
// with --fake-capture-child <scenario>.  No test opens a real microphone:
// the helper child is a test run, so it never initialises PortAudio and
// answers from a test device list.
//
// Modification history (NereusSDR):
//   2026-09-22: J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-24: R-R3-21: the helper child answers from a test device list
//               and never initialises PortAudio. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QElapsedTimer>
#include <QProcess>

#include <cstring>
#include <optional>

#include "core/audio/CaptureProtocol.h"
#include "fakes/FakeCaptureChild.h"

// Exercise the real entry point, as tst_daemon_signals does for nereusd.
#define main captureHelperEntryPointForTest
#include "../src/capture_main.cpp"
#undef main

using namespace NereusSDR;
namespace P = NereusSDR::CaptureProtocol;

namespace {

const QString kMissingDevice = QStringLiteral("NereusSDR test device that does not exist");
// R-R3-21: the helper child is this binary re-run, so it is a test run too
// and never initialises PortAudio. main() hands it this device list, which
// it answers from in place of the computer's real devices.
const QString kListedDevice = QStringLiteral("NereusSDR test microphone");

// One child process plus an incremental record reader over its stdout.
class Child {
public:
    ~Child()
    {
        if (m_process.state() != QProcess::NotRunning) {
            m_process.kill();
            m_process.waitForFinished(3000);
        }
    }

    bool start(const QStringList& arguments)
    {
        m_process.start(QCoreApplication::applicationFilePath(), arguments);
        return m_process.waitForStarted(5000);
    }

    // Next complete record, or nullopt when none arrives within timeoutMs
    // or the stream is malformed.
    std::optional<P::Record> next(int timeoutMs)
    {
        QElapsedTimer timer;
        timer.start();
        for (;;) {
            if (auto record = m_reader.next()) {
                return record;
            }
            if (m_reader.error() != P::RecordReader::Error::None) {
                return std::nullopt;
            }
            const qint64 left = timeoutMs - timer.elapsed();
            if (left <= 0) {
                return std::nullopt;
            }
            if (m_process.bytesAvailable() == 0) {
                if (m_process.state() == QProcess::NotRunning) {
                    return std::nullopt;
                }
                m_process.waitForReadyRead(static_cast<int>(qMin<qint64>(left, 50)));
            }
            const QByteArray bytes = m_process.readAllStandardOutput();
            m_reader.append(bytes.constData(), bytes.size());
        }
    }

    std::optional<P::Status> nextStatus(int timeoutMs)
    {
        const auto record = next(timeoutMs);
        if (!record || record->type != P::RecordType::Status) {
            return std::nullopt;
        }
        return P::decodeStatus(record->payload);
    }

    void send(const QByteArray& record) { m_process.write(record); m_process.waitForBytesWritten(1000); }

    bool finishes(int timeoutMs) { return m_process.waitForFinished(timeoutMs); }

    P::RecordReader::Error readerError() const { return m_reader.error(); }
    QProcess& process() { return m_process; }
    QByteArray diagnostics() { return m_process.readAllStandardError(); }

private:
    QProcess m_process;
    P::RecordReader m_reader;
};

QByteArray configureRecord(quint32 generation, const QString& deviceName)
{
    P::Configure configure;
    configure.generation = generation;
    configure.device.deviceName = deviceName;
    return P::encodeConfigure(configure);
}

bool nextStateIs(Child& child, P::HelperState state, int timeoutMs)
{
    const auto status = child.nextStatus(timeoutMs);
    return status.has_value() && status->state == state;
}

bool nextTypeIs(Child& child, P::RecordType type, int timeoutMs)
{
    const auto record = child.next(timeoutMs);
    return record.has_value() && record->type == type;
}

QByteArray openRecord(quint32 generation) { return P::encodeOpen(P::Command{generation}); }
QByteArray stopRecord(quint32 generation) { return P::encodeStop(P::Command{generation}); }

} // namespace

class TstCaptureHelperProcess : public QObject {
    Q_OBJECT
private slots:
    void helperSendsHelloFirstWithinDeadline()
    {
        Child helper;
        QElapsedTimer timer;
        timer.start();
        QVERIFY(helper.start({QStringLiteral("--capture-helper")}));
        const auto record = helper.next(3000);
        const qint64 elapsed = timer.elapsed();
        QVERIFY2(record.has_value(), helper.diagnostics().constData());
        QCOMPARE(record->type, P::RecordType::Hello);
        const auto hello = P::decodeHello(record->payload);
        QVERIFY(hello.has_value());
        QCOMPARE(hello->protocol, 1);
        QCOMPARE(hello->pid, helper.process().processId());
        QVERIFY(!hello->build.isEmpty());
        QVERIFY2(elapsed < 3000, qPrintable(QStringLiteral("hello after %1 ms").arg(elapsed)));
        qInfo("hello after %lld ms, build %s", elapsed, qPrintable(hello->build));
    }

    // R-R3-21: a device on the test list is found but never opened: a test
    // run touches no real audio device.
    void helperInATestRunOpensNoDevice()
    {
        Child helper;
        QVERIFY(helper.start({QStringLiteral("--capture-helper")}));
        QVERIFY(helper.next(3000).has_value());
        helper.send(configureRecord(4, kListedDevice));
        helper.send(openRecord(4));
        std::optional<P::Status> failed;
        while (!failed) {
            const auto status = helper.nextStatus(10000);
            QVERIFY2(status.has_value(), helper.diagnostics().constData());
            QCOMPARE(status->generation, 4u);
            QVERIFY2(status->state != P::HelperState::Ready, "a test run must never open a device");
            QVERIFY2(status->state != P::HelperState::Permission,
                     "the test process would have raised an OS permission prompt");
            if (status->state == P::HelperState::Failed) {
                failed = status;
            } else {
                QCOMPARE(status->state, P::HelperState::Opening);
            }
        }
        QVERIFY(failed->reason == P::FailReason::OpenFailed
                || failed->reason == P::FailReason::PermissionDenied);
        if (failed->reason == P::FailReason::OpenFailed) {
            QCOMPARE(failed->detail, QStringLiteral("a test run opens no audio device"));
        }
        helper.send(P::encodeShutdown());
        QVERIFY(helper.finishes(3000));
        QCOMPARE(helper.process().exitCode(), 0);
    }

    void helperReportsMissingNamedDevice()
    {
        Child helper;
        QVERIFY(helper.start({QStringLiteral("--capture-helper")}));
        QVERIFY(helper.next(3000).has_value());

        // An Open for any generation but the configured one is ignored.
        helper.send(configureRecord(7, kMissingDevice));
        helper.send(openRecord(6));
        QVERIFY(!helper.next(300).has_value());
        QCOMPARE(helper.readerError(), P::RecordReader::Error::None);

        helper.send(openRecord(7));
        std::optional<P::Status> failed;
        while (!failed) {
            const auto status = helper.nextStatus(10000);
            QVERIFY2(status.has_value(), helper.diagnostics().constData());
            QCOMPARE(status->generation, 7u);
            QVERIFY2(status->state != P::HelperState::Ready, "a missing device must never open");
            QVERIFY2(status->state != P::HelperState::Permission,
                     "the test process would have raised an OS permission prompt");
            if (status->state == P::HelperState::Failed) {
                failed = status;
            } else {
                QCOMPARE(status->state, P::HelperState::Opening);
            }
        }
        QVERIFY(failed->reason == P::FailReason::DeviceNotFound
                || failed->reason == P::FailReason::PermissionDenied);
        if (failed->reason == P::FailReason::DeviceNotFound) {
            QVERIFY2(failed->detail.startsWith(QStringLiteral("device-not-found:")),
                     qPrintable(failed->detail));
            QVERIFY(failed->detail.contains(kMissingDevice));
        }
        qInfo("missing device reported as %s: %s",
              failed->reason == P::FailReason::DeviceNotFound ? "device-not-found"
                                                              : "permission-denied",
              qPrintable(failed->detail));

        helper.send(stopRecord(7));
        const auto stopped = helper.nextStatus(2000);
        QVERIFY(stopped.has_value());
        QCOMPARE(stopped->state, P::HelperState::Stopped);
        QCOMPARE(stopped->generation, 7u);

        helper.send(P::encodeShutdown());
        QVERIFY(helper.finishes(3000));
        QCOMPARE(helper.process().exitStatus(), QProcess::NormalExit);
        QCOMPARE(helper.process().exitCode(), 0);
    }

    void helperExitsWhenStdinCloses()
    {
        Child helper;
        QVERIFY(helper.start({QStringLiteral("--capture-helper")}));
        QVERIFY(helper.next(3000).has_value());
        QElapsedTimer timer;
        timer.start();
        helper.process().closeWriteChannel();
        QVERIFY2(helper.finishes(1000), "helper outlived its control pipe");
        qInfo("exited %lld ms after stdin closed", timer.elapsed());
        QCOMPARE(helper.process().exitStatus(), QProcess::NormalExit);
        QCOMPARE(helper.process().exitCode(), 0);
    }

    void helperShutdownExitsWithZero()
    {
        Child helper;
        QVERIFY(helper.start({QStringLiteral("--capture-helper")}));
        QVERIFY(helper.next(3000).has_value());
        helper.send(P::encodeShutdown());
        QVERIFY(helper.finishes(3000));
        QCOMPARE(helper.process().exitStatus(), QProcess::NormalExit);
        QCOMPARE(helper.process().exitCode(), 0);
    }

    void helperExitsOnMalformedParentRecord()
    {
        Child helper;
        QVERIFY(helper.start({QStringLiteral("--capture-helper")}));
        QVERIFY(helper.next(3000).has_value());
        helper.send(QByteArray("XCAP\x01\x10\x00\x00\x00\x00\x00\x00", 12));
        QVERIFY(helper.finishes(1000));
        QCOMPARE(helper.process().exitCode(), 0);
    }

    // ── Scripted fake ──────────────────────────────────────────────────────

    void fakeReadyStreamsTone()
    {
        Child fake;
        QVERIFY(fake.start({QStringLiteral("--fake-capture-child"), QStringLiteral("ready")}));
        QVERIFY(nextTypeIs(fake, P::RecordType::Hello, 3000));
        fake.send(configureRecord(3, QString()));
        fake.send(openRecord(3));
        QVERIFY(nextStateIs(fake, P::HelperState::Opening, 1000));
        const auto ready = fake.nextStatus(1000);
        QVERIFY(ready.has_value());
        QCOMPARE(ready->state, P::HelperState::Ready);
        QCOMPARE(ready->nativeRate, 48000);
        for (quint64 expected = 0; expected < 3 * 480; expected += 480) {
            const auto record = fake.next(1000);
            QVERIFY(record.has_value());
            QCOMPARE(record->type, P::RecordType::Pcm);
            const auto pcm = P::decodePcm(record->payload);
            QVERIFY(pcm.has_value());
            QCOMPARE(pcm->generation, 3u);
            QCOMPARE(pcm->framePosition, expected);
            QCOMPARE(pcm->samples.size(), 480);
        }
        fake.send(stopRecord(3));
        std::optional<P::Status> stopped;
        while (!stopped) {
            const auto record = fake.next(1000);
            QVERIFY(record.has_value());
            if (record->type == P::RecordType::Status) {
                stopped = P::decodeStatus(record->payload);
            }
        }
        QCOMPARE(stopped->state, P::HelperState::Stopped);
        fake.send(P::encodeShutdown());
        QVERIFY(fake.finishes(2000));
        QCOMPARE(fake.process().exitCode(), 0);
    }

    void fakeScenarioShapes_data()
    {
        QTest::addColumn<QString>("scenario");
        QTest::newRow("hang-open") << QStringLiteral("hang-open");
        QTest::newRow("no-hello") << QStringLiteral("no-hello");
        QTest::newRow("permission-then-ready") << QStringLiteral("permission-then-ready");
        QTest::newRow("crash-after-ready") << QStringLiteral("crash-after-ready");
        QTest::newRow("malformed") << QStringLiteral("malformed");
        QTest::newRow("oversize") << QStringLiteral("oversize");
        QTest::newRow("input-lost") << QStringLiteral("input-lost");
        QTest::newRow("ignore-stop") << QStringLiteral("ignore-stop");
        QTest::newRow("stale") << QStringLiteral("stale");
    }

    void fakeScenarioShapes()
    {
        QFETCH(QString, scenario);
        Child fake;
        QVERIFY(fake.start({QStringLiteral("--fake-capture-child"), scenario}));

        if (scenario == QLatin1String("no-hello")) {
            QVERIFY(!fake.next(300).has_value());
        } else if (scenario == QLatin1String("malformed")) {
            // The reader's sticky error discards records queued in the same
            // read, so Hello may or may not surface before the bad magic.
            while (fake.next(1000).has_value()) {
            }
            QCOMPARE(fake.readerError(), P::RecordReader::Error::BadMagic);
        } else {
            QVERIFY(nextTypeIs(fake, P::RecordType::Hello, 3000));
        }

        fake.send(configureRecord(5, QString()));
        fake.send(openRecord(5));

        if (scenario == QLatin1String("hang-open")) {
            QVERIFY(nextStateIs(fake, P::HelperState::Opening, 1000));
            fake.send(stopRecord(5));
            QVERIFY(!fake.next(300).has_value());
        } else if (scenario == QLatin1String("permission-then-ready")) {
            QElapsedTimer timer;
            timer.start();
            QVERIFY(nextStateIs(fake, P::HelperState::Permission, 1000));
            QVERIFY(nextStateIs(fake, P::HelperState::Opening, 1000));
            QVERIFY(timer.elapsed() >= 250);
            QVERIFY(nextStateIs(fake, P::HelperState::Ready, 1000));
            QVERIFY(nextTypeIs(fake, P::RecordType::Pcm, 1000));
        } else if (scenario == QLatin1String("crash-after-ready")) {
            QVERIFY(nextStateIs(fake, P::HelperState::Opening, 1000));
            QVERIFY(nextStateIs(fake, P::HelperState::Ready, 1000));
            QVERIFY(fake.finishes(2000));
            QCOMPARE(fake.process().exitCode(), 3);
            return;
        } else if (scenario == QLatin1String("oversize")) {
            // Opening and Ready precede the bad header but may share its
            // read, in which case the sticky error discards them.
            while (fake.next(1000).has_value()) {
            }
            QCOMPARE(fake.readerError(), P::RecordReader::Error::Oversize);
        } else if (scenario == QLatin1String("input-lost")) {
            QVERIFY(nextStateIs(fake, P::HelperState::Opening, 1000));
            QVERIFY(nextStateIs(fake, P::HelperState::Ready, 1000));
            std::optional<P::Status> failed;
            while (!failed) {
                const auto record = fake.next(1000);
                QVERIFY(record.has_value());
                if (record->type == P::RecordType::Status) {
                    failed = P::decodeStatus(record->payload);
                }
            }
            QCOMPARE(failed->state, P::HelperState::Failed);
            QCOMPARE(failed->reason, P::FailReason::InputLost);
        } else if (scenario == QLatin1String("ignore-stop")) {
            QVERIFY(nextStateIs(fake, P::HelperState::Opening, 1000));
            QVERIFY(nextStateIs(fake, P::HelperState::Ready, 1000));
            fake.send(stopRecord(5));
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < 300) {
                const auto record = fake.next(100);
                QVERIFY(record.has_value());
                QCOMPARE(record->type, P::RecordType::Pcm);
            }
        } else if (scenario == QLatin1String("stale")) {
            const auto opening = fake.nextStatus(1000);
            QVERIFY(opening.has_value());
            QCOMPARE(opening->generation, 5u);
            const auto ready = fake.nextStatus(1000);
            QVERIFY(ready.has_value());
            QCOMPARE(ready->state, P::HelperState::Ready);
            QCOMPARE(ready->generation, 4u);
            const auto record = fake.next(1000);
            QVERIFY(record.has_value());
            const auto pcm = P::decodePcm(record->payload);
            QVERIFY(pcm.has_value());
            QCOMPARE(pcm->generation, 4u);
        }

        // Every scenario still exits on stdin EOF.
        fake.process().closeWriteChannel();
        QVERIFY2(fake.finishes(1000), "fake outlived its control pipe");
        QCOMPARE(fake.process().exitCode(), 0);
    }
};

int main(int argc, char* argv[])
{
    if (argc > 1 && std::strcmp(argv[1], "--capture-helper") == 0) {
        NereusSDR::setCaptureHelperTestDevices({kListedDevice});
        return captureHelperEntryPointForTest(argc - 1, argv + 1);
    }
    if (argc > 2 && std::strcmp(argv[1], "--fake-capture-child") == 0) {
        return NereusSDR::Test::runFakeCaptureChild(QString::fromLocal8Bit(argv[2]));
    }
    QCoreApplication app(argc, argv);
    TstCaptureHelperProcess test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_capture_helper_process.moc"
