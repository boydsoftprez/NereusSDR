// no-port-check: NereusSDR-original PureSignal presentation contract tests.
#include <QtTest/QtTest>
#include <QLabel>
#include <QTabWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPushButton>
#include <QDir>
#include <QImage>
#include <QScrollArea>
#include <QScrollBar>
#include "core/AppSettings.h"
#include "gui/PsForm.h"
#include "gui/ImdMeasurementPanel.h"
#include "gui/PureSignalInstruments.h"
#include "gui/applets/PureSignalApplet.h"
#include "core/session/PureSignalSessionFacade.h"
#include "gui/styles/WorkspacePreview.h"
#include "models/RadioModel.h"
using namespace NereusSDR;
class TstPureSignalRefreshState : public QObject {
    Q_OBJECT
private slots:
    void init() { AppSettings::instance().clear(); }
    void benchConsumesCachedMeasurementWithoutActions()
    {
        RadioModel radio(RadioModel::Role::Remote);
        PsForm form(&radio, nullptr);
        auto* tabs = form.findChild<QTabWidget*>("PureSignalBenchTabs");
        auto* panel = form.findChild<ImdMeasurementPanel*>("PureSignalBenchImd");
        QVERIFY(tabs);
        QVERIFY(panel);
        ImdMeasurementFrame frame;
        frame.status = ImdStatus::ProductsMissing;
        form.setImdMeasurementFrame(frame);
        QCOMPARE(panel->frame().status, frame.status);
        tabs->setCurrentIndex(1);
        QCOMPARE(panel->frame().status, frame.status);
    }
    void compactAppletHasDistinctInstruments()
    {
        RadioModel radio;
        PureSignalApplet applet(&radio);
        QVERIFY(applet.findChild<QLabel*>("PsAppletCorrectionState"));
        QVERIFY(applet.findChild<QLabel*>("PsAppletFeedbackClass"));
        QVERIFY(applet.findChild<QLabel*>("PsAppletActivity"));
        QVERIFY(!applet.findChild<QWidget*>("PsAppletCorrectionGauge"));
    }
    void feedbackThresholdsAndAppliedStateAreIndependent()
    {
        QCOMPARE(psFeedbackClass(90), 0);
        QCOMPARE(psFeedbackClass(91), 1);
        QCOMPARE(psFeedbackClass(128), 1);
        QCOMPARE(psFeedbackClass(129), 2);
        QCOMPARE(psFeedbackClass(181), 2);
        QCOMPARE(psFeedbackClass(182), 3);
        PureSignalInstruments instruments(QStringLiteral("State"), false);
        auto* correction = instruments.findChild<QLabel*>("StateCorrectionState");
        auto* predicate = instruments.findChild<QLabel*>("StateFeedbackPredicate");
        auto* activity = instruments.findChild<QLabel*>("StateActivity");
        auto* successes = instruments.findChild<QLabel*>("StateSuccessfulCalibrations");
        auto* attempts = instruments.findChild<QLabel*>("StateCalibrationAttempts");
        auto* meter = static_cast<PureSignalFeedbackMeter*>(instruments.findChild<QWidget*>("StateFeedbackMeter"));
        QVERIFY(correction && predicate && activity && successes && attempts && meter);
        Ps3StatusSnapshot status;
        status.feedbackLevel = 90;
        status.engineState = 4;
        status.successfulCalibrations = 7;
        status.attemptedCalibrations = 9;
        instruments.setStatus(status, true);
        QCOMPARE(correction->text(), QStringLiteral("Off"));
        QVERIFY(predicate->text().contains(QStringLiteral("≤90")));
        QVERIFY(activity->text().contains(QStringLiteral("Collect")));
        QVERIFY(successes->text().contains(QStringLiteral("7")));
        QVERIFY(attempts->text().contains(QStringLiteral("9")));
        status.feedbackLevel = 91;
        instruments.setStatus(status, true);
        QCOMPARE(correction->text(), QStringLiteral("Off"));
        QVERIFY(predicate->text().contains(QStringLiteral(">90")));
        status.correctionsApplied = true;
        status.feedbackLevel = 90;
        instruments.setStatus(status, true);
        QCOMPARE(correction->text(), QStringLiteral("● Applied"));
        QVERIFY(predicate->text().contains(QStringLiteral("≤90")));
        status.feedbackLevel = 150;
        instruments.setStatus(status, true);
        QCOMPARE(correction->text(), QStringLiteral("● Applied"));
        QVERIFY(predicate->text().contains(QStringLiteral(">90")));
        status.engineState = 6; // vendored calcc LCALC
        status.correctionBusy = false; // independent IQC update state
        instruments.setStatus(status, true);
        QVERIFY(activity->text().contains(QStringLiteral("Calculate (6)")));
        QVERIFY(!activity->text().contains(QStringLiteral("busy")));
        QVERIFY(!activity->text().contains(QStringLiteral("idle")));
        status.correctionBusy = true;
        instruments.setStatus(status, true);
        QCOMPARE(activity->text(), QStringLiteral("Calibration stage: Calculate (6)"));
        QVERIFY(meter->toolTip().contains(QStringLiteral("retained between collections")));
        QVERIFY(!correction->text().contains(QStringLiteral("distortion"), Qt::CaseInsensitive));
        instruments.setStatus(status, false);
        QCOMPARE(correction->text(), QStringLiteral("Unavailable"));
        QVERIFY(!meter->available());
        QVERIFY(successes->text().contains(QStringLiteral("–")));
    }
    void rawFeedbackAndLegacyDiagnosticHaveHonestCaptions()
    {
        PureSignalInstruments instruments(QStringLiteral("Raw"), false);
        auto* feedback = instruments.findChild<QLabel*>("RawFeedbackLevel");
        auto* diagnostic = instruments.findChild<QLabel*>("RawLegacyInfo13");
        auto* meter = static_cast<PureSignalFeedbackMeter*>(instruments.findChild<QWidget*>("RawFeedbackMeter"));
        QVERIFY(feedback && diagnostic && meter);
        Ps3StatusSnapshot status;
        status.dogCount = 37;
        for (int rawLevel : {256, 300}) {
            status.feedbackLevel = rawLevel;
            instruments.setStatus(status, true);
            QCOMPARE(meter->level(), rawLevel);
            QVERIFY(feedback->text().contains(QString::number(rawLevel)));
            QVERIFY(!feedback->text().contains(QStringLiteral("/ 255")));
            QVERIFY(!meter->toolTip().contains(QStringLiteral("0–255")));
            QCOMPARE(diagnostic->text(), QStringLiteral("Legacy info[13]  37"));
            QVERIFY(!diagnostic->text().contains(QStringLiteral("Rejected"), Qt::CaseInsensitive));
        }
        RadioModel radio;
        PsForm form(&radio, nullptr);
        bool raw13 = false;
        for (const QLabel* label : form.findChildren<QLabel*>()) {
            QVERIFY(!label->text().contains(QStringLiteral("Rejected sets")));
            raw13 = raw13 || label->text() == QStringLiteral("Raw 13 · [13]");
        }
        QVERIFY(raw13);
    }
    void remoteBenchKeepsRawCodesAndClearsOnRetirement()
    {
        RadioModel radio(RadioModel::Role::Remote);
        auto* facade = radio.pureSignalFacade();
        facade->setRemoteCapabilities(true, false);
        QVERIFY(facade->applyRemoteProperty("available", true));
        RadioModel source;
        source.installPureSignalForTest(nullptr);
        QJsonObject object = QJsonDocument::fromJson(source.pureSignalFacade()->statusJson().toUtf8()).object();
        object["feedbackLevel"] = 129;
        object["successfulCalibrations"] = 3;
        object["attemptedCalibrations"] = 5;
        object["engineState"] = 77;
        object["hardwarePeak"] = 0.625;
        QJsonArray raw = object["raw"].toArray();
        while (raw.size() < 16) { raw.append(0); }
        raw[15] = 77;
        object["raw"] = raw;
        QVERIFY(facade->applyRemoteProperty("statusJson", QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact))));
        PsForm form(&radio, nullptr);
        auto* activity = form.findChild<QLabel*>("PsBenchActivity");
        auto* peak = form.findChild<QLabel*>("GetPSpeak");
        auto* rawValue = form.findChild<QLabel*>("lblPSInfo15");
        QVERIFY(activity && peak && rawValue);
        QVERIFY(activity->text().contains(QStringLiteral("77")));
        QCOMPARE(rawValue->text(), QStringLiteral("77"));
        QCOMPARE(peak->text(), QStringLiteral("0.6250"));
        QVERIFY(form.findChild<QLabel*>("lblPSNativeStatus")->text().contains(QStringLiteral("IQ correction run/busy")));
        QCOMPARE(form.findChild<QPushButton*>("btnPSCalibrate")->isEnabled(), false);
        facade->resetSession();
        QCOMPARE(form.findChild<QLabel*>("PsBenchCorrectionState")->text(), QStringLiteral("Unavailable"));
        QVERIFY(!static_cast<PureSignalFeedbackMeter*>(form.findChild<QWidget*>("PsBenchFeedbackMeter"))->available());
    }
    void renderSyntheticUiWhenRequested()
    {
        const QString directory = qEnvironmentVariable("NEREUS_PS_REFRESH_CAPTURE_DIR");
        if (directory.isEmpty()) { QSKIP("NEREUS_PS_REFRESH_CAPTURE_DIR is not set"); }
        QVERIFY(QDir().mkpath(directory));
        auto capture = [&directory](QWidget& widget, const QString& stem) {
            QCoreApplication::processEvents();
            const qreal ratio = widget.devicePixelRatioF();
            QImage image(qRound(widget.width() * ratio), qRound(widget.height() * ratio), QImage::Format_ARGB32_Premultiplied);
            image.setDevicePixelRatio(ratio);
            image.fill(Qt::transparent);
            widget.render(&image);
            return image.save(directory + QLatin1Char('/') + stem + QStringLiteral(".png"));
        };
        for (bool modern : {true, false}) {
            WorkspacePreview::setEnabledForTest(modern);
            const QString style = modern ? QStringLiteral("new") : QStringLiteral("classic");
            RadioModel radio(RadioModel::Role::Remote);
            auto* facade = radio.pureSignalFacade();
            facade->setRemoteCapabilities(true, false);
            QVERIFY(facade->applyRemoteProperty("available", true));
            RadioModel source;
            source.installPureSignalForTest(nullptr);
            QJsonObject object = QJsonDocument::fromJson(source.pureSignalFacade()->statusJson().toUtf8()).object();
            object["feedbackLevel"] = 150;
            object["successfulCalibrations"] = 7;
            object["attemptedCalibrations"] = 9;
            object["correctionsApplied"] = true;
            object["engineState"] = 4;
            object["hardwarePeak"] = 0.625;
            QJsonArray raw = object["raw"].toArray();
            raw[4] = 150;
            raw[5] = 7;
            raw[7] = 9;
            raw[14] = 1;
            raw[15] = 4;
            object["raw"] = raw;
            QVERIFY(facade->applyRemoteProperty("statusJson", QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact))));
            PureSignalApplet applet(&radio);
            applet.resize(320, 345);
            applet.show();
            QVERIFY(capture(applet, QStringLiteral("task4-applet-%1").arg(style)));
            for (int width : {920, 620}) {
                PsForm form(&radio, nullptr);
                form.show();
                form.resize(width, 830);
                QCoreApplication::processEvents();
                QVERIFY(capture(form, QStringLiteral("task4-bench-%1-%2").arg(style).arg(width)));
                auto* tabs = form.findChild<QTabWidget*>("PureSignalBenchTabs");
                tabs->setCurrentIndex(1);
                QVERIFY(capture(form, QStringLiteral("task4-imd-%1-%2").arg(style).arg(width)));
                auto* scroll = form.findChild<QScrollArea*>("PureSignalBenchScroll");
                QVERIFY(scroll);
                scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
                QVERIFY(capture(form, QStringLiteral("task4-diagnostics-%1-%2").arg(style).arg(width)));
            }
        }
        WorkspacePreview::setEnabledForTest(true);
    }
    void benchActionRowsHaveVisibleSeparation()
    {
        for (bool modern : {true, false}) {
            WorkspacePreview::setEnabledForTest(modern);
            for (int width : {920, 620}) {
                PsForm form(nullptr, nullptr);
                form.show();
                form.resize(width, 830);
                QCoreApplication::processEvents();
                auto* top = form.findChild<QPushButton*>("btnPSTwoToneGen");
                auto* bottom = form.findChild<QPushButton*>("btnPSApplyCurrent");
                QVERIFY(top && bottom);
                qInfo().noquote() << "Bench action geometry" << modern << width
                    << "top" << top->geometry() << "bottom" << bottom->geometry()
                    << "minimum" << top->minimumSize() << bottom->minimumSize()
                    << "hints" << top->sizeHint() << bottom->sizeHint();
                QVERIFY2(bottom->geometry().top() - top->geometry().bottom() >= 5,
                    "Bench action rows visually touch or overlap");
            }
        }
        WorkspacePreview::setEnabledForTest(true);
    }
};
QTEST_MAIN(TstPureSignalRefreshState)
#include "tst_puresignal_refresh_state.moc"
