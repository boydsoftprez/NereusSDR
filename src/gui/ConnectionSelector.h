// =================================================================
// src/gui/ConnectionSelector.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R3 Core session presentation and actions.
//
// Presentation-only selector for local radios, discovered Cores and the
// operator's saved Cores. Session construction and target persistence belong
// to the coordinator and CoreTargetStore respectively.
//
// iPhone app Task 18 (R-IOS-08): the pairing design's three groups
// (section 6): Radios on this network, Cores on this network (a Core no
// device has paired with yet offers Pair in place of Connect) and Your
// Cores; plus Add a Core by code (AddCoreByCodeDialog) and Type an
// address (the Core setup editor, which the Add Core button used to open).
// =================================================================

#pragma once

#include <QDialog>
#include <QList>
#include <QString>

class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace NereusSDR {

enum class ConnectionTargetKind {
    LocalRadio,
    LanCore,
    SavedCore,
};

struct ConnectionTargetRow {
    QString key;
    ConnectionTargetKind kind;
    QString name;
    QString radioText;
    QString address;
    QString state;
    bool connectable = true;
    bool editable = false;
    bool forgettable = false;
    /// iPhone app Task 18: a Core on this network that takes new devices.
    /// Its action is Pair, in place of Connect.
    bool pairable = false;
};

class ConnectionSelector final : public QDialog {
    Q_OBJECT

public:
    explicit ConnectionSelector(QWidget* parent = nullptr);

    void setTargets(const QList<ConnectionTargetRow>& targets);
    void setCurrentConnection(const QString& summary, const QString& details,
                              bool canDisconnect, bool retrying);
    void setNotice(const QString& notice);
    void setDiscoveryStatus(const QString& text);

    /// Host enables this only after binding the native Cores destination.
    void setCoreManagementAvailable(bool available);
    QString selectedKey() const;
    void setSelectedKey(const QString& key);

signals:
    void connectRequested(QString key);
    /// iPhone app Task 18: Pair on a Core on this network.
    void pairRequested(QString key);
    /// iPhone app Task 18: Add a Core by code.
    void addByCodeRequested();
    void disconnectRequested();
    void editRequested(QString key);
    void manageCoreRequested(QString key);
    void forgetRequested(QString key);
    void detailsRequested(QString key);
    void addCoreRequested();
    void addRadioRequested();
    void scanRequested();

private:
    void addGroup(const QString& title, ConnectionTargetKind kind,
                  const QString& emptyText, const QList<ConnectionTargetRow>& targets);
    QTreeWidgetItem* groupForKind(ConnectionTargetKind kind) const;
    bool groupStructureMatches(ConnectionTargetKind kind,
                               const QList<ConnectionTargetRow>& targets) const;
    const ConnectionTargetRow* selectedTarget() const;
    void updateActions();
    QPushButton* makeButton(const QString& text, const QString& objectName);

    QTreeWidget* m_targetTree{nullptr};
    QLabel* m_discoveryStatusLabel{nullptr};
    QLabel* m_noticeLabel{nullptr};
    QLabel* m_currentSummaryLabel{nullptr};
    QLabel* m_currentDetailsLabel{nullptr};
    QPushButton* m_addCoreButton{nullptr};
    QPushButton* m_addByCodeButton{nullptr};
    QPushButton* m_addRadioButton{nullptr};
    QPushButton* m_scanButton{nullptr};
    QPushButton* m_editButton{nullptr};
    QPushButton* m_manageCoreButton{nullptr};
    bool m_coreManagementAvailable{false};
    QPushButton* m_forgetButton{nullptr};
    QPushButton* m_detailsButton{nullptr};
    QPushButton* m_disconnectButton{nullptr};
    QPushButton* m_connectButton{nullptr};
    QList<ConnectionTargetRow> m_targets;
    bool m_canDisconnect{false};
    bool m_retrying{false};
};

// iPhone app Task 18 (R-IOS-08): Add a Core by code. The operator types
// the code the Core shows (on its console, its status page or the Core
// computer's Remote Access page). The code is checked against the word list
// here, before anything is sent, so a typing mistake never burns it. A
// direct address is optional and disclosed only when chosen or prefilled
// from a known LAN Core; it takes a name, IPv4 or IPv6, with optional port.
class AddCoreByCodeDialog final : public QDialog {
    Q_OBJECT

public:
    explicit AddCoreByCodeDialog(const QString& address = QString(), QWidget* parent = nullptr);

    /// The code as typed (normalise it with PairingCode::normalise).
    QString code() const;
    /// The address, read by StationPairingClient::parseAddress.
    QString host() const { return m_host; }
    quint16 port() const { return m_port; }

private:
    bool validate();

    QLineEdit* m_codeEdit{nullptr};
    QLineEdit* m_addressEdit{nullptr};
    QWidget* m_addressGroup{nullptr};
    QLabel* m_errorLabel{nullptr};
    QString m_host;
    quint16 m_port{0};
};

} // namespace NereusSDR
