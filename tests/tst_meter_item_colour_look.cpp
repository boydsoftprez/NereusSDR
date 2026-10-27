// no-port-check: NereusSDR-original test.
//
// The workspace look for container and item colours (ItemColourLook.h):
// every default colour of every registry item type is in the table, mapped
// to a palette role or kept; a mapped default draws the role's colour with
// the look on, a user's colour stays, and the look off keeps the classic
// colour; records round-trip byte for byte; the editors' swatches show the
// colour drawn and a pick stores the user's colour.

#include <QtTest>
#include <QApplication>
#include <QColorDialog>
#include <QImage>
#include <QJsonDocument>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <memory>

#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerDocument.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/meter_property_editors/ClockItemEditor.h"
#include "gui/meters/ClockItem.h"
#include "gui/meters/ItemColourLook.h"
#include "gui/meters/LEDItem.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/styles/WorkspacePalette.h"
#include "gui/styles/WorkspacePreview.h"

using namespace NereusSDR;

namespace {

// The palette's colour for a role (WorkspacePalette::glassAndDepth()).
QColor role(const char* name)
{
    return Workspace::WorkspacePalette::glassAndDepth().color(QString::fromLatin1(name));
}

QByteArray recordOf(const ContainerContentRegistry& registry, const MeterItem& item,
                    const ContentEntry& prior)
{
    const ContentEntry captured = registry.captureMeterItem(item, prior);
    return QJsonDocument(captured.config).toJson(QJsonDocument::Compact)
           + QByteArray("|") + item.serialize().toUtf8();
}

void paintOnce(MeterItem& item)
{
    // Large enough for every face to lay out its text without complaint.
    QImage image(800, 600, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    QPainter painter(&image);
    item.paint(painter, image.width(), image.height());
}

} // namespace

class TestMeterItemColourLook : public QObject {
    Q_OBJECT

private slots:
    void cleanup() { WorkspacePreview::setEnabledForTest(false); }

    // Every default colour of every registry item type is in the table.
    void everyRegistryDefaultIsListed()
    {
        ContainerContentRegistry registry;
        QStringList unlisted;
        QSet<QString> seen;
        int items = 0;
        for (const ContentDescriptor& descriptor : registry.descriptors()) {
            const ContentEntry entry = registry.makeEntry(descriptor.typeId);
            std::unique_ptr<MeterItem> item(
                registry.createMeterItem(entry, nullptr, ContentRenderMode::Validation));
            if (!item) {
                continue;
            }
            ++items;
            for (const auto& field : ItemColourLook::colourFieldsOf(*item)) {
                if (!ItemColourLook::listed(field.field, field.stored)) {
                    const QString line = QStringLiteral("%1 %2").arg(field.field,
                                                                     field.stored.name(QColor::HexArgb));
                    if (!seen.contains(line)) {
                        seen.insert(line);
                        unlisted << line + QStringLiteral(" (") + descriptor.typeId + QStringLiteral(")");
                    }
                }
            }
        }
        QVERIFY(items > 40);
        QVERIFY2(unlisted.isEmpty(), qPrintable(QStringLiteral("\n") + unlisted.join(QLatin1Char('\n'))));
    }

    // Every role a row names is a palette role, and every row either maps
    // or says why it keeps the classic colour.
    void everyRowNamesARoleOrAReason()
    {
        QStringList bad;
        for (const ItemColourLook::Row& row : ItemColourLook::rows()) {
            if (row.role != nullptr && !role(row.role).isValid()) {
                bad << QStringLiteral("%1: no role %2").arg(QLatin1String(row.field), QLatin1String(row.role));
            }
            if (row.role == nullptr && (row.kept == nullptr || QByteArray(row.kept).isEmpty())) {
                bad << QStringLiteral("%1: kept without a reason").arg(QLatin1String(row.field));
            }
        }
        QVERIFY2(bad.isEmpty(), qPrintable(bad.join(QLatin1Char('\n'))));
    }

    // With the look on, every registry item paints without naming a colour
    // field the table does not know.
    void everyItemPaintsWithKnownFields()
    {
        WorkspacePreview::setEnabledForTest(true);
        QTest::failOnWarning(QRegularExpression(QStringLiteral("unknown colour field")));
        ContainerContentRegistry registry;
        for (const ContentDescriptor& descriptor : registry.descriptors()) {
            const ContentEntry entry = registry.makeEntry(descriptor.typeId);
            std::unique_ptr<MeterItem> item(
                registry.createMeterItem(entry, nullptr, ContentRenderMode::Validation));
            if (item) {
                paintOnce(*item);
            }
        }
    }

