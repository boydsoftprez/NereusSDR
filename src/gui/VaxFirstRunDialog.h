#pragma once

// =================================================================
// src/gui/VaxFirstRunDialog.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original file — no Thetis port; no attribution-registry row.
//
// Modal first-run assistant that walks the user through VAX channel
// setup. Five scenarios (Windows with/without 3rd-party cables, macOS
// native, Linux native, and post-install rescan) share one dialog
// class that dispatches layout by scenario.
//
// Task 11a ships the widget only. MainWindow wiring, persistence
// (audio/FirstRunComplete), and Setup→Audio→VAX tab integration land
// in Task 11b + Sub-Phase 12 respectively.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Written by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Anthropic Claude Code.
//   2026-10-09 - native audio plan Task 18 (R-AUD-03, R-AUD-06):
//                setAudioEngine() lists the cables from the engine's
//                device catalogue and follows it live; "Rescan now"
//                rescans the older drivers and lists again. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QDialog>
#include <QPair>
#include <QPointer>
#include <QString>
#include <QVector>

#include "core/audio/VirtualCableDetector.h"

class QPushButton;
class QVBoxLayout;

namespace NereusSDR {

class AudioEngine;
class IAudioDeviceCatalog;

enum class FirstRunScenario {
    WindowsCablesFound,   // A: Windows, 3rd-party virtual cables detected
    WindowsNoCables,      // B: Windows, no 3rd-party cables installed — prompt to install one
    MacNative,            // C: Mac, NereusSDR HAL plugin ready, success screen
    LinuxNative,          // D: Linux, PulseAudio/PipeWire pipe-source ready, success screen
    RescanNewCables,      // E: App-startup rescan, NEW cables appeared since last run
};

class VaxFirstRunDialog : public QDialog {
    Q_OBJECT

public:
    VaxFirstRunDialog(FirstRunScenario scenario,
                      const QVector<DetectedCable>& detected,
                      QWidget* parent = nullptr);

    // R-AUD-03 / R-AUD-06: the engine whose device catalogue the lists
    // come from. While the dialog is open a cable added or removed shows
    // at once (on Windows a first cable turns "no cables" into the
    // suggested bindings), and "Rescan now" rescans the older drivers
    // before listing again. Without an engine the dialog shows what it
    // was given and "Rescan now" scans once.
    void setAudioEngine(AudioEngine* engine);

#ifdef NEREUS_BUILD_TESTS
    // Test seam — returns the scenario this dialog was constructed for.
    // Gated behind NEREUS_BUILD_TESTS so production builds don't expose
    // the getter. Matches the AudioEngine::setVaxBusForTest pattern.
    FirstRunScenario scenarioForTest() const { return m_scenario; }

    // Test seam — forwards to the private computeSuggestedBindings()
    // helper so unit tests can assert the {channel, deviceName} payload
    // without having to click the Apply button and snoop the signal.
    QVector<QPair<int, QString>> suggestedBindingsForTest() const
    {
        return computeSuggestedBindings();
    }
    // The cables the dialog shows now.
    QVector<DetectedCable> detectedForTest() const { return m_detected; }
#endif

signals:
    // Scenario A, E: user clicked "Apply suggested" / "Apply to VAX 3 & 4".
    // Payload is {channel 1..4, deviceName} pairs.
    void applySuggested(const QVector<QPair<int, QString>>& bindings);

    // Scenario A, B: user clicked "Customize…" / "Why do I need this?" link.
    // Sub-Phase 12 wires this to SetupDialog::selectPage(pageLabel).
    // pageLabel is "Digital modes": the Audio left-nav item that holds VAX (R-SPK-21).
    void openSetupAudioPage(const QString& pageLabel);

    // Scenario B: user clicked "Open download page" for a vendor product.
    void openInstallUrl(const QString& url);

private:
    // Computes the {channel, deviceName} pairs that would be emitted by
    // "Apply suggested" / "Apply to VAX 3 & 4". Pure function of
    // m_scenario + m_detected; used internally by onApplySuggested() and
    // by the Scenario E footer label synthesis in buildFooter(). Exposed
    // to tests via the NEREUS_BUILD_TESTS-gated suggestedBindingsForTest()
    // forwarder above.
    QVector<QPair<int, QString>> computeSuggestedBindings() const;

    void buildUI();  // dispatches by m_scenario to the 5 layouts
    // The body and footer for m_scenario; rebuildBody() replaces them.
    void buildBodyAndFooter();
    void rebuildBody();
    // The cables for this scenario from the catalogue, then the body again.
    void refreshFromCatalogue();
    void onRescanNow();

    // Per-scenario body builders. Each appends its widgets to `bodyLayout`
    // and returns the footer widget row the caller should install.
    QWidget* buildHeaderBar();
    void buildBodyWindowsCablesFound(QVBoxLayout* bodyLayout);
    void buildBodyWindowsNoCables(QVBoxLayout* bodyLayout);
    void buildBodyMacNative(QVBoxLayout* bodyLayout);
    void buildBodyLinuxNative(QVBoxLayout* bodyLayout);
    void buildBodyRescanNewCables(QVBoxLayout* bodyLayout);
    QWidget* buildFooter();

    // Slot helpers. Defined as members so signal/slot wiring is explicit
    // and the signal-emit-site TODO comments are easy to locate.
    void onApplySuggested();
    void onSkip();
    void onCustomize();
    void onGotIt();

    // Header-bar platform badge text + colour depend on the scenario.
    QString platformBadgeText() const;
    QString platformBadgeColor() const;

    // Scenario-specific copy. Kept in one place so the byte-verbatim
    // mockup strings are easy to audit against the HTML source.
    QString headerTitle() const;

    FirstRunScenario m_scenario;
    QVector<DetectedCable> m_detected;

    QPointer<AudioEngine> m_engine;
    QPointer<IAudioDeviceCatalog> m_catalogue;
    // Scenario E: the cables known when the engine was attached, which
    // are not new.
    QVector<DetectedCable> m_known;
    bool m_rescanWaiting{false};
    QVBoxLayout* m_mainLayout{nullptr};
    QWidget* m_body{nullptr};
    QWidget* m_footer{nullptr};
};

} // namespace NereusSDR
