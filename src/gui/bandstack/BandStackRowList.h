#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/bandstack/BandStackRowList.h  (NereusSDR)
// =================================================================
//
// The band stack's rows for one slice, drawn as desktop.html's .sec and .en:
// bands grouped with the current one open, or one flat list, or one band for a
// popup. Clicking, renaming, dragging, the keys and the row menu all end in a
// BandStackAccess verb, and the Core's document is what the rows then show. A
// refusal comes back through the access, so a listener can read the rows and is
// told why a recall did not happen.
// Design: docs/architecture/2026-11-05-band-stack-design.md, section 7.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-06 - New for NereusSDR by J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-11-06 - contentHeightChanged and a public revealPlaceSoon, for the
//                 popup (band stack plan Task 10). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QColor>
#include <QHash>
#include <QList>
#include <QMap>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>
#include <optional>

#include "gui/NereusGuiExport.h"
#include "models/Band.h"
#include "models/bandstack/BandStackController.h"
#include "models/bandstack/BandStackStore.h"

class QLineEdit;
class QMenu;

namespace NereusSDR {

class BandStackAccess;

/// A slice as the rows need to know it, to flag the spot it is on.
struct BandStackSliceInfo {
    int id{-1};
    QChar letter;
    double frequencyHz{0.0};
};

/// What the rows ask the window. Any of these may be empty, which reads as an
/// empty view, no other slices, no tint and a slice that is not blocked.
struct BandStackRowContext {
    std::function<BandStackView(int sliceId)> viewForSlice;
    std::function<QList<BandStackSliceInfo>()> slices;
    /// The band plan segment's colour at a frequency, invalid outside the plan.
    std::function<QColor(qint64 frequencyHz)> planColourAt;
    /// Why a recall on this slice will be refused, empty when it will not be.
    /// The rows dim and say so; the refusal itself comes from the access.
    std::function<QString(int sliceId)> blockedReason;
};

class NEREUS_GUI_EXPORT BandStackRowList : public QWidget {
    Q_OBJECT

public:
    explicit BandStackRowList(BandStackAccess* access, QWidget* parent = nullptr);

    void setAccess(BandStackAccess* access);
    void setContext(BandStackRowContext context);

    /// The slice the rows act on, -1 for none. A new slice opens its own band
    /// again, as a change of band does.
    void setSliceId(int sliceId);
    int sliceId() const { return m_sliceId; }
    /// Where that slice is now. Decides the current band.
    void setSliceFrequency(std::optional<double> frequencyHz);
    /// The band the slice is on, as the Core counts it.
    std::optional<Band> currentBand() const { return m_currentBand; }

    /// One band with no group headers (a popup), or nullopt for every band.
    void setBand(std::optional<Band> band);
    /// With every band shown: grouped under band headers, or one list in
    /// frequency order.
    void setGrouped(bool grouped);
    bool isGrouped() const { return m_grouped; }

    /// The slices moved, or a slice's blocked reason or the band plan changed.
    void refreshSlices();

    bool isBandOpen(Band band) const;
    void setBandOpen(Band band, bool open);

    // What the band menu needs from the document.
    int entryCount(Band band) const;
    BandStackReturnMode returnMode(Band band) const;
    /// The entry Specific entry would pick: this slice's place, else the first
    /// voice entry, else the first. Empty on an empty band.
    QString defaultSpecificEntry(Band band) const;

    /// The row menu, for `entryId`, into an empty menu.
    void fillRowMenu(QMenu& menu, const QString& entryId);
    /// Opens the name editor on a row, opening its band first if need be.
    void beginRename(const QString& entryId);
    /// Scrolls the slice's place into view, once the layout has settled.
    void revealPlaceSoon();

    // What the rows show now.
    QStringList visibleRowIds() const;
    QRect rowRect(const QString& entryId) const;
    QRect sectionRect(Band band) const;
    QRect addRect() const;
    QString keyboardRowId() const { return m_keyboardId; }
    QString placeRowId() const;
    /// The band a dragged row is in, while a drag is under way.
    bool isDragging() const { return m_dragging; }
    QString blockedReason() const { return m_blockedReason; }

