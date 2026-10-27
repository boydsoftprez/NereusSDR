// tests/tst_accessory_readouts.cpp (NereusSDR)
//
// no-port-check: NereusSDR-original test file.
//
// The readouts of the tuner and amplifier panels (AccessoryReadouts): the
// four scales and limits, the text rules, reflected power from forward
// power and SWR, and BarInstrument fed a reading of the caller's own.

#include "gui/applets/AccessoryReadouts.h"
#include "gui/instruments/BarInstrument.h"
#include "gui/instruments/InstrumentFooter.h"
#include "gui/instruments/ReadingSource.h"
#include "gui/meters/MeterPoller.h"

#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QtTest/QtTest>

using namespace NereusSDR;

namespace {

QStringList tickLabels(const ReadingDescriptor& d)
{
    QStringList out;
    for (const ReadingTick& t : d.ticks) { out << t.label; }
    return out;
}

QList<double> tickValues(const ReadingDescriptor& d)
{
    QList<double> out;
    for (const ReadingTick& t : d.ticks) { out << t.value; }
    return out;
}

// The footer's three labels in the order it creates them: name, the peak
// and limit text, value.
QList<QLabel*> footerLabels(BarInstrument& bar)
{
    auto* footer = bar.findChild<InstrumentFooter*>();
    return footer ? footer->findChildren<QLabel*>() : QList<QLabel*>();
}

QImage render(BarInstrument& bar, QSize size)
{
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::black);
    QPainter p(&img);
    bar.paintInto(p, size, /*bare=*/false);
    return img;
}

} // namespace

class TstAccessoryReadouts : public QObject {
    Q_OBJECT
private slots:
    void wholeWatts();
    void swrTextForms();
    void wholeDegrees();
    void reflectedFromForwardAndSwr();
    void forwardScales();
    void reflectedScale();
    void swrScale();
    void temperatureScale();
    void rowNames();
    void ownReadingShowsItsValueAndName();
    void ownReadingKeepsItsValueThroughAScaleChange();
    void clearValueShowsDashes();
    void clearValueResetsThePeak();
    void compactNoteIsDrawn();
    void compactValueSizeIsUsed();
    void footerDetailCanBeLeftOff();
    void boundReadingIsUnchanged();
    void footerKeepsItsFontSizesUnderAHostRule();
    void makersSetTheSizes();
};

void TstAccessoryReadouts::wholeWatts()
{
    const ReadingDescriptor fwd = AccessoryReadouts::forwardPower();
    QCOMPARE(fwd.textWithUnit(1236.0), QStringLiteral("1236 W"));
    QCOMPARE(fwd.textWithUnit(1235.6), QStringLiteral("1236 W"));
    QCOMPARE(fwd.textWithUnit(0.0), QStringLiteral("0 W"));
    const ReadingDescriptor ref = AccessoryReadouts::reflectedPower();
    QCOMPARE(ref.textWithUnit(3.96), QStringLiteral("4 W"));
}

void TstAccessoryReadouts::swrTextForms()
{
    QCOMPARE(AccessoryReadouts::swrText(1.12), QStringLiteral("1.12"));
    QCOMPARE(AccessoryReadouts::swrText(1.0), QStringLiteral("1.00"));
    QCOMPARE(AccessoryReadouts::swrText(9.994), QStringLiteral("9.99"));
    QCOMPARE(AccessoryReadouts::swrText(9.995), QStringLiteral("10.0"));
    QCOMPARE(AccessoryReadouts::swrText(12.4), QStringLiteral("12.4"));
    QCOMPARE(AccessoryReadouts::swrText(99.94), QStringLiteral("99.9"));
    QCOMPARE(AccessoryReadouts::swrText(99.95), QStringLiteral(">99"));
    QCOMPARE(AccessoryReadouts::swrText(999.0), QStringLiteral(">99"));
    // The descriptor writes SWR by that rule and adds no unit.
    const ReadingDescriptor d = AccessoryReadouts::swr();
    QCOMPARE(d.textWithUnit(1.12), QStringLiteral("1.12"));
    QCOMPARE(d.textWithUnit(12.4), QStringLiteral("12.4"));
    QCOMPARE(d.textWithUnit(150.0), QStringLiteral(">99"));
}

