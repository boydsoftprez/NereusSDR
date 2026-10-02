// =================================================================
// src/gui/SetupDialog.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original Qt6 navigation shell for the Settings dialog.
// Independently implemented from Thetis Setup Form interface design;
// no direct C# port. Inline cites to Thetis files indicate per-SKU
// behaviour rules consulted while implementing visibility wiring.
//
// Modification history (NereusSDR):
//   2026-05-03 — PA calibration safety hotfix Phase 8 (#167): rewired
//                 the Setup → PA category to be always-built, with
//                 per-SKU visibility driven by BoardCapabilities and
//                 RadioModel::currentRadioChanged. Replaces the
//                 construction-time hasPaProfile gate that prevented
//                 dynamic visibility on radio swaps. Visibility rules
//                 derived from Thetis
//                 comboRadioModel_SelectedIndexChanged
//                 (setup.cs:19812-20310 [v2.10.3.13+501e3f51]).
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-05-03 — PA calibration safety hotfix Phase 9 (#167): added
//                 cross-page connect() in buildTree() that routes
//                 PaWattMeterPage::resetPaValuesRequested (Phase 5A)
//                 to PaValuesPage::resetPaValues() (Phase 5B). Mirrors
//                 Thetis btnResetPAValues_Click (setup.cs:16346-16357
//                 [v2.10.3.13+501e3f51]). Deferred from Phase 5 so
//                 Agents 5A and 5B could land in parallel without
//                 touching this file.
//   2026-07-27: issues #272 + #301. Split buildTree() into a cheap
//                 registration phase and an on-demand realization
//                 phase. buildTree() previously constructed all 55
//                 pages synchronously from the SetupDialog ctor, which
//                 blocked the Qt main thread for seconds on every
//                 Settings open. That stall starved the audio drain
//                 timer (#301, repeated RX buffer on ANAN-10E/macOS)
//                 and on slower hosts outlasted the Protocol 1 ep6
//                 watchdog, so the link was declared lost and the
//                 unclean audio teardown cascaded into a
//                 CLOCK_WATCHDOG_TIMEOUT BSOD (#272, HL2/Windows 11).
//                 Pages now build on first visit. Cross-page wiring
//                 moved into the per-page factories; the two sites
//                 that genuinely reach across pages (the PA
//                 [Reset PA Values] button and MainWindow's
//                 setTciServer()) are routed through the dialog so the
//                 dependency is realized on demand rather than eagerly.
//                 Construction-timing change only: no page's layout,
//                 controls, defaults, or behaviour are touched.
//                 AI-assisted transformation via Anthropic Claude Code.
//   2026-09-23: R-R3-21 remote control inventory. DSP > CFC and Test >
//                 Two-Tone IMD follow the negotiated transmit permission;
//                 a page the local-DSP gate disables shows a plain reason
//                 above it and as its tooltip. The permission is also
//                 pushed to every realized page, so a receive page's own
//                 transmit section (AGC/ALC TX groups, DSP Options TX
//                 combos) follows it. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-23: R-R3-23 / R-R3-36. Every page registration names a
//                 SetupScope (ThisComputer, Core, Mixed). The local-DSP
//                 gate applies to Core and Mixed pages as before; a
//                 ThisComputer page that reaches an audited accessor is
//                 disabled and logged at critical. The audio backend
//                 strip reaches the engine through
//                 RadioModel::localAudioDevices(), so Audio > Devices and
//                 TX Input work in a remote window; TX Input gates its
//                 own Core controls. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-23: R-R3-21 / R-R3-10 / R-R3-17. Setup opens in a remote
//                 window whether or not it is connected. While the Core's
//                 settings are unavailable (setStationSettingsAvailable),
//                 Core pages are disabled with "Connect to the Core to
//                 change these." and Mixed pages disable only their Core
//                 controls; a Core page opened before the first settings
//                 snapshot is a stand-in, not a page built from ship
//                 defaults. Each Core and Mixed page keeps a copy of its
//                 factory and is rebuilt from the Core's current values
//                 once they are available again. NR/ANF becomes Core: every
//                 control on it writes the Core's receiver or chooses the
//                 Core's models. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-23: R3 Setup fix wave (R-R3-21, R-R3-10, R-R3-23). Core pages
//                 wait for the Core's settings to really arrive (content
//                 or the seed marker), not just any snapshot; a page with
//                 its own dialog open is rebuilt only once that dialog is
//                 gone; a failed rebuild keeps the page it had; pages the
//                 local-DSP gate keeps disabled are not rebuilt. Filter
//                 Presets, Spectrum Peaks, Waterfall Defaults, 3D View and
//                 Export / Import are ThisComputer. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-23: R-R3-46 / R-R3-10. The PA pages follow the transmit
//                 permission with its reason; a remote window shows them
//                 for a Core radio that has them. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-23 - R-R3-46: Hardware Config is no longer declared
//                 unavailable remotely; a non-QDialog modal and a kept dialog's
//                 close hold and then run the postponed rebuild. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-23: R3 receiver audio plan, Task 4 (R-R3-42). Audio > TCI and
//                 CAT & Network > TCI Server are ThisComputer and work in a
//                 remote window. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-23: R3 receiver audio plan, Task 5 (R-R3-44). Audio > VAX is
//                 ThisComputer and Audio > Advanced reaches this computer's
//                 engine, so both work in a remote window. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-24: R-R3-49 / R-R3-21. Pages whose feature is not built yet
//                 (UnbuiltFeatures) are not registered, and a category left
//                 with no pages is not shown, so selectPage() finds none of
//                 them. J.J. Boyd (KG4VCF), with AI-assisted implementation
//                 via Anthropic Claude Code.
//   2026-09-24: R-R3-49 / R-R3-21. RX2 Display and Gradients are removed;
//                 Logging & Performance, left with only hidden logging
//                 groups, is registered only once logging is built. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-24: R-R3-47 / R-R3-48: CAT & Network > RF-Kit works in a remote
//                 window through the Core; the TCI Server page shows the
//                 Core's station TCI server. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: iPhone app Task 19 (R-IOS-06, D40): DSP > Filter Presets is
//                 a Core page; the presets live on the Core. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-24: R-R3-49 (parity Task 1): setTransmitSettingsPermitted,
//                 pushed to every realized page beside the transmit
//                 permission. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-25: R-R3-49 (parity Task 3): the transmit settings gate per
//                 transmitSettingsVersion (setTransmitSettingsPermitted's
//                 minVersion); Audio > TX Profile no longer waits for
//                 remote transmit, and gates its own controls. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-25: R-R3-49 (parity Task 4): DSP > CFC and Transmit > Speech
//                 Processor no longer wait for remote transmit; CFC gates
//                 its own controls at transmitSettingsVersion 4. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-25: R-R3-49 / R-R3-46 (parity Task 6): PA > PA Gain, Watt
//                 Meter and PA Values no longer wait for remote transmit;
//                 each gates its own controls at transmitSettingsVersion 6
//                 (the auto-calibrate sweep keeps the transmit permission).
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-25: R-R3-49 (parity Task 5): Transmit > Power, Transmit >
//                 DEXP/VOX and Test > Two-Tone IMD no longer wait for remote
//                 transmit; each gates its own controls at
//                 transmitSettingsVersion 5 (Enable VOX keeps the transmit
//                 permission). J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-25: R-R3-49 (parity Task 8): selectNavigationTarget. The Power
//                 Genius and Tuner Genius applets' Advanced entries and the
//                 interlock entry open CAT & Network > 4O3A at their own
//                 tab; the tree labels they named were folded into 4O3A.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-25: Receiver and transmit gaps plan, Task 16 fix wave (I1).
//                 While receive only is on, the Transmit and PA categories
//                 and Test > Two-Tone IMD are disabled with its reason,
//                 never hidden, following RadioModel::rxOnlyChanged, as
//                 Thetis's chkGeneralRXOnly_CheckedChanged disables
//                 tpTransmit, tpPowerAmplifier and grpTestTXIMD
//                 (setup.cs:6499-6501 [v2.10.3.15]). J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-25: Task 16 fix wave 2. The PA category is never hidden: on
//                 the receive-only kit it is disabled with the kit's
//                 reason, and on a radio without power amplifier settings
//                 with that reason. The Transmit and PA rows name the
//                 remote transmit reason beside receive only's. Receive
//                 only disables Audio > TX Input too (grpBoxMic is on
//                 tpTransmit), which a remote window without transmit
//                 keeps live. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-26: Transmit group fix wave 2 (M8): VOX shows disabled with the
//               plain reason while this computer has no microphone line to
//               the Core; the Core's refusal stays the backstop. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26 - Parity Task 21 (R-IOS-18): Setup > This Core in a remote
//                window (Change radio). J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-28 - R-R3-49 (found bug): Watt Meter > Show PA Values page
//                shows or hides the PA Values page, as Thetis's chkPAValues
//                does; nothing read the setting before. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
// =================================================================

#include "SetupDialog.h"
#include "SetupPage.h"
#include "UnbuiltFeatures.h"
#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/settings/SettingsProxy.h"
#include "core/PureSignal.h"
#include "models/RadioModel.h"

// General
#include "setup/GeneralSetupPages.h"
#include "setup/GeneralOptionsPage.h"
// Hardware
#include "setup/HardwarePage.h"
#include "setup/ThisCorePage.h"
#include "setup/CoresSetupPage.h"
#include "setup/HardwareDdcRoutingPage.h"
// PA (Setup IA reshape Phase 2 — placeholder pages, content lands in Phase 3+)
#include "setup/PaSetupPages.h"
// Phase 8 of #167: per-SKU PA visibility wiring needs RadioInfo
#include "core/RadioDiscovery.h"
// Audio
#include "setup/AudioBackendStrip.h"
#include "setup/AudioDevicesPage.h"
#include "setup/AudioTxInputPage.h"
#include "setup/AudioVaxPage.h"
#include "setup/AudioTciPage.h"
#include "setup/AudioAdvancedPage.h"
// DSP
#include "setup/DspSetupPages.h"
#include "setup/DspOptionsPage.h"   // Task 4.1
#include "setup/FilterPresetsSetupPage.h"
// Display
#include "setup/DisplaySetupPages.h"
#include "setup/SpectrumPeaksPage.h"
#include "setup/MultimeterPage.h"     // Task 3.1
// Transmit
#include "setup/TransmitSetupPages.h"
// Appearance
#include "setup/AppearanceSetupPages.h"
// CAT & Network
#include "setup/CatNetworkSetupPages.h"
// Phase 3P-II Phase 4 Task 78: PGXL Advanced page
#include "setup/PgxlAdvancedPage.h"
// Phase 3P-II Phase 4 Task 85: TGXL Advanced page
#include "setup/TgxlAdvancedPage.h"
// 4O3A integration page (Settings -> CAT & Network -> 4O3A).  Hosts the
// QTabWidget that folds the former Peripherals / PGXL Advanced / TGXL
// Advanced / PGXL Interlock entries into a single tree node under a
// master toggle.  PgxlInterlockPage's include lives inside FourO3APage.cpp.
#include "setup/FourO3APage.h"
#include <QHash>
// RF-Kit RF2K-S integration page (Settings -> CAT & Network -> RF-Kit).
#include "setup/RfKitPage.h"
// Keyboard
#include "setup/KeyboardSetupPages.h"
// Diagnostics
#include "setup/DiagnosticsSetupPages.h"
#include "diagnostics/RadioStatusPage.h"
#include "diagnostics/DiagnosticsPhaseHPages.h"
// Test (Phase 3M-1c H.1: Two-Tone IMD page)
#include "setup/TestTwoTonePage.h"
// TX Profile editor (Phase 3M-1c J.3 — under Audio)
#include "setup/TxProfileSetupPage.h"

#include <QApplication>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QShowEvent>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QScopeGuard>

#include <utility>

namespace NereusSDR {

namespace {

// Current board capabilities for the connected radio, with the same
// conservative fallback the ctor has always used: boardCapabilities()
// returns Unknown caps when no radio has ever connected, and Unknown sets
// hasPaProfile=false so the PA pages are disabled with their reason.
//
// Factored out of the ctor so the lazily-built PA pages can pick up the
// live caps at realization time without SetupDialog having to cache a
// BoardCapabilities member (which would pull the struct definition into
// SetupDialog.h).
BoardCapabilities capsForModel(RadioModel* model)
{
    return model ? model->boardCapabilities()
                 : BoardCapsTable::forBoard(HPSDRHW::Unknown);
}

} // namespace

// Timing instrumentation added in response to #272, where a ~3-second build
// + ~40-second click-time freeze stalled the audio engine long enough for
// the HL2 ep6 watchdog (2011 ms) to fire and the unclean audio teardown to
// cascade into a CLOCK_WATCHDOG_TIMEOUT BSOD on a legacy Realtek driver.
// Per-section + per-page-switch deltas let the next repro identify which
// page or interaction is blocking the UI thread, without needing a profiler.
//
// Disabled by default. Enable with:
//   QT_LOGGING_RULES="nereus.setup.timing.debug=true"
Q_LOGGING_CATEGORY(lcSetupTiming, "nereus.setup.timing")

// ── Construction ──────────────────────────────────────────────────────────────

SetupDialog::SetupDialog(RadioModel* model, QWidget* parent)
    : QDialog(parent), m_model(model)
{
    m_dspReceiverSelection = new DspReceiverSelection(this);
    m_transmitPermitted = model && model->ownsLocalDsp();
    m_transmitSettingsPermitted = m_transmitPermitted;
    m_transmitReason = tr("Remote transmit controls are not available from this Core.");
    m_localUnavailableReason = tr(
        "These settings control audio and signal processing on this computer. "
        "While connected to a Core, the Core does that work, so they cannot be "
        "changed here.");
    m_stationReason = tr("Connect to the Core to change these.");
    // R-R3-21: the same predicate MainWindow pushes from; a local window
    // has no settings proxy, so this is true there and never changes.
    m_stationAvailable = !(model && !model->ownsLocalDsp())
        || setupDialogAllowedForCurrentBackend();
    if (model && !model->ownsLocalDsp()) {
        // dynamic_cast: the backend interface is not a QObject (see
        // setupDialogAllowedForCurrentBackend()).
        m_settingsProxy = dynamic_cast<SettingsProxy*>(AppSettings::instance().remoteBackend());
        if (m_settingsProxy) {
            connect(m_settingsProxy, &SettingsProxy::snapshotApplied,
                    this, [this](int) { onStationSnapshotApplied(); });
        }
    }
    setWindowTitle("NereusSDR Settings");
    setMinimumSize(820, 600);
    resize(900, 650);
    setStyleSheet("QDialog { background: #0f0f1a; }");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Splitter: tree navigation | stacked pages ─────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet("QSplitter::handle { background: #304050; }");

    // Tree navigation
    m_tree = new QTreeWidget;
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(16);
    m_tree->setFixedWidth(200);
    m_tree->setStyleSheet(
        "QTreeWidget { background: #131326; color: #c8d8e8; border: none; "
        "font-size: 12px; selection-background-color: #00b4d8; selection-color:#0f0f1a; }"
        "QTreeWidget::item { padding: 2px 8px; min-height:17px; }"
        "QTreeWidget::item:hover { background: #1a2a3a; }"
        "QTreeWidget::item:selected { background:#00b4d8; color:#0f0f1a; }");

    // Stacked widget for page content
    m_stack = new QStackedWidget;
    m_stack->setStyleSheet("QStackedWidget { background: #0f0f1a; }");

    auto* pageContainer = new QWidget;
    auto* pageLayout = new QVBoxLayout(pageContainer);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    m_transmitNotice = new QLabel(pageContainer);
    m_transmitNotice->setObjectName(QStringLiteral("setupTransmitUnavailable"));
    m_transmitNotice->setWordWrap(true);
    m_transmitNotice->setMargin(12);
    m_transmitNotice->setStyleSheet(QStringLiteral("QLabel { color: #c8d8e8; background: #1a2a3a; }"));
    m_transmitNotice->hide();
    pageLayout->addWidget(m_transmitNotice);
    m_localUnavailableNotice = new QLabel(m_localUnavailableReason, pageContainer);
    m_localUnavailableNotice->setObjectName(QStringLiteral("setupLocalUnavailable"));
    m_localUnavailableNotice->setWordWrap(true);
    m_localUnavailableNotice->setMargin(12);
    m_localUnavailableNotice->setStyleSheet(
        QStringLiteral("QLabel { color: #c8d8e8; background: #1a2a3a; }"));
    m_localUnavailableNotice->hide();
    pageLayout->addWidget(m_localUnavailableNotice);
    m_stationNotice = new QLabel(m_stationReason, pageContainer);
    m_stationNotice->setObjectName(QStringLiteral("setupStationUnavailable"));
    m_stationNotice->setWordWrap(true);
    m_stationNotice->setMargin(12);
    m_stationNotice->setStyleSheet(
        QStringLiteral("QLabel { color: #c8d8e8; background: #1a2a3a; }"));
    m_stationNotice->hide();
    pageLayout->addWidget(m_stationNotice);
    // Task 16 fix wave (I1): the receive-only reason above a page it
    // disables.
    m_receiveOnlyNotice = new QLabel(pageContainer);
    m_receiveOnlyNotice->setObjectName(QStringLiteral("setupReceiveOnly"));
    m_receiveOnlyNotice->setWordWrap(true);
    m_receiveOnlyNotice->setMargin(12);
    m_receiveOnlyNotice->setStyleSheet(
        QStringLiteral("QLabel { color: #c8d8e8; background: #1a2a3a; }"));
    m_receiveOnlyNotice->hide();
    pageLayout->addWidget(m_receiveOnlyNotice);
    // Task 16 fix wave 2 (Important 2): why the PA pages are disabled on a
    // radio without power amplifier settings.
    m_noPaNotice = new QLabel(pageContainer);
    m_noPaNotice->setObjectName(QStringLiteral("setupNoPowerAmplifier"));
    m_noPaNotice->setWordWrap(true);
    m_noPaNotice->setMargin(12);
    m_noPaNotice->setStyleSheet(
        QStringLiteral("QLabel { color: #c8d8e8; background: #1a2a3a; }"));
    m_noPaNotice->hide();
    pageLayout->addWidget(m_noPaNotice);
    pageLayout->addWidget(m_stack, 1);
    splitter->addWidget(m_tree);
    splitter->addWidget(pageContainer);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({200, 699});
    splitter->setCollapsible(0, false);
    splitter->setCollapsible(1, false);

    layout->addWidget(splitter, 1);

    // ── Build the tree, register the page factories ───────────────────────────
    // #272 instrumentation: track total buildTree() cost. Per-category
    // deltas are emitted from inside buildTree() via a local tick helper.
    //
    // #272 / #301: buildTree() no longer constructs any page. It registers
    // one factory per leaf; realizePage() builds the widget on first visit.
    QElapsedTimer buildTimer;
    buildTimer.start();
    buildTree();
    qCDebug(lcSetupTiming)
        << "buildTree() total elapsed (ms):" << buildTimer.elapsed()
        << "pages registered:" << m_pages.size()
        << "realized:" << m_stack->count();

    // Connect tree selection → stack page.
    // #272 instrumentation: time the page-switch path. The QStackedWidget
    // setCurrentIndex() will fire showEvent() on the destination page, which
    // is where any deferred / click-time work would surface.
    //
    // #272 / #301: the UserRole payload is now a m_pages registry index, not
    // a stack index. showPageAt() resolves it, building the page on the first
    // visit and reusing the cached widget afterwards.
    connect(m_tree, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem* current, QTreeWidgetItem* /*previous*/) {
                if (current == nullptr) { return; }
                const int entryIndex = current->data(0, Qt::UserRole).toInt();
                if (entryIndex >= 0) {
                    const bool wasRealized =
                        entryIndex < static_cast<int>(m_pages.size())
                        && m_pages[static_cast<std::size_t>(entryIndex)].widget != nullptr;
                    QElapsedTimer switchTimer;
                    switchTimer.start();
                    showPageAt(entryIndex);
                    qCDebug(lcSetupTiming)
                        << "page switch ->" << current->text(0)
                        << (wasRealized ? "(cached)" : "(first visit, realized)")
                        << "elapsed (ms):" << switchTimer.elapsed();
                }
            });

    // ── Phase 8 of #167: per-SKU PA visibility wiring ────────────────────────
    // Subscribe to capability-changed signal so PA category + child pages
    // re-evaluate visibility on radio swap. Mirrors HardwarePage's
    // currentRadioChanged subscription pattern.
    if (m_model) {
        connect(m_model, &RadioModel::currentRadioChanged,
                this, &SetupDialog::onCurrentRadioChanged);
        // Task 16 fix wave (I1): receive only turning on or off, or its
        // reason changing (the kit), in a local and a remote window alike.
        connect(m_model, &RadioModel::rxOnlyChanged,
                this, [this](bool) { refreshTransmitPresentation(); });
    }

    // Apply initial visibility at construction time so the dialog opens
    // with the right state even when no currentRadioChanged has fired yet.
    // boardCapabilities() falls back to Unknown caps when no radio has
    // ever connected; Unknown sets hasPaProfile=false → PA pages
    // disabled with their reason, matching the conservative default.
    //
    // #272 / #301: this pass now only decides nav-tree row visibility, since
    // the three PA page widgets do not exist yet. Each PA factory re-applies
    // the live caps to its own page at realization time.
    applyPaVisibility(capsForModel(m_model));

    // R-R3-21: leaf tooltips for remote-unavailable pages from the start.
    refreshTransmitPresentation();
}

// ── showEvent ─────────────────────────────────────────────────────────────────

void SetupDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);

    // Only default to the first leaf if no page was pre-selected via selectPage()
    if (m_tree->currentItem() == nullptr) {
        QTreeWidgetItem* first = m_tree->topLevelItem(0);
        if (first != nullptr && first->childCount() > 0) {
            first = first->child(0);
        }
        if (first != nullptr) {
            m_tree->setCurrentItem(first);
        }
    }
}

