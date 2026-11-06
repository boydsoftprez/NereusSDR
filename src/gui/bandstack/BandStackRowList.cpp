// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/bandstack/BandStackRowList.cpp  (NereusSDR)
// =================================================================
//
// See BandStackRowList.h. Sizes and colours are desktop.html's .sec, .en and
// .add; the classic column holds the same roles in the classic look.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
// =================================================================

#include "gui/bandstack/BandStackRowList.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QEvent>
#include <QFocusEvent>
#include <QHelpEvent>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QToolTip>

#include <algorithm>
#include <array>
#include <utility>

#include "gui/ScopedChildWidget.h"
#include "gui/styles/PopupMenuStyle.h"
#include "gui/styles/WorkspacePreview.h"
#include "gui/widgets/VfoWidget.h"
#include "models/BandDefaults.h"
#include "models/bandstack/BandStackAccess.h"

namespace NereusSDR {

namespace {

// desktop.html's ORDER: the ham bands, then WWV, GEN and the shortwave bands.
// XVTR is not stacked (design V12).
constexpr std::array<Band, 27> kBandOrder{
    Band::Band160m, Band::Band80m,  Band::Band60m,  Band::Band40m, Band::Band30m, Band::Band20m,
    Band::Band17m,  Band::Band15m,  Band::Band12m,  Band::Band10m, Band::Band6m,  Band::Band2m,
    Band::WWV,      Band::GEN,      Band::Band120m, Band::Band90m, Band::Band61m, Band::Band49m,
    Band::Band41m,  Band::Band31m,  Band::Band25m,  Band::Band22m, Band::Band19m, Band::Band16m,
    Band::Band14m,  Band::Band13m,  Band::Band11m,
};

// The same spot, as the store and the controller count it.
constexpr qint64 kSameSpotHz = 100;

// desktop.html: a row starts its drag past 4 px of vertical travel.
constexpr int kDragStartPx = 4;

// .sec
constexpr int kSectionHeight = 22;
constexpr int kSectionMarginTop = 1;
// .en: margin 2px 0 2px 3px, padding 4px 6px 4px 7px, 1px border.
constexpr int kRowMarginY = 2;
constexpr int kRowMarginLeft = 3;
constexpr int kRowPaddingTop = 4;
constexpr int kRowPaddingRight = 6;
constexpr int kRowPaddingLeft = 7;
constexpr int kRowRadius = 4;
constexpr int kFrequencyLine = 17;
constexpr int kDetailLine = 13;
constexpr int kEditorHeight = 15;
constexpr int kFlagSize = 13;
constexpr int kIconRight = 4;
constexpr int kIconTop = 4;
constexpr int kIconGap = 3;
constexpr int kLockWidth = 10;
constexpr int kLockHeight = 11;
// .en.drag
constexpr int kDragOffsetX = 4;
constexpr qreal kDragOpacity = 0.7;
// #pb.ro
constexpr qreal kReadOnlyRowOpacity = 0.55;
constexpr qreal kReadOnlyAddOpacity = 0.4;
constexpr qreal kGreyedOpacity = 0.45;
// .add: margin 2px 0 3px 3px.
constexpr int kAddHeight = 20;
constexpr int kAddMarginTop = 2;
constexpr int kAddMarginBottom = 3;
// seg0.color+'22' in desktop.html's row().
constexpr int kTintAlpha = 0x22;
constexpr qreal kTintEnd = 0.75;

struct RowColours {
    QColor surface;
    QColor surfaceHover;
    QColor border;
    QColor borderHover;
    QColor activeTop;
    QColor activeBottom;
    QColor activeBorder;
    QColor keyboard;
    QColor text;
    QColor textBright;
    QColor secondary;
    QColor faint;
    QColor name;
};

RowColours rowColours()
{
    const bool workspace = WorkspacePreview::enabled();
    const auto pick = [workspace](QRgb look, QRgb classic) {
        return QColor(workspace ? look : classic);
    };
    RowColours c;
    c.surface = pick(0x111114, 0x16162a);
    c.surfaceHover = pick(0x17171b, 0x1c2a3a);
    c.border = WorkspacePreview::roleColour(QColor(0x20, 0x30, 0x40), "border-subtle");
    c.borderHover = pick(0x3d3d41, 0x304050);
    c.activeTop = pick(0x20344c, 0x1a3a5a);
    c.activeBottom = pick(0x182638, 0x12283f);
    c.activeBorder = pick(0x4d89c1, 0x00b4d8);
    c.keyboard = pick(0x8ab7e0, 0x8ad4e8);
    c.text = WorkspacePreview::roleColour(QColor(0xc8, 0xd8, 0xe8), "text");
    c.textBright = QColor(0xff, 0xff, 0xff);
    c.secondary = WorkspacePreview::roleColour(QColor(0x8a, 0xa8, 0xc0), "text-secondary");
    c.faint = WorkspacePreview::roleColour(QColor(0x60, 0x70, 0x80), "text-inactive");
    c.name = QColor(0x7f, 0xb2, 0xe0);
    return c;
}

// desktop.html's LOCK, viewBox 0 0 10 11.
void paintLock(QPainter& painter, const QRectF& box)
{
    const QColor gold(0xc9, 0xa2, 0x4a);
    painter.save();
    painter.translate(box.topLeft());
    painter.scale(box.width() / kLockWidth, box.height() / kLockHeight);
    painter.setPen(Qt::NoPen);
    painter.setBrush(gold);
    painter.drawRoundedRect(QRectF(1, 5, 8, 6), 1, 1);
    QPainterPath shackle;
    shackle.moveTo(3, 5);
    shackle.lineTo(3, 3.2);
    shackle.arcTo(QRectF(3, 1.2, 4, 4), 180, -180);
    shackle.lineTo(7, 5);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(gold, 1.3));
    painter.drawPath(shackle);
    painter.restore();
}

// desktop.html's PIN, viewBox 0 0 10 11.
void paintPin(QPainter& painter, const QRectF& box)
{
    painter.save();
    painter.translate(box.topLeft());
    painter.scale(box.width() / kLockWidth, box.height() / kLockHeight);
    QPainterPath star;
    star.moveTo(5, 0);
    star.lineTo(7, 4);
    star.lineTo(10, 4.5);
    star.lineTo(7.5, 7);
    star.lineTo(8, 10);
    star.lineTo(5, 8.4);
    star.lineTo(2, 10);
    star.lineTo(2.5, 7);
    star.lineTo(0, 4.5);
    star.lineTo(3, 4);
    star.closeSubpath();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0xd8, 0xa5, 0x5f));
    painter.drawPath(star);
    painter.restore();
}

