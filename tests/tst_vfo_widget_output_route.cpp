// no-port-check: NereusSDR-original test.
//
// R-R3-45 (VAX design 6.2): the Speakers and Headphones buttons in a slice
// flag's audio block. They are exclusive, they write the bound slice's
// route and follow it back, and with the headphones chosen and no
// headphones output open the flag says why the receiver is silent, in
// plain words. 2026-09-23, J.J. Boyd (KG4VCF), AI-assisted via Anthropic
// Claude Code.

#include <QtTest/QtTest>

#include <QLabel>
#include <QPushButton>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "gui/RemoteMediaController.h"
#include "gui/widgets/VfoWidget.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TstVfoWidgetOutputRoute : public QObject {
    Q_OBJECT

private:
    struct Parts {
        QPushButton* speakers{nullptr};
        QPushButton* headphones{nullptr};
        QLabel* notice{nullptr};
    };

    static Parts partsOf(VfoWidget& flag)
    {
        Parts p;
        p.speakers = flag.findChild<QPushButton*>(QStringLiteral("outputSpeakersButton"));
        p.headphones = flag.findChild<QPushButton*>(QStringLiteral("outputHeadphonesButton"));
        p.notice = flag.findChild<QLabel*>(QStringLiteral("outputRouteNotice"));
        return p;
    }

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // The operator's captions (2026-09-24), in capitals like the flag's
    // other buttons, with the tooltips in full plain words.
    void captionsAndTooltips()
    {
        VfoWidget flag;
        const Parts p = partsOf(flag);
        QVERIFY(p.speakers && p.headphones);
        QCOMPARE(p.speakers->text(), QStringLiteral("SPEAKERS"));
        QCOMPARE(p.headphones->text(), QStringLiteral("PHONES"));
        QCOMPARE(p.speakers->toolTip(), QStringLiteral("Play this receiver on the speakers"));
        QCOMPARE(p.headphones->toolTip(), QStringLiteral("Play this receiver on the headphones"));
    }

    void buttonsAreExclusiveAndDriveTheSlice()
    {
        SliceModel slice;
        slice.setSliceIndex(1);
        VfoWidget flag;
        flag.setSlice(&slice);
        const Parts p = partsOf(flag);
        QVERIFY(p.speakers && p.headphones && p.notice);
        QVERIFY(p.speakers->isChecked());
        QVERIFY(!p.headphones->isChecked());

        p.headphones->click();
        QCOMPARE(slice.outputRoute(), SliceModel::OutputRoute::Headphones);
        QVERIFY(p.headphones->isChecked());
        QVERIFY(!p.speakers->isChecked());

        // Clicking the chosen one keeps it chosen.
        p.headphones->click();
        QVERIFY(p.headphones->isChecked());
        QCOMPARE(slice.outputRoute(), SliceModel::OutputRoute::Headphones);

        p.speakers->click();
        QCOMPARE(slice.outputRoute(), SliceModel::OutputRoute::Speakers);
        QVERIFY(p.speakers->isChecked());
        QVERIFY(!p.headphones->isChecked());
    }

    void followsTheSlice()
    {
        SliceModel slice;
        slice.setOutputRoute(SliceModel::OutputRoute::Headphones);
        VfoWidget flag;
        flag.setSlice(&slice);
        const Parts p = partsOf(flag);
        QVERIFY(p.headphones->isChecked());

        slice.setOutputRoute(SliceModel::OutputRoute::Speakers);
        QVERIFY(p.speakers->isChecked());
        QVERIFY(!p.headphones->isChecked());
    }

    void saysWhyItIsSilentWithNoHeadphones()
    {
        SliceModel slice;
        VfoWidget flag;
        flag.setSlice(&slice);
        const Parts p = partsOf(flag);

        flag.setHeadphonesAvailable(false);
        QVERIFY(p.notice->isHidden());          // on the speakers: nothing to say

        slice.setOutputRoute(SliceModel::OutputRoute::Headphones);
        QVERIFY(!p.notice->isHidden());
        QCOMPARE(p.notice->text(), VfoWidget::headphonesMissingText());

        flag.setHeadphonesAvailable(true);
        QVERIFY(p.notice->isHidden());
        flag.setHeadphonesAvailable(false);
        QVERIFY(!p.notice->isHidden());

        slice.setOutputRoute(SliceModel::OutputRoute::Speakers);
        QVERIFY(p.notice->isHidden());
    }

    // R-R3-45 fix wave: headphones turned on in Setup that did not open
    // (unplugged, or refusing the format) say so, instead of asking the
    // operator to turn them on.
    void saysTheHeadphonesCouldNotBeOpened()
    {
        SliceModel slice;
        VfoWidget flag;
        flag.setSlice(&slice);
        const Parts p = partsOf(flag);
        slice.setOutputRoute(SliceModel::OutputRoute::Headphones);

        flag.setHeadphonesAvailable(false);
        flag.setHeadphonesEnabled(false);
        QVERIFY(!p.notice->isHidden());
        QCOMPARE(p.notice->text(), VfoWidget::headphonesMissingText());

        flag.setHeadphonesEnabled(true);
        QVERIFY(!p.notice->isHidden());
        QCOMPARE(p.notice->text(), VfoWidget::headphonesNotOpenedText());
        QCOMPARE(p.notice->text(),
                 QStringLiteral("Silent: the headphones could not be opened."));

        flag.setHeadphonesAvailable(true);
        QVERIFY(p.notice->isHidden());
        slice.setOutputRoute(SliceModel::OutputRoute::Speakers);
    }

    // R-R3-45: a remote window's own reason (the Core cannot send the
    // headphones mix, so the receiver plays on the speakers; or the
    // headphones failed) comes first, whatever this computer's headphones.
    void saysARemoteWindowsReason()
    {
        SliceModel slice;
        VfoWidget flag;
        flag.setSlice(&slice);
        const Parts p = partsOf(flag);
        const QString reason = QString::fromLatin1(
            RemoteMediaController::kHeadphonesMixUnavailableReason);
        flag.setHeadphonesAvailable(true);
        flag.setHeadphonesProblem(reason);
        QVERIFY(p.notice->isHidden());          // on the speakers: nothing to say

        slice.setOutputRoute(SliceModel::OutputRoute::Headphones);
        QVERIFY(!p.notice->isHidden());
        QCOMPARE(p.notice->text(), reason);

        // An older Core plays the receiver on the speakers: not "Silent",
        // even with no headphones set up here.
        flag.setHeadphonesAvailable(false);
        QCOMPARE(p.notice->text(), reason);
        QVERIFY(!p.notice->text().startsWith(QStringLiteral("Silent")));
        flag.setHeadphonesEnabled(true);
        QCOMPARE(p.notice->text(), reason);

        flag.setHeadphonesProblem(QString());
        QCOMPARE(p.notice->text(), VfoWidget::headphonesNotOpenedText());
        flag.setHeadphonesAvailable(true);
        QVERIFY(p.notice->isHidden());
    }

    void wordsArePlain()
    {
        VfoWidget flag;
        const Parts p = partsOf(flag);
        for (const QString& text : {p.speakers->text(), p.speakers->toolTip(),
                                    p.headphones->text(), p.headphones->toolTip(),
                                    VfoWidget::headphonesMissingText(),
                                    VfoWidget::headphonesNotOpenedText(),
                                    QString::fromLatin1(
                                        RemoteMediaController::kHeadphonesMixUnavailableReason)}) {
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
        }
    }
};

QTEST_MAIN(TstVfoWidgetOutputRoute)
#include "tst_vfo_widget_output_route.moc"
