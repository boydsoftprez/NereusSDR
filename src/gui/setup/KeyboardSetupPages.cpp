#include "KeyboardSetupPages.h"
#include "gui/StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>

namespace NereusSDR {

KeyboardShortcutsPage::KeyboardShortcutsPage(QWidget* parent)
    : SetupPage(QStringLiteral("Shortcuts"), parent)
{
    buildUI();
}

void KeyboardShortcutsPage::buildUI()
{
    setStyleSheet(QString::fromLatin1(Style::kPageStyle));

    auto* group = new QGroupBox(QStringLiteral("Shortcuts"), this);
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));

    auto* vLayout = new QVBoxLayout(group);
    vLayout->setSpacing(6);

    // Search bar
    m_searchEdit = new QLineEdit(group);
    m_searchEdit->setPlaceholderText(QStringLiteral("Search shortcuts…"));
    m_searchEdit->setStyleSheet(QString::fromLatin1(Style::kLineEditStyle));
    m_searchEdit->setDisabled(true);
    m_searchEdit->setToolTip(QStringLiteral("NYI — shortcut search"));
    vLayout->addWidget(m_searchEdit);

    // Table placeholder
    m_shortcutTableLabel = new QLabel(
        QStringLiteral("Shortcut list will appear here"), group);
    m_shortcutTableLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    m_shortcutTableLabel->setAlignment(Qt::AlignCenter);
    m_shortcutTableLabel->setMinimumHeight(180);
    vLayout->addWidget(m_shortcutTableLabel, 1);

    // Action buttons row
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(6);

    m_editButton = new QPushButton(QStringLiteral("Edit"), group);
    m_editButton->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    m_editButton->setDisabled(true);
    m_editButton->setToolTip(QStringLiteral("NYI — edit selected shortcut"));
    btnRow->addWidget(m_editButton);

    m_resetButton = new QPushButton(QStringLiteral("Reset"), group);
    m_resetButton->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    m_resetButton->setDisabled(true);
    m_resetButton->setToolTip(QStringLiteral("NYI — reset shortcuts to defaults"));
    btnRow->addWidget(m_resetButton);

    btnRow->addStretch();

    m_importButton = new QPushButton(QStringLiteral("Import…"), group);
    m_importButton->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    m_importButton->setDisabled(true);
    m_importButton->setToolTip(QStringLiteral("NYI — import shortcut map"));
    btnRow->addWidget(m_importButton);

    m_exportButton = new QPushButton(QStringLiteral("Export…"), group);
    m_exportButton->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    m_exportButton->setDisabled(true);
    m_exportButton->setToolTip(QStringLiteral("NYI — export shortcut map"));
    btnRow->addWidget(m_exportButton);

    vLayout->addLayout(btnRow);

    contentLayout()->addWidget(group);
    contentLayout()->addStretch();
}

} // namespace NereusSDR
