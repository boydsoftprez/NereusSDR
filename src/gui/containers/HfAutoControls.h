#pragma once
// NereusSDR-original: compact HF-Auto MMIO control strip shown in the HF-Auto
// container. Buttons send commands to the MMIO helper; their checked/enabled
// state follows the helper's QxG4 variables.
#include <QWidget>

class QLabel;
class QPushButton;
class QTimer;
class QShowEvent;
class QHideEvent;

namespace NereusSDR {

class HfAutoControls : public QWidget {
    Q_OBJECT
public:
    explicit HfAutoControls(QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void refresh();
    void sendMode(const char* command);
    QPushButton* makeButton(const QString& text, const QString& tooltip);

    QPushButton* m_auto{nullptr};
    QPushButton* m_manual{nullptr};
    QPushButton* m_bypass{nullptr};
    QPushButton* m_ant1{nullptr};
    QPushButton* m_ant2{nullptr};
    QPushButton* m_ant3{nullptr};
    QPushButton* m_dummy{nullptr};
    QPushButton* m_tf{nullptr};
    QPushButton* m_fine{nullptr};
    QPushButton* m_cl{nullptr};
    QPushButton* m_minus{nullptr};
    QPushButton* m_plus{nullptr};
    QPushButton* m_store{nullptr};
    QPushButton* m_hold{nullptr};
    QLabel* m_status{nullptr};
    QTimer* m_timer{nullptr};
    bool m_refreshing{false};
};

} // namespace NereusSDR
