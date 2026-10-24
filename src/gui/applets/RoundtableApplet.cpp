// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/applets/RoundtableApplet.cpp  (NereusSDR)
// =================================================================
//
// See RoundtableApplet.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 13).
// =================================================================

#include "RoundtableApplet.h"

#include "AppletPanelWidget.h"
#include "core/WdspTypes.h"
#include "gui/roundtable/RoundtablePanel.h"
#include "gui/roundtable/RoundtableWindow.h"
#include "models/RadioModel.h"
#include "models/RoundtableModel.h"
#include "models/SliceModel.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <cmath>

namespace NereusSDR {

namespace {

// The room family a slice's mode is in (wire spec 6.3's families).
QString familyOf(DSPMode mode)
{
    switch (mode) {
    case DSPMode::LSB:
        return QStringLiteral("lsb");
    case DSPMode::USB:
        return QStringLiteral("usb");
    case DSPMode::DIGL:
        return QStringLiteral("digl");
    case DSPMode::DIGU:
        return QStringLiteral("digu");
    case DSPMode::AM:
    case DSPMode::SAM:
        return QStringLiteral("am");
    case DSPMode::FM:
        return QStringLiteral("fm");
    case DSPMode::CWL:
    case DSPMode::CWU:
        return QStringLiteral("cw");
    case DSPMode::RADE_U:
    case DSPMode::RADE2_U:
        return QStringLiteral("fdvu");
    case DSPMode::RADE_L:
    case DSPMode::RADE2_L:
        return QStringLiteral("fdvl");
    default:
        return {};
    }
}

} // namespace

RoundtableApplet::RoundtableApplet(RadioModel* model, QWidget* parent)
    : RoundtableApplet(model, model != nullptr ? model->roundtableModel() : nullptr, parent)
{
}

RoundtableApplet::RoundtableApplet(RadioModel* model, RoundtableModel* roundtable, QWidget* parent)
    : AppletWidget(model, parent)
    , m_roundtable(roundtable)
{
    build();
    if (m_model != nullptr) {
        connect(m_model, &RadioModel::activeSliceChanged, this, [this](int) { bindActiveSlice(); });
    }
    bindActiveSlice();
}

RoundtableApplet::~RoundtableApplet() = default;

void RoundtableApplet::build()
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    m_panel = new RoundtablePanel(m_roundtable, this);
    m_panel->setMinimumHeight(360);
    m_layout->addWidget(m_panel);

    // Where the panel was while it is in its own window (mockup
    // .poppedNote).
    m_poppedNote = new QWidget(this);
    m_poppedNote->setObjectName(QStringLiteral("roundtablePoppedNote"));
    auto* note = new QHBoxLayout(m_poppedNote);
    note->setContentsMargins(8, 8, 8, 8);
    note->setSpacing(4);
    auto* text = new QLabel(QStringLiteral("Roundtable is in its own window."), m_poppedNote);
    text->setStyleSheet(QStringLiteral("color: #a6a6ac; font-size: 11px;"));
    note->addWidget(text);
    auto* back = new QPushButton(QStringLiteral("Put it back here"), m_poppedNote);
    back->setObjectName(QStringLiteral("roundtablePutBackBtn"));
    back->setCursor(Qt::PointingHandCursor);
    back->setStyleSheet(QStringLiteral("QPushButton { background: transparent; border: none; color: #00b4d8;"
                                       " font-size: 11px; padding: 0; }"
                                       "QPushButton:hover { color: #ffffff; }"));
    connect(back, &QPushButton::clicked, this, &RoundtableApplet::putBack);
    note->addWidget(back);
    note->addStretch(1);
    m_poppedNote->hide();
    m_layout->addWidget(m_poppedNote);

    connect(m_panel, &RoundtablePanel::tuneRequested, this, &RoundtableApplet::tuneHere);
    connect(m_panel, &RoundtablePanel::archiveRequested, this, &RoundtableApplet::openArchive);
    connect(m_panel, &RoundtablePanel::settingsRequested, this, &RoundtableApplet::settingsRequested);
    connect(m_panel, &RoundtablePanel::popOutRequested, this, &RoundtableApplet::popOut);
    if (m_roundtable) {
        connect(m_roundtable, &RoundtableModel::webLinkReady, this, [this](const QUrl& url) {
            if (!m_webLinkPending) {
                return;
            }
            m_webLinkPending = false;
            if (m_urlOpener) {
                m_urlOpener(url);
            } else {
                QDesktopServices::openUrl(url);
            }
        });
        connect(m_roundtable, &RoundtableModel::actionRefused, this, [this](const QString& verb) {
            if (verb == QLatin1String("webSignIn")) {
                m_webLinkPending = false;
            }
        });
    }
}

