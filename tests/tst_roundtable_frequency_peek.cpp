// no-port-check: NereusSDR-original.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// tests/tst_roundtable_frequency_peek.cpp  (NereusSDR)
// =================================================================
//
// The floating view of another frequency (overlay design D21 to D24): its words,
// the Tune gates, Your pan, how it closes, and that nothing inside it reaches the
// band underneath. The flow through MainWindow is in tst_roundtable_desktop_flow.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-03: original implementation for NereusSDR by J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QApplication>
#include <QDir>
#include <QPainter>
#include <QPushButton>
#include <QSignalSpy>
#include <QToolButton>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "gui/SpectrumWidget.h"
#include "gui/roundtable/RoundtableFrequencyPeek.h"
#include "gui/roundtable/RoundtableTagLayout.h"
#include "gui/widgets/VfoWidget.h"

using namespace NereusSDR;

namespace {

using Kind = RoundtableCardModel::Kind;
constexpr quint64 kViewedHz = 7188600;
constexpr quint64 kSliceHz = 7200000;
constexpr double kPanCenterHz = 7'200'000.0;
constexpr double kPanSpanHz = 192'000.0;
constexpr int kPixels = 193;  // 1 kHz a pixel

RoundtableCardRow rowOf(const QString& call, double dbm, qint64 ageMs = 1000)
{
    RoundtableCardRow row;
    row.call = call;
    row.dbm = dbm;
    row.ageMs = ageMs;
    row.peakDbm = dbm + 2.0;
    row.hasPeak = true;
    row.km = 2000;
    row.bearingDeg = 87;
    return row;
}

RoundtableCardModel talker()
{
    RoundtableCardModel card;
    card.kind = Kind::Talking;
    card.hz = kViewedHz;
    card.keyed = {QStringLiteral("VE3XYZ")};
    card.clockMs = 83000;
    card.onFrequency = {RoundtableCardPerson{QStringLiteral("VE3XYZ"), true, false, 0},
                        RoundtableCardPerson{QStringLiteral("W2ABC"), false, false, 20},
                        RoundtableCardPerson{QStringLiteral("N7ABC"), false, false, -40}};
    card.rows = {rowOf(QStringLiteral("W2ABC"), -68.0), rowOf(QStringLiteral("N7ABC"), -82.0, 25000)};
    card.rows[1].old = true;
    card.rows[1].hasPeak = false;
    return card;
}

RoundtableCardModel doubled()
{
    RoundtableCardModel card = talker();
    card.kind = Kind::Double;
    card.keyed = {QStringLiteral("VE3XYZ"), QStringLiteral("KD8LMN")};
    card.onFrequency = {RoundtableCardPerson{QStringLiteral("VE3XYZ"), true, false, 0},
                        RoundtableCardPerson{QStringLiteral("KD8LMN"), true, false, 60},
                        RoundtableCardPerson{QStringLiteral("W2ABC"), false, false, 0}};
    for (RoundtableCardRow& row : card.rows) {
        row.km.reset();
        row.bearingDeg.reset();
    }
    return card;
}

struct Pan {
    SpectrumWidget widget;
    RoundtableFrequencyPeek* peek;

