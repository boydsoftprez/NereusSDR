// Frequency memories: a named snapshot of the operating state for one slice.
//
// This is deliberately separate from the band stack. The band stack
// automatically remembers the last state per band; a memory is an explicit,
// named entry the operator saves and recalls from the Memories menu.
//
// Stored as one compact JSON document in AppSettings (Memories/Document), the
// same pattern the band stack and tune-memory stores use.

#pragma once

#include "core/NereusCoreExport.h"

#include <QString>
#include <QVector>

namespace NereusSDR {

struct FrequencyMemory
{
    QString id;
    QString name;
    double  frequencyHz{0.0};

    int     dspMode{0};
    int     filterLow{0};
    int     filterHigh{0};

    QString rxAntenna;
    QString txAntenna;

    int     agcMode{0};
    bool    autoAgcEnabled{false};
    double  autoAgcOffset{0.0};
    int     agcThreshold{0};
    int     agcHang{0};
    int     agcSlope{0};
    int     agcAttack{0};
    int     agcDecay{0};
    int     agcFixedGain{0};
    int     agcMaxGain{0};

    int     nbMode{0};
    int     activeNr{0};
    bool    anfEnabled{false};

    int     txPower{0};
    bool    includeTxPower{true};

    bool    attEnabled{false};
    int     attDb{0};
    int     preampMode{0};
    bool    rx1Preamp{false};
    bool    autoAttEnabled{false};
    int     autoAttMode{0};
    bool    autoAttUndo{false};
    int     autoAttUndoDelayMs{0};
    bool    includeAtt{true};

    bool    ritEnabled{false};
    int     ritHz{0};
    bool    xitEnabled{false};
    int     xitHz{0};
};

class NEREUS_CORE_EXPORT FrequencyMemoryStore
{
public:
    // Operator-level, so the list follows the operator rather than a radio.
    static constexpr const char* kSettingsKey = "Memories/Document";

    void load();
    void save() const;

    const QVector<FrequencyMemory>& entries() const { return m_entries; }

    FrequencyMemory*       find(const QString& id);
    const FrequencyMemory* find(const QString& id) const;

    void add(FrequencyMemory entry);
    void update(const FrequencyMemory& entry);
    void remove(const QString& id);

private:
    QVector<FrequencyMemory> m_entries;
};

} // namespace NereusSDR
