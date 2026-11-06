// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 10.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
//
// Relative knob decoding, messages as controls, and learn (R-HK-15). Simulated from
// documented layouts; no device has been tried.
#include <QtTest>

#include <QSignalSpy>

#include "fakes/FakeMidiBackend.h"
#include "gui/hotkeys/midi/MidiInput.h"
#include "gui/hotkeys/midi/MidiRelativeDecoder.h"

using namespace NereusSDR;

Q_DECLARE_METATYPE(NereusSDR::MidiRelativeCcEncoding)

class TestMidiDecoder : public QObject {
    Q_OBJECT

private slots:
    // ---- The relative decoder ----

    void relative_center64LocksOnItsDetents()
    {
        auto first = decodeMidiRelativeCc(63, MidiRelativeCcEncoding::Undetermined);
        QCOMPARE(first.delta, -1);
        QCOMPARE(first.encoding, MidiRelativeCcEncoding::Center64);
        auto second = decodeMidiRelativeCc(65, first.encoding);
        QCOMPARE(second.delta, 1);
        QCOMPARE(second.encoding, MidiRelativeCcEncoding::Center64);
    }

    void relative_twosComplementLocksOnItsDetents()
    {
        auto first = decodeMidiRelativeCc(1, MidiRelativeCcEncoding::Undetermined);
        QCOMPARE(first.delta, 1);
        QCOMPARE(first.encoding, MidiRelativeCcEncoding::TwosComplement);
        auto second = decodeMidiRelativeCc(127, first.encoding);
        QCOMPARE(second.delta, -1);
        QCOMPARE(second.encoding, MidiRelativeCcEncoding::TwosComplement);
    }

    void relative_ambiguousFirstValueWaits()
    {
        auto first = decodeMidiRelativeCc(40, MidiRelativeCcEncoding::Undetermined);
        QCOMPARE(first.delta, 0);
        QCOMPARE(first.encoding, MidiRelativeCcEncoding::Undetermined);
    }

    void relative_implausibleStepRelocks()
    {
        // In center-64, 1 is a delta of -63.
        auto healed = decodeMidiRelativeCc(1, MidiRelativeCcEncoding::Center64);
        QCOMPARE(healed.delta, 1);
        QCOMPARE(healed.encoding, MidiRelativeCcEncoding::TwosComplement);
        // And the other way: in two's complement, 65 is a delta of -63.
        auto back = decodeMidiRelativeCc(65, MidiRelativeCcEncoding::TwosComplement);
        QCOMPARE(back.delta, 1);
        QCOMPARE(back.encoding, MidiRelativeCcEncoding::Center64);
    }

    void relative_largestRealSpinKeepsItsLock()
    {
        // +50 in two's complement, just under the threshold's mirror: no heal.
        auto spin = decodeMidiRelativeCc(50, MidiRelativeCcEncoding::TwosComplement);
        QCOMPARE(spin.delta, 50);
        QCOMPARE(spin.encoding, MidiRelativeCcEncoding::TwosComplement);
        QCOMPARE(kMidiRelativeCcImplausibleStep, 56);
    }

    // ---- Messages ----

    void message_controlChangeIsAbsolute()
    {
        const auto control = decodeMidiMessage(MidiMessage{0xB0, 0x07, 0x40});
        QVERIFY(control.has_value());
        QCOMPARE(control->controlKey, QStringLiteral("ch1.cc7"));
        QCOMPARE(control->kind, MidiControlKind::AbsoluteKnob);
        QCOMPARE(control->value, 64);
        QVERIFY(control->isControlChange);
    }

    void message_pitchBend()
    {
        const auto control = decodeMidiMessage(MidiMessage{0xE0, 0x00, 0x40});
        QVERIFY(control.has_value());
        QCOMPARE(control->controlKey, QStringLiteral("ch1.bend"));
        QCOMPARE(control->kind, MidiControlKind::AbsoluteKnob);
        QCOMPARE(control->value, 8192);
        QCOMPARE(midiControlRange(control->controlKey), 16383);
        QVERIFY(!control->isControlChange);
        const auto top = decodeMidiMessage(MidiMessage{0xE0, 0x7F, 0x7F});
        QCOMPARE(top->value, 16383);
    }

