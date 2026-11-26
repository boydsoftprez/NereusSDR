// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/applets/RoundtableControlsApplet.cpp  (NereusSDR)
// =================================================================
//
// See RoundtableControlsApplet.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-25: Created. Brian Sutton (W4KZO), AI-assisted via xAI Grok.
//   2026-11-25: Roundtable, Start/Stop and Hide your status share one line,
//               with a gap between the button and the checkbox. Brian Sutton
//               (W4KZO), AI-assisted via xAI Grok.
//   2026-11-26: Start and Hide your status grey with a reason as Spot Hub's
//               do (setStationAvailable), and a reason line under the row
//               shows the spot source host's Roundtable text and refusals.
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code / Opus 5.5.
// =================================================================

#include "RoundtableControlsApplet.h"

#include "core/SpotSourceHost.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace NereusSDR {

RoundtableControlsApplet::RoundtableControlsApplet(RadioModel* model, QWidget* parent)
    : RoundtableControlsApplet(model, model != nullptr ? model->roundtableModel() : nullptr, parent)
{
}

RoundtableControlsApplet::RoundtableControlsApplet(RadioModel* model, RoundtableModel* roundtable,
                                                   QWidget* parent)
    : AppletWidget(model, parent)
    , m_roundtable(roundtable)
{
    build();
    if (m_roundtable) {
        connect(m_roundtable, &RoundtableModel::linkChanged, this, [this] {
            // Running again: a refused start is old news.
            if (running()) {
                m_refusal.clear();
            }
            refresh();
        });
        connect(m_roundtable, &RoundtableModel::actionRefused, this,
                [this](const QString& verb, const QString& reason) {
                    if (verb == QLatin1String("setHidden")) {
                        m_refusal = reason;
                        refresh();
                    }
                });
    }
    refresh();
}

RoundtableControlsApplet::~RoundtableControlsApplet() = default;

void RoundtableControlsApplet::build()
{
    setObjectName(QStringLiteral("roundtableControlsApplet"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 6, 8, 6);
    outer->setSpacing(4);
    auto* layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    outer->addLayout(layout);

    auto* name = new QLabel(QStringLiteral("Roundtable"), this);
    name->setObjectName(QStringLiteral("roundtableControlsLabel"));
    layout->addWidget(name);

    m_start = new QPushButton(QStringLiteral("Start"), this);
    m_start->setObjectName(QStringLiteral("roundtableControlsStartBtn"));
    m_start->setFixedWidth(72);
    m_start->setAutoDefault(false);
    connect(m_start, &QPushButton::clicked, this, [this] {
        m_refusal.clear();
        if (running()) {
            emit stopRequested();
        } else {
            emit startRequested();
        }
    });
    layout->addWidget(m_start);

    // Wider than the row spacing, so Stop and Hide your status read as two controls.
    layout->addSpacing(28);

    m_hide = new QCheckBox(QStringLiteral("Hide your status"), this);
    m_hide->setObjectName(QStringLiteral("roundtableControlsHideChk"));
    connect(m_hide, &QCheckBox::toggled, this, [this](bool on) {
        m_refusal.clear();
        if (m_roundtable) {
            m_roundtable->setHidden(on);
        }
    });
    layout->addWidget(m_hide);
    layout->addStretch(1);

    // Why Roundtable did not start, or why a hide was refused, as Spot
    // Hub's Roundtable status says it. Hidden while there is nothing to say.
    m_reason = new QLabel(this);
    m_reason->setObjectName(QStringLiteral("roundtableControlsReason"));
    m_reason->setWordWrap(true);
    m_reason->setStyleSheet(QStringLiteral("QLabel { color: #e6c200; font-size: 11px; }"));
    m_reason->hide();
    outer->addWidget(m_reason);
}

void RoundtableControlsApplet::setStationAvailable(bool available, const QString& reason)
{
    m_available = available;
    m_unavailableReason = reason;
    refresh();
}

void RoundtableControlsApplet::setSourceHost(SpotSourceHost* host)
{
    if (m_host) {
        disconnect(m_host, nullptr, this, nullptr);
    }
    m_host = host;
    if (m_host) {
        connect(m_host, &SpotSourceHost::sourceChanged, this, [this](const QString& source) {
            if (source == SpotSourceHost::kRoundtable) {
                refresh();
            }
        });
        connect(m_host, &SpotSourceHost::sourceRefused, this,
                [this](const QString& source, const QString& reason) {
            if (source == SpotSourceHost::kRoundtable) {
                m_refusal = reason;
                refresh();
            }
        });
    }
    refresh();
}

bool RoundtableControlsApplet::running() const
{
    return m_roundtable && m_roundtable->linkState() != RoundtableModel::LinkState::Off;
}

void RoundtableControlsApplet::refresh()
{
    const bool on = running();
    m_start->setText(on ? QStringLiteral("Stop") : QStringLiteral("Start"));
    m_start->setAccessibleName(on ? QStringLiteral("Stop Roundtable")
                                  : QStringLiteral("Start Roundtable"));
    // Greyed with the reason as their tooltip, as Spot Hub's Roundtable
    // tab greys its Start and Hide (SpotHubDialog::applyRoundtableAvailability).
    m_start->setEnabled(m_available);
    m_start->setToolTip(m_available ? QString() : m_unavailableReason);
    m_hide->setEnabled(m_available && m_roundtable != nullptr);
    m_hide->setToolTip(m_available ? QString() : m_unavailableReason);
    {
        const QSignalBlocker block(m_hide);
        m_hide->setChecked(m_roundtable && m_roundtable->hidden());
    }

    // The last refusal, else the host's text: why Roundtable stopped (Spot
    // Hub's Stopped line). Greyed controls say why in their tooltip.
    QString why = m_refusal;
    if (why.isEmpty() && !on && m_host) {
        why = m_host->text(SpotSourceHost::kRoundtable);
    }
    if (!m_available) {
        why.clear();
    }
    m_reason->setText(why);
    m_reason->setVisible(!why.isEmpty());
}

void RoundtableControlsApplet::syncFromModel()
{
    refresh();
}

} // namespace NereusSDR