void SetupDialog::selectPage(const QString& label)
{
    // Search all tree items (top-level categories + children) for matching text
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* cat = m_tree->topLevelItem(i);
        for (int j = 0; j < cat->childCount(); ++j) {
            QTreeWidgetItem* child = cat->child(j);
            if (child->text(0) == label) {
                m_tree->setCurrentItem(child);
                return;
            }
        }
    }
}

void SetupDialog::setReceiverSelector(std::function<SliceModel*()> selector,
                                      std::function<bool()> requiresOwnedReceiver)
{
    m_dspReceiverSelection->setSelector(std::move(selector),
                                       std::move(requiresOwnedReceiver));
}

void SetupDialog::notifyReceiverSelectionChanged()
{
    m_dspReceiverSelection->notifyChanged(m_model);
}

bool SetupDialog::selectNavigationTarget(const QString& pageKey)
{
    struct Target {
        const char* label;
        int fourO3ATab;   // -1: not a 4O3A tab
    };
    static const QHash<QString, Target> kTargets = {
        {QStringLiteral("pgxlAdvanced"),
         {"4O3A", static_cast<int>(FourO3APage::Tab::PowerGenius)}},
        {QStringLiteral("tgxlAdvanced"),
         {"4O3A", static_cast<int>(FourO3APage::Tab::TunerGenius)}},
        {QStringLiteral("pgxlInterlock"),
         {"4O3A", static_cast<int>(FourO3APage::Tab::General)}},
        {QStringLiteral("peripherals"),
         {"4O3A", static_cast<int>(FourO3APage::Tab::General)}},
        // Phase 3P-III Task 14: RF-Kit setup page (CAT & Network > RF-Kit).
        {QStringLiteral("rfKit"), {"RF-Kit", -1}},
    };
    const auto it = kTargets.constFind(pageKey);
    if (it == kTargets.cend()) {
        return false;
    }
    selectPage(QString::fromLatin1(it->label));
    if (it->fourO3ATab >= 0) {
        if (auto* page = findChild<FourO3APage*>()) {
            page->selectTab(static_cast<FourO3APage::Tab>(it->fourO3ATab));
        }
    }
    return true;
}

