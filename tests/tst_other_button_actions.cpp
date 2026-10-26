// =================================================================
// tests/tst_other_button_actions.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. It drives the container receive
// buttons through ContainerButtonDispatcher against a real RadioModel; no
// upstream logic is ported here.
//
// Container item parity Task 60: 31 receive buttons act on the container's
// slice (Thetis console.cs:51391-51614 [v2.10.3.15]):
//   - With slice A on the container and slice B active, each click
//     changes slice A only and lights as Thetis's clsOtherButtons does.
//   - NR cycles off, NR1-NR4, off; a NereusSDR-only reducer goes to off
//     and names itself on the NR button. NB cycles off, NB, NB2.
//   - A refused NR3 changes nothing and says the slice's reason.
//   - AF, BAL and AGC steps clamp at the ends.
//   - A container on a slice that is not open shows every receive button
//     unavailable with noSliceReason, and a click changes nothing.
//   - Mute All skips slices this window may not change.
//   - MNF+ adds a notch on the container's slice, and none while
//     transmitting.
//   - None keys the radio.
//   - Each receive button is offered, and its editor row is enabled.
//   - A settings preview's click never reaches the dispatcher.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26  J.J. Boyd / KG4VCF  Container item parity Task 60.
//                                    AI-assisted via Anthropic Claude
//                                    Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QSignalSpy>
#include <QSplitter>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/ControlRanges.h"
#include "core/MoxController.h"
#include "gui/UnbuiltFeatures.h"
#include "gui/containers/ContainerButtonDispatcher.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerPreviewWidget.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/meter_property_editors/OtherButtonItemEditor.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/OtherButtonItem.h"
#include "models/NotchModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <functional>
#include <memory>

using namespace NereusSDR;
using Id = OtherButtonItem::ButtonId;

namespace {

constexpr int kSliceA = 1;  // ContainerWidget::rxSource() for slice A
constexpr int kSliceC = 3;  // never opened here
constexpr int kSize = 600;

// The 31 receive buttons of Task 60, in Thetis id order.
const QList<Id> kReceive = {
    Id::Nr, Id::Nr1, Id::Nr2, Id::Nr3, Id::Nr4, Id::Nb, Id::Nb1, Id::Nb2, Id::MnfPlus,
    Id::Rit, Id::Rit0, Id::Xit, Id::Xit0, Id::LockA, Id::TuneStepUp, Id::TuneStepDown,
    Id::MuteAll, Id::SqlVsql, Id::AfP5, Id::AfM5, Id::BalP5, Id::BalM5,
    Id::AgcFixed, Id::AgcLong, Id::AgcSlow, Id::AgcMedium, Id::AgcFast, Id::AgcCustom,
    Id::AgcAuto, Id::AgcP5, Id::AgcM5,
};

// Everything a receive button may change on a slice.
struct SliceSnapshot {
    NrSlot nr;
    NbMode nb;
    bool rit;
    int ritHz;
    bool xit;
    int xitHz;
    bool locked;
    int stepHz;
    bool muted;
    bool ssql;
    int af;
    double pan;
    AGCMode agc;
    bool autoAgc;
    int agcThreshold;

    static SliceSnapshot of(const SliceModel& s)
    {
        return {s.activeNr(), s.nbMode(), s.ritEnabled(), s.ritHz(), s.xitEnabled(), s.xitHz(),
                s.locked(), s.stepHz(), s.muted(), s.ssqlEnabled(), s.afGain(), s.audioPan(),
                s.agcMode(), s.autoAgcEnabled(), s.agcThreshold()};
    }
    bool operator==(const SliceSnapshot& o) const
    {
        return nr == o.nr && nb == o.nb && rit == o.rit && ritHz == o.ritHz && xit == o.xit
            && xitHz == o.xitHz && locked == o.locked && stepHz == o.stepHz && muted == o.muted
            && ssql == o.ssql && af == o.af && qFuzzyCompare(pan + 2.0, o.pan + 2.0)
            && agc == o.agc && autoAgc == o.autoAgc && agcThreshold == o.agcThreshold;
    }
};

OtherButtonItem* firstGrid(const QList<MeterWidget*>& meters)
{
    for (MeterWidget* meter : meters) {
        for (MeterItem* item : meter->items()) {
            if (auto* other = qobject_cast<OtherButtonItem*>(item)) { return other; }
        }
    }
    return nullptr;
}

} // namespace

