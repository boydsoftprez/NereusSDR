#include "MeterPoller.h"
#include "MeterWidget.h"
#include "core/RxChannel.h"
#include "core/LogCategories.h"

namespace NereusSDR {

MeterPoller::MeterPoller(QObject* parent)
    : QObject(parent)
{
    m_timer.setInterval(100);  // 10 fps default (from Thetis UpdateInterval=100ms)
    connect(&m_timer, &QTimer::timeout, this, &MeterPoller::poll);
}

MeterPoller::~MeterPoller() = default;

void MeterPoller::setRxChannel(RxChannel* channel)
{
    m_rxChannel = channel;
    qCDebug(lcMeter) << "MeterPoller: RxChannel set, channelId:"
                      << (channel ? channel->channelId() : -1);
}

void MeterPoller::addTarget(MeterWidget* widget)
{
    if (widget && !m_targets.contains(widget)) {
        m_targets.append(widget);
    }
}

void MeterPoller::removeTarget(MeterWidget* widget)
{
    m_targets.removeAll(widget);
}

void MeterPoller::setInterval(int ms)
{
    m_timer.setInterval(ms);
}

int MeterPoller::interval() const
{
    return m_timer.interval();
}

void MeterPoller::start()
{
    if (!m_rxChannel) {
        qCWarning(lcMeter) << "MeterPoller: cannot start without RxChannel";
        return;
    }
    m_timer.start();
    qCDebug(lcMeter) << "MeterPoller: started at" << m_timer.interval() << "ms";
}

void MeterPoller::stop()
{
    m_timer.stop();
    qCDebug(lcMeter) << "MeterPoller: stopped";
}

void MeterPoller::poll()
{
    if (!m_rxChannel) { return; }

    // Poll all RX meter types. GetRXAMeter is lock-free.
    for (int bindingId = MeterBinding::SignalPeak;
         bindingId <= MeterBinding::AgcAvg; ++bindingId) {
        double value = m_rxChannel->getMeter(static_cast<RxMeterType>(bindingId));
        for (MeterWidget* target : m_targets) {
            target->updateMeterValue(bindingId, value);
        }
    }
}

} // namespace NereusSDR
