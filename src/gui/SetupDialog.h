#pragma once

// =================================================================
// src/gui/SetupDialog.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original Qt6 navigation shell for the Settings dialog.
// Independently implemented from Thetis Setup Form interface design;
// no direct C# port. See SetupDialog.cpp for inline citations to
// Thetis behavior rules consulted during implementation.
// =================================================================

#include <QDialog>
#include <QString>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QSplitter>

#include <functional>
#include <vector>

namespace NereusSDR {

class RadioModel;
class SetupPage;
class PaGainByBandPage;
class PaWattMeterPage;
class PaValuesPage;
struct BoardCapabilities;
struct RadioInfo;
class TciServer;
class CatTciServerPage;

// Main settings dialog with tree-based navigation.
// Left pane: QTreeWidget with top-level category items.
// Right pane: QStackedWidget showing the selected page.
//
// Pages are built lazily (issues #272 + #301). buildTree() only registers a
// label plus a factory callable per leaf; the widget itself is constructed by
// realizePage() the first time its tree node is selected, then cached for the
// lifetime of the dialog. See SetupDialog.cpp for the cross-page wiring rules
// that fall out of that split.
class SetupDialog : public QDialog {
    Q_OBJECT
public:
    explicit SetupDialog(RadioModel* model, QWidget* parent = nullptr);

    // Navigate to a page by its label text (e.g. "AGC/ALC").
    void selectPage(const QString& label);

    // Phase 3J-1 bench fix (2026-05-11): wire the live TciServer state into
    // the CatTciServerPage's Server group box title and Status label.  Pass
    // nullptr to detach (e.g. server destroyed).  The dialog forwards to the
    // page's setTciServer(); the page tracks via QPointer so the connection
    // is safe across server lifecycle changes.
    void setTciServer(class NereusSDR::TciServer* server);

signals:
    // Phase 3M-3a-ii Batch 6 (Task 3): forwarded from CfcSetupPage's
    // [Configure CFC bands…] button.  MainWindow connects this to the
    // TxApplet::requestOpenCfcDialog() slot so the modeless TxCfcDialog
    // instance is shared between the Setup page and the [CFC] right-click
    // on the TxApplet.
    void cfcDialogRequested();

    // Phase 3J-1 review P2.4: forwarded from CatTciServerPage — enable checkbox
    // toggled.  MainWindow connects this to call TciServer::start() / stop()
    // so the server goes live immediately without a disconnect/reconnect cycle.
    void tciServerEnableToggled(bool on, quint16 port);

    // Phase 3J-1 closeout Item 1 (2026-05-12): forwarded from CatTciServerPage —
    // bind-interface dropdown or port spinbox changed.  MainWindow restarts the
    // server live if it's running so the new bind/port takes effect without a
    // manual disable/enable cycle.
    void tciServerBindOrPortChanged(const QString& bindAddress, quint16 port);

    // Phase 3J-1 closeout Item 2 (2026-05-12): forwarded from CatTciServerPage —
    // "Show Log..." button clicked.  MainWindow lazy-constructs the TciLogWindow
    // and connects it to TciServer::messageLogged so the window outlives this
    // dialog closing.
    void tciShowLogRequested();

    // Task 3.6: forwarded from GeneralOptionsPage — CPU meter rate spinbox.
    // MainWindow::setCpuTimerIntervalHz() is the handler.
    void cpuMeterRateChanged(int hz);

    // Task 3.6: forwarded from RadioInfoTab — ANAN-8000DLE volts/amps toggle.
    // MainWindow::setVoltsAmpsVisible() is the handler.
    void anan8000DleVoltsAmpsChanged(bool visible);

    // Phase 3P-II Phase 4 Task 95: forwarded from TgxlAdvancedPage::antennaLabelChanged.
    // MainWindow::wireSetupDialog connects this to TunerApplet::onAntennaLabelChanged.
    // index is 1..3; label is the new text (empty resets to "ANT N" default).
    void tgxlAntennaLabelChanged(int index, const QString& label);

protected:
    void showEvent(QShowEvent* event) override;

private slots:
    // Phase 8 of #167: drives per-SKU PA category + page visibility from
    // RadioModel::currentRadioChanged. Mirrors Thetis
    // comboRadioModel_SelectedIndexChanged (setup.cs:19812-20310
    // [v2.10.3.13+501e3f51]) per-SKU PA tab visibility.
    void onCurrentRadioChanged(const RadioInfo& info);

private:
    void buildTree();

    // Phase 8 of #167: applies BoardCapabilities to the PA category +
    // sub-page visibility. Hides the category root when caps.isRxOnlySku
    // or !caps.hasPaProfile; forwards the caps struct to each PA page so
    // page-level controls can self-toggle (warning rows, banner labels).
    void applyPaVisibility(const BoardCapabilities& caps);

    // ── Lazy page registry (issues #272 + #301) ───────────────────────────────
    //
    // One entry per navigation leaf. `factory` is consumed (moved out and
    // cleared) by the first realizePage() call, so a page is built at most
    // once; `widget` and `stackIndex` are the cached result.
    struct PageEntry {
        QString                   label;
        std::function<QWidget*()> factory;
        QWidget*                  widget     = nullptr;
        int                       stackIndex = -1;
    };