class TstOtherButtonActions : public QObject {
    Q_OBJECT

private:
    // A local model with slices A and B, B active, and no radio (the
    // fixture of tst_other_button_item.cpp).
    struct Fixture {
        RadioModel model;
        SliceModel* a{nullptr};
        SliceModel* b{nullptr};
        // Slice ids this window may not change (another device's).
        QSet<int> refused;
        std::unique_ptr<ContainerButtonDispatcher> dispatcher;

        Fixture()
        {
            model.addSlice();
            model.addSlice();
            a = model.sliceById(0);
            b = model.sliceById(1);
            model.setActiveSliceById(1);
            a->setFrequency(14100000.0);
            b->setFrequency(3700000.0);

            ContainerButtonDispatcher::Hooks hooks;
            hooks.transmitPermitted = [] { return true; };
            hooks.sliceRefusal = [this](int sliceId) {
                return refused.contains(sliceId)
                    ? QStringLiteral("Another device controls this slice.") : QString();
            };
            dispatcher = std::make_unique<ContainerButtonDispatcher>(&model, std::move(hooks));
        }
    };

    struct Row {
        Id id;
        std::function<void(SliceModel&)> prepare;
        std::function<bool(const SliceModel&)> changed;
        bool litAfter;
    };

    static QList<Row> rows()
    {
        using S = SliceModel;
        const auto agcRow = [](Id id, AGCMode mode, AGCMode from) {
            return Row{id, [from](S& s) { s.setAgcMode(from); },
                       [mode](const S& s) { return s.agcMode() == mode; }, true};
        };
        return {
            {Id::Nr, [](S& s) { s.setActiveNr(NrSlot::Off); },
             [](const S& s) { return s.activeNr() == NrSlot::NR1; }, true},
            {Id::Nr1, [](S& s) { s.setActiveNr(NrSlot::Off); },
             [](const S& s) { return s.activeNr() == NrSlot::NR1; }, true},
            {Id::Nr2, [](S& s) { s.setActiveNr(NrSlot::NR1); },
             [](const S& s) { return s.activeNr() == NrSlot::NR2; }, true},
            {Id::Nr3, [](S& s) { s.setActiveNr(NrSlot::Off); },
             [](const S& s) { return s.activeNr() == NrSlot::NR3; }, true},
            {Id::Nr4, [](S& s) { s.setActiveNr(NrSlot::Off); },
             [](const S& s) { return s.activeNr() == NrSlot::NR4; }, true},
            {Id::Nb, [](S& s) { s.setNbMode(NbMode::Off); },
             [](const S& s) { return s.nbMode() == NbMode::NB; }, true},
            {Id::Nb1, [](S& s) { s.setNbMode(NbMode::NB2); },
             [](const S& s) { return s.nbMode() == NbMode::NB; }, true},
            {Id::Nb2, [](S& s) { s.setNbMode(NbMode::Off); },
             [](const S& s) { return s.nbMode() == NbMode::NB2; }, true},
            {Id::Rit, [](S& s) { s.setRitEnabled(false); },
             [](const S& s) { return s.ritEnabled(); }, true},
            {Id::Rit0, [](S& s) { s.setRitHz(500); },
             [](const S& s) { return s.ritHz() == 0; }, false},
            {Id::Xit, [](S& s) { s.setXitEnabled(false); },
             [](const S& s) { return s.xitEnabled(); }, true},
            {Id::Xit0, [](S& s) { s.setXitHz(-300); },
             [](const S& s) { return s.xitHz() == 0; }, false},
            {Id::LockA, [](S& s) { s.setLocked(false); },
             [](const S& s) { return s.locked(); }, true},
            {Id::TuneStepUp, [](S& s) { s.setStepHz(100); },
             [](const S& s) { return s.stepHz() > 100; }, false},
            {Id::TuneStepDown, [](S& s) { s.setStepHz(100); },
             [](const S& s) { return s.stepHz() < 100; }, false},
            {Id::SqlVsql, [](S& s) { s.setSsqlEnabled(false); },
             [](const S& s) { return s.ssqlEnabled(); }, true},
            {Id::AfP5, [](S& s) { s.setAfGain(50); },
             [](const S& s) { return s.afGain() == 55; }, false},
            {Id::AfM5, [](S& s) { s.setAfGain(50); },
             [](const S& s) { return s.afGain() == 45; }, false},
            {Id::BalP5, [](S& s) { s.setAudioPan(0.0); },
             [](const S& s) { return qFuzzyCompare(s.audioPan(), 0.1); }, false},
            {Id::BalM5, [](S& s) { s.setAudioPan(0.0); },
             [](const S& s) { return qFuzzyCompare(s.audioPan(), -0.1); }, false},
            agcRow(Id::AgcFixed, AGCMode::Off, AGCMode::Med),
            agcRow(Id::AgcLong, AGCMode::Long, AGCMode::Med),
            agcRow(Id::AgcSlow, AGCMode::Slow, AGCMode::Med),
            agcRow(Id::AgcMedium, AGCMode::Med, AGCMode::Fast),
            agcRow(Id::AgcFast, AGCMode::Fast, AGCMode::Med),
            agcRow(Id::AgcCustom, AGCMode::Custom, AGCMode::Med),
            {Id::AgcAuto, [](S& s) { s.setAutoAgcEnabled(false); },
             [](const S& s) { return s.autoAgcEnabled(); }, true},
            {Id::AgcP5, [](S& s) { s.setAgcThreshold(-60); },
             [](const S& s) { return s.agcThreshold() == -55; }, false},
            {Id::AgcM5, [](S& s) { s.setAgcThreshold(-60); },
             [](const S& s) { return s.agcThreshold() == -65; }, false},
        };
    }

private slots:
    void init()
    {
        UnbuiltFeatures::resetForTest();
        AppSettings::instance().clear();
    }
    void cleanup() { UnbuiltFeatures::resetForTest(); }

