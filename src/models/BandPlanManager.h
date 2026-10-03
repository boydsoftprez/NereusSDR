// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR — bandplan manager
//
// Ported from AetherSDR src/models/BandPlanManager.h [@0cd4559].
// AetherSDR is © its contributors and is licensed GPL-3.0-or-later.
//
// Modification history (NereusSDR):
//   2026-04-25  J.J. Boyd <jj@skyrunner.net>  Initial port for Phase 3G RX
//                                              Epic sub-epic D. Renamed to
//                                              NereusSDR namespace; reuses
//                                              src/models/BandPlan.h value
//                                              types instead of nested ones.
//                                              AppSettings key kept as
//                                              "BandPlanName" for upstream
//                                              parity. AI assistance:
//                                              Anthropic Claude
//                                              (claude-sonnet-4-6).
//   2026-09-24  J.J. Boyd / KG4VCF  iPhone app Task 19 (R-IOS-06):
//                                    PlanData is public with the plan's
//                                    file id, plans() lists every loaded
//                                    plan, and kDefaultPlanName names the
//                                    first-launch plan, for the Core's
//                                    catalogue. AI-assisted via Anthropic
//                                    Claude Code.

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "BandPlan.h"

namespace NereusSDR {

// Manages selectable band-plan overlays for the spectrum display.
// Plans are loaded from bundled Qt resource JSON files at
// :/bandplans/*.json. Active selection persists in AppSettings under
// "BandPlanName".
//
// From AetherSDR src/models/BandPlanManager.h [@0cd4559].
class BandPlanManager : public QObject {
    Q_OBJECT

public:
    // iPhone app Task 19: one loaded plan. `id` is its file's name without
    // ".json" (e.g. "arrl-us"), a stable name the Core's catalogue sends.
    struct PlanData {
        QString             id;
        QString             name;
        QVector<BandSegment> segments;
        QVector<BandSpot>    spots;
    };

    // The plan a first launch activates (loadPlans()), and the one the
    // Core's catalogue marks as the default.
    static constexpr const char* kDefaultPlanName = "ARRL (US)";

    explicit BandPlanManager(QObject* parent = nullptr);

    // Load all bundled plans from Qt resources. Idempotent: re-reads the
    // resource directory and re-applies the saved active plan.
    void loadPlans();

    // Active plan
    QString activePlanName() const { return m_activeName; }
    void    setActivePlan(const QString& name);
    const QVector<BandSegment>& segments() const { return m_segments; }
    const QVector<BandSpot>&    spots()    const { return m_spots; }

    // All loaded plan display names (for Setup → Display dropdown).
    QStringList availablePlans() const;

    // iPhone app Task 19: every loaded plan, in load order.
    const QVector<PlanData>& plans() const { return m_plans; }

signals:
    void planChanged();

private:
    bool loadPlanFromJson(const QString& path, PlanData& out);

    QVector<PlanData>    m_plans;
    QString              m_activeName;
    QVector<BandSegment> m_segments;  // active plan's segments
    QVector<BandSpot>    m_spots;     // active plan's spots
};

}  // namespace NereusSDR
