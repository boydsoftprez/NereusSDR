// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/roundtable/RoundtableWindow.cpp  (NereusSDR)
// =================================================================
//
// See RoundtableWindow.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-30: Approved frequency-first Roundtable desktop surfaces.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 13).
//   2026-10-25: every color is the workspace look's role (Style:: and
//               WorkspacePreview::roleColourName / statusColour), the room
//               states the look's status colors. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-26: rebuildList updates the same rows in place and drops the
//               pick before a refill, so a rooms refresh tells accessibility
//               nothing about a row that is gone (JJ's log on PR 657).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/roundtable/RoundtableWindow.h"

#include "core/AppSettings.h"
#include "gui/StyleConstants.h"
#include "gui/roundtable/RoundtablePanel.h"
#include "gui/roundtable/RoundtableTagLayout.h"
#include "gui/styles/WorkspacePreview.h"
#include "models/RoundtableModel.h"

#include <QBrush>
#include <QCloseEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
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

// Every color is the workspace look's role; with the look off, the classic
// Style:: value it stands for.
QString role(const char* legacy, const char* name)
{
    return WorkspacePreview::roleColourName(legacy, name);
}

QString panelColor() { return role(Style::kPanelBg, "panel"); }
QString ruleColor() { return role(Style::kBorderSubtle, "border-subtle"); }
QString dimColor() { return role(Style::kTextSecondary, "text-secondary"); }

QString listStyle()
{
    return QStringLiteral(
               "QListWidget { background: %1; border: none; color: %2; font-size: 12px;"
               "  outline: none; }"
               "QListWidget::item { padding: 4px 8px; border-bottom: 1px solid %3; }"
               "QListWidget::item:selected { background: %4; color: %5;"
               "  border-left: 2px solid %6; }"
               "QListWidget::item:disabled { color: %7; background: %8; }")
        .arg(panelColor(), role(Style::kTextPrimary, "text"), ruleColor(), role(Style::kBlueBg, "selected-top"),
             role(Style::kBlueText, "selected-text"), role(Style::kAccent, "accent"), dimColor(),
             role(Style::kDisabledBg, "disabled-bg"));
}

