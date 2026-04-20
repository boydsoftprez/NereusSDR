// =================================================================
// tests/tst_bar_preset_item.cpp  (NereusSDR)
// =================================================================
//
// Test suite for BarPresetItem — the single parameterised MeterItem
// subclass that collapses 18 ItemGroup::create*BarPreset() factories
// (3 Thetis-sourced + 15 NereusSDR-local + 1 Custom) into one
// first-class item whose behaviour flips by enum flavour. Verifies
// the Thetis-sourced ALC/ALC-Gain/ALC-Group calibration constants
// byte-for-byte, exercises the enum<->string round-trip, and covers
// the Custom pass-through plus the serialize/deserialize blob
// schema (flavour + binding + min/max + label + colour + threshold).
//
// no-port-check: test scaffolding; lineage is documented on the
// header of the implementation file
// (src/gui/meters/presets/BarPresetItem.{h,cpp}). This test exercises
// the public NereusSDR API only and contains no transcribed Thetis
// source text — only references it by description.
// =================================================================

#include <QtTest/QtTest>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QString>

#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/MeterPoller.h"   // MeterBinding::*

using namespace NereusSDR;

class TestBarPresetItem : public QObject
{
    Q_OBJECT

private slots:
    void configureAsMic_setsMicDefaults();
    void configureAsAlc_setsAlcDefaults();
    void configureAsAlcGain_setsAlcGainDefaults();
    void configureAsAlcGroup_setsAlcGroupDefaults();
    void configureAsCustom_acceptsCallerValues();
    void kindString_identifiesAllFlavors();
    void serialize_roundTrip_preservesAllFields();
    void paintSmoke_rendersAtDefaultAspect();
};

// -----------------------------------------------------------------
// configureAs* default-state tests.
// -----------------------------------------------------------------
void TestBarPresetItem::configureAsMic_setsMicDefaults()
{
    BarPresetItem item;
    item.configureAsMic();
    QCOMPARE(item.presetKind(), BarPresetItem::Kind::Mic);
    QCOMPARE(item.bindingId(), MeterBinding::TxMic);
    QCOMPARE(item.minValue(), -30.0);
    QCOMPARE(item.maxValue(),  12.0);
    QCOMPARE(item.label(),     QStringLiteral("Mic"));
}

void TestBarPresetItem::configureAsAlc_setsAlcDefaults()
{
    BarPresetItem item;
    item.configureAsAlc();
    QCOMPARE(item.presetKind(), BarPresetItem::Kind::Alc);
    QCOMPARE(item.bindingId(), MeterBinding::TxAlc);
    // Thetis AddALCBar (MeterManager.cs:23347-23349): -30 -> 0,
    // 0 -> 0.665, 12 -> 0.99. Min/max span the first and last keys.
    QCOMPARE(item.minValue(), -30.0);
    QCOMPARE(item.maxValue(),  12.0);
    QCOMPARE(item.label(),     QStringLiteral("ALC"));
}

void TestBarPresetItem::configureAsAlcGain_setsAlcGainDefaults()
{
    BarPresetItem item;
    item.configureAsAlcGain();
    QCOMPARE(item.presetKind(), BarPresetItem::Kind::AlcGain);
    QCOMPARE(item.bindingId(), MeterBinding::TxAlcGain);
    // Thetis AddALCGainBar (MeterManager.cs:23433-23435): 0 -> 0,
    // 20 -> 0.8, 25 -> 0.99.
    QCOMPARE(item.minValue(),  0.0);
    QCOMPARE(item.maxValue(), 25.0);
    QCOMPARE(item.label(),    QStringLiteral("ALC Gain"));
}

void TestBarPresetItem::configureAsAlcGroup_setsAlcGroupDefaults()
{
    BarPresetItem item;
    item.configureAsAlcGroup();
    QCOMPARE(item.presetKind(), BarPresetItem::Kind::AlcGroup);
    QCOMPARE(item.bindingId(), MeterBinding::TxAlcGroup);
    // Thetis AddALCGroupBar (MeterManager.cs:23494-23496): -30 -> 0,
    // 0 -> 0.5, 25 -> 0.99.
    QCOMPARE(item.minValue(), -30.0);
    QCOMPARE(item.maxValue(),  25.0);
    QCOMPARE(item.label(),    QStringLiteral("ALC Group"));
}

