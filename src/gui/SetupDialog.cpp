#include "SetupDialog.h"
#include "SetupPage.h"
#include "models/RadioModel.h"

// General
#include "setup/GeneralSetupPages.h"
#include "setup/GeneralOptionsPage.h"
// Hardware
#include "setup/HardwarePage.h"
// PA (Setup IA reshape Phase 2 — placeholder pages, content lands in Phase 3+)
#include "setup/PaSetupPages.h"
// Audio
#include "setup/AudioBackendStrip.h"
#include "setup/AudioDevicesPage.h"
#include "setup/AudioTxInputPage.h"
#include "setup/AudioVaxPage.h"
#include "setup/AudioTciPage.h"
#include "setup/AudioAdvancedPage.h"
// DSP
#include "setup/DspSetupPages.h"
#include "setup/FilterPresetsSetupPage.h"
// Display
#include "setup/DisplaySetupPages.h"
// Transmit
#include "setup/TransmitSetupPages.h"
// Appearance
#include "setup/AppearanceSetupPages.h"
// CAT & Network
#include "setup/CatNetworkSetupPages.h"
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

namespace NereusSDR {

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

    // ── Build the tree and pages ──────────────────────────────────────────────
    buildTree();

    // Connect tree selection → stack page
    connect(m_tree, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem* current, QTreeWidgetItem* /*previous*/) {
                if (current == nullptr) { return; }
                const int index = current->data(0, Qt::UserRole).toInt();
                if (index >= 0) {
                    m_stack->setCurrentIndex(index);
                }
            });
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

// ── Tree builder ──────────────────────────────────────────────────────────────

