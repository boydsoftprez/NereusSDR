// =================================================================
// tests/tst_audio_stream_supervisor.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test.
//
// V-SW-4 (R-AUD-08 to R-AUD-14) and the retry part of V-SW-1: the stream
// supervisor with a fake catalogue and a fake host, no audio device.
// Speakers and headphones fall back to the system default at once and
// return; the mic and VAX go silent and are never moved to another device;
// busy is handled as lost; "(platform default)" follows the system
// default, the mic never while transmitting and never to Bluetooth; the
// retry schedule; a Bluetooth mic opens before the outputs.
//
// Timing: the retry timers are scaled far out (setRetryScaleForTest(1000))
// and fired by hand with fireRetryForTest(), so the schedule is checked
// without the wall clock.  The two timer-driven cases only wait for an
// outcome (QTRY), never for a bound.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 5. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-10-09: early-review fix wave follow-up (R-AUD-08, R-AUD-14):
//               startMicOnly() opens the mic alone; startOutputs() opens
//               the outputs without reopening it.  J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan Task 13 (R-AUD-11): a fallback on a
//               default that is the chosen device plays as the chosen
//               device, with no retry.  J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioDeviceConfig.h"
#include "core/IAudioBus.h"
#include "core/audio/AudioDeviceTypes.h"
#include "core/audio/AudioStreamSupervisor.h"
#include "core/audio/IAudioDeviceCatalog.h"
#include "core/audio/IAudioStreamHost.h"

#include <QCoreApplication>
#include <QHash>
#include <QLoggingCategory>
#include <QSignalSpy>

#include <memory>
#include <optional>

using namespace NereusSDR;

namespace {

constexpr AudioBackendId kBackend = AudioBackendId::CoreAudio;
constexpr AudioEngineKind kEngine = AudioEngineKind::CoreAudio;

AudioDeviceInfo device(AudioDeviceDirection direction, const QString& id, const QString& name,
                       AudioTransport transport = AudioTransport::Usb)
{
    AudioDeviceInfo info;
    info.backend = kBackend;
    info.direction = direction;
    info.id = id;
    info.name = name;
    info.transport = transport;
    return info;
}

AudioDeviceInfo output(const QString& id, const QString& name, AudioTransport transport = AudioTransport::Usb)
{
    return device(AudioDeviceDirection::Output, id, name, transport);
}

AudioDeviceInfo input(const QString& id, const QString& name, AudioTransport transport = AudioTransport::Usb)
{
    return device(AudioDeviceDirection::Input, id, name, transport);
}

AudioDeviceConfig chosen(const QString& id, const QString& name)
{
    AudioDeviceConfig cfg;
    cfg.engine = kEngine;
    cfg.deviceId = id;
    cfg.deviceName = name;
    return cfg;
}

AudioDeviceConfig platformDefault()
{
    AudioDeviceConfig cfg;
    cfg.engine = kEngine;
    return cfg;
}

AudioDeviceConfig noneChoice()
{
    AudioDeviceConfig cfg;
    cfg.engine = kEngine;
    cfg.deviceId = QString::fromLatin1(kAudioDeviceNone);
    return cfg;
}

AudioStreamEvent streamEvent(AudioStreamEvent::Kind kind)
{
    AudioStreamEvent e;
    e.kind = kind;
    return e;
}

class FakeCatalogue final : public IAudioDeviceCatalog {
public:
    QList<AudioBackendId> backends() const override { return {kBackend}; }
    bool backendRunning(AudioBackendId id) const override { return id == kBackend; }

    QList<AudioDeviceInfo> devices(AudioBackendId id, AudioDeviceDirection direction) const override
    {
        if (id != kBackend) {
            return {};
        }
        QList<AudioDeviceInfo> out = list(direction);
        for (AudioDeviceInfo& d : out) {
            d.isDefault = d.id == defaultId(direction);
        }
        return out;
    }

    std::optional<AudioDeviceInfo> defaultDevice(AudioBackendId id,
                                                 AudioDeviceDirection direction) const override
    {
        const QList<AudioDeviceInfo> all = devices(id, direction);
        for (const AudioDeviceInfo& d : all) {
            if (d.isDefault) {
                return d;
            }
        }
        return std::nullopt;
    }

    void rescanOlderDrivers() override {}

    void add(const AudioDeviceInfo& info) { list(info.direction).append(info); }
    void remove(AudioDeviceDirection direction, const QString& id)
    {
        QList<AudioDeviceInfo>& l = list(direction);
        for (int i = l.size() - 1; i >= 0; --i) {
            if (l[i].id == id) {
                l.removeAt(i);
            }
        }
    }
    void setDefault(AudioDeviceDirection direction, const QString& id)
    {
        (direction == AudioDeviceDirection::Output ? m_defaultOut : m_defaultIn) = id;
    }
    void changed() { emit devicesChanged(); }
    void defaultMoved(AudioDeviceDirection direction) { emit defaultChanged(direction); }

private:
    QList<AudioDeviceInfo>& list(AudioDeviceDirection d)
    {
        return d == AudioDeviceDirection::Output ? m_outputs : m_inputs;
    }
    const QList<AudioDeviceInfo>& list(AudioDeviceDirection d) const
    {
        return d == AudioDeviceDirection::Output ? m_outputs : m_inputs;
    }
    QString defaultId(AudioDeviceDirection d) const
    {
        return d == AudioDeviceDirection::Output ? m_defaultOut : m_defaultIn;
    }

