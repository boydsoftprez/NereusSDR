// no-port-check: NereusSDR-original test.
// =================================================================
// tests/tst_cw_serial_key_input.cpp  (NereusSDR)
// =================================================================
//
// CW transmit (R-CW-04, R-CW-36): the serial key reader on an injected line
// source, and its wiring on a RadioModel with a fake connection. It checks:
//   - paddles: DSR down sets the dot, CTS down the dash (Thetis
//     SDRSerialPortII.cs:259-263 [v2.10.3.15]);
//   - the key line keys the CWX bit through the CWX sender, and the PTT
//     line posts pttLineChanged; a key or PTT line chosen takes the lines
//     from the paddles (Thetis's else);
//   - with the gate false the key bits stay 0; a gate refusal, a stop and
//     the transmit cut release them, and a line held through them keys
//     nothing until it goes up and down again;
//   - a line down when the port opens needs a fresh edge;
//   - the reader thread stops and joins (opt-in 50 ms measurement) on stopReading and
//     on destruction, and lets go of the key line it held;
//   - on a RadioModel: a port CAT or the CAT PTT input uses is refused with
//     the words and never opened; the PTT line keys MOX in Semi and a
//     refused gate is reported through MoxController; the connection
//     going away or swapping stops the reader before it goes (early review
//     M8); every stop and the cut's catch-up release the keys.
// Nothing here opens a radio socket or keys a radio. One case, opt-in
// through NEREUS_CW_SERIAL_MEASURE_PORT, measures the macOS poll's
// processor use on a port named there.
//
// Modification history (NereusSDR):
//   2026-10-27: Final fix wave: serial validation, queued refusal holds and quiet Off stops.
//               F3 keeps join correctness unconditional and latency measurement opt-in.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-26: CW transmit, the serial key reader. J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
//   2026-10-27: CW transmit final fix wave F5: the platform reader opens
//               only one of this computer's serial ports. J.J. Boyd
//               (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTemporaryFile>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <utility>

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
#include <sys/resource.h>
#endif

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/CwKeyLine.h"
#include "core/CwSerialKeyInput.h"
#include "core/CwTypes.h"
#include "core/CwxSender.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "core/cat/CatConfiguration.h"
#include "core/cat/CatService.h"
#include "core/safety/TxRefusal.h"
#include "models/CwKeyerController.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

// A line source the test drives. push() queues new levels; idle() waits
// until the reader has taken every one and is waiting again.
class FakeLineSource : public CwSerialKeyInput::LineSource {
public:
    struct Shared {
        std::mutex mutex;
        std::condition_variable cv;
        std::deque<std::pair<bool, bool>> pending;
        bool stopped{false};
        bool lost{false};
        bool waiting{false};
        bool failOpen{false};
        bool initialDsr{false};
        bool initialCts{false};
        int opens{0};
        QString lastPort;
    };

    explicit FakeLineSource(std::shared_ptr<Shared> shared) : m_s(std::move(shared)) {}

    bool open(const QString& port, QString* error) override
    {
        const std::lock_guard<std::mutex> lock(m_s->mutex);
        ++m_s->opens;
        m_s->lastPort = port;
        if (m_s->failOpen) {
            if (error != nullptr) {
                *error = QString::fromLatin1(kCwSerialPortOpenFailedText).arg(port);
            }
            return false;
        }
        m_s->stopped = false;
        m_s->lost = false;
        m_s->pending.clear();
        return true;
    }
    bool initialLevels(bool* dsr, bool* cts) override
    {
        const std::lock_guard<std::mutex> lock(m_s->mutex);
        *dsr = m_s->initialDsr;
        *cts = m_s->initialCts;
        return true;
    }
    bool waitForChange(bool* dsr, bool* cts) override
    {
        std::unique_lock<std::mutex> lock(m_s->mutex);
        m_s->waiting = true;
        m_s->cv.notify_all();
        m_s->cv.wait(lock, [this] { return m_s->stopped || m_s->lost || !m_s->pending.empty(); });
        m_s->waiting = false;
        if (m_s->stopped || m_s->lost) {
            return false;
        }
        *dsr = m_s->pending.front().first;
        *cts = m_s->pending.front().second;
        m_s->pending.pop_front();
        return true;
    }
    void stop() override
    {
        {
            const std::lock_guard<std::mutex> lock(m_s->mutex);
            m_s->stopped = true;
        }
        m_s->cv.notify_all();
    }

private:
    std::shared_ptr<Shared> m_s;
};

struct Lines {
    std::shared_ptr<FakeLineSource::Shared> shared = std::make_shared<FakeLineSource::Shared>();

