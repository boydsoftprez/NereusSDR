#pragma once
// NereusSDR-original station-helper options, folded into the existing
// Setup > General > Options page as two groups:
//   - Station helpers (WaveNode / HF-Auto), opt-in and removable
//   - Start and exit scripts (Denkovi relay), opt-in and removable
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QSpinBox;

namespace NereusSDR {

class RadioModel;

class StationHelperOptions : public QWidget {
    Q_OBJECT
public:
    explicit StationHelperOptions(RadioModel* model, QWidget* parent = nullptr);

private:
    void updateHelperStatus();
    void syncScripts();
    void saveStartEnabled(bool on);
    void saveStopEnabled(bool on);
    void saveStartPath();
    void saveStopPath();
    void browse(QLineEdit* edit);
    void editScript(QLineEdit* edit, const QString& fallback);
    void openScriptFolder(QLineEdit* edit, const QString& fallback);

    QCheckBox* m_hfFollow = nullptr;
    QLabel* m_waveStatus = nullptr;
    QLabel* m_hfStatus = nullptr;
    QString m_waveFault;
    QString m_hfFault;

    QCheckBox* m_startEnabled = nullptr;
    QLineEdit* m_startPath = nullptr;
    QCheckBox* m_stopEnabled = nullptr;
    QLineEdit* m_stopPath = nullptr;
    QSpinBox* m_delay = nullptr;
    bool m_syncing = false;
};

} // namespace NereusSDR
