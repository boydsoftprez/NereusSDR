// no-port-check: NereusSDR-original. CW transmit (R-CW-23, R-CW-24, R-CW-27,
// R-CW-28): the CW receive shift in the composed WDSP shift and the slice's
// demodulated frequency, the passband origin helper, and what a CW pitch
// change moves (filters, shift, APF) and never moves (the dial, the
// carrier). No RF and no hardware: the connection is a never-opened
// P1RadioConnection, so nothing here keys a radio or opens a socket.
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26: CW transmit, created: the CW receive shift and its
//               invariant, passbandOriginHz and cwShiftHz, and the pitch
//               follow. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include <cmath>

#include "core/AppSettings.h"
#include "core/CwTypes.h"
#include "core/DspControlThread.h"
#include "core/P1RadioConnection.h"
#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspEngine.h"
#include "models/FilterPresetStore.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// 192 kHz gives a +/- 96 kHz stream window, so the two slices share a
// stream: A holds its centre and B sits 10 kHz above it, a non-zero stream
// term, so a CW term that replaced the stream term instead of adding to it
// would show.
constexpr int    kRateHz      = 192000;
constexpr double kCentreHz    = 14020000.0;
constexpr double kDialHz      = 14030000.0;
constexpr double kStreamTerm  = kDialHz - kCentreHz;

bool laneIdle(RadioModel& model)
{
    return model.receiveLane() == nullptr || model.receiveLane()->waitIdleForTest(600000);
}

struct DetachConnection {
    RadioModel* model{nullptr};
    ~DetachConnection() { if (model) { model->injectConnectionForTest(nullptr); } }
};

// A model with two slices on one stream and a never-opened connection, so
// wireSliceSignals' mode and RIT re-pushes exist.
struct Rig {
    RadioModel model;
    P1RadioConnection conn;
    DetachConnection detach{&model};
    SliceModel* a{nullptr};
    SliceModel* b{nullptr};
    RxChannel* chA{nullptr};
    RxChannel* chB{nullptr};

    bool build()
    {
        model.injectConnectionForTest(&conn);
        // The caller primes model.wdspEngine()->m_initialized first: the
        // friendship is the test class's (NEREUS_BUILD_TESTS).
        model.configureStreamPool(2, 2, kRateHz);
        model.openRxChannelPool(2, bufferSizeForRate(kRateHz), kRateHz);
        a = model.sliceById(model.addSlice());
        if (!a) { return false; }
        a->setFrequency(kCentreHz);
        a->setDspMode(DSPMode::USB);
        b = model.sliceById(model.addSlice());
        if (!b) { return false; }
        b->setFrequency(kDialHz);
        chA = model.wdspEngine()->rxChannel(a->sliceIndex());
        chB = model.wdspEngine()->rxChannel(b->sliceIndex());
        return chA != nullptr && chB != nullptr;
    }
};

} // namespace

