// =================================================================
// tests/tst_device_parser_ulanzi.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - Decoder cases ported from AetherSDR
//                 tests/ulanzi_chord_decoder_test.cpp [@1e0718ad] for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code (Claude Opus 5.5). Hot keys plan Task
//                 16. Over control names, with cases for the releases upstream
//                 can lose, the three backends on simulated devices and the gate.
// =================================================================
//
// Simulated key streams and simulated devices (R-HK-15). No case opens a device or
// creates an IOHIDManager. A pass shows the code matches its sources, not what a dial
// does.
//
// Ulanzi Dial chord decoder — press/release balance under adversarial event
// ordering.
//
// The dial's buttons can be bound to momentary transmit actions (PTT hold, CW
// straight key and paddles), which key on press and un-key on release.  A
// swallowed release therefore strands the transmitter, so the invariant these
// cases exist to pin is:
//
//     every emitted press is followed by a matching release
#include <QtTest>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileSystemWatcher>
#include <QKeyEvent>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "core/AppSettings.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "core/hotkeys/HotKeyDeviceBindings.h"
#include "device_parser_test_util.h"
#include "gui/hotkeys/ControllerHost.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/HotKeyTargets.h"
#include "gui/hotkeys/KeyboardRouter.h"
#include "gui/hotkeys/devices/DeviceInput.h"
#include "gui/hotkeys/devices/UlanziChordDecoder.h"
#include "gui/hotkeys/devices/UlanziDialInput.h"
#include "gui/hotkeys/devices/UlanziLinuxInput.h"
#include "gui/hotkeys/devices/UlanziMacInput.h"
#include "gui/hotkeys/devices/UlanziWindowsInput.h"
#include "gui/hotkeys/devices/UsbPresence.h"
#include "models/SliceModel.h"

using namespace NereusSDR;
using parsertest::text;

namespace {

constexpr int kRelease = 0;
constexpr int kPress = 1;
constexpr int kRepeat = 2;

constexpr int kCtrl = UlanziKey::LeftCtrl;
constexpr int kY = UlanziKey::Y;
constexpr int kZ = UlanziKey::Z;
constexpr int kPrev = UlanziKey::PreviousSong;
constexpr int kMute = UlanziKey::Mute;

const QString kNotPlugged = QStringLiteral("Not plugged in");

struct Recorder {
    UlanziChordDecoder decoder;
    QStringList seen;
    int tuneSteps{0};

    void take(const QList<DeviceEvent>& events)
    {
        for (const DeviceEvent& event : events) {
            if (event.kind == DeviceEvent::Kind::Rotate) {
                tuneSteps += event.value;
            } else {
                seen.append(text({event}));
            }
        }
    }

    void feedAll(const std::vector<std::pair<int, int>>& sequence)
    {
        for (const auto& [key, value] : sequence) {
            take(decoder.feed(key, value));
        }
    }

    // The safety invariant: no control is left pressed, and no release arrives for a
    // control that was not pressed.
    // [original inline comment from tests/ulanzi_chord_decoder_test.cpp:54-55]
    bool balanced() const
    {
        QStringList held;
        for (const QString& entry : seen) {
            const QString control = entry.section(QLatin1Char(' '), 1);
            if (entry.startsWith(QLatin1String("down"))) {
                held.append(control);
            } else if (!held.removeOne(control)) {
                return false;   // release with no press
            }
        }
        return held.isEmpty();  // press with no release
    }

    int pressCount(const QString& control) const
    {
        return static_cast<int>(seen.count(QStringLiteral("down %1").arg(control)));
    }
};

// ---- Simulated devices ----

struct FakeMac {
    bool present{false};
    QString refuse;
    int checks{0};
    int opens{0};
    int alive{0};
};

class FakeMacSession : public UlanziMacSession {
public:
    explicit FakeMacSession(FakeMac* state)
        : m_state(state)
    {
        ++m_state->alive;
    }
    ~FakeMacSession() override { --m_state->alive; }

private:
    FakeMac* m_state;
};

class FakeMacLayer : public UlanziMacLayer {
public:
    explicit FakeMacLayer(FakeMac* state)
        : m_state(state)
    {
    }

    bool dialPresent() override
    {
        ++m_state->checks;
        return m_state->present;
    }

    std::unique_ptr<UlanziMacSession> open(UlanziMacSink* /*sink*/, QString* detail) override
    {
        ++m_state->opens;
        if (!m_state->refuse.isEmpty()) {
            *detail = m_state->refuse;
            return nullptr;
        }
        return std::make_unique<FakeMacSession>(m_state);
    }

private:
    FakeMac* m_state;
};

class NoSink : public UlanziMacSink {
public:
    void dialMatched(const void*) override {}
    void dialRemoved(const void*) override {}
    void dialValue(quint32, quint32, qint64) override {}
};

struct FakeEvdev {
    QString node;
    QString refuse;
    int finds{0};
    int opens{0};
    int closes{0};
    QPointer<UlanziEvdevHandle> live;
};

class FakeEvdevHandle : public UlanziEvdevHandle {
public:
    explicit FakeEvdevHandle(FakeEvdev* state)
        : m_state(state)
    {
    }
    void closeDevice() override { ++m_state->closes; }

private:
    FakeEvdev* m_state;
};

class FakeLinuxLayer : public UlanziLinuxLayer {
public:
    explicit FakeLinuxLayer(FakeEvdev* state)
        : m_state(state)
    {
    }

    QString findNode() override
    {
        ++m_state->finds;
        return m_state->node;
    }

    QString watchedDirectory() override { return {}; }

    std::unique_ptr<UlanziEvdevHandle> open(const QString& /*node*/, QString* detail) override
    {
        ++m_state->opens;
        if (!m_state->refuse.isEmpty()) {
            *detail = m_state->refuse;
            return nullptr;
        }
        auto handle = std::make_unique<FakeEvdevHandle>(m_state);
        m_state->live = handle.get();
        return handle;
    }

private:
    FakeEvdev* m_state;
};

struct FakeHid {
    QStringList paths;
    QStringList refused;
    QList<QByteArray> reports;      // the next read of the first open interface
    bool gone{false};
    int finds{0};
    int alive{0};
};

class FakeHidHandle : public HidDeviceHandle {
public:
    explicit FakeHidHandle(FakeHid* state)
        : m_state(state)
    {
        ++m_state->alive;
    }
    ~FakeHidHandle() override { --m_state->alive; }

    int read(char* buffer, int size) override
    {
        if (m_state->gone) {
            return -1;
        }
        if (m_state->reports.isEmpty()) {
            return 0;
        }
        const QByteArray report = m_state->reports.takeFirst();
        const int count = std::min(size, static_cast<int>(report.size()));
        std::copy_n(report.constData(), count, buffer);
        return count;
    }

    int write(const QByteArray&) override { return -1; }

private:
    FakeHid* m_state;
};

class FakeWindowsLayer : public UlanziWindowsLayer {
public:
    explicit FakeWindowsLayer(FakeHid* state)
        : m_state(state)
    {
    }