    void vfoDisplayFollowsTheLook()
    {
        VfoDisplayItem vfo;
        vfo.applyNewItemDefaults();
        const QColor rx = vfo.rxColour();
        const QColor tx = vfo.txColour();
        const QColor back = vfo.backgroundColour();

        // Look off: the classic colours.
        QCOMPARE(ItemColourLook::effective("VfoDisplay.rxColour", rx), rx);
        QCOMPARE(ItemColourLook::effective("VfoDisplay.backgroundColour", back, &vfo), back);

        // Look on: the roles; RX stays green, TX stays red.
        WorkspacePreview::setEnabledForTest(true);
        QCOMPARE(ItemColourLook::effective("VfoDisplay.rxColour", rx), role("live"));
        QCOMPARE(ItemColourLook::effective("VfoDisplay.txColour", tx), role("tx"));
        QCOMPARE(ItemColourLook::effective("VfoDisplay.backgroundColour", back, &vfo), role("panel"));
        QVERIFY(ItemColourLook::followsLook(QStringLiteral("VfoDisplay.rxColour"), rx));

        // Look on, a user's colour: that colour.
        const QColor user(0x12, 0x34, 0x56);
        vfo.setRxColour(user);
        QCOMPARE(ItemColourLook::effective("VfoDisplay.rxColour", vfo.rxColour()), user);
        QVERIFY(!ItemColourLook::followsLook(QStringLiteral("VfoDisplay.rxColour"), vfo.rxColour()));

        // The painted background is the role's.
        QImage image(240, 120, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::magenta);
        {
            QPainter painter(&image);
            vfo.paint(painter, image.width(), image.height());
        }
        // The bottom right corner: clear of the RX stripe and the text.
        QCOMPARE(image.pixelColor(image.width() - 3, image.height() - 3).rgb(), role("panel").rgb());
    }

    void buttonGridFollowsTheLook()
    {
        ModeButtonItem grid;
        QVERIFY(grid.buttonCount() > 0);
        const ButtonBoxItem::ButtonState& face = grid.button(0);
        QCOMPARE(ItemColourLook::effective("ButtonBox.fillColour", face.fillColour), face.fillColour);
        QCOMPARE(ItemColourLook::effective("ButtonBox.onColour", face.onColour), face.onColour);

        WorkspacePreview::setEnabledForTest(true);
        QCOMPARE(ItemColourLook::effective("ButtonBox.fillColour", face.fillColour), role("raised-top"));
        QCOMPARE(ItemColourLook::effective("ButtonBox.onColour", face.onColour), role("selected-top"));
        QCOMPARE(ItemColourLook::effective("ButtonBox.textColour", face.textColour), role("text"));
        const QColor user(0x80, 0x10, 0x90);
        QCOMPARE(ItemColourLook::effective("ButtonBox.fillColour", user), user);
    }

    void barFollowsTheLook()
    {
        auto bar = std::make_unique<BarItem>();
        bar->applyNewItemDefaults();
        const QColor fill = bar->barColor();
        const QColor peak = bar->peakHoldMarkerColour();
        QCOMPARE(ItemColourLook::effective("Bar.barColor", fill), fill);

        WorkspacePreview::setEnabledForTest(true);
        QCOMPARE(ItemColourLook::effective("Bar.barColor", fill), role("measured"));
        QCOMPARE(ItemColourLook::effective("Bar.peakHoldMarkerColour", peak), role("danger"));
        bar->setBarColor(QColor(0x22, 0x88, 0x44));
        QCOMPARE(ItemColourLook::effective("Bar.barColor", bar->barColor()), QColor(0x22, 0x88, 0x44));
    }

    void clockFollowsTheLook()
    {
        ClockItem clock;
        clock.applyNewItemDefaults();
        const QColor time = clock.timeColour();
        const QColor date = clock.dateColour();
        QCOMPARE(ItemColourLook::effective("Clock.timeColour", time), time);

        WorkspacePreview::setEnabledForTest(true);
        QCOMPARE(ItemColourLook::effective("Clock.timeColour", time), role("measured"));
        QCOMPARE(ItemColourLook::effective("Clock.dateColour", date), role("text-secondary"));
        clock.setTimeColour(QColor(0x40, 0xa0, 0xff));
        QCOMPARE(ItemColourLook::effective("Clock.timeColour", clock.timeColour()), QColor(0x40, 0xa0, 0xff));
    }

    void ledFollowsTheLook()
    {
        LEDItem led;
        led.applyNewItemDefaults();
        const QColor on = led.trueColour();
        const QColor off = led.falseColour();
        QCOMPARE(ItemColourLook::effective("Led.trueColour", on), on);

        WorkspacePreview::setEnabledForTest(true);
        QCOMPARE(ItemColourLook::effective("Led.trueColour", on), role("live"));
        QCOMPARE(ItemColourLook::effective("Led.falseColour", off), role("border-muted"));
        const QColor user(0xff, 0x00, 0xff);
        QCOMPARE(ItemColourLook::effective("Led.trueColour", user), user);
    }