    Pan(int height = 640)
    {
        widget.setUpdatesEnabled(false);
        widget.resize(1200, height);
        widget.setSampleRate(kPanSpanHz);
        widget.setDdcCenterFrequency(kPanCenterHz);
        widget.setFrequencyRange(kPanCenterHz, kPanSpanHz);
        widget.setConnectionState(ConnectionState::Connected);
        peek = new RoundtableFrequencyPeek(&widget);
        peek->setSlice(kSliceHz, QStringLiteral("lsb"), QStringLiteral("A"));
        widget.show();
    }
    QPushButton* tune() const { return peek->findChild<QPushButton*>(QStringLiteral("roundtablePeekTune")); }
    QPushButton* messages() const { return peek->findChild<QPushButton*>(QStringLiteral("roundtablePeekMessages")); }
    QToolButton* close() const { return peek->findChild<QToolButton*>(QStringLiteral("roundtablePeekClose")); }
};

// Whether any pixel of `rect` is `color`.
bool hasColor(const QImage& image, const QRect& rect, const QColor& color)
{
    const QRgb wanted = color.rgb();
    for (int y = rect.top(); y <= rect.bottom(); ++y) {
        for (int x = rect.left(); x <= rect.right(); ++x) {
            if ((image.pixel(x, y) & 0xffffff) == (wanted & 0xffffff)) { return true; }
        }
    }
    return false;
}

QImage render(const RoundtableFrequencyPeek& peek)
{
    QImage image(peek.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    QPainter painter(&image);
    paintRoundtableView(painter, peek.viewForTest(), QRect(QPoint(0, 0), peek.size()), peek.rowLimitForTest());
    return image;
}

} // namespace

class TestRoundtableFrequencyPeek : public QObject {
    Q_OBJECT

private slots:
    void theLineSaysHowFarAndWhichSideband()
    {
        RoundtableViewModel view;
        view.card.hz = kViewedHz;
        view.family = QStringLiteral("lsb");
        view.sliceHz = kSliceHz;
        const QChar minus(0x2212);
        QCOMPARE(roundtableViewSubText(view), QStringLiteral("Viewing, not tuned · %1 11.4 kHz from slice A · LSB")
                                                  .arg(minus).replace(QStringLiteral("%1 ").arg(minus), QString(minus)));
        view.card.hz = 7212000;
        view.family = QStringLiteral("fdvu");
        view.sliceName = QStringLiteral("B");
        QCOMPARE(roundtableViewSubText(view), QStringLiteral("Viewing, not tuned · +12.0 kHz from slice B · FreeDV U"));
        // Another family on the slice's own frequency has no distance to give.
        view.card.hz = kSliceHz;
        view.family = QStringLiteral("cw");
        QCOMPARE(roundtableViewSubText(view), QStringLiteral("Viewing, not tuned · CW"));
    }

    void offsetsAndPathsAreWrittenAsTheBoardDoes()
    {
        QCOMPARE(roundtableViewOffsetText(0), QStringLiteral("on freq"));
        QCOMPARE(roundtableViewOffsetText(20), QStringLiteral("+20 Hz"));
        QCOMPARE(roundtableViewOffsetText(-40), QString(QChar(0x2212)) + QStringLiteral("40 Hz"));
        RoundtableCardRow row = rowOf(QStringLiteral("W2ABC"), -70.0);
        QCOMPARE(roundtableViewPathText(row, true, true), QStringLiteral("%1 mi 087°").arg(QLocale().toString(1243)));
        QCOMPARE(roundtableViewPathText(row, false, false), QStringLiteral("%1 km").arg(QLocale().toString(2000)));
        row.km.reset();
        row.bearingDeg.reset();
        QVERIFY(roundtableViewPathText(row, true, true).isEmpty());
    }

    void theHeadSaysNobodyTalkingOnceNoTurnIsOpen()
    {
        RoundtableCardModel card;
        QCOMPARE(roundtableViewHeadText(card), QStringLiteral("Nobody talking"));
        card.kind = Kind::LastTalker;
        card.keyed = {QStringLiteral("W1AW")};
        QCOMPARE(roundtableViewHeadText(card), QStringLiteral("Nobody talking"));
        card.kind = Kind::LastDouble;
        QCOMPARE(roundtableViewHeadText(card), QStringLiteral("Last double"));
        card.kind = Kind::Talking;
        QCOMPARE(roundtableViewHeadText(card), QStringLiteral("W1AW"));
        card.kind = Kind::Unknown;
        QCOMPARE(roundtableViewHeadText(card), QStringLiteral("Unknown signal"));
    }

    void aDroppedLinkSaysLastKnown()
    {
        RoundtableCardModel card = talker();
        card.frozen = true;
        card.note = QStringLiteral("Reconnecting");
        QCOMPARE(roundtableViewLinkText(card), QStringLiteral("Reconnecting · last known"));
        card.note = QStringLiteral("Offline since 14:32 UTC");
        QCOMPARE(roundtableViewLinkText(card), QStringLiteral("Offline · last known"));
        card.frozen = false;
        card.note = QStringLiteral("You are hidden");
        QCOMPARE(roundtableViewLinkText(card), QStringLiteral("You are hidden"));
    }

