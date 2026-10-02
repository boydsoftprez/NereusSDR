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
#include <QMap>
#include <QString>
#include <QStringList>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QSplitter>
#include <QLabel>
#include <QPointer>
#include <QSet>

#include "gui/RemoteReceiverAudioNote.h"
#include "gui/setup/CoreSettingsContext.h"

#include <functional>
#include <vector>

namespace NereusSDR {

class RadioModel;
class SliceModel;
class DspReceiverSelection;
class SetupPage;
class PaGainByBandPage;
class PaWattMeterPage;
class PaValuesPage;
struct BoardCapabilities;
struct RadioInfo;
class TciServer;
class CatTciServerPage;
class AudioVaxPage;
class SettingsProxy;
class RemoteStationPage;
class CoreTargetStore;
class CoresSetupPage;

// R-R3-21 / R-R3-23: what a Setup page's settings belong to. Every page
// registration names one; registerPage() has no default, so a new page
// cannot be added without deciding.
//
//   ThisComputer -- only this computer's own settings and devices (sound
//                   cards, window preferences). Works the same in a remote
//                   window, connected or not, so the local-DSP gate does
//                   not apply to it. Reaching an audited RadioModel
//                   accessor (audioEngine(), wdspEngine(),
//                   receiverManager()) from one is a bug: the page is
//                   disabled, the accessor is logged at critical, and the
//                   Setup sweep test fails.
//   Core         -- settings the Core holds for the station.
//   Mixed        -- some of each. Core and Mixed pages keep the local-DSP
//                   gate exactly as it was before scopes existed.
enum class SetupScope {
    ThisComputer,
    Core,
    Mixed,
};

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
    /// Canonical store must outlive the dialog and all its pages. Binding is
    /// lazy, so a host can configure it before Your Cores is first visited.
    void setCoreTargets(CoreTargetStore* store);
    void setCoreSettingsContext(const CoreSettingsContext& context);
    void setCoresPageBinder(std::function<void(CoresSetupPage*)> binder);
    void inspectCoreTarget(const QString& id);
    // The Remote Access page is lazy; apply the current runtime binder now or when built.
    void setRemoteStationPageBinder(std::function<void(RemoteStationPage*)> binder);
    // Hosted desktop: receive controls follow this window's owned selection.
    // Ordinary local and remote dialogs retain RadioModel::activeSlice().
    void setReceiverSelector(std::function<SliceModel*()> selector,
                             std::function<bool()> requiresOwnedReceiver = {});
    void notifyReceiverSelectionChanged();
    // R-R3-49 (parity Task 8): an applet's right-click entry. "pgxlAdvanced",
    // "tgxlAdvanced" and "pgxlInterlock" open CAT & Network > 4O3A at their
    // own tab (Power Genius XL, Tuner Genius XL, General), "peripherals"
    // 4O3A's General tab, "rfKit" the RF-Kit page. False for an unknown key
    // (the dialog stays at its first page).
    bool selectNavigationTarget(const QString& pageKey);
    void setTransmitPermitted(bool permitted, const QString& reason = QString());
    // Fix wave 2 (M8): whether VOX can hear this computer (its microphone
    // line to the Core is open), pushed to every realized page
    // (SetupPage::setVoxPermitted). True in a local window.
    void setVoxPermitted(bool permitted, const QString& reason = QString());
    // R-R3-49 (parity Task 1): the transmit settings gate, pushed to every
    // realized page (SetupPage::setTransmitSettingsPermitted). True in a
    // local window; MainWindow pushes it in a remote one.
    // R-R3-49 (parity Task 3): minVersion above 1 is the gate for the
    // settings that transmitSettingsVersion brought, pushed through
    // SetupPage::setTransmitSettingsPermittedAt.
    void setTransmitSettingsPermitted(bool permitted, const QString& reason = QString(),
                                      int minVersion = 1);