    void message_notes()
    {
        const auto press = decodeMidiMessage(MidiMessage{0x90, 0x3C, 0x40});
        QCOMPARE(press->controlKey, QStringLiteral("ch1.note60"));
        QCOMPARE(press->kind, MidiControlKind::Button);
        QCOMPARE(press->value, 64);
        const auto zeroVelocity = decodeMidiMessage(MidiMessage{0x90, 0x3C, 0x00});
        QCOMPARE(zeroVelocity->controlKey, QStringLiteral("ch1.note60"));
        QCOMPARE(zeroVelocity->value, 0);
        const auto noteOff = decodeMidiMessage(MidiMessage{0x80, 0x3C, 0x00});
        QCOMPARE(noteOff->controlKey, QStringLiteral("ch1.note60"));
        QCOMPARE(noteOff->value, 0);
        // A note off's velocity is how it was let go, not a press.
        const auto noteOffVelocity = decodeMidiMessage(MidiMessage{0x80, 0x3C, 0x40});
        QCOMPARE(noteOffVelocity->value, 0);
    }

    void message_channelIsInTheKey()
    {
        QCOMPARE(decodeMidiMessage(MidiMessage{0x9F, 0x01, 0x40})->controlKey, QStringLiteral("ch16.note1"));
    }

    void message_othersAreIgnored_data()
    {
        QTest::addColumn<int>("status");
        QTest::newRow("poly pressure") << 0xA0;
        QTest::newRow("program") << 0xC0;
        QTest::newRow("channel pressure") << 0xD0;
        QTest::newRow("system") << 0xF8;
        QTest::newRow("data byte") << 0x40;
    }

    void message_othersAreIgnored()
    {
        QFETCH(int, status);
        QVERIFY(!decodeMidiMessage(MidiMessage{static_cast<quint8>(status), 1, 1}).has_value());
    }

    // ---- Port names ----

    void portName_matchesAnyPartInAnyCase()
    {
        QVERIFY(midiPortNameMatches(QStringLiteral("Launchkey Mini MIDI"), QStringLiteral("launchkey")));
        QVERIFY(!midiPortNameMatches(QStringLiteral("Launchkey Mini MIDI"), QStringLiteral("nanoKONTROL")));
        QVERIFY(!midiPortNameMatches(QStringLiteral("Launchkey Mini MIDI"), QString()));
    }

    void portName_alsaClientNumberMayMove()
    {
        QVERIFY(midiPortNameMatches(QStringLiteral("nanoKONTROL2:nanoKONTROL2 MIDI 1 28:0"),
                                    QStringLiteral("nanoKONTROL2:nanoKONTROL2 MIDI 1 24:0")));
    }

    // ---- The real device layer ----

    // The one thing that can run with no device: listing, whatever the machine has.
    // A machine with no MIDI service gets an empty list, not a thrown error.
    void realLayer_listingReturns()
    {
        MidiInput::setBackendFactoryForTest(nullptr);
        const QStringList names = MidiInput::portNames();
        qInfo() << "MIDI ports here:" << names.size();
        QCOMPARE(MidiInput::supported(),
#ifdef NEREUS_HAVE_RTMIDI
                 true
#else
                 false
#endif
        );
        QVERIFY(names.size() >= 0);
    }

    // ---- Controls out of MidiInput ----

    void input_absoluteDeltaFromItsLastValue()
    {
        FakeMidiDeviceLayer layer;
        MidiInput input;
        QSignalSpy events(&input, &MidiInput::controlEvent);
        input.handleMessage(MidiMessage{0xB0, 7, 40});
        input.handleMessage(MidiMessage{0xB0, 7, 45});
        input.handleMessage(MidiMessage{0xB0, 7, 43});
        QCOMPARE(events.size(), 3);
        QCOMPARE(events.at(0).at(0).toString(), QStringLiteral("ch1.cc7"));
        QCOMPARE(events.at(0).at(1).value<MidiControlKind>(), MidiControlKind::AbsoluteKnob);
        QCOMPARE(events.at(0).at(2).toInt(), 40);
        QCOMPARE(events.at(0).at(3).toInt(), 0);
        QCOMPARE(events.at(1).at(3).toInt(), 5);
        QCOMPARE(events.at(2).at(3).toInt(), -2);
    }

