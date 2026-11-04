#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/roundtable/RoundtableFrequencyPeek.h  (NereusSDR)
// =================================================================
//
// The floating view of another frequency on the pan (overlay design D21 to D24,
// D27): who is there and how each listener hears them, without tuning. A click
// on a tag opens it under the tag. Tune slice A here is the one thing in it that
// moves the radio.
//
// A child of the pan, so it follows the pan. It is not a SpectrumOverlayPanel, so
// it never becomes a tag keep-out, and it never takes the keyboard: the band's
// shortcuts keep working. Values update in place (D14); it changes size only when
// what it lists changes.
//
// It holds no model. MainWindow gives it a RoundtableCardModel on the 200 ms
// refresh while it is open. Escape, the close box, a click on the band and tuning
// close it; MainWindow routes all but the first two.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-03: original implementation for NereusSDR by J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code
//               (overlay redesign).
// =================================================================

#include "gui/NereusGuiExport.h"
#include "gui/roundtable/RoundtableTagLayout.h"

#include <QPoint>
#include <QString>
#include <QWidget>

#include <optional>

class QPushButton;
class QToolButton;

namespace NereusSDR {

class NEREUS_GUI_EXPORT RoundtableFrequencyPeek : public QWidget {
    Q_OBJECT

public:
    /// Board width (overlay.js peekEl, D24).
    static constexpr int kWidth = 300;

    explicit RoundtableFrequencyPeek(QWidget* parent = nullptr);
    ~RoundtableFrequencyPeek() override;

    /// Opens on `hz` in `family`, with its top left at `under` in the parent's
    /// coordinates, kept inside the parent. Until refresh(), it shows the frequency
    /// and no readings.
    void showFor(quint64 hz, const QString& family, const QPoint& under);
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
    quint64 hz() const { return m_view.card.hz; }
    QString family() const { return m_view.family; }
    /// What the view is drawing now.
    const RoundtableViewModel& viewForTest() const { return m_view; }
    int rowLimitForTest() const { return m_rowLimit; }

signals:
    void tuneRequested(quint64 hz);
    void messagesRequested(quint64 hz);
    void closed();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateTuneButton();
    void fitToContents();

    RoundtableViewModel m_view;
    RoundtablePeakTracker m_panPeak;
    QPushButton* m_tune{nullptr};
    QPushButton* m_messages{nullptr};
    QToolButton* m_close{nullptr};
    QPoint m_under;
    int m_rowLimit{0};
    int m_contentHeight{0};
    bool m_keyed{false};
    bool m_open{false};
};

} // namespace NereusSDR
