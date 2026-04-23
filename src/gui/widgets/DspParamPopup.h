// =================================================================
// src/gui/widgets/DspParamPopup.h  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR src/gui/DspParamPopup.h @ 0cd4559.
// AetherSDR has no per-file headers — project-level license applies
// (GPLv3 per https://github.com/ten9876/AetherSDR).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-23 — Imported from AetherSDR. Namespace changed from
//                AetherSDR to NereusSDR; otherwise byte-for-byte.
//                Authored by J.J. Boyd (KG4VCF) with AI-assisted
//                review via Anthropic Claude Code.
// =================================================================

#pragma once

#include <QWidget>
#include <QVector>
#include <climits>
#include <functional>

class QVBoxLayout;
class QSlider;
class QLabel;
class QPushButton;
class QRadioButton;
class QButtonGroup;
class QCheckBox;

namespace NereusSDR {

// DspParamPopup — floating right-click popup for quick NR parameter access.
// Shows essential controls with a "More Settings..." link to the full dialog.
// Auto-dismisses on click outside (like the band/ant/dsp sub-panels).
class DspParamPopup : public QWidget {
    Q_OBJECT

public:
    explicit DspParamPopup(QWidget* parent = nullptr);

    // Add controls programmatically before showing
    void addRadioGroup(const QString& label, const QStringList& options,
                       int defaultIdx, std::function<void(int)> onChange);
    void addSlider(const QString& label, int min, int max, int defaultVal,
                   std::function<QString(int)> format, std::function<void(int)> onChange,
                   const QString& tooltip = QString(),
                   int factoryDefault = INT_MIN);  // INT_MIN → use defaultVal
    void addCheckbox(const QString& label, bool defaultVal,
                     std::function<void(bool)> onChange);

    // Finalize layout (adds More Settings + Reset buttons)
    void finalize(std::function<void()> onMore, std::function<void()> onReset);

    // Show anchored to a button position
    void showAt(const QPoint& globalPos);

protected:
    bool event(QEvent* ev) override;

private:
    QVBoxLayout* m_layout{nullptr};
    QVector<std::function<void()>> m_resetters;
};

} // namespace NereusSDR