    void itOpensUnderTheTagInsideThePanWithoutTakingAnything()
    {
        Pan pan;
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"), QPoint(200, 300));
        QVERIFY(pan.peek->isOpen());
        QVERIFY(pan.peek->isVisible());
        QCOMPARE(pan.peek->parentWidget(), &pan.widget);
        QCOMPARE(pan.peek->geometry().topLeft(), QPoint(200, 300));
        QCOMPARE(pan.peek->width(), 300);
        // Not an overlay panel, so it is never a tag keep-out; no keyboard.
        QVERIFY(!pan.peek->inherits("NereusSDR::SpectrumOverlayPanel"));
        QCOMPARE(pan.peek->focusPolicy(), Qt::NoFocus);
        QCOMPARE(pan.tune()->focusPolicy(), Qt::NoFocus);
        QCOMPARE(pan.messages()->focusPolicy(), Qt::NoFocus);
        QCOMPARE(pan.close()->focusPolicy(), Qt::NoFocus);

        // Near an edge it stays inside, 4 px off.
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"), QPoint(1100, 630));
        QCOMPARE(pan.peek->geometry().right(), 1200 - 4 - 1);
        QVERIFY(pan.widget.rect().contains(pan.peek->geometry()));
    }

    void aViewNeverBecomesATagKeepOutOrDirtiesTheStaticOverlay()
    {
        Pan pan;
        RoundtableTag tag;
        tag.roomId = QStringLiteral("frequency:lsb-7188600");
        tag.hz = kViewedHz;
        tag.state = static_cast<int>(RoundtableGlyph::Talking);
        tag.label = QStringLiteral("VE3XYZ");
        pan.widget.setRoundtableTags({tag}, RoundtableTagDisplay::Live);
        const QRect spec = pan.widget.roundtableTagSpectrumRectForTest();
        const auto hits = [&]() {
            QImage image(pan.widget.size(), QImage::Format_ARGB32_Premultiplied);
            QPainter painter(&image);
            pan.widget.drawRoundtableTagsForTest(painter, spec);
            return pan.widget.roundtableTagHitsForTest();
        };
        const QRect before = hits().value(0).rect;
        QVERIFY(before.isValid());
        QCoreApplication::processEvents();
        pan.widget.clearOverlayStaticDirtyForTest();
        // Open right over the tag: the tag does not move for it.
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"), before.topLeft());
        pan.peek->refresh(doubled(), -71.0, false);
        pan.widget.setRoundtableViewedRoom(tag.roomId);
        QCoreApplication::processEvents();
        QCOMPARE(hits().value(0).rect, before);
        QVERIFY(!pan.widget.overlayStaticDirtyForTest());
        pan.peek->closePeek();
        pan.widget.setRoundtableViewedRoom(QString());
        QCoreApplication::processEvents();
        QVERIFY(!pan.widget.overlayStaticDirtyForTest());
    }

    void theViewedTagIsOutlined()
    {
        Pan pan;
        RoundtableTag tag;
        tag.roomId = QStringLiteral("frequency:lsb-7188600");
        tag.hz = kViewedHz;
        tag.label = QStringLiteral("VE3XYZ");
        pan.widget.setRoundtableTags({tag}, RoundtableTagDisplay::Live);
        const auto draw = [&]() {
            QImage image(pan.widget.size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::black);
            QPainter painter(&image);
            pan.widget.drawRoundtableTagsForTest(painter, pan.widget.roundtableTagSpectrumRectForTest());
            return image;
        };
        const QColor outline(0x5e, 0x8d, 0xb8);
        QImage image = draw();
        const QRect around = pan.widget.roundtableTagHitsForTest().value(0).rect.adjusted(-3, -3, 3, 3);
        QVERIFY(!hasColor(image, around, outline));
        pan.widget.setRoundtableViewedRoom(tag.roomId);
        image = draw();
        QVERIFY(hasColor(image, around, outline));
    }

    void tuneIsDisabledWhileKeyedAndWhenAlreadyThereAndNeverHidden()
    {
        Pan pan;
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"), QPoint(100, 100));
        pan.peek->refresh(talker(), std::nullopt, false);
        QCOMPARE(pan.tune()->text(), QStringLiteral("Tune slice A here"));
        QVERIFY(pan.tune()->isEnabled());
        QSignalSpy tune(pan.peek, &RoundtableFrequencyPeek::tuneRequested);
        pan.tune()->click();
        QCOMPARE(tune.size(), 1);
        QCOMPARE(tune.first().first().toULongLong(), kViewedHz);

        // Keying disables it at once and the view stays open.
        pan.peek->refresh(talker(), std::nullopt, true);
        QVERIFY(pan.peek->isOpen());
        QVERIFY(!pan.tune()->isEnabled());
        QVERIFY(!pan.tune()->isHidden());
        pan.tune()->click();
        QCOMPARE(tune.size(), 1);
        pan.peek->refresh(talker(), std::nullopt, false);
        QVERIFY(pan.tune()->isEnabled());

        // Already there, in the same family. The other family is somewhere else.
        pan.peek->setSlice(kViewedHz, QStringLiteral("lsb"), QStringLiteral("A"));
        QVERIFY(!pan.tune()->isEnabled());
        pan.peek->setSlice(kViewedHz, QStringLiteral("usb"), QStringLiteral("A"));
        QVERIFY(pan.tune()->isEnabled());
        pan.peek->setSlice(kSliceHz, QStringLiteral("lsb"), QStringLiteral("C"));
        QCOMPARE(pan.tune()->text(), QStringLiteral("Tune slice C here"));
    }

