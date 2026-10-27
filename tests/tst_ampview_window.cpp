// no-port-check: NereusSDR-original tests for the Qt AmpView presentation.

#include <QtTest/QtTest>

#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QGuiApplication>
#include <QLabel>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QScreen>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/session/PureSignalSessionFacade.h"
#include "gui/AmpViewChart.h"
#include "gui/AmpViewWindow.h"
#include "gui/styles/WorkspacePreview.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

Ps3Snapshot displaySnapshot(quint64 generation, quint64 sequence)
{
    Ps3Snapshot snapshot;
    snapshot.channelId = 0;
    snapshot.sessionGeneration = generation;
    snapshot.sequence = sequence;
    snapshot.capturedAtUnixMilliseconds = QDateTime::currentMSecsSinceEpoch();
    snapshot.sampleCount = 2;
    snapshot.correctionCount = 2;
    snapshot.x = {0.25, 0.75};
    snapshot.ym = {0.5, 0.8};
    snapshot.yc = {1.0, 0.0};
    snapshot.ys = {0.0, 1.0};
    snapshot.xmCorrection = {0.2, 0.7};
    snapshot.ymCorrection = {1.1, 1.2};
    snapshot.xaCorrection = {0.3, 0.9};
    snapshot.yaCorrection = {-12.0, 18.0};
    return snapshot;
}

Ps3Snapshot denseDisplaySnapshot(quint64 generation, quint64 sequence)
{
    Ps3Snapshot snapshot = displaySnapshot(generation, sequence);
    constexpr int kPoints = 65;
    snapshot.sampleCount = kPoints;
    snapshot.correctionCount = kPoints;
    snapshot.x.clear();
    snapshot.ym.clear();
    snapshot.yc.clear();
    snapshot.ys.clear();
    snapshot.xmCorrection.clear();
    snapshot.ymCorrection.clear();
    snapshot.xaCorrection.clear();
    snapshot.yaCorrection.clear();
    for (int index = 0; index < kPoints; ++index) {
        const double input = static_cast<double>(index) / (kPoints - 1);
        snapshot.x.push_back(input);
        snapshot.ym.push_back(0.9 + 0.1 * input);
        snapshot.yc.push_back(1.0);
        snapshot.ys.push_back((input - 0.5) * 0.35);
        snapshot.xmCorrection.push_back(input);
        snapshot.ymCorrection.push_back(1.05 - 0.08 * input);
        snapshot.xaCorrection.push_back(input);
        snapshot.yaCorrection.push_back(-9.0 + 18.0 * input);
    }
    return snapshot;
}

bool intersectsAvailableScreen(const QRect& geometry)
{
    for (QScreen* screen : QGuiApplication::screens()) {
        if (screen && screen->availableGeometry().intersects(geometry)) {
            return true;
        }
    }
    return QGuiApplication::screens().isEmpty();
}

bool captureIfRequested(QWidget& widget, const QString& stem)
{
    const QString directory = qEnvironmentVariable("NEREUS_DSP_UI_CAPTURE_DIR");
    if (directory.isEmpty()) {
        return true;
    }
    if (!QDir().mkpath(directory)) {
        return false;
    }
    QString scale = qEnvironmentVariable("QT_SCALE_FACTOR", QStringLiteral("1"));
    scale.replace(QLatin1Char('.'), QLatin1Char('_'));
    const qreal dpr = widget.devicePixelRatioF();
    QImage image(QSize(qRound(widget.width() * dpr), qRound(widget.height() * dpr)),
                 QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    widget.render(&painter);
    painter.end();
    return image.save(QDir(directory).filePath(stem + QStringLiteral("-scale-") + scale
                                               + QStringLiteral(".png")), "PNG");
}

} // namespace

