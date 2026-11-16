// =================================================================
// tests/tst_vfo_widget_rxl_tag.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// The RXL tag on the slice flag's "You control" line (JJ, 2026-11-15):
//   1. It reads "RXL +12 dB": whole dB with a sign, a real minus sign
//      (U+2212) for a cut, "RXL 0 dB" at zero.
//   2. It shows only while RXL is on and the mode is one RXL levels; off,
//      or in CW, FM or digital, it is hidden.
//   3. Its text is green turning the station up, amber turning it down,
//      dim at 0.
//   4. It follows SliceModel::rxLevelerGainDb, the reading local and
//      remote windows both have.
//   5. It sits at the right end of the access line ("You control"); on a
//      flag nobody else shares the line shows with the tag alone.
//   6. Folding the flag hides it; unfolding brings it back, and changes
//      while folded show on unfold.
//   7. With NEREUS_FLAG_CAPTURE_DIR set, captures of the flag in the
//      workspace look with the tag at +12, -5 and 0.
//
// Modification history (NereusSDR):
//   2026-11-15 - New. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QApplication>
#include <QDir>
#include <QLabel>
#include <QPixmap>

#include <algorithm>
#include <cmath>

#include "core/AppSettings.h"
#include "gui/StyleConstants.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/widgets/VfoWidget.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

VfoWidget::SliceAccess controlled()
{
    VfoWidget::SliceAccess access;
    access.state = VfoWidget::SliceAccess::State::Controlled;
    access.line = QStringLiteral("You control");
    return access;
}

QColor inkOf(const QLabel* tag)
{
    const QString sheet = tag->styleSheet();
    const int at = sheet.indexOf(QStringLiteral("color: "));
    return at < 0 ? QColor() : QColor::fromString(sheet.mid(at + 7, 7));
}

// A dark host like the workspace, so the capture looks as the flag does over a pan.
class Host : public QWidget
{
public:
    Host()
    {
        setAutoFillBackground(true);
        QPalette p = palette();
        p.setColor(QPalette::Window, QColor(0x0b, 0x0c, 0x0f));
        setPalette(p);
        resize(320, 420);
    }
};

void capture(QWidget* w, const QString& stem)
{
    const QString dir = qEnvironmentVariable("NEREUS_FLAG_CAPTURE_DIR");
    if (dir.isEmpty() || !w) {
        return;
    }
    QDir().mkpath(dir);
    const QPixmap shot = w->grab();
    const int scale = qRound(shot.devicePixelRatio());
    const QString path = QStringLiteral("%1/%2@%3x.png").arg(dir, stem).arg(scale);
    QVERIFY2(shot.save(path), qPrintable(path));
}

} // namespace