QString filterWidthText(int lowHz, int highHz)
{
    const int width = qAbs(highHz - lowHz);
    if (width >= 1000) {
        return QStringLiteral("%1K").arg(width / 1000.0, 0, 'g', 3);
    }
    return QString::number(width);
}

BandStackReturnMode returnModeFromText(const QString& text)
{
    if (text == QStringLiteral("lastUsed")) {
        return BandStackReturnMode::LastUsed;
    }
    if (text == QStringLiteral("specific")) {
        return BandStackReturnMode::Specific;
    }
    return BandStackReturnMode::Current;
}

QScrollArea* scrollAreaAround(QWidget* widget)
{
    for (QWidget* parent = widget->parentWidget(); parent; parent = parent->parentWidget()) {
        if (auto* area = qobject_cast<QScrollArea*>(parent)) {
            return area;
        }
    }
    return nullptr;
}

} // namespace

BandStackRowList::BandStackRowList(BandStackAccess* access, QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("bandStackRowList"));
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAccess(access);
}

void BandStackRowList::setAccess(BandStackAccess* access)
{
    if (m_access == access) {
        return;
    }
    if (m_access) {
        m_access->disconnect(this);
    }
    m_access = access;
    if (access) {
        connect(access, &BandStackAccess::documentChanged, this, &BandStackRowList::reloadDocument);
        // A refused reorder or rename leaves no new document, so what the rows
        // show has to go back to what the Core holds.
        connect(access, &BandStackAccess::refused, this, &BandStackRowList::reloadDocument);
    }
    reloadDocument();
}

void BandStackRowList::setContext(BandStackRowContext context)
{
    m_context = std::move(context);
    refreshSlices();
}

void BandStackRowList::setSliceId(int sliceId)
{
    if (m_sliceId == sliceId) {
        return;
    }
    m_sliceId = sliceId;
    m_userClosed.clear();
    refreshDerived();
    relayout();
    revealPlaceSoon();
}

void BandStackRowList::setSliceFrequency(std::optional<double> frequencyHz)
{
    if (m_sliceFrequencyHz == frequencyHz) {
        return;
    }
    m_sliceFrequencyHz = frequencyHz;
    const std::optional<Band> before = m_currentBand;
    refreshDerived();
    if (before != m_currentBand) {
        relayout();
    }
}

void BandStackRowList::setBand(std::optional<Band> band)
{
    if (m_singleBand == band) {
        return;
    }
    m_singleBand = band;
    relayout();
}

void BandStackRowList::setGrouped(bool grouped)
{
    if (m_grouped == grouped) {
        return;
    }
    m_grouped = grouped;
    relayout();
    revealPlaceSoon();
}

void BandStackRowList::refreshSlices()
{
    const std::optional<Band> before = m_currentBand;
    refreshDerived();
    if (before != m_currentBand) {
        relayout();
    } else {
        update();
    }
}

void BandStackRowList::reloadDocument()
{
    if (m_dragging) {
        m_documentChangedInDrag = true;
        return;
    }
    const QJsonObject document = m_access ? m_access->document() : QJsonObject{};
    m_hasDocument = document.contains(QStringLiteral("entries"));
    m_bands.clear();
    m_places.clear();

    QHash<QString, Entry> entries;
    QStringList unordered;
    const QJsonArray entryArray = document.value(QStringLiteral("entries")).toArray();
    for (const QJsonValue& value : entryArray) {
        const QJsonObject object = value.toObject();
        const std::optional<Band> band =
            BandStackStore::bandFromKey(object.value(QStringLiteral("band")).toString());
        if (!band.has_value()) {
            continue;
        }
        Entry entry;
        entry.id = object.value(QStringLiteral("id")).toString();
        entry.band = *band;
        entry.frequencyHz = object.value(QStringLiteral("frequencyHz")).toInteger();
        entry.mode = object.value(QStringLiteral("mode")).toString();
        entry.filterLowHz = object.value(QStringLiteral("filterLowHz")).toInt();
        entry.filterHighHz = object.value(QStringLiteral("filterHighHz")).toInt();
        entry.locked = object.value(QStringLiteral("locked")).toBool();
        entry.name = object.value(QStringLiteral("name")).toString();
        entries.insert(entry.id, entry);
        unordered.append(entry.id);
    }

    const QJsonObject bands = document.value(QStringLiteral("bands")).toObject();
    for (auto it = bands.constBegin(); it != bands.constEnd(); ++it) {
        const std::optional<Band> band = BandStackStore::bandFromKey(it.key());
        if (!band.has_value()) {
            continue;
        }
        const QJsonObject object = it.value().toObject();
        BandData& data = m_bands[*band];
        data.returnMode = returnModeFromText(object.value(QStringLiteral("returnMode")).toString());
        data.specificId = object.value(QStringLiteral("specificId")).toString();
        const QJsonArray order = object.value(QStringLiteral("order")).toArray();
        for (const QJsonValue& id : order) {
            const auto found = entries.constFind(id.toString());
            if (found != entries.constEnd() && found->band == *band) {
                data.entries.append(*found);
                unordered.removeOne(found->id);
            }
        }
    }
    for (const QString& id : std::as_const(unordered)) {
        const Entry& entry = entries[id];
        m_bands[entry.band].entries.append(entry);
    }

    const QJsonObject places = document.value(QStringLiteral("places")).toObject();
    for (auto slice = places.constBegin(); slice != places.constEnd(); ++slice) {
        if (slice.key().size() != 1) {
            continue;
        }
        QHash<Band, QString>& held = m_places[slice.key().at(0)];
        const QJsonObject perBand = slice.value().toObject();
        for (auto place = perBand.constBegin(); place != perBand.constEnd(); ++place) {
            if (const std::optional<Band> band = BandStackStore::bandFromKey(place.key())) {
                held.insert(*band, place.value().toString());
            }
        }
    }

    if (!m_keyboardId.isEmpty() && !entryById(m_keyboardId)) {
        m_keyboardId.clear();
    }
    if (!m_editId.isEmpty() && !entryById(m_editId)) {
        finishRename(false, false);
    }
    refreshDerived();
    relayout();
}

