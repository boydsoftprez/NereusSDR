#pragma once
// no-port-check: NereusSDR-original approved Core Settings UI.
// SPDX-License-Identifier: GPL-3.0-or-later
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "gui/SetupPage.h"
#include "gui/CoreAddressController.h"
#include "gui/setup/CoreSettingsContext.h"
#include <QPointer>

class QButtonGroup;
class QGridLayout;
class QStackedWidget;
class QTabBar;
namespace NereusSDR {
class ThisCorePage;
class CoresSetupPage final : public SetupPage {
    Q_OBJECT
public:
    explicit CoresSetupPage(CoreTargetStore* store, RadioModel* model = nullptr,
                            QWidget* parent = nullptr,
                            CoreAddressProbe::RungFactory factory = {},
                            int deadlineMs = CoreAddressProbe::kDeadlineMs);
    ~CoresSetupPage() override;
    void setContext(const CoreSettingsContext& context);
    void refreshTargets();
    void inspectTarget(const QString& id);
    QString inspectedId() const { return m_inspectedId; }
    void cancelOperations();
    /// Serial rename binder must echo all three fences. The accepted name is
    /// obtained from authoritative coreInfo by the host; it is not a nickname.
    void finishRename(const CoreRenameRequest& request, bool accepted, const QString& reason);
    void setStationSettingsAvailable(bool available, const QString& reason) override;
signals:
    void renameRequested(NereusSDR::CoreRenameRequest request);
    void renameCancelled(quint64 requestId);
    void connectionDetailsRequested();
    void diagnosticsRequested();
    void audioRequested();
protected:
    void hideEvent(QHideEvent* event) override;
private:
    bool currentTarget() const;
    bool renameTarget() const;
    QString coreName(const SavedCoreTarget& target) const;
    void renderInspection();
    void renderAddresses();
    void renderAdministration();
    void openAddressEditor(const QString& previous = {});
    void closeAddressEditor();
    void submitAddress();
    void removeAddress(const QString& address);
    void openRenameEditor();
    void closeRenameEditor();
    CoreTargetStore* m_store;
    RadioModel* m_radioModel;
    CoreAddressController* m_addresses = nullptr;
    CoreSettingsContext m_context;
    bool m_dialogStationAvailable = true;
    QString m_dialogStationReason;
    QString m_inspectedId;
    quint64 m_incarnation = 0;
    quint64 m_addressOperation = 0;
    quint64 m_renameOperation = 0;
    quint64 m_nextRenameOperation = 0;
    std::optional<CoreRenameRequest> m_renameRequest;
    QString m_previousAddress;
    QWidget* m_coreList;
    QGridLayout* m_coreGrid;
    QLabel* m_heading;
    QLabel* m_name;
    QLabel* m_nameReason;
    QPushButton* m_rename;
    QWidget* m_nameDisplay;
    QWidget* m_nameEditor;
    QLineEdit* m_nameInput;
    QPushButton* m_nameSave;
    QLabel* m_nameError;
    QLabel* m_nameProgress;
    QTabBar* m_tabs;
    QStackedWidget* m_panels;
    QList<QLabel*> m_facts;
    QWidget* m_addressList;
    QList<QWidget*> m_addressGroups;
    QLabel* m_addressCount;
    QPushButton* m_addAddress;
    QGroupBox* m_addressEditor;
    QLineEdit* m_host;
    QSpinBox* m_port;
    QPushButton* m_submitAddress;
    QLabel* m_addressError;
    QLabel* m_addressProgress;
    QGroupBox* m_removeConfirmation;
    QLabel* m_removeText;
    QString m_removingAddress;
    QWidget* m_radioPanel;
    QWidget* m_devicesPanel;
    QPointer<ThisCorePage> m_administration;
    QList<QPointer<QWidget>> m_adminSections;
    QString m_adminTarget;
    quint64 m_adminEpoch = 0;
    QLabel* m_status;
    QPushButton* m_audio;
    QPushButton* m_details;
    QPushButton* m_diagnostics;
};
} // namespace NereusSDR
