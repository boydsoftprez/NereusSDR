#include "MainWindow.h"
#include "ConnectionPanel.h"
#include "SupportDialog.h"
#include "SpectrumWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "widgets/VfoWidget.h"
#include "core/RxChannel.h"
#include "core/ReceiverManager.h"
#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "core/WdspEngine.h"
#include "core/FFTEngine.h"
#include "core/LogCategories.h"
#include "containers/ContainerManager.h"
#include "containers/ContainerWidget.h"
#include "meters/MeterWidget.h"
#include "meters/MeterItem.h"
#include "meters/ItemGroup.h"
#include "meters/MeterPoller.h"
#include "applets/AppletPanelWidget.h"
#include "applets/PhoneCwApplet.h"

#include <cmath>

#include <QApplication>
#include <QSlider>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QStatusBar>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QDateTime>
#include <QPainter>
#include <QPixmap>
#include <QProgressDialog>
#include <QTimer>
#include <QThread>

#include <cstdlib>

namespace NereusSDR {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_radioModel(new RadioModel(this))
{
    buildUI();
    buildMenuBar();
    buildStatusBar();
    applyDarkTheme();

    // Wire connection state changes to status bar
    connect(m_radioModel, &RadioModel::connectionStateChanged,
            this, &MainWindow::onConnectionStateChanged);

    // WDSP wisdom progress dialog — shown as a modal window during first-run
    // wisdom generation. Pattern from AetherSDR MainWindow::enableNr2WithWisdom().
    connect(m_radioModel->wdspEngine(), &WdspEngine::wisdomProgress,
            this, [this](int percent, const QString& status) {
        // Create dialog on first progress signal
        if (!m_wisdomDialog && percent < 100) {
            m_wisdomDialog = new QProgressDialog(this);
            m_wisdomDialog->setWindowTitle(QStringLiteral("NereusSDR — FFTW Wisdom"));
            m_wisdomDialog->setLabelText(
                QStringLiteral("Optimizing FFT plans for DSP engine...\n\n"
                               "This only happens on first run."));
            m_wisdomDialog->setRange(0, 100);
            m_wisdomDialog->setValue(0);
            m_wisdomDialog->setCancelButton(nullptr);
            m_wisdomDialog->setAutoClose(false);
            m_wisdomDialog->setMinimumWidth(500);
            m_wisdomDialog->setMinimumDuration(0);
            m_wisdomDialog->setWindowModality(Qt::ApplicationModal);
            m_wisdomDialog->setStyleSheet(QStringLiteral(
                "QProgressDialog { background: #0f0f1a; }"
                "QLabel { color: #c8d8e8; font-size: 13px; }"
                "QProgressBar {"
                "  text-align: center; font-size: 13px;"
                "  font-weight: bold; color: #c8d8e8;"
                "  background: #1a2a3a; border: 1px solid #2e4e6e;"
                "  border-radius: 3px; min-height: 24px;"
                "}"
                "QProgressBar::chunk { background: #00b4d8; }"));
            m_wisdomDialog->show();
        }

        if (m_wisdomDialog) {
            m_wisdomDialog->setValue(percent);
            if (!status.isEmpty() && percent < 100) {
                m_wisdomDialog->setLabelText(
                    QStringLiteral("Optimizing FFT plans for DSP engine...\n\n%1").arg(status));
            }
            if (percent >= 100) {
                m_wisdomDialog->setLabelText(QStringLiteral("FFTW planning complete!"));
                m_wisdomDialog->setValue(100);
                // Auto-close after brief delay
                QTimer::singleShot(800, this, [this]() {
                    if (m_wisdomDialog) {
                        m_wisdomDialog->close();
                        m_wisdomDialog->deleteLater();
                        m_wisdomDialog = nullptr;
                    }
                });
            }
        }
    });

    // Start discovery in background so radios are found before the user opens the panel
    m_radioModel->discovery()->startDiscovery();

    // Auto-reconnect to last radio if it appears
    tryAutoReconnect();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildUI()
{
    setWindowTitle(QStringLiteral("NereusSDR %1").arg(NEREUSSDR_VERSION));
    setMinimumSize(800, 600);
    resize(1280, 800);

    // --- Main QSplitter: spectrum (left) + container panel (right) ---
    // AetherSDR pattern: right panel is a proper layout element, not an overlay.
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->setHandleWidth(3);
    m_mainSplitter->setStyleSheet(QStringLiteral(
        "QSplitter::handle { background: #203040; }"));

    // Left side: spectrum + zoom bar
    auto* spectrumPane = new QWidget(m_mainSplitter);
    spectrumPane->setMinimumWidth(400);
    auto* layout = new QVBoxLayout(spectrumPane);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_spectrumWidget = new SpectrumWidget(spectrumPane);
    m_spectrumWidget->loadSettings();
    m_spectrumWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_spectrumWidget, 1);

    // Zoom slider bar below spectrum
    auto* zoomBar = new QSlider(Qt::Horizontal, spectrumPane);
    zoomBar->setRange(1, 768);
    zoomBar->setValue(768);
    zoomBar->setFixedHeight(20);
    zoomBar->setToolTip(QStringLiteral("Zoom: drag to adjust spectrum bandwidth"));
    zoomBar->setStyleSheet(QStringLiteral(
        "QSlider { background: #0a0a14; }"
        "QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #00b4d8; width: 14px; margin: -4px 0; border-radius: 7px; }"));
    layout->addWidget(zoomBar);
    connect(zoomBar, &QSlider::valueChanged, this, [this](int val) {
        double bwHz = val * 1000.0;
        m_spectrumWidget->setFrequencyRange(m_spectrumWidget->centerFrequency(), bwHz);
        emit m_spectrumWidget->bandwidthChangeRequested(bwHz);
    });

    m_mainSplitter->addWidget(spectrumPane);

    // Right side: Container #0 will be added by ContainerManager
    setCentralWidget(m_mainSplitter);

    // --- Container Infrastructure (Phase 3G-1) ---
    m_containerManager = new ContainerManager(spectrumPane, m_mainSplitter, this);
    m_containerManager->restoreState();
    if (m_containerManager->containerCount() == 0) {
        createDefaultContainers();
    }
    m_containerManager->restoreSplitterState();

    // Default splitter sizes on first run: ~80% spectrum, ~20% panel
    if (!AppSettings::instance().contains(QStringLiteral("MainSplitterSizes"))) {
        m_mainSplitter->setSizes({1024, 256});
    }

