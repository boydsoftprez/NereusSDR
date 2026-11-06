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
//   2026-11-06 - The rows, the band menu and the panel menu (band stack plan
//                 Task 9). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-11-06 - Says when Show on the spectrum changes (band stack plan Task
//                 11). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/bandstack/BandStackPanel.h"

#include <QAction>
#include <QDialog>
#include <QEasingCurve>
#include <QEvent>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include "core/AppSettings.h"
#include "gui/ScopedChildWidget.h"
#include "gui/bandstack/BandStackRowList.h"
#include "gui/styles/PopupMenuStyle.h"
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
    connect(this, &BandStackPanel::menuRequested, this, [this] {
        showPanelMenu(m_menuButton->mapToGlobal(m_menuButton->rect().bottomLeft()));
    });
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
    m_rows = new BandStackRowList(access, rowsHost);
    m_rows->setGrouped(groupByBand());
    m_rowsLayout->addWidget(m_rows);
    connect(m_rows, &BandStackRowList::bandContextRequested, this, &BandStackPanel::showBandMenu);
    connect(m_rows, &BandStackRowList::panelContextRequested, this, &BandStackPanel::showPanelMenu);
    connect(m_rows, &BandStackRowList::closeRequested, this, &BandStackPanel::closeRequested);
    connect(m_rows, &BandStackRowList::currentBandChanged, this, &BandStackPanel::refreshOnBand);
    // The empty part of the list below the last row.
    rowsHost->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(rowsHost, &QWidget::customContextMenuRequested, this, [this, rowsHost](const QPoint& pos) {
        showPanelMenu(rowsHost->mapToGlobal(pos));
    });
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
    m_rows->setAccess(access);
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
        m_rows->setSliceId(-1);
        m_rows->setSliceFrequency(std::nullopt);
        refreshOnBand();
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
    m_rows->setSliceId(m_slice->sliceIndex());
    m_rows->setSliceFrequency(m_slice->frequency());
    refreshOnBand();
}

void BandStackPanel::refreshOnBand()
{
    const std::optional<Band> band = m_rows->currentBand();
    m_onBand->setText(band.has_value() ? QStringLiteral("on %1").arg(bandLabel(*band))
                                       : QStringLiteral("No slice"));
}

void BandStackPanel::fillBandMenu(QMenu& menu, Band band)
{
    menu.setStyleSheet(WorkspacePreview::lookQss(QString::fromLatin1(kPopupMenu), &menu));
    menu.setToolTipsVisible(true);
    const BandStackReturnMode mode = m_rows->returnMode(band);

    QMenu* returnTo = menu.addMenu(QStringLiteral("Return to"));
    returnTo->setToolTipsVisible(true);
    const auto addReturn = [&](const QString& text, BandStackReturnMode target) {
        QAction* action = returnTo->addAction(text);
        action->setCheckable(true);
        action->setChecked(mode == target);
        return action;
    };
    connect(addReturn(QStringLiteral("Current entry"), BandStackReturnMode::Current),
            &QAction::triggered, this, [this, band] {
                if (m_access) {
                    m_access->setReturn(band, BandStackReturnMode::Current, QString());
                }
            });
    connect(addReturn(QStringLiteral("Last frequency used"), BandStackReturnMode::LastUsed),
            &QAction::triggered, this, [this, band] {
                if (m_access) {
                    m_access->setReturn(band, BandStackReturnMode::LastUsed, QString());
                }
            });
    QAction* specific = addReturn(QStringLiteral("Specific entry"), BandStackReturnMode::Specific);
    if (m_rows->entryCount(band) == 0) {
        specific->setEnabled(false);
        specific->setToolTip(QStringLiteral("This band has no entries."));
    }
    connect(specific, &QAction::triggered, this, [this, band] {
        if (m_access) {
            m_access->setReturn(band, BandStackReturnMode::Specific,
                                m_rows->defaultSpecificEntry(band));
        }
    });

    const bool open = m_rows->isBandOpen(band);
    connect(menu.addAction(open ? QStringLiteral("Collapse") : QStringLiteral("Expand")),
            &QAction::triggered, this, [this, band, open] { m_rows->setBandOpen(band, !open); });
    menu.addSeparator();

    const QString label = bandLabel(band);
    connect(menu.addAction(QStringLiteral("Clear unlocked %1 entries").arg(label)),
            &QAction::triggered, this, [this, band, label] {
                if (confirm(QStringLiteral("Clear the unlocked entries on %1?").arg(label),
                            QStringLiteral("Clear"))
                    && m_access) {
                    m_access->clear(band);
                }
            });
}