    QList<AudioDeviceInfo> m_outputs;
    QList<AudioDeviceInfo> m_inputs;
    QString m_defaultOut;
    QString m_defaultIn;
};

struct OpenCall {
    AudioRole role;
    AudioEngineKind engine;
    std::optional<AudioDeviceInfo> device;
};

// Results come from a per-device script (first), then a per-device sticky
// result, else Opened.  The key is the device ID, empty for the default.
class FakeHost final : public IAudioStreamHost {
public:
    AudioOpenResult openRole(AudioRole role, AudioEngineKind engine,
                             const std::optional<AudioDeviceInfo>& device,
                             const AudioDeviceConfig& /*config*/) override
    {
        opens.append({role, engine, device});
        log.append(QStringLiteral("open %1 %2").arg(int(role)).arg(device ? device->id : QStringLiteral("default")));
        const QString key = device ? device->id : QString();
        QList<AudioOpenResult>& script = scripted[key];
        if (!script.isEmpty()) {
            return script.takeFirst();
        }
        return sticky.value(key, AudioOpenResult::Opened);
    }

    void closeRole(AudioRole role) override
    {
        closes.append(role);
        log.append(QStringLiteral("close %1").arg(int(role)));
    }

    QList<OpenCall> opensFor(AudioRole role) const
    {
        QList<OpenCall> out;
        for (const OpenCall& c : opens) {
            if (c.role == role) {
                out.append(c);
            }
        }
        return out;
    }

    int closesFor(AudioRole role) const { return int(closes.count(role)); }

    void clear()
    {
        opens.clear();
        closes.clear();
        log.clear();
    }

    QList<OpenCall> opens;
    QList<AudioRole> closes;
    QStringList log;
    QHash<QString, QList<AudioOpenResult>> scripted;
    QHash<QString, AudioOpenResult> sticky;
};

struct Rig {
    FakeCatalogue catalogue;
    FakeHost host;
    std::unique_ptr<AudioStreamSupervisor> sup;

    Rig()
    {
        sup = std::make_unique<AudioStreamSupervisor>(catalogue, host);
        sup->setRetryScaleForTest(1000.0);   // timers never fire by themselves
    }

    // The supervisor's first evaluation is posted to the event loop.
    void start() { QCoreApplication::processEvents(); }

    AudioRoleStatus st(AudioRole role) const { return sup->status(role); }
};

const QString kUsbId = QStringLiteral("uid-usb");
const QString kUsbName = QStringLiteral("USB DAC");
const QString kBuiltId = QStringLiteral("uid-built");
const QString kBuiltName = QStringLiteral("MacBook Speakers");
const QString kMicBuiltId = QStringLiteral("uid-mic-built");
const QString kMicBuiltName = QStringLiteral("MacBook Microphone");
const QString kMicUsbId = QStringLiteral("uid-mic-usb");
const QString kMicUsbName = QStringLiteral("USB Mic");
const QString kPodsOutId = QStringLiteral("uid-pods-out");
const QString kPodsInId = QStringLiteral("uid-pods-in");
const QString kPodsName = QStringLiteral("AirPods");

void addStandardOutputs(FakeCatalogue& c)
{
    c.add(output(kBuiltId, kBuiltName, AudioTransport::BuiltIn));
    c.add(output(kUsbId, kUsbName));
    c.setDefault(AudioDeviceDirection::Output, kBuiltId);
}

void addStandardInputs(FakeCatalogue& c)
{
    c.add(input(kMicBuiltId, kMicBuiltName, AudioTransport::BuiltIn));
    c.add(input(kMicUsbId, kMicUsbName));
    c.setDefault(AudioDeviceDirection::Input, kMicBuiltId);
}

bool anyBluetoothInputOpened(const FakeHost& host)
{
    for (const OpenCall& c : host.opens) {
        if (c.role == AudioRole::TxInput && c.device && c.device->transport == AudioTransport::Bluetooth) {
            return true;
        }
    }
    return false;
}

} // namespace