    void containerChromeFollowsTheLook()
    {
        const QColor classic(0x0f, 0x0f, 0x1a);
        const QColor user(0x30, 0x10, 0x10);
        QCOMPARE(ContainerWidget::contentBackground(classic), classic);
        QCOMPARE(ContainerWidget::contentBackground(user), user);

        WorkspacePreview::setEnabledForTest(true);
        QCOMPARE(ContainerWidget::contentBackground(classic), role("panel"));
        QCOMPARE(ContainerWidget::contentBackground(user), user);
    }

    // Saved records never change: the record of every registry item is the
    // same byte for byte with the look on and off, before and after it
    // paints, and a stored record restores to the same bytes.
    void recordsRoundTripByteForByte()
    {
        ContainerContentRegistry registry;
        for (const ContentDescriptor& descriptor : registry.descriptors()) {
            const ContentEntry entry = registry.makeEntry(descriptor.typeId);
            WorkspacePreview::setEnabledForTest(false);
            std::unique_ptr<MeterItem> off(registry.createMeterItem(entry, nullptr, ContentRenderMode::Validation));
            if (!off) {
                continue;
            }
            const QByteArray classic = recordOf(registry, *off, entry);
            paintOnce(*off);
            QCOMPARE(recordOf(registry, *off, entry), classic);

            WorkspacePreview::setEnabledForTest(true);
            const ContentEntry stored = registry.captureMeterItem(*off, entry);
            std::unique_ptr<MeterItem> on(registry.createMeterItem(stored, nullptr, ContentRenderMode::Validation));
            QVERIFY2(on != nullptr, qPrintable(descriptor.typeId));
            paintOnce(*on);
            QVERIFY2(recordOf(registry, *on, stored) == classic, qPrintable(descriptor.typeId));
        }
    }

    // A user's colour round-trips byte for byte with the look on and off.
    void userColourRoundTripsByteForByte()
    {
        ClockItem clock;
        clock.applyNewItemDefaults();
        clock.setTimeColour(QColor(0x40, 0xa0, 0xff));
        const QString saved = clock.serialize();
        for (bool look : {false, true}) {
            WorkspacePreview::setEnabledForTest(look);
            ClockItem restored;
            QVERIFY(restored.deserialize(saved));
            paintOnce(restored);
            QCOMPARE(restored.serialize(), saved);
            QCOMPARE(restored.timeColour(), QColor(0x40, 0xa0, 0xff));
        }
    }

    // The editor's swatch shows the colour drawn, with the note while the
    // field follows the look; a pick stores the user's colour.
    void editorSwatchShowsTheEffectiveColour()
    {
        WorkspacePreview::setEnabledForTest(true);
        ClockItem clock;
        clock.applyNewItemDefaults();
        const QColor classicTitle = clock.typeTitleColour();
        ClockItemEditor editor;
        editor.setItem(&clock);
        auto* swatch = editor.findChild<QPushButton*>(QStringLiteral("clockTitleColour"));
        QVERIFY(swatch != nullptr);
        QVERIFY2(swatch->styleSheet().contains(role("text-secondary").name(QColor::HexArgb)),
                 qPrintable(swatch->styleSheet()));
        QCOMPARE(swatch->toolTip(), ItemColourLook::followsLookNote());
        QCOMPARE(clock.typeTitleColour(), classicTitle);

        // Pick a colour in the dialog the swatch opens.
        const QColor user(0x33, 0x99, 0x66);
        bool answered = false;
        QTimer timer;
        timer.setInterval(20);
        connect(&timer, &QTimer::timeout, this, [&]() {
            if (auto* dialog = qobject_cast<QColorDialog*>(QApplication::activeModalWidget())) {
                dialog->setCurrentColor(user);
                dialog->accept();
                answered = true;
                timer.stop();
            }
        });
        timer.start();
        swatch->click();
        QVERIFY(answered);
        QCOMPARE(clock.typeTitleColour().rgb(), user.rgb());
        QVERIFY2(swatch->styleSheet().contains(clock.typeTitleColour().name(QColor::HexArgb)),
                 qPrintable(swatch->styleSheet()));
        QVERIFY(swatch->toolTip().isEmpty());

        // The look off: the swatch shows the stored colour, without the note.
        WorkspacePreview::setEnabledForTest(false);
        ClockItem classic;
        classic.applyNewItemDefaults();
        editor.setItem(&classic);
        QVERIFY(swatch->styleSheet().contains(classicTitle.name(QColor::HexArgb)));
        QVERIFY(swatch->toolTip().isEmpty());
    }
};

QTEST_MAIN(TestMeterItemColourLook)
#include "tst_meter_item_colour_look.moc"