    // Wire spectrum display to SliceModel (values come from persisted state,
    // no longer hardcoded). Connection is deferred to wireSliceToSpectrum()
    // which runs after RadioModel creates slice 0.
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int index) {
        if (index == 0) {
            wireSliceToSpectrum();
        }
    });

    // Create FFTEngine on a worker thread (spectrum thread from architecture)
    m_fftEngine = new FFTEngine(0);  // receiver 0
    m_fftEngine->setSampleRate(768000.0);
    m_fftEngine->setFftSize(4096);
    m_fftEngine->setOutputFps(30);

    m_fftThread = new QThread(this);
    m_fftThread->setObjectName(QStringLiteral("SpectrumThread"));
    m_fftEngine->moveToThread(m_fftThread);

    // Clean up FFTEngine when thread finishes
    connect(m_fftThread, &QThread::finished, m_fftEngine, &QObject::deleteLater);

    // Wire: RadioModel raw I/Q → FFTEngine (auto-queued: main → spectrum thread)
    connect(m_radioModel, &RadioModel::rawIqData,
            m_fftEngine, &FFTEngine::feedIQ);

    // Wire: FFTEngine FFT bins → SpectrumWidget (auto-queued: spectrum → main thread)
    connect(m_fftEngine, &FFTEngine::fftReady,
            m_spectrumWidget, &SpectrumWidget::updateSpectrum);

    // Wire: zoom changes → adjust FFT size for appropriate bin resolution
    // Target: ~500-1000 bins across the visible bandwidth for good detail
    connect(m_spectrumWidget, &SpectrumWidget::bandwidthChangeRequested,
            this, [this](double bwHz) {
        // Pick FFT size so bin_width ≈ bw / 1000 (aim for ~1000 bins across display)
        // bin_width = sampleRate / fftSize → fftSize = sampleRate / bin_width
        double sampleRate = m_spectrumWidget->sampleRate();
        int targetBins = 1000;
        int desiredSize = static_cast<int>(sampleRate * targetBins / bwHz);
        // Round up to next power of 2, clamp to valid range
        int fftSize = 1024;
        while (fftSize < desiredSize && fftSize < 65536) {
            fftSize *= 2;
        }
        fftSize = std::clamp(fftSize, 1024, 65536);
        m_fftEngine->setFftSize(fftSize);
    });

    m_fftThread->start();

    // --- Meter Poller (Phase 3G-2) ---
    m_meterPoller = new MeterPoller(this);
    populateDefaultMeter();

    if (m_meterWidget) {
        m_meterPoller->addTarget(m_meterWidget);
    }

    // Wire RxChannel to poller when WDSP finishes initializing.
    // RadioModel's initializedChanged handler creates the RxChannel, but
    // it was registered AFTER this connection (RadioModel registers during
    // onConnectionStateChanged, not buildUI). Qt fires in registration order,
    // so we defer by one event loop pass to ensure RxChannel exists.
    connect(m_radioModel->wdspEngine(), &WdspEngine::initializedChanged,
            this, [this](bool ok) {
        if (!ok) { return; }
        QTimer::singleShot(0, this, [this]() {
            RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
            if (rxCh) {
                m_meterPoller->setRxChannel(rxCh);
                m_meterPoller->start();
                qCDebug(lcMeter) << "MeterPoller started on RxChannel 0";
            } else {
                qCWarning(lcMeter) << "MeterPoller: RxChannel 0 still null after WDSP init";
            }
        });
    });
}

void MainWindow::createDefaultContainers()
{
    // Container #0: panel-docked right side (AetherSDR pattern).
    // Placeholder content in 3G-1, replaced by AppletPanel in 3G-AP.
    ContainerWidget* c0 = m_containerManager->createContainer(1, DockMode::PanelDocked);
    c0->setNotes(QStringLiteral("Main Panel"));
    c0->setNoControls(false);

    qCDebug(lcContainer) << "Created default Container #0 (panel-docked):" << c0->id();
}

void MainWindow::populateDefaultMeter()
{
    ContainerWidget* c0 = m_containerManager->panelContainer();
    if (!c0) {
        qCWarning(lcContainer) << "No panel container for meter widget";
        return;
    }

    m_meterWidget = new MeterWidget();

    // S-Meter: top 55% — arc needle bound to SignalAvg
    // From Thetis MeterManager.cs: ANAN needle uses AVG_SIGNAL_STRENGTH
    ItemGroup* smeter = ItemGroup::createSMeterPreset(
        MeterBinding::SignalAvg, QStringLiteral("S-Meter"), m_meterWidget);
    smeter->installInto(m_meterWidget, 0.0f, 0.0f, 1.0f, 0.55f);
    delete smeter;

    // Power/SWR: middle 30% — stacked bars (stub TX bindings)
    ItemGroup* pwrSwr = ItemGroup::createPowerSwrPreset(
        QStringLiteral("Power/SWR"), m_meterWidget);
    pwrSwr->installInto(m_meterWidget, 0.0f, 0.55f, 1.0f, 0.30f);
    delete pwrSwr;

    // ALC: bottom 15% — horizontal bar (stub TX binding)
    ItemGroup* alc = ItemGroup::createAlcPreset(m_meterWidget);
    alc->installInto(m_meterWidget, 0.0f, 0.85f, 1.0f, 0.15f);
    delete alc;

    // Build an AppletPanelWidget: MeterWidget on top, PhoneCwApplet below.
    auto* panel = new AppletPanelWidget();
    panel->addWidget(m_meterWidget, QStringLiteral("Meters"));

    // PhoneCwApplet — 30 controls (Phone 13 + CW 9 + FM 8), all NYI.
    m_phoneCwApplet = new PhoneCwApplet(m_radioModel, nullptr);
    panel->addApplet(m_phoneCwApplet);

    c0->setContent(panel);
    qCDebug(lcMeter) << "Installed default meter layout: S-Meter + Power/SWR + ALC";
    qCDebug(lcContainer) << "Added PhoneCwApplet to Container #0";
}