std::optional<Band> BandStackRowList::stackBandFor(QChar letter, double frequencyHz) const
{
    // BandStackController::stackBandFor without its memory of the last recall:
    // a place holding this very frequency, then the band the frequency falls in.
    const Band grid = BandDefaults::bandGridBandFromFrequency(frequencyHz);
    const qint64 hz = qRound64(frequencyHz);
    const QHash<Band, QString> held = m_places.value(letter);
    const auto holdsThisFrequency = [&](Band band) {
        const Entry* entry = entryById(held.value(band));
        return entry && qAbs(entry->frequencyHz - hz) < kSameSpotHz;
    };
    if (holdsThisFrequency(grid)) {
        return grid;
    }
    for (Band band : kBandOrder) {
        if (holdsThisFrequency(band)) {
            return band;
        }
    }
    return grid;
}

void BandStackRowList::refreshDerived()
{
    const std::optional<Band> before = m_currentBand;
    m_currentBand = (m_sliceId >= 0 && m_sliceFrequencyHz.has_value())
        ? stackBandFor(sliceLetter(), *m_sliceFrequencyHz)
        : std::nullopt;

    m_flags.clear();
    if (m_context.slices) {
        const QList<BandStackSliceInfo> slices = m_context.slices();
        for (const BandStackSliceInfo& slice : slices) {
            if (slice.id == m_sliceId) {
                continue;
            }
            const std::optional<Band> band = stackBandFor(slice.letter, slice.frequencyHz);
            const QString place = band ? m_places.value(slice.letter).value(*band) : QString();
            if (!place.isEmpty()) {
                m_flags[place].append({slice.id, slice.letter});
            }
        }
    }
    m_blockedReason = (m_context.blockedReason && m_sliceId >= 0)
        ? m_context.blockedReason(m_sliceId)
        : QString();

    if (before != m_currentBand) {
        m_userClosed.clear();
        revealPlaceSoon();
        emit currentBandChanged();
    }
}

bool BandStackRowList::isBandOpen(Band band) const
{
    return (band == m_currentBand && !m_userClosed.contains(band)) || m_userOpen.contains(band);
}

void BandStackRowList::setBandOpen(Band band, bool open)
{
    if (open) {
        m_userOpen.insert(band);
        m_userClosed.remove(band);
    } else {
        m_userOpen.remove(band);
        if (band == m_currentBand) {
            m_userClosed.insert(band);
        }
    }
    relayout();
}

void BandStackRowList::toggleSection(Band band)
{
    setBandOpen(band, !isBandOpen(band));
}

int BandStackRowList::entryCount(Band band) const
{
    const auto found = m_bands.constFind(band);
    return found == m_bands.constEnd() ? 0 : static_cast<int>(found->entries.size());
}

BandStackReturnMode BandStackRowList::returnMode(Band band) const
{
    const auto found = m_bands.constFind(band);
    return found == m_bands.constEnd() ? BandStackReturnMode::Current : found->returnMode;
}

QString BandStackRowList::defaultSpecificEntry(Band band) const
{
    const QString place = m_places.value(sliceLetter()).value(band);
    if (!place.isEmpty() && entryById(place)) {
        return place;
    }
    const auto found = m_bands.constFind(band);
    if (found == m_bands.constEnd() || found->entries.isEmpty()) {
        return {};
    }
    for (const Entry& entry : found->entries) {
        if (entry.mode == QStringLiteral("LSB") || entry.mode == QStringLiteral("USB")) {
            return entry.id;
        }
    }
    return found->entries.first().id;
}

const BandStackRowList::Entry* BandStackRowList::entryById(const QString& id) const
{
    if (id.isEmpty()) {
        return nullptr;
    }
    for (const BandData& data : m_bands) {
        for (const Entry& entry : data.entries) {
            if (entry.id == id) {
                return &entry;
            }
        }
    }
    return nullptr;
}

QChar BandStackRowList::sliceLetter() const
{
    return QChar(QLatin1Char(static_cast<char>('A' + qMax(0, m_sliceId))));
}

bool BandStackRowList::isPlaceRow(Band band, const QString& entryId) const
{
    // desktop.html's row(): the slice's place on the row's own band, whichever
    // band the slice is on now.
    return m_sliceId >= 0 && m_places.value(sliceLetter()).value(band) == entryId;
}

BandStackView BandStackRowList::currentView() const
{
    return m_context.viewForSlice ? m_context.viewForSlice(m_sliceId) : BandStackView{};
}

QString BandStackRowList::placeRowId() const
{
    if (!m_currentBand.has_value()) {
        return {};
    }
    return m_places.value(sliceLetter()).value(*m_currentBand);
}

void BandStackRowList::addEntryItems(const QVector<Entry>& entries, int& y, int& margin)
{
    for (const Entry& entry : entries) {
        y += qMax(margin, kRowMarginY);
        const bool editing = entry.id == m_editId;
        const int nameHeight = editing ? kEditorHeight : (entry.name.isEmpty() ? 0 : kDetailLine);
        const int height = 2 + 2 * kRowPaddingTop + kFrequencyLine + kDetailLine + nameHeight;
        Item item;
        item.kind = Item::Row;
        item.band = entry.band;
        item.entryId = entry.id;
        item.rect = QRect(kRowMarginLeft, y, qMax(0, width() - kRowMarginLeft), height);
        m_items.append(item);
        y += height;
        margin = kRowMarginY;
    }
}

