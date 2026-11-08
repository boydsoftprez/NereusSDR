// =================================================================
// tests/tst_display_pill_wiring.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Display pill plan Task 18 (D2, D5, D6, D25; R-DSP-09, R-DSP-10,
// R-DSP-33): wireDisplayPill on a real strip and a real pan, with hooks
// that stand in for the window. Each of the FFT tab's requests reaches its
// hook once and the tab is then read again from the station, so a request
// the station refused goes back; "Reset this pan" clears the pan's own
// keys and leaves the station's; "Setup defaults", the Clarity switch,
// Re-tune and the band's dB Max and Min reach their hooks; and opening the
// pill reads the station again.
//
// 2026-10-10 - Created. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================
#include <QtTest/QtTest>

#include <QAbstractButton>
#include <QList>
#include <QLoggingCategory>
#include <QPushButton>
#include <QSlider>
#include <QStandardPaths>
#include <QStringList>

#include "core/AppSettings.h"
#include "core/ControlRanges.h"
#include "core/FFTEngine.h"
#include "core/HpsdrModel.h"
#include "core/spectrum/FftEnginePool.h"
#include "fakes/MainWindowTestSettings.h"
#include "gui/DisplayPillWiring.h"
#include "gui/MainWindow.h"
#include "gui/PanModels.h"
#include "gui/PanadapterStack.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/SpectrumWidget.h"
#include "models/PanadapterModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestDisplayPillWiring : public QObject {
    Q_OBJECT

    // A window's part, faked: what was asked, and the station it keeps.
    struct FakeWindow {
        DisplayPillFftState station;
        bool refuse{false};          // the station takes nothing
        int stateReads{0};
        QList<int> baselines;
        QList<int> fps;
        QList<int> windows;
        QList<double> targets;
        QList<ControlRanges::DisplayFftSizingMode> modes;
        QList<double> modeTargets;
        QList<int> decimations;
        QList<bool> clarity;
        int retunes{0};
        QList<int> dbMax;
        QList<int> dbMin;
        QStringList pages;

        FakeWindow()
        {
            station.baselineSize = 4096;
            station.fps = 30;
            station.windowType = 1;
            station.hzPerBinTarget = 0.0;
            station.decimation = 1;
            station.runningSize = 4096;
            station.sampleRateHz = 192000.0;
        }

        DisplayPillHooks hooks()
        {
            DisplayPillHooks h;
            h.applyFftBaseline = [this](int size) {
                baselines.append(size);
                if (!refuse) { station.baselineSize = size; station.runningSize = size; }
            };
            h.applyFps = [this](int value) {
                fps.append(value);
                if (!refuse) { station.fps = value; }
            };
            h.applyWindowType = [this](int value) {
                windows.append(value);
                if (!refuse) { station.windowType = value; }
            };
            h.applyHzPerBinTarget = [this](double hz) {
                targets.append(hz);
                if (!refuse) {
                    station.hzPerBinTarget = hz;
                    if (hz > 0.0) { station.lastHzPerBinTarget = hz; }
                }
            };
            // As the window does it: Hz/bin takes the target the station
            // remembers, and the one handed over only when it remembers none.
            h.applyFftSizingMode = [this](ControlRanges::DisplayFftSizingMode mode,
                                          double hzIfNoneRemembered) {
                modes.append(mode);
                modeTargets.append(hzIfNoneRemembered);
                if (refuse) { return; }
                station.sizingMode = mode;
                if (mode == ControlRanges::DisplayFftSizingMode::HzPerBin) {
                    if (station.lastHzPerBinTarget <= 0.0) {
                        station.lastHzPerBinTarget = hzIfNoneRemembered;
                    }
                    station.hzPerBinTarget = station.lastHzPerBinTarget;
                } else {
                    station.hzPerBinTarget = 0.0;
                }
            };
            h.applyDecimation = [this](int value) {
                decimations.append(value);
                if (!refuse) { station.decimation = value; }
            };
            h.fftState = [this]() {
                ++stateReads;
                return station;
            };
            h.setClarityEnabled = [this](bool on) { clarity.append(on); };
            h.retuneClarity = [this]() { ++retunes; };
            h.setPerBandDbMax = [this](int db) { dbMax.append(db); };
            h.setPerBandDbMin = [this](int db) { dbMin.append(db); };
            h.openSetupPage = [this](const QString& page) { pages.append(page); };
            return h;
        }
    };

    // The strip on a real pan, as it is in the window.
    struct Harness {
        SpectrumWidget pan;
        SpectrumOverlayPanel* panel{nullptr};
        FakeWindow window;
        Harness()
        {
            pan.resize(900, 600);
            panel = new SpectrumOverlayPanel(&pan);
            wireDisplayPill(panel, &pan, window.hooks());
        }
        QSlider* slider(const char* name)
        {
            return pan.findChild<QSlider*>(QString::fromLatin1(name));
        }
    };

    QString m_isolatedSettingsPath;

