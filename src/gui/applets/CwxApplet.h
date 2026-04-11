// src/gui/applets/CwxApplet.h
#pragma once
#include "AppletWidget.h"

class QPushButton;
class QTextEdit;
class QSlider;
class QLabel;
class QSpinBox;

namespace NereusSDR {

// CW keyer message composer and memory keyer.
// NYI — Phase 3I-2 (CW TX, sidetone, firmware keyer, QSK/break-in).
class CwxApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit CwxApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("cwx"); }
    QString appletTitle() const override { return QStringLiteral("CWX"); }
    void    syncFromModel() override;

private:
    QTextEdit*   m_textEdit      = nullptr;  // fixed height 60
    QPushButton* m_sendBtn       = nullptr;

    QSlider* m_speedSlider       = nullptr;  // 1–60 WPM
    QLabel*  m_speedValue        = nullptr;

    QPushButton* m_memBtn[6]     = {};       // M1–M6, fixedWidth 32

    QPushButton* m_repeatBtn     = nullptr;  // green toggle
    QSpinBox*    m_repeatSpin    = nullptr;  // 1–999 sec

    QPushButton* m_kbCwBtn       = nullptr;  // green toggle "KB→CW"
};

} // namespace NereusSDR
