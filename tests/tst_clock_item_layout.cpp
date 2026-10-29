// no-port-check: NereusSDR-original tests for the CLOCK item layouts.
// Modification history (NereusSDR):
//   2026-10-26 - New clocks take Thetis's look and layout; saved clocks
//                 keep theirs; the large time layout splits am / pm and
//                 pads a short time; the title colour row follows the
//                 title. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.

#include <QtTest>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QImage>
#include <QPainter>
#include <QPushButton>
#include <QTimeZone>
#include <memory>

#include "core/UnbuiltFeatureList.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/meter_property_editors/ClockItemEditor.h"
#include "gui/meters/ClockItem.h"

using namespace NereusSDR;

namespace {

QDateTime fixedUtc(int hour, int minute, int second)
{
    return QDateTime(QDate(2026, 10, 26), QTime(hour, minute, second), QTimeZone::UTC);
}

QStringList runTexts(const QVector<ClockItem::TextRun>& runs)
{
    QStringList texts;
    for (const ClockItem::TextRun& run : runs) { texts.append(run.text); }
    return texts;
}

const ClockItem::TextRun* findRun(const QVector<ClockItem::TextRun>& runs, const QString& text)
{
    for (const ClockItem::TextRun& run : runs) {
        if (run.text == text) { return &run; }
    }
    return nullptr;
}

} // namespace

class TstClockItemLayout : public QObject {
    Q_OBJECT

private slots:
    void init() { UnbuiltFeatures::resetForTest(); }
    void cleanup() { UnbuiltFeatures::resetForTest(); }

