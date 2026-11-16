// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 11.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
//
// Setup > Keys and controllers > MIDI, through a fake device layer: device, profile,
// learn, the kind correction, refusals, and the page with no device or no MIDI
// support. Simulated; no device has been tried (R-HK-19).
#include <QtTest>

#include <QComboBox>
#include <QDialog>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardItemModel>
#include <QSignalSpy>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <memory>

#include "core/AppSettings.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "fakes/FakeMidiBackend.h"
#include "gui/hotkeys/ControllerHost.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/HotKeyTargets.h"
#include "gui/hotkeys/midi/MidiInput.h"
#include "gui/hotkeys/midi/MidiProfile.h"
#include "gui/setup/MidiSetupPage.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

const QString kPad = QStringLiteral("Pad");
const QString kCc7 = QStringLiteral("ch1.cc7");
const QString kNote60 = QStringLiteral("ch1.note60");
const QString kBend = QStringLiteral("ch1.bend");
const QString kNoDevice = QStringLiteral("No MIDI device plugged in.");
const QString kNoSupport = QStringLiteral("This build has no MIDI support.");
constexpr quint8 kControlChange = 0xB0;
constexpr quint8 kNoteOn = 0x90;

class NoTargets : public HotKeyTargets {
public:
    SliceModel* windowSlice() const override { return nullptr; }
    SliceModel* sliceByLetter(QChar) const override { return nullptr; }
    QChar letterOf(const SliceModel*) const override { return QLatin1Char('A'); }
    QString sliceRefusal(const SliceModel*) const override { return {}; }
    bool listened(const SliceModel*) const override { return false; }
    bool tuneTo(SliceModel*, double) override { return true; }
    void runSliceAction(const QString&, SliceModel*, int) override {}
    void runWindowAction(const QString&) override {}
    QString unavailableReason(const QString&) const override { return {}; }
    bool transmitting() const override { return false; }
    bool tuneOrTwoToneOn() const override { return false; }
    bool keyHold(bool) override { return true; }
    void runTransmitAction(const QString&) override {}
    void runCwMessage(const QString&) override {}
    void setCwKeyDown(const QString&, bool) override {}
    void toast(const QString&) override {}
    qint64 nowMs() const override { return 1000; }
};

template <typename T>
T* named(QWidget* root, const QString& name)
{
    return root->findChild<T*>(name);
}

// What a choice in a popup does: the current item moves and activated fires.
bool choose(QComboBox* combo, const QVariant& data)
{
    const int index = combo != nullptr ? combo->findData(data) : -1;
    if (index < 0) {
        return false;
    }
    combo->setCurrentIndex(index);
    emit combo->activated(index);
    return true;
}

bool chooseText(QComboBox* combo, const QString& text)
{
    const int index = combo != nullptr ? combo->findText(text) : -1;
    if (index < 0) {
        return false;
    }
    combo->setCurrentIndex(index);
    emit combo->activated(index);
    return true;
}

} // namespace

class TestHotKeySetupMidi : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<HotKeyBindings> m_bindings;
    std::unique_ptr<NoTargets> m_targets;
    std::unique_ptr<HotKeyDispatcher> m_dispatcher;
    std::unique_ptr<FakeMidiDeviceLayer> m_layer;
    std::unique_ptr<ControllerHost> m_host;
    std::unique_ptr<MidiSetupPage> m_page;

    // Everything queued to the worker has run, and what it sent back has landed.
    // Blocking on the worker's own queue, so no clock and no load in it.
    void flush()
    {
        for (int i = 0; i < 2; ++i) {
            QMetaObject::invokeMethod(m_host->midi(), [] {}, Qt::BlockingQueuedConnection);
            QCoreApplication::processEvents();
        }
    }

    // A page with Pad plugged in and chosen, and its port open.
    MidiSetupPage& openWithDevice()
    {
        m_layer->ports->setNames({kPad});
        AppSettings::instance().setValue(QStringLiteral("HotKeys/Midi/Device"), kPad);
        m_host = std::make_unique<ControllerHost>(m_dispatcher.get(), m_bindings.get());
        flush();
        m_page = std::make_unique<MidiSetupPage>(m_host.get(), m_bindings.get());
        return *m_page;
    }

    void learn(const QString& id, quint8 status, quint8 data1, quint8 data2)
    {
        QVERIFY(choose(named<QComboBox>(m_page.get(), QStringLiteral("midiLearn")), id));
        m_layer->ports->send(status, data1, data2);
        flush();
    }

    QTableWidget* list() { return named<QTableWidget>(m_page.get(), QStringLiteral("midiList")); }
    // The first table row says when the list is empty.
    int rowCount() { return list()->rowCount() - 1; }
    QString status() { return named<QLabel>(m_page.get(), QStringLiteral("midiStatus"))->text(); }
    QComboBox* cell(const QString& what, const QString& key)
    {
        return named<QComboBox>(m_page.get(), QStringLiteral("midi%1:%2").arg(what, key));
    }

    static MidiBinding savedBinding(const QString& profile, const QString& key)
    {
        MidiProfile saved(profile);
        saved.load();
        const QList<MidiBinding>& bindings = saved.bindings();
        for (const MidiBinding& binding : bindings) {
            if (binding.controlKey == key) {
                return binding;
            }
        }
        return MidiBinding{};
    }