void RoundtableApplet::syncFromModel()
{
    bindActiveSlice();
}

void RoundtableApplet::bindActiveSlice()
{
    SliceModel* slice = m_model != nullptr ? m_model->activeSlice() : nullptr;
    if (slice == m_slice && slice != nullptr) {
        return;
    }
    disconnect(m_sliceFrequency);
    m_slice = slice;
    if (slice == nullptr) {
        m_panel->followSlice(-1);
        m_panel->setSliceFrequency(0);
        return;
    }
    // Tuning the slice anywhere goes back to following it (design 11.1).
    m_sliceFrequency = connect(slice, &SliceModel::frequencyChanged, this, [this](double hz) {
        m_panel->setSliceFrequency(std::llround(hz));
        m_panel->backToSlice();
    });
    m_panel->followSlice(slice->sliceIndex());
    m_panel->setSliceFrequency(std::llround(slice->frequency()));
}

void RoundtableApplet::tuneHere(qint64 hz, const QString& family)
{
    SliceModel* slice = m_slice;
    if (slice == nullptr || hz <= 0) {
        return;
    }
    // A slice already in the room's family keeps its mode (CWL stays CWL,
    // SAM stays SAM); otherwise the family's mode. FreeDV rooms move the
    // frequency only: which FreeDV mode is the operator's.
    if (familyOf(slice->dspMode()) != family) {
        const QList<std::pair<QString, DSPMode>> modes{
            {QStringLiteral("lsb"), DSPMode::LSB},   {QStringLiteral("usb"), DSPMode::USB},
            {QStringLiteral("digl"), DSPMode::DIGL}, {QStringLiteral("digu"), DSPMode::DIGU},
            {QStringLiteral("am"), DSPMode::AM},     {QStringLiteral("fm"), DSPMode::FM},
            {QStringLiteral("cw"), DSPMode::CWU}};
        for (const auto& [name, mode] : modes) {
            if (name == family) {
                slice->setDspMode(mode);
            }
        }
    }
    slice->setFrequency(static_cast<double>(hz));
    m_panel->backToSlice();
}

AppletPanelWidget* RoundtableApplet::host() const
{
    for (QWidget* up = parentWidget(); up != nullptr; up = up->parentWidget()) {
        if (auto* panelHost = qobject_cast<AppletPanelWidget*>(up)) {
            return panelHost;
        }
    }
    return nullptr;
}

bool RoundtableApplet::isPoppedOut() const
{
    return m_window != nullptr && m_window->panel() == m_panel;
}

void RoundtableApplet::popOut()
{
    // Through the panel that holds it, so it knows; on its own, directly.
    if (AppletPanelWidget* panelHost = host()) {
        panelHost->floatApplet(this);
        return;
    }
    if (QWidget* own = createFloatingWindow()) {
        own->show();
    }
}

void RoundtableApplet::putBack()
{
    if (AppletPanelWidget* panelHost = host(); panelHost != nullptr && panelHost->isAppletFloating(this)) {
        panelHost->dockApplet(this);
        return;
    }
    dockFloatingWindow();
}

QWidget* RoundtableApplet::createFloatingWindow()
{
    if (!m_window) {
        m_window = new RoundtableWindow(m_roundtable, window());
        // A close box docks the panel back.
        connect(m_window, &RoundtableWindow::dockRequested, this, &RoundtableApplet::putBack);
    }
    if (!isPoppedOut()) {
        m_layout->removeWidget(m_panel);
        m_window->takePanel(m_panel);
        m_poppedNote->show();
        m_window->showAtSavedGeometry();
    }
    return m_window;
}

void RoundtableApplet::dockFloatingWindow()
{
    if (!isPoppedOut()) {
        return;
    }
    RoundtablePanel* panel = m_window->releasePanel();
    m_window->hide();
    panel->setParent(this);
    m_layout->insertWidget(0, panel);
    panel->show();
    m_poppedNote->hide();
    emit floatingWindowDocked();
}

void RoundtableApplet::openArchive()
{
    if (!m_roundtable) {
        return;
    }
    m_webLinkPending = true;
    m_roundtable->requestWebLink();
}

} // namespace NereusSDR
