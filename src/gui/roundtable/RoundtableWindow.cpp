// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/roundtable/RoundtableWindow.cpp  (NereusSDR)
// =================================================================
//
// See RoundtableWindow.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 13).
// =================================================================

#include "gui/roundtable/RoundtableWindow.h"

#include "core/AppSettings.h"
#include "gui/roundtable/RoundtablePanel.h"
#include "models/RoundtableModel.h"

#include <QCloseEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QScreen>
#include <QVBoxLayout>

#include <algorithm>

namespace NereusSDR {

namespace {

constexpr int kRoomIdRole = Qt::UserRole + 1;

constexpr const char* kListStyle =
    "QListWidget { background: #0c0c0e; border: none; color: #dcdce1; font-size: 12px;"
    "  outline: none; }"
    "QListWidget::item { padding: 4px 8px; border-bottom: 1px solid #1c1c1f; }"
    "QListWidget::item:selected { background: #16222c; color: #ffffff;"
    "  border-left: 2px solid #00b4d8; }"
    "QListWidget::item:disabled { color: #a6a6ac; background: #111114; }";
constexpr const char* kSearchStyle =
    "QLineEdit { background: #0f0f12; border: 1px solid #3f3f42; border-radius: 3px;"
    "  color: #dcdce1; font-size: 12px; padding: 2px 5px; margin: 6px 8px;"
    "  placeholder-text-color: #8e8e93; }"
    "QLineEdit:focus { border-color: #00b4d8; }";

QString frequencyText(qint64 hz)
{
    const QString digits = QString::number(hz).rightJustified(7, QLatin1Char('0'));
    return digits.left(digits.size() - 6) + QLatin1Char('.') + digits.mid(digits.size() - 6, 3)
         + QLatin1Char('.') + digits.right(3);
}

QString familyText(const QString& family)
{
    if (family == QLatin1String("fdvl") || family == QLatin1String("fdvu")) {
        return QStringLiteral("FreeDV");
    }
    return family.toUpper();
}

} // namespace

RoundtableWindow::RoundtableWindow(RoundtableModel* model, QWidget* parent)
    : QWidget(parent, Qt::Window)
    , m_model(model)
{
    setObjectName(QStringLiteral("RoundtableWindow"));
    setWindowTitle(QStringLiteral("Roundtable"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral("QWidget#RoundtableWindow { background: #0d0d10; }"));
    setMinimumSize(300, 320);

    m_body = new QHBoxLayout(this);
    m_body->setContentsMargins(0, 0, 0, 0);
    m_body->setSpacing(0);

    // The room list (mockup .wl).
    m_listColumn = new QWidget(this);
    m_listColumn->setObjectName(QStringLiteral("roundtableRoomListColumn"));
    m_listColumn->setAttribute(Qt::WA_StyledBackground, true);
    m_listColumn->setStyleSheet(QStringLiteral("QWidget#roundtableRoomListColumn { background: #0c0c0e;"
                                               " border-right: 1px solid #1f1f23; }"));
    auto* listLayout = new QVBoxLayout(m_listColumn);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(0);
    auto* head = new QWidget(m_listColumn);
    head->setObjectName(QStringLiteral("roundtableRoomListHead"));
    head->setAttribute(Qt::WA_StyledBackground, true);
    head->setStyleSheet(QStringLiteral("QWidget#roundtableRoomListHead { border-bottom: 1px solid #1f1f23; }"));
    auto* headRow = new QHBoxLayout(head);
    headRow->setContentsMargins(8, 6, 8, 6);
    auto* title = new QLabel(QStringLiteral("FREQUENCIES"), head);
    title->setStyleSheet(QStringLiteral("color: #a6a6ac; font-size: 11px; font-weight: bold;"));
    QFont spaced = title->font();
    spaced.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    title->setFont(spaced);
    headRow->addWidget(title, 1);
    m_onAir = new QLabel(head);
    m_onAir->setObjectName(QStringLiteral("roundtableOnAir"));
    m_onAir->setStyleSheet(QStringLiteral("color: #a6a6ac; font-size: 11px;"));
    headRow->addWidget(m_onAir);
    listLayout->addWidget(head);
    m_search = new QLineEdit(m_listColumn);
    m_search->setObjectName(QStringLiteral("roundtableRoomSearch"));
    m_search->setPlaceholderText(QStringLiteral("Search  /"));
    m_search->setToolTip(QStringLiteral("Find a frequency, band or callsign"));
    m_search->setStyleSheet(QLatin1String(kSearchStyle));
    m_search->setClearButtonEnabled(true);
    m_search->installEventFilter(this);
    connect(m_search, &QLineEdit::textChanged, this, &RoundtableWindow::rebuildList);
    listLayout->addWidget(m_search);
    m_rooms = new QListWidget(m_listColumn);
    m_rooms->setObjectName(QStringLiteral("roundtableRoomList"));
    m_rooms->setStyleSheet(QLatin1String(kListStyle));
    m_rooms->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_rooms->installEventFilter(this);
    connect(m_rooms, &QListWidget::itemClicked, this, &RoundtableWindow::openItem);
    listLayout->addWidget(m_rooms, 1);
    m_body->addWidget(m_listColumn);

    // The room (and, three columns wide, the messages beside it).
    m_roomColumn = new QWidget(this);
    m_roomColumn->setObjectName(QStringLiteral("roundtableRoomColumn"));
    auto* roomLayout = new QVBoxLayout(m_roomColumn);
    roomLayout->setContentsMargins(0, 0, 0, 0);
    roomLayout->setSpacing(0);
    m_body->addWidget(m_roomColumn, 1);

    if (m_model) {
        connect(m_model, &RoundtableModel::roomsChanged, this, &RoundtableWindow::rebuildList);
        connect(m_model, &RoundtableModel::sliceRoomsChanged, this, &RoundtableWindow::rebuildList);
        connect(m_model, &RoundtableModel::mentionsChanged, this, &RoundtableWindow::rebuildList);
    }
    resize(kWideWidth, kWideHeight);
    rebuildList();
    applyShape();
}

RoundtableWindow::~RoundtableWindow() = default;

RoundtableWindow::Shape RoundtableWindow::shapeFor(int width)
{
    if (width < kTwoColumnsFrom) {
        return Shape::OneColumn;
    }
    if (width <= kThreeColumnsAbove) {
        return Shape::TwoColumns;
    }
    return Shape::ThreeColumns;
}

void RoundtableWindow::takePanel(RoundtablePanel* panel)
{
    if (panel == nullptr || panel == m_panel) {
        return;
    }
    m_panel = panel;
    panel->setParent(m_roomColumn);
    m_roomColumn->layout()->addWidget(panel);
    panel->setPopped(true);
    panel->show();
    connect(panel, &RoundtablePanel::stepBackRequested, this, &RoundtableWindow::stepBack);
    connect(panel, &RoundtablePanel::shownRoomChanged, this, &RoundtableWindow::rebuildList);
    connect(panel, &RoundtablePanel::widePresetRequested, this, &RoundtableWindow::applyWidePreset);
    m_listInOneColumn = false;
    rebuildList();
    applyShape();
}

RoundtablePanel* RoundtableWindow::releasePanel()
{
    RoundtablePanel* panel = m_panel;
    if (panel == nullptr) {
        return nullptr;
    }
    disconnect(panel, nullptr, this, nullptr);
    m_roomColumn->layout()->removeWidget(panel);
    panel->setSplit(false);
    panel->setPopped(false);
    panel->setParent(nullptr);
    m_panel = nullptr;
    return panel;
}

void RoundtableWindow::showAtSavedGeometry()
{
    const QString saved = AppSettings::instance().value(QLatin1String(kGeometryKey)).toString();
    m_restoring = true;
    if (saved.isEmpty() || !restoreGeometry(QByteArray::fromHex(saved.toLatin1()))) {
        applyWidePreset();
        if (QWidget* owner = parentWidget()) {
            QScreen* screen = owner->screen();
            const QRect area = screen != nullptr ? screen->availableGeometry() : QRect();
            if (area.isValid()) {
                move(area.center() - rect().center());
            }
        }
    }
    m_restoring = false;
    show();
    raise();
    activateWindow();
    applyShape();
}

void RoundtableWindow::applyWidePreset()
{
    resize(kWideWidth, kWideHeight);
}

bool RoundtableWindow::isListShown() const
{
    return m_listColumn->isVisibleTo(this);
}

void RoundtableWindow::applyShape()
{
    m_shape = shapeFor(width());
    const bool oneColumn = m_shape == Shape::OneColumn;
    if (oneColumn) {
        m_listColumn->setMinimumWidth(0);
        m_listColumn->setMaximumWidth(QWIDGETSIZE_MAX);
    } else {
        m_listColumn->setFixedWidth(kRoomListWidth);
    }
    const bool listShown = !oneColumn || m_listInOneColumn || m_panel == nullptr;
    m_listColumn->setVisible(listShown);
    m_roomColumn->setVisible(!oneColumn || !listShown);
    if (m_panel != nullptr) {
        m_panel->setSplit(m_shape == Shape::ThreeColumns);
    }
}

void RoundtableWindow::showList(bool shown)
{
    m_listInOneColumn = shown;
    applyShape();
}

QString RoundtableWindow::stateText(const QJsonObject& summary) const
{
    const QString state = summary.value(QStringLiteral("state")).toString();
    QStringList talkers;
    for (const QJsonValue& value : summary.value(QStringLiteral("talkers")).toArray()) {
        talkers.append(value.toString());
    }
    if (state == QLatin1String("talking") && !talkers.isEmpty()) {
        return QStringLiteral("%1 talking").arg(talkers.join(QStringLiteral(", ")));
    }
    if (state == QLatin1String("doubled")) {
        return QStringLiteral("%1 doubled").arg(talkers.join(QStringLiteral(" + ")));
    }
    if (state == QLatin1String("unknown")) {
        return QStringLiteral("unknown talker");
    }
    const QJsonValue ended = summary.value(QStringLiteral("ended"));
    const qint64 since = ended.isDouble() ? ended.toInteger()
                                          : summary.value(QStringLiteral("last")).toInteger();
    if (since > 0) {
        const qint64 minutes = std::max<qint64>(1, (RoundtableModel::nowMs() - since) / 60000);
        return QStringLiteral("quiet %1 min").arg(minutes);
    }
    return QStringLiteral("quiet");
}

void RoundtableWindow::rebuildList()
{
    if (!m_model) {
        return;
    }
    const QString filter = m_search->text().trimmed();
    const QString shownId = m_panel != nullptr ? m_panel->roomId() : QString();
    const QString sliceRoom = m_panel != nullptr ? m_model->sliceRoomId(m_panel->followedSlice()) : QString();
    const QChar letter(QLatin1Char(static_cast<char>(
        'A' + std::clamp(m_panel != nullptr ? m_panel->followedSlice() : 0, 0, 25))));

    QList<QJsonObject> rooms;
    int onAir = 0;
    for (const QJsonValue& value : m_model->rooms()) {
        const QJsonObject summary = value.toObject();
        onAir += summary.value(QStringLiteral("members")).toInt();
        rooms.append(summary);
    }
    std::sort(rooms.begin(), rooms.end(), [](const QJsonObject& a, const QJsonObject& b) {
        return a.value(QStringLiteral("hz")).toInteger() < b.value(QStringLiteral("hz")).toInteger();
    });
    m_onAir->setText(QStringLiteral("%1 on air").arg(onAir));

    const QString keep = m_rooms->currentItem() != nullptr
                             ? m_rooms->currentItem()->data(kRoomIdRole).toString()
                             : shownId;
    QSignalBlocker block(m_rooms);
    m_rooms->clear();
    QString band;
    bool firstRoom = true;
    for (const QJsonObject& summary : std::as_const(rooms)) {
        const QString id = summary.value(QStringLiteral("id")).toString();
        const qint64 hz = summary.value(QStringLiteral("hz")).toInteger();
        const QString roomBand = summary.value(QStringLiteral("band")).toString();
        const QString family = summary.value(QStringLiteral("family")).toString(
            id.left(std::max<qsizetype>(0, id.indexOf(QLatin1Char('-')))));
        QStringList talkers;
        for (const QJsonValue& value : summary.value(QStringLiteral("talkers")).toArray()) {
            talkers.append(value.toString());
        }
        const QString haystack = QStringLiteral("%1 %2 %3 %4 %5")
                                     .arg(frequencyText(hz), QString::number(hz), roomBand,
                                          familyText(family), talkers.join(QLatin1Char(' ')));
        if (!filter.isEmpty() && !haystack.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }
        if (firstRoom || roomBand != band) {
            firstRoom = false;
            band = roomBand;
            auto* heading = new QListWidgetItem(band.isEmpty() ? QStringLiteral("Other") : band.toUpper(),
                                                m_rooms);
            heading->setFlags(Qt::NoItemFlags);
            QFont bold = heading->font();
            bold.setBold(true);
            bold.setPixelSize(11);
            heading->setFont(bold);
        }
        QString line = QStringLiteral("%1  %2").arg(frequencyText(hz), familyText(family));
        if (id == sliceRoom) {
            line += QStringLiteral("  ·  YOU · %1").arg(letter);
        }
        line += QStringLiteral("    %1").arg(summary.value(QStringLiteral("members")).toInt());
        const int mentions = m_model->mentionCount(id);
        if (mentions > 0) {
            line += QStringLiteral("  @%1").arg(mentions);
        }
        auto* item = new QListWidgetItem(line + QLatin1Char('\n') + stateText(summary), m_rooms);
        item->setData(kRoomIdRole, id);
        item->setToolTip(QStringLiteral("Read %1 here").arg(frequencyText(hz)));
        const QString state = summary.value(QStringLiteral("state")).toString();
        if (state == QLatin1String("talking")) {
            item->setForeground(QColor(0x7f, 0xd0, 0x95));
        } else if (state == QLatin1String("unknown")) {
            item->setForeground(QColor(0xd8, 0xa5, 0x5f));
        } else if (state == QLatin1String("doubled")) {
            item->setForeground(QColor(0xe0, 0x8a, 0x8c));
        }
        if (id == keep) {
            m_rooms->setCurrentItem(item);
        }
    }
}

void RoundtableWindow::openItem(QListWidgetItem* item)
{
    if (item == nullptr || m_panel == nullptr) {
        return;
    }
    const QString id = item->data(kRoomIdRole).toString();
    if (id.isEmpty()) {
        return;
    }
    m_panel->readRoom(id);
    if (m_shape == Shape::OneColumn) {
        showList(false);
    }
    m_panel->focusFirstItem();
}

void RoundtableWindow::stepBack()
{
    // From the room back to the list.
    if (m_shape == Shape::OneColumn) {
        showList(true);
    }
    m_rooms->setFocus(Qt::OtherFocusReason);
    if (m_rooms->currentItem() == nullptr) {
        for (int i = 0; i < m_rooms->count(); ++i) {
            if (m_rooms->item(i)->flags() & Qt::ItemIsSelectable) {
                m_rooms->setCurrentRow(i);
                break;
            }
        }
    }
}

void RoundtableWindow::saveGeometryNow()
{
    if (m_restoring || !isVisible()) {
        return;
    }
    AppSettings::instance().setValue(QLatin1String(kGeometryKey),
                                     QString::fromLatin1(saveGeometry().toHex()));
}

void RoundtableWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    applyShape();
    saveGeometryNow();
}

void RoundtableWindow::moveEvent(QMoveEvent* event)
{
    QWidget::moveEvent(event);
    saveGeometryNow();
}

void RoundtableWindow::closeEvent(QCloseEvent* event)
{
    saveGeometryNow();
    event->accept();
    // The application's quit sweep and close() from code are not
    // spontaneous: the window only hides and keeps the panel (the rule
    // AppletFloatingWindow follows). The close box docks it back.
    if (event->spontaneous()) {
        emit dockRequested();
    }
}

void RoundtableWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Slash) {
        if (m_shape == Shape::OneColumn) {
            showList(true);
        }
        m_search->setFocus(Qt::ShortcutFocusReason);
        m_search->selectAll();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        stepBack();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool RoundtableWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(watched, event);
    }
    auto* key = static_cast<QKeyEvent*>(event);
    if (watched == m_rooms) {
        switch (key->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            openItem(m_rooms->currentItem());
            return true;
        case Qt::Key_Slash:
            m_search->setFocus(Qt::ShortcutFocusReason);
            m_search->selectAll();
            return true;
        case Qt::Key_Escape:
            if (!m_search->text().isEmpty()) {
                m_search->clear();
                return true;
            }
            return false;
        default:
            return false;
        }
    }
    if (watched == m_search) {
        switch (key->key()) {
        case Qt::Key_Down:
        case Qt::Key_Up: {
            m_rooms->setFocus(Qt::OtherFocusReason);
            const int step = key->key() == Qt::Key_Down ? 1 : -1;
            int row = m_rooms->currentRow();
            for (int i = 0; i < m_rooms->count(); ++i) {
                row = std::clamp(row + step, 0, m_rooms->count() - 1);
                if (m_rooms->item(row)->flags() & Qt::ItemIsSelectable) {
                    m_rooms->setCurrentRow(row);
                    break;
                }
            }
            return true;
        }
        case Qt::Key_Return:
        case Qt::Key_Enter: {
            QListWidgetItem* item = m_rooms->currentItem();
            if (item == nullptr || !(item->flags() & Qt::ItemIsSelectable)) {
                for (int i = 0; i < m_rooms->count(); ++i) {
                    if (m_rooms->item(i)->flags() & Qt::ItemIsSelectable) {
                        item = m_rooms->item(i);
                        break;
                    }
                }
            }
            openItem(item);
            return true;
        }
        case Qt::Key_Escape:
            if (!m_search->text().isEmpty()) {
                m_search->clear();
            } else {
                m_rooms->setFocus(Qt::OtherFocusReason);
            }
            return true;
        default:
            return false;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace NereusSDR
