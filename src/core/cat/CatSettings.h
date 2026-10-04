// no-port-check: NereusSDR-original CAT station admission/lifecycle.
// Independently implemented from the approved Nereus settings schema; no upstream serialization logic.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#pragma once
#include "CatConfiguration.h"
#include <QList>
#include <functional>
#include <memory>
#include <optional>
namespace NereusSDR {
class AppSettings; class RadioModel;
class CatSettings {
public:
    explicit CatSettings(AppSettings& settings);
    static QList<CatEndpointConfig> load(AppSettings&, const RadioModel&);
    static bool save(AppSettings&, CatEndpointConfig, std::function<bool()> current = {});
    static bool validate(const CatEndpointConfig&, QString* reason = nullptr);
    CatGlobalConfig global() const;
    bool setGlobal(CatGlobalConfig, std::function<bool()> current = {});
    static bool validateGlobal(const CatGlobalConfig&);
private:
    struct WriteState { quint64 revision{0}; std::optional<CatGlobalConfig> desired; };
    std::shared_ptr<WriteState> m_writeState{std::make_shared<WriteState>()};
    AppSettings& m_settings;
};
} // namespace NereusSDR
