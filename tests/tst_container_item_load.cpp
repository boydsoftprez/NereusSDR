// no-port-check: NereusSDR-original container item load tests.
// Modification history (NereusSDR):
//   2026-10-26 - A saved meter.adcMax entry is kept with its record and
//                 shown unavailable with its reason; Image and Magic eye
//                 records with an empty image path load without a file
//                 warning. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-26 - meter.adcMax is available on the container parity branch:
//                 the saved entry is kept with its record and its bar is
//                 built and veiled with its reason. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
#include <QtTest>
#include <QColor>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QRegularExpression>
#include <QSplitter>
#include <QTemporaryDir>
#include <memory>

#include "core/AppSettings.h"
#include "gui/containers/ContainerContentHost.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/meters/MagicEyeItem.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterWidget.h"

using namespace NereusSDR;

namespace {
// The reason the provider-less ADC maximum bar veils itself with
// (BarPresetItem.cpp adcMaxUnavailableText).
const QString kAdcMaxReason = QStringLiteral("ADC magnitude is not available");

QStringList typeIds(const QVector<ContentEntry>& contents)
{
    QStringList ids;
    for (const auto& entry : contents) { ids.append(entry.typeId); }
    return ids;
}
} // namespace

class TstContainerItemLoad : public QObject
{
    Q_OBJECT

private slots:
    // The ADC maximum magnitude bar has no reading yet. It is placed like
    // its siblings and its bar veils itself with the reason. A saved entry
    // of that type is kept through a save and a reload, keeps its record,
    // and is built as that veiled bar.
    void savedAdcMaxEntryIsKeptAndVeiled()
    {
        ContainerContentRegistry registry;
        QVERIFY(registry.isAvailable(QStringLiteral("meter.adcMax")));

        const ContentEntry adcMax = registry.makeEntry(QStringLiteral("meter.adcMax"));
        const ContentEntry signal = registry.makeEntry(QStringLiteral("meter.signal"));
        const QString adcRecord = adcMax.config.value(QStringLiteral("legacyRecord")).toString();
        QVERIFY(adcRecord.contains(QStringLiteral("\"flavor\":\"AdcMax\"")));
        QCOMPARE(adcMax.name, QStringLiteral("ADC maximum bar"));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        AppSettings settings(dir.filePath(QStringLiteral("settings")));
        {
            ContainerWorkspaceStore store(settings);
            QWidget root;
            QSplitter splitter(&root);
            ContainerManager manager(&root, &splitter);
            manager.setWorkspaceAdapter(&store, &registry);
            WorkspaceDocument document;
            document.mainContainerId = QStringLiteral("main");
            ContainerDocument main;
            main.id = QStringLiteral("main");
            main.layout = ContentLayout::VerticalStack;
            main.contents = {adcMax, signal};
            document.containers = {main};
            QCOMPARE(manager.commitWorkspace(document, 0).status, CommitStatus::Saved);
        }

        // Reload from the saved settings.
        ContainerWorkspaceStore reloadedStore(settings);
        const DocumentResult loaded = reloadedStore.load();
        QVERIFY2(loaded.ok, qPrintable(loaded.error));
        QCOMPARE(loaded.document.containers.size(), 1);
        const auto& contents = loaded.document.containers.first().contents;
        QCOMPARE(typeIds(contents),
                 (QStringList{QStringLiteral("meter.adcMax"), QStringLiteral("meter.signal")}));
        QCOMPARE(contents[0].id, adcMax.id);
        QCOMPARE(contents[0].config.value(QStringLiteral("legacyRecord")).toString(), adcRecord);

        // Shown: the live host keeps the row and builds the bar, veiled with
        // the reason.
        QWidget root;
        QSplitter splitter(&root);
        ContainerManager manager(&root, &splitter);
        manager.setWorkspaceAdapter(&reloadedStore, &registry);
        manager.reconcileWorkspace(loaded.document);
        ContainerContentHost* host = manager.contentHost(QStringLiteral("main"));
        QVERIFY(host);
        const auto& rows = host->entryRows();
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows[0].entryId, adcMax.id);
        QVERIFY(rows[0].item);
        QVERIFY(rows[0].effectiveVisible);
        QCOMPARE(rows[0].item->bindingUnavailableReason(rows[0].item->bindingId()), kAdcMaxReason);
        QCOMPARE(rows[1].entryId, signal.id);
        QVERIFY(rows[1].item);