void MainWindow::buildMenuBar()
{
    // =========================================================================
    // FILE
    // =========================================================================
    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));

    fileMenu->addAction(QStringLiteral("&Settings..."), this, [this]() {
        // SetupDialog NYI — placeholder until SetupDialog class is implemented
        qCDebug(lcConnection) << "SetupDialog NYI";
    });

    {
        QMenu* profilesMenu = fileMenu->addMenu(QStringLiteral("&Profiles"));
        profilesMenu->addAction(QStringLiteral("Profile &Manager..."), this, [this]() {
            qCDebug(lcConnection) << "Profile Manager NYI";
        });
        profilesMenu->addAction(QStringLiteral("&Import/Export..."), this, [this]() {
            qCDebug(lcConnection) << "Import/Export NYI";
        });
    }

    fileMenu->addSeparator();

    fileMenu->addAction(QStringLiteral("&Quit"), QKeySequence(Qt::CTRL | Qt::Key_Q),
                        qApp, &QApplication::quit);

    // =========================================================================
    // RADIO
    // =========================================================================
    QMenu* radioMenu = menuBar()->addMenu(QStringLiteral("&Radio"));

    radioMenu->addAction(QStringLiteral("&Connect..."), QKeySequence(Qt::CTRL | Qt::Key_K),
                         this, &MainWindow::showConnectionPanel);

    radioMenu->addAction(QStringLiteral("&Disconnect"), this, [this]() {
        m_radioModel->disconnectFromRadio();
    });

    radioMenu->addSeparator();

    radioMenu->addAction(QStringLiteral("&Radio Setup..."), this, [this]() {
        qCDebug(lcConnection) << "Radio Setup NYI";
    });
    radioMenu->addAction(QStringLiteral("&Antenna Setup..."), this, [this]() {
        qCDebug(lcConnection) << "Antenna Setup NYI";
    });
    radioMenu->addAction(QStringLiteral("Trans&verters..."), this, [this]() {
        qCDebug(lcConnection) << "Transverters NYI";
    });

    radioMenu->addSeparator();

    radioMenu->addAction(QStringLiteral("&Protocol Info"), this, [this]() {
        if (m_radioModel->isConnected()) {
            RadioInfo info = m_radioModel->connection()->radioInfo();
            QString msg = QStringLiteral("Radio: %1\nProtocol: P%2\nFirmware: %3\nMAC: %4\nIP: %5")
                .arg(info.displayName())
                .arg(static_cast<int>(info.protocol))
                .arg(info.firmwareVersion)
                .arg(info.macAddress, info.address.toString());
            qCDebug(lcConnection) << msg;
        }
    });

    // =========================================================================
    // VIEW
    // =========================================================================
    QMenu* viewMenu = menuBar()->addMenu(QStringLiteral("&View"));

    {
        QMenu* panLayoutMenu = viewMenu->addMenu(QStringLiteral("Pan &Layout"));
        panLayoutMenu->addAction(QStringLiteral("&1-up"), this, [this]() {
            qCDebug(lcConnection) << "Pan Layout 1-up NYI";
        });
        panLayoutMenu->addAction(QStringLiteral("2 &Vertical"), this, [this]() {
            qCDebug(lcConnection) << "Pan Layout 2 Vertical NYI";
        });
        panLayoutMenu->addAction(QStringLiteral("2 &Horizontal"), this, [this]() {
            qCDebug(lcConnection) << "Pan Layout 2 Horizontal NYI";
        });
        panLayoutMenu->addAction(QStringLiteral("2×&2 Grid"), this, [this]() {
            qCDebug(lcConnection) << "Pan Layout 2x2 Grid NYI";
        });
        panLayoutMenu->addAction(QStringLiteral("1+2 &Horizontal"), this, [this]() {
            qCDebug(lcConnection) << "Pan Layout 1+2 Horizontal NYI";
        });
    }

    viewMenu->addAction(QStringLiteral("&Add Panadapter"), this, [this]() {
        qCDebug(lcConnection) << "Add Panadapter NYI";
    });
    viewMenu->addAction(QStringLiteral("&Remove Panadapter"), this, [this]() {
        qCDebug(lcConnection) << "Remove Panadapter NYI";
    });

    viewMenu->addSeparator();

    {
        QMenu* bandPlanMenu = viewMenu->addMenu(QStringLiteral("&Band Plan"));
        bandPlanMenu->addAction(QStringLiteral("&Off"), this, [this]() {
            qCDebug(lcConnection) << "Band Plan Off NYI";
        });
        bandPlanMenu->addAction(QStringLiteral("&Small"), this, [this]() {
            qCDebug(lcConnection) << "Band Plan Small NYI";
        });
        bandPlanMenu->addAction(QStringLiteral("&Medium"), this, [this]() {
            qCDebug(lcConnection) << "Band Plan Medium NYI";
        });
        bandPlanMenu->addAction(QStringLiteral("&Large"), this, [this]() {
            qCDebug(lcConnection) << "Band Plan Large NYI";
        });
    }

    {
        QMenu* displayModeMenu = viewMenu->addMenu(QStringLiteral("&Display Mode"));
        displayModeMenu->addAction(QStringLiteral("&Panadapter"), this, [this]() {
            qCDebug(lcConnection) << "Display Mode Panadapter NYI";
        });
        displayModeMenu->addAction(QStringLiteral("&Waterfall"), this, [this]() {
            qCDebug(lcConnection) << "Display Mode Waterfall NYI";
        });
        displayModeMenu->addAction(QStringLiteral("Pan+&WF"), this, [this]() {
            qCDebug(lcConnection) << "Display Mode Pan+WF NYI";
        });
        displayModeMenu->addAction(QStringLiteral("&Scope"), this, [this]() {
            qCDebug(lcConnection) << "Display Mode Scope NYI";
        });
    }

    viewMenu->addSeparator();

    {
        QMenu* uiScaleMenu = viewMenu->addMenu(QStringLiteral("&UI Scale"));
        uiScaleMenu->addAction(QStringLiteral("&100%"), this, [this]() {
            qCDebug(lcConnection) << "UI Scale 100% NYI";
        });
        uiScaleMenu->addAction(QStringLiteral("&125%"), this, [this]() {
            qCDebug(lcConnection) << "UI Scale 125% NYI";
        });
        uiScaleMenu->addAction(QStringLiteral("&150%"), this, [this]() {
            qCDebug(lcConnection) << "UI Scale 150% NYI";
        });
        uiScaleMenu->addAction(QStringLiteral("&175%"), this, [this]() {
            qCDebug(lcConnection) << "UI Scale 175% NYI";
        });
        uiScaleMenu->addAction(QStringLiteral("&200%"), this, [this]() {
            qCDebug(lcConnection) << "UI Scale 200% NYI";
        });
    }

    viewMenu->addAction(QStringLiteral("Dark/&Light Theme"), this, [this]() {
        qCDebug(lcConnection) << "Dark/Light Theme NYI";
    });
    viewMenu->addAction(QStringLiteral("&Minimal Mode"), QKeySequence(Qt::CTRL | Qt::Key_M),
                        this, [this]() {
        qCDebug(lcConnection) << "Minimal Mode NYI";
    });

    viewMenu->addSeparator();

    viewMenu->addAction(QStringLiteral("&Keyboard Shortcuts..."), this, [this]() {
        qCDebug(lcConnection) << "Keyboard Shortcuts NYI";
    });
    viewMenu->addAction(QStringLiteral("&Configure Shortcuts..."), this, [this]() {
        qCDebug(lcConnection) << "Configure Shortcuts NYI";
    });

    // =========================================================================
    // DSP
    // =========================================================================
    QMenu* dspMenu = menuBar()->addMenu(QStringLiteral("&DSP"));

    // Checkable DSP toggles — stored as members so the overlay panel can sync
    m_nrAction = dspMenu->addAction(QStringLiteral("&NR"));
    m_nrAction->setCheckable(true);
    connect(m_nrAction, &QAction::toggled, this, [this](bool on) {
        RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
        if (rxCh) {
            rxCh->setNrEnabled(on);
        }
    });

    QAction* nr2Action = dspMenu->addAction(QStringLiteral("NR&2"));
    nr2Action->setCheckable(true);
    connect(nr2Action, &QAction::toggled, this, [this](bool /*on*/) {
        qCDebug(lcConnection) << "NR2 NYI";
    });

    m_nbAction = dspMenu->addAction(QStringLiteral("N&B"));
    m_nbAction->setCheckable(true);
    connect(m_nbAction, &QAction::toggled, this, [this](bool on) {
        RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
        if (rxCh) {
            rxCh->setNb1Enabled(on);
        }
    });

    QAction* nb2Action = dspMenu->addAction(QStringLiteral("NB&2"));
    nb2Action->setCheckable(true);
    connect(nb2Action, &QAction::toggled, this, [this](bool /*on*/) {
        qCDebug(lcConnection) << "NB2 NYI";
    });

    m_anfAction = dspMenu->addAction(QStringLiteral("&ANF"));
    m_anfAction->setCheckable(true);
    connect(m_anfAction, &QAction::toggled, this, [this](bool on) {
        RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
        if (rxCh) {
            rxCh->setAnfEnabled(on);
        }
    });

    QAction* tnfAction = dspMenu->addAction(QStringLiteral("&TNF"));
    tnfAction->setCheckable(true);
    connect(tnfAction, &QAction::toggled, this, [this](bool /*on*/) {
        qCDebug(lcConnection) << "TNF NYI";
    });

    QAction* binAction = dspMenu->addAction(QStringLiteral("&BIN"));
    binAction->setCheckable(true);
    connect(binAction, &QAction::toggled, this, [this](bool /*on*/) {
        qCDebug(lcConnection) << "BIN NYI";
    });

    dspMenu->addSeparator();

    {
        QMenu* agcMenu = dspMenu->addMenu(QStringLiteral("&AGC"));
        // AGCMode enum: Off=0, Long=1, Slow=2, Med=3, Fast=4, Custom=5
        // From Thetis dsp.cs AGCMode — Long is present in the enum but the
        // spec menu has: Off, Slow, Med, Fast, Custom
        agcMenu->addAction(QStringLiteral("&Off"), this, [this]() {
            SliceModel* slice = m_radioModel->activeSlice();
            if (slice) { slice->setAgcMode(AGCMode::Off); }
        });
        agcMenu->addAction(QStringLiteral("&Slow"), this, [this]() {
            SliceModel* slice = m_radioModel->activeSlice();
            if (slice) { slice->setAgcMode(AGCMode::Slow); }
        });
        agcMenu->addAction(QStringLiteral("&Med"), this, [this]() {
            SliceModel* slice = m_radioModel->activeSlice();
            if (slice) { slice->setAgcMode(AGCMode::Med); }
        });
        agcMenu->addAction(QStringLiteral("&Fast"), this, [this]() {
            SliceModel* slice = m_radioModel->activeSlice();
            if (slice) { slice->setAgcMode(AGCMode::Fast); }
        });
        agcMenu->addAction(QStringLiteral("&Custom"), this, [this]() {
            SliceModel* slice = m_radioModel->activeSlice();
            if (slice) { slice->setAgcMode(AGCMode::Custom); }
        });
    }

    dspMenu->addSeparator();

    dspMenu->addAction(QStringLiteral("&Equalizer..."), this, [this]() {
        qCDebug(lcConnection) << "Equalizer NYI";
    });
    dspMenu->addAction(QStringLiteral("&PureSignal..."), this, [this]() {
        qCDebug(lcConnection) << "PureSignal NYI";
    });
    dspMenu->addAction(QStringLiteral("&Diversity..."), this, [this]() {
        qCDebug(lcConnection) << "Diversity NYI";
    });

    // =========================================================================
    // BAND
    // =========================================================================
    QMenu* bandMenu = menuBar()->addMenu(QStringLiteral("&Band"));

    {
        QMenu* hfMenu = bandMenu->addMenu(QStringLiteral("&HF"));
        // Frequency values from Thetis console.cs band definitions
        struct { const char* label; double freqHz; } hfBands[] = {
            { "160m (1.8 MHz)",    1.8e6   },
            { "80m (3.5 MHz)",     3.5e6   },
            { "60m (5.3 MHz)",     5.3e6   },
            { "40m (7.0 MHz)",     7.0e6   },
            { "30m (10.1 MHz)",   10.1e6   },
            { "20m (14.0 MHz)",   14.0e6   },
            { "17m (18.068 MHz)", 18.068e6 },
            { "15m (21.0 MHz)",   21.0e6   },
            { "12m (24.89 MHz)",  24.89e6  },
            { "10m (28.0 MHz)",   28.0e6   },
            { "6m (50.0 MHz)",    50.0e6   },
        };
        for (const auto& band : hfBands) {
            double freq = band.freqHz;
            hfMenu->addAction(QString::fromUtf8(band.label), this, [this, freq]() {
                SliceModel* slice = m_radioModel->activeSlice();
                if (slice) {
                    slice->setFrequency(freq);
                }
            });
        }
    }

    {
        QMenu* vhfMenu = bandMenu->addMenu(QStringLiteral("&VHF"));
        vhfMenu->addAction(QStringLiteral("2m (144.0 MHz)"), this, [this]() {
            qCDebug(lcConnection) << "VHF 2m NYI";
        });
        vhfMenu->addAction(QStringLiteral("70cm (432.0 MHz)"), this, [this]() {
            qCDebug(lcConnection) << "VHF 70cm NYI";
        });
    }

    bandMenu->addAction(QStringLiteral("&GEN"), this, [this]() {
        qCDebug(lcConnection) << "GEN band NYI";
    });

    bandMenu->addAction(QStringLiteral("&WWV (10.0 MHz)"), this, [this]() {
        SliceModel* slice = m_radioModel->activeSlice();
        if (slice) {
            slice->setFrequency(10.0e6);
        }
    });

    bandMenu->addSeparator();

    bandMenu->addAction(QStringLiteral("Band &Stacking..."), this, [this]() {
        qCDebug(lcConnection) << "Band Stacking NYI";
    });

    // =========================================================================
    // MODE
    // =========================================================================
    QMenu* modeMenu = menuBar()->addMenu(QStringLiteral("&Mode"));

    // 12 modes in display order (spec order): LSB, USB, DSB, CWL, CWU, AM,
    // SAM, FM, DIGL, DIGU, DRM, SPEC.
    // Maps to DSPMode enum values from WdspTypes.h.
    // From Thetis dsp.cs DSPMode enum — enum values used directly, not indices.
    struct { const char* label; DSPMode mode; } modes[] = {
        { "LSB",  DSPMode::LSB  },
        { "USB",  DSPMode::USB  },
        { "DSB",  DSPMode::DSB  },
        { "CWL",  DSPMode::CWL  },
        { "CWU",  DSPMode::CWU  },
        { "AM",   DSPMode::AM   },
        { "SAM",  DSPMode::SAM  },
        { "FM",   DSPMode::FM   },
        { "DIGL", DSPMode::DIGL },
        { "DIGU", DSPMode::DIGU },
        { "DRM",  DSPMode::DRM  },
        { "SPEC", DSPMode::SPEC },
    };

    m_modeActionGroup = new QActionGroup(this);
    m_modeActionGroup->setExclusive(true);

    for (int i = 0; i < 12; ++i) {
        DSPMode mode = modes[i].mode;
        QAction* act = modeMenu->addAction(QString::fromUtf8(modes[i].label),
                                           this, [this, mode]() {
            SliceModel* slice = m_radioModel->activeSlice();
            if (slice) {
                slice->setDspMode(mode);
            }
        });
        act->setCheckable(true);
        m_modeActionGroup->addAction(act);
        m_modeActions[i] = act;
    }

    // Sync checked mode action when SliceModel reports a mode change.
    // Connection is deferred until slice 0 is available (sliceAdded signal).
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int index) {
        if (index != 0) { return; }
        SliceModel* slice = m_radioModel->activeSlice();
        if (!slice) { return; }
        connect(slice, &SliceModel::dspModeChanged, this, [this](DSPMode mode) {
            DSPMode displayOrder[] = {
                DSPMode::LSB, DSPMode::USB, DSPMode::DSB, DSPMode::CWL,
                DSPMode::CWU, DSPMode::AM,  DSPMode::SAM,  DSPMode::FM,
                DSPMode::DIGL, DSPMode::DIGU, DSPMode::DRM, DSPMode::SPEC,
            };
            for (int i = 0; i < 12; ++i) {
                if (m_modeActions[i]) {
                    m_modeActions[i]->setChecked(displayOrder[i] == mode);
                }
            }
        });
    });

    // =========================================================================
    // CONTAINERS
    // =========================================================================
    QMenu* containersMenu = menuBar()->addMenu(QStringLiteral("Contai&ners"));

    containersMenu->addAction(QStringLiteral("&New Container..."), this, [this]() {
        qCDebug(lcConnection) << "New Container NYI";
    });
    containersMenu->addAction(QStringLiteral("Container &Settings..."), this, [this]() {
        qCDebug(lcConnection) << "Container Settings NYI";
    });

    containersMenu->addSeparator();

    containersMenu->addAction(QStringLiteral("&Reset Default Layout"), this, [this]() {
        qCDebug(lcConnection) << "Reset Default Layout NYI";
    });

    // =========================================================================
    // TOOLS
    // =========================================================================
    QMenu* toolsMenu = menuBar()->addMenu(QStringLiteral("&Tools"));

    toolsMenu->addAction(QStringLiteral("C&WX..."), this, [this]() {
        qCDebug(lcConnection) << "CWX NYI";
    });
    toolsMenu->addAction(QStringLiteral("&Memory Manager..."), this, [this]() {
        qCDebug(lcConnection) << "Memory Manager NYI";
    });
    toolsMenu->addAction(QStringLiteral("&CAT Control..."), this, [this]() {
        qCDebug(lcConnection) << "CAT Control NYI";
    });
    toolsMenu->addAction(QStringLiteral("&TCI Server..."), this, [this]() {
        qCDebug(lcConnection) << "TCI Server NYI";
    });
    toolsMenu->addAction(QStringLiteral("&DAX Audio..."), this, [this]() {
        qCDebug(lcConnection) << "DAX Audio NYI";
    });

    toolsMenu->addSeparator();

    toolsMenu->addAction(QStringLiteral("&MIDI Mapping..."), this, [this]() {
        qCDebug(lcConnection) << "MIDI Mapping NYI";
    });
    toolsMenu->addAction(QStringLiteral("Macro &Buttons..."), this, [this]() {
        qCDebug(lcConnection) << "Macro Buttons NYI";
    });

    toolsMenu->addSeparator();

    toolsMenu->addAction(QStringLiteral("&Network Diagnostics..."), this, [this]() {
        qCDebug(lcConnection) << "Network Diagnostics NYI";
    });
    toolsMenu->addAction(QStringLiteral("&Support Bundle..."), this,
                         &MainWindow::showSupportDialog);

    // =========================================================================
    // HELP
    // =========================================================================
    QMenu* helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));

    helpMenu->addAction(QStringLiteral("&Getting Started..."), this, [this]() {
        qCDebug(lcConnection) << "Getting Started NYI";
    });
    helpMenu->addAction(QStringLiteral("&NereusSDR Help..."), this, [this]() {
        qCDebug(lcConnection) << "NereusSDR Help NYI";
    });
    helpMenu->addAction(QStringLiteral("Understanding &Data Modes..."), this, [this]() {
        qCDebug(lcConnection) << "Understanding Data Modes NYI";
    });

    helpMenu->addSeparator();

    helpMenu->addAction(QStringLiteral("What's &New..."), this, [this]() {
        qCDebug(lcConnection) << "What's New NYI";
    });

    helpMenu->addAction(QStringLiteral("&About NereusSDR"), this, [this]() {
        Q_UNUSED(this);
    });
}