void BandStackRowList::relayout()
{
    m_items.clear();
    m_hoverItem = -1;
    int y = 0;
    int margin = 0;
    const auto addRow = [&](Band band) {
        y += qMax(margin, kAddMarginTop);
        Item item;
        item.kind = Item::Add;
        item.band = band;
        item.rect = QRect(kRowMarginLeft, y, qMax(0, width() - kRowMarginLeft), kAddHeight);
        m_items.append(item);
        y += kAddHeight;
        margin = kAddMarginBottom;
    };

    if (m_hasDocument) {
        if (m_singleBand.has_value()) {
            addEntryItems(m_bands.value(*m_singleBand).entries, y, margin);
            if (m_singleBand == m_currentBand) {
                addRow(*m_singleBand);
            }
        } else if (m_grouped) {
            for (Band band : kBandOrder) {
                y += qMax(margin, kSectionMarginTop);
                Item section;
                section.kind = Item::Section;
                section.band = band;
                section.rect = QRect(0, y, width(), kSectionHeight);
                m_items.append(section);
                y += kSectionHeight;
                margin = 0;
                if (isBandOpen(band)) {
                    addEntryItems(m_bands.value(band).entries, y, margin);
                    if (band == m_currentBand) {
                        addRow(band);
                    }
                }
            }
        } else {
            QVector<Entry> all;
            for (const BandData& data : std::as_const(m_bands)) {
                all += data.entries;
            }
            std::stable_sort(all.begin(), all.end(), [](const Entry& a, const Entry& b) {
                return a.frequencyHz < b.frequencyHz;
            });
            addEntryItems(all, y, margin);
        }
    }

    m_contentHeight = y + margin;
    setFixedHeight(m_contentHeight);
    placeEditor();
    update();
}

void BandStackRowList::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    for (Item& item : m_items) {
        item.rect.setRight(width() - 1);
    }
    placeEditor();
}

QSize BandStackRowList::sizeHint() const
{
    return {width(), m_contentHeight};
}

int BandStackRowList::itemAt(const QPoint& pos) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).rect.contains(pos)) {
            return i;
        }
    }
    return -1;
}

QStringList BandStackRowList::visibleRowIds() const
{
    QStringList ids;
    for (const Item& item : m_items) {
        if (item.kind == Item::Row) {
            ids.append(item.entryId);
        }
    }
    return ids;
}

QRect BandStackRowList::rowRect(const QString& entryId) const
{
    for (const Item& item : m_items) {
        if (item.kind == Item::Row && item.entryId == entryId) {
            return item.rect;
        }
    }
    return {};
}

QRect BandStackRowList::sectionRect(Band band) const
{
    for (const Item& item : m_items) {
        if (item.kind == Item::Section && item.band == band) {
            return item.rect;
        }
    }
    return {};
}

QRect BandStackRowList::addRect() const
{
    for (const Item& item : m_items) {
        if (item.kind == Item::Add) {
            return item.rect;
        }
    }
    return {};
}

// ---- Verbs ----

void BandStackRowList::goTo(const QString& entryId)
{
    if (m_access && m_sliceId >= 0) {
        m_access->goTo(m_sliceId, entryId, currentView());
    }
}

void BandStackRowList::addCurrentSpot()
{
    if (m_access && m_sliceId >= 0) {
        m_access->add(m_sliceId, currentView());
    }
}

void BandStackRowList::toggleLock(const QString& entryId)
{
    const Entry* entry = entryById(entryId);
    if (m_access && entry) {
        m_access->edit(entryId, std::nullopt, !entry->locked);
    }
}

void BandStackRowList::removeEntry(const QString& entryId)
{
    if (m_access) {
        m_access->remove(entryId);
    }
}

// ---- The row menu ----

void BandStackRowList::fillRowMenu(QMenu& menu, const QString& entryId)
{
    const Entry* entry = entryById(entryId);
    menu.setStyleSheet(WorkspacePreview::lookQss(QString::fromLatin1(kPopupMenu), &menu));
    menu.setToolTipsVisible(true);
    if (!entry) {
        return;
    }
    const QString id = entryId;
    const Band band = entry->band;

    QAction* goThere = menu.addAction(QStringLiteral("Go there"));
    if (m_sliceId < 0) {
        goThere->setEnabled(false);
        goThere->setToolTip(QStringLiteral("Select a slice first."));
    }
    connect(goThere, &QAction::triggered, this, [this, id] { goTo(id); });

    QAction* save = menu.addAction(QStringLiteral("Save current spot here"));
    if (entry->locked) {
        save->setEnabled(false);
        save->setToolTip(QStringLiteral("This entry is locked. Unlock it to save over it."));
    } else if (m_sliceId < 0 || band != m_currentBand) {
        save->setEnabled(false);
        save->setToolTip(m_sliceId < 0
                             ? QStringLiteral("Select a slice first.")
                             : QStringLiteral("Slice %1 is not on %2. Tune to %2 to save here.")
                                   .arg(sliceLetter())
                                   .arg(bandLabel(band)));
    }
    connect(save, &QAction::triggered, this, [this, id] {
        if (m_access && m_sliceId >= 0) {
            m_access->saveHere(m_sliceId, id, currentView());
        }
    });
    menu.addSeparator();

    QAction* rename = menu.addAction(QStringLiteral("Rename\t%1")
                                         .arg(QKeySequence(Qt::Key_F2).toString(QKeySequence::NativeText)));
    connect(rename, &QAction::triggered, this, [this, id] { beginRename(id); });

    QAction* lock = menu.addAction(QStringLiteral("%1\t%2").arg(
        entry->locked ? QStringLiteral("Unlock") : QStringLiteral("Lock"),
        QKeySequence(Qt::CTRL | Qt::Key_U).toString(QKeySequence::NativeText)));
    connect(lock, &QAction::triggered, this, [this, id] { toggleLock(id); });

    const auto bandData = m_bands.constFind(band);
    const bool isSpecific = bandData != m_bands.constEnd()
        && bandData->returnMode == BandStackReturnMode::Specific && bandData->specificId == id;
    QAction* always = menu.addAction(QStringLiteral("Always return here"));
    always->setCheckable(true);
    always->setChecked(isSpecific);
    connect(always, &QAction::triggered, this, [this, id, band, isSpecific] {
        if (!m_access) {
            return;
        }
        if (isSpecific) {
            m_access->setReturn(band, BandStackReturnMode::Current, QString());
        } else {
            m_access->setReturn(band, BandStackReturnMode::Specific, id);
        }
    });
    menu.addSeparator();

#ifdef Q_OS_MAC
    const QKeySequence deleteKey(Qt::Key_Backspace);
#else
    const QKeySequence deleteKey(Qt::Key_Delete);
#endif
    QAction* remove = menu.addAction(
        QStringLiteral("Delete\t%1").arg(deleteKey.toString(QKeySequence::NativeText)));
    connect(remove, &QAction::triggered, this, [this, id] { removeEntry(id); });
}

