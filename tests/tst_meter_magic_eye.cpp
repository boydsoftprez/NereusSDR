// no-port-check: NereusSDR-original tests for the magic eye options; the
// expected values are Thetis's, cited beside each check.
// Modification history (NereusSDR):
//   2026-10-26 - Fix wave R1: the built-in bezel option is shown disabled,
//                 with its reason, in both editors. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Magic eye preset and building block: attack and decay,
//                 eye and bezel size, the bezel image, calibration and the
//                 VHF S9 rule, history, the saved record and new-item
//                 defaults. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QColor>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QPainter>
#include <QTemporaryDir>

#include <memory>

#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContentPropertyEditor.h"
#include "gui/containers/meter_property_editors/BaseItemEditor.h"
#include "gui/containers/meter_property_editors/MagicEyeItemEditor.h"
#include "gui/meters/AboveS9Frequency.h"
#include "gui/meters/MagicEyeItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/presets/MagicEyePresetItem.h"

using namespace NereusSDR;

namespace {

// A building-block eye filling a 200 x 200 widget, bound to the average
// signal strength.
std::unique_ptr<MagicEyeItem> makeEye(bool thetis)
{
    auto eye = std::make_unique<MagicEyeItem>();
    if (thetis) { eye->applyNewItemDefaults(); }
    eye->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    eye->setBindingId(MeterBinding::SignalAvg);
    return eye;
}

QImage render(MeterItem& item, int w = 200, int h = 200)
{
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    item.paint(p, w, h);
    p.end();
    return img;
}

// The record a magic eye saved before the options existed.
const QString kOldRecord = QStringLiteral("MAGICEYE|0|0|1|1|1|5|#ff00ff88|");

} // namespace

class TstMeterMagicEye : public QObject {
    Q_OBJECT

private slots:
    // --- Building block (MAGICEYE) ---------------------------------------

    void attackAndDecayShapeTheStepResponse()
    {
        // Thetis Update: rising by AttackRatio, falling by DecayRatio
        // (MeterManager.cs:17247-17250 [v2.10.3.15]); new eyes take .2 / .05
        // (AddMagicEye :23585-23586).
        auto eye = makeEye(true);
        QCOMPARE(eye->attackRatio(), 0.2f);
        QCOMPARE(eye->decayRatio(), 0.05f);
        QCOMPARE(eye->displayValue(), -127.0f);
        eye->setValue(-73.0);
        QVERIFY(qAbs(eye->displayValue() - (-127.0f + 0.2f * 54.0f)) < 1e-4f);
        const float risen = eye->displayValue();
        eye->setValue(-127.0);
        QVERIFY(qAbs(eye->displayValue() - (risen + 0.05f * (-127.0f - risen))) < 1e-4f);

        // A different attack gives a different response to the same step.
        auto fast = makeEye(true);
        fast->setAttackRatio(1.0f);
        fast->setValue(-73.0);
        QCOMPARE(fast->displayValue(), -73.0f);
        fast->setDecayRatio(0.5f);
        fast->setValue(-127.0);
        QCOMPARE(fast->displayValue(), -100.0f);

        // Ratios clamp to 0..1.
        fast->setAttackRatio(3.0f);
        fast->setDecayRatio(-1.0f);
        QCOMPARE(fast->attackRatio(), 1.0f);
        QCOMPARE(fast->decayRatio(), 0.0f);
    }