void MainWindow::buildStatusBar()
{
    // AetherSDR double-height status bar (46px fixed height, 3-section layout)
    QStatusBar* sb = statusBar();
    sb->setFixedHeight(46);
    sb->setSizeGripEnabled(false);
    sb->setStyleSheet(QStringLiteral(
        "QStatusBar { background: #0a0a14; border-top: 1px solid #203040; }"
        "QStatusBar::item { border: none; }"));

    // Wrapper widget for the full-width custom layout
    QWidget* barWidget = new QWidget(sb);
    barWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QHBoxLayout* hbox = new QHBoxLayout(barWidget);
    hbox->setContentsMargins(6, 0, 6, 0);
    hbox->setSpacing(6);

    // ── Left section ──────────────────────────────────────────────────────────

    // Band Stack: three grey circles (NYI)
    auto* bandStackLabel = new QLabel(barWidget);
    bandStackLabel->setFixedSize(10, 22);
    {
        QPixmap pm(10, 22);
        pm.fill(Qt::transparent);
        QPainter painter(&pm);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor(0x40, 0x48, 0x58));
        painter.setPen(Qt::NoPen);
        for (int i = 0; i < 3; ++i) {
            painter.drawEllipse(0, i * 7, 9, 6);
        }
        painter.end();
        bandStackLabel->setPixmap(pm);
    }
    bandStackLabel->setToolTip(QStringLiteral("Band Stack (NYI)"));
    hbox->addWidget(bandStackLabel);

    // +PAN icon (NYI)
    auto* panLabel = new QLabel(QStringLiteral("+PAN"), barWidget);
    panLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #404858; font-weight: bold; font-size: 22px; }"));
    panLabel->setToolTip(QStringLiteral("+PAN (NYI)"));
    hbox->addWidget(panLabel);

    // Panel toggle (☰)
    auto* panelToggleLabel = new QLabel(QStringLiteral("☰"), barWidget);
    panelToggleLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #404858; font-weight: bold; font-size: 22px; }"));
    panelToggleLabel->setToolTip(QStringLiteral("Toggle panel"));
    hbox->addWidget(panelToggleLabel);

    // TNF toggle
    m_tnfLabel = new QLabel(QStringLiteral("TNF"), barWidget);
    m_tnfLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #404858; font-weight: bold; font-size: 24px; }"));
    m_tnfLabel->setToolTip(QStringLiteral("Tracking Notch Filter (NYI)"));
    hbox->addWidget(m_tnfLabel);

    // CWX
    auto* cwxLabel = new QLabel(QStringLiteral("CWX"), barWidget);
    cwxLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #404858; font-weight: bold; font-size: 24px; }"));
    cwxLabel->setToolTip(QStringLiteral("CW Keyer (NYI)"));
    hbox->addWidget(cwxLabel);

    // DVK
    auto* dvkLabel = new QLabel(QStringLiteral("DVK"), barWidget);
    dvkLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #404858; font-weight: bold; font-size: 24px; }"));
    dvkLabel->setToolTip(QStringLiteral("Digital Voice Keyer (NYI)"));
    hbox->addWidget(dvkLabel);

    // FDX
    auto* fdxLabel = new QLabel(QStringLiteral("FDX"), barWidget);
    fdxLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #404858; font-weight: bold; font-size: 24px; }"));
    fdxLabel->setToolTip(QStringLiteral("Full Duplex (NYI)"));
    hbox->addWidget(fdxLabel);

    // Separator ·
    auto* sep1 = new QLabel(QStringLiteral(" · "), barWidget);
    sep1->setStyleSheet(QStringLiteral(
        "QLabel { color: #304050; font-size: 21px; }"));
    hbox->addWidget(sep1);

    // Radio info: stacked model + firmware labels
    auto* radioInfoWidget = new QWidget(barWidget);
    QVBoxLayout* radioVbox = new QVBoxLayout(radioInfoWidget);
    radioVbox->setContentsMargins(0, 0, 0, 0);
    radioVbox->setSpacing(0);

    m_radioModelLabel = new QLabel(QStringLiteral("—"), radioInfoWidget);
    m_radioModelLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #8aa8c0; font-size: 12px; }"));
    radioVbox->addWidget(m_radioModelLabel);

    m_radioFwLabel = new QLabel(QStringLiteral("—"), radioInfoWidget);
    m_radioFwLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-size: 12px; }"));
    radioVbox->addWidget(m_radioFwLabel);

    // Keep m_connStatusLabel pointing at model label (legacy compat)
    m_connStatusLabel = m_radioModelLabel;

    hbox->addWidget(radioInfoWidget);

    // ── Stretch ───────────────────────────────────────────────────────────────
    hbox->addStretch(1);

    // ── Center section: STATION callsign ─────────────────────────────────────
    auto* stationContainer = new QWidget(barWidget);
    stationContainer->setStyleSheet(QStringLiteral(
        "QWidget { border: 1px solid rgba(255,255,255,128); background: #0a0a14; padding: 2px 12px; }"));
    QHBoxLayout* stationHbox = new QHBoxLayout(stationContainer);
    stationHbox->setContentsMargins(0, 0, 0, 0);
    stationHbox->setSpacing(6);

    auto* stationLabel = new QLabel(QStringLiteral("STATION:"), stationContainer);
    stationLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #8aa8c0; font-size: 21px; border: none; background: transparent; padding: 0; }"));
    stationHbox->addWidget(stationLabel);

    m_callsignLabel = new QLabel(stationContainer);
    QString callsign = AppSettings::instance().value(
        QStringLiteral("StationCallsign"), QStringLiteral("—")).toString();
    m_callsignLabel->setText(callsign);
    m_callsignLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #c8d8e8; font-size: 21px; border: none; background: transparent; padding: 0; }"));
    stationHbox->addWidget(m_callsignLabel);

    hbox->addWidget(stationContainer);

    // ── Stretch ───────────────────────────────────────────────────────────────
    hbox->addStretch(1);

    // ── Right section: indicators ────────────────────────────────────────────

    // Helper lambda: create a stacked indicator pair (top label + bottom label)
    auto makeIndicator = [&](const QString& top, const QString& bottom) -> QWidget* {
        QWidget* w = new QWidget(barWidget);
        w->setMinimumWidth(84);
        QVBoxLayout* vl = new QVBoxLayout(w);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);
        auto* topLbl = new QLabel(top, w);
        topLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: #8aa8c0; font-size: 12px; }"));
        auto* botLbl = new QLabel(bottom, w);
        botLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: #607080; font-size: 12px; }"));
        vl->addWidget(topLbl);
        vl->addWidget(botLbl);
        return w;
    };

    // CAT
    hbox->addWidget(makeIndicator(QStringLiteral("CAT"), QStringLiteral("Off")));

    // Separator ·
    auto* sep2 = new QLabel(QStringLiteral(" · "), barWidget);
    sep2->setStyleSheet(QStringLiteral("QLabel { color: #304050; font-size: 21px; }"));
    hbox->addWidget(sep2);

    // TCI
    hbox->addWidget(makeIndicator(QStringLiteral("TCI"), QStringLiteral("Off")));

    // Separator ·
    auto* sep3 = new QLabel(QStringLiteral(" · "), barWidget);
    sep3->setStyleSheet(QStringLiteral("QLabel { color: #304050; font-size: 21px; }"));
    hbox->addWidget(sep3);

    // PA Voltage
    hbox->addWidget(makeIndicator(QStringLiteral("PA"), QStringLiteral("— V")));

    // Separator ·
    auto* sep4 = new QLabel(QStringLiteral(" · "), barWidget);
    sep4->setStyleSheet(QStringLiteral("QLabel { color: #304050; font-size: 21px; }"));
    hbox->addWidget(sep4);

    // CPU
    hbox->addWidget(makeIndicator(QStringLiteral("CPU"), QStringLiteral("—%")));

    // Separator ·
    auto* sep5 = new QLabel(QStringLiteral(" · "), barWidget);
    sep5->setStyleSheet(QStringLiteral("QLabel { color: #304050; font-size: 21px; }"));
    hbox->addWidget(sep5);

    // TX indicator
    auto* txLabel = new QLabel(QStringLiteral("TX"), barWidget);
    txLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: rgba(255,255,255,128); font-weight: bold; font-size: 21px; }"));
    txLabel->setToolTip(QStringLiteral("Transmit active"));
    hbox->addWidget(txLabel);

    // Separator ·
    auto* sep6 = new QLabel(QStringLiteral(" · "), barWidget);
    sep6->setStyleSheet(QStringLiteral("QLabel { color: #304050; font-size: 21px; }"));
    hbox->addWidget(sep6);

    // UTC clock
    m_utcTimeLabel = new QLabel(barWidget);
    m_utcTimeLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #c8d8e8; font-size: 21px; }"));
    m_utcTimeLabel->setText(QDateTime::currentDateTimeUtc().toString(
        QStringLiteral("hh:mm:ss UTC")));
    hbox->addWidget(m_utcTimeLabel);

    // UTC clock timer (1-second updates)
    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, [this]() {
        m_utcTimeLabel->setText(QDateTime::currentDateTimeUtc().toString(
            QStringLiteral("hh:mm:ss UTC")));
    });
    m_clockTimer->start(1000);

    // Add the full-width bar widget to the status bar
    sb->addWidget(barWidget, 1);
}

