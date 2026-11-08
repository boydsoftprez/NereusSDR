// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/bandstack/BandStackPopup.cpp  (NereusSDR)
// =================================================================
//
// See BandStackPopup.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include "gui/bandstack/BandStackPopup.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

#include "gui/bandstack/BandStackRowList.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/widgets/VfoWidget.h"
#include "models/SliceModel.h"
#include "models/bandstack/BandStackAccess.h"

namespace NereusSDR {

namespace {

// desktop.html's .bpop and .bpop .bh.
constexpr int kHeaderHeight = 24;
constexpr int kChipSize = 18;
constexpr int kBorder = 1;
constexpr int kPadding = 3;

QString colourName(const QColor& legacy, const char* role)
{
    return WorkspacePreview::roleColour(legacy, role).name();
}

} // namespace

BandStackPopup::BandStackPopup(BandStackAccess* access, QWidget* parent)
    : QWidget(parent, Qt::Popup)
{
    setObjectName(QStringLiteral("bandStackPopup"));
    setAttribute(Qt::WA_StyledBackground, true);

    const QString panel = colourName(QColor(0x0f, 0x0f, 0x1a), "panel");
    const QString border = colourName(QColor(0x20, 0x30, 0x40), "border");
    const QString title = colourName(QColor(0xc8, 0xd8, 0xe8), "text");
    const QString secondary = colourName(QColor(0x8a, 0xa8, 0xc0), "text-secondary");
    const QString inactive = colourName(QColor(0x40, 0x48, 0x58), "text-inactive");
    setStyleSheet(QStringLiteral(
        "#bandStackPopup { background: %1; border: 1px solid %2; border-radius: 5px; }"
        "#bandStackPopupTitle { color: %3; font-size: 11px; font-weight: 600;"
        " letter-spacing: 1px; }"
        "#bandStackPopupCount { color: %5; font-size: 11px; }"
        "#bandStackPopupReason { color: %4; font-size: 11px; }"
        "#bandStackPopupRows, #bandStackPopupRowsHost { background: %1; border: none; }")
        .arg(panel, border, title, secondary, inactive));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kBorder + kPadding, kBorder, kBorder + kPadding, kBorder + kPadding);
    layout->setSpacing(0);

    auto* header = new QWidget(this);
    header->setFixedHeight(kHeaderHeight);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(4, 0, 4, 0);
    headerLayout->setSpacing(5);
    m_chip = new QLabel(header);
    m_chip->setObjectName(QStringLiteral("bandStackPopupSliceFlag"));
    m_chip->setFixedSize(kChipSize, kChipSize);
    m_chip->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(m_chip);
    m_title = new QLabel(header);
    m_title->setObjectName(QStringLiteral("bandStackPopupTitle"));
    headerLayout->addWidget(m_title);
    headerLayout->addStretch(1);
    m_count = new QLabel(header);
    m_count->setObjectName(QStringLiteral("bandStackPopupCount"));
    headerLayout->addWidget(m_count);
    layout->addWidget(header);

    m_reasonLabel = new QLabel(this);
    m_reasonLabel->setObjectName(QStringLiteral("bandStackPopupReason"));
    m_reasonLabel->setWordWrap(true);
    m_reasonLabel->setContentsMargins(4, 2, 4, 4);
    m_reasonLabel->hide();
    layout->addWidget(m_reasonLabel);

    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName(QStringLiteral("bandStackPopupRows"));
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setWidgetResizable(true);
    auto* rowsHost = new QWidget(m_scroll);
    rowsHost->setObjectName(QStringLiteral("bandStackPopupRowsHost"));
    auto* rowsLayout = new QVBoxLayout(rowsHost);
    rowsLayout->setContentsMargins(0, 0, 0, 0);
    rowsLayout->setSpacing(0);
    rowsLayout->setAlignment(Qt::AlignTop);
    m_rows = new BandStackRowList(access, rowsHost);
    rowsLayout->addWidget(m_rows);
    m_scroll->setWidget(rowsHost);
    layout->addWidget(m_scroll, 1);

