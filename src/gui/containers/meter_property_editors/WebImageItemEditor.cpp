#include "WebImageItemEditor.h"
#include "../../meters/WebImageItem.h"

#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QColorDialog>
#include <QHBoxLayout>

namespace NereusSDR {

namespace {
void applyBtnColor(QPushButton* btn, const QColor& c)
{
    btn->setStyleSheet(
        QStringLiteral("QPushButton { background: %1; border: 1px solid #205070; }")
            .arg(c.name(QColor::HexArgb)));
}
} // namespace

WebImageItemEditor::WebImageItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void WebImageItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    WebImageItem* w = qobject_cast<WebImageItem*>(item);
    if (!w) { return; }

    beginProgrammaticUpdate();
    m_editUrl->setText(w->url());
    m_spinRefresh->setValue(w->refreshInterval());
    applyBtnColor(m_btnFallback, w->fallbackColor());
    endProgrammaticUpdate();
}

void WebImageItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Web Image"));

    // URL + reload button in a horizontal layout
    auto* rowWidget = new QWidget(this);
    auto* rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(4);

    m_editUrl = new QLineEdit(rowWidget);
    m_editUrl->setStyleSheet(
        QStringLiteral("QLineEdit { background: #0a0a18; color: #c8d8e8;"
                       " border: 1px solid #1e2e3e; border-radius: 3px;"
                       " padding: 1px 4px; min-height: 18px; }"));
    rowLayout->addWidget(m_editUrl, 1);

    auto* btnReload = new QPushButton(QStringLiteral("Reload"), rowWidget);
    btnReload->setFixedWidth(48);
    rowLayout->addWidget(btnReload);

    addRow(QStringLiteral("URL"), rowWidget);

    connect(m_editUrl, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        WebImageItem* w = qobject_cast<WebImageItem*>(m_item);
        if (!w) { return; }
        w->setUrl(m_editUrl->text());
        notifyChanged();
    });
    connect(btnReload, &QPushButton::clicked, this, [this]() {
        WebImageItem* w = qobject_cast<WebImageItem*>(m_item);
        if (!w) { return; }
        w->setUrl(m_editUrl->text()); // triggers a fetch
        notifyChanged();
    });

    // Refresh interval
    m_spinRefresh = makeIntRow(QStringLiteral("Refresh (s)"), 1, 3600);
    connect(m_spinRefresh, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (isProgrammaticUpdate()) { return; }
        WebImageItem* w = qobject_cast<WebImageItem*>(m_item);
        if (!w) { return; }
        w->setRefreshInterval(v);
        notifyChanged();
    });

    // Fallback color
    m_btnFallback = new QPushButton(this);
    m_btnFallback->setFixedSize(40, 18);
    connect(m_btnFallback, &QPushButton::clicked, this, [this]() {
        WebImageItem* w = qobject_cast<WebImageItem*>(m_item);
        if (!w) { return; }
        const QColor chosen = QColorDialog::getColor(
            w->fallbackColor(), this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            w->setFallbackColor(chosen);
            applyBtnColor(m_btnFallback, chosen);
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Fallback color"), m_btnFallback);
}

} // namespace NereusSDR