void BandStackPanel::fillPanelMenu(QMenu& menu)
{
    menu.setStyleSheet(WorkspacePreview::lookQss(QString::fromLatin1(kPopupMenu), &menu));
    menu.setToolTipsVisible(true);
    const QJsonObject document = m_access ? m_access->document() : QJsonObject{};
    const QJsonObject options = document.value(QStringLiteral("options")).toObject();

    QAction* group = menu.addAction(QStringLiteral("Group by band"));
    group->setCheckable(true);
    group->setChecked(groupByBand());
    connect(group, &QAction::triggered, this, [this](bool on) { setGroupByBand(on); });
    menu.addSeparator();

    QMenu* expiry = menu.addMenu(QStringLiteral("Auto-expiry"));
    const int expiryNow = options.value(QStringLiteral("expiryMinutes")).toInt();
    // The values the document allows (design section 6.2).
    for (const int minutes : {0, 5, 15, 30, 60}) {
        QAction* action = expiry->addAction(minutes == 0 ? QStringLiteral("Off")
                                                         : QStringLiteral("%1 min").arg(minutes));
        action->setCheckable(true);
        action->setChecked(minutes == expiryNow);
        connect(action, &QAction::triggered, this, [this, minutes] {
            if (m_access) {
                m_access->options(std::nullopt, minutes, std::nullopt);
            }
        });
    }
    QMenu* dwell = menu.addMenu(QStringLiteral("Auto-save dwell"));
    const int dwellNow = options.value(QStringLiteral("dwellSeconds")).toInt();
    for (const int seconds : {0, 10, 30, 60}) {
        QAction* action = dwell->addAction(seconds == 0 ? QStringLiteral("Off")
                                                        : QStringLiteral("%1 sec").arg(seconds));
        action->setCheckable(true);
        action->setChecked(seconds == dwellNow);
        connect(action, &QAction::triggered, this, [this, seconds] {
            if (m_access) {
                m_access->options(std::nullopt, std::nullopt, seconds);
            }
        });
    }
    menu.addSeparator();

    QAction* dupes = menu.addAction(QStringLiteral("Don't save over a frequency already here"));
    dupes->setCheckable(true);
    dupes->setChecked(options.value(QStringLiteral("dupeProtection")).toBool(true));
    connect(dupes, &QAction::triggered, this, [this](bool on) {
        if (m_access) {
            m_access->options(on, std::nullopt, std::nullopt);
        }
    });
    QAction* spectrum = menu.addAction(QStringLiteral("Show on the spectrum"));
    spectrum->setCheckable(true);
    spectrum->setChecked(showOnSpectrum());
    connect(spectrum, &QAction::triggered, this, [this](bool on) { setShowOnSpectrum(on); });
    menu.addSeparator();

    const QString region = document.value(QStringLiteral("region")).toString();
    connect(menu.addAction(QStringLiteral("Reset to region defaults...")), &QAction::triggered, this,
            [this, region] {
                if (confirm(QStringLiteral("Replace the unlocked entries with the defaults for %1? "
                                           "Your locked entries stay.").arg(region),
                            QStringLiteral("Reset"))
                    && m_access) {
                    m_access->reseed();
                }
            });
    connect(menu.addAction(QStringLiteral("Clear unlocked entries...")), &QAction::triggered, this,
            [this] {
                if (confirm(QStringLiteral("Clear every unlocked entry on every band? Locked "
                                           "entries stay."),
                            QStringLiteral("Clear"))
                    && m_access) {
                    m_access->clear(std::nullopt);
                }
            });
}

void BandStackPanel::showBandMenu(Band band, const QPoint& globalPos)
{
    ScopedChildWidget<QMenu> menuOwner(this);
    QMenu& menu = *menuOwner.get();
    fillBandMenu(menu, band);
    menu.exec(globalPos);
}

void BandStackPanel::showPanelMenu(const QPoint& globalPos)
{
    ScopedChildWidget<QMenu> menuOwner(this);
    QMenu& menu = *menuOwner.get();
    fillPanelMenu(menu);
    menu.exec(globalPos);
}

bool BandStackPanel::confirm(const QString& text, const QString& verb)
{
    ScopedChildWidget<QDialog> dialogOwner(this);
    QDialog& dialog = *dialogOwner.get();
    dialog.setObjectName(QStringLiteral("bandStackConfirm"));
    dialog.setWindowTitle(QStringLiteral("Band stack"));
    dialog.setStyleSheet(WorkspacePreview::lookQss(QStringLiteral(
        "#bandStackConfirm { background: #0f0f1a; }"
        "QLabel { color: #c8d8e8; font-size: 13px; }"
        "QPushButton { color: #c8d8e8; background: #1a2a3a; border: 1px solid #304050;"
        " border-radius: 4px; padding: 4px 12px; font-size: 12px; font-weight: 600; }"
        "#bandStackConfirmAccept { color: #f0dcdc; background: #7a2c2e; border-color: #a86b6d; }"),
        &dialog));
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);
    auto* message = new QLabel(text, &dialog);
    message->setObjectName(QStringLiteral("bandStackConfirmText"));
    message->setWordWrap(true);
    message->setMinimumWidth(388);
    layout->addWidget(message);
    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(6);
    buttons->addStretch(1);
    auto* cancel = new QPushButton(QStringLiteral("Cancel"), &dialog);
    cancel->setObjectName(QStringLiteral("bandStackConfirmCancel"));
    cancel->setDefault(true);
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    buttons->addWidget(cancel);
    auto* accept = new QPushButton(verb, &dialog);
    accept->setObjectName(QStringLiteral("bandStackConfirmAccept"));
    accept->setAutoDefault(false);
    connect(accept, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttons->addWidget(accept);
    layout->addLayout(buttons);
    return dialog.exec() == QDialog::Accepted;
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
        if (open) {
            m_rows->setFocus(Qt::OtherFocusReason);
        }
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
    m_rows->setGrouped(on);
}

bool BandStackPanel::showOnSpectrum() const
{
    return settingOn(kShowOnSpectrumKey, false);
}

void BandStackPanel::setShowOnSpectrum(bool on)
{
    const bool changed = on != showOnSpectrum();
    saveSetting(kShowOnSpectrumKey, settingText(on));
    if (changed) {
        emit showOnSpectrumChanged(on);
    }
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
