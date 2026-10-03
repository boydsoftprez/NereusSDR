// =================================================================
// src/gui/CoreTargetEditor.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R3 Core session presentation and actions.
// =================================================================

#pragma once

#include "gui/CoreTargetStore.h"

#include <QDialog>
#include <functional>
#include <optional>

class QCheckBox;
class QLabel;
class QLineEdit;

namespace NereusSDR {

class CoreTargetEditor final : public QDialog {
    Q_OBJECT

public:
    explicit CoreTargetEditor(const SavedCoreTarget& initial, QWidget* parent = nullptr);

    SavedCoreTarget target() const;
    using CurrentOptionsSource = std::function<std::optional<RemoteStationOptions>()>;
    void setCurrentOptionsSource(CurrentOptionsSource source);
    void refreshServiceAvailability();

private:
    bool validate();

    SavedCoreTarget m_initial;
    QLineEdit* m_labelEdit{nullptr};
    QLineEdit* m_addressEdit{nullptr};
    QLineEdit* m_tokenEdit{nullptr};
    QLineEdit* m_fingerprintEdit{nullptr};
    QCheckBox* m_allowUnpinnedCheck{nullptr};
    // iPhone app plan Task 29: reach the Core through the internet service.
    QCheckBox* m_reachAnywhereCheck{nullptr};
    QLabel* m_reachAnywhereReason{nullptr};
    QLabel* m_errorLabel{nullptr};
    CurrentOptionsSource m_currentOptionsSource;
};

} // namespace NereusSDR
