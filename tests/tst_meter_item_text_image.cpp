// no-port-check: NereusSDR-original tests for the TEXT and IMAGE building
// blocks (Thetis clsText tokens and centring, clsImage dark and size
// variants); no Thetis code is copied here.
// Modification history (NereusSDR):
//   2026-10-26 - TextItem tokens, font and centring; ImageItem variant
//                 lookup and dark mode; new-item defaults. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.

#include <QtTest>
#include <QColor>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>
#include <memory>

#include "gui/containers/ContainerContentRegistry.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"

using namespace NereusSDR;

namespace {

// TEXT and IMAGE records as NereusSDR saved them before this change.
const QString kSavedTextRecord =
    QStringLiteral("TEXT|0.1|0.2|0.3|0.4|-1|2|Hello|#ffc8d8e8|13|1| dBm|1");
const QString kSavedImageRecord =
    QStringLiteral("IMAGE|0|0|1|1|-1|0|/no/such/meter.png");

// Bounding box of pixels brighter than the black background.
QRect litBounds(const QImage& img)
{
    QRect box;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            if (qGray(img.pixel(x, y)) > 128) {
                box = box.united(QRect(x, y, 1, 1));
            }
        }
    }
    return box;
}

QImage renderText(TextItem& item, int w, int h)
{
    QImage img(w, h, QImage::Format_ARGB32);
    img.fill(Qt::black);
    QPainter p(&img);
    item.paint(p, w, h);
    p.end();
    return img;
}

bool writeImage(const QString& path, const QColor& colour)
{
    QImage img(8, 8, QImage::Format_ARGB32);
    img.fill(colour);
    return img.save(path);
}

} // namespace

class TestMeterItemTextImage : public QObject {
    Q_OBJECT
private slots:
    // --- TEXT ---------------------------------------------------------

    void valueTokenShowsTheReading()
    {
        TextItem t;
        t.setBindingId(MeterBinding::TxPower);
        t.setLabel(QStringLiteral("%value%"));
        t.setValue(12.34);
        QCOMPARE(t.displayText(), QStringLiteral("12.3"));
    }

    void valueWithTypeTokenAddsTheUnit()
    {
        TextItem t;
        t.setBindingId(MeterBinding::TxPower);
        t.setLabel(QStringLiteral("%valueWithType%"));
        t.setValue(12.34);
        QCOMPARE(t.displayText(), QStringLiteral("12.3W"));

        t.setBindingId(MeterBinding::SignalAvg);
        t.setValue(-73.4);
        QCOMPARE(t.displayText(), QStringLiteral("-73.4dBm"));

        t.setBindingId(MeterBinding::TxSwr);
        t.setValue(1.5);
        QCOMPARE(t.displayText(), QStringLiteral("1.5:1"));
    }

    void valueTokenWithNoReadingShowsDashes()
    {
        TextItem t;
        t.setBindingId(MeterBinding::SignalPeak);
        t.setLabel(QStringLiteral("%valueWithType%"));
        t.setValue(kNoMeterReadingDbm);
        QCOMPARE(t.displayText(), QStringLiteral("--dBm"));
    }

    void tokensAreCaseSensitiveExceptGroup()
    {
        TextItem t;
        t.setBindingId(MeterBinding::TxPower);
        t.setSuffix(QStringLiteral(" W"));
        t.setValue(5.0);
        // Thetis matches %value% exactly; anything else is plain text,
        // which a bound item shows as its value and suffix.
        t.setLabel(QStringLiteral("%VALUE%"));
        QCOMPARE(t.displayText(), QStringLiteral("5.0 W"));

        t.setDisplayGroupText(QStringLiteral("PWR/SWR"));
        t.setLabel(QStringLiteral("%GROUP%"));
        QCOMPARE(t.displayText(), QStringLiteral("PWR/SWR"));
        t.setLabel(QStringLiteral("%group%"));
        QCOMPARE(t.displayText(), QStringLiteral("PWR/SWR"));
    }

