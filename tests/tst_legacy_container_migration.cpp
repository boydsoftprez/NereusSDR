// =================================================================
// tests/tst_legacy_container_migration.cpp  (NereusSDR)
// =================================================================
//
// Test suite for Task 18 of the Edit Container refactor —
// LegacyPresetMigrator tolerant signature detection and the
// MeterWidget::deserializeItems migration hook.
//
// no-port-check: test scaffolding; lineage documented on the header
// of the implementation file (src/gui/meters/presets/
// LegacyPresetMigrator.{h,cpp}). NereusSDR-original — no Thetis
// source is transcribed here.
// =================================================================

#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QPointF>
#include <QString>
#include <QTemporaryDir>
#include <QVector>

#include "core/AppSettings.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/presets/AnanMultiMeterItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/presets/LegacyPresetMigrator.h"

using namespace NereusSDR;

class TestLegacyContainerMigration : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void anaMmFullSignature_collapses();
    void anaMmPartialSignature_fallsBackWithWarning();
    void primitivesOnly_passThrough();
    void unrecognisedGroup_staysAsLoose();
    void barRowFlavour_collapsesToBarPreset();
    void customizedBar_staysAsLoose();
    void doubleMigrationGuard_skipsAlreadyMigrated();
    void fixtureLoadsWithoutCrash();

private:
    static NeedleItem* makeAnanNeedle(int bindingId,
                                       const QString& label,
                                       float x, float y, float w, float h);
    static ImageItem*  makeAnanImage(float x, float y, float w, float h);
};

void TestLegacyContainerMigration::initTestCase()
{
    QLoggingCategory::setFilterRules(
        QStringLiteral("nereus.migrate.preset.debug=true"));
}

// ---------------------------------------------------------------------------
// Synthetic-item builders (match what legacy ItemGroup::createAnanMMPreset
// emitted via NeedleItem::serialize()).
// ---------------------------------------------------------------------------

NeedleItem* TestLegacyContainerMigration::makeAnanNeedle(int bindingId,
                                                          const QString& label,
                                                          float x, float y,
                                                          float w, float h)
{
    auto* n = new NeedleItem();
    n->setRect(x, y, w, h);
    n->setBindingId(bindingId);
    n->setSourceLabel(label);
    n->setNeedleOffset(QPointF(0.004, 0.736));
    n->setRadiusRatio(QPointF(1.0, 0.58));
    return n;
}