    void eyeScaleHalvesTheEyeRadius()
    {
        auto eye = makeEye(true);
        const QRect full = eye->eyeRect(200, 200);
        QCOMPARE(full.width(), 200);
        eye->setEyeScale(0.5f);
        const QRect half = eye->eyeRect(200, 200);
        QCOMPARE(half.width(), 100);
        QCOMPARE(half.center(), full.center());

        // Painted: 80 px left of centre lies inside the full eye only.
        auto big = makeEye(true);
        QVERIFY(qAlpha(render(*big).pixel(20, 100)) > 0);
        QCOMPARE(qAlpha(render(*eye).pixel(20, 100)), 0);
        QVERIFY(qAlpha(render(*eye).pixel(70, 100)) > 0);

        // Thetis's setup range, 0.01 to 1 (setup.designer.cs:55518-55537).
        eye->setEyeScale(0.0f);
        QCOMPARE(eye->eyeScale(), 0.01f);
        eye->setEyeScale(2.0f);
        QCOMPARE(eye->eyeScale(), 1.0f);
        eye->setBezelScale(0.001f);
        QCOMPARE(eye->bezelScale(), 0.01f);
    }

    void bezelImageDrawsAtTheBezelScale()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("bezel.png"));
        QImage bezel(64, 64, QImage::Format_ARGB32);
        bezel.fill(QColor(255, 0, 255));
        QVERIFY(bezel.save(path));

        auto eye = makeEye(true);
        eye->setEyeScale(0.25f);
        eye->setBezelScale(1.0f);
        eye->setBezelImagePath(path);
        QCOMPARE(eye->bezelRect(200, 200).width(), 200);
        const QImage img = render(*eye);
        QCOMPARE(QColor(img.pixel(5, 5)), QColor(255, 0, 255));
        eye->setBezelScale(0.5f);
        QCOMPARE(qAlpha(render(*eye).pixel(5, 5)), 0);
    }

    void calibrationPutsS9AtPoint85()
    {
        // AddMagicEye calibration: -127 -> 0, -73 -> .85, -13 -> 1
        // (MeterManager.cs:23590-23592 [v2.10.3.15]).
        auto eye = makeEye(true);
        QVERIFY(qAbs(eye->scalePosition(-127.0f) - 0.0f) < 1e-5f);
        QVERIFY(qAbs(eye->scalePosition(-73.0f) - 0.85f) < 1e-5f);
        QVERIFY(qAbs(eye->scalePosition(-13.0f) - 1.0f) < 1e-5f);
        QVERIFY(qAbs(eye->scalePosition(-100.0f) - 0.425f) < 1e-5f);
    }

    void vhfSliceUsesS9AtMinus93()
    {
        // A magic eye on a slice at or above 30 MHz: S9 = -93 dBm, the
        // reading raised 20 dB (MeterManager.cs:5419-5429, 41049-41052
        // [v2.10.3.15]).
        QVERIFY(isAboveS9Frequency(30.0e6));
        auto eye = makeEye(true);
        eye->setAboveS9Frequency(true);
        QVERIFY(qAbs(eye->scalePosition(-93.0f) - 0.85f) < 1e-5f);
        QVERIFY(eye->scalePosition(-73.0f) > 0.86f);
        eye->setAboveS9Frequency(false);
        QVERIFY(qAbs(eye->scalePosition(-73.0f) - 0.85f) < 1e-5f);

        // A saved eye (linear scale) moves 20 dB as well: -93 dBm above
        // sits where -73 dBm sits below.
        auto old = makeEye(false);
        const float below = old->scalePosition(-73.0f);
        old->setAboveS9Frequency(true);
        QCOMPARE(old->scalePosition(-93.0f), below);

        // A new eye made on a VHF slice starts 20 dB lower
        // (MeterManager.cs:23593-23594 [v2.10.3.15]).
        MagicEyeItem vhf;
        vhf.setAboveS9Frequency(true);
        vhf.applyNewItemDefaults();
        QCOMPARE(vhf.displayValue(), -147.0f);

        // The painted eye follows: -93 dBm above paints as -73 dBm below.
        auto low = makeEye(true);
        low->setAttackRatio(1.0f);
        low->setValue(-73.0);
        auto high = makeEye(true);
        high->setAttackRatio(1.0f);
        high->setAboveS9Frequency(true);
        high->setValue(-93.0);
        QCOMPARE(render(*high), render(*low));
    }

    void vhfPresetUsesS9AtMinus93()
    {
        MagicEyePresetItem eye;
        QVERIFY(qAbs(eye.calibratedPoint(0, -73.0).x() - 0.85) < 1e-9);
        eye.setAboveS9Frequency(true);
        QVERIFY(qAbs(eye.calibratedPoint(0, -93.0).x() - 0.85) < 1e-9);
    }

    void historyIgnoresReadingsAfterClear()
    {
        // Thetis ClearHistory: ignore IgnoreHistoryDuration / UpdateInterval
        // readings (MeterManager.cs:17319-17327 [v2.10.3.15]).
        auto eye = makeEye(true);
        eye->setUpdateIntervalMs(100);
        eye->setHistoryDurationMs(300);
        for (int i = 0; i < 10; ++i) { eye->setValue(-73.0); }
        QCOMPARE(eye->historySize(), 3);
        eye->clearHistory();
        QCOMPARE(eye->historySize(), 0);
        QCOMPARE(eye->ignoringReadings(), 20);
        eye->setValue(-50.0);
        QCOMPARE(eye->historySize(), 0);
        QCOMPARE(eye->ignoringReadings(), 19);
        QCOMPARE(eye->maxHistory(), eye->displayValue());
    }

    void oldRecordKeepsFixedSmoothingAndLook()
    {
        MagicEyeItem old;
        QVERIFY(old.deserialize(kOldRecord));
        QCOMPARE(old.attackRatio(), MagicEyeItem::kSmoothAlpha);
        QCOMPARE(old.decayRatio(), MagicEyeItem::kSmoothAlpha);
        QVERIFY(old.scaleCalibration().isEmpty());
        QCOMPARE(old.eyeScale(), 1.0f);
        QCOMPARE(old.bezelScale(), 1.0f);
        QCOMPARE(old.glowColor(), QColor(0x00, 0xff, 0x88));

        // The fixed smoothing: value += 0.3 * (reading - value), clamped to
        // S0..S9+60, both directions.
        float expected = MagicEyeItem::kS0Dbm;
        for (double reading : {-73.0, -73.0, -100.0, 0.0, -140.0}) {
            old.setValue(reading);
            expected += MagicEyeItem::kSmoothAlpha * (static_cast<float>(reading) - expected);
            expected = std::clamp(expected, MagicEyeItem::kS0Dbm, MagicEyeItem::kMaxDbm);
            QCOMPARE(old.displayValue(), expected);
        }
        // The linear scale.
        QCOMPARE(old.scalePosition(-70.0f), (-70.0f + 127.0f) / 114.0f);

        // Paints as an eye built in code (the look before the options).
        MagicEyeItem built;
        built.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        built.setBindingId(MeterBinding::SignalAvg);
        MagicEyeItem restored;
        QVERIFY(restored.deserialize(built.serialize()));
        for (MagicEyeItem* e : {&built, &restored}) {
            for (double reading : {-90.0, -80.0, -60.0}) { e->setValue(reading); }
        }
        QCOMPARE(render(restored), render(built));

        // Saving an old record keeps its values.
        MagicEyeItem again;
        QVERIFY(again.deserialize(old.serialize()));
        QCOMPARE(again.attackRatio(), MagicEyeItem::kSmoothAlpha);
        QVERIFY(again.scaleCalibration().isEmpty());
    }

    void newFieldsRoundTrip()
    {
        MagicEyeItem eye;
        eye.applyNewItemDefaults();
        eye.setAttackRatio(0.4f);
        eye.setDecayRatio(0.1f);
        eye.setShowHistory(true);
        eye.setHistoryDurationMs(1500);
        eye.setHistoryColour(QColor(10, 20, 30, 40));
        eye.setIgnoreHistoryMs(1000);
        eye.setEyeScale(0.6f);
        eye.setBezelScale(0.8f);
        MagicEyeItem copy;
        QVERIFY(copy.deserialize(eye.serialize()));
        QCOMPARE(copy.attackRatio(), 0.4f);
        QCOMPARE(copy.decayRatio(), 0.1f);
        QVERIFY(copy.showHistory());
        QCOMPARE(copy.historyDurationMs(), 1500);
        QCOMPARE(copy.historyColour(), QColor(10, 20, 30, 40));
        QCOMPARE(copy.ignoreHistoryMs(), 1000);
        QCOMPARE(copy.eyeScale(), 0.6f);
        QCOMPARE(copy.bezelScale(), 0.8f);
        QCOMPARE(copy.scaleCalibration(), eye.scaleCalibration());
        QCOMPARE(copy.serialize(), eye.serialize());
    }

    void makeEntryTakesThetisDefaults()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("MAGICEYE"));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* eye = qobject_cast<MagicEyeItem*>(item.get());
        QVERIFY(eye);
        const MagicEyeItemThetisDefaults& d = kMagicEyeThetisDefaults;
        QCOMPARE(eye->attackRatio(), d.attackRatio);
        QCOMPARE(eye->decayRatio(), d.decayRatio);
        QCOMPARE(eye->glowColor(), QColor(0x00, 0xff, 0x00));   // Color.Lime
        QCOMPARE(eye->bindingId(), MeterBinding::SignalAvg);
        QCOMPARE(eye->scaleCalibration().size(), 3);
        QCOMPARE(eye->scaleCalibration().value(-73.0f), 0.85f);
        QCOMPARE(eye->historyDurationMs(), 500);
        QCOMPARE(eye->ignoreHistoryMs(), 2000);
        QVERIFY(!eye->showHistory());
        QCOMPARE(eye->eyeScale(), 1.0f);
        QCOMPARE(eye->bezelScale(), 1.0f);
        QVERIFY(eye->bezelImagePath().isEmpty());
    }

    void editorShowsTheOptions()
    {
        auto eye = makeEye(true);
        MagicEyeItemEditor editor;
        editor.setItem(eye.get());
        bool sawCalibration = false;
        for (QLineEdit* edit : editor.findChildren<QLineEdit*>()) {
            if (edit->text().contains(QStringLiteral("-73=0.85"))) { sawCalibration = true; }
        }
        QVERIFY(sawCalibration);
    }

    // --- Preset (meter.magicEye) -----------------------------------------

    void presetEyeScaleHalvesTheEye()
    {
        MagicEyePresetItem full;
        full.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        MagicEyePresetItem half;
        half.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        QJsonObject edit{{QStringLiteral("eyeScale"), 0.5}};
        QVERIFY(half.applyConfiguration(edit));
        // A 260 x 180 face: the eye box is 145.6 x 128 px around (130, 92).
        const QImage a = render(full, 260, 180);
        const QImage b = render(half, 260, 180);
        // 60 px left of the centre: inside the full eye, outside the half.
        QVERIFY(a.pixel(70, 92) != b.pixel(70, 92));
        QCOMPARE(b.pixel(70, 92), b.pixel(5, 92));   // backdrop
        // The centre stays the eye.
        QCOMPARE(a.pixel(130, 92), b.pixel(130, 92));
        QVERIFY(!half.applyConfiguration({{QStringLiteral("eyeScale"), 0.0}}));
        QVERIFY(!half.applyConfiguration({{QStringLiteral("bezelScale"), 1.5}}));
    }

    void presetBezelImageIsLocalOnly()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("bezel.png"));
        QImage bezel(32, 32, QImage::Format_ARGB32);
        bezel.fill(QColor(255, 0, 255));
        QVERIFY(bezel.save(path));
        MagicEyePresetItem eye;
        eye.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        QVERIFY(eye.applyConfiguration({{QStringLiteral("bezelImage"), path}}));
        const QImage img = render(eye, 260, 180);
        // The bezel fills the eye box (bezel size 1).
        QCOMPARE(QColor(img.pixel(130, 40)), QColor(255, 0, 255));
        QVERIFY(!eye.applyConfiguration({{QStringLiteral("bezelImage"),
                                          QStringLiteral("https://example.invalid/eye.png")}}));
        QCOMPARE(eye.configuration()[QStringLiteral("bezelImage")].toString(), path);
    }

    void presetOldRecordKeepsItsLook()
    {
        MagicEyePresetItem built;
        built.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        QJsonObject old = built.configuration();
        old.remove(QStringLiteral("eyeScale"));
        old.remove(QStringLiteral("bezelScale"));
        old.remove(QStringLiteral("bezelImage"));
        MagicEyePresetItem restored;
        QVERIFY(restored.applyConfiguration(old));
        QCOMPARE(restored.configuration()[QStringLiteral("eyeScale")].toDouble(), 1.0);
        for (MagicEyePresetItem* e : {&built, &restored}) {
            e->pushBindingValue(e->bindingId(), -80.0);
            e->advanceMeter(1000);
        }
        QCOMPARE(render(restored, 260, 180), render(built, 260, 180));
    }

    void presetNewItemTakesThetisDefaults()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("meter.magicEye"));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* eye = qobject_cast<CompositePresetItem*>(item.get());
        QVERIFY(eye);
        const QJsonObject c = eye->configuration();
        QCOMPARE(c[QStringLiteral("eyeScale")].toDouble(), 1.0);
        QCOMPARE(c[QStringLiteral("bezelScale")].toDouble(), 1.0);
        const QJsonObject signal = c[QStringLiteral("channels")].toArray().first().toObject();
        QCOMPARE(signal[QStringLiteral("attack")].toDouble(), double(0.2f));
        QCOMPARE(signal[QStringLiteral("decay")].toDouble(), double(0.05f));
        QCOMPARE(signal[QStringLiteral("historyMs")].toInt(), 500);
        QCOMPARE(signal[QStringLiteral("bindingId")].toInt(), MeterBinding::SignalAvg);
        QCOMPARE(QColor(signal[QStringLiteral("color")].toString()), QColor(0x00, 0xff, 0x00));
        QCOMPARE(QColor(signal[QStringLiteral("historyColor")].toString()), QColor(255, 255, 255, 128));
        QVERIFY(eye->editableChannelFields(0).contains(QStringLiteral("showHistory")));
        QVERIFY(eye->editableFields().contains(QStringLiteral("bezelImage")));
    }
    // Fix wave R1: the built-in bezel art is not bundled; the option stays
    // visible, disabled, with the reason, and the bezel image path still
    // works.
    void builtInBezelIsShownDisabledWithItsReason()
    {
        const QString reason = QStringLiteral("No artwork is available for this option");
        QCOMPARE(BaseItemEditor::noArtworkReason(), reason);

        ContainerContentRegistry registry;
        ContentPropertyEditor editor(registry);
        editor.setEntry(registry.makeEntry(QStringLiteral("meter.magicEye")));
        auto* live = editor.findChild<QCheckBox*>(QStringLiteral("builtInBezel"));
        QVERIFY(live);
        QVERIFY(!live->isEnabled());
        QCOMPARE(live->toolTip(), reason);
        QVERIFY(editor.findChild<QLineEdit*>(QStringLiteral("bezelImage")));

        MagicEyeItem item;
        MagicEyeItemEditor legacy;
        legacy.setItem(&item);
        auto* old = legacy.findChild<QCheckBox*>(QStringLiteral("builtInBezel"));
        QVERIFY(old);
        QVERIFY(!old->isEnabled());
        QCOMPARE(old->toolTip(), reason);
    }
};

QTEST_MAIN(TstMeterMagicEye)
#include "tst_meter_magic_eye.moc"
