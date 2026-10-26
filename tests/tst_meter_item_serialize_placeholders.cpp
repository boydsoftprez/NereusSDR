// no-port-check: NereusSDR-original tests for meter item records.
// Modification history (NereusSDR):
//   2026-10-26 - Every meter item type the content registry creates writes
//                 a record with no unfilled QString::arg placeholder, and
//                 the record survives a load and save unchanged.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

#include <QtTest>
#include <QRegularExpression>
#include <QSet>
#include <memory>

#include "core/UnbuiltFeatureList.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/meters/MeterItem.h"

using namespace NereusSDR;

namespace {

// A QString::arg placeholder left in the text: more %N markers than
// .arg() calls.
const QRegularExpression kPlaceholder(QStringLiteral("%\\d"));

QString recordOf(const ContentEntry& entry)
{
    return entry.config.value(QStringLiteral("legacyRecord")).toString();
}

} // namespace

class TestMeterItemSerializePlaceholders : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        // Every type the registry knows, including those behind an
        // unbuilt feature.
        for (const auto& feature : UnbuiltFeatures::all()) {
            UnbuiltFeatures::setBuiltForTest(feature.feature, true);
        }
    }
    void cleanup() { UnbuiltFeatures::resetForTest(); }

    void recordHasNoPlaceholderAndRoundTrips_data()
    {
        UnbuiltFeatures::resetForTest();
        init();
        QTest::addColumn<QString>("typeId");
        ContainerContentRegistry registry;
        QSet<QString> seen;
        for (const auto& descriptor : registry.descriptors()) {
            if (descriptor.singleton || seen.contains(descriptor.typeId)) { continue; }
            seen.insert(descriptor.typeId);
            // Only meter items carry a record; applets and widget factories
            // have none.
            if (recordOf(registry.makeEntry(descriptor.typeId)).isEmpty()) { continue; }
            QTest::newRow(qPrintable(descriptor.typeId)) << descriptor.typeId;
        }
        QVERIFY(seen.size() > 30);
    }
    void recordHasNoPlaceholderAndRoundTrips()
    {
        QFETCH(QString, typeId);
        ContainerContentRegistry registry;
        ContentEntry entry = registry.makeEntry(typeId);
        const QString created = recordOf(entry);
        QVERIFY2(!created.contains(kPlaceholder), qPrintable(created));

        // Load the record and save it again.
        std::unique_ptr<MeterItem> loaded(registry.createMeterItem(entry, nullptr, ContentRenderMode::Validation));
        QVERIFY2(loaded, qPrintable(created));
        const QString saved = loaded->serialize();
        QVERIFY2(!saved.contains(kPlaceholder), qPrintable(saved));
        QCOMPARE(saved, created);

        // And once more from the saved record.
        entry.config.insert(QStringLiteral("legacyRecord"), saved);
        std::unique_ptr<MeterItem> reloaded(registry.createMeterItem(entry, nullptr, ContentRenderMode::Validation));
        QVERIFY2(reloaded, qPrintable(saved));
        QCOMPARE(reloaded->serialize(), saved);
    }
};

QTEST_MAIN(TestMeterItemSerializePlaceholders)
#include "tst_meter_item_serialize_placeholders.moc"