// Phase 3J-1 bench fix (2026-05-11): forward TciServer reference to the
// CatTciServerPage so the Server group box title + Status label update
// live as clients connect/disconnect and the server starts/stops.
//
// Idempotent and nullptr-safe — see CatTciServerPage::setTciServer() for
// the QPointer + disconnect-old-then-connect-new pattern.
//
// #272 / #301: MainWindow calls this from wireSetupDialog(), i.e. immediately
// after construction and long before the operator navigates to CAT & Network
// -> TCI Server. Record the pointer so the page factory can replay it when
// the page is finally realized; forward straight through when the page is
// already up (a later server start/stop while Setup is open).
void SetupDialog::setTciServer(NereusSDR::TciServer* server)
{
    m_pendingTciServer = server;
    if (m_tciServerPage) {
        m_tciServerPage->setTciServer(server);
    }
}

void SetupDialog::setReceiverAudioNote(RemoteReceiverAudioNote note)
{
    m_receiverAudioNote = note;
    if (m_vaxPage) {
        m_vaxPage->setReceiverAudioNote(note);
    }
}

// ── Lazy page registry (issues #272 + #301) ───────────────────────────────────

QTreeWidgetItem* SetupDialog::registerPage(QTreeWidgetItem* parent,
                                          const QString& label,
                                          SetupScope scope,
                                          std::function<QWidget*()> factory,
                                          bool requiresTransmit)
{
    auto* item = new QTreeWidgetItem(parent, QStringList{label});
    item->setData(0, Qt::UserRole, static_cast<int>(m_pages.size()));
    PageEntry entry;
    entry.label = label;
    entry.scope = scope;
    entry.requiresTransmit = requiresTransmit;
    if (scope != SetupScope::ThisComputer) {
        // R-R3-21: kept so the page can be rebuilt from a newer snapshot.
        entry.rebuildFactory = factory;
    }
    entry.factory = std::move(factory);
    m_pages.push_back(std::move(entry));
    if (requiresTransmit && !m_transmitPermitted) {
        item->setToolTip(0, m_transmitReason);
    }
    return item;
}

void SetupDialog::setCoreTargets(CoreTargetStore* store)
{
    // A canonical store is immutable for a realized page's lifetime. Replacing
    // it retires the old page/controller before any old result can repaint.
    if (store == m_coreTargets) { return; }
    m_coreTargets = store;
    for (PageEntry& entry : m_pages) {
        if (entry.label != QStringLiteral("Your Cores") || !entry.widget) { continue; }
        QWidget* previous = entry.widget;
        if (auto* page = qobject_cast<CoresSetupPage*>(previous)) { page->cancelOperations(); }
        const bool visible = m_stack->currentWidget() == previous;
        m_stack->removeWidget(previous);
        for (PageEntry& registered : m_pages) {
            if (registered.widget && registered.widget != previous) {
                registered.stackIndex = m_stack->indexOf(registered.widget);
            }
        }
        previous->hide();
        previous->deleteLater();
        entry.widget = nullptr;
        entry.stackIndex = -1;
        entry.factory = [this]() -> QWidget* {
            auto* page = new CoresSetupPage(m_coreTargets, m_model);
            page->setContext(m_coreSettingsContext);
            if (!m_inspectedCoreTarget.isEmpty()) { page->inspectTarget(m_inspectedCoreTarget); }
            connect(page, &CoresSetupPage::connectionDetailsRequested,
                    this, &SetupDialog::connectionsRequested);
            if (m_coresPageBinder) { m_coresPageBinder(page); }
            return page;
        };
        if (visible) {
            const int index = static_cast<int>(&entry - m_pages.data());
            if (QWidget* replacement = realizePage(index)) { m_stack->setCurrentWidget(replacement); }
        }
    }
}

void SetupDialog::setCoreSettingsContext(const CoreSettingsContext& context)
{
    m_coreSettingsContext = context;
    for (const PageEntry& entry : m_pages) {
        if (auto* page = qobject_cast<CoresSetupPage*>(entry.widget)) { page->setContext(context); }
    }
}

void SetupDialog::setCoresPageBinder(std::function<void(CoresSetupPage*)> binder)
{
    m_coresPageBinder = std::move(binder);
    if (!m_coresPageBinder) { return; }
    for (const PageEntry& entry : m_pages) {
        if (auto* page = qobject_cast<CoresSetupPage*>(entry.widget)) { m_coresPageBinder(page); }
    }
}

void SetupDialog::inspectCoreTarget(const QString& id)
{
    m_inspectedCoreTarget = id;
    selectPage(QStringLiteral("Your Cores"));
    for (const PageEntry& entry : m_pages) {
        if (auto* page = qobject_cast<CoresSetupPage*>(entry.widget)) { page->inspectTarget(id); page->refreshTargets(); }
    }
}

void SetupDialog::setRemoteStationPageBinder(
    std::function<void(RemoteStationPage*)> binder)
{
    m_remoteStationPageBinder = std::move(binder);
    if (!m_remoteStationPageBinder) { return; }
    for (const PageEntry& entry : m_pages) {
        if (auto* page = qobject_cast<RemoteStationPage*>(entry.widget)) {
            m_remoteStationPageBinder(page);
        }
    }
}

QWidget* SetupDialog::realizePage(int entryIndex)
{
    if (entryIndex < 0 || entryIndex >= static_cast<int>(m_pages.size())) {
        return nullptr;
    }

    PageEntry& entry = m_pages[static_cast<std::size_t>(entryIndex)];
    if (entry.widget != nullptr) {
        return entry.widget;
    }
    if (!entry.factory) {
        // Already attempted and yielded nothing; do not retry or re-warn.
        return nullptr;
    }

    // R-R3-21: a Core page opened in a remote window that has never received
    // the Core's settings. Building it now would show this computer's ship
    // defaults as if they were the Core's (and a constructor that seeds a
    // default would record it as an edit, or send it once the link is up),
    // so it gets an empty stand-in with the reason above it. The factory is
    // kept; rebuildStalePages() replaces the stand-in once the Core's
    // settings are available.
    //
    // R3 Setup fix wave (final review I2): "received" means the Core's
    // settings really arrived (stationSettingsArrived()), not merely that a
    // snapshot did: an empty, unseeded one leaves the session ready with
    // nothing for these pages to show.
    if (remoteSession() && entry.scope == SetupScope::Core && !stationSettingsArrived()) {
        auto* placeholder = new QWidget;
        placeholder->setObjectName(QStringLiteral("setupStationPlaceholder"));
        placeholder->setEnabled(false);
        entry.placeholder = true;
        entry.widget = placeholder;
        entry.stackIndex = m_stack->addWidget(placeholder);
        refreshTransmitPresentation();
        return placeholder;
    }

    // Move the factory out before invoking it so a page whose construction
    // re-enters realizePage() (e.g. a cross-page connect firing during the
    // ctor) cannot recurse into building the same page twice.
    const std::function<QWidget*()> factory = std::move(entry.factory);
    entry.factory = nullptr;

    // ── Remote-daemon R2 Task 20: the local-DSP page gate ────────────────
    //
    // ONE gate for all 55-odd leaves, and it classifies itself. A page that
    // binds itself to this process's WdspEngine, AudioEngine or
    // ReceiverManager while the DSP actually lives on a station is a page
    // whose controls move nothing: on a Role::Remote model those three
    // accessors hand back real but INERT objects (RadioModel.h,
    // localDspHandOutCount()'s comment), so the page looks live, accepts
    // input, and silently does nothing. Disabling it is the honest state.
    //
    // Deliberately not a hand-written list of page labels. A list has to be
    // edited every time a leaf is added, and the failure mode of forgetting
    // is a page that looks like it works -- the exact thing this gate
    // exists to prevent. Reading the model's own hand-out audit across the
    // factory call means a new page classifies itself on the day it is
    // written, with nothing to remember.
    //
    // Local direct mode never reaches the branch: ownsLocalDsp() is true,
    // and RadioModel's audit is not even armed for Role::Local.
    //
    // R-R3-23: the gate reads the page's declared scope. Core and Mixed
    // pages keep it exactly as above. A ThisComputer page is expected to
    // reach nothing the audit counts (this computer's sound devices come
    // through RadioModel::localAudioDevices(), which it does not count), so
    // if one does, that is a bug in the page, not a remote-mode state: it
    // is disabled all the same and logged at critical with the accessor,
    // and tst_remote_gui_gating's sweep fails on it.
    const bool gateRemotePages = (m_model != nullptr) && !m_model->ownsLocalDsp();
    const int handOutsBefore = (m_model != nullptr) ? m_model->localDspHandOutCount() : 0;
    const QSet<QByteArray> handOutNamesBefore =
        (m_model != nullptr) ? m_model->localDspHandOutNames() : QSet<QByteArray>{};

    QElapsedTimer realizeTimer;
    realizeTimer.start();
    QWidget* page = factory();
    if (page == nullptr) {
        qCWarning(lcSetupTiming) << "page factory yielded nothing for"
                                 << entry.label;
        return nullptr;
    }

    // A before/after difference rather than a reset-then-read, so a page
    // whose factory re-enters realizePage() (see the comment above the
    // factory move) cannot zero the outer page's tally on its way through.
    // The names below are therefore the running set for the whole dialog,
    // not this page's alone; the verdict is per-page, the names are a hint.
    if (gateRemotePages && !entry.remoteUnavailableReason.isEmpty()) {
        // Declared unavailable at registration (markRemoteUnavailable): the
        // page works on this computer's own radio connection or accessories.
        entry.localDspUnavailable = true;
        page->setEnabled(false);
    } else if (gateRemotePages && m_model->localDspHandOutCount() > handOutsBefore) {
        entry.localDspUnavailable = true;
        page->setEnabled(false);
        // R-R3-21: disabled with a visible reason, not just greyed out.
        // refreshTransmitPresentation() below owns the tooltip and the
        // notice, so a page that is also a transmit page gets the one
        // reason that currently applies.
        if (entry.scope == SetupScope::ThisComputer) {
            // The accessors this factory reached for the first time, or the
            // running set when every one of them was already reached by an
            // earlier page (the audit is dialog-wide; see above).
            QSet<QByteArray> reached = m_model->localDspHandOutNames();
            const QSet<QByteArray> fresh = reached - handOutNamesBefore;
            if (!fresh.isEmpty()) {
                reached = fresh;
            }
            qCCritical(lcSetupTiming).noquote()
                << "Setup page" << entry.label
                << "is declared ThisComputer but reached"
                << QString::fromLatin1(QList<QByteArray>(reached.cbegin(), reached.cend())
                                           .join(", "))
                << "on a remote-station model; it has been disabled. "
                   "Reach this computer's sound devices through "
                   "RadioModel::localAudioDevices(), or register the page as "
                   "Core or Mixed.";
        } else {
            qCWarning(lcSetupTiming)
                << "Setup page" << entry.label
                << "reached local DSP on a remote-station model and has been "
                   "disabled; accessors reached so far in this dialog:"
                << m_model->localDspHandOutNames().values();
        }
    }

    entry.widget     = page;
    entry.stackIndex = m_stack->addWidget(page);
    entry.placeholder = false;
    entry.stationDisabled = false;
    entry.builtGeneration = m_snapshotGeneration;
    refreshTransmitPresentation();
    qCDebug(lcSetupTiming) << "realized page" << entry.label
                           << "elapsed (ms):" << realizeTimer.elapsed();
    return page;
}

void SetupDialog::showPageAt(int entryIndex)
{
    if (m_rebuildPostponed) {
        // A rebuild waited for a page's own dialog (rebuildStalePages());
        // the page is shown again, so run it now.
        rebuildStalePages();
    }
    if (realizePage(entryIndex) == nullptr) {
        return;
    }
    // By widget, not by stored index: a rebuilt page is appended to the
    // stack, so stored indices are not stable across rebuilds.
    m_stack->setCurrentWidget(m_pages[static_cast<std::size_t>(entryIndex)].widget);
    refreshTransmitPresentation();
}

bool SetupDialog::remoteSession() const
{
    return m_model != nullptr && !m_model->ownsLocalDsp();
}

bool SetupDialog::stationSettingsArrived() const
{
    // The proxy's own Setup-gate condition without its ready() half: a
    // snapshot with real content, or the Core's seed marker saying its
    // profile is empty because it is fresh (SettingsProxy.h, "The
    // Setup-dialog gate"). A disconnected window keeps what arrived.
    return m_settingsProxy.isNull() || m_settingsProxy->hasNonEmptySnapshot()
        || m_settingsProxy->contains(QLatin1String(AppSettings::kDaemonProfileSeededKey));
}

