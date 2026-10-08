// =================================================================
// tests/tst_vfo_widget_audio_tab.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Radio speaker and Audio Setup plan, Task 7 (R-SPK-18, D6): the VFO flag's
// audio tab (objectName audioTabButton) shows the PC speaker icon, pc-on or
// pc-muted, following the mute the flag's audio controls show.
//
// Coverage:
//   1. The tab starts as pc-on, with no emoji text.
//   2. SliceModel::setMuted (as the RX applet, a remote window or the phone
//      writes it) turns the icon to pc-muted and back to pc-on.
//   3. The flag's own Mute button does the same, through the slice.
//   4. A listened flag follows its own listen mute, not the slice's.
//   5. Clicking the tab still opens the audio controls.
//   6. With NEREUS_FLAG_CAPTURE_DIR set, captures of the flag and its tab,
//      playing and muted, are saved (run once plain and once with
//      QT_SCALE_FACTOR=2 for 1x and 2x).
//
// Modification history (NereusSDR):
//   2026-10-06 - Written for the radio speaker and Audio Setup plan, Task 7.
//                J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                Anthropic Claude Code.
// =================================================================
#include <QtTest/QtTest>
#include <QDir>
#include <QPushButton>
#include <QSlider>

#include "core/AppSettings.h"
#include "gui/SliceFlagPresentationBinding.h"
#include "gui/widgets/AppIcon.h"
#include "gui/widgets/VfoWidget.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

QPushButton* audioTab(VfoWidget& flag)
{
    return flag.findChild<QPushButton*>(QStringLiteral("audioTabButton"));
}

QString iconName(const QWidget* w)
{
    return w ? w->property(AppIcon::kIconProperty).toString() : QString();
}

// The model-to-flag and flag-to-model mute wiring MainWindow uses.
void wireLikeMainWindow(VfoWidget& flag, SliceModel& slice)
{
    wireSliceFlagPresentation(&slice, &flag);
    QObject::connect(&flag, &VfoWidget::muteChanged,
                     &slice, &SliceModel::setMuted);
}

VfoWidget::SliceAccess listened()
{
    VfoWidget::SliceAccess access;
    access.state = VfoWidget::SliceAccess::State::Listening;
    access.line = QStringLiteral("Listening · controlled by Shack iPad");
    access.heldReason = QStringLiteral("Shack iPad controls this slice");
    return access;
}

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

class TestVfoWidgetAudioTab : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    void tab_starts_with_the_pc_on_icon()
    {
        VfoWidget flag;
        QPushButton* tab = audioTab(flag);
        QVERIFY(tab != nullptr);
        QCOMPARE(iconName(tab), QStringLiteral("pc-on"));
        QVERIFY(tab->text().isEmpty());
        QVERIFY(!tab->icon().isNull());
    }

    void slice_mute_from_elsewhere_drives_the_icon()
    {
        SliceModel slice(0);
        VfoWidget flag;
        flag.setSliceIndex(0);
        wireLikeMainWindow(flag, slice);
        QPushButton* tab = audioTab(flag);
        QVERIFY(tab != nullptr);

        slice.setMuted(true);
        QCOMPARE(iconName(tab), QStringLiteral("pc-muted"));
        QVERIFY(flag.muteButtonForTest()->isChecked());

        slice.setMuted(false);
        QCOMPARE(iconName(tab), QStringLiteral("pc-on"));
        QVERIFY(!flag.muteButtonForTest()->isChecked());
    }

    void flag_mute_button_drives_the_icon()
    {
        SliceModel slice(0);
        VfoWidget flag;
        flag.setSliceIndex(0);
        wireLikeMainWindow(flag, slice);
        QPushButton* tab = audioTab(flag);
        QVERIFY(tab != nullptr);

        flag.muteButtonForTest()->click();
        QVERIFY(slice.muted());
        QCOMPARE(iconName(tab), QStringLiteral("pc-muted"));

        flag.muteButtonForTest()->click();
        QVERIFY(!slice.muted());
        QCOMPARE(iconName(tab), QStringLiteral("pc-on"));
    }

    void listened_flag_follows_its_listen_mute()
    {
        SliceModel slice(1);
        VfoWidget flag;
        flag.setSliceIndex(1);
        wireLikeMainWindow(flag, slice);
        QPushButton* tab = audioTab(flag);
        QVERIFY(tab != nullptr);

        // The slice is muted for everyone, but this device listens unmuted.
        slice.setMuted(true);
        QCOMPARE(iconName(tab), QStringLiteral("pc-muted"));
        flag.setListenVolume(60, false);
        flag.setSliceAccess(listened());
        QVERIFY(flag.isListening());
        QCOMPARE(iconName(tab), QStringLiteral("pc-on"));

        // The slice's mute changing does not move a listened flag's icon.
        slice.setMuted(false);
        slice.setMuted(true);
        QCOMPARE(iconName(tab), QStringLiteral("pc-on"));

        // This device's own mute does, from the model and from the button.
        flag.setListenVolume(60, true);
        QCOMPARE(iconName(tab), QStringLiteral("pc-muted"));
        flag.muteButtonForTest()->click();
        QVERIFY(!flag.listenMuted());
        QCOMPARE(iconName(tab), QStringLiteral("pc-on"));
        QVERIFY(slice.muted());
    }

    void clicking_the_tab_opens_the_audio_controls()
    {
        VfoWidget flag;
        flag.show();
        QVERIFY(QTest::qWaitForWindowExposed(&flag));
        QPushButton* tab = audioTab(flag);
        QVERIFY(tab != nullptr);
        QSlider* af = flag.afSliderForTest();
        QVERIFY(af != nullptr);

        // The flag starts compact; a click opens the audio page.
        if (af->isVisible()) {
            tab->click();
            QVERIFY(!af->isVisible());
        }
        tab->click();
        QVERIFY(tab->isChecked());
        QVERIFY(af->isVisible());
        QVERIFY(flag.muteButtonForTest()->isVisible());
    }

    void captures_playing_and_muted()
    {
        SliceModel slice(0);
        VfoWidget flag;
        flag.setSliceIndex(0);
        flag.setFrequency(14'074'000.0);
        wireLikeMainWindow(flag, slice);
        flag.show();
        QVERIFY(QTest::qWaitForWindowExposed(&flag));
        QPushButton* tab = audioTab(flag);
        QVERIFY(tab != nullptr);
        flag.showTab(VfoWidget::Tab::Audio);
        QVERIFY(flag.afSliderForTest()->isVisible());

        slice.setMuted(false);
        QCOMPARE(iconName(tab), QStringLiteral("pc-on"));
        capture(&flag, QStringLiteral("flag-audio-playing"));
        capture(tab, QStringLiteral("flag-audio-tab-playing"));

        slice.setMuted(true);
        QCOMPARE(iconName(tab), QStringLiteral("pc-muted"));
        capture(&flag, QStringLiteral("flag-audio-muted"));
        capture(tab, QStringLiteral("flag-audio-tab-muted"));
    }
};

QTEST_MAIN(TestVfoWidgetAudioTab)
#include "tst_vfo_widget_audio_tab.moc"
