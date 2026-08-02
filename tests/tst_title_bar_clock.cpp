// no-port-check: NereusSDR-original. No upstream port.
// =================================================================
// tests/tst_title_bar_clock.cpp  (NereusSDR)
// =================================================================
//
// Task A7 (bottom-banner cleanup): TitleBar::utcText() smoke tests.
//
// Coverage:
//   1. utcTextIsPopulatedBeforeTheFirstTick — the label is filled at
//      construction time, not left blank until the first 1 s timer fire.
//   2. utcTextMatchesTheSingleRowFormat — exact "HH:MM:SS UTC" shape.
//      Fully anchored (^...$) so it rejects anything with extra content,
//      including a second row (date/local time) appended after the time
//      the old two-row banner clock used to show.
//   3. utcTextHasNoDateRow — belt-and-suspenders: no hyphen anywhere in
//      the string (a "yyyy-MM-dd" date component would introduce one).
//
// TitleBar requires a real AudioEngine* (no default-constructible
// overload), so these tests follow the same construction pattern as the
// sibling tst_title_bar.cpp rather than building a bare `TitleBar t;`.
// =================================================================

#include <QtTest/QtTest>
#include <QRegularExpression>

#include "core/AudioEngine.h"
#include "gui/TitleBar.h"

using namespace NereusSDR;

class TstTitleBarClock : public QObject {
    Q_OBJECT

private slots:

    void utcTextIsPopulatedBeforeTheFirstTick() {
        AudioEngine engine;
        TitleBar t(&engine);
        QVERIFY(!t.utcText().isEmpty());
    }

    void utcTextMatchesTheSingleRowFormat() {
        AudioEngine engine;
        TitleBar t(&engine);
        const QRegularExpression re(
            QStringLiteral("^[0-2][0-9]:[0-5][0-9]:[0-5][0-9] UTC$"));
        QVERIFY2(re.match(t.utcText()).hasMatch(), qPrintable(t.utcText()));
    }

    void utcTextHasNoDateRow() {
        AudioEngine engine;
        TitleBar t(&engine);
        QVERIFY(!t.utcText().contains(QLatin1Char('-')));
    }
};
QTEST_MAIN(TstTitleBarClock)
#include "tst_title_bar_clock.moc"
