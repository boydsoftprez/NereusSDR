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
// =================================================================

#include "SetupDialog.h"
#include "SetupPage.h"
#include "core/BoardCapabilities.h"
#include "core/PureSignal.h"
#include "models/RadioModel.h"

// General
#include "setup/GeneralSetupPages.h"
#include "setup/GeneralOptionsPage.h"
// Hardware
#include "setup/HardwarePage.h"
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

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QShowEvent>
#include <QElapsedTimer>
#include <QLoggingCategory>

#include <utility>

namespace NereusSDR {

namespace {

// Current board capabilities for the connected radio, with the same
// conservative fallback the ctor has always used: boardCapabilities()
// returns Unknown caps when no radio has ever connected, and Unknown sets
// hasPaProfile=false so the PA category stays hidden.
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
        "font-size: 12px; selection-background-color: #00b4d8; }"
        "QTreeWidget::item { padding: 4px 8px; }"
        "QTreeWidget::item:hover { background: #1a2a3a; }");

    // Stacked widget for page content
    m_stack = new QStackedWidget;
    m_stack->setStyleSheet("QStackedWidget { background: #0f0f1a; }");

    splitter->addWidget(m_tree);
    splitter->addWidget(m_stack);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

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
    }

    // Apply initial visibility at construction time so the dialog opens
    // with the right state even when no currentRadioChanged has fired yet.
    // boardCapabilities() falls back to Unknown caps when no radio has
    // ever connected; Unknown sets hasPaProfile=false → PA category
    // hidden, matching the conservative default.
    //
    // #272 / #301: this pass now only decides nav-tree row visibility, since
    // the three PA page widgets do not exist yet. Each PA factory re-applies
    // the live caps to its own page at realization time.
    applyPaVisibility(capsForModel(m_model));
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

// ── Lazy page registry (issues #272 + #301) ───────────────────────────────────

QTreeWidgetItem* SetupDialog::registerPage(QTreeWidgetItem* parent,
                                          const QString& label,
                                          std::function<QWidget*()> factory)
{
    auto* item = new QTreeWidgetItem(parent, QStringList{label});
    item->setData(0, Qt::UserRole, static_cast<int>(m_pages.size()));
    m_pages.push_back(PageEntry{label, std::move(factory), nullptr, -1});
    return item;
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

    // Move the factory out before invoking it so a page whose construction
    // re-enters realizePage() (e.g. a cross-page connect firing during the
    // ctor) cannot recurse into building the same page twice.
    const std::function<QWidget*()> factory = std::move(entry.factory);
    entry.factory = nullptr;

    QElapsedTimer realizeTimer;
    realizeTimer.start();
    QWidget* page = factory();
    if (page == nullptr) {
        qCWarning(lcSetupTiming) << "page factory yielded nothing for"
                                 << entry.label;
        return nullptr;
    }

    entry.widget     = page;
    entry.stackIndex = m_stack->addWidget(page);
    qCDebug(lcSetupTiming) << "realized page" << entry.label
                           << "elapsed (ms):" << realizeTimer.elapsed();
    return page;
}