    QStringList findPaths() override
    {
        ++m_state->finds;
        return m_state->paths;
    }

    std::unique_ptr<HidDeviceHandle> open(const QString& path, QString* error) override
    {
        if (m_state->refused.contains(path)) {
            *error = QStringLiteral("Access is denied.");
            return nullptr;
        }
        return std::make_unique<FakeHidHandle>(m_state);
    }

private:
    FakeHid* m_state;
};

class FakeTargets : public HotKeyTargets {
public:
    SliceModel* window{nullptr};
    bool transmittingNow{false};
    qint64 clock{1000};
    QStringList sliceActions;
    QStringList transmitActions;
    QList<bool> keyHolds;

    SliceModel* windowSlice() const override { return window; }
    SliceModel* sliceByLetter(QChar) const override { return window; }
    QChar letterOf(const SliceModel*) const override { return QLatin1Char('A'); }
    QString sliceRefusal(const SliceModel*) const override { return {}; }
    bool listened(const SliceModel*) const override { return false; }
    bool tuneTo(SliceModel* slice, double hz) override
    {
        slice->setFrequency(hz);
        return true;
    }
    void runSliceAction(const QString& id, SliceModel*, int) override { sliceActions.append(id); }
    void runWindowAction(const QString&) override {}
    QString unavailableReason(const QString&) const override { return {}; }
    bool transmitting() const override { return transmittingNow; }
    bool tuneOrTwoToneOn() const override { return false; }
    bool keyHold(bool down) override
    {
        keyHolds.append(down);
        transmittingNow = down;
        return true;
    }
    void runTransmitAction(const QString& id) override { transmitActions.append(id); }
    void runCwMessage(const QString&) override {}
    void setCwKeyDown(const QString&, bool) override {}
    void toast(const QString&) override {}
    qint64 nowMs() const override { return clock; }
};

QByteArray sourceText(const char* relative)
{
    QFile file(QStringLiteral(NEREUS_SOURCE_DIR "/") + QLatin1String(relative));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QStringList eventsOf(const QSignalSpy& spy)
{
    QStringList out;
    for (const QList<QVariant>& args : spy) {
        out += text({args.at(1).value<DeviceEvent>()});
    }
    return out;
}

QByteArray keyboardReport(quint8 modifiers, quint8 key = 0)
{
    QByteArray report(8, '\0');
    report[0] = static_cast<char>(modifiers);
    report[2] = static_cast<char>(key);
    return report;
}

} // namespace

class TestDeviceParserUlanzi : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        UlanziMacInput::setLayerFactoryForTest(nullptr);
        UlanziLinuxInput::setLayerFactoryForTest(nullptr);
        UlanziLinuxInput::setSchedulerForTest(nullptr);
        UlanziWindowsInput::setLayerFactoryForTest(nullptr);
    }

    void cleanup() { init(); }

    // ---- The brief's cases ----

    void volumeKeysTune()
    {
        UlanziChordDecoder decoder;
        const QStringList plus{QStringLiteral("turn wheel 1")};
        QCOMPARE(text(decoder.feed(115, kPress)), plus);
        QCOMPARE(text(decoder.feed(115, kRepeat)), plus);
        QVERIFY(decoder.feed(115, kRelease).isEmpty());
        const QStringList minus{QStringLiteral("turn wheel -1")};
        QCOMPARE(text(decoder.feed(114, kPress)), minus);
        QCOMPARE(text(decoder.feed(114, kRepeat)), minus);
    }

    void previousSongIsTopLeft()
    {
        UlanziChordDecoder decoder;
        QCOMPARE(text(decoder.feed(165, kPress)), QStringList{QStringLiteral("down topLeft")});
        QCOMPARE(text(decoder.feed(165, kRelease)), QStringList{QStringLiteral("up topLeft")});
    }

    void ctrlYIsRightTop()
    {
        UlanziChordDecoder decoder;
        QVERIFY(decoder.feed(29, kPress).isEmpty());
        QCOMPARE(text(decoder.feed(21, kPress)), QStringList{QStringLiteral("down rightTop")});
        QCOMPARE(text(decoder.feed(21, kRelease)), QStringList{QStringLiteral("up rightTop")});
        QVERIFY(decoder.feed(29, kRelease).isEmpty());
    }

    void aDroppedCtrlUpThenResetReleasesWhatIsDown()
    {
        UlanziChordDecoder decoder;
        decoder.feed(29, kPress);
        decoder.feed(21, kPress);
        decoder.feed(21, kRelease);
        QCOMPARE(text(decoder.feed(21, kPress)), QStringList{QStringLiteral("down rightTop")});
        QCOMPARE(text(decoder.reset()), QStringList{QStringLiteral("up rightTop")});
        QVERIFY(decoder.reset().isEmpty());
        // Ctrl's state went too, so 21 is a bare key again.
        QVERIFY(decoder.feed(21, kPress).isEmpty());
    }

    void everyControlHasItsKey_data()
    {
        QTest::addColumn<int>("key");
        QTest::addColumn<QString>("control");
        QTest::addColumn<bool>("chord");
        QTest::newRow("previous") << 165 << "topLeft" << false;
        QTest::newRow("play") << 164 << "topMiddle" << false;
        QTest::newRow("next") << 163 << "topRight" << false;
        QTest::newRow("mute") << 113 << "press" << false;
        QTest::newRow("v") << 47 << "leftTop" << true;
        QTest::newRow("c") << 46 << "leftBottom" << true;
        QTest::newRow("y") << 21 << "rightTop" << true;
        QTest::newRow("z") << 44 << "rightBottom" << true;
    }

    void everyControlHasItsKey()
    {
        QFETCH(int, key);
        QFETCH(QString, control);
        QFETCH(bool, chord);
        for (const int ctrl : {29, 97}) {
            UlanziChordDecoder decoder;
            if (chord) {
                decoder.feed(ctrl, kPress);
            }
            QCOMPARE(text(decoder.feed(key, kPress)), QStringList{QStringLiteral("down %1").arg(control)});
            QCOMPARE(text(decoder.feed(key, kRelease)), QStringList{QStringLiteral("up %1").arg(control)});
        }
    }

    void hidUsagesGiveLinuxCodes_data()
    {
        QTest::addColumn<int>("page");
        QTest::addColumn<int>("usage");
        QTest::addColumn<int>("key");
        QTest::newRow("volume up") << 0x0C << 0xE9 << 115;
        QTest::newRow("volume down") << 0x0C << 0xEA << 114;
        QTest::newRow("previous") << 0x0C << 0xB6 << 165;
        QTest::newRow("play") << 0x0C << 0xCD << 164;
        QTest::newRow("next") << 0x0C << 0xB5 << 163;
        QTest::newRow("mute") << 0x0C << 0xE2 << 113;
        QTest::newRow("left ctrl") << 0x07 << 0xE0 << 29;
        QTest::newRow("right ctrl") << 0x07 << 0xE4 << 97;
        QTest::newRow("v") << 0x07 << 0x19 << 47;
        QTest::newRow("c") << 0x07 << 0x06 << 46;
        QTest::newRow("y") << 0x07 << 0x1C << 21;
        QTest::newRow("z") << 0x07 << 0x1D << 44;
    }

    void hidUsagesGiveLinuxCodes()
    {
        QFETCH(int, page);
        QFETCH(int, usage);
        QFETCH(int, key);
        const std::optional<int> got =
            UlanziChordDecoder::keyCodeFromHidUsage(static_cast<quint8>(page), static_cast<quint16>(usage));
        QVERIFY(got.has_value());
        QCOMPARE(*got, key);
    }

    void otherUsagesGiveNothing()
    {
        QVERIFY(!UlanziChordDecoder::keyCodeFromHidUsage(0x0C, 0x1C).has_value());  // Y's number, wrong page
        QVERIFY(!UlanziChordDecoder::keyCodeFromHidUsage(0x07, 0xE9).has_value());
        QVERIFY(!UlanziChordDecoder::keyCodeFromHidUsage(0x01, 0xE9).has_value());
        QVERIFY(!UlanziChordDecoder::keyCodeFromHidUsage(0x07, 0x04).has_value());
    }

    void defaultBindings()
    {
        AppSettings::instance().clear();
        const HotKeyDeviceBindings bindings;
        const auto pressOf = [&bindings](const char* control) {
            return bindings.binding(DeviceId::UlanziDial, QString::fromLatin1(control), ControlGesture::Press).actionId;
        };
        QCOMPARE(pressOf("topLeft"), QStringLiteral("tx.mox"));
        QCOMPARE(pressOf("topMiddle"), QStringLiteral("rit.toggle"));
        QCOMPARE(pressOf("topRight"), QStringLiteral("tx.tune"));
        QCOMPARE(pressOf("rightTop"), QStringLiteral("slice.next"));
        QCOMPARE(pressOf("press"), QStringLiteral("mute"));
        const HotKeyDeviceBinding wheel =
            bindings.binding(DeviceId::UlanziDial, QStringLiteral("wheel"), ControlGesture::Turn);
        QCOMPARE(wheel.actionId, QStringLiteral("tune.up"));
        QCOMPARE(wheel.actionIdDown, QStringLiteral("tune.down"));
    }

    void volumeKeystrokesHaveNoChord_data()
    {
        QTest::addColumn<int>("key");
        QTest::newRow("up") << int(Qt::Key_VolumeUp);
        QTest::newRow("down") << int(Qt::Key_VolumeDown);
    }

    void volumeKeystrokesHaveNoChord()
    {
        // The window's half is in tst_hot_key_router. A key with no chord can't be bound.
        QFETCH(int, key);
        const QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
        QVERIFY(KeyboardRouter::chordFromEvent(&event).isEmpty());
    }

    // ---- Upstream's cases, from tests/ulanzi_chord_decoder_test.cpp ----

    void bareKeyEmitsPressAndRelease()
    {
        Recorder r;
        r.feedAll({{kMute, kPress}, {kMute, kRelease}});
        QCOMPARE(r.seen, (QStringList{QStringLiteral("down press"), QStringLiteral("up press")}));
        QVERIFY(r.balanced());
    }

    void rotaryCountsAutorepeatAndEmitsNoButtons()
    {
        Recorder r;
        r.feedAll({{UlanziKey::VolumeUp, kPress}, {UlanziKey::VolumeUp, kRepeat}, {UlanziKey::VolumeDown, kPress}});
        QCOMPARE(r.tuneSteps, 1);
        QVERIFY(r.seen.isEmpty());
    }

    void chordReleasedBeforeCtrl()
    {
        Recorder r;
        r.feedAll({{kCtrl, kPress}, {kY, kPress}, {kY, kRelease}, {kCtrl, kRelease}});
        QCOMPARE(r.seen, (QStringList{QStringLiteral("down rightTop"), QStringLiteral("up rightTop")}));
        QVERIFY(r.balanced());
    }

    // ---- single-key chord, Ctrl released first (the #4611 symptom) -------
    void ctrlReleasedFirstStillEmitsTheChordRelease()
    {
        Recorder r;
        r.feedAll({{kCtrl, kPress}, {kY, kPress}, {kCtrl, kRelease}, {kY, kRelease}});
        QCOMPARE(r.seen, (QStringList{QStringLiteral("down rightTop"), QStringLiteral("up rightTop")}));
        QVERIFY(r.balanced());
    }

    // ---- two chord keys held together ------------------------------------
    // Both physical orders: the operator can press the two adjacent side
    // buttons at once, and either can come up first.
    void twoChordKeysSecondReleasedFirst()
    {
        Recorder r;
        r.feedAll({{kCtrl, kPress}, {kY, kPress}, {kZ, kPress}, {kZ, kRelease}, {kY, kRelease}, {kCtrl, kRelease}});
        QCOMPARE(r.pressCount(QStringLiteral("rightTop")), 1);
        QCOMPARE(r.pressCount(QStringLiteral("rightBottom")), 1);
        QVERIFY(r.balanced());
    }

    void twoChordKeysFirstReleasedFirst()
    {
        Recorder r;
        r.feedAll({{kCtrl, kPress}, {kY, kPress}, {kZ, kPress}, {kY, kRelease}, {kZ, kRelease}, {kCtrl, kRelease}});
        QVERIFY(r.balanced());
    }

    // ---- Ctrl bounce while a chord key stays held -------------------------
    // Ctrl up (synthesises the chord release), Ctrl down again, then the key
    // up.  The suppression latch must not survive into the next bare press.
    void ctrlBounceMidChord()
    {
        Recorder r;
        r.feedAll({{kCtrl, kPress}, {kY, kPress}, {kCtrl, kRelease}, {kCtrl, kPress}, {kY, kRelease}, {kCtrl, kRelease}});
        QVERIFY(r.balanced());
    }

    void aLaterBarePressKeepsItsReleaseAfterACtrlBounce()
    {
        // Upstream runs this on Y, whose bare press has no control here. Mute has both
        // a bare control and a place in a chord window, so it shows the latch is gone.
        Recorder r;
        r.feedAll({{kCtrl, kPress}, {kMute, kPress}, {kCtrl, kRelease}, {kCtrl, kPress}, {kMute, kRelease}, {kCtrl, kRelease}});
        QVERIFY(r.seen.isEmpty());
        r.feedAll({{kMute, kPress}, {kMute, kRelease}});
        QCOMPARE(r.seen, (QStringList{QStringLiteral("down press"), QStringLiteral("up press")}));
    }

    // ---- trailing bare release after a Ctrl-first chord is swallowed -----
    void theKeyUpAfterASynthesisedReleaseIsSwallowed()
    {
        Recorder r;
        r.feedAll({{kCtrl, kPress}, {kY, kPress}, {kCtrl, kRelease}});
        r.seen.clear();
        r.feedAll({{kY, kRepeat}, {kY, kRelease}});
        QVERIFY(r.seen.isEmpty());
    }

    // ---- Mode Cycle compound (Ctrl+Y+KEY_PREVIOUSSONG) -------------------
    void theCompoundHasNoControl()
    {
        Recorder r;
        r.feedAll({{kCtrl, kPress}, {kPrev, kPress}, {kY, kPress}, {kY, kRelease}, {kPrev, kRelease}, {kCtrl, kRelease}});
        QVERIFY(r.seen.isEmpty());
    }

    void previousSongAloneIsAPlainButton()
    {
        Recorder r;
        r.feedAll({{kPrev, kPress}, {kPrev, kRelease}});
        QCOMPARE(r.seen, (QStringList{QStringLiteral("down topLeft"), QStringLiteral("up topLeft")}));
    }

    // ---- reset() drops a key held across a disconnect --------------------
    // A key still down when the dial unplugs leaves no chord state behind, so
    // the next session starts clean.  The stale key-up that may arrive first
    // surfaces as a bare release with no press — the harmless direction (every
    // momentary consumer ignores a release it never pressed), and the opposite
    // of the stuck-TX failure the balance invariant guards against.
    void afterResetAStaleKeyUpIsHarmless()
    {
        Recorder r;
        r.feedAll({{kMute, kPress}});
        r.take(r.decoder.reset());
        QVERIFY(r.balanced());
        r.seen.clear();
        r.feedAll({{kMute, kRelease}});
        QCOMPARE(r.seen, QStringList{QStringLiteral("up press")});
        r.seen.clear();
        r.feedAll({{kMute, kPress}, {kMute, kRelease}});
        QCOMPARE(r.seen, (QStringList{QStringLiteral("down press"), QStringLiteral("up press")}));
    }

    // ---- Where this differs from upstream, which loses each of these releases ----

    void aChordKeyLetsGoOfWhatItPressed_data()
    {
        QTest::addColumn<bool>("ctrlFirst");
        QTest::newRow("key lifts first") << false;
        QTest::newRow("ctrl lifts first") << true;
    }

    void aChordKeyLetsGoOfWhatItPressed()
    {
        // Upstream's release names the compound, so rightTop never lets go.
        QFETCH(bool, ctrlFirst);
        Recorder r;
        r.feedAll({{kCtrl, kPress}, {kY, kPress}, {kPrev, kPress}});
        if (ctrlFirst) {
            r.feedAll({{kCtrl, kRelease}, {kY, kRelease}, {kPrev, kRelease}});
        } else {
            r.feedAll({{kY, kRelease}, {kPrev, kRelease}, {kCtrl, kRelease}});
        }
        QCOMPARE(r.seen, (QStringList{QStringLiteral("down rightTop"), QStringLiteral("up rightTop")}));
    }

    void aBareKeyLiftingInAChordWindowLetsGo_data()
    {
        QTest::addColumn<int>("key");
        QTest::addColumn<QString>("control");
        QTest::newRow("media key") << int(UlanziKey::Mute) << "press";
        QTest::newRow("previous song") << int(UlanziKey::PreviousSong) << "topLeft";
    }

    void aBareKeyLiftingInAChordWindowLetsGo()
    {
        QFETCH(int, key);
        QFETCH(QString, control);
        Recorder r;
        r.feedAll({{key, kPress}, {kCtrl, kPress}, {key, kRelease}, {kCtrl, kRelease}});
        QCOMPARE(r.seen, (QStringList{QStringLiteral("down %1").arg(control), QStringLiteral("up %1").arg(control)}));
    }

    void aFreshPressDisarmsTheLatch_data()
    {
        QTest::addColumn<int>("key");
        QTest::addColumn<QString>("control");
        QTest::newRow("media key") << int(UlanziKey::Mute) << "press";
        QTest::newRow("previous song") << int(UlanziKey::PreviousSong) << "topLeft";
    }

    void aFreshPressDisarmsTheLatch()
    {
        // The key-up after a chord was lost. The next press must keep its release.
        QFETCH(int, key);
        QFETCH(QString, control);
        Recorder r;
        r.feedAll({{kCtrl, kPress}, {key, kPress}, {kCtrl, kRelease}});
        r.seen.clear();
        r.feedAll({{key, kPress}, {key, kRelease}});
        QCOMPARE(r.seen, (QStringList{QStringLiteral("down %1").arg(control), QStringLiteral("up %1").arg(control)}));
    }

    void aSecondPressOfAHeldKeyRunsNothing()
    {
        Recorder r;
        r.feedAll({{kMute, kPress}, {kMute, kPress}, {kCtrl, kPress}, {kY, kPress}, {kY, kPress}});
        QCOMPARE(r.pressCount(QStringLiteral("press")), 1);
        QCOMPARE(r.pressCount(QStringLiteral("rightTop")), 1);
    }

    void anyOrderLeavesNothingDown()
    {
        // Each physical key goes down and comes up once, in any order, with repeats.
        const QList<int> keys{kCtrl, kPrev, kY, kZ, kMute, UlanziKey::PlayPause, UlanziKey::RightCtrl};
        QRandomGenerator random(0x756C616E);
        for (int round = 0; round < 3000; ++round) {
            Recorder r;
            QList<int> pending = keys;
            QList<int> down;
            while (!pending.isEmpty() || !down.isEmpty()) {
                const int choice = random.bounded(3);
                if (choice == 0 && !pending.isEmpty()) {
                    const int key = pending.takeAt(random.bounded(static_cast<int>(pending.size())));
                    down.append(key);
                    r.feedAll({{key, kPress}});
                } else if (choice == 1 && !down.isEmpty()) {
                    r.feedAll({{down.at(random.bounded(static_cast<int>(down.size()))), kRepeat}});
                } else if (!down.isEmpty()) {
                    r.feedAll({{down.takeAt(random.bounded(static_cast<int>(down.size()))), kRelease}});
                }
            }
            // An extra release is the harmless direction. What may not happen is a press
            // left down.
            QStringList stillHeld;
            for (const QString& entry : std::as_const(r.seen)) {
                const QString control = entry.section(QLatin1Char(' '), 1);
                if (entry.startsWith(QLatin1String("down"))) {
                    stillHeld.append(control);
                } else {
                    stillHeld.removeOne(control);
                }
            }
            QVERIFY2(stillHeld.isEmpty(), qPrintable(r.seen.join(QLatin1String(", "))));
            QVERIFY(r.decoder.reset().isEmpty());
        }
    }

    // ---- macOS: the prompt ----

    void macOpensNothingWithNoDial()
    {
        FakeMac mac;
        UlanziMacInput::setLayerFactoryForTest([&mac] { return std::make_unique<FakeMacLayer>(&mac); });
        UlanziMacInput input;
        QSignalSpy states(&input, &UlanziMacInput::deviceStateChanged);
        input.start();
        for (int i = 0; i < 50; ++i) {
            input.checkPresence();
        }
        QCOMPARE(mac.checks, 51);
        QCOMPARE(mac.opens, 0);
        QCOMPARE(mac.alive, 0);
        QCOMPARE(states.size(), 1);
        QCOMPARE(states.at(0).at(0).value<DeviceId>(), DeviceId::UlanziDial);
        QVERIFY(!states.at(0).at(1).toBool());
        QCOMPARE(states.at(0).at(2).toString(), kNotPlugged);
    }

    void macOpensOnceTheDialIsSeenAndThenStopsLooking()
    {
        FakeMac mac;
        UlanziMacInput::setLayerFactoryForTest([&mac] { return std::make_unique<FakeMacLayer>(&mac); });
        UlanziMacInput input;
        QSignalSpy states(&input, &UlanziMacInput::deviceStateChanged);
        input.start();
        QCOMPARE(mac.opens, 0);
        mac.present = true;
        input.checkPresence();
        QCOMPARE(mac.opens, 1);
        const int checks = mac.checks;
        input.checkPresence();
        QCOMPARE(mac.opens, 1);
        QCOMPARE(mac.checks, checks);
        // Connected once the manager says the device matched, not before.
        QCOMPARE(states.size(), 1);
        const int device = 0;
        input.dialMatched(&device);
        QCOMPARE(states.size(), 2);
        QVERIFY(states.at(1).at(1).toBool());
        QCOMPARE(states.at(1).at(2).toString(), QString());
    }

    void macUnplugClosesTheManagerAndLetsGo()
    {
        FakeMac mac;
        mac.present = true;
        UlanziMacInput::setLayerFactoryForTest([&mac] { return std::make_unique<FakeMacLayer>(&mac); });
        UlanziMacInput input;
        QSignalSpy states(&input, &UlanziMacInput::deviceStateChanged);
        QSignalSpy events(&input, &UlanziMacInput::deviceEvent);
        input.start();
        const int keyboard = 0;
        const int consumer = 0;
        input.dialMatched(&keyboard);
        input.dialMatched(&consumer);
        QCOMPARE(states.size(), 2);

        input.dialValue(0x07, 0xE0, 1);
        input.dialValue(0x07, 0x1C, 1);
        QCOMPARE(eventsOf(events), QStringList{QStringLiteral("down rightTop")});

        // One interface going isn't the device going.
        input.dialRemoved(&keyboard);
        QCOMPARE(states.size(), 2);
        QCOMPARE(mac.alive, 1);

        states.clear();
        events.clear();
        input.dialRemoved(&consumer);
        QCOMPARE(states.size(), 1);
        QCOMPARE(states.at(0).at(2).toString(), kNotPlugged);
        QCOMPARE(eventsOf(events), QStringList{QStringLiteral("up rightTop")});
        // Not from inside the manager's own callback.
        QCOMPARE(mac.alive, 1);
        QCoreApplication::sendPostedEvents(&input, QEvent::MetaCall);
        QCOMPARE(mac.alive, 0);

        // Nothing held over to the next session, and it can be found again.
        mac.present = false;
        input.checkPresence();
        QCOMPARE(mac.opens, 1);
        mac.present = true;
        input.checkPresence();
        QCOMPARE(mac.opens, 2);
        input.dialMatched(&keyboard);
        events.clear();
        input.dialValue(0x07, 0x1C, 1);
        QVERIFY(events.isEmpty());
    }

    void macStopAndDestructionClose()
    {
        FakeMac mac;
        mac.present = true;
        UlanziMacInput::setLayerFactoryForTest([&mac] { return std::make_unique<FakeMacLayer>(&mac); });
        {
            UlanziMacInput input;
            QSignalSpy states(&input, &UlanziMacInput::deviceStateChanged);
            QSignalSpy events(&input, &UlanziMacInput::deviceEvent);
            input.start();
            const int device = 0;
            input.dialMatched(&device);
            input.dialValue(0x0C, 0xB6, 1);
            events.clear();
            states.clear();
            input.stop();
            QCOMPARE(mac.alive, 0);
            QCOMPARE(states.size(), 1);
            QCOMPARE(states.at(0).at(2).toString(), QString());
            QCOMPARE(eventsOf(events), QStringList{QStringLiteral("up topLeft")});
            input.start();
            QCOMPARE(mac.alive, 1);
        }
        QCOMPARE(mac.alive, 0);
    }

    void macRefusedOpenSaysWhyAndWaitsToTryAgain()
    {
        FakeMac mac;
        mac.present = true;
        mac.refuse = QStringLiteral("(iokit/common) not permitted");
        UlanziMacInput::setLayerFactoryForTest([&mac] { return std::make_unique<FakeMacLayer>(&mac); });
        UlanziMacInput input;
        QSignalSpy states(&input, &UlanziMacInput::deviceStateChanged);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Couldn't open the Ulanzi Dial")));
        input.start();
        QCOMPARE(mac.opens, 1);
        QCOMPARE(mac.alive, 0);
        QCOMPARE(states.last().at(2).toString(), deviceOpenFailureReason(mac.refuse));
        QVERIFY(!states.last().at(1).toBool());

        for (int i = 0; i < UlanziMacInput::kChecksBetweenTries; ++i) {
            input.checkPresence();
        }
        QCOMPARE(mac.opens, 1);
        // Same reason again: no second warning, no second state change.
        const qsizetype count = states.size();
        input.checkPresence();
        QCOMPARE(mac.opens, 2);
        QCOMPARE(states.size(), count);

        // Pulling it clears the wait, so plugging it back in tries at once.
        mac.present = false;
        mac.refuse.clear();
        input.checkPresence();
        QCOMPARE(states.last().at(2).toString(), kNotPlugged);
        mac.present = true;
        input.checkPresence();
        QCOMPARE(mac.opens, 3);
        QCOMPARE(mac.alive, 1);
    }

    void macValuesReachTheDecoderAndStrangeOnesDont()
    {
        FakeMac mac;
        UlanziMacInput::setLayerFactoryForTest([&mac] { return std::make_unique<FakeMacLayer>(&mac); });
        UlanziMacInput input;
        QSignalSpy events(&input, &UlanziMacInput::deviceEvent);
        input.dialValue(0x0C, 0xE9, 1);
        input.dialValue(0x0C, 0xE9, 0);
        input.dialValue(0x0C, 0xEA, 1);
        input.dialValue(0x0C, 0xE2, 1);
        input.dialValue(0x0C, 0xE2, 0);
        QCOMPARE(eventsOf(events), (QStringList{QStringLiteral("turn wheel 1"), QStringLiteral("turn wheel -1"),
                                                QStringLiteral("down press"), QStringLiteral("up press")}));
        events.clear();
        input.dialValue(0x0C, 0xFFFFFFFFu, 1);
        input.dialValue(0x10C, 0xE9, 1);
        input.dialValue(0x0C, 0x100E9, 1);
        QVERIFY(events.isEmpty());
    }

    // ---- The switch only the application turns on ----

    void aTestProcessHasTheSwitchOff()
    {
        QVERIFY(!DeviceInput::realDevicesAllowed());
    }

    void withTheSwitchOffTheRealManagerIsNeverReached()
    {
        // The last lock before IOHIDManagerCreate. With it gone this case would create
        // and open one, so it can fail here only by doing the thing it guards.
        QVERIFY(!DeviceInput::realDevicesAllowed());
        NoSink sink;
        QString detail;
        QVERIFY(UlanziMacInput::openRealSession(&sink, &detail) == nullptr);
        QVERIFY(detail.isEmpty());
    }

    void eachBackendStaysQuietWithTheSwitchOff()
    {
        UlanziMacInput mac;
        UlanziLinuxInput linux_;
        UlanziWindowsInput windows;
        for (UlanziDialInput* const input : std::initializer_list<UlanziDialInput*>{&mac, &linux_, &windows}) {
            QSignalSpy states(input, &UlanziDialInput::deviceStateChanged);
            input->start();
            QCOMPARE(states.size(), 1);
            QVERIFY(!states.at(0).at(1).toBool());
            QCOMPARE(states.at(0).at(2).toString(), kNotPlugged);
            input->stop();
            QCOMPARE(states.size(), 1);
        }
        // What the seams are checked for below: nothing to read, and no watcher.
        QVERIFY(linux_.findChildren<QFileSystemWatcher*>().isEmpty());
    }

    void theRealDeviceCallsAreBehindTheSwitch_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<QString>("guardLine");
        QTest::addColumn<QString>("realLayer");
        const QString guardLine = QStringLiteral("if (!DeviceInput::realDevicesAllowed())");
        QTest::newRow("mac") << "src/gui/hotkeys/devices/UlanziMacInput.cpp" << guardLine
                             << "std::make_unique<RealMacLayer>()";
        QTest::newRow("linux") << "src/gui/hotkeys/devices/UlanziLinuxInput.cpp" << guardLine
                               << "std::make_unique<RealLinuxLayer>()";
        QTest::newRow("windows") << "src/gui/hotkeys/devices/UlanziWindowsInput.cpp" << guardLine
                                 << "std::make_unique<RealWindowsLayer>()";
    }

    void theRealDeviceCallsAreBehindTheSwitch()
    {
        // The other half of the cases above, which can't see a call not being made. Text,
        // not behavior.
        QFETCH(QString, file);
        QFETCH(QString, guardLine);
        QFETCH(QString, realLayer);
        const QByteArray text = sourceText(file.toLatin1().constData());
        QVERIFY(!text.isEmpty());
        const qsizetype real = text.indexOf(realLayer.toLatin1());
        QVERIFY(real >= 0);
        QCOMPARE(text.count(realLayer.toLatin1()), 1);
        const qsizetype guard = text.lastIndexOf(guardLine.toLatin1(), real);
        QVERIFY(guard >= 0);
        QVERIFY(real - guard < 200);
    }

    void onlyTheManagerOpensAndAfterItsMatchIsSet()
    {
        const QByteArray text = sourceText("src/gui/hotkeys/devices/UlanziMacInput.cpp");
        QCOMPARE(text.count("IOHIDManagerOpen("), 1);
        QCOMPARE(text.count("IOHIDManagerCreate("), 1);
        const qsizetype create = text.indexOf("IOHIDManagerCreate(");
        const qsizetype match = text.indexOf("IOHIDManagerSetDeviceMatching(manager, match)");
        const qsizetype open = text.indexOf("IOHIDManagerOpen(");
        QVERIFY(create >= 0 && match > create && open > match);
        QVERIFY(text.contains("kIOHIDOptionsTypeSeizeDevice"));
        // The manager is reached only from openRealSession, past its own guard.
        const qsizetype guard = text.indexOf("UlanziMacInput::openRealSession");
        const qsizetype lock = text.indexOf("if (!DeviceInput::realDevicesAllowed())", guard);
        const qsizetype call = text.indexOf("return openManager(sink, detail)");
        QVERIFY(guard >= 0 && lock > guard && call > lock);
        QCOMPARE(text.count("openManager("), 2);
        // UsbPresence is what looks first, and it never touches the manager.
        const QByteArray presence = sourceText("src/gui/hotkeys/devices/UsbPresence.cpp");
        QVERIFY(!presence.isEmpty());
        QVERIFY(!presence.contains("IOHIDManager"));
        QVERIFY(!presence.contains("IOHIDDeviceOpen"));
    }

    void onlyMainAllowsRealDevices()
    {
        const QByteArray text = sourceText("src/main.cpp");
        QCOMPARE(text.count("DeviceInput::allowRealDevices()"), 1);
    }

    void readingProductNamesTouchesNothing()
    {
        // Runs the real registry read, the only real device-layer call any case here
        // makes. It says nothing about which devices are there. Nothing on CI need be.
        QElapsedTimer timer;
        timer.start();
        const QStringList names = UsbPresence::hidProductNames();
        qInfo() << names.size() << "HID interfaces read in" << timer.nsecsElapsed() / 1000 << "microseconds";
#if !defined(Q_OS_MAC)
        QVERIFY(names.isEmpty());
#endif
    }

    // ---- Linux ----

    void linuxFindsTheNodeByNameAlone()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto addNode = [&dir](const QString& node, const QByteArray& name) {
            QVERIFY(QDir(dir.path()).mkpath(node + QStringLiteral("/device")));
            QFile file(dir.filePath(node + QStringLiteral("/device/name")));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(name);
        };
        QCOMPARE(UlanziLinuxInput::findEventNode(dir.path()), QString());
        addNode(QStringLiteral("event1"), "AT Translated Set 2 keyboard\n");
        addNode(QStringLiteral("event2"), "Ulanzi Dial Keyboard Extra\n");
        addNode(QStringLiteral("mouse0"), "Ulanzi Dial Keyboard\n");
        QCOMPARE(UlanziLinuxInput::findEventNode(dir.path()), QString());
        addNode(QStringLiteral("event7"), "Ulanzi Dial Keyboard\n");
        QCOMPARE(UlanziLinuxInput::findEventNode(dir.path()), QStringLiteral("event7"));
    }

    void linuxOpensTheNodeReadsKeysAndClosesOnUnplug()
    {
        FakeEvdev evdev;
        QList<int> delays;
        UlanziLinuxInput::setLayerFactoryForTest([&evdev] { return std::make_unique<FakeLinuxLayer>(&evdev); });
        UlanziLinuxInput::setSchedulerForTest([&delays](int ms) { delays.append(ms); });
        UlanziLinuxInput input;
        QSignalSpy states(&input, &UlanziLinuxInput::deviceStateChanged);
        QSignalSpy events(&input, &UlanziLinuxInput::deviceEvent);
        input.start();
        QCOMPARE(evdev.opens, 0);
        QCOMPARE(states.last().at(2).toString(), kNotPlugged);

        // A burst of changes waits for the debounce, and only then is looked at.
        evdev.node = QStringLiteral("event7");
        const int finds = evdev.finds;
        input.onDirectoryChanged();
        input.onDirectoryChanged();
        QCOMPARE(delays, (QList<int>{250, 250}));
        QCOMPARE(evdev.finds, finds);
        input.checkNow();
        QCOMPARE(evdev.opens, 1);
        QVERIFY(states.last().at(1).toBool());

        // Only key events count. Type 0 is a sync, 2 a relative move.
        constexpr quint16 kEvKey = 1;
        emit evdev.live->inputEvent(0, 115, 1);
        emit evdev.live->inputEvent(2, 115, 1);
        QVERIFY(events.isEmpty());
        emit evdev.live->inputEvent(kEvKey, 115, 1);
        emit evdev.live->inputEvent(kEvKey, 115, 2);
        emit evdev.live->inputEvent(kEvKey, 165, 1);
        QCOMPARE(eventsOf(events), (QStringList{QStringLiteral("turn wheel 1"), QStringLiteral("turn wheel 1"),
                                                QStringLiteral("down topLeft")}));

        events.clear();
        states.clear();
        delays.clear();
        evdev.node.clear();
        emit evdev.live->wentAway();
        QCOMPARE(evdev.closes, 1);
        QCOMPARE(states.size(), 1);
        QCOMPARE(states.at(0).at(2).toString(), kNotPlugged);
        QCOMPARE(eventsOf(events), QStringList{QStringLiteral("up topLeft")});
        QCOMPARE(delays, QList<int>{250});

        evdev.node = QStringLiteral("event7");
        input.checkNow();
        QCOMPARE(evdev.opens, 2);
        input.stop();
        QCOMPARE(evdev.closes, 2);
    }

    void linuxAccessRefusalIsReported()
    {
        FakeEvdev evdev;
        evdev.node = QStringLiteral("event7");
        evdev.refuse = QStringLiteral("Permission denied");
        UlanziLinuxInput::setLayerFactoryForTest([&evdev] { return std::make_unique<FakeLinuxLayer>(&evdev); });
        UlanziLinuxInput input;
        QSignalSpy states(&input, &UlanziLinuxInput::deviceStateChanged);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Couldn't open the Ulanzi Dial")));
        input.start();
        const QString reason = states.last().at(2).toString();
        QCOMPARE(reason, deviceOpenFailureReason(evdev.refuse));
        QVERIFY(!states.last().at(1).toBool());
#if defined(Q_OS_LINUX)
        QVERIFY(reason.contains(QLatin1String("60-nereussdr-controllers.rules")));
#endif
        // Surfaced once.
        const qsizetype count = states.size();
        input.checkNow();
        QCOMPARE(states.size(), count);
    }

    void linuxDestructionClosesTheHandle()
    {
        FakeEvdev evdev;
        evdev.node = QStringLiteral("event7");
        UlanziLinuxInput::setLayerFactoryForTest([&evdev] { return std::make_unique<FakeLinuxLayer>(&evdev); });
        {
            UlanziLinuxInput input;
            input.start();
            QCOMPARE(evdev.closes, 0);
        }
        QCOMPARE(evdev.closes, 1);
    }

    // ---- Windows ----

    void windowsConsumerReportsPressAndRelease()
    {
        const QByteArray report("\xE9\x00\x00", 3);
        const QList<QPair<int, int>> keys = UlanziWindowsInput::decodeReport({}, report);
        QCOMPARE(keys, (QList<QPair<int, int>>{{115, 1}, {115, 0}}));
        // All-zero is the key coming up, and means nothing.
        QVERIFY(UlanziWindowsInput::decodeReport(report, QByteArray("\x00\x00\x00", 3)).isEmpty());
        QVERIFY(UlanziWindowsInput::decodeReport({}, QByteArray("\x01", 1)).isEmpty());
    }

    void windowsKeyboardReportsDiff()
    {
        constexpr quint8 kLeftCtrlBit = 0x01;
        constexpr quint8 kRightCtrlBit = 0x10;
        constexpr quint8 kYUsage = 0x1C;
        const QByteArray none = keyboardReport(0);
        const QByteArray ctrl = keyboardReport(kLeftCtrlBit);
        const QByteArray ctrlY = keyboardReport(kLeftCtrlBit, kYUsage);
        using Keys = QList<QPair<int, int>>;
        QCOMPARE(UlanziWindowsInput::decodeReport(none, ctrl), (Keys{{29, 1}}));
        QCOMPARE(UlanziWindowsInput::decodeReport(ctrl, ctrlY), (Keys{{21, 1}}));
        QCOMPARE(UlanziWindowsInput::decodeReport(ctrlY, ctrlY), Keys{});
        QCOMPARE(UlanziWindowsInput::decodeReport(ctrlY, ctrl), (Keys{{21, 0}}));
        QCOMPARE(UlanziWindowsInput::decodeReport(ctrl, none), (Keys{{29, 0}}));
        QCOMPARE(UlanziWindowsInput::decodeReport({}, keyboardReport(kRightCtrlBit)), (Keys{{29, 1}}));
        QCOMPARE(UlanziWindowsInput::decodeReport({}, keyboardReport(0, 0x04)), Keys{});
    }

    void windowsOpensEveryInterfaceItCanAndReadsThem()
    {
        FakeHid hid;
        UlanziWindowsInput::setLayerFactoryForTest([&hid] { return std::make_unique<FakeWindowsLayer>(&hid); });
        UlanziWindowsInput input;
        QSignalSpy states(&input, &UlanziWindowsInput::deviceStateChanged);
        QSignalSpy events(&input, &UlanziWindowsInput::deviceEvent);
        input.start();
        QCOMPARE(hid.alive, 0);
        QCOMPARE(states.last().at(2).toString(), kNotPlugged);

        hid.paths = QStringList{QStringLiteral("keyboard"), QStringLiteral("consumer"), QStringLiteral("mouse")};
        hid.refused = QStringList{QStringLiteral("keyboard")};
        input.hotplugCheck();
        QCOMPARE(hid.alive, 2);
        QVERIFY(states.last().at(1).toBool());

        const int finds = hid.finds;
        input.hotplugCheck();
        QCOMPARE(hid.finds, finds);

        hid.reports = {QByteArray("\xE9\x00\x00", 3), QByteArray("\xB6\x00\x00", 3)};
        input.poll();
        QCOMPARE(eventsOf(events), (QStringList{QStringLiteral("turn wheel 1"), QStringLiteral("down topLeft"),
                                                QStringLiteral("up topLeft")}));

        states.clear();
        hid.gone = true;
        input.poll();
        QCOMPARE(hid.alive, 0);
        QCOMPARE(states.size(), 1);
        QCOMPARE(states.at(0).at(2).toString(), kNotPlugged);
    }

    // A keyboard report says what is down now, so the first one after an open may be
    // nobody pressing anything. Connecting never keys.
    void windowsFirstKeyboardReportStartsNoPress()
    {
        constexpr quint8 kLeftCtrlBit = 0x01;
        constexpr quint8 kYUsage = 0x1C;
        FakeHid hid;
        hid.paths = QStringList{QStringLiteral("keyboard")};
        UlanziWindowsInput::setLayerFactoryForTest([&hid] { return std::make_unique<FakeWindowsLayer>(&hid); });
        UlanziWindowsInput input;
        QSignalSpy events(&input, &UlanziWindowsInput::deviceEvent);
        input.start();
        hid.reports = {keyboardReport(kLeftCtrlBit, kYUsage)};
        input.poll();
        QCOMPARE(eventsOf(events), QStringList{});

        // Seen up, it presses as usual.
        hid.reports = {keyboardReport(0), keyboardReport(kLeftCtrlBit), keyboardReport(kLeftCtrlBit, kYUsage)};
        input.poll();
        input.poll();
        input.poll();
        QCOMPARE(eventsOf(events), QStringList{QStringLiteral("down rightTop")});
    }

    void windowsReportsWhenNoInterfaceOpens()
    {
        FakeHid hid;
        hid.paths = QStringList{QStringLiteral("keyboard")};
        hid.refused = hid.paths;
        UlanziWindowsInput::setLayerFactoryForTest([&hid] { return std::make_unique<FakeWindowsLayer>(&hid); });
        UlanziWindowsInput input;
        QSignalSpy states(&input, &UlanziWindowsInput::deviceStateChanged);
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Couldn't open the Ulanzi Dial")));
        input.start();
        QCOMPARE(states.last().at(2).toString(), deviceOpenFailureReason(QStringLiteral("Access is denied.")));
    }

    void windowsStopAndDestructionClose()
    {
        FakeHid hid;
        hid.paths = QStringList{QStringLiteral("consumer")};
        UlanziWindowsInput::setLayerFactoryForTest([&hid] { return std::make_unique<FakeWindowsLayer>(&hid); });
        {
            UlanziWindowsInput input;
            input.start();
            QCOMPARE(hid.alive, 1);
            input.stop();
            QCOMPARE(hid.alive, 0);
            input.start();
            QCOMPARE(hid.alive, 1);
        }
        QCOMPARE(hid.alive, 0);
    }

    // ---- Through the host ----

    void theHostKeepsItWhereThisSystemNeedsIt()
    {
        AppSettings::instance().clear();
        HotKeyBindings bindings;
        FakeTargets targets;
        HotKeyDispatcher dispatcher(&bindings, &targets);
        ControllerHost host(&dispatcher, &bindings);
        QVERIFY(host.ulanzi() != nullptr);
        if (UlanziDialInput::staysOnMainThread()) {
            QCOMPARE(host.ulanzi()->thread(), QThread::currentThread());
            QCOMPARE(host.ulanzi()->parent(), &host);
        } else {
            QCOMPARE(host.ulanzi()->thread(), host.usb()->thread());
            QVERIFY(host.ulanzi()->thread() != QThread::currentThread());
        }
#if defined(Q_OS_MAC)
        QVERIFY(UlanziDialInput::staysOnMainThread());
#endif
        QTRY_COMPARE(host.deviceReason(DeviceId::UlanziDial), kNotPlugged);
        QVERIFY(!host.deviceConnected(DeviceId::UlanziDial));
    }

    void anUnplugDropsAHoldAndRunsNoTap()
    {
        AppSettings::instance().clear();
        FakeMac mac;
        mac.present = true;
        UlanziMacInput::setLayerFactoryForTest([&mac] { return std::make_unique<FakeMacLayer>(&mac); });
        HotKeyBindings bindings;
        FakeTargets targets;
        SliceModel slice(0);
        targets.window = &slice;
        HotKeyDispatcher dispatcher(&bindings, &targets);
        ControllerHost host(&dispatcher, &bindings);
        HotKeyDeviceBindings* const device = host.deviceBindings();
        // topLeft holds. topRight (tx.tune) also has a hold row, so its press is waiting
        // on the gesture engine when the dial goes, which an up would turn into a tap.
        device->setBinding(DeviceId::UlanziDial, QStringLiteral("topLeft"), ControlGesture::Press,
                           HotKeyDeviceBinding{QStringLiteral("tx.hold"), {}, {}});
        device->setBinding(DeviceId::UlanziDial, QStringLiteral("topRight"), ControlGesture::Hold,
                           HotKeyDeviceBinding{QStringLiteral("mute"), {}, {}});

        UlanziMacInput input;
        connect(&input, &UlanziMacInput::deviceEvent, &host, &ControllerHost::onDeviceEvent);
        connect(&input, &UlanziMacInput::deviceStateChanged, &host, &ControllerHost::onDeviceStateChanged);
        input.start();
        const int dial = 0;
        input.dialMatched(&dial);
        QVERIFY(host.deviceConnected(DeviceId::UlanziDial));

        input.dialValue(0x0C, 0xB6, 1);  // topLeft
        input.dialValue(0x0C, 0xB5, 1);  // topRight
        QCOMPARE(targets.keyHolds, QList<bool>{true});
        QVERIFY(targets.transmitActions.isEmpty());

        input.dialRemoved(&dial);
        QCOMPARE(targets.keyHolds, (QList<bool>{true, false}));
        QVERIFY(targets.transmitActions.isEmpty());
        QVERIFY(!host.deviceConnected(DeviceId::UlanziDial));
    }

    void pressesReachTheirBindings()
    {
        AppSettings::instance().clear();
        FakeMac mac;
        mac.present = true;
        UlanziMacInput::setLayerFactoryForTest([&mac] { return std::make_unique<FakeMacLayer>(&mac); });
        HotKeyBindings bindings;
        FakeTargets targets;
        SliceModel slice(0);
        targets.window = &slice;
        HotKeyDispatcher dispatcher(&bindings, &targets);
        ControllerHost host(&dispatcher, &bindings);
        UlanziMacInput input;
        connect(&input, &UlanziMacInput::deviceEvent, &host, &ControllerHost::onDeviceEvent);
        input.start();
        input.dialValue(0x0C, 0xB6, 1);
        input.dialValue(0x0C, 0xB6, 0);
        QCOMPARE(targets.transmitActions, QStringList{QStringLiteral("tx.mox")});
        input.dialValue(0x0C, 0xE2, 1);
        input.dialValue(0x0C, 0xE2, 0);
        QCOMPARE(targets.sliceActions, QStringList{QStringLiteral("mute")});
        // The compound stays unmapped: rightTop's slice.next doesn't run.
        input.dialValue(0x07, 0xE0, 1);
        input.dialValue(0x0C, 0xB6, 1);
        input.dialValue(0x07, 0x1C, 1);
        input.dialValue(0x07, 0x1C, 0);
        input.dialValue(0x0C, 0xB6, 0);
        input.dialValue(0x07, 0xE0, 0);
        QCOMPARE(targets.sliceActions, QStringList{QStringLiteral("mute")});
    }
};

QTEST_MAIN(TestDeviceParserUlanzi)
#include "tst_device_parser_ulanzi.moc"
