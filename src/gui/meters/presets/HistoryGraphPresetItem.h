// no-port-check: NereusSDR family adapter; source algorithms reside in CompositePresetItem.
// Modification history (NereusSDR):
//   2026-10-26 - attachTrace2 / updateTrace2Session / replayTrace2: trace 2
//                 reads its own slice through a MeterPoller reading
//                 consumer (D2, a slice per trace). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
#pragma once
#include "CompositePresetItem.h"
#include <QJsonObject>
#include <QMetaObject>
#include <QPointer>
namespace NereusSDR {
class MeterPoller;
class HistoryGraphPresetItem : public CompositePresetItem {
    Q_OBJECT
public:
    explicit HistoryGraphPresetItem(QObject* parent=nullptr);
    // Trace 2 reads trace2SourceContext(entryContext) through `poller` as a
    // reading consumer (addReadingConsumer, readingUpdated, rxSourceChanged),
    // so its slice can differ from the entry's. Off, or fed by data source
    // variable 2, it registers nothing. Calling again replaces the last
    // registration; the item's destruction removes it.
    void attachTrace2(MeterPoller* poller, const QJsonObject& entryContext, const QString& currentSessionId);
    // A trace 2 source on another session's radio shows its veil.
    void updateTrace2Session(const QJsonObject& entryContext, const QString& currentSessionId);
    // An inert preview copy: trace 2 takes its cached reading once and
    // registers nothing.
    void replayTrace2(const MeterPoller& poller, const QJsonObject& entryContext, const QString& currentSessionId);
    static QString otherRadioText();
private:
    void detachTrace2();
    QPointer<MeterPoller> m_trace2Poller;
    QMetaObject::Connection m_trace2Reading;
    QMetaObject::Connection m_trace2Source;
};
}
