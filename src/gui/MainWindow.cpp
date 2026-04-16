#include "MainWindow.h"
#include "ConnectionPanel.h"
#include "SupportDialog.h"
#include "AboutDialog.h"
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
#include "core/ClarityController.h"
#include "core/StepAttenuatorController.h"
#include "core/BoardCapabilities.h"
#include "models/PanadapterModel.h"
#include "models/Band.h"
#include "models/TransmitModel.h"
#include "core/LogCategories.h"
#include "containers/ContainerManager.h"
#include "containers/ContainerWidget.h"
#include "containers/ContainerSettingsDialog.h"
#include "meters/MeterWidget.h"
#include "meters/MeterItem.h"
#include "meters/ItemGroup.h"
#include "meters/MeterPoller.h"
#include "applets/AppletPanelWidget.h"
#include "applets/RxApplet.h"
#include "applets/TxApplet.h"
#include "applets/PhoneCwApplet.h"
#include "applets/EqApplet.h"
#include "applets/DigitalApplet.h"
#include "applets/PureSignalApplet.h"
#include "applets/DiversityApplet.h"
#include "applets/CwxApplet.h"
#include "applets/DvkApplet.h"
#include "applets/CatApplet.h"
#include "applets/TunerApplet.h"
#include "SpectrumOverlayPanel.h"
#include "SetupDialog.h"

#include <cmath>

#include <QApplication>
#include <QSlider>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QEvent>
#include <QMouseEvent>
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
#include <QMessageBox>
#include <QTimer>
#include <QThread>
#include <QPushButton>
#include <QToolBar>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>
#include <QPointer>

#include <cstdlib>

#ifdef Q_OS_MAC
#include <sys/resource.h>
#endif

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

    // Auto-reconnect to last radio — deferred so the event loop is running
    // before any signal/slot activity (e.g. discovery radioDiscovered).
    QTimer::singleShot(0, this, &MainWindow::tryAutoReconnect);

    // Defensive save on aboutToQuit. closeEvent is fine for ⌘Q but
    // does NOT run when the process is signaled (SIGTERM from pkill,
    // Activity Monitor force-quit, debugger detach). Without this
    // hook, any container/state change made mid-session is lost on
    // signal-based shutdown. saveState is idempotent so the ⌘Q path
    // (closeEvent → saveState; aboutToQuit → saveState again) is
    // harmless. (Preserved from main PR #13 alongside Phase 3I's
    // singleShot auto-reconnect above.)
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        if (m_containerManager) {
            m_containerManager->saveState();
        }
        AppSettings::instance().save();
    });
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

    // Left overlay panel (SpectrumOverlayPanel) — child of spectrum widget
    m_overlayPanel = new SpectrumOverlayPanel(m_spectrumWidget);
    m_overlayPanel->move(4, 4);
    m_overlayPanel->show();

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

    // Create the MeterPoller BEFORE restoreState / populateDefaultMeter
    // so the meterReadyForPolling signal fires into a live poller as
    // each container's MeterWidget is materialized. Previously the
    // poller was created later and only the panel container's
    // m_meterWidget was registered manually — every user-created
    // container's meter sat orphaned and bars never received setValue()
    // calls, the root of the "BarMeter not drawing" symptom.
    m_meterPoller = new MeterPoller(this);
    connect(m_containerManager, &ContainerManager::meterReadyForPolling,
            this, [this](MeterWidget* meter) {
        if (!meter || !m_meterPoller) { return; }
        m_meterPoller->addTarget(meter);
        // Auto-unregister when the meter is destroyed (e.g.
        // ContainerWidget::setContent deleteLater()s a previous
        // content during a swap). Without this the poller would
        // dereference a dangling pointer on its next tick.
        connect(meter, &QObject::destroyed, m_meterPoller,
                [this](QObject* obj) {
            if (m_meterPoller) {
                m_meterPoller->removeTarget(static_cast<MeterWidget*>(obj));
            }
        });
    });

    m_containerManager->restoreState();
    if (m_containerManager->containerCount() == 0) {
        createDefaultContainers();
    }
    // Always populate the panel container's content (meters + applets).
    // On first run, createDefaultContainers() creates the shell; on restore,
    // restoreState() recreates the shell but content is lost. This ensures
    // the applet panel is always populated regardless of restore path.
    populateDefaultMeter();
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

    // Create FFTEngine on a worker thread (spectrum thread from architecture).
    // Sample rate starts at P2 default (768k); RadioModel::wireSampleRateChanged
    // updates it to the actual wire rate on each connect (P1=192k, P2=768k).
    m_fftEngine = new FFTEngine(0);  // receiver 0
    m_fftEngine->setSampleRate(768000.0);
    connect(m_radioModel, &RadioModel::wireSampleRateChanged,
            this, [this](double rateHz) {
        if (m_fftEngine) {
            QMetaObject::invokeMethod(m_fftEngine, [engine = m_fftEngine, rateHz]() {
                engine->setSampleRate(rateHz);
            });
        }
        if (m_spectrumWidget) {
            m_spectrumWidget->setSampleRate(rateHz);
            // Phase 3G-12: preserve the user's current zoom level across
            // sample rate changes. Only reset the visible span if the
            // current bandwidth would now exceed the new DDC sample rate
            // (in which case we clamp to full-span).
            const double freq = m_spectrumWidget->centerFrequency();
            const double currentBw = m_spectrumWidget->bandwidth();
            const double clampedBw = (currentBw > rateHz) ? rateHz : currentBw;
            m_spectrumWidget->setFrequencyRange(freq, clampedBw);
        }
    });
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

    // Phase 3G-8: expose view hooks on RadioModel so Display setup pages can
    // reach the renderer / FFT engine without depending on MainWindow.
    m_radioModel->setSpectrumWidget(m_spectrumWidget);
    m_radioModel->setFftEngine(m_fftEngine);

    // Phase 3G-9b: no first-launch auto-apply of smooth defaults. Per
    // user decision 2026-04-15, the out-of-box waterfall stays on
    // WfColorScheme::Default; ClarityBlue is reachable only via the
    // "Reset to Smooth Defaults" button on SpectrumDefaultsPage or by
    // manually selecting "Clarity Blue" from the Waterfall Defaults combo.

    // --- Phase 3G-13: Step attenuator + ADC overload ---
    m_stepAttController = new StepAttenuatorController(this);
    m_radioModel->setStepAttController(m_stepAttController);

    // --- Phase 3G-9c: Clarity adaptive display tuning ---
    m_clarityController = new ClarityController(this);
    m_radioModel->setClarityController(m_clarityController);

    // Restore enabled state from AppSettings + sync the clarityActive
    // flag on SpectrumWidget so legacy AGC knows to stand down.
    {
        auto& s = AppSettings::instance();
        bool clarityOn = s.value(QStringLiteral("ClarityEnabled"), QStringLiteral("False"))
                            .toString() == QStringLiteral("True");
        m_clarityController->setEnabled(clarityOn);
        m_spectrumWidget->setClarityActive(clarityOn);
    }

    // Feed FFT bins to Clarity (auto-queued: spectrum thread → main)
    connect(m_fftEngine, &FFTEngine::fftReady,
            m_clarityController, [this](int /*rxId*/, const QVector<float>& binsDbm) {
        m_clarityController->feedBins(binsDbm);
    });

    // TX pause: MOX signal → ClarityController
    connect(&m_radioModel->transmitModel(), &TransmitModel::moxChanged,
            m_clarityController, &ClarityController::setTransmitting);

    // Clarity → SpectrumWidget threshold update + clarityActive flag
    connect(m_clarityController, &ClarityController::waterfallThresholdsChanged,
            m_spectrumWidget, [this](float low, float high) {
        m_spectrumWidget->setClarityActive(true);
        m_spectrumWidget->setWfLowThreshold(low);
        m_spectrumWidget->setWfHighThreshold(high);
    });

    // When Clarity pauses or is disabled, let legacy AGC resume.
    connect(m_clarityController, &ClarityController::pausedChanged,
            m_spectrumWidget, [this](bool paused) {
        if (paused) {
            m_spectrumWidget->setClarityActive(false);
        }
    });

    // Clarity ↔ overlay panel: badge + Re-tune button (wired after
    // m_overlayPanel creation in buildUI, which runs before this point).
    if (m_overlayPanel) {
        connect(m_clarityController, &ClarityController::waterfallThresholdsChanged,
                m_overlayPanel, [this](float, float) {
            m_overlayPanel->setClarityStatus(/*active=*/true, /*paused=*/false);
        });
        connect(m_clarityController, &ClarityController::pausedChanged,
                m_overlayPanel, [this](bool paused) {
            bool enabled = m_clarityController->isEnabled();
            m_overlayPanel->setClarityStatus(enabled, paused);
        });
        connect(m_overlayPanel, &SpectrumOverlayPanel::clarityRetuneRequested,
                m_clarityController, &ClarityController::retuneNow);
    }

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
    // Poller was created earlier so the meterReadyForPolling signal
    // could catch container restore + populateDefaultMeter emits.
    // m_meterWidget is registered automatically via that signal —
    // the call here is a defensive belt only and dedupes inside
    // MeterPoller::addTarget.
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

void MainWindow::rebuildEditContainerSubmenu()
{
    if (!m_editContainerMenu) { return; }
    m_editContainerMenu->clear();

    if (!m_containerManager) {
        auto* none = m_editContainerMenu->addAction(
            QStringLiteral("(no containers)"));
        none->setEnabled(false);
        return;
    }

    const QList<ContainerWidget*> all = m_containerManager->allContainers();
    if (all.isEmpty()) {
        auto* none = m_editContainerMenu->addAction(
            QStringLiteral("(no containers)"));
        none->setEnabled(false);
        return;
    }

    // Alphabetical by title so the menu order is predictable even
    // when containers are created in different orders.
    QList<ContainerWidget*> sorted = all;
    std::sort(sorted.begin(), sorted.end(),
              [](ContainerWidget* a, ContainerWidget* b) {
        const QString na = a->notes().isEmpty()
            ? a->id().left(8) : a->notes();
        const QString nb = b->notes().isEmpty()
            ? b->id().left(8) : b->notes();
        return na.localeAwareCompare(nb) < 0;
    });

    for (ContainerWidget* c : sorted) {
        const QString label = c->notes().isEmpty()
            ? (QStringLiteral("(unnamed) ") + c->id().left(8))
            : c->notes();
        QAction* act = m_editContainerMenu->addAction(label);
        const QString id = c->id();
        connect(act, &QAction::triggered, this, [this, id]() {
            if (!m_containerManager) { return; }
            ContainerWidget* target = m_containerManager->container(id);
            if (!target) { return; }
            ContainerSettingsDialog dialog(target, this, m_containerManager);
            dialog.exec();
        });
    }
}