class TstAmpViewWindow : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); WorkspacePreview::setEnabledForTest(true); }

    void preservesLegacyWindowContract()
    {
        AmpViewWindow window;
        QCOMPARE(window.windowTitle(), QStringLiteral("AmpView 1.0"));
        QCOMPARE(window.minimumSize(), QSize(440, 380));
        QVERIFY(!window.isModal());

        QVERIFY(window.findChild<QCheckBox*>(QStringLiteral("chkAVShowGain")));
        QVERIFY(window.findChild<QCheckBox*>(QStringLiteral("chkAVPhaseZoom")));
        QVERIFY(window.findChild<QCheckBox*>(QStringLiteral("chkAVLowRes")));
        QVERIFY(window.findChild<QCheckBox*>(QStringLiteral("chkStayOnTop")));

        auto* chart = window.findChild<AmpViewChart*>(QStringLiteral("ampViewChart"));
        QVERIFY(chart);
        QCOMPARE(chart->seriesCount(), 5);
        QCOMPARE(chart->seriesNames(),
                 QStringList({QStringLiteral("Reference"),
                              QStringLiteral("Measured magnitude"),
                              QStringLiteral("Measured phase"),
                              QStringLiteral("Correction magnitude"),
                              QStringLiteral("Correction phase")}));
    }

    void persistentModesAndSessionLocalSeries()
    {
        {
            AmpViewWindow window;
            window.findChild<QCheckBox*>(QStringLiteral("chkAVShowGain"))->setChecked(true);
            window.findChild<QCheckBox*>(QStringLiteral("chkAVPhaseZoom"))->setChecked(true);
            window.findChild<QCheckBox*>(QStringLiteral("chkAVLowRes"))->setChecked(false);
            window.findChild<QCheckBox*>(QStringLiteral("chkStayOnTop"))->setChecked(true);
            window.findChild<QCheckBox*>(QStringLiteral("chkAVMeasuredPhase"))->setChecked(false);
            window.findChild<QCheckBox*>(QStringLiteral("chkAVCorrectionMagnitude"))->setChecked(false);
        }

        // Old persisted series keys remain untouched but do not set a new
        // window session's visibility in the linked-plots design.
        AppSettings::instance().setValue(QStringLiteral("ampview/showMeasuredPhase"),
                                         QStringLiteral("False"));
        AppSettings::instance().setValue(QStringLiteral("ampview/showCorrectionMagnitude"),
                                         QStringLiteral("False"));
        AmpViewWindow restored;
        QVERIFY(restored.findChild<QCheckBox*>(QStringLiteral("chkAVShowGain"))->isChecked());
        QVERIFY(restored.findChild<QCheckBox*>(QStringLiteral("chkAVPhaseZoom"))->isChecked());
        QVERIFY(!restored.findChild<QCheckBox*>(QStringLiteral("chkAVLowRes"))->isChecked());
        QVERIFY(restored.findChild<QCheckBox*>(QStringLiteral("chkStayOnTop"))->isChecked());
        QVERIFY(restored.findChild<QCheckBox*>(QStringLiteral("chkAVMeasuredPhase"))->isChecked());
        QVERIFY(restored.findChild<QCheckBox*>(QStringLiteral("chkAVCorrectionMagnitude"))->isChecked());

        auto* chart = restored.findChild<AmpViewChart*>(QStringLiteral("ampViewChart"));
        QVERIFY(chart->showGain());
        QVERIFY(restored.findChild<QLabel*>(QStringLiteral("ampViewCursorReadout"))
                    ->text().contains(QStringLiteral("AMP GAIN")));
        QVERIFY(chart->phaseZoom());
        QVERIFY(!chart->lowRes());
        QVERIFY(chart->seriesVisible(AmpViewChart::Series::MeasuredPhase));
        QVERIFY(chart->seriesVisible(AmpViewChart::Series::CorrectionMagnitude));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("ampview/showMeasuredPhase"))
                     .toString(), QStringLiteral("False"));
    }

    void visibilityControlsFacadeSubscription()
    {
        RadioModel radio(RadioModel::Role::Remote);
        auto* facade = radio.pureSignalFacade();
        QVERIFY(facade);
        AmpViewWindow window(&radio, nullptr);

        QVERIFY(!facade->ampViewSubscribed());
        window.show();
        QCoreApplication::processEvents();
        QVERIFY(facade->ampViewSubscribed());

        window.hide();
        QCoreApplication::processEvents();
        QVERIFY(!facade->ampViewSubscribed());

        window.show();
        QCoreApplication::processEvents();
        QVERIFY(facade->ampViewSubscribed());
        auto* phase = window.findChild<QCheckBox*>(QStringLiteral("chkAVMeasuredPhase"));
        phase->setChecked(false);
        window.close();
        QCoreApplication::processEvents();
        QVERIFY(!facade->ampViewSubscribed());
        window.show();
        QCoreApplication::processEvents();
        QVERIFY(!phase->isChecked());
        window.close();
    }

    void snapshotUsesFacadeAndDoesNotActuate()
    {
        RadioModel radio(RadioModel::Role::Remote);
        auto* facade = radio.pureSignalFacade();
        QVERIFY(facade);
        facade->setRemoteCapabilities(true, false);
        QVERIFY(facade->applyRemoteProperty("available", true));
        QSignalSpy actions(facade, &PureSignalSessionFacade::actionResult);

        // The RadioModel-owned facade always wins when both legacy arguments
        // are present; a second facade would compete for subscriptions.
        AmpViewWindow window(&radio, radio.pureSignal());
        window.resize(520, 400);
        window.show();
        QCoreApplication::processEvents();

        const Ps3Snapshot snapshot = displaySnapshot(facade->displayGeneration(), 1);
        facade->receiveDisplaySnapshot(snapshot);
        QCoreApplication::processEvents();

        auto* chart = window.findChild<AmpViewChart*>(QStringLiteral("ampViewChart"));
        QVERIFY(chart);
        QCOMPARE(chart->plotData().measuredMagnitude.size(), std::size_t{2});
        QCOMPARE(chart->plotData().correctionMagnitude.at(0).x, 0.2);
        QCOMPARE(chart->plotData().correctionPhase.at(0).x, 0.3);
        chart->setSelectedInput(0.5);
        QVERIFY(window.findChild<QLabel*>(QStringLiteral("ampViewCursorReadout"))
                    ->text().contains(QStringLiteral("INPUT 0.50")));
        QVERIFY(window.findChild<QLabel*>(QStringLiteral("ampViewCursorReadout"))
                    ->text().contains(QStringLiteral("AMP MAG")));
        const std::size_t originalCount = chart->plotData().measuredMagnitude.size();
        Ps3Snapshot duplicate = displaySnapshot(facade->displayGeneration(), 1);
        duplicate.x[0] = 0.95;
        facade->receiveDisplaySnapshot(duplicate);
        QCoreApplication::processEvents();
        QCOMPARE(chart->plotData().measuredMagnitude.size(), originalCount);
        QCOMPARE(chart->plotData().measuredMagnitude.at(0).y, 0.25);
        Ps3Snapshot retired = displaySnapshot(facade->displayGeneration() - 1, 77);
        retired.x[0] = 0.95;
        facade->receiveDisplaySnapshot(retired);
        QCoreApplication::processEvents();
        QCOMPARE(chart->plotData().measuredMagnitude.size(), originalCount);
        QCOMPARE(chart->plotData().measuredMagnitude.at(0).y, 0.25);
        QCOMPARE(actions.count(), 0);
        QVERIFY(window.findChild<QLabel*>(QStringLiteral("ampViewDisplayStatus"))
                    ->text().contains(QStringLiteral("Retained")));
        RadioModel source;
        source.installPureSignalForTest(nullptr);
        QJsonObject status = QJsonDocument::fromJson(
            source.pureSignalFacade()->statusJson().toUtf8()).object();
        status["mox"] = true;
        QVERIFY(facade->applyRemoteProperty("statusJson",
            QString::fromUtf8(QJsonDocument(status).toJson(QJsonDocument::Compact))));
        QCoreApplication::processEvents();
        QVERIFY(window.findChild<QLabel*>(QStringLiteral("ampViewDisplayStatus"))
                    ->text().contains(QStringLiteral("Retained")));
        facade->receiveDisplaySnapshot(displaySnapshot(facade->displayGeneration(), 2));
        QCoreApplication::processEvents();
        QVERIFY(window.findChild<QLabel*>(QStringLiteral("ampViewDisplayStatus"))
                    ->text().contains(QStringLiteral("Current")));
        QVERIFY(captureIfRequested(window, QStringLiteral("ampview-current")));
        Ps3Snapshot oldCapture = displaySnapshot(facade->displayGeneration(), 3);
        oldCapture.capturedAtUnixMilliseconds -= 5000;
        facade->receiveDisplaySnapshot(oldCapture);
        QCoreApplication::processEvents();
        QVERIFY(window.findChild<QLabel*>(QStringLiteral("ampViewDisplayStatus"))
                    ->text().contains(QStringLiteral("Retained")));
        Ps3Snapshot malformed = displaySnapshot(facade->displayGeneration(), 4);
        malformed.ys.clear();
        facade->receiveDisplaySnapshot(malformed);
        QCoreApplication::processEvents();
        QCOMPARE(chart->plotData().measuredMagnitude.size(), originalCount);
        QVERIFY(window.findChild<QLabel*>(QStringLiteral("ampViewDisplayStatus"))
                    ->text().contains(QStringLiteral("Retained")));
        status["mox"] = false;
        QVERIFY(facade->applyRemoteProperty("statusJson",
            QString::fromUtf8(QJsonDocument(status).toJson(QJsonDocument::Compact))));
        QCoreApplication::processEvents();
        QVERIFY(captureIfRequested(window, QStringLiteral("ampview-populated")));
    }

    void sessionInvalidationClearsOldPlot()
    {
        RadioModel radio(RadioModel::Role::Remote);
        auto* facade = radio.pureSignalFacade();
        facade->setRemoteCapabilities(true, false);
        AmpViewWindow window(&radio);
        window.show();
        QCoreApplication::processEvents();

        facade->receiveDisplaySnapshot(displaySnapshot(facade->displayGeneration(), 5));
        QCoreApplication::processEvents();
        auto* chart = window.findChild<AmpViewChart*>(QStringLiteral("ampViewChart"));
        QVERIFY(!chart->plotData().measuredMagnitude.empty());

        facade->resetSession();
        QCoreApplication::processEvents();
        QVERIFY(chart->plotData().measuredMagnitude.empty());
        QVERIFY(window.findChild<QLabel*>(QStringLiteral("ampViewDisplayStatus"))
                    ->text().contains(QStringLiteral("unavailable")));
        QVERIFY(window.findChild<QLabel*>(QStringLiteral("ampViewCursorReadout"))
                    ->text().contains(QStringLiteral("AMP MAG –")));
        QVERIFY(captureIfRequested(window, QStringLiteral("ampview-unavailable")));

        // A queued frame from the retired identity cannot repopulate the plot.
        facade->receiveDisplaySnapshot(displaySnapshot(facade->displayGeneration() - 1, 99));
        QCoreApplication::processEvents();
        QVERIFY(chart->plotData().measuredMagnitude.empty());
    }

    void chartModesAndIndependentSeriesVisibility()
    {
        AmpViewChart chart;
        QCOMPARE(chart.magYMax(), 1.0);
        chart.setShowGain(true);
        QCOMPARE(chart.magYMax(), 2.0);
        chart.setPhaseZoom(true);
        QCOMPARE(chart.phaseYMin(), -45.0);
        QCOMPARE(chart.phaseYMax(), 45.0);
        chart.setLowRes(false);
        QCOMPARE(chart.lowResStride(), 1);

        for (int index = 0; index < chart.seriesCount(); ++index) {
            const auto series = static_cast<AmpViewChart::Series>(index);
            QVERIFY(chart.seriesVisible(series));
            chart.setSeriesVisible(series, false);
            QVERIFY(!chart.seriesVisible(series));
        }
    }

    void offscreenGeometryIsRecovered()
    {
        {
            AmpViewWindow window;
            window.setGeometry(-100000, -100000, 564, 401);
            window.close();
        }

        AmpViewWindow restored;
        QVERIFY(intersectsAvailableScreen(restored.frameGeometry()));
        QVERIFY(restored.width() >= 440);
        QVERIFY(restored.height() >= 380);
    }

    void linkedPlotsRenderInBothLooksAndWidths()
    {
        RadioModel radio(RadioModel::Role::Remote);
        auto* facade = radio.pureSignalFacade();
        facade->setRemoteCapabilities(true, false);
        QVERIFY(facade->applyRemoteProperty("available", true));
        AmpViewWindow window(&radio);
        quint64 nextSequence = 1;
        for (bool themed : {true, false}) {
            WorkspacePreview::setEnabledForTest(themed);
            for (int width : {564, 440}) {
                window.resize(width, 470);
                window.show();
                WorkspacePreview::themeSubtree(&window);
                QCoreApplication::processEvents();
                facade->receiveDisplaySnapshot(displaySnapshot(facade->displayGeneration(),
                                                               nextSequence++));
                QCoreApplication::processEvents();
                auto* chart = window.findChild<AmpViewChart*>(QStringLiteral("ampViewChart"));
                auto* readout = window.findChild<QLabel*>(QStringLiteral("ampViewCursorReadout"));
                auto* status = window.findChild<QLabel*>(QStringLiteral("ampViewDisplayStatus"));
                QVERIFY(chart && readout && status);
                QVERIFY(chart->geometry().bottom() < readout->geometry().top());
                QVERIFY(readout->geometry().bottom() < status->geometry().top());
                QVERIFY(readout->fontMetrics().height() >= 11);
                const auto checkBoxes = window.findChildren<QCheckBox*>();
                for (int i = 0; i < checkBoxes.size(); ++i) {
                    for (int j = i + 1; j < checkBoxes.size(); ++j) {
                        QVERIFY2(!checkBoxes[i]->geometry().intersects(checkBoxes[j]->geometry()),
                                 "AmpView controls overlap");
                    }
                }
                QVERIFY(captureIfRequested(window,
                    QStringLiteral("ampview-%1-%2")
                        .arg(themed ? QStringLiteral("new") : QStringLiteral("classic"))
                        .arg(width)));
                if (themed) {
                    facade->receiveDisplaySnapshot(denseDisplaySnapshot(
                        facade->displayGeneration(), nextSequence++));
                    QCoreApplication::processEvents();
                    QVERIFY(captureIfRequested(window,
                        QStringLiteral("ampview-synthetic-dense-new-%1").arg(width)));
                }
                if (themed && width == 564) {
                    window.findChild<QCheckBox*>(QStringLiteral("chkAVShowGain"))->setChecked(true);
                    window.findChild<QCheckBox*>(QStringLiteral("chkAVPhaseZoom"))->setChecked(true);
                    chart->setSelectedInput(0.0);
                    QCoreApplication::processEvents();
                    QVERIFY(!chart->readoutAt(0.0).magnitudeAmp.has_value());
                    QVERIFY(readout->text().contains(QStringLiteral("AMP GAIN –")));
                    QVERIFY(captureIfRequested(window, QStringLiteral("ampview-gain-zero")));
                    window.findChild<QCheckBox*>(QStringLiteral("chkAVShowGain"))->setChecked(false);
                    window.findChild<QCheckBox*>(QStringLiteral("chkAVPhaseZoom"))->setChecked(false);
                    chart->setSelectedInput(0.5);
                }
                window.hide();
            }
        }
    }
};

QTEST_MAIN(TstAmpViewWindow)
#include "tst_ampview_window.moc"
