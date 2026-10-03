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
//   2026-09-23  J.J. Boyd / KG4VCF  R3 Setup fix wave (R-R3-17,
//                                    R-R3-21): setActivePlan() writes
//                                    BandPlanName only when it changes.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-23  J.J. Boyd / KG4VCF  R3 Linux suite fixes (R-R3-10,
//                                    R-R3-21): the band-plan files are a
//                                    NereusCore resource, initialised
//                                    here, so a Core-only program
//                                    (nereusd) loads them; loadPlans()
//                                    logs how many it found. AI-assisted
//                                    transformation via Anthropic Claude
//                                    Code.
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 19 (R-IOS-06): each
//                                    plan records its file id; the
//                                    first-launch default is
//                                    kDefaultPlanName. AI-assisted via
//                                    Anthropic Claude Code.

#include "BandPlanManager.h"

#include "core/AppSettings.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

// R-R3-10 / R-R3-21: the band plans are compiled into NereusCore from
// resources/bandplans.qrc (CMakeLists.txt). Q_INIT_RESOURCE must sit
// outside any namespace; calling it from the loader ties the resource to
// the code that reads it, so no Core-only link can leave it out.
// Registering an already-registered resource is a no-op in Qt.
static void initBandPlanResources()
{
    Q_INIT_RESOURCE(bandplans);
}

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

    initBandPlanResources();

    QDir resDir(":/bandplans");
    const auto entries = resDir.entryList({"*.json"}, QDir::Files, QDir::Name);
    for (const auto& filename : entries) {
        PlanData plan;
        plan.id = QFileInfo(filename).completeBaseName();
        if (loadPlanFromJson(":/bandplans/" + filename, plan)) {
            m_plans.append(std::move(plan));
        }
    }
    if (m_plans.isEmpty()) {
        qCWarning(lcBandPlan) << "loadPlans: no band plans found in :/bandplans";
    } else {
        qCInfo(lcBandPlan) << "loadPlans: loaded" << m_plans.size() << "band plans";
    }

    // Activate the persisted plan, defaulting to "ARRL (US)" on first launch.
    QString saved = AppSettings::instance()
                        .value("BandPlanName", QString::fromLatin1(kDefaultPlanName))
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
            // R-R3-17 / R-R3-21 (R3 Setup fix wave): persist only a real
            // change. loadPlans() re-activates the plan it just read, and
            // in a remote window that read comes from the Core before its
            // settings have arrived; writing it back then counted as an
            // edit made while the link was down and, on the first
            // connect, as one that "did not stick".
            auto& settings = AppSettings::instance();
            if (settings.value("BandPlanName", QString::fromLatin1(kDefaultPlanName)).toString()
                != name) {
                settings.setValue("BandPlanName", name);
            }
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
