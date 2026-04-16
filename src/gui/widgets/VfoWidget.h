#pragma once

#include "core/WdspTypes.h"
#include "models/SliceModel.h"
#include "VfoLevelBar.h"
#include "ScrollableLabel.h"
#include "VfoModeContainers.h"

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <QStackedWidget>
#include <QLineEdit>
#include <QPointer>

namespace NereusSDR {

// Floating VFO flag widget — AetherSDR pattern.
// Each slice gets one VfoWidget, parented to SpectrumWidget.
// Positioned at the VFO marker via move() from updatePosition().
//
// Layout (top to bottom):
//   Header:    [RX ANT] [TX ANT] [Filter] ... [TX] [A]
//   Frequency: "14.225.000  MHz"  (26px mono, click-edit, wheel-tune)
//   S-Meter:   [████████░░░]  -85 dBm
//   Tab bar:   Audio | DSP | Mode | X/RIT
//   Tab pages: (collapsible content)
//
// From AetherSDR VfoWidget.h pattern.
class VfoWidget : public QWidget {
    Q_OBJECT

public:
    // Direction hint for positioning the flag relative to the VFO marker.
    enum class FlagDir { Auto, ForceLeft, ForceRight };

    explicit VfoWidget(QWidget* parent = nullptr);
    ~VfoWidget() override;

    // --- State setters (called from model, guarded against re-emit) ---

    void setFrequency(double hz);
    void setMode(DSPMode mode);
    void setFilter(int low, int high);
    void setAgcMode(AGCMode mode);
    void setAfGain(int gain);
    void setRfGain(int gain);
    void setRxAntenna(const QString& ant);
    void setTxAntenna(const QString& ant);
    void setStepHz(int hz);
    void setSliceIndex(int index);
    void setTxSlice(bool isTx);
    void setAntennaList(const QStringList& ants);
    void setSmeter(double dbm);

    // --- RIT/XIT state setters (S1.8a — guarded against re-emit) ---
    void setRitEnabled(bool v);
    void setRitHz(int hz);
    void setXitEnabled(bool v);
    void setXitHz(int hz);

    // --- Lock state setter (S1.8a review — syncs both m_lockBtn and m_xritLockBtn) ---
    void setLocked(bool v);

    // --- DSP tab state setters (S1.8b — guarded against re-emit) ---
    void setNb2Enabled(bool v);
    void setNr2Enabled(bool v);
    void setSnbEnabled(bool v);
    void setApfEnabled(bool v);
    void setApfTuneHz(int hz);

    // --- Mode container visibility (S1.9) ---
    // Shows/hides the three mode containers embedded in DspTab based on the
    // current demodulation mode, and re-evaluates the APF tune slider visibility.
    void applyModeVisibility(DSPMode mode);

    // --- Audio tab state setters (S1.8c — guarded against re-emit) ---
    void setMuted(bool v);
    void setAudioPan(double pan);      // drives m_panSlider → round(pan * 100)
    void setSsqlEnabled(bool v);
    void setSsqlThresh(double dB);     // drives m_sqlSlider → round(dB)
    void setAgcThreshold(int dBu);
    void setBinauralEnabled(bool v);

    // --- Slice coupling (for mode container binding only) ---
    void setSlice(SliceModel* slice);

    // --- Positioning ---

    // Reposition the flag at the given pixel x of the VFO marker.
    // specTop is the y of the spectrum widget's top edge.
    void updatePosition(int vfoX, int specTop, FlagDir dir = FlagDir::Auto);

    int sliceIndex() const { return m_sliceIndex; }

signals:
    void frequencyChanged(double hz);
    void modeChanged(NereusSDR::DSPMode mode);
    void filterChanged(int low, int high);
    void agcModeChanged(NereusSDR::AGCMode mode);
    void afGainChanged(int gain);
    void rfGainChanged(int gain);
    void rxAntennaChanged(const QString& ant);
    void txAntennaChanged(const QString& ant);
    void nb1Changed(bool enabled);
    void nrChanged(bool enabled);
    void anfChanged(bool enabled);
    void sliceActivationRequested(int sliceIndex);
    void closeRequested(int sliceIndex);
    void lockChanged(bool locked);

    // --- X/RIT tab signals (S1.8a) ---
    void ritEnabledChanged(bool enabled);
    void ritHzChanged(int hz);
    void xitEnabledChanged(bool enabled);
    void xitHzChanged(int hz);
    void stepCycleRequested();

    // --- DSP tab signals (S1.8b) ---
    void nb2Changed(bool enabled);
    void nr2Changed(bool enabled);    // maps to emnrEnabled in SliceModel
    void snbChanged(bool enabled);
    void apfChanged(bool enabled);
    void apfTuneHzChanged(int hz);

    // --- Audio tab signals (S1.8c) ---
    void panChanged(double pan);           // -1.0 to 1.0
    void muteChanged(bool muted);
    void binauralChanged(bool enabled);
    void squelchEnabledChanged(bool enabled);
    void squelchThreshChanged(int thresh);
    void agcThreshChanged(int dBu);

    // --- Mode tab signals (S1.8c) ---
    void quickModeRequested(int index);

    // --- Record/play signals (S1.10 — NYI in Stage 1, wired to future recording subsystem) ---
    void recordToggled(bool recording);
    void playToggled(bool playing);

