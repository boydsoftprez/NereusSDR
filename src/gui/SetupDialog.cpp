#include "SetupDialog.h"
#include "SetupPage.h"

// General
#include "setup/GeneralSetupPages.h"
#include "setup/GeneralOptionsPage.h"
// Hardware
#include "setup/HardwarePage.h"
// Audio
#include "setup/AudioSetupPages.h"
// DSP
#include "setup/DspSetupPages.h"
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

    // ── General ──────────────────────────────────────────────────────────────
    QTreeWidgetItem* general = addCategory("General");
    add(general, "Startup & Preferences", new StartupPrefsPage(m_model));
    add(general, "UI Scale & Theme",       new UiScalePage(m_model));
    add(general, "Navigation",             new NavigationPage(m_model));
    add(general, "Options",                new GeneralOptionsPage(m_model));

    // ── Hardware ─────────────────────────────────────────────────────────────
    QTreeWidgetItem* hardware = addCategory("Hardware");
    add(hardware, "Hardware Config", new HardwarePage(m_model));

    // ── Audio ─────────────────────────────────────────────────────────────────
    QTreeWidgetItem* audio = addCategory("Audio");
    add(audio, "Device Selection", new DeviceSelectionPage(m_model));
    add(audio, "ASIO Config",      new AsioConfigPage(m_model));
    add(audio, "VAC 1",            new Vac1Page(m_model));
    add(audio, "VAC 2",            new Vac2Page(m_model));
    add(audio, "NereusVAX",        new NereusVaxPage(m_model));
    add(audio, "Recording",        new RecordingPage(m_model));

    // ── DSP ───────────────────────────────────────────────────────────────────
    QTreeWidgetItem* dsp = addCategory("DSP");
    add(dsp, "AGC/ALC",  new AgcAlcSetupPage(m_model));
    add(dsp, "NR/ANF",   new NrAnfSetupPage(m_model));
    add(dsp, "NB/SNB",   new NbSnbSetupPage(m_model));
    add(dsp, "CW",       new CwSetupPage(m_model));
    add(dsp, "AM/SAM",   new AmSamSetupPage(m_model));
    add(dsp, "FM",       new FmSetupPage(m_model));
    add(dsp, "VOX/DEXP", new VoxDexpSetupPage(m_model));
    add(dsp, "CFC",      new CfcSetupPage(m_model));
    add(dsp, "MNF",      new MnfSetupPage(m_model));

    // ── Display ───────────────────────────────────────────────────────────────
    QTreeWidgetItem* display = addCategory("Display");
    add(display, "Spectrum Defaults",  new SpectrumDefaultsPage(m_model));
    add(display, "Waterfall Defaults", new WaterfallDefaultsPage(m_model));
    add(display, "Grid & Scales",      new GridScalesPage(m_model));
    add(display, "RX2 Display",        new Rx2DisplayPage(m_model));
    add(display, "TX Display",         new TxDisplayPage(m_model));

    // ── Transmit ──────────────────────────────────────────────────────────────
    QTreeWidgetItem* transmit = addCategory("Transmit");
    add(transmit, "Power & PA",         new PowerPaPage(m_model));
    add(transmit, "TX Profiles",        new TxProfilesPage(m_model));
    add(transmit, "Speech Processor",   new SpeechProcessorPage(m_model));
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

    // ── Diagnostics ───────────────────────────────────────────────────────────
    QTreeWidgetItem* diagnostics = addCategory("Diagnostics");
    add(diagnostics, "Signal Generator", new DiagSignalGeneratorPage);
    add(diagnostics, "Hardware Tests",   new DiagHardwareTestsPage);
    add(diagnostics, "Logging",          new DiagLoggingPage);

    m_tree->expandAll();
}

} // namespace NereusSDR