void TestBarPresetItem::configureAsCustom_acceptsCallerValues()
{
    BarPresetItem item;
    item.configureAsCustom(/*bindingId=*/999,
                           /*minV=*/-42.5,
                           /*maxV=*/17.25,
                           /*label=*/QStringLiteral("Widget"));
    QCOMPARE(item.presetKind(), BarPresetItem::Kind::Custom);
    QCOMPARE(item.bindingId(), 999);
    QCOMPARE(item.minValue(), -42.5);
    QCOMPARE(item.maxValue(),  17.25);
    QCOMPARE(item.label(),    QStringLiteral("Widget"));
}

// -----------------------------------------------------------------
// kindString() — one slot covers every enumerator.
// -----------------------------------------------------------------
void TestBarPresetItem::kindString_identifiesAllFlavors()
{
    struct Row {
        void (*configure)(BarPresetItem&);
        BarPresetItem::Kind kind;
        const char* expected;
    };

    // Each configureAs* sets kind + defaults; kindString() should
    // echo the enumerator's canonical string.
    const Row rows[] = {
        { [](BarPresetItem& i){ i.configureAsMic(); },
          BarPresetItem::Kind::Mic, "Mic" },
        { [](BarPresetItem& i){ i.configureAsAlc(); },
          BarPresetItem::Kind::Alc, "Alc" },
        { [](BarPresetItem& i){ i.configureAsAlcGain(); },
          BarPresetItem::Kind::AlcGain, "AlcGain" },
        { [](BarPresetItem& i){ i.configureAsAlcGroup(); },
          BarPresetItem::Kind::AlcGroup, "AlcGroup" },
        { [](BarPresetItem& i){ i.configureAsComp(); },
          BarPresetItem::Kind::Comp, "Comp" },
        { [](BarPresetItem& i){ i.configureAsCfc(); },
          BarPresetItem::Kind::Cfc, "Cfc" },
        { [](BarPresetItem& i){ i.configureAsCfcGain(); },
          BarPresetItem::Kind::CfcGain, "CfcGain" },
        { [](BarPresetItem& i){ i.configureAsLeveler(); },
          BarPresetItem::Kind::Leveler, "Leveler" },
        { [](BarPresetItem& i){ i.configureAsLevelerGain(); },
          BarPresetItem::Kind::LevelerGain, "LevelerGain" },
        { [](BarPresetItem& i){ i.configureAsAgc(); },
          BarPresetItem::Kind::Agc, "Agc" },
        { [](BarPresetItem& i){ i.configureAsAgcGain(); },
          BarPresetItem::Kind::AgcGain, "AgcGain" },
        { [](BarPresetItem& i){ i.configureAsPbsnr(); },
          BarPresetItem::Kind::Pbsnr, "Pbsnr" },
        { [](BarPresetItem& i){ i.configureAsEq(); },
          BarPresetItem::Kind::Eq, "Eq" },
        { [](BarPresetItem& i){ i.configureAsSignalBar(); },
          BarPresetItem::Kind::SignalBar, "SignalBar" },
        { [](BarPresetItem& i){ i.configureAsAvgSignalBar(); },
          BarPresetItem::Kind::AvgSignalBar, "AvgSignalBar" },
        { [](BarPresetItem& i){ i.configureAsMaxBinBar(); },
          BarPresetItem::Kind::MaxBinBar, "MaxBinBar" },
        { [](BarPresetItem& i){ i.configureAsAdcBar(); },
          BarPresetItem::Kind::AdcBar, "AdcBar" },
        { [](BarPresetItem& i){ i.configureAsAdcMaxMag(); },
          BarPresetItem::Kind::AdcMaxMag, "AdcMaxMag" },
        { [](BarPresetItem& i){
              i.configureAsCustom(42, 0.0, 1.0, QStringLiteral("X"));
          },
          BarPresetItem::Kind::Custom, "Custom" },
    };

    for (const Row& r : rows) {
        BarPresetItem item;
        r.configure(item);
        QCOMPARE(item.presetKind(), r.kind);
        QCOMPARE(item.kindString(), QString::fromLatin1(r.expected));
    }
}