    // Acceptance: every row, with slice A on the container and slice B
    // active, changes slice A only and lights as the row says.
    void everyRowChangesTheContainersSliceOnly()
    {
        for (const Row& row : rows()) {
            Fixture f;
            row.prepare(*f.a);
            row.prepare(*f.b);
            const SliceSnapshot bBefore = SliceSnapshot::of(*f.b);
            const auto before = f.dispatcher->stateOf(row.id, kSliceA);
            QVERIFY2(before.available, qPrintable(QString::number(int(row.id))));

            QCOMPARE(f.dispatcher->click(row.id, kSliceA), QString());
            QVERIFY2(row.changed(*f.a), qPrintable(QString::number(int(row.id))));
            QVERIFY2(SliceSnapshot::of(*f.b) == bBefore, qPrintable(QString::number(int(row.id))));
            QCOMPARE(f.model.activeSlice(), f.b);
            QCOMPARE(f.dispatcher->stateOf(row.id, kSliceA).on, row.litAfter);
            // Slice B's container lights from slice B, not A.
            QVERIFY2(!f.dispatcher->stateOf(row.id, kSliceA + 1).on,
                     qPrintable(QString::number(int(row.id))));
        }
    }

    // The toggles go back on a second click; the select buttons go off.
    void secondClicksTurnThemOff()
    {
        Fixture f;
        for (Id id : {Id::Nr1, Id::Nr2, Id::Nr3, Id::Nr4}) {
            f.dispatcher->click(id, kSliceA);
            QVERIFY(f.dispatcher->stateOf(id, kSliceA).on);
            f.dispatcher->click(id, kSliceA);
            QCOMPARE(f.a->activeNr(), NrSlot::Off);
            QVERIFY(!f.dispatcher->stateOf(id, kSliceA).on);
        }
        for (Id id : {Id::Nb1, Id::Nb2}) {
            f.dispatcher->click(id, kSliceA);
            QVERIFY(f.dispatcher->stateOf(id, kSliceA).on);
            f.dispatcher->click(id, kSliceA);
            QCOMPARE(f.a->nbMode(), NbMode::Off);
        }
        for (Id id : {Id::Rit, Id::Xit, Id::LockA, Id::SqlVsql, Id::AgcAuto}) {
            f.dispatcher->click(id, kSliceA);
            QVERIFY(f.dispatcher->stateOf(id, kSliceA).on);
            f.dispatcher->click(id, kSliceA);
            QVERIFY(!f.dispatcher->stateOf(id, kSliceA).on);
        }
        // Only the selected AGC mode lights.
        f.dispatcher->click(Id::AgcSlow, kSliceA);
        for (Id id : {Id::AgcFixed, Id::AgcLong, Id::AgcSlow, Id::AgcMedium, Id::AgcFast,
                      Id::AgcCustom}) {
            QCOMPARE(f.dispatcher->stateOf(id, kSliceA).on, id == Id::AgcSlow);
        }
        // Momentary buttons never light.
        for (Id id : {Id::Rit0, Id::Xit0, Id::TuneStepUp, Id::TuneStepDown, Id::AfP5, Id::AfM5,
                      Id::BalP5, Id::BalM5, Id::AgcP5, Id::AgcM5, Id::MnfPlus}) {
            QVERIFY(!f.dispatcher->stateOf(id, kSliceA).on);
        }
    }