private slots:
    void initTestCase()
    {
        QVERIFY(QStandardPaths::isTestModeEnabled());
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        const QString profile =
            QStringLiteral("display-pill-wiring-%1").arg(QCoreApplication::applicationPid());
        AppSettings::setProfileOverride(profile);
        m_isolatedSettingsPath = AppSettings::resolveSettingsPath(profile);
        QCOMPARE(AppSettings::instance().filePath(), m_isolatedSettingsPath);
    }

    void init() { AppSettings::instance().clear(); }

    void cleanup()
    {
        if (AppSettings::instance().filePath() == m_isolatedSettingsPath) {
            AppSettings::instance().clear();
        }
    }

    // Wiring shows the station at once and makes the pan the pill's.
    void wiringShowsTheStationAndTakesThePan()
    {
        Harness h;
        QCOMPARE(h.panel->displayTarget(), &h.pan);
        QCOMPARE(h.window.stateReads, 1);
        QCOMPARE(h.panel->fftState().baselineSize, 4096);
        QCOMPARE(h.panel->fftState().fps, 30);
        QCOMPARE(h.panel->fftState().sampleRateHz, 192000.0);
        QSlider* fps = h.slider("pillDisplaySpectrumFps");
        QVERIFY(fps);
        QCOMPARE(fps->value(), 30);
        // Showing the station asks for nothing.
        QVERIFY(h.window.fps.isEmpty());
        QVERIFY(h.window.baselines.isEmpty());
    }

    // Each FFT request calls its own hook once, and the tab is read again.
    void eachFftRequestReachesItsHookOnceAndTheTabIsReadAgain()
    {
        Harness h;
        int reads = h.window.stateReads;

        emit h.panel->fftBaselineRequested(16384);
        QCOMPARE(h.window.baselines, QList<int>{16384});
        QCOMPARE(h.window.stateReads, ++reads);
        QCOMPARE(h.panel->fftState().baselineSize, 16384);
        QCOMPARE(h.panel->fftState().runningSize, 16384);

        emit h.panel->fftFpsRequested(45);
        QCOMPARE(h.window.fps, QList<int>{45});
        QCOMPARE(h.window.stateReads, ++reads);
        QCOMPARE(h.panel->fftState().fps, 45);

        emit h.panel->fftWindowRequested(3);
        QCOMPARE(h.window.windows, QList<int>{3});
        QCOMPARE(h.window.stateReads, ++reads);
        QCOMPARE(h.panel->fftState().windowType, 3);

        emit h.panel->hzPerBinTargetRequested(1.5);
        QCOMPARE(h.window.targets, QList<double>{1.5});
        QCOMPARE(h.window.stateReads, ++reads);
        QCOMPARE(h.panel->fftState().hzPerBinTarget, 1.5);

        emit h.panel->decimationRequested(4);
        QCOMPARE(h.window.decimations, QList<int>{4});
        QCOMPARE(h.window.stateReads, ++reads);
        QCOMPARE(h.panel->fftState().decimation, 4);

        // No request reached a hook that was not its own.
        QCOMPARE(h.window.baselines.size(), 1);
        QCOMPARE(h.window.fps.size(), 1);
        QCOMPARE(h.window.windows.size(), 1);
        QCOMPARE(h.window.targets.size(), 1);
        QCOMPARE(h.window.decimations.size(), 1);
    }

    // The operator's own move of a row asks once: reading the station back
    // into the row does not ask again.
    void movingARowAsksOnceWithNoEcho()
    {
        Harness h;
        QSlider* fps = h.slider("pillDisplaySpectrumFps");
        QSlider* decimation = h.slider("pillDisplayDecimation");
        QVERIFY(fps);
        QVERIFY(decimation);

        fps->setValue(50);
        QCOMPARE(h.window.fps, QList<int>{50});
        QCOMPARE(fps->value(), 50);

        decimation->setValue(4);
        QCOMPARE(h.window.decimations, QList<int>{4});
        QCOMPARE(decimation->value(), 4);
        QCOMPARE(h.window.fps.size(), 1);
    }

    // A request the station did not take: the row goes back to the
    // station's value, and nothing more is asked.
    void aRefusedRequestGoesBackToTheStation()
    {
        Harness h;
        h.window.refuse = true;
        QSlider* fps = h.slider("pillDisplaySpectrumFps");
        QVERIFY(fps);

        fps->setValue(50);
        QCOMPARE(h.window.fps, QList<int>{50});
        QCOMPARE(fps->value(), 30);
        QCOMPARE(h.panel->fftState().fps, 30);
    }

    // FFT sizing mode (design D38): a Sizing button asks its hook once, with
    // the bin width running for a station that remembers no target, and the
    // tab is read again. The same in a remote window. A station that is not
    // there grays the buttons with its reason and nothing is asked.
    void theSizingButtonsReachTheirHook_data()
    {
        QTest::addColumn<bool>("remote");
        QTest::newRow("local window") << false;
        QTest::newRow("remote window") << true;
    }

    void theSizingButtonsReachTheirHook()
    {
        using Mode = ControlRanges::DisplayFftSizingMode;
        QFETCH(bool, remote);
        Harness h;
        h.panel->setRemoteWindow(remote, false);
        h.panel->setStationRowsAvailable(true, QString());
        auto* autoBtn = h.pan.findChild<QPushButton*>(
            QStringLiteral("pillDisplayFftSizingModeAuto"));
        auto* hzBtn = h.pan.findChild<QPushButton*>(
            QStringLiteral("pillDisplayFftSizingModeHzPerBin"));
        auto* fixedBtn = h.pan.findChild<QPushButton*>(
            QStringLiteral("pillDisplayFftSizingModeFixed"));
        QVERIFY(autoBtn && hzBtn && fixedBtn);
        QSlider* size = h.slider("pillDisplayFftSize");
        QSlider* target = h.slider("pillDisplayHzPerBinTarget");
        QVERIFY(size && target);
        QVERIFY(autoBtn->isChecked());
        QVERIFY(!size->isEnabled());
        QVERIFY(!target->isEnabled());
        int reads = h.window.stateReads;

        // 192 kHz over 4096 is 46.875 Hz: 47.0 on the 0.5 Hz step.
        hzBtn->click();
        QCOMPARE(h.window.modes, QList<Mode>{Mode::HzPerBin});
        QCOMPARE(h.window.modeTargets, QList<double>{47.0});
        QCOMPARE(h.window.stateReads, ++reads);
        QCOMPARE(h.panel->fftState().sizingMode, Mode::HzPerBin);
        QCOMPARE(h.panel->fftState().hzPerBinTarget, 47.0);
        QVERIFY(hzBtn->isChecked() && !autoBtn->isChecked() && !fixedBtn->isChecked());
        QVERIFY(target->isEnabled());
        QVERIFY(!size->isEnabled());
        // Choosing the mode moved no slider's request.
        QVERIFY(h.window.targets.isEmpty());
        QVERIFY(h.window.baselines.isEmpty());

        // A target chosen, then Fixed and back: the station's 12.5 returns,
        // whatever the pill hands over.
        target->setValue(25);
        QCOMPARE(h.window.targets, QList<double>{12.5});
        fixedBtn->click();
        QCOMPARE(h.window.modes.constLast(), Mode::Fixed);
        QCOMPARE(h.panel->fftState().sizingMode, Mode::Fixed);
        QVERIFY(size->isEnabled());
        QVERIFY(!target->isEnabled());
        QCOMPARE(target->value(), 25);
        hzBtn->click();
        QCOMPARE(h.window.modes.size(), 3);
        QCOMPARE(h.panel->fftState().hzPerBinTarget, 12.5);
        QCOMPARE(target->value(), 25);
        QCOMPARE(h.window.targets.size(), 1);

        // A request the station did not take: the buttons go back.
        h.window.refuse = true;
        autoBtn->click();
        QCOMPARE(h.window.modes.constLast(), Mode::Auto);
        QVERIFY(hzBtn->isChecked() && !autoBtn->isChecked());
        QCOMPARE(h.panel->fftState().sizingMode, Mode::HzPerBin);
        h.window.refuse = false;

        // The station is not there: grayed with its reason, never hidden,
        // and a click asks nothing.
        const QString reason = QStringLiteral("Connect to the Core to change these.");
        h.panel->setStationRowsAvailable(false, reason);
        const int asked = h.window.modes.size();
        for (QPushButton* button : {autoBtn, hzBtn, fixedBtn}) {
            QVERIFY(!button->isEnabled());
            QVERIFY(!button->isHidden());
            QCOMPARE(button->toolTip(), reason);
            button->click();
            QTest::mouseClick(button, Qt::LeftButton);
        }
        QCOMPARE(h.window.modes.size(), asked);
        QCOMPARE(size->toolTip(), reason);
        QCOMPARE(target->toolTip(), reason);
        QVERIFY(hzBtn->isChecked());
        h.panel->setStationRowsAvailable(true, QString());
        QVERIFY(autoBtn->isEnabled());
        QVERIFY(target->isEnabled());
        QVERIFY(autoBtn->toolTip() != reason);
    }

    // "Reset this pan" clears the pan's own appearance and leaves every
    // station key as it was.
    void resetClearsThePanAndLeavesTheStationKeys()
    {
        Harness h;
        auto& s = AppSettings::instance();
        const QStringList stationKeys{
            QLatin1String(ControlRanges::kDisplayFftSizeKey),
            QLatin1String(ControlRanges::kDisplayFftWindowKey),
            QLatin1String(ControlRanges::kDisplayHzPerBinTargetKey),
            QLatin1String(ControlRanges::kDisplaySpectrumFpsKey),
            QStringLiteral("DisplayGridMax_40m"),
            QStringLiteral("DisplayGridMin_40m")};
        const QStringList stationValues{
            QStringLiteral("32768"), QStringLiteral("3"), QStringLiteral("1.5"),
            QStringLiteral("45"), QStringLiteral("-30"), QStringLiteral("-120")};
        for (int i = 0; i < stationKeys.size(); ++i) {
            s.setValue(stationKeys.at(i), stationValues.at(i));
        }

        const bool gridDefault = h.pan.gridEnabled();
        h.pan.setGridEnabled(!gridDefault);
        QCOMPARE(h.pan.gridEnabled(), !gridDefault);
        QSignalSpy reset(&h.pan, &SpectrumWidget::panDisplayReset);

        emit h.panel->resetPanRequested();

        QCOMPARE(reset.count(), 1);
        QCOMPARE(h.pan.gridEnabled(), gridDefault);
        for (int i = 0; i < stationKeys.size(); ++i) {
            QVERIFY2(s.contains(stationKeys.at(i)), qPrintable(stationKeys.at(i)));
            QCOMPARE(s.value(stationKeys.at(i)).toString(), stationValues.at(i));
        }
        // A reset asks the station for nothing.
        QVERIFY(h.window.baselines.isEmpty());
        QVERIFY(h.window.fps.isEmpty());
        QVERIFY(h.window.dbMax.isEmpty());
        QVERIFY(h.window.dbMin.isEmpty());
    }

    void setupDefaultsOpensThePageNamed()
    {
        Harness h;
        emit h.panel->openSetupRequested(QStringLiteral("Waterfall Defaults"));
        QCOMPARE(h.window.pages, QStringList{QStringLiteral("Waterfall Defaults")});
    }

    void clarityRequestsReachTheirHooks()
    {
        Harness h;
        emit h.panel->clarityEnabledRequested(false);
        QCOMPARE(h.window.clarity, QList<bool>{false});
        emit h.panel->clarityEnabledRequested(true);
        QCOMPARE(h.window.clarity, (QList<bool>{false, true}));
        QCOMPARE(h.window.retunes, 0);
        emit h.panel->clarityRetuneRequested();
        QCOMPARE(h.window.retunes, 1);
    }

    void bandGridRequestsReachTheirHooks()
    {
        Harness h;
        emit h.panel->perBandDbMaxRequested(-30);
        QCOMPARE(h.window.dbMax, QList<int>{-30});
        QVERIFY(h.window.dbMin.isEmpty());
        emit h.panel->perBandDbMinRequested(-120);
        QCOMPARE(h.window.dbMin, QList<int>{-120});
        QCOMPARE(h.window.dbMax.size(), 1);
    }

    // The station changed while the pill was shut (Setup, another window):
    // opening it shows the value in force.
    void openingThePillReadsTheStationAgain()
    {
        Harness h;
        h.pan.show();
        QCOMPARE(h.panel->fftState().fps, 30);
        h.window.station.fps = 20;
        h.window.station.decimation = 8;
        const int reads = h.window.stateReads;

        h.panel->openDisplayPill();

        QVERIFY(h.panel->displayPillVisible());
        QVERIFY(h.window.stateReads > reads);
        QCOMPARE(h.panel->fftState().fps, 20);
        QCOMPARE(h.panel->fftState().decimation, 8);
        QSlider* fps = h.slider("pillDisplaySpectrumFps");
        QVERIFY(fps);
        QCOMPARE(fps->value(), 20);
        QVERIFY(h.window.fps.isEmpty());
    }

    // Hooks that are not given are skipped, and a missing strip or pan
    // wires nothing.
    void emptyHooksAndMissingPartsAreSafe()
    {
        SpectrumWidget pan;
        pan.resize(900, 600);
        auto* panel = new SpectrumOverlayPanel(&pan);
        wireDisplayPill(nullptr, &pan, DisplayPillHooks{});
        wireDisplayPill(panel, nullptr, DisplayPillHooks{});
        wireDisplayPill(panel, &pan, DisplayPillHooks{});

        emit panel->fftBaselineRequested(8192);
        emit panel->fftFpsRequested(20);
        emit panel->fftWindowRequested(2);
        emit panel->hzPerBinTargetRequested(2.0);
        emit panel->fftSizingModeRequested(ControlRanges::DisplayFftSizingMode::Fixed, 0.0);
        emit panel->decimationRequested(2);
        emit panel->openSetupRequested(QStringLiteral("3D View"));
        emit panel->clarityEnabledRequested(true);
        emit panel->clarityRetuneRequested();
        emit panel->perBandDbMaxRequested(-20);
        emit panel->perBandDbMinRequested(-130);

        // "Reset this pan" needs no hook.
        const bool gridDefault = pan.gridEnabled();
        pan.setGridEnabled(!gridDefault);
        emit panel->resetPanRequested();
        QCOMPARE(pan.gridEnabled(), gridDefault);
    }

    // Persistence audit 2026-11: Decimation has a station key beside the other
    // FFT knobs. Set from a pill, it comes back after a restart on the pool
    // and every engine; what does not fit the range is the default.
    void decimationComesBackAfterARestart()
    {
        Test::markAudioFirstRunDone();
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("PanLayoutId"), QStringLiteral("1"));
        const QString key = QLatin1String(ControlRanges::kDisplayDecimationKey);
        {
            MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
            window.show();
            PanadapterStack* stack = window.findChild<PanadapterStack*>();
            QVERIFY(stack);
            auto* pill = stack->spectrum(QStringLiteral("pan-0"))->findChild<SpectrumOverlayPanel*>();
            QVERIFY(pill);
            emit pill->decimationRequested(4);
            QCOMPARE(window.fftEnginePoolForTest()->decimation(), 4);
            QCOMPARE(s.value(key).toString(), QStringLiteral("4"));
            // Refused: the key keeps what the engines hold.
            emit pill->decimationRequested(99);
            QCOMPARE(s.value(key).toString(), QStringLiteral("4"));
        }
        {
            MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
            FftEnginePool* pool = window.fftEnginePoolForTest();
            QVERIFY(pool);
            QCOMPARE(pool->decimation(), 4);
            QVERIFY(!pool->streams().isEmpty());
            for (int stream : pool->streams()) {
                QCOMPARE(pool->engineForStream(stream)->decimation(), 4);
            }
            PanadapterStack* stack = window.findChild<PanadapterStack*>();
            auto* pill = stack->spectrum(QStringLiteral("pan-0"))->findChild<SpectrumOverlayPanel*>();
            QVERIFY(pill);
            QCOMPARE(pill->fftState().decimation, 4);

            // Each device has its own: another window's value on the Core
            // does not move this one.
            s.setValue(key, QStringLiteral("6"));
            emit window.radioModel()->stationSettingChanged(key);
            QCOMPARE(pool->decimation(), 4);
            s.setValue(key, QStringLiteral("4"));
        }
        for (const QString& bad : {QStringLiteral("99"), QStringLiteral("0"), QStringLiteral("many")}) {
            s.setValue(key, bad);
            QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Stored.*DisplayDecimation")));
            MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
            QCOMPARE(window.fftEnginePoolForTest()->decimation(), ControlRanges::kDisplayDecimationDefault);
            QCOMPARE(s.value(key).toString(), QString::number(ControlRanges::kDisplayDecimationDefault));
        }
    }

    // The window's own hooks, on a real local window with two pans, each
    // showing its own slice (no radio, no Core). A request from one pan's
    // pill reaches every engine and saves the station key, and both pans'
    // pills then show it; the band's dB Max goes to the band store; the
    // Clarity switch is the window's and both pills are told.
    void inTheWindowAPillsRequestReachesEveryEngineAndEveryPill()
    {
        Test::markAudioFirstRunDone();
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("PanLayoutId"), QStringLiteral("1"));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        window.resize(1280, 800);
        window.show();
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(model->addSlice(QStringLiteral("pan-0")) >= 0);
        QVERIFY(QMetaObject::invokeMethod(&window, "applyPanLayout",
                                          Q_ARG(QString, QStringLiteral("2v"))));
        PanadapterStack* stack = window.findChild<PanadapterStack*>();
        QVERIFY(stack);
        SpectrumWidget* first = stack->spectrum(QStringLiteral("pan-0"));
        SpectrumWidget* second = stack->spectrum(QStringLiteral("pan-1"));
        QVERIFY(first);
        QVERIFY(second);
        QTRY_VERIFY(second->vfoWidget(1));
        auto* firstPill = first->findChild<SpectrumOverlayPanel*>();
        auto* secondPill = second->findChild<SpectrumOverlayPanel*>();
        QVERIFY(firstPill);
        QVERIFY(secondPill);

        FftEnginePool* pool = window.fftEnginePoolForTest();
        QVERIFY(pool);
        const QList<int> streams = pool->streams();
        QVERIFY(streams.size() >= 2);
        const auto everyEngine = [pool, streams](const std::function<bool(NereusSDR::FFTEngine*)>& holds) {
            for (int stream : streams) {
                if (!holds(pool->engineForStream(stream))) { return false; }
            }
            return true;
        };
        const auto saved = [&s](const char* key) {
            return s.value(QLatin1String(key)).toString();
        };

        // As wired: each pill edits its own pan and shows the station, with
        // the size and rate of the engine its pan shows.
        QCOMPARE(firstPill->displayTarget(), first);
        QCOMPARE(secondPill->displayTarget(), second);
        QVERIFY(!secondPill->remoteWindow());
        SliceModel* secondSlice = model->sliceById(1);
        QVERIFY(secondSlice);
        QVERIFY(streams.contains(secondSlice->streamIndex()));
        NereusSDR::FFTEngine* secondEngine = pool->engineForStream(secondSlice->streamIndex());
        QCOMPARE(secondPill->fftState().baselineSize, secondEngine->fftSizeBaseline());
        QCOMPARE(secondPill->fftState().runningSize, secondEngine->fftSize());
        QCOMPARE(secondPill->fftState().sampleRateHz, secondEngine->sampleRate());
        QCOMPARE(secondPill->fftState().fps, ControlRanges::kDisplaySpectrumFpsDefault);

        // FPS, moved on the second pan's pill.
        QSlider* fps = second->findChild<QSlider*>(QStringLiteral("pillDisplaySpectrumFps"));
        QVERIFY(fps);
        QVERIFY(fps->isEnabled());
        fps->setValue(45);
        QCOMPARE(pool->config().fps, 45);
        QCOMPARE(saved(ControlRanges::kDisplaySpectrumFpsKey), QStringLiteral("45"));
        QCOMPARE(firstPill->fftState().fps, 45);
        QCOMPARE(secondPill->fftState().fps, 45);
        QCOMPARE(first->findChild<QSlider*>(QStringLiteral("pillDisplaySpectrumFps"))->value(),
                 45);

        emit secondPill->fftWindowRequested(3);
        QCOMPARE(pool->config().windowType, 3);
        QVERIFY(everyEngine([](NereusSDR::FFTEngine* e) {
            return static_cast<int>(e->windowFunction()) == 3;
        }));
        QCOMPARE(saved(ControlRanges::kDisplayFftWindowKey), QStringLiteral("3"));
        QCOMPARE(firstPill->fftState().windowType, 3);

        emit secondPill->hzPerBinTargetRequested(2.0);
        QVERIFY(everyEngine([](NereusSDR::FFTEngine* e) { return e->hzPerBinTarget() == 2.0; }));
        QCOMPARE(saved(ControlRanges::kDisplayHzPerBinTargetKey).toDouble(), 2.0);
        QCOMPARE(firstPill->fftState().hzPerBinTarget, 2.0);
        QCOMPARE(secondPill->fftState().hzPerBinTarget, 2.0);
        emit secondPill->hzPerBinTargetRequested(0.0);
        QVERIFY(everyEngine([](NereusSDR::FFTEngine* e) { return e->hzPerBinTarget() == 0.0; }));
        QCOMPARE(firstPill->fftState().hzPerBinTarget, 0.0);

        emit firstPill->decimationRequested(4);
        QCOMPARE(pool->decimation(), 4);
        QVERIFY(everyEngine([](NereusSDR::FFTEngine* e) { return e->decimation() == 4; }));
        QCOMPARE(secondPill->fftState().decimation, 4);
        // Out of range: nothing changes and the pill stays on the station.
        emit firstPill->decimationRequested(99);
        QCOMPARE(pool->decimation(), 4);
        QCOMPARE(firstPill->fftState().decimation, 4);
        emit firstPill->decimationRequested(1);
        QCOMPARE(pool->decimation(), 1);

        // The FFT Size slider's size. In Auto (design D38) it is saved and
        // shown and no engine takes it.
        using ControlRanges::DisplayFftSizingMode;
        QCOMPARE(firstPill->fftState().sizingMode, DisplayFftSizingMode::Auto);
        emit secondPill->fftBaselineRequested(16384);
        QVERIFY(everyEngine([](NereusSDR::FFTEngine* e) { return e->fftSizeBaseline() == 4096; }));
        QCOMPARE(saved(ControlRanges::kDisplayFftSizeKey), QStringLiteral("16384"));
        QCOMPARE(firstPill->fftState().baselineSize, 16384);
        QCOMPARE(secondPill->fftState().baselineSize, 16384);
        // In Fixed every engine runs it, and both pills show the mode.
        window.applyDisplayFftSizingMode(DisplayFftSizingMode::Fixed);
        QCOMPARE(firstPill->fftState().sizingMode, DisplayFftSizingMode::Fixed);
        QCOMPARE(secondPill->fftState().sizingMode, DisplayFftSizingMode::Fixed);
        QVERIFY(everyEngine([](NereusSDR::FFTEngine* e) {
            return e->fftSizeBaseline() == 16384 && e->fftSize() == 16384;
        }));
        emit secondPill->fftBaselineRequested(8192);
        QVERIFY(everyEngine([](NereusSDR::FFTEngine* e) {
            return e->fftSizeBaseline() == 8192 && e->fftSize() == 8192;
        }));
        QCOMPARE(saved(ControlRanges::kDisplayFftSizeKey), QStringLiteral("8192"));
        QCOMPARE(firstPill->fftState().baselineSize, 8192);
        QCOMPARE(secondPill->fftState().baselineSize, 8192);
        QCOMPARE(secondPill->fftState().runningSize, secondEngine->fftSize());
        QCOMPARE(secondPill->fftState().runningSize, 8192);
        // The last Hz/bin target stays for a grayed control to show.
        QCOMPARE(firstPill->fftState().hzPerBinTarget, 0.0);
        QCOMPARE(firstPill->fftState().lastHzPerBinTarget, 2.0);

        // dB Max is the band's: the store takes it and the pill shows it.
        PanModels* pans = window.panModels();
        QVERIFY(pans);
        PanadapterModel* secondModel = pans->model(QStringLiteral("pan-1"));
        QVERIFY(secondModel);
        const Band band = secondModel->band();
        const int oldMin = secondModel->perBandGrid(band).dbMin;
        QVERIFY(oldMin < -30);
        emit secondPill->perBandDbMaxRequested(-30);
        QCOMPARE(secondModel->perBandGrid(band).dbMax, -30);
        QCOMPARE(secondModel->perBandGrid(band).dbMin, oldMin);
        QSlider* dbMax = second->findChild<QSlider*>(QStringLiteral("pillDisplayGridMax"));
        QVERIFY(dbMax);
        QCOMPARE(dbMax->value(), -30);
        QCOMPARE(qRound(second->refLevel()), -30);
        // An end that would cross the other is not taken; the pill goes back.
        emit secondPill->perBandDbMaxRequested(oldMin - 5);
        QCOMPARE(secondModel->perBandGrid(band).dbMax, -30);
        QCOMPARE(dbMax->value(), -30);
        emit secondPill->perBandDbMinRequested(oldMin + 4);
        QCOMPARE(secondModel->perBandGrid(band).dbMin, oldMin + 4);

        // The approved rebuild replaces the global pill switch with the
        // captured pan's requested choice. The model remains the master.
        QVERIFY(model->clarityEnabled());
        QVERIFY(first->clarityEnabled());
        QVERIFY(second->clarityEnabled());
        emit secondPill->clarityEnabledRequested(false);
        QVERIFY(model->clarityEnabled());
        QVERIFY(first->clarityEnabled());
        QVERIFY(!second->clarityEnabled());
        emit firstPill->clarityEnabledRequested(false);
        QVERIFY(model->clarityEnabled());
        QVERIFY(!first->clarityEnabled());
        QVERIFY(!second->clarityEnabled());
        model->setClarityEnabled(false);
        model->setClarityEnabled(true);
        QVERIFY(first->clarityEnabled());
        QVERIFY(second->clarityEnabled());

    }

    // FFT sizing mode (design D38, D39) in a real local window with two
    // pans. At 192 kHz with 8192 running, choosing Hz/bin on one pan's pill
    // changes no stream's size: the target starts from the bin width that
    // runs. A target chosen is the station's, and it is the one Hz/bin
    // comes back to. Every engine and every pill follows each choice.
    void inTheWindowChoosingHzPerBinKeepsTheSizeThatRuns()
    {
        using Mode = ControlRanges::DisplayFftSizingMode;
        Test::markAudioFirstRunDone();
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("PanLayoutId"), QStringLiteral("1"));
        s.setValue(QLatin1String(ControlRanges::kDisplayFftSizingModeKey),
                   QLatin1String(ControlRanges::kDisplayFftSizingModeFixedToken));
        s.setValue(QLatin1String(ControlRanges::kDisplayFftSizeKey), QStringLiteral("8192"));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        window.resize(1280, 800);
        window.show();
        RadioModel* model = window.radioModel();
        model->setBoardForTest(HPSDRHW::Saturn);
        model->configureStreamPool(5, 5, 192000);
        model->setConnectionStateForTest(ConnectionState::Connected);
        QVERIFY(model->addSlice(QStringLiteral("pan-0")) >= 0);
        QVERIFY(QMetaObject::invokeMethod(&window, "applyPanLayout",
                                          Q_ARG(QString, QStringLiteral("2v"))));
        PanadapterStack* stack = window.findChild<PanadapterStack*>();
        QVERIFY(stack);
        SpectrumWidget* first = stack->spectrum(QStringLiteral("pan-0"));
        SpectrumWidget* second = stack->spectrum(QStringLiteral("pan-1"));
        QVERIFY(first);
        QVERIFY(second);
        QTRY_VERIFY(second->vfoWidget(1));
        for (SpectrumWidget* sw : {first, second}) {
            // Each pan carries its stream's rate, as it does once I/Q
            // arrives; the Hz/bin plan is made from it.
            sw->setSampleRate(192000.0);
            sw->setFrequencyRange(sw->centerFrequency(), sw->sampleRate());
            sw->requestAutoZoomReplan();
        }
        auto* firstPill = first->findChild<SpectrumOverlayPanel*>();
        auto* secondPill = second->findChild<SpectrumOverlayPanel*>();
        QVERIFY(firstPill);
        QVERIFY(secondPill);
        FftEnginePool* pool = window.fftEnginePoolForTest();
        QVERIFY(pool);
        const QList<int> streams = pool->streams();
        QVERIFY(streams.size() >= 2);
        const auto everyEngineRuns = [pool, streams](int size) {
            for (int stream : streams) {
                if (pool->engineForStream(stream)->fftSize() != size) { return false; }
            }
            return true;
        };
        const auto button = [](SpectrumWidget* pan, const char* name) {
            return pan->findChild<QPushButton*>(QString::fromLatin1(name));
        };
        const auto slider = [](SpectrumWidget* pan, const char* name) {
            return pan->findChild<QSlider*>(QString::fromLatin1(name));
        };
        const auto savedMode = [&s]() {
            return s.value(QLatin1String(ControlRanges::kDisplayFftSizingModeKey)).toString();
        };
        emit firstPill->fftBaselineRequested(8192);
        QVERIFY(everyEngineRuns(8192));
        for (SpectrumWidget* pan : {first, second}) {
            QVERIFY(button(pan, "pillDisplayFftSizingModeFixed")->isChecked());
            QVERIFY(slider(pan, "pillDisplayFftSize")->isEnabled());
            QVERIFY(!slider(pan, "pillDisplayHzPerBinTarget")->isEnabled());
        }

        // Hz/bin, chosen on the second pan: 192 kHz over 8192 is 23.44 Hz,
        // 23.5 on the 0.5 Hz step, which plans the 8192 that runs.
        button(second, "pillDisplayFftSizingModeHzPerBin")->click();
        QCOMPARE(savedMode(), QStringLiteral("HzPerBin"));
        QCOMPARE(pool->config().sizingMode, Mode::HzPerBin);
        QVERIFY(everyEngineRuns(8192));
        for (SpectrumOverlayPanel* pill : {firstPill, secondPill}) {
            QCOMPARE(pill->fftState().sizingMode, Mode::HzPerBin);
            QCOMPARE(pill->fftState().hzPerBinTarget, 23.5);
            QCOMPARE(pill->fftState().runningSize, 8192);
        }
        for (SpectrumWidget* pan : {first, second}) {
            QVERIFY(button(pan, "pillDisplayFftSizingModeHzPerBin")->isChecked());
            QVERIFY(!button(pan, "pillDisplayFftSizingModeFixed")->isChecked());
            QVERIFY(!slider(pan, "pillDisplayFftSize")->isEnabled());
            QVERIFY(slider(pan, "pillDisplayHzPerBinTarget")->isEnabled());
            QCOMPARE(slider(pan, "pillDisplayHzPerBinTarget")->value(), 47);
            // Grayed, FFT Size is on the size that runs.
            QCOMPARE(slider(pan, "pillDisplayFftSize")->value(),
                     ControlRanges::displayFftSizePosition(8192));
        }

        // 12.5 Hz, chosen on the first pan: 192 kHz over 12.5 is 15360
        // points, so every stream runs 16384.
        slider(first, "pillDisplayHzPerBinTarget")->setValue(25);
        QVERIFY(everyEngineRuns(16384));
        QCOMPARE(secondPill->fftState().hzPerBinTarget, 12.5);
        QCOMPARE(slider(second, "pillDisplayHzPerBinTarget")->value(), 25);
        QCOMPARE(slider(second, "pillDisplayFftSize")->value(),
                 ControlRanges::displayFftSizePosition(16384));

        // Fixed: the stored 8192 runs again and the slider is on it; Hz/bin
        // is grayed on the 12.5 remembered.
        button(first, "pillDisplayFftSizingModeFixed")->click();
        QCOMPARE(savedMode(), QStringLiteral("Fixed"));
        QVERIFY(everyEngineRuns(8192));
        for (SpectrumWidget* pan : {first, second}) {
            QVERIFY(slider(pan, "pillDisplayFftSize")->isEnabled());
            QCOMPARE(slider(pan, "pillDisplayFftSize")->value(),
                     ControlRanges::displayFftSizePosition(8192));
            QVERIFY(!slider(pan, "pillDisplayHzPerBinTarget")->isEnabled());
            QCOMPARE(slider(pan, "pillDisplayHzPerBinTarget")->value(), 25);
        }

        // Hz/bin again, from the other pan: the target is the 12.5 chosen,
        // not the 23.5 the pill hands over for the 8192 running.
        button(second, "pillDisplayFftSizingModeHzPerBin")->click();
        QVERIFY(everyEngineRuns(16384));
        for (SpectrumOverlayPanel* pill : {firstPill, secondPill}) {
            QCOMPARE(pill->fftState().sizingMode, Mode::HzPerBin);
            QCOMPARE(pill->fftState().hzPerBinTarget, 12.5);
        }

        // Auto: both pills, both sliders grayed.
        button(first, "pillDisplayFftSizingModeAuto")->click();
        QCOMPARE(savedMode(), QStringLiteral("Auto"));
        for (SpectrumWidget* pan : {first, second}) {
            QVERIFY(button(pan, "pillDisplayFftSizingModeAuto")->isChecked());
            QVERIFY(!slider(pan, "pillDisplayFftSize")->isEnabled());
            QVERIFY(!slider(pan, "pillDisplayHzPerBinTarget")->isEnabled());
            QCOMPARE(slider(pan, "pillDisplayHzPerBinTarget")->value(), 25);
        }
        QCOMPARE(firstPill->fftState().runningSize,
                 pool->engineForStream(streams.first())->fftSize());
    }
};

QTEST_MAIN(TestDisplayPillWiring)
#include "tst_display_pill_wiring.moc"