    void messagesAsksForThisFrequency()
    {
        Pan pan;
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"), QPoint(100, 100));
        QSignalSpy messages(pan.peek, &RoundtableFrequencyPeek::messagesRequested);
        pan.messages()->click();
        QCOMPARE(messages.size(), 1);
        QCOMPARE(messages.first().first().toULongLong(), kViewedHz);
        QVERIFY(pan.peek->isOpen());
    }

    void yourPanLeadsTheReadingsOnlyWhileSomeoneIsOnTheAir()
    {
        RoundtableViewModel view;
        view.card = talker();
        view.panDbm = -74.0;
        view.panPeakDbm = -72.0;
        QVector<RoundtableCardRow> rows = roundtableViewRows(view);
        QCOMPARE(rows.size(), 3);
        QCOMPARE(rows.first().label, QStringLiteral("Your pan"));
        QCOMPARE(rows.first().dbm, -74.0);
        QCOMPARE(rows.first().peakDbm, -72.0);
        QVERIFY(rows.first().hasPeak);
        QVERIFY(!rows.first().km.has_value());
        QCOMPARE(rows.at(1).call, QStringLiteral("W2ABC"));

        view.card = doubled();
        QCOMPARE(roundtableViewRows(view).first().label, QStringLiteral("Your pan"));

        // Blank off the pan, and with nobody talking, and on a frozen card.
        view.panDbm.reset();
        QCOMPARE(roundtableViewRows(view).size(), 2);
        view.panDbm = -74.0;
        view.card = RoundtableCardModel{};
        QVERIFY(roundtableViewRows(view).isEmpty());
        view.card = talker();
        view.card.kind = Kind::LastTalker;
        QCOMPARE(roundtableViewRows(view).size(), 2);
        view.card = talker();
        view.card.frozen = true;
        QCOMPARE(roundtableViewRows(view).size(), 2);
    }

    void aDoubleHasCombinedStripesAndNoPath()
    {
        Pan pan;
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"), QPoint(100, 100));
        pan.peek->refresh(doubled(), -71.0, false);
        const QImage image = render(*pan.peek);
        const QRect rail(6, 1, pan.peek->width() - 12, 3);
        QVERIFY(hasColor(image, rail, QColor(0xe0, 0x6c, 0x6e)));
        QVERIFY(hasColor(image, rail, QColor(0x3a, 0x1a, 0x1b)));
        for (const RoundtableCardRow& row : pan.peek->viewForTest().card.rows) {
            QVERIFY(roundtableViewPathText(row, true, true).isEmpty());
        }
    }

    void aFrozenViewDrawsNoLiveColor()
    {
        Pan pan;
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"), QPoint(100, 100));
        RoundtableCardModel card = doubled();
        card.frozen = true;
        card.note = QStringLiteral("Reconnecting");
        for (RoundtableCardRow& row : card.rows) { row.hasPeak = false; }
        pan.peek->refresh(card, -71.0, false);
        const QImage image = render(*pan.peek);
        const QRect all(QPoint(0, 0), image.size());
        QVERIFY(!hasColor(image, all, QColor(0xe0, 0x6c, 0x6e)));
        QVERIFY(!hasColor(image, all, QColor(0x3a, 0x1a, 0x1b)));
        QVERIFY(hasColor(image, all, QColor(0x60, 0x60, 0x66)));
    }