void MainWindow::wireSliceToSpectrum()
{
    SliceModel* slice = m_radioModel->activeSlice();
    if (!slice || !m_spectrumWidget) {
        return;
    }

    // Set initial spectrum display — 48 kHz centered on VFO.
    // With N/2 FFT, only positive frequencies (right half) show real signals.
    double freq = slice->frequency();
    m_spectrumWidget->setFrequencyRange(freq, 768000.0);
    m_spectrumWidget->setDdcCenterFrequency(freq);
    m_spectrumWidget->setSampleRate(768000.0);
    m_spectrumWidget->setVfoFrequency(freq);
    m_spectrumWidget->setFilterOffset(slice->filterLow(), slice->filterHigh());
    m_spectrumWidget->setStepSize(slice->stepHz());

    // --- Create floating VFO flag widget (AetherSDR pattern) ---
    VfoWidget* vfo = m_spectrumWidget->addVfoWidget(0);
    vfo->setFrequency(freq);
    vfo->setMode(slice->dspMode());
    vfo->setFilter(slice->filterLow(), slice->filterHigh());
    vfo->setAgcMode(slice->agcMode());
    vfo->setAfGain(slice->afGain());
    vfo->setRfGain(slice->rfGain());
    vfo->setRxAntenna(slice->rxAntenna());
    vfo->setTxAntenna(slice->txAntenna());
    vfo->setStepHz(slice->stepHz());

    // --- Slice → spectrum display ---

    // VFO frequency change → move VFO marker
    // In CTUN mode (SmartSDR-style): pan stays fixed, VFO moves within it.
    // In traditional mode: pan follows VFO (auto-scroll handled in setVfoFrequency).
    // Band changes (large jumps) always recenter regardless of mode.
    connect(slice, &SliceModel::frequencyChanged, this, [this, vfo, slice](double freq) {
        if (m_handlingBandJump) {
            return;
        }

        double center = m_spectrumWidget->centerFrequency();
        double halfBw = m_spectrumWidget->bandwidth() / 2.0;
        bool offScreen = (freq < center - halfBw) || (freq > center + halfBw);

        if (!m_spectrumWidget->ctunEnabled() || offScreen) {
            m_handlingBandJump = true;

            bool wasCTUN = m_spectrumWidget->ctunEnabled();
            m_radioModel->receiverManager()->setDdcFrequencyLocked(false);

            m_spectrumWidget->setCenterFrequency(freq);

            int rxIdx = slice->receiverIndex();
            if (rxIdx >= 0) {
                m_radioModel->receiverManager()->forceHardwareFrequency(
                    rxIdx, static_cast<quint64>(freq));
            }
            m_spectrumWidget->setDdcCenterFrequency(freq);

            RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
            if (rxCh) {
                rxCh->setShiftFrequency(0.0);
            }

            if (wasCTUN) {
                m_radioModel->receiverManager()->setDdcFrequencyLocked(true);
            }

            m_handlingBandJump = false;
        } else {
            // From Thetis radio.rs:1417 — WDSP shift = +(freq - center)
            double shiftHz = freq - center;
            RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
            if (rxCh) {
                rxCh->setShiftFrequency(shiftHz);
            }
        }
        m_spectrumWidget->setVfoFrequency(freq);
        vfo->setFrequency(freq);
    });

    connect(slice, &SliceModel::filterChanged, this, [this, vfo](int low, int high) {
        m_spectrumWidget->setFilterOffset(low, high);
        vfo->setFilter(low, high);
    });

    connect(slice, &SliceModel::dspModeChanged, this, [vfo](DSPMode mode) {
        vfo->setMode(mode);
    });

    connect(slice, &SliceModel::agcModeChanged, this, [vfo](AGCMode mode) {
        vfo->setAgcMode(mode);
    });

    connect(slice, &SliceModel::afGainChanged, this, [vfo](int gain) {
        vfo->setAfGain(gain);
    });

    connect(slice, &SliceModel::rfGainChanged, this, [vfo](int gain) {
        vfo->setRfGain(gain);
    });

    connect(slice, &SliceModel::stepHzChanged, this, [this, vfo](int hz) {
        m_spectrumWidget->setStepSize(hz);
        vfo->setStepHz(hz);
    });

    connect(slice, &SliceModel::rxAntennaChanged, this, [vfo](const QString& ant) {
        vfo->setRxAntenna(ant);
    });

    connect(slice, &SliceModel::txAntennaChanged, this, [vfo](const QString& ant) {
        vfo->setTxAntenna(ant);
    });

    // --- VFO flag → slice ---

    connect(vfo, &VfoWidget::frequencyChanged, this, [slice](double hz) {
        slice->setFrequency(hz);
    });

    connect(vfo, &VfoWidget::modeChanged, this, [slice](DSPMode mode) {
        slice->setDspMode(mode);
    });

    connect(vfo, &VfoWidget::filterChanged, this, [slice](int low, int high) {
        slice->setFilter(low, high);
    });

    connect(vfo, &VfoWidget::agcModeChanged, this, [slice](AGCMode mode) {
        slice->setAgcMode(mode);
    });

    connect(vfo, &VfoWidget::afGainChanged, this, [slice](int gain) {
        slice->setAfGain(gain);
    });

    connect(vfo, &VfoWidget::rfGainChanged, this, [slice](int gain) {
        slice->setRfGain(gain);
    });

    connect(vfo, &VfoWidget::rxAntennaChanged, this, [slice](const QString& ant) {
        slice->setRxAntenna(ant);
    });

    connect(vfo, &VfoWidget::txAntennaChanged, this, [slice](const QString& ant) {
        slice->setTxAntenna(ant);
    });

    // NB/NR/ANF → RxChannel directly (not SliceModel properties)
    connect(vfo, &VfoWidget::nb1Changed, this, [this](bool on) {
        RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
        if (rxCh) { rxCh->setNb1Enabled(on); }
    });
    connect(vfo, &VfoWidget::nrChanged, this, [this](bool on) {
        RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
        if (rxCh) { rxCh->setNrEnabled(on); }
    });
    connect(vfo, &VfoWidget::anfChanged, this, [this](bool on) {
        RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
        if (rxCh) { rxCh->setAnfEnabled(on); }
    });

    connect(vfo, &VfoWidget::sliceActivationRequested, this, [this](int index) {
        m_radioModel->setActiveSlice(index);
    });

    // --- Spectrum click-to-tune → slice ---
    connect(m_spectrumWidget, &SpectrumWidget::frequencyClicked,
            this, [slice](double hz) {
        slice->setFrequency(hz);
    });

    // --- Spectrum filter edge drag → slice ---
    connect(m_spectrumWidget, &SpectrumWidget::filterEdgeDragged,
            this, [slice](int low, int high) {
        slice->setFilter(low, high);
    });

    // --- Pan center changed (pan drag) ---
    // CTUN mode (SmartSDR): retune DDC to pan center, offset WDSP to keep
    // demodulating at VFO frequency. This lets the spectrum show real data
    // across the full pan range.
    // Traditional mode: pan drag retunes the VFO (DDC follows VFO naturally).
    connect(m_spectrumWidget, &SpectrumWidget::centerChanged,
            this, [this, slice](double centerHz) {
        if (m_handlingBandJump) {
            return;
        }
        if (!m_spectrumWidget->ctunEnabled()) {
            slice->setFrequency(centerHz);
        } else {
            // CTUN: retune DDC to pan center (bypasses lock) so spectrum shows correct data
            int rxIdx = slice->receiverIndex();
            if (rxIdx >= 0) {
                m_radioModel->receiverManager()->forceHardwareFrequency(
                    rxIdx, static_cast<quint64>(centerHz));
            }
            m_spectrumWidget->setDdcCenterFrequency(centerHz);
            // Offset WDSP shift so audio stays on VFO frequency
            // From Thetis radio.cs:1417 — SetRXAShiftFreq receives +(freq - center)
            double shiftHz = slice->frequency() - centerHz;
            RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
            if (rxCh) {
                rxCh->setShiftFrequency(shiftHz);
            }
        }
    });

    // --- CTUN mode toggled → lock/unlock DDC ---
    connect(m_spectrumWidget, &SpectrumWidget::ctunEnabledChanged,
            this, [this](bool enabled) {
        m_radioModel->receiverManager()->setDdcFrequencyLocked(enabled);
        if (!enabled) {
            RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
            if (rxCh) {
                rxCh->setShiftFrequency(0.0);
            }
        }
    });

    // Set initial lock state
    m_radioModel->receiverManager()->setDdcFrequencyLocked(
        m_spectrumWidget->ctunEnabled());

    // Position the VFO flag
    m_spectrumWidget->updateVfoPositions();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);

    // Update axis-lock positions for overlay-docked containers
    if (m_mainSplitter && m_containerManager) {
        // Use the spectrum pane (first splitter child) as reference
        QWidget* spectrumPane = m_mainSplitter->widget(0);
        if (spectrumPane) {
            m_hDelta = spectrumPane->width();
            m_vDelta = spectrumPane->height();
            m_containerManager->updateDockedPositions(m_hDelta, m_vDelta);
        }
    }
}