    void plainLabelUnchanged()
    {
        TextItem t;
        t.setLabel(QStringLiteral("Hello"));
        QCOMPARE(t.displayText(), QStringLiteral("Hello"));
    }

    void centringCentresTheText()
    {
        TextItem t;
        t.setLabel(QStringLiteral("AB"));
        t.setTextColor(Qt::white);
        t.setFontSize(12);
        t.setRect(0.0f, 0.0f, 1.0f, 1.0f);

        t.setCentred(true);
        const QRect centred = litBounds(renderText(t, 400, 100));
        QVERIFY(!centred.isEmpty());
        QVERIFY2(std::abs(centred.center().x() - 200) <= 10,
                 qPrintable(QStringLiteral("centred x %1").arg(centred.center().x())));

        t.setCentred(false);
        const QRect topLeft = litBounds(renderText(t, 400, 100));
        QVERIFY(!topLeft.isEmpty());
        QVERIFY2(topLeft.left() < 40,
                 qPrintable(QStringLiteral("left %1").arg(topLeft.left())));
        QVERIFY(topLeft.center().x() < centred.center().x() - 100);
    }

    void textFieldsRoundTrip()
    {
        TextItem t;
        t.setLabel(QStringLiteral("%valueWithType%"));
        t.setFontFamily(QStringLiteral("Trebuchet MS"));
        t.setItalic(true);
        t.setCentred(false);
        TextItem u;
        QVERIFY(u.deserialize(t.serialize()));
        QCOMPARE(u.label(), QStringLiteral("%valueWithType%"));
        QCOMPARE(u.fontFamily(), QStringLiteral("Trebuchet MS"));
        QVERIFY(u.italic());
        QVERIFY(!u.centred());
        QCOMPARE(u.serialize(), t.serialize());
    }

    void savedTextLooksTheSame()
    {
        TextItem t;
        QVERIFY(t.deserialize(kSavedTextRecord));
        QVERIFY(t.fontFamily().isEmpty());
        QVERIFY(!t.italic());
        QVERIFY(t.centred());
        QVERIFY(t.bold());
        QCOMPARE(t.fontSize(), 13);
        QCOMPARE(t.serialize(), kSavedTextRecord + QStringLiteral("||0|1"));

        ContainerContentRegistry registry;
        ContentEntry entry;
        entry.id = QStringLiteral("saved-text");
        entry.typeId = QStringLiteral("TEXT");
        entry.name = QStringLiteral("Text");
        entry.config.insert(QStringLiteral("legacyRecord"), kSavedTextRecord);
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* text = qobject_cast<TextItem*>(item.get());
        QVERIFY(text != nullptr);
        QVERIFY(text->centred());
        QVERIFY(text->fontFamily().isEmpty());
        QCOMPARE(text->textColor(), QColor(0xc8, 0xd8, 0xe8));
    }

