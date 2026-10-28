// no-port-check: NereusSDR-original tests for the VHF S9 rule per slice.
// Modification history (NereusSDR):
//   2026-10-25 - VHF S9 rule per slice: the 30 MHz boundary, the shared
//                 helper, every S-unit item, two slices in one container
//                 and a Follow selected RX entry. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QFile>
#include <QJsonObject>

#include "core/AppSettings.h"
#include "core/BuildIdentity.h"
#include "core/RadioDiscovery.h"
#include "gui/GuiSessionCoordinator.h"
#include "gui/MainWindow.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/AboveS9Frequency.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/SignalTextItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/presets/CompositePresetItem.h"
#include "gui/meters/presets/SignalTextPresetItem.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// A signal bar in S units that takes each reading at once.
BarItem* makeSBar(int binding = MeterBinding::SignalPeak)
{
    auto* bar = new BarItem();
    bar->setBindingId(binding);
    bar->setRange(-140, 0);
    bar->setAttackRatio(1);
    bar->setDecayRatio(1);
    bar->setUnitMode(MeterItem::MeterUnit::S);
    return bar;
}

// Feeds a smoothed item the same reading until it settles.
void settle(MeterItem& item, double dbm)
{
    for (int i = 0; i < 200; ++i) { item.setValue(dbm); }
}

SliceModel* ensureSlices(RadioModel* model, int count)
{
    if (model->activeSlice() == nullptr) {
        model->addSlice();
        model->setActiveSlice(0);
    }
    while (model->slices().size() < count) { model->addSlice(); }
    return model->activeSlice();
}

} // namespace