void MainWindow::resetDefaultLayout()
{
    if (!m_containerManager) { return; }

    // Destroy every non-panel container. Collect IDs first because
    // destroyContainer mutates the underlying map.
    const QList<ContainerWidget*> all = m_containerManager->allContainers();
    ContainerWidget* panel = m_containerManager->panelContainer();
    QStringList toRemove;
    for (ContainerWidget* c : all) {
        if (c == panel) { continue; }
        toRemove.append(c->id());
    }
    for (const QString& id : toRemove) {
        m_containerManager->destroyContainer(id);
    }

    // Also wipe the panel container's MeterWidget items and rebuild
    // from factories. Before this, a persisted item payload (e.g. a
    // bar-style S-Meter saved by an earlier build) would survive
    // Reset because only the non-panel containers were destroyed —
    // Container #0's MeterWidget was left untouched, so the stale
    // items reloaded every launch and Reset felt like a no-op for
    // the main meter column.
    if (m_meterWidget) {
        m_meterWidget->clearItems();

        ItemGroup* smeter = ItemGroup::createSMeterPreset(
            MeterBinding::SignalAvg, QStringLiteral("S-Meter"), m_meterWidget);
        smeter->installInto(m_meterWidget, 0.0f, 0.0f, 1.0f, 0.45f);
        delete smeter;

        ItemGroup* pwrSwr = ItemGroup::createPowerSwrPreset(
            QStringLiteral("Power/SWR"), m_meterWidget);
        pwrSwr->installInto(m_meterWidget, 0.0f, 0.45f, 1.0f, 0.40f);
        delete pwrSwr;

        ItemGroup* alc = ItemGroup::createAlcPreset(m_meterWidget);
        alc->installInto(m_meterWidget, 0.0f, 0.85f, 1.0f, 0.15f);
        delete alc;
    }

    rebuildEditContainerSubmenu();
    qCInfo(lcContainer) << "Reset default layout: removed"
                         << toRemove.size() << "non-panel containers"
                         << "and rebuilt panel meter defaults";
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

    // Guard: don't repopulate if real content (AppletPanelWidget) already exists.
    // The container constructor creates a placeholder QLabel("Container") which
    // we need to replace. Check if content is already an AppletPanelWidget.
    if (qobject_cast<AppletPanelWidget*>(c0->content()) != nullptr) {
        return;
    }

    // ContainerManager::restoreState may have set a MeterWidget with
    // user-saved items as the panel container's content. Capture that
    // payload before we overwrite c0's content. We can't reuse the
    // pointer directly because ContainerWidget::setContent() calls
    // deleteLater on the previous m_content; round-tripping through
    // serialize/deserialize transfers the items into a fresh
    // m_meterWidget that becomes the AppletPanelWidget header.
    QString restoredItems;
    if (auto* existingMeter = qobject_cast<MeterWidget*>(c0->content())) {
        if (!existingMeter->items().isEmpty()) {
            restoredItems = existingMeter->serializeItems();
        }
    }

    m_meterWidget = new MeterWidget();

    if (!restoredItems.isEmpty()) {
        m_meterWidget->deserializeItems(restoredItems);
        // Rebuild the runtime stack metadata from geometry so bar
        // rows restored from a saved container participate in the
        // reflow-on-resize path again. No-op for panels that don't
        // contain a stack.
        m_meterWidget->inferStackFromGeometry();
        qCDebug(lcContainer) << "Restored" << m_meterWidget->items().size()
                             << "panel meter items from saved state";
    } else {
        // S-Meter: top 45% — arc needle bound to SignalAvg
        // From Thetis MeterManager.cs: ANAN needle uses AVG_SIGNAL_STRENGTH
        ItemGroup* smeter = ItemGroup::createSMeterPreset(
            MeterBinding::SignalAvg, QStringLiteral("S-Meter"), m_meterWidget);
        smeter->installInto(m_meterWidget, 0.0f, 0.0f, 1.0f, 0.45f);
        delete smeter;

        // Power/SWR: middle 40% — stacked bars (stub TX bindings)
        ItemGroup* pwrSwr = ItemGroup::createPowerSwrPreset(
            QStringLiteral("Power/SWR"), m_meterWidget);
        pwrSwr->installInto(m_meterWidget, 0.0f, 0.45f, 1.0f, 0.40f);
        delete pwrSwr;

        // ALC: bottom 15% — compact single-line bar (stub TX binding)
        ItemGroup* alc = ItemGroup::createAlcPreset(m_meterWidget);
        alc->installInto(m_meterWidget, 0.0f, 0.85f, 1.0f, 0.15f);
        delete alc;
    }

    // Build an AppletPanelWidget: MeterWidget on top, then all applets below.
    // This is a single scrollable content widget per the v2 plan.
    m_appletPanel = new AppletPanelWidget();
    auto* panel = m_appletPanel;
    // Fixed header: MeterWidget stays visible, never scrolls.
    // Height scales dynamically with width. 1.3:1 gives the S-Meter arc
    // enough vertical room to not look squished.
    panel->setHeaderWidget(m_meterWidget, QStringLiteral("Meters"), 1.3f);

    // RxApplet — Tier 1 wired to SliceModel (slice attached in wireSliceToSpectrum)
    m_rxApplet = new RxApplet(nullptr, m_radioModel, nullptr);
    panel->addApplet(m_rxApplet);

    // TxApplet — NYI shell (Phase 3I-1)
    auto* txApplet = new TxApplet(m_radioModel, nullptr);
    panel->addApplet(txApplet);

    // PhoneCwApplet — Phone + CW pages, NYI
    m_phoneCwApplet = new PhoneCwApplet(m_radioModel, nullptr);
    panel->addApplet(m_phoneCwApplet);

    // EqApplet — 10-band EQ, NYI (Phase 3I-3)
    m_eqApplet = new EqApplet(m_radioModel, nullptr);
    panel->addApplet(m_eqApplet);

    // Tasks 7-10: NYI applets created but NOT added to the container.
    // Task 15 (final assembly) will wire these via the Containers menu.
    m_digitalApplet    = new DigitalApplet(m_radioModel, nullptr);
    m_pureSignalApplet = new PureSignalApplet(m_radioModel, nullptr);
    m_diversityApplet  = new DiversityApplet(m_radioModel, nullptr);
    m_cwxApplet        = new CwxApplet(m_radioModel, nullptr);
    m_dvkApplet        = new DvkApplet(m_radioModel, nullptr);
    m_catApplet        = new CatApplet(m_radioModel, nullptr);
    m_tunerApplet      = new TunerApplet(m_radioModel, nullptr);

    c0->setContent(panel);
    qCDebug(lcMeter) << "Installed default meter layout: S-Meter + Power/SWR + ALC";
    qCDebug(lcContainer) << "Container #0: Meters + RxApplet + TxApplet + PhoneCwApplet + EqApplet (10-band)";
}

