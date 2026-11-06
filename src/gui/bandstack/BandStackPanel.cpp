// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/bandstack/BandStackPanel.cpp  (NereusSDR)
// =================================================================
//
// See BandStackPanel.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include "gui/bandstack/BandStackPanel.h"

#include <QEasingCurve>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include "core/AppSettings.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/widgets/VfoWidget.h"
#include "models/Band.h"
#include "models/SliceModel.h"
#include "models/bandstack/BandStackAccess.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace NereusSDR {

namespace {

constexpr int kGripWidth = 5;
constexpr int kHeaderHeight = 24;
constexpr int kSliceRowHeight = 27;
constexpr int kChipSize = 18;

std::optional<bool> g_reducedMotionForTest;

bool reducedMotion()
{
    return g_reducedMotionForTest.value_or(systemPrefersReducedMotion());
}

QString settingText(bool on)
{
    return on ? QStringLiteral("True") : QStringLiteral("False");
}

bool settingOn(const char* key, bool fallback)
{
    return AppSettings::instance()
               .value(QString::fromLatin1(key), settingText(fallback))
               .toString()
        == QStringLiteral("True");
}

void saveSetting(const char* key, const QVariant& value)
{
    AppSettings::instance().setValue(QString::fromLatin1(key), value);
}

QString colourName(const QColor& legacy, const char* role)
{
    return WorkspacePreview::roleColour(legacy, role).name();
}

} // namespace

#if !defined(Q_OS_MAC)
bool systemPrefersReducedMotion()
{
#ifdef Q_OS_WIN
    BOOL animations = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0)) {
        return animations == FALSE;
    }
#endif
    return false;
}
#endif

void BandStackPanel::setReducedMotionForTest(std::optional<bool> reduced)
{
    g_reducedMotionForTest = reduced;
}

BandStackPanel::BandStackPanel(BandStackAccess* access, QWidget* parent)
    : QWidget(parent)
    , m_access(access)
{
    setObjectName(QStringLiteral("bandStackPanel"));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    m_content = new QWidget(this);
    m_content->setObjectName(QStringLiteral("bandStackPanelContent"));
    // The stylesheet's background and border need a styled surface.
    m_content->setAttribute(Qt::WA_StyledBackground, true);
    auto* contentLayout = new QVBoxLayout(m_content);
    contentLayout->setContentsMargins(0, 0, 1, 0);
    contentLayout->setSpacing(0);

    auto* header = new QWidget(m_content);
    header->setObjectName(QStringLiteral("bandStackPanelHeader"));
    header->setAttribute(Qt::WA_StyledBackground, true);
    header->setFixedHeight(kHeaderHeight);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 0, 5, 0);
    headerLayout->setSpacing(5);
    auto* title = new QLabel(QStringLiteral("BAND STACK"), header);
    title->setObjectName(QStringLiteral("bandStackPanelTitle"));
    headerLayout->addWidget(title);
    headerLayout->addStretch(1);
    m_menuButton = new QToolButton(header);
    m_menuButton->setObjectName(QStringLiteral("bandStackPanelMenu"));
    m_menuButton->setText(QStringLiteral("⋯"));
    m_menuButton->setToolTip(QStringLiteral("Band stack options"));
    m_menuButton->setAutoRaise(true);
    connect(m_menuButton, &QToolButton::clicked, this, &BandStackPanel::menuRequested);
    headerLayout->addWidget(m_menuButton);
    m_closeButton = new QToolButton(header);
    m_closeButton->setObjectName(QStringLiteral("bandStackPanelClose"));
    m_closeButton->setText(QStringLiteral("×"));
    m_closeButton->setToolTip(QStringLiteral("Close"));
    m_closeButton->setAutoRaise(true);
    connect(m_closeButton, &QToolButton::clicked, this, &BandStackPanel::closeRequested);
    headerLayout->addWidget(m_closeButton);
    contentLayout->addWidget(header);

    m_sliceRow = new QWidget(m_content);
    m_sliceRow->setObjectName(QStringLiteral("bandStackPanelSliceRow"));
    m_sliceRow->setAttribute(Qt::WA_StyledBackground, true);
    m_sliceRow->setFixedHeight(kSliceRowHeight);
    auto* sliceLayout = new QHBoxLayout(m_sliceRow);
    sliceLayout->setContentsMargins(8, 0, 8, 0);
    sliceLayout->setSpacing(7);
    m_chip = new QLabel(m_sliceRow);
    m_chip->setObjectName(QStringLiteral("bandStackPanelSliceFlag"));
    m_chip->setFixedSize(kChipSize, kChipSize);
    m_chip->setAlignment(Qt::AlignCenter);
    sliceLayout->addWidget(m_chip);
    m_onBand = new QLabel(m_sliceRow);
    m_onBand->setObjectName(QStringLiteral("bandStackPanelOnBand"));
    sliceLayout->addWidget(m_onBand, 1);
    contentLayout->addWidget(m_sliceRow);

    m_reasonLabel = new QLabel(m_content);
    m_reasonLabel->setObjectName(QStringLiteral("bandStackPanelReason"));
    m_reasonLabel->setWordWrap(true);
    m_reasonLabel->setContentsMargins(8, 6, 8, 6);
    m_reasonLabel->hide();
    contentLayout->addWidget(m_reasonLabel);

    m_rowsScroll = new QScrollArea(m_content);
    m_rowsScroll->setObjectName(QStringLiteral("bandStackPanelRows"));
    m_rowsScroll->setFrameShape(QFrame::NoFrame);
    m_rowsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_rowsScroll->setWidgetResizable(true);
    auto* rowsHost = new QWidget(m_rowsScroll);
    rowsHost->setObjectName(QStringLiteral("bandStackPanelRowsHost"));
    m_rowsLayout = new QVBoxLayout(rowsHost);
    m_rowsLayout->setContentsMargins(3, 2, 3, 8);
    m_rowsLayout->setSpacing(0);
    m_rowsLayout->setAlignment(Qt::AlignTop);
    m_rowsScroll->setWidget(rowsHost);
    contentLayout->addWidget(m_rowsScroll, 1);

    m_grip = new QWidget(this);
    m_grip->setObjectName(QStringLiteral("bandStackPanelGrip"));
    m_grip->setCursor(Qt::SizeHorCursor);
    m_grip->setToolTip(QStringLiteral("Drag to change the panel's width"));
    m_grip->installEventFilter(this);

    m_slide = new QVariantAnimation(this);
    m_slide->setDuration(kSlideMs);
    m_slide->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_slide, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value) { applyShownWidth(value.toInt()); });

    applyStyle();
    refreshHeader();

    bool widthRead = false;
    const int saved = AppSettings::instance()
                          .value(QString::fromLatin1(kWidthKey), kDefaultWidth)
                          .toInt(&widthRead);
    m_panelWidth = qBound(kMinWidth, widthRead ? saved : kDefaultWidth, kMaxWidth);
    m_open = settingOn(kOpenKey, false);
    applyShownWidth(m_open ? m_panelWidth : 0);
}