ImageItem* TestLegacyContainerMigration::makeAnanImage(float x, float y,
                                                        float w, float h)
{
    auto* img = new ImageItem();
    img->setRect(x, y, w, h);
    img->setImagePath(QStringLiteral(":/meters/ananMM.png"));
    return img;
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void TestLegacyContainerMigration::anaMmFullSignature_collapses()
{
    // Synthetic ANAN MM: 1 ImageItem + 7 NeedleItems, all sharing the
    // item rect, with kAnanMm pivot/radius. Matches the legacy
    // ItemGroup::createAnanMMPreset() byte format exactly.
    QVector<MeterItem*> legacy;
    const float X = 0.0f;
    const float Y = 0.0f;
    const float W = 1.0f;
    const float H = 0.441f;
    legacy.append(makeAnanImage(X, Y, W, H));
    legacy.append(makeAnanNeedle(MeterBinding::SignalAvg,  QStringLiteral("Signal"),      X, Y, W, H));
    legacy.append(makeAnanNeedle(MeterBinding::HwVolts,    QStringLiteral("Volts"),       X, Y, W, H));
    legacy.append(makeAnanNeedle(MeterBinding::HwAmps,     QStringLiteral("Amps"),        X, Y, W, H));
    legacy.append(makeAnanNeedle(MeterBinding::TxPower,    QStringLiteral("Power"),       X, Y, W, H));
    legacy.append(makeAnanNeedle(MeterBinding::TxSwr,      QStringLiteral("SWR"),         X, Y, W, H));
    legacy.append(makeAnanNeedle(MeterBinding::TxAlcGain,  QStringLiteral("Compression"), X, Y, W, H));
    legacy.append(makeAnanNeedle(MeterBinding::TxAlcGroup, QStringLiteral("ALC"),         X, Y, W, H));

    LegacyPresetMigrator::Result r =
        LegacyPresetMigrator::migrate(std::move(legacy));

    QCOMPARE(r.upgradedItems.size(), 1);
    QVERIFY(qobject_cast<AnanMultiMeterItem*>(r.upgradedItems.first()) != nullptr);
    QCOMPARE(r.presetsMigrated, 1);
    QCOMPARE(r.fallbackCases,   0);

    qDeleteAll(r.upgradedItems);
}

void TestLegacyContainerMigration::anaMmPartialSignature_fallsBackWithWarning()
{
    // Partial: image + only 1 needle. Does NOT satisfy ANAN MM
    // signature (needs 7). Items pass through as loose primitives.
    QVector<MeterItem*> legacy;
    legacy.append(makeAnanImage(0.0f, 0.0f, 1.0f, 0.441f));
    legacy.append(makeAnanNeedle(MeterBinding::SignalAvg,
                                  QStringLiteral("Signal"),
                                  0.0f, 0.0f, 1.0f, 0.441f));

    LegacyPresetMigrator::Result r =
        LegacyPresetMigrator::migrate(std::move(legacy));

    QCOMPARE(r.upgradedItems.size(), 2);
    QVERIFY(qobject_cast<AnanMultiMeterItem*>(r.upgradedItems.first()) == nullptr);
    QCOMPARE(r.presetsMigrated, 0);
    // fallbackCases tracks signature-matched-but-customised regions.
    // A partial (count-short) ANAN is not a signature match at all, so
    // nothing increments fallbackCases — the items just pass through.
    QCOMPARE(r.fallbackCases, 0);

    qDeleteAll(r.upgradedItems);
}

void TestLegacyContainerMigration::primitivesOnly_passThrough()
{
    QVector<MeterItem*> legacy;

    auto* txt = new TextItem();
    txt->setRect(0.0f, 0.0f, 0.5f, 0.1f);
    txt->setLabel(QStringLiteral("Hello"));
    legacy.append(txt);

    auto* scale = new ScaleItem();
    scale->setRect(0.0f, 0.1f, 1.0f, 0.1f);
    scale->setBindingId(-1);
    legacy.append(scale);

    LegacyPresetMigrator::Result r =
        LegacyPresetMigrator::migrate(std::move(legacy));

    QCOMPARE(r.upgradedItems.size(), 2);
    QCOMPARE(r.presetsMigrated, 0);

    qDeleteAll(r.upgradedItems);
}

void TestLegacyContainerMigration::unrecognisedGroup_staysAsLoose()
{
    // Image with a non-preset path + one unrelated needle.
    QVector<MeterItem*> legacy;

    auto* img = new ImageItem();
    img->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    img->setImagePath(QStringLiteral(":/art/logo.png"));
    legacy.append(img);

    auto* n = new NeedleItem();
    n->setRect(0.0f, 0.0f, 1.0f, 1.0f);
    n->setBindingId(MeterBinding::SignalAvg);
    legacy.append(n);

    LegacyPresetMigrator::Result r =
        LegacyPresetMigrator::migrate(std::move(legacy));

    QCOMPARE(r.upgradedItems.size(), 2);
    QCOMPARE(r.presetsMigrated, 0);
    QCOMPARE(r.fallbackCases,   0);

    qDeleteAll(r.upgradedItems);
}

void TestLegacyContainerMigration::barRowFlavour_collapsesToBarPreset()
{
    // A lone BarItem whose (binding, min, max) matches a known
    // flavour (AvgSignalBar: bindingId=SignalAvg, -140..0). With the
    // default bar colour it should collapse to BarPresetItem.
    QVector<MeterItem*> legacy;

    auto* bar = new BarItem();
    bar->setRect(0.0f, 0.2f, 1.0f, 0.1f);
    bar->setBindingId(MeterBinding::SignalAvg);
    bar->setRange(-140.0, 0.0);
    legacy.append(bar);

    LegacyPresetMigrator::Result r =
        LegacyPresetMigrator::migrate(std::move(legacy));

    QCOMPARE(r.upgradedItems.size(), 1);
    auto* preset = qobject_cast<BarPresetItem*>(r.upgradedItems.first());
    QVERIFY(preset != nullptr);
    QCOMPARE(preset->presetKind(), BarPresetItem::Kind::AvgSignalBar);
    QCOMPARE(r.presetsMigrated, 1);
    QCOMPARE(r.fallbackCases,   0);

    qDeleteAll(r.upgradedItems);
}

void TestLegacyContainerMigration::customizedBar_staysAsLoose()
{
    // Same binding/min/max as AvgSignalBar, but the user changed the
    // bar colour away from the flavour defaults. Should be kept as
    // a loose BarItem so the customisation survives.
    QVector<MeterItem*> legacy;

    auto* bar = new BarItem();
    bar->setRect(0.0f, 0.2f, 1.0f, 0.1f);
    bar->setBindingId(MeterBinding::SignalAvg);
    bar->setRange(-140.0, 0.0);
    bar->setBarColor(QColor(Qt::magenta));
    legacy.append(bar);

    LegacyPresetMigrator::Result r =
        LegacyPresetMigrator::migrate(std::move(legacy));

    QCOMPARE(r.upgradedItems.size(), 1);
    QVERIFY(qobject_cast<BarPresetItem*>(r.upgradedItems.first()) == nullptr);
    QVERIFY(qobject_cast<BarItem*>(r.upgradedItems.first()) != nullptr);
    QCOMPARE(r.presetsMigrated, 0);
    QCOMPARE(r.fallbackCases,   1);

    qDeleteAll(r.upgradedItems);
}

void TestLegacyContainerMigration::doubleMigrationGuard_skipsAlreadyMigrated()
{
    // Vector that already has a first-class preset mixed in: the
    // guard short-circuits the migrator and returns items as-is.
    QVector<MeterItem*> items;
    items.append(new AnanMultiMeterItem());
    items.append(new NeedleItem());

    LegacyPresetMigrator::Result r =
        LegacyPresetMigrator::migrate(std::move(items));

    QCOMPARE(r.upgradedItems.size(), 2);
    QCOMPARE(r.presetsMigrated, 0);
    QCOMPARE(r.fallbackCases,   0);

    qDeleteAll(r.upgradedItems);
}

void TestLegacyContainerMigration::fixtureLoadsWithoutCrash()
{
    // Load the pre-refactor settings file via an AppSettings instance
    // constructed directly with the fixture path, then feed every
    // ContainerItems_<id> payload through a MeterWidget::deserialize
    // (which invokes the migrator internally) and assert no crash
    // and a non-empty item vector per container.
    const QString fixturePath =
        QStringLiteral("%1/fixtures/legacy/pre-refactor-settings.xml")
            .arg(QLatin1String(TEST_DATA_DIR));
    QVERIFY2(QFile::exists(fixturePath),
             qPrintable(QStringLiteral("Fixture missing: ") + fixturePath));

    AppSettings s(fixturePath);
    s.load();

    const QStringList ids = s.value(QStringLiteral("ContainerIdList"))
        .toString().split(QLatin1Char(','), Qt::SkipEmptyParts);
    QCOMPARE(ids.size(), 4);

    for (const QString& id : ids) {
        const QString payload = s.value(
            QStringLiteral("ContainerItems_%1").arg(id)).toString();
        QVERIFY2(!payload.isEmpty(),
                 qPrintable(QStringLiteral("Empty payload for ") + id));

        MeterWidget mw;
        const bool ok = mw.deserializeItems(payload);
        QVERIFY2(ok,
                 qPrintable(QStringLiteral("deserializeItems returned false for ") + id));
        QVERIFY2(mw.items().size() >= 1,
                 qPrintable(QStringLiteral("Container ") + id
                          + QStringLiteral(" had zero items after migration")));
    }
}

QTEST_MAIN(TestLegacyContainerMigration)
#include "tst_legacy_container_migration.moc"