void MainWindow::buildMenuBar()
{
    // =========================================================================
    // FILE
    // =========================================================================
    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));

    {
        QAction* settingsAction = fileMenu->addAction(QStringLiteral("&Settings..."),
            this, [this]() {
                auto* dialog = new SetupDialog(m_radioModel, this);
                dialog->setAttribute(Qt::WA_DeleteOnClose);
                dialog->show();
            });
        settingsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
        settingsAction->setMenuRole(QAction::NoRole);  // Keep in File menu, don't let macOS move it
        settingsAction->setToolTip(QStringLiteral("Open application settings"));
    }

    {
        QMenu* profilesMenu = fileMenu->addMenu(QStringLiteral("&Profiles"));
        QAction* txProfilesAction = profilesMenu->addAction(QStringLiteral("&TX Profiles..."));
        txProfilesAction->setEnabled(false);
        txProfilesAction->setToolTip(QStringLiteral("NYI — Phase X"));
        QAction* micProfilesAction = profilesMenu->addAction(QStringLiteral("&Mic Profiles..."));
        micProfilesAction->setEnabled(false);
        micProfilesAction->setToolTip(QStringLiteral("NYI — Phase X"));
        profilesMenu->addSeparator();
        QAction* importAction = profilesMenu->addAction(QStringLiteral("&Import..."));
        importAction->setEnabled(false);
        importAction->setToolTip(QStringLiteral("NYI — Phase X"));
        QAction* exportAction = profilesMenu->addAction(QStringLiteral("&Export..."));
        exportAction->setEnabled(false);
        exportAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    fileMenu->addSeparator();

    fileMenu->addAction(QStringLiteral("&Quit"), QKeySequence(Qt::CTRL | Qt::Key_Q),
                        qApp, &QApplication::quit);

    // =========================================================================
    // RADIO
    // =========================================================================
    QMenu* radioMenu = menuBar()->addMenu(QStringLiteral("&Radio"));

    radioMenu->addAction(QStringLiteral("&Discover..."), QKeySequence(Qt::CTRL | Qt::Key_K),
                         this, &MainWindow::showConnectionPanel);

    {
        QAction* connectAction = radioMenu->addAction(QStringLiteral("&Connect"),
            this, [this]() {
                // Connect uses last known radio — opens panel if not connected
                if (!m_radioModel->isConnected()) {
                    showConnectionPanel();
                }
            });
        connectAction->setToolTip(QStringLiteral("Connect to last radio"));
    }

    radioMenu->addAction(QStringLiteral("&Disconnect"), this, [this]() {
        m_radioModel->disconnectFromRadio();
    });

    radioMenu->addSeparator();

    radioMenu->addAction(QStringLiteral("&Radio Setup..."), this, [this]() {
        if (!m_radioModel->isConnected()) {
            showConnectionPanel();
            return;
        }
        showConnectionPanel();
    });
    {
        QAction* antennaSetupAction = radioMenu->addAction(QStringLiteral("&Antenna Setup..."));
        antennaSetupAction->setEnabled(false);
        antennaSetupAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* transvertersAction = radioMenu->addAction(QStringLiteral("Trans&verters..."));
        transvertersAction->setEnabled(false);
        transvertersAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

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
        QActionGroup* layoutGroup = new QActionGroup(this);
        layoutGroup->setExclusive(true);
        const struct { const char* label; } layouts[] = {
            { "&Single" }, { "2 &Vertical" }, { "2 &Horizontal" },
            { "2×&2" }, { "1+2 &Horizontal" }
        };
        bool first = true;
        for (const auto& l : layouts) {
            QAction* a = panLayoutMenu->addAction(QString::fromUtf8(l.label));
            a->setCheckable(true);
            a->setEnabled(false);
            a->setToolTip(QStringLiteral("NYI — Phase 3F"));
            if (first) { a->setChecked(true); first = false; }
            layoutGroup->addAction(a);
        }
    }

    {
        QAction* addPanAction = viewMenu->addAction(QStringLiteral("&Add Panadapter"));
        addPanAction->setEnabled(false);
        addPanAction->setToolTip(QStringLiteral("NYI — Phase 3F"));
    }
    {
        QAction* rmPanAction = viewMenu->addAction(QStringLiteral("&Remove Panadapter"));
        rmPanAction->setEnabled(false);
        rmPanAction->setToolTip(QStringLiteral("NYI — Phase 3F"));
    }

    viewMenu->addSeparator();

    {
        QMenu* bandPlanMenu = viewMenu->addMenu(QStringLiteral("&Band Plan"));
        QActionGroup* bpGroup = new QActionGroup(this);
        bpGroup->setExclusive(true);
        const struct { const char* label; } bpModes[] = {
            { "&Off" }, { "&Small" }, { "&Medium" }, { "&Large" }
        };
        bool first = true;
        for (const auto& m : bpModes) {
            QAction* a = bandPlanMenu->addAction(QString::fromUtf8(m.label));
            a->setCheckable(true);
            a->setEnabled(false);
            a->setToolTip(QStringLiteral("NYI — Phase X"));
            if (first) { a->setChecked(true); first = false; }
            bpGroup->addAction(a);
        }
        bandPlanMenu->addSeparator();
        QMenu* regionMenu = bandPlanMenu->addMenu(QStringLiteral("&Region"));
        QActionGroup* regionGroup = new QActionGroup(this);
        regionGroup->setExclusive(true);
        const struct { const char* label; } regions[] = {
            { "&US" }, { "&EU" }, { "&JA" }, { "&Other" }
        };
        bool firstRegion = true;
        for (const auto& r : regions) {
            QAction* a = regionMenu->addAction(QString::fromUtf8(r.label));
            a->setCheckable(true);
            a->setEnabled(false);
            a->setToolTip(QStringLiteral("NYI — Phase X"));
            if (firstRegion) { a->setChecked(true); firstRegion = false; }
            regionGroup->addAction(a);
        }
    }

    {
        QMenu* displayModeMenu = viewMenu->addMenu(QStringLiteral("&Display Mode"));
        QAction* placeholder = displayModeMenu->addAction(QStringLiteral("(NYI placeholder)"));
        placeholder->setEnabled(false);
        placeholder->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    {
        QMenu* uiScaleMenu = viewMenu->addMenu(QStringLiteral("&UI Scale"));
        QActionGroup* scaleGroup = new QActionGroup(this);
        scaleGroup->setExclusive(true);
        const struct { const char* label; bool isDefault; } scales[] = {
            { "&75%",  false },
            { "&100%", true  },
            { "&125%", false },
            { "&150%", false },
            { "&175%", false },
            { "&200%", false },
        };
        for (const auto& s : scales) {
            QAction* a = uiScaleMenu->addAction(QString::fromUtf8(s.label));
            a->setCheckable(true);
            a->setEnabled(false);
            a->setToolTip(QStringLiteral("NYI — Phase X"));
            if (s.isDefault) { a->setChecked(true); }
            scaleGroup->addAction(a);
        }
    }

    viewMenu->addSeparator();

    m_darkThemeAction = viewMenu->addAction(QStringLiteral("&Dark Theme"));
    m_darkThemeAction->setCheckable(true);
    m_darkThemeAction->setChecked(true);
    m_darkThemeAction->setToolTip(QStringLiteral("Toggle dark theme (NYI — Phase X)"));
    connect(m_darkThemeAction, &QAction::toggled, this, [](bool /*on*/) {
        qCDebug(lcConnection) << "Dark Theme toggle NYI";
    });

    {
        QAction* minimalAction = viewMenu->addAction(QStringLiteral("&Minimal Mode"));
        minimalAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
        minimalAction->setEnabled(false);
        minimalAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    viewMenu->addSeparator();

    {
        QAction* kbAction = viewMenu->addAction(QStringLiteral("&Keyboard Shortcuts..."));
        kbAction->setEnabled(false);
        kbAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    // =========================================================================
    // DSP
    // =========================================================================
    QMenu* dspMenu = menuBar()->addMenu(QStringLiteral("&DSP"));

    // Checkable DSP toggles — stored as members so the overlay panel can sync
    m_nrAction = dspMenu->addAction(QStringLiteral("&NR"));
    m_nrAction->setCheckable(true);
    connect(m_nrAction, &QAction::toggled, this, [this](bool on) {
        RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
        if (rxCh) { rxCh->setNrEnabled(on); }
    });

    QAction* nr2Action = dspMenu->addAction(QStringLiteral("NR&2"));
    nr2Action->setCheckable(true);
    nr2Action->setToolTip(QStringLiteral("Toggle enhanced multiband noise reduction (NR2/EMNR)"));
    connect(nr2Action, &QAction::toggled, this, [this](bool on) {
        SliceModel* slice = m_radioModel->activeSlice();
        if (slice) { slice->setEmnrEnabled(on); }
    });

    m_nbAction = dspMenu->addAction(QStringLiteral("N&B"));
    m_nbAction->setCheckable(true);
    connect(m_nbAction, &QAction::toggled, this, [this](bool on) {
        RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
        if (rxCh) { rxCh->setNb1Enabled(on); }
    });

    QAction* nb2Action = dspMenu->addAction(QStringLiteral("NB&2"));
    nb2Action->setCheckable(true);
    nb2Action->setEnabled(false);
    nb2Action->setToolTip(QStringLiteral("NYI — Phase X"));

    m_anfAction = dspMenu->addAction(QStringLiteral("&ANF"));
    m_anfAction->setCheckable(true);
    connect(m_anfAction, &QAction::toggled, this, [this](bool on) {
        RxChannel* rxCh = m_radioModel->wdspEngine()->rxChannel(0);
        if (rxCh) { rxCh->setAnfEnabled(on); }
    });

    QAction* tnfAction = dspMenu->addAction(QStringLiteral("&TNF"));
    tnfAction->setCheckable(true);
    tnfAction->setEnabled(false);
    tnfAction->setToolTip(QStringLiteral("NYI — Phase X"));

    QAction* binAction = dspMenu->addAction(QStringLiteral("&BIN"));
    binAction->setCheckable(true);
    binAction->setEnabled(false);
    binAction->setToolTip(QStringLiteral("NYI — Phase X"));

    dspMenu->addSeparator();

    {
        // AGC submenu — checkable exclusive via QActionGroup.
        // AGCMode enum from WdspTypes.h: Off=0, Long=1, Slow=2, Med=3, Fast=4, Custom=5
        // From Thetis dsp.cs AGCMode — all 6 modes wired to SliceModel::setAgcMode().
        QMenu* agcMenu = dspMenu->addMenu(QStringLiteral("&AGC"));
        m_agcGroup = new QActionGroup(this);
        m_agcGroup->setExclusive(true);

        const struct { const char* label; AGCMode mode; } agcModes[] = {
            { "&Off",    AGCMode::Off    },
            { "&Long",   AGCMode::Long   },
            { "&Slow",   AGCMode::Slow   },
            { "&Med",    AGCMode::Med    },
            { "&Fast",   AGCMode::Fast   },
            { "&Custom", AGCMode::Custom },
        };
        for (const auto& agc : agcModes) {
            AGCMode agcMode = agc.mode;
            QAction* a = agcMenu->addAction(QString::fromUtf8(agc.label),
                this, [this, agcMode]() {
                    SliceModel* slice = m_radioModel->activeSlice();
                    if (slice) { slice->setAgcMode(agcMode); }
                });
            a->setCheckable(true);
            m_agcGroup->addAction(a);
        }

        // Sync AGC checked state when SliceModel changes
        connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int index) {
            if (index != 0) { return; }
            SliceModel* slice = m_radioModel->activeSlice();
            if (!slice) { return; }
            connect(slice, &SliceModel::agcModeChanged, this, [this](AGCMode mode) {
                QList<QAction*> acts = m_agcGroup->actions();
                const AGCMode agcOrder[] = {
                    AGCMode::Off, AGCMode::Long, AGCMode::Slow,
                    AGCMode::Med, AGCMode::Fast, AGCMode::Custom
                };
                for (int i = 0; i < acts.size() && i < 6; ++i) {
                    acts[i]->setChecked(agcOrder[i] == mode);
                }
            });
        });
    }

    dspMenu->addSeparator();

    {
        QAction* eqAction = dspMenu->addAction(QStringLiteral("&Equalizer..."));
        eqAction->setEnabled(false);
        eqAction->setToolTip(QStringLiteral("NYI — Phase 3I-3"));
    }
    {
        QAction* psAction = dspMenu->addAction(QStringLiteral("&PureSignal..."));
        psAction->setEnabled(false);
        psAction->setToolTip(QStringLiteral("NYI — Phase 3I-4"));
    }
    {
        QAction* divAction = dspMenu->addAction(QStringLiteral("&Diversity..."));
        divAction->setEnabled(false);
        divAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    // =========================================================================
    // BAND
    // =========================================================================
    QMenu* bandMenu = menuBar()->addMenu(QStringLiteral("&Band"));

    {
        QMenu* hfMenu = bandMenu->addMenu(QStringLiteral("&HF"));
        // Frequency values from Thetis console.cs band definitions
        const struct { const char* label; double freqHz; } hfBands[] = {
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
                if (slice) { slice->setFrequency(freq); }
            });
        }
    }

    {
        QMenu* vhfMenu = bandMenu->addMenu(QStringLiteral("&VHF"));
        QAction* placeholder = vhfMenu->addAction(QStringLiteral("(NYI — Phase X)"));
        placeholder->setEnabled(false);
        placeholder->setToolTip(QStringLiteral("VHF bands NYI — Phase X"));
    }

    {
        QMenu* genMenu = bandMenu->addMenu(QStringLiteral("&GEN"));
        QAction* placeholder = genMenu->addAction(QStringLiteral("(NYI — Phase X)"));
        placeholder->setEnabled(false);
        placeholder->setToolTip(QStringLiteral("GEN coverage NYI — Phase X"));
    }

    bandMenu->addAction(QStringLiteral("&WWV (10.0 MHz)"), this, [this]() {
        SliceModel* slice = m_radioModel->activeSlice();
        if (slice) { slice->setFrequency(10.0e6); }
    });

    bandMenu->addSeparator();

    {
        QAction* bandStackAction = bandMenu->addAction(QStringLiteral("Band &Stacking..."));
        bandStackAction->setEnabled(false);
        bandStackAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    // =========================================================================
    // MODE
    // =========================================================================
    QMenu* modeMenu = menuBar()->addMenu(QStringLiteral("&Mode"));

    // 12 modes in display order (spec order): LSB, USB, DSB, CWL, CWU, AM,
    // SAM, FM, DIGL, DIGU, DRM, SPEC.
    // Maps to DSPMode enum values from WdspTypes.h.
    // From Thetis dsp.cs DSPMode enum — enum values used directly, not indices.
    const struct { const char* label; DSPMode mode; } modes[] = {
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
            if (slice) { slice->setDspMode(mode); }
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
            const DSPMode displayOrder[] = {
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

    {
        // New Container: creates a floating container with a fresh MeterWidget,
        // then opens the settings dialog so the user can pick a preset or add items.
        // From Thetis setup.cs:24358 — btnAddRX1Container_Click → AddMeterContainer(1, false)
        QAction* newContAction = containersMenu->addAction(QStringLiteral("&New Container..."));
        connect(newContAction, &QAction::triggered, this, [this]() {
            if (!m_containerManager) { return; }

            ContainerWidget* c = m_containerManager->createContainer(1, DockMode::Floating);
            c->setNotes(QStringLiteral("Meter"));

            // Give it a MeterWidget as content (replaces the default placeholder label)
            MeterWidget* meter = new MeterWidget();
            c->setContent(meter);

            // Open settings dialog so user can configure it
            ContainerSettingsDialog dialog(c, this, m_containerManager);
            if (dialog.exec() == QDialog::Rejected) {
                // User cancelled — destroy the container
                m_containerManager->destroyContainer(c->id());
            }
        });
    }
    {
        // Phase 3G-6 block 6 commit 45: dynamic "Edit Container ▸"
        // submenu populated from ContainerManager::allContainers().
        // Replaces the old static "Container Settings..." action that
        // could only edit Container #0. Rebuilds on
        // containerAdded / containerRemoved / containerTitleChanged
        // signals so menu entries stay in sync with the live
        // container set.
        m_editContainerMenu = containersMenu->addMenu(
            QStringLiteral("&Edit Container"));
        rebuildEditContainerSubmenu();
        if (m_containerManager) {
            connect(m_containerManager, &ContainerManager::containerAdded,
                    this, [this](const QString&) { rebuildEditContainerSubmenu(); });
            connect(m_containerManager, &ContainerManager::containerRemoved,
                    this, [this](const QString&) { rebuildEditContainerSubmenu(); });
            connect(m_containerManager, &ContainerManager::containerTitleChanged,
                    this, [this](const QString&, const QString&) {
                rebuildEditContainerSubmenu();
            });
        }
    }
    {
        // Phase 3G-6 block 6 commit 46: Reset Default Layout — now
        // functional. Destroys every non-panel container and
        // rebuilds the submenu.
        QAction* resetAction = containersMenu->addAction(QStringLiteral("&Reset Default Layout"));
        connect(resetAction, &QAction::triggered, this,
                &MainWindow::resetDefaultLayout);
    }

    containersMenu->addSeparator();

    // Dynamic show/hide toggles for the 7 optional applets in Container #0.
    // Checked = visible in panel; unchecked = hidden/removed.
    // All 7 are hidden by default; user enables as needed.
    auto addContainerToggle = [&](const QString& name, AppletWidget* applet, bool defaultVisible) {
        auto* action = containersMenu->addAction(name);
        action->setCheckable(true);
        action->setChecked(defaultVisible);
        connect(action, &QAction::toggled, this, [this, applet](bool show) {
            if (!m_appletPanel) { return; }
            if (show) {
                m_appletPanel->addApplet(applet);
            } else {
                m_appletPanel->removeApplet(applet);
            }
        });
    };

    addContainerToggle(QStringLiteral("Digital / VAC"), m_digitalApplet,    false);
    addContainerToggle(QStringLiteral("PureSignal"),    m_pureSignalApplet, false);
    addContainerToggle(QStringLiteral("Diversity"),     m_diversityApplet,  false);
    addContainerToggle(QStringLiteral("CW Keyer"),      m_cwxApplet,        false);
    addContainerToggle(QStringLiteral("Voice Keyer"),   m_dvkApplet,        false);
    addContainerToggle(QStringLiteral("CAT / TCI"),     m_catApplet,        false);
    addContainerToggle(QStringLiteral("ATU Control"),   m_tunerApplet,      false);

    // =========================================================================
    // TOOLS
    // =========================================================================
    QMenu* toolsMenu = menuBar()->addMenu(QStringLiteral("&Tools"));

    {
        QAction* cwxAction = toolsMenu->addAction(QStringLiteral("C&WX..."));
        cwxAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_X));
        cwxAction->setEnabled(false);
        cwxAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* memAction = toolsMenu->addAction(QStringLiteral("&Memory Manager..."));
        memAction->setEnabled(false);
        memAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* catAction = toolsMenu->addAction(QStringLiteral("&CAT Control..."));
        catAction->setEnabled(false);
        catAction->setToolTip(QStringLiteral("NYI — Phase 3K"));
    }
    {
        QAction* tciAction = toolsMenu->addAction(QStringLiteral("&TCI Server..."));
        tciAction->setEnabled(false);
        tciAction->setToolTip(QStringLiteral("NYI — Phase 3J"));
    }
    {
        QAction* daxAction = toolsMenu->addAction(QStringLiteral("&DAX Audio..."));
        daxAction->setEnabled(false);
        daxAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* midiAction = toolsMenu->addAction(QStringLiteral("&MIDI Mapping..."));
        midiAction->setEnabled(false);
        midiAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* macroAction = toolsMenu->addAction(QStringLiteral("Macro &Buttons..."));
        macroAction->setEnabled(false);
        macroAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    toolsMenu->addSeparator();

    {
        QAction* netDiagAction = toolsMenu->addAction(QStringLiteral("&Network Diagnostics..."));
        netDiagAction->setEnabled(false);
        netDiagAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    toolsMenu->addAction(QStringLiteral("&Support Bundle..."), this,
                         &MainWindow::showSupportDialog);

    // =========================================================================
    // HELP
    // =========================================================================
    QMenu* helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));

    {
        QAction* gettingStartedAction = helpMenu->addAction(QStringLiteral("&Getting Started"));
        gettingStartedAction->setEnabled(false);
        gettingStartedAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* helpAction = helpMenu->addAction(QStringLiteral("&NereusSDR Help"));
        helpAction->setEnabled(false);
        helpAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* dataModesAction = helpMenu->addAction(QStringLiteral("Understanding &Data Modes"));
        dataModesAction->setEnabled(false);
        dataModesAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    helpMenu->addSeparator();

    {
        QAction* whatsNewAction = helpMenu->addAction(QStringLiteral("What's &New"));
        whatsNewAction->setEnabled(false);
        whatsNewAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    helpMenu->addSeparator();

    helpMenu->addAction(QStringLiteral("&About NereusSDR"), this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
    });

    // =========================================================================
    // 💡 ISSUE REPORTER — toolbar button (Phase 3G-14)
    // Ported from AetherSDR TitleBar::showFeatureRequestDialog()
    // Uses a QToolBar so the button is visible on macOS (native menu bar)
    // and on Linux/Windows (in-window menu bar) alike.
    // =========================================================================
    auto* featureBar = new QToolBar(this);
    featureBar->setMovable(false);
    featureBar->setFloatable(false);
    featureBar->setIconSize(QSize(28, 28));
    featureBar->setFixedHeight(32);
    featureBar->setStyleSheet(QStringLiteral(
        "QToolBar { background: #0f0f1a; border-bottom: 1px solid #203040; spacing: 0; padding: 0 4px; }"));

    auto* spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    featureBar->addWidget(spacer);

    // Paint a lightbulb icon so it renders cleanly at any DPI
    auto makeBulbIcon = [](QColor bulbColor, QColor baseColor) -> QIcon {
        constexpr int sz = 64;  // paint large, Qt scales down
        QPixmap pm(sz, sz);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);

        // Bulb (circle)
        p.setPen(Qt::NoPen);
        p.setBrush(bulbColor);
        p.drawEllipse(QRectF(14, 4, 36, 36));

        // Neck (trapezoid connecting bulb to base)
        QPolygonF neck;
        neck << QPointF(22, 36) << QPointF(42, 36)
             << QPointF(40, 44) << QPointF(24, 44);
        p.drawPolygon(neck);

        // Base (screw threads — 3 thin lines)
        p.setPen(QPen(baseColor, 2.5));
        p.drawLine(QPointF(24, 46), QPointF(40, 46));
        p.drawLine(QPointF(25, 50), QPointF(39, 50));
        p.drawLine(QPointF(27, 54), QPointF(37, 54));

        // Tip
        p.setPen(Qt::NoPen);
        p.setBrush(baseColor);
        p.drawEllipse(QRectF(29, 56, 6, 4));

        // Filament lines inside bulb
        p.setPen(QPen(baseColor, 1.5));
        p.drawLine(QPointF(28, 34), QPointF(28, 22));
        p.drawLine(QPointF(28, 22), QPointF(32, 16));
        p.drawLine(QPointF(32, 16), QPointF(36, 22));
        p.drawLine(QPointF(36, 22), QPointF(36, 34));

        p.end();
        return QIcon(pm);
    };

    QIcon bulbIcon = makeBulbIcon(QColor(0xFF, 0xD0, 0x60), QColor(0x80, 0x60, 0x20));

    m_featureBtn = new QPushButton;
    m_featureBtn->setIcon(bulbIcon);
    m_featureBtn->setIconSize(QSize(22, 22));
    m_featureBtn->setFixedSize(28, 28);
    m_featureBtn->setToolTip(QStringLiteral("Submit a feature request or bug report"));
    m_featureBtn->setAccessibleName(QStringLiteral("Feature request"));
    m_featureBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #3a2a00; border: 1px solid #806020; "
        "border-radius: 4px; padding: 0; }"
        "QPushButton:hover { background: #504000; border-color: #a08030; }"));
    connect(m_featureBtn, &QPushButton::clicked,
            this, &MainWindow::showFeatureRequestDialog);
    featureBar->addWidget(m_featureBtn);

    addToolBar(Qt::TopToolBarArea, featureBar);
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

    // Helper: styled separator dot
    auto makeSep = [&]() -> QLabel* {
        auto* sep = new QLabel(QStringLiteral(" · "), barWidget);
        sep->setStyleSheet(QStringLiteral("QLabel { color: #304050; font-size: 18px; }"));
        return sep;
    };

    // ── Left section ──────────────────────────────────────────────────────────

    // Band Stack: three grey circles (NYI — clickable placeholder)
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
    bandStackLabel->setCursor(Qt::PointingHandCursor);
    hbox->addWidget(bandStackLabel);

    // +PAN icon (NYI)
    auto* panLabel = new QLabel(QStringLiteral("+PAN"), barWidget);
    panLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #404858; font-weight: bold; font-size: 11px; }"));
    panLabel->setToolTip(QStringLiteral("+PAN (NYI — Phase 3F)"));
    panLabel->setCursor(Qt::PointingHandCursor);
    hbox->addWidget(panLabel);

    // Panel toggle (☰) — wired to QSplitter right pane visibility
    auto* panelToggleLabel = new QLabel(QStringLiteral("☰"), barWidget);
    panelToggleLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #8aa8c0; font-weight: bold; font-size: 16px; }"));
    panelToggleLabel->setToolTip(QStringLiteral("Toggle container panel"));
    panelToggleLabel->setCursor(Qt::PointingHandCursor);
    hbox->addWidget(panelToggleLabel);

    // Wire ☰ click: toggle QSplitter right pane (widget index 1) visibility.
    // When hiding: save sizes so we can restore them. When showing: restore.
    connect(panelToggleLabel, &QLabel::linkActivated, this, [](const QString&){});
    // QLabel doesn't emit click directly — install event filter via lambda via
    // a helper QObject. Use mousePressEvent via event filter on the label.
    panelToggleLabel->installEventFilter(this);
    // We need to store the label pointer to recognise it in eventFilter.
    // Use a property to mark it.
    panelToggleLabel->setProperty("isPanelToggle", true);

    // TNF toggle
    m_tnfLabel = new QLabel(QStringLiteral("TNF"), barWidget);
    m_tnfLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #404858; font-weight: bold; font-size: 11px; }"));
    m_tnfLabel->setToolTip(QStringLiteral("Tracking Notch Filter (NYI)"));
    m_tnfLabel->setCursor(Qt::PointingHandCursor);
    hbox->addWidget(m_tnfLabel);

    // CWX
    auto* cwxLabel = new QLabel(QStringLiteral("CWX"), barWidget);
    cwxLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #404858; font-weight: bold; font-size: 11px; }"));
    cwxLabel->setToolTip(QStringLiteral("CW Keyer (NYI)"));
    cwxLabel->setCursor(Qt::PointingHandCursor);
    hbox->addWidget(cwxLabel);

    // DVK
    auto* dvkLabel = new QLabel(QStringLiteral("DVK"), barWidget);
    dvkLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #404858; font-weight: bold; font-size: 11px; }"));
    dvkLabel->setToolTip(QStringLiteral("Digital Voice Keyer (NYI)"));
    dvkLabel->setCursor(Qt::PointingHandCursor);
    hbox->addWidget(dvkLabel);

    // FDX
    auto* fdxLabel = new QLabel(QStringLiteral("FDX"), barWidget);
    fdxLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #404858; font-weight: bold; font-size: 11px; }"));
    fdxLabel->setToolTip(QStringLiteral("Full Duplex (NYI)"));
    fdxLabel->setCursor(Qt::PointingHandCursor);
    hbox->addWidget(fdxLabel);

    hbox->addWidget(makeSep());

    // Radio info: stacked model + firmware labels
    auto* radioInfoWidget = new QWidget(barWidget);
    QVBoxLayout* radioVbox = new QVBoxLayout(radioInfoWidget);
    radioVbox->setContentsMargins(0, 0, 0, 0);
    radioVbox->setSpacing(0);

    m_radioModelLabel = new QLabel(QStringLiteral("—"), radioInfoWidget);
    m_radioModelLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #8aa8c0; font-size: 11px; }"));
    radioVbox->addWidget(m_radioModelLabel);

    m_radioFwLabel = new QLabel(QStringLiteral("—"), radioInfoWidget);
    m_radioFwLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-size: 11px; }"));
    radioVbox->addWidget(m_radioFwLabel);

    // Keep m_connStatusLabel pointing at model label (legacy compat)
    m_connStatusLabel = m_radioModelLabel;

    hbox->addWidget(radioInfoWidget);

    // ── Stretch ───────────────────────────────────────────────────────────────
    hbox->addStretch(1);

    // ── Center section: STATION callsign ─────────────────────────────────────
    auto* stationContainer = new QWidget(barWidget);
    stationContainer->setStyleSheet(QStringLiteral(
        "QWidget { border: 1px solid rgba(0,180,216,80); background: #0a0a14;"
        " padding: 1px 10px; border-radius: 3px; }"));
    QHBoxLayout* stationHbox = new QHBoxLayout(stationContainer);
    stationHbox->setContentsMargins(0, 0, 0, 0);
    stationHbox->setSpacing(6);

    auto* stationLabel = new QLabel(QStringLiteral("STATION:"), stationContainer);
    stationLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #00b4d8; font-size: 13px; border: none;"
        " background: transparent; padding: 0; }"));
    stationHbox->addWidget(stationLabel);

    m_callsignLabel = new QLabel(stationContainer);
    QString callsign = AppSettings::instance().value(
        QStringLiteral("StationCallsign"), QStringLiteral("NereusSDR")).toString();
    m_callsignLabel->setText(callsign);
    m_callsignLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #c8d8e8; font-size: 13px; font-weight: bold;"
        " border: none; background: transparent; padding: 0; }"));
    stationHbox->addWidget(m_callsignLabel);

    // ── ADC Overload indicator ─────────────────────────────────────────────
    // From Thetis console.cs ucInfoBar Warning() + pbAutoAttWarningRX1.
    m_adcOvlLabel = new QLabel(barWidget);
    m_adcOvlLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #FFD700; font-size: 12px; font-weight: bold;"
        " border: none; background: transparent; padding: 0 4px; }"));
    m_adcOvlLabel->hide();
    hbox->addWidget(m_adcOvlLabel);

    connect(m_stepAttController, &StepAttenuatorController::overloadStatusChanged,
            this, [this](int /*adc*/, OverloadLevel /*level*/) {
        // Build text from all ADCs
        QString text;
        bool anyRed = false;
        for (int i = 0; i < 3; ++i) {
            const OverloadLevel lvl = m_stepAttController->overloadLevel(i);
            if (lvl != OverloadLevel::None) {
                if (!text.isEmpty()) { text += QStringLiteral(" "); }
                text += QStringLiteral("ADC%1 OVL").arg(i);
                if (lvl == OverloadLevel::Red) {
                    anyRed = true;
                }
            }
        }

        if (text.isEmpty()) {
            m_adcOvlLabel->hide();
        } else {
            const QString color = anyRed ? QStringLiteral("#FF3333")
                                         : QStringLiteral("#FFD700");
            m_adcOvlLabel->setStyleSheet(
                QStringLiteral("QLabel { color: %1; font-size: 12px; font-weight: bold;"
                               " border: none; background: transparent;"
                               " padding: 0 4px; }").arg(color));
            m_adcOvlLabel->setText(text);
            m_adcOvlLabel->show();
        }

        // Tooltip: per-ADC detail
        // From Thetis console.cs: pbAutoAttWarningRX1 tooltip
        QString tip;
        for (int i = 0; i < 3; ++i) {
            if (m_stepAttController->overloadLevel(i) != OverloadLevel::None) {
                if (!tip.isEmpty()) { tip += QStringLiteral("\n"); }
                tip += QStringLiteral("ADC%1: overload").arg(i);
            }
        }
        m_adcOvlLabel->setToolTip(tip);
    });

    hbox->addWidget(stationContainer);

    // ── Stretch ───────────────────────────────────────────────────────────────
    hbox->addStretch(1);

    // ── Right section: indicators ────────────────────────────────────────────

    // Helper lambda: create a stacked indicator pair (top label + bottom label)
    // Returns the widget; sets topLbl/botLbl via out-params for wiring.
    auto makeIndicator = [&](const QString& top, const QString& bottom,
                              QLabel** outTop = nullptr, QLabel** outBot = nullptr) -> QWidget* {
        QWidget* w = new QWidget(barWidget);
        w->setMinimumWidth(60);
        QVBoxLayout* vl = new QVBoxLayout(w);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);
        auto* topLbl = new QLabel(top, w);
        topLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: #607080; font-size: 11px; }"));
        auto* botLbl = new QLabel(bottom, w);
        botLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: #404858; font-size: 11px; }"));
        vl->addWidget(topLbl);
        vl->addWidget(botLbl);
        if (outTop) { *outTop = topLbl; }
        if (outBot) { *outBot = botLbl; }
        return w;
    };

    // CAT Serial
    hbox->addWidget(makeIndicator(QStringLiteral("CAT"), QStringLiteral("Off")));
    hbox->addWidget(makeSep());

    // TCI
    hbox->addWidget(makeIndicator(QStringLiteral("TCI"), QStringLiteral("Off")));
    hbox->addWidget(makeSep());

    // PA Voltage
    hbox->addWidget(makeIndicator(QStringLiteral("PA"), QStringLiteral("— V")));
    hbox->addWidget(makeSep());

    // CPU usage — stacked "CPU: X.X%" / "Mem: —"
    // Wired to 1.5s QTimer using getrusage (macOS / POSIX).
    {
        QWidget* cpuWidget = makeIndicator(
            QStringLiteral("CPU: —"), QStringLiteral("Mem: —"),
            &m_cpuTopLabel, &m_cpuBotLabel);
        cpuWidget->setMinimumWidth(72);
        hbox->addWidget(cpuWidget);
    }
    hbox->addWidget(makeSep());

    // TX indicator — dim red when not transmitting; bright red when TX active (NYI)
    auto* txLabel = new QLabel(QStringLiteral("TX"), barWidget);
    txLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: rgba(180,40,40,120); font-weight: bold; font-size: 14px; }"));
    txLabel->setToolTip(QStringLiteral("Transmit (NYI)"));
    hbox->addWidget(txLabel);
    hbox->addWidget(makeSep());

    // Time display: stacked UTC + date / local
    // Top row: UTC time (hh:mm:ss UTC)
    // Bottom row: date + local time
    {
        auto* timeWidget = new QWidget(barWidget);
        timeWidget->setMinimumWidth(130);
        QVBoxLayout* tvl = new QVBoxLayout(timeWidget);
        tvl->setContentsMargins(0, 0, 0, 0);
        tvl->setSpacing(0);

        m_utcTimeLabel = new QLabel(timeWidget);
        m_utcTimeLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #8aa8c0; font-size: 11px; }"));
        m_utcTimeLabel->setToolTip(QStringLiteral("UTC time"));
        tvl->addWidget(m_utcTimeLabel);

        auto* localDateLabel = new QLabel(timeWidget);
        localDateLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #607080; font-size: 11px; }"));
        localDateLabel->setToolTip(QStringLiteral("Local date/time"));
        tvl->addWidget(localDateLabel);

        hbox->addWidget(timeWidget);

        // Combined clock timer — 1s updates for UTC+date+local
        m_clockTimer = new QTimer(this);
        connect(m_clockTimer, &QTimer::timeout, this, [this, localDateLabel]() {
            QDateTime utcNow = QDateTime::currentDateTimeUtc();
            QDateTime localNow = QDateTime::currentDateTime();
            m_utcTimeLabel->setText(utcNow.toString(QStringLiteral("hh:mm:ss UTC")));
            localDateLabel->setText(
                localNow.toString(QStringLiteral("yyyy-MM-dd  hh:mm")));
        });
        // Fire once immediately so labels are populated before first tick
        QDateTime utcNow = QDateTime::currentDateTimeUtc();
        QDateTime localNow = QDateTime::currentDateTime();
        m_utcTimeLabel->setText(utcNow.toString(QStringLiteral("hh:mm:ss UTC")));
        localDateLabel->setText(localNow.toString(QStringLiteral("yyyy-MM-dd  hh:mm")));
        m_clockTimer->start(1000);
    }

    // ── CPU usage timer (1.5s, macOS getrusage) ───────────────────────────────
    // Track cumulative CPU time between samples to compute percentage.
    // From macOS man page: getrusage(RUSAGE_SELF, &ru) gives ru_utime + ru_stime.