class TestVfoWidgetRxlTag : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        AppSettings::instance().clear();
        WorkspacePreview::installLiveTheming(*qobject_cast<QApplication*>(QCoreApplication::instance()));
    }
    void cleanup() { AppSettings::instance().clear(); }

    void text_is_whole_db_with_a_sign()
    {
        QCOMPARE(VfoWidget::rxlTagText(12.0), QStringLiteral("RXL +12 dB"));
        QCOMPARE(VfoWidget::rxlTagText(11.6), QStringLiteral("RXL +12 dB"));
        QCOMPARE(VfoWidget::rxlTagText(-5.0), QString(QStringLiteral("RXL ") + QChar(0x2212) + QStringLiteral("5 dB")));
        QCOMPARE(VfoWidget::rxlTagText(0.0), QStringLiteral("RXL 0 dB"));
        QCOMPARE(VfoWidget::rxlTagText(-0.4), QStringLiteral("RXL 0 dB"));
        QCOMPARE(VfoWidget::rxlTagText(0.4), QStringLiteral("RXL 0 dB"));
        QCOMPARE(VfoWidget::rxlTagText(std::nan("")), QStringLiteral("RXL 0 dB"));
    }

    void shows_only_while_rxl_is_on_in_a_voice_mode()
    {
        VfoWidget flag;
        flag.setSliceIndex(0);
        SliceModel slice(0);
        flag.setSlice(&slice);
        flag.setMode(DSPMode::USB);
        flag.show();
        QVERIFY(!flag.rxlTagForTest()->isVisibleTo(&flag));
        QVERIFY(!flag.topLineForTest()->isVisibleTo(&flag));

        slice.setRxLevelerMode(RxLevelerMode::Auto);
        QVERIFY(flag.rxlTagForTest()->isVisibleTo(&flag));
        QVERIFY(flag.topLineForTest()->isVisibleTo(&flag));

        flag.setMode(DSPMode::CWU);
        QVERIFY(!flag.rxlTagForTest()->isVisibleTo(&flag));
        QVERIFY(!flag.topLineForTest()->isVisibleTo(&flag));
        flag.setMode(DSPMode::FM);
        QVERIFY(!flag.rxlTagForTest()->isVisibleTo(&flag));
        flag.setMode(DSPMode::DIGU);
        QVERIFY(!flag.rxlTagForTest()->isVisibleTo(&flag));
        flag.setMode(DSPMode::LSB);
        QVERIFY(flag.rxlTagForTest()->isVisibleTo(&flag));

        slice.setRxLevelerMode(RxLevelerMode::Off);
        QVERIFY(!flag.rxlTagForTest()->isVisibleTo(&flag));
        QVERIFY(!flag.topLineForTest()->isVisibleTo(&flag));
    }

    void follows_the_slices_reading_and_colours()
    {
        VfoWidget flag;
        flag.setSliceIndex(0);
        SliceModel slice(0);
        slice.setRxLevelerMode(RxLevelerMode::Auto);
        slice.setRxLevelerGainDb(7.0);
        flag.setSlice(&slice);
        flag.setMode(DSPMode::USB);
        flag.show();
        // The reading the slice already has when the flag binds to it.
        QCOMPARE(flag.rxlTagForTest()->text(), QStringLiteral("RXL +7 dB"));

        slice.setRxLevelerGainDb(12.3);
        QCOMPARE(flag.rxlTagForTest()->text(), QStringLiteral("RXL +12 dB"));
        const QColor up = inkOf(flag.rxlTagForTest());

        slice.setRxLevelerGainDb(-4.8);
        QCOMPARE(flag.rxlTagForTest()->text(),
                 QString(QStringLiteral("RXL ") + QChar(0x2212) + QStringLiteral("5 dB")));
        const QColor down = inkOf(flag.rxlTagForTest());

        slice.setRxLevelerGainDb(0.2);
        QCOMPARE(flag.rxlTagForTest()->text(), QStringLiteral("RXL 0 dB"));
        const QColor zero = inkOf(flag.rxlTagForTest());

        QCOMPARE(up, WorkspacePreview::statusColour(QColor(Style::kGreenText), "ok"));
        QCOMPARE(down, WorkspacePreview::statusColour(QColor(Style::kAmberText), "warn"));
        // Dim at 0: the flag's secondary grey, as the look draws it.
        QVERIFY(zero.isValid());
        QVERIFY(std::max({zero.red(), zero.green(), zero.blue()})
                    - std::min({zero.red(), zero.green(), zero.blue()}) < 40);
        QVERIFY(zero != up && zero != down);
        // Green is greener than it is red, amber redder than it is blue.
        QVERIFY(up.green() > up.red());
        QVERIFY(down.red() > down.blue());
        QVERIFY(!flag.rxlTagForTest()->toolTip().isEmpty());
    }

    void sits_at_the_right_end_of_you_control()
    {
        Host host;
        auto* flag = new VfoWidget(&host);
        flag->setSliceIndex(0);
        SliceModel slice(0);
        slice.setRxLevelerMode(RxLevelerMode::Auto);
        slice.setRxLevelerGainDb(12.0);
        flag->setSlice(&slice);
        flag->setMode(DSPMode::USB);
        flag->setSliceAccess(controlled());
        host.show();
        flag->show();
        flag->updatePosition(20, 10);
        QVERIFY(QTest::qWaitForWindowExposed(&host));

        QLabel* tag = flag->rxlTagForTest();
        QWidget* line = flag->topLineForTest();
        QVERIFY(tag->isVisible());
        QCOMPARE(flag->accessLineText(), QStringLiteral("You control"));
        // One line: the tag shares the access line's row, at its right end.
        QCOMPARE(tag->parentWidget(), line);
        QVERIFY(tag->geometry().right() >= line->width() - 2);
        QVERIFY(tag->geometry().left() > line->width() / 2);
    }

    void folding_hides_it_and_unfolding_brings_it_back()
    {
        VfoWidget flag;
        flag.setSliceIndex(1);
        SliceModel slice(1);
        slice.setRxLevelerMode(RxLevelerMode::Auto);
        flag.setSlice(&slice);
        flag.setMode(DSPMode::USB);
        flag.show();
        QVERIFY(flag.topLineForTest()->isVisibleTo(&flag));

        flag.setCollapsed(true);
        QVERIFY(!flag.topLineForTest()->isVisibleTo(&flag));
        slice.setRxLevelerGainDb(9.0);
        QVERIFY(!flag.topLineForTest()->isVisibleTo(&flag));

        flag.setCollapsed(false);
        QVERIFY(flag.topLineForTest()->isVisibleTo(&flag));
        QCOMPARE(flag.rxlTagForTest()->text(), QStringLiteral("RXL +9 dB"));

        // RXL turned off while folded: the line stays away on unfold.
        flag.setCollapsed(true);
        slice.setRxLevelerMode(RxLevelerMode::Off);
        flag.setCollapsed(false);
        QVERIFY(!flag.topLineForTest()->isVisibleTo(&flag));

        // And on while folded: it shows on unfold.
        flag.setCollapsed(true);
        slice.setRxLevelerMode(RxLevelerMode::Custom);
        flag.setCollapsed(false);
        QVERIFY(flag.topLineForTest()->isVisibleTo(&flag));
    }

    void captures()
    {
        if (qEnvironmentVariableIsEmpty("NEREUS_FLAG_CAPTURE_DIR")) {
            QSKIP("NEREUS_FLAG_CAPTURE_DIR not set");
        }
        Host host;
        auto* flag = new VfoWidget(&host);
        flag->setSliceIndex(0);
        SliceModel slice(0);
        slice.setRxLevelerMode(RxLevelerMode::Auto);
        flag->setSlice(&slice);
        flag->setMode(DSPMode::LSB);
        flag->setFrequency(7251000.0);
        flag->setSliceAccess(controlled());
        host.show();
        flag->show();
        flag->updatePosition(20, 10);
        QVERIFY(QTest::qWaitForWindowExposed(&host));

        for (const auto& [dB, stem] : {std::pair{12.0, "rxl-tag-up"}, std::pair{-5.0, "rxl-tag-down"},
                                       std::pair{0.0, "rxl-tag-zero"}}) {
            slice.setRxLevelerGainDb(dB);
            QCoreApplication::processEvents();
            capture(flag, QString::fromLatin1(stem));
        }
        // A flag nobody else shares: the tag alone on the line.
        flag->setSliceAccess(VfoWidget::SliceAccess{});
        slice.setRxLevelerGainDb(12.0);
        QCoreApplication::processEvents();
        capture(flag, QStringLiteral("rxl-tag-unshared"));
        flag->setCollapsed(true);
        QCoreApplication::processEvents();
        capture(flag, QStringLiteral("rxl-tag-folded"));
    }
};

QTEST_MAIN(TestVfoWidgetRxlTag)
#include "tst_vfo_widget_rxl_tag.moc"