    // Registration phase: create the tree leaf and record its factory. The
    // leaf's Qt::UserRole holds the m_pages index (categories hold -1).
    QTreeWidgetItem* registerPage(QTreeWidgetItem* parent, const QString& label,
                                  std::function<QWidget*()> factory);

    // Realization phase: build the page if it has not been built yet, add it
    // to the stack, and return it. Returns nullptr for an out-of-range index
    // or a factory that yielded nothing.
    QWidget* realizePage(int entryIndex);

    // Realize (if needed) and raise the page for the given registry index.
    void showPageAt(int entryIndex);

    // Registry index for a leaf label, or -1 when no such leaf exists.
    int pageEntryIndex(const QString& label) const;

    // Builds the AudioBackendStrip + page container used by Setup -> Audio.
    // A member function rather than a buildTree() local because the audio
    // page factories call it after buildTree() has already returned.
    QWidget* wrapWithAudioBackendStrip(SetupPage* page);

    RadioModel*      m_model   = nullptr;
    QTreeWidget*     m_tree    = nullptr;
    QStackedWidget*  m_stack   = nullptr;

    std::vector<PageEntry> m_pages;

    // Phase 3J-1 bench fix (2026-05-11): store the TciServer page reference
    // so setTciServer() can forward to it without a tree-walk lookup.
    CatTciServerPage* m_tciServerPage = nullptr;

    // #272 / #301: MainWindow::wireSetupDialog() calls setTciServer() straight
    // after construction, long before the operator visits CAT & Network -> TCI
    // Server. Hold the pointer here and replay it into the page when it is
    // realized. Raw rather than QPointer because TciServer.h is entirely
    // #ifdef HAVE_WEBSOCKETS, so QPointer<TciServer> would not compile in
    // non-WebSocket builds. Safe: MainWindow parents its single TciServer to
    // itself and never destroys it independently, so it outlives this dialog.
    TciServer* m_pendingTciServer = nullptr;

    // Phase 8 of #167: PA category nav-tree root + 3 child items, plus
    // the page widgets themselves so applyPaVisibility() can toggle each.
    // The three page pointers stay null until their leaf is realized;
    // applyPaVisibility() already null-checks each one.
    QTreeWidgetItem* m_paCategoryItem  = nullptr;
    QTreeWidgetItem* m_paGainItem      = nullptr;
    QTreeWidgetItem* m_paWattMeterItem = nullptr;
    QTreeWidgetItem* m_paValuesItem    = nullptr;
    PaGainByBandPage* m_paGainPage     = nullptr;
    PaWattMeterPage*  m_paWattMeterPage= nullptr;
    PaValuesPage*     m_paValuesPage   = nullptr;

    // #272 / #301: registry index of the "PA Values" leaf. The Watt Meter
    // page's [Reset PA Values] button routes through the dialog, which
    // realizes this sibling on demand instead of relying on it having been
    // constructed up front.
    int m_paValuesEntry = -1;

#ifdef NEREUS_BUILD_TESTS
public:
    // Phase 9 of #167: test seams for verifying the cross-page wiring
    // connect() between PaWattMeterPage::resetPaValuesRequested and
    // PaValuesPage::resetPaValues(). Both stay null until the matching leaf
    // is realized -- call realizePageForTest("Watt Meter") first.
    PaWattMeterPage* paWattMeterPageForTest() const { return m_paWattMeterPage; }
    PaValuesPage*    paValuesPageForTest()    const { return m_paValuesPage;    }

    // #272 / #301 lazy-construction seams. Defined inline because
    // NEREUS_BUILD_TESTS is set on the test targets only, not on
    // NereusSDRObjs -- an out-of-line body in SetupDialog.cpp would never be
    // compiled and the test link would fail.

    // Number of navigation leaves registered by buildTree().
    int registeredPageCountForTest() const { return static_cast<int>(m_pages.size()); }

    // Number of leaves whose widget has actually been constructed.
    int realizedPageCountForTest() const
    {
        int count = 0;
        for (const PageEntry& entry : m_pages) {
            if (entry.widget != nullptr) {
                ++count;
            }
        }
        return count;
    }

    // True when the named leaf has been realized. False for unknown labels.
    bool isPageRealizedForTest(const QString& label) const
    {
        const int index = pageEntryIndex(label);
        if (index < 0) {
            return false;
        }
        return m_pages[static_cast<std::size_t>(index)].widget != nullptr;
    }

    // Force-realize one leaf by label; returns the page widget (or its
    // container for wrapped pages), nullptr for unknown labels.
    QWidget* realizePageForTest(const QString& label)
    {
        return realizePage(pageEntryIndex(label));
    }

    // Force-realize every registered leaf. Used by the Qt-warning regression
    // test, which must still see every page constructed to stay meaningful.
    void realizeAllPagesForTest()
    {
        for (int i = 0; i < static_cast<int>(m_pages.size()); ++i) {
            realizePage(i);
        }
    }
#endif
};

} // namespace NereusSDR
