// no-port-check: NereusSDR family adapter.
// Modification history (NereusSDR):
//   2026-10-26 - attachTrace2 / updateTrace2Session / replayTrace2: trace 2
//                 reads its own slice through a MeterPoller reading
//                 consumer (D2, a slice per trace). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
#include "HistoryGraphPresetItem.h"
#include "gui/meters/MeterPoller.h"
namespace NereusSDR {
HistoryGraphPresetItem::HistoryGraphPresetItem(QObject* parent) : CompositePresetItem(Face::History,parent) {}
QString HistoryGraphPresetItem::otherRadioText()
{
    return tr("Trace 2 reads another radio, not this one");
}
void HistoryGraphPresetItem::detachTrace2()
{
    disconnect(m_trace2Reading);
    disconnect(m_trace2Source);
    if (m_trace2Poller) { m_trace2Poller->removeReadingConsumer(this); }
    m_trace2Poller.clear();
}
void HistoryGraphPresetItem::attachTrace2(MeterPoller* poller, const QJsonObject& entryContext, const QString& currentSessionId)
{
    detachTrace2();
    updateTrace2Session(entryContext, currentSessionId);
    const QJsonObject context = trace2SourceContext(entryContext);
    const int binding = trace2Binding();
    if (!poller || context.isEmpty() || trace2FromDataSource() || binding < 0) { return; }
    m_trace2Poller = poller;
    m_trace2Reading = connect(poller, &MeterPoller::readingUpdated, this,
        [this, context, binding](const QJsonObject& from, int reading, double value) {
            if (from == context && reading == binding) { pushTrace2Value(value); }
        });
    // The slice behind a followed source changed: the old slice's line and
    // reading no longer apply.
    m_trace2Source = connect(poller, &MeterPoller::rxSourceChanged, this, [this, context](const QJsonObject& from) {
        if (from == context) { resetTrace2(); }
    });
    poller->addReadingConsumer(this, context, {binding}, [this, binding](int reading, double value) {
        if (reading == binding) { pushTrace2Value(value); }
    });
}
void HistoryGraphPresetItem::updateTrace2Session(const QJsonObject& entryContext, const QString& currentSessionId)
{
    const QString session = trace2SourceContext(entryContext).value(QStringLiteral("sessionId")).toString();
    setTrace2Unavailable(!session.isEmpty() && session != currentSessionId ? otherRadioText() : QString());
}
void HistoryGraphPresetItem::replayTrace2(const MeterPoller& poller, const QJsonObject& entryContext, const QString& currentSessionId)
{
    updateTrace2Session(entryContext, currentSessionId);
    const QJsonObject context = trace2SourceContext(entryContext);
    if (context.isEmpty() || trace2FromDataSource() || trace2Binding() < 0) { return; }
    if (const std::optional<double> value = poller.cachedReading(context, trace2Binding())) { pushTrace2Value(*value); }
}
}