void BandStackRowList::showRowMenu(const QString& entryId, const QPoint& globalPos)
{
    ScopedChildWidget<QMenu> menuOwner(this);
    QMenu& menu = *menuOwner.get();
    fillRowMenu(menu, entryId);
    menu.exec(globalPos);
}

void BandStackRowList::contextMenuEvent(QContextMenuEvent* event)
{
    if (event->reason() == QContextMenuEvent::Keyboard) {
        const QString id = m_keyboardId.isEmpty() ? placeRowId() : m_keyboardId;
        const QRect rect = rowRect(id);
        if (rect.isValid()) {
            showRowMenu(id, mapToGlobal(rect.center()));
        } else {
            emit panelContextRequested(mapToGlobal(QPoint(0, 0)));
        }
        return;
    }
    const int index = itemAt(event->pos());
    if (index >= 0 && m_items.at(index).kind == Item::Row) {
        showRowMenu(m_items.at(index).entryId, event->globalPos());
    } else if (index >= 0 && m_items.at(index).kind == Item::Section) {
        emit bandContextRequested(m_items.at(index).band, event->globalPos());
    } else {
        emit panelContextRequested(event->globalPos());
    }
}

// ---- Rename ----

void BandStackRowList::beginRename(const QString& entryId)
{
    const Entry* entry = entryById(entryId);
    if (!entry || m_dragging) {
        return;
    }
    finishRename(true, false);
    if (m_grouped && !m_singleBand.has_value() && !isBandOpen(entry->band)) {
        m_userOpen.insert(entry->band);
    }
    const QString name = entry->name;
    m_editId = entryId;
    m_editor = new QLineEdit(this);
    m_editor->setObjectName(QStringLiteral("bandStackRowRename"));
    m_editor->setPlaceholderText(QStringLiteral("Name this spot"));
    m_editor->setText(name);
    m_editor->setFrame(false);
    m_editor->setFont(paintFont(10.5, QFont::Normal, false, true));
    m_editor->setStyleSheet(WorkspacePreview::lookQss(QStringLiteral(
        "QLineEdit { color: #ffffff; background: %1; border: 1px solid %2; border-radius: 2px;"
        " padding: 0 2px; }").arg(WorkspacePreview::roleColourName("#0a0a14", "inset"),
                                   rowColours().activeBorder.name()), m_editor));
    m_editor->installEventFilter(this);
    relayout();
    m_editor->show();
    m_editor->selectAll();
    m_editor->setFocus(Qt::OtherFocusReason);
    revealRow(entryId);
}

void BandStackRowList::placeEditor()
{
    if (!m_editor) {
        return;
    }
    const QRect row = rowRect(m_editId);
    m_editor->setGeometry(row.left() + 1 + kRowPaddingLeft,
                          row.top() + 1 + kRowPaddingTop + kFrequencyLine + kDetailLine,
                          qMax(0, row.width() - 2 - kRowPaddingLeft - kRowPaddingRight),
                          kEditorHeight);
}

void BandStackRowList::finishRename(bool save, bool keepFocus)
{
    if (m_editId.isEmpty() || !m_editor) {
        return;
    }
    const QString id = m_editId;
    const QString text = m_editor->text().trimmed();
    const Entry* entry = entryById(id);
    const bool changed = entry && entry->name != text;
    QLineEdit* editor = std::exchange(m_editor, nullptr);
    m_editId.clear();
    editor->removeEventFilter(this);
    editor->hide();
    editor->setParent(nullptr);
    editor->deleteLater();
    relayout();
    if (keepFocus) {
        setFocus(Qt::OtherFocusReason);
    }
    if (save && changed && m_access) {
        m_access->edit(id, text, std::nullopt);
    }
}

bool BandStackRowList::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_editor && m_editor) {
        switch (event->type()) {
        case QEvent::ShortcutOverride: {
            auto* key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Escape) {
                key->accept();
                return true;
            }
            break;
        }
        case QEvent::KeyPress: {
            const int key = static_cast<QKeyEvent*>(event)->key();
            if (key == Qt::Key_Return || key == Qt::Key_Enter) {
                finishRename(true, true);
                return true;
            }
            if (key == Qt::Key_Escape) {
                finishRename(false, true);
                return true;
            }
            break;
        }
        case QEvent::FocusOut:
            if (static_cast<QFocusEvent*>(event)->reason() != Qt::PopupFocusReason) {
                finishRename(true, false);
            }
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

// ---- Mouse ----

void BandStackRowList::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    const int index = itemAt(event->position().toPoint());
    m_pressed = index >= 0 ? std::optional<Item>(m_items.at(index)) : std::nullopt;
    m_pressPos = event->position().toPoint();
}