#ifdef Q_OS_MAC
    m_cpuTimer = new QTimer(this);
    // Snapshot state for delta calculation
    struct timeval prevUser{0, 0};
    struct timeval prevSys{0, 0};
    qint64 prevWallUs = QDateTime::currentMSecsSinceEpoch() * 1000LL;

    connect(m_cpuTimer, &QTimer::timeout, this,
            [this, prevUser, prevSys, prevWallUs]() mutable {
        struct rusage ru{};
        if (getrusage(RUSAGE_SELF, &ru) != 0) { return; }

        qint64 nowUs = QDateTime::currentMSecsSinceEpoch() * 1000LL;
        qint64 wallDelta = nowUs - prevWallUs;
        if (wallDelta <= 0) { return; }

        auto toUs = [](const struct timeval& tv) -> qint64 {
            return static_cast<qint64>(tv.tv_sec) * 1'000'000LL + tv.tv_usec;
        };
        qint64 userDelta = toUs(ru.ru_utime) - toUs(prevUser);
        qint64 sysDelta  = toUs(ru.ru_stime) - toUs(prevSys);
        double cpuPct = 100.0 * static_cast<double>(userDelta + sysDelta)
                        / static_cast<double>(wallDelta);

        prevUser    = ru.ru_utime;
        prevSys     = ru.ru_stime;
        prevWallUs  = nowUs;

        if (m_cpuTopLabel) {
            m_cpuTopLabel->setText(
                QStringLiteral("CPU: %1%").arg(cpuPct, 0, 'f', 1));
        }
    });
    m_cpuTimer->start(1500);