    // --- Setup dialog request (e.g. AGC-T right-click → open settings) ---
    void openSetupRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void buildUI();
    void buildHeaderRow();
    void buildFrequencyRow();
    void buildSmeterRow();
    void buildTabBar();
    void buildAudioTab();
    void buildDspTab();
    void buildModeTab();
    void buildXRitTab();
    void rebuildFilterButtons(DSPMode mode);
    void updateFreqLabel();
    QString formatFilterWidth(int low, int high) const;

    // Guard to prevent signal re-emission during model updates
    bool m_updatingFromModel{false};

    // Internal helper — update m_locked + drive both lock buttons + emit lockChanged.
    // Called by both the floating m_lockBtn toggled lambda and m_xritLockBtn toggled
    // lambda so both paths are in sync.  Must be called outside m_updatingFromModel.
    void applyLockedState(bool on);

    // Slice identity
    int m_sliceIndex{0};
    int m_stepHz{100};
    double m_frequency{14225000.0};
    DSPMode m_currentMode{DSPMode::USB};
    double m_smeterDbm{-127.0};

    // Fixed width from AetherSDR VfoWidget
    static constexpr int kWidgetW = 252;

    // --- Header row ---
    QPushButton* m_rxAntBtn{nullptr};
    QPushButton* m_txAntBtn{nullptr};
    QLabel*      m_filterWidthLbl{nullptr};
    QPushButton* m_txBadge{nullptr};
    QLabel*      m_splitBadge{nullptr};
    QLabel*      m_sliceBadge{nullptr};
    QStringList  m_antennaList{QStringLiteral("ANT1"), QStringLiteral("ANT2"), QStringLiteral("ANT3")};

    // --- Frequency row ---
    QStackedWidget* m_freqStack{nullptr};
    QLabel*         m_freqLabel{nullptr};
    QLineEdit*      m_freqEdit{nullptr};

    // --- S-meter row ---
    VfoLevelBar* m_levelBar{nullptr};

    // --- Tab bar ---
    QList<QPushButton*> m_tabButtons;
    QStackedWidget*     m_tabStack{nullptr};
    int                 m_activeTab{0};

    // --- Mode tab ---
    QComboBox*          m_modeCmb{nullptr};
    QWidget*            m_filterBtnContainer{nullptr};
    QSlider*            m_rfGainSlider{nullptr};
    QLabel*             m_rfGainLabel{nullptr};
    QPushButton*        m_quickModeBtns[3]{};    // USB/CW/DIG shortcut slots (NYI)

    // --- Audio tab ---
    QSlider*            m_afGainSlider{nullptr};
    QLabel*             m_afGainLabel{nullptr};
    QPushButton*        m_agcBtns[5]{};          // Off/Long/Slow/Med/Fast — replaces m_agcCmb
    QSlider*            m_panSlider{nullptr};
    QLabel*             m_panLabel{nullptr};
    QPushButton*        m_muteBtn{nullptr};
    QPushButton*        m_binBtn{nullptr};
    QPushButton*        m_sqlBtn{nullptr};
    QSlider*            m_sqlSlider{nullptr};
    QSlider*            m_agcTSlider{nullptr};
    QLabel*             m_agcTLabel{nullptr};

    // --- DSP tab ---
    QPushButton*        m_nb1Toggle{nullptr};
    QPushButton*        m_nb2Toggle{nullptr};
    QPushButton*        m_nrToggle{nullptr};
    QPushButton*        m_nr2Toggle{nullptr};
    QPushButton*        m_anfToggle{nullptr};
    QPushButton*        m_snbToggle{nullptr};
    QPushButton*        m_apfToggle{nullptr};
    QLabel*             m_apfLabel{nullptr};        // "APF" row label (S1.9 — promoted from local)
    QSlider*            m_apfTuneSlider{nullptr};
    QLabel*             m_apfTuneLabel{nullptr};
    FmOptContainer*        m_fmContainer{nullptr};
    DigOffsetContainer*    m_digContainer{nullptr};
    RttyMarkShiftContainer* m_rttyContainer{nullptr};

    // --- Slice coupling (for mode container binding only) ---
    QPointer<SliceModel> m_slice;

    // --- X/RIT tab ---
    QPushButton*   m_ritBtn{nullptr};
    ScrollableLabel* m_ritLabel{nullptr};
    QPushButton*   m_ritZeroBtn{nullptr};
    QPushButton*   m_xitBtn{nullptr};
    ScrollableLabel* m_xitLabel{nullptr};
    QPushButton*   m_xitZeroBtn{nullptr};
    QPushButton*   m_xritLockBtn{nullptr};
    QPushButton*   m_stepCycleBtn{nullptr};

    // --- Floating control buttons (AetherSDR pattern) ---
    // Rendered as children of parent widget, positioned beside the VFO flag.
    QPushButton* m_closeBtn{nullptr};
    QPushButton* m_lockBtn{nullptr};
    QPushButton* m_recBtn{nullptr};
    QPushButton* m_playBtn{nullptr};
    bool m_locked{false};
    bool m_onLeft{false};  // track flag side for button placement
    void buildFloatingButtons();
    void positionFloatingButtons();

    // Slice color table: A=cyan, B=magenta, C=green, D=yellow
    static QColor sliceColor(int index);
};

} // namespace NereusSDR