class TstMeterVhfS9 : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        AppSettings::setProfileOverride(QStringLiteral("meter-vhf-s9-%1")
                                            .arg(QCoreApplication::applicationPid()));
    }

    void init()
    {
        AppSettings::instance().clear();
        AppSettings::instance().setValue(QStringLiteral("audio/FirstRunComplete"),
                                         QStringLiteral("True"));
        RadioDiscovery::clearHoldOffForTest();
        RadioDiscovery discovery;
        discovery.holdOffScans(std::chrono::minutes{5});
        BuildIdentity::setBuildTag(QString());
    }

    void cleanup()
    {
        RadioDiscovery::clearHoldOffForTest();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
    }

    void boundaryIsAtOrAbove30MHz()
    {
        QCOMPARE(kS9FrequencyHz, 30.0e6);
        QCOMPARE(kAboveS9OffsetDb, 20.0);
        QVERIFY(isAboveS9Frequency(30.0e6));
        QVERIFY(!isAboveS9Frequency(29999999.0));
        QVERIFY(isAboveS9Frequency(50.0e6));
        QVERIFY(!isAboveS9Frequency(14.0e6));
        static_assert(isAboveS9Frequency(30.0e6), "constexpr at the boundary");
    }

    void onlySignalStrengthTakesTheOffset()
    {
        QCOMPARE(aboveS9Adjusted(MeterBinding::SignalPeak, -93, true), -73.0);
        QCOMPARE(aboveS9Adjusted(MeterBinding::SignalAvg, -93, true), -73.0);
        QCOMPARE(aboveS9Adjusted(MeterBinding::SignalPeak, -93, false), -93.0);
        QCOMPARE(aboveS9Adjusted(MeterBinding::TxSwr, -93, true), -93.0);
        QCOMPARE(aboveS9Adjusted(MeterBinding::AdcPeak, -93, true), -93.0);
    }

    void formatValueTakesTheS9Level()
    {
        using U = MeterItem::MeterUnit;
        QCOMPARE(MeterItem::kS9DbmBelowS9Frequency, -73.0f);
        QCOMPARE(MeterItem::kS9DbmAboveS9Frequency, -93.0f);
        QCOMPARE(MeterItem::formatValue(-73.0f, U::S, true), QStringLiteral("S9"));
        QCOMPARE(MeterItem::formatValue(-73.0f, U::S, true, false), QStringLiteral("S9"));
        QCOMPARE(MeterItem::formatValue(-93.0f, U::S, true, true), QStringLiteral("S9"));
        QCOMPARE(MeterItem::formatValue(-73.0f, U::S, true, true), QStringLiteral("S9+18"));
        QCOMPARE(MeterItem::formatValue(-99.0f, U::S, true, true), QStringLiteral("S8"));
        // dBm and uV are not S units.
        QCOMPARE(MeterItem::formatValue(-93.0f, U::dBm, true, true), QStringLiteral("-93.0"));
        QCOMPARE(MeterItem::formatValue(-93.0f, U::uV, true, true),
                 MeterItem::formatValue(-93.0f, U::uV, true, false));
    }

    void everyItemUsesTheFlag()
    {
        std::unique_ptr<BarItem> bar(makeSBar());
        QVERIFY(!bar->aboveS9Frequency());
        bar->setValue(-73);
        QCOMPARE(bar->valueText(), QStringLiteral("S9"));
        bar->setAboveS9Frequency(true);
        QVERIFY(bar->aboveS9Frequency());
        bar->setValue(-93);
        QCOMPARE(bar->valueText(), QStringLiteral("S9"));
        QCOMPARE(bar->peakValueText().left(2), QStringLiteral("S9"));

        // A calibrated S-meter bar: S9 sits at -73 dBm on HF, -93 dBm on VHF.
        std::unique_ptr<BarItem> cal(makeSBar());
        cal->addScaleCalibration(-133.0, 0.0f);
        cal->addScaleCalibration(-73.0, 0.5f);
        cal->addScaleCalibration(-13.0, 0.99f);
        QCOMPARE(cal->valueToNormalizedX(-73.0), 0.5f);
        cal->setAboveS9Frequency(true);
        QCOMPARE(cal->valueToNormalizedX(-93.0), 0.5f);
        // Not a signal-strength binding: no offset.
        cal->setBindingId(MeterBinding::AdcPeak);
        QCOMPARE(cal->valueToNormalizedX(-73.0), 0.5f);

        TextItem text;
        text.setBindingId(MeterBinding::SignalAvg);
        text.setSuffix(QStringLiteral(" dBm"));
        text.setUnitMode(MeterItem::MeterUnit::S);
        text.setValue(-93);
        QCOMPARE(text.displayText(), QStringLiteral("S6"));
        text.setAboveS9Frequency(true);
        QCOMPARE(text.displayText(), QStringLiteral("S9"));

        NeedleItem needle;
        needle.setBindingId(MeterBinding::SignalPeak);
        needle.setUnitMode(MeterItem::MeterUnit::S);
        settle(needle, -101);
        QCOMPARE(needle.sUnitsReadout(), QStringLiteral("S4"));
        needle.setAboveS9Frequency(true);
        QCOMPARE(needle.sUnitsReadout(), QStringLiteral("S8"));
        settle(needle, -93);
        QCOMPARE(needle.valueReadout(), QStringLiteral("S9"));

        SignalTextItem signal;
        signal.setBindingId(MeterBinding::SignalPeak);
        signal.setUnitMode(MeterItem::MeterUnit::S);
        settle(signal, -62.5);
        QCOMPARE(signal.valueText(), QStringLiteral("S9+10"));
        signal.setAboveS9Frequency(true);
        QCOMPARE(signal.valueText(), QStringLiteral("S9+30"));
    }

    void presetsOverrideAndKeepTheirOwnRule()
    {
        BarPresetItem bar;
        MeterItem* base = &bar;
        base->setAboveS9Frequency(true);
        QVERIFY(bar.aboveS9Frequency());
        QVERIFY(bar.takeStaticPresentationChange());

        SignalTextPresetItem face;
        face.setUnitMode(MeterItem::MeterUnit::S);
        QCOMPARE(face.signalReadout(-73), QStringLiteral("S9"));
        static_cast<MeterItem&>(face).setAboveS9Frequency(true);
        QVERIFY(face.aboveS9Frequency());
        QCOMPARE(face.signalReadout(-93), QStringLiteral("S9"));
    }

    // A BAR in S units on a slice at exactly 30.000000 MHz reads S9 at
    // -93 dBm; at 29.999999 MHz it reads S9 at -73 dBm. Two entries in one
    // container, on Slice A and Slice B, each take their own slice's scale.
    void eachEntryTakesItsOwnSlicesScale()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MainWindow* window = sessions.window();
        RadioModel* model = window->radioModel();
        SliceModel* a = ensureSlices(model, 2);
        QVERIFY(a != nullptr);
        SliceModel* b = model->slices().value(1);
        QVERIFY(b && b != a);
        a->setFrequency(14.0e6);
        b->setFrequency(50.0e6);

        ContainerManager* cm = model->containerManager();
        ContainerWidget* container = cm->createContainer(1, DockMode::OverlayDocked);
        QVERIFY(container != nullptr);
        auto* meter = new MeterWidget();
        container->setContent(meter);
        BarItem* barA = makeSBar();
        barA->setProperty("containerSourceContext", QJsonObject{{"sliceId", a->sliceIndex()}});
        BarItem* barB = makeSBar();
        barB->setProperty("containerSourceContext", QJsonObject{{"sliceId", b->sliceIndex()}});
        meter->addItem(barA);
        meter->addItem(barB);
        // The window's unit setting reaches new items; read in S units.
        barA->setUnitMode(MeterItem::MeterUnit::S);
        barB->setUnitMode(MeterItem::MeterUnit::S);

        QTRY_VERIFY(barB->aboveS9Frequency());
        QVERIFY(!barA->aboveS9Frequency());
        barA->setValue(-73);
        barB->setValue(-93);
        QCOMPARE(barA->valueText(), QStringLiteral("S9"));
        QCOMPARE(barB->valueText(), QStringLiteral("S9"));

        // Exactly 30 MHz is above; one hertz under is not.
        a->setFrequency(30.0e6);
        QTRY_VERIFY(barA->aboveS9Frequency());
        barA->setValue(-93);
        QCOMPARE(barA->valueText(), QStringLiteral("S9"));
        a->setFrequency(29999999.0);
        QTRY_VERIFY(!barA->aboveS9Frequency());
        barA->setValue(-73);
        QCOMPARE(barA->valueText(), QStringLiteral("S9"));
        QVERIFY(barB->aboveS9Frequency());
        QVERIFY(sessions.replace({}, false));
    }

    // A Follow selected RX entry flips its scale when the selected slice
    // changes band, and when another slice on another band is selected.
    void followSelectedRxFlipsWithTheSelectedSlice()
    {
        GuiSessionCoordinator sessions;
        QVERIFY(sessions.replace({}, false));
        MainWindow* window = sessions.window();
        RadioModel* model = window->radioModel();
        SliceModel* a = ensureSlices(model, 2);
        SliceModel* b = model->slices().value(1);
        QVERIFY(a && b && a != b);
        a->setFrequency(14.0e6);
        b->setFrequency(144.0e6);
        model->setActiveSlice(0);
        QCOMPARE(model->activeSlice(), a);

        ContainerManager* cm = model->containerManager();
        ContainerWidget* container = cm->createContainer(1, DockMode::OverlayDocked);
        QVERIFY(container != nullptr);
        auto* meter = new MeterWidget();
        meter->setProperty("containerSourceContext",
            QJsonObject{{QStringLiteral("rxSourceMode"), QStringLiteral("followSelectedRx")}});
        container->setContent(meter);
        BarItem* bar = makeSBar();
        meter->addItem(bar);
        bar->setUnitMode(MeterItem::MeterUnit::S);
        QTRY_VERIFY(!bar->aboveS9Frequency());

        a->setFrequency(50.0e6);
        QTRY_VERIFY(bar->aboveS9Frequency());
        a->setFrequency(7.0e6);
        QTRY_VERIFY(!bar->aboveS9Frequency());

        model->setActiveSlice(1);
        QCOMPARE(model->activeSlice(), b);
        QTRY_VERIFY(bar->aboveS9Frequency());
        bar->setValue(-93);
        QCOMPARE(bar->valueText(), QStringLiteral("S9"));
        model->setActiveSlice(0);
        QTRY_VERIFY(!bar->aboveS9Frequency());
        QVERIFY(sessions.replace({}, false));
    }
};

QTEST_MAIN(TstMeterVhfS9)
#include "tst_meter_vhf_s9.moc"