QString searchStyle()
{
    return QStringLiteral(
               "QLineEdit { background: %1; border: 1px solid %2; border-radius: 3px;"
               "  color: %3; font-size: 12px; padding: 2px 5px; margin: 6px 8px;"
               "  placeholder-text-color: %4; }"
               "QLineEdit:focus { border-color: %5; }")
        .arg(role(Style::kInsetBg, "inset"), role(Style::kBorder, "border-muted"), role(Style::kTextPrimary, "text"),
             role(Style::kTextTertiary, "text-tertiary"), role(Style::kAccent, "accent"));
}

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
    setStyleSheet(QStringLiteral("QWidget#RoundtableWindow { background: %1; }").arg(role(Style::kAppBg, "app-bg")));
    setMinimumSize(300, 320);

    m_body = new QHBoxLayout(this);
    m_body->setContentsMargins(0, 0, 0, 0);
    m_body->setSpacing(0);

    // The room list (mockup .wl).
    m_listColumn = new QWidget(this);
    m_listColumn->setObjectName(QStringLiteral("roundtableRoomListColumn"));
    m_listColumn->setAttribute(Qt::WA_StyledBackground, true);
    m_listColumn->setStyleSheet(QStringLiteral("QWidget#roundtableRoomListColumn { background: %1;"
                                               " border-right: 1px solid %2; }")
                                    .arg(panelColor(), ruleColor()));
    auto* listLayout = new QVBoxLayout(m_listColumn);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(0);
    auto* head = new QWidget(m_listColumn);
    head->setObjectName(QStringLiteral("roundtableRoomListHead"));
    head->setAttribute(Qt::WA_StyledBackground, true);
    head->setStyleSheet(QStringLiteral("QWidget#roundtableRoomListHead { border-bottom: 1px solid %1; }")
                            .arg(ruleColor()));
    auto* headRow = new QHBoxLayout(head);
    headRow->setContentsMargins(8, 6, 8, 6);
    auto* title = new QLabel(QStringLiteral("FREQUENCIES"), head);
    title->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; font-weight: bold;").arg(dimColor()));
    QFont spaced = title->font();
    spaced.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    title->setFont(spaced);
    headRow->addWidget(title, 1);
    m_onAir = new QLabel(head);
    m_onAir->setObjectName(QStringLiteral("roundtableOnAir"));
    m_onAir->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(dimColor()));
    headRow->addWidget(m_onAir);
    listLayout->addWidget(head);
    m_search = new QLineEdit(m_listColumn);
    m_search->setObjectName(QStringLiteral("roundtableRoomSearch"));
    m_search->setPlaceholderText(QStringLiteral("Search  /"));
    m_search->setToolTip(QStringLiteral("Find a frequency, band or callsign"));
    m_search->setStyleSheet(searchStyle());
    m_search->setClearButtonEnabled(true);
    m_search->installEventFilter(this);
    connect(m_search, &QLineEdit::textChanged, this, &RoundtableWindow::rebuildList);
    listLayout->addWidget(m_search);
    m_rooms = new QListWidget(m_listColumn);
    m_rooms->setObjectName(QStringLiteral("roundtableRoomList"));
    m_rooms->setStyleSheet(listStyle());
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
    if (m_model->frequencyAvailable()) {
        for (const RoundtableTag& tag : roundtableTagsFor(m_model, {})) {
            QStringList calls;
            for (const QJsonValue& value : tag.stations) {
                const QJsonObject station = value.toObject();
                QString call = station.value(QStringLiteral("callsign")).toString();
                if (station.value(QStringLiteral("nth")).toInt(1) > 1) {
                    call += QStringLiteral(" (%1)").arg(station.value(QStringLiteral("nth")).toInt());
                }
                calls.append(call);
            }
            rooms.append(QJsonObject{{QStringLiteral("id"), tag.roomId},
                {QStringLiteral("hz"), tag.hz}, {QStringLiteral("members"), tag.members},
                {QStringLiteral("family"), tag.roomId.mid(QStringLiteral("frequency:").size()).section(QLatin1Char('-'), 0, 0)},
                {QStringLiteral("state"), tag.state == static_cast<int>(RoundtableGlyph::Talking) ? QStringLiteral("talking") : QStringLiteral("quiet")},
                {QStringLiteral("calls"), calls.join(QStringLiteral(" · "))}});
            onAir += tag.members;
        }
    } else {
        for (const QJsonValue& value : m_model->rooms()) {
            const QJsonObject summary = value.toObject();
            onAir += summary.value(QStringLiteral("members")).toInt();
            rooms.append(summary);
        }
    }
    std::sort(rooms.begin(), rooms.end(), [](const QJsonObject& a, const QJsonObject& b) {
        return a.value(QStringLiteral("hz")).toInteger() < b.value(QStringLiteral("hz")).toInteger();
    });
    m_onAir->setText(QStringLiteral("%1 on air").arg(onAir));

    const QString keep = m_rooms->currentItem() != nullptr
                             ? m_rooms->currentItem()->data(kRoomIdRole).toString()
                             : shownId;

    // The rows the list should show: a band heading, then its rooms.
    struct Row {
        QString id;  // empty for a heading
        QString text;
        QString toolTip;
        QVariant foreground;
    };
    QList<Row> rows;
    QString band;
    bool firstRoom = true;
    for (const QJsonObject& summary : std::as_const(rooms)) {
        const QString id = summary.value(QStringLiteral("id")).toString();
        const qint64 hz = summary.value(QStringLiteral("hz")).toInteger();
        const QString roomBand = summary.value(QStringLiteral("band")).toString();
        const QString family = summary.value(QStringLiteral("family")).toString(
            id.left(std::max<qsizetype>(0, id.indexOf(QLatin1Char('-')))));
        QStringList talkers;
        if (summary.contains(QStringLiteral("calls"))) { talkers.append(summary.value(QStringLiteral("calls")).toString()); }
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
            rows.append(Row{QString(), band.isEmpty() ? QStringLiteral("Other") : band.toUpper(), QString(),
                            QVariant()});
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
        Row row{id, line + QLatin1Char('\n') + (summary.contains(QStringLiteral("calls")) ? summary.value(QStringLiteral("calls")).toString() : stateText(summary)),
                QStringLiteral("Read %1 here").arg(frequencyText(hz)), QVariant()};
        const QString state = summary.value(QStringLiteral("state")).toString();
        if (state == QLatin1String("talking")) {
            row.foreground = QBrush(WorkspacePreview::statusColour(QColor(Style::kGreenText), "ok"));
        } else if (state == QLatin1String("unknown")) {
            row.foreground = QBrush(WorkspacePreview::statusColour(QColor(Style::kAmberText), "warn"));
        } else if (state == QLatin1String("doubled")) {
            row.foreground = QBrush(WorkspacePreview::statusColour(QColor(Style::kRedBorder), "danger"));
        }
        rows.append(row);
    }

    QSignalBlocker block(m_rooms);
    // The refresh every couple of seconds mostly changes counts and states:
    // the same headings and rooms in the same order are updated in place.
    // Clearing and refilling the list there took the operator's pick away
    // and back each time, and every time macOS accessibility was told of
    // the pick's removal; it could not find the row ("Cannot create
    // accessible child interface ... roundtableRoomList ...
    // SelectionRemove", JJ's log 2026-10-26).
    bool sameRows = m_rooms->count() == rows.size();
    for (int i = 0; sameRows && i < rows.size(); ++i) {
        const QListWidgetItem* item = m_rooms->item(i);
        const bool heading = rows[i].id.isEmpty();
        sameRows = heading ? (item->flags() == Qt::NoItemFlags && item->text() == rows[i].text)
                           : item->data(kRoomIdRole).toString() == rows[i].id;
    }
    if (sameRows) {
        for (int i = 0; i < rows.size(); ++i) {
            if (rows[i].id.isEmpty()) {
                continue;
            }
            QListWidgetItem* item = m_rooms->item(i);
            if (item->text() != rows[i].text) {
                item->setText(rows[i].text);
            }
            if (item->toolTip() != rows[i].toolTip) {
                item->setToolTip(rows[i].toolTip);
            }
            if (item->data(Qt::ForegroundRole) != rows[i].foreground) {
                item->setData(Qt::ForegroundRole, rows[i].foreground);
            }
            if (rows[i].id == keep && m_rooms->currentItem() != item) {
                m_rooms->setCurrentItem(item);
            }
        }
        return;
    }

    // Rooms came or went: the list is filled again, its pick dropped first
    // while its row is still there.
    m_rooms->setCurrentItem(nullptr);
    m_rooms->clearSelection();
    m_rooms->clear();
    for (const Row& row : std::as_const(rows)) {
        auto* item = new QListWidgetItem(row.text, m_rooms);
        if (row.id.isEmpty()) {
            item->setFlags(Qt::NoItemFlags);
            QFont bold = item->font();
            bold.setBold(true);
            bold.setPixelSize(11);
            item->setFont(bold);
            continue;
        }
        item->setData(kRoomIdRole, row.id);
        item->setToolTip(row.toolTip);
        if (row.foreground.isValid()) {
            item->setData(Qt::ForegroundRole, row.foreground);
        }
        if (row.id == keep) {
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