    void newTextTakesThetisDefaults()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("TEXT"));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* text = qobject_cast<TextItem*>(item.get());
        QVERIFY(text != nullptr);
        QCOMPARE(text->fontFamily(), QStringLiteral("Trebuchet MS"));
        QVERIFY(!text->bold());
        QVERIFY(!text->italic());
        QCOMPARE(text->textColor(), QColor(Qt::white));
        QVERIFY(!text->centred());
        QCOMPARE(text->fontSize(), 9);
        QCOMPARE(text->fontSize(), kTextThetisDefaults.fontSize);
    }

    // --- IMAGE --------------------------------------------------------

    void variantPathPrefersDarkOnlyWhenItExists()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString base = dir.filePath(QStringLiteral("meter.png"));
        QVERIFY(writeImage(base, Qt::red));

        // Dark mode with no -dark file: the base file.
        QCOMPARE(ImageItem::variantPath(base, true, 800.0), base);

        const QString dark = dir.filePath(QStringLiteral("meter-dark.png"));
        QVERIFY(writeImage(dark, Qt::blue));
        QCOMPARE(ImageItem::variantPath(base, true, 800.0), dark);
        QCOMPARE(ImageItem::variantPath(base, false, 800.0), base);
    }

    void variantPathPicksSizeByDiagonal()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString base = dir.filePath(QStringLiteral("meter.png"));
        QVERIFY(writeImage(base, Qt::red));

        // No -small file yet.
        QCOMPARE(ImageItem::variantPath(base, false, 400.0), base);

        const QString small = dir.filePath(QStringLiteral("meter-small.png"));
        const QString large = dir.filePath(QStringLiteral("meter-large.png"));
        QVERIFY(writeImage(small, Qt::green));
        QVERIFY(writeImage(large, Qt::yellow));
        QCOMPARE(ImageItem::variantPath(base, false, 400.0), small);
        QCOMPARE(ImageItem::variantPath(base, false, 450.0), small);
        QCOMPARE(ImageItem::variantPath(base, false, 451.0), base);
        QCOMPARE(ImageItem::variantPath(base, false, 1199.0), base);
        QCOMPARE(ImageItem::variantPath(base, false, 1200.0), large);

        // Dark first, then the size of the dark file, as renderImage does.
        const QString dark = dir.filePath(QStringLiteral("meter-dark.png"));
        QVERIFY(writeImage(dark, Qt::blue));
        QCOMPARE(ImageItem::variantPath(base, true, 400.0), dark);
        const QString darkSmall = dir.filePath(QStringLiteral("meter-dark-small.png"));
        QVERIFY(writeImage(darkSmall, Qt::cyan));
        QCOMPARE(ImageItem::variantPath(base, true, 400.0), darkSmall);
    }

    void variantPathOfEmptyPathIsEmpty()
    {
        QVERIFY(ImageItem::variantPath(QString(), true, 100.0).isEmpty());
    }

    void paintDrawsTheVariant()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString base = dir.filePath(QStringLiteral("meter.png"));
        QVERIFY(writeImage(base, Qt::red));
        QVERIFY(writeImage(dir.filePath(QStringLiteral("meter-dark.png")), Qt::blue));

        ImageItem img;
        img.setImagePath(base);
        img.setRect(0.0f, 0.0f, 1.0f, 1.0f);

        auto centrePixel = [&img]() {
            QImage out(200, 200, QImage::Format_ARGB32);
            out.fill(Qt::black);
            QPainter p(&out);
            img.paint(p, 200, 200);
            p.end();
            return QColor(out.pixel(100, 100));
        };
        QCOMPARE(centrePixel(), QColor(Qt::red));
        img.setDarkMode(true);
        QCOMPARE(centrePixel(), QColor(Qt::blue));
        img.setDarkMode(false);
        QCOMPARE(centrePixel(), QColor(Qt::red));
    }

    void darkModeRoundTripsAndOldRecordsAreNotDark()
    {
        ImageItem img;
        QVERIFY(img.deserialize(kSavedImageRecord));
        QVERIFY(!img.darkMode());
        QCOMPARE(img.serialize(), kSavedImageRecord + QStringLiteral("|0"));

        img.setDarkMode(true);
        ImageItem copy;
        QVERIFY(copy.deserialize(img.serialize()));
        QVERIFY(copy.darkMode());
    }

    void newImageTakesThetisDefaults()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("IMAGE"));
        std::unique_ptr<MeterItem> item(registry.createMeterItem(entry, nullptr));
        auto* img = qobject_cast<ImageItem*>(item.get());
        QVERIFY(img != nullptr);
        QCOMPARE(img->darkMode(), kImageThetisDefaults.darkMode);
        QVERIFY(!img->darkMode());
    }
};

QTEST_MAIN(TestMeterItemTextImage)
#include "tst_meter_item_text_image.moc"
