// no-port-check: NereusSDR-original tests for new container item defaults.
// Modification history (NereusSDR):
//   2026-10-25 - New items take Thetis defaults through
//                 MeterItem::applyNewItemDefaults(); saved items keep their
//                 look. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.

#include <QtTest>
#include <QColor>
#include <QJsonObject>
#include <memory>

#include "gui/containers/ContainerContentRegistry.h"
#include "gui/meters/MeterItem.h"

using namespace NereusSDR;

namespace {

// A SOLID record as NereusSDR saved it before new items took Thetis
// defaults: the old constructor colour #0f0f1a.
const QString kSavedSolidRecord =
    QStringLiteral("SOLID|0.1|0.2|0.3|0.4|-1|3|#ff0f0f1a");

// makeEntry("control.mox") as it was before this change.
const QString kMoxRecordToday = QStringLiteral("OTHERBTNS|0|0|1|1|-1|0|1|16|0.005|0|3|1|0|0");

} // namespace

class TestMeterNewItemDefaults : public QObject {
    Q_OBJECT
private slots:
    void newSolidIsBlack()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("SOLID"));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* solid = qobject_cast<SolidColourItem*>(item.get());
        QVERIFY(solid != nullptr);
        QCOMPARE(solid->colour(), QColor(Qt::black));
    }

    void savedSolidKeepsItsColour()
    {
        SolidColourItem direct;
        QVERIFY(direct.deserialize(kSavedSolidRecord));
        QCOMPARE(direct.colour(), QColor(0x0f, 0x0f, 0x1a));

        ContainerContentRegistry registry;
        ContentEntry entry;
        entry.id = QStringLiteral("saved-solid");
        entry.typeId = QStringLiteral("SOLID");
        entry.name = QStringLiteral("SolidColour");
        entry.config.insert(QStringLiteral("legacyRecord"), kSavedSolidRecord);
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* solid = qobject_cast<SolidColourItem*>(item.get());
        QVERIFY(solid != nullptr);
        QCOMPARE(solid->colour(), QColor(0x0f, 0x0f, 0x1a));
    }

    void freshSolidCapturesNoOverrides()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("SOLID"));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        QVERIFY(item != nullptr);
        const ContentEntry captured = registry.captureMeterItem(*item);
        QVERIFY(captured.config.contains(QStringLiteral("overrides")));
        QVERIFY(captured.config.value(QStringLiteral("overrides")).toObject().isEmpty());
    }

    void controlMoxUnchanged()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("control.mox"));
        QCOMPARE(entry.typeId, QStringLiteral("OTHERBTNS"));
        QCOMPARE(entry.config.value(QStringLiteral("legacyRecord")).toString(), kMoxRecordToday);
        QCOMPARE(entry.config.size(), 1);
    }
};

QTEST_MAIN(TestMeterNewItemDefaults)
#include "tst_meter_new_item_defaults.moc"