    // NR cycles as Thetis incrementNR (console.cs:43327-43339); the NR
    // button names the reducer (MeterManager.cs:8700). A NereusSDR-only
    // reducer goes to off and names itself.
    void nrAndNbCycleAndNameTheReducer()
    {
        Fixture f;
        QCOMPARE(f.dispatcher->stateOf(Id::Nr, kSliceA).label, QStringLiteral("NR"));
        const QList<QPair<NrSlot, QString>> cycle = {
            {NrSlot::NR1, QStringLiteral("NR")}, {NrSlot::NR2, QStringLiteral("NR2")},
            {NrSlot::NR3, QStringLiteral("NR3")}, {NrSlot::NR4, QStringLiteral("NR4")},
            {NrSlot::Off, QStringLiteral("NR")}};
        for (const auto& step : cycle) {
            QCOMPARE(f.dispatcher->click(Id::Nr, kSliceA), QString());
            QCOMPARE(f.a->activeNr(), step.first);
            const auto st = f.dispatcher->stateOf(Id::Nr, kSliceA);
            QCOMPARE(st.label, step.second);
            QCOMPARE(st.on, step.first != NrSlot::Off);
        }
        // This computer may not run every NereusSDR-only reducer; take them
        // all here, as a Core that has them would.
        f.a->setNrSelectionApplier([](NrSlot, QString*) { return true; });
        for (NrSlot own : {NrSlot::DFNR, NrSlot::BNR, NrSlot::MNR, NrSlot::NNR}) {
            f.a->setActiveNr(own);
            QCOMPARE(f.a->activeNr(), own);
            const QString name = OtherButtonItem::nrLabel(own);
            QCOMPARE(f.dispatcher->stateOf(Id::Nr, kSliceA).label, name);
            QVERIFY(f.dispatcher->stateOf(Id::Nr, kSliceA).on);
            f.dispatcher->click(Id::Nr, kSliceA);
            QCOMPARE(f.a->activeNr(), NrSlot::Off);
        }
        QCOMPARE(OtherButtonItem::nrLabel(NrSlot::DFNR), QStringLiteral("DFNR"));
        QCOMPARE(OtherButtonItem::nrLabel(NrSlot::NNR), QStringLiteral("NNR"));

        const QList<QPair<NbMode, QString>> nb = {
            {NbMode::NB, QStringLiteral("NB")}, {NbMode::NB2, QStringLiteral("NB2")},
            {NbMode::Off, QStringLiteral("NB")}};
        for (const auto& step : nb) {
            f.dispatcher->click(Id::Nb, kSliceA);
            QCOMPARE(f.a->nbMode(), step.first);
            QCOMPARE(f.dispatcher->stateOf(Id::Nb, kSliceA).label, step.second);
            QCOMPARE(f.dispatcher->stateOf(Id::Nb, kSliceA).on, step.first != NbMode::Off);
            // NB1 / NB2 light with the blanker they name.
            QCOMPARE(f.dispatcher->stateOf(Id::Nb1, kSliceA).on, step.first == NbMode::NB);
            QCOMPARE(f.dispatcher->stateOf(Id::Nb2, kSliceA).on, step.first == NbMode::NB2);
        }

        // apply() puts the caption on the grid; it moves with the id and is
        // never saved.
        OtherButtonItem item;
        f.a->setActiveNr(NrSlot::NR3);
        f.dispatcher->apply(&item, kSliceA);
        QCOMPARE(item.buttonLabel(Id::Nr), QStringLiteral("NR3"));
        const QString saved = item.serialize();
        f.a->setActiveNr(NrSlot::Off);
        f.dispatcher->apply(&item, kSliceA);
        QCOMPARE(item.buttonLabel(Id::Nr), QStringLiteral("NR"));
        QCOMPARE(item.serialize(), saved);
        // A slice that is not open shows the button's own caption.
        f.a->setActiveNr(NrSlot::NR2);
        f.dispatcher->apply(&item, kSliceA);
        QCOMPARE(item.buttonLabel(Id::Nr), QStringLiteral("NR2"));
        f.dispatcher->apply(&item, kSliceC);
        QCOMPARE(item.buttonLabel(Id::Nr), QStringLiteral("NR"));
        QCOMPARE(item.buttonLabel(Id::LockA), QStringLiteral("Lock"));
    }