// -----------------------------------------------------------------
// Serialize/deserialize — every tracked field must round-trip.
// -----------------------------------------------------------------
void TestBarPresetItem::serialize_roundTrip_preservesAllFields()
{
    // 1) AlcGain flavour, override label + bar colour + red threshold.
    {
        BarPresetItem a;
        a.configureAsAlcGain();
        a.setRect(0.1f, 0.2f, 0.8f, 0.1f);
        a.setLabel(QStringLiteral("GAIN!"));
        a.setBarColor(QColor(10, 20, 30, 250));
        a.setBackdropColor(QColor(40, 50, 60, 200));
        a.setRedThreshold(-5.0);
        const QString blob = a.serialize();
        QVERIFY(!blob.isEmpty());

        BarPresetItem b;
        QVERIFY(b.deserialize(blob));
        QCOMPARE(b.presetKind(),    BarPresetItem::Kind::AlcGain);
        QCOMPARE(b.bindingId(),     a.bindingId());
        QCOMPARE(b.minValue(),      a.minValue());
        QCOMPARE(b.maxValue(),      a.maxValue());
        QCOMPARE(b.label(),         QStringLiteral("GAIN!"));
        QCOMPARE(b.barColor(),      a.barColor());
        QCOMPARE(b.backdropColor(), a.backdropColor());
        QCOMPARE(b.redThreshold(),  -5.0);
        QCOMPARE(b.x(),             a.x());
        QCOMPARE(b.y(),             a.y());
        QCOMPARE(b.itemWidth(),     a.itemWidth());
        QCOMPARE(b.itemHeight(),    a.itemHeight());
    }

    // 2) Custom flavour with caller-supplied binding + min/max.
    {
        BarPresetItem a;
        a.configureAsCustom(/*bindingId=*/77, /*min=*/-1.25,
                            /*max=*/3.75, QStringLiteral("Custom!"));
        const QString blob = a.serialize();
        BarPresetItem b;
        QVERIFY(b.deserialize(blob));
        QCOMPARE(b.presetKind(), BarPresetItem::Kind::Custom);
        QCOMPARE(b.bindingId(),  77);
        QCOMPARE(b.minValue(),   -1.25);
        QCOMPARE(b.maxValue(),   3.75);
        QCOMPARE(b.label(),      QStringLiteral("Custom!"));
    }
}

// -----------------------------------------------------------------
// paint() smoke — both a Mic (NereusSDR-local) and an Alc
// (Thetis-sourced) flavour are exercised at a narrow strip aspect
// typical of the meter-row layout.
// -----------------------------------------------------------------
void TestBarPresetItem::paintSmoke_rendersAtDefaultAspect()
{
    auto paintOnce = [](BarPresetItem::Kind k) {
        BarPresetItem item;
        switch (k) {
        case BarPresetItem::Kind::Mic: item.configureAsMic(); break;
        case BarPresetItem::Kind::Alc: item.configureAsAlc(); break;
        default: item.configureAsCustom(0, 0.0, 1.0, QStringLiteral("X"));
        }
        item.setRect(0.0f, 0.0f, 1.0f, 1.0f);

        const int W = 400;
        const int H = 60;
        QImage img(W, H, QImage::Format_ARGB32);
        img.fill(Qt::black);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        item.paint(p, W, H);
        p.end();
        QVERIFY(!img.isNull());
        QCOMPARE(img.width(),  W);
        QCOMPARE(img.height(), H);
    };
    paintOnce(BarPresetItem::Kind::Mic);
    paintOnce(BarPresetItem::Kind::Alc);
}

QTEST_MAIN(TestBarPresetItem)
#include "tst_bar_preset_item.moc"