    QSize sizeHint() const override;

signals:
    /// A right-click on a band's header.
    void bandContextRequested(Band band, QPoint globalPos);
    /// A right-click on no row and no header.
    void panelContextRequested(QPoint globalPos);
    /// Esc, with the rows focused.
    void closeRequested();
    void currentBandChanged();
    /// The rows' height changed: a row or the add row came or went.
    void contentHeightChanged();

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    struct Entry {
        QString id;
        Band band{Band::GEN};
        qint64 frequencyHz{0};
        QString mode;
        int filterLowHz{0};
        int filterHighHz{0};
        bool locked{false};
        QString name;
    };
    struct BandData {
        QVector<Entry> entries;
        BandStackReturnMode returnMode{BandStackReturnMode::Current};
        QString specificId;
    };
    struct SliceFlag {
        int id{-1};
        QChar letter;
        bool operator==(const SliceFlag& other) const
        {
            return id == other.id && letter == other.letter;
        }
    };
    struct Item {
        enum Kind { Section, Row, Add } kind{Section};
        Band band{Band::GEN};
        QString entryId;
        QRect rect;
    };

    void reloadDocument();
    void refreshDerived();
    void relayout();
    void addEntryItems(const QVector<Entry>& entries, int& y, int& margin);
    int itemAt(const QPoint& pos) const;
    const Entry* entryById(const QString& id) const;
    QChar sliceLetter() const;
    bool isPlaceRow(Band band, const QString& entryId) const;
    std::optional<Band> stackBandFor(QChar letter, double frequencyHz) const;
    BandStackView currentView() const;
    QStringList rowOrderShown() const;

    void goTo(const QString& entryId);
    void addCurrentSpot();
    void toggleLock(const QString& entryId);
    void removeEntry(const QString& entryId);
    void toggleSection(Band band);

    void finishRename(bool save, bool keepFocus);
    void placeEditor();

    void beginDrag(const QString& entryId);
    void dragTo(int y);
    void finishDrag(bool drop);

    void showRowMenu(const QString& entryId, const QPoint& globalPos);
    void revealRow(const QString& entryId);

    void paintSection(QPainter& painter, const Item& item) const;
    void paintRow(QPainter& painter, const Item& item) const;
    void paintAdd(QPainter& painter, const Item& item) const;
    QFont paintFont(qreal pixelSize, int weight, bool mono = false, bool italic = false) const;

    QPointer<BandStackAccess> m_access;
    BandStackRowContext m_context;
    int m_sliceId{-1};
    std::optional<double> m_sliceFrequencyHz;
    std::optional<Band> m_currentBand;
    std::optional<Band> m_singleBand;
    bool m_grouped{true};

    bool m_hasDocument{false};
    QMap<Band, BandData> m_bands;
    QHash<QChar, QHash<Band, QString>> m_places;
    QHash<QString, QList<SliceFlag>> m_flags;
    QString m_blockedReason;

    // desktop.html's userOpen and userClosed: the current band opens when it
    // changes, and one you collapsed stays collapsed until it does.
    QSet<Band> m_userOpen;
    QSet<Band> m_userClosed;

    QVector<Item> m_items;
    int m_contentHeight{0};
    int m_hoverItem{-1};
    QString m_keyboardId;

    // The press a release may turn into a click. A copy, because a document
    // can arrive between the two and lay the rows out again.
    std::optional<Item> m_pressed;
    QPoint m_pressPos;

    bool m_dragging{false};
    bool m_documentChangedInDrag{false};
    QString m_dragId;
    Band m_dragBand{Band::GEN};
    QStringList m_dragOriginal;

    QString m_editId;
    QLineEdit* m_editor{nullptr};
};

} // namespace NereusSDR
