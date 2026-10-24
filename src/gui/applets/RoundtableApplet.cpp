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
#include "models/RadioModel.h"
#include "models/RoundtableModel.h"
#include "models/SliceModel.h"

#include <QDesktopServices>
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
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_panel = new RoundtablePanel(m_roundtable, this);
    m_panel->setMinimumHeight(360);
    layout->addWidget(m_panel);

    connect(m_panel, &RoundtablePanel::tuneRequested, this, &RoundtableApplet::tuneHere);
    connect(m_panel, &RoundtablePanel::archiveRequested, this, &RoundtableApplet::openArchive);
    connect(m_panel, &RoundtablePanel::settingsRequested, this, &RoundtableApplet::settingsRequested);
    connect(m_panel, &RoundtablePanel::popOutRequested, this, [this]() {
        // Through the panel that holds it, so its float state is kept.
        for (QWidget* up = parentWidget(); up != nullptr; up = up->parentWidget()) {
            if (auto* host = qobject_cast<AppletPanelWidget*>(up)) {
                host->floatApplet(this);
                return;
            }
        }
    });
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

void RoundtableApplet::openArchive()
{
    if (!m_roundtable) {
        return;
    }
    m_webLinkPending = true;
    m_roundtable->requestWebLink();
}

} // namespace NereusSDR
