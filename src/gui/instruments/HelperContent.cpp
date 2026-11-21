// no-port-check: NereusSDR-original WaveNode / HF-Auto container panels.
#include "HelperContent.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "core/mmio/ExternalVariableEngine.h"
#include "core/mmio/MmioEndpoint.h"
#include "gui/HelperApps.h"
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace NereusSDR {
namespace {

class HelperPanel : public QWidget {
    Q_OBJECT
public:
    HelperPanel(HelperApps::Helper helper, QWidget* parent, bool preview)
        : QWidget(parent), m_helper(helper), m_preview(preview)
    {
        const bool wave = helper == HelperApps::Helper::WaveNode;
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(6, 6, 6, 6);
        layout->setSpacing(4);

        auto* title = new QLabel(wave ? tr("WaveNode") : tr("HF-Auto"), this);
        QFont titleFont = title->font();
        titleFont.setBold(true);
        title->setFont(titleFont);
        layout->addWidget(title);

        m_status = new QLabel(this);
        m_status->setWordWrap(true);
        layout->addWidget(m_status);

        m_values = new QLabel(this);
        m_values->setWordWrap(true);
        layout->addWidget(m_values);

        auto* buttons = new QHBoxLayout;
        m_start = new QPushButton(tr("Start"), this);
        m_stop = new QPushButton(tr("Stop"), this);
        buttons->addWidget(m_start);
        buttons->addWidget(m_stop);
        buttons->addStretch(1);
        layout->addLayout(buttons);
        if (wave) {
            // The WaveNode USB helper is independent: there is nothing here
            // to start or stop, only its UDP feed to listen for.
            m_start->hide();
            m_stop->hide();
        }

        connect(m_start, &QPushButton::clicked, this, [this] { startHelper(); });
        connect(m_stop, &QPushButton::clicked, this,
                [this] { HelperApps::instance().stop(m_helper); refresh(); });
        connect(&HelperApps::instance(), &HelperApps::runningChanged, this,
                [this](HelperApps::Helper helper, bool) { if (helper == m_helper) { refresh(); } });
        connect(&HelperApps::instance(), &HelperApps::fault, this,
                [this](HelperApps::Helper helper, const QString& reason) {
            if (helper != m_helper) { return; }
            m_fault = reason;
            refresh();
        });

        m_timer.setInterval(1000);
        connect(&m_timer, &QTimer::timeout, this, [this] { refresh(); });

        refresh();
        if (!m_preview) {
            // The container is the selection: placing it starts the helper.
            QTimer::singleShot(0, this, [this] { startHelper(); });
            m_timer.start();
        }
    }

private:
    void startHelper()
    {
        if (m_preview) { return; }
        const HelperApps& helpers = HelperApps::instance();
        Q_UNUSED(helpers)
        QString error;
        if (!HelperApps::instance().start(m_helper, &error)) {
            m_fault = error;
        }
        refresh();
    }

    void refresh()
    {
        const HelperApps& helpers = HelperApps::instance();
        const bool wave = m_helper == HelperApps::Helper::WaveNode;
        const bool running = helpers.isRunning(m_helper);
        if (!m_preview && m_start && !wave) { m_start->setEnabled(!running); }
        if (!m_preview && m_stop && !wave) { m_stop->setEnabled(running); }
        QString status;
        if (wave) {
            status = running ? tr("Receiving data (independent helper)")
                             : tr("Waiting for WaveNodeUsb.exe");
        } else {
            status = running ? tr("Running") : tr("Stopped");
            if (!m_fault.isEmpty()) { status = tr("Fault: %1").arg(m_fault); }
        }
        if (m_status) {
            m_status->setText(status);
            m_status->setStyleSheet((!wave && !m_fault.isEmpty())
                ? QStringLiteral("color: #ff8080;")
                : QStringLiteral("color: #c8d8e8;"));
        }
        if (!m_values) { return; }
        MmioEndpoint* endpoint = ExternalVariableEngine::instance()
                                     .endpointByFourChar(wave ? QStringLiteral("Zjjg")
                                                              : QStringLiteral("QxG4"));
        if (!endpoint) {
            m_values->setText(tr("No data source."));
            return;
        }
        const auto number = [endpoint](const char* name, int decimals) {
            const QVariant value = endpoint->valueForName(QLatin1String(name));
            return value.isValid() ? QString::number(value.toDouble(), 'f', decimals)
                                   : QStringLiteral("\u2014");
        };
        const auto text = [endpoint](const char* name) {
            const QVariant value = endpoint->valueForName(QLatin1String(name));
            return value.isValid() ? value.toString() : QStringLiteral("\u2014");
        };
        if (wave) {
            m_values->setText(tr("PW %1 W   SWR %2   %3   %4")
                                  .arg(number("fwdpwr", 1), number("swr", 2),
                                       text("band"), number("tempC1", 1) + tr(" C")));
        } else {
            m_values->setText(tr("%1   %2   C=%3  L=%4  IMP=%5")
                                  .arg(text("ModeDescription"), text("DisplayedFrequency"),
                                       text("DisplayedValueC"), text("DisplayedValueL"),
                                       text("LoadImpeadance")));
        }
    }

    HelperApps::Helper m_helper;
    bool m_preview = false;
    QLabel* m_status = nullptr;
    QLabel* m_values = nullptr;
    QPushButton* m_start = nullptr;
    QPushButton* m_stop = nullptr;
    QString m_fault;
    QTimer m_timer;
};

} // namespace

void registerHelperContent(ContainerContentRegistry& registry)
{
    const auto add = [&registry](const QString& typeId, const QString& title,
                                 HelperApps::Helper helper) {
        ContentDescriptor descriptor;
        descriptor.typeId = typeId;
        descriptor.title = title;
        descriptor.singleton = false;
        descriptor.available = true;
        descriptor.group = ContentGroup::Panels;
        registry.registerWidgetFactory(typeId, descriptor,
            [helper](const ContentEntry&, QWidget* parent, ContentRenderMode mode) -> QWidget* {
                return new HelperPanel(helper, parent, mode == ContentRenderMode::Preview);
            });
    };
    add(QStringLiteral("helper.wavenode"), QStringLiteral("WaveNode"),
        HelperApps::Helper::WaveNode);
    add(QStringLiteral("helper.hfauto"), QStringLiteral("HF-Auto"),
        HelperApps::Helper::HfAuto);
}

} // namespace NereusSDR

#include "HelperContent.moc"
