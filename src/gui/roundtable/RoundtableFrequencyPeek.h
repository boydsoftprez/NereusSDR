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
// It stands on the foot line in your card's spot (showOwn, or showFor without a
// point), or under a point on the pan (showFor with one). A short pan folds everyone
// not keyed to "Also here", and what still does not fit scrolls by the wheel between
// the head and the buttons, which stay put.
//
// A child of the pan, so it follows the pan. It is not a SpectrumOverlayPanel, so
// it never becomes a tag keep-out, and it never takes the keyboard: the band's
// shortcuts keep working. Values update in place (D14); it changes size only when
// what it lists changes.
//
// It holds no model. MainWindow gives it a RoundtableCardModel on the 200 ms
// refresh while it is open. Escape, the close box, a click on the band and tuning
// close it; MainWindow routes all but the first two. Back and ⋯ only ask.
//
// =================================================================
// Modification history (NereusSDR):
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

    /// Opens on `hz` in `family`, with its top left at `under` in the parent's
    /// coordinates, kept inside the parent. Until refresh(), it shows the frequency
    /// and no readings.
    void showFor(quint64 hz, const QString& family, const QPoint& under);
    /// Opens on another frequency, standing on the foot line (D34). While open it
    /// swaps in place, with no close and reopen.
    void showFor(quint64 hz, const QString& family);
    /// The same on your own frequency: no Viewing line and no Your pan row, and Tune
    /// greyed with its reason (D22, D35).
    void showOwn(quint64 hz, const QString& family);
    /// Where the foot-line spot is: see placeRoundtableInspector. `pan` is in the
    /// parent's coordinates. Moves it only when something here changed.
    void setInspectorSpot(const QRect& pan, int dialX, const QVector<QRect>& flags, bool flagFlipped);
    /// The active slice, for the offset line and for "already there".
    void setSlice(quint64 hz, const QString& family, const QString& name);
    /// The station's distance and bearing preferences.
    void setPathPreferences(bool miles, bool bearing);
    /// The latest of the viewed frequency. `panDbm` is this client's own pan
    /// (D23). `keyed` disables Tune at once and leaves the view open.
    void refresh(const RoundtableCardModel& model, std::optional<double> panDbm, bool keyed);
    /// Closes, once: closed() follows only if it was open.
    void closePeek();

    bool isOpen() const { return m_open; }
    /// Open on your own frequency.
    bool showsOwn() const { return m_open && m_view.mine; }
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

signals:
    void tuneRequested(quint64 hz);
    void messagesRequested(quint64 hz);
    void closed();
    /// Back to my slice was pressed. It tunes nothing.
    void backRequested();
    /// ⋯ was pressed. `globalPos` is under the button, where its menu goes.
    void moreRequested(QPoint globalPos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void open(quint64 hz, const QString& family, bool mine);
    void updateTuneButton();
    void fitToContents();

    RoundtableViewModel m_view;
    RoundtablePeakTracker m_panPeak;
    int m_fits = 0;
    QPushButton* m_tune{nullptr};
    QPushButton* m_messages{nullptr};
    QToolButton* m_close{nullptr};
    QToolButton* m_more{nullptr};
    QPushButton* m_back{nullptr};
    QPoint m_under;
    bool m_onFoot{false};
    QRect m_spotPan;
    int m_spotDialX{0};
    QVector<QRect> m_spotFlags;
    bool m_spotFlipped{false};
    RoundtableViewLayout m_layout;
    bool m_fold{false};
    int m_scroll{0};
    int m_bodyHeight{0};
    bool m_keyed{false};
    bool m_open{false};
};

} // namespace NereusSDR
