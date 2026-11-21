#pragma once
// NereusSDR-original helper-application installation and supervision.
//
// The Palstar HF-Auto frontend is shipped beside NereusSDR, seeded into the
// per-user application data folder on first use, then started minimised and
// stopped with the desktop app.
//
// WaveNode USB ships beside NereusSDR in its own folder. When the helper is
// enabled, Nereus starts it minimised if it is not already running (a helper
// the operator started is never taken over) and creates the Zjjg UDP listener
// for its XML. It is started detached, so it outlives the desktop app.
#include <QDateTime>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include <memory>

namespace NereusSDR {

class HelperApps : public QObject {
    Q_OBJECT
public:
    enum class Helper { WaveNode, HfAuto };
    Q_ENUM(Helper)

    explicit HelperApps(QObject* parent = nullptr);
    ~HelperApps() override;

    // The desktop app's one service. Created on first use, parented to the
    // application, so a settings page and the shutdown path share it.
    static HelperApps& instance();

    // <AppLocalData>/NereusSDR/helpers
    static QString installRoot();
    static QString folderName(Helper helper);
    static QString helperDirectory(Helper helper);
    static QString executablePath(Helper helper);

    // Copies the shipped folder into helperDirectory(). Binaries refresh when
    // the shipped copy is newer; any other existing file (the helper's INI,
    // licence, change log) is never overwritten.
    static bool ensureInstalled(Helper helper, QString* error = nullptr);
    // Removes the per-user copy. The shipped folder beside the app is left
    // alone, so the helper can be enabled again later.
    static bool removeInstalled(Helper helper, QString* error = nullptr);

    bool isRunning(Helper helper) const;
    // The saved switch, independent of whether the process is running.
    // Thetis starts an enabled helper with the console and leaves it running
    // when the switch is turned off (a serial link survives); the process is
    // stopped when the desktop app exits.
    bool enabled(Helper helper) const;
    void setEnabled(Helper helper, bool enabled);
    // Installs when needed, then starts the helper minimised. WaveNode USB is
    // started only if it is not already running, and detached.
    bool start(Helper helper, QString* error = nullptr);
    void stop(Helper helper);
    void stopAll();

signals:
    void runningChanged(NereusSDR::HelperApps::Helper helper, bool running);
    void enabledChanged(NereusSDR::HelperApps::Helper helper, bool enabled);
    // A helper stopped unexpectedly, lost communication or could not start.
    // The switch is turned off and linked containers hide, as Thetis does.
    void fault(NereusSDR::HelperApps::Helper helper, const QString& reason);

private:
    struct Entry;
    std::unique_ptr<Entry> m_waveNode;
    std::unique_ptr<Entry> m_hfAuto;
    Entry& entry(Helper helper);
    const Entry& entry(Helper helper) const;
    // Creates (or removes) the MMIO input that supplies the helper's meter
    // variables: Zjjg.* from WaveNode's UDP XML on 9388 (commands out on
    // 9389), QxG4.* from the HF-Auto helper's optional UDP JSON on 12050.
    void syncEndpoints(Helper helper, bool enabled);
    void checkWaveNodeHealth();
    void noteData(const QUuid& guid);
    void reportFault(Helper helper, const QString& reason);

    QTimer m_faultTimer;
    QDateTime m_waveNodeEnabledAt;
    QDateTime m_waveNodeLastData;
    bool m_waveNodeSeenData = false;
};

} // namespace NereusSDR