class TstCwPitchFollow : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // R-CW-23: the receiver sits one pitch off the dial in CW, the TX
    // carrier stays on the dial, and the 4.1 invariant (tunefreq + shift ==
    // the demodulated RF) holds in both halves.
    void cwShiftsTheReceiverAndNotTheCarrier_data()
    {
        QTest::addColumn<int>("mode");
        QTest::addColumn<double>("cwTerm");
        QTest::newRow("CWU") << static_cast<int>(DSPMode::CWU) << -600.0;
        QTest::newRow("CWL") << static_cast<int>(DSPMode::CWL) << 600.0;
        QTest::newRow("USB") << static_cast<int>(DSPMode::USB) << 0.0;
        QTest::newRow("LSB") << static_cast<int>(DSPMode::LSB) << 0.0;
        QTest::newRow("AM") << static_cast<int>(DSPMode::AM) << 0.0;
    }
    void cwShiftsTheReceiverAndNotTheCarrier()
    {
        QFETCH(int, mode);
        QFETCH(double, cwTerm);
        Rig rig;
        rig.model.wdspEngine()->m_initialized = true;   // friend access
        QVERIFY(rig.build());
        rig.b->setDspMode(static_cast<DSPMode>(mode));
        QVERIFY(laneIdle(rig.model));

        QCOMPARE(rig.b->frequency(), kDialHz);
        QCOMPARE(rig.b->shiftOffsetHz(), kStreamTerm);       // the stream term alone
        QCOMPARE(rig.chB->notchTuneFrequencyHz(), kCentreHz);
        QCOMPARE(rig.chB->shiftOffsetHz(), kStreamTerm + cwTerm);
        QCOMPARE(rig.chB->notchShiftHz(), kStreamTerm + cwTerm);
        QCOMPARE(rig.b->cwShiftHz(), cwTerm);
        QCOMPARE(rig.b->demodulatedRxFrequency(), kDialHz + cwTerm);
        QCOMPARE(rig.chB->notchTuneFrequencyHz() + rig.chB->shiftOffsetHz(),
                 rig.b->demodulatedRxFrequency());

        // TX is the dial plus XIT, never the pitch.
        QCOMPARE(rig.model.txFrequencyForSliceForTest(rig.b), quint64(14030000));
        rig.b->setXitHz(50);
        rig.b->setXitEnabled(true);
        QCOMPARE(rig.model.txFrequencyForSliceForTest(rig.b), quint64(14030050));
        // XIT is not a receive term.
        QCOMPARE(rig.chB->shiftOffsetHz(), kStreamTerm + cwTerm);
    }

    // CWU at 14.030000 MHz, pitch 600, RIT off: 14.029400; CWL 14.030600.
    // RIT adds on top of the CW term.
    void demodulatedFrequencyCarriesTheCwTerm()
    {
        SliceModel s;
        s.setFrequency(kDialHz);
        s.setDspMode(DSPMode::CWU);
        QCOMPARE(s.demodulatedRxFrequency(), 14029400.0);
        s.setDspMode(DSPMode::CWL);
        QCOMPARE(s.demodulatedRxFrequency(), 14030600.0);
        s.setRitHz(100);
        s.setRitEnabled(true);
        QCOMPARE(s.demodulatedRxFrequency(), 14030700.0);
        s.setDspMode(DSPMode::USB);
        QCOMPARE(s.demodulatedRxFrequency(), 14030100.0);
    }

    // R-CW-24 (D5): passbandOriginHz = dial - pitch in CWU, + pitch in CWL,
    // the dial otherwise; cwShiftHz is the signed term alone; the pitch is
    // the station key.
    void passbandOriginFollowsModeAndPitch()
    {
        SliceModel s;
        s.setFrequency(kDialHz);
        s.setDspMode(DSPMode::CWU);
        QCOMPARE(s.cwShiftHz(), -600.0);
        QCOMPARE(s.passbandOriginHz(), 14029400.0);
        s.setDspMode(DSPMode::CWL);
        QCOMPARE(s.cwShiftHz(), 600.0);
        QCOMPARE(s.passbandOriginHz(), 14030600.0);
        // RIT is not a passband origin term.
        s.setRitHz(100);
        s.setRitEnabled(true);
        QCOMPARE(s.passbandOriginHz(), 14030600.0);
        for (DSPMode m : {DSPMode::USB, DSPMode::LSB, DSPMode::DIGU, DSPMode::DIGL,
                          DSPMode::AM, DSPMode::FM}) {
            s.setDspMode(m);
            QCOMPARE(s.cwShiftHz(), 0.0);
            QCOMPARE(s.passbandOriginHz(), kDialHz);
        }
        AppSettings::instance().setValue(QStringLiteral("CWPitch"), QStringLiteral("700"));
        s.setDspMode(DSPMode::CWU);
        QCOMPARE(s.passbandOriginHz(), 14029300.0);
    }

    // D8: a pitch change moves a CW filter by the change keeping its width;
    // the image-limit slide from the Thetis CWPitch setter still applies.
    void shiftCwFilterForPitchKeepsWidthAndSlides()
    {
        int low = 300;
        int high = 700;
        SliceModel::shiftCwFilterForPitch(low, high, DSPMode::CWU, 600, 700);
        QCOMPARE(low, 400);
        QCOMPARE(high, 800);
        low = -700;
        high = -300;
        SliceModel::shiftCwFilterForPitch(low, high, DSPMode::CWL, 600, 700);
        QCOMPARE(low, -800);
        QCOMPARE(high, -400);
        // Down to 200 Hz: CWU 100..500 would cross zero, so it slides to 0..400.
        low = 100;
        high = 500;
        SliceModel::shiftCwFilterForPitch(low, high, DSPMode::CWU, 600, 200);
        QCOMPARE(low, 0);
        QCOMPARE(high, 400);
        low = -500;
        high = -100;
        SliceModel::shiftCwFilterForPitch(low, high, DSPMode::CWL, 600, 200);
        QCOMPARE(low, -400);
        QCOMPARE(high, 0);
        // Any other mode is left alone.
        low = 100;
        high = 3000;
        SliceModel::shiftCwFilterForPitch(low, high, DSPMode::USB, 600, 700);
        QCOMPARE(low, 100);
        QCOMPARE(high, 3000);
    }

    // R-CW-27, R-CW-28, D7: 600 to 700 re-pushes the CW slice's shift and
    // every slice's APF centre, moves the CW filters, and leaves the dial,
    // the carrier and the voice slice alone.
    void aPitchChangeMovesFiltersShiftAndApfButNotTheDial()
    {
        auto& settings = AppSettings::instance();
        // Stored per-band CW filters (another slice index, so the rig's
        // own saves cannot touch them), a legacy band pair in CWL, and a
        // voice one that must not move.
        settings.setValue(QStringLiteral("Slice7/Band20m/ModeCWU/FilterLow"), 450);
        settings.setValue(QStringLiteral("Slice7/Band20m/ModeCWU/FilterHigh"), 750);
        settings.setValue(QStringLiteral("Slice7/Band40m/FilterLow"), -750);
        settings.setValue(QStringLiteral("Slice7/Band40m/FilterHigh"), -450);
        settings.setValue(QStringLiteral("Slice7/Band40m/DspMode"),
                          static_cast<int>(DSPMode::CWL));
        settings.setValue(QStringLiteral("Slice7/Band20m/ModeUSB/FilterLow"), 450);
        settings.setValue(QStringLiteral("Slice7/Band20m/ModeUSB/FilterHigh"), 750);
        settings.setValue(QStringLiteral("Slice7/Band80m/FilterLow"), 450);
        settings.setValue(QStringLiteral("Slice7/Band80m/FilterHigh"), 750);
        settings.setValue(QStringLiteral("Slice7/Band80m/DspMode"),
                          static_cast<int>(DSPMode::USB));

        Rig rig;
        rig.model.wdspEngine()->m_initialized = true;   // friend access
        QVERIFY(rig.build());
        FilterPresetStore* store = rig.model.filterPresetStore();
        QVERIFY(store);
        store->setPreset(DSPMode::CWU, 2, FilterPreset{QStringLiteral("Mine"), 300, 700});

        rig.b->setDspMode(DSPMode::CWU);
        rig.b->setFilter(400, 800);
        rig.a->setApfTuneHz(20);
        rig.b->setApfTuneHz(-30);
        QVERIFY(laneIdle(rig.model));
        QCOMPARE(rig.chB->shiftOffsetHz(), kStreamTerm - 600.0);
        QCOMPARE(rig.chA->apfFreqHz(), 620.0);
        QCOMPARE(rig.chB->apfFreqHz(), 570.0);
        const double aShift = rig.chA->shiftOffsetHz();
        const int aLow = rig.a->filterLow();
        const int aHigh = rig.a->filterHigh();

        QSignalSpy bFilter(rig.b, &SliceModel::filterChanged);
        QSignalSpy aFilter(rig.a, &SliceModel::filterChanged);
        QSignalSpy bFreq(rig.b, &SliceModel::frequencyChanged);
        QSignalSpy pitch(&rig.model, &RadioModel::cwPitchChanged);
        QSignalSpy cwuPresets(store, &FilterPresetStore::presetsChanged);

        rig.model.setCwPitchHz(700);
        QVERIFY(laneIdle(rig.model));
        QCOMPARE(pitch.count(), 1);

        // The shift moved by the pitch change; the stream did not.
        QCOMPARE(rig.chB->notchTuneFrequencyHz(), kCentreHz);
        QCOMPARE(rig.chB->shiftOffsetHz(), kStreamTerm - 700.0);
        QCOMPARE(rig.chB->notchShiftHz(), kStreamTerm - 700.0);
        QCOMPARE(rig.chB->notchTuneFrequencyHz() + rig.chB->shiftOffsetHz(),
                 rig.b->demodulatedRxFrequency());
        QCOMPARE(rig.b->demodulatedRxFrequency(), 14029300.0);
        QCOMPARE(rig.b->shiftOffsetHz(), kStreamTerm);

        // No dial or carrier change.
        QCOMPARE(bFreq.count(), 0);
        QCOMPARE(rig.b->frequency(), kDialHz);
        QCOMPARE(rig.model.txFrequencyForSliceForTest(rig.b), quint64(14030000));

        // The APF centre is the pitch plus each slice's tune offset.
        QCOMPARE(rig.chA->apfFreqHz(), 720.0);
        QCOMPARE(rig.chB->apfFreqHz(), 670.0);

        // The CW slice's current filter is re-applied once, moved.
        QCOMPARE(bFilter.count(), 1);
        QCOMPARE(rig.b->filterLow(), 500);
        QCOMPARE(rig.b->filterHigh(), 900);

        // Presets: the default F5 follows the pitch, the overridden slot
        // moves keeping its width, and both CW modes are announced.
        const QList<FilterPreset> cwu = store->presetsForMode(DSPMode::CWU);
        QCOMPARE(cwu.at(4).low, 500);
        QCOMPARE(cwu.at(4).high, 900);
        QCOMPARE(cwu.at(2).name, QStringLiteral("Mine"));
        QCOMPARE(cwu.at(2).low, 400);
        QCOMPARE(cwu.at(2).high, 800);
        QList<DSPMode> announced;
        for (const QList<QVariant>& args : std::as_const(cwuPresets)) {
            announced.append(args.at(0).value<DSPMode>());
        }
        QVERIFY(announced.contains(DSPMode::CWU));
        QVERIFY(announced.contains(DSPMode::CWL));

        // Stored per-band CW filters moved keeping their width.
        QCOMPARE(settings.value(QStringLiteral("Slice7/Band20m/ModeCWU/FilterLow")).toInt(), 550);
        QCOMPARE(settings.value(QStringLiteral("Slice7/Band20m/ModeCWU/FilterHigh")).toInt(), 850);
        QCOMPARE(settings.value(QStringLiteral("Slice7/Band40m/FilterLow")).toInt(), -850);
        QCOMPARE(settings.value(QStringLiteral("Slice7/Band40m/FilterHigh")).toInt(), -550);

        // The voice slice and the voice filters are untouched.
        QCOMPARE(aFilter.count(), 0);
        QCOMPARE(rig.a->filterLow(), aLow);
        QCOMPARE(rig.a->filterHigh(), aHigh);
        QCOMPARE(rig.chA->shiftOffsetHz(), aShift);
        QCOMPARE(settings.value(QStringLiteral("Slice7/Band20m/ModeUSB/FilterLow")).toInt(), 450);
        QCOMPARE(settings.value(QStringLiteral("Slice7/Band20m/ModeUSB/FilterHigh")).toInt(), 750);
        QCOMPARE(settings.value(QStringLiteral("Slice7/Band80m/FilterLow")).toInt(), 450);
        QCOMPARE(settings.value(QStringLiteral("Slice7/Band80m/FilterHigh")).toInt(), 750);

        // The same pitch again does nothing.
        rig.model.setCwPitchHz(700);
        QCOMPARE(pitch.count(), 1);
        QCOMPARE(bFilter.count(), 1);
        QCOMPARE(store->presetsForMode(DSPMode::CWU).at(2).low, 400);
    }

    // A remote window's filters are the Core's: its pitch follow moves
    // nothing in settings (the Core does, and the mirror brings it back),
    // so a pitch change cannot move them twice.
    void aRemoteWindowLeavesTheCoresFiltersToTheCore()
    {
        auto& settings = AppSettings::instance();
        settings.setValue(QStringLiteral("Slice7/Band20m/ModeCWU/FilterLow"), 450);
        settings.setValue(QStringLiteral("Slice7/Band20m/ModeCWU/FilterHigh"), 750);
        RadioModel window{RadioModel::Role::Remote};
        FilterPresetStore* store = window.filterPresetStore();
        QVERIFY(store);
        store->setPreset(DSPMode::CWU, 2, FilterPreset{QStringLiteral("Mine"), 300, 700});

        settings.setValue(QStringLiteral("CWPitch"), QStringLiteral("700"));
        window.notifyStationCwPitchMaybeChanged();
        QCOMPARE(window.cwPitchHz(), 700);

        QCOMPARE(settings.value(QStringLiteral("Slice7/Band20m/ModeCWU/FilterLow")).toInt(), 450);
        QCOMPARE(settings.value(QStringLiteral("Slice7/Band20m/ModeCWU/FilterHigh")).toInt(), 750);
        QCOMPARE(store->presetsForMode(DSPMode::CWU).at(2).low, 300);
        // The defaults are built from the Core's pitch.
        QCOMPARE(store->presetsForMode(DSPMode::CWU).at(4).low, 500);
    }
};

QTEST_MAIN(TstCwPitchFollow)
#include "tst_cw_pitch_follow.moc"