void SetupDialog::showPageAt(int entryIndex)
{
    if (realizePage(entryIndex) == nullptr) {
        return;
    }
    m_stack->setCurrentIndex(
        m_pages[static_cast<std::size_t>(entryIndex)].stackIndex);
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
    lay->addWidget(new AudioBackendStrip(m_model->audioEngine(), container));
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
    registerPage(general, "Startup & Preferences",
                 [this] { return new StartupPrefsPage(m_model); });
    registerPage(general, "UI Scale & Theme",
                 [this] { return new UiScalePage(m_model); });
    registerPage(general, "Navigation",
                 [this] { return new NavigationPage(m_model); });
    registerPage(general, "Options", [this]() -> QWidget* {
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
        return genOpts;
    });

    tick("General");

    // ── Hardware ─────────────────────────────────────────────────────────────
    QTreeWidgetItem* hardware = addCategory("Hardware");

    // Task 3.6: ANAN-8000DLE volts/amps toggle — forward signal up to
    // SetupDialog so MainWindow's wireSetupDialog() can connect it to
    // setVoltsAmpsVisible().
    registerPage(hardware, "Hardware Config", [this]() -> QWidget* {
        auto* hwPage = new HardwarePage(m_model);
        connect(hwPage, &HardwarePage::anan8000DleVoltsAmpsChanged,
                this,   &SetupDialog::anan8000DleVoltsAmpsChanged);
        return hwPage;
    });

    // Phase 3F Sub-Epic E Tasks 8-10: DDC Routing power-user override page.
    // Skeleton-only landing; per-DDC table + override schema follow once
    // codec layer (Sub-Epic B) is in place.
    registerPage(hardware, "DDC Routing", [this]() -> QWidget* {
        return new HardwareDdcRoutingPage(m_model);
    });

    // ── PA ────────────────────────────────────────────────────────────────────
    // Top-level PA category mirrors Thetis tpPowerAmplifier
    // (setup.designer.cs:47366-47371 [v2.10.3.13]). Three sub-pages:
    //   - PA Gain         → Thetis tpGainByBand (Phase 6+7 live editor)
    //   - Watt Meter      → Thetis tpWattMeter (cal spinboxes — Phase 3)
    //   - PA Values       → NereusSDR-spin live telemetry page (Phase 4)
    //
    // Phase 8 of #167 — the PA category and 3 sub-pages are now ALWAYS
    // built. Per-SKU visibility is driven dynamically via
    // applyPaVisibility() (called from onCurrentRadioChanged + at end of
    // ctor). The category root is hidden when caps.isRxOnlySku
    // or when !caps.hasPaProfile. Each child page additionally toggles
    // its own informational rows / banners per the BoardCapabilities flags.
    //
    // This replaces the construction-time hasPaProfile gate (which
    // prevented the PA category from appearing on radio-swap when the
    // dialog was already open) with a live capability subscription.
    // From Thetis comboRadioModel_SelectedIndexChanged (setup.cs:19812-20310
    // [v2.10.3.13+501e3f51]) — per-SKU PA tab visibility.
    tick("Hardware");

    m_paCategoryItem  = addCategory("PA");

    // #272 / #301: each PA factory re-applies the live BoardCapabilities to
    // its own page, because applyPaVisibility() ran in the ctor (or on an
    // earlier currentRadioChanged) while the page pointer was still null.
    m_paGainItem = registerPage(m_paCategoryItem, "PA Gain", [this]() -> QWidget* {
        m_paGainPage = new PaGainByBandPage(m_model);
        m_paGainPage->applyCapabilityVisibility(capsForModel(m_model));
        return m_paGainPage;
    });

    m_paWattMeterItem = registerPage(m_paCategoryItem, "Watt Meter",
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
        return m_paWattMeterPage;
    });

    m_paValuesItem = registerPage(m_paCategoryItem, "PA Values", [this]() -> QWidget* {
        m_paValuesPage = new PaValuesPage(m_model);
        m_paValuesPage->applyCapabilityVisibility(capsForModel(m_model));
        return m_paValuesPage;
    });

    // Cache the registry index so the Watt Meter cross-wire above can realize
    // the PA Values page without a label lookup on every button press.
    m_paValuesEntry = m_paValuesItem->data(0, Qt::UserRole).toInt();

    tick("PA");

    // ── Audio ─────────────────────────────────────────────────────────────────
    QTreeWidgetItem* audio = addCategory("Audio");
    registerPage(audio, "Devices",
                 [this] { return wrapWithAudioBackendStrip(new AudioDevicesPage(m_model)); });
    registerPage(audio, "TX Input",  // I.1
                 [this] { return wrapWithAudioBackendStrip(new AudioTxInputPage(m_model)); });
    registerPage(audio, "VAX",
                 [this] { return wrapWithAudioBackendStrip(new AudioVaxPage(m_model)); });
    registerPage(audio, "TCI",
                 [this] { return wrapWithAudioBackendStrip(new AudioTciPage(m_model)); });
    registerPage(audio, "Advanced",
                 [this] { return wrapWithAudioBackendStrip(new AudioAdvancedPage(m_model)); });
    // Phase 3M-1c J.3: TX Profile editor.
    //
    // 3M-1c L.1 update: RadioModel now constructs MicProfileManager in its
    // ctor (per RadioModel::m_micProfileMgr in RadioModel.cpp), so this page
    // gets the live manager pointer at SetupDialog construction time.  The
    // manager itself is per-MAC scoped — setMacAddress + load() run inside
    // RadioModel::connectToRadio().  Before any radio has connected the
    // manager is unscoped and every mutator silently no-ops; the page still
    // renders correctly (combo is empty) and Setup → TX Profile is harmless.
    registerPage(audio, "TX Profile", [this]() -> QWidget* {
        return new TxProfileSetupPage(
            m_model,
            m_model ? m_model->micProfileManager() : nullptr,
            m_model ? &m_model->transmitModel() : nullptr);
    });

    tick("Audio");

    // ── DSP ───────────────────────────────────────────────────────────────────
    QTreeWidgetItem* dsp = addCategory("DSP");
    registerPage(dsp, "AGC/ALC", [this] { return new AgcAlcSetupPage(m_model); });
    registerPage(dsp, "NR/ANF",  [this] { return new NrAnfSetupPage(m_model);  });
    registerPage(dsp, "NB/SNB",  [this] { return new NbSnbSetupPage(m_model);  });
    registerPage(dsp, "CW",      [this] { return new CwSetupPage(m_model);     });
    registerPage(dsp, "AM/SAM",  [this] { return new AmSamSetupPage(m_model);  });
    registerPage(dsp, "FM",      [this] { return new FmSetupPage(m_model);     });
    // (DSP > "VOX/DEXP" placeholder removed in 3M-3a-iii Task 16 — the wired
    //  page lives at Transmit > "DEXP/VOX" (DexpVoxPage from Task 14).)

    // Phase 3M-3a-ii Batch 6 (Task 3): CfcSetupPage's [Configure CFC bands…]
    // button emits openCfcDialogRequested.  Forward up to SetupDialog's
    // cfcDialogRequested signal so MainWindow can route it to the
    // TxApplet::requestOpenCfcDialog() slot (the same modeless dialog
    // instance is shared with the [CFC] right-click on the TxApplet).
    registerPage(dsp, "CFC", [this]() -> QWidget* {
        auto* cfcPage = new CfcSetupPage(m_model);
        connect(cfcPage, &CfcSetupPage::openCfcDialogRequested,
                this,    &SetupDialog::cfcDialogRequested);
        return cfcPage;
    });

    registerPage(dsp, "MNF", [this] { return new MnfSetupPage(m_model); });
    // Stage C2: user-customisable filter preset editor (10 slots × 12 modes).
    registerPage(dsp, "Filter Presets", [this]() -> QWidget* {
        return new FilterPresetsSetupPage(
            m_model ? m_model->filterPresetStore() : nullptr,
            m_model);
    });

    // Task 4.1: DSP → Options page (buffer/filter size+type, impulse cache,
    // high-res filter characteristics, time-to-last-change readout).
    // Mirrors Thetis tpDSPOptions tab (design Section 4A).
    registerPage(dsp, "Options", [this] { return new DspOptionsPage(m_model); });

    tick("DSP");

    // ── Display ───────────────────────────────────────────────────────────────
    QTreeWidgetItem* display = addCategory("Display");

    // Task 2.4: SpectrumDefaultsPage gains cross-link buttons to Spectrum Peaks
    // (and a forward-reference to Multimeter which lands in Task 3.1).
    //
    // #272 / #301: the cross-links go through selectPage(), which drives the
    // nav tree, so the destination page is realized by the tree-selection
    // handler. No sibling-page pointer is needed here.
    registerPage(display, "Spectrum Defaults", [this]() -> QWidget* {
        auto* specDefaultsPage = new SpectrumDefaultsPage(m_model);
        connect(specDefaultsPage, &SpectrumDefaultsPage::navigateToSpectrumPeaksRequested,
                this, [this]() { selectPage(QStringLiteral("Spectrum Peaks")); });
        // Task 3.1: Multimeter page now exists — wire the cross-link.
        connect(specDefaultsPage, &SpectrumDefaultsPage::navigateToMultimeterRequested,
                this, [this]() { selectPage(QStringLiteral("Multimeter")); });
        return specDefaultsPage;
    });

    // Task 2.4: Spectrum Peaks page — skeleton with APH + Blob controls + back cross-link.
    registerPage(display, "Spectrum Peaks", [this]() -> QWidget* {
        auto* specPeaksPage = new SpectrumPeaksPage(m_model);
        connect(specPeaksPage, &SpectrumPeaksPage::backToSpectrumDefaultsRequested,
                this, [this]() { selectPage(QStringLiteral("Spectrum Defaults")); });
        return specPeaksPage;
    });

    registerPage(display, "Waterfall Defaults",
                 [this] { return new WaterfallDefaultsPage(m_model); });
    registerPage(display, "Grid & Scales",
                 [this] { return new GridScalesPage(m_model); });

    // Task 3.1: Display → Multimeter — 8 multimeter globals + unit-mode + signal history.
    // Folded from Thetis Display→General Multimeter group per design Section 3A.
    // Cross-link: ← Spectrum Defaults / SpectrumDefaultsPage → Multimeter.
    registerPage(display, "Multimeter", [this]() -> QWidget* {
        auto* multimeterPage = new MultimeterPage(m_model);
        connect(multimeterPage, &MultimeterPage::backToSpectrumDefaultsRequested,
                this, [this]() { selectPage(QStringLiteral("Spectrum Defaults")); });
        return multimeterPage;
    });

    registerPage(display, "RX2 Display", [this] { return new Rx2DisplayPage(m_model); });
    registerPage(display, "TX Display",  [this] { return new TxDisplayPage(m_model);  });

    tick("Display");

    // ── Transmit ──────────────────────────────────────────────────────────────
    QTreeWidgetItem* transmit = addCategory("Transmit");
    registerPage(transmit, "Power",       [this] { return new PowerPage(m_model);      });
    registerPage(transmit, "TX Profiles", [this] { return new TxProfilesPage(m_model); });

    // SpeechProcessorPage is the TX dashboard (3M-3a-i Batch 5).  Its
    // openSetupRequested(category, page) signal feeds straight back into
    // selectPage() so the cross-link buttons jump within the same dialog
    // instance — no MainWindow round-trip required.
    registerPage(transmit, "Speech Processor", [this]() -> QWidget* {
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
    registerPage(transmit, "DEXP/VOX", [this] { return new DexpVoxPage(m_model); });

    // 2026-05-22 menu cleanup: the standalone "PGXL Interlock" entry that
    // previously lived here is removed. The same controls live under
    // Setup -> CAT & Network -> 4O3A -> General as an embedded section
    // (FourO3APage owns the PgxlInterlockPage instance).

    tick("Transmit");

    // ── Appearance ────────────────────────────────────────────────────────────
    QTreeWidgetItem* appearance = addCategory("Appearance");
    registerPage(appearance, "Colors & Theme",
                 [this] { return new ColorsThemePage(m_model); });
    registerPage(appearance, "Meter Styles",
                 [this] { return new MeterStylesPage(m_model); });
    registerPage(appearance, "Gradients",
                 [this] { return new GradientsPage(m_model); });
    registerPage(appearance, "Skins",
                 [this] { return new SkinsPage(m_model); });
    registerPage(appearance, "Collapsible Display",
                 [this] { return new CollapsibleDisplayPage(m_model); });

    tick("Appearance");

    // ── CAT & Network ─────────────────────────────────────────────────────────
    QTreeWidgetItem* cat = addCategory("CAT & Network");
    registerPage(cat, "Serial Ports", [] { return new CatSerialPortsPage; });
    registerPage(cat, "TCI Server", [this]() -> QWidget* {
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
    registerPage(cat, "4O3A", [this]() -> QWidget* {
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
    registerPage(cat, "RF-Kit",       [this] { return new RfKitPage(m_model); });
    registerPage(cat, "TCP/IP CAT",   [] { return new CatTcpIpPage;       });
    registerPage(cat, "MIDI Control", [] { return new CatMidiControlPage;  });

    tick("CAT & Network");

    // ── Keyboard ──────────────────────────────────────────────────────────────
    QTreeWidgetItem* keyboard = addCategory("Keyboard");
    registerPage(keyboard, "Shortcuts", [] { return new KeyboardShortcutsPage; });

    tick("Keyboard");

    // ── Test ──────────────────────────────────────────────────────────────────
    // Phase 3M-1c H.1: top-level Test category for the Two-Tone IMD page.
    QTreeWidgetItem* test = addCategory("Test");
    registerPage(test, "Two-Tone IMD", [this] { return new TestTwoTonePage(m_model); });

    tick("Test");

    // ── Diagnostics ───────────────────────────────────────────────────────────
    QTreeWidgetItem* diagnostics = addCategory("Diagnostics");
    registerPage(diagnostics, "Radio Status",
                 [this] { return new RadioStatusPage(m_model); });
    registerPage(diagnostics, "Connection Quality",
                 [this] { return new ConnectionQualityPage(m_model); });
    registerPage(diagnostics, "Settings Validation",
                 [this] { return new SettingsValidationPage(m_model); });
    registerPage(diagnostics, "Export / Import",
                 [this] { return new ExportImportConfigPage(m_model); });
    registerPage(diagnostics, "Logs",
                 [] { return new LogsPage; });
    registerPage(diagnostics, "Signal Generator",
                 [] { return new DiagSignalGeneratorPage; });
    registerPage(diagnostics, "Hardware Tests",
                 [] { return new DiagHardwareTestsPage; });
    registerPage(diagnostics, "Logging & Performance",
                 [] { return new DiagLoggingPage; });

    tick("Diagnostics");

    m_tree->expandAll();
    tick("expandAll");
}

// ── Phase 8 of #167: PA category visibility wiring ─────────────────────────────
//
// onCurrentRadioChanged: re-evaluate PA visibility when the connected
// radio changes (radio swap, fresh connect, MAC switch). Forwarded
// from RadioModel::currentRadioChanged.
//
// applyPaVisibility: collapses the per-SKU visibility decisions into
// a single switch. The PA category root is hidden when caps.isRxOnlySku
// (no TX hardware at all) or when !caps.hasPaProfile (the connected
// board has TX but no PA gain calibration support — Atlas, RedPitaya).
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

void SetupDialog::applyPaVisibility(const BoardCapabilities& caps)
{
    // Hide the entire PA category for RX-only SKUs and for boards that
    // lack PA gain calibration support. Hidden via QTreeWidgetItem::
    // setHidden which collapses the row out of the navigation tree
    // entirely (clean visual — no greyed-out unreachable entry).
    const bool paAvailable = !caps.isRxOnlySku && caps.hasPaProfile;

    if (m_paCategoryItem) {
        m_paCategoryItem->setHidden(!paAvailable);
    }
    if (m_paGainItem) {
        m_paGainItem->setHidden(!paAvailable);
    }
    if (m_paWattMeterItem) {
        m_paWattMeterItem->setHidden(!paAvailable);
    }
    if (m_paValuesItem) {
        m_paValuesItem->setHidden(!paAvailable);
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
}

} // namespace NereusSDR