void SetupDialog::buildTree()
{
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

    // ── Helper: add a page with its specialized class ─────────────────────────
    auto add = [this](QTreeWidgetItem* parent, const QString& label,
                      SetupPage* page) -> QTreeWidgetItem* {
        auto* item = new QTreeWidgetItem(parent, QStringList{label});
        const int idx = m_stack->count();
        item->setData(0, Qt::UserRole, idx);
        m_stack->addWidget(page);
        return item;
    };

    // ── Helper: wrap a SetupPage with an AudioBackendStrip header ─────────────
    // Returns a margin-less container QWidget that owns both the strip and the
    // page. Qt parent-ownership keeps memory clean — the container is reparented
    // into the QStackedWidget by addWrapped below.
    auto wrapWithAudioBackendStrip = [this](SetupPage* page) -> QWidget* {
        auto* container = new QWidget;
        auto* lay = new QVBoxLayout(container);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(0);
        lay->addWidget(new AudioBackendStrip(m_model->audioEngine(), container));
        lay->addWidget(page);
        return container;
    };

    // ── Helper: add a wrapped (plain QWidget) page to the tree ────────────────
    auto addWrapped = [this](QTreeWidgetItem* parent, const QString& label,
                             QWidget* container) -> QTreeWidgetItem* {
        auto* item = new QTreeWidgetItem(parent, QStringList{label});
        const int idx = m_stack->count();
        item->setData(0, Qt::UserRole, idx);
        m_stack->addWidget(container);
        return item;
    };

    // ── General ──────────────────────────────────────────────────────────────
    QTreeWidgetItem* general = addCategory("General");
    add(general, "Startup & Preferences", new StartupPrefsPage(m_model));
    add(general, "UI Scale & Theme",       new UiScalePage(m_model));
    add(general, "Navigation",             new NavigationPage(m_model));
    add(general, "Options",                new GeneralOptionsPage(m_model));

    // ── Hardware ─────────────────────────────────────────────────────────────
    QTreeWidgetItem* hardware = addCategory("Hardware");
    add(hardware, "Hardware Config", new HardwarePage(m_model));

    // ── PA ────────────────────────────────────────────────────────────────────
    // Top-level PA category mirrors Thetis tpPowerAmplifier
    // (setup.designer.cs:47366-47371 [v2.10.3.13]). Three sub-pages:
    //   - PA Gain         → Thetis tpGainByBand (placeholder for Phase 3M-3)
    //   - Watt Meter      → Thetis tpWattMeter (cal spinboxes — Phase 3)
    //   - PA Values       → NereusSDR-spin live telemetry page (Phase 4)
    //
    // Visibility gate: only shown when the connected radio reports
    // hasPaProfile=true. RX-only kits and Atlas (no integrated PA) hide
    // the entire category. Mirrors the access pattern used by HardwarePage
    // (HardwarePage.cpp:219 [v2.10.3.13]) — caps is a const pointer on
    // HardwareProfile so the null guard is mandatory.
    if (m_model && m_model->hardwareProfile().caps
        && m_model->hardwareProfile().caps->hasPaProfile) {
        QTreeWidgetItem* pa = addCategory("PA");
        add(pa, "PA Gain",    new PaGainByBandPage(m_model));
        add(pa, "Watt Meter", new PaWattMeterPage(m_model));
        add(pa, "PA Values",  new PaValuesPage(m_model));
    }

    // ── Audio ─────────────────────────────────────────────────────────────────
    QTreeWidgetItem* audio = addCategory("Audio");
    addWrapped(audio, "Devices",  wrapWithAudioBackendStrip(new AudioDevicesPage(m_model)));
    addWrapped(audio, "TX Input", wrapWithAudioBackendStrip(new AudioTxInputPage(m_model)));  // I.1
    addWrapped(audio, "VAX",      wrapWithAudioBackendStrip(new AudioVaxPage(m_model)));
    addWrapped(audio, "TCI",      wrapWithAudioBackendStrip(new AudioTciPage(m_model)));
    addWrapped(audio, "Advanced", wrapWithAudioBackendStrip(new AudioAdvancedPage(m_model)));
    // Phase 3M-1c J.3: TX Profile editor.
    //
    // 3M-1c L.1 update: RadioModel now constructs MicProfileManager in its
    // ctor (per RadioModel::m_micProfileMgr in RadioModel.cpp), so this page
    // gets the live manager pointer at SetupDialog construction time.  The
    // manager itself is per-MAC scoped — setMacAddress + load() run inside
    // RadioModel::connectToRadio().  Before any radio has connected the
    // manager is unscoped and every mutator silently no-ops; the page still
    // renders correctly (combo is empty) and Setup → TX Profile is harmless.
    add(audio, "TX Profile",
        new TxProfileSetupPage(
            m_model,
            m_model ? m_model->micProfileManager() : nullptr,
            m_model ? &m_model->transmitModel() : nullptr));

    // ── DSP ───────────────────────────────────────────────────────────────────
    QTreeWidgetItem* dsp = addCategory("DSP");
    add(dsp, "AGC/ALC",  new AgcAlcSetupPage(m_model));
    add(dsp, "NR/ANF",   new NrAnfSetupPage(m_model));
    add(dsp, "NB/SNB",   new NbSnbSetupPage(m_model));
    add(dsp, "CW",       new CwSetupPage(m_model));
    add(dsp, "AM/SAM",   new AmSamSetupPage(m_model));
    add(dsp, "FM",       new FmSetupPage(m_model));
    add(dsp, "VOX/DEXP", new VoxDexpSetupPage(m_model));

    // Phase 3M-3a-ii Batch 6 (Task 3): CfcSetupPage's [Configure CFC bands…]
    // button emits openCfcDialogRequested.  Forward up to SetupDialog's
    // cfcDialogRequested signal so MainWindow can route it to the
    // TxApplet::requestOpenCfcDialog() slot (the same modeless dialog
    // instance is shared with the [CFC] right-click on the TxApplet).
    auto* cfcPage = new CfcSetupPage(m_model);
    add(dsp, "CFC", cfcPage);
    connect(cfcPage, &CfcSetupPage::openCfcDialogRequested,
            this,    &SetupDialog::cfcDialogRequested);

    add(dsp, "MNF",      new MnfSetupPage(m_model));
    // Stage C2: user-customisable filter preset editor (10 slots × 12 modes).
    add(dsp, "Filter Presets",
        new FilterPresetsSetupPage(
            m_model ? m_model->filterPresetStore() : nullptr,
            m_model));

    // ── Display ───────────────────────────────────────────────────────────────
    QTreeWidgetItem* display = addCategory("Display");
    add(display, "Spectrum Defaults",  new SpectrumDefaultsPage(m_model));
    add(display, "Waterfall Defaults", new WaterfallDefaultsPage(m_model));
    add(display, "Grid & Scales",      new GridScalesPage(m_model));
    add(display, "RX2 Display",        new Rx2DisplayPage(m_model));
    add(display, "TX Display",         new TxDisplayPage(m_model));

    // ── Transmit ──────────────────────────────────────────────────────────────
    QTreeWidgetItem* transmit = addCategory("Transmit");
    add(transmit, "Power",              new PowerPage(m_model));
    add(transmit, "TX Profiles",        new TxProfilesPage(m_model));

    // SpeechProcessorPage is the TX dashboard (3M-3a-i Batch 5).  Its
    // openSetupRequested(category, page) signal feeds straight back into
    // selectPage() so the cross-link buttons jump within the same dialog
    // instance — no MainWindow round-trip required.
    auto* speechPage = new SpeechProcessorPage(m_model);
    add(transmit, "Speech Processor",   speechPage);
    connect(speechPage, &SpeechProcessorPage::openSetupRequested,
            this, [this](const QString& /*category*/, const QString& page) {
        selectPage(page);
    });

    add(transmit, "PureSignal",         new PureSignalPage(m_model));

    // ── Appearance ────────────────────────────────────────────────────────────
    QTreeWidgetItem* appearance = addCategory("Appearance");
    add(appearance, "Colors & Theme",       new ColorsThemePage(m_model));
    add(appearance, "Meter Styles",         new MeterStylesPage(m_model));
    add(appearance, "Gradients",            new GradientsPage(m_model));
    add(appearance, "Skins",               new SkinsPage(m_model));
    add(appearance, "Collapsible Display",  new CollapsibleDisplayPage(m_model));

    // ── CAT & Network ─────────────────────────────────────────────────────────
    QTreeWidgetItem* cat = addCategory("CAT & Network");
    add(cat, "Serial Ports", new CatSerialPortsPage);
    add(cat, "TCI Server",   new CatTciServerPage);
    add(cat, "TCP/IP CAT",   new CatTcpIpPage);
    add(cat, "MIDI Control", new CatMidiControlPage);

    // ── Keyboard ──────────────────────────────────────────────────────────────
    QTreeWidgetItem* keyboard = addCategory("Keyboard");
    add(keyboard, "Shortcuts", new KeyboardShortcutsPage);

    // ── Test ──────────────────────────────────────────────────────────────────
    // Phase 3M-1c H.1: top-level Test category for the Two-Tone IMD page.
    QTreeWidgetItem* test = addCategory("Test");
    add(test, "Two-Tone IMD", new TestTwoTonePage(m_model));

    // ── Diagnostics ───────────────────────────────────────────────────────────
    QTreeWidgetItem* diagnostics = addCategory("Diagnostics");
    add(diagnostics, "Radio Status",       new RadioStatusPage(m_model));
    add(diagnostics, "Connection Quality", new ConnectionQualityPage(m_model));
    add(diagnostics, "Settings Validation",new SettingsValidationPage(m_model));
    add(diagnostics, "Export / Import",    new ExportImportConfigPage(m_model));
    add(diagnostics, "Logs",               new LogsPage);
    add(diagnostics, "Signal Generator",   new DiagSignalGeneratorPage);
    add(diagnostics, "Hardware Tests",     new DiagHardwareTestsPage);
    add(diagnostics, "Logging",            new DiagLoggingPage);

    m_tree->expandAll();
}

} // namespace NereusSDR
