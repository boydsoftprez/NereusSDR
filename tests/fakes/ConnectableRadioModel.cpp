// tests/fakes/ConnectableRadioModel.cpp
//
// no-port-check: NereusSDR-original test fixture. Wires together two
// already-existing production classes (RadioModel, P1FakeRadio) for test
// use; no Thetis logic is ported or reimplemented here.

#include "ConnectableRadioModel.h"
#include "FakeAudioBus.h"

#include "core/AudioEngine.h"
#include "core/DspControlThread.h"

#include <QtTest/QtTest>

namespace NereusSDR::Test {

namespace {

// Group B fix wave (I3) kept FFTW's plans for this harness here. The parity
// mini-round moved that to tests/TestFftwWisdomCache.cpp, which
// nereus_add_test() links into every test that starts WDSP without its
// wisdom step, this harness's included; one file per build directory.

void installOpenAudioBuses(AudioEngine& engine)
{
    AudioFormat format;
    format.sampleRate = 48000;
    format.channels = 2;
    format.sample = AudioFormat::Sample::Float32;

    auto speakers = std::make_unique<FakeAudioBus>(QStringLiteral("Fake speakers"));
    auto txInput = std::make_unique<FakeAudioBus>(QStringLiteral("Fake TX input"));
    const bool speakersOpened = speakers->open(format);
    const bool txInputOpened = txInput->open(format);
    Q_ASSERT(speakersOpened);
    Q_ASSERT(txInputOpened);
    Q_UNUSED(speakersOpened);
    Q_UNUSED(txInputOpened);

    engine.setSpeakersBusForTest(std::move(speakers));
    engine.setTxInputBusForTest(std::move(txInput));
}

} // namespace

ConnectableRadioModel::~ConnectableRadioModel()
{
    // Member order as the header says: the model before the fake.
    m_model.reset();
    m_fake.reset();
}

std::unique_ptr<ConnectableRadioModel> ConnectableRadioModel::create(
    int timeoutMs, NereusSDR::RadioModel::Role role,
    std::function<void(NereusSDR::RadioModel&)> beforeConnect,
    NereusSDR::HPSDRHW board,
    bool stream)
{
    // ConnectableRadioModel's constructor is private (see the header), so
    // std::make_unique can't reach it from outside the class; new + wrap
    // is the standard workaround for a factory that IS a member function.
    std::unique_ptr<ConnectableRadioModel> harness(new ConnectableRadioModel());

    harness->m_fake = std::make_unique<P1FakeRadio>();
    if (!stream) {
        harness->m_fake->setAutoStreamEnabled(false);
        harness->m_keepAlive = std::make_unique<QTimer>();
        harness->m_keepAlive->setInterval(500);
        P1FakeRadio* const fake = harness->m_fake.get();
        QObject::connect(harness->m_keepAlive.get(), &QTimer::timeout,
                         [fake]() { fake->sendEp6Frames(1); });
    }
    harness->m_fake->start();
    if (harness->m_keepAlive) {
        harness->m_keepAlive->start();
    }

    // Mirrors tst_p1_loopback_connection.cpp's makeInfo() -- see
    // task-2-controller-notes.md "Wiring the fake". Built the same way
    // for both roles: a Role::Remote model's connectToRadio() never reads
    // any of it (its early return fires first), but building it
    // unconditionally keeps this function's shape simple and gives
    // radioInfo() a real value either way.
    harness->m_info.address         = harness->m_fake->localAddress();
    harness->m_info.port            = harness->m_fake->localPort();
    harness->m_info.boardType       = board;
    harness->m_info.protocol        = ProtocolVersion::Protocol1;
    harness->m_info.macAddress      = QStringLiteral("aa:bb:cc:11:22:33");
    harness->m_info.firmwareVersion = 72;
    harness->m_info.name            = QStringLiteral("ConnectableRadioModel fake");

    harness->m_model = std::make_unique<NereusSDR::RadioModel>(role);
    // This fixture verifies the real WDSP, receiver, radio-loopback and
    // reconnect lifecycle. Physical host audio is an unrelated integration
    // boundary with its own PortAudio coverage. stop() releases its buses, so
    // install fresh opened fakes before every start(), including reconnects.
    harness->m_model->audioEngine()->setStartInitializerForTest(
        installOpenAudioBuses);

    // Arm the synchronous test-only WdspEngine init path BEFORE calling
    // connectToRadio(). Order matters: connectToRadio() wires its
    // RX/TX-channel-creation lambda to WdspEngine::initializedChanged()
    // and only THEN calls initialize() unconditionally, so whichever path
    // initialize() takes has to already be selected by the time that call
    // happens. See WdspEngine::setSynchronousInitForTest()'s doc comment
    // for the full reasoning (including why a flag consulted inside
    // initialize() is required instead of pre-setting m_initialized).
    // Harmless to arm for Role::Remote too: initialize() is never called
    // on that path, so the flag just goes unread.
    harness->m_model->wdspEngine()->setSynchronousInitForTest(true);

    if (beforeConnect) {
        beforeConnect(*harness->m_model);
    }

    harness->m_model->connectToRadio(harness->m_info);

    if (role == NereusSDR::RadioModel::Role::Remote) {
        // Remote-daemon R2 Task 4: a Role::Remote model's connectToRadio()
        // returns immediately behind RadioModel's own early guard -- it
        // never reaches ConnectionState::Connected, so waiting for that
        // below would just burn timeoutMs and report a false failure. The
        // harness is already fully built; hand it back as-is.
        return harness;
    }

    NereusSDR::RadioModel* const model = harness->m_model.get();
    const bool reachedConnected = QTest::qWaitFor(
        [model]() {
            return model->connectionState() == NereusSDR::ConnectionState::Connected;
        },
        timeoutMs);

    if (!reachedConnected) {
        // harness's destructor tears down both m_model and m_fake in the
        // right order (see the header's member-order comment) -- a timed-
        // out connect leaks neither the socket nor the RadioModel.
        return nullptr;
    }

    // R-R3-39 (Task 32): the TX channel opens on the transmit lane now, not
    // inside connectToRadio, so a connected model may still be planning its
    // FFTs there (cold wisdom: tens of seconds). Wait for it the way the
    // connect used to, with the event loop running so the fake keeps
    // streaming; a test that then blocks the event loop on the receive lane
    // no longer outlasts the connection watchdog.
    if (NereusSDR::DspControlThread* lane = model->transmitLane()) {
        constexpr int kTxOpenTimeoutMs = 600000;
        if (!QTest::qWaitFor([lane]() { return lane->waitIdleForTest(0); },
                             kTxOpenTimeoutMs)) {
            return nullptr;
        }
    }
    // R-R3-49 load round: the receive channels open on the receive lane in
    // the same way, and Connected does not wait for them either. The fake
    // radio used to share the test's thread, which slowed the connect enough
    // to hide it; streaming from a thread of its own, it let
    // tst_remote_dsp_info read channel 0 before the lane had opened it.
    if (NereusSDR::DspControlThread* lane = model->receiveLane()) {
        constexpr int kRxOpenTimeoutMs = 600000;
        if (!QTest::qWaitFor([lane]() { return lane->waitIdleForTest(0); },
                             kRxOpenTimeoutMs)) {
            return nullptr;
        }
    }

    return harness;
}

} // namespace NereusSDR::Test
