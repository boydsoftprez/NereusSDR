#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/bandstack/BandStackPanel.h  (NereusSDR)
// =================================================================
//
// The panel left of the pans that shows the Core's band stack for the window's
// selected slice: the header, the rows, the band and panel menus, the greyed
// state and the slide. The panel keeps its own width, open state and view
// options on this device (design V6). It is a child of the spectrum pane and
// never of a splitter, so saved splitter sizes do not see it.
// Design: docs/architecture/2026-11-05-band-stack-design.md, sections 6.4 and 7.
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

#include <QPointer>
#include <QString>
#include <QWidget>

#include <optional>

#include "gui/NereusGuiExport.h"
#include "models/Band.h"

class QLabel;
class QMenu;
class QScrollArea;
class QToolButton;
class QVBoxLayout;
class QVariantAnimation;

namespace NereusSDR {

class BandStackAccess;
class BandStackRowList;
class SliceModel;

class NEREUS_GUI_EXPORT BandStackPanel : public QWidget {
    Q_OBJECT

public:
    static constexpr int kDefaultWidth = 158;
    static constexpr int kMinWidth = 120;
    static constexpr int kMaxWidth = 240;
    static constexpr int kSlideMs = 140;

    static constexpr auto kOpenKey = "BandStackPanel/Open";
    static constexpr auto kWidthKey = "BandStackPanel/Width";
    static constexpr auto kGroupByBandKey = "BandStackPanel/GroupByBand";
    static constexpr auto kShowOnSpectrumKey = "BandStackPanel/ShowOnSpectrum";

    /// Reads this device's saved width and open state; an open panel starts open
    /// at once, with no slide.
    explicit BandStackPanel(BandStackAccess* access, QWidget* parent = nullptr);

    BandStackAccess* access() const;
    void setAccess(BandStackAccess* access);

    /// The slice the panel acts on, named in its header. Null shows no slice.
    void setSlice(SliceModel* slice);
    SliceModel* slice() const;

    /// A non-empty reason greys the panel and says why. Close stays usable.
    void setUnavailableReason(const QString& reason);
    QString unavailableReason() const { return m_reason; }

    /// Where the rows go.
    QVBoxLayout* rowsLayout() const { return m_rowsLayout; }
    BandStackRowList* rows() const { return m_rows; }

    /// The menu a band's header opens, into an empty menu.
    void fillBandMenu(QMenu& menu, Band band);
    /// The "..." menu, into an empty menu.
    void fillPanelMenu(QMenu& menu);

    bool isOpen() const { return m_open; }
    /// Slides over kSlideMs, or at once when animated is false or the system
    /// asks for reduced motion. Saves the state.
    void setOpen(bool open, bool animated = true);

    /// The open width, kept within kMinWidth to kMaxWidth. Saves it.
    int panelWidth() const { return m_panelWidth; }
    void setPanelWidth(int width);

    bool groupByBand() const;
    void setGroupByBand(bool on);
    bool showOnSpectrum() const;
    void setShowOnSpectrum(bool on);

    /// Test seam: stands in for the system's reduced motion setting.
    static void setReducedMotionForTest(std::optional<bool> reduced);

signals:
    void closeRequested();
    void menuRequested();
    void openChanged(bool open);
    void showOnSpectrumChanged(bool on);

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void applyStyle();
    void applyShownWidth(int width);
    void placeChildren();
    void refreshHeader();
    void refreshOnBand();
    void showBandMenu(Band band, const QPoint& globalPos);
    void showPanelMenu(const QPoint& globalPos);
    /// Asks with desktop.html's dialog: the words, Cancel and a red `verb`.
    bool confirm(const QString& text, const QString& verb);

    QPointer<BandStackAccess> m_access;
    QPointer<SliceModel> m_slice;
    QString m_reason;
    bool m_open{false};
    int m_panelWidth{kDefaultWidth};
    int m_gripStartWidth{0};
    int m_gripStartX{0};
    bool m_gripActive{false};
    QMetaObject::Connection m_sliceFrequency;

    QWidget* m_content{nullptr};
    QWidget* m_grip{nullptr};
    QWidget* m_sliceRow{nullptr};
    QLabel* m_chip{nullptr};
    QLabel* m_onBand{nullptr};
    QLabel* m_reasonLabel{nullptr};
    QToolButton* m_menuButton{nullptr};
    QToolButton* m_closeButton{nullptr};
    QScrollArea* m_rowsScroll{nullptr};
    QVBoxLayout* m_rowsLayout{nullptr};
    BandStackRowList* m_rows{nullptr};
    QVariantAnimation* m_slide{nullptr};
};

/// The system's reduced motion setting: Accessibility > Display on macOS, "Show
/// animations" on Windows. Linux has no setting Qt or the desktop share, so it
/// reads false there.
bool systemPrefersReducedMotion();

} // namespace NereusSDR