class TstAudioStreamSupervisor : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()
    {
        // The supervisor logs a failed open once per run of failures; the
        // cases below fail opens on purpose.
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.audio.warning=false"));
    }

    // R-AUD-08: a chosen speaker or headphone device that goes away plays
    // on the default at once, and the device opens again when it returns.
    void outputLostFallsBackAtOnceAndReturns_data()
    {
        QTest::addColumn<AudioRole>("role");
        QTest::newRow("speakers") << AudioRole::Speakers;
        QTest::newRow("headphones") << AudioRole::Headphones;
    }
    void outputLostFallsBackAtOnceAndReturns()
    {
        QFETCH(AudioRole, role);
        Rig rig;
        addStandardOutputs(rig.catalogue);
        rig.sup->setChoice(role, chosen(kUsbId, kUsbName));
        rig.start();

        QCOMPARE(rig.host.opensFor(role).size(), 1);
        QCOMPARE(rig.host.opensFor(role).last().device->id, kUsbId);
        QCOMPARE(rig.host.opensFor(role).last().engine, kEngine);
        QCOMPARE(rig.st(role).state, AudioRoleState::Playing);
        QCOMPARE(rig.st(role).playingName, kUsbName);
        QCOMPARE(rig.st(role).chosenName, kUsbName);

        rig.host.clear();
        // The stream's error, before any list change.
        rig.sup->onStreamEvent(role, streamEvent(AudioStreamEvent::Kind::DeviceLost));
        QCOMPARE(rig.host.opensFor(role).size(), 1);
        QVERIFY(!rig.host.opensFor(role).last().device.has_value());   // the system default
        QCOMPARE(rig.st(role).state, AudioRoleState::PlayingOnDefault);
        QCOMPARE(rig.st(role).reason, AudioRoleReason::NotConnected);
        QCOMPARE(rig.st(role).playingName, kBuiltName);
        QCOMPARE(rig.st(role).chosenName, kUsbName);

        // The list catches up: the device is gone, no retries.
        rig.catalogue.remove(AudioDeviceDirection::Output, kUsbId);
        rig.catalogue.changed();
        QCOMPARE(rig.sup->retryDelayMsForTest(role), -1);
        QCOMPARE(rig.host.opensFor(role).size(), 1);
        QCOMPARE(rig.st(role).state, AudioRoleState::PlayingOnDefault);

        // It returns: opened again on the 250 ms retry.
        rig.catalogue.add(output(kUsbId, kUsbName));
        rig.catalogue.changed();
        QCOMPARE(rig.sup->retryDelayMsForTest(role), 250);
        QCOMPARE(rig.host.opensFor(role).size(), 1);
        rig.sup->fireRetryForTest(role);
        QCOMPARE(rig.host.opensFor(role).size(), 2);
        QCOMPARE(rig.host.opensFor(role).last().device->id, kUsbId);
        QCOMPARE(rig.st(role).state, AudioRoleState::Playing);
        QCOMPARE(rig.st(role).reason, AudioRoleReason::None);
        QCOMPARE(rig.st(role).playingName, kUsbName);
        QCOMPARE(rig.sup->retryDelayMsForTest(role), -1);
    }

    // An open that fails while the device is listed: 250, 500, 1000, 2000,
    // then every 2000 ms; leaving the list stops it; returning restarts it.
    void failedOpenRetriedOnSchedule()
    {
        Rig rig;
        addStandardOutputs(rig.catalogue);
        rig.host.sticky.insert(kUsbId, AudioOpenResult::Failed);
        rig.sup->setChoice(AudioRole::Speakers, chosen(kUsbId, kUsbName));
        rig.start();

        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::PlayingOnDefault);
        QCOMPARE(rig.st(AudioRole::Speakers).reason, AudioRoleReason::NotConnected);
        const QList<int> expected = {250, 500, 1000, 2000, 2000, 2000, 2000};
        for (int i = 0; i < expected.size(); ++i) {
            QCOMPARE(rig.sup->retryDelayMsForTest(AudioRole::Speakers), expected[i]);
            rig.sup->fireRetryForTest(AudioRole::Speakers);
        }
        int attempts = 0;
        for (const OpenCall& c : rig.host.opensFor(AudioRole::Speakers)) {
            if (c.device && c.device->id == kUsbId) {
                ++attempts;
            }
        }
        QCOMPARE(attempts, 1 + expected.size());

        // The default stream is kept through the failures (opened once).
        int defaultOpens = 0;
        for (const OpenCall& c : rig.host.opensFor(AudioRole::Speakers)) {
            if (!c.device) {
                ++defaultOpens;
            }
        }
        QCOMPARE(defaultOpens, 1);

        rig.catalogue.remove(AudioDeviceDirection::Output, kUsbId);
        rig.catalogue.changed();
        QCOMPARE(rig.sup->retryDelayMsForTest(AudioRole::Speakers), -1);
        const int before = int(rig.host.opens.size());
        rig.sup->fireRetryForTest(AudioRole::Speakers);   // nothing armed
        QCOMPARE(int(rig.host.opens.size()), before);

        rig.catalogue.add(output(kUsbId, kUsbName));
        rig.catalogue.changed();
        QCOMPARE(rig.sup->retryDelayMsForTest(AudioRole::Speakers), 250);
    }

    // V-SW-1, retry part: a returning Bluetooth device that fails its first
    // opens is retried on the schedule, on the supervisor's own timers.
    void returningBluetoothDeviceRetriedOnTimers()
    {
        Rig rig;
        addStandardOutputs(rig.catalogue);
        rig.sup->setChoice(AudioRole::Headphones, chosen(kPodsOutId, kPodsName));
        rig.start();
        QCOMPARE(rig.st(AudioRole::Headphones).state, AudioRoleState::PlayingOnDefault);
        QCOMPARE(rig.st(AudioRole::Headphones).reason, AudioRoleReason::NotConnected);
        QCOMPARE(rig.sup->retryDelayMsForTest(AudioRole::Headphones), -1);

        rig.sup->setRetryScaleForTest(0.01);
        rig.host.scripted.insert(kPodsOutId, {AudioOpenResult::Failed, AudioOpenResult::Failed});
        rig.catalogue.add(output(kPodsOutId, kPodsName, AudioTransport::Bluetooth));
        rig.catalogue.changed();

        QTRY_COMPARE(rig.st(AudioRole::Headphones).state, AudioRoleState::Playing);
        int attempts = 0;
        for (const OpenCall& c : rig.host.opensFor(AudioRole::Headphones)) {
            if (c.device && c.device->id == kPodsOutId) {
                ++attempts;
            }
        }
        QCOMPARE(attempts, 3);
        QVERIFY(rig.st(AudioRole::Headphones).playingBluetooth);
        QCOMPARE(rig.st(AudioRole::Headphones).playingName, kPodsName);
    }

    // R-AUD-11, D30: busy is handled as lost, with InUse, and moves back
    // when an open succeeds.
    void busyHandledAsLost()
    {
        Rig rig;
        addStandardOutputs(rig.catalogue);
        rig.sup->setChoice(AudioRole::Speakers, chosen(kUsbId, kUsbName));
        rig.start();
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::Playing);

        rig.host.clear();
        rig.sup->onStreamEvent(AudioRole::Speakers, streamEvent(AudioStreamEvent::Kind::DeviceBusy));
        QCOMPARE(rig.host.opensFor(AudioRole::Speakers).size(), 1);
        QVERIFY(!rig.host.opensFor(AudioRole::Speakers).last().device.has_value());
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::PlayingOnDefault);
        QCOMPARE(rig.st(AudioRole::Speakers).reason, AudioRoleReason::InUse);
        QCOMPARE(rig.st(AudioRole::Speakers).playingName, kBuiltName);

        rig.host.scripted.insert(kUsbId, {AudioOpenResult::InUse});
        QCOMPARE(rig.sup->retryDelayMsForTest(AudioRole::Speakers), 250);
        rig.sup->fireRetryForTest(AudioRole::Speakers);
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::PlayingOnDefault);
        QCOMPARE(rig.st(AudioRole::Speakers).reason, AudioRoleReason::InUse);
        QCOMPARE(rig.sup->retryDelayMsForTest(AudioRole::Speakers), 500);
        rig.sup->fireRetryForTest(AudioRole::Speakers);
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::Playing);
        QCOMPARE(rig.st(AudioRole::Speakers).reason, AudioRoleReason::None);
    }

    void busyAtFirstOpenPlaysOnDefault()
    {
        Rig rig;
        addStandardOutputs(rig.catalogue);
        rig.host.scripted.insert(kUsbId, {AudioOpenResult::InUse});
        rig.sup->setChoice(AudioRole::Headphones, chosen(kUsbId, kUsbName));
        rig.start();
        QCOMPARE(rig.st(AudioRole::Headphones).state, AudioRoleState::PlayingOnDefault);
        QCOMPARE(rig.st(AudioRole::Headphones).reason, AudioRoleReason::InUse);
        QCOMPARE(rig.sup->retryDelayMsForTest(AudioRole::Headphones), 250);
        rig.sup->fireRetryForTest(AudioRole::Headphones);
        QCOMPARE(rig.st(AudioRole::Headphones).state, AudioRoleState::Playing);
    }

    // Carried finding (Task 9): the chosen device is also the system
    // default.  When it is taken and the fallback opens the default, that
    // is the chosen device playing again (same engine, same config): it
    // reads Playing on it and arms no retry, which could only collide with
    // that open or reopen it with a gap.
    void busyWhenChosenIsDefaultPlaysOnItWithoutRetry()
    {
        Rig rig;
        addStandardOutputs(rig.catalogue);
        rig.catalogue.setDefault(AudioDeviceDirection::Output, kUsbId);
        rig.sup->setChoice(AudioRole::Speakers, chosen(kUsbId, kUsbName));
        rig.start();
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::Playing);

        rig.host.clear();
        rig.sup->onStreamEvent(AudioRole::Speakers, streamEvent(AudioStreamEvent::Kind::DeviceBusy));
        QCOMPARE(rig.host.opensFor(AudioRole::Speakers).size(), 1);
        QVERIFY(!rig.host.opensFor(AudioRole::Speakers).last().device.has_value());
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::Playing);
        QCOMPARE(rig.st(AudioRole::Speakers).reason, AudioRoleReason::None);
        QCOMPARE(rig.st(AudioRole::Speakers).playingName, kUsbName);
        QCOMPARE(rig.sup->retryDelayMsForTest(AudioRole::Speakers), -1);

        // Its next list change keeps it: no reopen.
        rig.host.clear();
        rig.catalogue.changed();
        QCoreApplication::processEvents();
        QCOMPARE(rig.host.opensFor(AudioRole::Speakers).size(), 0);
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::Playing);
    }

    void busyAtFirstOpenOnDefaultChosenPlaysOnIt()
    {
        Rig rig;
        addStandardOutputs(rig.catalogue);
        rig.catalogue.setDefault(AudioDeviceDirection::Output, kUsbId);
        rig.host.scripted.insert(kUsbId, {AudioOpenResult::InUse});
        rig.sup->setChoice(AudioRole::Headphones, chosen(kUsbId, kUsbName));
        rig.start();
        QCOMPARE(rig.st(AudioRole::Headphones).state, AudioRoleState::Playing);
        QCOMPARE(rig.st(AudioRole::Headphones).reason, AudioRoleReason::None);
        QCOMPARE(rig.st(AudioRole::Headphones).playingName, kUsbName);
        QCOMPARE(rig.sup->retryDelayMsForTest(AudioRole::Headphones), -1);

        // A default that is held too stays in use on the schedule.
        Rig held;
        addStandardOutputs(held.catalogue);
        held.catalogue.setDefault(AudioDeviceDirection::Output, kUsbId);
        held.host.scripted.insert(kUsbId, {AudioOpenResult::InUse});
        held.host.scripted.insert(QString(), {AudioOpenResult::InUse});
        held.sup->setChoice(AudioRole::Headphones, chosen(kUsbId, kUsbName));
        held.start();
        QCOMPARE(held.st(AudioRole::Headphones).reason, AudioRoleReason::InUse);
        QCOMPARE(held.sup->retryDelayMsForTest(AudioRole::Headphones), 250);
        held.sup->fireRetryForTest(AudioRole::Headphones);
        QCOMPARE(held.st(AudioRole::Headphones).state, AudioRoleState::Playing);
        QCOMPARE(held.st(AudioRole::Headphones).playingName, kUsbName);
    }

    // R-AUD-09, R-AUD-10, D5: the mic and VAX go silent and are never moved
    // to another device; they come back by themselves.
    void micAndVaxLostStaySilent_data()
    {
        QTest::addColumn<AudioRole>("role");
        QTest::newRow("mic") << AudioRole::TxInput;
        QTest::newRow("vax1") << AudioRole::Vax1;
        QTest::newRow("vax2") << AudioRole::Vax2;
        QTest::newRow("vax3") << AudioRole::Vax3;
        QTest::newRow("vax4") << AudioRole::Vax4;
    }
    void micAndVaxLostStaySilent()
    {
        QFETCH(AudioRole, role);
        const bool isMic = role == AudioRole::TxInput;
        const AudioDeviceDirection dir = isMic ? AudioDeviceDirection::Input : AudioDeviceDirection::Output;
        Rig rig;
        addStandardOutputs(rig.catalogue);
        addStandardInputs(rig.catalogue);
        const QString id = isMic ? kMicUsbId : kUsbId;
        const QString name = isMic ? kMicUsbName : kUsbName;
        rig.sup->setChoice(role, chosen(id, name));
        rig.start();
        QCOMPARE(rig.st(role).state, AudioRoleState::Playing);

        rig.host.clear();
        rig.sup->onStreamEvent(role, streamEvent(AudioStreamEvent::Kind::DeviceLost));
        QCOMPARE(rig.host.closesFor(role), 1);
        QCOMPARE(rig.host.opensFor(role).size(), 0);
        QCOMPARE(rig.st(role).state, AudioRoleState::Silent);
        QCOMPARE(rig.st(role).reason, AudioRoleReason::NotConnected);
        QVERIFY(rig.st(role).playingName.isEmpty());

        // Nothing moves it: the list, a default change, a transmission.
        rig.catalogue.remove(dir, id);
        rig.catalogue.changed();
        rig.catalogue.setDefault(dir, isMic ? kMicBuiltId : kBuiltId);
        rig.catalogue.defaultMoved(dir);
        rig.sup->setTransmitting(true);
        rig.sup->setTransmitting(false);
        rig.catalogue.changed();
        QCOMPARE(rig.host.opensFor(role).size(), 0);
        QCOMPARE(rig.st(role).state, AudioRoleState::Silent);
        QCOMPARE(rig.st(role).reason, AudioRoleReason::NotConnected);

        // Back by itself.
        rig.catalogue.add(device(dir, id, name));
        rig.catalogue.changed();
        QCOMPARE(rig.sup->retryDelayMsForTest(role), 250);
        rig.sup->fireRetryForTest(role);
        QCOMPARE(rig.st(role).state, AudioRoleState::Playing);
        QCOMPARE(rig.st(role).playingName, name);
        for (const OpenCall& c : rig.host.opensFor(role)) {
            QVERIFY(c.device.has_value());
            QCOMPARE(c.device->id, id);
        }
    }

    void micFailedOpenStaysSilentAndRetries()
    {
        Rig rig;
        addStandardInputs(rig.catalogue);
        rig.host.sticky.insert(kMicUsbId, AudioOpenResult::Pending);
        rig.sup->setChoice(AudioRole::TxInput, chosen(kMicUsbId, kMicUsbName));
        rig.start();
        QCOMPARE(rig.st(AudioRole::TxInput).state, AudioRoleState::Silent);
        QCOMPARE(rig.st(AudioRole::TxInput).reason, AudioRoleReason::None);

        rig.sup->onOpenFinished(AudioRole::TxInput, AudioOpenResult::NotFound);
        QCOMPARE(rig.st(AudioRole::TxInput).state, AudioRoleState::Silent);
        QCOMPARE(rig.st(AudioRole::TxInput).reason, AudioRoleReason::NotConnected);
        QCOMPARE(rig.sup->retryDelayMsForTest(AudioRole::TxInput), 250);

        rig.sup->fireRetryForTest(AudioRole::TxInput);
        rig.sup->onOpenFinished(AudioRole::TxInput, AudioOpenResult::Opened);
        QCOMPARE(rig.st(AudioRole::TxInput).state, AudioRoleState::Playing);
        QCOMPARE(rig.st(AudioRole::TxInput).playingName, kMicUsbName);
        for (const OpenCall& c : rig.host.opensFor(AudioRole::TxInput)) {
            QCOMPARE(c.device->id, kMicUsbId);
        }
    }

    void noDefaultDeviceOutputsSilent()
    {
        Rig rig;
        rig.sup->setChoice(AudioRole::Speakers, platformDefault());
        rig.sup->setChoice(AudioRole::Headphones, platformDefault());
        rig.start();
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::Silent);
        QCOMPARE(rig.st(AudioRole::Speakers).reason, AudioRoleReason::NoDevice);
        QCOMPARE(rig.st(AudioRole::Headphones).state, AudioRoleState::Silent);
        QCOMPARE(rig.st(AudioRole::Headphones).reason, AudioRoleReason::NoDevice);
        QCOMPARE(rig.host.opens.size(), 0);

        addStandardOutputs(rig.catalogue);
        rig.catalogue.changed();
        rig.catalogue.defaultMoved(AudioDeviceDirection::Output);
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::Playing);
        QCOMPARE(rig.st(AudioRole::Speakers).playingName, kBuiltName);
        QVERIFY(rig.st(AudioRole::Speakers).chosenName.isEmpty());
    }

    // R-AUD-12, D27.
    void platformDefaultOutputsFollowDefault()
    {
        Rig rig;
        addStandardOutputs(rig.catalogue);
        rig.sup->setChoice(AudioRole::Speakers, platformDefault());
        rig.start();
        QCOMPARE(rig.host.opensFor(AudioRole::Speakers).size(), 1);
        QVERIFY(!rig.host.opensFor(AudioRole::Speakers).last().device.has_value());
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::Playing);
        QCOMPARE(rig.st(AudioRole::Speakers).playingName, kBuiltName);

        rig.catalogue.add(output(kPodsOutId, kPodsName, AudioTransport::Bluetooth));
        rig.catalogue.changed();
        QCOMPARE(rig.host.opensFor(AudioRole::Speakers).size(), 1);   // a list change alone never reopens
        rig.catalogue.setDefault(AudioDeviceDirection::Output, kPodsOutId);
        rig.catalogue.defaultMoved(AudioDeviceDirection::Output);
        QCOMPARE(rig.host.opensFor(AudioRole::Speakers).size(), 2);
        QVERIFY(!rig.host.opensFor(AudioRole::Speakers).last().device.has_value());
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::Playing);
        QCOMPARE(rig.st(AudioRole::Speakers).playingName, kPodsName);
        QVERIFY(rig.st(AudioRole::Speakers).playingBluetooth);
    }

    // R-AUD-13, D28: the mic on "(platform default)" follows the default
    // input, but not while transmitting (applied at unkey).
    void platformMicFollowsDefaultNotWhileTransmitting()
    {
        Rig rig;
        addStandardInputs(rig.catalogue);
        rig.sup->setChoice(AudioRole::TxInput, platformDefault());
        rig.start();
        QCOMPARE(rig.host.opensFor(AudioRole::TxInput).size(), 1);
        QCOMPARE(rig.host.opensFor(AudioRole::TxInput).last().device->id, kMicBuiltId);   // explicit, never "the engine's default"
        QCOMPARE(rig.st(AudioRole::TxInput).state, AudioRoleState::Playing);
        QCOMPARE(rig.st(AudioRole::TxInput).playingName, kMicBuiltName);

        rig.catalogue.setDefault(AudioDeviceDirection::Input, kMicUsbId);
        rig.catalogue.defaultMoved(AudioDeviceDirection::Input);
        QCOMPARE(rig.host.opensFor(AudioRole::TxInput).size(), 2);
        QCOMPARE(rig.host.opensFor(AudioRole::TxInput).last().device->id, kMicUsbId);
        QCOMPARE(rig.st(AudioRole::TxInput).playingName, kMicUsbName);

        rig.sup->setTransmitting(true);
        rig.catalogue.setDefault(AudioDeviceDirection::Input, kMicBuiltId);
        rig.catalogue.defaultMoved(AudioDeviceDirection::Input);
        rig.catalogue.changed();
        QCOMPARE(rig.host.opensFor(AudioRole::TxInput).size(), 2);
        QCOMPARE(rig.st(AudioRole::TxInput).playingName, kMicUsbName);

        rig.sup->setTransmitting(false);
        QCOMPARE(rig.host.opensFor(AudioRole::TxInput).size(), 3);
        QCOMPARE(rig.host.opensFor(AudioRole::TxInput).last().device->id, kMicBuiltId);
        QCOMPARE(rig.st(AudioRole::TxInput).playingName, kMicBuiltName);
        QCOMPARE(rig.host.closesFor(AudioRole::TxInput), 0);
    }

    // R-AUD-13, D6: never follows to a Bluetooth mic.
    void platformMicNeverFollowsToBluetooth()
    {
        Rig rig;
        addStandardInputs(rig.catalogue);
        rig.sup->setChoice(AudioRole::TxInput, platformDefault());
        rig.start();
        QCOMPARE(rig.st(AudioRole::TxInput).playingName, kMicBuiltName);

        rig.catalogue.add(input(kPodsInId, kPodsName, AudioTransport::Bluetooth));
        rig.catalogue.changed();
        rig.catalogue.setDefault(AudioDeviceDirection::Input, kPodsInId);
        rig.catalogue.defaultMoved(AudioDeviceDirection::Input);
        rig.sup->setTransmitting(true);
        rig.sup->setTransmitting(false);
        QCOMPARE(rig.host.opensFor(AudioRole::TxInput).size(), 1);
        QVERIFY(!anyBluetoothInputOpened(rig.host));
        QCOMPARE(rig.st(AudioRole::TxInput).state, AudioRoleState::Playing);
        QCOMPARE(rig.st(AudioRole::TxInput).playingName, kMicBuiltName);
        QVERIFY(!rig.st(AudioRole::TxInput).playingBluetooth);

        // Its mic goes away while the default is Bluetooth: still never the headset.
        rig.sup->onStreamEvent(AudioRole::TxInput, streamEvent(AudioStreamEvent::Kind::DeviceLost));
        rig.catalogue.remove(AudioDeviceDirection::Input, kMicBuiltId);
        rig.catalogue.changed();
        QVERIFY(!anyBluetoothInputOpened(rig.host));
    }

    // R-AUD-14: a Bluetooth system default at start opens the built-in
    // input, else another non-Bluetooth input, else nothing.
    void platformMicStartsOffBluetoothDefault_data()
    {
        QTest::addColumn<bool>("withBuiltIn");
        QTest::addColumn<bool>("withUsb");
        QTest::addColumn<QString>("expectedId");
        QTest::newRow("built-in") << true << true << kMicBuiltId;
        QTest::newRow("usb") << false << true << kMicUsbId;
        QTest::newRow("nothing") << false << false << QString();
    }
    void platformMicStartsOffBluetoothDefault()
    {
        QFETCH(bool, withBuiltIn);
        QFETCH(bool, withUsb);
        QFETCH(QString, expectedId);
        Rig rig;
        rig.catalogue.add(input(kPodsInId, kPodsName, AudioTransport::Bluetooth));
        if (withUsb) {
            rig.catalogue.add(input(kMicUsbId, kMicUsbName));
        }
        if (withBuiltIn) {
            rig.catalogue.add(input(kMicBuiltId, kMicBuiltName, AudioTransport::BuiltIn));
        }
        rig.catalogue.setDefault(AudioDeviceDirection::Input, kPodsInId);
        rig.sup->setChoice(AudioRole::TxInput, platformDefault());
        rig.start();

        QVERIFY(!anyBluetoothInputOpened(rig.host));
        if (expectedId.isEmpty()) {
            QCOMPARE(rig.host.opensFor(AudioRole::TxInput).size(), 0);
            QCOMPARE(rig.st(AudioRole::TxInput).state, AudioRoleState::Silent);
            QCOMPARE(rig.st(AudioRole::TxInput).reason, AudioRoleReason::NoDevice);
        } else {
            QCOMPARE(rig.host.opensFor(AudioRole::TxInput).size(), 1);
            QCOMPARE(rig.host.opensFor(AudioRole::TxInput).last().device->id, expectedId);
            QCOMPARE(rig.st(AudioRole::TxInput).state, AudioRoleState::Playing);
        }
    }

    // R-AUD-14: a Bluetooth mic picked by name opens.
    void bluetoothMicPickedByNameOpens()
    {
        Rig rig;
        addStandardInputs(rig.catalogue);
        rig.catalogue.add(input(kPodsInId, kPodsName, AudioTransport::Bluetooth));
        rig.sup->setChoice(AudioRole::TxInput, chosen(kPodsInId, kPodsName));
        rig.start();
        QCOMPARE(rig.host.opensFor(AudioRole::TxInput).size(), 1);
        QCOMPARE(rig.host.opensFor(AudioRole::TxInput).last().device->id, kPodsInId);
        QCOMPARE(rig.st(AudioRole::TxInput).state, AudioRoleState::Playing);
        QVERIFY(rig.st(AudioRole::TxInput).playingBluetooth);
    }

    // R-AUD-14, D6: the Bluetooth mic opens first; outputs wait for it.
    void outputsWaitForBluetoothMic()
    {
        Rig rig;
        addStandardOutputs(rig.catalogue);
        addStandardInputs(rig.catalogue);
        rig.catalogue.add(input(kPodsInId, kPodsName, AudioTransport::Bluetooth));
        rig.catalogue.add(output(kPodsOutId, kPodsName, AudioTransport::Bluetooth));
        rig.host.sticky.insert(kPodsInId, AudioOpenResult::Pending);
        // Outputs set first: the order of setChoice does not matter.
        rig.sup->setChoice(AudioRole::Speakers, chosen(kPodsOutId, kPodsName));
        rig.sup->setChoice(AudioRole::Headphones, platformDefault());
        rig.sup->setChoice(AudioRole::TxInput, chosen(kPodsInId, kPodsName));
        rig.start();

        QCOMPARE(rig.host.opens.size(), 1);
        QCOMPARE(rig.host.opens.first().role, AudioRole::TxInput);
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::Silent);
        rig.catalogue.changed();   // still waiting
        QCOMPARE(rig.host.opens.size(), 1);

        rig.sup->onOpenFinished(AudioRole::TxInput, AudioOpenResult::Opened);
        QCOMPARE(rig.st(AudioRole::TxInput).state, AudioRoleState::Playing);
        QCOMPARE(rig.host.opensFor(AudioRole::Speakers).size(), 1);
        QCOMPARE(rig.host.opensFor(AudioRole::Speakers).last().device->id, kPodsOutId);
        QCOMPARE(rig.host.opensFor(AudioRole::Headphones).size(), 1);
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::Playing);
    }

    // startMicOnly() opens the mic alone: nothing (a list change, the
    // mic's open finishing, the posted start) opens an output until
    // startOutputs(), which opens them on their current choices and leaves
    // the mic as it is.
    void micOnlyStartOpensNoOutput()
    {
        Rig rig;
        addStandardOutputs(rig.catalogue);
        addStandardInputs(rig.catalogue);
        rig.host.sticky.insert(kMicUsbId, AudioOpenResult::Pending);
        rig.sup->setChoice(AudioRole::Speakers, chosen(kUsbId, kUsbName));
        rig.sup->setChoice(AudioRole::Headphones, platformDefault());
        rig.sup->setChoice(AudioRole::TxInput, chosen(kMicUsbId, kMicUsbName));
        rig.sup->startMicOnly();
        QVERIFY(!rig.sup->outputsStarted());
        QCOMPARE(rig.host.opens.size(), 1);
        QCOMPARE(rig.host.opens.first().role, AudioRole::TxInput);

        rig.start();                 // the posted start: nothing more
        rig.catalogue.changed();
        rig.sup->onOpenFinished(AudioRole::TxInput, AudioOpenResult::Opened);
        rig.sup->setChoice(AudioRole::Speakers, chosen(kBuiltId, kBuiltName));
        QCOMPARE(rig.host.opens.size(), 1);
        QCOMPARE(rig.st(AudioRole::TxInput).state, AudioRoleState::Playing);

        rig.sup->startOutputs();
        QVERIFY(rig.sup->outputsStarted());
        QCOMPARE(rig.host.opensFor(AudioRole::TxInput).size(), 1);
        QCOMPARE(rig.host.opensFor(AudioRole::Speakers).size(), 1);
        QCOMPARE(rig.host.opensFor(AudioRole::Speakers).last().device->id, kBuiltId);
        QCOMPARE(rig.host.opensFor(AudioRole::Headphones).size(), 1);
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::Playing);
        rig.sup->startOutputs();     // once
        QCOMPARE(rig.host.opens.size(), 3);
    }

    // R-AUD-14: the outputs open after kBluetoothMicFirstWaitMs (scaled
    // here) when the mic's open has not finished.
    void outputsOpenAfterBluetoothWait()
    {
        Rig rig;
        rig.sup->setRetryScaleForTest(0.005);
        addStandardOutputs(rig.catalogue);
        rig.catalogue.add(input(kPodsInId, kPodsName, AudioTransport::Bluetooth));
        rig.host.sticky.insert(kPodsInId, AudioOpenResult::Pending);
        rig.sup->setChoice(AudioRole::Speakers, chosen(kUsbId, kUsbName));
        rig.sup->setChoice(AudioRole::TxInput, chosen(kPodsInId, kPodsName));
        rig.start();

        QTRY_COMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::Playing);
        QCOMPARE(rig.host.opens.first().role, AudioRole::TxInput);
        QCOMPARE(rig.st(AudioRole::TxInput).state, AudioRoleState::Silent);
    }

    // "(none)": Off, or WaitingForPick on a Core that starts into a
    // desktop; nothing opens either way.
    void noneChoiceOffOrWaitingForPick()
    {
        Rig rig;
        addStandardOutputs(rig.catalogue);
        rig.sup->setChoice(AudioRole::Speakers, noneChoice());
        rig.start();
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::Off);
        rig.sup->setNoneMeansWaitingForPick(AudioRole::Speakers, true);
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::WaitingForPick);
        rig.catalogue.changed();
        rig.catalogue.defaultMoved(AudioDeviceDirection::Output);
        rig.sup->setNoneMeansWaitingForPick(AudioRole::Speakers, false);
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::Off);
        QCOMPARE(rig.host.opens.size(), 0);
        QCOMPARE(rig.host.closes.size(), 0);
    }

    // An empty VAX device never opens the system default (it would be the
    // speakers without a virtual cable); it reads as "(none)".
    void vaxOnPlatformDefaultNeverOpens_data()
    {
        QTest::addColumn<AudioRole>("role");
        QTest::newRow("vax1") << AudioRole::Vax1;
        QTest::newRow("vax2") << AudioRole::Vax2;
        QTest::newRow("vax3") << AudioRole::Vax3;
        QTest::newRow("vax4") << AudioRole::Vax4;
    }
    void vaxOnPlatformDefaultNeverOpens()
    {
        QFETCH(AudioRole, role);
        Rig rig;
        addStandardOutputs(rig.catalogue);
        rig.sup->setChoice(role, platformDefault());
        rig.start();
        QCOMPARE(rig.host.opensFor(role).size(), 0);
        QCOMPARE(rig.st(role).state, AudioRoleState::Off);
        QVERIFY(rig.st(role).playingName.isEmpty());

        rig.catalogue.add(output(kPodsOutId, kPodsName, AudioTransport::Bluetooth));
        rig.catalogue.changed();
        rig.catalogue.setDefault(AudioDeviceDirection::Output, kPodsOutId);
        rig.catalogue.defaultMoved(AudioDeviceDirection::Output);
        QCOMPARE(rig.host.opensFor(role).size(), 0);
        QCOMPARE(rig.st(role).state, AudioRoleState::Off);

        rig.sup->setNoneMeansWaitingForPick(role, true);
        QCOMPARE(rig.st(role).state, AudioRoleState::WaitingForPick);
        rig.sup->setNoneMeansWaitingForPick(role, false);
        QCOMPARE(rig.st(role).state, AudioRoleState::Off);
        QCOMPARE(rig.host.opensFor(role).size(), 0);
        QCOMPARE(rig.host.closesFor(role), 0);
    }

    void disabledRoleIsOff()
    {
        Rig rig;
        addStandardOutputs(rig.catalogue);
        rig.sup->setChoice(AudioRole::Speakers, chosen(kUsbId, kUsbName));
        rig.start();
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::Playing);
        rig.sup->setRoleEnabled(AudioRole::Speakers, false);
        QCOMPARE(rig.host.closesFor(AudioRole::Speakers), 1);
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::Off);
        rig.catalogue.changed();
        QCOMPARE(rig.host.opensFor(AudioRole::Speakers).size(), 1);
    }

    // A name-only match emits the matched ID once; saving it back does not reopen.
    void nameMatchLearnsIdOnce()
    {
        Rig rig;
        addStandardOutputs(rig.catalogue);
        QSignalSpy learned(rig.sup.get(), &AudioStreamSupervisor::savedIdentityLearned);
        rig.sup->setChoice(AudioRole::Speakers, chosen(QString(), kUsbName));
        rig.start();
        QCOMPARE(learned.size(), 1);
        QCOMPARE(learned.first().at(0).value<AudioRole>(), AudioRole::Speakers);
        QCOMPARE(learned.first().at(1).toString(), kUsbId);
        rig.catalogue.changed();
        rig.catalogue.changed();
        QCOMPARE(learned.size(), 1);

        rig.sup->setChoice(AudioRole::Speakers, chosen(kUsbId, kUsbName));
        QCOMPARE(learned.size(), 1);
        QCOMPARE(rig.host.opensFor(AudioRole::Speakers).size(), 1);
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::Playing);
    }

    // A burst of list changes while roles play their chosen devices: no reopen.
    void devicesChangedBurstNoReopen()
    {
        Rig rig;
        addStandardOutputs(rig.catalogue);
        addStandardInputs(rig.catalogue);
        rig.sup->setChoice(AudioRole::Speakers, chosen(kUsbId, kUsbName));
        rig.sup->setChoice(AudioRole::Headphones, chosen(kBuiltId, kBuiltName));
        rig.sup->setChoice(AudioRole::TxInput, chosen(kMicUsbId, kMicUsbName));
        rig.start();
        const AudioRoleStatus speakers = rig.st(AudioRole::Speakers);
        const AudioRoleStatus mic = rig.st(AudioRole::TxInput);
        QSignalSpy changed(rig.sup.get(), &AudioStreamSupervisor::statusChanged);
        rig.host.clear();
        for (int i = 0; i < 10; ++i) {
            if (i == 3) {
                rig.catalogue.add(output(QStringLiteral("uid-hdmi"), QStringLiteral("HDMI"), AudioTransport::Hdmi));
            }
            if (i == 6) {
                rig.catalogue.remove(AudioDeviceDirection::Output, QStringLiteral("uid-hdmi"));
            }
            rig.catalogue.changed();
        }
        QCOMPARE(rig.host.opens.size(), 0);
        QCOMPARE(rig.host.closes.size(), 0);
        QCOMPARE(changed.size(), 0);
        QCOMPARE(rig.st(AudioRole::Speakers), speakers);
        QCOMPARE(rig.st(AudioRole::TxInput), mic);
    }

    // A format change or reset reopens the same device.
    void formatChangeReopensSameDevice()
    {
        Rig rig;
        addStandardOutputs(rig.catalogue);
        rig.sup->setChoice(AudioRole::Speakers, chosen(kUsbId, kUsbName));
        rig.start();
        rig.host.clear();
        rig.sup->onStreamEvent(AudioRole::Speakers, streamEvent(AudioStreamEvent::Kind::FormatChanged));
        QCOMPARE(rig.host.opensFor(AudioRole::Speakers).size(), 1);
        QCOMPARE(rig.host.opensFor(AudioRole::Speakers).last().device->id, kUsbId);
        QCOMPARE(rig.st(AudioRole::Speakers).state, AudioRoleState::Playing);
    }
};

QTEST_GUILESS_MAIN(TstAudioStreamSupervisor)
#include "tst_audio_stream_supervisor.moc"