void TstAccessoryReadouts::wholeDegrees()
{
    const ReadingDescriptor d = AccessoryReadouts::temperature();
    QCOMPARE(d.textWithUnit(41.0), QStringLiteral("41 °C"));
    QCOMPARE(d.textWithUnit(42.5), QString::number(42.5, 'f', 0) + QStringLiteral(" °C"));
    QCOMPARE(d.textWithUnit(38.4), QStringLiteral("38 °C"));
}

void TstAccessoryReadouts::reflectedFromForwardAndSwr()
{
    QCOMPARE(AccessoryReadouts::reflectedWatts(1000.0, 1.0), 0.0);
    QCOMPARE(AccessoryReadouts::reflectedWatts(1000.0, 3.0), 250.0);
    QCOMPARE(AccessoryReadouts::reflectedWatts(1000.0, 99.0), 1000.0);
    QCOMPARE(AccessoryReadouts::reflectedWatts(1000.0, 999.0), 1000.0);
    // Just under the cap it is still the formula: (98/100)^2.
    QVERIFY(qAbs(AccessoryReadouts::reflectedWatts(1000.0, 98.999) - 960.4) < 0.1);
    // The approved picture's keyed state: 1236 W at 1.12 is 4 W.
    QCOMPARE(qRound(AccessoryReadouts::reflectedWatts(1236.0, 1.12)), 4);
    // Nothing forward, or an SWR below 1 (a device's idle junk), is none.
    QCOMPARE(AccessoryReadouts::reflectedWatts(0.0, 3.0), 0.0);
    QCOMPARE(AccessoryReadouts::reflectedWatts(1000.0, 0.0), 0.0);
    QCOMPARE(AccessoryReadouts::reflectedWatts(1000.0, -24.5), 0.0);
}

void TstAccessoryReadouts::forwardScales()
{
    const ReadingDescriptor amp = AccessoryReadouts::forwardPower();
    QVERIFY(amp.hasScale);
    QCOMPARE(amp.min, 0.0);
    QCOMPARE(amp.max, 2000.0);
    QCOMPARE(amp.threshold.value(), 1500.0);
    QCOMPARE(tickValues(amp), (QList<double>{0.0, 500.0, 1000.0, 1500.0, 2000.0}));
    QCOMPARE(tickLabels(amp), (QStringList{"0", "500", "1000", "1.5k", "2k"}));
    QCOMPARE(amp.minorTicks, (QList<double>{250.0, 750.0, 1250.0, 1750.0}));

    const ReadingDescriptor aurora = AccessoryReadouts::forwardPower(600.0, 500.0);
    QCOMPARE(aurora.max, 600.0);
    QCOMPARE(aurora.threshold.value(), 500.0);
    QCOMPARE(tickLabels(aurora), (QStringList{"0", "150", "300", "450", "600"}));

    const ReadingDescriptor barefoot = AccessoryReadouts::forwardPower(200.0, 125.0);
    QCOMPARE(barefoot.max, 200.0);
    QCOMPARE(barefoot.threshold.value(), 125.0);
    QCOMPARE(tickLabels(barefoot), (QStringList{"0", "50", "100", "150", "200"}));
}

void TstAccessoryReadouts::reflectedScale()
{
    const ReadingDescriptor d = AccessoryReadouts::reflectedPower();
    QVERIFY(d.hasScale);
    QCOMPARE(d.min, 0.0);
    QCOMPARE(d.max, 500.0);
    QVERIFY(!d.threshold.has_value());
    QCOMPARE(tickLabels(d), (QStringList{"0", "125", "250", "375", "500"}));
    QCOMPARE(tickValues(d), (QList<double>{0.0, 125.0, 250.0, 375.0, 500.0}));
}

