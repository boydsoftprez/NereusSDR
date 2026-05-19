// =================================================================
// src/core/TuneMemoryStore.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-native per-(antenna, band) cache of TGXL relay positions.
// See TuneMemoryStore.h for full design notes.
//
// AI tooling: Anthropic Claude Code.

#include "core/TuneMemoryStore.h"
#include "core/AppSettings.h"
#include "models/Band.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

namespace NereusSDR {

namespace {
// Antenna range: 1..3 (TGXL has three antenna ports).
constexpr int kAntMin = 1;
constexpr int kAntMax = 3;
} // namespace

TuneMemoryStore::TuneMemoryStore(QObject* parent)
    : QObject(parent)
{}

std::optional<TuneMemory> TuneMemoryStore::recall(int antenna, Band band) const
{
    auto& s = AppSettings::instance();
    const QString key = slotKey(antenna, band);
    const QString raw = s.value(key, "").toString();
    if (raw.isEmpty()) {
        return std::nullopt;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
    if (!doc.isObject()) {
        return std::nullopt;
    }

    const QJsonObject obj = doc.object();
    TuneMemory mem;
    mem.antenna   = antenna;
    mem.band      = band;
    mem.c1        = obj.value("c1").toInt();
    mem.l         = obj.value("l").toInt();
    mem.c2        = obj.value("c2").toInt();
    mem.savedAtMs = obj.value("savedAt").toInteger();
    return mem;
}

void TuneMemoryStore::store(const TuneMemory& mem)
{
    QJsonObject obj;
    obj["c1"]      = mem.c1;
    obj["l"]       = mem.l;
    obj["c2"]      = mem.c2;
    obj["savedAt"] = QJsonValue(mem.savedAtMs);

    const QString json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    AppSettings::instance().setValue(slotKey(mem.antenna, mem.band), json);
    emit changed();
}

void TuneMemoryStore::clear(int antenna, Band band)
{
    AppSettings::instance().setValue(slotKey(antenna, band), "");
    emit changed();
}

void TuneMemoryStore::clearAll()
{
    auto& s = AppSettings::instance();
    for (int ant = kAntMin; ant <= kAntMax; ++ant) {
        for (int b = 0; b < static_cast<int>(Band::Count); ++b) {
            s.setValue(slotKey(ant, static_cast<Band>(b)), "");
        }
    }
    emit changed();
}

QVector<TuneMemory> TuneMemoryStore::listAll() const
{
    QVector<TuneMemory> result;
    for (int b = 0; b < static_cast<int>(Band::Count); ++b) {
        const Band band = static_cast<Band>(b);
        for (int ant = kAntMin; ant <= kAntMax; ++ant) {
            auto mem = recall(ant, band);
            if (mem.has_value()) {
                result.append(mem.value());
            }
        }
    }
    // Already sorted: outer loop is ascending band enum, inner is ascending antenna.
    return result;
}

QString TuneMemoryStore::slotKey(int antenna, Band band) const
{
    return QString("TGXL_TuneMemory_Ant%1_Band%2")
               .arg(antenna)
               .arg(bandKeyName(band));
}

}  // namespace NereusSDR
