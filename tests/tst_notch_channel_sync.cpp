// =================================================================
// tests/tst_notch_channel_sync.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Thetis and WDSP
// file names appear in comments to document what each push forwards to; no
// upstream logic is ported into this file.
//
// Tunable Notch Filter, Task 4: the RadioModel notch fan-out and
// syncNotchesToAllChannels().
//
// Design: docs/architecture/2026-07-28-tunable-notch-filter-design.md
//   section 6.3  fan-out and the openRxChannelPool-tail reconcile
//   section 5.5  restore order (model populated before any channel exists)
//   section 8.1  RadioModel::notchModel() accessor
//   section 11   tst_notch_channel_sync
//
// Uses the WdspEngine NEREUS_BUILD_TESTS friend seam exactly as
// tests/tst_stream_pool_binding.cpp does: priming m_initialized lets
// openRxChannelPool run createRxChannel's real OpenChannel, so every
// RXANBP* wrapper here talks to a genuinely opened WDSP channel. Design
// section 11.1: an unopened channel is not merely inert, it is an
// out-of-bounds read on rxa[] (third_party/wdsp/src/comm.h sizes rxa at
// MAX_CHANNELS = 32, and every RXANBP* entry point dereferences
// rxa[channel] before it range-checks anything).
// =================================================================
#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/RxChannel.h"
#include "core/SampleRateCatalog.h"
#include "core/WdspEngine.h"
#include "core/dsp/Notch.h"
#include "models/NotchModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// Stream geometry shared by every slot, matching
// tests/tst_stream_pool_binding.cpp. 192 kHz gives a +/- 96 kHz window, so
// the 14.0745 / 14.076 MHz slice pair below shares one stream.
constexpr int    kRateHz       = 192000;
constexpr double kSliceAFreqHz = 14074500.0;
constexpr double kSliceBFreqHz = 14076000.0;

} // namespace

class TestNotchChannelSync : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void init()         { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // -- section 8.1: the accessor, alongside spotModel() -----------------
    void radio_model_owns_one_notch_model()
    {
        RadioModel model;
        QVERIFY(model.notchModel() != nullptr);
        // A non-owning view onto a unique_ptr member, not a factory.
        QCOMPARE(model.notchModel(), model.notchModel());
    }

    // -- section 5.5 / 8.1: restoreFromSettings() runs in the ctor, before
    // any channel exists, so the openRxChannelPool-tail reconcile always has
    // the full list to install.
    void notch_model_is_restored_at_construction()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("NotchGlobalEnabled"), QStringLiteral("True"));
        s.setValue(QStringLiteral("NotchCount"),         1);
        s.setValue(QStringLiteral("Notch0Center"),       14074000.0);
        s.setValue(QStringLiteral("Notch0Width"),        250.0);
        s.setValue(QStringLiteral("Notch0Active"),       QStringLiteral("True"));

        RadioModel model;
        const NotchModel* nm = model.notchModel();
        QVERIFY(nm != nullptr);
        QCOMPARE(nm->notches().size(), 1);
        QCOMPARE(nm->notches().at(0).centerHz, 14074000.0);
        QCOMPARE(nm->notches().at(0).widthHz,  250.0);
        QVERIFY(nm->notches().at(0).active);
        QVERIFY(nm->globalEnabled());
    }
};

QTEST_MAIN(TestNotchChannelSync)
#include "tst_notch_channel_sync.moc"