void TstAccessoryReadouts::swrScale()
{
    const ReadingDescriptor d = AccessoryReadouts::swr();
    QVERIFY(d.hasScale);
    QCOMPARE(d.min, 1.0);
    QCOMPARE(d.max, 3.0);
    QCOMPARE(d.threshold.value(), 2.5);
    QCOMPARE(tickLabels(d), (QStringList{"1", "1.5", "2", "2.5", "3"}));
    QCOMPARE(tickValues(d), (QList<double>{1.0, 1.5, 2.0, 2.5, 3.0}));
    QCOMPARE(d.minorTicks, (QList<double>{1.25, 1.75, 2.25, 2.75}));
    QVERIFY(d.unit.isEmpty());
}

void TstAccessoryReadouts::temperatureScale()
{
    const ReadingDescriptor d = AccessoryReadouts::temperature();
    QVERIFY(d.hasScale);
    QCOMPARE(d.min, 0.0);
    QCOMPARE(d.max, 100.0);
    QCOMPARE(d.threshold.value(), 80.0);
    QCOMPARE(tickLabels(d), (QStringList{"0", "30", "55", "80", "100"}));
    QCOMPARE(tickValues(d), (QList<double>{0.0, 30.0, 55.0, 80.0, 100.0}));
}

void TstAccessoryReadouts::rowNames()
{
    QCOMPARE(AccessoryReadouts::forwardName(), QStringLiteral("Forward"));
    QCOMPARE(AccessoryReadouts::reflectedName(), QStringLiteral("Reflected"));
    QCOMPARE(AccessoryReadouts::swrName(), QStringLiteral("SWR"));
    QCOMPARE(AccessoryReadouts::temperatureName(), QStringLiteral("Temp"));
}

void TstAccessoryReadouts::ownReadingShowsItsValueAndName()
{
    BarInstrument bar;
    QVERIFY(!bar.hasOwnReading());
    bar.setReading(AccessoryReadouts::forwardPower(), QStringLiteral("Forward"));
    QVERIFY(bar.hasOwnReading());
    QCOMPARE(bar.primary(), -1);
    QCOMPARE(bar.readingName(), QStringLiteral("Forward"));
    QVERIFY(!bar.hasValue());

    bar.setValue(1236.0);
    QVERIFY(bar.hasValue());
    QCOMPARE(bar.value(), 1236.0);
    const QList<QLabel*> labels = footerLabels(bar);
    QCOMPARE(labels.size(), 3);
    QCOMPARE(labels.at(0)->text(), QStringLiteral("Forward"));
    QCOMPARE(labels.at(2)->text(), QStringLiteral("1236 W"));

    // It paints: a reading of the caller's own has a scale to draw on.
    const QSize size(260, 100);
    QImage blank(size, QImage::Format_ARGB32_Premultiplied);
    blank.fill(Qt::black);
    QVERIFY(render(bar, size) != blank);
}

void TstAccessoryReadouts::ownReadingKeepsItsValueThroughAScaleChange()
{
    BarInstrument bar;
    bar.setReading(AccessoryReadouts::forwardPower(200.0, 125.0), QStringLiteral("Forward"));
    bar.setValue(98.0);
    bar.setReading(AccessoryReadouts::forwardPower(), QStringLiteral("Forward"));
    QVERIFY(bar.hasValue());
    QCOMPARE(bar.value(), 98.0);
    QCOMPARE(bar.reading()->max, 2000.0);
    QCOMPARE(footerLabels(bar).at(2)->text(), QStringLiteral("98 W"));
}

void TstAccessoryReadouts::clearValueShowsDashes()
{
    BarInstrument bar;
    bar.setReading(AccessoryReadouts::swr(), QStringLiteral("SWR"));
    bar.setValue(1.12);
    QCOMPARE(footerLabels(bar).at(2)->text(), QStringLiteral("1.12"));
    bar.clearValue();
    QVERIFY(!bar.hasValue());
    QCOMPARE(footerLabels(bar).at(2)->text(), QStringLiteral("\u2013"));

    // The compact form too: a row with a value and the row after
    // clearValue() are not the same picture.
    BarInstrument slim;
    slim.setReading(AccessoryReadouts::swr(), QStringLiteral("SWR"));
    slim.setCompact(true);
    const QSize size(260, 30);
    const QImage dashes = render(slim, size);
    slim.setValue(1.12);
    const QImage shown = render(slim, size);
    QVERIFY(shown != dashes);
    slim.clearValue();
    QCOMPARE(render(slim, size), dashes);
}