void MainWindow::applyDarkTheme()
{
    setStyleSheet(QStringLiteral(
        "QMainWindow { background: #0f0f1a; }"
        "QMenuBar {"
        "  background: #1a2a3a;"
        "  color: #c8d8e8;"
        "  border-bottom: 1px solid #203040;"
        "}"
        "QMenuBar::item:selected { background: #00b4d8; }"
        "QMenu {"
        "  background: #1a2a3a;"
        "  color: #c8d8e8;"
        "  border: 1px solid #203040;"
        "}"
        "QMenu::item:selected { background: #00b4d8; }"
        "QLabel { color: #c8d8e8; }"
        "QStatusBar {"
        "  background: #1a2a3a;"
        "  color: #8090a0;"
        "  border-top: 1px solid #203040;"
        "}"));
}

void MainWindow::showConnectionPanel()
{
    if (!m_connectionPanel) {
        m_connectionPanel = new ConnectionPanel(m_radioModel, this);
        m_connectionPanel->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_connectionPanel, &QObject::destroyed, this, [this]() {
            m_connectionPanel = nullptr;
        });
    }
    m_connectionPanel->show();
    m_connectionPanel->raise();
    m_connectionPanel->activateWindow();
}

void MainWindow::showSupportDialog()
{
    if (!m_supportDialog) {
        m_supportDialog = new SupportDialog(m_radioModel, this);
        m_supportDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_supportDialog, &QObject::destroyed, this, [this]() {
            m_supportDialog = nullptr;
        });
    }
    m_supportDialog->show();
    m_supportDialog->raise();
    m_supportDialog->activateWindow();
}

