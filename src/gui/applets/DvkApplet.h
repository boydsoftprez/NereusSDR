// src/gui/applets/DvkApplet.h
#pragma once
#include "AppletWidget.h"

class QPushButton;
class QLabel;
class QSpinBox;

namespace NereusSDR {

class HGauge;

// Digital Voice Keyer — voice memory playback/record.
// NYI — Phase 3I-1 (Basic SSB TX, mic input, MOX state machine).
class DvkApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit DvkApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("dvk"); }
    QString appletTitle() const override { return QStringLiteral("DVK"); }
    void    syncFromModel() override;

private:
    // 4 voice keyer slots: label + rec + play + stop
    QLabel*      m_slotLabel[4]  = {};
    QPushButton* m_recBtn[4]     = {};
    QPushButton* m_playBtn[4]    = {};
    QPushButton* m_stopBtn[4]    = {};

    // Record level gauge: -40 to 0 dB, red@-3
    HGauge*      m_recLevel      = nullptr;

    // Repeat toggle + interval
    QPushButton* m_repeatBtn     = nullptr;  // green toggle
    QSpinBox*    m_repeatSpin    = nullptr;  // 1–999 sec

    // Semi break-in toggle
    QPushButton* m_semiBkBtn     = nullptr;  // green toggle

    // WAV import button
    QPushButton* m_importBtn     = nullptr;
};

} // namespace NereusSDR