    // A newly placed clock takes the Thetis look.
    void newClockTakesThetisDefaults()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("CLOCK"));
        std::unique_ptr<MeterItem> made(registry.createMeterItem(entry, nullptr));
        const auto* clock = qobject_cast<const ClockItem*>(made.get());
        QVERIFY(clock);
        const ClockItemThetisDefaults& d = kClockThetisDefaults;
        QCOMPARE(clock->layout(), ClockItem::Layout::Thetis);
        QCOMPARE(d.layout, ClockItem::Layout::Thetis);
        QCOMPARE(clock->timeColour(), QColor(255, 165, 0));   // Color.Orange
        QCOMPARE(clock->dateColour(), QColor(128, 128, 128)); // Color.Gray
        QCOMPARE(clock->typeTitleColour(), QColor(128, 128, 128));
        QCOMPARE(clock->timeColour(), QColor::fromRgb(d.timeColour));
        QCOMPARE(clock->dateColour(), QColor::fromRgb(d.dateColour));
        QCOMPARE(clock->typeTitleColour(), QColor::fromRgb(d.typeTitleColour));
        QVERIFY(clock->show24Hour());
        QVERIFY(clock->showType());
        QCOMPARE(clock->backColour(), QColor(32, 32, 32));
        QCOMPARE(clock->backColour(), QColor::fromRgb(d.backColour));
        QVERIFY(clock->backEnabled());
        QCOMPARE(QString::fromLatin1(d.fontFamily), QStringLiteral("Trebuchet MS"));
        QCOMPARE(d.fontSize, 18.0f);
    }

    // A record saved before the layout existed keeps the NereusSDR layout,
    // its colours, and no background fill.
    void oldRecordKeepsNereusLayout()
    {
        const QString oldRecord = QStringLiteral(
            "CLOCK|0.1|0.2|0.3|0.4|5|6|0|1|#ff112233|#ff445566|#ff778899");
        ClockItem clock(nullptr, false);
        QVERIFY(clock.deserialize(oldRecord));
        QCOMPARE(clock.layout(), ClockItem::Layout::NereusSDR);
        QCOMPARE(clock.timeColour(), QColor(0x11, 0x22, 0x33));
        QCOMPARE(clock.dateColour(), QColor(0x44, 0x55, 0x66));
        QCOMPARE(clock.typeTitleColour(), QColor(0x77, 0x88, 0x99));
        QVERIFY(!clock.backEnabled());
        const ClockItem fresh(nullptr, false);
        QCOMPARE(clock.backColour(), fresh.backColour());
        // Placement aside, the saved fields come back as they were.
        const QStringList oldParts = oldRecord.split(QLatin1Char('|'));
        const QStringList newParts = clock.serialize().split(QLatin1Char('|'));
        QCOMPARE(newParts.mid(7, oldParts.size() - 7), oldParts.mid(7));
        QCOMPARE(newParts.size(), 15);

        // The constructor (presets, old records) keeps the NereusSDR look.
        QCOMPARE(fresh.layout(), ClockItem::Layout::NereusSDR);
        QCOMPARE(fresh.timeColour(), QColor(0xc8, 0xd8, 0xe8));
    }

    void newFieldsRoundTrip()
    {
        ClockItem clock(nullptr, false);
        clock.setLayout(ClockItem::Layout::Thetis);
        clock.setBackColour(QColor(1, 2, 3, 200));
        clock.setBackEnabled(true);
        ClockItem reloaded(nullptr, false);
        QVERIFY(reloaded.deserialize(clock.serialize()));
        QCOMPARE(reloaded.layout(), ClockItem::Layout::Thetis);
        QCOMPARE(reloaded.backColour(), QColor(1, 2, 3, 200));
        QVERIFY(reloaded.backEnabled());
        QCOMPARE(reloaded.serialize(), clock.serialize());
    }

    // 09:05:07, 12 hour: "9:05:07" and "am" are separate runs, and the
    // seven character time is padded by 3% of the width.
    void twelveHourSplitsAmPmAndPads()
    {
        ClockItem clock(nullptr, false);
        clock.applyNewItemDefaults();
        clock.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        clock.setShow24Hour(false);
        clock.setTimeSource([] { return fixedUtc(9, 5, 7); });
        constexpr int kW = 520;
        constexpr int kH = 100;
        const QVector<ClockItem::TextRun> runs = clock.thetisRuns(kW, kH);
        const QStringList texts = runTexts(runs);
        QCOMPARE(texts.count(QStringLiteral("9:05:07")), 2);
        QCOMPARE(texts.count(QStringLiteral("am")), 2);
        QCOMPARE(texts.count(QStringLiteral("local")), 1);
        QCOMPARE(texts.count(QStringLiteral("utc")), 1);
        QCOMPARE(texts.count(QStringLiteral("Mon 26 Oct 2026")), 2);

        const ClockItem::TextRun* time = findRun(runs, QStringLiteral("9:05:07"));
        QVERIFY(time);
        // x + w*0.12 + pad (w*0.03); y + h*0.02.
        QVERIFY(qAbs(time->topLeft.x() - (kW * 0.12 + kW * 0.03)) < 0.01);
        QVERIFY(qAbs(time->topLeft.y() - kH * 0.02) < 0.01);
        const double em = kW / 52.0 / 16.0;
        QVERIFY(qAbs(time->fontSize - 18.0 * 1.9 * em) < 0.01);
        QCOMPARE(time->colour, QColor(255, 165, 0));

        const ClockItem::TextRun* amPm = findRun(runs, QStringLiteral("am"));
        QVERIFY(amPm);
        // No pad on am / pm: x + w*0.12 + w*0.228, y + h*0.02 + h*0.285.
        QVERIFY(qAbs(amPm->topLeft.x() - (kW * 0.12 + kW * 0.228)) < 0.01);
        QVERIFY(qAbs(amPm->topLeft.y() - (kH * 0.02 + kH * 0.285)) < 0.01);
        QVERIFY(qAbs(amPm->fontSize - 18.0 * 0.8 * em) < 0.01);

        const ClockItem::TextRun* local = findRun(runs, QStringLiteral("local"));
        const ClockItem::TextRun* utc = findRun(runs, QStringLiteral("utc"));
        QVERIFY(local && utc);
        QVERIFY(qAbs(local->topLeft.x() - kW * 0.01) < 0.01);
        QVERIFY(qAbs(utc->topLeft.x() - kW * 0.52) < 0.01);
        QVERIFY(qAbs(local->topLeft.y() - kH * 0.03) < 0.01);
        QVERIFY(qAbs(utc->topLeft.y() - kH * 0.03) < 0.01);
    }

    // An eight character 12 hour time takes no pad; 24 hour has no am / pm.
    void padOnlyForSevenCharacters()
    {
        ClockItem clock(nullptr, false);
        clock.applyNewItemDefaults();
        clock.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        clock.setShow24Hour(false);
        clock.setTimeSource([] { return fixedUtc(22, 5, 7); });
        const QVector<ClockItem::TextRun> runs = clock.thetisRuns(520, 100);
        const ClockItem::TextRun* time = findRun(runs, QStringLiteral("10:05:07"));
        QVERIFY(time);
        QVERIFY(qAbs(time->topLeft.x() - 520 * 0.12) < 0.01);
        QVERIFY(findRun(runs, QStringLiteral("pm")));

        clock.setShow24Hour(true);
        clock.setTimeSource([] { return fixedUtc(9, 5, 7); });
        const QVector<ClockItem::TextRun> runs24 = clock.thetisRuns(520, 100);
        QVERIFY(findRun(runs24, QStringLiteral("09:05:07")));
        QVERIFY(!findRun(runs24, QStringLiteral("am")));
        QVERIFY(!findRun(runs24, QStringLiteral("pm")));
    }

    void hiddenTitleDrawsNoTitle()
    {
        ClockItem clock(nullptr, false);
        clock.applyNewItemDefaults();
        clock.setShowType(false);
        clock.setTimeSource([] { return fixedUtc(9, 5, 7); });
        const QStringList texts = runTexts(clock.thetisRuns(520, 100));
        QVERIFY(!texts.contains(QStringLiteral("local")));
        QVERIFY(!texts.contains(QStringLiteral("utc")));
    }

    // The background halves fill in the back colour; disabled, nothing does.
    void backgroundFillsTwoHalves()
    {
        ClockItem clock(nullptr, false);
        clock.applyNewItemDefaults();
        clock.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        clock.setTimeColour(Qt::transparent);
        clock.setDateColour(Qt::transparent);
        clock.setTypeTitleColour(Qt::transparent);
        clock.setTimeSource([] { return fixedUtc(9, 5, 7); });
        QImage image(2000, 60, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::black);
        {
            QPainter p(&image);
            clock.paint(p, image.width(), image.height());
        }
        QCOMPARE(QColor(image.pixel(10, 30)), QColor(32, 32, 32));
        QCOMPARE(QColor(image.pixel(1990, 30)), QColor(32, 32, 32));
        // The gap between the halves stays unfilled.
        QCOMPARE(QColor(image.pixel(1000, 30)), QColor(Qt::black));

        clock.setBackEnabled(false);
        image.fill(Qt::black);
        {
            QPainter p(&image);
            clock.paint(p, image.width(), image.height());
        }
        QCOMPARE(QColor(image.pixel(10, 30)), QColor(Qt::black));
    }

    // setup.cs updateTitleControlsClock: the title colour row is enabled
    // only while the title is shown.
    void titleColourRowFollowsTitle()
    {
        ClockItem clock(nullptr, false);
        clock.setShowType(false);
        ClockItemEditor editor;
        editor.setItem(&clock);
        auto* title = editor.findChild<QPushButton*>(QStringLiteral("clockTitleColour"));
        auto* show = editor.findChild<QCheckBox*>(QStringLiteral("clockShowType"));
        QVERIFY(title && show);
        QVERIFY(!title->isEnabled());
        show->setChecked(true);
        QVERIFY(title->isEnabled());
        QVERIFY(clock.showType());
        show->setChecked(false);
        QVERIFY(!title->isEnabled());

        auto* layout = editor.findChild<QComboBox*>(QStringLiteral("clockLayout"));
        QVERIFY(layout);
        QCOMPARE(layout->currentData().toInt(), int(ClockItem::Layout::NereusSDR));
        layout->setCurrentIndex(layout->findData(int(ClockItem::Layout::Thetis)));
        QCOMPARE(clock.layout(), ClockItem::Layout::Thetis);

        auto* back = editor.findChild<QCheckBox*>(QStringLiteral("clockBackEnabled"));
        auto* backColour = editor.findChild<QPushButton*>(QStringLiteral("clockBackColour"));
        QVERIFY(back && backColour);
        QVERIFY(!back->isChecked());
        QVERIFY(!backColour->isEnabled());
        back->setChecked(true);
        QVERIFY(clock.backEnabled());
        QVERIFY(backColour->isEnabled());
    }
};

QTEST_MAIN(TstClockItemLayout)
#include "tst_clock_item_layout.moc"
