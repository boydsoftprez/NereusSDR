// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR — bandplan manager (implementation)
//
// Ported from AetherSDR src/models/BandPlanManager.cpp [@0cd4559].
// AetherSDR is © its contributors and is licensed GPL-3.0-or-later.
//
// Modification history (NereusSDR):
//   2026-04-25  J.J. Boyd <jj@skyrunner.net>  Initial port for Phase 3G RX
//                                              Epic sub-epic D. See
//                                              BandPlanManager.h for full
//                                              attribution notes. AI
//                                              assistance: Anthropic Claude
//                                              (claude-sonnet-4-6).

#include "BandPlanManager.h"

#include "core/AppSettings.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

namespace NereusSDR {

namespace {
Q_LOGGING_CATEGORY(lcBandPlan, "nereussdr.bandplan")
}

BandPlanManager::BandPlanManager(QObject* parent)
    : QObject(parent)
{
}

// From AetherSDR BandPlanManager.cpp:17-40 [@0cd4559]
void BandPlanManager::loadPlans()
{
    m_plans.clear();

    QDir resDir(":/bandplans");
    const auto entries = resDir.entryList({"*.json"}, QDir::Files, QDir::Name);
    for (const auto& filename : entries) {
        PlanData plan;
        if (loadPlanFromJson(":/bandplans/" + filename, plan)) {
            m_plans.append(std::move(plan));
        }
    }

    // Activate the persisted plan, defaulting to "ARRL (US)" on first launch.
    QString saved = AppSettings::instance()
                        .value("BandPlanName", QStringLiteral("ARRL (US)"))
                        .toString();
    bool found = false;
    for (const auto& p : m_plans) {
        if (p.name == saved) { found = true; break; }
    }
    if (!found && !m_plans.isEmpty()) {
        saved = m_plans.first().name;
    }

    setActivePlan(saved);
}

// From AetherSDR BandPlanManager.cpp:42-55 [@0cd4559]
void BandPlanManager::setActivePlan(const QString& name)
{
    for (const auto& p : m_plans) {
        if (p.name == name) {
            m_activeName = name;
            m_segments   = p.segments;
            m_spots      = p.spots;
            AppSettings::instance().setValue("BandPlanName", name);
            emit planChanged();
            return;
        }
    }
    qCDebug(lcBandPlan) << "setActivePlan: unknown plan" << name;
}

QStringList BandPlanManager::availablePlans() const
{
    QStringList names;
    names.reserve(m_plans.size());
    for (const auto& p : m_plans) {
        names << p.name;
    }
    return names;
}

// From AetherSDR BandPlanManager.cpp:65-107 [@0cd4559]
bool BandPlanManager::loadPlanFromJson(const QString& path, PlanData& out)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        qCWarning(lcBandPlan) << "open failed:" << path;
        return false;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        qCWarning(lcBandPlan) << "JSON parse error in" << path << err.errorString();
        return false;
    }

    const QJsonObject root = doc.object();
    out.name = root.value("name").toString();
    if (out.name.isEmpty()) {
        qCWarning(lcBandPlan) << "missing 'name' in" << path;
        return false;
    }

    const auto segs = root.value("segments").toArray();
    for (const auto& val : segs) {
        const QJsonObject obj = val.toObject();
        BandSegment seg;
        seg.lowMhz  = obj.value("low").toDouble();
        seg.highMhz = obj.value("high").toDouble();
        seg.label   = obj.value("label").toString();
        seg.license = obj.value("license").toString();
        seg.color   = QColor(obj.value("color").toString());
        if (seg.lowMhz < seg.highMhz && seg.color.isValid()) {
            out.segments.append(seg);
        }
    }

    const auto spotArr = root.value("spots").toArray();
    for (const auto& val : spotArr) {
        const QJsonObject obj = val.toObject();
        BandSpot spot;
        spot.freqMhz = obj.value("freq").toDouble();
        spot.label   = obj.value("label").toString();
        if (spot.freqMhz > 0.0) {
            out.spots.append(spot);
        }
    }

    return true;
}

}  // namespace NereusSDR
