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
// =================================================================

#include "RoundtableControlsApplet.h"

#include "models/RadioModel.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>

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
        connect(m_roundtable, &RoundtableModel::linkChanged, this, [this] { refresh(); });
        connect(m_roundtable, &RoundtableModel::actionRefused, this,
                [this](const QString& verb, const QString&) {
                    if (verb == QLatin1String("setHidden")) {
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

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(8);

    auto* name = new QLabel(QStringLiteral("Roundtable"), this);
    name->setObjectName(QStringLiteral("roundtableControlsLabel"));
    layout->addWidget(name);

    m_start = new QPushButton(QStringLiteral("Start"), this);
    m_start->setObjectName(QStringLiteral("roundtableControlsStartBtn"));
    m_start->setFixedWidth(72);
    m_start->setAutoDefault(false);
    connect(m_start, &QPushButton::clicked, this, [this] {
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
        if (m_roundtable) {
            m_roundtable->setHidden(on);
        }
    });
    layout->addWidget(m_hide);
    layout->addStretch(1);
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
    m_hide->setEnabled(m_roundtable != nullptr);
    const QSignalBlocker block(m_hide);
    m_hide->setChecked(m_roundtable && m_roundtable->hidden());
}

void RoundtableControlsApplet::syncFromModel()
{
    refresh();
}

} // namespace NereusSDR