    std::unique_ptr<CwSerialKeyInput::LineSource> source() const
    {
        return std::make_unique<FakeLineSource>(shared);
    }
    void push(bool dsr, bool cts)
    {
        {
            const std::lock_guard<std::mutex> lock(shared->mutex);
            shared->pending.emplace_back(dsr, cts);
        }
        shared->cv.notify_all();
    }
    // The reader took every level and waits again.
    bool idle()
    {
        std::unique_lock<std::mutex> lock(shared->mutex);
        return shared->cv.wait_for(lock, std::chrono::seconds(2), [this] {
            return shared->pending.empty() && shared->waiting;
        });
    }
    void lose()
    {
        {
            const std::lock_guard<std::mutex> lock(shared->mutex);
            shared->lost = true;
        }
        shared->cv.notify_all();
    }
    int opens() const
    {
        const std::lock_guard<std::mutex> lock(shared->mutex);
        return shared->opens;
    }
};

void pump(int passes = 6)
{
    for (int i = 0; i < passes; ++i) {
        QCoreApplication::processEvents();
    }
}

// A reader on fake lines with the gate allowed and a key line.
struct Reader {
    Lines lines;
    std::shared_ptr<CwKeyLine> line = std::make_shared<CwKeyLine>();
    std::unique_ptr<CwSerialKeyInput> input;

    explicit Reader(bool gate = true)
        : input(std::make_unique<CwSerialKeyInput>(lines.source()))
    {
        input->setKeyLine(line);
        input->setGateAllowed(gate);
    }
    bool start(bool paddles, CwSerialLine key = CwSerialLine::None,
               CwSerialLine ptt = CwSerialLine::None)
    {
        QString error;
        const bool ok = input->start(QStringLiteral("fake0"), paddles, key, ptt, &error);
        return ok && lines.idle();
    }
    void push(bool dsr, bool cts)
    {
        lines.push(dsr, cts);
        QVERIFY(lines.idle());
    }
};

// ── The RadioModel rig ──────────────────────────────────────────────────

class CwMockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit CwMockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
};

RadioInfo saturn()
{
    RadioInfo info;
    info.protocol = ProtocolVersion::Protocol2;
    info.boardType = HPSDRHW::Saturn;
    info.firmwareVersion = 10;
    info.macAddress = QStringLiteral("00:1c:c0:a2:13:dd");
    return info;
}

// A local model on a P2 Saturn with a fake connection: one slice on 20 m
// CWU, break-in Semi, the serial reader on fake lines (not yet reading).
struct Rig {
    RadioModel model;
    CwMockConnection conn;
    Lines lines;
    SliceModel* slice{nullptr};

    Rig()
    {
        AppSettings::instance().clear();
        model.setCapsForTest(/*hasAlex=*/false);
        model.injectConnectionForTest(&conn);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.replaceCwSerialKeyInputForTest(std::make_unique<CwSerialKeyInput>(lines.source()));
        slice = model.sliceById(model.addSlice());
        slice->setFrequency(14'050'000.0);
        slice->setDspMode(DSPMode::CWU);
        tm().setCwBreakInMode(static_cast<int>(CwBreakInMode::Semi));
        model.setLastRadioInfoForTest(saturn());
        model.wireCwKeyerConnectionForTest();
        pump();
    }
    ~Rig()
    {
        model.injectConnectionForTest(nullptr);
        AppSettings::instance().clear();
    }

    TransmitModel& tm() { return model.transmitModel(); }
    CwSerialKeyInput* reader() { return model.cwSerialKeyInput(); }
    std::shared_ptr<CwKeyLine> keyLine() { return conn.cwKeyLine(); }
    void readPort(bool paddles, CwSerialLine key = CwSerialLine::None,
                  CwSerialLine ptt = CwSerialLine::None)
    {
        tm().setCwSerialPaddles(paddles);
        tm().setCwSerialKeyLine(static_cast<int>(key));
        tm().setCwSerialPttLine(static_cast<int>(ptt));
        tm().setCwSerialPort(QStringLiteral("fake0"));
        pump();
    }
    void push(bool dsr, bool cts)
    {
        lines.push(dsr, cts);
        QVERIFY(lines.idle());
        pump();
    }
};

} // namespace