    connect(m_rows, &BandStackRowList::closeRequested, this, &QWidget::close);
    // The "+ Add current spot" row comes and goes with the band the slice is on.
    connect(m_rows, &BandStackRowList::contentHeightChanged, this, &BandStackPopup::fitAndPlace);

    setAccess(access);
}

void BandStackPopup::setAccess(BandStackAccess* access)
{
    disconnect(m_document);
    m_access = access;
    m_rows->setAccess(access);
    if (access) {
        // After the rows' own reload, which was connected first.
        m_document = connect(access, &BandStackAccess::documentChanged,
                             this, &BandStackPopup::refreshHeader);
    }
    refreshHeader();
}

void BandStackPopup::setUnavailableReason(const QString& reason)
{
    m_reason = reason;
    refreshContent();
}

QString BandStackPopup::shownReason() const
{
    if (!m_reason.isEmpty()) {
        return m_reason;
    }
    return m_slice ? QString() : QStringLiteral("Select a slice first.");
}

void BandStackPopup::showFor(SliceModel* slice, Band band, const QPoint& globalPos)
{
    disconnect(m_sliceFrequency);
    m_slice = slice;
    m_band = band;
    m_anchor = globalPos;
    if (slice) {
        m_sliceFrequency = connect(slice, &SliceModel::frequencyChanged, this, [this] {
            if (m_slice) {
                m_rows->setSliceFrequency(m_slice->frequency());
            }
        });
    }
    m_rows->setSliceId(slice ? slice->sliceIndex() : -1);
    m_rows->setSliceFrequency(slice ? std::optional<double>(slice->frequency()) : std::nullopt);
    m_rows->setBand(band);
    refreshContent();
    show();
    m_scroll->verticalScrollBar()->setValue(0);
    m_rows->revealPlaceSoon();
    m_rows->setFocus(Qt::PopupFocusReason);
}

void BandStackPopup::refreshHeader()
{
    const bool usable = shownReason().isEmpty();
    const QColor flag = m_slice && m_reason.isEmpty()
        ? VfoWidget::sliceColor(m_slice->sliceIndex())
        : WorkspacePreview::roleColour(QColor(0x40, 0x48, 0x58), "text-inactive");
    m_chip->setText(m_slice ? QString(m_slice->sliceLetter()) : QStringLiteral("-"));
    m_chip->setToolTip(m_slice ? QStringLiteral("Slice %1").arg(m_slice->sliceLetter()) : QString());
    m_chip->setStyleSheet(QStringLiteral(
        "QLabel { background: %1; border-radius: 3px; color: #ffffff;"
        " font-size: 11px; font-weight: 700; }").arg(flag.name()));
    m_title->setText(m_band ? bandLabel(*m_band) : QString());
    m_count->setText(usable && m_band ? QString::number(m_rows->entryCount(*m_band)) : QString());
}

void BandStackPopup::refreshContent()
{
    const QString reason = shownReason();
    m_reasonLabel->setText(reason);
    m_reasonLabel->setVisible(!reason.isEmpty());
    m_scroll->setVisible(reason.isEmpty());
    refreshHeader();
    fitAndPlace();
}

void BandStackPopup::fitAndPlace()
{
    const int inner = kWidth - 2 * (kBorder + kPadding);
    const int content = m_reasonLabel->isHidden() ? m_rows->sizeHint().height()
                                                  : m_reasonLabel->heightForWidth(inner);
    const int height = qMin(kBorder + kHeaderHeight + content + kBorder + kPadding, kMaxHeight);
    setFixedSize(kWidth, height);

    // desktop.html's Math.min(p.x+2, 1512-196): beside the pointer, or moved in
    // far enough to stay on the screen.
    const QScreen* screen = QGuiApplication::screenAt(m_anchor);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    QPoint place = m_anchor + QPoint(kPointerGap, kPointerGap);
    if (screen) {
        const QRect area = screen->availableGeometry();
        place.setX(qMax(area.left(), qMin(place.x(), area.right() + 1 - kWidth - kScreenGap)));
        place.setY(qMax(area.top(), qMin(place.y(), area.bottom() + 1 - height - kScreenGap)));
    }
    move(place);
}

} // namespace NereusSDR
