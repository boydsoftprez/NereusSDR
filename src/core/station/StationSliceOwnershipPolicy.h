#pragma once
// no-port-check: NereusSDR-original established station ownership attachment.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include <QObject>
#include <QPointer>
#include <functional>
#include <vector>

namespace NereusSDR {
class RadioModel;

// Shared by rightful desktop bootstrap and an explicitly hosting station.
// An attachment claims only ordinary unclaimed slices, never on an action,
// release or mark change. It provisions no server, identity or listener.
class StationSliceOwnershipPolicy final : public QObject {
    Q_OBJECT
public:
    static void activate(RadioModel* model, QObject* lifetime,
                         std::function<bool()> stillAuthorized);

private:
    explicit StationSliceOwnershipPolicy(RadioModel* model);
    bool authorized() const;
    void adopt();
    struct Authority {
        QPointer<QObject> lifetime;
        std::function<bool()> allowed;
    };
    QPointer<RadioModel> m_model;
    std::vector<Authority> m_authorities;
    bool m_adopting{false};
    bool m_adoptAgain{false};
};
} // namespace NereusSDR
