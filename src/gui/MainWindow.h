#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QAction>
#include <QActionGroup>
#include <QTimer>

class QProgressDialog;
class QThread;
class QSplitter;

namespace NereusSDR {

class RadioModel;
class ConnectionPanel;
class SupportDialog;
class WdspEngine;
class FFTEngine;
class SpectrumWidget;
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
    SpectrumWidget* m_spectrumWidget{nullptr};
    FFTEngine*      m_fftEngine{nullptr};
    QThread*        m_fftThread{nullptr};

    // Re-entrancy guard: prevents centerChanged from firing a second
    // forceHardwareFrequency while frequencyChanged is already retuning the DDC
    bool m_handlingBandJump{false};

    // Container infrastructure (Phase 3G-1)
    ContainerManager* m_containerManager{nullptr};
    QSplitter* m_mainSplitter{nullptr};
    int m_hDelta{0};
    int m_vDelta{0};

    void createDefaultContainers();

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

    // PhoneCwApplet (Phase 3I-1 NYI shell — Phone/CW/FM 30 controls)
    class PhoneCwApplet* m_phoneCwApplet{nullptr};
};

} // namespace NereusSDR