    // R-R3-21 / R-R3-10 / R-R3-17: whether the Core's settings can be
    // changed from this window. MainWindow pushes it (applyRemoteRoleGating):
    // false in a remote window while it is disconnected from its Core or has
    // not received the Core's settings yet. While false, Core pages are
    // disabled with `reason` above them and as their tooltip, and Mixed pages
    // disable only their Core controls (SetupPage::setStationSettingsAvailable);
    // this computer's own pages stay usable. A Core page opened before the
    // first settings snapshot is not built at all: it shows the reason until
    // the Core's values arrive. When the settings become available again,
    // every realized Core and Mixed page built from an older snapshot is
    // rebuilt so it shows the Core's current values. Always true in a local
    // window, where none of this runs.
    void setStationSettingsAvailable(bool available, const QString& reason = QString());

    // Phase 3J-1 bench fix (2026-05-11): wire the live TciServer state into
    // the CatTciServerPage's Server group box title and Status label.  Pass
    // nullptr to detach (e.g. server destroyed).  The dialog forwards to the
    // page's setTciServer(); the page tracks via QPointer so the connection
    // is safe across server lifecycle changes.
    void setTciServer(class NereusSDR::TciServer* server);

    // R-R3-43 / R-R3-44: whether this remote window's receiver streams (the
    // ones feeding VAX) are Opus rather than lossless, and why (the choice,
    // or Lossless chosen but not running). MainWindow pushes it live; the
    // Audio > VAX page shows its compressed-audio note while it is not None.
    // Never pushed in a local window, where it stays None.
    void setReceiverAudioNote(RemoteReceiverAudioNote note);

public:
    // R-R3-21: the S-meter's face, peak hold or decay changed (its
    // right-click menu); Appearance > Meter Styles shows the new values if
    // it is open. MainWindow calls this.
    void reloadMeterStyles();
    void reloadFeedbackPreferences();

signals:
    void connectionsRequested();
    /// Present this window's connection details; never initiate a connection.
    void coreConnectionDetailsRequested();
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
    void hideFeedbackLevelChanged(bool hidden);
    void invertRedBluePsaChanged(bool inverted);

    // Task 3.6: forwarded from RadioInfoTab — ANAN-8000DLE volts/amps toggle.
    // MainWindow::setVoltsAmpsVisible() is the handler.
    void anan8000DleVoltsAmpsChanged(bool visible);

    // R-R3-21: forwarded from Appearance > Meter Styles. MainWindow applies
    // them to the S-meter on screen.
    void sMeterFaceChanged(int faceStyle);
    void sMeterPeakHoldChanged(bool enabled);
    void sMeterPeakDecayChanged(const QString& rate);

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
    // sub-pages. Task 16 fix wave 2 (Important 2): never hidden; on a radio
    // without power amplifier settings (!caps.hasPaProfile) the PA pages are
    // disabled with that reason, and on the receive-only kit with the kit's.
    // Forwards the caps struct to each PA page so page-level controls can
    // self-toggle (warning rows, banner labels).
    void applyPaVisibility(const BoardCapabilities& caps);
    // R-R3-49: hide PA Values when Watt Meter > Show PA Values page is off.
    void applyShowPaValuesPage();

