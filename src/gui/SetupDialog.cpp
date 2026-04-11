#include "SetupDialog.h"
#include "SetupPage.h"

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

    // Select the first leaf item so the stack shows a valid page.
    QTreeWidgetItem* first = m_tree->topLevelItem(0);
    if (first != nullptr && first->childCount() > 0) {
        first = first->child(0);
    }
    if (first != nullptr) {
        m_tree->setCurrentItem(first);
    }
}

// ── Tree builder ──────────────────────────────────────────────────────────────

QTreeWidgetItem* SetupDialog::addPage(QTreeWidgetItem* parent,
                                      const QString& label,
                                      RadioModel* model)
{
    auto* item = new QTreeWidgetItem(parent, QStringList{label});
    const int pageIndex = m_stack->count();
    item->setData(0, Qt::UserRole, pageIndex);

    auto* page = new SetupPage(label, model);
    m_stack->addWidget(page);

    return item;
}

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

    // ── General ──────────────────────────────────────────────────────────────
    QTreeWidgetItem* general = addCategory("General");
    addPage(general, "Startup & Preferences", m_model);
    addPage(general, "UI Scale & Theme",       m_model);
    addPage(general, "Navigation",             m_model);

    // ── Hardware ─────────────────────────────────────────────────────────────
    QTreeWidgetItem* hardware = addCategory("Hardware");
    addPage(hardware, "Radio Selection",     m_model);
    addPage(hardware, "ADC/DDC Config",      m_model);
    addPage(hardware, "Calibration",         m_model);
    addPage(hardware, "Alex Filters",        m_model);
    addPage(hardware, "Penny/Hermes OC",     m_model);
    addPage(hardware, "Firmware",            m_model);
    addPage(hardware, "Other H/W",           m_model);

    // ── Audio ─────────────────────────────────────────────────────────────────
    QTreeWidgetItem* audio = addCategory("Audio");
    addPage(audio, "Device Selection", m_model);
    addPage(audio, "ASIO Config",      m_model);
    addPage(audio, "VAC 1",            m_model);
    addPage(audio, "VAC 2",            m_model);
    addPage(audio, "NereusDAX",        m_model);
    addPage(audio, "Recording",        m_model);

    // ── DSP ───────────────────────────────────────────────────────────────────
    QTreeWidgetItem* dsp = addCategory("DSP");
    addPage(dsp, "AGC/ALC",  m_model);
    addPage(dsp, "NR/ANF",   m_model);
    addPage(dsp, "NB/SNB",   m_model);
    addPage(dsp, "CW",       m_model);
    addPage(dsp, "AM/SAM",   m_model);
    addPage(dsp, "FM",       m_model);
    addPage(dsp, "VOX/DEXP", m_model);
    addPage(dsp, "CFC",      m_model);
    addPage(dsp, "MNF",      m_model);

    // ── Display ───────────────────────────────────────────────────────────────
    QTreeWidgetItem* display = addCategory("Display");
    addPage(display, "Spectrum Defaults",  m_model);
    addPage(display, "Waterfall Defaults", m_model);
    addPage(display, "Grid & Scales",      m_model);
    addPage(display, "RX2 Display",        m_model);
    addPage(display, "TX Display",         m_model);

    // ── Transmit ──────────────────────────────────────────────────────────────
    QTreeWidgetItem* transmit = addCategory("Transmit");
    addPage(transmit, "Power & PA",         m_model);
    addPage(transmit, "TX Profiles",        m_model);
    addPage(transmit, "Speech Processor",   m_model);
    addPage(transmit, "PureSignal",         m_model);

    // ── Appearance ────────────────────────────────────────────────────────────
    QTreeWidgetItem* appearance = addCategory("Appearance");
    addPage(appearance, "Colors & Theme",       m_model);
    addPage(appearance, "Meter Styles",         m_model);
    addPage(appearance, "Gradients",            m_model);
    addPage(appearance, "Skins",                m_model);
    addPage(appearance, "Collapsible Display",  m_model);

    // ── CAT & Network ─────────────────────────────────────────────────────────
    QTreeWidgetItem* cat = addCategory("CAT & Network");
    addPage(cat, "Serial Ports", m_model);
    addPage(cat, "TCI Server",   m_model);
    addPage(cat, "TCP/IP CAT",   m_model);
    addPage(cat, "MIDI Control", m_model);

    // ── Keyboard ──────────────────────────────────────────────────────────────
    QTreeWidgetItem* keyboard = addCategory("Keyboard");
    addPage(keyboard, "Shortcuts", m_model);

    // ── Diagnostics ───────────────────────────────────────────────────────────
    QTreeWidgetItem* diagnostics = addCategory("Diagnostics");
    addPage(diagnostics, "Signal Generator", m_model);
    addPage(diagnostics, "Hardware Tests",   m_model);
    addPage(diagnostics, "Logging",          m_model);

    m_tree->expandAll();
}

} // namespace NereusSDR