    void input_relativeControlsDecodeAndKeepTheirLock()
    {
        FakeMidiDeviceLayer layer;
        MidiInput input;
        input.setRelativeControls({QStringLiteral("ch1.cc16")});
        QSignalSpy events(&input, &MidiInput::controlEvent);
        input.handleMessage(MidiMessage{0xB0, 16, 40});
        input.handleMessage(MidiMessage{0xB0, 16, 63});
        input.handleMessage(MidiMessage{0xB0, 16, 65});
        QCOMPARE(events.size(), 3);
        QCOMPARE(events.at(0).at(1).value<MidiControlKind>(), MidiControlKind::RelativeKnob);
        QCOMPARE(events.at(0).at(3).toInt(), 0);
        QCOMPARE(events.at(1).at(3).toInt(), -1);
        QCOMPARE(events.at(2).at(3).toInt(), 1);
    }

    // ---- Learn ----

    void learn_theNextMessageBinds()
    {
        FakeMidiDeviceLayer layer;
        MidiInput input;
        QSignalSpy learned(&input, &MidiInput::learned);
        QSignalSpy events(&input, &MidiInput::controlEvent);
        input.beginLearn();
        input.handleMessage(MidiMessage{0x90, 0x3C, 0x40});
        QCOMPARE(learned.size(), 1);
        QCOMPARE(learned.at(0).at(0).toString(), QStringLiteral("ch1.note60"));
        QCOMPARE(learned.at(0).at(1).value<MidiControlKind>(), MidiControlKind::Button);
        QCOMPARE(events.size(), 0);
        // Only the next one.
        input.handleMessage(MidiMessage{0x90, 0x3D, 0x40});
        QCOMPARE(learned.size(), 1);
        QCOMPARE(events.size(), 1);
    }

    void learn_aReleaseDoesNotBind_data()
    {
        QTest::addColumn<int>("status");
        QTest::newRow("note off") << 0x80;
        QTest::newRow("note on, velocity 0") << 0x90;
    }

    void learn_aReleaseDoesNotBind()
    {
        QFETCH(int, status);
        FakeMidiDeviceLayer layer;
        MidiInput input;
        QSignalSpy learned(&input, &MidiInput::learned);
        QSignalSpy events(&input, &MidiInput::controlEvent);
        input.beginLearn();
        input.handleMessage(MidiMessage{static_cast<quint8>(status), 0x3C, 0x00});
        QCOMPARE(learned.size(), 0);
        // It still goes on, or a button held across learn would never let go.
        QCOMPARE(events.size(), 1);
        QCOMPARE(events.at(0).at(2).toInt(), 0);
        // And learn is still waiting.
        input.handleMessage(MidiMessage{0xB0, 7, 64});
        QCOMPARE(learned.size(), 1);
    }

    void learn_knobKind_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<int>("data1");
        QTest::addColumn<int>("data2");
        QTest::addColumn<MidiControlKind>("kind");
        QTest::newRow("cc 1") << 0xB0 << 16 << 1 << MidiControlKind::RelativeKnob;
        QTest::newRow("cc 127") << 0xB0 << 16 << 127 << MidiControlKind::RelativeKnob;
        QTest::newRow("cc 64") << 0xB0 << 7 << 64 << MidiControlKind::AbsoluteKnob;
        QTest::newRow("cc 0") << 0xB0 << 7 << 0 << MidiControlKind::AbsoluteKnob;
        QTest::newRow("bend at 1 is no CC") << 0xE0 << 1 << 0 << MidiControlKind::AbsoluteKnob;
    }

    void learn_knobKind()
    {
        QFETCH(int, status);
        QFETCH(int, data1);
        QFETCH(int, data2);
        QFETCH(MidiControlKind, kind);
        FakeMidiDeviceLayer layer;
        MidiInput input;
        QSignalSpy learned(&input, &MidiInput::learned);
        input.beginLearn();
        input.handleMessage(MidiMessage{static_cast<quint8>(status), static_cast<quint8>(data1),
                                        static_cast<quint8>(data2)});
        QCOMPARE(learned.size(), 1);
        QCOMPARE(learned.at(0).at(1).value<MidiControlKind>(), kind);
    }

    void learn_cancelled()
    {
        FakeMidiDeviceLayer layer;
        MidiInput input;
        QSignalSpy learned(&input, &MidiInput::learned);
        input.beginLearn();
        input.cancelLearn();
        input.handleMessage(MidiMessage{0x90, 0x3C, 0x40});
        QCOMPARE(learned.size(), 0);
    }
};

QTEST_MAIN(TestMidiDecoder)
#include "tst_midi_decoder.moc"