    // Acceptance: NR3 refused changes nothing and shows the slice's
    // nrSelectionRefused reason.
    void aRefusedNr3ChangesNothingAndSaysWhy()
    {
        Fixture f;
        const QString why = QStringLiteral("NR3 needs its noise model on this Core.");
        f.a->setNrSelectionApplier([why](NrSlot slot, QString* reason) {
            if (slot == NrSlot::NR3) {
                *reason = why;
                return false;
            }
            return true;
        });
        f.a->setActiveNr(NrSlot::NR2);
        QSignalSpy changed(f.a, &SliceModel::activeNrChanged);
        QCOMPARE(f.dispatcher->click(Id::Nr3, kSliceA), why);
        QCOMPARE(f.a->activeNr(), NrSlot::NR2);
        // The NR button's cycle from NR2 is refused the same way.
        QCOMPARE(f.dispatcher->click(Id::Nr, kSliceA), why);
        QCOMPARE(f.a->activeNr(), NrSlot::NR2);
        QCOMPARE(changed.count(), 0);
        QVERIFY(!f.dispatcher->stateOf(Id::Nr3, kSliceA).on);
        QCOMPARE(f.b->activeNr(), NrSlot::Off);
        // A refusal once does not stick to the next click.
        QCOMPARE(f.dispatcher->click(Id::Nr4, kSliceA), QString());
        QCOMPARE(f.a->activeNr(), NrSlot::NR4);
    }

    // Acceptance: steps clamp at the ends.
    void stepsClampAtTheEnds()
    {
        Fixture f;
        f.a->setAfGain(ControlRanges::kAfGainMax - 2);
        f.dispatcher->click(Id::AfP5, kSliceA);
        QCOMPARE(f.a->afGain(), ControlRanges::kAfGainMax);
        f.dispatcher->click(Id::AfP5, kSliceA);
        QCOMPARE(f.a->afGain(), ControlRanges::kAfGainMax);
        f.a->setAfGain(ControlRanges::kAfGainMin + 3);
        f.dispatcher->click(Id::AfM5, kSliceA);
        QCOMPARE(f.a->afGain(), ControlRanges::kAfGainMin);
        f.dispatcher->click(Id::AfM5, kSliceA);
        QCOMPARE(f.a->afGain(), ControlRanges::kAfGainMin);

        f.a->setAudioPan(0.96);
        f.dispatcher->click(Id::BalP5, kSliceA);
        QCOMPARE(f.a->audioPan(), 1.0);
        f.dispatcher->click(Id::BalP5, kSliceA);
        QCOMPARE(f.a->audioPan(), 1.0);
        f.a->setAudioPan(-0.96);
        f.dispatcher->click(Id::BalM5, kSliceA);
        QCOMPARE(f.a->audioPan(), -1.0);
        f.dispatcher->click(Id::BalM5, kSliceA);
        QCOMPARE(f.a->audioPan(), -1.0);

        f.a->setAgcThreshold(ControlRanges::kAgcThresholdMaxDb - 1);
        f.dispatcher->click(Id::AgcP5, kSliceA);
        QCOMPARE(f.a->agcThreshold(), ControlRanges::kAgcThresholdMaxDb);
        f.dispatcher->click(Id::AgcP5, kSliceA);
        QCOMPARE(f.a->agcThreshold(), ControlRanges::kAgcThresholdMaxDb);
        f.a->setAgcThreshold(ControlRanges::kAgcThresholdMinDb + 4);
        f.dispatcher->click(Id::AgcM5, kSliceA);
        QCOMPARE(f.a->agcThreshold(), ControlRanges::kAgcThresholdMinDb);
        f.dispatcher->click(Id::AgcM5, kSliceA);
        QCOMPARE(f.a->agcThreshold(), ControlRanges::kAgcThresholdMinDb);
    }