BandStackAccess* BandStackPanel::access() const
{
    return m_access;
}

SliceModel* BandStackPanel::slice() const
{
    return m_slice;
}

void BandStackPanel::setAccess(BandStackAccess* access)
{
    m_access = access;
}

void BandStackPanel::applyStyle()
{
    const QString panel = colourName(QColor(0x0f, 0x0f, 0x1a), "panel");
    const QString subtle = colourName(QColor(0x20, 0x30, 0x40), "border-subtle");
    const QString title = colourName(QColor(0xc8, 0xd8, 0xe8), "text");
    const QString secondary = colourName(QColor(0x8a, 0xa8, 0xc0), "text-secondary");
    const QString disabled = colourName(QColor(0x40, 0x48, 0x58), "disabled-text");
    setStyleSheet(QStringLiteral(
        "#bandStackPanelContent { background: %1; border-right: 1px solid %2; }"
        "#bandStackPanelHeader, #bandStackPanelSliceRow {"
        " background: %1; border-bottom: 1px solid %2; }"
        "#bandStackPanelTitle { color: %3; font-size: 11px; font-weight: 600;"
        " letter-spacing: 1px; }"
        "#bandStackPanelOnBand { color: %4; font-size: 11px; }"
        "#bandStackPanelOnBand:disabled { color: %5; }"
        "#bandStackPanelReason { color: %4; font-size: 11px; }"
        "#bandStackPanelRows, #bandStackPanelRowsHost { background: %1; border: none; }"
        "#bandStackPanelMenu, #bandStackPanelClose { color: %4; background: transparent;"
        " border: none; font-size: 12px; font-weight: 600; padding: 0 2px; }"
        "#bandStackPanelMenu:hover, #bandStackPanelClose:hover { color: %3; }"
        "#bandStackPanelMenu:disabled { color: %5; }")
        .arg(panel, subtle, title, secondary, disabled));
}

void BandStackPanel::setSlice(SliceModel* slice)
{
    if (m_slice == slice) {
        refreshHeader();
        return;
    }
    disconnect(m_sliceFrequency);
    m_slice = slice;
    if (slice) {
        m_sliceFrequency = connect(slice, &SliceModel::frequencyChanged, this,
                                   [this] { refreshHeader(); });
    }
    refreshHeader();
}

