#include "MmioVariablePickerPopup.h"

#include "../../core/mmio/ExternalVariableEngine.h"
#include "../../core/mmio/MmioEndpoint.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QLabel>

namespace NereusSDR {

namespace {

constexpr const char* kPickerStyle =
    "QDialog { background: #0f0f1a; color: #c8d8e8; }"
    "QLabel { color: #c8d8e8; font-size: 11px; }"
    "QTreeWidget {"
    "  background: #0a0a18; color: #c8d8e8;"
    "  border: 1px solid #203040;"
    "  alternate-background-color: #111122;"
    "}"
    "QPushButton {"
    "  background: #1a2a3a; color: #c8d8e8;"
    "  border: 1px solid #205070; border-radius: 3px;"
    "  padding: 3px 10px; min-height: 20px;"
    "}"
    "QPushButton:hover { background: #203040; }"
    "QPushButton:pressed { background: #00b4d8; color: #0f0f1a; }";

} // namespace

MmioVariablePickerPopup::MmioVariablePickerPopup(const QUuid& initialGuid,
                                                 const QString& initialVariable,
                                                 QWidget* parent)
    : QDialog(parent)
    , m_selectedGuid(initialGuid)
    , m_selectedVariable(initialVariable)
{
    setWindowTitle(QStringLiteral("Pick MMIO Variable"));
    setMinimumSize(420, 420);
    setStyleSheet(QLatin1String(kPickerStyle));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto* lbl = new QLabel(
        QStringLiteral("Select an endpoint variable to bind this item to, "
                       "or click Clear to unbind."),
        this);
    lbl->setWordWrap(true);
    root->addWidget(lbl);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({QStringLiteral("Variable"),
                              QStringLiteral("Value")});
    m_tree->setAlternatingRowColors(true);
    root->addWidget(m_tree, 1);

    auto* btnRow = new QHBoxLayout();
    m_btnClear = new QPushButton(QStringLiteral("Clear binding"), this);
    btnRow->addWidget(m_btnClear);
    btnRow->addStretch();
    m_btnCancel = new QPushButton(QStringLiteral("Cancel"), this);
    m_btnOk     = new QPushButton(QStringLiteral("OK"), this);
    m_btnOk->setDefault(true);
    btnRow->addWidget(m_btnCancel);
    btnRow->addWidget(m_btnOk);
    root->addLayout(btnRow);

    connect(m_btnOk,     &QPushButton::clicked, this, &MmioVariablePickerPopup::onAccept);
    connect(m_btnClear,  &QPushButton::clicked, this, &MmioVariablePickerPopup::onClear);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    buildTree();
}

MmioVariablePickerPopup::~MmioVariablePickerPopup() = default;

void MmioVariablePickerPopup::buildTree()
{
    m_tree->clear();
    const QList<MmioEndpoint*> eps = ExternalVariableEngine::instance().endpoints();
    for (MmioEndpoint* ep : eps) {
        const QString label = ep->name().isEmpty()
            ? ep->guid().toString(QUuid::WithoutBraces).left(8)
            : ep->name();
        auto* root = new QTreeWidgetItem(m_tree);
        root->setText(0, label);
        root->setData(0, Qt::UserRole, ep->guid());
        root->setData(0, Qt::UserRole + 1, QString());  // endpoint row, no var
        root->setFirstColumnSpanned(true);

        const QStringList names = ep->variableNames();
        for (const QString& n : names) {
            auto* leaf = new QTreeWidgetItem(root);
            leaf->setText(0, n);
            leaf->setText(1, ep->valueForName(n).toString());
            leaf->setData(0, Qt::UserRole, ep->guid());
            leaf->setData(0, Qt::UserRole + 1, n);

            // Pre-select the current binding.
            if (ep->guid() == m_selectedGuid && n == m_selectedVariable) {
                m_tree->setCurrentItem(leaf);
            }
        }
        root->setExpanded(true);
    }
}

void MmioVariablePickerPopup::onAccept()
{
    QTreeWidgetItem* cur = m_tree->currentItem();
    if (!cur) { reject(); return; }
    const QString varName = cur->data(0, Qt::UserRole + 1).toString();
    if (varName.isEmpty()) {
        // User highlighted an endpoint row rather than a variable leaf.
        reject();
        return;
    }
    m_selectedGuid = cur->data(0, Qt::UserRole).toUuid();
    m_selectedVariable = varName;
    m_cleared = false;
    accept();
}

void MmioVariablePickerPopup::onClear()
{
    m_selectedGuid = QUuid();
    m_selectedVariable.clear();
    m_cleared = true;
    accept();
}

} // namespace NereusSDR