void TstAccessoryReadouts::clearValueResetsThePeak()
{
    BarInstrument bar;
    bar.setReading(AccessoryReadouts::forwardPower(), QStringLiteral("Forward"));
    bar.setValue(1310.0);
    bar.setValue(1236.0);
    QCOMPARE(bar.peakHold().value(), 1310.0);
    bar.clearValue();
    // The held peak is gone with the reading: the next value starts a new
    // one, well inside the hold time of the old.
    QCOMPARE(bar.peakHold().value(), 0.0);
    bar.setValue(100.0);
    QCOMPARE(bar.peakHold().value(), 100.0);
}

void TstAccessoryReadouts::compactNoteIsDrawn()
{
    BarInstrument bar;
    bar.setReading(AccessoryReadouts::forwardPower(), QStringLiteral("Forward"));
    bar.setCompact(true);
    bar.setValue(1236.0);
    const QSize size(260, 30);
    const QImage plain = render(bar, size);

    bar.setCompactNote(QStringLiteral("Drive"), QStringLiteral("42 W"));
    QCOMPARE(bar.compactNoteLabel(), QStringLiteral("Drive"));
    QCOMPARE(bar.compactNoteNumber(), QStringLiteral("42 W"));
    const QImage noted = render(bar, size);
    QVERIFY(noted != plain);
    // The two parts are drawn separately: a different number alone
    // changes the row.
    bar.setCompactNote(QStringLiteral("Drive"), QStringLiteral("98 W"));
    QVERIFY(render(bar, size) != noted);

    bar.setCompactNote(QString(), QString());
    QCOMPARE(render(bar, size), plain);

    // The full form does not draw it.
    BarInstrument full;
    full.setReading(AccessoryReadouts::forwardPower(), QStringLiteral("Forward"));
    full.setValue(1236.0);
    const QSize fullSize(260, 100);
    const QImage fullPlain = render(full, fullSize);
    full.setCompactNote(QStringLiteral("Drive"), QStringLiteral("42 W"));
    QCOMPARE(render(full, fullSize), fullPlain);
}

void TstAccessoryReadouts::compactValueSizeIsUsed()
{
    BarInstrument bar;
    bar.setReading(AccessoryReadouts::forwardPower(), QStringLiteral("Forward"));
    bar.setCompact(true);
    bar.setValue(1236.0);
    const QSize size(260, 30);
    QCOMPARE(bar.compactValuePx(), 0);
    const QImage look = render(bar, size);
    bar.setCompactValuePx(13);
    QCOMPARE(bar.compactValuePx(), 13);
    const QImage larger = render(bar, size);
    QVERIFY(larger != look);
    bar.setCompactValuePx(0);
    QCOMPARE(render(bar, size), look);
}

void TstAccessoryReadouts::footerDetailCanBeLeftOff()
{
    BarInstrument bar;
    bar.setReading(AccessoryReadouts::forwardPower(), QStringLiteral("Forward"));
    bar.setValue(1236.0);
    QVERIFY(bar.footerDetailVisible());
    QCOMPARE(footerLabels(bar).at(1)->text(), QStringLiteral("PK 1236 · LIM 1500"));
    bar.setFooterDetailVisible(false);
    QVERIFY(footerLabels(bar).at(1)->text().isEmpty());
    // It stays off through later readings and the no-reading state.
    bar.setValue(1300.0);
    QVERIFY(footerLabels(bar).at(1)->text().isEmpty());
    bar.clearValue();
    QVERIFY(footerLabels(bar).at(1)->text().isEmpty());
    // The name and the number are untouched.
    bar.setValue(1300.0);
    QCOMPARE(footerLabels(bar).at(0)->text(), QStringLiteral("Forward"));
    QCOMPARE(footerLabels(bar).at(2)->text(), QStringLiteral("1300 W"));
    bar.setFooterDetailVisible(true);
    QCOMPARE(footerLabels(bar).at(1)->text(), QStringLiteral("PK 1300 · LIM 1500"));
}