void BandStackPanel::refreshHeader()
{
    const bool usable = m_reason.isEmpty();
    if (!m_slice) {
        m_chip->setText(QStringLiteral("-"));
        m_chip->setToolTip(QString());
        m_chip->setStyleSheet(QStringLiteral(
            "QLabel { border: 1px solid %1; border-radius: 3px; color: %1;"
            " font-size: 11px; font-weight: 700; }")
            .arg(colourName(QColor(0x40, 0x48, 0x58), "text-inactive")));
        m_onBand->setText(QStringLiteral("No slice"));
        return;
    }
    const QChar letter = m_slice->sliceLetter();
    m_chip->setText(QString(letter));
    m_chip->setToolTip(QStringLiteral("Slice %1").arg(letter));
    const QColor flag = usable ? VfoWidget::sliceColor(m_slice->sliceIndex())
                               : WorkspacePreview::roleColour(QColor(0x40, 0x48, 0x58), "text-inactive");
    m_chip->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; border-radius: 3px; color: #ffffff;"
        " font-size: 11px; font-weight: 700; }").arg(flag.name()));
    m_onBand->setText(QStringLiteral("on %1").arg(bandLabel(bandFromFrequency(m_slice->frequency()))));
}

void BandStackPanel::setUnavailableReason(const QString& reason)
{
    m_reason = reason;
    const bool usable = reason.isEmpty();
    m_reasonLabel->setText(reason);
    m_reasonLabel->setVisible(!usable);
    m_rowsScroll->setEnabled(usable);
    m_sliceRow->setEnabled(usable);
    m_menuButton->setEnabled(usable);
    m_menuButton->setToolTip(usable ? QStringLiteral("Band stack options") : reason);
    m_rowsScroll->setToolTip(reason);
    m_sliceRow->setToolTip(reason);
    setToolTip(reason);
    refreshHeader();
}

void BandStackPanel::setOpen(bool open, bool animated)
{
    if (m_open != open) {
        m_open = open;
        saveSetting(kOpenKey, settingText(open));
        emit openChanged(open);
    }
    const int target = m_open ? m_panelWidth : 0;
    m_slide->stop();
    if (!animated || reducedMotion() || width() == target) {
        applyShownWidth(target);
        return;
    }
    show();
    m_slide->setStartValue(width());
    m_slide->setEndValue(target);
    m_slide->start();
}

void BandStackPanel::applyShownWidth(int width)
{
    setFixedWidth(width);
    setVisible(width > 0);
    placeChildren();
}

void BandStackPanel::setPanelWidth(int width)
{
    const int bounded = qBound(kMinWidth, width, kMaxWidth);
    if (bounded == m_panelWidth) {
        return;
    }
    m_panelWidth = bounded;
    saveSetting(kWidthKey, bounded);
    if (m_open) {
        m_slide->stop();
        applyShownWidth(bounded);
    }
}

bool BandStackPanel::groupByBand() const
{
    return settingOn(kGroupByBandKey, true);
}

void BandStackPanel::setGroupByBand(bool on)
{
    saveSetting(kGroupByBandKey, settingText(on));
}

bool BandStackPanel::showOnSpectrum() const
{
    return settingOn(kShowOnSpectrumKey, false);
}

void BandStackPanel::setShowOnSpectrum(bool on)
{
    saveSetting(kShowOnSpectrumKey, settingText(on));
}

void BandStackPanel::placeChildren()
{
    // The content keeps the open width while the panel slides, so the rows are
    // uncovered rather than squeezed.
    m_content->setGeometry(0, 0, m_panelWidth, height());
    m_grip->setGeometry(width() - kGripWidth, 0, kGripWidth, height());
    m_grip->raise();
}

void BandStackPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    placeChildren();
}

bool BandStackPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_grip) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            const auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton) {
                m_gripActive = true;
                m_gripStartWidth = m_panelWidth;
                m_gripStartX = qRound(mouse->globalPosition().x());
                return true;
            }
            break;
        }
        case QEvent::MouseMove:
            if (m_gripActive) {
                const auto* mouse = static_cast<QMouseEvent*>(event);
                setPanelWidth(m_gripStartWidth + qRound(mouse->globalPosition().x()) - m_gripStartX);
                return true;
            }
            break;
        case QEvent::MouseButtonRelease:
            if (m_gripActive) {
                m_gripActive = false;
                return true;
            }
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace NereusSDR