void SetupDialog::setStationSettingsAvailable(bool available, const QString& reason)
{
    m_stationReason = reason.isEmpty() ? tr("Connect to the Core to change these.") : reason;
    const bool becameAvailable = available && !m_stationAvailable;
    m_stationAvailable = available;
    if (becameAvailable) {
        // Every return of the Core's settings follows a new snapshot, so
        // the pages built while they were away are rebuilt now.
        rebuildStalePages();
    }
    refreshTransmitPresentation();
}

void SetupDialog::onStationSnapshotApplied()
{
    ++m_snapshotGeneration;
    // Queued: the proxy emits this from inside applySnapshot(), before the
    // session marks the settings ready, and a page must not be rebuilt
    // (constructed) from inside that call. A snapshot that arrives while
    // the settings are unavailable is picked up by the rebuild that
    // setStationSettingsAvailable(true) runs.
    QMetaObject::invokeMethod(this, [this] {
        rebuildStalePages();
        refreshTransmitPresentation();
    }, Qt::QueuedConnection);
}

void SetupDialog::forgetPagePointersInside(const QWidget* page)
{
    const auto inside = [page](const QWidget* candidate) {
        return candidate != nullptr && (candidate == page || page->isAncestorOf(candidate));
    };
    if (inside(m_tciServerPage))  { m_tciServerPage = nullptr; }
    if (inside(m_paGainPage))     { m_paGainPage = nullptr; }
    if (inside(m_paWattMeterPage)) { m_paWattMeterPage = nullptr; }
    if (inside(m_paValuesPage))   { m_paValuesPage = nullptr; }
}

QWidget* SetupDialog::openDialogOwnedBy(const QWidget* page) const
{
    // The modal on top, if the page is anywhere in its parent chain. Walked
    // by QObject parent, not QWidget::isAncestorOf(): that stops at a window
    // boundary, and a dialog is a window of its own.
    //
    // R-R3-46 (carried from the remote window Setup re-review): returned as
    // a QWidget, so a modal window that is not a QDialog still holds the
    // rebuild back; qobject_cast<QDialog*> used to turn it into "none".
    if (QWidget* const modal = QApplication::activeModalWidget()) {
        for (const QObject* object = modal; object != nullptr; object = object->parent()) {
            if (object == page) {
                return modal;
            }
        }
    }
    // A dialog of the page's that is open but not the active modal (a
    // native file dialog, or one under another window's modal).
    for (QDialog* dialog : page->findChildren<QDialog*>()) {
        if (dialog->isVisible()) {
            return dialog;
        }
    }
    return nullptr;
}

void SetupDialog::rebuildStalePages()
{
    if (m_rebuildingPages || !m_stationAvailable || !remoteSession()) {
        return;
    }
    m_rebuildingPages = true;
    m_rebuildPostponed = false;
    const auto done = qScopeGuard([this] { m_rebuildingPages = false; });

    for (std::size_t i = 0; i < m_pages.size(); ++i) {
        PageEntry& entry = m_pages[i];
        if (entry.widget == nullptr || entry.scope == SetupScope::ThisComputer) {
            continue;
        }
        if (!entry.placeholder && entry.builtGeneration == m_snapshotGeneration) {
            continue;  // already shows the newest snapshot
        }
        if (!entry.placeholder && entry.localDspUnavailable) {
            // R3 Setup fix wave (final review M5): the local-DSP gate keeps
            // this page disabled for the whole remote session, so a newer
            // snapshot changes nothing on it; rebuilding would only log the
            // gate's warning again.
            entry.builtGeneration = m_snapshotGeneration;
            continue;
        }
        if (!entry.placeholder && !entry.rebuildFactory) {
            continue;
        }
        QWidget* const old = entry.widget;
        // R3 Setup fix wave (final review I1): never under the page's own
        // open dialog. The page's code is on the stack below that dialog's
        // event loop, and deleting the page there would pull the page out
        // from under it. Postponed until the dialog is gone, or the page is
        // next shown.
        if (QWidget* const owner = openDialogOwnedBy(old)) {
            m_rebuildPostponed = true;
            if (!m_rebuildWaitsFor.contains(owner)) {
                m_rebuildWaitsFor.insert(owner);
                const auto rebuildLater = [this, owner] {
                    m_rebuildWaitsFor.remove(owner);
                    // Queued: the dialog goes on its way out of the page's
                    // own code, which finishes first.
                    QMetaObject::invokeMethod(this, [this] {
                        rebuildStalePages();
                        refreshTransmitPresentation();
                    }, Qt::QueuedConnection);
                };
                connect(owner, &QObject::destroyed, this, rebuildLater);
                // R-R3-46 (carried): a dialog the page keeps after it
                // closes is never destroyed, so its close runs the
                // postponed rebuild too (queued, as above: finished() is
                // emitted from inside the dialog's own done()).
                if (auto* const dialog = qobject_cast<QDialog*>(owner)) {
                    connect(dialog, &QDialog::finished, this, rebuildLater,
                            Qt::QueuedConnection);
                }
            }
            qCDebug(lcSetupTiming) << "rebuild of Setup page" << entry.label
                                   << "waits for its open dialog";
            continue;
        }
        if (!entry.placeholder) {
            entry.factory = entry.rebuildFactory;
        }

        // What the rebuild replaces, kept so a factory that yields nothing
        // leaves the page as it was (final review M3).
        const bool wasPlaceholder = entry.placeholder;
        const bool wasStationDisabled = entry.stationDisabled;
        const bool wasLocalDspUnavailable = entry.localDspUnavailable;
        CatTciServerPage* const tciServerPage = m_tciServerPage;
        PaGainByBandPage* const paGainPage = m_paGainPage;
        PaWattMeterPage* const paWattMeterPage = m_paWattMeterPage;
        PaValuesPage* const paValuesPage = m_paValuesPage;

        const bool wasCurrent = m_stack->currentWidget() == old;
        forgetPagePointersInside(old);
        entry.widget = nullptr;
        entry.stackIndex = -1;
        entry.placeholder = false;
        entry.stationDisabled = false;
        entry.localDspUnavailable = false;
        QWidget* const fresh = realizePage(static_cast<int>(i));
        if (fresh == nullptr) {
            qCWarning(lcSetupTiming) << "rebuilding Setup page" << entry.label
                                     << "yielded nothing; keeping the page it had";
            entry.widget = old;
            entry.stackIndex = m_stack->indexOf(old);
            entry.placeholder = wasPlaceholder;
            entry.stationDisabled = wasStationDisabled;
            entry.localDspUnavailable = wasLocalDspUnavailable;
            entry.builtGeneration = m_snapshotGeneration;
            m_tciServerPage = tciServerPage;
            m_paGainPage = paGainPage;
            m_paWattMeterPage = paWattMeterPage;
            m_paValuesPage = paValuesPage;
            // realizePage() moved the factory out; the next snapshot tries
            // again.
            if (!entry.factory) {
                entry.factory = entry.rebuildFactory;
            }
            continue;
        }
        if (wasCurrent) {
            m_stack->setCurrentWidget(fresh);
        }
        m_stack->removeWidget(old);
        old->hide();
        old->setParent(nullptr);
        old->deleteLater();
    }
}

void SetupDialog::setTransmitPermitted(bool permitted, const QString& reason)
{
    m_transmitPermitted = permitted;
    m_transmitReason = reason.isEmpty()
        ? tr("Remote transmit controls are not available from this Core.") : reason;
    refreshTransmitPresentation();
}

void SetupDialog::setVoxPermitted(bool permitted, const QString& reason)
{
    if (m_voxPermitted == permitted && m_voxReason == reason) {
        return;
    }
    m_voxPermitted = permitted;
    m_voxReason = reason;
    refreshTransmitPresentation();
}

void SetupDialog::setTransmitSettingsPermitted(bool permitted, const QString& reason,
                                               int minVersion)
{
    if (minVersion <= 1) {
        m_transmitSettingsPermitted = permitted;
        m_transmitSettingsReason = reason;
    } else {
        // R-R3-49 (parity Task 3): a later version's settings, pushed to the
        // pages through SetupPage::setTransmitSettingsPermittedAt.
        m_transmitSettingsGates.insert(minVersion, TransmitSettingsGate{permitted, reason});
    }
    refreshTransmitPresentation();
}