void TstAccessoryReadouts::boundReadingIsUnchanged()
{
    // A reading from the binding table still shows its Thetis name and
    // takes its values through onReading(); setPrimary() after
    // setReading() goes back to the table.
    BarInstrument bar;
    QVERIFY(bar.setPrimary(MeterBinding::TxSwr));
    QVERIFY(!bar.hasOwnReading());
    const ReadingDescriptor* table = readingFor(MeterBinding::TxSwr);
    QVERIFY(table != nullptr);
    QCOMPARE(bar.reading(), table);
    QCOMPARE(bar.readingName(), table->thetisName());
    bar.onReading(MeterBinding::TxSwr, 1.5);
    QVERIFY(bar.hasValue());
    QCOMPARE(bar.value(), 1.5);
    QCOMPARE(footerLabels(bar).at(0)->text(), table->thetisName());
    QCOMPARE(footerLabels(bar).at(2)->text(), table->textWithUnit(1.5));

    bar.setReading(AccessoryReadouts::swr(), QStringLiteral("SWR"));
    QCOMPARE(bar.readingName(), QStringLiteral("SWR"));
    QVERIFY(bar.setPrimary(MeterBinding::TxSwr));
    QVERIFY(!bar.hasOwnReading());
    QCOMPARE(bar.reading(), table);
    QCOMPARE(bar.readingName(), table->thetisName());
    QVERIFY(!bar.hasValue());
}

void TstAccessoryReadouts::footerKeepsItsFontSizesUnderAHostRule()
{
    // A host panel's own QLabel rule (the applets carry one) must not
    // resize the footer: each label keeps the size its QFont was given.
    QWidget bare;
    auto* plain = new BarInstrument(&bare);
    plain->setReading(AccessoryReadouts::forwardPower(), QStringLiteral("Forward"));
    plain->setValue(1236.0);
    plain->ensurePolished();
    QList<int> expected;
    for (QLabel* l : footerLabels(*plain)) {
        l->ensurePolished();
        expected << l->font().pixelSize();
    }
    QCOMPARE(expected.size(), 3);
    for (int px : expected) { QVERIFY(px > 0); }

    QWidget host;
    host.setStyleSheet(QStringLiteral("QLabel { font-size: 5px; }"));
    auto* hosted = new BarInstrument(&host);
    hosted->setReading(AccessoryReadouts::forwardPower(), QStringLiteral("Forward"));
    hosted->setValue(1236.0);
    hosted->ensurePolished();
    QList<int> got;
    for (QLabel* l : footerLabels(*hosted)) {
        l->ensurePolished();
        got << QFontInfo(l->font()).pixelSize();
    }
    QCOMPARE(got, expected);
}

void TstAccessoryReadouts::makersSetTheSizes()
{
    QWidget host;
    BarInstrument* large = AccessoryReadouts::makeLargeBar(
        AccessoryReadouts::forwardPower(), AccessoryReadouts::forwardName(), &host);
    QVERIFY(!large->isCompact());
    QVERIFY(!large->footerDetailVisible());
    QVERIFY(!large->hasValue());
    QCOMPARE(large->readingName(), QStringLiteral("Forward"));
    QCOMPARE(large->minimumHeight(), large->maximumHeight());
    QCOMPARE(large->minimumHeight(), large->sizeHint().height());

    BarInstrument* slim = AccessoryReadouts::makeSlimBar(
        AccessoryReadouts::swr(), AccessoryReadouts::swrName(), &host);
    QVERIFY(slim->isCompact());
    QCOMPARE(slim->compactValuePx(), 13);
    QCOMPARE(slim->minimumHeight(), 30);
    QCOMPARE(slim->maximumHeight(), 30);
    QVERIFY(!slim->hasValue());
    QCOMPARE(slim->readingName(), QStringLiteral("SWR"));
}

QTEST_MAIN(TstAccessoryReadouts)
#include "tst_accessory_readouts.moc"