    // ── Lazy page registry (issues #272 + #301) ───────────────────────────────
    //
    // One entry per navigation leaf. `factory` is consumed (moved out and
    // cleared) by the first realizePage() call, so a page is built at most
    // once; `widget` and `stackIndex` are the cached result.
    struct PageEntry {
        QString                   label;
        SetupScope                scope = SetupScope::Core;
        std::function<QWidget*()> factory;
        QWidget*                  widget     = nullptr;
        int                       stackIndex = -1;
        bool                      requiresTransmit = false;
        // Unavailable in a remote session: set by the local-DSP gate, or by
        // a non-empty remoteUnavailableReason on a page registered through
        // markRemoteUnavailable(). R-R3-21.
        bool                      localDspUnavailable = false;
        QString                   remoteUnavailableReason;
        // R-R3-21: a copy of the factory for Core and Mixed pages, so a page
        // realized from an older settings snapshot (or before the first one)
        // can be rebuilt with the Core's current values. Empty for
        // ThisComputer pages, which never hold the Core's settings.
        std::function<QWidget*()> rebuildFactory;
        // The settings snapshot generation the page was built from.
        int                       builtGeneration = 0;
        // A Core page opened before the Core's settings ever arrived: an
        // empty stand-in, replaced by the real page once they are available.
        bool                      placeholder = false;
        // Task 16 fix wave (I1): a page receive only disables (Thetis
        // setup.cs:6499-6501: tpTransmit, tpPowerAmplifier, grpTestTXIMD).
        // A transmit page (requiresTransmit), or one marked as a
        // non-transmit page the gate also reaches (fix wave 2: Audio > TX
        // Input, Thetis's grpBoxMic on tpTransmit).
        bool                      receiveOnlyGated = false;
        // Fix wave 2: a non-transmit page root disabled by receive only
        // (not for any other reason), enabled again when it goes off.
        bool                      receiveOnlyDisabled = false;
        // Task 16 fix wave 2 (Important 2): a PA page, disabled with
        // m_noPaReason while the radio has no power amplifier settings.
        bool                      paPage = false;
        // The page root was disabled because the Core's settings are
        // unavailable (not for any other reason), so it is enabled again,
        // and its tooltip cleared, when they return.
        bool                      stationDisabled = false;
    };

    // Registration phase: create the tree leaf and record its factory. The
    // leaf's Qt::UserRole holds the m_pages index (categories hold -1).
    // `scope` is required (R-R3-23): see SetupScope.
    QTreeWidgetItem* registerPage(QTreeWidgetItem* parent, const QString& label,
                                  SetupScope scope,
                                  std::function<QWidget*()> factory,
                                  bool requiresTransmit = false);
    void refreshTransmitPresentation();

    // R-R3-21: a leaf whose controls act on this computer's own radio
    // connection or accessories and move nothing on a Core. In a remote
    // session the page is disabled with `reason` above it; local direct
    // mode is untouched.
    void markRemoteUnavailable(QTreeWidgetItem* leaf, const QString& reason);

    // Task 16 fix wave (I1): marks a leaf, or every leaf under a category,
    // as disabled with the receive-only reason while receive only is on.
    // Every marked page must be one the gate reaches: a transmit page
    // (requiresTransmit), or, with `nonTransmitPage`, a page the gate
    // disables on its own path (fix wave 2, Minor 4: Audio > TX Input,
    // which a remote window without transmit keeps live, R-R3-36).
    void markReceiveOnlyGated(QTreeWidgetItem* item, bool nonTransmitPage = false);

    // Realization phase: build the page if it has not been built yet, add it
    // to the stack, and return it. Returns nullptr for an out-of-range index
    // or a factory that yielded nothing.
    QWidget* realizePage(int entryIndex);

    // R-R3-21: true in a window driving a remote Core (the local-DSP gate,
    // the station gate and the rebuild only ever run there).
    bool remoteSession() const;

    // R-R3-21: the Core's settings have really reached this window at
    // least once (so a Core page built now shows the Core's values, not
    // ship defaults): a snapshot with content, or the Core's seed marker.
    // An empty, unseeded snapshot does not count (R3 Setup fix wave, final
    // review I2). True when no settings proxy is installed.
    bool stationSettingsArrived() const;

    // R-R3-21: a new settings snapshot was applied. Counts it and queues a
    // rebuild of the pages built from an older one.
    void onStationSnapshotApplied();

    // R-R3-21: while the Core's settings are available, rebuild every
    // realized Core and Mixed page built from an older snapshot, and every
    // placeholder. Guarded against re-entry. R3 Setup fix wave: a page
    // with its own dialog open waits (I1); a page the local-DSP gate keeps
    // disabled is not rebuilt (M5); a factory that yields nothing leaves
    // the page it had (M3).
    void rebuildStalePages();