void SetupDialog::refreshTransmitPresentation()
{
    // The reason a page is unavailable, if it is. The transmit reason wins
    // on a page that carries both gates: it is the one a Core that later
    // permits transmit would lift, and the local-DSP reason takes over then.
    //
    // R-R3-21: on a Core page in a remote window without the Core's
    // settings, "Connect to the Core to change these." comes first, for the
    // same reason: connecting is the first thing that has to happen.
    const bool remoteSession = this->remoteSession();
    const bool stationBlocked = remoteSession && !m_stationAvailable;
    const auto coreBlocked = [stationBlocked](const PageEntry& entry) {
        return stationBlocked && entry.scope == SetupScope::Core;
    };
    // Task 16 fix wave (I1). From Thetis setup.cs:6499-6501 [v2.10.3.15]
    // (chkGeneralRXOnly_CheckedChanged):
    //   tpTransmit.Enabled = !chkGeneralRXOnly.Checked;
    //   tpPowerAmplifier.Enabled = !chkGeneralRXOnly.Checked;
    //   grpTestTXIMD.Enabled = !chkGeneralRXOnly.Checked;
    // Disabled with the reason (the kit's for the kit), never hidden. With a
    // remote window's missing transmit as well, both reasons (M6).
    const bool rxOnly = m_model && m_model->isRxOnly();
    const auto rxOnlyBlocked = [rxOnly](const PageEntry& entry) {
        return rxOnly && entry.receiveOnlyGated;
    };
    // Task 16 fix wave 2 (Important 2): a radio without power amplifier
    // settings shows the PA pages disabled with that reason, never hidden.
    // Receive only's reason comes first (the kit's for the kit).
    const auto paBlocked = [this](const PageEntry& entry) {
        return entry.paPage && !m_paAvailable;
    };
    const auto unavailableReason = [this, &coreBlocked, &rxOnlyBlocked, &paBlocked](
                                       const PageEntry& entry) -> QString {
        if (coreBlocked(entry)) {
            return m_stationReason;
        }
        if (rxOnlyBlocked(entry)) {
            return m_model->rxOnlyReasonAlongside(
                entry.requiresTransmit && !m_transmitPermitted ? m_transmitReason : QString());
        }
        if (paBlocked(entry)) {
            return m_noPaReason;
        }
        if (entry.requiresTransmit && !m_transmitPermitted) {
            return m_transmitReason;
        }
        if (entry.localDspUnavailable) {
            return entry.remoteUnavailableReason.isEmpty()
                ? m_localUnavailableReason : entry.remoteUnavailableReason;
        }
        return QString();
    };

    bool showNotice = false;
    bool showLocalNotice = false;
    bool showStationNotice = false;
    bool showReceiveOnlyNotice = false;
    bool showNoPaNotice = false;
    QString receiveOnlyNoticeText;
    QString localNoticeText = m_localUnavailableReason;
    for (PageEntry& entry : m_pages) {
        if (!entry.widget) { continue; }
        // R-R3-21: a receive page's own transmit section (AGC/ALC TX groups,
        // DSP Options TX combos) follows the same permission. The page may
        // sit inside a wrapper (the audio backend strip), so reach it
        // wherever it is.
        QList<SetupPage*> setupPages = entry.widget->findChildren<SetupPage*>();
        if (auto* self = qobject_cast<SetupPage*>(entry.widget)) {
            setupPages.prepend(self);
        }
        for (SetupPage* setupPage : setupPages) {
            setupPage->setTransmitPermitted(m_transmitPermitted, m_transmitReason);
            // Fix wave 2 (M8): VOX needs this computer's microphone line.
            setupPage->setVoxPermitted(m_voxPermitted, m_voxReason);
            // R-R3-49 (parity Task 1): the transmit settings that key nothing.
            setupPage->setTransmitSettingsPermitted(m_transmitSettingsPermitted,
                                                    m_transmitSettingsReason);
            // R-R3-49 (parity Task 3): each later version's settings.
            for (auto gate = m_transmitSettingsGates.cbegin();
                 gate != m_transmitSettingsGates.cend(); ++gate) {
                setupPage->setTransmitSettingsPermittedAt(gate.key(), gate->permitted,
                                                          gate->reason);
            }
            // R-R3-21: a Mixed page gates its own Core controls.
            setupPage->setStationSettingsAvailable(!stationBlocked, m_stationReason);
        }
        const bool blocked = coreBlocked(entry);
        const bool rxOnlyOff = rxOnlyBlocked(entry);
        const bool paOff = paBlocked(entry);
        if (entry.requiresTransmit) {
            // A negotiated TX permission cannot make this client's absent DSP
            // available. Preserve the independent resource gate and child rules.
            // Task 16 fix wave (I1): receive only disables it as well (a
            // gated non-transmit page is handled below, fix wave 2).
            // Fix wave 2 (Important 2): so does a radio without power
            // amplifier settings, on the PA pages (all transmit pages).
            entry.widget->setEnabled(m_transmitPermitted && !entry.localDspUnavailable
                                     && !blocked && !rxOnlyOff && !paOff);
        } else if (blocked && !entry.stationDisabled) {
            entry.widget->setEnabled(false);
        } else if (!blocked && entry.stationDisabled) {
            entry.widget->setEnabled(!entry.localDspUnavailable && !entry.placeholder);
            if (!entry.localDspUnavailable) {
                entry.widget->setToolTip(QString());
            }
        }
        entry.stationDisabled = blocked;
        // Fix wave 2 (Minor 4): a non-transmit page receive only reaches
        // (Audio > TX Input) is disabled while it is on, and given back the
        // state the other gates decide when it goes off.
        // Checkpoint join: the PA pages are non-transmit pages since parity
        // Task 6, so a radio without power amplifier settings disables them
        // here too (fix wave 2, Important 2).
        bool receiveOnlyLifted = false;
        if (!entry.requiresTransmit) {
            const bool off = rxOnlyOff || paOff;
            if (off) {
                entry.widget->setEnabled(false);
            } else if (entry.receiveOnlyDisabled) {
                entry.widget->setEnabled(!blocked && !entry.localDspUnavailable
                                         && !entry.placeholder);
                receiveOnlyLifted = true;
            }
            entry.receiveOnlyDisabled = off;
        }
        if (!entry.requiresTransmit && !entry.localDspUnavailable && !blocked && !rxOnlyOff
            && !paOff) {
            if (receiveOnlyLifted) {
                entry.widget->setToolTip(QString());
            }
            continue;
        }
        entry.widget->setToolTip(unavailableReason(entry));
        if (m_stack->currentWidget() == entry.widget) {
            if (blocked) {
                showStationNotice = true;
            } else if (rxOnlyOff) {
                showReceiveOnlyNotice = true;
                receiveOnlyNoticeText = unavailableReason(entry);
            } else if (paOff) {
                showNoPaNotice = true;
            } else if (entry.requiresTransmit && !m_transmitPermitted) {
                showNotice = true;
            } else if (entry.localDspUnavailable) {
                showLocalNotice = true;
                localNoticeText = unavailableReason(entry);
            }
        }
    }
    QTreeWidgetItemIterator it(m_tree);
    while (*it) {
        const int index = (*it)->data(0, Qt::UserRole).toInt();
        if (index >= 0 && index < static_cast<int>(m_pages.size())) {
            const PageEntry& entry = m_pages[static_cast<std::size_t>(index)];
            if (coreBlocked(entry) || entry.requiresTransmit || entry.localDspUnavailable
                || entry.receiveOnlyGated) {
                (*it)->setToolTip(0, unavailableReason(entry));
            } else if (remoteSession && !entry.remoteUnavailableReason.isEmpty()) {
                // Declared unavailable but not visited yet: the leaf already
                // says why before the operator opens it.
                (*it)->setToolTip(0, entry.remoteUnavailableReason);
            } else if (remoteSession && entry.scope == SetupScope::Core) {
                // R-R3-21: the Core's settings are back; the leaf no longer
                // carries the station reason.
                (*it)->setToolTip(0, QString());
            }
        }
        ++it;
    }
    m_transmitNotice->setText(m_transmitReason);
    m_transmitNotice->setVisible(showNotice);
    m_localUnavailableNotice->setText(localNoticeText);
    m_localUnavailableNotice->setVisible(showLocalNotice);
    m_stationNotice->setText(m_stationReason);
    m_stationNotice->setVisible(showStationNotice);
    m_receiveOnlyNotice->setText(receiveOnlyNoticeText);
    m_receiveOnlyNotice->setVisible(showReceiveOnlyNotice);
    m_noPaNotice->setText(m_noPaReason);
    m_noPaNotice->setVisible(showNoPaNotice);
    // The Transmit and PA categories themselves say why, as Thetis's tab
    // pages do by being disabled as a whole: with a remote window's missing
    // transmit as well, both reasons, as the pages under them say (fix
    // wave 2, Minor 1).
    // Checkpoint join: since parity Tasks 4 to 6 most of these pages no
    // longer wait for remote transmit, so a category names the missing
    // transmit only while a page under it still does.
    const auto holdsTransmitPage = [this](const QTreeWidgetItem* category) {
        for (int i = 0; i < category->childCount(); ++i) {
            const int index = category->child(i)->data(0, Qt::UserRole).toInt();
            if (index >= 0 && index < static_cast<int>(m_pages.size())
                && m_pages[static_cast<std::size_t>(index)].requiresTransmit) {
                return true;
            }
        }
        return false;
    };
    for (QTreeWidgetItem* category : m_receiveOnlyCategories) {
        const bool transmitToo = !m_transmitPermitted && holdsTransmitPage(category);
        category->setToolTip(0, rxOnly ? m_model->rxOnlyReasonAlongside(
                                             transmitToo ? m_transmitReason : QString())
                                       : QString());
    }
    // The PA category on a radio without power amplifier settings.
    if (m_paCategoryItem && !rxOnly && !m_paAvailable) {
        m_paCategoryItem->setToolTip(0, m_noPaReason);
    }
}

void SetupDialog::markReceiveOnlyGated(QTreeWidgetItem* item, bool nonTransmitPage)
{
    if (!item) {
        return;
    }
    const int index = item->data(0, Qt::UserRole).toInt();
    if (index >= 0 && index < static_cast<int>(m_pages.size())) {
        PageEntry& entry = m_pages[static_cast<std::size_t>(index)];
        // Fix wave 2 (Minor 4): a gated page is one the gate reaches. A
        // transmit page rides the requiresTransmit enable path; any other
        // page must be named as one on purpose, and is disabled on its own
        // path in refreshTransmitPresentation.
        Q_ASSERT_X(entry.requiresTransmit || nonTransmitPage, "markReceiveOnlyGated",
                   "a receive-only gated page must be a transmit page or marked as a "
                   "non-transmit page the gate reaches");
        entry.receiveOnlyGated = true;
    }
    for (int i = 0; i < item->childCount(); ++i) {
        markReceiveOnlyGated(item->child(i), nonTransmitPage);
    }
}

void SetupDialog::markRemoteUnavailable(QTreeWidgetItem* leaf, const QString& reason)
{
    const int index = leaf ? leaf->data(0, Qt::UserRole).toInt() : -1;
    if (index < 0 || index >= static_cast<int>(m_pages.size())) {
        return;
    }
    m_pages[static_cast<std::size_t>(index)].remoteUnavailableReason = reason;
}