        // Saved again, the entry and its record are unchanged.
        const ContainerDocument captured = host->captureDocument();
        QCOMPARE(typeIds(captured.contents),
                 (QStringList{QStringLiteral("meter.adcMax"), QStringLiteral("meter.signal")}));
        QCOMPARE(captured.contents[0].config.value(QStringLiteral("legacyRecord")).toString(), adcRecord);
    }

    // The same record pasted or loaded through a meter's item list (the
    // legacy path) keeps its record.
    void adcMaxRecordSurvivesTheLegacyItemList()
    {
        ContainerContentRegistry registry;
        const QString adcRecord = registry.makeEntry(QStringLiteral("meter.adcMax"))
                                      .config.value(QStringLiteral("legacyRecord")).toString();
        const QString text = registry.makeEntry(QStringLiteral("CLOCK"))
                                 .config.value(QStringLiteral("legacyRecord")).toString()
            + QLatin1Char('\n') + adcRecord;
        MeterWidget widget;
        QVERIFY(widget.deserializeItems(text));
        QCOMPARE(widget.serializeItems(), text);
    }

    // An Image or Magic eye record with no image path loads without
    // handing the empty path to a file API (which warned
    // "QFSFileEngine::open: No file name specified").
    void emptyImagePathLoadsWithoutAFileWarning_data()
    {
        QTest::addColumn<QString>("typeId");
        QTest::newRow("IMAGE") << QStringLiteral("IMAGE");
        QTest::newRow("MAGICEYE") << QStringLiteral("MAGICEYE");
    }
    void emptyImagePathLoadsWithoutAFileWarning()
    {
        QFETCH(QString, typeId);
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        ContainerContentRegistry registry;
        ContentEntry entry = registry.makeEntry(typeId);
        const QString record = entry.config.value(QStringLiteral("legacyRecord")).toString();
        QCOMPARE(record.section(QLatin1Char('|'), 0, 0), typeId);
        QCOMPARE(record.section(QLatin1Char('|'), -1), QString());
        std::unique_ptr<MeterItem> item(
            registry.createMeterItem(entry, nullptr, ContentRenderMode::Validation));
        QVERIFY(item);
        QCOMPARE(item->serialize(), record);
        if (auto* image = qobject_cast<ImageItem*>(item.get())) {
            QCOMPARE(image->imagePath(), QString());
            image->setImagePath(QString());
        }
        if (auto* eye = qobject_cast<MagicEyeItem*>(item.get())) {
            QCOMPARE(eye->bezelImagePath(), QString());
            eye->setBezelImagePath(QString());
        }
    }

    // A real path still loads and draws, and clearing it clears the image.
    void imagePathStillLoadsAndClears()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("red.png"));
        QImage red(4, 4, QImage::Format_ARGB32);
        red.fill(QColor(255, 0, 0));
        QVERIFY(red.save(path));

        ImageItem item;
        item.setRect(0, 0, 1, 1);
        const auto drawn = [&item] {
            QImage canvas(8, 8, QImage::Format_ARGB32);
            canvas.fill(Qt::transparent);
            QPainter p(&canvas);
            item.paint(p, 8, 8);
            p.end();
            return canvas.pixelColor(4, 4);
        };
        item.setImagePath(path);
        QCOMPARE(drawn(), QColor(255, 0, 0));
        item.setImagePath(QString());
        QCOMPARE(drawn().alpha(), 0);
    }
};

QTEST_MAIN(TstContainerItemLoad)
#include "tst_container_item_load.moc"