    // The open dialog that belongs to `page` (the page is in its QObject
    // parent chain), or nullptr. The active modal counts even when it is
    // not a QDialog (a QMessageBox subclass is, a modal QWidget is not).
    QWidget* openDialogOwnedBy(const QWidget* page) const;

    // Clears the cross-page pointers that point into a page about to be
    // replaced; the page's factory sets them again.
    void forgetPagePointersInside(const QWidget* page);

    // Realize (if needed) and raise the page for the given registry index.
    void showPageAt(int entryIndex);

    // Registry index for a leaf label, or -1 when no such leaf exists.
    int pageEntryIndex(const QString& label) const;

    // Builds the AudioBackendStrip + page container used by Setup -> Audio.
    // A member function rather than a buildTree() local because the audio
    // page factories call it after buildTree() has already returned.
    // R-R3-23: the strip (backend, Rescan, Open logs) acts on this
    // computer's sound system, so it reaches the engine through
    // RadioModel::localAudioDevices() and does not trip the local-DSP gate
    // on the page it wraps.
    QWidget* wrapWithAudioBackendStrip(SetupPage* page);

    RadioModel*      m_model   = nullptr;
    DspReceiverSelection* m_dspReceiverSelection = nullptr;
    QTreeWidget*     m_tree    = nullptr;
    QStackedWidget*  m_stack   = nullptr;
    QLabel*         m_transmitNotice = nullptr;
    bool            m_transmitPermitted = false;
    QString         m_transmitReason;
    bool            m_voxPermitted = true;   // fix wave 2 (M8)
    QString         m_voxReason;
    bool            m_transmitSettingsPermitted = false;  // R-R3-49
    QString         m_transmitSettingsReason;              // R-R3-49
    // R-R3-49 (parity Task 3): the gate for each later transmitSettingsVersion.
    struct TransmitSettingsGate {
        bool permitted = false;
        QString reason;
    };
    QMap<int, TransmitSettingsGate> m_transmitSettingsGates;
    // R-R3-21: the visible reason for a page the local-DSP gate disabled.
    // Shown above the page (objectName "setupLocalUnavailable") and as the
    // page's and its tree leaf's tooltip. Never shown in local direct mode,
    // where the gate does not run.
    QLabel*         m_localUnavailableNotice = nullptr;
    QString         m_localUnavailableReason;
    // R-R3-21: the Core's settings availability (see
    // setStationSettingsAvailable) and its notice (objectName
    // "setupStationUnavailable").
    bool            m_stationAvailable = true;
    QString         m_stationReason;
    QLabel*         m_stationNotice = nullptr;
    // Task 16 fix wave (I1): the receive-only reason above a page it
    // disables (objectName "setupReceiveOnly"), and the categories whose
    // tree rows carry it as their tooltip (Transmit, PA).
    QLabel*         m_receiveOnlyNotice = nullptr;
    std::vector<QTreeWidgetItem*> m_receiveOnlyCategories;
    // Task 16 fix wave 2 (Important 2): whether the radio has power
    // amplifier settings (applyPaVisibility), the reason the PA pages are
    // disabled when it has none, and its notice above the page (objectName
    // "setupNoPowerAmplifier").
    bool            m_paAvailable = true;
    QString         m_noPaReason;
    QLabel*         m_noPaNotice = nullptr;
    QPointer<SettingsProxy> m_settingsProxy;
    int             m_snapshotGeneration = 0;
    bool            m_rebuildingPages = false;
    // R3 Setup fix wave (I1): a rebuild waited for a page's own dialog; it
    // runs when that dialog is destroyed or the next page is shown.
    bool            m_rebuildPostponed = false;
    QSet<QWidget*>  m_rebuildWaitsFor;

    std::vector<PageEntry> m_pages;
    CoreTargetStore* m_coreTargets = nullptr;
    CoreSettingsContext m_coreSettingsContext;
    QString m_inspectedCoreTarget;
    std::function<void(CoresSetupPage*)> m_coresPageBinder;
    std::function<void(RemoteStationPage*)> m_remoteStationPageBinder;

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