class TestCwSerialKeyInput : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        AppSettings::instance().clear();
        setCwSerialPortNamesForTest(QStringList{QStringLiteral("fake0")});
    }
    void cleanup()
    {
        setCwSerialPortNamesForTest(std::nullopt);
        AppSettings::instance().clear();
    }

    // ── The words ────────────────────────────────────────────────────────
    void wordsArePlain()
    {
        for (const char* text : {kCwSerialPortInUseByCatText, kCwSerialPortInUseByCatPttText,
                                 kCwSerialPortOpenFailedText, kCwSerialPortLostText}) {
            QVERIFY2(OperatorWording::isPlain(QString::fromLatin1(text)), text);
        }
        QCOMPARE(QString::fromLatin1(kCwSerialPortInUseByCatText),
                 QStringLiteral("That port is in use by CAT."));
    }

    // ── Paddles (SDRSerialPortII.cs:259-263) ────────────────────────────
    void paddles_dsrSetsDot_ctsSetsDash()
    {
        Reader r;
        QSignalSpy activity(r.input.get(), &CwSerialKeyInput::paddleActivity);
        QVERIFY(r.start(/*paddles=*/true));
        r.push(true, false);
        QVERIFY(r.line->dot());
        QVERIFY(!r.line->dash());
        QVERIFY(!r.line->cwx());
        QVERIFY(r.input->paddleDown());
        r.push(true, true);
        QVERIFY(r.line->dot());
        QVERIFY(r.line->dash());
        r.push(false, true);
        QVERIFY(!r.line->dot());
        QVERIFY(r.line->dash());
        r.push(false, false);
        QVERIFY(!r.line->dash());
        QVERIFY(!r.input->paddleDown());
        QVERIFY(!r.input->isKeying());
        pump();
        QCOMPARE(activity.count(), 4);
    }

    void paddlesOff_andNoLines_keyNothing()
    {
        Reader r;
        QSignalSpy ptt(r.input.get(), &CwSerialKeyInput::pttLineChanged);
        QVERIFY(r.start(/*paddles=*/false));
        r.push(true, true);
        pump();
        QCOMPARE(r.line->p1Bits(false), quint8(0));
        QCOMPARE(ptt.count(), 0);
    }

    // ── The key line keys the CWX bit through the CWX sender ─────────────
    void keyLineDsr_keysCwxThroughTheSender()
    {
        Reader r;
        CwxSender cwx;
        cwx.setKeyLine(r.line);
        cwx.setBreakInActive(true);
        r.input->setStraightKeySink([&cwx](bool down) { return cwx.setHandKey(down); });
        QVERIFY(r.start(/*paddles=*/true, CwSerialLine::Dsr));
        r.push(true, false);
        QVERIFY(r.line->cwx());
        QVERIFY(!r.line->dot());          // the key line takes DSR from the paddles
        QVERIFY(r.input->isKeying());
        r.push(false, false);
        QVERIFY(!r.line->cwx());
        // CTS is neither chosen line, and a key line takes it from the dash.
        r.push(false, true);
        QCOMPARE(r.line->p1Bits(false), quint8(0));
        // The sender's stop and a new key line drop a held hand key.
        r.push(true, true);
        QVERIFY(r.line->cwx());
        cwx.stop();
        QVERIFY(!r.line->cwx());
        r.push(false, true);
        r.push(true, true);
        QVERIFY(r.line->cwx());
        cwx.setKeyLine(nullptr);
        QVERIFY(!r.line->cwx());
        r.push(false, false);
    }

    void keyLine_inOffWithoutMox_keysNothing()
    {
        Reader r;
        CwxSender cwx;
        cwx.setKeyLine(r.line);
        cwx.setBreakInActive(false);
        r.input->setStraightKeySink([&cwx](bool down) { return cwx.setHandKey(down); });
        QVERIFY(r.start(false, CwSerialLine::Cts));
        r.push(false, true);
        QVERIFY(!r.line->cwx());
        cwx.setMoxActive(true);
        QVERIFY(r.line->cwx());           // in Off CWX keys only while MOX is held
        r.push(false, false);
        QVERIFY(!r.line->cwx());
    }

    void keyLine_whileCwxSendsText_isIgnored()
    {
        Reader r;
        CwxSender cwx;
        cwx.setKeyLine(r.line);
        cwx.setBreakInActive(true);
        cwx.setSpeedWpm(5);
        r.input->setStraightKeySink([&cwx](bool down) { return cwx.setHandKey(down); });
        QVERIFY(r.start(false, CwSerialLine::Dsr));
        QVERIFY(cwx.sendText(QStringLiteral("TTTTT")));
        r.push(true, false);
        QVERIFY(!r.input->isKeying());
        cwx.stop();
        QVERIFY(!r.line->cwx());
        // The press was refused, so it keys nothing until a fresh edge.
        r.push(true, true);
        QVERIFY(!r.line->cwx());
        r.push(false, false);
        r.push(true, false);
        QVERIFY(r.line->cwx());
        r.push(false, false);
    }

    // ── The PTT line ─────────────────────────────────────────────────────
    void pttLineCts_postsPttLineChanged_evenWithTheGateFalse()
    {
        Reader r(/*gate=*/false);
        QSignalSpy ptt(r.input.get(), &CwSerialKeyInput::pttLineChanged);
        QVERIFY(r.start(/*paddles=*/true, CwSerialLine::None, CwSerialLine::Cts));
        r.push(false, true);
        pump();
        QCOMPARE(ptt.count(), 1);
        QCOMPARE(ptt.at(0).at(0).toBool(), true);
        QVERIFY(!r.line->dash());         // the PTT line takes CTS from the paddles
        QVERIFY(!r.line->dot());
        r.push(true, true);               // DSR is neither chosen line
        QVERIFY(!r.line->dot());
        r.push(false, false);
        pump();
        QCOMPARE(ptt.count(), 2);
        QCOMPARE(ptt.at(1).at(0).toBool(), false);
    }

    // ── The gate ─────────────────────────────────────────────────────────
    void gateFalse_keyBitsStayZero_untilAFreshEdge()
    {
        Reader r(/*gate=*/false);
        QVERIFY(r.start(true));
        r.push(true, true);
        QCOMPARE(r.line->p1Bits(false), quint8(0));
        r.input->setGateAllowed(true);
        QCOMPARE(r.line->p1Bits(false), quint8(0));   // held through: no key
        r.push(false, true);
        r.push(false, false);
        r.push(true, false);
        QVERIFY(r.line->dot());
        r.push(false, false);
    }

    void gateTurningFalse_releasesHeldBits_andAFreshEdgeKeysAgain()
    {
        Reader r;
        QVERIFY(r.start(true));
        r.push(true, true);
        QVERIFY(r.line->dot() && r.line->dash());
        r.input->setGateAllowed(false);
        QCOMPARE(r.line->p1Bits(false), quint8(0));
        QVERIFY(!r.input->isKeying());
        r.input->setGateAllowed(true);
        QCOMPARE(r.line->p1Bits(false), quint8(0));
        r.push(true, false);              // the dash went up
        QVERIFY(!r.line->dot());          // the dot is still the held one
        r.push(true, true);
        QVERIFY(r.line->dash());          // a fresh dash keys
        QVERIFY(!r.line->dot());
        r.push(false, false);
    }

    // ── A stop or the cut (releaseKeys) ─────────────────────────────────
    void releaseKeys_releasesBitsAndPtt_heldLinesNeedAFreshEdge()
    {
        Reader r;
        CwxSender cwx;
        cwx.setKeyLine(r.line);
        cwx.setBreakInActive(true);
        r.input->setStraightKeySink([&cwx](bool down) { return cwx.setHandKey(down); });
        QSignalSpy ptt(r.input.get(), &CwSerialKeyInput::pttLineChanged);
        QVERIFY(r.start(false, CwSerialLine::Dsr, CwSerialLine::Cts));
        r.push(true, true);
        pump();
        QVERIFY(r.line->cwx());
        QCOMPARE(ptt.count(), 1);

        // The cut: both lines held down through it and its clear.
        r.input->releaseKeys();
        pump();
        QVERIFY(!r.line->cwx());
        QCOMPARE(ptt.count(), 2);
        QCOMPARE(ptt.at(1).at(0).toBool(), false);
        QVERIFY(!r.input->isKeying());
        r.input->setGateAllowed(true);    // the cut clears
        r.push(true, true);               // no change: nothing to read
        pump();
        QCOMPARE(r.line->p1Bits(false), quint8(0));
        QCOMPARE(ptt.count(), 2);

        // A fresh edge keys again.
        r.push(false, false);
        r.push(true, true);
        pump();
        QVERIFY(r.line->cwx());
        QCOMPARE(ptt.count(), 3);
        r.push(false, false);
        pump();
        QCOMPARE(ptt.count(), 4);
    }

    void aLineDownWhenThePortOpens_needsAFreshEdge()
    {
        Reader r;
        r.lines.shared->initialDsr = true;
        QVERIFY(r.start(true));
        r.push(true, true);
        QVERIFY(!r.line->dot());          // down since the open
        QVERIFY(r.line->dash());
        r.push(false, true);
        r.push(true, true);
        QVERIFY(r.line->dot());
        r.push(false, false);
    }

    // ── The key line ─────────────────────────────────────────────────────
    void setKeyLine_releasesTheOldLine_andTheNewOneWaitsForAFreshEdge()
    {
        Reader r;
        QVERIFY(r.start(true));
        r.push(true, false);
        QVERIFY(r.line->dot());
        auto other = std::make_shared<CwKeyLine>();
        r.input->setKeyLine(other);
        QVERIFY(!r.line->dot());
        QVERIFY(!other->dot());
        r.push(true, true);
        QVERIFY(!other->dot());
        QVERIFY(other->dash());
        r.input->setKeyLine(nullptr);
        QVERIFY(!other->dash());
        r.push(false, false);
        r.push(true, false);              // no line: nothing written anywhere
        QVERIFY(!r.line->dot());
        QVERIFY(!other->dot());
    }

    // ── The thread ───────────────────────────────────────────────────────
    void stopReading_joinsAndReleases_withOptInTiming()
    {
        Reader r;
        QSignalSpy failed(r.input.get(), &CwSerialKeyInput::failed);
        QVERIFY(r.start(true));
        r.push(true, false);
        QVERIFY(r.input->isReading());
        QElapsedTimer t;
        t.start();
        r.input->stopReading();
        const qint64 ms = t.elapsed();
        // F3: release/join correctness is unconditional. Scheduler latency
        // is an opt-in measurement, run alone with recorded machine load.
        if (qEnvironmentVariableIsSet("NEREUS_CW_SERIAL_MEASURE_JOIN")) {
            QVERIFY2(ms < 50, qPrintable(QString::number(ms)));
        }
        QVERIFY(!r.input->isReading());
        QVERIFY(!r.line->dot());
        pump();
        QCOMPARE(failed.count(), 0);

        // It starts again on the same source.
        QVERIFY(r.start(true));
        r.push(false, true);
        QVERIFY(r.line->dash());
        r.input->stopReading();
        QVERIFY(!r.line->dash());
    }

    void destruction_joinsAndReleases_withOptInTiming()
    {
        Reader r;
        QVERIFY(r.start(true));
        r.push(false, true);
        QVERIFY(r.line->dash());
        QElapsedTimer t;
        t.start();
        r.input.reset();
        const qint64 ms = t.elapsed();
        // F3: release/join correctness is unconditional. Scheduler latency
        // is an opt-in measurement, run alone with recorded machine load.
        if (qEnvironmentVariableIsSet("NEREUS_CW_SERIAL_MEASURE_JOIN")) {
            QVERIFY2(ms < 50, qPrintable(QString::number(ms)));
        }
        QVERIFY(!r.line->dash());
    }

    void openFailure_returnsTheWords()
    {
        Reader r;
        r.lines.shared->failOpen = true;
        QString error;
        QVERIFY(!r.input->start(QStringLiteral("fake9"), true, CwSerialLine::None,
                                CwSerialLine::None, &error));
        QCOMPARE(error, QString::fromLatin1(kCwSerialPortOpenFailedText).arg(QStringLiteral("fake9")));
        QVERIFY(!r.input->isReading());
    }

    // F5: the platform reader opens only one of this computer's serial
    // ports. A file this test owns, which the reader could open and drive
    // DTR and RTS on, is refused with the words and never opened.
    void platformReader_opensOnlyThisComputersPorts()
    {
        QTemporaryFile file;
        QVERIFY(file.open());
        const QString path = file.fileName();
        file.close();
        setCwSerialPortNamesForTest(QStringList{QStringLiteral("/dev/cu.usbserial-test")});
        CwSerialKeyInput input(CwSerialKeyInput::createPlatformLineSource());
        for (const QString& port : {path, QStringLiteral("../../etc/passwd"), QString()}) {
            QString error;
            QVERIFY2(!input.start(port, true, CwSerialLine::None, CwSerialLine::None, &error),
                     qPrintable(port));
            QVERIFY(!input.isReading());
            QCOMPARE(error, QString::fromLatin1(kCwSerialPortNotHereText));
        }
        QVERIFY(OperatorWording::isPlain(QString::fromLatin1(kCwSerialPortNotHereText)));
        QVERIFY(cwSerialPortIsHere(QStringLiteral(" /dev/cu.usbserial-test ")));
        QVERIFY(!cwSerialPortIsHere(path));
        setCwSerialPortNamesForTest(std::nullopt);
    }

    void noEnumeratedPorts_refusesEveryDeviceButModelAcceptsNone()
    {
        setCwSerialPortNamesForTest(QStringList{});
        QVERIFY(cwSerialPortNamesHere().isEmpty());
        QVERIFY(!cwSerialPortIsHere(QStringLiteral("fake0")));
        RadioModel model;
        QTest::ignoreMessage(QtWarningMsg, "CW serial key input: That serial port is not on the Core's computer.");
        model.transmitModel().setCwSerialPort(QStringLiteral("fake0"));
        QVERIFY(model.transmitModel().cwSerialPort().isEmpty());
        QCOMPARE(model.cwSerialKeyRefusal(), QString::fromLatin1(kCwSerialPortNotHereText));
        model.transmitModel().setCwSerialPort(QString());
        QVERIFY(model.transmitModel().cwSerialPort().isEmpty());
#ifndef HAVE_SERIALPORT
        setCwSerialPortNamesForTest(std::nullopt);
        QVERIFY(cwSerialPortNamesHere().isEmpty());
#endif
    }

    void aLostPort_releasesAndSaysSo()
    {
        Reader r;
        QSignalSpy failed(r.input.get(), &CwSerialKeyInput::failed);
        QVERIFY(r.start(true));
        r.push(true, false);
        QVERIFY(r.line->dot());
        r.lines.lose();
        QTRY_VERIFY_WITH_TIMEOUT(!r.input->isReading(), 2000);
        QVERIFY(!r.line->dot());
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 2000);
        QCOMPARE(failed.at(0).at(0).toString(), QString::fromLatin1(kCwSerialPortLostText));
        r.input->stopReading();
    }

    // ── On a RadioModel ──────────────────────────────────────────────────
    void model_paddlesKeyTheConnectionsLine_andFollowTheKeyerGate()
    {
        Rig rig;
        rig.readPort(/*paddles=*/true);
        QVERIFY(rig.reader()->isReading());
        QCOMPARE(rig.lines.shared->lastPort, QStringLiteral("fake0"));
        QVERIFY2(rig.model.cwKeyerController()->keyerAllowed(),
                 qPrintable(rig.model.cwKeyerController()->refusal().text));
        rig.push(true, false);
        QVERIFY(rig.keyLine()->dot());
        rig.push(false, false);
        QVERIFY(!rig.keyLine()->dot());

        // The gate turns false (out of the band plan): bits stay 0.
        rig.slice->setFrequency(14'400'000.0);
        pump();
        QVERIFY(!rig.model.cwKeyerController()->keyerAllowed());
        rig.push(true, true);
        QCOMPARE(rig.keyLine()->p1Bits(false), quint8(0));
    }

    void model_straightKey_keysCwxThroughTheModelsSender()
    {
        Rig rig;
        rig.readPort(false, CwSerialLine::Dsr);
        rig.push(true, false);
        QVERIFY(rig.keyLine()->cwx());   // Semi: break-in active
        rig.push(false, false);
        QVERIFY(!rig.keyLine()->cwx());
    }

    void model_pttLine_keysMoxInSemi()
    {
        Rig rig;
        rig.readPort(false, CwSerialLine::None, CwSerialLine::Cts);
        rig.push(false, true);
        QTRY_VERIFY_WITH_TIMEOUT(rig.model.moxController()->isMox(), 2000);
        rig.push(false, false);
        QTRY_VERIFY_WITH_TIMEOUT(!rig.model.moxController()->isMox(), 2000);
    }

    void model_pttLine_gateRefused_isReportedAndKeysNothing()
    {
        Rig rig;
        rig.slice->setFrequency(14'400'000.0);   // outside the band plan
        pump();
        QVERIFY(!rig.model.cwKeyerController()->keyerAllowed());
        const TxRefusal refusal = rig.model.cwKeyerController()->refusal();
        QSignalSpy rejected(rig.model.moxController(), &MoxController::moxRejected);
        rig.readPort(false, CwSerialLine::None, CwSerialLine::Cts);
        rig.push(false, true);
        pump(20);
        QVERIFY(!rig.model.moxController()->isMox());
        QTRY_COMPARE_WITH_TIMEOUT(rejected.count(), 1, 2000);
        QCOMPARE(rejected.at(0).at(0).toString(), refusal.text);
        rig.push(false, false);
        pump();
        QVERIFY(!rig.model.moxController()->isMox());
    }

    void model_keyGateRefusal_isSaidOncePerHold_data()
    {
        QTest::addColumn<bool>("paddles");
        QTest::addColumn<int>("keyLine");
        QTest::newRow("paddles") << true << int(CwSerialLine::None);
        QTest::newRow("straight DSR") << false << int(CwSerialLine::Dsr);
        QTest::newRow("straight CTS") << false << int(CwSerialLine::Cts);
    }

    void model_keyGateRefusal_isSaidOncePerHold()
    {
        QFETCH(bool, paddles);
        QFETCH(int, keyLine);
        Rig rig;
        rig.slice->setFrequency(14'400'000.0);
        pump();
        QVERIFY(!rig.model.cwKeyerController()->keyerAllowed());
        const TxRefusal reason = rig.model.cwKeyerController()->refusal();
        QVERIFY(!reason.isEmpty());
        QSignalSpy rejected(rig.model.moxController(), &MoxController::moxRejected);
        rig.readPort(paddles, static_cast<CwSerialLine>(keyLine));
        rig.push(true, true);
        QTRY_COMPARE(rejected.count(), 1);
        QCOMPARE(rejected.first().first().toString(), reason.text);
        rig.push(true, true);
        rig.push(false, true);  // one paddle/CTS remains held
        QCOMPARE(rejected.count(), 1);
        QCOMPARE(rig.keyLine()->p1Bits(false), quint8(0));
        QVERIFY(!rig.model.mox());
        rig.push(false, false);
        rig.push(true, true);
        QTRY_COMPARE(rejected.count(), 2);
        rig.slice->setFrequency(14'050'000.0);
        pump();
        QVERIFY(rig.model.cwKeyerController()->keyerAllowed());
        QCOMPARE(rig.keyLine()->p1Bits(false), quint8(0));
        rig.push(false, false);
        rig.push(true, true);
        QVERIFY(rig.keyLine()->p1Bits(false) != 0);
        QCOMPARE(rejected.count(), 2);
        rig.push(false, false);
    }

    void gateRefusalKeepsItsWordsUntilMainThreadDelivery()
    {
        Reader r(false);
        const TxRefusal refusal{QByteArray(TxRefusals::kNotReady),
                                QStringLiteral("The radio is not connected."), {}};
        r.input->setGateAllowed(false, refusal);
        QList<TxRefusal> delivered;
        bool deliveredOnMain = false;
        connect(r.input.get(), &CwSerialKeyInput::keyRefused, this,
                [&](const TxRefusal& value) {
            delivered.append(value);
            deliveredOnMain = QThread::currentThread() == r.input->thread();
        });
        QVERIFY(r.start(true));
        r.push(true, false);
        QVERIFY(delivered.isEmpty());
        r.input->setGateAllowed(true);
        pump();
        QCOMPARE(delivered.size(), 1);
        QVERIFY(delivered.first() == refusal);
        QVERIFY(deliveredOnMain);
        QVERIFY(!r.line->dot());
    }

    void model_stopInOff_releasesTheSerialKeyWithoutAnnouncement()
    {
        Rig rig;
        rig.tm().setCwBreakInMode(static_cast<int>(CwBreakInMode::Off));
        rig.readPort(false, CwSerialLine::Dsr);
        rig.push(true, false);
        QVERIFY(rig.reader()->isKeying());
        QVERIFY(!rig.model.mox());
        QVERIFY(!rig.keyLine()->cwx());
        QSignalSpy stopped(&rig.model, &RadioModel::transmitStopped);
        rig.model.stopAllTx(QStringLiteral("Stopped."));
        QVERIFY(!rig.reader()->isKeying());
        QVERIFY(!rig.keyLine()->cwx());
        QCOMPARE(stopped.count(), 0);
        rig.push(true, false);
        QVERIFY(!rig.reader()->isKeying());
        rig.push(false, false);
        rig.push(true, false);
        QVERIFY(rig.reader()->isKeying());
        rig.push(false, false);
    }

    void model_straightKeyBusyWithText_doesNotInventGateRefusal()
    {
        Rig rig;
        rig.readPort(false, CwSerialLine::Dsr);
        QVERIFY(rig.model.cwKeyerController()->keyerAllowed());
        QSignalSpy rejected(rig.model.moxController(), &MoxController::moxRejected);
        QVERIFY(rig.model.cwxSender()->sendText(QString(1000, QLatin1Char('T'))));
        QVERIFY(rig.model.cwxSender()->isSending());
        rig.push(true, false);
        QCOMPARE(rejected.count(), 0);
        rig.model.cwxSender()->stop();
        rig.push(false, false);
    }

    void model_gateTurnsFalseWhileHeld_releasesAndSaysWhyOnce()
    {
        Rig rig;
        rig.readPort(true);
        rig.push(true, false);
        QVERIFY(rig.keyLine()->dot());
        QSignalSpy rejected(rig.model.moxController(), &MoxController::moxRejected);
        rig.slice->setFrequency(14'400'000.0);
        pump();
        QVERIFY(!rig.keyLine()->dot());
        QTRY_COMPARE(rejected.count(), 1);
        QCOMPARE(rejected.first().first().toString(), rig.model.cwKeyerController()->refusal().text);
        rig.model.cwKeyerController()->reevaluate();
        pump();
        QCOMPARE(rejected.count(), 1);
        rig.push(false, false);
    }

    void model_aPortCatUses_isRefused_andNeverOpened()
    {
        Rig rig;
        CatEndpointConfig cat = rig.model.catService()->channelConfig(1);
        cat.serialEnabled = true;
        cat.serialDevice = QStringLiteral("fake0");
        QVERIFY(rig.model.catService()->applyChannelConfig(1, cat));
        QSignalSpy refusal(&rig.model, &RadioModel::cwSerialKeyRefusalChanged);
        QTest::ignoreMessage(QtWarningMsg, "CW serial key input: That port is in use by CAT.");
        rig.readPort(true);
        QCOMPARE(rig.model.cwSerialKeyRefusal(), QStringLiteral("That port is in use by CAT."));
        QVERIFY(refusal.count() >= 1);
        QCOMPARE(rig.lines.opens(), 0);
        QVERIFY(!rig.reader()->isReading());

        // CAT lets go of it: the reader opens it.
        cat.serialEnabled = false;
        QVERIFY(rig.model.catService()->applyChannelConfig(1, cat));
        pump();
        QTRY_VERIFY_WITH_TIMEOUT(rig.reader()->isReading(), 2000);
        QVERIFY(rig.model.cwSerialKeyRefusal().isEmpty());
    }

    void model_aPortTheCatPttInputUses_isRefused()
    {
        Rig rig;
        CatGlobalConfig global = rig.model.catService()->globalConfig();
        global.pttEnabled = true;
        global.pttDeviceSource = QStringLiteral("Physical");
        global.pttSerialDevice = QStringLiteral("fake0");
        global.pttUseCts = true;
        QVERIFY(rig.model.catService()->applyGlobalConfig(global));
        QTest::ignoreMessage(QtWarningMsg,
                             "CW serial key input: That port is in use by the CAT PTT input.");
        rig.readPort(true);
        QCOMPARE(rig.model.cwSerialKeyRefusal(),
                 QStringLiteral("That port is in use by the CAT PTT input."));
        QCOMPARE(rig.lines.opens(), 0);
        QVERIFY(!rig.reader()->isReading());
    }

    // Early review M8: the reader stops and lets go of a connection's line
    // before the connection goes, and on a swap.
    void model_connectionGoingAway_stopsTheReaderFirst()
    {
        Rig rig;
        rig.readPort(true);
        rig.push(true, false);
        const std::shared_ptr<CwKeyLine> line = rig.keyLine();
        QVERIFY(line->dot());

        CwMockConnection other;
        rig.model.injectConnectionForTest(&other);
        pump();
        QVERIFY(!line->dot());
        QVERIFY(!other.cwKeyLine()->dot());   // held through the swap: no key

        rig.model.injectConnectionForTest(nullptr);
        pump();
        QVERIFY(!rig.reader()->isReading());
        rig.lines.push(false, true);          // nobody reads it now
        QVERIFY(!line->dash());
        QVERIFY(!other.cwKeyLine()->dash());
        rig.model.injectConnectionForTest(&rig.conn);
    }

    // Early re-review: every stop and the cut's catch-up release the keys,
    // and lines held through the cut and its clear key nothing until a
    // fresh edge.
    void model_stopAndCut_releaseTheKeys_untilAFreshEdge()
    {
        Rig rig;
        rig.readPort(true);
        rig.push(true, true);
        QVERIFY(rig.keyLine()->dot() && rig.keyLine()->dash());
        rig.model.stopAllTx(QStringLiteral("stop"));
        pump();
        QCOMPARE(rig.keyLine()->p1Bits(false), quint8(0));

        rig.push(false, true);
        rig.push(true, true);
        QVERIFY(rig.keyLine()->dot());
        rig.model.catchUpTransmitCut(QStringLiteral("cut"));
        pump(20);
        QCOMPARE(rig.keyLine()->p1Bits(false), quint8(0));
        rig.push(true, false);                // the dash up, the dot still held
        QCOMPARE(rig.keyLine()->p1Bits(false), quint8(0));
        rig.push(false, false);
        rig.push(true, false);
        QVERIFY(rig.keyLine()->dot());
        rig.push(false, false);
    }

    // ── The macOS poll's processor use (opt-in, a real port) ─────────────
    void macPoll_processorUse()
    {
#if !defined(Q_OS_MAC)
        QSKIP("The 1 ms poll is the macOS line source.");
#else
        const QByteArray port = qgetenv("NEREUS_CW_SERIAL_MEASURE_PORT");
        if (port.isEmpty()) {
            QSKIP("Set NEREUS_CW_SERIAL_MEASURE_PORT to a port with no radio on it.");
        }
        CwSerialKeyInput input(CwSerialKeyInput::createPlatformLineSource());
        QString error;
        QVERIFY2(input.start(QString::fromLocal8Bit(port), true, CwSerialLine::None,
                             CwSerialLine::None, &error),
                 qPrintable(error));
        const auto cpuSeconds = []() {
            rusage use{};
            getrusage(RUSAGE_SELF, &use);
            return double(use.ru_utime.tv_sec + use.ru_stime.tv_sec)
                + double(use.ru_utime.tv_usec + use.ru_stime.tv_usec) / 1e6;
        };
        QTest::qWait(200);
        const double before = cpuSeconds();
        QElapsedTimer wall;
        wall.start();
        QTest::qWait(3000);
        const double used = cpuSeconds() - before;
        const double share = used / (double(wall.elapsed()) / 1000.0) * 100.0;
        qInfo().noquote() << QStringLiteral("CW serial poll: %1 ms processor in %2 ms, %3% of one core")
                                 .arg(used * 1000.0, 0, 'f', 1)
                                 .arg(wall.elapsed())
                                 .arg(share, 0, 'f', 2);
        input.stopReading();
        QVERIFY2(share < 5.0, qPrintable(QString::number(share)));
#endif
    }
};

QTEST_MAIN(TestCwSerialKeyInput)
#include "tst_cw_serial_key_input.moc"