    void valuesUpdateInPlaceAndOnlyStructureResizes()
    {
        Pan pan;
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"), QPoint(100, 100));
        pan.peek->refresh(talker(), -71.0, false);
        const QRect geometry = pan.peek->geometry();
        const QRect tune = pan.tune()->geometry();
        for (int i = 0; i < 200; ++i) {
            RoundtableCardModel card = talker();
            card.rows[0].dbm = -60.0 - i % 30;
            card.clockMs += i * 1000;
            pan.peek->refresh(card, -70.0 - i % 20, false);
            QCOMPARE(pan.peek->geometry(), geometry);
        }
        QCOMPARE(pan.tune()->geometry(), tune);
        RoundtableCardModel more = talker();
        more.onFrequency.append(RoundtableCardPerson{QStringLiteral("K4ABC"), false, false, 0});
        pan.peek->refresh(more, -71.0, false);
        QVERIFY(pan.peek->height() > geometry.height());
    }

    void rowsGiveWayFirstOnAShortPan()
    {
        Pan pan(240);
        RoundtableCardModel card = talker();
        for (int i = 0; i < 12; ++i) { card.rows.append(rowOf(QStringLiteral("K%1").arg(i), -70.0)); }
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"), QPoint(100, 100));
        pan.peek->refresh(card, std::nullopt, false);
        QVERIFY(pan.peek->height() <= 240 - 8);
        QVERIFY(pan.peek->rowLimitForTest() < card.rows.size());
        QVERIFY(pan.widget.rect().contains(pan.peek->geometry()));
        QVERIFY(pan.widget.rect().contains(pan.peek->mapToParent(pan.tune()->geometry().bottomRight())));
    }

    void escapeAndTheCloseBoxCloseOnce()
    {
        Pan pan;
        pan.widget.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&pan.widget));
        QSignalSpy closed(pan.peek, &RoundtableFrequencyPeek::closed);

        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"), QPoint(100, 100));
        QTest::keyClick(&pan.widget, Qt::Key_Escape);
        QVERIFY(!pan.peek->isOpen());
        QVERIFY(!pan.peek->isVisible());
        QCOMPARE(closed.size(), 1);
        // Closed, Escape is the window's again and nothing more is said.
        QTest::keyClick(&pan.widget, Qt::Key_Escape);
        pan.peek->closePeek();
        QCOMPARE(closed.size(), 1);

        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"), QPoint(100, 100));
        pan.close()->click();
        QVERIFY(!pan.peek->isOpen());
        QCOMPARE(closed.size(), 2);