void BandStackRowList::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint pos = event->position().toPoint();
    if (m_dragging) {
        dragTo(pos.y());
        return;
    }
    // desktop.html's row(): a drag needs the grouped panel, not a flat list or
    // a popup.
    if ((event->buttons() & Qt::LeftButton) && m_pressed.has_value() && m_grouped
        && !m_singleBand.has_value() && m_pressed->kind == Item::Row
        && qAbs(pos.y() - m_pressPos.y()) > kDragStartPx) {
        beginDrag(m_pressed->entryId);
        dragTo(pos.y());
        return;
    }
    const int hover = itemAt(pos);
    if (hover != m_hoverItem) {
        m_hoverItem = hover;
        if (hover < 0) {
            unsetCursor();
        } else if (m_items.at(hover).kind != Item::Section && !m_blockedReason.isEmpty()) {
            setCursor(Qt::ForbiddenCursor);
        } else {
            setCursor(Qt::PointingHandCursor);
        }
        update();
    }
}

void BandStackRowList::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    if (m_dragging) {
        finishDrag(true);
        m_pressed.reset();
        return;
    }
    const std::optional<Item> pressed = std::exchange(m_pressed, std::nullopt);
    const int released = itemAt(event->position().toPoint());
    if (!pressed.has_value() || released < 0) {
        return;
    }
    const Item item = m_items.at(released);
    if (item.kind != pressed->kind || item.band != pressed->band
        || item.entryId != pressed->entryId) {
        return;
    }
    switch (item.kind) {
    case Item::Section:
        toggleSection(item.band);
        break;
    case Item::Add:
        addCurrentSpot();
        break;
    case Item::Row:
        m_keyboardId.clear();
        goTo(item.entryId);
        break;
    }
}

