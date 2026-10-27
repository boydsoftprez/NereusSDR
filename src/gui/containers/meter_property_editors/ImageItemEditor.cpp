#include "ImageItemEditor.h"
#include "../../meters/MeterItem.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QWidget>

namespace NereusSDR {

ImageItemEditor::ImageItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void ImageItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    ImageItem* img = qobject_cast<ImageItem*>(item);
    if (!img) { return; }

    beginProgrammaticUpdate();
    m_editPath->setText(img->imagePath());
    m_chkDarkMode->setChecked(img->darkMode());
    endProgrammaticUpdate();
    refreshDarkModeAvailability();
}

// Dark mode does something only when the image has a "-dark" file beside
// it (the ANAN face has none): otherwise it stays visible, disabled, with
// the reason. A saved Dark mode is kept and shown.
void ImageItemEditor::refreshDarkModeAvailability()
{
    const ImageItem* img = qobject_cast<const ImageItem*>(m_item);
    const QString path = img ? img->imagePath() : QString();
    const bool available = !path.isEmpty()
        && ImageItem::variantPath(path, true, 0.0) != ImageItem::variantPath(path, false, 0.0);
    m_chkDarkMode->setEnabled(available);
    m_chkDarkMode->setToolTip(available
        ? QStringLiteral("Use the image's -dark file (for example meter-dark.png) when there is one")
        : noArtworkReason());
}

void ImageItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Image"));

    // Row widget: QLineEdit + Browse button side by side
    QWidget* rowWidget = new QWidget(this);
    QHBoxLayout* hbox = new QHBoxLayout(rowWidget);
    hbox->setContentsMargins(0, 0, 0, 0);
    hbox->setSpacing(3);

    m_editPath = new QLineEdit(rowWidget);
    m_editPath->setPlaceholderText(QStringLiteral("(none)"));
    hbox->addWidget(m_editPath, 1);

    m_btnBrowse = new QPushButton(QStringLiteral("Browse\u2026"), rowWidget);
    m_btnBrowse->setFixedWidth(54);
    hbox->addWidget(m_btnBrowse);

    addRow(QStringLiteral("Image path"), rowWidget);

    // Dark mode: use the image's "-dark" file when there is one beside it.
    m_chkDarkMode = makeCheckRow(QStringLiteral("Dark mode"));
    m_chkDarkMode->setObjectName(QStringLiteral("imageDarkMode"));
    m_chkDarkMode->setToolTip(QStringLiteral(
        "Use the image's -dark file (for example meter-dark.png) when there is one"));
    connect(m_chkDarkMode, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        ImageItem* img = qobject_cast<ImageItem*>(m_item);
        if (!img) { return; }
        img->setDarkMode(on);
        notifyChanged();
    });

    // Apply path when the user finishes editing the line edit directly
    connect(m_editPath, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        ImageItem* img = qobject_cast<ImageItem*>(m_item);
        if (!img) { return; }
        img->setImagePath(m_editPath->text());
        refreshDarkModeAvailability();
        notifyChanged();
    });

    // Browse button opens a file dialog
    connect(m_btnBrowse, &QPushButton::clicked, this, [this]() {
        ImageItem* img = qobject_cast<ImageItem*>(m_item);
        if (!img) { return; }
        const QString path = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("Select image"),
            img->imagePath(),
            QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.svg);;All files (*)"));
        if (!path.isEmpty()) {
            img->setImagePath(path);
            beginProgrammaticUpdate();
            m_editPath->setText(path);
            endProgrammaticUpdate();
            refreshDarkModeAvailability();
            notifyChanged();
        }
    });
}

} // namespace NereusSDR
