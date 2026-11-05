// no-port-check: NereusSDR-original.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// tests/tst_roundtable_frequency_peek.cpp  (NereusSDR)
// =================================================================
//
// The one inspector (overlay design D21 to D24, D34, D35, D37): its words, your own
// frequency, the Tune gates, Back and ⋯, Your pan, the foot line, the Also here fold,
// the scrolling body, how it closes, and that nothing inside it reaches the band
// underneath. The flow through MainWindow is in tst_roundtable_desktop_flow.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-04: The one inspector. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-11-03: original implementation for NereusSDR by J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QtTest/qtestspontaneevent.h>

#include <cmath>

#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QPainter>
#include <QPushButton>
#include <QSignalSpy>
#include <QToolButton>
#include <QWheelEvent>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/session/media/SpectrumEndpoint.h"
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
    QToolButton* more() const { return peek->findChild<QToolButton*>(QStringLiteral("roundtablePeekMore")); }
    QPushButton* back() const { return peek->findChild<QPushButton*>(QStringLiteral("roundtablePeekBack")); }
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
    paintRoundtableView(painter, peek.viewForTest(), QRect(QPoint(0, 0), peek.size()));
    return image;
}

// A card too long for most pans: three on frequency and 14 readings.
RoundtableCardModel longTalker()
{
    RoundtableCardModel card = talker();
    for (int i = 0; i < 12; ++i) { card.rows.append(rowOf(QStringLiteral("K%1").arg(i), -70.0)); }
    return card;
}

// Counts the wheel events that reach a widget.
class WheelCounter : public QObject {
public:
    int count = 0;

protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
        if (event->type() == QEvent::Wheel) { ++count; }
        return false;
    }
};

// Sends one wheel notch to `target` as the window system would, so an unhandled one
// climbs to the parent; true if something took it.
bool wheelNotch(QWidget* target, int angle)
{
    const QPoint position = target->rect().center();
    QWheelEvent event(QPointF(position), QPointF(target->mapToGlobal(position)), QPoint(), QPoint(0, angle), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QSpontaneKeyEvent::setSpontaneous(&event);
    QCoreApplication::instance()->notify(target, &event);
    return event.isAccepted();
}

int brightness(const QImage& image, const QRect& area)
{
    int total = 0;
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) { total += qGray(image.pixel(x, y)); }
    }
    return total;
}

} // namespace

class TestRoundtableFrequencyPeek : public QObject {
    Q_OBJECT

private slots:
    // D35: the distance and nothing else, and no line at all on your own frequency.
    void theLineSaysHowFar()
    {
        RoundtableViewModel view;
        view.card.hz = kViewedHz;
        view.family = QStringLiteral("lsb");
        view.sliceHz = kSliceHz;
        const QChar minus(0x2212);
        QCOMPARE(roundtableViewSubText(view), QStringLiteral("Viewing · %1").arg(minus) + QStringLiteral("11.4 kHz"));
        view.card.hz = kSliceHz + 1400;
        view.family = QStringLiteral("fdvu");
        view.sliceName = QStringLiteral("B");
        QCOMPARE(roundtableViewSubText(view), QStringLiteral("Viewing · +1.4 kHz"));
        // Another family on the slice's own frequency has no distance to give.
        view.card.hz = kSliceHz;
        view.family = QStringLiteral("cw");
        QCOMPARE(roundtableViewSubText(view), QStringLiteral("Viewing"));
        // Nor does a window with no slice.
        view.card.hz = kViewedHz;
        view.sliceName.clear();
        QCOMPARE(roundtableViewSubText(view), QStringLiteral("Viewing"));
        view.mine = true;
        QVERIFY(roundtableViewSubText(view).isEmpty());
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
        QCOMPARE(roundtableViewLinkText(card), QStringLiteral("Offline since 14:32 UTC · last known"));
        card.frozen = false;
        card.note = QStringLiteral("You are hidden");
        QCOMPARE(roundtableViewLinkText(card), QStringLiteral("You are hidden"));
    }

    void itOpensInsideThePanWithoutTakingAnything()
    {
        Pan pan;
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
        QVERIFY(pan.peek->isOpen());
        QVERIFY(pan.peek->isVisible());
        QCOMPARE(pan.peek->parentWidget(), &pan.widget);
        QCOMPARE(pan.peek->width(), 300);
        QVERIFY(pan.widget.rect().contains(pan.peek->geometry()));
        // Not an overlay panel, so it is never a tag keep-out; no keyboard.
        QVERIFY(!pan.peek->inherits("NereusSDR::SpectrumOverlayPanel"));
        QCOMPARE(pan.peek->focusPolicy(), Qt::NoFocus);
        QCOMPARE(pan.tune()->focusPolicy(), Qt::NoFocus);
        QCOMPARE(pan.messages()->focusPolicy(), Qt::NoFocus);
        QCOMPARE(pan.close()->focusPolicy(), Qt::NoFocus);
    }

    // No radio is not transmitting: Tune greys for its own reason.
    void noRadioGreysTuneAndSaysSo()
    {
        Pan pan;
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
        pan.peek->refresh(talker(), std::nullopt, false, false);
        QVERIFY(!pan.tune()->isEnabled());
        QCOMPARE(pan.tune()->toolTip(), QStringLiteral("No radio connected"));
        QVERIFY(pan.peek->isOpen());
        pan.peek->refresh(talker(), std::nullopt, true, true);
        QCOMPARE(pan.tune()->toolTip(), QStringLiteral("Not while you transmit"));
        pan.peek->refresh(talker(), std::nullopt, false, true);
        QVERIFY(pan.tune()->isEnabled());
        QVERIFY(pan.tune()->toolTip().isEmpty());
    }