    // Acceptance: a container on a slice that is not open shows every row
    // unavailable with noSliceReason, and a click changes nothing.
    void aSliceNotOpenShowsEveryRowUnavailable()
    {
        Fixture f;
        const SliceSnapshot a = SliceSnapshot::of(*f.a);
        const SliceSnapshot b = SliceSnapshot::of(*f.b);
        const int notches = f.model.notchModel()->notches().size();
        const QString reason = ContainerButtonDispatcher::noSliceReason(kSliceC);
        for (Id id : kReceive) {
            const auto st = f.dispatcher->stateOf(id, kSliceC);
            QVERIFY2(!st.available, qPrintable(QString::number(int(id))));
            QCOMPARE(st.reason, reason);
            QVERIFY(!st.on);
            QCOMPARE(f.dispatcher->click(id, kSliceC), reason);
        }
        QVERIFY(SliceSnapshot::of(*f.a) == a);
        QVERIFY(SliceSnapshot::of(*f.b) == b);
        QCOMPARE(f.model.notchModel()->notches().size(), notches);

        OtherButtonItem item;
        f.dispatcher->apply(&item, kSliceC);
        for (Id id : kReceive) {
            QVERIFY(!item.isButtonAvailable(id));
        }
    }

    // Acceptance: Mute All skips slices this window may not change.
    void muteAllSkipsSlicesThisWindowMayNotChange()
    {
        {
            Fixture f;
            QVERIFY(!f.dispatcher->stateOf(Id::MuteAll, kSliceA).on);
            f.dispatcher->click(Id::MuteAll, kSliceA);
            QVERIFY(f.a->muted());
            QVERIFY(f.b->muted());
            QVERIFY(f.dispatcher->stateOf(Id::MuteAll, kSliceA).on);
            // One unmuted: not all muted, so the next click mutes all.
            f.b->setMuted(false);
            QVERIFY(!f.dispatcher->stateOf(Id::MuteAll, kSliceA).on);
            f.dispatcher->click(Id::MuteAll, kSliceA);
            QVERIFY(f.a->muted() && f.b->muted());
            f.dispatcher->click(Id::MuteAll, kSliceA);
            QVERIFY(!f.a->muted() && !f.b->muted());
        }
        {
            Fixture f;
            f.refused.insert(1);  // slice B is another device's
            f.dispatcher->click(Id::MuteAll, kSliceA);
            QVERIFY(f.a->muted());
            QVERIFY(!f.b->muted());
            // Lit: every slice this window may change is muted.
            QVERIFY(f.dispatcher->stateOf(Id::MuteAll, kSliceA).on);
            f.dispatcher->click(Id::MuteAll, kSliceA);
            QVERIFY(!f.a->muted());
            QVERIFY(!f.b->muted());
        }
    }

    // MNF+ adds a notch on the container's slice (slice A's frequency, not
    // the active slice B's).
    void mnfPlusAddsANotchOnTheContainersSlice()
    {
        Fixture f;
        NotchModel* notches = f.model.notchModel();
        QVERIFY(notches);
        const int before = notches->notches().size();
        QCOMPARE(f.dispatcher->click(Id::MnfPlus, kSliceA), QString());
        QCOMPARE(notches->notches().size(), before + 1);
        const double centre = notches->notches().last().centerHz;
        QCOMPARE(centre, RadioModel::tnfCentreHzFor(*f.a));
        QVERIFY(qAbs(centre - f.a->frequency()) < 10000.0);
        QVERIFY(!f.dispatcher->stateOf(Id::MnfPlus, kSliceA).on);
    }

