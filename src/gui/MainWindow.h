#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QAction>
#include <QActionGroup>
#include <QTimer>

class QProgressDialog;
class QThread;
class QSplitter;
class QMenu;

namespace NereusSDR {

class RadioModel;
class ConnectionPanel;
class SupportDialog;
class WdspEngine;
class FFTEngine;
class SpectrumWidget;
class ClarityController;
class ContainerManager;
class MeterWidget;
class MeterPoller;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onConnectionStateChanged();
    void showConnectionPanel();
    void showSupportDialog();

private:
    void buildUI();
    void buildMenuBar();
    void buildStatusBar();
    void applyDarkTheme();
    void tryAutoReconnect();
    void wireSliceToSpectrum();

    RadioModel* m_radioModel{nullptr};
    ConnectionPanel* m_connectionPanel{nullptr};
    SupportDialog* m_supportDialog{nullptr};

    // Status bar widgets (double-height AetherSDR design, 46px)
    QLabel* m_connStatusLabel{nullptr};
    QLabel* m_radioModelLabel{nullptr};
    QLabel* m_radioFwLabel{nullptr};
    QLabel* m_callsignLabel{nullptr};
    QLabel* m_utcTimeLabel{nullptr};
    QTimer* m_clockTimer{nullptr};
    QLabel* m_tnfLabel{nullptr};

    // Wisdom generation dialog (shown on first run)
    QProgressDialog* m_wisdomDialog{nullptr};

    // Spectrum display
    SpectrumWidget*     m_spectrumWidget{nullptr};
    FFTEngine*          m_fftEngine{nullptr};
    QThread*            m_fftThread{nullptr};
    ClarityController*  m_clarityController{nullptr};

    // Re-entrancy guard: prevents centerChanged from firing a second
    // forceHardwareFrequency while frequencyChanged is already retuning the DDC
    bool m_handlingBandJump{false};

    // Task 17: auto-reconnect guard — prevents the background attempt from
    // interfering with a subsequent user-initiated Start Discovery.
    bool m_autoReconnectInProgress{false};

    // Container infrastructure (Phase 3G-1)
    ContainerManager* m_containerManager{nullptr};
    QSplitter* m_mainSplitter{nullptr};
    int m_hDelta{0};
    int m_vDelta{0};

    void createDefaultContainers();

    // Phase 3G-6 block 6: dynamic "Edit Container ▸" submenu,
    // populated from ContainerManager::allContainers() and rebuilt
    // whenever a container is added, removed, or retitled. Addresses
    // the block 4 review observation that there was no way to see
    // or manage already-created containers from the menu bar.
    QMenu* m_editContainerMenu{nullptr};
    void rebuildEditContainerSubmenu();
    void resetDefaultLayout();

    // Meter system (Phase 3G-2)
    MeterWidget* m_meterWidget{nullptr};
    MeterPoller* m_meterPoller{nullptr};
    void populateDefaultMeter();

    // Menu DSP actions (for overlay sync — Phase 3-UI)
    QAction* m_nrAction  = nullptr;
    QAction* m_nbAction  = nullptr;
    QAction* m_anfAction = nullptr;

    // Mode menu actions (12 modes, mutual exclusion via QActionGroup)
    QAction*      m_modeActions[12]  = {};
    QActionGroup* m_modeActionGroup  = nullptr;

    // AGC menu action group (Task 12)
    QActionGroup* m_agcGroup = nullptr;

    // Dark theme checkable action (Task 12)
    QAction* m_darkThemeAction = nullptr;

    // Status bar members (Task 13)
    QLabel*  m_cpuTopLabel{nullptr};   // "CPU: X.X%"
    QLabel*  m_cpuBotLabel{nullptr};   // "Mem: —"
    QTimer*  m_cpuTimer{nullptr};
    QVector<int> m_splitterSizesBeforeHide;  // saved splitter sizes for ☰ toggle

    // Applets (Phase 3-UI)
    class RxApplet* m_rxApplet{nullptr};
    class PhoneCwApplet* m_phoneCwApplet{nullptr};
    class EqApplet* m_eqApplet{nullptr};

    // Applets — Tasks 7-10 (NYI shells, hidden until Task 15 Container wiring)
    class DigitalApplet*    m_digitalApplet{nullptr};
    class PureSignalApplet* m_pureSignalApplet{nullptr};
    class DiversityApplet*  m_diversityApplet{nullptr};
    class CwxApplet*        m_cwxApplet{nullptr};
    class DvkApplet*        m_dvkApplet{nullptr};
    class CatApplet*        m_catApplet{nullptr};
    class TunerApplet*      m_tunerApplet{nullptr};

    // Spectrum overlay panel
    class SpectrumOverlayPanel* m_overlayPanel{nullptr};

    // Applet panel — scrollable content widget inside Container #0
    class AppletPanelWidget* m_appletPanel{nullptr};
};

} // namespace NereusSDR