        // A refresh that arrives after a close does not reopen it.
        pan.peek->refresh(talker(), std::nullopt, false);
        QVERIFY(!pan.peek->isOpen());
        QVERIFY(!pan.peek->isVisible());
    }

    void aClickInsideNeverReachesTheBand()
    {
        Pan pan;
        pan.widget.setUpdatesEnabled(true);
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"), QPoint(100, 100));
        pan.peek->refresh(talker(), std::nullopt, false);
        QSignalSpy tuned(&pan.widget, &SpectrumWidget::frequencyClicked);
        QSignalSpy band(&pan.widget, &SpectrumWidget::roundtableBandClicked);
        QTest::mouseClick(pan.peek, Qt::LeftButton, Qt::NoModifier, QPoint(20, 50));
        QCOMPARE(tuned.size(), 0);
        QCOMPARE(band.size(), 0);
        QVERIFY(pan.peek->isOpen());
    }

    void thePanLevelIsThePeakInTheSidebandWindow()
    {
        Pan pan;
        QVector<float> pixels(kPixels, -120.0f);
        // Hz = 7104000 + i * 1000. A signal below the viewed frequency and one above.
        const auto pixelOf = [](quint64 hz) { return static_cast<int>((hz - 7104000) / 1000); };
        const quint64 viewed = 7188000;
        pixels[pixelOf(viewed - 2000)] = -70.0f;
        pixels[pixelOf(viewed + 2000)] = -60.0f;

        // Before the first frame.
        QVERIFY(!pan.widget.roundtablePanLevel(viewed, QStringLiteral("lsb")).has_value());
        pan.widget.setRenderedPixelsForTest(pixels);
        QCOMPARE(pan.widget.roundtablePanLevel(viewed, QStringLiteral("lsb")).value_or(0.0), -70.0);
        QCOMPARE(pan.widget.roundtablePanLevel(viewed, QStringLiteral("digl")).value_or(0.0), -70.0);
        QCOMPARE(pan.widget.roundtablePanLevel(viewed, QStringLiteral("fdvl")).value_or(0.0), -70.0);
        QCOMPARE(pan.widget.roundtablePanLevel(viewed, QStringLiteral("usb")).value_or(0.0), -60.0);
        QCOMPARE(pan.widget.roundtablePanLevel(viewed, QStringLiteral("digu")).value_or(0.0), -60.0);
        QCOMPARE(pan.widget.roundtablePanLevel(viewed, QStringLiteral("fdvu")).value_or(0.0), -60.0);
        // Past the 2.7 kHz edge the signal is out of the window.
        QCOMPARE(pan.widget.roundtablePanLevel(viewed + 2000 + 4000, QStringLiteral("lsb")).value_or(0.0), -120.0);
        // No sideband: 2.7 kHz centered, so neither signal 2 kHz off is in it.
        QCOMPARE(pan.widget.roundtablePanLevel(viewed, QStringLiteral("am")).value_or(0.0), -120.0);

        // Off the pan.
        QVERIFY(!pan.widget.roundtablePanLevel(7000000, QStringLiteral("lsb")).has_value());
        QVERIFY(!pan.widget.roundtablePanLevel(7400000, QStringLiteral("usb")).has_value());

        // Calibrated as the trace draws it.
        pan.widget.setDbmCalOffset(3.0f);
        QCOMPARE(pan.widget.roundtablePanLevel(viewed, QStringLiteral("lsb")).value_or(0.0), -67.0);
        pan.widget.setDbmCalOffset(0.0f);

        // Blank while transmitting.
        pan.widget.setMoxOverlay(true);
        QVERIFY(!pan.widget.roundtablePanLevel(viewed, QStringLiteral("lsb")).has_value());
        pan.widget.setMoxOverlay(false);
        pan.widget.setTxActiveForTest(true);
        QVERIFY(!pan.widget.roundtablePanLevel(viewed, QStringLiteral("lsb")).has_value());
        pan.widget.setTxActiveForTest(false);
        QVERIFY(pan.widget.roundtablePanLevel(viewed, QStringLiteral("lsb")).has_value());
    }

    void capturesForTheBoard_data()
    {
        QTest::addColumn<QString>("scene");
        QTest::newRow("double") << QStringLiteral("double");
        QTest::newRow("talker") << QStringLiteral("talker");
        QTest::newRow("your-pan-blank") << QStringLiteral("your-pan-blank");
        QTest::newRow("reconnecting") << QStringLiteral("reconnecting");
        QTest::newRow("quiet") << QStringLiteral("quiet");
    }

    void capturesForTheBoard()
    {
        const QString directory = qEnvironmentVariable("NEREUS_ROUNDTABLE_PEEK_CAPTURES");
        if (directory.isEmpty()) { return; }
        QFETCH(QString, scene);
        AppSettings::instance().setValue(QStringLiteral("Roundtable/SignalFormat"), QStringLiteral("Both"));
        Pan pan;
        pan.widget.setUpdatesEnabled(true);
        RoundtableCardModel card = talker();
        std::optional<double> level = -73.0;
        if (scene == QLatin1String("double")) { card = doubled(); }
        if (scene == QLatin1String("your-pan-blank")) { level.reset(); }
        if (scene == QLatin1String("reconnecting")) {
            card.frozen = true;
            card.note = QStringLiteral("Reconnecting");
            for (RoundtableCardRow& row : card.rows) { row.hasPeak = false; }
        }
        if (scene == QLatin1String("quiet")) {
            card.kind = Kind::LastTalker;
            card.endedAgoMs = 180000;
            card.clockMs = 0;
            for (RoundtableCardRow& row : card.rows) { row.hasPeak = false; }
            for (RoundtableCardPerson& person : card.onFrequency) { person.keyed = false; }
        }
        pan.peek->setPathPreferences(true, true);
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"), QPoint(40, 40));
        pan.peek->refresh(card, level, false);
        QImage image(pan.peek->size() + QSize(80, 80), QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor(0x06, 0x07, 0x0a));
        {
            QPainter painter(&image);
            pan.peek->render(&painter, QPoint(40, 40), QRegion(), QWidget::DrawChildren);
        }
        AppSettings::instance().remove(QStringLiteral("Roundtable/SignalFormat"));
        QDir().mkpath(directory);
        QVERIFY(image.save(QStringLiteral("%1/%2.png").arg(directory, scene)));
    }
};

QTEST_MAIN(TestRoundtableFrequencyPeek)
#include "tst_roundtable_frequency_peek.moc"
