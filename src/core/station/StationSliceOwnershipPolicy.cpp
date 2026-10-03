// no-port-check: NereusSDR-original established station ownership attachment.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "core/station/StationSliceOwnershipPolicy.h"
#include "core/SliceOwnership.h"
#include "models/RadioModel.h"
#include <QScopeGuard>
#include <QTimer>
#include <algorithm>
#include <utility>

namespace NereusSDR {
StationSliceOwnershipPolicy::StationSliceOwnershipPolicy(RadioModel* model)
    : QObject(model), m_model(model)
{
    connect(model, &RadioModel::sliceAdded, this, [this] { adopt(); });
    // Saved owner/held marks are applied after sliceAdded. The queued boundary
    // also lets RadioModel clear its hydration flag before inspecting them.
    connect(model, &RadioModel::receiveLayoutHydrated, this,
            [this] { adopt(); }, Qt::QueuedConnection);
}

void StationSliceOwnershipPolicy::activate(RadioModel* model, QObject* lifetime,
                                          std::function<bool()> stillAuthorized)
{
    if (!model || !lifetime || !stillAuthorized || model->role() != RadioModel::Role::Local) {
        return;
    }
    auto* policy = model->findChild<StationSliceOwnershipPolicy*>(
        QString(), Qt::FindDirectChildrenOnly);
    if (!policy) { policy = new StationSliceOwnershipPolicy(model); }
    auto& authorities = policy->m_authorities;
    std::erase_if(authorities, [](const Authority& value) { return !value.lifetime; });
    const auto found = std::find_if(authorities.begin(), authorities.end(),
        [lifetime](const Authority& value) { return value.lifetime == lifetime; });
    if (found == authorities.end()) {
        authorities.push_back({lifetime, std::move(stillAuthorized)});
    } else {
        found->allowed = std::move(stillAuthorized);
    }
    policy->adopt();
}

bool StationSliceOwnershipPolicy::authorized() const
{
    // A predicate may synchronously retire its context/model. Work on value
    // copies and never inspect the dead attachment after the callout.
    const QPointer<const StationSliceOwnershipPolicy> self(this);
    const auto authorities = m_authorities;
    for (const Authority& value : authorities) {
        if (!value.lifetime) { continue; }
        const bool allowed = value.allowed();
        if (!self) { return false; }
        if (value.lifetime && allowed) { return true; }
    }
    return false;
}

void StationSliceOwnershipPolicy::adopt()
{
    const QPointer<StationSliceOwnershipPolicy> self(this);
    const QPointer<RadioModel> model(m_model);
    if (!model || model->role() != RadioModel::Role::Local || model->m_receiveLayoutHydrating) {
        return;
    }
    if (m_adopting) { m_adoptAgain = true; return; }
    const bool allowed = authorized();
    if (!self || !model || !allowed) { return; }
    m_adopting = true;
    const auto finish = qScopeGuard([self] {
        if (!self) { return; }
        self->m_adopting = false;
        if (std::exchange(self->m_adoptAgain, false)) {
            QTimer::singleShot(0, self, [self] { if (self) { self->adopt(); } });
        }
    });
    const QPointer<SliceOwnership> ownership(model->sliceOwnership());
    if (!ownership) { return; }
    const auto adopted = ownership->adoptUnowned(SliceOwnership::stationDevice(), [self, model] {
        return self && model && model->role() == RadioModel::Role::Local && self->authorized();
    });
    if (!self || !model || !ownership || adopted.isEmpty()) { return; }
    const bool remainsAuthorized = authorized();
    if (!self || !model || !ownership || !remainsAuthorized) { return; }
    model->setActiveSliceByIdFor(SliceOwnership::stationDevice(),
                               ownership->activeFor(SliceOwnership::stationDevice()));
}
} // namespace NereusSDR
