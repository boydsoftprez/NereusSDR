// no-port-check: NereusSDR-original tests for the Qt AmpView presentation.

#include <QtTest/QtTest>

#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QGuiApplication>
#include <QLabel>
#include <QScreen>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/session/PureSignalSessionFacade.h"
#include "gui/AmpViewChart.h"
#include "gui/AmpViewWindow.h"
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
    return widget.grab().save(
        QDir(directory).filePath(stem + QStringLiteral("-scale-") + scale
                                 + QStringLiteral(".png")), "PNG");
}

} // namespace

class TstAmpViewWindow : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }

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

    void legacyAndSeriesPreferencesRoundTrip()
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

        AmpViewWindow restored;
        QVERIFY(restored.findChild<QCheckBox*>(QStringLiteral("chkAVShowGain"))->isChecked());
        QVERIFY(restored.findChild<QCheckBox*>(QStringLiteral("chkAVPhaseZoom"))->isChecked());
        QVERIFY(!restored.findChild<QCheckBox*>(QStringLiteral("chkAVLowRes"))->isChecked());
        QVERIFY(restored.findChild<QCheckBox*>(QStringLiteral("chkStayOnTop"))->isChecked());
        QVERIFY(!restored.findChild<QCheckBox*>(QStringLiteral("chkAVMeasuredPhase"))->isChecked());
        QVERIFY(!restored.findChild<QCheckBox*>(QStringLiteral("chkAVCorrectionMagnitude"))->isChecked());

        auto* chart = restored.findChild<AmpViewChart*>(QStringLiteral("ampViewChart"));
        QVERIFY(chart->showGain());
        QVERIFY(chart->phaseZoom());
        QVERIFY(!chart->lowRes());
        QVERIFY(!chart->seriesVisible(AmpViewChart::Series::MeasuredPhase));
        QVERIFY(!chart->seriesVisible(AmpViewChart::Series::CorrectionMagnitude));
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
        window.close();
        QCoreApplication::processEvents();
        QVERIFY(!facade->ampViewSubscribed());
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
        QCOMPARE(actions.count(), 0);
        QVERIFY(window.findChild<QLabel*>(QStringLiteral("ampViewDisplayStatus"))
                    ->text().contains(QStringLiteral("Live")));
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
};

QTEST_MAIN(TstAmpViewWindow)
#include "tst_ampview_window.moc"