void BandStackRowList::mouseDoubleClickEvent(QMouseEvent* event)
{
    const int index = itemAt(event->position().toPoint());
    if (event->button() == Qt::LeftButton && index >= 0 && m_items.at(index).kind == Item::Row) {
        m_pressed.reset();
        beginRename(m_items.at(index).entryId);
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void BandStackRowList::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    m_hoverItem = -1;
    unsetCursor();
    update();
}

// ---- Drag ----

void BandStackRowList::beginDrag(const QString& entryId)
{
    const Entry* entry = entryById(entryId);
    if (!entry) {
        return;
    }
    finishRename(true, false);
    m_dragging = true;
    m_documentChangedInDrag = false;
    m_dragId = entryId;
    m_dragBand = entry->band;
    m_dragOriginal = rowOrderShown();
}

QStringList BandStackRowList::rowOrderShown() const
{
    QStringList ids;
    const auto found = m_bands.constFind(m_dragBand);
    if (found != m_bands.constEnd()) {
        for (const Entry& entry : found->entries) {
            ids.append(entry.id);
        }
    }
    return ids;
}

void BandStackRowList::dragTo(int y)
{
    if (auto* area = scrollAreaAround(this)) {
        const QPoint inHost = mapTo(area->widget(), QPoint(width() / 2, y));
        area->ensureVisible(inHost.x(), inHost.y(), 0, kSectionHeight);
    }
    // desktop.html's mv(): over a sibling, the row goes before it in its top
    // half and after it in its bottom half.
    for (const Item& item : std::as_const(m_items)) {
        if (item.kind != Item::Row || item.band != m_dragBand || item.entryId == m_dragId
            || y <= item.rect.top() || y >= item.rect.bottom()) {
            continue;
        }
        const bool after = y > item.rect.center().y();
        QVector<Entry>& entries = m_bands[m_dragBand].entries;
        const QStringList before = rowOrderShown();
        const auto isDragged = [this](const Entry& e) { return e.id == m_dragId; };
        const auto dragged = std::find_if(entries.begin(), entries.end(), isDragged);
        if (dragged == entries.end()) {
            return;
        }
        const Entry moving = *dragged;
        entries.erase(dragged);
        const auto target = std::find_if(entries.begin(), entries.end(), [&item](const Entry& e) {
            return e.id == item.entryId;
        });
        entries.insert(after ? target + 1 : target, moving);
        if (rowOrderShown() != before) {
            relayout();
        }
        return;
    }
    update();
}

void BandStackRowList::finishDrag(bool drop)
{
    const QStringList order = rowOrderShown();
    const Band band = m_dragBand;
    m_dragging = false;
    m_dragId.clear();
    const bool orderChanged = order != m_dragOriginal;
    if (drop && orderChanged && m_access) {
        m_access->reorder(band, order);
    }
    if (m_documentChangedInDrag || !drop) {
        reloadDocument();
    } else {
        relayout();
    }
}

// ---- Keys ----

bool BandStackRowList::event(QEvent* event)
{
    if (event->type() == QEvent::ShortcutOverride) {
        // A focused panel keeps its own keys ahead of the window's shortcuts.
        auto* key = static_cast<QKeyEvent*>(event);
        switch (key->key()) {
        case Qt::Key_F2:
        case Qt::Key_Delete:
        case Qt::Key_Backspace:
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Escape:
            key->accept();
            return true;
        case Qt::Key_U:
            if (key->modifiers() & Qt::ControlModifier) {
                key->accept();
                return true;
            }
            break;
        default:
            break;
        }
    }
    if (event->type() == QEvent::ToolTip) {
        auto* help = static_cast<QHelpEvent*>(event);
        const int index = itemAt(help->pos());
        QString tip;
        if (index >= 0 && m_items.at(index).kind != Item::Section) {
            tip = m_blockedReason;
            if (tip.isEmpty() && m_items.at(index).kind == Item::Row) {
                if (const Entry* entry = entryById(m_items.at(index).entryId)) {
                    tip = entry->name;
                }
            }
        }
        if (tip.isEmpty()) {
            QToolTip::hideText();
            event->ignore();
        } else {
            QToolTip::showText(help->globalPos(), tip, this);
        }
        return true;
    }
    return QWidget::event(event);
}

void BandStackRowList::keyPressEvent(QKeyEvent* event)
{
    if (m_dragging) {
        if (event->key() == Qt::Key_Escape) {
            finishDrag(false);
            m_pressed.reset();
        }
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        emit closeRequested();
        return;
    }
    const QStringList rows = visibleRowIds();
    if (rows.isEmpty()) {
        QWidget::keyPressEvent(event);
        return;
    }
    qsizetype index = rows.indexOf(m_keyboardId);
    if (index < 0) {
        index = rows.indexOf(placeRowId());
    }
    const QString id = rows.at(qMax<qsizetype>(0, index));
    switch (event->key()) {
    case Qt::Key_Down:
    case Qt::Key_Up: {
        const qsizetype next = index + (event->key() == Qt::Key_Down ? 1 : -1);
        m_keyboardId = rows.at(qBound<qsizetype>(0, next, rows.size() - 1));
        revealRow(m_keyboardId);
        update();
        return;
    }
    case Qt::Key_Return:
    case Qt::Key_Enter:
        goTo(id);
        return;
    case Qt::Key_F2:
        beginRename(id);
        return;
    case Qt::Key_Delete:
    case Qt::Key_Backspace:
        removeEntry(id);
        return;
    case Qt::Key_U:
        if (event->modifiers() & Qt::ControlModifier) {
            toggleLock(id);
            return;
        }
        break;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void BandStackRowList::revealRow(const QString& entryId)
{
    const QRect rect = rowRect(entryId);
    QScrollArea* area = scrollAreaAround(this);
    if (!area || !rect.isValid() || !area->widget()) {
        return;
    }
    const QPoint center = mapTo(area->widget(), rect.center());
    area->ensureVisible(center.x(), center.y(), 0, rect.height() / 2 + kRowMarginY);
}

void BandStackRowList::revealPlaceSoon()
{
    // After the layout has given the list its new height.
    QMetaObject::invokeMethod(
        this,
        [this] {
            if (m_currentBand.has_value()) {
                const QRect section = sectionRect(*m_currentBand);
                QScrollArea* area = scrollAreaAround(this);
                if (area && area->widget() && section.isValid()) {
                    const QPoint top = mapTo(area->widget(), section.center());
                    area->ensureVisible(top.x(), top.y(), 0, kSectionHeight);
                }
            }
            revealRow(placeRowId());
        },
        Qt::QueuedConnection);
}

// ---- Painting ----

QFont BandStackRowList::paintFont(qreal pixelSize, int weight, bool mono, bool italic) const
{
    QFont paint = font();
    if (mono) {
        // desktop.html's --mono.
        paint.setFamilies({QStringLiteral("Menlo"), QStringLiteral("DejaVu Sans Mono"),
                           QStringLiteral("Consolas")});
        paint.setStyleHint(QFont::Monospace);
    }
    // QFont::setPixelSize takes whole pixels and the mockup has 10.5.
    paint.setPointSizeF(pixelSize * 72.0 / qMax(1, logicalDpiY()));
    paint.setWeight(static_cast<QFont::Weight>(weight));
    paint.setItalic(italic);
    return paint;
}

void BandStackRowList::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    if (!isEnabled()) {
        painter.setOpacity(kGreyedOpacity);
    }
    const QRect dirty = event->rect();
    for (const Item& item : std::as_const(m_items)) {
        if (!item.rect.adjusted(0, 0, kDragOffsetX, 0).intersects(dirty)) {
            continue;
        }
        switch (item.kind) {
        case Item::Section:
            paintSection(painter, item);
            break;
        case Item::Row:
            paintRow(painter, item);
            break;
        case Item::Add:
            paintAdd(painter, item);
            break;
        }
    }
}

void BandStackRowList::paintSection(QPainter& painter, const Item& item) const
{
    const RowColours colours = rowColours();
    const bool hover = m_hoverItem >= 0 && m_items.at(m_hoverItem).band == item.band
        && m_items.at(m_hoverItem).kind == Item::Section;
    if (hover) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(colours.surfaceHover);
        painter.drawRoundedRect(item.rect, 3, 3);
    }
    const bool current = item.band == m_currentBand;
    QRect content = item.rect.adjusted(4, 0, -4, 0);

    painter.setFont(paintFont(9, QFont::DemiBold));
    painter.setPen(colours.faint);
    painter.drawText(QRect(content.left(), content.top(), 9, content.height()), Qt::AlignCenter,
                     isBandOpen(item.band) ? QStringLiteral("▾") : QStringLiteral("▸"));

    painter.setFont(paintFont(12, QFont::DemiBold));
    painter.setPen(current ? colours.text : colours.secondary);
    painter.drawText(content.adjusted(13, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                     bandLabel(item.band));

    const auto found = m_bands.constFind(item.band);
    int total = 0;
    int locked = 0;
    if (found != m_bands.constEnd()) {
        total = static_cast<int>(found->entries.size());
        for (const Entry& entry : found->entries) {
            locked += entry.locked ? 1 : 0;
        }
    }
    painter.setFont(paintFont(11, QFont::Normal));
    painter.setPen(colours.faint);
    const QString count = QString::number(total);
    const int countWidth = painter.fontMetrics().horizontalAdvance(count);
    painter.drawText(content, Qt::AlignRight | Qt::AlignVCenter, count);
    if (locked > 0) {
        const QString lockCount = QString::number(locked);
        const int lockCountWidth = painter.fontMetrics().horizontalAdvance(lockCount);
        const int right = content.right() - countWidth - 4;
        painter.drawText(QRect(right - lockCountWidth, content.top(), lockCountWidth + 1,
                               content.height()),
                         Qt::AlignRight | Qt::AlignVCenter, lockCount);
        paintLock(painter, QRectF(right - lockCountWidth - 4 - 9,
                                  content.center().y() - 4.5, 9, 10));
    }
}

void BandStackRowList::paintRow(QPainter& painter, const Item& item) const
{
    const Entry* entry = entryById(item.entryId);
    if (!entry) {
        return;
    }
    const RowColours colours = rowColours();
    const bool dragged = m_dragging && item.entryId == m_dragId;
    const bool hover = !m_dragging && m_hoverItem >= 0 && m_items.at(m_hoverItem).rect == item.rect;
    const bool active = isPlaceRow(item.band, item.entryId);

    painter.save();
    if (!m_blockedReason.isEmpty()) {
        painter.setOpacity(painter.opacity() * kReadOnlyRowOpacity);
    }
    if (dragged) {
        painter.setOpacity(painter.opacity() * kDragOpacity);
        painter.translate(kDragOffsetX, 0);
    }

    const QRectF box = QRectF(item.rect).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath shape;
    shape.addRoundedRect(box, kRowRadius, kRowRadius);
    if (active) {
        QLinearGradient fill(box.topLeft(), box.bottomLeft());
        fill.setColorAt(0, colours.activeTop);
        fill.setColorAt(1, colours.activeBottom);
        painter.fillPath(shape, fill);
    } else {
        painter.fillPath(shape, hover ? colours.surfaceHover : colours.surface);
    }
    const QColor plan = m_context.planColourAt ? m_context.planColourAt(entry->frequencyHz) : QColor();
    if (plan.isValid()) {
        QColor tint = plan;
        tint.setAlpha(kTintAlpha);
        QColor clear = plan;
        clear.setAlpha(0);
        QLinearGradient wash(box.topLeft(), box.topRight());
        wash.setColorAt(0, tint);
        wash.setColorAt(kTintEnd, clear);
        painter.fillPath(shape, wash);
    }
    QColor border = colours.border;
    if (active || dragged) {
        border = colours.activeBorder;
    } else if (hover) {
        border = colours.borderHover;
    }
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(border, 1));
    painter.drawPath(shape);
    if (item.entryId == m_keyboardId && hasFocus()) {
        painter.setPen(QPen(colours.keyboard, 1));
        painter.drawRoundedRect(box.adjusted(1, 1, -1, -1), kRowRadius - 1, kRowRadius - 1);
    }

    const int left = item.rect.left() + 1 + kRowPaddingLeft;
    const int textWidth = item.rect.width() - 2 - kRowPaddingLeft - kRowPaddingRight;
    int y = item.rect.top() + 1 + kRowPaddingTop;

    painter.setFont(paintFont(14, QFont::Bold, true));
    painter.setPen(active ? colours.textBright : colours.text);
    painter.drawText(QRect(left, y, textWidth, kFrequencyLine), Qt::AlignLeft | Qt::AlignVCenter,
                     BandStackStore::displayFrequency(entry->frequencyHz));
    y += kFrequencyLine;

    painter.setFont(paintFont(10.5, QFont::Normal));
    painter.setPen(colours.secondary);
    painter.drawText(QRect(left, y, textWidth, kDetailLine), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("%1 · %2")
                         .arg(entry->mode, filterWidthText(entry->filterLowHz, entry->filterHighHz)));
    y += kDetailLine;

    if (!entry->name.isEmpty() && item.entryId != m_editId) {
        painter.setFont(paintFont(10.5, QFont::Normal, false, true));
        painter.setPen(colours.name);
        painter.drawText(QRect(left, y, textWidth, kDetailLine), Qt::AlignLeft | Qt::AlignVCenter,
                         painter.fontMetrics().elidedText(entry->name, Qt::ElideRight, textWidth));
    }

    // .ic: the flags of other slices on this spot, the pin, then the lock.
    int x = item.rect.right() - kIconRight;
    const int iconTop = item.rect.top() + kIconTop;
    if (entry->locked) {
        x -= kLockWidth;
        paintLock(painter, QRectF(x, iconTop + 1, kLockWidth, kLockHeight));
        x -= kIconGap;
    }
    const auto bandData = m_bands.constFind(item.band);
    if (bandData != m_bands.constEnd() && bandData->returnMode == BandStackReturnMode::Specific
        && bandData->specificId == item.entryId) {
        x -= kLockWidth;
        paintPin(painter, QRectF(x, iconTop + 1, kLockWidth, kLockHeight));
        x -= kIconGap;
    }
    const QList<SliceFlag> flags = m_flags.value(item.entryId);
    painter.setFont(paintFont(9, QFont::Bold));
    for (auto flag = flags.crbegin(); flag != flags.crend(); ++flag) {
        x -= kFlagSize;
        const QRect chip(x, iconTop, kFlagSize, kFlagSize);
        painter.setPen(Qt::NoPen);
        painter.setBrush(VfoWidget::sliceColor(flag->id));
        painter.drawRoundedRect(chip, 2, 2);
        painter.setPen(QColor(0xff, 0xff, 0xff));
        painter.drawText(chip, Qt::AlignCenter, QString(flag->letter));
        x -= kIconGap;
    }
    painter.restore();
}

void BandStackRowList::paintAdd(QPainter& painter, const Item& item) const
{
    const RowColours colours = rowColours();
    const bool hover = m_hoverItem >= 0 && m_items.at(m_hoverItem).kind == Item::Add;
    painter.save();
    if (!m_blockedReason.isEmpty()) {
        painter.setOpacity(painter.opacity() * kReadOnlyAddOpacity);
    }
    QPen dashed(hover ? colours.activeBorder : colours.borderHover, 1, Qt::DashLine);
    painter.setPen(dashed);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(item.rect).adjusted(0.5, 0.5, -0.5, -0.5), kRowRadius, kRowRadius);
    painter.setFont(paintFont(11, QFont::Normal));
    painter.setPen(hover ? colours.textBright : colours.secondary);
    painter.drawText(item.rect, Qt::AlignCenter, QStringLiteral("+ Add current spot"));
    painter.restore();
}

} // namespace NereusSDR