int SetupDialog::pageEntryIndex(const QString& label) const
{
    for (std::size_t i = 0; i < m_pages.size(); ++i) {
        if (m_pages[i].label == label) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

QWidget* SetupDialog::wrapWithAudioBackendStrip(SetupPage* page)
{
    // Returns a margin-less container QWidget that owns both the strip and the
    // page. Qt parent-ownership keeps memory clean — the container is
    // reparented into the QStackedWidget by realizePage().
    auto* container = new QWidget;
    auto* lay = new QVBoxLayout(container);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addWidget(new AudioBackendStrip(
        m_model ? m_model->localAudioDevices() : nullptr, container));
    lay->addWidget(page);
    return container;
}

// ── Tree builder ──────────────────────────────────────────────────────────────

void SetupDialog::buildTree()
{
    // #272 instrumentation: per-category registration-time tick. Each call
    // logs the elapsed time since the previous tick, so a slow category's
    // delta will stand out in the log. Resets the timer on each call.
    //
    // #272 / #301: these deltas now measure registration only (tree items +
    // std::function construction). Per-page construction cost shows up in the
    // "realized page <label>" lines emitted by realizePage() instead.
    QElapsedTimer sectionTimer;
    sectionTimer.start();
    auto tick = [&sectionTimer](const char* label) {
        const qint64 ms = sectionTimer.elapsed();
        qCDebug(lcSetupTiming) << "buildTree section" << label
                               << "elapsed (ms):" << ms;
        sectionTimer.restart();
    };

    // ── Helper: create a category (top-level, non-selectable) ─────────────────
    auto addCategory = [this](const QString& label) -> QTreeWidgetItem* {
        auto* item = new QTreeWidgetItem(m_tree, QStringList{label});
        item->setData(0, Qt::UserRole, -1);   // categories don't map to pages
        QFont f = item->font(0);
        f.setBold(true);
        item->setFont(0, f);
        item->setForeground(0, QColor("#8aa8c0"));
        return item;
    };

    // Pages are registered, not built: each registerPage() call records a
    // factory that realizePage() invokes on the leaf's first visit. Cross-page
    // connect() calls therefore live *inside* the factories, so they are set up
    // when the page that owns the signal actually exists.

    tick("helpers");

    // ── General ──────────────────────────────────────────────────────────────
    QTreeWidgetItem* general = addCategory("General");
    // R-R3-21: Mixed, since its callsign and grid are the station's
    // operator identity (User/Callsign, User/GridSquare).
    registerPage(general, "Startup & Preferences", SetupScope::Mixed,
                 [this] { return new StartupPrefsPage(m_model); });
    // R-R3-49: a page whose feature is not built yet is not registered.
    if (UnbuiltFeatures::isBuilt(UnbuiltFeature::UiScale)) {
        registerPage(general, "UI Scale & Theme", SetupScope::ThisComputer,
                     [this] { return new UiScalePage(m_model); });
    }
    if (UnbuiltFeatures::isBuilt(UnbuiltFeature::Navigation)) {
        registerPage(general, "Navigation", SetupScope::ThisComputer,
                     [this] { return new NavigationPage(m_model); });
    }
    registerPage(general, "Options", SetupScope::Mixed, [this]() -> QWidget* {
        // Phase 3M-4 Task 11: forward GeneralOptionsPage's PureSignal Info
        // Bar checkbox signals to the live PureSignal coordinator so the
        // bottom-banner FB indicator reflects the new state without having
        // to close the Setup dialog.  The page handles its own AppSettings
        // persistence; this connect handles the live wire only.
        // Mirrors Thetis chkHideFeebackLevel_CheckedChanged + chkSwapREDBluePSAColours_CheckedChanged
        // (setup.cs handlers fan out to puresignal.HideFeedback / InvertRedBlue).
        //
        // Task 3.6 (origin/main): also forward CPU meter rate spinbox so
        // MainWindow's wireSetupDialog() can connect to setCpuTimerIntervalHz.
        // The forward target is a SetupDialog signal, so MainWindow's
        // wireSetupDialog() connection made at construction time stays valid
        // however late this page is realized.
        auto* genOpts = new GeneralOptionsPage(m_model);
        if (m_model) {
            if (auto* ps = m_model->pureSignal()) {
                connect(genOpts, &GeneralOptionsPage::hideFeedbackLevelChanged,
                        ps,      &PureSignal::setHideFeedback);
                connect(genOpts, &GeneralOptionsPage::invertRedBluePsaChanged,
                        ps,      &PureSignal::setInvertRedBlue);
            }
        }
        connect(genOpts, &GeneralOptionsPage::cpuMeterRateChanged,
                this,    &SetupDialog::cpuMeterRateChanged);
        connect(genOpts, &GeneralOptionsPage::hideFeedbackLevelChanged,
                this, &SetupDialog::hideFeedbackLevelChanged);
        connect(genOpts, &GeneralOptionsPage::invertRedBluePsaChanged,
                this, &SetupDialog::invertRedBluePsaChanged);
        return genOpts;
    });

    // Parity Task 21 (R-IOS-18; the operator's "Option A"): a remote
    // window's Core, its radio first (Change radio). A window running its
    // own radio has no Core; its radio is Radio > Manage Radios. Mixed, not
    // Core: nothing on it is a setting the snapshot carries; it is built at
    // once and says itself why it waits (ThisCorePage::unavailableReason).
    if (remoteSession()) {
        registerPage(general, "This Core", SetupScope::Mixed,
                     [this] { return new ThisCorePage(m_model); });
    }

    tick("General");

    QTreeWidgetItem* cores = addCategory("Cores");
    registerPage(cores, "Your Cores", SetupScope::ThisComputer, [this]() -> QWidget* {
        auto* page = new CoresSetupPage(m_coreTargets, m_model);
        page->setContext(m_coreSettingsContext);
        if (!m_inspectedCoreTarget.isEmpty()) { page->inspectTarget(m_inspectedCoreTarget); }
        connect(page, &CoresSetupPage::connectionDetailsRequested,
                this, &SetupDialog::connectionsRequested);
        if (m_coresPageBinder) { m_coresPageBinder(page); }
        return page;
    });

    // ── Hardware ─────────────────────────────────────────────────────────────
    QTreeWidgetItem* hardware = addCategory("Hardware");

    // Task 3.6: ANAN-8000DLE volts/amps toggle — forward signal up to
    // SetupDialog so MainWindow's wireSetupDialog() can connect it to
    // setVoltsAmpsVisible().
    //
    // R-R3-21: both Hardware leaves act on the radio's hardware settings.
    // R-R3-46: in a remote window Hardware Config shows the Core's radio and
    // its receive settings write through to the Core, which applies them;
    // HardwarePage disables itself with the reason against a Core that does
    // not offer that, and its transmit fields follow the transmit
    // permission. DDC Routing (a placeholder locally too) stays declared
    // unavailable; its keys are per-MAC as well.
    const QString hardwareReason = tr(
        "The radio's hardware settings cannot be changed from a remote window.");
    registerPage(hardware, "Hardware Config", SetupScope::Core, [this]() -> QWidget* {
        auto* hwPage = new HardwarePage(m_model);
        connect(hwPage, &HardwarePage::anan8000DleVoltsAmpsChanged,
                this,   &SetupDialog::anan8000DleVoltsAmpsChanged);
        return hwPage;
    });

    // Phase 3F Sub-Epic E Tasks 8-10: DDC Routing power-user override page.
    // Skeleton-only landing; per-DDC table + override schema follow once
    // codec layer (Sub-Epic B) is in place.
    // R-R3-49 (ddc-routing): its choices do not steer the radio's receivers
    // yet; the page is hidden until multi-panadapter receiver routing is
    // built.
    if (UnbuiltFeatures::isBuilt(UnbuiltFeature::DdcRouting)) {
        markRemoteUnavailable(registerPage(hardware, "DDC Routing", SetupScope::Core, [this]() -> QWidget* {
            return new HardwareDdcRoutingPage(m_model);
        }), hardwareReason);
    }

    // ── PA ────────────────────────────────────────────────────────────────────
    // Top-level PA category mirrors Thetis tpPowerAmplifier
    // (setup.designer.cs:47366-47371 [v2.10.3.13]). Three sub-pages:
    //   - PA Gain         → Thetis tpGainByBand (Phase 6+7 live editor)
    //   - Watt Meter      → Thetis tpWattMeter (cal spinboxes — Phase 3)
    //   - PA Values       → NereusSDR-spin live telemetry page (Phase 4)
    //
    // Phase 8 of #167 — the PA category and 3 sub-pages are now ALWAYS
    // built. Per-SKU availability is driven dynamically via
    // applyPaVisibility() (called from onCurrentRadioChanged + at end of
    // ctor). Task 16 fix wave 2: never hidden; the pages are disabled with
    // the reason when caps.isRxOnlySku (the kit's) or when
    // !caps.hasPaProfile. Each child page additionally toggles
    // its own informational rows / banners per the BoardCapabilities flags.
    //
    // This replaces the construction-time hasPaProfile gate (which
    // prevented the PA category from appearing on radio-swap when the
    // dialog was already open) with a live capability subscription.
    // From Thetis comboRadioModel_SelectedIndexChanged (setup.cs:19812-20310
    // [v2.10.3.13+501e3f51]) — per-SKU PA tab visibility.
    tick("Hardware");

    m_paCategoryItem  = addCategory("PA");
    m_receiveOnlyCategories.push_back(m_paCategoryItem);

    // #272 / #301: each PA factory re-applies the live BoardCapabilities to
    // its own page, because applyPaVisibility() ran in the ctor (or on an
    // earlier currentRadioChanged) while the page pointer was still null.
    //
    // R-R3-46 / R-R3-10: the three are transmit settings. A remote window
    // now knows the Core's radio, so they are shown for a radio that has
    // them. R-R3-49 (parity Task 6): no longer held whole for remote
    // transmit. The Core takes their settings while its radio is off the
    // air, so each page gates its own controls on transmitSettingsVersion
    // 6; PA Gain's auto-calibrate sweep, which keys the radio, keeps the
    // transmit permission. Local mode is always permitted.
    m_paGainItem = registerPage(m_paCategoryItem, "PA Gain", SetupScope::Core, [this]() -> QWidget* {
        m_paGainPage = new PaGainByBandPage(m_model);
        m_paGainPage->applyCapabilityVisibility(capsForModel(m_model));
        return m_paGainPage;
    });

    m_paWattMeterItem = registerPage(m_paCategoryItem, "Watt Meter", SetupScope::Core,
                                     [this]() -> QWidget* {
        m_paWattMeterPage = new PaWattMeterPage(m_model);
        m_paWattMeterPage->applyCapabilityVisibility(capsForModel(m_model));

        // Phase 9 of #167: cross-wire PaWattMeterPage's [Reset PA Values]
        // button (Phase 5A — emits resetPaValuesRequested) to PaValuesPage's
        // resetPaValues() public slot (Phase 5B — clears peak/min trackers).
        // Deferred from Phase 5 to keep agents 5A and 5B mutually parallel and
        // conflict-free; the connect lands here once both pages exist.
        // Mirrors Thetis btnResetPAValues_Click (setup.cs:16346-16357
        // [v2.10.3.13+501e3f51]) — Thetis blanks the textbox text directly
        // from the same panel; NereusSDR fans out to a peer page since the
        // PA Values readout was promoted to its own dedicated page.
        //
        // #272 / #301: this is the one connect() in the dialog that spans two
        // sibling pages, so it now routes through the dialog. The Watt Meter
        // page can be realized while PA Values still is not; realizing the
        // sibling here (on button press, not on dialog open) keeps the fan-out
        // working without forcing a second page build up front.
        connect(m_paWattMeterPage, &PaWattMeterPage::resetPaValuesRequested,
                this, [this]() {
                    realizePage(m_paValuesEntry);
                    if (m_paValuesPage) {
                        m_paValuesPage->resetPaValues();
                    }
                });
        // R-R3-49 (found bug): Show PA Values page shows or hides the PA
        // Values page at once, as Thetis's chkPAValues shows or hides its
        // panel (setup.cs:16381-16385 [v2.10.3.15]).
        connect(m_paWattMeterPage, &PaWattMeterPage::showPaValuesPageChanged,
                this, [this](bool) { applyShowPaValuesPage(); });
        return m_paWattMeterPage;
    });

    m_paValuesItem = registerPage(m_paCategoryItem, "PA Values", SetupScope::Core, [this]() -> QWidget* {
        m_paValuesPage = new PaValuesPage(m_model);
        m_paValuesPage->applyCapabilityVisibility(capsForModel(m_model));
        return m_paValuesPage;
    });

    // Cache the registry index so the Watt Meter cross-wire above can realize
    // the PA Values page without a label lookup on every button press.
    m_paValuesEntry = m_paValuesItem->data(0, Qt::UserRole).toInt();
    // R-R3-49: the Watt Meter's Show PA Values page, as saved.
    applyShowPaValuesPage();
    // Task 16 fix wave (I1): tpPowerAmplifier.Enabled = !RXOnly
    // (setup.cs:6500 [v2.10.3.15]). Checkpoint join: the PA pages are no
    // longer transmit pages (parity Task 6), so the gate reaches them as
    // non-transmit pages.
    markReceiveOnlyGated(m_paCategoryItem, /*nonTransmitPage=*/true);

    tick("PA");

    // ── Audio ─────────────────────────────────────────────────────────────────
    QTreeWidgetItem* audio = addCategory("Audio");
    // R-R3-23: Devices picks this computer's speakers, headphones and
    // microphone, which a remote window uses too (remote playback, Test
    // Mic), so it works in every window, connected or not.
    registerPage(audio, "Devices", SetupScope::ThisComputer,
                 [this] { return wrapWithAudioBackendStrip(new AudioDevicesPage(m_model)); });
    // R-R3-36: the PC microphone device, backend, buffer and Test Mic are
    // this computer's; the mic source, mic gain and radio microphone
    // hardware controls follow the transmit permission inside the page
    // (AudioTxInputPage::setTransmitPermitted), so the leaf itself is no
    // longer a whole-page transmit leaf.
    //
    // Task 16 fix wave 2 (Minor 4): receive only disables it, with its
    // reason, as Thetis's tpTransmit.Enabled = !RXOnly (setup.cs:6499
    // [v2.10.3.15]) disables grpBoxMic, which sits on tpTransmit
    // (setup.designer.cs:46443 [v2.10.3.15]). It is not a transmit page, so a
    // remote window without transmit keeps it live (R-R3-36).
    markReceiveOnlyGated(
        registerPage(audio, "TX Input", SetupScope::Mixed,  // I.1
                     [this] { return wrapWithAudioBackendStrip(new AudioTxInputPage(m_model)); }),
        /*nonTransmitPage=*/true);
    // R-R3-44: the VAX channels are this computer's in a remote window as in
    // a local one (a remote window feeds them from the Core's receiver
    // streams), and the page writes only this computer's audio/Vax* keys,
    // so it works in every window, connected or not.
    registerPage(audio, "VAX", SetupScope::ThisComputer,
                 [this] {
                     auto* vaxPage = new AudioVaxPage(m_model);
                     // R-R3-43 / R-R3-44: the compressed-audio note's state.
                     m_vaxPage = vaxPage;
                     vaxPage->setReceiverAudioNote(m_receiverAudioNote);
                     return wrapWithAudioBackendStrip(vaxPage);
                 });
    // R-R3-42: Audio > TCI configures the TCI server that runs on this
    // computer, in a remote window as in a local one, and its keys are this
    // computer's (SettingsScope "Tci"). It reaches no local DSP (the backend
    // strip goes through localAudioDevices()), so it works in a remote
    // window, connected or not.
    registerPage(audio, "TCI", SetupScope::ThisComputer,
                 [this] { return wrapWithAudioBackendStrip(new AudioTciPage(m_model)); });
    // R-R3-44: Mixed. Its VAX groups (VAX feedback tuning, the VAX flags,
    // detected cables, Reset) are this computer's and work in a remote
    // window; the DSP group writes the Core's audio/DspRate and
    // audio/DspBlockSize and follows the Core's settings availability.
    registerPage(audio, "Advanced", SetupScope::Mixed,
                 [this] { return wrapWithAudioBackendStrip(new AudioAdvancedPage(m_model)); });
    // Phase 3M-1c J.3: TX Profile editor.
    //
    // R-R3-49 (parity Task 3): no longer held for remote transmit. In a
    // remote window the manager mirrors the Core's profiles and the page
    // gates its own controls on transmitSettingsVersion 3
    // (TxProfileSetupPage::setTransmitSettingsPermittedAt).
    //
    // 3M-1c L.1 update: RadioModel now constructs MicProfileManager in its
    // ctor (per RadioModel::m_micProfileMgr in RadioModel.cpp), so this page
    // gets the live manager pointer at SetupDialog construction time.  The
    // manager itself is per-MAC scoped — setMacAddress + load() run inside
    // RadioModel::connectToRadio().  Before any radio has connected the
    // manager is unscoped and every mutator silently no-ops; the page still
    // renders correctly (combo is empty) and Setup → TX Profile is harmless.
    // Task 16 fix wave (I1): Thetis's TX profile group (grpTXProfile) sits on
    // tpTransmit (setup.designer.cs:46448 [v2.10.3.15]), so receive only
    // disables it with the Transmit category (setup.cs:6499). Thetis's
    // "TX Profile has changed" label is hidden while receive only is on
    // (setup.cs:27362 [v2.10.3.15]:
    //   lblTXProfileWarning.Visible = !console.RXOnly && bChanged;);
    // NereusSDR shows no such label, so there is nothing to hide.
    markReceiveOnlyGated(registerPage(audio, "TX Profile", SetupScope::Core, [this]() -> QWidget* {
        return new TxProfileSetupPage(
            m_model,
            m_model ? m_model->micProfileManager() : nullptr,
            m_model ? &m_model->transmitModel() : nullptr);
    }), /*nonTransmitPage=*/true);

    tick("Audio");

    // ── DSP ───────────────────────────────────────────────────────────────────
    QTreeWidgetItem* dsp = addCategory("DSP");
    registerPage(dsp, "AGC/ALC", SetupScope::Core, [this] {
        return new AgcAlcSetupPage(m_model, nullptr, m_dspReceiverSelection);
    });
    // R-R3-21: Core, not Mixed. Every control writes the active receiver
    // (mirrored to the Core) or chooses among the Core's own models.
    registerPage(dsp, "NR/ANF", SetupScope::Core, [this] {
        return new NrAnfSetupPage(m_model, nullptr, m_dspReceiverSelection);
    });
    registerPage(dsp, "NB/SNB", SetupScope::Core, [this] {
        return new NbSnbSetupPage(m_model, nullptr, m_dspReceiverSelection);
    });
    registerPage(dsp, "CW", SetupScope::Core, [this] {
        return new CwSetupPage(m_model, nullptr, m_dspReceiverSelection);
    });
    registerPage(dsp, "AM/SAM", SetupScope::Core, [this] {
        return new AmSamSetupPage(m_model, nullptr, m_dspReceiverSelection);
    });
    registerPage(dsp, "FM", SetupScope::Core, [this] {
        return new FmSetupPage(m_model, nullptr, m_dspReceiverSelection);
    });
    // (DSP > "VOX/DEXP" placeholder removed in 3M-3a-iii Task 16 — the wired
    //  page lives at Transmit > "DEXP/VOX" (DexpVoxPage from Task 14).)

    // Phase 3M-3a-ii Batch 6 (Task 3): CfcSetupPage's [Configure CFC bands…]
    // button emits openCfcDialogRequested.  Forward up to SetupDialog's
    // cfcDialogRequested signal so MainWindow can route it to the
    // TxApplet::requestOpenCfcDialog() slot (the same modeless dialog
    // instance is shared with the [CFC] right-click on the TxApplet).
    //
    // R-R3-21: a transmit page. Phase Rotator, CFC and CESSB are all TXA
    // stages, and the [Configure CFC bands] button opens the TX CFC editor.
    // R-R3-49 (parity Task 4): no longer held for remote transmit. The Core
    // mirrors every setting on it; the page gates its own controls on
    // transmitSettingsVersion 4 (CfcSetupPage::setTransmitSettingsPermittedAt).
    registerPage(dsp, "CFC", SetupScope::Core, [this]() -> QWidget* {
        auto* cfcPage = new CfcSetupPage(m_model);
        connect(cfcPage, &CfcSetupPage::openCfcDialogRequested,
                this,    &SetupDialog::cfcDialogRequested);
        return cfcPage;
    });

    registerPage(dsp, "TNF", SetupScope::Core, [this] {
        return new MnfSetupPage(m_model, nullptr, m_dspReceiverSelection);
    });
    // Stage C2: user-customisable filter preset editor (10 slots × 12 modes).
    // iPhone app Task 19 (D40): the presets live on the Core ("filters/" is
    // Station scope), so in a remote window this page shows and edits the
    // Core's; a window running its radio locally is unchanged.
    registerPage(dsp, "Filter Presets", SetupScope::Core, [this]() -> QWidget* {
        return new FilterPresetsSetupPage(
            m_model ? m_model->filterPresetStore() : nullptr,
            m_model);
    });

    // Task 4.1: DSP → Options page (buffer/filter size+type, impulse cache,
    // high-res filter characteristics, time-to-last-change readout).
    // Mirrors Thetis tpDSPOptions tab (design Section 4A).
    registerPage(dsp, "Options", SetupScope::Core, [this] { return new DspOptionsPage(m_model); });

    tick("DSP");

    // ── Display ───────────────────────────────────────────────────────────────
    QTreeWidgetItem* display = addCategory("Display");

    // Task 2.4: SpectrumDefaultsPage gains cross-link buttons to Spectrum Peaks
    // (and a forward-reference to Multimeter which lands in Task 3.1).
    //
    // #272 / #301: the cross-links go through selectPage(), which drives the
    // nav tree, so the destination page is realized by the tree-selection
    // handler. No sibling-page pointer is needed here.
    registerPage(display, "Spectrum Defaults", SetupScope::Mixed, [this]() -> QWidget* {
        auto* specDefaultsPage = new SpectrumDefaultsPage(m_model);
        connect(specDefaultsPage, &SpectrumDefaultsPage::navigateToSpectrumPeaksRequested,
                this, [this]() { selectPage(QStringLiteral("Spectrum Peaks")); });
        // Task 3.1: Multimeter page now exists — wire the cross-link.
        connect(specDefaultsPage, &SpectrumDefaultsPage::navigateToMultimeterRequested,
                this, [this]() { selectPage(QStringLiteral("Multimeter")); });
        return specDefaultsPage;
    });

    // Task 2.4: Spectrum Peaks page — skeleton with APH + Blob controls + back cross-link.
    registerPage(display, "Spectrum Peaks", SetupScope::ThisComputer, [this]() -> QWidget* {
        auto* specPeaksPage = new SpectrumPeaksPage(m_model);
        connect(specPeaksPage, &SpectrumPeaksPage::backToSpectrumDefaultsRequested,
                this, [this]() { selectPage(QStringLiteral("Spectrum Defaults")); });
        return specPeaksPage;
    });

    registerPage(display, "Waterfall Defaults", SetupScope::ThisComputer,
                 [this] { return new WaterfallDefaultsPage(m_model); });
    registerPage(display, "Grid & Scales", SetupScope::Mixed,
                 [this] { return new GridScalesPage(m_model); });

    // Task 3.1: Display → Multimeter — 8 multimeter globals + unit-mode + signal history.
    // Folded from Thetis Display→General Multimeter group per design Section 3A.
    // Cross-link: ← Spectrum Defaults / SpectrumDefaultsPage → Multimeter.
    registerPage(display, "Multimeter", SetupScope::Mixed, [this]() -> QWidget* {
        auto* multimeterPage = new MultimeterPage(m_model);
        connect(multimeterPage, &MultimeterPage::backToSpectrumDefaultsRequested,
                this, [this]() { selectPage(QStringLiteral("Spectrum Defaults")); });
        return multimeterPage;
    });

    registerPage(display, "TX Display", SetupScope::Mixed,  [this] { return new TxDisplayPage(m_model);  });

    // 3D Stacked-Trace Spectrum Plan Task 15: mirrors the Task 13 overlay
    // menu's six 3D controls into Setup -> Display. Constructed against
    // the SpectrumWidget directly (not RadioModel, unlike every page
    // above) per Display3DSetupPage's own class-header comment.
    registerPage(display, "3D View", SetupScope::ThisComputer, [this]() -> QWidget* {
        return new Display3DSetupPage(m_model ? m_model->spectrumWidget() : nullptr);
    });

    tick("Display");

    // ── Transmit ──────────────────────────────────────────────────────────────
    QTreeWidgetItem* transmit = addCategory("Transmit");
    // R-R3-49 (parity Task 5): no longer held for remote transmit. The Core
    // mirrors every setting on it; the page gates its own controls on
    // transmitSettingsVersion 5 (PowerPage::setTransmitSettingsPermittedAt).
    registerPage(transmit, "Power", SetupScope::Core,       [this] { return new PowerPage(m_model);      });
    if (UnbuiltFeatures::isBuilt(UnbuiltFeature::TxProfilesLeaf)) {
        registerPage(transmit, "TX Profiles", SetupScope::Core,
                     [this] { return new TxProfilesPage(m_model); }, true);
    }

    // SpeechProcessorPage is the TX dashboard (3M-3a-i Batch 5).  Its
    // openSetupRequested(category, page) signal feeds straight back into
    // selectPage() so the cross-link buttons jump within the same dialog
    // instance — no MainWindow round-trip required.
    // R-R3-49 (parity Task 4): no longer held for remote transmit. The page
    // shows the Core's TX chain and opens the TX EQ dialog and the pages
    // that change it, each of which follows the Core's gate.
    registerPage(transmit, "Speech Processor", SetupScope::Core, [this]() -> QWidget* {
        auto* speechPage = new SpeechProcessorPage(m_model);
        connect(speechPage, &SpeechProcessorPage::openSetupRequested,
                this, [this](const QString& /*category*/, const QString& page) {
            selectPage(page);
        });
        return speechPage;
    });

    // Note: Setup → Transmit → PureSignal page retired in Phase 3M-4 Task 14
    // (no Thetis equivalent; PsForm at Tools > PureSignal is the entire PS
    // control surface — design §4.2).

    // Phase 3M-3a-iii Task 14: full DexpVoxPage that mirrors Thetis tpDSPVOXDE
    // 1:1 (setup.designer.cs:44763-45260 [v2.10.3.13]).  Registered as the
    // "DEXP/VOX" leaf so PhoneCwApplet's Task 15 right-click target
    // (SetupDialog::selectPage("DEXP/VOX")) lands here.  This is distinct
    // from the legacy DSP > VOX/DEXP placeholder above (line 245), which
    // remains a lightweight 4-control disabled stub for back-compat with
    // the Thetis tpDSPVOX tab IA.
    // R-R3-49 (parity Task 5): no longer held for remote transmit. Enable VOX
    // follows the transmit permission; the rest follows
    // transmitSettingsVersion 5 (DexpVoxPage).
    registerPage(transmit, "DEXP/VOX", SetupScope::Core, [this] { return new DexpVoxPage(m_model); });

    // 2026-05-22 menu cleanup: the standalone "PGXL Interlock" entry that
    // previously lived here is removed. The same controls live under
    // Setup -> CAT & Network -> 4O3A -> General as an embedded section
    // (FourO3APage owns the PgxlInterlockPage instance).

    // Task 16 fix wave (I1): tpTransmit.Enabled = !RXOnly
    // (setup.cs:6499 [v2.10.3.15]). Checkpoint join: Power, Speech
    // Processor and DEXP/VOX are no longer transmit pages (parity Tasks 4
    // and 5), so the gate reaches them as non-transmit pages; TX Profiles
    // still rides the transmit path.
    markReceiveOnlyGated(transmit, /*nonTransmitPage=*/true);
    m_receiveOnlyCategories.push_back(transmit);

    tick("Transmit");

    // ── Appearance ────────────────────────────────────────────────────────────
    QTreeWidgetItem* appearance = addCategory("Appearance");
    registerPage(appearance, "Colors & Theme", SetupScope::ThisComputer,
                 [this] { return new ColorsThemePage(m_model); });
    registerPage(appearance, "Meter Styles", SetupScope::ThisComputer, [this]() -> QWidget* {
        auto* page = new MeterStylesPage(m_model);
        connect(page, &MeterStylesPage::sMeterFaceChanged,
                this, &SetupDialog::sMeterFaceChanged);
        connect(page, &MeterStylesPage::sMeterPeakHoldChanged,
                this, &SetupDialog::sMeterPeakHoldChanged);
        connect(page, &MeterStylesPage::sMeterPeakDecayChanged,
                this, &SetupDialog::sMeterPeakDecayChanged);
        return page;
    });
    if (UnbuiltFeatures::isBuilt(UnbuiltFeature::Skins)) {
        registerPage(appearance, "Skins", SetupScope::ThisComputer,
                     [this] { return new SkinsPage(m_model); });
    }
    if (UnbuiltFeatures::isBuilt(UnbuiltFeature::MinimalMode)) {
        registerPage(appearance, "Collapsible Display", SetupScope::ThisComputer,
                     [this] { return new CollapsibleDisplayPage(m_model); });
    }

    tick("Appearance");

    // ── CAT & Network ─────────────────────────────────────────────────────────
    QTreeWidgetItem* cat = addCategory("CAT & Network");
    if (UnbuiltFeatures::isBuilt(UnbuiltFeature::Cat)) {
        registerPage(cat, "Serial Ports", SetupScope::ThisComputer,
                     [] { return new CatSerialPortsPage; });
    }
    // R-R3-42: this computer's TCI server and its own settings.
    registerPage(cat, "TCI Server", SetupScope::ThisComputer, [this]() -> QWidget* {
        // Phase 3J-1 review P2.4: forward CatTciServerPage::tciServerEnableToggled
        // through SetupDialog so wireSetupDialog() can connect it to the live
        // TciServer::start() / stop() path in MainWindow.
        //
        // Phase 3J-1 closeout Item 1 (2026-05-12): same pattern for the new
        // tciServerBindOrPortChanged signal so MainWindow can live-restart the
        // server when the operator picks a different bind interface or port.
        auto* tciPage = new CatTciServerPage;
        m_tciServerPage = tciPage;  // saved so setTciServer() can forward
        connect(tciPage, &CatTciServerPage::tciServerEnableToggled,
                this,    &SetupDialog::tciServerEnableToggled);
        connect(tciPage, &CatTciServerPage::tciServerBindOrPortChanged,
                this,    &SetupDialog::tciServerBindOrPortChanged);
        // Phase 3J-1 closeout Item 2 (2026-05-12): forward showLogRequested up
        // to MainWindow so the log window outlives this dialog's close.
        connect(tciPage, &CatTciServerPage::showLogRequested,
                this,    &SetupDialog::tciShowLogRequested);
        // #272 / #301: replay the TciServer pointer MainWindow handed us at
        // wireSetupDialog() time, so the Server group box title and Status
        // label are live on the operator's very first visit to this page.
        if (m_pendingTciServer) {
            tciPage->setTciServer(m_pendingTciServer);
        }
        // R-R3-48: the Core's station TCI server line.
        tciPage->setRadioModel(m_model);
        return tciPage;
    });
    // 4O3A integration page (replaces the previous standalone
    // "Peripherals", "PGXL Advanced", and "TGXL Advanced" entries
    // that lived side-by-side under CAT & Network).  FourO3APage
    // hosts a QTabWidget with four tabs (General / PowerGenius XL /
    // Tuner Genius XL / Diagnostics); the General tab carries the
    // master toggle that gates the FlexAPI listener and auto-connect.
    //
    // The antennaLabelChanged forwarding from TgxlAdvancedPage moves
    // inside FourO3APage's construction below so the SetupDialog
    // signal still fires through to TunerApplet::onAntennaLabelChanged
    // via wireSetupDialog().
    registerPage(cat, "4O3A", SetupScope::Core, [this]() -> QWidget* {
        auto* fourO3A = new FourO3APage(m_model);
        // Phase 3P-II Phase 4 Task 95 forwarding lives on FourO3APage
        // now; surface the embedded TGXL page's antennaLabelChanged
        // signal so wireSetupDialog continues to bridge it to TunerApplet.
        if (auto* tgxlAdv = fourO3A->findChild<TgxlAdvancedPage*>()) {
            connect(tgxlAdv, &TgxlAdvancedPage::antennaLabelChanged,
                    this, &SetupDialog::tgxlAntennaLabelChanged);
        }
        return fourO3A;
    });
    registerPage(cat, "Remote Access", SetupScope::ThisComputer, [this] {
        auto* page = new RemoteStationPage;
        connect(page, &RemoteStationPage::connectionsRequested,
                this, &SetupDialog::connectionsRequested);
        if (m_remoteStationPageBinder) { m_remoteStationPageBinder(page); }
        return page;
    });
    // R-R3-47: in a remote window RfKitPage asks the Core to switch,
    // connect and disconnect its amplifier (it never dials the amp from
    // this computer); a Core that does not offer that says so on the page.
    registerPage(cat, "RF-Kit", SetupScope::Core, [this] { return new RfKitPage(m_model); });
    if (UnbuiltFeatures::isBuilt(UnbuiltFeature::Cat)) {
        registerPage(cat, "TCP/IP CAT", SetupScope::ThisComputer, [] { return new CatTcpIpPage; });
    }
    if (UnbuiltFeatures::isBuilt(UnbuiltFeature::Midi)) {
        registerPage(cat, "MIDI Control", SetupScope::ThisComputer,
                     [] { return new CatMidiControlPage; });
    }

    tick("CAT & Network");

    // ── Keyboard ──────────────────────────────────────────────────────────────
    QTreeWidgetItem* keyboard = addCategory("Keyboard");
    if (UnbuiltFeatures::isBuilt(UnbuiltFeature::Keyboard)) {
        registerPage(keyboard, "Shortcuts", SetupScope::ThisComputer,
                     [] { return new KeyboardShortcutsPage; });
    }

    tick("Keyboard");

    // ── Test ──────────────────────────────────────────────────────────────────
    // Phase 3M-1c H.1: top-level Test category for the Two-Tone IMD page.
    QTreeWidgetItem* test = addCategory("Test");
    // R-R3-21: every control writes the TransmitModel's two-tone test
    // settings, which a keyed two-tone transmission reads when it starts.
    // R-R3-49 (parity Task 5): no longer held for remote transmit. The
    // settings key nothing; the page gates them on transmitSettingsVersion
    // 5, and the two-tone start (the TX applet's 2-Tone) keeps the transmit
    // permission.
    // Task 16 fix wave (I1): grpTestTXIMD.Enabled = !RXOnly
    // (setup.cs:6501 [v2.10.3.15]). Checkpoint join: no longer a transmit
    // page (parity Task 5), so the gate reaches it as a non-transmit page.
    markReceiveOnlyGated(registerPage(test, "Two-Tone IMD", SetupScope::Core,
                                      [this] { return new TestTwoTonePage(m_model); }),
                         /*nonTransmitPage=*/true);

    tick("Test");

    // ── Diagnostics ───────────────────────────────────────────────────────────
    QTreeWidgetItem* diagnostics = addCategory("Diagnostics");
    registerPage(diagnostics, "Radio Status", SetupScope::Core,
                 [this] { return new RadioStatusPage(m_model); });
    registerPage(diagnostics, "Connection Quality", SetupScope::Core,
                 [this] { return new ConnectionQualityPage(m_model); });
    registerPage(diagnostics, "Settings Validation", SetupScope::Mixed,
                 [this] { return new SettingsValidationPage(m_model); });
    registerPage(diagnostics, "Export / Import", SetupScope::ThisComputer,
                 [this] { return new ExportImportConfigPage(m_model); });
    // Remote-window parity Task 22 (R-R3-49): in a remote window the page
    // also shows the Core's recent log.
    registerPage(diagnostics, "Logs", SetupScope::ThisComputer,
                 [this] { return new LogsPage(m_model); });
    if (UnbuiltFeatures::isBuilt(UnbuiltFeature::SignalGenerator)) {
        registerPage(diagnostics, "Signal Generator", SetupScope::Core,
                     [] { return new DiagSignalGeneratorPage; });
        registerPage(diagnostics, "Hardware Tests", SetupScope::Core,
                     [] { return new DiagHardwareTestsPage; });
    }
    // R-R3-49: with its Performance checkboxes removed, this page holds only
    // the logging groups, hidden until logging is built. A page with nothing
    // to show is not registered, so neither the tree nor selectPage() finds it.
    if (UnbuiltFeatures::isBuilt(UnbuiltFeature::Logging)) {
        registerPage(diagnostics, "Logging & Performance", SetupScope::ThisComputer,
                     [] { return new DiagLoggingPage; });
    }

    tick("Diagnostics");

    // R-R3-49: a category left with no pages (Keyboard, while shortcut
    // editing is not built) is not shown.
    for (int i = m_tree->topLevelItemCount() - 1; i >= 0; --i) {
        QTreeWidgetItem* category = m_tree->topLevelItem(i);
        if (category->childCount() == 0) {
            delete m_tree->takeTopLevelItem(i);
        }
    }

    m_tree->expandAll();
    tick("expandAll");
}

// ── Phase 8 of #167: PA category visibility wiring ─────────────────────────────
//
// onCurrentRadioChanged: re-evaluate PA visibility when the connected
// radio changes (radio swap, fresh connect, MAC switch). Forwarded
// from RadioModel::currentRadioChanged.
//
// applyPaVisibility: collapses the per-SKU decisions into a single
// switch. The PA pages are disabled with the reason (never hidden, Task 16
// fix wave 2) when caps.isRxOnlySku (no TX hardware at all) or when
// !caps.hasPaProfile (the connected board has TX but no PA gain
// calibration support: Atlas, RedPitaya).
// Each child page additionally gates its own warning rows on the
// individual capability flags via applyCapabilityVisibility().
//
// From Thetis comboRadioModel_SelectedIndexChanged
// (setup.cs:19812-20310 [v2.10.3.13+501e3f51]) — per-SKU PA tab visibility.
// Thetis swaps dozens of controls per HPSDRModel; NereusSDR collapses
// the decisions into BoardCapabilities and surfaces the equivalent
// visibility here.

void SetupDialog::onCurrentRadioChanged(const RadioInfo& /*info*/)
{
    if (!m_model) { return; }
    applyPaVisibility(m_model->boardCapabilities());
}

void SetupDialog::applyShowPaValuesPage()
{
    // The Watt Meter's "Show PA Values page" (display/showPaValuesPage,
    // default True, the page's own reading of it).
    if (m_paValuesItem) {
        m_paValuesItem->setHidden(
            AppSettings::instance().value(QStringLiteral("display/showPaValuesPage"),
                                          QStringLiteral("True")).toString()
            != QStringLiteral("True"));
    }
}

void SetupDialog::applyPaVisibility(const BoardCapabilities& caps)
{
    // Task 16 fix wave 2 (Important 2): the operator's rule (2026-09-25) is
    // that a control that cannot run is shown disabled with its reason,
    // never hidden. The PA category and its pages stay in the tree on every
    // radio. On the receive-only kit they are disabled with the kit's reason
    // (receive only, refreshTransmitPresentation); on a radio without power
    // amplifier settings (Atlas, or no radio yet) with that reason.
    m_paAvailable = !caps.isRxOnlySku && caps.hasPaProfile;
    m_noPaReason = caps.board == HPSDRHW::Unknown
        ? tr("Connect a radio to change its power amplifier settings.")
        : tr("This radio has no power amplifier settings.");
    for (QTreeWidgetItem* item : {m_paCategoryItem, m_paGainItem, m_paWattMeterItem,
                                  m_paValuesItem}) {
        if (item) {
            item->setHidden(false);
        }
    }
    // R-R3-49: except PA Values, which the operator hides with Watt Meter >
    // Show PA Values page (applyShowPaValuesPage).
    applyShowPaValuesPage();
    for (QTreeWidgetItem* leaf : {m_paGainItem, m_paWattMeterItem, m_paValuesItem}) {
        const int index = leaf ? leaf->data(0, Qt::UserRole).toInt() : -1;
        if (index >= 0 && index < static_cast<int>(m_pages.size())) {
            m_pages[static_cast<std::size_t>(index)].paPage = true;
        }
    }

    // Forward the caps to each PA page so it can self-toggle the
    // per-SKU informational rows. Page-level visibility decisions
    // (warning labels, banner copy, individual control gates) live
    // inside the page implementations — SetupDialog only owns the
    // category-level decision.
    //
    // #272 / #301: any of these three pointers may still be null because its
    // leaf has not been visited yet. Skipping it is correct: the page factory
    // applies capsForModel() itself the moment the page is realized, so a page
    // built after a radio swap picks up the current caps either way.
    if (m_paGainPage) {
        m_paGainPage->applyCapabilityVisibility(caps);
    }
    if (m_paWattMeterPage) {
        m_paWattMeterPage->applyCapabilityVisibility(caps);
    }
    if (m_paValuesPage) {
        m_paValuesPage->applyCapabilityVisibility(caps);
    }
    refreshTransmitPresentation();
}


void SetupDialog::reloadMeterStyles()
{
    for (MeterStylesPage* page : findChildren<MeterStylesPage*>()) {
        page->reloadSMeterSettings();
    }
}

void SetupDialog::reloadFeedbackPreferences()
{
    for (GeneralOptionsPage* page : findChildren<GeneralOptionsPage*>()) {
        page->reloadFeedbackPreferences();
    }
}

} // namespace NereusSDR