private slots:
    void init()
    {
        AppSettings::instance().clear();
        m_bindings = std::make_unique<HotKeyBindings>();
        m_targets = std::make_unique<NoTargets>();
        m_dispatcher = std::make_unique<HotKeyDispatcher>(m_bindings.get(), m_targets.get());
        m_layer = std::make_unique<FakeMidiDeviceLayer>();
    }

    void cleanup()
    {
        m_page.reset();
        m_host.reset();
        m_layer.reset();
        m_dispatcher.reset();
        m_targets.reset();
        m_bindings.reset();
        AppSettings::instance().clear();
    }

    // ---- With no device, and with no MIDI ----

    void noDevice_controlsAreDisabledWithTheReason()
    {
        m_host = std::make_unique<ControllerHost>(m_dispatcher.get(), m_bindings.get());
        m_page = std::make_unique<MidiSetupPage>(m_host.get(), m_bindings.get());
        QCOMPARE(named<QLabel>(m_page.get(), QStringLiteral("midiAvailability"))->text(), kNoDevice);
        for (const char* name : {"midiDevice", "midiProfile", "midiProfileName", "midiProfileNew", "midiProfileRename",
                                 "midiProfileDelete"}) {
            auto* const widget = named<QWidget>(m_page.get(), QLatin1String(name));
            QVERIFY2(widget != nullptr, name);
            QVERIFY2(!widget->isEnabled(), name);
            QVERIFY2(!widget->isHidden(), name);
            QCOMPARE(widget->toolTip(), kNoDevice);
        }
        auto* const controls = named<QGroupBox>(m_page.get(), QStringLiteral("midiControlsGroup"));
        QVERIFY(!controls->isEnabled());
        QVERIFY(!controls->isHidden());
        QCOMPARE(controls->toolTip(), kNoDevice);
    }

    void plugging_inEnablesThePageWithoutReopeningIt()
    {
        m_host = std::make_unique<ControllerHost>(m_dispatcher.get(), m_bindings.get());
        m_page = std::make_unique<MidiSetupPage>(m_host.get(), m_bindings.get());
        m_layer->ports->setNames({kPad});
        m_page->refreshDevices();
        auto* const device = named<QComboBox>(m_page.get(), QStringLiteral("midiDevice"));
        QVERIFY(device->isEnabled());
        QVERIFY(device->findData(kPad) >= 0);
        QCOMPARE(m_page->unavailableReason(), QStringLiteral("Choose a MIDI device."));
        QVERIFY(choose(device, kPad));
        QVERIFY(m_page->unavailableReason().isEmpty());
        QVERIFY(named<QGroupBox>(m_page.get(), QStringLiteral("midiControlsGroup"))->isEnabled());
    }

    void noSupport_samePageDisabledWithItsOwnReason()
    {
        MidiSetupPage& page = openWithDevice();
        page.setSupportedForTest(false);
        QCOMPARE(named<QLabel>(&page, QStringLiteral("midiAvailability"))->text(), kNoSupport);
        auto* const device = named<QComboBox>(&page, QStringLiteral("midiDevice"));
        QVERIFY(!device->isEnabled());
        QCOMPARE(device->toolTip(), kNoSupport);
        QVERIFY(!named<QGroupBox>(&page, QStringLiteral("midiControlsGroup"))->isEnabled());
        QVERIFY(!named<QWidget>(&page, QStringLiteral("midiProfileNew"))->isEnabled());
    }

    void noHost_saysSo()
    {
        MidiSetupPage page(nullptr, nullptr);
        QVERIFY(!page.unavailableReason().isEmpty());
        QVERIFY(!named<QGroupBox>(&page, QStringLiteral("midiControlsGroup"))->isEnabled());
    }

    void savedDeviceGone_canStillBeChangedToNone()
    {
        m_layer->ports->setNames({});
        AppSettings::instance().setValue(QStringLiteral("HotKeys/Midi/Device"), kPad);
        m_host = std::make_unique<ControllerHost>(m_dispatcher.get(), m_bindings.get());
        m_page = std::make_unique<MidiSetupPage>(m_host.get(), m_bindings.get());
        QCOMPARE(m_page->unavailableReason(), kNoDevice);
        auto* const device = named<QComboBox>(m_page.get(), QStringLiteral("midiDevice"));
        QVERIFY(device->isEnabled());
        QCOMPARE(device->currentText(), QStringLiteral("Pad (not plugged in)"));
        QVERIFY(choose(device, QString()));
        QCOMPARE(ControllerHost::savedMidiDevice(), QString());
    }

    void shortcutsOff_disablesWithThatReason()
    {
        MidiSetupPage& page = openWithDevice();
        m_bindings->setEnabled(false);
        QCOMPARE(page.unavailableReason(), QStringLiteral("Use shortcuts is off."));
        QVERIFY(!named<QGroupBox>(&page, QStringLiteral("midiControlsGroup"))->isEnabled());
        m_bindings->setEnabled(true);
        QVERIFY(named<QGroupBox>(&page, QStringLiteral("midiControlsGroup"))->isEnabled());
    }

    // ---- What it says ----

    void aControllerStartsEmptyAndIsUntested()
    {
        MidiSetupPage& page = openWithDevice();
        QCOMPARE(rowCount(), 0);
        QVERIFY(!list()->isRowHidden(0));
        QCOMPARE(list()->item(0, 0)->text(), QStringLiteral("No controls learned."));
        QVERIFY(named<QLabel>(&page, QStringLiteral("midiUntested"))->text().contains(QStringLiteral("Untested")));
        QVERIFY(named<QGroupBox>(&page, QStringLiteral("midiControllerGroup"))->title().contains(QStringLiteral("untested")));
    }

    // ---- The device ----

    void choosingADeviceSavesAndOpensIt()
    {
        m_layer->ports->setNames({kPad});
        m_host = std::make_unique<ControllerHost>(m_dispatcher.get(), m_bindings.get());
        m_page = std::make_unique<MidiSetupPage>(m_host.get(), m_bindings.get());
        auto* const device = named<QComboBox>(m_page.get(), QStringLiteral("midiDevice"));
        QVERIFY(choose(device, kPad));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("HotKeys/Midi/Device")).toString(), kPad);
        flush();
        QCOMPARE(m_layer->ports->opened(), kPad);
        QVERIFY(choose(device, QString()));
        flush();
        QCOMPARE(m_layer->ports->opened(), QString());
    }

    void aDeviceThatWillNotOpenSaysWhy()
    {
        m_layer->ports->setNames({kPad});
        m_layer->ports->refuse = {kPad};
        m_host = std::make_unique<ControllerHost>(m_dispatcher.get(), m_bindings.get());
        m_page = std::make_unique<MidiSetupPage>(m_host.get(), m_bindings.get());
        QVERIFY(choose(named<QComboBox>(m_page.get(), QStringLiteral("midiDevice")), kPad));
        flush();
        QVERIFY(named<QLabel>(m_page.get(), QStringLiteral("midiAvailability"))->text().contains(QStringLiteral("could not be opened")));
    }

    void aSameDeviceListIsNotRebuilt()
    {
        MidiSetupPage& page = openWithDevice();
        auto* const device = named<QComboBox>(&page, QStringLiteral("midiDevice"));
        QSignalSpy removed(device->model(), &QAbstractItemModel::rowsRemoved);
        QSignalSpy reset(device->model(), &QAbstractItemModel::modelReset);
        page.refreshDevices();
        QCOMPARE(removed.size(), 0);
        QCOMPARE(reset.size(), 0);
        QCOMPARE(device->currentData().toString(), kPad);
        m_layer->ports->setNames({kPad, QStringLiteral("Other")});
        page.refreshDevices();
        QVERIFY(device->findData(QStringLiteral("Other")) >= 0);
        QCOMPARE(device->currentData().toString(), kPad);
    }

    // ---- Learn ----

    void learn_aCcFillsBothDirections()
    {
        MidiSetupPage& page = openWithDevice();
        QVERIFY(named<QLabel>(&page, QStringLiteral("midiLearnText"))->text().contains(QStringLiteral("Pick an action")));
        learn(QStringLiteral("tune.up"), kControlChange, 7, 40);
        QCOMPARE(rowCount(), 1);
        QVERIFY(list()->isRowHidden(0));
        QCOMPARE(cell(QStringLiteral("Action"), kCc7)->currentData().toString(), QStringLiteral("tune.up"));
        QCOMPARE(cell(QStringLiteral("Down"), kCc7)->currentData().toString(), QStringLiteral("tune.down"));
        QCOMPARE(cell(QStringLiteral("Kind"), kCc7)->currentData().toInt(), int(MidiControlKind::AbsoluteKnob));
        QCOMPARE(list()->item(1, 0)->text(), QStringLiteral("Ch 1 CC 7"));

        // Saved, active, and running now.
        const QString active = MidiProfile::activeName();
        QCOMPARE(active, QStringLiteral("Default"));
        QCOMPARE(savedBinding(active, kCc7).actionIdDown, QStringLiteral("tune.down"));
        QCOMPARE(m_host->midiProfile().bindings().size(), 1);
        // Learn is over: the combo is back, and the next move learns nothing.
        QCOMPARE(named<QComboBox>(&page, QStringLiteral("midiLearn"))->currentIndex(), 0);
        m_layer->ports->send(kControlChange, 8, 40);
        flush();
        QCOMPARE(rowCount(), 1);
    }

    void learn_aKnobDetentLearnsAsRelative()
    {
        openWithDevice();
        learn(QStringLiteral("volume.up"), kControlChange, 7, 1);
        QCOMPARE(cell(QStringLiteral("Kind"), kCc7)->currentData().toInt(), int(MidiControlKind::RelativeKnob));
        QCOMPARE(cell(QStringLiteral("Down"), kCc7)->currentData().toString(), QStringLiteral("volume.down"));
    }

    void learn_aNoteIsAButtonWithNoOtherDirection()
    {
        openWithDevice();
        learn(QStringLiteral("tune.up"), kNoteOn, 60, 100);
        QCOMPARE(cell(QStringLiteral("Kind"), kNote60)->currentData().toInt(), int(MidiControlKind::Button));
        QVERIFY(cell(QStringLiteral("Down"), kNote60)->currentData().toString().isEmpty());
        QVERIFY(!cell(QStringLiteral("Down"), kNote60)->isEnabled());
        QCOMPARE(cell(QStringLiteral("Down"), kNote60)->toolTip(), QStringLiteral("A button has no other direction."));
    }

    void learn_aButtonMayRunATransmitAction()
    {
        openWithDevice();
        learn(QStringLiteral("tx.hold"), kNoteOn, 60, 100);
        QCOMPARE(rowCount(), 1);
        QCOMPARE(cell(QStringLiteral("Action"), kNote60)->currentData().toString(), QStringLiteral("tx.hold"));
        QCOMPARE(savedBinding(MidiProfile::activeName(), kNote60).actionId, QStringLiteral("tx.hold"));
    }

    void learn_aKnobOnATransmitActionIsRefusedInWords()
    {
        openWithDevice();
        learn(QStringLiteral("tx.hold"), kControlChange, 7, 40);
        QCOMPARE(rowCount(), 0);
        QVERIFY2(status().startsWith(QStringLiteral("A knob can't run")), qPrintable(status()));
    }

    void learn_escapeCancels()
    {
        MidiSetupPage& page = openWithDevice();
        QDialog dialog;
        auto* const layout = new QVBoxLayout(&dialog);
        layout->addWidget(&page);
        dialog.show();

        auto* const combo = named<QComboBox>(&page, QStringLiteral("midiLearn"));
        QVERIFY(choose(combo, QStringLiteral("tune.up")));
        QVERIFY(named<QPushButton>(&page, QStringLiteral("midiLearnCancel"))->isEnabled());
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QCoreApplication::sendEvent(combo, &press);
        QKeyEvent release(QEvent::KeyRelease, Qt::Key_Escape, Qt::NoModifier);
        QCoreApplication::sendEvent(combo, &release);
        // Esc ended learn and stopped there: Setup is still open.
        QVERIFY(dialog.isVisible());
        QCOMPARE(combo->currentIndex(), 0);
        QVERIFY(!named<QPushButton>(&page, QStringLiteral("midiLearnCancel"))->isEnabled());
        QVERIFY(named<QLabel>(&page, QStringLiteral("midiLearnText"))->text().contains(QStringLiteral("Pick an action")));

        m_layer->ports->send(kControlChange, 7, 40);
        flush();
        QCOMPARE(rowCount(), 0);

        // With learn over, Esc is the dialog's again.
        QKeyEvent next(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QCoreApplication::sendEvent(combo, &next);
        QVERIFY(!dialog.isVisible());
        layout->removeWidget(&page);
        page.setParent(nullptr);
    }

    void learn_theCancelButtonAndNoneCancelToo()
    {
        MidiSetupPage& page = openWithDevice();
        auto* const combo = named<QComboBox>(&page, QStringLiteral("midiLearn"));
        QVERIFY(choose(combo, QStringLiteral("tune.up")));
        named<QPushButton>(&page, QStringLiteral("midiLearnCancel"))->click();
        m_layer->ports->send(kControlChange, 7, 40);
        flush();
        QCOMPARE(rowCount(), 0);

        QVERIFY(choose(combo, QStringLiteral("tune.up")));
        QVERIFY(choose(combo, QString()));
        m_layer->ports->send(kControlChange, 7, 40);
        flush();
        QCOMPARE(rowCount(), 0);
    }

    void learn_aControlLearnedAgainKeepsItsCorrectedKind()
    {
        openWithDevice();
        learn(QStringLiteral("tune.up"), kControlChange, 7, 1);
        QVERIFY(choose(cell(QStringLiteral("Kind"), kCc7), int(MidiControlKind::AbsoluteKnob)));
        learn(QStringLiteral("volume.up"), kControlChange, 7, 127);
        QCOMPARE(rowCount(), 1);
        QCOMPARE(cell(QStringLiteral("Kind"), kCc7)->currentData().toInt(), int(MidiControlKind::AbsoluteKnob));
        QCOMPARE(cell(QStringLiteral("Action"), kCc7)->currentData().toString(), QStringLiteral("volume.up"));
        QCOMPARE(cell(QStringLiteral("Down"), kCc7)->currentData().toString(), QStringLiteral("volume.down"));
    }

    // ---- The kind ----

    void kind_correctionIsSavedAndRunsNow()
    {
        openWithDevice();
        learn(QStringLiteral("tune.up"), kControlChange, 7, 1);    // a fader that happened to send 1
        QVERIFY(choose(cell(QStringLiteral("Kind"), kCc7), int(MidiControlKind::AbsoluteKnob)));
        QCOMPARE(savedBinding(MidiProfile::activeName(), kCc7).kind, MidiControlKind::AbsoluteKnob);
        QCOMPARE(m_host->midiProfile().bindings().first().kind, MidiControlKind::AbsoluteKnob);

        QVERIFY(choose(cell(QStringLiteral("Kind"), kCc7), int(MidiControlKind::RelativeKnob)));
        QCOMPARE(savedBinding(MidiProfile::activeName(), kCc7).kind, MidiControlKind::RelativeKnob);
    }

    void kind_aCorrectionKeepsTheRowsOrder()
    {
        openWithDevice();
        learn(QStringLiteral("tune.up"), kControlChange, 7, 40);
        learn(QStringLiteral("volume.up"), kControlChange, 8, 40);
        QVERIFY(choose(cell(QStringLiteral("Kind"), kCc7), int(MidiControlKind::RelativeKnob)));
        QCOMPARE(m_host->midiProfile().bindings().first().controlKey, kCc7);
        QCOMPARE(list()->item(1, 0)->text(), QStringLiteral("Ch 1 CC 7"));
        QCOMPARE(list()->item(2, 0)->text(), QStringLiteral("Ch 1 CC 8"));
    }

    void kind_aKnobCannotBecomeWhatItsActionRefuses()
    {
        openWithDevice();
        learn(QStringLiteral("tx.hold"), kNoteOn, 60, 100);
        learn(QStringLiteral("nr.toggle"), kControlChange, 7, 40);
        // A CC can be a button, which may run tx.hold.
        QVERIFY(choose(cell(QStringLiteral("Kind"), kCc7), int(MidiControlKind::Button)));
        QVERIFY(choose(cell(QStringLiteral("Action"), kCc7), QStringLiteral("tx.hold")));
        QCOMPARE(savedBinding(MidiProfile::activeName(), kCc7).actionId, QStringLiteral("tx.hold"));
        // Back to a knob is refused, and the row shows what is still true.
        QVERIFY(choose(cell(QStringLiteral("Kind"), kCc7), int(MidiControlKind::AbsoluteKnob)));
        QVERIFY2(status().startsWith(QStringLiteral("A knob can't run")), qPrintable(status()));
        QCOMPARE(cell(QStringLiteral("Kind"), kCc7)->currentData().toInt(), int(MidiControlKind::Button));
        QCOMPARE(savedBinding(MidiProfile::activeName(), kCc7).kind, MidiControlKind::Button);
    }

    void kind_aButtonTurnedKnobGetsItsOtherDirection()
    {
        openWithDevice();
        learn(QStringLiteral("tune.up"), kControlChange, 7, 40);
        QVERIFY(choose(cell(QStringLiteral("Kind"), kCc7), int(MidiControlKind::Button)));
        QVERIFY(cell(QStringLiteral("Down"), kCc7)->currentData().toString().isEmpty());
        QVERIFY(choose(cell(QStringLiteral("Kind"), kCc7), int(MidiControlKind::RelativeKnob)));
        QCOMPARE(cell(QStringLiteral("Down"), kCc7)->currentData().toString(), QStringLiteral("tune.down"));
    }

    void kind_aNoteAndABendOfferOnlyWhatTheyCanBe()
    {
        openWithDevice();
        learn(QStringLiteral("tune.up"), kNoteOn, 60, 100);
        learn(QStringLiteral("tune.up"), 0xE0, 0, 64);
        const auto enabled = [](QComboBox* combo, MidiControlKind kind) {
            auto* const model = qobject_cast<QStandardItemModel*>(combo->model());
            return model->item(combo->findData(int(kind)))->isEnabled();
        };
        QComboBox* const note = cell(QStringLiteral("Kind"), kNote60);
        QVERIFY(enabled(note, MidiControlKind::Button));
        QVERIFY(!enabled(note, MidiControlKind::AbsoluteKnob));
        QVERIFY(!enabled(note, MidiControlKind::RelativeKnob));
        QCOMPARE(note->itemData(note->findData(int(MidiControlKind::RelativeKnob)), Qt::ToolTipRole).toString(),
                 QStringLiteral("A note can only be a button."));
        QComboBox* const bend = cell(QStringLiteral("Kind"), kBend);
        QVERIFY(!enabled(bend, MidiControlKind::Button));
        QVERIFY(enabled(bend, MidiControlKind::AbsoluteKnob));
        QVERIFY(!enabled(bend, MidiControlKind::RelativeKnob));
    }

    // ---- The row ----

    void row_aRefusedActionPutsTheRowBack()
    {
        openWithDevice();
        learn(QStringLiteral("tune.up"), kControlChange, 7, 40);
        QVERIFY(choose(cell(QStringLiteral("Action"), kCc7), QStringLiteral("tx.mox")));
        QVERIFY2(status().startsWith(QStringLiteral("A knob can't run")), qPrintable(status()));
        QCOMPARE(cell(QStringLiteral("Action"), kCc7)->currentData().toString(), QStringLiteral("tune.up"));
        QVERIFY(choose(cell(QStringLiteral("Down"), kCc7), QStringLiteral("tx.hold")));
        QVERIFY2(status().startsWith(QStringLiteral("A knob can't run")), qPrintable(status()));
        QCOMPARE(cell(QStringLiteral("Down"), kCc7)->currentData().toString(), QStringLiteral("tune.down"));
    }

    void row_theWheelOverARowLeavesItAlone()
    {
        openWithDevice();
        learn(QStringLiteral("tune.up"), kControlChange, 7, 40);
        QComboBox* const action = cell(QStringLiteral("Action"), kCc7);
        const QPointF at(4, 4);
        QWheelEvent wheel(at, at, QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(action, &wheel);
        QCOMPARE(action->currentData().toString(), QStringLiteral("tune.up"));
        QCOMPARE(savedBinding(MidiProfile::activeName(), kCc7).actionId, QStringLiteral("tune.up"));
    }

    void row_noneIsNoAction()
    {
        openWithDevice();
        learn(QStringLiteral("tune.up"), kControlChange, 7, 40);
        QVERIFY(choose(cell(QStringLiteral("Action"), kCc7), QString()));
        QCOMPARE(status(), QStringLiteral("Choose an action first."));
        QCOMPARE(cell(QStringLiteral("Action"), kCc7)->currentData().toString(), QStringLiteral("tune.up"));
        // The other direction may do nothing.
        QVERIFY(choose(cell(QStringLiteral("Down"), kCc7), QString()));
        QVERIFY(savedBinding(MidiProfile::activeName(), kCc7).actionIdDown.isEmpty());
    }

    void row_theOtherDirectionFollowsOnlyIfItWasTheActionsOwn()
    {
        openWithDevice();
        learn(QStringLiteral("tune.up"), kControlChange, 7, 40);
        QVERIFY(choose(cell(QStringLiteral("Action"), kCc7), QStringLiteral("zoom.in")));
        QCOMPARE(cell(QStringLiteral("Down"), kCc7)->currentData().toString(), QStringLiteral("zoom.out"));
        QVERIFY(choose(cell(QStringLiteral("Down"), kCc7), QStringLiteral("band.next")));
        QVERIFY(choose(cell(QStringLiteral("Action"), kCc7), QStringLiteral("tune.up")));
        QCOMPARE(cell(QStringLiteral("Down"), kCc7)->currentData().toString(), QStringLiteral("band.next"));
    }

    void row_actsOnPinsASliceAndFixedOnesSayWhy()
    {
        openWithDevice();
        learn(QStringLiteral("tune.up"), kControlChange, 7, 40);
        QComboBox* const actsOn = cell(QStringLiteral("ActsOn"), kCc7);
        QVERIFY(actsOn->isEnabled());
        QVERIFY(chooseText(actsOn, QStringLiteral("Slice B")));
        QCOMPARE(savedBinding(MidiProfile::activeName(), kCc7).pin, QLatin1Char('B'));
        QCOMPARE(m_host->midiProfile().bindings().first().pin, QLatin1Char('B'));

        learn(QStringLiteral("tx.hold"), kNoteOn, 60, 100);
        QComboBox* const fixed = cell(QStringLiteral("ActsOn"), kNote60);
        QVERIFY(!fixed->isEnabled());
        QCOMPARE(fixed->currentText(), QStringLiteral("Transmit slice"));
        QCOMPARE(fixed->toolTip(), QStringLiteral("Transmit actions always use the transmit slice."));
    }

    void row_aFixedTargetActionDropsThePin()
    {
        openWithDevice();
        learn(QStringLiteral("tune.up"), kControlChange, 7, 40);
        QVERIFY(chooseText(cell(QStringLiteral("ActsOn"), kCc7), QStringLiteral("Slice B")));
        QVERIFY(choose(cell(QStringLiteral("Action"), kCc7), QStringLiteral("zoom.in")));
        QVERIFY(savedBinding(MidiProfile::activeName(), kCc7).pin.isNull());
        QVERIFY(!cell(QStringLiteral("ActsOn"), kCc7)->isEnabled());
    }

    void row_clearRemovesItAndLeavesTheOthersWidgets()
    {
        openWithDevice();
        learn(QStringLiteral("tune.up"), kControlChange, 7, 40);
        learn(QStringLiteral("volume.up"), kControlChange, 8, 40);
        QComboBox* const other = cell(QStringLiteral("Action"), QStringLiteral("ch1.cc8"));
        named<QPushButton>(m_page.get(), QStringLiteral("midiClear:") + kCc7)->click();
        QCOMPARE(rowCount(), 1);
        QCOMPARE(cell(QStringLiteral("Action"), QStringLiteral("ch1.cc8")), other);
        QVERIFY(cell(QStringLiteral("Action"), kCc7) == nullptr);
        QCOMPARE(m_host->midiProfile().bindings().size(), 1);
        QCOMPARE(savedBinding(MidiProfile::activeName(), QStringLiteral("ch1.cc8")).actionId, QStringLiteral("volume.up"));
        named<QPushButton>(m_page.get(), QStringLiteral("midiClear:ch1.cc8"))->click();
        QCOMPARE(rowCount(), 0);
        QVERIFY(!list()->isRowHidden(0));
    }

    void row_anActionThisWindowCannotUseIsDisabledWithTheReason()
    {
        MidiSetupPage& page = openWithDevice();
        const QString reason = QStringLiteral("Not on this window.");
        page.setUnavailableReasonProvider([reason](const QString& id) {
            return id == QLatin1String("tune.goto") ? reason : QString();
        });
        auto* const combo = named<QComboBox>(&page, QStringLiteral("midiLearn"));
        auto* const model = qobject_cast<QStandardItemModel*>(combo->model());
        QStandardItem* const item = model->item(combo->findData(QStringLiteral("tune.goto")));
        QVERIFY(!item->isEnabled());
        QCOMPARE(item->toolTip(), reason);
        QVERIFY(model->item(combo->findData(QStringLiteral("tune.up")))->isEnabled());
    }

    // ---- Profiles ----

    void profile_newStartsEmptyAndTheOldOneStays()
    {
        MidiSetupPage& page = openWithDevice();
        learn(QStringLiteral("tune.up"), kControlChange, 7, 40);
        auto* const name = named<QLineEdit>(&page, QStringLiteral("midiProfileName"));
        auto* const create = named<QPushButton>(&page, QStringLiteral("midiProfileNew"));
        QVERIFY(!create->isEnabled());
        QCOMPARE(create->toolTip(), QStringLiteral("Type a name first."));
        name->setText(QStringLiteral("Contest"));
        QVERIFY(create->isEnabled());
        create->click();

        QCOMPARE(rowCount(), 0);
        QCOMPARE(MidiProfile::activeName(), QStringLiteral("Contest"));
        QCOMPARE(m_host->midiProfile().bindings().size(), 0);
        QVERIFY(MidiProfile::savedNames().contains(QStringLiteral("Default")));
        QVERIFY(cell(QStringLiteral("Action"), kCc7) == nullptr);

        auto* const combo = named<QComboBox>(&page, QStringLiteral("midiProfile"));
        QCOMPARE(combo->currentText(), QStringLiteral("Contest"));
        QVERIFY(chooseText(combo, QStringLiteral("Default")));
        QCOMPARE(rowCount(), 1);
        QCOMPARE(MidiProfile::activeName(), QStringLiteral("Default"));
        QCOMPARE(m_host->midiProfile().bindings().size(), 1);
    }

    void profile_aNameInUseIsRefused()
    {
        MidiSetupPage& page = openWithDevice();
        auto* const name = named<QLineEdit>(&page, QStringLiteral("midiProfileName"));
        name->setText(QStringLiteral("Default"));
        auto* const create = named<QPushButton>(&page, QStringLiteral("midiProfileNew"));
        QVERIFY(!create->isEnabled());
        QCOMPARE(create->toolTip(), QStringLiteral("A profile named Default already exists."));
        QVERIFY(!named<QPushButton>(&page, QStringLiteral("midiProfileRename"))->isEnabled());
    }

    void profile_renameKeepsTheRows()
    {
        MidiSetupPage& page = openWithDevice();
        learn(QStringLiteral("tune.up"), kControlChange, 7, 40);
        named<QLineEdit>(&page, QStringLiteral("midiProfileName"))->setText(QStringLiteral("Contest"));
        named<QPushButton>(&page, QStringLiteral("midiProfileRename"))->click();
        QCOMPARE(rowCount(), 1);
        QCOMPARE(MidiProfile::activeName(), QStringLiteral("Contest"));
        QCOMPARE(MidiProfile::savedNames(), QStringList{QStringLiteral("Contest")});
        QCOMPARE(savedBinding(QStringLiteral("Contest"), kCc7).actionId, QStringLiteral("tune.up"));
        QCOMPARE(named<QComboBox>(&page, QStringLiteral("midiProfile"))->currentText(), QStringLiteral("Contest"));
    }

    void profile_deleteGoesToAnotherAndTheLastCannotGo()
    {
        MidiSetupPage& page = openWithDevice();
        learn(QStringLiteral("tune.up"), kControlChange, 7, 40);
        auto* const remove = named<QPushButton>(&page, QStringLiteral("midiProfileDelete"));
        QVERIFY(!remove->isEnabled());
        QVERIFY(remove->toolTip().contains(QStringLiteral("only profile")));
        named<QLineEdit>(&page, QStringLiteral("midiProfileName"))->setText(QStringLiteral("Contest"));
        named<QPushButton>(&page, QStringLiteral("midiProfileNew"))->click();
        QVERIFY(remove->isEnabled());
        remove->click();
        QCOMPARE(MidiProfile::savedNames(), QStringList{QStringLiteral("Default")});
        QCOMPARE(MidiProfile::activeName(), QStringLiteral("Default"));
        QCOMPARE(rowCount(), 1);
        QCOMPARE(m_host->midiProfile().bindings().size(), 1);
        QVERIFY(!remove->isEnabled());
    }

    void profile_theActiveOneComesBackOnANewPage()
    {
        openWithDevice();
        learn(QStringLiteral("tune.up"), kControlChange, 7, 40);
        m_page = std::make_unique<MidiSetupPage>(m_host.get(), m_bindings.get());
        QCOMPARE(rowCount(), 1);
        QCOMPARE(cell(QStringLiteral("Action"), kCc7)->currentData().toString(), QStringLiteral("tune.up"));
    }

    void words_plainAndAmerican()
    {
        MidiSetupPage& page = openWithDevice();
        const QList<QLabel*> labels = page.findChildren<QLabel*>();
        for (QLabel* const label : labels) {
            QVERIFY2(!label->text().contains(QChar(0x2014)), qPrintable(label->text()));
        }
    }
};

QTEST_MAIN(TestHotKeySetupMidi)
#include "tst_hot_key_setup_midi.moc"