    // A short body keeps its rows readable: the fade takes at most half of it.
    void theFadeNeverCoversAShortBody()
    {
        QCOMPARE(roundtableViewFadeHeight(11), 5);
        QCOMPARE(roundtableViewFadeHeight(44), 22);
        QCOMPARE(roundtableViewFadeHeight(400), 22);
        QCOMPARE(roundtableViewFadeHeight(0), 0);
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
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
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
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
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
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
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
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
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
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
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
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
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

    // D22: your own frequency drops the Viewing line and Your pan; Tune stays, greyed, with why.
    void yourOwnFrequencyHasNoViewingLineOrYourPan()
    {
        Pan pan;
        pan.peek->setSlice(kSliceHz, QStringLiteral("lsb"), QStringLiteral("A"));
        QSignalSpy closed(pan.peek, &RoundtableFrequencyPeek::closed);
        pan.peek->showOwn(kSliceHz, QStringLiteral("lsb"));
        RoundtableCardModel card = talker();
        card.hz = kSliceHz;
        pan.peek->refresh(card, -71.0, false);
        QVERIFY(pan.peek->showsOwn());
        QVERIFY(pan.peek->viewForTest().mine);
        QVERIFY(roundtableViewSubText(pan.peek->viewForTest()).isEmpty());
        QCOMPARE(roundtableViewLayout(pan.peek->viewForTest(), 300, false, pan.peek).viewingTop, -1);
        QVERIFY(pan.back()->isHidden());
        for (const RoundtableCardRow& row : roundtableViewRows(pan.peek->viewForTest())) {
            QVERIFY(row.label != QStringLiteral("Your pan"));
        }
        QVERIFY(pan.tune()->isVisible());
        QVERIFY(!pan.tune()->isEnabled());
        QCOMPARE(pan.tune()->text(), QStringLiteral("Tune slice A here"));
        QCOMPARE(pan.tune()->toolTip(), QStringLiteral("Slice A is already here"));
        // The slice's own name, and the reason holds while keyed.
        pan.peek->setSlice(kSliceHz, QStringLiteral("lsb"), QStringLiteral("C"));
        pan.peek->refresh(card, std::nullopt, true);
        QCOMPARE(pan.tune()->toolTip(), QStringLiteral("Slice C is already here"));
        // No slice: greyed, the plain words, no slice to name.
        pan.peek->setSlice(0, QString(), QString());
        pan.peek->refresh(card, std::nullopt, false);
        QVERIFY(!pan.tune()->isEnabled());
        QCOMPARE(pan.tune()->text(), QStringLiteral("Tune slice here"));
        QVERIFY(pan.tune()->toolTip().isEmpty());

        // Your own frequency keeps its last talker's name; another says nobody is talking.
        RoundtableViewModel view;
        view.card.kind = Kind::LastTalker;
        view.card.keyed = {QStringLiteral("W1AW")};
        QCOMPARE(roundtableViewHeadText(view), QStringLiteral("Nobody talking"));
        view.mine = true;
        QCOMPARE(roundtableViewHeadText(view), QStringLiteral("W1AW"));

        // Another frequency swaps in place: no close, and the Viewing line is back.
        pan.peek->setSlice(kSliceHz, QStringLiteral("lsb"), QStringLiteral("A"));
        pan.peek->showFor(kSliceHz + 1400, QStringLiteral("lsb"));
        QVERIFY(pan.peek->isOpen());
        QVERIFY(!pan.peek->showsOwn());
        QVERIFY(!pan.back()->isHidden());
        QCOMPARE(closed.size(), 0);
    }

    void yourPansComingAndGoingDoesNotResizeYourOwnFrequency()
    {
        Pan pan;
        pan.peek->showOwn(kSliceHz, QStringLiteral("lsb"));
        RoundtableCardModel card = talker();
        card.hz = kSliceHz;
        pan.peek->refresh(card, std::nullopt, false);
        const int fits = pan.peek->fitsForTest();
        pan.peek->refresh(card, -71.0, false);
        pan.peek->refresh(card, std::nullopt, false);
        QCOMPARE(pan.peek->fitsForTest(), fits);
    }

    void tuneSaysWhyItIsGreyedWhileKeyed()
    {
        Pan pan;
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
        pan.peek->refresh(talker(), std::nullopt, true);
        QVERIFY(!pan.tune()->isEnabled());
        QCOMPARE(pan.tune()->toolTip(), QStringLiteral("Not while you transmit"));
        pan.peek->refresh(talker(), std::nullopt, false);
        QVERIFY(pan.tune()->isEnabled());
        QVERIFY(pan.tune()->toolTip().isEmpty());
        // On your slice's own frequency by another route, the same reason as your own.
        pan.peek->setSlice(kViewedHz, QStringLiteral("lsb"), QStringLiteral("A"));
        pan.peek->refresh(talker(), std::nullopt, false);
        QCOMPARE(pan.tune()->toolTip(), QStringLiteral("Slice A is already here"));
    }

    void backAsksAndTunesNothing()
    {
        Pan pan;
        pan.widget.setUpdatesEnabled(true);
        pan.peek->showFor(kSliceHz + 1400, QStringLiteral("lsb"));
        RoundtableCardModel card = talker();
        card.hz = kSliceHz + 1400;
        pan.peek->refresh(card, -71.0, false);
        QCOMPARE(roundtableViewSubText(pan.peek->viewForTest()), QStringLiteral("Viewing · +1.4 kHz"));
        QVERIFY(pan.back()->isVisible());
        QVERIFY(pan.back()->isEnabled());
        QCOMPARE(pan.back()->focusPolicy(), Qt::NoFocus);
        // Beside the words, under the head, all of it inside the view.
        QVERIFY(pan.peek->rect().contains(pan.back()->geometry()));
        QVERIFY(pan.back()->geometry().top() >= pan.more()->geometry().bottom());
        QVERIFY(pan.back()->width() >= pan.back()->fontMetrics().horizontalAdvance(pan.back()->text()));

        QSignalSpy back(pan.peek, &RoundtableFrequencyPeek::backRequested);
        QSignalSpy tune(pan.peek, &RoundtableFrequencyPeek::tuneRequested);
        QSignalSpy clicked(&pan.widget, &SpectrumWidget::frequencyClicked);
        QTest::mouseClick(pan.back(), Qt::LeftButton);
        QCOMPARE(back.size(), 1);
        QCOMPARE(tune.size(), 0);
        QCOMPARE(clicked.size(), 0);
        QVERIFY(pan.peek->isOpen());

        // Nowhere to go without a slice: greyed, not hidden.
        pan.peek->setSlice(0, QString(), QString());
        QVERIFY(pan.back()->isVisible());
        QVERIFY(!pan.back()->isEnabled());
    }

    void moreAsksForItsMenuUnderTheButton()
    {
        Pan pan;
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
        QCOMPARE(pan.more()->focusPolicy(), Qt::NoFocus);
        QCOMPARE(pan.more()->geometry().size(), QSize(24, 24));
        QCOMPARE(pan.close()->geometry().size(), QSize(24, 24));
        QVERIFY(pan.more()->geometry().right() < pan.close()->geometry().left());
        QSignalSpy more(pan.peek, &RoundtableFrequencyPeek::moreRequested);
        pan.more()->click();
        QCOMPARE(more.size(), 1);
        QCOMPARE(more.first().first().toPoint(), pan.more()->mapToGlobal(QPoint(0, 24)));
        QVERIFY(pan.peek->isOpen());
    }

    void itStandsOnTheFootLineInTheCardsSpot()
    {
        Pan pan;
        const QRect spot(4, 30, 1192, 500);
        const int dialX = 600;
        const QRect flag(dialX - 24, 30, 276, 162);
        QSignalSpy closed(pan.peek, &RoundtableFrequencyPeek::closed);
        pan.peek->setInspectorSpot(spot, dialX, {flag}, false);
        QVERIFY(pan.peek->inspectorRect().isNull());
        pan.peek->showOwn(kSliceHz, QStringLiteral("lsb"));
        pan.peek->refresh(talker(), std::nullopt, false);
        QRect rect = pan.peek->inspectorRect();
        QCOMPARE(rect, pan.peek->geometry());
        QCOMPARE(rect.width(), 300);
        QCOMPARE(rect.left(), flag.left() + flag.width() + 4);
        QCOMPARE(rect.top() + rect.height(), spot.top() + spot.height());

        // The same spot again does nothing; a flipped flag moves it past the flag.
        const int fits = pan.peek->fitsForTest();
        pan.peek->setInspectorSpot(spot, dialX, {flag}, false);
        QCOMPARE(pan.peek->fitsForTest(), fits);
        const QRect flipped(dialX - 252, 30, 276, 162);
        pan.peek->setInspectorSpot(spot, dialX, {flipped}, true);
        rect = pan.peek->inspectorRect();
        QCOMPARE(rect.left() + rect.width(), flipped.left() - 4);
        QCOMPARE(rect.top() + rect.height(), spot.top() + spot.height());
        QCOMPARE(closed.size(), 0);

        // A taller card grows upward from the foot.
        const int top = rect.top();
        RoundtableCardModel card = talker();
        card.rows.append(rowOf(QStringLiteral("K4NEW"), -60.0));
        pan.peek->refresh(card, std::nullopt, false);
        QVERIFY(pan.peek->geometry().top() < top);
        QCOMPARE(pan.peek->geometry().top() + pan.peek->height(), spot.top() + spot.height());

        pan.peek->closePeek();
        QVERIFY(pan.peek->inspectorRect().isNull());
    }

    // D19: a short pan folds everyone not keyed before anything else gives way.
    void aShortPanFoldsTheUnkeyedToOneLineFirst()
    {
        Pan pan;
        RoundtableViewModel view;
        view.card = talker();
        const RoundtableViewLayout whole = roundtableViewLayout(view, 300, false, pan.peek);
        const RoundtableViewLayout folded = roundtableViewLayout(view, 300, true, pan.peek);
        // Two quiet stations become one line. The head does not move.
        QVERIFY(folded.total < whole.total);
        QCOMPARE(folded.head, whole.head);
        // Keyed names never fold.
        view.card = doubled();
        view.card.onFrequency.removeLast();
        QCOMPARE(roundtableViewLayout(view, 300, true, pan.peek).total, roundtableViewLayout(view, 300, false, pan.peek).total);

        const QRect spot(4, 30, 1192, 500);
        pan.peek->setInspectorSpot(spot, 600, {}, false);
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
        pan.peek->refresh(talker(), std::nullopt, false);
        QVERIFY(!pan.peek->foldedForTest());
        const int wants = pan.peek->height();
        QCOMPARE(pan.peek->maxScrollForTest(), 0);

        // One pixel short of the whole list folds, and the fold is enough: nothing scrolls.
        pan.peek->setInspectorSpot(QRect(4, 30, 1192, wants - 1), 600, {}, false);
        QVERIFY(pan.peek->foldedForTest());
        QCOMPARE(pan.peek->maxScrollForTest(), 0);
        QVERIFY(pan.peek->height() < wants);
        pan.peek->setInspectorSpot(spot, 600, {}, false);
        QVERIFY(!pan.peek->foldedForTest());
    }

    // The fold draws its line where the measure said. Height and paint are one walk.
    void heightAndPaintAreOneWalk_data()
    {
        QTest::addColumn<bool>("fold");
        QTest::addColumn<bool>("mine");
        QTest::addColumn<bool>("frozen");
        QTest::newRow("whole") << false << false << false;
        QTest::newRow("fold") << true << false << false;
        QTest::newRow("mine fold") << true << true << false;
        QTest::newRow("frozen fold") << true << false << true;
    }

    void heightAndPaintAreOneWalk()
    {
        QFETCH(bool, fold);
        QFETCH(bool, mine);
        QFETCH(bool, frozen);
        RoundtableViewModel view;
        view.card = longTalker();
        view.mine = mine;
        view.panDbm = -70.0;
        if (frozen) {
            view.card.frozen = true;
            view.card.note = QStringLiteral("Reconnecting");
        }
        QImage image(300, 700, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::black);
        const RoundtableViewLayout measured = roundtableViewLayout(view, 300, fold, &image);
        QPainter painter(&image);
        const RoundtableViewLayout painted = paintRoundtableView(painter, view, image.rect(), fold, 7, 60);
        QCOMPARE(painted.head, measured.head);
        QCOMPARE(painted.total, measured.total);
        QCOMPARE(painted.viewingTop, measured.viewingTop);
    }

    // D19, D35: what still does not fit scrolls by the wheel, and only the body moves.
    void whatStillDoesNotFitScrolls()
    {
        Pan pan;
        pan.widget.setUpdatesEnabled(true);
        const QRect spot(4, 30, 1192, 200);
        pan.peek->setInspectorSpot(spot, 600, {}, false);
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
        pan.peek->refresh(longTalker(), -71.0, false);
        QVERIFY(pan.peek->foldedForTest());
        QCOMPARE(pan.peek->geometry().height(), spot.height());
        QCOMPARE(pan.peek->geometry().top(), spot.top());
        QVERIFY(pan.peek->maxScrollForTest() > 0);
        QVERIFY(pan.peek->bodyHeightForTest() > 0);
        QCOMPARE(pan.peek->scrollForTest(), 0);

        const QRect tune = pan.tune()->geometry();
        const QRect messages = pan.messages()->geometry();
        const QRect close = pan.close()->geometry();
        const QRect back = pan.back()->geometry();
        const QRect geometry = pan.peek->geometry();
        QCOMPARE(tune.bottom() + 1 + 8, geometry.height());
        const int fits = pan.peek->fitsForTest();

        WheelCounter onBand;
        pan.widget.installEventFilter(&onBand);
        QSignalSpy clicked(&pan.widget, &SpectrumWidget::frequencyClicked);

        QVERIFY(wheelNotch(pan.peek, -120));
        QCOMPARE(pan.peek->scrollForTest(), 40);
        // Over a button, and at both ends, it still stops here.
        QVERIFY(wheelNotch(pan.tune(), -120));
        QCOMPARE(pan.peek->scrollForTest(), 80);
        for (int i = 0; i < 40; ++i) { QVERIFY(wheelNotch(pan.peek, -120)); }
        QCOMPARE(pan.peek->scrollForTest(), pan.peek->maxScrollForTest());
        for (int i = 0; i < 40; ++i) { QVERIFY(wheelNotch(pan.peek, 120)); }
        QCOMPARE(pan.peek->scrollForTest(), 0);
        // A wheel with no vertical part, too.
        QVERIFY(wheelNotch(pan.peek, 0));

        QCOMPARE(onBand.count, 0);
        QCOMPARE(clicked.size(), 0);
        QCOMPARE(pan.tune()->geometry(), tune);
        QCOMPARE(pan.messages()->geometry(), messages);
        QCOMPARE(pan.close()->geometry(), close);
        QCOMPARE(pan.back()->geometry(), back);
        QCOMPARE(pan.peek->geometry(), geometry);
        QCOMPARE(pan.peek->fitsForTest(), fits);
        QVERIFY(pan.peek->isOpen());

        // The place holds through a refresh, and a shorter list pulls it back.
        wheelNotch(pan.peek, -120);
        pan.peek->refresh(longTalker(), -72.0, false);
        QCOMPARE(pan.peek->scrollForTest(), 40);
        for (int i = 0; i < 40; ++i) { wheelNotch(pan.peek, -120); }
        RoundtableCardModel shorter = longTalker();
        shorter.rows.removeLast();
        pan.peek->refresh(shorter, -72.0, false);
        QCOMPARE(pan.peek->scrollForTest(), pan.peek->maxScrollForTest());
        // Another frequency starts at the top.
        pan.peek->showFor(kViewedHz + 1000, QStringLiteral("lsb"));
        QCOMPARE(pan.peek->scrollForTest(), 0);
    }

    // The two ends of the short pan: 120 px still holds the head and the buttons.
    void aVeryShortPanKeepsTheButtons()
    {
        Pan pan;
        const QRect spot(4, 30, 1192, 120);
        pan.peek->setInspectorSpot(spot, 600, {}, false);
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
        pan.peek->refresh(longTalker(), -71.0, false);
        QCOMPARE(pan.peek->height(), 120);
        QVERIFY(pan.peek->bodyHeightForTest() >= 0);
        QVERIFY(pan.peek->rect().contains(pan.tune()->geometry()));
        QVERIFY(pan.peek->rect().contains(pan.messages()->geometry()));
        QVERIFY(pan.back()->geometry().bottom() < pan.tune()->geometry().top());
    }

    // D19 with no spot given yet: on a short pan the view folds and scrolls, inside the pan.
    void withNoSpotAShortPanFoldsAndScrollsInsideIt()
    {
        Pan pan(240);
        RoundtableCardModel card = longTalker();
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
        pan.peek->refresh(card, std::nullopt, false);
        QVERIFY(pan.peek->height() <= 240 - 8);
        QVERIFY(pan.peek->foldedForTest());
        QVERIFY(pan.peek->maxScrollForTest() > 0);
        QVERIFY(pan.widget.rect().contains(pan.peek->geometry()));
        QVERIFY(pan.widget.rect().contains(pan.peek->mapToParent(pan.tune()->geometry().bottomRight())));
    }

    void aCutFadesWhileMoreLiesBelowAndNotAtTheEnd()
    {
        RoundtableViewModel view;
        view.card = longTalker();
        QImage image(300, 700, QImage::Format_ARGB32_Premultiplied);
        const RoundtableViewLayout layout = roundtableViewLayout(view, 300, false, &image);
        const int body = 120;
        const QRect band(1, layout.head + body - 8, 298, 8);
        const auto draw = [&](int scrollY, int bodyHeight) {
            QImage out(300, 700, QImage::Format_ARGB32_Premultiplied);
            out.fill(Qt::black);
            QPainter painter(&out);
            paintRoundtableView(painter, view, out.rect(), false, scrollY, bodyHeight);
            return out;
        };
        // The same words at the same place, with and without the fade over them.
        const int faded = brightness(draw(0, body), band);
        const int whole = brightness(draw(0, -1), band);
        QVERIFY2(faded < whole, qPrintable(QStringLiteral("%1 against %2").arg(faded).arg(whole)));
        // At the end nothing lies below: the last pixels are the words, undimmed.
        const int end = layout.body() - body;
        const QRect endBand(1, layout.head + body - 8, 298, 8);
        const QImage atEnd = draw(end, body);
        QVERIFY(brightness(atEnd, endBand) > 0);
        QImage shifted = draw(0, -1);
        const QRect sameWords(1, layout.head + layout.body() - 8, 298, 8);
        QCOMPARE(brightness(atEnd, endBand), brightness(shifted, sameWords));
    }

    // Escape is the view's only while its own window has the keyboard. A dialog
    // of that window keeps it, however QWindow::isActive answers for the window.
    void escapeWaitsForItsOwnWindowToBeActive()
    {
        Pan pan;
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
        QDialog dialog(&pan.widget);
        dialog.show();
        QTRY_VERIFY(QApplication::activeWindow() == &dialog);
        QVERIFY(pan.widget.windowHandle()->isActive());

        QTest::keyClick(&pan.widget, Qt::Key_Escape);
        QVERIFY(pan.peek->isOpen());

        pan.widget.activateWindow();
        QTRY_VERIFY(QApplication::activeWindow() == &pan.widget);
        QTest::keyClick(&pan.widget, Qt::Key_Escape);
        QVERIFY(!pan.peek->isOpen());
    }

    void escapeAndTheCloseBoxCloseOnce()
    {
        Pan pan;
        pan.widget.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&pan.widget));
        QSignalSpy closed(pan.peek, &RoundtableFrequencyPeek::closed);

        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
        QTest::keyClick(&pan.widget, Qt::Key_Escape);
        QVERIFY(!pan.peek->isOpen());
        QVERIFY(!pan.peek->isVisible());
        QCOMPARE(closed.size(), 1);
        // Closed, Escape is the window's again and nothing more is said.
        QTest::keyClick(&pan.widget, Qt::Key_Escape);
        pan.peek->closePeek();
        QCOMPARE(closed.size(), 1);

        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
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
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
        pan.peek->refresh(talker(), std::nullopt, false);
        QSignalSpy tuned(&pan.widget, &SpectrumWidget::frequencyClicked);
        QTest::mouseClick(pan.peek, Qt::LeftButton, Qt::NoModifier, QPoint(20, 50));
        QCOMPARE(tuned.size(), 0);
        QVERIFY(pan.peek->isOpen());
    }

    // D37: a fold's list, its rows only ask, and the close box is all it keeps.
    void aFoldListsItsFrequenciesAndARowOnlyAsks()
    {
        Pan pan;
        pan.widget.setUpdatesEnabled(true);
        QVector<RoundtableFoldRow> rows;
        for (int i = 0; i < 3; ++i) {
            RoundtableFoldRow row;
            row.roomId = QStringLiteral("frequency:lsb-%1").arg(7185000 + i * 50);
            row.hz = 7185000 + i * 50;
            row.state = static_cast<int>(RoundtableGlyph::Quiet);
            row.who = QStringLiteral("W%1ABC").arg(i);
            rows.append(row);
        }
        QSignalSpy asked(pan.peek, &RoundtableFrequencyPeek::foldRowClicked);
        QSignalSpy closed(pan.peek, &RoundtableFrequencyPeek::closed);
        QSignalSpy tuned(&pan.widget, &SpectrumWidget::frequencyClicked);
        pan.peek->showFold(rows);
        QVERIFY(pan.peek->showsFold());
        QVERIFY(!pan.peek->showsOwn());
        QVERIFY(!pan.tune()->isVisibleTo(pan.peek));
        QVERIFY(!pan.messages()->isVisibleTo(pan.peek));
        QVERIFY(!pan.more()->isVisibleTo(pan.peek));
        QVERIFY(!pan.back()->isVisibleTo(pan.peek));
        QVERIFY(pan.close()->isVisibleTo(pan.peek));
        QCOMPARE(pan.peek->height(), roundtableFoldListHeight(3));

        const int rowStep = 30;  // .ins-fr and the list's gap
        QTest::mouseClick(pan.peek, Qt::LeftButton, Qt::NoModifier, QPoint(40, roundtableFoldListBodyTop() + rowStep + 14));
        QCOMPARE(asked.size(), 1);
        QCOMPARE(asked.first().first().toString(), rows.at(1).roomId);
        // The gap between rows and the head ask nothing.
        QTest::mouseClick(pan.peek, Qt::LeftButton, Qt::NoModifier, QPoint(40, roundtableFoldListBodyTop() + 29));
        QTest::mouseClick(pan.peek, Qt::LeftButton, Qt::NoModifier, QPoint(40, 5));
        QCOMPARE(asked.size(), 1);
        QCOMPARE(tuned.size(), 0);
        QCOMPARE(closed.size(), 0);

        // Fewer rows in place; then back to a frequency, and every button returns.
        rows.removeLast();
        pan.peek->setFoldRows(rows);
        QCOMPARE(pan.peek->foldRowsForTest().size(), qsizetype(2));
        QCOMPARE(pan.peek->height(), roundtableFoldListHeight(2));
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
        QVERIFY(!pan.peek->showsFold());
        QCOMPARE(closed.size(), 0);
        QVERIFY(pan.tune()->isVisibleTo(pan.peek));
        QVERIFY(pan.more()->isVisibleTo(pan.peek));
        QVERIFY(pan.back()->isVisibleTo(pan.peek));
        pan.close()->click();
        QCOMPARE(closed.size(), 1);
    }

    void aLongFoldScrollsAndKeepsItsHead()
    {
        Pan pan;
        QVector<RoundtableFoldRow> rows;
        for (int i = 0; i < 30; ++i) {
            RoundtableFoldRow row;
            row.roomId = QStringLiteral("frequency:lsb-%1").arg(7185000 + i * 50);
            row.hz = 7185000 + i * 50;
            rows.append(row);
        }
        pan.peek->setInspectorSpot(QRect(4, 2, 1000, 240), 300, {}, false);
        pan.peek->showFold(rows);
        QCOMPARE(pan.peek->height(), 240);
        QVERIFY(pan.peek->maxScrollForTest() > 0);
        QWheelEvent wheel(QPointF(40, 100), pan.peek->mapToGlobal(QPointF(40, 100)), QPoint(), QPoint(0, -120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(pan.peek, &wheel);
        QCOMPARE(pan.peek->scrollForTest(), 40);
        QCOMPARE(roundtableFoldListRowAt(QPoint(40, 5), pan.peek->rect(), 30, pan.peek->scrollForTest()), -1);
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

        // The trace's whole calibration: the station's term as well, as attenuation sets it.
        pan.widget.setStationCalOffsetDb(20.0f);
        QCOMPARE(pan.widget.roundtablePanLevel(viewed, QStringLiteral("lsb")).value_or(0.0), -50.0);
        pan.widget.setDbmCalOffset(3.0f);
        QCOMPARE(pan.widget.roundtablePanLevel(viewed, QStringLiteral("lsb")).value_or(0.0), -47.0);
        pan.widget.setDbmCalOffset(0.0f);
        pan.widget.setStationCalOffsetDb(0.0f);

        // Blank while transmitting.
        pan.widget.setMoxOverlay(true);
        QVERIFY(!pan.widget.roundtablePanLevel(viewed, QStringLiteral("lsb")).has_value());
        pan.widget.setMoxOverlay(false);
        pan.widget.setTxActiveForTest(true);
        QVERIFY(!pan.widget.roundtablePanLevel(viewed, QStringLiteral("lsb")).has_value());
        pan.widget.setTxActiveForTest(false);
        QVERIFY(pan.widget.roundtablePanLevel(viewed, QStringLiteral("lsb")).has_value());
    }

    void thePanLevelMatchesTheTraceWithNormalizeOn()
    {
        Pan pan;
        QVector<float> pixels(kPixels, -120.0f);
        const quint64 viewed = 7188000;
        pixels[static_cast<int>((viewed - 2000 - 7104000) / 1000)] = -70.0f;
        pan.widget.setRenderedPixelsForTest(pixels);
        pan.widget.setSpectrumDetector(SpectrumDetector::Average);
        pan.widget.setDispNormalize(true);
        const double shift = static_cast<double>(pan.widget.fftTraceShiftDbForTest());
        QVERIFY(std::abs(shift) > 1.0);
        QCOMPARE(pan.widget.roundtablePanLevel(viewed, QStringLiteral("lsb")).value_or(0.0), -70.0 + shift);
    }

    // A remote pan's frames carry the Core's calibration already, so the trace
    // adds no operator offset and neither does the level beside it (I1).
    void aRemotePansLevelAddsNoOperatorOffset()
    {
        Pan pan;
        pan.widget.setSpectrumDetector(SpectrumDetector::Average);
        const quint64 viewed = 7188000;
        QVector<float> pixels(kPixels, -120.0f);
        pixels[static_cast<int>((viewed - 2000 - 7104000) / 1000)] = -70.0f;
        pan.widget.setDbmCalOffset(3.0f);
        pan.widget.setStationCalOffsetDb(20.0f);
        pan.widget.setRenderedPixelsForTest(pixels);
        // Local, every term counts: this is what the remote branch must not do.
        QCOMPARE(pan.widget.roundtablePanLevel(viewed, QStringLiteral("lsb")).value_or(0.0), -47.0);
        pan.widget.setRemoteSpectrumContext(SpectrumEndpointContext{}, kPanCenterHz, kPanSpanHz, 4096);
        pan.widget.setRenderedPixelsForTest(pixels);
        QCOMPARE(static_cast<double>(pan.widget.fftTraceShiftDbForTest()), 0.0);
        QCOMPARE(pan.widget.roundtablePanLevel(viewed, QStringLiteral("lsb")).value_or(0.0), -70.0);
    }

    // D22: Tune greys when it would do nothing, and with no slice to move.
    void tuneGreysWhenItWouldWriteNothing()
    {
        Pan pan;
        pan.peek->showFor(kViewedHz, QStringLiteral("usb"));
        const auto tuneEnabled = [&pan](quint64 viewedHz, const char* viewedFamily, quint64 sliceHz, const char* sliceFamily) {
            pan.peek->showFor(viewedHz, QString::fromLatin1(viewedFamily));
            pan.peek->setSlice(sliceHz, QString::fromLatin1(sliceFamily), QStringLiteral("A"));
            RoundtableCardModel card = talker();
            card.hz = viewedHz;
            pan.peek->refresh(card, std::nullopt, false);
            return pan.tune()->isEnabled();
        };
        QVERIFY(!tuneEnabled(7188000, "usb", 7188000, "usb"));
        QVERIFY(tuneEnabled(7188000, "usb", 7188000, "lsb"));   // writes the mode
        QVERIFY(tuneEnabled(7188000, "usb", 7190000, "usb"));   // moves
        // A FreeDV view writes no mode: from another family on this frequency, nothing to do.
        QVERIFY(!tuneEnabled(7188000, "fdvu", 7188000, "usb"));
        QVERIFY(!tuneEnabled(7188000, "fdvl", 7188000, "lsb"));
        QVERIFY(!tuneEnabled(7188000, "fdvl", 7188000, "fdvl"));
        QVERIFY(tuneEnabled(7188000, "fdvu", 7190000, "usb"));

        // No slice this window controls: Tune is in view, greyed, and says no letter.
        pan.peek->showFor(7188000, QStringLiteral("usb"));
        pan.peek->setSlice(0, QString(), QString());
        pan.peek->refresh(talker(), std::nullopt, false);
        QVERIFY(pan.tune()->isVisible());
        QVERIFY(!pan.tune()->isEnabled());
        QCOMPARE(pan.tune()->text(), QStringLiteral("Tune slice here"));
        QCOMPARE(roundtableViewSubText(pan.peek->viewForTest()), QStringLiteral("Viewing"));
    }

    // D7: a dropped link has no live clock, even a stopped one.
    void aFrozenViewHasNoClock()
    {
        Pan pan;
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
        const auto drawn = [&pan](qint64 clockMs, bool frozen) {
            RoundtableCardModel card = talker();
            card.clockMs = clockMs;
            card.frozen = frozen;
            if (frozen) { card.note = QStringLiteral("Reconnecting"); }
            pan.peek->refresh(card, std::nullopt, false);
            return pan.peek->grab().toImage();
        };
        QVERIFY(drawn(83000, false) != drawn(3599000, false));
        QCOMPARE(drawn(83000, true), drawn(3599000, true));
    }

    // D14: a value-only refresh redraws and does not work out the size again.
    void aValueOnlyRefreshDoesNotResizeTheView()
    {
        Pan pan;
        pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
        RoundtableCardModel card = talker();
        pan.peek->refresh(card, -70.0, false);
        const int fits = pan.peek->fitsForTest();
        const QRect geometry = pan.peek->geometry();
        card.clockMs += 1000;
        card.rows[0].dbm -= 5.0;
        card.rows[0].ageMs += 1000;
        pan.peek->refresh(card, -71.0, false);
        QCOMPARE(pan.peek->fitsForTest(), fits);
        QCOMPARE(pan.peek->viewForTest().card.rows.first().dbm, card.rows[0].dbm);
        QCOMPARE(pan.peek->viewForTest().panDbm.value_or(0.0), -71.0);
        QCOMPARE(pan.peek->geometry(), geometry);
        // Your pan's row going away is structure, and so is a new reading.
        pan.peek->refresh(card, std::nullopt, false);
        QCOMPARE(pan.peek->fitsForTest(), fits + 1);
        card.rows.append(rowOf(QStringLiteral("K4NEW"), -60.0));
        pan.peek->refresh(card, std::nullopt, false);
        QCOMPARE(pan.peek->fitsForTest(), fits + 2);
    }

    void capturesForTheBoard_data()
    {
        QTest::addColumn<QString>("scene");
        QTest::newRow("double") << QStringLiteral("double");
        QTest::newRow("talker") << QStringLiteral("talker");
        QTest::newRow("your-pan-blank") << QStringLiteral("your-pan-blank");
        QTest::newRow("reconnecting") << QStringLiteral("reconnecting");
        QTest::newRow("quiet") << QStringLiteral("quiet");
        QTest::newRow("freedv-same-frequency") << QStringLiteral("freedv-same-frequency");
        QTest::newRow("own") << QStringLiteral("own");
        QTest::newRow("viewing") << QStringLiteral("viewing");
        QTest::newRow("viewing-double") << QStringLiteral("viewing-double");
        QTest::newRow("short-fold") << QStringLiteral("short-fold");
        QTest::newRow("short-scroll") << QStringLiteral("short-scroll");
        QTest::newRow("short-120") << QStringLiteral("short-120");
        QTest::newRow("no-slice") << QStringLiteral("no-slice");
        QTest::newRow("flag-flipped") << QStringLiteral("flag-flipped");
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
        const bool freedv = scene == QLatin1String("freedv-same-frequency");
        if (freedv) { pan.peek->setSlice(kViewedHz, QStringLiteral("usb"), QStringLiteral("A")); }

        // The new scenes stand on the board's foot line beside a flag, on the pan.
        const bool flipped = scene == QLatin1String("flag-flipped");
        const bool own = scene == QLatin1String("own") || flipped;
        const bool fold = scene == QLatin1String("short-fold");
        const bool scroll = scene == QLatin1String("short-scroll");
        const bool tiny = scene == QLatin1String("short-120");
        const bool onFoot = own || fold || scroll || tiny || scene == QLatin1String("viewing")
            || scene == QLatin1String("viewing-double") || scene == QLatin1String("no-slice");
        if (scene == QLatin1String("viewing-double")) { card = doubled(); }
        if (scroll || tiny) { card = longTalker(); }
        if (scene == QLatin1String("no-slice")) { pan.peek->setSlice(0, QString(), QString()); }
        const int dialX = flipped ? 800 : 570;
        const QRect flag = flipped ? QRect(dialX - 252, 30, 276, 162) : QRect(dialX - 24, 30, 276, 162);
        const int footTop = fold ? 133 : scroll ? 93 : tiny ? 183 : 32;
        const QRect spot(80, footTop, 1109 - 80, 303 - footTop);
        if (onFoot) {
            pan.peek->setInspectorSpot(spot, dialX, {flag}, flipped);
            if (own) {
                pan.peek->setSlice(kViewedHz, QStringLiteral("lsb"), QStringLiteral("A"));
                pan.peek->showOwn(kViewedHz, QStringLiteral("lsb"));
            } else {
                pan.peek->showFor(kViewedHz, QStringLiteral("lsb"));
            }
        } else {
            pan.peek->showFor(kViewedHz, freedv ? QStringLiteral("fdvu") : QStringLiteral("lsb"));
        }
        pan.peek->refresh(card, level, false);
        if (fold) {
            // Exactly the room the folded list needs, and one row less than the whole.
            const RoundtableViewLayout folded = roundtableViewLayout(pan.peek->viewForTest(), 300, true, pan.peek);
            pan.peek->setInspectorSpot(QRect(80, 303 - (folded.total + 45), 1109 - 80, folded.total + 45), dialX, {flag}, false);
        }
        QImage image;
        if (onFoot) {
            image = QImage(1143, 340, QImage::Format_ARGB32_Premultiplied);
            image.fill(QColor(0x06, 0x07, 0x0a));
            QPainter painter(&image);
            painter.fillRect(flag, QColor(0x22, 0x26, 0x2e));
            painter.setPen(QColor(0x5e, 0x8d, 0xb8));
            painter.drawLine(dialX, 0, dialX, 339);
            painter.setPen(QColor(0x3d, 0x3d, 0x41));
            painter.drawLine(0, 303, 1142, 303);
            pan.peek->render(&painter, pan.peek->pos(), QRegion(), QWidget::DrawChildren);
        } else {
            image = QImage(pan.peek->size() + QSize(80, 80), QImage::Format_ARGB32_Premultiplied);
            image.fill(QColor(0x06, 0x07, 0x0a));
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