#endif

    // Add the full-width bar widget to the status bar
    sb->addWidget(barWidget, 1);
}

void MainWindow::wireSliceToSpectrum()
{
    SliceModel* slice = m_radioModel->activeSlice();
    if (!slice || !m_spectrumWidget) {
        return;
    }

    // Set initial spectrum display. Phase 3G-12: preserve the user's
    // persisted zoom level if present. SpectrumWidget::loadSettings()
    // has already read "DisplayBandwidth" from AppSettings into
    // m_bandwidthHz by this point. If the loaded value is sensible
    // (between 10 kHz and the DDC sample rate), keep it; otherwise
    // fall back to the full-span default (768 kHz = sample rate).
    double freq = slice->frequency();
    const double loadedBw = m_spectrumWidget->bandwidth();
    const double initialBw = (loadedBw >= 10000.0 && loadedBw <= 768000.0)
                             ? loadedBw : 768000.0;
    m_spectrumWidget->setFrequencyRange(freq, initialBw);
    m_spectrumWidget->setDdcCenterFrequency(freq);
    m_spectrumWidget->setSampleRate(768000.0);
    m_spectrumWidget->setVfoFrequency(freq);
    m_spectrumWidget->setFilterOffset(slice->filterLow(), slice->filterHigh());
    m_spectrumWidget->setStepSize(slice->stepHz());

    // --- Create floating VFO flag widget (AetherSDR pattern) ---
    VfoWidget* vfo = m_spectrumWidget->addVfoWidget(0);
    vfo->setSlice(slice);
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

    connect(slice, &SliceModel::dspModeChanged, this, [this, vfo](DSPMode mode) {
        vfo->setMode(mode);
        // Switch PhoneCwApplet page based on active mode
        if (m_phoneCwApplet) {
            switch (mode) {
                case DSPMode::CWL:
                case DSPMode::CWU:
                    m_phoneCwApplet->showPage(1);  // CW page
                    break;
                case DSPMode::FM:
                    m_phoneCwApplet->showPage(2);  // FM page
                    break;
                default:
                    m_phoneCwApplet->showPage(0);  // Phone page
                    break;
            }
        }
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

    // --- SliceModel → VfoWidget: RIT/XIT inbound (S1.8a stubs) ---
    connect(slice, &SliceModel::ritEnabledChanged, this, [vfo](bool on) {
        vfo->setRitEnabled(on);
    });

    connect(slice, &SliceModel::ritHzChanged, this, [vfo](int hz) {
        vfo->setRitHz(hz);
    });

    connect(slice, &SliceModel::xitEnabledChanged, this, [vfo](bool on) {
        vfo->setXitEnabled(on);
    });

    connect(slice, &SliceModel::xitHzChanged, this, [vfo](int hz) {
        vfo->setXitHz(hz);
    });

    // --- SliceModel → VfoWidget: DSP tab inbound (S1.8b stubs) ---
    connect(slice, &SliceModel::nb2EnabledChanged, this, [vfo](bool v) {
        vfo->setNb2Enabled(v);
    });

    connect(slice, &SliceModel::emnrEnabledChanged, this, [vfo](bool v) {
        vfo->setNr2Enabled(v);
    });

    connect(slice, &SliceModel::snbEnabledChanged, this, [vfo](bool v) {
        vfo->setSnbEnabled(v);
    });

    connect(slice, &SliceModel::apfEnabledChanged, this, [vfo](bool v) {
        vfo->setApfEnabled(v);
    });

    connect(slice, &SliceModel::apfTuneHzChanged, this, [vfo](int hz) {
        vfo->setApfTuneHz(hz);
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

    // --- VfoWidget → SliceModel: DSP tab outbound (S1.8b stubs) ---
    connect(vfo, &VfoWidget::nb2Changed, this, [slice](bool on) {
        slice->setNb2Enabled(on);
    });
    connect(vfo, &VfoWidget::nr2Changed, this, [slice](bool on) {
        // NR2 = EMNR in Thetis naming
        slice->setEmnrEnabled(on);
    });
    connect(vfo, &VfoWidget::snbChanged, this, [slice](bool on) {
        slice->setSnbEnabled(on);
    });
    connect(vfo, &VfoWidget::apfChanged, this, [slice](bool on) {
        slice->setApfEnabled(on);
    });
    connect(vfo, &VfoWidget::apfTuneHzChanged, this, [slice](int hz) {
        slice->setApfTuneHz(hz);
    });

    // --- SliceModel → VfoWidget: Audio tab inbound (S1.8c stubs) ---
    connect(slice, &SliceModel::mutedChanged, this, [vfo](bool v) {
        vfo->setMuted(v);
    });
    connect(slice, &SliceModel::audioPanChanged, this, [vfo](double p) {
        vfo->setAudioPan(p);
    });
    connect(slice, &SliceModel::ssqlEnabledChanged, this, [vfo](bool v) {
        vfo->setSsqlEnabled(v);
    });
    connect(slice, &SliceModel::ssqlThreshChanged, this, [vfo](double d) {
        vfo->setSsqlThresh(d);
    });
    connect(slice, &SliceModel::agcThresholdChanged, this, [vfo](int v) {
        vfo->setAgcThreshold(v);
    });
    connect(slice, &SliceModel::binauralEnabledChanged, this, [vfo](bool v) {
        vfo->setBinauralEnabled(v);
    });

    // --- VfoWidget → SliceModel: Audio tab outbound (S1.8c stubs) ---
    connect(vfo, &VfoWidget::muteChanged, this, [slice](bool v) {
        slice->setMuted(v);
    });
    connect(vfo, &VfoWidget::panChanged, this, [slice](double p) {
        slice->setAudioPan(p);
    });
    connect(vfo, &VfoWidget::squelchEnabledChanged, this, [slice](bool v) {
        slice->setSsqlEnabled(v);
    });
    connect(vfo, &VfoWidget::squelchThreshChanged, this, [slice](int v) {
        slice->setSsqlThresh(static_cast<double>(v));
    });
    connect(vfo, &VfoWidget::agcThreshChanged, this, [slice](int v) {
        slice->setAgcThreshold(v);
    });
    connect(vfo, &VfoWidget::binauralChanged, this, [slice](bool v) {
        slice->setBinauralEnabled(v);
    });
    connect(vfo, &VfoWidget::quickModeRequested, this, [slice](int index) {
        // Quick-mode buttons: 0=USB, 1=CW, 2=DIG (matching AetherSDR defaults)
        static constexpr DSPMode kQuickModes[] = {DSPMode::USB, DSPMode::CWU, DSPMode::DIGU};
        if (index >= 0 && index < 3) {
            slice->setDspMode(kQuickModes[index]);
        }
    });

    // --- VfoWidget → MainWindow: open Setup dialog (e.g. AGC-T right-click) ---
    connect(vfo, &VfoWidget::openSetupRequested, this, [this]() {
        auto* dialog = new SetupDialog(m_radioModel, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });

    // --- VfoWidget → SliceModel: RIT/XIT outbound (S1.8a stubs) ---
    connect(vfo, &VfoWidget::ritEnabledChanged, this, [slice](bool on) {
        slice->setRitEnabled(on);
    });

    connect(vfo, &VfoWidget::ritHzChanged, this, [slice](int hz) {
        slice->setRitHz(hz);
    });

    connect(vfo, &VfoWidget::xitEnabledChanged, this, [slice](bool on) {
        slice->setXitEnabled(on);
    });

    connect(vfo, &VfoWidget::xitHzChanged, this, [slice](int hz) {
        slice->setXitHz(hz);
    });

    // --- VfoWidget → SliceModel: STEP cycle (S1.8a — wires to live setStepHz) ---
    connect(vfo, &VfoWidget::stepCycleRequested, this, [slice]() {
        int current = slice->stepHz();
        int next = kStageOneStepLadder[0];
        for (int i = 0; i < kStageOneStepLadderSize; ++i) {
            if (kStageOneStepLadder[i] == current) {
                next = kStageOneStepLadder[(i + 1) % kStageOneStepLadderSize];
                break;
            }
        }
        // setStepHz emits stepHzChanged which the :1626-1629 handler uses to
        // propagate to m_spectrumWidget->setStepSize and vfo->setStepHz.
        slice->setStepHz(next);
    });

    // --- VfoWidget → SliceModel: lock state (S1.8a — verifying edge exists) ---
    connect(vfo, &VfoWidget::lockChanged, this, [slice](bool locked) {
        slice->setLocked(locked);
    });

    // --- SliceModel → VfoWidget: lock state inbound (S1.8a review — I3) ---
    // Without this edge, programmatic changes to SliceModel::locked (e.g. from
    // a future CAT/TCI command) would not be reflected in either lock button.
    connect(slice, &SliceModel::lockedChanged, this, [vfo](bool v) {
        vfo->setLocked(v);
    });

    // closeRequested → removeSlice wiring deferred to S1.10 — Stage 1
    // has no sliceRemoved cleanup path yet, so calling removeSlice leaves
    // dangling VfoWidget + lambda captures. See code review findings.

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

    // --- S-meter → VfoWidget level bar ---
    // MeterPoller emits smeterUpdated(double dbm) on each poll tick (100ms).
    // VfoWidget::setSmeter drives the VfoLevelBar S-meter indicator.
    if (m_meterPoller) {
        connect(m_meterPoller, &MeterPoller::smeterUpdated, vfo, &VfoWidget::setSmeter);
    }

    // --- Wire RxApplet to active slice ---
    if (m_rxApplet) {
        m_rxApplet->setSlice(slice);
    }

    // --- Wire overlay Band flyout to slice ---
    if (m_overlayPanel) {
        connect(m_overlayPanel, &SpectrumOverlayPanel::bandSelected,
                this, [slice](const QString& /*name*/, double freqHz, const QString& /*mode*/) {
            slice->setFrequency(freqHz);
        });
    }
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

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    // Handle ☰ panel toggle click — label has property "isPanelToggle"
    if (event->type() == QEvent::MouseButtonPress) {
        auto* label = qobject_cast<QLabel*>(watched);
        if (label && label->property("isPanelToggle").toBool()) {
            // Toggle QSplitter right pane (index 1) visibility.
            // Save/restore sizes so spectrum expands when panel is hidden.
            if (!m_mainSplitter || m_mainSplitter->count() < 2) {
                return QMainWindow::eventFilter(watched, event);
            }
            QWidget* rightPane = m_mainSplitter->widget(1);
            if (rightPane->isVisible()) {
                // Hide: save current sizes, then collapse right to 0
                m_splitterSizesBeforeHide = m_mainSplitter->sizes();
                rightPane->hide();
                label->setStyleSheet(QStringLiteral(
                    "QLabel { color: #404858; font-weight: bold; font-size: 16px; }"));
            } else {
                // Show: restore saved sizes (or default 80/20 if none saved)
                rightPane->show();
                if (!m_splitterSizesBeforeHide.isEmpty()) {
                    m_mainSplitter->setSizes(m_splitterSizesBeforeHide);
                } else {
                    m_mainSplitter->setSizes({1024, 256});
                }
                label->setStyleSheet(QStringLiteral(
                    "QLabel { color: #8aa8c0; font-weight: bold; font-size: 16px; }"));
            }
            return true;  // event consumed
        }
    }
    return QMainWindow::eventFilter(watched, event);
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

        // Wire step attenuator controller to the live radio connection
        // and set max attenuation from board capabilities.
        // From Thetis console.cs ucInfoBar Warning() + SetupForm attenuator init.
        m_stepAttController->setRadioConnection(m_radioModel->connection());
        const auto& caps = BoardCapsTable::forBoard(
            m_radioModel->connection()->radioInfo().boardType);
        m_stepAttController->setMaxAttenuation(caps.attenuator.maxDb);
        m_stepAttController->loadSettings(m_radioModel->connection()->radioInfo().macAddress);
    } else {
        m_radioModelLabel->setText(QStringLiteral("—"));
        m_radioModelLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #8aa8c0; font-size: 12px; }"));
        m_radioFwLabel->setText(QStringLiteral("—"));
        m_radioFwLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: #607080; font-size: 12px; }"));

        // Save step attenuator settings before disconnecting.
        if (m_radioModel->connection()) {
            m_stepAttController->saveSettings(m_radioModel->connection()->radioInfo().macAddress);
        }

        // Disconnect step attenuator from radio
        m_stepAttController->setRadioConnection(nullptr);
    }
}

// Phase 3I Task 17 — auto-reconnect on launch.
//
// Logic:
//   1. Read radios/lastConnected from AppSettings (set by ConnectionPanel
//      whenever the user explicitly connects).
//   2. Look up the SavedRadio entry; bail silently if missing or
//      autoConnect == false.
//   3a. pinToMac=true  → run a Fast-profile discovery, connect when the
//      same MAC is seen.  A 3-second kill timer stops the attempt if no
//      reply arrives — ConnectionPanel is unaffected.
//   3b. pinToMac=false → direct connect using the saved IP (faster;
//      if the IP drifted the user just opens ConnectionPanel and rescans).
//
// The m_autoReconnectInProgress flag gates the 3-second cleanup so it
// does not call stopDiscovery() if the user has already launched a manual
// scan via ConnectionPanel::onStartDiscoveryClicked.
void MainWindow::tryAutoReconnect()
{
    AppSettings& s = AppSettings::instance();
    const QString lastMac = s.lastConnected();
    if (lastMac.isEmpty()) {
        return;
    }

    const auto saved = s.savedRadio(lastMac);
    if (!saved.has_value()) {
        return;
    }
    if (!saved->autoConnect) {
        return;
    }

    qCInfo(lcConnection) << "Auto-reconnect: attempting" << lastMac
                         << "at" << saved->info.address.toString()
                         << "(pinToMac=" << saved->pinToMac << ")";

    if (saved->pinToMac) {
        // Use Fast profile — ~480ms per NIC, shorter than the SafeDefault
        // that the user's manual Start Discovery uses.
        RadioDiscovery* disc = m_radioModel->discovery();
        disc->setProfile(DiscoveryProfile::Fast);
        m_autoReconnectInProgress = true;

        // Listen for a radio that matches our saved MAC
        QMetaObject::Connection* connPtr = new QMetaObject::Connection;
        *connPtr = connect(disc, &RadioDiscovery::radioDiscovered,
            this, [this, lastMac, connPtr](const RadioInfo& found) {
            if (found.macAddress != lastMac) {
                return;
            }
            if (m_radioModel->isConnected()) {
                return; // User beat us to it
            }
            qCInfo(lcConnection) << "Auto-reconnect: MAC found —"
                                 << found.displayName()
                                 << found.address.toString();
            QObject::disconnect(*connPtr);
            delete connPtr;
            m_autoReconnectInProgress = false;
            // Load persisted model override for auto-reconnect (Phase 3I-RP)
            RadioInfo ri = found;
            HPSDRModel mo = AppSettings::instance().modelOverride(ri.macAddress);
            if (mo != HPSDRModel::FIRST) {
                ri.modelOverride = mo;
            }
            m_radioModel->connectToRadio(ri);
        });

        // Kick off the Fast-profile discovery
        disc->startDiscovery();

        // 3-second hard timeout — give up silently if MAC never appears
        QTimer::singleShot(3000, this, [this, connPtr]() {
            if (!m_autoReconnectInProgress) {
                // Already connected (lambda cleaned up) — nothing to do
                return;
            }
            qCInfo(lcConnection) << "Auto-reconnect: 3-second timeout — giving up silently";
            // Disconnect the listener so it doesn't fire on later scans
            QObject::disconnect(*connPtr);
            delete connPtr;
            m_autoReconnectInProgress = false;
            // Stop the discovery pass we started; restore SafeDefault profile
            // so the user's next manual scan uses the full timing.
            m_radioModel->discovery()->stopDiscovery();
            m_radioModel->discovery()->setProfile(DiscoveryProfile::SafeDefault);
        });
    } else {
        // No MAC pinning — direct connect to saved IP address.
        // Silent failure: if the radio doesn't respond, RadioConnection's
        // internal state machine eventually times out without any popup.
        if (!m_radioModel->isConnected()) {
            // Load persisted model override for auto-reconnect (Phase 3I-RP)
            RadioInfo ri = saved->info;
            HPSDRModel mo = AppSettings::instance().modelOverride(ri.macAddress);
            if (mo != HPSDRModel::FIRST) {
                ri.modelOverride = mo;
            }
            m_radioModel->connectToRadio(ri);
        }
    }
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

    // Ask Qt for an orderly exit from the event loop. Previously called
    // std::exit(0) which runs C++ static destructors before Qt's thread
    // cleanup — that caused QThreadStoragePrivate::finish to fire a qWarning
    // against a destructed QRegularExpression in the PII-redaction message
    // handler, segfaulting every close (~100 diagnostic reports in one day).
    QCoreApplication::quit();
}

// =============================================================================
// Phase 3G-14: AI-Assisted Issue Reporter
// Ported from AetherSDR TitleBar::showFeatureRequestDialog() /
// showFeatureRequestDialogImpl()
// =============================================================================

void MainWindow::showFeatureRequestDialog()
{
    // Version check gate — warn if not on latest release before filing
    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl(QStringLiteral(
        "https://api.github.com/repos/boydsoftprez/NereusSDR/releases/latest")));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("NereusSDR"));
    auto* reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam] {
        reply->deleteLater();
        nam->deleteLater();

        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QString latest = doc.object().value(QStringLiteral("tag_name")).toString();
            if (latest.startsWith(QLatin1Char('v'))) {
                latest = latest.mid(1);
            }
            QVersionNumber latestVer = QVersionNumber::fromString(latest);
            QVersionNumber currentVer = QVersionNumber::fromString(
                QCoreApplication::applicationVersion());
            if (!latestVer.isNull() && currentVer < latestVer) {
                auto answer = QMessageBox::warning(this,
                    QStringLiteral("Outdated Version"),
                    QStringLiteral(
                        "<p>You are running <b>v%1</b> but <b>v%2</b> is available.</p>"
                        "<p>Your issue may already be fixed in the latest release. "
                        "Please update before filing a bug report.</p>"
                        "<p>Continue anyway?</p>")
                        .arg(QCoreApplication::applicationVersion(), latest),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (answer != QMessageBox::Yes) {
                    return;
                }
            }
        }
        // Proceed to show the issue dialog
        showFeatureRequestDialogImpl();
    });
}