    // MNF+ is refused while transmitting, as the filter display's +MNF is.
    void mnfPlusIsRefusedWhileTransmitting()
    {
        // A remote window holds the Core's transmit state as observed; a
        // local model's only comes from keying, which this test never does.
        RadioModel remote(RadioModel::Role::Remote);
        QVERIFY(remote.addSliceWithStationId(0, QString()) >= 0);
        SliceModel* slice = remote.sliceById(0);
        QVERIFY(slice);
        QVERIFY(remote.notchModel());
        ContainerButtonDispatcher::Hooks hooks;
        ContainerButtonDispatcher dispatcher(&remote, std::move(hooks));
        QVERIFY(dispatcher.stateOf(Id::MnfPlus, kSliceA).available);
        QVERIFY(remote.applyMirroredValue("transmitting", true).isEmpty());
        QVERIFY(remote.isTransmitting());
        const auto st = dispatcher.stateOf(Id::MnfPlus, kSliceA);
        QVERIFY(!st.available);
        QCOMPARE(st.reason, ContainerButtonDispatcher::notchWhileTransmittingReason());
        const int before = remote.notchModel()->notches().size();
        QCOMPARE(dispatcher.click(Id::MnfPlus, kSliceA),
                 ContainerButtonDispatcher::notchWhileTransmittingReason());
        QCOMPARE(remote.notchModel()->notches().size(), before);
    }

    // None keys: every receive button clicked, several times, leaves the
    // radio on receive.
    void noneKeys()
    {
        Fixture f;
        QSignalSpy transmitting(&f.model, &RadioModel::transmittingChanged);
        for (int round = 0; round < 3; ++round) {
            for (Id id : kReceive) { f.dispatcher->click(id, kSliceA); }
        }
        QVERIFY(!f.model.isTransmitting());
        QVERIFY(!f.model.transmitModel().isTune());
        QVERIFY(!f.model.moxController() || !f.model.moxController()->isMox());
        QCOMPARE(transmitting.count(), 0);
        QVERIFY(!f.model.isConnected());
    }

    // Each receive button is offered and built, so the editor lists it
    // enabled with no change to the editor.
    void everyReceiveButtonIsOfferedAndItsEditorRowEnabled()
    {
        OtherButtonItem item;
        OtherButtonItemEditor editor;
        editor.setItem(&item);
        for (Id id : kReceive) {
            QVERIFY2(item.isOffered(id), qPrintable(QString::number(int(id))));
            QVERIFY(!OtherButtonItem::unbuiltFeatureFor(id).has_value());
            // Hidden on a new grid, so an earlier layout looks the same.
            QVERIFY(!item.isButtonVisible(id));
            auto* check = editor.findChild<QCheckBox*>(
                QStringLiteral("otherVisible_%1").arg(int(id)));
            QVERIFY2(check, qPrintable(QString::number(int(id))));
            QVERIFY(check->isEnabled());
            QVERIFY(!check->toolTip().contains(UnbuiltFeatures::notBuiltReason()));
            check->setChecked(true);
            QVERIFY(item.isButtonShown(id));
        }
        const QStringList captions = {
            "NR", "NR1", "NR2", "NR3", "NR4", "NB", "NB1", "NB2", "MNF+", "RIT", "RIT0",
            "XIT", "XIT0", "Lock", "TS+", "TS-", "Mute All", "Voice SQL", "AF+", "AF-",
            "BAL+", "BAL-", "FIX", "LONG", "SLOW", "MED", "FAST", "CUST", "AUTO", "AGC+",
            "AGC-"};
        for (int i = 0; i < kReceive.size(); ++i) {
            QCOMPARE(item.buttonLabel(kReceive[i]), captions[i]);
        }
    }

