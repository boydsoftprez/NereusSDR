// =================================================================
// src/gui/widgets/RadioSpeakerWidget.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original widget. See RadioSpeakerWidget.h.
//
// Modification history (NereusSDR):
//   2026-10-06 - Written for the radio speaker and Audio Setup plan,
//                Task 6 (R-SPK-06, R-SPK-07, R-SPK-16, R-SPK-17, D1, D5,
//                D10). J.J. Boyd (KG4VCF), with AI-assisted implementation
//                via Anthropic Claude Code.
//   2026-10-06 - Task 6, JJ decision 2 (R-SPK-17, D1): setStacked() for
//                the header's stacked form. J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "RadioSpeakerWidget.h"

#include "gui/widgets/AppIcon.h"
#include "gui/widgets/MasterOutputWidget.h"
#include "models/RadioModel.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSize>
#include <QSlider>

namespace NereusSDR {

namespace {

constexpr int kButtonPx = 20;
constexpr int kSliderWidth = 100;
constexpr int kSliderHeight = 16;
constexpr int kReadoutWidth = 22;
constexpr int kSpacing = 4;

const char* const kRadioOnIcon    = "radio-on";
const char* const kRadioMutedIcon = "radio-muted";
const char* const kRadioNoneIcon  = "radio-none";

} // namespace

RadioSpeakerWidget::RadioSpeakerWidget(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_iconPx(HeaderVolumeStyle::kIconPx)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(kSpacing);

    m_button = new QPushButton(this);
    m_button->setObjectName(QStringLiteral("radioSpeakerBtn"));
    m_button->setFixedSize(kButtonPx, kButtonPx);
    m_button->setCheckable(true);
    m_button->setStyleSheet(QLatin1String(HeaderVolumeStyle::kIconButton));
    m_button->setAccessibleName(tr("Mute radio speaker"));
    layout->addWidget(m_button);

    m_label = new QLabel(QStringLiteral("RADIO"), this);
    m_label->setObjectName(QStringLiteral("radioLabel"));
    m_label->setStyleSheet(QLatin1String(HeaderVolumeStyle::kWordLabel));
    layout->addWidget(m_label);

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setObjectName(QStringLiteral("radioSlider"));
    m_slider->setRange(0, 100);
    m_slider->setFixedWidth(kSliderWidth);
    m_slider->setFixedHeight(kSliderHeight);
    m_slider->setStyleSheet(QLatin1String(HeaderVolumeStyle::kRadioSlider));
    m_slider->setAccessibleName(tr("Radio speaker volume"));
    m_slider->setAccessibleDescription(tr("Radio speaker volume, 0 to 100"));
    layout->addWidget(m_slider);

    m_valueLabel = new QLabel(this);
    m_valueLabel->setObjectName(QStringLiteral("radioValueLabel"));
    m_valueLabel->setFixedWidth(kReadoutWidth);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    m_valueLabel->setStyleSheet(QLatin1String(HeaderVolumeStyle::kReadout));
    layout->addWidget(m_valueLabel);

    for (QWidget* w : {static_cast<QWidget*>(this), static_cast<QWidget*>(m_button),
                       static_cast<QWidget*>(m_label), static_cast<QWidget*>(m_slider),
                       static_cast<QWidget*>(m_valueLabel)}) {
        w->installEventFilter(this);
    }

    // ── Widget -> model ────────────────────────────────────────────────
    connect(m_slider, &QSlider::valueChanged, this, [this](int value) {
        if (m_updatingFromModel) {
            return;
        }
        m_valueLabel->setText(QString::number(value));
        if (m_model) {
            m_model->setRadioSpeakerVolume(value);
        }
        // The model may clamp or refuse; show what it holds.
        syncFromModel();
    });
    connect(m_button, &QPushButton::toggled, this, [this](bool muted) {
        if (m_updatingFromModel) {
            return;
        }
        if (m_model) {
            m_model->setRadioSpeakerMuted(muted);
        }
        syncFromModel();
    });

    setRadioModel(model);
}

void RadioSpeakerWidget::setRadioModel(RadioModel* model)
{
    if (m_model) {
        disconnect(m_model, nullptr, this, nullptr);
    }
    m_model = model;
    if (m_model) {
        // ── Model -> widget, from any source ───────────────────────────
        connect(m_model, &RadioModel::radioSpeakerVolumeChanged,
                this, &RadioSpeakerWidget::syncFromModel);
        connect(m_model, &RadioModel::radioSpeakerMutedChanged,
                this, &RadioSpeakerWidget::syncFromModel);
        connect(m_model, &RadioModel::radioSpeakerAvailabilityChanged,
                this, &RadioSpeakerWidget::syncFromModel);
        // The reason can change with the connection while availability
        // stays 0 (a remote window's Core link coming or going).
        connect(m_model, &RadioModel::connectionStateChanged,
                this, &RadioSpeakerWidget::syncFromModel);
        // Destroyed before this widget: drop back to no radio.
        connect(m_model, &QObject::destroyed, this, [this]() {
            m_model = nullptr;
            syncFromModel();
        });
    }
    syncFromModel();
}

void RadioSpeakerWidget::syncFromModel()
{
    const bool available = m_model
        && m_model->radioSpeakerAvailability() != RadioModel::kRadioSpeakerNoRadio;
    const int volume = m_model ? m_model->radioSpeakerVolume() : 0;
    const bool muted = m_model && m_model->radioSpeakerMuted();

    m_updatingFromModel = true;
    {
        const QSignalBlocker sliderBlock(m_slider);
        const QSignalBlocker buttonBlock(m_button);
        m_slider->setValue(volume);
        m_button->setChecked(muted);
    }
    m_updatingFromModel = false;

    // D10: disabled, never hidden.
    m_button->setEnabled(available);
    m_slider->setEnabled(available);
    m_label->setEnabled(available);
    m_valueLabel->setEnabled(available);
    m_valueLabel->setText(available ? QString::number(volume) : QStringLiteral("--"));
    applyIcon(available, muted);
    refreshToolTip();
}

void RadioSpeakerWidget::refreshToolTip()
{
    const QString tip = m_model ? m_model->radioSpeakerToolTip()
                                : tr("No radio connected");
    for (QWidget* w : {static_cast<QWidget*>(this), static_cast<QWidget*>(m_button),
                       static_cast<QWidget*>(m_label), static_cast<QWidget*>(m_slider),
                       static_cast<QWidget*>(m_valueLabel)}) {
        if (w->toolTip() != tip) {
            w->setToolTip(tip);
        }
    }
}

void RadioSpeakerWidget::applyIcon(bool available, bool muted)
{
    const QString name = QLatin1String(!available ? kRadioNoneIcon
                                       : muted    ? kRadioMutedIcon
                                                  : kRadioOnIcon);
    if (m_button->property(AppIcon::kIconProperty).toString() == name
        && m_button->iconSize() == QSize(m_iconPx, m_iconPx)) {
        return;
    }
    AppIcon::apply(m_button, name, m_iconPx);
    if (!available) {
        // A disabled button would otherwise draw Qt's own greyed copy of
        // the icon; radio-none is already the greyed radio.
        QIcon icon = m_button->icon();
        for (const qreal ratio : {1.0, 2.0, m_button->devicePixelRatioF()}) {
            icon.addPixmap(AppIcon::pixmap(name, m_iconPx, ratio),
                           QIcon::Disabled);
        }
        m_button->setIcon(icon);
    }
}

void RadioSpeakerWidget::setStacked(bool stacked, int stackedLabelWidth)
{
    m_stacked = stacked;
    m_iconPx = HeaderVolumeStyle::applyForm(m_button, m_label, m_slider, m_valueLabel,
                                            stacked, stackedLabelWidth,
                                            HeaderVolumeStyle::kReadout);
    syncFromModel();
}

bool RadioSpeakerWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::ToolTip) {
        refreshToolTip();
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace NereusSDR