void MainWindow::showFeatureRequestDialogImpl()
{
    static const QString kPrompt = QStringLiteral(
        "IMPORTANT — before doing anything else, fetch the complete list of open\n"
        "issues by reading pages sequentially until you get fewer than 100 results:\n"
        "  Page 1: https://github.com/boydsoftprez/NereusSDR/issues?state=open&per_page=100&page=1\n"
        "  Page 2: https://github.com/boydsoftprez/NereusSDR/issues?state=open&per_page=100&page=2\n"
        "  ... continue until a page returns fewer than 100 issues.\n"
        "Do NOT rely on cached or training data for the issue list.\n\n"
        "Also fetch CLAUDE.md fresh (do not use cached versions):\n"
        "  https://raw.githubusercontent.com/boydsoftprez/NereusSDR/main/CLAUDE.md\n\n"
        "I want to report an issue or request a feature for NereusSDR, a cross-platform\n"
        "Qt6/C++20 SDR console for OpenHPSDR radios (ANAN, Hermes Lite 2, etc.). It uses\n"
        "the OpenHPSDR Protocol 1 and Protocol 2 over UDP, with client-side DSP via WDSP.\n\n"
        "DUPLICATE CHECK — this is mandatory. Search the fetched issue list for keywords\n"
        "related to my description below. Check titles AND bodies. If you find an existing\n"
        "issue that covers the same thing, STOP and tell me:\n"
        "  > Duplicate found: #<number> — <title>\n"
        "  > I recommend adding a +1 reaction and a comment describing your use case.\n"
        "Do NOT write a new issue if a duplicate exists.\n\n"
        "If no duplicate exists, determine whether my description is a BUG REPORT or a\n"
        "FEATURE REQUEST, then write a GitHub issue using the appropriate format below.\n"
        "Use GitHub-flavored Markdown formatting (headers, code blocks, bullet points).\n\n"
        "FOR FEATURE REQUESTS include:\n"
        "1. A clear, concise title (imperative mood)\n"
        "2. ## What — what the feature does from the user's perspective\n"
        "3. ## Why — what problem it solves\n"
        "4. ## How Other Clients Do It — how Thetis, PowerSDR, SparkSDR, etc. handle this\n"
        "5. ## Suggested Behavior — specific UX: what the user clicks, sees, what happens.\n"
        "   Reference NereusSDR UI elements (AppletPanel, VfoWidget, RxApplet, SetupDialog, etc.)\n"
        "6. ## Protocol Hints — relevant OpenHPSDR commands, or \"Unknown — needs research\"\n"
        "7. ## Acceptance Criteria — 3-5 bullet points defining done vs not-done\n\n"
        "FOR BUG REPORTS include:\n"
        "1. A clear title describing the broken behavior\n"
        "2. ## What happened — describe the incorrect behavior\n"
        "3. ## What I expected — describe the correct behavior\n"
        "4. ## Steps to reproduce — numbered steps to trigger the bug\n"
        "5. ## Environment — OS, radio model, protocol version, firmware version if relevant\n"
        "6. ## Suggested fix — if you have an idea what's wrong, describe it\n\n"
        "Suggest appropriate labels from: enhancement, bug, documentation,\n"
        "help wanted, good first issue, question\n\n"
        "Here is my idea or bug report:\n\n"
        "[Describe your feature or bug here in plain English]");

    // Reuse existing dialog if still open
    static QPointer<QDialog> sDlg;
    if (sDlg) {
        sDlg->raise();
        sDlg->activateWindow();
        return;
    }

    auto* dlg = new QDialog(this);
    sDlg = dlg;
    dlg->setWindowTitle(QStringLiteral("AI-Assisted Issue Reporter"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setStyleSheet(QStringLiteral("QDialog { background: #0f0f1a; }"));
    dlg->setMinimumWidth(620);

    auto* vbox = new QVBoxLayout(dlg);
    vbox->setSpacing(8);
    vbox->setContentsMargins(16, 16, 16, 16);

    auto* header = new QLabel(QStringLiteral(
        "<h3 style='color:#c8d8e8;'>AI-Assisted Issue Reporter</h3>"
        "<p style='color:#8090a0;'>Use any AI assistant to write a detailed bug report or feature request.</p>"
        "<ol style='color:#c8d8e8;'>"
        "<li><b>Choose your AI</b> below — prompt is copied to your clipboard</li>"
        "<li><b>Paste the prompt</b> into the AI chat</li>"
        "<li><b>Describe your idea</b> — edit the [bracketed] section</li>"
        "<li><b>Copy the AI's output</b> and click <b>Submit Your Idea</b></li>"
        "</ol>"));
    header->setWordWrap(true);
    vbox->addWidget(header);

    // Status label — shows after provider selected
    auto* statusLabel = new QLabel;
    statusLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #20c060; font-size: 11px; font-weight: bold; }"));
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->hide();
    vbox->addWidget(statusLabel);

    // AI provider buttons
    const QString btnStyle = QStringLiteral(
        "QPushButton { background: #1a2a3a; border: 1px solid #304050; "
        "border-radius: 3px; color: #c8d8e8; font-size: 12px; font-weight: bold; "
        "padding: 6px 12px; }"
        "QPushButton:hover { background: #203040; }");

    auto* btnRow1 = new QHBoxLayout;
    struct Provider { const char* name; const char* url; };
    static constexpr Provider providers[] = {
        {"Claude",     "https://claude.ai/new"},
        {"ChatGPT",    "https://chat.openai.com/"},
        {"Gemini",     "https://gemini.google.com/"},
        {"Grok",       "https://grok.x.ai/"},
        {"Perplexity", "https://www.perplexity.ai/"},
    };
    for (const auto& p : providers) {
        auto* btn = new QPushButton(QString::fromUtf8(p.name), dlg);
        btn->setStyleSheet(btnStyle);
        btn->setAutoDefault(false);
        QString url = QString::fromUtf8(p.url);
        connect(btn, &QPushButton::clicked, dlg, [url, statusLabel] {
            QApplication::clipboard()->setText(kPrompt);
            QDesktopServices::openUrl(QUrl(url));
            statusLabel->setText(QStringLiteral(
                "Prompt copied to clipboard — paste into the AI, "
                "then come back and click Submit Your Idea"));
            statusLabel->show();
        });
        btnRow1->addWidget(btn);
    }
    vbox->addLayout(btnRow1);

    vbox->addSpacing(8);

    // Submit / Report / Close
    auto* btnRow2 = new QHBoxLayout;

    auto* submitBtn = new QPushButton(QStringLiteral("Submit Your Idea"), dlg);
    submitBtn->setAutoDefault(false);
    submitBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #00b4d8; color: #0f0f1a; font-weight: bold; "
        "border-radius: 4px; padding: 8px 20px; font-size: 13px; }"
        "QPushButton:hover { background: #00c8f0; }"));
    connect(submitBtn, &QPushButton::clicked, dlg, [dlg] {
        QDesktopServices::openUrl(QUrl(QStringLiteral(
            "https://github.com/boydsoftprez/NereusSDR/issues/new?template=feature_request.yml")));
        QTimer::singleShot(500, dlg, &QDialog::close);
    });
    btnRow2->addWidget(submitBtn);

    auto* bugBtn = new QPushButton(QStringLiteral("Report a Bug"), dlg);
    bugBtn->setAutoDefault(false);
    bugBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #cc4040; color: #ffffff; font-weight: bold; "
        "border-radius: 4px; padding: 8px 20px; font-size: 13px; }"
        "QPushButton:hover { background: #dd5050; }"));
    connect(bugBtn, &QPushButton::clicked, dlg, [dlg] {
        QDesktopServices::openUrl(QUrl(QStringLiteral(
            "https://github.com/boydsoftprez/NereusSDR/issues/new?template=bug_report.yml")));
        QTimer::singleShot(500, dlg, &QDialog::close);
    });
    btnRow2->addWidget(bugBtn);

    auto* closeBtn = new QPushButton(QStringLiteral("Close"), dlg);
    closeBtn->setAutoDefault(false);
    closeBtn->setStyleSheet(btnStyle);
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);
    btnRow2->addWidget(closeBtn);
    vbox->addLayout(btnRow2);

    // Copy prompt to clipboard on first open
    QApplication::clipboard()->setText(kPrompt);

    dlg->show();
}

} // namespace NereusSDR