    // A settings preview is inert: its click never reaches the window's
    // dispatcher, while the live grid's click does.
    void aPreviewClickNeverReachesTheDispatcher()
    {
        Fixture f;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings{directory.filePath(QStringLiteral("actions.xml"))};
        ContainerWorkspaceStore store{settings};
        ContainerContentRegistry registry;
        QWidget dock;
        QSplitter splitter;
        ContainerManager manager{&dock, &splitter};
        manager.setWorkspaceAdapter(&store, &registry);
        MeterPoller poller;
        manager.setPreviewPoller(&poller);
        dock.resize(1200, 900);

        OtherButtonItem shape;
        shape.setOnlyVisible(Id::Nr);
        ContentEntry entry = registry.makeEntry(QStringLiteral("OTHERBTNS"));
        entry.id = QStringLiteral("receive-buttons");
        entry.config[QStringLiteral("legacyRecord")] = shape.serialize();
        ContainerDocument container;
        container.id = QStringLiteral("grid");
        container.name = QStringLiteral("grid");
        container.layout = ContentLayout::VerticalStack;
        container.geometry = QRect(10, 20, kSize, kSize);
        container.contents.append(entry);
        WorkspaceDocument document;
        document.containers.append(container);
        document.mainContainerId = container.id;
        QCOMPARE(manager.commitWorkspace(document, 0).status, CommitStatus::Saved);

        // MainWindow's wiring: a container's click goes to the dispatcher.
        int dispatched = 0;
        for (ContainerWidget* c : manager.allContainers()) {
            connect(c, &ContainerWidget::otherButtonClicked, this, [&f, &dispatched](int id) {
                ++dispatched;
                f.dispatcher->click(static_cast<Id>(id), kSliceA);
            });
        }
        ContainerContentHost* host = manager.contentHost(QStringLiteral("grid"));
        QVERIFY(host);
        QVERIFY(QTest::qWaitFor([&] { return firstGrid(host->meterSurfaces()) != nullptr; }, 5000));
        OtherButtonItem* live = firstGrid(host->meterSurfaces());
        QVERIFY(live->isButtonShown(Id::Nr));
        // MainWindow::watchContainerItems wires each live item through its
        // container.
        manager.container(QStringLiteral("grid"))->wireInteractiveItem(live);

        ContainerSettingsDialog dialog(manager.container(QStringLiteral("grid")), nullptr, &manager);
        dialog.refreshDraftView();
        auto* preview = dialog.findChild<ContainerPreviewWidget*>();
        QVERIFY(preview);
        OtherButtonItem* copy = firstGrid(preview->findChildren<MeterWidget*>());
        QVERIFY(copy);
        QVERIFY(copy != live);
        QVERIFY(copy->isButtonShown(Id::Nr));

        // The preview surface takes no mouse input at all, and even a click
        // the copy itself raises goes nowhere.
        for (MeterWidget* meter : preview->findChildren<MeterWidget*>()) {
            QVERIFY(meter->testAttribute(Qt::WA_TransparentForMouseEvents));
        }
        emit copy->otherButtonClicked(int(Id::Nr));
        for (Id id : kReceive) { emit copy->otherButtonClicked(int(id)); }
        QCOMPARE(dispatched, 0);
        QCOMPARE(f.a->activeNr(), NrSlot::Off);

        // The live grid's click reaches it (the wiring above is real).
        emit live->otherButtonClicked(int(Id::Nr));
        QCOMPARE(dispatched, 1);
        QCOMPARE(f.a->activeNr(), NrSlot::NR1);
        dialog.reject();
    }

    // UI evidence: a grid of every receive button, lit from slice A.
    void captureTheReceiveGrid()
    {
        const QString dir = qEnvironmentVariable("NEREUS_OTHER_BUTTONS_CAPTURE_DIR");
        if (dir.isEmpty()) { QSKIP("Set NEREUS_OTHER_BUTTONS_CAPTURE_DIR to capture the grid."); }
        Fixture f;
        f.a->setActiveNr(NrSlot::NR2);
        f.a->setNbMode(NbMode::NB2);
        f.a->setRitEnabled(true);
        f.a->setSsqlEnabled(true);
        f.a->setAgcMode(AGCMode::Med);
        OtherButtonItem item;
        item.setVisibleGroups(QVector<qint32>(OtherButtonItem::kGroupCount, 0));
        for (Id id : kReceive) { item.setButtonVisible(id, true); }
        item.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        f.dispatcher->apply(&item, kSliceA);
        QImage image(kSize, kSize, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(0x0f, 0x0f, 0x1a));
        QPainter p(&image);
        item.paint(p, kSize, kSize);
        p.end();
        QVERIFY(QDir().mkpath(dir));
        QVERIFY(image.save(QDir(dir).filePath(QStringLiteral("receive-buttons-grid.png"))));
    }
};

QTEST_MAIN(TstOtherButtonActions)
#include "tst_other_button_actions.moc"