void MainWindow::onConnectionStateChanged()
{
    if (m_radioModel->isConnected()) {
        m_radioModelLabel->setText(m_radioModel->name());
        m_radioModelLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #c8d8e8; font-size: 12px; }"));
        m_radioFwLabel->setText(QStringLiteral("FW %1").arg(m_radioModel->version()));
        m_radioFwLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #8aa8c0; font-size: 12px; }"));
    } else {
        m_radioModelLabel->setText(QStringLiteral("—"));
        m_radioModelLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #8aa8c0; font-size: 12px; }"));
        m_radioFwLabel->setText(QStringLiteral("—"));
        m_radioFwLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #607080; font-size: 12px; }"));
    }
}

void MainWindow::tryAutoReconnect()
{
    QString lastMac = AppSettings::instance()
        .value(QStringLiteral("LastConnectedRadioMac")).toString();
    if (lastMac.isEmpty()) {
        return;
    }

    // When a radio matching the last MAC is discovered, auto-connect
    connect(m_radioModel->discovery(), &RadioDiscovery::radioDiscovered,
            this, [this, lastMac](const RadioInfo& info) {
        if (info.macAddress == lastMac && !m_radioModel->isConnected() && !info.inUse) {
            qCDebug(lcConnection) << "Auto-reconnecting to" << info.displayName();
            m_radioModel->connectToRadio(info);
        }
    });
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Stop discovery to prevent new signals during shutdown
    m_radioModel->discovery()->stopDiscovery();

    // Stop FFT thread
    if (m_fftThread && m_fftThread->isRunning()) {
        m_fftThread->quit();
        m_fftThread->wait(2000);
    }

    // Save display settings before shutdown
    if (m_spectrumWidget) {
        m_spectrumWidget->saveSettings();
    }

    // Tear down connection (sends stop command, closes sockets, joins thread)
    m_radioModel->disconnectFromRadio();

    // Save container layout
    if (m_containerManager) {
        m_containerManager->saveState();
    }

    AppSettings::instance().save();
    event->accept();

    // Force process exit. Worker threads and active QObjects can prevent
    // the Qt event loop from returning. Settings are saved, sockets closed,
    // and stop command sent to the radio — safe to exit now.
    std::exit(0);
}

} // namespace NereusSDR
