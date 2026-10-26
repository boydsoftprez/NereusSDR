// no-port-check: NereusSDR-original tests for meter item records.
// Modification history (NereusSDR):
//   2026-10-26 - Every meter item type the content registry creates writes
//                 a record with no unfilled QString::arg placeholder, and
//                 the record survives a load and save unchanged.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Free text holding %1, %20 and %% survives a save, load and
//                 save in every type's record (chained QString::arg re-scans
//                 substituted text).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - The LED condition is free text too.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <functional>
#include <memory>

#include "core/UnbuiltFeatureList.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/meters/DataOutItem.h"
#include "gui/meters/LEDItem.h"
#include "gui/meters/MagicEyeItem.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/NeedleScalePwrItem.h"
#include "gui/meters/RotatorItem.h"
#include "gui/meters/SignalTextItem.h"
#include "gui/meters/TextOverlayItem.h"
#include "gui/meters/WebImageItem.h"

using namespace NereusSDR;

namespace {

// A QString::arg placeholder left in the text: more %N markers than
// .arg() calls.
const QRegularExpression kPlaceholder(QStringLiteral("%\\d"));

QString recordOf(const ContentEntry& entry)
{
    return entry.config.value(QStringLiteral("legacyRecord")).toString();
}

// User text that QString::arg would treat as placeholders if it re-scanned
// a substituted value. No '|': a pipe record cannot hold one.
const QString kText = QStringLiteral("a%1b%20c%%d");
// WebImage keeps four characters of its background ID.
const QString kFourChar = QStringLiteral("%1%%");

// One free-text field of a pipe record: write the text, read it back.
struct TextField {
    QString name;
    QString value;
    std::function<bool(MeterItem*)> set;
    std::function<QString(const MeterItem*)> get;
};

template <typename T>
TextField field(const QString& name, void (T::*setter)(const QString&), QString (T::*getter)() const,
                const QString& value = kText)
{
    return TextField{
        name, value,
        [setter, value](MeterItem* item) {
            auto* typed = dynamic_cast<T*>(item);
            if (!typed) { return false; }
            (typed->*setter)(value);
            return true;
        },
        [getter](const MeterItem* item) {
            const auto* typed = dynamic_cast<const T*>(item);
            return typed ? (typed->*getter)() : QStringLiteral("<wrong type>");
        }};
}

// Every free-text field a pipe-record serializer writes, read from each
// serializer (MeterItem.cpp, MagicEyeItem.cpp, NeedleScalePwrItem.cpp,
// SignalTextItem.cpp, TextOverlayItem.cpp, WebImageItem.cpp,
// RotatorItem.cpp, DataOutItem.cpp, LEDItem.cpp). Every other pipe record holds only
// numbers, flags, colours and GUIDs.
QVector<TextField> textFieldsOf(MeterItem* item)
{
    QVector<TextField> all{
        field(QStringLiteral("imagePath"), &ImageItem::setImagePath, &ImageItem::imagePath),
        field(QStringLiteral("label"), &TextItem::setLabel, &TextItem::label),
        field(QStringLiteral("suffix"), &TextItem::setSuffix, &TextItem::suffix),
        field(QStringLiteral("fontFamily"), &TextItem::setFontFamily, &TextItem::fontFamily),
        field(QStringLiteral("sourceLabel"), &NeedleItem::setSourceLabel, &NeedleItem::sourceLabel),
        field(QStringLiteral("bezelImagePath"), &MagicEyeItem::setBezelImagePath,
              &MagicEyeItem::bezelImagePath),
        field(QStringLiteral("fontFamily"), &NeedleScalePwrItem::setFontFamily,
              &NeedleScalePwrItem::fontFamily),
        field(QStringLiteral("fontFamily"), &SignalTextItem::setFontFamily, &SignalTextItem::fontFamily),
        field(QStringLiteral("fontFamily1"), &TextOverlayItem::setFontFamily1, &TextOverlayItem::fontFamily1),
        field(QStringLiteral("fontFamily2"), &TextOverlayItem::setFontFamily2, &TextOverlayItem::fontFamily2),
        field(QStringLiteral("text1"), &TextOverlayItem::setText1, &TextOverlayItem::text1),
        field(QStringLiteral("text2"), &TextOverlayItem::setText2, &TextOverlayItem::text2),
        field(QStringLiteral("url"), &WebImageItem::setUrl, &WebImageItem::url,
              QStringLiteral("https://example.invalid/") + kText),
        field(QStringLiteral("backgroundFourChar"), &WebImageItem::setBackgroundFourChar,
              &WebImageItem::backgroundFourChar, kFourChar),
        field(QStringLiteral("backgroundImagePath"), &RotatorItem::setBackgroundImagePath,
              &RotatorItem::backgroundImagePath),
        field(QStringLiteral("azControlString"), &RotatorItem::setAzControlString,
              &RotatorItem::azControlString),
        field(QStringLiteral("eleControlString"), &RotatorItem::setEleControlString,
              &RotatorItem::eleControlString),
        field(QStringLiteral("stopControlString"), &RotatorItem::setStopControlString,
              &RotatorItem::stopControlString),
        field(QStringLiteral("mmioGuid"), &DataOutItem::setMmioGuid, &DataOutItem::mmioGuid),
        field(QStringLiteral("mmioVariable"), &DataOutItem::setMmioVariable, &DataOutItem::mmioVariable),
        field(QStringLiteral("condition"), &LEDItem::setCondition, &LEDItem::condition),
    };
    QVector<TextField> applied;
    for (const auto& f : all) {
        if (f.set(item)) { applied.append(f); }
    }
    return applied;
}

// The free-text keys of a JSON record (BarPresetItem::configuration,
// CompositePresetItem::configuration, WaveformMeterItem::configuration).
// A composite writes title, units and bezelImage only where the face can
// edit them; its channel name and units are editable on the needle faces
// (CompositePresetItem::editableChannelFields).
const QStringList kJsonTextKeys{QStringLiteral("label"), QStringLiteral("units"), QStringLiteral("title"),
                                QStringLiteral("bezelImage")};
const QStringList kChannelTextKinds{QStringLiteral("PowerSwrPreset"), QStringLiteral("AnanMM"),
                                    QStringLiteral("CrossNeedle")};
const QStringList kChannelTextKeys{QStringLiteral("name"), QStringLiteral("units")};

// Sets each free-text key the record has; returns the paths it set.
QStringList setJsonText(QJsonObject& record)
{
    QStringList paths;
    for (const QString& key : kJsonTextKeys) {
        if (record.value(key).isString()) {
            record[key] = kText;
            paths << key;
        }
    }
    if (kChannelTextKinds.contains(record.value(QStringLiteral("kind")).toString())) {
        QJsonArray channels = record.value(QStringLiteral("channels")).toArray();
        for (int i = 0; i < channels.size(); ++i) {
            QJsonObject channel = channels[i].toObject();
            for (const QString& key : kChannelTextKeys) {
                if (channel.value(key).isString()) {
                    channel[key] = kText;
                    paths << QStringLiteral("channels/%1/%2").arg(i).arg(key);
                }
            }
            channels[i] = channel;
        }
        record[QStringLiteral("channels")] = channels;
    }
    return paths;
}

QString jsonText(const QJsonObject& record, const QString& path)
{
    const QStringList parts = path.split(QLatin1Char('/'));
    if (parts.size() == 1) { return record.value(path).toString(QStringLiteral("<missing>")); }
    return record.value(parts[0]).toArray().at(parts[1].toInt()).toObject().value(parts[2]).toString(
        QStringLiteral("<missing>"));
}

QJsonObject jsonOf(const QString& record)
{
    return QJsonDocument::fromJson(record.toUtf8()).object();
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

    // User text holding %1, %20 and %% comes back exactly as written after
    // a save, a load and another save.
    void freeTextSurvivesPercentSequences_data() { recordHasNoPlaceholderAndRoundTrips_data(); }
    void freeTextSurvivesPercentSequences()
    {
        QFETCH(QString, typeId);
        ContainerContentRegistry registry;
        ContentEntry entry = registry.makeEntry(typeId);
        const QString created = recordOf(entry);
        const auto load = [&](const QString& record) {
            entry.config.insert(QStringLiteral("legacyRecord"), record);
            return std::unique_ptr<MeterItem>(
                registry.createMeterItem(entry, nullptr, ContentRenderMode::Validation));
        };

        if (created.trimmed().startsWith(QLatin1Char('{'))) {
            QJsonObject record = jsonOf(created);
            const QStringList paths = setJsonText(record);
            if (paths.isEmpty()) { return; }
            // The entry's properties apply after the record; they carry the
            // same keys, so set the text there too.
            QJsonObject properties = entry.config.value(QStringLiteral("properties")).toObject();
            setJsonText(properties);
            entry.config.insert(QStringLiteral("properties"), properties);
            const QString edited = QString::fromUtf8(QJsonDocument(record).toJson(QJsonDocument::Compact));
            const auto loaded = load(edited);
            QVERIFY2(loaded, qPrintable(edited));
            const QString saved = loaded->serialize();
            for (const QString& path : paths) {
                QVERIFY2(jsonText(jsonOf(saved), path) == kText,
                         qPrintable(path + QStringLiteral(": ") + saved));
            }
            const auto reloaded = load(saved);
            QVERIFY2(reloaded, qPrintable(saved));
            QCOMPARE(reloaded->serialize(), saved);
            return;
        }

        const auto item = load(created);
        QVERIFY2(item, qPrintable(created));
        if (auto* web = dynamic_cast<WebImageItem*>(item.get())) { web->setFetchEnabled(false); }
        const QVector<TextField> fields = textFieldsOf(item.get());
        if (fields.isEmpty()) { return; }
        const QString saved = item->serialize();
        const auto loaded = load(saved);
        QVERIFY2(loaded, qPrintable(saved));
        for (const auto& f : fields) {
            QVERIFY2(f.get(loaded.get()) == f.value,
                     qPrintable(f.name + QStringLiteral(" = ") + f.get(loaded.get()) + QStringLiteral(" in ") + saved));
        }
        QCOMPARE(loaded->serialize(), saved);
        const auto reloaded = load(saved);
        QVERIFY2(reloaded, qPrintable(saved));
        QCOMPARE(reloaded->serialize(), saved);
    }
};

QTEST_MAIN(TestMeterItemSerializePlaceholders)
#include "tst_meter_item_serialize_placeholders.moc"
