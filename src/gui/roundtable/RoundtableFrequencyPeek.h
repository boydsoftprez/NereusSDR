#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/roundtable/RoundtableFrequencyPeek.h  (NereusSDR)
// =================================================================
//
// The one inspector on the pan (overlay design D21 to D24, D27, D34, D35, D37): who
// is on a frequency and how each listener hears them, your own or another, without
// tuning. Tune slice A here is the one thing in it that moves the radio.
//
// It hangs from the Roundtable strip over the waterfall, lined up with your line
// (strip design 5), or stands on the divider over the spectrum when the
// waterfall is too short. Short room folds everyone not keyed to "Also here", and
// what still does not fit scrolls by the wheel between the head and the buttons,
// which stay put.
//
// A child of the pan, so it follows the pan. It is not a SpectrumOverlayPanel, so
// it never becomes a tag keep-out, and it never takes the keyboard: the band's
// shortcuts keep working. Values update in place (D14); it changes size only when
// what it lists changes.
//
// It holds no model. MainWindow gives it a RoundtableCardModel on the 200 ms
// refresh while it is open. Escape and the close box close it here; Tune, a refused
// read and a change of pan close it from MainWindow. A click on the band does not
// (D27). Back and ⋯ only ask. It can also list a fold's frequencies (showFold, D37),
// and a row only asks too.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-16: Review fixes: details opened upward line up with their own left
//               edge (setInspectorSpot's upwardAnchorLeft) and paint opaque.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-11-16: The details open over the waterfall under the strip, or upward
//               over the spectrum when the waterfall is short:
//               setInspectorSpot takes the two areas and the left edge
//               (placeRoundtableDetails). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-11-04: A fold's list (showFold, setFoldRows, foldRowClicked). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-11-04: The one inspector: your own frequency, ⋯, the Viewing line with
//               Back, the foot anchor and the scrolling body. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-11-03: original implementation for NereusSDR by J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code
//               (overlay redesign).
// =================================================================

#include "gui/NereusGuiExport.h"
#include "gui/roundtable/RoundtableTagLayout.h"

#include <QPoint>
#include <QRect>
#include <QString>
#include <QVector>
#include <QWidget>

#include <optional>

class QPushButton;
class QToolButton;

namespace NereusSDR {

class NEREUS_GUI_EXPORT RoundtableFrequencyPeek : public QWidget {
    Q_OBJECT

public:
    /// Board width (overlay.js peekEl, D24).
    static constexpr int kWidth = kRoundtableInspectorWidth;

    explicit RoundtableFrequencyPeek(QWidget* parent = nullptr);
    ~RoundtableFrequencyPeek() override;

    /// Opens on another frequency, under the strip (strip design 5). While open it
    /// swaps in place, with no close and reopen. Until refresh(), it shows the
    /// frequency and no readings.
    void showFor(quint64 hz, const QString& family);
    /// The same on your own frequency: no Viewing line and no Your pan row, and Tune
    /// greyed with its reason (D22, D35).
    void showOwn(quint64 hz, const QString& family);
    /// Lists a fold's frequencies, under the strip, with only the close box.
    void showFold(const QVector<RoundtableFoldRow>& rows);
    /// The list's rows in place, no reopen.
    void setFoldRows(const QVector<RoundtableFoldRow>& rows);
    /// Where it may stand: see placeRoundtableDetails. `below` and `above` are in
    /// the parent's coordinates. Moves it only when something here changed.
    /// `upwardAnchorLeft` is the left edge when they open upward; none means
    /// `anchorLeft` there too.
    void setInspectorSpot(const QRect& below, const QRect& above, int anchorLeft,
                          std::optional<int> upwardAnchorLeft = std::nullopt);
    /// The active slice, for the offset line and for "already there".
    void setSlice(quint64 hz, const QString& family, const QString& name);
    /// The station's distance and bearing preferences.
    void setPathPreferences(bool miles, bool bearing);
    /// The latest of the viewed frequency. `panDbm` is this client's own pan
    /// (D23). `keyed` disables Tune at once and leaves the view open; so does no
    /// `connected` radio, with its own reason.
    void refresh(const RoundtableCardModel& model, std::optional<double> panDbm, bool keyed, bool connected = true);
    /// Closes, once: closed() follows only if it was open.
    void closePeek();

    bool isOpen() const { return m_open; }
    /// Open on your own frequency.
    bool showsOwn() const { return m_open && !m_listing && m_view.mine; }
    /// Open on a fold's list.
    bool showsFold() const { return m_open && m_listing; }
    const QVector<RoundtableFoldRow>& foldRowsForTest() const { return m_rows; }
    /// Where it is in the parent's coordinates; null while closed.
    QRect inspectorRect() const { return m_open ? geometry() : QRect(); }
    quint64 hz() const { return m_view.card.hz; }
    QString family() const { return m_view.family; }
    /// What the view is drawing now.
    const RoundtableViewModel& viewForTest() const { return m_view; }
    /// How many times the view has worked out its size: a value-only refresh
    /// does not (overlay design D14).
    int fitsForTest() const { return m_fits; }
    bool foldedForTest() const { return m_fold; }
    int scrollForTest() const { return m_scroll; }
    int maxScrollForTest() const;
    int bodyHeightForTest() const { return m_bodyHeight; }
    /// Standing on the divider over the spectrum, not hanging under the strip.
    bool upwardForTest() const { return m_upward; }

signals:
    void tuneRequested(quint64 hz);
    void messagesRequested(quint64 hz);
    void closed();
    /// Back to my slice was pressed. It tunes nothing.
    void backRequested();
    /// ⋯ was pressed. `globalPos` is under the button, where its menu goes.
    void moreRequested(QPoint globalPos);
    /// A row of a fold's list was clicked. It tunes nothing.
    void foldRowClicked(QString roomId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void open(quint64 hz, const QString& family, bool mine);
    void updateTuneButton();
    void fitToContents();
    void fitList();
    RoundtableDetailsPlacement placeFor(int height) const;
    void placeHeadButtons(const QRect& wanted);
    void setHoverRow(int row);

    RoundtableViewModel m_view;
    RoundtablePeakTracker m_panPeak;
    int m_fits = 0;
    QPushButton* m_tune{nullptr};
    QPushButton* m_messages{nullptr};
    QToolButton* m_close{nullptr};
    QToolButton* m_more{nullptr};
    QPushButton* m_back{nullptr};
    QRect m_spotBelow;
    QRect m_spotAbove;
    int m_spotAnchorLeft{0};
    std::optional<int> m_spotUpwardAnchorLeft;
    bool m_upward{false};
    RoundtableViewLayout m_layout;
    bool m_fold{false};
    int m_scroll{0};
    int m_bodyHeight{0};
    bool m_keyed{false};
    bool m_connected{true};
    bool m_open{false};
    bool m_listing{false};
    QVector<RoundtableFoldRow> m_rows;
    int m_hoverRow{-1};
};

} // namespace NereusSDR