    // R-R3-43 / R-R3-44: the value setReceiverAudioNote() last got,
    // replayed into the VAX page when it is realized.
    RemoteReceiverAudioNote m_receiverAudioNote = RemoteReceiverAudioNote::None;
    QPointer<AudioVaxPage> m_vaxPage;

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
    // R-R3-49: whether the PA Values leaf is hidden (Watt Meter > Show PA
    // Values page).
    bool isPaValuesPageHiddenForTest() const { return m_paValuesItem && m_paValuesItem->isHidden(); }
    void applyPaVisibilityForTest(const BoardCapabilities& caps) { applyPaVisibility(caps); }

    // #272 / #301 lazy-construction seams. Defined inline because
    // NEREUS_BUILD_TESTS is set on the test targets only, not on
    // NereusSDRLib -- an out-of-line body in SetupDialog.cpp would never be
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

    // Remote-daemon R2 Task 20. Every registered leaf label, in
    // registration order, so a test can sweep the tree without hardcoding
    // a list that goes stale the next time a page is added.
    //
    // Labels are NOT unique: "Options" is registered twice (General and
    // DSP). A sweep must iterate by INDEX, via realizePageAtForTest below,
    // or pageEntryIndex() resolves the second one back to the first and the
    // second leaf is silently never visited.
    QStringList pageLabelsForTest() const
    {
        QStringList labels;
        labels.reserve(static_cast<int>(m_pages.size()));
        for (const PageEntry& entry : m_pages) {
            labels << entry.label;
        }
        return labels;
    }

    // Remote-daemon R2 Task 20. Force-realize one leaf by REGISTRY INDEX.
    //
    // The label-keyed realizePageForTest() cannot express "every leaf":
    // two leaves share the label "Options", so a label-driven loop realizes
    // the General one twice and never builds the DSP one. A gate sweep that
    // skips a page is worse than no sweep, because it reports a clean run.
    QWidget* realizePageAtForTest(int entryIndex)
    {
        return realizePage(entryIndex);
    }

    // Remote-daemon R2 Task 20. The realized widget for a leaf, or nullptr
    // if that leaf has not been realized. Distinct from realizePageForTest,
    // which builds on demand: a gating test needs to ask about the widget
    // WITHOUT the question itself constructing one.
    QWidget* realizedPageForTest(const QString& label) const
    {
        const int index = pageEntryIndex(label);
        if (index < 0) {
            return nullptr;
        }
        return m_pages[static_cast<std::size_t>(index)].widget;
    }

    // R-R3-21: the pushed availability of the Core's settings.
    bool stationSettingsAvailableForTest() const { return m_stationAvailable; }

    // R-R3-21: true while a leaf shows the stand-in for a Core page opened
    // before the Core's settings arrived. False for unknown labels.
    bool isPagePlaceholderForTest(const QString& label) const
    {
        const int index = pageEntryIndex(label);
        return index >= 0 && m_pages[static_cast<std::size_t>(index)].placeholder;
    }

    // R-R3-23: the scope a leaf was registered with, by REGISTRY INDEX
    // (labels are not unique; see pageLabelsForTest).
    SetupScope pageScopeAtForTest(int entryIndex) const
    {
        return m_pages[static_cast<std::size_t>(entryIndex)].scope;
    }

    // R-R3-23: register one extra leaf under a "Test" category of its own,
    // after buildTree(), so the sweep can be proved against a page that
    // breaks the ThisComputer rule on purpose. Returns its registry index.
    int registerPageForTest(const QString& label, SetupScope scope,
                            std::function<QWidget*()> factory)
    {
        auto* category = new QTreeWidgetItem(m_tree, QStringList{QStringLiteral("Test pages")});
        category->setData(0, Qt::UserRole, -1);
        QTreeWidgetItem* leaf = registerPage(category, label, scope, std::move(factory));
        return leaf->data(0, Qt::UserRole).toInt();
    }
#endif
};

} // namespace NereusSDR
